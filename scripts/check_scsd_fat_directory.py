"""Execute FAT directory decoding with shift traps and buffer canaries."""
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
    readers = re.findall(r'^static u(?:16|32) Rd(?:16|32)\([^\n]+', source, re.M)
    assert len(readers) == 2
    production = '\n'.join(readers) + '\n' + function(source, 'static s32 ScanFat(struct ScsdFs *fs)')
    fixture = r'''
typedef unsigned char u8;typedef unsigned short u16;typedef unsigned int u32;typedef int s32;typedef u8 bool8;
#define TRUE 1
#define FALSE 0
struct ScsdFs {u8 before,buf[512],after;char name83[11];u32 type,fileClus,fileSize;bool8 fileNoChain;};
PRODUCTION
static void init(struct ScsdFs *fs){
    fs->before=0x5a;fs->after=0xa5;fs->fileClus=0x12345678;fs->fileSize=0x87654321;fs->fileNoChain=TRUE;
    for(unsigned i=0;i<512;i++) fs->buf[i]=0;
    for(unsigned i=0;i<11;i++) fs->name83[i]='A'+i;
}
static void entry(struct ScsdFs *fs,unsigned index){
    for(unsigned i=0;i<11;i++) fs->buf[index*32+i]=fs->name83[i];
    fs->buf[index*32+11]=0x20;
}
int main(void){
    const u16 lows[]={0,0xffff,0xa5a5};
    struct ScsdFs fs;
    for(unsigned high=0;high<65536;high++) for(unsigned low=0;low<3;low++) for(unsigned fat32=0;fat32<2;fat32++){
        init(&fs);fs.type=fat32?32:16;entry(&fs,0);
        fs.buf[20]=high;fs.buf[21]=high>>8;fs.buf[26]=lows[low];fs.buf[27]=lows[low]>>8;
        fs.buf[28]=0x98;fs.buf[29]=0xba;fs.buf[30]=0xdc;fs.buf[31]=0xfe;
        u32 expected=(fat32?high*65536u:0)+lows[low];
        if(ScanFat(&fs)!=1||fs.fileClus!=expected||fs.fileSize!=0xfedcba98u||fs.fileNoChain||fs.before!=0x5a||fs.after!=0xa5) return 1;
    }
    for(unsigned position=0;position<16;position++) for(unsigned attribute=0;attribute<256;attribute++){
        init(&fs);fs.type=32;
        for(unsigned i=0;i<position;i++) fs.buf[i*32]='Z';
        entry(&fs,position);fs.buf[position*32+11]=attribute;
        int accepted=(attribute&0x18)==0;
        int expected=accepted?1:(position==15?0:-1);
        if(ScanFat(&fs)!=expected||fs.before!=0x5a||fs.after!=0xa5) return 2;
        if(!accepted&&(fs.fileClus!=0x12345678||fs.fileSize!=0x87654321||!fs.fileNoChain)) return 3;
    }
    init(&fs);entry(&fs,0);fs.buf[0]=0xe5;if(ScanFat(&fs)!=-1||fs.fileClus!=0x12345678) return 4;
    init(&fs);for(unsigned i=0;i<16;i++) fs.buf[i*32]='Z';
    if(ScanFat(&fs)!=0||fs.before!=0x5a||fs.after!=0xa5||fs.fileClus!=0x12345678) return 5;
    return 0;
}
'''.replace('PRODUCTION', production)
    with tempfile.TemporaryDirectory() as directory:
        path = Path(directory)
        c, obj, exe = (path / ('fat_directory' + suffix) for suffix in ('.c', '.obj', '.exe'))
        c.write_text(fixture, encoding='utf-8')
        subprocess.run(['clang', '--target=x86_64-pc-windows-msvc', '-O2', '-fno-builtin',
                        '-fno-stack-protector', '-fsanitize=shift', '-fsanitize-trap=shift',
                        '-c', str(c), '-o', str(obj)], check=True)
        subprocess.run(['lld-link', '/entry:main', '/subsystem:console', '/nodefaultlib',
                        '/out:' + str(exe), str(obj)], check=True)
        subprocess.run([str(exe)], check=True)
    print('FAT directory: 393216 cluster decodes, 4096 attribute/position cases, deleted/full directories and canaries pass with shift sanitizer.')


if __name__ == '__main__':
    main()
