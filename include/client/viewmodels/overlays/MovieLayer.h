#ifndef NORTH_ISLAND_CLIENT_VIEWMODELS_OVERLAYS_MOVIE_LAYER_H
#define NORTH_ISLAND_CLIENT_VIEWMODELS_OVERLAYS_MOVIE_LAYER_H

#include "client/services/GameServices.h"
#include "client/subsystems/movie/MovieDirector.h"
#include "ui/DrawingArea.h"
#include "ui/Element.h"
#include "ui/Screen.h"
#include "ui/Spot.h"

#include <cstdint>

namespace client {

    /// @brief The screen of a story movie: black, the ground picture at its scroll place, pictures and roles by their feet, fade darkening and the efficacy-mode fill.
    /// @details Original: MovieLayer.xml, DrawMovie. It lies over the world and under the windows, and shows while the director's screen mode is on.
    class MovieLayer {
        public:
            /// @brief Loads the layer and sets its painter; creates the MovieDirector.
            /// @param services Shared game services.
            /// @param screen Screen the layer is loaded into.
            /// @param hooks Hooks handed to the MovieDirector.
            MovieLayer(
                GameServices& services,
                ui::Screen& screen,
                MovieDirector::Hooks hooks
            );

            /// @brief Gives the movie director.
            /// @return The MovieDirector.
            MovieDirector& director() { return m_director; }

            /// @brief Tells whether a movie is playing.
            /// @return True while the director is active.
            bool active() const { return m_director.active(); }

            /// @brief Tells whether the layer is on the screen: a movie plays or a held one has left its last picture.
            /// @return True when the layer should show.
            bool shown() const { return m_director.active() || m_director.frozen(); }

            /// @brief Tells whether the movie hides the world: shown and not veiled by the SFW setting (a veiled movie draws nothing, but the
            /// layer still takes the clicks so that none reaches the map).
            /// @return True when the world is not to be painted.
            bool coversWorld() const { return shown() && !m_director.veiled(); }

            /// @brief Steps the director once a frame and shows or hides the layer.
            /// @details Original: UpdateMovie.
            /// @param nowMs Current time in milliseconds.
            void update(std::uint64_t nowMs);

        private:
            /// @brief Draws the movie, zoomed when the screen filter zooms it.
            /// @details Original: DrawMovie; StretchBufToScreen: the screen filters 4 to 6 zoom what the movie draws (the middle of the screen cut out and stretched).
            /// @param canvas Canvas to draw on.
            /// @param nowMs Current time in milliseconds.
            void paint(gfx::Canvas& canvas, std::uint64_t nowMs);

            /// @brief Draws one movie frame: black, ground, effects, roles and pictures in order, colour fill and weather.
            /// @details Original: UpdateMovie fills the screen black first; DrawGround / DrawMovieMapGround (filter 3 leaves it out); DrawSceneEffectsUnder and DrawSceneEffectsOver of the efficacy mode (colour of the modes 7 to 9, then the weather); ro_Clipper_Alpha_Fill on the 16-bit screen.
            /// @param canvas Canvas to draw on.
            /// @param nowMs Current time in milliseconds.
            void paintScreen(gfx::Canvas& canvas, std::uint64_t nowMs);

            /// @brief Draws a strip picture frame with its foot at the place.
            /// @details Original: DrawPicture.
            /// @param canvas Canvas to draw on.
            /// @param picture The picture to draw.
            /// @param origin Scroll origin subtracted from world places.
            void paintPicture(gfx::Canvas& canvas, const MovieFrame::Picture& picture, core::Point origin);

            /// @brief Draws a role: shadow, then the sprite of its look at its action slot and frame (afterimage blended or only its light added), or its face.
            /// @details Original: DrawRole, DrawMovieRoleFace.
            /// @param canvas Canvas to draw on.
            /// @param role The role to draw.
            /// @param origin Scroll origin subtracted from world places.
            void paintRole(gfx::Canvas& canvas, const MovieFrame::Role& role, core::Point origin);

            GameServices& m_services;
            MovieDirector m_director;
            ui::Element& m_layer;
            const ui::Spot& m_screenSpot;
    };

}

#endif
