/*
===============================================================================
  task_input.cpp — Tâche input (100 Hz)
-------------------------------------------------------------------------------
  Rôle :
    - Interroger le module d’entrée (input_poll).
    - Mettre à jour l’état global des touches (g_keys).
    - Fournir une fréquence d’échantillonnage élevée (100 Hz) pour garantir
      une excellente réactivité, même si la boucle de jeu tourne à 60 FPS.

  Notes :
    - Cette tâche tourne en parallèle du moteur de jeu.
    - input_poll() lit l’état matériel des boutons et du joystick.
    - Le debug RAW peut être activé pour visualiser les états bruts.

  Auteur : Jean-Charles LEBEAU
  Date   : Janvier 2026
===============================================================================
*/

#include "task_input.h"
#include "core/input.h"
#include "esp_timer.h"
#include "esp_rom_sys.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "gamebuino.h"

extern gb_core g_core;
namespace baba {


} // namespace baba
