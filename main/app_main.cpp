/*
===============================================================================
  app_main.cpp — Point d'entrée d'Akart (Gamebuino AKA)
-------------------------------------------------------------------------------
  Rôle :
    - Initialiser le moteur AKA (g_core.init()).
    - Initialiser l’audio applicatif (core::audio_init()).
    - Lancer la tâche principale du jeu (task_kart).
    - Boucle idle :
         * g_core.pool() → mise à jour hardware (boutons, joystick, audio, timers)
         * check_return_to_loader() → RUN + MENU maintenus 500 ms
    - IMPORTANT :
         Le retour loader utilise les touches hardware GB_KEY_RUN et GB_KEY_MENU,
         comme dans TAKATRIS. On ne modifie pas cette logique.
===============================================================================
*/

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "gb_core.h"
#include "core/input.h"
#include "core/audio.h"
#include "tasks/task_kart.h"

#include <esp_ota_ops.h>
#include <esp_partition.h>
#include <esp_system.h>

gb_core g_core;

// -----------------------------------------------------------------------------
// Combo RUN + MENU → Retour au loader (500ms maintenus)
// Copie conforme du modèle TAKATRIS (Jicehel), adaptée à Akart.
// -----------------------------------------------------------------------------
static uint32_t combo_start = 0;

static void check_return_to_loader()
{
    // Lecture directe hardware (comme TAKATRIS)
    bool run_held  = (g_core.buttons.state() & GB_KEY_RUN)  != 0;
    bool menu_held = (g_core.buttons.state() & GB_KEY_MENU) != 0;

    if (run_held && menu_held)
    {
        if (combo_start == 0)
        {
            // Début du combo
            combo_start = g_core.get_millis();
        }
        else if (g_core.get_millis() - combo_start >= 500)
        {
            // Combo validé : retour au loader
            combo_start = 0;

            const esp_partition_t* loader =
                esp_partition_find_first(
                    ESP_PARTITION_TYPE_APP,
                    ESP_PARTITION_SUBTYPE_APP_OTA_1,
                    nullptr);

            if (loader)
            {
                esp_ota_set_boot_partition(loader);
                esp_restart();
            }
        }
    }
    else
    {
        // Combo interrompu
        combo_start = 0;
    }
}

// -----------------------------------------------------------------------------
// app_main()
// -----------------------------------------------------------------------------
extern "C" void app_main(void)
{
    printf("\n=============================================\n");
    printf("  Akart - Jeu de Kart pour Gamebuino AKA\n");
    printf("=============================================\n\n");

    // -------------------------------------------------------------------------
    // Initialisation complète du hardware :
    //   - LCD (bus i80)
    //   - SD
    //   - Audio bas niveau
    //   - Expander (boutons, joystick)
    //   - Timers internes
    // -------------------------------------------------------------------------
    g_core.init();

    // -------------------------------------------------------------------------
    // Initialisation audio applicative :
    //   - Démarre le mixeur logiciel
    //   - Prépare les buffers audio
    // -------------------------------------------------------------------------
    core::audio_init();

    // -------------------------------------------------------------------------
    // Calibration du joystick : à faire une fois le hardware prêt, en supposant
    // le stick au repos (l'utilisateur ne le touche normalement pas pendant le
    // tout début du boot). Sans cet appel, input_calibrate_joystick() existait
    // mais n'était jamais invoquée nulle part — la calibration ne servait donc
    // à rien en pratique.
    // -------------------------------------------------------------------------
    core::input_calibrate_joystick();

    // -------------------------------------------------------------------------
    // Lancement de la tâche principale du jeu
    // -------------------------------------------------------------------------
    xTaskCreatePinnedToCore(
        kart::task_kart,     // fonction
        "KartTask",          // nom
        8192,                // stack
        nullptr,             // paramètre
        6,                   // priorité
        nullptr,             // handle
        1                    // CPU core
    );

    printf("[Akart] Tâche lancée. Idle loop.\n");

    // -------------------------------------------------------------------------
    // Boucle idle :
    //   - g_core.pool() → mise à jour hardware
    //   - check_return_to_loader() → RUN + MENU 500ms
    //   - petit délai pour éviter de saturer le CPU
    // -------------------------------------------------------------------------
    while (true)
    {
        g_core.pool();              // boutons, joystick, audio, timers
        check_return_to_loader();   // RUN + MENU 500ms
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
