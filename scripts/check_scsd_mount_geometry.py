"""Execute FAT/exFAT mounting against controlled boot sectors and 64-bit arithmetic."""
import argparse
import re
import subprocess
import tempfile
from pathlib import Path
from check_factory_party_copy import function

ROOT = Path(__file__).resolve().parent.parent


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--baseline')
    args = parser.parse_args()
    source = (ROOT / 'src/hack_scsd.c').read_text(encoding='utf-8')
    if args.baseline:
        source = subprocess.run(['git', 'show', args.baseline + ':src/hack_scsd.c'], cwd=ROOT,
                                capture_output=True, encoding='utf-8', check=True).stdout
    helpers = re.findall(r'^static u32 (?:Rd16|Rd32|Log2)\([^\n]+', source, re.M)
    assert len(helpers) == 3
    production = '\n'.join(helpers) + '\n' + function(source, 'static bool8 HackScsd_Mount(struct ScsdFs *fs)')
    fixture = r'''
#include <stddef.h>
typedef unsigned char u8;typedef unsigned int u32;typedef u8 bool8;
void *memset(void *dest,int value,size_t n){volatile u8 *p=dest;while(n--) *p++=(u8)value;return dest;}
#define TRUE 1
#define FALSE 0
struct ScsdFs {u8 buf[512];u32 part,fatCached,type,fat,data,spcShift,rootClus,rootLba,rootSecs;};
static u8 boot[512],mbr[512];static u32 partition;static int useMbr,calls,errors,fail;
static bool8 HackScsd_Read(struct ScsdFs *fs,u32 lba,u32 n,u8 *out){
    (void)fs;calls++;if(n!=1||lba!=(calls==1?0:partition)) errors++;
    if(calls==fail) return FALSE;
    const u8 *src=useMbr&&calls==1?mbr:boot;
    for(unsigned i=0;i<512;i++) out[i]=src[i];return TRUE;
}
PRODUCTION
static void wr32(u8 *p,u32 value){for(unsigned i=0;i<4;i++) p[i]=value>>(8*i);}
static void wr16(u8 *p,u32 value){p[0]=value;p[1]=value>>8;}
static int check(unsigned format,int mb,u32 part,unsigned shift,u32 size,unsigned fats,u32 root,unsigned reserved,unsigned entries){
    for(unsigned i=0;i<512;i++) boot[i]=mbr[i]=0;
    useMbr=mb;partition=mb?part:0;calls=errors=fail=0;wr32(mbr+0x1c6,partition);
    unsigned long long fat,data;int valid;unsigned expectedShift=0;
    if(format==2){
        boot[3]='E';boot[4]='X';boot[5]='F';boot[6]='A';boot[7]='T';boot[0x6c]=9;boot[0x6d]=shift;
        wr32(boot+0x50,size);wr32(boot+0x58,reserved);wr32(boot+0x60,root);
        fat=(unsigned long long)partition+size;data=(unsigned long long)partition+reserved;
        valid=shift<=16&&root>=2&&fat<=0xffffffffULL&&data<=0xffffffffULL;expectedShift=shift;
    }else{
        boot[0]=0xeb;wr16(boot+0x0b,512);boot[0x0d]=shift;boot[0x10]=fats;wr16(boot+0x0e,reserved);
        if(format==0){size=(unsigned short)size;wr16(boot+0x16,size);wr16(boot+0x11,entries);}
        else{wr32(boot+0x24,size);wr32(boot+0x2c,root);}
        fat=(unsigned long long)partition+reserved;data=fat+(unsigned long long)fats*size;
        if(format==0) data+=((unsigned long long)entries*32+511)/512;
        valid=shift>0&&(shift&(shift-1))==0&&fats>0&&reserved>0&&size>0&&fat<=0xffffffffULL&&data<=0xffffffffULL;
        valid=valid&&(format==0?entries>0:root>=2);
        for(unsigned x=shift;x>1;x>>=1) expectedShift++;
    }
    struct ScsdFs fs={0};int result=HackScsd_Mount(&fs);
    if(result!=valid||errors) return 1;
    if(valid&&(fs.part!=partition||fs.fat!=fat||fs.data!=data||fs.spcShift!=expectedShift||fs.fatCached!=0xffffffffu||fs.type!=(format==0?16:format==1?32:0xef))) return 2;
    if(valid&&format!=0&&fs.rootClus!=root) return 3;
    return 0;
}
int main(void){
    for(unsigned format=0;format<3;format++) for(unsigned shift=0;shift<256;shift++) for(int mb=0;mb<2;mb++)
        if(check(format,mb,32,shift,128,2,2,32,512)) return 1;
    const u32 parts[]={0,1,0xffffff00,0xffffffff},sizes[]={0,1,0xffff,0xffffffff},roots[]={0,1,2,0xffffffff};
    const unsigned counts[]={0,1,2,255},reserves[]={0,1,32,65535};
    for(unsigned format=0;format<3;format++) for(unsigned p=0;p<4;p++) for(unsigned s=0;s<4;s++)
    for(unsigned r=0;r<4;r++) for(unsigned c=0;c<4;c++) for(unsigned reserve=0;reserve<4;reserve++)
        if(check(format,1,parts[p],format==2?0:8,sizes[s],counts[c],roots[r],reserves[reserve],512)) return 2;
    if(check(0,1,32,8,128,2,2,32,0)) return 3;
    for(int failure=1;failure<=2;failure++){
        check(1,1,32,8,128,2,2,32,512);struct ScsdFs fs={0};fail=failure;calls=errors=0;
        if(HackScsd_Mount(&fs)||errors||calls!=failure) return 4;
    }
    return 0;
}
'''.replace('PRODUCTION', production)
    with tempfile.TemporaryDirectory() as directory:
        path = Path(directory)
        c, obj, exe = (path / ('mount_geometry' + suffix) for suffix in ('.c', '.obj', '.exe'))
        c.write_text(fixture, encoding='utf-8')
        subprocess.run(['clang', '--target=x86_64-pc-windows-msvc', '-O2', '-fno-builtin',
                        '-fno-stack-protector', '-fsanitize=shift', '-fsanitize-trap=shift',
                        '-c', str(c), '-o', str(obj)], check=True)
        subprocess.run(['lld-link', '/entry:main', '/subsystem:console', '/nodefaultlib',
                        '/out:' + str(exe), str(obj)], check=True)
        subprocess.run([str(exe)], check=True)
    print('SD mount: all cluster bytes, FAT/exFAT direct/partition boots, 3072 arithmetic boundaries and I/O failures pass.')


if __name__ == '__main__':
    main()
