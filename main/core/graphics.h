/*
===============================================================================
  graphics.h — Couche graphique Akart (wrap gb_graphics)
===============================================================================
*/
#pragma once
#include <cstdint>
#include "sprites.h"

namespace core {

enum class Color {
    Black,
    White,
    Yellow,
    Red,
    Green,
    DarkGreen,   // ← ajouté : 2e teinte d'herbe (au lieu de réutiliser DarkGray, qui donnait un gris pur, pas une nuance de vert)
    DarkGray,
    Gray,
    SkyBlue,
    DarkBlue,
    Orange,
    Brown,
    LightGray,
    Cyan,
};

void graphics_init();
void graphics_present();   // transfère le framebuffer à l'écran

void graphics_clear(Color color);

void graphics_fill_rect(int x, int y, int w, int h, Color color);
void graphics_draw_rect(int x, int y, int w, int h, Color color);
void graphics_draw_line(int x0, int y0, int x1, int y1, Color color);
void graphics_fill_circle(int cx, int cy, int r, Color color);
void graphics_draw_circle(int cx, int cy, int r, Color color);
void graphics_fill_triangle(int x0, int y0, int x1, int y1, int x2, int y2, Color color);

// Trapèze horizontal (segment de route façon OutRun) entre deux lignes y1/y2,
// chacune avec ses propres bords gauche/droit.
void graphics_fill_trapezoid(float x1l, float x1r, int y1,
                              float x2l, float x2r, int y2,
                              Color color);

void graphics_draw_text(int x, int y, const char* text, Color color = Color::White);
void graphics_draw_text_center(int y, const char* text, Color color = Color::White);

// Pile de translation simple (utilisée pour les petits effets de vibration caméra)
void graphics_push();
void graphics_translate(int dx, int dy);
void graphics_pop();

// Sprites (cf. core/sprites.h)
void graphics_draw_sprite(SpriteId id, int x, int y);
void graphics_draw_sprite_rotated(SpriteId id, int x, int y, float angle_deg);

// Bitmap RGB565 brut (sprites importés, cf. core/kart_loader.h). Le pixel de
// valeur transparent_key (magenta 0xF81F par défaut) n'est pas dessiné.
constexpr uint16_t TRANSPARENT_KEY = 0xF81F;
void graphics_draw_bitmap565(int x, int y, int w, int h, const uint16_t* data,
                              bool use_transparency = true,
                              uint16_t transparent_key = TRANSPARENT_KEY);

// Variante avec mise à l'échelle (nearest-neighbor, suffisant en pixel-art).
// (x,y) = coin haut-gauche de la zone DE DESTINATION (déjà à l'échelle).
// dst_w/dst_h = taille voulue à l'écran ; src_w/src_h = taille réelle des
// données sources (data). Utilisée pour les karts adverses, dont la taille
// à l'écran varie avec la distance (perspective).
void graphics_draw_bitmap565_scaled(int x, int y, int dst_w, int dst_h,
                                     const uint16_t* data, int src_w, int src_h,
                                     bool use_transparency = true,
                                     uint16_t transparent_key = TRANSPARENT_KEY);

// Dessin direct d'un pixel RGB565 hardware (sans safe_coord ni translation) —
// utilisé uniquement pour le rendu du fond défilant (draw_sky) dont les
// coordonnées sont toujours dans les limites de l'écran.
void graphics_draw_pixel_raw(int x, int y, uint16_t color);

// Dessin d'une bande horizontale RGB565 hardware (sans translation ni écrêtage).
// Utilisé par le rendu du fond défilant (draw_sky) — plus rapide que fillRect
// car évite la conversion de couleur (données déjà en format hardware).
void graphics_hline_raw(int x, int y, int w, uint16_t color);

int graphics_width();
int graphics_height();

// Capture l'écran courant (ce qui a déjà été dessiné dans le framebuffer, AVANT
// graphics_present()) et l'enregistre en BMP 24 bits sur la carte SD, dans
// /sdcard/AKART/, sous un nom court 8.3 (SHOTxxxx.BMP) auto-incrémenté.
// Retourne true si l'écriture a réussi ; out_path (si non nul) reçoit le
// chemin complet du fichier écrit (utile pour l'affichage d'un message).
bool graphics_save_screenshot_bmp(char* out_path = nullptr, int out_path_size = 0);

// Accès interne utilisé par sprites.cpp pour dessiner avec les coordonnées
// déjà décalées par la pile de translation.
void graphics_get_translate(int* dx, int* dy);

} // namespace core
