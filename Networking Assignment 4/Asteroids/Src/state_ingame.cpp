#include <cstring>           // memset
#include <cassert>           // assert
#include <cstdio>            // sprintf
#include <iostream>          // cout

#include "../engine/opengl.hpp" // opengl, glfw
#include "../engine/shader.hpp" // shader
#include "../engine/window.hpp" // window
#include "../engine/font.hpp"   // font
#include "../src/game.hpp"     // game features
#include "network/system/networking.hpp"
#include "state_ingame.h"
#include "../Src/TimeMgr/Time.h"

void GameStatePlayUnload()
{
    mGame.Unload();
}
void GameStatePlayFree()
{
    mGame.Free();
}
void GameStatePlayDraw()
{
    mGame.Draw();
}
void GameStatePlayUpdate()
{
    mGame.Update();
}
void GameStatePlayInit()
{
    mGame.Init();
}
void GameStatePlayLoad()
{
    mGame.Load();
}


void Game::Load(void)
{
    // zero the game object list
    std::memset(sGameObjList, 0, sizeof(GameObj) * GAME_OBJ_NUM_MAX);
    sGameObjNum = 0;

    // zero the game object instance list
    std::memset(sGameObjInstList, 0, sizeof(GameObjInst) * GAME_OBJ_INST_NUM_MAX);
    sGameObjInstNum = 0;

    // load/create the mesh data
    loadGameObjList();

    // initialize the initial number of asteroid
    sAstCtr = 0;
}

// ---------------------------------------------------------------------------

void Game::Init(void)
{
    // reset the number of current asteroid and the total allowed
    sAstCtr = 0;
    sAstNum = AST_NUM_MIN;

    // create the main ship
    if (NetMgr.Im_server)
    {   
        spShip = gameObjInstCreate(TYPE_SHIP, SHIP_SIZE, 0, 0, 0.0f, true);
        mShips[0] = spShip;

        assert(spShip);
    }

    // get the time the asteroid is created
    sAstCreationTime = game::instance().game_time();

    // generate the initial asteroid
    if(NetMgr.Im_server)
        for (uint32_t i = 0; i < sAstNum; i++)
            astCreate(0);

    // reset the score and the number of 
    for (uint32_t i = 0; i < mShips.size(); i++)
        mScores[i] = 0;

    sShipCtr    = SHIP_INITIAL_NUM;

    // reset the delay to switch to the result state after game over
    sGameStateChangeCtr = 2.0f;
}

// ---------------------------------------------------------------------------

void Game::Update(void)
{
    //end the program after some time after the game neded
    if (game_ended || game_won)
    {
        timer += TimeMgr.GetDt();
        if (timer >= timer_to_disconect.count())
            game::instance().game_end = true;
    }

    //check if the game ended
    if (NetMgr.Im_server && !game_ended && !game_won)
    {
        if (mShips.empty())
        {
            game_ended = true;
            NetMgr.BroadCastMsg(network::net_action::NET_GAME_OVER, true);
        }
        
        for (auto& it : mScores)
        {
            if (it.second >= points_to_win)
            {
                game_won = true;
                won_id = it.first;
                char msg[sizeof(int)] = {};
                memcpy(&msg, &it.first, sizeof(int));

                //remove all the players
                for (auto& it : mShips)
                    gameObjInstDestroy(it.second);
                mShips.clear();
                spShip = nullptr;

                NetMgr.BroadCastMsg(network::net_action::NET_GAME_WON, true, msg, sizeof(int));
                break;
            }
        }
    }

    //use this when a player just died so it doesnt die straght forward when spawning
    if (player_inmortal)
    {
        timer += TimeMgr.GetDt();
        if (timer >= inmortal_timer.count())
        {
            timer = 0.0f;
            player_inmortal = false;
        }
    }

    //update ship position in other simulations
    if(spShip)
        NetMgr.BroadCastMsg(network::net_action::NET_PLAYER_UPDATE);

    //if server send the asteroids positions in other simulations
    if(NetMgr.Im_server)
        NetMgr.BroadCastMsg(network::net_action::NET_ASTEROID_UPDATE);

    // =================
    // update the input
    // =================
    float const dt = game::instance().dt();
    if (spShip == 0) {
        sGameStateChangeCtr -= dt;

        if (sGameStateChangeCtr < 0.0) {
            // TODO: Change to RESULT GAME STATE
        }
    } else {
        if (game::instance().input_key_pressed(GLFW_KEY_UP)) {
            vec2 pos, dir;
            dir             = {glm::cos(spShip->dirCurr), glm::sin(spShip->dirCurr)};
            pos             = dir;
            dir             = dir * (SHIP_ACCEL_FORWARD * dt);
            spShip->velCurr = spShip->velCurr + dir;
            spShip->velCurr = spShip->velCurr * glm::pow(SHIP_DAMP_FORWARD, dt);

            pos = pos * -spShip->scale;
            pos = pos + spShip->posCurr;
        }
        if (game::instance().input_key_pressed(GLFW_KEY_DOWN)) {
            vec2 dir;

            dir             = {glm::cos(spShip->dirCurr), glm::sin(spShip->dirCurr)};
            dir             = dir * SHIP_ACCEL_BACKWARD * dt;
            spShip->velCurr = spShip->velCurr + dir;
            spShip->velCurr = spShip->velCurr * glm::pow(SHIP_DAMP_BACKWARD, dt);
        }
        if (game::instance().input_key_pressed(GLFW_KEY_LEFT)) {
            sShipRotSpeed += (SHIP_ROT_SPEED - sShipRotSpeed) * 0.1f;
            spShip->dirCurr += sShipRotSpeed * dt;
            spShip->dirCurr = wrap(spShip->dirCurr, -PI, PI);
        } else if (game::instance().input_key_pressed(GLFW_KEY_RIGHT)) {
            sShipRotSpeed += (SHIP_ROT_SPEED - sShipRotSpeed) * 0.1f;
            spShip->dirCurr -= sShipRotSpeed * dt;
            spShip->dirCurr = wrap(spShip->dirCurr, -PI, PI);
        } else {
            sShipRotSpeed = 0.0f;
        }
        if (game::instance().input_key_triggered(GLFW_KEY_SPACE)) {
            vec2 vel;

            vel = {glm::cos(spShip->dirCurr), glm::sin(spShip->dirCurr)};
            vel = vel * BULLET_SPEED;

            gameObjInstCreate(TYPE_BULLET, BULLET_SIZE, &spShip->posCurr, &vel, spShip->dirCurr, true, NetMgr.system->m_id);
            NetMgr.BroadCastMsg(network::net_action::NET_PLAYER_SHOT);
        }
    }

    // ==================================
    // create new asteroids if necessary
    // ==================================

    if ((sAstCtr < sAstNum) && NetMgr.Im_server &&
        ((game::instance().game_time() - sAstCreationTime) > AST_CREATE_DELAY)) {
        // keep track the last time an asteroid is created
        sAstCreationTime = game::instance().game_time();

        // create an asteroid
        astCreate(0);
    }

    // ===============
    // update physics
    // ===============

    for (uint32_t i = 0; i < GAME_OBJ_INST_NUM_MAX; i++) {
        GameObjInst* pInst = sGameObjInstList + i;

        // skip non-active object
        if ((pInst->flag & FLAG_ACTIVE) == 0)
            continue;

        // update the position
        pInst->posCurr += pInst->velCurr * dt;
    }

    // ===============
    // update objects
    // ===============

    for (uint32_t i = 0; i < GAME_OBJ_INST_NUM_MAX; i++) {
        GameObjInst* pInst = sGameObjInstList + i;

        // skip non-active object
        if ((pInst->flag & FLAG_ACTIVE) == 0)
            continue;

        // check if the object is a ship
        if (pInst->pObject->type == TYPE_SHIP) {
            // warp the ship from one end of the screen to the other
            pInst->posCurr.x = wrap(pInst->posCurr.x, gAEWinMinX - SHIP_SIZE, gAEWinMaxX + SHIP_SIZE);
            pInst->posCurr.y = wrap(pInst->posCurr.y, gAEWinMinY - SHIP_SIZE, gAEWinMaxY + SHIP_SIZE);
        }
        // check if the object is an asteroid
        else if (pInst->pObject->type == TYPE_ASTEROID) {
            vec2  u;
            float uLen;

            // warp the asteroid from one end of the screen to the other
            pInst->posCurr.x = wrap(pInst->posCurr.x, gAEWinMinX - AST_SIZE_MAX, gAEWinMaxX + AST_SIZE_MAX);
            pInst->posCurr.y = wrap(pInst->posCurr.y, gAEWinMinY - AST_SIZE_MAX, gAEWinMaxY + AST_SIZE_MAX);

            //update asteroids if we are the server
            if (NetMgr.Im_server)
            {
                //get the closest ship to the asteroid
                int closest_id = -1;
                float min_dist = FLT_MAX;
                for (auto& it : mShips)
                {
                    float dist = glm::distance(pInst->posCurr, it.second->posCurr);
                    if (dist < min_dist)
                    {
                        closest_id = it.first;
                        min_dist = dist;
                    }
                }

                //if valid ship
                if (closest_id != -1)
                {
                    // pull the asteroid toward the closest ship a little bit
                    if (mShips[closest_id]) {
                        // apply acceleration propotional to the distance from the asteroid to
                        // the ship
                        u = mShips[closest_id]->posCurr - pInst->posCurr;
                        u = u * AST_TO_SHIP_ACC * dt;
                        pInst->velCurr = pInst->velCurr + u;
                    }
                }

                // if the asterid velocity is more than its maximum velocity, reduce its
                // speed
                if ((uLen = glm::length(pInst->velCurr)) > (AST_VEL_MAX * 2.0f)) {
                    u = pInst->velCurr * (1.0f / uLen) * (AST_VEL_MAX * 2.0f - uLen) * glm::pow(AST_VEL_DAMP, dt);
                    pInst->velCurr = pInst->velCurr + u;
                }
            }
        }
        // check if the object is a bullet
        else if (pInst->pObject->type == TYPE_BULLET) {
            // kill the bullet if it gets out of the screen
            if (!in_range(pInst->posCurr.x, gAEWinMinX - AST_SIZE_MAX, gAEWinMaxX + AST_SIZE_MAX) ||
                !in_range(pInst->posCurr.y, gAEWinMinY - AST_SIZE_MAX, gAEWinMaxY + AST_SIZE_MAX))
                gameObjInstDestroy(pInst);
        }
    }

    // ====================
    // check for collision
    // ====================
#if 1
    for (uint32_t i = 0; i < GAME_OBJ_INST_NUM_MAX; i++) {
        GameObjInst* pSrc = sGameObjInstList + i;

        // skip non-active object
        if ((pSrc->flag & FLAG_ACTIVE) == 0)
            continue;

        if (pSrc->pObject->type == TYPE_BULLET) 
        {
            for (uint32_t j = 0; j < GAME_OBJ_INST_NUM_MAX; j++) {
                GameObjInst* pDst = sGameObjInstList + j;

                // skip no-active and non-asteroid object
                if (((pDst->flag & FLAG_ACTIVE) == 0) ||
                    (pDst->pObject->type != TYPE_ASTEROID))
                    continue;

                if (point_in_aabb(pSrc->posCurr, pDst->posCurr, pDst->scale, pDst->scale) == false)
                    continue;

                if (NetMgr.Im_server) {
                    network::net_explosion exp{ pDst->m_id, 0, 0, pDst->scale, pSrc->dirCurr,pDst->posCurr };
                    std::vector<char> msg(sizeof(network::net_explosion));
                    memcpy(msg.data(), &exp, sizeof(network::net_explosion));
                    NetMgr.BroadCastMsg(network::net_action::NET_ASTEROID_DESTROY, true, msg.data(), msg.size());


                    //so nasty code but necessary
                    mScores[pSrc->m_id]++;
                    NetMgr.BroadCastMsg(network::net_action::NET_SCORE_UPDATE);

                    if ((mScores[pSrc->m_id] % AST_SHIP_RATIO) == 0)
                        sShipCtr++;
                    if (mScores[pSrc->m_id] == sAstNum * 5)
                        sAstNum = (sAstNum < AST_NUM_MAX) ? (sAstNum * 2) : sAstNum;
                    // destroy the asteroid
                    gameObjInstDestroy(pDst);
                } 
                else {
                    // impart some of the bullet velocity to the asteroid
                    pSrc->velCurr = pSrc->velCurr * 0.01f * (1.0f - pDst->scale / AST_SIZE_MAX);
                    pDst->velCurr = pDst->velCurr + pSrc->velCurr;
                }

                // destroy the bullet
                gameObjInstDestroy(pSrc);

                break;
            }
        } 
        else if (pSrc->pObject->type == TYPE_ASTEROID) 
{
            for (uint32_t j = 0; j < GAME_OBJ_INST_NUM_MAX; j++) 
            {
                GameObjInst* pDst = sGameObjInstList + j;
                float        d;
                vec2         nrm, u;

                // skip no-active and non-asteroid object
                if ((pSrc == pDst) || ((pDst->flag & FLAG_ACTIVE) == 0) ||
                    (pDst->pObject->type != TYPE_ASTEROID))
                    continue;

                // check if the object rectangle overlap
                d = aabb_vs_aabb(pSrc->posCurr, pSrc->scale, pSrc->scale, pDst->posCurr, pDst->scale, pDst->scale);
                //&nrm);

                if (d >= 0.0f)
                    continue;

                // adjust object position so that they do not overlap
                u             = nrm * d * 0.25f;
                pSrc->posCurr = pSrc->posCurr - u;
                pDst->posCurr = pDst->posCurr + u;

                // calculate new object velocities
                resolveCollision(pSrc, pDst, &nrm);
            }
        } 
        else if (pSrc->pObject->type == TYPE_SHIP && pSrc->m_id == NetMgr.system->m_id)
        {
            for (uint32_t j = 0; j < GAME_OBJ_INST_NUM_MAX; j++) 
            {
                GameObjInst* pDst = sGameObjInstList + j;

                // skip no-active and non-asteroid object
                if ((pSrc == pDst) || ((pDst->flag & FLAG_ACTIVE) == 0) ||
                    (pDst->pObject->type != TYPE_ASTEROID) || player_inmortal)
                    continue;

                // check if the object rectangle overlap
                if (aabb_vs_aabb(pSrc->posCurr, pSrc->scale, pSrc->scale, pDst->posCurr, pDst->scale, pDst->scale) == false)
                    continue;

                // reset the ship position and direction
                spShip->posCurr = {};
                spShip->velCurr = {};
                spShip->dirCurr = 0.0f;

                //make the player inmortal for 2 seconds 
                player_inmortal = true;

                // reduce the ship counter
                sShipCtr--;

                // if counter is less than 0, game over
                if (sShipCtr < 0) {
                    sGameStateChangeCtr = 2.0;
                    gameObjInstDestroy(spShip);
                    spShip = 0;
                    mShips.erase(NetMgr.system->m_id);
                    NetMgr.BroadCastMsg(network::net_action::NET_PLAYER_DEATH, true);
                    NetMgr.BroadCastMsg(network::net_action::NET_SCORE_UPDATE, true);
                }
                break;
            }
        }
    }
#endif
    // =====================================
    // calculate the matrix for all objects
    // =====================================

    for (uint32_t i = 0; i < GAME_OBJ_INST_NUM_MAX; i++) {
        GameObjInst* pInst = sGameObjInstList + i;
        glm::mat3    m;

        // skip non-active object
        if ((pInst->flag & FLAG_ACTIVE) == 0)
            continue;

        auto t           = glm::translate(vec3(pInst->posCurr.x, pInst->posCurr.y, 0));
        auto r           = glm::rotate(pInst->dirCurr, vec3(0, 0, 1));
        auto s           = glm::scale(vec3(pInst->scale, pInst->scale, 1));
        pInst->transform = t * r * s;
    }

    
}
// ---------------------------------------------------------------------------

void Game::Draw(void)
{
    auto window = game::instance().window();
    gAEWinMinX  = -window->size().x * 0.5f;
    gAEWinMaxX  = window->size().x * 0.5f;
    gAEWinMinY  = -window->size().y * 0.5f;
    gAEWinMaxY  = window->size().y * 0.5f;
    mat4 vp     = glm::ortho(float(gAEWinMinX), float(gAEWinMaxX), float(gAEWinMinY), float(gAEWinMaxY), 0.01f, 100.0f) * glm::lookAt(vec3(0, 0, 10), vec3(0, 0, 0), vec3(0, 1, 0));

    glViewport(0, 0, window->size().x, window->size().y);
    glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    glDisable(GL_CULL_FACE);

    char strBuffer[1024];
    mat4 tmp, tmpScale = glm::scale(glm::vec3{10, 10, 1});

    // draw all object in the list
    for (uint32_t i = 0; i < GAME_OBJ_INST_NUM_MAX; i++) {
        GameObjInst* pInst = sGameObjInstList + i;
        // skip non-active object
        if ((pInst->flag & FLAG_ACTIVE) == 0)
            continue;

        // if (pInst->pObject->type != TYPE_SHIP) continue;
        tmp = /*tmpScale * */ vp * sGameObjInstList[i].transform;
        game::instance().shader_default()->use();

        vec4 color = { 0.0f, 0.0f, 0.0f, 1.0f };
        if (pInst->pObject->type == TYPE_SHIP)
        {
            int color_id = pInst->m_id < 6 ? pInst->m_id : pInst->m_id % 6;
            color = colors[color_id];
        }
        game::instance().shader_default()->set_uniform(0, tmp);
        game::instance().shader_default()->set_uniform(1, color);
        sGameObjInstList[i].pObject->pMesh->draw();
    }
    
    // Render text
    auto w = gAEWinMaxX - gAEWinMinX;
    auto h = gAEWinMaxY - gAEWinMinY;
    vp = glm::ortho(float(0), float(gAEWinMaxX - gAEWinMinX), float(0), float(h), 0.01f, 100.0f) * glm::lookAt(vec3(0, 0, 10), vec3(0, 0, 0), vec3(0, 1, 0));

    if (!game_ended && !game_won)
    {
        for (auto& it : mScores)
        {
            int color_id = it.first < 6 ? it.first : it.first % 6;
            sprintf_s(strBuffer, "Score: %d", it.second);
            game::instance().font_default()->render(strBuffer, 10 + (150 * it.first), h - 10, 24, vp, colors[color_id]);
        }

        sprintf_s(strBuffer, "Level: %d", glm::log2<uint32_t>(sAstNum));
        game::instance().font_default()->render(strBuffer, 600, h - 30,24, vp);

        sprintf_s(strBuffer, "Life: %d", sShipCtr >= 0 ? sShipCtr : 0);
        game::instance().font_default()->render(strBuffer, 600, h - 10,  24, vp);

        // display the game over message
        if (sShipCtr < 0)
            game::instance().font_default()->render("       GAME OVER       ", 280, 260,  24, vp);
    }
    else
    {
        auto* window = game::instance().window();
        auto size = window->size();
        if(game_ended)
            game::instance().font_default()->render("GAME OVER", size.x/2 - 50, size.y - 100, 30, vp);
        if (game_won)
        {
            int color_id = won_id < 6 ? won_id : won_id % 6;
            std::string str("Player ");
            str += std::to_string(won_id);
            str += " Won The Game";
            game::instance().font_default()->render(str.c_str(), size.x / 2 - 70, size.y - 100, 30 , vp, colors[color_id]);
        }
         
        unsigned i = 1;
        for (auto& it : mScores)
        {
            int color_id = it.first < 6 ? it.first : it.first % 6;
            sprintf_s(strBuffer, ("Player" + std::to_string(it.first) + ": %d" + " points").c_str(), it.second);
            game::instance().font_default()->render(strBuffer, size.x / 2 - 50, size.y - 150 - 50 * i, 24 , vp, colors[color_id]);
            i++;
        }
    }
}

// ---------------------------------------------------------------------------

void Game::Free(void)
{
    // kill all object in the list
    for (uint32_t i = 0; i < GAME_OBJ_INST_NUM_MAX; i++)
        gameObjInstDestroy(sGameObjInstList + i);

    // reset asteroid count
    sAstCtr = 0;
}

// ---------------------------------------------------------------------------

void Game::Unload(void)
{
    // free all mesh
    for (uint32_t i = 0; i < sGameObjNum; i++) {
        delete sGameObjList[i].pMesh;
        sGameObjList[i].pMesh = nullptr;
    }
}

// ---------------------------------------------------------------------------
// Static function implementations

void Game::loadGameObjList()
{
    GameObj* pObj;

    // ================
    // create the ship
    // ================

    pObj       = sGameObjList + sGameObjNum++;
    pObj->type = TYPE_SHIP;

    {
        engine::mesh* mesh = new engine::mesh();
        mesh->add_triangle(engine::gfx_triangle(-0.5f, -0.5f, 0xFFFF0000, 0.0f, 0.0f, 0.5f, 0.0f, 0xFFFFFFFF, 0.0f, 0.0f, -0.5f, 0.5f, 0xFFFF0000, 0.0f, 0.0f));
        mesh->create();
        pObj->pMesh = mesh;
        AE_ASSERT_MESG(pObj->pMesh, "fail to create object!!");
    }

    // ==================
    // create the bullet
    // ==================

    pObj       = sGameObjList + sGameObjNum++;
    pObj->type = TYPE_BULLET;

    {
        engine::mesh* mesh = new engine::mesh();
        mesh->add_triangle(engine::gfx_triangle(-1.0f, 0.2f, 0x00FFFF00, 0.0f, 0.0f, -1.0f, -0.2f, 0x00FFFF00, 0.0f, 0.0f, 1.0f, -0.2f, 0xFFFFFF00, 0.0f, 0.0f));
        mesh->add_triangle(engine::gfx_triangle(-1.0f, 0.2f, 0x00FFFF00, 0.0f, 0.0f, 1.0f, -0.2f, 0xFFFFFF00, 0.0f, 0.0f, 1.0f, 0.2f, 0xFFFFFF00, 0.0f, 0.0f));
        mesh->create();
        pObj->pMesh = mesh;
        AE_ASSERT_MESG(pObj->pMesh, "fail to create object!!");
    }

    // ====================
    // create the asteroid
    // ====================

    pObj       = sGameObjList + sGameObjNum++;
    pObj->type = TYPE_ASTEROID;

    {
        engine::mesh* mesh = new engine::mesh();
        mesh->add_triangle(engine::gfx_triangle(-0.5f, -0.5f, 0xFF808080, 0.0f, 0.0f, 0.5f, 0.5f, 0xFF808080, 0.0f, 0.0f, -0.5f, 0.5f, 0xFF808080, 0.0f, 0.0f));
        mesh->add_triangle(engine::gfx_triangle(-0.5f, -0.5f, 0xFF808080, 0.0f, 0.0f, 0.5f, -0.5f, 0xFF808080, 0.0f, 0.0f, 0.5f, 0.5f, 0xFF808080, 0.0f, 0.0f));
        mesh->create();
        pObj->pMesh = mesh;
        AE_ASSERT_MESG(pObj->pMesh, "fail to create object!!");
    }
}

// ---------------------------------------------------------------------------

GameObjInst* Game::gameObjInstCreate(uint32_t type, float scale, vec2* pPos, vec2* pVel, float dir, bool forceCreate, int m_id)
{
    vec2 zero = {0.0f, 0.0f};

    // AE_ASSERT(type < sGameObjNum);

    // loop through the object instance list to find a non-used object instance
    for (uint32_t i = 0; i < GAME_OBJ_INST_NUM_MAX; i++) {
        GameObjInst* pInst = sGameObjInstList + i;

        // check if current instance is not used
        if (pInst->flag == 0) {
            // it is not used => use it to create the new instance
            pInst->pObject   = sGameObjList + type;
            pInst->flag      = FLAG_ACTIVE;
            pInst->life      = 1.0f;
            pInst->scale     = scale;
            pInst->posCurr   = pPos ? *pPos : zero;
            pInst->velCurr   = pVel ? *pVel : zero;
            pInst->dirCurr   = dir;
            pInst->pUserData = 0;
            pInst->m_id = m_id;

            // keep track the number of asteroid
            if (pInst->pObject->type == TYPE_ASTEROID)
                sAstCtr++;

            // return the newly created instance
            return pInst;
        }
    }

    if (forceCreate) {
        float        scaleMin = FLT_MAX;
        GameObjInst* pDst     = 0;

        if (pDst) {
            pDst->pObject   = sGameObjList + type;
            pDst->flag      = FLAG_ACTIVE;
            pDst->life      = 1.0f;
            pDst->scale     = scale;
            pDst->posCurr   = pPos ? *pPos : zero;
            pDst->velCurr   = pVel ? *pVel : zero;
            pDst->dirCurr   = dir;
            pDst->pUserData = 0;

            // keep track the number of asteroid
            if (pDst->pObject->type == TYPE_ASTEROID)
                sAstCtr++;

            // return the newly created instance
            return pDst;
        }
    }

    // cannot find empty slot => return 0
    return 0;
}

// ---------------------------------------------------------------------------

void Game::gameObjInstDestroy(GameObjInst* pInst)
{
    // if instance is destroyed before, just return
    if (pInst->flag == 0)
        return;

    // zero out the flag
    pInst->flag = 0;

    // keep track the number of asteroid
    if (pInst->pObject->type == TYPE_ASTEROID)
    {
        mAsteroids.erase(pInst->m_id);
        sAstCtr--;
    }
}

// ---------------------------------------------------------------------------

GameObjInst* Game::astCreate(GameObjInst* pSrc, bool is_child)
{
    GameObjInst* pInst;
    vec2         pos, vel;
    float        t, angle, size;

    if (pSrc) {
        float posOffset = pSrc->scale * 0.25f;
        float velOffset = (AST_SIZE_MAX - pSrc->scale + 1.0f) * 0.25f;
        float scaleNew  = pSrc->scale * 0.5f;

        pInst          = astCreate(0, true);
        if (!pInst) return nullptr;
        pInst->scale   = scaleNew;
        pInst->posCurr = {pSrc->posCurr.x - posOffset, pSrc->posCurr.y - posOffset};
        pInst->velCurr = { pSrc->velCurr.x - velOffset, pSrc->velCurr.y - velOffset };

        pInst          = astCreate(0, true);
        if (!pInst) return nullptr;
        pInst->scale   = scaleNew;
        pInst->posCurr = {pSrc->posCurr.x + posOffset, pSrc->posCurr.y - posOffset};
        pInst->velCurr = {pSrc->velCurr.x + velOffset, pSrc->velCurr.y - velOffset};

        pInst          = astCreate(0, true);
        if (!pInst) return nullptr;
        pInst->scale   = scaleNew;
        pInst->posCurr = {pSrc->posCurr.x - posOffset, pSrc->posCurr.y + posOffset};
        pInst->velCurr = {pSrc->velCurr.x - velOffset, pSrc->velCurr.y + velOffset};

        pSrc->scale   = scaleNew;
        pSrc->posCurr = {pSrc->posCurr.x + posOffset, pSrc->posCurr.y + posOffset};
        pSrc->velCurr = {pSrc->velCurr.x + velOffset, pSrc->velCurr.y + velOffset};

        return pSrc;
    }

    // pick a random angle and velocity magnitude
    angle = frand() * 2.0f * PI;
    size  = frand() * (AST_SIZE_MAX - AST_SIZE_MIN) + AST_SIZE_MIN;

    // pick a random position along the top or left edge
    if ((t = frand()) < 0.5f)
        pos = {gAEWinMinX + (t * 2.0f) * (gAEWinMaxX - gAEWinMinX), gAEWinMinY - size * 0.5f};
    else
        pos = {gAEWinMinX - size * 0.5f, gAEWinMinY + ((t - 0.5f) * 2.0f) * (gAEWinMaxY - gAEWinMinY)};

    // calculate the velocity vector
    vel = {glm::cos(angle), glm::sin(angle)};
    vel = vel * frand() * (AST_VEL_MAX - AST_VEL_MIN) + AST_VEL_MIN;

    // create the object instance
    pInst = gameObjInstCreate(TYPE_ASTEROID, size, &pos, &vel, 0.0f, true);
    if (!pInst) return nullptr;

    // set the life based on the size
    pInst->life = size / AST_SIZE_MAX * AST_LIFE_MAX;

    if (NetMgr.Im_server)
    {
        pInst->m_id = asteroids_id++;
        mAsteroids[pInst->m_id] = pInst;

    }

    return pInst;
}

// ---------------------------------------------------------------------------

void Game::resolveCollision(GameObjInst* pSrc, GameObjInst* pDst, vec2* pNrm)
{
    float ma = pSrc->scale * pSrc->scale, mb = pDst->scale * pDst->scale,
          e = COLL_COEF_OF_RESTITUTION;

    if (pNrm->y == 0) // EPSILON)
    {
        // calculate the relative velocity of the 1st object againts the 2nd object
        // along the x-axis
        float velRel = pSrc->velCurr.x - pDst->velCurr.x;

        // if the object is separating, do nothing
        if ((velRel * pNrm->x) >= 0.0f)
            return;

        pSrc->velCurr.x =
            (ma * pSrc->velCurr.x + mb * (pDst->velCurr.x - e * velRel)) /
            (ma + mb);
        pDst->velCurr.x = pSrc->velCurr.x + e * velRel;
    } else {
        // calculate the relative velocity of the 1st object againts the 2nd object
        // along the y-axis
        float velRel = pSrc->velCurr.y - pDst->velCurr.y;

        // if the object is separating, do nothing
        if ((velRel * pNrm->y) >= 0.0f)
            return;

        pSrc->velCurr.y =
            (ma * pSrc->velCurr.y + mb * (pDst->velCurr.y - e * velRel)) /
            (ma + mb);
        pDst->velCurr.y = pSrc->velCurr.y + e * velRel;
    }
}