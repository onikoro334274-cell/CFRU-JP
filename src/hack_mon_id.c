#include "defines.h"
#include "../include/pokemon.h"
#include "../include/pokemon_storage_system.h"
#include "../include/random.h"
#include "hack_mon_id.h"
#include "hack_box_shelf.h"

/*
 * ポケモン自身の表 ID・裏 ID（#70・2026-10-09）。
 * ユーザー「トレーナーと同じ感じで表IDと裏ID持たせて…裏IDでmoemon決める」「今のポケモンデータを変えていいからやって」
 * 「全ポケモンのデータに使う」「B 案でボックス枠も50になるように拡張」「CFRUを改造して」。
 *
 * - 置き場（手持ち・育て屋など 80 バイトの形）: コンディションの 4 バイト（かっこよさ・うつくしさ・かわいさ・かしこさ）
 *   ＝ +0x3E 表 ID（u16）・+0x40 裏 ID（u16）。FRLG にコンテストは無く、読むのは CFRU のスクリプトの命令だけ。
 * - 置き場（ボックスの 58 バイト）: 縮めた形はコンディションを捨てるので、いつも 0 の空きビット 32 本に分けて入れる
 *   （HackMonId_PackComp・箱の数と 1 匹の大きさは変えない・patches/0259）。空きビットが 0 なのは 2,000 匹で確かめた。
 * - 0 は「まだ無い」。まだ無い子の色違いは今までどおり（親の ID と性格値）。付けるときは、そのときの色違いのまま
 *   （色違いの子には「表 ^ 裏 < 8」の ID）なので、見た目は変わらない。付けたあとは性格値・親の ID を見ない。
 * - 付けるとき: 手持ちに入ったら（フィールドの毎コマ）・ボックスに入れるとき（縮めるとき）。
 * - Moémon（第 2 段・2026-10-10 ユーザー「すすめて1/16」）: 裏 ID で当たり（1/16）とどの絵かを決める（DPE の src/hack_moemon.c が
 *   gHackMonIdBridge＝HackMonId_MoemonKey で聞く）。付けるときは今の見た目のまま（当たり・はずれと絵の選び方の値を性格値の式と
 *   そろえた裏 ID を探す・前からいた子の一覧の子は はずれ）。まだ ID の無い子は今までどおり性格値で。
 * - 色違いの判定: IsMonShiny 08043AB8 はその子の ID で。IsShinyOtIdPersonality 08043AE4（性格値と親の ID しか来ない）は、
 *   手持ち・相手・今の箱から同じ性格値と親の ID の子を探して、その子の ID で決める（見つからなければ今までどおり）。
 */

#define OFF_TERA 0x11
#define OFF_LANG 0x12
#define OFF_SANITY 0x13
#define OFF_MARKINGS 0x1B
#define C_ITEM 0x1E
#define C_EXP 0x20
#define C_BALL 0x26
#define C_METINFO 0x34
#define B_ITEM 0x22
#define B_EXP 0x24
#define B_BALL 0x2A
#define B_METINFO 0x46
#define B_ID 0x3E                    //substruct2 の cool・beauty・cute・smart
#define SHINY_THRESHOLD 8

static u32 Rd16(const u8* p) { return p[0] | (p[1] << 8); }
static u32 Rd32(const u8* p) { return p[0] | (p[1] << 8) | (p[2] << 16) | ((u32) p[3] << 24); }
static void Wr16(u8* p, u32 v) { p[0] = v; p[1] = v >> 8; }
static void Wr32(u8* p, u32 v) { p[0] = v; p[1] = v >> 8; p[2] = v >> 16; p[3] = v >> 24; }

/* 32 ビットの分け方（ビット位置は ID の中・入れ物のビット）:
 *   0〜5   持ち物 bit10〜15（道具 < 1024）      6〜10  経験値 bit27〜31（Lv200 でも 27 ビット）
 *   11〜14 マーク bit4〜7                       15〜19 言葉 bit3〜7
 *   20〜21 出会った情報 bit13〜14（blank）        22〜25 +0x13 の bit4〜7（0x08 は Moémon の通信の印）
 *   26〜28 テラスタイプ bit5〜7（< 32）           29〜31 ボール bit5〜7（< 32）
 * tools/save_editor/savelib.py・web 内の JS と同じ（verify_mon_id.py が見る） */
u32 HackMonId_UnpackComp(const void* comp)
{
	const u8* c = comp;
	u32 v = 0;
	v |= (Rd16(c + C_ITEM) >> 10) & 0x3F;
	v |= ((Rd32(c + C_EXP) >> 27) & 0x1F) << 6;
	v |= ((c[OFF_MARKINGS] >> 4) & 0xF) << 11;
	v |= ((c[OFF_LANG] >> 3) & 0x1F) << 15;
	v |= ((Rd16(c + C_METINFO) >> 13) & 3) << 20;
	v |= ((c[OFF_SANITY] >> 4) & 0xF) << 22;
	v |= (u32) ((c[OFF_TERA] >> 5) & 7) << 26;
	v |= (u32) ((c[C_BALL] >> 5) & 7) << 29;
	return v;
}

void HackMonId_PackComp(void* comp, u32 v)
{
	u8* c = comp;
	Wr16(c + C_ITEM, (Rd16(c + C_ITEM) & 0x03FF) | ((v & 0x3F) << 10));
	Wr32(c + C_EXP, (Rd32(c + C_EXP) & 0x07FFFFFF) | (((v >> 6) & 0x1F) << 27));
	c[OFF_MARKINGS] = (c[OFF_MARKINGS] & 0x0F) | (((v >> 11) & 0xF) << 4);
	c[OFF_LANG] = (c[OFF_LANG] & 0x07) | (((v >> 15) & 0x1F) << 3);
	Wr16(c + C_METINFO, (Rd16(c + C_METINFO) & ~(3 << 13)) | (((v >> 20) & 3) << 13));
	c[OFF_SANITY] = (c[OFF_SANITY] & 0x0F) | (((v >> 22) & 0xF) << 4);
	c[OFF_TERA] = (c[OFF_TERA] & 0x1F) | (((v >> 26) & 7) << 5);
	c[C_BALL] = (c[C_BALL] & 0x1F) | (((v >> 29) & 7) << 5);
}

void HackMonId_ClearSpare(struct BoxPokemon* mon)   //80 バイトの形に戻したあと、入れ物のビットを消す
{
	u8* b = (u8*) mon;
	Wr16(b + B_ITEM, Rd16(b + B_ITEM) & 0x03FF);
	Wr32(b + B_EXP, Rd32(b + B_EXP) & 0x07FFFFFF);
	b[OFF_MARKINGS] &= 0x0F;
	b[OFF_LANG] &= 0x07;
	Wr16(b + B_METINFO, Rd16(b + B_METINFO) & ~(3 << 13));
	b[OFF_SANITY] &= 0x0F;
	b[OFF_TERA] &= 0x1F;
	b[B_BALL] &= 0x1F;
}

u32 HackMonId_Get(const struct BoxPokemon* mon)
{
	return Rd32((const u8*) mon + B_ID);
}

void HackMonId_Set(struct BoxPokemon* mon, u32 id)
{
	Wr32((u8*) mon + B_ID, id);
}

static bool8 OldShiny(u32 otId, u32 pid)
{
	return (((otId >> 16) ^ (otId & 0xFFFF) ^ (pid >> 16) ^ (pid & 0xFFFF)) & 0xFFFF) < SHINY_THRESHOLD;
}

static bool8 IdShiny(u32 id)
{
	return (((id & 0xFFFF) ^ (id >> 16)) & 0xFFFF) < SHINY_THRESHOLD;
}

//Moémon（DPE の src/hack_moemon.c と同じ式）: 性格値のころは性格値・ID があれば裏 ID をかき混ぜる
#define MOEMON_HIT_OF(x) (((((u32) (x)) * 0x9E3779B1u) >> 28) == 0xA)   //1/16
#define MOEMON_PICK_OF(x) ((((u32) (x)) * 0x85EBCA6Bu) >> 16)           //どの絵か（絵の数で割った余り・絵は 1 種族 6 枚まで）
#define MOEMON_PICK_LCM 60                                             //1〜6 の最小公倍数＝どの絵の数でも同じ絵になる
bool8 HackMoemonSwitch_IsOldMon(u32 personality);                     //src/hack_moemon_switch.c（前からいた子の一覧）

//裏 ID: Moémon の当たり・はずれと絵の選び方を、今（性格値の式）とそろえる
static u32 PickSid(u32 pid)
{
	bool8 hit = MOEMON_HIT_OF(pid) && !HackMoemonSwitch_IsOldMon(pid);
	u32 want = MOEMON_PICK_OF(pid) % MOEMON_PICK_LCM;
	u32 start = Random(), k, sid;

	for (k = 0; k < 0x10000; ++k)
	{
		sid = (start + k) & 0xFFFF;
		if (MOEMON_HIT_OF(sid) == hit && (!hit || MOEMON_PICK_OF(sid) % MOEMON_PICK_LCM == want))
			return sid;
	}
	return start & 0xFFFF;
}

static u32 MakeId(bool8 shiny, u32 pid)
{
	u32 tid, sid = PickSid(pid);
	do
	{
		if (shiny)
			tid = sid ^ (Random() % SHINY_THRESHOLD);
		else
			do { tid = Random(); } while (((tid ^ sid) & 0xFFFF) < SHINY_THRESHOLD);
	} while ((tid & 0xFFFF) == 0 && sid == 0);
	return (tid & 0xFFFF) | (sid << 16);
}

//★ 2026-10-10 Moemonかせき（src/hack_moemon_fossil.c）: 今の色違いのまま、裏 ID を Moémon の当たりにする
void HackMonId_MakeMoemon(struct BoxPokemon* mon)
{
	bool8 shiny = HackMonId_IsShinyBox(mon);
	u32 start = Random(), k, sid = 0, tid;

	for (k = 0; k < 0x10000; ++k)
	{
		sid = (start + k) & 0xFFFF;
		if (MOEMON_HIT_OF(sid))
			break;
	}
	do
	{
		if (shiny)
			tid = sid ^ (Random() % SHINY_THRESHOLD);
		else
			do { tid = Random(); } while (((tid ^ sid) & 0xFFFF) < SHINY_THRESHOLD);
	} while ((tid & 0xFFFF) == 0 && sid == 0);
	HackMonId_Set(mon, (tid & 0xFFFF) | (sid << 16));
}

void HackMonId_Ensure(struct BoxPokemon* mon)
{
	if (mon->substruct0.species == SPECIES_NONE || HackMonId_Get(mon) != 0)
		return;
	HackMonId_Set(mon, MakeId(OldShiny(mon->otId, mon->personality), mon->personality));
}

bool8 HackMonId_IsMoemonHit(const struct BoxPokemon* mon)   //萌えもんの当たりか（ON/OFF は見ない）
{
	u32 id = HackMonId_Get(mon);
	if (id == 0)
		return MOEMON_HIT_OF(mon->personality) && !HackMoemonSwitch_IsOldMon(mon->personality);
	return MOEMON_HIT_OF(id >> 16);
}

bool8 HackMonId_IsShinyBox(const struct BoxPokemon* mon)
{
	u32 id = HackMonId_Get(mon);
	return (id != 0) ? IdShiny(id) : OldShiny(mon->otId, mon->personality);
}

//functionrewrites: IsMonShiny 08043AB8
bool8 HackMonId_IsMonShiny(struct Pokemon* mon)
{
	return HackMonId_IsShinyBox((struct BoxPokemon*) mon);
}

extern struct CompressedPokemon* const sPokemonBoxPtrs[25];   //20〜25 箱（棚でない箱・src/pokemon_storage_system.c）

//functionrewrites: IsShinyOtIdPersonality 08043AE4
bool8 HackMonId_IsShinyOtIdPersonality(u32 otId, u32 personality)
{
	u32 i;
	struct Pokemon* const lists[2] = {gPlayerParty, gEnemyParty};

	for (i = 0; i < 2 * PARTY_SIZE; ++i)
	{
		struct BoxPokemon* m = (struct BoxPokemon*) &lists[i / PARTY_SIZE][i % PARTY_SIZE];
		if (m->personality == personality && m->otId == otId && m->substruct0.species != SPECIES_NONE)
			return HackMonId_IsShinyBox(m);
	}
	{
		//今の箱（パソコンの画面）。棚の箱は入れ物にあるときだけ見る（ここでフラッシュから読み込まない＝野生を作るたびに呼ばれる）
		u8 box = StorageGetCurrentBox();
		const u8* base = (const u8*) BoxShelf_PeekBoxMons(box);
		if (base == NULL && BoxShelf_ShelfIndex(box) < 0 && box < 25)
			base = (const u8*) sPokemonBoxPtrs[box];
		for (i = 0; base != NULL && i < IN_BOX_COUNT; ++i)
		{
			const u8* c = base + i * 58;
			if ( Rd32(c) == personality && Rd32(c + 4) == otId && Rd16(c + 0x1C) != SPECIES_NONE)
			{
				u32 id = HackMonId_UnpackComp(c);
				return (id != 0) ? IdShiny(id) : OldShiny(otId, personality);
			}
		}
	}
	return OldShiny(otId, personality);
}

//DPE の src/hack_moemon.c から（gHackMonIdBridge・tools/build/build_rom.py が番地を書く）。
//性格値しか来ないので、手持ち・相手・今の箱から同じ性格値の子を探す → 0x10000 | 裏 ID（ID の無い子・見つからなければ 0）
u32 HackMonId_MoemonKey(u32 personality)
{
	u32 i;
	struct Pokemon* const lists[2] = {gPlayerParty, gEnemyParty};

	for (i = 0; i < 2 * PARTY_SIZE; ++i)
	{
		struct BoxPokemon* m = (struct BoxPokemon*) &lists[i / PARTY_SIZE][i % PARTY_SIZE];
		if (m->personality == personality && m->substruct0.species != SPECIES_NONE)
		{
			u32 id = HackMonId_Get(m);
			return id ? (0x10000 | (id >> 16)) : 0;
		}
	}
	{
		u8 box = StorageGetCurrentBox();
		const u8* base = (const u8*) BoxShelf_PeekBoxMons(box);
		if (base == NULL && BoxShelf_ShelfIndex(box) < 0 && box < 25)
			base = (const u8*) sPokemonBoxPtrs[box];
		for (i = 0; base != NULL && i < IN_BOX_COUNT; ++i)
		{
			const u8* c = base + i * 58;
			if (Rd32(c) == personality && Rd16(c + 0x1C) != SPECIES_NONE)
			{
				u32 id = HackMonId_UnpackComp(c);
				return id ? (0x10000 | (id >> 16)) : 0;
			}
		}
	}
	return 0;
}

void HackMonId_FieldTick(void)
{
	u32 i;
	for (i = 0; i < PARTY_SIZE; ++i)
		HackMonId_Ensure((struct BoxPokemon*) &gPlayerParty[i]);
}
