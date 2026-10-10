"""Check PC send rejection happens before storage writes and metadata updates."""
import json
from pathlib import Path
import re
import subprocess
import tempfile
from check_ai_difficulty import function
ROOT = Path(__file__).resolve().parent.parent


def main():
    def extract(source):
        return function(re.sub(r'/\*.*?\*/|//[^\n]*', '', source, flags=re.S), 'SendMonToBoxPos')
    production = extract((ROOT / 'src/pokemon_storage_system.c').read_text(encoding='utf-8'))
    header = (ROOT / 'include/pokemon_storage_system.h').read_text(encoding='utf-8')
    def value(name):
        return int(re.search(r'#define\s+' + name + r'\s+(\d+)', header)[1])
    boxes, slots = value('TOTAL_BOXES_COUNT'), value('IN_BOX_ROWS') * value('IN_BOX_COLUMNS')
    fixture = r'''
typedef unsigned char u8;typedef unsigned short u16;typedef u8 bool8;
#define TRUE 1
#define FALSE 0
#define TOTAL_BOXES_COUNT BOXES
#define IN_BOX_COUNT SLOTS
#define FLAG_SHOWN_BOX_WAS_FULL_MESSAGE 7
#define VAR_PC_BOX_TO_SEND_MON 8
struct Pokemon{u8 unused;};struct BoxPokemon{u8 unused;};struct CompressedPokemon{u8 unused;};
static struct Pokemon mon;static struct CompressedPokemon stored;
static u16 gSpecialVar_MonBoxId,gSpecialVar_MonBoxPos;
static int box,pos,current,locates,creates,clears,sets,errors;
static struct CompressedPokemon* MonPtrForWrite(u8 b,u8 p){locates++;if(b!=box||p!=pos||b>=BOXES||p>=SLOTS)errors++;return &stored;}
static void CreateCompressedMonFromBoxMon(struct BoxPokemon*m,struct CompressedPokemon*out){creates++;if(m!=(struct BoxPokemon*)&mon||out!=&stored||locates!=1)errors++;}
static u8 GetPCBoxToSendMon(void){return current;}
static void FlagClear(u16 flag){clears++;if(flag!=7)errors++;}
static void VarSet(u16 id,u16 b){sets++;if(id!=8||b!=box||creates!=1)errors++;}
PRODUCTION
int main(void){
 for(box=0;box<256;box++)for(pos=0;pos<256;pos++)for(int same=0;same<2;same++){
  current=same?box:(box+1)%256;locates=creates=clears=sets=errors=0;
  gSpecialVar_MonBoxId=777;gSpecialVar_MonBoxPos=888;
  int valid=box<BOXES&&pos<SLOTS;int result=SendMonToBoxPos(&mon,box,pos);
  if(errors||result!=!valid||locates!=valid||creates!=valid||sets!=valid||clears!=(valid&&!same))return 1;
  if(gSpecialVar_MonBoxId!=(valid?box:777)||gSpecialVar_MonBoxPos!=(valid?pos:888))return 2;
 }return 0;}
'''
    fixture = re.sub(r'\bBOXES\b', str(boxes), fixture)
    fixture = re.sub(r'\bSLOTS\b', str(slots), fixture).replace('PRODUCTION', production)
    def execute(code):
        with tempfile.TemporaryDirectory() as temp:
            c, obj, exe = (Path(temp) / ('send' + ext) for ext in ('.c', '.obj', '.exe'))
            c.write_text(code, encoding='utf-8')
            subprocess.run(['clang', '--target=x86_64-pc-windows-msvc', '-O2', '-fno-builtin', '-fno-stack-protector', '-c', str(c), '-o', str(obj)], check=True)
            subprocess.run(['lld-link', '/entry:main', '/subsystem:console', '/nodefaultlib', str(obj), '/out:' + str(exe)], check=True)
            return subprocess.run([str(exe)]).returncode
    assert execute(fixture) == 0
    baseline = subprocess.check_output(['git', 'show', 'c151e48:src/pokemon_storage_system.c'], cwd=ROOT).decode('utf-8')
    assert execute(fixture.replace(production, extract(baseline))) == 1
    report = {'result': 'passed', 'cases': 131072, 'boxes': boxes, 'slots_per_box': slots,
              'baseline': 'c151e48 dispatches storage writes at invalid one-past-end coordinates.',
              'scope': 'Actual SendMonToBoxPos for all byte coordinates with same/different current send box; invalid coordinates rejected before locator/compression/metadata mutation, valid sends route correct destination and update metadata/flag.',
              'limitations': 'Storage locator/compression services simulated; virtual shelf layout, actual persistence and linked GBA runtime unverified.'}
    (ROOT / 'audit/pc_send_bounds_review.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    print('131072 PC send bounds/metadata cases passed; old boundary failure reproduced')


if __name__ == '__main__':
    main()
