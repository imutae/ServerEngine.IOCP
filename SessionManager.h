#pragma once

namespace SE::Net
{
	class Session;

    class SessionManager
    {
    public:
        std::shared_ptr<Session> CreateSession(SOCKET socket, std::function<void(Session*, const char*, int32_t)> onPacketReceivedCallback, std::function<void(Session*)> onDisconnectCallback);
        void RemoveSession(uint64_t sessionId);

    private:
        std::mutex _lock;

        std::unordered_map<uint64_t, std::shared_ptr<Session>> _sessionMap;
        std::atomic<uint64_t> _sessionIdGenerator = 1;
    };
}