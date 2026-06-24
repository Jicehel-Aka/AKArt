/*
===============================================================================
  graphics.cpp — Implémentation graphique Akart (basée sur gb_graphics)
===============================================================================
*/
#include "graphics.h"
#include "gb_graphics.h"
#include "gb_common.h"
#include <algorithm>

extern char font8x8_basic[128][8];

namespace core {

static gb_graphics g_gfx;

// Pile de translation (petite, suffit pour l'effet de vibration caméra)
static constexpr int kMaxTranslateStack = 8;
static int s_tx_stack[kMaxTranslateStack];
static int s_ty_stack[kMaxTranslateStack];
static int s_tx = 0, s_ty = 0, s_stack_top = 0;

static inline uint16_t rgb(uint8_t r, uint8_t g, uint8_t b) {
    return g_gfx.makeColor(r, g, b);
}

static uint16_t color_to_rgb565(Color c) {
    switch (c) {
        case Color::Black:     return rgb(0, 0, 0);
        case Color::White:     return rgb(255, 255, 255);
        case Color::Yellow:    return rgb(255, 220, 0);
        case Color::Red:       return rgb(220, 30, 30);
        case Color::Green:     return rgb(40, 180, 70);
        case Color::DarkGray:  return rgb(60, 60, 65);
        case Color::Gray:      return rgb(130, 130, 135);
        case Color::SkyBlue:   return rgb(110, 180, 230);
        case Color::DarkBlue:  return rgb(20, 30, 70);
        case Color::Orange:    return rgb(230, 130, 30);
        case Color::Brown:     return rgb(110, 70, 40);
        case Color::LightGray: return rgb(190, 190, 195);
    }
    return rgb(255, 0, 255);
}

void graphics_init() {
    g_gfx.set_backlight_percent(80);
    g_gfx.set_refresh_rate(60);
}

void graphics_present() { g_gfx.update(); }

void graphics_clear(Color color) { g_gfx.clear(color_to_rgb565(color)); }

void graphics_fill_rect(int x, int y, int w, int h, Color color) {
    g_gfx.setColor(color_to_rgb565(color));
    g_gfx.fillRect(x + s_tx, y + s_ty, w, h);
}

void graphics_draw_rect(int x, int y, int w, int h, Color color) {
    g_gfx.setColor(color_to_rgb565(color));
    g_gfx.drawRect(x + s_tx, y + s_ty, w, h);
}

void graphics_draw_line(int x0, int y0, int x1, int y1, Color color) {
    g_gfx.setColor(color_to_rgb565(color));
    g_gfx.drawLine(x0 + s_tx, y0 + s_ty, x1 + s_tx, y1 + s_ty);
}

void graphics_fill_circle(int cx, int cy, int r, Color color) {
    g_gfx.setColor(color_to_rgb565(color));
    g_gfx.fillCircle(cx + s_tx, cy + s_ty, r);
}

void graphics_draw_circle(int cx, int cy, int r, Color color) {
    g_gfx.setColor(color_to_rgb565(color));
    g_gfx.drawCircle(cx + s_tx, cy + s_ty, r);
}

void graphics_fill_triangle(int x0, int y0, int x1, int y1, int x2, int y2, Color color) {
    g_gfx.setColor(color_to_rgb565(color));
    g_gfx.fillTriangle(x0 + s_tx, y0 + s_ty, x1 + s_tx, y1 + s_ty, x2 + s_tx, y2 + s_ty);
}

void graphics_fill_trapezoid(float x1l, float x1r, int y1,
                              float x2l, float x2r, int y2,
                              Color color)
{
    // On normalise pour que y1 <= y2 (en gardant chaque arête associée à sa ligne)
    if (y1 > y2) {
        std::swap(x1l, x2l);
        std::swap(x1r, x2r);
        std::swap(y1, y2);
    }
    if (y2 <= y1) y2 = y1 + 1;

    // Garde-fou CRITIQUE : quand un segment de route est projeté très proche
    // de la caméra (dz proche de 0), les coordonnées explosent (des centaines
    // de milliers de pixels). Sans cet écrêtage, la boucle scanline ci-dessous
    // tente de dessiner autant de lignes hors écran -> blocage du task et
    // déclenchement du watchdog (vu en pratique sur la carte AKA).
    // On calcule le facteur d'interpolation sur le span ORIGINAL (pour garder
    // une perspective correcte), mais on ne boucle que sur les lignes
    // réellement visibles à l'écran.
    const int screen_h = graphics_height();
    const int screen_w = graphics_width();

    float span = (float)(y2 - y1);
    if (span < 1.0f) span = 1.0f;

    int loop_y1 = y1 < 0 ? 0 : y1;
    int loop_y2 = y2 > screen_h - 1 ? screen_h - 1 : y2;
    if (loop_y1 > loop_y2) return; // entièrement hors écran verticalement

    g_gfx.setColor(color_to_rgb565(color));
    for (int y = loop_y1; y <= loop_y2; ++y) {
        float t = (y - y1) / span;
        float xl = x1l + (x2l - x1l) * t;
        float xr = x1r + (x2r - x1r) * t;
        if (xl > xr) std::swap(xl, xr);

        // Écrêtage horizontal également (mêmes raisons que ci-dessus)
        if (xl < 0.0f) xl = 0.0f;
        if (xr > (float)(screen_w - 1)) xr = (float)(screen_w - 1);
        if (xl > xr) continue;

        int ix0 = (int)xl, ix1 = (int)xr;
        g_gfx.fillRect(ix0 + s_tx, y + s_ty, (ix1 - ix0) + 1, 1);
    }
}

void graphics_draw_text(int x, int y, const char* text, Color color) {
    uint16_t col = color_to_rgb565(color);
    int px = x + s_tx;
    int py = y + s_ty;
    while (*text) {
        unsigned char c = (unsigned char)*text++;
        if (c >= 128) c = '?';
        const uint8_t* glyph = (const uint8_t*)font8x8_basic[c];
        for (int gy = 0; gy < 8; ++gy) {
            uint8_t row = glyph[gy];
            for (int gx = 0; gx < 8; ++gx)
                if (row & (1 << gx))
                    g_gfx.drawPixel(px + gx, py + gy, col);
        }
        px += 8;
    }
}

void graphics_draw_text_center(int y, const char* text, Color color) {
    int len = 0;
    for (const char* p = text; *p; ++p) len++;
    graphics_draw_text((graphics_width() - len * 8) / 2, y, text, color);
}

void graphics_push() {
    if (s_stack_top < kMaxTranslateStack) {
        s_tx_stack[s_stack_top] = s_tx;
        s_ty_stack[s_stack_top] = s_ty;
        s_stack_top++;
    }
}

void graphics_translate(int dx, int dy) {
    s_tx += dx;
    s_ty += dy;
}

void graphics_pop() {
    if (s_stack_top > 0) {
        s_stack_top--;
        s_tx = s_tx_stack[s_stack_top];
        s_ty = s_ty_stack[s_stack_top];
    }
}

void graphics_get_translate(int* dx, int* dy) {
    *dx = s_tx;
    *dy = s_ty;
}

void graphics_draw_sprite(SpriteId id, int x, int y) {
    sprite_draw(id, x, y);
}

void graphics_draw_sprite_rotated(SpriteId id, int x, int y, float angle_deg) {
    sprite_draw_rotated(id, x, y, angle_deg);
}

int graphics_width()  { return SCREEN_WIDTH; }
int graphics_height() { return SCREEN_HEIGHT; }

} // namespace core
