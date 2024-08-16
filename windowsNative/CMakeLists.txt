cmake_minimum_required(VERSION 3.28)
project(AneWebSocket)

set(CMAKE_CXX_STANDARD 17)
add_definitions(-D_WIN32_WINNT=0x0601)

# Definindo as opções de compilação para Debug e Release
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /D_ITERATOR_DEBUG_LEVEL=2 /MTd")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /D_ITERATOR_DEBUG_LEVEL=0 /MT")

if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(STATUS "Compiling for x64")
    set(LIBRARY_PATH "C:/AIRSDKs/AIRSDK_51.0.1/lib/win64")
    set(INCLUDE_PATH "C:/AIRSDKs/AIRSDK_51.0.1/include")
else()
    message(STATUS "Compiling for x86")
    set(LIBRARY_PATH "C:/AIRSDKs/AIRSDK_51.0.1/lib/win")
    set(INCLUDE_PATH "C:/AIRSDKs/AIRSDK_51.0.1/include")
endif()

if(VCPKG_TARGET_TRIPLET MATCHES "-static$")
    message(STATUS "Compiling static")
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
else()
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreadedDLL$<$<CONFIG:Debug>:Debug>")
endif()

link_directories(${LIBRARY_PATH})
include_directories(${INCLUDE_PATH})
include_directories(${CMAKE_SOURCE_DIR}/src)

find_package(Boost REQUIRED COMPONENTS thread system uuid beast)
include_directories(${Boost_INCLUDE_DIRS})

find_package(OpenSSL REQUIRED)

add_library(AneWebSocket SHARED
        src/library.h
        src/library.cpp
        src/WebSocketClient.h
        src/WebSocketClient.cpp
        src/log.h
        src/log.cpp
        src/FileLoggerStream.cpp
        src/FileLoggerStream.h
)

target_link_libraries(AneWebSocket PRIVATE ${LIBRARY_PATH}/FlashRuntimeExtensions.lib)
target_link_libraries(AneWebSocket PUBLIC ${Boost_LIBRARIES} OpenSSL::SSL PRIVATE ws2_32 bcrypt)

set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /O2 /GL")
set(CMAKE_SHARED_LINKER_FLAGS_RELEASE "${CMAKE_SHARED_LINKER_FLAGS_RELEASE} /LTCG /OPT:REF /OPT:ICF /DEBUG /PDBALTPATH:%_PDB%")