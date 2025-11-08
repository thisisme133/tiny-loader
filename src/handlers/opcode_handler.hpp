#pragma once

#include "core/packet.hpp"
#include "core/session.hpp"
#include <functional>
#include <unordered_map>
#include <memory>
#include <print>

namespace handlers {

// Type pour les handlers d'opcodes
using PacketHandler = std::function<void(core::Session&, core::Packet&)>;

// Gestionnaire central des opcodes
class OpcodeHandler {
public:
    static auto instance() -> OpcodeHandler& {
        static OpcodeHandler instance;
        return instance;
    }

    // Enregistre un handler pour un opcode
    void register_handler(core::Opcode opcode, PacketHandler handler) {
        handlers_[opcode] = std::move(handler);
    }

    // Traite un paquet
    void handle(core::Session& session, core::Packet& packet) {
        auto it = handlers_.find(packet.opcode());

        if (it != handlers_.end()) {
            try {
                it->second(session, packet);
            } catch (const std::exception& e) {
                std::println("[Error] Handler exception for opcode {:#x}: {}",
                           static_cast<uint16_t>(packet.opcode()), e.what());
            }
        } else {
            std::println("[Warning] No handler for opcode {:#x}",
                       static_cast<uint16_t>(packet.opcode()));
        }
    }

    // Vérifie si un handler existe
    [[nodiscard]] bool has_handler(core::Opcode opcode) const {
        return handlers_.contains(opcode);
    }

private:
    OpcodeHandler() = default;
    std::unordered_map<core::Opcode, PacketHandler> handlers_;
};

// Macro helper pour enregistrer des handlers
#define REGISTER_OPCODE_HANDLER(opcode, handler) \
    static struct Opcode##handler##Registrar { \
        Opcode##handler##Registrar() { \
            handlers::OpcodeHandler::instance().register_handler(opcode, handler); \
        } \
    } opcode##handler##_registrar;

} // namespace handlers
