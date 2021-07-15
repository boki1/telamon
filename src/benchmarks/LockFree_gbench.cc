#include <thread>
#include <vector>
#include <ranges>
#include <iostream>
using namespace std::ranges::views;

#include <benchmark/benchmark.h>

#include <example_client/list/LockFreeLinkedList.hh>
using namespace harrislinkedlist;

static void BM_Insertion (benchmark::State &state) {
	const size_t num_threads = state.range(0);
	const size_t num_operations = state.range(1);
	LinkedList<int16_t> ll;
	std::vector<std::thread> threads{num_threads};

	auto insert = [&] (int id) {
	  for (int i : iota(num_operations * id) | take(num_operations)) { ll.insert(i); }
	};

	for (auto _ : state) {
		for (int id = 0; id < num_threads; ++id)
			threads.emplace_back(insert, id);
		for (auto &t: threads) if (t.joinable()) t.join();
	}
}

static void BM_Removal (benchmark::State &state) {
	const size_t num_threads = state.range(0);
	const size_t num_operations = state.range(1);
	LinkedList<int16_t> ll;
	std::vector<std::thread> threads{num_threads};
	for (int i : iota(0) | take(state.range(1))) {
		ll.insert(i);
	}

	auto remove = [&] (int id) {
	  for (int i : iota(num_operations * id) | take(num_operations)) { ll.remove(i); }
	};

	for (auto _ : state) {
		for (int id = 0; id < num_threads; ++id)
			threads.emplace_back(remove, id);
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

//BENCHMARK(BM_Removal)
//	->Unit(benchmark::kMillisecond)
//	->Args({2 << 0, 1000})
//	->Args({2 << 1, 1000})
//	->Args({2 << 2, 1000})
//	->Args({2 << 3, 1000})
//	->Args({2 << 4, 1000})
//	->Args({2 << 0, 10000})
//	->Args({2 << 1, 10000})
//	->Args({2 << 2, 10000})
//	->Args({2 << 3, 10000})
//	->Args({2 << 4, 10000})
//	->Args({2 << 5, 10000});

BENCHMARK_MAIN();