"""Validate every shuffled encounter slot against its current original table."""
import collections
import hashlib
import json
from pathlib import Path
import re
import struct
import argparse

ROOT = Path(__file__).resolve().parent.parent


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--rom', required=True)
    rom = Path(parser.parse_args().rom).read_bytes()
    assert len(rom) == 0x1000000 and rom[0xAC:0xB0] == b'BPRJ' and rom[0xBC] == 0
    path = ROOT / 'src/Tables/wild_encounter_tables.c'
    source = re.sub(r'/\*.*?\*/|//[^\n]*', '', path.read_text(encoding='utf-8'), flags=re.S)
    species_text = (ROOT / 'include/constants/species.h').read_text(encoding='utf-8')
    species = {name: int(value, 0) for name, value in re.findall(
        r'^#define\s+(SPECIES_\w+)\s+(0x[0-9A-Fa-f]+|\d+)\b', species_text, re.M)}
    last = re.search(r'^#define NUM_SPECIES\s+\((SPECIES_\w+)\s*\+\s*1\)', species_text, re.M)[1]
    limit = species[last] + 1
    arrays = {}
    for name, body in re.findall(r'(?:static\s+)?const struct WildPokemon (\w+)\[\]\s*=\s*\{(.*?)\};', source, re.S):
        rows = [tuple(x.strip() for x in row.split(',')) for row in re.findall(r'\{([^{}]+)\}', body)]
        assert rows and all(len(row) == 3 for row in rows), name
        arrays[name] = rows
    infos = {name: (int(rate, 0), array) for name, rate, array in re.findall(
        r'(?:static\s+)?const struct WildPokemonInfo (\w+)\s*=\s*\{\s*(\d+|0x[0-9A-Fa-f]+)\s*,\s*(\w+)\s*\};', source)}
    pairs = re.search(r'gHackWildShufflePairs\[\]\s*=\s*\{(.*?)\};', source, re.S)[1]
    rows = [tuple(x.strip() for x in row.split(',')) for row in re.findall(r'\{([^{}]+)\}', pairs)]
    assert len(rows) == int(re.search(r'gHackWildShufflePairCount\s*=\s*(\d+)', source)[1])
    checked_slots = 0
    lengths = collections.Counter()
    for original, shuffled, region, variant in rows:
        shuffled = shuffled.lstrip('&')
        new_rate, new_array = infos[shuffled]
        if original.startswith('&'):
            original_rate, original_array = infos[original[1:]]
            old = arrays[original_array]
        else:
            address = int(re.search(r'0x[0-9A-Fa-f]+', original)[0], 16) - 0x08000000
            assert 0 <= address <= len(rom) - 8
            original_rate = rom[address]
            pointer = struct.unpack_from('<I', rom, address + 4)[0] - 0x08000000
            count = len(arrays[new_array])
            assert 0 <= pointer <= len(rom) - count * 4
            old = [tuple(str(x) for x in struct.unpack_from('<BBH', rom, pointer + i * 4)) for i in range(count)]
        assert original_rate == new_rate and 0 < new_rate <= 100, shuffled
        new = arrays[new_array]
        assert len(old) == len(new), shuffled
        expected_count = len(new) if not original.startswith('&') else 12 if '_Land_' in original else 10 if '_Fish' in original else 5
        assert len(new) == expected_count, shuffled
        for before, after in zip(old, new):
            assert tuple(int(x, 0) for x in after[:2]) == tuple(int(x, 0) for x in before[:2]), (shuffled, before, after)
            low, high, sid = (int(value, 0) for value in after)
            assert 1 <= low <= high <= 200, (shuffled, after)
            assert 0 < sid < limit and sid != species['SPECIES_EGG'], (shuffled, after)
            checked_slots += 1
        lengths[len(new)] += 1
    report = {'result': 'passed', 'pair_entries': len(rows), 'encounter_slots': checked_slots,
              'table_slot_counts': dict(lengths), 'species_limit': limit,
              'source_sha256': hashlib.sha256(path.read_bytes()).hexdigest(),
              'scope': 'All generated shuffle targets resolve; encounter rate, slot counts and per-slot level ranges match current original tables; species IDs valid/nonempty/nonegg and levels in 1..200.',
              'rom_sha256': hashlib.sha256(rom).hexdigest(),
              'limitations': 'Named originals use checkout sources; numeric originals use supplied BPRJ rev0 ROM. Numeric original slot counts are inferred from targets. Form validity, encounter balance, linked data layout and GBA runtime unverified.'}
    (ROOT / 'audit/wild_shuffle_contents_review.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    print(f'{len(rows)} shuffled tables / {checked_slots} encounter slots passed content checks')


if __name__ == '__main__':
    main()
