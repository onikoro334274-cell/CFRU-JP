"""Execute production root/card allocation ownership and failure paths."""
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent


def extract(source, name):
    start = source.index(name + '(void)\n{')
    start = source.rfind('static ', 0, start)
    brace = source.index('{', start)
    depth = 1
    end = brace + 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[start:end]


def main():
    source = (ROOT / 'src/hoenn_dome.c').read_text(encoding='utf-8')
    functions = '\n'.join(extract(source, name) for name in
                          ('DomeRoot', 'FreeDomeRootIfEmpty', 'PrepareDomeInfoCard'))
    fixture = r'''
typedef unsigned char bool8;
#define NULL ((void*)0)
#define FALSE 0
#define TRUE 1
struct Card {int pos;};
struct DomeScreenRoot {struct Card *infoCard; unsigned char *tilemapBuffer;};
struct DomeScreenRoot storage,*sDomeRoot;
struct Card card;
int calls,failCall,frees;
void *AllocZeroed(unsigned long long size) {
    calls++;
    if(calls==failCall) return NULL;
    if(size==sizeof(storage)) {storage.infoCard=NULL;storage.tilemapBuffer=NULL;return &storage;}
    card.pos=0; return &card;
}
#define FREE_AND_SET_NULL(p) do {frees++;p=NULL;} while(0)
FUNCTIONS
int main(void) {
    failCall=1;
    if(PrepareDomeInfoCard() || sDomeRoot || calls!=1 || frees) return 1;
    calls=0;failCall=2;
    if(PrepareDomeInfoCard() || sDomeRoot || calls!=2 || frees!=1) return 2;
    calls=0;frees=0;failCall=0;
    if(!PrepareDomeInfoCard() || sDomeRoot!=&storage || storage.infoCard!=&card || calls!=2) return 3;
    card.pos=2;
    if(!PrepareDomeInfoCard() || calls!=2 || card.pos!=2 || frees) return 4;
    storage.infoCard=NULL;storage.tilemapBuffer=(void*)1;calls=0;failCall=1;
    if(PrepareDomeInfoCard() || sDomeRoot!=&storage || storage.tilemapBuffer!=(void*)1 || frees) return 5;
    calls=0;failCall=0;
    if(!PrepareDomeInfoCard() || storage.infoCard!=&card || calls!=1) return 6;
    return 0;
}
'''.replace('FUNCTIONS', functions)
    with tempfile.TemporaryDirectory() as directory:
        path = Path(directory)
        c, obj, exe = (path / ('allocation' + suffix) for suffix in ('.c', '.obj', '.exe'))
        c.write_text(fixture, encoding='utf-8')
        subprocess.run(['clang', '--target=x86_64-pc-windows-msvc', '-O2', '-fno-builtin',
                        '-fno-stack-protector', '-c', str(c), '-o', str(obj)], check=True)
        subprocess.run(['lld-link', '/entry:main', '/subsystem:console', '/nodefaultlib',
                        '/out:' + str(exe), str(obj)], check=True)
        subprocess.run([str(exe)], check=True)
    print('Dome root/card allocation passes failures, successful reuse, empty-root cleanup and retained tree-buffer ownership.')


if __name__ == '__main__':
    main()
