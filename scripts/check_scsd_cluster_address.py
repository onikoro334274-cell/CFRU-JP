"""Execute cluster addressing and FAT cache lookup with a wide arithmetic oracle."""
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
    readers = re.findall(r'^static u32 Rd(?:16|32)\([^\n]+', source, re.M)
    if 'static u32 ClusLba(struct ScsdFs *fs, u32 c)\n{' in source:
        cluster = function(source, 'static u32 ClusLba(struct ScsdFs *fs, u32 c)')
    else:
        cluster = re.search(r'^static u32 ClusLba[^\n]+', source, re.M)[0]
    production = '\n'.join(readers) + '\n' + function(source, 'static bool8 EndClus(struct ScsdFs *fs, u32 c)') + '\n' + cluster
    production += '\n' + function(source, 'static u32 NextClus(struct ScsdFs *fs, u32 c)')
    production += '\n' + function(source, 'static u32 FileLba(struct ScsdFs *fs, u32 n)')
    fixture = r'''
typedef unsigned char u8;typedef unsigned int u32;typedef u8 bool8;
#define TRUE 1
#define FALSE 0
struct ScsdFs {u32 type,data,spcShift,fat,fatCached,fileClus;bool8 fileNoChain;u8 fatBuf[512];};
static int reads,fail,errors;
static bool8 HackScsd_Read(struct ScsdFs *fs,u32 lba,u32 n,u8 *out){
    reads++;if(n!=1||out!=fs->fatBuf||lba<fs->fat){errors++;return FALSE;}
    if(fail) return FALSE;
    unsigned width=fs->type==16?2:4,per=512/width;
    for(unsigned i=0;i<per;i++){
        u32 value=(lba-fs->fat)*per+i+1;
        if(fs->type==32) value|=0xf0000000;
        for(unsigned b=0;b<width;b++) out[i*width+b]=value>>(b*8);
    }
    return TRUE;
}
PRODUCTION
static u32 oracle(struct ScsdFs *fs,u32 c){
    u32 end=fs->type==16?0xfff8u:fs->type==32?0x0ffffff8u:0xfffffff7u;
    if(c<2||c>=end||fs->spcShift>=32) return 0;
    unsigned long long lba=fs->data+((unsigned long long)(c-2)<<fs->spcShift);
    return lba>0xffffffffULL?0:(u32)lba;
}
int main(void){
    const u32 types[]={16,32,0xef},datas[]={0,1,1000,0xffffff00,0xffffffff},shifts[]={0,1,7,16,31,32,255};
    const u32 offsets[]={0,1,31,0xffffffff};struct ScsdFs fs;
    for(unsigned t=0;t<3;t++) for(unsigned c=0;c<65536;c++) for(unsigned d=0;d<5;d++) for(unsigned s=0;s<7;s++){
        fs.type=types[t];fs.data=datas[d];fs.spcShift=shifts[s];fs.fileClus=c;fs.fileNoChain=TRUE;
        u32 base=oracle(&fs,c);
        if(ClusLba(&fs,c)!=base) return 1;
        for(unsigned i=0;i<4;i++){
            unsigned long long sum=(unsigned long long)base+offsets[i];
            u32 expected=base&&sum<=0xffffffffULL?(u32)sum:0;
            if(FileLba(&fs,offsets[i])!=expected) return 2;
        }
    }
    for(unsigned t=0;t<3;t++){
        fs.type=types[t];fs.data=1000;fs.fat=100;fs.spcShift=0;fs.fileNoChain=FALSE;
        fs.fatCached=0xffffffff;reads=errors=fail=0;
        for(unsigned c=2;c<1024;c++) if(NextClus(&fs,c)!=c+1) return 3;
        if(errors||reads!=(t==0?4:8)) return 4;
        for(unsigned s=0;s<4;s++) for(unsigned n=0;n<32;n++){
            fs.spcShift=s;fs.fileClus=126;fs.fatCached=0xffffffff;reads=errors=0;
            if(FileLba(&fs,n)!=1000+(126-2)*(1u<<s)+n||errors) return 5;
        }
        fs.fat=0xffffffff;fs.fatCached=0xffffffff;reads=errors=0;
        if(NextClus(&fs,2)!=3||NextClus(&fs,2)!=3||reads!=2||errors) return 6;
        reads=0;if(NextClus(&fs,512)!=0xffffffff||reads) return 7;
        fs.fat=100;fs.fatCached=0xffffffff;fail=1;reads=0;
        if(NextClus(&fs,2)!=0xffffffff||reads!=1||fs.fatCached!=0xffffffff) return 8;
        fail=0;reads=0;if(NextClus(&fs,0)!=0xffffffff||reads) return 9;
    }
    return 0;
}
'''.replace('PRODUCTION', production)
    with tempfile.TemporaryDirectory() as directory:
        path = Path(directory)
        c, obj, exe = (path / ('cluster_address' + suffix) for suffix in ('.c', '.obj', '.exe'))
        c.write_text(fixture, encoding='utf-8')
        subprocess.run(['clang', '--target=x86_64-pc-windows-msvc', '-O2', '-fno-builtin',
                        '-fno-stack-protector', '-fsanitize=shift', '-fsanitize-trap=shift',
                        '-c', str(c), '-o', str(obj)], check=True)
        subprocess.run(['lld-link', '/entry:main', '/subsystem:console', '/nodefaultlib',
                        '/out:' + str(exe), str(obj)], check=True)
        subprocess.run([str(exe)], check=True)
    print('SD cluster addressing: 6881280 geometry cases, offset overflows, FAT chain/cache boundaries and failed reads pass.')


if __name__ == '__main__':
    main()
