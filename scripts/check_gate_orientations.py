"""Execute gate orientation/arm lookup with every saved byte and actual map tables."""
import argparse
import re
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--baseline', help='Git revision whose production gate logic should be checked')
    args = parser.parse_args()
    source = (ROOT / 'src/hoenn_gates.c').read_text(encoding='utf-8')
    if args.baseline:
        source = subprocess.run(['git', 'show', args.baseline + ':src/hoenn_gates.c'],
                                cwd=ROOT, capture_output=True, check=True,
                                encoding='utf-8').stdout
    header = (ROOT / 'src/Tables/hoenn_gates.h').read_text(encoding='utf-8')
    tables = '\n'.join(re.findall(r'^#define HOENN_GATE_(?:COUNT|SHAPE_\w+)\s+\d+.*$', header, re.M))
    tables += '\nstruct HoennGateSpot { u8 x, y, shape, orient; };\n'
    for name in ('sHoennGateSpots', 'sHoennGateArmLayout'):
        match = re.search(r'static const [^;\n]*\b' + name + r'\[[^;]*?\n\};', header)
        assert match, name
        tables += match.group(0) + '\n'
    start = source.index('static u8 HoennGateOrientation(')
    end = source.index('/* 素 `RotatingGate_CanRotate`', start)
    functions = source[start:end]
    fixture = '''
typedef unsigned char u8;
typedef u8 bool8;
TABLES
static u8 saved[HOENN_GATE_COUNT];
static u8* HoennGateState(void) { return saved; }
FUNCTIONS
int main(void) {
    for (int gate = 0; gate < HOENN_GATE_COUNT; gate++) {
        if (sHoennGateSpots[gate].orient > 3 || sHoennGateSpots[gate].shape >= 12) return 1;
        for (int value = 0; value < 256; value++) {
            saved[gate] = value;
            int expected = value >= 1 && value <= 4 ? value - 1 : sHoennGateSpots[gate].orient;
            if (HoennGateOrientation(gate) != expected) return 2;
            for (int arm = 0; arm < 8; arm++) {
                int direction = (arm / 2 + 4 - expected) % 4;
                int result = sHoennGateArmLayout[sHoennGateSpots[gate].shape][2 * direction + arm % 2];
                if (HoennGateHasArm(gate, arm) != result) return 3;
            }
            if (saved[gate] != value) return 4;
        }
    }
    return 0;
}
'''.replace('TABLES', tables).replace('FUNCTIONS', functions)
    with tempfile.TemporaryDirectory() as directory:
        path = Path(directory)
        c, obj, exe = (path / ('orientations' + suffix) for suffix in ('.c', '.obj', '.exe'))
        c.write_text(fixture, encoding='utf-8')
        subprocess.run(['clang', '--target=x86_64-pc-windows-msvc', '-O2', '-fno-builtin',
                        '-fno-stack-protector', '-c', str(c), '-o', str(obj)], check=True)
        subprocess.run(['lld-link', '/entry:main', '/subsystem:console', '/nodefaultlib',
                        '/out:' + str(exe), str(obj)], check=True)
        subprocess.run([str(exe)], check=True)
    print('Gate orientations: 2048 saved-state cases and 16384 arm lookups pass using actual map tables.')


if __name__ == '__main__':
    main()
