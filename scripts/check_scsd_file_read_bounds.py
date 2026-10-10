"""Execute file-sector reads with a 64-bit range oracle and controlled SD I/O."""
import argparse
import subprocess
import tempfile
from pathlib import Path
from check_factory_party_copy import function

ROOT = Path(__file__).resolve().parent.parent


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--baseline')
    parser.add_argument('--overflow-only', action='store_true')
    args = parser.parse_args()
    source = (ROOT / 'src/hack_scsd.c').read_text(encoding='utf-8')
    if args.baseline:
        source = subprocess.run(['git', 'show', args.baseline + ':src/hack_scsd.c'], cwd=ROOT,
                                capture_output=True, encoding='utf-8', check=True).stdout
    production = function(source, 'static bool8 HackScsd_ReadOpened(struct ScsdFs *fs, u32 sector, u32 n, u8 *out)')
    fixture = r'''
#include <stddef.h>
typedef unsigned char u8;typedef unsigned int u32;typedef u8 bool8;
#define TRUE 1
#define FALSE 0
struct ScsdFs {u32 fileSize;u8 spcShift;bool8 fileNoChain;};
static u8 output[4098];static u32 cursor,remaining,start;static int maps,reads,errors,failMap,failRead;
static u32 FileLba(struct ScsdFs *fs,u32 sector){
    maps++;if(sector!=cursor) errors++;
    if(maps==failMap) return 0;
    return 1000+sector+(fs->fileNoChain?0:(sector>>fs->spcShift)*7);
}
static bool8 HackScsd_Read(struct ScsdFs *fs,u32 lba,u32 n,u8 *out){
    reads++;u32 expected=1000+cursor+(fs->fileNoChain?0:(cursor>>fs->spcShift)*7);
    if(lba!=expected||n>remaining||n>8||out!=output+1+(cursor-start)*512){errors++;return FALSE;}
    if(!fs->fileNoChain && n>(1u<<fs->spcShift)-(cursor% (1u<<fs->spcShift))) errors++;
    if(reads==failRead) return FALSE;
    for(u32 i=0;i<n*512;i++) out[i]=(u8)((cursor+i/512)*37+i%512);
    cursor+=n;remaining-=n;return TRUE;
}
PRODUCTION
static int check(u32 bytes,u32 sector,u32 n,unsigned shift,int contiguous,int nullOut){
    struct ScsdFs fs={bytes,shift,contiguous};cursor=start=sector;remaining=n;maps=reads=errors=failMap=failRead=0;
    for(unsigned i=0;i<4098;i++) output[i]=0xcc;
    int valid=(((unsigned long long)sector+n)*512<=bytes)&&shift<32&&(!n||!nullOut);
    int result=HackScsd_ReadOpened(&fs,sector,n,nullOut?NULL:output+1);
    if(result!=valid||errors) return 1;
    if(!valid&&(maps||reads)) return 2;
    if(valid&&remaining) return 3;
    if(output[0]!=0xcc||output[4097]!=0xcc) return 4;
    for(unsigned i=0;i<4096;i++){
        u8 expected=valid&&i<(unsigned long long)n*512?(u8)((sector+i/512)*37+i%512):0xcc;
        if(output[i+1]!=expected) return 5;
    }
    return 0;
}
int main(void){
    const u32 sizes[]={0,1,511,512,513,1023,1024,1025,2047,2048,2049,4095,4096,4097,0xffffffff};
    for(unsigned f=0;f<15;f++) for(unsigned sector=0;sector<10;sector++) for(unsigned n=0;n<9;n++)
    for(unsigned shift=0;shift<5;shift++) for(int contiguous=0;contiguous<2;contiguous++)
    for(int nullOut=0;nullOut<2;nullOut++) if(check(sizes[f],sector,n,shift,contiguous,nullOut)) return 1;
    const u32 extreme[]={0x7fffff,0x800000,0x7fffffff,0x80000000,0xfffffffe,0xffffffff};
    for(unsigned i=0;i<6;i++) for(unsigned f=0;f<15;f++){
        if(check(sizes[f],extreme[i],0,0,1,0)||check(sizes[f],extreme[i],1,0,1,0)) return 2;
        /* Accepted multi-gigabyte reads require a correspondingly large caller
         * buffer; only rejected huge counts belong in this bounded fixture. */
        if((unsigned long long)extreme[i]*512>sizes[f] && check(sizes[f],0,extreme[i],0,1,0)) return 3;
    }
    for(unsigned shift=5;shift<256;shift++) if(check(512,0,1,shift,0,0)) return 4;
    struct ScsdFs fs={4096,0,FALSE};
    for(int failure=0;failure<2;failure++){
        cursor=start=0;remaining=3;maps=reads=errors=0;failMap=failure?0:2;failRead=failure?2:0;
        for(unsigned i=0;i<4098;i++) output[i]=0xcc;
        if(HackScsd_ReadOpened(&fs,0,3,output+1)||errors||cursor!=1||remaining!=2) return 5;
        for(unsigned i=512;i<4096;i++) if(output[i+1]!=0xcc) return 6;
    }
    return 0;
}
'''.replace('PRODUCTION', production)
    if args.overflow_only:
        fixture = fixture.replace('int main(void){', 'int unused_cases(void){')
        fixture += '\nint main(void){return check(512,0x800000,1,0,1,0)||check(512,0xffffffff,1,0,1,0)||check(512,0,0x800000,0,1,0);}\n'
    with tempfile.TemporaryDirectory() as directory:
        path = Path(directory)
        c, obj, exe = (path / ('file_read' + suffix) for suffix in ('.c', '.obj', '.exe'))
        c.write_text(fixture, encoding='utf-8')
        subprocess.run(['clang', '--target=x86_64-pc-windows-msvc', '-O2', '-fno-builtin',
                        '-fno-stack-protector', '-fsanitize=shift', '-fsanitize-trap=shift',
                        '-c', str(c), '-o', str(obj)], check=True)
        subprocess.run(['lld-link', '/entry:main', '/subsystem:console', '/nodefaultlib',
                        '/out:' + str(exe), str(obj)], check=True)
        subprocess.run([str(exe)], check=True)
    print('SD file reads: 64-bit EOF oracle, overflow inputs, cluster runs, NULLs, shift bounds and partial-failure exits pass.')


if __name__ == '__main__':
    main()
