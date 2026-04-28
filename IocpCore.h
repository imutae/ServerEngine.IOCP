#pragma once

#include <cstdint>

namespace SE::Core
{
	class IocpObject;

	class IocpCore
	{
	public:
		IocpCore();
		~IocpCore();

		IocpCore(const IocpCore&) = delete;
		IocpCore& operator=(const IocpCore&) = delete;
		IocpCore(IocpCore&&) = delete;
		IocpCore& operator=(IocpCore&&) = delete;

	public:
		HANDLE GetHandle() const { return _iocpHandle; }

		bool Register(IocpObject* iocpObject);
		bool Dispatch(uint32_t timeoutMs = INFINITE);

		bool PostShutdown(uint32_t count);

	private:
		HANDLE _iocpHandle = INVALID_HANDLE_VALUE;
	};
}
