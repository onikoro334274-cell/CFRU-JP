"""Assemble every build-input .s file using an actual ARM GNU assembler."""
import argparse
import concurrent.futures
import hashlib
import json
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--assembler', required=True)
    args = parser.parse_args()
    flags = [args.assembler, '-mthumb', '-I', 'assembly']
    with tempfile.TemporaryDirectory() as directory:
        def check(path):
            name = path.relative_to(ROOT).as_posix()
            output = Path(directory) / (hashlib.sha256(name.encode()).hexdigest() + '.o')
            result = subprocess.run(flags + ['-c', name, '-o', str(output)], cwd=ROOT,
                                    capture_output=True, text=True, encoding='utf-8', errors='replace')
            return {'path': name, 'exit_code': result.returncode,
                    'sha256': hashlib.sha256(path.read_bytes()).hexdigest(),
                    'diagnostics': result.stdout + result.stderr}
        paths = sorted((ROOT / 'assembly').rglob('*.s'))
        with concurrent.futures.ThreadPoolExecutor(max_workers=4) as pool:
            files = list(pool.map(check, paths))
    report = {'command': flags, 'scope': 'All assembly .s build inputs; assembling does not prove '
              'linking, hook addresses or runtime correctness.', 'files': files}
    destination = ROOT / 'audit' / 'assembly.json'
    destination.parent.mkdir(exist_ok=True)
    destination.write_text(json.dumps(report, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
    print('{} assembly files checked; {} passed; {} failed'.format(len(files),
          sum(f['exit_code'] == 0 for f in files), sum(f['exit_code'] != 0 for f in files)))


if __name__ == '__main__':
    main()
