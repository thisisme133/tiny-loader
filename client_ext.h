#ifndef CLIENT_EXT_H
#define CLIENT_EXT_H

#include "packet.h"

// Extended client functionality for streaming and advanced packet handling
// Compatible with no-CRT environment

// === HELPER STRUCTURES ===

typedef struct {
    unsigned int image_size;
    unsigned int entry_point;
    unsigned char* image_data;  // Allocated via VirtualAlloc
} MapperData;

typedef struct {
    unsigned char id;
    unsigned char version;
    unsigned char x64;
    char name[64];
    char process_name[64];
} GameData;

typedef struct {
    void* socket_handle;
    char session_id[SESSION_ID_LEN + 1];
    unsigned char state;
    unsigned int hwid_result;
    unsigned char active;

    MapperData mapper;
    GameData games[16];
    unsigned char game_count;
    GameData selected_game;
} ExtendedClient;

// === NETWORK HELPERS (NO CRT) ===

#ifdef _WIN32
#include <winsock2.h>

// Send with retry (no CRT)
static inline int send_safe(void* sock, const void* data, int len) {
    int total_sent = 0;
    const char* ptr = (const char*)data;

    while(total_sent < len) {
        int sent = send((long long)sock, ptr + total_sent, len - total_sent, 0);
        if(sent <= 0) return -1;
        total_sent += sent;
    }
    return total_sent;
}

// Receive exact amount (no CRT)
static inline int recv_exact(void* sock, void* data, int len) {
    int total_recv = 0;
    char* ptr = (char*)data;

    while(total_recv < len) {
        int recvd = recv((long long)sock, ptr + total_recv, len - total_recv, 0);
        if(recvd <= 0) return -1;
        total_recv += recvd;
    }
    return total_recv;
}

#else  // Linux
#include <sys/socket.h>
#include <unistd.h>

static inline int send_safe(void* sock, const void* data, int len) {
    int total_sent = 0;
    const char* ptr = (const char*)data;

    while(total_sent < len) {
        int sent = write((long)sock, ptr + total_sent, len - total_sent);
        if(sent <= 0) return -1;
        total_sent += sent;
    }
    return total_sent;
}

static inline int recv_exact(void* sock, void* data, int len) {
    int total_recv = 0;
    char* ptr = (char*)data;

    while(total_recv < len) {
        int recvd = read((long)sock, ptr + total_recv, len - total_recv);
        if(recvd <= 0) return -1;
        total_recv += recvd;
    }
    return total_recv;
}

#endif

// === PACKET SEND/RECEIVE (HIGH LEVEL) ===

static inline int client_send_packet(ExtendedClient* client, BinaryPacket* pkt) {
    unsigned char buffer[sizeof(BinaryPacket) + 16];
    unsigned short size = packet_serialize(pkt, buffer);
    return send_safe(client->socket_handle, buffer, size);
}

static inline int client_recv_packet(ExtendedClient* client, BinaryPacket* pkt) {
    unsigned char buffer[sizeof(BinaryPacket) + 16];

    // Read header first (seq + id + len = 4 bytes)
    if(recv_exact(client->socket_handle, buffer, 4) <= 0) return 0;

    unsigned short msg_len = (buffer[2] << 8) | buffer[3];
    if(msg_len > PACKET_MESSAGE_LEN) return 0;

    // Read rest (session_id + message + crc32)
    unsigned short rest_len = SESSION_ID_LEN + msg_len + 4;  // +4 for CRC32
    if(recv_exact(client->socket_handle, buffer + 4, rest_len) <= 0) return 0;

    return packet_read(pkt, buffer, 4 + rest_len);
}

// === STREAMING FOR LARGE FILES ===

// Send large data stream (like image)
static inline int client_stream_send(ExtendedClient* client, const void* data, unsigned int size) {
    if(size == 0 || size > MAX_STREAM_SIZE) return -1;

    // Send size first (network byte order)
    unsigned int net_size = htonl_custom(size);
    if(send_safe(client->socket_handle, &net_size, sizeof(net_size)) <= 0) {
        return -1;
    }

    // Send data in chunks
    StreamState stream;
    stream_init(&stream, size);

    const unsigned char* ptr = (const unsigned char*)data;

    while(!stream_complete(&stream)) {
        unsigned int chunk = stream_next_chunk_size(&stream);
        int sent = send_safe(client->socket_handle, ptr + stream.sent, chunk);
        if(sent <= 0) return -1;
        stream_advance(&stream, sent);
    }

    return stream.sent;
}

// Receive large data stream
// Returns allocated buffer via VirtualAlloc (Windows) or mmap (Linux)
static inline void* client_stream_recv(ExtendedClient* client, unsigned int* out_size) {
    // Receive size first
    unsigned int net_size;
    if(recv_exact(client->socket_handle, &net_size, sizeof(net_size)) <= 0) {
        return 0;
    }

    unsigned int size = ntohl_custom(net_size);
    if(size == 0 || size > MAX_STREAM_SIZE) return 0;

    // Allocate buffer (no CRT)
#ifdef _WIN32
    void* buffer = VirtualAlloc(0, size, 0x3000, 0x04);  // MEM_COMMIT|MEM_RESERVE, PAGE_READWRITE
#else
    void* buffer = mmap(0, size, 3, 0x22, -1, 0);  // PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS
#endif

    if(!buffer) return 0;

    // Receive data in chunks
    StreamState stream;
    stream_init(&stream, size);

    unsigned char* ptr = (unsigned char*)buffer;

    while(!stream_complete(&stream)) {
        unsigned int chunk = stream_next_chunk_size(&stream);
        int recvd = recv_exact(client->socket_handle, ptr + stream.sent, chunk);
        if(recvd <= 0) {
#ifdef _WIN32
            VirtualFree(buffer, 0, 0x8000);  // MEM_RELEASE
#else
            munmap(buffer, size);
#endif
            return 0;
        }
        stream_advance(&stream, recvd);
    }

    *out_size = size;
    return buffer;
}

// === SECURITY REPORT PACKET ===

typedef struct {
    unsigned char debugger_detected;
    unsigned char vm_detected;
    float trust_factor;
    unsigned int uptime;
    unsigned int checksum;  // Binary checksum
} SecurityReport;

static inline void client_send_security_report(ExtendedClient* client, SecurityReport* report) {
    BinaryPacket pkt;
    packet_create_write(&pkt, PKT_ID_SECURITY_REPORT, client->session_id, report, sizeof(SecurityReport));
    client_send_packet(client, &pkt);
}

// === LOGIN PACKET ===

typedef struct {
    char username[32];
    char password[64];
    char hwid[64];
    unsigned int version;
} LoginRequest;

typedef struct {
    unsigned char success;
    unsigned int result_code;  // HWID_OK, HWID_FAIL, etc.
    char message[128];
} LoginResponse;

static inline void client_send_login(ExtendedClient* client, LoginRequest* login) {
    BinaryPacket pkt;
    packet_create_write(&pkt, PKT_ID_LOGIN_REQ, client->session_id, login, sizeof(LoginRequest));
    client_send_packet(client, &pkt);
}

// === GAME SELECTION ===

typedef struct {
    unsigned char game_id;
} GameSelectRequest;

static inline void client_send_game_select(ExtendedClient* client, unsigned char game_id) {
    GameSelectRequest req;
    req.game_id = game_id;

    BinaryPacket pkt;
    packet_create_write(&pkt, PKT_ID_GAME_SELECT, client->session_id, &req, sizeof(req));
    client_send_packet(client, &pkt);
}

// === SESSION GENERATION (no CRT) ===

static inline void generate_session_id(char* session_id) {
    const char charset[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";

    for(int i = 0; i < SESSION_ID_LEN; i++) {
        unsigned char rand_val = random_byte();
        session_id[i] = charset[rand_val % (sizeof(charset) - 1)];
    }
    session_id[SESSION_ID_LEN] = 0;
}

#endif // CLIENT_EXT_H
