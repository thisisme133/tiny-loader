#include "client/client.hpp"
#include "handlers/opcode_handler.hpp"
#include <print>
#include <thread>
#include <chrono>

using namespace std::chrono_literals;

// Handlers côté client
namespace {

void handle_data(core::Session& session, core::Packet& packet) {
    std::println("[Client Handler] DATA received ({} bytes)", packet.size());

    // Afficher le contenu si c'est du texte
    if (packet.size() > 0) {
        auto data = packet.read_bytes(packet.size());
        std::string msg(data.begin(), data.end());
        std::println("[Server says] {}", msg);
    }
}

void handle_heartbeat(core::Session& session, core::Packet& packet) {
    std::println("[Client Handler] HEARTBEAT from server");
}

void register_client_handlers() {
    auto& handler = handlers::OpcodeHandler::instance();

    handler.register_handler(core::Opcode::DATA, handle_data);
    handler.register_handler(core::Opcode::HEARTBEAT, handle_heartbeat);

    std::println("[Client] Handlers registered");
}

}

int main() {
    try {
        std::println("=== TinyLoader Client (C++23) ===\n");

        register_client_handlers();

        client::Client cli;

        if (!cli.connect("127.0.0.1", 8888)) {
            std::println("[Error] Failed to connect to server");
            return 1;
        }

        // Envoyer HELLO
        core::Packet hello(core::Opcode::HELLO);
        std::string msg = "Hello from client!";
        hello.write(std::span(reinterpret_cast<const uint8_t*>(msg.data()), msg.size()));

        if (cli.send(hello)) {
            std::println("[Client] HELLO sent");
        }

        // Envoyer des heartbeats périodiques
        int heartbeat_count = 0;
        while (cli.is_connected() && heartbeat_count < 5) {
            std::this_thread::sleep_for(3s);

            core::Packet hb(core::Opcode::HEARTBEAT);
            if (cli.send(hb)) {
                std::println("[Client] Heartbeat #{} sent", ++heartbeat_count);
            }
        }

        std::println("\n[Client] Closing connection...");
        cli.disconnect();

    } catch (const std::exception& e) {
        std::println("[Fatal Error] {}", e.what());
        return 1;
    }

    return 0;
}
