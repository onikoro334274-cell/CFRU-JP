"""Execute production tile-map expansion with a controlled inflate boundary."""
import argparse
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--baseline')
    args = parser.parse_args()
    source = (ROOT / 'src/fcomp_dpe.c').read_text(encoding='utf-8')
    if args.baseline:
        source = subprocess.run(['git', 'show', args.baseline + ':src/fcomp_dpe.c'],
                                cwd=ROOT, capture_output=True, encoding='utf-8', check=True).stdout
    function = source[source.index('static bool8 DpeFExpand('):source.index('//振り分け役から呼ばれる')]
    fixture = r'''
typedef unsigned char u8;typedef unsigned short u16;typedef unsigned int u32;typedef u8 bool8;
#define TRUE 1
#define FALSE 0
#define DPEF_MAX_TILES 128
#include "src/fcomp.h"
static const struct BwFTable gDpeFLitTable,gDpeFDistTable;
static const u16 gDpeFLitSymbols[1],gDpeFDistSymbols[1],gDpeFLitFast[1],gDpeFDistFast[1];
static const u8 gDpeFDict[1],gDpeFPixT1[1],gDpeFPixT2[1];
static const u32 gDpeFDictSize=0;
static u32 tiles,unique;static u8 map[128];
static u8 scratch[4352];
bool8 FCompInflate(const struct FCompSet* set,const u8* data,u8* out,u32 size) {
    if(size!=unique*32+tiles) return FALSE;
    for(u32 i=0;i<unique;i++) for(u32 b=0;b<32;b++) out[i*32+b]=(u8)(i*7+b);
    for(u32 i=0;i<tiles;i++) out[unique*32+i]=map[i];
    for(u32 i=0;i<size;i++) scratch[i]=out[i];
    return TRUE;
}
FUNCTION
static u32 aligned[1100];
int main(void) {
    u8 data[1]={0};
    for(int offset=0;offset<2;offset++) {
        u8* out=(u8*)aligned+offset;
        for(tiles=2;tiles<=128;tiles++) for(unique=1;unique<tiles;unique++) {
            if(unique*32+tiles>tiles*32) continue; /* DpeFTryDecode's input constraint */
            for(u32 i=0;i<tiles;i++) map[i]=i<unique ? i : (i%unique);
            for(int i=0;i<4352;i++) out[i]=0xA5;
            if(!DpeFExpand(data,out,tiles,unique)) return 1;
            for(u32 i=0;i<tiles;i++) for(u32 b=0;b<32;b++)
                if(out[i*32+b]!=(u8)(map[i]*7+b)) return 2;
            for(u32 i=tiles*32;i<4352;i++) if(out[i]!=0xA5) return 3;
        }
        tiles=8;unique=2;
        for(int i=0;i<8;i++) map[i]=i ? 1 : 0;
        map[7]=2; /* Within tile index 7, but outside the two decoded tiles. */
        for(int i=0;i<4352;i++) out[i]=0xA5;
        if(DpeFExpand(data,out,tiles,unique)) return 7;
        for(int i=0;i<72;i++) if(out[i]!=scratch[i]) return 8;
        for(int slot=0;slot<8;slot++) for(int value=0;value<256;value++) {
            for(int i=0;i<8;i++) map[i]=i ? 1 : 0;
            map[slot]=value;
            for(int i=0;i<4352;i++) out[i]=0xA5;
            int valid=value<2 && value<=slot;
            if(DpeFExpand(data,out,tiles,unique)!=valid) return 4;
            if(!valid) for(int i=0;i<72;i++) if(out[i]!=scratch[i]) return 5;
            for(u32 i=tiles*32;i<4352;i++) if(out[i]!=0xA5) return 6;
        }
    }
    return 0;
}
'''.replace('FUNCTION', function)
    with tempfile.TemporaryDirectory() as directory:
        path = Path(directory)
        c, obj, exe = (path / ('tilemap' + suffix) for suffix in ('.c', '.obj', '.exe'))
        c.write_text(fixture, encoding='utf-8')
        subprocess.run(['clang', '--target=x86_64-pc-windows-msvc', '-O2', '-fno-builtin',
                        '-fno-stack-protector', '-Wno-pointer-to-int-cast', '-I', str(ROOT),
                        '-c', str(c), '-o', str(obj)], check=True)
        subprocess.run(['lld-link', '/entry:main', '/subsystem:console', '/nodefaultlib',
                        '/out:' + str(exe), str(obj)], check=True)
        subprocess.run([str(exe)], check=True)
    cases = 2 * sum(unique * 32 + tiles <= tiles * 32
                    for tiles in range(2, 129) for unique in range(1, tiles))
    print('DPE tile map: %d valid expansions, 4096 map-byte cases and two out-of-range reproductions pass.' % cases)


if __name__ == '__main__':
    main()
