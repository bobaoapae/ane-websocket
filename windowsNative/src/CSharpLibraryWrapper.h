//
// Created by User on 06/09/2024.
//

#ifndef CSHARPLIBRARYWRAPPER_H
#define CSHARPLIBRARYWRAPPER_H

#ifdef _WIN32
#include "windows.h"
#else
#include "dlfcn.h"
#endif

int initializerCallbacks(const void* callBackConnect, const void *callBackData, const void *callBackDisconnect, const void *callBackLog);
char* createWebSocketClient();
int connect(const void* guidPointer, const char* url);
void sendMessage(const void* guidPointer, const void* data, int length);
void disconnect(const void* guidPointer, int closeCode);

#endif //CSHARPLIBRARYWRAPPER_H
