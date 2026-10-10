"""Compile real timestamp consumers and inspect ARM stores and packed layout."""
import hashlib
import json
import os
import re
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent


def main():
    command = json.loads((ROOT / 'audit/gnu_c.json').read_text(encoding='utf-8'))['command']
    compiler = Path(command[0])
    suffix = '.exe' if compiler.suffix.lower() == '.exe' else ''
    objdump = compiler.with_name('arm-none-eabi-objdump' + suffix)
    objcopy = compiler.with_name('arm-none-eabi-objcopy' + suffix)
    env = os.environ.copy()
    env['PATH'] = str(compiler.parent) + os.pathsep + env.get('PATH', '')
    source = (ROOT / 'src/scripting.c').read_text(encoding='utf-8')
    structure = re.search(r'^struct [^\n]*DailyEventVar\n\{[\s\S]*?\n\};', source, re.M)[0]
    functions = {}
    with tempfile.TemporaryDirectory() as directory:
        path = Path(directory)
        obj = path / 'scripting.o'
        result = subprocess.run(command + ['src/scripting.c', '-o', str(obj)], cwd=ROOT, env=env,
                                capture_output=True, text=True, check=True)
        assert not result.stdout and not result.stderr, 'Unexpected compiler diagnostics'
        for name in ('CheckAndSetDailyEvent', 'UpdateTimeInVars'):
            disassembly = subprocess.run([str(objdump), '-d', '--disassemble=' + name, str(obj)],
                                         env=env, capture_output=True, text=True, check=True).stdout
            assert '<' + name + '>:' in disassembly
            halfword_stores = re.findall(r'\bstrh\s+[^\n]+', disassembly)
            assert len(halfword_stores) >= 2, (name, 'timestamp halfword stores not found')
            word_stores = re.findall(r'\bstr(?:\.n|\.w)?\s+[^\n]+', disassembly)
            assert all('[sp' in store for store in word_stores), (name, word_stores)
            functions[name] = disassembly.replace(str(obj), '<current scripting.o>')
        probe_source = path / 'layout.c'
        probe_source.write_text('typedef unsigned int u32;\n' + structure + '''
_Static_assert(sizeof(struct DailyEventVar)==4,"timestamp size");
_Static_assert(_Alignof(struct DailyEventVar)==2,"timestamp alignment");
const struct DailyEventVar probe={.minute=59,.hour=23,.day=31,.month=12,.year=99,.century=20};
''', encoding='utf-8')
        probe_obj, data = path / 'layout.o', path / 'layout.bin'
        subprocess.run(command + [str(probe_source), '-o', str(probe_obj)], env=env, check=True)
        subprocess.run([str(objcopy), '-O', 'binary', '--only-section=.rodata', str(probe_obj), str(data)],
                       env=env, check=True)
        packed = 59 | (23 << 6) | (31 << 11) | (12 << 16) | (99 << 20) | (20 << 27)
        assert data.read_bytes() == packed.to_bytes(4, 'little')
    report = {
        'flags': command, 'source_sha256': hashlib.sha256((ROOT / 'src/scripting.c').read_bytes()).hexdigest(),
        'functions': functions,
        'scope': 'Current GNU ARM object timestamp stores: halfword stores found, no non-stack word stores in both consumers. No linked GBA runtime claim.',
        'bit_layout_probe': {'bytes_hex': packed.to_bytes(4, 'little').hex(), 'expected_packed_u32': packed,
                             'method': 'Actual source struct compiled with size/alignment assertions; objcopy .rodata bytes compared'},
    }
    (ROOT / 'audit/daily_event_arm_disassembly.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    print('Current ARM timestamp consumers use halfword stores; four-byte bit layout and two-byte alignment pass.')


if __name__ == '__main__':
    main()
