"""Verify production strongest-stat selection and ordered text-pair indices."""
import argparse
import re
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
BASELINE = '61e1b61653c1c87077860400a995ff3a36b81689'


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--baseline', action='store_true')
    args = parser.parse_args()
    source = (subprocess.check_output(['git', 'show', BASELINE + ':src/hoenn_dome.c'], cwd=ROOT).decode('utf-8')
              if args.baseline else (ROOT / 'src/hoenn_dome.c').read_text(encoding='utf-8'))
    start = source.index('    // Count the number of good/bad stats for the party')
    block = source[start:source.index('    // Get the string ID to display', start)]
    for name, value in [('HP', 0), ('ATK', 5), ('DEF', 9), ('SPEED', 12), ('SPATK', 14)]:
        match = re.search(r'^#define DOME_TEXT_' + name + r'\s+(\d+)', source, re.M)
        assert match and int(match[1]) == value
    fixture = r'''
#define NUM_STATS 6
int verify(int *values) {
    short allocatedArray[18]={0};
    int i,j,k,order[6],count=0;
    for(i=0;i<6;i++) {allocatedArray[i]=values[i]; order[i]=i;}
    // Independent stable descending sort, retaining earlier stats on ties.
    for(i=1;i<6;i++) {
        int value=order[i],pos=i;
        while(pos && values[order[pos-1]]<values[value]) {
            order[pos]=order[pos-1]; pos--;
        }
        order[pos]=value;
    }
    for(i=0;i<6;i++) if(values[i]>29) count++;
BLOCK
    if(i!=(count>2?2:count)) return 1;
    if(count==1 && allocatedArray[6]!=order[0]) return 2;
    if(count>=2) {
        int first=order[0]<order[1]?order[0]:order[1];
        int second=order[0]>order[1]?order[0]:order[1];
        if(allocatedArray[6]!=first || allocatedArray[7]!=second) return 3;
        const int offsets[5]={0,5,9,12,14};
        int text=offsets[first]+second-first-1;
        if(text<0 || text>=15) return 4;
    }
    return 0;
}
int main(void) {
    int example[6]={35,30,35,0,0,0};
    if(verify(example)) return 3;
    for(int a=0;a<4;a++) for(int b=a+1;b<5;b++) for(int c=b+1;c<6;c++)
    for(int x=0;x<=100;x++) for(int y=0;y<=100-x;y++) {
        int values[6]={0}; values[a]=x; values[b]=y; values[c]=100-x-y;
        if(verify(values)) return 5;
    }
    unsigned state=42;
    for(int t=0;t<100000;t++) {
        int values[6]={0},remaining=100;
        for(int n=0;n<5;n++) {
            state=state*1664525u+1013904223u;
            values[n]=state%(remaining+1); remaining-=values[n];
        }
        values[5]=remaining;
        if(verify(values)) return 6;
    }
    return 0;
}
'''.replace('BLOCK', block)
    with tempfile.TemporaryDirectory() as directory:
        path = Path(directory)
        c, obj, exe = (path / ('labels' + suffix) for suffix in ('.c', '.obj', '.exe'))
        c.write_text(fixture, encoding='utf-8')
        subprocess.run(['clang', '--target=x86_64-pc-windows-msvc', '-O2', '-fno-builtin',
                        '-fno-stack-protector', '-c', str(c), '-o', str(obj)], check=True)
        subprocess.run(['lld-link', '/entry:main', '/subsystem:console', '/nodefaultlib',
                        '/out:' + str(exe), str(obj)], check=True)
        result = subprocess.run([str(exe)]).returncode
        assert result == (3 if args.baseline else 0), result
    print('Original wrong strongest-stat labels reproduced.' if args.baseline else
          'Strongest-stat selection and ordered pair IDs pass 103020 three-stat distributions and 100000 six-stat distributions.')


if __name__ == '__main__':
    main()
