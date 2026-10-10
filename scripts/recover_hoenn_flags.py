"""Recover C definitions from preserved event allocations, without reallocating IDs."""
import argparse
import ast
import json
from pathlib import Path


def literals(path):
    result = {}
    for node in ast.parse(path.read_text(encoding='utf-8')).body:
        if isinstance(node, ast.Assign):
            try:
                value = ast.literal_eval(node.value)
            except (ValueError, TypeError):
                continue
            for target in node.targets:
                if isinstance(target, ast.Name):
                    result[target.id] = value
    return result


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--source-tools', required=True)
    parser.add_argument('--output', required=True)
    args = parser.parse_args()
    tools = Path(args.source_tools)
    constants = literals(tools / 'build_hoenn_scripts.py')
    trainers = literals(tools / 'build_hoenn_trainers.py')
    allocations = json.loads((tools / 'hoenn_scripts.json').read_text(encoding='utf-8'))
    definitions = {}

    def add_definition(name, value):
        if type(value) is not int or not 0 <= value <= 65535:
            raise ValueError(('Invalid halfword ID', name, value))
        if name in definitions:
            raise ValueError(('Duplicate definition name', name))
        definitions[name] = value

    for names, allocation in [('C_FLAGS', 'flags'), ('C_VARS', 'vars')]:
        for name, original in constants[names].items():
            value = allocations[allocation][original]
            add_definition(name, value)
    for names, lower, upper in [('RESERVED_FLAGS', 'HOENN_FLAG_END', 'VISIT_FLAG_FIRST'),
                                 ('RESERVED_VARS', 'HOENN_VAR_END', 'VAR_RESERVED_LAST')]:
        reserved_ids = set()
        for name, value in constants[names].items():
            if type(value) is not int or not 0 <= value <= 65535:
                raise ValueError(('Invalid reserved halfword ID', name, value))
            allowed = constants[lower] <= value < constants[upper]
            if names == 'RESERVED_VARS':
                allowed |= constants['VAR_RESERVED_HIGH_FIRST'] <= value < constants['VAR_RESERVED_HIGH_END']
            if not allowed or value in reserved_ids or value in allocations['flags' if names == 'RESERVED_FLAGS' else 'vars'].values():
                raise ValueError(('Reserved ID overlaps or is out of range', name, value))
            reserved_ids.add(value)
            add_definition(name, value)
    add_definition('HOENN_FLAG_CAN_GO_HOENN', int(constants['FERRY_SEVII_FLAG'], 0))
    pairs = sorted((base, name) for name, (base, _) in trainers['LEAGUE_REMATCH'].items())
    pairs = [(allocations['trainers'][base], allocations['trainers'][name]) for base, name in pairs]
    for pair in pairs:
        if any(type(value) is not int or not 0 <= value <= 65535 for value in pair):
            raise ValueError(('Invalid rematch trainer IDs', pair))
    lines = ['// Recovered by scripts/recover_hoenn_flags.py from preserved event allocations.',
             '// Regenerate together with event scripts if allocations change.', '#pragma once', '']
    lines += ['#define %s 0x%04X' % pair for pair in sorted(definitions.items())]
    lines += ['#define HOENN_LEAGUE_REMATCH_COUNT %d' % len(pairs),
              '#define HOENN_LEAGUE_REMATCH_TABLE {%s}' % ', '.join('{%d, %d}' % pair for pair in pairs), '']
    Path(args.output).write_text('\n'.join(lines), encoding='utf-8')
    print('%d definitions and %d rematch pairs recovered from existing allocations.' % (len(definitions), len(pairs)))


if __name__ == '__main__':
    main()
