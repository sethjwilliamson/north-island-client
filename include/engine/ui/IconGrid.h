#ifndef NORTH_ISLAND_UI_ICON_GRID_H
#define NORTH_ISLAND_UI_ICON_GRID_H

#include "core/Color.h"
#include "ui/Element.h"
#include "ui/IconCell.h"

#include <map>
#include <optional>
#include <set>
#include <string>

namespace ui {

    /// @brief Cells of icons in columns and rows (item bag, equipment slots) filled by the view model; supports hover, click, double click, right click and drag and drop.
    /// @details Original: drag and drop follows TSe_ItemImage.HandleMouseDown; frames and cells draw like TRe_ItemGridForm.DrawControl and TSe_ItemImage.DrawControl.
    class IconGrid : public Element {
        public:
            /// @brief Creates a grid and declares its events (DoubleClick, Drop, Click, RightClick, Press, MouseWheel, QuickMove, Pick).
            IconGrid();

            /// @brief Reads the IconGrid attributes (Columns, Rows, Pitch, PitchY, CellSize, FirstIndex, HoverColor, CountColor, Draggable, CellPicture, CellSource, OutlineColor, OutlineGapColor).
            /// @param name The attribute name.
            /// @param value The attribute text.
            /// @param context The load context.
            void applyAttribute(const std::string& name, const std::string& value, const LoadContext& context) override;

            /// @brief Takes the size of the cells when the layout gave none.
            /// @param context The load context.
            void finishLoading(const LoadContext& context) override;

            /// @brief Fills a cell.
            /// @param index The cell number.
            /// @param cell What the cell shows.
            void setCell(int index, const IconCell& cell) { m_cells[index] = cell; }

            /// @brief Empties one cell.
            /// @param index The cell number.
            void clearCell(int index) { m_cells.erase(index); }

            /// @brief Empties every cell.
            void clearCells() { m_cells.clear(); }

            /// @brief Changes the number of columns and rows; the grid takes their size.
            /// @param columns The new column count.
            /// @param rows The new row count.
            void setColumns(int columns, int rows);

            /// @brief The distance from one cell's origin to the next.
            /// @return The Pitch (x) and PitchY (y) attributes, px.
            core::Point pitch() const { return {m_pitch, m_pitchY}; }

            /// @brief Whether a cell shows a thing.
            /// @param index The cell number.
            /// @return True when the cell has a picture.
            bool holdsIcon(int index) const {
                const auto cell = m_cells.find(index);
                return cell != m_cells.end() && cell->second.picture.isValid();
            }

            /// @brief How many things a cell holds.
            /// @param index The cell number.
            /// @return Its count (1 for a thing with none shown); 0 for an empty cell.
            int countAt(int index) const {
                const auto cell = m_cells.find(index);
                return cell == m_cells.end() || !cell->second.picture.isValid() ? 0 : cell->second.count.value_or(1);
            }

            /// @brief Whether a cell shows the same thing as a drag (the same picture), so what is carried may join it.
            /// @param index The cell number.
            /// @param drag The drag.
            /// @return True when it does.
            bool holdsSame(int index, const DragItem& drag) const {
                const auto cell = m_cells.find(index);
                return cell != m_cells.end() && cell->second.picture.isValid() && cell->second.picture == drag.picture && cell->second.source == drag.source;
            }

            /// @brief Raises QuickMove for a cell (a Shift click the modern way), when the layout gave it a handler.
            /// @param context The UI context.
            /// @param index The cell number.
            /// @return True when the grid has a QuickMove handler and a thing in that cell.
            bool quickMove(UiContext& context, int index) const {
                if (!hasHandler("QuickMove") || !holdsIcon(index)) {
                    return false;
                }
                raise(context, "QuickMove", index);
                return true;
            }

            /// @brief Turns picking on or off (qol): while on, a left press on a cell, empty or not, raises Pick with its number and
            /// does nothing else (no drag, click, double click or right click); the Screen sends the press here first.
            /// @param picking True to pick cells.
            void setPicking(bool picking) { m_picking = picking; }

            /// @brief Whether the grid is picking cells.
            /// @return True while picking.
            bool isPicking() const { return m_picking; }

            /// @brief Raises Pick for the cell under a point (an item's own cells first, else the grid cell there).
            /// @param context The UI context.
            /// @param local The point relative to the grid.
            void pick(UiContext& context, core::Point local) const {
                if (const int index = dropIndexAt(local); index > 0) {
                    raise(context, "Pick", index);
                }
            }

            /// @brief Sets the cells drawn with a lock mark (qol: cells a sort leaves alone), empty cells too.
            /// @param cells The cell numbers.
            void setLocked(std::set<int> cells) { m_locked = std::move(cells); }

            /// @brief Whether the grid's things can be dragged (the Draggable attribute).
            /// @return True when draggable.
            bool isDraggable() const { return m_draggable; }

            /// @brief Whether a double click on a thing does something (the DoubleClick event has a handler).
            /// @return True when it does.
            bool usesDoubleClick() const { return hasHandler("DoubleClick"); }

            /// @brief The number of the grid's first cell.
            /// @return The FirstIndex attribute.
            int firstIndex() const { return m_firstIndex; }

            /// @brief How many cells the grid has.
            /// @return Columns times rows.
            int cellCount() const { return m_columns * m_rows; }

            /// @brief The size of one cell.
            /// @return The CellSize attribute as width and height, px.
            core::Size cellSize() const { return {m_cellSize, m_cellSize}; }

            /// @brief The number of the cell whose box holds a point of the grid.
            /// @param local The point relative to the grid.
            /// @return The cell number (0: none).
            int cellAt(core::Point local) const;

            /// @brief The top left of a cell in the grid.
            /// @param index The cell number.
            /// @return The cell's top left relative to the grid.
            core::Point cellPosition(int index) const;

            /// @brief Makes the drag of a cell's icon as a press on its middle would, for a drag started from code.
            /// @param index The cell.
            /// @return The drag, none for a cell without an icon.
            std::optional<DragItem> dragOf(int index) const;

            /// @brief Whether a point is inside the grid's bounds.
            /// @param local The point relative to the grid.
            /// @param pictures The picture library (unused).
            /// @return True when inside.
            bool hitTest(core::Point local, const gfx::PictureLibrary& pictures) const override;

            /// @brief The hint of the cell under a point.
            /// @param local The point relative to the grid.
            /// @return The cell's hint ("" when none).
            const std::string& hintAt(core::Point local) const override;

            /// @brief The hint box origin: the top left of the cell under the point.
            /// @param local The point relative to the grid.
            /// @return The cell's top left.
            core::Point hintOrigin(core::Point local) const override;

            /// @brief Raises DoubleClick with the cell number and suppresses the drag pick-up of its press.
            /// @param context The UI context.
            /// @param local The point relative to the grid.
            void onDoubleClick(UiContext& context, core::Point local) override;

            /// @brief Raises Press on a left press and starts dragging the icon when Draggable.
            /// @param context The UI context.
            /// @param button The button.
            /// @param local The point relative to the grid.
            void onMouseDown(UiContext& context, MouseButton button, core::Point local) override;

            /// @brief Raises Click (left, same cell as the press) or RightClick (right) for the cell under the mouse.
            /// @param context The UI context.
            /// @param button The button.
            /// @param local The point relative to the grid.
            void onMouseUp(UiContext& context, MouseButton button, core::Point local) override;

            /// @brief The wheel over the grid: raises "MouseWheel" with the steps.
            /// @param context The UI context.
            /// @param steps The wheel steps.
            void onMouseWheel(UiContext& context, int steps) override;

            /// @brief Raises Drop for an icon dragged from this grid.
            /// @param context The UI context.
            /// @param sourceIndex The cell the icon came from.
            /// @param target The element it was let go over.
            /// @param targetIndex The target's cell (-1: none).
            /// @param screenPoint Where the mouse was, in screen coordinates.
            /// @param grab Where the mouse held the icon, from its top left.
            void onDropped(
                UiContext& context,
                int sourceIndex,
                std::optional<std::reference_wrapper<const Element>> target,
                int targetIndex,
                core::Point screenPoint,
                core::Point grab
            ) override;

            /// @brief The cell a dragged icon would go into: an item's own cells first, else the grid cell there.
            /// @param local The point relative to the grid.
            /// @return The cell number (-1: none).
            int dropIndexAt(core::Point local) const override;

        protected:
            /// @brief Draws the outline round the cells, the cell picture under every cell, the cell frames, then each cell's hover fill, icon, count and overlays.
            /// @param context The draw context.
            /// @param screenPosition The grid's top left on the screen.
            void draw(DrawContext& context, core::Point screenPosition) override;

        private:
            int m_columns = 1;
            int m_rows = 1;
            int m_pitch = 34; // horizontal distance between cell origins, px
            int m_pitchY = 34; // vertical distance between cell origins, px
            int m_cellSize = 32; // icon width and height, px
            int m_firstIndex = 1;
            bool m_draggable = false;
            std::string m_cellPictureName; // drawn under every cell, empty ones too; none when empty
            gfx::PictureId m_cellPicture;
            std::optional<core::Rect> m_cellSource; // the part of the cell picture drawn; the whole picture when none
            std::optional<core::Color> m_outlineColor; // a 1 px line round all the cells; none when unset
            std::optional<core::Color> m_outlineGapColor; // when set, the bag's frame: a gap of it and a second line on the top and left, a gap row below
            bool m_doubleClicked = false;
            int m_pressedCell = 0;
            core::Color m_hoverColor{255, 108, 1, 80}; // orange hover highlight, alpha 80
            core::Color m_countColor{8, 8, 8, 255}; // item count text: near black
            std::map<int, IconCell> m_cells;
            bool m_picking = false;
            std::set<int> m_locked;
    };

}

#endif
