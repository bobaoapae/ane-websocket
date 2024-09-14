#include "CSharpLibraryWrapper.h"
#include <iostream>
#include <memory>
#include <string>
#include <Windows.h>
#include <log.h>

// Use a unique_ptr with a custom deleter to manage the library handle
static std::unique_ptr<std::remove_pointer<HMODULE>::type, decltype(&FreeLibrary)> library(nullptr, FreeLibrary);

std::string GetLibraryLocation(int argc, char *argv[]) {
    std::string baseDirectory;

    // Check for -extdir argument
    for (int i = 0; i < argc; ++i) {
        if (std::string(argv[i]) == "-extdir" && i + 1 < argc) {
            baseDirectory = argv[i + 1];
            baseDirectory += R"(\br.com.redesurftank.androidwebsocket.ane)";
            break;
        }
    }

    if (baseDirectory.empty()) {
        char buffer[MAX_PATH];
        DWORD length = GetModuleFileNameA(nullptr, buffer, MAX_PATH);
        if (length == 0) {
            std::cerr << "Error getting module file name: " << GetLastError() << std::endl;
            return "";
        }
        baseDirectory = std::string(buffer, length);
        baseDirectory = baseDirectory.substr(0, baseDirectory.find_last_of("\\/"));
        baseDirectory += R"(\META-INF\AIR\extensions\br.com.redesurftank.androidwebsocket)";
    }

    return baseDirectory + R"(\META-INF\ANE\Windows-x86\WebSocketClientNativeLibrary.dll)";
}

bool loadNativeLibrary() {
    if (library) {
        std::cerr << "Library is already loaded." << std::endl;
        return true;
    }

    auto libraryPath = GetLibraryLocation(__argc, __argv);
    writeLog(("Loading native library from: " + libraryPath).c_str());

    HMODULE handle = LoadLibraryA(libraryPath.c_str());
    if (!handle) {
        std::cerr << "Could not load library: " << GetLastError() << std::endl;
        writeLog("Could not load library");
        return false;
    }

    library.reset(handle); // Pass handle directly, not &handle
    writeLog("Library loaded successfully");
    return true;
}

void *getFunctionPointer(const char *functionName) {
    if (!library && !loadNativeLibrary()) {
        return nullptr;
    }

    void *func = GetProcAddress(library.get(), functionName); // Use library.get() instead of *library
    if (!func) {
        std::cerr << "Could not load function: " << GetLastError() << std::endl;
        writeLog("Could not load function");
    } else {
        writeLog(("Function loaded: " + std::string(functionName)).c_str());
    }

    return func;
}

int initializerCallbacks(const void *callBackConnect, const void *callBackData, const void *callBackDisconnect, const void *callBackLog) {
    writeLog("initializerCallbacks called");
    using InitializerFunc = int (__cdecl *)(const void *, const void *, const void *, const void *);
    auto func = reinterpret_cast<InitializerFunc>(getFunctionPointer("initializerCallbacks"));

    if (!func) {
        writeLog("Could not load initializerCallbacks function");
        return -1;
    }

    int result = func(callBackConnect, callBackData, callBackDisconnect, callBackLog);
    writeLog(("initializerCallbacks result: " + std::to_string(result)).c_str());
    return result;
}

char *createWebSocketClient() {
    writeLog("createWebSocketClient called");
    using CreateWebSocketClientFunc = char *(__cdecl *)();
    auto func = reinterpret_cast<CreateWebSocketClientFunc>(getFunctionPointer("createWebSocketClient"));

    if (!func) {
        writeLog("Could not load createWebSocketClient function");
        return nullptr;
    }

    char *result = func();
    writeLog("createWebSocketClient result: ");
    writeLog(result);
    return result;
}

int connect(const void *guidPointer, const char *url) {
    writeLog("connect called");
    using ConnectFunc = int (__cdecl *)(const void *, const char *);
    auto func = reinterpret_cast<ConnectFunc>(getFunctionPointer("connect"));

    if (!func) {
        writeLog("Could not load connect function");
        return -1;
    }

    auto result = func(guidPointer, url);
    writeLog(("connect result: " + std::to_string(result)).c_str());
    return result;
}

void sendMessage(const void *guidPointer, const void *data, int length) {
    writeLog("sendMessage called");
    using SendMessageFunc = void (__cdecl *)(const void *, const void *, int);
    auto func = reinterpret_cast<SendMessageFunc>(getFunctionPointer("sendMessage"));

    if (!func) {
        writeLog("Could not load sendMessage function");
        return;
    }

    func(guidPointer, data, length);
}

void disconnect(const void *guidPointer, int closeCode) {
    writeLog("disconnect called");
    using DisconnectFunc = void (__cdecl *)(const void *, int);
    auto func = reinterpret_cast<DisconnectFunc>(getFunctionPointer("disconnect"));

    if (!func) {
        writeLog("Could not load disconnect function");
        return;
    }

    func(guidPointer, closeCode);
}