#pragma once
#include "kart_types.h"

namespace kart {

// Trouve l'index du segment contenant la position monde z (recherche binaire
// sur start_z). Exposé publiquement (pas seulement pour kart_engine.cpp en
// interne) car kart_game.cpp en a besoin pour initialiser correctement
// KartState::seg_index au moment de placer la grille de départ — sans ça,
// seg_index restait à 0 alors que la grille est désormais placée derrière
// la ligne (z proche de la fin du circuit, donc un tout autre segment), ce
// qui faisait planter tout le calcul de rendu (route invisible, juste un
// grand rectangle vert) au début de chaque course.
int find_segment(const Track& t, float z);

// item_box_cooldowns : un flottant par segment de la piste (même taille que
// t.segs), décrémenté chaque frame — un segment avec has_item_box==true ne
// redonne un bonus que lorsque son cooldown est revenu à 0 (cf.
// kart_game.cpp, qui le crée/redimensionne à chaque kart_game_init()).
// Séparé de Track (passée en const&) plutôt qu'ajouté à Segment pour ne pas
// rendre Track mutable dans toutes ces fonctions pour un simple minuteur.

void update_player(KartState& k, const Track& t, float dt, std::vector<float>& item_box_cooldowns);

// own_index : indice de ce kart dans "all_karts" (pour qu'il s'ignore
// lui-même en cherchant des karts à éviter).
void update_ai(KartState& k, size_t own_index, const std::vector<KartState>& all_karts,
               const Track& t, float dt, std::vector<float>& item_box_cooldowns);

void update_all(std::vector<KartState>& karts, const Track& t, float dt,
                 std::vector<float>& item_box_cooldowns);

} // namespace kart
