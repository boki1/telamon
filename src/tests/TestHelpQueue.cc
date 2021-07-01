#include <gtest/gtest.h>
#include <telamon/HelpQueue.hh>

using namespace helpqueue;

namespace helpqueue_testsuite {

class HelpQueueTest : public ::testing::Test {
 protected:
  HelpQueue <int> hq{};

  void SetUp () override {}

  void TearDown () override {}
};

TEST_F(HelpQueueTest, PeekNoValue) {
  auto head = hq.peek ();
  EXPECT_TRUE(!head.has_value ());
}

TEST_F(HelpQueueTest, EnqueueSingleThread) {
  hq.enqueue (10, 0);
  auto head = hq.peek ();
  EXPECT_TRUE(head.has_value ());
  EXPECT_EQ(head, std::optional <int>{10});
}

} // helpqueue_testsuite
