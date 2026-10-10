"""Validate hyper-training inputs and bit updates for all u16 stat values."""
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
    production = function(source, 'CanHyperTraining') + '\n' + function(source, 'sp12F_SetHyperTraining')
    fixture = r'''
typedef unsigned char u8;typedef unsigned short u16;typedef unsigned int u32;typedef u8 bool8;
#define NULL ((void*)0)
#define TRUE 1
#define FALSE 0
#define PARTY_SIZE 6
#define MON_DATA_HP_IV 10
enum{CheckIVs_SpDef=5,SetAllIVs=6};
struct Pokemon{u8 hyperTraining;};static struct Pokemon gPlayerParty[6];
static const u32 gBitTable[]={1,2,4,8,16,32};static u16 Var8004,Var8005;
static int maxIV,errors,reads,calculations,ivPattern=-1;
static u32 GetMonData(struct Pokemon*m,int field,void*p){
 reads++;if(m<gPlayerParty||m>=gPlayerParty+6||field<10||field>15||p)errors++;
 return (ivPattern>=0?(ivPattern&(1u<<(field-10)))!=0:maxIV)?31:17;
}
static void CalculateMonStats(struct Pokemon*m){calculations++;if(m!=&gPlayerParty[Var8004])errors++;}
PRODUCTION
int main(void){
 for(int iv=0;iv<2;iv++)for(int trained=0;trained<2;trained++)for(int party=0;party<8;party++)for(u32 stat=0;stat<65536;stat++){
  maxIV=iv;Var8004=party;Var8005=stat;
  for(int i=0;i<6;i++)gPlayerParty[i].hyperTraining=trained?63:0;
  reads=calculations=errors=0;
  int can=CanHyperTraining(party,stat);
  int expectedCan=party<6&&stat<6&&!iv&&!trained;
  if(errors||can!=expectedCan||reads!=(party<6&&stat<6)||calculations)return 1;
  reads=calculations=errors=0;
  int result=sp12F_SetHyperTraining();
  int valid=party<6&&stat<=6;
  int expected=valid&&!iv&&!trained;
  int expectedReads=valid?(stat==6?6:1):0;
  if(errors||result!=expected||calculations!=expected||reads!=expectedReads)return 2;
  for(int i=0;i<6;i++){
   u32 mask=trained?63:0;
   if(expected&&i==party)mask=stat==6?63:1u<<stat;
   if(gPlayerParty[i].hyperTraining!=mask)return 3;
  }
 }
 for(ivPattern=0;ivPattern<64;ivPattern++)for(int trained=0;trained<64;trained++)for(int party=0;party<6;party++)for(int stat=0;stat<=6;stat++){
  Var8004=party;Var8005=stat;for(int i=0;i<6;i++)gPlayerParty[i].hyperTraining=trained;
  reads=calculations=errors=0;
  u32 eligible=(~ivPattern)&(~trained)&63;
  u32 added=stat==6?eligible:eligible&(1u<<stat);
  int result=sp12F_SetHyperTraining();
  if(errors||result!=(added!=0)||calculations!=(added!=0)||reads!=(stat==6?6:1))return 4;
  for(int i=0;i<6;i++)if(gPlayerParty[i].hyperTraining!=(trained|(i==party?added:0)))return 5;
 }
 return 0;}
'''.replace('PRODUCTION', production)
    with tempfile.TemporaryDirectory() as temp:
        c, obj, exe = (Path(temp) / ('hyper' + ext) for ext in ('.c', '.obj', '.exe'))
        c.write_text(fixture, encoding='utf-8')
        subprocess.run(['clang', '--target=x86_64-pc-windows-msvc', '-O2', '-fno-builtin', '-fno-stack-protector', '-c', str(c), '-o', str(obj)], check=True)
        subprocess.run(['lld-link', '/entry:main', '/subsystem:console', '/nodefaultlib', str(obj), '/out:' + str(exe)], check=True)
        subprocess.run([str(exe)], check=True)
    report = {'result': 'passed', 'bounds_cases': 2097152, 'mixed_mask_cases': 172032,
              'scope': 'Production eligibility and training special for all u16 stat values, six valid/two invalid party slots, max/nonmax IV and trained/untrained masks. Invalid indices never access mon data; stat 6 trains all; valid individual stats alter exactly their bit and recalculate only when changed.',
              'mixed_scope': 'All 64 max-IV patterns and 64 existing training masks, seven stat requests and six party slots; only eligible bits added and unrelated slots preserved.',
              'limitations': 'Native stats calculations simulated. NPC inventory flow and linked runtime remain unverified.'}
    (ROOT / 'audit/hyper_training_bounds_review.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    print('2097152 bounds and 172032 mixed hyper-training cases passed')


if __name__ == '__main__':
    main()
