/*
===============================================================================
  task_game.h — Tâche principale du jeu (boucle logique + rendu)
-------------------------------------------------------------------------------
  Rôle :
    - Déclarer la fonction task_game(void*), appelée comme tâche FreeRTOS.
    - Cette tâche exécute en boucle :
        * game_update() → logique du moteur BabaIsU
        * game_draw()   → rendu graphique
    - Elle gère aussi les transitions d’état (Title, Playing, Win, Dead, Menu).

  Notes :
    - La fréquence de rafraîchissement est généralement fixée à 60 FPS.
    - La tâche est lancée depuis app_main() avec xTaskCreatePinnedToCore().
    - Elle utilise game_mode() pour savoir quel écran afficher.
===============================================================================
*/

#pragma once
#include "game/game.h"        // pour GameMode, game_update(), game_draw()
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

namespace baba {

/*
===============================================================================
  Déclaration de la tâche principale du jeu
-------------------------------------------------------------------------------
  Paramètre :
    - void* param : non utilisé (peut servir pour passer un contexte futur).

  Fonctionnement :
    - Boucle infinie (while true).
    - Selon game_mode(), affiche l’écran de titre ou exécute la logique du jeu.
    - Utilise vTaskDelay() pour cadencer à ~60 FPS.
===============================================================================
*/
void task_game(void* param);

} // namespace baba
