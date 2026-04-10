#pragma once

namespace SE::Core
{
	enum class EventType : uint8_t
	{
		Accept,
		Recv,
		Send
	};

	struct IocpEvent : public OVERLAPPED
	{
		EventType type;

		IocpEvent(EventType t) : type(t)
		{
			::ZeroMemory(static_cast<OVERLAPPED*>(this), sizeof(OVERLAPPED));
		}
	};

	struct AcceptEvent : public IocpEvent
	{
		SOCKET acceptSocket;
		char buffer[(sizeof(sockaddr_in) + 16) * 2];

		AcceptEvent() : IocpEvent(EventType::Accept), acceptSocket(INVALID_SOCKET)
		{
			::ZeroMemory(buffer, sizeof(buffer));
		}
	};

	struct RecvEvent : public IocpEvent
	{
		WSABUF wsaBuf;

		RecvEvent() : IocpEvent(EventType::Recv)
		{
			::ZeroMemory(&wsaBuf, sizeof(wsaBuf));
		}
	};

	struct SendEvent : public IocpEvent
	{
		WSABUF wsaBuf;
		std::shared_ptr<std::vector<char>> buffer;

		SendEvent() : IocpEvent(EventType::Send)
		{
			::ZeroMemory(&wsaBuf, sizeof(wsaBuf));
		}
	};
}

