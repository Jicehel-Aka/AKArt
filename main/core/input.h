/*
===============================================================================
  input.h — Entrées utilisateur Akart
===============================================================================
*/
#pragma once

namespace core {

enum class Button {
    JoystickLeft,
    JoystickRight,
    A,
    B,
    C,
    D,
    L1,
    R1,
};

void input_init();
void input_poll();             // à appeler une fois par frame

bool input_is_held(Button b);      // état maintenu
bool input_was_pressed(Button b);  // front montant (cette frame)

} // namespace core
