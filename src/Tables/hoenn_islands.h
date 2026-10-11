/* さいはてのことう・みなみのことうの番号。tools/rom/build_hoenn_islands.py が生成。手で編集しないこと。*/
#pragma once

#define HOENN_FARAWAY_INTERIOR_GROUP 69   /* FarawayIsland_Interior（エメラルドの群 + 43）*/
#define HOENN_FARAWAY_INTERIOR_NUM   57
#define HOENN_FARAWAY_MEW_LOCALID    1   /* build_hoenn.EXTRA_NPCS の並び */
#define HOENN_FLAG_MEW_GONE          0x13DE   /* build_hoenn_scripts.RESERVED_FLAGS（捕まえた・倒した）*/
#define HOENN_FLAG_HIDE_MEW          0x1E   /* 一時フラグ */
#define VAR_MEW_STEPS                0x4004   /* 一時変数（エメラルドの VAR_FARAWAY_ISLAND_STEP_COUNTER）*/
#define VAR_MEW_GRASS                0x4005
#define VAR_MEW_PEND                 0x4006
#define VAR_MEW_SHAKE                0x4007
