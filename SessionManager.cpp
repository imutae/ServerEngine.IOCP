#include "pch.h"
#include "SessionManager.h"
#include "Session.h"
#include "IServerLogic.h"

namespace SE::Net
{
    std::shared_ptr<Session> SessionManager::CreateSession(SOCKET socket, IServerLogic* logic)
    {
		uint64_t sessionId = _sessionIdGenerator.fetch_add(1);

		auto session = std::make_shared<Session>(sessionId);
        {
            std::lock_guard<std::mutex> lock(_lock);
            _sessionMap.emplace(sessionId, session);
        }

		session->Initialize(socket, logic);

        return session;
    }

    void SessionManager::RemoveSession(uint64_t sessionId)
    {
        std::shared_ptr<Session> session;

        {
            std::lock_guard<std::mutex> lock(_lock);
            auto it = _sessionMap.find(sessionId);
            if (it == _sessionMap.end())
                return;

            session = it->second;
            _sessionMap.erase(it);
        }
    }
}