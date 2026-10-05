// Romhack randomizer core: Pokemon Traits - base stats, base stat totals, EXP curves, types, abilities, catch rate
// (port of the FireRed/Emerald rh_traits.c). Applied to the personal data as the game reads it
// (RH_TransformPersonal, called from the data loader hook).
#include "../../include/types.h"
#include "../../include/save.h"
#include "../../include/constants/file.h"
#include "../../include/constants/species.h"
#include "../../include/constants/ability.h"
#include "../../include/constants/pokemon.h"
#include "../../include/rh/rh_internal.h"
#include "rh_abilities.h"

// HeartGold type ids in the order the FireRed/Emerald versions use (Fairy last).
static const u8 sMonTypes[RH_NUM_TYPES] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 10, 11, 12, 13, 14, 15, 16, 17, 9 };

u32 RH_MonTypeAt(u32 i)
{
    return sMonTypes[i % RH_NUM_TYPES];
}

u32 RH_TypeIndexOf(u32 type)
{
    u32 i;
    for (i = 0; i < RH_NUM_TYPES; i++)
        if (sMonTypes[i] == type)
            return i;
    return 0;
}

u32 RH_RandomMonType(u32 hash)
{
    return sMonTypes[hash % RH_NUM_TYPES];
}

// Hash of every setting: cache key for results that depend on many options.
u32 RH_SettingsHash(void)
{
    const u8 *b = (const u8 *)S;
    u32 i, h = 2166136261u;
    for (i = 0; i < sizeof(struct RhSettings); i++)
        h = (h ^ b[i]) * 16777619u;
    return h | 1;
}

// ---------------------------------------------------------------------------
// Forms and families
// ---------------------------------------------------------------------------
// Alternate forms (Megas, Rotom appliances, Unown letters, battle forms...) follow their base form, like FVX's
// "Follow Mega Evolutions" / cosmetic forms. Regional forms are Pokemon of their own.
static u32 FormBaseEx(u32 sp, BOOL followMegas)
{
    struct RhSpeciesInfo i;
    if (!RH_GetInfo(sp, &i) || RH_KEY_FORM(i.key) == 0)
        return sp;
    if (i.flags2 & RH_SPF2_REGIONAL)
        return sp;
    if ((i.flags2 & RH_SPF2_MEGA_LIKE) && !followMegas)
        return sp;                                       // FVX: Megas get their own rolls unless "Follow Mega Evolutions"
    return RH_KEY_SPECIES(i.key);
}

static u32 FormBase(u32 sp)
{
    return FormBaseEx(sp, TRUE);
}

// Different Pokemon (forms count once) this species evolves into in the base game, among the allowed generations.
static u32 DistinctTargets(const struct RhSpeciesInfo *i)
{
    switch (S->speciesPool)
    {
    case RH_POOL_GEN1:   return (i->children >> 4) & 3;
    case RH_POOL_GEN1_3: return (i->children >> 6) & 3;
    default:             return i->children & 15;
    }
}

// Root for "Follow Evolutions" traits: like the family root, but a split evolution (Eeveelutions, Bellossom,
// Slowking, Gallade, Shedinja...) starts its own branch, as in FVX.
static u32 TraitRoot(u32 sp)
{
    u32 g;
    for (g = 0; g < 3; g++)
    {
        struct RhSpeciesInfo pre;
        u32 p = RH_PreEvo(sp);
        if (p == 0 || !RH_GetInfo(p, &pre) || DistinctTargets(&pre) > 1)
            break;
        sp = p;
    }
    return sp;
}

static u32 FollowRoot(u32 sp)
{
    return FormBase(TraitRoot(sp));
}

// ---------------------------------------------------------------------------
// Types
// ---------------------------------------------------------------------------
static u32 VanillaType(u32 sp, u32 slot)
{
    struct RhSpeciesInfo i;
    RH_GetInfo(sp, &i);
    return slot ? i.type2 : i.type1;
}

static u32 SecondType(u32 key, u32 t1, u32 salt2)
{
    u32 t2 = RH_RandomMonType(RH_Hash(SALT_TYPE2, key, salt2));
    if (t2 == t1)
        t2 = sMonTypes[(RH_TypeIndexOf(t1) + 1 + RH_Hash(SALT_TYPE2, key, salt2 + 1) % 17) % 18];
    return t2;
}

u32 RH_SpeciesType(u32 sp, u32 slot)
{
    struct RhSpeciesInfo info;
    u32 key, t1;
    if (!S->enabled || S->types == 0 || !RH_GetInfo(sp, &info))
        return VanillaType(sp, slot);
    if (FormBaseEx(sp, S->typesFollowMegas) != sp)
    {
        u32 base = FormBase(sp);
        struct RhSpeciesInfo b;
        RH_GetInfo(base, &b);
        t1 = RH_SpeciesType(base, 0);
        if (info.type1 == b.type1 && info.type2 == b.type2)
            return RH_SpeciesType(base, slot);               // same typing as its base form
        if (slot == 0)
            return t1;
        if (info.type1 == info.type2 && !S->forceDualTypes)
            return t1;
        return SecondType(sp, t1, 7);                        // the form's typing differs: a new second type
    }
    // Follow evolutions: the family shares its first type; evolutions that gain a second type in the base game
    // gain a (family-wide) random second type.
    key = (S->types == 1) ? FollowRoot(sp) : sp;
    t1 = RH_RandomMonType(RH_Hash(SALT_TYPE1, key, 0));
    if (slot == 0 || !(S->forceDualTypes || info.type1 != info.type2))
        return t1;
    return SecondType(key, t1, 0);
}

BOOL RH_SpeciesHasType(u32 sp, u32 type)
{
    return RH_SpeciesType(sp, 0) == type || RH_SpeciesType(sp, 1) == type;
}

// ---------------------------------------------------------------------------
// Base stats
// ---------------------------------------------------------------------------
enum { ST_HP, ST_ATK, ST_DEF, ST_SPE, ST_SPA, ST_SPD, NUM_ST };   // personal data order

static void VanillaStats(u32 sp, u8 *v)
{
    ReadFromNarcMemberByIdPair(v, ARC_PERSONAL, sp, 0, NUM_ST);   // raw read (not transformed)
}

static u32 Total(const u8 *v)
{
    u32 i, t = 0;
    for (i = 0; i < NUM_ST; i++)
        t += v[i];
    return t;
}

// Splits "amount" over the stats with random weights on top of "add"; stats in "fixed" are skipped.
static void Distribute(u32 amount, u32 key, u32 salt2, u8 *out, const u8 *add, u32 fixedMask)
{
    u32 w[NUM_ST], total = 0, given = 0, i;
    for (i = 0; i < NUM_ST; i++)
    {
        w[i] = (fixedMask & (1u << i)) ? 0 : 20 + RH_Hash(SALT_STATS, key, i + salt2 * 8) % 100;
        total += w[i];
    }
    for (i = 0; i < NUM_ST; i++)
    {
        u32 share = total ? amount * w[i] / total : 0;
        u32 val = (add ? add[i] : 0) + share;
        given += share;
        out[i] = val > 255 ? 255 : val;
    }
    for (i = 0; given < amount && i < NUM_ST * 4; i++)
    {
        u32 s = RH_Hash(SALT_STATS, key, 100 + i) % NUM_ST;
        if ((fixedMask & (1u << s)) || out[s] == 255)
            continue;
        out[s]++;
        given++;
    }
}

// FVX "Base Stat Totals" ----------------------------------------------------
static u32 FirstVanillaEvolution(u32 sp)
{
    u16 e[3];
    ReadFromNarcMemberByIdPair(e, ARC_EVOLUTIONS, sp, 0, sizeof(e));   // method, param, target (raw)
    if (e[0] == 0 || e[2] == 0)
        return 0;
    return (e[2] >> 11) ? RH_SpeciesIndex(e[2] & 0x7FF, e[2] >> 11) : e[2];
}

// FVX's estimated evolution level (AbstractRomHandler.findEvolutionLevel), looked up by (pre BST, post BST) and
// kept between the levels of the Pokemon's own neighbouring evolutions.
#include "rh_evo_levels.h"

static u32 RawEstimate(u32 from, u32 to)
{
    s32 i = ((s32)RH_SpeciesBST(from) - RH_EVO_BST_LO + RH_EVO_BST_STEP / 2) / RH_EVO_BST_STEP;
    s32 j = ((s32)RH_SpeciesBST(to) - RH_EVO_BST_LO + RH_EVO_BST_STEP / 2) / RH_EVO_BST_STEP;
    i = i < 0 ? 0 : (i > RH_EVO_BST_N - 1 ? RH_EVO_BST_N - 1 : i);
    j = j < 0 ? 0 : (j > RH_EVO_BST_N - 1 ? RH_EVO_BST_N - 1 : j);
    return sRhEvoLevelEstimate[i][j];
}

u32 RH_EstimateEvoLevel(u32 from, u32 to)
{
    u32 est = RawEstimate(from, to), pre = RH_PreEvo(from), next = FirstVanillaEvolution(to), v;
    if (pre != 0)
    {
        v = (RawEstimate(pre, from) * 5 + 3) / 4;
        est = est > v ? est : v;
    }
    if (next != 0)
    {
        v = (RawEstimate(to, next) * 4 + 4) / 5;
        est = est < v ? est : v;
    }
    return est < 2 ? 2 : (est > 100 ? 100 : est);
}

static u32 ShuffleGroup(const struct RhSpeciesInfo *m)
{
    u32 g = 0;
    if (S->bstFollowEvos)
        g = RH_INFO_CHAIN(m);                                // families are shuffled with families of the same length
    if (S->bstSeparateLegends)
        g = g * 2 + ((m->flags & RH_SPF_LEGENDARY) != 0);
    return g;
}

static BOOL ShuffleMember(u32 sp, const struct RhSpeciesInfo *m, u32 group)
{
    (void)sp;
    return (m->flags & RH_SPF_BASE) && (m->flags & RH_SPF_POOL) && ShuffleGroup(m) == group
        && (!S->bstFollowEvos || RH_INFO_STAGE(m) == 1);
}

// Shuffle: the base stat total of the Pokemon whose slot this one takes (keyed permutation inside its group).
static u32 ShuffledBst(u32 sp)
{
    struct RhSpeciesInfo m;
    u32 root = sp, depth = 0, group, count = 0, k = 0, i, n = RH_InfoCount(), donorIdx, donor = 0;
    if (S->bstFollowEvos)
    {
        while (depth < 2 && RH_PreEvo(root) != 0)
        {
            root = RH_PreEvo(root);
            depth++;
        }
    }
    RH_GetInfo(root, &m);
    if (!(m.flags & RH_SPF_BASE) || !(m.flags & RH_SPF_POOL))
        return RH_VanillaBST(sp);
    group = ShuffleGroup(&m);
    RH_BeginSession(3);
    for (i = 1; i < n; i++)
    {
        struct RhSpeciesInfo o;
        RH_GetInfo(i, &o);
        if (!ShuffleMember(i, &o, group))
            continue;
        if (i == root)
            k = count;
        count++;
    }
    donorIdx = RH_Permute(SALT_STATS + 0x55 + group, k, count);
    for (i = 1; i < n; i++)
    {
        struct RhSpeciesInfo o;
        RH_GetInfo(i, &o);
        if (ShuffleMember(i, &o, group) && donorIdx-- == 0)
        {
            donor = i;
            break;
        }
    }
    RH_EndSession();
    if (donor == 0)
        return RH_VanillaBST(sp);
    // evolutions take the total of the donor family's evolution at the same stage
    while (depth-- > 0 && donor != 0)
    {
        u32 next = FirstVanillaEvolution(donor);
        if (next == 0)
            break;
        donor = next;
    }
    return RH_VanillaBST(donor);
}

#define BST_CACHE 16
static struct { u16 sp; u16 bst; u32 key; } sBstCache[BST_CACHE];
static u8 sBstCacheNext;

static u32 ComputeBst(u32 sp)
{
    u32 base = FormBaseEx(sp, TRUE), vanilla = RH_VanillaBST(sp);
    if (base != sp)
    {
        // alternate forms: Megas get the base form's new total + 100, others keep their proportion
        struct RhSpeciesInfo i;
        u32 b = RH_SpeciesBST(base), bv = RH_VanillaBST(base), v;
        RH_GetInfo(sp, &i);
        v = (i.flags2 & RH_SPF2_MEGA_LIKE) ? b + 100 : (bv ? b * vanilla / bv : vanilla);
        return v > 255 * NUM_ST ? 255 * NUM_ST : v;
    }
    switch (S->bstMode)
    {
    case 1:
    {
        // random buff / nerf: each (family, with Follow Evolutions) gets one modifier
        u32 pct = S->bstChangePct > 100 ? 100 : S->bstChangePct, key = S->bstFollowEvos ? FollowRoot(sp) : sp, m, v;
        m = 100 - pct + RH_Hash(SALT_STATS + 0x66, key, 0) % (2 * pct + 1);
        v = vanilla * m / 100;
        return v > 255 * NUM_ST ? 255 * NUM_ST : v;
    }
    case 2:
        return ShuffledBst(sp);
    case 3:
        return 180 + RH_Hash(SALT_STATS + 0x77, sp, 0) % (720 - 180);   // between Sunkern and Arceus
    }
    return vanilla;
}

u32 RH_SpeciesBST(u32 sp)
{
    u32 i, key, v;
    if (sp == 0 || sp >= RH_InfoCount())
        return 0;
    if (!S->enabled || S->bstMode == 0)
        return RH_VanillaBST(sp);
    key = RH_SettingsHash();
    for (i = 0; i < BST_CACHE; i++)
        if (sBstCache[i].sp == sp && sBstCache[i].key == key)
            return sBstCache[i].bst;
    v = ComputeBst(sp);
    sBstCache[sBstCacheNext].sp = sp;
    sBstCache[sBstCacheNext].bst = v;
    sBstCache[sBstCacheNext].key = key;
    sBstCacheNext = (sBstCacheNext + 1) % BST_CACHE;
    return v;
}

// Scales the stats to a new total, keeping their proportions (Shedinja keeps 1 HP).
static void ScaleToTotal(u8 *v, u32 target, u32 fixedMask)
{
    u32 i, total = 0, freeSum, given = 0, fixedSum = 0;
    for (i = 0; i < NUM_ST; i++)
    {
        total += v[i];
        if (fixedMask & (1u << i))
            fixedSum += v[i];
    }
    if (total == 0 || target == total)
        return;
    freeSum = total - fixedSum;
    target = target > fixedSum ? target - fixedSum : 0;
    for (i = 0; i < NUM_ST; i++)
    {
        u32 nv;
        if (fixedMask & (1u << i))
            continue;
        nv = freeSum ? v[i] * target / freeSum : 0;
        nv = nv < 1 ? 1 : nv > 255 ? 255 : nv;
        given += nv;
        v[i] = nv;
    }
    for (i = 0; given < target && i < NUM_ST * 64; i++)
    {
        u32 s = i % NUM_ST;
        if ((fixedMask & (1u << s)) || v[s] == 255)
            continue;
        v[s]++;
        given++;
    }
}

static void ComputeStats(u32 sp, u8 *out, u32 depth)
{
    u8 base[NUM_ST];
    u32 key, i, fixed = 0;
    VanillaStats(sp, base);
    if (base[ST_HP] == 1)
        fixed = 1u << ST_HP;                                 // Shedinja keeps 1 HP
    if (S->bstMode != 0)
        ScaleToTotal(base, RH_SpeciesBST(sp), fixed);
    memcpy(out, base, NUM_ST);
    if (S->baseStats == 0)
        return;
    key = FormBaseEx(sp, S->statsFollowMegas);
    if (S->baseStatsFollowEvos)
        key = FollowRoot(key);
    if (S->baseStats == 1)
    {
        u8 perm[NUM_ST] = { 0, 1, 2, 3, 4, 5 };
        for (i = NUM_ST - 1; i > 0; i--)
        {
            u32 j = RH_Hash(SALT_STAT_PERM, key, i) % (i + 1);
            u8 t = perm[i];
            perm[i] = perm[j];
            perm[j] = t;
        }
        for (i = 0; i < NUM_ST; i++)
            out[i] = base[perm[i]];
        if (fixed)
        {
            for (i = 0; i < NUM_ST; i++)
                if (perm[i] == ST_HP)
                    out[i] = base[perm[ST_HP]];
            out[ST_HP] = 1;
        }
        return;
    }
    // Random: keep the base stat total.
    if (S->baseStatsFollowEvos && S->baseStatsRandomAdded && depth < 3 && RH_PreEvo(sp) != 0)
    {
        u8 pre[NUM_ST], preBase[NUM_ST];
        u32 preEvo = RH_PreEvo(sp);
        s32 diff;
        ComputeStats(preEvo, pre, depth + 1);
        VanillaStats(preEvo, preBase);
        if (S->bstMode != 0)
            ScaleToTotal(preBase, RH_SpeciesBST(preEvo), preBase[ST_HP] == 1 ? 1u << ST_HP : 0);
        diff = (s32)Total(base) - (s32)Total(preBase);
        if (diff > 0)
        {
            if (fixed)
                pre[ST_HP] = 1;
            Distribute(diff, sp, 1, out, pre, fixed);
            if (fixed)
                out[ST_HP] = 1;
            return;
        }
    }
    {
        // FVX: HP at least 20, other stats at least 10, the rest of the total split randomly.
        u8 floor[NUM_ST] = { 20, 10, 10, 10, 10, 10 };
        u32 reserved = 0, total = Total(base);
        if (fixed)
            floor[ST_HP] = 1;
        for (i = 0; i < NUM_ST; i++)
            reserved += floor[i];
        Distribute(total > reserved ? total - reserved : 0, key, 0, out, floor, fixed);
        if (fixed)
            out[ST_HP] = 1;
    }
}

// ---------------------------------------------------------------------------
// EXP curves
// ---------------------------------------------------------------------------
static const u8 sCurveOfSetting[] = { 0, GROWTH_MEDIUM_FAST, GROWTH_MEDIUM_SLOW, GROWTH_FAST, GROWTH_SLOW, GROWTH_ERRATIC, GROWTH_FLUCTUATING };

static u32 FinalStage(u32 sp)
{
    u32 g;
    for (g = 0; g < 3; g++)
    {
        u32 n = FirstVanillaEvolution(sp);
        if (n == 0)
            break;
        sp = n;
    }
    return sp;
}

static u32 GrowthRate(u32 sp, u32 vanilla)
{
    if (S->expCurve == 0 || S->expCurve >= sizeof(sCurveOfSetting))
        return vanilla;
    switch (S->expCurveWho)
    {
    case 0:
        if (RH_IsLegendary(sp))
            return GROWTH_SLOW;
        break;
    case 1:
        sp = FormBase(sp);                                   // forms keep their base form's curve
        if (RH_IsLegendary(sp) && RH_VanillaBST(FinalStage(sp)) > 600)
            return GROWTH_SLOW;                              // judged by the final stage (Cosmog -> Solgaleo)
        break;
    }
    return sCurveOfSetting[S->expCurve];
}

// ---------------------------------------------------------------------------
// Abilities
// ---------------------------------------------------------------------------
static const u16 sDuplicateGroups[][4] = {
    { ABILITY_INSOMNIA, ABILITY_VITAL_SPIRIT },
    { ABILITY_CLEAR_BODY, ABILITY_WHITE_SMOKE, ABILITY_FULL_METAL_BODY },
    { ABILITY_HUGE_POWER, ABILITY_PURE_POWER },
    { ABILITY_BATTLE_ARMOR, ABILITY_SHELL_ARMOR },
    { ABILITY_CLOUD_NINE, ABILITY_AIR_LOCK },
    { ABILITY_FILTER, ABILITY_SOLID_ROCK, ABILITY_PRISM_ARMOR },
    { ABILITY_ROUGH_SKIN, ABILITY_IRON_BARBS },
    { ABILITY_MOLD_BREAKER, ABILITY_TURBOBLAZE, ABILITY_TERAVOLT },
    { ABILITY_WIMP_OUT, ABILITY_EMERGENCY_EXIT },
    { ABILITY_QUEENLY_MAJESTY, ABILITY_DAZZLING, ABILITY_ARMOR_TAIL },
    { ABILITY_GOOEY, ABILITY_TANGLING_HAIR },
    { ABILITY_RECEIVER, ABILITY_POWER_OF_ALCHEMY },
    { ABILITY_MULTISCALE, ABILITY_SHADOW_SHIELD },
    { ABILITY_PROTEAN, ABILITY_LIBERO },
    { ABILITY_PROPELLER_TAIL, ABILITY_STALWART },
};

#define BIT(arr, i) (((arr)[(i) / 32] >> ((i) % 32)) & 1)

static BOOL IsNonFirstDuplicate(u32 ability)
{
    u32 i, j;
    for (i = 0; i < NELEMS(sDuplicateGroups); i++)
        for (j = 1; j < 4; j++)
            if (sDuplicateGroups[i][j] == ability)
                return TRUE;
    return FALSE;
}

static u32 ExpandDuplicate(u32 ability, u32 hash)
{
    u32 i, n;
    for (i = 0; i < NELEMS(sDuplicateGroups); i++)
    {
        if (sDuplicateGroups[i][0] != ability)
            continue;
        for (n = 1; n < 4 && sDuplicateGroups[i][n] != ABILITY_NONE; n++)
            ;
        return sDuplicateGroups[i][hash % n];
    }
    return ability;
}

static BOOL AbilityAllowed(u32 ability)
{
    if (ability == ABILITY_NONE || ability >= RH_ABILITY_COUNT || !BIT(sRhAbilityImplemented, ability))
        return FALSE;
    switch (ability)
    {
    case ABILITY_WONDER_GUARD:
        return S->allowWonderGuard;
    case ABILITY_ARENA_TRAP: case ABILITY_SHADOW_TAG: case ABILITY_MAGNET_PULL:
        return !S->banTrapAbilities;
    case ABILITY_DEFEATIST: case ABILITY_SLOW_START: case ABILITY_TRUANT: case ABILITY_KLUTZ: case ABILITY_STALL:
        return !S->banNegativeAbilities;
    case ABILITY_MINUS: case ABILITY_PLUS: case ABILITY_ANTICIPATION: case ABILITY_FOREWARN: case ABILITY_FRISK:
    case ABILITY_HONEY_GATHER: case ABILITY_AURA_BREAK: case ABILITY_RECEIVER: case ABILITY_POWER_OF_ALCHEMY:
        return !S->banBadAbilities;
    case ABILITY_FRIEND_GUARD: case ABILITY_HEALER: case ABILITY_TELEPATHY: case ABILITY_SYMBIOSIS: case ABILITY_BATTERY:
        return !S->banBadAbilities || (S->battleStyle == 2 && S->battleStyleDoubles);
    case ABILITY_FORECAST: case ABILITY_FLOWER_GIFT: case ABILITY_ZEN_MODE: case ABILITY_ICE_FACE: case ABILITY_HUNGER_SWITCH:
    case ABILITY_GULP_MISSILE: case ABILITY_COMMANDER: case ABILITY_ZERO_TO_HERO: case ABILITY_POWER_CONSTRUCT:
    case ABILITY_SCHOOLING: case ABILITY_SHIELDS_DOWN: case ABILITY_DISGUISE: case ABILITY_BATTLE_BOND:
    case ABILITY_TERA_SHIFT: case ABILITY_TERA_SHELL: case ABILITY_TERAFORM_ZERO: case ABILITY_MULTITYPE:
    case ABILITY_STANCE_CHANGE: case ABILITY_RKS_SYSTEM: case ABILITY_AS_ONE_GLASTRIER: case ABILITY_AS_ONE_SPECTRIER:
        return FALSE;
    }
    if (BIT(sRhAbilityNoSwap, ability))
        return FALSE;                                        // form / signature abilities
    if (S->combineDuplicateAbilities && IsNonFirstDuplicate(ability))
        return FALSE;
    return TRUE;
}

static u32 RandomAbility(u32 key, u32 slot)
{
    u32 tries;
    for (tries = 0; tries < 64; tries++)
    {
        u32 h = RH_Hash(SALT_ABILITY, key, slot * 64 + tries);
        u32 a = 1 + h % (RH_ABILITY_COUNT - 1);
        if (AbilityAllowed(a))
            return S->combineDuplicateAbilities ? ExpandDuplicate(a, h >> 16) : a;
    }
    return ABILITY_RUN_AWAY;
}

static BOOL NoSwap(u32 ability)
{
    return ability < RH_ABILITY_COUNT && BIT(sRhAbilityNoSwap, ability);
}

static u32 VanillaAbility(u32 sp, u32 slot)
{
    u16 a;
    ReadFromNarcMemberByIdPair(&a, ARC_PERSONAL, sp, slot ? 0x1A : 0x16, 2);
    return a;
}

// slot 0/1 = the two regular abilities (hidden abilities are kept)
static u32 SpeciesAbility(u32 sp, u32 slot, u32 vanilla)
{
    u32 key, s, t, other, a0, first;
    u16 abil[2];
    if (VanillaAbility(sp, 0) == ABILITY_WONDER_GUARD)
        return vanilla;                                      // Shedinja keeps Wonder Guard (FVX)
    if (FormBaseEx(sp, S->abilitiesFollowMegas) != sp)
    {
        // Battle forms keep their form abilities (Zen Mode...); cosmetic forms and (with "Follow Mega
        // Evolutions") Megas copy the base form's new abilities.
        u32 base = FormBase(sp);
        if (NoSwap(VanillaAbility(sp, 0)) || NoSwap(vanilla))
            return vanilla;
        return SpeciesAbility(base, slot, VanillaAbility(base, slot));
    }
    if (vanilla == ABILITY_NONE && !(slot == 1 && S->ensureTwoAbilities))
        return ABILITY_NONE;
    key = S->abilitiesFollowEvos ? FollowRoot(sp) : sp;
    // both slots are rolled together so they always differ from each other
    for (s = 0; s <= slot; s++)
    {
        abil[s] = RandomAbility(key, s);
        for (t = 1; t < 12; t++)
        {
            BOOL clash = FALSE;
            for (other = 0; other < s; other++)
                clash |= (abil[s] == abil[other]);
            if (!clash)
                break;
            abil[s] = RandomAbility(key, s + 3 * t);
        }
    }
    (void)a0;
    (void)first;
    return abil[slot];
}

// ---------------------------------------------------------------------------
// Catch rate ("Set Minimum Catch Rate", FVX levels)
// ---------------------------------------------------------------------------
static u32 CatchRate(u32 sp, u32 vanilla)
{
    static const u8 sNormal[] = { 0, 75, 128, 200, 255, 255 };
    static const u8 sLegend[] = { 0, 37, 64, 100, 255, 255 };
    u32 level, min;
    if (!S->wildCatchRateOn)
        return vanilla;
    level = S->wildCatchRate > 5 ? 5 : S->wildCatchRate;
    min = RH_IsLegendary(sp) ? sLegend[level] : sNormal[level];
    return vanilla > min ? vanilla : min;
}

// ---------------------------------------------------------------------------
// The personal data as the game reads it
// ---------------------------------------------------------------------------
#define PERSONAL_CACHE 8
struct PersonalCache { u16 sp; u8 stats[NUM_ST]; u8 types[2]; u8 growth; u8 pad; u16 abilities[2]; u32 key; };
static struct PersonalCache sPersonalCache[PERSONAL_CACHE];
static u8 sPersonalCacheNext;

void RH_TransformPersonal(u32 sp, u8 *data)
{
    struct RhSettings *s = S;
    struct PersonalCache *c = NULL;
    u32 i, key;
    if (s->magic != RH_SETTINGS_MAGIC || sp == 0 || sp >= RH_InfoCount())
        return;
    // Set Minimum Catch Rate works without the randomizer (as in FVX)
    data[8] = CatchRate(sp, data[8]);
    if (!s->enabled)
        return;
    if (s->baseStats == 0 && s->bstMode == 0 && s->types == 0 && s->abilities == 0 && s->expCurve == 0)
        return;
    key = RH_SettingsHash();
    for (i = 0; i < PERSONAL_CACHE; i++)
        if (sPersonalCache[i].sp == sp && sPersonalCache[i].key == key)
            c = &sPersonalCache[i];
    if (c == NULL)
    {
        c = &sPersonalCache[sPersonalCacheNext];
        sPersonalCacheNext = (sPersonalCacheNext + 1) % PERSONAL_CACHE;
        c->sp = 0;                                       // invalid while being rebuilt
        if (s->baseStats != 0 || s->bstMode != 0)
            ComputeStats(sp, c->stats, 0);
        else
            memcpy(c->stats, data, NUM_ST);
        c->types[0] = RH_SpeciesType(sp, 0);
        c->types[1] = RH_SpeciesType(sp, 1);
        c->growth = GrowthRate(sp, data[0x13]);
        if (s->abilities != 0)
        {
            c->abilities[0] = SpeciesAbility(sp, 0, *(u16 *)(data + 0x16));
            c->abilities[1] = SpeciesAbility(sp, 1, *(u16 *)(data + 0x1A));
        }
        else
        {
            c->abilities[0] = *(u16 *)(data + 0x16);
            c->abilities[1] = *(u16 *)(data + 0x1A);
        }
        c->key = key;
        c->sp = sp;
    }
    memcpy(data, c->stats, NUM_ST);
    data[6] = c->types[0];
    data[7] = c->types[1];
    data[0x13] = c->growth;
    *(u16 *)(data + 0x16) = c->abilities[0];
    *(u16 *)(data + 0x1A) = c->abilities[1];
}
