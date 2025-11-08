// Example usage of the packet system (no CRT, pure C)
// This demonstrates how to use the binary packet system with 2-key encryption

#include "packet.h"
#include "client_ext.h"

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#endif

// === EXAMPLE 1: Basic packet send/receive ===

void example_basic_packet() {
    // Create a packet
    BinaryPacket pkt;
    char session[] = "ABC1234567";
    char message[] = "Hello Server!";

    packet_create_write(&pkt, PKT_ID_MESSAGE, session, message, sizeof(message) - 1);

    // Serialize for transmission
    unsigned char buffer[512];
    unsigned short size = packet_serialize(&pkt, buffer);

    // ... send buffer via socket ...
    // send(sock, buffer, size, 0);

    // On receive side:
    BinaryPacket received;
    if(packet_read(&received, buffer, size)) {
        // Packet was decrypted and parsed successfully
        // received.message contains "Hello Server!"
        // received.session_id contains "ABC1234567"
    }
}

// === EXAMPLE 2: Security report with extended client ===

void example_security_report(ExtendedClient* client) {
    SecurityReport report;
    report.debugger_detected = 0;
    report.vm_detected = 0;
    report.trust_factor = 95.5f;
    report.uptime = 3600;  // 1 hour
    report.checksum = 0x12345678;

    client_send_security_report(client, &report);
}

// === EXAMPLE 3: Login flow ===

void example_login(ExtendedClient* client) {
    // Send login request
    LoginRequest login;

    // Copy username (no strcpy, no CRT)
    const char* user = "testuser";
    int i = 0;
    while(user[i] && i < 31) {
        login.username[i] = user[i];
        i++;
    }
    login.username[i] = 0;

    // Copy password
    const char* pass = "password123";
    i = 0;
    while(pass[i] && i < 63) {
        login.password[i] = pass[i];
        i++;
    }
    login.password[i] = 0;

    // Generate HWID (simplified)
    const char* hwid = "HWID-1234-5678-90AB";
    i = 0;
    while(hwid[i] && i < 63) {
        login.hwid[i] = hwid[i];
        i++;
    }
    login.hwid[i] = 0;

    login.version = 1;

    client_send_login(client, &login);

    // Receive response
    BinaryPacket response;
    if(client_recv_packet(client, &response)) {
        if(response.id == PKT_ID_LOGIN_RESP) {
            LoginResponse* resp = (LoginResponse*)response.message;

            if(resp->success) {
                client->state = CLIENT_LOGGED_IN;
                client->hwid_result = resp->result_code;
            }
        }
    }
}

// === EXAMPLE 4: Streaming large file (image) ===

void example_stream_image(ExtendedClient* client) {
    // Simulate large image data (allocated via VirtualAlloc)
#ifdef _WIN32
    unsigned int image_size = 5 * 1024 * 1024;  // 5MB
    void* image_data = VirtualAlloc(0, image_size, 0x3000, 0x04);
#else
    unsigned int image_size = 5 * 1024 * 1024;
    void* image_data = mmap(0, image_size, 3, 0x22, -1, 0);
#endif

    if(!image_data) return;

    // Fill with some data
    unsigned char* ptr = (unsigned char*)image_data;
    for(unsigned int i = 0; i < image_size; i++) {
        ptr[i] = (unsigned char)(i & 0xFF);
    }

    // Send image data
    int sent = client_stream_send(client, image_data, image_size);

    // Cleanup
#ifdef _WIN32
    VirtualFree(image_data, 0, 0x8000);
#else
    munmap(image_data, image_size);
#endif
}

// === EXAMPLE 5: Receiving large file ===

void example_receive_image(ExtendedClient* client) {
    unsigned int size;
    void* data = client_stream_recv(client, &size);

    if(data) {
        // Process image data
        client->mapper.image_data = (unsigned char*)data;
        client->mapper.image_size = size;

        // Don't forget to free when done:
        // VirtualFree(data, 0, 0x8000) on Windows
        // munmap(data, size) on Linux
    }
}

// === EXAMPLE 6: Game selection ===

void example_game_select(ExtendedClient* client) {
    // Assume we received game list and want to select game #2
    unsigned char game_id = 2;

    client_send_game_select(client, game_id);

    // Server will respond with game data or image
}

// === EXAMPLE 7: Complete client flow ===

#ifdef _WIN32
void example_complete_flow() {
    WSADATA wsa;
    WSAStartup(0x0202, &wsa);

    SOCKET sock = socket(AF_INET, SOCK_STREAM, 0);

    struct sockaddr_in srv;
    srv.sin_family = AF_INET;
    srv.sin_port = ((PORT & 0xFF) << 8) | ((PORT >> 8) & 0xFF);
    srv.sin_addr.s_addr = 0x0100007F;  // 127.0.0.1

    if(connect(sock, (struct sockaddr*)&srv, sizeof(srv)) != 0) {
        closesocket(sock);
        return;
    }

    ExtendedClient client = {0};
    client.socket_handle = (void*)(long long)sock;
    client.active = 1;
    client.state = CLIENT_CONNECTING;

    generate_session_id(client.session_id);

    // 1. Send HELLO
    BinaryPacket hello;
    char msg[] = "Client connected";
    packet_create_write(&hello, PKT_ID_MESSAGE, client.session_id, msg, sizeof(msg) - 1);
    client_send_packet(&client, &hello);

    // 2. Login
    example_login(&client);

    // 3. Send security report periodically
    SecurityReport report = {0, 0, 100.0f, 0, 0x12345678};
    client_send_security_report(&client, &report);

    // 4. Select game
    client_send_game_select(&client, 1);

    // 5. Receive image
    unsigned int img_size;
    void* img_data = client_stream_recv(&client, &img_size);
    if(img_data) {
        // Process image...
        VirtualFree(img_data, 0, 0x8000);
    }

    closesocket(sock);
    WSACleanup();
}
#endif

// === EXAMPLE 8: Encryption test ===

void example_encryption_test() {
    unsigned char data[100] = "This is a secret message!";
    unsigned short len = 25;

    // Encrypt
    encrypt_message(data, &len);
    // Now data = [k1][encrypted][k2], len = 27

    // Decrypt
    decrypt_message(data, &len);
    // Now data = "This is a secret message!", len = 25
}
