#ifndef WEBSOCKETSUPPORT_HPP
#define WEBSOCKETSUPPORT_HPP

#include <windows.h>
#include "FlashRuntimeExtensions.h"
#include "WebSocketClient.hpp"

class WebSocketSupport {
    bool _isDebugMode;
    uint32_t _numFunctions;
    FRENamedFunction* _functions;
    WebSocketClient* _websocketClient;

public:
    // Constructor and Destructor
    WebSocketSupport();
    ~WebSocketSupport();

    // Getter methods
    uint32_t numFunctions() const;
    FRENamedFunction* getFunctions() const;
    bool isDebugging() const;

    // Action methods
    void setDebugMode(uint32_t val);
    void connect(const char* uri, uint32_t uriLength, FREContext ctx);
    void close(uint32_t closeCode);
    void sendMessage(FREObject argv[]);
    FREObject getNextMessage();

    // Static exported functions for Adobe AIR
    static FREObject connectWebSocket(FREContext ctx, void *funcData, uint32_t argc, FREObject argv[]);
    static FREObject closeWebSocket(FREContext ctx, void *funcData, uint32_t argc, FREObject argv[]);
    static FREObject sendMessageWebSocket(FREContext ctx, void *funcData, uint32_t argc, FREObject argv[]);
    static FREObject getByteArrayMessage(FREContext ctx, void *funcData, uint32_t argc, FREObject argv[]);
    static FREObject setDebugMode(FREContext ctx, void *funcData, uint32_t argc, FREObject argv[]);
};

// Function declarations for context initializer and finalizer
void WebSocketSupportContextInitializer(void* extData, const uint8_t* ctxType, FREContext ctx, uint32_t* numFunctionsToSet, const FRENamedFunction** functionsToSet);
void WebSocketSupportContextFinalizer(FREContext ctx);

#endif // WEBSOCKETSUPPORT_HPP
