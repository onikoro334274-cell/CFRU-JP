"""Execute production card sprite setup with all valid and failed sprite IDs."""
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent


def main():
    source = (ROOT / 'src/hoenn_dome.c').read_text(encoding='utf-8')
    start = source.index('static u8 DomeInfoCardSpriteId(')
    helper = source[start:source.index('static void DisplayTrainerInfoOnCard(', start)]
    start = source.index('    // Create trainer pic sprite', start)
    trainer = source[start:source.index('    // Initialize the text printer', start)]
    start = source.index('    // Draw left trainer sprite.')
    match = source[start:source.index('    // Print the win string', start)]
    fixture = r'''
typedef unsigned char u8;
typedef unsigned short u16;
#define MAX_SPRITES 64
#define SPRITE_NONE 255
#define FRONTIER_PARTY_SIZE 3
#define TRAINER_PLAYER 100
#define TRAINER_FRONTIER_BRAIN 101
#define MOVE_CARD 2
#define TRUE 1
#define sMonIconStill data[3]
struct Sprite {int invisible; struct {int priority,paletteNum;} oam; int data[4];} gSprites[256];
struct Card {u8 spriteIds[16];} card,*sInfoCard=&card;
unsigned result;
#define CreateTrainerPicSprite(...) result
#define CreateMonIcon(...) result
HELPER
void setup(int type,int actor,int alternate,int moving,int defeated) {
    int flags=moving?MOVE_CARD:0, arrId=alternate?8:0,i;
    int trainerId=actor,trainerIds[2]={actor,actor},lost[2]={defeated,defeated};
    if(type==0) {@TRAINER@} else {@MATCH@}
}
int main(void) {
    for(result=0;result<65536;result++) {
        if(DomeInfoCardSpriteId(result)!=(result<64?result:255)) return 1;
    }
    unsigned ids[]={0,63,64,255,65535};
    for(int id=0;id<5;id++) for(int type=0;type<2;type++)
    for(int actor=0;actor<3;actor++) for(int alternate=0;alternate<2;alternate++)
    for(int moving=0;moving<2;moving++) for(int defeated=0;defeated<2;defeated++) {
        result=ids[id];
        for(int n=0;n<256;n++) {
            gSprites[n].invisible=0; gSprites[n].oam.priority=7;
            gSprites[n].oam.paletteNum=0; gSprites[n].data[3]=0;
        }
        for(int n=0;n<16;n++) card.spriteIds[n]=255;
        setup(type,actor==0?100:actor==1?101:1,alternate,moving,defeated);
        for(int n=0;n<256;n++) {
            if(n!=result || result>=64) {
                if(gSprites[n].invisible || gSprites[n].oam.priority!=7
                   || gSprites[n].oam.paletteNum || gSprites[n].data[3]) return 2;
            }
        }
        int offset=alternate?8:0;
        for(int n=0;n<8;n++) {
            int used=type==1 || n==0 || (n>=2 && n<=4);
            int expected=used && result<64?result:255;
            if(card.spriteIds[offset+n]!=expected) return 3;
        }
        if(result<64 && (gSprites[result].oam.priority!=0
           || gSprites[result].invisible!=moving)) return 4;
        if(type==1 && result<64 && defeated
           && (gSprites[result].oam.paletteNum!=3 || !gSprites[result].data[3])) return 5;
    }
    return 0;
}
'''.replace('HELPER', helper).replace('@TRAINER@', trainer).replace('@MATCH@', match)
    with tempfile.TemporaryDirectory() as directory:
        path = Path(directory)
        c, obj, exe = (path / ('sprites' + suffix) for suffix in ('.c', '.obj', '.exe'))
        c.write_text(fixture, encoding='utf-8')
        subprocess.run(['clang', '--target=x86_64-pc-windows-msvc', '-O2', '-fno-builtin',
                        '-fno-stack-protector', '-c', str(c), '-o', str(obj)], check=True)
        subprocess.run(['lld-link', '/entry:main', '/subsystem:console', '/nodefaultlib',
                        '/out:' + str(exe), str(obj)], check=True)
        subprocess.run([str(exe)], check=True)
    print('Card sprite setup passes all 65536 ID normalizations and 480 creation scenarios; invalid sprites stay untouched.')


if __name__ == '__main__':
    main()
