// Romhack: Items (port of the FireRed/Emerald rh_items.c): field items (item balls + hidden items), and the
// romhack's script command (vanilla script command 490, a no-op, is replaced by ScrCmd_RhSpecial).
// The item pool (a028 member 24) and the field item list (member 25) come from tools/rh/gen_rh_items.py; they are
// read from the ROM when needed (the field overlay has no room to keep them).
#include "../../include/types.h"
#include "../../include/item.h"
#include "../../include/pokemon.h"
#include "../../include/save.h"
#include "../../include/script.h"
#include "../../include/constants/file.h"
#include "../../include/constants/item.h"
#include "../../include/rh/rh_internal.h"
#include "../../include/rh/rh_items.h"

#define IS_TM(item) ((item) >= ITEM_TM001 && (item) <= ITEM_TM092)

// ---------------------------------------------------------------------------
// The tables
// ---------------------------------------------------------------------------
struct Table { u16 count; void *data; };

static void *LoadTable(u32 member, u32 size, u16 *count)
{
    u16 hdr[2];
    void *p;
    ReadFromNarcMemberByIdPair(hdr, ARC_CODE_ADDONS, member, 0, 4);
    *count = hdr[0];
    p = sys_AllocMemory(RH_SessionHeap(), hdr[0] * size + 4);
    if (p != NULL)
        ReadFromNarcMemberByIdPair(p, ARC_CODE_ADDONS, member, 4, hdr[0] * size);
    else
        *count = 0;
    return p;
}

static void FreeTable(void *p)
{
    if (p != NULL)
        sys_FreeMemoryEz(p);
}

BOOL RH_ItemBanned(u32 item)
{
    return item == ITEM_LUCKY_EGG && S->banLuckyEgg;
}

static BOOL PoolItemOk(const struct RhItem *it, BOOL banBad)
{
    if (RH_ItemBanned(it->item) || (it->pocket & RHP_SHOP_ONLY))
        return FALSE;
    if (banBad && (it->flags & RHI_BAD))
        return FALSE;
    return TRUE;
}

// ---------------------------------------------------------------------------
// Field items (item balls + hidden items)
// ---------------------------------------------------------------------------
static BOOL InPool(const struct RhItem *pool, u32 n, u32 item)
{
    s32 lo = 0, hi = (s32)n - 1;
    while (lo <= hi)
    {
        s32 mid = (lo + hi) / 2;
        if (pool[mid].item == item)
            return TRUE;
        if (pool[mid].item < item)
            lo = mid + 1;
        else
            hi = mid - 1;
    }
    return FALSE;
}

static const struct RhItem *PoolEntry(const struct RhItem *pool, u32 n, u32 item)
{
    s32 lo = 0, hi = (s32)n - 1;
    while (lo <= hi)
    {
        s32 mid = (lo + hi) / 2;
        if (pool[mid].item == item)
            return &pool[mid];
        if (pool[mid].item < item)
            lo = mid + 1;
        else
            hi = mid - 1;
    }
    return NULL;
}

// Items the randomizer leaves alone: key items, HMs, anything outside the pool (Mega Stones, quest items...).
static BOOL Protected(const struct RhItem *pool, u32 n, u32 item)
{
    return item == ITEM_NONE || (!IS_TM(item) && !InPool(pool, n, item));
}

// Shuffle keeps the game's own items (FVX: "Ban Bad Items" only affects the random modes).
static BOOL FieldPoolItemOk(const struct RhItem *pool, u32 n, u32 item)
{
    const struct RhItem *e = PoolEntry(pool, n, item);
    return !Protected(pool, n, item) && !IS_TM(item) && !RH_ItemBanned(item) && !(e != NULL && (e->pocket & RHP_SHOP_ONLY));
}

static BOOL KindOk(const struct RhItem *pool, u32 n, u32 item, BOOL tms)
{
    return tms ? IS_TM(item) : FieldPoolItemOk(pool, n, item);
}

// k-th field item (in a keyed order) of its kind: different k never share an entry.
static s32 RankedFieldEntry(const struct RhFieldItem *f, u32 nf, const struct RhItem *pool, u32 n, u32 k, BOOL tms)
{
    u32 p, count = 0, accepted = 0;
    for (p = 0; p < nf; p++)
        accepted += KindOk(pool, n, f[p].item, tms);
    if (accepted == 0)
        return -1;
    k = RH_Permute(SALT_FIELD_ITEM, k % accepted, accepted);
    for (p = 0; p < nf; p++)
        if (KindOk(pool, n, f[p].item, tms) && count++ == k)
            return p;
    return -1;
}

// Rank of field entry i among the spots of its own kind (TM or not; every spot counts, so no two spots share a
// rank even when their original item is banned).
static u32 FieldRank(const struct RhFieldItem *f, const struct RhItem *pool, u32 n, u32 i, BOOL tm)
{
    u32 p, r = 0;
    for (p = 0; p < i; p++)
        if (!Protected(pool, n, f[p].item) && IS_TM(f[p].item) == tm)
            r++;
    return r;
}

static u32 FieldItem(u32 item, u32 key, const struct RhItem *pool, u32 n, const struct RhFieldItem *f, u32 nf)
{
    u32 i, x, guard, tries;
    BOOL isTM = IS_TM(item);
    s32 e;
    if (Protected(pool, n, item))
        return item;
    for (i = 0; i < nf; i++)
        if (f[i].key == key)
            break;
    if (isTM || S->fieldItems == 1)
    {
        // Shuffle: the game's own items move around (TMs stay in TM spots, only their numbers move); in the random
        // modes the TMs found in the field are placed first (FVX getRequiredFieldTMs), so none becomes unobtainable
        if (i == nf)
            return item;
        e = RankedFieldEntry(f, nf, pool, n, FieldRank(f, pool, n, i, isTM), isTM);
        return e >= 0 ? f[e].item : item;
    }
    if (S->fieldItems == 3)
    {
        // random, every item about equally often
        x = (i == nf) ? key % n : i;
        for (guard = 0; guard < 128; guard++)
        {
            x = RH_Permute(SALT_FIELD_ITEM, x, n);
            if (PoolItemOk(&pool[x], S->fieldBanBad))
                return pool[x].item;
        }
        return item;
    }
    for (tries = 0; tries < 48; tries++)
    {
        const struct RhItem *it = &pool[RH_Hash(SALT_FIELD_ITEM, key, tries) % n];
        if (PoolItemOk(it, S->fieldBanBad))
            return it->item;
    }
    return item;
}

u32 RH_FieldItem(u32 item, u32 key)
{
    u16 n, nf;
    struct RhItem *pool;
    struct RhFieldItem *f;
    if (!RH_Active() || S->fieldItems == 0)
        return item;
    pool = LoadTable(CODE_ADDON_RH_ITEMS, sizeof(struct RhItem), &n);
    f = LoadTable(CODE_ADDON_RH_FIELD_ITEMS, sizeof(struct RhFieldItem), &nf);
    if (pool != NULL && f != NULL && n > 0)
        item = FieldItem(item, key, pool, n, f, nf);
    FreeTable(f);
    FreeTable(pool);
    return item;
}

// ---------------------------------------------------------------------------
// The romhack's script command: "nop_var_490 VAR" with VAR = RH_SPECIAL_* (other inputs in VAR_SPECIAL_x8004..)
// ---------------------------------------------------------------------------
void LONG_CALL SetupNativeScript(SCRIPTCONTEXT *ctx, BOOL (*fn)(SCRIPTCONTEXT *ctx));
BOOL LONG_CALL ScrNative_WaitApplication(SCRIPTCONTEXT *ctx);

static BOOL ShopsRandomized(void)
{
    return RH_Active() && (S->shopItems != 0 || S->shopSpecial);
}

BOOL ScrCmd_RhSpecial(SCRIPTCONTEXT *ctx)
{
    u16 *var = ScriptGetVarPointer(ctx);
    switch (*var)
    {
    case RH_SPECIAL_ITEM_BALL:
    {
        // item ball: VAR_SPECIAL_x8004 = its item, VAR_SPECIAL_x800A = its number (scr_seq 141)
        u16 *item = GetVarPointer(ctx->fsys, 0x8004);
        *item = RH_FieldItem(*item, VarGet(ctx->fsys, 0x800A));
        break;
    }
    case RH_SPECIAL_OPEN_SETTINGS:
        // the Randomizer Settings key item: the options screen in-game; the script waits for it to close
        RH_OpenSettingsFromField(ctx->fsys);
        SetupNativeScript(ctx, ScrNative_WaitApplication);
        return TRUE;
    case RH_SPECIAL_MASTER_BALL_OFFER:
        // Kurt offers his one Master Ball himself when the shops aren't randomized (else it's in his shop)
        *GetVarPointer(ctx->fsys, 0x800C) = !ShopsRandomized();
        break;
    }
    return FALSE;
}

// ---------------------------------------------------------------------------
// Shops (FVX "Shop Items"): the specialty marts' stock; the regular clerks (balls, medicine, repels) stay.
// ---------------------------------------------------------------------------
// A slot the Shop Items option may change (not balls / medicine / repels, key items, guaranteed items).
static BOOL ShopSlotRandomizable(const struct RhItem *pool, u32 n, u32 item)
{
    const struct RhItem *e = PoolEntry(pool, n, item);
    if (item == ITEM_NONE || item == 0xFFFF || Protected(pool, n, item))
        return FALSE;
    if (e != NULL && (e->flags & RHI_REGULAR))
        return FALSE;
    if (e != NULL && S->shopGuaranteeEvo && (e->flags & RHI_EVO))
        return FALSE;
    if (e != NULL && S->shopGuaranteeX && (e->flags & RHI_X))
        return FALSE;
    return TRUE;
}

static BOOL ShopItemAllowed(const struct RhItem *it)
{
    if (RH_ItemBanned(it->item) || (it->flags & RHI_SELL_ONLY))
        return FALSE;
    if (S->shopBanBad && (it->flags & RHI_BAD))
        return FALSE;
    if (S->shopBanRegular && (it->flags & RHI_REGULAR))
        return FALSE;
    if (S->shopBanOverpowered && (it->flags & RHI_OVERPOWERED))
        return FALSE;
    if (S->nuzlocke && it->item == ITEM_RARE_CANDY)
        return FALSE;                                        // Nuzlocke Mode: no Rare Candy in any shop
    return TRUE;
}

// Shuffle: the g-th randomizable slot of all the specialty marts gets the item of slot Permute(g).
static u32 ShuffledShopItem(const u16 *const *lists, u32 count, const struct RhItem *pool, u32 n, u32 which, u32 slot, u32 item)
{
    u32 m, i, g = 0, total = 0, target, mine = 0xFFFF;
    for (m = 0; m < count; m++)
        for (i = 0; lists[m][i] != 0xFFFF; i++)
            if (ShopSlotRandomizable(pool, n, lists[m][i]))
            {
                if (m == which && i == slot)
                    mine = total;
                total++;
            }
    if (mine == 0xFFFF || total == 0)
        return item;
    target = RH_Permute(SALT_SHOP, mine, total);
    for (m = 0; m < count; m++)
        for (i = 0; lists[m][i] != 0xFFFF; i++)
            if (ShopSlotRandomizable(pool, n, lists[m][i]) && g++ == target)
                return lists[m][i];
    return item;
}

static u32 ShopItem(const u16 *const *lists, u32 count, const struct RhItem *pool, u32 n, u32 which, u32 slot, u32 item)
{
    u32 tries;
    if (!ShopSlotRandomizable(pool, n, item))
        return item;
    if (S->shopItems == 1)
        return ShuffledShopItem(lists, count, pool, n, which, slot, item);
    for (tries = 0; tries < 48; tries++)
    {
        const struct RhItem *it = &pool[RH_Hash(SALT_SHOP, which * 256 + slot, tries) % n];
        if (ShopItemAllowed(it))
            return it->item;
    }
    return item;
}

void RH_FilterMart(u16 *items, u32 max)
{
    u32 i, n = 0;
    BOOL candy = FALSE;
    if (!RH_Active())
        return;
    for (i = 0; items[i] != 0xFFFF && i < max; i++)
    {
        if (S->nuzlocke && items[i] == ITEM_RARE_CANDY)
            continue;
        candy |= items[i] == ITEM_RARE_CANDY;
        items[n++] = items[i];
    }
    if (S->shopAddCheapRareCandy && !S->nuzlocke && !candy && n + 1 < max)
        items[n++] = ITEM_RARE_CANDY;                        // FVX "Add Cheap Rare Candies"
    items[n] = 0xFFFF;
}

void RH_BuildMart(const u16 *const *lists, u32 count, u32 which, u16 *out, u32 max)
{
    const u16 *list = lists[which];
    u32 i, j, k = 0;
    u16 n = 0;
    struct RhItem *pool = NULL;
    if (RH_Active() && S->shopItems != 0)
        pool = LoadTable(CODE_ADDON_RH_ITEMS, sizeof(struct RhItem), &n);
    for (i = 0; list[i] != 0xFFFF && k + 2 < max; i++)
    {
        u32 item = (pool != NULL && n > 0) ? ShopItem(lists, count, pool, n, which, i, list[i]) : list[i];
        for (j = 0; j < k && out[j] != item; j++)
            ;
        if (j == k)
            out[k++] = item;                                 // no item twice in one mart
    }
    out[k] = 0xFFFF;
    FreeTable(pool);
    RH_FilterMart(out, max);
}

// ---------------------------------------------------------------------------
// The romhack's special shops (port of the FireRed/Emerald RH_BuildSpecialShop). With "Randomize Special Shops"
// every slot of every special shop gets a different item; with "Guarantee Evolution Items" each evolution item
// is sold in one of them. "Guarantee X Items" keeps the training counter as it is.
// ---------------------------------------------------------------------------
static BOOL SpecialListKept(u32 list)
{
    return list == RH_SPECIAL_TRAINING && S->shopGuaranteeX;
}

static BOOL IsKeptSpecialItem(const u16 *d, u32 item)
{
    u32 l, i;
    for (l = 0; l < RH_SPECIAL_COUNT; l++)
        if (SpecialListKept(l))
            for (i = d[l]; i < d[l + 1]; i++)
                if (d[i] == item)
                    return TRUE;
    return FALSE;
}

static u32 RandomSpecialShop(const u16 *d, u32 list, u16 *out, u32 max)
{
    u16 np = 0;
    struct RhItem *pool = LoadTable(CODE_ADDON_RH_ITEMS, sizeof(struct RhItem), &np);
    u16 *acc = pool != NULL ? sys_AllocMemory(RH_SessionHeap(), np * sizeof(u16) + 4) : NULL;
    u32 l, p, j, k, n = 0, accepted = 0, evoCount = 0, total = 0, offset = d[list] - d[0];
    u32 len = d[list + 1] - d[list];
    for (l = 0; l < RH_SPECIAL_COUNT; l++)
        if (!SpecialListKept(l))
            total += d[l + 1] - d[l];
    for (k = 0; k < 2 && acc != NULL; k++)                  // the evolution items first (Guarantee Evolution Items)
        for (p = 0; p < np; p++)
        {
            BOOL evo = S->shopGuaranteeEvo && (pool[p].flags & RHI_EVO);
            if ((k == 0) == evo && ShopItemAllowed(&pool[p]) && !IsKeptSpecialItem(d, pool[p].item))
            {
                acc[accepted++] = pool[p].item;
                evoCount += evo;
            }
        }
    for (j = 0; j < len && accepted != 0 && n + 1 < max; j++)
    {
        u32 r;
        if (evoCount != 0 && total >= evoCount && accepted > evoCount)
        {
            r = RH_Permute(SALT_SPECIAL_SHOP, (offset + j) % total, total);
            k = r < evoCount ? r : evoCount + (r - evoCount) % (accepted - evoCount);
        }
        else
            k = RH_Permute(SALT_SPECIAL_SHOP, (offset + j) % accepted, accepted);
        out[n++] = acc[k];
    }
    FreeTable(acc);
    FreeTable(pool);
    return n;
}

void RH_BuildSpecialShop(u32 list, u16 *out, u32 max)
{
    u16 count = 0;
    u16 *d = LoadTable(CODE_ADDON_RH_SPECIAL_SHOPS, sizeof(u16), &count);
    u32 i, n = 0;
    if (list >= RH_SPECIAL_COUNT)
        list = 0;
    if (d != NULL && RH_Active() && S->shopSpecial && !SpecialListKept(list))
        n = RandomSpecialShop(d, list, out, max);
    else if (d != NULL)
        for (i = d[list]; i < d[list + 1] && n + 1 < max; i++)
            if (d[i] != ITEM_MASTER_BALL || ShopsRandomized())     // Kurt offers the one Master Ball himself
                out[n++] = d[i];
    out[n] = 0xFFFF;
    FreeTable(d);
    RH_FilterMart(out, max);
}

// ---------------------------------------------------------------------------
// GetPartySlotWithMove (script command 141): through GetIdxOfFirstPartyMonWithMove, so the HM Kit counts
// (src/item.c RH_GetIdxOfFirstPartyMonWithMove)
// ---------------------------------------------------------------------------
u8 LONG_CALL GetIdxOfFirstPartyMonWithMove(void *party, u16 move);
void *LONG_CALL SaveArray_Party_Get(void *saveData);

BOOL ScrCmd_GetPartySlotWithMove(SCRIPTCONTEXT *ctx)
{
    u16 *slot = ScriptGetVarPointer(ctx);
    u16 move = ScriptGetVar(ctx);
    u8 i = GetIdxOfFirstPartyMonWithMove(SaveArray_Party_Get(ctx->fsys->savedata), move);
    *slot = (i == 0xFF) ? 6 : i;
    return FALSE;
}
