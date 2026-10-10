"""Recover shop stock/prices with original rules and explicit read-only inputs."""
import argparse
import contextlib
import hashlib
import importlib.util
import io
import json
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source-tools', type=Path, required=True)
    parser.add_argument('--dpe', type=Path, required=True)
    parser.add_argument('--rom', type=Path, required=True)
    parser.add_argument('--check', action='store_true')
    args = parser.parse_args()
    source_tools, dpe, rom = args.source_tools.resolve(), args.dpe.resolve(), args.rom.resolve()
    data = rom.read_bytes()
    assert len(data) == 16 * 1024 * 1024 and data[0xAC:0xB0] == b'BPRJ' and data[0xBC] == 0, 'Expected BPRJ revision 0 original ROM'
    generator = source_tools / 'megas/build_mega_shop.py'
    spec = importlib.util.spec_from_file_location('original_mega_shop', generator)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    module.CFRU = module.W.CFRU = str(ROOT)
    module.W.DPE = str(dpe)
    module.W.K.WILD_C = str(ROOT / 'src/Tables/wild_encounter_tables.c')
    module.W.K.BASE_ROM = str(rom)
    # The newer source-tools snapshot lists optional items absent from this branch.
    # Include extras only when the branch defines and implements the item.
    items = module.item_ids()
    item_table = (ROOT / 'src/Tables/item_tables.c').read_text(encoding='utf-8')
    excluded_extras = []
    for group in ('Z_SHOP_EXTRA', 'DANGER_LEAF_SHOP'):
        extras = []
        for name, price in getattr(module, group):
            if name not in items:
                assert name not in item_table, 'Item implementation without item ID: ' + name
                excluded_extras.append(name)
            else:
                assert '.itemId = ' + name + ',' in item_table, 'Missing item implementation: ' + name
                extras.append((name, price))
        assert extras, 'No implemented items in ' + group
        setattr(module, group, extras)
    output = ROOT / 'src/Tables/mega_shop_items.h'
    with tempfile.TemporaryDirectory() as directory:
        module.OUT = str(Path(directory) / output.name)
        with contextlib.redirect_stdout(io.StringIO()):
            assert module.main() == 0
        text = Path(module.OUT).read_text(encoding='utf-8')
    rows = module.build()
    assert rows and len({row[1] for row in rows}) == len(rows), 'Empty or duplicate mega shop stock'
    assert all(price in (module.PRICE, module.CHEAP_PRICE) for _, _, price in rows)
    if args.check:
        assert output.read_text(encoding='utf-8') == text, 'Regenerated header differs'
    else:
        output.write_text(text, encoding='utf-8')
        inputs = [generator, source_tools / 'megas/build_wild_mega_stones.py',
                  source_tools / 'wild_editor/wildlib.py', dpe / 'include/species.h',
                  dpe / 'src/Evolution Table.c', ROOT / 'include/constants/items.h',
                  ROOT / 'include/constants/species.h', ROOT / 'src/Tables/item_tables.c',
                  ROOT / 'src/Tables/wild_encounter_tables.c', rom]
        record = {'method': 'Original stock/price generator main() with redirected output and explicit current CFRU/read-only DPE/original BPRJ inputs.',
                  'mega_stock': len(rows), 'cheap_stones': [name for name, _, price in rows if price == module.CHEAP_PRICE],
                  'extras_absent_from_current_branch': excluded_extras,
                  'inputs': {str(p): hashlib.sha256(p.read_bytes()).hexdigest() for p in inputs},
                  'limitations': 'Generated stock/price metadata; shop script, item pricing and purchase behavior require separate validation.'}
        (ROOT / 'audit/mega_shop_header_recovery.json').write_text(json.dumps(record, indent=2) + '\n', encoding='utf-8')
    print('Mega shop header: %d unique mega stones; %d optional extras absent from this branch excluded.' % (len(rows), len(excluded_extras)))


if __name__ == '__main__':
    main()
