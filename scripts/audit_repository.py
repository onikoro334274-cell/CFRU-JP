"""Inventory tracked files and report reproducible static checks, without building."""
import ast
import hashlib
import json
import re
import subprocess
from pathlib import Path


ROOT = Path(__file__).resolve().parent.parent
TEXT_EXTENSIONS = {'.c', '.h', '.s', '.asm', '.py', '.string', '.ld', '.tbl',
                   '.txt', '.md', '.json', '.bat', '.yml', '.yaml'}


def main():
    paths = subprocess.check_output(['git', 'ls-files', '--cached', '--others',
                                     '--exclude-standard', '-z'], cwd=ROOT).decode().split('\0')
    files = []
    for name in sorted(set(filter(None, paths))):
        if name == 'audit/repository.json':
            continue
        path = ROOT / name
        if not path.is_file():
            files.append({'path': name, 'state': 'deleted', 'review': 'pending',
                          'checks': [], 'findings': []})
            continue
        data = path.read_bytes()
        entry = {'path': name, 'bytes': len(data), 'sha256': hashlib.sha256(data).hexdigest(),
                 'review': 'pending', 'checks': [], 'findings': []}
        if path.suffix.lower() in TEXT_EXTENSIONS or not path.suffix:
            # Some source files use legacy encodings; byte checks preserve them.
            entry['checks'].append('merge_conflict_markers')
            for number, line in enumerate(data.splitlines(), 1):
                if re.match(rb'^(<<<<<<< |=======\s*$|>>>>>>> )', line):
                    entry['findings'].append({'line': number, 'kind': 'possible_merge_conflict'})
            if path.suffix == '.py':
                entry['checks'].append('python_ast')
                try:
                    ast.parse(data, filename=name)
                except SyntaxError as error:
                    entry['findings'].append({'line': error.lineno, 'kind': 'python_syntax',
                                              'message': str(error)})
            if path.suffix in ('.c', '.h'):
                entry['checks'].append('quoted_include_exists')
                for number, line in enumerate(data.splitlines(), 1):
                    match = re.match(rb'\s*#\s*include\s*"([^"]+)"', line)
                    if match:
                        target = match[1].decode('utf-8', errors='replace')
                        if not (path.parent / target).is_file():
                            entry['findings'].append({'line': number, 'kind': 'missing_local_include',
                                                      'target': target})
        files.append(entry)
    report = {'branch': subprocess.check_output(['git', 'branch', '--show-current'], cwd=ROOT).decode().strip(),
              'scope': 'Tracked and nonignored untracked files, excluding this generated inventory; '
                       'deleted tracked files are retained. Static checks do not establish semantic correctness.',
              'files': files}
    destination = ROOT / 'audit' / 'repository.json'
    destination.parent.mkdir(exist_ok=True)
    destination.write_text(json.dumps(report, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
    findings = sum(len(entry['findings']) for entry in files)
    print('{} inventoried files; {} findings; {}'.format(len(files), findings, destination))


if __name__ == '__main__':
    main()
