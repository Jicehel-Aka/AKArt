#pragma once

/*
============================================================
  task_audio.h — Tâche audio (cadence stable)
------------------------------------------------------------
Cette tâche exécute :
 - audio_init() : initialisation du moteur audio
 - audio_update() : mixage, PMF, SFX

Elle tourne à cadence fixe (ex: 200 Hz) pour garantir une
qualité audio stable, indépendamment du framerate du jeu.

NOTE :
 - C’est la SEULE tâche autorisée à appeler audio_update().
============================================================
*/

#ifdef __cplusplus
extern "C" {
#endif

void task_audio(void* param);

#ifdef __cplusplus
}
#endif

