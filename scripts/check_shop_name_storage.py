"""Validate shop name slot writes and the matching ARM allocation stride."""
import subprocess
import tempfile
from pathlib import Path
from check_mega_shop_escalation import function

ROOT = Path(__file__).resolve().parent.parent


def main():
    source = (ROOT / 'src/mega_shop.c').read_text(encoding='utf-8')
    production = function(source, 'struct ListMenuItem *Shop_BuildListStrings(void)')
    assembly = (ROOT / 'assembly/hooks/general_hooks.s').read_text(encoding='utf-8')
    allocation = assembly.split('RemoveShopModifierCheats:', 1)[1].split('FreeShopMemoryReturn:', 1)[0]
    assert 'lsl r0, r1, #0x4' in allocation
    assert 'beq FreeShopMemoryReturn' in allocation
    fixture = r'''
typedef unsigned char u8;typedef unsigned short u16;typedef unsigned int u32;
struct ListMenuItem {const u8* name;int id;};
static struct ListMenuItem entries[257];
static struct ListMenuItem *sShopMenuListMenu=entries;
static u8 buffer[257*16+2];static u8 *sShopMenuItemStrings=buffer+1;
static u16 items[256];
static struct {u16 itemCount;const u16* itemList;} gShopData;
static const u8 gText_ShopQuit[]={0xA1,0xA2,0xFF};
#define SHOP_NAME_STRIDE 16
static void PokeMartWriteNameAndIdAt(struct ListMenuItem* entry,u16 item,u8* dest) {
    for(u32 i=0;i<9;i++) dest[i]=(u8)(item+i);
    dest[9]=0xFF;entry->name=dest;entry->id=item;
}
static void StringCopy(u8* dest,const u8* source) {
    do {*dest++=*source;} while(*source++!=0xFF);
}
PRODUCTION
int main(void) {
    for(u32 count=0;count<=256;count++) {
        for(u32 i=0;i<sizeof(buffer);i++) buffer[i]=0xCC;
        for(u32 i=0;i<count;i++) items[i]=i+1;
        gShopData.itemCount=count;gShopData.itemList=items;
        if(Shop_BuildListStrings()!=entries) return 1;
        if(buffer[0]!=0xCC || buffer[(count+1)*16+1]!=0xCC) return 2;
        for(u32 i=0;i<count;i++) {
            if(entries[i].name!=buffer+1+i*16 || entries[i].id!=(int)i+1) return 3;
            for(u32 j=0;j<9;j++) if(entries[i].name[j]!=(u8)(items[i]+j)) return 4;
            if(entries[i].name[9]!=0xFF) return 5;
            for(u32 j=10;j<16;j++) if(entries[i].name[j]!=0xCC) return 6;
        }
        if(entries[count].id!=-2 || entries[count].name!=buffer+1+count*16) return 7;
        for(u32 j=0;j<3;j++) if(entries[count].name[j]!=gText_ShopQuit[j]) return 8;
    }
    return 0;
}
'''.replace('PRODUCTION', production)
    with tempfile.TemporaryDirectory() as directory:
        path = Path(directory)
        c, obj, exe = (path / ('shop_names' + suffix) for suffix in ('.c', '.obj', '.exe'))
        c.write_text(fixture, encoding='utf-8')
        subprocess.run(['clang', '--target=x86_64-pc-windows-msvc', '-O2', '-fno-builtin',
                        '-fno-stack-protector', '-c', str(c), '-o', str(obj)], check=True)
        subprocess.run(['lld-link', '/entry:main', '/subsystem:console', '/nodefaultlib',
                        '/out:' + str(exe), str(obj)], check=True)
        subprocess.run([str(exe)], check=True)
    print('Shop names: counts 0..256 passed; slot boundaries, terminators, IDs and cancel entry preserved.')


if __name__ == '__main__':
    main()
