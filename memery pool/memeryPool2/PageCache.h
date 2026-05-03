#pragma once
#include<map>
#include<mutex>

namespace MengDa_memoryPool {
	class PageCache {
	public:
		static const size_t PAGE_SIZE = 4096;//操作系统默认一页是4kb（4096字节）

		//获取单例，全局唯一，不用返回指针了，直接返回对象
		static PageCache& getInstance() {
			static PageCache instance;
			return instance;
		}

		void* allocateSpan(size_t numPages);
		void deallocateSpan(void* ptr, size_t numPages);

	private:
		PageCache() = default;
		//向系统申请内存
		void* systemAllocate(size_t numPages);

	private:
		struct Span {
			void* pageAddr;//页的起始地址
			size_t numPages;//页数
			Span* next;//链表指针
		};

		std::map<size_t, Span*> freeSpans_;//按页数管理空闲span，不同页数对应不同span链表
		std::map<void*, Span*> spanMap_;//方便通过地址查找Span：通过一小块内存的首地址找出它队医你个的是哪个Span，所有span都要存进去
		std::mutex mutex_;
	};
}
