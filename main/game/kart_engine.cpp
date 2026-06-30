/*
===============================================================================
  kart_engine.cpp — Moteur de physique et de contrôle des karts (AKArt)
-------------------------------------------------------------------------------
  Rôle :
    - Gérer la vitesse, l’accélération, le freinage et la friction.
    - Gérer le drift, les boosts, les sauts et la gravité.
    - Gérer les collisions (bord de piste, décor, autres karts).
    - Fournir un angle de vue (k.angle) qui sert à la caméra :
        * la courbure de la piste (s.curve)
        * + le steering du joueur (gauche/droite)
      → la vue tourne quand le joueur tourne, au lieu de simplement glisser
        le kart à gauche/droite.
===============================================================================
*/

#include "kart_engine.h"
#include "../core/input.h"
#include "../core/audio.h"
#include <algorithm>
#include <cmath>

namespace kart {

static const float MAX_SPEED       = 2.0f;   // plus rapide (était 1.25)
static const float ACCEL           = 0.04f;   // accélération plus franche (était 0.025)
static const float BRAKE           = 0.08f;   // freinage légèrement plus fort
static const float FRICTION        = 0.02f;   // décélération passive (était 0.015)
static const float BOOST_SPEED     = 2.6f;
static const float BOOST_TIME      = 0.8f;
static const float OFFROAD_PENALTY = 0.45f;
static const float GRAVITY         = 0.03f;

// -----------------------------------------------------------------------------
// Direction : fidèle au moteur de référence (cf. javascript-racer de Jake
// Gordon). Le déplacement latéral lié au volant est PROPORTIONNEL à la
// vitesse courante (speedPercent = speed/MAX_SPEED) : à l'arrêt, tourner le
// volant ne déplace pas le kart d'un pixel — exactement comme dans une
// vraie voiture. À cela s'ajoute une force centrifuge AUTOMATIQUE (sans
// appui d'aucune touche) qui pousse vers l'extérieur du virage, elle aussi
// proportionnelle à la vitesse : c'est un effet voulu (pas un bug) qui
// donne l'impression de devoir "tenir" le virage en contre-braquant.
// -----------------------------------------------------------------------------
static const float STEER_RATE   = 3.2f;  // sensibilité du volant
static const float CENTRIFUGAL  = 0.3f;  // force qui pousse vers l'extérieur du virage
static const float DRIFT_BOOST_TURN = 1.6f; // multiplicateur de braquage en drift

// Hors-piste prolongé : au-delà de ce délai, le kart est replacé sur la
// piste (au centre de la voie) avec une forte perte de vitesse, plutôt que
// de laisser le joueur s'éloigner indéfiniment (ce qui finissait par ne
// plus rien afficher d'utile à l'écran, la caméra étant alors centrée sur
// une zone hors de la route).
static const float OFFTRACK_RESET_DELAY = 1.5f;  // secondes
static const float OFFTRACK_RESET_SPEED_MULT = 0.25f;

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
// Pénalité hors-piste : ralentissement immédiat, puis replacement forcé
// sur la piste si on y reste trop longtemps.
// ---------------------------------------------------------
static void handle_offtrack(KartState& k, float dt) {
    bool offtrack = std::abs(k.x) > 1.0f;

    if (!offtrack) {
        k.off_track_timer = 0.0f;
        return;
    }

    k.speed *= OFFROAD_PENALTY;
    k.off_track_timer += dt;

    if (k.off_track_timer >= OFFTRACK_RESET_DELAY) {
        k.x = 0.0f; // replacé au centre de la piste
        k.speed *= OFFTRACK_RESET_SPEED_MULT;
        k.off_track_timer = 0.0f;
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
//  - Lit les inputs.
//  - Met à jour vitesse / drift / bonus / saut.
//  - Met à jour la position longitudinale (z).
//  - Met à jour la position latérale (x) pour les collisions / hors-piste.
//  - Met à jour l’angle de vue (k.angle) : courbe de piste + steering joueur.
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
    float speed_percent = k.speed / MAX_SPEED;

    // Drift (l'intensité 0..1 sert seulement à l'inclinaison visuelle du
    // sprite ; la vitesse de "build/decay" est volontairement indépendante
    // de la vitesse du kart — c'est un ressenti purement visuel)
    if (driftB && (left || right) && k.speed > 0.3f) {
        k.drifting = true;
        k.drift = std::min(1.0f, k.drift + dt * 4.0f);
    } else {
        k.drifting = false;
        k.drift = std::max(0.0f, k.drift - dt * 6.0f);
    }

    // Steering brut (input gauche/droite)
    float turn = 0.0f;
    if (left)  turn -= 1.0f;
    if (right) turn += 1.0f;

    const Segment& cur_seg = t.segs[k.seg_index];

    // Déplacement latéral PROPORTIONNEL à la vitesse (référence OutRun) :
    // à l'arrêt, speed_percent=0, donc tourner le volant ne déplace rien.
    float dx = dt * STEER_RATE * speed_percent;
    if (k.drifting) dx *= DRIFT_BOOST_TURN;
    k.x += turn * dx;

    // Force centrifuge automatique (sans appui d'aucune touche), elle aussi
    // proportionnelle à la vitesse : pousse vers l'extérieur du virage.
    // C'est un effet voulu (cf. javascript-racer de référence), pas un bug.
    k.x -= dx * speed_percent * cur_seg.curve * CENTRIFUGAL;

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

    // Avance longitudinale
    if (!k.finished) {
        k.z += k.speed * dt * 60.0f;
        if (k.z >= t.total_length) {
            k.z -= t.total_length;
            k.lap++;
            core::sfx_lap();
            if (k.lap >= t.laps) {
                k.finished = true;
                k.speed = 0.0f;
                // Points selon la place finale (rank commence à 1)
                int rank_idx = k.rank - 1;
                if (rank_idx < 0) rank_idx = 0;
                if (rank_idx > 3) rank_idx = 3;
                k.score += t.points_by_rank[rank_idx];
            }
        }
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

    // Hors-piste : pénalité + replacement forcé après un délai
    handle_offtrack(k, dt);

    handle_decor_collision(k, s);

    // -----------------------------------------------------
    // Angle cosmétique (k.angle) — utilisé pour le volant/HUD uniquement,
    // pas pour la route (cf. kart_render.cpp : c'est la route qui se
    // décale, jamais une vraie rotation de caméra).
    // -----------------------------------------------------
    k.angle = s.curve * 0.5f + turn * dx * 4.0f;
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
    if (!k.finished) {
        k.z += k.speed * dt * 60.0f;
        if (k.z >= t.total_length) {
            k.z -= t.total_length;
            k.lap++;
            if (k.lap >= t.laps) {
                k.finished = true;
                k.speed = 0.0f;
                int rank_idx = k.rank - 1;
                if (rank_idx < 0) rank_idx = 0;
                if (rank_idx > 3) rank_idx = 3;
                k.score += t.points_by_rank[rank_idx];
            }
        }
    }

    k.seg_index = find_segment(t, k.z);

    // Hors-piste : pénalité + replacement forcé après un délai
    handle_offtrack(k, dt);

    handle_decor_collision(k, s);

    // Pour l’IA, on garde un angle basé uniquement sur la courbure de la piste.
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
