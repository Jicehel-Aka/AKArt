/*
===============================================================================
  sprite_viewer.cpp — Visualisation des sprites (AKArt)
-------------------------------------------------------------------------------
*/

#include "sprite_viewer.h"
#include "kart_game.h"
#include "../core/graphics.h"
#include "../core/input.h"
#include "../core/kart_loader.h"
#include <cstdio>
#include "../assets/gfx/decor_tree1.h"
#include "../assets/gfx/decor_tree2.h"
#include "../assets/gfx/decor_tree3.h"
#include "../assets/gfx/decor_tree4.h"
#include "../assets/gfx/decor_palm.h"
#include "../assets/gfx/decor_tire.h"
#include "../assets/gfx/decor_flag.h"
#include "../assets/gfx/decor_fire.h"
#include "../assets/gfx/decor_barrier.h"
#include "../assets/gfx/decor_start_gate.h"
#include "../assets/gfx/decor_finish_gate.h"
#include "../assets/gfx/decor_house.h"
#include "../assets/gfx/decor_rock.h"
#include "../assets/gfx/decor_sign_left.h"
#include "../assets/gfx/decor_sign_right.h"
#include "../assets/gfx/decor_cactus1.h"
#include "../assets/gfx/decor_cactus2.h"
#include "../assets/gfx/decor_cactus3.h"
#include "../assets/gfx/decor_oil_slick.h"
#include "../assets/gfx/decor_bonus_boost.h"
#include "../assets/gfx/decor_bonus_shield.h"
#include "../assets/gfx/decor_bonus_shock.h"

namespace kart {

// Mode et mode sont déclarés dans kart_game.h (inclus ci-dessus) et définis
// dans kart_game.cpp.

// -----------------------------------------------------------------------------
// Liste des sprites décor
// -----------------------------------------------------------------------------
struct DecorEntry {
    const char* name;
    const uint16_t* data;
    int w, h;
};

// IMPORTANT : ces dimensions doivent correspondre exactement à w*h = taille du
// tableau de pixels (cf. extern const uint16_t decor_xxx[TAILLE] dans les .h
// respectifs), sous peine d'afficher une image décalée/cisaillée (mauvais
// stride de lecture). Valeurs alignées sur celles déjà utilisées et validées
// dans kart_render.cpp (decor_info()) — plusieurs tailles d'origine ici
// étaient incorrectes et ont été corrigées :
//   Tree1/2/3  : 48x64 (3072) → 48x96 (4608)
//   Palm       : 64x64 (4096) → 64x96 (6144)
//   Flag       : 32x64 (2048) → 64x64 (4096)
//   Start/Finish Gate : 96x64 (6144) → 128x80 (10240)
//   House      : 64x64 (4096) → 64x80 (5120)
// "Fire" n'est utilisé nulle part ailleurs dans le code : sa taille réelle
// n'a pas pu être confirmée par recoupement. 2304 pixels = 48x48 est une
// hypothèse (cohérente avec Tire, même famille de petits décors) ; à vérifier
// visuellement dans le viewer et corriger si l'image apparaît déformée.
static DecorEntry decor_list[] = {
    { "Tree1",        decor_tree1,       48, 96 },
    { "Tree2",        decor_tree2,       48, 96 },
    { "Tree3",        decor_tree3,       48, 96 },
    { "Tree4",        decor_tree4,       48, 96 },
    { "Palm",         decor_palm,        64, 96 },
    { "Tire",         decor_tire,        48, 48 },
    { "Flag",         decor_flag,        64, 64 },
    { "Fire",         decor_fire,        48, 48 },   // hypothèse, à vérifier
    { "Barrier",      decor_barrier,     64, 32 },
    { "Rock",         decor_rock,        64, 64 },
    { "Start Gate",   decor_start_gate,  128, 80 },
    { "Finish Gate",  decor_finish_gate, 128, 80 },
    { "House",        decor_house,       64, 80 },
    { "Sign Left",    decor_sign_left,   16, 32 },
    { "Sign Right",   decor_sign_right,  16, 32 },
    { "Cactus1",      decor_cactus1,     48, 96 },
    { "Cactus2",      decor_cactus2,     48, 96 },
    { "Cactus3",      decor_cactus3,     48, 96 },
    { "Oil Slick",    decor_oil_slick,   48, 24 },
    { "Bonus Boost",  decor_bonus_boost, 32, 32 },
    { "Bonus Shield", decor_bonus_shield,32, 32 },
    { "Bonus Shock",  decor_bonus_shock, 32, 32 }
};

static int decor_count = sizeof(decor_list) / sizeof(decor_list[0]);

// -----------------------------------------------------------------------------
// État du viewer
// -----------------------------------------------------------------------------
static int index = 0;
static float zoom = 1.0f;

// Message temporaire affiché quand on tente d'aller au-delà du premier ou du
// dernier sprite (ex: "PREMIER SPRITE" / "DERNIER SPRITE").
static const char* edge_msg = nullptr;
static float        edge_msg_timer = 0.0f;
static constexpr float kEdgeMsgDuration = 1.0f; // secondes
static constexpr float kFrameDt = 0.025f;       // cf task_kart.cpp (~40 FPS)

// -----------------------------------------------------------------------------
// Initialisation
// -----------------------------------------------------------------------------
void sprite_viewer_init() {
    index = 0;
    zoom = 1.0f;
    edge_msg = nullptr;
    edge_msg_timer = 0.0f;
}

// -----------------------------------------------------------------------------
// Mise à jour
// -----------------------------------------------------------------------------
void sprite_viewer_update() {
    using namespace core;

    if (edge_msg_timer > 0.0f) {
        edge_msg_timer -= kFrameDt;
        if (edge_msg_timer <= 0.0f) edge_msg = nullptr;
    }

    // Navigation
    if (input_was_pressed(Button::JoystickLeft)) {
        if (index > 0) {
            index--;
        } else {
            edge_msg = "PREMIER SPRITE";
            edge_msg_timer = kEdgeMsgDuration;
        }
    }
    if (input_was_pressed(Button::JoystickRight)) {
        if (index < decor_count - 1) {
            index++;
        } else {
            edge_msg = "DERNIER SPRITE";
            edge_msg_timer = kEdgeMsgDuration;
        }
    }

    // Zoom
    if (input_was_pressed(Button::A)) {
        zoom += 0.25f;
        if (zoom > 4.0f) zoom = 1.0f;
    }

    // Retour à l'écran titre (point d'entrée actuel du viewer)
    if (input_was_pressed(Button::B)) {
        mode = Mode::Title;
    }
}

// -----------------------------------------------------------------------------
// Rendu
// -----------------------------------------------------------------------------
void sprite_viewer_draw() {
    using namespace core;

    graphics_clear(Color::DarkBlue);

    const DecorEntry& e = decor_list[index];

    // Nom du sprite + position dans la liste (ex: "Tree1  (1/12)")
    char title[48];
    snprintf(title, sizeof(title), "%s  (%d/%d)", e.name, index + 1, decor_count);
    graphics_draw_text_center(20, title, Color::White);

    // Calcul de la taille affichée
    int dst_w = (int)(e.w * zoom);
    int dst_h = (int)(e.h * zoom);

    int x = 160 - dst_w / 2;
    int y = 120 - dst_h / 2;

    graphics_draw_bitmap565_scaled(x, y, dst_w, dst_h, e.data, e.w, e.h, true);

    // Message temporaire de bord de liste (prioritaire sur l'aide normale)
    if (edge_msg_timer > 0.0f && edge_msg != nullptr) {
        graphics_draw_text_center(190, edge_msg, Color::Yellow);
    }

    // Aide de navigation, toujours visible
    graphics_draw_text_center(210, "< / > : Naviguer   A: Zoom   B: Retour", Color::Gray);
}

} // namespace kart
