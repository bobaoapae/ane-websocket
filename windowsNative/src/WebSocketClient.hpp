//
//  WebSocketClient.hpp
//  WebSocketANE
//
//  Created by João Vitor Borges on 23/08/24.
//

#ifndef WebSocketClient_hpp
#define WebSocketClient_hpp

#include <windows.h>
#include <FlashRuntimeExtensions.h>
#include <queue>
#include <vector>
#include <mutex>
#include <optional>

class WebSocketClient {
public:
    explicit WebSocketClient(FREContext ctx);

    ~WebSocketClient();

    void connect(const char *uri) const;

    void close(uint32_t closeCode) const;

    void sendMessage(uint8_t *bytes, int lenght) const;

    std::optional<std::vector<uint8_t> > getNextMessage();

    void enqueueMessage(const std::vector<uint8_t> &message);

private:
    std::mutex m_lock_receive_queue;
    std::queue<std::vector<uint8_t> > m_received_message_queue;
    char *m_guidPointer;
};

#endif /* WebSocketClient_hpp */
