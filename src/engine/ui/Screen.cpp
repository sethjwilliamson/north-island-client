#include "ui/Screen.h"

#include "ui/BlockList.h"
#include "ui/DrawingArea.h"
#include "ui/IconGrid.h"
#include "ui/ImageButton.h"

#include "core/FileSystem.h"
#include "gfx/TextStyle.h"
#include "ui/LayoutLoader.h"
#include "ui/ListBox.h"
#include "ui/Memo.h"
#include "ui/ScrollBar.h"
#include "ui/TextBox.h"
#include "ui/Tooltip.h"
#include "xml/Parser.h"

#include <cctype>
#include <cmath>
#include <unordered_set>

namespace ui {

    namespace {

        /// @brief Gives every element of a tree a unique id, depth first.
        /// @param element Root of the tree.
        /// @param next Next free id; advanced past the ids handed out.
        void assignIds(Element& element, int& next) {
            element.setId(next++);
            for (const auto& child : element.children()) {
                assignIds(*child, next);
            }
        }

        /// @brief Finds the first element of that name in a tree, depth first.
        /// @param element Root of the tree.
        /// @param name Element name.
        /// @return The element, or nothing.
        std::optional<std::reference_wrapper<Element>> findNamed(Element& element, const std::string& name) {
            if (element.name() == name) {
                return std::ref(element);
            }
            for (const auto& child : element.children()) {
                if (auto found = findNamed(*child, name)) {
                    return found;
                }
            }
            return std::nullopt;
        }

        /// @brief Collects the rectangles of the visible windows (ui::Window) in a tree; does not descend into windows.
        /// @param element Root of the tree.
        /// @param origin Screen position of the element's parent.
        /// @param windows Receives the window rectangles in the screen's coordinates.
        void collectWindows(const Element& element, core::Point origin, std::vector<core::Rect>& windows) {
            if (!element.isVisible()) {
                return;
            }
            const core::Point position = origin + element.bounds().position();
            if (dynamic_cast<const Window*>(&element) != nullptr) {
                windows.push_back({position.x, position.y, element.bounds().width, element.bounds().height});
                return;
            }
            for (const auto& child : element.children()) {
                collectWindows(*child, position, windows);
            }
        }

        /// @brief Finds the topmost element whose hit test (or overlay hit test) takes the point: children before their parent, later children before earlier ones.
        /// @param element Root of the tree.
        /// @param origin Screen position of the element's parent.
        /// @param mouse Mouse position in the layout's coordinates.
        /// @param pictures Picture library for pixel hit tests.
        /// @param overlay Whether to test the overlays (open drop-down lists) instead of the elements.
        /// @return The element id, 0 for none.
        int topmostHit(
            Element& element,
            core::Point origin,
            core::Point mouse,
            const gfx::PictureLibrary& pictures,
            bool overlay
        ) {
            if (!element.isVisible()) {
                return 0;
            }
            const core::Point position = origin + element.bounds().position();
            const auto& children = element.children();
            const bool childrenShown =
                !element.clipsChildren() || core::Rect{position.x, position.y, element.bounds().width, element.bounds().height}.contains(mouse);
            for (auto child = children.rbegin(); childrenShown && child != children.rend(); ++child) {
                if (const int id = topmostHit(**child, position, mouse, pictures, overlay)) {
                    return id;
                }
            }
            const core::Point local = mouse - position;
            const bool hit = overlay ? element.hitTestOverlay(local) : element.hitTest(local, pictures);
            return hit ? element.id() : 0;
        }

    }

    Screen::Screen(
        gfx::PictureLibrary& pictures
    )
        : m_root(std::make_unique<Element>()),
          m_pictures(pictures) {
        m_context.focusByName = [this](const std::string& name) { focus(name); };
        m_context.bringToFront = [this](int id) { m_root->raiseChildHolding(id); };
        m_context.bringAbovePopups = [this](int id) { m_root->raiseChildHoldingAbovePopups(id); };
    }

    Element& Screen::load(const std::filesystem::path& layoutFile, const LoadContext& loadContext) {
        const auto data = core::readFile(layoutFile);
        if (!data) {
            throw LayoutError("cannot read " + layoutFile.string());
        }
        return load(LayoutSource{layoutFile.filename().string(), core::toString(*data)}, loadContext);
    }

    void Screen::prefetchPictures(const xml::Node& node, std::vector<std::string>& started) {
        // the attributes that name pictures (Window, ImageButton, ImageView, CheckBox, ComboBox, IconGrid, Memo, Panel, ScrollBar, ...)
        static const std::unordered_set<std::string> kPictureAttributes{
            "Picture", "Animation", "Caret", "CellPicture", "CheckedPicture", "DropButton", "ScrollBar", "Skin", "Thumb", "Track"
        };
        for (const auto& [name, value] : node.attributes()) {
            if (kPictureAttributes.count(name) != 0 && m_pictures.prefetch(value)) {
                started.push_back(value);
            }
        }
        for (const xml::Node& child : node.children()) {
            prefetchPictures(child, started);
        }
    }

    Element& Screen::load(const LayoutSource& source, const LoadContext& loadContext) {
        std::unique_ptr<Element> layout;
        std::vector<std::string> prefetched; // pictures decoding on workers while the layout is built
        const auto forgetPrefetched = [&] {
            for (const std::string& name : prefetched) {
                m_pictures.cancelPrefetch(name); // no-op for the ones the layout used
            }
        };
        try {
            const xml::Node root = xml::Parser::parse(source.text);
            if (!m_pictures.defersLoading()) { // pictures read on first use are not decoded ahead
                prefetchPictures(root, prefetched);
            }
            layout = LayoutLoader::build(root, loadContext);
        } catch (const std::exception& error) {
            forgetPrefetched();
            throw LayoutError(source.name + ": " + error.what());
        }
        forgetPrefetched();
        assignIds(*layout, m_nextId);
        layout->applyView(m_view, {});
        Element& added = *layout;
        m_root->addChild(std::move(layout));
        return added;
    }

    void Screen::setView(const core::Rect& view) {
        m_view = view;
        m_context.view = view;
        for (const auto& layout : m_root->children()) {
            layout->applyView(view, {});
        }
    }

    Element& Screen::findByName(const std::string& name) {
        if (m_namedAt != Element::treeChanges()) {
            m_named.clear(); // a tree grew, was reordered or renamed: the first match may differ now
            m_namedAt = Element::treeChanges();
        }
        if (const auto known = m_named.find(name); known != m_named.end()) {
            return *known->second;
        }
        auto found = findNamed(*m_root, name);
        if (!found) {
            throw LayoutError("the layout has no element " + name);
        }
        m_named.emplace(name, &found->get());
        return found->get();
    }

    std::optional<Screen::Located> Screen::locate(int id) {
        if (id == 0) {
            return std::nullopt;
        }
        std::function<std::optional<Located>(Element&, core::Point, LayoutRef)> search =
            [&](Element& element, core::Point origin, LayoutRef layout) -> std::optional<Located> {
            if (!element.isVisible()) {
                return std::nullopt;
            }
            const core::Point position = origin + element.bounds().position();
            if (element.id() == id) {
                return Located{element, position, layout};
            }
            for (const auto& child : element.children()) {
                if (auto found = search(*child, position, layout ? layout : LayoutRef(std::cref(*child)))) {
                    return found;
                }
            }
            return std::nullopt;
        };
        return search(*m_root, {0, 0}, std::nullopt);
    }

    core::Point Screen::mouseIn(const Element& element) {
        const auto located = locate(element.id());
        return located ? toLayout(m_mouse, located->layout) : m_mouse;
    }

    float Screen::scaleOf(LayoutRef layout) const { return layout && layout->get().isHudScaled() ? m_hudScale : 1.0f; }

    core::Point Screen::toLayout(core::Point point, LayoutRef layout) const {
        const float scale = scaleOf(layout);
        if (scale == 1.0f) {
            return point;
        }
        const core::Point pivot = layout->get().anchorPoint();
        return {pivot.x + static_cast<int>(std::floor((point.x - pivot.x) / scale)), pivot.y + static_cast<int>(std::floor((point.y - pivot.y) / scale))};
    }

    core::Point Screen::fromLayout(core::Point point, LayoutRef layout) const {
        const float scale = scaleOf(layout);
        if (scale == 1.0f) {
            return point;
        }
        const core::Point pivot = layout->get().anchorPoint();
        return {pivot.x + static_cast<int>(std::lround((point.x - pivot.x) * scale)), pivot.y + static_cast<int>(std::lround((point.y - pivot.y) * scale))};
    }

    int Screen::hitAt(core::Point position, const Element** layoutOut) {
        if (layoutOut != nullptr) {
            *layoutOut = nullptr;
        }
        const auto& layouts = m_root->children();
        for (const bool overlay : {true, false}) {
            for (auto layout = layouts.rbegin(); layout != layouts.rend(); ++layout) {
                if (!kept(**layout) || (*layout)->isSlidAway() || (*layout)->slide() != core::Point{}) {
                    continue; // filtered out, or sliding: the mouse is kept off a bar on its way
                }
                if (const int id = topmostHit(**layout, {0, 0}, toLayout(position, std::cref(**layout)), m_pictures, overlay)) {
                    if (layoutOut != nullptr) {
                        *layoutOut = layout->get();
                    }
                    return id;
                }
            }
        }
        return 0;
    }

    void Screen::refreshHover() {
        if (m_padFocusId != 0) {
            if (const auto focused = locate(m_padFocusId)) {
                m_hoveredId = m_padFocusId;
                m_hoveredLayout = focused->layout ? &focused->layout->get() : nullptr;
                return;
            }
        }
        m_hoveredId = hitAt(m_mouse, &m_hoveredLayout);
    }

    void Screen::setPadFocus(const std::string& name, const PadPointer& pointer) {
        m_padFocusId = 0;
        m_padPointer = pointer;
        if (!name.empty()) {
            try {
                m_padFocusId = findByName(name).id();
            } catch (const LayoutError&) {
                // no such element on this screen: nothing is highlighted
            }
        }
        refreshHover();
    }

    bool Screen::pressPadFocus() {
        const auto focused = locate(m_padFocusId);
        return focused && pressButton(focused->element.get());
    }

    bool Screen::pressButton(Element& element) {
        auto* button = dynamic_cast<ImageButton*>(&element);
        if (button == nullptr || !button->isEnabled()) {
            return false;
        }
        if (!button->clickSound().empty() && m_context.playSound) {
            m_context.playSound(button->clickSound());
        }
        button->click(m_context);
        return true;
    }

    std::optional<Screen::Located> Screen::padWindow() {
        // the layouts and their children are searched from the top down, as a click is
        const std::function<Element*(Element&)> topWindow = [&](Element& element) -> Element* {
            if (!element.isVisible()) {
                return nullptr;
            }
            if (element.padWindow() || element.padPage() || dynamic_cast<Window*>(&element) != nullptr) { // a pop-up menu and a page count as windows
                return &element;
            }
            for (auto child = element.children().rbegin(); child != element.children().rend(); ++child) {
                if (Element* inner = topWindow(**child)) {
                    return inner;
                }
            }
            return nullptr;
        };
        const auto& layouts = m_root->children();
        for (auto layout = layouts.rbegin(); layout != layouts.rend(); ++layout) {
            if (!kept(**layout) || (*layout)->isSlidAway()) {
                continue;
            }
            if (Element* window = topWindow(**layout)) {
                return locate(window->id());
            }
        }
        return std::nullopt;
    }

    int Screen::padWindowId() {
        const auto window = padWindow();
        return window ? window->element.get().id() : 0;
    }

    std::vector<PadSpot> Screen::padSpots() {
        std::vector<PadSpot> spots;
        const auto window = padWindow();
        if (!window) {
            return spots;
        }
        const LayoutRef layout = window->layout;
        // a place counts when the mouse would reach its element there: nothing lies over it
        const auto add = [&](const Element& element, core::Point inLayout, bool cell, bool up, bool down, bool checked) {
            const core::Point point = fromLayout(inLayout, layout);
            if (checked && hitAt(point) != element.id()) {
                return false;
            }
            spots.push_back({point, element.id(), cell, up, down});
            return true;
        };
        const std::function<void(Element&, core::Point)> collect = [&](Element& element, core::Point origin) {
            if (!element.isVisible() || element.padSkip() || dynamic_cast<ScrollBar*>(&element) != nullptr) {
                return;
            }
            const core::Rect& bounds = element.bounds();
            const core::Point at = origin + bounds.position();
            if (const core::Point cells = element.padCells(); cells.x > 0 || cells.y > 0) {
                // an element drawn by hand: a column of rows, or a grid of cells
                const int width = cells.x > 0 ? cells.x : bounds.width;
                const int height = cells.y > 0 ? cells.y : bounds.height;
                const int columns = std::max(1, bounds.width / std::max(1, width));
                int rows = std::max(1, (bounds.height + height / 2) / std::max(1, height)); // the nearest whole number: rows may have gaps between them
                bool up = false;
                bool down = false;
                if (!element.padScroll().empty()) {
                    try {
                        if (const auto* bar = dynamic_cast<const ScrollBar*>(&findByName(element.padScroll()))) {
                            rows = std::min(rows, std::max(0, (bar->count() - bar->position() + columns - 1) / columns));
                            up = bar->canScrollUp();
                            down = bar->canScrollDown();
                        }
                    } catch (const LayoutError&) {
                        // no such scroll bar: every row counts
                    }
                }
                for (int row = 0; row < rows; ++row) {
                    for (int column = 0; column < columns; ++column) {
                        add(element, {at.x + column * width + width / 2, at.y + row * height + height / 2}, true, up && row == 0, down && row == rows - 1, true);
                    }
                }
            } else if (const auto* list = dynamic_cast<const ListBox*>(&element)) {
                const int rows = std::min(list->visibleRows(), list->itemCount() - list->topIndex());
                for (int row = 0; row < rows; ++row) {
                    // a row is drawn from 2 px above its place in the list
                    add(
                        element,
                        {at.x + bounds.width / 2, at.y + row * list->rowHeight() + list->rowHeight() / 2 - 2},
                        true,
                        row == 0 && list->topIndex() > 0,
                        row == rows - 1 && list->topIndex() + list->visibleRows() < list->itemCount(),
                        true
                    );
                }
            } else if (const auto* grid = dynamic_cast<const IconGrid*>(&element)) {
                // an empty cell does not catch the mouse, but a thing can be put on it
                const core::Size cell = grid->cellSize();
                for (int index = grid->firstIndex(); index < grid->firstIndex() + grid->cellCount(); ++index) {
                    const core::Point corner = grid->cellPosition(index);
                    add(element, {at.x + corner.x + cell.width / 2, at.y + corner.y + cell.height / 2}, true, false, false, false);
                }
            } else if (const auto* blocks = dynamic_cast<const BlockList*>(&element)) {
                const int rows = std::min(blocks->visibleRows(), blocks->rowCount() - blocks->topIndex());
                for (int row = 0; row < rows; ++row) {
                    add(element, {at.x + bounds.width / 2, at.y + row * blocks->pitch() + blocks->rowHeight() / 2}, true, false, false, true);
                }
            } else if (const auto* area = dynamic_cast<const DrawingArea*>(&element); area != nullptr && !area->padPoints().empty()) {
                for (const core::Point& place : area->padPoints()) { // the places its view model names
                    if (add(element, at + place, false, false, false, true)) {
                        spots.back().loose = true;
                    }
                }
            } else if (dynamic_cast<const DrawingArea*>(&element) != nullptr) {
                // a place drawn by hand that takes the mouse (a slot a thing is put on, a picture with a hint, a heading) is one
                // stop; a strip thinner than a finger (a slider) and an area the size of a page are left to the pointer itself
                constexpr int kLeast = 20; // the least width and height, px
                constexpr int kMost = 300; // the greatest width and height, px
                constexpr int kBar = 60; // a longer one still counts when it is no higher or wider than this, px
                const int narrow = std::min(bounds.width, bounds.height);
                if (element.padSlider() || (narrow >= kLeast && (std::max(bounds.width, bounds.height) <= kMost || narrow <= kBar))) {
                    if (add(element, {at.x + bounds.width / 2, at.y + bounds.height / 2}, false, false, false, true)) {
                        spots.back().reach = bounds.width / 2;
                        spots.back().slider = element.padSlider();
                    }
                }
            } else if (element.isEnabled() && dynamic_cast<ImageButton*>(&element) != nullptr) {
                // the small close button in the window's top right corner is passed by: the pad's cancel button closes
                constexpr int kCorner = 70; // the corner's width and height, px
                constexpr int kSmall = 32; // the widest button that counts as the small one, px
                const core::Rect& form = window->element.get().bounds();
                const core::Point inWindow = at - window->screenPosition;
                const bool cornerClose = bounds.width <= kSmall && inWindow.x >= form.width - kCorner && inWindow.y + bounds.height <= kCorner;
                // a button may catch the mouse only where its picture is drawn: the middle first, then around it
                constexpr int kTries[5][2] = {{2, 2}, {1, 2}, {3, 2}, {2, 1}, {2, 3}}; // quarters of the width and of the height
                for (const auto& part : kTries) {
                    if (cornerClose) {
                        break;
                    }
                    if (add(element, {at.x + bounds.width * part[0] / 4, at.y + bounds.height * part[1] / 4}, false, false, false, true)) {
                        break;
                    }
                }
            } else if (const auto* field = dynamic_cast<const TextBox*>(&element)) {
                if (element.isEnabled() && !field->isReadOnly()) { // a text field that cannot be typed in is a label
                    add(element, {at.x + bounds.width / 2, at.y + bounds.height / 2}, false, false, false, false); // its parts catch the mouse for it
                }
                return;
            }
            const std::size_t before = spots.size();
            for (const auto& child : element.children()) {
                collect(*child, at);
            }
            if (!element.padScroll().empty() && spots.size() > before) {
                // a panel of controls that scrolls: going on from its highest and its lowest controls scrolls it first
                try {
                    if (const auto* bar = dynamic_cast<const ScrollBar*>(&findByName(element.padScroll()))) {
                        constexpr int kSameRow = 6; // controls this near in height stand in one row, px
                        int top = spots[before].point.y;
                        int bottom = top;
                        for (std::size_t i = before; i < spots.size(); ++i) {
                            top = std::min(top, spots[i].point.y);
                            bottom = std::max(bottom, spots[i].point.y);
                        }
                        for (std::size_t i = before; i < spots.size(); ++i) {
                            spots[i].scrollsUp = bar->canScrollUp() && spots[i].point.y - top <= kSameRow;
                            spots[i].scrollsDown = bar->canScrollDown() && bottom - spots[i].point.y <= kSameRow;
                            spots[i].scrollBarId = bar->id();
                        }
                    }
                } catch (const LayoutError&) {
                    // no such scroll bar: the panel does not scroll
                }
            }
        };
        for (const auto& child : window->element.get().children()) {
            collect(*child, window->screenPosition);
        }
        return spots;
    }

    std::optional<PadGridCell> Screen::padGridAt(core::Point point) {
        const auto located = locate(hitAt(point));
        const auto* grid = located ? dynamic_cast<const IconGrid*>(&located->element.get()) : nullptr;
        if (grid == nullptr) {
            return std::nullopt;
        }
        const int index = grid->cellAt(localMouse(*located, point));
        return PadGridCell{index != 0 && grid->holdsIcon(index), grid->isDraggable(), grid->usesDoubleClick()};
    }

    void Screen::padScroll(int scrollBarId, bool up) {
        const auto located = locate(scrollBarId);
        if (auto* bar = located ? dynamic_cast<ScrollBar*>(&located->element.get()) : nullptr) {
            constexpr int kLines = 3; // lines a press: about a row of controls
            bar->onMouseWheel(m_context, up ? kLines : -kLines);
        }
    }

    bool Screen::pressPadCancel() {
        const auto window = padWindow();
        if (!window) {
            return false;
        }
        if (const auto* form = dynamic_cast<const Window*>(&window->element.get()); form != nullptr && form->closesOnEscape()) {
            return false;
        }
        // the button that backs out is known by the end of its name; the first kind found wins
        static const char* const kEnds[] = {"cancel", "close", "quit", "exit", "leave", "back", "closesmall", "previous"};
        std::vector<ImageButton*> buttons;
        const std::function<void(Element&)> gather = [&](Element& element) {
            if (!element.isVisible()) {
                return;
            }
            if (auto* button = dynamic_cast<ImageButton*>(&element); button != nullptr && element.isEnabled() && !element.padSkip()) {
                buttons.push_back(button);
            }
            for (const auto& child : element.children()) {
                gather(*child);
            }
        };
        gather(window->element.get());
        for (const char* end : kEnds) {
            const std::string tail = end;
            for (ImageButton* button : buttons) {
                std::string name = button->name();
                std::transform(name.begin(), name.end(), name.begin(), [](unsigned char letter) { return static_cast<char>(std::tolower(letter)); });
                if (name.size() >= tail.size() && name.compare(name.size() - tail.size(), tail.size(), tail) == 0) {
                    pressButton(*button);
                    return true;
                }
            }
        }
        return false;
    }

    std::optional<core::Point> Screen::padStartPoint() {
        const std::vector<PadSpot> spots = padSpots();
        const auto window = padWindow();
        if (spots.empty() || !window) {
            return std::nullopt;
        }
        if (const auto* form = dynamic_cast<const Window*>(&window->element.get()); form != nullptr && !form->padStart().empty()) {
            try {
                const int id = findByName(form->padStart()).id();
                const auto named = std::find_if(spots.begin(), spots.end(), [id](const PadSpot& spot) { return spot.elementId == id; });
                if (named != spots.end()) {
                    return named->point;
                }
            } catch (const LayoutError&) {
                // no such element: the usual start
            }
        }
        const auto cell = std::find_if(spots.begin(), spots.end(), [](const PadSpot& spot) { return spot.cell; });
        if (cell != spots.end()) {
            return cell->point;
        }
        // the first control as one reads, the highest and of those the leftmost, passing by the tabs (the shoulder buttons
        // change them) and the small close button in the window's top right corner (the cancel button closes)
        constexpr int kCorner = 60; // the corner's width and height, px
        std::set<int> tabs;
        const core::Rect& bounds = window->element.get().bounds();
        const core::Point corner = fromLayout(window->screenPosition + core::Point{bounds.width - kCorner, kCorner}, window->layout);
        if (const auto* form = dynamic_cast<const Window*>(&window->element.get())) {
            for (const std::string& listed : form->padTabs()) {
                const std::string name = !listed.empty() && listed.front() == '*' ? listed.substr(1) : listed;
                try {
                    tabs.insert(findByName(name).id());
                } catch (const LayoutError&) {
                    // a tab this screen does not have
                }
            }
        }
        const auto passedBy = [&](const PadSpot& spot) { return tabs.count(spot.elementId) != 0 || (spot.point.x >= corner.x && spot.point.y <= corner.y); };
        const auto first = std::min_element(spots.begin(), spots.end(), [&](const PadSpot& a, const PadSpot& b) {
            if (passedBy(a) != passedBy(b)) {
                return passedBy(b);
            }
            return a.point.y != b.point.y ? a.point.y < b.point.y : a.point.x < b.point.x;
        });
        return first->point;
    }

    bool Screen::pressPadShoulder(bool next) {
        const auto window = padWindow();
        const auto* form = window ? dynamic_cast<const Window*>(&window->element.get()) : nullptr;
        if (form == nullptr) {
            return false;
        }
        const auto buttonOf = [&](const std::string& name) -> ImageButton* {
            try {
                Element& element = findByName(name);
                return element.isVisible() && element.isEnabled() ? dynamic_cast<ImageButton*>(&element) : nullptr;
            } catch (const LayoutError&) {
                return nullptr;
            }
        };
        if (!form->padTabs().empty()) {
            std::vector<ImageButton*> tabs;
            const ImageButton* own = nullptr; // the tab of this window itself, where each tab is a window (*Name)
            for (const std::string& listed : form->padTabs()) {
                const bool starred = !listed.empty() && listed.front() == '*';
                if (ImageButton* tab = buttonOf(starred ? listed.substr(1) : listed)) {
                    tabs.push_back(tab);
                    if (starred) {
                        own = tab;
                    }
                }
            }
            if (!tabs.empty()) {
                const auto open = std::find_if(tabs.begin(), tabs.end(), [own](const ImageButton* tab) { return own != nullptr ? tab == own : tab->isSelected(); });
                const int count = static_cast<int>(tabs.size());
                // from the last tab on to the first and back; with no tab open, the first or the last
                const int index = open == tabs.end() ? (next ? 0 : count - 1) : (static_cast<int>(open - tabs.begin()) + (next ? 1 : count - 1)) % count;
                pressButton(*tabs[static_cast<std::size_t>(index)]);
            }
            return true;
        }
        if (form->padShoulders().size() == 2) { // 2: the button before and the button after
            if (ImageButton* button = buttonOf(form->padShoulders()[next ? 1 : 0])) {
                pressButton(*button);
            }
            return true;
        }
        return false;
    }

    void Screen::focus(const std::string& name) { m_context.focusedId = findByName(name).id(); }

    void Screen::mouseMove(core::Point position, bool leftHeld) {
        const core::Point previous = m_mouse;
        m_mouse = position;
        refreshHover();
        if (m_context.drag && ((m_spreading && leftHeld) || m_rightHeld)) {
            // a quick sweep jumps many px between two mouse events: every cell on the way counts, not only where it lands
            constexpr int kStepPx = 4; // px between the points tried along the way (under the smallest cell gap)
            const int dx = position.x - previous.x;
            const int dy = position.y - previous.y;
            const int steps = std::max(1, std::max(std::abs(dx), std::abs(dy)) / kStepPx);
            for (int step = 1; step <= steps && m_context.drag; ++step) {
                const core::Point at{previous.x + dx * step / steps, previous.y + dy * step / steps};
                const auto cell = spreadCellAt(at);
                if (!cell) {
                    continue;
                }
                if (m_spreading && leftHeld) {
                    // the left button held while carrying: each new cell passed takes a share, no more cells than things carried
                    if (std::find(m_spreadCells.begin(), m_spreadCells.end(), *cell) == m_spreadCells.end() &&
                        static_cast<int>(m_spreadCells.size()) < carried()) {
                        m_spreadCells.push_back(*cell);
                    }
                } else if (std::find(m_rightCells.begin(), m_rightCells.end(), *cell) == m_rightCells.end()) {
                    putOneDown(at); // the right button held while carrying: each new cell passed gets one
                }
            }
        }
        if (leftHeld && m_capturedId != 0) {
            if (auto captured = locate(m_capturedId)) {
                captured->element.get().onMouseDrag(m_context, localMouse(*captured, position));
            }
        }
    }

    void Screen::startStickyDrag(const std::string& gridName, int index, int count) {
        try {
            if (const auto* grid = dynamic_cast<const IconGrid*>(&findByName(gridName))) {
                if (auto drag = grid->dragOf(index)) {
                    drag->sticky = true;
                    drag->count = count;
                    m_context.drag = *drag;
                }
            }
        } catch (const LayoutError&) {
            // no such grid: nothing to drag
        }
    }

    const IconGrid* Screen::modernGrid(int id) {
        const auto found = locate(id);
        const auto* grid = found ? dynamic_cast<const IconGrid*>(&found->element.get()) : nullptr;
        return grid && std::find(m_modernDragGrids.begin(), m_modernDragGrids.end(), grid->name()) != m_modernDragGrids.end() ? grid : nullptr;
    }

    int Screen::actualCount(int gridId, int index) {
        const IconGrid* grid = modernGrid(gridId);
        return grid ? grid->countAt(index) : 0;
    }

    int Screen::unanswered(const PendingMove& move) {
        return std::max(0, move.before + move.amount - actualCount(move.gridId, move.index));
    }

    int Screen::pendingInto(int gridId, int index) {
        int sum = 0;
        for (const PendingMove& move : m_pending) {
            if (move.gridId == gridId && move.index == index) {
                sum += unanswered(move);
            }
        }
        return sum;
    }

    int Screen::pendingOutOf(int gridId, int index) {
        int sum = 0;
        for (const PendingMove& move : m_pending) {
            if (move.fromGridId == gridId && move.fromIndex == index) {
                sum += unanswered(move);
            }
        }
        return sum;
    }

    void Screen::notePlaced(const DragItem& drag, int gridId, int index) {
        const int placed = m_placed;
        m_placed = 0;
        if (placed <= 0 || modernGrid(gridId) == nullptr || (gridId == drag.sourceId && index == drag.sourceIndex)) {
            return;
        }
        // shown at once; the server's answer takes its place when it comes
        PendingMove move;
        move.gridId = gridId;
        move.index = index;
        move.picture = drag.picture;
        move.source = drag.source;
        move.before = actualCount(gridId, index) + pendingInto(gridId, index);
        move.amount = placed;
        move.fromGridId = drag.sourceId;
        move.fromIndex = drag.sourceIndex;
        m_pending.push_back(move);
    }

    int Screen::carried() {
        if (!m_context.drag || modernGrid(m_context.drag->sourceId) == nullptr) {
            return 0;
        }
        const int stack = actualCount(m_context.drag->sourceId, m_context.drag->sourceIndex) - pendingOutOf(m_context.drag->sourceId, m_context.drag->sourceIndex);
        return std::max(0, m_context.drag->count > 0 ? std::min(m_context.drag->count, stack) : stack);
    }

    std::optional<std::pair<int, int>> Screen::spreadCellAt(core::Point position) {
        if (!m_context.drag) {
            return std::nullopt;
        }
        const int hit = hitAt(position);
        const auto target = locate(hit);
        const IconGrid* grid = target ? modernGrid(hit) : nullptr;
        if (grid == nullptr) {
            return std::nullopt;
        }
        const int index = grid->dropIndexAt(localMouse(*target, position));
        if (index <= 0) {
            return std::nullopt;
        }
        const bool own = grid->id() == m_context.drag->sourceId && index == m_context.drag->sourceIndex;
        const bool pendingThere = pendingInto(grid->id(), index) > 0;
        if (!own && (grid->holdsIcon(index) || pendingThere)) {
            const bool same = grid->holdsIcon(index) ? grid->holdsSame(index, *m_context.drag) : true;
            const int shown = grid->countAt(index) + pendingInto(grid->id(), index);
            if (!same || (m_modernStackLimit > 0 && shown >= m_modernStackLimit)) {
                return std::nullopt; // another thing, or a full stack
            }
        }
        return std::make_pair(grid->id(), index);
    }

    void Screen::putOneDown(core::Point position) {
        const auto cell = spreadCellAt(position);
        if (!cell || carried() <= 0) {
            return;
        }
        m_rightCells.push_back(*cell);
        const DragItem drag = *m_context.drag;
        const int left = carried() - 1;
        if (cell->first != drag.sourceId || cell->second != drag.sourceIndex) {
            const auto source = locate(drag.sourceId);
            const auto target = locate(cell->first);
            if (!source || !target) {
                return;
            }
            m_placed = 0;
            m_context.dropPartial = true;
            m_context.dropCarried = drag.count;
            source->element.get().onDropped(m_context, drag.sourceIndex, std::cref(target->element.get()), cell->second, position, drag.grab);
            m_context.dropPartial = false;
            m_context.dropCarried = 0;
            if (m_placed <= 0) {
                return; // refused: nothing left the mouse
            }
            notePlaced(drag, cell->first, cell->second);
        } // one put back in its own cell only leaves it there
        if (!m_context.drag) {
            return;
        }
        if (left <= 0) {
            m_context.drag.reset(); // the last one carried is down
        } else {
            m_context.drag->count = left;
        }
    }

    void Screen::finishSpread(core::Point position) {
        m_spreading = false;
        const std::vector<std::pair<int, int>> cells = std::move(m_spreadCells);
        m_spreadCells.clear();
        if (cells.size() <= 1) {
            dropDragged(position);
            return;
        }
        const DragItem drag = *m_context.drag;
        const int total = carried();
        const int cellCount = static_cast<int>(cells.size());
        const int share = total / cellCount;
        int keep = total - share * cellCount; // what does not share out evenly stays carried
        m_context.drag.reset();
        const auto source = locate(drag.sourceId);
        if (!source || share <= 0) {
            return;
        }
        for (const auto& [gridId, index] : cells) {
            if (gridId == drag.sourceId && index == drag.sourceIndex) {
                continue; // the source cell's share stays where it is
            }
            const auto target = locate(gridId);
            if (!target) {
                keep += share;
                continue;
            }
            m_placed = 0;
            m_context.dropCarried = share;
            source->element.get().onDropped(m_context, drag.sourceIndex, std::cref(target->element.get()), index, position, drag.grab);
            const int placed = std::min(m_placed, share);
            keep += share - placed; // what a cell did not take stays carried
            m_placed = placed;
            notePlaced(drag, gridId, index);
        }
        m_context.dropCarried = 0;
        m_keepCarry.reset();
        if (keep > 0) {
            DragItem kept = drag;
            kept.sticky = true;
            kept.fromClick = false;
            kept.count = keep;
            m_context.drag = kept;
        }
    }

    void Screen::buildPreviews(std::uint64_t nowMs) {
        m_previews.clear();
        // what the server has answered, or never will (refused, 3 s on), shows as it is
        constexpr std::uint64_t kPendingMs = 3000; // ms a move shown before the server's answer is kept
        for (PendingMove& move : m_pending) {
            if (move.atMs == 0) {
                move.atMs = nowMs;
            }
        }
        m_pending.erase(
            std::remove_if(
                m_pending.begin(), m_pending.end(),
                [&](const PendingMove& move) { return modernGrid(move.gridId) == nullptr || unanswered(move) == 0 || nowMs - move.atMs > kPendingMs; }
            ),
            m_pending.end()
        );
        std::map<std::pair<int, int>, int> change; // what each cell shows more (or less) than it holds
        std::map<std::pair<int, int>, std::pair<gfx::PictureId, core::Rect>> looks; // the picture of what goes into an empty cell
        for (const PendingMove& move : m_pending) {
            const int left = unanswered(move);
            change[{move.gridId, move.index}] += left;
            change[{move.fromGridId, move.fromIndex}] -= left;
            looks[{move.gridId, move.index}] = {move.picture, move.source};
        }
        if (m_context.drag && modernGrid(m_context.drag->sourceId) != nullptr) {
            const DragItem& drag = *m_context.drag;
            const int total = carried();
            change[{drag.sourceId, drag.sourceIndex}] -= total; // what is carried has left its cell
            if (m_spreading && m_spreadCells.size() >= 2) {
                // a left button spread shows each cell's share while the button is held
                const int share = total / static_cast<int>(m_spreadCells.size());
                for (const auto& cell : m_spreadCells) {
                    change[cell] += share;
                    looks[cell] = {drag.picture, drag.source};
                }
            }
        }
        for (const auto& [cell, more] : change) {
            if (more == 0) {
                continue;
            }
            CellPreview preview;
            if (const auto look = looks.find(cell); look != looks.end()) {
                preview.picture = look->second.first;
                preview.source = look->second.second;
            }
            preview.count = actualCount(cell.first, cell.second) + more;
            m_previews[cell] = preview;
        }
    }

    int Screen::cursorCount() {
        const int total = carried();
        if (m_spreading && m_spreadCells.size() >= 2) {
            return total - total / static_cast<int>(m_spreadCells.size()) * static_cast<int>(m_spreadCells.size());
        }
        return total;
    }

    void Screen::refreshDrag() {
        if (!m_context.drag) {
            return;
        }
        const auto found = locate(m_context.drag->sourceId);
        const auto* grid = found ? dynamic_cast<const IconGrid*>(&found->element.get()) : nullptr;
        if (grid == nullptr || modernGrid(grid->id()) == nullptr) {
            return;
        }
        if (const auto fresh = grid->dragOf(m_context.drag->sourceIndex)) {
            m_context.drag->picture = fresh->picture;
            m_context.drag->source = fresh->source;
        } else {
            m_context.drag.reset(); // the carried thing's cell is empty now
            m_spreading = false;
            m_rightHeld = false;
        }
    }

    void Screen::mouseDown(core::Point position, MouseButton button, int clicks, bool ctrl, bool shift) {
        m_mouse = position;
        m_context.ctrlHeld = ctrl;
        m_context.shiftHeld = shift;
        refreshHover();
        if (m_pressHook && m_hoveredId != 0) {
            m_pressHook(m_hoveredId);
        }
        if (!m_context.drag && (button == MouseButton::Left || button == MouseButton::Right)) {
            // a grid picking cells (qol sort locks): a left press picks the cell and nothing else happens; a right press does nothing
            const auto target = locate(m_hoveredId);
            const auto* grid = target ? dynamic_cast<const IconGrid*>(&target->element.get()) : nullptr;
            if (grid != nullptr && grid->isPicking()) {
                if (button == MouseButton::Left) {
                    if (m_context.playSound && !grid->clickSound().empty()) {
                        m_context.playSound(grid->clickSound());
                    }
                    grid->pick(m_context, localMouse(*target, position));
                    m_releaseConsumed = true;
                } else {
                    m_rightUpConsumed = true;
                }
                return;
            }
        }
        if (m_context.drag) {
            const bool modern = modernGrid(m_context.drag->sourceId) != nullptr;
            if (button == MouseButton::Right && modern) {
                // one of what is carried is put down where the mouse is; the rest stays carried, and while the button is held each
                // new cell passed gets one too
                m_rightCells.clear();
                m_rightHeld = true;
                putOneDown(position);
                m_rightUpConsumed = true;
                return;
            }
            if (button == MouseButton::Left && m_context.drag->sticky) {
                const auto target = locate(m_hoveredId);
                const bool onOwnCell = target && m_hoveredId == m_context.drag->sourceId &&
                                       target->element.get().dropIndexAt(localMouse(*target, position)) == m_context.drag->sourceIndex;
                if (modern && clicks >= 2 && m_context.drag->fromClick && onOwnCell) {
                    // the second press of a double click: what its first click took up goes back, and the double click goes on
                    m_context.drag.reset();
                } else if (modern) {
                    // what is carried goes down when the button comes up: where it was pressed, or shared out over the cells passed
                    m_spreading = true;
                    m_spreadCells.clear();
                    if (const auto cell = spreadCellAt(position)) {
                        m_spreadCells.push_back(*cell);
                    }
                    m_releaseConsumed = true;
                    return;
                } else {
                    dropDragged(position);
                    m_releaseConsumed = true;
                    return;
                }
            }
        }
        if (button == MouseButton::Left && shift && !m_context.drag) {
            const auto target = locate(m_hoveredId);
            const auto* grid = target ? dynamic_cast<const IconGrid*>(&target->element.get()) : nullptr;
            if (grid != nullptr && modernGrid(m_hoveredId) == nullptr &&
                std::find(m_quickMoveGrids.begin(), m_quickMoveGrids.end(), grid->name()) == m_quickMoveGrids.end()) {
                grid = nullptr; // a grid that does not quick-move
            }
            if (grid != nullptr) {
                if (grid->quickMove(m_context, grid->cellAt(localMouse(*target, position)))) {
                    m_releaseConsumed = true; // a Shift click moves the thing at once to where it goes
                    return;
                }
            }
        }
        m_context.focusedId = m_hoveredId;
        auto target = locate(m_hoveredId);
        if (!target) {
            return;
        }
        Element& element = target->element.get();
        if (button == MouseButton::Left) {
            m_capturedId = m_hoveredId;
        }
        if (element.isEnabled() && !element.clickSound().empty() && m_context.playSound) {
            m_context.playSound(element.clickSound());
        }
        if (m_root->raisesOnPressAt(m_hoveredId)) {
            if (m_root->raisesAbovePopupsAt(m_hoveredId)) {
                m_root->raiseChildHoldingAbovePopups(m_hoveredId);
            } else {
                m_root->raiseChildHolding(m_hoveredId);
            }
        }
        const core::Point local = localMouse(*target, position);
        // 2: a double click (second press of a pair)
        if (clicks >= 2) {
            clicks = clicks % 2 == 0 ? 2 : 1;
        }
        if (button == MouseButton::Left) {
            m_pressWasDouble = clicks >= 2;
        }
        if (button == MouseButton::Left && clicks >= 2) {
            element.onDoubleClick(m_context, local);
        }
        if (button == MouseButton::Left && element.takesText()) {
            m_textClickId = element.id();
        }
        element.onMouseDown(m_context, button, local);
    }

    TextBox* Screen::textBoxOf(int id) {
        const auto found = locate(id);
        return found ? dynamic_cast<TextBox*>(&found->element.get()) : nullptr;
    }

    void Screen::mouseUp(core::Point position, MouseButton button, bool ctrl) {
        m_mouse = position;
        m_context.ctrlHeld = ctrl;
        refreshHover();
        if (button == MouseButton::Right) {
            m_rightHeld = false;
            m_rightCells.clear();
        }
        if (button == MouseButton::Right && m_rightUpConsumed) {
            m_rightUpConsumed = false;
            return;
        }
        if (button == MouseButton::Left && m_context.drag) {
            const auto target = locate(m_hoveredId);
            const bool onOwnCell = target && m_hoveredId == m_context.drag->sourceId &&
                                   target->element.get().dropIndexAt(localMouse(*target, position)) == m_context.drag->sourceIndex;
            if (m_spreading) {
                finishSpread(position);
            } else if (!m_context.drag->sticky && onOwnCell && modernGrid(m_context.drag->sourceId) != nullptr) {
                // a click on a thing the modern way takes it up: it follows the mouse until the next click puts it down
                m_context.drag->sticky = true;
                m_context.drag->fromClick = true;
            } else {
                dropDragged(position);
            }
        } else if (button == MouseButton::Left) {
            m_spreading = false;
            m_spreadCells.clear();
        }
        const int captured = button == MouseButton::Left ? m_capturedId : 0;
        if (button == MouseButton::Left) {
            m_capturedId = 0;
        }
        if (m_releaseConsumed) {
            m_releaseConsumed = false;
            return;
        }
        if (auto target = locate(m_hoveredId)) {
            Element& element = target->element.get();
            if (button == MouseButton::Left && element.isEnabled() && !element.releaseSound().empty() && m_context.playSound) {
                m_context.playSound(element.releaseSound());
            }
            m_context.releaseOfDoubleClick = button == MouseButton::Left && m_pressWasDouble;
            element.onMouseUp(m_context, button, localMouse(*target, position));
            m_context.releaseOfDoubleClick = false;
        }
        if (captured != 0 && captured != m_hoveredId) {
            if (auto previous = locate(captured)) {
                previous->element.get().onCaptureReleased(m_context);
            }
        }
    }

    void Screen::dropDragged(core::Point position) {
        const DragItem drag = *m_context.drag;
        m_context.drag.reset();
        const auto source = locate(drag.sourceId);
        if (!source) {
            return;
        }
        const auto target = locate(m_hoveredId);
        std::optional<std::reference_wrapper<const Element>> targetElement;
        if (target) {
            targetElement = std::cref(target->element.get());
        }
        const int targetIndex = target ? targetElement->get().dropIndexAt(localMouse(*target, position)) : -1;
        m_keepCarry.reset();
        m_placed = 0;
        m_context.dropCarried = drag.count;
        source->element.get().onDropped(m_context, drag.sourceIndex, targetElement, targetIndex, position, drag.grab);
        m_context.dropCarried = 0;
        if (target) {
            notePlaced(drag, m_hoveredId, targetIndex);
        }
        m_placed = 0;
        if (m_keepCarry && !m_context.drag) {
            // what did not go down stays on the mouse, the modern way
            DragItem kept = drag;
            kept.sticky = true;
            kept.fromClick = false;
            kept.count = *m_keepCarry;
            m_context.drag = kept;
        }
        m_keepCarry.reset();
    }

    void Screen::mouseWheel(int steps) {
        if (auto target = locate(m_context.focusedId)) {
            target->element.get().onMouseWheel(m_context, steps);
        }
    }

    void Screen::keyDown(const KeyEvent& key) {
        m_heldKeys.insert(key.key);
        if (key.key == Key::Escape && m_context.drag && m_context.drag->sticky) {
            m_context.drag.reset(); // what is carried without a button goes back
            m_spreading = false;
            m_rightHeld = false;
            return;
        }
        if (m_keyPreview && m_keyPreview(key)) {
            return;
        }
        if (auto target = locate(m_context.focusedId)) {
            target->element.get().onKeyDown(m_context, key);
        }
    }

    std::optional<std::reference_wrapper<Window>> Screen::topEscapeWindow(Element& parent) {
        for (auto child = parent.children().rbegin(); child != parent.children().rend(); ++child) {
            Element& element = **child;
            if (!element.isVisible()) {
                continue;
            }
            if (auto* window = dynamic_cast<Window*>(&element); window != nullptr && window->closesOnEscape()) {
                return *window;
            }
            if (const auto inner = topEscapeWindow(element)) {
                return inner;
            }
        }
        return std::nullopt;
    }

    bool Screen::closeTopOnEscape() {
        if (const auto window = topEscapeWindow(*m_root)) {
            window->get().raiseEscape(m_context);
            return true;
        }
        return false;
    }

    void Screen::keyUp(const KeyEvent& key) {
        m_heldKeys.erase(key.key);
        if (auto target = locate(m_context.focusedId)) {
            target->element.get().onKeyUp(m_context, key);
        }
    }

    void Screen::textInput(const std::string& text) {
        if (m_textFallback) {
            const auto holder = locate(m_context.focusedId);
            const bool textField =
                holder && (dynamic_cast<const TextBox*>(&holder->element.get()) != nullptr || dynamic_cast<const Memo*>(&holder->element.get()) != nullptr);
            if (!textField) {
                m_textFallback();
            }
        }
        if (auto target = locate(m_context.focusedId)) {
            target->element.get().onTextInput(m_context, text);
        }
    }

    void Screen::collectListBoxes(Element& element) {
        if (auto* list = dynamic_cast<ListBox*>(&element)) {
            m_listBoxes.push_back(list);
        }
        for (const auto& child : element.children()) {
            collectListBoxes(*child);
        }
    }

    void Screen::syncScrollBars() {
        if (m_listBoxesAt != Element::treeChanges()) {
            m_listBoxes.clear(); // in tree order, as the scroll bars were synced before
            collectListBoxes(*m_root);
            m_listBoxesAt = Element::treeChanges();
        }
        for (ListBox* list : m_listBoxes) {
            if (!list->scrollBarName().empty()) {
                list->syncScrollBar(get<ScrollBar>(list->scrollBarName()));
            }
        }
    }

    void Screen::update(std::uint64_t nowMs) {
        m_context.nowMs = nowMs;
        m_root->update(m_context);
        raiseOpenedWindows();
        syncScrollBars();
        refreshHover();
    }

    std::vector<core::Rect> Screen::shownWindows() const {
        std::vector<core::Rect> windows;
        for (const auto& layout : m_root->children()) {
            if (kept(*layout)) {
                collectWindows(*layout, {0, 0}, windows);
            }
        }
        return windows;
    }

    bool Screen::windowOver(const core::Rect& area, const std::function<bool(const Element& layout)>& popup) const {
        // the element's own rectangle, in the screen's coordinates
        const auto onScreen = [this](const Element& element, core::Point position, LayoutRef layout) {
            const core::Point topLeft = fromLayout(position, layout);
            const core::Point bottomRight = fromLayout(position + core::Point{element.bounds().width, element.bounds().height}, layout);
            return core::Rect::fromEdges(topLeft.x, topLeft.y, bottomRight.x, bottomRight.y);
        };
        // a popup's element takes the mouse at a point of the area (each pixel of the overlap, until one does)
        const auto takesMouseIn = [&](const Element& element, core::Point position, LayoutRef layout) {
            const core::Rect overlap = onScreen(element, position, layout).intersected(area);
            for (int y = overlap.y; y < overlap.bottom(); ++y) {
                for (int x = overlap.x; x < overlap.right(); ++x) {
                    if (element.hitTest(toLayout({x, y}, layout) - position, m_pictures)) {
                        return true;
                    }
                }
            }
            return false;
        };
        // an element, its screen origin, its layout and whether that layout is a popup's
        using Search = std::function<bool(const Element&, core::Point, LayoutRef, bool)>;
        Search search = [&](const Element& element, core::Point origin, LayoutRef layout, bool inPopup) {
            if (!element.isVisible()) {
                return false;
            }
            const core::Point position = origin + element.bounds().position();
            if (element.padWindow() || dynamic_cast<const Window*>(&element) != nullptr) {
                return !onScreen(element, position, layout).intersected(area).isEmpty();
            }
            if (inPopup && takesMouseIn(element, position, layout)) {
                return true;
            }
            return std::any_of(element.children().begin(), element.children().end(), [&](const auto& child) {
                return search(*child, position, layout, inPopup);
            });
        };
        const core::Point rootOrigin = m_root->bounds().position();
        return std::any_of(m_root->children().begin(), m_root->children().end(), [&](const auto& layout) {
            return kept(*layout) && !layout->isSlidAway() && search(*layout, rootOrigin, std::cref(*layout), popup && popup(*layout));
        });
    }

    std::vector<const Element*> Screen::layoutsFrom(const std::function<bool(const Element& layout)>& pick) const {
        const auto& layouts = m_root->children();
        const auto lowest = std::find_if(layouts.begin(), layouts.end(), [&pick](const auto& layout) {
            return layout->isVisible() && !layout->isSlidAway() && pick(*layout);
        });
        std::vector<const Element*> picked;
        for (auto layout = lowest; layout != layouts.end(); ++layout) {
            picked.push_back(layout->get());
        }
        return picked;
    }

    std::optional<core::Rect> Screen::screenRectOf(const std::string& name) {
        Element* element = nullptr;
        try {
            element = &findByName(name);
        } catch (const LayoutError&) {
            return std::nullopt;
        }
        const auto located = locate(element->id());
        if (!located) {
            return std::nullopt;
        }
        const core::Rect& bounds = element->bounds();
        const core::Point topLeft = fromLayout(located->screenPosition, located->layout);
        const core::Point bottomRight = fromLayout(located->screenPosition + core::Point{bounds.width, bounds.height}, located->layout);
        return core::Rect{topLeft.x, topLeft.y, bottomRight.x - topLeft.x, bottomRight.y - topLeft.y};
    }

    std::optional<core::Point> Screen::screenPositionOf(const Element& target) const {
        std::function<std::optional<core::Point>(const Element&, core::Point, LayoutRef)> search =
            [&](const Element& element, core::Point origin, LayoutRef layout) -> std::optional<core::Point> {
            if (!element.isVisible()) {
                return std::nullopt;
            }
            const core::Point position = origin + element.bounds().position();
            if (&element == &target) {
                return fromLayout(position, layout);
            }
            for (const auto& child : element.children()) {
                if (auto found = search(*child, position, layout ? layout : LayoutRef(std::cref(*child)))) {
                    return found;
                }
            }
            return std::nullopt;
        };
        return search(*m_root, {0, 0}, std::nullopt);
    }

    bool Screen::placeAt(Element& element, core::Point position) {
        // searched whether shown or not (locate finds shown elements only): a window opened in a hidden layer goes to its place too
        std::function<bool(const Element&, core::Point, LayoutRef)> search = [&](const Element& parent, core::Point origin, LayoutRef layout) {
            for (const auto& child : parent.children()) {
                const LayoutRef childLayout = layout ? layout : LayoutRef(std::cref(*child));
                if (child.get() == &element) {
                    element.setPosition(toLayout(position, childLayout) - origin);
                    return true;
                }
                if (search(*child, origin + child->bounds().position(), childLayout)) {
                    return true;
                }
            }
            return false;
        };
        return search(*m_root, m_root->bounds().position(), std::nullopt);
    }

    void Screen::raiseOpenedWindows() {
        // a window that opens lies over those open before it, as one that is clicked does; it stays under the layouts kept
        // on top (the talk box, the hints) unless a window was raised over those already
        std::set<int> open;
        const std::function<void(const Element&)> gather = [&](const Element& element) {
            if (!element.isVisible()) {
                return;
            }
            if (element.padWindow() || dynamic_cast<const Window*>(&element) != nullptr) {
                open.insert(element.id());
                return;
            }
            for (const auto& child : element.children()) {
                gather(*child);
            }
        };
        for (const auto& layout : m_root->children()) {
            if (kept(*layout) && !layout->isTopmost()) {
                gather(*layout);
            }
        }
        if (m_openWindowsKnown) {
            for (const int id : open) {
                if (m_openWindows.count(id) != 0) {
                    continue;
                }
                m_root->raiseChildHolding(id);
                const auto& layouts = m_root->children();
                const auto holder = std::find_if(layouts.begin(), layouts.end(), [id](const auto& layout) { return layout->contains(id); });
                const bool windowAbove = holder != layouts.end() && std::any_of(holder + 1, layouts.end(), [&](const auto& layout) {
                    return !layout->isTopmost() && std::any_of(open.begin(), open.end(), [&](int other) { return layout->contains(other); });
                });
                if (windowAbove) {
                    m_root->raiseChildHoldingAbovePopups(id);
                }
            }
        }
        m_openWindows = std::move(open);
        m_openWindowsKnown = true;
    }

    core::Point Screen::layoutSlide(const std::string& layoutName) const {
        for (const auto& layout : m_root->children()) {
            if (layout->name() == layoutName) {
                return layout->slide();
            }
        }
        return {};
    }

    bool Screen::layoutAway(const std::string& layoutName) const {
        for (const auto& layout : m_root->children()) {
            if (layout->name() == layoutName) {
                return layout->isSlidAway();
            }
        }
        return false;
    }

    void Screen::slideLayout(const std::string& layoutName, core::Point slide, bool away) {
        for (const auto& layout : m_root->children()) {
            if (layout->name() == layoutName) {
                layout->setSlide(slide, away);
            }
        }
    }

    std::optional<core::Rect> Screen::padPointerPlace() {
        if (m_padPointer.picture == nullptr || !m_padPointer.picture->isValid()) {
            return std::nullopt;
        }
        const auto focused = locate(m_padFocusId);
        if (!focused) {
            return std::nullopt;
        }
        const core::Rect& bounds = focused->element.get().bounds();
        const core::Point slide = focused->layout ? focused->layout->get().slide() : core::Point{};
        const core::Point topLeft = fromLayout(focused->screenPosition, focused->layout) + slide;
        const core::Point bottomRight = fromLayout(focused->screenPosition + core::Point{bounds.width, bounds.height}, focused->layout) + slide;
        // the fingertip lies a little way inside the element's upper or lower edge
        const float scale = scaleOf(focused->layout);
        constexpr int kInside = 6; // how far inside the edge, px
        const int inside = static_cast<int>(std::lround(kInside * scale));
        const core::Point aim{(topLeft.x + bottomRight.x) / 2, m_padPointer.below ? bottomRight.y - inside : topLeft.y + inside};
        return core::Rect{
            aim.x - static_cast<int>(std::lround(m_padPointer.tip.x * scale)),
            aim.y - static_cast<int>(std::lround(m_padPointer.tip.y * scale)),
            static_cast<int>(std::lround(m_padPointer.picture->width() * scale)),
            static_cast<int>(std::lround(m_padPointer.picture->height() * scale))
        };
    }

    void Screen::draw(gfx::Canvas& canvas, std::uint64_t nowMs) {
        const bool hoverHidden = m_hoveredLayout != nullptr && !kept(*m_hoveredLayout);
        if (hoverHidden && m_refindHover) {
            // update found the hovered element without this filter (party member screens get it only to draw), in a
            // layout the filter hides: find it again among the kept layouts, so hidden layouts give no highlight or
            // hint. A hit in a kept layout is already what the filtered search would find: every layout searched
            // before it gave no hit.
            refreshHover();
        }
        refreshDrag();
        // a screen drawn in passes: the hovered element is another pass's, and nothing under it is highlighted in this one
        const int hovered = hoverHidden && !m_refindHover ? 0 : m_hoveredId;
        DrawContext context{canvas, hovered, m_context.focusedId, m_mouse, nowMs};
        buildPreviews(nowMs);
        const int carriedNow = cursorCount();
        if (!m_previews.empty()) {
            context.previews = &m_previews;
        }
        const auto drawLayouts = [&](bool overlay) {
            for (const auto& layout : m_root->children()) {
                if (!kept(*layout) || layout->isSlidAway()) {
                    continue;
                }
                const float scale = scaleOf(std::cref(*layout));
                if (scale != 1.0f) {
                    canvas.beginScale(layout->anchorPoint(), scale);
                    context.mouse = toLayout(m_mouse, std::cref(*layout));
                }
                // the slide is in screen pixels; inside the HUD scale it is that much less
                const core::Point slide = layout->slide();
                const core::Point origin{static_cast<int>(std::lround(slide.x / scale)), static_cast<int>(std::lround(slide.y / scale))};
                if (overlay) {
                    layout->drawOverlayTree(context, origin);
                } else {
                    layout->drawTree(context, origin);
                }
                if (scale != 1.0f) {
                    canvas.endScale();
                    context.mouse = m_mouse;
                }
            }
        };
        drawLayouts(false);
        drawLayouts(true);
        if (const auto focused = locate(m_padFocusId)) {
            if (const auto hand = padPointerPlace()) {
                canvas.drawTextureScaled(*m_padPointer.picture, *hand); // the game pad's highlight: a hand pointing at the element
            } else {
                // without a hand: a light frame with a dark one around it, to show on any ground
                const core::Rect& bounds = focused->element.get().bounds();
                const core::Point slide = focused->layout ? focused->layout->get().slide() : core::Point{};
                const core::Point topLeft = fromLayout(focused->screenPosition, focused->layout) + slide;
                const core::Point bottomRight = fromLayout(focused->screenPosition + core::Point{bounds.width, bounds.height}, focused->layout) + slide;
                constexpr int kGap = 2; // px between the element and its frame
                const core::Rect frame{topLeft.x - kGap, topLeft.y - kGap, bottomRight.x - topLeft.x + 2 * kGap, bottomRight.y - topLeft.y + 2 * kGap};
                canvas.drawRectOutline({frame.x - 1, frame.y - 1, frame.width + 2, frame.height + 2}, {0x20, 0x18, 0x00, 255}, 1); // dark brown
                canvas.drawRectOutline(frame, {0xFF, 0xE0, 0x40, 255}, 2); // bright yellow, 2 px
            }
        }
        if (m_context.drag) {
            const core::Point at = m_mouse - m_context.drag->grab;
            const bool sharedOut = modernGrid(m_context.drag->sourceId) != nullptr && carriedNow <= 0; // a spread leaves nothing on the mouse
            if (!sharedOut) {
                canvas.drawPicture(m_context.drag->picture, at, m_context.drag->source);
            }
            if (carriedNow > 1) {
                // how many are carried, at the place a cell shows its count
                constexpr core::Point kCountOffset{2, 20}; // px from the picture's top left, as IconGrid draws a cell's count
                constexpr int kCountWidth = 32; // px width of the count text area
                canvas.drawText(at + kCountOffset, std::to_string(carriedNow), {{255, 255, 255, 255}, gfx::TextEffect::Shadow, std::nullopt}, kCountWidth);
            }
            for (const auto& [gridId, index] : m_spreadCells) {
                // the cells a held left button shares what is carried out over
                if (m_spreadCells.size() < 2) {
                    break;
                }
                const auto grid = locate(gridId);
                const auto* cells = grid ? dynamic_cast<const IconGrid*>(&grid->element.get()) : nullptr;
                if (cells != nullptr) {
                    const core::Point topLeft = fromLayout(grid->screenPosition + cells->cellPosition(index), grid->layout);
                    const core::Point bottomRight =
                        fromLayout(grid->screenPosition + cells->cellPosition(index) + core::Point{cells->cellSize().width, cells->cellSize().height}, grid->layout);
                    canvas.drawRectOutline({topLeft.x, topLeft.y, bottomRight.x - topLeft.x, bottomRight.y - topLeft.y}, {0xFF, 0xFF, 0xFF, 255}, 1);
                }
            }
            return;
        }
        drawHint(canvas);
    }

    void Screen::drawHint(gfx::Canvas& canvas) {
        const auto target = locate(m_hoveredId);
        if (!target || (target->layout && !kept(target->layout->get()))) {
            return; // nothing hovered, or in a layout the filter hides
        }
        const core::Point local = localMouse(*target, m_mouse);
        if (const auto hand = m_hoveredId == m_padFocusId ? padPointerPlace() : std::nullopt) {
            // the hint stands past the game pad's hand, not over it: its box is drawn 26 px above the point given, one line 23 px high
            constexpr int kGap = 2; // px between the hand and the box
            const core::Point origin = fromLayout(target->screenPosition + target->element.get().hintOrigin(local), target->layout);
            const int y = m_padPointer.below ? hand->y + hand->height + kGap + 26 : hand->y - kGap - 23 + 26;
            drawHintBox(canvas, {origin.x, y}, target->element.get().hintAt(local));
            return;
        }
        drawHintBox(canvas, fromLayout(target->screenPosition + target->element.get().hintOrigin(local), target->layout), target->element.get().hintAt(local));
    }

    CursorKind Screen::cursor() {
        if (auto hovered = locate(m_hoveredId)) {
            return hovered->element.get().cursor();
        }
        return CursorKind::Normal;
    }

    bool Screen::isTyping() {
        const auto target = locate(m_context.focusedId);
        return target && target->element.get().takesText();
    }

    bool Screen::isShown(const std::string& name) {
        std::function<bool(const Element&)> search = [&](const Element& element) {
            if (!element.isVisible()) {
                return false;
            }
            if (element.name() == name) {
                return true;
            }
            for (const auto& child : element.children()) {
                if (search(*child)) {
                    return true;
                }
            }
            return false;
        };
        return search(*m_root);
    }

    bool Screen::isTopmostAt(const std::string& name, core::Point position) {
        const auto found = findNamed(*m_root, name);
        return found && hitAt(position) == found->get().id();
    }

}
