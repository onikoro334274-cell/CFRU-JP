"""Execute swap exit task: party mutation, script result and child cleanup."""
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent


def main():
    source = (ROOT / 'src/hoenn_factory_screen.c').read_text(encoding='utf-8')
    start = source.index('static void Swap_Task_Exit(u8 taskId)\n{')
    brace = source.index('{', start)
    depth, end = 1, brace + 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    function = source[start:end]
    fixture = r'''
typedef unsigned char u8;
#define TRUE 1
#define FALSE 0
#define NULL ((void*)0)
#define PALETTES_ALL 0
#define RGB_BLACK 0
static struct {u8 monPicAnimating,fadeSpeciesNameTaskId,monSwapped,enemyMonId,cursorPos;} screen,*sFactorySwapScreen;
static struct {void *tiles,*picTiles,*tilemap,*picTilemap;} root,*sRoot;
#define sSwapMenuTilesetBuffer (sRoot->tiles)
#define sSwapMonPicBgTilesetBuffer (sRoot->picTiles)
#define sSwapMenuTilemapBuffer (sRoot->tilemap)
#define sSwapMonPicBgTilemapBuffer (sRoot->picTilemap)
static struct {int tState;} gTasks[16];
static int gSpecialVar_LastResult,busy,freed,leaked,copied,sprites,windows,callback,fade,destroyed;
static int allocations[4];
static void Free(void* p) {
    if(p==&root && (root.tiles || root.picTiles || root.tilemap || root.picTilemap)) leaked=1;
    freed++;
}
#define FREE_AND_SET_NULL(p) do {Free(p);(p)=NULL;} while(0)
static void BeginNormalPaletteFade(int a,int b,int c,int d,int e){fade++;}
static int UpdatePaletteFade(void){return busy;}
static void CopySwappedMonData(void){copied++;}
static void DestroyTask(u8 id){destroyed++;}
static void Swap_DestroyAllSprites(void){sprites++;}
static void FreeAllWindowBuffers(void){windows++;}
static void CB2_ReturnToFieldContinueScript(void){}
static void SetMainCallback2(void(*fn)(void)){callback=fn==CB2_ReturnToFieldContinueScript;}
FUNCTION
int main(void) {
    for(int animating=0;animating<2;animating++) for(int state=0;state<4;state++)
    for(busy=0;busy<2;busy++) for(int swapped=0;swapped<2;swapped++) for(int cursor=0;cursor<3;cursor++) {
        root.tiles=&allocations[0];root.picTiles=&allocations[1];
        root.tilemap=&allocations[2];root.picTilemap=&allocations[3];sRoot=&root;
        sFactorySwapScreen=&screen;screen.monPicAnimating=animating;screen.fadeSpeciesNameTaskId=3;
        screen.monSwapped=swapped;screen.cursorPos=cursor;screen.enemyMonId=77;
        gTasks[7].tState=state;gSpecialVar_LastResult=77;
        freed=leaked=copied=sprites=windows=callback=fade=destroyed=0;
        Swap_Task_Exit(7);
        int exiting=!animating && state==3 && !busy;
        if(leaked || freed!=(exiting ? 6 : 0)) return 1;
        if(sprites!=exiting || windows!=exiting || callback!=exiting || destroyed!=exiting*2) return 2;
        int copy=!animating && state==1 && swapped;
        if(copied!=copy || screen.enemyMonId!=(copy ? cursor : 77)) return 3;
        if(gSpecialVar_LastResult!=(!animating && state==0 ? !swapped : 77)) return 4;
        if(fade!=(!animating && state==2)) return 5;
        int next=animating || state==3 ? state : state==0 ? (swapped ? 1 : 2) : state+1;
        if(gTasks[7].tState!=next) return 6;
        if(exiting && (sRoot || sFactorySwapScreen)) return 7;
    }
    return 0;
}
'''.replace('FUNCTION', function)
    with tempfile.TemporaryDirectory() as directory:
        path = Path(directory)
        c, obj, exe = (path / ('swap_exit' + suffix) for suffix in ('.c', '.obj', '.exe'))
        c.write_text(fixture, encoding='utf-8')
        subprocess.run(['clang', '--target=x86_64-pc-windows-msvc', '-O2', '-fno-builtin',
                        '-fno-stack-protector', '-c', str(c), '-o', str(obj)], check=True)
        subprocess.run(['lld-link', '/entry:main', '/subsystem:console', '/nodefaultlib',
                        '/out:' + str(exe), str(obj)], check=True)
        subprocess.run([str(exe)], check=True)
    print('Factory swap exit: 96 animation/state/fade/selection/cursor cases pass without child leaks.')


if __name__ == '__main__':
    main()
