#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""ラティ兄妹がホウエンを徘徊するための「場所の表」を作る（2026-09-29）。

ユーザー「ホウエンリーグ制覇時にフラグをたててラティ兄妹2匹を徘徊させる」。
**読み取りのみ。** CFRU の `src/Tables/hoenn_roamer_locations.h` を出すだけ。

    .venv/Scripts/python.exe scripts/generate_hoenn_roamers.py

出典: `参考/pokeemerald/src/roamer.c` の `sRoamerLocations`（1 行 6 マス・先頭が今いる道路・残りが次に動ける道路・
`___` = MAP_UNDEFINED が終わり）と `data/maps/map_groups.json`（群 0 の並び = 地図の番号）。
CFRU の表（`src/roamer.c`）は 1 行 7 マス・終わりは 0xFF・最後の行は全部 0xFF なので、その形にする。
エメラルドの群 g はこちらの群 43 + g（`build_hoenn.GROUP_BASE`）。道路はエメラルドの群 0 → **43**。
"""
import argparse
import io
import json
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(HERE))
EM = os.path.join(ROOT, '参考', 'pokeemerald')
OUT = os.path.join(ROOT, 'CFRU-Builder v1.01', 'CFRU-JP-main', 'src', 'Tables', 'hoenn_roamer_locations.h')
sys.path.insert(0, HERE)

CFRU_SET = 7


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--reference', required=True)
    parser.add_argument('--map-bank', type=int, required=True)
    parser.add_argument('--output', required=True)
    args = parser.parse_args()
    if not 0 <= args.map_bank < 256:
        raise ValueError('Map bank must fit in a byte')
    EM = args.reference
    OUT = args.output
    groups = json.load(io.open(os.path.join(EM, 'data', 'maps', 'map_groups.json'), encoding='utf-8'))
    g0name = groups['group_order'][0]
    nums = {}
    for i, name in enumerate(groups[g0name]):
        mj = json.load(io.open(os.path.join(EM, 'data', 'maps', name, 'map.json'), encoding='utf-8'))
        nums[mj['id']] = i
    src = io.open(os.path.join(EM, 'src', 'roamer.c'), encoding='utf-8').read()
    m = re.search(r'sRoamerLocations\[\]\[(\d+)\]\s*=\s*\{(.*?)\n\};', src, re.S)
    width = int(m.group(1))
    if width > CFRU_SET or width < 2:
        raise ValueError('Unsupported source row width')
    rows = []
    for line in re.findall(r'\{([^{}]*)\}', m.group(2)):
        cells = [c.strip() for c in line.split(',') if c.strip()]
        if len(cells) != width:
            raise SystemExit('★ 行の幅が %d ではない: %s' % (width, line))
        row = []
        for c in cells:
            if c == '___':
                row.append(0xFF)
                continue
            mm = re.match(r'MAP_NUM\((MAP_\w+)\)', c)
            if not mm or mm.group(1) not in nums:
                raise SystemExit('★ 群 0 に無い地図: %s' % c)
            row.append(nums[mm.group(1)])
        # ★★ エメラルドの表の最後は全部 ___ の終わりの行。**写さない**（CFRU の形の終わりの行は下で 1 つだけ足す）。
        #   2026-09-29 に写してしまい、全部 0xFF の行が表の中にまぎれた → 離れた道路へ飛ぶ（1/16）とき居場所が 0xFF になり、
        #   次の RoamersMove がその行から行き先を引き続けて止まった（ユーザー「バトルフロンティアのマップの区切りでフリーズ」）
        if all(v == 0xFF for v in row):
            continue
        if row[0] == 0xFF:
            raise SystemExit('★ 先頭が ___ の行: %s' % line)
        rows.append(row + [0xFF] * (CFRU_SET - width))
    starts = [row[0] for row in rows]
    if len(set(starts)) != len(starts) or not rows:
        raise ValueError('Missing or duplicate starting routes')
    if any(value not in starts for row in rows for value in row[1:] if value != 0xFF):
        raise ValueError('A destination route has no movement row')
    if any(value >= 255 for value in nums.values()):
        raise ValueError('Map numbers collide with terminator')
    group = args.map_bank
    L = ['/* ラティ兄妹がホウエンを徘徊する場所の表。scripts/generate_hoenn_roamers.py が生成。手で編集しないこと。',
         '   出典: pokeemerald src/roamer.c sRoamerLocations（エメラルドの群 0 の地図の番号）。',
         '   形は CFRU の sRoamerLocations と同じ（1 行 %d マス・先頭が今いる道路・0xFF が終わり・最後の行は全部 0xFF）*/' % CFRU_SET,
         '#pragma once', '',
         '#define HOENN_ROAMING_MAP_BANK %d   /* エメラルドの群 0（道路）→ 43 + 0 */' % group,
         '#define HOENN_ROAMER_LEVEL     40   /* エメラルドの徘徊（src/roamer.c: ROAMER->level = 40） */', '',
         'static const u8 sHoennRoamerLocations[][%d] =' % CFRU_SET, '{']
    inv = {v: k for k, v in nums.items()}
    for r in rows:
        L.append('\t{%s},\t/* %s */' % (', '.join('0x%02X' % v for v in r),
                                          ' '.join(inv[v].replace('MAP_', '') for v in r if v != 0xFF)))
    L.append('\t{%s},' % ', '.join(['0xFF'] * CFRU_SET))
    L += ['};', '']
    text = '\n'.join(L)
    old = io.open(OUT, encoding='utf-8').read() if os.path.exists(OUT) else None
    if old != text:
        with io.open(OUT, 'w', encoding='utf-8', newline='\n') as f:
            f.write(text)
    print('ホウエンの徘徊の場所の表: %d 行（群 %d）→ %s' % (len(rows), group, os.path.relpath(OUT, ROOT)))
    return 0


if __name__ == '__main__':
    sys.exit(main())
