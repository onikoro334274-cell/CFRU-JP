"""Verify ID generation preserves shiny status and legacy Moemon appearance."""
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
    source = re.sub(r'/\*.*?\*/|//[^\n]*', '', path.read_text(encoding='utf-8'), flags=re.S)
    production = '\n'.join(re.findall(r'^#define (?:SHINY_THRESHOLD|MOEMON_\w+)\b[^\n]*', source, re.M))
    production += '\n' + '\n'.join(function(source, name) for name in ['PickSid', 'MakeId'])
    # Independent enumeration proves every legacy artwork class has a 16-bit SID.
    classes = {((sid * 0x85EBCA6B) & 0xFFFFFFFF) >> 16 for sid in range(65536)
               if ((sid * 0x9E3779B1) & 0xFFFFFFFF) >> 28 == 10}
    residues = {value % 60 for value in classes}
    if residues != set(range(60)):
        raise RuntimeError('Some legacy artwork classes cannot be represented')
    fixture = r'''
typedef unsigned char bool8;typedef unsigned int u32;
static u32 rng=0x12345678u;static int old;
static unsigned short Random(void){rng=rng*1664525u+1013904223u;return rng>>16;}
static bool8 HackMoemonSwitch_IsOldMon(u32 pid){(void)pid;return old;}
PRODUCTION
static int hit(u32 v){return ((v*0x9E3779B1u)>>28)==10;}
static u32 art(u32 v){return ((v*0x85EBCA6Bu)>>16)%60;}
int main(void){
 for(u32 i=0;i<65536;i++)for(old=0;old<2;old++)for(int shiny=0;shiny<2;shiny++){
  u32 pid=i*0x9E3779B1u;u32 id=MakeId(shiny,pid),tid=id&65535,sid=id>>16;
  if(!id||((tid^sid)<8)!=shiny)return 1;
  int expected=hit(pid)&&!old;
  if(hit(sid)!=expected)return 2;
  if(expected&&art(sid)!=art(pid))return 3;
 }
 return 0;
}
'''.replace('PRODUCTION', production)
    with tempfile.TemporaryDirectory() as temp:
        c, obj, exe = (Path(temp) / ('generation' + ext) for ext in ('.c', '.obj', '.exe'))
        c.write_text(fixture, encoding='utf-8')
        subprocess.run(['clang', '--target=x86_64-pc-windows-msvc', '-O2', '-fno-builtin', '-fno-stack-protector', '-c', str(c), '-o', str(obj)], check=True)
        subprocess.run(['lld-link', '/entry:main', '/subsystem:console', '/nodefaultlib', str(obj), '/out:' + str(exe)], check=True)
        subprocess.run([str(exe)], check=True)
    report = {'source_sha256': hashlib.sha256(path.read_bytes()).hexdigest(), 'result': 'passed',
              'generation_cases': 262144, 'representable_art_classes': 60,
              'scope': 'Actual PickSid/MakeId with seeded RNG: nonzero ID, shiny preservation, legacy exclusion and artwork class modulo 60; all 16-bit SIDs independently enumerated for class coverage.',
              'limitations': 'Does not prove all personalities/RNG sequences, actual game RNG parity, forced Moemon conversion, full callers, or GBA execution.'}
    (ROOT / 'audit/mon_id_generation_review.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    print('262144 individual ID generation cases passed; all 60 artwork classes representable')


if __name__ == '__main__':
    main()
