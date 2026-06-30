/*
===============================================================================
  task_kart.h — Tâche FreeRTOS principale du jeu Akart
-------------------------------------------------------------------------------
  Rôle :
    - Déclarer la fonction task_kart(), exécutée dans un thread dédié.
    - Cette tâche gère :
         * la boucle de jeu (40 FPS)
         * la mise à jour des entrées
         * la logique du jeu (kart_game_update)
         * le rendu pseudo‑3D (kart_game_draw)
         * l’envoi du framebuffer à l’écran (graphics_present)
    - La création de cette tâche est effectuée dans app_main.cpp.
===============================================================================
*/

#pragma once

namespace kart {

// -----------------------------------------------------------------------------
// task_kart() — Boucle de jeu principale (~40 FPS)
// -----------------------------------------------------------------------------
// Appelée par app_main.cpp via xTaskCreatePinnedToCore().
// Ne retourne jamais.
// -----------------------------------------------------------------------------
void task_kart(void* arg);

} // namespace kart
