#pragma once
#include<array>
#include"Common.h"

namespace Kama_memoryPool {
	class ThreadCache {//线程单例
	public:
		//提供对外获取线程对象接口
		static ThreadCache* getInstanc() {
			static thread_local ThreadCache instance;
			return &instance;
		}
		//提供分配内存接口,线程缓存内部处理
		void* allocate(size_t size);
		void deallocate(void* ptr, size_t size);

	private:
		//单例模式构造函数私有
		ThreadCache() {
			freeList_.fill(nullptr);
			freeListSize_.fill(0);
		};
		//从中心缓存获取内存
		void* fetchFromCentralCache(size_t index);
		//判断是否需要吧内存还给中心缓存
		bool shouldReturnToCentralCache(size_t index);
		//把内存还给中心缓存
		void ReturnToCentralCache(void* start, size_t index);

	private://写两个private把私有数据和私有方法分开
		std::array<void*, FREE_LIST_SIZE> freeList_;//储存自由链表的数组
		std::array<size_t, FREE_LIST_SIZE> freeListSize_;//自由链表大小统计
	};
}
