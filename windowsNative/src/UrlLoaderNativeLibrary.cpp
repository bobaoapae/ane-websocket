//
// Created by User on 31/07/2024.
//
#include <iostream>
#include "UrlLoaderNativeLibrary.h"

#include <string>

#include "log.h"

static void *library = nullptr;

std::string GetLibraryLocation(int argc, char *argv[]) {
    std::string baseDirectory;

    // Verificar se o argumento -extdir foi passado
    for (int i = 0; i < argc; ++i) {
        if (std::string(argv[i]) == "-extdir" && i + 1 < argc) {
            baseDirectory = argv[i + 1];
            // Anexar br.com.redesurftank.androidwebsocket.ane ao diretório base
            baseDirectory += R"(\br.com.redesurftank.androidwebsocket.ane)";
            break;
        }
    }

    if (baseDirectory.empty()) {
        // Se não foi passado, usar o diretório do executável atual
        char buffer[MAX_PATH];
        DWORD length = GetModuleFileNameA(nullptr, buffer, MAX_PATH);
        if (length == 0) {
            std::cerr << "Error getting module file name: " << GetLastError() << std::endl;
            return "";
        }
        std::string fullPath(buffer, length);
        baseDirectory = fullPath.substr(0, fullPath.find_last_of("\\/"));
    }

    return baseDirectory + R"(\META-INF\ANE\Windows-x86\UrlLoaderNativeLibrary.dll)";
}

void *loadNativeLibrary() {
    if (library) {
        std::cerr << "Library is already loaded." << std::endl;
        return library;
    }
    writeLog("Loading native library");
    auto libraryPath = GetLibraryLocation(__argc, __argv);
    writeLog(libraryPath.c_str());
#ifdef _WIN32
    HINSTANCE handle = LoadLibraryA(libraryPath.c_str());
#else
    void *handle = dlopen(libraryPath, RTLD_LAZY);
#endif

    if (!handle) {
#ifdef _WIN32
        std::cerr << "Could not load library: " << GetLastError() << std::endl;
        writeLog("Could not load library");
#else
        std::cerr << "Could not load library: " << dlerror() << std::endl;
        writeLog("Could not load library");
#endif
        return nullptr;
    }

    writeLog("Library loaded");
    library = handle;
    return library;
}

void *getFunctionPointer(const char *functionName) {
    loadNativeLibrary();
    if (!library) {
        std::cerr << "Library is not loaded." << std::endl;
        return nullptr;
    }

    void *func = nullptr;
#ifdef _WIN32
    func = GetProcAddress((HINSTANCE) library, functionName);
#else
    func = dlsym(library, functionName);
#endif

    if (!func) {
#ifdef _WIN32
        std::cerr << "Could not load function: " << GetLastError() << std::endl;
        writeLog("Could not load function");
#else
        std::cerr << "Could not load function: " << dlerror() << std::endl;
#endif
    }

    writeLog("Function loaded");
    return func;
}

int callInitializerLoader() {
    writeLog("Calling initializerLoader");
    typedef int (*myFunc)();
    myFunc func = (myFunc) getFunctionPointer("initializerLoader");
    if (!func) {
        writeLog("Could not load function initializerLoader");
        return -1;
    }

    int result = func();
    writeLog("InitializerLoader called");

    std::string resultMsg = "Result: " + std::to_string(result);
    writeLog(resultMsg.c_str());
    return result;
}
