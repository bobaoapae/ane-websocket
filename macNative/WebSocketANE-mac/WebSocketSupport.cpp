#include "WebSocketClient.hpp"
#include <cstdio>
#include "log.hpp"

class WebSocketSupport {
    bool _isDebugMode;
    uint32_t _numFunctions;
    FRENamedFunction* _functions;
    WebSocketClient* _websocketClient;

public:
    WebSocketSupport()
    : _isDebugMode(false)
    , _websocketClient(nullptr)
    {
        _numFunctions = 5;
        _functions = new FRENamedFunction[_numFunctions];
        _functions[0].name = (const uint8_t*)"connect";
        _functions[0].function = WebSocketSupport::connectWebSocket;
        _functions[1].name = (const uint8_t*)"close";
        _functions[1].function = WebSocketSupport::closeWebSocket;
        _functions[2].name = (const uint8_t*)"sendMessage";
        _functions[2].function = WebSocketSupport::sendMessageWebSocket;
        _functions[3].name = (const uint8_t*)"getByteArrayMessage";
        _functions[3].function = WebSocketSupport::getByteArrayMessage;
        _functions[4].name = (const uint8_t*)"setDebugMode";
        _functions[4].function = WebSocketSupport::setDebugMode;
    }

    ~WebSocketSupport() {
        delete[] _functions;
        if (_websocketClient) {
            delete _websocketClient;
        }
        _functions = nullptr;
    }

    uint32_t numFunctions() const {
        return _numFunctions;
    }

    FRENamedFunction* getFunctions() const {
        return _functions;
    }

    bool isDebugging() const {
        return _isDebugMode;
    }

    void setDebugMode(uint32_t val) {
        _isDebugMode = (val != 0);
        if (_websocketClient) {
            _websocketClient->setDebugMode(_isDebugMode);
        }
    }

    void connect(const char* uri, uint32_t uriLength, FREContext ctx) {
        if (_websocketClient != nullptr) {
            _websocketClient->close(boost::beast::websocket::close_code::abnormal, "Reconnecting");
            delete _websocketClient;
        }
        _websocketClient = new WebSocketClient();
        _websocketClient->setFREContext(ctx);
        _websocketClient->connect(std::string(uri, uriLength));
    }

    void close(uint32_t closeCode) {
        if (_websocketClient != nullptr) {
            auto beastCloseCode = static_cast<boost::beast::websocket::close_code>(closeCode);
            _websocketClient->close(beastCloseCode, "Connection closed");
        }
    }

    void sendMessage(FREObject argv[]) {
        if (_websocketClient == nullptr) return;

        FREObjectType objectType;
        FREGetObjectType(argv[1], &objectType);

        if (objectType == FRE_TYPE_STRING) {
            uint32_t payloadLength;
            const uint8_t *payload;
            FREGetObjectAsUTF8(argv[1], &payloadLength, &payload);

            _websocketClient->sendMessage(std::string(reinterpret_cast<const char *>(payload), payloadLength));
        } else if (objectType == FRE_TYPE_BYTEARRAY) {
            FREByteArray byteArray;
            FREAcquireByteArray(argv[1], &byteArray);

            const std::vector payload(byteArray.bytes, byteArray.bytes + byteArray.length);
            _websocketClient->sendMessage(payload);

            FREReleaseByteArray(argv[1]);
        }
    }

    FREObject getNextMessage() {
        if (_websocketClient == nullptr) return nullptr;

        const auto message = _websocketClient->getNextMessage();

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

    // Exported functions:
    static FREObject connectWebSocket(FREContext ctx, void *funcData, uint32_t argc, FREObject argv[]) {
        writeLog("connectWebSocket called");
        if (argc < 1) return nullptr;

        uint32_t uriLength;
        const uint8_t *uri;
        FREGetObjectAsUTF8(argv[0], &uriLength, &uri);

        WebSocketSupport* wsSupport = nullptr;
        FREGetContextNativeData(ctx, reinterpret_cast<void **>(&wsSupport));

        if (wsSupport != nullptr) {
            wsSupport->connect(reinterpret_cast<const char *>(uri), uriLength, ctx);
        }

        return nullptr;
    }

    static FREObject closeWebSocket(FREContext ctx, void *funcData, uint32_t argc, FREObject argv[]) {
        writeLog("closeWebSocket called");

        WebSocketSupport* wsSupport = nullptr;
        FREGetContextNativeData(ctx, reinterpret_cast<void **>(&wsSupport));

        if (wsSupport != nullptr) {
            uint32_t closeCode = 1000; // Default close code
            if (argc > 0) {
                FREGetObjectAsUint32(argv[0], &closeCode);
            }

            wsSupport->close(closeCode);
        }
        return nullptr;
    }

    static FREObject sendMessageWebSocket(FREContext ctx, void *funcData, uint32_t argc, FREObject argv[]) {
        writeLog("sendMessageWebSocket called");
        if (argc < 2) return nullptr;

        WebSocketSupport* wsSupport = nullptr;
        FREGetContextNativeData(ctx, reinterpret_cast<void **>(&wsSupport));

        if (wsSupport != nullptr) {
            wsSupport->sendMessage(argv);
        }

        return nullptr;
    }

    static FREObject getByteArrayMessage(FREContext ctx, void *funcData, uint32_t argc, FREObject argv[]) {
        writeLog("getByteArrayMessage called");

        WebSocketSupport* wsSupport = nullptr;
        FREGetContextNativeData(ctx, reinterpret_cast<void **>(&wsSupport));

        if (wsSupport != nullptr) {
            return wsSupport->getNextMessage();
        }

        return nullptr;
    }

    static FREObject setDebugMode(FREContext ctx, void *funcData, uint32_t argc, FREObject argv[]) {
        writeLog("setDebugMode called");
        if (argc < 1) return nullptr;

        WebSocketSupport* wsSupport = nullptr;
        FREGetContextNativeData(ctx, reinterpret_cast<void **>(&wsSupport));

        if (wsSupport != nullptr) {
            uint32_t debugMode;
            FREGetObjectAsBool(argv[0], &debugMode);
            wsSupport->setDebugMode(debugMode);
        }

        return nullptr;
    }
};

static void WebSocketSupportContextInitializer(
        void* extData,
        const uint8_t* ctxType,
        FREContext ctx,
        uint32_t* numFunctionsToSet,
        const FRENamedFunction** functionsToSet
) {
    WebSocketSupport* wsSupport = new WebSocketSupport();
    FRESetContextNativeData(ctx, wsSupport);
    if (numFunctionsToSet) *numFunctionsToSet = wsSupport->numFunctions();
    if (functionsToSet) *functionsToSet = wsSupport->getFunctions();
}

static void WebSocketSupportContextFinalizer(FREContext ctx) {
    WebSocketSupport* wsSupport = nullptr;
    if (FRE_OK == FREGetContextNativeData(ctx, reinterpret_cast<void**>(&wsSupport))) {
        delete wsSupport;
    }
}

extern "C" {

__attribute__ ((visibility ("default")))
void InitExtension(void** extDataToSet, FREContextInitializer* ctxInitializerToSet, FREContextFinalizer* ctxFinalizerToSet) {
    writeLog("InitExtension called");
    if (extDataToSet) *extDataToSet = nullptr;
    if (ctxInitializerToSet) *ctxInitializerToSet = WebSocketSupportContextInitializer;
    if (ctxFinalizerToSet) *ctxFinalizerToSet = WebSocketSupportContextFinalizer;
    writeLog("InitExtension completed");
}

__attribute__ ((visibility ("default")))
void DestroyExtension(void* extData)  {
    writeLog("DestroyExtension called");
    closeLog();
}
} // end of extern "C"
