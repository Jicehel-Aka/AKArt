/*
===============================================================================
  title_screen.cpp — Écran de titre d'Akart
-------------------------------------------------------------------------------
  Version compatible AKA :
    - Affiche l’image AKArt (320x240)
    - Fade‑in progressif (sans alpha, compatible moteur AKA)
    - Clignotement “Appuyez sur A”
    - Ajout d’un petit menu d’accueil :
         * A : Jouer
         * B : Sprite Viewer
===============================================================================
*/

#include "title_screen.h"
#include "../core/graphics.h"
#include "../assets/gfx/title_screen_data.h"
#include <cmath>    // pour fmodf()
#include <cstdio>   // pour snprintf()

namespace kart {

using namespace core;

void title_screen_draw(float blink_time, const char* track_name) {

    // --- 1) Dessin de l'image de fond ---
    graphics_draw_bitmap565(0, 0, 320, 240, title_screen_pixels, false);

    // --- 2) Fade-in progressif (0 → 1 seconde) ---
    float fade = (blink_time < 1.0f) ? blink_time : 1.0f;

    if (fade < 1.0f) {
        // On fonce l'image en dessinant un voile noir proportionnel
        // (pas d'alpha dans AKA → on simule en dessinant plusieurs bandes)
        int darkness = (int)((1.0f - fade) * 8);  // 0 → 8 passes

        for (int i = 0; i < darkness; ++i) {
            graphics_draw_rect(0, 0, 320, 240, Color::Black);
        }
    }

    // --- 3) Cache le "PRESS START" incrusté dans l'image de fond -------
    // (autour de y=218-226, x=114-197 dans le bitmap d'origine) : sans ça,
    // notre propre texte se superposait dessus. On pose un bandeau uni par
    // dessus avant de dessiner notre propre menu.
    graphics_fill_rect(80, 210, 160, 26, Color::Black);

    // --- 4) Clignotement "Appuyez sur A pour lancer la course" ---
    bool blink = fmodf(blink_time, 1.0f) < 0.5f;
    if (blink) {
        graphics_draw_text_center(200, "Appuyez sur A pour lancer la course", Color::White);
    }

    // --- 5) Sélecteur de circuit (Joystick G/D pour changer) ---
    char theme_line[48];
    snprintf(theme_line, sizeof(theme_line), "< %s >", track_name);
    graphics_draw_text_center(214, theme_line, Color::Cyan);

    // --- 6) Menu d'accueil ---
    // "A : Jouer" retiré (redondant avec le message clignotant ci-dessus).
    graphics_draw_text_center(226, "B : Sprites   Menu : Options", Color::Cyan);
}

} // namespace kart
