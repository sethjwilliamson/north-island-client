#ifndef NORTH_ISLAND_CLIENT_VIEWMODELS_ITEMS_ITEM_WINDOW_H
#define NORTH_ISLAND_CLIENT_VIEWMODELS_ITEMS_ITEM_WINDOW_H

#include "client/rules/InventorySort.h"
#include "client/viewmodels/items/SortLocks.h"
#include "client/services/GameServices.h"
#include "client/rules/BagRequests.h"
#include "client/models/character/PlayerStatus.h"
#include "client/models/character/FigureAnimation.h"
#include "client/models/items/BagMarks.h"
#include "client/models/items/Inventory.h"
#include "client/models/pets/EquipBonusSnapshot.h"
#include "client/models/pets/PetTeam.h"
#include "client/viewmodels/items/ItemInfoPopup.h"
#include "client/viewmodels/chat/PlayerMessages.h"
#include "client/models/world/MapCharacter.h"
#include "ui/DigitText.h"
#include "ui/DrawingArea.h"
#include "ui/IconGrid.h"
#include "ui/ImageButton.h"
#include "ui/ImageView.h"
#include "ui/Screen.h"
#include "ui/Spot.h"
#include "ui/Window.h"

#include <functional>
#include <map>
#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>

namespace client {

    /// @brief The item window: own character status, six worn slots, pet pages and the bag.
    /// @details Original: ItemWindow.xml, TSe_EquipForm2; double click on a bag item sends 23/11 (pet 23/17) to wear or 23/15 to use; on a worn item 23/12 (pet 23/18) to take off.
    class ItemWindow {
        public:
            /// @brief Builds the window and wires its controls.
            /// @param services Shared game services.
            /// @param screen Screen the window lives on.
            /// @param status Player status shown.
            /// @param inventory Bag and worn items.
            /// @param pets Pet team (pet pages).
            /// @param marks Bag item marks.
            /// @param player The own map character (figure).
            /// @param messages Player message sink.
            /// @param info Item info popup.
            ItemWindow(
                GameServices& services,
                ui::Screen& screen,
                const PlayerStatus& status,
                const Inventory& inventory,
                const PetTeam& pets,
                const BagMarks& marks,
                const MapCharacter& player,
                PlayerMessages& messages,
                ItemInfoPopup& info
            );

            /// @brief Whether the window is visible.
            /// @return True when open.
            bool isOpen() const { return m_window.isVisible(); }

            /// @brief Opens the window.
            void open();

            /// @brief Closes the window and drops unsent stat points.
            /// @details Original: HideCtrl.
            void close();

            /// @brief Opens the window when closed, else closes it.
            void toggle();

            /// @brief Re-reads the bag, worn items and status.
            void refresh();

            /// @brief Per-frame update of the hover info popup.
            void update();

            /// @brief Sets the callback that opens the notebook for a notebook item.
            /// @param openNotebook Opener.
            void setNotebook(std::function<void()> openNotebook) { m_openNotebook = std::move(openNotebook); }

            /// @brief Sets the query telling whether a stat is raised by a running buff.
            /// @param buffed Returns true for stat 1..5 (ATK, DEF, MATK, MDEF, SPD) when buffed.
            /// @details Original: 23/119 buff; the number is drawn red.
            void setBuffedStat(std::function<bool(int stat)> buffed) { m_buffed = std::move(buffed); }

            /// @brief Sets the query for the own player's strengthen buff mask.
            /// @param buffMask Returns the mask.
            /// @details Original: FStrengthenBuffMask.
            void setBuffMask(std::function<std::uint32_t()> buffMask) { m_buffMask = std::move(buffMask); }

            /// @brief Sets the handler for items the scene uses itself.
            /// @param useSpecial Called with item id, bag place and pet order; returns true when it took the use.
            /// @details Original: UseGridItem chain of items by number.
            void setSpecialItems(std::function<bool(int itemId, int slot, int petOrder)> useSpecial) { m_useSpecial = std::move(useSpecial); }

            /// @brief Sets the handler for a vehicle item used.
            /// @param useVehicle Called with the bag place.
            void setUseVehicle(std::function<void(int slot)> useVehicle) { m_useVehicle = std::move(useVehicle); }

            /// @brief Sets the handler for fuel dropped on the Store Tank.
            /// @param dropOnTank Called with the bag place and the row (0 the form).
            void setTankDrop(std::function<void(int slot, int row)> dropOnTank) { m_dropOnTank = std::move(dropOnTank); }

            /// @brief Sets the handler for a vehicle dropped in the garage's repair space.
            /// @param dropOnRepair Called with the bag place.
            void setRepairDrop(std::function<void(int slot)> dropOnRepair) { m_dropOnRepair = std::move(dropOnRepair); }

            /// @brief Sets the handler for a bag item let go on the named element.
            /// @param target Element name.
            /// @param handler Called with the bag place.
            void setBagDrop(const std::string& target, std::function<void(int slot)> handler) { m_bagDrops[target] = std::move(handler); }

            /// @brief Sets the handler for a bag item dropped on the bank's item form.
            /// @param dropOnBank Drop handler.
            void setBankDrop(std::function<void(const ui::EventArgs&)> dropOnBank) { m_dropOnBank = std::move(dropOnBank); }

            /// @brief Sets the handler for a bag item dropped on a vending machine form.
            /// @param dropOnVending Called with the bag place.
            void setVendingDrop(std::function<void(int slot)> dropOnVending) { m_dropOnVending = std::move(dropOnVending); }

            /// @brief Sets the hook called when the window is opened or closed.
            /// @param shown Hook.
            /// @details Original: ShowCtrl and HideCtrl clear the Item button's alert.
            void setShownHook(std::function<void()> shown) { m_shown = std::move(shown); }

            /// @brief Sets the handler for a bag item dropped on the Fortune Egg machine form.
            /// @param dropOnEgg Called with the bag place and the machine's place.
            void setEggDrop(std::function<void(int slot, int place)> dropOnEgg) { m_dropOnEgg = std::move(dropOnEgg); }

            /// @brief Sets the handler for a bag item dropped on a display case form.
            /// @param dropOnCase Called with the bag place.
            void setCaseDrop(std::function<void(int slot)> dropOnCase) { m_dropOnCase = std::move(dropOnCase); }

            /// @brief Sets the handler for a bag item dropped on a cupboard form.
            /// @param dropOnCupboard Called with the bag place and the cupboard place.
            void setCupboardDrop(std::function<void(int slot, int place)> dropOnCupboard) { m_dropOnCupboard = std::move(dropOnCupboard); }

            /// @brief Sets the handler for a bag item dropped on the Fridge/cabinets or the Locker.
            /// @param dropOnHouseBox Called with locker flag, bag place and box place.
            void setHouseBoxDrop(std::function<void(bool locker, int slot, int place)> dropOnHouseBox) { m_dropOnHouseBox = std::move(dropOnHouseBox); }

            /// @brief Sets the handler for a bag item dropped on the Bookcase or the Desk.
            /// @param dropOnShelf Called with desk flag, bag place and shelf place.
            void setBookShelfDrop(std::function<void(bool desk, int slot, int place)> dropOnShelf) { m_dropOnBookShelf = std::move(dropOnShelf); }

            /// @brief Sets the handler for a bag item dropped on a place of the F1-F8 bar.
            /// @param putOnBar Called with the bag place and item id.
            /// @details Original: SetHotKeyItem.
            void setHotKeyDrop(std::function<void(int slot, int itemId)> putOnBar) { m_putOnBar = std::move(putOnBar); }

            /// @brief Sets the handler for a bag item let go on the Item Bag cells.
            /// @param drop Drop handler.
            /// @details Original: HandleGridItemDrop, slot kind 15.
            void setItemBagDrop(std::function<void(const ui::EventArgs&)> drop) { m_dropOnItemBag = std::move(drop); }

            /// @brief Sets the handler for a bag item let go on the storeroom.
            /// @param drop Drop handler.
            void setStoreroomDrop(std::function<void(const ui::EventArgs&)> drop) { m_dropOnStoreroom = std::move(drop); }

            /// @brief Sets where a Shift click sends a bag stack the modern way (an open Storeroom).
            /// @param quickMove Handler given the bag place; it returns false when nothing is open to take it.
            void setQuickMove(std::function<bool(int slot)> quickMove) { m_quickMove = std::move(quickMove); }

            /// @brief Gives the window the bag move/drop requests (questions and refusals).
            /// @param requests Request helper; the window asks how many with Ctrl held.
            void setBagRequests(BagRequests& requests) { m_requests = requests; }

            /// @brief The bag place being dragged by the mouse.
            /// @return The place, or none when no bag item is carried.
            std::optional<int> draggedBagItem() const;

            /// @brief Sets the handler for an item let go over the map.
            /// @param dropOnWorld Called with bag place, item id and count; returns true when the scene took it, else it is dropped on the ground.
            void setWorldDrop(std::function<bool(int slot, int itemId, int count)> dropOnWorld) { m_dropOnWorld = std::move(dropOnWorld); }

            /// @brief Sets the handler for a right click on items the scene handles.
            /// @param rightClicked Called with item id and bag place.
            /// @details Original: ItemSlotRightClick (e.g. a pitched tent taken back).
            void setRightClick(std::function<void(int itemId, int slot)> rightClicked) { m_rightClicked = std::move(rightClicked); }

            /// @brief Sets the handler for the Potential button.
            /// @param toggle Handler.
            void setPotentialToggle(std::function<void()> toggle) { m_togglePotential = std::move(toggle); }

            /// @brief Sets the handler for the Express button of a pet page.
            /// @param open Called with the pet shown.
            /// @details Original: OpenPetExpress.
            void setExpressOpen(std::function<void(const Pet&)> open) { m_openExpress = std::move(open); }

            /// @brief Sets the query telling whether a pet is up for sale (its gear cannot change).
            /// @param inStall Returns true for the team place.
            /// @details Original: Pet.FInStall.
            void setPetInStall(std::function<bool(int place)> inStall) { m_petInStall = std::move(inStall); }

            /// @brief Screen position of a bag place.
            /// @param slot Bag place.
            /// @return Its top left corner.
            core::Point bagCellPosition(int slot) const { return m_window.bounds().position() + m_bag.bounds().position() + m_bag.cellPosition(slot); }

            /// @brief Makes the window lie over the popups.
            /// @details Original: SetFormActiveState.
            void raiseAbovePopups() {
                if (m_screen.context().bringAbovePopups) {
                    m_screen.context().bringAbovePopups(m_window.id());
                }
            }

            /// @brief Window bounds on the screen.
            /// @return The bounds rectangle.
            core::Rect bounds() const { return m_window.bounds(); }

            /// @brief Moves the window horizontally.
            /// @param left New left edge.
            void moveLeft(int left) {
                const core::Rect area = m_window.bounds();
                m_window.setBounds({left, area.y, area.width, area.height});
            }

            /// @brief Unequips for the player or a pet.
            /// @param order Character order (0 the player, else a pet).
            /// @param equipSlot Worn slot.
            /// @param bagSlot Bag place to put it in.
            /// @details Original: RequestUnequip with the shown character set first (the Remote Control's use).
            void unequipFor(int order, int equipSlot, int bagSlot);

            /// @brief Uses a bag item on the player or a pet.
            /// @param slot Bag place.
            /// @param order Character order (0 the player).
            /// @param fighting True when in a battle.
            /// @param remote True when called by the Remote Control.
            /// @details Original: RequestUseBagItem; the target's refusals, then 23/15.
            void requestUse(int slot, int order, bool fighting = false, bool remote = false);

            /// @brief Puts the item of a bag place on the player.
            /// @param slot Bag place.
            /// @details Original: HandleMagnetChoice 1 (a pet page cannot).
            void wearBagItem(int slot);

            /// @brief Bonus lines of a pet's gear change since its request went out.
            /// @param place Pet team place.
            /// @return The lines, empty when no request of that pet is waiting.
            /// @details Original: ShowEquipChangeMsg ("Attack increases 2"); the snapshot is taken with the request.
            std::string equipChangeLines(int place);

            /// @brief Sets the query telling whether the Remote Control fights (items are left alone in a battle).
            /// @param autoCombat Query.
            /// @details Original: UseGridItem.
            void setAutoCombat(std::function<bool()> autoCombat) { m_autoCombat = std::move(autoCombat); }

            /// @brief Sets the battle query and the battle item chooser.
            /// @param inBattle Returns true in a battle.
            /// @param chooseItem Called with item, bag place and count; the window then closes.
            /// @details Original: ChooseBattleItem.
            void setBattle(std::function<bool()> inBattle, std::function<void(int, int, int)> chooseItem) {
                m_inBattle = std::move(inBattle);
                m_chooseBattleItem = std::move(chooseItem);
            }

            /// @brief Updates the qol Sort button and runs one sort move per beat.
            /// @param nowMs Current time in ms.
            void updateSort(std::uint64_t nowMs);

            /// @brief Moves the window.
            /// @param at Its new top left corner.
            void setPosition(core::Point at) { m_window.setPosition(at); }

            /// @brief The bag's places the sort lock holds (qol).
            /// @return The places (1-based).
            const std::set<int>& lockedPlaces() const { return m_sortLocks.places(); }

            /// @brief The server's sort of this grid ended.
            /// @param result Result code.
            /// @details Server 23/250 2.
            void sortAnswered(int result) { m_sorter.serverAnswered(result); }

        private:
            /// @brief Paints the status column (level, HP, SP, exp, stats, gold or amity).
            /// @param canvas Target canvas.
            /// @param at Window origin on the screen.
            /// @param nowMs Current time in ms.
            void paintStatus(gfx::Canvas& canvas, core::Point at, std::uint64_t nowMs);

            /// @brief Paints one status bar.
            /// @param canvas Target canvas.
            /// @param at Window origin on the screen.
            /// @param bar Bar index.
            /// @param ratio Fill ratio 0..1.
            void paintBar(gfx::Canvas& canvas, core::Point at, int bar, double ratio);

            /// @brief Paints the shown character's name.
            /// @param canvas Target canvas.
            /// @param at Window origin on the screen.
            void paintName(gfx::Canvas& canvas, core::Point at);

            /// @brief The pet whose page is shown.
            /// @return The pet, or none for the player's page.
            /// @details Original: SelectShownCharacter, FShownPetOrderAlt (0 the player, else the order a pet joined in).
            std::optional<std::reference_wrapper<const Pet>> shownPet() const;

            /// @brief Team place of the shown pet.
            /// @return The place, 0 for the player.
            int shownPlace() const;

            /// @brief What the shown character wears in a slot.
            /// @param slot Worn slot.
            /// @return The worn item.
            const protocol::ItemInstance& shownEquipped(int slot) const;

            /// @brief Shows the page of a character.
            /// @param order 0 the player, else the order a pet joined in.
            void showCharacter(int order);

            /// @brief Steps to the next or previous character page.
            /// @param forward True for the next.
            /// @details Original: ShowNextCharacter, ShowPrevCharacter.
            void stepCharacter(bool forward);

            /// @brief Applies the shown character's level, element, gear and buttons.
            void applyShown();

            /// @brief Double click on a bag item: wears or uses it.
            /// @param slot Bag place.
            void useBagItem(int slot);

            /// @brief Right click on a bag item.
            /// @param slot Bag place.
            void rightClickBagItem(int slot);

            /// @brief Takes a worn item off into the first free bag place.
            /// @param equipSlot Worn slot.
            void unequip(int equipSlot);

            /// @brief Sends the unequip request.
            /// @param equipSlot Worn slot.
            /// @param bagSlot Bag place to put it in.
            /// @details Original: 23/12 for the player, 23/18 for a pet.
            void requestUnequip(int equipSlot, int bagSlot);

            /// @brief Puts a bag item on the shown character.
            /// @param slot Bag place.
            /// @details Original: 23/11 for the player, 23/17 for a pet.
            void equipFromBag(int slot);

            /// @brief Handles an item let go on the window.
            /// @param args Drop event.
            void dropped(const ui::EventArgs& args);

            /// @brief Stat points still free after the pending ones.
            /// @return Points left.
            int pointsLeft() const;

            /// @brief Puts one pending point on a stat.
            /// @param stat Stat index 0..4 (STR, CON, INT, WIS, AGI).
            /// @details Original: AddPendingStatPoint.
            void addStatPoint(int stat);

            /// @brief Sends the pending stat points.
            /// @details Original: SendStatPoints.
            void sendStatPoints();

            /// @brief Drops the pending stat points.
            /// @details Original: CancelStatPoints.
            void cancelStatPoints();

            /// @brief Shows or hides the point buttons to match the points left.
            void syncPointButtons();

            /// @brief Paints the hover tips.
            /// @param canvas Target canvas.
            /// @param at Window origin on the screen.
            /// @param nowMs Current time in ms.
            void paintTips(gfx::Canvas& canvas, core::Point at, std::uint64_t nowMs);

            /// @brief Sets which columns show.
            /// @param mode 0 both columns, 1 the status column only, 2 the bag only.
            /// @details Original: SetColumnMode.
            void setColumnMode(int mode);

            /// @brief The item under the mouse, its screen box and whether it lies in the bag.
            struct HoveredItem {
                protocol::ItemInstance item;
                core::Rect box;
                bool inBag = false;
            };

            /// @brief Finds the item under the mouse.
            /// @return The hovered item, or none.
            std::optional<HoveredItem> hoveredItem() const;

            /// @brief What the status column shows of the shown character.
            /// @details Original: TSe_EquipForm2.DrawControl.
            struct Shown {
                std::string name;
                int level = 0;
                int reborn = 0;
                std::int32_t hp = 0;
                std::int32_t maxHp = 0;
                int sp = 0;
                int maxSp = 0;
                std::int64_t exp = 0;
                std::array<int, 5> stats{};
                std::array<int, 5> combat{};
                int potential = 0;
                int element = 0;
            };

            /// @brief Collects the stats of the shown character.
            /// @return The stats to draw.
            Shown shownStats() const;

            GameServices& m_services;
            const PlayerStatus& m_status;
            const Inventory& m_inventory;
            const PetTeam& m_pets;
            std::optional<std::pair<int, EquipBonusSnapshot>> m_equipSnapshot;
            const BagMarks& m_marks;
            const MapCharacter& m_player;
            PlayerMessages& m_messages;
            ItemInfoPopup& m_info;
            ui::Screen& m_screen;
            ui::Window& m_window;
            ui::ImageView& m_element;
            ui::DigitText& m_level;
            ui::IconGrid& m_bag;
            std::array<std::reference_wrapper<ui::IconGrid>, Inventory::kEquipmentSlots> m_equipment;
            std::array<std::reference_wrapper<ui::ImageButton>, 5> m_pointButtons;
            ui::ImageButton& m_close;
            ui::ImageButton& m_pointsOk;
            ui::ImageButton& m_pointsCancel;
            std::array<int, 5> m_pending{};
            bool m_spending = false;
            int m_columnMode = 0;
            int m_shownOrder = 0;
            ui::ImageButton& m_closeSmall;
            ui::ImageButton& m_statusOnly;
            ui::ImageButton& m_itemsOnly;
            ui::ImageButton& m_potential;
            ui::ImageButton& m_express;
            ui::ImageButton& m_previousCharacter;
            ui::ImageButton& m_nextCharacter;
            ui::ImageView& m_levelWord;
            const ui::Spot& m_textSpot;
            const ui::Spot& m_buffedSpot;
            const ui::Spot& m_figureSpot;
            const ui::Spot& m_tipFillSpot;
            const ui::Spot& m_tipBorderSpot;
            const ui::Spot& m_expTipBorderSpot;
            FigureAnimation m_animation;
            std::function<bool(int)> m_petInStall;
            std::function<void()> m_togglePotential;
            std::function<void(const Pet&)> m_openExpress;
            std::function<bool()> m_inBattle;
            std::function<void()> m_openNotebook;
            std::function<bool(int, int, int)> m_useSpecial;
            std::function<bool(int)> m_buffed;
            std::function<std::uint32_t()> m_buffMask;
            std::function<void(int)> m_useVehicle;
            std::function<void(int)> m_dropOnRepair;
            std::map<std::string, std::function<void(int)>> m_bagDrops;
            std::function<void(int, int)> m_dropOnTank;
            std::function<void(int, int)> m_rightClicked;
            std::function<void(int, int)> m_putOnBar;
            std::function<bool(int, int, int)> m_dropOnWorld;
            std::function<void(const ui::EventArgs&)> m_dropOnBank;
            std::function<void(int)> m_dropOnVending;
            std::function<void(int, int)> m_dropOnEgg;
            std::function<void()> m_shown;
            std::optional<std::reference_wrapper<BagRequests>> m_requests;
            std::function<void(int)> m_dropOnCase;
            std::function<void(int, int)> m_dropOnCupboard;
            std::function<void(bool, int, int)> m_dropOnHouseBox;
            std::function<void(bool, int, int)> m_dropOnBookShelf;
            std::function<void(int, int, int)> m_chooseBattleItem;
            std::function<bool()> m_autoCombat;
            std::function<void(const ui::EventArgs&)> m_dropOnItemBag;
            std::function<void(const ui::EventArgs&)> m_dropOnStoreroom;
            std::function<bool(int)> m_quickMove;
            InventorySorter m_sorter;
            SortLocks m_sortLocks; // the qol lock button beside the sort button, and the places it keeps out of a sort
    };

}

#endif
