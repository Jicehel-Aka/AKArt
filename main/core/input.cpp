/*
===============================================================================
  input.cpp — Gestion des entrées pour AKArt (moteur AKA)
-------------------------------------------------------------------------------
  Rôle :
    - Mapper les boutons abstraits (enum Button) vers les touches réelles
      du moteur AKA (gb_buttons::KEY_*).
    - S'appuyer sur g_core.pool() pour mettre à jour l’état matériel :
        * boutons
        * joystick
        * expander
        * timers internes
    - Fournir :
        * input_is_held()     → bouton maintenu
        * input_was_pressed() → front montant (press)
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
    }
    return gb_buttons::KEY_A; // fallback
}

// -----------------------------------------------------------------------------
// input_init() — Rien à faire : g_core.init() configure déjà le hardware
// -----------------------------------------------------------------------------
void input_init() {
    // Le moteur AKA initialise déjà :
    //  - expander
    //  - joystick
    //  - boutons
    //  - timers
    // Donc aucune initialisation supplémentaire n’est nécessaire ici.
}

// -----------------------------------------------------------------------------
// input_poll() — Met à jour l’état des boutons
// -----------------------------------------------------------------------------
void input_poll() {
    // g_core.pool() met à jour :
    //  - boutons
    //  - joystick
    //  - audio
    //  - timers internes
    g_core.pool();
}

// -----------------------------------------------------------------------------
// input_is_held() — Bouton maintenu
// -----------------------------------------------------------------------------
bool input_is_held(Button b) {
    return (g_core.buttons.state() & (uint16_t)map_button(b)) != 0;
}

// -----------------------------------------------------------------------------
// input_was_pressed() — Front montant (press)
// -----------------------------------------------------------------------------
bool input_was_pressed(Button b) {
    return g_core.buttons.pressed(map_button(b));
}

} // namespace core
