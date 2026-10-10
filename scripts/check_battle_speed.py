"""Exercise tagged speed storage and callback/input isolation of extra ticks."""
import subprocess
import tempfile
from pathlib import Path
from check_mega_shop_escalation import function

ROOT = Path(__file__).resolve().parent.parent


def main():
    source = (ROOT / 'src/hack_battle_speed.c').read_text(encoding='utf-8')
    production = '\n'.join(function(source, signature) for signature in (
        'u16 HackBattleSpeed_Get(void)', 'void HackBattleSpeed_Set(u16 speed)',
        'static u8 ExtraIterations(void)', 'static void BattleMainCB2_Body(bool8 fade)',
        'void HackBattleSpeed_BattleMainCB2(void)'))
    fixture = r'''
typedef unsigned char u8;typedef unsigned short u16;typedef unsigned int u32;typedef u8 bool8;
typedef void (*MainCallback)(void);
#define NULL ((void*)0)
#define TRUE 1
#define FALSE 0
#define BATTLE_SPEED_MAX 3
#define BATTLE_SPEED_TAG 0xBA00
#define HACK_VAR_BATTLE_SPEED 0x50F7
#define BATTLE_TYPE_LINK 1
#define BATTLE_TYPE_TOWER_LINK_MULTI 2
#define BATTLE_TYPE_POKE_DUDE 4
#define B_BUTTON 2
#define B_OUTCOME_DREW 3
#define RGB_BLACK 0
#define JOY_HELD(key) (held&(key))
static u16 stored,gSpecialVar_LastResult;static u8 gBattleOutcome;
static u32 gBattleTypeFlags,held;
static struct {MainCallback callback1,callback2;u16 newKeys,newKeysRaw,newAndRepeatedKeys;} gMain;
static int bodies,cbCalls,fades,mode,threshold,errors,quitFades;
static void normal(void) {}
static void different(void) {}
static void quit(void) {}
#define BattleMainCB2_ normal
#define CB2_QuitPokedudeBattle_ quit
static u16 VarGet(u16 id) {if(id!=HACK_VAR_BATTLE_SPEED) errors++;return stored;}
static void VarSet(u16 id,u16 value) {if(id!=HACK_VAR_BATTLE_SPEED) errors++;stored=value;}
static void AnimateSprites(void) {
    bodies++;
    if(bodies>1 && (gMain.newKeys || gMain.newKeysRaw || gMain.newAndRepeatedKeys)) errors++;
}
static void BuildOamBuffer(void) {}
static void RunTextPrinters(void) {}
static void UpdatePaletteFade(void) {fades++;}
static void RunTasks(void) {if(mode==3 && bodies==threshold) gMain.callback2=different;}
static void ResetPaletteFadeControl(void) {}
static void BeginNormalPaletteFade(u32 mask,int delay,int from,int to,int color) {
    if(mask!=0xFFFFFFFF || delay || from || to!=16 || color) errors++;quitFades++;
}
static void SetMainCallback2(MainCallback callback) {gMain.callback2=callback;}
static void cb1(void) {
    cbCalls++;
    if(gMain.newKeys || gMain.newKeysRaw || gMain.newAndRepeatedKeys) errors++;
    if(cbCalls==threshold) {
        if(mode==1) gMain.callback1=different;
        if(mode==2) gMain.callback2=different;
    }
}
PRODUCTION
static void reset(int speed) {
    stored=0xBA00|speed;held=gBattleTypeFlags=0;bodies=cbCalls=fades=errors=quitFades=0;
    gMain.callback1=cb1;gMain.callback2=normal;
    gMain.newKeys=7;gMain.newKeysRaw=11;gMain.newAndRepeatedKeys=13;
}
int main(void) {
    for(u32 raw=0;raw<65536;raw++) {
        stored=raw;
        u16 expected=(raw&0xFF00)==0xBA00 && (raw&255)<=3 ? raw&255 : 0;
        if(HackBattleSpeed_Get()!=expected) return 1;
        HackBattleSpeed_Set(raw);
        if(stored!=(0xBA00|(raw<=3 ? raw : 0))) return 2;
    }
    for(int speed=0;speed<4;speed++) for(int flags=0;flags<4;flags++)
    for(mode=0;mode<4;mode++) for(threshold=1;threshold<=4;threshold++) {
        reset(speed);gBattleTypeFlags=flags;
        int extra=flags ? 0 : speed,expectedCb=0,expectedBodies=1;
        int active=!(mode==3 && threshold==1);
        for(int i=0;i<extra && active;i++) {
            expectedCb++;
            if((mode==1 || mode==2) && expectedCb==threshold) break;
            expectedBodies++;
            if(mode==3 && expectedBodies==threshold) active=0;
        }
        HackBattleSpeed_BattleMainCB2();
        if(cbCalls!=expectedCb || bodies!=expectedBodies || fades!=1 || errors) return 3;
        if(gMain.newKeys!=7 || gMain.newKeysRaw!=11 || gMain.newAndRepeatedKeys!=13) return 4;
    }
    reset(3);mode=0;gMain.callback1=NULL;HackBattleSpeed_BattleMainCB2();
    if(cbCalls || bodies!=1 || fades!=1 || errors || gMain.newKeys!=7) return 5;
    reset(3);held=B_BUTTON;gBattleTypeFlags=BATTLE_TYPE_POKE_DUDE;
    HackBattleSpeed_BattleMainCB2();
    if(cbCalls || bodies!=1 || quitFades!=1 || gMain.callback2!=quit || gBattleOutcome!=3 || gSpecialVar_LastResult!=3 || errors) return 6;
    return 0;
}
'''.replace('PRODUCTION', production)
    with tempfile.TemporaryDirectory() as directory:
        path = Path(directory)
        c, obj, exe = (path / ('battle_speed' + suffix) for suffix in ('.c', '.obj', '.exe'))
        c.write_text(fixture, encoding='utf-8')
        subprocess.run(['clang', '--target=x86_64-pc-windows-msvc', '-O2', '-fno-builtin',
                        '-fno-stack-protector', '-c', str(c), '-o', str(obj)], check=True)
        subprocess.run(['lld-link', '/entry:main', '/subsystem:console', '/nodefaultlib',
                        '/out:' + str(exe), str(obj)], check=True)
        subprocess.run([str(exe)], check=True)
    print('Battle speed: every halfword storage input and 258 callback/link/input/fade scenarios passed.')


if __name__ == '__main__':
    main()
