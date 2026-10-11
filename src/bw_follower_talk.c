#include "defines.h"
#include "bw_follower_internal.h"
#include "../include/pokemon.h"
#include "../include/sound.h"
#include "../include/string_util.h"
#include "../include/random.h"

/* ---- 話しかけたときの反応 ----
   気分の決め方は pokeemerald-expansion の `GetFollowerAction()`（src/event_object_movement.c）に
   合わせた。重みを振っておいて、なつき度・HP・状態異常で重みを付け替え、そこから 1 つ選ぶ。
   ★ 本家 HGSS は「きげん」を 1 つの値で持って増減させるが（`FieldSystem_UnkSub108_*Mood`）、
   そのための RAM がこちらには無いので、expansion 式の「その場で重みを振る」ほうを採った。
   ★★★ **文章に差し込みコード（FD xx）を入れてはいけない。**
   素 FRLG の `ShowFieldMessage(str)` は中で `StringExpandPlaceholders(gStringVar4, str)` を呼ぶ
   （pokefirered `field_message_box.c`）。こちらは組み立てた文章を **gStringVar4 に置いて渡す**ので、
   差し込みがあると**同じ入れ物の中で展開されて自分を壊す**。展開後のほうが長いため、
   読んでいる先を書き潰して暴走する（実機 2026-09-20「話しかけたらクラッシュ」の正体）。
   → **ニックネームはこちらで前に足す。**文章は「〜は …」から始まる形にしてある
   （strings/bw_follower.string）。差し込みが無ければ展開は「自分から自分への写し」になり無害。 */
enum BwFollowerEmotion
{
    BWF_EMO_HAPPY, BWF_EMO_NEUTRAL, BWF_EMO_SAD, BWF_EMO_UPSET, BWF_EMO_ANGRY,
    BWF_EMO_PENSIVE, BWF_EMO_LOVE, BWF_EMO_SURPRISE, BWF_EMO_CURIOUS, BWF_EMO_MUSIC,
    BWF_EMO_SICK, BWF_EMO_COUNT
};

extern const u8 gText_BwFollower_Happy1[], gText_BwFollower_Happy2[];
extern const u8 gText_BwFollower_Neutral1[], gText_BwFollower_Neutral2[];
extern const u8 gText_BwFollower_Sad1[], gText_BwFollower_Sad2[];
extern const u8 gText_BwFollower_Upset1[], gText_BwFollower_Upset2[];
extern const u8 gText_BwFollower_Angry1[], gText_BwFollower_Angry2[];
extern const u8 gText_BwFollower_Pensive1[], gText_BwFollower_Pensive2[];
extern const u8 gText_BwFollower_Love1[], gText_BwFollower_Love2[];
extern const u8 gText_BwFollower_Surprise1[], gText_BwFollower_Surprise2[];
extern const u8 gText_BwFollower_Curious1[], gText_BwFollower_Curious2[];
extern const u8 gText_BwFollower_Music1[], gText_BwFollower_Music2[];
extern const u8 gText_BwFollower_Sick1[], gText_BwFollower_Sick2[];

static const u8 *const sBwFollowerTalk[BWF_EMO_COUNT][2] = {
    [BWF_EMO_HAPPY]    = { gText_BwFollower_Happy1,    gText_BwFollower_Happy2 },
    [BWF_EMO_NEUTRAL]  = { gText_BwFollower_Neutral1,  gText_BwFollower_Neutral2 },
    [BWF_EMO_SAD]      = { gText_BwFollower_Sad1,      gText_BwFollower_Sad2 },
    [BWF_EMO_UPSET]    = { gText_BwFollower_Upset1,    gText_BwFollower_Upset2 },
    [BWF_EMO_ANGRY]    = { gText_BwFollower_Angry1,    gText_BwFollower_Angry2 },
    [BWF_EMO_PENSIVE]  = { gText_BwFollower_Pensive1,  gText_BwFollower_Pensive2 },
    [BWF_EMO_LOVE]     = { gText_BwFollower_Love1,     gText_BwFollower_Love2 },
    [BWF_EMO_SURPRISE] = { gText_BwFollower_Surprise1, gText_BwFollower_Surprise2 },
    [BWF_EMO_CURIOUS]  = { gText_BwFollower_Curious1,  gText_BwFollower_Curious2 },
    [BWF_EMO_MUSIC]    = { gText_BwFollower_Music1,    gText_BwFollower_Music2 },
    [BWF_EMO_SICK]     = { gText_BwFollower_Sick1,     gText_BwFollower_Sick2 },
};

/* 素の重み（expansion と同じ値） */
static const u8 sBwFollowerEmoWeight[BWF_EMO_COUNT] = {
    [BWF_EMO_HAPPY] = 10, [BWF_EMO_NEUTRAL] = 15, [BWF_EMO_SAD] = 5,
    [BWF_EMO_UPSET] = 15, [BWF_EMO_ANGRY] = 15, [BWF_EMO_PENSIVE] = 15,
    [BWF_EMO_LOVE] = 0, [BWF_EMO_SURPRISE] = 10, [BWF_EMO_CURIOUS] = 10,
    [BWF_EMO_MUSIC] = 15, [BWF_EMO_SICK] = 0,
};

void BwFollowerTalk(void)
{
    struct Pokemon *mon = BwFollowerLeadMon();
    u8 w[BWF_EMO_COUNT];
    u32 hp, maxHp;
    u16 friendship, total, roll;
    u8 i, emo;

    for (i = 0; i < BWF_EMO_COUNT; ++i)
        w[i] = sBwFollowerEmoWeight[i];

    if (mon == NULL)
    {
        StringCopy(gStringVar4, sBwFollowerTalk[BWF_EMO_NEUTRAL][0]);
        return;
    }

    /* ★ ニックネームを前に、文章をうしろに。gStringVar1 は置き場として借りるだけ */
    GetMonData(mon, MON_DATA_NICKNAME, gStringVar1);
    PlayCry1(GetMonData(mon, MON_DATA_SPECIES2, NULL), 0);

    friendship = GetMonData(mon, MON_DATA_FRIENDSHIP, NULL);
    if (friendship > 80)
    {
        w[BWF_EMO_HAPPY] = 20; w[BWF_EMO_UPSET] = 5; w[BWF_EMO_ANGRY] = 5;
        w[BWF_EMO_LOVE] = 20;  w[BWF_EMO_MUSIC] = 20;
    }
    if (friendship > 170)
    {
        w[BWF_EMO_HAPPY] = 30; w[BWF_EMO_LOVE] = 30;
    }

    hp = GetMonData(mon, MON_DATA_HP, NULL);
    maxHp = GetMonData(mon, MON_DATA_MAX_HP, NULL);
    if (maxHp != 0 && hp * 100 / maxHp < 50)
        w[BWF_EMO_SAD] = 30;
    if (maxHp != 0 && hp * 100 / maxHp < 20)
        w[BWF_EMO_SAD] = 45;
    if (GetMonData(mon, MON_DATA_STATUS, NULL) != 0)
    {
        w[BWF_EMO_SICK] = 40;
        w[BWF_EMO_SAD] = 20;
    }

    total = 0;
    for (i = 0; i < BWF_EMO_COUNT; ++i)
        total += w[i];

    emo = BWF_EMO_NEUTRAL;
    if (total != 0)
    {
        roll = Random() % total;
        for (i = 0; i < BWF_EMO_COUNT; ++i)
        {
            if (roll < w[i])
            {
                emo = i;
                break;
            }
            roll -= w[i];
        }
    }

    StringCopy(StringCopy(gStringVar4, gStringVar1), sBwFollowerTalk[emo][Random() & 1]);
}
