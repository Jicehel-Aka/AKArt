#include "input.h"
#include "gb_core.h"

extern gb_core g_core;

namespace core {

static gb_buttons::gb_key map_button(Button b) {
    switch (b) {
        case Button::JoystickLeft:  return gb_buttons::KEY_LEFT;
        case Button::JoystickRight: return gb_buttons::KEY_RIGHT;
        case Button::A:  return gb_buttons::KEY_A;
        case Button::B:  return gb_buttons::KEY_B;
        case Button::C:  return gb_buttons::KEY_C;
        case Button::D:  return gb_buttons::KEY_D;
        case Button::L1: return gb_buttons::KEY_L1;
        case Button::R1: return gb_buttons::KEY_R1;
    }
    return gb_buttons::KEY_A;
}

void input_init() { /* rien à faire : g_core.init() gère le matériel */ }

void input_poll() { g_core.pool(); }

bool input_is_held(Button b) {
    return (g_core.buttons.state() & (uint16_t)map_button(b)) != 0;
}

bool input_was_pressed(Button b) {
    return g_core.buttons.pressed(map_button(b));
}

} // namespace core
