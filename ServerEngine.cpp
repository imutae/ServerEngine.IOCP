#include "pch.h"

#include "ServerEngine.h"

#include <algorithm>

#include "ServerContext.h"
#include "IocpCore.h"
#include "Listener.h"
#include "SessionManager.h"
#include "Session.h"
#include "IServerLogic.h"

namespace SE
{
	ServerEngine::ServerEngine()
		: _logic(nullptr)
		, _context(nullptr)
		, _workerThreads()
		, _workerCount(std::max(1u, std::thread::hardware_concurrency()))
		, _running(false)
		, _wsaStarted(false)
	{
	}

	ServerEngine::~ServerEngine()
	{
		Shutdown();
	}

	bool ServerEngine::Initialize(IServerLogic* logic, const char* ip, uint16_t port)
	{
		if ( logic == nullptr || ip == nullptr )
			return false;

		if ( _logic != nullptr || _context != nullptr || _wsaStarted.load() )
			return false;

		if ( _running.load() )
			return false;

		WSADATA wsaData{};

		if ( ::WSAStartup(MAKEWORD(2, 2), &wsaData) != 0 )
			return false;

		std::unique_ptr<Internal::ServerContext> context;

		auto rollback = [ &context ] ()
			{
				context.reset();
				::WSACleanup();
			};

		context = std::make_unique<Internal::ServerContext>();

		context->iocpCore = std::make_shared<Core::IocpCore>();
		context->listener = std::make_shared<Net::Listener>();
		context->sessionManager = std::make_shared<Net::SessionManager>();

		bool listenerInitialized = context->listener->Initialize(
			[ this, ctx = context.get(), logic ] (SOCKET socket)
			{
				auto session = ctx->sessionManager->CreateSession(socket, logic);

				if ( session == nullptr )
				{
					::closesocket(socket);
					return;
				}

				if ( ctx->iocpCore->Register(session.get()) == false )
				{
					session->Disconnect();
					return;
				}

				logic->OnConnected(session.get());
				session->Start();
			},
			ip,
			port);

		if ( listenerInitialized == false )
		{
			rollback();
			return false;
		}

		if ( context->iocpCore->Register(context->listener.get()) == false )
		{
			rollback();
			return false;
		}

		if ( context->listener->StartAccept() == false )
		{
			rollback();
			return false;
		}

		_logic = logic;
		_context = std::move(context);
		_wsaStarted.store(true);

		return true;
	}

	void ServerEngine::Run()
	{
		if ( _context == nullptr )
			return;

		bool expected = false;

		if ( _running.compare_exchange_strong(expected, true) == false )
			return;

		StartWorkerThreads();
	}

	void ServerEngine::Shutdown()
	{
		const bool wasRunning = _running.exchange(false);

		if ( wasRunning && _context != nullptr && _context->iocpCore != nullptr )
		{
			_context->iocpCore->PostShutdown(
				static_cast< uint32_t >( _workerThreads.size() ));
		}

		JoinWorkerThreads();

		_context.reset();
		_logic = nullptr;

		if ( _wsaStarted.exchange(false) )
			::WSACleanup();
	}

	void ServerEngine::StartWorkerThreads()
	{
		const uint32_t threadCount = _workerCount;

		for ( uint32_t i = 0; i < threadCount; ++i )
		{
			_workerThreads.emplace_back([ this ] ()
				{
					WorkerThreadMain();
				});
		}
	}

	void ServerEngine::JoinWorkerThreads()
	{
		for ( auto& t : _workerThreads )
		{
			if ( t.joinable() )
				t.join();
		}

		_workerThreads.clear();
	}

	void ServerEngine::WorkerThreadMain()
	{
		while ( _running.load() )
		{
			if ( _context == nullptr || _context->iocpCore == nullptr )
				break;

			_context->iocpCore->Dispatch();
		}
	}
}
