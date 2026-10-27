cmake_minimum_required(VERSION 3.20)
project(SpaceCadetPort CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(SDL2 REQUIRED CONFIG)

add_executable(pinball
    src/main.cpp
    src/platform/audio_sdl.cpp
    src/platform/midi_mac.cpp
    src/dat_file.cpp
    src/exe_resources.cpp
    src/engine/maths.cpp
    src/engine/loader.cpp
    src/engine/render.cpp
    src/engine/timer.cpp
    src/engine/collision.cpp
    src/engine/component.cpp
    src/engine/ball.cpp
    src/engine/table.cpp
    src/engine/objects.cpp
    src/engine/objects_a.cpp
    src/engine/objects_b.cpp
    src/engine/lights.cpp
    src/engine/misc_objects.cpp
    src/engine/score.cpp
    src/engine/control/control.cpp
    src/engine/control/tags.cpp
    src/engine/control/control_c1.cpp
    src/engine/control/control_c2.cpp
    src/engine/control/control_c3.cpp
)
target_compile_options(pinball PRIVATE -Wall -Wextra)
target_link_libraries(pinball PRIVATE SDL2::SDL2 "-framework AudioToolbox" "-framework CoreFoundation")
