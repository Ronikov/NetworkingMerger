cmake_minimum_required(VERSION 3.6.2)

############################
# Compiler options (for all targets)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(MSVC)
  add_compile_options(/W4 /WX)
elseif (GNU)
  add_compile_options(-Wall -Wextra -Werror)
endif()

############################
# Game
project(asteroids)

# Source files
set(SRC  
  # Game
  src/main.cpp
  # src/game/imgui.hpp
  src/game/game.hpp
  src/game/game.cpp
  src/game/state_ingame.cpp
  src/game/state_ingame.h

  src/game/network/system/networking.cpp
  src/game/network/system/networking.hpp
  src/game/network/system/utilsnetwork.cpp
  src/game/network/system/utilsnetwork.hpp
  src/game/network/client/client.cpp
  src/game/network/client/client.hpp
  src/game/network/server/server.cpp
  src/game/network/server/server.hpp

  src/game/TimeMgr/Time.cpp
  src/game/TimeMgr/Time.h
)

# Executable
add_executable(asteroids ${SRC})
target_include_directories(asteroids PRIVATE ./src)

############################
# Libs
find_package(lodepng CONFIG REQUIRED) # vcpkg install lodepng:x64-windows
find_package(glfw3 CONFIG REQUIRED) # vcpkg install glfw3:x64-windows
find_package(glm CONFIG REQUIRED) # vcpkg install glm:x64-windows
find_package(glad CONFIG REQUIRED) # vcpkg install glad:x64-windows
# find_package(imgui CONFIG REQUIRED) # vcpkg install imgui[glfw-binding,opengl3-glad-binding]:x64-windows
# vcpkg integrate install

############################
# Engine
add_subdirectory(src/engine)

target_link_libraries(asteroids PRIVATE 
  asteroids_engine
  # imgui::imgui
  glad::glad
  glfw
  lodepng
)

if(WIN32)
    target_link_libraries(${PROJECT_NAME} PRIVATE ws2_32.lib)
endif()

