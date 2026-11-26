cmake_minimum_required(VERSION 3.21)
project(DuskPatrolQt LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Quick OpenGL Concurrent)

qt_add_executable(dusk-patrol-qt
    src/main.cpp
    src/GameWindow.h
    src/GameWindow.cpp
    src/GameItem.h
    src/GameItem.cpp
    src/GameState.h
    src/FlightModel.h
    src/FlightModel.cpp
    src/Mesh.h
    src/Mesh.cpp
    src/Trail.h
    src/Trail.cpp
    src/Particles.h
    src/Particles.cpp
    src/AudioEngine.h
    src/AudioEngine.cpp
    src/TrailShader.h
    src/Renderer.h
    src/Renderer.cpp
    src/Shaders.h
    src/SceneShader.h
    src/MeshShader.h
    src/World.h
    src/World.cpp
)

target_include_directories(dusk-patrol-qt PRIVATE src)
target_compile_definitions(dusk-patrol-qt PRIVATE
    QML_DIR="${CMAKE_CURRENT_SOURCE_DIR}/qml"
)
target_link_libraries(dusk-patrol-qt PRIVATE
    Qt6::Core Qt6::Gui Qt6::Quick Qt6::OpenGL Qt6::Concurrent
    winmm
)
