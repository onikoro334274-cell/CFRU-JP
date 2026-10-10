#include "defines.h"

/*
hoenn_tileset_anim.c
    ホウエンのタイルセット・アニメ（花・水面・海流ほか）を動かす。

    ★ やりたいこと #13。詳細は
      docs/research/2026-09-15_tileset-animation.md

    仕組み:
      タイルセットの callback（構造体の +0x10）は「毎フレーム関数を RAM に
      登録するだけ」の関数。登録された関数が毎フレーム呼ばれ、
      AppendTilesetAnimToBuffer(src, dest, size) で VRAM にコマを積む。

      素 FRLG の値（BPRJ で実測）:
        AppendTilesetAnimToBuffer     0x0806F6A8
        プライマリ  カウンタ 0x03000FAE / 周回 0x03000FB0 / 関数 0x03000FB8
        セカンダリ  カウンタ 0x03000FB2 / 周回 0x03000FB4 / 関数 0x03000FBC

    ★ 表は「timer ごとの書き込み一覧」。
      エメラルドの毎フレーム関数を tools/rom/thumb_emu.py で実行し、
      AppendTilesetAnimToBuffer の引数をそのまま記録したもの。
      ここでは位相もコマ数も計算しない。索引を引いて書くだけ。

    ★★ ここは**状態を持ってはいけない**。
      CFRU が挿入するコードには書き込める RAM（.bss）が無い。
      `static` な変数を置くと ROM 上に置かれ、書き込んでも効かず
      読むとゴミが返る。2026-09-15 に実際にこれでアニメが全部止まった。

    表と毎フレーム関数は tools/rom/build_hoenn_anim.py が
    assembly/hoenn/ と src/hoenn_anim_generated.c に生成する。
*/

#include "hoenn_tileset_anim.h"

#define AppendTilesetAnimToBuffer ((void (*)(const void *, void *, u32))                                    (0x0806F6A8 + 1))

#define BG_VRAM_TILES 0x06000000

// 生成した毎フレーム関数から、自分の表を渡して呼ばれる。
// ★ 状態を持たない。引数だけで完結させること。
void HoennRunSched(u16 timer, const struct HoennAnimSched *sched)
{
    u16 i, end;

    if (sched == NULL || timer >= sched->period)
        return;

    end = sched->index[timer + 1];

    for (i = sched->index[timer]; i < end; ++i)
    {
        const struct HoennAnimStep *s = &sched->steps[i];

        AppendTilesetAnimToBuffer(
            s->src,
            (void *)(BG_VRAM_TILES + s->destTile * 32),
            s->size);
    }
}
