#include "pch.h"
#include "Session.h"
#include "PacketHeader.h"
#include "IServerLogic.h"

namespace SE::Net
{
	Session::Session(uint64_t sessionId)
	{
		_logic = nullptr;
		_socket = INVALID_SOCKET;
		_isConnected = false;
		_sessionId = sessionId;
	}

	Session::~Session()
	{
		Disconnect();
	}

	HANDLE Session::GetHandle()
	{
		return reinterpret_cast<HANDLE>(_socket);
	}

	void Session::Dispatch(Core::IocpEvent* event, int32_t numOfBytes)
	{
		switch (event->type)
		{
		case Core::EventType::Recv:
			ProcessRecv(numOfBytes);
			break;
		case Core::EventType::Send:
			ProcessSend(event, numOfBytes);
			break;
		default:
			assert(false && "Unknown session event type");
			break;
		}
	}

	bool Session::Initialize(SOCKET socket, IServerLogic* logic)
	{
		if (socket == INVALID_SOCKET || !logic)
			return false;

		_socket = socket;
		_logic = logic;

		_isConnected = true;

		return _isConnected;
	}

	void Session::Disconnect()
	{
		if (_isConnected == false)
			return;

		_isConnected = false;

		if (_socket != INVALID_SOCKET)
		{
			_logic->OnDisconnected(this);
			::shutdown(_socket, SD_BOTH);
			closesocket(_socket);
			_socket = INVALID_SOCKET;
		}
	}

	void Session::PostRecv()
	{
		if (_socket == INVALID_SOCKET || _isConnected == false)
			return;

		_recvBuffer.Clean();

		if (_recvBuffer.FreeSize() == 0)
		{
			// TODO: 버퍼 확장 or 에러 처리
			Disconnect();
			return;
		}

		_recvEvent.wsaBuf.buf = _recvBuffer.WritePos();
		_recvEvent.wsaBuf.len = _recvBuffer.FreeSize();

		DWORD flags = 0;
		DWORD numOfBytes = 0;

		int ret = ::WSARecv(
			_socket,
			&_recvEvent.wsaBuf,
			1,
			&numOfBytes,
			&flags,
			reinterpret_cast<OVERLAPPED*>(&_recvEvent),
			nullptr
		);

		if (ret == SOCKET_ERROR)
		{
			int err = ::WSAGetLastError();
			if (err != WSA_IO_PENDING)
			{
				Disconnect();
				return;
			}
		}
	}

	void Session::Start()
	{
		PostRecv();
	}

	bool Session::Send(uint16_t packetId, const void* body, int32_t len)
	{
		if (_socket == INVALID_SOCKET || body == nullptr || len <= 0 || _isConnected == false)
			return false;

		const uint16_t packetSize =
			static_cast<uint16_t>(sizeof(Packet::PacketHeader) + len);

		Core::SendEvent* sendEvent = new Core::SendEvent();

		sendEvent->buffer = std::make_shared<std::vector<char>>(packetSize);

		Packet::PacketHeader header;
		header.size = packetSize;
		header.id = packetId;

		::memcpy(sendEvent->buffer->data(), &header, sizeof(header));
		::memcpy(sendEvent->buffer->data() + sizeof(header), body, len);

		sendEvent->wsaBuf.buf = sendEvent->buffer->data();
		sendEvent->wsaBuf.len = static_cast<ULONG>(sendEvent->buffer->size());

		DWORD numOfBytes = 0;
		int ret = ::WSASend(
			_socket,
			&sendEvent->wsaBuf,
			1,
			&numOfBytes,
			0,
			reinterpret_cast<OVERLAPPED*>(sendEvent),
			nullptr
		);

		if (ret == SOCKET_ERROR)
		{
			const int err = ::WSAGetLastError();
			if (err != WSA_IO_PENDING)
			{
				delete sendEvent;
				Disconnect();
				return false;
			}
		}

		return true;
	}

	void Session::ProcessRecv(int32_t numOfBytes)
	{
		if (numOfBytes == 0)
		{
			Disconnect();
			return;
		}

		if (_recvBuffer.OnWrite(numOfBytes) == false)
		{
			Disconnect();
			return;
		}

		while (true)
		{
			if (_recvBuffer.DataSize() < sizeof(Packet::PacketHeader))
				break;

			Packet::PacketHeader header;
			::memcpy(&header, _recvBuffer.ReadPos(), sizeof(Packet::PacketHeader));

			if (header.size < sizeof(Packet::PacketHeader))
			{
				Disconnect();
				return;
			}

			if (_recvBuffer.DataSize() < header.size)
				break;

			const char* body = _recvBuffer.ReadPos() + sizeof(Packet::PacketHeader);
			const int32_t bodyLen = header.size - static_cast<int32_t>(sizeof(Packet::PacketHeader));

			_logic->DispatchPacket(this, header.id, body, bodyLen);

			if (_recvBuffer.OnRead(header.size) == false)
			{
				Disconnect();
				return;
			}
		}

		PostRecv();
	}

	void Session::ProcessSend(Core::IocpEvent* event, int32_t numOfBytes)
	{
		Core::SendEvent* sendEvent = static_cast<Core::SendEvent*>(event);

		if (numOfBytes <= 0)
		{
			delete sendEvent;
			Disconnect();
			return;
		}

		if (sendEvent->buffer == nullptr)
		{
			delete sendEvent;
			Disconnect();
			return;
		}

		if (numOfBytes != static_cast<int32_t>(sendEvent->buffer->size()))
		{
			// TODO: 부분 송신 재전송 처리
			delete sendEvent;
			Disconnect();
			return;
		}

		delete sendEvent;
	}
}