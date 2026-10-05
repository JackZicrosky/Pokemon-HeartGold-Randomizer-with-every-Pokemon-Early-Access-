#ifndef RH_STATICS_H
#define RH_STATICS_H
// Romhack: Static Pokemon in the field scripts (src/field/rh_statics.c).

#include "types.h"

typedef struct FieldSystem FieldSystem;

u32 RH_CurrentMap(FieldSystem *fsys);
u32 RH_FieldStaticSpecies(FieldSystem *fsys, u32 species);     // species | form << 11 in and out

#endif // RH_STATICS_H
