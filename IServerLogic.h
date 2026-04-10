#pragma once
#include <cstdint>

namespace SE
{
    namespace Net
    {
        class Session;
	}

    class IServerLogic
    {
    public:
        virtual ~IServerLogic() = default;

        virtual void OnConnected(Net::Session* session) = 0;
        virtual void OnDisconnected(Net::Session* session) = 0;
        virtual void DispatchPacket(Net::Session* session, const char* packet, int32_t len) = 0;
    };
}