# Tiny Loader - Binary Packet System

Projet éducatif démontrant un système de communication client-serveur sécurisé sans CRT, avec chiffrement et streaming de données.

## Architecture

### Client (Windows)
- `client.c` - Client anti-cheat avec détections anti-debug/anti-VM
- Support du chiffrement à 2 clés
- Streaming de gros fichiers (images, binaires)
- Pas de CRT (no malloc, no stdlib, no string.h)

### Serveur (Linux/Unix)
- `server.c` - Serveur multi-threadé avec pthread
- Monitoring des heartbeats avec timeout
- Réception et traitement des paquets binaires
- Bannissement automatique basé sur trust factor

## Système de Paquets

### Architecture Binaire (packet.h)

Remplace JSON par une structure binaire compacte :

```c
typedef struct {
    unsigned char seq;                      // Numéro de séquence
    unsigned char id;                       // Type de paquet
    unsigned short message_len;             // Taille du message
    char session_id[11];                    // ID de session
    unsigned char message[512];             // Données (chiffrées)
} BinaryPacket;
```

### Types de Paquets

| ID | Nom | Description |
|----|-----|-------------|
| 0x00 | MESSAGE | Message texte générique |
| 0x01 | HWID | Demande de HWID |
| 0x02 | HWID_RESP | Réponse HWID |
| 0x03 | SESSION | Session établie |
| 0x04 | LOGIN_REQ | Requête de login |
| 0x05 | LOGIN_RESP | Réponse de login |
| 0x06 | SECURITY_REPORT | Rapport de sécurité (anti-debug/VM) |
| 0x07 | BAN | Bannissement client |
| 0x08 | GAME_SELECT | Sélection de jeu |
| 0x09 | IMAGE | Transfert d'image/binaire |
| 0x0A | FUNCTION_REQUEST | Requête de fonction |
| 0x0B | FUNCTION_BYTES | Bytes de fonction |

## Chiffrement à 2 Clés

Inspiré du système original avec XOR alterné :

```c
// Encryption
encrypt_message(data, &len);
// Format: [k1][encrypted_data][k2]

// Decryption
decrypt_message(data, &len);
```

### Algorithme

1. **Génération des clés** : 2 bytes aléatoires (k1, k2) via RDRAND ou RDTSC
2. **XOR alterné** : `data[i] ^= (i % 2) ? k1 : k2`
3. **Insertion clés** : k1 au début, k2 à la fin
4. **Décryption** : Inverse (extraction clés, XOR alterné)

## Intégrité des Paquets - CRC32

Chaque paquet inclut un CRC32 à la fin pour détecter la corruption ou le tampering.

### Format du Paquet sur le Réseau

```
[seq][id][message_len][session_id][message][CRC32]
  1    1       2           10       0-512      4   bytes
```

### Fonctionnement

**Côté Client** (envoi) :
```c
// CRC calculé automatiquement lors de la sérialisation
BinaryPacket pkt;
packet_create_write(&pkt, PKT_ID_MESSAGE, session, data, len);
unsigned short size = packet_serialize(&pkt, buffer);  // CRC ajouté ici
send(sock, buffer, size, 0);
```

**Côté Serveur** (réception + vérification) :
```c
#include "server_packet_handler.h"

BinaryPacket pkt;
int crc_valid;

int result = server_recv_packet_with_crc(socket, &pkt, &crc_valid);
if(result == -1) {
    // CRC invalide - paquet corrompu ou modifié
    handle_crc_failure(socket, CRC_FAIL_DISCONNECT);
} else if(result == 1 && crc_valid) {
    // Paquet valide, traiter
}
```

### Actions sur Échec CRC

| Action | Description |
|--------|-------------|
| `CRC_FAIL_IGNORE` | Ignorer et continuer |
| `CRC_FAIL_WARN` | Logger un avertissement |
| `CRC_FAIL_DISCONNECT` | Déconnecter le client |
| `CRC_FAIL_BAN` | Bannir le client (tampering détecté) |

### Calcul CRC32

Utilise le polynôme IEEE 802.3 (0xEDB88320) avec table de lookup précalculée :

```c
#include "crc32.h"

// Calcul simple
unsigned int crc = crc32_calculate(data, length);

// Calcul incrémental (streaming)
unsigned int crc = crc32_init();
crc = crc32_update(crc, chunk1, len1);
crc = crc32_update(crc, chunk2, len2);
crc = crc32_finalize(crc);

// Vérification
int valid = crc32_verify(data, length, expected_crc);
```

### Statistiques de Paquets

Le serveur peut monitorer les échecs CRC :

```c
PacketStats stats;
packet_stats_init(&stats);

// Après chaque réception
packet_stats_update(&stats, result, crc_valid);

// Afficher les stats
packet_stats_print(&stats);
// Output:
// === Packet Statistics ===
// Total packets: 1000
// CRC failures: 5
// Successful: 995
// =========================
```

## Streaming de Données

Pour transférer de gros fichiers (images, binaires > 512 bytes) :

```c
// Envoi
client_stream_send(client, data, size);

// Réception (allocation via VirtualAlloc/mmap)
void* data = client_stream_recv(client, &size);
```

### Protocole de Streaming

1. Envoi de la taille (4 bytes, network byte order)
2. Transfert par chunks de 4KB
3. Validation de la taille (max 100MB)

## Exemples d'Utilisation

### 1. Envoyer un paquet simple

```c
BinaryPacket pkt;
char message[] = "Hello Server";
packet_create_write(&pkt, PKT_ID_MESSAGE, session_id, message, sizeof(message));
client_send_packet(&client, &pkt);
```

### 2. Rapport de sécurité

```c
SecurityReport report = {
    .debugger_detected = 0,
    .vm_detected = 0,
    .trust_factor = 95.5f,
    .uptime = 3600,
    .checksum = 0x12345678
};
client_send_security_report(&client, &report);
```

### 3. Login

```c
LoginRequest login;
// Remplir les champs (username, password, hwid, version)
client_send_login(&client, &login);

// Recevoir la réponse
BinaryPacket response;
client_recv_packet(&client, &response);
```

### 4. Streaming d'image

```c
// Envoi de 5MB
unsigned char* image = VirtualAlloc(0, 5*1024*1024, MEM_COMMIT, PAGE_READWRITE);
client_stream_send(&client, image, 5*1024*1024);

// Réception
unsigned int size;
void* data = client_stream_recv(&client, &size);
```

## Structures de Données

### ExtendedClient

```c
typedef struct {
    void* socket_handle;
    char session_id[11];
    unsigned char state;
    unsigned int hwid_result;
    MapperData mapper;
    GameData games[16];
    GameData selected_game;
} ExtendedClient;
```

### SecurityReport

```c
typedef struct {
    unsigned char debugger_detected;
    unsigned char vm_detected;
    float trust_factor;
    unsigned int uptime;
    unsigned int checksum;
} SecurityReport;
```

## États du Client

| État | Valeur | Description |
|------|--------|-------------|
| CONNECTING | 0 | Connexion en cours |
| IDLE | 1 | Connecté, en attente |
| LOGGED_IN | 2 | Authentifié |
| IMPORTS_READY | 3 | Imports prêts |
| WAITING | 4 | En attente de données |
| IMAGE_READY | 5 | Image reçue |
| INJECTED | 6 | Injection effectuée |
| BLACKLISTED | 7 | Client banni |

## Résultats HWID

| Code | Signification |
|------|---------------|
| 5671 | HWID_FAIL - Échec validation |
| 4567 | HWID_BLACKLISTED - HWID banni |
| 5472 | VERSION_MISMATCH - Version incorrecte |
| 3247 | HWID_OK - Validation réussie |

## Compilation

### Client (Windows - MSVC)

```bash
mkdir build && cd build
cmake .. -G "Visual Studio 17 2022"
cmake --build . --config Release
```

### Serveur (Linux)

```bash
mkdir build && cd build
cmake ..
make
./server
```

## Sécurité

Ce projet est **éducatif** et démontre :

- ✅ Chiffrement XOR à 2 clés (éducatif, pas production)
- ✅ **Intégrité CRC32** - Détection corruption/tampering
- ✅ Détection anti-debug/anti-VM (9+6 techniques)
- ✅ Trust factor et bannissement automatique
- ✅ Communication binaire sans CRT
- ✅ Streaming sécurisé avec validation de taille
- ✅ Statistiques de paquets côté serveur

### Protections Implémentées

| Niveau | Protection | Implémentation |
|--------|------------|----------------|
| **Transport** | Intégrité | CRC32 (IEEE 802.3) |
| **Application** | Chiffrement | XOR 2 clés alternées |
| **Session** | Trust Factor | Score 0-100%, ban < 30% |
| **Client** | Anti-debug | 9 techniques (PEB, timing, etc.) |
| **Client** | Anti-VM | 6 techniques (CPUID, registre, etc.) |

**Attention** : Ce code est destiné à l'apprentissage uniquement.

## Fichiers

```
tiny-loader/
├── CMakeLists.txt              # Configuration CMake
├── common.h                    # Structures communes (legacy)
├── crc32.h                     # Calcul CRC32 (IEEE 802.3)
├── packet.h                    # Système de paquets binaires + CRC
├── client_ext.h                # Extensions client (streaming, etc.)
├── server_packet_handler.h     # Gestion serveur avec vérification CRC
├── client.c                    # Client Windows
├── server.c                    # Serveur Linux
├── example_usage.c             # Exemples d'utilisation (11 exemples)
└── README.md                   # Cette documentation
```

## Notes Techniques

### Pas de CRT

Toutes les fonctions évitent la CRT :
- Pas de `malloc/free` → VirtualAlloc/mmap
- Pas de `strcpy/strlen` → Boucles manuelles
- Pas de `printf` → write/OutputDebugString
- Pas de `rand()` → RDRAND/RDTSC

### Compatibilité

- **Client** : Windows x64, Visual Studio 2019+, privilèges admin requis
- **Serveur** : Linux/Unix, gcc/clang, pthread

### Performance

- Streaming : ~90 MB/s avec chunks de 4KB
- Encryption : Négligeable (simple XOR)
- Overhead paquet : 14 bytes (header + session)

## License

Projet éducatif - Usage académique uniquement
