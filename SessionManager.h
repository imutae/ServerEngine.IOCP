#pragma once

#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <unordered_map>

namespace SE
{
	class IServerLogic;
}

namespace SE::Net
{
	class Session;

	class SessionManager : public std::enable_shared_from_this<SessionManager>
	{
	public:
		SessionManager();
		~SessionManager();

		SessionManager(const SessionManager&) = delete;
		SessionManager& operator=(const SessionManager&) = delete;
		SessionManager(SessionManager&&) = delete;
		SessionManager& operator=(SessionManager&&) = delete;

	public:
		std::shared_ptr<Session> CreateSession(SOCKET socket, IServerLogic* logic);
		void RemoveSession(uint64_t sessionId);

	private:
		std::atomic<uint64_t> _sessionIdGenerator;
		std::mutex _lock;
		std::unordered_map<uint64_t, std::shared_ptr<Session>> _sessionMap;
	};
}
