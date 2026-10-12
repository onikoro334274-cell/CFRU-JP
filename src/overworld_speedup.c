#include "defines.h"
#include "hack_profile.h"
#include "../include/event_data.h"
#include "../include/main.h"
#include "../include/palette.h"
#include "../include/script.h"
#include "../include/sprite.h"
#include "../include/task.h"
#include "../include/fieldmap.h"
#include "../include/event_object_movement.h"
#include "../include/field_player_avatar.h"
#include "../include/constants/metatile_behaviors.h"
#include "../include/new/dexnav.h"
#include "Tables/hoenn_script_flags.h"
#include "hack_debug_menu.h"   //[hack] patches/0331 デバッグメニュー（-DHACK_DEBUG_MENU のときだけ）
#include "hack_network_trade.h"   //[hack] patches/0336 ネット交換

/*
    フィールドの倍速（1〜4 倍）— 2026-10-01 ユーザー「その倍速を設定に組み込んでほしい」

    元: soulgold（参考/soulgold/src/overworld.c の CB2_Overworld と OverworldSpeedup_AdditionalIterations）。
    経緯: docs/research/2026-10-01_hns-overworld-speed.md

    素 FRLG の CB2_Overworld（08055E74）を functionrewrites で丸ごとこれに置き換える。
    番地は変わらないので、`gMain.callback2 == CB2_Overworld` と比べている所（dexnav.c・dns.c）はそのまま効く。
    中身は素と同じ（暗転中は VBlank を止めて OverworldBasic・終わったら SetFieldVBlankCallback）で、
    OverworldBasic のあとに「スプライトを動かす・カメラを追う」を余分に回す。
      - 人の歩き（主人公・NPC・連れ歩き）はスプライトのコールバックで 1 コマずつ進むので、回した数だけ速くなる
      - タスク（RunTasks）・スクリプト・タイルのアニメは 1 フレームに 1 回のまま（soulgold と同じ）
    余分に回さないとき: スクリプトが動いている（会話・イベント中の動き）／ずかんナビで探している／氷の床（2026-10-05）。
    ★ soulgold の「R を押している間は等倍」は入れていない（CFRU では R がずかんナビの呼び出しに使われうる）。

    設定は変数 HOENN_VAR_OVERWORLD_SPEED（0 = 1 倍 … 3 = 4 倍）。せってい画面（src/bw_option_menu.c）が書く。
*/

#define OverworldBasic_ ((void (*)(void)) (0x08055E38 | 1))          /* 素 FRLG（CB2_OverworldBasic 08055E68 が呼ぶ）*/
#define OverworldCB2_ ((MainCallback) (0x08055E74 | 1))
#define SetFieldVBlankCallback_ ((void (*)(void)) (0x080562C4 | 1))  /* 素 FRLG（CB2_Overworld の最後の BL）*/
/* ★ 次の 2 つは BPRJ.ld に無い（6.29）。OverworldBasic の BL の並び（pokefirered と同じ順）から取った */
#define CameraUpdate_ ((void (*)(void)) (0x0805A46C | 1))
#define UpdateCameraPanning_ ((void (*)(void)) (0x0805A6E4 | 1))

#define OVERWORLD_SPEED_MAX 3                                         /* 4 倍 = 余分に 3 回 */

static bool8 IsIceBehavior(u32 behavior)
{
    return behavior == MB_ICE || behavior == MB_THIN_ICE || behavior == MB_CRACKED_ICE;
}

/* ★ 2026-10-05 ユーザー「いてだきのどうくつで移動速度上げているとバグる 氷の床だけ1倍速にする」:
   氷の床（すべる氷・薄氷・ひびの入った氷）は 1 歩ごとの処理（素の IcefallCaveIcePerStepCallback・ルネジムの氷）が
   フレームで数えて割るので、歩きだけ速くすると食い違う → 今いるマスか前のマスが氷なら等倍 */
static bool8 PlayerOnIce(void)
{
    if (gPlayerAvatar->eventObjectId >= EVENT_OBJECTS_COUNT)
        return TRUE; /* No valid player object: suppress extra movement ticks. */
    struct EventObject* player = &gEventObjects[gPlayerAvatar->eventObjectId];

    return IsIceBehavior(MapGridGetMetatileBehaviorAt(player->currentCoords.x, player->currentCoords.y))
        || IsIceBehavior(MapGridGetMetatileBehaviorAt(player->previousCoords.x, player->previousCoords.y));
}

u8 OverworldSpeedup_ExtraIterations(void)
{
    u16 speed = VarGet(HOENN_VAR_OVERWORLD_SPEED);

    if (speed == 0 || speed > OVERWORLD_SPEED_MAX)
        return 0;
    if (ScriptContext2_IsEnabled() || IsDexNavHudActive())
        return 0;
    if (PlayerOnIce())
        return 0;
    return speed;
}

void HackMomoIce_RefreshParty(void);
void HackTrainingPouch_FieldTick(void);
void HackMoemonSwitch_FieldTick(void);
void HackMomSavings_FieldTick(void);
void HackMonId_FieldTick(void);
void HackPhone_FieldTick(void);   /* src/hack_phone.c（2026-10-11 電話帳の着信） */
void HackHabitatSwitch_FieldTick(void);
void HackFriendOt_FieldTick(void);
void HackWatashi_FieldTick(void);   /* src/hack_watashitachi.c（2026-10-12 #99 フロンティアへ行けるまで 10 人を隠す） */
#define HACK_FLAG_ALWAYS_ON 0xF0B   /* 常に立っている旗（予約フラグ・5e の台帳 2026-10-11）。生成器 build_hoenn_scripts.PEOPLE_HIDE_FLAG と同じ値 */

/* ★ 2026-10-10 ユーザー「CFRU-DPEの最適化」→「2 実装」（測ってから重い所を直す）: 計測用のビルドだけ（build_rom.py --profile・-DHACK_PROFILE）。
   VBlank（走査線 160）を 0 にした走査線の数で、区間ごとの合計と最大を 0x0203F800 に書く（tools/rom/profile_overworld.py が mGBA で読む）。
   [0] 'PROF' [1] 測ったコマの数 [2] 処理落ちしたコマ（前の CB2 から VBlank が 2 回以上） [3] 区間の数 / 区間 k: [4+2k] 合計 [5+2k] 最大
   区間: 0 CB1（入力・1 歩ごとの判定＝CB2 に入るまで） 1〜7 自作の毎コマの処理 8 素の OverworldBasic 9 倍速の追加分 10 CB2 の終わりまでの全部
   ふつうのビルドには何も入らない */
#ifdef HACK_PROFILE
#define PROF_BASE ((u32*) 0x0203F800)
#define PROF_SECTIONS 28
#define PROF_WORDS (370 + 3) /* Includes wild_shuffle.c counters 370..372. */
#define PROF_VCOUNT (*(vu16*) 0x04000006)
/* 区間 11〜16 は CB1 の中（ProfCB1 が素の DoCB1_Overworld 08055D0C と同じ順に呼ぶ）・17 は CB1 に入るまで（VBlank の割り込みなど）・
   18〜26 は素の OverworldBasic 08055E38 の 9 つ（ProfOverworldBasic が同じ順に呼ぶ）。番地は日本語版の ROM を逆アセンブルして確かめた */
static u32 ProfSampleTime(u32* frameBase)
{
    /* VCOUNT とコマの数の間に VBlank が入ると 1 コマ（228 本）飛ぶので、コマの数が前後で同じになるまで読み直す */
    u32 c, v;
    const vu32* counter = &gMain.vblankCounter2;
    do
    {
        c = *counter;
        v = PROF_VCOUNT;
    } while (c != *counter || v == 160);   /* 160 は割り込みが来る線（コマの数が増える前か後か分からない） */
    u32 base = c * 228u;
    if (frameBase != NULL)
        *frameBase = base;
    return base + (v > 160 ? v - 160 : v + 68);
}
static u32 ProfNow(void)
{
    return ProfSampleTime(NULL);
}
u32 HackProfile_Now(void)
{
    return ProfNow();
}
static void ProfAdd(u32 k, u32 lines)
{
    u32* p = PROF_BASE;
    if (p[0] != 0x464F5250)   /* 'PROF' */
    {
        Memset(p, 0, PROF_WORDS * sizeof(*p));
        p[0] = 0x464F5250;
        p[3] = PROF_SECTIONS;
    }
    p[4 + 2 * k] += lines;
    if (lines > p[5 + 2 * k])
        p[5 + 2 * k] = lines;
    p[300 + k] += lines;   /* このコマの値（CB1 の始めで 0 に戻す） */
}
//呼び出しの行はふだんと同じ形のまま前後にはさむ（検算が「    関数名();」の行を探す）
#define PROF_BEGIN() profT = ProfNow()
#define PROF_END(k) ProfAdd(k, ProfNow() - profT)
#define PROF_CALL(k, call) do { u32 _t = ProfNow(); call; ProfAdd(k, ProfNow() - _t); } while (0)

typedef void (*ProfVoidFn)(void);
#define PF(addr) ((ProfVoidFn) ((addr) | 1))
struct ProfFieldInput { u8 b[4]; };   /* 素の struct FieldInput（4 バイト・+2 が dpadDirection） */
#define ProfCB1_Overworld 0x08055DF5
#define ProfCB2_Overworld 0x08055E75

static void ProfRunTasks(void)
{
    u32 id = FindFirstActiveTask();
    if (id == NUM_TASKS)
        return;
    do
    {
        struct Task* t = &gTasks[id];
        void (*func)(u8) = t->func;
        u32 st = ProfNow();
        func(id);
        st = ProfNow() - st;
        if (st > PROF_BASE[265])
        {
            PROF_BASE[264] = (u32) func;
            PROF_BASE[265] = st;
        }
        id = t->next;
    } while (id != 0xFF);
}

/* 素の VBlankCB_Field（080562D4）を包む（割り込みの中＝走査線の数だけで測る）。区間 27 */
static void ProfVBlankField(void)
{
    u32 v0 = PROF_VCOUNT;
    ((void (*)(void)) 0x080562D5)();
    u32 v1 = PROF_VCOUNT;
    ProfAdd(27, v1 >= v0 ? v1 - v0 : v1 + 228 - v0);
}

/* 素の OverworldBasic（08055E38）と同じ 9 つを同じ順に */
static void ProfOverworldBasic(void)
{
    PROF_CALL(18, PF(0x08069368)());   /* ScriptContext_RunScript */
    PROF_CALL(19, ProfRunTasks());     /* RunTasks（素の 08076D10 と同じ順に呼び、いちばん時間のかかったタスクを [264] 関数・[265] 本数に） */
    PROF_CALL(20, PF(0x080066D8)());   /* AnimateSprites */
    PROF_CALL(21, PF(0x0805A46C)());   /* CameraUpdate */
    PROF_CALL(22, PF(0x0805A6E4)());   /* UpdateCameraPanning */
    PROF_CALL(23, PF(0x0806FC74)());   /* UpdatePaletteFade */
    PROF_CALL(24, PF(0x0806F760)());   /* UpdateTilesetAnimations */
    PROF_CALL(25, PF(0x080F7810)());   /* DoScheduledBgTilemapCopiesToVram */
    PROF_CALL(26, PF(0x08006724)());   /* BuildOamBuffer */
}

/* 素の DoCB1_Overworld（08055D0C）と同じ順に */
static void ProfDoCB1(u16 newKeys, u16 heldKeys)
{
    struct ProfFieldInput in;

    PROF_CALL(11, PF(0x08113684)());                                              /* QL_TryRunActions */
    PROF_CALL(12, PF(0x0805B774)());                                              /* UpdatePlayerAvatarTransitionState */
    PROF_CALL(13, (((void (*)(void*)) 0x0806C07D)(&in),                           /* FieldClearPlayerInput */
                   ((void (*)(void*, u16, u16)) 0x0806C0B1)(&in, newKeys, heldKeys),   /* FieldGetPlayerInput */
                   ((void (*)(void*)) 0x0806C525)(&in)));                         /* FieldInput_HandleCancelSignpost */
    if (!((u8 (*)(void)) 0x08069219)())                                           /* ArePlayerFieldControlsLocked */
    {
        u32 r;
        u32 t = ProfNow();
        r = ((u32 (*)(void*)) 0x0806C2BD)(&in);                                   /* ProcessPlayerFieldInput */
        t = ProfNow() - t;
        ProfAdd(14, t);
        /* 山（40 本より上）が出たコマに確かめた実績の番号（src/achievements.c は 1 コマに 1 つ・番号 = コマの数 % ACH_COUNT 100）
           ごとに [64 + 番号] 回数・[164 + 番号] 最大（0x0203F900〜0x0203FBA0） */
        if (t > 40)
        {
            u32 id = gMain.vblankCounter2 % 100;
            PROF_BASE[64 + id]++;
            if (t > PROF_BASE[164 + id])
                PROF_BASE[164 + id] = t;
        }
        if (r == TRUE)
        {
            if (*(u8*) 0x03005ED8 == 2)                                            /* gQuestLogPlaybackState == RECORDING */
                ((void (*)(void*)) 0x08113341)((void*) 0x030050B8);               /* QL_RecordFieldInput(&gFieldInputRecord) */
            PF(0x08069200)();                                                     /* LockPlayerFieldControls */
            PF(0x08097CDC)();                                                     /* DismissMapNamePopup */
        }
        else
        {
            PROF_CALL(15, ((void (*)(u8, u16, u16)) 0x0805AC9D)(in.b[2], newKeys, heldKeys));   /* player_step */
        }
    }
    PROF_CALL(16, PF(0x08111478)());                                              /* RunQuestLogCB */
}

/* 素の CB1_Overworld（08055DF4）と同じ形。フィールドにいる間だけ gMain.callback1 をこれに差し替える（計測用のビルドだけ） */
static void ProfCB1(void)
{
    ProfAdd(17, ProfNow() - gMain.vblankCounter2 * 228u);
    if ((u32) gMain.callback2 == ProfCB2_Overworld)
    {
        if (((u8 (*)(void)) 0x081137F5)() == 1 || *(u8*) 0x0203AD72 == 2)       /* QL_GetPlaybackState・gQuestLogState */
            PF(0x08055D88)();                                                     /* DoCB1_Overworld_QuestLogPlayback */
        else
            ProfDoCB1(gMain.newKeys, gMain.heldKeys);
    }
}
#else
#define PROF_BEGIN()
#define PROF_END(k)
#endif

void OverworldSpeedup_CB2Overworld(void)
{
    bool32 fading = (gPaletteFade->active != 0);
    u8 extra, i;
#ifdef HACK_PROFILE
    u32 profT;
    u32 frameStart;
    u32 start = ProfSampleTime(&frameStart);
    ProfAdd(0, start - frameStart);
    PROF_BASE[1]++;
    if (gMain.vblankCounter2 - PROF_BASE[4 + 2 * PROF_SECTIONS] > 1 && PROF_BASE[4 + 2 * PROF_SECTIONS] != 0)
        PROF_BASE[2]++;
    PROF_BASE[4 + 2 * PROF_SECTIONS] = gMain.vblankCounter2;   /* 前の CB2 の VBlank の数（区間の表の後ろ） */
#endif

    PROF_BEGIN();
    FlagSet(HACK_FLAG_ALWAYS_ON);   /* ★ 2026-10-11 常に立っている旗（ホウエンの「隠す人」672 か所の旗・tools/rom/build_hoenn_scripts.py の PEOPLE_HIDE_FLAG）。
                                       前は 0x5C（素の FLAG_HIDE_CERULEAN_CAVE_GUARD＝ななしまクリア）で、立っていないセーブでは
                                       111 ばんどうろの割れる岩の位置に看護師の絵が見えた。ビット 1 つなので毎コマでよい */
    HackWatashi_FieldTick();        /* ★ 2026-10-12 #99 わたしたち 10 人の隠す旗 0xF0D を「フロンティアへ行ける 0x13EB の逆」に保つ（src/hack_watashitachi.c） */
    HackMomoIce_RefreshParty();     /* ★ 2026-10-05 ももアイスを持たせた／外したら能力を計算し直す（src/hack_momo_ice.c） */
    PROF_END(1);
    PROF_BEGIN();
    HackMomSavings_FieldTick();     /* ★ 2026-10-06 お母さんからの電話（src/hack_mom_savings.c） */
    PROF_END(2);
    PROF_BEGIN();
    HackTrainingPouch_FieldTick();  /* ★ 2026-10-06 いくせいポーチの印を下ろす・育成の道具を持ったらポーチを渡す（src/hack_training_pouch.c） */
    PROF_END(3);
    PROF_BEGIN();
    HackMoemonSwitch_FieldTick();   /* ★ 2026-10-08 Moemonボタンを持っていなければ渡す（src/hack_moemon_switch.c・オミット版は渡さない） */
    PROF_END(4);
    PROF_BEGIN();
    HackHabitatSwitch_FieldTick();  /* ★ 2026-10-10 せいそくスイッチを持っていなければ渡す（src/hack_habitat_switch.c） */
    PROF_END(5);
    PROF_BEGIN();
    HackMonId_FieldTick();          /* ★ 2026-10-09 #70 手持ちの子に表 ID・裏 ID を付ける（src/hack_mon_id.c・前からいた子の一覧を作ったあと） */
    PROF_END(6);
    PROF_BEGIN();
    HackFriendOt_FieldTick();       /* ★ 2026-10-10 なつき度 255 の子は親を主人公に（src/hack_friend_ot.c・ID を付けたあと） */
    HackPhone_FieldTick();          /* ★ 2026-10-11 電話帳の相手からの着信（src/hack_phone.c・pm-start/poke-call） */
#ifndef HACK_PROFILE   /* ★ 2026-10-12 5e のレビュー C-1: 計測用のビルドは ProfCB1 が callback1 を差し替える（ネット交換・デバッグメニューは開けない）*/
    NetTrade_FieldTick();           /* ★ 2026-10-12 A＋Select でネット交換・B＋Select でデバッグメニュー（-DHACK_DEBUG_MENU のとき）＝gMain.callback1 を差し替える（src/hack_network_trade.c・patches/0336）。入力の処理（CB1）より前 */
#endif
    PROF_END(7);

    if (fading)
        SetVBlankCallback(NULL);
    PROF_BEGIN();
#ifdef HACK_PROFILE
    ProfOverworldBasic();
#else
    OverworldBasic_();
#endif
    PROF_END(8);

    extra = OverworldSpeedup_ExtraIterations();
#ifdef HACK_PROFILE
    u32 t9 = ProfNow();
#endif
    for (i = 0; i < extra; ++i)
    {
        if (gMain.callback2 != OverworldCB2_)
            break;
        AnimateSprites();
        if (gMain.callback2 != OverworldCB2_)
            break;
        CameraUpdate_();
        UpdateCameraPanning_();
    }
#ifdef HACK_PROFILE
    ProfAdd(9, ProfNow() - t9);
    ProfAdd(10, ProfNow() - frameStart);
    if (PROF_BASE[300 + 10] > PROF_BASE[329])   /* いちばん重いコマの内訳を写す */
    {
        PROF_BASE[329] = PROF_BASE[300 + 10];
        Memcpy(&PROF_BASE[330], &PROF_BASE[300], PROF_SECTIONS * 4);
    }
    Memset(&PROF_BASE[300], 0, PROF_SECTIONS * 4);   /* 次のコマの値を 0 から（次の VBlank・CB1 の分はこのあとに入る） */
    if ((u32) gMain.callback1 == ProfCB1_Overworld)
        gMain.callback1 = ProfCB1;     /* 次のコマから CB1 の中も区間に分ける（メニューなどへ行けば素の処理がまた差し替える） */
    if ((u32) gMain.vblankCallback == 0x080562D5)
        gMain.vblankCallback = ProfVBlankField;
#endif

    if (fading)
        SetFieldVBlankCallback_();
}
