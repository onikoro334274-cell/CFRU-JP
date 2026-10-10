#pragma once

//ボックスの仮想化（src/hack_box_shelf.c・docs/research/2026-10-06_box-virtualization-design.md）
s32 BoxShelf_ShelfIndex(u8 boxId);
struct CompressedPokemon* BoxShelf_BoxMons(u8 boxId, bool8 forWrite);
struct CompressedPokemon* BoxShelf_PeekBoxMons(u8 boxId);   //読み込まずに見る（#70）
void BoxShelf_ReadMon(u8 boxId, u8 pos, struct CompressedPokemon* out);
void BoxShelf_MarkPtrDirty(const void* ptr);
u32 BoxShelf_DirtyPageCount(void);
bool8 BoxShelf_CanTouchBox(u8 boxId);
void BoxShelf_NewGame(void);
void BoxShelf_OnLoad(void);
void BoxShelf_Commit(void);
void BoxShelf_AfterSave(bool8 ok);
