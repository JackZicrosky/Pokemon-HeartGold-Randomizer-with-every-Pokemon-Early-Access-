// Romhack randomizer core (overlay 152, resident in ITCM). See include/rh/rh.h.
#include "../../include/types.h"
#include "../../include/save.h"
#include "../../include/constants/file.h"
#include "../../include/rh/rh.h"
#include "../../include/rh/rh_internal.h"

struct RhDebug gRhDebug = { .magic = 0x52484442 };

// ---------------------------------------------------------------------------
// Settings
// ---------------------------------------------------------------------------
struct RhSettings *RH_Settings(void)
{
    static struct RhSettings sNone;                  // before the save exists: magic 0 = nothing set up
    void *save = SaveBlock2_get();
    if (save == NULL)
        return &sNone;
    return &Sav2_Misc_get(save)->rh;
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

// ---------------------------------------------------------------------------
// Misc. Tweaks: Instantaneous Text.
// Replaces the text printer task (arm9 0x020202EC, pret RunTextPrinter). With the option on, a message is
// drawn up to its next stop (button prompt, scroll, end) in one frame; the prompts still wait for A.
// ---------------------------------------------------------------------------
void LONG_CALL GenerateFontHalfRowLookupTable(u32 fg, u32 bg, u32 shadow);
u32 LONG_CALL RenderFont(void *printer);
void LONG_CALL CopyWindowToVram(void *window);
void LONG_CALL DestroyTextPrinter(u32 id);

#define TEXT_PRINTERS_DISABLED (*(u8 *)0x021D1F6C)
enum { RENDER_PRINT, RENDER_FINISH, RENDER_REPEAT, RENDER_UPDATE };

static BOOL InstantText(void)
{
    struct RhSettings *s = RH_Settings();
    return s->magic == RH_SETTINGS_MAGIC && s->instantText;
}

void RH_RunTextPrinter(void *task, u8 *printer)
{
    u8 (*callback)(void *, u16) = *(void **)(printer + 0x1C);
    (void)task;
    if (TEXT_PRINTERS_DISABLED)
        return;
    if (printer[0x2D] == 0)
    {
        BOOL instant = InstantText(), printed = FALSE;
        u32 r, guard = 0;
        *(u16 *)(printer + 0x2E) = 0;
        GenerateFontHalfRowLookupTable(printer[0x15], printer[0x16], printer[0x17]);
        for (;;)
        {
            if (instant)
                printer[0x2A] = 0;                // no delay before the next letter
            r = RenderFont(printer);
            if (r != RENDER_PRINT || !instant || ++guard > 2048)
                break;
            printed = TRUE;
        }
        if (printed && r != RENDER_PRINT)
            CopyWindowToVram(*(void **)(printer + 4));     // letters drawn by the instant-text loop
        switch (r)
        {
        case RENDER_PRINT:
            CopyWindowToVram(*(void **)(printer + 4));
            // fallthrough
        case RENDER_UPDATE:
            if (callback != NULL)
                printer[0x2D] = callback(printer, *(u16 *)(printer + 0x2E));
            break;
        case RENDER_FINISH:
            DestroyTextPrinter(printer[0x2C]);
            break;
        }
    }
    else
    {
        printer[0x2D] = callback(printer, *(u16 *)(printer + 0x2E));
    }
}

// Replaces ReadWholeNarcMemberByIdPair (arm9 0x02007508): read as before, then let the randomizer
// transform the data it cares about.
void RH_ReadWholeNarcMemberByIdPair(void *dest, u32 narcId, s32 fileId)
{
    ReadFromNarcMemberByIdPair(dest, narcId, fileId, 0, 0);
    gRhDebug.narcReads++;
    {
        // DEBUG: heap headroom
        static const u8 sHeaps[] = { 0, 1, 2, 3, 4, 5, 11 };
        u32 i;
        for (i = 0; i < sizeof(sHeaps); i++)
        {
            u32 *handles = *(u32 **)0x021D1584;               // sHeapInfo.heapHandles
            u8 *idxs = *(u8 **)(0x021D1584 + 0x10);           // sHeapInfo.heapIdxs
            if (handles != NULL && idxs != NULL && handles[idxs[sHeaps[i]]] != 0)
            {
                u32 f = GF_ExpHeap_FndGetTotalFreeSize(sHeaps[i]);
                if (gRhDebug.heapMinFree[sHeaps[i]] == 0 || f < gRhDebug.heapMinFree[sHeaps[i]])
                    gRhDebug.heapMinFree[sHeaps[i]] = f;
            }
        }
    }
    if (narcId == ARC_PERSONAL) {
        gRhDebug.personalReads++;
        gRhDebug.lastPersonal = fileId;
        RH_TransformPersonal(fileId, dest);
    }
    else if (narcId == 37)                                    // wild encounter table of a map (a/0/3/7)
    {
        RH_TransformEncounters(fileId, dest);
    }
}
