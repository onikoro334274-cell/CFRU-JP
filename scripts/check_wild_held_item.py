"""Execute held-item selection across all bonuses and controlled RNG outcomes."""
import subprocess
import tempfile
from pathlib import Path
from check_factory_party_copy import function
ROOT = Path(__file__).resolve().parent.parent

def main():
    production = function((ROOT / 'src/build_pokemon.c').read_text(encoding='utf-8'),
        'u16 GenerateWildMonHeldItem(u16 species, u8 bonus)')
    fixture = r'''
typedef unsigned char u8;typedef unsigned short u16;typedef unsigned int u32;
#define NULL ((void*)0)
#define ITEM_NONE 0
#define MON_DATA_IS_EGG 1
static struct {u16 item1,item2;} gBaseStats[2];
static int gPlayerParty[1];
static u16 randomValue,mega;static u8 egg,ability;static unsigned calls;
static u16 Random(void){calls++;return randomValue;}
static u16 WildMegaStoneFor(u16 species){return species==1?mega:0;}
static u32 GetMonData(const int* mon,int field,void* unused){return egg;}
static u8 GetMonAbility(const int* mon){return ability;}
static u8 AbilityIncreasesWildItemChance(u8 a){return a;}
PRODUCTION
int main(void){
    const u16 items[][2]={{0,0},{101,202},{101,101},{0,202},{101,0}};
    for(unsigned mode=0;mode<5;mode++)for(egg=0;egg<2;egg++)
    for(ability=0;ability<2;ability++)for(unsigned bonus=0;bonus<256;bonus++)
    for(unsigned roll=0;roll<100;roll++)for(unsigned stone=0;stone<2;stone++){
        gBaseStats[1].item1=items[mode][0];gBaseStats[1].item2=items[mode][1];
        mega=stone?303:0;randomValue=roll;calls=0;
        unsigned first=ability&&!egg?20:45,second=ability&&!egg?80:95;
        first=bonus>=first?0:first-bonus;second=bonus>=second?0:second-bonus;
        u16 expected=stone?303:
            items[mode][0]&&items[mode][0]==items[mode][1]?items[mode][0]:
            roll<first?0:roll<second?items[mode][0]:items[mode][1];
        if(GenerateWildMonHeldItem(1,bonus)!=expected||calls!=1)return 1;
    }
    /* Verify the RNG reduction for every possible raw 16-bit result. */
    gBaseStats[1].item1=101;gBaseStats[1].item2=202;egg=ability=0;mega=0;
    for(unsigned raw=0;raw<65536;raw++){
        randomValue=raw;calls=0;unsigned roll=raw%100;
        u16 expected=roll<45?0:roll<95?101:202;
        if(GenerateWildMonHeldItem(1,0)!=expected||calls!=1)return 2;
    }
    return 0;
}
'''.replace('PRODUCTION', production)
    with tempfile.TemporaryDirectory() as directory:
        path = Path(directory)
        c, obj, exe = (path / ('held_item' + suffix) for suffix in ('.c', '.obj', '.exe'))
        c.write_text(fixture, encoding='utf-8')
        subprocess.run(['clang', '--target=x86_64-pc-windows-msvc', '-O2', '-fno-builtin',
            '-fno-stack-protector', '-c', str(c), '-o', str(obj)], check=True)
        subprocess.run(['lld-link', '/entry:main', '/subsystem:console', '/nodefaultlib',
            '/out:' + str(exe), str(obj)], check=True)
        subprocess.run([str(exe)], check=True)
    print('Wild held items: 1024000 item/egg/ability/bonus/stone cases and 65536 raw RNG values pass.')

if __name__ == '__main__':
    main()
