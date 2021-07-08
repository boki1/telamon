#include <utility>
#include <thread>
#include <mutex>
#include <numeric>
#include <algorithm>

#include <gtest/gtest.h>
#include <extern/expected_lite/expected.hpp>

#include <telamon/Versioning.hh>
#include <ranges>

using namespace telamon_simulator::cas_versioning;

namespace versioning_testsuite {

TEST(VersioningTest, CoreFunctionality) {
	auto uniq = std::make_unique<int>(42);
	auto *raw = uniq.get();
	VersionedAtomic<std::unique_ptr<int>> ptr{std::move(uniq)};
	VersionedAtomic<int> ptr2{3};

	EXPECT_TRUE(ptr.load() != nullptr);
	EXPECT_TRUE(ptr2.load() != nullptr);
	EXPECT_EQ(ptr2.load()->value, 3);
	EXPECT_EQ(ptr2.load()->version, 0);
	using version_type = typename Cas<int>::VersionNum;
	auto get_both = [&] (int value, version_type version_num) -> std::pair<int, version_type> {
	  return std::make_pair(value, version_num);
	};
	auto[val, ver] = ptr2.transform(get_both);
	EXPECT_TRUE(val == 3 && ver == 0);
	auto sum_plus_one = [&] (int value, version_type version_num) -> int {
	  return value + version_num + 1;
	};
	auto s = ptr2.transform(sum_plus_one);
	EXPECT_EQ(s, 4);

	int sum = 0;
	VersionedAtomic<int> counter{0};
	telamon_simulator::ContentionFailureCounter failure_counter;
	std::mutex mutex;
	std::vector<std::pair<int, version_type>> cycled{};
	std::array<std::thread, 100> threads;
	auto inc_and_cas = [&] () {
	  int successful_cases = 0;
	  while (true) {
		  auto loaded = counter.load();
		  auto current_count = loaded->value;
		  {
			  std::lock_guard<std::mutex> lock{mutex};
			  cycled.emplace_back(loaded->value, loaded->version);
		  }
		  auto is_successful = counter.compare_exchange_strong(loaded->value, loaded->version, loaded->value + 1, failure_counter);
		  if (is_successful) successful_cases++;
		  if (successful_cases == 10) return;
	  }
	};

	for (auto &t: threads) {
		t = std::thread{inc_and_cas};
	}
	for (auto &t: threads) {
		t.join();
	}

	const auto sz = cycled.size();
	(void) std::unique(cycled.begin(), cycled.end());
	EXPECT_EQ(cycled.size(), sz);
	counter.store(42);
	EXPECT_EQ(counter.load()->value, 42);
	EXPECT_EQ(counter.load()->version, 1001);
#if 0
	std::vector<std::pair<int, version_type>> a{{1, 1}};
	EXPECT_EQ(cycled, a);
	EXPECT_EQ(failure_counter.get(), 0);
#endif
}

}
