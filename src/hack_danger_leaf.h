#pragma once

//あぶないはっぱ（src/hack_danger_leaf.c・assembly/hack_danger_leaf/danger_leaf_scripts.s）
bool8 HackDangerLeaf_SwitchIn(u8 bank);
void HackDangerLeaf_KeepConfused(u8 bank);
extern const u8 BattleScript_DangerLeafRet[];
extern const u8 BattleScript_DangerLeafEnd2[];
