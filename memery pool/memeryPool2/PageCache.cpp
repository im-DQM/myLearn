#include "PageCache.h"
#include<windows.h>

namespace MengDa_memoryPool {
	void PageCache::deallocateSpan(void* ptr, size_t numPages) {//？？我觉得没必要传这个numPages
		//上锁
		std::lock_guard<std::mutex> lock(mutex_);

		//看看是不是本内存池分配的内存，如果不是直接返回
		auto it = spanMap_.find(ptr);
		if (it == spanMap_.end()) return;
		Span* span = it->second;//这种用迭代器一以贯之的写法只需要查一次哈希表，效率高

		//接下来判断你能不能合并
		void* nextAddr = static_cast<char*>(ptr) + numPages * PAGE_SIZE;
		//先判断相邻的span在不在spanMap里
		auto nextIt = spanMap_.find(nextAddr);
		if (nextIt != spanMap_.end()) {//如果在
			Span* nextSpan = nextIt->second;
			//再判断它在不在freeSpans里
			bool found = false;
			auto& nextList = freeSpans_[nextSpan->numPages];//因为涉及到对链表本身的操作，所以要用引用
			//先判断是不是在头部
			if (nextSpan == nextList) {
				found = true;
				nextList = nextSpan->next;
			}
			else if (nextList) {
				Span* cur = nextList;
				while (cur->next) {
					if (cur->next == nextSpan) {
						found = true;
						cur->next = nextSpan->next;
						break;
					}
					cur = cur->next;
				}
			}

			if (found) {//如果在空闲链表里找到了就合并
				span->numPages = numPages + nextSpan->numPages;
				spanMap_.erase(nextAddr);
				delete nextSpan;
			}
			
			//将合并后的span放到空闲列表中
			auto& list = freeSpans_[span->numPages];
			span->next = list;
			list = span;
		}
	}

	void* PageCache::allocateSpan(size_t numPages) {
		//上锁
		std::lock_guard<std::mutex> lock(mutex_);

		auto it = freeSpans_.lower_bound(numPages);//找到第一个大于等于num的
		if (it != freeSpans_.end()) {//如果找到了
			Span* span = it->second;

			//把span从free数组里移除
			if (span->next) //如果span后面还有
				freeSpans_[it->first] = span->next;
			else//如果span在链表尾部
				freeSpans_.erase(it);

			if (span->numPages > numPages) {//如果取出的span比要求的大，那么就要切割
				//定义切割下来的不用的span
				Span* newSpan = new Span;
				newSpan->pageAddr = static_cast<char*>(span->pageAddr) + numPages * PAGE_SIZE;
				newSpan->numPages = span->numPages - numPages;
				newSpan->next = nullptr;

				//把newSpan放到map里
				spanMap_[newSpan->pageAddr] = newSpan;
				//把newSpan放到free链表里
				auto& list = freeSpans_[newSpan->numPages];//头指针
				newSpan->next = list;
				list = newSpan;

				span->numPages = numPages;//更新span切割之后的页数
			}
			
			spanMap_[span->pageAddr] = span;//把span放到链表中
			return span->pageAddr;//返回span的头地址
		}

		//如果没找到，要向系统申请
		void* memory = systemAllocate(numPages);
		if (!memory)
			return nullptr;

		Span* span = new Span;
		span->pageAddr = memory;
		span->numPages = numPages;
		span->next = nullptr;

		spanMap_[span->pageAddr] = span;//把span放到忙碌链表中
		return span->pageAddr;//返回span的头地址

	}

	void* PageCache::systemAllocate(size_t numPages) {
		//计算要分配的内存大小
		size_t size = numPages * PAGE_SIZE;

		// Windows 分配可读写内存（和 mmap 功能完全一样）
		void* ptr = VirtualAlloc(nullptr,size,MEM_COMMIT | MEM_RESERVE,PAGE_READWRITE);
		if (ptr == nullptr)
			return nullptr;

		//清空内存（初始化）
		memset(ptr, 0, size);
		return ptr;
}


}
