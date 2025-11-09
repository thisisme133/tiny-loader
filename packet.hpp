#pragma once
#include <vector>
#include <cstring>
#include <stdexcept>
#include <string>
#include <cstdint>

/**
 * @brief binary packet for network communication
 */
class packet_t
{
public:
	/**
	 * @brief write typed data to packet
	 * @tparam T data type
	 * @param data data to write
	 * @return reference to this packet
	 */
	template<typename T>
	auto operator<<( const T& data ) -> packet_t&
	{
		const char* bytes{ reinterpret_cast<const char*>( &data ) };
		m_buffer.insert( m_buffer.end( ), bytes, bytes + sizeof( T ) );
		return *this;
	}

	/**
	 * @brief read typed data from packet
	 * @tparam T data type
	 * @param data output variable
	 * @return reference to this packet
	 */
	template<typename T>
	auto operator>>( T& data ) -> packet_t&
	{
		if ( m_pos + sizeof( T ) > m_buffer.size( ) )
		{
			throw std::runtime_error{ "Packet read overflow" };
		}

		std::memcpy( &data, m_buffer.data( ) + m_pos, sizeof( T ) );
		m_pos += sizeof( T );
		return *this;
	}

	/**
	 * @brief write string to packet
	 * @param str string to write
	 * @return reference to this packet
	 */
	auto operator<<( const std::string& str ) -> packet_t&
	{
		uint32_t size{ static_cast<uint32_t>( str.size( ) ) };
		*this << size;
		m_buffer.insert( m_buffer.end( ), str.begin( ), str.end( ) );
		return *this;
	}

	/**
	 * @brief read string from packet
	 * @param str output string
	 * @return reference to this packet
	 */
	auto operator>>( std::string& str ) -> packet_t&
	{
		uint32_t size{ };
		*this >> size;

		if ( m_pos + size > m_buffer.size( ) )
		{
			throw std::runtime_error{ "Packet read overflow" };
		}

		str.assign( m_buffer.begin( ) + m_pos, m_buffer.begin( ) + m_pos + size );
		m_pos += size;
		return *this;
	}

	/**
	 * @brief get const buffer
	 * @return const reference to buffer
	 */
	auto data( ) const -> const std::vector<char>&
	{
		return m_buffer;
	}

	/**
	 * @brief get mutable buffer
	 * @return reference to buffer
	 */
	auto data( ) -> std::vector<char>&
	{
		return m_buffer;
	}

	/**
	 * @brief get packet size
	 * @return size in bytes
	 */
	auto size( ) const -> size_t
	{
		return m_buffer.size( );
	}

	/**
	 * @brief clear packet data
	 */
	auto clear( ) -> void
	{
		m_buffer.clear( );
		m_pos = 0;
	}

	/**
	 * @brief reset read position
	 */
	auto reset( ) -> void
	{
		m_pos = 0;
	}

	/**
	 * @brief encrypt packet with XOR
	 * @param key encryption key
	 */
	auto encrypt( const std::vector<char>& key ) -> void
	{
		for ( size_t i{ 0 }; i < m_buffer.size( ); ++i )
		{
			m_buffer[i] ^= key[i % key.size( )];
		}
	}

private:
	std::vector<char> m_buffer{ };
	size_t m_pos{ 0 };
};
