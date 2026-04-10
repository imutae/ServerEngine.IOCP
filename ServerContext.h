#pragma once
#include <memory>

namespace SE
{
	namespace Core
	{
		class IocpCore;
	}

	namespace Net
	{
		class Listener;
		class SessionManager;
	}

	namespace Internal
	{
		struct ServerContext
		{
			std::shared_ptr<Core::IocpCore> iocpCore;
			std::shared_ptr<Net::Listener> listener;
			std::shared_ptr<Net::SessionManager> sessionManager;
		};
	}
}