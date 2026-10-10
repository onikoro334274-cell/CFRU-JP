"""Compile the actual ARM inflate core and check its IWRAM copy constraints."""
import json
import os
import re
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent


def main():
    audit = json.loads((ROOT / 'audit/gnu_c.json').read_text(encoding='utf-8'))
    command = audit['command']
    compiler_dir = Path(command[0]).parent
    env = dict(os.environ)
    env['PATH'] = str(compiler_dir) + os.pathsep + env.get('PATH', '')
    source = (ROOT / 'src/fcomp.c').read_text(encoding='utf-8')
    capacity = int(re.search(r'^#define FC_IWRAM_SIZE\s+(0x[0-9A-Fa-f]+)', source, re.M)[1], 16)
    with tempfile.TemporaryDirectory() as directory:
        obj = Path(directory) / 'fcomp.o'
        subprocess.run(command + ['src/fcomp.c', '-o', str(obj)], cwd=ROOT, env=env, check=True)
        dumper = str(compiler_dir / 'arm-none-eabi-objdump.exe')
        symbols = subprocess.run([dumper, '-t', str(obj)], capture_output=True, text=True, check=True).stdout
        symbol = next(line.split() for line in symbols.splitlines() if line.endswith(' FCompInflateArm'))
        size = int(symbol[-2], 16)
        assert 0 < size <= capacity, 'Inflate core exceeds IWRAM copy size'
        disassembly = subprocess.run([dumper, '-d', '--disassemble=FCompInflateArm', str(obj)],
                                     capture_output=True, text=True, check=True).stdout
        assert not re.search(r'\sblx?\s', disassembly), 'Copied core must not issue relative calls'
        result = {'core_bytes': size, 'copy_capacity_bytes': capacity,
                  'relative_calls': 0, 'compiler_flags': command,
                  'scope': 'ARM object core size and absence of BL/BLX instructions; not linked ROM or IWRAM runtime.'}
    (ROOT / 'audit/fcomp_arm.json').write_text(json.dumps(result, indent=2) + '\n', encoding='utf-8')
    print('FComp ARM core: %d/%d IWRAM bytes, no BL/BLX instructions.' % (size, capacity))


if __name__ == '__main__':
    main()
