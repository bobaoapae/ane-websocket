#ifndef WEBSOCKETCLIENT_H
#define WEBSOCKETCLIENT_H
#include "FileLoggerStream.h"

#ifdef WIN32
    #include <winsock2.h>
    #include <windows.h>
    #include <cstdint>
    typedef unsigned __int32 uint32_t;
typedef unsigned __int8 uint8_t;
typedef __int32 int32_t;
#endif

#include <websocketpp/config/asio_client.hpp>
#include <websocketpp/client.hpp>
#include <websocketpp/common/thread.hpp>
#include <websocketpp/common/memory.hpp>
#include <websocketpp/common/connection_hdl.hpp>
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
    void close(websocketpp::close::status::value code, const std::string& reason);
    void sendMessage(websocketpp::frame::opcode::value opcode, const std::string& payload);
    void sendMessage(websocketpp::frame::opcode::value opcode, const std::vector<uint8_t>& payload);
    void setFREContext(FREContext ctx);
    void setDebugMode(bool debugMode);

    std::vector<uint8_t> getMessage(const std::string& uuid);
    void storeMessage(const std::string& uuid, const std::vector<uint8_t>& message);

private:
    typedef websocketpp::client<websocketpp::config::asio_tls_client> client;
    client m_client;
    websocketpp::connection_hdl m_hdl;
    bool m_open;
    std::mutex m_lock;
    FREContext m_ctx;
    bool m_debugMode;

    std::unordered_map<std::string, std::vector<uint8_t>> message_map;

    void on_message(websocketpp::connection_hdl hdl, client::message_ptr msg);
    void on_fail(websocketpp::connection_hdl hdl);
    void on_close(websocketpp::connection_hdl hdl);
    void dispatchEvent(const std::string& eventType, const std::string& eventData);
    std::string generateUUID();

    websocketpp::lib::shared_ptr<boost::asio::ssl::context> on_tls_init(websocketpp::connection_hdl hdl);

    void cleanup();
};

#endif // WEBSOCKETCLIENT_H
