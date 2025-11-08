#ifndef SERVER_PACKET_HANDLER_H
#define SERVER_PACKET_HANDLER_H

#include "packet.h"
#include <sys/socket.h>
#include <unistd.h>

// Server-side packet reception with CRC verification
// This module provides functions to receive and validate BinaryPacket with CRC32

// Helper: receive exact number of bytes
static inline int server_recv_exact(int socket_fd, void* buffer, int length) {
    int total = 0;
    char* ptr = (char*)buffer;

    while(total < length) {
        int received = read(socket_fd, ptr + total, length - total);
        if(received <= 0) return -1;
        total += received;
    }

    return total;
}

// Receive and verify BinaryPacket with CRC32 check
// Returns: 1 on success, 0 on failure, -1 on CRC error
static inline int server_recv_packet_with_crc(int socket_fd, BinaryPacket* pkt, int* crc_valid) {
    unsigned char buffer[sizeof(BinaryPacket) + 16];

    // Read header (seq + id + message_len = 4 bytes)
    if(server_recv_exact(socket_fd, buffer, 4) <= 0) {
        return 0;
    }

    // Extract message length
    unsigned short msg_len = (buffer[2] << 8) | buffer[3];
    if(msg_len > PACKET_MESSAGE_LEN) {
        write(1, "[!] Invalid packet: message too large\n", 38);
        return 0;
    }

    // Read rest: session_id (10) + message (msg_len) + crc32 (4)
    unsigned short rest_len = SESSION_ID_LEN + msg_len + 4;
    if(server_recv_exact(socket_fd, buffer + 4, rest_len) <= 0) {
        return 0;
    }

    unsigned short total_len = 4 + rest_len;

    // Verify CRC32 BEFORE parsing/decrypting
    *crc_valid = packet_verify_crc(buffer, total_len);

    if(!*crc_valid) {
        write(1, "[!] CRC32 verification failed - corrupted packet or tampering detected\n", 71);
        return -1;  // CRC error
    }

    // CRC valid, parse packet
    if(!packet_read(pkt, buffer, total_len)) {
        write(1, "[!] Failed to parse packet\n", 27);
        return 0;
    }

    return 1;
}

// Statistics structure for monitoring
typedef struct {
    unsigned long long total_packets;
    unsigned long long crc_failures;
    unsigned long long parse_failures;
    unsigned long long successful_packets;
} PacketStats;

static inline void packet_stats_init(PacketStats* stats) {
    stats->total_packets = 0;
    stats->crc_failures = 0;
    stats->parse_failures = 0;
    stats->successful_packets = 0;
}

static inline void packet_stats_update(PacketStats* stats, int result, int crc_valid) {
    stats->total_packets++;

    if(result == -1) {
        stats->crc_failures++;
    } else if(result == 0) {
        stats->parse_failures++;
    } else if(result == 1 && crc_valid) {
        stats->successful_packets++;
    }
}

// Print statistics (no CRT, manual formatting)
static inline void packet_stats_print(PacketStats* stats) {
    write(1, "\n=== Packet Statistics ===\n", 26);

    // Total packets
    write(1, "Total packets: ", 15);
    unsigned long long val = stats->total_packets;
    char buf[32];
    int pos = 0;
    if(val == 0) {
        buf[pos++] = '0';
    } else {
        char temp[32];
        int temp_pos = 0;
        while(val > 0) {
            temp[temp_pos++] = '0' + (val % 10);
            val /= 10;
        }
        while(temp_pos > 0) {
            buf[pos++] = temp[--temp_pos];
        }
    }
    write(1, buf, pos);
    write(1, "\n", 1);

    // CRC failures
    write(1, "CRC failures: ", 14);
    val = stats->crc_failures;
    pos = 0;
    if(val == 0) {
        buf[pos++] = '0';
    } else {
        char temp[32];
        int temp_pos = 0;
        while(val > 0) {
            temp[temp_pos++] = '0' + (val % 10);
            val /= 10;
        }
        while(temp_pos > 0) {
            buf[pos++] = temp[--temp_pos];
        }
    }
    write(1, buf, pos);
    write(1, "\n", 1);

    // Success rate
    write(1, "Successful: ", 12);
    val = stats->successful_packets;
    pos = 0;
    if(val == 0) {
        buf[pos++] = '0';
    } else {
        char temp[32];
        int temp_pos = 0;
        while(val > 0) {
            temp[temp_pos++] = '0' + (val % 10);
            val /= 10;
        }
        while(temp_pos > 0) {
            buf[pos++] = temp[--temp_pos];
        }
    }
    write(1, buf, pos);
    write(1, "\n", 1);

    write(1, "=========================\n\n", 28);
}

// Action on CRC failure (ban client, log, etc.)
typedef enum {
    CRC_FAIL_IGNORE = 0,
    CRC_FAIL_WARN = 1,
    CRC_FAIL_DISCONNECT = 2,
    CRC_FAIL_BAN = 3
} CRCFailureAction;

static inline int handle_crc_failure(int socket_fd, CRCFailureAction action) {
    switch(action) {
        case CRC_FAIL_IGNORE:
            return 1;  // Continue

        case CRC_FAIL_WARN:
            write(1, "[WARN] CRC failure detected, continuing...\n", 43);
            return 1;

        case CRC_FAIL_DISCONNECT:
            write(1, "[!] CRC failure - disconnecting client\n", 39);
            return 0;

        case CRC_FAIL_BAN:
            write(1, "[!] CRC failure - BANNING client (tampering detected)\n", 54);
            // Could add IP ban logic here
            return 0;

        default:
            return 1;
    }
}

#endif // SERVER_PACKET_HANDLER_H
