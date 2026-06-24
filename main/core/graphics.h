/*
===============================================================================
  graphics.h — Couche graphique Akart (wrap gb_graphics)
===============================================================================
*/
#pragma once
#include "sprites.h"

namespace core {

enum class Color {
    Black,
    White,
    Yellow,
    Red,
    Green,
    DarkGray,
    Gray,
    SkyBlue,
    DarkBlue,
    Orange,
    Brown,
    LightGray,
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

int graphics_width();
int graphics_height();

// Accès interne utilisé par sprites.cpp pour dessiner avec les coordonnées
// déjà décalées par la pile de translation.
void graphics_get_translate(int* dx, int* dy);

} // namespace core
