#include <thread>
#include <vector>
#include <iostream>
#include <chrono>
#include <ranges>
using namespace std::ranges::views;

#include <gtest/gtest.h>

#include <samples/NormalizedLinkedList.hh>
using namespace normalizedlinkedlist;

#include <telamon/WaitFreeSimulator.hh>

namespace normalizedlinkedlist_testsuite {

TEST(NormalizedLinkedList, Insertion) {
	const size_t num_threads = 8;
	const size_t num_operations = 1<<10;

	LinkedList<int16_t> ll;
	std::vector<std::thread> threads{num_threads};
	auto norm_insertion = decltype(ll)::NormalizedInsert{ll};
	auto wf_insertion_sim = tsim::WaitFreeSimulatorHandle<decltype(norm_insertion)>{norm_insertion};

	auto insert = [&] (int id) {
	  if (auto opt = wf_insertion_sim.fork(); opt.has_value()) {
		  auto handle = opt.value();
		  for (int i : iota(num_operations * id) | take(num_operations)) {
			  handle.submit(i);
			  std::this_thread::sleep_for(std::chrono::milliseconds(200));
		  }
		  handle.retire();
	  }
	};

	for (int id = 0; id < num_threads; ++id)
		threads.emplace_back(insert, id);
	for (auto &t: threads) {
		if (t.joinable())
			t.join();
	}

	EXPECT_TRUE(ll.size() > 0);
}

}
