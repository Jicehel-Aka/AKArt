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
    float elevation;    // pente (optionnel)
    uint8_t decor_left;
    uint8_t decor_right;

    bool jump_pad;      // tremplin
};

// ---------------------------------------------------------
// Piste complete
// ---------------------------------------------------------
struct Track {
    std::vector<Segment> segs;
    float total_length;
    int laps;
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
// Etat d'un kart
// ---------------------------------------------------------
struct KartState {
    KartType type;

    float x;        // position laterale (-1..+1)
    float z;        // position le long de la piste
    float y;        // hauteur (sauts)
    float vy;       // vitesse verticale
    float speed;    // vitesse
    float angle;    // orientation visuelle
    float drift;    // intensite drift
    bool drifting;
    bool on_ground;

    int seg_index;
    int lap;
    int rank;

    // Bonus
    BonusType bonus;
    bool has_boost;
    float boost_timer;

    // Collision
    float radius;   // rayon collision
};

// ---------------------------------------------------------
// Camera pseudo-3D
// ---------------------------------------------------------
struct Camera {
    float height;
    float fov;
    float offset_x;

    bool cockpit_view;
    float shake;
};

} // namespace kart
