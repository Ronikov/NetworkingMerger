#include "AtomicVariables.h"

std::atomic<bool> av_start{ false };
std::atomic<bool> av_connected{ false };

std::atomic<bool> av_game_over{ false };

std::atomic<int> av_player_max{-1};
std::atomic<int> av_player_num{-1};

std::atomic<int> av_asteroid_max{ -1 };
std::atomic<int> av_asteroid_num{ -1 };

std::atomic<int> av_bullet_max{ -1 };
std::atomic<int> av_bullet_num{ -1 };
