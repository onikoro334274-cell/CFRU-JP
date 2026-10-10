"""Run production v2 pixel expansion with output canaries and damaged sizes."""
import argparse
import re
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--baseline', help='Git revision of the inflate source')
    args = parser.parse_args()
    source = (ROOT / 'src/fcomp.c').read_text(encoding='utf-8')
    if args.baseline:
        source = subprocess.run(['git', 'show', args.baseline + ':src/fcomp.c'], cwd=ROOT,
                                capture_output=True, encoding='utf-8', check=True).stdout
    source = source[source.index('static const u16 sLenBase'):source.index('\n/*\n * 2026-10-06')]
    # Native fixture uses the production core, without the ARM-only target attribute.
    source = re.sub(r'^#define FC_ARM .*$', '#define FC_ARM', source, flags=re.M)
    fixture = r'''
typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
typedef u8 bool8;
#define FALSE 0
#define TRUE 1
#define NULL ((void*)0)
#include "src/fcomp.h"
SOURCE
static u8 t1[17*17*96],t2[17*17*48];
static u16 litFast[1024],distFast[256];
static struct BwFTable table;
static u16 symbols[1];
int main(void) {
    for(int record=0;record<289;record++) for(int slot=0;slot<16;slot++) {
        t1[record*96+64+slot*2]=0;t1[record*96+65+slot*2]=1;
        t2[record*48+32+slot]=16;
    }
    for(int slot=0;slot<1024;slot++) litFast[slot]=256 | (1<<9);
    struct FCompSet set={NULL,0,&table,symbols,litFast,&table,symbols,distFast,t1,t2};
    int unique[7]={0,1,2,3,127,128,65535};
    u16 aligned[20];u8* data=(u8*)aligned;
    u8 output[128];
    for(int test=0;test<7;test++) for(int size=0;size<=96;size++) {
        for(int i=0;i<40;i++) data[i]=0;
        data[2]=unique[test]%256;data[3]=unique[test]/256;data[5]=1;data[6]=8;
        for(int i=0;i<128;i++) output[i]=0xA5;
        u32 result=FCompInflateArm(&set,data,output,size);
        int pixels=unique[test]*32;
        if(result!=(pixels==size)) return 1;
        int written=pixels>size ? 0 : pixels;
        for(int i=0;i<written;i++) if(output[i]!=0) return 2;
        for(int i=written;i<128;i++) if(output[i]!=0xA5) return 3;
    }
    /* Two one-bit pixel codes per byte, plus a one-bit end symbol. */
    for(int uniq=0;uniq<=3;uniq++) for(int words=0;words<=8;words++) {
        for(int i=0;i<40;i++) data[i]=0;
        data[2]=uniq;data[5]=1;data[6]=words;
        for(int i=0;i<128;i++) output[i]=0xA5;
        int size=uniq*32;
        int enough=words*32>=size*2+1;
        if(FCompInflateArm(&set,data,output,size)!=enough) return 4;
        if(enough) for(int i=0;i<size;i++) if(output[i]!=0) return 5;
        for(int i=size;i<128;i++) if(output[i]!=0xA5) return 6;
    }
    return 0;
}
'''.replace('SOURCE', source)
    with tempfile.TemporaryDirectory() as directory:
        path = Path(directory)
        c, obj, exe = (path / ('pixels' + suffix) for suffix in ('.c', '.obj', '.exe'))
        c.write_text(fixture, encoding='utf-8')
        subprocess.run(['clang', '--target=x86_64-pc-windows-msvc', '-O2', '-fno-builtin',
                        '-fno-stack-protector', '-I', str(ROOT), '-c', str(c), '-o', str(obj)], check=True)
        subprocess.run(['lld-link', '/entry:main', '/subsystem:console', '/nodefaultlib',
                        '/out:' + str(exe), str(obj)], check=True)
        subprocess.run([str(exe)], check=True)
    print('FComp v2: 679 output-bound cases and 36 declared-word-length cases pass; valid streams expand.')


if __name__ == '__main__':
    main()
