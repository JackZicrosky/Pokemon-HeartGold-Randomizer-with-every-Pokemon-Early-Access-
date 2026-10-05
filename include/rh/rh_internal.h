#ifndef RH_INTERNAL_H
#define RH_INTERNAL_H
// Romhack: shared helpers for the randomizer modules. Every function here lives in the core overlay
// (src/rh, ITCM) and is LONG_CALL so any overlay can call it.

#include "types.h"
#include "rh/rh.h"
#include "rh/rh_data.h"

#define S (RH_Settings())

// "Species" in the randomizer = a personal-data index: a base species (1..SPECIES_MAX_MON_NUM) or one of its
// forms (higher indexes). RH_KEY_SPECIES/RH_KEY_FORM of its info record give the species + form to create.

// Independent random streams (same list as FireRed/Emerald).
enum RhSalt
{
    SALT_WILD = 1, SALT_WILD_AREA, SALT_WILD_THEME, SALT_TRAINER, SALT_TRAINER_THEME, SALT_STARTER,
    SALT_STATIC, SALT_TRADE, SALT_TYPE1, SALT_TYPE2, SALT_ABILITY, SALT_STATS, SALT_STAT_PERM,
    SALT_LEARNSET, SALT_EVO, SALT_TM, SALT_TM_COMPAT, SALT_TUTOR_COMPAT, SALT_MOVE_POWER, SALT_MOVE_ACC,
    SALT_MOVE_PP, SALT_MOVE_TYPE, SALT_MOVE_CAT, SALT_FIELD_ITEM, SALT_SHOP, SALT_HELD_ITEM, SALT_GLOBAL_PERM,
    SALT_TYPE_CHART, SALT_MOVE_NAME, SALT_TRAINER_NAME, SALT_TRAINER_CLASS, SALT_NICKNAME, SALT_OT, SALT_IV,
    SALT_TUTOR, SALT_PICKUP, SALT_SPECIAL_SHOP, SALT_EXTRA_MON, SALT_RIVAL, SALT_LEAGUE, SALT_ADDED_STATS,
    SALT_EVO_LEVEL, SALT_WILD_ITEM, SALT_CATCH_ALL, SALT_BATTLE_STYLE, SALT_STARTER_ITEM, SALT_STARTER_TYPE,
    SALT_EVO_RANK, SALT_STATIC_SLOT, SALT_TRADE_SLOT, SALT_TRAINER_SHINY, SALT_EGG_MOVES, SALT_MOVE_CAT2,
    SALT_INTRO,
};

#define RH_NUM_TYPES 18
#define RH_TYPE_NONE 0xFF

// ---- species data (src/rh/rh_species.c) ----
u32 LONG_CALL RH_InfoCount(void);
BOOL LONG_CALL RH_GetInfo(u32 sp, struct RhSpeciesInfo *out);          // FALSE if sp is out of range
u32 LONG_CALL RH_VanillaBST(u32 sp);
BOOL LONG_CALL RH_IsLegendary(u32 sp);
u32 LONG_CALL RH_PreEvo(u32 sp);                                        // personal index, 0 = basic
u32 LONG_CALL RH_SpeciesStage(u32 sp);                                  // 1 = basic
u32 LONG_CALL RH_SpeciesChain(u32 sp);                                  // stages in its line
u32 LONG_CALL RH_SpeciesIndex(u32 species, u32 form);                   // species + form -> personal index
void LONG_CALL RH_SpeciesKey(u32 sp, u16 *species, u16 *form);          // personal index -> species + form
BOOL LONG_CALL RH_PoolAllowed(const struct RhSpeciesInfo *info);       // the Pokemon Pool setting (never Megas)

// A whole-table session: the species table loaded into a heap buffer for many lookups (picks).
// RH_BeginSession/RH_EndSession nest; the table is freed by the outermost RH_EndSession.
void LONG_CALL RH_BeginSession(u32 heapId);   // heapId 0xFFFF = RH_SessionHeap()
void LONG_CALL RH_EndSession(void);

// ---- picking (src/rh/rh_pick.c) ----
#define RH_MAX_EXCLUDE 12
struct RhFilter
{
    u16 minBst, maxBst;          // 0 = no limit
    u8 type;                     // RH_TYPE_NONE = any
    u8 primaryType;              // RH_TYPE_NONE = any; replacement's first type must match
    u8 legend;                   // 0 any, 1 no legendaries, 2 only legendaries
    u8 stage;                    // 0 any, else exact evolution stage
    u8 threeStageBasic;
    u8 allowMegas;
    u8 monoType;
    u8 megasOnly;                // with allowMegas: only Mega Evolutions
    u8 allowVariants;            // regional/alternate forms even if the pool setting excludes them
    u8 legalAtLevel;             // "No Premature Evolutions": 0 = off
    u8 excludeCount;
    u16 exclude[RH_MAX_EXCLUDE];
    BOOL (*extra)(u32 sp);       // optional extra predicate (LONG_CALL-safe function pointer)
};
void LONG_CALL RH_FilterInit(struct RhFilter *f);
void LONG_CALL RH_FilterExclude(struct RhFilter *f, u32 sp);
BOOL LONG_CALL RH_FilterAccepts(const struct RhFilter *f, u32 sp);
u32 LONG_CALL RH_PickWithFilter(const struct RhFilter *f, u32 hash);
u32 LONG_CALL RH_PickRanked(const struct RhFilter *f, u32 rank, u32 salt);
u32 LONG_CALL RH_PickSpeciesNearBst(struct RhFilter *f, u32 hash, u32 bst);   // relaxes rules when nothing fits
u32 LONG_CALL RH_PickSpeciesNearBstRanked(struct RhFilter *f, u32 rank, u32 salt, u32 bst);
u32 LONG_CALL RH_Permute(u32 salt, u32 x, u32 n);
s32 LONG_CALL RH_ApplyPercent(s32 value, s16 percent);

// ---- traits (src/rh/rh_traits.c) ----
u32 LONG_CALL RH_SpeciesType(u32 sp, u32 slot);                         // current (randomized) type
BOOL LONG_CALL RH_SpeciesHasType(u32 sp, u32 type);
u32 LONG_CALL RH_SpeciesBST(u32 sp);                                    // current base stat total
u32 LONG_CALL RH_RandomMonType(u32 hash);
u32 LONG_CALL RH_TypeIndexOf(u32 type);
u32 LONG_CALL RH_MonTypeAt(u32 i);
BOOL LONG_CALL RH_TypeBeats(u32 attackType, u32 defendType);           // super effective (rh_sources.c)
u32 LONG_CALL RH_TypeEffect(u32 attackType, u32 defendType);           // 0 none, 1 half, 2 normal, 3 double
u32 LONG_CALL RH_StarterForSlot(u32 slot);                              // personal index in Elm's ball 0-2
void LONG_CALL RH_TransformPersonal(u32 sp, u8 *data);                 // applied to every personal data read
u32 LONG_CALL RH_SettingsHash(void);                                    // hash of every setting (cache key)
u32 LONG_CALL RH_EstimateEvoLevel(u32 from, u32 to);                    // FVX estimated evolution level
u32 LONG_CALL RH_GrowthRate(u32 sp);                                    // current EXP curve
void LONG_CALL RH_TransformEvolutions(u32 sp, u16 *data);
// ---- moves (src/rh/rh_moves.c) ----
struct BattleMove;
void LONG_CALL RH_TransformMove(u32 move, struct BattleMove *bm);     // applied to every move data read
void LONG_CALL RH_TransformLearnset(u32 sp, u32 *data);               // level-up learnset (u32 level << 16 | move)
void LONG_CALL RH_TransformEggMoves(u32 sp, u16 *moves, u32 max);
u32 LONG_CALL RH_MachineMove(u32 index, u32 vanilla);                 // TM move (index = item - ITEM_TM001)
BOOL LONG_CALL RH_MachineCompat(u32 sp, u32 index, BOOL vanilla);     // TM / HM compatibility
void LONG_CALL RH_TutorMoves(const u16 *vanilla, u16 *out, u32 count);
BOOL LONG_CALL RH_TutorCompat(u32 sp, u32 slot, u32 move, BOOL vanilla);              // applied to every evolution data read (rh_evos.c)

// ---- evolutions (src/rh/rh_species.c) ----
u32 LONG_CALL RH_EvolveOnce(u32 sp, u32 hash);                          // a (current) evolution of sp, 0 = none
u32 LONG_CALL RH_EvolveTimes(u32 sp, u32 times);
u32 LONG_CALL RH_HeapFree(u32 heapId);                                  // 0 if the heap doesn't exist
u32 LONG_CALL RH_SessionHeap(void);                                     // a heap with room for temporary tables

// ---- wild (src/rh/rh_wild.c) ----
u32 LONG_CALL RH_WildForm(u32 species);                                 // form picked for this species on this map
u32 LONG_CALL RH_ModifyWildLevel(u32 level);
struct EncounterData;
void LONG_CALL RH_TransformEncounters(u32 bank, struct EncounterData *d);   // a map's table as it loads

#endif // RH_INTERNAL_H
