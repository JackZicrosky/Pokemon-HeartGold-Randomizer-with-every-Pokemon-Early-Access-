// Romhack: loads the randomizer core (overlay 152) into ITCM right after the arm9 expansion (overlay 129)
// is loaded at boot. armips/asm/syntheticoverlay.s calls this; it sits in .rh_boot (first in src/linker.ld) so it is always the first
// function of overlay 129 (0x023D8060).
#include "../include/types.h"
#include "../include/overlay.h"
#include "../include/rh/rh.h"

u32 __attribute__((section(".rh_boot"))) RH_Boot(void)
{
    HandleLoadOverlay(OVERLAY_RH_CORE, 0);
    return 0;
}

// Resident RAM for the randomizer core's per-settings tables (the core's ITCM is too small for them): taken once
// from the end of the system heap (heap 0, which lives for the whole game and keeps ~48 KB free) on first use,
// zeroed. (It used to be a static array in overlay 129, which is full.)
#define Heap_AllocAtEnd ((void *(*)(u32, u32))(0x0201AACC | 1))
static u32 *sRhResident;

void *RH_Resident(void)
{
    u32 i;
    if (sRhResident == NULL)
    {
        sRhResident = Heap_AllocAtEnd(0, RH_RESIDENT_SIZE);
        for (i = 0; i < RH_RESIDENT_SIZE / 4; i++)
            sRhResident[i] = 0;
    }
    return sRhResident;
}
