"""Check field-speed gating and default-speed early return for all save values."""
import subprocess
import tempfile
from pathlib import Path
from check_mega_shop_escalation import function

ROOT = Path(__file__).resolve().parent.parent


def main():
    production = function((ROOT / 'src/overworld_speedup.c').read_text(encoding='utf-8'),
                          'u8 OverworldSpeedup_ExtraIterations(void)')
    fixture = r'''
typedef unsigned char u8;typedef unsigned short u16;typedef unsigned int u32;
#define HOENN_VAR_OVERWORLD_SPEED 0x5147
#define OVERWORLD_SPEED_MAX 3
static u16 saved;static int scripts,dex,ice,scriptCalls,dexCalls,iceCalls,errors;
static u16 VarGet(u16 id) {if(id!=0x5147) errors++;return saved;}
static int ScriptContext2_IsEnabled(void) {scriptCalls++;return scripts;}
static int IsDexNavHudActive(void) {dexCalls++;return dex;}
static int PlayerOnIce(void) {iceCalls++;return ice;}
PRODUCTION
int main(void) {
    for(u32 raw=0;raw<65536;raw++) for(int flags=0;flags<8;flags++) {
        saved=raw;scripts=flags&1;dex=flags&2;ice=flags&4;
        scriptCalls=dexCalls=iceCalls=errors=0;
        int enabled=raw>0 && raw<=3;
        u8 expected=enabled && !scripts && !dex && !ice ? raw : 0;
        if(OverworldSpeedup_ExtraIterations()!=expected || errors) return 1;
        if(scriptCalls!=enabled || dexCalls!=(enabled && !scripts) || iceCalls!=(enabled && !scripts && !dex)) return 2;
    }
    return 0;
}
'''.replace('PRODUCTION', production)
    with tempfile.TemporaryDirectory() as directory:
        path = Path(directory)
        c, obj, exe = (path / ('overworld_speed' + suffix) for suffix in ('.c', '.obj', '.exe'))
        c.write_text(fixture, encoding='utf-8')
        subprocess.run(['clang', '--target=x86_64-pc-windows-msvc', '-O2', '-fno-builtin',
                        '-fno-stack-protector', '-c', str(c), '-o', str(obj)], check=True)
        subprocess.run(['lld-link', '/entry:main', '/subsystem:console', '/nodefaultlib',
                        '/out:' + str(exe), str(obj)], check=True)
        subprocess.run([str(exe)], check=True)
    print('Field speed: 524288 speed/script/DexNav/ice cases passed; default and invalid speed skip all state queries.')


if __name__ == '__main__':
    main()
