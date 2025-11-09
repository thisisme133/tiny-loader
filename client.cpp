#include "session.hpp"
#include <iostream>
#include <thread>
#include <chrono>

#ifdef _WIN32
#pragma comment(lib, "ws2_32.lib")
#endif

using namespace std::chrono_literals;

int main() {
    std::cout << "=== TinyLoader Client ===\n\n";

#ifdef _WIN32
    WSADATA wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);
#endif

    socket_t sock = socket(AF_INET, SOCK_STREAM, 0);

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(8888);
#ifdef _WIN32
    addr.sin_addr.s_addr = inet_addr("127.0.0.1");
#else
    inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);
#endif

    std::cout << "[*] Connecting to server...\n";
    if (connect(sock, (sockaddr*)&addr, sizeof(addr)) < 0) {
        std::cout << "[!] Connection failed\n";
        return 1;
    }

    Session session(sock);
    std::cout << "[+] Connected!\n";

    // Recevoir la clé du serveur
    if (!session.receive_key()) {
        std::cout << "[!] Failed to receive key\n";
        return 1;
    }
    std::cout << "[+] Session key received\n\n";

    // === EXEMPLE 1: Envoyer HELLO ===
    {
        Packet pkt;
        pkt << (uint8_t)1; // opcode HELLO
        pkt << std::string("Hello from client!");

        session.send(pkt);
        std::cout << "[->] Sent HELLO\n";

        // Attendre réponse
        Packet response;
        session.receive(response);

        uint8_t opcode;
        std::string msg;
        response >> opcode >> msg;

        std::cout << "[<-] Server response: " << msg << "\n\n";
    }

    std::this_thread::sleep_for(1s);

    // === EXEMPLE 2: Envoyer DATA ===
    {
        Packet pkt;
        pkt << (uint8_t)3; // opcode DATA
        pkt << (uint32_t)42;

        session.send(pkt);
        std::cout << "[->] Sent DATA: 42\n";

        // Attendre réponse
        Packet response;
        session.receive(response);

        uint8_t opcode;
        uint32_t value;
        response >> opcode >> value;

        std::cout << "[<-] Server response: " << value << "\n\n";
    }

    std::this_thread::sleep_for(1s);

    // === EXEMPLE 3: Packet complexe ===
    {
        Packet pkt;
        pkt << (uint8_t)3;        // opcode
        pkt << (uint32_t)123;     // un nombre
        pkt << (float)3.14f;      // un float
        pkt << (uint16_t)999;     // un petit nombre

        std::cout << "[*] Packet size: " << pkt.size() << " bytes\n";
        std::cout << "[*] Contains: uint32, float, uint16\n\n";
    }

    // Disconnect
    std::cout << "[*] Disconnecting...\n";
    Packet bye;
    bye << (uint8_t)99; // DISCONNECT
    session.send(bye);

#ifdef _WIN32
    WSACleanup();
#endif

    return 0;
}
