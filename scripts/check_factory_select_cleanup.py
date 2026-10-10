"""Execute the select exit task and detect leaked child graphics allocations."""
import argparse
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--baseline')
    args = parser.parse_args()
    source = (ROOT / 'src/hoenn_factory_screen.c').read_text(encoding='utf-8')
    if args.baseline:
        source = subprocess.run(['git', 'show', args.baseline + ':src/hoenn_factory_screen.c'],
                                cwd=ROOT, capture_output=True, encoding='utf-8', check=True).stdout
    start = source.index('static void Select_Task_Exit(u8 taskId)\n{')
    function = source[start:source.index('// Handles the Yes/No prompt', start)]
    fixture = r'''
typedef unsigned char u8;
#define TRUE 1
#define NULL ((void*)0)
#define PALETTES_ALL 0
#define RGB_BLACK 0
static struct {u8 monPicAnimating,fadeSpeciesNameTaskId;} screen,*sFactorySelectScreen;
static struct {void *tiles,*picTiles,*tilemap,*picTilemap;} root,*sRoot;
#define sSelectMenuTilesetBuffer (sRoot->tiles)
#define sSelectMonPicBgTilesetBuffer (sRoot->picTiles)
#define sSelectMenuTilemapBuffer (sRoot->tilemap)
#define sSelectMonPicBgTilemapBuffer (sRoot->picTilemap)
static void* sFactorySelectMons;
static struct {int tState;} gTasks[16];
static int busy,freed,leaked,copied,sprites,windows,callback,fade,destroyed;
static int allocations[5];
static void Free(void* p) {
    if(p==&root && (root.tiles || root.picTiles || root.tilemap || root.picTilemap)) leaked=1;
    freed++;
}
#define FREE_AND_SET_NULL(p) do {Free(p);(p)=NULL;} while(0)
#define TRY_FREE_AND_SET_NULL(p) do {if(p) FREE_AND_SET_NULL(p);} while(0)
static void BeginNormalPaletteFade(int a,int b,int c,int d,int e){fade++;}
static int UpdatePaletteFade(void){return busy;}
static void Select_CopyMonsToPlayerParty(void){copied++;}
static void DestroyTask(u8 id){destroyed++;}
static void Select_DestroyAllSprites(void){sprites++;}
static void FreeAllWindowBuffers(void){windows++;}
static void CB2_ReturnToFieldContinueScript(void){}
static void SetMainCallback2(void(*fn)(void)){callback=fn==CB2_ReturnToFieldContinueScript;}
FUNCTION
int main(void) {
    for(int animating=0;animating<2;animating++) for(int state=0;state<2;state++)
    for(busy=0;busy<2;busy++) for(int mons=0;mons<2;mons++) {
        root.tiles=&allocations[0];root.picTiles=&allocations[1];
        root.tilemap=&allocations[2];root.picTilemap=&allocations[3];sRoot=&root;
        sFactorySelectScreen=&screen;screen.monPicAnimating=animating;screen.fadeSpeciesNameTaskId=3;
        sFactorySelectMons=mons ? &allocations[4] : NULL;
        gTasks[7].tState=state;freed=leaked=copied=sprites=windows=callback=fade=destroyed=0;
        Select_Task_Exit(7);
        int exiting=!animating && state==1 && !busy;
        if(leaked) return 1;
        if(freed!=(exiting ? 6+mons : 0)) return 2;
        if(copied!=exiting || sprites!=exiting || windows!=exiting || callback!=exiting || destroyed!=exiting*2) return 3;
        if(fade!=(!animating && state==0)) return 4;
        if(exiting && (sRoot || sFactorySelectScreen || sFactorySelectMons)) return 5;
    }
    return 0;
}
'''.replace('FUNCTION', function)
    with tempfile.TemporaryDirectory() as directory:
        path = Path(directory)
        c, obj, exe = (path / ('cleanup' + suffix) for suffix in ('.c', '.obj', '.exe'))
        c.write_text(fixture, encoding='utf-8')
        subprocess.run(['clang', '--target=x86_64-pc-windows-msvc', '-O2', '-fno-builtin',
                        '-fno-stack-protector', '-c', str(c), '-o', str(obj)], check=True)
        subprocess.run(['lld-link', '/entry:main', '/subsystem:console', '/nodefaultlib',
                        '/out:' + str(exe), str(obj)], check=True)
        subprocess.run([str(exe)], check=True)
    print('Factory select exit: all 16 animation/state/fade/optional-party cases release children before root.')


if __name__ == '__main__':
    main()
