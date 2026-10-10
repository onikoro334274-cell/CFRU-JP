"""Execute production rental party copies and packed save-variable accessors."""
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent


def function(source, signature):
    start = source.index(signature + '\n{')
    pos = source.index('{', start) + 1
    depth = 1
    while depth:
        depth += (source[pos] == '{') - (source[pos] == '}')
        pos += 1
    return source[start:pos]


def main():
    screen = (ROOT / 'src/hoenn_factory_screen.c').read_text(encoding='utf-8')
    factory = (ROOT / 'src/hoenn_factory.c').read_text(encoding='utf-8')
    functions = '\n'.join([function(factory, 'u16 HoennFactory_RentalPacked(u8 i)'),
                           function(factory, 'void HoennFactory_SetRentalPacked(u8 i, u16 v)'),
                           function(screen, 'static void Select_CopyMonsToPlayerParty(void)'),
                           function(screen, 'static void CopySwappedMonData(void)')])
    fixture = r'''
typedef unsigned char u8;typedef unsigned short u16;typedef unsigned int u32;
#define FRONTIER_PARTY_SIZE 3
#define SELECTABLE_MONS_COUNT 6
#define FACTORY_SELECTABLE 6
#include "src/Tables/hoenn_script_flags.h"
struct Pokemon {u8 bytes[100];};
static struct Pokemon gPlayerParty[6],gEnemyParty[6];
static struct {struct {u8 selectedId;u16 monId;struct Pokemon monData;} mons[6];} select,*sFactorySelectScreen=&select;
static struct {u8 playerMonId,enemyMonId;} swap,*sFactorySwapScreen=&swap;
static u16 saved[6];static int invalid,counted;
static u16 VarGet(u16 id) {if(id<HOENN_VAR_FACTORY_RENTAL || id>=HOENN_VAR_FACTORY_RENTAL+6){invalid=1;return 0;}return saved[id-HOENN_VAR_FACTORY_RENTAL];}
static void VarSet(u16 id,u16 value) {if(id<HOENN_VAR_FACTORY_RENTAL || id>=HOENN_VAR_FACTORY_RENTAL+6){invalid=1;return;}saved[id-HOENN_VAR_FACTORY_RENTAL]=value;}
static void Memcpy(void* dst,const void* src,u32 size) {for(u32 i=0;i<size;i++) ((u8*)dst)[i]=((const u8*)src)[i];}
static void CalculatePlayerPartyCount(void) {counted++;}
FUNCTIONS
int main(void) {
    for(int high=0;high<64;high++) for(int a=0;a<6;a++) for(int b=0;b<6;b++) for(int c=0;c<6;c++) {
        if(a==b || a==c || b==c) continue;
        int choices[3]={a,b,c};invalid=counted=0;
        for(int i=0;i<6;i++) {
            saved[i]=select.mons[i].monId=(high<<10)|100+i;select.mons[i].selectedId=0;
            for(int k=0;k<100;k++){select.mons[i].monData.bytes[k]=i*17+k;gPlayerParty[i].bytes[k]=0xA5;}
        }
        for(int i=0;i<3;i++) select.mons[choices[i]].selectedId=i+1;
        Select_CopyMonsToPlayerParty();
        if(invalid || counted!=1) return 1;
        for(int i=0;i<6;i++) {
            int expected=i<3 ? choices[i] : i;
            if(saved[i]!=((high<<10)|100+expected)) return 2;
            for(int k=0;k<100;k++) if(gPlayerParty[i].bytes[k]!=(i<3 ? (u8)(expected*17+k) : 0xA5)) return 3;
        }
    }
    for(int high=0;high<64;high++) for(int own=0;own<3;own++) for(int enemy=0;enemy<3;enemy++) {
        invalid=0;swap.playerMonId=own;swap.enemyMonId=enemy;
        for(int i=0;i<6;i++) {
            saved[i]=(high<<10)|200+i;
            for(int k=0;k<100;k++){gPlayerParty[i].bytes[k]=i*11+k;gEnemyParty[i].bytes[k]=i*17+k;}
        }
        CopySwappedMonData();
        if(invalid) return 4;
        for(int i=0;i<6;i++) {
            if(saved[i]!=((high<<10)|200+(i==own ? enemy+3 : i))) return 5;
            for(int k=0;k<100;k++) if(gPlayerParty[i].bytes[k]!=(u8)(i==own ? enemy*17+k : i*11+k)) return 6;
        }
    }
    return 0;
}
'''.replace('FUNCTIONS', functions)
    with tempfile.TemporaryDirectory() as directory:
        path = Path(directory)
        c, obj, exe = (path / ('party_copy' + suffix) for suffix in ('.c', '.obj', '.exe'))
        c.write_text(fixture, encoding='utf-8')
        subprocess.run(['clang', '--target=x86_64-pc-windows-msvc', '-O2', '-fno-builtin',
                        '-fno-stack-protector', '-I', str(ROOT), '-c', str(c), '-o', str(obj)], check=True)
        subprocess.run(['lld-link', '/entry:main', '/subsystem:console', '/nodefaultlib',
                        '/out:' + str(exe), str(obj)], check=True)
        subprocess.run([str(exe)], check=True)
    print('Factory party copy: 7680 ordered selections and 576 swaps preserve party bytes and packed rental slots.')


if __name__ == '__main__':
    main()
