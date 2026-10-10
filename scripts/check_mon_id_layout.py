"""Check individual-ID storage offsets against actual ARM Pokemon structures."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parent.parent


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--compiler', type=Path, required=True)
    parser.add_argument('--arm-include', type=Path, required=True)
    args = parser.parse_args()
    source = r'''
#include "src/defines.h"
#include "include/pokemon.h"
#include "include/new/pokemon_storage_system.h"
#include "include/constants/items.h"
#define OFFSET(t,f,n) _Static_assert(__builtin_offsetof(struct t,f)==n,#t "." #f)
_Static_assert(sizeof(struct BoxPokemon)==80,"BoxPokemon size");
_Static_assert(sizeof(struct CompressedPokemon)==58,"CompressedPokemon size");
OFFSET(BoxPokemon,teratype,0x11);OFFSET(BoxPokemon,language,0x12);
OFFSET(BoxPokemon,markings,0x1B);OFFSET(BoxPokemon,substruct0.heldItem,0x22);
OFFSET(BoxPokemon,substruct0.experience,0x24);OFFSET(BoxPokemon,substruct0.pokeball,0x2A);
OFFSET(BoxPokemon,substruct2.cool,0x3E);OFFSET(BoxPokemon,substruct2.beauty,0x3F);
OFFSET(BoxPokemon,substruct2.cute,0x40);OFFSET(BoxPokemon,substruct2.smart,0x41);
OFFSET(BoxPokemon,substruct3,0x44);
OFFSET(CompressedPokemon,teratype,0x11);OFFSET(CompressedPokemon,language,0x12);
OFFSET(CompressedPokemon,sanity,0x13);OFFSET(CompressedPokemon,markings,0x1B);
OFFSET(CompressedPokemon,substruct0.heldItem,0x1E);
OFFSET(CompressedPokemon,substruct0.experience,0x20);
OFFSET(CompressedPokemon,substruct0.pokeball,0x26);
OFFSET(CompressedPokemon,metInfo,0x34);
_Static_assert(ITEMS_COUNT<=1024,"ID carrier overlaps item IDs");
'''
    with tempfile.TemporaryDirectory() as temp:
        c, obj = Path(temp) / 'layout.c', Path(temp) / 'layout.o'
        c.write_text(source, encoding='utf-8')
        command = [str(args.compiler.resolve()), '-mthumb', '-mcpu=arm7tdmi', '-std=gnu11',
                   '-I', str(ROOT), '-isystem', str(args.arm_include.resolve()), '-c', str(c), '-o', str(obj)]
        subprocess.run(command, check=True, cwd=ROOT)
    headers = ['include/pokemon.h', 'include/new/pokemon_storage_system.h', 'include/constants/items.h']
    report = {'result': 'passed', 'scope': 'ARM compilation of actual struct sizes, addressable carrier offsets, ID bytes and item ID capacity.',
              'header_sha256': {name: hashlib.sha256((ROOT / name).read_bytes()).hexdigest() for name in headers},
              'limitations': 'Does not verify unaddressable bitfield allocation, full conversion, save migration, or GBA execution.'}
    (ROOT / 'audit/mon_id_layout_review.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    print('Individual ID ARM layout assertions passed')


if __name__ == '__main__':
    main()
