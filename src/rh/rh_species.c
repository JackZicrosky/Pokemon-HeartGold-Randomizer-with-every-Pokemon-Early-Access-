// Romhack randomizer core: the species table (a028 member 20, tools/rh/gen_rh_data.py) and the Pokemon Pool.
#include "../../include/types.h"
#include "../../include/save.h"
#include "../../include/constants/file.h"
#include "../../include/constants/species.h"
#include "../../include/rh/rh_internal.h"

#define INFO_COUNT (MAX_SPECIES_INCLUDING_FORMS + 1)

static struct RhSpeciesInfo *sTable;     // the whole table while a session is open
static u8 sSessionDepth;

u32 RH_InfoCount(void)
{
    return INFO_COUNT;
}

BOOL RH_GetInfo(u32 sp, struct RhSpeciesInfo *out)
{
    if (sp == 0 || sp >= INFO_COUNT)
    {
        memset(out, 0, sizeof(*out));
        return FALSE;
    }
    if (sTable != NULL)
        *out = sTable[sp];
    else
        ReadFromNarcMemberByIdPair(out, ARC_CODE_ADDONS, CODE_ADDON_RH_SPECIES_INFO, sp * sizeof(*out), sizeof(*out));
    return TRUE;
}

// Free bytes of a heap, 0 if it doesn't exist (the game's own call asserts on missing heaps).
u32 RH_HeapFree(u32 heapId)
{
    u32 *handles = *(u32 **)0x021D1584;                       // sHeapInfo.heapHandles
    u8 *idxs = *(u8 **)(0x021D1584 + 0x10);                   // sHeapInfo.heapIdxs
    if (heapId > 0x80 || handles == NULL || idxs == NULL || handles[idxs[heapId]] == 0)
        return 0;
    return GF_ExpHeap_FndGetTotalFreeSize(heapId);
}

// The field heap when it exists (lots of room), else the root heap.
u32 RH_SessionHeap(void)
{
    return RH_HeapFree(4) > 0x10000 ? 4 : 3;
}

void RH_BeginSession(u32 heapId)
{
    if (heapId == 0xFFFF)
        heapId = RH_SessionHeap();
    if (sSessionDepth++ == 0)
    {
        u32 size = INFO_COUNT * sizeof(struct RhSpeciesInfo);
        // only when the heap has plenty of room; without the table every lookup reads the cartridge instead
        if (RH_HeapFree(heapId) > size + 0x4000)
        {
            sTable = sys_AllocMemory(heapId, size);
            if (sTable != NULL)
                ReadFromNarcMemberByIdPair(sTable, ARC_CODE_ADDONS, CODE_ADDON_RH_SPECIES_INFO, 0, size);
        }
    }
}

void RH_EndSession(void)
{
    if (sSessionDepth != 0 && --sSessionDepth == 0 && sTable != NULL)
    {
        sys_FreeMemoryEz(sTable);
        sTable = NULL;
    }
}

u32 RH_VanillaBST(u32 sp)
{
    struct RhSpeciesInfo i;
    RH_GetInfo(sp, &i);
    return i.bst;
}

BOOL RH_IsLegendary(u32 sp)
{
    struct RhSpeciesInfo i;
    RH_GetInfo(sp, &i);
    return (i.flags & RH_SPF_LEGENDARY) != 0;
}

u32 RH_PreEvo(u32 sp)
{
    struct RhSpeciesInfo i;
    RH_GetInfo(sp, &i);
    return i.preEvo;
}

u32 RH_SpeciesStage(u32 sp)
{
    struct RhSpeciesInfo i;
    if (!RH_GetInfo(sp, &i))
        return 1;
    return RH_INFO_STAGE(&i);
}

u32 RH_SpeciesChain(u32 sp)
{
    struct RhSpeciesInfo i;
    if (!RH_GetInfo(sp, &i))
        return 1;
    return RH_INFO_CHAIN(&i);
}

u32 RH_SpeciesIndex(u32 species, u32 form)
{
    return form ? (u32)PokeOtherFormMonsNoGet(species, form) : species;
}

void RH_SpeciesKey(u32 sp, u16 *species, u16 *form)
{
    struct RhSpeciesInfo i;
    if (!RH_GetInfo(sp, &i))
    {
        *species = sp;
        *form = 0;
        return;
    }
    *species = RH_KEY_SPECIES(i.key);
    *form = RH_KEY_FORM(i.key);
}

// The Pokemon Pool setting (never Megas; regional/alternate forms only in "All + Forms").
BOOL RH_PoolAllowed(const struct RhSpeciesInfo *i)
{
    if (!(i->flags & RH_SPF_POOL) || (i->flags & RH_SPF_MEGA) || (i->flags2 & RH_SPF2_VIVILLON))
        return FALSE;
    switch (S->speciesPool)
    {
    case RH_POOL_GEN1:   return RH_INFO_GEN(i) == 1 && !(i->flags & RH_SPF_VARIANT);
    case RH_POOL_GEN1_3: return RH_INFO_GEN(i) <= 3 && !(i->flags & RH_SPF_VARIANT);
    case RH_POOL_ALL:    return !(i->flags & RH_SPF_VARIANT);
    default:             return TRUE;
    }
}

// ---------------------------------------------------------------------------
// Evolutions
// ---------------------------------------------------------------------------
// The evolution table as the game reads it (so random evolutions count), as personal indexes.
static u32 Evolutions(u32 sp, u16 *targets)
{
    u16 e[9 * 3 + 1];
    u32 i, n = 0;
    ReadWholeNarcMemberByIdPair(e, ARC_EVOLUTIONS, sp);
    for (i = 0; i < 9; i++)
    {
        u32 t = e[i * 3 + 2];
        if (e[i * 3] == 0 || t == 0)
            continue;
        t = (t >> 11) ? RH_SpeciesIndex(t & 0x7FF, t >> 11) : t;
        if (t != sp)
            targets[n++] = t;
    }
    return n;
}

u32 RH_EvolveOnce(u32 sp, u32 hash)
{
    u16 t[9];
    u32 n;
    if (sp == 0 || sp >= INFO_COUNT)
        return 0;
    n = Evolutions(sp, t);
    return n ? t[hash % n] : 0;
}

u32 RH_EvolveTimes(u32 sp, u32 times)
{
    while (times--)
    {
        u32 next = RH_EvolveOnce(sp, RH_Hash(SALT_EVO, sp, times));
        if (next == 0)
            break;
        sp = next;
    }
    return sp;
}
