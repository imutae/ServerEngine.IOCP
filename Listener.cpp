#include "pch.h"

#include "Listener.h"
#include "Session.h"

namespace SE::Net
{
	Listener::Listener()
	{
	}

	Listener::~Listener()
	{
		Close();
	}

	HANDLE Listener::GetHandle()
	{
		return reinterpret_cast< HANDLE >( _listenSocket );
	}

	void Listener::Dispatch(Core::IocpEvent* event, int32_t numOfBytes)
	{
		UNREFERENCED_PARAMETER(numOfBytes);

		switch ( event->type )
		{
		case Core::EventType::Accept:
			ProcessAccept(event);
			break;

		default:
			assert(false && "Unknown listener event type");
			break;
		}
	}

	bool Listener::Initialize(std::function<void(SOCKET)> onAccept, const char* ip, uint16_t port)
	{
		if ( !onAccept || ip == nullptr )
			return false;

		_createSession = onAccept;

		_listenSocket = ::WSASocket(
			AF_INET,
			SOCK_STREAM,
			IPPROTO_TCP,
			nullptr,
			0,
			WSA_FLAG_OVERLAPPED
		);

		if ( _listenSocket == INVALID_SOCKET )
		{
			return false;
		}

		SOCKADDR_IN addr{};
		addr.sin_family = AF_INET;
		addr.sin_port = ::htons(port);

		if ( ::inet_pton(AF_INET, ip, &addr.sin_addr) != 1 )
		{
			// TODO: 에러 처리 추가
			Close();
			return false;
		}

		if ( ::bind(_listenSocket, reinterpret_cast< SOCKADDR* >( &addr ), sizeof(addr)) == SOCKET_ERROR )
		{
			Close();
			return false;
		}

		if ( LoadAcceptEx() == false )
		{
			Close();
			return false;
		}

		if ( ::listen(_listenSocket, SOMAXCONN) == SOCKET_ERROR )
		{
			Close();
			return false;
		}
		return true;
	}

	bool Listener::StartAccept()
	{
		if ( _listenSocket == INVALID_SOCKET || _acceptEx == nullptr )
			return false;
		
		return RegisterAccept();
	}
	
	bool Listener::LoadAcceptEx()
	{
		DWORD bytes = 0;
		GUID guid = WSAID_ACCEPTEX;

		int ret = ::WSAIoctl(
			_listenSocket,
			SIO_GET_EXTENSION_FUNCTION_POINTER,
			&guid,
			sizeof(guid),
			&_acceptEx,
			sizeof(_acceptEx),
			&bytes,
			nullptr,
			nullptr
		);

		if ( ret == SOCKET_ERROR )
			return false;

		return _acceptEx != nullptr;
	}

	bool Listener::RegisterAccept()
	{
		::ZeroMemory(static_cast< OVERLAPPED* >( &_acceptEvent ), sizeof(OVERLAPPED));

		_acceptEvent.acceptSocket = ::WSASocket(
			AF_INET,
			SOCK_STREAM,
			IPPROTO_TCP,
			nullptr,
			0,
			WSA_FLAG_OVERLAPPED
		);

		if ( _acceptEvent.acceptSocket == INVALID_SOCKET )
			return false;

		DWORD bytes = 0;

		BOOL ok = _acceptEx(
			_listenSocket,
			_acceptEvent.acceptSocket,
			_acceptEvent.buffer,
			0,
			sizeof(SOCKADDR_IN) + 16,
			sizeof(SOCKADDR_IN) + 16,
			&bytes,
			reinterpret_cast< OVERLAPPED* >( &_acceptEvent )
		);

		if ( ok == FALSE )
		{
			int err = ::WSAGetLastError();

			if ( err != WSA_IO_PENDING )
			{
				::closesocket(_acceptEvent.acceptSocket);
				_acceptEvent.acceptSocket = INVALID_SOCKET;
				return false;
			}
		}

		return true;
	}

	void Listener::ProcessAccept(Core::IocpEvent* event)
	{
		auto acceptEvent = static_cast< Core::AcceptEvent* >( event );

		SOCKET clientSocket = acceptEvent->acceptSocket;
		acceptEvent->acceptSocket = INVALID_SOCKET;

		if ( clientSocket == INVALID_SOCKET )
		{
			assert(false && "Invalid accepted socket");
			return;
		}

		if ( ::setsockopt(
			clientSocket,
			SOL_SOCKET,
			SO_UPDATE_ACCEPT_CONTEXT,
			reinterpret_cast< const char* >( &_listenSocket ),
			sizeof(_listenSocket)) == SOCKET_ERROR )
		{
			::closesocket(clientSocket);

			if ( RegisterAccept() == false )
			{
				assert(false && "RegisterAccept failed");
			}

			return;
		}

		_createSession(clientSocket);

		if ( RegisterAccept() == false )
		{
			assert(false && "RegisterAccept failed");
		}
	}

	void Listener::Close()
	{
		if ( _acceptEvent.acceptSocket != INVALID_SOCKET )
		{
			::closesocket(_acceptEvent.acceptSocket);
			_acceptEvent.acceptSocket = INVALID_SOCKET;
		}

		if ( _listenSocket != INVALID_SOCKET )
		{
			::closesocket(_listenSocket);
			_listenSocket = INVALID_SOCKET;
		}
	}
}
