#pragma once

typedef unsigned long long SOCKET;

bool InitClient();

bool ConnectToServer();

bool WaitForStart();

void ShutDownClient();

SOCKET GetSocket();