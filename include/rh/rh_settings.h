#ifndef RH_SETTINGS_H
#define RH_SETTINGS_H
// Romhack (HeartGold Expansion Randomizer): settings chosen on the options screen at New Game.
// Same option set as the FireRed/Emerald versions (modelled on Universal Pokemon Randomizer FVX).
// Stored in the save (struct SAVE_MISC_DATA, field rh). Every field is a small number so the options menu
// can edit it generically.

#include "types.h"

enum { RH_POOL_GEN1, RH_POOL_GEN1_3, RH_POOL_ALL, RH_POOL_ALL_FORMS, RH_POOL_JOHTO };

enum { RH_THEME_DEFAULT, RH_THEME_RANDOMIZER, RH_THEME_AMOLED };

#define RH_SEED_TEXT_LENGTH 10
#define RH_SETTINGS_MAGIC   0x52480001 // "RH" + layout version

struct RhSettings
{
    u32 magic;                    // RH_SETTINGS_MAGIC when the settings were set up by the options screen
    u32 seed;                     // hash of seedText
    u16 seedText[RH_SEED_TEXT_LENGTH + 1]; // the seed as typed (ASCII), 0-terminated
    u8 enabled;
    // ---- General ----
    u8 mechanicsGen;              // 1-9 (battle mechanics generation)
    u8 speciesPool;               // RH_POOL_*
    u8 nuzlocke;                  // gives the Infinite Candy + Healing Kit key items
    u8 noPrematureEvos;
    u8 randomIntroMon;

    // ---- Pokemon Traits ----
    u8 baseStats;                 // 0 unchanged, 1 shuffle, 2 random
    u8 baseStatsFollowEvos;
    u8 baseStatsRandomAdded;
    u8 statsFollowMegas;
    u8 bstMode;                   // 0 unchanged, 1 random buff/nerf %, 2 shuffle, 3 random
    u8 bstChangePct;
    u8 bstFollowEvos, bstSeparateLegends;
    u8 updateBaseStatsGen;        // 0 off, else generation 1-9
    u8 expCurve;                  // 0 off, else 1 Medium Fast, 2 Medium Slow, 3 Fast, 4 Slow, 5 Erratic, 6 Fluctuating
    u8 expCurveWho;               // 0 Legendaries: Slow, 1 Strong Legendaries: Slow, 2 All Pokemon
    u8 types;                     // 0 unchanged, 1 random (follow evos), 2 random (completely)
    u8 forceDualTypes;
    u8 typesFollowMegas;
    u8 abilities;                 // 0 unchanged, 1 random
    u8 allowWonderGuard, combineDuplicateAbilities, ensureTwoAbilities, abilitiesFollowEvos, abilitiesFollowMegas;
    u8 banTrapAbilities, banNegativeAbilities, banBadAbilities;
    u8 evolutions;                // 0 unchanged, 1 random, 2 random every level
    u8 evoSimilarStrength, evoSameTyping, evoLimitThreeStages, evoNoConvergence, evoForceChange, evoForceGrowth;
    u8 evoChangeImpossible;
    u8 evoMakeEasier;             // 0 off, else max evolution level (30-55)
    u8 evoEstimatedLevels, evoRemoveTimeBased, evoAdjustLevels;

    // ---- Type Effectiveness ----
    u8 typeChart;                 // 0 unchanged, 1 random, 2 random (balanced), 3 keep type identities, 4 inverse
    u8 updateTypeChart;
    u8 inverseRandomImmunities;

    // ---- Starters, Statics & Trades ----
    u8 starters;                  // 0 unchanged, 1 custom, 2 random (completely), 3 random basic 2-evo, 4 random incl. regional
    u16 customStarters[3];        // 0 = the game's own starter
    u8 starterAllowAltFormes, starterNoLegends, starterHeldItems, starterBanBadItems;
    u8 starterBstMinOn, starterBstMaxOn;
    u16 starterBstMin, starterBstMax;
    u8 starterTypes;              // 0 none, 1 fire/water/grass, 2 any type triangle, 3 unique, 4 single type
    u8 starterSingleType;         // TYPE_* (0xFF = random)
    u8 starterNoDualTypes;
    u8 statics;                   // 0 unchanged, 1 swap legends & standards, 2 random completely, 3 random similar strength
    u8 staticRandomize600, staticAllowAltFormes, staticFixMusic, staticLevelModOn;
    s16 staticLevelMod;           // -100..150 %
    u8 trades;                    // 0 unchanged, 1 given only, 2 both
    u8 tradeNicknames, tradeOTs, tradeIVs, tradeItems;

    // ---- Moves & Movesets ----
    u8 movePower, moveAccuracy, movePP, moveType, moveCategory, moveNames;
    u8 updateMovesGen;            // 0 off, else generation 1-9
    u8 movesets;                  // 0 unchanged, 1 random prefer type, 2 random completely, 3 metronome only
    u8 guaranteedLevel1On, guaranteedLevel1Moves;
    u8 reorderDamagingMoves, evolutionMovesForAll, movesetNoGameBreaking;
    u8 movesetGoodDamagingOn, movesetGoodDamaging;

    // ---- Foe Pokemon ----
    u8 trainers;                  // 0 unchanged, 1 random, 2 random even, 3 type themed, 4 type themed (E4/gyms), 5 keep themed, 6 keep themes or primary
    u8 betterMovesets[3];         // boss, important, regular
    u8 additionalMons[3];
    u8 heldItemsFor[3];
    u8 heldConsumableOnly, heldSensible, heldHighestOnly;
    u8 diverseTypes[3];
    u8 battleStyle, battleStyleDoubles;
    u8 rivalCarriesTeam, rivalSameTeam;
    u8 trainerSimilarStrength, trainerAvoidDuplicates, trainerWeightTypes, trainerLocalPokemon;
    u8 trainerAllowAltFormes, trainerRandomShiny, trainerNoLegends, noEarlyWonderGuard;
    u8 leagueUnique;
    u8 randomTrainerNames, randomTrainerClassNames;
    u8 trainersEvolveOn, trainersEvolveLevel;
    u8 trainerLevelModOn;
    s16 trainerLevelMod;

    // ---- Wild Pokemon ----
    u8 wild;
    u8 wildZone;                  // 0 1 in whole game, 1 per named location, 2 per encounter set, 3 maximum possible, 4 completely random always
    u8 wildSplitEncounterTypes;
    u8 wildTypeRestriction, wildKeepThemes;
    u8 wildEvoRestriction, wildKeepRelations;
    u8 wildAllowAltFormes, wildNoLegends;
    u8 wildCatchRateOn, wildCatchRate;
    u8 wildHeldItems, wildBanBadItems, wildCatchEmAll, wildSimilarStrength, wildBalanceLowLevel, wildMegas;
    u8 wildLevelModOn;
    s16 wildLevelMod;

    // ---- TMs & Tutors ----
    u8 tmMoves, tmNoGameBreaking, tmKeepFieldMoves, tmGoodDamagingOn, tmGoodDamaging;
    u8 tmCompat, tmLevelupSanity, tmCompatFollowEvos, fullHMCompat;
    u8 tutorMoves, tutorNoGameBreaking, tutorGoodDamagingOn, tutorGoodDamaging;
    u8 tutorCompat, tutorLevelupSanity, tutorCompatFollowEvos;

    // ---- Items ----
    u8 fieldItems, fieldBanBad;
    u8 shopItems, shopBanBad, shopBanRegular, shopBanOverpowered, shopGuaranteeEvo, shopGuaranteeX;
    u8 shopBalancePrices, shopAddCheapRareCandy, shopSpecial;
    u8 pickupItems, pickupBanBad;

    // ---- Custom Player Graphics ----
    u8 playerGraphics;            // 0 = default, else a pack
    u8 playerGraphicsReplace;     // 0 boy & girl, 1 boy only, 2 girl only
    u8 paletteMode, paletteFollowTypes, paletteFollowEvos, paletteShinyFromNormal;

    // ---- Misc. Tweaks ----
    u8 uiTheme, instantText, randomPcPotion, nationalDexAtStart, fastEggs, lowerCaseNames;
    u8 randomCatchTutorial, banLuckyEgg, balanceStaticLevels, runWithoutShoes, reusableTMs, forgettableTMs, noEVs;

    u8 reserved[16];
};

#endif // RH_SETTINGS_H
