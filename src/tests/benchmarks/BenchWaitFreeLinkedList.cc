#include <thread>
#include <vector>
#include <ranges>
#include <iostream>
using namespace std::ranges::views;

#include <benchmark/benchmark.h>

#include <samples/NormalizedLinkedList.hh>
#include <telamon/WaitFreeSimulator.hh>

using namespace normalizedlinkedlist;

static void BM_Insertion (benchmark::State &state) {
	const size_t num_threads = state.range(0);
	const size_t num_operations = state.range(1);
	LinkedList<int16_t> ll;
	std::vector<std::thread> threads{num_threads};
	auto norm_insertion = decltype(ll)::NormalizedInsert{ll};
	auto wf_insertion_sim = tsim::WaitFreeSimulatorHandle<decltype(norm_insertion)>{norm_insertion};

	auto insert = [&] (int id) {
	  if (auto opt = wf_insertion_sim.fork(); opt.has_value()) {
		  auto handle = opt.value();
		  for (int i : iota(num_operations * id) | take(num_operations)) {
			  handle.submit(i);
		  }
		  handle.retire();
	  }
	};

	for (auto _ : state) {
		for (int id = 0; id < num_threads; ++id)
			threads.emplace_back(insert, id);
		for (auto &t: threads) if (t.joinable()) t.join();
	}
}

BENCHMARK(BM_Insertion)
	->Unit(benchmark::kMillisecond)
	->Args({2 << 0, 500})
	->Args({2 << 1, 500})
	->Args({2 << 2, 500})
	->Args({2 << 3, 500})
	->Args({2 << 4, 500})
	->Args({2 << 0, 1000})
	->Args({2 << 1, 1000})
	->Args({2 << 2, 1000})
	->Args({2 << 3, 1000})
	->Args({2 << 4, 1000})
	->Args({2 << 5, 1000});

BENCHMARK_MAIN();
