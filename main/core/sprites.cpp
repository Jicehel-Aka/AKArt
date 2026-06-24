/*
===============================================================================
  sprites.cpp — Sprites procéduraux du Kart (Akart)
-------------------------------------------------------------------------------
  Aucun bitmap externe : chaque sprite est dessiné avec des formes vectorielles
  (rects/cercles/triangles) à partir de core/graphics. C'est l'approche la
  plus simple et la plus légère sans pipeline d'import d'images.
===============================================================================
*/
#include "sprites.h"
#include "graphics.h"
#include <cmath>

namespace core {

// -----------------------------------------------------------------------------
//  SpriteId::KartPlayer — 32x32, vue arrière, style cartoon
// -----------------------------------------------------------------------------
//  (x,y) = coin haut-gauche de la zone 32x32.
static void draw_kart_player(int x, int y) {
    // Ombre au sol
    graphics_fill_circle(x + 16, y + 29, 13, Color::DarkGray);

    // Roues arrière (vue de derrière : on les voit de trois-quarts, simplifiées
    // en deux gros disques noirs avec un moyeu clair)
    graphics_fill_circle(x + 6,  y + 24, 7, Color::Black);
    graphics_fill_circle(x + 26, y + 24, 7, Color::Black);
    graphics_fill_circle(x + 6,  y + 24, 3, Color::Gray);
    graphics_fill_circle(x + 26, y + 24, 3, Color::Gray);

    // Châssis (corps du kart), forme évasée vers le bas
    graphics_fill_triangle(x + 8,  y + 22, x + 24, y + 22, x + 4,  y + 12, Color::Red);
    graphics_fill_triangle(x + 24, y + 22, x + 4,  y + 12, x + 28, y + 12, Color::Red);
    graphics_fill_rect(x + 6, y + 10, 20, 10, Color::Red);

    // Aileron / spoiler arrière
    graphics_fill_rect(x + 4, y + 8, 24, 3, Color::DarkGray);
    graphics_fill_rect(x + 6, y + 4, 4, 5, Color::DarkGray);
    graphics_fill_rect(x + 22, y + 4, 4, 5, Color::DarkGray);

    // Pot d'échappement
    graphics_fill_rect(x + 2, y + 23, 3, 4, Color::DarkGray);
    graphics_fill_rect(x + 27, y + 23, 3, 4, Color::DarkGray);

    // Pilote : casque rond + visière
    graphics_fill_circle(x + 16, y + 8, 7, Color::SkyBlue);
    graphics_fill_rect(x + 10, y + 7, 12, 3, Color::DarkBlue); // visière
    graphics_draw_circle(x + 16, y + 8, 7, Color::Black);
}

// -----------------------------------------------------------------------------
//  SpriteId::Wheel — 64x64, volant vu de face (cockpit), rotation autour du centre
// -----------------------------------------------------------------------------
static void draw_wheel(int x, int y, float angle_deg) {
    int cx = x + WHEEL_W / 2;
    int cy = y + WHEEL_H / 2;
    int outer_r = 28;
    int inner_r = 21;
    int hub_r   = 7;

    // Jante extérieure (anneau : grand disque sombre + disque "trou" couleur fond)
    graphics_fill_circle(cx, cy, outer_r, Color::DarkGray);
    graphics_fill_circle(cx, cy, inner_r, Color::DarkBlue); // doit matcher le fond du tableau de bord

    // 3 branches, tournant avec le volant
    float rad = angle_deg * 3.14159265f / 180.0f;
    for (int i = 0; i < 3; ++i) {
        float a = rad + i * (2.0f * 3.14159265f / 3.0f);
        int ex = cx + (int)(std::cos(a) * inner_r);
        int ey = cy + (int)(std::sin(a) * inner_r);
        graphics_draw_line(cx, cy, ex, ey, Color::Gray);
        graphics_draw_line(cx + 1, cy, ex + 1, ey, Color::Gray);
    }

    // Moyeu central
    graphics_fill_circle(cx, cy, hub_r, Color::Black);
    graphics_fill_circle(cx, cy, hub_r - 3, Color::Gray);

    // Petit repère lumineux en haut de la jante (lisible même tourné)
    float top_a = rad - 3.14159265f / 2.0f;
    int tx = cx + (int)(std::cos(top_a) * (outer_r - 3));
    int ty = cy + (int)(std::sin(top_a) * (outer_r - 3));
    graphics_fill_circle(tx, ty, 2, Color::Yellow);
}

// -----------------------------------------------------------------------------
//  SpriteId::Dashboard — 320x70, bandeau bas avec compteurs stylisés
// -----------------------------------------------------------------------------
static void draw_dashboard(int x, int y) {
    // Fond du bandeau
    graphics_fill_rect(x, y, DASHBOARD_W, DASHBOARD_H, Color::DarkBlue);
    graphics_draw_line(x, y, x + DASHBOARD_W, y, Color::Black);

    // Cadran gauche (vitesse) : juste le cadre, l'aiguille/texte est géré par
    // kart_render.cpp (qui connaît la vitesse courante)
    int speedo_cx = x + 50, speedo_cy = y + 35;
    graphics_fill_circle(speedo_cx, speedo_cy, 30, Color::Black);
    graphics_draw_circle(speedo_cx, speedo_cy, 30, Color::Gray);
    graphics_draw_circle(speedo_cx, speedo_cy, 26, Color::DarkGray);

    // Graduations du cadran
    for (int i = 0; i <= 8; ++i) {
        float a = 3.14159265f * (0.75f + i * 1.5f / 8.0f);
        int gx0 = speedo_cx + (int)(std::cos(a) * 22);
        int gy0 = speedo_cy + (int)(std::sin(a) * 22);
        int gx1 = speedo_cx + (int)(std::cos(a) * 27);
        int gy1 = speedo_cy + (int)(std::sin(a) * 27);
        graphics_draw_line(gx0, gy0, gx1, gy1, Color::LightGray);
    }

    // Cadran droit (tour / rang), purement décoratif ici
    int info_cx = x + DASHBOARD_W - 50, info_cy = y + 35;
    graphics_fill_circle(info_cx, info_cy, 30, Color::Black);
    graphics_draw_circle(info_cx, info_cy, 30, Color::Gray);

    // Bande centrale (zone pour le volant + jauges, laissée vide ici)
    graphics_fill_rect(x + 90, y + 5, DASHBOARD_W - 180, DASHBOARD_H - 10, Color::DarkBlue);
    graphics_draw_rect(x + 90, y + 5, DASHBOARD_W - 180, DASHBOARD_H - 10, Color::Gray);
}

// -----------------------------------------------------------------------------
//  Dispatch
// -----------------------------------------------------------------------------
void sprite_draw(SpriteId id, int x, int y) {
    switch (id) {
        case SpriteId::KartPlayer: draw_kart_player(x, y); break;
        case SpriteId::Wheel:      draw_wheel(x, y, 0.0f); break;
        case SpriteId::Dashboard:  draw_dashboard(x, y); break;
    }
}

void sprite_draw_rotated(SpriteId id, int x, int y, float angle_deg) {
    switch (id) {
        case SpriteId::Wheel: draw_wheel(x, y, angle_deg); break;
        // Les autres sprites n'ont pas de variante tournée pour l'instant :
        // on retombe simplement sur le rendu non tourné.
        default: sprite_draw(id, x, y); break;
    }
}

} // namespace core
