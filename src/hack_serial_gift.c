#include "defines.h"
#include "../include/data2.h"
#include "../include/event_data.h"
#include "../include/new/text.h"
#include "../include/item.h"
#include "../include/naming_screen.h"
#include "../include/new/catching.h"
#include "../include/pokedex.h"
#include "../include/pokemon.h"
#include "../include/save.h"
#include "../include/script.h"
#include "../include/string_util.h"
#include "../include/constants/items.h"
#include "../include/constants/species.h"
#include "hack_mon_id.h"
#include "../include/new/achievements.h"

/*
 * シリアルコードの ふしぎなおくりもの（2026-10-11 ユーザー「ふしぎなおくりものをシリアルコードで入力して受け取るみたいなシステムできる？」
 * →「コードの中に品の中身を書き込む」案・patches/0329）。
 *
 * - タイトルのあとのメニューの「ふしぎなもらいもの」（src/bw_main_menu.c）→ CFRU の合言葉の入力画面（NAMING_SCREEN_ENTER_PASSWORD・12 文字）
 *   → HackSerial_AfterInput: 空なら今までの おくりもの（HackMG_Receive）・コードなら確かめて受け取り待ちに置き、レポートを書く。
 * - 品はポケセン 2 階の配達員が渡す（src/hack_mystery_gift.c の HackMG_GetDeliverymanScript → HackSerial_EventScript_Deliveryman）。
 * - コード 12 文字 = 60 ビット（形は tools/serial_gift/serial_gift.py の頭）: 上 24 ビット 印（SipHash-2-4 の下 24）・下 36 ビット 中身 XOR 覆い。
 *   中身: 版 2・おくりもの番号 8・種類 2（0 道具・1 ポケモン）・本体 24。
 * - 1 つのセーブで同じおくりもの番号は 1 回だけ（フラグ 0xD00 + 番号・5e の割り当て）。受け取り待ちは 1 つだけ（変数 0x50D8〜0x50DA）。
 * - 鍵（sSerialKey）は道具の Python がここから読む＝書くのはここだけ。**ROM を読めば誰でもコードを作れる**（暗号ではない）。
 * - 64 ビットの数を変数の数だけずらす（__aeabi_llsl）・割る は使わない（CFRU にない／6.83）。ずらしは定数だけ。
 */

#define SERIAL_GIFT_FLAG_BASE 0xD00        //受け取った印（おくりもの番号 0〜255）
#define VAR_SERIAL_GIFT_0 0x50D8           //受け取り待ちの中身の下 16 ビット
#define VAR_SERIAL_GIFT_1 0x50D9           //16〜31 ビット
#define VAR_SERIAL_GIFT_2 0x50DA           //上 4 ビット・bit 15 = 受け取り待ちがある
#define SERIAL_PENDING 0x8000
#define SERIAL_CODE_LEN 12
#define SERIAL_VERSION 1
#define SERIAL_TYPE_ITEM 0
#define SERIAL_TYPE_MON 1

#if ACH_FLAG_START + 4 * ACH_MAX_COUNT > SERIAL_GIFT_FLAG_BASE
#error "じっせきのフラグ（ACH_FLAG_START〜）がシリアルコードの印 0xD00〜 と重なる"
#endif

#define SAVE_NORMAL 0
#define SAVE_STATUS_OK 1

static const u32 sSerialKey[4] = {0xA8704390, 0x8EB73885, 0x3E30142C, 0x98AF0569};
static const u8 sSerialAlphabet[32] = "ABCDEFGHJKLMNPQRSTUVWXYZ23456789";   //ゲームの文字では引かない（下の CharValue）

extern const u8 gText_HackSerial_Received[];
extern const u8 gText_HackSerial_Wrong[];
extern const u8 gText_HackSerial_Used[];
extern const u8 gText_HackSerial_Pending[];
extern const u8 gText_HackSerial_Unknown[];
extern const u8 gText_HackMG_SaveFailed[];
extern const u8 HackSerial_EventScript_Deliveryman[];
void HackMG_Receive(void);

/* ---- SipHash-2-4（tools/serial_gift/serial_gift.py の siphash24 と同じ）---- */
#define ROTL(x, b) (((x) << (b)) | ((x) >> (64 - (b))))
#define SIPROUND do { \
	v0 += v1; v1 = ROTL(v1, 13); v1 ^= v0; v0 = ROTL(v0, 32); \
	v2 += v3; v3 = ROTL(v3, 16); v3 ^= v2; \
	v0 += v3; v3 = ROTL(v3, 21); v3 ^= v0; \
	v2 += v1; v1 = ROTL(v1, 17); v1 ^= v2; v2 = ROTL(v2, 32); \
} while (0)

//len は 8 未満だけ（使うのは 5 バイトと 3 バイト）。バイトを 64 ビットへ並べるのは u32 の半分ずつ（変数の数のずらしを 64 ビットでしない）
static u64 SipHash(u64 k0, u64 k1, const u8* data, u32 len)
{
	u64 v0 = 0x736f6d6570736575ULL ^ k0, v1 = 0x646f72616e646f6dULL ^ k1;
	u64 v2 = 0x6c7967656e657261ULL ^ k0, v3 = 0x7465646279746573ULL ^ k1;
	u32 lo = 0, hi = (len & 0xFF) << 24;
	u64 m;
	u32 i;

	for (i = 0; i < len && i < 4; i++)
		lo |= (u32) data[i] << (8 * i);
	for (; i < len; i++)
		hi |= (u32) data[i] << (8 * (i - 4));
	m = ((u64) hi << 32) | lo;
	v3 ^= m; SIPROUND; SIPROUND; v0 ^= m;
	v2 ^= 0xFF;
	SIPROUND; SIPROUND; SIPROUND; SIPROUND;
	return v0 ^ v1 ^ v2 ^ v3;
}

static u64 Key0(void) { return ((u64) sSerialKey[1] << 32) | sSerialKey[0]; }
static u64 Key1(void) { return ((u64) sSerialKey[3] << 32) | sSerialKey[2]; }

static u32 MacOf(u64 payload)
{
	u8 b[5];
	u32 lo = (u32) payload, hi = (u32) (payload >> 32);

	b[0] = lo; b[1] = lo >> 8; b[2] = lo >> 16; b[3] = lo >> 24; b[4] = hi;
	return (u32) SipHash(Key0(), Key1(), b, 5) & 0xFFFFFF;
}

static u64 MaskOf(u32 mac)
{
	u8 b[3];

	b[0] = mac; b[1] = mac >> 8; b[2] = mac >> 16;
	return SipHash(Key1(), Key0(), b, 3) & 0xFFFFFFFFFULL;
}

//ゲームの文字 → 0〜31（小文字は大文字に・使わない文字は 0xFF）
static u8 CharValue(u8 c)
{
	u32 i;

	if (c >= PC_a && c < PC_a + 26)
		c = c - PC_a + PC_A;
	for (i = 0; i < 32; i++)
	{
		u8 a = sSerialAlphabet[i];
		u8 g = (a >= 'A' && a <= 'Z') ? PC_A + (a - 'A') : PC_0 + (a - '0');
		if (g == c)
			return i;
	}
	return 0xFF;
}

//コード（ゲームの文字・EOS まで）→ 中身（36 ビット）。合わなければ 0（中身 0 は版 0 なので本物には無い）
u64 HackSerial_Decode(const u8* code)
{
	u64 v = 0, payload;
	u32 i, n = 0, mac;

	for (i = 0; code[i] != EOS && i < 16; i++)   //入力画面の入れ物（gStringVar1）は 12 文字＋EOS・念のため 16 まで
	{
		u8 d;
		if (code[i] == 0x00 || code[i] == 0xAE)   //空白・「ー」は飛ばす（区切りに打っても通す）
			continue;
		d = CharValue(code[i]);
		if (d == 0xFF || n >= SERIAL_CODE_LEN)
			return 0;
		v = (v << 5) | d;
		n++;
	}
	if (n != SERIAL_CODE_LEN)
		return 0;
	mac = (u32) (v >> 36);
	payload = (v & 0xFFFFFFFFFULL) ^ MaskOf(mac);
	if (MacOf(payload) != mac || (u32) (payload >> 34) != SERIAL_VERSION)
		return 0;
	return payload;
}

static u32 GiftId(u64 p) { return (u32) (p >> 26) & 0xFF; }
static u32 GiftKind(u64 p) { return (u32) (p >> 24) & 3; }
static u32 GiftBody(u64 p) { return (u32) p & 0xFFFFFF; }

static bool8 GiftValid(u64 p)
{
	u32 body = GiftBody(p);

	if (GiftKind(p) == SERIAL_TYPE_ITEM)
	{
		u32 item = body >> 13, count = (body >> 6) & 0x7F;
		return item != ITEM_NONE && item < ITEMS_COUNT && count != 0;
	}
	if (GiftKind(p) == SERIAL_TYPE_MON)
	{
		u32 species = body >> 13, level = (body >> 5) & 0xFF;
		return species != SPECIES_NONE && species < NUM_SPECIES && species != SPECIES_EGG && level >= 1 && level <= MAX_LEVEL;
	}
	return FALSE;
}

static bool8 HasPending(void)
{
	return (VarGet(VAR_SERIAL_GIFT_2) & SERIAL_PENDING) != 0;
}

static u64 Pending(void)
{
	return ((u64) (VarGet(VAR_SERIAL_GIFT_2) & 0xF) << 32) | ((u32) VarGet(VAR_SERIAL_GIFT_1) << 16) | VarGet(VAR_SERIAL_GIFT_0);
}

static void SetPending(u64 p)
{
	VarSet(VAR_SERIAL_GIFT_0, (u16) p);
	VarSet(VAR_SERIAL_GIFT_1, (u16) (p >> 16));
	VarSet(VAR_SERIAL_GIFT_2, SERIAL_PENDING | ((u32) (p >> 32) & 0xF));
}

static void ClearPending(void)
{
	VarSet(VAR_SERIAL_GIFT_0, 0);
	VarSet(VAR_SERIAL_GIFT_1, 0);
	VarSet(VAR_SERIAL_GIFT_2, 0);
}

bool8 HackSerial_HasPending(void)
{
	return HasPending();
}

const u8* HackSerial_DeliverymanScript(void)
{
	return HasPending() ? HackSerial_EventScript_Deliveryman : NULL;
}

//入力画面から戻ったとき（gStringVar1 = 打った文字）。結果の文を gStringVar4 に（メニューが出す）
void HackSerial_AfterInput(void)
{
	u64 p;

	if (gStringVar1[0] == EOS)
	{
		HackMG_Receive();   //空なら今までの おくりもの（がくしゅうそうち など）
		return;
	}
	p = HackSerial_Decode(gStringVar1);
	if (p == 0)
		StringCopy(gStringVar4, gText_HackSerial_Wrong);
	else if (!GiftValid(p))
		StringCopy(gStringVar4, gText_HackSerial_Unknown);
	else if (FlagGet(SERIAL_GIFT_FLAG_BASE + GiftId(p)))
		StringCopy(gStringVar4, gText_HackSerial_Used);
	else if (HasPending())
		StringCopy(gStringVar4, gText_HackSerial_Pending);
	else
	{
		FlagSet(SERIAL_GIFT_FLAG_BASE + GiftId(p));
		SetPending(p);
		if (TrySavingData(SAVE_NORMAL) == SAVE_STATUS_OK)
			StringCopy(gStringVar4, gText_HackSerial_Received);
		else
		{
			FlagClear(SERIAL_GIFT_FLAG_BASE + GiftId(p));   //書けなかったら無かったことに（RAM の上だけ）
			ClearPending();
			StringCopy(gStringVar4, gText_HackMG_SaveFailed);
		}
	}
}

//配達員の台本から（callasm）: Var8004 = 種類（0 道具・1 ポケモン・0xFF なし）・道具なら Var8005 番号・Var8006 個数（obtainitem に渡す）
void HackSerial_Prepare(void)
{
	u64 p = Pending();
	u32 body = GiftBody(p);

	if (!HasPending() || !GiftValid(p))
	{
		ClearPending();
		Var8004 = 0xFF;
		return;
	}
	Var8004 = GiftKind(p);
	Var8005 = body >> 13;
	Var8006 = (body >> 6) & 0x7F;
}

#define SaveMapView ((void (*)(void)) (0x08058994 | 1))
bool8 HackMG_HasOldGifts(void);   //src/hack_mystery_gift.c

//渡したあと（台本から・道具はバッグに入ったあと・ポケモンは文を出したあと）: 受け取り待ちを消して**すぐレポートを書く**
//（5e のレビュー 2026-10-11: RAM の上だけで消すと、レポートを書かずにリセットすれば同じ品をもう一度受け取れた）。
//LASTRESULT = 1 なら今までの おくりもの（がくしゅうそうち など）がまだある＝配達員は消えずに続けて渡す
void HackSerial_Finish(void)
{
	ClearPending();
	SaveMapView();
	TrySavingData(SAVE_NORMAL);   //書けなくても品は渡してある（書けなかったときに複製できるのは素のレポートと同じ）
	gSpecialVar_LastResult = HackMG_HasOldGifts();
}

//ポケモン（台本から）: LASTRESULT 0 手持ち / 1 パソコン / 2 どちらもいっぱい（受け取り待ちのまま）。gStringVar1 = 種族名
void HackSerial_GiveMon(void)
{
	struct Pokemon mon;
	u32 body = GiftBody(Pending());
	u16 species = body >> 13;
	u8 level = (body >> 5) & 0xFF;
	u8 result;

	CreateMon(&mon, species, level, 32, FALSE, 0, OT_ID_PLAYER_ID, 0);   //出会ったレベルの欄は 7 ビット（Lv128 から切れる・63 節）・ボールは CreateMon の既定（モンスターボール）
	if (body & (1 << 4))
		HackMonId_MakeShiny((struct BoxPokemon*) &mon);    //性格値は触らない（6.5）
	if (body & (1 << 3))
		HackMonId_MakeMoemon((struct BoxPokemon*) &mon);
	result = GiveMonToPlayer(&mon);
	gSpecialVar_LastResult = result;
	StringCopy(gStringVar1, gSpeciesNames[species]);
	if (result < 2)
	{
		HandleSetPokedexFlag(SpeciesToNationalPokedexNum(species), FLAG_SET_SEEN, mon.personality);
		HandleSetPokedexFlag(SpeciesToNationalPokedexNum(species), FLAG_SET_CAUGHT, mon.personality);
	}   //受け取り待ちは文のあとの HackSerial_Finish で消す（レポートも）
}
