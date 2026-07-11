/*
===============================================================================
  kart_game.cpp — Boucle de jeu principale (AKArt)
-------------------------------------------------------------------------------
  Rôle :
    - Gérer les différents modes du jeu :
         * Title      → écran de titre
         * Race       → course
         * Pause      → menu pause (overlay)
         * Results    → écran de fin de course
    - Appeler :
         * kart_engine (update_all)
         * kart_render (draw_race)
         * title_screen
         * pause_menu
         * audio_update_engine
===============================================================================
*/

#include "kart_game.h"
#include "kart_types.h"
#include "kart_engine.h"
#include "kart_render.h"
#include "title_screen.h"
#include "track_registry.h"
#include "music_registry.h"
#include "pause_menu.h"        // ← ajouté
#include "sprite_viewer.h"     // ← ajouté
#include "../core/input.h"
#include "../core/audio.h"
#include "../core/graphics.h"
#include <cstdio>
#include <algorithm>
#include <cmath>

namespace kart {

// -----------------------------------------------------------------------------
// Modes du jeu
// -----------------------------------------------------------------------------
// NOTE : l'enum `Mode` et la variable `mode` sont déclarés dans kart_game.h
// (et non plus en `static` ici) car pause_menu.cpp et sprite_viewer.cpp ont
// besoin d'y accéder. `mode` est donc DÉFINI ici (une seule fois).
Mode  mode          = Mode::Title;
static float title_timer   = 0.0f;
static float results_timer = 0.0f;

// -----------------------------------------------------------------------------
// Championnat (préparation pour les coupes multi-circuits à venir)
// -----------------------------------------------------------------------------
// Nombre total de karts sur la piste (1 joueur + NUM_AI IA). Centralisé ici
// pour ne plus avoir de "4"/"3" en dur disséminés dans tout le fichier —
// c'est ce qui rendait le passage de 4 à 8 karts fastidieux et source
// d'oublis (grille, tableaux championnat, barème de points...).
// -----------------------------------------------------------------------------
static const int NUM_KARTS = 8;
static const int NUM_AI    = NUM_KARTS - 1;

// -----------------------------------------------------------------------------
// Index stable 0=joueur, 1..NUM_AI=les IA (même ordre que karts[], qui est
// toujours reconstruit dans le même ordre à chaque kart_game_init()).
// Persiste tant que l'appli tourne (plusieurs courses d'affilée) ; remis à
// zéro uniquement via reset_championship() (pas encore appelée nulle part
// pour l'instant — à brancher sur un futur menu "Nouvelle coupe").
static int   champ_points[NUM_KARTS] = {0};
static float champ_time[NUM_KARTS]   = {0};
static int   champ_races_done = 0;

static int selected_cup = 0;   // coupe choisie sur l'écran titre (Joystick G/D)
static int cup_race_index = 0; // course courante DANS la coupe (0 = première)

// --- Préférences (menu options) ---
static core::KartColor player_color = core::KartColor::Red;
static int master_volume = 200; // 0..255, cf. audio_set_master_volume()

static float race_elapsed = 0.0f; // chrono de la course en cours (remis à 0 à chaque départ)

// Compte à rebours de départ (3-2-1-GO) : tant que > 0, la physique/les
// entrées de conduite sont gelées (les karts restent sur la grille), seul le
// HUD de compte à rebours évolue. 0 = course en cours normalement.
const float COUNTDOWN_DURATION = 4.0f;
static float countdown_timer = 0.0f;

void reset_championship() {
    for (int i = 0; i < NUM_KARTS; ++i) { champ_points[i] = 0; champ_time[i] = 0.0f; }
    champ_races_done = 0;
}

void kart_abandon_cup() {
    cup_race_index = 0;
}

// Calcule l'ordre de grille (qui démarre devant/derrière) pour la PROCHAINE
// course. Renvoie, pour chaque indice de kart (0=joueur, 1..NUM_AI=IA), sa
// position sur la grille (0=pole/en tête, NUM_AI=dernier).
//   - Course 1 (champ_races_done==0) : grille fixe, le joueur part TOUJOURS
//     dernier (comportement d'origine du jeu).
//   - Course 2+ : ordre du championnat (points décroissants, puis temps
//     cumulé croissant en cas d'égalité — le plus rapide gagne l'égalité).
static void compute_grid_order(int grid_pos[NUM_KARTS]) {
    if (champ_races_done == 0) {
        // Grille fixe d'origine : joueur (0) dernier, IA 1..NUM_AI devant
        // dans l'ordre de leur indice.
        grid_pos[0] = NUM_AI;
        for (int i = 1; i < NUM_KARTS; ++i) grid_pos[i] = i - 1;
        return;
    }

    int idx[NUM_KARTS];
    for (int i = 0; i < NUM_KARTS; ++i) idx[i] = i;
    std::sort(idx, idx + NUM_KARTS, [](int a, int b) {
        if (champ_points[a] != champ_points[b])
            return champ_points[a] > champ_points[b];
        return champ_time[a] < champ_time[b]; // le plus rapide gagne l'égalité
    });
    for (int pos = 0; pos < NUM_KARTS; ++pos)
        grid_pos[idx[pos]] = pos;
}

// Thème de circuit choisi sur l'écran titre (Joystick G/D) — persiste tant
// que l'appli tourne, pris en compte au (re)lancement d'une course.
// (selected_cup / cup_race_index déclarés plus haut, avant reset_championship)

// --- Jingles (dernier tour / arrivée) ---
// Musique de fond normale coupée temporairement pour laisser jouer le
// jingle (l'API audio ne gère qu'UNE piste à la fois, cf. audio.h), puis
// restaurée. Durées choisies par défaut (jingles courts) — à ajuster une
// fois entendues sur le hardware réel si besoin.
static int   prev_player_lap        = 0;
static float lastlap_jingle_timer   = 0.0f;
const float  LASTLAP_JINGLE_DURATION = 3.0f;

static bool  finishing              = false;
static float finish_jingle_timer    = 0.0f;
const float  FINISH_JINGLE_DURATION = 3.0f;

// --- Capture d'écran (Menu maintenu ~0.6s) ---
static float menu_hold_time   = 0.0f;
static bool  menu_shot_done   = false;
static bool  menu_was_held    = false;
static float shot_toast_timer = 0.0f;
static char  shot_toast_text[64] = "";

static Track                  track;
static std::vector<KartState> karts;
static Camera                 cam;
static std::vector<float>     item_box_cooldowns; // un par segment, cf. kart_engine.h

static const float MAX_SPEED_AUDIO = 7.0f;

// -----------------------------------------------------------------------------
// Initialisation (appelée aussi pour reset complet)
// -----------------------------------------------------------------------------
void kart_game_init() {
    const Cup& cup = kCups[selected_cup];
    int track_idx = cup.track_indices[cup_race_index];
    track = kTrackList[track_idx].build();
    static int s_track_generation_counter = 0;
    track.generation = ++s_track_generation_counter;
    karts.clear();
    mode = Mode::Title;
    race_elapsed = 0.0f;
    item_box_cooldowns.assign(track.segs.size(), 0.0f);

    prev_player_lap      = 0;
    lastlap_jingle_timer  = 0.0f;
    finishing             = false;
    finish_jingle_timer   = 0.0f;

    // Grille de départ : position 0 = pole (le plus proche de la ligne),
    // position 3 = dernier (le plus loin derrière). Course 1 = grille fixe
    // (joueur dernier), courses suivantes = ordre du championnat (cf.
    // compute_grid_order()).
    //
    // IMPORTANT : la grille doit être placée DERRIÈRE le portique START
    // (segment z=0), jamais à z=0 ni au-delà. L'ancienne formule
    // `(3 - grid_pos) * GRID_SPACING` plaçait tout le monde ENTRE 0 et 3
    // segments APRÈS la ligne (le dernier exactement dessus, le premier 3
    // segments plus loin) : le portique de départ n'était donc jamais visible
    // au lancement de la course (déjà "passé" pour tout le monde). On
    // recule maintenant tout le monde d'1 à 4 segments AVANT la ligne
    // (wrap sur la fin du circuit), pour que le portique soit bien visible
    // devant, comme sur une vraie grille de départ.
    const float GRID_SPACING = 80.0f;
    int grid_pos[NUM_KARTS];
    compute_grid_order(grid_pos);

    auto grid_start_z = [&](int pos) {
        float z = track.total_length - (pos + 1) * GRID_SPACING;
        if (z < 0.0f) z += track.total_length;
        return z;
    };

    // Caméra
    cam.height       = 100.0f;
    cam.fov          = 120.0f;
    cam.offset_x     = 0.0f;
    cam.cockpit_view = false;
    cam.shake        = 1.0f;
    cam.angle        = 0.0f;
    camera_setup(cam);

    // Joueur
    KartState p{};
    p.type       = KartType::Player;
    p.color      = player_color;
    p.x          = 0.0f;
    p.z          = grid_start_z(grid_pos[0]);
    p.y          = 0.0f;
    p.vy         = 0.0f;
    p.speed      = 0.0f;
    p.seg_index  = find_segment(track, p.z);
    p.lap        = 0;
    p.rank       = 1;
    p.drift      = 0.0f;
    p.drifting   = false;
    p.on_ground  = true;
    p.bonus      = BonusType::None;
    p.has_boost  = false;
    p.boost_timer= 0.0f;
    p.radius     = 0.15f;
    p.angle      = 0.0f;
    p.finished   = false;
    p.score      = 0;
    p.score_awarded = false;
    p.off_track_timer = 0.0f;
    karts.push_back(p);

    // IA — couleurs assignées parmi celles qui restent (celle du joueur
    // exclue, pour ne jamais avoir deux karts de la même couleur sur la
    // piste). 8 couleurs disponibles pour NUM_AI IA : avec NUM_AI=7, il en
    // reste exactement 7 après avoir exclu celle du joueur — chacune est
    // donc utilisée exactement une fois.
    core::KartColor ai_colors[NUM_AI];
    {
        int ci = 0;
        for (int c = 0; c < core::kKartColorCount && ci < NUM_AI; ++c) {
            if ((core::KartColor)c != player_color) ai_colors[ci++] = (core::KartColor)c;
        }
    }

    for (int i = 0; i < NUM_AI; ++i) {
        KartState ai{};
        ai.type       = KartType::AI;
        ai.color      = ai_colors[i];
        ai.x          = ((i % 3) - 1) * 0.3f;
        ai.z          = grid_start_z(grid_pos[i + 1]);
        ai.y          = 0.0f;
        ai.vy         = 0.0f;
        ai.speed      = 0.0f;
        ai.seg_index  = find_segment(track, ai.z);
        ai.lap        = 0;
        ai.rank       = i + 2;
        ai.drift      = 0.0f;
        ai.drifting   = false;
        ai.on_ground  = true;
        ai.bonus      = BonusType::None;
        ai.has_boost  = false;
        ai.boost_timer= 0.0f;
        ai.radius     = 0.15f;
        ai.angle      = 0.0f;
        ai.finished   = false;
        ai.score      = 0;
        ai.score_awarded = false;
        ai.off_track_timer = 0.0f;
        karts.push_back(ai);
    }
}

// -----------------------------------------------------------------------------
// Mise à jour
// -----------------------------------------------------------------------------
static void options_menu_update();
static void options_menu_draw();

void kart_game_update(float dt) {
    using namespace core;

    // --- Capture d'écran (Menu maintenu ~0.6s) / Pause (Menu appui court) --
    // Un seul et même bouton Menu sert aux deux usages, distingués par la
    // durée d'appui :
    //   - maintenu >= 0.6s  → capture d'écran (comportement existant)
    //   - relâché avant 0.6s, en course → bascule en pause
    // On libère ainsi B (précédemment utilisé pour la pause) pour un usage
    // futur (menu d'options, drift...), sans toucher au combo RUN+MENU
    // (retour loader, géré à part dans app_main.cpp).
    if (shot_toast_timer > 0.0f) shot_toast_timer -= dt;

    bool menu_held = input_is_held(Button::Menu);
    bool run_held  = input_is_held(Button::Run);
    if (menu_held && !run_held) {
        menu_hold_time += dt;
        if (!menu_shot_done && menu_hold_time >= 0.6f) {
            menu_shot_done = true;
            char path[64];
            bool ok = graphics_save_screenshot_bmp(path, sizeof(path));
            if (ok)
                // %.48s limite la taille du chemin copié pour éviter tout warning
                // de troncature (path fait jusqu'à 63 car., shot_toast_text 64)
                snprintf(shot_toast_text, sizeof(shot_toast_text), "Capture : %.48s", path);
            else
                snprintf(shot_toast_text, sizeof(shot_toast_text), "Capture SD : echec");
            shot_toast_timer = 2.0f;
        }
    } else {
        // Relâchement (ou Run maintenu en même temps, auquel cas on ignore
        // complètement Menu ici : c'est le combo loader qui prend le relais).
        bool short_tap = menu_was_held && !menu_shot_done &&
                         menu_hold_time > 0.0f && menu_hold_time < 0.6f && !run_held;
        if (short_tap) {
            if (mode == Mode::Race) mode = Mode::Pause;
            else if (mode == Mode::Title) mode = Mode::Options;
            else if (mode == Mode::Options) mode = Mode::Title;
        }
        menu_hold_time = 0.0f;
        menu_shot_done = false;
    }
    menu_was_held = menu_held;

    // --- Écran de titre ---
    if (mode == Mode::Title) {
        title_timer += dt;

        // Joystick G/D : cycle parmi les pistes du registre (track_registry.h).
        // Chaque piste porte déjà son thème (Track::sky_theme), donc plus
        // besoin de choisir le thème séparément.
        if (input_was_pressed(Button::JoystickLeft)) {
            selected_cup = (selected_cup - 1 + kCupCount) % kCupCount;
        }
        if (input_was_pressed(Button::JoystickRight)) {
            selected_cup = (selected_cup + 1) % kCupCount;
        }

        static float a_held = 0.0f;
        if (input_is_held(Button::A)) a_held += dt;
        else a_held = 0.0f;

        if (input_was_pressed(Button::A) || a_held >= 0.08f) {
            title_timer = 0.0f;
            cup_race_index = 0;
            reset_championship(); // nouvelle coupe : on repart de zéro
            // Reconstruit la piste (avec le thème choisi), les karts et la
            // caméra pour une course fraîche. kart_game_init() repasse le
            // mode à Title en interne : on le corrige juste après.
            kart_game_init();
            mode = Mode::Race;
            countdown_timer = COUNTDOWN_DURATION;
            auto m = music_for_theme(track.sky_theme);
            audio_play_music(m.data, m.len);
            return;
        }

        // B → Sprite Viewer (cf. texte affiché sur title_screen.cpp).
        // Options s'ouvre désormais via Menu (appui court), pas B — cohérent
        // avec l'usage de Menu partout ailleurs dans le jeu.
        if (input_was_pressed(Button::B)) {
            sprite_viewer_init();
            mode = Mode::SpriteViewer;
        }
        return;
    }

    // --- Options ---
    if (mode == Mode::Options) {
        options_menu_update();
        return;
    }

    // --- Sprite Viewer ---
    if (mode == Mode::SpriteViewer) {
        sprite_viewer_update();   // B (dans le viewer) ramène mode = Mode::Title
        return;
    }

    // --- Écran de résultats ---
    if (mode == Mode::Results) {
        results_timer += dt;

        if (results_timer >= 8.0f || input_was_pressed(Button::A)) {
            const Cup& cup = kCups[selected_cup];
            cup_race_index++;

            if (cup_race_index < cup.track_count) {
                // Course suivante de la MÊME coupe : on enchaîne directement
                // (le championnat, lui, n'est PAS remis à zéro entre les
                // courses d'une coupe — seulement au tout début d'une coupe,
                // cf. plus haut).
                kart_game_init();
                mode = Mode::Race;
                countdown_timer = COUNTDOWN_DURATION;
                auto m = music_for_theme(track.sky_theme);
                audio_play_music(m.data, m.len);
            } else {
                // Coupe terminée : retour au titre, prêt pour une nouvelle coupe.
                cup_race_index = 0;
                kart_game_init();
                mode = Mode::Title;
                auto m = music_title_ref();
                audio_play_music(m.data, m.len);
            }
        }
        return;
    }

    // --- Menu Pause ---
    if (mode == Mode::Pause) {
        pause_menu_update();   // ← nouveau
        return;
    }

    // --- Course ---
    // (Pause désormais déclenchée par un appui court sur Menu, cf. plus haut.
    // B est libre pour un usage futur : options, drift...)

    // Compte à rebours de départ : la physique reste gelée, on ne fait que
    // décompter (le joueur/l'IA restent immobiles sur la grille de départ).
    if (countdown_timer > 0.0f) {
        float prev = countdown_timer;
        countdown_timer -= dt;
        // Bip à chaque passage de seconde entière (3, 2, 1), son différent
        // pour le "GO" final (cf. audio.h : sfx_countdown_beep/sfx_countdown_go).
        if ((int)prev != (int)countdown_timer && countdown_timer > 0.0f) {
            core::sfx_countdown_beep();
        } else if (prev > 0.0f && countdown_timer <= 0.0f) {
            core::sfx_countdown_go();
        }
        return;
    }

    // R1 → cockpit view
    if (input_was_pressed(Button::R1))
        cam.cockpit_view = !cam.cockpit_view;

    race_elapsed += dt;

    // Physique + IA
    update_all(karts, track, dt, item_box_cooldowns);

    // Jingle "dernier tour" : déclenché une seule fois, au moment précis où
    // le JOUEUR entame son dernier tour (transition, pas un état continu).
    if (karts[0].lap != prev_player_lap) {
        if (!karts[0].finished && karts[0].lap == track.laps - 1) {
            lastlap_jingle_timer = LASTLAP_JINGLE_DURATION;
            auto j = jingle_lastlap_ref();
            audio_play_music(j.data, j.len);
        }
        prev_player_lap = karts[0].lap;
    }
    if (lastlap_jingle_timer > 0.0f) {
        lastlap_jingle_timer -= dt;
        if (lastlap_jingle_timer <= 0.0f) {
            // Jingle terminé : on restaure la musique de course normale.
            auto m = music_for_theme(track.sky_theme);
            audio_play_music(m.data, m.len);
        }
    }

    // Mémorise le temps de course de chaque kart dès qu'il termine (utile
    // pour le classement du championnat, cf. plus bas).
    for (auto& k : karts) {
        if (k.finished && k.finish_race_time < 0.0f) {
            k.finish_race_time = race_elapsed;
        }
    }

    // Jingle "arrivée" : joué dès que le JOUEUR termine, en retardant de
    // quelques secondes le passage à l'écran de résultats (et la musique
    // music_results qui va avec) pour lui laisser le temps de jouer — sans
    // ce délai, music_results coupait le jingle instantanément.
    if (karts[0].finished && !finishing) {
        finishing = true;
        finish_jingle_timer = FINISH_JINGLE_DURATION;
        auto j = jingle_finish_ref();
        audio_play_music(j.data, j.len);
    }

    if (finishing) {
        finish_jingle_timer -= dt;
        if (finish_jingle_timer > 0.0f) {
            return; // laisse le jingle jouer ; la course continue de se
                    // dessiner normalement (karts en décélération post-ligne,
                    // cf. kart_engine.cpp) sans encore basculer en Results.
        }
    }

    // Fin de course
    if (karts[0].finished) {
        // La course s'arrête dès que le JOUEUR termine : les IA pas encore
        // arrivées à ce moment-là gardent leur rang/score actuels comme
        // résultat final (elles ne recevaient sinon jamais leurs points,
        // cf. score_awarded plus bas qui ne s'appliquait qu'aux karts
        // "finished"). On fige donc ici le résultat de TOUT le monde.
        for (auto& k : karts) {
            if (!k.score_awarded) {
                int rank_idx = std::clamp(k.rank - 1, 0, NUM_KARTS - 1);
                k.score += track.points_by_rank[rank_idx];
                k.score_awarded = true;
            }
            if (k.finish_race_time < 0.0f) k.finish_race_time = race_elapsed;
        }

        // Championnat : cumule points + temps de cette course (préparation
        // pour les coupes multi-circuits — cf. compute_grid_order()).
        for (int i = 0; i < NUM_KARTS && i < (int)karts.size(); ++i) {
            champ_points[i] += karts[i].score;
            champ_time[i]   += karts[i].finish_race_time;
        }
        champ_races_done++;

        mode = Mode::Results;
        results_timer = 0.0f;
        {
            auto m = music_results_ref();
            audio_play_music(m.data, m.len);
        }
        return;
    }

    // Caméra + audio
    cam.angle = karts[0].angle;
    audio_update_engine(karts[0].speed / MAX_SPEED_AUDIO);

    // Le tremblement scale maintenant en continu avec la vitesse (au lieu
    // d'un simple 1.0/3.0 tout ou rien) : ça renforce la sensation de
    // vitesse aux hauts régimes, en plus du bump existant en drift/boost.
    float speed_ratio = karts[0].speed / MAX_SPEED_AUDIO;
    float base_shake = 1.0f + speed_ratio * 1.2f;
    cam.shake = (karts[0].drifting || karts[0].has_boost) ? std::max(base_shake, 3.0f) : base_shake;
}

// -----------------------------------------------------------------------------
// Écran de résultats
// -----------------------------------------------------------------------------
static void draw_results() {
    using namespace core;

    graphics_clear(Color::DarkBlue);
    graphics_draw_text_center(16, "FIN DE COURSE !", Color::Yellow);

    const KartState& p = karts[0];
    char buf[48];

    snprintf(buf, sizeof(buf), "Votre rang : %d / %d", p.rank, (int)karts.size());
    graphics_draw_text_center(34, buf, Color::White);

    snprintf(buf, sizeof(buf), "Points gagnes : %d", p.score);
    graphics_draw_text_center(50, buf, Color::White);

    graphics_draw_text_center(66, "Bareme :", Color::LightGray);
    const char* medals[] = {
        "1er  : 10 pts", "2eme :  8 pts", "3eme :  6 pts", "4eme :  5 pts",
        "5eme :  4 pts", "6eme :  3 pts", "7eme :  2 pts", "8eme :  1 pt "
    };
    for (int i = 0; i < NUM_KARTS; ++i) {
        Color c = (p.rank == i + 1) ? Color::Yellow : Color::Gray;
        graphics_draw_text_center(78 + i * 11, medals[i], c);
    }

    // Championnat (préparation coupes multi-circuits) : total cumulé sur
    // toutes les courses jouées depuis le lancement de l'appli.
    snprintf(buf, sizeof(buf), "Championnat : %d pts (course %d)", champ_points[0], champ_races_done);
    graphics_draw_text_center(196, buf, Color::Cyan);

    bool blink = (int)(results_timer * 2.0f) % 2 == 0;
    if (blink)
        graphics_draw_text_center(210, "A : retour au menu", Color::Green);
}

// -----------------------------------------------------------------------------
// Toast "capture d'écran" (overlay, tous modes confondus)
// -----------------------------------------------------------------------------
static void draw_screenshot_toast() {
    if (shot_toast_timer > 0.0f) {
        core::graphics_draw_text_center(230, shot_toast_text, core::Color::Yellow);
    }
}

// -----------------------------------------------------------------------------
// Rendu
// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------
// Menu Options — volume principal + couleur du kart joueur.
// Joystick G/D : volume. A : cycle la couleur. B : retour au titre.
// -----------------------------------------------------------------------------
static void options_menu_update() {
    using namespace core;

    if (input_was_pressed(Button::JoystickLeft)) {
        master_volume = std::max(0, master_volume - 15);
        audio_set_master_volume((uint8_t)master_volume);
    }
    if (input_was_pressed(Button::JoystickRight)) {
        master_volume = std::min(255, master_volume + 15);
        audio_set_master_volume((uint8_t)master_volume);
    }
    if (input_was_pressed(Button::A)) {
        player_color = (core::KartColor)(((int)player_color + 1) % core::kKartColorCount);
    }
    // Retour au titre : Menu (appui court), cf. le bloc partagé plus haut
    // dans kart_game_update() — cohérent avec le reste du jeu (Menu = pause/
    // options/retour partout, jamais B).
}

static const char* kart_color_name(core::KartColor c) {
    switch (c) {
        case core::KartColor::Red:      return "Rouge";
        case core::KartColor::Blue:     return "Bleu";
        case core::KartColor::Yellow:   return "Jaune";
        case core::KartColor::Green:    return "Vert";
        case core::KartColor::Pink:     return "Rose";
        case core::KartColor::Gray:     return "Gris/Blanc";
        case core::KartColor::DarkGray: return "Gris fonce";
        case core::KartColor::Purple:   return "Violet";
    }
    return "?";
}

static void options_menu_draw() {
    using namespace core;

    graphics_clear(Color::DarkBlue);
    graphics_draw_text_center(30, "OPTIONS", Color::Yellow);

    char buf[48];
    snprintf(buf, sizeof(buf), "Volume : %d %%", (master_volume * 100) / 255);
    graphics_draw_text_center(90, buf, Color::White);
    graphics_draw_text_center(106, "(Joystick Gauche/Droite)", Color::Gray);

    snprintf(buf, sizeof(buf), "Couleur kart : %s", kart_color_name(player_color));
    graphics_draw_text_center(140, buf, Color::White);
    graphics_draw_text_center(156, "(A pour changer)", Color::Gray);

    graphics_draw_text_center(210, "Menu : Retour", Color::Cyan);
}

// -----------------------------------------------------------------------------
// Overlay compte à rebours — feu tricolore (rien → rouge → orange → vert)
// -----------------------------------------------------------------------------
// Remplace l'ancien texte "3-2-1-GO" par un vrai feu de départ, plus lisible
// d'un coup d'œil et plus proche des jeux de course classiques : la case
// "3" (1ère seconde) reste éteinte (préparation), puis rouge, orange, vert.
// Une fois le vert allumé, le countdown_timer retombe à 0 et l'overlay
// disparaît (cf. garde en haut de fonction) — le vert marque bien le "GO".
static void draw_countdown_overlay() {
    using namespace core;
    if (countdown_timer <= 0.0f) return;

    int n = (int)std::ceil(countdown_timer);

    // Boîtier du feu, centré en haut de l'écran (zone libre entre le HUD
    // vitesse/tour à gauche et la minimap à droite, cf. captures d'écran).
    const int W = graphics_width();
    const int cx = W / 2;
    const int box_w = 36, box_h = 96;
    const int box_x = cx - box_w / 2;
    const int box_y = 20;
    const int r = 13;

    graphics_fill_rect(box_x, box_y, box_w, box_h, Color::Black);
    graphics_draw_rect(box_x, box_y, box_w, box_h, Color::White);

    Color off = Color::DarkGray;
    Color red    = (n == 3) ? Color::Red    : off;
    Color orange = (n == 2) ? Color::Orange : off;
    Color green  = (n == 1) ? Color::Green  : off;

    graphics_fill_circle(cx, box_y + box_h * 1 / 6, r, red);
    graphics_fill_circle(cx, box_y + box_h * 3 / 6, r, orange);
    graphics_fill_circle(cx, box_y + box_h * 5 / 6, r, green);
}

// -----------------------------------------------------------------------------
// Overlay flash rouge (choc subi par le joueur) — retour visuel clair en plus
// du ralentissement lui-même (sinon on ne comprend pas pourquoi on ralentit).
// -----------------------------------------------------------------------------
static void draw_shock_flash_overlay() {
    using namespace core;
    if (karts.empty() || karts[0].shock_slow_timer <= 0.0f) return;

    // Clignote (pas un cadre plein en continu, plus lisible) et s'estompe
    // vers la fin de l'effet.
    bool blink_on = ((int)(karts[0].shock_slow_timer * 6.0f)) % 2 == 0;
    if (!blink_on) return;

    const int W = graphics_width();
    const int H = graphics_height();
    const int thick = 6;
    graphics_fill_rect(0, 0, W, thick, Color::Red);
    graphics_fill_rect(0, H - thick, W, thick, Color::Red);
    graphics_fill_rect(0, 0, thick, H, Color::Red);
    graphics_fill_rect(W - thick, 0, thick, H, Color::Red);
}

void kart_game_draw() {

    // Titre
    if (mode == Mode::Title) {
        char label[48];
        snprintf(label, sizeof(label), "%s", kCups[selected_cup].name);
        title_screen_draw(title_timer, label);
        draw_screenshot_toast();
        return;
    }

    // Sprite Viewer
    if (mode == Mode::SpriteViewer) {
        sprite_viewer_draw();
        draw_screenshot_toast();
        return;
    }

    // Options
    if (mode == Mode::Options) {
        options_menu_draw();
        draw_screenshot_toast();
        return;
    }

    // Résultats
    if (mode == Mode::Results) {
        draw_results();
        draw_screenshot_toast();
        return;
    }

    // Course (toujours dessinée, même en pause)
    draw_race(track, karts, cam, item_box_cooldowns);
    draw_countdown_overlay();
    draw_shock_flash_overlay();

    // Pause (overlay)
    if (mode == Mode::Pause) {
        pause_menu_draw();     // ← nouveau
    }

    draw_screenshot_toast();
}

} // namespace kart
