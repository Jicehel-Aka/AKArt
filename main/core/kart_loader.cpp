/*
===============================================================================
  kart_loader.cpp — Accès aux sprites de kart importés
-------------------------------------------------------------------------------
  Les pixels sont des tableaux C++ classiques (cf. assets/gfx/ *.cpp, générés
  depuis les PNG fournis). On a délibérément abandonné EMBED_FILES/objcopy :
  le nom des symboles "_binary_..._start" qu'il génère dépend de la version
  d'ESP-IDF/CMake et a posé des soucis de lien sur certains environnements.
  Un tableau C++ normal compile et se lie de façon totalement portable.
===============================================================================
*/
#include "kart_loader.h"
#include "graphics.h"
#include "../assets/gfx/kart_rouge_data.h"
#include "../assets/gfx/kart_bleu_data.h"
#include "../assets/gfx/kart_jaune_data.h"
#include "../assets/gfx/kart_vert_data.h"
#include "../assets/gfx/kart_rose_data.h"
#include "../assets/gfx/kart_gris_data.h"
#include "../assets/gfx/kart_gris_fonce_data.h"
#include "../assets/gfx/kart_violet_data.h"

namespace core {

static KartSprites make_sprites(const uint16_t* base) {
    KartSprites s;
    for (int i = 0; i < 4; ++i)
        s.frame[i] = base + i * KART_SPRITE_PIXELS;
    return s;
}

const KartSprites& get_kart_sprites(KartColor color) {
    static const KartSprites s_red       = make_sprites(kart_rouge_pixels);
    static const KartSprites s_blue      = make_sprites(kart_bleu_pixels);
    static const KartSprites s_yellow    = make_sprites(kart_jaune_pixels);
    static const KartSprites s_green     = make_sprites(kart_vert_pixels);
    static const KartSprites s_pink      = make_sprites(kart_rose_data_pixels);
    static const KartSprites s_gray      = make_sprites(kart_gris_data_pixels);
    static const KartSprites s_darkgray  = make_sprites(kart_gris_fonce_data_pixels);
    static const KartSprites s_purple    = make_sprites(kart_violet_data_pixels);

    switch (color) {
        case KartColor::Red:      return s_red;
        case KartColor::Blue:     return s_blue;
        case KartColor::Yellow:   return s_yellow;
        case KartColor::Green:    return s_green;
        case KartColor::Pink:     return s_pink;
        case KartColor::Gray:     return s_gray;
        case KartColor::DarkGray: return s_darkgray;
        case KartColor::Purple:   return s_purple;
    }
    return s_red;
}

void draw_kart_sprite(const KartSprites& sprites, KartSpriteId id, int x, int y) {
    const uint16_t* data = sprites.frame[(int)id];
    if (!data) return;
    graphics_draw_bitmap565(x, y, KART_SPRITE_W, KART_SPRITE_H, data);
}

void draw_kart_sprite_scaled(const KartSprites& sprites, KartSpriteId id,
                              int x, int y, int dst_size) {
    const uint16_t* data = sprites.frame[(int)id];
    if (!data) return;
    graphics_draw_bitmap565_scaled(x, y, dst_size, dst_size,
                                    data, KART_SPRITE_W, KART_SPRITE_H);
}

void debug_show_kart(const KartSprites& sprites, int x, int y) {
    draw_kart_sprite(sprites, KartSpriteId::Back,  x + 0,  y);
    draw_kart_sprite(sprites, KartSpriteId::Left,  x + 40, y);
    draw_kart_sprite(sprites, KartSpriteId::Right, x + 80, y);
    draw_kart_sprite(sprites, KartSpriteId::Crash, x + 120, y);
}

} // namespace core
