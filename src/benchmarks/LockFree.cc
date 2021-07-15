#include <thread>
#include <vector>
#include <iostream>
#include <chrono>
#include <ranges>
using namespace std::ranges::views;

#include <example_client/list/LockFreeLinkedList.hh>
using namespace harrislinkedlist;

int main (int argc, char *argv[]) {
	if (argc < 3) {
		std::cerr << "3 args required: ./lf num_threads num_operations\n";
		return 1;
	}
	const size_t num_threads = std::atoi(argv[1]);
	const size_t num_operations = std::atoi(argv[2]);
	std::vector<std::thread> threads;
	threads.reserve(num_threads);
	auto ll = LinkedList<int>{};

	auto insert = [&] (int id) {
	  for (int i : iota(num_operations * id) | take(num_operations)) {
		  ll.insert(i);
		  std::this_thread::sleep_for(std::chrono::milliseconds(200));
	  }
	};

	for (int id = 0; id < num_threads; ++id)
		threads.emplace_back(insert, id);

	for (auto &t: threads) {
		if (t.joinable()) {
			t.join();
		}
	}
}
