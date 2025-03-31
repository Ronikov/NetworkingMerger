/******************************************************************************/
/*!
\file		GameStateManager.h
\author     goh.a@digipen.edu

\date   	March 27 2025
\brief		GameState Manager function

Copyright (C) 2023 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
 */
 /******************************************************************************/
//extern float	g_dt;

#include "Main.h"
#include "PathSmoother.h"
AEVec2 PathSmoother::Lerp(const AEVec2& start, const AEVec2& end, float t) 
{
    AEVec2 newPos;
    AEVec2Set(&newPos, (1.0f - t) * start.x + t * end.x, (1.0f - t) * start.y + t * end.y);
    return newPos;
}

// Update the entity's state with new position and velocity if threshold is met
void PathSmoother::directUpdatePath(const AEVec2& newData, AEVec2& objectData) 
{
    AEVec2Set(&objectData, newData.x, newData.y);
}
// Interpolate the entity's position and velocity based on calculateInterpolationFactor before threshold is met
void PathSmoother::interpolatePath(const AEVec2& startPosition, AEVec2& targetPosition, AEVec2& objectPos, float& interpolationFactor)
{
    AEVec2 newPos = Lerp(startPosition, targetPosition, interpolationFactor);
    AEVec2Set(&objectPos, newPos.x, newPos.y);
}



//float PathSmoother::calculateDynamicDuration(float distance)
//{
//    const float minDuration = 0.05f;
//    const float maxDuration = 0.2f;
//    const float maxDistance = 50.0f; // Adjusted for your distance threshold
//
//    float scaledDuration = (distance / maxDistance) * (maxDuration - minDuration) + minDuration;
//
//    // Clamp scaledDuration between minDuration and maxDuration
//    if (scaledDuration < minDuration)
//    {
//        scaledDuration = minDuration;
//    }
//    else if (scaledDuration > maxDuration)
//    {
//        scaledDuration = maxDuration;
//    }
//
//    return scaledDuration;
//}








