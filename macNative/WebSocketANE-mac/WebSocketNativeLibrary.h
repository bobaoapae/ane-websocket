//
//  WebSocketNativeLibrary.h
//  WebSocketANE
//
//  Created by João Vitor Borges on 13/09/24.
//

#ifndef WebSocketNativeLibrary_h
#define WebSocketNativeLibrary_h
extern "C" {
    __cdecl int csharpWebSocketLibrary_initializerCallbacks(const void* callBackConnect, const void *callBackData, const void *callBackDisconnect, const void *callBackLog);
    __cdecl char* csharpWebSocketLibrary_createWebSocketClient(const void* ctx);
    __cdecl int csharpWebSocketLibrary_connect(const void* guidPointer, const char* url);
    __cdecl void csharpWebSocketLibrary_sendMessage(const void* guidPointer, const void* data, int length);
    __cdecl void csharpWebSocketLibrary_disconnect(const void* guidPointer, int closeCode);
}

#endif /* WebSocketNativeLibrary_h */
