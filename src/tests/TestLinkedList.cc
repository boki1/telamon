#include <ranges>
#include <array>
#include <thread>
#include <random>
using namespace std::views;

#include <gtest/gtest.h>

#include <example_client/list/LockFreeLinkedList.hh>
#include <example_client/list/NormalizedLinkedList.hh>

namespace harrislinkedlist_testsuite {

TEST(HarissLinkedListTest, CoreFunctionalities) {
	namespace lfll = harrislinkedlist;
	lfll::LinkedList<int16_t> ll;
	auto nod = lfll::LinkedList<int16_t>::Node{3};
	auto[l, r] = ll.search(4);
	for (int i : iota(1) | take(10)) {
		EXPECT_TRUE(ll.insert(i));
	}
	EXPECT_EQ(ll.size(), 10);

	std::array<std::thread, 10> threads;
	for (int id = 0; auto &t: threads) {
		t = std::thread{[&] (int id) {
		  for (int i : iota(10 * id) | take(10)) {
			  ll.insert(i);
		  }
		}, id};
		++id;
	}

	for (auto &t : threads) t.join();
	EXPECT_EQ(ll.size(), threads.size() * 10);
	for (int i : iota(0) | take(30)) {
		EXPECT_TRUE(ll.appears(i));
	}
	EXPECT_FALSE(ll.appears(-42));
	EXPECT_FALSE(ll.insert(2));

	EXPECT_TRUE(ll.remove(2));
	EXPECT_FALSE(ll.appears(2));
	EXPECT_TRUE(ll.appears(3));
	std::tie(std::ignore, std::ignore) = ll.search(3);
	EXPECT_TRUE(ll.insert(2));
	EXPECT_TRUE(ll.appears(2));
}

TEST(HarissLinkedListTest, SimulationIntegrationFastPathOperationsOnly) {
	namespace nll = normalizedlinkedlist;
	auto lf = nll::LinkedList<int>{};
	auto norm_insertion = decltype(lf)::NormalizedInsert{lf};
	auto wf_insertion_sim = tsim::WaitFreeSimulatorHandle<decltype(norm_insertion)>{norm_insertion};

	constexpr int nums = 10;

	for (int i : iota(1) | take(nums)) {
		EXPECT_EQ(lf.size(), i - 1);
		EXPECT_FALSE(lf.appears(i));
		EXPECT_TRUE(wf_insertion_sim.submit(i, decltype(wf_insertion_sim)::Use_fast_path));
		EXPECT_EQ(lf.size(), i);
		EXPECT_TRUE(lf.appears(i));
	}

	auto norm_removal = decltype(lf)::NormalizedRemove{lf};
	auto wf_removal_sim = tsim::WaitFreeSimulatorHandle<decltype(norm_removal)>{norm_removal};
	for (int i : iota(1) | take(nums)) {
		EXPECT_EQ(lf.size(), nums - i + 1);
		EXPECT_TRUE(lf.appears(i));
		EXPECT_TRUE(wf_removal_sim.submit(i, decltype(wf_removal_sim)::Use_fast_path));
		EXPECT_FALSE(lf.appears(i));
		EXPECT_EQ(lf.size(), nums - i);
	}

	EXPECT_EQ(lf.size(), 0);
	EXPECT_EQ(lf.removed_not_deleted(), 10);
}

TEST(HarissLinkedListTest, SimulationIntegrationSlowPathWithSleeps) {
	namespace nll = normalizedlinkedlist;
	for (int j : iota(0) | take(10)) {
		auto lf = nll::LinkedList<int>{};
		auto norm_insertion = decltype(lf)::NormalizedInsert{lf};
		auto wf_insertion_sim = tsim::WaitFreeSimulatorHandle<decltype(norm_insertion)>{norm_insertion};

		std::array<std::thread, 15> threads;
		for (int id = 0; auto &t: threads) {
			t = std::thread{[&] (int id) {
			  if (auto handle_opt = wf_insertion_sim.fork(); handle_opt.has_value()) {
				  auto handle = handle_opt.value();
				  std::mt19937 gen(std::random_device{}());
				  std::uniform_int_distribution<> dis(100, 300);
				  for (int i : iota(10 * id) | take(10)) {
					  EXPECT_FALSE(lf.appears(i));
					  EXPECT_TRUE(handle.submit(i, decltype(handle)::Use_fast_path));
					  EXPECT_TRUE(lf.appears(i));

					  std::this_thread::sleep_for(std::chrono::milliseconds(dis(gen)));
				  }
				  handle.retire();
			  }
			}, id};
			++id;
		}

		for (auto &t : threads) t.join();
		EXPECT_EQ(lf.size(), threads.size() * 10);
		for (int i : iota(0) | take(150)) {
			EXPECT_TRUE(lf.appears(i));
		}
		EXPECT_FALSE(lf.appears(-42));
	}
}

TEST(HarissLinkedListTest, SimulationIntegrationSlowPath) {
	namespace nll = normalizedlinkedlist;
	for (int j : iota(0) | take(10)) {
		constexpr int nums = 100;
		auto lf = nll::LinkedList<int>{};
		auto norm_insertion = decltype(lf)::NormalizedInsert{lf};
		auto wf_insertion_sim = tsim::WaitFreeSimulatorHandle<decltype(norm_insertion), 1>{norm_insertion};
		for (int i : iota(0, nums)) {
			EXPECT_FALSE(lf.appears(i));
			EXPECT_TRUE(wf_insertion_sim.submit(i, decltype(wf_insertion_sim)::Use_slow_path));
			EXPECT_TRUE(lf.appears(i));
		}
		for (int i : iota(0, nums)) {
			EXPECT_TRUE(lf.appears(i));
		}

//		auto norm_removal = decltype(lf)::NormalizedRemove{lf};
//		auto wf_removal_sim = tsim::WaitFreeSimulatorHandle<decltype(norm_removal)>{norm_removal};
//		for (int i : iota(0, nums)) {
//			EXPECT_TRUE(lf.appears(i));
//			EXPECT_TRUE(wf_removal_sim.submit(i, decltype(wf_removal_sim)::Use_slow_path));
//			EXPECT_FALSE(lf.appears(i));
//		}

		EXPECT_FALSE(lf.appears(-42));
	}
}

TEST(HarissLinkedListTest, SimulationIntegrationSlowPathTwoThreads) {
	namespace nll = normalizedlinkedlist;
	for (int j : iota(0) | take(10)) {
		auto lf = nll::LinkedList<int>{};
		auto norm_insertion = decltype(lf)::NormalizedInsert{lf};
		auto wf_insertion_sim = tsim::WaitFreeSimulatorHandle<decltype(norm_insertion), 3>{norm_insertion};

		std::array<std::thread, 2> threads;
		for (int id = 0; auto &t: threads) {
			t = std::thread{[&] (int id) {
			  if (auto handle_opt = wf_insertion_sim.fork(); handle_opt.has_value()) {
				  auto handle = handle_opt.value();
				  for (int i : iota(10 * id) | take(10)) {
					  EXPECT_FALSE(lf.appears(i));
					  EXPECT_TRUE(handle.submit(i, decltype(handle)::Use_slow_path));
					  EXPECT_TRUE(lf.appears(i));
				  }
				  handle.retire();
			  }
			}, id};
			++id;
		}

		for (auto &t : threads) t.join();
		EXPECT_EQ(lf.size(), threads.size() * 10);
		for (int i : iota(0, 20)) {
			EXPECT_TRUE(lf.appears(i));
		}
		EXPECT_FALSE(lf.appears(-42));
	}
}

TEST(HarissLinkedListTest, SimulationIntegrationSlowPathManyThreadsLittleOperations) {
	namespace nll = normalizedlinkedlist;
	for (int j : iota(0) | take(10)) {
		auto lf = nll::LinkedList<int>{};
		auto norm_insertion = decltype(lf)::NormalizedInsert{lf};

		constexpr int num_iters = 10;
		constexpr int num_threads = 128;
		constexpr int nums = num_threads * num_iters;
		auto wf_insertion_sim = tsim::WaitFreeSimulatorHandle<decltype(norm_insertion), num_threads + 1>{norm_insertion};
		std::array<std::thread, num_threads> threads;
		for (int id = 0; auto &t: threads) {
			t = std::thread{[&] (int id) {
			  if (auto handle_opt = wf_insertion_sim.fork(); handle_opt.has_value()) {
				  auto handle = handle_opt.value();
				  for (int i : iota(num_iters * id) | take(num_iters)) {
					  EXPECT_FALSE(lf.appears(i));
					  EXPECT_TRUE(handle.submit(i));
					  EXPECT_TRUE(lf.appears(i));
				  }
				  handle.retire();
			  }
			}, id};
			++id;
		}

		for (auto &t : threads) t.join();
		EXPECT_EQ(lf.size(), nums);
		for (int i : iota(0, nums - 1)) {
			EXPECT_TRUE(lf.appears(i));
		}
		EXPECT_FALSE(lf.appears(-42));
	}
}

TEST(HarissLinkedListTest, SimulationIntegrationSlowPathLittleThreadsManyOperations) {
	namespace nll = normalizedlinkedlist;
	for (int j : iota(0) | take(100)) {
		auto lf = nll::LinkedList<int>{};
		auto norm_insertion = decltype(lf)::NormalizedInsert{lf};
		auto wf_insertion_sim = tsim::WaitFreeSimulatorHandle<decltype(norm_insertion), 5>{norm_insertion};

		constexpr int num_iters = 1000;
		constexpr int num_threads = 4;
		constexpr int nums = num_threads * num_iters;
		std::array<std::thread, num_threads> threads;
		for (int id = 0; auto &t: threads) {
			t = std::thread{[&] (int id) {
			  if (auto handle_opt = wf_insertion_sim.fork(); handle_opt.has_value()) {
				  auto handle = handle_opt.value();
				  for (int i : iota(num_iters * id) | take(num_iters)) {
					  EXPECT_FALSE(lf.appears(i));
					  EXPECT_TRUE(handle.submit(i, decltype(handle)::Use_slow_path));
					  EXPECT_TRUE(lf.appears(i));
				  }
				  handle.retire();
			  }
			}, id};
			++id;
		}

		for (auto &t : threads) t.join();
		EXPECT_EQ(lf.size(), nums);
		for (int i : iota(0, nums)) {
			EXPECT_TRUE(lf.appears(i));
		}
		EXPECT_FALSE(lf.appears(-42));
	}
}

TEST(HarissLinkedListTest, SimulationIntegrationSlowPathManyThreadsManyOperations) {
	namespace nll = normalizedlinkedlist;
	for (int j : iota(0) | take(1)) {
		auto lf = nll::LinkedList<int>{};
		auto norm_insertion = decltype(lf)::NormalizedInsert{lf};
		auto wf_insertion_sim = tsim::WaitFreeSimulatorHandle<decltype(norm_insertion), 65>{norm_insertion};

		constexpr int num_iters = 1e4;
		constexpr int num_threads = 4;
		constexpr int nums = num_threads * num_iters;
		std::array<std::thread, num_threads> threads;
		for (int id = 0; auto &t: threads) {
			t = std::thread{[&] (int id) {
			  if (auto handle_opt = wf_insertion_sim.fork(); handle_opt.has_value()) {
				  auto handle = handle_opt.value();
				  for (int i : iota(num_iters * id) | take(num_iters)) {
					  EXPECT_FALSE(lf.appears(i));
					  EXPECT_TRUE(handle.submit(i, decltype(handle)::Use_fast_path));
					  EXPECT_TRUE(lf.appears(i));
				  }
				  handle.retire();
			  }
			}, id};
			++id;
		}

		for (auto &t : threads) t.join();
		EXPECT_EQ(lf.size(), nums);
		for (int i : iota(0, nums)) {
			EXPECT_TRUE(lf.appears(i));
		}
		EXPECT_FALSE(lf.appears(-42));
	}
}

}
