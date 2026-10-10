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
    parser.add_argument('--directory-only', action='store_true')
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
    requested[0]='a';requested[1]=0;requestedLength=1;
    for(unsigned attribute=0;attribute<65536;attribute++) for(unsigned fault=0;fault<6;fault++){
        for(unsigned s=0;s<2;s++) for(unsigned i=0;i<512;i++) sectors[s][i]=0;
        entry(0)[0]=0x85;entry(0)[1]=2;entry(0)[4]=attribute;entry(0)[5]=attribute>>8;
        entry(1)[0]=0xc0;entry(1)[1]=2;entry(1)[3]=1;entry(1)[20]=7;entry(1)[24]=123;
        entry(2)[0]=0xc1;entry(2)[2]='A';
        if(fault==1){for(unsigned i=0;i<32;i++){u8 value=entry(1)[i];entry(1)[i]=entry(2)[i];entry(2)[i]=value;}}
        if(fault==2){entry(0)[1]=3;for(unsigned i=0;i<32;i++) entry(3)[i]=entry(2)[i];for(unsigned i=0;i<32;i++) entry(2)[i]=entry(1)[i];}
        if(fault==3) entry(0)[0]=0x81;
        if(fault==4) entry(0)[1]=1;
        if(fault==5) entry(1)[3]=0;
        struct ScsdFs current;current.type=0xef;current.longName=requested;current.rootClus=2;current.spcShift=1;
        current.fileClus=99;current.fileSize=999;current.fileNoChain=FALSE;errors=reads=0;
        int expected=fault==0&&!(attribute&0x10);
        if(HackScsd_FindFile(&current)!=expected||errors) return 4;
        if(!expected&&(current.fileClus!=99||current.fileSize!=999||current.fileNoChain)) return 5;
    }
    for(unsigned s=0;s<2;s++) for(unsigned i=0;i<512;i++) sectors[s][i]=0;
    for(unsigned group=0;group<2;group++){
        unsigned base=group*3;entry(base)[0]=0x85;entry(base)[1]=2;entry(base)[4]=group?0:0x10;
        entry(base+1)[0]=0xc0;entry(base+1)[3]=1;entry(base+1)[20]=group?8:7;entry(base+1)[24]=group?124:123;
        entry(base+2)[0]=0xc1;entry(base+2)[2]='A';
    }
    struct ScsdFs fs;fs.type=0xef;fs.longName=requested;fs.rootClus=2;fs.spcShift=1;errors=reads=0;
    if(!HackScsd_FindFile(&fs)||errors||fs.fileClus!=8||fs.fileSize!=124||fs.fileNoChain) return 6;
    fs.longName=NULL;errors=reads=0;
    if(HackScsd_FindFile(&fs)||errors||reads) return 4;
    return 0;
}
'''.replace('READERS', '\n'.join(readers)).replace('PRODUCTION', production)
    if args.directory_only:
        fixture = fixture.replace('for(unsigned attribute=0;attribute<65536;attribute++) for(unsigned fault=0;fault<6;fault++)',
                                  'for(unsigned attribute=0x10;attribute<0x11;attribute++) for(unsigned fault=0;fault<1;fault++)')
    with tempfile.TemporaryDirectory() as directory:
        path = Path(directory)
        c, obj, exe = (path / ('exfat_names' + suffix) for suffix in ('.c', '.obj', '.exe'))
        c.write_text(fixture, encoding='utf-8')
        subprocess.run(['clang', '--target=x86_64-pc-windows-msvc', '-O2', '-fno-builtin',
                        '-fno-stack-protector', '-c', str(c), '-o', str(obj)], check=True)
        subprocess.run(['lld-link', '/entry:main', '/subsystem:console', '/nodefaultlib',
                        '/out:' + str(exe), str(obj)], check=True)
        subprocess.run([str(exe)], check=True)
    print('exFAT directory attribute rejected; existing name cases pass.' if args.directory_only else
          'exFAT names: 196608 string cases and 393216 attribute/entry-order cases, cross-sector boundaries and NULL rejection pass.')


if __name__ == '__main__':
    main()
