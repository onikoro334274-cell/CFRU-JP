"""Execute extended-save access guards with controlled flash and heap operations."""

import argparse
import re
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--baseline', action='store_true')
    args = parser.parse_args()
    name = 'src/hack_ext_save.c'
    source = (subprocess.check_output(['git', 'show', '61e1b61653c1c87077860400a995ff3a36b81689:' + name], cwd=ROOT).decode('utf-8')
              if args.baseline else (ROOT / name).read_text(encoding='utf-8'))
    functions = []
    for name in ('HackExtSave_ReadSector', 'HackExtSave_Read', 'HackExtSave_WriteSector'):
        match = re.search(r'bool8 ' + name + r'\([^;{}]*\)\s*\{', source)
        end, depth = match.end(), 1
        while depth:
            depth += (source[end] == '{') - (source[end] == '}')
            end += 1
        functions.append(source[match.start():end])
    fixture = r'''
typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
typedef u8 bool8;
#define TRUE 1
#define FALSE 0
#define NULL ((void*)0)
#define EXT_FIRST_SECTOR 32
#define EXT_SECTOR_COUNT 736
#define EXT_SECTOR_SIZE 0x1000
#define FLASHTYPE_COPY_SIZE 0x20
static u32 flashType[8] = {0x20000};
static const u8 *gFlashPtr = (const u8*)flashType;
static u32 scratch[8];
static u8 available = TRUE, allocationFails;
static u32 reads, writes, frees, writeStatus;
static u16 lastSector, patchedCount;
static u32 lastOffset, lastSize;
static void *lastDestination;
bool8 HackExtSave_Available(void) { return available; }
void HackSaveCrypt_ReadFlash(u16 sector, u32 off, void *dst, u32 size) {
    ++reads; lastSector = sector; lastOffset = off; lastDestination = dst; lastSize = size;
}
void *Malloc(u32 size) { return allocationFails ? NULL : scratch; }
void Free(void *ptr) { ++frees; }
u32 ProgramFlashSectorAndVerifyVanilla(u16 sector, u8 *src) {
    ++writes; lastSector = sector; patchedCount = *(const u16*)(gFlashPtr + 0xA);
    return writeStatus;
}
''' + '\n'.join(functions) + r'''
int main(void) {
    static u8 buffer[0x1000];
    u32 before;
    if (HackExtSave_Read(0, 0xfff, buffer, 2) || reads) return 1;
    if (HackExtSave_Read(0, 0x1001, buffer, 0) || reads) return 2;
    if (HackExtSave_Read(0, 0xffffffff, buffer, 2) || reads) return 3;
    if (HackExtSave_Read(0, 1, buffer, 0xffffffff) || reads) return 4;
    if (HackExtSave_ReadSector(0, NULL) || reads) return 5;
    if (HackExtSave_Read(0, 0, NULL, 1) || reads) return 6;
    if (HackExtSave_WriteSector(0, NULL) || writes) return 7;
    if (HackExtSave_ReadSector(736, buffer) || reads) return 8;
    if (HackExtSave_WriteSector(736, buffer) || writes) return 9;
    available = FALSE;
    if (HackExtSave_ReadSector(0, buffer) || HackExtSave_Read(0, 0, buffer, 1)
        || HackExtSave_WriteSector(0, buffer) || reads || writes) return 10;
    available = TRUE;
    if (!HackExtSave_ReadSector(735, buffer) || reads != 1 || lastSector != 767
        || lastOffset || lastSize != 0x1000 || lastDestination != buffer) return 11;
    if (!HackExtSave_Read(0, 0xfff, buffer, 1) || reads != 2 || lastSector != 32
        || lastOffset != 0xfff || lastSize != 1) return 12;
    if (!HackExtSave_Read(0, 0x1000, buffer, 0) || reads != 3) return 13;
    if (!HackExtSave_WriteSector(735, buffer) || writes != 1 || lastSector != 767
        || patchedCount != 768 || gFlashPtr != (const u8*)flashType || frees != 1) return 14;
    writeStatus = 1;
    if (HackExtSave_WriteSector(0, buffer) || writes != 2 || frees != 2
        || gFlashPtr != (const u8*)flashType) return 15;
    before = writes;
    allocationFails = TRUE;
    if (HackExtSave_WriteSector(0, buffer) || writes != before || frees != 2) return 16;
    allocationFails = FALSE;
    gFlashPtr = NULL;
    if (HackExtSave_WriteSector(0, buffer) || writes != before) return 17;
    gFlashPtr = (const u8*)flashType;
    flashType[0] = 0;
    if (HackExtSave_WriteSector(0, buffer) || writes != before) return 18;
    return 0;
}
'''
    with tempfile.TemporaryDirectory() as directory:
        path = Path(directory)
        c_file, obj, exe = (path / name for name in ('save.c', 'save.obj', 'save.exe'))
        c_file.write_text(fixture, encoding='utf-8')
        subprocess.run(['clang', '--target=x86_64-pc-windows-msvc', '-O2',
                        '-fno-stack-protector', '-c', str(c_file), '-o', str(obj)], check=True)
        subprocess.run(['lld-link', '/entry:main', '/subsystem:console', '/nodefaultlib',
                        '/out:' + str(exe), str(obj)], check=True)
        result = subprocess.run([str(exe)])
        if args.baseline:
            if result.returncode != 1:
                raise RuntimeError('Original sector-boundary violation was not reproduced')
            print('Original HEAD accepts a read spanning beyond the extended-save sector.')
        else:
            result.check_returncode()
            print('Production extended-save guards passed boundary, overflow, null, '
                  'availability, allocation-failure and flash-metadata-restoration cases.')


if __name__ == '__main__':
    main()
