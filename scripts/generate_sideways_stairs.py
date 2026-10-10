#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""斜めの階段（Spherical Ice の「[FR] Sideways Stairs」を CFRU の形で書き直したもの）の、動きの表の素の分を写す（2026-09-29）。

    python scripts/generate_sideways_stairs.py --rom <BPRJ.gba>

→ CFRU の src/Tables/sideways_stairs_table.h（生成物・手で編集しないこと）
   素の動きの表（日本語版 0836A644・0xAA 個）の中身をそのまま並べる。src/sideways_stairs.c が 0xE2 個に広げた表を作り、
   repoints で表を指す 3 か所（08063E00・08063E44・08063E74）を付け替える
★ 番地の確かめ: 素の ROM で、表を指す 3 か所の語が 0836A644 で、表の 0xAA 個がどれも「段の関数の配列」（0x083xxxxx → 0x0806xxxx の関数）であること
"""
import argparse
import os
import struct
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.dirname(os.path.dirname(HERE))
CFRU = os.path.dirname(HERE)
OUT = os.path.join(CFRU, 'src', 'Tables', 'sideways_stairs_table.h')
TABLE = 0x0836A644                      # sMovementActionFuncs（英語版 083A65BC・pokefirered の形で探した）
TABLE_REFS = (0x08063E00, 0x08063E44, 0x08063E74)
COUNT = 0xAA
NOT_ON_BIKE_FUNCS = 0x0831F89C          # sPlayerNotOnBikeFuncs（英語版 0835B814）: 止まる／その場で向く／歩く
PLAYER_NOT_ON_BIKE_MOVING = 0x0805B291


def main():
    parser = argparse.ArgumentParser(description='Generate the verified BPRJ movement pointer table.')
    parser.add_argument('--rom', required=True)
    parser.add_argument('--output', default=OUT)
    args = parser.parse_args()
    d = open(args.rom, 'rb').read()
    if len(d) < 0x400000 or d[0xAC:0xB0] != b'BPRJ' or d[0xBC] != 0:
        raise SystemExit('Expected a BPRJ revision 0 ROM of at least 4 MiB')
    u32 = lambda a: struct.unpack_from('<I', d, a - 0x08000000)[0]
    for r in TABLE_REFS:
        if u32(r) != TABLE:
            raise SystemExit('★ 素の ROM の %08X が %08X（%08X のはず）' % (r, u32(r), TABLE))
    if u32(NOT_ON_BIKE_FUNCS + 8) != PLAYER_NOT_ON_BIKE_MOVING:
        raise SystemExit('★ 素の ROM の歩きの振り分けの表が違う')
    ents = []
    for k in range(COUNT):
        a = u32(TABLE + 4 * k)
        f = u32(a)
        if not (0x08300000 <= a < 0x08400000 and 0x08060000 <= f < 0x08070000 and f & 1):
            raise SystemExit('★ 動きの表の %02X 番が「段の関数の配列」ではない: %08X → %08X' % (k, a, f))
        ents.append(a)
    L = ['/* 斜めの階段: 素の動きの表（日本語版 0x%08X・0x%X 個）の中身。scripts/generate_sideways_stairs.py が生成・手で編集しないこと */' % (TABLE, COUNT),
         '#pragma once', '', '#define SIDEWAYS_VANILLA_MOVEMENT_ACTIONS 0x%X' % COUNT, '',
         '#define SIDEWAYS_VANILLA_MOVEMENT_ACTION_TABLE \\']
    for i in range(0, COUNT, 4):
        L.append('\t' + ' '.join('(const void*) 0x%08X,' % a for a in ents[i:i + 4]) + ' \\')
    L[-1] = L[-1].rstrip(' \\').rstrip(',')        # 使う側で「, 続き」と書くので最後の読点は付けない
    L.append('')
    txt = '\n'.join(L) + '\n'
    if not os.path.exists(args.output) or open(args.output, encoding='utf-8').read() != txt:
        open(args.output, 'w', encoding='utf-8', newline='\n').write(txt)
    print('斜めの階段: 素の動きの表 %d 個 → %s' % (COUNT, os.path.relpath(args.output, CFRU)))
    return 0


if __name__ == '__main__':
    sys.exit(main())
