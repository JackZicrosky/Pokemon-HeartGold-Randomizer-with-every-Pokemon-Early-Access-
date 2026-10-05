// Romhack randomizer core (overlay 152, resident in ITCM). See include/rh/rh.h.
#include "../../include/types.h"
#include "../../include/save.h"
#include "../../include/constants/file.h"
#include "../../include/rh/rh.h"

struct RhDebug gRhDebug = { .magic = 0x52484442 };

// ---------------------------------------------------------------------------
// Settings
// ---------------------------------------------------------------------------
struct RhSettings *RH_Settings(void)
{
    return &Sav2_Misc_get(SaveBlock2_get())->rh;
}

BOOL RH_Active(void)
{
    struct RhSettings *s = RH_Settings();
    return s->magic == RH_SETTINGS_MAGIC && s->enabled;
}

// The owner's defaults (same as RH_SetDefaultSettings in the FireRed/Emerald versions).
void RH_SetDefaultSettings(struct RhSettings *s)
{
    memset(s, 0, sizeof(*s));
    s->magic = RH_SETTINGS_MAGIC;
    s->enabled = TRUE;
    s->mechanicsGen = 9;
    s->speciesPool = RH_POOL_ALL;
    s->baseStatsFollowEvos = TRUE;
    s->abilitiesFollowEvos = TRUE;
    s->banTrapAbilities = TRUE;
    s->banNegativeAbilities = TRUE;
    s->updateTypeChart = TRUE;
    s->starterBstMin = 300;
    s->starterBstMax = 350;
    s->starterSingleType = 0xFF;          // (random)
    s->guaranteedLevel1Moves = 4;
    s->movesetNoGameBreaking = TRUE;
    s->movesetGoodDamaging = 50;
    s->noEarlyWonderGuard = TRUE;
    s->trainersEvolveLevel = 55;
    s->wildZone = 2;
    s->wildCatchRate = 1;
    s->wildMegas = TRUE;
    s->tmNoGameBreaking = TRUE;
    s->tmKeepFieldMoves = TRUE;
    s->tmGoodDamaging = 50;
    s->tutorNoGameBreaking = TRUE;
    s->tutorGoodDamaging = 50;
    s->fieldBanBad = TRUE;
    s->shopBanBad = TRUE;
    s->pickupBanBad = TRUE;
    s->randomIntroMon = TRUE;
    s->bstChangePct = 20;
    s->bstFollowEvos = TRUE;
    s->statsFollowMegas = TRUE;
    s->typesFollowMegas = TRUE;
    s->abilitiesFollowMegas = TRUE;
    s->seed = 1;
}

// ---------------------------------------------------------------------------
// Hashing
// ---------------------------------------------------------------------------
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
    return Mix(RH_Settings()->seed ^ Mix(salt * 0x9E3779B1 + a * 0x85EBCA77 + b * 0xC2B2AE3D + 0x27D4EB2F));
}

u32 RH_Random(u32 salt, u32 a, u32 b, u32 n)
{
    return n ? RH_Hash(salt, a, b) % n : 0;
}

// ---------------------------------------------------------------------------
// Screen flow: New Game -> randomizer options -> Professor Oak.
// Replaces RegisterMainOverlay (arm9 0x02000EF4), which only stores the next app for the main loop.
// ---------------------------------------------------------------------------
#define MAIN_NEXT_OVERLAY  (*(u32 *)0x02111870)
#define MAIN_NEXT_TEMPLATE (*(const void **)0x02111874)
#define APP_OAK_SPEECH     ((const void *)0x02106068)

static u8 sMenuShown;

void RH_RegisterMainOverlay(u32 ovyId, const void *template)
{
    if (template == APP_OAK_SPEECH && !sMenuShown) {
        // a new game: show the options first; the options screen registers Oak's speech when it closes
        ovyId = OVERLAY_RH_MENU;
        template = RH_MENU_APP_TEMPLATE;
    }
    sMenuShown = (template == RH_MENU_APP_TEMPLATE);
    MAIN_NEXT_OVERLAY = ovyId;
    MAIN_NEXT_TEMPLATE = template;
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
