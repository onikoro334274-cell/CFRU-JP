"""Execute exFAT name lookup with bounded caller-string access instrumentation."""
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
    readers = re.findall(r'^static u32 (?:Rd16|Rd32|Upper)\([^\n]+', source, re.M)
    assert len(readers) == 3
    production = function(source, 'static bool8 HackScsd_FindFile(struct ScsdFs *fs)')
    assert len(re.findall(r'name\[(?:namePos|nameLen)\]', production)) == 3
    production = re.sub(r'name\[(namePos|nameLen)\]', r'NameAt(name, \1)', production)
    fixture = r'''
#include <stddef.h>
typedef unsigned char u8;typedef unsigned int u32;typedef int s32;typedef u8 bool8;
#define TRUE 1
#define FALSE 0
struct ScsdFs {u8 buf[512];const char *longName;u32 type,rootSecs,rootLba,rootClus,spcShift,fileClus,fileSize;bool8 fileNoChain;};
static u8 sectors[2][512];static char requested[256];static unsigned requestedLength;static int errors,reads;
static char NameAt(const char *name,unsigned index){
    if(name!=requested||index>requestedLength){errors++;return 0;}return name[index];
}
static bool8 HackScsd_Read(struct ScsdFs *fs,u32 lba,u32 n,u8 *out){
    (void)fs;reads++;if(lba<1000||lba>1001||n!=1){errors++;return FALSE;}
    for(unsigned i=0;i<512;i++) out[i]=sectors[lba-1000][i];return TRUE;
}
static bool8 EndClus(struct ScsdFs *fs,u32 c){(void)fs;return c<2||c>=0xfffffff7;}
static u32 ClusLba(struct ScsdFs *fs,u32 c){(void)fs;return c==2?1000:0;}
static u32 NextClus(struct ScsdFs *fs,u32 c){(void)fs;(void)c;return 0xffffffff;}
static s32 ScanFat(struct ScsdFs *fs){(void)fs;errors++;return -1;}
READERS
PRODUCTION
static u8 *entry(unsigned index){return sectors[index/16]+(index%16)*32;}
int main(void){
    for(unsigned length=0;length<256;length++) for(requestedLength=0;requestedLength<256;requestedLength++)
    for(unsigned mismatch=0;mismatch<3;mismatch++){
        for(unsigned s=0;s<2;s++) for(unsigned i=0;i<512;i++) sectors[s][i]=0;
        for(unsigned i=0;i<requestedLength;i++) requested[i]='a'+i%26;requested[requestedLength]=0;
        unsigned fragments=(length+14)/15;
        entry(0)[0]=0x85;entry(0)[1]=fragments+1;
        u8 *stream=entry(1);stream[0]=0xc0;stream[1]=2;stream[3]=length;stream[20]=7;stream[24]=123;
        for(unsigned fragment=0;fragment<fragments;fragment++){
            u8 *part=entry(fragment+2);part[0]=0xc1;
            for(unsigned i=0;i<15&&fragment*15+i<length;i++) part[2+i*2]='A'+(fragment*15+i)%26;
        }
        if(length&&mismatch==1) entry(2)[2]='Z';
        if(length&&mismatch==2) entry(2)[3]=1;
        struct ScsdFs fs;fs.longName=requested;fs.type=0xef;fs.rootClus=2;fs.spcShift=1;
        fs.fileClus=99;fs.fileSize=999;fs.fileNoChain=FALSE;errors=reads=0;
        int expected=length>0&&length==requestedLength&&mismatch==0;
        if(HackScsd_FindFile(&fs)!=expected||errors||reads<1||reads>2) return 1;
        if(expected&&(fs.fileClus!=7||fs.fileSize!=123||!fs.fileNoChain)) return 2;
        if(!expected&&(fs.fileClus!=99||fs.fileSize!=999||fs.fileNoChain)) return 3;
    }
    struct ScsdFs fs;fs.type=0xef;fs.longName=NULL;errors=reads=0;
    if(HackScsd_FindFile(&fs)||errors||reads) return 4;
    return 0;
}
'''.replace('READERS', '\n'.join(readers)).replace('PRODUCTION', production)
    with tempfile.TemporaryDirectory() as directory:
        path = Path(directory)
        c, obj, exe = (path / ('exfat_names' + suffix) for suffix in ('.c', '.obj', '.exe'))
        c.write_text(fixture, encoding='utf-8')
        subprocess.run(['clang', '--target=x86_64-pc-windows-msvc', '-O2', '-fno-builtin',
                        '-fno-stack-protector', '-c', str(c), '-o', str(obj)], check=True)
        subprocess.run(['lld-link', '/entry:main', '/subsystem:console', '/nodefaultlib',
                        '/out:' + str(exe), str(obj)], check=True)
        subprocess.run([str(exe)], check=True)
    print('exFAT names: 196608 name-length/mismatch cases, cross-sector names, exact string boundaries and NULL rejection pass.')


if __name__ == '__main__':
    main()
