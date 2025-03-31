/******************************************************************************/
/*!
\file		Collision.cpp
\author     goh.a@digipen.edu

\date   	March 27 2025
\brief		Collision function

Copyright (C) 2023 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
 */
 /******************************************************************************/

#include "main.h"

/**************************************************************************/
/*!
	Check for collision between two rectangles using AABB 
	*/
/**************************************************************************/
bool CollisionIntersection_RectRect(const AABB & aabb1, const AEVec2 & vel1, 
									const AABB & aabb2, const AEVec2 & vel2)
{
	UNREFERENCED_PARAMETER(aabb1);
	UNREFERENCED_PARAMETER(vel1);
	UNREFERENCED_PARAMETER(aabb2);
	UNREFERENCED_PARAMETER(vel2);


	// Implement the collision intersection over here. The steps are: 

	// Step 1: Check for static collision detection between rectangles (before moving). 
			//If the check returns no overlap you continue with the following next steps (dynamics).
			//Otherwise you return collision true
	if (!((aabb1.max.x > aabb2.min.x) || (aabb2.max.x > aabb1.min.x)
		|| (aabb1.max.y > aabb2.min.y) || (aabb2.max.y > aabb1.min.y)))
		return 0;

	// Step 2: Initialize and calculate the new velocity of Vb
	AEVec2 vel;
	AEVec2Set(&vel, (vel2.x - vel1.x), (vel2.y - vel1.y));

	float tFirst = 0;
	float tLast = g_dt;

	// Step 3: Working with one dimension (x-axis).
	if (vel.x <= 0)
	{
		if (aabb1.min.x > aabb2.max.x)									// case 1
			return 0;													// no intersection and B is moving away

		if (aabb1.max.x < aabb2.min.x)									//case 4 p1
			tFirst = AEMax((aabb1.max.x - aabb2.min.x) / vel.x, tFirst);

		if (aabb1.min.x < aabb2.max.x)									//case 4 p2
			tLast = AEMin((aabb1.min.x - aabb2.max.x) / vel.x, tLast);

	}
	else if (vel.x > 0)
	{
		if (aabb1.max.x < aabb2.min.x)									// case 3
			return 0;													//no intersection and B is moving away

		if (aabb1.min.x > aabb2.max.x)									// case 2 p1
			tFirst = AEMax((aabb1.min.x - aabb2.max.x) / vel.x, tFirst);

		if (aabb1.max.x > aabb2.min.x)									// case 2 p2
			tLast = AEMin((aabb1.max.x - aabb2.min.x) / vel.x, tLast);
	}

	// Step 5: Otherwise the rectangles intersect
	if (tFirst > tLast)
		return 0; // no intersection

	// Step 4: Repeat step 3 on the y-axis
	if (vel.y <= 0)
	{
		if (aabb1.min.y > aabb2.max.y)									// case 1
			return 0;													// no intersection and B is moving away

		if (aabb1.max.y < aabb2.min.y)									//case 4 p1
			tFirst = AEMax((aabb1.max.y - aabb2.min.y) / vel.y, tFirst);

		if (aabb1.min.y < aabb2.max.y)									//case 4 p2
			tLast = AEMin((aabb1.min.y - aabb2.max.y) / vel.y, tLast);
	}
	else if (vel.y > 0)
	{
		if (aabb1.max.y < aabb2.min.y)									// case 3
			return 0;													//no intersection and B is moving away

		if (aabb1.min.y > aabb2.max.y)									// case 2 p1
			tFirst = AEMax((aabb1.min.y - aabb2.max.y) / vel.y, tFirst);

		if (aabb1.max.y > aabb2.min.y)									// case 2 p2
			tLast = AEMin((aabb1.max.y - aabb2.min.y) / vel.y, tLast);
	}

	// Step 5: Otherwise the rectangles intersect
	if (tFirst > tLast)
		return 0;														// no intersection

	else return 1;
}
