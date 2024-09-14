@echo off
REM Set the path to the 7z.exe file
set PATH=%PATH%;"C:\Program Files\7-Zip\"
xcopy ..\androidNative\app\build\outputs\aar\app-debug.aar app-debug.aar /Y /F
7z e app-debug.aar classes.jar -o./android -aoa
7z x app-debug.aar jni\* -o./temp -aoa
xcopy ..\out\production\AndroidWebSocketANE\AndroidWebSocketANE.swc library.swc /Y /F
xcopy .\temp\jni\* android\libs\ /E /I /S /Y
xcopy ..\windowsNative\cmake-build-release-x32\AneWebSocket.dll .\windows-32\AneWebSocket.dll /Y /F
dotnet publish /p:NativeLib=Shared /p:Configuration=Release ..\CSharpLibrary\WebSocketClientNativeLibrary\WebSocketClientNativeLibrary.csproj
xcopy ..\CSharpLibrary\WebSocketClientNativeLibrary\bin\Release\net9.0\win-x86\publish\WebSocketClientNativeLibrary.dll .\windows-32\WebSocketClientNativeLibrary.dll /Y /F
7z e library.swc library.swf -o./default -aoa
7z e library.swc library.swf -o./android -aoa
7z e library.swc library.swf -o./windows-32 -aoa
7z e library.swc library.swf -o./macos -aoa
rmdir /S /Q temp
adt -package -target ane br.com.redesurftank.androidwebsocket.ane extension.xml -swc library.swc -platform Android-ARM -platformoptions platformAndroid.xml -C android . -platform Android-ARM64 -platformoptions platformAndroid.xml -C android . -platform Android-x86 -platformoptions platformAndroid.xml -C android . -platform Android-x64 -platformoptions platformAndroid.xml -C android . -platform default -C default . -platform Windows-x86 -C windows-32 . -platform MacOS-x86-64 -C macos .