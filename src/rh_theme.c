// Romhack: UI Theme (Misc. Tweaks: Default / Randomizer / AMOLED), port of the FireRed/Emerald rh_theme.c idea.
// Every palette file (NCLR) the game reads through the graphics loader (GfGfxLoader_LoadFromNarc / LoadFromOpenNarc
// and their _GetSizeOut forms) or the whole-member readers (AllocAndReadWholeNarcMemberByIdPair, its AllocAtEnd form,
// NARC_AllocAndReadWholeMember) is swapped for its themed version as it loads, before anything is drawn: screen fades
// (master brightness) and the PaletteData fades then work on the themed colours, so transitions and animations stay right.
// AMOLED: the owner's own designs ("UI screenshots - AMOLED"), turned into whole replacement palettes by
// tools/rh/gen_rh_amoled.py and stored in the ROM file data/rh_amoled.bin; palettes not in it stay as they are.
// Default = never touched.
// NARC_New is hooked too, to remember which archive an open NARC is (the spare bytes 0x52-0x53 of the struct).
// The code that themes (rhtheme/, data/rh_theme.bin) is loaded onto the system heap only while the theme is AMOLED:
// this file is what stays loaded (see include/rh/rh_theme_blob.h).
#include "../include/types.h"
#include "../include/rh/rh_coretable.h"
#include "../include/rh/rh_test.h"
#include "../include/rh/rh_theme_blob.h"

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

// The game's palette buffers (PaletteData_AllocBuffers / _FreeBuffers, hooked below): the live copies of the
// palettes besides palette RAM.
#define PaletteData_SetBuffers ((void (*)(void *, u32, void *, void *, u32))(0x0200310C | 1))
struct RhPalBuf gRhBufs[PG_MAX_BUFS];
#define sBufs gRhBufs

// a tracked buffer the game still uses (its PaletteData still points at it): never write into freed memory
BOOL RH_BufLive(u32 i)
{
    return sBufs[i].unfaded != NULL && *(u32 **)(sBufs[i].pd + sBufs[i].id * 20) == sBufs[i].unfaded
        && *(u32 **)(sBufs[i].pd + sBufs[i].id * 20 + 4) == sBufs[i].faded;
}
#define BufLive RH_BufLive

// heap 0 (the system heap) only when the game keeps plenty: a starved heap 0 can hang the game. (Measured
// 2026-11-04, test builds' gRhHeap0: with the Default theme the game's own use of heap 0 hardly moves - battles,
// menus, saving: under 100 bytes - and about 47-52 KB stay free; 20 KB kept for it)
#define GF_ExpHeap_FndGetTotalFreeSize_ ((u32 (*)(u32))(0x0201ABEC | 1))
void *RH_ThemeSafeAlloc(u32 size)
{
    if (GF_ExpHeap_FndGetTotalFreeSize_(0) < size + 0x5000)
        return NULL;
    return Heap_AllocAtEnd(0, size);
}
#define SafeAlloc RH_ThemeSafeAlloc

// PaletteData_AllocBuffers (arm9 0x02003120)
void RH_PaletteData_AllocBuffers(void *plttData, u32 bufferId, u32 size, u32 heapId)
{
    void *unfaded = Heap_Alloc(heapId, size), *faded = Heap_Alloc(heapId, size);
    u32 i;
    PaletteData_SetBuffers(plttData, bufferId, unfaded, faded, size);
    for (i = 0; i < PG_MAX_BUFS; i++)
        if (sBufs[i].unfaded == NULL || !BufLive(i))
        {
            sBufs[i].unfaded = unfaded; sBufs[i].faded = faded; sBufs[i].size = size; sBufs[i].id = bufferId;
            sBufs[i].pd = plttData;
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

// the colour an entry has now: from the game's unfaded palette buffer when there is one (palette RAM may be mid-fade);
// GetBuf(idx, TRUE): from its faded buffer (what the game's fades copy into palette RAM)
u32 GetBuf(u32 idx, BOOL faded)          // (also called by the battle overlay: src/battle/rh_hptext.c)
{
    static const u8 sBufFor[4] = { 0, 2, 1, 3 };
    u32 i, id = sBufFor[idx >> 8], off = idx & 255;
    for (i = 0; i < PG_MAX_BUFS; i++)
        if (BufLive(i) && sBufs[i].id == id && off * 2 < sBufs[i].size)
            return ((u16 *)(faded ? sBufs[i].faded : sBufs[i].unfaded))[off];
    return ((vu16 *)0x05000000)[idx];
}

// an entry's colour, in palette RAM and the game's buffers (also called by the battle overlay)
void SetIndex(u32 idx, u32 c)
{
    static const u8 sBufFor[4] = { 0, 2, 1, 3 };
    u32 i, id = sBufFor[idx >> 8], off = idx & 255;
    ((vu16 *)0x05000000)[idx] = (u16)c;
    for (i = 0; i < PG_MAX_BUFS; i++)
        if (BufLive(i) && sBufs[i].id == id && off * 2 < sBufs[i].size)
        {
            ((u16 *)sBufs[i].unfaded)[off] = (u16)c;
            ((u16 *)sBufs[i].faded)[off] = (u16)c;
        }
}

// The rest of the theme (data/rh_theme.bin), loaded the first time it is needed while the theme is AMOLED, and kept.
// It goes where the system heap has room: its words holding its own addresses get the difference added.
#define DC_FlushRange         ((void (*)(void *, u32))0x020D2894)                   // ARM
#define IC_InvalidateRange    ((void (*)(void *, u32))0x020D28D0)                   // ARM
static const struct RhThemeBlob *sBlob;

static const struct RhThemeBlob *Blob(void)
{
    u32 file[FSFILE_SIZE / 4], hdr[4], rel[32], i, k, n, theme = RHCT_UiTheme();
    u8 *base;
    if (theme != RH_THEME_AMOLED && theme != RH_THEME_IDS)
        return NULL;
    if (sBlob != NULL)
        return sBlob;
    FS_InitFile(file);
    if (!FS_OpenFile(file, "data/rh_theme.bin"))
        return NULL;
    base = NULL;
    if (FS_ReadFile(file, hdr, 16) == 16 && hdr[0] == RHTB_MAGIC && (base = SafeAlloc(hdr[2])) != NULL)
    {
        FS_ReadFile(file, base, hdr[1]);
        for (i = hdr[1]; i < hdr[2]; i++)
            base[i] = 0;
        for (n = hdr[3]; n != 0; n -= k)
        {
            k = n < 32 ? n : 32;
            FS_ReadFile(file, rel, k * 4);
            for (i = 0; i < k; i++)
                *(u32 *)(base + rel[i]) += (u32)base - RHTB_BASE;
        }
        DC_FlushRange(base, hdr[2]);
        IC_InvalidateRange(base, hdr[2]);
        sBlob = (const struct RhThemeBlob *)base;
    }
    FS_CloseFile(file);
    return sBlob;
}

static void ThemePalette(u32 narcId, s32 fileId, u8 *data, u32 fileSize)
{
    const struct RhThemeBlob *b = Blob();
    if (b != NULL)
        b->palette(narcId, fileId, data, fileSize);
}

static void ThemeRead(u32 narcId, s32 fileId, u32 offset, u32 size, u8 *dst, BOOL whole, u32 ctx)
{
    const struct RhThemeBlob *b = Blob();
    if (b != NULL)
        b->read(narcId, fileId, offset, size, dst, whole, ctx);
}

// GX_LoadOAM (arm9 0x020CFDC8) and the sprite library's main-screen OAM loader (0x020B78C4, through a pointer table),
// both (source, offset, size), hooked (armhooks): the copy (16-bit: it can be a single entry's attribute), then the
// battle HP-box text overlays back in (src/battle/rh_hptext.c), right after every copy of the game's sprites
#if RH_TEST
// test builds: the system heap's smallest free size seen (every frame, any theme: the tools compare the themes)
struct RhHeap0 { u32 magic, min, now; };
struct RhHeap0 gRhHeap0;     // (overlay 129 has no initialised data: the magic "RHH0" is set on the first frame)
#endif
static void LoadOam(const u32 *src, u32 offset, u32 size, u32 part)
{
    const u32 *hp = (const u32 *)0x023C4000;
    vu16 *dst = (vu16 *)(0x07000000 + offset);
    const u16 *from = (const u16 *)src;
    for (size >>= 1; size != 0; size--)
        *dst++ = *from++;
#if RH_TEST
    {
        u32 *handles = *(u32 **)0x021D1584;
        u8 *idxs = *(u8 **)(0x021D1584 + 0x10);
        gRhHeap0.magic = 0x30484852;
        if (handles != NULL && idxs != NULL && handles[idxs[0]] != 0)
        {
            gRhHeap0.now = GF_ExpHeap_FndGetTotalFreeSize_(0);
            if (gRhHeap0.min == 0 || gRhHeap0.now < gRhHeap0.min)
                gRhHeap0.min = gRhHeap0.now;
        }
    }
#endif
    if (RHCT_UiTheme() == RH_THEME_AMOLED && hp[0] == 0x54504852 && hp[1] == 0x31545854)
    {
        // (a copy of only some sprites may have overwritten an overlay: RH_HpSync says so, then they go back in)
        if (((u32 (*)(void))hp[part])() && part == 4)
            ((void (*)(void))hp[3])();
    }
    if (sBlob != NULL && RHCT_UiTheme() == RH_THEME_AMOLED)
        sBlob->oam(offset, size);
}

void RH_LoadOamCopy(const u32 *src, u32 offset, u32 size)
{
    LoadOam(src, offset, size, 4);     // (only the overlays' positions: see src/battle/rh_hptext.c RH_HpSync)
}

void RH_LoadOamMain(const u32 *src, u32 offset, u32 size)
{
    LoadOam(src, offset, size, 3);
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
