#pragma once
#include <atomic>

extern std::atomic<bool> av_start;
extern std::atomic<bool> av_connected;

extern std::atomic<bool> av_game_over;

extern std::atomic<int> av_player_max;
extern std::atomic<int> av_player_num;