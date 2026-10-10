"""Execute the option window allocation guard and its ordered cleanup."""
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent


def main():
    source = (ROOT / 'src/bw_option_menu.c').read_text(encoding='utf-8')
    start = source.index('if (!InitWindows(sWindowTemplates))')
    end = source.index('DeactivateAllTextPrinters();', start)
    production = 'static void init_section(void) {\n' + source[start:end] + '\ncontinued++;\n}'
    fixture = r'''
static int success,stage,continued,errors;
static const int sWindowTemplates[]={123};
static void return_callback(void) {}
static void previous_callback(void) {}
static struct {void (*savedCallback)(void);} gMain;
static void (*current)(void);
static int InitWindows(const int* templates) {
    if(templates!=sWindowTemplates || stage) errors++;
    stage=1;return success;
}
static void FreeAllWindowBuffers(void) {if(stage!=1) errors++;stage=2;}
static void SetMainCallback2(void (*callback)(void)) {
    if(stage!=2 || callback!=gMain.savedCallback) errors++;
    stage=3;current=callback;
}
PRODUCTION
int main(void) {
    for(success=0;success<2;success++) {
        stage=continued=errors=0;current=previous_callback;gMain.savedCallback=return_callback;
        init_section();
        if(errors || continued!=success || stage!=(success ? 1 : 3)) return 1;
        if(current!=(success ? previous_callback : return_callback)) return 2;
    }
    return 0;
}
'''.replace('PRODUCTION', production)
    with tempfile.TemporaryDirectory() as directory:
        path = Path(directory)
        c, obj, exe = (path / ('option_windows' + suffix) for suffix in ('.c', '.obj', '.exe'))
        c.write_text(fixture, encoding='utf-8')
        subprocess.run(['clang', '--target=x86_64-pc-windows-msvc', '-O2', '-fno-builtin',
                        '-fno-stack-protector', '-c', str(c), '-o', str(obj)], check=True)
        subprocess.run(['lld-link', '/entry:main', '/subsystem:console', '/nodefaultlib',
                        '/out:' + str(exe), str(obj)], check=True)
        subprocess.run([str(exe)], check=True)
    print('Option window guard: failure cleans before returning to saved callback; success continues without cleanup.')


if __name__ == '__main__':
    main()
