"""Execute profile task traversal, post-callback link edits and elapsed wrap."""
import subprocess
import tempfile
from pathlib import Path
from check_mega_shop_escalation import function

ROOT = Path(__file__).resolve().parent.parent


def main():
    production = function((ROOT / 'src/overworld_speedup.c').read_text(encoding='utf-8'),
                          'static void ProfRunTasks(void)')
    fixture = r'''
typedef unsigned char u8;typedef unsigned int u32;typedef short s16;
#define NUM_TASKS 16
struct Task {void (*func)(u8);u8 isActive,prev,next,priority;s16 data[16];};
static struct Task gTasks[16];static u32 profile[358];
#define PROF_BASE profile
static u8 first;static u32 clock;static int mode,seen[16],count;
static u8 FindFirstActiveTask(void) {return first;}
static u32 ProfNow(void) {return clock;}
static void callback(u8 id) {
    seen[count++]=id;clock+=id+1;
    if(mode==1) gTasks[id].next=255;
    if(mode==2 && gTasks[id].next!=255) gTasks[id].next=gTasks[gTasks[id].next].next;
}
PRODUCTION
int main(void) {
    for(int seed=0;seed<16;seed++) for(int length=0;length<=16;length++)
    for(mode=0;mode<3;mode++) for(int wrap=0;wrap<2;wrap++) {
        int order[16],expected[16],expectedCount=0;u32 maximum=0;
        for(int i=0;i<16;i++) {order[i]=(seed+i*5)%16;gTasks[i].next=255;gTasks[i].func=callback;}
        for(int i=0;i<length;i++) gTasks[order[i]].next=i+1<length ? order[i+1] : 255;
        for(int i=0;i<length;i+=(mode==2 ? 2 : 1)) {
            expected[expectedCount++]=order[i];
            if((u32)order[i]+1>maximum) maximum=order[i]+1;
            if(mode==1) break;
        }
        first=length ? order[0] : 16;count=0;clock=wrap ? 0xFFFFFFF0 : 0;
        for(int i=0;i<358;i++) profile[i]=0;
        profile[0]=0x464F5250;profile[357]=77;
        ProfRunTasks();
        if(count!=expectedCount || profile[265]!=maximum || profile[357]!=77) return 1;
        for(int i=0;i<count;i++) if(seen[i]!=expected[i]) return 2;
        if(count && profile[264]!=(u32)callback) return 3;
        if(!count && profile[264]) return 4;
        for(int i=0;i<358;i++) {
            if(i==264 || i==265) continue;
            u32 wanted=i==0 ? 0x464F5250 : i==357 ? 77 : 0;
            if(profile[i]!=wanted) return 6;
        }
        profile[264]=123;profile[265]=100;count=0;
        // A prior larger maximum must retain its callback identity.
        first=length ? order[0] : 16;ProfRunTasks();
        if(profile[264]!=123 || profile[265]!=100) return 5;
    }
    return 0;
}
'''.replace('PRODUCTION', production)
    with tempfile.TemporaryDirectory() as directory:
        path = Path(directory)
        c, obj, exe = (path / ('profile_tasks' + suffix) for suffix in ('.c', '.obj', '.exe'))
        c.write_text(fixture, encoding='utf-8')
        subprocess.run(['clang', '--target=x86_64-pc-windows-msvc', '-O2', '-fno-builtin',
                        '-Wno-pointer-to-int-cast', '-fno-stack-protector', '-c', str(c), '-o', str(obj)], check=True)
        subprocess.run(['lld-link', '/entry:main', '/subsystem:console', '/nodefaultlib',
                        '/out:' + str(exe), str(obj)], check=True)
        subprocess.run([str(exe)], check=True)
    print('Profile tasks: 1632 chain/order/mutation/timer-wrap scenarios passed.')


if __name__ == '__main__':
    main()
