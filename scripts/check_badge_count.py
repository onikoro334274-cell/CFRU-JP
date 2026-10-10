"""Execute production four-region badge counting and save-screen formatting."""
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent


def main():
    source = (ROOT / 'src/badge_count.c').read_text(encoding='utf-8')
    functions = source[source.index('u8 GetAllBadgeCount(void)'):]
    headers = '\n'.join('#include "' + (ROOT / name).as_posix() + '"' for name in
                        ('include/constants/flags.h', 'src/Tables/hoenn_script_flags.h',
                         'src/Tables/johto_flags.h', 'src/Tables/sinnoh_flags.h'))
    fixture = r'''
typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
#define CHAR_0 0xA1
#define CHAR_HIRAGANA_KO 0x0A
#define EOS 0xFF
HEADERS
u8 flags[65536];
int calls;
u8 FlagGet(u16 flag) {calls++;return flags[flag];}
FUNCTIONS
u16 badges[4][8]={
{FLAG_BADGE01_GET,FLAG_BADGE01_GET+1,FLAG_BADGE01_GET+2,FLAG_BADGE01_GET+3,
 FLAG_BADGE01_GET+4,FLAG_BADGE01_GET+5,FLAG_BADGE01_GET+6,FLAG_BADGE01_GET+7},
{HOENN_FLAG_BADGE1,HOENN_FLAG_BADGE2,HOENN_FLAG_BADGE3,HOENN_FLAG_BADGE4,
 HOENN_FLAG_BADGE5,HOENN_FLAG_BADGE6,HOENN_FLAG_BADGE7,HOENN_FLAG_BADGE8},
{JOHTO_FLAG_BADGE1,JOHTO_FLAG_BADGE2,JOHTO_FLAG_BADGE3,JOHTO_FLAG_BADGE4,
 JOHTO_FLAG_BADGE5,JOHTO_FLAG_BADGE6,JOHTO_FLAG_BADGE7,JOHTO_FLAG_BADGE8},
{SINNOH_FLAG_BADGE1,SINNOH_FLAG_BADGE2,SINNOH_FLAG_BADGE3,SINNOH_FLAG_BADGE4,
 SINNOH_FLAG_BADGE5,SINNOH_FLAG_BADGE6,SINNOH_FLAG_BADGE7,SINNOH_FLAG_BADGE8}};
int check(int expected) {
    calls=0;
    if(GetAllBadgeCount()!=expected || calls!=32) return 1;
    u8 text[6]={77,77,77,77,77,77};
    calls=0;SaveStatBadgesToString(text+1);
    int pos=1;
    if(expected>=10 && text[pos++]!=CHAR_0+expected/10) return 2;
    if(text[pos++]!=CHAR_0+expected%10 || text[pos++]!=CHAR_HIRAGANA_KO
       || text[pos++]!=EOS || text[0]!=77 || text[pos]!=77 || calls!=32) return 3;
    return 0;
}
int main(void) {
    for(int a=0;a<32;a++) for(int b=a+1;b<32;b++)
        if(badges[a/8][a%8]==badges[b/8][b%8]) return 4;
    for(int a=0;a<=8;a++) for(int b=0;b<=8;b++)
    for(int c=0;c<=8;c++) for(int d=0;d<=8;d++) {
        int counts[4]={a,b,c,d};
        for(int r=0;r<4;r++) for(int n=0;n<8;n++) flags[badges[r][n]]=n<counts[r];
        if(check(a+b+c+d)) return 5;
    }
    for(int r=0;r<4;r++) for(int mask=0;mask<256;mask++) {
        int expected=0;
        for(int region=0;region<4;region++) for(int n=0;n<8;n++) {
            int value=region==r && ((mask>>n)&1);
            flags[badges[region][n]]=value; expected+=value;
        }
        if(check(expected)) return 6;
    }
    return 0;
}
'''.replace('HEADERS', headers).replace('FUNCTIONS', functions)
    with tempfile.TemporaryDirectory() as directory:
        path = Path(directory)
        c, obj, exe = (path / ('badges' + suffix) for suffix in ('.c', '.obj', '.exe'))
        c.write_text(fixture, encoding='utf-8')
        subprocess.run(['clang', '--target=x86_64-pc-windows-msvc', '-O2', '-fno-builtin',
                        '-fno-stack-protector', '-c', str(c), '-o', str(obj)], check=True)
        subprocess.run(['lld-link', '/entry:main', '/subsystem:console', '/nodefaultlib',
                        '/out:' + str(exe), str(obj)], check=True)
        subprocess.run([str(exe)], check=True)
    print('Badge counting/formatting pass 6561 regional count combinations and 1024 individual flag patterns, including 32 badges and buffer canaries.')


if __name__ == '__main__':
    main()
