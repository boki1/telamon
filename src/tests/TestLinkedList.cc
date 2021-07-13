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

	for (int i : iota(1) | take(100)) {
		EXPECT_EQ(lf.size(), i - 1);
		EXPECT_FALSE(lf.appears(i));
		EXPECT_TRUE(wf_insertion_sim.submit(i));
		EXPECT_EQ(lf.size(), i);
		EXPECT_TRUE(lf.appears(i));
	}

//	auto norm_removal = decltype(lf)::NormalizedRemove{lf};
//	auto wf_removal_sim = tsim::WaitFreeSimulatorHandle<decltype(norm_removal)>{norm_removal};
//	EXPECT_TRUE(wf_removal_sim.submit(1));
//	EXPECT_EQ(lf.size(), 0);
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
				  std::uniform_int_distribution<> dis(100, 1300);
				  for (int i : iota(10 * id) | take(10)) {
					  EXPECT_FALSE(lf.appears(i));
					  EXPECT_TRUE(handle.submit(i));
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
		auto lf = nll::LinkedList<int>{};
		auto norm_insertion = decltype(lf)::NormalizedInsert{lf};
		auto wf_insertion_sim = tsim::WaitFreeSimulatorHandle<decltype(norm_insertion), 1>{norm_insertion};
		for (int i : iota(0, 10)) {
			EXPECT_FALSE(lf.appears(i));
			EXPECT_TRUE(wf_insertion_sim.submit(i, true));
			EXPECT_TRUE(lf.appears(i));
		}
		for (int i : iota(0, 10)) {
			EXPECT_TRUE(lf.appears(i));
		}
		EXPECT_FALSE(lf.appears(-42));
	}
}

TEST(HarissLinkedListTest, SimulationIntegrationSlowPathManyThreads) {
//	namespace nll = normalizedlinkedlist;
//	for (int j : iota(0) | take(1)) {
//		auto lf = nll::LinkedList<int>{};
//		auto norm_insertion = decltype(lf)::NormalizedInsert{lf};
//		auto wf_insertion_sim = tsim::WaitFreeSimulatorHandle<decltype(norm_insertion), 65>{norm_insertion};
//
//		std::array<std::thread, 63> threads;
//		for (int id = 0; auto &t: threads) {
//			t = std::thread{[&] (int id) {
//			  if (auto handle_opt = wf_insertion_sim.fork(); handle_opt.has_value()) {
//				  auto handle = handle_opt.value();
//				  if (id % 2 == 0) {
//					  for (int i : iota(10 * id) | take(10)) {
//						  EXPECT_FALSE(lf.appears(i));
//						  EXPECT_TRUE(handle.submit(i));
//						  EXPECT_TRUE(lf.appears(i));
//					  }
//				  } else {
//					  handle.help();
//				  }
//				  handle.retire();
//			  }
//			}, id};
//			++id;
//		}
//
//		for (auto &t : threads) t.join();
//		EXPECT_EQ(lf.size(), threads.size() / 2 * 10);
//		for (int i : iota(0, 640) | filter([](int i) { return (i / 10) % 2 == 0; })) {
//			EXPECT_TRUE(lf.appears(i));
//		}
//		EXPECT_FALSE(lf.appears(-42));
//	}
}

}