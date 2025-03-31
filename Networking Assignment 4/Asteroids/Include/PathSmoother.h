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

#ifndef PATH_SMOOTHER_H
#define PATH_SMOOTHER_H
#include "AEEngine.h"
#include <algorithm>
// ---------------------------------------------------------------------------
// includes

struct currPathData
{
    AEVec2 currentPosition;
    AEVec2 currentVelocity;
    float currentDir;
};
struct newPathData
{
	AEVec2 newPosition;
	AEVec2 newVelocity;
    float newDir;
};
struct currLerpPathData
{
	AEVec2 currLerpPosition;
	AEVec2 currLerpVelocity;
    float currLerpDir;
    bool isLerping;
    float lerpDuration;
    float totalLerpDuration;
};
class PathSmoother
{
public:

    static AEVec2 Lerp(const AEVec2& start, const AEVec2& end, float t);

    // Update the entity's state with new position and velocity if threshold is met
    void directUpdatePath(const AEVec2& newData, AEVec2& objectData);

    // Interpolate the entity's position and velocity based on alpha before threshold is met
    void interpolatePath(const AEVec2& startPosition, AEVec2& targetPosition, AEVec2& objectPos, float& interpolationFactor);
};


#endif // PATH_SMOOTHER_H










