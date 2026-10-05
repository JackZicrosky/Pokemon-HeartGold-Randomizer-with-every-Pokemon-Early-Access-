// Romhack randomizer core: Moves & Movesets (port of the FireRed/Emerald rh_moves.c): move data (power, accuracy,
// PP, type, category), level-up movesets, egg moves. The move pool is a028 member 23 (tools/rh/gen_rh_moves.py).
#include "../../include/types.h"
#include "../../include/battle.h"
#include "../../include/pokemon.h"
#include "../../include/save.h"
#include "../../include/constants/file.h"
#include "../../include/constants/moves.h"
#include "../../include/constants/species.h"
#include "../../include/rh/rh_internal.h"

#define SPLIT_PHYS 0
#define SPLIT_SPEC 1
#define SPLIT_STATUS 2

// ---------------------------------------------------------------------------
// The move pool
// ---------------------------------------------------------------------------
// The pool and the per-settings move cache live in overlay 129's resident RAM (ITCM is too small)
struct MoveTables
{
    u16 move[RH_MOVE_MAX];
    u8 flags[RH_MOVE_MAX];
    u8 type[RH_MOVE_MAX];                  // current (randomized) type
    u8 cflags[RH_MOVE_MAX];                // MF_* for the current data
};
_Static_assert(sizeof(struct MoveTables) <= RH_RESIDENT_SIZE, "resident RAM too small");
#define T ((struct MoveTables *)RH_Resident())
#define sPoolMove (T->move)
#define sPoolFlags (T->flags)
#define sCacheType (T->type)
#define sCacheFlags (T->cflags)

static u16 sPoolCount;

static void EnsurePool(void)
{
    struct RhMove m[16];
    u32 i, j;
    u16 hdr[2];
    if (sPoolCount != 0)
        return;
    ReadFromNarcMemberByIdPair(hdr, ARC_CODE_ADDONS, CODE_ADDON_RH_MOVES, 0, 4);
    if (hdr[0] > RH_MOVE_MAX)
        hdr[0] = RH_MOVE_MAX;
    for (i = 0; i < hdr[0]; i += 16)
    {
        u32 n = hdr[0] - i < 16 ? hdr[0] - i : 16;
        ReadFromNarcMemberByIdPair(m, ARC_CODE_ADDONS, CODE_ADDON_RH_MOVES, 4 + i * sizeof(struct RhMove), n * sizeof(struct RhMove));
        for (j = 0; j < n; j++)
        {
            sPoolMove[i + j] = m[j].move;
            sPoolFlags[i + j] = m[j].flags;
        }
    }
    sPoolCount = hdr[0];
}

static s32 PoolIndexOf(u32 move)
{
    s32 lo = 0, hi;
    EnsurePool();
    hi = sPoolCount - 1;
    while (lo <= hi)
    {
        s32 mid = (lo + hi) / 2;
        if (sPoolMove[mid] == move)
            return mid;
        if (sPoolMove[mid] < move)
            lo = mid + 1;
        else
            hi = mid - 1;
    }
    return -1;
}

static u32 HitCount(u32 move)
{
    s32 i = PoolIndexOf(move);
    u32 h = i >= 0 ? RHM_HITS(sPoolFlags[i]) : 1;
    return h ? h : 1;
}

// ---------------------------------------------------------------------------
// Move data (FVX rules); applied to every move data read
// ---------------------------------------------------------------------------
// FVX: 2/3 "regular" 50-100, 1/3 "extreme" 20-150, 1% (twice) +50; multi-hit moves divide by their hit count.
static u32 MovePower(u32 move, u32 vanilla)
{
    u32 h, p, hits;
    if (!S->movePower || vanilla < 10)
        return vanilla;
    h = RH_Hash(SALT_MOVE_POWER, move, 0);
    p = (h % 3 != 2) ? 50 + 5 * ((h >> 2) % 11) : 20 + 5 * ((h >> 2) % 27);
    if ((h >> 10) % 100 == 0)
        p += 50;
    if ((h >> 17) % 100 == 0)
        p += 50;
    hits = HitCount(move);
    if (hits > 1)
    {
        p = (p / hits + 2) / 5 * 5;
        if (p == 0)
            p = 5;
    }
    return p > 255 ? 255 : p;
}

// FVX tiers: accuracy <= 50 -> 20-50 (10%: x1.5); < 90 -> count down from 100 with a 20% stop chance per 5%;
// >= 90 -> the same with a 40% stop chance. Moves that never miss are left alone.
static u32 MoveAccuracy(u32 move, u32 vanilla)
{
    u32 h, acc, i;
    if (!S->moveAccuracy || vanilla < 5)
        return vanilla;
    h = RH_Hash(SALT_MOVE_ACC, move, 0);
    if (vanilla <= 50)
    {
        acc = 20 + 5 * (h % 7);
        if ((h >> 4) % 10 == 0)
            acc = acc * 3 / 2 / 5 * 5;
        return acc;
    }
    acc = 100;
    for (i = 1; acc > 20; i++)
    {
        if (RH_Hash(SALT_MOVE_ACC, move, i) % 10 < (vanilla < 90 ? 2u : 4u))
            break;
        acc -= 5;
    }
    return acc;
}

static u32 MovePP(u32 move, u32 vanilla)
{
    u32 h;
    if (vanilla == 0)
        return vanilla;
    if (S->movesets == 3 && move == MOVE_METRONOME)
        return 40;                                           // FVX "Metronome Only"
    if (!S->movePP)
        return vanilla;
    h = RH_Hash(SALT_MOVE_PP, move, 0);
    return (h % 3 != 2) ? 15 + 5 * ((h >> 2) % 3) : 5 + 5 * ((h >> 2) % 8);
}

void RH_TransformMove(u32 move, struct BattleMove *bm)
{
    if (!RH_Active() || move == MOVE_NONE || move == MOVE_STRUGGLE || PoolIndexOf(move) < 0)
        return;
    bm->power = MovePower(move, bm->power);
    bm->accuracy = MoveAccuracy(move, bm->accuracy);
    bm->pp = MovePP(move, bm->pp);
    if (S->moveType && bm->type != TYPE_MYSTERY && bm->type < 18)
        bm->type = RH_RandomMonType(RH_Hash(SALT_MOVE_TYPE, move, 0));
    if (S->moveCategory && bm->split != SPLIT_STATUS && (RH_Hash(SALT_MOVE_CAT2, move, 0) & 1))
        bm->split = bm->split == SPLIT_PHYS ? SPLIT_SPEC : SPLIT_PHYS;
}

// ---------------------------------------------------------------------------
// Move picking. Picks look at the moves' current (possibly randomized) type / power / accuracy, cached once per
// settings (reading the move data for every pick would be far too slow).
// ---------------------------------------------------------------------------
enum { PICK_ANY, PICK_DAMAGING, PICK_GOOD };
#define MF_DAMAGING (1 << 0)
#define MF_GOOD     (1 << 1)
#define MF_PHYSICAL (1 << 3)
#define MF_SPECIAL  (1 << 4)

static u32 sCacheKey;

static void EnsureMoveCache(void)
{
    u32 i, key = RH_SettingsHash() + 1;
    if (sCacheKey == key)
        return;
    EnsurePool();
    for (i = 0; i < sPoolCount; i++)
    {
        struct BattleMove bm;
        u32 move = sPoolMove[i], power, f = 0;
        ReadFromNarcMemberByIdPair(&bm, ARC_MOVE_DATA, move, 0, sizeof(bm));
        RH_TransformMove(move, &bm);
        power = bm.power * RHM_HITS(sPoolFlags[i]);
        sCacheType[i] = bm.type;
        if (bm.split != SPLIT_STATUS && bm.power > 1 && !(sPoolFlags[i] & RHM_NO_DAMAGING))
        {
            f |= MF_DAMAGING;
            // FVX Move.isGoodDamaging: power >= 100, or >= 50 with 90%+ (or perfect) accuracy
            if (power >= 100 || (power >= 50 && (bm.accuracy >= 90 || bm.accuracy == 0)))
                f |= MF_GOOD;
        }
        if (bm.split == SPLIT_PHYS)
            f |= MF_PHYSICAL;
        else if (bm.split == SPLIT_SPEC)
            f |= MF_SPECIAL;
        sCacheFlags[i] = f;
    }
    sCacheKey = key;
}

static u8 sPickCategoryFlag;    // forced good attacks lean Physical or Special by Attack : Sp. Atk (FVX)

static BOOL MoveOk(u32 i, u32 type, u32 need, BOOL noBreaking, BOOL noHM)
{
    u32 f = sCacheFlags[i];
    if (noBreaking && ((sPoolFlags[i] & RHM_BROKEN) || ((sPoolFlags[i] & RHM_GEN1_BROKEN) && S->mechanicsGen == 1)))
        return FALSE;
    if (sPickCategoryFlag && need != PICK_ANY && !(f & sPickCategoryFlag))
        return FALSE;
    if (noHM && (sPoolFlags[i] & RHM_HM))
        return FALSE;
    if (need == PICK_DAMAGING && !(f & MF_DAMAGING))
        return FALSE;
    if (need == PICK_GOOD && !(f & MF_GOOD))
        return FALSE;
    if (type != RH_TYPE_NONE && sCacheType[i] != type)
        return FALSE;
    return TRUE;
}

static u32 PickMove(u32 hash, u32 type, u32 need, BOOL noBreaking, BOOL noHM)
{
    u32 i, count = 0, target;
    EnsureMoveCache();
    if (type == RH_TYPE_NONE)
    {
        // any type: random tries first (most moves qualify), a full count only when unlucky
        for (i = 0; i < 48; i++)
        {
            u32 k = RH_Hash(SALT_LEARNSET, hash, 1000 + i) % sPoolCount;
            if (MoveOk(k, type, need, noBreaking, noHM))
                return sPoolMove[k];
        }
    }
    for (i = 0; i < sPoolCount; i++)
        count += MoveOk(i, type, need, noBreaking, noHM);
    if (count == 0)
    {
        if (sPickCategoryFlag)
        {
            sPickCategoryFlag = 0;
            return PickMove(hash, type, need, noBreaking, noHM);
        }
        if (type != RH_TYPE_NONE)
            return PickMove(hash, RH_TYPE_NONE, need, noBreaking, noHM);
        if (need != PICK_ANY)
            return PickMove(hash, RH_TYPE_NONE, need == PICK_GOOD ? PICK_DAMAGING : PICK_ANY, noBreaking, noHM);
        return MOVE_TACKLE;
    }
    target = hash % count;
    for (i = 0; i < sPoolCount; i++)
        if (MoveOk(i, type, need, noBreaking, noHM) && target-- == 0)
            return sPoolMove[i];
    return MOVE_TACKLE;
}

static u32 MovePowerForSort(u32 move)
{
    s32 i = PoolIndexOf(move);
    struct BattleMove bm;
    if (i < 0)
        return 0;
    ReadFromNarcMemberByIdPair(&bm, ARC_MOVE_DATA, move, 0, sizeof(bm));
    RH_TransformMove(move, &bm);
    return bm.power * HitCount(move);
}

static BOOL IsDamaging(u32 move)
{
    s32 i = PoolIndexOf(move);
    EnsureMoveCache();
    return i >= 0 && (sCacheFlags[i] & MF_DAMAGING);
}

// "Random (prefer same type)" (FVX): Normal/X -> 10% Normal, 30% X; X/Y -> 20% each; single type -> 40%. Else any.
static u32 SameTypePick(u32 sp, u32 h)
{
    u32 t1 = RH_SpeciesType(sp, 0), t2 = RH_SpeciesType(sp, 1), r = (h >> 3) % 100;
    if (t1 != t2 && (t1 == TYPE_NORMAL || t2 == TYPE_NORMAL))
    {
        u32 other = (t1 == TYPE_NORMAL) ? t2 : t1;
        if (r < 10)
            return TYPE_NORMAL;
        return r < 40 ? other : RH_TYPE_NONE;
    }
    if (t1 != t2)
        return r < 20 ? t1 : (r < 40 ? t2 : RH_TYPE_NONE);
    return r < 40 ? t1 : RH_TYPE_NONE;
}

// ---------------------------------------------------------------------------
// Level-up movesets: u32 entries (level << 16 | move), ended by move 0xFFFF
// ---------------------------------------------------------------------------
#define LS_MAX MAX_LEVELUP_MOVES_RH
#define MAX_LEVELUP_MOVES_RH 34
#define LS_END 0xFFFF
#define LS_LEVEL(e) ((e) >> 16)
#define LS_MOVE(e) ((e) & 0xFFFF)

#define LS_CACHE 6
struct LearnsetCache { u16 sp; u32 key; u32 e[LS_MAX]; };
static struct LearnsetCache sLsCache[LS_CACHE];
static u8 sLsCacheNext;

static BOOL InList(const u32 *e, u32 n, u32 move)
{
    u32 i;
    for (i = 0; i < n; i++)
        if (LS_MOVE(e[i]) == move)
            return TRUE;
    return FALSE;
}

static void BuildLearnset(u32 sp, const u32 *v, u32 *out)
{
    u32 i, j, n = 0, total, lv1Count = 0, lastLv1, goodPct;
    u8 level[LS_MAX];
    if (S->movesets == 3)
    {
        out[0] = (1 << 16) | MOVE_METRONOME;
        out[1] = LS_END;
        return;
    }
    // Level slots: the vanilla ones (evolution moves at level 0 first), plus extra level-1 slots for
    // "Guaranteed Level 1 Moves" (FVX only adds, never removes).
    for (total = 0; total < LS_MAX && LS_MOVE(v[total]) != LS_END; total++)
        lv1Count += (LS_LEVEL(v[total]) == 1);
    for (i = 0; i < total && LS_LEVEL(v[i]) == 0; i++)
        level[n++] = 0;
    if (S->evolutionMovesForAll && n == 0 && RH_PreEvo(sp) != 0)
        level[n++] = 0;                                      // "Evolution Moves for All Pokemon"
    if (S->guaranteedLevel1On)
    {
        u32 want = S->guaranteedLevel1Moves < 2 ? 2 : (S->guaranteedLevel1Moves > 4 ? 4 : S->guaranteedLevel1Moves);
        for (j = lv1Count; j < want && n < LS_MAX - 2; j++)
            level[n++] = 1;
    }
    for (; i < total && n < LS_MAX - 2; i++)
        level[n++] = LS_LEVEL(v[i]);
    lastLv1 = n;
    for (i = 0; i < n; i++)
        if (level[i] == 1)
            lastLv1 = i;
    if (lastLv1 == n)
    {
        // no level-1 move at all: add one so the Pokemon can always attack
        for (i = n; i > 0 && level[i - 1] > 1; i--)
            level[i] = level[i - 1];
        level[i] = 1;
        lastLv1 = i;
        n++;
    }

    goodPct = S->movesetGoodDamagingOn ? S->movesetGoodDamaging : 0;
    {
        // "Force % Good Damaging": exactly round(% x moves) of the moves are good attacks (FVX), at hashed slots
        u8 forced[LS_MAX];
        u32 want = (goodPct * n + 50) / 100, have = 0;
        u32 atk = PokePersonalParaGet(sp, PERSONAL_BASE_ATTACK), spa = PokePersonalParaGet(sp, PERSONAL_BASE_SP_ATTACK);
        memset(forced, 0, sizeof(forced));
        forced[lastLv1] = TRUE;                              // FVX: always start with a good attack
        for (i = 0; have < want && i < n * 4; i++)
        {
            u32 k = RH_Hash(SALT_LEARNSET, sp, 5000 + i) % n;
            if (!forced[k])
                forced[k] = TRUE, have++;
        }
        for (i = 0; have < want && i < n; i++)
            if (!forced[i])
                forced[i] = TRUE, have++;
        for (i = 0; i < n; i++)
        {
            u32 move = MOVE_NONE, tries, need = forced[i] ? PICK_GOOD : PICK_ANY;
            for (tries = 0; tries < 12; tries++)
            {
                u32 h = RH_Hash(SALT_LEARNSET, sp, i * 16 + tries);
                u32 type = (S->movesets == 1) ? SameTypePick(sp, h) : RH_TYPE_NONE;
                sPickCategoryFlag = 0;
                if (need == PICK_GOOD && atk + spa > 0)
                    sPickCategoryFlag = ((RH_Hash(SALT_LEARNSET, sp, 9000 + i) % (atk + spa)) < atk) ? MF_PHYSICAL : MF_SPECIAL;
                move = PickMove(h >> 12, type, need, S->movesetNoGameBreaking, TRUE);
                sPickCategoryFlag = 0;
                if (!InList(out, i, move))
                    break;
                move = MOVE_NONE;
            }
            if (move == MOVE_NONE)
            {
                // no duplicates: take the first unused move that fits
                for (j = 0; j < sPoolCount && move == MOVE_NONE; j++)
                    if (MoveOk(j, RH_TYPE_NONE, need, S->movesetNoGameBreaking, TRUE) && !InList(out, i, sPoolMove[j]))
                        move = sPoolMove[j];
                if (move == MOVE_NONE)
                    move = MOVE_TACKLE;
            }
            out[i] = (level[i] << 16) | move;
        }
    }
    if (S->reorderDamagingMoves)
    {
        // Stronger attacks are learned later: sort the damaging moves by power, keeping the level slots
        // (evolution moves at level 0 and the guaranteed level-1 attack stay where they are).
        for (i = 0; i < n; i++)
        {
            if (level[i] == 0 || i == lastLv1 || !IsDamaging(LS_MOVE(out[i])))
                continue;
            for (j = i + 1; j < n; j++)
            {
                if (j != lastLv1 && IsDamaging(LS_MOVE(out[j])) && MovePowerForSort(LS_MOVE(out[j])) < MovePowerForSort(LS_MOVE(out[i])))
                {
                    u32 t = LS_MOVE(out[i]);
                    out[i] = (level[i] << 16) | LS_MOVE(out[j]);
                    out[j] = (level[j] << 16) | t;
                }
            }
        }
    }
    if (n < LS_MAX)
        out[n] = LS_END;
}

void RH_TransformLearnset(u32 sp, u32 *data)
{
    u32 i, key;
    struct LearnsetCache *c;
    if (!RH_Active() || S->movesets == 0 || sp == 0)
        return;
    key = RH_SettingsHash() + 2;
    for (i = 0; i < LS_CACHE; i++)
        if (sLsCache[i].sp == sp && sLsCache[i].key == key)
        {
            memcpy(data, sLsCache[i].e, sizeof(sLsCache[i].e));
            return;
        }
    c = &sLsCache[sLsCacheNext];
    sLsCacheNext = (sLsCacheNext + 1) % LS_CACHE;
    c->sp = 0;
    {
        u32 v[LS_MAX];
        memcpy(v, data, sizeof(v));
        for (i = 0; i < LS_MAX; i++)
            c->e[i] = LS_END;
        BuildLearnset(sp, v, c->e);
    }
    c->key = key;
    c->sp = sp;
    memcpy(data, c->e, sizeof(c->e));
}

// Egg moves: with random movesets every Pokemon keeps its number of egg moves, but they are random (FVX).
void RH_TransformEggMoves(u32 sp, u16 *moves, u32 max)
{
    u32 i, j, n;
    if (!RH_Active() || S->movesets == 0)
        return;
    for (n = 0; n < max && moves[n] != 0xFFFF; n++)
        ;
    for (i = 0; i < n; i++)
    {
        u32 move = MOVE_METRONOME;
        if (S->movesets != 3)
        {
            for (j = 0; j < 12; j++)
            {
                u32 h = RH_Hash(SALT_EGG_MOVES, sp, i * 16 + j);
                u32 type = (S->movesets == 1) ? SameTypePick(sp, h) : RH_TYPE_NONE, k;
                // egg moves follow "Force % Good Damaging" too (FVX)
                u32 need = (S->movesetGoodDamagingOn && (RH_Hash(SALT_EGG_MOVES, sp, 500 + i) % 100) < S->movesetGoodDamaging) ? PICK_GOOD : PICK_ANY;
                move = PickMove(h >> 12, type, need, S->movesetNoGameBreaking, TRUE);
                for (k = 0; k < i && moves[k] != move; k++)
                    ;
                if (k == i)
                    break;
            }
        }
        moves[i] = move;
        if (S->movesets == 3)
        {
            if (max > 1)
                moves[1] = 0xFFFF;
            break;
        }
    }
}

// ---------------------------------------------------------------------------
// TMs: every TM teaches a unique random move (HMs never change). Machine index = item - ITEM_TM001:
// 0-91 = TM01-TM92, 92-99 = HM01-HM08.
// ---------------------------------------------------------------------------
#define NUM_TMS 92
#define NUM_TMHM 100

// FVX's HeartGold/SoulSilver field moves
static const u16 sFieldMoves[] = {
    MOVE_CUT, MOVE_FLY, MOVE_SURF, MOVE_STRENGTH, MOVE_FLASH, MOVE_DIG, MOVE_TELEPORT, MOVE_WATERFALL,
    MOVE_ROCK_SMASH, MOVE_SWEET_SCENT, MOVE_WHIRLPOOL, MOVE_ROCK_CLIMB, MOVE_HEADBUTT,
};

u16 LONG_CALL ItemToMachineMoveVanilla(u32 index);

static BOOL IsFieldMove(u32 move)
{
    u32 i;
    for (i = 0; i < NELEMS(sFieldMoves); i++)
        if (sFieldMoves[i] == move)
            return TRUE;
    return FALSE;
}

static u32 sMachineKey;
static u16 sMachine[NUM_TMS];
static u16 sVanillaMachine[NUM_TMHM];

static BOOL UsedMachineMove(u32 move, u32 count)
{
    u32 i;
    for (i = 0; i < count; i++)
        if (sMachine[i] == move)
            return TRUE;
    for (i = 0; i < NUM_TMHM; i++)
        if (sVanillaMachine[i] == move && (i >= NUM_TMS || (S->tmKeepFieldMoves && IsFieldMove(move))))
            return TRUE;                                     // HMs and kept field-move TMs keep their moves to themselves
    return FALSE;
}

static void EnsureMachines(void)
{
    u32 i, tries, key = RH_SettingsHash() + 3;
    if (sMachineKey == key)
        return;
    EnsureMoveCache();
    for (i = 0; i < NUM_TMHM; i++)
        sVanillaMachine[i] = ItemToMachineMoveVanilla(i);
    for (i = 0; i < NUM_TMS; i++)
    {
        u32 vanilla = sVanillaMachine[i], move = 0;
        // "Force % Good Damaging" (FVX): exactly round(% x count) of the TMs teach a good attack
        u32 pct = S->tmGoodDamagingOn ? S->tmGoodDamaging : 0;
        BOOL good = RH_Permute(SALT_TM + 77, i, NUM_TMS) < (pct * NUM_TMS + 50) / 100;
        sMachine[i] = vanilla;
        if (S->tmKeepFieldMoves && IsFieldMove(vanilla))
            continue;
        for (tries = 0; tries < 64 && move == 0; tries++)
        {
            u32 m = PickMove(RH_Hash(SALT_TM, i, tries) >> 8, RH_TYPE_NONE, good ? PICK_GOOD : PICK_ANY, S->tmNoGameBreaking, FALSE);
            if (!UsedMachineMove(m, i))
                move = m;
        }
        // unlucky: the next unused move of the pool, so no TM ever repeats one
        for (tries = 0; tries < sPoolCount && move == 0; tries++)
        {
            u32 k = (RH_Hash(SALT_TM, i, 999) + tries) % sPoolCount;
            if (!(S->tmNoGameBreaking && (sPoolFlags[k] & RHM_BROKEN)) && !UsedMachineMove(sPoolMove[k], i))
                move = sPoolMove[k];
        }
        if (move != 0)
            sMachine[i] = move;
    }
    sMachineKey = key;
}

u32 RH_MachineMove(u32 index, u32 vanilla)
{
    if (!RH_Active() || index >= NUM_TMS)
        return vanilla;                                      // HMs never change
    if (S->movesets == 3)
        return MOVE_METRONOME;                               // FVX "Metronome Only": every TM is Metronome
    if (!S->tmMoves)
        return vanilla;
    EnsureMachines();
    return sMachine[index];
}

// ---------------------------------------------------------------------------
// TM / HM compatibility
// ---------------------------------------------------------------------------
static BOOL LearnsByLevelUp(u32 sp, u32 move)
{
    u32 l[MAX_LEVELUP_MOVES_RH], i;
    u16 species, form;
    RH_SpeciesKey(sp, &species, &form);
    ReadFromNarcMemberByIdPair(l, ARC_LEVELUP_LEARNSETS, 0, sp * MAX_LEVELUP_MOVES_RH * 4, sizeof(l));
    RH_TransformLearnset(sp, l);
    for (i = 0; i < MAX_LEVELUP_MOVES_RH && LS_MOVE(l[i]) != LS_END; i++)
        if (LS_MOVE(l[i]) == move)
            return TRUE;
    return FALSE;
}

static BOOL LearnsByLevelUpOrAncestor(u32 sp, u32 move, BOOL followEvos)
{
    u32 depth;
    for (depth = 0; depth < 4 && sp != 0; depth++)
    {
        if (LearnsByLevelUp(sp, move))
            return TRUE;
        if (!followEvos)
            break;
        sp = RH_PreEvo(sp);
    }
    return FALSE;
}

static u32 MoveTypeNow(u32 move)
{
    s32 i = PoolIndexOf(move);
    EnsureMoveCache();
    return i >= 0 ? sCacheType[i] : TYPE_NORMAL;
}

// FVX odds: prefer type 90% (own type) / 50% (Normal) / 25% (other); completely random 50%.
static u32 BaseOdds(u32 mode, u32 sp, u32 move)
{
    u32 type = MoveTypeNow(move);
    if (mode == 2)
        return 50;
    if (RH_SpeciesHasType(sp, type))
        return 90;
    return type == TYPE_NORMAL ? 50 : 25;
}

static BOOL RandomCompat(u32 mode, BOOL followEvos, u32 sp, u32 move, u32 slot, u32 depth)
{
    u32 pre, odds;
    if (mode == 3)
        return TRUE;
    pre = (followEvos && depth < 3) ? RH_PreEvo(sp) : 0;
    if (pre == 0)
        odds = BaseOdds(mode, sp, move);
    else if (RandomCompat(mode, TRUE, pre, move, slot, depth + 1))
        return TRUE;                                         // Follow Evolutions: everything the pre-evolution learns
    else if (mode == 2)
        odds = 25;                                           // ... plus a few more
    else
        odds = (RH_SpeciesHasType(sp, MoveTypeNow(move)) && !RH_SpeciesHasType(pre, MoveTypeNow(move))) ? 90 : 10;
    return RH_Hash(SALT_TM_COMPAT, sp, slot) % 100 < odds;
}

BOOL RH_MachineCompat(u32 sp, u32 index, BOOL vanilla)
{
    u32 move;
    if (!RH_Active() || index >= NUM_TMHM)
        return vanilla;
    if (S->movesets == 3 && index < NUM_TMS)
        return TRUE;                                         // Metronome for everyone
    if (index >= NUM_TMS)                                    // HM
    {
        if (S->fullHMCompat || S->tmCompat == 3)
            return TRUE;
        if (S->tmCompat == 0)
            return vanilla;
        EnsureMachines();
        return RandomCompat(S->tmCompat, S->tmCompatFollowEvos, sp, sVanillaMachine[index], index, 0);
    }
    move = RH_MachineMove(index, 0);
    if (S->tmLevelupSanity && move != 0 && LearnsByLevelUpOrAncestor(sp, move, S->tmCompatFollowEvos))
        return TRUE;
    if (S->tmCompat == 0)
        return vanilla;                                      // the slot keeps its original compatibility
    return RandomCompat(S->tmCompat, S->tmCompatFollowEvos, sp, move ? move : MOVE_TACKLE, index, 0);
}

// ---------------------------------------------------------------------------
// Move tutors: every tutor slot teaches a unique random move, never one a TM / HM teaches (FVX).
// ---------------------------------------------------------------------------
static BOOL UsedTutorMove(u32 move, const u16 *out, u32 count)
{
    u32 i;
    for (i = 0; i < count; i++)
        if (out[i] == move)
            return TRUE;
    if (S->tmMoves)
    {
        for (i = 0; i < NUM_TMS; i++)
            if (sMachine[i] == move)
                return TRUE;
    }
    for (i = 0; i < NUM_TMHM; i++)
        if (sVanillaMachine[i] == move && (i >= NUM_TMS || !S->tmMoves))
            return TRUE;
    return FALSE;
}

// vanilla: the tutors' moves (count slots); out: the moves they teach with the current settings.
void RH_TutorMoves(const u16 *vanilla, u16 *out, u32 count)
{
    u32 i, tries;
    for (i = 0; i < count; i++)
        out[i] = vanilla[i];
    if (!RH_Active())
        return;
    if (S->movesets == 3)
    {
        for (i = 0; i < count; i++)
            out[i] = MOVE_METRONOME;
        return;
    }
    if (!S->tutorMoves)
        return;
    EnsureMachines();
    for (i = 0; i < count; i++)
    {
        u32 pct = S->tutorGoodDamagingOn ? S->tutorGoodDamaging : 0, move = 0;
        BOOL good = RH_Permute(SALT_TUTOR + 77, i, count) < (pct * count + 50) / 100;
        if (S->tmKeepFieldMoves && IsFieldMove(vanilla[i]))
            continue;                                        // Headbutt stays with its tutor
        for (tries = 0; tries < 64 && move == 0; tries++)
        {
            u32 m = PickMove(RH_Hash(SALT_TUTOR, i, tries) >> 8, RH_TYPE_NONE, good ? PICK_GOOD : PICK_ANY, S->tutorNoGameBreaking, FALSE);
            if (!UsedTutorMove(m, out, i))
                move = m;
        }
        for (tries = 0; tries < sPoolCount && move == 0; tries++)
        {
            u32 k = (RH_Hash(SALT_TUTOR, i, 999) + tries) % sPoolCount;
            if (!(S->tutorNoGameBreaking && (sPoolFlags[k] & RHM_BROKEN)) && !UsedTutorMove(sPoolMove[k], out, i))
                move = sPoolMove[k];
        }
        if (move != 0)
            out[i] = move;
    }
}

BOOL RH_TutorCompat(u32 sp, u32 slot, u32 move, BOOL vanilla)
{
    if (!RH_Active())
        return vanilla;
    if (S->movesets == 3)
        return TRUE;
    if (S->tutorLevelupSanity && LearnsByLevelUpOrAncestor(sp, move, S->tutorCompatFollowEvos))
        return TRUE;
    if (S->tutorCompat == 0)
        return vanilla;
    return RandomCompat(S->tutorCompat, S->tutorCompatFollowEvos, sp, move, 0x100 + slot, 0);
}
