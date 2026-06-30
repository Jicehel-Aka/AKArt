#pragma once
#include "kart_types.h"

namespace kart {

// Precalcule UNE SEULE FOIS Segment::start_z (position cumulee le long de
// la piste). C'est juste un utilitaire de distance, sans rapport avec le
// rendu du virage : la courbure, elle, est accumulee a chaque frame dans
// kart_render.cpp (cf. son en-tete), fidelement a l'algorithme de
// reference - ne pas reintroduire de precalcul de "world_x" ici.
inline void compute_track_geometry(Track& t) {
    float z = 0.0f;
    for (auto& s : t.segs) {
        s.start_z = z;
        z += s.length;
    }
}

inline Track make_test_track() {
    Track t;
    t.laps = 3;

    // Ligne droite
    for (int i = 0; i < 40; ++i)
        t.segs.push_back({ 8.0f, 0.0f, 0.0f, 0, 0, false });

    // Virage droite
    for (int i = 0; i < 40; ++i)
        t.segs.push_back({ 8.0f, 0.4f, 0.0f, 1, 1, false });

    // Tremplin
    for (int i = 0; i < 5; ++i)
        t.segs.push_back({ 8.0f, 0.0f, 0.0f, 0, 0, true });

    // Ligne droite
    for (int i = 0; i < 40; ++i)
        t.segs.push_back({ 8.0f, 0.0f, 0.0f, 0, 0, false });

    // Virage gauche
    for (int i = 0; i < 40; ++i)
        t.segs.push_back({ 8.0f, -0.4f, 0.0f, 2, 2, false });

    t.total_length = 0.0f;
    for (auto& s : t.segs) t.total_length += s.length;

    compute_track_geometry(t);

    return t;
}

} // namespace kart
