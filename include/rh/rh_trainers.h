#ifndef RH_TRAINERS_H
#define RH_TRAINERS_H
// Romhack: Foe Pokemon randomizer (src/field/rh_trainers.c, field overlay). Called by MakeTrainerPokemonParty
// (src/field/enemy_party.c) while it builds a trainer's party.

#include "types.h"

// what RH_TrainerMon changed / wants applied
#define RHTM_ITEM        0x01    // give "item"
#define RHTM_MOVES       0x02    // set "moves" (otherwise the species' default moves)
#define RHTM_ABILITY     0x04    // set "ability" (otherwise the ability slot's ability)
#define RHTM_NO_NICKNAME 0x08    // the species changed: drop the trainer's nickname
#define RHTM_SHINY       0x10

struct RhTrainerMon
{
    u16 species;                 // species number (no form bits)
    u16 form;
    u16 level;
    u16 item;
    u16 ability;
    u16 moves[4];
    u8 ivs;                      // the trainer data's IV byte (0-255)
    u8 abilitySlot;
    u8 flags;                    // RHTM_*
};

// Starts a party: returns how many Pokemon the trainer gets (count + Additional Pokemon), or count when the
// randomizer leaves the team alone. buf = the trainer's party file, dataType = its TRAINER_DATA_TYPE_* bits.
u32 RH_TrainerPartyBegin(u32 trainerId, u32 dataType, const u8 *buf, u32 count, u32 fightType);
// Changes slot "slot" (in: the trainer data's values with flags set for the fields it has; slots >= count are
// Additional Pokemon and are filled in completely).
void RH_TrainerMon(u32 slot, struct RhTrainerMon *m);
// Where slot "slot" goes in the party (Additional Pokemon go before the team's last Pokemon, its ace).
u32 RH_TrainerPartyOrder(u32 slot);
void RH_TrainerPartyEnd(void);

#endif // RH_TRAINERS_H
