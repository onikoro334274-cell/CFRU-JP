"""Generate real ARM objects for every C file with the configured GNU build flags."""
import argparse
import concurrent.futures
import hashlib
import json
import os
import subprocess
import tempfile
from pathlib import Path

import build

ROOT = Path(__file__).resolve().parent.parent


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--compiler', required=True)
    parser.add_argument('--arm-include', required=True)
    args = parser.parse_args()
    compiler = Path(args.compiler).resolve()
    env = os.environ.copy()
    env['PATH'] = str(compiler.parent) + os.pathsep + env.get('PATH', '')
    flags = [str(compiler)] + build.CFLAGS + ['-isystem', args.arm_include, '-c']
    with tempfile.TemporaryDirectory() as directory:
        def check(path):
            name = path.relative_to(ROOT).as_posix()
            output = Path(directory) / (hashlib.sha256(name.encode()).hexdigest() + '.o')
            result = subprocess.run(flags + [name, '-o', str(output)], cwd=ROOT, env=env,
                                    capture_output=True, text=True, encoding='utf-8', errors='replace')
            return {'path': name, 'exit_code': result.returncode,
                    'source_sha256': hashlib.sha256(path.read_bytes()).hexdigest(),
                    'object_bytes': output.stat().st_size if output.is_file() else 0,
                    'diagnostics': result.stdout + result.stderr}
        paths = sorted((ROOT / 'src').rglob('*.c'))
        with concurrent.futures.ThreadPoolExecutor(max_workers=4) as pool:
            files = list(pool.map(check, paths))
    report = {'command': flags, 'scope': 'All src C files with actual build CFLAGS; ARM object '
              'generation does not establish linker resolution or runtime correctness.', 'files': files}
    destination = ROOT / 'audit' / 'gnu_c.json'
    destination.parent.mkdir(exist_ok=True)
    destination.write_text(json.dumps(report, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
    print('{} C files compiled; {} passed; {} failed'.format(len(files),
          sum(f['exit_code'] == 0 for f in files), sum(f['exit_code'] != 0 for f in files)))


if __name__ == '__main__':
    main()
