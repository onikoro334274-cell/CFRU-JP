"""Check legacy Moemon exclusion list bounds, filtering and duplicate handling."""
import hashlib
import json
from pathlib import Path
import re
import subprocess
import tempfile
from check_ai_difficulty import function

ROOT = Path(__file__).resolve().parent.parent


def main():
    path = ROOT / 'src/hack_moemon_switch.c'
    source = re.sub(r'/\*.*?\*/|//[^\n]*', '', path.read_text(encoding='utf-8'), flags=re.S)
    constants = '\n'.join(re.findall(r'^#define (?:MOEMON_OLD_(?:MAGIC\w*|MAX)|MOEMON_HIT\w*)\b[^\n]*', source, re.M))
    struct = re.search(r'struct MoemonOldList\s*\{.*?\};', source, re.S)[0]
    production = constants + '\n' + struct + '\nstatic struct MoemonOldList storage;\n#define MOEMON_OLD_LIST (&storage)\n'
    production += '\n'.join(function(source, name) for name in ['MoemonOld_Has', 'MoemonOld_Add'])
    fixture = r'''
typedef unsigned char bool8;typedef unsigned short u16;typedef unsigned int u32;
#define TRUE 1
#define FALSE 0
PRODUCTION
int main(void){
 u32 magics[4]={MOEMON_OLD_MAGIC,MOEMON_OLD_MAGIC_UPGRADE,MOEMON_OLD_MAGIC_8,0};
 for(int m=0;m<4;m++){
  storage.magic=magics[m];for(int i=0;i<384;i++)storage.personality[i]=(u32)i+1;
  for(u32 count=0;count<65536;count++){
   storage.count=(u16)count;
   if(MoemonOld_Has(384)!=(m<3&&count>=384))return 1;
   if(MoemonOld_Has(385)||MoemonOld_Has(0))return 2;
   if(storage.count!=count||storage.magic!=magics[m])return 3;
  }
 }
 for(int m=0;m<3;m++)for(u32 pid=0;pid<65536;pid++){
  storage.magic=magics[m];storage.count=0;storage.step=0x1234;
  MoemonOld_Add(pid);int hit=((pid*0x9E3779B1u)>>28)==(m==1?0xB:0xA);
  if(storage.count!=hit||storage.step!=0x1234||storage.magic!=magics[m])return 4;
  if(hit&&storage.personality[0]!=pid)return 5;
  MoemonOld_Add(pid);if(storage.count!=hit)return 6;
 }
 /* Full and damaged counts must never append, even for an eligible PID. */
 u32 eligible=0;while(((eligible*0x9E3779B1u)>>28)!=0xA)eligible++;
 for(u32 count=384;count<65536;count++){
  storage.magic=MOEMON_OLD_MAGIC;storage.count=(u16)count;
  for(int i=0;i<384;i++)storage.personality[i]=0;
  MoemonOld_Add(eligible);if(storage.count!=count)return 7;
  for(int i=0;i<384;i++)if(storage.personality[i])return 8;
 }
 storage.count=383;MoemonOld_Add(eligible);
 if(storage.count!=384||storage.personality[383]!=eligible)return 9;
 return 0;
}
'''.replace('PRODUCTION', production)
    with tempfile.TemporaryDirectory() as temp:
        c, obj, exe = (Path(temp) / ('old_list' + ext) for ext in ('.c', '.obj', '.exe'))
        c.write_text(fixture, encoding='utf-8')
        subprocess.run(['clang', '--target=x86_64-pc-windows-msvc', '-fno-builtin', '-fno-stack-protector', '-c', str(c), '-o', str(obj)], check=True)
        subprocess.run(['lld-link', '/entry:main', '/subsystem:console', '/nodefaultlib', str(obj), '/out:' + str(exe)], check=True)
        subprocess.run([str(exe)], check=True)
    report = {'source_sha256': hashlib.sha256(path.read_bytes()).hexdigest(), 'result': 'passed',
              'count_lookup_cases': 262144, 'filter_and_duplicate_cases': 196608, 'full_count_cases': 65152,
              'scope': 'Production legacy exclusion lookup/add functions: all u16 counts, three supported magic values, unknown magic lookup, PID filtering, duplicates, full/damaged count rejection and final writable slot.',
              'limitations': 'Does not verify scan sequencing, DPE consumer parity, save memory ownership, migration, or GBA runtime.'}
    (ROOT / 'audit/moemon_old_list_review.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    print('Moemon legacy list bounds, filtering, duplicate and capacity checks passed')


if __name__ == '__main__':
    main()
