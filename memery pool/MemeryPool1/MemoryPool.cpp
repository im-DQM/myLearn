#include "MemoryPool.h"
#include <cassert>
namespace MemoryPool {
	MemoryPool::MemoryPool(size_t BlockSize)//构造函数初始化列表，在构造函数的冒号后面逐个初始化成员变量；左侧是构造函数基本格式
		:BlockSize_(BlockSize)//构造函数除了在头文件声明中，是不需要加分号的
	{}

	MemoryPool::~MemoryPool() {
		//删除连续的Slot
		Slot* cur = firstBlock_;
		while (cur) {
			Slot* next = cur->next;
			//等同于free，转化为void指针，因为void类型不需要析构函数，只释放空间
			operator delete(reinterpret_cast<void*>(cur));
			cur = next;
		}
	}

	void MemoryPool::init(size_t size) {//初始化
		assert(size > 0);//断言宏，如果为false则输出错误信息，调用abort终止程序
		SlotSize_ = size;
		firstBlock_ = nullptr;
		curSlot_ = nullptr;
		freeList_ = nullptr;
		lastSlot_ = nullptr;
	}

	void* MemoryPool::allocate() {
		//优先使用空闲链表里的内存槽
		if (freeList_ != nullptr) {//减少锁竞争，锁是很昂贵的操作
			{//额外作用域用来精准控制锁的范围，锁在作用域结束时自动解除
				std::lock_guard<std::mutex> lock(mutexForBlock_);
				if (freeList_ != nullptr) {//保证线程安全
					Slot* temp = freeList_;
					freeList_ = freeList_->next;
					return temp;
				}
			}
		}

		Slot* temp;
		{
			std::lock_guard<std::mutex> lock(mutexForBlock_);
			if (curSlot_ >= lastSlot_) {
				//当前内存块无内存槽可用，要开辟一块新内存
				allocateNewBlock();
			}

			temp = curSlot_;
			//不能直接加SlotSzie因为curSlot是Slot*类型
			curSlot_ += SlotSize_ / sizeof(Slot);
		}
		return temp;
	}

	void MemoryPool::deallocate(void* ptr) {
		if (ptr) {
			//回收内存，将内存通过头插法插入空闲链表中
			std::lock_guard<std::mutex> lock(mutexForBlock_);
			reinterpret_cast<Slot*>(ptr)->next = freeList_;
			freeList_ = reinterpret_cast<Slot*>(ptr);
		}
	}

	void MemoryPool::allocateNewBlock() {
		void* newBlock = operator new(BlockSize_);
		reinterpret_cast<Slot*>(newBlock)->next = firstBlock_;
		firstBlock_ = reinterpret_cast<Slot*>(newBlock);

		char* body = reinterpret_cast<char*>(newBlock) + sizeof(Slot*);
		size_t paddingSize = padPointer(body, SlotSize_);//计算对其需要
		curSlot_ = reinterpret_cast<Slot*>(body + paddingSize);
		
		//超过该标记位置，则说明无内存条可用，需要申请新的内存块
		lastSlot_ = reinterpret_cast<Slot*>(reinterpret_cast<size_t>(newBlock) + BlockSize_ - SlotSize_ + 1);
		freeList_ = nullptr;
	}

	//让指针对齐到槽大小的倍数位置
	size_t MemoryPool::padPointer(char* p, size_t align) {
		//align是槽大小
		return (align - reinterpret_cast<size_t>(p)) % align;
	}

	void HashBucket::initMemoryPool() {
		for (int i = 0; i < MEMORY_POOL_NUM; i++) {
			getMemoryPool(i).init((i + 1) * SLOT_BASE_SIZE);
		}
	}

	//单例模式，确保一个类只存在一个对象
	MemoryPool& HashBucket::getMemoryPool(int index) {
		static MemoryPool memoryPool[MEMORY_POOL_NUM];
		return memoryPool[index];
	}

}//namespace memoryPool