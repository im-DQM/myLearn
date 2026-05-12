#pragma once
#include<thread>
#include<mutex>
#include<condition_variable>
#include<queue>
#include<vector>
#include<functional>

class threadPool {
public:
	//构造函数
	threadPool(int threadNum):stop_(false) {
		for (int i = 0; i < threadNum; i++) {
			threads_.emplace_back([this]() {
				while (1) {
					std::unique_lock<std::mutex> lock(mtx_);//设置智能锁,对任务队列进行操作
					condition_.wait(lock, [this]() {
						return !tasks_.empty() || stop_;//stop是用来最后析构的时候唤醒这个条件变量的
						});
					//如果stop则直接退出
					if (stop_)
						return;
				
					std::function<void()> task(std::move(tasks_.front()));//把任务队列列首取出并转换成线程池能统一调用的右值
					tasks_.pop();

					lock.unlock();
					task();//执行任务
				}
				});
		}

	}
	//析构函数
	~threadPool() {
		{
			std::unique_lock<std::mutex> lock(mtx_);
			stop_ = true;
		}
		//唤醒所有阻塞的线程，让他们全部退出程序
		condition_.notify_all();
		for (auto &t : threads_) {
			t.join();//线程结束再走在主程序
		}
	}

	//添加任务函数
	template<typename T,typename... Args>
	void enqueue(T&& t, Args&&... args) {//args再外部调用的时候，需要使用具体的右值
		std::function<void()> task = 
			std::bind(std::forward<T>(t), std::forward<Args>(args)...);//完美转发保证左右值状态不变，防止左右值产生的问题
		{
			std::unique_lock<std::mutex> lock(mtx_);
			tasks_.emplace(std::move(task));
		}
		//唤醒一个线程
		condition_.notify_one();
	}

private:
	std::vector<std::thread> threads_;//线程数组
	std::queue<std::function<void()>> tasks_;//任务队列
	//这里使用了包装器，让函数也能成为统一的对象存储

	std::mutex mtx_;//锁
	std::condition_variable condition_;//条件变量

	bool stop_;//线程池停止标志
};