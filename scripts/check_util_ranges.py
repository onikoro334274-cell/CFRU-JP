"""Execute production scalar utilities with controlled RNG boundary cases."""
import subprocess
import tempfile
from pathlib import Path
from check_factory_party_copy import function

ROOT = Path(__file__).resolve().parent.parent

def main():
    source = (ROOT / 'src/util.c').read_text(encoding='utf-8')
    production = '\n'.join(function(source, signature) for signature in (
        'u32 MathMax(u32 num1, u32 num2)',
        'u32 MathMin(u32 num1, u32 num2)',
        'u16 RandRange(u16 min, u16 max)',
    ))
    fixture = r'''
typedef unsigned short u16;typedef unsigned int u32;
static u16 randomValue;static unsigned calls;
static u16 Random(void){calls++;return randomValue;}
PRODUCTION
int main(void){
    const u32 values[]={0,1,2,0x7fffffff,0x80000000,0xfffffffe,0xffffffff};
    for(unsigned i=0;i<7;i++)for(unsigned j=0;j<7;j++){
        u32 a=values[i],b=values[j];
        if(MathMin(a,b)!=(a<b?a:b)||MathMax(a,b)!=(a>b?a:b))return 1;
    }
    for(unsigned n=0;n<65536;n++){
        calls=0;randomValue=n;
        if(RandRange(n,n)!=n||calls!=0)return 2;
    }
    const u16 ranges[][2]={{0,1},{0,2},{0,3},{0,6},{0,17},{0,65535},
        {1,65535},{32767,32768},{32768,65535},{65534,65535},
        {2,5},{15,20},{25,30},{35,40},{50,55},{60,65},{75,90}};
    for(unsigned p=0;p<sizeof(ranges)/sizeof(ranges[0]);p++){
        unsigned low=ranges[p][0],high=ranges[p][1];
        for(unsigned r=0;r<65536;r++){
            randomValue=r;calls=0;unsigned actual=RandRange(low,high);
            if(actual<low||actual>=high||actual!=low+r%(high-low)||calls!=1)return 3;
        }
    }
    return 0;
}
'''.replace('PRODUCTION', production)
    with tempfile.TemporaryDirectory() as directory:
        path = Path(directory)
        c, obj, exe = (path / ('util_ranges' + suffix) for suffix in ('.c', '.obj', '.exe'))
        c.write_text(fixture, encoding='utf-8')
        subprocess.run(['clang', '--target=x86_64-pc-windows-msvc', '-O2',
            '-fno-builtin', '-fno-stack-protector', '-fsanitize=integer-divide-by-zero',
            '-fsanitize-trap=integer-divide-by-zero', '-c', str(c), '-o', str(obj)], check=True)
        subprocess.run(['lld-link', '/entry:main', '/subsystem:console', '/nodefaultlib',
            '/out:' + str(exe), str(obj)], check=True)
        subprocess.run([str(exe)], check=True)
    print('Utilities: 49 unsigned min/max pairs, 65536 singleton ranges and 1114112 controlled RNG cases pass; upper bound is exclusive.')

if __name__ == '__main__':
    main()
