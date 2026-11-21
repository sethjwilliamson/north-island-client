#ifndef NORTH_ISLAND_CLIENT_RULES_INVENTORY_SORT_H
#define NORTH_ISLAND_CLIENT_RULES_INVENTORY_SORT_H

#include "client/models/items/Inventory.h"
#include "formats/ItemTable.h"

#include <cstdint>
#include <functional>
#include <optional>
#include <set>
#include <string>
#include <vector>

namespace client {

    /// @brief One cell of the grid being sorted.
    struct SortCell {
        int itemId = 0;
        int count = 0;
        int kind = 0;
        bool stackable = false;
        bool fixed = false;

        /// @brief Whether the cell holds no item.
        /// @return True when empty.
        bool empty() const { return itemId == 0; }

        /// @brief Compares every field.
        /// @param other The cell to compare with.
        /// @return True when equal.
        bool operator==(const SortCell& other) const {
            return itemId == other.itemId && count == other.count && kind == other.kind && stackable == other.stackable && fixed == other.fixed;
        }
    };

    /// @brief One move of items between two places of the grid (1-based).
    struct SortMove {
        int from = 0;
        int to = 0;
        int count = 0;

        /// @brief Compares every field.
        /// @param other The move to compare with.
        /// @return True when equal.
        bool operator==(const SortMove& other) const { return from == other.from && to == other.to && count == other.count; }
    };

    /// @brief The next step of a sort: a move, finished, or blocked for want of an empty cell.
    struct SortStep {
        /// @brief What the step is.
        enum class Kind { Move, Done, NeedsSpace };
        Kind kind = Kind::Done;
        SortMove move;
    };

    /// @brief Finds the next move towards the sorted grid.
    /// @param cells The grid's cells.
    /// @return A move, Done, or NeedsSpace when an item in the way has no empty cell to go to.
    SortStep nextSortMove(const std::vector<SortCell>& cells);

    /// @brief Reads a grid into sort cells.
    /// @param grid The bag, bank, Item Bag or storeroom.
    /// @param items The item table (for each item's kind).
    /// @param locked Places (1-based) the sort leaves alone (qol sort locks): read as fixed, whatever lies there, empty or not.
    /// @return One cell per place, place 1 at index 0.
    std::vector<SortCell> sortCellsOf(const Inventory& grid, const formats::ItemTable& items, const std::set<int>& locked = {});

    /// @brief Runs a sort one move at a time, waiting for the grid to change between moves.
    class InventorySorter {
        public:
            using Send = std::function<void(const SortMove&)>;

            /// @brief Starts a client-side sort.
            void start() {
                m_running = true;
                m_sent.reset();
                m_moves = 0;
            }

            /// @brief Starts a server-side sort; the sort ends with the server's answer.
            /// @details Original: server action 23 / 250, sub 0 (the request is sent by the caller).
            /// @param nowMs Current time in ms.
            void startOnServer(std::uint64_t nowMs) {
                m_running = true;
                m_onServer = true;
                m_answer.reset();
                m_sentMs = nowMs;
            }

            /// @brief Records the server's answer to a server-side sort.
            /// @param result The server's result code.
            void serverAnswered(int result) {
                if (m_running && m_onServer) {
                    m_answer = result;
                }
            }

            /// @brief Ends the sort.
            void stop() {
                m_running = false;
                m_onServer = false;
                m_sent.reset();
            }

            /// @brief Whether a sort is running.
            /// @return True while running.
            bool isRunning() const { return m_running; }

            /// @brief Advances the sort; called once a beat while running.
            /// @param cells The grid's current cells.
            /// @param nowMs Current time in ms.
            /// @param send Callback that sends a move to the server.
            /// @return What to tell the player when the sort ends, else nothing.
            std::optional<std::string> step(const std::vector<SortCell>& cells, std::uint64_t nowMs, const Send& send);

        private:
            static constexpr std::uint64_t kAnswerMs = 2000; // ms to wait for the server's answer to a move
            static constexpr int kMostMoves = 300; // moves after which a sort stops

            bool m_running = false;
            bool m_onServer = false;
            std::optional<int> m_answer;
            std::optional<SortMove> m_sent;
            std::vector<SortCell> m_sentFrom;
            std::uint64_t m_sentMs = 0;
            int m_moves = 0;
    };

}

#endif
