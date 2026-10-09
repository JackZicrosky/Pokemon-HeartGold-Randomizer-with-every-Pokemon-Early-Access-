// Romhack: the core's entry table for overlay 129 (see include/rh/rh_coretable.h). Must stay first in the core.
#include "../../include/types.h"
#include "../../include/rh/rh_coretable.h"
#include "../../include/rh/rh_internal.h"

void *const gRhCoreTable[RHCT_COUNT] __attribute__((section(".rh_header"))) = {
    [RHCT_STARTER_FOR_SLOT] = (void *)RH_StarterForSlot,
    [RHCT_SPECIES_KEY]      = (void *)RH_SpeciesKey,
    [RHCT_TRANSFORM_MOVE]   = (void *)RH_TransformMove,
    [RHCT_TRANSFORM_LEARNSET] = (void *)RH_TransformLearnset,
    [RHCT_TRANSFORM_EGG_MOVES] = (void *)RH_TransformEggMoves,
    [RHCT_MACHINE_MOVE]     = (void *)RH_MachineMove,
    [RHCT_MACHINE_COMPAT]   = (void *)RH_MachineCompat,
    [RHCT_STATIC_SPECIES]   = (void *)RH_StaticSpecies,
    [RHCT_STATIC_LEVEL]     = (void *)RH_StaticLevel,
    [RHCT_TRANSFORM_TRADE]  = (void *)RH_TransformTrade,
    [RHCT_ITEM_PRICE]       = (void *)RH_ItemPrice,
};
