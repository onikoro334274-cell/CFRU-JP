"""Execute production F-stream recognition, header guards and VRAM failures."""
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
        source = subprocess.run(['git', 'show', args.baseline + ':src/fcomp_dpe.c'], cwd=ROOT,
                                capture_output=True, encoding='utf-8', check=True).stdout
    function = source[source.index('u32 DpeFTryDecode('):]
    fixture = r'''
typedef unsigned char u8;typedef unsigned short u16;typedef unsigned int u32;
typedef u8 bool8;
#define NULL ((void*)0)
#define DPEF_MAX_TILES 128
#include "src/fcomp.h"
static u32 allocs,frees,expands,failAlloc,failExpand;
static u32 tempWords[1024],dstWords[1040];
static void* Malloc(u32 size) {allocs++;return failAlloc ? NULL : tempWords;}
static void Free(void* ptr) {frees++;}
static bool8 DpeFExpand(const u8* data,u8* out,u32 tiles,u32 unique) {
    expands++;
    if(failExpand) return 0;
    for(u32 i=0;i<tiles*32;i++) out[i]=(u8)(i+7);
    return 1;
}
FUNCTION
static int check(u32 tiles,u32 unique,u32 width,u32 version,u32 raw,u32 vram,u32 marker) {
    u8 src[12]={0};u8* dst=(u8*)dstWords;
    src[0]=marker;src[1]=raw;src[2]=raw>>8;src[3]=raw>>16;
    src[4]=tiles;src[5]=tiles>>8;src[6]=unique;src[7]=unique>>8;
    src[8]=width;src[9]=version;
    for(u32 i=0;i<4160;i++) dst[i]=0xA5;
    allocs=frees=expands=0;
    u32 result=DpeFTryDecode(src,dst,vram);
    if(result!=(marker==0x46)) return 1;
    int valid=marker==0x46 && width==1 && version<=1 && tiles*32==raw && tiles<=128
        && unique>0 && unique<=tiles && unique*32+tiles<=raw;
    int expand=valid && (!vram || !failAlloc);
    if(expands!=expand || allocs!=(valid && vram) || frees!=(valid && vram && !failAlloc)) return 2;
    int written=expand && !failExpand;
    for(u32 i=0;i<4160;i++) {
        u8 expected=written && i<raw ? (u8)(i+7) : 0xA5;
        if(dst[i]!=expected) return 3;
    }
    return 0;
}
int main(void) {
    /* Header boundaries, both storage paths and failure combinations. */
    u32 tileCases[9]={0,1,2,31,32,33,127,128,129};
    u32 uniqueCases[9]={0,1,2,30,31,32,126,127,128};
    for(u32 t=0;t<9;t++) for(u32 u=0;u<9;u++) for(u32 width=0;width<3;width++)
    for(u32 version=0;version<4;version++) for(u32 rawCase=0;rawCase<3;rawCase++)
    for(u32 vram=0;vram<2;vram++) for(failAlloc=0;failAlloc<2;failAlloc++)
    for(failExpand=0;failExpand<2;failExpand++) {
        u32 tiles=tileCases[t],raw=tiles*32;
        if(rawCase==1) raw++; else if(rawCase==2) raw=raw ? raw-1 : 1;
        int result=check(tiles,uniqueCases[u],width,version,raw,vram,0x46);
        if(result) return result;
    }
    failAlloc=failExpand=0;
    for(u32 marker=0;marker<256;marker++) for(u32 vram=0;vram<2;vram++) {
        int result=check(8,2,1,0,256,vram,marker);
        if(result) return result;
    }
    return 0;
}
'''.replace('FUNCTION', function)
    with tempfile.TemporaryDirectory() as directory:
        path = Path(directory)
        c, obj, exe = (path / ('dispatch' + suffix) for suffix in ('.c', '.obj', '.exe'))
        c.write_text(fixture, encoding='utf-8')
        subprocess.run(['clang', '--target=x86_64-pc-windows-msvc', '-O2', '-fno-builtin',
                        '-fno-stack-protector', '-I', str(ROOT), '-c', str(c), '-o', str(obj)], check=True)
        subprocess.run(['lld-link', '/entry:main', '/subsystem:console', '/nodefaultlib',
                        '/out:' + str(exe), str(obj)], check=True)
        subprocess.run([str(exe)], check=True)
    print('FComp dispatch: 23328 header/failure combinations and 512 marker/path cases pass.')


if __name__ == '__main__':
    main()
