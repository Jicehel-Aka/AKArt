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

// -----------------------------------------------------------------------------
// Échelle du monde : Segment::length est passé de 8 à 20 unités (x2.5,
// cf. track_example.h) pour que le circuit représente une distance plus
// réaliste à "200 km/h". Pour que la route défile VRAIMENT plus vite à
// l'écran (pas juste des nombres plus gros sans effet visuel — une mise à
// l'échelle uniforme de tout serait neutre), la vitesse est augmentée plus
// que le simple facteur 2.5 : x3.5 au total. Résultat net : ~1.4x plus de
// segments traversés par seconde qu'avant (donc bandes au sol qui défilent
// notablement plus vite), tout en gardant un tour de circuit 2.5x plus
// long en distance brute.
// -----------------------------------------------------------------------------
static const float MAX_SPEED       = 7.0f;
static const float ACCEL           = 0.14f;
static const float BRAKE           = 0.28f;
static const float FRICTION        = 0.07f;
static const float BOOST_SPEED     = 9.1f;
static const float BOOST_TIME      = 0.8f;
static const float OFFROAD_PENALTY = 0.45f;
static const float GRAVITY         = 0.075f; // x2.5 (garde le même temps de vol, cf. plus bas)

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
static const float CENTRIFUGAL  = 0.6f;  // force qui pousse vers l'extérieur du virage
// (remonté de 0.3 à 0.6 en même temps que CURVE_RENDER_SCALE côté rendu :
// sinon les virages auraient l'air plus serrés visuellement sans que la
// conduite ne devienne réellement plus exigeante — décalage entre ce qu'on
// voit et ce qu'on ressent à la manette, peu satisfaisant. À ajuster
// ensemble si besoin : plus CENTRIFUGAL est haut, plus il faut contre-
// braquer activement dans les virages serrés pour ne pas sortir de piste.)
static const float DRIFT_BOOST_TURN = 1.6f; // multiplicateur de braquage en drift

// -----------------------------------------------------------------------------
// Drift-boost façon "mini-turbo" (Mario Kart) : plus on maintient un drift
// qualifiant longtemps, meilleur est le palier de boost accordé au
// relâchement. Donne enfin un usage mécanique au bouton C (drift), qui
// n'affectait avant que l'inclinaison visuelle du sprite.
// -----------------------------------------------------------------------------
static const float DRIFT_TIER1_TIME = 1.0f;  // secondes de drift continu pour le palier 1
static const float DRIFT_TIER2_TIME = 2.0f;  // palier 2
static const float DRIFT_TIER3_TIME = 3.2f;  // palier 3 (le plus fort)
static const float DRIFT_BOOST_SPEED[4] = { 0.0f, 8.0f, 8.7f, 9.6f };   // par palier (0=inutilisé)
static const float DRIFT_BOOST_TIME[4]  = { 0.0f, 0.5f, 0.8f, 1.2f };

// -----------------------------------------------------------------------------
// Bonus/malus
// -----------------------------------------------------------------------------
static const float SHIELD_TIME       = 5.0f;   // durée du bouclier
static const float SHOCK_RADIUS      = 25.0f;  // portée (en z, "devant soi") de l'onde de choc
static const float SHOCK_SLOW_MULT   = 0.5f;   // ralentissement infligé aux adversaires touchés
static const float SHOCK_SLOW_TIME   = 1.0f;   // durée du ralentissement subi
static const float OIL_SPINOUT_TIME  = 1.1f;   // durée du dérapage forcé sur une flaque d'huile
static const float OIL_SPEED_MULT    = 0.4f;   // coupe de vitesse immédiate en touchant l'huile
static const float ITEM_BOX_COOLDOWN = 6.0f;   // secondes avant qu'une bonus box ne redonne un objet

// Choix aléatoire pondéré du contenu d'une bonus box. Simple LCG déterministe
// (pas de <random> : suffisant ici, pas besoin de qualité cryptographique)
// pour éviter toute dépendance supplémentaire sur la cible embarquée.
static uint32_t s_bonus_rng_state = 0x9E3779B9u;
static BonusType roll_bonus() {
    s_bonus_rng_state = s_bonus_rng_state * 1664525u + 1013904223u;
    uint32_t r = (s_bonus_rng_state >> 16) % 100;
    if (r < 50) return BonusType::Boost;   // 50%
    if (r < 80) return BonusType::Shock;   // 30%
    return BonusType::Shield;              // 20%
}

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
// Recherche binaire sur start_z (précalculé, croissant par construction :
// cf. compute_track_geometry() / les boucles d'assemblage dans
// track_example.h et track_registry.h) : O(log n) au lieu d'un parcours
// linéaire de TOUS les segments — appelé pour CHAQUE kart à CHAQUE frame,
// ça compte dès que les pistes dépassent la centaine de segments (les
// circuits dessinés dans l'éditeur en ont couramment 150-200).
int find_segment(const Track& t, float z) {
    int lo = 0, hi = (int)t.segs.size() - 1;
    while (lo < hi) {
        int mid = (lo + hi + 1) / 2;
        if (t.segs[mid].start_z <= z) lo = mid;
        else hi = mid - 1;
    }
    return lo;
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
// Flaques d'huile + bonus box : hasards/objets posés SUR la chaussée,
// partagés entre joueur et IA (contrairement au décor de bord de route, qui
// ne concerne que la collision "sortie de piste").
// ---------------------------------------------------------
static void handle_track_items(KartState& k, const Segment& s, int seg_index,
                                std::vector<float>& item_box_cooldowns) {
    // Flaque d'huile : dérapage forcé, sauf sous bouclier. Localisée autour
    // de s.oil_offset_x (pas toute la largeur de la piste) : on peut
    // l'éviter en se décalant, comme un vrai obstacle plutôt qu'un mur.
    const float OIL_HIT_RADIUS = 0.22f;
    if (s.has_oil && std::abs(k.x - s.oil_offset_x) < OIL_HIT_RADIUS && k.spinout_timer <= 0.0f) {
        if (k.has_shield) {
            // Le bouclier absorbe l'huile (petit effet gratifiant : on
            // traverse la flaque sans conséquence).
            core::sfx_shield_block();
        } else {
            k.spinout_timer = OIL_SPINOUT_TIME;
            k.speed *= OIL_SPEED_MULT;
            // Écart latéral déterministe (dépend de la position sur la
            // piste, pas d'un vrai hasard) : suffisant pour donner
            // l'impression d'un dérapage incontrôlé, et reproductible d'un
            // essai à l'autre au même endroit.
            float kick = (std::fmod(k.z, 1.0f) > 0.5f) ? 0.5f : -0.5f;
            k.x = std::clamp(k.x + kick, -1.2f, 1.2f);
            core::sfx_oil_spinout();
        }
    }

    // Bonus box : donne un objet aléatoire si on n'en tient pas déjà un et
    // que cette bonus box n'est pas en recharge.
    if (s.has_item_box && k.bonus == BonusType::None &&
        seg_index >= 0 && seg_index < (int)item_box_cooldowns.size() &&
        item_box_cooldowns[seg_index] <= 0.0f) {
        k.bonus = roll_bonus();
        item_box_cooldowns[seg_index] = ITEM_BOX_COOLDOWN;
        core::sfx_item_pickup();
    }
}

// ---------------------------------------------------------
// Pénalité hors-piste : ralentissement immédiat, puis replacement forcé
// sur la piste si on y reste trop longtemps.
// ---------------------------------------------------------
static void handle_offtrack(KartState& k, float dt) {
    // Le bord visible de la bande rouge/blanche commence exactement à
    // |x|=1.0 (cf. kart_render.cpp, le bitume va jusqu'à pr.w, la bande
    // jusqu'à pr.w*1.12). Mais la détection se fait sur le CENTRE du kart
    // (k.x), qui a une largeur visuelle non nulle à l'écran : avec un seuil
    // à 1.0 pile, le centre franchit la limite — et donc la pénalité se
    // déclenche — avant que le sprite du kart ne touche réellement la
    // bande à l'écran. On ajoute une petite marge (~ moitié largeur kart)
    // pour faire correspondre le déclenchement au contact visuel réel.
    const float OFFTRACK_THRESHOLD = 1.08f;
    bool offtrack = std::abs(k.x) > OFFTRACK_THRESHOLD;

    if (!offtrack) {
        k.off_track_timer = 0.0f;
        return;
    }

    k.speed *= OFFROAD_PENALTY;
    k.off_track_timer += dt;

    // Son hors-piste : déclenché une seule fois par sortie (première frame
    // where off_track_timer vient de démarrer), pas en boucle à chaque frame
    if (k.off_track_timer < dt * 2.0f)
        core::sfx_offtrack();

    if (k.off_track_timer >= OFFTRACK_RESET_DELAY) {
        k.x = 0.0f;
        k.speed *= OFFTRACK_RESET_SPEED_MULT;
        k.off_track_timer = 0.0f;
        core::sfx_crash_hard();
    }
}

// ---------------------------------------------------------
// Collisions entre karts
// ---------------------------------------------------------
// total_length : nécessaire pour calculer la distance la plus courte le
// long de la piste (en tenant compte du rebouclage de tour). Sans ça, deux
// karts physiquement côte à côte mais de part et d'autre de la ligne
// d'arrivée (l'un juste avant, l'autre juste après) avaient un |A.z - B.z|
// proche de total_length au lieu de quelques unités : la collision n'était
// donc jamais détectée entre eux — c'est le bug rapporté.
static void handle_kart_collisions(std::vector<KartState>& karts, float total_length) {
    for (size_t i = 0; i < karts.size(); ++i) {
        for (size_t j = i + 1; j < karts.size(); ++j) {

            KartState& A = karts[i];
            KartState& B = karts[j];

            // Un kart arrivé (et a fortiori disparu) ne doit plus jamais
            // participer aux collisions physiques — sinon un concurrent
            // encore en course qui le frôle reste "collé" contact après
            // contact, et sfx_bump()/sfx_crash_hard() se redéclenchent en
            // boucle tant qu'ils se touchent (le bruit horrible signalé).
            if (A.finished || B.finished) continue;

            float raw_dz = std::abs(A.z - B.z);
            float dz = std::min(raw_dz, total_length - raw_dz); // distance la plus courte sur le tour
            if (dz > 15.0f) continue; // x2.5 (cf. Segment::length 8->20)

            float dx = A.x - B.x;
            float dist = std::abs(dx);
            float min_dist = A.radius + B.radius;

            if (dist < min_dist) {
                float push = (min_dist - dist) * 0.5f;
                core::sfx_bump();
                // Crash violent si les deux karts sont rapides (sauf si l'un
                // des deux est protégé par un bouclier : pas de casse alors)
                if (A.speed > MAX_SPEED * 0.6f && B.speed > MAX_SPEED * 0.6f
                    && !A.has_shield && !B.has_shield)
                    core::sfx_crash_hard();
                if (A.speed > B.speed) {
                    A.x += (dx >= 0 ? push : -push);
                    B.x -= (dx >= 0 ? push : -push);
                    if (!A.has_shield) B.speed *= 0.9f;
                } else {
                    A.x -= (dx >= 0 ? push : -push);
                    B.x += (dx >= 0 ? push : -push);
                    if (!B.has_shield) A.speed *= 0.9f;
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
void update_player(KartState& k, const Track& t, float dt, std::vector<float>& item_box_cooldowns) {
    using namespace core;

    bool left      = input_is_held(Button::JoystickLeft);
    bool right     = input_is_held(Button::JoystickRight);
    bool accel     = input_is_held(Button::A);
    bool brake     = input_is_held(Button::B);
    bool driftB    = input_is_held(Button::C);
    bool jump      = input_is_held(Button::D);
    bool use_bonus = input_was_pressed(Button::L1);

    // Dérapage forcé (flaque d'huile) : le volant ne répond plus, on
    // subit juste la glisse jusqu'à la fin du minuteur.
    bool spinning_out = k.spinout_timer > 0.0f;
    if (spinning_out) k.spinout_timer -= dt;

    // Acceleration / frein
    if (accel) {
        if (k.speed < 0.05f) core::sfx_accel_start(); // démarrage depuis l'arrêt
        k.speed += ACCEL;
    }
    if (brake) k.speed -= BRAKE;

    if (!accel && !brake) {
        if (k.speed > 0) k.speed -= FRICTION;
        else if (k.speed < 0) k.speed += FRICTION;
    }

    k.speed = std::clamp(k.speed, 0.0f, MAX_SPEED);
    if (k.shock_slow_timer > 0.0f) {
        k.shock_slow_timer -= dt;
        k.speed = std::min(k.speed, MAX_SPEED * SHOCK_SLOW_MULT);
    }
    float speed_percent = k.speed / MAX_SPEED;

    // -------------------------------------------------------------------
    // Drift + mini-turbo (façon Mario Kart) : maintenir le drift qualifiant
    // (virage + C maintenu, vitesse suffisante) charge un boost de plus en
    // plus fort selon la durée. Le boost est accordé au RELÂCHEMENT du
    // drift (touche C ou virage relâché), pas pendant — ça récompense le
    // bon timing plutôt que le simple fait de rester en drift.
    // -------------------------------------------------------------------
    bool qualifying_drift = driftB && (left || right) && k.speed > 0.3f && !spinning_out;
    if (qualifying_drift) {
        k.drifting = true;
        k.drift = std::min(1.0f, k.drift + dt * 4.0f);
        k.drift_charge += dt;
        int prev_tier = k.drift_tier_reached;
        k.drift_tier_reached =
            (k.drift_charge >= DRIFT_TIER3_TIME) ? 3 :
            (k.drift_charge >= DRIFT_TIER2_TIME) ? 2 :
            (k.drift_charge >= DRIFT_TIER1_TIME) ? 1 : 0;
        if (k.drift_tier_reached > prev_tier) core::sfx_drift_tier_up();
    } else {
        k.drifting = false;
        k.drift = std::max(0.0f, k.drift - dt * 6.0f);

        // Relâchement d'un drift qui avait atteint au moins le palier 1 :
        // on accorde le mini-turbo correspondant, sauf si un boost est déjà
        // en cours (pas de cumul).
        if (k.drift_charge > 0.0f && k.drift_tier_reached > 0 && !k.has_boost) {
            int tier = k.drift_tier_reached;
            k.has_boost = true;
            k.boost_timer = DRIFT_BOOST_TIME[tier];
            k.speed = std::max(k.speed, DRIFT_BOOST_SPEED[tier]);
            core::sfx_boost();
        }
        k.drift_charge = 0.0f;
        k.drift_tier_reached = 0;
    }

    // Steering brut (input gauche/droite) — ignoré pendant un dérapage forcé
    // (flaque d'huile) : on ne contrôle plus rien jusqu'à la fin du délai.
    float turn = 0.0f;
    if (!spinning_out) {
        if (left)  turn -= 1.0f;
        if (right) turn += 1.0f;
    }

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

    // Bonus (Boost = immédiat, Shield = activation avec durée, Shock =
    // déclenché ici mais appliqué aux adversaires dans update_all(), qui a
    // accès à tous les karts).
    if (use_bonus) {
        if (k.bonus == BonusType::Boost) {
            k.has_boost = true;
            k.boost_timer = BOOST_TIME;
            k.bonus = BonusType::None;
            core::sfx_boost();
        } else if (k.bonus == BonusType::Shield) {
            k.has_shield = true;
            k.shield_timer = SHIELD_TIME;
            k.bonus = BonusType::None;
            core::sfx_shield_up();
        } else if (k.bonus == BonusType::Shock) {
            k.pending_shock = true; // traité dans update_all()
            k.bonus = BonusType::None;
            core::sfx_shock_use();
        }
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

    // Bouclier
    if (k.has_shield) {
        k.shield_timer -= dt;
        if (k.shield_timer <= 0.0f) k.has_shield = false;
    }

    // Avance longitudinale
    if (!k.finished) {
        k.z += k.speed * dt * 60.0f;
        if (k.z >= t.total_length) {
            k.z -= t.total_length;
            k.lap++;
            core::sfx_lap();
            if (k.lap >= t.laps) {
                // IMPORTANT : on marque seulement "fini" ici. Le score est
                // attribué plus tard dans update_all(), une fois le
                // classement de TOUS les karts recalculé pour cette frame
                // (cf. update_all) — sinon on utiliserait le rang de la
                // frame précédente, potentiellement faux pile au moment où
                // plusieurs karts franchissent la ligne presque ensemble.
                k.finished = true;
                k.speed = 0.0f;
            }
        }
    }

    k.seg_index = find_segment(t, k.z);
    const Segment& s = t.segs[k.seg_index];

    // Tremplin
    if (s.jump_pad && k.on_ground) {
        k.vy = 1.125f; // x2.5 (cohérent avec GRAVITY x2.5 : même temps de vol, saut plus haut)
        k.on_ground = false;
        core::sfx_jump();
    }

    // Saut manuel
    if (jump && k.on_ground) {
        k.vy = 0.875f; // x2.5
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
    handle_track_items(k, s, k.seg_index, item_box_cooldowns);

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
void update_ai(KartState& k, size_t own_index, const std::vector<KartState>& all_karts,
               const Track& t, float dt, std::vector<float>& item_box_cooldowns) {
    const Segment& s = t.segs[k.seg_index];

    // Dérapage forcé (flaque d'huile) : l'IA aussi en subit les
    // conséquences — plus de recherche de trajectoire tant que ça dure.
    bool spinning_out = k.spinout_timer > 0.0f;
    if (spinning_out) k.spinout_timer -= dt;

    // -------------------------------------------------------------------
    // Evitement : AVANT, un seul kart gênant était pris en compte (le
    // premier trouvé), les autres ignorés — dans un enchaînement de
    // virages avec plusieurs IA groupées, ça les faisait se marcher
    // dessus (chacune corrigeant sa trajectoire sans tenir compte des
    // AUTRES karts proches). Désormais on moyenne la direction d'évitement
    // de TOUS les karts proches devant, pondérée par leur proximité (plus
    // c'est proche, plus ça pèse dans la décision).
    // -------------------------------------------------------------------
    float target_x = 0.0f;
    if (!spinning_out) {
        const float AVOID_LOOKAHEAD = 30.0f;  // distance "devant" surveillee (x2.5, cf. Segment::length)
        const float AVOID_LATERAL   = 0.5f;   // distance laterale jugee "sur la trajectoire"

        float avoid_sum = 0.0f;
        float weight_sum = 0.0f;

        for (size_t i = 0; i < all_karts.size(); ++i) {
            if (i == own_index) continue;
            const KartState& other = all_karts[i];
            if (other.finished) continue; // ne pas éviter un kart qui a fini (pourrait ralentir/s'arrêter)

            float dz = other.z - k.z;
            if (dz < 0.0f) dz += t.total_length; // gere le rebouclage de tour
            if (dz > AVOID_LOOKAHEAD) continue;  // trop loin devant, ignore

            float dx = other.x - k.x;
            if (std::abs(dx) > AVOID_LATERAL) continue; // pas vraiment sur la trajectoire

            float weight = 1.0f - (dz / AVOID_LOOKAHEAD); // plus proche = plus important
            float side = (dx >= 0.0f) ? -1.0f : 1.0f;     // s'écarte du côté opposé à cet obstacle
            avoid_sum += side * weight;
            weight_sum += weight;
        }

        if (weight_sum > 0.0f) {
            target_x = std::clamp(avoid_sum / weight_sum, -1.0f, 1.0f) * 0.8f;
        }
    }

    float diff = target_x - k.x;
    k.x += diff * 0.05f; // plus reactif que l'ancien 0.03f, pour eviter a temps

    // -------------------------------------------------------------------
    // Rattrapage (rubber-banding) léger : SANS ça, une IA qui prend le
    // large est quasi impossible à rattraper pour un joueur humain — elle
    // accélère à 80% de ACCEL EN PERMANENCE (pas besoin de tenir un bouton
    // comme le joueur) et sort très rarement de la piste (son évitement la
    // garde proche du centre, donc jamais pénalisée par handle_offtrack).
    // On réduit progressivement son plafond de vitesse quand elle mène le
    // joueur de plus de 3 segments d'avance (jusqu'à -15% à 10 segments et
    // plus) — effet discret, jamais un brusque coup de frein.
    // all_karts[0] est toujours le joueur (convention utilisée partout
    // ailleurs, cf. draw_race/kart_game.cpp).
    // -------------------------------------------------------------------
    float rubber_band_mult = 1.0f;
    if (own_index != 0 && !all_karts.empty()) {
        float gap_ahead = k.z - all_karts[0].z;
        if (gap_ahead < 0.0f) gap_ahead += t.total_length;

        const float LEAD_START = 240.0f; // 3 segments d'avance
        const float LEAD_MAX   = 800.0f; // 10 segments d'avance
        float t_lead = std::clamp((gap_ahead - LEAD_START) / (LEAD_MAX - LEAD_START), 0.0f, 1.0f);
        rubber_band_mult = 1.0f - 0.15f * t_lead;
    }

    if (k.speed < MAX_SPEED * 0.9f * rubber_band_mult)
        k.speed += ACCEL * 0.8f * rubber_band_mult;
    if (k.shock_slow_timer > 0.0f) {
        k.shock_slow_timer -= dt;
        k.speed = std::min(k.speed, MAX_SPEED * SHOCK_SLOW_MULT);
    }

    // Bonus/malus : l'IA utilise tout objet ramassé immédiatement (heuristique
    // simple, pas de "bonne occasion" à évaluer pour ce niveau d'IA).
    if (k.bonus == BonusType::Boost) {
        k.has_boost = true;
        k.boost_timer = BOOST_TIME;
        k.bonus = BonusType::None;
    } else if (k.bonus == BonusType::Shield) {
        k.has_shield = true;
        k.shield_timer = SHIELD_TIME;
        k.bonus = BonusType::None;
        core::sfx_shield_up();
    } else if (k.bonus == BonusType::Shock) {
        k.pending_shock = true; // traité dans update_all()
        k.bonus = BonusType::None;
        core::sfx_shock_use();
    }

    if (k.has_boost) {
        k.boost_timer -= dt;
        if (k.boost_timer <= 0.0f) k.has_boost = false;
        else k.speed = std::max(k.speed, BOOST_SPEED);
    }
    if (k.has_shield) {
        k.shield_timer -= dt;
        if (k.shield_timer <= 0.0f) k.has_shield = false;
    }

    // Avance
    if (!k.finished) {
        k.z += k.speed * dt * 60.0f;
        if (k.z >= t.total_length) {
            k.z -= t.total_length;
            k.lap++;
            if (k.lap >= t.laps) {
                k.finished = true;
                // On NE fige plus la vitesse à 0 ici : un kart qui s'arrête
                // pile en plein milieu de la piste bloque les concurrents
                // encore en course et fait spammer le son de collision
                // (handle_kart_collisions) en continu tant qu'ils restent
                // au contact. Il continue à rouler, ralentit doucement, puis
                // disparaît (cf. bloc ci-dessous) une fois assez loin.
            }
        }
    } else {
        // Post-arrivée : continue d'avancer tout droit (plus de logique de
        // tour/lap), en décélérant doucement, pendant quelques secondes,
        // puis disparaît (faded) — plus jamais dessiné ni pris en compte
        // dans les collisions (cf. handle_kart_collisions et kart_render.cpp).
        const float FINISH_FADE_DELAY = 3.0f; // secondes avant disparition
        k.finish_timer += dt;
        k.speed = std::max(0.0f, k.speed - ACCEL * 1.5f);
        k.z += k.speed * dt * 60.0f;
        if (k.z >= t.total_length) k.z -= t.total_length;
        if (k.finish_timer >= FINISH_FADE_DELAY) {
            k.faded = true;
        }
    }

    k.seg_index = find_segment(t, k.z);

    if (!k.faded) {
        // Hors-piste : penalite + replacement force apres un delai
        handle_offtrack(k, dt);

        handle_decor_collision(k, s);
        handle_track_items(k, s, k.seg_index, item_box_cooldowns);
    }

    // Pour l'IA, on garde un angle base uniquement sur la courbure de la piste.
    k.angle = s.curve * 0.5f;
}

// ---------------------------------------------------------
// update_all()
// ---------------------------------------------------------
void update_all(std::vector<KartState>& karts,
                const Track& t, float dt,
                std::vector<float>& item_box_cooldowns)
{
    // Recharge des bonus box (une seule fois par frame, pas par kart).
    for (auto& c : item_box_cooldowns)
        if (c > 0.0f) c -= dt;

    // Joueur
    update_player(karts[0], t, dt, item_box_cooldowns);

    // IA
    for (size_t i = 1; i < karts.size(); ++i)
        update_ai(karts[i], i, karts, t, dt, item_box_cooldowns);

    // Onde de choc (BonusType::Shock) : ralentit tous les karts encore en
    // course dans un petit rayon devant celui qui l'a déclenchée. Traité
    // ici (pas dans update_player/update_ai) car il faut accéder à TOUS
    // les karts, pas seulement celui qui utilise l'objet.
    for (size_t i = 0; i < karts.size(); ++i) {
        if (!karts[i].pending_shock) continue;
        karts[i].pending_shock = false;
        if (karts[i].faded) continue;

        for (size_t j = 0; j < karts.size(); ++j) {
            if (j == i || karts[j].finished) continue;
            float dz = karts[j].z - karts[i].z;
            if (dz < 0.0f) dz += t.total_length;
            if (dz > SHOCK_RADIUS) continue;
            if (karts[j].has_shield) continue; // le bouclier protège aussi du choc
            karts[j].speed *= SHOCK_SLOW_MULT;
            karts[j].shock_slow_timer = SHOCK_SLOW_TIME;
        }
        core::sfx_shock_hit();
    }

    // Collisions
    handle_kart_collisions(karts, t.total_length);

    // Attribue l'ordre RÉEL d'arrivée dès qu'un kart passe la ligne, pour
    // pouvoir départager correctement le classement une fois plusieurs
    // karts "finished" (comparer leur lap/z à ce stade ne reflète plus rien
    // : un kart figé/décéléré après l'arrivée peut se retrouver avec un z
    // arbitraire selon la frame exacte où il a franchi la ligne, ce qui
    // donnait des classements finaux incohérents — ex: le dernier de la
    // course affiché à tort en 3e place).
    {
        int next_order = 0;
        for (auto& k : karts) if (k.finish_order >= 0) next_order++;
        for (auto& k : karts) {
            if (k.finished && k.finish_order < 0) {
                k.finish_order = next_order++;
            }
        }
    }

    // Classement
    std::vector<size_t> order(karts.size());
    for (size_t i = 0; i < karts.size(); ++i) order[i] = i;

    std::sort(order.begin(), order.end(),
        [&](size_t a, size_t b) {
            bool fa = karts[a].finished, fb = karts[b].finished;
            if (fa != fb) return fa; // un kart arrivé passe toujours devant un kart encore en course
            if (fa && fb) return karts[a].finish_order < karts[b].finish_order;
            if (karts[a].lap != karts[b].lap)
                return karts[a].lap > karts[b].lap;
            return karts[a].z > karts[b].z;
        });

    for (size_t r = 0; r < order.size(); ++r)
        karts[order[r]].rank = (int)r + 1;

    // Attribution des points : APRÈS que le classement de cette frame soit
    // à jour, pour ne jamais utiliser un rang périmé (cf. update_player/
    // update_ai, qui se contentent de marquer "finished" sans donner de
    // points). Chaque kart ne reçoit ses points qu'une seule fois.
    for (auto& k : karts) {
        if (k.finished && !k.score_awarded) {
            int rank_idx = k.rank - 1;
            if (rank_idx < 0) rank_idx = 0;
            if (rank_idx > 3) rank_idx = 3;
            k.score += t.points_by_rank[rank_idx];
            k.score_awarded = true;
        }
    }
}

} // namespace kart
