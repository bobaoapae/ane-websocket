using System;
using System.Buffers;
using System.Collections.Concurrent;
using System.Linq;
using System.Net.WebSockets;
using System.Threading;
using System.Threading.Tasks;
using DnsClient;

namespace WebSocketClientNativeLibrary;

public class WebSocketClient : IDisposable
{
    private static readonly LookupClient DnsClient = new(new LookupClientOptions(NameServer.Cloudflare, NameServer.Cloudflare2, NameServer.GooglePublicDns, NameServer.GooglePublicDns2)
    {
        UseCache = true
    });

    private readonly ConcurrentQueue<byte[]> _sendQueue = new();
    private CancellationTokenSource _cancellationTokenSource;

    // Callbacks
    private readonly Action _onConnect;
    private readonly Action<ArraySegment<byte>> _onReceived;
    private readonly Action<int, string> _onIoError;
    private readonly Action<string> _onLog;

    private ClientWebSocket _activeWebSocket;
    private bool _disposed = false;

    public WebSocketClient(Action onConnect, Action<ArraySegment<byte>> onReceived, Action<int, string> onIoError, Action<string> onLog)
    {
        _onConnect = onConnect;
        _onReceived = onReceived;
        _onIoError = onIoError;
        _onLog = onLog; // Log callback
    }

    public void Connect(string uri)
    {
        _ = Task.Run(async () => await ConnectAsync(uri));
    }

    public async Task ConnectAsync(string uri)
    {
        _cancellationTokenSource = new CancellationTokenSource();

        try
        {
            _onLog?.Invoke($"Connecting to {uri}...");
            var uriObject = new Uri(uri);
            var host = uriObject.Host;

            // Resolve the host to multiple IPs
            var queryResult = await DnsClient.GetHostEntryAsync(host);
            var ipAddresses = queryResult.AddressList;

            if (ipAddresses.Length == 0)
            {
                throw new Exception("No IP addresses resolved for the domain.");
            }

            _onLog?.Invoke($"Resolved {ipAddresses.Length} IP addresses for {host}.");

            // Try to connect to all resolved IPs in parallel
            var ctxSource = new CancellationTokenSource();
            var connectionTasks = ipAddresses.Select(ip => AttemptConnectionAsync(uriObject, ip.ToString(), ctxSource.Token)).ToList();
            var completedTask = await Task.WhenAny(connectionTasks);

            _activeWebSocket = completedTask.Result;

            ctxSource.Cancel(); // Cancel the other connection attempts

            _ = Task.Run(async () =>
            {
                await Task.WhenAll(connectionTasks); // Wait for all connection attempts to finish
                foreach (var task in connectionTasks)
                {
                    if (task == completedTask || task.Result is not { State: WebSocketState.Open })
                        continue;
                    try
                    {
                        await task.Result.CloseAsync(WebSocketCloseStatus.EndpointUnavailable, "Connection established", CancellationToken.None);
                    }
                    catch (Exception)
                    {
                        //ignore
                    }

                    task.Result.Dispose();
                }
            });

            // Check if the connection succeeded
            if (_activeWebSocket is { State: WebSocketState.Open })
            {
                _onConnect?.Invoke(); // Callback after successful connection

                _onLog?.Invoke("Connection established.");

                // Start background tasks for sending and receiving messages
                _ = Task.Factory.StartNew(() => SendLoopAsync(_cancellationTokenSource.Token), TaskCreationOptions.LongRunning);
                _ = Task.Factory.StartNew(() => ReceiveLoopAsync(_cancellationTokenSource.Token), TaskCreationOptions.LongRunning);
            }
            else
            {
                _onLog?.Invoke("All connection attempts failed.");
                await DisconnectAsync((int)WebSocketCloseStatus.EndpointUnavailable, "All connection attempts failed.");
            }
        }
        catch (Exception ex)
        {
            _onLog?.Invoke($"Error during connection: {ex.Message}");
            await DisconnectAsync((int)WebSocketCloseStatus.InternalServerError, $"Error during connection: {ex.Message}");
        }
    }

    private async Task<ClientWebSocket> AttemptConnectionAsync(Uri uri, string ipAddress, CancellationToken ctx)
    {
        try
        {
            var webSocket = new ClientWebSocket();

            // Set the 'Host' header to the domain from the original URI
            webSocket.Options.SetRequestHeader("Host", uri.Host);

            _onLog?.Invoke($"Attempting connection to {uri} via IP {ipAddress}");

            // Create the URI using the IP address but keep the correct path and scheme
            var webSocketUri = new UriBuilder(uri)
            {
                Host = ipAddress // Use the IP address for the connection
            }.Uri;

            await webSocket.ConnectAsync(webSocketUri, ctx);

            if (webSocket.State == WebSocketState.Open)
            {
                return webSocket;
            }
        }
        catch (Exception ex)
        {
            _onLog?.Invoke($"Error connecting to {ipAddress}: {ex.Message}");
        }

        return null;
    }


    public void Send(byte[] data)
    {
        // Synchronous method to add data to the send queue
        _sendQueue.Enqueue(data);
    }

    private async Task SendLoopAsync(CancellationToken cancellationToken)
    {
        while (!cancellationToken.IsCancellationRequested)
        {
            if (_sendQueue.TryDequeue(out var data))
            {
                try
                {
                    await _activeWebSocket.SendAsync(new ArraySegment<byte>(data), WebSocketMessageType.Binary, true, cancellationToken);
                    _onLog?.Invoke("Message sent.");
                }
                catch (Exception ex)
                {
                    await DisconnectAsync((int)WebSocketCloseStatus.InternalServerError, $"Error during send: {ex.Message}");
                }
            }

            await Task.Delay(10, cancellationToken); // Avoid busy-waiting
        }
    }

    private async Task ReceiveLoopAsync(CancellationToken cancellationToken)
    {
        var bufferPool = ArrayPool<byte>.Shared; // ArrayPool for efficient buffer management
        var buffer = bufferPool.Rent(1024); // Rent a buffer from the pool
        try
        {
            while (!cancellationToken.IsCancellationRequested)
            {
                var totalBytesReceived = 0;
                WebSocketReceiveResult result;

                do
                {
                    result = await _activeWebSocket.ReceiveAsync(new ArraySegment<byte>(buffer, totalBytesReceived, buffer.Length - totalBytesReceived), cancellationToken);

                    if (result.MessageType == WebSocketMessageType.Close)
                    {
                        await DisconnectAsync((int)result.CloseStatus.GetValueOrDefault()); // Get the close code from the client
                        return;
                    }

                    totalBytesReceived += result.Count;

                    if (totalBytesReceived >= buffer.Length)
                    {
                        var newBuffer = bufferPool.Rent(buffer.Length * 2); // Double the buffer size if necessary
                        Array.Copy(buffer, newBuffer, buffer.Length);
                        bufferPool.Return(buffer); // Return the old buffer
                        buffer = newBuffer;
                    }
                } while (!result.EndOfMessage); // Keep receiving until the end of the message
                
                _onReceived?.Invoke(new ArraySegment<byte>(buffer, 0, totalBytesReceived));
                _onLog?.Invoke("Message received.");
            }
        }
        catch (Exception ex)
        {
            await DisconnectAsync((int)WebSocketCloseStatus.InternalServerError, $"Error during receive: {ex.Message}");
        }
        finally
        {
            bufferPool.Return(buffer); // Return the buffer to the pool
        }
    }

    public void Disconnect(int closeReason)
    {
        _ = Task.Run(async () => await DisconnectAsync(closeReason));
    }

    private async Task DisconnectAsync(int closeReason, string reason = "Closing connection gracefully.")
    {
        if (_activeWebSocket is { State: WebSocketState.Open or WebSocketState.CloseReceived })
        {
            _onIoError?.Invoke(closeReason, reason);
            try
            {
                await _activeWebSocket.CloseAsync((WebSocketCloseStatus)closeReason, $"Closing with reason {closeReason}", CancellationToken.None);
                _onLog?.Invoke($"Connection closed gracefully. Reason: {closeReason}");
            }
            catch (Exception ex)
            {
                _onLog?.Invoke($"Error during close: {ex.Message}");
            }
        }

        _cancellationTokenSource?.Cancel();
        _onLog?.Invoke($"Connection closed. Reason: {closeReason}");
    }

    public void Stop()
    {
        _cancellationTokenSource?.Cancel();
    }

    public void Dispose()
    {
        Dispose(true);
        GC.SuppressFinalize(this);
    }

    protected virtual void Dispose(bool disposing)
    {
        if (!_disposed)
        {
            if (disposing)
            {
                // Dispose managed resources
                _cancellationTokenSource?.Cancel();
                _cancellationTokenSource?.Dispose();
                _activeWebSocket?.Dispose();
            }

            // Mark as disposed
            _disposed = true;
            _onLog?.Invoke("WebSocket client disposed.");
        }
    }

    ~WebSocketClient()
    {
        Dispose(false);
    }
}