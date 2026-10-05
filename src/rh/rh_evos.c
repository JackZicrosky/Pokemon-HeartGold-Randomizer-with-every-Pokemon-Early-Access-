// Romhack randomizer core: Pokemon Evolutions (port of the FireRed/Emerald rh_traits.c evolution part): random /
// random every level evolutions, the global No Convergence table, and the FVX rules that rewrite evolution methods
// (Change Impossible Evos, Make Evolutions Easier, Remove Time-Based Evos, Adjust Evolution Levels). Applied to the
// evolution data as the game reads it (RH_TransformEvolutions, called from the data loader hook).
#include "../../include/types.h"
#include "../../include/save.h"
#include "../../include/pokemon.h"
#include "../../include/constants/file.h"
#include "../../include/constants/item.h"
#include "../../include/constants/species.h"
#include "../../include/rh/rh_internal.h"

#define EVOS MAX_EVOS_PER_POKE                     // entries per species: method, param, target
#define TARGET_SPECIES(t) ((t) & 0x7FF)
#define TARGET_FORM(t) ((t) >> 11)

static BOOL EvosActive(void)
{
    return S->enabled && (S->evolutions || S->evoChangeImpossible || S->evoMakeEasier || S->evoRemoveTimeBased || S->evoAdjustLevels);
}

static u32 KeyEvos(void)
{
    return RH_SettingsHash() + 0x3E;
}

static void ReadVanilla(u32 sp, u16 *e)
{
    ReadFromNarcMemberByIdPair(e, ARC_EVOLUTIONS, sp, 0, EVOS * 6);
}

static u32 TargetIndex(u32 t)
{
    return TARGET_FORM(t) ? RH_SpeciesIndex(TARGET_SPECIES(t), TARGET_FORM(t)) : t;
}

static u32 EncodeTarget(u32 sp)
{
    u16 species, form;
    RH_SpeciesKey(sp, &species, &form);
    return species | (form << 11);
}

static u32 Remaining(u32 sp)
{
    u32 st = RH_SpeciesStage(sp), ch = RH_SpeciesChain(sp);
    return ch > st ? ch - st : 0;
}

// Evolutions only ever go "down" this ranking, so random evolutions can never loop (FVX never creates cycles).
static u32 EvoRank(u32 sp)
{
    return (Remaining(sp) << 28) | ((RH_Hash(SALT_EVO_RANK, sp, 0) & 0xFFFF) << 12) | (sp & 0xFFF);
}

// ---------------------------------------------------------------------------
// Target rules
// ---------------------------------------------------------------------------
static u16 sEvoSource;
static u8 sEvoGrowth;
static u32 sEvoSourceRank;
static u16 sEvoMinBst;
static s8 sEvoRemaining;
static u16 sVanillaTargets[EVOS];
static u8 sVanillaTargetCount;

static BOOL EvoTargetOk(u32 sp)
{
    u32 i;
    if (RH_GrowthRate(sp) != sEvoGrowth)
        return FALSE;                                        // FVX: evolutions keep the EXP curve
    if (S->evoSameTyping && !RH_SpeciesHasType(sp, RH_SpeciesType(sEvoSource, 0)) && !RH_SpeciesHasType(sp, RH_SpeciesType(sEvoSource, 1)))
        return FALSE;
    if (sEvoMinBst && RH_SpeciesBST(sp) < sEvoMinBst)
        return FALSE;
    if (sEvoRemaining >= 0 && (s32)Remaining(sp) != sEvoRemaining)
        return FALSE;
    if (S->evolutions == 1 && EvoRank(sp) >= sEvoSourceRank)
        return FALSE;
    if (S->evoForceChange)
        for (i = 0; i < sVanillaTargetCount; i++)
            if (sVanillaTargets[i] == sp)
                return FALSE;
    return TRUE;
}

static void SetEvoContext(u32 sp, u32 original, const u16 *vanilla)
{
    u32 i;
    sEvoSource = sp;
    sEvoGrowth = RH_GrowthRate(sp);
    sEvoSourceRank = EvoRank(sp);
    // Similar Strength / Limit to Three Stages / Force Growth only apply to "Random" (not "Every Level"), as in FVX
    sEvoMinBst = (S->evoForceGrowth && S->evolutions == 1) ? RH_SpeciesBST(sp) + 1 : 0;
    sEvoRemaining = (S->evoLimitThreeStages && S->evolutions == 1 && original != 0) ? (s8)Remaining(original) : -1;
    sVanillaTargetCount = 0;
    for (i = 0; vanilla != NULL && i < EVOS; i++)
        if (vanilla[i * 3] != EVO_NONE && vanilla[i * 3 + 2] != 0)
            sVanillaTargets[sVanillaTargetCount++] = TargetIndex(vanilla[i * 3 + 2]);
}

static BOOL InPool(u32 sp)
{
    struct RhSpeciesInfo i;
    return RH_GetInfo(sp, &i) && RH_PoolAllowed(&i);
}

// A form outside the pool (Alolan Raichu...) whose sibling entry is in it: that entry is dropped.
static BOOL HasPoolSibling(const u16 *v, u32 target)
{
    u32 i;
    for (i = 0; i < EVOS; i++)
        if (v[i * 3] != EVO_NONE && v[i * 3 + 2] != 0 && TargetIndex(v[i * 3 + 2]) != target && InPool(TargetIndex(v[i * 3 + 2])))
            return TRUE;
    return FALSE;
}

// ---------------------------------------------------------------------------
// Global No Convergence table (like FVX, which picks from the targets not used yet): every evolution of the allowed
// Pokemon gets its target once per settings, in species order, never a target that was already taken.
// "Random": pairs (source, entry << 11 | target); "Every Level": one target per personal index.
// ---------------------------------------------------------------------------
#define NC_WORDS 2048
static u32 sNoConvKey;
static u16 sNoConvCount;                   // pairs ("Random")
static u16 sNoConv[NC_WORDS];

struct NcMon { u16 sp; u16 bst; u8 growth; u8 rem; u8 t1, t2; };
struct NcCtx
{
    struct NcMon *mon;
    u16 *order;                            // pool indexes grouped by (growth, stages left), shuffled in each group
    u16 start[6 * 4 + 1];
    u16 len[6 * 4];                        // Pokemon left in each group (taken targets are swapped out)
    u8 *used;                              // targets already taken (bitset by personal index)
    u32 n;
};

#define NC_GROUP(g, r) ((g) * 4 + (r))
#define NC_USED(u, s) ((u)[(s) >> 3] & (1 << ((s) & 7)))

static u32 PickNoConv(struct NcCtx *x, u32 sp, u32 original, const u16 *vanilla)
{
    static const u8 sWindows[] = { 10, 20, 35, 0, 0 };
    u32 growth = RH_GrowthRate(sp), st1 = RH_SpeciesType(sp, 0), st2 = RH_SpeciesType(sp, 1);
    u32 srcBst = RH_SpeciesBST(sp), pass, k, g, remLo, remHi, i;
    u32 bst = (S->evoSimilarStrength && S->evolutions == 1 && original != 0) ? RH_SpeciesBST(original) : 0;
    if (growth >= 6)
        return 0;
    if (S->evolutions == 1)
        remLo = remHi = Remaining(original) < 3 ? Remaining(original) : 3;   // chains keep their length: no loops
    else
        remLo = 0, remHi = 3;
    for (pass = 0; pass < 5; pass++)
    {
        u32 window = sWindows[pass];
        BOOL relaxed = (pass == 4);                          // last resort: no Same Typing / Force Growth
        if (pass < 3 && bst == 0)
            continue;
        for (g = NC_GROUP(growth, remLo); g <= NC_GROUP(growth, remHi); g++)
        {
            u32 lo = x->start[g], cnt = x->len[g], off;
            if (cnt == 0)
                continue;
            off = RH_Hash(SALT_EVO, sp, original + pass) % cnt;
            for (k = 0; k < cnt; k++)
            {
                u32 pos = lo + (off + k) % cnt;
                const struct NcMon *m = &x->mon[x->order[pos]];
                BOOL vanillaTarget = FALSE;
                if (m->sp == sp || NC_USED(x->used, m->sp))
                    continue;
                if (window && (m->bst * 100 < bst * (100 - window) || m->bst * 100 > bst * (100 + window)))
                    continue;
                if (!relaxed && S->evoSameTyping && m->t1 != st1 && m->t1 != st2 && m->t2 != st1 && m->t2 != st2)
                    continue;
                if (!relaxed && S->evoForceGrowth && S->evolutions == 1 && m->bst <= srcBst)
                    continue;
                for (i = 0; S->evoForceChange && vanilla != NULL && i < EVOS; i++)
                    vanillaTarget |= vanilla[i * 3] != EVO_NONE && TargetIndex(vanilla[i * 3 + 2]) == m->sp;
                if (vanillaTarget)
                    continue;
                {
                    // taken: swap it out of its group so later searches don't walk over it
                    u16 taken = m->sp, tmp = x->order[lo + cnt - 1];
                    x->order[lo + cnt - 1] = x->order[pos];
                    x->order[pos] = tmp;
                    x->len[g]--;
                    return taken;
                }
            }
        }
    }
    return 0;
}

static void BuildNoConv(void)
{
    struct NcCtx x;
    u32 p, k, j, g, n = RH_InfoCount(), heap;
    u16 count[6 * 4];
    sNoConvKey = KeyEvos();
    sNoConvCount = 0;
    memset(sNoConv, 0, sizeof(sNoConv));
    memset(count, 0, sizeof(count));
    RH_BeginSession(0xFFFF);
    heap = RH_SessionHeap();
    x.mon = sys_AllocMemory(heap, n * sizeof(struct NcMon));
    x.order = sys_AllocMemory(heap, n * sizeof(u16));
    x.used = sys_AllocMemory(heap, (n + 7) / 8);
    if (x.mon == NULL || x.order == NULL || x.used == NULL)
        goto done;
    memset(x.used, 0, (n + 7) / 8);
    x.n = 0;
    for (p = 1; p < n; p++)
    {
        struct NcMon *m = &x.mon[x.n];
        if (!InPool(p))
            continue;
        m->sp = p;
        m->bst = RH_SpeciesBST(p);
        m->growth = RH_GrowthRate(p);
        m->rem = Remaining(p) < 3 ? Remaining(p) : 3;
        m->t1 = RH_SpeciesType(p, 0);
        m->t2 = RH_SpeciesType(p, 1);
        if (m->growth < 6)
            count[NC_GROUP(m->growth, m->rem)]++;
        x.n++;
    }
    x.start[0] = 0;
    for (g = 0; g < 6 * 4; g++)
    {
        x.start[g + 1] = x.start[g] + count[g];
        x.len[g] = count[g];
    }
    memset(count, 0, sizeof(count));
    for (p = 0; p < x.n; p++)
        if (x.mon[p].growth < 6)
        {
            g = NC_GROUP(x.mon[p].growth, x.mon[p].rem);
            x.order[x.start[g] + count[g]++] = p;
        }
    for (g = 0; g < 6 * 4; g++)                              // shuffle each group (keyed)
    {
        u32 lo = x.start[g], cnt = x.start[g + 1] - lo;
        for (k = cnt; k > 1; k--)
        {
            u32 r = RH_Hash(SALT_EVO + 0x77, g, k) % k;
            u16 t = x.order[lo + k - 1];
            x.order[lo + k - 1] = x.order[lo + r];
            x.order[lo + r] = t;
        }
    }
    for (p = 0; p < x.n; p++)
    {
        u32 sp = x.mon[p].sp, t;
        u16 v[EVOS * 3 + 1];
        if (S->evolutions == 2)
        {
            t = PickNoConv(&x, sp, 0, NULL);
            if (t != 0)
                x.used[t >> 3] |= 1 << (t & 7);
            sNoConv[sp] = t;
            continue;
        }
        ReadVanilla(sp, v);
        for (k = 0; k < EVOS && sNoConvCount < NC_WORDS / 2; k++)
        {
            BOOL shared = FALSE;
            u32 target = TargetIndex(v[k * 3 + 2]);
            if (v[k * 3] == EVO_NONE || v[k * 3 + 2] == 0 || (!InPool(target) && HasPoolSibling(v, target)))
                continue;
            for (j = 0; j < k; j++)
                shared |= v[j * 3] != EVO_NONE && TargetIndex(v[j * 3 + 2]) == target;
            if (shared)
                continue;                                    // entries to the same Pokemon share one target
            t = PickNoConv(&x, sp, target, v);
            if (t != 0)
                x.used[t >> 3] |= 1 << (t & 7);
            sNoConv[sNoConvCount * 2] = sp;
            sNoConv[sNoConvCount * 2 + 1] = (k << 11) | t;
            sNoConvCount++;
        }
    }
done:
    if (x.mon != NULL)
        sys_FreeMemoryEz(x.mon);
    if (x.order != NULL)
        sys_FreeMemoryEz(x.order);
    if (x.used != NULL)
        sys_FreeMemoryEz(x.used);
    RH_EndSession();
}

// TRUE if the table has the entry; *target = 0 when no free target fitted.
static BOOL NoConvLookup(u32 sp, u32 entry, u32 *target)
{
    s32 lo = 0, hi, mid;
    if (sNoConvKey != KeyEvos())
        BuildNoConv();
    if (S->evolutions == 2)
    {
        *target = sp < NC_WORDS ? sNoConv[sp] : 0;
        return TRUE;
    }
    hi = sNoConvCount - 1;
    while (lo <= hi)
    {
        mid = (lo + hi) / 2;
        if (sNoConv[mid * 2] == sp)
        {
            while (mid > 0 && sNoConv[(mid - 1) * 2] == sp)
                mid--;
            for (; mid < sNoConvCount && sNoConv[mid * 2] == sp; mid++)
                if ((sNoConv[mid * 2 + 1] >> 11) == entry)
                {
                    *target = sNoConv[mid * 2 + 1] & 0x7FF;
                    return TRUE;
                }
            return FALSE;
        }
        if (sNoConv[mid * 2] < sp)
            lo = mid + 1;
        else
            hi = mid - 1;
    }
    return FALSE;
}

static u32 RandomEvoTarget(u32 sp, u32 original, u32 n, const u16 *chosen, u32 chosenCount, const u16 *vanilla)
{
    struct RhFilter f;
    u32 i, t;
    if (S->evoNoConvergence)
    {
        if (NoConvLookup(sp, n, &t) && t != 0)
            return t;
    }
    SetEvoContext(sp, original, vanilla);
    RH_FilterInit(&f);
    f.extra = EvoTargetOk;
    RH_FilterExclude(&f, sp);
    for (i = 0; i < chosenCount; i++)
        RH_FilterExclude(&f, chosen[i]);
    t = RH_PickSpeciesNearBst(&f, RH_Hash(SALT_EVO, sp, n),
                              (S->evoSimilarStrength && S->evolutions == 1 && original) ? RH_SpeciesBST(original) : 0);
    // the picker relaxes its rules when nothing fits; never accept a pick that could create a loop
    if (t == sp || (t != 0 && !EvoTargetOk(t)))
        return 0;
    return t;
}

// ---------------------------------------------------------------------------
// Method rules (FVX): Change Impossible Evos, Make Evolutions Easier, Remove Time-Based Evos, Adjust Evo Levels
// ---------------------------------------------------------------------------
static u16 sAdjustFrom;

static u32 ImpossibleLevel(u32 target)
{
    return S->evoEstimatedLevels ? RH_EstimateEvoLevel(sAdjustFrom, target) : 37;
}

// Methods that can't happen in this game (no trading partner, no Mt. Coronet / Moss / Ice Rock, Sword/Shield and
// Let's Go mechanics...). Knowing a move is impossible when movesets are random (it may never be learned).
static BOOL MethodImpossible(u32 method)
{
    switch (method)
    {
    case EVO_TRADE: case EVO_TRADE_ITEM: case EVO_TRADE_SPECIFIC_MON: case EVO_BEAUTY:
    case EVO_MAGNETIC_FIELD: case EVO_MOSSY_ROCK: case EVO_ICY_ROCK:
    case EVO_SPIN_CLOCKWISE_LESS_THAN_5_SECONDS_DAY: case EVO_SPIN_COUNTERCLOCKWISE_LESS_THAN_5_SECONDS_DAY:
    case EVO_SPIN_CLOCKWISE_LESS_THAN_5_SECONDS_NIGHT: case EVO_SPIN_COUNTERCLOCKWISE_MORE_THAN_5_SECONDS_NIGHT:
    case EVO_SPIN_CLOCKWISE_MORE_THAN_5_SECONDS_NIGHT: case EVO_SPIN_COUNTERCLOCKWISE_LESS_THAN_5_SECONDS_NIGHT:
    case EVO_SPIN_COUNTERCLOCKWISE_MORE_THAN_5_SECONDS_DAY: case EVO_SPIN_CLOCKWISE_MORE_THAN_5_SECONDS_DAY:
    case EVO_SPIN_MORE_THAN_10_SECONDS_EVENING: case EVO_FORM_ARGUMENT: case EVO_LETS_GO: case EVO_DUMMY:
        return TRUE;
    case EVO_HAS_MOVE:
        return S->movesets != 0;
    }
    return FALSE;
}

enum { TIME_NONE, TIME_DAY, TIME_NIGHT, TIME_DUSK };

static u32 TimeKind(u32 method)
{
    switch (method)
    {
    case EVO_FRIENDSHIP_DAY: case EVO_LEVEL_DAY:     return TIME_DAY;
    case EVO_FRIENDSHIP_NIGHT: case EVO_LEVEL_NIGHT: return TIME_NIGHT;
    case EVO_LEVEL_DUSK:                             return TIME_DUSK;
    }
    return TIME_NONE;
}

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

// Rewrites entry k (e = method, param, target) of a species whose vanilla list is v.
static void AdjustEvolution(u16 *e, const u16 *v, u32 k)
{
    u32 i, method = v[k * 3], tk = TimeKind(method), target = TargetIndex(e[2]);
    if (e[0] == EVO_NONE || e[2] == 0)
        return;
    if (S->evoChangeImpossible && MethodImpossible(method))
    {
        // another, possible entry already leads to the same Pokemon (Magnezone's Thunder Stone...): drop this one
        for (i = 0; i < EVOS; i++)
            if (i != k && v[i * 3] != EVO_NONE && v[i * 3 + 2] == v[k * 3 + 2] && !MethodImpossible(v[i * 3]))
            {
                e[0] = EVO_NONE;
                return;
            }
        if (method == EVO_MAGNETIC_FIELD)
            e[0] = EVO_STONE, e[1] = ITEM_THUNDER_STONE;     // FVX: Thunder Stone
        else
            e[0] = EVO_LEVEL, e[1] = ImpossibleLevel(target);
    }
    // Time-based: dusk -> Dusk Stone; a day/night pair (Espeon/Umbreon...) -> Sun/Moon Stone; otherwise the time
    // condition is simply dropped.
    if (S->evoRemoveTimeBased && tk != TIME_NONE)
    {
        BOOL pair = FALSE;
        for (i = 0; i < EVOS; i++)
        {
            u32 other = TimeKind(v[i * 3]);
            if (i != k && v[i * 3 + 2] != v[k * 3 + 2] && ((tk == TIME_DAY && other == TIME_NIGHT) || (tk == TIME_NIGHT && other == TIME_DAY)))
                pair = TRUE;
        }
        if (tk == TIME_DUSK)
            e[0] = EVO_STONE, e[1] = ITEM_DUSK_STONE;
        else if (pair)
            e[0] = EVO_STONE, e[1] = (tk == TIME_DAY) ? ITEM_SUN_STONE : ITEM_MOON_STONE;
        else if (method == EVO_FRIENDSHIP_DAY || method == EVO_FRIENDSHIP_NIGHT)
            e[0] = EVO_FRIENDSHIP;
        else
            e[0] = EVO_LEVEL;
    }
    if (S->evoMakeEasier && method == EVO_OTHER_PARTY_MON)
        e[0] = EVO_LEVEL, e[1] = ImpossibleLevel(target);    // Mantyke: needs a Remoraid in the party
    // "Adjust Evolution Levels" (FVX): level-up evolutions happen at a level that fits the two Pokemon
    if (S->evoAdjustLevels && IsLevelMethod(e[0]) && e[1] > 1)
        e[1] = RH_EstimateEvoLevel(sAdjustFrom, target);
    if (S->evoMakeEasier && S->evoMakeEasier < 55 && IsLevelMethod(e[0]))
    {
        u32 cap = S->evoMakeEasier;
        if (Remaining(target) >= 1)
            cap = cap * 3 / 4;                               // three-stage lines reach the middle stage by 75%
        if (e[1] > cap)
            e[1] = cap;
    }
}

// ---------------------------------------------------------------------------
// The data hook
// ---------------------------------------------------------------------------
#define EVO_CACHE 6
struct EvoCache { u16 sp; u32 key; u16 e[EVOS * 3]; };
static struct EvoCache sEvoCache[EVO_CACHE];
static u8 sEvoCacheNext;

static void Build(u32 sp, u16 *out, const u16 *v)
{
    u16 chosen[EVOS];
    u32 n, j;
    memcpy(out, (void *)v, EVOS * 6);
    if (S->evolutions == 2)
    {
        // Random Every Level: one level-1 evolution into a random Pokemon
        u32 t = RandomEvoTarget(sp, 0, 0, NULL, 0, NULL);
        memset(out, 0, EVOS * 6);
        if (t != 0)
            out[0] = EVO_LEVEL, out[1] = 1, out[2] = EncodeTarget(t);
        return;
    }
    sAdjustFrom = sp;
    for (n = 0; n < EVOS; n++)
    {
        u16 *e = out + n * 3;
        u32 vt = TargetIndex(v[n * 3 + 2]);
        chosen[n] = 0;
        if (v[n * 3] == EVO_NONE || v[n * 3 + 2] == 0)
            continue;
        if (S->evolutions == 1)
        {
            u32 target = 0;
            if (!InPool(vt) && HasPoolSibling(v, vt))
            {
                e[0] = EVO_NONE;                             // a form outside the pool: its sibling's entry stays
                continue;
            }
            // entries that lead to the same Pokemon (Magneton...) keep sharing one target
            for (j = 0; j < n; j++)
                if (TargetIndex(v[j * 3 + 2]) == vt && chosen[j] != 0)
                    target = chosen[j];
            if (target == 0)
                target = RandomEvoTarget(sp, vt, n, chosen, n, v);
            if (target != 0)
                e[2] = EncodeTarget(target);
            else if (EvoRank(vt) >= EvoRank(sp))
                e[0] = EVO_NONE;                             // keeping the original could create a loop
            chosen[n] = TargetIndex(e[2]);
        }
        AdjustEvolution(e, v, n);
    }
    // Shedinja: the extra Pokemon of Nincada's evolution follows Ninjask's new target
    for (n = 0; n < EVOS; n++)
        if (v[n * 3] == EVO_LEVEL_SHEDINJA && S->evolutions == 1)
            out[n * 3] = EVO_NONE;
}

void RH_TransformEvolutions(u32 sp, u16 *data)
{
    u32 i, key;
    struct EvoCache *c;
    if (!EvosActive() || sp == 0 || sp >= RH_InfoCount())
        return;
    if (S->evolutions != 2 && data[0] == EVO_NONE && data[3] == EVO_NONE)
    {
        for (i = 0; i < EVOS && data[i * 3] == EVO_NONE; i++)
            ;
        if (i == EVOS)
            return;                                          // no evolutions to change
    }
    if (S->evolutions == 2 && !InPool(sp))
        return;
    key = KeyEvos();
    for (i = 0; i < EVO_CACHE; i++)
        if (sEvoCache[i].sp == sp && sEvoCache[i].key == key)
        {
            memcpy(data, sEvoCache[i].e, EVOS * 6);
            return;
        }
    c = &sEvoCache[sEvoCacheNext];
    sEvoCacheNext = (sEvoCacheNext + 1) % EVO_CACHE;
    c->sp = 0;
    {
        u16 v[EVOS * 3 + 1];
        memcpy(v, data, EVOS * 6);                           // the data as read = vanilla
        RH_BeginSession(0xFFFF);
        Build(sp, c->e, v);
        RH_EndSession();
    }
    c->key = key;
    c->sp = sp;
    memcpy(data, c->e, EVOS * 6);
}
