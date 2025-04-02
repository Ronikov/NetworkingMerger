#pragma once
#include <unordered_map>
#include "engine/math.hpp"   // math
#include "engine/mesh.hpp"   // mesh

// ---------------------------------------------------------------------------
// Defines

#define GAME_OBJ_NUM_MAX 32
#define GAME_OBJ_INST_NUM_MAX 2048

#define AST_NUM_MIN 2         // minimum number of asteroid alive
#define AST_NUM_MAX 32        // maximum number of asteroid alive
#define AST_SIZE_MAX 200.0f   // maximum asterois size
#define AST_SIZE_MIN 50.0f    // minimum asterois size
#define AST_VEL_MAX 100.0f    // maximum asterois velocity
#define AST_VEL_MIN 50.0f     // minimum asterois velocity
#define AST_CREATE_DELAY 0.1f // delay of creation between one asteroid to the next
#define AST_SPECIAL_RATIO 4   // number of asteroid for each 'special'
#define AST_SHIP_RATIO 100    // number of asteroid for each ship
#define AST_LIFE_MAX 10       // the life of the biggest asteroid (smaller one is scaled accordingly)
#define AST_VEL_DAMP 1E-8f    // dampening to use if the asteroid velocity is above the maximum
#define AST_TO_SHIP_ACC 0.01f // how much acceleration to apply to steer the asteroid toward the ship

#define SHIP_INITIAL_NUM 3          // initial number of ship
#define SHIP_SPECIAL_NUM 20         //
#define SHIP_SIZE 30.0f             // ship size
#define SHIP_ACCEL_FORWARD 100.0f   // ship forward acceleration (in m/s^2)
#define SHIP_ACCEL_BACKWARD -100.0f // ship backward acceleration (in m/s^2)
#define SHIP_DAMP_FORWARD 0.55f     // ship forward dampening
#define SHIP_DAMP_BACKWARD 0.05f    // ship backward dampening
#define SHIP_ROT_SPEED (1.0f * PI)  // ship rotation speed (degree/second)

#define BULLET_SPEED 1000.0f // bullet speed (m/s)
#define BULLET_SIZE 10.0f

#define BOMB_COST 0        // cost to fire a bomb
#define BOMB_RADIUS 250.0f // bomb radius
#define BOMB_LIFE 5.0f     // how many seconds the bomb will be alive
#define BOMB_SIZE 1.0f

#define MISSILE_COST 0
#define MISSILE_ACCEL 1000.0f // missile acceleration
#define MISSILE_TURN_SPEED PI // missile turning speed (radian/sec)
#define MISSILE_DAMP 1.5E-6f  // missile dampening
#define MISSILE_LIFE 10.0f    // how many seconds the missile will be alive
#define MISSILES_SIZE 10.0f   // size of missile.

#define PTCL_SCALE_DAMP 0.05f // particle scale dampening
#define PTCL_VEL_DAMP 0.05f   // particle velocity dampening

#define COLL_COEF_OF_RESTITUTION 1.0f // collision coefficient of restitution
#define COLL_RESOLVE_SIMPLE 1

// ---------------------------------------------------------------------------
enum
{
    // list of game object types
    TYPE_SHIP = 0,
    TYPE_BULLET,
    TYPE_BOMB,
    TYPE_MISSILE,
    TYPE_ASTEROID,
    TYPE_STAR,

    TYPE_PTCL_WHITE,
    TYPE_PTCL_YELLOW,
    TYPE_PTCL_RED,

    TYPE_NUM,

    PTCL_EXHAUST,
    PTCL_EXPLOSION_S,
    PTCL_EXPLOSION_M,
    PTCL_EXPLOSION_L,
};

// ---------------------------------------------------------------------------
// object flag definition

#define FLAG_ACTIVE 0x00000001
#define FLAG_LIFE_CTR_S 8
#define FLAG_LIFE_CTR_M 0x000000FF

// ---------------------------------------------------------------------------
// Struct/Class definitions

struct GameObj
{
    uint32_t      type;  // object type
    engine::mesh* pMesh; // pbject
};

// ---------------------------------------------------------------------------

struct GameObjInst
{
    GameObj* pObject;   // pointer to the 'original'
    int      m_id = 0;  // id of the net interface or id of the object itself
    uint32_t flag;      // bit flag or-ed together
    float    life;      // object 'life'
    float    scale;     //
    vec2     posCurr;   // object current position
    vec2     velCurr;   // object current velocity
    float    dirCurr;   // object current direction
    mat4     transform; // object drawing matrix
    void* pUserData; // pointer to custom data specific for each object type
};


// ---------------------------------------------------------------------------
// Static variables
struct Game
{
    //bools to check the state of the game
    std::chrono::nanoseconds timer_to_disconect{ std::chrono::seconds(30) };
    bool game_won = false;
    bool game_ended = false;
    int points_to_win = 50;
    int won_id = 0;

    //different colors for the players
    vec4 colors[6]{
        {1.0f, 0.0f, 0.0f, 1.0f},
        {0.0f, 1.0f, 0.0f, 1.0f},
        {0.0f, 0.0f, 1.0f, 1.0f},
        {1.0f, 1.0f, 0.0f, 1.0f},
        {0.0f, 1.0f, 1.0f, 1.0f},
        {1.0f, 0.0f, 1.0f, 1.0f}
    };

    //variables when a players dies
    bool player_inmortal = false;
    float timer = 0.0f;
    std::chrono::nanoseconds inmortal_timer{ std::chrono::seconds(3) };
     
    //make the network a singleton
    static Game& Instance()
    {
        static Game gamesys;
        return gamesys;
    }

    // list of original object
    GameObj  sGameObjList[GAME_OBJ_NUM_MAX];
    uint32_t sGameObjNum;

    // list of object instances
    GameObjInst sGameObjInstList[GAME_OBJ_INST_NUM_MAX];
    uint32_t    sGameObjInstNum;

    // pointer ot the ship object
    int asteroids_id = 0;
    std::unordered_map<int, GameObjInst*> mAsteroids;
    std::unordered_map<int, GameObjInst*> mShips;
    std::unordered_map<int, uint32_t> mScores;
    GameObjInst* spShip;

    // keep track when the last asteroid was created
    float sAstCreationTime;

    // keep track the total number of asteroid active and the maximum allowed
    uint32_t sAstCtr;
    uint32_t sAstNum;

    // current ship rotation speed
    float sShipRotSpeed;

    // number of ship available (lives, 0 = game over)
    long sShipCtr;

    // number of special attack
    long sSpecialCtr;

    float sGameStateChangeCtr;

    // Window size
    int gAEWinMinX = 0;
    int gAEWinMaxX = 640;
    int gAEWinMinY = 0;
    int gAEWinMaxY = 480;


    // ---------------------------------------------------------------------------

    // function to 'load' object data
    void loadGameObjList();

    // function to create/destroy a game object object
    GameObjInst* gameObjInstCreate(uint32_t type, float scale, vec2* pPos, vec2* pVel, float dir, bool forceCreate, int m_id = 0);
    void         gameObjInstDestroy(GameObjInst* pInst);

    // function to create asteroid
    GameObjInst* astCreate(GameObjInst* pSrc, bool is_child = false);

    // function to calculate the object's velocity after collison
    void resolveCollision(GameObjInst* pSrc, GameObjInst* pDst, vec2* pNrm);

    // function to create the particles
    void sparkCreate(uint32_t type, vec2* pPos, uint32_t count, float angleMin, float angleMax, float srcSize = 0.0f, float velScale = 1.0f, vec2* pVelInit = 0);

    // function for the missile to find a new target
    GameObjInst* missileAcquireTarget(GameObjInst* pMissile);

    // ---------------------------------------------------------------------------
    void Unload();
    void Free();
    void Draw();
    void Update();
    void Init();
    void Load();

private:
    Game() {}
};

#define mGame (Game::Instance())

void GameStatePlayLoad(void);
void GameStatePlayInit(void);
void GameStatePlayUpdate(void);
void GameStatePlayDraw(void);
void GameStatePlayFree(void);
void GameStatePlayUnload(void);