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
