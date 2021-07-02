#include <gtest/gtest.h>
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
	hq.push_back(10, 0);
	auto head = hq.peek_front();
	EXPECT_TRUE(head.has_value());
	EXPECT_EQ(head, std::optional <int> {10});
}

TEST_F(HelpQueueTest, MultipleEnqueuesSingleThread) {
	EXPECT_EQ(hq.peek_front(), std::optional <int> {});
	hq.push_back(10, 0);
	for (int i = 1; i <= 10; ++i) {
		EXPECT_EQ(hq.peek_front(), std::optional <int> {10});
		hq.push_back(i * 10, 0);
	}
}

TEST_F(HelpQueueTest, DequeueSingleThread) {
	EXPECT_FALSE(hq.try_pop_front(10));
	hq.push_back(10, 0);
	EXPECT_EQ(hq.peek_front(), std::optional <int> {10});
	EXPECT_TRUE(hq.try_pop_front(hq.peek_front().value()));
}

TEST_F(HelpQueueTest, SingleThreadOperations) {
	EXPECT_EQ(hq.peek_front(), std::optional <int> {});
	hq.push_back(10, 0);
	EXPECT_EQ(hq.peek_front(), std::optional <int> {10});
	hq.push_back(20, 0);
	EXPECT_EQ(hq.peek_front(), std::optional <int> {10});
	EXPECT_TRUE(hq.try_pop_front(hq.peek_front().value()));
	EXPECT_EQ(hq.peek_front(), std::optional <int> {20});
	EXPECT_TRUE(hq.try_pop_front(hq.peek_front().value()));
	EXPECT_EQ(hq.peek_front(), std::optional <int> {});
}

} // helpqueue_testsuite
