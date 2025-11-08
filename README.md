# TinyLoader - Modern C++23 Client/Server

Serveur et client réseau moderne en **C++23 pur**, avec système de paquets binaires, multi-threading, handlers d'opcodes et encryption de session. **Aucune bibliothèque externe** (seulement STL).

## 🚀 Caractéristiques

### Architecture
- ✅ **C++23** - Utilise std::print, concepts, std::span, etc.
- ✅ **Multi-threaded** - std::jthread pour chaque client
- ✅ **Cross-platform** - Windows (WinSock) et Linux (sockets POSIX)
- ✅ **Header-only** - Implémentation dans les headers pour performance
- ✅ **Aucune lib externe** - 100% STL

### Système de Paquets
- Format binaire compact : `[opcode:2][size:4][data:variable]`
- Sérialisation/Désérialisation automatique
- Lecture/Écriture typée avec `packet.write<T>()` / `packet.read<T>()`
- Big-endian pour compatibilité réseau

### Encryption
- Clé de session unique par client (32 bytes)
- XOR encryption symétrique sur le payload
- Échange de clé automatique à la connexion
- Header non-encrypté (opcode + size) pour parsing

### Handlers d'Opcodes
- Système modulaire d'enregistrement de handlers
- `OpcodeHandler::instance().register_handler(opcode, func)`
- Handlers côté serveur ET client
- Gestion d'erreurs automatique

## 📦 Structure du Projet

```
tiny-loader/
├── CMakeLists.txt              # Configuration CMake
├── src/
│   ├── core/
│   │   ├── packet.hpp          # Système de paquets binaires
│   │   ├── session.hpp         # Gestion session + socket
│   │   ├── encryption.hpp      # Clé de session XOR
│   │   ├── packet.cpp
│   │   ├── session.cpp
│   │   └── encryption.cpp
│   ├── handlers/
│   │   ├── opcode_handler.hpp  # Gestionnaire d'opcodes
│   │   └── opcode_handler.cpp
│   ├── server/
│   │   ├── server.hpp          # Serveur multi-threaded
│   │   ├── server.cpp
│   │   └── main.cpp            # Point d'entrée serveur
│   └── client/
│       ├── client.hpp          # Client avec thread de réception
│       ├── client.cpp
│       └── main.cpp            # Point d'entrée client
└── README.md
```

## 🔧 Compilation

### Prérequis
- **Compilateur C++23** : GCC 13+, Clang 17+, ou MSVC 2022+
- **CMake 3.25+**

### Windows (MSVC)
```bash
mkdir build && cd build
cmake .. -G "Visual Studio 17 2022"
cmake --build . --config Release
```

### Linux/Unix
```bash
mkdir build && cd build
cmake ..
make -j$(nproc)
```

## 🎯 Utilisation

### Lancer le Serveur
```bash
./server
```

Output :
```
=== TinyLoader Server (C++23) ===

[Server] Handlers registered
[Server] Listening on port 8888
```

### Lancer le Client
```bash
./client
```

Output :
```
=== TinyLoader Client (C++23) ===

[Client] Handlers registered
[Client] Connected to 127.0.0.1:8888
[Client] Session key received and encryption enabled
[Client] HELLO sent
[Server says] Welcome to the server!
[Client] Heartbeat #1 sent
[Client] Heartbeat #2 sent
...
```

## 💻 Exemples de Code

### Créer un Paquet

```cpp
#include "core/packet.hpp"

core::Packet packet(core::Opcode::DATA);

// Écrire des données typées
packet.write<uint32_t>(12345);
packet.write<float>(3.14f);

// Écrire un buffer
std::string msg = "Hello";
packet.write(std::span(reinterpret_cast<const uint8_t*>(msg.data()), msg.size()));

// Sérialiser pour envoi
auto bytes = packet.serialize();
```

### Lire un Paquet

```cpp
// Recevoir via session
auto packet_opt = session.receive();
if (packet_opt) {
    auto& packet = *packet_opt;

    // Lire données typées
    uint32_t value = packet.read<uint32_t>();
    float pi = packet.read<float>();

    // Lire buffer
    auto data = packet.read_bytes(5);
    std::string msg(data.begin(), data.end());
}
```

### Enregistrer un Handler

```cpp
#include "handlers/opcode_handler.hpp"

void handle_custom_opcode(core::Session& session, core::Packet& packet) {
    std::println("[Handler] Custom opcode received");

    // Traiter le paquet
    auto data = packet.read<uint32_t>();

    // Répondre
    core::Packet response(core::Opcode::DATA);
    response.write<uint32_t>(data * 2);
    session.send(response);
}

// Enregistrement
handlers::OpcodeHandler::instance().register_handler(
    core::Opcode::DATA,
    handle_custom_opcode
);
```

### Ajouter un Opcode

```cpp
// Dans packet.hpp
enum class Opcode : uint16_t {
    HELLO = 0x0001,
    AUTH_REQUEST = 0x0002,
    AUTH_RESPONSE = 0x0003,
    HEARTBEAT = 0x0004,
    DATA = 0x0005,
    DISCONNECT = 0x0006,
    MY_CUSTOM_OPCODE = 0x0100,  // ← Ajouter ici
};
```

## 🔐 Encryption de Session

### Côté Serveur
```cpp
// 1. Créer session (génère clé auto)
auto session = std::make_unique<core::Session>(client_socket);

// 2. Envoyer la clé au client
core::Packet key_packet(core::Opcode::AUTH_RESPONSE);
auto key_data = session->key().data();
key_packet.write(std::span(key_data.begin(), key_data.end()));
session->send(key_packet);  // Envoi sans encryption

// 3. Activer encryption
session->set_encrypted(true);
```

### Côté Client
```cpp
// 1. Recevoir la clé du serveur
auto key_packet_opt = session->receive();
auto& key_packet = *key_packet_opt;

// 2. Extraire et appliquer la clé
auto key_bytes = key_packet.read_bytes(core::SessionKey::KEY_SIZE);
core::SessionKey new_key;
new_key.set_key(std::span<const uint8_t, 32>(key_bytes.data(), 32));
session->set_key(new_key);

// 3. Activer encryption
session->set_encrypted(true);
```

## 🧵 Multi-Threading

### Serveur
- Thread principal : `accept()` loop
- 1 thread par client : `std::jthread` avec `std::stop_token`
- Auto-cleanup : threads terminés automatiquement

```cpp
// Dans Server::accept_loop()
client_threads_.emplace_back([this, client_socket]() {
    handle_client(client_socket);
});
```

### Client
- Thread principal : envoi de paquets
- Thread de réception : `std::jthread` dédié

```cpp
receive_thread_ = std::jthread([this](std::stop_token stoken) {
    receive_loop(stoken);
});
```

## 📊 Format de Paquet

```
┌─────────────┬──────────┬─────────────┐
│  Opcode     │  Size    │    Data     │
│  (2 bytes)  │ (4 bytes)│  (variable) │
│  Big-endian │Big-endian│  Encrypted  │
└─────────────┴──────────┴─────────────┘
```

- **Opcode** : Type de paquet (2 bytes, big-endian)
- **Size** : Taille du payload (4 bytes, big-endian, max 1MB)
- **Data** : Payload encrypté avec clé de session (XOR)

## 🎓 Fonctionnalités C++23 Utilisées

- `std::print` / `std::println` - I/O formaté moderne
- `std::span` - Vues non-propriétaires sur mémoire
- `std::jthread` - Threads avec stop_token auto
- `concepts` / `requires` - Contraintes de templates
- `std::optional` - Retours optionnels type-safe
- `[[nodiscard]]` - Attributs de fonction
- Range-based for avec init-statement

## 📝 TODO / Extensions Possibles

- [ ] Ajout de compression (zlib optionnelle)
- [ ] Support IPv6
- [ ] Rate limiting
- [ ] Blacklist IP
- [ ] Logs dans fichier
- [ ] Statistiques réseau
- [ ] Reconnexion automatique client
- [ ] SSL/TLS (optionnel)

## 📄 License

Projet éducatif - Usage académique uniquement

---

**Note** : Ce projet démontre une architecture moderne C++23 pour un système client/serveur. Le chiffrement XOR est simple mais suffisant pour comprendre les concepts. Pour la production, utilisez TLS/SSL.
