//
//  WebSocketNativeLibrary.h
//  WebSocketANE
//
//  Created by João Vitor Borges on 13/09/24.
//

#ifndef WebSocketNativeLibrary_h
#define WebSocketNativeLibrary_h
int __cdecl csharpWebSocketLibrary_initializerCallbacks(const void* callBackConnect, const void *callBackData, const void *callBackDisconnect, const void *callBackLog);
char* __cdecl csharpWebSocketLibrary_createWebSocketClient(const void* ctx);
int __cdecl csharpWebSocketLibrary_connect(const void* guidPointer, const char* url);
void __cdecl csharpWebSocketLibrary_sendMessage(const void* guidPointer, const void* data, int length);
void __cdecl csharpWebSocketLibrary_disconnect(const void* guidPointer, int closeCode);

#endif /* WebSocketNativeLibrary_h */
