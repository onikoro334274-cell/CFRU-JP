/* 生成物。tools/bwplayer/build_bw_trainer.py が出す。手で編集しないこと。 */
#pragma once

#define BW_CARD_PIC_W 64
#define BW_CARD_PIC_H 80
#define BW_CARD_PIC_MALE_ID 135
#define BW_CARD_PIC_FEMALE_ID 136

extern const u8 gBwCardPicMale[];
extern const u8 gBwCardPicFemale[];

#define BW_HOF_PIC_TOP_SIZE 0x800
#define BW_HOF_PIC_BOTTOM_SIZE 0x400
extern const u8 gBwHofPicMaleTop[];
extern const u8 gBwHofPicMaleBottom[];
extern const u8 gBwHofPicFemaleTop[];
extern const u8 gBwHofPicFemaleBottom[];
