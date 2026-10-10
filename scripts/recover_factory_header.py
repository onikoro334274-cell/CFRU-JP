"""Recover the factory style header using its original generator and pinned public data."""
import ast
import hashlib
import io
import json
import os
from pathlib import Path
import re
import subprocess
import tempfile
import urllib.request

ROOT = Path(__file__).resolve().parent.parent


def main():
    generator = ROOT.parent / 'PokemonStart-src/tools/frontier/build_frontier_factory.py'
    tree = ast.parse(generator.read_text(encoding='utf-8'))
    functions = {'write', 'c_array', 'move_ids', 'style_lists', 'build_data'}
    selected = [node for node in tree.body if
                isinstance(node, ast.FunctionDef) and node.name in functions or
                isinstance(node, ast.Assign) and any(isinstance(t, ast.Name) and t.id == 'STYLE_LISTS' for t in node.targets)]
    assert len(selected) == 6
    manifest = ROOT / 'audit/factory_header_recovery.json'
    if manifest.exists():
        commit = json.loads(manifest.read_text(encoding='utf-8'))['upstream_commit']
    else:
        commit = subprocess.check_output(['git', 'ls-remote', 'https://github.com/pret/pokeemerald.git', 'HEAD'], text=True).split()[0]
    assert re.fullmatch(r'[0-9a-f]{40}', commit)
    inputs = {}
    with tempfile.TemporaryDirectory() as temp:
        directory = Path(temp)
        for relative in ('src/battle_factory.c', 'include/constants/moves.h'):
            url = f'https://raw.githubusercontent.com/pret/pokeemerald/{commit}/{relative}'
            data = urllib.request.urlopen(url, timeout=30).read()
            target = directory / relative
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_bytes(data)
            inputs[relative] = {'url': url, 'sha256': hashlib.sha256(data).hexdigest()}
        output = ROOT / 'src/hoenn_factory_data.h'
        namespace = {'io': io, 'os': os, 're': re, 'EM': str(directory),
                     'OUT_DATA': str(output), 'REPO': str(ROOT)}
        exec(compile(ast.Module(body=selected, type_ignores=[]), str(generator), 'exec'), namespace)
        lists, counts = namespace['style_lists']()
        # The generation source checks Gen3 IDs; also verify every named move against this CFRU.
        local_moves = (ROOT / 'include/constants/moves.h').read_text(encoding='utf-8')
        local_ids = {name: int(value, 0) for name, value in re.findall(
            r'#define\s+(MOVE_\w+)\s+(0x[0-9A-Fa-f]+|\d+)\b', local_moves)}
        normalized = {name.replace('_', ''): value for name, value in local_ids.items()}
        assert len(normalized) == len(local_ids)
        for _, names, values in lists:
            for name, value in zip(names, values):
                key = name.replace('_', '')
                assert normalized[key] == value, (name, value, normalized.get(key))
        namespace['build_data']()
    report = {'result': 'generated', 'upstream_commit': commit, 'inputs': inputs,
              'generator': str(generator), 'generator_sha256': hashlib.sha256(generator.read_bytes()).hexdigest(),
              'output': 'src/hoenn_factory_data.h', 'output_sha256': hashlib.sha256(output.read_bytes()).hexdigest(),
              'style_lists': len(lists), 'required_move_counts': counts,
              'scope': 'Original generator data-only functions; pinned public pokeemerald source, all move IDs checked against current CFRU. No ROM/graphics extraction or placeholder data.',
              'limitations': 'Factory graphics/script assets and complete linked game remain unverified.'}
    manifest.write_text(json.dumps(report, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')


if __name__ == '__main__':
    main()
