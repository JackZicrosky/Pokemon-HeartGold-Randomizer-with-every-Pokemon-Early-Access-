// Romhack (HeartGold Expansion Randomizer): the randomizer options screen, shown at New Game before
// Professor Oak (and, later, from the Randomizer Settings key item). Modelled on Universal Pokemon
// Randomizer FVX; same rows, wording and controls as the FireRed/Emerald versions (src/rh_randomizer_menu.c
// there), laid out for the DS's two screens:
//   top screen     header (section) + the option rows
//   bottom screen  the selected option's description, YES/NO boxes, messages, controls, text entry
// SELECT opens the section menu, L/R jump between sections, UP/DOWN pick an option, LEFT/RIGHT change it,
// A toggles / steps a choice / types text, START jumps to "Begin Run".
//
// Drawing doesn't use the game's window system: each screen is one 4bpp background whose 768 tiles form a
// 256x192 bitmap kept in RAM; text is drawn with the game's own font (src/rhmenu/rh_font.h).

#include "../../include/types.h"
#include "../../include/system.h"
#include "../../include/save.h"
#include "../../include/sound.h"
#include "../../include/constants/sndseq.h"
#include "../../include/rh/rh.h"
#include "../../include/rh/rh_test.h"

#include "rh_font.h"
#include "rh_names.h"

// ---------------------------------------------------------------------------
// Game functions
// ---------------------------------------------------------------------------
void *LONG_CALL OverlayManager_GetArgs(void *manager);
BOOL LONG_CALL Heap_Create(u32 parent, u32 child, u32 size);
void LONG_CALL Heap_Destroy(u32 heapId);
void LONG_CALL BeginNormalPaletteFade(u32 mode, u32 typeMain, u32 typeSub, u16 color, u32 steps, u32 framesPerStep, u32 heapId);
BOOL LONG_CALL IsPaletteFadeFinished(void);

#define HEAPID_RH_MENU 75     // the New Game app's heap id (that app has finished)

#define APP_OAK_SPEECH ((const void *)0x02106068)

struct OverlayManagerTemplate
{
    BOOL (*init)(void *manager, int *state);
    BOOL (*exec)(void *manager, int *state);
    BOOL (*exit)(void *manager, int *state);
    u32 ovyId;
};

static BOOL MenuInit(void *manager, int *state);
static BOOL MenuExec(void *manager, int *state);
static BOOL MenuExit(void *manager, int *state);

// First thing in overlay 153 (RH_MENU_APP_TEMPLATE).
const struct OverlayManagerTemplate __attribute__((section(".rh_header"))) gRhMenuApp = {
    .init = MenuInit,
    .exec = MenuExec,
    .exit = MenuExit,
    .ovyId = 0xFFFFFFFF,
};

// ---------------------------------------------------------------------------
// Hardware
// ---------------------------------------------------------------------------
#define PAD_KEY_ANY (PAD_KEY_UP | PAD_KEY_DOWN | PAD_KEY_LEFT | PAD_KEY_RIGHT)
#define REG32(a) (*(vu32 *)(a))
#define REG16(a) (*(vu16 *)(a))
#define DISPCNT_MAIN 0x04000000
#define DISPCNT_SUB  0x04001000
#define BG0CNT_MAIN  0x04000008
#define BG0CNT_SUB   0x04001008
#define BG0HOFS_MAIN 0x04000010
#define BG0HOFS_SUB  0x04001010
#define BLDCNT_MAIN  0x04000050
#define BLDCNT_SUB   0x04001050
#define POWCNT1      0x04000304
#define BGPAL_MAIN   ((vu16 *)0x05000000)
#define BGPAL_SUB    ((vu16 *)0x05000400)
#define VRAM_MAIN_BG 0x06000000
#define VRAM_SUB_BG  0x06200000
#define MAP_OFFSET   0xF800            // screen base block 31
#define SCREEN_BYTES (32 * 24 * 32)    // 768 tiles of 4bpp

enum { SCR_TOP, SCR_BOTTOM };

// GraphicsBanks for GfGfx_SetBanks: main BG = VRAM A, sub BG = VRAM C, nothing else
static const u32 sBanks[10] = { 0x01, 0, 0x04, 0, 0, 0, 0, 0, 0, 0 };

// Brand palette (the randomizer options screen colours).
#define RGB(r, g, b) ((r) | ((g) << 5) | ((b) << 10))
enum { C_BG, C_PANEL, C_TEXT, C_SHADOW, C_VALUE, C_VALUE_SH, C_DIM, C_HEADER, C_SEL, C_ACCENT, C_HEADBAR, C_OK, C_WARN, C_BLOOD, C_BLOOD_SEL };
static const u16 sPal[16] = {
    [C_BG]        = RGB(2, 3, 4),
    [C_PANEL]     = RGB(3, 4, 5),
    [C_TEXT]      = RGB(28, 28, 29),
    [C_SHADOW]    = RGB(7, 8, 10),
    [C_VALUE]     = RGB(12, 26, 31),
    [C_VALUE_SH]  = RGB(3, 8, 11),
    [C_DIM]       = RGB(13, 14, 16),
    [C_HEADER]    = RGB(31, 22, 10),
    [C_SEL]       = RGB(7, 9, 13),
    [C_ACCENT]    = RGB(20, 13, 31),
    [C_HEADBAR]   = RGB(5, 6, 9),
    [C_OK]        = RGB(10, 27, 12),
    [C_WARN]      = RGB(31, 11, 9),
    [C_BLOOD]     = RGB(31, 2, 3),
    [C_BLOOD_SEL] = RGB(8, 1, 2),
};

static u8 *sBuf[2];          // 256x192 4bpp bitmaps, tiled like the background
static u8 sDirty[2];

static void Fill(u32 scr, u8 c)
{
    memset(sBuf[scr], c | (c << 4), SCREEN_BYTES);
    sDirty[scr] = TRUE;
}

static void Rect(u32 scr, s32 x, s32 y, s32 w, s32 h, u8 c)
{
    s32 i, j;
    u8 *b = sBuf[scr];
    if (x < 0) w += x, x = 0;
    if (y < 0) h += y, y = 0;
    if (x + w > 256) w = 256 - x;
    if (y + h > 192) h = 192 - y;
    for (j = y; j < y + h; j++)
    {
        for (i = x; i < x + w; i++)
        {
            u8 *p = b + ((j >> 3) * 32 + (i >> 3)) * 32 + (j & 7) * 4 + ((i & 7) >> 1);
            *p = (i & 1) ? ((*p & 0x0F) | (c << 4)) : ((*p & 0xF0) | c);
        }
    }
    sDirty[scr] = TRUE;
}

static inline void Pixel(u8 *b, u32 x, u32 y, u8 c)
{
    u8 *p;
    if (x >= 256 || y >= 192)
        return;
    p = b + ((y >> 3) * 32 + (x >> 3)) * 32 + (y & 7) * 4 + ((x & 7) >> 1);
    *p = (x & 1) ? ((*p & 0x0F) | (c << 4)) : ((*p & 0xF0) | c);
}

// UTF-8 to code point
static u32 NextChar(const char **s)
{
    const u8 *p = (const u8 *)*s;
    u32 c = *p++;
    if (c >= 0xE0 && p[0] && p[1])
    {
        c = ((c & 0x0F) << 12) | ((p[0] & 0x3F) << 6) | (p[1] & 0x3F);
        p += 2;
    }
    else if (c >= 0xC0 && p[0])
    {
        c = ((c & 0x1F) << 6) | (p[0] & 0x3F);
        p++;
    }
    *s = (const char *)p;
    if (c == '\'')
        c = 0x2019;                       // the game font has no straight quotes
    else if (c == '"')
        c = 0x201D;
    return c;
}

static const struct RhGlyph *Glyph(u32 cp)
{
    s32 lo = 0, hi = RH_FONT_GLYPHS - 1;
    while (lo <= hi)
    {
        s32 mid = (lo + hi) / 2;
        if (sRhFont[mid].codepoint == cp)
            return &sRhFont[mid];
        if (sRhFont[mid].codepoint < cp)
            lo = mid + 1;
        else
            hi = mid - 1;
    }
    return Glyph('?');
}

static u32 TextWidth(const char *s)
{
    u32 w = 0, best = 0;
    while (*s)
    {
        u32 c = NextChar(&s);
        if (c == '\n')
        {
            best = w > best ? w : best;
            w = 0;
            continue;
        }
        w += Glyph(c)->width;
    }
    return w > best ? w : best;
}

// Draws text (several lines with '\n', 16 px apart); returns the x after the last character.
static s32 Text(u32 scr, s32 x, s32 y, const char *s, u8 fg, u8 sh)
{
    s32 x0 = x;
    u8 *b = sBuf[scr];
    while (*s)
    {
        u32 c = NextChar(&s), gx, gy;
        const struct RhGlyph *g;
        if (c == '\n')
        {
            x = x0;
            y += 16;
            continue;
        }
        g = Glyph(c);
        for (gy = 0; gy < 16; gy++)
        {
            u32 row = g->rows[gy];
            if (row == 0)
                continue;
            for (gx = 0; gx < 16; gx++)
            {
                u32 v = (row >> (2 * gx)) & 3;
                if (v == 1)
                    Pixel(b, x + gx, y + gy, fg);
                else if (v == 2 && sh)
                    Pixel(b, x + gx, y + gy, sh);
            }
        }
        x += g->width;
    }
    sDirty[scr] = TRUE;
    return x;
}

static void TextRight(u32 scr, s32 right, s32 y, const char *s, u8 fg, u8 sh)
{
    Text(scr, right - TextWidth(s), y, s, fg, sh);
}

static void TextCenter(u32 scr, s32 cx, s32 y, const char *s, u8 fg, u8 sh)
{
    Text(scr, cx - TextWidth(s) / 2, y, s, fg, sh);
}

static void Flush(void)
{
    u32 scr;
    for (scr = 0; scr < 2; scr++)
    {
        if (sDirty[scr])
        {
            const u32 *src = (const u32 *)sBuf[scr];
            vu32 *dst = (vu32 *)(scr == SCR_TOP ? VRAM_MAIN_BG : VRAM_SUB_BG);
            u32 i;
            for (i = 0; i < SCREEN_BYTES / 4; i++)
                dst[i] = src[i];
            sDirty[scr] = FALSE;
        }
    }
}

static void SetupScreens(void)
{
    u32 scr, i;
    GfGfx_SetBanks((void *)sBanks);
    for (scr = 0; scr < 2; scr++)
    {
        u32 vram = (scr == SCR_TOP) ? VRAM_MAIN_BG : VRAM_SUB_BG;
        vu16 *map = (vu16 *)(vram + MAP_OFFSET);
        vu16 *pal = (scr == SCR_TOP) ? BGPAL_MAIN : BGPAL_SUB;
        for (i = 0; i < 32 * 32; i++)
            map[i] = (i < 32 * 24) ? i : 0;   // tile i at cell i: the 768 tiles form the bitmap
        for (i = 0; i < 16; i++)
            pal[i] = sPal[i];
    }
    REG32(DISPCNT_MAIN) = 0x00010100;         // graphics display, BG mode 0, BG0 only
    REG32(DISPCNT_SUB) = 0x00010100;
    REG16(BG0CNT_MAIN) = 31 << 8;             // 4bpp, char base 0, screen base 31, 256x256
    REG16(BG0CNT_SUB) = 31 << 8;
    REG32(BG0HOFS_MAIN) = 0;
    REG32(BG0HOFS_SUB) = 0;
    REG16(BLDCNT_MAIN) = 0;
    REG16(BLDCNT_SUB) = 0;
    REG16(POWCNT1) |= 0x8000;                  // engine A (the list) on the top screen
}

// ---------------------------------------------------------------------------
// Rows
// ---------------------------------------------------------------------------
enum RowKind { RK_HEADER, RK_CHOICE, RK_TOGGLE, RK_SLIDER, RK_TEXT, RK_ACTION };
enum TextField { TF_SEED, TF_STARTER1, TF_STARTER2, TF_STARTER3, TF_TYPE, TF_BST_MIN, TF_BST_MAX };
enum Action { ACT_BEGIN, ACT_SHOW_CODE, ACT_ENTER_CODE, ACT_SAVE_1, ACT_SAVE_2, ACT_SAVE_3, ACT_LOAD_1, ACT_LOAD_2, ACT_LOAD_3,
              ACT_DEFAULTS, ACT_EXIT, ACT_RESET_RUN, ACT_RESET_RUN_2 };
enum SliderFmt { SF_PLAIN, SF_PERCENT, SF_SIGNED_PERCENT, SF_LEVEL };

struct RhRow
{
    u8 kind;
    u8 section;
    u8 indent;
    u8 fmt;          // slider format / text field id / action
    u16 offset;      // field offset in struct RhSettings
    u8 size;         // 1 = u8, 2 = s16/u16
    u8 choiceCount;
    s16 min, max, step;
    const char *label;
    const char *const *choices;
    const char *desc;
    BOOL (*visible)(const struct RhSettings *s);
};

enum
{
    SEC_GENERAL, SEC_TRAITS, SEC_TYPES, SEC_STARTERS, SEC_MOVES, SEC_FOES, SEC_WILD, SEC_TMS, SEC_ITEMS,
    SEC_GRAPHICS, SEC_MISC, SEC_CODES, SEC_BEGIN, SEC_COUNT
};

static const char *const sSectionNames[SEC_COUNT] =
{
    [SEC_GENERAL]  = "General",
    [SEC_TRAITS]   = "Pokémon Traits",
    [SEC_TYPES]    = "Type Effectiveness",
    [SEC_STARTERS] = "Starters, Statics & Trades",
    [SEC_MOVES]    = "Moves & Movesets",
    [SEC_FOES]     = "Foe Pokémon",
    [SEC_WILD]     = "Wild Pokémon",
    [SEC_TMS]      = "TM/HMs & Tutors",
    [SEC_ITEMS]    = "Items",
    [SEC_GRAPHICS] = "Custom Player Graphics",
    [SEC_MISC]     = "Misc. Tweaks",
    [SEC_CODES]    = "Settings Codes & Presets",
    [SEC_BEGIN]    = "Begin Run",
};

#define LIST(name, ...) static const char *const name[] = { __VA_ARGS__ }
LIST(sOffOn, "Off", "On");
LIST(sPool, "Kanto 151", "Gen 1-3 (386)", "All 1025", "All + Forms");
LIST(sMechGen, "Gen 1", "Gen 2", "Gen 3", "Gen 4", "Gen 5", "Gen 6", "Gen 7", "Gen 8", "Gen 9");
LIST(sUSR, "Unchanged", "Shuffle", "Random");
LIST(sExpCurves, "Medium Fast", "Medium Slow", "Fast", "Slow", "Erratic", "Fluctuating");
LIST(sExpWho, "Legendaries: Slow", "Strong Legends: Slow", "All Pokémon");
LIST(sTypesC, "Unchanged", "Random (follow evos)", "Random (completely)");
LIST(sUR, "Unchanged", "Random");
LIST(sTheme, "Default", "Randomizer", "AMOLED");
LIST(sEvosC, "Unchanged", "Random", "Random Every Level");
LIST(sTypeChart, "Unchanged", "Random", "Random (balanced)", "Keep Identities", "Inverse");
LIST(sStarters, "Unchanged", "Custom", "Random (completely)", "Random (2 evolutions)", "Random (basic)");
LIST(sBstModes, "Unchanged", "Random Buff/Nerf", "Shuffle", "Random");
LIST(sStarterTypes, "None", "Fire/Water/Grass", "Any Type Triangle", "Unique", "Single Type");
LIST(sStatics, "Unchanged", "Swap Legend/Standard", "Random (completely)", "Random (similar str.)");
LIST(sTrades, "Unchanged", "Given Pokémon Only", "Requested & Given");
LIST(sMovesets, "Unchanged", "Random (same type)", "Random (completely)", "Metronome Only");
LIST(sTrainers, "Unchanged", "Random", "Random (even)", "Type Themed", "Type Themed (E4/Gyms)",
     "Keep Themed Trainers", "Keep Themes Or Primary");
LIST(sBattleStyle, "Unchanged", "Random", "Single Style");
LIST(sSinglesDoubles, "Single Battles", "Double Battles");
LIST(sWildZone, "1 In Whole Game", "1 Per Location", "1 Per Encounter Set", "Maximum Possible",
     "Random Always", "1 Per Map");
LIST(sWildType, "None", "Randomize Zone Themes", "Keep Primary Type");
LIST(sWildEvo, "None", "Only Basic Pokémon", "Same Evolution Stage");
LIST(sCompat, "Unchanged", "Random (same type)", "Random (completely)", "Full Compatibility");
LIST(sFieldItems, "Unchanged", "Shuffle", "Random", "Random (even)");
LIST(sShopItems, "Unchanged", "Shuffle", "Random");
LIST(sBoyGirl, "Boy & Girl", "Boy Only", "Girl Only");
LIST(sPlayerGraphics, "Default");

// ---------------------------------------------------------------------------
// Visibility rules
// ---------------------------------------------------------------------------
static struct RhSettings sPending;   // the settings being edited
#define S (&sPending)

static u8 sInGame;
static BOOL SaveExists(void)
{
    return ((SaveData *)SaveBlock2_get())->saveFileExists;
}                  // opened from the Randomizer Settings key item during a run

#define VIS(name, expr) static BOOL name(const struct RhSettings *s) { (void)s; return (expr); }
VIS(visInGame, sInGame)
VIS(visResetRun, sInGame || SaveExists())
VIS(visPlayerGraphics, s->playerGraphics != 0)
VIS(visPalettes, s->paletteMode != 0)
VIS(visStatsRandomized, s->baseStats != 0)
VIS(visBst, s->bstMode != 0)
VIS(visBstPct, s->bstMode == 1)
VIS(visBstShuffle, s->bstMode == 2)
VIS(visExpCurve, s->expCurve != 0)
VIS(visTypesRandom, s->types != 0)
VIS(visAbilities, s->abilities != 0)
VIS(visEvos, s->evolutions != 0)
VIS(visEvosRandom, s->evolutions == 1)
VIS(visStatsAdded, s->baseStats == 2 && s->baseStatsFollowEvos)
VIS(visEstLevels, s->evolutions != 2 && (s->evoChangeImpossible || s->evoMakeEasier))
VIS(visEvoFixes, s->evolutions != 2)
VIS(visInverse, s->typeChart == 4)
VIS(visCustomStarters, s->starters == 1)
VIS(visRandomStarters, s->starters >= 2)
VIS(visStarterItems, s->starterHeldItems)
VIS(visBstMin, visRandomStarters(s) && s->starterBstMinOn)
VIS(visBstMax, visRandomStarters(s) && s->starterBstMaxOn)
VIS(visSingleType, visRandomStarters(s) && s->starterTypes == 4)
VIS(visStatics, s->statics != 0)
VIS(visStaticLevel, s->staticLevelModOn)
VIS(visStaticsSimilar, s->statics == 3)
VIS(visTrades, s->trades != 0)
VIS(visMovesets, s->movesets != 0 && s->movesets != 3)
VIS(visLevel1, s->movesets != 0 && s->movesets != 3 && s->guaranteedLevel1On)
VIS(visMovesetDamaging, s->movesets != 0 && s->movesets != 3 && s->movesetGoodDamagingOn)
VIS(visSingleStyle, s->battleStyle == 2)
VIS(visTrainerEvolve, s->trainersEvolveOn)
VIS(visTrainerLevel, s->trainerLevelModOn)
VIS(visWild, s->wild)
VIS(visWildSplit, s->wild && (s->wildZone == 0 || s->wildZone == 1 || s->wildZone == 5))
VIS(visWildKeepThemes, s->wild)
VIS(visWildKeepRelations, s->wild && s->wildEvoRestriction != 2 && s->wildZone != 3 && s->wildZone != 4)
VIS(visWildCatch, s->wildCatchRateOn)
VIS(visWildHeldBan, s->wildHeldItems)
VIS(visWildBalance, s->wild && s->wildSimilarStrength)
VIS(visWildLevel, s->wildLevelModOn)
VIS(visTmMoves, s->tmMoves != 0 && s->movesets != 3)
VIS(visTmDamaging, s->tmMoves != 0 && s->movesets != 3 && s->tmGoodDamagingOn)
VIS(visNotFullTm, s->tmCompat != 3)
VIS(visNotFullTutor, s->tutorCompat != 3)
VIS(visTmFollow, s->tmCompat == 1 || s->tmCompat == 2 || (s->tmCompat == 0 && s->tmLevelupSanity))
VIS(visTutorFollow, s->tutorCompat == 1 || s->tutorCompat == 2 || (s->tutorCompat == 0 && s->tutorLevelupSanity))
VIS(visNotMetronome, s->movesets != 3)
VIS(visTutorMoves, s->tutorMoves != 0 && s->movesets != 3)
VIS(visTutorDamaging, s->tutorMoves != 0 && s->movesets != 3 && s->tutorGoodDamagingOn)
VIS(visFieldRandom, s->fieldItems >= 2)
VIS(visShopFilters, s->shopItems != 0 || s->shopSpecial)
VIS(visTrainersRandom, s->trainers != 0)
VIS(visTrainerWeight, s->trainers == 3)
VIS(visPickup, s->pickupItems != 0)

#define OFS(f) ((u16)(u32)&((struct RhSettings *)0)->f)
#define SZ(f) sizeof(((struct RhSettings *)0)->f)
#define HDR(sec, text) { .kind = RK_HEADER, .section = sec, .label = text }
#define CHOICE(sec, ind, f, list, text, d, vis) { .kind = RK_CHOICE, .section = sec, .indent = ind, .offset = OFS(f), .size = SZ(f), \
    .choices = list, .choiceCount = NELEMS(list), .label = text, .desc = d, .visible = vis }
#define TOGGLE(sec, ind, f, text, d, vis) CHOICE(sec, ind, f, sOffOn, text, d, vis)
#define SLIDER(sec, ind, f, lo, hi, st, fm, text, d, vis) { .kind = RK_SLIDER, .section = sec, .indent = ind, .offset = OFS(f), .size = SZ(f), \
    .min = lo, .max = hi, .step = st, .fmt = fm, .label = text, .desc = d, .visible = vis }
#define ACTION(sec, act, text, d) { .kind = RK_ACTION, .section = sec, .fmt = act, .label = text, .desc = d }
#define ACTION_VIS(sec, act, text, d, vis) { .kind = RK_ACTION, .section = sec, .fmt = act, .label = text, .desc = d, .visible = vis }
#define TEXTROW(sec, ind, tf, text, d, vis) { .kind = RK_TEXT, .section = sec, .indent = ind, .fmt = tf, .label = text, .desc = d, .visible = vis }

static const struct RhRow sRows[] =
{
    // ---------------- General ----------------
    ACTION_VIS(SEC_GENERAL, ACT_RESET_RUN, "Reset The Run", "Erases your saved game and starts over\nwith these settings. Presets are kept.", visResetRun),
    CHOICE(SEC_GENERAL, 0, mechanicsGen, sMechGen, "Battle Mechanics", "Which generation's battle rules apply.\nPhys./Special split + Fairy always on.", NULL),
    TOGGLE(SEC_GENERAL, 0, enabled, "Randomizer", "Master switch. Off = nothing is\nrandomized (Misc. Tweaks still apply).", NULL),
    CHOICE(SEC_GENERAL, 0, speciesPool, sPool, "Pokémon Pool", "Which Pokémon the randomizer may use.\n+ Forms adds regional/alt. forms.", NULL),
    TEXTROW(SEC_GENERAL, 0, TF_SEED, "Seed", "Type any text. Same seed + same options\n= the exact same randomized game.", NULL),
    TOGGLE(SEC_GENERAL, 0, nuzlocke, "Nuzlocke Mode", "Adds two key items. One is an infinite\nRare Candy, one heals your whole party.", NULL),
    TOGGLE(SEC_GENERAL, 0, noPrematureEvos, "No Premature Evolutions", "Random wild/trainer Pokémon are never\nmore evolved than their level allows.", NULL),
    TOGGLE(SEC_GENERAL, 0, randomIntroMon, "Random Intro Pokémon", "Oak shows a random Pokémon in his\nintro instead of Marill.", NULL),

    // ---------------- Pokemon Traits ----------------
    HDR(SEC_TRAITS, "Pokémon Base Statistics"),
    CHOICE(SEC_TRAITS, 1, baseStats, sUSR, "Base Stats", "Shuffle: stats swap around.\nRandom: the total is redistributed.", NULL),
    TOGGLE(SEC_TRAITS, 2, baseStatsFollowEvos, "Follow Evolutions", "Evolutions keep the same shuffle / stat\nproportions (split evos roll their own).", visStatsRandomized),
    TOGGLE(SEC_TRAITS, 2, baseStatsRandomAdded, "Rand. Added Stats on Evo", "The stats a Pokémon gains when it\nevolves are distributed randomly.", visStatsAdded),
    TOGGLE(SEC_TRAITS, 2, statsFollowMegas, "Follow Mega Evolutions", "Megas keep their base form's new stat\norder / proportions.", visStatsRandomized),
    CHOICE(SEC_TRAITS, 1, bstMode, sBstModes, "Base Stat Totals", "Change each Pokémon's total. Its stats\nkeep their proportions.", NULL),
    SLIDER(SEC_TRAITS, 2, bstChangePct, 5, 100, 5, SF_PERCENT, "Maximum Change", "Buff/Nerf: each Pokémon gets 100% +-\nup to this much of its total.", visBstPct),
    TOGGLE(SEC_TRAITS, 2, bstFollowEvos, "Follow Evolutions", "Families share a buff/nerf, or are\nshuffled with same-length families.", visBst),
    TOGGLE(SEC_TRAITS, 2, bstSeparateLegends, "Separate Legendaries", "Legendaries only swap totals with\nother legendaries.", visBstShuffle),
    CHOICE(SEC_TRAITS, 1, updateBaseStatsGen, sMechGen, "Update Base Stats to Gen", "Use base stats from this generation.\nOff = same as Battle Mechanics.", NULL),
    CHOICE(SEC_TRAITS, 1, expCurve, sExpCurves, "Standardize EXP Curves", "All Pokémon get this EXP curve. Also\nwidens random evolution choices.", NULL),
    CHOICE(SEC_TRAITS, 2, expCurveWho, sExpWho, "Applies To", "Legendaries (or only >600 BST ones)\nget Slow instead; All = no exception.", visExpCurve),
    HDR(SEC_TRAITS, "Pokémon Types"),
    CHOICE(SEC_TRAITS, 1, types, sTypesC, "Types", "Follow evos: evolutions keep the base\ntypes (may add a 2nd). Completely: new.", NULL),
    TOGGLE(SEC_TRAITS, 2, forceDualTypes, "Force Dual Types", "Every Pokémon gets two types.", visTypesRandom),
    TOGGLE(SEC_TRAITS, 2, typesFollowMegas, "Follow Mega Evolutions", "Megas keep their base form's new\ntype (a changed type stays different).", visTypesRandom),
    HDR(SEC_TRAITS, "Pokémon Abilities"),
    CHOICE(SEC_TRAITS, 1, abilities, sUR, "Abilities", "New random abilities (Wonder Guard\nholders like Shedinja keep theirs).", NULL),
    TOGGLE(SEC_TRAITS, 2, allowWonderGuard, "Allow Wonder Guard", "Any Pokémon may get Wonder Guard.\nCan be very broken - use with care.", visAbilities),
    TOGGLE(SEC_TRAITS, 2, combineDuplicateAbilities, "Combine Duplicate Abil.", "Abilities with the same effect (Clear\nBody/White Smoke...) count as one.", visAbilities),
    TOGGLE(SEC_TRAITS, 2, ensureTwoAbilities, "Ensure Two Abilities", "Every Pokémon gets two abilities.", visAbilities),
    TOGGLE(SEC_TRAITS, 2, abilitiesFollowEvos, "Follow Evolutions", "Non-split evolutions keep their\npre-evolution's random abilities.", visAbilities),
    TOGGLE(SEC_TRAITS, 2, abilitiesFollowMegas, "Follow Mega Evolutions", "Megas get their base form's new\nabilities.", visAbilities),
    TOGGLE(SEC_TRAITS, 2, banTrapAbilities, "Ban Trapping Abilities", "No Arena Trap, Magnet Pull or\nShadow Tag.", visAbilities),
    TOGGLE(SEC_TRAITS, 2, banNegativeAbilities, "Ban Negative Abilities", "No Defeatist, Slow Start, Truant,\nKlutz or Stall.", visAbilities),
    TOGGLE(SEC_TRAITS, 2, banBadAbilities, "Ban Bad Abilities", "No Plus/Minus, Frisk, Forewarn... and\ndoubles-only ones unless all doubles.", visAbilities),
    HDR(SEC_TRAITS, "Pokémon Evolutions"),
    CHOICE(SEC_TRAITS, 1, evolutions, sEvosC, "Evolutions", "Random: new targets with the same EXP\ncurve. Every Level: evolve each level.", NULL),
    TOGGLE(SEC_TRAITS, 2, evoSimilarStrength, "Similar Strength", "New evolutions have a similar base\nstat total to the original.", visEvosRandom),
    TOGGLE(SEC_TRAITS, 2, evoSameTyping, "Same Typing", "New evolutions share a type with\nthe Pokémon evolving.", visEvos),
    TOGGLE(SEC_TRAITS, 2, evoLimitThreeStages, "Limit to Three Stages", "No evolution chain longer than\nthree stages.", visEvosRandom),
    TOGGLE(SEC_TRAITS, 2, evoNoConvergence, "No Convergence", "No two Pokémon evolve into the\nsame Pokémon.", visEvos),
    TOGGLE(SEC_TRAITS, 2, evoForceChange, "Force Change", "Every evolution is different from\nthe original.", visEvos),
    TOGGLE(SEC_TRAITS, 2, evoForceGrowth, "Force Growth", "Evolutions always have a higher base\nstat total than before.", visEvosRandom),
    TOGGLE(SEC_TRAITS, 1, evoChangeImpossible, "Change Impossible Evos", "Trade and other impossible evolutions\nbecome level-up or item evolutions.", visEvoFixes),
    SLIDER(SEC_TRAITS, 1, evoMakeEasier, 0, 55, 5, SF_LEVEL, "Make Evolutions Easier", "Final stage by this Lv, middle by 75%.\nMax (55): levels kept, other fixes on.", visEvoFixes),
    TOGGLE(SEC_TRAITS, 1, evoEstimatedLevels, "Use Estimated Evo Levels", "Changed evolutions use levels estimated\nfrom the game instead of fixed ones.", visEstLevels),
    TOGGLE(SEC_TRAITS, 1, evoRemoveTimeBased, "Remove Time-Based Evos", "Day/night evos work any time; day/night\npairs and dusk use Sun/Moon/Dusk Stone.", visEvoFixes),
    TOGGLE(SEC_TRAITS, 1, evoAdjustLevels, "Adjust Evolution Levels", "Level-up evolutions happen at a level\nthat fits (no Dragonite at Lv 7).", visEvoFixes),

    // ---------------- Type effectiveness ----------------
    CHOICE(SEC_TYPES, 0, typeChart, sTypeChart, "Type Effectiveness", "Balanced: no type over base-game max.\nIdentities: types keep their counts.", NULL),
    TOGGLE(SEC_TYPES, 1, inverseRandomImmunities, "Add Random Immunities", "A few weaknesses become immunities\ninstead (as many as the base game).", visInverse),
    TOGGLE(SEC_TYPES, 0, updateTypeChart, "Update Type Effectiveness", "Gen 6+ matchups before any randomizing.\nOff = chart of Battle Mechanics gen.", NULL),

    // ---------------- Starters, statics & trades ----------------
    HDR(SEC_STARTERS, "Starter Pokémon"),
    CHOICE(SEC_STARTERS, 1, starters, sStarters, "Starters", "Custom: type 3 names (blank = default).\nBasic: not evolved from anything.", NULL),
    TEXTROW(SEC_STARTERS, 2, TF_STARTER1, "Starter 1", "Replaces Chikorita. Type the name;\nblank = Chikorita.", visCustomStarters),
    TEXTROW(SEC_STARTERS, 2, TF_STARTER2, "Starter 2", "Replaces Cyndaquil. Type the name;\nblank = Cyndaquil.", visCustomStarters),
    TEXTROW(SEC_STARTERS, 2, TF_STARTER3, "Starter 3", "Replaces Totodile. Type the name;\nblank = Totodile.", visCustomStarters),
    TOGGLE(SEC_STARTERS, 2, starterAllowAltFormes, "Allow Alternate Formes", "Regional and other alternate forms can\nbe starters.", visRandomStarters),
    TOGGLE(SEC_STARTERS, 2, starterNoLegends, "Don't Use Legendaries", "Random starters are never legendary.", visRandomStarters),
    TOGGLE(SEC_STARTERS, 1, starterHeldItems, "Random Starter Held Items", "All three starters hold the same\nrandom item (Gen 3 rule).", NULL),
    TOGGLE(SEC_STARTERS, 2, starterBanBadItems, "Ban Bad Items", "No berries, mail or other weak items.", visStarterItems),
    TOGGLE(SEC_STARTERS, 2, starterBstMinOn, "Limit BST: Minimum", "Random starters have at least this\nbase stat total.", visRandomStarters),
    TEXTROW(SEC_STARTERS, 3, TF_BST_MIN, "Minimum BST", "Type a number (e.g. 300).", visBstMin),
    TOGGLE(SEC_STARTERS, 2, starterBstMaxOn, "Limit BST: Maximum", "Random starters have at most this\nbase stat total.", visRandomStarters),
    TEXTROW(SEC_STARTERS, 3, TF_BST_MAX, "Maximum BST", "Type a number (e.g. 350).", visBstMax),
    CHOICE(SEC_STARTERS, 2, starterTypes, sStarterTypes, "Type Restrictions", "Triangle: each beats the next. Unique:\nno shared types. Single: all 1 type.", visRandomStarters),
    TEXTROW(SEC_STARTERS, 3, TF_TYPE, "Single Type", "Type name (e.g. DRAGON).\nBlank = a random type.", visSingleType),
    TOGGLE(SEC_STARTERS, 2, starterNoDualTypes, "No Dual Types", "Random starters have only one type\n(they may still evolve into two).", visRandomStarters),
    HDR(SEC_STARTERS, "Static Pokémon"),
    CHOICE(SEC_STARTERS, 1, statics, sStatics, "Static Pokémon", "Swap: legend<->legend, other<->other.\nSimilar: close BST. No repeats.", NULL),
    TOGGLE(SEC_STARTERS, 2, staticRandomize600, "Randomize 600+ BST", "Statics with 600+ BST (Ho-Oh...) get\na purely random replacement.", visStaticsSimilar),
    TOGGLE(SEC_STARTERS, 2, staticAllowAltFormes, "Allow Alternate Formes", "Regional and other alternate forms can\nreplace static Pokémon.", visStatics),
    TOGGLE(SEC_STARTERS, 2, staticFixMusic, "Fix Music", "Legendary encounters keep their special\nbattle music after randomizing.", visStatics),
    TOGGLE(SEC_STARTERS, 1, staticLevelModOn, "Static Level Modifier", "Change the level of static Pokémon\n(works with statics unchanged too).", NULL),
    SLIDER(SEC_STARTERS, 3, staticLevelMod, -100, 150, 5, SF_SIGNED_PERCENT, "Level Change", "Percentage added to static levels.", visStaticLevel),
    HDR(SEC_STARTERS, "In-Game Trades"),
    CHOICE(SEC_STARTERS, 1, trades, sTrades, "In-Game Trades", "Given only, or both the Pokémon you\nget and the one asked for (all unique).", NULL),
    TOGGLE(SEC_STARTERS, 2, tradeNicknames, "Randomize Nicknames", "Traded Pokémon get a random nickname.", visTrades),
    TOGGLE(SEC_STARTERS, 2, tradeOTs, "Randomize OTs", "Traded Pokémon get a random Original\nTrainer name and ID.", visTrades),
    TOGGLE(SEC_STARTERS, 2, tradeIVs, "Randomize IVs", "Traded Pokémon get random IVs.", visTrades),
    TOGGLE(SEC_STARTERS, 2, tradeItems, "Randomize Items", "Traded Pokémon hold a random item.", visTrades),

    // ---------------- Moves ----------------
    HDR(SEC_MOVES, "Move Data"),
    TOGGLE(SEC_MOVES, 1, movePower, "Randomize Move Power", "Random power, mostly 50-100, sometimes\n20-150. Multi-hit: divided per hit.", NULL),
    TOGGLE(SEC_MOVES, 1, moveAccuracy, "Randomize Move Accuracy", "Most moves get random accuracy. OHKO\nstays low; sure-hit moves never change.", NULL),
    TOGGLE(SEC_MOVES, 1, movePP, "Randomize Move PP", "Every move gets random PP (5-40,\nmostly 15-25).", NULL),
    TOGGLE(SEC_MOVES, 1, moveType, "Randomize Move Types", "Every move except Struggle and ???\nmoves gets a random type.", NULL),
    TOGGLE(SEC_MOVES, 1, moveCategory, "Randomize Move Category", "Damaging moves randomly become\nPhysical or Special.", NULL),
    TOGGLE(SEC_MOVES, 1, moveNames, "Randomize Move Names", "Moves get random names based on\ntheir (randomized) type.", NULL),
    CHOICE(SEC_MOVES, 1, updateMovesGen, sMechGen, "Update Moves to Gen", "Move power/accuracy/PP/type from this\ngen. Off = same as Battle Mechanics.", NULL),
    HDR(SEC_MOVES, "Pokémon Movesets"),
    CHOICE(SEC_MOVES, 1, movesets, sMovesets, "Movesets", "Random movesets; each Pokémon starts\nwith a good attack. Metronome: only it.", NULL),
    TOGGLE(SEC_MOVES, 2, guaranteedLevel1On, "Guaranteed Level 1 Moves", "Every Pokémon knows a set number of\nmoves at level 1.", visMovesets),
    SLIDER(SEC_MOVES, 3, guaranteedLevel1Moves, 2, 4, 1, SF_PLAIN, "Moves at Level 1", "How many moves at level 1 (2-4).", visLevel1),
    TOGGLE(SEC_MOVES, 2, reorderDamagingMoves, "Reorder Damaging Moves", "Weaker attacks are learned first;\nother moves keep their places.", visMovesets),
    TOGGLE(SEC_MOVES, 2, evolutionMovesForAll, "Evolution Moves for All", "Every evolved Pokémon learns a\nmove when it evolves.", visMovesets),
    TOGGLE(SEC_MOVES, 2, movesetNoGameBreaking, "No Game-Breaking Moves", "No Sonic Boom or Dragon Rage. Gen 1\nrules: also OHKO moves and Spore.", visMovesets),
    TOGGLE(SEC_MOVES, 2, movesetGoodDamagingOn, "Force % Good Damaging", "A set share of each Pokémon's moves\nwill be good attacks.", visMovesets),
    SLIDER(SEC_MOVES, 3, movesetGoodDamaging, 0, 100, 5, SF_PERCENT, "Good Damaging Moves", "This share of random moves is forced\nto be a good attack.", visMovesetDamaging),

    // ---------------- Foes ----------------
    CHOICE(SEC_FOES, 0, trainers, sTrainers, "Trainer Pokémon", "Random, even spread, or type themes\n(per gym/E4, or keep original themes).", NULL),
    HDR(SEC_FOES, "Better Movesets for..."),
    TOGGLE(SEC_FOES, 1, betterMovesets[0], "Boss Trainers", "Leaders, Rocket bosses, Elite Four,\nLance, Red: TM/tutor moves that fit.", NULL),
    TOGGLE(SEC_FOES, 1, betterMovesets[1], "Important Trainers", "Your rival Silver's battles.", NULL),
    TOGGLE(SEC_FOES, 1, betterMovesets[2], "Regular Trainers", "Every other trainer.", NULL),
    HDR(SEC_FOES, "Additional Pokémon for..."),
    SLIDER(SEC_FOES, 1, additionalMons[0], 0, 5, 1, SF_PLAIN, "Boss Trainers", "Extra Pokémon (up to 6 total).\n0 = off.", NULL),
    SLIDER(SEC_FOES, 1, additionalMons[1], 0, 5, 1, SF_PLAIN, "Important Trainers", "Extra Pokémon (up to 6 total).\n0 = off.", NULL),
    SLIDER(SEC_FOES, 1, additionalMons[2], 0, 5, 1, SF_PLAIN, "Regular Trainers", "Extra Pokémon (up to 6 total).\n0 = off.", NULL),
    HDR(SEC_FOES, "Add Held Items to..."),
    TOGGLE(SEC_FOES, 1, heldItemsFor[0], "Boss Trainers", "Leaders, Rocket bosses, Elite Four,\nLance and Red's Pokémon hold items.", NULL),
    TOGGLE(SEC_FOES, 1, heldItemsFor[1], "Important Trainers", "Silver's Pokémon hold items.", NULL),
    TOGGLE(SEC_FOES, 1, heldItemsFor[2], "Regular Trainers", "Give regular trainers' Pokémon items.", NULL),
    TOGGLE(SEC_FOES, 1, heldConsumableOnly, "Consumable Only", "Only single-use items: mostly berries,\nplus Focus Sash, White Herb...", NULL),
    TOGGLE(SEC_FOES, 1, heldSensible, "Sensible Items", "Items that suit the Pokémon and its\nmoves (resist berries, boosters...).", NULL),
    TOGGLE(SEC_FOES, 1, heldHighestOnly, "Highest Level Only", "Only one of the trainer's highest-\nlevel Pokémon gets an item.", NULL),
    HDR(SEC_FOES, "Force Diverse Types for..."),
    TOGGLE(SEC_FOES, 1, diverseTypes[0], "Boss Trainers", "No two Pokémon share a type.\nIgnored for type-themed trainers.", NULL),
    TOGGLE(SEC_FOES, 1, diverseTypes[1], "Important Trainers", "No two Pokémon share a type.\nIgnored for type-themed trainers.", NULL),
    TOGGLE(SEC_FOES, 1, diverseTypes[2], "Regular Trainers", "No two Pokémon share a type.\nIgnored for type-themed trainers.", NULL),
    HDR(SEC_FOES, "Battle Style"),
    CHOICE(SEC_FOES, 1, battleStyle, sBattleStyle, "Battle Style", "Random singles/doubles. The first rival\nbattle never changes.", NULL),
    CHOICE(SEC_FOES, 2, battleStyleDoubles, sSinglesDoubles, "Style", "Every trainer battle uses this style.", visSingleStyle),
    HDR(SEC_FOES, "Other"),
    TOGGLE(SEC_FOES, 1, rivalCarriesTeam, "Rival Carries Starter", "Silver always uses the starter he\nstole (evolved); the rest is random.", visTrainersRandom),
    TOGGLE(SEC_FOES, 1, rivalSameTeam, "Rival Keeps Same Team", "Silver uses one team all game; his\nPokémon evolve as his levels rise.", visTrainersRandom),
    TOGGLE(SEC_FOES, 1, trainerSimilarStrength, "Similar Strength", "Replacements have a similar base stat\ntotal (other rules come first).", NULL),
    TOGGLE(SEC_FOES, 1, trainerAvoidDuplicates, "Try to Avoid Duplicates", "A trainer's Pokémon are all different\n(no two from one family).", NULL),
    TOGGLE(SEC_FOES, 1, trainerWeightTypes, "Weight Types by Count", "Types with more Pokémon are picked\nmore often for themes.", visTrainerWeight),
    TOGGLE(SEC_FOES, 1, trainerLocalPokemon, "Use Local Pokémon", "Trainers use the wild Pokémon of the\narea they are in (evolved).", visTrainersRandom),
    TOGGLE(SEC_FOES, 1, trainerAllowAltFormes, "Allow Alternate Formes", "Regional and other alternate forms\ncan be used by trainers.", NULL),
    TOGGLE(SEC_FOES, 1, trainerRandomShiny, "Random Shiny Trainer Mons", "Trainer Pokémon have a 1/256 chance\nto be shiny.", NULL),
    TOGGLE(SEC_FOES, 1, trainerNoLegends, "Don't Use Legendaries", "Trainers never use legendaries.", NULL),
    TOGGLE(SEC_FOES, 1, noEarlyWonderGuard, "No Early Wonder Guard", "No Wonder Guard on trainer Pokémon\nbefore level 20.", NULL),
    SLIDER(SEC_FOES, 1, leagueUnique, 0, 2, 1, SF_PLAIN, "League Unique Pokémon", "E4/Champion's top-level Pokémon are\nused by no other trainer. 0 = off.", visTrainersRandom),
    TOGGLE(SEC_FOES, 1, randomTrainerNames, "Randomize Trainer Names", "Trainers get random names (the same\nname always becomes the same one).", NULL),
    TOGGLE(SEC_FOES, 1, randomTrainerClassNames, "Random Trainer Classes", "Trainer classes get random names.", NULL),
    TOGGLE(SEC_FOES, 1, trainersEvolveOn, "Trainers Evolve Pokémon", "Trainer Pokémon evolve as their level\nallows; fully evolved by the level below.", NULL),
    SLIDER(SEC_FOES, 2, trainersEvolveLevel, 30, 65, 1, SF_LEVEL, "Fully Evolved By", "Latest level for fully evolved\ntrainer Pokémon.", visTrainerEvolve),
    TOGGLE(SEC_FOES, 1, trainerLevelModOn, "Percentage Level Modifier", "Change every trainer Pokémon's level\n(1-100).", NULL),
    SLIDER(SEC_FOES, 2, trainerLevelMod, -100, 150, 5, SF_SIGNED_PERCENT, "Level Change", "Percentage added to trainer levels.", visTrainerLevel),

    // ---------------- Wild ----------------
    TOGGLE(SEC_WILD, 0, wild, "Randomize Wild Pokémon", "Turn on to randomize wild encounters.", NULL),
    HDR(SEC_WILD, "Replacements Per Species"),
    CHOICE(SEC_WILD, 1, wildZone, sWildZone, "Replacements", "Where a Pokémon always gets the same\nreplacement. Max/Always: none.", visWild),
    TOGGLE(SEC_WILD, 2, wildSplitEncounterTypes, "Split by Encounter Types", "Grass, surfing, fishing and Rock Smash\nget separate replacements.", visWildSplit),
    HDR(SEC_WILD, "Type Restrictions"),
    CHOICE(SEC_WILD, 1, wildTypeRestriction, sWildType, "Types", "Zone Themes: each zone gets one random\ntype. Primary: keep the first type.", visWild),
    TOGGLE(SEC_WILD, 2, wildKeepThemes, "Keep Set/Zone Themes", "Encounter sets (or zones) that share a\ntype keep it as their theme.", visWildKeepThemes),
    HDR(SEC_WILD, "Evolution Restrictions"),
    CHOICE(SEC_WILD, 1, wildEvoRestriction, sWildEvo, "Evolutions", "Only Basic: nothing evolves into them.\nSame Stage: keep the evolution stage.", visWild),
    TOGGLE(SEC_WILD, 2, wildKeepRelations, "Keep Relations", "Related Pokémon in a zone are replaced\nby Pokémon related the same way.", visWildKeepRelations),
    HDR(SEC_WILD, "Other"),
    TOGGLE(SEC_WILD, 1, wildAllowAltFormes, "Allow Alternate Formes", "Regional and other alternate forms can\nappear in the wild.", visWild),
    TOGGLE(SEC_WILD, 1, wildNoLegends, "Don't Use Legendaries", "Legendaries never appear in the wild.", visWild),
    TOGGLE(SEC_WILD, 1, wildCatchRateOn, "Set Minimum Catch Rate", "Raise catch rates below the chosen\nlevel (works without wild rando).", NULL),
    SLIDER(SEC_WILD, 2, wildCatchRate, 1, 5, 1, SF_PLAIN, "Catch Rate Level", "1-3: higher minimums, 4: max catch rate\nfor all, 5: every ball always catches.", visWildCatch),
    TOGGLE(SEC_WILD, 1, wildHeldItems, "Randomize Held Items", "Random held items; Pokémon that held\nnothing still hold nothing.", NULL),
    TOGGLE(SEC_WILD, 2, wildBanBadItems, "Ban Bad Items", "No berries, mail or other weak items.", visWildHeldBan),
    TOGGLE(SEC_WILD, 1, wildCatchEmAll, "Catch Em' All Mode", "Every Pokémon appears in the wild\nbefore any repeats (beats Similar Str.).", visWild),
    TOGGLE(SEC_WILD, 1, wildSimilarStrength, "Similar Strength", "Replacements have a similar base\nstat total.", visWild),
    TOGGLE(SEC_WILD, 2, wildBalanceLowLevel, "Balance Low Level", "Caps base stat totals by level: no\nabnormally strong low-level Pokémon.", visWildBalance),
    TOGGLE(SEC_WILD, 1, wildMegas, "Permanent Mega Pokémon", "Mega Evolved Pokémon can appear and\nstay Mega after you catch them.", visWild),
    TOGGLE(SEC_WILD, 1, wildLevelModOn, "Percentage Level Modifier", "Change every wild Pokémon's level.", NULL),
    SLIDER(SEC_WILD, 2, wildLevelMod, -100, 150, 5, SF_SIGNED_PERCENT, "Level Change", "Percentage added to wild levels.", visWildLevel),

    // ---------------- TMs & tutors ----------------
    HDR(SEC_TMS, "TMs & HMs"),
    CHOICE(SEC_TMS, 1, tmMoves, sUR, "TM Moves", "Each TM gets a new unique move.\nHMs never change.", visNotMetronome),
    TOGGLE(SEC_TMS, 2, tmNoGameBreaking, "No Game-Breaking Moves", "No Sonic Boom or Dragon Rage. Gen 1\nrules: also OHKO moves and Spore.", visTmMoves),
    TOGGLE(SEC_TMS, 2, tmKeepFieldMoves, "Keep Field Move TMs", "TMs holding field moves (Dig, Flash...)\nkeep them; not healing moves.", visTmMoves),
    TOGGLE(SEC_TMS, 2, tmGoodDamagingOn, "Force % Good Damaging", "A set share of the TMs will be good\nattacks.", visTmMoves),
    SLIDER(SEC_TMS, 3, tmGoodDamaging, 0, 100, 5, SF_PERCENT, "Good Damaging Moves", "This share of the TMs is forced to be\na good attack.", visTmDamaging),
    CHOICE(SEC_TMS, 1, tmCompat, sCompat, "TM/HM Compat.", "Same type: 90% own type, 50% Normal,\n25% others. Completely: 50% each.", NULL),
    TOGGLE(SEC_TMS, 2, tmLevelupSanity, "TM/Levelup Move Sanity", "A Pokémon can always learn TMs of\nmoves it learns by level up.", visNotFullTm),
    TOGGLE(SEC_TMS, 2, tmCompatFollowEvos, "Follow Evolutions", "Evolutions learn every TM their\npre-evolution learns (+ a few more).", visTmFollow),
    TOGGLE(SEC_TMS, 2, fullHMCompat, "Full HM Compatibility", "Every Pokémon can learn every HM.", visNotFullTm),
    HDR(SEC_TMS, "Move Tutors"),
    CHOICE(SEC_TMS, 1, tutorMoves, sUR, "Move Tutor Moves", "Each tutor gets a new unique move\n(no TM/HM moves).", visNotMetronome),
    TOGGLE(SEC_TMS, 2, tutorNoGameBreaking, "No Game-Breaking Moves", "No Sonic Boom or Dragon Rage. Gen 1\nrules: also OHKO moves and Spore.", visTutorMoves),
    TOGGLE(SEC_TMS, 2, tutorGoodDamagingOn, "Force % Good Damaging", "A set share of the tutor moves will be\ngood attacks.", visTutorMoves),
    SLIDER(SEC_TMS, 3, tutorGoodDamaging, 0, 100, 5, SF_PERCENT, "Good Damaging Moves", "This share of the tutor moves is\nforced to be a good attack.", visTutorDamaging),
    CHOICE(SEC_TMS, 1, tutorCompat, sCompat, "Tutor Compat.", "Same type: 90% own type, 50% Normal,\n25% others. Completely: 50% each.", NULL),
    TOGGLE(SEC_TMS, 2, tutorLevelupSanity, "Tutor/Levelup Move Sanity", "A Pokémon can always learn tutor\nmoves it learns by level up.", visNotFullTutor),
    TOGGLE(SEC_TMS, 2, tutorCompatFollowEvos, "Follow Evolutions", "Evolutions learn all tutor moves their\npre-evolution learns (+ a few more).", visTutorFollow),

    // ---------------- Items ----------------
    CHOICE(SEC_ITEMS, 0, fieldItems, sFieldItems, "Field Items", "Item balls & hidden items. TM spots\nget other TMs; key items stay.", NULL),
    TOGGLE(SEC_ITEMS, 1, fieldBanBad, "Ban Bad Items", "No berries, mail or other weak items.", visFieldRandom),
    CHOICE(SEC_ITEMS, 0, shopItems, sShopItems, "Shop Items", "Shop stock except balls, medicine and\nrepels (incl. Dept. Store counters).", NULL),
    TOGGLE(SEC_ITEMS, 1, shopBanBad, "Ban Bad Items", "No berries, mail or other weak items.", visShopFilters),
    TOGGLE(SEC_ITEMS, 1, shopBanRegular, "Ban Regular Shop Items", "Random stock never includes normal\nmart items (Potions, Repels...).", visShopFilters),
    TOGGLE(SEC_ITEMS, 1, shopBanOverpowered, "Ban Overpowered Items", "No Lucky Egg, Rare Candy, Master Ball\nor items that sell for a fortune.", visShopFilters),
    TOGGLE(SEC_ITEMS, 1, shopGuaranteeEvo, "Guarantee Evolution Items", "Evolution items stay in regular shops,\nand each is sold in some special shop.", visShopFilters),
    TOGGLE(SEC_ITEMS, 1, shopGuaranteeX, "Guarantee X Items", "X items, Guard Spec. and Dire Hit keep\nbeing sold where they were.", visShopFilters),
    TOGGLE(SEC_ITEMS, 0, shopBalancePrices, "Balance Shop Prices", "Use FVX's balanced prices for items\nthat are far too cheap or expensive.", NULL),
    TOGGLE(SEC_ITEMS, 0, shopAddCheapRareCandy, "Add Cheap Rare Candies", "Poké Marts and the Dept. Store\ncounters sell Rare Candies for ¥10.", NULL),
    TOGGLE(SEC_ITEMS, 0, shopSpecial, "Randomize Special Shops", "Every special seller (evolution items,\nherbs...): random stock, no duplicates.", NULL),
    CHOICE(SEC_ITEMS, 0, pickupItems, sUR, "Pickup Items", "Items found by the Pickup ability are\nrandomized.", NULL),
    TOGGLE(SEC_ITEMS, 1, pickupBanBad, "Ban Bad Items", "No berries, mail or other weak items.", visPickup),

    // ---------------- Graphics ----------------
    CHOICE(SEC_GRAPHICS, 0, playerGraphics, sPlayerGraphics, "Player Character", "Play as a different character (UPR\nFVX packs): field, battle, trainer card.", NULL),
    CHOICE(SEC_GRAPHICS, 1, playerGraphicsReplace, sBoyGirl, "Character to Replace", "Which choice in Oak's intro uses them.\nBoy & Girl: you get them either way.", visPlayerGraphics),
    HDR(SEC_GRAPHICS, "Pokémon Palettes"),
    CHOICE(SEC_GRAPHICS, 0, paletteMode, sUR, "Pokémon Palettes", "Random: each Pokémon gets new colors\n(battle, summary and Pokédex sprites).", NULL),
    TOGGLE(SEC_GRAPHICS, 1, paletteFollowTypes, "Follow Types", "Colors match the Pokémon's (new) types:\na Fire Chikorita turns red.", visPalettes),
    TOGGLE(SEC_GRAPHICS, 1, paletteFollowEvos, "Follow Evolutions", "A family keeps similar colors.", visPalettes),
    TOGGLE(SEC_GRAPHICS, 1, paletteShinyFromNormal, "Shiny From Normal", "Shiny Pokémon show their original\ncolors.", visPalettes),

    // ---------------- Misc ----------------
    CHOICE(SEC_MISC, 0, uiTheme, sTheme, "UI Theme", "Dark menus and text boxes. Randomizer:\nthese colors. AMOLED: pitch black.", NULL),
    TOGGLE(SEC_MISC, 0, instantText, "Instantaneous Text", "All text appears instantly, whatever\nthe text speed option says.", NULL),
    TOGGLE(SEC_MISC, 0, randomPcPotion, "Randomize PC Potion", "The Potion in your bedroom PC becomes\na random useful item.", NULL),
    TOGGLE(SEC_MISC, 0, nationalDexAtStart, "National Dex at Start", "The National Dex from the start.\nPokédex ratings count National entries.", NULL),
    TOGGLE(SEC_MISC, 0, fastEggs, "Fast Egg Hatching", "Every Egg hatches in as few steps as\npossible (under 256).", NULL),
    TOGGLE(SEC_MISC, 0, lowerCaseNames, "Lower Case Pokémon Names", "“Pikachu” instead of “PIKACHU”.", NULL),
    TOGGLE(SEC_MISC, 0, randomCatchTutorial, "Random Catching Tutorial", "Your friend catches a random Pokémon\nin the catching demonstration.", NULL),
    TOGGLE(SEC_MISC, 0, banLuckyEgg, "Ban Lucky Egg", "The Lucky Egg is never picked as a\nrandomized item.", NULL),
    TOGGLE(SEC_MISC, 0, balanceStaticLevels, "Balance Static Levels", "Revived fossils come back at level 30\ninstead of 20 (as in UPR FVX).", NULL),
    TOGGLE(SEC_MISC, 0, noRandomCalls, "No Random Phone Calls", "Only story calls ring your Pokégear.\nTrainers and friends never call.", NULL),
    TOGGLE(SEC_MISC, 0, reusableTMs, "Infinitely Reusable TMs", "TMs are never used up (they can still\nbe tossed, held and sold).", NULL),
    TOGGLE(SEC_MISC, 0, forgettableTMs, "Forgettable HMs", "HM moves can be forgotten like any\nother move. Careful not to softlock!", NULL),
    TOGGLE(SEC_MISC, 0, noEVs, "No EVs From Pokémon", "Defeated Pokémon give 0 EVs (vitamins\nand Power items still work).", NULL),

    // ---------------- Codes & presets ----------------
    HDR(SEC_CODES, "Settings Code"),
    ACTION(SEC_CODES, ACT_SHOW_CODE, "Show Settings Code", "Shows these settings as a code you can\nwrite down or share with friends."),
    ACTION(SEC_CODES, ACT_ENTER_CODE, "Enter Settings Code", "Type in a code to load someone's\nsettings (seed included)."),
    HDR(SEC_CODES, "Presets (kept on this cartridge)"),
    ACTION(SEC_CODES, ACT_SAVE_1, "Save as Preset 1", "Saves these settings. Presets stay\nwhen you start a new game."),
    ACTION(SEC_CODES, ACT_SAVE_2, "Save as Preset 2", "Saves these settings. Presets stay\nwhen you start a new game."),
    ACTION(SEC_CODES, ACT_SAVE_3, "Save as Preset 3", "Saves these settings. Presets stay\nwhen you start a new game."),
    ACTION(SEC_CODES, ACT_LOAD_1, "Load Preset 1", "Replaces the current settings with\nthe saved ones."),
    ACTION(SEC_CODES, ACT_LOAD_2, "Load Preset 2", "Replaces the current settings with\nthe saved ones."),
    ACTION(SEC_CODES, ACT_LOAD_3, "Load Preset 3", "Replaces the current settings with\nthe saved ones."),
    HDR(SEC_CODES, "Other"),
    ACTION(SEC_CODES, ACT_DEFAULTS, "Reset to Defaults", "Every option goes back to its default\n(the seed is kept)."),

    // ---------------- Begin ----------------
    ACTION(SEC_BEGIN, ACT_BEGIN, "Begin Run", "Start the adventure with these\nsettings. Press A."),
    ACTION_VIS(SEC_BEGIN, ACT_EXIT, "Exit Without Changes", "Back to your game. Nothing you changed\nhere is kept. (B does this too.)", visInGame),
};

#define ROW_COUNT NELEMS(sRows)

// ---------------------------------------------------------------------------
// State
// ---------------------------------------------------------------------------
#define LIST_ROWS 11
#define ROW_H     16
#define MENU_ROW_H 13

static u8 sSection, sRow, sScroll, sMenuOpen, sMenuCursor, sConfirm, sPendingAction;
static u8 sCodeMode, sCodeLen, sCodeCursor, sPresetUsed;
static u8 sExitTo;                     // 0 = still open, 1 = begin (Oak's speech) / apply (in-game), 2 = leave unchanged
static const char *sMessage;

// text entry (seed / starters / type / BST)
#define TEXT_MAX 12
static u8 sEditing, sEditField, sEditLen, sKeyCursor;
static char sEditBuf[TEXT_MAX + 1];

static BOOL RowVisible(const struct RhRow *r)
{
    return r->visible == NULL || r->visible(S);
}

static u32 VisibleRows(u32 section, const struct RhRow **out)
{
    u32 i, n = 0;
    for (i = 0; i < ROW_COUNT; i++)
    {
        if (sRows[i].section != section || !RowVisible(&sRows[i]))
            continue;
        if (sRows[i].kind == RK_HEADER)
        {
            // a header with nothing visible under it is hidden too
            u32 j;
            BOOL any = FALSE;
            for (j = i + 1; j < ROW_COUNT && sRows[j].section == section && sRows[j].kind != RK_HEADER && !any; j++)
                any = RowVisible(&sRows[j]);
            if (!any)
                continue;
        }
        out[n++] = &sRows[i];
    }
    return n;
}

static s32 RowGet(const struct RhRow *r, const struct RhSettings *st)
{
    const u8 *p = ((const u8 *)st) + r->offset;
    return r->size == 2 ? *(const s16 *)p : *p;
}

static void RowSet(const struct RhRow *r, struct RhSettings *st, s32 v)
{
    u8 *p = ((u8 *)st) + r->offset;
    if (r->size == 2)
        *(s16 *)p = v;
    else
        *p = v;
}

static void SetValue(const struct RhRow *r, s32 v)
{
    RowSet(r, S, v);
    if (r->section != SEC_GENERAL && r->section != SEC_MISC && r->section != SEC_GRAPHICS && r->section != SEC_BEGIN)
        S->enabled = TRUE;             // touching a randomizer option switches the randomizer on
}

static BOOL IsGenRow(const struct RhRow *r)
{
    return r->choices == sMechGen;
}

static BOOL GenRowHasOff(const struct RhRow *r)
{
    return r->offset != OFS(mechanicsGen);
}

static BOOL ChoiceHasOff(const struct RhRow *r)
{
    return (IsGenRow(r) && GenRowHasOff(r)) || r->offset == OFS(expCurve);
}

static u32 ChoiceCount(const struct RhRow *r)
{
    return r->choiceCount + (ChoiceHasOff(r) ? 1 : 0);
}

static const char *ChoiceText(const struct RhRow *r, s32 v)
{
    if (IsGenRow(r))
    {
        if (GenRowHasOff(r))
            return v == 0 ? sOffOn[0] : sMechGen[(v > 9 ? 9 : v) - 1];
        return sMechGen[(v < 1 ? 1 : v > 9 ? 9 : v) - 1];
    }
    if (ChoiceHasOff(r))
        return v == 0 ? sOffOn[0] : r->choices[(v > r->choiceCount ? r->choiceCount : v) - 1];
    return r->choices[v < r->choiceCount ? v : r->choiceCount - 1];
}

static void ChangeChoice(const struct RhRow *r, s32 dir)
{
    s32 v = RowGet(r, S);
    if (IsGenRow(r) && !GenRowHasOff(r))
    {
        v += dir;
        if (v < 1) v = 9;
        if (v > 9) v = 1;
    }
    else
    {
        s32 n = ChoiceCount(r);
        v = (v + dir + n) % n;
        // FVX: "Randomize Zone Themes" needs zones ("1 In Whole Game" would make the whole game one type)
        if (r->offset == OFS(wildTypeRestriction) && v == 1 && S->wildZone == 0)
            v = (v + dir + n) % n;
    }
    SetValue(r, v);
    if (r->offset == OFS(wildZone) && S->wildZone == 0 && S->wildTypeRestriction == 1)
        S->wildTypeRestriction = 0;
}

static void ChangeSlider(const struct RhRow *r, s32 dir)
{
    s32 v = RowGet(r, S);
    if (r->fmt == SF_LEVEL && r->min == 0)
    {
        // "Make evolutions easier": 0 = off, otherwise 30..55
        if (v == 0)
            v = (dir > 0) ? 30 : 55;
        else
        {
            v += dir * r->step;
            if (v < 30 || v > 55)
                v = 0;
        }
    }
    else
    {
        v += dir * r->step;
        if (v < r->min) v = r->max;
        if (v > r->max) v = r->min;
    }
    SetValue(r, v);
}

// ---------------------------------------------------------------------------
// Small string helpers (UTF-8 text in plain char buffers)
// ---------------------------------------------------------------------------
static char *StrCopy(char *d, const char *s)
{
    while ((*d = *s++) != 0)
        d++;
    return d;
}

static char *IntToStr(char *d, s32 v)
{
    char tmp[12];
    u32 n = 0, u = v < 0 ? -v : v;
    if (v < 0)
        *d++ = '-';
    do
    {
        tmp[n++] = '0' + u % 10;
        u /= 10;
    } while (u);
    while (n)
        *d++ = tmp[--n];
    *d = 0;
    return d;
}

static const char *SpeciesName(u32 species)
{
    if (species == 0 || species >= RH_NAME_COUNT)
        return "";
    return sRhNames + sRhNameOffsets[species];
}

static const char *const sTypeNames[18] = {
    "Normal", "Fighting", "Flying", "Poison", "Ground", "Rock", "Bug", "Ghost", "Steel",
    "Fairy", "Fire", "Water", "Grass", "Electric", "Psychic", "Ice", "Dragon", "Dark",
};
#define TYPE_NONE_SETTING 0xFF

static const u16 sVanillaStarters[3] = { 152, 155, 158 };   // Chikorita, Cyndaquil, Totodile

static void FormatValue(const struct RhRow *r, char *dst)
{
    s32 v = RowGet(r, S);
    dst[0] = 0;
    switch (r->kind)
    {
    case RK_CHOICE:
    case RK_TOGGLE:
        StrCopy(dst, ChoiceText(r, v));
        break;
    case RK_SLIDER:
        if (r->fmt == SF_LEVEL && r->min == 0 && v == 0)
        {
            StrCopy(dst, sOffOn[0]);
            break;
        }
        if (r->fmt == SF_LEVEL)
            dst = StrCopy(dst, "Lv. ");
        if (r->fmt == SF_SIGNED_PERCENT && v > 0)
            *dst++ = '+';
        dst = IntToStr(dst, v);
        if (r->fmt == SF_PERCENT || r->fmt == SF_SIGNED_PERCENT)
            StrCopy(dst, "%");
        break;
    case RK_TEXT:
        switch (r->fmt)
        {
        case TF_SEED:
            if (S->seedText[0] == 0)
                StrCopy(dst, "(random)");
            else
            {
                u32 i;
                for (i = 0; i < RH_SEED_TEXT_LENGTH && S->seedText[i] && S->seedText[i] != 0xFFFF; i++)
                    dst[i] = (char)S->seedText[i];
                dst[i] = 0;
            }
            break;
        case TF_STARTER1: case TF_STARTER2: case TF_STARTER3:
        {
            u16 sp = S->customStarters[r->fmt - TF_STARTER1];
            StrCopy(dst, SpeciesName(sp ? sp : sVanillaStarters[r->fmt - TF_STARTER1]));
            break;
        }
        case TF_TYPE:
            StrCopy(dst, S->starterSingleType < 18 ? sTypeNames[S->starterSingleType] : "(random)");
            break;
        case TF_BST_MIN:
            IntToStr(dst, S->starterBstMin);
            break;
        case TF_BST_MAX:
            IntToStr(dst, S->starterBstMax);
            break;
        }
        break;
    }
}

// ---------------------------------------------------------------------------
// Seed
// ---------------------------------------------------------------------------
static const char sCodeChars[] = "ABCDEFGHJKLMNPQRSTUVWXYZ23456789";

static u32 HashSeedText(const u16 *s)
{
    u32 h = 2166136261u, i;
    for (i = 0; i < RH_SEED_TEXT_LENGTH && s[i] && s[i] != 0xFFFF; i++)
        h = (h ^ s[i]) * 16777619u;
    return h ? h : 1;
}

static u32 sRandState;

static u32 MenuRandom(void)
{
    sRandState = sRandState * 1103515245 + 12345 + gSystem.vblankCounter;
    return (sRandState >> 8) ^ (gf_rand() << 16);
}

static void RandomSeedText(void)
{
    u32 i;
    for (i = 0; i < 8; i++)
        S->seedText[i] = sCodeChars[MenuRandom() % 32];
    S->seedText[8] = 0;
    S->seed = HashSeedText(S->seedText);
}

// ---------------------------------------------------------------------------
// Settings codes (same format as FireRed/Emerald: rows bit-packed in sRows order, seed, extras, checksum)
// ---------------------------------------------------------------------------
#define CODE_VERSION      2         // 2: Misc. Tweaks "No Random Phone Calls" replaced "Run Without Running Shoes"
#define CODE_MAX_BYTES    96
#define CODE_MAX_CHARS    (CODE_MAX_BYTES * 8 / 5)
#define CODE_CHECK_BITS   10
#define NUM_SPECIES_CODE  1076

static u8 sCode[CODE_MAX_CHARS + 1];

static u32 BitsFor(u32 maxValue)
{
    u32 b = 0;
    while ((1u << b) <= maxValue)
        b++;
    return b;
}

static void PutBits(u8 *buf, u32 *pos, u32 value, u32 n)
{
    u32 i;
    for (i = 0; i < n; i++, (*pos)++)
    {
        if (*pos >= CODE_MAX_BYTES * 8)
            return;
        if (value & (1u << i))
            buf[*pos / 8] |= 1 << (*pos % 8);
        else
            buf[*pos / 8] &= ~(1 << (*pos % 8));
    }
}

static u32 GetBits(const u8 *buf, u32 *pos, u32 n)
{
    u32 i, v = 0;
    for (i = 0; i < n; i++, (*pos)++)
        if (*pos < CODE_MAX_BYTES * 8 && (buf[*pos / 8] & (1 << (*pos % 8))))
            v |= 1u << i;
    return v;
}

static BOOL RowRange(const struct RhRow *r, s32 *base, u32 *span)
{
    switch (r->kind)
    {
    case RK_CHOICE:
    case RK_TOGGLE:
        *base = 0;
        *span = (IsGenRow(r) && !GenRowHasOff(r)) ? 9 : ChoiceCount(r) - 1;
        return TRUE;
    case RK_SLIDER:
        *base = r->min < 0 ? r->min : 0;
        *span = r->max - *base;
        return TRUE;
    }
    return FALSE;
}

static u32 RowCode(const struct RhRow *r, const struct RhSettings *st, s32 base, u32 span)
{
    u32 v = (u32)(RowGet(r, st) - base);
    return v < span ? v : span;
}

static BOOL SeedTextIsCode(const u16 *t)
{
    u32 i, k;
    for (i = 0; i < RH_SEED_TEXT_LENGTH && t[i] && t[i] != 0xFFFF; i++)
    {
        for (k = 0; k < 32 && (u16)sCodeChars[k] != t[i]; k++)
            ;
        if (k == 32)
            return FALSE;
    }
    return i > 0;
}

static u32 CodeCheck(const u8 *buf, u32 bits)
{
    u32 h = 2166136261u, i, pos = 0;
    for (i = 0; i < bits; i++)
        h = (h ^ GetBits(buf, &pos, 1) ^ (i << 1)) * 16777619u;
    return (h ^ (h >> 13)) & ((1 << CODE_CHECK_BITS) - 1);
}

static struct RhSettings sDefaults;

static BOOL ExtrasAreDefault(const struct RhSettings *st)
{
    return st->customStarters[0] == 0 && st->customStarters[1] == 0 && st->customStarters[2] == 0
        && st->starterSingleType == sDefaults.starterSingleType
        && st->starterBstMin == sDefaults.starterBstMin && st->starterBstMax == sDefaults.starterBstMax;
}

static u32 PackSettings(const struct RhSettings *st, u8 *buf)
{
    u32 pos = 0, i, fullBits = 0, sparseBits, changed = 0;
    const u32 idxBits = BitsFor(ROW_COUNT - 1);
    s32 base;
    u32 span;
    RH_SetDefaultSettings(&sDefaults);
    memset(buf, 0, CODE_MAX_BYTES);
    PutBits(buf, &pos, CODE_VERSION, 3);
    if (SeedTextIsCode(st->seedText))
    {
        u32 len;
        for (len = 0; len < RH_SEED_TEXT_LENGTH && st->seedText[len] && st->seedText[len] != 0xFFFF; len++)
            ;
        PutBits(buf, &pos, 1, 1);
        PutBits(buf, &pos, len - 1, 4);
        for (i = 0; i < len; i++)
        {
            u32 k;
            for (k = 0; (u16)sCodeChars[k] != st->seedText[i]; k++)
                ;
            PutBits(buf, &pos, k, 5);
        }
    }
    else
    {
        PutBits(buf, &pos, 0, 1);
        PutBits(buf, &pos, st->seed, 32);
    }
    for (i = 0; i < ROW_COUNT; i++)
    {
        if (!RowRange(&sRows[i], &base, &span))
            continue;
        fullBits += BitsFor(span);
        if (RowCode(&sRows[i], st, base, span) != RowCode(&sRows[i], &sDefaults, base, span))
            changed++;
    }
    sparseBits = idxBits;
    for (i = 0; i < ROW_COUNT; i++)
        if (RowRange(&sRows[i], &base, &span) && RowCode(&sRows[i], st, base, span) != RowCode(&sRows[i], &sDefaults, base, span))
            sparseBits += idxBits + BitsFor(span);
    if (sparseBits < fullBits)
    {
        PutBits(buf, &pos, 1, 1);
        PutBits(buf, &pos, changed, idxBits);
        for (i = 0; i < ROW_COUNT; i++)
        {
            if (RowRange(&sRows[i], &base, &span) && RowCode(&sRows[i], st, base, span) != RowCode(&sRows[i], &sDefaults, base, span))
            {
                PutBits(buf, &pos, i, idxBits);
                PutBits(buf, &pos, RowCode(&sRows[i], st, base, span), BitsFor(span));
            }
        }
    }
    else
    {
        PutBits(buf, &pos, 0, 1);
        for (i = 0; i < ROW_COUNT; i++)
            if (RowRange(&sRows[i], &base, &span))
                PutBits(buf, &pos, RowCode(&sRows[i], st, base, span), BitsFor(span));
    }
    if (ExtrasAreDefault(st))
    {
        PutBits(buf, &pos, 0, 1);
    }
    else
    {
        PutBits(buf, &pos, 1, 1);
        for (i = 0; i < 3; i++)
            PutBits(buf, &pos, st->customStarters[i] < NUM_SPECIES_CODE ? st->customStarters[i] : 0, BitsFor(NUM_SPECIES_CODE - 1));
        PutBits(buf, &pos, st->starterSingleType, 8);
        PutBits(buf, &pos, st->starterBstMin > 1023 ? 1023 : st->starterBstMin, 10);
        PutBits(buf, &pos, st->starterBstMax > 1023 ? 1023 : st->starterBstMax, 10);
    }
    i = CodeCheck(buf, pos);
    PutBits(buf, &pos, i, CODE_CHECK_BITS);
    return pos;
}

static struct RhSettings sTmp;

static BOOL UnpackSettings(struct RhSettings *st, const u8 *buf, u32 bits)
{
    u32 pos = 0, i, payload, v;
    const u32 idxBits = BitsFor(ROW_COUNT - 1);
    s32 base;
    u32 span;
    if (bits < 3 + CODE_CHECK_BITS || GetBits(buf, &pos, 3) != CODE_VERSION)
        return FALSE;
    RH_SetDefaultSettings(&sDefaults);
    sTmp = *st;
    if (GetBits(buf, &pos, 1))
    {
        u32 len = GetBits(buf, &pos, 4) + 1;
        for (i = 0; i < len && i < RH_SEED_TEXT_LENGTH; i++)
            sTmp.seedText[i] = sCodeChars[GetBits(buf, &pos, 5)];
        sTmp.seedText[i] = 0;
        sTmp.seed = HashSeedText(sTmp.seedText);
    }
    else
    {
        static const char sCodeSeed[] = "(code)";
        sTmp.seed = GetBits(buf, &pos, 32);
        if (sTmp.seed == 0)
            sTmp.seed = 1;
        for (i = 0; sCodeSeed[i]; i++)
            sTmp.seedText[i] = sCodeSeed[i];
        sTmp.seedText[i] = 0;
    }
    for (i = 0; i < ROW_COUNT; i++)
        if (RowRange(&sRows[i], &base, &span))
            RowSet(&sRows[i], &sTmp, RowGet(&sRows[i], &sDefaults));
    if (GetBits(buf, &pos, 1))
    {
        u32 count = GetBits(buf, &pos, idxBits), k;
        for (k = 0; k < count && pos < bits; k++)
        {
            i = GetBits(buf, &pos, idxBits);
            if (i >= ROW_COUNT || !RowRange(&sRows[i], &base, &span))
            {
                pos = bits + 1;
                break;
            }
            v = GetBits(buf, &pos, BitsFor(span));
            RowSet(&sRows[i], &sTmp, base + (v < span ? v : span));
        }
    }
    else
    {
        for (i = 0; i < ROW_COUNT; i++)
        {
            if (RowRange(&sRows[i], &base, &span))
            {
                v = GetBits(buf, &pos, BitsFor(span));
                RowSet(&sRows[i], &sTmp, base + (v < span ? v : span));
            }
        }
    }
    for (i = 0; i < 3; i++)
        sTmp.customStarters[i] = 0;
    sTmp.starterSingleType = sDefaults.starterSingleType;
    sTmp.starterBstMin = sDefaults.starterBstMin;
    sTmp.starterBstMax = sDefaults.starterBstMax;
    if (GetBits(buf, &pos, 1))
    {
        for (i = 0; i < 3; i++)
        {
            v = GetBits(buf, &pos, BitsFor(NUM_SPECIES_CODE - 1));
            sTmp.customStarters[i] = (v < NUM_SPECIES_CODE && SpeciesName(v)[0]) ? v : 0;
        }
        v = GetBits(buf, &pos, 8);
        sTmp.starterSingleType = v < 18 ? v : TYPE_NONE_SETTING;
        v = GetBits(buf, &pos, 10);
        sTmp.starterBstMin = v > 999 ? 999 : v;
        v = GetBits(buf, &pos, 10);
        sTmp.starterBstMax = v > 999 ? 999 : v;
    }
    payload = pos;
    if (payload + CODE_CHECK_BITS > bits || bits - (payload + CODE_CHECK_BITS) >= 5
     || GetBits(buf, &pos, CODE_CHECK_BITS) != CodeCheck(buf, payload))
        return FALSE;
    sTmp.magic = RH_SETTINGS_MAGIC;
    *st = sTmp;
    return TRUE;
}

static u32 SettingsToCode(const struct RhSettings *st, u8 *chars)
{
    u8 buf[CODE_MAX_BYTES];
    u32 bits = PackSettings(st, buf), n = (bits + 4) / 5, i, pos = 0;
    for (i = 0; i < n; i++)
        chars[i] = GetBits(buf, &pos, 5);
    return n;
}

static BOOL CodeToSettings(struct RhSettings *st, const u8 *chars, u32 n)
{
    u8 buf[CODE_MAX_BYTES];
    u32 i, pos = 0;
    if (n == 0 || n > CODE_MAX_CHARS)
        return FALSE;
    memset(buf, 0, sizeof(buf));
    for (i = 0; i < n; i++)
        PutBits(buf, &pos, chars[i], 5);
    return UnpackSettings(st, buf, n * 5);
}

// ---------------------------------------------------------------------------
// Drawing
// ---------------------------------------------------------------------------
static const char *SectionName(u32 sec)
{
    if (sec == SEC_BEGIN && sInGame)
        return "Apply or Exit";
    return sSectionNames[sec];
}

static void DrawHeader(void)
{
    const char *hint = sInGame ? "SELECT Menu  B Exit" : "SELECT Menu  START Begin";
    Rect(SCR_TOP, 0, 0, 256, 16, C_HEADBAR);
    Text(SCR_TOP, 4, 0, "=", C_ACCENT, C_SHADOW);
    Text(SCR_TOP, 14, 0, SectionName(sSection), C_TEXT, C_SHADOW);
    if (14 + TextWidth(SectionName(sSection)) + 8 > 252 - TextWidth(hint))
        hint = sInGame ? "B Exit" : "SELECT Menu";
    TextRight(SCR_TOP, 252, 0, hint, C_DIM, 0);
}

// ---------------------------------------------------------------------------
// Presets: three settings codes in a spare flash sector (62: hg-engine's extra save chunks end at sector 58),
// which erasing the save (Reset The Run, the game's own delete) leaves alone (src/save.c Save_DeleteAllData).
// ---------------------------------------------------------------------------
#define PRESET_COUNT  3
#define PRESET_MAGIC  0x52485053   // "RHPS"
#define PRESET_OFFSET (62 * 0x1000)
struct RhPresetSector
{
    u32 magic;
    u16 bits[PRESET_COUNT];        // 0 = empty slot
    u16 pad;
    u8 data[PRESET_COUNT][CODE_MAX_BYTES];
};
static struct RhPresetSector sPresets;

s32 LONG_CALL FlashWriteChunk(u32 offset, void *data, u32 size);
BOOL LONG_CALL FlashLoadChunk(u32 offset, void *data, u32 size);
// one step of the game's soft reset (arm9 0x02000F40, pret DoSoftReset's loop): resets once the card is idle
#define SoftResetStep ((void (*)(u32))(0x02000F40 | 1))

static void ReadPresets(void)
{
    u32 i;
    FlashLoadChunk(PRESET_OFFSET, &sPresets, sizeof(sPresets));
    if (sPresets.magic != PRESET_MAGIC)
    {
        memset(&sPresets, 0, sizeof(sPresets));
        sPresets.magic = PRESET_MAGIC;
    }
    sPresetUsed = 0;
    for (i = 0; i < PRESET_COUNT; i++)
        if (sPresets.bits[i] != 0)
            sPresetUsed |= 1 << i;
}

static BOOL SavePreset(u32 slot)
{
    BOOL ok;
    ReadPresets();
    sPresets.bits[slot] = PackSettings(S, sPresets.data[slot]);
    ok = FlashWriteChunk(PRESET_OFFSET, &sPresets, sizeof(sPresets)) != 0;
    ReadPresets();
    return ok && (sPresetUsed & (1 << slot));
}

static BOOL LoadPreset(u32 slot)
{
    ReadPresets();
    return sPresets.bits[slot] != 0 && UnpackSettings(S, sPresets.data[slot], sPresets.bits[slot]);
}

static void DrawAll(void);
static void FixCursor(s32 dir);
static void GoToSection(u32 sec);

static void EraseGameSave(void)
{
    // the game's own way to invalidate both save slots (Save_DeleteAllData: spoil each chunk's footer), then the
    // extra data (Hall of Fame, battle videos...: sectors 47-61 and their copies) - not the presets' sector 62.
    // (Save_DeleteAllData itself rewrites all 128 sectors, which takes long, from heap 3, full under this screen)
    SaveData *save = SaveBlock2_get();
    u8 *buf = sys_AllocMemory(HEAPID_RH_MENU, 0x1000);
    u32 i;
    FlashClobberChunkFooter(save, 0, save->lastGoodSector == 0 ? 1 : 0);
    FlashClobberChunkFooter(save, 1, save->lastGoodSector == 0 ? 1 : 0);
    FlashClobberChunkFooter(save, 0, save->lastGoodSector);
    FlashClobberChunkFooter(save, 1, save->lastGoodSector);
    memset(buf, 0xFF, 0x1000);
    for (i = 47; i < PRESET_OFFSET / 0x1000; i++)
    {
        FlashWriteChunk(i * 0x1000, buf, 0x1000);
        FlashWriteChunk((i + 64) * 0x1000, buf, 0x1000);
    }
    sys_FreeMemoryEz(buf);
    save->saveFileExists = FALSE;
}

// Reset The Run (two confirmations): erase the saved game, not the presets. In-game the game then restarts (title
// screen); at New Game the options screen stays, with a new seed.
static void ResetTheRun(void)
{
    EraseGameSave();
    if (sInGame)
        for (;;)
            SoftResetStep(0);             // as the game's own soft reset: once the cartridge is idle, restart
    RandomSeedText();
    PlaySE(SEQ_SE_DP_SAVE);
    GoToSection(SEC_GENERAL);
    FixCursor(1);
    sMessage = "Your saved game was erased. Pick your\nsettings and begin a new run.";
    DrawAll();
}

static const char *ActionValueText(u32 act)
{
    if (act >= ACT_SAVE_1 && act <= ACT_SAVE_3)
        return (sPresetUsed & (1 << (act - ACT_SAVE_1))) ? "In use" : "";
    if (act >= ACT_LOAD_1 && act <= ACT_LOAD_3)
        return (sPresetUsed & (1 << (act - ACT_LOAD_1))) ? "Saved" : "Empty";
    return "";
}

static void DrawList(void)
{
    const struct RhRow *rows[ROW_COUNT];
    u32 n = VisibleRows(sSection, rows), i;
    BOOL dimAll = !S->enabled && sSection != SEC_GENERAL && sSection != SEC_MISC && sSection != SEC_GRAPHICS && sSection != SEC_BEGIN
                  && sSection != SEC_CODES;

    Rect(SCR_TOP, 0, 16, 256, 176, C_PANEL);
    for (i = 0; i < LIST_ROWS && sScroll + i < n; i++)
    {
        const struct RhRow *r = rows[sScroll + i];
        s32 y = 16 + i * ROW_H;
        s32 x = 8 + r->indent * 8;
        char buf[48];
        BOOL selected = (sScroll + i == sRow);
        if (selected)
        {
            BOOL reset = (r->kind == RK_ACTION && r->fmt == ACT_RESET_RUN);
            Rect(SCR_TOP, 0, y, 256, ROW_H, reset ? C_BLOOD_SEL : C_SEL);
            Rect(SCR_TOP, 0, y, 3, ROW_H, reset ? C_BLOOD : C_ACCENT);
        }
        if (r->kind == RK_HEADER)
        {
            Text(SCR_TOP, 6, y, r->label, C_HEADER, C_SHADOW);
            continue;
        }
        if (r->kind == RK_ACTION)
        {
            if (r->fmt == ACT_BEGIN)
                TextCenter(SCR_TOP, 128, y, sInGame ? "▶ APPLY & RETURN" : "▶ BEGIN RUN", C_OK, C_SHADOW);
            else if (r->fmt == ACT_RESET_RUN)
                Text(SCR_TOP, x, y, r->label, C_BLOOD, C_SHADOW);
            else
            {
                const char *v = ActionValueText(r->fmt);
                Text(SCR_TOP, x, y, r->label, C_TEXT, C_SHADOW);
                TextRight(SCR_TOP, 244, y, v, (r->fmt >= ACT_LOAD_1 && r->fmt <= ACT_LOAD_3 && v[0] == 'E') ? C_DIM : C_VALUE, C_VALUE_SH);
            }
            continue;
        }
        Text(SCR_TOP, x, y, r->label, dimAll ? C_DIM : C_TEXT, C_SHADOW);
        FormatValue(r, buf);
        if (r->kind == RK_CHOICE && r->choices != sOffOn)
        {
            char tmp[56], *p = tmp;
            p = StrCopy(p, "‹ ");
            p = StrCopy(p, buf);
            StrCopy(p, " ›");
            StrCopy(buf, tmp);
        }
        TextRight(SCR_TOP, 244, y, buf, dimAll ? C_DIM : (r->kind == RK_TEXT ? C_ACCENT : C_VALUE), dimAll ? C_SHADOW : C_VALUE_SH);
    }
    if (sScroll > 0)
        Text(SCR_TOP, 246, 16, "↑", C_DIM, 0);
    if (sScroll + LIST_ROWS < n)
        Text(SCR_TOP, 246, 16 + (LIST_ROWS - 1) * ROW_H, "↓", C_DIM, 0);
}

static void DrawMenu(void)
{
    u32 i;
    if (!sMenuOpen)
        return;
    Rect(SCR_TOP, 8, 16, 196, 4 + SEC_COUNT * MENU_ROW_H, C_HEADBAR);
    Rect(SCR_TOP, 8, 16, 196, 2, C_ACCENT);
    for (i = 0; i < SEC_COUNT; i++)
    {
        s32 y = 18 + i * MENU_ROW_H;
        if (i == sMenuCursor)
            Rect(SCR_TOP, 8, y, 196, MENU_ROW_H, C_SEL);
        Text(SCR_TOP, 16, y - 2, SectionName(i), i == SEC_BEGIN ? C_OK : (i == sSection ? C_VALUE : C_TEXT), C_SHADOW);
    }
}

static void DrawFooter(const char *line1, const char *line2)
{
    Rect(SCR_BOTTOM, 0, 160, 256, 32, C_HEADBAR);
    Text(SCR_BOTTOM, 8, 160, line1, C_DIM, 0);
    Text(SCR_BOTTOM, 8, 175, line2, C_DIM, 0);
}

static const char *ConfirmQuestion(u8 *colour)
{
    *colour = C_TEXT;
    if (sPendingAction >= ACT_SAVE_1 && sPendingAction <= ACT_SAVE_3)
        return "Overwrite this preset?";
    if (sPendingAction == ACT_DEFAULTS)
        return "Reset every option to its default?";
    if (sPendingAction == ACT_EXIT)
        return "Leave without changing anything?";
    if (sPendingAction == ACT_RESET_RUN)
        return *colour = C_BLOOD, "Reset the run? Your save gets ERASED.";
    if (sPendingAction == ACT_RESET_RUN_2)
        return *colour = C_BLOOD, "Are you REALLY sure? No undo!";
    if (sInGame)
        return "Apply these settings to your run?";
    return S->enabled ? "Begin a RANDOMIZED run?" : "Begin a normal (not randomized) run?";
}

static void DrawKeyboard(void);
static void DrawCodeScreen(void);

static void DrawBottom(void)
{
    const struct RhRow *rows[ROW_COUNT];
    u32 n = VisibleRows(sSection, rows);
    const struct RhRow *r = (sRow < n) ? rows[sRow] : NULL;

    Fill(SCR_BOTTOM, C_BG);
    if (sEditing)
    {
        DrawKeyboard();
        return;
    }
    if (sCodeMode)
    {
        DrawCodeScreen();
        return;
    }
    // title bar: the selected option
    Rect(SCR_BOTTOM, 0, 0, 256, 16, C_HEADBAR);
    Rect(SCR_BOTTOM, 0, 16, 256, 2, C_ACCENT);
    Text(SCR_BOTTOM, 8, 0, (r != NULL && r->kind != RK_HEADER) ? r->label : SectionName(sSection), C_HEADER, C_SHADOW);
    Rect(SCR_BOTTOM, 0, 18, 256, 142, C_PANEL);
    if (sConfirm)
    {
        u8 col;
        const char *q = ConfirmQuestion(&col);
        Text(SCR_BOTTOM, 8, 28, q, col, C_SHADOW);
        Rect(SCR_BOTTOM, 56, 64, 56, 18, sConfirm == 1 ? C_SEL : C_HEADBAR);
        Rect(SCR_BOTTOM, 144, 64, 56, 18, sConfirm == 2 ? C_SEL : C_HEADBAR);
        TextCenter(SCR_BOTTOM, 84, 65, "YES", sConfirm == 1 ? C_OK : C_DIM, C_SHADOW);
        TextCenter(SCR_BOTTOM, 172, 65, "NO", sConfirm == 2 ? C_WARN : C_DIM, C_SHADOW);
        DrawFooter("←→ Choose   A Confirm", "B Cancel");
        return;
    }
    if (sMessage != NULL)
        Text(SCR_BOTTOM, 8, 28, sMessage, C_WARN, C_SHADOW);
    else if (sInGame && r != NULL && r->kind == RK_ACTION && r->fmt == ACT_BEGIN)
        Text(SCR_BOTTOM, 8, 28, "Use these settings from now on. Save\nyour game to keep them. Press A.", C_TEXT, C_SHADOW);
    else if (r != NULL && r->desc != NULL)
        Text(SCR_BOTTOM, 8, 28, r->desc, C_TEXT, C_SHADOW);
    if (sMenuOpen)
        DrawFooter("↑↓ Section   A Open", "B/SELECT Close");
    else
        DrawFooter("↑↓ Option   ←→ Change   A Select", sInGame ? "L/R Section   SELECT Menu   B Exit" : "L/R Section   SELECT Menu   START Begin");
}

static void DrawAll(void)
{
    DrawHeader();
    DrawList();
    DrawMenu();
    DrawBottom();
}

// Keeps sRow on a selectable row and inside the scroll window.
static void FixCursor(s32 dir)
{
    const struct RhRow *rows[ROW_COUNT];
    u32 n = VisibleRows(sSection, rows), guard;
    if (n == 0)
    {
        sRow = sScroll = 0;
        return;
    }
    if (sRow >= n)
        sRow = n - 1;
    for (guard = 0; guard < n && rows[sRow]->kind == RK_HEADER; guard++)
        sRow = (dir < 0) ? (sRow == 0 ? n - 1 : sRow - 1) : (sRow + 1) % n;
    if (sRow < sScroll)
        sScroll = sRow;
    if (sRow >= sScroll + LIST_ROWS)
        sScroll = sRow - LIST_ROWS + 1;
    if (sRow > 0 && sScroll == sRow && rows[sRow - 1]->kind == RK_HEADER)
        sScroll--;                                   // show the section header above the first row
}

static void GoToSection(u32 sec)
{
    const struct RhRow *rows[ROW_COUNT];
    u32 n;
    sSection = sec;
    sRow = 0;
    sScroll = 0;
    sMessage = NULL;
    FixCursor(1);
    // the cursor starts below "Reset The Run", never on it
    n = VisibleRows(sSection, rows);
    if (n > 1 && rows[sRow]->kind == RK_ACTION && rows[sRow]->fmt == ACT_RESET_RUN)
        sRow++;
}

// ---------------------------------------------------------------------------
// Text entry (on the bottom screen)
// ---------------------------------------------------------------------------
static const char sKeys[] = "ABCDEFGHIJKLM" "NOPQRSTUVWXYZ" "abcdefghijklm" "nopqrstuvwxyz" "0123456789-. ";
#define KEY_COLS 13
#define KEY_COUNT (sizeof(sKeys) - 1)

static void DrawKeyboard(void)
{
    static const char *const sTitles[] = { "Seed (any text):", "Starter Pokémon:", "Starter Pokémon:", "Starter Pokémon:",
                                           "Starter type:", "Base stat total:", "Base stat total:" };
    u32 i;
    char ch[2];
    Rect(SCR_BOTTOM, 0, 0, 256, 16, C_HEADBAR);
    Rect(SCR_BOTTOM, 0, 16, 256, 2, C_ACCENT);
    Text(SCR_BOTTOM, 8, 0, sTitles[sEditField], C_HEADER, C_SHADOW);
    Rect(SCR_BOTTOM, 8, 24, 240, 20, C_PANEL);
    sEditBuf[sEditLen] = 0;
    i = Text(SCR_BOTTOM, 14, 26, sEditBuf, C_VALUE, C_VALUE_SH);
    Text(SCR_BOTTOM, i, 26, "_", C_ACCENT, 0);
    for (i = 0; i < KEY_COUNT; i++)
    {
        s32 x = 10 + (i % KEY_COLS) * 18, y = 54 + (i / KEY_COLS) * 20;
        ch[0] = sKeys[i];
        ch[1] = 0;
        if (i == sKeyCursor)
            Rect(SCR_BOTTOM, x - 3, y, 16, 18, C_SEL);
        TextCenter(SCR_BOTTOM, x + 5, y + 1, ch[0] == ' ' ? "·" : ch, i == sKeyCursor ? C_ACCENT : C_TEXT, C_SHADOW);
    }
    if (sMessage != NULL)
        Text(SCR_BOTTOM, 8, 156, sMessage, C_WARN, C_SHADOW);
    else
        DrawFooter("A Type   B Delete   START Done", "SELECT Cancel   (blank = default)");
}

static BOOL NamesEqual(const char *a, const char *b)
{
    for (; *a && *b; a++, b++)
    {
        char ca = *a, cb = *b;
        if (ca >= 'a' && ca <= 'z') ca -= 'a' - 'A';
        if (cb >= 'a' && cb <= 'z') cb -= 'a' - 'A';
        if (ca != cb)
        {
            // é (0xC3 0xA9) matches E
            if ((u8)ca == 0xC3 && b[0] == 'E' - 0 && (u8)a[1] == 0xA9)
            {
                a++;
                continue;
            }
            if ((u8)cb == 0xC3 && (ca == 'E' || ca == 'e') && (u8)b[1] == 0xA9)
            {
                b++;
                continue;
            }
            return FALSE;
        }
    }
    return *a == 0 && *b == 0;
}

static u16 FindSpeciesByName(const char *name)
{
    u32 i;
    for (i = 1; i < RH_NAME_COUNT; i++)
        if (NamesEqual(SpeciesName(i), name))
            return i;
    return 0;
}

static u8 FindTypeByName(const char *name)
{
    u32 i;
    for (i = 0; i < 18; i++)
        if (NamesEqual(sTypeNames[i], name))
            return i;
    return TYPE_NONE_SETTING;
}

static u16 ParseNumber(const char *s)
{
    u32 v = 0;
    for (; *s; s++)
        if (*s >= '0' && *s <= '9')
            v = v * 10 + (*s - '0');
    return v > 999 ? 999 : v;
}

static void StartTextEntry(u32 field)
{
    u32 i;
    sEditing = TRUE;
    sEditField = field;
    sEditLen = 0;
    sKeyCursor = 0;
    sMessage = NULL;
    switch (field)
    {
    case TF_SEED:
        for (i = 0; i < RH_SEED_TEXT_LENGTH && S->seedText[i] && S->seedText[i] != 0xFFFF; i++)
            sEditBuf[sEditLen++] = (char)S->seedText[i];
        break;
    case TF_STARTER1: case TF_STARTER2: case TF_STARTER3:
        if (S->customStarters[field - TF_STARTER1])
            sEditLen = StrCopy(sEditBuf, SpeciesName(S->customStarters[field - TF_STARTER1])) - sEditBuf;
        break;
    case TF_TYPE:
        if (S->starterSingleType < 18)
            sEditLen = StrCopy(sEditBuf, sTypeNames[S->starterSingleType]) - sEditBuf;
        break;
    default:
        sEditLen = IntToStr(sEditBuf, field == TF_BST_MIN ? S->starterBstMin : S->starterBstMax) - sEditBuf;
        break;
    }
    if (sEditLen > TEXT_MAX)
        sEditLen = TEXT_MAX;
    sEditBuf[sEditLen] = 0;
}

// TRUE when the entry was accepted (or cancelled)
static BOOL FinishTextEntry(void)
{
    u32 i;
    sEditBuf[sEditLen] = 0;
    sMessage = NULL;
    switch (sEditField)
    {
    case TF_SEED:
        if (sEditLen == 0)
            RandomSeedText();
        else
        {
            for (i = 0; i < RH_SEED_TEXT_LENGTH && i < sEditLen; i++)
                S->seedText[i] = (u8)sEditBuf[i];
            S->seedText[i] = 0;
            S->seed = HashSeedText(S->seedText);
        }
        break;
    case TF_STARTER1: case TF_STARTER2: case TF_STARTER3:
        if (sEditLen == 0)
            S->customStarters[sEditField - TF_STARTER1] = 0;
        else
        {
            u16 sp = FindSpeciesByName(sEditBuf);
            if (sp == 0)
            {
                sMessage = "No Pokémon has that name.";
                return FALSE;
            }
            S->customStarters[sEditField - TF_STARTER1] = sp;
        }
        break;
    case TF_TYPE:
        if (sEditLen == 0)
            S->starterSingleType = TYPE_NONE_SETTING;
        else
        {
            u8 t = FindTypeByName(sEditBuf);
            if (t == TYPE_NONE_SETTING)
            {
                sMessage = "That's not a type (e.g. FIRE).";
                return FALSE;
            }
            S->starterSingleType = t;
        }
        break;
    case TF_BST_MIN:
        S->starterBstMin = ParseNumber(sEditBuf);
        break;
    case TF_BST_MAX:
        S->starterBstMax = ParseNumber(sEditBuf);
        break;
    }
    return TRUE;
}

static void TextEntryInput(u32 newKeys, u32 rep)
{
    if (sMessage != NULL && (newKeys & (PAD_BUTTON_A | PAD_BUTTON_B | PAD_KEY_ANY)))
    {
        sMessage = NULL;
        DrawBottom();
        if (newKeys & (PAD_BUTTON_A | PAD_BUTTON_B))
            return;
    }
    if (rep & PAD_KEY_LEFT)
        sKeyCursor = (sKeyCursor % KEY_COLS == 0) ? sKeyCursor + KEY_COLS - 1 : sKeyCursor - 1;
    else if (rep & PAD_KEY_RIGHT)
        sKeyCursor = (sKeyCursor % KEY_COLS == KEY_COLS - 1) ? sKeyCursor - KEY_COLS + 1 : sKeyCursor + 1;
    else if (rep & PAD_KEY_UP)
        sKeyCursor = (sKeyCursor + KEY_COUNT - KEY_COLS) % KEY_COUNT;
    else if (rep & PAD_KEY_DOWN)
        sKeyCursor = (sKeyCursor + KEY_COLS) % KEY_COUNT;
    else if (rep & PAD_BUTTON_A)
    {
        if (sEditLen < (sEditField == TF_SEED ? RH_SEED_TEXT_LENGTH : TEXT_MAX))
            sEditBuf[sEditLen++] = sKeys[sKeyCursor];
    }
    else if (rep & PAD_BUTTON_B)
    {
        if (sEditLen > 0)
            sEditLen--;
    }
    else if (newKeys & PAD_BUTTON_START)
    {
        if (FinishTextEntry())
        {
            sEditing = FALSE;
            PlaySE(SEQ_SE_DP_DECIDE);
            DrawAll();
        }
        else
        {
            PlaySE(SEQ_SE_DP_BEEP);
            DrawBottom();
        }
        return;
    }
    else if (newKeys & PAD_BUTTON_SELECT)
    {
        sEditing = FALSE;
        sMessage = NULL;
        PlaySE(SEQ_SE_DP_SELECT);
        DrawAll();
        return;
    }
    else
        return;
    PlaySE(SEQ_SE_DP_SELECT);
    DrawBottom();
}

// ---------------------------------------------------------------------------
// Code screens (bottom screen)
// ---------------------------------------------------------------------------
#define CODE_PER_LINE 20

static void DrawCodeChars(s32 y0, BOOL cursor)
{
    u32 i;
    char str[2] = { 0, 0 };
    for (i = 0; i <= sCodeLen && i < CODE_MAX_CHARS; i++)
    {
        u32 line = i / CODE_PER_LINE, col = i % CODE_PER_LINE;
        s32 x = 12 + col * 10 + (col / 5) * 6, y = y0 + line * 16;
        if (i == sCodeLen)
        {
            if (cursor)
                Text(SCR_BOTTOM, x, y, "_", C_ACCENT, 0);
            break;
        }
        str[0] = sCodeChars[sCode[i] & 31];
        Text(SCR_BOTTOM, x, y, str, C_VALUE, C_VALUE_SH);
    }
}

static void DrawCodeScreen(void)
{
    u32 i;
    char str[2] = { 0, 0 };
    Rect(SCR_BOTTOM, 0, 0, 256, 16, C_HEADBAR);
    Rect(SCR_BOTTOM, 0, 16, 256, 2, C_ACCENT);
    Rect(SCR_BOTTOM, 0, 18, 256, 142, C_PANEL);
    if (sCodeMode == 1)
    {
        Text(SCR_BOTTOM, 8, 0, "Settings Code", C_HEADER, C_SHADOW);
        DrawCodeChars(28, FALSE);
        if (sMessage == NULL)
            DrawFooter("Write it down or share it. Friends can", "type it in to play the same game. B: back");
    }
    else
    {
        Text(SCR_BOTTOM, 8, 0, "Enter Settings Code", C_HEADER, C_SHADOW);
        DrawCodeChars(24, TRUE);
        for (i = 0; i < 32; i++)
        {
            s32 x = 10 + (i % 16) * 15, y = 112 + (i / 16) * 20;
            if (i == sCodeCursor)
                Rect(SCR_BOTTOM, x - 3, y, 14, 18, C_SEL);
            str[0] = sCodeChars[i];
            Text(SCR_BOTTOM, x, y + 1, str, i == sCodeCursor ? C_ACCENT : C_TEXT, C_SHADOW);
        }
        if (sMessage == NULL)
            DrawFooter("A Type   B Delete   START Load code", "SELECT Cancel");
    }
    if (sMessage != NULL)
    {
        Rect(SCR_BOTTOM, 0, 160, 256, 32, C_HEADBAR);
        Text(SCR_BOTTOM, 8, 160, sMessage, C_WARN, C_SHADOW);
    }
}

static void OpenCodeScreen(u32 mode)
{
    sCodeMode = mode;
    sMessage = NULL;
    sCodeLen = (mode == 1) ? SettingsToCode(S, sCode) : 0;
    sCodeCursor = 0;
}

static BOOL CodeScreenInput(u32 newKeys, u32 rep)
{
    if (sCodeMode == 0)
        return FALSE;
    if (sMessage != NULL && (newKeys & (PAD_BUTTON_A | PAD_BUTTON_B | PAD_BUTTON_START | PAD_BUTTON_SELECT | PAD_KEY_ANY)))
    {
        sMessage = NULL;
        DrawBottom();
        return TRUE;
    }
    if (sCodeMode == 1)
    {
        if (newKeys & (PAD_BUTTON_A | PAD_BUTTON_B | PAD_BUTTON_START | PAD_BUTTON_SELECT))
        {
            sCodeMode = 0;
            PlaySE(SEQ_SE_DP_SELECT);
            DrawAll();
        }
        return TRUE;
    }
    if (rep & PAD_KEY_LEFT)
        sCodeCursor = (sCodeCursor & 16) | ((sCodeCursor + 15) & 15);
    else if (rep & PAD_KEY_RIGHT)
        sCodeCursor = (sCodeCursor & 16) | ((sCodeCursor + 1) & 15);
    else if (rep & (PAD_KEY_UP | PAD_KEY_DOWN))
        sCodeCursor ^= 16;
    else if (rep & PAD_BUTTON_A)
    {
        if (sCodeLen < CODE_MAX_CHARS)
            sCode[sCodeLen++] = sCodeCursor;
    }
    else if (rep & PAD_BUTTON_B)
    {
        if (sCodeLen > 0)
            sCodeLen--;
    }
    else if (newKeys & PAD_BUTTON_SELECT)
    {
        sCodeMode = 0;
        PlaySE(SEQ_SE_DP_SELECT);
        DrawAll();
        return TRUE;
    }
    else if (newKeys & PAD_BUTTON_START)
    {
        if (CodeToSettings(S, sCode, sCodeLen))
        {
            sCodeMode = 0;
            PlaySE(SEQ_SE_DP_DECIDE);
            sMessage = "Code loaded! Check the options, then\nBegin Run.";
            FixCursor(1);
            DrawAll();
        }
        else
        {
            PlaySE(SEQ_SE_DP_BEEP);
            sMessage = "That code doesn't work. Check every\ncharacter (and its length) and retry.";
            DrawBottom();
        }
        return TRUE;
    }
    else
        return TRUE;
    PlaySE(SEQ_SE_DP_SELECT);
    DrawBottom();
    return TRUE;
}

// ---------------------------------------------------------------------------
// Actions
// ---------------------------------------------------------------------------
static void DoAction(u32 act, BOOL confirmed)
{
    sMessage = NULL;
    switch (act)
    {
    case ACT_SHOW_CODE:
        PlaySE(SEQ_SE_DP_SELECT);
        OpenCodeScreen(1);
        DrawBottom();
        return;
    case ACT_ENTER_CODE:
        PlaySE(SEQ_SE_DP_SELECT);
        OpenCodeScreen(2);
        DrawBottom();
        return;
    case ACT_SAVE_1: case ACT_SAVE_2: case ACT_SAVE_3:
        if (!confirmed && (sPresetUsed & (1 << (act - ACT_SAVE_1))))
        {
            sPendingAction = act;
            sConfirm = 2;                 // default NO for overwriting
            PlaySE(SEQ_SE_DP_SELECT);
            DrawBottom();
            return;
        }
        if (SavePreset(act - ACT_SAVE_1))
        {
            PlaySE(SEQ_SE_DP_SAVE);
            sMessage = "Preset saved.";
        }
        else
        {
            PlaySE(SEQ_SE_DP_BEEP);
            sMessage = "Couldn't save. Is your emulator's save\ntype set to 4 Mbit Flash?";
        }
        break;
    case ACT_LOAD_1: case ACT_LOAD_2: case ACT_LOAD_3:
        if (LoadPreset(act - ACT_LOAD_1))
        {
            PlaySE(SEQ_SE_DP_DECIDE);
            sMessage = "Preset loaded.";
        }
        else
        {
            PlaySE(SEQ_SE_DP_BEEP);
            sMessage = "This preset is empty.";
        }
        break;
    case ACT_EXIT:
        sPendingAction = act;
        sConfirm = 1;
        PlaySE(SEQ_SE_DP_SELECT);
        DrawBottom();
        return;
    case ACT_RESET_RUN:
        sPendingAction = act;
        sConfirm = 2;                     // default NO, twice
        PlaySE(SEQ_SE_DP_SELECT);
        DrawBottom();
        return;
    case ACT_DEFAULTS:
        if (!confirmed)
        {
            sPendingAction = act;
            sConfirm = 2;
            PlaySE(SEQ_SE_DP_SELECT);
            DrawBottom();
            return;
        }
        {
            u16 seedText[RH_SEED_TEXT_LENGTH + 1];
            u32 seed = S->seed;
            memcpy(seedText, S->seedText, sizeof(seedText));
            RH_SetDefaultSettings(S);
            S->seed = seed;
            memcpy(S->seedText, seedText, sizeof(seedText));
        }
        PlaySE(SEQ_SE_DP_SELECT);
        sMessage = "Every option is back to its default.";
        break;
    }
    FixCursor(1);
    DrawAll();
}

static void HandleInput(void)
{
    const struct RhRow *rows[ROW_COUNT];
    u32 n = VisibleRows(sSection, rows);
    const struct RhRow *r = (sRow < n) ? rows[sRow] : NULL;
    u32 newKeys = gSystem.newKeys, rep = gSystem.newAndRepeatedKeys;

    if (sEditing)
    {
        TextEntryInput(newKeys, rep);
        return;
    }
    if (CodeScreenInput(newKeys, rep))
        return;
    if (sMenuOpen)
    {
        if (newKeys & PAD_KEY_UP)
            sMenuCursor = (sMenuCursor + SEC_COUNT - 1) % SEC_COUNT;
        else if (newKeys & PAD_KEY_DOWN)
            sMenuCursor = (sMenuCursor + 1) % SEC_COUNT;
        else if (newKeys & PAD_BUTTON_A)
        {
            sMenuOpen = FALSE;
            GoToSection(sMenuCursor);
            PlaySE(SEQ_SE_DP_SELECT);
            DrawAll();
            return;
        }
        else if (newKeys & (PAD_BUTTON_B | PAD_BUTTON_SELECT))
            sMenuOpen = FALSE;
        else
            return;
        PlaySE(SEQ_SE_DP_SELECT);
        DrawAll();
        return;
    }

    if (sConfirm)
    {
        if (newKeys & (PAD_KEY_LEFT | PAD_KEY_RIGHT))
        {
            sConfirm = (sConfirm == 1) ? 2 : 1;
            PlaySE(SEQ_SE_DP_SELECT);
            DrawBottom();
        }
        else if (newKeys & PAD_BUTTON_A)
        {
            if (sConfirm == 1 && sPendingAction == ACT_RESET_RUN)
            {
                sPendingAction = ACT_RESET_RUN_2;     // first YES: ask a second, separate time
                sConfirm = 2;
                PlaySE(SEQ_SE_DP_SELECT);
                DrawBottom();
                return;
            }
            if (sConfirm == 1 && sPendingAction == ACT_BEGIN)
            {
                sConfirm = 0;
                PlaySE(SEQ_SE_DP_DECIDE);
                sExitTo = 1;
                return;
            }
            if (sConfirm == 1 && sPendingAction == ACT_EXIT)
            {
                sConfirm = 0;
                PlaySE(SEQ_SE_DP_DECIDE);
                sExitTo = 2;                  // back to the game, nothing changed
                return;
            }
            if (sConfirm == 1 && sPendingAction == ACT_RESET_RUN_2)
            {
                sConfirm = 0;
                ResetTheRun();
                return;
            }
            if (sConfirm == 1)
            {
                sConfirm = 0;
                DoAction(sPendingAction, TRUE);
            }
            else
            {
                sConfirm = 0;
                PlaySE(SEQ_SE_DP_SELECT);
                DrawBottom();
            }
        }
        else if (newKeys & PAD_BUTTON_B)
        {
            sConfirm = 0;
            PlaySE(SEQ_SE_DP_SELECT);
            DrawBottom();
        }
        return;
    }

    if (sMessage != NULL && (newKeys & (PAD_BUTTON_A | PAD_BUTTON_B | PAD_KEY_ANY)))
    {
        sMessage = NULL;
        DrawBottom();
        if (newKeys & (PAD_BUTTON_A | PAD_BUTTON_B))
            return;
    }

    if (sInGame && (newKeys & PAD_BUTTON_B))
    {
        DoAction(ACT_EXIT, FALSE);
    }
    else if (newKeys & PAD_BUTTON_SELECT)
    {
        sMenuOpen = TRUE;
        sMenuCursor = sSection;
        PlaySE(SEQ_SE_DP_SELECT);
        DrawAll();
    }
    else if (newKeys & PAD_BUTTON_START)
    {
        GoToSection(SEC_BEGIN);
        sConfirm = 1;
        sPendingAction = ACT_BEGIN;
        PlaySE(SEQ_SE_DP_SELECT);
        DrawAll();
    }
    else if (newKeys & (PAD_BUTTON_L | PAD_BUTTON_R))
    {
        GoToSection((sSection + ((newKeys & PAD_BUTTON_R) ? 1 : SEC_COUNT - 1)) % SEC_COUNT);
        PlaySE(SEQ_SE_DP_SELECT);
        DrawAll();
    }
    else if (rep & (PAD_KEY_UP | PAD_KEY_DOWN))
    {
        s32 dir = (rep & PAD_KEY_UP) ? -1 : 1;
        if (n == 0)
            return;
        sRow = (dir < 0) ? (sRow == 0 ? n - 1 : sRow - 1) : (sRow + 1) % n;
        if (dir > 0 && sRow == 0)
            sScroll = 0;
        FixCursor(dir);
        PlaySE(SEQ_SE_DP_SELECT);
        DrawList();
        DrawBottom();
    }
    else if (r != NULL && (rep & (PAD_KEY_LEFT | PAD_KEY_RIGHT)))
    {
        s32 dir = (rep & PAD_KEY_RIGHT) ? 1 : -1;
        switch (r->kind)
        {
        case RK_CHOICE:
        case RK_TOGGLE:
            ChangeChoice(r, dir);
            break;
        case RK_SLIDER:
            ChangeSlider(r, dir);
            break;
        default:
            return;
        }
        FixCursor(1);
        PlaySE(SEQ_SE_DP_SELECT);
        DrawList();
        DrawBottom();
    }
    else if (r != NULL && (newKeys & PAD_BUTTON_A))
    {
        switch (r->kind)
        {
        case RK_CHOICE:   // A toggles On/Off and steps other choices forward
        case RK_TOGGLE:
            ChangeChoice(r, 1);
            FixCursor(1);
            PlaySE(SEQ_SE_DP_SELECT);
            DrawList();
            DrawBottom();
            break;
        case RK_TEXT:
            PlaySE(SEQ_SE_DP_SELECT);
            StartTextEntry(r->fmt);
            DrawBottom();
            break;
        case RK_ACTION:
            if (r->fmt == ACT_BEGIN)
            {
                sConfirm = 1;
                sPendingAction = ACT_BEGIN;
                PlaySE(SEQ_SE_DP_SELECT);
                DrawBottom();
            }
            else
            {
                DoAction(r->fmt, FALSE);
            }
            break;
        }
    }
}

// ---------------------------------------------------------------------------
// App
// ---------------------------------------------------------------------------
enum { ST_SETUP, ST_FADE_IN, ST_INPUT, ST_FADE_OUT, ST_DONE };

static BOOL MenuInit(void *manager, int *state)
{
    (void)manager;
    (void)state;
    Heap_Create(3, HEAPID_RH_MENU, 0x14000);
    sBuf[SCR_TOP] = sys_AllocMemory(HEAPID_RH_MENU, SCREEN_BYTES);
    sBuf[SCR_BOTTOM] = sys_AllocMemory(HEAPID_RH_MENU, SCREEN_BYTES);
    sRandState = gSystem.vblankCounter * 2654435761u;
    return TRUE;
}

static BOOL MenuExec(void *manager, int *state)
{
    switch (*state)
    {
    case ST_SETUP:
        // opened from the Randomizer Settings key item (RH_OpenSettingsFromField): edit the run's settings;
        // at New Game: the defaults with a new seed
        sInGame = RH_SettingsOpenedInGame(OverlayManager_GetArgs(manager));
        if (sInGame)
        {
            sPending = *RH_Settings();
        }
        else
        {
            RH_SetDefaultSettings(S);
#if RH_TEST
            S->instantText = TRUE;            // test builds: fast text for the emulator tests
#endif
            RandomSeedText();
        }
        sExitTo = 0;
        sMenuOpen = sConfirm = sCodeMode = sEditing = FALSE;
        ReadPresets();
        sMessage = NULL;
        GoToSection(SEC_GENERAL);
        SetupScreens();
        DrawAll();
        Flush();
        BeginNormalPaletteFade(0, 1, 1, 0, 6, 1, HEAPID_RH_MENU);
        *state = ST_FADE_IN;
        break;
    case ST_FADE_IN:
        if (IsPaletteFadeFinished())
            *state = ST_INPUT;
        break;
    case ST_INPUT:
        HandleInput();
        Flush();
        if (sExitTo)
        {
            BeginNormalPaletteFade(0, 0, 0, 0, 6, 1, HEAPID_RH_MENU);
            *state = ST_FADE_OUT;
        }
        break;
    case ST_FADE_OUT:
        if (IsPaletteFadeFinished())
        {
            *state = ST_DONE;
            return TRUE;
        }
        break;
    case ST_DONE:
        return TRUE;
    }
    return FALSE;
}

static BOOL MenuExit(void *manager, int *state)
{
    (void)manager;
    (void)state;
    // Begin Run: the settings go into the new save, then Professor Oak's speech starts.
    // In-game: Apply & Return changes the run's settings (kept when the player saves); Exit changes nothing.
    S->magic = RH_SETTINGS_MAGIC;
    if (S->seed == 0)
        S->seed = 1;
    if (!sInGame || sExitTo == 1)
        *RH_Settings() = sPending;
    if (sInGame && sExitTo == 1)
        RH_GiveKeyItems();                    // e.g. Nuzlocke Mode switched on mid-run: its key items now
    sys_FreeMemoryEz(sBuf[SCR_TOP]);
    sys_FreeMemoryEz(sBuf[SCR_BOTTOM]);
    Heap_Destroy(HEAPID_RH_MENU);
    if (!sInGame)
        RH_RegisterMainOverlay(0xFFFFFFFF, APP_OAK_SPEECH);
    return TRUE;
}
