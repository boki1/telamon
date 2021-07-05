#include <optional>

// #include <folly/Expected.h>
#include <extern/expected_lite/expected.hpp>
#include <gtest/gtest.h>

#include <telamon/NormalizedRepresentation.hh>

using namespace telamon_simulator;

namespace telamon_simulator_testsuite {

struct NormalizedLockFreeTest {};

struct LF {
  struct LFInput {};
  struct LFOutput {};
  struct LFCommitDescriptor {};

  using Input = LFInput;
  using Output = LFOutput;
  using CommitDescriptor = LFCommitDescriptor;

  auto wrap_up(const LF::Input &inp, ContentionMeasure &contention) -> expected<std::optional<LF::Output>, ContentionMeasure> {
	(void) inp;
	(void) contention;
	return {};
  }

  auto generator(const CommitDescriptor &desc, std::optional<int> executed, ContentionMeasure &contention) -> expected<LF::Input, LF::Output> {
	(void) desc;
	(void) executed;
	(void) contention;
	return LFInput{};
  }

  auto fast_path(const LF::Input &inp, ContentionMeasure &contention) -> expected<LF::Output, ContentionMeasure> {
	(void) inp;
	(void) contention;
	return LFOutput{};
  }
};

class TelamonSimulatorTest : public ::testing::Test {
 protected:
  void SetUp() override {}

  void TearDown() override {}
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
