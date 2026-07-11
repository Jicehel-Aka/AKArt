#include "audio.h"
#include "gb_audio_player.h"
#include "gb_audio_track_tone.h"
#include "gb_audio_track_pmf.h"
#include "gb_ll_audio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_timer.h"

namespace core {

static gb_audio_player     g_player;
static gb_audio_track_tone g_engine_tone;
static gb_audio_track_tone g_sfx_tone;
static gb_audio_track_pmf  g_music_track;
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
    g_player.add_track(&g_music_track);
    g_engine_tone.set_track_volume(0.5f);
    g_sfx_tone.set_track_volume(0.8f);
    g_music_track.set_track_volume(0.6f);

    xTaskCreatePinnedToCore(audio_task, "AudioTask", 4096, nullptr, 5, nullptr, 0);
}

void audio_set_master_volume(uint8_t volume) {
    gb_ll_audio_set_volume(volume);
}

// -----------------------------------------------------------------------------
// Musique de fond (.pmf)
// -----------------------------------------------------------------------------
// IMPORTANT : il n'y a ici AUCUNE donnée .pmf réelle fournie — cette API ne
// fait que brancher le lecteur PMF déjà présent dans la lib Gamebuino
// (gb_audio_track_pmf / pmf_player, vus dans components/gamebuino/gb_lib).
// Il faut composer ou obtenir de vrais fichiers .pmf (format tracker, ex.
// via un outil compatible ProTracker/Impulse Tracker exportant en .pmf, ou
// un convertisseur .mod/.xm -> .pmf s'il existe pour cette lib), puis les
// convertir en tableau d'octets .h/.cpp exactement comme les sprites (cf.
// assets_data/*.h) pour pouvoir les passer ici.
void audio_play_music(const void* pmf_data, unsigned int pmf_len) {
    if (!pmf_data || pmf_len == 0) return; // pas de vraies données -> ne joue rien
    g_music_track.play_pmf(pmf_data);
}

void audio_stop_music() {
    g_music_track.stop_playing();
}

void audio_set_music_volume(float volume) {
    g_music_track.set_track_volume(volume);
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
void sfx_bump()  { g_sfx_tone.play_tone(150, 80,  0.7f, 0.7f, 100, gb_audio_track_tone::NOISE);    }

// Crash violent : deux tonalités descendantes en cascade + bruit
void sfx_crash_hard() {
    g_sfx_tone.play_tone(350, 60, 0.9f, 0.9f, 180, gb_audio_track_tone::NOISE);
}

// Hors-piste : bruit plus long et plus grave que le simple bump (herbe/gravier)
void sfx_offtrack() {
    g_sfx_tone.play_tone(90, 60, 0.5f, 0.5f, 220, gb_audio_track_tone::NOISE);
}

// Démarrage : sweep montant bref pour marquer l'accélération de 0
void sfx_accel_start() {
    g_sfx_tone.play_tone(80, 320, 0.7f, 0.7f, 200, gb_audio_track_tone::SQUARE);
}

// Ramassage bonus box : petit "ding" montant, clair et joyeux
void sfx_item_pickup() {
    g_sfx_tone.play_tone(500, 1200, 0.7f, 0.6f, 140, gb_audio_track_tone::TRIANGLE);
}

// Bouclier activé : montée douce et brillante (distincte du boost, plus aiguë)
void sfx_shield_up() {
    g_sfx_tone.play_tone(600, 1400, 0.6f, 0.5f, 300, gb_audio_track_tone::TRIANGLE);
}

// Bouclier qui absorbe un impact : "toc" bref et sourd
void sfx_shield_block() {
    g_sfx_tone.play_tone(220, 180, 0.6f, 0.5f, 90, gb_audio_track_tone::SQUARE);
}

// Onde de choc envoyée : impulsion grave qui explose vers l'aigu
void sfx_shock_use() {
    g_sfx_tone.play_tone(120, 500, 0.9f, 0.8f, 220, gb_audio_track_tone::SQUARE);
}

// Onde de choc subie : descente rapide + bruit, pour bien marquer "aïe"
void sfx_shock_hit() {
    g_sfx_tone.play_tone(400, 90, 0.9f, 0.8f, 260, gb_audio_track_tone::NOISE);
}

// Dérapage sur flaque d'huile : glissement bruité, distinct du bump (plus long, plus grave)
void sfx_oil_spinout() {
    g_sfx_tone.play_tone(140, 40, 0.8f, 0.6f, 400, gb_audio_track_tone::NOISE);
}

// Palier de mini-turbo atteint : "ding" bref, monte en hauteur selon le palier
// appelé (tier 1/2/3) pour donner un retour clair sans avoir à regarder le HUD
void sfx_drift_tier_up() {
    g_sfx_tone.play_tone(700, 1000, 0.6f, 0.5f, 100, gb_audio_track_tone::SQUARE);
}

// Compte à rebours : bip net et court à chaque chiffre
void sfx_countdown_beep() {
    g_sfx_tone.play_tone(500, 500, 0.8f, 0.6f, 150, gb_audio_track_tone::SQUARE);
}

// GO ! : plus aigu et plus long que les bips, pour bien marquer le départ
void sfx_countdown_go() {
    g_sfx_tone.play_tone(700, 1100, 0.9f, 0.8f, 280, gb_audio_track_tone::SQUARE);
}

} // namespace core
