@echo off
REM SSH copy for macOS and iOS libraries (before packaging)
echo Copying macOS and iOS libraries via SSH

REM Copying iOS library
scp joaovitorborges@192.168.80.102:/Users/joaovitorborges/IdeaProjects/ane-websocket/CSharpLibrary/WebSocketClientNativeLibrary/bin/Release/net9.0/ios-universal/WebSocketClientNativeLibrary.dylib .\ios\

REM Delete the local macOS framework folder before copying
if exist .\macos\WebSocketANE.framework (
    rmdir /S /Q .\macos\WebSocketANE.framework
)

REM Copying macOS framework
scp -r joaovitorborges@192.168.80.102:/Users/joaovitorborges/IdeaProjects/ane-websocket/macNative/DerivedData/WebSocketANE/Build/Products/Debug/WebSocketANE.framework/Versions/Current .\macos\WebSocketANE.framework