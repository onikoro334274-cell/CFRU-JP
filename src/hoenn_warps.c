#include "defines.h"

#include "Tables/hoenn_warps.h"

/*
hoenn_warps.c
    トウカのもり北側の入口（`MB_DEEP_SOUTH_WARP`）を素 FRLG に持ち込む。

    ## なぜ要るか（2026-09-22 にユーザーが実機で発見）

    「マップ移動がワープになっている　トウカシティ側は問題ない」

    素 FRLG の `MB_REGULAR_WARP`(0x67) は `MetatileBehavior_IsWarpPad` で、
    踏むと **`DoTeleportWarp()`**（シルフカンパニーの転送板の演出）になる。
    移植の変換表は陸のワープをまとめて 0x67 に寄せていたので、
    エメラルドでは**ただの暗転**だった入口まで転送板になっていた。

    ★ 同じトウカのもりでも**南側 (10,38) は `MB_NON_ANIMATED_DOOR`**
      （→ 素の 0x60）だったので正しく動いていた。
      **北側 (10,30) だけが `MB_DEEP_SOUTH_WARP`**（2 マス）。

    ## 0x6E が素 FRLG に無い

    エメラルドでは `MB_DEEP_SOUTH_WARP` は

      ・`MetatileBehavior_IsNonAnimDoor` が真
        → ワープは**ただの暗転**、着いたら**一歩出る**
      ・`MetatileBehavior_IsDeepSouthWarp` が真
        → 着いたとき **北**を向く（森から出てくる向き）

    素 FRLG の `IsNonAnimDoor` は 0x60 しか真にせず、そのときの向きは**南**。
    北側の入口は**南が木で塞がっている**（実測: (10,31) は通行不可）ので、
    0x60 に寄せると木に向かって足踏みする。

    ## 直し方

    ホウエン専用の値 **0xEC** を取り、判定を 2 つとも開ける。

    | 判定 | 番地 | 開け方 |
    |---|---|---|
    | `MetatileBehavior_IsNonAnimDoor` | `08059548` | `hooks`（この関数） |
    | `MetatileBehavior_IsDeepSouthWarp` | `0805955C` | 呼び元の BL 4 バイト |

    ★ `IsDeepSouthWarp` は **4 バイトの空関数**なので `functionrewrites` では
      潰せない（CLAUDE.md 6.22）。呼び元は `0805545A` の 1 箇所だけで、
      受け方が `CMP r0,#1` なので **`SUB r0,#0xEB` ＋ `NOP`** に置き換える。
      0xEC のときだけ r0 が 1 になって北向きが選ばれる。

    ★ `IsNonAnimDoor` は 20 バイトあり、呼び元は 4 箇所。
      入口 8 バイトを潰すが、**その途中へ飛んでくる分岐は無い**
      （`verify_hoenn_behaviors.py` が毎回確かめる）。
      素の 0x60 は従来どおり真のままなので、**カントーは何も変わらない**。
*/

/*
    ## 水の南向き矢印ワープ（2026-09-23）

    ユーザー実機「海底洞窟の出入り口の出口がワープになっている」。
    エメラルドの MB_WATER_SOUTH_ARROW_WARP(0x6D) を以前は 0x67（転送板）に寄せていたので、
    海の中の出口が転送板の演出になっていた。
    専用の値 HOENN_MB_WATER_SOUTH_ARROW_WARP（0xED・なみのり可）を与え、
    素の `MetatileBehavior_IsSouthArrowWarp`（080595D4・20 バイト）を入口ごと差し替える。
    呼び元は BL 4 か所と関数ポインタの表 2 か所（0831F8EC / 0831F918）で、入口を替えれば全部に効く。
    素の 0x65 は従来どおり真なので、カントーは何も変わらない。
*/
bool8 HoennIsSouthArrowWarp(u8 behavior)
{
	return behavior == HOENN_MB_SOUTH_ARROW_WARP
	    || behavior == HOENN_MB_WATER_SOUTH_ARROW_WARP;
}

bool8 HoennIsNonAnimDoor(u8 behavior)
{
	return behavior == HOENN_MB_CAVE_DOOR
	    || behavior == HOENN_MB_DEEP_SOUTH_WARP;
}
