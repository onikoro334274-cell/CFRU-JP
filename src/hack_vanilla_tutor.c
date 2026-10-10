#include "defines.h"
#include "../include/item.h"
#include "../include/pokemon.h"
#include "../include/constants/items.h"
#include "../include/constants/moves.h"
#include "../include/new/item.h"

/*
 * 素の FRLG の技教えの番号の読み替え（2026-10-06）。
 * ユーザー「究極技のわざおしえが参照先ずれているみたい・他にもないか調査修正」。
 *
 * 素の技教えのスクリプトは 0x8005 に素の番号（0 メガトンパンチ … 14 みがわり・15 ハードプラント・16 ブラストバーン・
 * 17 ハイドロカノン）を入れて special ChooseMonForMoveTutor（と 2 のしまの CapeBrinkGetMoveToTeachLeadPokemon）を呼ぶ。
 * CFRU（EXPANDED_MOVE_TUTORS）は素の入口 08121398（技）と 08121310（覚えられるか）を DPE の 152 個の表に置き換えたので、
 * 素の番号が DPE の表の別の技（0 ほのおのパンチ … 15 スナッチ・16 うらみ・17 さきおくり）を指していた。
 *
 * 素の入口だけをここへ向ける（hooks・patches/0185）。素の入口を通るのは素のスクリプトの技教えだけで、CFRU の C
 * （技思い出し・相手の技の入れ替え）は GetExpandedTutorMove / CanMonLearnTMTutor を直に呼ぶので DPE の番号のまま。
 *  - 技は素の技に戻す。
 *  - 覚えられるかは、その技が DPE の表にあれば DPE の表（全世代）、無ければ素の表を全国図鑑の番号で引く（第 1〜3 世代・
 *    src/Tables/hack_vanilla_tutor_compat.h・tools/tutor/build_vanilla_tutor.py）。0x8005 は書き換えない
 *    （2 のしまは あとで 0x8005 の 15〜17 で「教えた印」を立てる）。
 */

#include "Tables/hack_vanilla_tutor_compat.h"
#include "Tables/hack_emerald_tutor_compat.h"

/* ★ 2026-10-07 ユーザー「エメラルドの教え技が原作と違う CFRU の影響」:
   ホウエンの技教え（エメラルドのスクリプト）はエメラルドの番号 0〜29 を渡していて、上の読み替えでカントーの番号
   （15〜17 は究極技）や DPE の表の別の技になっていた。ホウエンのスクリプトはエメラルドの番号 + HOENN_TUTOR_BASE を渡す
   （tools/rom/em_script.py の HOENN_TUTOR_BASE と同じ値）。技はエメラルドの技・覚えられるかは DPE の表にあれば DPE の表、
   無ければエメラルドの表（第 1〜3 世代・tools/tutor/build_emerald_tutor.py） */
#define HOENN_TUTOR_BASE 0x40
#define IS_HOENN_TUTOR(t) ((t) >= HOENN_TUTOR_BASE && (t) < HOENN_TUTOR_BASE + NUM_EMERALD_TUTORS)

#define NUM_VANILLA_TUTORS 18

static const u16 sVanillaTutorMoves[NUM_VANILLA_TUTORS] =
{
	MOVE_MEGAPUNCH, MOVE_SWORDSDANCE, MOVE_MEGAKICK, MOVE_BODYSLAM, MOVE_DOUBLEEDGE,
	MOVE_COUNTER, MOVE_SEISMICTOSS, MOVE_MIMIC, MOVE_METRONOME, MOVE_SOFTBOILED,
	MOVE_DREAMEATER, MOVE_THUNDERWAVE, MOVE_EXPLOSION, MOVE_ROCKSLIDE, MOVE_SUBSTITUTE,
	MOVE_FRENZYPLANT, MOVE_BLASTBURN, MOVE_HYDROCANNON,
};

static u8 FindExpandedTutorId(u16 move)
{
	u32 i;

	for (i = 0; i < NUM_MOVE_TUTORS; ++i)
	{
		if (gTutorMoves[i] == move)
			return i;
	}
	return 0xFF;
}

//素の GetTutorMove（08121398）の代わり
u16 HackVanillaTutor_GetMove(u8 tutor)
{
	if (tutor < NUM_VANILLA_TUTORS)
		return sVanillaTutorMoves[tutor];
	if (IS_HOENN_TUTOR(tutor))
		return sHackEmeraldTutorMoves[tutor - HOENN_TUTOR_BASE];
	return GetExpandedTutorMove(tutor);
}

//素の CanMonLearnTMTutor（08121310）の代わり
u8 HackVanillaTutor_CanMonLearnTMTutor(struct Pokemon* mon, u16 item, u8 tutor)
{
	u16 move, species, nat;
	u8 expanded;
	bool8 canLearn;

	if (GetPocketByItemId(item) == POCKET_TM_HM || (tutor >= NUM_VANILLA_TUTORS && !IS_HOENN_TUTOR(tutor)))
		return CanMonLearnTMTutor(mon, item, tutor);

	if (GetMonData(mon, MON_DATA_IS_EGG, NULL))
		return CANNOT_LEARN_MOVE_IS_EGG;

	move = HackVanillaTutor_GetMove(tutor);
	expanded = FindExpandedTutorId(move);
	if (expanded != 0xFF)
		canLearn = CanMonLearnTutorMove(mon, expanded);
	else
	{
		species = GetMonData(mon, MON_DATA_SPECIES, NULL);
		nat = SpeciesToNationalPokedexNum(species);
		if (IS_HOENN_TUTOR(tutor))
			canLearn = nat >= 1 && nat < ARRAY_COUNT(sHackEmeraldTutorCompat)
			        && (sHackEmeraldTutorCompat[nat] & (1u << (tutor - HOENN_TUTOR_BASE))) != 0;
		else
			canLearn = nat >= 1 && nat < ARRAY_COUNT(sHackVanillaTutorCompat)
			        && (sHackVanillaTutorCompat[nat] & (1 << tutor)) != 0;
	}

	if (!canLearn)
		return CANNOT_LEARN_MOVE;
	if (MonKnowsMove(mon, move))
		return ALREADY_KNOWS_MOVE;
	return CAN_LEARN_MOVE;
}
