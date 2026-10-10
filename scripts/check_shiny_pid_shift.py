"""Trap signed shift overflow in the actual shiny PID assignment expressions."""
import re
import subprocess
import tempfile
from pathlib import Path
import argparse
ROOT = Path(__file__).resolve().parent.parent

def main():
    parser=argparse.ArgumentParser()
    parser.add_argument('--expect-trap',action='store_true')
    args=parser.parse_args()
    source=(ROOT/'src/build_pokemon.c').read_text(encoding='utf-8')
    expressions=re.findall(r'personality = ([^;\n]*shinyRange[^;\n]*);',source)
    assert len(expressions)==2, expressions
    fixture=r'''
typedef unsigned char u8;typedef unsigned short u16;typedef unsigned int u32;
#define LOHALF(n) ((u16)(n))
static u32 calculate(unsigned which,u32 personality,u16 sid,u16 tid,u8 shinyRange){
    if(which==0)personality=EXPR0;else personality=EXPR1;
    return personality;
}
int main(void){
    const u16 halves[]={0,1,0x7fff,0x8000,0xffff};
    for(unsigned which=0;which<2;which++)for(unsigned low=0;low<65536;low++)
    for(unsigned id=0;id<5;id++)for(unsigned shiny=0;shiny<8;shiny++){
        u16 sid=halves[id],tid=0xabcd;
        u32 actual=calculate(which,0x12340000u|low,sid,tid,shiny);
        u32 expected=((u32)(shiny^sid^tid^low)*65536u)|low;
        if(actual!=expected)return 1;
    }
    return 0;
}
'''.replace('EXPR0',expressions[0]).replace('EXPR1',expressions[1])
    with tempfile.TemporaryDirectory() as directory:
        path=Path(directory);c=path/'pid.c';obj=path/'pid.obj';exe=path/'pid.exe'
        c.write_text(fixture,encoding='utf-8')
        subprocess.run(['clang','--target=x86_64-pc-windows-msvc','-O1','-fno-builtin',
            '-fno-stack-protector','-fsanitize=shift','-fsanitize-trap=shift',
            '-c',str(c),'-o',str(obj)],check=True)
        subprocess.run(['lld-link','/entry:main','/subsystem:console','/nodefaultlib',
            '/out:'+str(exe),str(obj)],check=True)
        result=subprocess.run([str(exe)])
        if args.expect_trap:
            assert result.returncode in (0xC000001D,-1073741795),result.returncode
            print('Baseline signed shiny PID shift traps as expected.')
        else:
            assert result.returncode==0,result.returncode
            print('Both shiny PID expressions: 5242880 combinations match unsigned oracle without shift traps.')

if __name__=='__main__':main()
