#ifndef WEBSOCKETCLIENT_H
#define WEBSOCKETCLIENT_H

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

    void connect(const std::string& uri);
    void close(boost::beast::websocket::close_code code, const std::string& reason);
    void sendMessage(const std::string& payload);
    void sendMessage(const std::vector<uint8_t>& payload);
    void setFREContext(FREContext ctx);
    void setDebugMode(bool debugMode);

    std::vector<uint8_t> getMessage(const std::string& uuid);
    void storeMessage(const std::string& uuid, const std::vector<uint8_t>& message);

private:
    bool m_open;
    bool m_disconnectDispatched;
    FREContext m_ctx;
    bool m_debugMode;
    boost::asio::io_context m_ioc;
    boost::asio::ssl::context m_ssl_context{boost::asio::ssl::context::tlsv12};
    boost::asio::ip::tcp::resolver m_resolver;
    boost::beast::websocket::stream<boost::beast::ssl_stream<boost::beast::tcp_stream>> m_ws;
    boost::beast::flat_buffer m_buffer;
    std::unordered_map<std::string, std::vector<uint8_t>> message_map;
    std::mutex m_lock_messages;
    std::mutex m_lock_disconnect;
    std::mutex m_lock_write;

    void do_read();
    void cleanup();
    void dispatchDisconnectEvent(const uint16_t& code, const std::string& reason);
    void dispatchEvent(const std::string& eventType, const std::string& eventData);
    std::string generateUUID();

    // Funções adicionais que substituem as lambdas
    void on_resolve(const boost::system::error_code &ec, const boost::asio::ip::tcp::resolver::results_type &results, const std::string& protocol, const std::string& host, const std::string& target);
    void on_connect(const boost::system::error_code &ec, const boost::asio::ip::tcp::endpoint &endpoint, const std::string &protocol, const std::string& host, const std::string& target);
    void on_ssl_handshake(const boost::system::error_code &ec, const std::string &host, const std::string &target);
    void on_websocket_handshake(const std::string &host, const std::string &target);
    void on_websocket_handshake_complete(const boost::system::error_code &ec);
};

#endif // WEBSOCKETCLIENT_H
