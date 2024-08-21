#include "WebSocketClient.h"

#include <future>

#include "log.h"
#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/beast/ssl/ssl_stream.hpp>
#include <utility>

void ssl_info_callback(const SSL *ssl, int where, int ret) {
    const char *str = (where & SSL_ST_CONNECT) ? "SSL_connect" : (where & SSL_ST_ACCEPT) ? "SSL_accept" : "undefined";
    if (where & SSL_CB_LOOP) {
        writeLog((std::string("SSL: ") + str + ": " + SSL_state_string_long(ssl)).c_str());
    } else if (where & SSL_CB_ALERT) {
        const char *alert_type = (where & SSL_CB_READ) ? "read" : "write";
        writeLog((std::string("SSL alert ") + alert_type + ": " + SSL_alert_type_string_long(ret) + ": " + SSL_alert_desc_string_long(ret)).c_str());
    } else if (where & SSL_CB_EXIT) {
        if (ret == 0) {
            writeLog((std::string("SSL: ") + str + ": failed in " + SSL_state_string_long(ssl)).c_str());
        } else if (ret < 0) {
            writeLog((std::string("SSL: ") + str + ": error in " + SSL_state_string_long(ssl)).c_str());
        }
    }
}

WebSocketClient::WebSocketClient()
    : m_open(false), m_disconnectDispatched(false), m_ctx(nullptr), m_debugMode(false), m_resolver(m_ioc), m_ws(m_ioc, m_ssl_context), m_buffer(m_readBuffer) {
    writeLog("WebSocketClient created");
    m_ssl_context.set_options(
        boost::asio::ssl::context::default_workarounds |
        boost::asio::ssl::context::no_sslv2 | // Desabilitar SSLv2
        boost::asio::ssl::context::no_sslv3 | // Desabilitar SSLv3
        boost::asio::ssl::context::single_dh_use // Usar Diffie-Hellman de chave única
    );

    // Adicionar suporte para TLS 1.1, 1.2, e 1.3, com fallback para versões mais baixas
    if (SSL_CTX_set_max_proto_version(m_ssl_context.native_handle(), TLS1_3_VERSION) != 1) {
        writeLog("Failed to set TLS max protocol version to TLS 1.3");
    }
    if (SSL_CTX_set_min_proto_version(m_ssl_context.native_handle(), TLS1_1_VERSION) != 1) {
        writeLog("Failed to set TLS min protocol version to TLS 1.1");
    }

    SSL_CTX_set_cipher_list(m_ssl_context.native_handle(), "HIGH:!aNULL:!MD5:!RC4");

    m_ssl_context.set_verify_mode(boost::asio::ssl::verify_none); // Desabilitar verificação de certificado para depuração
    SSL_CTX_set_info_callback(m_ssl_context.native_handle(), &ssl_info_callback);
}

WebSocketClient::~WebSocketClient() {
    m_open = false;
    m_sending_thread.detach();
    m_read_thread.detach();
}

void WebSocketClient::connect(const std::string &uri) {
    writeLog("connect called");

    try {
        std::string host, port, target;
        const size_t host_start = uri.find("://") + 3;
        const size_t path_start = uri.find('/', host_start);

        std::string protocol = uri.substr(0, host_start - 3);
        if (path_start != std::string::npos) {
            host = uri.substr(host_start, path_start - host_start);
            target = uri.substr(path_start);
        } else {
            host = uri.substr(host_start);
            target = "/";
        }

        writeLog(("Protocol: " + protocol).c_str());
        writeLog(("Host: " + host).c_str());
        writeLog(("Target: " + target).c_str());

        if (target.empty()) {
            target = "/";
        }

        if (protocol == "http" || protocol == "ws") {
            port = "80";
        } else if (protocol == "https" || protocol == "wss") {
            port = "443";
        } else {
            throw std::runtime_error("Unsupported protocol: " + protocol);
        }

        SSL *ssl = m_ws.next_layer().native_handle();
        if (!SSL_set_tlsext_host_name(ssl, host.c_str())) {
            writeLog("Failed to set SNI host name");
        }

        writeLog(("Resolved Port: " + port).c_str());

        m_resolver.async_resolve(host, port, [this, protocol, host, target](auto &&PH1, auto &&PH2) { on_resolve(std::forward<decltype(PH1)>(PH1), std::forward<decltype(PH2)>(PH2), protocol, host, target); });

        std::thread([this] {
            writeLog("Starting io_context run loop");
            m_ioc.run();
        }).detach();
    } catch (std::exception const &e) {
        writeLog(("connect exception: " + std::string(e.what())).c_str());
        dispatchEvent("error", e.what());
        cleanup();
    }
}

void WebSocketClient::on_resolve(const boost::system::error_code &ec, const boost::asio::ip::tcp::resolver::results_type &results, const std::string &protocol, const std::string &host, const std::string &target) {
    if (ec) {
        writeLog(("Resolve error: " + ec.message()).c_str());
        dispatchEvent("error", "Failed to resolve host: " + ec.message());
        cleanup();
        return;
    }

    writeLog("Host resolved successfully");
    get_lowest_layer(m_ws).expires_after(std::chrono::seconds(30));

    get_lowest_layer(m_ws).async_connect(results, [this, protocol, host, target](auto &&PH1, auto &&PH2) { on_connect(std::forward<decltype(PH1)>(PH1), std::forward<decltype(PH2)>(PH2), protocol, host, target); }
    );
}

void WebSocketClient::on_connect(const boost::system::error_code &ec, const boost::asio::ip::tcp::endpoint &endpoint, const std::string &protocol, const std::string &host, const std::string &target) {
    if (ec) {
        writeLog(("Connect error: " + ec.message()).c_str());
        dispatchEvent("error", "Failed to connect to host: " + ec.message());
        cleanup();
        return;
    }

    writeLog(("Connected to: " + endpoint.address().to_string()).c_str());

    if (protocol == "https" || protocol == "wss") {
        writeLog("Starting SSL handshake");
        m_ws.next_layer().async_handshake(boost::asio::ssl::stream_base::client, [this, host, target](auto &&PH1) { on_ssl_handshake(std::forward<decltype(PH1)>(PH1), host, target); }
        );
    } else {
        on_websocket_handshake(host, target);
    }
}

void WebSocketClient::on_ssl_handshake(const boost::system::error_code &ec, const std::string &host, const std::string &target) {
    if (ec) {
        writeLog(("SSL Handshake error: " + ec.message()).c_str());
        dispatchEvent("error", "SSL handshake failed: " + ec.message());
        cleanup();
        return;
    }

    writeLog("SSL handshake successful");
    on_websocket_handshake(host, target);
}

void WebSocketClient::on_websocket_handshake(const std::string &host, const std::string &target) {
    get_lowest_layer(m_ws).expires_never();

    m_ws.set_option(boost::beast::websocket::stream_base::timeout::suggested(boost::beast::role_type::client));

    m_ws.set_option(boost::beast::websocket::stream_base::decorator(
        [](boost::beast::websocket::request_type &req) {
            req.set(boost::beast::http::field::user_agent, std::string(BOOST_BEAST_VERSION_STRING) + " websocket-client-async");
        }
    ));

    writeLog("Starting WebSocket handshake");
    m_ws.async_handshake(host, target, [this](auto &&PH1) { on_websocket_handshake_complete(std::forward<decltype(PH1)>(PH1)); }
    );
}

void WebSocketClient::on_websocket_handshake_complete(const boost::system::error_code &ec) {
    if (ec) {
        writeLog(("WebSocket Handshake error: " + ec.message()).c_str());
        dispatchEvent("error", "WebSocket handshake failed: " + ec.message());
        cleanup();
        return;
    }

    writeLog("WebSocket handshake successful");

    m_open = true;
    dispatchEvent("connected", "Connection opened");
    m_sending_thread = std::thread(&WebSocketClient::sendLoop, this);
    m_read_thread = std::thread(&WebSocketClient::readLoop, this);
}

void WebSocketClient::sendLoop() {
    writeLog("sendLoop started");
    while (m_open) {
        std::vector<uint8_t> payload; {
            std::unique_lock lock(m_lock_write);
            m_cv.wait(lock, [this] { return !m_send_message_queue.empty() || !m_open; });

            if (!m_open && m_send_message_queue.empty()) {
                break;
            }

            payload = std::move(m_send_message_queue.front());
            m_send_message_queue.pop();
        }

        try {
            m_ws.binary(true);
            m_ws.write(boost::asio::buffer(payload));
            writeLog("Message sent successfully");
        } catch (const std::exception &e) {
            std::string error = e.what();
            writeLog(("sendMessage exception: " + error).c_str());
        }
    }
    writeLog("sendLoop finished");
}

void WebSocketClient::readLoop() {
    writeLog("readLoop started");
    while (m_open) {
        try {
            // Limpa o buffer antes de cada leitura
            m_buffer.consume(m_buffer.size());

            // Variável para armazenar se o frame é o último fragmento
            bool is_final_frame = false;

            // Vetor para armazenar os dados do frame completo
            std::vector<uint8_t> full_payload;

            while (!is_final_frame) {
                // Executa a leitura síncrona de um fragmento
                std::size_t bytes_transferred = m_ws.read(m_buffer);

                // Verifica se o fragmento lido é o último
                is_final_frame = m_ws.is_message_done();

                // Adiciona o fragmento ao payload completo
                full_payload.insert(full_payload.end(),
                                    boost::asio::buffer_cast<const uint8_t *>(m_buffer.data()),
                                    boost::asio::buffer_cast<const uint8_t *>(m_buffer.data()) + m_buffer.size());

                // Log para indicar se o frame é parcial ou completo
                if (is_final_frame) {
                    writeLog("Received final frame fragment.");
                } else {
                    writeLog("Received partial frame fragment.");
                }

                // Limpa o buffer para a próxima leitura
                m_buffer.consume(m_buffer.size());
            }

            // Gera um UUID para a mensagem
            enqueueMessage(full_payload);
            dispatchEvent("nextMessage", "");
        } catch (const boost::beast::system_error &e) {
            if (e.code() == boost::beast::websocket::error::closed) {
                writeLog("Connection closed cleanly by peer.");

                // Obter o código de fechamento e o motivo
                const auto reason = m_ws.reason();
                const auto close_code = reason.code;
                const auto close_reason = reason.reason;

                dispatchDisconnectEvent(close_code, close_reason.c_str());
                cleanup();
                break; // Sai do loop, pois a conexão foi fechada
            }
            writeLog(("Read error: " + e.code().message()).c_str());

            // Tratar o erro como um evento de desconexão
            dispatchDisconnectEvent(1006, "Network error during read: " + e.code().message());
            cleanup();
            break; // Sai do loop em caso de erro
        }
    }
}

void WebSocketClient::close(const boost::beast::websocket::close_code code, const std::string &reason) {
    m_open = false;
    try {
        m_ws.close(code);
        dispatchDisconnectEvent(code, reason);
    } catch (std::exception const &e) {
        writeLog(("close exception: " + std::string(e.what())).c_str());
    }
}

void WebSocketClient::sendMessage(const std::string &payload) {
    writeLog("sendMessage called");
    if (m_open) {
        std::future asyncWrapper = std::async(std::launch::async, [this, payload] {
            try {
                m_ws.text(true);
                m_ws.write(boost::asio::buffer(payload));
                writeLog("Message sent successfully");
            } catch (std::exception const &e) {
                std::string error = e.what();
                writeLog(("sendMessage exception: " + error).c_str());
            }
        });
    }
}

void WebSocketClient::sendMessage(const std::vector<uint8_t> &payload) {
    writeLog("sendMessage called");
    if (m_open) {
        {
            std::lock_guard guard(m_lock_write);
            m_send_message_queue.push(payload);
        }
        m_cv.notify_one();
    }
}

void WebSocketClient::setFREContext(const FREContext ctx) {
    m_ctx = ctx;
}

void WebSocketClient::setDebugMode(const bool debugMode) {
    m_debugMode = debugMode;
}

std::optional<std::vector<uint8_t> > WebSocketClient::getNextMessage() {
    std::lock_guard guard(m_lock_messages);
    if (m_received_message_queue.empty()) {
        return std::nullopt;
    }

    // Pega a próxima mensagem da fila
    std::vector<uint8_t> message = std::move(m_received_message_queue.front());
    m_received_message_queue.pop();

    return message;
}


void WebSocketClient::enqueueMessage(const std::vector<uint8_t> &message) {
    std::lock_guard guard(m_lock_messages);
    m_received_message_queue.push(message);
}


void WebSocketClient::cleanup() {
    writeLog("cleanup called");
    m_open = false;
    m_ioc.stop();
    std::queue<std::vector<uint8_t> > empty;
    std::swap(m_send_message_queue, empty);
    std::queue<std::vector<uint8_t> > empty2;
    std::swap(m_received_message_queue, empty2);
}

void WebSocketClient::dispatchDisconnectEvent(const uint16_t &code, const std::string &reason) {
    std::lock_guard lock(m_lock_disconnect);

    if (m_disconnectDispatched)
        return;
    m_disconnectDispatched = true;

    const std::string eventData = std::to_string(code) + ";" + reason + ";0";
    writeLog(("Dispatching disconnect event: " + eventData).c_str());
    dispatchEvent("disconnected", eventData);
}

void WebSocketClient::dispatchEvent(const std::string &eventType, const std::string &eventData) {
    if (m_ctx == nullptr) return;

    FREDispatchStatusEventAsync(m_ctx, reinterpret_cast<const uint8_t *>(eventType.c_str()), reinterpret_cast<const uint8_t *>(eventData.c_str()));
}

std::string WebSocketClient::generateUUID() {
    const auto uuid = boost::uuids::random_generator()();
    return to_string(uuid);
}
