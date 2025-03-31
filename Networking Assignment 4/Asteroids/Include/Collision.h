/******************************************************************************/
/*!
\file		Collision.h
\author     goh.a@digipen.edu

\date   	March 27 2025
\brief		Collision function header

Copyright (C) 2023 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
 */
 /******************************************************************************/

#ifndef CSD1130_COLLISION_H_
#define CSD1130_COLLISION_H_

#include "AEEngine.h"

/**************************************************************************/
/*!
	Struct AABB which contain the minimum and maximum point of a rectangle
	*/
/**************************************************************************/
struct AABB
{
	
	AEVec2	min;
	AEVec2	max;
};

bool CollisionIntersection_RectRect(const AABB & aabb1, const AEVec2 & vel1, 
									const AABB & aabb2, const AEVec2 & vel2);


#endif // CSD1130_COLLISION_H_