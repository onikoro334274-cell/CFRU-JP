"""Track GNU compiler/assembler prerequisites and command changes."""
import json
import re
from pathlib import Path


def prerequisites(dependency_file):
    text = Path(dependency_file).read_text(encoding='utf-8')
    text = re.sub(r'\\\r?\n', '', text)
    # A drive-letter colon is followed by a slash, not target whitespace.
    match = re.search(r':\s', text)
    if match is None:
        raise ValueError('Invalid dependency rule')
    rule = text[match.end():].splitlines()[0]
    tokens = re.findall(r'(?:\\.|[^\s])+', rule)
    return [re.sub(r'\\([\\ #:$])', r'\1', token).replace('$$', '$') for token in tokens]


def needs_rebuild(source, output, command):
    output = Path(output)
    try:
        if json.loads(Path(str(output) + '.cmd').read_text(encoding='utf-8')) != command:
            return True
        dependencies = prerequisites(str(output) + '.d')
        if not dependencies:
            return True
        object_time = output.stat().st_mtime_ns
        return any(Path(name).stat().st_mtime_ns >= object_time
                   for name in [source] + dependencies)
    except (OSError, ValueError, IndexError):
        return True


def record_command(output, command):
    Path(str(output) + '.cmd').write_text(json.dumps(command) + '\n', encoding='utf-8')


def invalidate_command(output):
    try:
        Path(str(output) + '.cmd').unlink()
    except FileNotFoundError:
        pass


def record_prerequisites(output, dependencies):
    def escape(name):
        return str(Path(name).as_posix()).replace('\\', '\\\\').replace('$', '$$') \
            .replace(' ', '\\ ').replace('#', '\\#').replace(':', '\\:')
    rule = escape(output) + ': ' + ' '.join(escape(name) for name in sorted(set(dependencies)))
    Path(str(output) + '.d').write_text(rule + '\n', encoding='utf-8')
