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

static u32 Nib(const u8 *chr, u32 x, u32 y)
{
    u32 t = (y >> 3) * 8 + (x >> 3), p = (y & 7) * 8 + (x & 7), b = chr[t * 32 + p / 2];
    return p & 1 ? b >> 4 : b & 15;
}

static BOOL Is14or2(u32 n)
{
    return n == 14 || n == 2;
}

// how well a half's mask fits a box sprite: its art pixels shown in 14/2 (the theme's tile patches may have moved a
// few to spare entries)
static s32 Score(const u8 *mask, const u8 *chr)
{
    s32 s = 0;
    u32 i;
    for (i = 0; i < 4096; i++)
        if (mask[i >> 3] & 1 << (i & 7))
            s += Is14or2(Nib(chr, i & 63, i >> 6)) ? 1 : -1;
    return s;
}

// the art of the box sprites without one: every half's mask scored against them, the best kept
static void MatchArt(const u8 *const *chr, u32 n)
{
    static u8 sMask[512];
    u32 file[FSFILE_SIZE / 4], hdr[2], i, j, k, a, bestKey[MAX_PIECES], pass;
    s32 best[MAX_PIECES];
    for (k = 0; k < n; k++)
    {
        best[k] = 0;
        bestKey[k] = 0;
    }
    // pass 0: score every half; pass 1: keep the winners' masks
    for (pass = 0; pass < 2; pass++)
    {
        FS_InitFile(file);
        if (!FS_OpenFile(file, "data/rh_hpart.bin"))
            return;
        if (FS_ReadFile(file, hdr, 8) == 8 && hdr[0] == 0x50484852)
            for (i = 0; i < hdr[1]; i++)
            {
                u32 key;
                if (FS_ReadFile(file, &key, 4) != 4 || FS_ReadFile(file, sMask, 512) != 512)
                    break;
                for (k = 0; k < n; k++)
                {
                    if (sPiece[k].art != NO_ART)
                        continue;
                    if (pass == 0)
                    {
                        s32 s = Score(sMask, chr[k]);
                        if (s > best[k])
                        {
                            best[k] = s;
                            bestKey[k] = key;
                        }
                        continue;
                    }
                    if (bestKey[k] != key)
                        continue;
                    for (a = 0; a < sArtN && sArt[a].key != key; a++)
                        ;
                    if (a == sArtN)
                    {
                        if (sArtN == MAX_ART)
                            continue;
                        sArt[a].key = key;
                        for (j = 0; j < 512; j++)
                            sArt[a].mask[j] = sMask[j];
                        sArtN++;
                    }
                    sPiece[k].art = (u8)a;
                }
            }
        FS_CloseFile(file);
    }
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
            u8 *ovl = (u8 *)(OBJ_VRAM + (OVL_SLOT(k) << shift));
            const u8 *m = sArt[sPiece[k].art].mask;
            u32 x, y;
            for (y = 0; y < 64; y++)
                for (x = 0; x < 64; x += 4)
                {
                    u32 out = 0, q;
                    for (q = 0; q < 4; q++)
                    {
                        u32 v = Nib(chr[k], x + q, y), p = y * 64 + x + q;
                        if (Is14or2(v) && !(m[p >> 3] & 1 << (p & 7)))
                            out |= v << (q * 4);
                    }
                    // (VRAM takes 16-bit writes: 4 pixels at a time)
                    *(vu16 *)(ovl + ((y >> 3) * 8 + (x >> 3)) * 32 + (y & 7) * 4 + (x & 7) / 2) = (u16)out;
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

// the entry the theme looks for at the start of this overlay (0x023C4000)
const u32 gRhHpTextEntry[5] __attribute__((section(".init"))) = { 0x54504852, 0x31545854, (u32)RH_HpText, (u32)RH_HpOam,
    (u32)RH_HpSync };
