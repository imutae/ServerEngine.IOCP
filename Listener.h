#pragma once

#include "IocpObject.h"
#include "IocpEvent.h"

namespace SE::Net
{
	class Session;

	class Listener : public Core::IocpObject
	{
	public:
		Listener();
		~Listener() override;

		Listener(const Listener&) = delete;
		Listener& operator=(const Listener&) = delete;
		Listener(Listener&&) = delete;
		Listener& operator=(Listener&&) = delete;

	public:
		HANDLE GetHandle() override;
		void Dispatch(Core::IocpEvent* event, int32_t numOfBytes) override;

	public:
		bool Initialize(
			std::function<void(SOCKET)> onAccept,
			const char* ip,
			uint16_t port);

		bool StartAccept();
		void Close();

	private:
		bool RegisterAccept();
		bool LoadAcceptEx();
		void ProcessAccept(Core::IocpEvent* event);

	private:
		SOCKET _listenSocket = INVALID_SOCKET;
		LPFN_ACCEPTEX _acceptEx = nullptr;
		Core::AcceptEvent _acceptEvent;
		std::function<void(SOCKET)> _createSession;
	};
}
