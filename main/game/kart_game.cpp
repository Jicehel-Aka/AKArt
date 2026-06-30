/*
===============================================================================
  kart_game.cpp — Boucle de jeu principale (AKArt)
===============================================================================
*/

#include "kart_game.h"
#include "kart_types.h"
#include "kart_engine.h"
#include "kart_render.h"
#include "title_screen.h"
#include "track_example.h"
#include "../core/input.h"
#include "../core/audio.h"
#include "../core/graphics.h"
#include <cstdio>

namespace kart {

enum class Mode { Title, Race, Results };

static Mode  mode        = Mode::Title;
static float title_timer = 0.0f;
static float results_timer = 0.0f;

static Track              track;
static std::vector<KartState> karts;
static Camera             cam;

// Constante locale (valeur de référence pour le son moteur)
static const float MAX_SPEED_AUDIO = 2.0f;

// -----------------------------------------------------------------------------
// Initialisation
// -----------------------------------------------------------------------------
void kart_game_init() {
    track = make_test_track();

    cam.height       = 40.0f;
    cam.fov          = 120.0f;
    cam.offset_x     = 0.0f;
    cam.cockpit_view = false;
    cam.shake        = 1.0f;
    cam.angle        = 0.0f;
    camera_setup(cam);

    KartState p{};
    p.type       = KartType::Player;
    p.x          = 0.0f;
    p.z          = 0.0f;
    p.y          = 0.0f;
    p.vy         = 0.0f;
    p.speed      = 0.0f;
    p.seg_index  = 0;
    p.lap        = 0;
    p.rank       = 1;
    p.drift      = 0.0f;
    p.drifting   = false;
    p.on_ground  = true;
    p.bonus      = BonusType::Boost;
    p.has_boost  = false;
    p.boost_timer= 0.0f;
    p.radius     = 0.15f;
    p.angle      = 0.0f;
    p.finished   = false;
    p.score      = 0;
    karts.push_back(p);

    for (int i = 0; i < 3; ++i) {
        KartState ai{};
        ai.type       = KartType::AI;
        ai.x          = (i - 1) * 0.3f;
        ai.z          = track.total_length - (i + 1) * 20.0f;
        if (ai.z < 0.0f) ai.z += track.total_length;
        ai.y          = 0.0f;
        ai.vy         = 0.0f;
        ai.speed      = 0.0f;
        ai.seg_index  = 0;
        ai.lap        = 0;
        ai.rank       = i + 2;
        ai.drift      = 0.0f;
        ai.drifting   = false;
        ai.on_ground  = true;
        ai.bonus      = BonusType::None;
        ai.has_boost  = false;
        ai.boost_timer= 0.0f;
        ai.radius     = 0.15f;
        ai.angle      = 0.0f;
        ai.finished   = false;
        ai.score      = 0;
        karts.push_back(ai);
    }
}

// -----------------------------------------------------------------------------
// Mise à jour
// -----------------------------------------------------------------------------
void kart_game_update(float dt) {
    using namespace core;

    // --- Écran titre ---
    if (mode == Mode::Title) {
        title_timer += dt;

        static float a_held_time = 0.0f;
        if (input_is_held(Button::A)) a_held_time += dt;
        else a_held_time = 0.0f;

        if (input_was_pressed(Button::A) || a_held_time >= 0.08f)
            mode = Mode::Race;
        return;
    }

    // --- Écran résultats ---
    if (mode == Mode::Results) {
        results_timer += dt;
        // Retour au titre après 8s ou sur appui A
        if (results_timer >= 8.0f || input_was_pressed(Button::A))
            mode = Mode::Title;
        return;
    }

    // --- Course ---
    if (input_was_pressed(Button::R1))
        cam.cockpit_view = !cam.cockpit_view;

    update_all(karts, track, dt);

    // Passage aux résultats dès que le joueur a terminé
    if (karts[0].finished) {
        mode = Mode::Results;
        results_timer = 0.0f;
        return;
    }

    cam.angle = karts[0].angle;
    audio_update_engine(karts[0].speed / MAX_SPEED_AUDIO);

    if (karts[0].drifting || karts[0].has_boost)
        cam.shake = 3.0f;
    else
        cam.shake = 1.0f;
}

// -----------------------------------------------------------------------------
// Rendu écran résultats
// -----------------------------------------------------------------------------
static void draw_results() {
    using namespace core;

    graphics_clear(Color::DarkBlue);
    graphics_draw_text_center(30, "FIN DE COURSE !", Color::Yellow);

    const KartState& p = karts[0];
    char buf[48];
    snprintf(buf, sizeof(buf), "Votre rang : %d / %d", p.rank, (int)karts.size());
    graphics_draw_text_center(60, buf, Color::White);

    snprintf(buf, sizeof(buf), "Points gagnes : %d", p.score);
    graphics_draw_text_center(80, buf, Color::White);

    // Tableau des points par rang
    graphics_draw_text_center(110, "Bareme :", Color::LightGray);
    const char* medals[] = { "1er :  10 pts", "2eme:   6 pts", "3eme:   3 pts", "4eme+:  1 pt " };
    for (int i = 0; i < 4; ++i) {
        Color c = (p.rank == i + 1) ? Color::Yellow : Color::Gray;
        graphics_draw_text_center(130 + i * 16, medals[i], c);
    }

    bool blink = (int)(results_timer * 2.0f) % 2 == 0;
    if (blink)
        graphics_draw_text_center(210, "A : retour au menu", Color::Green);
}

// -----------------------------------------------------------------------------
// Dessin
// -----------------------------------------------------------------------------
void kart_game_draw() {
    if (mode == Mode::Title) {
        title_screen_draw(title_timer);
        return;
    }
    if (mode == Mode::Results) {
        draw_results();
        return;
    }
    draw_race(track, karts, cam);
}

} // namespace kart
