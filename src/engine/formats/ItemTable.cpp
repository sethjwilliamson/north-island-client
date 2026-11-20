#include "formats/ItemTable.h"

#include "core/ByteReader.h"

#include <algorithm>

namespace formats {

    namespace {

        constexpr std::uint8_t kByteKey = 0x9A; // XOR key for byte fields of Item.dat
        constexpr std::uint16_t kWordKey = 0xEFC3; // XOR key for 16-bit fields of Item.dat
        constexpr std::uint32_t kIntKey = 0x0B80F4B4; // XOR key for 32-bit fields of Item.dat

        /// @brief Reads the obfuscated fields of one Item.dat record.
        class RecordReader {
            public:
                /// @brief Creates a reader for the record starting at a byte offset.
                /// @param data Whole file bytes.
                /// @param start Offset of the record start.
                RecordReader(
                    const core::Bytes& data,
                    std::size_t start
                )
                    : m_reader(data),
                      m_start(start) {}

                /// @brief Reads a decoded byte field ((value ^ key) - 9).
                /// @param offset Offset inside the record.
                /// @return The decoded value.
                int byteAt(std::size_t offset) {
                    m_reader.seek(m_start + offset);
                    return static_cast<std::uint8_t>((m_reader.u8() ^ kByteKey) - 9); // numbers are stored as value + 9
                }

                /// @brief Reads a decoded 16-bit field ((value ^ key) - 9).
                /// @param offset Offset inside the record.
                /// @return The decoded value.
                int wordAt(std::size_t offset) {
                    m_reader.seek(m_start + offset);
                    return static_cast<std::uint16_t>((m_reader.u16() ^ kWordKey) - 9); // numbers are stored as value + 9
                }

                /// @brief Reads a decoded 32-bit field ((value ^ key) - 9).
                /// @param offset Offset inside the record.
                /// @return The decoded value.
                std::int32_t intAt(std::size_t offset) {
                    m_reader.seek(m_start + offset);
                    return static_cast<std::int32_t>((m_reader.u32() ^ kIntKey) - 9u); // numbers are stored as value + 9
                }

                /// @brief Reads a length-prefixed text stored reversed.
                /// @param offset Offset inside the record (length byte first).
                /// @param capacity Size of the text field in bytes.
                /// @return The text in reading order.
                std::string reversedText(std::size_t offset, std::size_t capacity) {
                    m_reader.seek(m_start + offset);
                    const std::size_t length = std::min<std::size_t>(m_reader.u8(), capacity);
                    std::string raw = m_reader.text(capacity);
                    std::reverse(raw.begin(), raw.end());
                    return raw.substr(0, std::min(length, raw.size()));
                }

            private:
                core::ByteReader m_reader;
                std::size_t m_start;
        };

    }

    std::optional<ItemTable> ItemTable::parse(const core::Bytes& data) {
        if (data.size() < kRecordSize * 2) {
            return std::nullopt;
        }
        ItemTable table;
        const std::size_t count = data.size() / kRecordSize;
        for (std::size_t i = 1; i < count; ++i) {
            RecordReader record(data, i * kRecordSize);
            ItemRecord item;
            item.name = record.reversedText(0, 20); // name at offset 0, capacity 20
            item.type = record.byteAt(21); // record offset of the item type
            item.id = record.wordAt(22); // record offset of the item id
            item.iconId = record.wordAt(24); // record offset of the icon picture id
            for (std::size_t k = 0; k < 2; ++k) {
                item.attributeKinds[k] = record.wordAt(36 + k * 2); // attribute kinds start at 36, 2 bytes each
                item.attributeValues[k] = record.intAt(42 + k * 4); // attribute values start at 42, 4 bytes each
            }
            item.rank = record.byteAt(51); // record offset of the rank
            item.subType = record.byteAt(53); // record offset of the sub type
            item.setId = record.byteAt(135); // record offset of the set id
            item.requiredLevel = record.byteAt(119); // record offset of the required level
            item.price = record.intAt(120); // record offset of the price
            item.sellPrice = record.intAt(124); // record offset of the NPC sell price
            item.usableBy = record.byteAt(128); // record offset of the usable-by code
            item.flags = record.wordAt(129); // record offset of the flags
            item.unlisted = record.byteAt(118); // record offset of the unlisted marker
            item.combineKind = record.wordAt(142); // record offset of the alchemy class
            item.linkedSkillId = record.wordAt(140); // record offset of the linked skill id
            item.description = record.reversedText(152, 254); // description at offset 152, capacity 254
            item.cellWidth = std::max(1, static_cast<int>(record.byteAt(412))); // record offset of the bag cell width
            item.cellHeight = std::max(1, static_cast<int>(record.byteAt(413))); // record offset of the bag cell height
            item.petNpcId = record.wordAt(419); // record offset of the pet NPC id
            item.usePeriod = record.wordAt(427); // record offset of the use period (hours)
            if (item.id == 34128) { // item 34128: Electron Control, a 30-day remote the file gives no period (the server's ItemTable.Patch)
                item.usePeriod = 720; // 720 hours: 30 days, so it never stacks, as the server keeps it
            }
            item.maxUses = record.wordAt(429); // record offset of the max uses
            item.aliasItemId = record.wordAt(431); // record offset of the alias item id
            item.shopPicId = record.wordAt(26); // record offset of the mall picture id
            for (std::size_t bt = 0; bt < 4; ++bt) { // 4 body types
                item.lookIds[bt] = record.wordAt(28 + bt * 2); // worn sprite ids start at 28, 2 bytes each
            }
            item.equipSlot = record.byteAt(52); // record offset of the equip slot
            for (std::size_t part = 0; part < 4; ++part) { // 4 colour parts
                for (std::size_t bt = 0; bt < 4; ++bt) { // 4 body types
                    item.colorPatterns[part][bt] = record.intAt(54 + part * 16 + bt * 4); // patterns start at 54, 16 bytes per part, 4 per body type
                }
            }
            item.weaponKind = record.byteAt(414); // record offset of the weapon kind
            item.furnSizeX = record.byteAt(407); // record offset of the furniture x size
            item.furnSizeY = record.byteAt(408); // record offset of the furniture y size
            item.furnSizeZ = record.byteAt(409); // record offset of the furniture z size
            item.furnPicture0 = record.wordAt(415); // record offset of the first facing picture
            item.furnPicture1 = record.wordAt(417); // record offset of the second facing picture
            item.furnPlaceFlags = record.byteAt(421); // record offset of the furniture placing flags
            if (item.linkedSkillId != 0) {
                table.m_itemsBySkill.emplace(item.linkedSkillId, item.id);
            }
            table.m_items[item.id] = item;
        }
        return table;
    }

    std::optional<ItemRecord> ItemTable::findByLinkedSkill(int skillId) const {
        const auto found = m_itemsBySkill.find(skillId);
        return found != m_itemsBySkill.end() ? find(found->second) : std::nullopt;
    }

    std::vector<int> ItemTable::ids() const {
        std::vector<int> ids;
        ids.reserve(m_items.size());
        for (const auto& [id, record] : m_items) {
            ids.push_back(id);
        }
        std::sort(ids.begin(), ids.end());
        return ids;
    }

    std::optional<ItemRecord> ItemTable::find(int itemId) const {
        const auto found = m_items.find(itemId);
        if (found == m_items.end()) {
            return std::nullopt;
        }
        return found->second;
    }

    bool ItemTable::setText(int itemId, const std::optional<std::string>& name, const std::optional<std::string>& description) {
        const auto found = m_items.find(itemId);
        if (found == m_items.end()) {
            return false;
        }
        if (name) {
            found->second.name = *name;
        }
        if (description) {
            found->second.description = *description;
        }
        return true;
    }

}
