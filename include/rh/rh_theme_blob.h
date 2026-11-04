// Romhack: the AMOLED UI theme's code, split in two (2026-11-04):
// - src/rh_theme.c (overlay 129, always loaded, no room to grow): the hooks on the game's loaders, the palette
//   buffer tracking, and the loader of the rest;
// - rhtheme/*.c: the rest, linked on its own (rhtheme/linker.ld) into the ROM file data/rh_theme.bin
//   (tools/rh/mk_theme_blob.py) and loaded onto the system heap only while the theme is AMOLED (Default and
//   Randomizer never load it: the game runs exactly as before). It is relocated where it lands: its words that hold
//   its own addresses are listed in the file.
#ifndef RH_THEME_BLOB_H
#define RH_THEME_BLOB_H

#include "../types.h"

#define RH_THEME_DEFAULT    0
#define RH_THEME_RANDOMIZER 1
#define RH_THEME_AMOLED     2
#define RH_THEME_IDS        3   // test builds only: every colour becomes its id (palette slot, index)

#define RHTB_MAGIC 0x42544852   // "RHTB"
#define RHTB_BASE  0x0A000000   // the address rhtheme/ is linked at (nothing of the game's lives there)

// the start of data/rh_theme.bin's code: what the resident part calls
struct RhThemeBlob
{
    u32 magic;
    void (*palette)(u32 narcId, s32 fileId, u8 *data, u32 fileSize);                       // a whole palette file read
    void (*read)(u32 narcId, s32 fileId, u32 offset, u32 size, u8 *dst, BOOL whole, u32 ctx); // part of one
    void (*oam)(u32 offset, u32 size);   // after the game copied sprites to OAM (main screen: offset < 0x400)
};

// the file: { magic, image size, memory size (image + zeroed rest), relocation count }, the image, then the
// relocations (offsets in the image of words to add the load address to, less RHTB_BASE)
struct RhThemeBlobFile { u32 magic, imageSize, memSize, relocCount; };

// ---- the resident part's services (src/rh_theme.c) ----
#define PG_MAX_BUFS 24
struct RhPalBuf { u32 *unfaded, *faded, size, id; u8 *pd; };
extern struct RhPalBuf gRhBufs[PG_MAX_BUFS];   // the game's palette buffers (PaletteData), tracked
BOOL LONG_CALL RH_BufLive(u32 i);
u32 LONG_CALL GetBuf(u32 idx, BOOL faded);
void LONG_CALL SetIndex(u32 idx, u32 c);
void * LONG_CALL RH_ThemeSafeAlloc(u32 size);

#endif
