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

/**
 * @brief network session with encryption
 */
class session_t
{
public:
	/**
	 * @brief constructor
	 * @param sock socket handle
	 */
	explicit session_t( socket_t sock = -1 )
		: m_sock{ sock }
	{
		/*
		   generate session key
		*/
		std::random_device rd{ };
		std::mt19937 gen{ rd( ) };
		std::uniform_int_distribution<> dis{ 0, 255 };

		m_key.resize( 32 );

		for ( auto& k : m_key )
		{
			k = static_cast<char>( dis( gen ) );
		}
	}

	/**
	 * @brief destructor
	 */
	~session_t( )
	{
		close( );
	}

	/**
	 * @brief send packet
	 * @param pkt packet to send
	 * @return true if success
	 */
	auto send( packet_t& pkt ) -> bool
	{
		/*
		   encrypt packet
		*/
		pkt.encrypt( m_key );

		/*
		   send size then data
		*/
		uint32_t size{ static_cast<uint32_t>( pkt.size( ) ) };

		if ( ::send( m_sock, reinterpret_cast<char*>( &size ), 4, 0 ) != 4 )
		{
			return false;
		}

		if ( ::send( m_sock, pkt.data( ).data( ), static_cast<int>( size ), 0 ) != static_cast<int>( size ) )
		{
			return false;
		}

		return true;
	}

	/**
	 * @brief receive packet
	 * @param pkt output packet
	 * @return true if success
	 */
	auto receive( packet_t& pkt ) -> bool
	{
		pkt.clear( );

		/*
		   read size
		*/
		uint32_t size{ };

#ifdef _WIN32
		if ( ::recv( m_sock, reinterpret_cast<char*>( &size ), 4, 0 ) != 4 )
#else
		if ( ::recv( m_sock, reinterpret_cast<char*>( &size ), 4, MSG_WAITALL ) != 4 )
#endif
		{
			return false;
		}

		if ( size > 10000000 )
		{
			return false; // max 10MB
		}

		/*
		   read data
		*/
		std::vector<char> buffer( size );

#ifdef _WIN32
		int total_recv{ 0 };
		while ( total_recv < static_cast<int>( size ) )
		{
			int received{ ::recv( m_sock, buffer.data( ) + total_recv, static_cast<int>( size ) - total_recv, 0 ) };
			if ( received <= 0 )
			{
				return false;
			}
			total_recv += received;
		}
#else
		if ( ::recv( m_sock, buffer.data( ), size, MSG_WAITALL ) != static_cast<int>( size ) )
		{
			return false;
		}
#endif

		pkt.data( ) = buffer;

		/*
		   decrypt packet
		*/
		pkt.encrypt( m_key );

		return true;
	}

	/**
	 * @brief send session key (unencrypted)
	 * @return true if success
	 */
	auto send_key( ) -> bool
	{
		uint32_t size{ static_cast<uint32_t>( m_key.size( ) ) };

		if ( ::send( m_sock, reinterpret_cast<char*>( &size ), 4, 0 ) != 4 )
		{
			return false;
		}

		if ( ::send( m_sock, m_key.data( ), static_cast<int>( size ), 0 ) != static_cast<int>( size ) )
		{
			return false;
		}

		return true;
	}

	/**
	 * @brief receive session key
	 * @return true if success
	 */
	auto receive_key( ) -> bool
	{
		uint32_t size{ };

#ifdef _WIN32
		if ( ::recv( m_sock, reinterpret_cast<char*>( &size ), 4, 0 ) != 4 )
#else
		if ( ::recv( m_sock, reinterpret_cast<char*>( &size ), 4, MSG_WAITALL ) != 4 )
#endif
		{
			return false;
		}

		if ( size != 32 )
		{
			return false;
		}

		m_key.resize( size );

#ifdef _WIN32
		int total_recv{ 0 };
		while ( total_recv < static_cast<int>( size ) )
		{
			int received{ ::recv( m_sock, m_key.data( ) + total_recv, static_cast<int>( size ) - total_recv, 0 ) };
			if ( received <= 0 )
			{
				return false;
			}
			total_recv += received;
		}
#else
		if ( ::recv( m_sock, m_key.data( ), size, MSG_WAITALL ) != static_cast<int>( size ) )
		{
			return false;
		}
#endif

		return true;
	}

	/**
	 * @brief close session
	 */
	auto close( ) -> void
	{
		if ( m_sock != -1 )
		{
#ifdef _WIN32
			closesocket( m_sock );
#else
			::close( m_sock );
#endif
			m_sock = -1;
		}
	}

	/**
	 * @brief get socket handle
	 * @return socket handle
	 */
	auto socket( ) const -> socket_t
	{
		return m_sock;
	}

	/**
	 * @brief set socket handle
	 * @param sock socket handle
	 */
	auto set_socket( socket_t sock ) -> void
	{
		m_sock = sock;
	}

private:
	socket_t m_sock{ -1 };
	std::vector<char> m_key{ };
};
