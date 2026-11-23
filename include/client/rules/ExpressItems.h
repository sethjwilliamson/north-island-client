#ifndef NORTH_ISLAND_CLIENT_RULES_EXPRESS_ITEMS_H
#define NORTH_ISLAND_CLIENT_RULES_EXPRESS_ITEMS_H

#include "formats/ItemTable.h"
#include "protocol/ItemInstance.h"

#include <optional>
#include <string>

namespace client {

    /// @brief Tells why the pet express does not carry an item, whatever its bag place is in use for.
    /// @details Original: the item checks of TTH_ExpressForm.AddExpressItem.
    /// @param items The item table.
    /// @param item The item.
    /// @return The line to show, nothing when the item may go.
    std::optional<std::string> expressRefusal(const formats::ItemTable& items, const protocol::ItemInstance& item);

}

#endif
