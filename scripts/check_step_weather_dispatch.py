"""Execute the actual step detector and weather dispatch from HoennStepTick."""
import re
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent


def main():
    source = (ROOT / 'src/hoenn_steps.c').read_text(encoding='utf-8')
    start = source.index('    hold = (u8)')
    end = source.index('HoennAbnormalWeatherTick(moved);', start) + len('HoennAbnormalWeatherTick(moved);')
    block = source[start:end]
    defines = '\n'.join(re.findall(r'^#define STEP_(?:TIMER|LASTX|LASTY)\s+\d+', source, re.M))
    fixture = r'''
typedef unsigned char u8;
typedef short s16;
struct Task {s16 data[16];} storage;
int currentX,currentY,calls,seen;
void PlayerGetDestCoords(s16 *x,s16 *y) {*x=currentX;*y=currentY;}
u8 MapGridGetMetatileBehaviorAt(s16 x,s16 y) {return 0;}
void HoennAbnormalWeatherTick(u8 moved) {calls++;seen=moved;}
DEFINES
void dispatch(void) {
    struct Task *task=&storage;
    u8 hold,moved,beh;
    s16 px,py;
BLOCK
}
int main(void) {
    for(int x=7;x<71;x++) for(int y=7;y<71;y++) {
        for(int n=0;n<16;n++) storage.data[n]=0;
        currentX=x;currentY=y;calls=0;
        dispatch();
        if(calls!=1 || seen) return 1;
        dispatch();
        if(calls!=2 || seen) return 2;
        currentX++;storage.data[STEP_TIMER]=8;
        dispatch();
        if(calls!=3 || !seen || storage.data[STEP_TIMER]!=7) return 3;
        dispatch();
        if(calls!=4 || seen || storage.data[STEP_TIMER]!=6) return 4;
        currentY++;dispatch();
        if(calls!=5 || !seen || storage.data[STEP_LASTX]!=currentX
           || storage.data[STEP_LASTY]!=currentY) return 5;
    }
    return 0;
}
'''.replace('DEFINES', defines).replace('BLOCK', block)
    with tempfile.TemporaryDirectory() as directory:
        path = Path(directory)
        c, obj, exe = (path / ('steps' + suffix) for suffix in ('.c', '.obj', '.exe'))
        c.write_text(fixture, encoding='utf-8')
        subprocess.run(['clang', '--target=x86_64-pc-windows-msvc', '-O2', '-fno-builtin',
                        '-fno-stack-protector', '-c', str(c), '-o', str(obj)], check=True)
        subprocess.run(['lld-link', '/entry:main', '/subsystem:console', '/nodefaultlib',
                        '/out:' + str(exe), str(obj)], check=True)
        subprocess.run([str(exe)], check=True)
    print('Step/weather dispatch passes 20480 frames across padded-map coordinates, initial/stationary frames and X/Y movement.')


if __name__ == '__main__':
    main()
