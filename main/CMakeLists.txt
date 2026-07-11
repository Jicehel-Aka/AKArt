idf_component_register(
    SRCS
        app_main.cpp

        core/graphics.cpp
        core/input.cpp
        core/audio.cpp
        core/sprites.cpp
        core/kart_loader.cpp

        game/kart_engine.cpp
        game/kart_render.cpp
        game/kart_game.cpp
        game/title_screen.cpp
        game/pause_menu.cpp
        game/sprite_viewer.cpp

        tasks/task_kart.cpp

        assets/gfx/kart_rouge_data.cpp
        assets/gfx/kart_bleu_data.cpp
        assets/gfx/kart_jaune_data.cpp
        assets/gfx/kart_vert_data.cpp
        assets/gfx/kart_rose_data.cpp
        assets/gfx/kart_gris_data.cpp
        assets/gfx/kart_gris_fonce_data.cpp
        assets/gfx/kart_violet_data.cpp
        assets/music/music_title.cpp
        assets/music/music_hills.cpp
        assets/music/music_city.cpp
        assets/music/music_desert.cpp
        assets/music/music_results.cpp
        assets/music/jingle_lastlap.cpp
        assets/music/jingle_finish.cpp
        assets/gfx/title_screen_data.cpp

        assets/gfx/decor_palm.cpp
        assets/gfx/decor_tree1.cpp
        assets/gfx/decor_tree2.cpp
        assets/gfx/decor_tree3.cpp
        assets/gfx/decor_tree4.cpp
        assets/gfx/decor_cactus1.cpp
        assets/gfx/decor_cactus2.cpp
        assets/gfx/decor_cactus3.cpp
        assets/gfx/decor_tire.cpp
        assets/gfx/decor_flag.cpp
        assets/gfx/decor_fire.cpp
        assets/gfx/decor_barrier.cpp
        assets/gfx/decor_rock.cpp
        assets/gfx/decor_start_gate.cpp
        assets/gfx/decor_finish_gate.cpp
        assets/gfx/decor_house.cpp
        assets/gfx/decor_sky_bg.cpp
        assets/gfx/decor_sky_bg_city.cpp
        assets/gfx/decor_sky_bg_desert.cpp
        assets/gfx/decor_sign_left.cpp
        assets/gfx/decor_sign_right.cpp
        assets/gfx/decor_oil_slick.cpp
        assets/gfx/decor_bonus_boost.cpp
        assets/gfx/decor_bonus_shield.cpp
        assets/gfx/decor_bonus_shock.cpp

    INCLUDE_DIRS
        .
        core
        game
        tasks
        assets/gfx
        assets/music

    REQUIRES
        gamebuino
        nvs_flash
        app_update
        esp_partition
        esp_system
)
