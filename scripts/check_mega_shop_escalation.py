"""Exercise actual escalating shop prices for every saved counter value."""
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent


def function(source, signature):
    start = source.index(signature)
    opening = source.index('{', start)
    depth = 1
    end = opening + 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return signature + '\n' + source[opening:end]


def main():
    source = (ROOT / 'src/mega_shop.c').read_text(encoding='utf-8')
    production = '\n'.join(function(source, signature) for signature in (
        'static u8 BoughtShift(u16 item)',
        'static u16 DangerLeafBought(u16 item)',
        'static u32 DangerLeafPrice(u16 item, u32 base)',
        'bool8 MegaShop_AfterPurchase(u16 item)',
    ))
    fixture = r'''
typedef unsigned char u8; typedef unsigned short u16;
typedef unsigned int u32; typedef u8 bool8;
#include "include/constants/items.h"
#define TRUE 1
#define FALSE 0
#define HACK_VAR_DANGER_LEAF_BOUGHT 0x50F6
#define DANGER_LEAF_TAG 0xD000
#define MONEY_MAX 9999999
#define BOUGHT_BITS 6
#define BOUGHT_MAX 63
static u16 saved; static int open=1;
static u16 VarGet(u16 id) { (void)id; return saved; }
static void VarSet(u16 id,u16 value) { (void)id; saved=value; }
static bool8 MegaShop_IsOpen(void) { return open; }
PRODUCTION
int main(void) {
    const u16 items[]={ITEM_DANGER_LEAF,ITEM_DANGER_SUGAR,ITEM_MILD};
    for(u32 value=0;value<65536;value++) {
        for(u32 which=0;which<3;which++) {
            saved=value;
            u32 count=0,expected=5000;
            if(which<2 && (value&0xF000)==0xD000)
                count=(value>>(which*6))&63;
            for(u32 i=0;i<count;i++) {
                expected=expected+expected/2;
                if(expected>9999999) expected=9999999;
            }
            if(DangerLeafPrice(items[which],5000)!=expected) return 1;
            u32 original=(value&0xF000)==0xD000 ? value&0xFFF : 0;
            if(which==2) {
                if(MegaShop_AfterPurchase(items[which]) || saved!=value) return 2;
            } else {
                u32 shift=which*6,old=(original>>shift)&63;
                u32 next=old<63 ? old+1 : 63;
                u16 wanted=0xD000|(original&~(63u<<shift))|(next<<shift);
                if(!MegaShop_AfterPurchase(items[which]) || saved!=wanted) return 3;
            }
            open=0; saved=value;
            if(MegaShop_AfterPurchase(items[which]) || saved!=value) return 4;
            open=1;
        }
    }
    return 0;
}
'''.replace('PRODUCTION', production)
    with tempfile.TemporaryDirectory() as directory:
        path = Path(directory)
        c, obj, exe = (path / ('shop_escalation' + suffix) for suffix in ('.c', '.obj', '.exe'))
        c.write_text(fixture, encoding='utf-8')
        subprocess.run(['clang', '--target=x86_64-pc-windows-msvc', '-O2', '-fno-builtin',
                        '-fno-stack-protector', '-I', str(ROOT), '-c', str(c), '-o', str(obj)], check=True)
        subprocess.run(['lld-link', '/entry:main', '/subsystem:console', '/nodefaultlib',
                        '/out:' + str(exe), str(obj)], check=True)
        subprocess.run([str(exe)], check=True)
    print('Shop escalation: 196608 saved-counter/item combinations passed; unrelated stock retains base price.')


if __name__ == '__main__':
    main()
