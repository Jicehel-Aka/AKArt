/*
===============================================================================
  pause_menu.h — Menu Pause d'Akart
-------------------------------------------------------------------------------
  Rôle :
    - Fournir deux fonctions :
         * pause_menu_update() → logique du menu pause
         * pause_menu_draw()   → rendu du menu pause
    - Le menu pause est activé depuis kart_game.cpp (Mode::Pause).
    - Les touches utilisées :
         B → reprendre la course
         C → retour à l'écran titre
         (RUN+MENU → loader, géré dans app_main.cpp)
===============================================================================
*/

#pragma once

namespace kart {

// Logique du menu pause (navigation, actions)
void pause_menu_update();

// Rendu du menu pause (overlay sur la course)
void pause_menu_draw();

} // namespace kart
