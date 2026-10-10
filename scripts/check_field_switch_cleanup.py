"""Verify staggered obsolete switch-item cleanup and Moemon compile variants."""
import json
from pathlib import Path
import re
import subprocess
import tempfile
from check_ai_difficulty import function

ROOT = Path(__file__).resolve().parent.parent


def main():
    functions = []
    for file, name in [('hack_moemon_switch.c', 'HackMoemonSwitch_FieldTick'),
                       ('hack_habitat_switch.c', 'HackHabitatSwitch_FieldTick')]:
        source = (ROOT / 'src' / file).read_text(encoding='utf-8')
        source = re.sub(r'/\*.*?\*/|//[^\n]*', '', source, flags=re.S)
        functions.append(function(source, name))
    fixture = r'''
typedef unsigned char u8;typedef unsigned int u32;
#define ITEM_MOEMON_SWITCH 1
#define ITEM_HABITAT_SWITCH 2
static struct {u32 vblankCounter2;} gMain;
static int present,checks[3],removed[3],scans,marks,errors;
static void MoemonOld_ScanStep(void){scans++;}
static void MoemonLink_MarkParty(void){marks++;}
static int CheckBagHasItem(int item,int n){if(item<1||item>2||n!=1)errors++;checks[item]++;return present;}
static int RemoveBagItem(int item,int n){if(!present||item<1||item>2||n!=1)errors++;removed[item]++;return 1;}
PRODUCTION
int main(void){
 for(u32 tick=0;tick<256;tick++)for(present=0;present<2;present++){
  gMain.vblankCounter2=tick;checks[1]=checks[2]=removed[1]=removed[2]=scans=marks=errors=0;
  HackMoemonSwitch_FieldTick();HackHabitatSwitch_FieldTick();
  int moe=(tick%64)==32,hab=(tick%64)==48;
  if(errors||checks[1]!=moe||checks[2]!=hab||removed[1]!=moe*present||removed[2]!=hab*present)return 1;
 #ifdef HACK_NO_MOEMON
  if(scans||marks)return 2;
 #else
  if(scans!=1||marks!=((tick%16)==0))return 3;
 #endif
 }return 0;}
'''.replace('PRODUCTION', '\n'.join(functions))
    with tempfile.TemporaryDirectory() as temp:
        c, obj, exe = (Path(temp) / ('cleanup' + ext) for ext in ('.c', '.obj', '.exe'))
        c.write_text(fixture, encoding='utf-8')
        for flags in [[], ['-DHACK_NO_MOEMON']]:
            subprocess.run(['clang', '--target=x86_64-pc-windows-msvc', '-fno-builtin', '-fno-stack-protector', *flags, '-c', str(c), '-o', str(obj)], check=True)
            subprocess.run(['lld-link', '/entry:main', '/subsystem:console', '/nodefaultlib', str(obj), '/out:' + str(exe)], check=True)
            subprocess.run([str(exe)], check=True)
    report = {'result': 'passed', 'cases': 1024,
              'scope': 'Production field ticks: staggered bag queries/removals at phases 32/48 per 64 frames; normal vs HACK_NO_MOEMON scan/link marking behavior.',
              'limitations': 'Controlled bag and scan APIs; old-list scan, link timing, repeated-frame calls, bag mutations and GBA execution not verified.'}
    (ROOT / 'audit/field_switch_cleanup_review.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    print('1024 switch cleanup/frame/compile-variant cases passed')


if __name__ == '__main__':
    main()
