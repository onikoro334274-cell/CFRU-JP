"""Exercise non-profile field callback extra ticks and screen-change exits."""
import subprocess
import tempfile
from pathlib import Path
from check_mega_shop_escalation import function

ROOT = Path(__file__).resolve().parent.parent


def main():
    production = function((ROOT / 'src/overworld_speedup.c').read_text(encoding='utf-8'),
                          'void OverworldSpeedup_CB2Overworld(void)')
    fixture = r'''
typedef unsigned char u8;typedef unsigned int u32;typedef u8 bool32;
typedef void (*MainCallback)(void);
#define NULL ((void*)0)
#define PROF_BEGIN()
#define PROF_END(k)
static struct {u8 active;} palette;
#define gPaletteFade (&palette)
static struct {MainCallback callback2;} gMain;
static int speed,mode,threshold,helpers,basic,sprites,camera,panning,disabled,restored,errors;
static void normal(void) {}
static void other(void) {}
#define OverworldCB2_ normal
#define HELPER(name) static void name(void) {helpers++;}
HELPER(HackMomoIce_RefreshParty)
HELPER(HackMomSavings_FieldTick)
HELPER(HackTrainingPouch_FieldTick)
HELPER(HackMoemonSwitch_FieldTick)
HELPER(HackHabitatSwitch_FieldTick)
HELPER(HackMonId_FieldTick)
HELPER(HackFriendOt_FieldTick)
static void SetVBlankCallback(MainCallback callback) {if(callback) errors++;disabled++;}
static void SetFieldVBlankCallback_(void) {restored++;}
static void OverworldBasic_(void) {basic++;if(mode==1) gMain.callback2=other;}
static u8 OverworldSpeedup_ExtraIterations(void) {return speed;}
static void AnimateSprites(void) {sprites++;if(mode==2 && sprites==threshold) gMain.callback2=other;}
static void CameraUpdate_(void) {camera++;if(gMain.callback2!=normal) errors++;}
static void UpdateCameraPanning_(void) {panning++;if(gMain.callback2!=normal) errors++;}
PRODUCTION
int main(void) {
    for(speed=0;speed<4;speed++) for(int fade=0;fade<2;fade++)
    for(mode=0;mode<3;mode++) for(threshold=1;threshold<=3;threshold++) {
        palette.active=fade;gMain.callback2=normal;
        helpers=basic=sprites=camera=panning=disabled=restored=errors=0;
        OverworldSpeedup_CB2Overworld();
        int wantedSprites=mode==1 ? 0 : mode==2 && threshold<=speed ? threshold : speed;
        int wantedCamera=mode==2 && threshold<=speed ? wantedSprites-1 : wantedSprites;
        if(helpers!=7 || basic!=1 || sprites!=wantedSprites || camera!=wantedCamera || panning!=wantedCamera || errors) return 1;
        if(disabled!=fade || restored!=fade) return 2;
    }
    return 0;
}
'''.replace('PRODUCTION', production)
    with tempfile.TemporaryDirectory() as directory:
        path = Path(directory)
        c, obj, exe = (path / ('overworld_callback' + suffix) for suffix in ('.c', '.obj', '.exe'))
        c.write_text(fixture, encoding='utf-8')
        subprocess.run(['clang', '--target=x86_64-pc-windows-msvc', '-O2', '-fno-builtin',
                        '-fno-stack-protector', '-c', str(c), '-o', str(obj)], check=True)
        subprocess.run(['lld-link', '/entry:main', '/subsystem:console', '/nodefaultlib',
                        '/out:' + str(exe), str(obj)], check=True)
        subprocess.run([str(exe)], check=True)
    print('Field callback: 72 speed/fade/screen-change cases passed; extra camera ticks stop on screen change.')


if __name__ == '__main__':
    main()
