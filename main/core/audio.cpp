#include "audio.h"
#include "gb_audio_player.h"
#include "gb_audio_track_tone.h"
#include "gb_ll_audio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_timer.h"

namespace core {

static gb_audio_player     g_player;
static gb_audio_track_tone g_engine_tone;
static gb_audio_track_tone g_sfx_tone;
static uint32_t            s_last_engine_retrigger_ms = 0;

static inline uint32_t millis_now() {
    return (uint32_t)(esp_timer_get_time() / 1000ULL);
}

static void audio_task(void*) {
    while (true) {
        g_player.pool();
        vTaskDelay(pdMS_TO_TICKS(2));
    }
}

void audio_init() {
    gb_ll_audio_set_volume(200);
    g_player.add_track(&g_engine_tone);
    g_player.add_track(&g_sfx_tone);
    g_engine_tone.set_track_volume(0.5f);
    g_sfx_tone.set_track_volume(0.8f);

    xTaskCreatePinnedToCore(audio_task, "AudioTask", 4096, nullptr, 4, nullptr, 1);
}

void audio_update_engine(float speed_ratio) {
    uint32_t now = millis_now();
    if (now - s_last_engine_retrigger_ms < 120) return;
    s_last_engine_retrigger_ms = now;

    if (speed_ratio < 0.02f) return; // moteur au ralenti : pas de son

    float base = 90.0f + speed_ratio * 260.0f; // grave -> aigu selon la vitesse
    g_engine_tone.play_tone(base, base * 1.05f, 0.4f, 0.4f, 140, gb_audio_track_tone::SQUARE);
}

void sfx_jump()  { g_sfx_tone.play_tone(400, 700, 0.8f, 0.8f, 120, gb_audio_track_tone::TRIANGLE); }
void sfx_boost() { g_sfx_tone.play_tone(300, 900, 0.9f, 0.9f, 250, gb_audio_track_tone::SQUARE);   }
void sfx_lap()   { g_sfx_tone.play_tone(500, 1000, 0.8f, 0.8f, 300, gb_audio_track_tone::TRIANGLE);}
void sfx_bump()  { g_sfx_tone.play_tone(150, 80, 0.7f, 0.7f, 100, gb_audio_track_tone::NOISE);     }

} // namespace core
