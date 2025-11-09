#pragma once

#include <cstdint>
#include <string>

/**
 * @brief opcodes for client-server communication
 */
enum class opcode_t : uint8_t
{
	HELLO = 1,
	GAME_LIST = 2,
	DATA = 3,
	GAME_REQUEST = 4,
	DISCONNECT = 99
};

/**
 * @brief game status enumeration
 */
enum class game_status_t : uint8_t
{
	OFFLINE = 0,
	ONLINE = 1,
	MAINTENANCE = 2
};

/**
 * @brief structure containing game information
 */
struct game_info_t
{
	std::string name{ };
	game_status_t status{ game_status_t::OFFLINE };
	std::string process_name{ };
};
