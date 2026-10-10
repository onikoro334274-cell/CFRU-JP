"""Execute partner registration independently of the preceding script check."""
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
    source = (ROOT / 'src/hack_story_partner.c').read_text(encoding='utf-8')
    if args.baseline:
        source = subprocess.run(['git', 'show', args.baseline + ':src/hack_story_partner.c'], cwd=ROOT,
                                capture_output=True, encoding='utf-8', check=True).stdout
    defines = '\n'.join(re.findall(r'^#define (?:FLAG_STORY_PARTNER|VAR_PARTNER_\w+|PARTNER_BONUS)\s+[^\n]+', source, re.M))
    pokemon = (ROOT / 'include/pokemon.h').read_text(encoding='utf-8')
    for field in ('MON_DATA_SPECIES', 'MON_DATA_IS_EGG'):
        defines += '\n' + re.search(r'^#define ' + field + r'\s+[^\n]+', pokemon, re.M)[0]
    production = '\n'.join(function(source, signature) for signature in (
        'void HoennNative_StoryPartnerCheck(void)', 'void HoennNative_StoryMakePartner(void)',
    ))
    fixture = r'''
#include <stddef.h>
typedef unsigned char u8;typedef unsigned short u16;typedef unsigned int u32;typedef u8 bool8;
#define TRUE 1
#define FALSE 0
#define PARTY_SIZE 6
#define SPECIES_NONE 0
DEFINES
static struct Pokemon {u32 personality,otid;u16 species;u8 egg;} gPlayerParty[6];
static u16 Var8004,gSpecialVar_LastResult,vars[4];
static int flag,sets,stats,errors;
static bool8 FlagGet(u16 id){if(id!=FLAG_STORY_PARTNER) errors++;return flag;}
static void FlagSet(u16 id){if(id!=FLAG_STORY_PARTNER) errors++;flag=1;sets++;}
static void VarSet(u16 id,u16 value){if(id<0x50f0||id>0x50f3){errors++;return;}vars[id-0x50f0]=value;}
static u32 GetMonData(struct Pokemon *mon,int field,void *out){
    if(mon<gPlayerParty||mon>=gPlayerParty+6||out){errors++;return 0;}
    if(field==MON_DATA_SPECIES) return mon->species;
    if(field==MON_DATA_IS_EGG) return mon->egg;
    errors++;return 0;
}
static void CalculateMonStats(struct Pokemon *mon){if(mon!=gPlayerParty+Var8004) errors++;stats++;}
PRODUCTION
int main(void){
    for(unsigned slot=0;slot<65536;slot++) for(int existing=0;existing<2;existing++)
    for(int state=0;state<3;state++){
        for(unsigned i=0;i<6;i++){
            gPlayerParty[i].personality=0x89abcdefu+i;gPlayerParty[i].otid=0xfedcba98u-i;
            gPlayerParty[i].species=state?1:0;gPlayerParty[i].egg=state==2;
        }
        for(unsigned i=0;i<4;i++) vars[i]=0x5a5a;
        Var8004=slot;flag=existing;sets=stats=errors=0;gSpecialVar_LastResult=0x55aa;
        HoennNative_StoryMakePartner();int valid=slot<6&&state==1&&!existing;
        if(sets!=valid||stats!=valid||flag!=(existing||valid)||errors||gSpecialVar_LastResult!=0x55aa) return 1;
        for(unsigned i=0;i<4;i++){
            u16 expected=0x5a5a;
            if(valid){u32 value=i<2?gPlayerParty[slot].personality:gPlayerParty[slot].otid;expected=i%2?value>>16:value;}
            if(vars[i]!=expected) return 2;
        }
        HoennNative_StoryPartnerCheck();if(gSpecialVar_LastResult!=(slot<6&&state==1?0:1)||errors) return 3;
        if(valid){
            Var8004=(slot+1)%6;sets=stats=0;HoennNative_StoryMakePartner();
            if(sets||stats) return 4;
        }
    }
    return 0;
}
'''.replace('DEFINES', defines).replace('PRODUCTION', production)
    with tempfile.TemporaryDirectory() as directory:
        path = Path(directory)
        c, obj, exe = (path / ('partner' + suffix) for suffix in ('.c', '.obj', '.exe'))
        c.write_text(fixture, encoding='utf-8')
        subprocess.run(['clang', '--target=x86_64-pc-windows-msvc', '-O2', '-fno-builtin',
                        '-fno-stack-protector', '-c', str(c), '-o', str(obj)], check=True)
        subprocess.run(['lld-link', '/entry:main', '/subsystem:console', '/nodefaultlib',
                        '/out:' + str(exe), str(obj)], check=True)
        subprocess.run([str(exe)], check=True)
    print('Story partner: 393216 slot/state/registration cases pass; invalid choices preserve identity and valid registration remains permanent.')


if __name__ == '__main__':
    main()
