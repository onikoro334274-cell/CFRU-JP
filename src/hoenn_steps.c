#include "defines.h"
#include "map_region.h"
#include "../include/fieldmap.h"
#include "../include/field_camera.h"
#include "../include/field_player_avatar.h"
#include "../include/sound.h"
#include "../include/constants/songs.h"
#include "../include/task.h"
#include "../include/field_effect.h"
#include "../include/constants/field_effects.h"
#include "../include/script.h"
#include "../include/new/roamer.h"
#include "Tables/hoenn_script_flags.h"
#include "region_fly.h"

/* `assembly/hoenn/fly.s`（tools/rom/build_hoenn_fly.py が生成）*/
extern const u8 gHoennMapsecKind[];
extern const u16 gHoennVisitFlagBase[];

void BwFollowerHideForFieldEffect(void);   /* src/bw_follower.c（#16） */
void HoennSootopolisIceStep(s16 x, s16 y, u8 behavior);  /* src/hoenn_gym_puzzles.c（#2 ジム）*/
void HoennPcMonitorTick(void);            /* src/hoenn_pc_monitor.c（#2）*/
void HoennGatesTick(void);                /* src/hoenn_gates.c（#2 ヒワマキ）*/
void HoennBikeTick(void);                 /* src/hoenn_bike.c（自転車のモード）*/
void HoennAbnormalWeatherTick(bool8 moved); /* src/hoenn_abnormal_weather.c（#2 うみ／りくのどうくつ）*/
void HoennWeatherCoordTick(bool8 moved, s16 px, s16 py); /* src/hoenn_weather.c（天気を切り替えるマス・灰）*/
void HoennFarawayMewTick(bool8 moved);   /* src/hoenn_islands.c（さいはてのことうのミュウのかくれんぼ）*/
void HoennSkyPillarCrackedFloorTick(s16 *data, s16 x, s16 y, bool8 moved); /* src/hoenn_sky_pillar.c（そらのはしらのひびの床）*/

/*
hoenn_steps.c
    踏むとメタタイルが変わる仕掛け（やりたいこと #2）。

    ★ 詳細は docs/research/2026-09-18_step-effects.md

    エメラルドは「1 歩ごとのコールバック」（関数ポインタ表 0x084E8990）で
    足元のメタタイルを差し替える。素 FRLG にはこの 3 つが無い:

      表の欄 0  0809DC8C  火山灰の草（フエン周り・挙動 0x24）
      表の欄 1  0809D7C8  ヒワマキシティの吊り橋（挙動 0x78）
      表の欄 2  0809D58C  キナギタウンの丸太（挙動 0x74〜0x77）

    差し替え表は tools/rom/build_hoenn_steps.py が
    assembly/hoenn/steps.s に生成する（メタタイル番号は詰め直し済み）。

    ## ★★ 状態を持たない

    CFRU の挿入コードには書き込める RAM が無く、`static` な変数は ROM 上に
    置かれて書き込みが効かない（CLAUDE.md 6.13）。そこで

      - 「今どの段か」は**メタタイルの番号そのもの**が覚えている
      - 「前にどこにいたか」は覚えず、**足元のまわり 5x5 を毎回見て**
        踏んでいないものを戻す

    ## ★★ 呼ばれる場所（2026-09-18 に変えた）

    最初は `DoGroundEffects_OnBeginStep`（`080680FC`）に置いていたが、
    **あれは動いている間しか呼ばれない**。そのため丸太が
    途中の段で止まり、浮き上がらなくなった（実機）。

    今は素 FRLG の「1 歩ごとのコールバック」を回すタスク
    （`0806E004`）に置いてあるので、**毎フレーム**呼ばれる。

    ## ★★ 段の進め方（2026-09-18・実機報告で 3 度直した）

    エメラルドの `PacifidlogBridgePerStepCallback`（pret/pokeemerald）は

      1. 歩いた瞬間に「今のマス」も「前のマス」も**途中の段**にする
      2. **`tDelay = 8` フレーム**待つ
      3. 今のマスを沈みきり、前のマスを通常に戻す

    こちらもそれに合わせてある:

      - **1 段目（通常 -> 途中 / 沈みきり -> 途中）はすぐ**
      - そこから **きっかり 8 フレーム**待って 2 段目へ

    ★★ 待ち時間は**タスクの空き欄**（`gTasks[taskId].data[15]`）で数える。
      `gTasks` は RAM なので書ける（ROM に置かれる `static` は使えない・6.13）。
      素 FRLG の 1 歩ごとのコールバックは `data[0]`〜`data[5]` までしか
      使っていないことを確認済み（`0806E1D4` / `0806E434` を走査）。

    ★ 最初は「8 フレームに 1 回だけ次へ進む」という数え方にしていたが、
      それだと**途中の段が 1〜8 フレーム（平均 4）しか続かず**、
      **白い水しぶきがほとんど見えなかった**（2026-09-18 実機）。
      しぶきは「途中の段」のメタタイル（594/595/601/609）の絵なので、
      ここが短いと出ていないように見える。

    ★ 参考: `gMain.vblankCounter1`（gMain + 0x20）は**素 FRLG ではポインタで
      普段 NULL**（VBlank `08000744`〜で確認。エメラルドでは素のカウンタ）。
      これを数と思って読んだせいで、丸太が 2 段階目で止まったことがある。

    ## ★ 2026-09-18 に実機で出た不具合（点滅）

    「押したあとの番号」を自分のマスだと認識していなかったため、
    毎フレーム「戻す→押す」を繰り返して激しく点滅した。
    **今の番号が m0 / m1 / m2 のどれかなら自分のマス**として扱うこと。
*/

#define STEP_ANY 0xFFFF     /* この行は今の番号を見ない（火山灰の 2 本目） */
#define STEP_WINDOW 2       /* 足元のまわり 5x5 を戻す */
#define STEP_MAX_PRESS 4    /* 1 回に踏むマスの数（丸太が 2） */
#define STEP_DELAY 8        /* 2 段目まで待つフレーム（エメラルドの tDelay）*/

struct HoennStepRule
{
    const void *tileset;
    u8 behavior;
    u8 restore;
    s8 dx;
    s8 dy;
    u16 m0;                 /* 通常（STEP_ANY なら今の番号を見ない） */
    u16 m1;                 /* 途中（無いものは m2 と同じ） */
    u16 m2;                 /* 沈みきり */
    u16 sound;              /* 踏んだ瞬間に鳴らす音（0 なら鳴らさない） */
};

extern const struct HoennStepRule gHoennStepRules[];

static void HoennStepSet(s16 x, s16 y, u16 to)
{
    if (MapGridGetMetatileIdAt(x, y) == to)
        return;
    MapGridSetMetatileIdAt(x, y, to);
    CurrentMapDrawMetatileAt(x, y);
}

/* このマスはこの行のものか（どの段にいても真） */
static bool8 HoennStepOwns(const struct HoennStepRule *r, u16 cur)
{
    if (r->m0 == STEP_ANY)
        return TRUE;
    return (cur == r->m0 || cur == r->m1 || cur == r->m2);
}

/* ★★★ いま立っている街の「訪問済み」フラグを立てる（そらをとぶの解放）。

   素 FRLG の `GetMapsecType`（`080C4A3E` 付近）は**区分ごとに別のフラグ**を
   見て「飛べる街(2)」か「まだの街(3)」かを返す。移植したホウエンには
   その仕組みが無く、2026-09-22 までは
   **`0x5C` 1 つで 17 都市が一気に解放**されていた（ユーザー判断で変更）。

   ・`gHoennMapsecKind[区分]` は **2 + 街の番号**（`build_hoenn_fly.py` が生成）
   ・フラグは `gHoennVisitFlagBase`(0x13EF) + 街の番号
   ・判定は `assembly/hoenn/townmap_hook.s` の `HoennMapsecTypeHook`

   ★ 毎フレーム呼ぶが `FlagSet` は何度立てても同じなので安い。
     マップヘッダの区分（**+0x14**）を読むだけで、地図の中の位置は見ない。 */
static void HoennFlyVisitTick(void)
{
	u8 sec = gMapHeader.regionMapSectionId;   /* マップヘッダ +0x14 */
	u8 kind;

	/* ★ シンオウのマップの番号はシンオウだけのもので、ホウエンの番号とぶつかる（2026-09-29・src/map_region.h）。
	   ホウエンの街の「訪問済み」を立てないよう、シンオウでは見ない */
	if (GetCurrentMapRegion() == MAP_REGION_SINNOH || GetCurrentMapRegion() == MAP_REGION_KITAKAMI)
		return;     //★ 2026-10-02 キタカミもキタカミだけの番号
	kind = gHoennMapsecKind[sec];

	if (kind < 2)
		return;             /* 素の区分(0) か 道(1) */
	FlagSet((u16) (gHoennVisitFlagBase[0] + (kind - 2)));
}

/*
    ずかんナビを使えるようにする（ユーザー要望 2026-09-22）。

    ★★ **仕組みは CFRU が全部持っていて、足りないのは旗 1 つだけだった。**
      `FLAG_SYS_DEXNAV`(0x91E) を立てるものが `src/debug_menu.c`（CFRU の
      デバッグ用 `special`）にしか無く、素 FRLG のスクリプトにも
      `setflag 0x91E` は **0 箇所**。だから普通に遊んでいる限り
      スタートメニューに「ずかんナビ」が出てこなかった。

    ★ 旗を見ている所は 3 つ。どれもこれ 1 つで動き出す:

      | 場所 | 何が動くか |
      |---|---|
      | `src/start_menu.c` / `src/bw_start_menu.c` | メニューの「ずかんナビ」|
      | `src/read_keys.c` | R ボタンでその場を探す |
      | `src/overworld.c` | 揺れる草（ DEXNAV_DETECTOR_MODE のときだけ）|

    ★ **R ボタンの設定は触らなくてよい** — ずかんナビの画面で種を登録すると
      `src/dexnav.c` が `VAR_R_BUTTON_MODE` を自分で切り替える。

    ★ 条件は**ポケモンずかんをもらってから**（`FLAG_SYS_POKEDEX_GET`）。
      素のスタートメニューもその 2 つを両方見ているので、そこに揃える。
      旗を無条件に立てると、ずかんをもらう前に R ボタンの探索だけ
      使えてしまう。

    ★ 毎フレーム呼ぶが、**立ったあとは `FlagGet` 1 回で帰る**ので安い。
      （スクリプト側に 1 行足す手もあるが、ずかんを渡す素のスクリプトは
        ROM の中にあって**バイトを挿し込めない**ので、こちらが素直。）

    ★ ずかんナビは**その場の野生データを読む**ので、野生を移していない
      ホウエンでは「ここにはいない」と出る。カントーでは普通に使える。
*/
/*
    VS 画面（顔見せ）の主人公を BW の絵にする（#20・2026-09-23）。

    CFRU は `FLAG_LOAD_MUGSHOT_SPRITE_FROM_TABLE`（0x924）が立っているときだけ
    `sPreBattleMugshotSprites`（src/Tables/mugshot_tables.c）の絵を使う。
    その表の [135]/[136] に BW の主人公を入れたので、フラグを常に立てておく。
    ★ 表に絵の無い相手は今までどおり前向きの絵を 2 倍にして出す（CFRU の作り）。
    詳細は docs/research/2026-09-23_bw-vs-mugshot.md
*/
static void HoennMugshotFlagTick(void)
{
#ifdef FLAG_LOAD_MUGSHOT_SPRITE_FROM_TABLE
	if (!FlagGet(FLAG_LOAD_MUGSHOT_SPRITE_FROM_TABLE))
		FlagSet(FLAG_LOAD_MUGSHOT_SPRITE_FROM_TABLE);
#endif
}

static void HoennDexNavUnlockTick(void)
{
	if (FlagGet(FLAG_SYS_DEXNAV))
		return;
	if (FlagGet(FLAG_SYS_POKEDEX_GET))
		FlagSet(FLAG_SYS_DEXNAV);
}

/*
    徘徊ポケモン 3 匹の「補充」（ユーザー要望 2026-09-23）。

    `patches/0050` で `sp129_InitRoamer()` は 3 匹とも作るようになったが、
    これが走るのは素 FRLG の `special InitRoamer` ＝
    **ななしま 1 のポケセンでセリオがホウエンと通信をつなぐイベントの最後**
    （`FLAG_SYS_CAN_LINK_WITH_RS` を立てた直後）で、**1 回きり**。

    ★ そこを**既に過ぎているセーブ**では 1 匹しか登録されていないので、
      ここで一度だけ呼び直して残り 2 匹を足す。
      `CreateInitialRoamerMon()` は `IsSpeciesRoaming()` で
      **同じ種族を二重に登録しない**ので、足りないものだけ増える。

    ★★★ **必ず一度きりにすること。** 倒した／捕まえた個体は枠が空くので、
      毎回呼ぶと**復活してしまう**。そのための印が
      `HOENN_FLAG_ROAMERS_TOPPED_UP`（`0x13EE`・
      `tools/rom/build_hoenn_scripts.py` の `RESERVED_FLAGS` が予約）。

    ★ スクリプトが走っている間は呼ばない（`ScriptContext2_IsEnabled`）。
      `CreateInitialRoamerMon()` は `gEnemyParty[0]` と
      `gSpecialVar_LastResult` を作業用に使うので、
      イベントの最中に横から触らないようにする。

    ★ 新しく始めたセーブでも害は無い — スクリプトが先に 3 匹作っており、
      そのあとここが一度呼ばれても `IsSpeciesRoaming()` が全部弾く。
*/
static void HoennRoamerTopUpTick(void)
{
	if (FlagGet(HOENN_FLAG_ROAMERS_TOPPED_UP))
		return;                     /* もう済んでいる（ほぼ毎回ここで帰る） */
	if (!FlagGet(FLAG_SYS_CAN_LINK_WITH_RS))
		return;                     /* セリオのイベントがまだ */
	if (ScriptContext2_IsEnabled())
		return;                     /* スクリプトの最中は触らない */

	sp129_InitRoamer();
	FlagSet(HOENN_FLAG_ROAMERS_TOPPED_UP);
}

static void HoennSplashTick(void);

/*
    火山灰の草の「灰の雲」（2026-09-19）。

    ★★ 素 FRLG は **AshGrassPerStepCallback（0806E349）を丸ごと持っている**
      （1 歩ごとのコールバック表 0836B340 の欄 1）。雲のスプライトも
      `FLDEFF_ASH` として残っている。だが、こちらからは使えない:

        1. **欄 1 が選ばれるのはマップスクリプトが指定したときだけ。**
           ホウエンはスクリプトを移していないので常に欄 0（何もしない）
        2. 判定の `MetatileBehavior_IsAshGrass`(`0805980C`) が空関数
        3. 差し替え先のメタタイル番号が**カントーの番号で焼き込まれている**
           （`0x20A` なら `0x212`、それ以外は `0x206`）

    → **`StartAshFieldEffect` を自分で呼ぶ。** 差し替え先の番号は
      こちらの表（`gHoennStepRules` の `m1`）が variant ごとに持っている。

    ★ この効果は **d フレーム待ってからメタタイルを自分で差し替える**
      （`UpdateAshFieldEffect_Step1`）。だから灰だけは
      `HoennStepSet` を呼ばない。**呼ぶと二重になる。**

    ★ 素と同じく「**1 歩に 1 回**」にする（毎フレーム呼ぶと雲が積み上がる）。
      主人公のマスが変わった回だけ動かす。マスは `data[12]` / `data[13]`。
*/
#define HoennStartAshFieldEffect     ((void (*)(s16, s16, u16, s16))(0x080DD224 + 1))
#define STEP_ASH_BEHAVIOR 0x24  /* 火山灰の草。ここだけ雲を出す */
#define STEP_ASH_DELAY 4        /* 素と同じ（AshGrassPerStepCallback）*/

#define STEP_TIMER 15       /* 使うタスクの欄（素は data[0]〜data[5] まで） */
#define STEP_LASTX 12       /* 前に見た主人公のマス（1 歩に 1 回にするため）*/
#define STEP_LASTY 13

/* この二次タイルセットに沈むマスの決まりがあるか（gHoennStepRules は tileset が NULL の行で終わる） */
static bool8 HoennStepTilesetHasRules(const void *sec)
{
    const struct HoennStepRule *r;

    for (r = gHoennStepRules; r->tileset != NULL; ++r)
    {
        if (r->tileset == sec)
            return TRUE;
    }
    return FALSE;
}

void HoennStepTick(u8 taskId)
{
    struct Task *task = &gTasks[taskId];
    const struct MapLayout *layout = gMapHeader.mapLayout;
    const void *sec;
    const struct HoennStepRule *r;
    s16 px, py, x, y;
    s16 tx[STEP_MAX_PRESS];
    s16 ty[STEP_MAX_PRESS];
    u8 n = 0;
    u8 i;
    u8 beh;
    u8 hold;                /* 途中の段を保っている最中か */
    u16 cur;
    u16 se = 0;             /* この回に鳴らす音（1 回だけ） */
    u8 moved;               /* この回にマスが変わったか（灰の雲用） */

    HoennFlyVisitTick();    /* いる街の「訪問済み」を立てる */
    RegionFlyVisitTick();   /* ジョウト・シンオウの町の「訪問済み」（src/region_fly.c・2026-09-30）*/
    RegionHealTick();       /* ホウエン・ジョウト・シンオウのポケセンにいる間は「最後の回復地点」をその町に */
    HoennDexNavUnlockTick();  /* ずかんをもらったら「ずかんナビ」を使えるように */
    HoennMugshotFlagTick();   /* VS の主人公を BW の絵に（表を使う旗）*/
    HoennRoamerTopUpTick();   /* 3 犬が揃っていなければ一度だけ補う */
    HoennSplashTick();      /* 足元の水しぶき（地形を見ないので先に） */
    HoennPcMonitorTick();   /* ポケセンの回復モニターをホウエンの絵に */
    HoennGatesTick();       /* ヒワマキジムの回転門（出すだけ） */
    HoennBikeTick();        /* 自転車のモード（旗・絵・ダートの入力の履歴）*/

    /* ★ 連れ歩き（#16）。そらをとぶ等の間は隠す。素 FRLG も CFRU も面倒を見ないので、
       毎フレーム動くここに相乗りしている（src/bw_follower.c） */
    BwFollowerHideForFieldEffect();

    if (layout == NULL)
        return;
    sec = layout->secondaryTileset;
    if (sec == NULL)
        return;

    /* ★ 先に読む。この回に途中の段へ変えても、その回は止めない */
    hold = (u8)(task->data[STEP_TIMER] > 0);
    if (task->data[STEP_TIMER] > 0)
        --task->data[STEP_TIMER];

    PlayerGetDestCoords(&px, &py);
    beh = MapGridGetMetatileBehaviorAt(px, py);

    /* ★ 1 歩に 1 回だけ動かしたいもの（灰の雲）のための目印。
         タスクを作り直した直後は data[] が 0 なので、
         そのときは「動いた」と数えない（(0,0) は必ず地図の外）。 */
    moved = (u8)((task->data[STEP_LASTX] != 0 || task->data[STEP_LASTY] != 0)
                 && (task->data[STEP_LASTX] != px
                     || task->data[STEP_LASTY] != py));
    task->data[STEP_LASTX] = px;
    task->data[STEP_LASTY] = py;

    /* ★ うみ／りくのどうくつの異常気象（自動で起こす・1000 歩で終わる）。1 歩の目印を使う */
    HoennAbnormalWeatherTick(moved);

    /* ★ 天気を切り替えるマス（113 ばんどうろ・えんとつやまの山道の灰。素 FRLG の処理は空）*/
    HoennWeatherCoordTick(moved, px, py);

    /* ★ さいはてのことうのミュウ（主人公が 1 歩動き始めたら逃げる・エメラルドの src/faraway_island.c）*/
    HoennFarawayMewTick(moved);

    /* ★ そらのはしら 2F・4F のひびの床（2026-10-04・data[7]〜[11]・[14] を使う）。毎フレーム呼ぶ（3 コマあとに穴にする） */
    HoennSkyPillarCrackedFloorTick(task->data, px, py, moved);

    /* ★ ルネジムの氷（#2 ジム・src/hoenn_gym_puzzles.c）。
       素 FRLG に「1 歩ごとの処理」の枠が無いので、ここに相乗りしている。
       **動いた回だけ**呼ぶ（毎フレームだと 1 歩で割れきってしまう）。 */
    if (moved)
        HoennSootopolisIceStep(px, py, beh);

    /* ★ 2026-10-07 今のタイルセットに沈むマスの決まりが 1 つも無ければ、足元の表とまわり 5x5 を見ない
         （前は毎コマ約 15,000 サイクル・どの地図でも見ていた・デモ版の報告「処理落ち」）。
         決まりが無いタイルセットでは下の 1. 2. はどちらも何もしないので、結果は変わらない。
         ★ ここより上（訪問の旗・異常気象・天気のマス・ミュウ・ひびの床・ルネの氷）はタイルセットに関係なく毎コマ呼ぶので、下へ動かさない */
    if (!HoennStepTilesetHasRules(sec))
        return;

    /* 1. 足元（と、その行が指すマス）を 1 段沈める */
    for (r = gHoennStepRules; r->tileset != NULL; ++r)
    {
        if (r->tileset != sec || r->behavior != beh)
            continue;
        x = px + r->dx;
        y = py + r->dy;
        for (i = 0; i < n; ++i)
        {
            if (tx[i] == x && ty[i] == y)
                break;
        }
        if (i < n)
            continue;                   /* このマスはもう決まっている */
        cur = MapGridGetMetatileIdAt(x, y);
        if (!HoennStepOwns(r, cur))
            continue;                   /* この行のマスではない */
        if (n < STEP_MAX_PRESS)
        {
            tx[n] = x;
            ty[n] = y;
            ++n;
        }
        if (r->behavior == STEP_ASH_BEHAVIOR)
        {
            /* ★ 火山灰だけは雲のスプライトを出す。
                 メタタイルの差し替えは**効果が自分でやる**ので
                 ここでは触らない（触ると二重になる）。 */
            if (moved && cur != r->m1)
                HoennStartAshFieldEffect(x, y, r->m1, STEP_ASH_DELAY);
        }
        else if (cur == r->m1)
        {
            if (!hold)
                HoennStepSet(x, y, r->m2);      /* 途中 -> 沈みきり */
        }
        else if (cur != r->m2)
        {
            /* ★ 通常 -> 途中は「踏んだ瞬間」。ここでだけ音を鳴らす。
                 丸太は 1 歩で 2 マス変わるので、鳴らすのは 1 回だけにする。
                 音は pret/pokeemerald の field_tasks.c と同じ:
                   丸太   SE_PUDDLE（EM の SE_MIZU）
                   吊り橋 SE_BOARDWALK（EM の SE_BRIDGE_WALK / SE_HASHI）*/
            if (se == 0)
                se = r->sound;
            HoennStepSet(x, y, r->m1);          /* 通常 -> 途中（すぐ） */
            task->data[STEP_TIMER] = STEP_DELAY;
        }
    }

    if (se != 0)
        PlaySE(se);

    /* 2. まわりで「沈んだまま」のものを 1 段戻す */
    for (y = py - STEP_WINDOW; y <= py + STEP_WINDOW; ++y)
    {
        for (x = px - STEP_WINDOW; x <= px + STEP_WINDOW; ++x)
        {
            for (i = 0; i < n; ++i)
            {
                if (tx[i] == x && ty[i] == y)
                    break;
            }
            if (i < n)
                continue;               /* 今踏んでいるマスは戻さない */
            cur = MapGridGetMetatileIdAt(x, y);
            for (r = gHoennStepRules; r->tileset != NULL; ++r)
            {
                if (r->tileset != sec || !r->restore || r->m0 == STEP_ANY)
                    continue;
                if (cur == r->m2 && cur != r->m1)
                {
                    HoennStepSet(x, y, r->m1);  /* 沈みきり -> 途中（すぐ） */
                    task->data[STEP_TIMER] = STEP_DELAY;
                    break;
                }
                if (cur == r->m1)
                {
                    if (!hold)
                        HoennStepSet(x, y, r->m0);  /* 途中 -> 通常 */
                    break;
                }
            }
        }
    }
}

/*
    走るのを止める（2026-09-18・実機報告「走れてしまう」）。

    素 FRLG の MetatileBehavior_IsRunningDisallowed（0x08059AF0）は
    `return behavior == 0x0A;` を返すだけなので、functionrewrites で
    丸ごと置き換える。カントーは 0x74〜0x78 を 1 マスも使っていない。

    ★ エメラルドの `MetatileBehavior_IsRunningDisallowed`（pret/pokeemerald）は
      MB_NO_RUNNING / MB_LONG_GRASS / MB_HOT_SPRINGS / 丸太 4 種 を止める。

      - 丸太（0x74〜0x77）と MB_LONG_GRASS（0x03）は**そのとおりにした**
        （どちらもカントーは 0 マス）
      - MB_HOT_SPRINGS（0x28）も**そのとおりにした**（2026-09-18・ユーザー判断）。
        ★ **素 FRLG も 1 マップ 37 マスだけ使っている**（群2 番号57・27x39・
          区分 0xC3）。そこでも走れなくなる。影響はそのマップだけ
      - ヒワマキの吊り橋（0x78）は**エメラルドでは走れる**ので、
        こちらも走れるようにした（2026-09-18・ユーザー判断で原作に合わせた）
*/
bool8 HoennIsRunningDisallowed(u8 behavior)
{
    if (behavior == 0x0A)               /* MB_NO_RUNNING（素 FRLG の判定） */
        return TRUE;
    if (behavior == 0x03)               /* MB_LONG_GRASS。カントーは 0 マス */
        return TRUE;
    if (behavior == 0x28)               /* MB_HOT_SPRINGS */
        return TRUE;
    if (behavior >= 0x74 && behavior <= 0x77)   /* キナギの丸太 */
        return TRUE;
    return FALSE;
}

/*
    足元の水しぶき（2026-09-18・実機写真で確定）。

    ★★ しぶきは**メタタイルではなくスプライト**（`FLDEFF_FEET_IN_FLOWING_WATER`）。
      `GetGroundEffectFlags_ShallowFlowingWater` が
      「今のマスと前のマスが**どちらも丸太**」のときに出す。

    素 FRLG にも処理も絵も残っているが、判定の

        bool8 MetatileBehavior_IsPacifidlogLog(u8 b) { return FALSE; }
        （参考/pokefirered/src/metatile_behavior.c:607 / ROM 0x08059808）

    が常に FALSE なので一度も出ない。

    ★★★ **この空関数を書き換えてはいけない（2026-09-18 実機でクラッシュ）。**
      `functionrewrites` は入口に **20 バイト**書き込むが、この関数は
      **4 バイト**しかなく、直後に同じ 4 バイトの空関数が 4 つ並んでいる。
      5 つまとめて潰して "Jumped to invalid address" になった。
      `hooks`（8 バイト）でも隣を 1 つ潰す。**5 つとも呼ばれている。**

    → そこで **ROM は一切書き換えず**、毎フレームのこのコードから
      自分で効果を出す。やっていることは素の関数と同じ:

        どちらも丸太なら inShallowFlowingWater を立てて効果を出す
        そうでなければ倒す（効果は自分で消える）

    ★ 重ねて出さないよう `FieldEffectActiveListContains` で見張る。
      素の処理は 1 歩ごとに inShallowFlowingWater を倒してしまうので、
      **毎フレーム立て直す**（倒れるとスプライトが自分で消える）。
    ★ 効果そのものも座標が変わるたび `SE_PUDDLE` を鳴らす
      （`UpdateFeetInFlowingWaterFieldEffect`）。**表の音と重なるが、
      エメラルドも同じ**（`PacifidlogBridgePerStepCallback` が鳴らし、
      同じフレームにスプライトも鳴らす）ので表はそのままにしてある。
      おかげで「同じ丸太の反対端へ移る」ときの音もスプライト側が出す。
*/
#define HoennStartFieldEffectForObjectEvent     ((u8 (*)(u8, struct EventObject *))(0x08068B88 + 1))

static bool8 HoennIsPacifidlogLog(u8 behavior)
{
    return (behavior >= 0x74 && behavior <= 0x77);
}

static void HoennSplashTick(void)
{
    struct EventObject *p = &gEventObjects[gPlayerAvatar->eventObjectId];

    if (HoennIsPacifidlogLog(p->currentMetatileBehavior)
     && HoennIsPacifidlogLog(p->previousMetatileBehavior))
    {
        p->inShallowFlowingWater = TRUE;
        if (!FieldEffectActiveListContains(FLDEFF_FEET_IN_FLOWING_WATER))
            HoennStartFieldEffectForObjectEvent(
                FLDEFF_FEET_IN_FLOWING_WATER, p);
    }
    else if (p->inShallowFlowingWater)
    {
        p->inShallowFlowingWater = FALSE;
    }
}
