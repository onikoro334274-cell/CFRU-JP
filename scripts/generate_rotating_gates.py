#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""ヒワマキジムの回転門の絵と配置（やりたいこと #2）。

    .venv/Scripts/python.exe tools/fortree/build_rotating_gates.py

## 何を作るか

- 門の絵（4bpp）… ヒワマキが使う **3 種だけ**
- 門のパレット（16 色）
- 門の配置 8 個（エメラルドの `sRotatingGate_FortreePuzzleConfig`）
- 回転のアフィンアニメ 20 本

## ★ ヒワマキが使う形は 3 種だけ

`sRotatingGate_FortreePuzzleConfig` は T2 x3 / T1 x4 / L4 x1。
エメラルドは 8 種全部の絵を積むが、こちらは**使う 3 種だけ**でよい。

| 形 | 大きさ | バイト |
|---|---|---|
| T1 | 32x32 | 512 |
| T2 | 64x64 | 2048 |
| L4 | 64x64 | 2048 |

合計 **4,608 バイト**（エメラルドの 8 種全部だと 13,312）。

## ★★ パレットはこちらで持つ

エメラルドは**パレットを読み込まない** — `paletteTag = TAG_NONE` /
OAM の `paletteNum = 2` で、**野外の NPC パレットの 2 番**に
たまたま入っている色（番号 11/12/13/15）を借りている。

こちらでそれをやると危ない（CLAUDE.md #16: 置き場 2〜5 は素の NPC が
上書きし合う）。**絵が使う色は 11/12/13/15 の 4 つだけ**なので、
PNG のパレットからその色を取り出して**自前の札で読み込む**。
`CreateSprite` が札から置き場を引いてくれるので固定の枠を取らない。

## ★ カメラのフックは要らない

エメラルドが `RotatingGatePuzzleCameraUpdate` で画面外の門を作り消し
するのは**スプライト数の節約**のため（トリックハウスは門が多い）。
**ヒワマキは 8 つだけ**なので、入場時に全部作って放置でよい。
`coordOffsetEnabled` がスクロールに追従する。
"""
import argparse
import json
import io
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(os.path.dirname(HERE), 'rom'))
sys.path.insert(0, os.path.join(os.path.dirname(HERE), 'pcmonitor'))

from indexed_png import read_indexed_png

EM = ''
GATE_SRC = os.path.join(EM, 'src', 'rotating_gate.c')
GFX_DIR = os.path.join(EM, 'graphics', 'rotating_gates')
OUT = os.path.join(os.path.dirname(HERE), 'src', 'Tables', 'hoenn_gates.h')


def map_id(name):
    with open(os.path.join(EM, 'data', 'maps', 'map_groups.json'), encoding='utf-8') as f:
        groups = json.load(f)
    for group, key in enumerate(groups['group_order']):
        if name in groups[key]:
            number = groups[key].index(name)
            if group + 43 >= 256 or number >= 256:
                raise ValueError('Map identifier does not fit in bytes')
            return group, number
    raise ValueError('Missing map: ' + name)

MAP = 'FortreeCity_Gym'
# エメラルドの形の名前 -> PNG の名前（`enum` の並びと同じ）
# ★ enum は **12 個**（うしろ 4 つは使われていない十字形で絵も無い）。
#   添字がずれないよう 12 個そろえる。
SHAPES = ['L1', 'L2', 'L3', 'L4', 'T1', 'T2', 'T3', 'T4',
          'UNUSED_T1', 'UNUSED_T2', 'UNUSED_T3', 'UNUSED_T4']
N_SHAPES = len(SHAPES)
PNG = {'L1': 'l1', 'L2': 'l2', 'L3': 'l3', 'L4': 'l4',
       'T1': 't1', 'T2': 't2', 'T3': 't3', 'T4': 't4'}
# 32x32 の形（残りは 64x64）。エメラルドの `RotatingGate_CreateGate` と同じ
SMALL = {'L1', 'T1'}
ORIENT = {'GATE_ORIENTATION_0': 0, 'GATE_ORIENTATION_90': 1,
          'GATE_ORIENTATION_180': 2, 'GATE_ORIENTATION_270': 3}


def config():
    """ヒワマキの門の配置を decomp から**名前で**読む。"""
    src = io.open(GATE_SRC, encoding='utf-8').read()
    m = re.search(r'sRotatingGate_FortreePuzzleConfig\[\]\s*=\s*\{(.*?)\n\};',
                  src, re.S)
    if not m:
        raise SystemExit('★ ヒワマキの配置が読めない')
    out = []
    for x, y, shape, orient in re.findall(
            r'\{\s*(-?\d+)\s*,\s*(-?\d+)\s*,\s*GATE_SHAPE_(\w+)\s*,'
            r'\s*(GATE_ORIENTATION_\w+)\s*\}', m.group(1)):
        if shape not in PNG:
            raise SystemExit('★ 知らない形: %s' % shape)
        out.append((int(x), int(y), shape, ORIENT[orient]))
    if not out:
        raise SystemExit('★ ヒワマキの門が 0 個')
    return out


def affine_anims():
    """回転のアフィンアニメ 20 本を decomp から読む -> [(名前, [語...])]

    ★ `AFFINEANIMCMD_FRAME(xScale, yScale, rotation, duration)` は
      4 半語。`AFFINEANIMCMD_JUMP(n)` は `0x7FFF, n, 0, 0`、
      `AFFINEANIMCMD_END` は `0x7FFF, 0, 0, 0`… ではなく
      素の `sprite.h` の決まりに従う。ここでは**式のまま**書き出して
      C 側のマクロに任せる（数を作り直さないので読み違えが起きない）。
    """
    src = io.open(GATE_SRC, encoding='utf-8').read()
    m = re.search(r'sSpriteAffineAnimTable_RotatingGate\[\]\s*=\s*\{(.*?)\n\};',
                  src, re.S)
    if not m:
        raise SystemExit('★ アフィンアニメの表が読めない')
    names = re.findall(r'sSpriteAffineAnim_(\w+)', m.group(1))
    out = []
    for n in names:
        mm = re.search(r'sSpriteAffineAnim_%s\[\]\s*=\s*\{(.*?)\n\};' % n,
                       src, re.S)
        if not mm:
            raise SystemExit('★ %s の中身が読めない' % n)
        cmds = []
        for line in mm.group(1).splitlines():
            line = line.strip().rstrip(',')
            if not line:
                continue
            f = re.match(r'AFFINEANIMCMD_FRAME\((.*)\)$', line)
            if f:
                cmds.append(('FRAME', [a.strip() for a in f.group(1).split(',')]))
                continue
            j = re.match(r'AFFINEANIMCMD_JUMP\((.*)\)$', line)
            if j:
                cmds.append(('JUMP', [j.group(1).strip()]))
                continue
            if line == 'AFFINEANIMCMD_END':
                cmds.append(('END', []))
                continue
            raise SystemExit('★ %s に読めない行: %s' % (n, line))
        out.append((n, cmds))
    return out


# 回す向き（decomp の enum）
ROT_NONE, ROT_ACW, ROT_CW = 0, 1, 2
# 腕の向き（decomp の enum）。**世界の方角**であって形の向きではない
ARM = {'GATE_ARM_NORTH': 0, 'GATE_ARM_EAST': 1,
       'GATE_ARM_SOUTH': 2, 'GATE_ARM_WEST': 3}
GATE_ROT_NONE = 255


def _gate_rot(rot, arm, long_arm):
    """decomp の `GATE_ROT(dir, arm, longArm)` と同じ計算"""
    return ((rot & 15) << 4) | ((arm & 7) << 1) | (long_arm & 1)


def rotation_info():
    """押した向きごとの 4x4 の表 -> {'North': [16 個], ...}

    ★ `GATE_ROT_CW(GATE_ARM_WEST, 1)` のようなマクロを**そのまま計算**する。
      値を手で写さない。
    """
    src = io.open(GATE_SRC, encoding='utf-8').read()
    out = {}
    for d in ('North', 'South', 'West', 'East'):
        m = re.search(r'sRotatingGate_RotationInfo%s\[4 \* 4\]\s*=\s*\{(.*?)\n\};'
                      % d, src, re.S)
        if not m:
            raise SystemExit('★ %s の 4x4 の表が読めない' % d)
        vals = []
        for tok in re.findall(r'GATE_ROT_NONE|GATE_ROT_(?:A?CW)\([^)]*\)',
                              m.group(1)):
            if tok == 'GATE_ROT_NONE':
                vals.append(GATE_ROT_NONE)
                continue
            mm = re.match(r'GATE_ROT_(A?CW)\(\s*(\w+)\s*,\s*(\d)\s*\)', tok)
            rot = ROT_ACW if mm.group(1) == 'ACW' else ROT_CW
            vals.append(_gate_rot(rot, ARM[mm.group(2)], int(mm.group(3))))
        if len(vals) != 16:
            raise SystemExit('★ %s の表が %d 個（16 のはず）' % (d, len(vals)))
        out[d] = vals
    return out


def arm_layout():
    """形ごとの腕の有無 -> [8 個] x 8 形（北・東・南・西 x 近い/遠い）"""
    src = io.open(GATE_SRC, encoding='utf-8').read()
    m = re.search(r'sRotatingGate_ArmLayout\[\]\[4 \* 2\]\s*=\s*\{(.*?)\n\};',
                  src, re.S)
    if not m:
        raise SystemExit('★ 腕の表が読めない')
    nums = [int(x) for x in re.findall(r'\b([01])\b', m.group(1))]
    if len(nums) != N_SHAPES * 8:
        raise SystemExit('★ 腕の表が %d 個（%d のはず）'
                         % (len(nums), N_SHAPES * 8))
    return [nums[i * 8:(i + 1) * 8] for i in range(N_SHAPES)]


def arm_positions():
    """回るときに腕が通るマス -> (時計回り 8 組, 反時計回り 8 組)"""
    src = io.open(GATE_SRC, encoding='utf-8').read()
    out = []
    for name in ('Clockwise', 'AntiClockwise'):
        m = re.search(r'sRotatingGate_ArmPositions%sRotation\[\]\s*=\s*\{(.*?)\};'
                      % name, src, re.S)
        if not m:
            raise SystemExit('★ %s の通り道が読めない' % name)
        p = [(int(a), int(b)) for a, b in
             re.findall(r'\{\s*(-?\d+),\s*(-?\d+)\s*\}', m.group(1))]
        if len(p) != 8:
            raise SystemExit('★ %s の通り道が %d 組（8 のはず）' % (name, len(p)))
        out.append(p)
    return out[0], out[1]


def tiles(name):
    """PNG -> 4bpp（8x8 タイルを左上から行順に並べたもの）"""
    w, h, rows, pal = read_indexed_png(os.path.join(GFX_DIR, PNG[name] + '.png'))
    if w % 8 or h % 8:
        raise SystemExit('★ %s の大きさが 8 の倍数でない（%dx%d）' % (name, w, h))
    out = bytearray()
    for ty in range(h // 8):
        for tx in range(w // 8):
            for y in range(8):
                for x in range(0, 8, 2):
                    lo = rows[ty * 8 + y][tx * 8 + x]
                    hi = rows[ty * 8 + y][tx * 8 + x + 1]
                    out.append(lo | (hi << 4))
    return (w, h), bytes(out), pal


def gba_color(rgb):
    r, g, b = (v * 31 // 255 for v in rgb)
    return r | (g << 5) | (b << 10)


def main():
    cfg = config()
    used = sorted({s for _x, _y, s, _o in cfg})
    anims = affine_anims()

    blobs, pal = {}, None
    for s in used:
        (w, h), raw, p = tiles(s)
        want = (32, 32) if s in SMALL else (64, 64)
        if (w, h) != want:
            raise SystemExit('★ %s が %dx%d（%dx%d のはず）'
                             % (s, w, h, want[0], want[1]))
        blobs[s] = raw
        if pal is None:
            pal = p
        elif p != pal:
            raise SystemExit('★ %s のパレットが他と違う' % s)

    # 絵が使う色番号（エメラルドは 11/12/13/15 の 4 つだけ）
    ink = set()
    for s in used:
        for b in blobs[s]:
            ink.add(b & 0xF)
            ink.add(b >> 4)
    ink.discard(0)

    g, m = map_id(MAP)
    w = []
    a = w.append
    a('// tools/fortree/build_rotating_gates.py が生成。手で編集しないこと。')
    a('//')
    a('// ヒワマキジムの回転門。元は pokeemerald の src/rotating_gate.c と')
    a('// graphics/rotating_gates/。**使う形は %d 種だけ**（%s）。'
      % (len(used), ' / '.join(used)))
    a('//')
    a('// ★ エメラルドはパレットを読み込まず、野外の NPC パレットの 2 番に')
    a('//   たまたま入っている色を借りている。こちらは**自前の札**で読む。')
    a('//   絵が使う色番号は %s。' % sorted(ink))
    a('')
    a('#define HOENN_GATE_MAP_GROUP %d   // エメラルドでの群（+43 で FRLG）' % g)
    a('#define HOENN_GATE_MAP_NUM   %d' % m)
    a('#define HOENN_GATE_COUNT     %d' % len(cfg))
    a('#define HOENN_GATE_TILE_TAG  0x1300')
    a('#define HOENN_GATE_PAL_TAG   0x1300')
    a('')
    for s in used:
        a('#define HOENN_GATE_SHAPE_%s %d' % (s, SHAPES.index(s)))
    a('')
    a('static const u16 sHoennGatePalette[16] =')
    a('{')
    a('\t' + ', '.join('0x%04X' % gba_color(pal[i]) for i in range(16)) + ',')
    a('};')
    a('')
    for s in used:
        raw = blobs[s]
        a('static const u8 sHoennGateTiles_%s[%d] =' % (s, len(raw)))
        a('{')
        for i in range(0, len(raw), 16):
            a('\t' + ', '.join('0x%02X' % b for b in raw[i:i + 16]) + ',')
        a('};')
        a('')
    a('// 門の配置（x, y, 形, 向き）。x/y は MAP_OFFSET を足す前の値')
    a('struct HoennGateSpot { u8 x; u8 y; u8 shape; u8 orient; };')
    a('static const struct HoennGateSpot sHoennGateSpots[HOENN_GATE_COUNT] =')
    a('{')
    for x, y, s, o in cfg:
        a('\t{%2d, %2d, HOENN_GATE_SHAPE_%s, %d},' % (x, y, s, o))
    a('};')
    a('')
    a('// 32x32 の形（残りは 64x64）')
    a('static const u8 sHoennGateIsSmall[%d] = {%s};'
      % (N_SHAPES, ', '.join('1' if s in SMALL else '0' for s in SHAPES)))
    a('')
    # ---- 回すための表（第 2 段）----
    rot = rotation_info()
    arms = arm_layout()
    cw, acw = arm_positions()
    a('// 押した向きごとの 4x4 の表。値は `GATE_ROT(向き, 腕, 遠い)`')
    a('//   上位 4 ビット = 回る向き（1 反時計 / 2 時計）')
    a('//   下位 4 ビット = 腕（0 北 / 1 東 / 2 南 / 3 西）<< 1 | 遠い腕か')
    a('//   255 = 何も起きない')
    a('// ★ 並びは DIR_SOUTH(1) / DIR_NORTH(2) / DIR_WEST(3) / DIR_EAST(4)')
    a('//   に合わせて **1 から**入れてある（0 番は使わない）')
    a('static const u8 sHoennGateRotInfo[5][16] =')
    a('{')
    a('\t{0},')
    for d in ('South', 'North', 'West', 'East'):
        a('\t{' + ', '.join(str(v) for v in rot[d]) + '},')
    a('};')
    a('')
    a('// 形ごとの腕の有無（北・東・南・西 x 近い/遠い）')
    a('static const u8 sHoennGateArmLayout[%d][8] =' % N_SHAPES)
    a('{')
    for row in arms:
        a('\t{' + ', '.join(str(v) for v in row) + '},')
    a('};')
    a('')
    a('// 回るときに腕が通るマス（向き x 近い/遠い の 8 組）')
    a('static const s8 sHoennGateArmPosCw[8][2] =')
    a('{')
    for x, y in cw:
        a('\t{%d, %d},' % (x, y))
    a('};')
    a('static const s8 sHoennGateArmPosAcw[8][2] =')
    a('{')
    for x, y in acw:
        a('\t{%d, %d},' % (x, y))
    a('};')
    a('')
    a('// 回転のアフィンアニメ %d 本（エメラルドのまま）' % len(anims))
    for n, cmds in anims:
        a('static const union AffineAnimCmd sHoennGateAffine_%s[] =' % n)
        a('{')
        for kind, args in cmds:
            if kind == 'FRAME':
                a('\tAFFINEANIMCMD_FRAME(%s),' % ', '.join(args))
            elif kind == 'JUMP':
                a('\tAFFINEANIMCMD_JUMP(%s),' % args[0])
            else:
                a('\tAFFINEANIMCMD_END,')
        a('};')
    a('')
    a('static const union AffineAnimCmd* const sHoennGateAffineTable[] =')
    a('{')
    for n, _c in anims:
        a('\tsHoennGateAffine_%s,' % n)
    a('};')
    io.open(OUT, 'w', encoding='utf-8', newline='\n').write('\n'.join(w) + '\n')

    print('■ ヒワマキの回転門')
    print('   門 %d 個 / 形 %d 種（%s）' % (len(cfg), len(used), ', '.join(used)))
    print('   絵 %d バイト / 使う色番号 %s'
          % (sum(len(blobs[s]) for s in used), sorted(ink)))
    print('   アフィンアニメ %d 本 / 4x4 の表 4 枚 / 腕の形 8 種' % len(anims))
    print('   エメラルドの群 %d 番 %d（FRLG では群 %d）' % (g, m, g + 43))
    print('   出力: %s' % os.path.relpath(OUT, os.path.dirname(HERE)))


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--reference', required=True)
    parser.add_argument('--output', default=OUT)
    args = parser.parse_args()
    EM = args.reference
    GATE_SRC = os.path.join(EM, 'src', 'rotating_gate.c')
    GFX_DIR = os.path.join(EM, 'graphics', 'rotating_gates')
    OUT = args.output
    main()
