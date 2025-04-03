#pragma once
#include <chrono>

//structure that controll the timers
class TimeSystem
{
public:
    //make the network a singleton
    static TimeSystem& Instance()
    {
        static TimeSystem time;
        return time;
    }

    void StartFrame();
    void EndFrame();
    float GetDt();
private:
    //variables that controll the time
    std::chrono::steady_clock::time_point start_time;
    std::chrono::nanoseconds  dt{ 0 };

    //constructor of the network manager
    TimeSystem() {}
};

#define TimeMgr (TimeSystem::Instance())