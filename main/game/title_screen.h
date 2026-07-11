/*
===============================================================================
  title_screen.h — Écran de titre d'Akart
-------------------------------------------------------------------------------
  Rôle :
    - Déclarer la fonction title_screen_draw(), chargée d'afficher l'écran
      de titre (image AKArt + effets visuels éventuels).
    - blink_time est un temps écoulé (en secondes) fourni par kart_game_update().
      Il permet d’animer :
         * un fade‑in progressif
         * un clignotement éventuel du texte “Appuyez sur A”
    - IMPORTANT :
        Ce module NE gère PAS l’appui sur A.
        Le changement d’état (Title → Race) est géré dans kart_game_update().
===============================================================================
*/

#pragma once

namespace kart {

// -----------------------------------------------------------------------------
// Dessine l'écran de titre.
// blink_time  : temps écoulé depuis l'entrée dans l'écran titre (en secondes).
// track_name  : nom de la piste actuellement sélectionnée (Joystick G/D pour
//               changer, cf. track_registry.h / kart_game.cpp) — affiché en
//               bas de l'écran.
// -----------------------------------------------------------------------------
void title_screen_draw(float blink_time, const char* track_name);

} // namespace kart
