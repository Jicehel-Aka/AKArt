idf_component_register(
    SRCS
        app_main.cpp

        core/graphics.cpp
        core/input.cpp
        core/audio.cpp
        core/sprites.cpp

        game/kart_engine.cpp
        game/kart_render.cpp
        game/kart_game.cpp

        tasks/task_kart.cpp

    INCLUDE_DIRS
        .
        core
        game
        tasks

    REQUIRES
        gamebuino
)
