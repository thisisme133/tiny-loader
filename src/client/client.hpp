#pragma once

#include "core/session.hpp"
#include "handlers/opcode_handler.hpp"
#include <thread>
#include <atomic>
#include <print>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define _WINSOCKAPI_
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#endif

namespace client {

class Client {
public:
    Client() {
#ifdef _WIN32
        WSADATA wsa;
        if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
            throw std::runtime_error("WSAStartup failed");
        }
#endif
    }

    ~Client() {
        disconnect();
#ifdef _WIN32
        WSACleanup();
#endif
    }

    bool connect(const std::string& host, uint16_t port) {
        socket_ = ::socket(AF_INET, SOCK_STREAM, 0);
        if (socket_ < 0) {
            std::println("[Error] Failed to create socket");
            return false;
        }

        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);

#ifdef _WIN32
        addr.sin_addr.s_addr = inet_addr(host.c_str());
#else
        inet_pton(AF_INET, host.c_str(), &addr.sin_addr);
#endif

        if (::connect(socket_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
            std::println("[Error] Connection failed");
#ifdef _WIN32
            closesocket(socket_);
#else
            ::close(socket_);
#endif
            return false;
        }

        session_ = std::make_unique<core::Session>(socket_);
        connected_.store(true);

        std::println("[Client] Connected to {}:{}", host, port);

        // Recevoir la clé de session du serveur
        auto key_packet_opt = session_->receive();
        if (!key_packet_opt) {
            std::println("[Error] Failed to receive session key");
            disconnect();
            return false;
        }

        auto& key_packet = *key_packet_opt;
        if (key_packet.opcode() == core::Opcode::AUTH_RESPONSE) {
            auto key_bytes = key_packet.read_bytes(core::SessionKey::KEY_SIZE);
            core::SessionKey new_key;
            new_key.set_key(std::span<const uint8_t, core::SessionKey::KEY_SIZE>(
                key_bytes.data(), core::SessionKey::KEY_SIZE));
            session_->set_key(new_key);
            session_->set_encrypted(true);

            std::println("[Client] Session key received and encryption enabled");
        }

        // Démarrer thread de réception
        receive_thread_ = std::jthread([this](std::stop_token stoken) {
            receive_loop(stoken);
        });

        return true;
    }

    void disconnect() {
        if (connected_.exchange(false)) {
            if (receive_thread_.joinable()) {
                receive_thread_.request_stop();
                receive_thread_.join();
            }

            session_.reset();

#ifdef _WIN32
            closesocket(socket_);
#else
            ::close(socket_);
#endif

            std::println("[Client] Disconnected");
        }
    }

    bool send(core::Packet& packet) {
        if (!session_ || !connected_.load()) {
            return false;
        }
        return session_->send(packet);
    }

    [[nodiscard]] bool is_connected() const noexcept {
        return connected_.load();
    }

private:
    socket_t socket_{};
    std::unique_ptr<core::Session> session_;
    std::atomic<bool> connected_{false};
    std::jthread receive_thread_;

    void receive_loop(std::stop_token stoken) {
        while (!stoken.stop_requested() && session_ && session_->is_active()) {
            auto packet_opt = session_->receive();

            if (!packet_opt) {
                break;
            }

            auto& packet = *packet_opt;

            // Traiter via handlers
            handlers::OpcodeHandler::instance().handle(*session_, packet);
        }

        connected_.store(false);
        std::println("[Client] Receive loop ended");
    }
};

} // namespace client
