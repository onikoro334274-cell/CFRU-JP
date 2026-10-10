"""Verify individual ID bit packing without overwriting other compressed fields."""
from pathlib import Path
import hashlib
import json
import re
import subprocess
import tempfile
from check_ai_difficulty import function

ROOT = Path(__file__).resolve().parent.parent


def main():
    path = ROOT / 'src/hack_mon_id.c'
    source = path.read_text(encoding='utf-8')
    source = re.sub(r'/\*.*?\*/|//[^\n]*', '', source, flags=re.S)
    names = ['Rd16', 'Rd32', 'Wr16', 'Wr32', 'HackMonId_UnpackComp', 'HackMonId_PackComp', 'HackMonId_ClearSpare']
    production = '\n'.join(re.findall(r'^#define (?:OFF_\w+|[CB]_\w+)\b[^\n]*', source, re.M))
    production += '\n' + '\n'.join(function(source, name) for name in names)
    fixture = r'''
typedef unsigned char u8;typedef unsigned int u32;
struct BoxPokemon {u8 bytes[80];};
PRODUCTION
static u32 rng=0xADBEEF12u;
static u32 next(void){rng^=rng<<13;rng^=rng>>17;rng^=rng<<5;return rng;}
int main(void){
 u8 mask[80];for(int j=0;j<80;j++)mask[j]=0;
 /* Independently enumerate the compressed ID bit carriers. */
 mask[0x1F]=0xFC;mask[0x23]=0xF8;mask[0x1B]=0xF0;mask[0x12]=0xF8;
 mask[0x35]=0x60;mask[0x13]=0xF0;mask[0x11]=0xE0;mask[0x26]=0xE0;
 for(u32 sample=0;sample<200034;sample++){
  u8 buf[80],before[80];for(int j=0;j<80;j++)buf[j]=before[j]=(u8)next();
  u32 id=sample<32?(1u<<sample):sample==32?0:sample==33?0xFFFFFFFFu:next();
  HackMonId_PackComp(buf,id);
  if(HackMonId_UnpackComp(buf)!=id)return 1;
  for(int j=0;j<80;j++)if(((buf[j]^before[j])&~mask[j])!=0)return 2;
  HackMonId_PackComp(buf,0);
  for(int j=0;j<80;j++)if(buf[j]!=(before[j]&~mask[j]))return 3;
 }
 /* Clear spare carriers in the expanded layout, preserving the ID at 0x3E. */
 for(int j=0;j<80;j++)mask[j]=0;
 mask[0x23]=0xFC;mask[0x27]=0xF8;mask[0x1B]=0xF0;mask[0x12]=0xF8;
 mask[0x47]=0x60;mask[0x13]=0xF0;mask[0x11]=0xE0;mask[0x2A]=0xE0;
 for(int sample=0;sample<4096;sample++){
  struct BoxPokemon box;u8 before[80];for(int j=0;j<80;j++)box.bytes[j]=before[j]=(u8)next();
  HackMonId_ClearSpare(&box);
  for(int j=0;j<80;j++)if(box.bytes[j]!=(before[j]&~mask[j]))return 4;
 }
 return 0;
}
'''.replace('PRODUCTION', production)
    with tempfile.TemporaryDirectory() as temp:
        c, obj, exe = (Path(temp) / ('packing' + ext) for ext in ('.c', '.obj', '.exe'))
        c.write_text(fixture, encoding='utf-8')
        subprocess.run(['clang', '--target=x86_64-pc-windows-msvc', '-fno-builtin', '-fno-stack-protector', '-c', str(c), '-o', str(obj)], check=True)
        subprocess.run(['lld-link', '/entry:main', '/subsystem:console', '/nodefaultlib', str(obj), '/out:' + str(exe)], check=True)
        subprocess.run([str(exe)], check=True)
    report = {'source_sha256': hashlib.sha256(path.read_bytes()).hexdigest(),
              'packing_cases': 200034, 'expanded_clear_cases': 4096, 'result': 'passed',
              'scope': 'Actual production pack/unpack/clear functions; all ID bits, zero, maximum, seeded random IDs, unrelated bits and buffer tail preserved.',
              'limitations': 'Does not verify full struct conversion, encryption, save migration, domain limits for item/experience/type/ball, or GBA runtime.'}
    (ROOT / 'audit/mon_id_packing_review.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    print('Individual ID: 200034 packing and 4096 expanded clear cases passed')


if __name__ == '__main__':
    main()
