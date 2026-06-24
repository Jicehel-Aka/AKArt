#pragma once
#include "kart_types.h"
#include <vector>

namespace kart {

void draw_race(const Track& t,
               const std::vector<KartState>& karts,
               const Camera& cam);

} // namespace kart
