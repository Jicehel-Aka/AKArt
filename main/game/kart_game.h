#pragma once

namespace kart {

// -----------------------------------------------------------------------------
// Modes du jeu — déclarés ici (et non dans kart_game.cpp) car partagés avec
// pause_menu.cpp et sprite_viewer.cpp, qui ont besoin de lire/écrire `mode`.
// -----------------------------------------------------------------------------
enum class Mode { Title, Race, Pause, Results, SpriteViewer, Options };

// Variable de mode courante, définie (non "static") dans kart_game.cpp afin
// d'être accessible depuis les autres modules via cette déclaration extern.
extern Mode mode;

void kart_game_init();
void kart_game_update(float dt);
void kart_game_draw();

// Abandonne la coupe en cours (retour à la 1ère course la prochaine fois) —
// appelé quand le joueur quitte manuellement vers le titre (cf. pause_menu.cpp).
void kart_abandon_cup();

} // namespace kart
