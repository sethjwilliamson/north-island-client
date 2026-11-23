#include "client/viewmodels/overlays/MovieLayer.h"

#include "client/rendering/characters/FigureShadow.h"
#include "client/rendering/characters/SpriteHighlight.h"
#include "formats/GamePixels.h"
#include "gfx/RenderDevice.h"
#include "gfx/Texture.h"

#include <variant>

namespace client {

    namespace {

        constexpr int kLargeDrop = 68; // px a large-sprite NPC is moved down from its foot point
        constexpr int kTallDrop = 32; // px a tall (heightMode 1) NPC is moved down
        constexpr int kTallLargeDrop = 168; // px a tall NPC with a large sprite (spriteSize 1) is moved down
        constexpr int kTallScale = 20; // highlight scale used for tall NPCs
        constexpr int kFaceSlot = 0; // sprite slot of the face sprite
        constexpr int kFaceDrop = 26; // px the face portrait is drawn below the foot point

        /// @brief Loads MovieLayer.xml into the screen and returns its layer.
        /// @param services Shared game services.
        /// @param screen Screen to load into.
        /// @return The "MovieLayer" element.
        ui::Element& loadLayer(GameServices& services, ui::Screen& screen) {
            screen.load(services.game.layout("MovieLayer.xml"), services.loadContext());
            return screen.get<ui::Element>("MovieLayer");
        }

    }

    MovieLayer::MovieLayer(
        GameServices& services,
        ui::Screen& screen,
        MovieDirector::Hooks hooks
    )
        : m_services(services),
          m_director(services, std::move(hooks)),
          m_layer(loadLayer(services, screen)),
          m_screenSpot(screen.get<ui::Spot>("MovieScreen")) {
        screen.get<ui::DrawingArea>("MoviePage").setPainter([this](gfx::Canvas& canvas, core::Point, std::uint64_t nowMs) { paint(canvas, nowMs); });
    }

    void MovieLayer::update(std::uint64_t nowMs) {
        m_director.update(nowMs);
        m_layer.setVisible(shown());
    }

    void MovieLayer::paint(gfx::Canvas& canvas, std::uint64_t nowMs) {
        if (m_director.veiled()) {
            return; // SFW: the world painted under the layer shows instead of the movie
        }
        const core::Rect window = m_services.zoom.window();
        const core::Rect part = m_services.zoom.shown(nowMs);
        const bool zoomed = !(part == window) && part.width < window.width;
        if (zoomed) {
            canvas.beginZoom(window);
        }
        paintScreen(canvas, nowMs);
        if (zoomed) {
            canvas.endZoom(part);
        }
    }

    void MovieLayer::paintScreen(gfx::Canvas& canvas, std::uint64_t nowMs) {
        canvas.fillRect(canvas.window(), {0, 0, 0, 255}); // opaque black RGBA
        const auto frame = m_director.frame(canvas, nowMs);
        if (!frame) {
            return;
        }
        gfx::PictureLibrary& pictures = m_services.pictures;
        const auto ground = frame->groundShown && !frame->mapGround ? pictures.find(frame->groundName) : std::nullopt;
        const bool lit = frame->groundShown && frame->mapGround;
        std::vector<core::Point> people; // where the roles stand: the lighting may give them a light
        if (lit) {
            for (const MovieFrame::Role& role : frame->roles) {
                if (!role.face && role.slot >= 0) {
                    people.push_back(role.foot);
                }
            }
            m_director.paintMapGround(canvas, frame->groundOrigin);
            m_director.lightMapGround(canvas, frame->groundOrigin, people, nowMs);
        } else if (ground) {
            canvas.drawPicture(*ground, {-frame->groundOrigin.x, -frame->groundOrigin.y});
            if (frame->groundDarkening) {
                canvas.fillRectAlpha(m_screenSpot.rect(), {0, 0, 0, static_cast<std::uint8_t>(*frame->groundDarkening)}); // black with the fade's alpha
            }
        }
        if (frame->efficacyMode > 0) {
            m_director.paintSceneEffects(canvas, frame->groundOrigin, frame->efficacyMode, false, nowMs);
        }
        for (const MovieFrame::Drawn& entry : frame->order) {
            if (entry.object) {
                m_director.paintMapObject(canvas, entry.index, frame->groundOrigin, nowMs);
            } else if (entry.role) {
                paintRole(canvas, frame->roles[entry.index], frame->groundOrigin);
            } else {
                paintPicture(canvas, frame->pictures[entry.index], frame->groundOrigin);
            }
        }
        if (lit) {
            m_director.lightMapOver(canvas, frame->groundOrigin, people, nowMs);
        }
        if (frame->fill && !canvas.exactAlphaFills()) {
            canvas.fillRect(canvas.window(), *frame->fill);
        } else if (frame->fill) {
            formats::Image screen = canvas.device().readFrame();
            formats::alphaFill16(screen, *frame->fill);
            if (const auto filled = gfx::Texture::create(canvas.device(), screen)) {
                canvas.drawTexture(*filled, canvas.window().position());
            }
        }
        if (frame->efficacyMode > 0) {
            m_director.paintSceneEffects(canvas, frame->groundOrigin, frame->efficacyMode, true, nowMs);
        }
    }

    void MovieLayer::paintPicture(gfx::Canvas& canvas, const MovieFrame::Picture& drawn, core::Point origin) {
        gfx::PictureLibrary& pictures = m_services.pictures;
        const auto picture = pictures.find(drawn.name);
        if (!picture) {
            return;
        }
        const int frameWidth = pictures.picture(*picture).width() / drawn.cols;
        const int frameHeight = pictures.picture(*picture).height() / drawn.rows;
        const core::Point at{drawn.foot.x - frameWidth / 2 - origin.x, drawn.foot.y - frameHeight - origin.y};
        const core::Rect source{0, (drawn.frame - 1) * frameHeight, frameWidth, frameHeight};
        if (drawn.light) {
            canvas.drawPictureLight(*picture, at, drawn.alpha, source);
        } else {
            canvas.drawPicture(*picture, at, source);
        }
    }

    void MovieLayer::paintRole(gfx::Canvas& canvas, const MovieFrame::Role& role, core::Point origin) {
        const auto look = m_director.roleLook(role.surface);
        if (!look) {
            return;
        }
        core::Point feet{role.foot.x - origin.x, role.foot.y - origin.y};
        if (role.face) {
            if (std::holds_alternative<CharacterLook>(*look)) {
                m_services.characters.drawMovieFace(canvas, std::get<CharacterLook>(*look), {feet.x, feet.y + kFaceDrop}, role.faceFrame, role.faceFlip);
            } else if (const auto& npc = std::get<formats::NpcInfo>(*look); npc.faceSpriteId > 0) {
                m_services.characters
                    .drawFaceSprite(canvas, npc.faceSpriteId, {feet.x, feet.y + kFaceDrop}, kFaceSlot, role.faceFrame, CharacterRenderer::npcPattern(npc));
            }
            return;
        }
        const bool person = std::holds_alternative<CharacterLook>(*look);
        if (role.shadow) {
            drawFigureShadow(canvas, feet, !person && std::get<formats::NpcInfo>(*look).shadow == 1 ? 1 : 0);
        }
        if (role.slot < 0) {
            return;
        }
        SpriteHighlight highlight;
        highlight.scale = role.scale;
        if (!person) {
            const formats::NpcInfo& npc = std::get<formats::NpcInfo>(*look);
            if (npc.heightMode == 1) {
                highlight.scale = kTallScale;
                feet.y += npc.spriteSize == 1 ? kTallLargeDrop : kTallDrop;
            } else if (npc.spriteSize == 1) {
                feet.y += kLargeDrop;
            }
        }
        if (role.trail) {
            if (role.drawMode == 2) { // draw mode 2: afterimage blended with opacity; other modes add only its light
                highlight.opacity = static_cast<std::uint8_t>(role.alpha);
            } else {
                highlight.ghost = true;
                highlight.light = static_cast<std::uint8_t>(role.alpha);
            }
        }
        if (person) {
            m_services.characters.drawFigure(canvas, std::get<CharacterLook>(*look), feet, role.slot, role.frame, highlight);
        } else {
            // drawNpc adds the map's large-sprite drop (68, +32 when tall) itself; a movie role already has its own drop above and
            // the original's sprite routine adds none for it (the movie role's FHuman.FHumId is 0), so take that drop back off
            const formats::NpcInfo& npc = std::get<formats::NpcInfo>(*look);
            core::Point bodyFeet = feet;
            if (npc.spriteSize == 1) {
                bodyFeet.y -= kLargeDrop + (npc.heightMode == 1 ? kTallDrop : 0);
            }
            m_services.characters.drawNpc(canvas, npc, bodyFeet, role.slot, role.frame, highlight);
            m_services.characters.drawNpcLook(canvas, std::get<formats::NpcInfo>(*look), feet, role.slot, role.frame, highlight);
        }
    }

}
