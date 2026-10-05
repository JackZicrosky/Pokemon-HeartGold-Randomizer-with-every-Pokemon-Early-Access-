// Romhack randomizer core: picking species with filters (port of the FireRed/Emerald rh_core.c pickers).
#include "../../include/types.h"
#include "../../include/save.h"
#include "../../include/constants/species.h"
#include "../../include/rh/rh_internal.h"

u32 RH_Permute(u32 salt, u32 x, u32 n)
{
    u32 bits = 1, half, mask, i, l, r;
    if (n <= 1)
        return 0;
    while ((1u << bits) < n)
        bits++;
    if (bits & 1)
        bits++;
    half = bits / 2;
    mask = (1u << half) - 1;
    do
    {
        l = x >> half;
        r = x & mask;
        for (i = 0; i < 4; i++)
        {
            u32 t = l ^ (RH_Hash(salt, r, i) & mask);
            l = r;
            r = t;
        }
        x = (l << half) | r;
    } while (x >= n);
    return x;
}

s32 RH_ApplyPercent(s32 value, s16 percent)
{
    s32 v = (value * (100 + percent) + 50) / 100;          // rounded (FVX)
    return v < 1 ? 1 : v;
}

void RH_FilterInit(struct RhFilter *f)
{
    memset(f, 0, sizeof(*f));
    f->type = RH_TYPE_NONE;
    f->primaryType = RH_TYPE_NONE;
}

void RH_FilterExclude(struct RhFilter *f, u32 sp)
{
    if (sp != 0 && f->excludeCount < RH_MAX_EXCLUDE)
        f->exclude[f->excludeCount++] = sp;
}

static BOOL FilterOk(u32 sp, const struct RhFilter *f)
{
    struct RhSpeciesInfo m;
    u32 gen, i, bst;
    if (!RH_GetInfo(sp, &m) || !(m.flags & RH_SPF_POOL) || (m.flags2 & RH_SPF2_VIVILLON))
        return FALSE;
    gen = RH_INFO_GEN(&m);
    if (m.flags & RH_SPF_MEGA)
    {
        if (!f->allowMegas)
            return FALSE;
        if (!f->allowVariants && S->speciesPool < RH_POOL_ALL && gen > (S->speciesPool == RH_POOL_GEN1 ? 1 : 3))
            return FALSE;
    }
    else if (f->allowMegas && f->megasOnly)
    {
        return FALSE;
    }
    else if (f->allowVariants && (m.flags & RH_SPF_VARIANT))
    {
        // regional / alternate forms allowed regardless of the pool setting (still limited by generation)
        if (S->speciesPool == RH_POOL_GEN1 && gen != 1)
            return FALSE;
        if (S->speciesPool == RH_POOL_GEN1_3 && gen > 3)
            return FALSE;
    }
    else if (!RH_PoolAllowed(&m))
    {
        return FALSE;
    }
    if (f->legend == 1 && (m.flags & RH_SPF_LEGENDARY))
        return FALSE;
    if (f->legend == 2 && !(m.flags & RH_SPF_LEGENDARY))
        return FALSE;
    if (f->minBst || f->maxBst)
    {
        bst = (S->enabled && S->bstMode != 0) ? RH_SpeciesBST(sp) : m.bst;   // "Base Stat Totals" changed it
        if (f->minBst && bst < f->minBst)
            return FALSE;
        if (f->maxBst && bst > f->maxBst)
            return FALSE;
    }
    if (f->stage && RH_INFO_STAGE(&m) != f->stage)
        return FALSE;
    if (f->threeStageBasic && !(RH_INFO_STAGE(&m) == 1 && RH_INFO_CHAIN(&m) == 3))
        return FALSE;
    for (i = 0; i < f->excludeCount; i++)
        if (f->exclude[i] == sp)
            return FALSE;
    if (f->monoType && RH_SpeciesType(sp, 0) != RH_SpeciesType(sp, 1))
        return FALSE;
    if (f->primaryType != RH_TYPE_NONE && RH_SpeciesType(sp, 0) != f->primaryType)
        return FALSE;
    if (f->type != RH_TYPE_NONE && !RH_SpeciesHasType(sp, f->type))
        return FALSE;
    if (f->extra != NULL && !f->extra(sp))
        return FALSE;
    return TRUE;
}

BOOL RH_FilterAccepts(const struct RhFilter *f, u32 sp)
{
    return FilterOk(sp, f);
}

// Vivillon's wing patterns count as ONE Pokemon (owner's rule): a pick that lands on Vivillon rolls a pattern
// when forms are in play.
static u32 RollVivillonPattern(u32 sp, const struct RhFilter *f, u32 hash)
{
    if (sp != SPECIES_VIVILLON || !(f->allowVariants || S->speciesPool == RH_POOL_ALL_FORMS))
        return sp;
    return RH_SpeciesIndex(SPECIES_VIVILLON, RH_Hash(hash, sp, 0x717) % 20);
}

// The k-th accepted Pokemon (by scanning).
static u32 AcceptedAt(const struct RhFilter *f, u32 k)
{
    u32 i, n = RH_InfoCount();
    for (i = 1; i < n; i++)
        if (FilterOk(i, f) && k-- == 0)
            return i;
    return 0;
}

static u32 CountAccepted(const struct RhFilter *f)
{
    u32 i, n = RH_InfoCount(), count = 0;
    for (i = 1; i < n; i++)
        if (FilterOk(i, f))
            count++;
    return count;
}

static u32 PickWithFilter(const struct RhFilter *f, u32 hash)
{
    u32 i, count, n = RH_InfoCount();
    // random candidates first: uniform among the accepted ones and far cheaper than scanning everything
    for (i = 0; i < 48; i++)
    {
        u32 k = 1 + RH_Hash(hash, i, 0x5EED) % (n - 1);
        if (FilterOk(k, f))
            return k;
    }
    count = CountAccepted(f);
    if (count == 0)
        return 0;
    return AcceptedAt(f, hash % count);
}

u32 RH_PickWithFilter(const struct RhFilter *f, u32 hash)
{
    return RollVivillonPattern(PickWithFilter(f, hash), f, hash);
}

// The rank-th allowed Pokemon in a keyed order (a bijection on the accepted ones): different ranks give different
// Pokemon until every accepted one has been used. Used by "even distribution" and Catch Em' All.
u32 RH_PickRanked(const struct RhFilter *f, u32 rank, u32 salt)
{
    u32 count = CountAccepted(f);
    if (count == 0)
        return 0;
    return RollVivillonPattern(AcceptedAt(f, RH_Permute(salt, rank % count, count)), f, rank ^ salt);
}

static u8 sRankedPick;
static u32 sRankedSalt;

static u32 PickOrRanked(const struct RhFilter *f, u32 hash)
{
    if (sRankedPick)
        return RH_PickRanked(f, hash, sRankedSalt);
    return RH_PickWithFilter(f, hash);
}

// Picks a species around a base stat total: +-10%, 20%, 35% windows first. When nothing matches, relaxes the
// type, stage, exclusion and legendary rules (in that order) rather than failing.
u32 RH_PickSpeciesNearBst(struct RhFilter *f, u32 hash, u32 bst)
{
    static const u8 sWindows[] = { 10, 20, 35 };
    u32 i, result;
    u16 keepMin = f->minBst, keepMax = f->maxBst;
    if (bst != 0)
    {
        for (i = 0; i < sizeof(sWindows); i++)
        {
            u32 lo = bst * (100 - sWindows[i]) / 100, hi = bst * (100 + sWindows[i]) / 100;
            f->minBst = lo > keepMin ? lo : keepMin;
            f->maxBst = (keepMax && keepMax < hi) ? keepMax : hi;
            result = PickOrRanked(f, hash);
            if (result != 0)
                return result;
        }
        f->minBst = keepMin;
        f->maxBst = keepMax;
    }
    if ((result = PickOrRanked(f, hash)) != 0)
        return result;
    if (f->type != RH_TYPE_NONE || f->primaryType != RH_TYPE_NONE || f->monoType)
    {
        f->type = f->primaryType = RH_TYPE_NONE;
        f->monoType = FALSE;
        if ((result = PickOrRanked(f, hash)) != 0)
            return result;
    }
    if (f->stage || f->threeStageBasic || f->minBst || f->maxBst || f->legalAtLevel)
    {
        f->stage = 0;
        f->threeStageBasic = FALSE;
        f->minBst = f->maxBst = 0;
        f->legalAtLevel = 0;
        if ((result = PickOrRanked(f, hash)) != 0)
            return result;
    }
    f->excludeCount = 0;
    f->extra = NULL;
    f->megasOnly = FALSE;
    if ((result = PickOrRanked(f, hash)) != 0)
        return result;
    f->legend = 0;
    return PickOrRanked(f, hash);
}

// Like RH_PickSpeciesNearBst, but different ranks with the same filter and salt never give the same Pokemon.
u32 RH_PickSpeciesNearBstRanked(struct RhFilter *f, u32 rank, u32 salt, u32 bst)
{
    u32 result;
    sRankedPick = TRUE;
    sRankedSalt = salt;
    result = RH_PickSpeciesNearBst(f, rank, bst);
    sRankedPick = FALSE;
    return result;
}
