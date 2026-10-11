#include "defines.h"

/*
    ボックスの壁紙「もも」（2026-10-09 ユーザー「これボックス壁紙にできる？」→「新しく足す（17 枚目）」「ポケモンの枠に合わせる 6×5」）。
    資料: docs/research/2026-10-09_box-wallpaper-peach.md

    素の AddWallpapersMenu（0808F798・functionrewrites で置き換え）と同じに 4 つずつ並べ、「そのほか」（3 組目）だけ 5 つ目に もも を足す。
    メニューは選んだ文の番号を返し、素の Task_HandleWallpapers が「番号 - 0x16（もり）」を壁紙の番号にする。
    CFRU のメニューの文の表（assembly/hooks/pc_select_hooks.s の pcSelect_newBrmTable）は 0x26 が「Select」なので、ももは 0x27 ＝ 壁紙 17。
    壁紙の絵と表は assembly/hack_boxwall/box_wallpapers.s（tools/boxwall/build_box_wallpaper.py の生成物・repoints で素の表を差し替え）。
*/

#define MENU_TEXT_FOREST 0x16
#define MENU_TEXT_PEACH 0x27
#define WALLPAPER_SET_ETCETERA 3

#define InitMenu_ ((void (*)(void)) (0x080947DC | 1))
#define SetMenuText_ ((void (*)(u8)) (0x08094820 | 1))
#define AddMenu_ ((void (*)(void)) (0x080948BC | 1))

void HackBoxWall_AddWallpapersMenu(u8 wallpaperSet)
{
	u32 i;

	InitMenu_();
	if (wallpaperSet <= WALLPAPER_SET_ETCETERA)
	{
		for (i = 0; i < 4; ++i)
			SetMenuText_(MENU_TEXT_FOREST + wallpaperSet * 4 + i);
		if (wallpaperSet == WALLPAPER_SET_ETCETERA)
			SetMenuText_(MENU_TEXT_PEACH);
	}
	AddMenu_();
}
