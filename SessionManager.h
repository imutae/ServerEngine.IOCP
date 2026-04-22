#pragma once

namespace SE::Net
{
	class Session;

    class SessionManager
    {
    public:
        SessionManager() = default;
        ~SessionManager() = default;

        SessionManager(const SessionManager&) = delete;
        SessionManager& operator=(const SessionManager&) = delete;

        SessionManager(SessionManager&&) = delete;
        SessionManager& operator=(SessionManager&&) = delete;

    public:
        std::shared_ptr<Session> CreateSession(SOCKET socket, IServerLogic* logic);
        void RemoveSession(uint64_t sessionId);

    private:
        std::mutex _lock;

        std::unordered_map<uint64_t, std::shared_ptr<Session>> _sessionMap;
        std::atomic<uint64_t> _sessionIdGenerator = 1;
    };
}