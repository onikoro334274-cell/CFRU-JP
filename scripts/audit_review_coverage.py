"""Inventory source coverage without treating compilation or scoped tests as completion."""
import hashlib
import json
import subprocess
from collections import Counter, defaultdict
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
EXTENSIONS = {'.c', '.h', '.s', '.asm', '.py', '.ld', '.tbl', '.bat', '.yml', '.yaml'}
INSERTION_FILES = {'hooks', 'functionrewrites', 'bytes', 'offsets'}


def main():
    paths = subprocess.check_output(['git', 'ls-files', '--cached', '--others',
                                     '--exclude-standard', '-z'], cwd=ROOT).decode('utf-8').split('\0')
    reviews = json.loads((ROOT / 'audit/semantic_reviews.json').read_text(encoding='utf-8'))['reviews']
    compiled = json.loads((ROOT / 'audit/gnu_c.json').read_text(encoding='utf-8'))['files']
    objects = {row['path'].replace('\\', '/'): row for row in compiled}
    grouped = defaultdict(list)
    for row in reviews:
        grouped[row['path'].replace('\\', '/')].append(row)
    files = []
    scope_counts = Counter()
    for name in sorted(set(filter(None, paths))):
        path = ROOT / name
        if path.suffix.lower() not in EXTENSIONS and name not in INSERTION_FILES:
            continue
        digest = hashlib.sha256(path.read_bytes()).hexdigest() if path.is_file() else None
        scopes = []
        for row in grouped.get(name, []):
            state = 'missing_source' if digest is None else (
                'identity_not_recorded' if not row.get('source_sha256') else (
                    'current' if row['source_sha256'] == digest else 'source_changed'))
            references = []
            for record in row.get('verification_records', []):
                target = Path(record) if record.startswith('audit/') else Path('audit') / record
                references.append({'path': target.as_posix(), 'exists': (ROOT / target).is_file()})
            scope_counts[state] += 1
            scopes.append({'state': state, 'sections': row.get('sections', []),
                           'recorded_source_sha256': row.get('source_sha256'),
                           'evidence': references, 'verification': row.get('verification'),
                           'remaining': row.get('remaining')})
        record = objects.get(name)
        compilation = 'not_recorded' if path.suffix.lower() == '.c' else 'not_applicable'
        if record is not None:
            compilation = 'source_changed' if record.get('source_sha256') != digest else (
                'object_compiled' if record['exit_code'] == 0 else 'failed')
        files.append({'path': name, 'source_sha256': digest,
                      'gnu_c_object': compilation, 'scoped_reviews': scopes,
                      'whole_file_completion': 'not_established'})
    counts = Counter(row['gnu_c_object'] for row in files if row['gnu_c_object'] != 'not_applicable')
    report = {
        'scope': 'Tracked and nonignored source/linker/configuration/insertion files at current worktree state',
        'limitations': 'Current hashes establish identity, not semantic completeness. Compilation covers source translation units only, not dependency provenance or linking. Scoped reviews do not establish whole-file or whole-project completion. Files without indexed scopes may have other reports; this inventory does not infer evidence from filenames or incidental text mentions.',
        'summary': {'source_files': len(files), 'files_with_indexed_scopes': sum(bool(row['scoped_reviews']) for row in files),
                    'indexed_scopes': len(reviews), 'scope_states': dict(scope_counts),
                    'gnu_c_states': dict(counts), 'whole_file_completions_established': 0},
        'files': files,
    }
    (ROOT / 'audit/review_coverage.json').write_text(json.dumps(report, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
    print(json.dumps(report['summary'], ensure_ascii=False))


if __name__ == '__main__':
    main()
