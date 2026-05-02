#pragma once
#include "Common.h"
#include <mutex>
#include <unordered_map>
#include <array>
#include <atomic>
#include <chrono>

namespace Kama_memoryPool {
	struct SpanTracker {
		std::atomic<void*> spanAddr{ nullptr };//span头地址
		std::atomic<size_t> numPages{ 0 };//页数
		std::atomic<size_t> blockCount{ 0 };//被分成多少个小块内存，用于延迟归还环节
		std::atomic<size_t> freeCount{ 0 };//有多少空闲小块
	};

	class CentralCache {
	public:
		//获取全局单例
		static CentralCache& getInstance() {
			static CentralCache instance;
			return instance;
		}

		void* fetchRange(size_t index);//从中心缓存取内存给线程缓存
		void returnRange(void* start, size_t size, size_t index);//??为什么要同时传size和index，我理解是双向映射，防止错乱

	private:
		CentralCache();
		//从页缓存中获取内存
		void* fetchFromPageCache(size_t size);
		//根据小块内存的地址获取它从属的span
		SpanTracker* getSpanTracker(void* blockAddr);
		//更新span空闲计数，并检查是否可以归还
		void updateSpanFreeCount(SpanTracker* tracker, size_t newFreeBlocks, size_t index);//index是用来在centralList里面用的，如果全部空闲要从表中移除返还给page的

	private:
		// 中心缓存的自由链表，这里是用来存储碎片化的block的
		std::array<std::atomic<void*>, FREE_LIST_SIZE> centralFreeList_;

		// 用于同步的自旋锁
		std::array<std::atomic_flag, FREE_LIST_SIZE> locks_;

		// 使用数组存储span信息，避免map的开销
		std::array<SpanTracker, 1024> spanTrackers_;
		std::atomic<size_t> spanCount_{ 0 };

		// 延迟归还相关的成员变量，包括空间延迟和时间延迟双保险机制
		static const size_t MAX_DELAY_COUNT = 48;  // 最大延迟计数
		std::array<std::atomic<size_t>, FREE_LIST_SIZE> delayCounts_;  // 每个大小类的延迟计数，统计的是申请的次数
		std::array<std::chrono::steady_clock::time_point, FREE_LIST_SIZE> lastReturnTimes_;  // 上次归还时间
		static const std::chrono::milliseconds DELAY_INTERVAL;  // 延迟间隔

		bool shouldPerformDelayedReturn(size_t index, size_t currentCount, std::chrono::steady_clock::time_point currentTime);
		void performDelayedReturn(size_t index);
	};
}
