#pragma once
#include <cstdint>
#include <vector>

namespace SE::Net
{
	class RecvBuffer
	{
	public:
		explicit RecvBuffer(int32_t bufferSize = 8192);

	public:
		char* ReadPos();
		char* WritePos();

		int32_t DataSize() const;
		int32_t FreeSize() const;

		bool OnRead(int32_t numOfBytes);
		bool OnWrite(int32_t numOfBytes);

		void Clean();

	private:
		std::vector<char> _buffer;
		int32_t _readPos = 0;
		int32_t _writePos = 0;
	};
}
