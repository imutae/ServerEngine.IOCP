#pragma once

namespace SE {

	namespace Internal {
		struct ServerContext;
	}

	class IServerLogic;

	class ServerEngine
	{
	public:
		ServerEngine();
		~ServerEngine();

		ServerEngine(const ServerEngine&) = delete;
		ServerEngine& operator=(const ServerEngine&) = delete;

		ServerEngine(ServerEngine&&) = delete;
		ServerEngine& operator=(ServerEngine&&) = delete;

	public:
		bool Initialize(IServerLogic* logic, const char* ip, uint16_t port);
		void Run();
		void Shutdown();

	private:
		bool StartWorkerThreads();
		void JoinWorkerThreads();
		void WorkerThreadMain();

	private:
		IServerLogic* _logic;
		std::unique_ptr<Internal::ServerContext> _context;

		std::vector<std::thread> _workerThreads;
		uint32_t _workerCount;

		std::atomic<bool> _running;
	};
}