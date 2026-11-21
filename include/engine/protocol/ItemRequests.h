#ifndef NORTH_ISLAND_PROTOCOL_ITEM_REQUESTS_H
#define NORTH_ISLAND_PROTOCOL_ITEM_REQUESTS_H

#include "core/Bytes.h"

#include <cstdint>
#include <set>
#include <vector>

namespace protocol {

    /// @brief Builds 23/11: put on the item in a place of the bag.
    /// @param bagSlot Bag place (u8).
    /// @return The encoded payload.
    core::Bytes equipRequest(int bagSlot);

    /// @brief Builds 23/12: take off what is worn there into a free bag place.
    /// @param equipSlot Equipment slot (u8).
    /// @param bagSlot Free bag place (u8).
    /// @return The encoded payload.
    core::Bytes unequipRequest(int equipSlot, int bagSlot);

    /// @brief Builds 23/17: put the item in a place of the bag on the pet.
    /// @param petPlace Pet place (u8).
    /// @param bagSlot Bag place (u8).
    /// @return The encoded payload.
    core::Bytes petEquipRequest(int petPlace, int bagSlot);

    /// @brief Builds 23/18: take off what the pet wears there into a free bag place.
    /// @param petPlace Pet place (u8).
    /// @param equipSlot Equipment slot (u8).
    /// @param bagSlot Free bag place (u8).
    /// @return The encoded payload.
    core::Bytes petUnequipRequest(int petPlace, int equipSlot, int bagSlot);

    /// @brief Builds 23/19: destroy what the pet wears there (ConfirmDiscardEquip).
    /// @param petPlace Pet place (u8).
    /// @param equipSlot Equipment slot (u8).
    /// @return The encoded payload.
    core::Bytes petDiscardEquipRequest(int petPlace, int equipSlot);

    /// @brief Builds 23/15: use an item.
    /// @param bagSlot Bag place (u8).
    /// @param count Item count (u8).
    /// @param petSlot Pet place (u8, 0: the player).
    /// @param argument Use argument (u8).
    /// @return The encoded payload.
    core::Bytes useItemRequest(int bagSlot, int count, int petSlot, int argument);

    /// @brief Builds 23/82: the same use as 23/15, asked by the Remote Control (RequestUseBagItem's AMsg82).
    /// @param bagSlot Bag place (u8).
    /// @param count Item count (u8).
    /// @param petSlot Pet place (u8).
    /// @param argument Use argument (u8).
    /// @return The encoded payload.
    core::Bytes remoteUseItemRequest(int bagSlot, int count, int petSlot, int argument);

    /// @brief Builds 23/96: a companion voucher (item sub type 146) is used (RequestUseBagItem).
    /// @param bagSlot Bag place (u8).
    /// @return The encoded payload.
    core::Bytes companionVoucherRequest(int bagSlot);

    /// @brief Builds 23/120: a mall gift box (item type 26) is opened (UseGridItem).
    /// @param bagSlot Bag place (u8).
    /// @return The encoded payload.
    core::Bytes mallGiftBoxRequest(int bagSlot);

    /// @brief Builds 23/75: open a box or voucher of the bag (the Carnie Voucher with no key).
    /// @param bagSlot Bag place (u8).
    /// @param purpleKeySlot Bag place of the purple key (u8).
    /// @return The encoded payload.
    core::Bytes openBoxRequest(int bagSlot, int purpleKeySlot);

    /// @brief Builds 23/74: open a Sweets Pack (34078, UseGridItem).
    /// @param bagSlot Bag place (u8).
    /// @return The encoded payload.
    core::Bytes sweetsPackRequest(int bagSlot);

    /// @brief Builds 23/128: open one of the later boxes and lucky bags of the bag.
    /// @param bagSlot Bag place (u8).
    /// @param purpleKeySlot Bag place of the purple key (u8).
    /// @return The encoded payload.
    core::Bytes openBox2Request(int bagSlot, int purpleKeySlot);

    /// @brief Builds 23/83: a forgetting scroll on the player (ConfirmForgetAbility).
    /// @param bagSlot Bag place (u8).
    /// @param count Item count (u8).
    /// @param petSlot Pet place (u8).
    /// @param attribute Attribute to forget (u8, 1..5).
    /// @return The encoded payload.
    core::Bytes forgetAbilityRequest(int bagSlot, int count, int petSlot, int attribute);

    /// @brief Builds 23/85: a pet's ability swap (SelectPetAbilityToAdd).
    /// @param bagSlot Bag place (u8).
    /// @param count Item count (u8).
    /// @param petSlot Pet place (u8).
    /// @param forgotten Forgotten attribute (u8, 1..5).
    /// @param added Added attribute (u8, 1..5).
    /// @return The encoded payload.
    core::Bytes petAbilitySwapRequest(
        int bagSlot,
        int count,
        int petSlot,
        int forgotten,
        int added
    );

    /// @brief Builds 23/73: the auto collecting began at a map NPC (StartAutoCollect), 0 when it stopped (a walk).
    /// @param npc Map NPC (u8).
    /// @return The encoded payload.
    core::Bytes autoCollectRequest(int npc);

    /// @brief Builds 23/117: a thrown item (UseThrowItem).
    /// @param bagSlot Bag place (u8).
    /// @param x Map pixel x (u16).
    /// @param y Map pixel y (u16).
    /// @return The encoded payload.
    core::Bytes throwItemRequest(int bagSlot, int x, int y);

    /// @brief Builds 23/106 (u8 1): the Win Plate used in a battle (ConfirmUseWinPlate).
    /// @param bagSlot Bag place (u8).
    /// @return The encoded payload.
    core::Bytes winPlateRequest(int bagSlot);

    /// @brief Builds action 77 (no sub code): the Escape Button 34164 is used to quit the battle (ConfirmQuitBattleItem).
    /// @param bagSlot Bag place (u8).
    /// @return The encoded payload.
    core::Bytes quitBattleItemRequest(int bagSlot);

    /// @brief Builds 32/4: the Yes-No Board 34378 is used (SendItemChoice).
    /// @param bagSlot Bag place (u8).
    /// @param choice 1 O, 2 X, 3 Random (u8).
    /// @return The encoded payload.
    core::Bytes yesNoBoardRequest(int bagSlot, int choice);

    /// @brief Builds 23/108: the Time Mantle used at home (UseGridItem).
    /// @param bagSlot Bag place (u8).
    /// @return The encoded payload.
    core::Bytes timeMantleRequest(int bagSlot);

    /// @brief Builds 23/107: the Delay Clock used in a battle (UseGridItem); an item command with no item (50/2) follows.
    /// @param bagSlot Bag place (u8).
    /// @return The encoded payload.
    core::Bytes delayClockRequest(int bagSlot);

    /// @brief Builds 23/124 (u8 2 last): the item the server asked about is destroyed (ConfirmDiscardItem).
    /// @param bagSlot Bag place (u8).
    /// @param count Item count (u8).
    /// @return The encoded payload.
    core::Bytes discardItemRequest(int bagSlot, int count);

    /// @brief Builds 23/10: move items within the bag.
    /// @param fromSlot Source bag place (u8).
    /// @param count Item count (u8).
    /// @param toSlot Destination bag place (u8).
    /// @return The encoded payload.
    core::Bytes moveItemRequest(int fromSlot, int count, int toSlot);

    /// @brief The grid a qol sort request (23/250) applies to.
    enum class SortGrid : std::uint8_t { Bag = 1, Bank = 2, ItemBag = 3, Storeroom = 4, Picnic = 5, ExpBag = 6, StoreTank = 7 };

    /// @brief Builds 23/250 (u8 0): ask whether the qol server sorts (the original game has no sort).
    /// @return The encoded payload.
    core::Bytes sortQueryRequest();

    /// @brief Builds 23/250 (u8 1, u8 grid, then u8 count and a u8 per locked place when any): ask the qol server to sort a grid.
    /// @details A server without sort locks reads the grid and leaves the rest, so it sorts the locked places too.
    /// @param grid The grid to sort.
    /// @param locked The places (1-based) the sort leaves alone: what lies there stays, and nothing is moved in.
    /// @return The encoded payload.
    core::Bytes sortRequest(SortGrid grid, const std::set<int>& locked = {});

    /// @brief Builds 23/3 (u8 1 last): drop items on the ground.
    /// @param bagSlot Bag place (u8).
    /// @param count Item count (u8).
    /// @return The encoded payload.
    core::Bytes dropItemRequest(int bagSlot, int count);

    /// @brief Builds 23/13: destroy the worn item there (ConfirmDiscardEquip).
    /// @param equipSlot Equipment slot (u8).
    /// @return The encoded payload.
    core::Bytes discardEquipRequest(int equipSlot);

    /// @brief Builds 23/16: drop gold on the ground.
    /// @param amount Gold amount (i32).
    /// @return The encoded payload.
    core::Bytes throwMoneyRequest(std::int32_t amount);

    /// @brief Builds 23/kind: compound items (ConfirmCompound).
    /// @details Kind: 14 plain, 87 junior alchemy, 101 high alchemy, 115 the Bless Pack's items.
    /// @param kind Sub code.
    /// @param bagSlots Bag places, the main material first (u8 n, then n bytes).
    /// @return The encoded payload.
    core::Bytes compoundRequest(int kind, const std::vector<int>& bagSlots);

    /// @brief Builds 23/2: pick up what lies on the map there (TGdThing.HandleMouseUp).
    /// @param groundIndex Ground index (u16).
    /// @return The encoded payload.
    core::Bytes pickUpRequest(int groundIndex);

    /// @brief Builds 81/1: the owner picks up the thing of a special house room (TGdThingInSpecFloor.HandleMouseUp).
    /// @return The encoded payload.
    core::Bytes pickSpecFloorThingRequest();

    /// @brief Builds 23/34: items go into the city warehouse (WareDepositClick).
    /// @param bagSlots Bag place of each item (u8 each).
    /// @return The encoded payload.
    core::Bytes warehouseDepositRequest(const std::vector<int>& bagSlots);

    /// @brief Builds 23/35: items go back from the city warehouse into the bag (WareWithdrawClick).
    /// @param warehouseSlots Warehouse place of each item (u8 each).
    /// @return The encoded payload.
    core::Bytes warehouseWithdrawRequest(const std::vector<int>& warehouseSlots);

    /// @brief Builds 23/40: items go into the sale store (PutItemsForSale).
    /// @param bagSlots Bag place of each item (u8 each).
    /// @return The encoded payload.
    core::Bytes saleStorePutRequest(const std::vector<int>& bagSlots);

    /// @brief Builds 23/41: items go back from the sale store into the bag (TakeBackSaleItems).
    /// @param storeSlots Store place of each item (u8 each).
    /// @return The encoded payload.
    core::Bytes saleStoreTakeBackRequest(const std::vector<int>& storeSlots);

    /// @brief Builds 23/42: ask for the sale manager page (RequestSaleManage).
    /// @return The encoded payload.
    core::Bytes saleManageRequest();

    /// @brief Builds 23/44: ask for the sale list (RequestSendSale).
    /// @return The encoded payload.
    core::Bytes saleSendRequest();

    /// @brief Builds 23/43: collect the checked items of the sale net's list (RequestSaleNetGet; TCY_SaleMenu, TCY_SaleNetMenu).
    /// @param gold Gold (i32).
    /// @param places Place of each checked item (u8 each).
    /// @return The encoded payload.
    core::Bytes saleNetGetRequest(std::int32_t gold, const std::vector<int>& places);

    /// @brief Builds 26/3: gold into the sale account (DepositSaleMoney).
    /// @param gold Gold (i32).
    /// @return The encoded payload.
    core::Bytes saleMoneyDepositRequest(std::int32_t gold);

    /// @brief Builds 26/4: gold out of the sale account (WithdrawSaleMoney).
    /// @param gold Gold (i32).
    /// @return The encoded payload.
    core::Bytes saleMoneyWithdrawRequest(std::int32_t gold);

    /// @brief Builds 23/113: the divorce item is used (ConfirmForcedDivorce).
    /// @param bagSlot Bag place (u8).
    /// @return The encoded payload.
    core::Bytes forcedDivorceRequest(int bagSlot);

}

#endif
