#pragma once
#include "packet.hpp"
#include <iostream>
#include <vector>
#include <random>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define _WINSOCKAPI_
#include <winsock2.h>
#include <ws2tcpip.h>
using socket_t = SOCKET;
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
using socket_t = int;
#endif

class Session {
public:
    Session(socket_t sock = -1) : sock_(sock) {
        // Générer clé de session
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(0, 255);
        key_.resize(32);
        for (auto& k : key_) k = dis(gen);
    }

    ~Session() { close(); }

    // Envoyer packet
    bool send(Packet& pkt) {
        // Encrypt
        pkt.encrypt(key_);

        // Envoyer taille puis data
        uint32_t size = pkt.size();
        if (::send(sock_, (char*)&size, 4, 0) != 4) return false;
        if (::send(sock_, pkt.data().data(), size, 0) != (int)size) return false;

        return true;
    }

    // Recevoir packet
    bool receive(Packet& pkt) {
        pkt.clear();

        // Lire taille
        uint32_t size;
        if (::recv(sock_, (char*)&size, 4, MSG_WAITALL) != 4) return false;
        if (size > 10000000) return false; // Max 10MB

        // Lire data
        std::vector<char> buffer(size);
        if (::recv(sock_, buffer.data(), size, MSG_WAITALL) != (int)size) return false;

        pkt.data() = buffer;

        // Decrypt
        pkt.encrypt(key_);

        return true;
    }

    // Envoyer la clé (non encryptée)
    bool send_key() {
        uint32_t size = key_.size();
        if (::send(sock_, (char*)&size, 4, 0) != 4) return false;
        if (::send(sock_, key_.data(), size, 0) != (int)size) return false;
        return true;
    }

    // Recevoir la clé
    bool receive_key() {
        uint32_t size;
        if (::recv(sock_, (char*)&size, 4, MSG_WAITALL) != 4) return false;
        if (size != 32) return false;

        key_.resize(size);
        if (::recv(sock_, key_.data(), size, MSG_WAITALL) != (int)size) return false;
        return true;
    }

    void close() {
        if (sock_ != -1) {
#ifdef _WIN32
            closesocket(sock_);
#else
            ::close(sock_);
#endif
            sock_ = -1;
        }
    }

    socket_t socket() const { return sock_; }
    void set_socket(socket_t s) { sock_ = s; }

private:
    socket_t sock_;
    std::vector<char> key_;
};
