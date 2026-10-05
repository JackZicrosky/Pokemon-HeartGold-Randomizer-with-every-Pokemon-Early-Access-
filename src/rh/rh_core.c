// Romhack randomizer core (overlay 152, resident in ITCM). See include/rh/rh.h.
#include "../../include/types.h"
#include "../../include/save.h"
#include "../../include/constants/file.h"
#include "../../include/rh/rh.h"

struct RhDebug gRhDebug = { .magic = 0x52484442 };

struct RhSettings *RH_Settings(void)
{
    return &Sav2_Misc_get(SaveBlock2_get())->rh;
}

BOOL RH_Active(void)
{
    struct RhSettings *s = RH_Settings();
    return s->magic == RH_SETTINGS_MAGIC && s->enabled;
}

// 32-bit mix (lowbias32), keyed on the run's seed.
static u32 Mix(u32 x)
{
    x ^= x >> 16;
    x *= 0x7feb352d;
    x ^= x >> 15;
    x *= 0x846ca68b;
    x ^= x >> 16;
    return x;
}

u32 RH_Hash(u32 salt, u32 a, u32 b)
{
    return Mix(Mix(Mix(RH_Settings()->seed ^ salt) + a) + b);
}

u32 RH_Random(u32 salt, u32 a, u32 b, u32 n)
{
    return n ? RH_Hash(salt, a, b) % n : 0;
}

// Replaces ReadWholeNarcMemberByIdPair (arm9 0x02007508): read as before, then let the randomizer
// transform the data it cares about.
void RH_ReadWholeNarcMemberByIdPair(void *dest, u32 narcId, s32 fileId)
{
    ReadFromNarcMemberByIdPair(dest, narcId, fileId, 0, 0);
    gRhDebug.narcReads++;
    if (narcId == ARC_PERSONAL) {
        gRhDebug.personalReads++;
        gRhDebug.lastPersonal = fileId;
    }
}
