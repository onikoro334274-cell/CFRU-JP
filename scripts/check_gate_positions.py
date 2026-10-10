"""Execute production gate placement over signed player/camera coordinates."""
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent


def main():
    source = (ROOT / 'src/hoenn_gates.c').read_text(encoding='utf-8')
    start = source.index('static void HoennGatePlace(')
    function = source[start:source.index('/* 画面の外へ出た門', start)]
    fixture = r'''
typedef short s16;
#define HOENN_MAP_OFFSET 7
struct HoennGateSpot {s16 x,y;} sHoennGateSpots[2]={{4,9},{30,2}};
struct Sprite {s16 data[16];struct {s16 x,y;} pos1;};
struct {struct {s16 x,y;} pos;} save,*gSaveBlock1=&save;
s16 gTotalCameraPixelOffsetX,gTotalCameraPixelOffsetY;
FUNCTION
int main(void) {
    struct Sprite sprite;
    int cameras[7]={-32768,-256,-1,0,1,256,32767};
    for(int gate=0;gate<2;gate++) for(int value=-32768;value<=32767;value++)
    for(int camera=0;camera<7;camera++) {
        sprite.data[0]=gate;save.pos.x=value;save.pos.y=-1-value;
        gTotalCameraPixelOffsetX=cameras[camera];gTotalCameraPixelOffsetY=-1-cameras[camera];
        HoennGatePlace(&sprite);
        int x=(sHoennGateSpots[gate].x+7-value)*16-cameras[camera];
        int y=(sHoennGateSpots[gate].y+7-(-1-value))*16-(-1-cameras[camera]);
        if(sprite.pos1.x!=(s16)x || sprite.pos1.y!=(s16)y) return 1;
    }
    return 0;
}
'''.replace('FUNCTION', function)
    with tempfile.TemporaryDirectory() as directory:
        path = Path(directory)
        c, obj, exe = (path / ('gates' + suffix) for suffix in ('.c', '.obj', '.exe'))
        c.write_text(fixture, encoding='utf-8')
        subprocess.run(['clang', '--target=x86_64-pc-windows-msvc', '-O2', '-fno-builtin',
                        '-fno-stack-protector', '-c', str(c), '-o', str(obj)], check=True)
        subprocess.run(['lld-link', '/entry:main', '/subsystem:console', '/nodefaultlib',
                        '/out:' + str(exe), str(obj)], check=True)
        subprocess.run([str(exe)], check=True)
    print('Gate placement passes 917504 signed player-position/camera cases, including negative coordinate differences.')


if __name__ == '__main__':
    main()
