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
		if ( logic == nullptr )
			return false;

		if ( _logic != nullptr || _context != nullptr )
			return false;

		if ( _running )
			return false;

		WSADATA wsaData;

		if ( ::WSAStartup(MAKEWORD(2, 2), &wsaData) != 0 )
		{
			return false;
		}

		_logic = logic;

		auto context = std::make_unique<Internal::ServerContext>();

		context->iocpCore = std::make_shared<Core::IocpCore>();
		context->listener = std::make_shared<Net::Listener>();
		context->sessionManager = std::make_shared<Net::SessionManager>();

		bool listenerInitialized = context->listener->Initialize(
			[ this, ctx = context.get() ] (SOCKET socket)
			{
				auto session = ctx->sessionManager->CreateSession(socket, _logic);

				if ( session == nullptr )
						{
					::closesocket(socket);
					return;
					}
				
					if ( !ctx->iocpCore->Register(session.get()) )
					{
						session->Disconnect();
						return;
					}

				_logic->OnConnected(session.get());
				session->Start();
			},
			ip,
			port
		);

		if ( listenerInitialized == false )
			return false; // TODO: throw exception으로 변환 필요. Main loop이기 때문에 초기화 실패 시 서버가 종료되어야 함.

		if ( context->iocpCore->Register(context->listener.get()) == false )
			return false;
		
		if ( context->listener->StartAccept() == false )
			return false;
		
		_context = std::move(context);

		return true;
	}

	void ServerEngine::Run()
	{
		_running = true;
		StartWorkerThreads();
	}

	void ServerEngine::Shutdown()
	{
		_running = false;
		JoinWorkerThreads();
		WSACleanup();
	}

	void ServerEngine::StartWorkerThreads()
	{
		uint32_t threadCount = std::thread::hardware_concurrency();

		for (uint32_t i = 0; i < threadCount; i++)
		{
			_workerThreads.emplace_back([this]()
				{
					WorkerThreadMain();
				});
		}
	}

	void ServerEngine::JoinWorkerThreads()
	{
		for (auto& t : _workerThreads)
		{
			if (t.joinable())
				t.join();
		}
	}

	void ServerEngine::WorkerThreadMain()
	{
		while (_running)
		{
			_context->iocpCore->Dispatch();
		}
	}
}
