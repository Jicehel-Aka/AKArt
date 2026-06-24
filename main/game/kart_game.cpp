#include "kart_game.h"
#include "kart_types.h"
#include "kart_engine.h"
#include "kart_render.h"
#include "track_example.h"
#include "../core/input.h"
#include "../core/audio.h"

namespace kart {

static Track track;
static std::vector<KartState> karts;
static Camera cam;

void kart_game_init() {
    track = make_test_track();

    cam.height = 40.0f;
    cam.fov = 120.0f;
    cam.offset_x = 0.0f;
    cam.cockpit_view = false;
    cam.shake = 1.0f;

    // Joueur
    KartState p{};
    p.type = KartType::Player;
    p.x = 0.0f;
    p.z = 0.0f;
    p.y = 0.0f;
    p.vy = 0.0f;
    p.speed = 0.0f;
    p.seg_index = 0;
    p.lap = 0;
    p.rank = 1;
    p.drift = 0.0f;
    p.drifting = false;
    p.on_ground = true;
    p.bonus = BonusType::Boost; // pour test
    p.has_boost = false;
    p.boost_timer = 0.0f;
    p.radius = 0.15f;
    karts.push_back(p);

    // IA
    for (int i = 0; i < 5; ++i) {
        KartState ai{};
        ai.type = KartType::AI;
        ai.x = (i - 2) * 0.2f;
        ai.z = track.total_length - (i + 1) * 20.0f; // décalées derrière le joueur, dans [0, total_length)
        if (ai.z < 0.0f) ai.z += track.total_length;
        ai.y = 0.0f;
        ai.vy = 0.0f;
        ai.speed = 0.0f;
        ai.seg_index = 0;
        ai.lap = 0;
        ai.rank = i + 2;
        ai.drift = 0.0f;
        ai.drifting = false;
        ai.on_ground = true;
        ai.bonus = BonusType::None;
        ai.has_boost = false;
        ai.boost_timer = 0.0f;
        ai.radius = 0.15f;
        karts.push_back(ai);
    }
}

void kart_game_update(float dt) {
    using namespace core;

    if (input_was_pressed(Button::R1))
        cam.cockpit_view = !cam.cockpit_view;

    update_all(karts, track, dt);

    // La caméra suit la position latérale du joueur : sans ça, diriger le
    // kart ne produisait aucun retour visuel sur la route (elle restait
    // toujours centrée à l'écran quoi qu'il arrive).
    cam.offset_x = karts[0].x;

    // A garder en sync avec kart_engine.cpp
    static const float MAX_SPEED = 1.25f;
    audio_update_engine(karts[0].speed / MAX_SPEED);

    // Vibration simple
    if (karts[0].drifting || karts[0].has_boost)
        cam.shake = 3.0f;
    else
        cam.shake = 1.0f;
}

void kart_game_draw() {
    draw_race(track, karts, cam);
}

} // namespace kart
