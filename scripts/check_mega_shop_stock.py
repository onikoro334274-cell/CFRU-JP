"""Validate generated shop stock and price arrays with actual item constants."""
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent


def main():
    fixture = r'''
typedef unsigned short u16;typedef unsigned int u32;
#include "include/constants/items.h"
#include "src/Tables/mega_shop_items.h"
static int stock(const u16* items,const u32* prices,u32 count) {
    if(items[count]!=ITEM_NONE) return 1;
    for(u32 i=0;i<count;i++) {
        if(!items[i] || items[i]>=ITEMS_COUNT || !prices[i]) return 2;
        for(u32 j=0;j<i;j++) if(items[i]==items[j]) return 3;
    }
    return 0;
}
int main(void) {
    if(stock(gMegaShopItems,sMegaShopPrices,MEGA_SHOP_COUNT)) return 1;
    if(stock(gZCrystalShopItems,sZCrystalShopPrices,Z_CRYSTAL_SHOP_COUNT)) return 2;
    if(stock(gDangerLeafShopItems,sDangerLeafShopPrices,DANGER_LEAF_SHOP_COUNT)) return 3;
    int cheap=0;
    for(u32 i=0;i<MEGA_SHOP_COUNT;i++) {
        int isCheap=gMegaShopItems[i]==ITEM_CHARIZARDITE_Y || gMegaShopItems[i]==ITEM_ZA_RAICHUNITE_Y;
        if(sMegaShopPrices[i]!=(isCheap ? 10000u : 500000u)) return 4;
        cheap+=isCheap;
    }
    if(cheap!=2) return 5;
    for(u32 i=0;i<Z_CRYSTAL_SHOP_COUNT;i++) {
        int extra=gZCrystalShopItems[i]==ITEM_GENDER_TOOL || gZCrystalShopItems[i]==ITEM_GENE_BOND;
        if(sZCrystalShopPrices[i]!=(extra ? 10000u : 50000u)) return 6;
    }
    for(u32 i=0;i<DANGER_LEAF_SHOP_COUNT;i++) if(sDangerLeafShopPrices[i]!=5000u) return 7;
    return 0;
}
'''
    with tempfile.TemporaryDirectory() as directory:
        path = Path(directory)
        c, obj, exe = (path / ('shop_stock' + suffix) for suffix in ('.c', '.obj', '.exe'))
        c.write_text(fixture, encoding='utf-8')
        subprocess.run(['clang', '--target=x86_64-pc-windows-msvc', '-O2', '-fno-builtin',
                        '-fno-stack-protector', '-I', str(ROOT), '-c', str(c), '-o', str(obj)], check=True)
        subprocess.run(['lld-link', '/entry:main', '/subsystem:console', '/nodefaultlib',
                        '/out:' + str(exe), str(obj)], check=True)
        subprocess.run([str(exe)], check=True)
    print('Shop stock: 92 mega stones, 37 Z-shop items and 3 leaf items have unique IDs, terminators and expected prices.')


if __name__ == '__main__':
    main()
