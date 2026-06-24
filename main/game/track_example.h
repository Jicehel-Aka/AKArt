#pragma once
#include "kart_types.h"

namespace kart {

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

    return t;
}

} // namespace kart
