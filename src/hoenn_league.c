#include "defines.h"
#include "../include/event_data.h"
#include "../include/load_save.h"
#include "../include/overworld.h"

#include "Tables/hoenn_script_flags.h"
#include "Tables/sinnoh_flags.h"
#include "map_region.h"

/*
hoenn_league.c
    ホウエンリーグの強化版（2026-09-25）

    ユーザー「1 回目のホウエンリーグに勝つとホウエンリーグが強化されるように設定」
    （判断「6 匹にしてレベルも上げる」）。

    ・強化版の手持ちは tools/rom/build_hoenn_trainers.py の LEAGUE_REMATCH が作る
      （元の四天王・チャンピオンを写して 6 匹目を足し、レベル +15）。番号の組は
      build_hoenn_scripts.py が Tables/hoenn_script_flags.h に書き出す
      （HOENN_LEAGUE_REMATCH_TABLE = {元の番号, 強化版の番号}）。
    ・制覇の回数 HOENN_VAR_LEAGUE_WINS は殿堂入り（special GameClear の差し替え）で 1 足される。
    ・★ 差し替えるのは**手持ちを作るときの番号だけ**（src/build_pokemon.c の CreateNPCTrainerParty の入口）。
      戦ったかどうかの印・会話・勝ったあとの処理は元の番号（gTrainerBattleOpponent_A）のまま。
*/

u16 HoennLeagueRematchTrainer(u16 trainerId)
{
	#ifdef HOENN_LEAGUE_REMATCH_COUNT
	static const u16 sPairs[HOENN_LEAGUE_REMATCH_COUNT][2] = HOENN_LEAGUE_REMATCH_TABLE;

	if (VarGet(HOENN_VAR_LEAGUE_WINS) >= 1)
	{
		for (u32 i = 0; i < HOENN_LEAGUE_REMATCH_COUNT; ++i)
		{
			if (sPairs[i][0] == trainerId)
				return sPairs[i][1];
		}
	}
	#endif

	return trainerId;
}

/*
    ホウエンリーグの殿堂入りのあと（2026-09-28 ユーザー「希望はエンディング流れずマサラタウンの家の前」）

    素 FRLG は殿堂入りの画面の終わりで SetWarpsToRollCredits（080F3894・hall_of_fame.c）を呼び、セキエイ高原の入口へ
    ワープしてスタッフロールを流す。そこを丸ごと置き換え（hooks・r3）:
    ・ホウエンの殿堂入り（印 HOENN_FLAG_HOF_IN_HOENN。GameClear の差し替えで立つ）→ スタッフロールを流さず
      マサラタウンの自宅の前（扉 (6,7) の 1 マス下）へ。印を消す
    ・カントーの殿堂入り → 2026-10-10 からホウエンと同じ（下の★）
    ★ 素の EnterHallOfFame は「続きから始めたらマサラの家」の印（continue warp）を立て、スタッフロールのあとの
      リセットで使って消える。スタッフロールを飛ばすと残ったままになり、あとで別の場所でレポートを書いても
      続きからマサラへ飛ばされる → ここで消す（殿堂入りの画面のレポートには立ったまま残るので、
      その直後に電源を切ったときだけマサラの家から始まる。素と同じ）
*/
#define DoWarp ((void (*)(void)) (0x0807DB68 | 1))
#define MAP_GROUP_TOWNS 3
#define MAP_NUM_PALLET_TOWN 0

/*
    ★ 2026-10-10 ユーザー「カントーのエンディングを消す　ホウエンと一緒にして」: カントーの殿堂入りもスタッフロールを流さず
      マサラタウンの家の前へ。素のスタッフロール（セキエイ高原の入口の場面 → DoCredits）は終わりに SoftReset するだけで、
      中で変えるのは地名の窓の印だけ（pokefirered src/credits.c 746/793・data/maps/IndigoPlateau_Exterior/scripts.inc）。
      飛ばしても失うものは無い。続きからの印はホウエンと同じ理由で消す。
      素の流れ（VAR_MAP_SCENE_INDIGO_PLATEAU_EXTERIOR = 1 でセキエイ高原の入口へ）に戻すときは git の履歴を見る。
*/
/*
    ★ 2026-10-10 シンオウのポケモンリーグの殿堂入り（印 SINNOH_FLAG_LEAGUE_HOF・tools/sinnoh/build_sinnoh_gyms.py の league）→
      リーグのポケセン（HnS のセキエイのポケセン）の看護師の前へ。行き先は src/Tables/sinnoh_flags.h（生成物）
    ★ 2026-10-12 ユーザー（5e 経由）「殿堂入りのあとの戻り先はマサラタウンの家の前（カントー・ホウエンと同じ）」:
      シンオウも印を消すだけで、行き先はマサラ（つづきからの戻る場所もマサラ）。sinnoh_flags.h の SINNOH_LEAGUE_PC_* はもう使わない
*/
void HoennLeague_SetWarpsAfterHallOfFame(void)
{
	u8 group = MAP_GROUP_TOWNS, num = MAP_NUM_PALLET_TOWN;
	s16 x = 6, y = 8;

	if (FlagGet(SINNOH_FLAG_LEAGUE_HOF))
	{
		FlagClear(SINNOH_FLAG_LEAGUE_HOF);
	}
	else if (FlagGet(HOENN_FLAG_HOF_IN_HOENN))
	{
		FlagClear(HOENN_FLAG_HOF_IN_HOENN);
	}
	else
	{
		//★ 2026-10-02 カントーの強化リーグ（ななしまのあと＝FLAG_SYS_CAN_LINK_WITH_RS のときのチャンピオン戦は REMATCH の手持ち・
		//  pokefirered data/maps/PokemonLeague_ChampionsRoom/scripts.inc）の殿堂入り → カントーの野生を他の世代に（src/wild_shuffle.c）
		if (FlagGet(FLAG_SYS_CAN_LINK_WITH_RS))
			FlagSet(KANTO_FLAG_WILD_SHUFFLE);
	}

	ClearContinueGameWarpStatus2();
	SetWarpDestination(group, num, -1, x, y);
	DoWarp();
	ResetInitialPlayerAvatarState();
}

/*
    ホウエンリーグを制覇したとき（殿堂入り・special GameClear の差し替えから callnative・2026-09-29）

    ユーザー「ホウエンリーグ制覇時にフラグをたててラティ兄妹2匹を徘徊させる　ホウエンリーグ2回目の強化リーグを
    クリアするとフラグをたててさいはてのことうとみなみのことうに行ける」。
    ・制覇 1 回目: 印 HOENN_FLAG_LATI_ROAMING を立て、ラティアスとラティオスの 2 匹をホウエンで徘徊させる
      （エメラルドは自宅のテレビで色を選んで 1 匹。こちらは 2 匹とも。レベル 40・陸でも水でも出る＝エメラルドと同じ）。
      ★ 一度きり（印を見る）。倒した／捕まえた個体は戻らない（CFRU の徘徊と同じ）
    ・制覇 2 回目（強化されたリーグ）: 印 HOENN_FLAG_ISLANDS_OPEN を立てる → ミナモの船乗りがみなみのことう・さいはてのことうへ
    ★ 回数（HOENN_VAR_LEAGUE_WINS）は同じ差し替えの中で先に 1 足してある
*/
extern void CreateHoennRoamer(u16 species, u8 level);   /* src/roamer.c（patches/0093） */
#define HOENN_ROAMER_LEVEL_LATI 40

void HoennNative_LeagueHallOfFame(void)
{
	u16 wins = VarGet(HOENN_VAR_LEAGUE_WINS);

	if (wins >= 1 && !FlagGet(HOENN_FLAG_LATI_ROAMING))
	{
		FlagSet(HOENN_FLAG_LATI_ROAMING);
		CreateHoennRoamer(SPECIES_LATIAS, HOENN_ROAMER_LEVEL_LATI);
		CreateHoennRoamer(SPECIES_LATIOS, HOENN_ROAMER_LEVEL_LATI);
	}
	if (wins >= 2)
		FlagSet(HOENN_FLAG_ISLANDS_OPEN);
}

/*
    殿堂入りの部屋でボールを置く位置（2026-09-29 ユーザー「殿堂入りの部屋　ボールの置く位置がずれている」）

    ・ボールを並べる演出（FldEff_HallOfFameRecord・素 FRLG 080838CC）の 1 個目の位置は、
      素 FRLG が (117, 60)（pokefirered src/field_effect.c:817）、エメラルドが (117, 52)（pokeemerald src/field_effect.c:1074）。
      ホウエンの殿堂入りの部屋はエメラルドの地図なので、素のままだと 8 画素下にずれる。
    ・y を入れる所（080838F4）に hooks（assembly/hoenn/hof_hook.s）。ホウエンの地図なら 52、ほか（セキエイ）は 60
    ★ エメラルドの小さなモニター 4 つ（CreateHofMonitorSprite の 40/72/168/200, 8）は入れていない（素 FRLG は大きなモニター 1 つだけ）
*/
s16 HoennHofFirstBallY(void)
{
	return (GetCurrentMapRegion() == MAP_REGION_HOENN) ? 52 : 60;
}
