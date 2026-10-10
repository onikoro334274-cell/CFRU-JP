#!/usr/bin/env python3

import os
import subprocess
import shutil
import sys
import tempfile
import struct
import hashlib
import json
from datetime import datetime
import _io
from pathlib import Path

OFFSET_TO_PUT = 0x900000
SOURCE_ROM = "BPRJ0.gba"
ROM_NAME = "test.gba"

from toolchain import tool_path
from rom_bounds import validate_payload

OBJDUMP = tool_path('objdump')
NM = tool_path('nm')
AS = tool_path('as')

OUTPUT = 'build/output.bin'
BYTE_REPLACEMENT = 'bytereplacement'
HOOKS = 'hooks'
REPOINTS = 'repoints'
GENERATED_REPOINTS = 'generatedrepoints'
REPOINT_ALL = 'repointall'
ROUTINE_POINTERS = 'routinepointers'
FUNCTION_REWRITES = 'functionrewrites'
EVENT_SCRIPTS = "eventscripts"
SONGS = "songs"
SPECIAL_INSERTS = 'special_inserts.asm'
SPECIAL_INSERTS_OUT = 'build/special_inserts.bin'


def ExtractPointer(byteList: [bytes]):
    pointer = 0
    for a in range(len(byteList)):
        pointer += (int(byteList[a])) << (8 * a)

    return pointer


def GetTextSection() -> int:
    try:
        # Dump sections
        out = subprocess.check_output([OBJDUMP, '-t', 'build/linked.o'])
        lines = out.decode().split('\n')

        # Find text section
        text = filter(lambda x: x.strip().endswith('.text'), lines)
        section = (list(text))[0]

        # Get the offset
        offset = int(section.split(' ')[0], 16)
        return offset

    except:
        print("Error: The insertion process could not be completed.\n"
              + "The linker symbol file was not found. Most likely the compilation process was not completed.")
        sys.exit(1)


def GetSymbols(subtract=0) -> {str: int}:
    out = subprocess.check_output([NM, 'build/linked.o'])
    lines = out.decode().split('\n')

    ret = {}
    for line in lines:
        parts = line.strip().split()

        if len(parts) < 3:
            continue

        if parts[1].lower() not in {'t', 'd'}:
            continue

        offset = int(parts[0], 16)
        ret[parts[2]] = offset - subtract

    return ret


def _check_code_patch(rom, target, offset, size):
    if not 0 <= target < 0x2000000:
        raise ValueError('Hook target must be in the first 32 MiB of ROM')
    previous = rom.tell()
    rom.seek(0, os.SEEK_END)
    length = rom.tell()
    rom.seek(previous)
    if offset < 0 or offset > min(length, 0x2000000) - size:
        raise ValueError('Code patch is outside the existing ROM: ' + hex(offset))


def Hook(rom: _io.BufferedReader, space: int, hookAt: int, register=0):
    if not 0 <= register <= 7:
        raise ValueError('Thumb hook register must be r0..r7')
    # Align 2
    if hookAt & 1:
        hookAt -= 1

    _check_code_patch(rom, space, hookAt, 10 if hookAt % 4 else 8)
    rom.seek(hookAt)

    register &= 7

    if hookAt % 4:
        data = bytes([0x01, 0x48 | register, 0x00 | (register << 3), 0x47, 0x0, 0x0])
    else:
        data = bytes([0x00, 0x48 | register, 0x00 | (register << 3), 0x47])

    space = (space + 0x08000000) | 1
    data += (space.to_bytes(4, 'little'))
    rom.write(bytes(data))


def FunctionWrap(rom: _io.BufferedReader, space: int, hookAt: int, numParams: int, isReturning: int):
    if not 0 <= numParams <= 125 or isReturning not in (0, 1):
        raise ValueError('Invalid wrapper parameter count or return mode')
    # Align 2
    if hookAt & 1:
        hookAt -= 1
    if hookAt % 4:
        raise ValueError('Function wrapper must start at a word-aligned address')

    rom.seek(hookAt)
    numParams = numParams - 1

    if numParams < 4:
        data = bytes([0x10, 0xB5, 0x3, 0x4C, 0x0, 0xF0, 0x3, 0xF8, 0x10, 0xBC,
                      (isReturning + 1), 0xBC, (isReturning << 3), 0x47, 0x20, 0x47])
    else:
        k = numParams - 3
        data = bytes([0x10, 0xB5, 0x82, 0xB0])
        for i in range(k + 2):
            data += bytes([i + 2, 0x9C, i, 0x94])
        data += bytes([0x0, 0x9C, numParams - 1, 0x94, 0x1, 0x9C, numParams, 0x94, 0x2, 0xB0, k + 8, 0x4C,
                       0x0, 0xF0, (k << 1) + 13, 0xF8, 0x82, 0xB0, numParams, 0x9C, 0x1, 0x94, numParams - 1,
                       0x9C, 0x0, 0x94])

        for i in reversed(range(k + 2)):
            data += bytes([i, 0x9C, i+2, 0x94])
        data += bytes([0x2, 0xB0, 0x10, 0xBC, isReturning + 1, 0xBC, isReturning << 3, 0x47, 0x20, 0x47])

    _check_code_patch(rom, space, hookAt, len(data) + 4)
    space = (space + 0x08000000) | 1
    data += (space.to_bytes(4, 'little'))
    rom.write(bytes(data))


def Repoint(rom: _io.BufferedReader, space: int, repointAt: int, slideFactor=0):
    space += (0x08000000 + slideFactor)
    data = (space.to_bytes(4, 'little'))
    _check_existing_write(rom, repointAt, len(data))
    rom.seek(repointAt)
    rom.write(bytes(data))


# These offsets contain the word 0x8900000 - the attack data from
# Mr. DS's rombase. In order to maintain as much compatibility as
# possible, the data at these offsets is never modified.
IGNORED_OFFSETS = [0x35C748, 0x35C774, 0xCABDF0]


def RealRepoint(rom: _io.BufferedReader, offsetTuples: [(int, int, str)]):
    pointerDict = {}
    for offset, target, symbol in offsetTuples:
        rom.seek(offset)
        raw = rom.read(4)
        if len(raw) != 4:
            raise ValueError('Repoint source is outside the ROM: ' + hex(offset))
        pointer = ExtractPointer(raw)
        if pointer == 0:
            raise ValueError('Cannot globally repoint a null pointer: ' + hex(offset))
        pointerDict[pointer] = (target + 0x08000000, symbol)
    results = []
    if not pointerDict:
        return results
    rom.seek(0, os.SEEK_END)
    limit = min(rom.tell(), 0x1000000)
    ignored = set(IGNORED_OFFSETS)
    for start in range(0, limit, 65536):
        rom.seek(start)
        data = rom.read(min(65536, limit - start))
        for index, (word,) in enumerate(struct.iter_unpack('<I', data[:len(data) & ~3])):
            offset = start + index * 4
            replacement = pointerDict.get(word)
            if replacement is not None and offset not in ignored:
                results.append((offset, replacement[1]))
                rom.seek(offset)
                rom.write(replacement[0].to_bytes(4, 'little'))
    return results


def _check_existing_write(rom, offset, size):
    previous = rom.tell()
    rom.seek(0, os.SEEK_END)
    length = rom.tell()
    rom.seek(previous)
    if offset < 0 or offset > length - size:
        raise ValueError('Data patch is outside the existing ROM: ' + hex(offset))


def ReplaceBytes(rom: _io.BufferedReader, offset: int, data: str):
    payload = bytes(int(word, 16) for word in data.split())
    _check_existing_write(rom, offset, len(payload))
    rom.seek(offset)
    rom.write(payload)


from preprocess import TryProcessFileInclusion, TryProcessConditionalCompilation, validate_definition_file


def _active_repoint_requests():
    requests, definitions, conditionals = [], {}, []
    if os.path.isfile(REPOINT_ALL):
        with open(REPOINT_ALL, 'r', encoding='utf-8-sig') as file:
            for line in file:
                if TryProcessConditionalCompilation(line, definitions, conditionals):
                    continue
                if TryProcessFileInclusion(line, definitions):
                    continue
                if line.strip().startswith('#') or not line.strip():
                    continue
                symbol, address = line.split()
                requests.append((symbol, int(address, 16) - 0x08000000))
    return requests


def _repoint_signature(rom, requests):
    digest = hashlib.sha256(json.dumps({'version': 1, 'requests': requests,
                                      'ignored': IGNORED_OFFSETS}, sort_keys=True).encode('utf-8'))
    rom.seek(0)
    while True:
        block = rom.read(1048576)
        if not block:
            break
        digest.update(block)
    return digest.hexdigest()


def _load_repoint_cache(path, signature, targets):
    if not os.path.isfile(path):
        return None
    try:
        text = Path(path).read_text(encoding='utf-8-sig')
        header, body = text.split('\n', 1)
        prefix = '##repoint-cache-v1 '
        if not header.startswith(prefix):
            return None
        metadata = json.loads(header[len(prefix):])
        if metadata['input'] != signature or metadata['entries'] != hashlib.sha256(body.encode('utf-8')).hexdigest():
            return None
        entries = []
        for line in body.splitlines():
            symbol, address = line.split()
            offset = int(address)
            if symbol not in targets or offset < 0 or offset >= 0x1000000 or offset % 4:
                return None
            entries.append((symbol, offset))
        return entries
    except (ValueError, KeyError, TypeError, UnicodeError):
        return None


def _write_repoint_cache(path, signature, entries):
    body = ''.join('{} {}\n'.format(symbol, offset) for symbol, offset in entries)
    metadata = {'input': signature, 'entries': hashlib.sha256(body.encode('utf-8')).hexdigest()}
    Path(path).write_text('##repoint-cache-v1 ' + json.dumps(metadata, sort_keys=True) + '\n' + body,
                          encoding='utf-8')


def _read_rom_pointer(rom, offset):
    _check_existing_write(rom, offset, 4)
    rom.seek(offset)
    pointer = ExtractPointer(rom.read(4)) - 0x08000000
    _check_existing_write(rom, pointer, 1)
    return pointer


def _event_script_offset(rom, bank, map_number, kind, event_id, headers):
    if not 0 <= bank <= 255 or not 0 <= map_number <= 255:
        raise ValueError('Map bank and number must be in 0..255')
    key = (bank, map_number)
    if key not in headers:
        banks = _read_rom_pointer(rom, 0x54B0C)
        maps = _read_rom_pointer(rom, banks + bank * 4)
        headers[key] = _read_rom_pointer(rom, maps + map_number * 4)
    header = headers[key]
    if kind == 'map':
        _check_existing_write(rom, header + 8, 4)
        return header + 8
    layouts = {'npc': (0, 4, 0x18, 0x10), 'trainer': (0, 4, 0x18, 0x10),
               'item': (0, 4, 0x18, 0x10), 'tile': (2, 12, 0x10, 12),
               'sign': (3, 16, 0xC, 8)}
    if kind not in layouts:
        raise ValueError('Unknown event type: ' + kind)
    count_offset, pointer_offset, stride, script_offset = layouts[kind]
    events = _read_rom_pointer(rom, header + 4)
    _check_existing_write(rom, events + count_offset, 1)
    rom.seek(events + count_offset)
    count = rom.read(1)[0]
    if event_id < 0 or event_id >= count:
        raise ValueError('{} event index {} is outside count {}'.format(kind, event_id, count))
    table = _read_rom_pointer(rom, events + pointer_offset)
    _check_existing_write(rom, table, count * stride)
    return table + event_id * stride + script_offset


def _insert(output_rom, generated_repoints, offsets_ini):
    validate_payload(OFFSET_TO_PUT, os.path.getsize(OUTPUT))
    for path in (BYTE_REPLACEMENT, HOOKS, REPOINTS, REPOINT_ALL, ROUTINE_POINTERS,
                 FUNCTION_REWRITES, EVENT_SCRIPTS, SONGS):
        if os.path.isfile(path):
            validate_definition_file(path)

    try:
        shutil.copyfile(SOURCE_ROM, output_rom)
    except FileNotFoundError:
        print('Error: Insertion could not be completed.\n'
              + 'Could not find source rom: "' + SOURCE_ROM + '".\n'
              + 'Please make sure a rom with this name exists in the root.')
        sys.exit(1)
    except PermissionError:
        print('Error: Insertion could not be completed.\n'
              + '"' + ROM_NAME + '" is currently in use by another application.'
              + '\nPlease free it up before trying again.')
        sys.exit(1)

    with open(output_rom, 'rb+') as rom:
        print("Inserting code.")
        table = GetSymbols(GetTextSection())
        rom.seek(OFFSET_TO_PUT)
        with open(OUTPUT, 'rb') as binary:
            rom.write(binary.read())
            binary.close()

        # Adjust symbol table
        for entry in table:
            table[entry] += OFFSET_TO_PUT

        # Insert byte changes
        if os.path.isfile(BYTE_REPLACEMENT):
            with open(BYTE_REPLACEMENT, 'r', encoding='utf-8-sig') as replacelist:
                definesDict = {}
                conditionals = []
                for line in replacelist:
                    if TryProcessConditionalCompilation(line, definesDict, conditionals):
                        continue
                    if TryProcessFileInclusion(line, definesDict):
                        continue
                    if line.strip().startswith('#') or line.strip() == '':
                        continue
 
                    offset = int(line[:8], 16) - 0x08000000
                    try:
                        ReplaceBytes(rom, offset, line[9:].strip())
                    except ValueError: #Try loading from the defines dict if unrecognizable character
                        if line[9:].strip() not in definesDict:
                            raise
                        newNumber = definesDict[line[9:].strip()]
                        try:
                            newNumber = int(newNumber)
                        except ValueError:
                            newNumber = int(newNumber, 16)

                        newNumber = str(hex(newNumber)).split('0x')[1]
                        ReplaceBytes(rom, offset, newNumber) 

        # Do Special Inserts
        if os.path.isfile(SPECIAL_INSERTS) and os.path.isfile(SPECIAL_INSERTS_OUT):
            with open(SPECIAL_INSERTS, 'r', encoding='utf-8-sig') as file:
                offsetList = []
                for line in file:
                    if line.strip().startswith('.org '):
                        offsetList.append(int(line.split('.org ')[1].split(',')[0], 16))

                offsetList.sort()

            with open(SPECIAL_INSERTS_OUT, 'rb') as binFile:
                for offset in offsetList:
                    originalOffset = offset
                    dataList = ""

                    if offsetList.index(offset) == len(offsetList) - 1:
                        while True:
                            try:
                                binFile.seek(offset)
                                dataList += hex(binFile.read(1)[0]) + ' '
                            except IndexError:
                                break

                            offset += 1
                    else:
                        binFile.seek(offset)
                        word = ExtractPointer(binFile.read(4))

                        while word != 0xFFFFFFFF:
                            binFile.seek(offset)
                            dataList += hex(binFile.read(1)[0]) + ' '
                            offset += 1

                            if offset in offsetList:  # Overlapping data
                                break

                            word = ExtractPointer(binFile.read(4))

                    ReplaceBytes(rom, originalOffset, dataList.strip())

        # Read hooks from a file
        if os.path.isfile(HOOKS):
            with open(HOOKS, 'r', encoding='utf-8-sig') as hookList:
                definesDict = {}
                conditionals = []
                for line in hookList:
                    if TryProcessConditionalCompilation(line, definesDict, conditionals):
                        continue
                    if TryProcessFileInclusion(line, definesDict):
                        continue
                    if line.strip().startswith('#') or line.strip() == '':
                        continue

                    symbol, address, register = line.split()
                    offset = int(address, 16) - 0x08000000
                    try:
                        code = table[symbol]
                    except KeyError:
                        raise ValueError('Missing insertion symbol: ' + symbol)

                    Hook(rom, code, offset, int(register))

        # Read repoints from a file
        if os.path.isfile(REPOINTS):
            with open(REPOINTS, 'r', encoding='utf-8-sig') as repointList:
                definesDict = {}
                conditionals = []
                for line in repointList:
                    if TryProcessConditionalCompilation(line, definesDict, conditionals):
                        continue
                    if TryProcessFileInclusion(line, definesDict):
                        continue
                    if line.strip().startswith('#') or line.strip() == '':
                        continue

                    if len(line.split()) == 2:
                        symbol, address = line.split()
                        offset = int(address, 16) - 0x08000000
                        try:
                            code = table[symbol]
                        except KeyError:
                            raise ValueError('Missing insertion symbol: ' + symbol)

                        Repoint(rom, code, offset)

                    elif len(line.split()) == 3:
                        symbol, address, slide = line.split()
                        offset = int(address, 16) - 0x08000000
                        try:
                            code = table[symbol]
                        except KeyError:
                            raise ValueError('Missing insertion symbol: ' + symbol)

                        Repoint(rom, code, offset, int(slide))
                    else:
                        raise ValueError('Expected symbol, address and optional slide: ' + line.strip())

        requests = _active_repoint_requests()
        targets = {}
        for symbol, offset in requests:
            if symbol not in table:
                raise ValueError('Missing insertion symbol: ' + symbol)
            targets[symbol] = table[symbol]
        signature = _repoint_signature(rom, requests)
        cached = _load_repoint_cache(generated_repoints, signature, targets)
        if cached is None:
            matches = RealRepoint(rom, [(offset, targets[symbol], symbol) for symbol, offset in requests])
            cached = [(symbol, offset) for offset, symbol in matches]
        else:
            for symbol, offset in cached:
                Repoint(rom, targets[symbol], offset)
        _write_repoint_cache(generated_repoints, signature, cached)

        # Read routine repoints from a file
        if os.path.isfile(ROUTINE_POINTERS):
            with open(ROUTINE_POINTERS, 'r', encoding='utf-8-sig') as pointerlist:
                definesDict = {}
                conditionals = []
                for line in pointerlist:
                    if TryProcessConditionalCompilation(line, definesDict, conditionals):
                        continue
                    if TryProcessFileInclusion(line, definesDict):
                        continue
                    if line.strip().startswith('#') or line.strip() == '':
                        continue

                    symbol, address = line.split()
                    offset = int(address, 16) - 0x08000000
                    try:
                        code = table[symbol]
                    except KeyError:
                        raise ValueError('Missing insertion symbol: ' + symbol)

                    Repoint(rom, code, offset, 1)

        # Read routine rewrite wrapper from a file
        if os.path.isfile(FUNCTION_REWRITES):
            with open(FUNCTION_REWRITES, 'r', encoding='utf-8-sig') as frwlist:
                definesDict = {}
                conditionals = []
                for line in frwlist:
                    if TryProcessConditionalCompilation(line, definesDict, conditionals):
                        continue
                    if TryProcessFileInclusion(line, definesDict):
                        continue
                    if line.strip().startswith('#') or line.strip() == '':
                        continue

                    symbol, address, numParams, isReturning = line.split()
                    offset = int(address, 16) - 0x08000000
                    try:
                        code = table[symbol]
                    except KeyError:
                        raise ValueError('Missing insertion symbol: ' + symbol)

                    FunctionWrap(rom, code, offset, int(numParams), int(isReturning))

        # Insert Event Scripts
        if os.path.isfile(EVENT_SCRIPTS):
            definesDict, conditionals, headers = {}, [], {}
            with open(EVENT_SCRIPTS, 'r', encoding='utf-8-sig') as file:
                for number, line in enumerate(file, 1):
                    if TryProcessConditionalCompilation(line, definesDict, conditionals):
                        continue
                    if TryProcessFileInclusion(line, definesDict):
                        continue
                    if line.strip().startswith('#') or not line.strip():
                        continue
                    try:
                        fields = line.split()
                        if len(fields) == 5:
                            kind, bank, map_number, event_id, symbol = fields
                            event_id = int(event_id)
                        elif len(fields) == 4:
                            kind, bank, map_number, symbol = fields
                            event_id = -1
                        else:
                            raise ValueError('Invalid event script row')
                        kind = kind.lower()
                        offset = _event_script_offset(rom, int(bank), int(map_number), kind, event_id, headers)
                        if kind == 'trainer':
                            symbol = 'EventScript_' + symbol
                        elif kind == 'item':
                            symbol = 'ItemFindScript_' + symbol
                        symbol = definesDict.get(symbol, symbol)
                        if symbol in table:
                            code = table[symbol]
                        else:
                            try:
                                code = int(symbol, 16)
                            except ValueError as error:
                                raise ValueError('Missing insertion symbol: ' + symbol) from error
                        Repoint(rom, code, offset)
                    except (OSError, ValueError, IndexError) as error:
                        raise ValueError('Invalid event script on line {}: {}'.format(number, line.strip())) from error

        # Insert Song Pointers
        if os.path.isfile(SONGS):
            definesDict = {}
            conditionals = []
            rom.seek(0x1C10D8)  # m4aSongNumStart
            songTable = ExtractPointer(rom.read(4)) - 0x08000000

            with open(SONGS, "r", encoding='utf-8-sig') as file:
                for i, line in enumerate(file):
                    if TryProcessConditionalCompilation(line, definesDict, conditionals):
                        continue
                    if TryProcessFileInclusion(line, definesDict):
                        continue
                    if line.strip().startswith('#') or line.strip() == '':
                        continue

                    try:
                        lineList = line.split()
                        try:
                            songId = int(lineList[0])
                        except ValueError:
                            songId = int(lineList[0], 16)  # Hex
                        song = lineList[1]
                        offset = songTable + songId * 8

                        try:
                                code = table[song]
                        except KeyError:
                            try:
                                code = int(song, 16)  # If script offset was written in hex
                            except ValueError:
                                raise ValueError('Missing insertion symbol: ' + song)

                        Repoint(rom, code, offset)
                    except (OSError, ValueError, IndexError) as error:
                        raise ValueError("Invalid song on line {}: {}".format(i + 1, line.strip())) from error

        width = max(map(len, table.keys())) + 1
        offsetIni = open(offsets_ini, 'w', encoding='utf-8')

        offsetIni.truncate()
        for key in sorted(table.keys()):
            fstr = ('{:' + str(width) + '} {:08X}')
            offsetIni.write(fstr.format(key + ':', table[key] + 0x08000000) + '\n')
        offsetIni.close()

def _publish_files(files):
    """Publish staged outputs together, restoring previous files on failure."""
    published = []
    try:
        for staged, destination in files:
            backup = staged.with_name(staged.name + '.previous')
            had_previous = destination.exists()
            if had_previous:
                os.replace(destination, backup)
            published.append((destination, backup if had_previous else None))
            os.replace(staged, destination)
    except BaseException as error:
        rollback_errors = []
        for destination, backup in reversed(published):
            try:
                if backup is not None:
                    os.replace(backup, destination)
                else:
                    destination.unlink(missing_ok=True)
            except OSError as rollback_error:
                rollback_errors.append(str(rollback_error))
        if rollback_errors:
            raise RuntimeError('Output rollback failed; backups retained: ' + '; '.join(rollback_errors)) from error
        raise


def main():
    start_time = datetime.now()
    destination = Path(ROM_NAME).resolve()
    if destination == Path(SOURCE_ROM).resolve():
        raise ValueError('Source ROM and output ROM must have different paths')
    repoints_destination = Path(GENERATED_REPOINTS).resolve()
    offsets_destination = Path('offsets.ini').resolve()
    if Path(SOURCE_ROM).resolve() in {repoints_destination, offsets_destination}:
        raise ValueError('Source ROM cannot be a metadata output path')
    if len({destination, repoints_destination, offsets_destination}) != 3:
        raise ValueError('ROM and metadata output paths must be distinct')
    staged = Path(tempfile.mkdtemp(prefix='.cfru-insert-', dir=destination.parent))
    preserve_backups = False
    try:
        output_rom, generated_repoints, offsets_ini = (staged / name for name in ('rom', 'repoints', 'offsets'))
        if repoints_destination.is_file():
            shutil.copyfile(repoints_destination, generated_repoints)
        _insert(output_rom, generated_repoints, offsets_ini)
        _publish_files([(generated_repoints, repoints_destination),
                        (offsets_ini, offsets_destination), (output_rom, destination)])
    except BaseException as error:
        if any(staged.glob('*.previous')):
            preserve_backups = True
            raise RuntimeError('Insertion failed; previous outputs are retained in ' + str(staged)) from error
        raise
    finally:
        if not preserve_backups:
            if staged.resolve().parent != destination.parent or not staged.name.startswith('.cfru-insert-'):
                raise RuntimeError('Unexpected insertion staging directory: ' + str(staged))
            shutil.rmtree(staged)
    print('Inserted in ' + str(datetime.now() - start_time) + '.')


if __name__ == '__main__':
    main()
