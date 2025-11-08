#pragma once

#include <cstdint>
#include <vector>
#include <span>
#include <concepts>
#include <type_traits>

namespace core {

// Opcodes pour les différents types de paquets
enum class Opcode : uint16_t {
    HELLO = 0x0001,
    AUTH_REQUEST = 0x0002,
    AUTH_RESPONSE = 0x0003,
    HEARTBEAT = 0x0004,
    DATA = 0x0005,
    DISCONNECT = 0x0006,
    // Ajoutez vos opcodes ici
};

// Structure de paquet binaire
// Format: [opcode:2][size:4][data:variable]
class Packet {
public:
    Packet() = default;
    explicit Packet(Opcode opcode) : opcode_(opcode) {}

    // Accesseurs
    [[nodiscard]] auto opcode() const noexcept -> Opcode { return opcode_; }
    [[nodiscard]] auto size() const noexcept -> uint32_t { return static_cast<uint32_t>(data_.size()); }
    [[nodiscard]] auto data() const noexcept -> std::span<const uint8_t> { return data_; }
    [[nodiscard]] auto data() noexcept -> std::span<uint8_t> { return data_; }

    // Écriture de données typées
    template<typename T>
    requires std::is_trivially_copyable_v<T>
    void write(const T& value) {
        const auto* bytes = reinterpret_cast<const uint8_t*>(&value);
        data_.insert(data_.end(), bytes, bytes + sizeof(T));
    }

    // Écriture de buffer
    void write(std::span<const uint8_t> buffer) {
        data_.insert(data_.end(), buffer.begin(), buffer.end());
    }

    // Lecture de données typées
    template<typename T>
    requires std::is_trivially_copyable_v<T>
    [[nodiscard]] auto read() -> T {
        if (read_pos_ + sizeof(T) > data_.size()) {
            throw std::out_of_range("Packet read overflow");
        }

        T value;
        std::memcpy(&value, data_.data() + read_pos_, sizeof(T));
        read_pos_ += sizeof(T);
        return value;
    }

    // Lecture de buffer
    [[nodiscard]] auto read_bytes(size_t count) -> std::vector<uint8_t> {
        if (read_pos_ + count > data_.size()) {
            throw std::out_of_range("Packet read overflow");
        }

        std::vector<uint8_t> result(data_.begin() + read_pos_, data_.begin() + read_pos_ + count);
        read_pos_ += count;
        return result;
    }

    // Reset position de lecture
    void reset_read_pos() noexcept { read_pos_ = 0; }

    // Sérialisation complète (opcode + size + data)
    [[nodiscard]] auto serialize() const -> std::vector<uint8_t> {
        std::vector<uint8_t> buffer;
        buffer.reserve(sizeof(Opcode) + sizeof(uint32_t) + data_.size());

        // Opcode (2 bytes, big-endian)
        uint16_t op = static_cast<uint16_t>(opcode_);
        buffer.push_back((op >> 8) & 0xFF);
        buffer.push_back(op & 0xFF);

        // Size (4 bytes, big-endian)
        uint32_t sz = static_cast<uint32_t>(data_.size());
        buffer.push_back((sz >> 24) & 0xFF);
        buffer.push_back((sz >> 16) & 0xFF);
        buffer.push_back((sz >> 8) & 0xFF);
        buffer.push_back(sz & 0xFF);

        // Data
        buffer.insert(buffer.end(), data_.begin(), data_.end());

        return buffer;
    }

    // Désérialisation
    static auto deserialize(std::span<const uint8_t> buffer) -> Packet {
        if (buffer.size() < 6) {
            throw std::invalid_argument("Buffer too small for packet header");
        }

        // Read opcode (big-endian)
        uint16_t op = (static_cast<uint16_t>(buffer[0]) << 8) | buffer[1];

        // Read size (big-endian)
        uint32_t size = (static_cast<uint32_t>(buffer[2]) << 24) |
                       (static_cast<uint32_t>(buffer[3]) << 16) |
                       (static_cast<uint32_t>(buffer[4]) << 8) |
                       static_cast<uint32_t>(buffer[5]);

        if (buffer.size() < 6 + size) {
            throw std::invalid_argument("Buffer too small for packet data");
        }

        Packet packet(static_cast<Opcode>(op));
        packet.data_.assign(buffer.begin() + 6, buffer.begin() + 6 + size);

        return packet;
    }

    // Clear packet
    void clear() noexcept {
        data_.clear();
        read_pos_ = 0;
    }

private:
    Opcode opcode_{};
    std::vector<uint8_t> data_;
    size_t read_pos_{0};
};

} // namespace core
