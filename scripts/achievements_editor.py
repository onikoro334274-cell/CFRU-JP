#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
じっせき・報酬エディタ
  achievements_editor.bat をダブルクリック (または python -I scripts/achievements_editor.py) で起動します。
  scripts/achievements.json を編集し、「保存して生成」で achievements_gen.py と同じファイルを作り直します。
"""
import importlib.util, os, subprocess, sys, threading
import tkinter as tk
from tkinter import ttk, messagebox

HERE = os.path.dirname(os.path.abspath(__file__))
_spec = importlib.util.spec_from_file_location("achievements_gen", os.path.join(HERE, "achievements_gen.py"))
gen = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(gen)
ROOT = os.path.normpath(gen.ROOT)

TIER_LABELS = {
    "BRONZE": "ブロンズ (モンスターボール)",
    "SILVER": "シルバー (スーパーボール)",
    "GOLD": "ゴールド (ハイパーボール)",
    "PLATINUM": "プラチナ (マスターボール)",
}
KIND_LABELS = {
    "FLAG": "フラグが立ったら",
    "STAT": "ゲームの記録が 目標値 以上",
    "DEX": "図鑑で つかまえた数が 目標値 以上",
    "SPECIES": "そのポケモンを つかまえたら",
    "FUNC": "特別な条件 (C で判定)",
    "ITEM": "その道具が バッグに あったら",
}
REWARD_LABELS = {"NONE": "なし", "ITEM": "アイテム", "MON": "ポケモン"}
KIND_SHORT = {"FLAG": "フラグ", "STAT": "記録", "DEX": "図鑑", "SPECIES": "捕獲", "FUNC": "特別", "ITEM": "道具"}
TIER_SHORT = {"BRONZE": "ブロンズ", "SILVER": "シルバー", "GOLD": "ゴールド", "PLATINUM": "プラチナ"}
# 目標値を使わない種別 (常に 1)
FIXED_TARGET = ("FLAG", "SPECIES", "ITEM")

STAT_HINTS = {
    "GAME_STAT_STEPS": "あるいた歩数",
    "GAME_STAT_TOTAL_BATTLES": "たたかった回数 (すべて)",
    "GAME_STAT_WILD_BATTLES": "やせいの ポケモンと たたかった回数",
    "GAME_STAT_TRAINER_BATTLES": "トレーナーと たたかった回数",
    "GAME_STAT_ENTERED_HOF": "でんどういり した回数",
    "GAME_STAT_POKEMON_CAPTURES": "ポケモンを つかまえた数",
    "GAME_STAT_FISHING_CAPTURES": "つりで つかまえた数",
    "GAME_STAT_HATCHED_EGGS": "タマゴを かえした数",
    "GAME_STAT_EVOLVED_POKEMON": "しんか させた回数",
    "GAME_STAT_USED_POKECENTER": "ポケモンセンターを つかった回数",
    "GAME_STAT_RESTED_AT_HOME": "いえで やすんだ回数",
    "GAME_STAT_ENTERED_SAFARI_ZONE": "サファリゾーンに はいった回数",
    "GAME_STAT_USED_CUT": "いあいぎりを つかった回数",
    "GAME_STAT_USED_ROCK_SMASH": "いわくだきを つかった回数",
    "GAME_STAT_FOUND_HIDDEN_ITEM": "かくれた どうぐを みつけた数",
    "GAME_STAT_POKEMON_TRADES": "こうかん した回数",
    "GAME_STAT_LINK_BATTLE_WINS": "つうしん たいせんで かった回数",
    "GAME_STAT_USED_SPLASH": "はねるを つかった回数",
    "GAME_STAT_USED_STRUGGLE": "わるあがきを つかった回数",
    "GAME_STAT_SLOT_JACKPOTS": "スロットで おおあたり した回数",
    "GAME_STAT_ENTERED_BATTLE_FACILITY": "バトルしせつに はいった回数",
    "GAME_STAT_BATTLE_FACILITY_BEST_STREAK": "バトルしせつの さいこう れんしょう",
    "GAME_STAT_REVIVED_FOSSIL": "かせきを ふっかつ させた回数",
    "GAME_STAT_SHOPPED": "かいもの した回数",
    "GAME_STAT_USED_ITEMFINDER": "ダウジングマシンを つかった回数",
    "GAME_STAT_GOT_RAINED_ON": "あめに ふられた回数",
    "GAME_STAT_CHECKED_POKEDEX": "ずかんを みた回数",
    "GAME_STAT_JUMPED_DOWN_LEDGES": "だんさを とびおりた回数",
    "GAME_STAT_USED_DAYCARE": "そだてやを つかった回数",
    "GAME_STAT_RAID_BATTLES": "レイドバトルの回数",
    "GAME_STAT_DEXNAV_SCANNED": "DexNav で さがした回数",
}
FUNC_HINTS = {
    "ACH_FUNC_KANTO_DEX": "カントー図鑑 151ひきを すべて つかまえた",
    "ACH_FUNC_SHINY": "色ちがいが てもちに いる",
    "ACH_FUNC_LEVEL_100": "レベル100が てもちに いる",
    "ACH_FUNC_LEGEND_BIRDS": "フリーザー・サンダー・ファイヤーを つかまえた",
    "ACH_FUNC_LEGEND_BEASTS": "ライコウ・エンテイ・スイクンを つかまえた",
    "ACH_FUNC_TM_10": "わざマシンを 10しゅるい もっている",
    "ACH_FUNC_TM_30": "わざマシンを 30しゅるい もっている",
    "ACH_FUNC_TM_50": "わざマシンを 50しゅるい もっている",
    "ACH_FUNC_ALL_OTHERS": "ほかの じっせきを すべて 解除した",
    "ACH_FUNC_SURF_PIKACHU": "なみのりを おぼえた ピカチュウが てもちに いる",
    "ACH_FUNC_PARTY_ALL_LEVEL_100": "6ひき ぜんいん レベル100",
    "ACH_FUNC_PARTY_SAME_TYPE": "6ひき ぜんいん おなじ タイプを もつ",
    "ACH_FUNC_NICKNAME": "ニックネームの ついた ポケモンが てもちに いる",
    "ACH_FUNC_MEGA_RING": "メガリングを もっている",
    "ACH_FUNC_MEGA_STONE": "メガストーンを もった ポケモンが てもちに いる",
    "ACH_FUNC_Z_RING": "Zリングを もっている",
    "ACH_FUNC_Z_CRYSTAL": "Zクリスタルを もった ポケモンが てもちに いる",
    "ACH_FUNC_DYNAMAX_BAND": "ダイマックスバンドを もっている",
    "ACH_FUNC_GIGANTAMAX": "キョダイマックスできる ポケモンが てもちに いる",
    "ACH_FUNC_TERA_ORB": "テラスタルオーブを もっている",
    "ACH_FUNC_HIDDEN_ABILITY": "かくれとくせいの ポケモンが てもちに いる",
    "ACH_FUNC_MAX_FRIENDSHIP": "なつき度 255 の ポケモンが てもちに いる",
    "ACH_FUNC_POKERUS": "ポケルスに かかった ポケモンが てもちに いる",
    "ACH_FUNC_PERFECT_IV": "個体値 6V の ポケモンが てもちに いる",
    "ACH_FUNC_MAX_EV": "努力値 合計510 の ポケモンが てもちに いる",
    "ACH_FUNC_SHINY_CHARM": "ひかるおまもりを もっている",
    "ACH_FUNC_OVAL_CHARM": "まるいおまもりを もっている",
    "ACH_FUNC_ALL_REGIONS": "8地方 (図鑑 1-898) それぞれで 1ぴき以上 つかまえた (目標値 8)",
    "ACH_FUNC_EEVEELUTIONS": "イーブイの進化系 8しゅるいを つかまえた (目標値 8)",
    "ACH_FUNC_WEATHER_TRIO": "グラードン・カイオーガ・レックウザを つかまえた (目標値 3)",
    "ACH_FUNC_REGI_TRIO": "レジロック・レジアイス・レジスチルを つかまえた (目標値 3)",
    "ACH_FUNC_LAKE_TRIO": "ユクシー・エムリット・アグノムを つかまえた (目標値 3)",
    "ACH_FUNC_CREATION_TRIO": "ディアルガ・パルキア・ギラティナを つかまえた (目標値 3)",
    "ACH_FUNC_TAO_TRIO": "レシラム・ゼクロム・キュレムを つかまえた (目標値 3)",
    "ACH_FUNC_XY_DUO": "ゼルネアス・イベルタルを つかまえた (目標値 2)",
    "ACH_FUNC_SUN_MOON_DUO": "ソルガレオ・ルナアーラを つかまえた (目標値 2)",
    "ACH_FUNC_HERO_DUO": "ザシアン・ザマゼンタを つかまえた (目標値 2)",
    "ACH_FUNC_PSEUDO_LEGENDS": "600族 9しゅるいを つかまえた (目標値 9)",
    "ACH_FUNC_ALL_STARTERS": "1-8世代の御三家 24しゅるいを つかまえた (目標値 24)",
    "ACH_FUNC_ULTRA_BEAST": "ウルトラビーストを どれか 1ぴき つかまえた",
}


class FilterCombobox(ttk.Combobox):
    """入力した文字を ふくむ候補だけに しぼりこめる Combobox"""

    def __init__(self, master, **kw):
        super().__init__(master, **kw)
        self.all_values = []
        self.bind("<KeyRelease>", self._on_key)

    def set_all_values(self, values):
        self.all_values = list(values)
        self["values"] = self.all_values

    def _on_key(self, event):
        if event.keysym in ("Up", "Down", "Return", "Escape", "Tab"):
            return
        text = self.get().strip().upper()
        self["values"] = [v for v in self.all_values if text in v.upper()] if text else self.all_values


class Editor(tk.Tk):
    def __init__(self):
        super().__init__()
        self.title("じっせき・報酬エディタ")
        self.geometry("1180x860")
        self.minsize(960, 760)
        for name in ("TkDefaultFont", "TkTextFont", "TkMenuFont", "TkHeadingFont"):
            try:
                tk.font.nametofont(name).configure(family="Meiryo UI", size=10)
            except Exception:
                pass

        self.symbols = gen.load_symbols()
        self.charset = gen.load_charset()
        self.data = gen.load_data()
        # ファイルに保存済みの件数。これより前の項目はセーブデータで使われているかもしれない
        self.saved_ach = len(self.data["achievements"])
        self.saved_rew = len(self.data["rewards"])
        self.dirty = False
        self.loading = False
        self.cur_ach = None
        self.cur_rew = None
        self.building = False

        nb = ttk.Notebook(self)
        nb.pack(fill="both", expand=True, padx=6, pady=(6, 0))
        self.ach_tab = ttk.Frame(nb)
        self.rew_tab = ttk.Frame(nb)
        nb.add(self.ach_tab, text="  じっせき  ")
        nb.add(self.rew_tab, text="  解除数の報酬  ")
        self._build_ach_tab()
        self._build_rew_tab()
        self._build_bottom()

        self.refresh_ach_tree()
        self.refresh_rew_tree()
        if self.data["achievements"]:
            self.select_ach(0)
        if self.data["rewards"]:
            self.select_rew(0)
        self.update_title()
        self.protocol("WM_DELETE_WINDOW", self.on_close)
        self.log("scripts/achievements.json を読みこみました (じっせき %d / 報酬 %d)" % (self.saved_ach, self.saved_rew))

    # ------------------------------------------------------------------ #
    # じっせき タブ
    # ------------------------------------------------------------------ #
    def _build_ach_tab(self):
        left = ttk.Frame(self.ach_tab)
        left.pack(side="left", fill="both", expand=True, padx=(6, 3), pady=6)
        cols = ("no", "id", "name", "tier", "cond", "reward")
        self.ach_tree = ttk.Treeview(left, columns=cols, show="headings", selectmode="browse")
        for c, label, w in (("no", "No", 40), ("id", "ID", 120), ("name", "名前", 170), ("tier", "ランク", 64),
                            ("cond", "条件", 230), ("reward", "報酬", 130)):
            self.ach_tree.heading(c, text=label)
            self.ach_tree.column(c, width=w, stretch=(c == "cond"), anchor="center" if c == "no" else "w")
        sb = ttk.Scrollbar(left, orient="vertical", command=self.ach_tree.yview)
        self.ach_tree.configure(yscrollcommand=sb.set)
        self.ach_tree.pack(side="left", fill="both", expand=True)
        sb.pack(side="left", fill="y")
        self.ach_tree.bind("<<TreeviewSelect>>", lambda e: self._on_ach_select())

        right = ttk.Frame(self.ach_tab, width=400)
        right.pack(side="left", fill="y", padx=(3, 6), pady=6)

        btns = ttk.Frame(right)
        btns.pack(fill="x", pady=(0, 8))
        ttk.Button(btns, text="＋ じっせきを追加", command=self.add_ach).pack(side="left")
        ttk.Button(btns, text="最後のじっせきを削除", command=self.delete_last_ach).pack(side="left", padx=6)

        form = ttk.LabelFrame(right, text="えらんだ じっせき")
        form.pack(fill="both", expand=True)
        form.columnconfigure(1, weight=1)
        r = 0

        ttk.Label(form, text="ID").grid(row=r, column=0, sticky="w", padx=6, pady=4)
        self.v_id = tk.StringVar()
        self.e_id = ttk.Entry(form, textvariable=self.v_id)
        self.e_id.grid(row=r, column=1, sticky="ew", padx=6, pady=4)
        r += 1
        self.l_id_note = ttk.Label(form, text="", foreground="#777")
        self.l_id_note.grid(row=r, column=1, sticky="w", padx=6)
        r += 1

        ttk.Label(form, text="名前").grid(row=r, column=0, sticky="w", padx=6, pady=4)
        self.v_name = tk.StringVar()
        ttk.Entry(form, textvariable=self.v_name).grid(row=r, column=1, sticky="ew", padx=6, pady=4)
        r += 1
        self.l_name_count = ttk.Label(form, text="", foreground="#777")
        self.l_name_count.grid(row=r, column=1, sticky="w", padx=6)
        r += 1

        ttk.Label(form, text="説明").grid(row=r, column=0, sticky="nw", padx=6, pady=4)
        self.t_desc = tk.Text(form, height=2, width=34, wrap="none", undo=True)
        self.t_desc.grid(row=r, column=1, sticky="ew", padx=6, pady=4)
        self.t_desc.bind("<<Modified>>", self._on_desc_modified)
        r += 1
        self.l_desc_count = ttk.Label(form, text="", foreground="#777", justify="left")
        self.l_desc_count.grid(row=r, column=1, sticky="w", padx=6)
        r += 1

        ttk.Label(form, text="ランク").grid(row=r, column=0, sticky="w", padx=6, pady=4)
        self.v_tier = tk.StringVar()
        ttk.Combobox(form, textvariable=self.v_tier, state="readonly",
                     values=[TIER_LABELS[t] for t in gen.TIERS]).grid(row=r, column=1, sticky="ew", padx=6, pady=4)
        r += 1

        ttk.Label(form, text="解除の条件").grid(row=r, column=0, sticky="w", padx=6, pady=4)
        self.v_kind = tk.StringVar()
        ttk.Combobox(form, textvariable=self.v_kind, state="readonly",
                     values=[KIND_LABELS[k] for k in gen.KINDS]).grid(row=r, column=1, sticky="ew", padx=6, pady=4)
        r += 1

        self.l_param = ttk.Label(form, text="対象")
        self.l_param.grid(row=r, column=0, sticky="w", padx=6, pady=4)
        self.v_param = tk.StringVar()
        self.c_param = FilterCombobox(form, textvariable=self.v_param)
        self.c_param.grid(row=r, column=1, sticky="ew", padx=6, pady=4)
        self.c_param.bind("<<ComboboxSelected>>", lambda e: self._on_ach_field())
        r += 1
        self.l_param_hint = ttk.Label(form, text="", foreground="#777", wraplength=320, justify="left")
        self.l_param_hint.grid(row=r, column=1, sticky="w", padx=6)
        r += 1

        ttk.Label(form, text="目標値").grid(row=r, column=0, sticky="w", padx=6, pady=4)
        self.v_target = tk.StringVar()
        self.e_target = ttk.Entry(form, textvariable=self.v_target, width=12)
        self.e_target.grid(row=r, column=1, sticky="w", padx=6, pady=4)
        r += 1

        ttk.Separator(form).grid(row=r, column=0, columnspan=2, sticky="ew", padx=6, pady=(8, 2))
        r += 1
        ttk.Label(form, text="このじっせきの報酬 (解除後、一覧画面で Aボタンで 1回だけ もらえる)", foreground="#555").grid(
            row=r, column=0, columnspan=2, sticky="w", padx=6)
        r += 1

        ttk.Label(form, text="報酬").grid(row=r, column=0, sticky="w", padx=6, pady=4)
        self.v_rtype = tk.StringVar()
        ttk.Combobox(form, textvariable=self.v_rtype, state="readonly",
                     values=[REWARD_LABELS[t] for t in gen.REWARD_TYPES]).grid(row=r, column=1, sticky="ew", padx=6, pady=4)
        r += 1

        self.l_rid = ttk.Label(form, text="アイテム")
        self.l_rid.grid(row=r, column=0, sticky="w", padx=6, pady=4)
        self.v_rid = tk.StringVar()
        self.c_rid = FilterCombobox(form, textvariable=self.v_rid)
        self.c_rid.grid(row=r, column=1, sticky="ew", padx=6, pady=4)
        self.c_rid.bind("<<ComboboxSelected>>", lambda e: self._on_ach_field())
        r += 1

        self.l_ramount = ttk.Label(form, text="個数")
        self.l_ramount.grid(row=r, column=0, sticky="w", padx=6, pady=4)
        self.v_ramount = tk.StringVar()
        self.s_ramount = ttk.Spinbox(form, from_=1, to=255, textvariable=self.v_ramount, width=8)
        self.s_ramount.grid(row=r, column=1, sticky="w", padx=6, pady=4)
        r += 1

        ttk.Label(form, text="もちもの").grid(row=r, column=0, sticky="w", padx=6, pady=4)
        self.v_rheld = tk.StringVar()
        self.c_rheld = FilterCombobox(form, textvariable=self.v_rheld)
        self.c_rheld.set_all_values(self.symbols.get("ITEM", []))
        self.c_rheld.grid(row=r, column=1, sticky="ew", padx=6, pady=4)
        self.c_rheld.bind("<<ComboboxSelected>>", lambda e: self._on_ach_field())
        r += 1
        self.l_reward_hint = ttk.Label(form, text="", foreground="#c33")
        self.l_reward_hint.grid(row=r, column=1, sticky="w", padx=6)
        r += 1

        ttk.Label(form, text=(
            "・新しい じっせきは いちばん下に 追加されます\n"
            "  (並び順 = セーブのフラグ番号なので 入れかえ・途中削除は できません)\n"
            "・説明は 2行まで。Enter で改行すると \\n になります\n"
            "・対象は 文字を入力すると 候補がしぼれます (▼で一覧)"),
            foreground="#555", justify="left").grid(row=r, column=0, columnspan=2, sticky="w", padx=6, pady=(10, 6))

        for v in (self.v_id, self.v_name, self.v_tier, self.v_kind, self.v_param, self.v_target,
                  self.v_rtype, self.v_rid, self.v_ramount, self.v_rheld):
            v.trace_add("write", lambda *a: self._on_ach_field())

    def ach_cond_text(self, a):
        kind = a["kind"]
        if kind == "DEX":
            return "図鑑 %d ひき" % a["target"]
        if kind in FIXED_TARGET:
            return "%s: %s" % (KIND_SHORT.get(kind, kind), a["param"])
        return "%s: %s ≧ %d" % (KIND_SHORT.get(kind, kind), a["param"], a["target"])

    def ach_row(self, i):
        a = self.data["achievements"][i]
        return (i + 1, a["id"], a["name"], TIER_SHORT.get(a["tier"], a["tier"]), self.ach_cond_text(a), self.reward_text(a))

    @staticmethod
    def reward_text(a):
        rw = gen.get_reward(a)
        if rw["type"] == "ITEM":
            return "%s x%d" % (rw["id"].replace("ITEM_", "", 1), rw["amount"])
        if rw["type"] == "MON":
            return "%s Lv%d" % (rw["id"].replace("SPECIES_", "", 1), rw["amount"])
        return ""

    def _apply_reward_widgets(self, rtype):
        if rtype == "MON":
            self.l_rid.configure(text="ポケモン")
            self.l_ramount.configure(text="レベル")
            self.c_rid.set_all_values(self.symbols.get("SPECIES", []))
            self.s_ramount.configure(to=100)
        else:
            self.l_rid.configure(text="アイテム")
            self.l_ramount.configure(text="個数")
            self.c_rid.set_all_values(self.symbols.get("ITEM", []))
            self.s_ramount.configure(to=255)
        on = "normal" if rtype != "NONE" else "disabled"
        self.c_rid.configure(state=on)
        self.s_ramount.configure(state=on)
        self.c_rheld.configure(state="normal" if rtype == "MON" else "disabled")

    def refresh_ach_tree(self):
        self.ach_tree.delete(*self.ach_tree.get_children())
        for i in range(len(self.data["achievements"])):
            self.ach_tree.insert("", "end", iid=str(i), values=self.ach_row(i))

    def select_ach(self, i):
        self.ach_tree.selection_set(str(i))
        self.ach_tree.see(str(i))

    def _on_ach_select(self):
        sel = self.ach_tree.selection()
        if not sel:
            return
        self.cur_ach = int(sel[0])
        a = self.data["achievements"][self.cur_ach]
        self.loading = True
        self.v_id.set(a["id"])
        existing = self.cur_ach < self.saved_ach
        self.e_id.configure(state="disabled" if existing else "normal")
        self.l_id_note.configure(text="保存ずみの ID は かえられません (C のコードで使われている場合があるため)" if existing
                                 else "半角英大文字・数字・_ (例: CATCH_100)")
        self.v_name.set(a["name"])
        self.t_desc.delete("1.0", "end")
        self.t_desc.insert("1.0", a["desc"].replace("\\n", "\n"))
        self.t_desc.edit_modified(False)
        self.v_tier.set(TIER_LABELS.get(a["tier"], ""))
        self.v_kind.set(KIND_LABELS.get(a["kind"], ""))
        self._apply_kind_widgets(a["kind"])
        self.v_param.set("" if a["kind"] == "DEX" else a["param"])
        self.v_target.set(str(a["target"]))
        rw = gen.get_reward(a)
        self.v_rtype.set(REWARD_LABELS.get(rw["type"], "なし"))
        self._apply_reward_widgets(rw["type"])
        self.v_rid.set(rw["id"] if rw["type"] != "NONE" else "")
        self.v_ramount.set(str(rw["amount"]) if rw["type"] != "NONE" else "")
        self.v_rheld.set(rw["held"] if rw["type"] == "MON" else "")
        self.loading = False
        self._update_ach_hints()

    def _apply_kind_widgets(self, kind):
        labels = {"FLAG": "フラグ", "STAT": "記録 (GAME_STAT)", "DEX": "対象", "SPECIES": "ポケモン", "FUNC": "条件 (ACH_FUNC)", "ITEM": "道具"}
        self.l_param.configure(text=labels.get(kind, "対象"))
        self.c_param.set_all_values(self.symbols.get(kind, []))
        self.c_param.configure(state="disabled" if kind == "DEX" else "normal")
        self.e_target.configure(state="disabled" if kind in FIXED_TARGET else "normal")

    def _update_ach_hints(self):
        if self.cur_ach is None:
            return
        a = self.data["achievements"][self.cur_ach]
        n = len(a["name"])
        self.l_name_count.configure(text="%d もじ (一覧に収まる めやす %d もじ)" % (n, gen.NAME_WARN_CHARS),
                                    foreground="#c33" if n > gen.NAME_WARN_CHARS else "#777")
        lines = a["desc"].split("\\n")
        parts = ["%d行目: %d / %d もじ" % (i + 1, len(l), gen.DESC_WARN_CHARS) for i, l in enumerate(lines)]
        bad = len(lines) > 2 or any(len(l) > gen.DESC_WARN_CHARS for l in lines)
        self.l_desc_count.configure(text="   ".join(parts) + ("   ← 2行までです" if len(lines) > 2 else ""),
                                    foreground="#c33" if bad else "#777")
        kind, param = a["kind"], a["param"]
        if kind == "STAT":
            hint = STAT_HINTS.get(param, "")
        elif kind == "FUNC":
            hint = FUNC_HINTS.get(param, "")
        elif kind == "DEX":
            hint = "ぜんこく図鑑で つかまえた しゅるいの数が 目標値に とどいたら解除"
        elif kind == "FLAG":
            hint = "0x820 のような 数字でも指定できます"
        else:
            hint = ""
        if kind != "DEX" and param and param not in self.symbols.get(kind, []) and not (kind == "FLAG" and gen._is_number(param)):
            hint = "⚠ %s は見つかりません" % param
        self.l_param_hint.configure(text=hint, foreground="#c33" if hint.startswith("⚠") else "#777")

        rw = gen.get_reward(a)
        rhint = ""
        if rw["type"] == "ITEM" and (rw["id"] not in self.symbols.get("ITEM", []) or rw["id"] == "ITEM_NONE"):
            rhint = "⚠ アイテムを えらんでください" if not rw["id"] or rw["id"] == "ITEM_NONE" else "⚠ %s は見つかりません" % rw["id"]
        elif rw["type"] == "MON":
            if rw["id"] not in self.symbols.get("SPECIES", []) or rw["id"] == "SPECIES_NONE":
                rhint = "⚠ ポケモンを えらんでください" if not rw["id"] else "⚠ %s は見つかりません" % rw["id"]
            elif rw["held"] not in self.symbols.get("ITEM", []):
                rhint = "⚠ もちもの %s は見つかりません" % rw["held"]
        self.l_reward_hint.configure(text=rhint)

    def _on_desc_modified(self, event=None):
        if not self.t_desc.edit_modified():
            return
        self.t_desc.edit_modified(False)
        self._on_ach_field()

    def _on_ach_field(self):
        if self.loading or self.cur_ach is None:
            return
        a = self.data["achievements"][self.cur_ach]
        if self.cur_ach >= self.saved_ach:
            a["id"] = self.v_id.get().strip().upper()
        a["name"] = self.v_name.get()
        a["desc"] = self.t_desc.get("1.0", "end-1c").replace("\r", "").replace("\n", "\\n")
        tier = [t for t, l in TIER_LABELS.items() if l == self.v_tier.get()]
        if tier:
            a["tier"] = tier[0]
        kind = [k for k, l in KIND_LABELS.items() if l == self.v_kind.get()]
        if kind and kind[0] != a["kind"]:
            a["kind"] = kind[0]
            self.loading = True
            self._apply_kind_widgets(a["kind"])
            self.v_param.set("")
            if a["kind"] in FIXED_TARGET:
                self.v_target.set("1")
            self.loading = False
        a["param"] = "0" if a["kind"] == "DEX" else self.v_param.get().strip()
        try:
            a["target"] = 1 if a["kind"] in FIXED_TARGET else int(self.v_target.get().strip(), 0)
        except ValueError:
            a["target"] = 0

        rtype = [t for t, l in REWARD_LABELS.items() if l == self.v_rtype.get()]
        rtype = rtype[0] if rtype else "NONE"
        if rtype != gen.get_reward(a)["type"]:
            self.loading = True
            self._apply_reward_widgets(rtype)
            self.v_rid.set("")
            self.v_ramount.set("5" if rtype == "MON" else "1" if rtype == "ITEM" else "")
            self.v_rheld.set("ITEM_NONE" if rtype == "MON" else "")
            self.loading = False
        if rtype == "NONE":
            a.pop("reward", None)
        else:
            try:
                amount = int(self.v_ramount.get().strip())
            except ValueError:
                amount = 0
            a["reward"] = {"type": rtype, "id": self.v_rid.get().strip(), "amount": amount}
            if rtype == "MON":
                a["reward"]["held"] = self.v_rheld.get().strip() or "ITEM_NONE"
        self.ach_tree.item(str(self.cur_ach), values=self.ach_row(self.cur_ach))
        self._update_ach_hints()
        self.set_dirty()

    def add_ach(self):
        achs = self.data["achievements"]
        if len(achs) >= gen.MAX_ACHIEVEMENTS:
            messagebox.showwarning("追加できません", "じっせきは %d 個までです" % gen.MAX_ACHIEVEMENTS)
            return
        n = len(achs) + 1
        new_id = "NEW_%d" % n
        while any(a["id"] == new_id for a in achs):
            n += 1
            new_id = "NEW_%d" % n
        achs.append({"id": new_id, "name": "あたらしい じっせき", "desc": "せつめい",
                     "tier": "BRONZE", "kind": "STAT", "param": "GAME_STAT_STEPS", "target": 1})
        i = len(achs) - 1
        self.ach_tree.insert("", "end", iid=str(i), values=self.ach_row(i))
        self.select_ach(i)
        self.set_dirty()
        self.e_id.focus_set()
        self.e_id.select_range(0, "end")

    def delete_last_ach(self):
        achs = self.data["achievements"]
        if len(achs) <= 1:
            return
        i = len(achs) - 1
        if i < self.saved_ach:
            msg = ("「%s」は 保存ずみの じっせきです。\n\n"
                   "すでに配布したROMのセーブで 解除されている場合、\n"
                   "あとで追加した じっせきが 解除ずみに なってしまいます。\n\n"
                   "本当に削除しますか？" % achs[i]["name"])
            if not messagebox.askyesno("削除の確認", msg, icon="warning"):
                return
            self.saved_ach = i
        achs.pop()
        self.ach_tree.delete(str(i))
        self.cur_ach = None
        self.select_ach(i - 1)
        self.set_dirty()

    # ------------------------------------------------------------------ #
    # 報酬 タブ
    # ------------------------------------------------------------------ #
    def _build_rew_tab(self):
        left = ttk.Frame(self.rew_tab)
        left.pack(side="left", fill="both", expand=True, padx=(6, 3), pady=6)
        cols = ("no", "count", "item", "amount")
        self.rew_tree = ttk.Treeview(left, columns=cols, show="headings", selectmode="browse")
        for c, label, w in (("no", "No", 40), ("count", "必要な解除数", 110), ("item", "アイテム", 260), ("amount", "個数", 70)):
            self.rew_tree.heading(c, text=label)
            self.rew_tree.column(c, width=w, stretch=(c == "item"), anchor="w" if c == "item" else "center")
        self.rew_tree.pack(side="left", fill="both", expand=True)
        self.rew_tree.bind("<<TreeviewSelect>>", lambda e: self._on_rew_select())

        right = ttk.Frame(self.rew_tab, width=400)
        right.pack(side="left", fill="y", padx=(3, 6), pady=6)
        btns = ttk.Frame(right)
        btns.pack(fill="x", pady=(0, 8))
        ttk.Button(btns, text="＋ 報酬を追加", command=self.add_rew).pack(side="left")
        ttk.Button(btns, text="最後の報酬を削除", command=self.delete_last_rew).pack(side="left", padx=6)

        form = ttk.LabelFrame(right, text="えらんだ 報酬")
        form.pack(fill="both", expand=True)
        form.columnconfigure(1, weight=1)

        ttk.Label(form, text="必要な解除数").grid(row=0, column=0, sticky="w", padx=6, pady=4)
        self.v_count = tk.StringVar()
        ttk.Spinbox(form, from_=1, to=gen.MAX_ACHIEVEMENTS, textvariable=self.v_count, width=8).grid(row=0, column=1, sticky="w", padx=6, pady=4)
        self.l_count_hint = ttk.Label(form, text="", foreground="#777")
        self.l_count_hint.grid(row=1, column=1, sticky="w", padx=6)

        ttk.Label(form, text="アイテム").grid(row=2, column=0, sticky="w", padx=6, pady=4)
        self.v_item = tk.StringVar()
        self.c_item = FilterCombobox(form, textvariable=self.v_item, width=30)
        self.c_item.set_all_values(self.symbols.get("ITEM", []))
        self.c_item.grid(row=2, column=1, sticky="ew", padx=6, pady=4)
        self.l_item_hint = ttk.Label(form, text="", foreground="#c33")
        self.l_item_hint.grid(row=3, column=1, sticky="w", padx=6)

        ttk.Label(form, text="個数").grid(row=4, column=0, sticky="w", padx=6, pady=4)
        self.v_amount = tk.StringVar()
        ttk.Spinbox(form, from_=1, to=255, textvariable=self.v_amount, width=8).grid(row=4, column=1, sticky="w", padx=6, pady=4)

        ttk.Label(form, text=(
            "・解除数が とどいた報酬は ぜんぶ もらえます\n"
            "  (config.h の ACH_REWARDS_VIA_NPC が有効なら NPC から)\n"
            "・新しい報酬は いちばん下に 追加されます\n"
            "  (並び順 = セーブのフラグ番号。解除数の順に ならべる必要は ありません)\n"
            "・報酬は %d 個まで" % gen.MAX_REWARDS),
            foreground="#555", justify="left").grid(row=5, column=0, columnspan=2, sticky="w", padx=6, pady=(10, 6))

        for v in (self.v_count, self.v_item, self.v_amount):
            v.trace_add("write", lambda *a: self._on_rew_field())

    def rew_row(self, i):
        r = self.data["rewards"][i]
        return (i + 1, r["count"], r["item"], r["amount"])

    def refresh_rew_tree(self):
        self.rew_tree.delete(*self.rew_tree.get_children())
        for i in range(len(self.data["rewards"])):
            self.rew_tree.insert("", "end", iid=str(i), values=self.rew_row(i))

    def select_rew(self, i):
        self.rew_tree.selection_set(str(i))
        self.rew_tree.see(str(i))

    def _on_rew_select(self):
        sel = self.rew_tree.selection()
        if not sel:
            return
        self.cur_rew = int(sel[0])
        r = self.data["rewards"][self.cur_rew]
        self.loading = True
        self.v_count.set(str(r["count"]))
        self.v_item.set(r["item"])
        self.v_amount.set(str(r["amount"]))
        self.loading = False
        self._update_rew_hints()

    def _update_rew_hints(self):
        if self.cur_rew is None:
            return
        r = self.data["rewards"][self.cur_rew]
        total = len(self.data["achievements"])
        self.l_count_hint.configure(text="(いまの じっせきの数: %d)" % total,
                                    foreground="#c33" if r["count"] > total else "#777")
        self.l_item_hint.configure(text="" if r["item"] in self.symbols.get("ITEM", []) else "⚠ %s は見つかりません" % (r["item"] or "アイテム"))

    def _on_rew_field(self):
        if self.loading or self.cur_rew is None:
            return
        r = self.data["rewards"][self.cur_rew]
        for key, var in (("count", self.v_count), ("amount", self.v_amount)):
            try:
                r[key] = int(var.get().strip())
            except ValueError:
                r[key] = 0
        r["item"] = self.v_item.get().strip()
        self.rew_tree.item(str(self.cur_rew), values=self.rew_row(self.cur_rew))
        self._update_rew_hints()
        self.set_dirty()

    def add_rew(self):
        rews = self.data["rewards"]
        if len(rews) >= gen.MAX_REWARDS:
            messagebox.showwarning("追加できません", "報酬は %d 個までです" % gen.MAX_REWARDS)
            return
        last = max([r["count"] for r in rews] + [0])
        count = max(1, min(last + 10, len(self.data["achievements"])))
        rews.append({"count": count, "item": "ITEM_RARE_CANDY", "amount": 1})
        i = len(rews) - 1
        self.rew_tree.insert("", "end", iid=str(i), values=self.rew_row(i))
        self.select_rew(i)
        self.set_dirty()

    def delete_last_rew(self):
        rews = self.data["rewards"]
        if not rews:
            return
        i = len(rews) - 1
        if i < self.saved_rew:
            if not messagebox.askyesno("削除の確認",
                                       "この報酬は 保存ずみです。\nすでに受け取った セーブでは、あとで この位置に追加した報酬が\n"
                                       "受け取りずみに なってしまいます。\n\n本当に削除しますか？", icon="warning"):
                return
            self.saved_rew = i
        rews.pop()
        self.rew_tree.delete(str(i))
        self.cur_rew = None
        if rews:
            self.select_rew(i - 1)
        self.set_dirty()

    # ------------------------------------------------------------------ #
    # 下の操作パネル
    # ------------------------------------------------------------------ #
    def _build_bottom(self):
        bar = ttk.Frame(self)
        bar.pack(fill="x", padx=6, pady=6)
        ttk.Button(bar, text="チェック", command=self.check).pack(side="left")
        ttk.Button(bar, text="保存して生成", command=self.save_and_generate).pack(side="left", padx=6)
        self.b_build = ttk.Button(bar, text="保存・生成して ビルド (make)", command=self.save_generate_build)
        self.b_build.pack(side="left")
        self.l_status = ttk.Label(bar, text="")
        self.l_status.pack(side="right")

        logf = ttk.Frame(self)
        logf.pack(fill="both", padx=6, pady=(0, 6))
        self.t_log = tk.Text(logf, height=9, state="disabled", wrap="word", background="#f7f7f7")
        sb = ttk.Scrollbar(logf, orient="vertical", command=self.t_log.yview)
        self.t_log.configure(yscrollcommand=sb.set)
        self.t_log.pack(side="left", fill="both", expand=True)
        sb.pack(side="left", fill="y")
        self.t_log.tag_configure("err", foreground="#c00")
        self.t_log.tag_configure("warn", foreground="#a60")
        self.t_log.tag_configure("ok", foreground="#070")

    def log(self, text, tag=None):
        self.t_log.configure(state="normal")
        self.t_log.insert("end", text + "\n", tag)
        self.t_log.see("end")
        self.t_log.configure(state="disabled")

    def set_dirty(self, dirty=True):
        self.dirty = dirty
        self.update_title()

    def update_title(self):
        self.title("じっせき・報酬エディタ%s" % (" *" if self.dirty else ""))
        self.l_status.configure(text="じっせき %d / %d    報酬 %d / %d%s" % (
            len(self.data["achievements"]), gen.MAX_ACHIEVEMENTS, len(self.data["rewards"]), gen.MAX_REWARDS,
            "    (未保存)" if self.dirty else ""))

    def check(self, quiet=False):
        errors, warnings = gen.validate(self.data, self.symbols, self.charset)
        for w in warnings:
            self.log("[注意] " + w, "warn")
        for e in errors:
            self.log("[エラー] " + e, "err")
        if not errors and not quiet:
            self.log("チェックOK%s" % (" (注意 %d 件)" % len(warnings) if warnings else ""), "ok")
        if errors:
            messagebox.showerror("エラーがあります", "\n".join(errors[:12]) + ("\n…ほか %d 件" % (len(errors) - 12) if len(errors) > 12 else ""))
        return not errors

    def save_and_generate(self):
        if not self.check(quiet=True):
            return False
        gen.save_data(self.data)
        gen.generate(self.data)
        self.saved_ach = len(self.data["achievements"])
        self.saved_rew = len(self.data["rewards"])
        self.set_dirty(False)
        if self.cur_ach is not None:
            self._on_ach_select_keep()
        self.log("保存して生成しました (じっせき %d / 報酬 %d)" % (self.saved_ach, self.saved_rew), "ok")
        return True

    def _on_ach_select_keep(self):
        # 保存後は ID が かえられなくなるので フォームを更新
        self.loading = True
        existing = self.cur_ach < self.saved_ach
        self.e_id.configure(state="disabled" if existing else "normal")
        self.l_id_note.configure(text="保存ずみの ID は かえられません (C のコードで使われている場合があるため)")
        self.loading = False

    def save_generate_build(self):
        if self.building or not self.save_and_generate():
            return
        self.building = True
        self.b_build.configure(state="disabled")
        self.log("ビルド中…")

        def run():
            try:
                p = subprocess.run([sys.executable, os.path.join("scripts", "make.py")], cwd=ROOT,
                                   stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
                out = p.stdout.decode("utf-8", errors="replace") if p.stdout else ""
                if "�" in out:
                    out = p.stdout.decode("cp932", errors="replace")
                ok = p.returncode == 0 and " error" not in out.lower()
            except Exception as ex:
                out, ok = str(ex), False
            self.after(0, lambda: self._build_done(out, ok))

        threading.Thread(target=run, daemon=True).start()

    def _build_done(self, out, ok):
        lines = [l for l in out.splitlines() if l.strip()]
        for l in lines[-25:]:
            self.log("  " + l, "err" if "error" in l.lower() else None)
        self.log("ビルド完了 → test.gba" if ok else "ビルドに失敗しました (上のエラーを確認してください)", "ok" if ok else "err")
        self.building = False
        self.b_build.configure(state="normal")

    def on_close(self):
        if self.dirty:
            ans = messagebox.askyesnocancel("終了", "保存していない変更があります。保存して生成しますか？")
            if ans is None:
                return
            if ans and not self.save_and_generate():
                return
        self.destroy()


if __name__ == "__main__":
    import tkinter.font  # noqa: F401  (tk.font を使えるようにする)
    Editor().mainloop()
