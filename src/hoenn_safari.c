#include "defines.h"
#include "map_region.h"

/*
hoenn_safari.c
    ホウエンのサファリゾーン（2026-10-04・ユーザー「ホウエンのサファリゾーンも実装して」）。
    入口と中の地図はエメラルドのスクリプトを地図まるごと写す（tools/rom/build_hoenn_scripts.PUZZLE_MAPS）。サファリの仕組み（歩数・ボール・
    戦い）は素 FRLG の EnterSafariMode をそのまま使う（600 歩・30 個・素はエメラルドの 500 歩より多い）。
    終わったときの戻り先だけ、素のセキチクのゲートの代わりに 121 ばんどうろのゲートへ（assembly/hoenn_safari/safari_exit.s）。
*/

/* callasm: LASTRESULT = 今ホウエンの地図にいるか */
void HoennSafari_InHoenn(void)
{
	gSpecialVar_LastResult = (GetCurrentMapRegion() == MAP_REGION_HOENN);
}
