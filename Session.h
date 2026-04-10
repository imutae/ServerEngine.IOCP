#pragma once
#include "IocpObject.h"
#include "IocpEvent.h"
#include "RecvBuffer.h"

namespace SE::Net
{
	class Session : public Core::IocpObject
	{
	public:
		Session(uint64_t sessionId);
		~Session() override;

		Session(const Session&) = delete;
		Session& operator=(const Session&) = delete;

		Session(Session&&) = delete;
		Session& operator=(Session&&) = delete;

	public:
		HANDLE GetHandle() override;
		void Dispatch(Core::IocpEvent* event, int32_t numOfBytes) override;

	public:
		bool Initialize(SOCKET socket, std::function<void(Session*, const char*, int32_t)> onPacketReceivedCallback, std::function<void(Session*)> onDisconnectCallback);
		void Disconnect();

	public:
		void Start();
		void Send(const char* data, int32_t len);

	public:
		int64_t GetSessionId() const { return _sessionId; }

	private:
		void PostRecv();

	private:
		void ProcessRecv(int32_t numOfBytes);
		void ProcessSend(Core::IocpEvent* event, int32_t numOfBytes);

	private:
		SOCKET _socket;
		bool _isConnected;
		int64_t _sessionId;

		Core::RecvEvent _recvEvent;
		Core::SendEvent _sendEvent;

		RecvBuffer _recvBuffer;

		std::function<void(Session*, const char*, int32_t)> _onPacketReceivedCallback;
		std::function<void(Session*)> _onDisconnectCallback;
	};
}