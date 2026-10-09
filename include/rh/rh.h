#ifndef RH_H
#define RH_H
// Romhack (HeartGold Expansion Randomizer) public interface.
//
// Code layout (the DS has no spare main RAM, so the randomizer is split by where it runs):
//   src/rh/      overlay 152, loaded at boot into free ITCM and never unloaded: settings access, seeded
//                hashing, species pool, and the transforms applied to game data as it is read from the
//                cartridge (Pokemon stats/types/abilities, evolutions, learnsets, move data).
//   src/field/   (hg-engine's field extension overlay) trainer parties, wild encounters, items, shops.
//   src/rhmenu/  the options screen, loaded only while it is open.

#include "types.h"
#include "rh/rh_settings.h"

#define OVERLAY_RH_CORE 152
#define OVERLAY_RH_MENU 153
// The options screen's app template is the first thing in overlay 153 (see src/rhmenu/linker.ld).
#define RH_MENU_APP_TEMPLATE ((const void *)0x021E5900)

// ---- src/rh/rh_core.c ----
// (LONG_CALL: called through a register so the Thumb bit survives; the core is linked separately)
struct RhSettings *LONG_CALL RH_Settings(void);           // the active settings (the save's copy)
BOOL LONG_CALL RH_Active(void);                           // randomizer set up and enabled
void LONG_CALL RH_SetDefaultSettings(struct RhSettings *s);
void LONG_CALL RH_RegisterMainOverlay(u32 ovyId, const void *template);
u32 LONG_CALL RH_Hash(u32 salt, u32 a, u32 b);            // deterministic, seed-dependent hash
u32 LONG_CALL RH_Random(u32 salt, u32 a, u32 b, u32 n);   // RH_Hash(...) % n
void LONG_CALL RH_ReadWholeNarcMemberByIdPair(void *dest, u32 narcId, s32 fileId);
void LONG_CALL RH_OpenSettingsFromField(void *fieldSystem);      // the options screen in-game (key item)
BOOL LONG_CALL RH_SettingsOpenedInGame(void *appArgs);           // the options screen's args: opened in-game?
void LONG_CALL RH_GiveKeyItems(void);                            // Randomizer Settings (+ Nuzlocke's items) if missing

// RAM in overlay 129 (always loaded) for the core's large per-settings tables (src/rh_boot.c)
#define RH_RESIDENT_SIZE 4096
void *LONG_CALL RH_Resident(void);

// debug counters (read by the test harness)
struct RhDebug
{
    u32 magic;            // 0x52484442 "RHDB" once the core overlay is loaded
    u32 narcReads;
    u32 personalReads;
    u32 lastPersonal;
    u32 heapMinFree[12];  // smallest free size seen per heap id (sampled on data reads), 0 = never sampled
};
// (gRhDebug lives in the core; other overlays must not reference core variables: the build exports them
// with the Thumb bit set. Only call core functions.)

#endif // RH_H
