#ifndef WEBSOCKETCLIENT_H
#define WEBSOCKETCLIENT_H

#include <queue>
#include <boost/beast/ssl/ssl_stream.hpp>
#include "FileLoggerStream.h"

#ifdef WIN32
    #include <winsock2.h>
    #include <windows.h>
    #include <cstdint>
    typedef unsigned __int32 uint32_t;
    typedef unsigned __int8 uint8_t;
    typedef __int32 int32_t;
#endif

#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/beast/websocket/ssl.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/asio/connect.hpp>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <string>
#include <vector>
#include <unordered_map>
#include <mutex>
#include <functional>
#include <FlashRuntimeExtensions.h>

class WebSocketClient {
public:
    WebSocketClient();

    ~WebSocketClient();

    void connect(const std::string& uri);
    void close(boost::beast::websocket::close_code code, const std::string& reason);
    void sendMessage(const std::string& payload);
    void sendMessage(const std::vector<uint8_t>& payload);
    void setFREContext(FREContext ctx);
    void setDebugMode(bool debugMode);

    std::optional<std::vector<uint8_t>> getNextMessage();
    void enqueueMessage(const std::vector<uint8_t>& message);

private:
    bool m_open;
    bool m_disconnectDispatched;
    FREContext m_ctx;
    bool m_debugMode;
    boost::asio::io_context m_ioc;
    boost::asio::ssl::context m_ssl_context{boost::asio::ssl::context::tlsv12};
    boost::asio::ip::tcp::resolver m_resolver;
    boost::beast::websocket::stream<boost::beast::ssl_stream<boost::asio::ip::tcp::socket>> m_ws;
    std::vector<uint8_t> m_readBuffer;
    boost::asio::dynamic_vector_buffer<uint8_t, std::allocator<uint8_t>> m_buffer;
    std::mutex m_lock_receive_queue;
    std::mutex m_lock_disconnect;
    std::mutex m_lock_send_queue;
    std::mutex m_lock_cleanup;
    std::queue<std::vector<uint8_t>> m_send_message_queue;
    std::thread m_sending_thread;
    std::condition_variable m_cv;
    std::thread m_read_thread;
    std::queue<std::vector<uint8_t>> m_received_message_queue;

    void connectImpl(const std::string &uri);
    void cleanupImpl();
    void sendLoop();
    void readLoop();
    void cleanup();
    void dispatchDisconnectEvent(const uint16_t& code, const std::string& reason);
    void dispatchEvent(const std::string& eventType, const std::string& eventData);
    std::string generateUUID();
};

#endif // WEBSOCKETCLIENT_H
