#include "client/scenes/world/WorldScene.h"

#include <algorithm>

namespace client {

    void WorldScene::setUpMovableWindows() {
        const auto window = [this](const char* name) { return &m_screen.get<ui::Window>(name); };
        // a toggle serves as open and close: open is called on a closed kind alone, close on an open one
        m_movableWindows = {
            {"item", {window("ItemWindow")}, [this] { m_itemWindow.open(); }, [this] { m_itemWindow.close(); }},
            {"skills", {window("SkillWindow")}, [this] { m_skillWindow.open(); }, [this] { m_skillWindow.close(); }},
            {"settings", {window("SettingsWindow")}, [this] { m_systemWindow.open(); }, [this] { m_systemWindow.close(); }},
            {"group",
             {window("GroupWindow"), window("GuildWindow"), window("InstanceWindow")},
             [this] { m_groupWindow.open(); },
             [this] { m_groupWindow.close(); }},
            {"social", {window("SocialWindow")}, [this] { m_socialWindow.open(); }, [this] { m_socialWindow.close(); }},
            {"compound", {window("CompoundWindow")}, [this] { m_compoundWindow.open(); }, [this] { m_compoundWindow.close(); }},
            {"money", {window("MoneyMenu")}, [this] { m_moneyMenu.toggle(); }, [this] { m_moneyMenu.close(); }},
            {"quests", {window("Quests")}, [this] { toggleQuests(); }, [this] { m_quests.toggle(); }},
            {"monsterBook", {window("MonsterBook")}, [this] { m_monsterBookWindow.toggle(); }, [this] { m_monsterBookWindow.close(); }},
            {"blacklist", {window("Blacklist")}, [this] { m_blacklist.toggle(); }, [this] { m_blacklist.toggle(); }},
            {"msgSkin", {window("MsgSkin")}, [this] { m_msgSkin.toggle(); }, [this] { m_msgSkin.toggle(); }},
            {"potential", {window("PotentialWindow")}, [this] { m_potentialWindow.toggle(); }, [this] { m_potentialWindow.close(); }},
            {"activities", {window("ActivitiesWindow")}, [this] { toggleActivities(); }, [this] { m_activities.close(); }},
            {"retainFur", {window("RetainFurWindow")}, [this] { m_retainFur.toggle(); }, [this] { m_retainFur.close(); }},
        };
        m_screen.setPressHook([this](int elementId) {
            for (MovableKind& kind : m_movableWindows) {
                const ui::Window* shown = shownWindowOf(kind);
                if (shown != nullptr && shown->contains(elementId)) {
                    kind.usedAtMs = m_nowMs;
                }
            }
        });
    }

    void WorldScene::trackMovableWindows() {
        for (MovableKind& kind : m_movableWindows) {
            const bool open = shownWindowOf(kind) != nullptr;
            if (open && !kind.wasOpen) {
                kind.usedAtMs = m_nowMs;
            }
            kind.wasOpen = open;
        }
    }

    ui::Window* WorldScene::shownWindowOf(const MovableKind& kind) {
        const auto shown = std::find_if(kind.windows.begin(), kind.windows.end(), [](const ui::Window* window) { return window->isVisible(); });
        return shown != kind.windows.end() ? *shown : nullptr;
    }

    WorldScene::MovableKind* WorldScene::movableKind(const std::string& key) {
        const auto found = std::find_if(m_movableWindows.begin(), m_movableWindows.end(), [&key](const MovableKind& kind) { return kind.key == key; });
        return found != m_movableWindows.end() ? &*found : nullptr;
    }

    std::optional<WorldScene::MovableWindow> WorldScene::lastUsedWindow() const {
        std::optional<MovableWindow> last;
        for (const MovableKind& kind : m_movableWindows) {
            const ui::Window* shown = shownWindowOf(kind);
            if (shown == nullptr || (last && last->usedAtMs >= kind.usedAtMs)) {
                continue;
            }
            if (const auto position = m_screen.screenPositionOf(*shown)) { // nothing while its layer is hidden (an exclusive talk)
                last = MovableWindow{kind.key, *position, kind.usedAtMs};
            }
        }
        return last;
    }

    std::vector<WorldScene::MovableWindow> WorldScene::openWindows() const {
        std::vector<MovableWindow> open;
        for (const MovableKind& kind : m_movableWindows) {
            const ui::Window* shown = shownWindowOf(kind);
            if (shown == nullptr) {
                continue;
            }
            if (const auto position = m_screen.screenPositionOf(*shown)) {
                open.push_back(MovableWindow{kind.key, *position, kind.usedAtMs});
            }
        }
        // opened again in this order, the one used last ends up in front
        std::stable_sort(open.begin(), open.end(), [](const MovableWindow& a, const MovableWindow& b) { return a.usedAtMs < b.usedAtMs; });
        return open;
    }

    bool WorldScene::closeMovableWindow(const std::string& key) {
        MovableKind* kind = movableKind(key);
        if (kind == nullptr || shownWindowOf(*kind) == nullptr) {
            return false;
        }
        kind->close();
        kind->wasOpen = shownWindowOf(*kind) != nullptr;
        return true;
    }

    bool WorldScene::openMovableWindow(const std::string& key, core::Point position) {
        MovableKind* kind = movableKind(key);
        if (kind == nullptr || !m_inWorld || m_fightState != 0) {
            return false;
        }
        if (shownWindowOf(*kind) == nullptr) {
            kind->open();
        }
        ui::Window* shown = shownWindowOf(*kind);
        if (shown == nullptr) {
            return false; // a precondition failed (no quest notebook, not in a floor room, ...)
        }
        m_screen.placeAt(*shown, position); // also while its layer is hidden (an exclusive talk)
        if (m_screen.context().bringToFront) {
            m_screen.context().bringToFront(shown->id());
        }
        kind->usedAtMs = m_nowMs;
        kind->wasOpen = true;
        return true;
    }

}
