"""Execute menu scrolling and difficulty cycling in both menu variants."""
import re
import subprocess
import tempfile
from pathlib import Path
from check_ai_difficulty import function

ROOT = Path(__file__).resolve().parent.parent


def main():
    source = (ROOT / 'src/bw_option_menu.c').read_text(encoding='utf-8')
    enums = '\n'.join(re.search(r'enum\s*\{\s*' + name + r'\b.*?\};', source, re.S)[0]
                      for name in ('TD_MENUSELECTION', 'MENUITEM_TEXTSPEED'))
    production = '\n'.join(function(source, name) for name in ('CycleInput', 'ProcessInputFor', 'Task_ProcessInput', 'LoadFrameGfx'))
    beginning = source.index('data[TD_TEXTSPEED] = gSaveBlock2->optionsTextSpeed;')
    ending = source.index('data[TD_BATTLESCENE] =', beginning)
    production += '\nstatic void InitTextSpeed(s16* data) {\n' + source[beginning:ending] + '\n}\n'
    fixture = r'''
typedef unsigned char u8;typedef short s16;
#define TRUE 1
#define FALSE 0
#define A_BUTTON 1
#define B_BUTTON 2
#define DPAD_UP 4
#define DPAD_DOWN 8
#define DPAD_RIGHT 16
#define DPAD_LEFT 32
#define WIN_TEXT 0
#define BWOPT_VISIBLE_ROWS 6
#define BWOPT_SPEED_COUNT 4
#define DIFFICULTY_COUNT 5
#define OPTIONS_TEXT_SPEED_FAST 2
#define OPTIONS_TEXT_SPEED_MID 1
#define BWOPT_FRAME_COUNT 10
#define BWOPT_FRAME_TILE 0x1A2
#define BWOPT_FRAME_PAL 14
#define VRAM 0
#define JOY_NEW(k) (keys&(k))
ENUMS
static unsigned keys;static int errors,descriptions,rows,mapIndex;
static struct {unsigned optionsTextSpeed:3;} saved;
#define gSaveBlock2 (&saved)
static int cryCalls,cryMode;
struct TilesPal {const u8* tiles;const unsigned short* pal;};
static const u8 frameTiles[288]={0};static const unsigned short framePal[16]={0};
static const struct TilesPal frame={frameTiles,framePal};
static int frameIndex,frameLoads;
static const struct TilesPal* GetWindowFrameTilesPal(u8 index) {frameIndex=index;return &frame;}
static void LoadBgTiles(int bg,const u8* tiles,int size,int start) {
    if(bg!=2 || tiles!=frameTiles || size!=288 || start!=0x1A2) errors++;frameLoads++;
}
static void LoadPalette(const unsigned short* pal,int start,int size) {
    if(pal!=framePal || start!=224 || size!=32) errors++;frameLoads++;
}
static struct {s16 data[16];void (*func)(u8);} gTasks[1];
static const u8 maps[6]={0,1,2,3,4,5};
static const u8* sMaps[6]={maps,maps+1,maps+2,maps+3,maps+4,maps+5};
static void Task_Save(u8 id) {(void)id;}
static void Task_Cancel(u8 id) {(void)id;}
static void original(u8 id) {(void)id;}
static void DrawRows(const s16* data) {
    int item=data[TD_MENUSELECTION],top=data[TD_TOP];rows++;
    if(item<top || item>=top+6 || top<0 || item>=MENUITEM_COUNT) errors++;
}
static void LZ77UnCompVram(const u8* source,void* dest) {(void)dest;mapIndex=*source;if(mapIndex>=6) errors++;}
static void ShowDescription(u8 item) {descriptions++;if(item>=MENUITEM_COUNT) errors++;}
static void SetPokemonCryStereo(u8 mode) {cryCalls++;cryMode=mode;}
static void DrawChoicesFor(u8 item,u8 selection,u8 top) {(void)item;(void)selection;(void)top;}
static void CopyWindowToVram(u8 id,u8 mode) {(void)id;(void)mode;}
PRODUCTION
int main(void) {
    if(TD_TOP>=16 || TD_TEXTSPEED+MENUITEM_COUNT!=TD_TIMER) return 1;
    for(int raw=0;raw<256;raw++) {
        frameIndex=-1;frameLoads=errors=0;LoadFrameGfx(raw);
        if(frameIndex!=(raw<10 ? raw : 0) || frameLoads!=2 || errors) return 9;
    }
    for(int raw=0;raw<8;raw++) {
        saved.optionsTextSpeed=raw;
        InitTextSpeed(gTasks[0].data);
        if(gTasks[0].data[TD_TEXTSPEED]!=(raw<3 ? raw : 1) || saved.optionsTextSpeed!=(unsigned)raw) return 8;
    }
    for(int item=0;item<MENUITEM_COUNT;item++) for(int top=0;top<=item;top++) {
        if(item>=top+6) continue;
        for(keys=0;keys<16;keys++) {
            for(int i=0;i<16;i++) gTasks[0].data[i]=0;
            gTasks[0].data[TD_MENUSELECTION]=item;gTasks[0].data[TD_TOP]=top;
            gTasks[0].func=original;errors=descriptions=rows=0;mapIndex=-1;
            int expected=item,newTop=top;
            if(!(keys&3) && (keys&12)) {
                expected=keys&4 ? (item ? item-1 : MENUITEM_COUNT-1) : (item+1)%MENUITEM_COUNT;
                if(expected<top || expected>=top+6) newTop=expected<top ? expected : expected-5;
            }
            Task_ProcessInput(0);
            if(errors || gTasks[0].data[TD_MENUSELECTION]!=expected || gTasks[0].data[TD_TOP]!=newTop) return 2;
            if(keys&3) {
                if(gTasks[0].func!=((keys&1) ? Task_Save : Task_Cancel) || gTasks[0].data[TD_TIMER]!=20) return 3;
            } else if(keys&12) {
                if(descriptions!=1 || rows!=(newTop!=top) || mapIndex!=expected-newTop) return 4;
            }
        }
    }
    for(int count=2;count<=5;count++) for(int selection=0;selection<count;selection++)
    for(int mask=0;mask<4;mask++) {
        keys=mask*16;s16 arrow=0;int expected=selection;
        if(mask&1) expected=(expected+1)%count;
        if(mask&2) expected=(expected+count-1)%count;
        if(CycleInput(selection,count,&arrow)!=expected || arrow!=(mask!=0)) return 5;
    }
    for(int item=0;item<MENUITEM_COUNT;item++) {
        int count=item==MENUITEM_TEXTSPEED ? 3 : item==MENUITEM_DIFFICULTY ? 5 :
                  (item==MENUITEM_FIELDSPEED || item==MENUITEM_BATTLESPEED) ? 4 : 2;
        for(int selection=0;selection<count;selection++) for(int mask=0;mask<4;mask++) {
            keys=mask*16;s16 arrow=0;int expected=selection;cryCalls=0;cryMode=-1;
            if(mask&1) expected=(expected+1)%count;
            if(mask&2) expected=(expected+count-1)%count;
            if(ProcessInputFor(item,selection,&arrow)!=expected || arrow!=(mask!=0)) return 6;
            int soundChanged=item==MENUITEM_SOUND && expected!=selection;
            if(cryCalls!=soundChanged || (soundChanged && cryMode!=expected)) return 7;
        }
    }
    return 0;
}
'''.replace('ENUMS', enums).replace('PRODUCTION', production)
    with tempfile.TemporaryDirectory() as directory:
        path = Path(directory)
        c, obj, exe = (path / ('option_navigation' + suffix) for suffix in ('.c', '.obj', '.exe'))
        c.write_text(fixture, encoding='utf-8')
        for omitted in (False, True):
            flags = ['-DHACK_NO_MOEMON'] if omitted else []
            subprocess.run(['clang', '--target=x86_64-pc-windows-msvc', '-O2', '-fno-builtin',
                            '-fno-stack-protector', *flags, '-c', str(c), '-o', str(obj)], check=True)
            subprocess.run(['lld-link', '/entry:main', '/subsystem:console', '/nodefaultlib',
                            '/out:' + str(exe), str(obj)], check=True)
            subprocess.run([str(exe)], check=True)
    print('Option navigation: both menu variants passed scrolling, choice dispatch, 2..5-choice cycling and sound preview calls.')


if __name__ == '__main__':
    main()
