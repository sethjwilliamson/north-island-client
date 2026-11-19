#ifndef NORTH_ISLAND_CLIENT_VIEWMODELS_LOGIN_DISCONNECT_FORM_H
#define NORTH_ISLAND_CLIENT_VIEWMODELS_LOGIN_DISCONNECT_FORM_H

#include "client/services/GameServices.h"
#include "client/rules/DisconnectNotice.h"
#include "ui/DrawingArea.h"
#include "ui/Element.h"
#include "ui/ImageButton.h"
#include "ui/Screen.h"
#include "ui/Spot.h"
#include "ui/TextBox.h"

#include <functional>
#include <string>

namespace client {

    /// @brief Disconnection form showing the server's reason or the lost connection, with Prev., Quit, End or Update buttons.
    /// @details Original: DisconnectForm.xml, DZ_Menu.pas TDZ_WillCloseForm1.
    class DisconnectForm {
        public:
            /// @brief Actions the form's buttons trigger.
            struct Hooks {
                std::function<void()> previous;
                std::function<void()> quit;
            };

            /// @brief Loads DisconnectForm.xml and wires its buttons.
            /// @param services Shared game services.
            /// @param screen Screen the layout is loaded into.
            /// @param hooks Prev. and Quit actions.
            DisconnectForm(
                GameServices& services,
                ui::Screen& screen,
                Hooks hooks
            );

            /// @brief Tells whether the form is shown.
            /// @return True when open.
            bool isOpen() const { return m_layer.isVisible(); }

            /// @brief Shows the form with the notice text (first 20 characters) and the buttons its reason asks for.
            /// @details Original: ShowCtrl.
            /// @param notice Reason text and button set.
            void show(const DisconnectNotice& notice);

            /// @brief Shows the form with the text only; buttons stay as they were (Prev., Update and Quit).
            /// @details Original: ShowCtrl.
            /// @param text Text for the field (first 20 characters show).
            void show(const std::string& text);

            /// @brief Hides the form and empties the field.
            /// @details Original: HideCtrl.
            void hide();

        private:
            /// @brief Sets which of the four buttons are visible.
            /// @param previous Show Prev.
            /// @param update Show Update.
            /// @param quit Show Quit.
            /// @param end Show End.
            void setButtons(bool previous, bool update, bool quit, bool end);

            /// @brief Paints the centred captions of the visible buttons.
            /// @param canvas Target canvas.
            /// @param at Origin of the drawing area.
            void paintCaptions(gfx::Canvas& canvas, core::Point at);

            /// @brief Tells the services' errorShown hook (a test script ends there) what the form shows.
            /// @param text The notice text.
            void reportShown(const std::string& text);

            GameServices& m_services;
            Hooks m_hooks;
            ui::Element& m_layer;
            ui::TextBox& m_text;
            ui::ImageButton& m_previous;
            ui::ImageButton& m_update;
            ui::ImageButton& m_quit;
            ui::ImageButton& m_end;
            const ui::Spot& m_captionSpot;
    };

}

#endif
