#include "protocol/ItemRequests.h"

#include "core/ByteWriter.h"

namespace protocol {

    namespace {

        constexpr std::uint8_t kItems = 23; // action 23: items

        /// @brief Narrows an int to a payload byte.
        /// @param value The value.
        /// @return The low byte.
        std::uint8_t byte(int value) { return static_cast<std::uint8_t>(value); }

    }

    core::Bytes equipRequest(int bagSlot) { return {kItems, 11, byte(bagSlot)}; } // sub 11: put on an item

    core::Bytes unequipRequest(int equipSlot, int bagSlot) { return {kItems, 12, byte(equipSlot), byte(bagSlot)}; } // sub 12: take off

    core::Bytes petEquipRequest(int petPlace, int bagSlot) { return {kItems, 17, byte(petPlace), byte(bagSlot)}; } // sub 17: equip the pet

    core::Bytes petUnequipRequest(int petPlace, int equipSlot, int bagSlot) {
        return {kItems, 18, byte(petPlace), byte(equipSlot), byte(bagSlot)}; // sub 18: take off the pet's gear
    }

    core::Bytes petDiscardEquipRequest(int petPlace, int equipSlot) { return {kItems, 19, byte(petPlace), byte(equipSlot)}; } // sub 19: destroy the pet's gear

    core::Bytes useItemRequest(int bagSlot, int count, int petSlot, int argument) {
        return {kItems, 15, byte(bagSlot), byte(count), byte(petSlot), byte(argument)}; // sub 15: use an item
    }

    core::Bytes remoteUseItemRequest(int bagSlot, int count, int petSlot, int argument) {
        return {kItems, 82, byte(bagSlot), byte(count), byte(petSlot), byte(argument)}; // sub 82: use an item, from the Remote Control
    }

    core::Bytes companionVoucherRequest(int bagSlot) { return {kItems, 96, byte(bagSlot)}; } // sub 96: companion voucher

    core::Bytes mallGiftBoxRequest(int bagSlot) { return {kItems, 120, byte(bagSlot)}; } // sub 120: mall gift box

    core::Bytes openBoxRequest(int bagSlot, int purpleKeySlot) { return {kItems, 75, byte(bagSlot), byte(purpleKeySlot)}; } // sub 75: open a box or voucher

    core::Bytes sweetsPackRequest(int bagSlot) { return {kItems, 74, byte(bagSlot)}; } // sub 74: Sweets Pack

    core::Bytes openBox2Request(int bagSlot, int purpleKeySlot) {
        return {kItems, 128, byte(bagSlot), byte(purpleKeySlot)};
    } // sub 128: open a later box or lucky bag

    core::Bytes forgetAbilityRequest(int bagSlot, int count, int petSlot, int attribute) {
        return {kItems, 83, byte(bagSlot), byte(count), byte(petSlot), byte(attribute)}; // sub 83: forgetting scroll
    }

    core::Bytes petAbilitySwapRequest(
        int bagSlot,
        int count,
        int petSlot,
        int forgotten,
        int added
    ) {
        return {kItems, 85, byte(bagSlot), byte(count), byte(petSlot), byte(forgotten), byte(added)}; // sub 85: pet ability swap
    }

    core::Bytes autoCollectRequest(int npc) { return {kItems, 73, byte(npc)}; } // sub 73: auto collecting

    core::Bytes throwItemRequest(int bagSlot, int x, int y) {
        core::ByteWriter writer;
        writer.u8(23).u8(117).u8(byte(bagSlot)).u16(static_cast<std::uint16_t>(x)).u16(static_cast<std::uint16_t>(y)); // action 23, sub 117: thrown item
        return writer.take();
    }

    core::Bytes winPlateRequest(int bagSlot) { return {kItems, 106, 1, byte(bagSlot)}; } // sub 106: Win Plate (then constant 1)

    core::Bytes quitBattleItemRequest(int bagSlot) { return {77, byte(bagSlot)}; } // action 77: Escape Button

    core::Bytes yesNoBoardRequest(int bagSlot, int choice) { return {32, 4, byte(bagSlot), byte(choice)}; } // action 32, sub 4: Yes-No Board

    core::Bytes timeMantleRequest(int bagSlot) { return {kItems, 108, byte(bagSlot)}; } // sub 108: Time Mantle

    core::Bytes delayClockRequest(int bagSlot) { return {kItems, 107, byte(bagSlot)}; } // sub 107: Delay Clock

    core::Bytes discardItemRequest(int bagSlot, int count) { return {kItems, 124, byte(bagSlot), byte(count), 2}; } // sub 124: destroy confirmed (last byte 2)

    core::Bytes moveItemRequest(int fromSlot, int count, int toSlot) {
        return {kItems, 10, byte(fromSlot), byte(count), byte(toSlot)}; // sub 10: move within the bag
    }

    core::Bytes sortQueryRequest() { return {kItems, 250, 0}; } // sub 250 (qol only): does the server sort?

    core::Bytes sortRequest(SortGrid grid, const std::set<int>& locked) { // sub 250 (qol only): sort a grid
        core::Bytes bytes{kItems, 250, 1, static_cast<std::uint8_t>(grid)};
        std::vector<int> places;
        for (const int place : locked) {
            if (place >= 1 && place <= 255 && places.size() < 255) {
                places.push_back(place);
            }
        }
        if (!places.empty()) {
            bytes.push_back(byte(static_cast<int>(places.size())));
            for (const int place : places) {
                bytes.push_back(byte(place));
            }
        }
        return bytes;
    }

    core::Bytes dropItemRequest(int bagSlot, int count) { return {kItems, 3, byte(bagSlot), byte(count), 1}; } // sub 3: drop on the ground

    core::Bytes discardEquipRequest(int equipSlot) { return {kItems, 13, byte(equipSlot)}; } // sub 13: destroy worn item

    core::Bytes throwMoneyRequest(std::int32_t amount) {
        core::ByteWriter writer;
        writer.u8(23).u8(16).i32(amount); // action 23, sub 16: drop gold
        return writer.take();
    }

    core::Bytes compoundRequest(int kind, const std::vector<int>& bagSlots) {
        core::ByteWriter writer;
        writer.u8(23).u8(static_cast<std::uint8_t>(kind)).u8(static_cast<std::uint8_t>(bagSlots.size())); // action 23 (items)
        for (const int slot : bagSlots) {
            writer.u8(static_cast<std::uint8_t>(slot));
        }
        return writer.take();
    }

    core::Bytes pickSpecFloorThingRequest() { return {81, 1}; } // action 81, sub 1: pick up the special floor thing

    core::Bytes pickUpRequest(int groundIndex) {
        core::ByteWriter writer;
        writer.u8(kItems).u8(2).u16(static_cast<std::uint16_t>(groundIndex)); // sub 2: pick up from the map
        return writer.take();
    }

    core::Bytes warehouseDepositRequest(const std::vector<int>& bagSlots) {
        core::ByteWriter writer;
        writer.u8(kItems).u8(34); // sub 34: into the city warehouse
        for (const int slot : bagSlots) {
            writer.u8(byte(slot));
        }
        return writer.take();
    }

    core::Bytes warehouseWithdrawRequest(const std::vector<int>& warehouseSlots) {
        core::ByteWriter writer;
        writer.u8(kItems).u8(35); // sub 35: out of the city warehouse
        for (const int slot : warehouseSlots) {
            writer.u8(byte(slot));
        }
        return writer.take();
    }

    namespace {

        /// @brief Builds an action 23 packet with a sub code and one byte per place.
        /// @param sub Sub code.
        /// @param places Places, one byte each.
        /// @return The encoded payload.
        core::Bytes placesRequest(std::uint8_t sub, const std::vector<int>& places) {
            core::ByteWriter writer;
            writer.u8(kItems).u8(sub);
            for (const int place : places) {
                writer.u8(byte(place));
            }
            return writer.take();
        }

    }

    core::Bytes saleStorePutRequest(const std::vector<int>& bagSlots) { return placesRequest(40, bagSlots); } // sub 40: into the sale store

    core::Bytes saleStoreTakeBackRequest(const std::vector<int>& storeSlots) { return placesRequest(41, storeSlots); } // sub 41: back from the sale store

    core::Bytes saleManageRequest() { return {kItems, 42}; } // sub 42: sale manager page

    core::Bytes saleSendRequest() { return {kItems, 44}; } // sub 44: sale list

    core::Bytes saleNetGetRequest(std::int32_t gold, const std::vector<int>& places) {
        core::ByteWriter writer;
        writer.u8(kItems).u8(43).i32(gold); // sub 43: collect from the sale net's list
        for (const int place : places) {
            writer.u8(byte(place));
        }
        return writer.take();
    }

    core::Bytes saleMoneyDepositRequest(std::int32_t gold) {
        core::ByteWriter writer;
        writer.u8(26).u8(3).i32(gold); // action 26, sub 3: into the sale account
        return writer.take();
    }

    core::Bytes saleMoneyWithdrawRequest(std::int32_t gold) {
        core::ByteWriter writer;
        writer.u8(26).u8(4).i32(gold); // action 26, sub 4: out of the sale account
        return writer.take();
    }

    core::Bytes forcedDivorceRequest(int bagSlot) {
        core::ByteWriter writer;
        writer.u8(kItems).u8(113).u8(byte(bagSlot)); // sub 113: forced divorce item
        return writer.take();
    }

}
