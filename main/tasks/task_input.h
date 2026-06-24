/*
============================================================
  task_input.h — Tâche input (100 Hz)
------------------------------------------------------------
Cette tâche exécute :
 - input_poll(g_keys)

Elle met à jour l’état global des touches 100 fois par seconde,
ce qui garantit une excellente réactivité même si le jeu tourne
à 60 FPS.
============================================================
*/

#pragma once
#include "core/input.h"

