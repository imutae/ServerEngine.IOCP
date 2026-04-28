#pragma once

#include <cstdint>

#include "IocpObject.h"
#include "IocpEvent.h"
#include "RecvBuffer.h"

namespace SE
{
	class IServerLogic;
}

namespace SE::Net
{
	class Session : public Core::IocpObject
	{
	public:
		explicit Session(uint64_t sessionId);
		~Session() override;

		Session(const Session&) = delete;
		Session& operator=(const Session&) = delete;
		Session(Session&&) = delete;
		Session& operator=(Session&&) = delete;

	public:
		HANDLE GetHandle() override;

		void Dispatch(
			Core::IocpEvent* event,
			int32_t numOfBytes,
			int32_t errorCode) override;

	public:
		bool Initialize(SOCKET socket, IServerLogic* logic);
		void Disconnect();

	public:
		void Start();
		bool Send(uint16_t packetId, const void* body, int32_t len);

	public:
		uint64_t GetSessionId() const { return _sessionId; }

	private:
		void PostRecv();

	private:
		void ProcessRecv(int32_t numOfBytes, int32_t errorCode);
		void ProcessSend(Core::IocpEvent* event, int32_t numOfBytes, int32_t errorCode);

	private:
		IServerLogic* _logic;
		SOCKET _socket;
		bool _isConnected;
		uint64_t _sessionId;

		Core::RecvEvent _recvEvent;

		RecvBuffer _recvBuffer;
	};
}
