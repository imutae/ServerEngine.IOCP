#pragma once

#include <cstdint>
#include "IocpEvent.h"

namespace SE::Core
{
	struct IocpEvent;

	class IocpObject
	{
	public:
		virtual ~IocpObject() = default;

		virtual HANDLE GetHandle() = 0;

		virtual void Dispatch(
			IocpEvent* iocpEvent,
			int32_t numOfBytes,
			int32_t errorCode) = 0;
	};
}
