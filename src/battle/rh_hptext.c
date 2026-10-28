// romhack: AMOLED theme, battle HP boxes. The owner's design gives the box text (name, gender, level, HP numbers)
// white with a grey shadow, and the box art itself other colours in the same palette entries (text 14 = the frame's
// dark grey, shadow 2 = the body's near-black): 18 colours, more than the box's one 16-colour palette row holds.
// So every frame, right after the game copied its sprites to the hardware (the theme's task, vwait queue), each HP-box
// sprite gets an overlay sprite in front of it: a copy of only its text pixels, with a palette row of its own (text
// white, shadow grey). Text pixels = the ones in entries 14/2 where the box art has neither (whatever the game drew:
// any name, gender, level, number), plus the art's own text (the "/" between the HP numbers).
// Called by the theme (src/rh_theme.c, AMOLED only) through the entry at the start of this overlay.
#include "../../include/types.h"

#define FS_InitFile   ((void (*)(void *))0x020D7F98)                 // ARM
#define FS_OpenFile   ((BOOL (*)(void *, const char *))0x020D8278)   // ARM
#define FS_CloseFile  ((BOOL (*)(void *))0x020D82C0)                 // ARM
#define FS_ReadFile   ((s32 (*)(void *, void *, s32))0x020D83CC)     // ARM
#define FS_SeekFile   ((BOOL (*)(void *, s32, u32))0x020D83DC)       // ARM
#define FSFILE_SIZE   0x48

#define OAM       ((vu16 *)0x07000000)
#define OBJ_PAL   ((vu16 *)0x05000200)
#define OBJ_VRAM  0x06400000
#define OBJ_TOP   0x10000                 // main OBJ VRAM in battle: bank E, 64 KB
#define MAX_PIECES 8                      // HP-box sprites (64x64): 2 per box, up to 4 boxes (double battles)
#define MAX_ART    8
#define TEXT_COLOUR   0x7FFF              // owner: white text
#define SHADOW_COLOUR 0x2D6B              // owner: (11,11,11) shadow
#define NO_ART     0xFF

// the box art's masks (data/rh_hpart.bin, made by tools/rh/gen_rh_amoled.py from the narc 8 gauge sprites): per sprite
// half, 1 = an art pixel of entry 14/2 that is not text. The halves in use are kept for the rest of the battle.
static struct { u32 key; u8 mask[512]; } sArt[MAX_ART];
static u32 sArtN;
static struct { u32 sum; u16 tile; u8 art; } sPiece[MAX_PIECES];

// A 64x64 sprite's tile row (8 pixels, one word of its graphics) and the masks' byte for the same 8 pixels: tile t,
// row r is word t * 8 + r and mask byte (t / 8 * 8 + r) * 8 + t % 8. Bit q: pixel q of the row (low nibble first).
#define MASK_BYTE(t, r) ((((t) >> 3) * 8 + (r)) * 8 + ((t) & 7))

// the row's pixels in entry 14 or 2 (1110 / 0010: bit 1 set, bit 0 clear, bits 2 and 3 equal), one bit each
static u32 Bits14or2(u32 w)
{
    u32 c = (w >> 1) & ~w & ~((w >> 2) ^ (w >> 3)) & 0x11111111;
    c = (c | c >> 3) & 0x03030303;
    c = (c | c >> 6) & 0x000F000F;
    return (c | c >> 12) & 0xFF;
}

// back: 8 bits -> their pixels' nibbles all set
static u32 Nibbles(u32 b)
{
    b = (b | b << 12) & 0x000F000F;
    b = (b | b << 6) & 0x03030303;
    return ((b | b << 3) & 0x11111111) * 15;
}

static u32 Count8(u32 x)
{
    x = x - ((x >> 1) & 0x55);
    x = (x & 0x33) + ((x >> 2) & 0x33);
    return (x + (x >> 4)) & 0x0F;
}

// how well a half's mask fits a box sprite: its art pixels shown in 14/2 (the theme's tile patches may have moved a
// few to spare entries). A row at a time: this runs in the vertical blank when the boxes appear
static s32 Score(const u8 *mask, const u8 *chr)
{
    const u32 *w = (const u32 *)chr;
    s32 s = 0;
    u32 t, r;
    for (t = 0; t < 64; t++)
        for (r = 0; r < 8; r++)
        {
            u32 m = mask[MASK_BYTE(t, r)];
            if (m != 0)
                s += 2 * (s32)Count8(m & Bits14or2(w[t * 8 + r])) - (s32)Count8(m);
        }
    return s;
}

// the art of the box sprites without one: every half's mask scored against them, the best kept
static void MatchArt(const u8 *const *chr, u32 n)
{
    static u8 sMask[512];
    u32 file[FSFILE_SIZE / 4], hdr[2], i, k, a, bestKey[MAX_PIECES], bestAt[MAX_PIECES];
    s32 best[MAX_PIECES];
    for (k = 0; k < n; k++)
    {
        best[k] = 0;
        bestKey[k] = 0;
    }
    FS_InitFile(file);
    if (!FS_OpenFile(file, "data/rh_hpart.bin"))
        return;
    // every half scored; then only the winners' masks read again (by position: 8 + index * 516)
    if (FS_ReadFile(file, hdr, 8) == 8 && hdr[0] == 0x50484852)
        for (i = 0; i < hdr[1]; i++)
        {
            u32 key;
            if (FS_ReadFile(file, &key, 4) != 4 || FS_ReadFile(file, sMask, 512) != 512)
                break;
            for (k = 0; k < n; k++)
            {
                s32 sc;
                if (sPiece[k].art == NO_ART && (sc = Score(sMask, chr[k])) > best[k])
                {
                    best[k] = sc;
                    bestKey[k] = key;
                    bestAt[k] = 8 + i * 516 + 4;
                }
            }
        }
    for (k = 0; k < n; k++)
    {
        if (sPiece[k].art != NO_ART || best[k] <= 0)
            continue;
        for (a = 0; a < sArtN && sArt[a].key != bestKey[k]; a++)
            ;
        if (a == sArtN)
        {
            if (sArtN == MAX_ART)
                continue;
            FS_SeekFile(file, bestAt[k], 0);
            if (FS_ReadFile(file, sArt[a].mask, 512) != 512)
                continue;
            sArt[a].key = bestKey[k];
            sArtN++;
        }
        sPiece[k].art = (u8)a;
    }
    FS_CloseFile(file);
}

static BOOL IsHpRow(u32 r)
{
    return (OBJ_PAL[r * 16 + 5] & 0x7FFF) == 0x0240 && (OBJ_PAL[r * 16 + 6] & 0x7FFF) == 0x1303
        && (OBJ_PAL[r * 16 + 8] & 0x7FFF) == 0x02DF;
}

static BOOL Free(u32 j)
{
    return (OAM[j * 4] & 0x300) == 0x200 || (OAM[j * 4] & 255) == 192;
}

#define OVL_SLOT(k) ((OBJ_TOP - ((k) + 1) * 0x800) >> shift)    // the overlay of box sprite k: its first tile

// the shown HP-box sprites (not the overlays): their OAM indices and graphics; the palette rows shown sprites use,
// and whether any sprite has graphics where the overlays go
static u32 Boxes(u32 shift, u16 *idx, const u8 **chr, u32 *used)
{
    u32 i, n = 0, high = 0;
    *used = 0;
    for (i = 0; i < 128; i++)
    {
        u32 a0 = OAM[i * 4], a1 = OAM[i * 4 + 1], a2 = OAM[i * 4 + 2];
        if ((a0 & 0x300) == 0x200)
            continue;
        if ((a2 & 1023) >= OVL_SLOT(MAX_PIECES - 1) && (a2 & 1023) <= OVL_SLOT(0) && (a0 & 255) != 192)
            continue;                                         // (an overlay already in)
        *used |= 1 << (a2 >> 12);
        if ((a2 & 1023) << shift > high)
            high = (a2 & 1023) << shift;
        if ((a0 & 0x100) || (a0 & 0x2000) || a0 >> 14 != 0 || a1 >> 14 != 3 || !IsHpRow(a2 >> 12) || n >= MAX_PIECES)
            continue;
        idx[n] = i;
        chr[n] = (const u8 *)(OBJ_VRAM + ((a2 & 1023) << shift));
        n++;
    }
    return high >= OBJ_TOP - MAX_PIECES * 0x800 ? 0 : n;      // (sprites where the overlays would go: none)
}

static u32 sRow;
static u8 sPut[MAX_PIECES];          // 1 + the OAM index of each box sprite's overlay (0: none)

// each overlay goes in front of its box sprite: a free OAM entry with a lower index (drawn on top). Done after every
// copy of the game's sprites to the hardware, so harmless again: a box whose overlay is already in is left alone.
static u32 SlotOf(u32 tile)
{
    u32 s;
    for (s = 0; s < MAX_PIECES && !(sPiece[s].tile == tile && sPiece[s].art != NO_ART && sPiece[s].sum); s++)
        ;
    return s;
}

static void Insert(u32 shift, const u16 *idx, u32 n)
{
    u32 k, j, s;
    // overlays left from earlier frames (a box that moved or went away): hidden
    for (j = 0; j < 128; j++)
    {
        u32 t = OAM[j * 4 + 2] & 1023;
        if ((OAM[j * 4] & 0x300) == 0x200 || (OAM[j * 4] & 255) == 192 || t < OVL_SLOT(MAX_PIECES - 1) || t > OVL_SLOT(0))
            continue;
        for (k = 0; k < n; k++)
            if (OAM[idx[k] * 4] == OAM[j * 4] && OAM[idx[k] * 4 + 1] == OAM[j * 4 + 1]
                && OVL_SLOT(SlotOf(OAM[idx[k] * 4 + 2] & 1023)) == t && j < idx[k])
                break;
        if (k == n)
            OAM[j * 4] = 0x200;
    }
    for (k = 0; k < n; k++)
    {
        u32 at = idx[k], a0 = OAM[at * 4], a1 = OAM[at * 4 + 1], a2 = OAM[at * 4 + 2];
        // (its overlay by the box sprite's graphics, not by order: a copy may hold only some of the sprites)
        s = SlotOf(a2 & 1023);
        if (s == MAX_PIECES || sRow == 0)
            continue;
        for (j = 0; j < at; j++)
            if ((OAM[j * 4 + 2] & 1023) == OVL_SLOT(s) && OAM[j * 4] == a0 && OAM[j * 4 + 1] == a1)
                break;
        if (j < at)
            continue;
        for (j = 0; j < at && !Free(j); j++)
            ;
        if (j == at)
            continue;
        OAM[j * 4] = (u16)a0;
        OAM[j * 4 + 1] = (u16)a1;
        OAM[j * 4 + 2] = (u16)((a2 & 0x0C00) | sRow << 12 | OVL_SLOT(s));
        sPut[s] = (u8)(j + 1);
    }
}

// every frame after the game's vertical-blank copies (theme task, vwait queue): find the boxes' art, copy their text
// into the overlays' graphics, put the overlays in
void RH_HpText(void)
{
    u32 dispcnt = *(vu32 *)0x04000000, shift = 5 + (dispcnt >> 20 & 3), i, k, n, used, search = 0;
    u16 idx[MAX_PIECES];
    const u8 *chr[MAX_PIECES];
    if (!(dispcnt & 0x10) || shift < 6 || (n = Boxes(shift, idx, chr, &used)) == 0)
        return;                                               // (OBJ VRAM not 1D 64 KB: not the battle's top screen)
    for (k = 0; k < n; k++)
    {
        u32 a2 = OAM[idx[k] * 4 + 2];
        if (sPiece[k].tile != (a2 & 1023))
        {
            sPiece[k].art = NO_ART;                           // (another sprite: find its art again)
            sPiece[k].sum = 0;
        }
        sPiece[k].tile = a2 & 1023;
        if (sPiece[k].art == NO_ART)
            search = 1;
    }
    // new box sprites: their art (the boxes appear during the battle's intro)
    if (search)
        MatchArt(chr, n);
    for (k = 0; k < n; k++)
    {
        const u32 *w = (const u32 *)chr[k];
        u32 sum = 0;
        for (i = 0; i < 512; i++)
            sum = sum * 31 + w[i];
        if (sPiece[k].art != NO_ART && sPiece[k].sum != sum)
        {
            // what the game drew changed: copy its text again
            vu32 *ovl = (vu32 *)(OBJ_VRAM + (OVL_SLOT(k) << shift));
            const u8 *m = sArt[sPiece[k].art].mask;
            const u32 *src = (const u32 *)chr[k];
            u32 t, r;
            for (t = 0; t < 64; t++)
                for (r = 0; r < 8; r++)
                {
                    // the row's text pixels: 14/2 where the box art isn't (same layout as the box sprite)
                    u32 w = src[t * 8 + r];
                    ovl[t * 8 + r] = w & Nibbles(Bits14or2(w) & ~m[MASK_BYTE(t, r)]);
                }
            sPiece[k].sum = sum;
        }
    }
    // the overlays' palette row: one no shown sprite uses
    for (sRow = 15; sRow > 2 && used & 1 << sRow; sRow--)
        ;
    if (sRow <= 2)
    {
        sRow = 0;
        return;
    }
    OBJ_PAL[sRow * 16 + 14] = TEXT_COLOUR;
    OBJ_PAL[sRow * 16 + 2] = SHADOW_COLOUR;
    Insert(shift, idx, n);
}

// right after every copy of the game's sprites to the hardware (the theme's OAM loader hook): the overlays back in
void RH_HpOam(void)
{
    u32 dispcnt = *(vu32 *)0x04000000, shift = 5 + (dispcnt >> 20 & 3), n, used;
    u16 idx[MAX_PIECES];
    const u8 *chr[MAX_PIECES];
    if (!(dispcnt & 0x10) || shift < 6 || sRow == 0 || (n = Boxes(shift, idx, chr, &used)) == 0)
        return;
    Insert(shift, idx, n);
}

// after the sprite library's own OAM copies (moving sprites between the game's full copies, theme's hook): each
// overlay put in earlier takes its box sprite's new position, or goes away with it
void RH_HpSync(void)
{
    u32 s, i, shift = 5 + (*(vu32 *)0x04000000 >> 20 & 3);
    for (s = 0; s < MAX_PIECES; s++)
    {
        u32 j = sPut[s] - 1u;
        if (j >= 128 || (OAM[j * 4 + 2] & 1023) != OVL_SLOT(s))
            continue;
        for (i = 0; i < 128; i++)
            if (i != j && (OAM[i * 4 + 2] & 1023) == sPiece[s].tile && (OAM[i * 4] & 0x300) != 0x200)
                break;
        if (i == 128)
            OAM[j * 4] = 0x200;
        else
        {
            OAM[j * 4] = OAM[i * 4];
            OAM[j * 4 + 1] = OAM[i * 4 + 1];
        }
    }
}


// ---- moved from src/rh_theme.c (always-loaded space): battle only ----
u32 LONG_CALL GetBuf(u32 idx, BOOL faded);    // src/rh_theme.c (always loaded: called through a register)
void LONG_CALL SetIndex(u32 idx, u32 c);
#define GetIndex(idx) GetBuf(idx, FALSE)

// Battle move buttons. Their colours come from a table in battle overlay 6 (0x0221BA00), one 16-colour row per type
// (Normal first, then type 18 down to 1), copied into the palette by code: no palette file to theme.
// - FIGHT screen (owner's 61): inside dark, every type (entries 1, 13, 14), patched in the table once it is loaded.
// - Check Moves (owner's 72): the game draws all four buttons with sub BG row 12; the owner gave each button the
//   border of its move's type (entries 1, 2, 5, 6 = the type row's 6, 7, 8, 5). Each button's type comes from its
//   type icon (a word of its graphics, narc 8), and the button is re-pointed at a row of its own (12, 7, 8, 10:
//   unused on that screen), a copy of row 12 with those 4 colours.
#define OV6_MOVE_ROWS ((u16 *)0x0221BB4C)
static const u32 sIconWord[18] = {     // word 7 of each type's icon graphics, by type
    0xEFEFFFF8, 0xFFF33333, 0xF2EFFF22, 0xFEEFFFBB, 0xFFEFFEF4, 0xFFF55555, 0xF7777777, 0xFFEF3333, 0xEFFECCCC,
    0xFFFAAAAA, 0xFFF22222, 0x44EF4444, 0xFFEF2222, 0xF1EFFF11, 0xFEEEFFF8, 0x55555555, 0xFFEF4EF4, 0xAEFAAAAA,
};

u32 RH_BattleMoves(void)
{
    static const u8 sRow[4] = { 12, 7, 8, 10 }, sFrom[4] = { 6, 7, 8, 5 }, sTo[4] = { 1, 2, 5, 6 };
    u16 *t = OV6_MOVE_ROWS;
    vu16 *oam = (vu16 *)0x07000400, *map;
    u32 i, k, sh = 5 + (*(vu32 *)0x04001000 >> 20 & 3);
    u32 cmMap = 0;
    if (t[0] != 0x75CD || t[2] != 0x6AD7 || t[9] != 0x7319)      // (overlay 6 not loaded)
        return 0;
    if (t[1] == 0x7FFF)
        for (i = 0; i < 19; i++)
        {
            t[16 * i + 1] = 0x1084; t[16 * i + 13] = 0x0842; t[16 * i + 14] = 0x0C63;
        }
    if (*(vu16 *)0x0400100C != 0x010A)                           // (sub BG2 of the battle party screens)
        return 0;
    map = (vu16 *)0x06200800;
    for (k = 0; k < 128; k++, oam += 4)
    {
        u32 a0 = oam[0], a1 = oam[1], x = a1 & 511, y = a0 & 255, b, ty, w;
        const u16 *row;
        if ((a0 & 0x300) == 0x200 || a0 >> 14 != 1 || a1 >> 14 != 2 || y < 48 || y >= 144 || (x & 127) >= 64)
            continue;                  // (hidden, not a 32x16 icon, not on a button)
        w = *(vu32 *)(0x06600000 + ((oam[2] & 1023) << sh) + 28);
        for (ty = 0; ty < 18 && sIconWord[ty] != w; ty++)
            ;
        if (ty == 18)
            continue;
        b = (y >= 96) * 2 + (x >= 128);
        row = t + 16 * (ty ? 19 - ty : 0);
        for (i = 0; i < 16; i++)       // (only what differs: the game's fades keep working)
        {
            u32 c = GetIndex(512 + 12 * 16 + i);
            for (w = 0; w < 4; w++)
                if (sTo[w] == i)
                    c = row[sFrom[w]];
            if (GetIndex(512 + sRow[b] * 16 + i) != c)
                SetIndex(512 + sRow[b] * 16 + i, c);
        }
        for (i = 0; i < 6 * 16 && b != 0; i++)
        {
            vu16 *e = map + (6 + (b >> 1) * 6 + i / 16) * 32 + (b & 1) * 16 + i % 16;
            if (*e >> 12 == 12)
                *e = (u16)((*e & 0x0FFF) | sRow[b] << 12);
        }
        cmMap = (u32)map;
    }
    return cmMap;
}

// the entry the theme looks for at the start of this overlay (0x023C4000)
const u32 gRhHpTextEntry[6] __attribute__((section(".init"))) = { 0x54504852, 0x31545854, (u32)RH_HpText, (u32)RH_HpOam,
    (u32)RH_HpSync, (u32)RH_BattleMoves };
