#pragma once

#include <cstddef>
#include <memory>
#include <vector>

namespace SE::Core
{
	enum class EventType
	{
		Accept,
		Recv,
		Send
	};

	struct IocpEvent : OVERLAPPED
	{
		EventType type;

		IocpEvent()
			: OVERLAPPED{}
			, type(EventType::Recv)
		{
		}

		explicit IocpEvent(EventType eventType)
			: OVERLAPPED{}
			, type(eventType)
		{
		}
	};

	struct AcceptEvent : IocpEvent
	{
		SOCKET acceptSocket = INVALID_SOCKET;
		char buffer[ ( sizeof(SOCKADDR_IN) + 16 ) * 2 ]{};

		AcceptEvent()
			: IocpEvent(EventType::Accept)
		{
		}
	};

	struct RecvEvent : IocpEvent
	{
		WSABUF wsaBuf{};

		RecvEvent()
			: IocpEvent(EventType::Recv)
		{
		}
	};

	struct SendEvent : IocpEvent
	{
		WSABUF wsaBuf{};
		std::shared_ptr<std::vector<char>> buffer;
		size_t offset = 0;

		SendEvent()
			: IocpEvent(EventType::Send)
		{
		}
	};
}
