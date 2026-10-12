#pragma once

/* ボックスジャンプ（src/hack_box_jump.c・pm-start/box-jump の書き直し・#105・patches/0341）*/

const void* HackBoxJump_IconHint(void);   /* いまアイコンを決めている子の縮めた 58 バイト（HackMonId_MoemonKey から・無ければ NULL）*/
void HackBoxJump_Create(u8 curBox);
void HackBoxJump_Destroy(void);
void HackBoxJump_Render(void);
u8 HackBoxJump_Input(void);
