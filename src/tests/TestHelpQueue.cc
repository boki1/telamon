#include <gtest/gtest.h>
#include <telamon/HelpQueue.hh>

using namespace helpqueue;

TEST(HelpQ, Construction) {
  auto hq = HelpQueue<int>{};
  auto head = hq.peek();
  EXPECT_TRUE(!head.has_value());
}
