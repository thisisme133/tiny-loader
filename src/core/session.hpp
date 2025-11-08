#pragma once

#include "core/packet.hpp"
#include "core/encryption.hpp"
#include <memory>
#include <atomic>
#include <chrono>

#ifdef _WIN32
#include <winsock2.h>
using socket_t = SOCKET;
#else
using socket_t = int;
#endif

namespace core {

class Session {
public:
    explicit Session(socket_t socket)
        : socket_(socket)
        , active_(true)
        , last_activity_(std::chrono::steady_clock::now())
    {
        key_.generate();
    }

    ~Session() {
        close();
    }

    // Non-copyable, movable
    Session(const Session&) = delete;
    Session& operator=(const Session&) = delete;
    Session(Session&&) noexcept = default;
    Session& operator=(Session&&) noexcept = default;

    // Envoie un paquet (encrypt automatiquement)
    [[nodiscard]] bool send(Packet& packet) {
        auto data = packet.serialize();

        // Encrypt payload (skip header: opcode + size)
        if (data.size() > 6 && encrypted_) {
            std::span<uint8_t> payload(data.data() + 6, data.size() - 6);
            key_.crypt(payload);
        }

        return send_raw(data);
    }

    // Reçoit un paquet (decrypt automatiquement)
    [[nodiscard]] auto receive() -> std::optional<Packet> {
        // Lire header (6 bytes)
        std::array<uint8_t, 6> header{};
        if (!recv_exact(header)) {
            return std::nullopt;
        }

        // Parse size
        uint32_t size = (static_cast<uint32_t>(header[2]) << 24) |
                       (static_cast<uint32_t>(header[3]) << 16) |
                       (static_cast<uint32_t>(header[4]) << 8) |
                       static_cast<uint32_t>(header[5]);

        if (size > 1024 * 1024) { // Max 1MB
            return std::nullopt;
        }

        // Lire payload
        std::vector<uint8_t> payload(size);
        if (size > 0 && !recv_exact(payload)) {
            return std::nullopt;
        }

        // Decrypt payload
        if (encrypted_ && size > 0) {
            key_.crypt(payload);
        }

        // Reconstruct packet
        std::vector<uint8_t> full_packet;
        full_packet.reserve(6 + size);
        full_packet.insert(full_packet.end(), header.begin(), header.end());
        full_packet.insert(full_packet.end(), payload.begin(), payload.end());

        update_activity();
        return Packet::deserialize(full_packet);
    }

    // Accesseurs
    [[nodiscard]] auto socket() const noexcept -> socket_t { return socket_; }
    [[nodiscard]] bool is_active() const noexcept { return active_.load(); }
    [[nodiscard]] auto key() const noexcept -> const SessionKey& { return key_; }

    void set_encrypted(bool encrypted) noexcept { encrypted_ = encrypted; }
    void set_key(const SessionKey& key) noexcept { key_ = key; }

    // Ferme la session
    void close() noexcept {
        if (active_.exchange(false)) {
#ifdef _WIN32
            closesocket(socket_);
#else
            ::close(socket_);
#endif
        }
    }

    // Vérifie timeout (pour heartbeat)
    [[nodiscard]] bool is_timeout(std::chrono::seconds timeout) const noexcept {
        auto now = std::chrono::steady_clock::now();
        return (now - last_activity_) > timeout;
    }

private:
    socket_t socket_;
    std::atomic<bool> active_;
    SessionKey key_;
    bool encrypted_{false};
    std::chrono::steady_clock::time_point last_activity_;

    void update_activity() noexcept {
        last_activity_ = std::chrono::steady_clock::now();
    }

    bool send_raw(std::span<const uint8_t> data) {
        size_t total_sent = 0;
        while (total_sent < data.size()) {
            auto sent = ::send(socket_,
                             reinterpret_cast<const char*>(data.data() + total_sent),
                             static_cast<int>(data.size() - total_sent),
                             0);

            if (sent <= 0) {
                active_.store(false);
                return false;
            }

            total_sent += sent;
        }
        update_activity();
        return true;
    }

    template<typename Container>
    bool recv_exact(Container& buffer) {
        size_t total_recv = 0;
        const size_t size = buffer.size();

        while (total_recv < size) {
            auto received = ::recv(socket_,
                                 reinterpret_cast<char*>(buffer.data() + total_recv),
                                 static_cast<int>(size - total_recv),
                                 0);

            if (received <= 0) {
                active_.store(false);
                return false;
            }

            total_recv += received;
        }

        return true;
    }
};

} // namespace core
