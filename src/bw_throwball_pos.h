/* 生成物。手で編集しないこと: tools/bwplayer/build_bw_trainer.py が作る。
   BW の後ろ姿に描き込まれていたボールを消した跡の位置。
   64x64 の絵の中の座標で、スプライトの中心 (32,32) からのずれに直してある。
   ★ 素 FRLG の後ろ姿はボールを持たず、ゲームが別のスプライトで描く。
     こちらは「投げ始めてから離すまで」だけ、本物のボールをここに出す。 */
#pragma once

#define BW_HELD_BALL_MALE_DX (12)
#define BW_HELD_BALL_MALE_DY (-18)
#define BW_HELD_BALL_FEMALE_DX (7)
#define BW_HELD_BALL_FEMALE_DY (-9)
