#pragma once

#include <atomic>
#include <cstdint>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <vector>

#include "IocpObject.h"
#include "IocpEvent.h"
#include "RecvBuffer.h"

namespace SE
{
	class IServerLogic;
}

namespace SE::Net
{
	class Session
		: public Core::IocpObject
		, public std::enable_shared_from_this<Session>
	{
	private:
		enum class SessionState : uint8_t
		{
			Created,
			Connected,
			Closing,
			Closed
		};

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

		void SetOnClosed(std::function<void(uint64_t)> onClosed);

	private:
		void PostRecv();

	private:
		void ProcessRecv(int32_t numOfBytes, int32_t errorCode);
		void ProcessSend(Core::IocpEvent* event, int32_t numOfBytes, int32_t errorCode);

	private:
		bool PostSendLocked(size_t offset, bool& shouldCompletePendingIo);
		void ClearSendQueueLocked();

	private:
		void AddPendingIo();
		void CompletePendingIo();
		void TryFinalizeClose();

		bool IsConnected() const;

	private:
		IServerLogic* _logic;
		SOCKET _socket;
		const uint64_t _sessionId;

		std::atomic<SessionState> _state;
		std::atomic<int32_t> _pendingIoCount;

		std::mutex _socketLock;

		std::function<void(uint64_t)> _onClosed;

		Core::RecvEvent _recvEvent;
		RecvBuffer _recvBuffer;

		std::deque<std::shared_ptr<std::vector<char>>> _sendQueue;
		bool _sendPending;
	};
}
