#ifndef NORTH_ISLAND_CLIENT_VIEWMODELS_ITEMS_MAGIC_BOX_WINDOW_H
#define NORTH_ISLAND_CLIENT_VIEWMODELS_ITEMS_MAGIC_BOX_WINDOW_H

#include "client/services/GameServices.h"
#include "client/models/items/BagMarks.h"
#include "client/models/items/Inventory.h"
#include "client/viewmodels/items/ItemInfoPopup.h"
#include "client/viewmodels/chat/PlayerMessages.h"
#include "core/Bytes.h"
#include "core/Rect.h"
#include "gfx/Canvas.h"
#include "protocol/ItemInstance.h"
#include "ui/Screen.h"
#include "ui/Spot.h"
#include "ui/Window.h"

#include <functional>
#include <string>

namespace client {

    /// @brief The Rebirth Box form: drops a pose piece to return its look to the original.
    /// @details Original: MagicBoxWindow.xml, HL_Menu.pas TMagicBoxForm, item 34198; OK sends 23/105 option 2, the answer is 23/105 3 or 4.
    class MagicBoxWindow {
        public:
            /// @brief Callbacks into the surrounding scene.
            struct Hooks {
                std::function<core::Rect()> itemWindowBounds;
                std::function<void()> marksChanged;
                std::function<void(const core::Bytes&)> send;
                std::function<void(const std::string&, std::function<void()>)> ask;
                std::function<void(int bagSlot)> reverted; // the piece in a bag place is back to its own look
            };

            static constexpr int kMark = BagMarks::kMagicBox;
            static constexpr int kBoxItem = 34198; // item 34198: Rebirth Box (type 25)

            /// @brief Builds the form and wires its controls.
            /// @param services Shared game services.
            /// @param screen Screen the form lives on.
            /// @param inventory Bag contents.
            /// @param marks Bag cell marks.
            /// @param messages Player message sink.
            /// @param info Item info popup.
            /// @param hooks Scene callbacks.
            MagicBoxWindow(
                GameServices& services,
                ui::Screen& screen,
                const Inventory& inventory,
                BagMarks& marks,
                PlayerMessages& messages,
                ItemInfoPopup& info,
                Hooks hooks
            );

            /// @brief Whether the form is visible.
            /// @return True when open.
            bool isOpen() const { return m_window.isVisible(); }

            /// @brief Opens the form for the box of a bag place.
            /// @param boxSlot Bag place of the box.
            /// @details Original: OpenMagicBoxForm.
            void open(int boxSlot);

            /// @brief A bag item was dropped on the form's place.
            /// @param bagSlot Bag place of the piece.
            /// @details Original: PutMagicBoxItem.
            void put(int bagSlot);

            /// @brief The server's answer.
            /// @param code 3 the look is back, 4 it failed.
            /// @details Original: RecvMagicBoxResult (server 23/105 3 or 4).
            void result(int code);

            /// @brief Checks the box and piece still exist and shows the piece's info.
            /// @details Original: DrawControl, CheckMagicBoxItems.
            void update();

        private:
            /// @brief Hides the form; empties the places when no request waits.
            /// @details Original: HideCtrl.
            void close();

            /// @brief Frees the box and piece cells and clears the request flag.
            /// @details Original: ResetMagicBoxForm.
            void reset();

            /// @brief Takes the piece out of the place (nothing while a request waits).
            /// @details Original: ClearMagicBoxImage.
            void clearImage();

            /// @brief Asks the player to confirm.
            /// @details Original: ConfirmMagicBox.
            void confirm();

            /// @brief Sends the request and closes the form.
            /// @details Original: SendMagicBox.
            void send();

            /// @brief Paints the piece in the place.
            /// @param canvas Target canvas.
            /// @param at Top left of the image area.
            void paint(gfx::Canvas& canvas, core::Point at);

            /// @brief Clears this form's mark on a bag cell.
            /// @param slot Bag place.
            void release(int slot);

            GameServices& m_services;
            ui::Screen& m_screen;
            const Inventory& m_inventory;
            BagMarks& m_marks;
            PlayerMessages& m_messages;
            ItemInfoPopup& m_info;
            Hooks m_hooks;
            ui::Window& m_window;
            ui::Element& m_image;
            const ui::Spot& m_countSpot;
            int m_boxSlot = 0;
            int m_bagSlot = 0;
            int m_sentSlot = 0; // the piece's bag place in the request waiting for its answer
            bool m_requestSent = false;
            protocol::ItemInstance m_shown;
    };

}

#endif
