"""Execute doubled box callbacks with guarded inputs and screen transitions."""
import subprocess
import tempfile
from pathlib import Path
from check_mega_shop_escalation import function

ROOT = Path(__file__).resolve().parent.parent


def main():
    source = (ROOT / 'src/hack_box_speed.c').read_text(encoding='utf-8')
    production = '\n'.join(function(source, signature) for signature in (
        'static void Body(bool8 extra)', 'void HackBoxSpeed_CB2PokeStorage(void)'))
    fixture = r'''
typedef unsigned short u16;typedef unsigned int u32;typedef void (*MainCallback)(void);
typedef unsigned char bool8;
#define FALSE 0
#define TRUE 1
#define BOX_SPEED_EXTRA 1
static void normal(void) {}
static void other(void) {}
#define CB2_PokeStorage_ normal
static struct {MainCallback callback2;u16 newKeys,newKeysRaw,newAndRepeatedKeys,heldKeys;} gMain;
static int stage,calls,errors;static u16 a,b,c,held;
static void step(int which) {
    if(which!=calls%6) errors++;
    int extra=calls>=6;
    if(gMain.newKeys!=(extra ? 0 : a) || gMain.newKeysRaw!=(extra ? 0 : b) ||
       gMain.newAndRepeatedKeys!=(extra ? 0 : c) || gMain.heldKeys!=held) errors++;
    calls++;
    if(stage && calls==stage) gMain.callback2=other;
}
static void RunTasks(void) {step(0);}
static void DoScheduledBgTilemapCopiesToVram_(void) {step(1);}
static void ScrollBackground_(void) {step(2);}
static void UpdateCloseBoxButtonFlash_(void) {step(3);}
static void AnimateSprites(void) {step(4);}
static void BuildOamBuffer(void) {step(5);}
PRODUCTION
int main(void) {
    for(int mask=0;mask<8;mask++) for(int heldMode=0;heldMode<2;heldMode++)
    for(stage=0;stage<=12;stage++) {
        a=mask&1 ? 65535 : 0;b=mask&2 ? 65535 : 0;c=mask&4 ? 65535 : 0;
        held=heldMode ? 65535 : 0;
        gMain.newKeys=a;gMain.newKeysRaw=b;gMain.newAndRepeatedKeys=c;gMain.heldKeys=held;
        gMain.callback2=normal;calls=errors=0;HackBoxSpeed_CB2PokeStorage();
        if(errors || calls!=(stage>6 ? stage : stage ? 6 : 12)) return 1;
        if(gMain.newKeys!=a || gMain.newKeysRaw!=b || gMain.newAndRepeatedKeys!=c || gMain.heldKeys!=held) return 2;
        if(gMain.callback2!=(stage ? other : normal)) return 3;
    }
    return 0;
}
'''.replace('PRODUCTION', production)
    with tempfile.TemporaryDirectory() as directory:
        path = Path(directory)
        c, obj, exe = (path / ('box_speed' + suffix) for suffix in ('.c', '.obj', '.exe'))
        c.write_text(fixture, encoding='utf-8')
        subprocess.run(['clang', '--target=x86_64-pc-windows-msvc', '-O2', '-fno-builtin',
                        '-fno-stack-protector', '-c', str(c), '-o', str(obj)], check=True)
        subprocess.run(['lld-link', '/entry:main', '/subsystem:console', '/nodefaultlib',
                        '/out:' + str(exe), str(obj)], check=True)
        subprocess.run([str(exe)], check=True)
    print('Box speed: 208 fresh/repeat/held-input and primary/extra screen-transition cases passed.')


if __name__ == '__main__':
    main()
