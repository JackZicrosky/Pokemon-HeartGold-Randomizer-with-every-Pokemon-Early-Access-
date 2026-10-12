// Romhack: item prices (port of the FireRed/Emerald RH_ItemPrice in rh_items.c). Every price the game reads
// (GetItemData(item, ITEM_PARAM_PRICE), src/item.c) goes through RH_ItemPrice.
#include "../../include/types.h"
#include "../../include/constants/item.h"
#include "../../include/rh/rh.h"
#include "../../include/rh/rh_settings.h"
#include "../../include/rh/rh_internal.h"

// Prices for the Poke Balls the games never sell (Kurt sells them all).
static u32 ExtraBallPrice(u32 item)
{
    switch (item)
    {
    case ITEM_MASTER_BALL:  return 100000;
    case ITEM_CHERISH_BALL: return 3000;
    case ITEM_BEAST_BALL:   return 2000;
    case ITEM_DREAM_BALL:   return 1000;
    case ITEM_PARK_BALL:    return 1000;
    case ITEM_STRANGE_BALL: return 1000;
    case ITEM_SAFARI_BALL:  return 500;
    case ITEM_SPORT_BALL:   return 300;
    }
    return 0;
}

// "Balance Shop Prices" curve (owner's spec): prices up to 1000 stay; above that, the more expensive an item, the
// harder the cut (10000 -> ~6800, 20000 -> ~10500, 100000 -> ~22200, 250000 -> ~29500). sCurve[k] is the price
// for 1000 << k (1000 * e^(x - 0.07 x^2), x = ln(price / 1000)); prices in between are interpolated.
static const u16 sBalanceCurve[] = { 1000, 1933, 3497, 5911, 9346, 13806, 19069, 24632, 29730, 33580, 35450 };

static u32 BalancePrice(u32 price)
{
    u32 k, lo, out;
    if (price <= 1000)
        return price;
    for (k = 0; k < NELEMS(sBalanceCurve) - 1; k++)
    {
        lo = 1000u << k;
        if (price < (lo << 1))
        {
            out = sBalanceCurve[k] + (sBalanceCurve[k + 1] - sBalanceCurve[k]) * (price - lo) / lo;
            return (out + 5) / 10 * 10;
        }
    }
    return sBalanceCurve[NELEMS(sBalanceCurve) - 1];
}

#include "../../include/rh/rh_balanced_prices.h"

// UPR FVX's balanced price of an item (0 = not in the table)
static u32 BalancedPrice(u32 item)
{
    s32 lo = 0, hi = NELEMS(sRhBalancedPrices) - 1;
    while (lo <= hi)
    {
        s32 mid = (lo + hi) / 2;
        if (sRhBalancedPrices[mid][0] == item)
            return sRhBalancedPrices[mid][1];
        if (sRhBalancedPrices[mid][0] < item)
            lo = mid + 1;
        else
            hi = mid - 1;
    }
    return 0;
}

// vanilla: the item data's price
u32 LONG_CALL RH_ItemPrice(u32 item, u32 vanilla)
{
    u32 balanced, price = vanilla != 0 ? vanilla : ExtraBallPrice(item);
    if (!RH_Active() || price == 0)
        return price;
    if (S->shopAddCheapRareCandy && item == ITEM_RARE_CANDY)
        return 10;                                           // FVX "Add Cheap Rare Candies"
    if (!S->shopBalancePrices)
        return price;
    // FVX's balanced price table first (not for the balls only the romhack sells: FVX lists the Master Ball at 3000)
    balanced = BalancedPrice(item);
    if (vanilla != 0 && balanced != 0)
        price = balanced;
    return BalancePrice(price);
}
