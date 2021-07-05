/**
 * \file NormalizedRepresentation.hh
 * \brief Provides the foundational structure of a normalized structure which the simulated algorithm is required to adhere to
 */
#ifndef TELAMON_NORMALIZED_REPRESENTATION_HH
#define TELAMON_NORMALIZED_REPRESENTATION_HH

#include <concepts>
#include <variant>
#include <vector>
#include <thread>
#include <algorithm>
#include <mutex>
#include <numeric>
#include <utility>
#include <optional>

#include <extern/expected_lite/expected.hpp>

namespace telamon_simulator {

/// \brief Measures the contention which was encountered during simulation
/// \details Keeps an internal counter of the detected contention and responds according to it.
class ContentionMeasure {
 public:
  constexpr static inline int THRESHOLD = 2;

 public:
  auto detect() -> bool {
	return (++counter > ContentionMeasure::THRESHOLD);
  }

 private:
  int counter = 0;
};

/// \brief   Here are the operations which are required to be described in the lock-free algorithm in order to use the
/// 	     simulation. There are 3 types which the lock-free has to define according to its specifics as well as 3 functions.
/// \tparam  LockFree The lock-free algorithm which is being simulated
/// \details The`generator` and `wrap_up` functions correspond to the first and third stage of the algorithm operation.
/// 		 The fast path represents the steps which are used when the operation in executed as lock-free.
template<typename LockFree>
concept NormalizedRepresentation = requires(LockFree lf, ContentionMeasure &contention, const typename LockFree::Input &inp,
									  const typename LockFree::CommitDescriptor &desc, std::optional<int> executed) {
  typename LockFree::Input;
  typename LockFree::Output;
  typename LockFree::CommitDescriptor;

  { lf.wrap_up(inp, contention) } -> std::same_as<nonstd::expected<std::optional<typename LockFree::Output>, ContentionMeasure>>;

  { lf.generator(desc, executed, contention) } -> std::same_as<nonstd::expected<typename LockFree::Input, typename LockFree::Output>>;

  { lf.fast_path(inp, contention) } -> std::same_as<nonstd::expected<typename LockFree::Output, ContentionMeasure>>;
};

//!
/// Example usage:
/// struct LF {
/// 		using Input = int;
/// 		using Output = int;
/// 		using CommitDescriptor = int;
///
/// 		auto wrap_up(const Input &inp, ContentionMeasure &contention) -> Expected<optional<Output>, ContentionMeasure> { ... }
///
/// 		auto generator(const CommitDescriptor &desc, std::optional<int> executed, ContentionMeasure &contention) -> Expected<Input, Output> { ... } /
///
/// 		auto fast_path(const Input &inp, ContentionMeasure &contention) -> Expected<Output, ContentionMeasure> { ... }
/// };

}

#endif // TELAMON_NORMALIZED_REPRESENTATION_HH
