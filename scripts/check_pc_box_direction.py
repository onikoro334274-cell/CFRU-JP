"""Check constant-time PC switch direction against the previous valid-input loop."""
import json
from pathlib import Path
import re
import subprocess
import tempfile
from check_ai_difficulty import function
ROOT = Path(__file__).resolve().parent.parent


def main():
    source = (ROOT / 'src/pokemon_storage_system.c').read_text(encoding='utf-8')
    source = re.sub(r'/\*.*?\*/|//[^\n]*', '', source, flags=re.S)
    production = function(source, 'sub_8091130')
    header = (ROOT / 'include/pokemon_storage_system.h').read_text(encoding='utf-8')
    boxes = int(re.search(r'#define\s+TOTAL_BOXES_COUNT\s+(\d+)', header)[1])
    baseline = subprocess.check_output(['git', 'show', 'e943518:src/pokemon_storage_system.c'], cwd=ROOT).decode('utf-8')
    baseline = function(baseline, 'sub_8091130').replace('sub_8091130', 'previous_direction')
    fixture = r'''
typedef unsigned char u8;typedef signed char s8;typedef unsigned short u16;
#define TOTAL_BOXES_COUNT COUNT
static u8 current;static int reads;
static u8 StorageGetCurrentBox(void){reads++;return current;}
PRODUCTION
BASELINE
int main(void){
 for(int from=0;from<256;from++)for(int to=0;to<256;to++){
  current=from;reads=0;int actual=sub_8091130(to);
  if(reads!=1)return 1;
  if(from>=TOTAL_BOXES_COUNT||to>=TOTAL_BOXES_COUNT){if(actual!=1)return 2;continue;}
  /* Independent circular distance, including same-box and halfway tie behavior. */
  int distance=(to-from+TOTAL_BOXES_COUNT)%TOTAL_BOXES_COUNT;
  if(actual!=(distance<TOTAL_BOXES_COUNT/2?1:-1))return 3;
  if(actual!=previous_direction(to))return 4;
 }return 0;}
'''.replace('COUNT\n', str(boxes) + '\n').replace('PRODUCTION', production).replace('BASELINE', baseline)
    with tempfile.TemporaryDirectory() as temp:
        c, obj, exe = (Path(temp) / ('direction' + ext) for ext in ('.c', '.obj', '.exe'))
        c.write_text(fixture, encoding='utf-8')
        subprocess.run(['clang', '--target=x86_64-pc-windows-msvc', '-O2', '-fno-builtin', '-fno-stack-protector', '-c', str(c), '-o', str(obj)], check=True)
        subprocess.run(['lld-link', '/entry:main', '/subsystem:console', '/nodefaultlib', str(obj), '/out:' + str(exe)], check=True)
        subprocess.run([str(exe)], check=True)
    report = {'result': 'passed', 'byte_pairs': 65536, 'valid_baseline_pairs': boxes * boxes,
              'scope': 'Actual direction helper for every byte current/target box pair. Valid results match previous production loop and independent modulo-distance oracle; same-box/tie behavior preserved. Invalid indices return +1 without loops.',
              'optimization': 'Removes up to 49 loop iterations at current 50-box configuration; invalid target >=50 previously never matched the cyclic counter.',
              'limitations': 'Direction only; caller validation, animation and actual storage switching on linked GBA unverified.'}
    (ROOT / 'audit/pc_box_direction_review.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    print(f'65536 box-direction pairs passed; {boxes * boxes} valid baseline results preserved')


if __name__ == '__main__':
    main()
