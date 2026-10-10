#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
じっせき(Achievements) のデータ生成スクリプト

  データ: scripts/achievements.json  (じっせき と 報酬)
          → ふだんは achievements_editor.bat (エディタ) で編集してください

  python -I scripts/achievements_gen.py
で以下の3ファイルを再生成します。
  strings/achievements.string      ... 名前・説明文・セリフ (かな)
  src/achievements_data.h          ... じっせきテーブル本体
  src/achievements_rewards.h       ... マイルストーン報酬テーブル

★重要★ 既に配布したセーブデータとの互換性のため、じっせき・報酬の追加は必ず末尾に行ってください。
   (並び順 = セーブ上のフラグ番号です。途中の挿入・削除・入れ替えはしないでください)
   じっせき 最大 128 個 / 報酬 最大 32 個。
"""
import io, json, os, re, sys

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
DATA_FILE = os.path.join(ROOT, "scripts", "achievements.json")

MAX_ACHIEVEMENTS = 128 # ACH_MAX_COUNT (include/new/achievements.h)
MAX_REWARDS = 32       # 報酬のフラグ数 (ACH_FLAG_GIFT_PIKACHU = ACH_FLAG_REWARD_START + 32 の手前まで)
NAME_WARN_CHARS = 12   # 一覧に収まる名前の目安
DESC_WARN_CHARS = 24   # 説明 1行の目安

TIERS = ["BRONZE", "SILVER", "GOLD", "PLATINUM"]
KINDS = ["FLAG", "STAT", "DEX", "SPECIES", "FUNC", "ITEM"]   # [hack] 2026-10-10 ITEM = バッグにその道具がある
TIER_C = {t: "ACH_TIER_" + t for t in TIERS}
KIND_C = {"FLAG": "ACH_KIND_FLAG", "STAT": "ACH_KIND_STAT", "DEX": "ACH_KIND_DEX_COUNT",
          "SPECIES": "ACH_KIND_SPECIES", "FUNC": "ACH_KIND_FUNC", "ITEM": "ACH_KIND_ITEM"}

# その他のUI文字列 (ラベル, 本文)  ※改行は \\n (同じ窓で改行) / \\p (次の窓へ送る)。本物の改行は不可
UI = [
 ("gText_MenuAchievements",        "じっせき"),
 ("gText_AchievementsDescription", "あつめた じっせきを\\nかくにんする"),
 ("gText_Ach_Title",               "じっせき"),
 ("gText_Ach_Unlocked",            "じっせき『[BUFFER1]』を\\nかいじょした！"),
 ("gText_Ach_UnlockedMany",        "[BUFFER1]この じっせきを\\nかいじょした！"),
 ("gText_Ach_TierBronze",          "ブロンズ"),
 ("gText_Ach_TierSilver",          "シルバー"),
 ("gText_Ach_TierGold",            "ゴールド"),
 ("gText_Ach_TierPlatinum",        "プラチナ"),
 ("gText_Ach_Done",                "かいじょずみ"),
 ("gText_Ach_Slash",               "/"),
 ("gText_Ach_StatusDone",          "クリア"),
 ("gText_Ach_StatusLocked",        "ロック"),
 ("gText_Ach_Cursor",              "[EF]"),
 ("gText_Ach_Back",                "もどる"),
 ("gText_AchNpc_Greeting",         "おお！ きみの じっせきは\\nいま [BUFFER1]こ かいじょ されているね！"),
 ("gText_AchNpc_Next",             "つぎの ほうしゅうは\\n[BUFFER2]こ かいじょで もらえるよ！"),
 ("gText_AchNpc_AllDone",          "ぜんぶの ほうしゅうを\\nうけとったようだね！ すばらしい！"),
 ("gText_AchNpc_BagFull",          "おっと！ バッグが いっぱいだ！\\nあけてから また はなしかけておくれ"),
 ("gText_Ach_GiftParty",           "ほうしゅうとして\\nメタモンを もらった！"),
 ("gText_Ach_GiftPC",              "ほうしゅうの メタモンは\\nパソコンに おくられた！"),
 ("gText_Ach_GiftFail",            "ほうしゅうの メタモンを\\nうけとれなかった…"),
 ("gText_Ach_Reward",              "ほうしゅうとして\\n[BUFFER1]を [BUFFER2]こ もらった！"),
 ("gText_Ach_PopupTitle",          "じっせき かいじょ！"),
 ("gText_Ach_PopupMany",           "この じっせきを かいじょ"),
 ("gText_Ach_StatusClaim",         "うけとる"),
 ("gText_Ach_RewardLabel",         "ほうしゅう:"),
 ("gText_Ach_RewardTimes",         "x"),
 ("gText_Ach_RewardLv",            "Lv"),
 ("gText_Ach_ClaimItem",           "[BUFFER1]を [BUFFER2]こ もらった！"),
 ("gText_Ach_ClaimMonParty",       "[BUFFER1]を もらった！"),
 ("gText_Ach_ClaimMonPC",          "[BUFFER1]は\\nパソコンに おくられた！"),
 ("gText_Ach_ClaimBagFull",        "バッグが いっぱいで\\nうけとれない！"),
 ("gText_Ach_ClaimMonFull",        "てもちも パソコンも いっぱいで\\nうけとれない！"),
]

# じっせきごとの報酬 (一覧画面で Aボタンで受け取る)
REWARD_TYPES = ["NONE", "ITEM", "MON"]
REWARD_TYPE_C = {"NONE": "ACH_REWARD_NONE", "ITEM": "ACH_REWARD_ITEM", "MON": "ACH_REWARD_MON"}

def get_reward(a):
    """{"type", "id", "amount", "held"} を返す (報酬なしなら type=NONE)"""
    r = dict(a.get("reward") or {})
    r.setdefault("type", "NONE")
    r.setdefault("id", "")
    r.setdefault("amount", 1)
    r.setdefault("held", "ITEM_NONE")
    return r

# ------------------------------------------------------------------ #
# データの読み書き
# ------------------------------------------------------------------ #
def load_data(path=DATA_FILE):
    with io.open(path, encoding="utf-8") as f:
        return json.load(f)

def save_data(data, path=DATA_FILE):
    with io.open(path, "w", encoding="utf-8", newline="\n") as f:
        f.write(json.dumps(data, ensure_ascii=False, indent=1) + "\n")

# ------------------------------------------------------------------ #
# 定数一覧 (ヘッダから読む)
# ------------------------------------------------------------------ #
def _defines(path, prefix, exclude=()):
    if not os.path.exists(path):
        return []
    text = io.open(path, encoding="utf-8", errors="ignore").read()
    names = re.findall(r"^\s*#define\s+(%s\w+)" % prefix, text, re.M)
    return [n for n in names if not any(n.startswith(e) for e in exclude)]

def load_symbols(root=ROOT):
    inc = os.path.join(root, "include")
    ach_h = io.open(os.path.join(inc, "new", "achievements.h"), encoding="utf-8", errors="ignore").read()
    flags = _defines(os.path.join(inc, "constants", "flags.h"), "FLAG_") \
          + _defines(os.path.join(root, "src", "config.h"), "FLAG_")
    return {
        "FLAG": sorted(set(flags)),
        "STAT": _defines(os.path.join(inc, "constants", "game_stat.h"), "GAME_STAT_"),
        "SPECIES": _defines(os.path.join(inc, "constants", "species.h"), "SPECIES_"),
        "FUNC": re.findall(r"^\s*(ACH_FUNC_\w+)", ach_h, re.M),
        "ITEM": _defines(os.path.join(inc, "constants", "items.h"), "ITEM_"),
    }

def load_charset(root=ROOT):
    chars = set()
    path = os.path.join(root, "charmap.tbl")
    if os.path.exists(path):
        for line in io.open(path, encoding="utf-8", errors="ignore"):
            line = line.rstrip("\r\n")
            if "=" in line:
                chars.add(line.split("=", 1)[1])
    return chars

def _is_number(s):
    return re.match(r"^(0x[0-9A-Fa-f]+|\d+)$", str(s).strip()) is not None

def unknown_chars(text, charset):
    """ゲームで表示できない文字 ([BUFFER1] などの制御コードと \\n \\p \\l は除く)"""
    if not charset:
        return []
    plain = re.sub(r"\[[^\]]*\]|\\[npl]", "", text)
    return sorted(set(c for c in plain if c not in charset))

# ------------------------------------------------------------------ #
# チェック: (エラー, 警告) のリストを返す。エラーがあると生成しない
# ------------------------------------------------------------------ #
def validate(data, symbols=None, charset=None):
    symbols = symbols if symbols is not None else load_symbols()
    charset = charset if charset is not None else load_charset()
    errors, warnings = [], []
    achs, rewards = data.get("achievements", []), data.get("rewards", [])

    if not achs:
        errors.append("じっせきが 1つも ありません")
    if len(achs) > MAX_ACHIEVEMENTS:
        errors.append("じっせきが多すぎます (%d / 最大 %d)" % (len(achs), MAX_ACHIEVEMENTS))
    if len(rewards) > MAX_REWARDS:
        errors.append("報酬が多すぎます (%d / 最大 %d)" % (len(rewards), MAX_REWARDS))

    seen = set()
    for i, a in enumerate(achs):
        where = "じっせき %d (%s)" % (i + 1, a.get("id") or "IDなし")
        aid = a.get("id", "")
        if not re.match(r"^[A-Z][A-Z0-9_]*$", aid):
            errors.append("%s: ID は 半角英大文字・数字・_ で書いてください" % where)
        elif aid in seen:
            errors.append("%s: ID が ほかと かぶっています" % where)
        seen.add(aid)

        for key, label in (("name", "名前"), ("desc", "説明")):
            text = a.get(key, "")
            if not text.strip():
                errors.append("%s: %sが からっぽです" % (where, label))
            if "\n" in text or "\r" in text:
                errors.append("%s: %sに本物の改行があります (\\n と書いてください)" % (where, label))
            bad = unknown_chars(text, charset)
            if bad:
                errors.append("%s: %sに ゲームで使えない文字があります: %s" % (where, label, " ".join(bad)))

        name = a.get("name", "")
        if "\\n" in name or "\\p" in name:
            errors.append("%s: 名前は 1行にしてください" % where)
        elif len(name) > NAME_WARN_CHARS:
            warnings.append("%s: 名前が長めです (%dもじ / めやす %dもじ)" % (where, len(name), NAME_WARN_CHARS))

        lines = a.get("desc", "").split("\\n")
        if len(lines) > 2:
            errors.append("%s: 説明は 2行までです" % where)
        for n, line in enumerate(lines):
            if len(line) > DESC_WARN_CHARS:
                warnings.append("%s: 説明 %d行目が長めです (%dもじ / めやす %dもじ)" % (where, n + 1, len(line), DESC_WARN_CHARS))

        tier, kind, param = a.get("tier"), a.get("kind"), str(a.get("param", "")).strip()
        if tier not in TIERS:
            errors.append("%s: ランクが正しくありません" % where)
        if kind not in KINDS:
            errors.append("%s: 種別が正しくありません" % where)
        elif kind == "DEX":
            pass
        elif not param:
            errors.append("%s: パラメータ(条件)が からっぽです" % where)
        elif kind == "FLAG" and _is_number(param):
            pass
        elif param not in symbols.get(kind, []):
            errors.append("%s: %s が見つかりません (つづりを確認してください)" % (where, param))

        target = a.get("target")
        if not isinstance(target, int) or not (1 <= target <= 0xFFFFFFFF):
            errors.append("%s: 目標値は 1以上の数にしてください" % where)

        rw = get_reward(a)
        if rw["type"] not in REWARD_TYPES:
            errors.append("%s: 報酬の種類が正しくありません" % where)
        elif rw["type"] == "ITEM":
            if rw["id"] not in symbols.get("ITEM", []) or rw["id"] == "ITEM_NONE":
                errors.append("%s: 報酬のアイテム %s が見つかりません" % (where, rw["id"] or "(なし)"))
            if not isinstance(rw["amount"], int) or not (1 <= rw["amount"] <= 255):
                errors.append("%s: 報酬の個数は 1-255 にしてください" % where)
        elif rw["type"] == "MON":
            if rw["id"] not in symbols.get("SPECIES", []) or rw["id"] == "SPECIES_NONE":
                errors.append("%s: 報酬のポケモン %s が見つかりません" % (where, rw["id"] or "(なし)"))
            if not isinstance(rw["amount"], int) or not (1 <= rw["amount"] <= 100):
                errors.append("%s: 報酬のレベルは 1-100 にしてください" % where)
            if rw["held"] not in symbols.get("ITEM", []):
                errors.append("%s: もちもの %s が見つかりません" % (where, rw["held"]))

    for i, r in enumerate(rewards):
        where = "報酬 %d" % (i + 1)
        count, item, amount = r.get("count"), str(r.get("item", "")).strip(), r.get("amount")
        if not isinstance(count, int) or not (1 <= count <= MAX_ACHIEVEMENTS):
            errors.append("%s: 必要な解除数は 1-%d にしてください" % (where, MAX_ACHIEVEMENTS))
        elif count > len(achs):
            warnings.append("%s: 必要な解除数 %d が じっせきの数 (%d) より多いので もらえません" % (where, count, len(achs)))
        if item not in symbols.get("ITEM", []):
            errors.append("%s: アイテム %s が見つかりません" % (where, item or "(なし)"))
        if not isinstance(amount, int) or not (1 <= amount <= 255):
            errors.append("%s: 個数は 1-255 にしてください" % where)

    for label, text in UI:
        if "\n" in text:
            errors.append("%s: 本物の改行があります (\\n か \\p と書いてください)" % label)

    return errors, warnings

# ------------------------------------------------------------------ #
# 生成
# ------------------------------------------------------------------ #
def _write(path, text):
    with io.open(path, "w", encoding="utf-8", newline="\n") as f:
        f.write(text)

def generate(data, root=ROOT):
    achs, rewards = data["achievements"], data["rewards"]

    out = "// 自動生成: python scripts/achievements_gen.py (直接編集しないでください)\n\n"
    for label, text in UI:
        out += "#org @%s\n%s\n\n" % (label, text)
    for a in achs:
        out += "#org @gText_Ach_Name_%s\n%s\n\n" % (a["id"], a["name"])
        out += "#org @gText_Ach_Desc_%s\n%s\n\n" % (a["id"], a["desc"])
    _write(os.path.join(root, "strings", "achievements.string"), out)

    out = "// Generated by scripts/achievements_gen.py - do not edit directly.\n#pragma once\n\n"
    out += "#define ACH_COUNT %d\n\nenum AchievementId\n{\n" % len(achs)
    out += "".join("\tACH_%s,\n" % a["id"] for a in achs) + "};\n\n"
    out += "".join("extern const u8 gText_Ach_Name_%s[];\nextern const u8 gText_Ach_Desc_%s[];\n" % (a["id"], a["id"]) for a in achs)
    out += "\nconst struct Achievement gAchievements[ACH_COUNT] =\n{\n"
    for a in achs:
        param = "0" if a["kind"] == "DEX" else str(a["param"]).strip()
        rw = get_reward(a)
        if rw["type"] == "NONE":
            reward = "ACH_REWARD_NONE, 0, 0, 0"
        else:
            reward = "%s, %d, %s, %s" % (REWARD_TYPE_C[rw["type"]], rw["amount"], rw["id"],
                                         rw["held"] if rw["type"] == "MON" else "ITEM_NONE")
        out += "\t[ACH_%s] = {gText_Ach_Name_%s, gText_Ach_Desc_%s, %s, %s, %s, %d, %s},\n" % (
            a["id"], a["id"], a["id"], TIER_C[a["tier"]], KIND_C[a["kind"]], param, a["target"], reward)
    out += "};\n"
    _write(os.path.join(root, "src", "achievements_data.h"), out)

    out = "// Generated by scripts/achievements_gen.py - do not edit directly.\n#pragma once\n\n"
    out += "//Milestone rewards: given once each when the unlock count is reached. The position is the save flag index.\n"
    out += "#define NUM_ACH_REWARDS %d\n\n" % len(rewards)
    out += "static const struct AchievementReward sAchievementRewards[NUM_ACH_REWARDS] =\n{\n"
    out += "".join("\t{%d, %s, %d},\n" % (r["count"], r["item"], r["amount"]) for r in rewards)
    out += "};\n"
    _write(os.path.join(root, "src", "achievements_rewards.h"), out)

    # ビルドは .c 自身の更新日時しか見ないので、生成したヘッダを使う achievements.c を更新扱いにする
    os.utime(os.path.join(root, "src", "achievements.c"), None)

def main():
    data = load_data()
    errors, warnings = validate(data)
    for w in warnings:
        print("[注意] " + w)
    for e in errors:
        print("[エラー] " + e)
    if errors:
        print("エラーがあるため 生成しませんでした")
        return 1
    generate(data)
    print("generated %d achievements, %d rewards" % (len(data["achievements"]), len(data["rewards"])))
    return 0

if __name__ == "__main__":
    sys.exit(main())
