"""Execute DNS palette helpers against independent RGB555 weighting and canaries."""
import subprocess
import tempfile
from pathlib import Path
from check_factory_party_copy import function

ROOT = Path(__file__).resolve().parent.parent


def main():
    source = (ROOT / 'src/dns.c').read_text(encoding='utf-8')
    production = '\n'.join(function(source, signature) for signature in (
        'static u16 FadeColourForDNS(u16 colour, u16 blend, u8 coeff)',
        'static void BlendFadedPalette(u16 palOffset, u16 numEntries, u8 coeff, u32 blendColor)',
        'static void BlendFadedUnfadedPalette(u16 palOffset, u16 numEntries, u8 coeff, u32 blendColor, bool8 palFadeActive)',
        'void DNSBattleBGPalFade(void)',
    ))
    fixture = r'''
typedef unsigned char u8;typedef signed char s8;typedef unsigned short u16;typedef unsigned int u32;typedef u8 bool8;
#define RGB_BLACK 0
#define RGB_WHITE 0x7fff
static u16 gPlttBufferFaded[512],gPlttBufferUnfaded[512],hardware[512],oldFaded[512],oldUnfaded[512];
static bool8 gIgnoredDNSPalIndices[32][16],gDontFadeWhite;
static struct {bool8 inBattle;} gMain;
#define PLTT hardware
#define MAP_TYPE_0 0
#define MAP_TYPE_UNDERGROUND 4
#define MAP_TYPE_INDOOR 8
#define MAP_TYPE_SECRET_BASE 9
#define BATTLE_DNS_PAL_FADE 0x1c
static u8 mapType;
static u8 GetCurrentMapType(void){return mapType;}
static struct {u8 hour,minute;} gClock;
static struct {u8 amount;u16 colour;} gDNSNightFadingByTime[24][6];
PRODUCTION
static u16 oracle(u16 source,u16 target,unsigned coeff){
    u16 result=0;
    for(unsigned lane=0;lane<3;lane++){
        unsigned from=(source>>(lane*5))&31,to=(target>>(lane*5))&31;
        result|=((from*(16-coeff)+to*coeff)/16)<<(lane*5);
    }
    return result;
}
int main(void){
    for(unsigned palette=0;palette<32;palette++) for(unsigned coeff=0;coeff<=16;coeff++)
    for(unsigned ignored=0;ignored<4;ignored++) for(unsigned white=0;white<2;white++)
    for(unsigned battle=0;battle<2;battle++) for(unsigned active=0;active<2;active++){
        unsigned offset=palette*16;gDontFadeWhite=white;gMain.inBattle=battle;
        for(unsigned i=0;i<512;i++){
            unsigned column=i%16;
            oldFaded[i]=column==0?0:column==1?0x7fff:(u16)(i*37+0x1234);
            oldUnfaded[i]=column==0?0:column==1?0x7fff:(u16)(i*71+0x5678);
            gPlttBufferFaded[i]=oldFaded[i];gPlttBufferUnfaded[i]=oldUnfaded[i];hardware[i]=0xa55a;
            gIgnoredDNSPalIndices[i/16][column]=ignored==1||(ignored==2&&column%2==0)||(ignored==3&&column%2!=0);
        }
        BlendFadedPalette(offset,16,coeff,0xdead2312);
        for(unsigned i=0;i<512;i++){
            int write=i>=offset&&i<offset+16&&oldFaded[i]!=0&&!gIgnoredDNSPalIndices[i/16][i%16]
                &&!(white&&!battle&&oldFaded[i]==0x7fff);
            if(hardware[i]!=(write?oracle(oldFaded[i],0x2312,coeff):0xa55a)) return 1;
            if(gPlttBufferFaded[i]!=oldFaded[i]||gPlttBufferUnfaded[i]!=oldUnfaded[i]) return 2;
        }
        BlendFadedUnfadedPalette(offset,16,coeff,0xdead2312,active);
        for(unsigned i=0;i<512;i++){
            int write=i>=offset&&i<offset+16&&oldUnfaded[i]!=0&&!gIgnoredDNSPalIndices[i/16][i%16];
            u16 value=write?oracle(oldUnfaded[i],0x2312,coeff):oldUnfaded[i];
            if(gPlttBufferUnfaded[i]!=value||gPlttBufferFaded[i]!=(write&&!active?value:oldFaded[i])) return 3;
        }
    }
    for(unsigned map=0;map<256;map++) for(unsigned hour=0;hour<24;hour++)
    for(unsigned minute=0;minute<60;minute++){
        mapType=map;gClock.hour=hour;gClock.minute=minute;
        unsigned coeff=(hour+minute/10)%17;
        u16 target=(u16)((hour*123+minute/10*997)&0x7fff);
        gDNSNightFadingByTime[hour][minute/10].amount=coeff;
        gDNSNightFadingByTime[hour][minute/10].colour=target;
        for(unsigned i=0;i<512;i++){
            oldUnfaded[i]=(u16)((i*37)&0x7fff);oldFaded[i]=0x1234;
            gPlttBufferUnfaded[i]=oldUnfaded[i];gPlttBufferFaded[i]=oldFaded[i];hardware[i]=0xa55a;
        }
        DNSBattleBGPalFade();
        for(unsigned i=0;i<512;i++){
            int write=map!=0&&map!=4&&map!=8&&map!=9&&i>=32&&i<80;
            u16 expected=write?oracle(oldUnfaded[i],target,coeff):oldUnfaded[i];
            if(gPlttBufferUnfaded[i]!=expected) return 4;
            if(gPlttBufferFaded[i]!=(write?expected:oldFaded[i])) return 5;
            if(hardware[i]!=0xa55a) return 6;
        }
    }
    return 0;
}
'''.replace('PRODUCTION', production)
    with tempfile.TemporaryDirectory() as directory:
        path = Path(directory)
        c, obj, exe = (path / ('dns_palette' + suffix) for suffix in ('.c', '.obj', '.exe'))
        c.write_text(fixture, encoding='utf-8')
        subprocess.run(['clang', '--target=x86_64-pc-windows-msvc', '-O2', '-fno-builtin',
                        '-fno-stack-protector', '-fsanitize=shift', '-fsanitize-trap=shift',
                        '-c', str(c), '-o', str(obj)], check=True)
        subprocess.run(['lld-link', '/entry:main', '/subsystem:console', '/nodefaultlib',
                        '/out:' + str(exe), str(obj)], check=True)
        subprocess.run([str(exe)], check=True)
    print('DNS palette writes: 17408 palette/coefficient/ignore/white/battle/fade cases match independent RGB555 weighting; surrounding palettes remain untouched.')


if __name__ == '__main__':
    main()
