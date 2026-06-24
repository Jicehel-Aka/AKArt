/*
===============================================================================
  task_kart.cpp — Boucle de jeu principale d'Akart (~40 FPS)
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

    core::graphics_init();
    core::input_init();
    core::audio_init();

    kart_game_init();

    const TickType_t frame_ticks = pdMS_TO_TICKS(25); // ~40 FPS
    TickType_t last_wake = xTaskGetTickCount();

    while (true) {
        core::input_poll();
        kart_game_update(0.025f);
        kart_game_draw();
        core::graphics_present();
        vTaskDelayUntil(&last_wake, frame_ticks);
    }
}

} // namespace kart
