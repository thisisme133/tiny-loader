#ifndef PACKET_H
#define PACKET_H

// Packet types
#define PKT_TYPE_WRITE 0
#define PKT_TYPE_READ 1

// Packet IDs (étendu)
#define PKT_ID_MESSAGE 0x00
#define PKT_ID_HWID 0x01
#define PKT_ID_HWID_RESP 0x02
#define PKT_ID_SESSION 0x03
#define PKT_ID_LOGIN_REQ 0x04
#define PKT_ID_LOGIN_RESP 0x05
#define PKT_ID_SECURITY_REPORT 0x06
#define PKT_ID_BAN 0x07
#define PKT_ID_GAME_SELECT 0x08
#define PKT_ID_IMAGE 0x09
#define PKT_ID_FUNCTION_REQUEST 0x0A
#define PKT_ID_FUNCTION_BYTES 0x0B

// HWID results
#define HWID_FAIL 5671
#define HWID_BLACKLISTED 4567
#define VERSION_MISMATCH 5472
#define HWID_OK 3247

// Client states
#define CLIENT_CONNECTING 0
#define CLIENT_IDLE 1
#define CLIENT_LOGGED_IN 2
#define CLIENT_IMPORTS_READY 3
#define CLIENT_WAITING 4
#define CLIENT_IMAGE_READY 5
#define CLIENT_INJECTED 6
#define CLIENT_BLACKLISTED 7

#define SESSION_ID_LEN 10
#define PACKET_MESSAGE_LEN 512
#define MAX_STREAM_SIZE (100 * 1024 * 1024)  // 100MB
#define CHUNK_SIZE 4096

// Binary packet structure (no JSON, just raw bytes)
typedef struct {
    unsigned char seq;
    unsigned char id;
    unsigned short message_len;
    char session_id[SESSION_ID_LEN + 1];
    unsigned char message[PACKET_MESSAGE_LEN];
} BinaryPacket;

// === ENCRYPTION WITH 2 KEYS (like enc.h) ===

// Generate random byte using RDRAND if available, fallback to RDTSC
static inline unsigned char random_byte() {
    unsigned int val;

#ifdef _MSC_VER
    // Try RDRAND first
    if (_rdrand32_step(&val)) {
        return (unsigned char)(val & 0xFF);
    }
#endif

    // Fallback: use RDTSC
    unsigned long long tsc = __rdtsc();
    return (unsigned char)((tsc ^ (tsc >> 8) ^ (tsc >> 16) ^ (tsc >> 24)) & 0xFF);
}

// Encrypt message with 2-key XOR (keys inserted at start and end)
// Format: [k1][encrypted_data][k2]
static inline void encrypt_message(unsigned char* data, unsigned short* size) {
    if (*size == 0 || *size >= PACKET_MESSAGE_LEN - 2) return;

    unsigned char k1 = random_byte();
    unsigned char k2 = random_byte();

    // XOR encrypt with alternating keys
    for(unsigned short i = 0; i < *size; i++) {
        unsigned char k = (i % 2) ? k1 : k2;
        data[i] ^= k;
    }

    // Shift data right by 1 to insert k1 at start
    for(int i = *size; i > 0; i--) {
        data[i] = data[i - 1];
    }
    data[0] = k1;

    // Insert k2 at end
    data[*size + 1] = k2;
    *size += 2;
}

// Decrypt message with 2-key XOR
static inline void decrypt_message(unsigned char* data, unsigned short* size) {
    if (*size < 2) return;

    unsigned char k1 = data[0];
    unsigned char k2 = data[*size - 1];

    // Remove keys and shift data left
    *size -= 2;
    for(unsigned short i = 0; i < *size; i++) {
        data[i] = data[i + 1];
    }

    // XOR decrypt with alternating keys
    for(unsigned short i = 0; i < *size; i++) {
        unsigned char k = (i % 2) ? k1 : k2;
        data[i] ^= k;
    }
}

// === PACKET OPERATIONS ===

// Create packet for writing (encrypts automatically)
static inline void packet_create_write(BinaryPacket* pkt, unsigned char id,
                                       const char* session,
                                       const void* msg_data,
                                       unsigned short msg_len) {
    pkt->seq = 0;
    pkt->id = id;
    pkt->message_len = (msg_len > PACKET_MESSAGE_LEN - 2) ? PACKET_MESSAGE_LEN - 2 : msg_len;

    // Copy session ID
    int i = 0;
    while(i < SESSION_ID_LEN && session && session[i]) {
        pkt->session_id[i] = session[i];
        i++;
    }
    pkt->session_id[i] = 0;

    // Copy message
    const unsigned char* src = (const unsigned char*)msg_data;
    for(i = 0; i < pkt->message_len; i++) {
        pkt->message[i] = src[i];
    }

    // Encrypt
    encrypt_message(pkt->message, &pkt->message_len);
}

// Read packet (decrypts automatically)
static inline int packet_read(BinaryPacket* pkt, const void* raw_data, unsigned short data_len) {
    if (data_len < sizeof(unsigned char) * 2 + sizeof(unsigned short)) return 0;

    const unsigned char* data = (const unsigned char*)raw_data;
    unsigned short offset = 0;

    pkt->seq = data[offset++];
    pkt->id = data[offset++];

    // Read message length (2 bytes, big endian)
    pkt->message_len = (data[offset] << 8) | data[offset + 1];
    offset += 2;

    if (pkt->message_len > PACKET_MESSAGE_LEN || offset + pkt->message_len > data_len) {
        return 0;
    }

    // Read session_id (10 bytes)
    for(int i = 0; i < SESSION_ID_LEN; i++) {
        pkt->session_id[i] = data[offset++];
    }
    pkt->session_id[SESSION_ID_LEN] = 0;

    // Read encrypted message
    for(unsigned short i = 0; i < pkt->message_len; i++) {
        pkt->message[i] = data[offset++];
    }

    // Decrypt
    decrypt_message(pkt->message, &pkt->message_len);

    pkt->seq++;
    return 1;
}

// Serialize packet for transmission (returns total size)
static inline unsigned short packet_serialize(const BinaryPacket* pkt, unsigned char* out_buffer) {
    unsigned short offset = 0;

    out_buffer[offset++] = pkt->seq;
    out_buffer[offset++] = pkt->id;

    // Message length (big endian)
    out_buffer[offset++] = (pkt->message_len >> 8) & 0xFF;
    out_buffer[offset++] = pkt->message_len & 0xFF;

    // Session ID
    for(int i = 0; i < SESSION_ID_LEN; i++) {
        out_buffer[offset++] = pkt->session_id[i];
    }

    // Message data
    for(unsigned short i = 0; i < pkt->message_len; i++) {
        out_buffer[offset++] = pkt->message[i];
    }

    return offset;
}

// === STREAMING FOR LARGE DATA ===

// Network byte order conversion
static inline unsigned int htonl_custom(unsigned int hostlong) {
    return ((hostlong & 0xFF) << 24) |
           ((hostlong & 0xFF00) << 8) |
           ((hostlong & 0xFF0000) >> 8) |
           ((hostlong & 0xFF000000) >> 24);
}

static inline unsigned int ntohl_custom(unsigned int netlong) {
    return htonl_custom(netlong);  // Same operation
}

// Stream helpers for large data transfers
typedef struct {
    unsigned int total_size;
    unsigned int sent;
    unsigned int chunk_size;
} StreamState;

static inline void stream_init(StreamState* state, unsigned int total_size) {
    state->total_size = total_size;
    state->sent = 0;
    state->chunk_size = CHUNK_SIZE;
}

static inline unsigned int stream_next_chunk_size(StreamState* state) {
    unsigned int remaining = state->total_size - state->sent;
    return (remaining > state->chunk_size) ? state->chunk_size : remaining;
}

static inline void stream_advance(StreamState* state, unsigned int bytes_sent) {
    state->sent += bytes_sent;
}

static inline int stream_complete(StreamState* state) {
    return state->sent >= state->total_size;
}

#endif // PACKET_H
