/*
===============================================================================
  sprite_viewer.h — Visualisation des sprites (AKArt)
-------------------------------------------------------------------------------
  Rôle :
    - Permet de parcourir tous les sprites du jeu (kart, décor, HUD, etc.)
    - Navigation :
         * Joystick gauche/droite → sprite précédent / suivant
         * A → zoomer / dézoomer
         * B → retour au jeu (Mode::Race)
===============================================================================
*/

#pragma once

namespace kart {

void sprite_viewer_init();
void sprite_viewer_update();
void sprite_viewer_draw();

} // namespace kart
