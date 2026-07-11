/*
===============================================================================
  music_registry.h — Musiques de fond du jeu (AKArt)
-------------------------------------------------------------------------------
  Fonctionne comme track_registry.h : un seul endroit à modifier pour
  ajouter/remplacer une musique.

  Pour ajouter/remplacer une musique :
    1. Dépose ton fichier .pmf n'importe où (ex: à la racine du repo).
    2. python3 tools/pmf_to_c.py music_xxx.pmf assets/music/music_xxx.cpp
    3. Si c'est une piste qui n'existait pas encore, crée aussi
       assets/music/music_xxx.h (modèle en bas de tools/pmf_to_c.py),
       ajoute son #include ci-dessous et une entrée dans le switch voulu.
    4. Recompile — tant que music_xxx_pmf_len vaut 0 (stub), l'API audio ne
       joue rien (silence, pas de bruit), donc aucun risque à laisser une
       piste "pas encore prête".
===============================================================================
*/
#pragma once
#include "kart_types.h"
#include "../assets/music/music_title.h"
#include "../assets/music/music_hills.h"
#include "../assets/music/music_city.h"
#include "../assets/music/music_desert.h"
#include "../assets/music/music_results.h"
#include "../assets/music/jingle_lastlap.h"
#include "../assets/music/jingle_finish.h"

namespace kart {

struct MusicRef { const uint8_t* data; unsigned int len; };

inline MusicRef music_title_ref()   { return { music_title_pmf,   music_title_pmf_len }; }
inline MusicRef music_results_ref() { return { music_results_pmf, music_results_pmf_len }; }
inline MusicRef jingle_lastlap_ref(){ return { jingle_lastlap_pmf, jingle_lastlap_pmf_len }; }
inline MusicRef jingle_finish_ref() { return { jingle_finish_pmf,  jingle_finish_pmf_len }; }

// Musique de course selon le thème de la piste (Track::sky_theme) — chaque
// piste porte déjà son thème, donc pas besoin de choisir la musique à part.
inline MusicRef music_for_theme(SkyTheme theme) {
    switch (theme) {
        case SkyTheme::City:   return { music_city_pmf,   music_city_pmf_len };
        case SkyTheme::Desert: return { music_desert_pmf, music_desert_pmf_len };
        case SkyTheme::Hills:
        default:               return { music_hills_pmf,  music_hills_pmf_len };
    }
}

} // namespace kart
