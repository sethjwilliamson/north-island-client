#include "client/rules/ExpressItems.h"

#include "client/rules/Stalling.h"

namespace client {

    namespace {

        constexpr int kUnavailableFlag = 2; // item record flag bit value 2: cannot be transferred
        constexpr int kMagicMagnet = 34021; // item 34021: Magic Magnet (cannot be sent by express)

    }

    std::optional<std::string> expressRefusal(const formats::ItemTable& items, const protocol::ItemInstance& item) {
        const auto record = items.find(item.itemId);
        if (!record || (record->flags & kUnavailableFlag) != 0 || item.itemId == kMagicMagnet) {
            return "Unavailable for expres";
        }
        if (record->cellWidth > 1 || record->cellHeight > 1) {
            return "Express failed because this item is oversized";
        }
        if (item.lookItemId != 0) {
            return "Pose Items can't be delivered";
        }
        if (isNonTransferable(item.insertItemId)) {
            return "This item cannot be transferred";
        }
        return std::nullopt;
    }

}
