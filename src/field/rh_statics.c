// Romhack: Static Pokemon in the field scripts: gifts (vanilla script command 137, ScrCmd_GiveMon, is replaced), wild
// battles and eggs (src/field/script_commands.c) go through RH_FieldStaticSpecies.
#include "../../include/types.h"
#include "../../include/pokemon.h"
#include "../../include/save.h"
#include "../../include/script.h"
#include "../../include/npc_trade.h"
#include "../../include/rh/rh_internal.h"
#include "../../include/rh/rh_statics.h"

#define HEAPID_FIELD2 11

u32 RH_CurrentMap(FieldSystem *fsys)
{
    return fsys != NULL && fsys->location != NULL ? (u32)fsys->location->mapId : 0;
}

// the replacement of a static Pokemon on the current map (species | form << 11 in, same out)
u32 RH_FieldStaticSpecies(FieldSystem *fsys, u32 species)
{
    return RH_StaticSpecies(species, RH_CurrentMap(fsys));
}

BOOL ScrCmd_GiveMon(SCRIPTCONTEXT *ctx)
{
    FieldSystem *fsys = ctx->fsys;
    u32 mapsec = MapHeader_GetMapSec(RH_CurrentMap(fsys));
    u32 species = ScriptGetVar(ctx);
    u32 level = ScriptGetVar(ctx);
    u16 heldItem = ScriptGetVar(ctx);
    u32 form = ScriptGetVar(ctx);
    u16 ability = ScriptGetVar(ctx);
    u16 *ret = ScriptGetVarPointer(ctx);
    // romhack: Static Pokemon (gifts and Game Corner prizes)
    u32 r = RH_FieldStaticSpecies(fsys, species | (form << 11));
    if (r != (species | (form << 11)))
    {
        species = r & 0x7FF;
        form = r >> 11;
    }
    level = RH_StaticLevel(level);
    *ret = GiveMon(HEAPID_FIELD2, fsys->savedata, species, level, form, ability, heldItem, mapsec, 24);
    return FALSE;
}
