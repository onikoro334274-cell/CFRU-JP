"""Check shop tile allocation and execute frame remapping with guarded data."""
import re
import subprocess
import tempfile
from pathlib import Path
from check_mega_shop_escalation import function

ROOT = Path(__file__).resolve().parent.parent


def main():
    source = (ROOT / 'src/mega_shop.c').read_text(encoding='utf-8')
    windows = source.split('static const struct WindowTemplate sMegaShopWindows[] =', 1)[1].split('};', 1)[0]
    occupied = set()
    count = 0
    for row in re.findall(r'\{([^{}]+)\}', windows):
        bg, left, top, width, height, palette, base = [int(value.strip(), 0) for value in row.split(',')]
        assert bg == 0 and palette < 16
        assert left + width <= 30 and top + height <= 20
        tiles = set(range(base, base + width * height))
        assert not occupied.intersection(tiles), 'Window tile allocations overlap'
        assert min(tiles) >= 0x27 and max(tiles) < 1024
        occupied.update(tiles)
        count += 1
    assert count == 7
    production = function(source, 'void MegaShop_LoadFrameTilemap(void)')
    fixture = r'''
typedef unsigned short u16;typedef unsigned int u32;
static u16 guarded[1026];static u16 *sShopTilemapBuffer1=guarded+1;
static int open;
#define gBuyMenuFrame_Tilemap ((const void*)0)
static int MegaShop_IsOpen(void) {return open;}
static void LZDecompressWram(const void* ignored,u16* dest) {
    (void)ignored;for(u32 i=0;i<1024;i++) dest[i]=(u16)(i+1);
}
PRODUCTION
int main(void) {
    for(open=0;open<=1;open++) {
        guarded[0]=0xCAFE;guarded[1025]=0xBEEF;
        MegaShop_LoadFrameTilemap();
        if(guarded[0]!=0xCAFE || guarded[1025]!=0xBEEF) return 1;
        for(u32 row=0;row<32;row++) for(u32 col=0;col<32;col++) {
            u16 expected=row*32+col+1;
            if(open && row<14 && col<30) {
                if(row<4) expected=col<10 ? expected : 0;
                else {
                    u32 fromRow=row==4 ? 0 : row==13 ? 13 : row-4;
                    u32 fromCol=col==0 ? 10 : col==1 ? 11 : col>=28 ? col : 15;
                    expected=fromRow*32+fromCol+1;
                }
            }
            if(sShopTilemapBuffer1[row*32+col]!=expected) return 2;
        }
    }
    return 0;
}
'''.replace('PRODUCTION', production)
    with tempfile.TemporaryDirectory() as directory:
        path = Path(directory)
        c, obj, exe = (path / ('shop_layout' + suffix) for suffix in ('.c', '.obj', '.exe'))
        c.write_text(fixture, encoding='utf-8')
        subprocess.run(['clang', '--target=x86_64-pc-windows-msvc', '-O2', '-fno-builtin',
                        '-fno-stack-protector', '-c', str(c), '-o', str(obj)], check=True)
        subprocess.run(['lld-link', '/entry:main', '/subsystem:console', '/nodefaultlib',
                        '/out:' + str(exe), str(obj)], check=True)
        subprocess.run([str(exe)], check=True)
    print('Shop layout: seven disjoint tile allocations and 2048 open/closed frame cells passed.')


if __name__ == '__main__':
    main()
