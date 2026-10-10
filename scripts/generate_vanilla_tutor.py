#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""素の FRLG の技教え（0〜14 番）の「覚えられるか」の表を、全国図鑑の番号で並べ直す（2026-10-06）。

ユーザー「究極技のわざおしえが参照先ずれているみたい・他にもないか調査修正」。
CFRU（EXPANDED_MOVE_TUTORS）は素の技教えの関数を DPE の 152 個の表に置き換えたので、素のスクリプトの
0〜17 番が DPE の別の技を指していた（src/hack_vanilla_tutor.c が素の番号を読み替える）。
素の 15 技のうち DPE の表に無い 13 技（メガトンパンチ など）は、素の ROM の表を使う。

素の ROM（日本語版）:
  技の表 sTutorMoves        084192F0（u16 × 15）
  覚えられるかの表 sTutorLearnsets  関数 CanLearnTutorMove（081213D8）のリテラル 08121420 が指す（u16 × 412・素の種族番号）
素の種族番号 → 全国図鑑は pokefirered の species.h と pokedex.h を名前で突き合わせる。

出力: CFRU-JP-main/src/Tables/hack_vanilla_tutor_compat.h（全国図鑑 0〜386 の u16・bit n = 素の技教え n 番）
"""
import argparse
import os
import re
import struct
import sys

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..'))
VANILLA = os.path.join(ROOT, 'CFRU-Builder v1.01', 'Pocket Monsters - FireRed (Japan).gba')
PRET = os.path.join(ROOT, 'include', 'constants')
OUT = os.path.join(ROOT, 'src', 'Tables', 'hack_vanilla_tutor_compat.h')

VANILLA_MOVES = [5, 14, 25, 34, 38, 68, 69, 102, 118, 135, 138, 86, 153, 157, 164]   # 確かめ用（メガトンパンチ〜みがわり）
NUM_VANILLA_SPECIES = 412


def species_ids():
    s = open(os.path.join(PRET, 'species.h'), encoding='utf-8').read()
    return {m.group(1): int(m.group(2), 0) for m in re.finditer(r'^#define SPECIES_(\w+)\s+(0x[0-9A-Fa-f]+|\d+)\s*$', s, re.M)}


def national_ids():
    s = open(os.path.join(PRET, 'pokedex.h'), encoding='utf-8').read()
    return {m.group(1): int(m.group(2), 0) for m in re.finditer(r'^#define NATIONAL_DEX_(\w+)\s+(0x[0-9A-Fa-f]+|\d+)\s*$', s, re.M)}


def build(rom_path, output=OUT):
    rom = open(rom_path, 'rb').read()
    if len(rom) < 0x420000 or rom[0xAC:0xB0] != b'BPRJ' or rom[0xBC] != 0:
        raise ValueError('Expected BPRJ revision 0 ROM')
    moves = [struct.unpack_from('<H', rom, 0x4192F0 + 2 * i)[0] for i in range(15)]
    if moves != VANILLA_MOVES:
        sys.exit('素の技教えの表が想定と違う: %s' % moves)
    lit = struct.unpack_from('<I', rom, 0x121420)[0]
    if lit != 0x0841930E:
        sys.exit('覚えられるかの表の番地が想定と違う: %X' % lit)
    learn = [struct.unpack_from('<H', rom, lit - 0x08000000 + 2 * i)[0] for i in range(NUM_VANILLA_SPECIES)]

    sp, nat = species_ids(), national_ids()
    table = [0] * 387
    hits = 0
    for name, sid in sp.items():
        if 1 <= sid < NUM_VANILLA_SPECIES and name in nat and 1 <= nat[name] <= 386:
            table[nat[name]] = learn[sid] & 0x7FFF
            hits += 1
    if hits != 386:
        sys.exit('種族と全国図鑑の突き合わせが %d 件（386 のはず）' % hits)

    lines = ['/* 生成物: tools/tutor/build_vanilla_tutor.py（手で直さない）',
             '   素の FRLG の技教え 0〜14 番の「覚えられるか」（素の ROM の sTutorLearnsets を全国図鑑の番号で並べ直した）。',
             '   bit n = 素の技教え n 番（0 メガトンパンチ … 14 みがわり）。src/hack_vanilla_tutor.c */',
             'static const u16 sHackVanillaTutorCompat[387] =', '{']
    for i in range(0, 387, 8):
        lines.append('\t' + ' '.join('0x%04X,' % v for v in table[i:i + 8]))
    lines.append('};')
    text = '\n'.join(lines) + '\n'
    old = open(output, encoding='utf-8').read() if os.path.exists(output) else None
    if old != text:
        open(output, 'w', encoding='utf-8', newline='\n').write(text)
    print('OK: %d 種（全国図鑑 1〜386）→ %s' % (hits, os.path.relpath(OUT, ROOT)))
    return table


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--rom', required=True)
    parser.add_argument('--output', default=OUT)
    args = parser.parse_args()
    build(args.rom, args.output)
