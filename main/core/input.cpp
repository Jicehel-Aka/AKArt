/*
===============================================================================
  input.cpp — Gestion des entrées pour AKArt (moteur AKA)
-------------------------------------------------------------------------------
  Rôle :
    - Mapper les boutons abstraits (enum Button) vers les touches réelles
      du moteur AKA (gb_buttons::KEY_*).
    - S'appuyer sur g_core.pool() pour mettre à jour l'état matériel :
        * boutons (D-Pad / touches physiques, via l'expander I2C)
        * joystick analogique (émulation D-Pad par seuils, cf gb_core.cpp)
        * audio
        * timers internes
    - Fournir :
        * input_is_held()     → bouton maintenu
        * input_was_pressed() → front montant (press)

  NOTE IMPORTANTE (fix navigation peu fiable / "il faut appuyer trop
  longtemps") :
    gb_core expose DEUX sources indépendantes pour les directions
    gauche/droite/haut/bas :
      - g_core.buttons  : touches physiques lues via l'expander I2C
      - g_core.joystick : émulation D-Pad par seuils sur le stick analogique
    Ces deux objets maintiennent CHACUN leur propre historique (état
    précédent) pour calculer leurs fronts de pression respectifs. Le code
    d'origine ne lisait QUE g_core.buttons : si, sur le hardware réel, les
    directions remontent (aussi, ou uniquement) via g_core.joystick, aucune
    lecture ne les voyait jamais côté "front" — d'où une navigation qui
    semblait ne réagir qu'après un appui prolongé (en pratique: presque
    jamais un vrai front propre détecté).
    Pour corriger cela sans dépendre d'une hypothèse hardware précise, on
    COMBINE les deux sources (OR bit à bit) pour lire l'état "maintenu", et
    on calcule NOUS-MÊMES le front de pression à partir de cet état combiné
    (plutôt que de combiner button.pressed() | joystick.pressed(), qui sont
    deux détecteurs de front indépendants et peuvent se comporter de façon
    incohérente lorsqu'on les combine).
===============================================================================
*/

#include "input.h"
#include "gb_core.h"

extern gb_core g_core;

namespace core {

// -----------------------------------------------------------------------------
// map_button() — Convertit un bouton abstrait (enum Button) en touche AKA
// -----------------------------------------------------------------------------
static gb_buttons::gb_key map_button(Button b) {
    switch (b) {
        case Button::JoystickLeft:  return gb_buttons::KEY_LEFT;
        case Button::JoystickRight: return gb_buttons::KEY_RIGHT;

        case Button::A:             return gb_buttons::KEY_A;
        case Button::B:             return gb_buttons::KEY_B;
        case Button::C:             return gb_buttons::KEY_C;
        case Button::D:             return gb_buttons::KEY_D;

        case Button::L1:            return gb_buttons::KEY_L1;
        case Button::R1:            return gb_buttons::KEY_R1;

        // Ajout pour le menu interne et le retour loader
        case Button::Menu:          return gb_buttons::KEY_MENU;
        case Button::Run:           return gb_buttons::KEY_RUN;
    }

    // Fallback (ne devrait jamais arriver)
    return gb_buttons::KEY_A;
}

// Nombre de valeurs dans l'enum Button (doit rester synchronisé avec input.h).
static constexpr int kButtonCount = (int)Button::Run + 1;

// État combiné (boutons | joystick) mémorisé frame par frame, pour calculer
// nos propres fronts de pression de façon fiable et cohérente.
static bool s_held_now[kButtonCount]  = {};
static bool s_held_prev[kButtonCount] = {};

// -----------------------------------------------------------------------------
// input_init() — Rien à faire : g_core.init() configure déjà le hardware
// -----------------------------------------------------------------------------
void input_init() {
    // Le moteur AKA initialise déjà :
    //  - expander
    //  - joystick
    //  - boutons
    //  - timers
    // Donc aucune initialisation supplémentaire n'est nécessaire ici.
}

// -----------------------------------------------------------------------------
// input_calibrate_joystick() — Recalibre le centre du joystick sur sa position
// actuelle. La lib gb_core expose déjà gb_joystick::calibrate_center() (cf.
// gb_core.h) : on ne fait que l'appeler au bon moment (joystick supposé au
// repos), pas besoin de réimplémenter la logique de seuils/marge nous-mêmes.
// -----------------------------------------------------------------------------
void input_calibrate_joystick() {
    g_core.joystick.calibrate_center();
}

// -----------------------------------------------------------------------------
// input_poll() — Met à jour l'état matériel puis notre snapshot combiné
// -----------------------------------------------------------------------------
void input_poll() {
    // g_core.pool() met à jour :
    //  - boutons
    //  - joystick
    //  - audio
    //  - timers internes
    g_core.pool();

    // État matériel combiné : D-Pad physique OU émulation D-Pad du joystick
    // analogique. Voir la note en tête de fichier.
    const uint16_t combined = g_core.buttons.state() | g_core.joystick.state();

    for (int i = 0; i < kButtonCount; ++i) {
        const bool held = (combined & (uint16_t)map_button((Button)i)) != 0;
        s_held_prev[i] = s_held_now[i];
        s_held_now[i]  = held;
    }
}

// -----------------------------------------------------------------------------
// input_is_held() — Bouton maintenu
// -----------------------------------------------------------------------------
bool input_is_held(Button b) {
    return s_held_now[(int)b];
}

// -----------------------------------------------------------------------------
// input_was_pressed() — Front montant (press), calculé sur l'état combiné
// -----------------------------------------------------------------------------
bool input_was_pressed(Button b) {
    int i = (int)b;
    return s_held_now[i] && !s_held_prev[i];
}

} // namespace core
