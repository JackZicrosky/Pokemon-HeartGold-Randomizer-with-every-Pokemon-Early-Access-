// Romhack: UI Theme (Misc. Tweaks: Default / Randomizer / AMOLED), port of the FireRed/Emerald rh_theme.c idea.
// Every palette file (NCLR) the game reads through the graphics loader (GfGfxLoader_LoadFromNarc / LoadFromOpenNarc
// and their _GetSizeOut forms) or the whole-member readers (AllocAndReadWholeNarcMemberByIdPair, its AllocAtEnd form,
// NARC_AllocAndReadWholeMember) is swapped for its themed version as it loads, before anything is drawn: screen fades
// (master brightness) and the PaletteData fades then work on the themed colours, so transitions and animations stay right.
// AMOLED: the owner's own designs ("UI screenshots - AMOLED"), turned into whole replacement palettes by
// tools/rh/gen_rh_amoled.py and stored in the ROM file data/rh_amoled.bin; palettes not in it stay as they are.
// Default = never touched.
// NARC_New is hooked too, to remember which archive an open NARC is (the spare bytes 0x52-0x53 of the struct).
#include "../include/types.h"
#include "../include/rh/rh_coretable.h"
#include "../include/rh/rh_test.h"

#define RH_THEME_DEFAULT    0
#define RH_THEME_RANDOMIZER 1
#define RH_THEME_AMOLED     2
#define RH_THEME_IDS        3   // test builds only: every colour becomes its id (palette slot, index), see below

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

// data/rh_amoled.bin (tools/rh/gen_rh_amoled.py): "RHA3", entry count, a bit mask of the archives in it (320 bits),
// then the entries { u32 narc << 16 | file, u32 screen context (0xFFFFFFFF: any screen), u32 offset } sorted, each
// offset pointing at { u32 colour count, u32 where the colours start in the palette file, colours }. Every palette
// file of the UI archives is in it once for any screen, plus a version for each screen the owner coloured it
// differently on.
#define AM_MAGIC   0x33414852
#define AM_HEADER  48
#define AM_ANY     0xFFFFFFFF
static u32 sArcMask[10];
static BOOL sArcMaskLoaded;

// Opens the table and looks (narc, file, context) up: the file position of its colours (the table stays open), or 0
// (closed)
static u32 AmoledFind(u32 *file, u32 narcId, s32 fileId, u32 ctx, u32 *start, u32 *count)
{
    u32 hdr[AM_HEADER / 4], key = narcId << 16 | (u32)fileId, lo, hi, mid, i;
    if (narcId >= 320 || (sArcMaskLoaded && !(sArcMask[narcId >> 5] & 1u << (narcId & 31))))
        return 0;
    FS_InitFile(file);
    if (!FS_OpenFile(file, "data/rh_amoled.bin"))
        return 0;
    if (FS_ReadFile(file, hdr, AM_HEADER) == AM_HEADER && hdr[0] == AM_MAGIC)
    {
        for (i = 0; i < 10; i++)
            sArcMask[i] = hdr[2 + i];
        sArcMaskLoaded = TRUE;
        lo = 0; hi = hdr[1];
        while (lo < hi && (sArcMask[narcId >> 5] & 1u << (narcId & 31)))
        {
            mid = (lo + hi) / 2;
            FS_SeekFile(file, AM_HEADER + mid * 12, 0);
            FS_ReadFile(file, hdr, 12);
            if (hdr[0] == key && hdr[1] == ctx)
            {
                FS_SeekFile(file, hdr[2], 0);
                FS_ReadFile(file, hdr + 3, 8);
                *count = hdr[3];
                *start = hdr[4];
                return hdr[2] + 8;
            }
            if (hdr[0] < key || (hdr[0] == key && hdr[1] < ctx))
                lo = mid + 1;
            else
                hi = mid;
        }
    }
    FS_CloseFile(file);
    return 0;
}

// A read of bytes [offset, offset + size) of a palette file landed at dst: swap the colours in it for the theme's.
// whole: a whole file (its screen context was counted by the caller: ctx); else a partial read: some screens read
// only a few colours of a palette file, e.g. the battle move buttons' type colours (counted here when the file is a
// UI palette).
static void ThemeRead(u32 narcId, s32 fileId, u32 offset, u32 size, u8 *dst, BOOL whole, u32 ctx)
{
    u32 file[FSFILE_SIZE / 4];      // FSFile (word aligned)
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
        FS_CloseFile(file);
        if (a < b && !((a - start) & 1))
            IdColours(narcId, fileId, (a - start) / 2, (u16 *)(dst + a - offset), (b - a) / 2, ctx);
        return;
    }
#endif
    // this screen's version of the palette, if the owner coloured it differently here
    FS_CloseFile(file);
    pos = AmoledFind(file, narcId, fileId, ctx, &start, &count);
    if (pos == 0)
        pos = AmoledFind(file, narcId, fileId, AM_ANY, &start, &count);
    if (pos == 0)
        return;
    if (a < b && !((a - start) & 1))
    {
        FS_SeekFile(file, pos + a - start, 0);
        FS_ReadFile(file, dst + a - offset, (b - a) & ~1u);
    }
    FS_CloseFile(file);
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
};

static void ThemeCodeColours(void)
{
    u32 i;
    for (i = 0; i < NELEMS(sCodeColours); i++)
    {
        const struct CodeColour *c = &sCodeColours[i];
        if (*(u16 *)c->guard == c->guardValue && *(u16 *)c->addr == c->vanilla)
            *(u16 *)c->addr = c->amoled;
    }
}

// Screen fingerprint: some screens show a palette that another screen loaded (Pokedex pages, battle menus, bag
// pockets...), and the owner coloured it differently on each. A hash of the background tile maps of both screens
// says which one is showing; it is worked out every frame (AMOLED only) by a task on the main queue.
#define SysTask_CreateOnMainQueue ((void *(*)(void (*)(void *, void *), void *, u32))(0x0200E320 | 1))
struct RhScreenSig { u32 magic, sig, shown, patches, rows; };
struct RhScreenSig gRhScreenSig = { 0x47534852 /* "RHSG" */, 0, 0, 0, 0 };   // (the test tools read it)
static BOOL sTaskMade;

static u32 ScreenSig(void)
{
    u32 h = 2166136261u, eng, bg, i;
    for (eng = 0; eng < 2; eng++)
    {
        u32 io = eng ? 0x04001000 : 0x04000000, disp = *(vu32 *)io;
        for (bg = 0; bg < 4; bg++)
        {
            u32 cnt;
            const vu32 *map;
            if (!(disp & (0x100u << bg)))
                continue;
            cnt = *(vu16 *)(io + 8 + bg * 2);
            map = (const vu32 *)((eng ? 0x06200000 : 0x06000000 + ((disp >> 27) & 7) * 0x10000) + ((cnt >> 8) & 31) * 0x800);
            h = (h ^ cnt ^ bg << 16 ^ eng << 20) * 16777619u;
            for (i = 0; i < 0x600 / 4; i++)
                h = (h ^ map[i]) * 16777619u;
        }
    }
    return h;
}

// data/rh_amoled_pages.bin (tools/rh/gen_rh_amoled.py): "RHPG", screen count, row count, then the screens
// { u32 fingerprint, u32 offset } sorted; a screen's offset points at { u32 n, n x { u32 row, u32 target row } }; then
// the rows, 16 colours each (8 words) from offset 12 + screens * 8. A palette row (16 colours) that screens sharing it
// want in different versions is listed once per version; showing a screen = every live copy of a version of one of
// its rows (in palette RAM and in the game's palette buffers in main RAM) becomes the screen's version. One pass over
// the memory for all of them.
// The game's palette buffers (PaletteData_AllocBuffers / _FreeBuffers, hooked below): the live copies of the
// palettes besides palette RAM.
#define PaletteData_SetBuffers ((void (*)(void *, u32, void *, void *, u32))(0x0200310C | 1))
#define PG_MAX_BUFS 24
static struct { u32 *unfaded, *faded, size, id; } sBufs[PG_MAX_BUFS];

// PaletteData_AllocBuffers (arm9 0x02003120)
void RH_PaletteData_AllocBuffers(void *plttData, u32 bufferId, u32 size, u32 heapId)
{
    void *unfaded = Heap_Alloc(heapId, size), *faded = Heap_Alloc(heapId, size);
    u32 i;
    PaletteData_SetBuffers(plttData, bufferId, unfaded, faded, size);
    for (i = 0; i < PG_MAX_BUFS; i++)
        if (sBufs[i].unfaded == NULL)
        {
            sBufs[i].unfaded = unfaded; sBufs[i].faded = faded; sBufs[i].size = size; sBufs[i].id = bufferId;
            break;
        }
}

// PaletteData_FreeBuffers (arm9 0x02003150)
void RH_PaletteData_FreeBuffers(void *plttData, u32 bufferId)
{
    u32 **buf = (u32 **)((u8 *)plttData + bufferId * 20), i;
    for (i = 0; i < PG_MAX_BUFS; i++)
        if (sBufs[i].unfaded == buf[0])
            sBufs[i].unfaded = NULL;
    Heap_Free(buf[0]);
    Heap_Free(buf[1]);
}

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
static u32 *sPatch;
static u32 sPatchCount;

static void TilePatches(BOOL undo)
{
    u32 i;
    for (i = 0; i < sPatchCount; i++)
    {
        u32 e = sPatch[i], a = 0x06000000 + (e & 0x7FFFFF), sh = (a & 1) * 8 + (e >> 23 & 1) * 4;
        vu16 *b = (vu16 *)(a & ~1u);             // (VRAM takes no byte writes)
        u32 from = undo ? e >> 28 & 15 : e >> 24 & 15, to = undo ? e >> 24 & 15 : e >> 28 & 15;
        if ((*b >> sh & 15) == from)
            *b = (u16)((*b & ~(15u << sh)) | to << sh);
    }
}

// Palette index patches: on a screen, colour c for palette RAM entry idx (0-1023: main BG, main OBJ, sub BG, sub
// OBJ), in palette RAM and the game's buffers for that palette (PaletteData ids 0 main BG, 1 sub BG, 2 main OBJ,
// 3 sub OBJ): what the owner's picture needs there that the palette files can't give (colours set by code, rows
// another screen loaded, colours under the hardware's dimming / blending, worked back). Set once when the screen
// shows, so fades still work on them.
static u32 *sIndexUndo;     // (idx << 16 | colour it had) for the shown screen's index patches
static u32 sIndexCount;
static u32 *sRegs;          // register patches of the shown screen: (offset from 0x04000000 << 16 | value)
static u32 sRegCount;

// the colour an entry has now: from the game's unfaded palette buffer when there is one (palette RAM may be mid-fade)
static u32 GetIndex(u32 idx)
{
    static const u8 sBufFor[4] = { 0, 2, 1, 3 };
    u32 i, id = sBufFor[idx >> 8], off = idx & 255;
    for (i = 0; i < PG_MAX_BUFS; i++)
        if (sBufs[i].unfaded != NULL && sBufs[i].id == id && off * 2 < sBufs[i].size)
            return ((u16 *)sBufs[i].unfaded)[off];
    return ((vu16 *)0x05000000)[idx];
}

static void SetIndex(u32 idx, u32 c)
{
    static const u8 sBufFor[4] = { 0, 2, 1, 3 };
    u32 i, id = sBufFor[idx >> 8], off = idx & 255;
    ((vu16 *)0x05000000)[idx] = (u16)c;
    for (i = 0; i < PG_MAX_BUFS; i++)
        if (sBufs[i].unfaded != NULL && sBufs[i].id == id && off * 2 < sBufs[i].size)
        {
            ((u16 *)sBufs[i].unfaded)[off] = (u16)c;
            ((u16 *)sBufs[i].faded)[off] = (u16)c;
        }
}

static u32 *sSigs;         // the file's screen list (fingerprint, offset) pairs, read once (system heap)
static u32 sSigCount, sRowsAt;

static void ShowScreen(u32 sig)
{
    u32 file[FSFILE_SIZE / 4], hdr[3], ent[2], lo, hi, mid, n = 0, i, bloom = 0, pair[2];
    struct PgRow *row;
    if (sSigs == NULL)
    {
        FS_InitFile(file);
        if (!FS_OpenFile(file, "data/rh_amoled_pages.bin"))
            return;
        if (FS_ReadFile(file, hdr, 12) == 12 && hdr[0] == PG_MAGIC && hdr[1] != 0)
        {
            sSigs = Heap_AllocAtEnd(0, hdr[1] * 8);
            if (sSigs != NULL)
            {
                FS_ReadFile(file, sSigs, hdr[1] * 8);
                sSigCount = hdr[1];
                sRowsAt = 12 + hdr[1] * 8;
            }
        }
        FS_CloseFile(file);
        if (sSigs == NULL)
        {
            sSigs = (u32 *)1;      // (no list: never look again)
            sSigCount = 0;
        }
    }
    lo = 0; hi = sSigCount;
    ent[1] = 0;
    while (lo < hi)
    {
        mid = (lo + hi) / 2;
        if (sSigs[mid * 2] == sig)
        {
            ent[1] = sSigs[mid * 2 + 1];
            break;
        }
        if (sSigs[mid * 2] < sig)
            lo = mid + 1;
        else
            hi = mid;
    }
    if (ent[1] == 0)
        return;
    gRhScreenSig.shown = sig;
    FS_InitFile(file);
    if (!FS_OpenFile(file, "data/rh_amoled_pages.bin"))
        return;
    FS_SeekFile(file, ent[1], 0);
    FS_ReadFile(file, &n, 4);
    mid = n;                         // (the real count, for the tile patches' offset)
    hdr[1] = (sRowsAt - 12) / 8;
    if (n > PG_MAX / 2)
        n = PG_MAX / 2;
    row = n ? Heap_AllocAtEnd(0, sizeof(struct PgRow) * n * 2) : NULL;
    if (row != NULL)
    {
        // each patch: the row it finds (any version) and the version it writes, as two list items
        for (i = 0; i < n; i++)
        {
            FS_SeekFile(file, ent[1] + 4 + i * 8, 0);
            FS_ReadFile(file, pair, 8);
            FS_SeekFile(file, 12 + hdr[1] * 8 + pair[0] * 32, 0);
            FS_ReadFile(file, row[2 * i].c, 32);
            FS_SeekFile(file, 12 + hdr[1] * 8 + pair[1] * 32, 0);
            FS_ReadFile(file, row[2 * i + 1].c, 32);
            row[2 * i].target = 2 * i + 1;
            row[2 * i + 1].target = 2 * i + 1;
            bloom |= 1u << (row[2 * i].c[0] & 31);
        }
        ReplaceRows((u32 *)0x05000000, (u32 *)0x05000800, row, n * 2, bloom);      // palette RAM, both screens
        for (i = 0; i < PG_MAX_BUFS; i++)                                        // the game's palette buffers
            if (sBufs[i].unfaded != NULL)
            {
                ReplaceRows(sBufs[i].unfaded, sBufs[i].unfaded + sBufs[i].size / 4, row, n * 2, bloom);
                ReplaceRows(sBufs[i].faded, sBufs[i].faded + sBufs[i].size / 4, row, n * 2, bloom);
            }
        Heap_Free(row);
    }
    // its tile patches: after the pairs
    FS_SeekFile(file, ent[1] + 4 + mid * 8, 0);
    n = 0;
    if (FS_ReadFile(file, &n, 4) == 4 && n != 0 && n <= 8192)
    {
        sPatch = Heap_AllocAtEnd(0, n * 4);
        if (sPatch != NULL)
        {
            FS_ReadFile(file, sPatch, n * 4);
            sPatchCount = n;
            gRhScreenSig.patches = n;
        }
    }
    // its palette index patches: after the tile patches (what each entry had is kept, to undo them)
    FS_SeekFile(file, ent[1] + 4 + mid * 8 + 4 + n * 4, 0);
    pair[1] = ent[1] + 4 + mid * 8 + 4 + n * 4;
    if (FS_ReadFile(file, &n, 4) == 4 && n != 0 && n <= 1024)
    {
        sIndexUndo = Heap_AllocAtEnd(0, n * 4);
        if (sIndexUndo != NULL)
        {
            FS_ReadFile(file, sIndexUndo, n * 4);      // one read; each entry then becomes its undo record
            for (i = 0; i < n; i++)
            {
                mid = sIndexUndo[i];
                sIndexUndo[i] = (mid & 0x3FF0000) | GetIndex(mid >> 16 & 1023);
                SetIndex(mid >> 16 & 1023, mid & 0x7FFF);
            }
        }
        sIndexCount = sIndexUndo != NULL ? n : 0;
    }
    else if (n > 1024)
        n = 0;
    // its register patches: after the index patches
    FS_SeekFile(file, pair[1] + 4 + n * 4, 0);
    if (FS_ReadFile(file, &n, 4) == 4 && n != 0 && n <= 16)
    {
        sRegs = Heap_AllocAtEnd(0, n * 4);
        if (sRegs != NULL)
        {
            FS_ReadFile(file, sRegs, n * 4);
            sRegCount = n;
        }
    }
    FS_CloseFile(file);
}

static void ThemeTask(void *task, void *data)
{
    u32 theme = RHCT_UiTheme(), sig;
    (void)task; (void)data;
    if (theme != RH_THEME_AMOLED && theme != RH_THEME_IDS)
        return;
    sig = ScreenSig();
    gRhScreenSig.sig = sig;
    if (theme != RH_THEME_AMOLED)
        return;
    if (sig != sLastSig)
    {
        sLastSig = sig;
        if (sPatch != NULL)
        {
            TilePatches(TRUE);
            Heap_Free(sPatch);
            sPatch = NULL;
            sPatchCount = 0;
        }
        if (sIndexUndo != NULL)
        {
            u32 i;
            for (i = sIndexCount; i-- > 0;)
                SetIndex(sIndexUndo[i] >> 16 & 1023, sIndexUndo[i] & 0x7FFF);
            Heap_Free(sIndexUndo);
            sIndexUndo = NULL;
            sIndexCount = 0;
        }
        if (sRegs != NULL)
        {
            Heap_Free(sRegs);
            sRegs = NULL;
            sRegCount = 0;
        }
        ShowScreen(sig);
    }
    TilePatches(FALSE);
    {
        u32 i;
        for (i = 0; i < sRegCount; i++)
            *(vu16 *)(0x04000000 + (sRegs[i] >> 16)) = (u16)sRegs[i];
    }
}

static void ThemePalette(u32 narcId, s32 fileId, u8 *data, u32 fileSize)
{
    if (!sTaskMade)
    {
        sTaskMade = SysTask_CreateOnMainQueue(ThemeTask, NULL, 0) != NULL;
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

static void *Uncompress(void *data, u32 heapId, BOOL atEnd, u32 *sizeOut)
{
    void *out;
    u32 size = *(u32 *)data >> 8;
    if (sizeOut != NULL)
        *sizeOut = size;
    out = atEnd ? Heap_AllocAtEnd(heapId, size) : Heap_Alloc(heapId, size);
    if (out != NULL)
    {
        MI_UncompressLZ8(data, out);
        Heap_Free(data);
    }
    return out;
}

// GfGfxLoader_LoadFromNarc_GetSizeOut (arm9 0x02007AC4)
void *RH_GfxLoadFromNarc_GetSizeOut(u32 narcId, s32 fileId, BOOL isCompressed, u32 heapId, BOOL atEnd, u32 *sizeOut)
{
    u32 size = GetNarcMemberSize(narcId, fileId), real;
    void *data = (isCompressed || atEnd == TRUE) ? Heap_AllocAtEnd(heapId, size) : Heap_Alloc(heapId, size);
    if (sizeOut != NULL)
        *sizeOut = size;
    if (data != NULL)
    {
        ReadWholeNarcMember(data, narcId, fileId);
        real = isCompressed ? *(u32 *)data >> 8 : size;
        if (isCompressed)
            data = Uncompress(data, heapId, atEnd, sizeOut);
        ThemePalette(narcId, fileId, data, real);
    }
    return data;
}

// GfGfxLoader_LoadFromNarc (arm9 0x02007A44)
void *RH_GfxLoadFromNarc(u32 narcId, s32 fileId, BOOL isCompressed, u32 heapId, BOOL atEnd)
{
    return RH_GfxLoadFromNarc_GetSizeOut(narcId, fileId, isCompressed, heapId, atEnd, NULL);
}

// GfGfxLoader_LoadFromOpenNarc_GetSizeOut (arm9 0x02007CAC)
void *RH_GfxLoadFromOpenNarc_GetSizeOut(void *narc, s32 fileId, BOOL isCompressed, u32 heapId, BOOL atEnd, u32 *sizeOut)
{
    u32 size = NARC_GetMemberSize(narc, fileId), real;
    void *data = (isCompressed || atEnd == TRUE) ? Heap_AllocAtEnd(heapId, size) : Heap_Alloc(heapId, size);
    if (sizeOut != NULL)
        *sizeOut = size;
    if (data != NULL)
    {
        NARC_ReadWholeMember(narc, fileId, data);
        real = isCompressed ? *(u32 *)data >> 8 : size;
        if (isCompressed)
            data = Uncompress(data, heapId, atEnd, sizeOut);
        ThemePalette(NARC_ID(narc), fileId, data, real);
    }
    return data;
}

// (GfGfxLoader_LoadFromOpenNarc, arm9 0x02007C98, only calls the _GetSizeOut form)

static void *AllocAndReadWhole(u32 narcId, s32 fileId, u32 heapId, BOOL atEnd)
{
    u32 size = GetNarcMemberSize(narcId, fileId);
    void *data = atEnd ? Heap_AllocAtEnd(heapId, size) : Heap_Alloc(heapId, size);
    if (data != NULL)
    {
        ReadFromNarcInternal(data, sNarcFileList[narcId], fileId, 0, size);
        ThemePalette(narcId, fileId, data, size);
    }
    return data;
}

// AllocAndReadWholeNarcMemberByIdPair (arm9 0x02007524)
void *RH_AllocAndReadWholeNarcMember(u32 narcId, s32 fileId, u32 heapId)
{
    return AllocAndReadWhole(narcId, fileId, heapId, FALSE);
}

// AllocAtEndAndReadWholeNarcMemberByIdPair (arm9 0x02007540)
void *RH_AllocAtEndAndReadWholeNarcMember(u32 narcId, s32 fileId, u32 heapId)
{
    return AllocAndReadWhole(narcId, fileId, heapId, TRUE);
}

// NARC_AllocAndReadWholeMember (arm9 0x0200771C)
void *RH_NARC_AllocAndReadWholeMember(void *narc, s32 fileId, u32 heapId)
{
    u32 size = NARC_GetMemberSize(narc, fileId);
    void *data = Heap_Alloc(heapId, size);
    if (data != NULL)
    {
        NARC_ReadWholeMember(narc, fileId, data);
        ThemePalette(NARC_ID(narc), fileId, data, size);
    }
    return data;
}

// NARC_New (arm9 0x02007688), the same plus the archive's id in the struct's spare bytes
void *RH_NARC_New(u32 narcId, u32 heapId)
{
    u8 *narc = Heap_Alloc(heapId, NARC_SIZE);
    u32 chunk, btnf;
    if (narc != NULL)
    {
        *(u32 *)(narc + 0x48) = 0;
        FS_InitFile(narc);
        FS_OpenFile(narc, sNarcFileList[narcId]);
        FS_SeekFile(narc, 12, 0);
        FS_ReadFile(narc, narc + 0x48, 2);
        FS_SeekFile(narc, *(u32 *)(narc + 0x48) + 4, 0);
        FS_ReadFile(narc, &chunk, 4);
        FS_ReadFile(narc, narc + 0x50, 2);
        btnf = *(u32 *)(narc + 0x48) + chunk;
        FS_SeekFile(narc, btnf + 4, 0);
        FS_ReadFile(narc, &chunk, 4);
        *(u32 *)(narc + 0x4C) = btnf + chunk;
        NARC_ID(narc) = (u16)narcId;
    }
    return narc;
}

// ReadFromNarcMemberByIdPair (arm9 0x02007560): part of a file (size 0: all of it, themed by the loaders above)
void RH_ReadFromNarcMemberByIdPair(void *dest, u32 narcId, s32 fileId, u32 offset, u32 size)
{
    ReadFromNarcInternal(dest, sNarcFileList[narcId], fileId, offset, size);
    ThemeRead(narcId, fileId, offset, size, dest, FALSE, 0);
}

// AllocAndReadFromNarcMemberByIdPair (arm9 0x0200757C) and its AllocAtEnd form (0x0200759C)
void *RH_AllocAndReadFromNarcMemberByIdPair(u32 narcId, s32 fileId, u32 heapId, u32 offset, u32 size)
{
    void *data = AllocReadFromNarcInternal(sNarcFileList[narcId], fileId, heapId, offset, size, FALSE);
    if (data != NULL)
        ThemeRead(narcId, fileId, offset, size, data, FALSE, 0);
    return data;
}

void *RH_AllocAtEndAndReadFromNarcMemberByIdPair(u32 narcId, s32 fileId, u32 heapId, u32 offset, u32 size)
{
    void *data = AllocReadFromNarcInternal(sNarcFileList[narcId], fileId, heapId, offset, size, TRUE);
    if (data != NULL)
        ThemeRead(narcId, fileId, offset, size, data, FALSE, 0);
    return data;
}

// NARC_ReadFromMember (arm9 0x0200782C): part of a file of an open NARC
void RH_NARC_ReadFromMember(void *narc, s32 fileId, u32 pos, u32 size, void *dst)
{
    u32 start;
    FS_SeekFile(narc, *(u32 *)((u8 *)narc + 0x48) + 12 + fileId * 8, 0);
    FS_ReadFile(narc, &start, 4);
    FS_SeekFile(narc, *(u32 *)((u8 *)narc + 0x4C) + 8 + start + pos, 0);
    FS_ReadFile(narc, dst, size);
    ThemeRead(NARC_ID(narc), fileId, pos, size, dst, FALSE, 0);
}
