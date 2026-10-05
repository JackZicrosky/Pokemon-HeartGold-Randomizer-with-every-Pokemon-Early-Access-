// Romhack: the core's entry table for overlay 129 (see include/rh/rh_coretable.h). Must stay first in the core.
#include "../../include/types.h"
#include "../../include/rh/rh_coretable.h"
#include "../../include/rh/rh_internal.h"

void *const gRhCoreTable[RHCT_COUNT] __attribute__((section(".rh_header"))) = {
    [RHCT_STARTER_FOR_SLOT] = (void *)RH_StarterForSlot,
    [RHCT_SPECIES_KEY]      = (void *)RH_SpeciesKey,
};
