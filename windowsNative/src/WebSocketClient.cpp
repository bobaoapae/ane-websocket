#include "WebSocketClient.h"
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <websocketpp/config/asio_client.hpp>
#include <websocketpp/client.hpp>
#include <boost/asio/ssl.hpp>

#include "log.h"

WebSocketClient::WebSocketClient() : m_open(false), m_ctx(nullptr), m_debugMode(false) {
    writeLog("WebSocketClient created");
    m_client.set_access_channels(websocketpp::log::alevel::all);
    m_client.clear_access_channels(websocketpp::log::alevel::frame_payload);
    m_client.clear_access_channels(websocketpp::log::alevel::frame_header);
    m_client.clear_access_channels(websocketpp::log::alevel::message_header);
    m_client.clear_access_channels(websocketpp::log::alevel::message_payload);
    m_client.init_asio();
    m_client.set_tls_init_handler([this](websocketpp::connection_hdl hdl) {
        return on_tls_init(hdl);
    });
    m_client.set_open_handler([this](websocketpp::connection_hdl hdl) {
        std::lock_guard guard(m_lock);
        m_hdl = hdl;
        m_open = true;
        dispatchEvent("connected", "Connection opened");
    });

    m_client.set_message_handler([this](websocketpp::connection_hdl hdl, client::message_ptr msg) {
        on_message(hdl, msg);
    });

    m_client.set_close_handler([this](websocketpp::connection_hdl hdl) {
        on_close(hdl);
    });

    m_client.set_fail_handler([this](websocketpp::connection_hdl hdl) {
        on_fail(hdl);
    });

    m_client.set_open_handshake_timeout(3000);
}

websocketpp::lib::shared_ptr<boost::asio::ssl::context> WebSocketClient::on_tls_init(websocketpp::connection_hdl hdl) {
    writeLog("on_tls_init called");
    websocketpp::lib::shared_ptr<boost::asio::ssl::context> ctx = websocketpp::lib::make_shared<boost::asio::ssl::context>(boost::asio::ssl::context::tlsv12);

    try {
        ctx->set_options(boost::asio::ssl::context::default_workarounds |
                         boost::asio::ssl::context::no_sslv2 |
                         boost::asio::ssl::context::no_sslv3 |
                         boost::asio::ssl::context::single_dh_use |
                         boost::asio::ssl::context::tlsv11 |
                         boost::asio::ssl::context::tlsv12);

        // Ignorar verificação de certificado
        ctx->set_verify_mode(boost::asio::ssl::verify_none);
    } catch (std::exception &e) {
        std::cout << "Error in context pointer: " << e.what() << std::endl;
    }

    return ctx;
}

void WebSocketClient::cleanup() {
    writeLog("cleanup called");
    std::lock_guard<std::mutex> guard(m_lock);

    // Reset the client's state
    m_client.stop();
    m_open = false;

    // Clear stored messages
    message_map.clear();

    // Reset handlers
    m_client.set_tls_init_handler(nullptr);
    m_client.set_open_handler(nullptr);
    m_client.set_message_handler(nullptr);
    m_client.set_close_handler(nullptr);
    m_client.set_fail_handler(nullptr);
}

void WebSocketClient::connect(const std::string &uri) {
    writeLog("connect called");
    websocketpp::lib::error_code ec;
    client::connection_ptr con = m_client.get_connection(uri, ec);

    if (ec) {
        dispatchEvent("error", "Connection failed: " + ec.message());
        cleanup();
        return;
    }

    m_client.connect(con);
    std::thread([this]() {
        try {
            m_client.run();
        } catch (std::exception const &e) {
            writeLog(("run exception: " + std::string(e.what())).c_str());
        }
    }).detach();
}

void WebSocketClient::close(websocketpp::close::status::value code, const std::string &reason) {
    std::thread([this, code, reason]() {
        std::lock_guard<std::mutex> guard(m_lock);
        if (m_open) {
            try {
                m_client.close(m_hdl, code, reason);
            } catch (websocketpp::exception const &e) {
                writeLog(("close exception: " + std::string(e.what())).c_str());
            }
        }
    }).detach();
}

void WebSocketClient::sendMessage(websocketpp::frame::opcode::value opcode, const std::string &payload) {
    writeLog("sendMessage called");
    std::lock_guard<std::mutex> guard(m_lock);
    if (m_open) {
        m_client.send(m_hdl, payload, opcode);
    }
}

void WebSocketClient::sendMessage(websocketpp::frame::opcode::value opcode, const std::vector<uint8_t> &payload) {
    writeLog("sendMessage called");
    std::lock_guard<std::mutex> guard(m_lock);
    if (m_open) {
        m_client.send(m_hdl, payload.data(), payload.size(), opcode);
    }
}

void WebSocketClient::setFREContext(FREContext ctx) {
    m_ctx = ctx;
}

void WebSocketClient::setDebugMode(bool debugMode) {
    m_debugMode = debugMode;
}

std::vector<uint8_t> WebSocketClient::getMessage(const std::string &uuid) {
    std::lock_guard<std::mutex> guard(m_lock);
    auto it = message_map.find(uuid);
    if (it != message_map.end()) {
        std::vector<uint8_t> message = it->second;
        message_map.erase(it);
        return message;
    }
    return std::vector<uint8_t>();
}

void WebSocketClient::storeMessage(const std::string &uuid, const std::vector<uint8_t> &message) {
    std::lock_guard<std::mutex> guard(m_lock);
    message_map[uuid] = message;
}

void WebSocketClient::on_message(websocketpp::connection_hdl hdl, client::message_ptr msg) {
    std::vector<uint8_t> payload(msg->get_payload().begin(), msg->get_payload().end());
    std::string uuid = generateUUID();

    storeMessage(uuid, payload);
    dispatchEvent("binaryMessageFast", uuid);
}

void WebSocketClient::on_fail(websocketpp::connection_hdl hdl) {
    writeLog("on_fail called");
    std::lock_guard<std::mutex> guard(m_lock);
    m_open = false;

    client::connection_ptr con = m_client.get_con_from_hdl(hdl);
    websocketpp::lib::error_code ec = con->get_ec();
    std::string reason = con->get_ec().message();

    std::string eventData = "Connection failed: " + reason;
    dispatchEvent("error", eventData);
    cleanup(); // Ensure cleanup is called
}

void WebSocketClient::on_close(websocketpp::connection_hdl hdl) {
    writeLog("on_close called");
    std::lock_guard<std::mutex> guard(m_lock);
    m_open = false;

    client::connection_ptr con = m_client.get_con_from_hdl(hdl);
    websocketpp::close::status::value code = con->get_remote_close_code();
    std::string reason = con->get_remote_close_reason();

    std::string eventData = std::to_string(code) + ";" + reason + ";0";
    dispatchEvent("disconnected", eventData);
    cleanup(); // Add this line to call cleanup
}

void WebSocketClient::dispatchEvent(const std::string &eventType, const std::string &eventData) {
    if (m_ctx == nullptr) return;

    FREDispatchStatusEventAsync(m_ctx, reinterpret_cast<const uint8_t *>(eventType.c_str()), reinterpret_cast<const uint8_t *>(eventData.c_str()));
}

std::string WebSocketClient::generateUUID() {
    boost::uuids::uuid uuid = boost::uuids::random_generator()();
    return boost::uuids::to_string(uuid);
}
