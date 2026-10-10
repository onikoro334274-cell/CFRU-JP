"""Execute the production scroll-arrow initializer for all byte sprite IDs."""
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent


def main():
    source = (ROOT / 'src/hoenn_dome.c').read_text(encoding='utf-8')
    start = source.index('static void CreateInfoCardScrollArrow(')
    function = source[start:source.index('// For showing the opponent info card', start)]
    fixture = r'''
typedef unsigned char u8;
typedef short s16;
typedef u8 bool8;
#define MAX_SPRITES 64
struct SpriteTemplate {int unused;};
struct Sprite {int data[2]; int invisible;} gSprites[256];
int result,called,seenAnim,seenX,seenY;
u8 CreateSprite(const struct SpriteTemplate *t,s16 x,s16 y,u8 priority) {
    seenX=x; seenY=y; return result;
}
void StartSpriteAnim(struct Sprite *s,u8 anim) {called++;seenAnim=anim;}
FUNCTION
int main(void) {
    struct SpriteTemplate t={0};
    for(result=0;result<256;result++) for(int anim=0;anim<2;anim++)
    for(int direction=0;direction<2;direction++) for(int invisible=0;invisible<2;invisible++) {
        called=0;seenAnim=-1;
        for(int n=0;n<256;n++) {
            gSprites[n].data[0]=-1;gSprites[n].data[1]=-1;gSprites[n].invisible=-1;
        }
        CreateInfoCardScrollArrow(&t,234,156,anim,15,direction,invisible);
        if(seenX!=234 || seenY!=156 || called!=(result<64)) return 1;
        for(int n=0;n<256;n++) {
            if(n==result && result<64) {
                if(gSprites[n].data[0]!=15 || gSprites[n].data[1]!=direction
                   || gSprites[n].invisible!=invisible || seenAnim!=anim) return 2;
            } else if(gSprites[n].data[0]!=-1 || gSprites[n].data[1]!=-1
                      || gSprites[n].invisible!=-1) return 3;
        }
    }
    return 0;
}
'''.replace('FUNCTION', function)
    with tempfile.TemporaryDirectory() as directory:
        path = Path(directory)
        c, obj, exe = (path / ('arrows' + suffix) for suffix in ('.c', '.obj', '.exe'))
        c.write_text(fixture, encoding='utf-8')
        subprocess.run(['clang', '--target=x86_64-pc-windows-msvc', '-O2', '-fno-builtin',
                        '-fno-stack-protector', '-c', str(c), '-o', str(obj)], check=True)
        subprocess.run(['lld-link', '/entry:main', '/subsystem:console', '/nodefaultlib',
                        '/out:' + str(exe), str(obj)], check=True)
        subprocess.run([str(exe)], check=True)
    print('Scroll arrows pass 2048 sprite-ID/animation/direction/visibility cases; failed creations cause no sprite writes.')


if __name__ == '__main__':
    main()
