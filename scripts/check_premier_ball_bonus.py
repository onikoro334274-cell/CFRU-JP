"""Exercise purchase-return bonus behavior in both configuration branches."""
import subprocess
import tempfile
from pathlib import Path
from check_mega_shop_escalation import function

ROOT = Path(__file__).resolve().parent.parent


def main():
    production = function((ROOT / 'src/item.c').read_text(encoding='utf-8'),
                          'void Task_ReturnToItemListAfterItemPurchase(u8 taskId)')
    fixture = r'''
typedef unsigned char u8;typedef unsigned short u16;typedef short s16;
#define A_BUTTON 1
#define B_BUTTON 2
#define SE_SELECT 5
#define POCKET_POKE_BALLS 3
#define ITEM_PREMIER_BALL 12
#define tItemId data[5]
#define tItemCount data[1]
static struct {s16 data[16];} gTasks[1];
static struct {u16 newKeys;} gMain;
static u8 gStringVar1[20],gStringVar2[20];
static const u8 gText_ThrowInOnePremierBall[]={1},gText_ThrowInPremierBalls[]={2};
static int pocket,bagSuccess,calls,bonus,returned,message;
static int GetPocketByItemId(u16 item) {(void)item;return pocket;}
static void PlaySE(int sound) {(void)sound;}
static int AddBagItem(u16 item,u16 count) {if(item!=12) return 0;calls++;bonus=count;return bagSuccess;}
static void ConvertIntToDecimalStringN(u8* dest,int value,int mode,int digits) {(void)dest;(void)value;(void)mode;(void)digits;}
static const u8* ItemId_GetName(u16 item) {(void)item;return gStringVar1;}
static void StringCopy(u8* dest,const u8* source) {(void)dest;(void)source;}
static void BuyMenuReturnToItemList(u8 id) {(void)id;returned++;}
static void BuyMenuDisplayMessage(u8 id,const u8* text,void (*callback)(u8)) {(void)id;(void)callback;message=text[0];}
PRODUCTION
int main(void) {
    for(int count=0;count<=999;count++) for(int balls=0;balls<2;balls++)
    for(int keys=0;keys<4;keys++) for(int success=0;success<2;success++) {
        gTasks[0].data[1]=count;gTasks[0].data[5]=1;gMain.newKeys=keys;
        pocket=balls ? 3 : 1;bagSuccess=success;calls=bonus=returned=message=0;
        Task_ReturnToItemListAfterItemPurchase(0);
        int expected=0;
        if(balls && keys) {
#ifdef MULTIPLE_PREMIER_BALLS_AT_ONCE
            expected=count/10;
#else
            expected=count>=10;
#endif
        }
        if(calls!=(expected>0) || bonus!=expected) return 1;
        int award=expected>0 && success;
        if(message!=(award ? (expected==1 ? 1 : 2) : 0)) return 2;
        if(returned!=(keys && !award)) return 3;
    }
    return 0;
}
'''.replace('PRODUCTION', production)
    with tempfile.TemporaryDirectory() as directory:
        path = Path(directory)
        c, obj, exe = (path / ('premier_bonus' + suffix) for suffix in ('.c', '.obj', '.exe'))
        c.write_text(fixture, encoding='utf-8')
        for multiple in (False, True):
            flags = ['-DMULTIPLE_PREMIER_BALLS_AT_ONCE'] if multiple else []
            subprocess.run(['clang', '--target=x86_64-pc-windows-msvc', '-O2', '-fno-builtin',
                            '-fno-stack-protector', *flags, '-c', str(c), '-o', str(obj)], check=True)
            subprocess.run(['lld-link', '/entry:main', '/subsystem:console', '/nodefaultlib',
                            '/out:' + str(exe), str(obj)], check=True)
            subprocess.run([str(exe)], check=True)
    print('Premier bonus: 32000 cases passed across both configurations, quantities 0..999, keys and bag failures.')


if __name__ == '__main__':
    main()
