#include "kart_engine.h"
#include "../core/input.h"
#include "../core/audio.h"
#include <algorithm>
#include <cmath>

namespace kart {

static const float MAX_SPEED       = 1.25f;
static const float ACCEL           = 0.025f;
static const float BRAKE           = 0.05f;
static const float FRICTION        = 0.015f;
static const float TURN            = 0.035f;
static const float DRIFT_TURN      = 0.07f;
static const float DRIFT_BUILD     = 0.025f;
static const float DRIFT_DECAY     = 0.04f;
static const float BOOST_SPEED     = 1.6f;
static const float BOOST_TIME      = 0.8f;
static const float OFFROAD_PENALTY = 0.45f;
static const float GRAVITY         = 0.03f;

// ---------------------------------------------------------
// Trouve le segment courant
// ---------------------------------------------------------
static int find_segment(const Track& t, float z) {
    float acc = 0.0f;
    for (int i = 0; i < (int)t.segs.size(); ++i) {
        acc += t.segs[i].length;
        if (z < acc) return i;
    }
    return 0;
}

// ---------------------------------------------------------
// Collision avec le décor de bord de piste
// ---------------------------------------------------------
// Convention (cf. Segment::decor_left/right) : 0 = rien, >0 = objet solide
// (arbre, rocher...). Si le kart est proche du bord ET qu'un décor solide
// y est présent, on traite ça comme un choc franc (plus punitif que le
// simple hors-piste) : forte perte de vitesse + repoussé vers la piste.
static const float DECOR_EDGE_THRESHOLD = 0.85f;
static const float DECOR_HIT_SPEED_MULT = 0.35f;

static void handle_decor_collision(KartState& k, const Segment& s) {
    bool hit_left  = (k.x < -DECOR_EDGE_THRESHOLD) && (s.decor_left  != 0);
    bool hit_right = (k.x >  DECOR_EDGE_THRESHOLD) && (s.decor_right != 0);

    if (hit_left || hit_right) {
        k.speed *= DECOR_HIT_SPEED_MULT;
        k.x = hit_left ? -DECOR_EDGE_THRESHOLD : DECOR_EDGE_THRESHOLD;
        core::sfx_bump();
    }
}

// ---------------------------------------------------------
// Collisions entre karts
// ---------------------------------------------------------
static void handle_kart_collisions(std::vector<KartState>& karts) {
    for (size_t i = 0; i < karts.size(); ++i) {
        for (size_t j = i + 1; j < karts.size(); ++j) {

            KartState& A = karts[i];
            KartState& B = karts[j];

            float dz = std::abs(A.z - B.z);
            if (dz > 6.0f) continue;

            float dx = A.x - B.x;
            float dist = std::abs(dx);
            float min_dist = A.radius + B.radius;

            if (dist < min_dist) {
                float push = (min_dist - dist) * 0.5f;
                core::sfx_bump();
                if (A.speed > B.speed) {
                    A.x += (dx >= 0 ? push : -push);
                    B.x -= (dx >= 0 ? push : -push);
                    B.speed *= 0.9f;
                } else {
                    A.x -= (dx >= 0 ? push : -push);
                    B.x += (dx >= 0 ? push : -push);
                    A.speed *= 0.9f;
                }
            }
        }
    }
}

// ---------------------------------------------------------
// update_player()
// ---------------------------------------------------------
void update_player(KartState& k, const Track& t, float dt) {
    using namespace core;

    bool left      = input_is_held(Button::JoystickLeft);
    bool right     = input_is_held(Button::JoystickRight);
    bool accel     = input_is_held(Button::A);
    bool brake     = input_is_held(Button::B);
    bool driftB    = input_is_held(Button::C);
    bool jump      = input_is_held(Button::D);
    bool use_bonus = input_was_pressed(Button::L1);

    // Acceleration / frein
    if (accel) k.speed += ACCEL;
    if (brake) k.speed -= BRAKE;

    if (!accel && !brake) {
        if (k.speed > 0) k.speed -= FRICTION;
        else if (k.speed < 0) k.speed += FRICTION;
    }

    k.speed = std::clamp(k.speed, 0.0f, MAX_SPEED);

    // Drift
    if (driftB && (left || right) && k.speed > 0.3f) {
        k.drifting = true;
        k.drift = std::min(1.0f, k.drift + DRIFT_BUILD);
    } else {
        k.drifting = false;
        k.drift = std::max(0.0f, k.drift - DRIFT_DECAY);
    }

    float turn = 0.0f;
    if (left)  turn -= 1.0f;
    if (right) turn += 1.0f;

    if (k.drifting)
        k.x += turn * DRIFT_TURN * (0.5f + k.speed);
    else
        k.x += turn * TURN * (0.5f + k.speed);

    k.x = std::clamp(k.x, -1.2f, 1.2f);

    // Bonus
    if (use_bonus && k.bonus == BonusType::Boost) {
        k.has_boost = true;
        k.boost_timer = BOOST_TIME;
        k.bonus = BonusType::None;
        core::sfx_boost();
    }

    // Boost
    if (k.has_boost) {
        k.boost_timer -= dt;
        if (k.boost_timer <= 0.0f) {
            k.has_boost = false;
        } else {
            k.speed = std::max(k.speed, BOOST_SPEED);
        }
    }

    // Avance
    k.z += k.speed * dt * 60.0f;
    if (k.z >= t.total_length) {
        k.z -= t.total_length;
        k.lap++;
        core::sfx_lap();
    }

    k.seg_index = find_segment(t, k.z);
    const Segment& s = t.segs[k.seg_index];

    // Tremplin
    if (s.jump_pad && k.on_ground) {
        k.vy = 0.45f;
        k.on_ground = false;
        core::sfx_jump();
    }

    // Saut manuel
    if (jump && k.on_ground) {
        k.vy = 0.35f;
        k.on_ground = false;
        core::sfx_jump();
    }

    // Physique verticale
    if (!k.on_ground) {
        k.vy -= GRAVITY;
        k.y += k.vy;

        if (k.y <= 0.0f) {
            k.y = 0.0f;
            k.vy = 0.0f;
            k.on_ground = true;
            k.speed = std::min(MAX_SPEED, k.speed + 0.15f);
        }
    }

    // Offroad
    if (std::abs(k.x) > 1.0f)
        k.speed *= OFFROAD_PENALTY;

    handle_decor_collision(k, s);

    k.angle = s.curve * 0.5f;
}

// ---------------------------------------------------------
// update_ai()
// ---------------------------------------------------------
void update_ai(KartState& k, const Track& t, float dt) {
    const Segment& s = t.segs[k.seg_index];

    float target_x = 0.0f;
    float diff = target_x - k.x;
    k.x += diff * 0.03f;

    if (k.speed < MAX_SPEED * 0.9f)
        k.speed += ACCEL * 0.8f;

    // Avance
    k.z += k.speed * dt * 60.0f;
    if (k.z >= t.total_length) {
        k.z -= t.total_length;
        k.lap++;
    }

    k.seg_index = find_segment(t, k.z);

    // Offroad
    if (std::abs(k.x) > 1.0f)
        k.speed *= OFFROAD_PENALTY;

    handle_decor_collision(k, s);

    k.angle = s.curve * 0.5f;
}

// ---------------------------------------------------------
// update_all()
// ---------------------------------------------------------
void update_all(std::vector<KartState>& karts,
                const Track& t, float dt)
{
    // Joueur
    update_player(karts[0], t, dt);

    // IA
    for (size_t i = 1; i < karts.size(); ++i)
        update_ai(karts[i], t, dt);

    // Collisions
    handle_kart_collisions(karts);

    // Classement
    std::vector<size_t> order(karts.size());
    for (size_t i = 0; i < karts.size(); ++i) order[i] = i;

    std::sort(order.begin(), order.end(),
        [&](size_t a, size_t b) {
            if (karts[a].lap != karts[b].lap)
                return karts[a].lap > karts[b].lap;
            return karts[a].z > karts[b].z;
        });

    for (size_t r = 0; r < order.size(); ++r)
        karts[order[r]].rank = (int)r + 1;
}

} // namespace kart
