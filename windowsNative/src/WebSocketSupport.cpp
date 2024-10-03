#include "WebSocketSupport.hpp"
#include <unordered_map>
#include <string>
#include "log.h"
#include "WebSocketNativeLibrary.h"

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) {
    switch (ul_reason_for_call) {
        case DLL_PROCESS_ATTACH:
            initLog();
            writeLog("DLL loaded (DLL_PROCESS_ATTACH)");
            break;
        case DLL_THREAD_ATTACH:
            writeLog("DLL loaded (DLL_THREAD_ATTACH)");
            break;
        case DLL_THREAD_DETACH:
            writeLog("DLL unloaded (DLL_THREAD_DETACH)");
            break;
        case DLL_PROCESS_DETACH:
            writeLog("DLL unloaded (DLL_PROCESS_DETACH)");
            closeLog();
            break;
    }
    return TRUE;
}

static bool alreadyInitialized = false;
static FRENamedFunction *exportedFunctions = new FRENamedFunction[7];
static std::unordered_map<FREContext, WebSocketClient *> wsClientMap;
static std::mutex wsClientMapMutex;

// Helper function to safely retrieve WebSocketClient from the map
static WebSocketClient *getWebSocketClient(FREContext ctx) {
    std::lock_guard guard(wsClientMapMutex);
    auto it = wsClientMap.find(ctx);
    if (it != wsClientMap.end()) {
        return it->second;
    }
    return nullptr;
}

// Helper function to safely insert WebSocketClient into the map
static void setWebSocketClient(FREContext ctx, WebSocketClient *wsClient) {
    std::lock_guard guard(wsClientMapMutex);
    wsClientMap[ctx] = wsClient;
}

// Helper function to safely remove WebSocketClient from the map
static void removeWebSocketClient(FREContext ctx) {
    std::lock_guard guard(wsClientMapMutex);
    wsClientMap.erase(ctx);
}

static void __cdecl connectCallback(void *ctx) {
    writeLog("connectCallback called");
    FREDispatchStatusEventAsync(ctx, reinterpret_cast<const uint8_t *>("connected"), reinterpret_cast<const uint8_t *>(""));
}

static void __cdecl dataCallback(void *ctx, const uint8_t *data, int length) {
    writeLog("dataCallback called");

    WebSocketClient *wsClient = getWebSocketClient(ctx);

    if (wsClient == nullptr) {
        writeLog("wsClient not found");
        return;
    }

    auto dataCopyVector = std::vector<uint8_t>();
    dataCopyVector.resize(length);
    std::copy_n(data, length, dataCopyVector.begin());
    wsClient->enqueueMessage(dataCopyVector);

    FREDispatchStatusEventAsync(ctx, reinterpret_cast<const uint8_t *>("nextMessage"), data);
}

static void __cdecl ioErrorCallback(void *ctx, int closeCode, const char *reason) {
    writeLog("disconnectCallback called");

    auto closeCodeReason = std::to_string(closeCode) + ";" + std::string(reason);

    writeLog(closeCodeReason.c_str());

    FREDispatchStatusEventAsync(ctx, reinterpret_cast<const uint8_t *>("disconnected"), reinterpret_cast<const uint8_t *>(closeCodeReason.c_str()));
}

static void writeLogCallback(const char *message) {
    writeLog(message);
}

// Exported functions:
static FREObject connectWebSocket(FREContext ctx, void *funcData, uint32_t argc, FREObject argv[]) {
    writeLog("connectWebSocket called");
    if (argc < 1) return nullptr;

    uint32_t uriLength;
    const uint8_t *uri;
    FREGetObjectAsUTF8(argv[0], &uriLength, &uri);

    auto uriChar = reinterpret_cast<const char *>(uri);

    writeLog("Calling connect to uri: ");
    writeLog(uriChar);

    WebSocketClient *wsClient = getWebSocketClient(ctx);

    if (wsClient == nullptr) {
        writeLog("wsClient not found");
        return nullptr;
    }

    if (wsClient != nullptr) {
        wsClient->connect(reinterpret_cast<const char *>(uri));
    }

    return nullptr;
}

static FREObject closeWebSocket(FREContext ctx, void *funcData, uint32_t argc, FREObject argv[]) {
    writeLog("closeWebSocket called");

    WebSocketClient *wsClient = nullptr;
    FREGetContextNativeData(ctx, reinterpret_cast<void **>(&wsClient));

    if (wsClient == nullptr) {
        writeLog("wsClient not found");
        return nullptr;
    }

    uint32_t closeCode = 1000; // Default close code
    if (argc > 0) {
        FREGetObjectAsUint32(argv[0], &closeCode);
    }

    wsClient->close(closeCode);
    return nullptr;
}

static FREObject sendMessageWebSocket(FREContext ctx, void *funcData, uint32_t argc, FREObject argv[]) {
    writeLog("sendMessageWebSocket called");
    if (argc < 2) return nullptr;

    WebSocketClient *wsClient = nullptr;
    FREGetContextNativeData(ctx, reinterpret_cast<void **>(&wsClient));

    uint32_t messageType;
    FREGetObjectAsUint32(argv[0], &messageType);

    FREObjectType objectType;
    FREGetObjectType(argv[1], &objectType);

    if (objectType == FRE_TYPE_STRING) {
        //TODO: Implement string message
    } else if (objectType == FRE_TYPE_BYTEARRAY) {
        FREByteArray byteArray;
        FREAcquireByteArray(argv[1], &byteArray);

        wsClient->sendMessage(byteArray.bytes, static_cast<int>(byteArray.length));

        FREReleaseByteArray(argv[1]);
    }


    return nullptr;
}

static FREObject getByteArrayMessage(FREContext ctx, void *funcData, uint32_t argc, FREObject argv[]) {
    writeLog("getByteArrayMessage called");

    WebSocketClient *wsClient = nullptr;
    FREGetContextNativeData(ctx, reinterpret_cast<void **>(&wsClient));

    auto nextMessageResult = wsClient->getNextMessage();

    if (!nextMessageResult.has_value()) {
        writeLog("no messages found");
        return nullptr;
    }

    auto vectorData = nextMessageResult.value();

    if (vectorData.empty()) {
        writeLog("message it's empty");
        return nullptr;
    }

    FREObject byteArrayObject = nullptr;
    FREByteArray byteArray;
    byteArray.length = vectorData.size();
    byteArray.bytes = vectorData.data();

    FRENewByteArray(&byteArray, &byteArrayObject);

    return byteArrayObject;
}

static FREObject setDebugMode(FREContext ctx, void *funcData, uint32_t argc, FREObject argv[]) {
    writeLog("setDebugMode called");
    if (argc < 1) return nullptr;

    /*WebSocketSupport* wsSupport = nullptr;
    FREGetContextNativeData(ctx, reinterpret_cast<void **>(&wsSupport));

    if (wsSupport != nullptr) {
        uint32_t debugMode;
        FREGetObjectAsBool(argv[0], &debugMode);
        wsSupport->setDebugMode(debugMode);
    }*/

    return nullptr;
}

static FREObject addStaticHost(FREContext ctx, void *funcData, uint32_t argc, FREObject argv[]) {
    writeLog("addStaticHost called");
    if (argc < 2) return nullptr;

    uint32_t hostLength;
    const uint8_t *host;
    FREGetObjectAsUTF8(argv[0], &hostLength, &host);

    uint32_t ipLength;
    const uint8_t *ip;
    FREGetObjectAsUTF8(argv[1], &ipLength, &ip);

    writeLog("Calling addStaticHost with host: ");
    writeLog(reinterpret_cast<const char *>(host));
    writeLog(" and ip: ");
    writeLog(reinterpret_cast<const char *>(ip));

    csharpWebSocketLibrary_addStaticHost(reinterpret_cast<const char *>(host), reinterpret_cast<const char *>(ip));
    return nullptr;
}

static FREObject removeStaticHost(FREContext ctx, void *funcData, uint32_t argc, FREObject argv[]) {
    writeLog("removeStaticHost called");
    if (argc < 1) return nullptr;

    uint32_t hostLength;
    const uint8_t *host;
    FREGetObjectAsUTF8(argv[0], &hostLength, &host);

    writeLog("Calling removeStaticHost with host: ");
    writeLog(reinterpret_cast<const char *>(host));

    csharpWebSocketLibrary_removeStaticHost(reinterpret_cast<const char *>(host));
    return nullptr;
}

static void WebSocketSupportContextInitializer(
    void *extData,
    const uint8_t *ctxType,
    FREContext ctx,
    uint32_t *numFunctionsToSet,
    const FRENamedFunction **functionsToSet
) {
    if (!alreadyInitialized) {
        alreadyInitialized = true;
        exportedFunctions[0].name = (const uint8_t *) "connect";
        exportedFunctions[0].function = connectWebSocket;
        exportedFunctions[1].name = (const uint8_t *) "close";
        exportedFunctions[1].function = closeWebSocket;
        exportedFunctions[2].name = (const uint8_t *) "sendMessage";
        exportedFunctions[2].function = sendMessageWebSocket;
        exportedFunctions[3].name = (const uint8_t *) "getByteArrayMessage";
        exportedFunctions[3].function = getByteArrayMessage;
        exportedFunctions[4].name = (const uint8_t *) "setDebugMode";
        exportedFunctions[4].function = setDebugMode;
        exportedFunctions[5].name = (const uint8_t *) "addStaticHost";
        exportedFunctions[5].function = addStaticHost;
        exportedFunctions[6].name = (const uint8_t *) "removeStaticHost";
        exportedFunctions[6].function = removeStaticHost;
        csharpWebSocketLibrary_initializerCallbacks((void *) &connectCallback, (void *) &dataCallback, (void *) &ioErrorCallback, (void *) &writeLogCallback);
    }
    WebSocketClient *wsClient = new WebSocketClient(ctx);
    FRESetContextNativeData(ctx, wsClient);
    setWebSocketClient(ctx, wsClient);
    if (numFunctionsToSet) *numFunctionsToSet = 7;
    if (functionsToSet) *functionsToSet = exportedFunctions;
}

static void WebSocketSupportContextFinalizer(FREContext ctx) {
    WebSocketClient *wsClient = nullptr;
    if (FRE_OK == FREGetContextNativeData(ctx, reinterpret_cast<void **>(&wsClient))) {
        delete wsClient;
    }
    removeWebSocketClient(ctx);
}

extern "C" {
__declspec(dllexport) void InitExtension(void **extDataToSet, FREContextInitializer *ctxInitializerToSet, FREContextFinalizer *ctxFinalizerToSet) {
    writeLog("InitExtension called");
    *extDataToSet = nullptr;
    *ctxInitializerToSet = WebSocketSupportContextInitializer;
    *ctxFinalizerToSet = WebSocketSupportContextFinalizer;
    writeLog("InitExtension completed");
}

__declspec(dllexport) void DestroyExtension(void *extData) {
    writeLog("DestroyExtension called");
}
} // end of extern "C"
