#include "pch.h"
#include "ServerEngine.h"
#include "ServerContext.h"
#include "IocpCore.h"
#include "Listener.h"
#include "SessionManager.h"
#include "Session.h"
#include "IServerLogic.h"

namespace SE {
	ServerEngine::ServerEngine() 
		: _logic(nullptr)
		, _context(nullptr)
		, _workerThreads()
		, _workerCount(std::max(1u, std::thread::hardware_concurrency()))
		, _running(false) 
	{

	}

	ServerEngine::~ServerEngine()
	{
		Shutdown();
	}

	bool ServerEngine::Initialize(IServerLogic* logic, const char* ip, uint16_t port)
	{
		if(logic == nullptr)
			return false;

		if (_logic != nullptr || _context != nullptr)
			return false;

		if (_running)
			return false;

		_logic = logic;

		auto context = std::make_unique<Internal::ServerContext>();
		context->iocpCore = std::make_shared<Core::IocpCore>();
		context->listener = std::make_shared<Net::Listener>();
		context->sessionManager = std::make_shared<Net::SessionManager>();

		context->listener->Initialize(
			[ctx = context.get()](Core::IocpObject* obj) -> bool
			{
				return ctx->iocpCore->Register(obj);
			},
			[this, ctx = context.get()](SOCKET socket) -> std::shared_ptr<Net::Session>
			{
				auto session = ctx->sessionManager->CreateSession(
					socket,
					[this](Net::Session* session, const char* data, int32_t len)
					{
						if (_logic)
							_logic->DispatchPacket(session, data, len);
					},
					[this](Net::Session* session)
					{
						if (_logic)
							_logic->OnDisconnected(session);
					}
				);

				return session;
			},
			ip,
			port
		);

		_context = std::move(context);

		return true;
	}

	void ServerEngine::Run()
	{
		_running = true;

	}

	void ServerEngine::Shutdown()
	{
		_running = false;
		for (auto& thread : _workerThreads)
		{
			if (thread.joinable())
				thread.join();
		}
	}

	bool ServerEngine::StartWorkerThreads()
	{
		return false;
	}

	void ServerEngine::JoinWorkerThreads()
	{
	}

	void ServerEngine::WorkerThreadMain()
	{
	}
}