// Romhack: Foe Pokemon (port of the FireRed/Emerald rh_trainers.c): trainer teams, type themes, the League's
// unique Pokemon, Silver's starter and roster, evolving by level, better movesets, held items, additional Pokemon.
// Trainer tags (tiers, gym / Elite Four groups) follow UPR FVX's tagTrainersHGSS: include/rh/rh_trainer_table.h.
#include "../../include/types.h"
#include "../../include/battle.h"
#include "../../include/encounter.h"
#include "../../include/pokemon.h"
#include "../../include/save.h"
#include "../../include/script.h"
#include "../../include/trainer_data.h"
#include "../../include/constants/ability.h"
#include "../../include/constants/battle_constants.h"
#include "../../include/constants/file.h"
#include "../../include/constants/item.h"
#include "../../include/constants/moves.h"
#include "../../include/constants/species.h"
#include "../../include/constants/generated/learnsets.h"
#include "../../include/rh/rh_internal.h"
#include "../../include/rh/rh_trainers.h"
#include "../../include/rh/rh_trainer_table.h"

u16 LONG_CALL ItemToMachineMove(u16 itemId);
u16 LONG_CALL ItemToMachineMoveIndex(u16 itemId);
void LONG_CALL LoadLevelUpLearnset_HandleAlternateForm(int species, int form, u32 *levelUpLearnset);

enum { TIER_BOSS, TIER_IMPORTANT, TIER_REGULAR };

#define MAX_PARTY 6

// ---------------------------------------------------------------------------
// Trainer tags
// ---------------------------------------------------------------------------
static u32 Tag(u32 id)
{
    return id < RH_NUM_TRAINERS ? sRhTrainerTags[id] : TIER_REGULAR;
}

static u32 TierOf(u32 id)              { return Tag(id) & 3; }
static BOOL IsRival(u32 id)            { return (Tag(id) & 4) != 0; }      // Silver
static BOOL IsFirstRivalBattle(u32 id) { return (Tag(id) & 8) != 0; }      // Cherrygrove: only his starter
static u32 GroupOf(u32 id)             { return Tag(id) >> 8; }

// The Pokemon League (League Unique Pokemon): 0-3 = Will, Koga, Bruno, Karen, 4 = Lance; rematches included.
static s32 LeagueMemberOf(u32 id)
{
    u32 g = GroupOf(id);
    if (g >= RH_GROUP_ELITE1 && g < RH_GROUP_ELITE1 + 4)
        return g - RH_GROUP_ELITE1;
    if (g == RH_GROUP_CHAMPION)
        return 4;
    return -1;
}

// ---------------------------------------------------------------------------
// The team being built
// ---------------------------------------------------------------------------
struct OrigMon { u16 sp; u16 level; u8 ivs; u8 abilitySlot; };

static struct OrigMon sOrig[MAX_PARTY];   // the trainer data's team (slots >= sOrigCount: Additional Pokemon)
static u8 sOrigCount, sTotal;
static u16 sTrainerId;
static u8 sActive;
static u16 sTeam[MAX_PARTY];               // species chosen so far
static u8 sTeamCount;
static u8 sDiverse, sAvoidDupes, sNoWonderGuard;
static u8 sLeagueMember;                   // league member + 1 while building its team
static u8 sEvolveKeepType = RH_TYPE_NONE;  // type theme an evolution must keep
static u8 sForceEvolve;                    // evolve as far as the level allows (Silver's roster)
static u8 sTypeCountReady;
static u16 sTypeCount[RH_NUM_TYPES];       // Pokemon of each type in the pool (by type index)

static u32 Rd16(const u8 *b) { return b[0] | (b[1] << 8); }

// Walks the party file like MakeTrainerPokemonParty does.
static u32 ParseParty(const u8 *buf, u32 dt, u32 count, struct OrigMon *out)
{
    u32 off = 0, i, n = 0;
    for (i = 0; i < count && i < MAX_PARTY; i++)
    {
        u32 species, extra;
        out[n].ivs = buf[off];
        out[n].abilitySlot = buf[off + 1];
        out[n].level = Rd16(buf + off + 2);
        species = Rd16(buf + off + 4);
        out[n].sp = RH_SpeciesIndex(species & 0x7FF, species >> 11);
        n++;
        off += 6;
        if (dt & TRAINER_DATA_TYPE_ITEMS)      off += 2;
        if (dt & TRAINER_DATA_TYPE_MOVES)      off += 8;
        if (dt & TRAINER_DATA_TYPE_ABILITY)    off += 2;
        if (dt & TRAINER_DATA_TYPE_BALL)       off += 2;
        if (dt & TRAINER_DATA_TYPE_IV_EV_SET)  off += 12;
        if (dt & TRAINER_DATA_TYPE_NATURE_SET) off += 1;
        if (dt & TRAINER_DATA_TYPE_SHINY_LOCK) off += 1;
        if (dt & TRAINER_DATA_TYPE_ADDITIONAL_FLAGS)
        {
            u32 bit;
            extra = buf[off] | (buf[off + 1] << 8) | (buf[off + 2] << 16) | (buf[off + 3] << 24);
            off += 4;
            if (extra & TRAINER_DATA_EXTRA_TYPE_STATUS)
                off += 4;
            for (bit = TRAINER_DATA_EXTRA_TYPE_HP; bit <= TRAINER_DATA_EXTRA_TYPE_SP_DEF; bit <<= 1)
                if (extra & bit)
                    off += 2;
            if (extra & TRAINER_DATA_EXTRA_TYPE_PP_COUNTS)
                off += 4;
            if (extra & TRAINER_DATA_EXTRA_TYPE_NICKNAME)
                off += 22;
        }
        off += 2;                                                  // ball seal
    }
    return n;
}

// Another trainer's vanilla team (the League members' teams for League Unique Pokemon).
static u32 LoadTeam(u32 trainerId, struct OrigMon *out)
{
    u8 hdr[4];
    u8 *buf;
    u32 n = 0;
    ReadFromNarcMemberByIdPair(hdr, ARC_TRAINER_DATA, trainerId, 0, sizeof(hdr));   // data_type, class, pic, count
    buf = sys_AllocMemory(RH_SessionHeap(), sizeof(struct FULL_TRAINER_MON_DATA_STRUCTURE) * MAX_PARTY);
    if (buf != NULL)
    {
        TT_TrainerPokeDataGet(trainerId, buf);
        n = ParseParty(buf, hdr[0], hdr[3] & 0x7F, out);
        sys_FreeMemoryEz(buf);
    }
    return n;
}

static u32 VanillaType(u32 sp, u32 slot)
{
    struct RhSpeciesInfo i;
    RH_GetInfo(sp, &i);
    return slot ? i.type2 : i.type1;
}

static u32 FamilyRoot(u32 sp)
{
    u32 g;
    for (g = 0; g < 4 && RH_PreEvo(sp) != 0; g++)
        sp = RH_PreEvo(sp);
    return sp;
}

// ---------------------------------------------------------------------------
// Rank of a party slot by level (0 = highest; ties: the Pokemon further back ranks higher, FVX). Silver's starter
// doesn't count (FVX: the Champion's starter isn't one of his unique Pokemon).
// ---------------------------------------------------------------------------
static s32 StarterFamily(u32 sp, u32 *stage)
{
    u16 species, form;
    RH_SpeciesKey(sp, &species, &form);
    if (species < SPECIES_CHIKORITA || species > SPECIES_FERALIGATR)
        return -1;
    *stage = (species - SPECIES_CHIKORITA) % 3;
    return (species - SPECIES_CHIKORITA) / 3;
}

static BOOL IsStarterSlot(const struct OrigMon *t, u32 slot)
{
    u32 stage;
    return IsRival(sTrainerId) && StarterFamily(t[slot].sp, &stage) >= 0;
}

static u32 LevelRank(const struct OrigMon *t, u32 count, u32 slot, BOOL rival)
{
    u32 r = 0, i;
    if (rival && IsStarterSlot(t, slot))
        return MAX_PARTY;
    for (i = 0; i < count; i++)
        if (!(rival && IsStarterSlot(t, i))
         && (t[i].level > t[slot].level || (t[i].level == t[slot].level && i > slot)))
            r++;
    return r;
}

// "Highest Level Only": the highest-level Pokemon (the first one on ties, FVX), Silver's starter included.
static BOOL IsHighestLevelSlot(u32 slot)
{
    u32 i;
    if (slot >= sOrigCount)
        return FALSE;
    for (i = 0; i < sOrigCount; i++)
        if (sOrig[i].level > sOrig[slot].level || (sOrig[i].level == sOrig[slot].level && i < slot))
            return FALSE;
    return TRUE;
}

// ---------------------------------------------------------------------------
// Themes
// ---------------------------------------------------------------------------
static void CountTypes(void)
{
    struct RhFilter f;
    u32 i, n = RH_InfoCount();
    if (sTypeCountReady)
        return;
    memset(sTypeCount, 0, sizeof(sTypeCount));
    RH_FilterInit(&f);
    for (i = 1; i < n; i++)
    {
        u32 t1, t2;
        if (!RH_FilterAccepts(&f, i))
            continue;
        t1 = RH_SpeciesType(i, 0);
        t2 = RH_SpeciesType(i, 1);
        sTypeCount[RH_TypeIndexOf(t1)]++;
        if (t2 != t1)
            sTypeCount[RH_TypeIndexOf(t2)]++;
    }
    sTypeCountReady = TRUE;
}

static u32 TypeCount(u32 typeIndex)
{
    CountTypes();
    return sTypeCount[typeIndex % RH_NUM_TYPES];
}

// A type shared by every Pokemon of a vanilla team (FVX: a single Pokemon counts as themed; a shared Normal type
// loses to another shared type, so Normal/Flying bird teams become Flying).
static u32 TeamTheme(const struct OrigMon *t, u32 count)
{
    u32 s, i;
    u32 found = RH_TYPE_NONE;
    if (count < 1)
        return RH_TYPE_NONE;
    for (s = 0; s < 2; s++)
    {
        u32 type = VanillaType(t[0].sp, s);
        for (i = 1; i < count; i++)
            if (VanillaType(t[i].sp, 0) != type && VanillaType(t[i].sp, 1) != type)
                break;
        if (i == count && (found == RH_TYPE_NONE || found == TYPE_NORMAL))
            found = type;
    }
    return found;
}

// A random theme type. "Weight Types by # of Pokemon": in proportion to how many Pokemon have each type.
// Types with (almost) no Pokemon in the pool are never picked (FVX).
static u32 RandomTheme(u32 hash)
{
    u32 total = 0, i, r;
    for (i = 0; i < RH_NUM_TYPES; i++)
        if (TypeCount(i) >= 3)
            total += S->trainerWeightTypes ? TypeCount(i) : 1;
    if (total == 0)
        return RH_RandomMonType(hash);
    r = hash % total;
    for (i = 0; i < RH_NUM_TYPES; i++)
    {
        u32 w;
        if (TypeCount(i) < 3)
            continue;
        w = S->trainerWeightTypes ? TypeCount(i) : 1;
        if (r < w)
            return RH_MonTypeAt(i);
        r -= w;
    }
    return RH_MonTypeAt(0);
}

// Gyms and Elite Four members get different random types (a keyed permutation of the groups). Types with (almost)
// no Pokemon in the pool are skipped by cycle walking, which keeps the groups' types different.
static u32 GroupTheme(u32 group)
{
    u32 x = RH_Permute(SALT_TRAINER_THEME, (group - 1) % RH_NUM_TYPES, RH_NUM_TYPES), tries;
    for (tries = 0; tries < RH_NUM_TYPES && TypeCount(x) < 3; tries++)
        x = RH_Permute(SALT_TRAINER_THEME, x, RH_NUM_TYPES);
    return RH_MonTypeAt(x);
}

// The type theme of a trainer (RH_TYPE_NONE = none).
static u32 ThemeOf(u32 trainerId, const struct OrigMon *t, u32 count)
{
    u32 g = GroupOf(trainerId);
    u32 official = g ? sRhGroupType[g] : RH_TYPE_NONE;
    switch (S->trainers)
    {
    case 3:     // type themed: every trainer gets a theme; gyms / Elite Four / themed teams share theirs
        if (g >= 1 && g < RH_GROUP_CHAMPION)
            return GroupTheme(g);
        if (g == RH_GROUP_CHAMPION)                          // all of Lance's battles share one theme
            return RandomTheme(RH_Hash(SALT_TRAINER_THEME, sRhLeagueMembers[4], 0));
        if (g != 0)
            return RandomTheme(RH_Hash(SALT_TRAINER_THEME, 0x1000 + g, 0));
        return RandomTheme(RH_Hash(SALT_TRAINER_THEME, trainerId, 0));
    case 4:     // themes for gyms and the Elite Four only
        return (g >= 1 && g < RH_GROUP_CHAMPION) ? GroupTheme(g) : RH_TYPE_NONE;
    case 5:     // keep type-themed trainers' themes (gyms / Elite Four: their official type)
    case 6:
        return official != RH_TYPE_NONE ? official : TeamTheme(t, count);
    }
    return RH_TYPE_NONE;
}

// ---------------------------------------------------------------------------
// League Unique Pokemon: the Elite Four's / Champion's highest-level Pokemon are used by no other trainer.
// ---------------------------------------------------------------------------
#define MAX_RESERVED 10
static u32 sReservedKey;
static u8 sReservedCount;
static u16 sReserved[MAX_RESERVED];

static void EnsureReserved(void);

static BOOL IsLeagueReserved(u32 sp)
{
    u32 i;
    if (!S->enabled || !S->leagueUnique || S->trainers == 0)
        return FALSE;
    EnsureReserved();
    for (i = 0; i < sReservedCount; i++)
        if (sReserved[i] == sp)
            return TRUE;
    return FALSE;
}

// Is "sp" of the same family as one of the unique Pokemon of the league member being built?
static BOOL IsOwnReservedFamily(u32 sp)
{
    u32 r, member = sLeagueMember - 1;
    EnsureReserved();
    for (r = 0; r < S->leagueUnique; r++)
    {
        u32 idx = member * S->leagueUnique + r;
        if (idx < sReservedCount && FamilyRoot(sReserved[idx]) == FamilyRoot(sp))
            return TRUE;
    }
    return FALSE;
}

static u32 MemberTheme(u32 member, const struct OrigMon *t, u32 count)
{
    static const u8 sOfficial[5] = { TYPE_PSYCHIC, TYPE_POISON, TYPE_FIGHTING, TYPE_DARK, RH_TYPE_NONE };
    switch (S->trainers)
    {
    case 3:
        if (sOfficial[member] == RH_TYPE_NONE)               // Lance: his own random theme (see ThemeOf)
            return RandomTheme(RH_Hash(SALT_TRAINER_THEME, sRhLeagueMembers[member], 0));
        return GroupTheme(RH_GROUP_ELITE1 + member);
    case 4:
        return sOfficial[member] != RH_TYPE_NONE ? GroupTheme(RH_GROUP_ELITE1 + member) : RH_TYPE_NONE;
    case 5: case 6:
        return sOfficial[member] != RH_TYPE_NONE ? sOfficial[member] : TeamTheme(t, count);
    }
    return RH_TYPE_NONE;
}

// FVX: Silver's starters are never league-unique.
static BOOL NotStarterFamily(u32 sp)
{
    u32 i;
    for (i = 0; i < 3; i++)
        if (FamilyRoot(RH_StarterForSlot(i)) == FamilyRoot(sp))
            return FALSE;
    return TRUE;
}

static void BuildReserved(void)
{
    u32 member, rank, slot;
    u8 savedMember = sLeagueMember;
    struct OrigMon team[MAX_PARTY];
    sReservedKey = RH_SettingsHash() + 7;
    sReservedCount = 0;
    sLeagueMember = 1;                                       // the reserved list itself may use reserved species
    for (member = 0; member < 5; member++)
    {
        u32 count = LoadTeam(sRhLeagueMembers[member], team);
        u32 theme = MemberTheme(member, team, count);
        for (rank = 0; rank < S->leagueUnique && sReservedCount < MAX_RESERVED; rank++)
        {
            struct RhFilter f, g;
            u32 j, sp = 0, orig = 0, h;
            for (slot = 0; slot < count; slot++)
                if (LevelRank(team, count, slot, FALSE) == rank)
                    orig = team[slot].sp;
            if (orig == 0)
                continue;
            RH_FilterInit(&f);
            if (S->trainerNoLegends)
                f.legend = 1;
            f.allowVariants = S->trainerAllowAltFormes;
            f.type = theme;
            f.extra = NotStarterFamily;
            for (j = 0; j < sReservedCount; j++)
                RH_FilterExclude(&f, sReserved[j]);
            // the league's unique Pokemon stay different from each other and keep the member's theme; Silver's
            // starter families are avoided when possible (tiny pools can't always)
            h = RH_Hash(SALT_LEAGUE, member * 8 + rank, 0);
            g = f;
            sp = S->trainerSimilarStrength ? RH_PickSpeciesNearBst(&g, h, RH_SpeciesBST(orig)) : RH_PickWithFilter(&g, h);
            if (sp != 0 && !RH_FilterAccepts(&f, sp))
                sp = RH_PickWithFilter(&f, h);
            if (sp == 0)
            {
                g = f;
                g.extra = NULL;                              // allow a starter family
                sp = RH_PickWithFilter(&g, h);
            }
            if (sp == 0)
            {
                g = f;
                g.extra = NULL;
                g.type = RH_TYPE_NONE;                       // then drop the theme, but never repeat one
                sp = RH_PickWithFilter(&g, h);
            }
            if (sp == 0)
            {
                g = f;
                sp = RH_PickSpeciesNearBst(&g, h, 0);
            }
            sReserved[sReservedCount++] = sp;
        }
    }
    sLeagueMember = savedMember;
}

static void EnsureReserved(void)
{
    if (sReservedKey != RH_SettingsHash() + 7)
        BuildReserved();
}

// ---------------------------------------------------------------------------
// Team rules (diverse types / avoid duplicates / league-unique Pokemon / Wonder Guard)
// ---------------------------------------------------------------------------
static BOOL HasOnlyWonderGuard(u32 sp)
{
    u32 a1 = PokePersonalParaGet(sp, PERSONAL_ABILITY_1), a2 = PokePersonalParaGet(sp, PERSONAL_ABILITY_2);
    return a1 == ABILITY_WONDER_GUARD && (a2 == ABILITY_WONDER_GUARD || a2 == ABILITY_NONE);
}

static BOOL TeamPredicate(u32 sp)
{
    u32 i;
    if (sNoWonderGuard && HasOnlyWonderGuard(sp))
        return FALSE;
    if (sLeagueMember == 0 && IsLeagueReserved(sp))
        return FALSE;                                        // kept for the Elite Four / Champion
    if (sLeagueMember != 0 && sAvoidDupes && IsOwnReservedFamily(sp))
        return FALSE;                                        // their unique Pokemon are on the team already
    for (i = 0; i < sTeamCount; i++)
    {
        if (sAvoidDupes && FamilyRoot(sTeam[i]) == FamilyRoot(sp))
            return FALSE;
        if (sDiverse && (RH_SpeciesHasType(sp, RH_SpeciesType(sTeam[i], 0)) || RH_SpeciesHasType(sp, RH_SpeciesType(sTeam[i], 1))))
            return FALSE;
    }
    return TRUE;
}

static void SetupTeamFilter(struct RhFilter *f, u32 level, u32 tier, u32 theme)
{
    RH_FilterInit(f);
    if (S->trainerNoLegends)
        f->legend = 1;
    f->allowVariants = S->trainerAllowAltFormes;
    sAvoidDupes = S->trainerAvoidDuplicates;
    sDiverse = S->diverseTypes[tier] && theme == RH_TYPE_NONE;   // FVX: ignored for type-themed trainers
    sNoWonderGuard = S->noEarlyWonderGuard && level < 20;
    f->extra = TeamPredicate;
    f->type = theme;
    // "No Premature Evolutions" (FVX): nothing evolved further than its level allows
    if (S->noPrematureEvos && !(S->trainersEvolveOn && level >= S->trainersEvolveLevel))
        f->legalAtLevel = level > 255 ? 255 : level;
}

// ---------------------------------------------------------------------------
// Evolving trainer Pokemon to their level
// ---------------------------------------------------------------------------
static BOOL IsLevelMethod(u32 method)
{
    switch (method)
    {
    case EVO_LEVEL: case EVO_LEVEL_ATK_GT_DEF: case EVO_LEVEL_ATK_EQ_DEF: case EVO_LEVEL_ATK_LT_DEF:
    case EVO_LEVEL_PID_LO: case EVO_LEVEL_PID_HI: case EVO_LEVEL_NINJASK: case EVO_LEVEL_MALE: case EVO_LEVEL_FEMALE:
    case EVO_LEVEL_DAY: case EVO_LEVEL_NIGHT: case EVO_LEVEL_DUSK: case EVO_LEVEL_RAIN:
    case EVO_LEVEL_DARK_TYPE_MON_IN_PARTY: case EVO_LEVEL_NATURE_AMPED: case EVO_LEVEL_NATURE_LOW_KEY:
        return TRUE;
    }
    return FALSE;
}

static u32 DecodeTarget(u32 t)
{
    return (t >> 11) ? RH_SpeciesIndex(t & 0x7FF, t >> 11) : t;
}

static u32 EvolveForLevelEx(u32 sp, u32 level, u32 salt, u32 *steps)
{
    u32 i, guard;
    u32 fullBy = sForceEvolve ? 100 : (S->trainersEvolveOn ? S->trainersEvolveLevel : 0);
    for (guard = 0; guard < 3; guard++)
    {
        u16 e[MAX_EVOS_PER_POKE * 3 + 1];
        u16 legal[MAX_EVOS_PER_POKE];
        u32 n = 0, next, est;
        ReadWholeNarcMemberByIdPair(e, ARC_EVOLUTIONS, sp);
        // FVX: pick among the evolutions that are possible at this level (not a branch that isn't reached yet)
        for (i = 0; i < MAX_EVOS_PER_POKE; i++)
        {
            u32 method = e[i * 3], param = e[i * 3 + 1], t;
            BOOL can;
            if (method == EVO_NONE || e[i * 3 + 2] == 0 || method == EVO_LEVEL_SHEDINJA)
                continue;
            t = DecodeTarget(e[i * 3 + 2]);
            if (t == sp)
                continue;
            if (IsLevelMethod(method))
                can = level >= param && param > 1;
            else
            {
                est = RH_EstimateEvoLevel(sp, t);            // FVX: estimated evo level
                can = fullBy && level >= (est < fullBy ? est : fullBy);
            }
            if (fullBy && level >= fullBy)
                can = TRUE;
            if (can)
                legal[n++] = t;
        }
        if (n == 0)
            break;
        next = legal[RH_Hash(SALT_EVO_LEVEL, sp, salt) % n];
        if (sLeagueMember == 0 && IsLeagueReserved(next))
            break;                                           // the Elite Four's unique Pokemon stay theirs
        if (sEvolveKeepType != RH_TYPE_NONE && !RH_SpeciesHasType(next, sEvolveKeepType))
            break;                                           // a themed trainer's Pokemon keep the theme
        sp = next;
        (*steps)++;
    }
    return sp;
}

static u32 EvolveForLevel(u32 sp, u32 level, u32 salt)
{
    u32 steps = 0;
    return EvolveForLevelEx(sp, level, salt, &steps);
}

// Evolve "stages" times along the Pokemon's real (unrandomized) evolution line, so Silver's starter stays the
// line of the ball he stole (the first listed branch for branched lines).
static u32 VanillaEvolveTimes(u32 sp, u32 stages)
{
    while (stages-- > 0)
    {
        u16 e[MAX_EVOS_PER_POKE * 3 + 1];
        u32 i, next = 0;
        ReadFromNarcMemberByIdPair(e, ARC_EVOLUTIONS, sp, 0, MAX_EVOS_PER_POKE * 6);   // raw
        for (i = 0; i < MAX_EVOS_PER_POKE && next == 0; i++)
            if (e[i * 3] != EVO_NONE && e[i * 3] != EVO_LEVEL_SHEDINJA && e[i * 3 + 2] != 0)
                next = DecodeTarget(e[i * 3 + 2]);
        if (next == 0)
            break;
        sp = next;
    }
    return sp;
}

// ---------------------------------------------------------------------------
// Use Local Pokemon: trainers use the (randomized) wild Pokemon of the map they stand on, evolved to their level.
// (FVX uses the wild Pokemon of the whole game; working that out for every map would stall the DS.)
// ---------------------------------------------------------------------------
#define FS_MAP_EVENTS 0x14
#define MAP_EVENTS_ENCOUNTERS 0x920

static u32 LocalSpecies(u32 hash, u32 level)
{
    const u8 *events;
    const EncounterData *d;
    u16 list[12 + 5 + 15];
    u32 n = 0, i, v;
    if (gFieldSysPtr == NULL)
        return 0;
    events = *(const u8 **)((const u8 *)gFieldSysPtr + FS_MAP_EVENTS);
    if (events == NULL)
        return 0;
    d = (const EncounterData *)(events + MAP_EVENTS_ENCOUNTERS);
    if (d->rateWalk)
        for (i = 0; i < NUM_ENCOUNTERS_LAND; i++)
            list[n++] = d->landSlots.speciesDay[i];
    if (d->rateSurf)
        for (i = 0; i < NUM_ENCOUNTERS_SURF; i++)
            list[n++] = d->surfSlots[i].species;
    if (d->rateOldRod)
        for (i = 0; i < NUM_ENCOUNTERS_FISH; i++)
            list[n++] = d->oldRodSlots[i].species;
    if (d->rateGoodRod)
        for (i = 0; i < NUM_ENCOUNTERS_FISH; i++)
            list[n++] = d->goodRodSlots[i].species;
    if (d->rateSuperRod)
        for (i = 0; i < NUM_ENCOUNTERS_FISH; i++)
            list[n++] = d->superRodSlots[i].species;
    if (n == 0)
        return 0;
    v = list[hash % n];
    if (v == 0 || (v & 0x7FF) > SPECIES_MAX_MON_NUM)
        return 0;
    v = RH_SpeciesIndex(v & 0x7FF, (v >> 11) ? (v >> 11) : RH_WildForm(v & 0x7FF));
    return EvolveForLevel(v, level, 7);
}

// ---------------------------------------------------------------------------
// Better movesets
// ---------------------------------------------------------------------------
static const u16 sSetupMoves[] = {
    MOVE_SWORDS_DANCE, MOVE_NASTY_PLOT, MOVE_CALM_MIND, MOVE_DRAGON_DANCE, MOVE_BULK_UP, MOVE_QUIVER_DANCE,
    MOVE_RECOVER, MOVE_ROOST, MOVE_SOFT_BOILED, MOVE_TOXIC, MOVE_THUNDER_WAVE, MOVE_WILL_O_WISP, MOVE_SPORE,
    MOVE_SLEEP_POWDER, MOVE_LEECH_SEED, MOVE_PROTECT, MOVE_SUBSTITUTE, MOVE_STEALTH_ROCK, MOVE_AGILITY,
};

#define SPLIT_STATUS 2

static void MoveInfo(u32 move, struct BattleMove *bm)
{
    ReadFromNarcMemberByIdPair(bm, ARC_MOVE_DATA, move, 0, sizeof(*bm));
}

static BOOL IsDamaging(const struct BattleMove *bm)
{
    return bm->split != SPLIT_STATUS && bm->power > 1;
}

// FVX "Better Movesets": the strongest moves of up to three types (STAB and the better attacking stat count),
// plus one setup / status move, from the Pokemon's level-up moves (known by now) and TMs.
static void BetterMoveset(u32 sp, u32 level, u16 *moves)
{
    u16 best[3] = { 0, 0, 0 }, candidates[MAX_LEVELUP_MOVES + 92];
    u32 score[3] = { 0, 0, 0 };
    u8 bestType[3] = { 0xFF, 0xFF, 0xFF };
    u16 status = MOVE_NONE, species, form;
    u32 i, j, k, n = 0, count = 0, levelUpCount;
    u32 learnset[MAX_LEVELUP_MOVES];
    u32 tm[MACHINE_LEARNSETS_BITFIELD_COUNT];
    BOOL physical = PokePersonalParaGet(sp, PERSONAL_BASE_ATTACK) >= PokePersonalParaGet(sp, PERSONAL_BASE_SP_ATTACK);

    RH_SpeciesKey(sp, &species, &form);
    LoadLevelUpLearnset_HandleAlternateForm(species, form, learnset);
    for (i = 0; i < MAX_LEVELUP_MOVES; i++)
    {
        u32 m = LEVEL_UP_LEARNSET_MOVE(learnset[i]);
        if (m == LEVEL_UP_LEARNSET_END)
            break;
        if (LEVEL_UP_LEARNSET_LEVEL(learnset[i]) <= level && m != MOVE_NONE)
            candidates[count++] = m;
    }
    levelUpCount = count;
    ReadFromNarcMemberByIdPair(tm, ARC_CODE_ADDONS, CODE_ADDON_MACHINE_LEARNSETS, sp * sizeof(tm), sizeof(tm));
    for (i = ITEM_TM001; i <= ITEM_TM092; i++)
    {
        u32 idx = ItemToMachineMoveIndex(i);
        if (RH_MachineCompat(sp, idx, (tm[idx / 32] >> (idx % 32)) & 1))
            candidates[count++] = ItemToMachineMove(i);
    }
    for (i = 0; i < count; i++)
    {
        struct BattleMove bm;
        u16 m = candidates[i];
        MoveInfo(m, &bm);
        if (IsDamaging(&bm))
        {
            u32 acc = bm.accuracy ? bm.accuracy : 100;
            u32 s = bm.power * acc / 100;
            u32 type = bm.type;
            if (RH_SpeciesHasType(sp, type))
                s = s * 3 / 2;
            if ((bm.split == 0) == physical)
                s = s * 5 / 4;                               // synergy with the better attacking stat
            if (m == MOVE_EXPLOSION || m == MOVE_SELF_DESTRUCT || m == MOVE_MISTY_EXPLOSION || m == MOVE_HYPER_BEAM
             || m == MOVE_GIGA_IMPACT)
                s /= 3;
            // keep the best move per type, up to 3 types
            for (j = 0; j < 3; j++)
            {
                if (bestType[j] == type)
                {
                    if (s > score[j])
                        best[j] = m, score[j] = s;
                    break;
                }
            }
            if (j == 3)
            {
                u32 worst = 0;
                for (k = 1; k < 3; k++)
                    if (score[k] < score[worst])
                        worst = k;
                if (s > score[worst])
                    best[worst] = m, score[worst] = s, bestType[worst] = type;
            }
        }
        else if (status == MOVE_NONE)
        {
            for (j = 0; j < NELEMS(sSetupMoves); j++)
                if (sSetupMoves[j] == m)
                    status = m;
        }
    }
    for (i = 0; i < 4; i++)
        moves[i] = MOVE_NONE;
    for (i = 0; i < 3; i++)
        if (best[i] != MOVE_NONE)
            moves[n++] = best[i];
    if (status != MOVE_NONE)
        moves[n++] = status;
    // top up with the newest remaining level-up moves if needed
    for (i = levelUpCount; i > 0 && n < 4; i--)
    {
        u16 m = candidates[i - 1];
        for (j = 0; j < n; j++)
            if (moves[j] == m)
                break;
        if (j == n)
            moves[n++] = m;
    }
}

// The moves the Pokemon will actually have (its set moves, or the default level-up ones).
static u32 EffectiveMoves(const struct RhTrainerMon *m, u32 sp, u16 *moves)
{
    u32 i, n = 0;
    u32 learnset[MAX_LEVELUP_MOVES];
    if (m->flags & RHTM_MOVES)
    {
        for (i = 0; i < 4; i++)
            if (m->moves[i] != MOVE_NONE)
                moves[n++] = m->moves[i];
        return n;
    }
    LoadLevelUpLearnset_HandleAlternateForm(m->species, m->form, learnset);
    for (i = 0; i < MAX_LEVELUP_MOVES; i++)
    {
        u32 mv = LEVEL_UP_LEARNSET_MOVE(learnset[i]);
        if (mv == LEVEL_UP_LEARNSET_END || LEVEL_UP_LEARNSET_LEVEL(learnset[i]) > m->level)
            break;
        if (n < 4)
            moves[n++] = mv;
        else
        {
            moves[0] = moves[1]; moves[1] = moves[2]; moves[2] = moves[3]; moves[3] = mv;
        }
    }
    (void)sp;
    return n;
}

// ---------------------------------------------------------------------------
// Held items
// ---------------------------------------------------------------------------
// type boost items / resist berries in the order of RH_MonTypeAt (Fairy last)
static const u16 sTypeBoostItems[RH_NUM_TYPES] = {
    ITEM_SILK_SCARF, ITEM_BLACK_BELT, ITEM_SHARP_BEAK, ITEM_POISON_BARB, ITEM_SOFT_SAND, ITEM_HARD_STONE,
    ITEM_SILVER_POWDER, ITEM_SPELL_TAG, ITEM_METAL_COAT, ITEM_CHARCOAL, ITEM_MYSTIC_WATER, ITEM_MIRACLE_SEED,
    ITEM_MAGNET, ITEM_TWISTED_SPOON, ITEM_NEVER_MELT_ICE, ITEM_DRAGON_FANG, ITEM_BLACK_GLASSES, ITEM_FAIRY_FEATHER,
};

static const u16 sResistBerries[RH_NUM_TYPES] = {
    ITEM_CHILAN_BERRY, ITEM_CHOPLE_BERRY, ITEM_COBA_BERRY, ITEM_KEBIA_BERRY, ITEM_SHUCA_BERRY, ITEM_CHARTI_BERRY,
    ITEM_TANGA_BERRY, ITEM_KASIB_BERRY, ITEM_BABIRI_BERRY, ITEM_OCCA_BERRY, ITEM_PASSHO_BERRY, ITEM_RINDO_BERRY,
    ITEM_WACAN_BERRY, ITEM_PAYAPA_BERRY, ITEM_YACHE_BERRY, ITEM_HABAN_BERRY, ITEM_COLBUR_BERRY, ITEM_ROSELI_BERRY,
};

// FVX's held item lists for trainers: consumable items, and the other battle items.
static const u16 sConsumableHeld[] = {
    ITEM_CHERI_BERRY, ITEM_CHESTO_BERRY, ITEM_PECHA_BERRY, ITEM_RAWST_BERRY, ITEM_ASPEAR_BERRY, ITEM_LEPPA_BERRY,
    ITEM_ORAN_BERRY, ITEM_PERSIM_BERRY, ITEM_LUM_BERRY, ITEM_SITRUS_BERRY, ITEM_FIGY_BERRY, ITEM_WIKI_BERRY,
    ITEM_MAGO_BERRY, ITEM_AGUAV_BERRY, ITEM_IAPAPA_BERRY, ITEM_LIECHI_BERRY, ITEM_GANLON_BERRY, ITEM_SALAC_BERRY,
    ITEM_PETAYA_BERRY, ITEM_APICOT_BERRY, ITEM_LANSAT_BERRY, ITEM_STARF_BERRY, ITEM_MICLE_BERRY, ITEM_CUSTAP_BERRY,
    ITEM_JABOCA_BERRY, ITEM_ROWAP_BERRY, ITEM_WHITE_HERB, ITEM_MENTAL_HERB, ITEM_POWER_HERB, ITEM_FOCUS_SASH,
    ITEM_AIR_BALLOON, ITEM_WEAKNESS_POLICY,
};

static const u16 sOtherHeld[] = {
    ITEM_BRIGHT_POWDER, ITEM_QUICK_CLAW, ITEM_KINGS_ROCK, ITEM_SCOPE_LENS, ITEM_LEFTOVERS, ITEM_SHELL_BELL,
    ITEM_FOCUS_BAND, ITEM_WIDE_LENS, ITEM_MUSCLE_BAND, ITEM_WISE_GLASSES, ITEM_EXPERT_BELT, ITEM_LIGHT_CLAY,
    ITEM_LIFE_ORB, ITEM_ZOOM_LENS, ITEM_BIG_ROOT, ITEM_CHOICE_BAND, ITEM_CHOICE_SCARF, ITEM_CHOICE_SPECS,
    ITEM_ROCKY_HELMET, ITEM_ASSAULT_VEST, ITEM_EVIOLITE, ITEM_BLACK_SLUDGE, ITEM_HEAT_ROCK, ITEM_DAMP_ROCK,
    ITEM_SMOOTH_ROCK, ITEM_ICY_ROCK, ITEM_SILK_SCARF, ITEM_BLACK_BELT, ITEM_SHARP_BEAK, ITEM_POISON_BARB,
    ITEM_SOFT_SAND, ITEM_HARD_STONE, ITEM_SILVER_POWDER, ITEM_SPELL_TAG, ITEM_METAL_COAT, ITEM_CHARCOAL,
    ITEM_MYSTIC_WATER, ITEM_MIRACLE_SEED, ITEM_MAGNET, ITEM_TWISTED_SPOON, ITEM_NEVER_MELT_ICE, ITEM_DRAGON_FANG,
    ITEM_BLACK_GLASSES, ITEM_FAIRY_FEATHER,
};

static u32 RandomHeldItem(u32 hash)
{
    u32 n = NELEMS(sConsumableHeld) + (S->heldConsumableOnly ? 0 : NELEMS(sOtherHeld));
    u32 r = hash % n;
    return r < NELEMS(sConsumableHeld) ? sConsumableHeld[r] : sOtherHeld[r - NELEMS(sConsumableHeld)];
}

// effectiveness of an attack type on a species, in quarters (4 = normal)
static u32 Effectiveness(u32 type, u32 sp)
{
    static const u8 sMul[4] = { 0, 1, 2, 4 };                // none, half, normal, double (in halves)
    u32 t1 = RH_SpeciesType(sp, 0), t2 = RH_SpeciesType(sp, 1);
    u32 m = sMul[RH_TypeEffect(type, t1)];
    return t2 != t1 ? m * sMul[RH_TypeEffect(type, t2)] : m * 2;
}

// FVX "Sensible Items": a random pick among items that make sense for this Pokemon and its moves.
static u32 SensibleItem(const struct RhTrainerMon *m, u32 sp, u32 hash)
{
    u16 list[48], moves[4];
    u32 n = 0, i, t, nMoves, damaging = 0, physical = 0;
    u32 spe = PokePersonalParaGet(sp, PERSONAL_BASE_SPEED);
    u32 bulk = PokePersonalParaGet(sp, PERSONAL_BASE_HP) + PokePersonalParaGet(sp, PERSONAL_BASE_DEFENSE)
             + PokePersonalParaGet(sp, PERSONAL_BASE_SP_DEFENSE);
    BOOL consumable = S->heldConsumableOnly;

    nMoves = EffectiveMoves(m, sp, moves);
    for (i = 0; i < nMoves; i++)
    {
        struct BattleMove bm;
        MoveInfo(moves[i], &bm);
        if (!IsDamaging(&bm))
            continue;
        damaging++;
        physical += (bm.split == 0);
        if (!consumable && n < 4)
            list[n++] = sTypeBoostItems[RH_TypeIndexOf(bm.type)];   // Silk Scarf only with a damaging Normal move
    }
    // resist berries for types this Pokemon is weak to
    for (t = 0; t < RH_NUM_TYPES; t++)
        if (Effectiveness(RH_MonTypeAt(t), sp) > 4)
            list[n++] = sResistBerries[t];
    list[n++] = ITEM_SITRUS_BERRY;
    list[n++] = ITEM_LUM_BERRY;
    list[n++] = ITEM_WHITE_HERB;
    if (bulk < 220 && spe >= 80)
        list[n++] = ITEM_FOCUS_SASH;
    if (!consumable)
    {
        list[n++] = ITEM_LEFTOVERS;
        if (damaging > 0)
            list[n++] = ITEM_LIFE_ORB;
        if (RH_EvolveOnce(sp, 0) != 0)
            list[n++] = ITEM_EVIOLITE;
        if (damaging == nMoves && nMoves >= 2)
        {
            list[n++] = ITEM_ASSAULT_VEST;
            list[n++] = ITEM_CHOICE_SCARF;
            if (physical == damaging)
                list[n++] = ITEM_CHOICE_BAND;
            if (physical == 0)
                list[n++] = ITEM_CHOICE_SPECS;
        }
    }
    return list[hash % n];
}

// ---------------------------------------------------------------------------
// Species choice
// ---------------------------------------------------------------------------
static u32 ChooseSpecies(const struct OrigMon *o, u32 slot, u32 tier, u32 theme)
{
    struct RhFilter f;
    u32 similar = 0, result, tries;
    u32 key = sTrainerId * 8 + slot;

    SetupTeamFilter(&f, o->level, tier, theme);
    if (theme == RH_TYPE_NONE && S->trainers == 6)
    {
        f.type = VanillaType(o->sp, 0);                      // "or primary": the replacement has the original's primary type
        sDiverse = 0;
    }
    if (S->trainerSimilarStrength)
        similar = o->sp;

    if (S->trainerLocalPokemon)
    {
        for (tries = 0; tries < 8; tries++)
        {
            result = LocalSpecies(RH_Hash(SALT_TRAINER, key, 1 + tries), o->level);
            if (result != 0 && (f.type == RH_TYPE_NONE || RH_SpeciesHasType(result, f.type))
             && !(f.legend == 1 && RH_IsLegendary(result)) && TeamPredicate(result))
                return result;
        }
    }
    if (S->trainers == 2)
    {
        // "Random (even distribution)": the key-th allowed Pokemon in a keyed order, so every Pokemon is used about
        // equally often; similar strength takes precedence (FVX): even distribution inside the +-20% window
        struct RhFilter g = f;
        if (similar != 0)
        {
            u32 bst = RH_SpeciesBST(similar);
            g.minBst = bst * 80 / 100;
            g.maxBst = bst * 120 / 100;
        }
        result = RH_PickRanked(&g, key, SALT_TRAINER);
        if (result != 0)
            return result;
    }
    return RH_PickSpeciesNearBst(&f, RH_Hash(SALT_TRAINER, key, 0), similar ? RH_SpeciesBST(similar) : 0);
}

// ---------------------------------------------------------------------------
// Silver: "Rival Keeps Same Team" roster - one roster of basic Pokemon for the whole game. Each of his battles uses
// the roster in order (his starter keeps its own slot), every member evolved as far as its level allows.
// ---------------------------------------------------------------------------
#define ROSTER_SIZE 6
static u32 sRosterKey;
static u16 sRoster[ROSTER_SIZE];

static BOOL NotRosterConflict(u32 sp)
{
    return NotStarterFamily(sp) && !IsLeagueReserved(sp);
}

static u32 RosterSpecies(u32 index)
{
    u32 i, j;
    if (sRosterKey != RH_SettingsHash() + 17)
    {
        sRosterKey = RH_SettingsHash() + 17;
        for (i = 0; i < ROSTER_SIZE; i++)
        {
            struct RhFilter f;
            RH_FilterInit(&f);
            if (S->trainerNoLegends)
                f.legend = 1;
            f.stage = 1;                                     // basic Pokemon: they grow with him
            f.allowVariants = S->trainerAllowAltFormes;
            f.extra = NotRosterConflict;
            for (j = 0; j < i; j++)
                RH_FilterExclude(&f, sRoster[j]);
            sRoster[i] = RH_PickSpeciesNearBst(&f, RH_Hash(SALT_TRAINER, 0xA17A1, i), 0);
        }
    }
    return sRoster[index < ROSTER_SIZE ? index : ROSTER_SIZE - 1];
}

static BOOL UsesRivalRoster(void)
{
    return S->rivalSameTeam && S->trainers != 0 && IsRival(sTrainerId) && !IsFirstRivalBattle(sTrainerId);
}

// Roster index of a party slot: his non-starter slots in order (slots past the party are added Pokemon).
static u32 RosterIndex(u32 slot)
{
    u32 i, j = 0;
    for (i = 0; i < slot && i < sOrigCount; i++)
        if (!IsStarterSlot(sOrig, i))
            j++;
    if (slot >= sOrigCount)
        j += slot - sOrigCount;
    return j < ROSTER_SIZE ? j : ROSTER_SIZE - 1;
}

// ---------------------------------------------------------------------------
// The hooks
// ---------------------------------------------------------------------------
u32 RH_TrainerPartyBegin(u32 trainerId, u32 dataType, const u8 *buf, u32 count, u32 fightType)
{
    u32 tier, want, i, lo = 255, hi = 1, atHi = 0;
    sActive = FALSE;
    if (!RH_Active() || count == 0)
        return count;
    sActive = TRUE;
    RH_BeginSession(0xFFFF);
    sTrainerId = trainerId;
    sTeamCount = 0;
    sTypeCountReady = FALSE;
    sOrigCount = ParseParty(buf, dataType, count, sOrig);
    sTotal = sOrigCount;
    if (IsFirstRivalBattle(trainerId) || (fightType & (BATTLE_TYPE_MULTI | BATTLE_TYPE_TAG)))
        return sTotal;
    // "Additional Pokemon" (FVX): chosen like the rest of the team, with levels between the team's lowest and its
    // highest - 1 (the highest itself when two or more share it)
    tier = TierOf(trainerId);
    want = sOrigCount + S->additionalMons[tier];
    if (want > MAX_PARTY)
        want = MAX_PARTY;
    for (i = 0; i < sOrigCount; i++)
    {
        lo = sOrig[i].level < lo ? sOrig[i].level : lo;
        hi = sOrig[i].level > hi ? sOrig[i].level : hi;
    }
    for (i = 0; i < sOrigCount; i++)
        atHi += (sOrig[i].level == hi);
    if (atHi >= 2)
        hi++;
    for (i = sOrigCount; i < want; i++)
    {
        u32 span = (hi > lo + 1) ? hi - lo : 1;
        sOrig[i] = sOrig[(i - 1) % sOrigCount];
        sOrig[i].level = (hi > lo) ? lo + RH_Hash(SALT_EXTRA_MON, trainerId, i + 16) % span : lo;
    }
    sTotal = want;
    return sTotal;
}

u32 RH_TrainerPartyOrder(u32 slot)
{
    // the team's last Pokemon (its ace) stays last
    if (!sActive || sTotal <= sOrigCount || sOrigCount == 0)
        return slot;
    if (slot + 1 == sOrigCount)
        return sTotal - 1;
    if (slot >= sOrigCount)
        return slot - 1;
    return slot;
}

void RH_TrainerPartyEnd(void)
{
    if (sActive)
        RH_EndSession();
    sActive = FALSE;
}

void RH_TrainerMon(u32 slot, struct RhTrainerMon *m)
{
    u32 tier, stage, sp, newSp, trainerId = sTrainerId;
    s32 member, starterSlot;
    BOOL changed = FALSE, extra = slot >= sOrigCount;
    struct OrigMon *o = &sOrig[slot < MAX_PARTY ? slot : MAX_PARTY - 1];
    if (!sActive)
        return;
    tier = TierOf(trainerId);
    if (extra)
    {
        // an Additional Pokemon: a copy of a team member's basics, chosen below
        m->species = 0;
        m->form = 0;
        m->level = o->level;
        m->ivs = o->ivs;
        m->abilitySlot = 0;
        m->flags = 0;
    }
    sp = extra ? o->sp : RH_SpeciesIndex(m->species, m->form);
    newSp = sp;
    member = (S->leagueUnique && S->trainers != 0) ? LeagueMemberOf(trainerId) : -1;
    sLeagueMember = member + 1;

    if (S->trainerLevelModOn && !(IsFirstRivalBattle(trainerId) && S->trainerLevelMod > 0))
    {
        s32 lv = RH_ApplyPercent(m->level, S->trainerLevelMod);
        m->level = lv < 1 ? 1 : (lv > 100 ? 100 : lv);
    }

    starterSlot = extra ? -1 : StarterFamily(sp, &stage);
    if (starterSlot >= 0 && IsRival(trainerId))
    {
        // Owner's rule: exactly the Pokemon in the ball Silver stole, whatever the other options; evolved along its
        // own line to the stage the original starter had in this battle.
        newSp = VanillaEvolveTimes(RH_StarterForSlot(starterSlot), stage);
        changed = TRUE;
    }
    else if (UsesRivalRoster())
    {
        u32 steps = 0, ri = RosterIndex(slot);
        sForceEvolve = TRUE;
        newSp = EvolveForLevelEx(RosterSpecies(ri), m->level, ri, &steps);   // same branch every battle
        sForceEvolve = FALSE;
        changed = TRUE;
    }
    else if (member >= 0 && !extra && LevelRank(sOrig, sOrigCount, slot, FALSE) < S->leagueUnique)
    {
        u32 idx;
        EnsureReserved();
        idx = member * S->leagueUnique + LevelRank(sOrig, sOrigCount, slot, FALSE);
        if (idx < sReservedCount && sReserved[idx] != 0)
        {
            newSp = sReserved[idx];
            if (S->trainersEvolveOn)
                newSp = EvolveForLevel(newSp, m->level, slot);
            changed = TRUE;
        }
    }
    if (!changed && S->trainers != 0)
    {
        u32 theme = ThemeOf(trainerId, sOrig, sOrigCount);
        newSp = ChooseSpecies(o, slot, tier, theme);
        if (newSp == 0)
            newSp = sp;
        if (S->trainersEvolveOn)
        {
            sEvolveKeepType = theme;
            if (theme == RH_TYPE_NONE && S->trainers == 6)
                sEvolveKeepType = VanillaType(o->sp, 0);
            newSp = EvolveForLevel(newSp, m->level, slot);
            sEvolveKeepType = RH_TYPE_NONE;
        }
    }
    else if (!changed && extra)
    {
        // Trainers unchanged: pick by the "Keep Themed" rules (FVX), avoiding the team's species
        struct RhFilter f;
        u32 g = GroupOf(trainerId);
        u32 theme = (g && sRhGroupType[g] != RH_TYPE_NONE) ? sRhGroupType[g] : TeamTheme(sOrig, sOrigCount);
        SetupTeamFilter(&f, m->level, tier, theme);
        sAvoidDupes = TRUE;
        newSp = RH_PickSpeciesNearBst(&f, RH_Hash(SALT_EXTRA_MON, trainerId, slot), S->trainerSimilarStrength ? RH_SpeciesBST(sp) : 0);
        if (S->trainersEvolveOn)
            newSp = EvolveForLevel(newSp, m->level, slot);
    }
    else if (!changed && S->trainersEvolveOn)
    {
        newSp = EvolveForLevel(sp, m->level, slot);
    }
    if (newSp == 0)
        newSp = sp;

    if (newSp != sp || extra)
    {
        RH_SpeciesKey(newSp, &m->species, &m->form);
        m->flags &= ~(RHTM_MOVES | RHTM_ABILITY);            // default moves / abilities for the new species
        m->flags |= RHTM_NO_NICKNAME;
    }
    if (S->abilities)
        m->flags &= ~RHTM_ABILITY;                           // hand-picked abilities don't exist on randomized species
    if (S->noEarlyWonderGuard && m->level < 20 && !(m->flags & RHTM_ABILITY))
    {
        // the ability slot would give Wonder Guard: use the other ability
        u32 a1 = PokePersonalParaGet(newSp, PERSONAL_ABILITY_1), a2 = PokePersonalParaGet(newSp, PERSONAL_ABILITY_2);
        BOOL second = a2 != 0 && ((m->abilitySlot & 1) || m->abilitySlot == TRAINER_POKEMON_ABILITY_2);
        u32 got = second ? a2 : a1, other = second ? a1 : a2;
        if (m->abilitySlot == TRAINER_POKEMON_ABILITY_HIDDEN)
            got = GetMonHiddenAbility(m->species, m->form);
        if (got == ABILITY_WONDER_GUARD && other != ABILITY_NONE && other != ABILITY_WONDER_GUARD)
        {
            m->ability = other;
            m->flags |= RHTM_ABILITY;
        }
    }
    if (S->movesets == 3)
    {
        m->moves[0] = MOVE_METRONOME;
        m->moves[1] = m->moves[2] = m->moves[3] = MOVE_NONE;
        m->flags |= RHTM_MOVES;
    }
    else if (S->betterMovesets[tier])
    {
        BetterMoveset(newSp, m->level, m->moves);
        m->flags |= RHTM_MOVES;
    }
    else if (S->movesets != 0)
    {
        m->flags &= ~RHTM_MOVES;                             // hand-picked moves may not exist in the random learnset
    }
    if (S->heldItemsFor[tier] && !IsFirstRivalBattle(trainerId) && (!S->heldHighestOnly || IsHighestLevelSlot(slot)))
    {
        u32 h = RH_Hash(SALT_HELD_ITEM, trainerId, slot);
        m->item = S->heldSensible ? SensibleItem(m, newSp, h) : RandomHeldItem(h);
        m->flags |= RHTM_ITEM;
    }
    if (S->trainerRandomShiny && (RH_Hash(SALT_TRAINER_SHINY, trainerId, slot) & 0xFF) == 0)
        m->flags |= RHTM_SHINY;                              // FVX: 1/256
    if (sTeamCount < MAX_PARTY)
        sTeam[sTeamCount++] = newSp;
    sLeagueMember = 0;
}
