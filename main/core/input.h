/*
===============================================================================
  input.h — API d’entrée pour AKArt (moteur AKA)
-------------------------------------------------------------------------------
  Fournit :
    - input_init()       → initialisation logique (hardware déjà géré par g_core)
    - input_poll()       → mise à jour des boutons (via g_core.pool())
    - input_is_held()    → bouton maintenu
    - input_was_pressed()→ front montant (press)
===============================================================================
*/

#pragma once
#include <cstdint>

namespace core {

// Boutons abstraits utilisés par le jeu
enum class Button {
    JoystickLeft,
    JoystickRight,
    A, B, C, D,
    L1, R1
};

// Initialisation logique (le hardware est déjà géré par g_core.init())
void input_init();

// Mise à jour de l’état des boutons (appelle g_core.pool())
void input_poll();

// Bouton maintenu
bool input_is_held(Button b);

// Front montant (press)
bool input_was_pressed(Button b);

} // namespace core
