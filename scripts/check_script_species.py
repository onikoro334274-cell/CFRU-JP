"""Check species script bounds and party/PC target routing."""
import json
from pathlib import Path
import re
import subprocess
import tempfile
from check_ai_difficulty import function
ROOT = Path(__file__).resolve().parent.parent


def main():
    def extract(source):
        return function(re.sub(r'/\*.*?\*/|//[^\n]*', '', source, flags=re.S), 'sp016_ChangePokemonSpecies')
    production = extract((ROOT / 'src/scripting.c').read_text(encoding='utf-8'))
    species = (ROOT / 'include/constants/species.h').read_text(encoding='utf-8')
    name = re.search(r'#define\s+NUM_SPECIES\s+\((\w+)\s*\+\s*1\)', species)[1]
    limit = int(re.search(r'#define\s+' + name + r'\s+(0x[0-9A-Fa-f]+|\d+)', species)[1], 0) + 1
    fixture = r'''
typedef unsigned char u8;typedef unsigned short u16;typedef unsigned int u32;
#define PARTY_SIZE 6
#define NUM_SPECIES LIMIT
#define MON_DATA_SPECIES 11
struct Pokemon{u16 species,other;};static struct Pokemon gPlayerParty[6],box;
static u16 Var8003,Var8004,Var8005;static int writes,calculations,errors;
static struct Pokemon* target(void){
#ifdef SELECT_FROM_PC
 if(Var8003==1)return &box;
#endif
 return &gPlayerParty[Var8004];
}
static void SetMonDataFromVar8003(u8 field){writes++;if(field!=11||Var8005>=NUM_SPECIES)errors++;target()->species=Var8005;}
static void CalculateMonStats(struct Pokemon*m){calculations++;if(m!=&gPlayerParty[Var8004]||writes!=1||m->species!=Var8005)errors++;
#ifdef SELECT_FROM_PC
 if(Var8003==1)errors++;
#endif
}
PRODUCTION
static void reset(void){writes=calculations=errors=0;for(int i=0;i<6;i++){gPlayerParty[i].species=17;gPlayerParty[i].other=999;}box.species=19;box.other=777;}
int main(void){
 for(Var8003=0;Var8003<2;Var8003++)for(Var8004=0;Var8004<6;Var8004++)for(u32 id=0;id<65536;id++){
  reset();Var8005=id;sp016_ChangePokemonSpecies();int valid=id<NUM_SPECIES,calc=valid;
#ifdef SELECT_FROM_PC
  calc=valid&&Var8003!=1;
#endif
  if(errors||writes!=valid||calculations!=calc||Var8005!=id)return 1;
  for(int i=0;i<6;i++)if(gPlayerParty[i].species!=((valid&&target()==&gPlayerParty[i])?id:17)||gPlayerParty[i].other!=999)return 2;
  if(box.species!=((valid&&target()==&box)?id:19)||box.other!=777)return 3;
 }
 for(Var8003=0;Var8003<2;Var8003++)for(u32 party=6;party<65536;party++){
  reset();Var8004=party;Var8005=25;sp016_ChangePokemonSpecies();if(errors||writes||calculations||Var8005!=25)return 4;
 }return 0;}
'''.replace('LIMIT\n', str(limit) + '\n').replace('PRODUCTION', production)
    def execute(code, pc):
        with tempfile.TemporaryDirectory() as temp:
            c, obj, exe = (Path(temp) / ('species' + ext) for ext in ('.c', '.obj', '.exe'))
            c.write_text(code, encoding='utf-8')
            command = ['clang', '--target=x86_64-pc-windows-msvc', '-O2', '-fno-builtin', '-fno-stack-protector']
            if pc:
                command.append('-DSELECT_FROM_PC')
            subprocess.run(command + ['-c', str(c), '-o', str(obj)], check=True)
            subprocess.run(['lld-link', '/entry:main', '/subsystem:console', '/nodefaultlib', str(obj), '/out:' + str(exe)], check=True)
            return subprocess.run([str(exe)]).returncode
    for pc in (False, True):
        assert execute(fixture, pc) == 0
    baseline = subprocess.check_output(['git', 'show', '4b08534:src/scripting.c'], cwd=ROOT).decode('utf-8')
    assert execute(fixture.replace(production, extract(baseline)), False) == 1
    report = {'result': 'passed', 'species_cases': 1572864, 'invalid_party_cases': 262120, 'species_limit': limit,
              'baseline': '4b08534 passes out-of-table species into setter/stat recalculation.',
              'scope': 'Actual species-change special for all u16 species IDs, six party indices and party/PC modes with/without PC support. Out-of-table IDs rejected; correct target only written, party-only stats recalculated, all invalid u16 party indices rejected.',
              'limitations': 'Preserves in-table IDs including existing sentinel/form entries; semantic species/form compatibility and native data/stat calculations remain unverified.'}
    (ROOT / 'audit/script_species_review.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    print('1572864 species-routing and 262120 invalid-party cases passed; old bounds failure reproduced')


if __name__ == '__main__':
    main()
