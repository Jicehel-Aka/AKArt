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

        tasks/task_kart.cpp

        assets_data/kart_rouge_data.cpp
        assets_data/kart_bleu_data.cpp
        assets_data/kart_jaune_data.cpp
        assets_data/kart_vert_data.cpp
        assets_data/title_screen_data.cpp

    INCLUDE_DIRS
        .
        core
        game
        tasks
        assets_data

    REQUIRES 
        gamebuino 
        nvs_flash
        app_update
        esp_partition
)

