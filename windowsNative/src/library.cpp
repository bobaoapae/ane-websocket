#include <iterator>
#include <map>
#include <mutex>
#include <optional>
#include <queue>
#include <string>

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
#include <CSharpLibraryWrapper.h>
#include <cstdio>

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

class HolderCSharpWebSocket {
private:
    char *_guid;
    FREContext _ctx;
    std::queue<std::vector<uint8_t> > _messageQueue;
    std::mutex _messageQueueMutex;

public:
    explicit HolderCSharpWebSocket(char *guid, FREContext ctx) {
        _guid = guid;
        _ctx = ctx;
    }

    ~HolderCSharpWebSocket() {
        delete[] _guid;
        std::queue<std::vector<uint8_t> > empty;
        std::swap(_messageQueue, empty);
    }

    [[nodiscard]] char *getIdentifier() const {
        return _guid;
    }

    [[nodiscard]] FREContext getContext() const {
        return _ctx;
    }

    void addMessageToQueue(const std::vector<uint8_t> &message) {
        std::lock_guard lock(_messageQueueMutex);
        _messageQueue.push(message);
    }

    std::optional<std::vector<uint8_t> > getNextMessage() {
        std::lock_guard lock(_messageQueueMutex);
        if (_messageQueue.empty()) {
            return std::nullopt;
        }

        auto message = _messageQueue.front();
        _messageQueue.pop();
        return message;
    }
};

static bool alreadyInitialized = false;
std::map<std::string, HolderCSharpWebSocket *> webSocketClients;

static void connectCallback(char *guid) {
    writeLog("connectCallback called");

    auto guidString = std::string(guid);
    writeLog(guidString.c_str());

    if (webSocketClients.find(guidString) == webSocketClients.end()) {
        writeLog("WebSocket client not found");
        return;
    }

    HolderCSharpWebSocket *holderCSharpWEbSocket = webSocketClients[guidString];
    FREDispatchStatusEventAsync(holderCSharpWEbSocket->getContext(), reinterpret_cast<const uint8_t *>("connected"), reinterpret_cast<const uint8_t *>(""));
}

static void dataCallback(char *guid, const uint8_t *data, int length) {
    writeLog("dataCallback called");
    auto guidString = std::string(guid);
    writeLog(guidString.c_str());

    if (webSocketClients.find(guidString) == webSocketClients.end()) {
        writeLog("WebSocket client not found");
        return;
    }

    HolderCSharpWebSocket *holderCSharpWEbSocket = webSocketClients[guidString];

    auto dataCopyVector = std::vector<uint8_t>();
    dataCopyVector.resize(length);
    std::copy_n(data, length, dataCopyVector.begin());
    holderCSharpWEbSocket->addMessageToQueue(dataCopyVector);

    FREDispatchStatusEventAsync(holderCSharpWEbSocket->getContext(), reinterpret_cast<const uint8_t *>("nextMessage"), data);
}

static void ioErrorCallback(char *guid, int closeCode, const char *reason) {
    writeLog("disconnectCallback called");

    auto closeCodeReason = std::to_string(closeCode) + ";" + std::string(reason);

    writeLog(closeCodeReason.c_str());

    auto guidString = std::string(guid);
    writeLog(guidString.c_str());

    if (webSocketClients.find(guidString) == webSocketClients.end()) {
        writeLog("WebSocket client not found");
        return;
    }

    HolderCSharpWebSocket *holderCSharpWEbSocket = webSocketClients[guidString];

    FREDispatchStatusEventAsync(holderCSharpWEbSocket->getContext(), reinterpret_cast<const uint8_t *>("disconnected"), reinterpret_cast<const uint8_t *>(closeCodeReason.c_str()));
}

static void writeLogCallback(const char *message) {
    writeLog(message);
}

FREObject connectWebSocket(FREContext ctx, void *funcData, uint32_t argc, FREObject argv[]) {
    writeLog("connectWebSocket called");
    if (argc < 1) return nullptr;

    uint32_t uriLength;
    const uint8_t *uri;
    FREGetObjectAsUTF8(argv[0], &uriLength, &uri);

    auto uriChar = reinterpret_cast<const char *>(uri);

    writeLog("Calling connect to uri: ");
    writeLog(uriChar);

    HolderCSharpWebSocket *holderCSharpWEbSocketGuid = nullptr;

    if (FRE_OK == FREGetContextNativeData(ctx, reinterpret_cast<void **>(&holderCSharpWEbSocketGuid)) && holderCSharpWEbSocketGuid != nullptr) {
        disconnect(holderCSharpWEbSocketGuid->getIdentifier(), 1004);
        delete holderCSharpWEbSocketGuid;
        auto guidString = std::string(holderCSharpWEbSocketGuid->getIdentifier());
        webSocketClients.erase(guidString);
    }

    auto newGuid = createWebSocketClient();
    holderCSharpWEbSocketGuid = new HolderCSharpWebSocket(newGuid, ctx);

    auto guidString = std::string(newGuid);
    webSocketClients[guidString] = holderCSharpWEbSocketGuid;

    FRESetContextNativeData(ctx, holderCSharpWEbSocketGuid);

    auto result = connect(holderCSharpWEbSocketGuid->getIdentifier(), uriChar);

    if (result != 1) {
        writeLog("Error connecting to WebSocket");
    }

    FREObject resultObject = nullptr;
    FRENewObjectFromBool(result == 1, &resultObject);
    return resultObject;
}

FREObject closeWebSocket(FREContext ctx, void *funcData, uint32_t argc, FREObject argv[]) {
    writeLog("closeWebSocket called");

    HolderCSharpWebSocket *holderCSharpWEbSocketGuid = nullptr;
    FREGetContextNativeData(ctx, reinterpret_cast<void **>(&holderCSharpWEbSocketGuid));

    if (holderCSharpWEbSocketGuid != nullptr) {
        uint32_t closeCode = 1000; // Default close code
        if (argc > 0) {
            FREGetObjectAsUint32(argv[0], &closeCode);
        }

        disconnect(holderCSharpWEbSocketGuid->getIdentifier(), static_cast<int>(closeCode));
    }
    return nullptr;
}

FREObject sendMessageWebSocket(FREContext ctx, void *funcData, uint32_t argc, FREObject argv[]) {
    writeLog("sendMessageWebSocket called");
    if (argc < 2) return nullptr;

    HolderCSharpWebSocket *holderCSharpWEbSocketGuid = nullptr;
    FREGetContextNativeData(ctx, reinterpret_cast<void **>(&holderCSharpWEbSocketGuid));

    if (holderCSharpWEbSocketGuid == nullptr) {
        writeLog("WebSocket client context not found");
        return nullptr;
    }

    uint32_t messageType;
    FREGetObjectAsUint32(argv[0], &messageType);

    FREObjectType objectType;
    FREGetObjectType(argv[1], &objectType);

    if (objectType == FRE_TYPE_STRING) {
        //TODO: Implement string message
    } else if (objectType == FRE_TYPE_BYTEARRAY) {
        FREByteArray byteArray;
        FREAcquireByteArray(argv[1], &byteArray);

        sendMessage(holderCSharpWEbSocketGuid->getIdentifier(), byteArray.bytes, static_cast<int>(byteArray.length));

        FREReleaseByteArray(argv[1]);
    }

    return nullptr;
}

FREObject getByteArrayMessage(FREContext ctx, void *funcData, uint32_t argc, FREObject argv[]) {
    writeLog("getByteArrayMessage called");

    HolderCSharpWebSocket *holderCSharpWEbSocket = nullptr;
    FREGetContextNativeData(ctx, reinterpret_cast<void **>(&holderCSharpWEbSocket));

    if (holderCSharpWEbSocket == nullptr) {
        writeLog("WebSocket client context not found");
        return nullptr;
    }

    auto nextMessageResult = holderCSharpWEbSocket->getNextMessage();

    if (!nextMessageResult.has_value()) {
        return nullptr;
    }

    auto vectorData = nextMessageResult.value();

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
    if (argc < 1) return nullptr;

    void *websocketClient = nullptr;
    FREGetContextNativeData(ctx, &websocketClient);

    if (websocketClient == nullptr) return nullptr;

    uint32_t debugMode;
    FREGetObjectAsBool(argv[0], &debugMode);

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

    if (!alreadyInitialized) {
        initializerCallbacks(&connectCallback, &dataCallback, &ioErrorCallback, &writeLogCallback);
        alreadyInitialized = true;
    }

    writeLog("ContextInitializer completed");
}

void ContextFinalizer(FREContext ctx) {
    writeLog("ContextFinalizer called");
}

extern "C" {
__declspec(dllexport) void InitExtension(void **extDataToSet, FREContextInitializer *ctxInitializerToSet, FREContextFinalizer *ctxFinalizerToSet) {
    writeLog("InitExtension called");
    *extDataToSet = nullptr;
    *ctxInitializerToSet = ContextInitializer;
    *ctxFinalizerToSet = ContextFinalizer;
    writeLog("InitExtension completed");
}

__declspec(dllexport) void DestroyExtension(void *extData) {
    writeLog("DestroyExtension called");
}
}
