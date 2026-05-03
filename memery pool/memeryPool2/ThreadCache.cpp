#include "ThreadCache.h"
#include "CentralCache.h"
#include "common.h"

namespace MengDa_memoryPool {

	void* ThreadCache::allocate(size_t size) {
		if (size > MAX_BYTES) {//大对象直接系统分配
			return malloc(size);
		}

		if (size == 0) {
			size = ALIGNMENT;
		}

		size_t index = SizeClass::getIndex(size);

		freeListSize_[index]--;//后续一定会取一块内存，链表或者中心缓存
		//如果链表里有直接取一块
		if (void* ptr = freeList_[index]) {
			freeList_[index] = *reinterpret_cast<void**>(ptr);
			return ptr;
		}
		//如果本地链表没有，去中心缓存申请
		return fetchFromCentralCache(index);
	}

	void ThreadCache::deallocate(void* ptr, size_t size) {
		if (size > MAX_BYTES) {
			free(ptr);
			return;
		}

		size_t index = SizeClass::getIndex(size);
		//头插法插入自由链表
		*reinterpret_cast<void**>(ptr) = freeList_[index];
		freeList_[index] = ptr;
		//更新计数
		freeListSize_[index]++;

		if (shouldReturnToCentralCache(index)) {
			ReturnToCentralCache(freeList_[index],size);
		}
	}

	void* ThreadCache::fetchFromCentralCache(size_t index) {
		//注意：调用这个函数的时候free链表已经是空的了
		void* start = CentralCache::getInstance().fetchRange(index);
		if (!start) {
			return nullptr;
		}
		//取第一个block
		void* result = start;
		start = *reinterpret_cast<void**>(result);

		//把剩下的放到链表里
		freeList_[index] = start;

		//统计有多少个block
		void* cur = start;
		size_t blockNum = 0;
		while (cur != nullptr) {
			cur = *reinterpret_cast<void**>(cur);
			blockNum++;
		}

		//更新freeSize
		freeListSize_[index] += blockNum;
	
		return result;
	}

	bool ThreadCache::shouldReturnToCentralCache(size_t index) {
		size_t hold = 256;
		return freeListSize_[index] >= hold;
	}

	void ThreadCache::ReturnToCentralCache(void* start, size_t size) {//??要不要直接传index
		size_t index = SizeClass::getIndex(size);

		size_t blockNum = freeListSize_[index];
		//安全保障
		if (blockNum <= 1)
			return;

		//计算要返回多少，保留多少
		size_t keepNum = std::max(blockNum / 4,size_t(1));
		size_t returnNum = blockNum - keepNum;

		void* cur = start;
		//找到要分割的节点
		void* splitNode = cur;
		for (size_t i = 0; i < keepNum - 1; i++) {
			splitNode = *reinterpret_cast<void**>(splitNode);
			if (!splitNode)//如果链表提前终止，前面的计数出错
				break;
		}

		//分割
		if (splitNode) {//如果没提前终止，计数正确
			void* next = *reinterpret_cast<void**>(splitNode);
			*reinterpret_cast<void**>(splitNode) = nullptr;

			//更新数据
			freeList_[index] = start;
			freeListSize_[index] = keepNum;

			//返还给中心缓存
			if (returnNum > 0 && next != nullptr) {
				CentralCache::getInstance().returnRange(next, size, index);
			}
		}
	}
}