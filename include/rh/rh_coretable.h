#ifndef RH_CORETABLE_H
#define RH_CORETABLE_H
// Romhack: calls INTO the randomizer core from overlay 129 (src/*.c), which is linked before the core and so
// can't name core functions. The core starts with a table of function pointers (src/rh/rh_coretable.c, placed
// first by src/rh/linker.ld at 0x01FF8700); these wrappers call through it.

#include "types.h"

enum
{
    RHCT_STARTER_FOR_SLOT,
    RHCT_SPECIES_KEY,
    RHCT_COUNT
};

#define RH_CORE_TABLE ((void *const *)0x01FF8700)

static inline u32 RHCT_StarterForSlot(u32 slot)
{
    return ((u32 (*)(u32))RH_CORE_TABLE[RHCT_STARTER_FOR_SLOT])(slot);
}

static inline void RHCT_SpeciesKey(u32 sp, u16 *species, u16 *form)
{
    ((void (*)(u32, u16 *, u16 *))RH_CORE_TABLE[RHCT_SPECIES_KEY])(sp, species, form);
}

#endif // RH_CORETABLE_H
