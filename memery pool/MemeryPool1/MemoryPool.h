#pragma once
#include <mutex>
namespace MemoryPool {
#define MEMORY_POOL_NUM 64	
#define SLOT_BASE_SIZE 8
#define MAX_SLOT_SIZE 512

	struct Slot {
		Slot* next;
};

	class MemoryPool {
	public:
		MemoryPool(size_t BlockSize = 4096);//构造函数
		~MemoryPool();//折构函数，用于清理内存池防止泄露

		void init(size_t);

		void* allocate();
		void deallocate(void*);//void*指针不能直接解引用，而是要转换成具体的类型之后使用
	private:
		void allocateNewBlock();
		size_t padPointer(char* p, size_t align);//计算指针对其所需填充的字节数

	private:
		int BlockSize_;//内存块大小，其中的下划线是c++命名约定，用来代表成员变量
		int SlotSize_;//内存槽大小
		Slot* firstBlock_;//指向内存池管理的首个实际内存块
		Slot* curSlot_;//指向当前未被使用的槽
		Slot* freeList_;//指向空闲的槽（被使用后又被释放的）
		Slot* lastSlot_;//作为当前内存块中最后能存放元素的位置并表示（超过该位置需要申请新的内存块）
		std::mutex mutexForFreeList_;//保证freeList在多线程操作中的原子性
		std::mutex mutexForBlock_;//保证多线程情况下避免不必要的重复开辟内存导致浪费
	};

	class HashBucket {
	public:
		static void initMemoryPool();
		static MemoryPool& getMemoryPool(int index);

		static void* useMemory(size_t size) {
			if (size <= 0)
				return nullptr;
			if (size > MAX_SLOT_SIZE)//大于512字节的内存直接用new
				return operator new(size);

			//相当于size/8向上取整
			return getMemoryPool(((size + 7 )/ SLOT_BASE_SIZE) - 1).allocate();
		}

		static void freeMemory(void* ptr, size_t size) {
			if (!ptr)
				return;
			if (size > MAX_SLOT_SIZE) {
				operator delete(ptr);
				return;
			}

			getMemoryPool(((size + 7) / SLOT_BASE_SIZE) - 1).deallocate(ptr);
		}

		template<typename T,typename...Args>//这四行代码没懂是干什么的，待会仔细学习
		friend void deleteElement(Args&&...args);

		template<typename T>//泛型编程，这里的T代表随意的变量类型
		friend void deleteElement(T* p);
	};

	template<typename T, typename...Args>//后面的。。。args是可变参数模板，表示函数可以接受任意类型任意数量的参数
	T* newElement(Args&&...args) {//&&和args配合形成万能引用
		T* p = nullptr;
		//根据元素大小选取合适的内存池分配
		if ((p = reinterpret_cast<T*>(HashBucket::useMemory(sizeof(T)))) != nullptr)//先把p指向use函数返回的内存池对象，再用它和空指针比较，这个时候p已经指向相应大小的内存池对象了
			//在分配的内存上构造对象并返回这个指针，这样得到的指针就已经指向了T对象
			new(p) T(std::forward<Args>(args)...);

		return p;
	}

	template<typename T>
	void deleteElement(T* p) {
		//对象析构，结束一个对象的生命周期执行的清理操作
		if (p) {
			p->~T();
			//内存回收
			HashBucket::freeMemory(reinterpret_cast<void*>(p), sizeof(T));
		}
	}
}//namespace memoryPool