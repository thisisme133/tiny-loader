#pragma once

#include "core/session.hpp"
#include "handlers/opcode_handler.hpp"
#include <thread>
#include <vector>
#include <memory>
#include <atomic>
#include <mutex>
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

namespace server {

class Server {
public:
    explicit Server(uint16_t port) : port_(port) {
#ifdef _WIN32
        WSADATA wsa;
        if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
            throw std::runtime_error("WSAStartup failed");
        }
#endif
    }

    ~Server() {
        stop();
#ifdef _WIN32
        WSACleanup();
#endif
    }

    void start() {
        socket_ = ::socket(AF_INET, SOCK_STREAM, 0);
        if (socket_ < 0) {
            throw std::runtime_error("Failed to create socket");
        }

        // Réutilisation d'adresse
        int opt = 1;
        setsockopt(socket_, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&opt), sizeof(opt));

        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = INADDR_ANY;
        addr.sin_port = htons(port_);

        if (bind(socket_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
            throw std::runtime_error("Bind failed");
        }

        if (listen(socket_, 10) < 0) {
            throw std::runtime_error("Listen failed");
        }

        running_.store(true);
        std::println("[Server] Listening on port {}", port_);

        accept_loop();
    }

    void stop() {
        if (running_.exchange(false)) {
#ifdef _WIN32
            closesocket(socket_);
#else
            ::close(socket_);
#endif

            // Attendre tous les threads
            for (auto& thread : client_threads_) {
                if (thread.joinable()) {
                    thread.join();
                }
            }
            client_threads_.clear();
        }
    }

private:
    uint16_t port_;
    socket_t socket_{};
    std::atomic<bool> running_{false};
    std::vector<std::jthread> client_threads_;
    std::mutex threads_mutex_;

    void accept_loop() {
        while (running_.load()) {
            sockaddr_in client_addr{};
            socklen_t addr_len = sizeof(client_addr);

            auto client_socket = accept(socket_, reinterpret_cast<sockaddr*>(&client_addr), &addr_len);

            if (client_socket < 0) {
                if (running_.load()) {
                    std::println("[Error] Accept failed");
                }
                continue;
            }

            std::println("[Server] Client connected from {}:{}",
                       inet_ntoa(client_addr.sin_addr),
                       ntohs(client_addr.sin_port));

            // Créer thread pour ce client
            {
                std::lock_guard lock(threads_mutex_);
                client_threads_.emplace_back([this, client_socket]() {
                    handle_client(client_socket);
                });
            }
        }
    }

    void handle_client(socket_t client_socket) {
        auto session = std::make_unique<core::Session>(client_socket);

        // Envoyer la clé de session
        core::Packet key_packet(core::Opcode::AUTH_RESPONSE);
        auto key_data = session->key().data();
        key_packet.write(std::span(key_data.begin(), key_data.end()));

        if (!session->send(key_packet)) {
            std::println("[Error] Failed to send session key");
            return;
        }

        // Activer encryption après envoi de la clé
        session->set_encrypted(true);

        std::println("[Session] Client session started");

        // Boucle de réception
        while (session->is_active()) {
            auto packet_opt = session->receive();

            if (!packet_opt) {
                break; // Connexion fermée ou erreur
            }

            auto& packet = *packet_opt;

            // Traiter via le système de handlers
            handlers::OpcodeHandler::instance().handle(*session, packet);

            // Check timeout (30 secondes)
            if (session->is_timeout(std::chrono::seconds(30))) {
                std::println("[Session] Timeout - closing");
                break;
            }
        }

        std::println("[Session] Client disconnected");
    }
};

} // namespace server
