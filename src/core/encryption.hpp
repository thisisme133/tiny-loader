#pragma once

#include <array>
#include <span>
#include <random>
#include <cstdint>

namespace core {

// Clé de session pour encryption XOR (simple mais efficace pour le projet)
class SessionKey {
public:
    static constexpr size_t KEY_SIZE = 32;

    SessionKey() {
        generate();
    }

    // Génère une clé aléatoire
    void generate() {
        std::random_device rd;
        std::mt19937_64 gen(rd());
        std::uniform_int_distribution<uint8_t> dist(0, 255);

        for (auto& byte : key_) {
            byte = dist(gen);
        }
    }

    // Encrypt/Decrypt XOR (symétrique)
    void crypt(std::span<uint8_t> data) const noexcept {
        for (size_t i = 0; i < data.size(); ++i) {
            data[i] ^= key_[i % KEY_SIZE];
        }
    }

    // Accesseurs
    [[nodiscard]] auto data() const noexcept -> std::span<const uint8_t, KEY_SIZE> {
        return key_;
    }

    void set_key(std::span<const uint8_t, KEY_SIZE> new_key) noexcept {
        std::copy(new_key.begin(), new_key.end(), key_.begin());
    }

private:
    std::array<uint8_t, KEY_SIZE> key_{};
};

} // namespace core
