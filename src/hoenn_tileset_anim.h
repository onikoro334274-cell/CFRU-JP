#ifndef GUARD_HOENN_TILESET_ANIM_H
#define GUARD_HOENN_TILESET_ANIM_H

/*
    ホウエンのタイルセット・アニメ（やりたいこと #13）。
    詳細は docs/research/2026-09-15_tileset-animation.md

    ★ ここに static なデータを置かないこと。
      CFRU の挿入コードには書き込める RAM が無く、
      static 変数は ROM 上に置かれて書き込みが効かない。

    ★ 2026-09-15 に表の形を変えた。
      それまでは「位相・コマ数・ずらし量」でエメラルドの計算を
      真似ていたが、元のコードの読み取りを誤ると静かに壊れる
      （実際に 49 本中 33 本を誤読し、海流が止まり、
      キンセツの自転車の看板に無関係な絵が描かれた）。

      いまは **エメラルドのコードを実行して記録した「timer ごとの
      書き込み一覧」** をそのまま持つ。実行時の計算は索引引きだけ。
*/

// 素 FRLG のアニメ用 RAM（BPRJ で実測）
#define sHoennPrimaryCounter    (*(vu16 *) 0x03000FAE)
#define sHoennPrimaryPeriod     (*(vu16 *) 0x03000FB0)
#define sHoennPrimaryCallback   (*(void (**)(u16)) 0x03000FB8)
#define sHoennSecondaryCounter  (*(vu16 *) 0x03000FB2)
#define sHoennSecondaryPeriod   (*(vu16 *) 0x03000FB4)
#define sHoennSecondaryCallback (*(void (**)(u16)) 0x03000FBC)

struct HoennAnimStep
{
    const void *src;        // コマの絵
    u16 timer;              // カウンタがこの値のときに書く
    u16 destTile;           // 書き込み先のタイル番号（詰め直し後）
    u16 size;               // バイト数（32 の倍数 = 連続したタイル枚数）
    u16 pad;
};

struct HoennAnimSched
{
    u16 period;             // カウンタの周回数
    u16 count;              // steps の本数
    const struct HoennAnimStep *steps;  // timer の昇順
    const u16 *index;       // index[t] 〜 index[t + 1] が timer t の分
};

void HoennRunSched(u16 timer, const struct HoennAnimSched *sched);

#endif // GUARD_HOENN_TILESET_ANIM_H
