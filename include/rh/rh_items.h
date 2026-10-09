#ifndef RH_ITEMS_H
#define RH_ITEMS_H
// Romhack: Items randomizer (src/field/rh_items.c, field overlay) and the romhack's script command.

#include "types.h"

// "nop_var_490 VAR" (vanilla script command 490, a no-op) runs the romhack special in VAR:
#define RH_SPECIAL_ITEM_BALL 1          // VAR_SPECIAL_x8004 = item, VAR_SPECIAL_x800A = item ball number -> new item
#define RH_SPECIAL_OPEN_SETTINGS 3      // the options screen (Randomizer Settings key item); waits until it closes
#define RH_SPECIAL_MASTER_BALL_OFFER 2  // VAR_SPECIAL_RESULT = 1: Kurt offers his one Master Ball (shops not randomized)

u32 RH_FieldItem(u32 item, u32 key);    // key: item ball number, or RH_FIELD_KEY_HIDDEN + hidden item index
BOOL RH_ItemBanned(u32 item);
// a specialty mart's stock with the Shop Items options (lists: every specialty mart, which: this one)
void RH_BuildMart(const u16 *const *lists, u32 count, u32 which, u16 *out, u32 max);
// Nuzlocke: no Rare Candy; "Add Cheap Rare Candies": Rare Candy in every mart (list ends with 0xFFFF, room for one more)
void RH_FilterMart(u16 *items, u32 max);
// special shop RH_SPECIAL_* (include/rh/rh_data.h) into out (0xFFFF-terminated)
void RH_BuildSpecialShop(u32 list, u16 *out, u32 max);

#endif // RH_ITEMS_H
