#include "defines.h"
#include "../include/pokemon.h"

/*
    BW 風の手持ち画面（2 列 3 段）— カーソル移動（#19 の 3 つ目）

    枠の形・窓の型・座標表・文字の位置表は tools/bwparty/build_bw_party.py が
    bytereplacement / repoints で素の表を書き換える（docs/research/2026-09-19_bw-party-menu.md）。
    C で書き直すのは**カーソル移動だけ**:

      UpdatePartySelectionSingleLayout（BPRJ 081207E0）
      UpdatePartySelectionDoubleLayout（BPRJ 081208E8）
        → どちらも BwPmUpdateSelection（functionrewrites・引数 2・戻り値なし）

    素は「左の大きな枠 1 つ + 右の小さな枠 5 つ」用の動きで、2 列 3 段には合わない。
    こちらは 6 枠を 枠 i → 列 i%2・段 i/2 に置くので、
      上下: 同じ列の上下の枠（ポケモンがいる枠だけ。段の外は CANCEL / CONFIRM）
      左右: 隣の列の同じ段（ポケモンがいれば）
    とした。CONFIRM（6）と CANCEL（7）の扱いは素と同じ
    （CONFIRM は複数選ぶ画面だけ。sPartyMenuInternal->chooseMultiple）。

    ★ 書き込み可能な static は無い（6.13）。
*/

/* ★ 素の番号は CONFIRM = 6（PARTY_SIZE）/ CANCEL = 7（PARTY_SIZE + 1）。逆に書いていて、
   「もどる」で A を押すと PartyMenuButtonHandler が B 扱いにしてくれず、
   HandleChooseMonSelection が「6 = けってい」として gPartyMenu.task（通常は空）を呼んで落ちた（実機 2026-09-20） */
#define SLOT_CONFIRM 6          /* PARTY_SIZE */
#define SLOT_CANCEL 7           /* PARTY_SIZE + 1 */
#define MENU_DIR_DOWN 1
#define MENU_DIR_UP (-1)
#define MENU_DIR_RIGHT 2
#define MENU_DIR_LEFT (-2)

/* 素の sPartyMenuInternal（BPRJ.ld 0203B010 = 入れ物へのポインタ）。
   +8 の u32 の bit0 が chooseMultiple（pokefirered の struct PartyMenuInternal） */
#define BwPmChooseMultiple() ((*(u32 *)(*(u8 **)0x0203B010 + 8)) & 1)


static bool8 HasMon(s8 slot)
{
    if (slot < 0 || slot >= 6)
        return FALSE;
    return GetMonData(&gPlayerParty[slot], MON_DATA_SPECIES, NULL) != SPECIES_NONE;
}

static s8 LastMon(void)
{
    s8 i;

    for (i = 5; i > 0; --i)
        if (HasMon(i))
            break;
    return i;
}

/* 同じ列を dir（±2）の向きに、ポケモンがいる枠まで進む。無ければ -1 */
static s8 NextInColumn(s8 slot, s8 dir)
{
    while (TRUE)
    {
        slot += dir;
        if (slot < 0 || slot >= 6)
            return -1;
        if (HasMon(slot))
            return slot;
    }
}

void BwPmUpdateSelection(s8 *slotPtr, s8 movementDir)
{
    s8 slot = *slotPtr;
    s8 next;

    switch (movementDir)
    {
    case MENU_DIR_UP:
        if (slot == SLOT_CANCEL)
        {
            *slotPtr = BwPmChooseMultiple() ? SLOT_CONFIRM : LastMon();
        }
        else if (slot == SLOT_CONFIRM)
        {
            *slotPtr = LastMon();
        }
        else
        {
            next = NextInColumn(slot, -2);
            *slotPtr = (next >= 0) ? next : SLOT_CANCEL;
        }
        break;
    case MENU_DIR_DOWN:
        if (slot == SLOT_CANCEL)
        {
            *slotPtr = 0;
        }
        else if (slot == SLOT_CONFIRM)
        {
            *slotPtr = SLOT_CANCEL;
        }
        else
        {
            next = NextInColumn(slot, 2);
            if (next >= 0)
                *slotPtr = next;
            else
                *slotPtr = BwPmChooseMultiple() ? SLOT_CONFIRM : SLOT_CANCEL;
        }
        break;
    case MENU_DIR_RIGHT:
        if (slot < 6 && (slot & 1) == 0 && HasMon(slot + 1))
            *slotPtr = slot + 1;
        break;
    case MENU_DIR_LEFT:
        if (slot < 6 && (slot & 1) == 1)
            *slotPtr = slot - 1;
        break;
    }
}

/* 入れ替えのスライドが「2 つとも画面の外へ出たか」（assembly/bwparty/bw_party_hooks.s から）。
   素の Task_SlideSelectedSlotsOffscreen（08123A4C）は「左端 > 33」を u16 で見るので、左へ滑る枠は
   1 歩目で負になった瞬間に「出た」扱いになる。素は左へ滑る枠が左の大枠 1 つだけだったので右の枠を
   待つあいだに流れて見えていたが、2 列 3 段では左の列どうしの入れ替えが 1 歩で終わる（実機 2026-09-20）。
   data[0]/[4] = 左端, [2]/[6] = 幅, [8]/[9] = ずらした量（gTasks の data は s16） */
bool8 BwPmBothSlotsOffscreen(s16 *data)
{
    s16 x1 = data[0] + data[8];
    s16 x2 = data[4] + data[9];
    bool8 off1 = (x1 > 33) || (x1 + data[2] <= 0);
    bool8 off2 = (x2 > 33) || (x2 + data[6] <= 0);

    return off1 && off2;
}
