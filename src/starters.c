#include "../include/constants/species.h"
#include "../include/pokemon.h"
#include "../include/types.h"
#include "../include/rh/rh_coretable.h"

extern u32 space_for_setmondata;
extern u32 sStarterChoiceCries[];

void LONG_CALL GetMonSpriteCharAndPlttNarcIdsEx(MON_PIC *picdata, u16 mons_no, u8 dir, u8 col, u8 form_no, u8 a5, u32 personality);

// Use MON_WITH_FORM(SPECIES_NAME, form) to specify a starter
// with a form.
//
// Keep in mind that these choices will impact Rival team
// determination in the appropriate trigger-scripts.
//
// This will NOT update the text during the starter-selection
// sequence. To update that text, modify text archive 190 in
// DSPRE.
static const u16 sStarterChoices[3] = {
    SPECIES_CHIKORITA,
    SPECIES_CYNDAQUIL,
    SPECIES_TOTODILE,
};

// romhack: the starter in each of Elm's balls comes from the randomizer (the game's own starters when the
// "Starters" option is Unchanged). species | form << 11, like sStarterChoices.
static u16 StarterChoice(int slot)
{
    u16 species, form;
    RHCT_SpeciesKey(RHCT_StarterForSlot(slot), &species, &form);
    return species | (form << 11);
}

/**
 *  @brief fills the given array with the species ids of the starter choices
 *
 *  @param species pointer to the species array
 */
void LONG_CALL CreateStarter_SetStarterSpecies(int *species)
{
    for (int i = 0; i < 3; i++) {
        // strip off form
        species[i] = StarterChoice(i) & 0x7FF;
    }
}

/**
 *  @brief wrap the CreateMon call within CreateStarter to allow for starter forms
 *
 *  @param mon the PartyPokemon pointer
 *  @param species the species id
 *  @param slot starter choice (0-2)
 */
void LONG_CALL CreateStarter_CreateMon(struct PartyPokemon *mon, int species, int slot)
{
    u32 form = 0;

    if (slot >= 0 && slot < 3) {
        form = StarterChoice(slot) >> 11;
    }

    space_for_setmondata = form;
    PokeParaSet(mon, species, 5, 32, FALSE, 0, 0, 0);
    space_for_setmondata = 0;

    if (form != 0) {
        SetMonData(mon, MON_DATA_FORM, &form);
    }
}

/**
 *  @brief wrap the GetMonSpriteCharAndPlttNarcIdsEx call within createMonSprites to handle starter forms
 *
 *  @param pic MON_PIC pointer
 *  @param species species id
 */
void LONG_CALL CreateMonSprites_HandleForm(MON_PIC *pic, u16 species, u8 gender, u8 shiny, int slot)
{
    u32 form = 0;

    if (slot >= 0 && slot < 3) {
        form = StarterChoice(slot) >> 11;
        sStarterChoiceCries[slot] = (form == 0) ? species : PokeOtherFormMonsNoGet(species, form);
    }

    GetMonSpriteCharAndPlttNarcIdsEx(pic, species, gender, 2, shiny, 0, 0);

    if (form != 0) {
        GetOtherFormPic(pic, species, 2, shiny, form);
    }
}

// romhack: ScrCmd_GetStarterChoice (arm9 0x02043938). The game stores the species of the chosen starter, and the
// rival scripts pick Silver's team by comparing it with Chikorita / Cyndaquil (else: Totodile). With randomized
// starters it matches neither, so he always got the Totodile-player team (and with it, sometimes the player's own
// starter). This returns the game's own starter of the ball the player chose.
#define ScriptReadHalfword_     ((u16 (*)(void *))(0x0203FE2C | 1))
#define GetVarPointer_          ((u16 *(*)(void *, u16))(0x02040374 | 1))
#define Save_VarsFlags_Get_     ((void *(*)(void *))(0x020503D0 | 1))
#define Save_VarsFlags_GetStarter_ ((u16 (*)(void *))(0x02066AF0 | 1))

BOOL LONG_CALL RH_ScrCmd_GetStarterChoice(u8 *ctx)
{
    void *fsys = *(void **)(ctx + 0x80);
    u16 *ret = GetVarPointer_(fsys, ScriptReadHalfword_(ctx));
    u16 sp = Save_VarsFlags_GetStarter_(Save_VarsFlags_Get_(*(void **)((u8 *)fsys + 0x0C)));
    int i;
    for (i = 0; i < 3; i++)
        if ((StarterChoice(i) & 0x7FF) == sp) {
            sp = sStarterChoices[i];
            break;
        }
    *ret = sp;
    return FALSE;
}
