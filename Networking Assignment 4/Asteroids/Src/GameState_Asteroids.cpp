/******************************************************************************/
/*!
\file		GameState_Asteroids.cpp
\author     goh.a@digipen.edu

\date   	March 27 2025
\brief		GameState Manager function

Copyright (C) 2023 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
 */
 /******************************************************************************/

#include <iostream>
#include <vector>

#include "AtomicVariables.h"
#include "PathSmoother.h"
#include "main.h"

/******************************************************************************/
/*!
	Defines
*/
/******************************************************************************/
const unsigned int	GAME_OBJ_NUM_MAX		= 32;			// The total number of different objects (Shapes)
const unsigned int	GAME_OBJ_INST_NUM_MAX	= 2048;			// The total number of different game object instances

const int			SCORE_WIN				= 8000;			// win condition


//FOR FONT
const int			FONT_SIZE				= 40;			// font size


//FOR SHIP
const unsigned int	SHIP_INITIAL_NUM		= 3;			// initial number of ship lives
const float			SHIP_SIZE				= 16.0f;		// ship size
const float			SHIP_ACCEL_FORWARD		= 100.0f;		// ship forward acceleration (in m/s^2)
const float			SHIP_ACCEL_BACKWARD		= -60.0f;		// ship backward acceleration (in m/s^2)
const float			SHIP_ROT_SPEED			= (2.0f * PI);	// ship rotation speed (degree/second)


//FOR BULLET
const float			BULLET_SPEED			= 200.0f;		// bullet speed (m/s)
const float			BULLET_SIZE				= 20.0f;		// bullet size


//FOR AESTEROIDS
const int			ASTEROID_SCORE			= 300;			// score per asteroid destroyed
const float			ASTEROID_SIZE			= 70.0f;		// asteroid size
const float			ASTEROID_SPEED			= 100.0f;		// maximum asteroid speed
const float			ASTEROID_TIME			= 0.0f;			// 2 second spawn time for asteroids
const int			ASTEROID_INIT_CNT		= 0;			// number of asteroid to init with


//FOR LIVES PICKUP
const float			LIVES_SIZE				= 30.0f;		// lives pickup size

// -----------------------------------------------------------------------------
enum TYPE
{
	// list of game object types
	TYPE_SHIP = 0,
	TYPE_BULLET,
	TYPE_ASTEROID,
	TYPE_LIVES,				//lives pickup

	TYPE_NUM
};

// -----------------------------------------------------------------------------
// object flag definition

const unsigned long FLAG_ACTIVE = 0x00000001;
const unsigned long FLAG_LAGGING = 0x00000010;

/******************************************************************************/
/*!
	Struct/Class Definitions
*/
/******************************************************************************/

//Game object structure
struct GameObj
{
	unsigned long	 type;		// object type
	AEGfxVertexList* pMesh;		// This will hold the triangles which will form the shape of the object
};

// ---------------------------------------------------------------------------

//Game object instance structure
struct GameObjInst
{
	GameObj*			pObject;	// pointer to the 'original' shape
	unsigned long		flag;		// bit flag or-ed together
	float				scale;		// scaling value of the object instance
	AEVec2				posCurr;	// object current position
	AEVec2				velCurr;	// object current velocity
	float				dirCurr;	// object current direction
	AABB				boundingBox;// object bouding box that encapsulates the object
	AEMtx33				transform;	// object transformation matrix: Each frame, 
									// calculate the object instance's transformation matrix and save it here
	int					id;
};

/******************************************************************************/
/*!
	Static Variables
*/
/******************************************************************************/

// list of original object
static GameObj				sGameObjList[GAME_OBJ_NUM_MAX];				// Each element in this array represents a unique game object (shape)
static unsigned long		sGameObjNum;								// The number of defined game objects

// list of object instances
static GameObjInst			sGameObjInstList[GAME_OBJ_INST_NUM_MAX];	// Each element in this array represents a unique game object instance (sprite)
static unsigned long		sGameObjInstNum;							// The number of used game object instances

// pointer to the ship object
static GameObjInst* spShip;												// Pointer to the "Ship" game object instance

// number of ship available (lives 0 = game over)
static long					sShipLives;									// The number of lives left

// the score = number of asteroid destroyed
static unsigned long		sScore;										// Number of asteroid destroyed
static bool					onValueChange = true;						// for checking game over or win
static float				asteroid_timer = 0;							// for spawning asteroids
s8							font;										// roboto mono font ID



//ADDING TEXTURES//
static AEGfxTexture* asteroidTex;										// asteroid texture
static AEGfxTexture* bulletTex;											// bullet texture
static AEGfxTexture* livesTex;											// lives pickup texture
//static AEGfxTexture* shipTex;

// ---------------------------------------------------------------------------

// functions to create/destroy a game object instance
GameObjInst *		gameObjInstCreate(unsigned long type, float scale,
AEVec2*				pPos, AEVec2* pVel, float dir);
void				gameObjInstDestroy(GameObjInst* pInst);

void				spawnAsteroid(unsigned int count);		// function to spawn asteroids
void				spawnLives(unsigned int count);			// function to spawn lives pickups

void				initMultiPlayer(int num_player);

std::vector<GameObjInst*> player_list;
bool newDataReceived = false;
PathSmoother pathSmootherInstance;
std::vector<std::pair<int ,newPathData>> pathData;		  // newly received path data for the object
std::vector<std::pair<int, currPathData>> currData;		  // current path data for the object
std::vector<std::pair<int, currLerpPathData>> lerpData;   // current lerp path data for the object
std::vector<GameObjInst*> asteroids_list;
std::vector<GameObjInst*> bullet_list;

/******************************************************************************/
/*!
	"Load" function of this state
*/
/******************************************************************************/
void GameStateAsteroidsLoad(void)
{
	//LOAD FONT
	font = AEGfxCreateFont("../Resources/Fonts/Arial Italic.ttf", FONT_SIZE);

	//LOAD TEX
	asteroidTex = AEGfxTextureLoad("../Resources/Textures/AsteroidTexture.png");
	if (asteroidTex == NULL) { printf("fail to load texture!!"); }

	bulletTex = AEGfxTextureLoad("../Resources/Textures/BulletTexture.png");
	if (bulletTex == NULL) { printf("fail to load texture!!"); }

	livesTex = AEGfxTextureLoad("../Resources/Textures/LivesTexture.png");
	if (livesTex == NULL) { printf("fail to load texture!!"); }

	// zero the game object array
	memset(sGameObjList, 0, sizeof(GameObj) * GAME_OBJ_NUM_MAX);
	// No game objects (shapes) at this point
	sGameObjNum = 0;

	// zero the game object instance array
	memset(sGameObjInstList, 0, sizeof(GameObjInst) * GAME_OBJ_INST_NUM_MAX);
	// No game object instances (sprites) at this point
	sGameObjInstNum = 0;

	// The ship object instance hasn't been created yet, so this "spShip" pointer is initialized to 0
	spShip = nullptr;

	// load/create the mesh data (game objects / Shapes)
	GameObj* pObj;

	// =====================
	// create the ship shape
	// =====================

	pObj = sGameObjList + sGameObjNum++;
	pObj->type = TYPE_SHIP;

	AEGfxMeshStart();
	AEGfxTriAdd(
		-0.5f, 0.5f, 0xFF746AB0, 0.0f, 0.0f,
		-0.5f, -0.5f, 0xFF746AB0, 0.0f, 0.0f,
		0.5f, 0.0f, 0xFFFFFFFF, 0.0f, 0.0f);

	pObj->pMesh = AEGfxMeshEnd();
	AE_ASSERT_MESG(pObj->pMesh, "fail to create object!!");


	// =======================
	// create the bullet shape
	// =======================

	pObj = sGameObjList + sGameObjNum++;
	pObj->type = TYPE_BULLET;

	AEGfxMeshStart();
	AEGfxTriAdd(
		-0.5f, -0.5f, 0xFF00FFFF, 0.0f, 1.0f,
		0.5f, -0.5f, 0xFF00FFFF, 1.0f, 1.0f,
		-0.5f, 0.5f, 0xFF00FFFF, 0.0f, 0.0f);

	AEGfxTriAdd(
		0.5f, -0.5f, 0xFF00FFFF, 1.0f, 1.0f,
		0.5f, 0.5f, 0xFF00FFFF, 1.0f, 0.0f,
		-0.5f, 0.5f, 0xFF00FFFF, 0.0f, 0.0f);

	pObj->pMesh = AEGfxMeshEnd();
	AE_ASSERT_MESG(pObj->pMesh, "fail to create object!!");


	// =========================
	// create the asteroid shape
	// =========================

	pObj = sGameObjList + sGameObjNum++;
	pObj->type = TYPE_ASTEROID;

	AEGfxMeshStart();
	AEGfxTriAdd(
		-0.5f, -0.5f, 0xFFFF00FF, 0.0f, 1.0f,
		0.5f, -0.5f, 0xFFFF00FF, 1.0f, 1.0f,
		-0.5f, 0.5f, 0xFFFF00FF, 0.0f, 0.0f);

	AEGfxTriAdd(
		0.5f, -0.5f, 0xFFFF00FF, 1.0f, 1.0f,
		0.5f, 0.5f, 0xFFFF00FF, 1.0f, 0.0f,
		-0.5f, 0.5f, 0xFFFF00FF, 0.0f, 0.0f);

	pObj->pMesh = AEGfxMeshEnd();
	AE_ASSERT_MESG(pObj->pMesh, "fail to create object!!");

	// ===============================
	// create the lives shape
	// ===============================

	pObj = sGameObjList + sGameObjNum++;
	pObj->type = TYPE_LIVES;

	AEGfxMeshStart();
	AEGfxTriAdd(
		-0.5f, -0.5f, 0xFFFF00FF, 0.0f, 1.0f,
		0.5f, -0.5f, 0xFFFF00FF, 1.0f, 1.0f,
		-0.5f, 0.5f, 0xFFFF00FF, 0.0f, 0.0f);

	AEGfxTriAdd(
		0.5f, -0.5f, 0xFFFF00FF, 1.0f, 1.0f,
		0.5f, 0.5f, 0xFFFF00FF, 1.0f, 0.0f,
		-0.5f, 0.5f, 0xFFFF00FF, 0.0f, 0.0f);

	pObj->pMesh = AEGfxMeshEnd();
	AE_ASSERT_MESG(pObj->pMesh, "fail to create object!!");
}

/******************************************************************************/
/*!
	"Initialize" function of this state
*/
/******************************************************************************/
void GameStateAsteroidsInit(void)
{
	// create the main ship
	spShip = gameObjInstCreate(TYPE_SHIP, SHIP_SIZE, nullptr, nullptr, 0.0f);
	AE_ASSERT(spShip);

	// CREATE THE INITIAL ASTEROIDS INSTANCES USING THE "gameObjInstCreate" FUNCTION
	spawnAsteroid(ASTEROID_INIT_CNT);

	// reset the score and the number of ships
	sScore		= 0;
	sShipLives	= SHIP_INITIAL_NUM;
}

void GameStateAsteroidsInitMultiPlayer(void)
{
	initMultiPlayer(av_player_max);
}

/******************************************************************************/
/*!
	"Update" function of this state
*/
/******************************************************************************/
void GameStateAsteroidsUpdate(void)
{
	// check for game over 
	if ((sShipLives <= 0) || sScore >= SCORE_WIN) return;

	//spawn asteroids
	asteroid_timer += g_dt;
	if (asteroid_timer > ASTEROID_TIME && ASTEROID_TIME != 0)
	{
		asteroid_timer -= ASTEROID_TIME;

		spawnAsteroid(2);

		spawnLives(1);
	}

	// =========================
	// update according to input
	// =========================

	// This input handling moves the ship without any velocity nor acceleration
	// It should be changed when implementing the Asteroids project
	//
	// Updating the velocity and position according to acceleration is 
	// done by using the following:
	// Pos1 = 1/2 * a*t*t + v0*t + Pos0
	//
	// In our case we need to divide the previous equation into two parts in order 
	// to have control over the velocity and that is done by:
	//
	// v1 = a*t + v0		//This is done when the UP or DOWN key is pressed 
	// Pos1 = v1*t + Pos0

	if (AEInputCheckCurr(AEVK_UP))
	{
		AEVec2 added;
		AEVec2Set(&added, cosf(spShip->dirCurr), sinf(spShip->dirCurr));
		AEVec2Normalize(&added, &added);

		// Find the velocity according to the acceleration
		AEVec2Scale(&added, &added, SHIP_ACCEL_FORWARD * g_dt);

		// Edit current velocity for position update later
		AEVec2Add(&spShip->velCurr, &spShip->velCurr, &added);

		// Limit ship speed
		AEVec2Scale(&spShip->velCurr, &spShip->velCurr, 0.99f);
	}

	if (AEInputCheckCurr(AEVK_DOWN))
	{
		AEVec2 added;
		AEVec2Set(&added, cosf(spShip->dirCurr), sinf(spShip->dirCurr));
		AEVec2Normalize(&added, &added);

		// Find the velocity according to the decceleration
		AEVec2Scale(&added, &added, SHIP_ACCEL_BACKWARD * g_dt);

		// Edit current velocity for position update later
		AEVec2Add(&spShip->velCurr, &spShip->velCurr, &added);

		// Limit ship speed
		AEVec2Scale(&spShip->velCurr, &spShip->velCurr, 0.99f);
	}

	if (AEInputCheckCurr(AEVK_LEFT))
	{
		// Rotate left
		spShip->dirCurr += SHIP_ROT_SPEED * g_dt;
		spShip->dirCurr = AEWrap(spShip->dirCurr, -PI, PI);
	}

	if (AEInputCheckCurr(AEVK_RIGHT))
	{
		// Rotate right
		spShip->dirCurr -= SHIP_ROT_SPEED * g_dt; //(float)(AEFrameRateControllerGetFrameTime ());
		spShip->dirCurr = AEWrap(spShip->dirCurr, -PI, PI);
	}

	// Shoot a bullet if space is triggered (Create a new object instance)
	if (AEInputCheckTriggered(AEVK_SPACE))
	{
		// Get the bullet's direction according to the ship's direction
		// Set the velocity
		// Create an instance


	
		// Temp storage for new bullet, spawned  ship pos
		AEVec2 added;

		// Get the bullet's direction according to the ship's direction
		AEVec2Set(&added, cosf(spShip->dirCurr), sinf(spShip->dirCurr));
		AEVec2Normalize(&added, &added);

		// Set the velocity
		AEVec2Scale(&added, &added, BULLET_SPEED);

		// Create an instance
		//gameObjInstCreate(TYPE_BULLET, BULLET_SIZE, &spShip->posCurr, &added, spShip->dirCurr);
		bullet_list.emplace_back(gameObjInstCreate(TYPE_BULLET, BULLET_SIZE, &spShip->posCurr, &added, spShip->dirCurr));
	}
	if (av_connected)
	{
		//assign game object position to lerp data
		for (unsigned long i = 0; i < GAME_OBJ_INST_NUM_MAX; ++i)
		{
			GameObjInst* pInst = sGameObjInstList + i;
			if (lerpData.empty()) continue;
			auto lerpDataEntry = std::find_if(lerpData.begin(), lerpData.end(), [i](const std::pair<int, currLerpPathData>& entry) {return entry.first == i; });
			lerpDataEntry->second.currLerpPosition = pInst->posCurr;
			lerpDataEntry->second.currLerpVelocity = pInst->velCurr;
			// ===================================
			// update active game data
			// ===================================
			if (newDataReceived)
			{
				// Loop through all the new path data received
				for (const auto& path : pathData)
				{
					// Find and update the corresponding current data and lerp data
					for (auto& curr : currData)
					{
						if (curr.first == path.first) // If the object ID matches
						{
							// Update the current data with new path data
							curr.second.currentPosition = path.second.newPosition;
							curr.second.currentVelocity = path.second.newVelocity;
							curr.second.currentDir = path.second.newDir;
						}
					}
				}
				auto newDataEntry = std::find_if(pathData.begin(), pathData.end(), [i](const std::pair<int, newPathData>& entry) {return entry.first == i; });
				auto lerpDataEntry = std::find_if(lerpData.begin(), lerpData.end(), [i](const std::pair<int, currLerpPathData>& entry) {return entry.first == i; });

				//if new data is greater than a certain threshold set lagging flag to true
				float thresholdToTriggerLaggingCompensation = 50.f;
				if (AEVec2Distance(&lerpDataEntry->second.currLerpPosition, &newDataEntry->second.newPosition) > thresholdToTriggerLaggingCompensation)
				{
					pInst->flag = FLAG_LAGGING;
				}
				//else flag is active
				else
				{
					pInst->flag = FLAG_ACTIVE;
				}
				newDataReceived = false; // Reset the new data flag
			}
		}

	}
	// ======================================================
	// update physics of all active game object instances
	//  -- Get the AABB bounding rectangle of every active instance:
	//		boundingRect_min = -(BOUNDING_RECT_SIZE/2.0f) * instance->scale + instance->pos
	//		boundingRect_max = +(BOUNDING_RECT_SIZE/2.0f) * instance->scale + instance->pos
	//
	//	-- Positions of the instances are updated here with the already computed velocity (above)
	// ======================================================
	for (unsigned long i = 0; i < GAME_OBJ_INST_NUM_MAX; ++i)
	{
		GameObjInst* pInst = sGameObjInstList + i;

		// skip non-active object
		if ((pInst->flag & FLAG_ACTIVE) == 0)
			continue;

		if ((pInst->flag & FLAG_LAGGING) != 0)
		{
			//calculate movement based on lerp data
			auto currDataEntry = std::find_if(currData.begin(), currData.end(), [i](const std::pair<int, currPathData>& entry) {return entry.first == i;});

			// Find the corresponding lerp data entry for this game object
			auto lerpDataEntry = std::find_if(lerpData.begin(), lerpData.end(), [i](const std::pair<int, currLerpPathData>& entry) {return entry.first == i;});

			lerpDataEntry->second.lerpDuration += g_dt;

			//lerp the pInst velocity
			//lerp the pInst position
			float thresholdToTriggerLerp = 50.0f;
			//lerp the position if within threshold
			if (AEVec2Distance(&lerpDataEntry->second.currLerpPosition, &currDataEntry->second.currentPosition) < thresholdToTriggerLerp)
			{
				//pathSmootherInstance.interpolatePath(lerpDataEntry->second.currLerpVelocity, currDataEntry->second.currentVelocity, pInst->velCurr, interpolateFact);
				pathSmootherInstance.interpolatePath(lerpDataEntry->second.currLerpPosition, currDataEntry->second.currentPosition, pInst->posCurr, lerpDataEntry->second.lerpDuration);
			}
			//directly set the position 
			else
			{
				//pathSmootherInstance.directUpdatePath(currDataEntry->second.currentVelocity, pInst->velCurr);
				pathSmootherInstance.directUpdatePath(currDataEntry->second.currentPosition, pInst->posCurr);
			}
			AEVec2 moveLerp;
			// Calculate based on previously derived velocity
			AEVec2Scale(&moveLerp, &pInst->velCurr, g_dt);

			// Update current position
			AEVec2Add(&pInst->posCurr, &moveLerp, &pInst->posCurr);

			// Bounding box
			AEVec2 boundSizeMin, boundSizeMax;

			// Normalise bounding box
			AEVec2Set(&boundSizeMax, 0.5f, 0.5f);

			// Scale box, if any scale applied
			AEVec2Scale(&boundSizeMax, &boundSizeMax, pInst->scale);
				
			// Flip for min size
			AEVec2Neg(&boundSizeMin, &boundSizeMax);

			// Update bounding box, min and max
			AEVec2Add(&pInst->boundingBox.min, &boundSizeMin, &pInst->posCurr);
			AEVec2Add(&pInst->boundingBox.max, &boundSizeMax, &pInst->posCurr);
			continue;
		}
		// Movement
		AEVec2 move;

		// Calculate based on previously derived velocity
		AEVec2Scale(&move, &pInst->velCurr, g_dt);

		// Update current position
		AEVec2Add(&pInst->posCurr, &move, &pInst->posCurr);

		// Bounding box
		AEVec2 boundSizeMin, boundSizeMax;

		// Normalise bounding box
		AEVec2Set(&boundSizeMax, 0.5f, 0.5f);

		// Scale box, if any scale applied
		AEVec2Scale(&boundSizeMax, &boundSizeMax, pInst->scale);

		// Flip for min size
		AEVec2Neg(&boundSizeMin, &boundSizeMax);

		// Update bounding box, min and max
		AEVec2Add(&pInst->boundingBox.min, &boundSizeMin, &pInst->posCurr);
		AEVec2Add(&pInst->boundingBox.max, &boundSizeMax, &pInst->posCurr);
	}


	// ====================
	// check for collision
	// ====================

	/*
	for each object instance: oi1
		if oi1 is not active
			skip

		if oi1 is an asteroid
			for each object instance oi2
				if(oi2 is not active or oi2 is an asteroid)
					skip

				if(oi2 is the ship)
					Check for collision between ship and asteroids (Rectangle - Rectangle)
					Update game behavior accordingly
					Update "Object instances array"
				else
				if(oi2 is a bullet)
					Check for collision between bullet and asteroids (Rectangle - Rectangle)
					Update game behavior accordingly
					Update "Object instances array"
	*/

	for (unsigned long i = 0; i < GAME_OBJ_INST_NUM_MAX; ++i)
	{
		GameObjInst* pInst = sGameObjInstList + i;

		// skip non-active object
		if ((pInst->flag & FLAG_ACTIVE) == 0)
			continue;

	

		if (pInst->pObject->type == TYPE_ASTEROID)
		{
			for (unsigned int j = 0; j < GAME_OBJ_INST_NUM_MAX; ++j)
			{
				GameObjInst* pOther = sGameObjInstList + j;

				// skip non-active object
				if ((pOther->flag & FLAG_ACTIVE) == 0)
					continue;

				// if pInst is the ship
				if (pOther->pObject->type == TYPE_SHIP)
				{
					if (CollisionIntersection_RectRect(pInst->boundingBox, pInst->velCurr,
						pOther->boundingBox, pOther->velCurr))
					{
						onValueChange = true;

						// Reduce ship lives
						--sShipLives;

						// Reset ship position
						AEVec2Zero(&spShip->posCurr);
						AEVec2Zero(&spShip->velCurr);

						// Disable asteroid
						gameObjInstDestroy(pInst);
					}
				}
				// if pInst is a bullet
				if (pOther->pObject->type == TYPE_BULLET)
				{
					if (CollisionIntersection_RectRect(pInst->boundingBox, pInst->velCurr,
						pOther->boundingBox, pOther->velCurr))
					{
						onValueChange = true;

						bullet_list.erase(std::remove_if(bullet_list.begin(), bullet_list.end(),
							[pInst](GameObjInst* bullet) {
								return bullet == pInst;
							}), bullet_list.end());

						// Disable bullet and asteroid
						gameObjInstDestroy(pOther);
						gameObjInstDestroy(pInst);

						// Add to the overall score
						sScore += ASTEROID_SCORE;
					}
				}
			}
		}
		if (pInst->pObject->type == TYPE_LIVES)
		{
			for (unsigned int j = 0; j < GAME_OBJ_INST_NUM_MAX; ++j)
			{
				GameObjInst* pOther = sGameObjInstList + j;

				// skip non-active object
				if ((pOther->flag & FLAG_ACTIVE) == 0)
					continue;

				// if pInst is the ship
				if (pOther->pObject->type == TYPE_SHIP)
				{
					if (CollisionIntersection_RectRect(pInst->boundingBox, pInst->velCurr,
						pOther->boundingBox, pOther->velCurr))
					{
						printf_s("1 UP ");
						// Increase ship lives
						++sShipLives;
						// Disable lives pickup
						gameObjInstDestroy(pInst);
					}
				}
			}
		}
	}



	// ===================================
	// update active game object instances
	// Example:
	//		-- Wrap specific object instances around the world (Needed for the assignment)
	//		-- Removing the bullets as they go out of bounds (Needed for the assignment)
	//		-- If you have a homing missile for example, compute its new orientation 
	//			(Homing missiles are not required for the Asteroids project)
	//		-- Update a particle effect (Not required for the Asteroids project)
	// ===================================
	for (unsigned long i = 0; i < GAME_OBJ_INST_NUM_MAX; ++i)
	{
		GameObjInst* pInst = sGameObjInstList + i;

		// skip non-active object
		if ((pInst->flag & FLAG_ACTIVE) == 0)
			continue;

		// check if the object is a ship
		if (pInst->pObject->type == TYPE_SHIP)
		{
			// warp the ship   one end of the screen to the other
			pInst->posCurr.x = AEWrap(pInst->posCurr.x, AEGfxGetWinMinX() - SHIP_SIZE,
														AEGfxGetWinMaxX() + SHIP_SIZE);
			pInst->posCurr.y = AEWrap(pInst->posCurr.y, AEGfxGetWinMinY() - SHIP_SIZE,
														AEGfxGetWinMaxY() + SHIP_SIZE);
		}

		// Wrap asteroids here
		if (pInst->pObject->type == TYPE_ASTEROID)
		{
			pInst->posCurr.x = AEWrap(pInst->posCurr.x, AEGfxGetWinMinX() - ASTEROID_SIZE,
				AEGfxGetWinMaxX() + ASTEROID_SIZE);
			pInst->posCurr.y = AEWrap(pInst->posCurr.y, AEGfxGetWinMinY() - ASTEROID_SIZE,
				AEGfxGetWinMaxY() + ASTEROID_SIZE);
		}

		// Remove bullets that go out of bounds
		if (pInst->pObject->type == TYPE_BULLET)
		{
			if ((pInst->posCurr.x < AEGfxGetWinMinX()) || (pInst->posCurr.x > AEGfxGetWinMaxX())
				|| (pInst->posCurr.y < AEGfxGetWinMinY()) || pInst->posCurr.y > AEGfxGetWinMaxY())
			{
				bullet_list.erase(std::remove_if(bullet_list.begin(), bullet_list.end(),
					[pInst](GameObjInst* bullet) {
						return bullet == pInst;
					}), bullet_list.end());

				gameObjInstDestroy(pInst);
			}
		}
	}


	

	// =====================================
	// calculate the matrix for all objects
	// =====================================

	for (unsigned long i = 0; i < GAME_OBJ_INST_NUM_MAX; i++)
	{
		GameObjInst* pInst = sGameObjInstList + i;
		AEMtx33		 trans, rot, scale;

		/*UNREFERENCED_PARAMETER(trans);
		UNREFERENCED_PARAMETER(rot);
		UNREFERENCED_PARAMETER(scale);*/

		// skip non-active object
		if ((pInst->flag & FLAG_ACTIVE) == 0)
			continue;

		// Compute the scaling matrix
		AEMtx33Scale(&scale, pInst->scale, pInst->scale);

		// Compute the rotation matrix 
		AEMtx33Rot(&rot, pInst->dirCurr);

		// Compute the translation matrix
		AEMtx33Trans(&trans, pInst->posCurr.x, pInst->posCurr.y);

		// Concatenate the 3 matrix in the correct order in the object instance's "transform" matrix
		AEMtx33Concat(&pInst->transform, &rot, &scale);
		AEMtx33Concat(&pInst->transform, &trans, &pInst->transform);
	}
}

/******************************************************************************/
/*!
	"Draw" function of this state
*/
/******************************************************************************/
void GameStateAsteroidsDraw(void)
{
	char strBuffer[1024];

	AEGfxSetRenderMode(AE_GFX_RM_COLOR);
	AEGfxTextureSet(NULL, 0, 0);

	// draw all object instances in the list
	for (unsigned long i = 0; i < GAME_OBJ_INST_NUM_MAX; i++)
	{
		GameObjInst* pInst = sGameObjInstList + i;

		// skip non-active object
		if ((pInst->flag & FLAG_ACTIVE) == 0)
			continue;

		if (pInst->pObject->type == TYPE_ASTEROID)
		{
			AEGfxSetRenderMode(AE_GFX_RM_TEXTURE);
			AEGfxSetTransform(pInst->transform.m);
			AEGfxSetTintColor(1.0f, 1.0f, 1.0f, 1.0f);
			AEGfxTextureSet(asteroidTex, 0.0f, 0.0f);
			AEGfxSetTransparency(1.0);
			AEGfxMeshDraw(pInst->pObject->pMesh, AE_GFX_MDM_TRIANGLES);
			continue;
		}
		if (pInst->pObject->type == TYPE_BULLET)
		{
			AEGfxSetRenderMode(AE_GFX_RM_TEXTURE);
			AEGfxSetTransform(pInst->transform.m);
			AEGfxSetTintColor(1.0f, 1.0f, 1.0f, 1.0f);
			AEGfxTextureSet(bulletTex, 0.0f, 0.0f);
			AEGfxSetTransparency(1.0);
			AEGfxMeshDraw(pInst->pObject->pMesh, AE_GFX_MDM_TRIANGLES);
			continue;
		}
		if (pInst->pObject->type == TYPE_LIVES)
		{
			AEGfxSetRenderMode(AE_GFX_RM_TEXTURE);
			AEGfxSetTransform(pInst->transform.m);
			AEGfxSetTintColor(1.0f, 1.0f, 1.0f, 1.0f);
			AEGfxTextureSet(livesTex, 0.0f, 0.0f);
			AEGfxSetTransparency(1.0);
			AEGfxMeshDraw(pInst->pObject->pMesh, AE_GFX_MDM_TRIANGLES);
			continue;
		}

		// render each object accordingly
		AEGfxSetRenderMode(AE_GFX_RM_COLOR);
		AEGfxTextureSet(nullptr, 0, 0);
		AEGfxSetBlendMode(AE_GFX_BM_BLEND);
		AEGfxSetTransparency(1.0f);
		AEGfxSetTintColor(1.0f, 1.0f, 1.0f, 1.0f);
		AEGfxSetTransform(pInst->transform.m);
		AEGfxMeshDraw(pInst->pObject->pMesh, AE_GFX_MDM_TRIANGLES);
	}

	//You can replace this condition/variable by your own data.
	//The idea is to display any of these variables/strings whenever a change in their value happens
	if (onValueChange)
	{
		sprintf_s(strBuffer, "Score: %d", sScore);
		printf("%s \n", strBuffer);

		sprintf_s(strBuffer, " Left: %d", sShipLives >= 0 ? sShipLives : 0);
		printf("%s \n", strBuffer);

		// display the game over message
		if (sShipLives <= 0)
		{
			printf("       YOU LOSE       \n");
		}
		if (sScore >= SCORE_WIN)
		{
			printf("        VICTORY ROYALE       \n");
		}
		onValueChange = false;
	}

	// text that is drawn every frame
	f32 textWidth, textHeight;
	textWidth = 0.0f;
	textHeight = 0.0f;

	sprintf_s(strBuffer, "Score: %d", sScore);
	AEGfxGetPrintSize(font, strBuffer, 1.0f, textWidth, textHeight);
	AEGfxSetBlendMode(AE_GFX_BM_BLEND);
	AEGfxPrint(font, strBuffer, 0.99f - textWidth, -0.99f + textHeight, 1, 255, 255, 255);

	sprintf_s(strBuffer, "Life Left: %d", sShipLives >= 0 ? sShipLives : 0);
	AEGfxSetBlendMode(AE_GFX_BM_BLEND);
	AEGfxPrint(font, strBuffer, -0.99f, 0.99f - textHeight, 1, 255, 255, 255);

	// text that is drawn at the end of the game
	if (sShipLives <= 0)
	{
		AEGfxPrint(font, "YOU LOSE", -0.3f, 0.0f, 1, 255, 255, 255);

		sprintf_s(strBuffer, "Score: %d / 8000", sScore);
		AEGfxGetPrintSize(font, strBuffer, 1.0f, textWidth, textHeight);
		AEGfxSetBlendMode(AE_GFX_BM_BLEND);
		AEGfxPrint(font, strBuffer, -0.3f, -0.05f - textHeight, 1, 255, 255, 255);
	}
	if (sScore >= SCORE_WIN)
	{
		AEGfxPrint(font, "THATS A WIN CHIEF", -0.3f, 0.0f, 1, 255, 255, 255);

		sprintf_s(strBuffer, "Score: %d / 8000", sScore);
		AEGfxGetPrintSize(font, strBuffer, 1.0f, textWidth, textHeight);
		AEGfxSetBlendMode(AE_GFX_BM_BLEND);
		AEGfxPrint(font, strBuffer, -0.3f, -0.05f - textHeight, 1, 255, 255, 255);

	}
}

/******************************************************************************/
/*!
	"Free" function of this state
*/
/******************************************************************************/
void GameStateAsteroidsFree(void)
{
	// kill all object instances in the array using "gameObjInstDestroy"
	for (unsigned long i = 0; i < sGameObjInstNum; ++i)
		gameObjInstDestroy(sGameObjInstList + i);
}

/******************************************************************************/
/*!
	"Unload" function of this state
*/
/******************************************************************************/
void GameStateAsteroidsUnload(void)
{
	// free all mesh data (shapes) of each object using "AEGfxTriFree"
	for (unsigned long i = 0; i < sGameObjNum; ++i)
	
		AEGfxMeshFree(sGameObjList[i].pMesh);
		AEGfxDestroyFont(font);
		AEGfxTextureUnload(asteroidTex);
		AEGfxTextureUnload(bulletTex);
		AEGfxTextureUnload(livesTex);
	
}

/******************************************************************************/
/*!
	Initiates and sets active a game object instance
*/
/******************************************************************************/
GameObjInst* gameObjInstCreate(unsigned long type,
							  float scale,
							  AEVec2* pPos,
							  AEVec2* pVel,
							  float dir)
{
	AEVec2 zero;
	AEVec2Zero(&zero);

	AE_ASSERT_PARM(type < sGameObjNum);

	// loop through the object instance list to find a non-used object instance
	for (unsigned long i = 0; i < GAME_OBJ_INST_NUM_MAX; i++)
	{
		GameObjInst* pInst = sGameObjInstList + i;

		// check if current instance is not used
		if (pInst->flag == 0)
		{
			// it is not used => use it to create the new instance
			pInst->pObject = sGameObjList + type;
			pInst->flag = FLAG_ACTIVE;
			pInst->scale = scale;
			pInst->posCurr = pPos ? *pPos : zero;
			pInst->velCurr = pVel ? *pVel : zero;
			pInst->dirCurr = dir;

			// return the newly created instance
			return pInst;
		}
	}

	// cannot find empty slot => return 0
	return 0;
}

/******************************************************************************/
/*!
	Sets a game object instance to inactive
*/
/******************************************************************************/
void gameObjInstDestroy(GameObjInst* pInst)
{
	// if instance is destroyed before, just return
	if (pInst->flag == 0)
		return;

	// zero out the flag
	pInst->flag = 0;
}

/******************************************************************************/
/*!
	Spawns count number of asteroids randomly
*/
/******************************************************************************/
void spawnAsteroid(unsigned int count)
{
	for (unsigned int i = 0; i < count; ++i)
	{
		// for activating object instance 
		AEVec2 pos, vel;

		// random size
		float size = AERandFloat() * ASTEROID_SIZE + ASTEROID_SIZE;

		// random rotation
		float rot = AERandFloat() * 360.0f;

		// random direction
		AEVec2Set(&vel, cosf(rot), sinf(rot));

		// random speed 
		int dir = (int)(AERandFloat() * 2.0f);
		switch (dir)
		{
		case 0:

		case 1:
			AEVec2Scale(&vel, &vel, ASTEROID_SPEED + AERandFloat() * ASTEROID_SPEED);
			break;

		case 2:
			AEVec2Scale(&vel, &vel, ASTEROID_SPEED - AERandFloat() * -ASTEROID_SPEED);
			break;

		default:
			break;

		}

		// random position,   outside the window
		// random window side
		int side = (int)(AERandFloat() * 5.0f);

		// set half window width and height global variables
		float HALF_WIDTH = (float)AEGetWindowWidth() / 2.0f;	// half screen width 

		float HALF_HEIGHT = (float)AEGetWindowHeight() / 2.0f;	// half screen height

		// spawn   selected side
		switch (side)
		{
		case 0: // fall through
		case 1: //   top left
			AEVec2Set(&pos, (-AERandFloat() * HALF_WIDTH - size), (HALF_HEIGHT + size));
			break;

		case 2: //   top right
			AEVec2Set(&pos, (AERandFloat() * HALF_WIDTH + size), (HALF_HEIGHT + size));
			break;

		case 3: //   bottom left
			AEVec2Set(&pos, (-AERandFloat() * HALF_WIDTH - size), (-HALF_HEIGHT - size));
			break;

		case 4:

		case 5: //   bottom right
			AEVec2Set(&pos, (AERandFloat() * HALF_WIDTH + size), (-HALF_HEIGHT - size));
			break;

		default:
			break;

		}

		// spawn asteroid by activating a game object instance
		gameObjInstCreate(TYPE_ASTEROID, size, &pos, &vel, rot);
	}
}

/******************************************************************************/
/*!
	Spawns count number of helath pickups randomly
*/
/******************************************************************************/
void spawnLives(unsigned int count)
{
	for (unsigned int i = 0; i < count; ++i)
	{
		// for activating object instance 
		AEVec2 pos, vel{ 0.0f, 0.0f };
		// random position,   outside the window
		// random window side
		int side = (int)(AERandFloat() * 5.0f);

		// set half window width and height global variables
		float HALF_WIDTH = (float)AEGetWindowWidth() / 2.0f;	// half screen width 

		float HALF_HEIGHT = (float)AEGetWindowHeight() / 2.0f;	// half screen height

		// spawn   selected side
		switch (side)
		{
		case 0: // fall through

		case 1: //   top left
			AEVec2Set(&pos, (AERandFloat() * HALF_WIDTH), (AERandFloat() * HALF_HEIGHT));
			break;

		case 2: //   top right
			AEVec2Set(&pos, (-AERandFloat() * HALF_WIDTH), (AERandFloat() * HALF_HEIGHT));
			break;

		case 3: //   bottom left
			AEVec2Set(&pos, (AERandFloat() * HALF_WIDTH), (-AERandFloat() * HALF_HEIGHT));
			break;

		case 4:

		case 5: //   bottom right
			AEVec2Set(&pos, (-AERandFloat() * HALF_WIDTH), (-AERandFloat() * HALF_HEIGHT));
			break;

		default:
			break;

		}

		// spawn asteroid by activating a game object instance
		gameObjInstCreate(TYPE_LIVES, LIVES_SIZE, &pos, &vel, 0.0f);
	}
}

void AsteroidsDataTransfer(SOCKET udp_socket)
{
	BOOL bOptVal = TRUE;
	if (setsockopt(udp_socket, SOL_SOCKET, SO_BROADCAST, (char*)&bOptVal, sizeof(bOptVal)) == SOCKET_ERROR)
	{
		std::cerr << "setsockopt failed with error: " << WSAGetLastError() << '\n';
		closesocket(udp_socket);
		WSACleanup();
		return;
	}
	// Define the timeout for each attempt in milliseconds
	const int timeout_ms = 1000;

	// Set the receive timeout option for the socket
	DWORD timeout = timeout_ms;
	if (setsockopt(udp_socket, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof(timeout)) == SOCKET_ERROR)
	{
		std::cerr << "setsockopt failed with error: " << WSAGetLastError() << '\n';
		closesocket(udp_socket);
		WSACleanup();
		return;
	}

	while(!av_game_over)
	{
		if(!av_connected || !av_start)
		{
			continue;
		}

		// Setup the broadcast address
		sockaddr_in broadcastAddr;
		broadcastAddr.sin_family = AF_INET;
		broadcastAddr.sin_port = htons(9000); // 9000 for Server port auto connect
		broadcastAddr.sin_addr.s_addr = INADDR_BROADCAST;

		int num_bullets = bullet_list.size();
		//std::vector<AEVec2> player_pos(1, spShip->posCurr);
		std::vector<Player> player(1);
		player[0].player_id = av_player_num;
		player[0].position = spShip->posCurr;
		player[0].velocity = spShip->velCurr;
		player[0].direction = spShip->dirCurr;
		player[0].num_bullets = num_bullets;
		player[0].shoot = AEInputCheckTriggered(AEVK_SPACE) ? 1 : 0;

		std::vector<Bullet> bullets(num_bullets);
		for (int i{}; i < num_bullets; ++i)
		{
			bullets[i].player_id = av_player_num;
			bullets[i].position = bullet_list[i]->posCurr;
		}

		CMDID send_id = SEND_PLAYERS;
		size_t dataSize = sizeof(Player) + (sizeof(Bullet) * num_bullets) + sizeof(send_id);
		std::vector<char> buffer(dataSize);
		memcpy(buffer.data(), &send_id, sizeof(send_id));
		memcpy(buffer.data() + sizeof(send_id), player.data(), sizeof(Player));
		memcpy(buffer.data() + sizeof(send_id) + sizeof(Player), bullets.data(), sizeof(Bullet) * num_bullets);

		if (sendto(udp_socket, buffer.data(), dataSize, 0, (SOCKADDR*)&broadcastAddr, sizeof(broadcastAddr)) == SOCKET_ERROR)
		{
			std::cerr << "sendto failed with error: " << WSAGetLastError() << '\n';
			closesocket(udp_socket);
			WSACleanup();
			return;
		}

		// Receive the response from the server
		sockaddr_in serverAddr;
		int serverAddrSize = sizeof(serverAddr);

		// Receive the vector elements
		CMDID receive_id;
		//std::vector<AEVec2> receivedPositions(av_player_max);

		size_t data_size = 4096;
		std::vector<char> receive_buffer(data_size);
		int bytesReceived = recvfrom(udp_socket, receive_buffer.data(), data_size, 0, (SOCKADDR*)&serverAddr, &serverAddrSize);

		if (bytesReceived != SOCKET_ERROR) {
			char* bufferPtr = receive_buffer.data();
			memcpy(&receive_id, receive_buffer.data(), sizeof(receive_id));
			bufferPtr += sizeof(receive_id);
			newDataReceived = true;
			switch (receive_id)
			{
			case SEND_PLAYERS: {
				std::vector<Player> receivedplayer(av_player_max);
				memcpy(receivedplayer.data(), receive_buffer.data() + sizeof(receive_id), av_player_max * sizeof(Player));
				for (int i{}; i < player_list.size(); ++i)
				{
					if (player_list[i] == nullptr) continue;
					player_list[i]->id = receivedplayer[i].player_id;
					player_list[i]->posCurr = receivedplayer[i].position;
					player_list[i]->velCurr = receivedplayer[i].velocity;
					player_list[i]->dirCurr = receivedplayer[i].direction;
					newPathData temp;
					temp.newPosition = player_list[i]->posCurr;
					temp.newVelocity = player_list[i]->velCurr;
					temp.newDir = player_list[i]->dirCurr;
					pathData.push_back(std::make_pair(i,temp));

					if (receivedplayer[i].shoot) {
						AEVec2 added;

						// Get the bullet's direction according to the ship's direction
						AEVec2Set(&added, cosf(player_list[i]->dirCurr), sinf(player_list[i]->dirCurr));
						AEVec2Normalize(&added, &added);

						// Set the velocity
						AEVec2Scale(&added, &added, BULLET_SPEED);

						// Create an instance
						//gameObjInstCreate(TYPE_BULLET, BULLET_SIZE, &spShip->posCurr, &added, spShip->dirCurr);
						bullet_list.emplace_back(gameObjInstCreate(TYPE_BULLET, BULLET_SIZE, &player_list[i]->posCurr, &added, player_list[i]->dirCurr));
					}
				}
				bufferPtr += av_player_max * sizeof(Player);
				std::vector<Bullet> receivedBullets;
				while (bufferPtr < receive_buffer.data() + bytesReceived) {
					Bullet bullet;
					memcpy(&bullet, bufferPtr, sizeof(Bullet));
					receivedBullets.push_back(bullet);
					bufferPtr += sizeof(Bullet);
				}

				for (int i{}; i < receivedBullets.size(); ++i)
				{
					/*std::cout << "Player id: " << receivedBullets[i].player_id << "\nPos: x,"
						<< receivedBullets[i].position.x << " y, " << receivedBullets[i].position.y << "\n";*/
				}

				break;
			}

			case SEND_BULLETS: {

			}
 			default:
				break;
			}
		}
	}
}

void initMultiPlayer(int num_player)
{
	player_list.resize(num_player);
	for(int i {}; i < num_player; ++i)
	{
		if(i == 0) continue;
		player_list[i] = gameObjInstCreate(TYPE_SHIP, SHIP_SIZE, nullptr, nullptr, 0.f);
	}
}
