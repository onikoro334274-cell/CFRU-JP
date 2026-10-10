"""Verify ribbon-mask bit mapping through standard Pokemon data APIs."""
import json
from pathlib import Path
import re
import subprocess
import tempfile
from check_ai_difficulty import function
ROOT = Path(__file__).resolve().parent.parent


def main():
    source = (ROOT / 'src/scripting.c').read_text(encoding='utf-8')
    source = re.sub(r'/\*.*?\*/|//[^\n]*', '', source, flags=re.S)
    production = function(source, 'sp011_RibbonSetterCleaner')
    fixture = r'''
typedef unsigned char u8;typedef unsigned short u16;typedef unsigned int u32;typedef u8 bool8;
#define NULL ((void*)0)
#define PARTY_SIZE 6
#define MON_DATA_CHAMPION_RIBBON 67
#define MON_DATA_COOL_RIBBON 50
#define MON_DATA_WINNING_RIBBON 68
struct Pokemon{u32 ribbons;u32 other;};static struct Pokemon gPlayerParty[6];
static u16 Var8004,Var8005;static int reads,writes,errors;
/* Independent explicit native field->aggregate mask mapping. */
static const int shifts[]={1,4,7,10,13};
static int shift(int field){if(field>=50&&field<=54)return shifts[field-50];if(field==67)return 0;if(field>=68&&field<=78)return field-52;errors++;return 0;}
static int width(int field){return field>=50&&field<=54?3:1;}
static u32 GetMonData(struct Pokemon*m,int field,void*p){reads++;if(m!=&gPlayerParty[Var8004]||p)errors++;int n=shift(field);return(m->ribbons>>n)&((1u<<width(field))-1);}
static void SetMonData(struct Pokemon*m,int field,const void*p){writes++;if(m!=&gPlayerParty[Var8004]||reads!=1)errors++;int n=shift(field);u32 mask=((1u<<width(field))-1)<<n;m->ribbons=(m->ribbons&~mask)|((u32)*(const u8*)p<<n);}
PRODUCTION
int main(void){
 const u32 originals[]={0,0x07ffffff,0x05555555,0x02aaaaaa};
 for(int party=0;party<8;party++)for(u32 input=0;input<65536;input++)for(int k=0;k<4;k++){
  Var8004=party;Var8005=input;reads=writes=errors=0;
  for(int i=0;i<6;i++){gPlayerParty[i].ribbons=originals[k];gPlayerParty[i].other=0xdeadbeef;}
  sp011_RibbonSetterCleaner();u32 index=input&~256u;int valid=party<6&&index<=26;
  if(errors||reads!=valid||writes!=valid||Var8005!=input)return 1;
  for(int i=0;i<6;i++){
   u32 expected=originals[k];if(valid&&i==party){u32 bit=1u<<index;expected=(input&256)?expected&~bit:expected|bit;}
   if(gPlayerParty[i].ribbons!=expected||gPlayerParty[i].other!=0xdeadbeef)return 2;
  }
 }return 0;}
'''.replace('PRODUCTION', production)
    with tempfile.TemporaryDirectory() as temp:
        c, obj, exe = (Path(temp) / ('ribbons' + ext) for ext in ('.c', '.obj', '.exe'))
        c.write_text(fixture, encoding='utf-8')
        subprocess.run(['clang', '--target=x86_64-pc-windows-msvc', '-O2', '-fno-builtin', '-fno-stack-protector', '-c', str(c), '-o', str(obj)], check=True)
        subprocess.run(['lld-link', '/entry:main', '/subsystem:console', '/nodefaultlib', str(obj), '/out:' + str(exe)], check=True)
        subprocess.run([str(exe)], check=True)
    report = {'result': 'passed', 'cases': 2097152,
              'scope': 'Production ribbon setter for every u16 command, six valid/two invalid party indices and four ribbon masks. Aggregate bits 0..26 mapped independently to champion/contest ranks/individual ribbons; exactly one read/write on valid input, invalid commands rejected and unrelated fields/slots preserved.',
              'behavior_change': 'Reserved aggregate bits 27..31 have no native MON_DATA_RIBBONS representation and are rejected. Removes direct fixed-address secure-data write; native Get/SetMonData handle substructure order/encryption/checksum.',
              'limitations': 'Native data APIs simulated; native encryption/checksum and linked ROM execution not exercised by this fixture.'}
    (ROOT / 'audit/script_ribbons_review.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    print('2097152 ribbon command/mask cases passed')


if __name__ == '__main__':
    main()
