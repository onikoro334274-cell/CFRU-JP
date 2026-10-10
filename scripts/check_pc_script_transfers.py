"""Check script PC transfer bounds and conversion-before-clear routing."""
import json
from pathlib import Path
import re
import subprocess
import tempfile
from check_ai_difficulty import function
ROOT = Path(__file__).resolve().parent.parent


def main():
    def extract(source):
        clean = re.sub(r'/\*.*?\*/|//[^\n]*', '', source, flags=re.S)
        return '\n'.join(function(clean, name) for name in ['sp01A_CopyPartyData', 'sp01B_SwapPartyAndBoxData'])
    production = extract((ROOT / 'src/scripting.c').read_text(encoding='utf-8'))
    fixture = r'''
typedef unsigned char u8;typedef unsigned short u16;typedef unsigned int u32;
#define SELECT_FROM_PC
#define TOTAL_BOXES_COUNT 50
#define IN_BOX_COUNT 30
struct Pokemon{u8 data[100];};struct CompressedPokemon{u8 data[58];};
static struct Pokemon gPlayerParty[6],gEnemyParty[6],box;
static u16 Var8000,Var8001,Var8002,Var8005,gSpecialVar_LastResult;
static int sendFailed,copying,converts,sends,copies,clears,legacy,errors;
static struct Pokemon* destination;
static void Memcpy(void*out,const void*in,u32 size){
 copies++;if(Var8005>=6||size!=100){errors++;return;}
 if(Var8002==0){if(out!=&gEnemyParty[5]||in!=&gPlayerParty[Var8005])errors++;}
 else if(Var8002==1){if(out!=&gPlayerParty[Var8005]||in!=&gEnemyParty[5])errors++;}
 else errors++;
}
static u8 SendMonToBoxPos(struct Pokemon*m,u8 b,u8 p){
 sends++;if(b!=Var8000||p!=Var8001||b>=50||p>=30||m!=(copying?&gEnemyParty[5]:&gPlayerParty[Var8005]))errors++;return sendFailed;
}
static void BoxMonAtToMon(u8 b,u8 p,struct Pokemon*m){
 converts++;if(b!=Var8000||p!=Var8001||b>=50||p>=30||m!=(copying?&gEnemyParty[5]:&gPlayerParty[Var8005]))errors++;
 destination=m;for(int i=0;i<100;i++)m->data[i]=i+1;
}
static void ZeroBoxMonAt(u8 b,u8 p){
 clears++;if(b!=Var8000||p!=Var8001||converts!=1||!destination)errors++;
 if(destination)for(int i=0;i<100;i++)if(destination->data[i]!=i+1)errors++;
}
static void*GetBoxedMonAddr(void){legacy++;return &box;}
static void CalculateMonStats(struct Pokemon*m){(void)m;legacy++;}
PRODUCTION
int main(void){
 const u16 coords[]={0,29,30,49,50,255,256,65535},slots[]={0,5,6,256,65535};
 for(copying=0;copying<2;copying++)for(int b=0;b<8;b++)for(int p=0;p<8;p++)for(int s=0;s<5;s++)for(int mode=0;mode<5;mode++)for(sendFailed=0;sendFailed<2;sendFailed++){
  Var8000=coords[b];Var8001=coords[p];Var8005=slots[s];Var8002=mode;gSpecialVar_LastResult=99;
  converts=sends=copies=clears=legacy=errors=0;destination=0;
  for(int i=0;i<6;i++)for(int j=0;j<100;j++){gPlayerParty[i].data[j]=77;gEnemyParty[i].data[j]=88;}
  if(copying)sp01A_CopyPartyData();else sp01B_SwapPartyAndBoxData();
  int inBox=Var8000<50&&Var8001<30,valid=Var8005<6;
  valid=valid&&(copying?(mode<4&&(mode<2||inBox)):(mode<2&&inBox));
  int convert=valid&&(copying?mode==3:mode==0);
  int send=valid&&(copying?mode==2:mode==1);
  int copy=valid&&copying&&mode<2,clear=convert&&!copying;
  int result=valid?(send?sendFailed:0):1;
  if(errors||legacy||converts!=convert||sends!=send||copies!=copy||clears!=clear||gSpecialVar_LastResult!=result)return 1;
  for(int i=0;i<6;i++)for(int j=0;j<100;j++){
   int partyDest=convert&&!copying&&i==Var8005,enemyDest=convert&&copying&&i==5;
   if(gPlayerParty[i].data[j]!=(partyDest?j+1:77)||gEnemyParty[i].data[j]!=(enemyDest?j+1:88))return 2;
  }
 }return 0;}
'''.replace('PRODUCTION', production)
    def execute(code):
        with tempfile.TemporaryDirectory() as temp:
            c, obj, exe = (Path(temp) / ('transfer' + ext) for ext in ('.c', '.obj', '.exe'))
            c.write_text(code, encoding='utf-8')
            subprocess.run(['clang', '--target=x86_64-pc-windows-msvc', '-O2', '-fno-builtin', '-fno-stack-protector', '-c', str(c), '-o', str(obj)], check=True)
            subprocess.run(['lld-link', '/entry:main', '/subsystem:console', '/nodefaultlib', str(obj), '/out:' + str(exe)], check=True)
            return subprocess.run([str(exe)]).returncode
    assert execute(fixture) == 0
    baseline = subprocess.check_output(['git', 'show', '6d646b6:src/scripting.c'], cwd=ROOT).decode('utf-8')
    assert execute(fixture.replace(production, extract(baseline))) == 1
    report = {'result': 'passed', 'cases': 6400,
              'baseline': '6d646b6 uses legacy partial copy instead of full conversion and truncates out-of-byte coordinates.',
              'scope': 'Both actual script specials across party/coordinate boundaries including 256/65535, all modes and send failure/success. Invalid inputs call no native transfer; withdrawals dispatch full conversion before clearing source; controlled converter fills all 100 destination bytes, other slots preserved.',
              'limitations': 'Converter/compression and memory-copy services simulated. Actual compressed codec and native party stats not proven by these dispatch tests.'}
    (ROOT / 'audit/pc_script_transfers_review.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    print('6400 PC script transfer cases passed; partial-copy failure reproduced')


if __name__ == '__main__':
    main()
