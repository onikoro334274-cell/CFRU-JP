"""Check temporary party backup/restore bounds and per-slot restore ordering."""
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
        return '\n'.join(function(clean, name) for name in ['BackupPartyToTempTeam', 'RestorePartyFromTempTeam'])
    production = extract((ROOT / 'src/pokemon_storage_system.c').read_text(encoding='utf-8'))
    fixture = r'''
typedef unsigned char u8;typedef unsigned int u32;
#define PARTY_SIZE 6
struct Pokemon{u8 data[100];};struct BoxPokemon{u8 data[80];};struct CompressedPokemon{u8 data[58];};
static struct Pokemon gPlayerParty[6];static struct CompressedPokemon gTempTeamBackup[6];
static int first,count,restoring,calls,clears,stats,errors;static int stage[6];
static int indexOf(void*p,void*base,int size){long long diff=(u8*)p-(u8*)base;int i=diff/size;if(diff<0||diff%size||i>=6){errors++;return -1;}return i;}
static void CreateCompressedMonFromBoxMon(struct BoxPokemon*m,struct CompressedPokemon*out){
 calls++;int i=indexOf(m,gPlayerParty,100),j=indexOf(out,gTempTeamBackup,58);
 if(i<0||j<0)return;if(restoring||i!=j||i<first||i>=first+count)errors++;out->data[0]=m->data[0];
}
static void Memset(void*p,int value,u32 size){
 clears++;int i=indexOf(p,gPlayerParty,100);if(i<0)return;
 if(!restoring||value||size!=100||stage[i]++)errors++;for(int j=0;j<100;j++)gPlayerParty[i].data[j]=0;
}
static void CreateBoxMonFromCompressedMon(struct BoxPokemon*m,struct CompressedPokemon*in){
 calls++;int i=indexOf(m,gPlayerParty,100),j=indexOf(in,gTempTeamBackup,58);if(i<0||j<0)return;
 if(!restoring||i!=j||stage[i]++!=1)errors++;
 for(int k=0;k<100;k++)if(gPlayerParty[i].data[k])errors++;
 gPlayerParty[i].data[0]=in->data[0];
}
static void CalculateMonStats(struct Pokemon*m){stats++;int i=indexOf(m,gPlayerParty,100);if(i<0)return;if(stage[i]++!=2)errors++;m->data[99]=222;}
PRODUCTION
int main(void){
 for(restoring=0;restoring<2;restoring++)for(first=0;first<256;first++)for(count=0;count<256;count++){
  calls=clears=stats=errors=0;for(int i=0;i<6;i++){stage[i]=0;for(int j=0;j<100;j++)gPlayerParty[i].data[j]=77+i;for(int j=0;j<58;j++)gTempTeamBackup[i].data[j]=33+i;}
  if(restoring)RestorePartyFromTempTeam(first,count);else BackupPartyToTempTeam(first,count);
  int valid=first<6&&count<=6-first,n=valid?count:0;
  if(errors||calls!=n||clears!=(restoring?n:0)||stats!=(restoring?n:0))return 1;
  for(int i=0;i<6;i++){
   int selected=valid&&i>=first&&i<first+count;
   for(int j=0;j<100;j++){
    int expected=selected&&restoring?(j==0?33+i:j==99?222:0):77+i;
    if(gPlayerParty[i].data[j]!=expected)return 2;
   }
   for(int j=0;j<58;j++)if(gTempTeamBackup[i].data[j]!=(selected&&!restoring&&j==0?77+i:33+i))return 3;
  }
 }return 0;}
'''.replace('PRODUCTION', production)
    def execute(code):
        with tempfile.TemporaryDirectory() as temp:
            c, obj, exe = (Path(temp) / ('backup' + ext) for ext in ('.c', '.obj', '.exe'))
            c.write_text(code, encoding='utf-8')
            subprocess.run(['clang', '--target=x86_64-pc-windows-msvc', '-O2', '-fno-builtin', '-fno-stack-protector', '-c', str(c), '-o', str(obj)], check=True)
            subprocess.run(['lld-link', '/entry:main', '/subsystem:console', '/nodefaultlib', str(obj), '/out:' + str(exe)], check=True)
            return subprocess.run([str(exe)]).returncode
    assert execute(fixture) == 0
    baseline = subprocess.check_output(['git', 'show', 'b89c042:src/pokemon_storage_system.c'], cwd=ROOT).decode('utf-8')
    assert execute(fixture.replace(production, extract(baseline))) == 1
    report = {'result': 'passed', 'range_cases': 131072,
              'baseline': 'b89c042 permits first/count ranges beyond six-entry party and backup arrays.',
              'scope': 'Production backup/restore for every byte first index/count. Rejects invalid ranges before any service, valid ranges select exactly intended slots. Restore wipes entire party record before expansion and stats; other party slots and backup bytes preserved.',
              'limitations': 'Controlled compression and stats services. Codec fidelity, actual HP/status reconstruction and linked GBA runtime unverified.'}
    (ROOT / 'audit/temp_team_ranges_review.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    print('131072 temporary-team range/order cases passed; old range failure reproduced')


if __name__ == '__main__':
    main()
