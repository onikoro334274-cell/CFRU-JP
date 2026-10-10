#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""エメラルドの技教え（30 技）の「覚えられるか」の表を、全国図鑑の番号で並べ直す（2026-10-07）。

ユーザー「エメラルドの教え技が原作と違う CFRU の影響」。ホウエンのスクリプトはエメラルドの番号 + 0x40 を渡し
（tools/rom/em_script.py の HOENN_TUTOR_BASE）、src/hack_vanilla_tutor.c がエメラルドの技に読み替える。
技が DPE の表にあれば DPE の表（全世代）、無ければこの表（第 1〜3 世代）で覚えられるかを決める。

材料: pokeemerald src/data/pokemon/tutor_learnsets.h（gTutorMoves と sTutorLearnsets）・species.h・pokedex.h（名前で突き合わせ）
出力: CFRU-JP-main/src/Tables/hack_emerald_tutor_compat.h（技 30 個の番号と、全国図鑑 0〜386 の u32・bit n = エメラルドの技教え n 番）
"""
import argparse
import os
import re
import sys

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..'))
EM = os.path.join(ROOT, '参考', 'pokeemerald')
CFRU = ROOT
OUT = os.path.join(CFRU, 'src', 'Tables', 'hack_emerald_tutor_compat.h')


def read(*p):
    return open(os.path.join(*p), encoding='utf-8').read()


def build():
    pm = read(EM, 'include', 'constants', 'party_menu.h')
    idx = {m.group(1): int(m.group(2)) for m in re.finditer(r'#define TUTOR_MOVE_(\w+)\s+(\d+)', pm)}
    idx.pop('COUNT', None)
    if len(idx) != 30:
        sys.exit('エメラルドの技教えが %d 個（30 のはず）' % len(idx))
    tl = read(EM, 'src', 'data', 'pokemon', 'tutor_learnsets.h')
    moves = {}
    for m in re.finditer(r'\[TUTOR_MOVE_(\w+)\]\s*=\s*MOVE_(\w+)', tl):
        moves[idx[m.group(1)]] = m.group(2)
    if sorted(moves) != list(range(30)):
        sys.exit('gTutorMoves を読めない')
    # CFRU の技の名前（MOVE_DYNAMICPUNCH のように _ が無い）
    cm = read(CFRU, 'include', 'constants', 'moves.h')
    cfru = {m.group(1): m.group(1) for m in re.finditer(r'#define MOVE_(\w+)\s', cm)}
    names = []
    for i in range(30):
        n = moves[i].replace('_', '')
        if n not in cfru:
            sys.exit('CFRU に MOVE_%s が無い' % n)
        names.append('MOVE_' + n)
    sp = {m.group(1): int(m.group(2)) for m in re.finditer(r'^#define SPECIES_(\w+)\s+(\d+)\s*$',
                                                             read(EM, 'include', 'constants', 'species.h'), re.M)}
    dex = read(EM, 'include', 'constants', 'pokedex.h')
    body = dex[dex.index('NATIONAL_DEX_NONE'):]
    nat = {n: i for i, n in enumerate(re.findall(r'NATIONAL_DEX_(\w+)', body[:body.index('}')]))}
    move_to_idx = {moves[i]: i for i in range(30)}
    table = [0] * 387
    hits = 0
    seen_dex = set()
    body = tl[tl.index('sTutorLearnsets'):]
    for m in re.finditer(r'\[SPECIES_(\w+)\]\s*=\s*(.*?)(?=\n\s*\[SPECIES_|\n};)', body, re.S):
        name, expr = m.group(1), m.group(2)
        bits = 0
        for mv in re.findall(r'TUTOR\(MOVE_(\w+)\)', expr):
            bits |= 1 << move_to_idx[mv]
        if name in nat and 1 <= nat[name] <= 386:
            if nat[name] in seen_dex:
                raise ValueError('Duplicate national dex entry: ' + name)
            seen_dex.add(nat[name])
            table[nat[name]] = bits
            hits += 1
    if hits != 386 or seen_dex != set(range(1, 387)):
        sys.exit('種族と全国図鑑の突き合わせが %d 件（386 のはず）' % hits)
    lines = ['/* 生成物: tools/tutor/build_emerald_tutor.py（手で直さない）',
             '   エメラルドの技教え 30 技と「覚えられるか」（pokeemerald の sTutorLearnsets を全国図鑑の番号で並べ直した）。',
             '   bit n = エメラルドの技教え n 番。src/hack_vanilla_tutor.c */',
             '#define NUM_EMERALD_TUTORS 30',
             'static const u16 sHackEmeraldTutorMoves[NUM_EMERALD_TUTORS] =', '{']
    for i in range(0, 30, 5):
        lines.append('\t' + ' '.join('%s,' % n for n in names[i:i + 5]))
    lines += ['};', 'static const u32 sHackEmeraldTutorCompat[387] =', '{']
    for i in range(0, 387, 6):
        lines.append('\t' + ' '.join('0x%08X,' % v for v in table[i:i + 6]))
    lines.append('};')
    text = '\n'.join(lines) + '\n'
    old = open(OUT, encoding='utf-8').read() if os.path.exists(OUT) else None
    if old != text:
        open(OUT, 'w', encoding='utf-8', newline='\n').write(text)
    print('OK: 技 30・%d 種（全国図鑑 1〜386）→ %s' % (hits, os.path.relpath(OUT, ROOT)))
    print('  ' + ' '.join(names))
    return names, table


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--reference', required=True)
    parser.add_argument('--output', default=OUT)
    args = parser.parse_args()
    EM = args.reference
    OUT = args.output
    build()
