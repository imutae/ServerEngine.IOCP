#include "pch.h"

#include "IocpCore.h"

#include "IocpObject.h"
#include "IocpEvent.h"

namespace SE::Core
{
	IocpCore::IocpCore()
		: _iocpHandle(INVALID_HANDLE_VALUE)
	{
		_iocpHandle = ::CreateIoCompletionPort(
			INVALID_HANDLE_VALUE,
			nullptr,
			0,
			0);
	}

	IocpCore::~IocpCore()
	{
		if ( _iocpHandle != nullptr && _iocpHandle != INVALID_HANDLE_VALUE )
		{
			::CloseHandle(_iocpHandle);
			_iocpHandle = INVALID_HANDLE_VALUE;
		}
	}

	bool IocpCore::Register(IocpObject* iocpObject)
	{
		if ( iocpObject == nullptr )
			return false;

		if ( _iocpHandle == nullptr || _iocpHandle == INVALID_HANDLE_VALUE )
			return false;

		HANDLE handle = iocpObject->GetHandle();

		if ( handle == nullptr || handle == INVALID_HANDLE_VALUE )
			return false;

		HANDLE result = ::CreateIoCompletionPort(
			handle,
			_iocpHandle,
			reinterpret_cast< ULONG_PTR >( iocpObject ),
			0);

		return result == _iocpHandle;
	}

	bool IocpCore::Dispatch(uint32_t timeoutMs)
	{
		if ( _iocpHandle == nullptr || _iocpHandle == INVALID_HANDLE_VALUE )
			return false;

		DWORD numOfBytes = 0;
		ULONG_PTR completionKey = 0;
		OVERLAPPED* overlapped = nullptr;

		BOOL ok = ::GetQueuedCompletionStatus(
			_iocpHandle,
			&numOfBytes,
			&completionKey,
			&overlapped,
			timeoutMs);

		const int32_t errorCode = ( ok == TRUE )
			? static_cast< int32_t >( ERROR_SUCCESS )
			: static_cast< int32_t >( ::GetLastError() );

		// overlapped == nullptr인 경우:
		// 1. timeout
		// 2. IOCP handle close
		// 3. PostShutdown()에서 넣은 종료 패킷
		// 이 경우에는 실제 IocpEvent가 없으므로 object dispatch를 하지 않는다.
		if ( overlapped == nullptr )
			return false;

		IocpObject* iocpObject = reinterpret_cast< IocpObject* >( completionKey );
		IocpEvent* iocpEvent = reinterpret_cast< IocpEvent* >( overlapped );

		if ( iocpObject == nullptr || iocpEvent == nullptr )
			return false;

		iocpObject->Dispatch(
			iocpEvent,
			static_cast< int32_t >( numOfBytes ),
			errorCode);

		return ok == TRUE;
	}

	bool IocpCore::PostShutdown(uint32_t count)
	{
		if ( _iocpHandle == nullptr || _iocpHandle == INVALID_HANDLE_VALUE )
			return false;

		bool result = true;

		for ( uint32_t i = 0; i < count; ++i )
		{
			BOOL ok = ::PostQueuedCompletionStatus(
				_iocpHandle,
				0,
				0,
				nullptr);

			if ( ok == FALSE )
				result = false;
		}

		return result;
	}
}
