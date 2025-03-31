/******************************************************************************/
/*!
\file		Main.cpp
\author 	Jaasmeet Singh
\par    	email: jaasmeet.s@digipen.edu
\date   	February 02, 2023
\brief		contains function main

Copyright (C) 2023 DigiPen Institute of Technology.
Reproduction or disclosure of this file or its contents without the
prior written consent of DigiPen Institute of Technology is prohibited.
 */
/******************************************************************************/

#include "main.h"
#include <memory>
#include <thread>

#include "AtomicVariables.h"
#include "Client.h"

// ---------------------------------------------------------------------------
// Globals
float	 g_dt;
double	 g_appTime;

/******************************************************************************/
/*!
	Starting point of the application
*/
/******************************************************************************/
int WINAPI WinMain(HINSTANCE instanceH, HINSTANCE prevInstanceH, LPSTR command_line, int show)		//IDK WHY GOT WARNING INCONSISTENT ANNOTATION BUT IDK WHAT IT MEANS!!!!!!!!!!!
{
	UNREFERENCED_PARAMETER(prevInstanceH);
	UNREFERENCED_PARAMETER(command_line);

	// Enable run-time memory check for debug builds.
	#if defined(DEBUG) | defined(_DEBUG)
		_CrtSetDbgFlag( _CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF );
	#endif

	// Initialize the system
	AESysInit (instanceH, show, 800, 600, 1, 60, false, NULL);

	// Changing the window title
	AESysSetWindowTitle("Asteroids");

	//set background color
	AEGfxSetBackgroundColor(0.0f, 0.0f, 0.0f);

	InitClient();

	std::thread connect(ConnectToServer);
	//ConnectToServer();

	//const bool start = WaitForStart();

	GameStateMgrInit(GS_ASTEROIDS);			

	while(gGameStateCurr != GS_QUIT)
	{
		SOCKET udp_socket = GetSocket();
		std::thread data_transfer(GameStateDataTransfer, udp_socket);
		// reset the system modules
		AESysReset();

		// If not restarting, load the gamestate
		if(gGameStateCurr != GS_RESTART)
		{
			GameStateMgrUpdate();
			GameStateLoad();
		}
		else
			gGameStateNext = gGameStateCurr = gGameStatePrev;

		// Initialize the gamestate
		GameStateInit();

		while(gGameStateCurr == gGameStateNext)
		{
			AESysFrameStart();

			AEInputUpdate();

			if(av_start)
			{
				GameStateUpdate();
			}
			else
			{
				if(AEInputCheckTriggered(AEVK_SPACE))
				{
					av_start = true;
					av_connected = false;
					if (connect.joinable())
					{
						connect.detach();
					}
				}

				if(!av_start && av_connected)
				{
					av_start = WaitForStart();

					if(av_start)
					{
						GameStateInitMP();
					}
				}
			}

			GameStateDraw();
			
			AESysFrameEnd();

			// check if forcing the application to quit
			if ((AESysDoesWindowExist() == false) || AEInputCheckTriggered(AEVK_ESCAPE))
				gGameStateNext = GS_QUIT;

			g_dt = (f32)AEFrameRateControllerGetFrameTime();
			g_appTime += g_dt;
		}
		
		GameStateFree();

		if(gGameStateNext != GS_RESTART)
			GameStateUnload();

		gGameStatePrev = gGameStateCurr;
		gGameStateCurr = gGameStateNext;

		if (data_transfer.joinable())
		{
			data_transfer.detach();
		}
	}

	ShutDownClient();

	// free the system
	AESysExit();

	if (connect.joinable())
	{
		connect.detach();
	}
}