"""Execute partner identity reconstruction with trapping shift sanitization."""
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
    production = function(source, 'bool8 HackPartner_IsMon(const struct Pokemon* mon)') + '\n' + function(source, 'u8 HackPartner_BaseBonus(const struct Pokemon* mon)')
    fixture = r'''
typedef unsigned char u8;typedef unsigned short u16;typedef unsigned int u32;typedef u8 bool8;
#define TRUE 1
#define FALSE 0
DEFINES
struct Pokemon {u32 personality,otid;};
static u16 vars[4];static int flag,reads,errors;
static bool8 FlagGet(u16 id){if(id!=FLAG_STORY_PARTNER) errors++;return flag;}
static u16 VarGet(u16 id){reads++;if(id<0x50f0||id>0x50f3){errors++;return 0;}return vars[id-0x50f0];}
PRODUCTION
int main(void){
    const u16 lows[]={0,1,0x7fff,0x8000,0xffff};
    for(unsigned high=0;high<65536;high++) for(unsigned i=0;i<5;i++){
        u32 pid=high*65536u+lows[i],ot=((high^0xa5a5u)*65536u)+(u16)(lows[i]^0x5a5a);
        vars[0]=pid;vars[1]=pid>>16;vars[2]=ot;vars[3]=ot>>16;
        struct Pokemon mon={pid,ot};flag=1;reads=errors=0;
        if(!HackPartner_IsMon(&mon)||reads!=4||errors||HackPartner_BaseBonus(&mon)!=20) return 1;
        for(unsigned bit=0;bit<32;bit++){
            mon.personality=pid^(1u<<bit);mon.otid=ot;
            if(HackPartner_IsMon(&mon)||HackPartner_BaseBonus(&mon)) return 2;
            mon.personality=pid;mon.otid=ot^(1u<<bit);
            if(HackPartner_IsMon(&mon)||HackPartner_BaseBonus(&mon)) return 3;
        }
        mon.personality=pid;mon.otid=ot;flag=0;reads=0;
        if(HackPartner_IsMon(&mon)||HackPartner_BaseBonus(&mon)||reads||errors) return 4;
        if(mon.personality!=pid||mon.otid!=ot) return 5;
    }
    return 0;
}
'''.replace('DEFINES', defines).replace('PRODUCTION', production)
    with tempfile.TemporaryDirectory() as directory:
        path = Path(directory)
        c, obj, exe = (path / ('partner_identity' + suffix) for suffix in ('.c', '.obj', '.exe'))
        c.write_text(fixture, encoding='utf-8')
        subprocess.run(['clang', '--target=x86_64-pc-windows-msvc', '-O2', '-fno-builtin',
                        '-fno-stack-protector', '-fsanitize=shift', '-fsanitize-trap=shift',
                        '-c', str(c), '-o', str(obj)], check=True)
        subprocess.run(['lld-link', '/entry:main', '/subsystem:console', '/nodefaultlib',
                        '/out:' + str(exe), str(obj)], check=True)
        subprocess.run([str(exe)], check=True)
    print('Partner identity: 327680 full-width identities, every single-bit mismatch and bonus gating pass with shift sanitizer.')


if __name__ == '__main__':
    main()
