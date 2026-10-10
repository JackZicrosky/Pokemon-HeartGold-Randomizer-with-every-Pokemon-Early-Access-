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
    RHCT_TRANSFORM_MOVE,
    RHCT_TRANSFORM_LEARNSET,
    RHCT_TRANSFORM_EGG_MOVES,
    RHCT_MACHINE_MOVE,
    RHCT_MACHINE_COMPAT,
    RHCT_STATIC_SPECIES,
    RHCT_STATIC_LEVEL,
    RHCT_TRANSFORM_TRADE,
    RHCT_ITEM_PRICE,
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

static inline void RHCT_TransformMove(u32 move, void *battleMove)
{
    ((void (*)(u32, void *))RH_CORE_TABLE[RHCT_TRANSFORM_MOVE])(move, battleMove);
}

static inline void RHCT_TransformLearnset(u32 sp, u32 *data)
{
    ((void (*)(u32, u32 *))RH_CORE_TABLE[RHCT_TRANSFORM_LEARNSET])(sp, data);
}

static inline void RHCT_TransformEggMoves(u32 sp, u16 *moves, u32 max)
{
    ((void (*)(u32, u16 *, u32))RH_CORE_TABLE[RHCT_TRANSFORM_EGG_MOVES])(sp, moves, max);
}

static inline u32 RHCT_MachineMove(u32 index, u32 vanilla)
{
    return ((u32 (*)(u32, u32))RH_CORE_TABLE[RHCT_MACHINE_MOVE])(index, vanilla);
}

static inline BOOL RHCT_MachineCompat(u32 sp, u32 index, BOOL vanilla)
{
    return ((BOOL (*)(u32, u32, BOOL))RH_CORE_TABLE[RHCT_MACHINE_COMPAT])(sp, index, vanilla);
}

static inline u32 RHCT_StaticSpecies(u32 species, u32 map)
{
    return ((u32 (*)(u32, u32))RH_CORE_TABLE[RHCT_STATIC_SPECIES])(species, map);
}

static inline u32 RHCT_StaticLevel(u32 level)
{
    return ((u32 (*)(u32))RH_CORE_TABLE[RHCT_STATIC_LEVEL])(level);
}

static inline void RHCT_TransformTrade(u32 tradeno, void *tradeData)
{
    ((void (*)(u32, void *))RH_CORE_TABLE[RHCT_TRANSFORM_TRADE])(tradeno, tradeData);
}

static inline u32 RHCT_ItemPrice(u32 item, u32 vanilla, u32 balanced)
{
    return ((u32 (*)(u32, u32, u32))RH_CORE_TABLE[RHCT_ITEM_PRICE])(item, vanilla, balanced);
}

#endif // RH_CORETABLE_H
