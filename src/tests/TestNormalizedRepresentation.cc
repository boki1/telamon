#include <optional>
#include <variant>

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

  auto wrap_up (std::optional<int> executed,
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
  void SetUp () override {}

  void TearDown () override {}
};

TEST_F(TelamonSimulatorTest, NormalizedLockFreeConcept) {
	EXPECT_TRUE(true);
}

TEST_F(TelamonSimulatorTest, HandleSimulatorConstruction) {
	auto algorithm = LF{};
	WaitFreeSimulatorHandle<LF, 2> origin_handle{algorithm};

	auto second_handle = origin_handle.Fork().value();
	EXPECT_TRUE(!origin_handle.Fork().has_value());
	second_handle.Retire();
	auto forth_handle = origin_handle.Fork().value();
}

}  // namespace telamon_simulator_testsuite
