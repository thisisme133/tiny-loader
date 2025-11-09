#include "session.hpp"
#include <iostream>
#include <thread>
#include <vector>

#ifdef _WIN32
#pragma comment( lib, "ws2_32.lib" )
#endif

/**
 * @brief handle client connection
 * @param client_sock client socket handle
 */
auto handle_client( socket_t client_sock ) -> void
{
	session_t session{ client_sock };

	std::cout << "[+] Client connected\n";

	/*
	   send session key
	*/
	if ( !session.send_key( ) )
	{
		std::cout << "[!] Failed to send key\n";
		return;
	}

	std::cout << "[+] Session key sent\n";

	/*
	   receive and process packets
	*/
	while ( true )
	{
		packet_t pkt{ };

		if ( !session.receive( pkt ) )
		{
			std::cout << "[-] Client disconnected\n";
			break;
		}

		/*
		   read opcode
		*/
		uint8_t opcode{ };
		pkt >> opcode;

		std::cout << "[*] Received opcode: " << static_cast<int>( opcode ) << "\n";

		/*
		   process by opcode
		*/
		switch ( opcode )
		{
			case 1: // HELLO
			{
				std::string msg{ };
				pkt >> msg;
				std::cout << "[HELLO] " << msg << "\n";

				/*
				   send response
				*/
				packet_t response{ };
				response << static_cast<uint8_t>( 2 ); // opcode RESPONSE
				response << std::string{ "Hello from server!" };
				session.send( response );
				break;
			}

			case 3: // DATA
			{
				uint32_t value{ };
				pkt >> value;
				std::cout << "[DATA] Received: " << value << "\n";

				/*
				   echo back
				*/
				packet_t response{ };
				response << static_cast<uint8_t>( 3 );
				response << static_cast<uint32_t>( value * 2 );
				session.send( response );
				break;
			}

			case 99: // DISCONNECT
				std::cout << "[-] Client requested disconnect\n";
				return;

			default:
				std::cout << "[?] Unknown opcode: " << static_cast<int>( opcode ) << "\n";
		}
	}
}

/**
 * @brief main entry point
 * @return exit code
 */
auto main( ) -> int
{
	std::cout << "=== TinyLoader Server ===\n\n";

#ifdef _WIN32
	WSADATA wsa{ };
	WSAStartup( MAKEWORD( 2, 2 ), &wsa );
#endif

	/*
	   create server socket
	*/
	socket_t server_sock{ socket( AF_INET, SOCK_STREAM, 0 ) };

	int opt{ 1 };
	setsockopt( server_sock, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<char*>( &opt ), sizeof( opt ) );

	sockaddr_in addr{ };
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = INADDR_ANY;
	addr.sin_port = htons( 8888 );

	bind( server_sock, reinterpret_cast<sockaddr*>( &addr ), sizeof( addr ) );
	listen( server_sock, 10 );

	std::cout << "[Server] Listening on port 8888...\n\n";

	/*
	   accept clients
	*/
	std::vector<std::thread> threads{ };

	while ( true )
	{
		socket_t client{ accept( server_sock, nullptr, nullptr ) };

		/*
		   spawn thread for client
		*/
		threads.emplace_back( handle_client, client );
		threads.back( ).detach( );
	}

#ifdef _WIN32
	WSACleanup( );
#endif

	return 0;
}
