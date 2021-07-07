#include <optional>
#include <variant>
#include <thread>
#include <array>

#include <extern/expected_lite/expected.hpp>
#include <gtest/gtest.h>

#include <telamon/WaitFreeSimulator.hh>

using namespace telamon_simulator;

namespace telamon_simulator_testsuite {

struct LF {
  struct LFInput {};
  struct LFOutput {};
  struct LFCommitDescriptor {};

  using Input = LFInput;
  using Output = LFOutput;
  using CommitDescriptor = LFCommitDescriptor;

  auto wrap_up (nonstd::expected<std::monostate, std::optional<int>> executed,
                const LF::CommitDescriptor &desc,
                ContentionFailureCounter &contention) -> nonstd::expected<std::optional<LF::Output>, std::monostate> {
	  (void) desc;
	  (void) executed;
	  (void) contention;
	  return {};
  }

  auto generator (const LF::Input &input, ContentionFailureCounter &contention) -> std::optional<LF::CommitDescriptor> {
	  (void) input;
	  (void) contention;
	  return LFCommitDescriptor{};
  }

  auto fast_path (const LF::Input &inp, ContentionFailureCounter &contention) -> std::optional<LF::Output> {
	  (void) inp;
	  (void) contention;
	  return LFOutput{};
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

	auto second_handle = origin_handle.Fork().value();
	EXPECT_TRUE(!origin_handle.Fork().has_value());
	second_handle.Retire();
	auto forth_handle = origin_handle.Fork().value();
}

TEST_F(TelamonSimulatorTest, Helping) {
	WaitFreeSimulatorHandle<LF, ConcurrentTasks + 1> origin_handle{algorithm};
	std::array<std::thread, ConcurrentTasks> tasks;
	for (auto &t: tasks) {
		t = std::thread{[&] {
		  auto handle_opt = origin_handle.Fork();
		  if (!handle_opt.has_value()) return;
		  auto handle = handle_opt.value();
		  handle.try_help_others();
		}};
	}

	for (auto &t: tasks) {
		t.join();
	}
}

}  // namespace telamon_simulator_testsuite
