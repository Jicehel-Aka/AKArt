/*
===============================================================================
  task_kart.cpp — Boucle de jeu principale d'Akart (~40 FPS)
-------------------------------------------------------------------------------
  Rôle :
    - Exécuter la boucle de jeu à fréquence fixe (~40 FPS).
    - Appeler :
         * input_poll()     → mise à jour des boutons
         * kart_game_update → logique du jeu
         * kart_game_draw   → rendu pseudo‑3D
         * graphics_present → envoi du framebuffer à l’écran
    - NE PAS ré‑initialiser le hardware ici :
         g_core.init() est déjà appelé dans app_main.cpp
===============================================================================
*/

#include "task_kart.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "../core/input.h"
#include "../core/graphics.h"
#include "../core/audio.h"
#include "../game/kart_game.h"

namespace kart {

void task_kart(void* arg) {
    (void)arg;

    // IMPORTANT :
    // ---------------------------------------------------------
    // NE PAS appeler :
    //   - graphics_init()
    //   - audio_init()
    //   - input_init()
    //
    // Ces fonctions sont déjà appelées par g_core.init()
    // dans app_main.cpp.
    //
    // Les rappeler ici provoquait :
    //   - scanline timeout
    //   - route qui bouge toute seule
    //   - input instable
    //   - A non détecté sur l’écran titre
    // ---------------------------------------------------------

    // Initialisation logique du jeu (piste, karts, caméra…)
    kart_game_init();

    // 40 FPS → 25 ms par frame
    const TickType_t frame_ticks = pdMS_TO_TICKS(25);
    TickType_t last_wake = xTaskGetTickCount();

    while (true) {

        // 1) Mise à jour des entrées
        //    (doit être AVANT kart_game_update)
        core::input_poll();

        // 2) Logique du jeu
        kart_game_update(0.025f);

        // 3) Rendu pseudo‑3D
        kart_game_draw();

        // 4) Envoi du framebuffer à l’écran
        core::graphics_present();

        // 5) Attendre la prochaine frame
        vTaskDelayUntil(&last_wake, frame_ticks);
    }
}

} // namespace kart
