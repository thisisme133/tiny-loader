#include "session.hpp"
#include <iostream>
#include <thread>
#include <vector>

#ifdef _WIN32
#pragma comment(lib, "ws2_32.lib")
#endif

void handle_client(socket_t client_sock) {
    Session session(client_sock);

    std::cout << "[+] Client connected\n";

    // Envoyer la clé de session
    if (!session.send_key()) {
        std::cout << "[!] Failed to send key\n";
        return;
    }

    std::cout << "[+] Session key sent\n";

    // Recevoir et traiter les packets
    while (true) {
        Packet pkt;
        if (!session.receive(pkt)) {
            std::cout << "[-] Client disconnected\n";
            break;
        }

        // Lire l'opcode
        uint8_t opcode;
        pkt >> opcode;

        std::cout << "[*] Received opcode: " << (int)opcode << "\n";

        // Traiter selon l'opcode
        switch (opcode) {
            case 1: { // HELLO
                std::string msg;
                pkt >> msg;
                std::cout << "[HELLO] " << msg << "\n";

                // Répondre
                Packet response;
                response << (uint8_t)2; // opcode RESPONSE
                response << std::string("Hello from server!");
                session.send(response);
                break;
            }

            case 3: { // DATA
                uint32_t value;
                pkt >> value;
                std::cout << "[DATA] Received: " << value << "\n";

                // Echo back
                Packet response;
                response << (uint8_t)3;
                response << (uint32_t)(value * 2);
                session.send(response);
                break;
            }

            case 99: // DISCONNECT
                std::cout << "[-] Client requested disconnect\n";
                return;

            default:
                std::cout << "[?] Unknown opcode: " << (int)opcode << "\n";
        }
    }
}

int main() {
    std::cout << "=== TinyLoader Server ===\n\n";

#ifdef _WIN32
    WSADATA wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);
#endif

    socket_t server_sock = socket(AF_INET, SOCK_STREAM, 0);

    int opt = 1;
    setsockopt(server_sock, SOL_SOCKET, SO_REUSEADDR, (char*)&opt, sizeof(opt));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(8888);

    bind(server_sock, (sockaddr*)&addr, sizeof(addr));
    listen(server_sock, 10);

    std::cout << "[Server] Listening on port 8888...\n\n";

    std::vector<std::thread> threads;

    while (true) {
        socket_t client = accept(server_sock, nullptr, nullptr);

        // Nouveau thread pour chaque client
        threads.emplace_back(handle_client, client);
        threads.back().detach();
    }

#ifdef _WIN32
    WSACleanup();
#endif

    return 0;
}
