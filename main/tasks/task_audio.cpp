/*
===============================================================================
  task_audio.cpp — Tâche audio (cadence stable)
-------------------------------------------------------------------------------
  Rôle :
    - Initialiser le moteur audio (I2S + DMA + pistes AKA)
    - Mettre à jour le mixage (PMF + SFX) à cadence fixe
    - Gérer les changements de musique via un système de commandes thread-safe
    - Garantir que le moteur audio reste indépendant du framerate du jeu

  Notes importantes :
    - Cette tâche est la SEULE autorisée à appeler :
        • audio_update()
        • audio_play_music_internal()
    - Le moteur PMF nécessite un timing stable (100–200 Hz).
    - Le jeu peut tourner à 40 FPS ou 60 FPS sans impacter l’audio.
    - Les autres tâches (GameTask, InputTask…) ne font que demander une musique
      via audio_request_music(), jamais la lancer directement.
===============================================================================
*/

#include "core/audio.h"
#include "game/game.h"        // game_state(), game_mode()
#include "music_map.h"        // MusicID, getMusicForLevel()
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

extern "C" void task_audio(void*)
{
    /*
    ---------------------------------------------------------------------------
      Initialisation du moteur audio AKA
      - Création du mutex de commande
      - Chargement des pistes (PMF, TONE, WAV)
      - Application du volume utilisateur
    ---------------------------------------------------------------------------
    */
    baba::audio_init();

    /*
    ---------------------------------------------------------------------------
      Musique d’écran titre
      (simple commande : l’exécution réelle se fera dans cette tâche)
    ---------------------------------------------------------------------------
    */
    baba::audio_request_music(MusicID::BABA_SAMBA);

    /*
    ---------------------------------------------------------------------------
      Boucle principale audio
      - Cadence fixe : 
      - Mise à jour du moteur PMF
      - Traitement des commandes de musique
    ---------------------------------------------------------------------------
    */
    while (true)
    {
        // Cadence stable pour le moteur PMF
        vTaskDelay(pdMS_TO_TICKS(18)); // 55 Hz
		
        // Mise à jour du mixage (PMF + SFX + WAV)
        baba::audio_update();

        /*
        -----------------------------------------------------------------------
          Lecture de la commande de musique
          - Protégée par mutex
          - Permet au jeu de demander une musique sans toucher au player
        -----------------------------------------------------------------------
        */
        MusicID requested = MusicID::NONE;

//        if (baba::g_audio_cmd_mutex)
        {
//            xSemaphoreTake(baba::g_audio_cmd_mutex, portMAX_DELAY);
            requested = baba::g_requested_music;
//            xSemaphoreGive(baba::g_audio_cmd_mutex);
        }

        /*
        -----------------------------------------------------------------------
          Exécution de la commande
          - Uniquement si la musique demandée est différente
          - Appel interne thread-safe : stop/load/play PMF
        -----------------------------------------------------------------------
        */
        if (requested != MusicID::NONE &&
            requested != baba::g_current_music)
        {
            baba::g_current_music = requested;
            baba::audio_play_music_internal(baba::g_current_music);
        }
    }
}
