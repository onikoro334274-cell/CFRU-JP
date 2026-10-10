"""Check every C translation unit with Clang's actual ARM target and Newlib headers."""
import argparse
import concurrent.futures
import hashlib
import json
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--arm-include', required=True, help='Actual ARM Newlib include directory')
    args = parser.parse_args()
    flags = ['clang', '--target=arm-none-eabi', '-mcpu=arm7tdmi', '-mthumb', '-std=gnu11',
             '-Wno-unknown-attributes', '-Wno-error=incompatible-pointer-types',
             '-Wno-error=int-conversion', '-isystem', args.arm_include, '-fsyntax-only']
    def check(path):
        name = path.relative_to(ROOT).as_posix()
        result = subprocess.run(flags + [name], cwd=ROOT, capture_output=True, text=True,
                                encoding='utf-8', errors='replace')
        return {'path': name, 'exit_code': result.returncode,
                'sha256': hashlib.sha256(path.read_bytes()).hexdigest(),
                'diagnostics': result.stdout + result.stderr}
    paths = sorted((ROOT / 'src').rglob('*.c'))
    with concurrent.futures.ThreadPoolExecutor(max_workers=4) as pool:
        files = list(pool.map(check, paths))
    report = {'command': flags, 'scope': 'All C files under src; ARM syntax checking with Clang '
              'does not establish GNU code generation, linking or runtime correctness.', 'files': files}
    destination = ROOT / 'audit' / 'c_syntax.json'
    destination.parent.mkdir(exist_ok=True)
    destination.write_text(json.dumps(report, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
    print('{} C files checked; {} passed; {} failed'.format(len(files),
          sum(f['exit_code'] == 0 for f in files), sum(f['exit_code'] != 0 for f in files)))


if __name__ == '__main__':
    main()
