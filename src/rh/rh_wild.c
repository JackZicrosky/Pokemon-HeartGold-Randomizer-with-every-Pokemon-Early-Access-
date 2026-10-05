// Romhack randomizer core: wild Pokemon (port of the FireRed/Emerald rh_sources.c wild part).
// The encounter table of a map is randomized as it loads (RH_TransformEncounters, from the data loader hook).
#include "../../include/types.h"
#include "../../include/save.h"
#include "../../include/encounter.h"
#include "../../include/system.h"
#include "../../include/constants/file.h"
#include "../../include/constants/species.h"
#include "../../include/rh/rh_internal.h"

enum { ZONE_GLOBAL, ZONE_LOCATION, ZONE_SET, ZONE_MAX, ZONE_ALWAYS, ZONE_MAP };
enum { AREA_LAND, AREA_SURF, AREA_ROCKS, AREA_FISHING };

// Forms picked for the map that is loaded (encounter tables only hold species): applied when the wild
// Pokemon is created (AddWildPartyPokemon, src/field/enemy_party.c).
#define FORM_SLOTS 24
static struct { u16 species; u8 form; } sForms[FORM_SLOTS];
static u8 sFormCount;

u32 RH_WildForm(u32 species)
{
    u32 i;
    for (i = 0; i < sFormCount; i++)
        if (sForms[i].species == species)
            return sForms[i].form;
    return 0;
}

static u16 StoreResult(u32 sp)
{
    u16 species, form;
    RH_SpeciesKey(sp, &species, &form);
    if (form != 0 && sFormCount < FORM_SLOTS)
    {
        sForms[sFormCount].species = species;
        sForms[sFormCount].form = form;
        sFormCount++;
    }
    return species;
}

u32 RH_ModifyWildLevel(u32 level)
{
    s32 l;
    if (S->magic != RH_SETTINGS_MAGIC || !S->enabled || !S->wildLevelModOn || level == 0)
        return level;
    l = RH_ApplyPercent(level, S->wildLevelMod);
    return l < 1 ? 1 : l > 100 ? 100 : l;
}

static u32 CatchAllRankMode(void)
{
    BOOL split = S->wildSplitEncounterTypes;
    switch (S->wildZone)
    {
    case ZONE_GLOBAL:   return split ? 1 : 0;
    case ZONE_LOCATION: return split ? 3 : 2;
    case ZONE_MAP:      return split ? 5 : 4;
    default:            return 6;
    }
}

static BOOL UsesMapping(void)
{
    return S->wildZone != ZONE_MAX && S->wildZone != ZONE_ALWAYS;
}

// Zone of a wild pair: every (zone, species) gets one fixed replacement.
static u32 ZoneOf(const struct RhWildPair *p)
{
    BOOL split = S->wildSplitEncounterTypes;
    switch (S->wildZone)
    {
    case ZONE_GLOBAL:   return split ? p->area + 1 : 0;
    case ZONE_LOCATION: return 0x10000 + p->location + (split ? (p->area + 1) * 0x100 : 0);
    case ZONE_MAP:      return 0x20000 + p->bank * 8 + (split ? p->area + 1 : 0);
    default:            return 0x40000 + p->bank * 8 + p->area;   // encounter set (and "Maximum Possible")
    }
}

// A random type that at least 3 allowed Pokemon have (FVX: only types that exist).
static u32 RandomPopulatedType(u32 hash)
{
    u32 i, k;
    for (i = 0; i < RH_NUM_TYPES; i++)
    {
        struct RhFilter f;
        u32 type = RH_MonTypeAt(hash + i), n = 0, count = RH_InfoCount();
        RH_FilterInit(&f);
        f.type = type;
        for (k = 1; k < count && n < 3; k++)
            n += RH_FilterAccepts(&f, k);
        if (n >= 3)
            return type;
    }
    return RH_MonTypeAt(hash);
}

static u8 sRelationStage;

static BOOL ChainLongEnough(u32 sp)
{
    return RH_SpeciesChain(sp) >= sRelationStage;
}

static u32 FamilyRoot(u32 sp)
{
    u32 g;
    for (g = 0; g < 4 && RH_PreEvo(sp) != 0; g++)
        sp = RH_PreEvo(sp);
    return sp;
}

static u32 PickWild(const struct RhWildPair *p, u32 slot, u32 salt)
{
    struct RhFilter f;
    u32 species = p->species, mode = CatchAllRankMode();
    u32 zone = ZoneOf(p), themeZoneMode = mode, speciesKey = species, hash, bst = 0, result;
    BOOL relations;
    u32 keptTheme = RH_TYPE_NONE;

    if (S->wildZone == ZONE_MAX)
        speciesKey = 0x8000 + slot;
    else if (S->wildZone == ZONE_ALWAYS)
    {
        zone = RH_Hash(SALT_WILD_AREA, gSystem.vblankCounter, salt);   // a new Pokemon every time the map loads
        speciesKey = salt;
        themeZoneMode = 6;                                              // no zones: themes per encounter set (FVX)
    }

    RH_FilterInit(&f);
    f.allowVariants = S->wildAllowAltFormes;
    if (S->wildNoLegends)
        f.legend = 1;
    if (S->wildKeepThemes)                                   // "Keep Set/Zone Themes" wins over the other type rules
        keptTheme = p->theme[(S->wildTypeRestriction == 1 && S->wildZone != ZONE_ALWAYS) ? themeZoneMode : 6];
    if (keptTheme != RH_TYPE_NONE)
        f.type = keptTheme;
    else if (S->wildTypeRestriction == 1)
        f.type = RandomPopulatedType(RH_Hash(SALT_WILD_THEME, zone, 0));
    else if (S->wildTypeRestriction == 2)
        f.primaryType = RH_SpeciesType(species, 0);
    if (S->wildEvoRestriction == 1)
        f.stage = 1;
    else if (S->wildEvoRestriction == 2)
        f.stage = RH_SpeciesStage(species);
    // FVX: only Pokemon that have a relative in the same zone are kept related
    relations = S->wildKeepRelations && UsesMapping() && S->wildEvoRestriction != 2 && (p->relatives & (1 << mode));
    if (S->wildSimilarStrength)
    {
        bst = RH_SpeciesBST(species);
        if (S->wildBalanceLowLevel)                          // FVX: no Geodude at level 2
        {
            u32 low = RH_ModifyWildLevel(UsesMapping() ? p->zoneLow[mode] : p->low), cap = low * 10 + 250;
            bst = bst < cap ? bst : cap;
        }
    }
    hash = RH_Hash(SALT_WILD, zone, speciesKey);

    if (relations)
    {
        // Related Pokemon stay related: the family gets one random basic Pokemon, evolved to the original's stage.
        u32 stage = RH_SpeciesStage(species), root = FamilyRoot(species);
        struct RhFilter g = f;
        g.stage = 1;
        sRelationStage = stage;
        g.extra = ChainLongEnough;
        result = RH_PickSpeciesNearBst(&g, RH_Hash(SALT_WILD, zone, root), bst ? RH_SpeciesBST(root) : 0);
        if (result != 0)
            return RH_EvolveTimes(result, stage - 1);
    }
    if (S->wildCatchEmAll && S->wildZone != ZONE_ALWAYS)
    {
        // every Pokemon is used once before any repeats; "Maximum Possible" ranks each slot of a set separately
        u32 rank = (S->wildZone == ZONE_MAX) ? p->rank[6] * 13 + slot : p->rank[mode];
        result = RH_PickRanked(&f, rank, SALT_CATCH_ALL);
        if (result != 0)
            return result;
    }
    if (UsesMapping())
    {
        // 1-to-1: the Pokemon of one zone all get different replacements (FVX)
        result = RH_PickSpeciesNearBstRanked(&f, p->zoneRank[mode], RH_Hash(SALT_WILD, zone, 0x2A), bst);
        return result ? result : species;
    }
    result = RH_PickSpeciesNearBst(&f, hash, bst);
    return result ? result : species;
}

// ---------------------------------------------------------------------------
// Applying it to an encounter table
// ---------------------------------------------------------------------------
static struct RhWildPair *sPairs;
static u32 sPairCount;

static u32 ToPersonal(u32 v)
{
    return (v >> 11) ? RH_SpeciesIndex(v & 0x7FF, v >> 11) : v;
}

static const struct RhWildPair *FindPair(u32 area, u32 sp)
{
    u32 i;
    for (i = 0; i < sPairCount; i++)
        if (sPairs[i].area == area && sPairs[i].species == sp)
            return &sPairs[i];
    return NULL;
}

// cache of the picks made for this table: the same (area, species) always gets the same result here
#define PICK_CACHE 48
static struct { u16 in; u8 area; u16 out; } sPicks[PICK_CACHE];
static u8 sPickCount;

static u16 Randomize(u32 v, u32 area, u32 slot)
{
    u32 sp, i;
    const struct RhWildPair *p;
    if (v == 0)
        return 0;
    sp = ToPersonal(v);
    if (S->wildZone != ZONE_MAX)
        for (i = 0; i < sPickCount; i++)
            if (sPicks[i].in == sp && sPicks[i].area == area)
                return StoreResult(sPicks[i].out);
    p = FindPair(area, sp);
    if (p == NULL)
        return v;
    sp = PickWild(p, slot, area * 64 + slot);
    if (sPickCount < PICK_CACHE)
    {
        sPicks[sPickCount].in = p->species;
        sPicks[sPickCount].area = area;
        sPicks[sPickCount].out = sp;
        sPickCount++;
    }
    return StoreResult(sp);
}

static void ModifyLevel(u8 *lv)
{
    *lv = RH_ModifyWildLevel(*lv);
}

void RH_TransformEncounters(u32 bank, EncounterData *d)
{
    u32 i, heap;
    u16 start[2];
    if (S->magic != RH_SETTINGS_MAGIC || !S->enabled)
        return;
    sFormCount = 0;
    if (S->wildLevelModOn)
    {
        for (i = 0; i < NUM_ENCOUNTERS_LAND; i++)
            ModifyLevel(&d->landSlots.levels[i]);
        for (i = 0; i < NUM_ENCOUNTERS_SURF; i++)
            ModifyLevel(&d->surfSlots[i].levelMin), ModifyLevel(&d->surfSlots[i].levelMax);
        for (i = 0; i < NUM_ENCOUNTERS_ROCKSMASH; i++)
            ModifyLevel(&d->rockSmashSlots[i].levelMin), ModifyLevel(&d->rockSmashSlots[i].levelMax);
        for (i = 0; i < NUM_ENCOUNTERS_FISH; i++)
        {
            ModifyLevel(&d->oldRodSlots[i].levelMin), ModifyLevel(&d->oldRodSlots[i].levelMax);
            ModifyLevel(&d->goodRodSlots[i].levelMin), ModifyLevel(&d->goodRodSlots[i].levelMax);
            ModifyLevel(&d->superRodSlots[i].levelMin), ModifyLevel(&d->superRodSlots[i].levelMax);
        }
    }
    if (!S->wild || bank >= RH_WILD_BANKS)
        return;
    // this map's pairs
    ReadFromNarcMemberByIdPair(start, ARC_CODE_ADDONS, CODE_ADDON_RH_WILD, bank * 2, 4);
    sPairCount = start[1] - start[0];
    if (sPairCount == 0)
        return;
    heap = RH_SessionHeap();
    sPairs = sys_AllocMemory(heap, sPairCount * sizeof(struct RhWildPair));
    if (sPairs == NULL)
        return;
    ReadFromNarcMemberByIdPair(sPairs, ARC_CODE_ADDONS, CODE_ADDON_RH_WILD,
                               (RH_WILD_BANKS + 1) * 2 + start[0] * sizeof(struct RhWildPair), sPairCount * sizeof(struct RhWildPair));
    RH_BeginSession(heap);
    sPickCount = 0;
    for (i = 0; i < NUM_ENCOUNTERS_LAND; i++)
    {
        d->landSlots.speciesMorning[i] = Randomize(d->landSlots.speciesMorning[i], AREA_LAND, i);
        d->landSlots.speciesDay[i] = Randomize(d->landSlots.speciesDay[i], AREA_LAND, i);
        d->landSlots.speciesNight[i] = Randomize(d->landSlots.speciesNight[i], AREA_LAND, i);
    }
    for (i = 0; i < NUM_ENCOUNTERS_SOUNDS; i++)
    {
        d->hoennSoundSpecies[i] = Randomize(d->hoennSoundSpecies[i], AREA_LAND, 12 + i);
        d->sinnohSoundSpecies[i] = Randomize(d->sinnohSoundSpecies[i], AREA_LAND, 14 + i);
    }
    for (i = 0; i < NUM_ENCOUNTERS_SURF; i++)
        d->surfSlots[i].species = Randomize(d->surfSlots[i].species, AREA_SURF, i);
    for (i = 0; i < NUM_ENCOUNTERS_ROCKSMASH; i++)
        d->rockSmashSlots[i].species = Randomize(d->rockSmashSlots[i].species, AREA_ROCKS, i);
    for (i = 0; i < NUM_ENCOUNTERS_FISH; i++)
    {
        d->oldRodSlots[i].species = Randomize(d->oldRodSlots[i].species, AREA_FISHING, i);
        d->goodRodSlots[i].species = Randomize(d->goodRodSlots[i].species, AREA_FISHING, 5 + i);
        d->superRodSlots[i].species = Randomize(d->superRodSlots[i].species, AREA_FISHING, 10 + i);
    }
    d->landSwarm = Randomize(d->landSwarm, AREA_LAND, 16);
    d->surfSwarm = Randomize(d->surfSwarm, AREA_SURF, 5);
    d->nightFish = Randomize(d->nightFish, AREA_FISHING, 15);
    d->fishSwarm = Randomize(d->fishSwarm, AREA_FISHING, 16);
    RH_EndSession();
    sys_FreeMemoryEz(sPairs);
    sPairs = NULL;
}
