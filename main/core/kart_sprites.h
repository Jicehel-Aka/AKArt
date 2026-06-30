/*
===============================================================================
  kart_sprites.h — Sprites de kart importés (art réel, 32x32 RGB565)
-------------------------------------------------------------------------------
  Adapté du schéma proposé par l'utilisateur, mais sans Arduino/SD.h : ces
  fichiers .bin sont embarqués directement dans le firmware (cf. EMBED_FILES
  dans main/CMakeLists.txt) plutôt que chargés depuis la carte SD. Avantages :
  aucune dépendance à une SD card bien formatée, et les données restent en
  flash (pas de copie en RAM) puisqu'on ne fait que pointer dans le binaire.

  Format d'un fichier .bin (généré depuis les PNG fournis) :
    4 frames consécutives de 32x32 pixels RGB565 little-endian, dans l'ordre
    SPR_BACK, SPR_LEFT, SPR_RIGHT, SPR_CRASH (= les frames 1,2,3,4 fournies).
    Le pixel magenta (0xF81F) sert de clé de transparence (cf. graphics.h).
===============================================================================
*/
#pragma once
#include <cstdint>

namespace core {

constexpr int KART_SPRITE_W = 32;
constexpr int KART_SPRITE_H = 32;
constexpr int KART_SPRITE_PIXELS = KART_SPRITE_W * KART_SPRITE_H;

enum class KartSpriteId {
    Back  = 0, // vue arrière (3e personne)
    Left  = 1, // penché à gauche (virage / drift)
    Right = 2, // penché à droite (virage / drift)
    Crash = 3, // collision / sortie de piste
};

enum class KartColor {
    Red = 0,
    Blue,
    Yellow,
    Green,
};

// Pointeurs vers les 4 frames d'une couleur de kart (données en flash,
// aucune copie : cf. kart_loader.cpp).
struct KartSprites {
    const uint16_t* frame[4] = { nullptr, nullptr, nullptr, nullptr };
};

} // namespace core
