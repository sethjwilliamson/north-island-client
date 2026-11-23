#ifndef NORTH_ISLAND_CLIENT_VIEWMODELS_PETS_EXPRESS_WINDOW_H
#define NORTH_ISLAND_CLIENT_VIEWMODELS_PETS_EXPRESS_WINDOW_H

#include "client/services/GameServices.h"
#include "client/models/character/FigureAnimation.h"
#include "client/models/social/FriendList.h"
#include "client/models/guild/Guild.h"
#include "client/models/items/BagMarks.h"
#include "client/models/items/Inventory.h"
#include "client/models/pets/Pet.h"
#include "client/models/pets/PetTeam.h"
#include "client/viewmodels/chat/PlayerMessages.h"
#include "ui/CheckBox.h"
#include "ui/DrawingArea.h"
#include "ui/IconGrid.h"
#include "ui/ListBox.h"
#include "ui/Screen.h"
#include "ui/Spot.h"
#include "ui/TextBox.h"
#include "ui/Window.h"

#include <array>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace client {

    /// @brief The pet express form: up to five bag items are put on its places, a recipient is typed or picked, and the pet is sent off with them.
    /// @details Original: ExpressWindow.xml, TH_Menu.pas TTH_ExpressForm. Send is 25/20; the server says when the pet has gone and when it is back (25/20), what it brought (25/23) and what went wrong (25/22); Stop calls it back (25/21).
    class ExpressWindow {
        public:
            /// @brief Callbacks into the rest of the game.
            /// @details Original: FindWorldPlayerByName (findPlayer), AddTalkMessage(TalkMsgForm, id, text, 10, 255) (chatLine), Player.FActivePetId (activePetKind).
            struct Hooks {
                std::function<std::optional<std::int32_t>(const std::string&)> findPlayer;
                std::function<std::string(std::int32_t)> playerName;
                std::function<void(std::int32_t, const std::string&)> chatLine;
                std::function<std::vector<std::string>()> teamNames;
                std::function<std::int32_t()> ownId;
                std::function<int()> activePetKind;
                std::function<void()> marksChanged;
                std::function<bool()> autoDelivering;
            };

            static constexpr int kPlaces = 5;
            static constexpr int kExpressMark = BagMarks::kExpress;
            static constexpr int kDeliveringBusy = 2;

            /// @brief Loads the form and wires all its handlers; centres the form on the screen.
            /// @param services Shared game services.
            /// @param screen Screen the form is loaded into.
            /// @param inventory The player's bag.
            /// @param marks Bag cell marks (use / busy).
            /// @param pets The pet team.
            /// @param friends The friend list (for the recipient list).
            /// @param guild The guild (for the recipient list).
            /// @param messages Sink for on-screen messages.
            /// @param hooks Callbacks into the rest of the game.
            ExpressWindow(
                GameServices& services,
                ui::Screen& screen,
                const Inventory& inventory,
                BagMarks& marks,
                const PetTeam& pets,
                const FriendList& friends,
                const Guild& guild,
                PlayerMessages& messages,
                Hooks hooks
            );

            /// @brief Tells whether the form is showing.
            /// @return True when the window is visible.
            bool isOpen() const { return m_window.isVisible(); }

            /// @brief Tells whether the pet of this form is off, or asked to go with the answer still out.
            /// @return True while busy.
            bool isBusy() const { return m_delivering || m_sentMs != 0; }

            /// @brief Shows the form for a pet, with the name list ready.
            /// @details Original: ShowCtrl and SetExpressPet.
            /// @param pet The pet that will be sent.
            void open(const Pet& pet);

            /// @brief Hides the form.
            /// @details Original: HideCtrl.
            void hide() { m_window.setVisible(false); }

            /// @brief Redraws the bag cells and the five places; a place whose item is gone is let go.
            /// @details Original: RefreshGridItems (the bag changed), RefreshExpressSlots.
            void refresh();

            /// @brief Once a frame: keeps the list panel on the screen, and gives up waiting for a send's answer.
            /// @param nowMs Current time in ms.
            void update(std::uint64_t nowMs);

            /// @brief Handles the server's delivery state: 1 the pet is off, 2 it is back or was called back.
            /// @details Original: RecvPetDeliveryState (25/20).
            /// @param petKind Npc.dat kind of the pet.
            /// @param state 1 the pet is off, 2 it is back or was called back.
            void deliveryState(std::int32_t petKind, int state);

            /// @brief Shows the reason an express failed.
            /// @details Original: RecvExpressError (25/22).
            /// @param code Error code (1 not enough quantity ... 8 on the target's blacklist).
            /// @param itemId Item the error is about (code 1).
            void failed(int code, int itemId);

            /// @brief Shows the outcome of an express, on the screen and in the chat.
            /// @details Original: RecvExpressResult (25/23): kind 1 an item came from another player, 2 an item went to him.
            /// @param kind 1 received, 2 sent.
            /// @param otherId Id of the other player.
            /// @param itemId Item moved.
            /// @param count Quantity moved.
            /// @param onScreen False to leave it to the chat (Auto Deliver's trips).
            void result(int kind, std::int32_t otherId, int itemId, int count, bool onScreen = true);

            /// @brief Calls the pet back (also on leaving the game).
            /// @details Original: StopExpress (25/21).
            void stop();

        private:
            /// @brief Puts a bag item on the first free place after the allowed-item checks.
            /// @details Original: AddExpressItem, SetExpressDownSlot.
            /// @param slot Bag slot clicked.
            void addItem(int slot);

            /// @brief Takes a bag item off its place (right button on a bag cell).
            /// @details Original: RemoveExpressItem.
            /// @param slot Bag slot.
            void removeItem(int slot);

            /// @brief Takes the item off a place (right button on the place; not while the pet is out).
            /// @details Original: RemoveExpressSlot.
            /// @param place Place number 1..5.
            void removeFromPlace(int place);

            /// @brief Finds the first free place.
            /// @return Place number, or 0 when all are taken.
            int freePlace() const;

            /// @brief Sends the pet off with the items to the typed recipient.
            /// @details Original: SendExpress (25/20).
            void send();

            /// @brief Closes the form; items are let go and the pet forgotten unless it is out.
            /// @details Original: CloseExpressForm.
            void closeForm();

            /// @brief Marks the items of the pet that left as in use and busy.
            /// @details Original: LockExpressItems.
            void lockItems();

            /// @brief Lets all placed items go.
            /// @details Original: ClearExpressItems, ReleaseExpressItems.
            void clearItems();

            /// @brief Shows or hides the name list panel.
            /// @details Original: ToggleNameList, ToggleNameFilter.
            void toggleNames();

            /// @brief Hides the name list panel.
            /// @details Original: CloseNameList.
            void closeNames();

            /// @brief Refills the list with the team, friends and guild members as the ticks say, each name once.
            /// @details Original: RefillNameList.
            void refillNames();

            /// @brief Puts a picked name into the recipient box and hides the panel.
            /// @details Original: PickRecipientName.
            /// @param row List row picked.
            void pickName(int row);

            /// @brief Shows or hides the list panel and its controls.
            /// @param shown True to show.
            void showNames(bool shown);

            /// @brief Positions the list controls by the panel: right of the form, or left when it would leave the screen.
            /// @details Original: DrawControl.
            void placeNames();

            /// @brief Draws the pet (turned away while it is out) and its name over its head.
            /// @details Original: DrawControl.
            /// @param canvas Canvas to draw on.
            /// @param at Position of the drawing area.
            /// @param nowMs Current time in milliseconds.
            void paintPet(gfx::Canvas& canvas, core::Point at, std::uint64_t nowMs);

            /// @brief Draws the item icon of a place and its count; sets the name as the hint.
            /// @details Original: TComImage of the form.
            /// @param canvas Canvas to draw on.
            /// @param at Position of the place's area.
            /// @param place Place number 1..5.
            void paintPlace(gfx::Canvas& canvas, core::Point at, int place);

            GameServices& m_services;
            ui::Screen& m_screen;
            const Inventory& m_inventory;
            BagMarks& m_marks;
            const PetTeam& m_pets;
            const FriendList& m_friends;
            const Guild& m_guild;
            PlayerMessages& m_messages;
            Hooks m_hooks;
            ui::Window& m_window;
            ui::IconGrid& m_bag;
            std::array<std::reference_wrapper<ui::DrawingArea>, kPlaces> m_placeAreas;
            ui::TextBox& m_recipient;
            ui::ListBox& m_names;
            std::array<std::reference_wrapper<ui::CheckBox>, 3> m_filters; // three recipient-list tick boxes: team, friends, guild
            ui::DrawingArea& m_paintArea;
            const ui::Spot& m_petSpot;
            const ui::Spot& m_nameLineSpot;
            const ui::Spot& m_textColorSpot;
            const ui::Spot& m_sideWidthSpot;
            const ui::Spot& m_sideListSpot;
            const ui::Spot& m_sideScrollSpot;
            const ui::Spot& m_sideCloseSpot;
            const ui::Spot& m_sideCloseSmallSpot;
            const ui::Spot& m_sideFilterSpot;
            const ui::Spot& m_iconSpot;
            const ui::Spot& m_countSpot;
            std::array<int, kPlaces + 1> m_bagSlots{};
            int m_downSlot = 0;
            int m_petKind = 0;
            formats::NpcInfo m_petInfo;
            std::string m_petName;
            bool m_delivering = false;
            bool m_stopped = false;
            std::uint64_t m_sentMs = 0; // when Send asked, 0 once answered
            std::uint64_t m_nowMs = 0;
            FigureAnimation m_animation;
    };

}

#endif
