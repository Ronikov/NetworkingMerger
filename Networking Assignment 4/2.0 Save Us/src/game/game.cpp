#include "game.hpp"

#include <chrono>
#include <iostream>
#include "engine/opengl.hpp"
#include "engine/window.hpp"
#include "engine/shader.hpp"
#include "engine/font.hpp"
#include "game/network/system/networking.hpp"
#include "game/TimeMgr/Time.h"
#include "state_ingame.h"

namespace {

}

/**
 * @brief 
 * 
 */
void game::create(bool mbserver)
{
    // Window
    m_window = new engine::window();
    m_window->create(1270, 780, "asteroids");

    // Assets
    m_default_shader = engine::shader_default_create();

    m_default_font = new engine::font();
    m_default_font->create("resources/monospaced_24.fnt");

    // General
    m_start_time_point = clock::now();
    m_frame_time_point = m_start_time_point;
    m_game_time        = 0.0f;
    set_state_ingame(mbserver);
}

/**
 * @brief 
 * 
 */
bool game::update()
{
    TimeMgr.StartFrame();

    bool should_continue_net = NetMgr.Update();
    // dt
    auto now           = clock::now();
    auto diff          = now - m_frame_time_point;
    m_frame_time_point = now;
    m_dt               = std::chrono::duration_cast<std::chrono::milliseconds>(diff).count() * 0.001f;
    m_game_time        = m_game_time + m_dt;

    // Window update
    bool should_continue_window = m_window->update();

    m_key_states_prev = m_key_states;
    for (int i = 0; i < GLFW_KEY_LAST; ++i) {
        m_key_states[i] = glfwGetKey(m_window->handle(), i);
    }

    // States
    m_state_update();
    m_state_render();

    m_window->swap_buffers();

    TimeMgr.EndFrame();
    return should_continue_window && should_continue_net && !game_end;
}

/**
 * @brief 
 * 
 */
void game::set_state_ingame(bool mbserver)
{
    std::string serverIp, clientIP;

    configFile.open("config.config");

    if (configFile.is_open())
    {
        std::getline(configFile, serverIp);
        std::getline(configFile, clientIP);

        serverIp = serverIp.substr(11);
        clientIP = clientIP.substr(11);
    }
    else
        std::cout << "Error opening the config file" << std::endl;


    const char* ip = serverIp.c_str();
    if (!mbserver)
        ip = clientIP.c_str();

    if (m_state_free) m_state_free();
    if (m_state_unload) m_state_unload();

    m_state_load = &GameStatePlayLoad;
    m_state_init = &GameStatePlayInit;
    m_state_update = &GameStatePlayUpdate;
    m_state_render = &GameStatePlayDraw;
    m_state_free = &GameStatePlayFree;
    m_state_unload = &GameStatePlayUnload;

    m_state_load();
    NetMgr.Start(ip, 8001, mbserver, false);
    m_state_init();
}

void game::destroy()
{
    NetMgr.ShutDown();

    delete m_default_shader;
    m_default_shader = nullptr;
    delete m_default_font;
    m_default_font = nullptr;
    delete m_window;
    m_window = nullptr;
}