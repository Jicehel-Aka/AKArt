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
#include "../core/kart_sprites.h"

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

    // Hasards/objets sur la CHAUSSÉE elle-même (par opposition à
    // decor_left/decor_right, qui sont des décors posés sur le BORD de la
    // route) : placés par le concepteur de piste, statiques.
    bool has_oil = false;       // flaque d'huile : dérapage forcé si on roule dessus
    float oil_offset_x = 0.0f; // position latérale de la flaque (-1..+1), pour rester évitable
    bool has_item_box = false;  // bonus box : donne un bonus aléatoire (si on n'en a pas déjà un)

    // Position cumulée le long de la piste (= somme des longueurs des
    // segments précédents). Précalculée une seule fois (cf. track_example.h,
    // fonction compute_track_geometry) pour éviter de la re-sommer à chaque
    // frame — un simple utilitaire, sans rapport avec le rendu du virage.
    float start_z = 0.0f;

    // Déplacement RÉEL (x,y) de ce segment dans le plan du dessus, tel que
    // dessiné dans l'éditeur de circuit (track_editor.html). Seulement
    // rempli pour les pistes exportées par l'éditeur (cf. Track::has_shape)
    // — permet à la minimap de retracer EXACTEMENT le circuit dessiné, au
    // lieu de le redéduire (approximativement) de `curve`, qui n'est qu'un
    // artifice de rendu pseudo-3D sans lien géométrique exact avec la vraie
    // forme du circuit.
    float shape_dx = 0.0f;
    float shape_dy = 0.0f;
};

// ---------------------------------------------------------
// Piste complète
// ---------------------------------------------------------
// ---------------------------------------------------------
// Thème visuel de fond (ciel qui défile en haut de l'écran)
// ---------------------------------------------------------
enum class SkyTheme {
    Hills,   // collines vertes (decor_sky_bg)
    City,    // skyline urbaine (decor_sky_bg_city)
    Desert,  // désert/canyon (decor_sky_bg_desert)
};

// Nombre total de thèmes (pour faire cycler le sélecteur du menu titre sans
// avoir à toucher à ce nombre à chaque nouveau thème ajouté).
constexpr int kSkyThemeCount = 3;

inline const char* sky_theme_name(SkyTheme t) {
    switch (t) {
        case SkyTheme::Hills:  return "Collines";
        case SkyTheme::City:   return "Ville";
        case SkyTheme::Desert: return "Desert";
    }
    return "?";
}

struct Track {
    std::vector<Segment> segs;
    float total_length;
    int laps;
    // Points attribués par place (index 0 = 1ère place) — cf. kart_engine.cpp
    int points_by_rank[8] = { 10, 8, 6, 5, 4, 3, 2, 1 };
    SkyTheme sky_theme = SkyTheme::Hills;

    // true pour les pistes exportées par l'éditeur de circuit (chaque
    // Segment a alors un shape_dx/shape_dy réel et exact) ; false pour les
    // pistes "à la main" comme track_example.h (minimap alors dérivée de
    // façon approximative à partir de `curve`, cf. build_shape()).
    bool has_shape = false;

    // Identifiant unique incrémenté à chaque nouvelle piste construite (cf.
    // kart_game.cpp::kart_game_init()). Sert à invalider le cache de forme
    // de la minimap (kart_render.cpp) : SANS ça, la minimap ne recalculait
    // sa forme qu'une seule fois pour toute la durée de l'appli (au tout
    // premier chargement), et réaffichait ensuite ce même tracé pour
    // TOUTES les pistes suivantes, quelle que soit la piste réellement
    // chargée — d'où un tracé "propre dans l'éditeur" mais complètement
    // faux en jeu dès qu'on changeait de piste après la première course.
    int generation = 0;
};

// ---------------------------------------------------------
// Bonus
// ---------------------------------------------------------
enum class BonusType {
    None,
    Boost,   // accélération temporaire immédiate (existant)
    Shield,  // protège des flaques d'huile et amortit les collisions pendant sa durée
    Shock,   // onde de choc : ralentit tous les adversaires proches devant soi
};

// ---------------------------------------------------------
// État d'un kart
// ---------------------------------------------------------
struct KartState {
    KartType type;
    core::KartColor color = core::KartColor::Red;

    float x;        // position latérale (-1..+1)
    float z;        // position le long de la piste
    float y;        // hauteur (sauts)
    float vy;       // vitesse verticale
    float speed;    // vitesse

    float angle;    // orientation visuelle (steering, volant, HUD)
    float drift;    // intensité drift (0..1, purement visuel/inclinaison)
    bool drifting;
    bool on_ground;

    // Drift-boost façon "mini-turbo" : temps cumulé de drift QUALIFIANT en
    // continu (vitesse suffisante + virage maintenu). Remis à 0 dès que le
    // drift s'interrompt. Au relâchement, si le seuil d'un palier a été
    // atteint, un boost est accordé (cf. update_player, DRIFT_TIER_*).
    float drift_charge = 0.0f;
    int   drift_tier_reached = 0; // 0=aucun, 1/2/3 = palier mini-turbo atteint (pour le HUD)

    // Bouclier (BonusType::Shield) : protège des flaques d'huile et amortit
    // les collisions pendant sa durée.
    bool  has_shield = false;
    float shield_timer = 0.0f;

    // Flaque d'huile : dérapage forcé en cours (steering ignoré, vitesse
    // coupée) pendant ce délai.
    float spinout_timer = 0.0f;

    // BonusType::Shock : mis à true par update_player/update_ai à
    // l'utilisation, traité puis remis à false dans update_all() (qui a
    // accès à tous les karts pour appliquer le ralentissement aux autres).
    bool pending_shock = false;
    float shock_slow_timer = 0.0f; // tant que >0, vitesse plafonnée (subit un choc)

    int seg_index;
    int lap;
    int rank;
    bool finished = false;  // a terminé la course (tous les tours bouclés)
    int  finish_order = -1; // ordre réel d'arrivée (0=premier arrivé), -1=pas encore fini
    float finish_race_time = -1.0f; // chrono de course au moment de l'arrivée (championnat)
    bool score_awarded = false; // évite d'attribuer les points plusieurs fois
    int  score    = 0;      // points de victoire cumulés (après la course)

    // Après l'arrivée : le kart continue à rouler quelques secondes (au
    // lieu de se figer net en plein milieu de la piste, ce qui bloquait les
    // autres concurrents et spammait le son de collision) avant de
    // disparaître proprement.
    float finish_timer = 0.0f;
    bool  faded = false;

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
