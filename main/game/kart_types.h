/*
===============================================================================
  kart_types.h — Types de base du moteur de kart (AKArt)
-------------------------------------------------------------------------------
  Rôle :
    - Définir les structures de données utilisées par tout le jeu :
        * Segment : description d’un tronçon de piste (type OutRun).
        * Track   : piste complète (liste de segments, longueur totale, tours).
        * BonusType : types de bonus (boost, etc.).
        * KartState : état complet d’un kart (joueur ou IA).
        * Camera    : paramètres de la vue pseudo‑3D.
    - Servir de contrat entre :
        * kart_engine.cpp  (physique, drift, collisions, angle du kart)
        * kart_render.cpp  (rendu pseudo‑3D, billboards, HUD)
        * kart_game.cpp    (boucle de jeu, caméra, mode titre/course).
===============================================================================
*/

#pragma once
#include <vector>
#include <cstdint>

namespace kart {

// ---------------------------------------------------------
// Type de kart
// ---------------------------------------------------------
enum class KartType {
    Player,
    AI
};

// ---------------------------------------------------------
// Segment de piste (type OutRun)
// ---------------------------------------------------------
struct Segment {
    float length;       // longueur du segment
    float curve;        // courbure (-1..+1)
    float elevation;    // pente (optionnel, non utilisé pour l'instant)
    uint8_t decor_left;
    uint8_t decor_right;

    bool jump_pad;      // tremplin

    // Position cumulée le long de la piste (= somme des longueurs des
    // segments précédents). Précalculée une seule fois (cf. track_example.h,
    // fonction compute_track_geometry) pour éviter de la re-sommer à chaque
    // frame — un simple utilitaire, sans rapport avec le rendu du virage.
    float start_z = 0.0f;
};

// ---------------------------------------------------------
// Piste complète
// ---------------------------------------------------------
struct Track {
    std::vector<Segment> segs;
    float total_length;
    int laps;
    // Points attribués par place (index 0 = 1ère place) — cf. kart_engine.cpp
    int points_by_rank[4] = { 10, 6, 3, 1 };
};

// ---------------------------------------------------------
// Bonus
// ---------------------------------------------------------
enum class BonusType {
    None,
    Boost,
    // futur : missile, mine, etc.
};

// ---------------------------------------------------------
// État d'un kart
// ---------------------------------------------------------
struct KartState {
    KartType type;

    float x;        // position latérale (-1..+1)
    float z;        // position le long de la piste
    float y;        // hauteur (sauts)
    float vy;       // vitesse verticale
    float speed;    // vitesse

    float angle;    // orientation visuelle (steering, volant, HUD)
    float drift;    // intensité drift
    bool drifting;
    bool on_ground;

    int seg_index;
    int lap;
    int rank;
    bool finished = false;  // a terminé la course (tous les tours bouclés)
    int  score    = 0;      // points de victoire cumulés (après la course)

    // Bonus
    BonusType bonus;
    bool has_boost;
    float boost_timer;

    // Collision
    float radius;   // rayon collision

    // Hors-piste prolongé : pénalité puis replacement sur la piste
    float off_track_timer = 0.0f;
};

// ---------------------------------------------------------
// Camera pseudo-3D (projection fidèle à un moteur OutRun classique)
// ---------------------------------------------------------
//   - height      : hauteur de la caméra (unités "monde", mêmes unités que
//                   Segment::length / Track::total_length)
//   - fov         : champ de vision en degrés
//   - depth       : distance caméra-écran, dérivée de fov — calculée UNE
//                   FOIS par camera_setup() (cf. kart_render.cpp), ne pas
//                   modifier directement après coup
//   - player_z    : décalage fixe (en unités "monde") entre la position
//                   caméra et la position visuelle du kart du joueur —
//                   calculé une fois par camera_setup(), constant ensuite
//   - offset_x    : inutilisé pour la route (cf. kart_render.cpp), conservé
//                   pour compatibilité éventuelle avec d'autres effets
//   - cockpit_view: vue cockpit ou 3e personne
//   - shake       : vibration (drift, boost)
//   - angle       : réservé pour HUD / volant (pas utilisé pour la route)
// ---------------------------------------------------------
struct Camera {
    float height;
    float fov;
    float depth     = 0.0f;
    float player_z  = 0.0f;
    float offset_x;

    bool  cockpit_view;
    float shake;

    float angle = 0.0f;
};

} // namespace kart
