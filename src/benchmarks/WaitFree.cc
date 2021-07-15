#include <thread>
#include <vector>
#include <iostream>
#include <chrono>
#include <ranges>
using namespace std::ranges::views;

#include <example_client/list/NormalizedLinkedList.hh>
using namespace normalizedlinkedlist;
#include <telamon/WaitFreeSimulator.hh>

int main (int argc, char *argv[]) {
	if (argc < 3) {
		std::cerr << "3 args required: ./wf num_threads num_operations\n";
		return 1;
	}
	const size_t num_threads = std::atoi(argv[1]);
	const size_t num_operations = std::atoi(argv[2]);

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
		if (t.joinable()) t.join();
	}
}
