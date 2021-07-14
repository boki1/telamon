#include <thread>
#include <array>
#include <experimental/random>

#include <gtest/gtest.h>
#include <extern/loguru/loguru.hpp>

#include <telamon/HelpQueue.hh>

using namespace helpqueue;

namespace helpqueue_testsuite {

class HelpQueueTest : public ::testing::Test {
 protected:
  HelpQueue <int> hq;
  void SetUp () override { }

  void TearDown () override { }

};

TEST_F(HelpQueueTest, PeekNoValue) {
	auto head = hq.peek_front();
	EXPECT_TRUE(!head.has_value());
}

TEST_F(HelpQueueTest, EnqueueSingleThread) {
	hq.push_back(0, 10);
	auto head = hq.peek_front();
	EXPECT_TRUE(head.has_value());
	EXPECT_EQ(head, std::optional <int> {10});
}

TEST_F(HelpQueueTest, MultipleEnqueuesSingleThread) {
	EXPECT_EQ(hq.peek_front(), std::optional <int> {});
	hq.push_back(00, 10);
	for (int i = 1; i <= 10; ++i) {
		EXPECT_EQ(hq.peek_front(), std::optional <int> {10});
		hq.push_back(0, i * 10);
	}
}

TEST_F(HelpQueueTest, DequeueSingleThread) {
	EXPECT_FALSE(hq.try_pop_front(10));
	hq.push_back(0, 10);
	EXPECT_EQ(hq.peek_front(), std::optional <int> {10});
	EXPECT_TRUE(hq.try_pop_front(hq.peek_front().value()));
}

TEST_F(HelpQueueTest, SingleThreadOperations) {
	EXPECT_EQ(hq.peek_front(), std::optional <int> {});
	hq.push_back(0, 10);
	EXPECT_EQ(hq.peek_front(), std::optional <int> {10});
	hq.push_back(0, 20);
	EXPECT_EQ(hq.peek_front(), std::optional <int> {10});
	EXPECT_TRUE(hq.try_pop_front(hq.peek_front().value()));
	EXPECT_EQ(hq.peek_front(), std::optional <int> {20});
	EXPECT_TRUE(hq.try_pop_front(hq.peek_front().value()));
	EXPECT_EQ(hq.peek_front(), std::optional <int> {});
}

TEST_F(HelpQueueTest, MultipleThreadsEnqueue) {
	auto fun = [&] (int id) {
	  for (int i = 0; i < 2; ++i) {
		  sleep(std::experimental::randint(1, 2));
		  hq.push_back(id, std::experimental::randint(1, 100));
	  }
	};

	std::array <std::thread, 3> threads;
	for (int i = 0; i < 3; ++i)
		threads[i] = std::thread {fun, i};
	for (auto &t: threads)
		t.join();

	int size = 0;
	while (true) {
		auto data = hq.peek_front();
		if (!data.has_value()) break;
		++size;
		EXPECT_TRUE(hq.try_pop_front(data.value()));
	}

	EXPECT_EQ(size, 6);
}

TEST_F(HelpQueueTest, MultipleThreadsDequeue) {
	for (int i = 0; i < 1000; ++i) {
		hq.push_back(0, std::experimental::randint(1, 100));
	}

	auto fun = [&] (int id) {
	  while (true) {
		  do {
			  auto data = hq.peek_front();
			  if (!data.has_value())
				  return;
			  if (hq.try_pop_front(data.value()))
				  break;
		  } while (true);
	  }
	};

	std::array <std::thread, 3> threads;
	for (int i = 0; i < 3; ++i)
		threads[i] = std::thread {fun, i};
	for (auto &t: threads)
		t.join();

	EXPECT_FALSE(hq.peek_front().has_value());
}

} // helpqueue_testsuite
