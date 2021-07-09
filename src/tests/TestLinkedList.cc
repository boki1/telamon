#include <ranges>
#include <array>
#include <thread>
using namespace std::views;

#include <gtest/gtest.h>

#include <list/LinkedList.hh>
using namespace harrislinkedlist;

namespace harrislinkedlist_testsuite {

TEST(HarissLinkedList, CoreFunctionalities) {
	LinkedList<int16_t> ll;
	auto nod = LinkedList<int16_t>::Node{3};
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

}