#pragma once

namespace SE::Core
{
    class IocpObject;

	class IocpCore
	{
    public:
        IocpCore();
        ~IocpCore();

    public:
        HANDLE GetHandle() const { return _iocpHandle; }

        bool Register(IocpObject* iocpObject);
        bool Dispatch(uint32_t timeoutMs = INFINITE);

    private:
        HANDLE _iocpHandle = INVALID_HANDLE_VALUE;
	};
}