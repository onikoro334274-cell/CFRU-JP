"""Execute RTC frame cadence and play-time counter boundary transitions."""
import argparse
import re
import subprocess
import tempfile
from pathlib import Path
from check_factory_party_copy import function

ROOT = Path(__file__).resolve().parent.parent


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--baseline')
    args = parser.parse_args()
    source = (ROOT / 'src/rtc.c').read_text(encoding='utf-8')
    if args.baseline:
        source = subprocess.run(['git', 'show', args.baseline + ':src/rtc.c'], cwd=ROOT,
                                capture_output=True, encoding='utf-8', check=True).stdout
    defines = '\n'.join(re.findall(r'^#define (?:RTC_\w+|PLAYTIME_RUNNING)\s+\w+.*$',
                                   '\n'.join((ROOT / p).read_text(encoding='utf-8')
                                             for p in ('include/rtc.h', 'include/play_time.h')), re.M))
    production = '\n'.join(function(source, name) for name in
                           ('void RtcCalcLocalTime(void)', 'void PlayTimeCounter_Update(void)'))
    fixture = r'''
typedef unsigned char u8;typedef unsigned short u16;typedef unsigned int u32;typedef u8 bool8;
DEFINES
static u8 sRTCFrameCount,sPlayTimeCounterState;
static u16 sRTCErrorStatus,errors;static int mode,updates,fillCalls,maxCalls,wrongSource;
static struct SiiRtcInfo {int opaque;} sRtc;
static const struct SiiRtcInfo sRtcDummy={123};
static struct {u8 second;} gClock;
static u32 gLastClockSecond;
static struct {u16 playTimeHours;u8 playTimeMinutes,playTimeSeconds,playTimeVBlanks;} save;
#define gSaveBlock2 (&save)
static void RtcInit(void){updates++;sRTCErrorStatus=errors;sRtc.opaque=567;}
static void UpdateClockFromRtc(struct SiiRtcInfo* info){if(info->opaque!=((errors&RTC_ERR_FLAG_MASK) ? 123 : 567)) wrongSource=1;}
static bool8 PlayClock_IsOn(void){return mode;}
static void PlayClock_Fill(void){fillCalls++;}
static u16 RtcGetErrorStatus(void){return sRTCErrorStatus;}
static void PlayTimeCounter_SetToMax(void){maxCalls++;}
PRODUCTION
int main(void) {
    u16 errorCases[4]={0,RTC_INIT_ERROR,RTC_INIT_WARNING,RTC_ERR_INVALID_DAY};
    for(int e=0;e<4;e++) for(mode=0;mode<2;mode++) {
        errors=errorCases[e];sRTCFrameCount=0;updates=fillCalls=wrongSource=0;
        for(int frame=0;frame<600;frame++) {
            RtcCalcLocalTime();
            if(updates!=frame/60+1 || sRTCFrameCount!=(frame+1)%60) return 1;
            if(fillCalls!=(mode ? updates : 0) || wrongSource) return 2;
        }
    }
    u8 states[4]={0,1,2,255};u16 hours[3]={0,998,999};u8 times[3]={0,58,59};
    for(int st=0;st<4;st++) for(int e=0;e<4;e++) for(mode=0;mode<2;mode++)
    for(int changed=0;changed<2;changed++) for(int frames=0;frames<256;frames++)
    for(int h=0;h<3;h++) for(int m=0;m<3;m++) for(int s=0;s<3;s++) {
        sPlayTimeCounterState=states[st];sRTCErrorStatus=errorCases[e];
        gClock.second=20+changed;gLastClockSecond=20;maxCalls=0;
        save.playTimeHours=hours[h];save.playTimeMinutes=times[m];save.playTimeSeconds=times[s];save.playTimeVBlanks=frames;
        int running=states[st]==PLAYTIME_RUNNING;
        int nextFrames=running && frames<255 ? frames+1 : frames;
        int elapsed=running && nextFrames>=60 && ((errorCases[e]&RTC_ERR_FLAG_MASK) || mode || changed);
        u32 total=hours[h]*3600u+times[m]*60u+times[s]+elapsed;
        PlayTimeCounter_Update();
        if(save.playTimeHours!=total/3600 || save.playTimeMinutes!=(total/60)%60 || save.playTimeSeconds!=total%60) return 3;
        if(save.playTimeVBlanks!=(elapsed ? 0 : nextFrames) || gLastClockSecond!=(elapsed ? 20+changed : 20)) return 4;
        if(maxCalls!=(elapsed && total>=1000*3600u)) return 5;
    }
    return 0;
}
'''.replace('DEFINES', defines).replace('PRODUCTION', production)
    with tempfile.TemporaryDirectory() as directory:
        path = Path(directory)
        c, obj, exe = (path / ('frames' + suffix) for suffix in ('.c', '.obj', '.exe'))
        c.write_text(fixture, encoding='utf-8')
        subprocess.run(['clang', '--target=x86_64-pc-windows-msvc', '-O2', '-fno-builtin',
                        '-fno-stack-protector', '-c', str(c), '-o', str(obj)], check=True)
        subprocess.run(['lld-link', '/entry:main', '/subsystem:console', '/nodefaultlib',
                        '/out:' + str(exe), str(obj)], check=True)
        subprocess.run([str(exe)], check=True)
    print('RTC cadence: 4800 frames; play-time counter: 442368 boundary/state/error/mode cases pass.')


if __name__ == '__main__':
    main()
