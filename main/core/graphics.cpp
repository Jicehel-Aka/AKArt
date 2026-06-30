/*
===============================================================================
  graphics.cpp — Implémentation graphique Akart (basée sur gb_graphics)
-------------------------------------------------------------------------------
  IMPORTANT : gb_graphics attend des coordonnées int16_t (short). Le moteur
  de rendu pseudo-3D peut, dans certains cas (virage cumulé, division par une
  distance proche de 0...), produire des coordonnées qui dépassent largement
  cette plage. Sans protection, ces valeurs "wrap" en nombres aberrants au
  niveau du driver LCD bas niveau, qui se met alors à boucler indéfiniment
  (vu en pratique : "Loop scanline 0 timeout" puis crash du task watchdog).
  TOUTES les primitives ci-dessous écrêtent donc systématiquement leurs
  coordonnées à une plage large mais sûre avant d'appeler gb_graphics.
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

// Plage de sécurité : largement suffisante pour tout dessin (même très
// hors-écran), mais bien à l'intérieur des limites d'un int16_t (±32767).
static constexpr int kSafeCoordLimit = 4000;

static inline int16_t safe_coord(int v) {
    if (v > kSafeCoordLimit) v = kSafeCoordLimit;
    if (v < -kSafeCoordLimit) v = -kSafeCoordLimit;
    return (int16_t)v;
}

static inline int16_t safe_size(int v) {
    if (v < 0) v = 0;
    if (v > kSafeCoordLimit) v = kSafeCoordLimit;
    return (int16_t)v;
}

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
    g_gfx.fillRect(safe_coord(x + s_tx), safe_coord(y + s_ty), safe_size(w), safe_size(h));
}

void graphics_draw_rect(int x, int y, int w, int h, Color color) {
    g_gfx.setColor(color_to_rgb565(color));
    g_gfx.drawRect(safe_coord(x + s_tx), safe_coord(y + s_ty), safe_size(w), safe_size(h));
}

void graphics_draw_line(int x0, int y0, int x1, int y1, Color color) {
    g_gfx.setColor(color_to_rgb565(color));
    g_gfx.drawLine(safe_coord(x0 + s_tx), safe_coord(y0 + s_ty),
                    safe_coord(x1 + s_tx), safe_coord(y1 + s_ty));
}

void graphics_fill_circle(int cx, int cy, int r, Color color) {
    g_gfx.setColor(color_to_rgb565(color));
    g_gfx.fillCircle(safe_coord(cx + s_tx), safe_coord(cy + s_ty), safe_size(r));
}

void graphics_draw_circle(int cx, int cy, int r, Color color) {
    g_gfx.setColor(color_to_rgb565(color));
    g_gfx.drawCircle(safe_coord(cx + s_tx), safe_coord(cy + s_ty), safe_size(r));
}

void graphics_fill_triangle(int x0, int y0, int x1, int y1, int x2, int y2, Color color) {
    g_gfx.setColor(color_to_rgb565(color));
    g_gfx.fillTriangle(safe_coord(x0 + s_tx), safe_coord(y0 + s_ty),
                        safe_coord(x1 + s_tx), safe_coord(y1 + s_ty),
                        safe_coord(x2 + s_tx), safe_coord(y2 + s_ty));
}

void graphics_fill_trapezoid(float x1l, float x1r, int y1,
                              float x2l, float x2r, int y2,
                              Color color)
{
    if (y1 > y2) {
        std::swap(x1l, x2l);
        std::swap(x1r, x2r);
        std::swap(y1, y2);
    }
    if (y2 <= y1) y2 = y1 + 1;

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

        if (xl < 0.0f) xl = 0.0f;
        if (xr > (float)(screen_w - 1)) xr = (float)(screen_w - 1);
        if (xl > xr) continue;

        int ix0 = (int)xl, ix1 = (int)xr;
        g_gfx.fillRect(safe_coord(ix0 + s_tx), safe_coord(y + s_ty), safe_size(ix1 - ix0 + 1), 1);
    }
}

void graphics_draw_text(int x, int y, const char* text, Color color) {
    uint16_t col = color_to_rgb565(color);

    // Hors-champ large : on évite de boucler sur du texte totalement
    // invisible (protection légère, le vrai risque est dans les formes
    // dérivées de la perspective, pas le texte, mais coûte rien d'être sûr).
    if (x < -kSafeCoordLimit || x > kSafeCoordLimit ||
        y < -kSafeCoordLimit || y > kSafeCoordLimit) return;

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
                    g_gfx.drawPixel(safe_coord(px + gx), safe_coord(py + gy), col);
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

void graphics_draw_bitmap565(int x, int y, int w, int h, const uint16_t* data,
                              bool use_transparency, uint16_t transparent_key) {
    // w/h viennent toujours de constantes connues (sprites 32x32, titre
    // 320x240) : pas besoin de les écrêter. x/y peuvent en théorie dériver
    // d'un calcul de jeu ; on ignore l'appel s'ils sont absurdes.
    if (x < -kSafeCoordLimit || x > kSafeCoordLimit ||
        y < -kSafeCoordLimit || y > kSafeCoordLimit) return;

    int px = x + s_tx;
    int py = y + s_ty;
    for (int row = 0; row < h; ++row) {
        const uint16_t* line = data + row * w;
        for (int col = 0; col < w; ++col) {
            uint16_t v = line[col];
            if (use_transparency && v == transparent_key) continue;
            g_gfx.drawPixel(safe_coord(px + col), safe_coord(py + row), v);
        }
    }
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
