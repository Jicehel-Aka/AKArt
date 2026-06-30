#pragma once
#include "kart_sprites.h"

namespace core {

// Retourne les sprites (déjà disponibles, embarqués en flash) pour une
// couleur de kart donnée. Aucune init nécessaire.
const KartSprites& get_kart_sprites(KartColor color);

void draw_kart_sprite(const KartSprites& sprites, KartSpriteId id, int x, int y);

// Affiche les 4 frames côte à côte (utile pour vérifier visuellement l'import).
void debug_show_kart(const KartSprites& sprites, int x, int y);

} // namespace core
