// Romhack: UI Theme (Misc. Tweaks: Default / Randomizer / AMOLED), port of the FireRed/Emerald rh_theme.c idea.
// Every palette file (NCLR) the game's graphics loader reads (GfGfxLoader_LoadFromNarc / LoadFromOpenNarc and their
// _GetSizeOut forms; LoadFromOpenNarc goes through its _GetSizeOut, hooked here) is re-lit as it loads, before anything is drawn: screen fades (master brightness)
// and the PaletteData fades then work on the themed colours, so transitions and animations stay right.
// Pokemon / trainer / item art and the title / intro are left alone (sNotThemed). Default = never touched.
// AMOLED (placeholder, until the owner's own edits): see AmoledColor.
// NARC_New is hooked too, to remember which archive an open NARC is (the spare bytes 0x52-0x53 of the struct).
#include "../include/types.h"
#include "../include/rh/rh_coretable.h"

#define RH_THEME_DEFAULT    0
#define RH_THEME_RANDOMIZER 1
#define RH_THEME_AMOLED     2

// ---- game functions (arm9) ----
#define Heap_Alloc            ((void *(*)(u32, u32))(0x0201AA8C | 1))
#define Heap_AllocAtEnd       ((void *(*)(u32, u32))(0x0201AACC | 1))
#define Heap_Free             ((void (*)(void *))(0x0201AB0C | 1))
#define GetNarcMemberSize     ((u32 (*)(u32, s32))(0x020075BC | 1))
#define ReadWholeNarcMember   ((void (*)(void *, u32, s32))(0x02007508 | 1))   // hooked: RH_ReadWholeNarcMemberByIdPair
#define NARC_GetMemberSize    ((u32 (*)(void *, s32))(0x020077E8 | 1))
#define ReadFromNarcMember    ((void (*)(void *, u32, s32, u32, u32))(0x02007560 | 1))
#define NARC_ReadWholeMember  ((void (*)(void *, s32, void *))(0x0200778C | 1))
#define MI_UncompressLZ8      ((void (*)(const void *, void *))0x020D4BE8)          // ARM
#define FS_InitFile           ((void (*)(void *))0x020D7F98)                        // ARM
#define FS_OpenFile           ((BOOL (*)(void *, const char *))0x020D8278)          // ARM
#define FS_ReadFile           ((s32 (*)(void *, void *, s32))0x020D83CC)            // ARM
#define FS_SeekFile           ((BOOL (*)(void *, s32, u32))0x020D83DC)              // ARM
#define sNarcFileList         ((const char *const *)0x0210F210)

#define NARC_SIZE    0x54
#define NARC_ID(n)   (*(u16 *)((u8 *)(n) + 0x52))     // romhack: spare struct bytes
#define NARC_UNKNOWN 0xFFFF

// Archives whose palettes are art, not UI (pret include/filesystem_files_def.h)
static const u16 sNotThemed[] = {
    4,      // poketool/pokegra/pokegra: Pokemon sprites
    114,    // poketool/pokegra/otherpoke
    194, 195,   // pbr/pokegra, pbr/otherpoke
    18,     // item icons
    20,     // Pokemon icons
    69,     // footprints
    8,      // trainer pictures (the player in the bag too)
    46,     // title screen
    120,    // Prof. Oak's intro (but see sThemedFiles)
    174,    // legend demo
    264,    // opening
    81,     // overworld models
};

// Debug log (read by the test harness): the last palettes themed / seen
struct RhPalLog { u32 magic, count; u16 narc[64], file[64]; };
struct RhPalLog gRhPalLog = { 0x52485044 /* "RHPD" */, 0, {0}, {0} };

#define NARC_FONT_PALETTES 16    // a/0/1/6: the text colours (coloured text keeps its colour)

// UI files inside art archives (narc, file): themed anyway
static const u16 sThemedFiles[][2] = {
    { 120, 1 }, { 120, 30 },    // Prof. Oak's intro: the backgrounds
};

static BOOL Themed(u32 narcId, s32 fileId)
{
    u32 i;
    if (narcId == NARC_UNKNOWN)
        return FALSE;
    for (i = 0; i < NELEMS(sThemedFiles); i++)
        if (sThemedFiles[i][0] == narcId && sThemedFiles[i][1] == fileId)
            return TRUE;
    for (i = 0; i < NELEMS(sNotThemed); i++)
        if (sNotThemed[i] == narcId)
            return FALSE;
    return TRUE;
}

// AMOLED (placeholder, owner's rule: text white, text shadow grey, not a plain inversion):
// greys by lightness: white / light backgrounds -> black, mid greys (text shadows, frames) -> grey,
// dark greys (text) -> white, near-black outlines kept; pale tinted panels -> very dark, same hue;
// bright coloured panels / buttons much darker, same hue; vivid colours (coloured text, accents) a bit darker;
// the font palettes' colours kept.
static u16 AmoledColor(u16 c, BOOL font)
{
    u32 r = c & 31, g = (c >> 5) & 31, b = (c >> 10) & 31;
    u32 mx = r > g ? (r > b ? r : b) : (g > b ? g : b);
    u32 mn = r < g ? (r < b ? r : b) : (g < b ? g : b);
    if (mx - mn <= 4)
    {
        if (mx >= 28)
            r = g = b = 0;                                   // background
        else if (mx >= 17)
            r = g = b = 13;                                  // text shadow / frame
        else if (mx >= 5)
            r = g = b = 31;                                  // text
    }
    else if (mn >= 12)
    {
        r = r / 5; g = g / 5; b = b / 5;                     // pale panel
    }
    else if (!font && mx >= 20 && mx - mn < 20)
    {
        r = r * 3 / 10; g = g * 3 / 10; b = b * 3 / 10;      // bright coloured panel
    }
    else if (!font && mx >= 20)
    {
        r = r * 6 / 10; g = g * 6 / 10; b = b * 6 / 10;      // vivid: darker, still readable as coloured text
    }
    return (u16)(r | (g << 5) | (b << 10) | (c & 0x8000));
}

static void ThemePalette(u32 narcId, s32 fileId, u8 *data, u32 fileSize)
{
    u32 theme, size, start, i;
    u16 *colors;
    if (data == NULL || data[0] != 'R' || data[1] != 'L' || data[2] != 'C' || data[3] != 'N')
        return;
    gRhPalLog.narc[gRhPalLog.count & 63] = (u16)narcId;
    gRhPalLog.file[gRhPalLog.count & 63] = (u16)fileId;
    gRhPalLog.count++;
    theme = RHCT_UiTheme();
    if (theme != RH_THEME_AMOLED || !Themed(narcId, fileId))
        return;                                              // Default (and Randomizer, for now): untouched
    // NCLR: TTLP section at 0x10; its data at 0x18: format, extPltt, size, offset of the colours (from 0x18)
    // (the size field is often bigger than the file: never write past its end)
    size = *(u32 *)(data + 0x20);
    start = 0x18 + *(u32 *)(data + 0x24);
    if (fileSize < 0x28 || start >= fileSize)
        return;
    if (size > fileSize - start)
        size = fileSize - start;
    colors = (u16 *)(data + start);
    for (i = 0; i < size / 2; i++)
        colors[i] = AmoledColor(colors[i], narcId == NARC_FONT_PALETTES);
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
        ReadFromNarcMember(data, narcId, fileId, 0, size);
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
