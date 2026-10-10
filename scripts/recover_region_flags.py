"""Recover regional badge and league destination definitions from saved metadata."""
import argparse
import ast
import json
from pathlib import Path

from recover_hoenn_flags import literals


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--tools', required=True)
    parser.add_argument('--output-dir', required=True)
    args = parser.parse_args()
    tools = Path(args.tools)
    output = Path(args.output_dir)
    all_flags = set()
    for region, filename in [('johto', 'johto_ids.json'), ('sinnoh', 'sinnoh_gym_ids.json')]:
        data = json.loads((tools / region / filename).read_text(encoding='utf-8'))
        definitions = {region.upper() + '_FLAG_BADGE' + str(i): data['flags']['BADGE' + str(i)] for i in range(1, 9)}
        if region == 'sinnoh':
            maps = literals(tools / region / 'sinnoh_maps.py')
            pc = 'POKEMON_LEAGUE_POKEMON_CENTER'
            tree = ast.parse((tools / region / 'sinnoh_rooms.py').read_text(encoding='utf-8'))
            override = next(n.value for n in tree.body if isinstance(n, ast.Assign)
                            and any(isinstance(t, ast.Name) and t.id == 'PC_OVERRIDE' for t in n.targets))
            entry = next(v for k, v in zip(override.keys, override.values) if ast.literal_eval(k) == pc)
            assert isinstance(entry, ast.Call) and isinstance(entry.func, ast.Name) and entry.func.id == 'dict'
            nurse = next(ast.literal_eval(k.value) for k in entry.keywords if k.arg == 'nurse')
            definitions.update(SINNOH_FLAG_LEAGUE_HOF=data['flags']['LEAGUE_HOF'],
                               SINNOH_LEAGUE_PC_GROUP=maps['SINNOH_GROUP'],
                               SINNOH_LEAGUE_PC_NUM=[m[0] for m in maps['MAPS']].index(pc),
                               SINNOH_LEAGUE_PC_X=nurse[0], SINNOH_LEAGUE_PC_Y=nurse[1] + 2)
        for name, value in definitions.items():
            if not isinstance(value, int) or not 0 <= value <= 65535:
                raise ValueError((name, value))
            if '_FLAG_' in name:
                if value in all_flags:
                    raise ValueError(('Flag collision', name, value))
                all_flags.add(value)
        lines = ['// Recovered by scripts/recover_region_flags.py from preserved event/map metadata.', '#pragma once']
        lines += ['#define %s 0x%X' % pair for pair in definitions.items()]
        (output / (region + '_flags.h')).write_text('\n'.join(lines) + '\n', encoding='utf-8')
        print(region + ': ' + str(len(definitions)) + ' definitions recovered')


if __name__ == '__main__':
    main()
