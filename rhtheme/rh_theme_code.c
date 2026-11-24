// Romhack: the AMOLED UI theme, the part loaded only while the theme is AMOLED (see include/rh/rh_theme_blob.h;
// the always-loaded hooks are in src/rh_theme.c). What it does: see src/rh_theme.c's top comment.
#include "../include/types.h"
#include "../include/rh/rh_coretable.h"
#include "../include/rh/rh_test.h"
#include "../include/rh/rh_theme_blob.h"

#define BufLive RH_BufLive
// (scripts/generate_ld.py exports every overlay 129 symbol as Thumb code, address | 1: data too. The compiler would
// take the mask off, sure an array's address is even: the address goes through an empty asm)
static inline struct RhPalBuf *ResidentData(const void *sym)
{
    u32 a = (u32)sym;
    __asm__("" : "+r"(a));
    return (struct RhPalBuf *)(a & ~1u);
}
#define sBufs ResidentData(gRhBufs)
#define SafeAlloc RH_ThemeSafeAlloc

// ---- game functions (arm9) ----
#define Heap_Alloc            ((void *(*)(u32, u32))(0x0201AA8C | 1))
#define Heap_AllocAtEnd       ((void *(*)(u32, u32))(0x0201AACC | 1))
#define Heap_Free             ((void (*)(void *))(0x0201AB0C | 1))
#define GetNarcMemberSize     ((u32 (*)(u32, s32))(0x020075BC | 1))
#define ReadWholeNarcMember   ((void (*)(void *, u32, s32))(0x02007508 | 1))   // hooked: RH_ReadWholeNarcMemberByIdPair
#define NARC_GetMemberSize    ((u32 (*)(void *, s32))(0x020077E8 | 1))
#define ReadFromNarcInternal  ((void (*)(void *, const char *, s32, u32, u32))(0x0200732C | 1))
#define AllocReadFromNarcInternal ((void *(*)(const char *, s32, u32, u32, u32, BOOL))(0x0200740C | 1))
#define NARC_ReadWholeMember  ((void (*)(void *, s32, void *))(0x0200778C | 1))
#define MI_UncompressLZ8      ((void (*)(const void *, void *))0x020D4BE8)          // ARM
#define FS_InitFile           ((void (*)(void *))0x020D7F98)                        // ARM
#define FS_OpenFile           ((BOOL (*)(void *, const char *))0x020D8278)          // ARM
#define FS_CloseFile          ((BOOL (*)(void *))0x020D82C0)                        // ARM
#define FS_ReadFile           ((s32 (*)(void *, void *, s32))0x020D83CC)            // ARM
#define FS_SeekFile           ((BOOL (*)(void *, s32, u32))0x020D83DC)              // ARM
#define sNarcFileList         ((const char *const *)0x0210F210)

#define NARC_SIZE    0x54
#define FSFILE_SIZE  0x48
#define NARC_ID(n)   (*(u16 *)((u8 *)(n) + 0x52))     // romhack: spare struct bytes
#define NARC_UNKNOWN 0xFFFF

// Pokemon / item / title art: never themed, not counted for the screen contexts below
static const u16 sArt[] = { 4, 114, 194, 195, 18, 20, 69, 46, 174, 264, 81 };

static BOOL IsArt(u32 narcId)
{
    u32 i;
    if (narcId == NARC_UNKNOWN)
        return TRUE;
    for (i = 0; i < NELEMS(sArt); i++)
        if (narcId == sArt[i])
            return TRUE;
    return FALSE;
}

// Screen contexts. The owner coloured the same palette entry differently on different screens (white text on one,
// a black panel on another), so a palette can have one version per screen. The game doesn't say which screen is
// loading, but every screen loads its palettes in the same order: the palette read just before identifies it.
// sPrevious = a hash of the last themed palette read (whole non-art files, and partial reads of table files).
static u16 sPrevious;

static u32 NextContext(u32 narcId, s32 fileId)
{
    u32 ctx = sPrevious;
    sPrevious = (u16)(narcId * 0x9E37 + (u32)fileId * 0x79B9);
    return ctx;
}

#if RH_TEST
// Colour-id mode (test only; tools/rh/amoled reads it): each palette read takes slots of 256 colours, colour =
// slot << 8 | index, and the log says which archive / file / part of 256 / screen context each slot is. The capture
// tools match screenshots taken this way with the owner's edited ones.
// (The log lives on the system heap, allocated on first use: overlay 129 has no room for it. The tools find it by
// its two magic words, "RHPD" and its complement.)
struct RhPalLog { u32 magic, magic2, count; u16 narc[64], file[64], ctx[64]; };
static struct RhPalLog *sLog;

// colours [first, first + n) of a palette file
static void IdColours(u32 narcId, s32 fileId, u32 first, u16 *colors, u32 n, u32 ctx)
{
    u32 i, slot = 0;
    if (sLog == NULL)
    {
        sLog = Heap_Alloc(0, sizeof(struct RhPalLog));
        if (sLog == NULL)
            return;
        sLog->magic = 0x52485044;
        sLog->magic2 = ~sLog->magic;
        sLog->count = 0;
    }
    for (i = 0; i < n; i++)
    {
        if (i == 0 || ((first + i) & 255) == 0)
        {
            slot = sLog->count++ & 63;
            sLog->narc[slot] = (u16)narcId;
            sLog->file[slot] = (u16)(fileId | ((first + i) >> 8) << 12);
            sLog->ctx[slot] = (u16)ctx;
        }
        colors[i] = (u16)(slot << 8 | ((first + i) & 255));
    }
}
#endif

// data/rh_amoled.bin (tools/rh/gen_rh_amoled.py): "RHA4", entry count, a bit mask of the archives in it (320 bits),
// then the entries { u32 narc << 16 | file, u32 screen context (0xFFFFFFFF: any screen), u32 offset of the colours,
// u32 colour count | where the colours start in the palette file << 16 } sorted. Every palette
// file of the UI archives is in it once for any screen, plus a version for each screen the owner coloured it
// differently on.
#define AM_MAGIC   0x34414852
#define AM_HEADER  48
#define AM_ANY     0xFFFFFFFF
static u32 sArcMask[10];
static u32 *sAmBlocks;      // the table's entries in blocks of AM_BLOCK: each block's first (key, context), read once
static u32 sAmCount;
static u32 *sAmCache;       // the block read last (a screen loads files of one archive one after another), sAmCacheB
static u32 *sAmFile;        // the table, open
#define AM_BLOCK 32

// Opens the table and looks (narc, file, context) up: the file position of its colours (the table stays open), or 0
// (closed). With the blocks' first keys in memory: one read of the block, one of the entry's header.
static u32 sAmCacheB = 0xFFFFFFFF;
static u32 AmoledFind(u32 *file, u32 narcId, s32 fileId, u32 ctx, u32 *start, u32 *count)
{
    u32 hdr[12], key = narcId << 16 | (u32)fileId, i, b, n, *e;
    if (narcId >= 320 || (sAmBlocks != NULL && !(sArcMask[narcId >> 5] & 1u << (narcId & 31))))
        return 0;
    if (sAmFile == NULL)                       // (opened once, kept open: opening by path costs a directory search)
    {
        if ((sAmFile = SafeAlloc(FSFILE_SIZE)) == NULL)
            return 0;
        FS_InitFile(sAmFile);
        if (!FS_OpenFile(sAmFile, "data/rh_amoled.bin"))
        {
            Heap_Free(sAmFile);
            sAmFile = NULL;
            return 0;
        }
    }
    file = sAmFile;
    if (sAmBlocks == NULL)
    {
        FS_SeekFile(file, 0, 0);
        if (FS_ReadFile(file, hdr, AM_HEADER) != AM_HEADER || hdr[0] != AM_MAGIC)
            goto none;
        for (i = 0; i < 10; i++)
            sArcMask[i] = hdr[2 + i];
        n = hdr[1];
        if ((sAmCache == NULL && (sAmCache = SafeAlloc(AM_BLOCK * 16)) == NULL)
            || (sAmBlocks = SafeAlloc((n + AM_BLOCK - 1) / AM_BLOCK * 8)) == NULL)
            goto none;
        for (b = 0; b * AM_BLOCK < n; b++)
        {
            FS_SeekFile(file, AM_HEADER + b * AM_BLOCK * 16, 0);
            FS_ReadFile(file, sAmBlocks + b * 2, 8);
        }
        sAmCount = n;
    }
    // the last block starting at or before (key, ctx); then the entry in it
    for (b = 0; (b + 1) * AM_BLOCK < sAmCount
         && (sAmBlocks[b * 2 + 2] < key || (sAmBlocks[b * 2 + 2] == key && sAmBlocks[b * 2 + 3] <= ctx)); b++)
        ;
    n = sAmCount - b * AM_BLOCK < AM_BLOCK ? sAmCount - b * AM_BLOCK : AM_BLOCK;
    if (b != sAmCacheB)
    {
        FS_SeekFile(file, AM_HEADER + b * AM_BLOCK * 16, 0);
        FS_ReadFile(file, sAmCache, n * 16);
        sAmCacheB = b;
    }
    for (i = 0, e = sAmCache; i < n; i++, e += 4)
        if (e[0] == key && e[1] == ctx)
        {
            *count = e[3] & 0xFFFF;
            *start = e[3] >> 16;
            return e[2];
        }
none:
    return 0;
}

// A read of bytes [offset, offset + size) of a palette file landed at dst: swap the colours in it for the theme's.
// whole: a whole file (its screen context was counted by the caller: ctx); else a partial read: some screens read
// only a few colours of a palette file, e.g. the battle move buttons' type colours (counted here when the file is a
// UI palette).
static void ThemeRead(u32 narcId, s32 fileId, u32 offset, u32 size, u8 *dst, BOOL whole, u32 ctx)
{
    u32 *file = NULL;
    u32 theme = RHCT_UiTheme(), pos, start, count, a, b;
    if (size == 0 || (theme != RH_THEME_AMOLED && theme != RH_THEME_IDS))
        return;
    pos = AmoledFind(file, narcId, fileId, AM_ANY, &start, &count);
    if (pos == 0)
        return;
    if (!whole)
        ctx = NextContext(narcId, fileId);
    a = offset > start ? offset : start;
    b = offset + size < start + count * 2 ? offset + size : start + count * 2;
#if RH_TEST
    if (theme == RH_THEME_IDS)
    {
        if (a < b && !((a - start) & 1))
            IdColours(narcId, fileId, (a - start) / 2, (u16 *)(dst + a - offset), (b - a) / 2, ctx);
        return;
    }
#endif
    // this screen's version of the palette, if the owner coloured it differently here (else the one found above)
    {
        u32 s2, c2, p2 = AmoledFind(file, narcId, fileId, ctx, &s2, &c2);
        if (p2 != 0)
            pos = p2;
    }
    if (a < b && !((a - start) & 1))
    {
        file = sAmFile;
        FS_SeekFile(file, pos + a - start, 0);
        FS_ReadFile(file, dst + a - offset, (b - a) & ~1u);
    }
}

// Colours some screens write by code (not from a palette file): constants in their overlays' literal pools, swapped
// in RAM while the overlay is loaded (each palette load checks; the code next to the constant must be the expected
// one, so another overlay at the same address is never touched). The overlay comes back from the ROM unchanged.
struct CodeColour { u32 addr; u16 vanilla, amoled; u32 guard; u16 guardValue; };
static const struct CodeColour sCodeColours[] = {
    // main menu (ov74): window fill (palette 1 colour 15) and the bottom screen's backdrop
    { 0x02227ACC, 0x6B5A, 0x0000, 0x02227822, 0x8001 },
    { 0x02227CB8, 0x6B5A, 0x0000, 0x02227C6A, 0x87D0 },
    { 0x02228B84, 0x7D8C, 0x0000, 0x02228A6E, 0x8001 },
    { 0x0222AC18, 0x7D8C, 0x0000, 0x0222ABC6, 0x8001 },
    // field menu, cursor mode (opened with X): G2S_SetBlendAlpha(0, 0x23, 6, 9) every frame dims the buttons through
    // their semi-transparent sprites; the owner's design has no dimming (the register patch lost the race every other
    // frame: the game sets it after the theme's tasks). An instruction: "movs r2, #0x23" -> "movs r2, #0" (plane 2
    // none), guarded by the next one ("movs r3, #6")
    { 0x0225A9A2, 0x2223, 0x2200, 0x0225A9A4, 0x2306 },
};
#define DC_FlushRange_      ((void (*)(void *, u32))0x020D2894)       // ARM
#define IC_InvalidateRange_ ((void (*)(void *, u32))0x020D28D0)       // ARM

static void ThemeCodeColours(void)
{
    u32 i;
    for (i = 0; i < NELEMS(sCodeColours); i++)
    {
        const struct CodeColour *c = &sCodeColours[i];
        if (*(u16 *)c->guard == c->guardValue && *(u16 *)c->addr == c->vanilla)
        {
            *(u16 *)c->addr = c->amoled;
            DC_FlushRange_((void *)(c->addr & ~31u), 32);        // (code: the CPU must fetch the new instruction)
            IC_InvalidateRange_((void *)(c->addr & ~31u), 32);
        }
    }
}

// Screen fingerprint: some screens show a palette that another screen loaded (Pokedex pages, battle menus, bag
// pockets...), and the owner coloured it differently on each. A hash of the background tile maps of both screens
// says which one is showing; it is worked out every frame (AMOLED only) by a task on the main queue.
// the "vwait" queue: run by the main loop right after the screen's vertical-blank callback (which copies the game's
// palettes, tile maps and sprites to the hardware), so the theme's changes are in before the frame is drawn
#define SysTask_CreateOnVWaitQueue ((void *(*)(void (*)(void *, void *), void *, u32))(0x0200E374 | 1))
#define SysTask_CreateOnVBlankQueue ((void *(*)(void (*)(void *, void *), void *, u32))(0x0200E33C | 1))
struct RhScreenSig { u32 magic, sig, shown, patches, rows, dbg, calls, callsNow, bytes; };
struct RhScreenSig gRhScreenSig = { 0x47534852 /* "RHSG" */, 0, 0, 0, 0, 0, 0, 0, 0 };   // (dbg, test builds: bit 0 no ThemeTask, 1 no RegTask, 2 no ThemeOam)   // (the test tools read it)
static BOOL sTaskMade;
static u32 *sPatch;         // the shown screen's tile patches (see TilePatches), sorted by address
static u32 sPatchCount;
u32 sCmMap;                 // the battle Check Moves map re-pointed at its type rows (src/battle/rh_hptext.c), or 0

// Each map's quick checksum (as it is, theme's patches and all) and its part of the fingerprint: a map is fingerprinted
// the long way only when its checksum changes (they rarely do; every cell the long way each frame overran the
// vertical blank and slowed the game down)
static u32 sMapQuick[8], sMapHash[8];
// and its 6 region hashes (rows 0-7 / 8-15 / 16-23 x columns 0-15 / 16-31; palette bits left out, tile numbers as steps
// from the region's cell before): the shown screen's layer setup and regions, for FuzzyFind (tools/rh/amoled/sigpy.py)
static u16 sMapReg[8][6];
static u32 sCurCfg;
static u16 sCurReg[48];
static u32 ScreenSig(void)
{
    u32 h = 2166136261u, eng, bg, i, lo, prev, nl = 0;
    sCurCfg = 2166136261u;
    for (eng = 0; eng < 2; eng++)
    {
        u32 io = eng ? 0x04001000 : 0x04000000, disp = *(vu32 *)io;
        for (bg = 0; bg < 4; bg++)
        {
            u32 cnt, cm, q, hb;
            const vu16 *map;
            if (!(disp & (0x100u << bg)) || (bg == 0 && !eng && (disp & 8)))
                continue;           // (off, or the main engine's 3D layer: no tile map, what is there changes)
            cnt = *(vu16 *)(io + 8 + bg * 2);
            map = (const vu16 *)((eng ? 0x06200000 : 0x06000000 + ((disp >> 27) & 7) * 0x10000) + ((cnt >> 8) & 31) * 0x800);
            h = (h ^ cnt ^ bg << 16 ^ eng << 20) * 16777619u;
            sCurCfg = (sCurCfg ^ cnt ^ bg << 16 ^ eng << 20) * 16777619u;
            // (multiplied, not just rotated: rows moved by a few rows must change it too)
            for (i = 0, q = (u32)map ^ sCmMap; i < 0x600 / 4; i++)
                q = (q ^ ((const vu32 *)map)[i]) * 16777619u;
            if (q == sMapQuick[eng * 4 + bg])
            {
                h = (h ^ sMapHash[eng * 4 + bg]) * 16777619u;
                for (i = 0; i < 6 && nl < 8; i++)
                    sCurReg[nl * 6 + i] = sMapReg[eng * 4 + bg][i];
                nl++;
                continue;
            }
            // tile patches on the map (palette numbers / tile numbers of cells, "map patches"): hashed as the game
            // drew them. The patches are sorted by address like the map: found once, then walked along with it
            {
                u32 a = (u32)map - 0x06000000, k, hi = sPatchCount;
                lo = 0;
                while (lo < hi)
                {
                    k = (lo + hi) / 2;
                    if ((sPatch[k] & 0x7FFFFF) < a)
                        lo = k + 1;
                    else
                        hi = k;
                }
            }
            // tile numbers as steps from the cell before's (cells without a tile skipped): text windows get their
            // tiles where the game finds room (what was open before decides), the same screen must give the same
            // fingerprint
            cm = (u32)map == sCmMap;
            prev = 0;
            hb = 2166136261u;
            u32 hr[6] = { 2166136261u, 2166136261u, 2166136261u, 2166136261u, 2166136261u, 2166136261u };
            u16 pr[6] = { 0, 0, 0, 0, 0, 0 };
            for (i = 0; i < 0x600 / 2; i++)
            {
                u32 e = map[i], t;
                if (lo < sPatchCount && (sPatch[lo] & 0x7FFFFF) < (u32)&map[i] + 2 - 0x06000000)
                {
                    u32 a = (u32)&map[i] - 0x06000000;
                    for (; lo < sPatchCount && (sPatch[lo] & 0x7FFFFF) < a; lo++)
                        ;
                    for (; lo < sPatchCount && (sPatch[lo] & 0x7FFFFF) < a + 2; lo++)
                    {
                        u32 p = sPatch[lo], sh = (p & 1) * 8 + (p >> 23 & 1) * 4;
                        if ((e >> sh & 15) == (p >> 28 & 15))
                            e = (e & ~(15u << sh)) | (p >> 24 & 15) << sh;
                    }
                }
                {
                    u32 r = (i >> 8) * 2 + ((i & 31) >= 16), f = e & 0x0FFF, ft = f & 0x3FF;
                    if (ft != 0)
                    {
                        f = (f & ~0x3FFu) | ((ft - pr[r]) & 0x3FF);
                        pr[r] = (u16)ft;
                    }
                    hr[r] = (hr[r] ^ f) * 16777619u;
                }
                if (cm && i >= 12 * 16 && i < 36 * 16 && ((e >> 12) == 7 || (e >> 12) == 8 || (e >> 12) == 10))
                    e = (e & 0x0FFF) | 12u << 12;      // (as the game drew it: palette 12)
                if ((t = e & 0x3FF) != 0)
                {
                    e = (e & ~0x3FFu) | ((t - prev) & 0x3FF);
                    prev = t;
                }
                hb = (hb ^ e) * 16777619u;
            }
            sMapQuick[eng * 4 + bg] = q;
            sMapHash[eng * 4 + bg] = hb;
            h = (h ^ hb) * 16777619u;
            for (i = 0; i < 6; i++)
            {
                sMapReg[eng * 4 + bg][i] = (u16)(hr[i] ^ hr[i] >> 16);
                if (nl < 8)
                    sCurReg[nl * 6 + i] = sMapReg[eng * 4 + bg][i];
            }
            nl++;
        }
    }
    for (i = nl * 6; i < 48; i++)
        sCurReg[i] = 0;
    return h;
}

#define PaletteData_SetBuffers ((void (*)(void *, u32, void *, void *, u32))(0x0200310C | 1))

#define PG_MAGIC   0x47504852
#define PG_MAX     96
struct PgRow { u32 c[8]; u32 target; };
static u32 sLastSig;

static void ReplaceRows(u32 *w, u32 *end, struct PgRow *row, u32 n, u32 bloom)
{
    u32 i, j;
    for (; w + 8 <= end; w++)
    {
        if (!(bloom & 1u << (w[0] & 31)))
            continue;
        for (i = 0; i < n; i++)
        {
            if (w[0] != row[i].c[0] || row[i].target == i)
                continue;
            for (j = 1; j < 8 && w[j] == row[i].c[j]; j++)
                ;
            if (j == 8)
            {
                for (j = 0; j < 8; j++)
                    w[j] = row[row[i].target].c[j];
                w += 7;
                break;
            }
        }
    }
}

// Tile patches: where the owner gave pixels that share a palette entry different colours on one screen, those pixels'
// tile data (background or sprite VRAM, 4bpp) are pointed at a spare entry of the same palette row, which the
// screen's palette version colours. Each patch: VRAM offset from 0x06000000 (bits 0-22), which half of the byte (23),
// the entry it had (24-27), the spare one (28-31). Re-applied every frame while the screen shows (the game may redraw), undone
// when it changes.
static u16 *sPatchVal;       // each patched VRAM word as the theme left it (after sPatch's items), for the undo
static void TilePatches(BOOL undo)
{
    u32 i, lastA = 0, ok = 0;
    for (i = 0; i < sPatchCount; i++)
    {
        u32 e = sPatch[i], a = 0x06000000 + (e & 0x7FFFFF), sh = (a & 1) * 8 + (e >> 23 & 1) * 4;
        vu16 *b = (vu16 *)(a & ~1u);             // (VRAM takes no byte writes)
        u32 from = undo ? e >> 28 & 15 : e >> 24 & 15, to = undo ? e >> 24 & 15 : e >> 28 & 15;
        // undo only words still as the theme left them: the game may already have drawn the next screen there, and
        // a value that happens to equal a patched one must not be "undone" (that broke the next screen's maps)
        if (undo)
        {
            if ((a & ~1u) != lastA)
            {
                lastA = a & ~1u;
                ok = sPatchVal == NULL || *b == sPatchVal[i];
            }
            if (!ok)
                continue;
        }
        if ((*b >> sh & 15) == from)
            *b = (u16)((*b & ~(15u << sh)) | to << sh);
    }
    if (!undo && sPatchVal != NULL)
        for (i = 0; i < sPatchCount; i++)
            sPatchVal[i] = *(vu16 *)((0x06000000 + (sPatch[i] & 0x7FFFFF)) & ~1u);
}

// Palette index patches: on a screen, colour c for palette RAM entry idx (0-1023: main BG, main OBJ, sub BG, sub
// OBJ), in palette RAM and the game's buffers for that palette (PaletteData ids 0 main BG, 1 sub BG, 2 main OBJ,
// 3 sub OBJ): what the owner's picture needs there that the palette files can't give (colours set by code, rows
// another screen loaded, colours under the hardware's dimming / blending, worked back). Set once when the screen
// shows, so fades still work on them.
static struct PgRow *sRows;  // the shown screen's row versions (pairs), kept: re-applied every frame, since the game
static u32 sRowN, sBloom, sRowFrames;   // may load the screen's palettes after its tile maps (= after the screen counts as shown)

static void ApplyRows(void)
{
    u32 i;
    if (sRows == NULL || sRowFrames == 0)      // (the first second of a screen: the late loads)
        return;
    sRowFrames--;
    ReplaceRows((u32 *)0x05000000, (u32 *)0x05000800, sRows, sRowN, sBloom);   // palette RAM, both screens
    for (i = 0; i < PG_MAX_BUFS; i++)                                         // the game's palette buffers
        if (BufLive(i))
        {
            ReplaceRows(sBufs[i].unfaded, sBufs[i].unfaded + sBufs[i].size / 4, sRows, sRowN, sBloom);
            ReplaceRows(sBufs[i].faded, sBufs[i].faded + sBufs[i].size / 4, sRows, sRowN, sBloom);
        }
}

static u32 *sIndexUndo;     // (idx << 16 | colour it had) for the shown screen's index patches, then (idx << 16 |
                            // colour wanted): re-set every frame where the game's own colour came back (late loads),
                            // then the colour palette RAM showed
static u32 sIndexCount;
#define IDX_MARK 0x8000     // bit 15 (unused by the hardware) on the colours the index patches write
// an index patch's colour: its own, or (bit 31: a link) the colour another entry has now (palette RAM index in bits
// 0-9: e.g. each bag pocket's colour, from the pocket's palette row the game loads)
#define IdxWant(w) (((w) >> 31 ? ((vu16 *)0x05000000)[(w) & 1023] & 0x7FFF : (w) & 0x7FFF) | IDX_MARK)
static u32 *sRegs;         // register patches of the shown screen: (offset from 0x04000000 << 16 | value)
static u32 sRegCount;

#define GetIndex(idx) GetBuf(idx, FALSE)


// SetIndex for a colour the game put back in its unfaded buffer (a reload): the faded copy and palette RAM only where no
// fade is under way for the entry (faded = the unfaded colour it had), else the fade goes on from the game's colours
// (the battle's exit fade: the message text would light up again)
static void SetIndexKeepFade(u32 idx, u32 want, u32 old)
{
    static const u8 sBufFor[4] = { 0, 2, 1, 3 };
    u32 i, id = sBufFor[idx >> 8], off = idx & 255, found = 0;
    for (i = 0; i < PG_MAX_BUFS; i++)
        if (BufLive(i) && sBufs[i].id == id && off * 2 < sBufs[i].size)
        {
            u16 *fd = (u16 *)sBufs[i].faded;
            ((u16 *)sBufs[i].unfaded)[off] = (u16)want;
            if (fd[off] == (u16)old)
            {
                fd[off] = (u16)want;
                ((vu16 *)0x05000000)[idx] = (u16)want;
            }
            found = 1;
        }
    if (!found)
        ((vu16 *)0x05000000)[idx] = (u16)want;
}

static u32 *sSigs;         // the file's screen list (fingerprint, offset) pairs, read once (system heap)
static u32 sSigCount, sRowsAt, sFuzzyAt;

// The screen list's entry for a fingerprint: the offset of its page, 0 (none), or bit 31 + a palette RAM index: the
// screen comes in versions told apart by that entry's colour (the bag's pockets, a pressed button: the same tile
// maps), each with a page of its own under the fingerprint mixed with the colour (tools/rh/amoled/learn.py)
static u32 FindPage(u32 sig)
{
    u32 file[FSFILE_SIZE / 4], hdr[4], lo, hi, mid;
    if (sSigs == NULL)
    {
        FS_InitFile(file);
        if (!FS_OpenFile(file, "data/rh_amoled_pages.bin"))
            return 0;
        if (FS_ReadFile(file, hdr, 16) == 16 && hdr[0] == PG_MAGIC && hdr[1] != 0)
        {
            sSigs = SafeAlloc(hdr[1] * 8);
            if (sSigs == NULL)
            {
                FS_CloseFile(file);
                return 0;          // (no room now: try again on the next screen)
            }
            FS_ReadFile(file, sSigs, hdr[1] * 8);
            sSigCount = hdr[1];
            sRowsAt = 16 + hdr[1] * 8;
            sFuzzyAt = hdr[3];
        }
        FS_CloseFile(file);
        if (sSigs == NULL)
        {
            sSigs = (u32 *)1;      // (no list in the ROM: never look again)
            sSigCount = 0;
        }
    }
    lo = 0; hi = sSigCount;
    while (lo < hi)
    {
        mid = (lo + hi) / 2;
        if (sSigs[mid * 2] == sig)
            return sSigs[mid * 2 + 1];
        if (sSigs[mid * 2] < sig)
            lo = mid + 1;
        else
            hi = mid;
    }
    return 0;
}

static u32 *sPgFile;        // the pages file, open
static u32 *PgFile(void)
{
    if (sPgFile == NULL)                       // (opened once, kept open: opening by path costs a directory search,
    {                                          // and a new screen's colours should be in before the frame is drawn)
        if ((sPgFile = SafeAlloc(FSFILE_SIZE)) == NULL)
            return NULL;
        FS_InitFile(sPgFile);
        if (!FS_OpenFile(sPgFile, "data/rh_amoled_pages.bin"))
        {
            Heap_Free(sPgFile);
            sPgFile = NULL;
        }
    }
    return sPgFile;
}

// A screen whose exact fingerprint has no page (the owner's screen in another state: a 4th move, more items in the
// bag, another Pokemon...): the learned screen with the same layer setup whose map regions match best (at least 3 of
// 4 of the regions either shows something in, and no other as good). Its page then goes on, tile patches too (they
// are on the screen's ROM graphics only, at the same place in every state of it, and check each pixel first). The list (tools/rh/gen_rh_amoled.py): { u32 count, count x { u32 fingerprint,
// u32 layer setup, u16 regions[48] } }, read once.
#define FZ_EMPTY 0x9070            // a region without tiles
#define FZ_SIZE  (8 + 96)
#define FZ_CACHE 16                // results kept: a screen's animated maps give fingerprints that come back
static u32 sFzSig[FZ_CACHE], sFzRes[FZ_CACHE], sFzNext;
static u8 *sFzTable;
static u32 sFzCount;
static u32 FuzzyScore(const u8 *e, u32 *best, u32 *bm, u32 *tie)
{
    const u16 *r = (const u16 *)(e + 8);
    u32 i, tot = 0, m = 0;
    if (*(const u32 *)(e + 4) != sCurCfg)
        return 0;
    for (i = 0; i < 48; i++)
    {
        if ((r[i] == FZ_EMPTY || r[i] == 0) && (sCurReg[i] == FZ_EMPTY || sCurReg[i] == 0))
            continue;
        tot++;
        m += r[i] == sCurReg[i];
    }
    if (tot == 0 || m * 4 < tot * 3)
        return 0;
    if (m > *bm)
    {
        *bm = m; *best = *(const u32 *)e; *tie = 0;
    }
    else if (m == *bm && *(const u32 *)e != *best)
        *tie = 1;                      // (states of one family leading to the same screen: no tie)
    return 0;
}

static u32 FuzzyFind(u32 sig)
{
    u32 k, n = 0, best = 0, bm = 0, tie = 0, *file;
    for (k = 0; k < FZ_CACHE; k++)
        if (sFzSig[k] == sig && sig != 0)
            return sFzRes[k];
    if (sFzTable == NULL)              // (the table in memory, read once when the system heap has room: read from the
    {                                  // card each time, a battle's animated maps made it lag; no room: tried again
        static u32 sRetry;             // a while later, not on every fingerprint)
        if ((sRetry++ & 63) != 0 || sFuzzyAt == 0 || (file = PgFile()) == NULL)
            return 0;
        FS_SeekFile(file, sFuzzyAt, 0);
        if (FS_ReadFile(file, &n, 4) != 4 || n == 0 || n > 512 || (sFzTable = SafeAlloc(n * FZ_SIZE)) == NULL)
            return 0;
        FS_ReadFile(file, sFzTable, n * FZ_SIZE);
        sFzCount = n;
    }
    for (k = 0; k < sFzCount; k++)
        FuzzyScore(sFzTable + k * FZ_SIZE, &best, &bm, &tie);
    k = sFzNext++ % FZ_CACHE;
    sFzSig[k] = sig;
    sFzRes[k] = tie ? 0 : best;
    return sFzRes[k];
}

// Row rules (tools/rh/amoled/learn.py): buttons with a palette row each, the selected one told apart by the game's
// colours (the field menu's cursor). Every frame each row of the rule: the game shows it unselected (its key entry has
// the unselected colour) -> the owner's colours (what the game had is kept); the game changed it (selected) -> the
// game's colours back where the owner's still are. The theme's colours carry IDX_MARK (bit 15), the game's don't.
struct RowRule { u32 w[2]; u16 col[16]; u16 save[256]; };
static struct RowRule *sRules;
static u32 sRuleCount;
static void RowRules(BOOL undo)
{
    u32 n, j, e, k;
    for (n = 0; n < sRuleCount; n++)
    {
        struct RowRule *r = &sRules[n];
        u32 blk = r->w[0] >> 16 & 3, key = r->w[0] >> 24 & 15, kc = r->w[1] & 0x7FFF, em = r->w[1] >> 16;
        for (j = 0; j < 16; j++)
        {
            u32 base = blk * 256 + j * 16, kv;
            if (!(r->w[0] >> j & 1))
                continue;
            kv = GetIndex(base + key);
            if (!undo && (kv & IDX_MARK))
                continue;                       // (ours still: unselected)
            for (e = 0, k = 0; e < 16; e++)
            {
                if (!(em >> e & 1))
                    continue;
                if (!undo && (kv & 0x7FFF) == kc)
                {
                    r->save[j * 16 + e] = (u16)(GetIndex(base + e) & 0x7FFF);
                    SetIndex(base + e, r->col[k] | IDX_MARK);
                }
                else if (GetIndex(base + e) & IDX_MARK)
                    SetIndex(base + e, r->save[j * 16 + e]);
                k++;
            }
        }
    }
}

static BOOL sFuzzyShown, sFuzzyNext;    // the shown (next) page was found by FuzzyFind
struct ObjCopy { u32 w[2], mine; };   // (ObjCopies, below; mine: the copy's place was found empty, it is the theme's)
static struct ObjCopy *sObjCopies;
static u32 sObjEngMask;          // (bit per engine some entry is for)
static u16 sObjAct[64];          // (the entries the sprite pass acts on: ObjCopies)
static u32 sObjActCount;
static u16 sObjKey[64];          // (those matched by their sprite's tile, and the tiles: ObjCopies)
static u32 sObjKeyCount, sObjTileMask[32];
static u32 sObjCopyCount;
static void ObjCopyTiles(void);
static void ObjCopies(u32 offset, u32 size, u16 *obuf);
static u32 sFrame, sObjDone, sRuleFrame = 0xFFFFFFFF, sTilesFrame = 0xFFFFFFFF, sHookFrame = 0xFFFFFFFF, sHooked;       // (vertical blanks counted by RegTask; the frame ObjCopies last ran in)
static void ShowScreen(u32 sig)
{
    u32 *file, hdr[3], ent[2], mid, n = 0, i, bloom = 0, pair[2];
    struct PgRow *row;
    ent[1] = FindPage(sig);
    if (ent[1] == 0 || ent[1] >> 31)
        return;
    gRhScreenSig.shown = sig;
    if ((file = PgFile()) == NULL)
        return;
    FS_SeekFile(file, ent[1], 0);
    FS_ReadFile(file, &n, 4);
    mid = n;                         // (the real count, for the tile patches' offset)
    hdr[1] = (sRowsAt - 16) / 8;
    // its tile patches: after the pairs
    FS_SeekFile(file, ent[1] + 4 + mid * 8, 0);
    n = 0;
    if (FS_ReadFile(file, &n, 4) == 4 && n != 0 && n <= 8192)
    {
        sPatch = SafeAlloc(n * 6);
        if (sPatch != NULL)
        {
            FS_ReadFile(file, sPatch, n * 4);
            sPatchVal = (u16 *)(sPatch + n);
            sPatchCount = n;
        }
    }
    // its palette index patches: after the tile patches (what each entry had is kept, to undo them)
    FS_SeekFile(file, ent[1] + 4 + mid * 8 + 4 + n * 4, 0);
    pair[1] = ent[1] + 4 + mid * 8 + 4 + n * 4;
    if (FS_ReadFile(file, &n, 4) == 4 && n != 0 && n <= 1024)
    {
        sIndexUndo = SafeAlloc(n * 12);
        if (sIndexUndo != NULL)
        {
            FS_ReadFile(file, sIndexUndo + n, n * 4);
            for (i = 0; i < n; i++)
            {
                pair[0] = sIndexUndo[n + i];
                // (palette RAM gets the buffer's colour back too: palette RAM may not have it yet, the game's copy
                // into it still to come; a later load straight into palette RAM is seen in ThemeTask)
                sIndexUndo[i] = (pair[0] & 0x3FF0000) | (GetIndex(pair[0] >> 16 & 1023) & 0x7FFF);
                sIndexUndo[2 * n + i] = sIndexUndo[i] & 0x7FFF;
                // (palette RAM only for now: the row versions below find their rows in the game's buffers)
                ((vu16 *)0x05000000)[pair[0] >> 16 & 1023] = (u16)IdxWant(pair[0]);
            }
        }
        sIndexCount = sIndexUndo != NULL ? n : 0;
    }
    else if (n > 1024)
        n = 0;
    // its register patches: after the index patches
    pair[1] += 4 + n * 4;
    FS_SeekFile(file, pair[1], 0);
    if (FS_ReadFile(file, &n, 4) != 4 || n > 16)
        n = 0;
    if (n != 0)
    {
        sRegs = SafeAlloc(n * 4);
        if (sRegs != NULL)
        {
            FS_ReadFile(file, sRegs, n * 4);
            sRegCount = n;
        }
    }
    // its row rules: after the register patches; then its sprite copies (read in order: the rules' size varies)
    FS_SeekFile(file, pair[1] + 4 + n * 4, 0);
    i = FS_ReadFile(file, &n, 4) == 4 && n <= 4;
    if (i && n != 0 && (sRules = SafeAlloc(n * sizeof(struct RowRule))) == NULL)
        i = FALSE;
    pair[0] = i;                       // (the file is at the sprite copies)
    if (i && n != 0)
    {
        for (i = 0; i < n; i++)
        {
            struct RowRule *r = &sRules[i];
            u32 k, c = 0;
            FS_ReadFile(file, r->w, 8);
            for (k = 0; k < 16; k++)
                c += r->w[1] >> (16 + k) & 1;
            FS_ReadFile(file, r->col, (c + (c & 1)) * 2);
            for (k = 0; k < 256; k++)
                r->save[k] = 0;
        }
        sRuleCount = n;
    }
    bloom = 0xFFFFFFFF;                  // (the file's place after the sprite copies: the rows inline; unknown)
    if (pair[0] && FS_ReadFile(file, &n, 4) == 4)
        bloom = n;
    if (bloom != 0xFFFFFFFF && n != 0 && n <= 512  // (512: pixel patches, overlay tiles: ObjCopyTiles)
        && (sObjCopies = SafeAlloc(n * sizeof(struct ObjCopy))) != NULL)
    {
        // (one read for all: a read per entry - ~100 with pixel patches - cost frames when the screen came up)
        FS_ReadFile(file, sObjCopies, n * 8);
        for (i = n; i-- > 0;)                    // (spread out to 12-byte entries, from the end: no overlap)
        {
            u32 a = ((u32 *)sObjCopies)[2 * i], b = ((u32 *)sObjCopies)[2 * i + 1];
            sObjCopies[i].w[0] = a;
            sObjCopies[i].w[1] = b;
            sObjCopies[i].mine = 0;
        }
        sObjCopyCount = n;
        for (i = 0; i < 32; i++)
            sObjTileMask[i] = 0;
        for (i = 0, sObjEngMask = 0, sObjActCount = 0, sObjKeyCount = 0; i < n; i++)
        {
            u32 w0 = sObjCopies[i].w[0];
            sObjEngMask |= 1u << (w0 & 1);
            // (the entries the sprite pass acts on: not pixel patches, tile words or scanline colours; those matched by
            // their sprite's tile - palette moves of one tile, tile copies - apart: a slot is looked at only when its
            // tile is one of theirs)
            if ((w0 >> 12 & 1) || (w0 >> 15 & 1) || ((w0 >> 13 & 1) && !(w0 >> 14 & 1)))
                continue;
            if (!(w0 >> 14 & 1) && !(w0 >> 9 & 1) && !(w0 >> 11 & 1) && sObjKeyCount < 64)
            {
                sObjKey[sObjKeyCount++] = (u16)i;
                sObjTileMask[(w0 >> 16 & 1023) >> 5] |= 1u << (w0 >> 16 & 31);
            }
            else if (sObjActCount < 64)
                sObjAct[sObjActCount++] = (u16)i;
        }
    }
    // its row versions last (many reads: the index patches above are in palette RAM before the vertical blank ends,
    // a new screen has the owner's colours from its first frame), then the index patches again on top of them
    n = mid > PG_MAX / 2 ? PG_MAX / 2 : mid;
    row = n ? SafeAlloc(sizeof(struct PgRow) * n * 2) : NULL;
    if (row != NULL)
    {
        // each patch: the row it finds (any version) and the version it writes, as two list items; the rows are
        // inline after the sprite copies (one read: a seek and read per row cost frames when the screen came up)
        u32 at = bloom;
        bloom = 0;
        if (at != 0xFFFFFFFF)
        {
            if (sObjCopies == NULL)
                FS_SeekFile(file, at * 8, 1);    // (sprite copies not kept: past them)
            for (i = 0; i < n; i++)
            {
                FS_ReadFile(file, row[2 * i].c, 32);
                FS_ReadFile(file, row[2 * i + 1].c, 32);
            }
        }
        for (i = 0; i < n; i++)
        {
            if (at == 0xFFFFFFFF)                // (the old way: the pair's row numbers, the rows from the list)
            {
                FS_SeekFile(file, ent[1] + 4 + i * 8, 0);
                FS_ReadFile(file, pair, 8);
                FS_SeekFile(file, 16 + hdr[1] * 8 + pair[0] * 32, 0);
                FS_ReadFile(file, row[2 * i].c, 32);
                FS_SeekFile(file, 16 + hdr[1] * 8 + pair[1] * 32, 0);
                FS_ReadFile(file, row[2 * i + 1].c, 32);
            }
            row[2 * i].target = 2 * i + 1;
            row[2 * i + 1].target = 2 * i + 1;
            bloom |= 1u << (row[2 * i].c[0] & 31);
        }
        sRows = row; sRowN = n * 2; sBloom = bloom; sRowFrames = 60;
        ApplyRows();
    }
    for (i = 0; i < sIndexCount; i++)
        SetIndex(sIndexUndo[sIndexCount + i] >> 16 & 1023, IdxWant(sIndexUndo[sIndexCount + i]));

}

// The register patches again in every vertical blank (vblank queue, after the game's own tasks there): ThemeTask runs
// only when the game waits for the vertical blank (every other frame on 30 fps screens), and the game sets its
// registers every vertical blank (the field menu's dimming blend would come back every other frame)
// Scanline colours (sprite copy list entries with w0 bit 13, not 12/14/15): one palette entry given a colour per line
// by an H-blank DMA (channel 0, unused by the game), for a gradient the owner drew over one flat colour (the options
// menu's selected value box: one entry, its colour changing from line to line). Header: w0 = 1 << 13 | engine | layer
// << 1 | map column << 16 | first line in the cell row << 21, w1 = palette entry | box tile << 10 | its palette row <<
// 20 | colour count << 24; then colour pairs: w0 = 1 << 13 | 1 << 3 | colour << 16, w1 = next colour. Each vertical
// blank: the box's cell row is found in the layer's tile map (its first cell in that column with that tile and
// palette row), and the entry gets the owner's colours on the box's lines, its own (palette RAM, a fade's too) on the
// others; outside the theme's colour (a fade under way) all lines keep palette RAM's
static u16 *sLineTab;
static void Scanlines(void)
{
    u32 n, k;
    vu32 *dma = (vu32 *)0x040000B0;
    for (n = 0; n < sObjCopyCount; n++)
    {
        u32 w0 = sObjCopies[n].w[0], w1 = sObjCopies[n].w[1];
        if (!(w0 >> 13 & 1) || (w0 >> 12 & 1) || (w0 >> 14 & 3) || (w0 >> 3 & 1))
            continue;
        u32 eng = w0 & 1, layer = w0 >> 1 & 3, col = w0 >> 16 & 31, off = w0 >> 21 & 15;
        u32 idx = w1 & 1023, tile = w1 >> 10 & 1023, pal = w1 >> 20 & 15, cnt = w1 >> 24 & 63;
        u32 disp = *(vu32 *)(eng ? 0x04001000 : 0x04000000), bgc = *(vu16 *)((eng ? 0x04001008 : 0x04000008) + layer * 2);
        vu16 *map = (vu16 *)((eng ? 0x06200000 : 0x06000000 + ((disp >> 27) & 7) * 0x10000) + ((bgc >> 8) & 31) * 0x800);
        u16 now = (u16)(GetBuf(idx, TRUE) & 0x7FFF);  // (what the game shows: its faded buffer)
        u32 row = 32, want = GetIndex(idx) & 0x7FFF;
        if (sLineTab == NULL && (sLineTab = SafeAlloc(192 * 2)) == NULL)
            return;
        for (k = 0; k < 24; k++)
            if ((map[k * 32 + col] & 1023) == tile && (map[k * 32 + col] >> 12) == pal)
            {
                row = k;
                break;
            }
        // line L shows the colour written in the H-blank before it: table[L - 1]; line 0 the colour set now
        for (k = 0; k < 192; k++)
        {
            u32 line = k + 1, c = now, j;
            if (row < 24 && now == want && line >= row * 8 + off && line < row * 8 + off + cnt)
            {
                j = line - row * 8 - off;
                c = j & 1 ? sObjCopies[n + 1 + j / 2].w[1] : sObjCopies[n + 1 + j / 2].w[0] >> 16;
            }
            sLineTab[k] = (u16)c;
        }
        dma[2] = 0;                                    // (channel 0 stopped, restarted at the table's start)
        ((vu16 *)0x05000000)[idx] = now;               // (line 0)
        dma[0] = (u32)sLineTab;
        dma[1] = 0x05000000 + idx * 2;
        DC_FlushRange_(sLineTab, 192 * 2);
        dma[2] = 1 | 2u << 21 | 1u << 25 | 2u << 27 | 1u << 31;   // (1 halfword, fixed dest, repeat, H-blank, on)
        return;
    }
}

static void RegTask(void *task, void *data)
{
    u32 i;
    (void)task; (void)data;
#if RH_TEST
    gRhScreenSig.calls = gRhScreenSig.callsNow | gRhScreenSig.bytes << 12;
    gRhScreenSig.callsNow = 0;
    gRhScreenSig.bytes = 0;
    if (gRhScreenSig.dbg & 2)
        return;
#endif
    for (i = 0; i < sRegCount; i++)             // (first: early in the vertical blank)
        *(vu16 *)(0x04000000 + (sRegs[i] >> 16)) = (u16)sRegs[i];
    RowRules(FALSE);                            // (a selection the game moved: right away)
    Scanlines();
    if (sTilesFrame != sFrame)                  // (once a frame: here, or at the first copy of the game's sprites)
    {
        sTilesFrame = sFrame;
        ObjCopyTiles();
    }
    ObjCopies(0xFFFFFFFF, 0, NULL);                   // (the sub screen's sprites: their copy doesn't come through the hook)
    sFrame++;
    // the screen's palette index patches in palette RAM: a colour the game loads straight into it (an icon's blink,
    // the battle HP boxes) would show until ThemeTask's next run. Not over a fade step (palette RAM = the game's
    // faded buffer: fades go on, also the ones of a single row, e.g. a blinking icon). What the game loaded is what
    // the undo puts back
    for (i = 0; i < sIndexCount; i++)
    {
        u32 idx = sIndexUndo[sIndexCount + i] >> 16 & 1023, want = IdxWant(sIndexUndo[sIndexCount + i]);
        u32 c = ((vu16 *)0x05000000)[idx];
        if (c != want && c != GetBuf(idx, TRUE))
        {
            sIndexUndo[2 * sIndexCount + i] = c & 0x7FFF;
            ((vu16 *)0x05000000)[idx] = (u16)want;
        }
    }
}

static u32 sLastBase;       // the fingerprint before a version's colour went in (FindPage)
static void ThemeTask(void *task, void *data)
{
    u32 theme = RHCT_UiTheme(), sig, i;
    (void)task; (void)data;
    if (theme != RH_THEME_AMOLED && theme != RH_THEME_IDS)
        return;
#if RH_TEST
    if (gRhScreenSig.dbg & 1)
        return;
#endif
    {
        // both screens at full black (master brightness down 16: a map's loading): nothing shows, the screen's
        // fingerprint changes as the maps load - looked at every 8th frame only (every frame cost the load 12 frames
        // at a door); the new screen's page is still in before the fade in
        static u32 sDark;
        u16 b0 = *(vu16 *)0x0400006C, b1 = *(vu16 *)0x0400106C;
        if ((b0 >> 14) == 2 && (b0 & 31) >= 16 && (b1 >> 14) == 2 && (b1 & 31) >= 16)
        {
            if ((sDark++ & 7) != 0)
                return;
        }
        else
            sDark = 0;
    }
    sig = ScreenSig();
    gRhScreenSig.sig = sig;
#if RH_TEST
    gRhScreenSig.patches = ((u32 (*)(u32))(0x0201ABEC | 1))(0);    // (test tools: the system heap's free bytes)
#endif
    if (theme != RH_THEME_AMOLED)
        return;
#if RH_TEST
    if (gRhScreenSig.rows == 0x45474150)       // (test tools: "PAGE" = no screen pages, for the captures learn.py reads)
        sig = 0;
#endif
    {
        // the version the entry's colour says; one without a page while the screen stays (the version's own colours
        // reached that entry) keeps the version shown. No page: the learned screen most like it (FuzzyFind)
        u32 v;
        BOOL fz = FALSE;
        if ((i = FindPage(sig)) == 0 && sig != 0 && (v = FuzzyFind(sig)) != 0)
        {
            sig = v;
            i = FindPage(sig);
            fz = TRUE;
        }
        v = sig;
        sFuzzyNext = fz;
        if (i >> 31)
        {
            v = (sig ^ (((vu16 *)0x05000000)[i & 1023] & 0x7FFF)) * 16777619u;
            if (FindPage(v) == 0 && sig == sLastBase)
                v = sLastSig;
            else if (FindPage(v) == 0)            // (no version's colour: the default version, the owner's
                v = (sig ^ 0x8000) * 16777619u;   // unpressed picture - tools/rh/gen_rh_amoled.py)
        }
        sLastBase = sig;
        sig = v;
    }
    if (sig != sLastSig)               // (the same page found by fingerprint or by FuzzyFind: kept, no undo/redo blip)
    {
        sLastSig = sig;
        if (sPatch != NULL)
        {
            TilePatches(TRUE);
            Heap_Free(sPatch);
            sPatchVal = NULL;
            sPatch = NULL;
            sPatchCount = 0;
        }
        if (sIndexUndo != NULL)
        {
            u32 i;
            for (i = sIndexCount; i-- > 0;)
            {
                // the game's buffers get what they had, palette RAM what it showed: the game can load colours
                // straight into palette RAM (the battle HP boxes), leaving a stale buffer (the intro's sky blue).
                // Only where the theme's colour (IDX_MARK) is still there: what the game loaded since (the next
                // screen's palettes, often loaded before its tile maps make it count as shown) stays
                u32 idx = sIndexUndo[i] >> 16 & 1023, ram = ((vu16 *)0x05000000)[idx];
                if (GetIndex(idx) & IDX_MARK)
                    SetIndex(idx, sIndexUndo[i] & 0x7FFF);
                ((vu16 *)0x05000000)[idx] = (u16)(ram & IDX_MARK ? sIndexUndo[2 * sIndexCount + i] : ram);
            }
            sIndexCount = 0;                    // (first: RegTask may run in between)
            Heap_Free(sIndexUndo);
            sIndexUndo = NULL;
        }
        if (sRows != NULL)
        {
            Heap_Free(sRows);
            sRows = NULL;
        }
        if (sRegs != NULL)
        {
            sRegCount = 0;                      // (first: RegTask may run in between)
            Heap_Free(sRegs);
            sRegs = NULL;
        }
        if (sObjCopies != NULL)
        {
            for (i = 0; i < sObjCopyCount; i++) // (the copies' places empty again: the next screen's may be there)
                if (sObjCopies[i].mine && (sObjCopies[i].w[0] >> 12 & 1))
                    continue;                   // (a pixel patch: no place of its own)
                else if (sObjCopies[i].mine && (sObjCopies[i].w[0] >> 15 & 1))
                    *(vu32 *)(((sObjCopies[i].w[0] & 1) ? 0x06600000 : 0x06400000) + (sObjCopies[i].w[0] >> 16) * 4) = 0;
                else if (sObjCopies[i].w[0] >> 14 & 1)
                {
                    if (sObjCopies[i].mine >= 2 && sObjCopies[i].mine != 0xFFFFFFFF)   // (its slot parked, hidden)
                        ((vu16 *)0x07000000)[((sObjCopies[i].w[0] & 1 ? 128 : 0) + sObjCopies[i].mine - 2) * 4] = 0x200;
                }
                else if (sObjCopies[i].mine)
                {
                    u32 w1 = sObjCopies[i].w[1], eng = sObjCopies[i].w[0] & 1, k;
                    u32 sh = 5 + (*(vu32 *)(eng ? 0x04001000 : 0x04000000) >> 20 & 3);
                    vu32 *vd = (vu32 *)((eng ? 0x06600000 : 0x06400000) + ((w1 & 1023) << sh));
                    for (k = 0; k < (w1 >> 16 & 255) * 8; k++)
                        vd[k] = 0;
                }
            for (i = 0; i < sObjCopyCount; i++) // (scanline colours: the H-blank DMA off, the entry the game's again)
                if ((sObjCopies[i].w[0] >> 13 & 1) && !(sObjCopies[i].w[0] >> 12 & 1) && !(sObjCopies[i].w[0] >> 14 & 3)
                    && !(sObjCopies[i].w[0] >> 3 & 1))
                {
                    *(vu32 *)0x040000B8 = 0;
                    ((vu16 *)0x05000000)[sObjCopies[i].w[1] & 1023] = (u16)GetBuf(sObjCopies[i].w[1] & 1023, TRUE);
                }
            sObjCopyCount = 0; sObjActCount = 0; sObjKeyCount = 0;   // (first: the OAM hook may run in between)
            Heap_Free(sObjCopies);
            sObjCopies = NULL;
        }
        if (sRules != NULL)
        {
            RowRules(TRUE);
            sRuleCount = 0;                     // (first: RegTask may run in between)
            Heap_Free(sRules);
            sRules = NULL;
        }
        sFuzzyShown = sFuzzyNext;
        ShowScreen(sig);
        // (the maps' cached parts were hashed with the old screen's patches undone: hash them again)
        for (i = 0; i < 8; i++)
            sMapQuick[i] = 0;
    }
    TilePatches(FALSE);
    ApplyRows();
    {
        u32 i;
        for (i = 0; i < sIndexCount; i++)
        {
            // the theme writes its colours with IDX_MARK: an entry without it was loaded by the game after the screen
            // showed (even in the very colour wanted), and its new colour is the one to put back on undo, not what
            // the entry held before the load (e.g. the battle intro's sky under the HP boxes)
            u32 idx = sIndexUndo[sIndexCount + i] >> 16 & 1023, want = IdxWant(sIndexUndo[sIndexCount + i]);
            u32 c = GetIndex(idx);
            if (c != want)
            {
                sIndexUndo[i] = idx << 16 | (c & 0x7FFF);
                sIndexUndo[2 * sIndexCount + i] = c & 0x7FFF;
                SetIndexKeepFade(idx, want, c);
            }
            // (loaded straight into palette RAM: RegTask, every vertical blank)
        }
    }
    RowRules(FALSE);
    // the register patches after the game's main loop too (it may set its registers there, after RegTask's vertical
    // blank: the field menu's dimming blend came back every other frame)
    for (i = 0; i < sRegCount; i++)
        *(vu16 *)(0x04000000 + (sRegs[i] >> 16)) = (u16)sRegs[i];
    {
        const u32 *hp = (const u32 *)0x023C4000;    // the battle overlay's parts (src/battle/rh_hptext.c)
        // (the register patches: RegTask, every vertical blank)
        sCmMap = 0;
        if (hp[0] == 0x54504852 && hp[1] == 0x31545854)
        {
            sCmMap = ((u32 (*)(void))hp[5])();      // battle move buttons (the Check Moves map it re-pointed, or 0)
            ((void (*)(void))hp[2])();              // HP-box text
        }

    }
}

static void ThemePalette(u32 narcId, s32 fileId, u8 *data, u32 fileSize)
{
    if (!sTaskMade)
    {
        sTaskMade = SysTask_CreateOnVWaitQueue(ThemeTask, NULL, 0) != NULL
                    && SysTask_CreateOnVBlankQueue(RegTask, NULL, 0x7FFFFFFF) != NULL;
    }
    u32 theme, size, start, ctx;
    if (data == NULL || data[0] != 'R' || data[1] != 'L' || data[2] != 'C' || data[3] != 'N')
        return;
    theme = RHCT_UiTheme();
    if ((theme != RH_THEME_AMOLED && theme != RH_THEME_IDS) || IsArt(narcId))
        return;                                              // Default (and Randomizer, for now): untouched
    // NCLR: TTLP section at 0x10; its data at 0x18: format, extPltt, size, offset of the colours (from 0x18)
    // (the size field is often bigger than the file: never write past its end)
    size = *(u32 *)(data + 0x20);
    start = 0x18 + *(u32 *)(data + 0x24);
    if (fileSize < 0x28 || start >= fileSize)
        return;
    if (size > fileSize - start)
        size = fileSize - start;
    ctx = NextContext(narcId, fileId);
#if RH_TEST
    if (theme == RH_THEME_IDS)
    {
        IdColours(narcId, fileId, 0, (u16 *)(data + start), size / 2, ctx);
        return;
    }
#endif
    ThemeRead(narcId, fileId, start, size, data + start, TRUE, ctx);
    ThemeCodeColours();
}

// Sprite copies (tools/rh/amoled/learn.py OBJ_COPIES): sprites drawn with the same tiles twice, flipped, that the owner
// coloured differently (the bag's selected-item frame: its bottom half is its top half upside down, the owner's bottom
// edge darker). The flipped sprites are pointed at a copy of their tiles in free OBJ VRAM (written only where those
// tiles are empty or the copy), with one entry moved to a spare one. Each: { w0 = engine | palette << 4 | vflip << 8 |
// first tile << 16, w1 = copy's first tile | tile count << 16 | entry << 24 | spare entry << 28 }. Tile count 0: a
// palette move instead (sprites sharing a row's entries with others the owner coloured otherwise: the sprite of that
// first tile (and x, w1 bits 4-13: 1 + x, 0 any) goes to palette row w1 & 15, a copy of its row with the owner's
// colours, set by index patches).
// tiles: once per frame (RegTask: the game also copies single sprites, many times a frame); sprites: after every copy
// of the game's sprites
static void ObjCopyTiles(void)
{
    u32 n, i;
    for (n = 0; n < sObjCopyCount; n++)
    {
        u32 w0 = sObjCopies[n].w[0], w1 = sObjCopies[n].w[1], eng = w0 & 1;
        if (w0 >> 12 & 1)                              // (a pixel patch of the copy before it: below)
            continue;
        if ((w0 >> 13 & 1) && !(w0 >> 14 & 3))         // (scanline colours: Scanlines)
            continue;
        if (w0 >> 15 & 1)
        {
            // Overlay tile data (w0 bit 15): one word of an overlay sprite's tiles (ObjCopies, bit 14) at OBJ VRAM
            // byte offset (w0 >> 16) * 4: pixels no palette row can give (a screen whose rows are all in use, the
            // trainer card's leader portraits) drawn by sprites of the theme's own, in the owner's colours. Written
            // where that word is free (zero) or already the theme's; zero again when the screen goes
            vu32 *vd = (vu32 *)((eng ? 0x06600000 : 0x06400000) + (w0 >> 16) * 4);
            if (!sObjCopies[n].mine && *vd != 0)
            {
                // (the game uses that word: the overlay sprite these words are for - the next bit 14 entry - is
                // not shown, it would show the game's tiles)
                u32 m;
                for (m = n + 1; m < sObjCopyCount && !((sObjCopies[m].w[0] >> 14 & 1) && !(sObjCopies[m].w[0] >> 12 & 1)
                                                        && !(sObjCopies[m].w[0] >> 15 & 1)); m++)
                    ;
                if (m < sObjCopyCount)
                    sObjCopies[m].mine = 0xFFFFFFFF;
                continue;
            }
            sObjCopies[n].mine = 1;
            if (*vd != w1)
                *vd = w1;
            continue;
        }
        if (w0 >> 14 & 1)                              // (an overlay sprite: ObjCopies; 0xFFFFFFFF: blocked)
        {
            if (!sObjCopies[n].mine)
                sObjCopies[n].mine = 1;
            continue;
        }
        if ((w1 >> 16 & 255) == 0)                     // (a palette move: no tiles to copy)
        {
            if (!sObjCopies[n].mine)                   // (2: a translucent rule's row is set, ObjCopies)
                sObjCopies[n].mine = 1;
            continue;
        }
        u32 src = w0 >> 16 & 1023, dst = w1 & 1023, cnt = w1 >> 16 & 255, from = w1 >> 24 & 15, to = w1 >> 28 & 15;
        u32 sh = 5 + (*(vu32 *)(eng ? 0x04001000 : 0x04000000) >> 20 & 3);
        vu32 *vs = (vu32 *)((eng ? 0x06600000 : 0x06400000) + (src << sh)), *vd = (vu32 *)((eng ? 0x06600000 : 0x06400000) + (dst << sh));
        BOOL ok = TRUE;
        if (!sObjCopies[n].mine)                       // (the copy's place: empty, else the game uses it)
            for (i = 0; i < cnt * 8 && ok; i++)
                ok = vd[i] == 0;
        if (!ok)
            continue;
        // the source's tiles change: the copy follows once they have held still for two looks (the game clears and
        // redraws a label's tiles now and then: a copy made in between blinked); mine = 1 | the pending sum << 1 |
        // the copied sum << 16
        {
            u32 h = 0, h16, m0 = sObjCopies[n].mine;
            for (i = 0; i < cnt * 8; i++)
                h = h * 31 + vs[i];
            h16 = (h ^ h >> 16) & 0xFFFF;
            if (m0 != 0 && h16 == m0 >> 16)
                continue;                              // (the copy is up to date)
            if (m0 != 0 && (h16 & 0x7FFF) != (m0 >> 1 & 0x7FFF))
            {
                sObjCopies[n].mine = (m0 & 0xFFFF0000) | (h16 & 0x7FFF) << 1 | 1;
                continue;                              // (changed: wait for it to hold still)
            }
            sObjCopies[n].mine = h16 << 16 | (h16 & 0x7FFF) << 1 | 1;
        }
        u32 p = n + 1;                                 // (the pixel patches, sorted by word: one pass)
        for (i = 0; i < cnt * 8; i++)                  // (again every frame: the game animates the tiles)
        {
            u32 w = vs[i], m = 0, q;
            if (from != to)                            // (pixel copies remap nothing: from = to = 0)
            {
                for (q = 0; q < 8; q++)
                    if ((w >> (q * 4) & 15) == from)
                        m |= 15u << (q * 4);
                w = (w & ~m) | ((to * 0x11111111u) & m);
            }
            // pixel patches (w0 bit 12) following the copy: single pixels of the copy pointed at another entry - a
            // label's box that ends inside a tile the button's background also fills (Oak's Touch, the field menu's
            // CHECK): { w0 = 1 << 12 | engine | a second one << 13 (bit 30: there is one), w1 = word | pixel in the
            // word << 12 | entry << 16 }; sorted by word (tools/rh/amoled/learn.py), so one pass over them
            while (p < sObjCopyCount && (sObjCopies[p].w[0] >> 12 & 1))
            {
                u32 a = sObjCopies[p].w[1], b = sObjCopies[p].w[0] >> 13, hb = b >> 17 & 1;
                u32 last = hb ? (b & 1023) : (a & 1023);
                if ((a & 1023) > i)
                    break;                             // (the next patch is for a later word)
                if ((a & 1023) == i)
                {
                    q = a >> 12 & 7;
                    w = (w & ~(15u << (q * 4))) | (a >> 16 & 15) << (q * 4);
                }
                if (hb && (b & 1023) == i)
                {
                    q = b >> 10 & 7;
                    w = (w & ~(15u << (q * 4))) | (b >> 13 & 15) << (q * 4);
                }
                if (last > i)
                    break;                             // (its second patch is for a later word)
                p++;
            }
            if (vd[i] != w)
                vd[i] = w;
        }
    }
}

// (a sprite patched in OAM: in the game's buffer the copy came from too - another copy of that buffer later in the
// frame, by a path without the hook, carries the patch: the field menu's CHECK label blinked every A icon step)
#define MIRROR(k_, e_) do { if (obuf != NULL && offset != 0xFFFFFFFF && (k_) * 8 >= offset && (k_) * 8 - offset < size)     { u16 *b_ = obuf + ((k_) * 8 - offset) / 2; b_[0] = (e_)[0]; b_[1] = (e_)[1]; b_[2] = (e_)[2]; } } while (0)
static void ObjCopies(u32 offset, u32 size, u16 *obuf)
{
    u32 n, k;
    // (at most 16 times a frame: the overworld copies sprites hundreds of times a frame, the Mart lagged; the screen's
    // own sprite copies come in between, so not just once)
    static u32 sCalls;
    if (sObjCopyCount == 0)
        return;
    if (offset != 0xFFFFFFFF)          // (this frame's hooked copies, per engine: RegTask's pass skips those engines)
    {
        if (sHookFrame != sFrame)
            sHookFrame = sFrame, sHooked = 0;
        sHooked |= 1u << (offset >= 0x400);
    }
    if (offset != 0xFFFFFFFF)          // (a copy to the other screen's sprites than every copy's: nothing to do - the
    {                                  // overworld copies its sprites hundreds of times a frame, the field menu's
        u32 e = offset >= 0x400;       // label moves are the sub screen's; a mask made once: not a walk through
        if (!(sObjEngMask >> e & 1))   // the list for every copy, ~100 pixel patches made the overworld lag)
            return;
    }
    if (sObjDone != sFrame)
    {
        sObjDone = sFrame;
        sCalls = 0;
    }
    // (offset 0xFFFFFFFF: RegTask, every vertical blank; every copy of the game's sprites, each pass only over the
    // sprites that copy wrote: below)
    sCalls++;
    // (the game animates the tiles: the copy follows at the first 4 sprite copies of each frame - not at every one: the
    // bag copies its sprites many times a frame, the tile copies made it lag and drop button presses; not just the
    // first: the field menu animates its A icon after it, the CHECK label's copy blinked)
    if (offset != 0xFFFFFFFF && sTilesFrame != sFrame)
    {
        sTilesFrame = sFrame;
        ObjCopyTiles();
    }
    // the entries matched by their sprite's tile: one look at each slot the copy wrote, its tile against the mask
    for (u32 eg = 0; eg < 2 && sObjKeyCount != 0; eg++)
    {
        u32 lo = eg ? 128 : 0, hi = lo + 128, a_;
        vu16 *oam = (vu16 *)0x07000000;
        if (!(sObjEngMask >> eg & 1))
            continue;
        if (offset != 0xFFFFFFFF)
        {
            u32 a = offset / 8, b = (offset + size + 7) / 8;
            if (a > lo) lo = a;
            if (b < hi) hi = b;
        }
        else if (sHookFrame == sFrame && (sHooked >> eg & 1))
            continue;                  // (RegTask: that engine's sprites came through the hook this frame, done)
        for (k = lo; k < hi; k++)
        {
            vu16 *e = oam + k * 4;
            u32 t = e[2] & 1023;
            if (!(sObjTileMask[t >> 5] >> (t & 31) & 1) || (e[0] & 0x300) == 0x200)
                continue;
            for (a_ = 0; a_ < sObjKeyCount; a_++)
            {
                u32 m = sObjKey[a_], w0 = sObjCopies[m].w[0], w1 = sObjCopies[m].w[1];
                u32 pal = w0 >> 4 & 15, vf = w0 >> 8 & 1, src = w0 >> 16 & 1023, dst = w1 & 1023;
                if ((w0 & 1) != eg || !sObjCopies[m].mine)
                    continue;
                if ((w1 >> 16 & 255) == 0)     // (a palette move of one tile: to palette row dst; x: w1 bits 4-13)
                {
                    u32 x1 = w1 >> 4 & 1023;
                    if ((e[2] & 1023) == src && (e[2] >> 12) == pal && (x1 == 0 || (e[1] & 511) + 1 == x1)
                        && (!(w0 >> 10 & 1) || (e[1] >> 13 & 1) == vf))
                    {
                        e[2] = (u16)((e[2] & 0x0FFF) | (dst & 15) << 12);
                        MIRROR(k, e);
                    }
                }
                else if ((e[2] & 1023) == src && (e[2] >> 12) == pal && (e[1] >> 13 & 1) == vf)
                {
                    e[2] = (u16)((e[2] & ~1023u) | dst);
                    MIRROR(k, e);
                }
            }
        }
    }
    for (u32 a_ = 0; a_ < sObjActCount; a_++)
    {
        n = sObjAct[a_];
        if (offset == 0xFFFFFFFF && sHookFrame == sFrame && (sHooked >> (sObjCopies[n].w[0] & 1) & 1))
            continue;                  // (RegTask: that engine's sprites came through the hook this frame, done)
        u32 w0 = sObjCopies[n].w[0], eng = w0 & 1, pal = w0 >> 4 & 15, vf = w0 >> 8 & 1, src = w0 >> 16 & 1023;
        u32 dst = sObjCopies[n].w[1] & 1023, lo = eng ? 128 : 0, hi = lo + 128;
        vu16 *oam = (vu16 *)0x07000000;
        if (!sObjCopies[n].mine || (w0 >> 12 & 1) || (w0 >> 15 & 1) || ((w0 >> 13 & 1) && !(w0 >> 14 & 1)))
            continue;
        if (w0 >> 14 & 1)
        {
            // An overlay sprite (w0 bit 14): w0 = engine | priority << 1 | shape << 4 | size << 6 | y << 16, w1 = x |
            // tile << 9 | palette row << 19. It takes a sprite slot the game leaves parked (hidden, or at y 192 off
            // screen), from the last one down, kept while the game keeps it parked (mine = 2 + slot)
            u32 s = sObjCopies[n].mine, a0, a1, a2, w1 = sObjCopies[n].w[1];
            vu16 *e;
            if (s == 0xFFFFFFFF)
                continue;                              // (its tiles' place is the game's: not shown)
            if (s >= 2)
            {
                e = oam + (eng ? 128 : 0) * 4 + (s - 2) * 4;
                if ((e[0] & 0x300) != 0x200 && (e[0] & 255) != 192 && e[2] != (u16)(w1 >> 9 & 1023 | (w0 >> 1 & 3) << 10 | (w1 >> 19 & 15) << 12))
                    s = sObjCopies[n].mine = 1;        // (the game uses that slot now: another one)
            }
            if (s < 2)
            {
                u32 k2, m;
                for (k2 = 128; k2-- > 0;)
                {
                    e = oam + (eng ? 128 : 0) * 4 + k2 * 4;
                    if ((e[0] & 0x300) != 0x200 && (e[0] & 255) != 192)
                        continue;
                    for (m = 0; m < sObjCopyCount && !((sObjCopies[m].w[0] >> 14 & 1) && !(sObjCopies[m].w[0] >> 12 & 1)
                                                       && !(sObjCopies[m].w[0] >> 15 & 1) && sObjCopies[m].mine == k2 + 2); m++)
                        ;
                    if (m == sObjCopyCount)
                        break;
                }
                if (k2 >= 128)
                    continue;                          // (no parked slot: the game uses all 128)
                s = sObjCopies[n].mine = k2 + 2;
            }
            k = (eng ? 128 : 0) + s - 2;
            if (k < lo || k >= hi)
                continue;                              // (this copy didn't write that slot)
            a0 = (w0 >> 16 & 255) | (w0 >> 4 & 3) << 14;
            a1 = (w1 & 511) | (w0 >> 6 & 3) << 14;
            a2 = (w1 >> 9 & 1023) | (w0 >> 1 & 3) << 10 | (w1 >> 19 & 15) << 12;
            e = oam + k * 4;
            e[0] = (u16)a0;
            e[1] = (u16)a1;
            e[2] = (u16)a2;
            MIRROR(k, e);
            continue;
        }
        // (a copy of the game's sprites: only the sprites it wrote - offset / size in bytes, the sub screen's from
        // 0x400 - not the whole OAM: the Oak intro copies single sprites to the sub screen many times a frame, the
        // whole-OAM pass for every copy starved the game; RegTask's pass, offset 0xFFFFFFFF: all)
        if (offset != 0xFFFFFFFF)
        {
            u32 a = offset / 8, b = (offset + size + 7) / 8;
            if (a > lo)
                lo = a;
            if (b < hi)
                hi = b;
        }
        if (w0 >> 9 & 1)
        {
            // Translucent sprites (w0 bit 9): the game blends them over the screen's background (the Pokedex list's
            // seen-only Pokemon: pale over the Default theme's white cells, dark over the owner's black ones). Sprites
            // of palette row pal in translucent mode are drawn solid with row dst, which holds row pal's colours
            // blended as the game would over the Default background colour (w0 >> 16), with the game's own blend
            // weights: the owner's pale icon for any Pokemon, and row pal stays the game's for solid sprites.
            u32 base = eng ? 768 : 256, bg = w0 >> 16 & 0x7FFF, e, ch;
            u32 ab = *(vu16 *)(eng ? 0x04001052 : 0x04000052), eva = ab & 31, evb = ab >> 8 & 31;
            if (eva > 16) eva = 16;
            if (evb > 16) evb = 16;
            // (the blended row: when the screen appears, then once per frame, in the vertical blank - not after
            // every sprite copy: 15 buffer lookups each made the Pokedex lag)
            if (sObjCopies[n].mine == 2 && offset != 0xFFFFFFFF)
                e = 16;
            else
                e = 1, sObjCopies[n].mine = 2;
            for (; e < 16; e++)
            {
                u32 c = GetIndex(base + pal * 16 + e) & 0x7FFF, out = 0, old = GetIndex(base + dst * 16 + e);
                for (ch = 0; ch < 15; ch += 5)
                {
                    u32 v = ((c >> ch & 31) * eva + (bg >> ch & 31) * evb) >> 4;
                    out |= (v > 31 ? 31 : v) << ch;
                }
                if ((old & 0x7FFF) != out)
                    SetIndexKeepFade(base + dst * 16 + e, out, old);
            }
            for (k = lo; k < hi; k++)
            {
                vu16 *e2 = oam + k * 4;
                if ((e2[0] & 0x300) != 0x200 && (e2[0] >> 10 & 3) == 1 && (e2[2] >> 12) == pal)
                {
                    e2[0] = (u16)(e2[0] & ~0x0C00);
                    e2[2] = (u16)((e2[2] & 0x0FFF) | (dst & 15) << 12);
                    MIRROR(k, e2);
                }
            }
            continue;
        }
        if ((sObjCopies[n].w[1] >> 16 & 255) == 0)     // (a palette move: the sprite of that tile to palette dst)
        {
            u32 x1 = sObjCopies[n].w[1] >> 4 & 1023;   // (1 + the sprite's x, 0: any: sprites sharing tiles)
            for (k = lo; k < hi; k++)
            {
                vu16 *e = oam + k * 4;
                // (w0 bit 10: only sprites flipped as w0 bit 8 says - the Mart's selected-item frame, its bottom half
                // the top half upside down, the owner's bottom edge darker; bit 11: sprites of any tile)
                if ((e[0] & 0x300) != 0x200 && ((w0 >> 11 & 1) || (e[2] & 1023) == src) && (e[2] >> 12) == pal
                    && (x1 == 0 || (e[1] & 511) + 1 == x1) && (!(w0 >> 10 & 1) || (e[1] >> 13 & 1) == vf))
                {
                    e[2] = (u16)((e[2] & 0x0FFF) | (dst & 15) << 12);
                    MIRROR(k, e);
                }
            }
            continue;
        }
        for (k = lo; k < hi; k++)
        {
            vu16 *e = oam + k * 4;
            if ((e[0] & 0x300) != 0x200 && (e[2] & 1023) == src && (e[2] >> 12) == pal && (e[1] >> 13 & 1) == vf)
            {
                e[2] = (u16)((e[2] & ~1023u) | dst);
                MIRROR(k, e);
            }
        }
    }
}

// after the game copied sprites to OAM
static void ThemeOam(u32 offset, u32 size, u16 *src)
{
    u32 i;
#if RH_TEST
    gRhScreenSig.callsNow++;           // (test tools: sprite copies a frame - RegTask keeps the last frame's)
    gRhScreenSig.bytes += size;
    if (gRhScreenSig.dbg & 4)
        return;
#endif
#if RH_TEST
    if (!(gRhScreenSig.dbg & 8))
#endif
    ObjCopies(offset, size, src);
#if RH_TEST
    if (gRhScreenSig.dbg & 16)
        return;
#endif
    // the register patches here too: the game's vertical-blank callback (which copies the sprites) may set its
    // registers after RegTask ran (the field menu's dimming blend came back every other frame)
    for (i = 0; i < sRegCount; i++)
        *(vu16 *)(0x04000000 + (sRegs[i] >> 16)) = (u16)sRegs[i];
    // the row rules too: colours the game writes every frame (Oak's boy / girl cards: the unselected card's frame)
    // reach palette RAM in its vertical-blank callback, after RegTask; the rule's colour and the game's alternated
    if (sRuleCount != 0 && sRuleFrame != sFrame)
    {
        sRuleFrame = sFrame;
        RowRules(FALSE);
    }
}

// what the resident part calls (the first thing in data/rh_theme.bin: rhtheme/linker.ld)
__attribute__((section(".rhtb_header"), used))
const struct RhThemeBlob gRhThemeBlobHeader = { RHTB_MAGIC, ThemePalette, ThemeRead, ThemeOam };
