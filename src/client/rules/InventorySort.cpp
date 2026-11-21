#include "client/rules/InventorySort.h"

#include <algorithm>
#include <tuple>

namespace client {

    SortStep nextSortMove(const std::vector<SortCell>& cells) {
        const int size = static_cast<int>(cells.size());
        const auto movable = [&cells](int index) { return !cells[static_cast<std::size_t>(index)].fixed && !cells[static_cast<std::size_t>(index)].empty(); };
        for (int i = 0; i < size; ++i) {
            const SortCell& into = cells[static_cast<std::size_t>(i)];
            if (!movable(i) || !into.stackable || into.count >= Inventory::kMaxStack) {
                continue;
            }
            for (int j = i + 1; j < size; ++j) {
                const SortCell& from = cells[static_cast<std::size_t>(j)];
                if (movable(j) && from.stackable && from.itemId == into.itemId) {
                    return {SortStep::Kind::Move, {j + 1, i + 1, std::min(from.count, Inventory::kMaxStack - into.count)}};
                }
            }
        }
        std::vector<int> wanted;
        std::vector<int> free;
        for (int i = 0; i < size; ++i) {
            if (movable(i)) {
                wanted.push_back(i);
            }
            if (!cells[static_cast<std::size_t>(i)].fixed) {
                free.push_back(i);
            }
        }
        std::stable_sort(wanted.begin(), wanted.end(), [&cells](int a, int b) {
            const SortCell& x = cells[static_cast<std::size_t>(a)];
            const SortCell& y = cells[static_cast<std::size_t>(b)];
            return std::make_tuple(x.kind, x.itemId, -x.count) < std::make_tuple(y.kind, y.itemId, -y.count);
        });
        const auto same = [](const SortCell& x, const SortCell& y) { return !x.empty() && x.itemId == y.itemId && x.count == y.count; };
        for (std::size_t k = 0; k < wanted.size(); ++k) {
            const int cell = free[k];
            const SortCell& want = cells[static_cast<std::size_t>(wanted[k])];
            const SortCell& there = cells[static_cast<std::size_t>(cell)];
            if (same(there, want)) {
                continue;
            }
            if (!there.empty()) {
                for (std::size_t e = k + 1; e < free.size(); ++e) {
                    if (cells[static_cast<std::size_t>(free[e])].empty()) {
                        return {SortStep::Kind::Move, {cell + 1, free[e] + 1, there.count}};
                    }
                }
                return {SortStep::Kind::NeedsSpace, {}};
            }
            for (std::size_t s = k + 1; s < free.size(); ++s) {
                const SortCell& source = cells[static_cast<std::size_t>(free[s])];
                if (same(source, want)) {
                    return {SortStep::Kind::Move, {free[s] + 1, cell + 1, source.count}};
                }
            }
        }
        return {SortStep::Kind::Done, {}};
    }

    std::vector<SortCell> sortCellsOf(const Inventory& grid, const formats::ItemTable& items, const std::set<int>& locked) {
        std::vector<SortCell> cells(static_cast<std::size_t>(grid.slots()));
        for (int slot = 1; slot <= grid.slots(); ++slot) {
            SortCell& cell = cells[static_cast<std::size_t>(slot - 1)];
            if (locked.count(slot) != 0) {
                cell.fixed = true; // a locked place: neither moved from nor moved into
                continue;
            }
            const int occupant = grid.occupantOf(slot);
            if (occupant == 0) {
                continue;
            }
            const protocol::ItemInstance& item = grid.bagItem(occupant);
            const core::Size size = grid.cellsOf(item.itemId);
            if (size.width > 1 || size.height > 1) {
                cell.fixed = true;
                continue;
            }
            cell.itemId = item.itemId;
            cell.count = item.count;
            cell.stackable = grid.isStackable(item.itemId);
            if (const auto record = items.find(item.itemId)) {
                cell.kind = record->type;
            }
        }
        return cells;
    }

    std::optional<std::string> InventorySorter::step(const std::vector<SortCell>& cells, std::uint64_t nowMs, const Send& send) {
        if (!m_running) {
            return std::nullopt;
        }
        if (m_onServer) {
            if (!m_answer) {
                if (nowMs - m_sentMs < kAnswerMs) {
                    return std::nullopt;
                }
                stop();
                return "The server did not sort.";
            }
            const int result = *m_answer;
            stop();
            switch (result) {
                case 0: return "Sorted."; // server result 0: sorted
                case 1: return "Already in order."; // server result 1: nothing to do
                case 2: return "Sorting has to wait (a fight, a trade or a stall)."; // server result 2: busy
                case 3: return "The storeroom sorts in your own house."; // server result 3: storeroom outside own house
                default: return "The server did not sort."; // any other result: failure
            }
        }
        if (m_sent) {
            if (cells == m_sentFrom) {
                if (nowMs - m_sentMs < kAnswerMs) {
                    return std::nullopt;
                }
                stop();
                return "The server did not take a move; sorting stopped.";
            }
            m_sent.reset();
        }
        const SortStep next = nextSortMove(cells);
        if (next.kind == SortStep::Kind::Done) {
            const bool moved = m_moves > 0;
            stop();
            return moved ? "Sorted." : "Already in order.";
        }
        if (next.kind == SortStep::Kind::NeedsSpace) {
            stop();
            return "Sorting needs one empty cell.";
        }
        if (m_moves >= kMostMoves) {
            stop();
            return "Sorting stopped after " + std::to_string(kMostMoves) + " moves.";
        }
        send(next.move);
        m_sent = next.move;
        m_sentFrom = cells;
        m_sentMs = nowMs;
        ++m_moves;
        return std::nullopt;
    }

}
