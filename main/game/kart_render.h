/*
===============================================================================
  kart_render.h — Interface du rendu pseudo-3D (AKArt)
-------------------------------------------------------------------------------
  Rôle :
    - Définir l’API de rendu de la course :
        * route (segments, courbure, hors-piste)
        * décor (arbres, rochers, billboards)
        * karts (joueur + IA)
        * HUD + mini-carte.
    - Utiliser la structure Camera pour paramétrer la vue :
        * position longitudinale (cam_z implicite via draw_race)
        * hauteur de caméra (height)
        * décalage latéral (offset_x) pour suivre le joueur
        * angle (angle) pour faire pivoter la vue quand le joueur tourne.
===============================================================================
*/

#pragma once
#include "kart_types.h"
#include <vector>

namespace kart {

// -----------------------------------------------------------------------------
// Camera — paramètres de la vue pseudo-3D
// -----------------------------------------------------------------------------
// À vérifier dans kart_types.h : la struct Camera doit contenir au minimum :
//   - float fov;        // champ de vision (facteur de projection)
//   - float height;     // hauteur de la caméra (décalage vertical)
//   - float offset_x;   // décalage latéral (suivi du joueur)
//   - bool  cockpit_view;
//   - float shake;      // éventuelle vibration
//   - float angle;      // 🔥 angle de la caméra (piste + steering joueur)
// -----------------------------------------------------------------------------

// camera_setup()
//   - Calcule UNE FOIS cam.depth et cam.player_z à partir de cam.fov et
//     cam.height. À appeler après avoir réglé fov/height (kart_game.cpp),
//     avant le premier draw_race().
void camera_setup(Camera& cam);

// draw_race()
//   - Dessine la course complète : route, décor, karts, HUD.
//   - Utilise karts[0] comme joueur (caméra centrée sur lui).
void draw_race(const Track& t,
               const std::vector<KartState>& karts,
               const Camera& cam,
               const std::vector<float>& item_box_cooldowns);

} // namespace kart
