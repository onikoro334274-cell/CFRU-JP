"""Execute production save-screen allocation and window-failure control flow."""

import argparse
import re
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent


def function(source, signature):
    match = re.search(signature + r'\s*\{', source)
    if match is None:
        raise ValueError('Screen function not found')
    end, depth = match.end(), 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[match.start():end]


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--baseline-windows', action='store_true')
    args = parser.parse_args()
    source = (ROOT / 'src/hack_save_slot.c').read_text(encoding='utf-8')
    windows = (ROOT / 'src/hoenn_frontier.c').read_text(encoding='utf-8')
    if args.baseline_windows:
        windows = subprocess.check_output(['git', 'show', '61e1b61653c1c87077860400a995ff3a36b81689:src/hoenn_frontier.c'],
                                          cwd=ROOT).decode('utf-8')
    structs = '\n'.join(re.search(r'struct ' + name + r'\s*\{[^}]*\};', source)[0]
                        for name in ('SlotInfo', 'SlotScreen'))
    create = function(source, r'static bool8 CreateSlotScreen\([^;{}]*\)')
    skip = function(source, r'static void SkipSlotScreen\([^;{}]*\)')
    init_windows = function(windows, r'bool16 HoennFrontier_InitWindowsZeroed\([^;{}]*\)')
    fixture = r'''
typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
typedef u8 bool8;
typedef u16 bool16;
#define TRUE 1
#define FALSE 0
#define NULL ((void*)0)
''' + structs + r'''
static struct SlotScreen screen;
static struct { short data[16]; } gTasks[16];
static u8 allocationFails, taskCalls, infoCalls, lastSlot;
static u8 infoOrder[2], cleanupOrder;
void *AllocZeroed(u32 size) {
    if (size != sizeof(screen)) return NULL;
    return allocationFails ? NULL : &screen;
}
void Task_HackSaveSlot(u8 taskId) {}
u8 CreateTask(void (*func)(u8), u8 priority) { ++taskCalls; return 5; }
void ReadSlotInfo(struct SlotInfo *info, u8 slot) {
    if (infoCalls < 2) infoOrder[infoCalls] = slot;
    ++infoCalls; info->status = slot + 1;
}
void FreeAllWindowBuffers(void) { cleanupOrder = cleanupOrder * 10 + 1; }
void ResetTasks(void) { cleanupOrder = cleanupOrder * 10 + 2; }
void LoadChosenAndGoToMainMenu(u8 slot) {
    cleanupOrder = cleanupOrder * 10 + 3; lastSlot = slot;
}
struct WindowTemplate { u8 bg; };
static u8 storage[4];
static void *bgBuffers[4];
static u8 windowFailure, pixelFills, tilemapFills;
static u32 sizesFilled[4];
#define BG_TILEMAP_OF(bg) bgBuffers[bg]
#define BG_ATTR_SCREENSIZE 0
#define PIXEL_FILL(value) value
bool16 InitWindows(const struct WindowTemplate *templates) {
    bgBuffers[0] = &storage[0]; bgBuffers[2] = &storage[2];
    return !windowFailure;
}
void FillWindowPixelBuffer(u8 windowId, u8 value) { ++pixelFills; }
u8 GetBgAttribute(u8 bg, u8 attribute) { return bg == 2 ? 3 : 0; }
void CpuFill16(u16 value, void *dst, u32 size) {
    ++tilemapFills;
    for (u8 bg = 0; bg < 4; ++bg)
        if (dst == &storage[bg]) sizesFilled[bg] = size;
}
''' + create + '\n' + skip + '\n' + init_windows + r'''
int main(void) {
    allocationFails = TRUE;
    if (CreateSlotScreen(1, FALSE) || taskCalls || infoCalls) return 1;
    allocationFails = FALSE;
    if (!CreateSlotScreen(1, FALSE) || taskCalls != 1 || infoCalls != 2
        || infoOrder[0] != 1 || infoOrder[1] != 0 || screen.cursor != 1
        || screen.unavailable || screen.info[0].status != 1 || screen.info[1].status != 2)
        return 2;
    if ((u16)gTasks[5].data[0] != (u16)(u32)&screen
        || (u16)gTasks[5].data[1] != (u16)((u32)&screen >> 16)) return 3;
    infoCalls = 0; screen.cursor = 0;
    if (!CreateSlotScreen(1, TRUE) || taskCalls != 2 || infoCalls || !screen.unavailable
        || screen.cursor) return 4;
    SkipSlotScreen(1);
    if (cleanupOrder != 123 || lastSlot != 1) return 5;
    const struct WindowTemplate templates[] = {{0}, {1}, {0xff}};
    windowFailure = TRUE;
    bgBuffers[1] = &storage[1];
    if (HoennFrontier_InitWindowsZeroed(templates) || pixelFills || tilemapFills) return 6;
    windowFailure = FALSE; bgBuffers[0] = NULL; bgBuffers[2] = NULL;
    if (!HoennFrontier_InitWindowsZeroed(templates) || pixelFills != 2 || tilemapFills != 2
        || sizesFilled[0] != 0x800 || sizesFilled[2] != 0x2000
        || sizesFilled[1] || bgBuffers[1] != &storage[1]) return 7;
    return 0;
}
'''
    with tempfile.TemporaryDirectory() as directory:
        path = Path(directory)
        c_file, obj, exe = (path / name for name in ('screen.c', 'screen.obj', 'screen.exe'))
        c_file.write_text(fixture, encoding='utf-8')
        subprocess.run(['clang', '--target=x86_64-pc-windows-msvc', '-O2',
                        '-Wno-pointer-to-int-cast', '-fno-stack-protector', '-c',
                        str(c_file), '-o', str(obj)], check=True)
        subprocess.run(['lld-link', '/entry:main', '/subsystem:console', '/nodefaultlib',
                        '/out:' + str(exe), str(obj)], check=True)
        result = subprocess.run([str(exe)])
        if args.baseline_windows:
            if result.returncode != 6:
                raise RuntimeError('Original window failure was not reproduced')
            print('Original HEAD fills window/tilemap buffers after InitWindows fails.')
            return
        result.check_returncode()
    print('Production screen helpers passed allocation failure, slot-load order, '
          'cleanup order, unavailable-slot behavior and window-initialization cases.')


if __name__ == '__main__':
    main()
