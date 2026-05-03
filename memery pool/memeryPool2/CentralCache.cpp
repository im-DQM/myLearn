#include "CentralCache.h"
#include "PageCache.h"

namespace MengDa_memoryPool {

	const std::chrono::milliseconds CentralCache::DELAY_INTERVAL{ 1000 };//设置延迟间隔为1000ms
	static const size_t SPAN_PAGES = 8;//span的最小页数

	//构造函数
	CentralCache::CentralCache() {
		for (auto& ptr : centralFreeList_) {
			ptr.store(nullptr, std::memory_order_relaxed);
		}

		for (auto& lock : locks_) {
			lock.clear();
		}

		spanCount_.store(0, std::memory_order_relaxed);//？？在头文件里已经初始化过了，我认为这是多余的

		//初始化延迟数据
		for (auto& count : delayCounts_) {
			count.store(0, std::memory_order_relaxed);
		}

		for (auto& time : lastReturnTimes_) {
			time = std::chrono::steady_clock::now();//记录为当前时间
		}
	}

	//和Page的链接
	void* CentralCache::fetchFromPageCache(size_t size) {
		size_t numPages = (size + PageCache::PAGE_SIZE - 1) / PageCache::PAGE_SIZE;//向上取整，根据size求numPages

		if (numPages < SPAN_PAGES) {//如果小于span的最小页数
			return PageCache::getInstance().allocateSpan(SPAN_PAGES);
		}
		else {
			return PageCache::getInstance().allocateSpan(numPages);
		}
	}

	SpanTracker* CentralCache::getSpanTracker(void* blockAddr) {
		//看这块地址在哪个Span里面
		for (int i = 0; i < spanCount_.load(std::memory_order_relaxed); i++) {
			void* spanAddr = spanTrackers_[i].spanAddr.load(std::memory_order_relaxed);
			size_t numPages = spanTrackers_[i].numPages.load(std::memory_order_relaxed);
			if (blockAddr >= spanAddr && blockAddr < (static_cast<char*>(spanAddr) + numPages * PageCache::PAGE_SIZE)) {
				return& spanTrackers_[i];
			}
		}
		return nullptr;
	}

	//延迟归还
	bool CentralCache::shouldPerformDelayedReturn(size_t index, size_t currentCount, std::chrono::steady_clock::time_point currentTime) {
		//先看申请次数，次数到了了必须还
		if (currentCount >= MAX_DELAY_COUNT) {
			return true;
		}
		//再看时间
		return (currentTime - lastReturnTimes_[index]) >= DELAY_INTERVAL;
	}

	void CentralCache::performDelayedReturn(size_t index) {
		//重置延迟计数
		delayCounts_[index].store(0,std::memory_order_relaxed);
		//重置延迟时间
		lastReturnTimes_[index] = std::chrono::steady_clock::now();

		//统计每个span的空闲块数（同一个span会被切分成几个相同大小的内存块）
		std::unordered_map<SpanTracker*, size_t> spanFreeCounts;
		void* currentBlock = centralFreeList_[index].load(std::memory_order_relaxed);
		//把这个index都遍历一遍
		while (currentBlock) {
			SpanTracker* st = getSpanTracker(currentBlock);
			if (st) {
				spanFreeCounts[st]++;
			}
			currentBlock = *reinterpret_cast<void**>(currentBlock);
		}
		
		//更新每个span的空闲计数并检查是否可以归还
		for (const auto& [tracker, newFreeBlocks] : spanFreeCounts) {
			updateSpanFreeCount(tracker, newFreeBlocks, index);
		}
	}

	void CentralCache::updateSpanFreeCount(SpanTracker* tracker, size_t newFreeBlocks, size_t index)
	{
		size_t oldFreeCount = tracker->freeCount.load(std::memory_order_relaxed);
		size_t newFreeCount = oldFreeCount + newFreeBlocks;
		tracker->freeCount.store(newFreeCount, std::memory_order_release);

		// 如果所有块都空闲，归还span
		if (newFreeCount == tracker->blockCount.load(std::memory_order_relaxed))
		{
			void* spanAddr = tracker->spanAddr.load(std::memory_order_relaxed);
			size_t numPages = tracker->numPages.load(std::memory_order_relaxed);

			// 从自由链表中移除这些块
			void* head = centralFreeList_[index].load(std::memory_order_relaxed);
			void* newHead = nullptr;
			void* prev = nullptr;
			void* current = head;

			while (current)
			{
				void* next = *reinterpret_cast<void**>(current);
				if (current >= spanAddr &&
					current < static_cast<char*>(spanAddr) + numPages * PageCache::PAGE_SIZE)
				{
					if (prev)
					{
						*reinterpret_cast<void**>(prev) = next;
					}
					else
					{
						newHead = next;
					}
				}
				else
				{
					prev = current;
				}
				current = next;
			}

			centralFreeList_[index].store(newHead, std::memory_order_release);
			PageCache::getInstance().deallocateSpan(spanAddr, numPages);
		}
	}

	//和线程缓存的链接
	void* CentralCache::fetchRange(size_t index) {
		if (index > FREE_LIST_SIZE)//如果index超出最大值
			return nullptr;

		while (locks_[index].test_and_set(std::memory_order_acquire)) {//添加自旋锁
			std::this_thread::yield();//防止cpu死循环，等待时让出cpu
		}

		void* result = nullptr;
		try {
			result = centralFreeList_[index].load(std::memory_order_acquire);
			if (!result) {//如果获取失败，那么就要从页缓存中获取大内存块，然后切割成index对应大小的许多小块
				size_t size = (index+1) * ALIGNMENT;
				result = fetchFromPageCache(size);

				if (!result) {//如果依旧失败，那就没办法了
					locks_[index].clear();//释放锁
					return nullptr;
				}
				//如果成功获取大块页内存,要开始切割，制作block链表
				char* start = static_cast<char*>(result);
				//计算实际分配的页数
				size_t numPages = (size <= SPAN_PAGES * PageCache::PAGE_SIZE) ? SPAN_PAGES : (size + PageCache::PAGE_SIZE - 1) / PageCache::PAGE_SIZE;
				//计算切割biolc数量
				size_t blockNum = numPages * PageCache::PAGE_SIZE / size;

				//开始切割链表
				if (blockNum > 1) {//块数大于1才需要做链表？？这里是不是没有处理blockNum=1的情况？
					for (size_t i = 1; i < blockNum; i++) {
						void* cur = start + size * (i - 1);
						void* next = start + size * i;
						*reinterpret_cast<void**>(cur) = next;
					}
					*reinterpret_cast<void**>(start + (blockNum - 1) * size) = nullptr;//设置最后一个块的next
					//接下来把result取出来
					void* next = *reinterpret_cast<void**>(result);
					*reinterpret_cast<void**>(result) = nullptr;
					centralFreeList_[index].store(next, std::memory_order_release);

					size_t trackerIndex = spanCount_++;
					if (trackerIndex < spanTrackers_.size()) {
						spanTrackers_[trackerIndex].spanAddr.store(start, std::memory_order_release);
						spanTrackers_[trackerIndex].numPages.store(numPages, std::memory_order_release);
						spanTrackers_[trackerIndex].blockCount.store(blockNum, std::memory_order_release);
						spanTrackers_[trackerIndex].freeCount.store(blockNum-1, std::memory_order_release);
					}
				}
				
			}
			else {//成功在centralFreeList_里面获取
				void* next = *reinterpret_cast<void**>(result);
				*reinterpret_cast<void**>(result) = nullptr;
				centralFreeList_[index].store(next, std::memory_order_release);

				SpanTracker* tracker = getSpanTracker(result);
				if (tracker) {
					tracker->freeCount.fetch_sub(1, std::memory_order_relaxed);
				}
			}
		}
		catch (...) {
			locks_[index].clear(std::memory_order_release);//解锁
			throw;
		}

		locks_[index].clear(std::memory_order_release);
		return result;
	}

	void CentralCache::returnRange(void* start, size_t size, size_t index) {//就是把这些重新挂链表上，然后看看要不要延迟归还
		if (!start || index > FREE_LIST_SIZE)
			return;

		size_t blockSize = (index + 1) * ALIGNMENT;
		size_t blockNum = size / blockSize;

		while (locks_[index].test_and_set(std::memory_order_acquire)) {
			std::this_thread::yield();
		}

		//将归还的链表连接到中心缓存上
		try {//??这里为很么要用trycatch
			void* end = start;
			size_t count = 1;
			while (*reinterpret_cast<void**>(end) != nullptr && count < blockNum) {
				end = *reinterpret_cast<void**>(end);
				count++;
			}

			void* cur = centralFreeList_[index].load(std::memory_order_acquire);
			*reinterpret_cast<void**>(end) = cur;
			centralFreeList_[index].store(start, std::memory_order_release);
			
			//更新延迟计数
			size_t currentCount = delayCounts_[index].fetch_add(1, std::memory_order_relaxed) + 1;
			auto currentTime = std::chrono::steady_clock::now();

			if (shouldPerformDelayedReturn(index, currentCount, currentTime)) {
				performDelayedReturn(index);
			}
		}
		catch (...) {
			locks_[index].clear(std::memory_order_release);
			throw;
		}

		locks_[index].clear(std::memory_order_release);
	}
}