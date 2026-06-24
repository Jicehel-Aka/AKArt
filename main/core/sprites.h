/*
===============================================================================
  sprites.h — Sprites du jeu de Kart (Akart)
-------------------------------------------------------------------------------
  Spécification (résumé) :
    - SpriteId::KartPlayer : 32x32, vue arrière du kart, style cartoon
    - SpriteId::Wheel      : 64x64, volant vu de face (vue cockpit), centré
    - SpriteId::Dashboard  : 320x70, bandeau bas avec compteurs stylisés

  Il n'existe pas de fichier image fourni pour ce projet : les sprites sont
  donc générés PROCÉDURALEMENT (formes vectorielles dessinées à chaque appel
  avec les primitives de core/graphics), plutôt que d'être des bitmaps figés.
  Avantage : aucune ressource externe à embarquer, légère empreinte mémoire,
  et facile à recolorer/redimensionner plus tard.
===============================================================================
*/
#pragma once

namespace core {

enum class SpriteId {
    KartPlayer, // 32x32 - vue arrière du kart du joueur
    Wheel,      // 64x64 - volant (vue cockpit), pivote avec la direction
    Dashboard,  // 320x70 - bandeau du bas (optionnel, utilisable via fill_rect)
};

// Dimensions logiques de référence (cf. spécification) -----------------------
constexpr int KART_PLAYER_W = 32, KART_PLAYER_H = 32;
constexpr int WHEEL_W = 64, WHEEL_H = 64;
constexpr int DASHBOARD_W = 320, DASHBOARD_H = 70;

// Dessine le sprite à la position (x,y) = coin haut-gauche, sans rotation.
void sprite_draw(SpriteId id, int x, int y);

// Dessine le sprite à la position (x,y) = coin haut-gauche, avec rotation
// (en degrés, sens horaire) autour de son propre centre. Utilisé pour le volant.
void sprite_draw_rotated(SpriteId id, int x, int y, float angle_deg);

} // namespace core
