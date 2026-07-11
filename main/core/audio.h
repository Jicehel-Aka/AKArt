#pragma once
#include <cstdint>

namespace core {

void audio_init();

// Volume principal (0..255) — utilisé par le menu options.
void audio_set_master_volume(uint8_t volume);

// --- Musique de fond (fichiers .pmf, format tracker Gamebuino) -------------
// Charge et joue un module musical en boucle. `pmf_data` doit rester
// accessible tant que la musique joue (c'est un pointeur brut vers un
// tableau de données, comme les sprites : cf. assets/gfx/*.h). `pmf_len` à 0
// (ou données absentes) => ne fait rien, ne joue pas de silence bruité.
// Appeler à nouveau avec un autre pointeur change de morceau (utile pour
// changer de musique selon le thème du circuit ou l'écran affiché).
void audio_play_music(const void* pmf_data, unsigned int pmf_len);
void audio_stop_music();
void audio_set_music_volume(float volume); // 0.0 .. 1.0

// Son moteur : à appeler chaque frame avec la vitesse courante (0..1).
// Fait varier la hauteur d'un bourdonnement continu selon la vitesse.
void audio_update_engine(float speed_ratio);

void sfx_jump();            // saut (tremplin ou bouton)
void sfx_boost();           // turbo / boost activé
void sfx_lap();             // franchissement d'une ligne de tour
void sfx_bump();            // collision entre karts (légère)
void sfx_crash_hard();      // collision violente (adversaire ou décor)
void sfx_offtrack();        // sortie de route / herbe sous les roues
void sfx_accel_start();     // démarrage (accélération de 0)

// Nouveaux sons — bonus/malus (cf. kart_engine.cpp)
void sfx_item_pickup();     // ramassage d'une bonus box
void sfx_shield_up();       // activation du bouclier
void sfx_shield_block();    // le bouclier absorbe un impact (huile/collision/choc)
void sfx_shock_use();       // utilisation de l'onde de choc (celui qui l'envoie)
void sfx_shock_hit();       // touché par une onde de choc (celui qui la subit)
void sfx_oil_spinout();     // dérapage forcé sur une flaque d'huile
void sfx_drift_tier_up();   // le mini-turbo passe au palier suivant (retour de charge)

// Compte à rebours de départ (3-2-1-GO)
void sfx_countdown_beep();  // à chaque chiffre décompté (3, 2, 1)
void sfx_countdown_go();    // au "GO !" final

} // namespace core
