#include "pch.h"
#include "RecvBuffer.h"

#include <cassert>
#include <cstring>

namespace SE::Net
{
	RecvBuffer::RecvBuffer(int32_t bufferSize)
		: _buffer(bufferSize)
	{
	}

	char* RecvBuffer::ReadPos()
	{
		return _buffer.data() + _readPos;
	}

	char* RecvBuffer::WritePos()
	{
		return _buffer.data() + _writePos;
	}

	int32_t RecvBuffer::DataSize() const
	{
		return _writePos - _readPos;
	}

	int32_t RecvBuffer::FreeSize() const
	{
		return static_cast<int32_t>(_buffer.size()) - _writePos;
	}

	bool RecvBuffer::OnRead(int32_t numOfBytes)
	{
		if (numOfBytes < 0 || DataSize() < numOfBytes)
			return false;

		_readPos += numOfBytes;

		if (_readPos == _writePos)
		{
			_readPos = 0;
			_writePos = 0;
		}

		return true;
	}

	bool RecvBuffer::OnWrite(int32_t numOfBytes)
	{
		if (numOfBytes < 0 || FreeSize() < numOfBytes)
			return false;

		_writePos += numOfBytes;
		return true;
	}

	void RecvBuffer::Clean()
	{
		const int32_t dataSize = DataSize();

		if (dataSize == 0)
		{
			_readPos = 0;
			_writePos = 0;
			return;
		}

		if (_readPos == 0)
			return;

		::memmove(_buffer.data(), _buffer.data() + _readPos, dataSize);
		_readPos = 0;
		_writePos = dataSize;
	}
}