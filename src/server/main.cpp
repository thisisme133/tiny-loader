#include "server/server.hpp"
#include "handlers/opcode_handler.hpp"
#include <print>
#include <csignal>

// Handlers d'exemple
namespace {

void handle_hello(core::Session& session, core::Packet& packet) {
    std::println("[Handler] HELLO received");

    // Répondre
    core::Packet response(core::Opcode::DATA);
    std::string msg = "Welcome to the server!";
    response.write(std::span(reinterpret_cast<const uint8_t*>(msg.data()), msg.size()));

    session.send(response);
}

void handle_data(core::Session& session, core::Packet& packet) {
    std::println("[Handler] DATA received ({} bytes)", packet.size());

    // Echo back
    core::Packet response(core::Opcode::DATA);
    response.write(packet.data());
    session.send(response);
}

void handle_heartbeat(core::Session& session, core::Packet& packet) {
    std::println("[Handler] HEARTBEAT received");

    // Optionnel: répondre au heartbeat
    core::Packet response(core::Opcode::HEARTBEAT);
    session.send(response);
}

// Enregistrer les handlers
void register_handlers() {
    auto& handler = handlers::OpcodeHandler::instance();

    handler.register_handler(core::Opcode::HELLO, handle_hello);
    handler.register_handler(core::Opcode::DATA, handle_data);
    handler.register_handler(core::Opcode::HEARTBEAT, handle_heartbeat);

    std::println("[Server] Handlers registered");
}

}

int main() {
    try {
        std::println("=== TinyLoader Server (C++23) ===\n");

        register_handlers();

        server::Server srv(8888);
        srv.start();

    } catch (const std::exception& e) {
        std::println("[Fatal Error] {}", e.what());
        return 1;
    }

    return 0;
}
