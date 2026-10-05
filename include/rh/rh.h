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

// ---- src/rh/rh_core.c ----
struct RhSettings *RH_Settings(void);           // the active settings (the save's copy)
BOOL RH_Active(void);                           // randomizer set up and enabled
u32 RH_Hash(u32 salt, u32 a, u32 b);            // deterministic, seed-dependent hash
u32 RH_Random(u32 salt, u32 a, u32 b, u32 n);   // RH_Hash(...) % n
void RH_ReadWholeNarcMemberByIdPair(void *dest, u32 narcId, s32 fileId);

// debug counters (read by the test harness)
struct RhDebug
{
    u32 magic;            // 0x52484442 "RHDB" once the core overlay is loaded
    u32 narcReads;
    u32 personalReads;
    u32 lastPersonal;
};
extern struct RhDebug gRhDebug;

#endif // RH_H
