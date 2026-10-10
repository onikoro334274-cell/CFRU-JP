#include "defines.h"
#include "../include/data2.h"
#include "../include/constants/items.h"
#include "../include/event_data.h"
#include "../include/pokemon.h"
#include "../include/random.h"
#include "../include/string_util.h"
#include "../include/new/catching.h"
#include "../include/text.h"
#include "../include/constants/pokedex.h"
#include "../include/pokedex.h"
#include "../include/constants/pokemon.h"
#include "../include/new/evolution.h"
#include "Tables/hoenn_script_flags.h"

/*
hack_wonder_trade.c — ミラクル交換（2026-10-07 ユーザー「これいれてほしい　クレジットに図鑑ナビの人
	全ポケモンからランダムで　フォルムチェンジ系気をつけて　配置場所はだいすきクラブ」）
	元: pkhk_BeGiNNeR さんの「CFRUへのミラクル交換の実装」（wonder_trade.c・Wonder_Trades.s・Wonder_Trades.string・
	    wonder_trade_hooks.s。ずかんナビの日本語化の方）。ここでの変更:
	  ・1 日の回数の変数を 0x50F5〜0x50F7 に（元の 0x5100〜0x5102 はバトルファクトリーが使っている・
	    tools/rom/build_hoenn_scripts.py の VAR_RESERVED_FIRST を 0x50F5 に下げた）
	  ・出るポケモンは図鑑 1〜1025 の基本の姿から同じ確率で（元は第 3 世代までを 4 つの組に分けていた）。
	    DPE の NationalPokedexNumToSpecies（src/updated_code.c）は図鑑の番号の「いちばん前の種族」＝基本の姿を返すので、
	    メガ・ゲンシ・キョダイ・戦闘中だけの姿（ダルマモード・ブレードフォルムなど）・別の姿の種族番号は出ない
	置き場所: クチバのポケモンだいすきクラブの 9 人目（assembly/kanto/kanto_gible_egg.s の人物の表）。
	スクリプト assembly/hack_wonder_trade/wonder_trade_script.s・文 strings/hack_wonder_trade.string・
	交換の場面の文字のフック assembly/hack_wonder_trade/wonder_trade_hooks.s（hooks 08050770）

	1. 通信交換の場面（DoInGameTradeScene）で受け取る、ランダムなポケモンを gEnemyParty[0] に作る
	   （親の名前はランダム、ニックネームはポケモン名か、確率でランダムなニックネーム）
	2. 交換の場面の文（gStringVar1〜3）を差し替える。ミラクル交換のときは「だれか」と「受け取るポケモンの名前」

	Inputs:
		Var8005: 交換に出す手持ちの位置 (0-5)
*/
//トレーナー名の数
#define WONDER_TRADE_NUM_OT_NAMES 64

//（使っていない・2026-10-07 回数の制限を外した）1日にできる回数 (RTCの日付が変わるとリセットされる)
//・VAR_WONDER_TRADE_DAILY: 日付の記録用 (連続した2つの変数を使う)
//・VAR_WONDER_TRADE_COUNT: その日の交換回数
//※他の機能で使っていない変数番号にしてください
#define WONDER_TRADE_MAX_PER_DAY 5
#define VAR_WONDER_TRADE_DAILY 0x50F5 //0x50F5と0x50F6を使用（2026-10-07 元は 0x5100 = バトルファクトリーとぶつかる）
#define VAR_WONDER_TRADE_COUNT 0x50F7


//受け取るポケモンにニックネームが付く確率(パーセント)と、ニックネームの数
#define WONDER_TRADE_NICKNAME_PERCENT 10
#define WONDER_TRADE_NUM_NICKNAMES 64

//ミラクル交換の目印 (交換シーン中、Var8004 にこの値を入れておく)
#define WONDER_TRADE_MARKER 0xFFFF

//ゲーム内交換テーブル (BPRJ ROM)
#define IN_GAME_TRADE_TABLE 0x822D2F8
#define IN_GAME_TRADE_SIZE 0x3C
#define IN_GAME_TRADE_OT_NAME_OFFSET 0x2B

extern const u8 gText_WonderTrade_Someone[]; //だれか

extern const u8 gText_WonderTradeOT0[];
extern const u8 gText_WonderTradeOT1[];
extern const u8 gText_WonderTradeOT2[];
extern const u8 gText_WonderTradeOT3[];
extern const u8 gText_WonderTradeOT4[];
extern const u8 gText_WonderTradeOT5[];
extern const u8 gText_WonderTradeOT6[];
extern const u8 gText_WonderTradeOT7[];
extern const u8 gText_WonderTradeOT8[];
extern const u8 gText_WonderTradeOT9[];
extern const u8 gText_WonderTradeOT10[];
extern const u8 gText_WonderTradeOT11[];
extern const u8 gText_WonderTradeOT12[];
extern const u8 gText_WonderTradeOT13[];
extern const u8 gText_WonderTradeOT14[];
extern const u8 gText_WonderTradeOT15[];
extern const u8 gText_WonderTradeOT16[];
extern const u8 gText_WonderTradeOT17[];
extern const u8 gText_WonderTradeOT18[];
extern const u8 gText_WonderTradeOT19[];
extern const u8 gText_WonderTradeOT20[];
extern const u8 gText_WonderTradeOT21[];
extern const u8 gText_WonderTradeOT22[];
extern const u8 gText_WonderTradeOT23[];
extern const u8 gText_WonderTradeOT24[];
extern const u8 gText_WonderTradeOT25[];
extern const u8 gText_WonderTradeOT26[];
extern const u8 gText_WonderTradeOT27[];
extern const u8 gText_WonderTradeOT28[];
extern const u8 gText_WonderTradeOT29[];
extern const u8 gText_WonderTradeOT30[];
extern const u8 gText_WonderTradeOT31[];
extern const u8 gText_WonderTradeOT32[];
extern const u8 gText_WonderTradeOT33[];
extern const u8 gText_WonderTradeOT34[];
extern const u8 gText_WonderTradeOT35[];
extern const u8 gText_WonderTradeOT36[];
extern const u8 gText_WonderTradeOT37[];
extern const u8 gText_WonderTradeOT38[];
extern const u8 gText_WonderTradeOT39[];
extern const u8 gText_WonderTradeOT40[];
extern const u8 gText_WonderTradeOT41[];
extern const u8 gText_WonderTradeOT42[];
extern const u8 gText_WonderTradeOT43[];
extern const u8 gText_WonderTradeOT44[];
extern const u8 gText_WonderTradeOT45[];
extern const u8 gText_WonderTradeOT46[];
extern const u8 gText_WonderTradeOT47[];
extern const u8 gText_WonderTradeOT48[];
extern const u8 gText_WonderTradeOT49[];
extern const u8 gText_WonderTradeOT50[];
extern const u8 gText_WonderTradeOT51[];
extern const u8 gText_WonderTradeOT52[];
extern const u8 gText_WonderTradeOT53[];
extern const u8 gText_WonderTradeOT54[];
extern const u8 gText_WonderTradeOT55[];
extern const u8 gText_WonderTradeOT56[];
extern const u8 gText_WonderTradeOT57[];
extern const u8 gText_WonderTradeOT58[];
extern const u8 gText_WonderTradeOT59[];
extern const u8 gText_WonderTradeOT60[];
extern const u8 gText_WonderTradeOT61[];
extern const u8 gText_WonderTradeOT62[];
extern const u8 gText_WonderTradeOT63[];

static const u8* const sWonderTradeOTNames[WONDER_TRADE_NUM_OT_NAMES] =
{
	gText_WonderTradeOT0,
	gText_WonderTradeOT1,
	gText_WonderTradeOT2,
	gText_WonderTradeOT3,
	gText_WonderTradeOT4,
	gText_WonderTradeOT5,
	gText_WonderTradeOT6,
	gText_WonderTradeOT7,
	gText_WonderTradeOT8,
	gText_WonderTradeOT9,
	gText_WonderTradeOT10,
	gText_WonderTradeOT11,
	gText_WonderTradeOT12,
	gText_WonderTradeOT13,
	gText_WonderTradeOT14,
	gText_WonderTradeOT15,
	gText_WonderTradeOT16,
	gText_WonderTradeOT17,
	gText_WonderTradeOT18,
	gText_WonderTradeOT19,
	gText_WonderTradeOT20,
	gText_WonderTradeOT21,
	gText_WonderTradeOT22,
	gText_WonderTradeOT23,
	gText_WonderTradeOT24,
	gText_WonderTradeOT25,
	gText_WonderTradeOT26,
	gText_WonderTradeOT27,
	gText_WonderTradeOT28,
	gText_WonderTradeOT29,
	gText_WonderTradeOT30,
	gText_WonderTradeOT31,
	gText_WonderTradeOT32,
	gText_WonderTradeOT33,
	gText_WonderTradeOT34,
	gText_WonderTradeOT35,
	gText_WonderTradeOT36,
	gText_WonderTradeOT37,
	gText_WonderTradeOT38,
	gText_WonderTradeOT39,
	gText_WonderTradeOT40,
	gText_WonderTradeOT41,
	gText_WonderTradeOT42,
	gText_WonderTradeOT43,
	gText_WonderTradeOT44,
	gText_WonderTradeOT45,
	gText_WonderTradeOT46,
	gText_WonderTradeOT47,
	gText_WonderTradeOT48,
	gText_WonderTradeOT49,
	gText_WonderTradeOT50,
	gText_WonderTradeOT51,
	gText_WonderTradeOT52,
	gText_WonderTradeOT53,
	gText_WonderTradeOT54,
	gText_WonderTradeOT55,
	gText_WonderTradeOT56,
	gText_WonderTradeOT57,
	gText_WonderTradeOT58,
	gText_WonderTradeOT59,
	gText_WonderTradeOT60,
	gText_WonderTradeOT61,
	gText_WonderTradeOT62,
	gText_WonderTradeOT63,

};

//★ 2026-10-07 ユーザー「ミラクル交換の親の名前がおとこ固定っぽいので女っぽい名前がピンクになるように」:
//  親の性別を名前で決める（0 = おとこ・1 = おんな・2 = どちらでも＝ランダム）。並びは sWonderTradeOTNames と同じ
//  おんな: アスナ アユミ イクエ コズエ サエコ シホミ ショウコ ツツジ ナギ ナギサ ナツミ ハツネ フヨウ プリム ホノカ マキ マサコ
//          マドカ マナミ ママ マリア ミサコ ヤヨイ ユリカ ヨリコ レイカ カスミ／どちらでも: ももアイス カオル ツバサ
static const u8 sWonderTradeOTGenders[WONDER_TRADE_NUM_OT_NAMES] =
{
	2, 1, 0, 1, 1, 0, 0, 0, 2, 0, 0, 0, 0, 0, 1, 0,
	1, 1, 1, 0, 0, 0, 1, 2, 0, 0, 0, 1, 1, 1, 0, 0,
	1, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 0, 0,
	1, 0, 0, 0, 0, 1, 0, 0, 1, 0, 1, 1, 0, 0, 0, 1,
};

extern const u8 gText_WonderTradeNick0[];
extern const u8 gText_WonderTradeNick1[];
extern const u8 gText_WonderTradeNick2[];
extern const u8 gText_WonderTradeNick3[];
extern const u8 gText_WonderTradeNick4[];
extern const u8 gText_WonderTradeNick5[];
extern const u8 gText_WonderTradeNick6[];
extern const u8 gText_WonderTradeNick7[];
extern const u8 gText_WonderTradeNick8[];
extern const u8 gText_WonderTradeNick9[];
extern const u8 gText_WonderTradeNick10[];
extern const u8 gText_WonderTradeNick11[];
extern const u8 gText_WonderTradeNick12[];
extern const u8 gText_WonderTradeNick13[];
extern const u8 gText_WonderTradeNick14[];
extern const u8 gText_WonderTradeNick15[];
extern const u8 gText_WonderTradeNick16[];
extern const u8 gText_WonderTradeNick17[];
extern const u8 gText_WonderTradeNick18[];
extern const u8 gText_WonderTradeNick19[];
extern const u8 gText_WonderTradeNick20[];
extern const u8 gText_WonderTradeNick21[];
extern const u8 gText_WonderTradeNick22[];
extern const u8 gText_WonderTradeNick23[];
extern const u8 gText_WonderTradeNick24[];
extern const u8 gText_WonderTradeNick25[];
extern const u8 gText_WonderTradeNick26[];
extern const u8 gText_WonderTradeNick27[];
extern const u8 gText_WonderTradeNick28[];
extern const u8 gText_WonderTradeNick29[];
extern const u8 gText_WonderTradeNick30[];
extern const u8 gText_WonderTradeNick31[];
extern const u8 gText_WonderTradeNick32[];
extern const u8 gText_WonderTradeNick33[];
extern const u8 gText_WonderTradeNick34[];
extern const u8 gText_WonderTradeNick35[];
extern const u8 gText_WonderTradeNick36[];
extern const u8 gText_WonderTradeNick37[];
extern const u8 gText_WonderTradeNick38[];
extern const u8 gText_WonderTradeNick39[];
extern const u8 gText_WonderTradeNick40[];
extern const u8 gText_WonderTradeNick41[];
extern const u8 gText_WonderTradeNick42[];
extern const u8 gText_WonderTradeNick43[];
extern const u8 gText_WonderTradeNick44[];
extern const u8 gText_WonderTradeNick45[];
extern const u8 gText_WonderTradeNick46[];
extern const u8 gText_WonderTradeNick47[];
extern const u8 gText_WonderTradeNick48[];
extern const u8 gText_WonderTradeNick49[];
extern const u8 gText_WonderTradeNick50[];
extern const u8 gText_WonderTradeNick51[];
extern const u8 gText_WonderTradeNick52[];
extern const u8 gText_WonderTradeNick53[];
extern const u8 gText_WonderTradeNick54[];
extern const u8 gText_WonderTradeNick55[];
extern const u8 gText_WonderTradeNick56[];
extern const u8 gText_WonderTradeNick57[];
extern const u8 gText_WonderTradeNick58[];
extern const u8 gText_WonderTradeNick59[];
extern const u8 gText_WonderTradeNick60[];
extern const u8 gText_WonderTradeNick61[];
extern const u8 gText_WonderTradeNick62[];
extern const u8 gText_WonderTradeNick63[];

static const u8* const sWonderTradeNicknames[WONDER_TRADE_NUM_NICKNAMES] =
{
	gText_WonderTradeNick0,
	gText_WonderTradeNick1,
	gText_WonderTradeNick2,
	gText_WonderTradeNick3,
	gText_WonderTradeNick4,
	gText_WonderTradeNick5,
	gText_WonderTradeNick6,
	gText_WonderTradeNick7,
	gText_WonderTradeNick8,
	gText_WonderTradeNick9,
	gText_WonderTradeNick10,
	gText_WonderTradeNick11,
	gText_WonderTradeNick12,
	gText_WonderTradeNick13,
	gText_WonderTradeNick14,
	gText_WonderTradeNick15,
	gText_WonderTradeNick16,
	gText_WonderTradeNick17,
	gText_WonderTradeNick18,
	gText_WonderTradeNick19,
	gText_WonderTradeNick20,
	gText_WonderTradeNick21,
	gText_WonderTradeNick22,
	gText_WonderTradeNick23,
	gText_WonderTradeNick24,
	gText_WonderTradeNick25,
	gText_WonderTradeNick26,
	gText_WonderTradeNick27,
	gText_WonderTradeNick28,
	gText_WonderTradeNick29,
	gText_WonderTradeNick30,
	gText_WonderTradeNick31,
	gText_WonderTradeNick32,
	gText_WonderTradeNick33,
	gText_WonderTradeNick34,
	gText_WonderTradeNick35,
	gText_WonderTradeNick36,
	gText_WonderTradeNick37,
	gText_WonderTradeNick38,
	gText_WonderTradeNick39,
	gText_WonderTradeNick40,
	gText_WonderTradeNick41,
	gText_WonderTradeNick42,
	gText_WonderTradeNick43,
	gText_WonderTradeNick44,
	gText_WonderTradeNick45,
	gText_WonderTradeNick46,
	gText_WonderTradeNick47,
	gText_WonderTradeNick48,
	gText_WonderTradeNick49,
	gText_WonderTradeNick50,
	gText_WonderTradeNick51,
	gText_WonderTradeNick52,
	gText_WonderTradeNick53,
	gText_WonderTradeNick54,
	gText_WonderTradeNick55,
	gText_WonderTradeNick56,
	gText_WonderTradeNick57,
	gText_WonderTradeNick58,
	gText_WonderTradeNick59,
	gText_WonderTradeNick60,
	gText_WonderTradeNick61,
	gText_WonderTradeNick62,
	gText_WonderTradeNick63,
};

//★ 2026-10-07 全ポケモン（図鑑 1〜NATIONAL_DEX_COUNT の基本の姿・2026-10-10 から追加ポケモンも）から同じ確率で選ぶ（元は第 3 世代までを 4 つの組に分けていた）。
//  NationalPokedexNumToSpecies（DPE）は図鑑の番号のいちばん前の種族＝基本の姿を返す。
//  データの無い番号（無いはず）・タマゴは選び直す
static u16 GetRandomWonderTradeSpecies(void)
{
	u32 tries;

	for (tries = 0; tries < 20; ++tries)
	{
		u16 species = NationalPokedexNumToSpecies(1 + Random() % NATIONAL_DEX_COUNT);   //★ 2026-10-10 ユーザー「追加のポケモンも含まれるように都度回収して」: 図鑑の数（config.h・カゲノナカニ 1026・リンドリス…）まで＝足せば自動で入る

		if (species != SPECIES_NONE && species != SPECIES_EGG && species < NUM_SPECIES && gBaseStats[species].baseHP != 0)
			return species;
	}
	return SPECIES_PIKACHU;
}

//ランダムなボールとテラスタイプ
//1にするとランダムになる。0にすると今までどおり (ボール: モンスターボール、テラスタイプ: ポケモンごとの標準タイプ)
#define WONDER_TRADE_RANDOM_BALL 1
#define WONDER_TRADE_RANDOM_TERA_TYPE 1

//受け取るポケモンのボールの候補 (マスターボール・プレシャスボール・パークボールなどは除外)
static const u8 sWonderTradeBalls[] =
{
	BALL_TYPE_ULTRA_BALL, BALL_TYPE_GREAT_BALL, BALL_TYPE_POKE_BALL, BALL_TYPE_SAFARI_BALL,
	BALL_TYPE_NET_BALL, BALL_TYPE_DIVE_BALL, BALL_TYPE_NEST_BALL, BALL_TYPE_REPEAT_BALL,
	BALL_TYPE_TIMER_BALL, BALL_TYPE_LUXURY_BALL, BALL_TYPE_PREMIER_BALL, BALL_TYPE_DUSK_BALL,
	BALL_TYPE_HEAL_BALL, BALL_TYPE_QUICK_BALL, BALL_TYPE_FAST_BALL, BALL_TYPE_LEVEL_BALL,
	BALL_TYPE_LURE_BALL, BALL_TYPE_HEAVY_BALL, BALL_TYPE_LOVE_BALL, BALL_TYPE_FRIEND_BALL,
	BALL_TYPE_MOON_BALL, BALL_TYPE_SPORT_BALL, BALL_TYPE_BEAST_BALL, BALL_TYPE_DREAM_BALL,
};

//受け取るポケモンのテラスタイプの候補 (ステラタイプも出したい場合は TYPE_STELLAR を追加する)
static const u8 sWonderTradeTeraTypes[] =
{
	TYPE_NORMAL, TYPE_FIGHTING, TYPE_FLYING, TYPE_POISON, TYPE_GROUND, TYPE_ROCK,
	TYPE_BUG, TYPE_GHOST, TYPE_STEEL, TYPE_FIRE, TYPE_WATER, TYPE_GRASS,
	TYPE_ELECTRIC, TYPE_PSYCHIC, TYPE_ICE, TYPE_DRAGON, TYPE_DARK, TYPE_FAIRY,
};

//★ 2026-10-07 ユーザー「1日5回まで制限はずして」: 1 日の回数の確かめ（元の CheckWonderTradeLimit）と数えるのをやめた。
//  変数 0x50F5〜0x50F7 は使わなくなったが予約のまま（tools/rom/build_hoenn_scripts.py の VAR_RESERVED_FIRST）

void CreateWonderTradePokemon(void)
{
	u8 i;
	u8 otName[PLAYER_NAME_LENGTH + 1];
	u8 nickname[POKEMON_NAME_LENGTH + 1];
	u8 otGender;
	u8 otIndex = Random() % WONDER_TRADE_NUM_OT_NAMES;
	u16 species = GetRandomWonderTradeSpecies();
	u8 slot = Var8005;
	u8 level;
	struct Pokemon* mon = &gEnemyParty[0];

	if (slot >= PARTY_SIZE)
		slot = 0;

	//交換に出すポケモンと同じレベルで受け取る (通常のゲーム内交換と同じ)
	level = GetMonData(&gPlayerParty[slot], MON_DATA_LEVEL, NULL);
	if (level == 0)
		level = 5;

	//親ID・性格値はランダム(プレイヤーとは別の親)
	CreateMon(mon, species, level, 32, FALSE, 0, OT_ID_PRESET, Random32());

	//ランダムな親の名前 (EOSで埋めてからコピー。名前は最大5文字)
	for (i = 0; i < PLAYER_NAME_LENGTH + 1; ++i)
		otName[i] = 0xFF;
	StringCopy(otName, sWonderTradeOTNames[otIndex]);
	otGender = sWonderTradeOTGenders[otIndex];
	if (otGender > 1)
		otGender = Random() & 1;

	//ニックネーム: 基本はポケモン名、一定の確率でランダムなニックネームが付く
	//(交換シーンのメッセージは BufferInGameTradeSceneStrings でポケモン名を表示する)
	for (i = 0; i < POKEMON_NAME_LENGTH + 1; ++i)
		nickname[i] = 0xFF;
	if (Random() % 100 < WONDER_TRADE_NICKNAME_PERCENT)
		StringCopy(nickname, sWonderTradeNicknames[Random() % WONDER_TRADE_NUM_NICKNAMES]); //最大5文字
	else
		GetSpeciesName(nickname, species);

	SetMonData(mon, MON_DATA_OT_NAME, otName);
	SetMonData(mon, MON_DATA_OT_GENDER, &otGender);
	SetMonData(mon, MON_DATA_NICKNAME, nickname);
	CalculateMonStats(mon);

#if WONDER_TRADE_RANDOM_BALL
	{
		u8 ball = sWonderTradeBalls[Random() % ARRAY_COUNT(sWonderTradeBalls)];
		SetMonData(mon, MON_DATA_POKEBALL, &ball);
	}
#endif

#if WONDER_TRADE_RANDOM_TERA_TYPE
	mon->teratype = sWonderTradeTeraTypes[Random() % ARRAY_COUNT(sWonderTradeTeraTypes)];
#endif

	//交換シーンのメッセージでミラクル交換だと判別するための目印
	Var8004 = WONDER_TRADE_MARKER;
}

//★ 2026-10-08 元の作者さんの直し（fix_wonder_names・ユーザー「これミラクル交換の直し　6文字対応」）:
//ポケモンの名前を表示用に取り出す
//JP版の仕様でニックネームが5文字までしか読めないため、
//「種族名の先頭5文字と同じ名前」なら 種族名をそのまま(最大10文字)使う
static void GetMonDisplayName(u8* dest, struct Pokemon* mon)
{
	u8 nickname[POKEMON_NAME_LENGTH + 2];
	u16 species = GetMonData(mon, MON_DATA_SPECIES, NULL);
	const u8* speciesName = gSpeciesNames[species];
	u8 i;

	GetMonData(mon, MON_DATA_NICKNAME, nickname);
	nickname[POKEMON_NAME_LENGTH] = 0xFF;

	for (i = 0; nickname[i] != 0xFF && i < POKEMON_NAME_LENGTH; ++i)
	{
		if (speciesName[i] != nickname[i])
			break;
	}

	//ニックネームが 種族名の先頭と一致している(=種族名が5文字で切れているだけ)
	if (nickname[i] == 0xFF && i >= 5 && speciesName[i] != 0xFF)
		StringCopy(dest, speciesName);
	else
		StringCopy(dest, nickname);   //★ 2026-10-09 元の作者さんの直し（wonder_trade_fix）: JP版の StringCopy10 (0x8008870) は 5 文字までしかコピーしない
}

//交換シーンのメッセージ用の文字列バッファ (ゲーム内交換のとき)
//BufferTradeSceneStrings (0x80506E0) の「ゲーム内交換」側の処理(0x8050770～)を置き換える
//  gStringVar1: 相手のトレーナー名 / gStringVar2: 送るポケモンの名前 / gStringVar3: 受け取るポケモンの名前
void BufferInGameTradeSceneStrings(void)
{
	if (Var8004 == WONDER_TRADE_MARKER)
	{
		//ミラクル交換: 「だれか」 と 受け取るポケモンのポケモン名
		StringCopy(gStringVar1, gText_WonderTrade_Someone);
		StringCopy(gStringVar3, gSpeciesNames[GetMonData(&gEnemyParty[0], MON_DATA_SPECIES, NULL)]);   //★ 6 文字（元の作者さんの直し）
	}
	else
	{
		//通常のゲーム内交換 (元の処理と同じ)
		const u8* trade = (const u8*) (IN_GAME_TRADE_TABLE + Var8004 * IN_GAME_TRADE_SIZE);
		StringCopy(gStringVar1, trade + IN_GAME_TRADE_OT_NAME_OFFSET);
		StringCopy(gStringVar3, trade);   //★ 2026-10-09 同上（素の交換の表の名前は 0xFF で終わる）
	}

	GetMonDisplayName(gStringVar2, &gPlayerParty[Var8005]);
}

//★ 2026-10-07 ユーザー「ミラクル交換でポケモン選択時に確認が無いのが危ない」「持ち物を持ってる場合はそのまま消えるので確認があった方が良さそう」:
//  選んだ子（Var8004）のニックネームを gStringVar1・持ち物の名前を gStringVar2 に。Var8006 = 持ち物があるか
//★ 2026-10-10 ユーザー「設定でミラクル交換の通信シーンをカットできるように」: せっていの「ミラクルこうかん」が みない（旗 HACK_FLAG_WONDER_SKIP）なら、
//  通信交換の場面（special 0xFE）を出さずにここで入れ替える（素の trade_scene.c の TradeMons と同じ: 入れ替え・なつき度 70・図鑑）。
//  交換で進化する子とメールを持たせた子は、場面（進化の場面・メールの片付け）が要るのでいつもどおり場面を出す。
//  入れ替えたら LASTRESULT = 1・gStringVar1 にもらった子の名前
void WonderTrade_TrySkipScene(void)
{
	struct Pokemon* mine;
	struct Pokemon* theirs = &gEnemyParty[0];
	struct Pokemon tmp;
	u16 item, species;
	u8 friendship = 70;

	gSpecialVar_LastResult = FALSE;
	if (!FlagGet(HACK_FLAG_WONDER_SKIP) || Var8005 >= PARTY_SIZE)
		return;
	mine = &gPlayerParty[Var8005];
	item = GetMonData(mine, MON_DATA_HELD_ITEM, NULL);
	if (item >= ITEM_ORANGE_MAIL && item <= ITEM_RETRO_MAIL)
		return;
	if (GetEvolutionTargetSpecies(theirs, EVO_MODE_TRADE, ITEM_NONE) != SPECIES_NONE)
		return;
	tmp = *mine;
	*mine = *theirs;
	*theirs = tmp;
	if (!GetMonData(mine, MON_DATA_IS_EGG, NULL))
	{
		SetMonData(mine, MON_DATA_FRIENDSHIP, &friendship);
		species = GetMonData(mine, MON_DATA_SPECIES, NULL);
		HandleSetPokedexFlag(SpeciesToNationalPokedexNum(species), FLAG_SET_SEEN, mine->personality);
		HandleSetPokedexFlag(SpeciesToNationalPokedexNum(species), FLAG_SET_CAUGHT, mine->personality);
	}
	GetMonData(mine, MON_DATA_NICKNAME, gStringVar1);
	gSpecialVar_LastResult = TRUE;
}

//★ 2026-10-10 ユーザー「ミラクル交換の回数で実績システムを作って」: 交換が済んだら数える（場面あり・なし のどちらも
//  WonderTrade_AfterScene を通る）。数はゲームの記録 GAME_STAT_WONDER_TRADES（じっせきの STAT で読む）
void WonderTrade_CountTrade(void)
{
	IncrementGameStat(GAME_STAT_WONDER_TRADES);
}

void WonderTrade_BufferChosen(void)
{
	struct Pokemon *mon = &gPlayerParty[Var8004];
	u16 item = GetMonData(mon, MON_DATA_HELD_ITEM, NULL);

	GetMonDisplayName(gStringVar1, mon);    //★ 2026-10-08 6 文字（元の作者さんの直しと同じ）
	Var8006 = (item == ITEM_MOMO_ICE) ? 2 : (item != ITEM_NONE);   //★ 2026-10-09 2 = ももアイス（交換に出せない）
	if (item != ITEM_NONE)
		CopyItemName(item, gStringVar2);
}
