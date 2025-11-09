# TinyLoader - Simple C++20 Client/Server

**Ultra simple** client/serveur avec packets style SFML. Seulement 4 fichiers !

## 📁 Fichiers

- `packet.hpp` - Packet style SFML avec `<<` et `>>`
- `session.hpp` - Socket + encryption XOR
- `server.cpp` - Main serveur (1 thread par client)
- `client.cpp` - Main client

## 🚀 Compilation

```bash
mkdir build && cd build
cmake ..
cmake --build . --config Release
```

## ▶️ Usage

**Terminal 1 - Serveur :**
```bash
./server
```

**Terminal 2 - Client :**
```bash
./client
```

## 💡 Exemples

### Créer un packet

```cpp
Packet pkt;
pkt << (uint8_t)1;              // opcode
pkt << (uint32_t)12345;         // data
pkt << std::string("hello");    // string
pkt << (float)3.14f;            // float
```

### Lire un packet

```cpp
Packet pkt;
// ... recevoir le packet ...

uint8_t opcode;
uint32_t value;
std::string text;
float pi;

pkt >> opcode >> value >> text >> pi;
```

### Envoyer/Recevoir

```cpp
Session session(socket);

// Envoyer
session.send(pkt);

// Recevoir
Packet response;
session.receive(response);
```

## 🔐 Encryption

- Clé de session générée automatiquement (32 bytes)
- Encryption XOR simple et efficace
- Échange de clé au début de la connexion

## 🎯 Opcodes

Vous définissez vos opcodes comme vous voulez :

```cpp
// Exemple
const uint8_t OPCODE_HELLO = 1;
const uint8_t OPCODE_RESPONSE = 2;
const uint8_t OPCODE_DATA = 3;
const uint8_t OPCODE_DISCONNECT = 99;
```

## ✨ C'est tout !

Simple, court, efficace. Ajoutez vos opcodes et votre logique dans `server.cpp` !
