"""Execute option cancel countdown and restore the saved cry sound mode."""
import subprocess
import tempfile
from pathlib import Path
from check_mega_shop_escalation import function

ROOT = Path(__file__).resolve().parent.parent


def main():
    source = (ROOT / 'src/bw_option_menu.c').read_text(encoding='utf-8')
    # Skip the forward declaration when extracting the implementation.
    source = source[source.index('static void Task_Cancel(u8 taskId)\n{'):]
    production = function(source, 'static void Task_Cancel(u8 taskId)')
    fixture = r'''
typedef unsigned char u8;typedef short s16;
#define TD_TIMER 11
#define DESC_CANCEL 99
#define RGB_BLACK 0
static struct {u8 optionsSound;} saved;
#define gSaveBlock2 (&saved)
static struct {s16 data[16];void (*func)(u8);} gTasks[2];
static int sound,restores,descriptions,fades,errors;
static void Task_FadeOut(u8 id) {(void)id;}
static void original(u8 id) {(void)id;}
static void ShowDescription(int which) {if(which!=99) errors++;descriptions++;}
static void SetPokemonCryStereo(int mode) {sound=mode;restores++;}
static void BeginNormalPaletteFade(unsigned mask,int delay,int from,int to,int color) {
    if(mask!=0xFFFFFFFF || delay || from || to!=16 || color || sound!=saved.optionsSound) errors++;
    fades++;
}
PRODUCTION
int main(void) {
    for(int prior=0;prior<2;prior++) for(int preview=0;preview<2;preview++) {
        saved.optionsSound=prior;sound=preview;restores=descriptions=fades=errors=0;
        gTasks[1].data[11]=20;gTasks[1].func=original;gTasks[0].data[11]=123;
        for(int tick=0;tick<20;tick++) {
            Task_Cancel(1);
            if(sound!=preview || restores || fades || gTasks[1].func!=original || gTasks[1].data[11]!=19-tick) return 1;
        }
        Task_Cancel(1);
        if(sound!=prior || restores!=1 || fades!=1 || descriptions!=1 || errors || saved.optionsSound!=prior) return 2;
        if(gTasks[1].func!=Task_FadeOut || gTasks[0].data[11]!=123) return 3;
    }
    return 0;
}
'''.replace('PRODUCTION', production)
    with tempfile.TemporaryDirectory() as directory:
        path = Path(directory)
        c, obj, exe = (path / ('option_cancel' + suffix) for suffix in ('.c', '.obj', '.exe'))
        c.write_text(fixture, encoding='utf-8')
        subprocess.run(['clang', '--target=x86_64-pc-windows-msvc', '-O2', '-fno-builtin',
                        '-fno-stack-protector', '-c', str(c), '-o', str(obj)], check=True)
        subprocess.run(['lld-link', '/entry:main', '/subsystem:console', '/nodefaultlib',
                        '/out:' + str(exe), str(obj)], check=True)
        subprocess.run([str(exe)], check=True)
    print('Option cancel: both saved and preview sound modes passed all 21 countdown/exit ticks.')


if __name__ == '__main__':
    main()
