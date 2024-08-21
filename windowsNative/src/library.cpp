#include "log.h"
#ifdef WIN32
#include <winsock2.h>
#include <windows.h>
#include <cstdint>
typedef unsigned __int32 uint32_t;
typedef unsigned __int8 uint8_t;
typedef __int32 int32_t;
#endif
#include "library.h"
#include <WebSocketClient.h>
#include <cstdio>

static WebSocketClient *websocketClient = nullptr;

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

FREObject connectWebSocket(FREContext ctx, void *funcData, uint32_t argc, FREObject argv[]) {
    writeLog("connectWebSocket called");
    if (argc < 1) return nullptr;

    uint32_t uriLength;
    const uint8_t *uri;
    FREGetObjectAsUTF8(argv[0], &uriLength, &uri);

    if (websocketClient != nullptr) {
        websocketClient->close(boost::beast::websocket::close_code::abnormal, "Reconnecting");
        websocketClient = nullptr;
    }

    websocketClient = new WebSocketClient();
    websocketClient->setFREContext(ctx);

    websocketClient->connect(std::string(reinterpret_cast<const char *>(uri), uriLength));

    return nullptr;
}

FREObject closeWebSocket(FREContext ctx, void *funcData, uint32_t argc, FREObject argv[]) {
    writeLog("closeWebSocket called");
    if (websocketClient != nullptr) {
        uint32_t closeCode = 1000; // Default close code
        if (argc > 0) {
            FREGetObjectAsUint32(argv[0], &closeCode);
        }

        // Mapeia o closeCode para boost::beast::websocket::close_code
        auto beastCloseCode = static_cast<boost::beast::websocket::close_code>(closeCode);

        // Fecha o WebSocket usando o código de fechamento mapeado
        websocketClient->close(beastCloseCode, "Connection closed");
    }
    return nullptr;
}

FREObject sendMessageWebSocket(FREContext ctx, void *funcData, uint32_t argc, FREObject argv[]) {
    writeLog("sendMessageWebSocket called");
    if (argc < 2 || websocketClient == nullptr) return nullptr;

    uint32_t messageType;
    FREGetObjectAsUint32(argv[0], &messageType);

    FREObjectType objectType;
    FREGetObjectType(argv[1], &objectType);

    if (objectType == FRE_TYPE_STRING) {
        uint32_t payloadLength;
        const uint8_t *payload;
        FREGetObjectAsUTF8(argv[1], &payloadLength, &payload);

        websocketClient->sendMessage(std::string(reinterpret_cast<const char *>(payload), payloadLength));
    } else if (objectType == FRE_TYPE_BYTEARRAY) {
        FREByteArray byteArray;
        FREAcquireByteArray(argv[1], &byteArray);

        const std::vector payload(byteArray.bytes, byteArray.bytes + byteArray.length);
        websocketClient->sendMessage(payload);

        FREReleaseByteArray(argv[1]);
    }

    return nullptr;
}

FREObject getByteArrayMessage(FREContext ctx, void *funcData, uint32_t argc, FREObject argv[]) {
    writeLog("getByteArrayMessage called");
    if (websocketClient == nullptr) return nullptr;

    const auto message = websocketClient->getNextMessage();

    if (!message.has_value())
        return nullptr;

    auto vectorData = message.value();

    FREObject byteArrayObject = nullptr;
    if (!vectorData.empty()) {
        FREByteArray byteArray;
        byteArray.length = vectorData.size();
        byteArray.bytes = vectorData.data();

        FRENewByteArray(&byteArray, &byteArrayObject);
    }

    return byteArrayObject;
}

FREObject setDebugMode(FREContext ctx, void *funcData, uint32_t argc, FREObject argv[]) {
    writeLog("setDebugMode called");
    if (argc < 1 || websocketClient == nullptr) return nullptr;

    uint32_t debugMode;
    FREGetObjectAsBool(argv[0], &debugMode);

    websocketClient->setDebugMode(debugMode);

    return nullptr;
}

void ContextInitializer(void *extData, const uint8_t *ctxType, FREContext ctx, uint32_t *numFunctionsToSet, const FRENamedFunction **functionsToSet) {
    writeLog("ContextInitializer called");
    static FRENamedFunction arrFunctions[] = {
        {reinterpret_cast<const uint8_t *>("connect"), nullptr, &connectWebSocket},
        {reinterpret_cast<const uint8_t *>("close"), nullptr, &closeWebSocket},
        {reinterpret_cast<const uint8_t *>("sendMessage"), nullptr, &sendMessageWebSocket},
        {reinterpret_cast<const uint8_t *>("getByteArrayMessage"), nullptr, &getByteArrayMessage},
        {reinterpret_cast<const uint8_t *>("setDebugMode"), nullptr, &setDebugMode}
    };

    *functionsToSet = arrFunctions;
    *numFunctionsToSet = std::size(arrFunctions);
    writeLog("ContextInitializer completed");
}

extern "C" {
__declspec(dllexport) void InitExtension(void **extDataToSet, FREContextInitializer *ctxInitializerToSet, FREContextFinalizer *ctxFinalizerToSet) {
    writeLog("InitExtension called");
    *extDataToSet = nullptr;
    *ctxInitializerToSet = ContextInitializer;
    *ctxFinalizerToSet = DestroyExtension;
    writeLog("InitExtension completed");
}

__declspec(dllexport) void DestroyExtension(void *extData) {
    writeLog("DestroyExtension called");
}
}
