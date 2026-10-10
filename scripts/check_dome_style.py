"""Execute production info-card EV/nature percentages, including empty spreads."""
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent


def main():
    source = (ROOT / 'src/hoenn_dome.c').read_text(encoding='utf-8')
    start = source.index('    // Calculate EV/nature points for the stat portion')
    block = source[start:source.index('    // Count the number of good/bad stats', start)]
    fixture = r'''
#define TRAINER_PLAYER 100
#define TRAINER_FRONTIER_BRAIN 101
#define FRONTIER_PARTY_SIZE 3
#define NUM_STATS 6
#define NUM_NATURE_STATS 5
#define STAT_HP 0
#define MAX_TOTAL_EVS 510
typedef short s16;
int sNatureStatTable[25][5];
int DOME_MONS[1][3];
int currentMask, currentNature, currentEvs[6];
int DomeBrainMonEvs(int i,int j) {return currentEvs[j];}
int DomePlayerMonEv(int i,int j) {return currentEvs[j];}
int DomeBrainMonNature(int i) {return currentNature;}
int DomePlayerMonNature(int i) {return currentNature;}
int HoennFrontier_MonEvSpread(int i) {return currentMask;}
int HoennFrontier_MonNature(int i) {return currentNature;}
int main(void) {
    for(int n=0;n<25;n++) if(n/5!=n%5) {
        sNatureStatTable[n][n/5]=1;
        sNatureStatTable[n][n%5]=-1;
    }
    for(int actor=0;actor<3;actor++)
    for(int mask=0;mask<256;mask++)
    for(int n=0;n<25;n++) {
        int trainerId=actor==0?TRAINER_PLAYER:actor==1?TRAINER_FRONTIER_BRAIN:1;
        int trainerTourneyId=0, i,j,k;
        unsigned char nature=0;
        s16 allocatedArray[18]={0};
        int expected[6],total=0,count=0;
        currentMask=mask; currentNature=n;
        for(i=0;i<6;i++) if(mask&(1<<i)) count++;
        for(i=0;i<6;i++) {
            currentEvs[i]=(mask&(1<<i))?(actor==2?(count?510/count:0):252):0;
            int value=currentEvs[i];
            if(i>0) {
                if(sNatureStatTable[n][i-1]>0) value=value*110/100;
                if(sNatureStatTable[n][i-1]<0) value=value*90/100;
            }
            expected[i]=value*3; total+=expected[i];
        }
BLOCK
        for(i=0;i<6;i++) {
            if(allocatedArray[i]!=(total?expected[i]*100/total:0)) return 1;
            if(i>0 && allocatedArray[i+12]!=(sNatureStatTable[n][i-1]<0?3:0)) return 2;
        }
    }
    return 0;
}
'''.replace('BLOCK', block)
    with tempfile.TemporaryDirectory() as directory:
        path = Path(directory)
        c, obj, exe = (path / ('style' + suffix) for suffix in ('.c', '.obj', '.exe'))
        c.write_text(fixture, encoding='utf-8')
        subprocess.run(['clang', '--target=x86_64-pc-windows-msvc', '-O2', '-fno-builtin',
                        '-fno-stack-protector', '-c', str(c), '-o', str(obj)], check=True)
        subprocess.run(['lld-link', '/entry:main', '/subsystem:console', '/nodefaultlib',
                        '/out:' + str(exe), str(obj)], check=True)
        subprocess.run([str(exe)], check=True)
    print('Info-card style passes 19200 player/brain/NPC mask and nature cases, including zero EVs.')


if __name__ == '__main__':
    main()
