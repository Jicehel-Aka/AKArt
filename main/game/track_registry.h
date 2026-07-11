/*
===============================================================================
  track_registry.h — Liste des circuits disponibles (AKArt)
-------------------------------------------------------------------------------
  Rôle :
    - Centraliser TOUTES les pistes jouables en un seul endroit.
    - L'écran titre affiche kTrackList[i].name et cycle dessus (Joystick G/D)
      à la place du sélecteur de thème brut : chaque piste porte déjà son
      thème (Track::sky_theme), donc choisir une piste choisit son thème.

  Pour ajouter une nouvelle piste (dessinée dans track_editor.html) :
    1. Colle le fichier exporté dans game/tracks/ (ex: tracks/track_xxx.h).
    2. Ajoute une ligne #include ci-dessous.
    3. Ajoute une ligne dans kTrackList[] (nom affiché + fonction "make_...").
  C'est tout — aucun autre fichier à toucher.

  Plus tard (coupes multi-circuits) : ce fichier sera aussi la base pour
  définir des enchaînements (ex: un tableau d'indices dans kTrackList).
===============================================================================
*/
#pragma once
#include "kart_types.h"
#include "track_example.h"
#include "tracks/track_desert_01.h"
// #include "tracks/track_xxx.h"   // <- ajouter une ligne par nouvelle piste

namespace kart {

struct TrackDef {
    const char* name;
    Track (*build)();
};

// Chaque piste doit être exposée via une fonction SANS argument (les lambdas
// sans capture se convertissent en pointeur de fonction, donc pas besoin
// d'écrire une fonction à part si l'appel a besoin d'un paramètre par défaut).
static const TrackDef kTrackList[] = {
    { "Circuit Test (Collines)",  []() -> Track { return make_test_track(SkyTheme::Hills); } },
    { "Circuit Test (Ville)",     []() -> Track { return make_test_track(SkyTheme::City); } },
    { "Circuit Test (Desert)",    []() -> Track { return make_test_track(SkyTheme::Desert); } },
    { "Circuit Desert 1",         []() -> Track { return make_editor_track(SkyTheme::Desert); } },
    // { "Nom affiché sur l'écran titre", []() -> Track { return make_xxx(); } },
};

constexpr int kTrackCount = sizeof(kTrackList) / sizeof(kTrackList[0]);

// -----------------------------------------------------------------------------
// Coupes — regroupent plusieurs circuits (indices dans kTrackList) joués à la
// suite, avec un championnat qui se cumule sur toute la coupe (remis à zéro
// au DÉBUT d'une coupe, cf. kart_game.cpp/reset_championship()).
//
// Pour ajouter un circuit à une coupe existante : ajoute son index dans le
// tableau track_indices et augmente track_count. Pour ajouter une coupe :
// ajoute une entrée dans kCups[] (max 6 circuits par coupe, augmente
// MAX_CUP_TRACKS si besoin).
// -----------------------------------------------------------------------------
constexpr int MAX_CUP_TRACKS = 6;

struct Cup {
    const char* name;
    int track_indices[MAX_CUP_TRACKS];
    int track_count;
};

static const Cup kCups[] = {
    { "Coupe Campagne", { 0 },    1 },  // Circuit Test (Collines)
    { "Coupe Ville",    { 1 },    1 },  // Circuit Test (Ville)
    { "Coupe Desert",   { 2, 3 }, 2 },  // Circuit Test (Desert) + Circuit Desert 1
};
constexpr int kCupCount = sizeof(kCups) / sizeof(kCups[0]);

} // namespace kart
