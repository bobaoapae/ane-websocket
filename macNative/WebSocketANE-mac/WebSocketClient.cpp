#include "WebSocketClient.hpp"
#include "WebSocketNativeLibrary.h"
#include "log.hpp"

WebSocketClient::WebSocketClient(FREContext ctx) {
    writeLog("WebSocketClient created");
    m_guidPointer = csharpWebSocketLibrary_createWebSocketClient(ctx);
    writeLog(m_guidPointer);
}

WebSocketClient::~WebSocketClient() {
}

void WebSocketClient::connect(const char* uri) {
    csharpWebSocketLibrary_connect(m_guidPointer, uri);
}

void WebSocketClient::close(uint32_t closeCode) {
    csharpWebSocketLibrary_disconnect(m_guidPointer, static_cast<int>(closeCode));
}

void WebSocketClient::sendMessage(uint8_t* bytes, int lenght) {
    csharpWebSocketLibrary_sendMessage(m_guidPointer, bytes, lenght);
}

std::optional<std::vector<uint8_t> > WebSocketClient::getNextMessage() {
    std::lock_guard guard(m_lock_receive_queue);
    if (m_received_message_queue.empty()) {
        return std::nullopt;
    }

    // Pega a próxima mensagem da fila
    std::vector<uint8_t> message = std::move(m_received_message_queue.front());
    m_received_message_queue.pop();

    return message;
}

void WebSocketClient::enqueueMessage(const std::vector<uint8_t> &message) {
    std::lock_guard guard(m_lock_receive_queue);
    m_received_message_queue.push(message);
}
