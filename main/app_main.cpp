/*
===============================================================================
  app_main.cpp — Point d'entrée d'Akart
-------------------------------------------------------------------------------
  Rôle :
    - Initialiser le moteur AKA (g_core.init()).
    - Lancer la tâche principale du jeu de kart.
    - Boucle idle :
        * g_core.pool() pour garder le hardware à jour.
        * combo RUN + MENU pour retourner au loader.
===============================================================================
*/

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "gb_core.h"
#include "tasks/task_kart.h"

#include <esp_ota_ops.h>
#include <esp_partition.h>

gb_core g_core;

// -----------------------------------------------------------------------------
// Combo RUN + MENU → Retour au loader
// -----------------------------------------------------------------------------
static uint32_t combo_start = 0;

static void check_return_to_loader()
{
    bool run_held  = g_core.buttons.state() & GB_KEY_RUN;
    bool menu_held = g_core.buttons.state() & GB_KEY_MENU;

    if (run_held && menu_held)
    {
        if (combo_start == 0)
            combo_start = g_core.get_millis();
        else if (g_core.get_millis() - combo_start >= 500)
        {
            combo_start = 0;

            const esp_partition_t* loader =
                esp_partition_find_first(
                    ESP_PARTITION_TYPE_APP,
                    ESP_PARTITION_SUBTYPE_APP_OTA_1,
                    nullptr
                );

            if (loader)
            {
                esp_ota_set_boot_partition(loader);
                esp_restart();
            }
        }
    }
    else
    {
        combo_start = 0;
    }
}

// -----------------------------------------------------------------------------
// app_main()
// -----------------------------------------------------------------------------
extern "C" void app_main(void) {
    printf("\n=============================================\n");
    printf("  Akart - Jeu de Kart pour Gamebuino AKA\n");
    printf("=============================================\n\n");

    // Initialisation complète du matériel (LCD, audio, SD, input, timers…)
    g_core.init();

    // Lancement de la tâche principale du jeu
    xTaskCreatePinnedToCore(
        kart::task_kart,
        "KartTask",
        8192,
        nullptr,
        6,
        nullptr,
        1
    );

    printf("[Akart] Tâche lancée. Entrée en idle loop.\n");

    // Boucle idle : mise à jour du hardware + retour loader
    while (true)
    {
        g_core.pool();              // boutons + joystick + audio + timers
        check_return_to_loader();   // RUN + MENU
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
