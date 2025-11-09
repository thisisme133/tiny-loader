#include "session.hpp"
#include <iostream>
#include <thread>
#include <chrono>

#ifdef _WIN32
#pragma comment( lib, "ws2_32.lib" )
#endif

using namespace std::chrono_literals;

/**
 * @brief main entry point
 * @return exit code
 */
auto main( ) -> int
{
	std::cout << "=== TinyLoader Client ===\n\n";

#ifdef _WIN32
	WSADATA wsa{ };
	WSAStartup( MAKEWORD( 2, 2 ), &wsa );
#endif

	/*
	   create socket and connect
	*/
	socket_t sock{ socket( AF_INET, SOCK_STREAM, 0 ) };

	sockaddr_in addr{ };
	addr.sin_family = AF_INET;
	addr.sin_port = htons( 8888 );

#ifdef _WIN32
	addr.sin_addr.s_addr = inet_addr( "127.0.0.1" );
#else
	inet_pton( AF_INET, "127.0.0.1", &addr.sin_addr );
#endif

	std::cout << "[*] Connecting to server...\n";

	if ( connect( sock, reinterpret_cast<sockaddr*>( &addr ), sizeof( addr ) ) < 0 )
	{
		std::cout << "[!] Connection failed\n";
		return 1;
	}

	session_t session{ sock };
	std::cout << "[+] Connected!\n";

	/*
	   receive session key
	*/
	if ( !session.receive_key( ) )
	{
		std::cout << "[!] Failed to receive key\n";
		return 1;
	}

	std::cout << "[+] Session key received\n\n";

	/*
	   example 1: send HELLO
	*/
	{
		packet_t pkt{ };
		pkt << static_cast<uint8_t>( 1 ); // opcode HELLO
		pkt << std::string{ "Hello from client!" };

		session.send( pkt );
		std::cout << "[->] Sent HELLO\n";

		/*
		   wait for response
		*/
		packet_t response{ };
		session.receive( response );

		uint8_t opcode{ };
		std::string msg{ };
		response >> opcode >> msg;

		std::cout << "[<-] Server response: " << msg << "\n\n";
	}

	std::this_thread::sleep_for( 1s );

	/*
	   example 2: send DATA
	*/
	{
		packet_t pkt{ };
		pkt << static_cast<uint8_t>( 3 ); // opcode DATA
		pkt << static_cast<uint32_t>( 42 );

		session.send( pkt );
		std::cout << "[->] Sent DATA: 42\n";

		/*
		   wait for response
		*/
		packet_t response{ };
		session.receive( response );

		uint8_t opcode{ };
		uint32_t value{ };
		response >> opcode >> value;

		std::cout << "[<-] Server response: " << value << "\n\n";
	}

	std::this_thread::sleep_for( 1s );

	/*
	   example 3: complex packet
	*/
	{
		packet_t pkt{ };
		pkt << static_cast<uint8_t>( 3 );        // opcode
		pkt << static_cast<uint32_t>( 123 );     // uint32
		pkt << static_cast<float>( 3.14f );      // float
		pkt << static_cast<uint16_t>( 999 );     // uint16

		std::cout << "[*] Packet size: " << pkt.size( ) << " bytes\n";
		std::cout << "[*] Contains: uint32, float, uint16\n\n";
	}

	/*
	   disconnect
	*/
	std::cout << "[*] Disconnecting...\n";

	packet_t bye{ };
	bye << static_cast<uint8_t>( 99 ); // DISCONNECT
	session.send( bye );

#ifdef _WIN32
	WSACleanup( );
#endif

	return 0;
}
