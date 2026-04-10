#include "pch.h"
#include "IocpCore.h"
#include "IocpObject.h"
#include "IocpEvent.h"

namespace SE::Core
{
	IocpCore::IocpCore() : _iocpHandle(INVALID_HANDLE_VALUE)
	{
		_iocpHandle = ::CreateIoCompletionPort(INVALID_HANDLE_VALUE, nullptr, 0, 0);
	}

	IocpCore::~IocpCore()
	{
		if (_iocpHandle != nullptr && _iocpHandle != INVALID_HANDLE_VALUE)
		{
			::CloseHandle(_iocpHandle);
			_iocpHandle = INVALID_HANDLE_VALUE;
		}
	}

	bool IocpCore::Register(IocpObject* iocpObject)
	{
		if (iocpObject == nullptr)
			return false;

		HANDLE handle = iocpObject->GetHandle();
		if (handle == nullptr || handle == INVALID_HANDLE_VALUE)
			return false;

		HANDLE result = ::CreateIoCompletionPort(
			handle,
			_iocpHandle,
			reinterpret_cast<ULONG_PTR>(iocpObject),
			0
		);

		return (result != nullptr);
	}

	bool IocpCore::Dispatch(uint32_t timeoutMs)
	{
		DWORD numOfBytes = 0;
		ULONG_PTR completionKey = 0;
		OVERLAPPED* overlapped = nullptr;

		BOOL ok = ::GetQueuedCompletionStatus(
			_iocpHandle,
			&numOfBytes,
			&completionKey,
			&overlapped,
			timeoutMs
		);

		if (overlapped == nullptr)
		{
			return false;
		}

		IocpObject* iocpObject = reinterpret_cast<IocpObject*>(completionKey);
		IocpEvent* iocpEvent = reinterpret_cast<IocpEvent*>(overlapped);

		if (iocpObject == nullptr || iocpEvent == nullptr)
		{
			return false;
		}

		if (ok == FALSE)
		{
			const int32_t errorCode = ::GetLastError();

			if (errorCode == WAIT_TIMEOUT)
				return false;

			if (numOfBytes == 0)
			{
				iocpObject->Dispatch(iocpEvent, 0);
				return false;
			}

			iocpObject->Dispatch(iocpEvent, static_cast<int32_t>(numOfBytes));
			return false;
		}

		iocpObject->Dispatch(iocpEvent, static_cast<int32_t>(numOfBytes));
		return true;
	}
}

