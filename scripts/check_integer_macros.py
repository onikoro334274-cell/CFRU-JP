"""Run extracted production macros with shift sanitization (Windows Clang/lld)."""
import re
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent


def main():
    names = {'T1_READ_32', 'T2_READ_32', 'CpuFastFill8', 'CpuFastFill16'}
    macros = []
    for name in ('include/global.h', 'include/gba/macro.h'):
        for line in (ROOT / name).read_text().splitlines():
            if any(re.match(r'#define ' + macro + r'\(', line) for macro in names):
                macros.append(line)
    if len(macros) != len(names):
        raise RuntimeError('Required production macros could not be found.')
    source = '''
typedef unsigned int u32;
static volatile int violations;
void __ubsan_handle_shift_out_of_bounds(void *data, void *lhs, void *rhs) {
    (void)data; (void)lhs; (void)rhs; violations++;
}
static u32 filled;
#define CpuFastFill(value, dest, size) (filled = (value))
''' + '\n'.join(macros) + '''
int main(void) {
    unsigned char bytes[4];
    bytes[0] = 0xEF; bytes[1] = 0xCD; bytes[2] = 0xAB;
    for (u32 high = 0; high < 256; high++) {
        bytes[3] = high;
        u32 expected = (high << 24) | 0xABCDEFu;
        if (T1_READ_32(bytes) != expected || T2_READ_32(bytes) != expected) return 1;
        CpuFastFill8(high, 0, 32);
        if (filled != high * 0x01010101u) return 2;
    }
    for (u32 value = 0; value < 65536; value++) {
        CpuFastFill16(value, 0, 32);
        if (filled != value * 0x00010001u) return 3;
    }
    return violations ? 4 : 0;
}
'''
    with tempfile.TemporaryDirectory() as directory:
        path = Path(directory)
        c_file = path / 'macros.c'
        obj = path / 'macros.obj'
        exe = path / 'macros.exe'
        c_file.write_text(source)
        subprocess.run(['clang', '--target=x86_64-pc-windows-msvc', '-O2',
                        '-fsanitize=shift', '-fno-stack-protector', '-c', str(c_file),
                        '-o', str(obj)], check=True)
        subprocess.run(['lld-link', '/entry:main', '/subsystem:console', '/nodefaultlib',
                        '/out:' + str(exe), str(obj)], check=True)
        subprocess.run([str(exe)], check=True)
    print('Production macros passed all byte/halfword values with shift sanitization.')


if __name__ == '__main__':
    main()
