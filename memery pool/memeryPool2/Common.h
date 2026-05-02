#pragma once
#include <cstddef>
#include <atomic>
#include <array>

namespace Kama_memoryPool
{
	// 对齐数和大小定义
	constexpr size_t ALIGNMENT = 8;
	constexpr size_t MAX_BYTES = 256 * 1024; // 256KB
	constexpr size_t FREE_LIST_SIZE = MAX_BYTES / ALIGNMENT; // ALIGNMENT等于指针void*的大小

	// 内存块头部信息？？还尚未用到
	struct BlockHeader
	{
		size_t size; // 内存块大小
		bool   inUse; // 使用标志
		BlockHeader* next; // 指向下一个内存块
	};

	class SizeClass {
		static size_t roundUp(size_t bytes) {
			return (bytes + ALIGNMENT - 1) & ~(ALIGNMENT - 1);
		}

		static size_t getIndex(size_t bytes) {
			bytes = std::max(bytes, ALIGNMENT);
			return (bytes + ALIGNMENT - 1) / ALIGNMENT - 1;//主要是为了处理bytes是ALIGNMENT的倍数的情况
		}
	};
}