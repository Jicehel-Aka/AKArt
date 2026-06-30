/*
===============================================================================
  title_screen.cpp — Écran de titre d'Akart
-------------------------------------------------------------------------------
  Version compatible AKA :
    - Affiche l’image AKArt (320x240)
    - Fade‑in progressif (sans alpha, compatible moteur AKA)
    - Clignotement “Appuyez sur A”
===============================================================================
*/

#include "title_screen.h"
#include "../core/graphics.h"
#include "../assets_data/title_screen_data.h"
#include <cmath>    // pour fmodf()

namespace kart {

using namespace core;

void title_screen_draw(float blink_time) {

    // --- 1) Dessin de l’image de fond ---
    graphics_draw_bitmap565(0, 0, 320, 240, title_screen_pixels, false);

    // --- 2) Fade‑in progressif (0 → 1 seconde) ---
    float fade = blink_time < 1.0f ? blink_time : 1.0f;

    if (fade < 1.0f) {
        // On fonce l’image en dessinant un voile noir proportionnel
        // (pas d’alpha dans AKA → on simule en dessinant plusieurs bandes)
        int darkness = (int)((1.0f - fade) * 8);  // 0 → 8 passes

        for (int i = 0; i < darkness; ++i) {
            graphics_draw_rect(0, 0, 320, 240, Color::Black);
        }
    }

    // --- 3) Clignotement “Appuyez sur A” ---
    // (si ton image contient déjà le texte, tu peux commenter cette section)
    bool blink = fmodf(blink_time, 1.0f) < 0.5f;
    if (blink) {
        graphics_draw_text_center(200, "Appuyez sur A", Color::White);
    }
}

} // namespace kart
