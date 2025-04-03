/* Start Header *****************************************************************/

/*! \file README

   \author Adam Goh Zheng Shan
   \author Brandon Poon
   \author Zulfami Ashrafi
   \author Gabriel Peh Jun Wei
   \author Kwek Wei Jie

   \par Student IDs:
        Adam Goh Zheng Shan (2301303)
        Brandon Poon (2301224)
        Zulfami Ashrafi (2301298)
        Gabriel Peh Jun Wei (2301454)
        Kwek Wei Jie (2301325)

   \par Emails:
        goh.a@digipen.edu
        b.poon@digipen.edu
        b.zulfamiashrafi@digipen.edu
        peh.j@digipen.edu
        k.weijie@digipen.edu

   \date April 03, 2025

   \brief
        Networking Asteroids Project.
        Multiplayer and Single Player implementation with basic physics, 
        collision handling, and data synchronization over UDP.

   \copyright
        Copyright (C) 2025 DigiPen Institute of Technology.
        Reproduction or disclosure of this file or its contents without 
        the prior written consent of DigiPen Institute of Technology is prohibited.

*/
/* End Header *******************************************************************/

----------------------------------------------------------------------------------
Project Responsibilities
----------------------------------------------------------------------------------

- **Adam Goh Zheng Shan (2301303):**
  - Implemented game object creation, bullet spawning, and collision detection.
  - Integrated game state updates and object lifecycle management.

- **Brandon Poon (2301224):**
  - Set up and configured UDP socket communication.
  - Handled player data sending and receiving between server and client.

- **Zulfami Ashrafi (2301298):**
  - Structured player data formatting for multiplayer synchronization.

- **Gabriel Peh Jun Wei (2301454):**
  - Managed scoring system, lives counter, and end-game condition handling.

- **Kuek Wei Jie (2301325):**
  - Designed and implemented path smoothing and interpolation logic.
  - Implemented lag compensation and data prediction systems.

----------------------------------------------------------------------------------
How to Run the Game
----------------------------------------------------------------------------------

**Multiplayer Mode (with Server):**
1. Launch the server executable.
2. Open **4 client instances** to start the game.
3. Every time a client connects, the server console will display:
   `Received message from client: Hello, server!`
4. The game will start automatically once all clients are connected.

**Single Player Mode (without Server):**
1. Launch a single client executable.
2. Press the **Spacebar** to trigger single player mode.
3. Enjoy the game and try to beat the score target!

----------------------------------------------------------------------------------
