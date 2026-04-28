#include "pch.h"

#include "SessionManager.h"

#include "Session.h"
#include "IServerLogic.h"

namespace SE::Net
{
	SessionManager::SessionManager()
		: _sessionIdGenerator(1)
		, _sessionMap()
	{
	}

	SessionManager::~SessionManager()
	{
	}

	std::shared_ptr<Session> SessionManager::CreateSession(SOCKET socket, IServerLogic* logic)
	{
		if ( socket == INVALID_SOCKET || logic == nullptr )
			return nullptr;

		const uint64_t sessionId = _sessionIdGenerator.fetch_add(1);

		auto session = std::make_shared<Session>(sessionId);

		std::weak_ptr<SessionManager> weakManager = weak_from_this();

		session->SetOnClosed(
			[ weakManager ] (uint64_t closedSessionId)
			{
				if ( auto manager = weakManager.lock() )
					manager->RemoveSession(closedSessionId);
			});

		if ( session->Initialize(socket, logic) == false )
			return nullptr;

		{
			std::lock_guard<std::mutex> lock(_lock);

			auto [it, inserted] = _sessionMap.emplace(sessionId, session);

			if ( inserted == false )
				return nullptr;
		}

		return session;
	}

	void SessionManager::RemoveSession(uint64_t sessionId)
	{
		std::shared_ptr<Session> removedSession;

		{
			std::lock_guard<std::mutex> lock(_lock);

			auto it = _sessionMap.find(sessionId);

			if ( it == _sessionMap.end() )
				return;

			removedSession = std::move(it->second);
			_sessionMap.erase(it);
		}
	}
}
