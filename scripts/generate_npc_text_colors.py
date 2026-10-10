#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""足した人の見た目（CFRU の表 7＝ホウエン・ジョウト・シンオウ・モモカ）ごとの、話すときの文の色（男 青・女 ピンク）。

    python scripts/generate_npc_text_colors.py           書き直す
    python scripts/generate_npc_text_colors.py --check   書き直さずに、生成物と同じか見る（verify_all）

★ 2026-10-09 ユーザー「メッセージの色分けがただしくないみたい」「かいじゅうマニアが女のメッセージになっていたり　アダンも　ナギが男の
  メッセージになっていたり」。素の ContextNpcGetTextColor（080CCF90）は見た目の番号の下 8 ビットしか読まず、表 7 の人を関係ない素の人の色で出していた。
  → src/hack_npc_text_color.c が上の 8 ビットも見て、表 7 はここで作る src/Tables/npc_text_colors.h を引く。
  色の番号は素と同じ: 0 男・1 女・2 ポケモン・3 ほか（素の NPC_TEXT_COLOR_*）。
  見た目を tools/rom/build_hoenn_ow.py に足したら、ここの FEMALE / MON / NEUTRAL に入れるか（入れなければ男）を決める。
  名前が分からないもの（どれにも当てはまらない新しい名前）は止める＝男と決めつけない。
"""
import argparse
import io
import json
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.dirname(os.path.dirname(HERE))
OW_JSON = os.path.join(HERE, 'data', 'npc_graphics_ids.json')
OUT = os.path.join(os.path.dirname(HERE), 'src', 'Tables', 'npc_text_colors.h')

MALE, FEMALE, MON, NEUTRAL = 0, 1, 2, 3
FEMALE_NAMES = {
    'ROXANNE', 'FLANNERY', 'WINONA', 'LIZA', 'PHOEBE', 'GLACIA', 'RIVAL_MAY_NORMAL', 'MOM', 'BEAUTY', 'TEALA',
    'GIRL_1', 'GIRL_2', 'GIRL_3', 'LITTLE_GIRL', 'TWIN', 'POKEFAN_F', 'HEX_MANIAC', 'EXPERT_F', 'OLD_WOMAN', 'PICNICKER',
    'SWIMMER_F', 'LASS', 'RUNNING_TRIATHLETE_F', 'CYCLING_TRIATHLETE_F', 'TUBER_F', 'NURSE', 'UNION_ROOM_NURSE', 'REPORTER_F',
    'ANABEL', 'GRETA', 'LUCY', 'ROOFTOP_SALE_WOMAN', 'AQUA_MEMBER_F', 'MAGMA_MEMBER_F', 'HOT_SPRINGS_OLD_WOMAN',
    'WOMAN_1', 'WOMAN_2', 'WOMAN_3', 'WOMAN_4', 'WOMAN_5',
    'JOHTO_WHITNEY', 'JOHTO_JASMINE', 'JOHTO_CLAIR', 'SINNOH_GARDENIA', 'MOMOKA', 'SINNOH_MAYLENE', 'SINNOH_FANTINA', 'SINNOH_CANDICE', 'SINNOH_BERTHA', 'SINNOH_CYNTHIA',
}
MALE_NAMES = {
    'BRAWLY', 'WATTSON', 'NORMAN', 'TATE', 'JUAN', 'WALLY', 'SIDNEY', 'DRAKE', 'WALLACE', 'STEVEN', 'PROF_BIRCH',
    'RIVAL_BRENDAN_NORMAL', 'SCOTT', 'SPENSER', 'TUCKER', 'NOLAND', 'BRANDON', 'ARCHIE', 'MAXIE', 'QUINTY_PLUMP',
    'MAN_1', 'MAN_2', 'MAN_3', 'MAN_4', 'MAN_5', 'POKEFAN_M', 'PSYCHIC_M', 'MANIAC', 'EXPERT_M', 'NINJA_BOY',
    'BOY_1', 'BOY_2', 'BOY_3', 'LITTLE_BOY', 'LITTLE_BOY_3', 'RICH_BOY', 'FAT_MAN', 'COOK', 'OLD_MAN', 'CAMPER', 'YOUNGSTER',
    'BUG_CATCHER', 'SCHOOL_KID_M', 'SWIMMER_M', 'BLACK_BELT', 'SCIENTIST_1', 'SCIENTIST_2', 'GENTLEMAN', 'SAILOR', 'FISHERMAN',
    'RUNNING_TRIATHLETE_M', 'CYCLING_TRIATHLETE_M', 'TUBER_M', 'TUBER_M_SWIMMING', 'HIKER', 'REPORTER_M', 'BARD', 'ARTIST',
    'CAMERAMAN', 'DEVON_EMPLOYEE', 'MART_EMPLOYEE', 'AQUA_MEMBER_M', 'MAGMA_MEMBER_M', 'GAMEBOY_KID', 'CONTEST_JUDGE',
    'MYSTERY_GIFT_MAN',
    'JOHTO_FALKNER', 'JOHTO_BUGSY', 'JOHTO_MORTY', 'JOHTO_CHUCK', 'JOHTO_PRYCE', 'SINNOH_ROARK', 'SINNOH_WAKE', 'SINNOH_BYRON', 'SINNOH_VOLKNER', 'SINNOH_AARON', 'SINNOH_FLINT', 'SINNOH_LUCIAN',
}
MON_NAMES = {
    'KYOGRE_FRONT', 'GROUDON_FRONT', 'RAYQUAZA_STILL', 'ZIGZAGOON_1', 'ZIGZAGOON_2', 'DUSCLOPS', 'AZURILL', 'AZUMARILL',
    'LATIAS', 'LATIOS', 'MEW', 'KIRLIA', 'KECLEON', 'SKITTY', 'WINGULL', 'REGIROCK', 'REGICE', 'REGISTEEL',
}
NEUTRAL_NAMES = {'TRICK_HOUSE_STATUE', 'CABLE_CAR', 'MR_BRINEYS_BOAT', 'SS_TIDAL'}


def build(metadata=OW_JSON):
    j = json.load(io.open(metadata, encoding='utf-8'))
    table = j['table']
    gfx = j['gfx']
    base = table << 8
    if table != 7 or not gfx or any(not isinstance(v, int) or not base <= v < base + 256 for v in gfx.values()):
        raise ValueError('Expected nonempty table-7 graphics IDs')
    if len(set(gfx.values())) != len(gfx):
        raise ValueError('Duplicate graphics IDs would overwrite named slots')
    slots = {v - base: k for k, v in gfx.items()}
    n = max(slots) + 1
    colors = []
    unknown = []
    for i in range(n):
        name = slots.get(i)
        if name is None:
            colors.append((NEUTRAL, '(空き)'))
        elif name in FEMALE_NAMES:
            colors.append((FEMALE, name))
        elif name in MALE_NAMES:
            colors.append((MALE, name))
        elif name in MON_NAMES:
            colors.append((MON, name))
        elif name in NEUTRAL_NAMES:
            colors.append((NEUTRAL, name))
        else:
            unknown.append(name)
    if unknown:
        raise SystemExit('★ 色を決めていない見た目: %s（scripts/generate_npc_text_colors.py の FEMALE / MALE / MON / NEUTRAL に入れる）' % unknown)
    L = ['/* 生成物。scripts/generate_npc_text_colors.py が作る。手で編集しないこと。',
         '   表 %d（0x%04X〜）の見た目ごとの話すときの文の色（0 男・1 女・2 ポケモン・3 ほか）。src/hack_npc_text_color.c が引く */' % (table, base),
         '#pragma once', '',
         '#define NPC_TEXT_COLOR_TABLE %d' % table,
         '#define NPC_TEXT_COLOR_COUNT %d' % n, '',
         'static const u8 sHackNpcTextColors[NPC_TEXT_COLOR_COUNT] =', '{']
    L += ['\t%d,\t// 0x%04X %s' % (c, base + i, name) for i, (c, name) in enumerate(colors)]
    L += ['};', '']
    return '\n'.join(L)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--metadata', default=OW_JSON)
    parser.add_argument('--output', default=OUT)
    parser.add_argument('--check', action='store_true')
    args = parser.parse_args()
    txt = build(args.metadata)
    old = io.open(args.output, encoding='utf-8').read() if os.path.exists(args.output) else None
    if args.check:
        ok = old == txt
        print('=== 結果: %s ===' % ('OK' if ok else 'NG（scripts/generate_npc_text_colors.py を走らせる）'))
        return 0 if ok else 1
    if old != txt:
        io.open(args.output, 'w', encoding='utf-8', newline='\n').write(txt)
    print('文の色: 表 7 の %d 個 → %s' % (txt.count('// 0x'), os.path.relpath(OUT, REPO)))
    return 0


if __name__ == '__main__':
    sys.exit(main())
