#ifndef ANEWEBSOCKET_LIBRARY_H
#define ANEWEBSOCKET_LIBRARY_H

#include <FlashRuntimeExtensions.h>

extern "C" {
    __declspec(dllexport) void InitExtension(void** extDataToSet, FREContextInitializer* ctxInitializerToSet, FREContextFinalizer* ctxFinalizerToSet);
    __declspec(dllexport) void DestroyExtension(void* extData);
}

#endif //ANEWEBSOCKET_LIBRARY_H