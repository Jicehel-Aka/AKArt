/*
===============================================================================
  input.h — API d’entrée pour AKArt (moteur AKA)
-------------------------------------------------------------------------------
  Rôle :
    - Fournir une couche d’abstraction stable pour les boutons du jeu.
    - Les touches hardware (GB_KEY_*) sont mappées ici vers un enum Button
      utilisé par tout le moteur (kart_game, menus, HUD, etc.).
    - L’état des boutons est mis à jour via input_poll(), qui appelle g_core.pool().
===============================================================================
*/

#pragma once
#include <cstdint>

namespace core {

// -----------------------------------------------------------------------------
// Boutons abstraits utilisés par le jeu
// -----------------------------------------------------------------------------
enum class Button {
    JoystickLeft,
    JoystickRight,

    A, B, C, D,

    L1, R1,

    // Ajout pour le menu interne et le retour loader
    Menu,
    Run
};

// -----------------------------------------------------------------------------
// Initialisation logique (le hardware est déjà géré par g_core.init())
// -----------------------------------------------------------------------------
void input_init();

// Calibre le centre du joystick sur sa position ACTUELLE (à appeler quand le
// joystick est au repos — typiquement au démarrage, cf. app_main.cpp). Sans
// ça, le "centre" supposé peut être légèrement décalé de la vraie position
// de repos du stick (tolérances mécaniques), ce qui rend gauche/droite
// asymétriques ou peu fiables près du centre.
void input_calibrate_joystick();

// -----------------------------------------------------------------------------
// Mise à jour de l’état des boutons (appelle g_core.pool())
// -----------------------------------------------------------------------------
void input_poll();

// -----------------------------------------------------------------------------
// Bouton maintenu
// -----------------------------------------------------------------------------
bool input_is_held(Button b);

// -----------------------------------------------------------------------------
// Front montant (press)
// -----------------------------------------------------------------------------
bool input_was_pressed(Button b);

} // namespace core
