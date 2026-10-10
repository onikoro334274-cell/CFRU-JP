"""Check recovered Huffman table layout and all stream-header field values."""
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent


def main():
    fixture = r'''
typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
typedef u8 bool8;
#include "src/fcomp.h"
_Static_assert(sizeof(struct BwFTable)==96,"serialized Huffman table size");
_Static_assert(__builtin_offsetof(struct BwFTable,first)==0,"first offset");
_Static_assert(__builtin_offsetof(struct BwFTable,count)==32,"count offset");
_Static_assert(__builtin_offsetof(struct BwFTable,index)==64,"index offset");
int main(void) {
    u8 storage[10];
    for(int alignment=0;alignment<2;alignment++) {
        u8* data=storage+alignment;
        for(u32 value=0;value<65536;value++) {
            for(int i=0;i<8;i++) data[i]=0xA5;
            data[0]=value%256;data[1]=value/256;
            if(FCOMP_NUM_TILES(data)!=value) return 1;
            data[2]=value%256;data[3]=value/256;
            if(FCOMP_NUM_UNIQ(data)!=value) return 2;
            data[6]=value%256;data[7]=value/256;
            if(FCOMP_NUM_WORDS(data)!=value) return 3;
        }
        for(int value=0;value<256;value++) {
            data[4]=value;data[5]=255-value;
            if(FCOMP_WIDTH(data)!=value || FCOMP_VERSION(data)!=255-value) return 4;
        }
    }
    return 0;
}
'''
    with tempfile.TemporaryDirectory() as directory:
        path = Path(directory)
        c, obj, exe = (path / ('header' + suffix) for suffix in ('.c', '.obj', '.exe'))
        c.write_text(fixture, encoding='utf-8')
        subprocess.run(['clang', '--target=x86_64-pc-windows-msvc', '-O2', '-fno-builtin',
                        '-fno-stack-protector', '-I', str(ROOT), '-c', str(c), '-o', str(obj)], check=True)
        subprocess.run(['lld-link', '/entry:main', '/subsystem:console', '/nodefaultlib',
                        '/out:' + str(exe), str(obj)], check=True)
        subprocess.run([str(exe)], check=True)
    print('FComp header: 96-byte table layout, 393216 u16 field cases and 512 width/version cases pass.')


if __name__ == '__main__':
    main()
