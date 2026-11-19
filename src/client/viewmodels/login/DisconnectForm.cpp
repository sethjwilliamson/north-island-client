#include "client/viewmodels/login/DisconnectForm.h"

namespace client {

    namespace {

        constexpr std::size_t kShownChars = 20; // most characters of the notice text shown

        /// @brief Loads DisconnectForm.xml into the screen.
        /// @param services Shared game services.
        /// @param screen Screen the layout is loaded into.
        /// @return The loaded form layer.
        ui::Element& loadForm(GameServices& services, ui::Screen& screen) {
            return screen.load(services.game.layout("DisconnectForm.xml"), services.loadContext());
        }

    }

    DisconnectForm::DisconnectForm(
        GameServices& services,
        ui::Screen& screen,
        Hooks hooks
    )
        : m_services(services),
          m_hooks(std::move(hooks)),
          m_layer(loadForm(services, screen)),
          m_text(screen.get<ui::TextBox>("DisconnectText")),
          m_previous(screen.get<ui::ImageButton>("DisconnectPrevious")),
          m_update(screen.get<ui::ImageButton>("DisconnectUpdate")),
          m_quit(screen.get<ui::ImageButton>("DisconnectQuit")),
          m_end(screen.get<ui::ImageButton>("DisconnectEnd")),
          m_captionSpot(screen.get<ui::Spot>("DisconnectCaption")) {
        screen.on("onDisconnectPrevious", [this](const ui::EventArgs&) {
            hide();
            m_hooks.previous();
        });
        screen.on("onDisconnectQuit", [this](const ui::EventArgs&) { m_hooks.quit(); });
        screen.get<ui::DrawingArea>("DisconnectCaptions").setPainter([this](gfx::Canvas& canvas, core::Point at, std::uint64_t) { paintCaptions(canvas, at); });
        setButtons(true, true, true, false);
    }

    void DisconnectForm::setButtons(bool previous, bool update, bool quit, bool end) {
        m_previous.setVisible(previous);
        m_update.setVisible(update);
        m_quit.setVisible(quit);
        m_end.setVisible(end);
    }

    void DisconnectForm::show(const DisconnectNotice& notice) {
        m_text.setTextFromStart(notice.text.substr(0, kShownChars));
        switch (notice.buttons) {
            case DisconnectNotice::Buttons::PreviousAndQuit: setButtons(true, false, true, false); break; // Prev. and Quit
            case DisconnectNotice::Buttons::UpdateAndQuit: setButtons(false, true, true, false); break; // Update and Quit
            case DisconnectNotice::Buttons::End: setButtons(false, false, false, true); break; // End only
        }
        m_layer.setVisible(true);
        reportShown(notice.text);
    }

    void DisconnectForm::show(const std::string& text) {
        m_text.setTextFromStart(text.substr(0, kShownChars));
        m_layer.setVisible(true);
        reportShown(text);
    }

    void DisconnectForm::reportShown(const std::string& text) {
        if (m_services.errorShown) {
            m_services.errorShown("disconnection form: " + text);
        }
    }

    void DisconnectForm::hide() {
        m_layer.setVisible(false);
        m_text.clear();
    }

    void DisconnectForm::paintCaptions(gfx::Canvas& canvas, core::Point at) {
        const auto caption = [&](const ui::ImageButton& button, const std::string& text) {
            if (!button.isVisible()) {
                return;
            }
            const core::Rect area = button.bounds();
            const core::Point where{
                at.x + area.x + (area.width - static_cast<int>(text.size()) * m_captionSpot.pitch().x) / 2,
                at.y + area.y + m_captionSpot.at().y
            };
            canvas.drawText(where, text, {m_captionSpot.color(), gfx::TextEffect::Shadow, std::nullopt});
        };
        if (!m_previous.isVisible()) {
            caption(m_update, "Update");
        }
        caption(m_previous, "Prev.");
        caption(m_quit, "Quit");
        caption(m_end, "End");
    }

}
