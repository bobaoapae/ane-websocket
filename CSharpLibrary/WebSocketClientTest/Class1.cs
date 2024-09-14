using System;
using WebSocketClientNativeLibrary;

var client = new WebSocketClient(
    () => Console.WriteLine("connected"),
    data => Console.WriteLine($"received message: {data}"),
    (closeCode, error) => Console.WriteLine($"io error: {closeCode} - {error}"),
    message => Console.WriteLine($"log: {message}")
);

await client.ConnectAsync("wss://websocket.redesurftank.com.br/token123/1016");


while (true)
{
    var message = Console.ReadLine();
    if (message == "exit")
    {
        break;
    }
    
    byte[] data = System.Text.Encoding.UTF8.GetBytes(message);
    
    client.Send(data);
}