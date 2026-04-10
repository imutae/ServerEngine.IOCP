#pragma once
#include <cstdint>

namespace SE::Packet {
#pragma pack(push, 1)
	struct PacketHeader {
		uint16_t size;
		uint16_t id;
	};
#pragma pack(pop)
}