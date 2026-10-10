#ifndef GUARD_FCOMP_H
#define GUARD_FCOMP_H

/* Recovered from tools/bwfollow/fcomp.py: ftable_lines emits three arrays
 * of 16 halfwords, in first/count/index order. */
struct BwFTable
{
	u16 first[16];
	u16 count[16];
	u16 index[16];
};

/* The C callers populate this descriptor; no serialized representation. */
struct FCompSet
{
	const u8 *dict;
	u32 dictSize;
	const struct BwFTable *litTable;
	const u16 *litSymbols;
	const u16 *litFast;
	const struct BwFTable *distTable;
	const u16 *distSymbols;
	const u16 *distFast;
	const u8 *pixT1;
	const u8 *pixT2;
};

/* Stream header: Python struct '<HHBxH', followed by 32-bit bitstream words.
 * Byte loads also support headers without halfword alignment. */
#define FCOMP_READ_U16(data, offset) ((u32) (data)[offset] | ((u32) (data)[(offset) + 1] << 8))
#define FCOMP_NUM_TILES(data) FCOMP_READ_U16(data, 0)
#define FCOMP_NUM_UNIQ(data) FCOMP_READ_U16(data, 2)
#define FCOMP_WIDTH(data) ((data)[4])
#define FCOMP_VERSION(data) ((data)[5])
#define FCOMP_NUM_WORDS(data) FCOMP_READ_U16(data, 6)

bool8 FCompInflate(const struct FCompSet *set, const u8 *data, u8 *out, u32 outSize);

#endif
