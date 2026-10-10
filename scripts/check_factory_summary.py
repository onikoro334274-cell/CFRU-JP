"""Execute the factory summary transition with controlled allocation failures."""
import argparse
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
BASELINE = '61e1b61653c1c87077860400a995ff3a36b81689'


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--baseline', action='store_true')
    args = parser.parse_args()
    name = 'src/hoenn_factory_screen.c'
    source = (subprocess.check_output(['git', 'show', BASELINE + ':' + name], cwd=ROOT).decode('utf-8')
              if args.baseline else (ROOT / name).read_text(encoding='utf-8'))
    start = source.index('static void Select_Task_OpenSummaryScreen(u8 taskId)\n{')
    end = source.index('\nstatic void Select_Task_Exit', start)
    function = source[start:end]
    fixture = r'''
typedef unsigned char u8;
typedef unsigned short u16;
#define NULL ((void *)0)
#define TRUE 1
#define STATE_MENU_HANDLE_INPUT 3
#define STATE_SUMMARY_FADE 6
#define STATE_SUMMARY_CLEAN 7
#define STATE_SUMMARY_SHOW 8
#define SELECTABLE_MONS_COUNT 6
#define PALNUM_FADE_TEXT 0
#define BG_PLTT_ID(n) (n)
#define PALETTES_ALL 0
#define RGB_BLACK 0
#define PSS_MODE_UNK1 0
#define FREE_AND_SET_NULL(p) do { cleanup++; p = NULL; } while (0)
struct Pokemon { unsigned int value; };
struct Task { int tState; void (*func)(u8); } gTasks[1];
struct Mon { struct Pokemon monData; };
struct Screen {
    u8 fadeSpeciesNameTaskId, monPicAnimating, fromSummaryScreen, cursorPos;
    unsigned int monPics[2];
    u16 speciesNameColorBackup;
    struct Mon mons[SELECTABLE_MONS_COUNT];
} screen, *sFactorySelectScreen = &screen;
struct Fade { u8 active; } fade, *gPaletteFade = &fade;
struct Pokemon party[SELECTABLE_MONS_COUNT], *sFactorySelectMons;
void *sSelectMenuTilesetBuffer, *sSelectMonPicBgTilesetBuffer;
void *sSelectMenuTilemapBuffer, *sSelectMonPicBgTilemapBuffer;
u16 gPlttBufferUnfaded[16], gPlttBufferFaded[16];
int failAllocation, allocations, cleanup, paletteFades, destroys, summaryShows, bad;
static void Select_Task_HandleMenu(u8 id) { (void)id; }
static void CB2_InitSelectScreen(void) {}
static void *AllocZeroed(unsigned long long size) {
    allocations++;
    if (size != sizeof(party)) bad = 1;
    return failAllocation ? NULL : party;
}
static void BeginNormalPaletteFade(int a,int b,int c,int d,int e) {
    (void)a;(void)b;(void)c;(void)d;(void)e; paletteFades++;
}
static void DestroyTask(u8 id) { (void)id; destroys++; }
static void HideMonPic(unsigned int pic, u8 *animating) {
    (void)pic;(void)animating;cleanup++;
}
static void Select_DestroyAllSprites(void) { cleanup++; }
static void FreeAllWindowBuffers(void) { cleanup++; }
static void Memcpy(void *dst, const void *src, unsigned long long size) {
    if (!dst) { bad = 1; return; }
    for (unsigned long long i=0; i<size; i++) ((u8*)dst)[i] = ((const u8*)src)[i];
}
static void BwShowPokemonSummaryScreen(struct Pokemon *mons, u8 index, u8 last,
                                      void (*callback)(void), int mode) {
    summaryShows++;
    if (mons != party || index != 2 || last != 5 || callback != CB2_InitSelectScreen || mode != 0)
        bad = 1;
}
FUNCTION
int main(void) {
    failAllocation = 1;
    gTasks[0].tState = STATE_SUMMARY_FADE;
    gTasks[0].func = Select_Task_OpenSummaryScreen;
    gPlttBufferUnfaded[4] = 10; gPlttBufferFaded[4] = 20;
    Select_Task_OpenSummaryScreen(0);
    if (gTasks[0].tState != STATE_MENU_HANDLE_INPUT || gTasks[0].func != Select_Task_HandleMenu
        || allocations != 1 || cleanup || paletteFades || destroys || summaryShows
        || gPlttBufferUnfaded[4] != 10 || screen.fromSummaryScreen || sFactorySelectMons)
        return 1;
    failAllocation = 0; allocations = 0;
    screen.cursorPos = 2;
    for (u8 i=0;i<SELECTABLE_MONS_COUNT;i++) screen.mons[i].monData.value = 100+i;
    gTasks[0].tState = STATE_SUMMARY_FADE;
    Select_Task_OpenSummaryScreen(0);
    if (allocations != 1 || paletteFades != 1 || gTasks[0].tState != STATE_SUMMARY_CLEAN
        || cleanup || gPlttBufferUnfaded[4] != 20) return 2;
    fade.active = 1;
    Select_Task_OpenSummaryScreen(0);
    if (cleanup || destroys || gTasks[0].tState != STATE_SUMMARY_CLEAN) return 3;
    fade.active = 0;
    Select_Task_OpenSummaryScreen(0);
    if (cleanup != 7 || destroys != 1 || gTasks[0].tState != STATE_SUMMARY_SHOW) return 4;
    Select_Task_OpenSummaryScreen(0);
    if (allocations != 1 || destroys != 2 || summaryShows != 1 || bad || !screen.fromSummaryScreen)
        return 5;
    for (u8 i=0;i<SELECTABLE_MONS_COUNT;i++) if (party[i].value != 100+i) return 6;
    return 0;
}
'''.replace('FUNCTION', function)
    with tempfile.TemporaryDirectory() as directory:
        path = Path(directory)
        c_file, obj, exe = (path / name for name in ('summary.c', 'summary.obj', 'summary.exe'))
        c_file.write_text(fixture, encoding='utf-8')
        subprocess.run(['clang', '--target=x86_64-pc-windows-msvc', '-O2', '-fno-builtin',
                        '-fno-stack-protector', '-c', str(c_file), '-o', str(obj)], check=True)
        subprocess.run(['lld-link', '/entry:main', '/subsystem:console', '/nodefaultlib',
                        '/out:' + str(exe), str(obj)], check=True)
        result = subprocess.run([str(exe)])
        if args.baseline:
            if result.returncode != 1:
                raise RuntimeError('Original unsafe summary transition was not reproduced')
            print('Original baseline starts tearing down the screen without a summary buffer.')
        else:
            result.check_returncode()
            print('Factory summary allocation failure preserves the menu; successful fade, cleanup and copy pass.')


if __name__ == '__main__':
    main()
