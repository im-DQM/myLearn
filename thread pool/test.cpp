#include"threadPool.h"
#include<string>
#include<iostream>
int main() {
	threadPool pool(4);
	for (int i = 0; i < 10; i++) {
		pool.enqueue([i]() {
			std::cout << "task:" << i << "is running" << std::endl;
			std::this_thread::sleep_for(std::chrono::milliseconds(100));
			std::cout << "task:" << i << "end" << std::endl;
			});
	}
	
	std::this_thread::sleep_for(std::chrono::seconds(2));
	return 0;
}