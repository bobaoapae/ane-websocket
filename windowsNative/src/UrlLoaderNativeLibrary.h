//
// Created by User on 31/07/2024.
//

#ifndef URLLOADERNATIVELIBRARY_H
#define URLLOADERNATIVELIBRARY_H

#ifdef _WIN32
#include "windows.h"
#else
#include "dlfcn.h"
#endif

void *loadNativeLibrary();
void *getFunctionPointer(const char *functionName);
int callInitializerLoader();


#endif //URLLOADERNATIVELIBRARY_H
