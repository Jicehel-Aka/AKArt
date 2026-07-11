#pragma once
#include "kart_types.h"
#include <cstdio>
#include <cmath>

namespace kart {

// -----------------------------------------------------------------------------
// Normalisation de la géométrie (start_z cumulés)
// -----------------------------------------------------------------------------
inline void compute_track_geometry(Track& t) {
    float z = 0.0f;
    for (auto& s : t.segs) {
        s.start_z = z;
        z += s.length;
    }
    t.total_length = z;
}

// -----------------------------------------------------------------------------
// Ajoute n segments identiques
// -----------------------------------------------------------------------------
inline void add(Track& t, int n, float len, float curve,
                uint8_t dl, uint8_t dr, bool jump=false)
{
    for (int i = 0; i < n; ++i)
        t.segs.push_back({len, curve, 0.f, dl, dr, jump});
}

// -----------------------------------------------------------------------------
// Pose automatique des panneaux "virage à gauche/droite"
// -----------------------------------------------------------------------------
// Repère chaque DÉBUT de virage (transition ligne droite -> courbe) et pose
// le panneau correspondant quelques segments avant, côté droit de la route
// (convention réaliste : le panneau annonçant un virage est posté sur
// l'accotement droit, qu'il annonce un virage à gauche OU à droite — seul le
// dessin du panneau change selon le sens du virage).
//   12 = decor_sign_left  (virage à gauche, curve < 0)
//   13 = decor_sign_right (virage à droite, curve > 0)
// -----------------------------------------------------------------------------
// Placement automatique des bonus box et flaques d'huile
// -----------------------------------------------------------------------------
// Répartit quelques bonus box régulièrement le long du tour, et pose des
// flaques d'huile dans une partie des virages serrés (là où elles sont les
// plus punitives/intéressantes). Purement une valeur de départ pratique
// pour que la mécanique soit testable sans avoir à tout placer à la main
// dans l'éditeur — libre à toi de personnaliser au cas par cas une fois le
// feeling validé (l'éditeur pourra exposer ça plus tard si besoin).
inline void place_track_items(Track& t, int item_box_spacing = 20, int oil_every_nth_sharp_turn = 2) {
    int n = (int)t.segs.size();
    if (n == 0) return;

    for (int i = 0; i < n; ++i) {
        if (i % item_box_spacing == item_box_spacing / 2) {
            t.segs[i].has_item_box = true;
        }
    }

    int sharp_turn_count = 0;
    for (int i = 1; i < n; ++i) {
        bool turn_starts = (std::abs(t.segs[i].curve) > 0.5f) && (std::abs(t.segs[i - 1].curve) <= 0.5f);
        if (!turn_starts) continue;
        sharp_turn_count++;
        if (sharp_turn_count % oil_every_nth_sharp_turn == 0) {
            t.segs[i].has_oil = true;
            // Alterne gauche/droite plutôt que de couvrir toute la largeur :
            // une flaque évitable en se décalant, pas un mur infranchissable.
            t.segs[i].oil_offset_x = (sharp_turn_count % 2 == 0) ? -0.45f : 0.45f;
        }
    }
}

inline void place_turn_signs(Track& t, int segments_before = 3) {
    const uint8_t SIGN_LEFT  = 12;
    const uint8_t SIGN_RIGHT = 13;

    int n = (int)t.segs.size();
    if (n == 0) return;

    for (int i = 1; i < n; ++i) {
        float prev_curve = t.segs[i - 1].curve;
        float curve      = t.segs[i].curve;

        bool turn_starts = (curve != 0.0f) && (prev_curve == 0.0f);
        if (!turn_starts) continue;

        int sign_idx = i - segments_before;
        if (sign_idx < 0) sign_idx += n; // virage proche du début du tour

        uint8_t sign_id = (curve > 0.0f) ? SIGN_RIGHT : SIGN_LEFT;
        t.segs[sign_idx].decor_right = sign_id;
    }
}

// -----------------------------------------------------------------------------
// make_test_track() — Circuit long, varié, équilibré pour la minimap
// -----------------------------------------------------------------------------
inline Track make_test_track(SkyTheme theme = SkyTheme::Hills) {

    Track t;
    t.sky_theme = theme;
    t.laps = 3;

    // Décor IDs :
    // 1=tree1  2=tree2  3=tree3  4=tree4  5=palmier
    // 6=pneu   7=drapeau 8=rocher 9=maison
    // 10=START gate 11=FINISH gate
    // 12=panneau virage gauche  13=panneau virage droite (posés automatiquement, cf. place_turn_signs)

    // == DÉPART ==
    add(t,  1, 80.f, 0.f, 10, 10);   // Portique START
    add(t,  3, 80.f, 0.f,  7,  7);   // Barrières
    add(t,  4, 80.f, 0.f,  1,  2);   // Arbres

    // == LIGNE DROITE 1 ==
    add(t,  6, 80.f, 0.f,  5,  1);   // Palmiers / arbres
    add(t,  4, 80.f, 0.f,  2,  4);   // Arbres / rochers

    // == VIRAGE DROITE LARGE ==
    add(t,  3, 80.f, 0.f,   1,  1);
    add(t,  6, 80.f, 0.40f, 6,  1);  // pneus intérieur
    add(t,  4, 80.f, 0.40f, 6,  5);  // pneus / palmiers
    add(t,  3, 80.f, 0.f,   1,  1);

    // == LIGNE DROITE PALMIERS ==
    add(t,  6, 80.f, 0.f,   5,  5);

    // == CHICANE DROITE ==
    add(t,  2, 80.f, 0.f,   2,  1);
    add(t,  3, 80.f, 0.75f, 6,  1);
    add(t,  2, 80.f, 0.f,   1,  1);
    add(t,  3, 80.f,-0.75f, 1,  6);
    add(t,  2, 80.f, 0.f,   1,  1);

    // == LIGNE DROITE MAISONS ==
    add(t,  5, 80.f, 0.f,   9,  2);

    // == TREMPLIN ==
    add(t,  2, 80.f, 0.f,   7,  7);      // annonce
    add(t,  2, 80.f, 0.f,   7,  7, true); // SAUT
    add(t,  3, 80.f, 0.f,   1,  1);      // atterrissage

    // == VIRAGE GAUCHE LARGE ==
    add(t,  3, 80.f, 0.f,   1,  1);
    add(t,  5, 80.f,-0.55f, 1,  6);
    add(t,  3, 80.f,-0.30f, 5,  6);
    add(t,  2, 80.f, 0.f,   1,  1);

    // == CHICANE GAUCHE ==
    add(t,  2, 80.f, 0.f,   1,  2);
    add(t,  3, 80.f,-0.75f, 1,  6);
    add(t,  2, 80.f, 0.f,   1,  1);
    add(t,  3, 80.f, 0.75f, 6,  1);
    add(t,  2, 80.f, 0.f,   1,  1);

    // == LIGNE DROITE ARBRES ==
    add(t,  6, 80.f, 0.f,   3,  4);

    // == VIRAGE DROITE LONG ==
    add(t,  4, 80.f, 0.35f, 8,  5);
    add(t,  4, 80.f, 0.f,   1,  1);

    // == LIGNE DROITE LONGUE ==
    add(t,  8, 80.f, 0.f,   5,  5);
    add(t,  6, 80.f, 0.f,   1,  2);

    // == VIRAGE GAUCHE LONG ==
    add(t,  5, 80.f,-0.40f, 6,  1);
    add(t,  4, 80.f,-0.40f, 5,  3);

    // == LIGNE DROITE FINALE ==
    add(t,  6, 80.f, 0.f,   3,  4);

    // == PORTIQUE FINISH ==
    add(t,  1, 80.f, 0.f,  11, 11);
    add(t,  2, 80.f, 0.f,   7,  7);

    // Géométrie
    compute_track_geometry(t);

    // Panneaux virage (posés après coup : besoin des courbures déjà en place)
    place_turn_signs(t);
    place_track_items(t);

    printf("[Track] %d segments, total_length=%.0f\n",
           (int)t.segs.size(), t.total_length);

    return t;
}

} // namespace kart
