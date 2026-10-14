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

#if RH_TEST
// Colour-id mode (test only; tools/rh/amoled reads it): each palette read takes slots of 256 colours, colour =
// slot << 8 | index, and the log says which archive / file / part of 256 each slot is. The capture tools match
// screenshots taken this way with the owner's edited ones.
static const u16 sNotIded[] = { 4, 114, 194, 195, 18, 20, 69, 46, 174, 264, 81 };   // Pokemon / item / title art
struct RhPalLog { u32 magic, count; u16 narc[64], file[64]; };
struct RhPalLog gRhPalLog = { 0x52485044 /* "RHPD" */, 0, {0}, {0} };

// colours [first, first + n) of a palette file
static void IdColours(u32 narcId, s32 fileId, u32 first, u16 *colors, u32 n)
{
    u32 i, slot = 0;
    for (i = 0; i < NELEMS(sNotIded); i++)
        if (narcId == sNotIded[i] || narcId == NARC_UNKNOWN)
            return;
    for (i = 0; i < n; i++)
    {
        if (i == 0 || ((first + i) & 255) == 0)
        {
            slot = gRhPalLog.count++ & 63;
            gRhPalLog.narc[slot] = (u16)narcId;
            gRhPalLog.file[slot] = (u16)(fileId | ((first + i) >> 8) << 12);
        }
        colors[i] = (u16)(slot << 8 | ((first + i) & 255));
    }
}
#endif

// data/rh_amoled.bin (tools/rh/gen_rh_amoled.py): "RHA2", entry count, a bit mask of the archives in it (320 bits),
// then the entries { u32 narc << 16 | file, u32 offset } sorted, each offset pointing at { u32 colour count,
// u32 where the colours start in the palette file, colours }. Every palette file of the UI archives is in it.
#define AM_MAGIC   0x32414852
#define AM_HEADER  48
static u32 sArcMask[10];
static BOOL sArcMaskLoaded;

// Opens the table and looks (narc, file) up: the file position of its colours (the table stays open), or 0 (closed)
static u32 AmoledFind(u32 *file, u32 narcId, s32 fileId, u32 *start, u32 *count)
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
            FS_SeekFile(file, AM_HEADER + mid * 8, 0);
            FS_ReadFile(file, hdr, 8);
            if (hdr[0] == key)
            {
                FS_SeekFile(file, hdr[1], 0);
                FS_ReadFile(file, hdr + 2, 8);
                *count = hdr[2];
                *start = hdr[3];
                return hdr[1] + 8;
            }
            if (hdr[0] < key)
                lo = mid + 1;
            else
                hi = mid;
        }
    }
    FS_CloseFile(file);
    return 0;
}

// A read of bytes [offset, offset + size) of a palette file landed at dst: swap the colours in it for the theme's.
// (Whole files come here too, offset 0.) Some screens read only a few colours of a palette file, e.g. the battle
// move buttons' type colours.
static void ThemeRead(u32 narcId, s32 fileId, u32 offset, u32 size, u8 *dst)
{
    u32 file[FSFILE_SIZE / 4];      // FSFile (word aligned)
    u32 theme = RHCT_UiTheme(), pos, start, count, a, b;
    if (size == 0 || (theme != RH_THEME_AMOLED && theme != RH_THEME_IDS))
        return;
    pos = AmoledFind(file, narcId, fileId, &start, &count);
    if (pos == 0)
        return;
    a = offset > start ? offset : start;
    b = offset + size < start + count * 2 ? offset + size : start + count * 2;
    if (a < b && !((a - start) & 1))
    {
#if RH_TEST
        if (theme == RH_THEME_IDS)
            IdColours(narcId, fileId, (a - start) / 2, (u16 *)(dst + a - offset), (b - a) / 2);
        else
#endif
        {
            FS_SeekFile(file, pos + a - start, 0);
            FS_ReadFile(file, dst + a - offset, (b - a) & ~1u);
        }
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

static void ThemePalette(u32 narcId, s32 fileId, u8 *data, u32 fileSize)
{
    u32 theme, size, start;
    if (data == NULL || data[0] != 'R' || data[1] != 'L' || data[2] != 'C' || data[3] != 'N')
        return;
    theme = RHCT_UiTheme();
    if (theme != RH_THEME_AMOLED && theme != RH_THEME_IDS)
        return;                                              // Default (and Randomizer, for now): untouched
    // NCLR: TTLP section at 0x10; its data at 0x18: format, extPltt, size, offset of the colours (from 0x18)
    // (the size field is often bigger than the file: never write past its end)
    size = *(u32 *)(data + 0x20);
    start = 0x18 + *(u32 *)(data + 0x24);
    if (fileSize < 0x28 || start >= fileSize)
        return;
    if (size > fileSize - start)
        size = fileSize - start;
#if RH_TEST
    if (theme == RH_THEME_IDS)
    {
        IdColours(narcId, fileId, 0, (u16 *)(data + start), size / 2);
        return;
    }
#endif
    ThemeRead(narcId, fileId, start, size, data + start);
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
    ThemeRead(narcId, fileId, offset, size, dest);
}

// AllocAndReadFromNarcMemberByIdPair (arm9 0x0200757C) and its AllocAtEnd form (0x0200759C)
void *RH_AllocAndReadFromNarcMemberByIdPair(u32 narcId, s32 fileId, u32 heapId, u32 offset, u32 size)
{
    void *data = AllocReadFromNarcInternal(sNarcFileList[narcId], fileId, heapId, offset, size, FALSE);
    if (data != NULL)
        ThemeRead(narcId, fileId, offset, size, data);
    return data;
}

void *RH_AllocAtEndAndReadFromNarcMemberByIdPair(u32 narcId, s32 fileId, u32 heapId, u32 offset, u32 size)
{
    void *data = AllocReadFromNarcInternal(sNarcFileList[narcId], fileId, heapId, offset, size, TRUE);
    if (data != NULL)
        ThemeRead(narcId, fileId, offset, size, data);
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
    ThemeRead(NARC_ID(narc), fileId, pos, size, dst);
}
