#include "Time.h"

/**
    * this function will start the local timer
    * @return  void
    */
void TimeSystem::StartFrame()
{
    start_time = std::chrono::steady_clock::now();
}

/**
* this function will update the dt with the local start and the current time
* @return  void
*/
void TimeSystem::EndFrame()
{
    auto end_time = std::chrono::steady_clock::now();
    dt = end_time - start_time;
}

/**
* this function will return the dt in float type
* @return  float
*/
float TimeSystem::GetDt()
{
    return static_cast<float>(dt.count());
}