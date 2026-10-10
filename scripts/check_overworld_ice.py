"""Execute ice classification, coordinate routing and invalid-player guards."""
import subprocess
import tempfile
from pathlib import Path
from check_mega_shop_escalation import function

ROOT = Path(__file__).resolve().parent.parent


def main():
    source = (ROOT / 'src/overworld_speedup.c').read_text(encoding='utf-8')
    production = '\n'.join(function(source, signature) for signature in (
        'static bool8 IsIceBehavior(u32 behavior)', 'static bool8 PlayerOnIce(void)'))
    fixture = r'''
typedef unsigned char u8;typedef unsigned int u32;typedef u8 bool8;
#include "include/constants/metatile_behaviors.h"
#define TRUE 1
#define EVENT_OBJECTS_COUNT 16
struct EventObject {struct {int x,y;} currentCoords,previousCoords;};
static struct EventObject gEventObjects[16];
static struct {u8 eventObjectId;} avatar;
#define gPlayerAvatar (&avatar)
static u32 current,previous;static int queries,errors;
static u32 MapGridGetMetatileBehaviorAt(int x,int y) {
    int id=avatar.eventObjectId;
    if(id>=16) {errors++;return 0;}
    queries++;
    if(x==100+id && y==200+id) return current;
    if(x==-100-id && y==-200-id) return previous;
    errors++;return 0;
}
PRODUCTION
static int expected(u32 behavior) {
    return behavior==MB_ICE || behavior==MB_THIN_ICE || behavior==MB_CRACKED_ICE;
}
int main(void) {
    for(u32 behavior=0;behavior<65536;behavior++) if(IsIceBehavior(behavior)!=expected(behavior)) return 1;
    if(IsIceBehavior(0xFFFFFFFF)) return 2;
    const u32 choices[]={0,MB_ICE,MB_THIN_ICE,MB_CRACKED_ICE,255,65536};
    for(int i=0;i<16;i++) {
        gEventObjects[i].currentCoords.x=100+i;gEventObjects[i].currentCoords.y=200+i;
        gEventObjects[i].previousCoords.x=-100-i;gEventObjects[i].previousCoords.y=-200-i;
    }
    for(int id=0;id<256;id++) for(int first=0;first<6;first++) for(int last=0;last<6;last++) {
        avatar.eventObjectId=id;current=choices[first];previous=choices[last];queries=errors=0;
        int blocked=id>=16 || expected(current) || expected(previous);
        if(PlayerOnIce()!=blocked || errors) return 3;
        int wanted=id>=16 ? 0 : expected(current) ? 1 : 2;
        if(queries!=wanted) return 4;
    }
    return 0;
}
'''.replace('PRODUCTION', production)
    with tempfile.TemporaryDirectory() as directory:
        path = Path(directory)
        c, obj, exe = (path / ('overworld_ice' + suffix) for suffix in ('.c', '.obj', '.exe'))
        c.write_text(fixture, encoding='utf-8')
        subprocess.run(['clang', '--target=x86_64-pc-windows-msvc', '-O2', '-fno-builtin',
                        '-fno-stack-protector', '-I', str(ROOT), '-c', str(c), '-o', str(obj)], check=True)
        subprocess.run(['lld-link', '/entry:main', '/subsystem:console', '/nodefaultlib',
                        '/out:' + str(exe), str(obj)], check=True)
        subprocess.run([str(exe)], check=True)
    print('Field ice: 65537 behavior classifications and 9216 player/coordinate/ice cases passed.')


if __name__ == '__main__':
    main()
