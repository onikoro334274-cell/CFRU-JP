"""Execute save-slot input/fade/exit transitions and cleanup ownership order."""
import re
import subprocess
import tempfile
from pathlib import Path
from check_factory_party_copy import function

ROOT = Path(__file__).resolve().parent.parent


def main():
    source = (ROOT / 'src/hack_save_slot.c').read_text(encoding='utf-8')
    structs = '\n'.join(re.search(r'struct ' + name + r'\s*\{[^}]*\};', source)[0]
                        for name in ('SlotInfo', 'SlotScreen'))
    keys = (ROOT / 'include/gba/io_reg.h').read_text(encoding='utf-8')
    defines = '\n'.join(re.search(r'^#define ' + name + r'\s+[^\n]*', keys, re.M)[0]
                        for name in ('A_BUTTON', 'B_BUTTON', 'START_BUTTON', 'DPAD_UP', 'DPAD_DOWN'))
    production = function(source, 'static void Task_HackSaveSlot(u8 taskId)')
    fixture = r'''
typedef unsigned char u8;typedef unsigned short u16;typedef unsigned int u32;
#define TRUE 1
#define SE_SELECT 5
#define PALETTES_ALL 0xffffffffu
#define RGB_BLACK 0
DEFINES
STRUCTS
static struct SlotScreen screen;
static struct {u16 newKeys;} gMain;
static struct {u8 active;} fade,*gPaletteFade=&fade;
#define JOY_NEW(buttons) (gMain.newKeys&(buttons))
static int order[8],calls,sounds,prints,fades,errors,freed;static u8 chosen;
static struct SlotScreen *ScreenOf(u8 task){if(task!=7||freed) errors++;return &screen;}
static void FreeAllWindowBuffers(void){order[calls++]=1;if(freed) errors++;}
static void Free(void *pointer){
    order[calls++]=2;if(pointer!=&screen||freed) errors++;freed=1;
    /* Poison the released memory to verify that the slot is cached before free. */
    for(unsigned i=0;i<sizeof(screen);i++) ((u8*)&screen)[i]=0xa5;
}
static void DestroyTask(u8 task){order[calls++]=3;if(task!=7||!freed) errors++;}
static void LoadChosenAndGoToMainMenu(u8 slot){order[calls++]=4;if(!freed) errors++;chosen=slot;}
static void PlaySE(u16 sound){sounds++;if(sound!=SE_SELECT||freed) errors++;}
static void PrintScreen(struct SlotScreen *s){prints++;if(s!=&screen||freed) errors++;}
static void BeginNormalPaletteFade(u32 palettes,int delay,u8 start,u8 end,u16 color){
    fades++;if(palettes!=0xffffffffu||delay||start||end!=16||color||freed) errors++;
}
PRODUCTION
int main(void){
    for(unsigned keys=0;keys<1024;keys++) for(unsigned active=0;active<2;active++)
    for(unsigned exiting=0;exiting<2;exiting++) for(unsigned unavailable=0;unavailable<2;unavailable++)
    for(unsigned cursor=0;cursor<2;cursor++){
        screen.cursor=cursor;screen.exiting=exiting;screen.unavailable=unavailable;gMain.newKeys=keys;fade.active=active;
        calls=sounds=prints=fades=errors=freed=0;chosen=99;Task_HackSaveSlot(7);
        if(errors) return 1;
        if(!active&&exiting){
            if(!freed||calls!=4||chosen!=cursor||sounds||prints||fades) return 2;
            for(unsigned i=0;i<4;i++) if(order[i]!=(int)i+1) return 3;
        }else{
            if(freed||calls||chosen!=99) return 4;
            unsigned expectedCursor=cursor,expectedExit=exiting;int expectedSound=0,expectedPrint=0,expectedFade=0;
            if(!active&&!exiting){
                if(unavailable){if(keys&(A_BUTTON|B_BUTTON|START_BUTTON)){expectedCursor=0;expectedExit=1;expectedSound=expectedFade=1;}}
                else if(keys&(DPAD_UP|DPAD_DOWN)){expectedCursor^=1;expectedSound=expectedPrint=1;}
                else if(keys&A_BUTTON){expectedExit=1;expectedSound=expectedFade=1;}
            }
            if(screen.cursor!=expectedCursor||screen.exiting!=expectedExit||screen.unavailable!=unavailable||sounds!=expectedSound||prints!=expectedPrint||fades!=expectedFade) return 5;
        }
    }
    return 0;
}
'''.replace('DEFINES', defines).replace('STRUCTS', structs).replace('PRODUCTION', production)
    with tempfile.TemporaryDirectory() as directory:
        path = Path(directory)
        c, obj, exe = (path / ('slot_task' + suffix) for suffix in ('.c', '.obj', '.exe'))
        c.write_text(fixture, encoding='utf-8')
        subprocess.run(['clang', '--target=x86_64-pc-windows-msvc', '-O2', '-fno-builtin',
                        '-fno-stack-protector', '-c', str(c), '-o', str(obj)], check=True)
        subprocess.run(['lld-link', '/entry:main', '/subsystem:console', '/nodefaultlib',
                        '/out:' + str(exe), str(obj)], check=True)
        subprocess.run([str(exe)], check=True)
    print('Save-slot task: 16384 key/fade/exit/mode/cursor cases and poisoned-free cleanup order pass.')


if __name__ == '__main__':
    main()
