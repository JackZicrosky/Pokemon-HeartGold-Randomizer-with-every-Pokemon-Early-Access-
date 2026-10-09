// Romhack: item prices (port of the FireRed/Emerald RH_GetItemPrice in rh_items.c). Every price the game reads
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

u32 LONG_CALL RH_ItemPrice(u32 item, u32 vanilla)
{
    return vanilla != 0 ? vanilla : ExtraBallPrice(item);
}
