"""Execute production mixed-region roaming and repair of invalid locations."""
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent


def main():
    source = (ROOT / 'src/roamer.c').read_text(encoding='utf-8')
    tables = source[source.index('#define ROAMING_MAP_BANK'):source.index('//This file\'s functions:')]
    moves = source[source.index('void RoamersMoveToOtherLocationSet(void)'):source.index('static bool8 IsRoamerAt(u8 mapGroup, u8 mapNum, u8 id)\n{')]
    fixture = r'''
typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
typedef u8 bool8;
#define TRUE 1
#define SPECIES_NONE 0
#define ARRAY_COUNT(a) (sizeof(a)/sizeof((a)[0]))
#define MAX_NUM_ROAMERS 4
#define MAP_GRP 0
HEADERS
struct Roamer {u16 species;u8 location[2];u8 locationHistory[3][2];} gRoamers[4];
unsigned state=42,calls;
u16 Random(void) {calls++;state=state*1664525u+1013904223u;return state>>16;}
TABLES
#undef MAP_NUM
#define MAP_NUM 1
MOVES
int main(void) {
    for(int i=0;i<4;i++) {
        u32 count;
        int group=i%2?HOENN_ROAMING_MAP_BANK:ROAMING_MAP_BANK;
        RoamerLocationSet *table=GetRoamerLocationTable(group,&count);
        gRoamers[i].species=1;gRoamers[i].location[0]=group;
        gRoamers[i].location[1]=i==0?255:table[i%count][0];
        for(u32 r=0;r<count;r++) {
            int candidates=0;
            for(int k=0;k<NUM_MAPS_IN_SET;k++) if(table[r][k]!=255) candidates++;
            if(candidates<2 || table[r][0]==255) return 1;
        }
    }
    for(int turn=0;turn<20000;turn++) {
        for(int i=0;i<4;i++) {
            gRoamers[i].locationHistory[2][0]=gRoamers[i].location[0];
            gRoamers[i].locationHistory[2][1]=gRoamers[i].location[1];
        }
        RoamersMove();
        for(int i=0;i<4;i++) {
            u32 count;
            int group=i%2?HOENN_ROAMING_MAP_BANK:ROAMING_MAP_BANK;
            RoamerLocationSet *table=GetRoamerLocationTable(group,&count);
            if(gRoamers[i].location[0]!=group) return 2;
            int found=0;
            for(u32 r=0;r<count;r++) if(table[r][0]==gRoamers[i].location[1]) found=1;
            if(!found) return 3;
        }
    }
    if(calls>1000000) return 4;
    return 0;
}
'''
    headers = '\n'.join('#include "' + (ROOT / name).as_posix() + '"' for name in
                        ('include/constants/maps.h', 'src/Tables/hoenn_roamer_locations.h'))
    fixture = fixture.replace('HEADERS', headers).replace('TABLES', tables).replace('MOVES', moves)
    with tempfile.TemporaryDirectory() as directory:
        path = Path(directory)
        c, obj, exe = (path / ('roaming' + suffix) for suffix in ('.c', '.obj', '.exe'))
        c.write_text(fixture, encoding='utf-8')
        subprocess.run(['clang', '--target=x86_64-pc-windows-msvc', '-O2', '-fno-builtin',
                        '-fno-stack-protector', '-c', str(c), '-o', str(obj)], check=True)
        subprocess.run(['lld-link', '/entry:main', '/subsystem:console', '/nodefaultlib',
                        '/out:' + str(exe), str(obj)], check=True)
        subprocess.run([str(exe)], check=True, timeout=10)
    print('Production roaming passes 80000 mixed-region moves, valid route membership, invalid-location repair and nonempty candidate rows.')


if __name__ == '__main__':
    main()
