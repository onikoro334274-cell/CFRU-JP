"""Execute every legal candidate-selection transition before confirmation."""
import subprocess
import tempfile
from pathlib import Path
from check_factory_party_copy import function

ROOT = Path(__file__).resolve().parent.parent


def main():
    source = (ROOT / 'src/hoenn_factory_screen.c').read_text(encoding='utf-8')
    production = function(source, 'static void Select_HandleMonSelectionChange(void)')
    fixture = r'''
typedef unsigned char u8;
#define FRONTIER_PARTY_SIZE 3
#define SELECTABLE_MONS_COUNT 6
#define PALTAG_BALL_GRAY 1
#define PALTAG_BALL_SELECTED 2
static struct {struct {u8 selectedId,ballSpriteId;} mons[6];u8 cursorPos,selectingMonsState;} screen,*sFactorySelectScreen=&screen;
static struct {struct {u8 paletteNum;} oam;} gSprites[6];
static u8 IndexOfSpritePaletteTag(u8 tag) {return tag+10;}
FUNCTION
int main(void) {
    for(int a=-1;a<6;a++) for(int b=-1;b<6;b++) {
        if((a<0 && b>=0) || (a>=0 && b==a)) continue;
        int count=(a>=0)+(b>=0),order[3]={a,b,-1};
        for(int cursor=0;cursor<6;cursor++) {
            for(int i=0;i<6;i++){screen.mons[i].selectedId=0;screen.mons[i].ballSpriteId=i;gSprites[i].oam.paletteNum=77;}
            for(int i=0;i<count;i++) screen.mons[order[i]].selectedId=i+1;
            screen.selectingMonsState=count+1;screen.cursorPos=cursor;
            int expected[6]={0},next=0,removed=0;
            for(int i=0;i<count;i++) {
                if(order[i]==cursor) removed=1;
                else expected[order[i]]=++next;
            }
            if(!removed) expected[cursor]=++next;
            Select_HandleMonSelectionChange();
            if(screen.selectingMonsState!=next+1) return 1;
            for(int i=0;i<6;i++) {
                if(screen.mons[i].selectedId!=expected[i]) return 2;
                int palette=i==cursor ? (removed ? 11 : 12) : 77;
                if(gSprites[i].oam.paletteNum!=palette) return 3;
            }
        }
    }
    /* Declining confirmation deselects the just-chosen third candidate. */
    for(int a=0;a<6;a++) for(int b=0;b<6;b++) for(int c=0;c<6;c++) {
        if(a==b || a==c || b==c) continue;
        for(int i=0;i<6;i++){screen.mons[i].selectedId=0;screen.mons[i].ballSpriteId=i;}
        screen.mons[a].selectedId=1;screen.mons[b].selectedId=2;screen.mons[c].selectedId=3;
        screen.selectingMonsState=4;screen.cursorPos=c;
        Select_HandleMonSelectionChange();
        if(screen.selectingMonsState!=3) return 4;
        for(int i=0;i<6;i++) if(screen.mons[i].selectedId!=(i==a ? 1 : i==b ? 2 : 0)) return 5;
    }
    return 0;
}
'''.replace('FUNCTION', production)
    with tempfile.TemporaryDirectory() as directory:
        path = Path(directory)
        c, obj, exe = (path / ('selection' + suffix) for suffix in ('.c', '.obj', '.exe'))
        c.write_text(fixture, encoding='utf-8')
        subprocess.run(['clang', '--target=x86_64-pc-windows-msvc', '-O2', '-fno-builtin',
                        '-fno-stack-protector', '-c', str(c), '-o', str(obj)], check=True)
        subprocess.run(['lld-link', '/entry:main', '/subsystem:console', '/nodefaultlib',
                        '/out:' + str(exe), str(obj)], check=True)
        subprocess.run([str(exe)], check=True)
    print('Factory selection: 222 pre-confirmation transitions and 120 third-choice cancellations preserve selection order.')


if __name__ == '__main__':
    main()
