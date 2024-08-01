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
        websocketClient->close(websocketpp::close::status::abnormal_close, "Reconnecting");
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

        websocketClient->close(websocketpp::close::status::internal_endpoint_error, std::to_string(closeCode));
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

        websocketClient->sendMessage(websocketpp::frame::opcode::text, std::string(reinterpret_cast<const char *>(payload), payloadLength));
    } else if (objectType == FRE_TYPE_BYTEARRAY) {
        FREByteArray byteArray;
        FREAcquireByteArray(argv[1], &byteArray);

        std::vector<uint8_t> payload(byteArray.bytes, byteArray.bytes + byteArray.length);
        websocketClient->sendMessage(websocketpp::frame::opcode::binary, payload);

        FREReleaseByteArray(argv[1]);
    }

    return nullptr;
}

FREObject getByteArrayMessage(FREContext ctx, void *funcData, uint32_t argc, FREObject argv[]) {
    writeLog("getByteArrayMessage called");
    if (argc < 1 || websocketClient == nullptr) return nullptr;

    uint32_t uuidLength;
    const uint8_t *uuid;
    FREGetObjectAsUTF8(argv[0], &uuidLength, &uuid);
    std::string uuidStr(reinterpret_cast<const char *>(uuid), uuidLength);

    std::vector<uint8_t> message = websocketClient->getMessage(uuidStr);

    FREObject byteArrayObject = nullptr;
    if (!message.empty()) {
        FREByteArray byteArray;
        byteArray.length = message.size();
        byteArray.bytes = message.data();

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
        {(const uint8_t *) "connect", NULL, &connectWebSocket},
        {(const uint8_t *) "close", NULL, &closeWebSocket},
        {(const uint8_t *) "sendMessage", NULL, &sendMessageWebSocket},
        {(const uint8_t *) "getByteArrayMessage", NULL, &getByteArrayMessage},
        {(const uint8_t *) "setDebugMode", NULL, &setDebugMode}
    };

    *functionsToSet = arrFunctions;
    *numFunctionsToSet = sizeof(arrFunctions) / sizeof(arrFunctions[0]);
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