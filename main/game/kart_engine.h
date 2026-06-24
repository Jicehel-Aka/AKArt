#pragma once
#include "kart_types.h"

namespace kart {

void update_player(KartState& k, const Track& t, float dt);
void update_ai(KartState& k, const Track& t, float dt);
void update_all(std::vector<KartState>& karts, const Track& t, float dt);

} // namespace kart
