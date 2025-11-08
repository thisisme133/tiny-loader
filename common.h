#ifndef COMMON_H
#define COMMON_H

// Platform-specific includes
#ifdef __linux__
#include <sys/mman.h>
#endif

// Configuration
#define PORT 8888
#define KEY_SIZE 32
#define MAX_PAYLOAD 65000
#define HEARTBEAT_TIMEOUT 15  // seconds

// Packet types
#define PKT_HELLO 0x01
#define PKT_ACK 0x02
#define PKT_DATA 0x03
#define PKT_HEARTBEAT 0x04
#define PKT_SESSION_KEY 0x05
#define PKT_STEAM_DATA 0x06

// Structures
typedef struct {
    void* socket;
    unsigned char key[KEY_SIZE];
    unsigned char active;
    unsigned long long last_heartbeat;  // For server-side timeout monitoring
} Session;

typedef struct {
    unsigned char type;
    unsigned short size;
    unsigned char data[MAX_PAYLOAD];
} Packet;

typedef struct {
    unsigned char debugger_detected;
    unsigned char vm_detected;
    float trust_factor;
    unsigned int uptime;
} HeartbeatData;

// XOR encryption/decryption function
static inline void xor_crypt(unsigned char* data, unsigned short size, unsigned char* key) {
    for(unsigned short i = 0; i < size; i++) {
        data[i] ^= key[i % KEY_SIZE];
    }
}

#endif // COMMON_H
