#!/usr/bin/env python3

import subprocess
import sys
from pathlib import Path
from rom_bounds import MAX_ROM_SIZE, validate_payload

############
# Options go here.
############

ROM_NAME = "BPRJ0.gba"  # The name of your rom
OFFSET_TO_PUT = 0xfe0000
SEARCH_FREE_SPACE = True  # Set to True if you want the script to search for free space
                           # Set to False if you don't want to search for free space as you for example update the engine

#############
# Options end here.
#############

###############
# Functions start here.
###############


def MakeOffset0x100Aligned(offset: int) -> int:
    return (offset + 15) & ~15


def FindOffsetToPut(rom, neededBytes: int, startOffset: int, endOffset=None) -> int:
    if neededBytes <= 0 or startOffset < 0:
        raise ValueError("Free-space size must be positive and offset nonnegative.")
    offset = MakeOffset0x100Aligned(startOffset)
    rom.seek(offset)
    numFoundBytes = 0
    position = offset
    while True:
        readSize = 65536 if endOffset is None else min(65536, max(0, endOffset - position))
        chunk = rom.read(readSize)
        if not chunk:
            raise ValueError("End of file reached. Not enough free space.")
        for value in chunk:
            if position >= offset:
                if value == 0xFF:
                    numFoundBytes += 1
                    if numFoundBytes == neededBytes:
                        return offset
                else:
                    offset = MakeOffset0x100Aligned(position + 1)
                    numFoundBytes = 0
            position += 1


def ChangeFileLine(filePath: str, lineToChange: int, replacement: str):
    with open(filePath, 'r') as file:
        lines = file.readlines()
    if not 1 <= lineToChange <= len(lines):
        raise ValueError("Line to change is outside the file.")
    lines[lineToChange - 1] = replacement

    with open(filePath, 'w') as file:
        file.writelines(lines)


def EditLinker(offset: int):
    ChangeFileLine("linker.ld", 4, "\t\trom     : ORIGIN = (0x08000000 + " + hex(offset) + "), LENGTH = 32M\n")


def EditInsert(offset: int):
    ChangeFileLine("./scripts/insert.py", 10, "OFFSET_TO_PUT = " + hex(offset) + '\n')
    ChangeFileLine("./scripts/insert.py", 11, 'SOURCE_ROM = "' + ROM_NAME + '"\n')


def BuildCode():
    subprocess.run([sys.executable, "scripts/build.py"], check=True)


def InsertCode():
    subprocess.run([sys.executable, "scripts/insert.py"], check=True)


def ClearFromTo(rom, from_: int, to_: int):
    rom.seek(from_)
    for i in range(0, to_ - from_):
        rom.write(b'\xFF')

##############
# Functions end here.
##############


def main():
    try:
        with open(ROM_NAME, 'rb') as rom:
            offset = OFFSET_TO_PUT
            validate_payload(offset, 1)
            if SEARCH_FREE_SPACE is True:
                offset = MakeOffset0x100Aligned(offset)

            while True:
                EditLinker(offset)
                EditInsert(offset)
                BuildCode()
                size = Path('build/output.bin').stat().st_size
                validate_payload(offset, size)
                if not SEARCH_FREE_SPACE:
                    break
                verifiedOffset = FindOffsetToPut(rom, size, offset, MAX_ROM_SIZE)
                if verifiedOffset == offset:
                    break
                offset = verifiedOffset
            InsertCode()

    except FileNotFoundError as error:
        print('Error: Required build input or output is missing: ' + str(error.filename))
        sys.exit(1)


if __name__ == '__main__':
    main()

