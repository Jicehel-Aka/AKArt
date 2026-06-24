//===============================================================================
// app_main.cpp — Point d'entrée d'Akart
//===============================================================================
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "gb_core.h"
#include "tasks/task_kart.h"

gb_core g_core;

extern "C" void app_main(void) {
    printf("\n=============================================\n");
    printf("  Akart - Jeu de Kart pour Gamebuino AKA\n");
    printf("=============================================\n\n");

    g_core.init();

    xTaskCreatePinnedToCore(
        kart::task_kart,
        "KartTask",
        8192,
        nullptr,
        6,
        nullptr,
        1
    );

    printf("[Akart] Tache lancee. Entree en idle loop.\n");

    while (true)
        vTaskDelay(pdMS_TO_TICKS(1000));
}
