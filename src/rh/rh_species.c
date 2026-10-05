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

void RH_BeginSession(u32 heapId)
{
    if (sSessionDepth++ == 0)
    {
        u32 size = INFO_COUNT * sizeof(struct RhSpeciesInfo);
        // only when the heap has plenty of room; without the table every lookup reads the cartridge instead
        if (GF_ExpHeap_FndGetTotalFreeSize(heapId) > size + 0x4000)
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
