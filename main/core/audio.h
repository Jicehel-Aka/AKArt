#pragma once

namespace core {

void audio_init();

// À appeler chaque frame avec la vitesse courante (0..1) : fait varier la
// hauteur d'un bourdonnement moteur continu (retrigger périodique, simple
// mais suffisant sans synthèse audio complexe).
void audio_update_engine(float speed_ratio);

void sfx_jump();
void sfx_boost();
void sfx_lap();
void sfx_bump(); // collision entre karts

} // namespace core
