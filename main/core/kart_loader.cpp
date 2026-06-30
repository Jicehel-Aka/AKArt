/*
===============================================================================
  kart_loader.cpp — Accès aux sprites de kart importés
-------------------------------------------------------------------------------
  Les pixels sont des tableaux C++ classiques (cf. assets_data/ *.cpp, générés
  depuis les PNG fournis). On a délibérément abandonné EMBED_FILES/objcopy :
  le nom des symboles "_binary_..._start" qu'il génère dépend de la version
  d'ESP-IDF/CMake et a posé des soucis de lien sur certains environnements.
  Un tableau C++ normal compile et se lie de façon totalement portable.
===============================================================================
*/
#include "kart_loader.h"
#include "graphics.h"
#include "../assets_data/kart_rouge_data.h"
#include "../assets_data/kart_bleu_data.h"
#include "../assets_data/kart_jaune_data.h"
#include "../assets_data/kart_vert_data.h"

namespace core {

static KartSprites make_sprites(const uint16_t* base) {
    KartSprites s;
    for (int i = 0; i < 4; ++i)
        s.frame[i] = base + i * KART_SPRITE_PIXELS;
    return s;
}

const KartSprites& get_kart_sprites(KartColor color) {
    static const KartSprites s_red    = make_sprites(kart_rouge_pixels);
    static const KartSprites s_blue   = make_sprites(kart_bleu_pixels);
    static const KartSprites s_yellow = make_sprites(kart_jaune_pixels);
    static const KartSprites s_green  = make_sprites(kart_vert_pixels);

    switch (color) {
        case KartColor::Red:    return s_red;
        case KartColor::Blue:   return s_blue;
        case KartColor::Yellow: return s_yellow;
        case KartColor::Green:  return s_green;
    }
    return s_red;
}

void draw_kart_sprite(const KartSprites& sprites, KartSpriteId id, int x, int y) {
    const uint16_t* data = sprites.frame[(int)id];
    if (!data) return;
    graphics_draw_bitmap565(x, y, KART_SPRITE_W, KART_SPRITE_H, data);
}

void debug_show_kart(const KartSprites& sprites, int x, int y) {
    draw_kart_sprite(sprites, KartSpriteId::Back,  x + 0,  y);
    draw_kart_sprite(sprites, KartSpriteId::Left,  x + 40, y);
    draw_kart_sprite(sprites, KartSpriteId::Right, x + 80, y);
    draw_kart_sprite(sprites, KartSpriteId::Crash, x + 120, y);
}

} // namespace core
