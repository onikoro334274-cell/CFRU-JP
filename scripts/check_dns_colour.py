"""Execute production DNS interpolation against the original RGB555 formula."""

import re
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent


def extract_function(source, signature):
    match = re.search(signature + r'\s*\{', source)
    if match is None:
        raise ValueError('DNS interpolation function not found')
    end, depth = match.end(), 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[match.start():end]


def main():
    current = (ROOT / 'src/dns.c').read_text(encoding='utf-8')
    baseline = subprocess.check_output(
        ['git', 'show', '61e1b61653c1c87077860400a995ff3a36b81689:src/dns.c'], cwd=ROOT).decode('utf-8')
    production = extract_function(current, r'static u16 FadeColourForDNS\([^;{}]*\)')
    original = extract_function(baseline, r'static u16 FadeColourForDNS\([^;{}]*\)')
    original = original.replace('FadeColourForDNS', 'OriginalFadeColourForDNS')
    fixture = r'''
typedef unsigned char u8;
typedef signed char s8;
typedef unsigned short u16;
struct PlttData { u16 r:5; u16 g:5; u16 b:5; u16 unused_15:1; };
static volatile unsigned int violations;
void __ubsan_handle_shift_out_of_bounds(void *data, unsigned long long a,
                                      unsigned long long b) { ++violations; }
''' + production + '\n' + original + r'''
int main(void) {
    // Every source/target channel pair, every legal coefficient, and each RGB lane.
    for (unsigned int lane = 0; lane < 3; ++lane)
    for (unsigned int source = 0; source < 32; ++source)
    for (unsigned int target = 0; target < 32; ++target)
    for (unsigned int coeff = 0; coeff <= 16; ++coeff) {
        struct PlttData blend = {0, 0, 0, 0};
        if (lane == 0) blend.r = target;
        if (lane == 1) blend.g = target;
        if (lane == 2) blend.b = target;
        u16 expected = OriginalFadeColourForDNS(&blend, coeff,
            lane == 0 ? source : 0, lane == 1 ? source : 0, lane == 2 ? source : 0);
        if (FadeColourForDNS(source << (lane * 5), target << (lane * 5), coeff) != expected)
            return 1;
    }
    // Exercise channel packing and the ignored high bit for all 65536 source values.
    const u16 targets[] = {0, 0x7fff, 0x4411, 0x1ce7};
    for (unsigned int source = 0; source <= 0xffff; ++source)
    for (unsigned int t = 0; t < 4; ++t)
    for (unsigned int coeff = 0; coeff <= 16; ++coeff) {
        u16 target = targets[t];
        struct PlttData blend = {target & 31, (target >> 5) & 31, (target >> 10) & 31, 0};
        u16 expected = OriginalFadeColourForDNS(&blend, coeff,
            source & 31, (source >> 5) & 31, (source >> 10) & 31);
        u16 actual = FadeColourForDNS(source, target, coeff);
        if (actual != expected || actual != FadeColourForDNS(source, target | 0x8000, coeff))
            return 2;
        if (coeff == 0 && actual != (source & 0x7fff)) return 3;
        if (coeff == 16 && actual != target) return 4;
    }
    return violations ? 5 : 0;
}
'''
    with tempfile.TemporaryDirectory() as directory:
        path = Path(directory)
        c_file, obj, exe = (path / name for name in ('dns.c', 'dns.obj', 'dns.exe'))
        c_file.write_text(fixture, encoding='utf-8')
        subprocess.run(['clang', '--target=x86_64-pc-windows-msvc', '-O2',
                        '-fsanitize=shift', '-fno-stack-protector', '-c',
                        str(c_file), '-o', str(obj)], check=True)
        subprocess.run(['lld-link', '/entry:main', '/subsystem:console', '/nodefaultlib',
                        '/out:' + str(exe), str(obj)], check=True)
        subprocess.run([str(exe)], check=True)
    print('Production DNS RGB555 interpolation matches the original formula for '
          'all channel pairs and coefficients 0..16; packed-color and shift checks passed.')


if __name__ == '__main__':
    main()
