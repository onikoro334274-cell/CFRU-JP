"""Execute production buy/sell price routing across all halfword item IDs."""
import subprocess
import tempfile
from pathlib import Path
from check_mega_shop_escalation import function

ROOT = Path(__file__).resolve().parent.parent


def main():
    source = (ROOT / 'src/mega_shop.c').read_text(encoding='utf-8')
    production = '\n'.join(function(source, signature) for signature in (
        'static u16 ItemIdGetBasePrice(u16 item)', 'u16 MegaShop_ItemIdGetPrice(u16 item)', 'u32 MegaShop_Price(u16 item)'))
    production = function((ROOT / 'src/hack_difficulty.c').read_text(encoding='utf-8'), 'u32 HackDifficulty_BuyPrice(u32 price, u16 difficulty)') + '\n' + production
    fixture = r'''
typedef unsigned short u16;typedef unsigned int u32;typedef unsigned char u8;
#include "include/constants/items.h"
#include "include/constants/hold_effects.h"
#include "src/Tables/mega_shop_items.h"
#define DIFFICULTY_HARD 3
#define DIFFICULTY_MOMO_ICE 4
#define HOENN_VAR_DIFFICULTY 0
static u16 difficulty;
static u16 VarGet(u16 id){return difficulty;}
static u32 oracle(u32 price){if(!price||difficulty<3)return price;u32 value=(unsigned long long)price*(difficulty==3?20:40)/100;return value?value:1;}
#define MEGA_SELL_TABLE_PRICE 60000
#define SILPH_MASTER_BALL_PRICE 65535
static struct {u16 price;u8 holdEffect;} gItemData[ITEMS_COUNT];
static const u16 KantoSilph_MasterBallItems[]={ITEM_MASTER_BALL,0};
static struct {const u16* itemList;} gShopData;
static void CB2_BuyMenu(void) {}
static void CB2_InitBuyMenu(void) {}
static void other(void) {}
static struct {void (*callback2)(void);} gMain;
static u32 DangerLeafPrice(u16 item,u32 base) {(void)item;return base;}
u16 MegaShop_ItemIdGetPrice(u16 item);
static u16 ItemId_GetPrice(u16 item) {return MegaShop_ItemIdGetPrice(item);}
PRODUCTION
int main(void) {
    const u16* shops[]={0,gMegaShopItems,gZCrystalShopItems,gDangerLeafShopItems,KantoSilph_MasterBallItems};
    void (*callbacks[])(void)={other,CB2_BuyMenu,CB2_InitBuyMenu};
    for(difficulty=0;difficulty<5;difficulty++)for(u32 mode=0;mode<4;mode++) {
        for(u32 i=0;i<ITEMS_COUNT;i++) {
            gItemData[i].price=mode&1 ? (u16)(i+123) : 0;
            gItemData[i].holdEffect=mode&2 ? ITEM_EFFECT_MEGA_STONE : 0;
        }
        for(u32 shop=0;shop<5;shop++) for(u32 callback=0;callback<3;callback++) {
            gShopData.itemList=shops[shop];gMain.callback2=callbacks[callback];
            for(u32 raw=0;raw<65536;raw++) {
                u32 id=raw<ITEMS_COUNT ? raw : ITEM_NONE;
                u32 sell=gItemData[id].price;
                if(!sell && gItemData[id].holdEffect==ITEM_EFFECT_MEGA_STONE) {
                    sell=60000;
                    if(id==ITEM_CHARIZARDITE_Y || id==ITEM_ZA_RAICHUNITE_Y) sell=10000;
                }
                if(raw==ITEM_MASTER_BALL && shop==4 && callback!=0) sell=65535;
                if(callback!=0)sell=oracle(sell);
                if(MegaShop_ItemIdGetPrice(raw)!=sell) return 1;
                u32 buy=sell;
                const u16* stock=0;const u32* prices=0;u32 count=0;
                if(shop==1) {stock=gMegaShopItems;prices=sMegaShopPrices;count=MEGA_SHOP_COUNT;}
                if(shop==2) {stock=gZCrystalShopItems;prices=sZCrystalShopPrices;count=Z_CRYSTAL_SHOP_COUNT;}
                if(shop==3) {stock=gDangerLeafShopItems;prices=sDangerLeafShopPrices;count=DANGER_LEAF_SHOP_COUNT;}
                for(u32 i=0;i<count;i++) if(stock[i]==raw) buy=oracle(prices[i]);
                if(MegaShop_Price(raw)!=buy) return 2;
            }
        }
    }
    return 0;
}
'''.replace('PRODUCTION', production)
    with tempfile.TemporaryDirectory() as directory:
        path = Path(directory)
        c, obj, exe = (path / ('shop_prices' + suffix) for suffix in ('.c', '.obj', '.exe'))
        c.write_text(fixture, encoding='utf-8')
        subprocess.run(['clang', '--target=x86_64-pc-windows-msvc', '-O2', '-fno-builtin',
                        '-fno-stack-protector', '-I', str(ROOT), '-c', str(c), '-o', str(obj)], check=True)
        subprocess.run(['lld-link', '/entry:main', '/subsystem:console', '/nodefaultlib',
                        '/out:' + str(exe), str(obj)], check=True)
        subprocess.run([str(exe)], check=True)
    print('Shop prices: 19660800 item/table/shop/callback/difficulty cases passed for buy and sell routing.')


if __name__ == '__main__':
    main()
