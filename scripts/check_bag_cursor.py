"""Execute the production bag-cursor function with array-bound sanitization."""
import argparse
import re
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--baseline', action='store_true')
    args = parser.parse_args()
    name = 'src/item.c'
    text = (subprocess.check_output(['git', 'show', '61e1b61653c1c87077860400a995ff3a36b81689:' + name], cwd=ROOT).decode('utf-8')
            if args.baseline else (ROOT / name).read_text(encoding='utf-8'))
    match = re.search(r'void PocketCalculateInitialCursorPosAndItemsAbove\([^;{]*\)\s*\{', text)
    end, depth = match.end(), 1
    while depth:
        depth += (text[end] == '{') - (text[end] == '}')
        end += 1
    function = text[match.start():end]
    fixture = '''
typedef unsigned char u8;
typedef unsigned short u16;
#define MAIN_POCKETS_COUNT 3
static int violations, calls;
void __ubsan_handle_out_of_bounds(void* data, void* index) { violations++; }
static struct {u16 itemsAbove[3]; u16 cursorPos[3]; u16 sentinel[256];} gBagMenuState;
static struct {u8 maxShowed[3]; u8 sentinel[256];} display;
#define sBagMenuDisplay (&display)
static u16 GetNumItemsInPocket(u8 pocket) { calls++; return pocket < 3 ? 10 : 0; }
''' + function + '''
int main(void) {
    volatile u8 invalid = 3;
    gBagMenuState.cursorPos[0] = 1;
    gBagMenuState.sentinel[0] = 0x1234;
    display.sentinel[0] = 7;
    PocketCalculateInitialCursorPosAndItemsAbove(invalid);
    if (violations || calls || gBagMenuState.sentinel[0] != 0x1234 || gBagMenuState.cursorPos[0] != 1) return 1;
    display.maxShowed[0] = 8;
    gBagMenuState.cursorPos[0] = 7;
    gBagMenuState.itemsAbove[0] = 9;
    PocketCalculateInitialCursorPosAndItemsAbove(0);
    if (violations || gBagMenuState.cursorPos[0] != 3 || gBagMenuState.itemsAbove[0] != 10) return 2;
    return 0;
}
'''
    with tempfile.TemporaryDirectory() as directory:
        path = Path(directory)
        c_file, obj, exe = (path / name for name in ('cursor.c', 'cursor.obj', 'cursor.exe'))
        c_file.write_text(fixture)
        subprocess.run(['clang', '--target=x86_64-pc-windows-msvc', '-O1', '-fsanitize=array-bounds',
                        '-fno-stack-protector', '-c', str(c_file), '-o', str(obj)], check=True)
        subprocess.run(['lld-link', '/entry:main', '/subsystem:console', '/nodefaultlib',
                        '/out:' + str(exe), str(obj)], check=True)
        result = subprocess.run([str(exe)])
        if args.baseline:
            if result.returncode != 1:
                raise RuntimeError('Original bounds failure was not reproduced')
            print('Original HEAD reproduces out-of-bounds bag cursor access.')
        else:
            result.check_returncode()
            print('Production bag cursor passed invalid-pocket and valid-cursor cases with bounds sanitization.')


if __name__ == '__main__':
    main()
