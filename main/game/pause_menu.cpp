/*
===============================================================================
  pause_menu.cpp — Menu Pause d'Akart
-------------------------------------------------------------------------------
  Rôle :
    - Gérer le menu pause :
         * B → reprendre la course
         * C → retour à l'écran titre
         * RUN+MENU → loader (géré dans app_main.cpp)
    - Le menu pause est un overlay : la course est dessinée derrière.
===============================================================================
*/

#include "pause_menu.h"
#include "kart_game.h"      // pour accéder à mode et kart_game_init()
#include "music_registry.h"
#include "../core/input.h"
#include "../core/audio.h"
#include "../core/graphics.h"

namespace kart {

// Mode et mode sont déclarés dans kart_game.h (inclus ci-dessus) et définis
// dans kart_game.cpp.

// -----------------------------------------------------------------------------
// pause_menu_update()
// -----------------------------------------------------------------------------
void pause_menu_update() {
    using namespace core;

    // B → reprendre la course
    if (input_was_pressed(Button::B)) {
        mode = Mode::Race;
        return;
    }

    // C → retour à l'écran titre
    if (input_was_pressed(Button::C)) {
        kart_abandon_cup();    // abandonne la coupe en cours (cf. kart_game.cpp)
        kart_game_init();      // reset complet du jeu
        mode = Mode::Title;
        auto m = music_title_ref();
        audio_play_music(m.data, m.len);
        return;
    }

    // RUN+MENU → loader (géré dans app_main.cpp)
}

// -----------------------------------------------------------------------------
// pause_menu_draw()
// -----------------------------------------------------------------------------
void pause_menu_draw() {
    using namespace core;

    // Fond semi-opaque (overlay)
    graphics_fill_rect(60, 60, 200, 120, Color::DarkBlue);
    graphics_draw_rect(60, 60, 200, 120, Color::White);

    graphics_draw_text_center(78,  "PAUSE", Color::Yellow);
    graphics_draw_text_center(106, "B : reprendre",    Color::White);
    graphics_draw_text_center(124, "C : quitter",      Color::White);
    graphics_draw_text_center(148, "(RUN+MENU = loader)", Color::Gray);
}

} // namespace kart
