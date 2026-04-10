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
		bool Initialize(std::function<bool(Core::IocpObject*)> iocpObjectRegister, std::function<std::shared_ptr<Session>(SOCKET)> onConnectedCallback, const char* ip, uint16_t port);
		void Close();

	private:
		bool RegisterAccept();
		bool LoadAcceptEx();
		void ProcessAccept(Core::IocpEvent* event);

	private:
		std::function<bool(Core::IocpObject*)> _iocpObjectRegister;
		SOCKET _listenSocket = INVALID_SOCKET;
		LPFN_ACCEPTEX _acceptEx = nullptr;

		Core::AcceptEvent _acceptEvent;
		std::function<std::shared_ptr<Session>(SOCKET)> _createSession;
	};
}
