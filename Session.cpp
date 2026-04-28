#include "pch.h"

#include "Session.h"

#include <cassert>
#include <limits>
#include <memory>
#include <utility>

#include "PacketHeader.h"
#include "IServerLogic.h"

namespace
{
	template <typename F>
	class ScopeExit
	{
	public:
		explicit ScopeExit(F&& f)
			: _f(std::forward<F>(f))
			, _active(true)
		{
		}

		~ScopeExit()
		{
			if ( _active )
				_f();
		}

		ScopeExit(const ScopeExit&) = delete;
		ScopeExit& operator=(const ScopeExit&) = delete;

		ScopeExit(ScopeExit&& other) noexcept
			: _f(std::move(other._f))
			, _active(other._active)
		{
			other._active = false;
		}

		ScopeExit& operator=(ScopeExit&&) = delete;

	private:
		F _f;
		bool _active;
	};

	template <typename F>
	ScopeExit<F> MakeScopeExit(F&& f)
	{
		return ScopeExit<F>(std::forward<F>(f));
	}
}

namespace SE::Net
{
	Session::Session(uint64_t sessionId)
		: _logic(nullptr)
		, _socket(INVALID_SOCKET)
		, _sessionId(sessionId)
		, _state(SessionState::Created)
		, _pendingIoCount(0)
		, _recvEvent()
		, _recvBuffer()
		, _sendQueue()
		, _sendPending(false)
	{
	}

	Session::~Session()
	{
		Disconnect();
	}

	HANDLE Session::GetHandle()
	{
		std::lock_guard<std::mutex> lock(_socketLock);
		return reinterpret_cast< HANDLE >( _socket );
	}

	void Session::Dispatch(
		Core::IocpEvent* event,
		int32_t numOfBytes,
		int32_t errorCode)
	{
		if ( event == nullptr )
			return;

		std::shared_ptr<Session> keepAlive;

		try
		{
			keepAlive = shared_from_this();
		}
		catch ( const std::bad_weak_ptr& )
		{
			assert(false && "Session is not managed by shared_ptr");
			return;
		}

		switch ( event->type )
		{
		case Core::EventType::Recv:
			ProcessRecv(numOfBytes, errorCode);
			break;

		case Core::EventType::Send:
			ProcessSend(event, numOfBytes, errorCode);
			break;

		default:
			assert(false && "Unknown session event type");
			break;
		}
	}

	bool Session::Initialize(SOCKET socket, IServerLogic* logic)
	{
		if ( socket == INVALID_SOCKET || logic == nullptr )
			return false;

		std::lock_guard<std::mutex> lock(_socketLock);

		if ( _state.load() != SessionState::Created )
			return false;

		_socket = socket;
		_logic = logic;
		_state.store(SessionState::Connected);

		return true;
	}

	void Session::SetOnClosed(std::function<void(uint64_t)> onClosed)
	{
		_onClosed = std::move(onClosed);
	}

	void Session::Disconnect()
	{
		std::shared_ptr<Session> keepAlive;

		try
		{
			keepAlive = shared_from_this();
		}
		catch ( const std::bad_weak_ptr& )
		{
			// destructor 경로에서는 shared_from_this()가 실패할 수 있다.
		}

		SOCKET socketToClose = INVALID_SOCKET;

		{
			std::lock_guard<std::mutex> lock(_socketLock);

			SessionState expected = SessionState::Connected;

			if ( _state.compare_exchange_strong(expected, SessionState::Closing) == false )
				return;

			socketToClose = _socket;
			_socket = INVALID_SOCKET;

			ClearSendQueueLocked();
		}

		if ( socketToClose != INVALID_SOCKET )
		{
			::shutdown(socketToClose, SD_BOTH);
			::closesocket(socketToClose);
		}

		if ( _logic != nullptr )
			_logic->OnDisconnected(this);

		TryFinalizeClose();
	}

	void Session::Start()
	{
		std::shared_ptr<Session> keepAlive;

		try
		{
			keepAlive = shared_from_this();
		}
		catch ( const std::bad_weak_ptr& )
		{
			assert(false && "Session is not managed by shared_ptr");
			return;
		}

		PostRecv();
	}

	bool Session::Send(uint16_t packetId, const void* body, int32_t len)
	{
		std::shared_ptr<Session> keepAlive;

		try
		{
			keepAlive = shared_from_this();
		}
		catch ( const std::bad_weak_ptr& )
		{
			assert(false && "Session is not managed by shared_ptr");
			return false;
		}

		if ( body == nullptr || len <= 0 )
			return false;

		constexpr size_t headerSize = sizeof(Packet::PacketHeader);
		static_assert( headerSize <= std::numeric_limits<uint16_t>::max() );

		const size_t maxBodySize = std::numeric_limits<uint16_t>::max() - headerSize;

		if ( static_cast< size_t >( len ) > maxBodySize )
			return false;

		const uint16_t packetSize =
			static_cast< uint16_t >( headerSize + static_cast< size_t >( len ) );

		auto sendBuffer = std::make_shared<std::vector<char>>(packetSize);

		Packet::PacketHeader header;
		header.size = packetSize;
		header.id = packetId;

		::memcpy(sendBuffer->data(), &header, sizeof(header));
		::memcpy(sendBuffer->data() + sizeof(header), body, len);

		bool shouldCompletePendingIo = false;
		bool shouldDisconnect = false;

		{
			std::lock_guard<std::mutex> lock(_socketLock);

			if ( _state.load() != SessionState::Connected || _socket == INVALID_SOCKET )
				return false;

			_sendQueue.push_back(std::move(sendBuffer));

			if ( _sendPending == false )
			{
				if ( PostSendLocked(0, shouldCompletePendingIo) == false )
				{
					ClearSendQueueLocked();
					shouldDisconnect = true;
				}
			}
		}

		if ( shouldCompletePendingIo )
			CompletePendingIo();

		if ( shouldDisconnect )
		{
			Disconnect();
			return false;
		}

		return true;
	}

	void Session::PostRecv()
	{
		bool shouldCompletePendingIo = false;
		bool shouldDisconnect = false;

		{
			std::lock_guard<std::mutex> lock(_socketLock);

			if ( _state.load() != SessionState::Connected || _socket == INVALID_SOCKET )
				return;

			_recvBuffer.Clean();

			if ( _recvBuffer.FreeSize() == 0 )
			{
				// TODO: 버퍼 확장 or 에러 처리
				shouldDisconnect = true;
			}
			else
			{
				::ZeroMemory(static_cast< OVERLAPPED* >( &_recvEvent ), sizeof(OVERLAPPED));

				_recvEvent.type = Core::EventType::Recv;
				_recvEvent.wsaBuf.buf = _recvBuffer.WritePos();
				_recvEvent.wsaBuf.len = _recvBuffer.FreeSize();

				AddPendingIo();

				DWORD flags = 0;
				DWORD numOfBytes = 0;

				int ret = ::WSARecv(
					_socket,
					&_recvEvent.wsaBuf,
					1,
					&numOfBytes,
					&flags,
					reinterpret_cast< OVERLAPPED* >( &_recvEvent ),
					nullptr);

				if ( ret == SOCKET_ERROR )
				{
					int err = ::WSAGetLastError();

					if ( err != WSA_IO_PENDING )
					{
						shouldCompletePendingIo = true;
						shouldDisconnect = true;
					}
				}
			}
		}

		if ( shouldCompletePendingIo )
			CompletePendingIo();

		if ( shouldDisconnect )
			Disconnect();
	}

	void Session::ProcessRecv(int32_t numOfBytes, int32_t errorCode)
	{
		auto completeIo = MakeScopeExit([ this ] ()
			{
				CompletePendingIo();
			});

		if ( errorCode != ERROR_SUCCESS )
		{
			Disconnect();
			return;
		}

		if ( numOfBytes == 0 )
		{
			Disconnect();
			return;
		}

		if ( _recvBuffer.OnWrite(numOfBytes) == false )
		{
			Disconnect();
			return;
		}

		while ( true )
		{
			if ( _recvBuffer.DataSize() < static_cast< int32_t >( sizeof(Packet::PacketHeader) ) )
				break;

			Packet::PacketHeader header;
			::memcpy(&header, _recvBuffer.ReadPos(), sizeof(Packet::PacketHeader));

			const int32_t packetSize = static_cast< int32_t >( header.size );
			const int32_t headerSize = static_cast< int32_t >( sizeof(Packet::PacketHeader) );

			if ( packetSize < headerSize )
			{
				Disconnect();
				return;
			}

			if ( _recvBuffer.DataSize() < packetSize )
				break;

			const char* body = _recvBuffer.ReadPos() + headerSize;
			const int32_t bodyLen = packetSize - headerSize;

			if ( _logic == nullptr )
			{
				Disconnect();
				return;
			}

			_logic->DispatchPacket(this, header.id, body, bodyLen);

			if ( IsConnected() == false )
				return;

			if ( _recvBuffer.OnRead(packetSize) == false )
			{
				Disconnect();
				return;
			}
		}

		PostRecv();
	}

	void Session::ProcessSend(
		Core::IocpEvent* event,
		int32_t numOfBytes,
		int32_t errorCode)
	{
		auto completeIo = MakeScopeExit([ this ] ()
			{
				CompletePendingIo();
			});

		std::unique_ptr<Core::SendEvent> sendEvent(static_cast< Core::SendEvent* >( event ));

		bool shouldCompletePendingIo = false;
		bool shouldDisconnect = false;

		{
			std::lock_guard<std::mutex> lock(_socketLock);

			if ( _state.load() != SessionState::Connected || _socket == INVALID_SOCKET )
			{
				ClearSendQueueLocked();
				return;
			}

			if ( errorCode != ERROR_SUCCESS )
			{
				ClearSendQueueLocked();
				shouldDisconnect = true;
			}
			else if ( numOfBytes <= 0 )
			{
				ClearSendQueueLocked();
				shouldDisconnect = true;
			}
			else if ( sendEvent->buffer == nullptr )
			{
				ClearSendQueueLocked();
				shouldDisconnect = true;
			}
			else
			{
				const size_t bufferSize = sendEvent->buffer->size();
				const size_t offset = sendEvent->offset;

				if ( offset >= bufferSize )
				{
					ClearSendQueueLocked();
					shouldDisconnect = true;
				}
				else
				{
					const size_t remainingSize = bufferSize - offset;
					const size_t sentSize = static_cast< size_t >( numOfBytes );

					if ( sentSize > remainingSize )
					{
						ClearSendQueueLocked();
						shouldDisconnect = true;
					}
					else
					{
						const size_t nextOffset = offset + sentSize;

						if ( _sendQueue.empty() || _sendQueue.front() != sendEvent->buffer )
						{
							ClearSendQueueLocked();
							shouldDisconnect = true;
						}
						else if ( nextOffset < bufferSize )
						{
							// Partial send.
							// 같은 buffer의 남은 구간을 다시 WSASend로 post한다.
							if ( PostSendLocked(nextOffset, shouldCompletePendingIo) == false )
							{
								ClearSendQueueLocked();
								shouldDisconnect = true;
							}
						}
						else
						{
							// Current packet send completed.
							_sendQueue.pop_front();

							if ( _sendQueue.empty() )
							{
								_sendPending = false;
							}
							else
							{
								if ( PostSendLocked(0, shouldCompletePendingIo) == false )
								{
									ClearSendQueueLocked();
									shouldDisconnect = true;
								}
							}
						}
					}
				}
			}
		}

		if ( shouldCompletePendingIo )
			CompletePendingIo();

		if ( shouldDisconnect )
			Disconnect();
	}

	bool Session::PostSendLocked(size_t offset, bool& shouldCompletePendingIo)
	{
		shouldCompletePendingIo = false;

		if ( _state.load() != SessionState::Connected || _socket == INVALID_SOCKET )
			return false;

		if ( _sendQueue.empty() )
			return false;

		const std::shared_ptr<std::vector<char>>& buffer = _sendQueue.front();

		if ( buffer == nullptr )
			return false;

		if ( offset >= buffer->size() )
			return false;

		auto sendEvent = std::make_unique<Core::SendEvent>();

		sendEvent->type = Core::EventType::Send;
		sendEvent->buffer = buffer;
		sendEvent->offset = offset;
		sendEvent->wsaBuf.buf = buffer->data() + offset;
		sendEvent->wsaBuf.len = static_cast< ULONG >( buffer->size() - offset );

		AddPendingIo();

		DWORD numOfBytes = 0;

		int ret = ::WSASend(
			_socket,
			&sendEvent->wsaBuf,
			1,
			&numOfBytes,
			0,
			reinterpret_cast< OVERLAPPED* >( sendEvent.get() ),
			nullptr);

		if ( ret == SOCKET_ERROR )
		{
			const int err = ::WSAGetLastError();

			if ( err != WSA_IO_PENDING )
			{
				shouldCompletePendingIo = true;
				return false;
			}
		}

		_sendPending = true;
		sendEvent.release();

		return true;
	}

	void Session::ClearSendQueueLocked()
	{
		_sendQueue.clear();
		_sendPending = false;
	}

	void Session::AddPendingIo()
	{
		_pendingIoCount.fetch_add(1);
	}

	void Session::CompletePendingIo()
	{
		const int32_t previous = _pendingIoCount.fetch_sub(1);

		assert(previous > 0);

		if ( previous <= 0 )
			return;

		if ( previous == 1 )
			TryFinalizeClose();
	}

	void Session::TryFinalizeClose()
	{
		if ( _pendingIoCount.load() != 0 )
			return;

		SessionState expected = SessionState::Closing;

		if ( _state.compare_exchange_strong(expected, SessionState::Closed) == false )
			return;

		if ( _onClosed )
			_onClosed(_sessionId);
	}

	bool Session::IsConnected() const
	{
		return _state.load() == SessionState::Connected;
	}
}
