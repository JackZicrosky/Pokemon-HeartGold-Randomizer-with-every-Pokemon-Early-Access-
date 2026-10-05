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
    if (RH_ItemBanned(it->item))
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

// Items the randomizer leaves alone: key items, HMs, anything outside the pool (Mega Stones, quest items...).
static BOOL Protected(const struct RhItem *pool, u32 n, u32 item)
{
    return item == ITEM_NONE || (!IS_TM(item) && !InPool(pool, n, item));
}

// Shuffle keeps the game's own items (FVX: "Ban Bad Items" only affects the random modes).
static BOOL FieldPoolItemOk(const struct RhItem *pool, u32 n, u32 item)
{
    return !Protected(pool, n, item) && !IS_TM(item) && !RH_ItemBanned(item);
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
    }
    return FALSE;
}

// ---------------------------------------------------------------------------
// Shops (FVX "Shop Items"): the specialty marts' stock; the regular clerks (balls, medicine, repels) stay.
// ---------------------------------------------------------------------------
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
