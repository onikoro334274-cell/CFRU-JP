"""Execute both production Dome duplicate predicates with controlled candidates."""
import argparse
import re
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
BASELINE = '61e1b61653c1c87077860400a995ff3a36b81689'


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--baseline', action='store_true')
    args = parser.parse_args()
    source = (subprocess.check_output(['git', 'show', BASELINE + ':src/hoenn_dome.c'], cwd=ROOT).decode('utf-8')
              if args.baseline else (ROOT / 'src/hoenn_dome.c').read_text(encoding='utf-8'))
    predicates = re.findall(r'if \((alreadySelectedMonId == monId.*?)\)\s*break;', source, re.S)
    assert len(predicates) == 2
    fixture = '''
static int HoennFactory_MonSpecies(int id) { return id / 10; }
static int HoennFactory_MonItem(int id) { return id % 10; }
static int duplicate(int alreadySelectedMonId, int monId, int staleSpecies) {
    int species[3] = {HoennFactory_MonSpecies(alreadySelectedMonId), staleSpecies, 0};
    return PREDICATE;
}
int main(void) {
    // Candidate matches the previous trainer, but not the current party.
    if (duplicate(11, 22, 2)) return 1;
    if (!duplicate(11, 12, 9)) return 2;
    if (!duplicate(11, 21, 9)) return 3;
    if (!duplicate(11, 11, 9)) return 4;
    if (duplicate(11, 22, 9)) return 5;
    return 0;
}
'''
    with tempfile.TemporaryDirectory() as directory:
        path = Path(directory)
        for index, predicate in enumerate(predicates):
            c, obj, exe = (path / ('party' + str(index) + suffix) for suffix in ('.c', '.obj', '.exe'))
            c.write_text(fixture.replace('PREDICATE', predicate), encoding='utf-8')
            subprocess.run(['clang', '--target=x86_64-pc-windows-msvc', '-O2', '-fno-builtin',
                            '-fno-stack-protector', '-c', str(c), '-o', str(obj)], check=True)
            subprocess.run(['lld-link', '/entry:main', '/subsystem:console', '/nodefaultlib',
                            '/out:' + str(exe), str(obj)], check=True)
            result = subprocess.run([str(exe)]).returncode
            if args.baseline:
                assert result == 1, result
            else:
                assert result == 0, result
    print('Original stale-trainer exclusion reproduced.' if args.baseline else
          'Both Dome party predicates accept unrelated candidates and reject duplicate IDs, species and items.')


if __name__ == '__main__':
    main()
