#pragma once
#include <vector>
#include <cstring>
#include <stdexcept>

// Packet style SFML - ultra simple !
class Packet {
public:
    // Écrire des données
    template<typename T>
    Packet& operator<<(const T& data) {
        const char* bytes = reinterpret_cast<const char*>(&data);
        buffer_.insert(buffer_.end(), bytes, bytes + sizeof(T));
        return *this;
    }

    // Lire des données
    template<typename T>
    Packet& operator>>(T& data) {
        if (pos_ + sizeof(T) > buffer_.size())
            throw std::runtime_error("Packet read overflow");

        std::memcpy(&data, buffer_.data() + pos_, sizeof(T));
        pos_ += sizeof(T);
        return *this;
    }

    // Écrire un string
    Packet& operator<<(const std::string& str) {
        uint32_t size = str.size();
        *this << size;
        buffer_.insert(buffer_.end(), str.begin(), str.end());
        return *this;
    }

    // Lire un string
    Packet& operator>>(std::string& str) {
        uint32_t size;
        *this >> size;
        if (pos_ + size > buffer_.size())
            throw std::runtime_error("Packet read overflow");

        str.assign(buffer_.begin() + pos_, buffer_.begin() + pos_ + size);
        pos_ += size;
        return *this;
    }

    // Accès direct au buffer
    const std::vector<char>& data() const { return buffer_; }
    std::vector<char>& data() { return buffer_; }

    // Taille
    size_t size() const { return buffer_.size(); }

    // Clear
    void clear() { buffer_.clear(); pos_ = 0; }

    // Reset lecture
    void reset() { pos_ = 0; }

    // Encryption XOR simple
    void encrypt(const std::vector<char>& key) {
        for (size_t i = 0; i < buffer_.size(); i++) {
            buffer_[i] ^= key[i % key.size()];
        }
    }

private:
    std::vector<char> buffer_;
    size_t pos_ = 0;
};
