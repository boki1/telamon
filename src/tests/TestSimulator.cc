#include <optional>
#include <ranges>
#include <variant>
#include <thread>
#include <array>

#include <nonstd/expected.hpp>
#include <gtest/gtest.h>

#include <telamon/WaitFreeSimulator.hh>

using namespace telamon_simulator;

namespace telamon_simulator_testsuite {

struct LF {
  struct VersionedCas {
	auto has_modified_bit () const noexcept -> bool { return false; }
	auto clear_bit () const noexcept {}
	auto state () const noexcept -> CasStatus { return CasStatus::Success; }
	auto set_state (CasStatus new_status) noexcept { (void) new_status; }
	auto swap_state (CasStatus expected, CasStatus desired) noexcept -> bool {
		(void) expected;
		(void) desired;
		return true;
	}
	auto execute (ContentionFailureCounter &failures) noexcept -> nonstd::expected<bool, std::monostate> {
		(void) failures;
		return nonstd::make_unexpected(std::monostate{});
	}
  };

  using LFCommit = std::ranges::single_view<VersionedCas>;

  using Input = int;
  using Output = int;
  using Commit = LFCommit;

  auto wrap_up (const nonstd::expected<std::monostate, std::optional<int>> &executed,
                const LF::Commit &desc,
                ContentionFailureCounter &contention) -> nonstd::expected<std::optional<LF::Output>, std::monostate> {
	  (void) desc;
	  (void) executed;
	  (void) contention;
	  return {};
  }

  auto generator (const LF::Input &input, ContentionFailureCounter &contention) -> std::optional<LF::Commit> {
	  (void) input;
	  (void) contention;
	  return std::optional<LF::Commit>{};
  }

  auto fast_path (const LF::Input &inp, ContentionFailureCounter &contention) -> std::optional<LF::Output> {
	  (void) inp;
	  (void) contention;
	  return LF::Output{};
  }
};

class TelamonSimulatorTest : public ::testing::Test {
 protected:
  auto constexpr inline static ConcurrentTasks = int{5};
  LF algorithm{};

  void SetUp () override {}
  void TearDown () override {}
};

TEST_F(TelamonSimulatorTest, NormalizedLockFreeConcept) {
	EXPECT_TRUE(true);
}

TEST_F(TelamonSimulatorTest, HandleSimulatorConstruction) {
	WaitFreeSimulatorHandle<LF, 2> origin_handle{algorithm};

	auto second_handle = origin_handle.fork().value();
	EXPECT_TRUE(!origin_handle.fork().has_value());
	second_handle.retire();
	auto forth_handle = origin_handle.fork().value();
}

TEST_F(TelamonSimulatorTest, Helping) {
	WaitFreeSimulatorHandle<LF, ConcurrentTasks> origin_handle{algorithm};
	std::array<std::thread, ConcurrentTasks - 1> tasks;
	for (auto &t: tasks) {
		t = std::thread{[&] {
		  auto handle_opt = origin_handle.fork();
		  if (!handle_opt.has_value()) return;
		  auto handle = handle_opt.value();
		  handle.help();
		}};
	}

	for (auto &t: tasks) {
		t.join();
	}
}

TEST_F(TelamonSimulatorTest, SubmittingOperations) {
	WaitFreeSimulatorHandle<LF, ConcurrentTasks> origin_handle{algorithm};
	std::array<std::thread, ConcurrentTasks - 1> tasks;
	for (auto &t: tasks) {
		t = std::thread{[&] () -> std::optional<LF::Output> {
		  auto handle_opt = origin_handle.fork();
		  if (!handle_opt.has_value()) return std::nullopt;
		  auto handle = handle_opt.value();
		  auto output = handle.submit(LF::Input{});
		  return std::make_optional(output);
		}};
	}

	for (auto &t: tasks) {
		t.join();
	}
}

}  // namespace telamon_simulator_testsuite
