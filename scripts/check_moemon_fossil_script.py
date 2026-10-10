"""Execute fossil script result routing with removeitem clobbering LASTRESULT."""
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent


def run(source, result):
    lines = [line.split('@', 1)[0].strip() for line in source.splitlines()]
    labels = {line[:-1]: i for i, line in enumerate(lines) if line.endswith(':')}
    pc = lines.index('callasm HackMoemonFossil_Revive') + 1
    values = {'LASTRESULT': result}
    equal = False
    removed = 0
    messages = []
    for _ in range(100):
        line = lines[pc]
        pc += 1
        parts = line.split()
        if not parts:
            continue
        def value(text):
            return values[text] if text in values else int(text, 0)
        if parts[0] == 'compare':
            equal = value(parts[1]) == value(parts[2])
        elif parts[0] == 'if' and equal:
            pc = labels[parts[3]] + 1
        elif parts[0] == 'copyvar':
            values[parts[1]] = value(parts[2])
        elif parts[0] == 'removeitem':
            removed += int(parts[2])
            values['LASTRESULT'] = 1
        elif parts[0] == 'msgbox':
            messages.append(parts[1])
        elif parts[0] == 'end':
            return removed, messages
    raise RuntimeError('Script failed to terminate')


def main():
    source = (ROOT / 'assembly/kanto/kanto_moemon_fossil.s').read_text(encoding='utf-8')
    for result in range(3):
        removed, messages = run(source, result)
        assert removed == (0 if result == 2 else 1)
        assert ('gText_MoeFossil_ToPC' in messages) == (result == 1)
        assert ('gText_MoeFossil_Full' in messages) == (result == 2)
    original = source.replace('copyvar 0x8001 LASTRESULT\n\tremoveitem ITEM_MOEMON_FOSSIL 1',
                              'removeitem ITEM_MOEMON_FOSSIL 1\n\tcopyvar 0x8001 LASTRESULT')
    assert 'gText_MoeFossil_ToPC' in run(original, 0)[1]
    report = {'result': 'passed', 'delivery_results': [0, 1, 2], 'baseline_party_routing_failure_reproduced': True,
              'rom_evidence': 'Original BPRJ removeitem handler 0x08069F1C calls RemoveBagItem at 0x08069F4A and stores its return through the 0x02037004 literal at 0x08069F5C.',
              'limitations': 'Limited script interpreter; does not emulate complete script VM, inventory races, full revival or GBA runtime.'}
    (ROOT / 'audit/moemon_fossil_script_review.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    print('Fossil party/PC/full routing passed; original party-result overwrite reproduced')


if __name__ == '__main__':
    main()
