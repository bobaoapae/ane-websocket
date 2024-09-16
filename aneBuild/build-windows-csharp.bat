@echo off
xcopy ..\windowsNative\cmake-build-release-x32\AneWebSocket.dll .\windows-32\AneWebSocket.dll /Y /F
dotnet publish /p:NativeLib=Shared /p:Configuration=Release ..\CSharpLibrary\WebSocketClientNativeLibrary\WebSocketClientNativeLibrary.csproj
xcopy ..\CSharpLibrary\WebSocketClientNativeLibrary\bin\Release\net9.0\win-x86\publish\WebSocketClientNativeLibrary.dll .\windows-32\WebSocketClientNativeLibrary.dll /Y /F