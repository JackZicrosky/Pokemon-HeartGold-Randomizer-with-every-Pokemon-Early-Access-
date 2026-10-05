// Prints the byte offset of every RhSettings field (for the test routes' rh:OFFSET:VALUE steps).
// Build: arm-none-eabi-gcc -I include -c ... is not needed; this is compiled for the host by gen_offsets.sh.
#include <stdio.h>
#include <stddef.h>
#include <stdint.h>
typedef uint8_t u8; typedef uint16_t u16; typedef uint32_t u32; typedef int16_t s16;
#define TYPES_H
#include "rh/rh_settings.h"
#define F(x) printf("%s %u\n", #x, (unsigned)offsetof(struct RhSettings, x));
int main(void)
{
    F(enabled) F(speciesPool) F(baseStats) F(bstMode) F(types) F(abilities) F(evolutions) F(starters)
    F(trainers) F(wild) F(wildZone) F(wildLevelModOn) F(wildLevelMod) F(movesets) F(instantText) F(uiTheme)
    F(fieldItems) F(shopItems) F(tmMoves) F(wildAllowAltFormes) F(wildMegas) F(wildCatchEmAll) F(statics)
    F(trades) F(trainerLevelModOn) F(trainerLevelMod) F(nuzlocke) F(typeChart) F(expCurve) F(wildSimilarStrength)
    printf("size %u\n", (unsigned)sizeof(struct RhSettings));
    return 0;
}
