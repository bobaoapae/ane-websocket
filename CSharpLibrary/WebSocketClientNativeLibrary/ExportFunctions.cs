using System;
using System.Collections.Concurrent;
using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;
using System.Text;

namespace WebSocketClientNativeLibrary;

public static class ExportFunctions
{
    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    private delegate void CallBackConnectPointer(IntPtr contextPointer);

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    private delegate void CallBackReceivedMessagePointer(IntPtr contextPointer, IntPtr pointerArray, int length);

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    private delegate void CallBackIoErrorPointer(IntPtr contextPointer, int closeCode, IntPtr pointerMessage);

    [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
    private delegate void CallBackLogPointer(IntPtr pointerMessage);

    private static CallBackConnectPointer _callbackConnect;
    private static CallBackReceivedMessagePointer _callbackReceivedMessage;
    private static CallBackIoErrorPointer _callbackIoError;
    private static CallBackLogPointer _callbackLog;

    private static ConcurrentDictionary<Guid, WebSocketClient> _clients = new();

    [UnmanagedCallersOnly(EntryPoint = "csharpWebSocketLibrary_initializerCallbacks", CallConvs = [typeof(CallConvCdecl)])]
    public static int InitializerCallbacks(IntPtr pointerCallBackConnect, IntPtr pointerCallBackReceivedMessage, IntPtr pointerCallBackIoError, IntPtr pointerCallBackLog)
    {
        var result = -1;
        try
        {
            _callbackConnect = Marshal.GetDelegateForFunctionPointer<CallBackConnectPointer>(pointerCallBackConnect);
            _callbackReceivedMessage = Marshal.GetDelegateForFunctionPointer<CallBackReceivedMessagePointer>(pointerCallBackReceivedMessage);
            _callbackIoError = Marshal.GetDelegateForFunctionPointer<CallBackIoErrorPointer>(pointerCallBackIoError);
            _callbackLog = Marshal.GetDelegateForFunctionPointer<CallBackLogPointer>(pointerCallBackLog);
            result = 1;
        }
        catch (Exception e)
        {
            result = -2;
        }

        return result;
    }

    [UnmanagedCallersOnly(EntryPoint = "csharpWebSocketLibrary_createWebSocketClient", CallConvs = [typeof(CallConvCdecl)])]
    public static IntPtr CreateWebSocketClient(IntPtr freContext)
    {
        var guid = Guid.NewGuid();
        var guidPointer = Marshal.StringToCoTaskMemAnsi(guid.ToString());
        var client = new WebSocketClient(
            () => SafeInvoke(() => _callbackConnect(freContext)),
            data =>
                SafeInvoke(() =>
                {
                    var ptr = Marshal.AllocCoTaskMem(data.Count);
                    Marshal.Copy(data.Array!, data.Offset, ptr, data.Count);
                    _callbackReceivedMessage(freContext, ptr, data.Count);
                    Marshal.FreeCoTaskMem(ptr);
                }),
            (closeCode, error) =>
                SafeInvoke(() =>
                {
                    var ptr = Marshal.StringToCoTaskMemAnsi(error);
                    _callbackIoError(freContext, closeCode, ptr);
                    Marshal.FreeCoTaskMem(ptr);
                }),
            log =>
                SafeInvoke(() =>
                {
                    var ptr = Marshal.StringToCoTaskMemAnsi(log);
                    _callbackLog(ptr);
                    Marshal.FreeCoTaskMem(ptr);
                })
        );

        _clients.TryAdd(guid, client);
        return guidPointer;
    }

    [UnmanagedCallersOnly(EntryPoint = "csharpWebSocketLibrary_connect", CallConvs = [typeof(CallConvCdecl)])]
    public static int Connect(IntPtr guidPointer, IntPtr pointerUri)
    {
        try
        {
            var guidString = Marshal.PtrToStringAnsi(guidPointer);
            if (!Guid.TryParse(guidString, out var guid))
            {
                return 0;
            }

            if (!_clients.TryGetValue(guid, out var client))
            {
                return 0;
            }

            var uri = Marshal.PtrToStringAnsi(pointerUri);
            client.Connect(uri);
            return 1;
        }
        catch (Exception e)
        {
            LogException(e);
            return 0;
        }
    }

    [UnmanagedCallersOnly(EntryPoint = "csharpWebSocketLibrary_sendMessage", CallConvs = [typeof(CallConvCdecl)])]
    public static int SendMessage(IntPtr guidPointer, IntPtr pointerData, int length)
    {
        try
        {
            var guidString = Marshal.PtrToStringAnsi(guidPointer);
            if (!Guid.TryParse(guidString, out var guid))
            {
                return 0;
            }

            if (!_clients.TryGetValue(guid, out var client))
            {
                return 0;
            }

            var data = new byte[length];
            Marshal.Copy(pointerData, data, 0, length);
            client.Send(data);
            return 1;
        }
        catch (Exception e)
        {
            LogException(e);
            return 0;
        }
    }

    [UnmanagedCallersOnly(EntryPoint = "csharpWebSocketLibrary_disconnect", CallConvs = [typeof(CallConvCdecl)])]
    public static int Disconnect(IntPtr guidPointer, int closeCode)
    {
        try
        {
            var guidString = Marshal.PtrToStringAnsi(guidPointer);
            if (!Guid.TryParse(guidString, out var guid))
            {
                return 0;
            }

            if (!_clients.TryGetValue(guid, out var client))
            {
                return 0;
            }

            client.Disconnect(closeCode);
            client.Dispose();
            return 1;
        }
        catch (Exception e)
        {
            LogException(e);
            return 0;
        }
    }

    [UnmanagedCallersOnly(EntryPoint = "csharpWebSocketLibrary_addStaticHost", CallConvs = [typeof(CallConvCdecl)])]
    public static void AddStaticHost(IntPtr hostPtr, IntPtr ipPtr)
    {
        try
        {
            var host = Marshal.PtrToStringAnsi(hostPtr);
            var ip = Marshal.PtrToStringAnsi(ipPtr);

            WebSocketClient.AddStaticHost(host, ip);
        }
        catch
        {
            // ignored
        }
    }

    [UnmanagedCallersOnly(EntryPoint = "csharpWebSocketLibrary_removeStaticHost", CallConvs = [typeof(CallConvCdecl)])]
    public static void RemoveStaticHost(IntPtr hostPtr)
    {
        try
        {
            var host = Marshal.PtrToStringAnsi(hostPtr);

            WebSocketClient.RemoveStaticHost(host);
        }
        catch
        {
            // ignored
        }
    }

    private static void SafeInvoke(Action action)
    {
        try
        {
            action();
        }
        catch (Exception e)
        {
            LogException(e);
        }
    }

    private static void LogException(Exception e)
    {
        var ptr = Marshal.StringToCoTaskMemAnsi(e.ToString());
        SafeInvoke(() => _callbackLog(ptr));
        Marshal.FreeCoTaskMem(ptr);
    }
}