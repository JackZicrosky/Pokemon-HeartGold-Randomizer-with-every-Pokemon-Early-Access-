// Romhack randomizer core: starters (port of the FireRed/Emerald rh_sources.c starter part).
#include "../../include/types.h"
#include "../../include/save.h"
#include "../../include/constants/species.h"
#include "../../include/constants/file.h"
#include "../../include/rh/rh_internal.h"

// ---------------------------------------------------------------------------
// Type matchups (Gen 6+ chart) in the sMonTypes order of rh_traits.c:
// Normal Fighting Flying Poison Ground Rock Bug Ghost Steel Fire Water Grass Electric Psychic Ice Dragon Dark Fairy
// 0 = no effect, 1 = not very effective, 2 = normal, 3 = super effective
// ---------------------------------------------------------------------------
static const u8 sChart[RH_NUM_TYPES][RH_NUM_TYPES] = {
    /* Normal   */ { 2, 2, 2, 2, 2, 1, 2, 0, 1, 2, 2, 2, 2, 2, 2, 2, 2, 2 },
    /* Fighting */ { 3, 2, 1, 1, 2, 3, 1, 0, 3, 2, 2, 2, 2, 1, 3, 2, 3, 1 },
    /* Flying   */ { 2, 3, 2, 2, 2, 1, 3, 2, 1, 2, 2, 3, 1, 2, 2, 2, 2, 2 },
    /* Poison   */ { 2, 2, 2, 1, 1, 1, 2, 1, 0, 2, 2, 3, 2, 2, 2, 2, 2, 3 },
    /* Ground   */ { 2, 2, 0, 3, 2, 3, 1, 2, 3, 3, 2, 1, 3, 2, 2, 2, 2, 2 },
    /* Rock     */ { 2, 1, 3, 2, 1, 2, 3, 2, 1, 3, 2, 2, 2, 2, 3, 2, 2, 2 },
    /* Bug      */ { 2, 1, 1, 1, 2, 2, 2, 1, 1, 1, 2, 3, 2, 3, 2, 2, 3, 1 },
    /* Ghost    */ { 0, 2, 2, 2, 2, 2, 2, 3, 2, 2, 2, 2, 2, 3, 2, 2, 1, 2 },
    /* Steel    */ { 2, 2, 2, 2, 2, 3, 2, 2, 1, 1, 1, 2, 1, 2, 3, 2, 2, 3 },
    /* Fire     */ { 2, 2, 2, 2, 2, 1, 3, 2, 3, 1, 1, 3, 2, 2, 3, 1, 2, 2 },
    /* Water    */ { 2, 2, 2, 2, 3, 3, 2, 2, 2, 3, 1, 1, 2, 2, 2, 1, 2, 2 },
    /* Grass    */ { 2, 2, 1, 1, 3, 3, 1, 2, 1, 1, 3, 1, 2, 2, 2, 1, 2, 2 },
    /* Electric */ { 2, 2, 3, 2, 0, 2, 2, 2, 2, 2, 3, 1, 1, 2, 2, 1, 2, 2 },
    /* Psychic  */ { 2, 3, 2, 3, 2, 2, 2, 2, 1, 2, 2, 2, 2, 1, 2, 2, 0, 2 },
    /* Ice      */ { 2, 2, 3, 2, 3, 2, 2, 2, 1, 1, 1, 3, 2, 2, 1, 3, 2, 2 },
    /* Dragon   */ { 2, 2, 2, 2, 2, 2, 2, 2, 1, 2, 2, 2, 2, 2, 2, 3, 2, 0 },
    /* Dark     */ { 2, 1, 2, 2, 2, 2, 2, 3, 2, 2, 2, 2, 2, 3, 2, 2, 1, 1 },
    /* Fairy    */ { 2, 3, 2, 1, 2, 2, 2, 2, 1, 1, 2, 2, 2, 2, 2, 3, 3, 2 },
};

// attacking type a beats defending type b (by type id)
BOOL RH_TypeBeats(u32 a, u32 b)
{
    return sChart[RH_TypeIndexOf(a)][RH_TypeIndexOf(b)] == 3;
}

// 0 = no effect, 1 = not very effective, 2 = normal, 3 = super effective
u32 RH_TypeEffect(u32 a, u32 b)
{
    return sChart[RH_TypeIndexOf(a)][RH_TypeIndexOf(b)];
}

// ---------------------------------------------------------------------------
// Starters
// ---------------------------------------------------------------------------
// Elm's three balls: index 0 = Chikorita, 1 = Cyndaquil, 2 = Totodile (the "Starter 1-3" order of the menu).
static const u16 sVanillaStarters[3] = { SPECIES_CHIKORITA, SPECIES_CYNDAQUIL, SPECIES_TOTODILE };

static u32 sStarterKey;
static u16 sStarters[3];
static u8 sSlotTypes[3];
static u8 sSlot;
static u16 sChosen[3];

static u32 StarterKey(void)
{
    const u8 *b = (const u8 *)S;
    u32 i, h = 2166136261u;
    for (i = 0; i < sizeof(struct RhSettings); i++)
        h = (h ^ b[i]) * 16777619u;
    return (h + 4) | 1;
}

// Picks 3 slot types for "Any Type Triangle": slot0 beats slot2, slot2 beats slot1, slot1 beats slot0,
// mirroring Grass > Water > Fire > Grass.
static BOOL PickTriangle(u8 *types)
{
    u32 count = 0, target = 0, pass, a, b, c;
    for (pass = 0; pass < 2; pass++)
    {
        if (pass == 1)
        {
            if (count == 0)
                return FALSE;
            target = RH_Hash(SALT_STARTER_TYPE, 0, 0) % count;
            count = 0;
        }
        for (a = 0; a < RH_NUM_TYPES; a++)
            for (b = 0; b < RH_NUM_TYPES; b++)
                for (c = 0; c < RH_NUM_TYPES; c++)
                {
                    if (a == b || b == c || a == c)
                        continue;
                    if (sChart[a][c] != 3 || sChart[c][b] != 3 || sChart[b][a] != 3)
                        continue;
                    if (pass == 1 && count == target)
                    {
                        types[0] = RH_MonTypeAt(a);
                        types[1] = RH_MonTypeAt(b);
                        types[2] = RH_MonTypeAt(c);
                        return TRUE;
                    }
                    count++;
                }
    }
    return FALSE;
}

static BOOL StarterExtraOk(u32 sp)
{
    u32 k;
    // Fire/Water/Grass and Type Triangle: the other type may not be another slot's type (FVX)
    if (S->starterTypes == 1 || S->starterTypes == 2)
        for (k = 0; k < 3; k++)
            if (k != sSlot && RH_SpeciesHasType(sp, sSlotTypes[k]))
                return FALSE;
    // Unique: no type shared with the starters already chosen
    if (S->starterTypes == 3)
    {
        for (k = 0; k < 3; k++)
        {
            u32 o = (k == sSlot) ? 0 : sChosen[k];
            if (o != 0 && (RH_SpeciesHasType(sp, RH_SpeciesType(o, 0)) || RH_SpeciesHasType(sp, RH_SpeciesType(o, 1))))
                return FALSE;
        }
    }
    return TRUE;
}

static u32 CountTypeCandidates(u32 type, const struct RhFilter *base)
{
    struct RhFilter f = *base;
    u32 i, n = 0, count = RH_InfoCount();
    f.type = type;
    for (i = 1; i < count && n < 3; i++)
        n += RH_FilterAccepts(&f, i);
    return n;
}

static void BuildStarters(void)
{
    u32 i;
    struct RhFilter base;
    u8 slotTypes[3] = { RH_TYPE_NONE, RH_TYPE_NONE, RH_TYPE_NONE };

    RH_BeginSession(3);
    RH_FilterInit(&base);
    if (S->starters == 3)
    {
        base.threeStageBasic = (S->evolutions == 0);
        base.stage = S->evolutions ? 1 : 0;
    }
    if (S->starters == 4)
        base.stage = 1;                                      // Random (basic): not evolved from anything
    base.allowVariants = S->starterAllowAltFormes;
    if (S->starterNoLegends)
        base.legend = 1;
    if (S->starterBstMinOn)
        base.minBst = S->starterBstMin;
    if (S->starterBstMaxOn)
        base.maxBst = S->starterBstMax;
    base.monoType = S->starterNoDualTypes;
    memset(sChosen, 0, sizeof(sChosen));

    switch ((S->starters == 1 && (S->starterTypes == 1 || S->starterTypes == 2)) ? 0 : S->starterTypes)
    {
    case 1:
        slotTypes[0] = 12;   // Grass
        slotTypes[1] = 10;   // Fire
        slotTypes[2] = 11;   // Water
        break;
    case 2:
        PickTriangle(slotTypes);
        break;
    case 4:
        slotTypes[0] = S->starterSingleType;
        if (slotTypes[0] >= RH_TYPE_NONE || slotTypes[0] == 18)
        {
            // random single type: only types with at least 3 possible starters
            u32 k;
            slotTypes[0] = RH_TYPE_NONE;
            for (k = 0; k < RH_NUM_TYPES; k++)
            {
                u32 type = RH_MonTypeAt(RH_Hash(SALT_STARTER_TYPE, 1, 0) + k);
                if (CountTypeCandidates(type, &base) >= 3)
                {
                    slotTypes[0] = type;
                    break;
                }
            }
        }
        slotTypes[1] = slotTypes[2] = slotTypes[0];
        break;
    }
    memcpy(sSlotTypes, slotTypes, 3);

    // Custom: typed names; a blank slot keeps the game's own starter for that ball (owner's rule).
    if (S->starters == 1)
        for (i = 0; i < 3; i++)
            sStarters[i] = sChosen[i] = S->customStarters[i] ? S->customStarters[i] : sVanillaStarters[i];
    for (i = 0; i < 3 && S->starters != 1; i++)
    {
        struct RhFilter f = base;
        u32 j;
        for (j = 0; j < 3; j++)
            if (j != i && sChosen[j] != 0)
                RH_FilterExclude(&f, sChosen[j]);
        f.type = slotTypes[i];
        sSlot = i;
        f.extra = StarterExtraOk;
        sStarters[i] = RH_PickSpeciesNearBst(&f, RH_Hash(SALT_STARTER, i, 0), 0);
        if (sStarters[i] == 0)
            sStarters[i] = sVanillaStarters[i];
        sChosen[i] = sStarters[i];
    }
    RH_EndSession();
    sStarterKey = StarterKey();
}

// The Pokemon in Elm's ball "slot" (personal index: species or one of its forms).
u32 RH_StarterForSlot(u32 slot)
{
    if (slot > 2)
        return sVanillaStarters[0];
    if (S->magic != RH_SETTINGS_MAGIC || !S->enabled || S->starters == 0)
        return sVanillaStarters[slot];
    if (sStarterKey != StarterKey())
        BuildStarters();
    return sStarters[slot];
}

// ---------------------------------------------------------------------------
// Static Pokemon (FVX): every static encounter / gift / egg / prize / roamer of UPR FVX's HeartGold list (a028 member
// 26, tools/rh/gen_rh_statics.py) gets its own replacement, never one used twice.
// ---------------------------------------------------------------------------
#define STATIC_MAX 64
static u32 sStaticKey;
static u8 sStaticCount, sStaticsBuilt;
static struct RhStatic sStaticTable[STATIC_MAX];
static u16 sStaticResult[STATIC_MAX];

static BOOL StaticNotUsedYet(u32 sp)
{
    u32 i;
    for (i = 0; i < sStaticsBuilt; i++)
        if (sStaticResult[i] == sp)
            return FALSE;
    return TRUE;
}

static void BuildStatics(void)
{
    u16 hdr[2];
    u32 i;
    ReadFromNarcMemberByIdPair(hdr, ARC_CODE_ADDONS, CODE_ADDON_RH_STATICS, 0, 4);
    sStaticCount = hdr[0] < STATIC_MAX ? hdr[0] : STATIC_MAX;
    ReadFromNarcMemberByIdPair(sStaticTable, ARC_CODE_ADDONS, CODE_ADDON_RH_STATICS, 4, sStaticCount * sizeof(struct RhStatic));
    RH_BeginSession(0xFFFF);
    for (i = 0; i < sStaticCount; i++)
    {
        u32 sp = sStaticTable[i].species, bst = 0, result;
        struct RhFilter f;
        sStaticsBuilt = i;
        RH_FilterInit(&f);
        f.extra = StaticNotUsedYet;
        // roamers can't have forms; the others follow "Allow Alternate Formes"
        f.allowVariants = sStaticTable[i].map != 0xFFFF && S->staticAllowAltFormes;
        RH_FilterExclude(&f, sp);                            // always a different Pokemon (FVX banSamePokemon)
        switch (S->statics)
        {
        case 1:                                              // Swap Legendaries & Swap Standards
            f.legend = RH_IsLegendary(sp) ? 2 : 1;
            break;
        case 3:                                              // Similar Strength
            if (!(S->staticRandomize600 && RH_SpeciesBST(sp) >= 600))
                bst = RH_SpeciesBST(sp);                     // "Randomize 600+ BST": those get a purely random pick
            break;
        }
        result = RH_PickSpeciesNearBst(&f, RH_Hash(SALT_STATIC_SLOT, i, 0), bst);
        if (result != 0 && sStaticTable[i].map == 0xFFFF && RH_SpeciesIndex(result, 0) != result)
            result = 0;
        sStaticResult[i] = result ? result : sp;
    }
    RH_EndSession();
    sStaticsBuilt = sStaticCount;
    sStaticKey = RH_SettingsHash() + 5;
}

// species (form bits allowed) on map (0xFFFF = a roamer) -> its replacement, species | form << 11
u32 RH_StaticSpecies(u32 species, u32 map)
{
    u32 i, base = species & 0x7FF;
    u16 sp, form;
    if (!RH_Active() || S->statics == 0)
        return species;
    if (sStaticKey != RH_SettingsHash() + 5)
        BuildStatics();
    for (i = 0; i < sStaticCount; i++)
        if (sStaticTable[i].map == map && sStaticTable[i].species == base)
        {
            RH_SpeciesKey(sStaticResult[i], &sp, &form);
            return sp | (form << 11);
        }
    return species;
}

// Static Level Modifier (FVX: works with the statics unchanged too)
u32 RH_StaticLevel(u32 level)
{
    s32 l = level;
    if (!RH_Active() || !S->staticLevelModOn)
        return level;
    l = RH_ApplyPercent(l, S->staticLevelMod);
    return l < 1 ? 1 : (l > 100 ? 100 : l);
}

// ---------------------------------------------------------------------------
// In-game trades (FVX): every trade gives (and, with "Requested & Given", asks for) a different random Pokemon,
// never one used twice. The loans (Kenya, Kirk's Shuckle...: given = requested) stay. Trade data: a/1/1/2.
// ---------------------------------------------------------------------------
#define NARC_NPC_TRADE 112
#define TRADE_COUNT 13
#define TRADE_WORDS 21                                       // struct NPCTrade, s32 fields
enum { TR_GIVE = 0, TR_IV = 1, TR_OTID = 8, TR_ITEM = 15, TR_SHEEN = 17, TR_ASK = 19 };
#define TRADE_DONE 0x40000000                                // in the (unused) sheen field: already randomized

static u32 sTradeKey;
static u8 sTradesBuilt;
static u16 sTradeGive[TRADE_COUNT], sTradeAsk[TRADE_COUNT];

static BOOL TradeUnused(u32 sp)
{
    u32 i;
    for (i = 0; i < sTradesBuilt; i++)
        if (sTradeGive[i] == sp || sTradeAsk[i] == sp)
            return FALSE;
    return TRUE;
}

static void BuildTrades(void)
{
    u32 i;
    RH_BeginSession(0xFFFF);
    for (i = 0; i < TRADE_COUNT; i++)
    {
        s32 t[TRADE_WORDS];
        struct RhFilter f;
        u32 r;
        ReadFromNarcMemberByIdPair(t, NARC_NPC_TRADE, i, 0, sizeof(t));
        sTradesBuilt = i;
        sTradeGive[i] = t[TR_GIVE];
        sTradeAsk[i] = t[TR_ASK];
        if (t[TR_GIVE] == t[TR_ASK])
            continue;                                        // a loan
        RH_FilterInit(&f);
        f.extra = TradeUnused;
        if (S->trades >= 2 && (r = RH_PickWithFilter(&f, RH_Hash(SALT_TRADE, i, 1))) != 0)
            sTradeAsk[i] = r;
        RH_FilterExclude(&f, sTradeAsk[i]);
        if ((r = RH_PickWithFilter(&f, RH_Hash(SALT_TRADE, i, 0))) != 0)
            sTradeGive[i] = r;
    }
    RH_EndSession();
    sTradesBuilt = TRADE_COUNT;
    sTradeKey = RH_SettingsHash() + 6;
}

static u32 RandomHeldItem(u32 hash)
{
    u16 hdr[2];
    u32 tries;
    ReadFromNarcMemberByIdPair(hdr, ARC_CODE_ADDONS, CODE_ADDON_RH_ITEMS, 0, 4);
    for (tries = 0; tries < 64 && hdr[0] != 0; tries++)
    {
        struct RhItem it;
        ReadFromNarcMemberByIdPair(&it, ARC_CODE_ADDONS, CODE_ADDON_RH_ITEMS, 4 + (RH_Hash(hash, tries, 0x17) % hdr[0]) * sizeof(it), sizeof(it));
        if ((it.flags & RHI_HELD) && !(it.flags & RHI_BAD))
            return it.item;
    }
    return 0;
}

// The trade's data (struct NPCTrade, as s32 words) as the trade NPC uses it.
void RH_TransformTrade(u32 tradeno, s32 *t)
{
    u32 i;
    if (!RH_Active() || S->trades == 0 || tradeno >= TRADE_COUNT || (t[TR_SHEEN] & TRADE_DONE) || t[TR_GIVE] == t[TR_ASK])
        return;
    if (sTradeKey != RH_SettingsHash() + 6)
        BuildTrades();
    t[TR_SHEEN] |= TRADE_DONE;
    t[TR_GIVE] = sTradeGive[tradeno];
    t[TR_ASK] = sTradeAsk[tradeno];
    if (S->tradeIVs)
        for (i = 0; i < 6; i++)
            t[TR_IV + i] = RH_Hash(SALT_IV, tradeno, i) % 32;
    if (S->tradeOTs)
        t[TR_OTID] = RH_Hash(SALT_OT, tradeno, 1) & 0xFFFF;  // FVX: a random 16-bit trainer ID
    if (S->tradeItems && (i = RandomHeldItem(RH_Hash(SALT_HELD_ITEM, 0x7700 + tradeno, 0))) != 0)
        t[TR_ITEM] = i;
}
