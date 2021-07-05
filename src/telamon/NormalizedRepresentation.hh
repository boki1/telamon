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

//#include <folly/Expected.h>
//template<typename Value, typename Error>
//using expected = folly::Expected<Value, Error>;

#include <extern/expected_lite/expected.hpp>
using namespace nonstd;

#include "HelpQueue.hh"
#include "OperationHelping.hh"

namespace telamon_simulator {

class NormalizedRepresentation {};

///
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
concept NormalizedLockFree = requires(LockFree lf, ContentionMeasure &contention, const typename LockFree::Input &inp,
									  const typename LockFree::CommitDescriptor &desc, std::optional<int> executed) {
  typename LockFree::Input;
  typename LockFree::Output;
  typename LockFree::CommitDescriptor;

  { lf.wrap_up(inp, contention) } -> std::same_as<expected<std::optional<typename LockFree::Output>, ContentionMeasure>>;

  { lf.generator(desc, executed, contention) } -> std::same_as<expected<typename LockFree::Input, typename LockFree::Output>>;

  { lf.fast_path(inp, contention) } -> std::same_as<expected<typename LockFree::Output, ContentionMeasure>>;
};

template<typename LockFree, const int N = 16, typename = std::enable_if<(N > 0)>> requires NormalizedLockFree<LockFree>
class WaitFreeSimulatorHandle {
 public:
  using Id_type = int;
 private:
  struct Common {
	LockFree m_algorithm;
	helpqueue::HelpQueue<LockFree, N> m_helpqueue;
	std::vector<Id_type> m_free;
	std::mutex m_free_lock;

	explicit Common(const LockFree &lf) : m_algorithm{lf}, m_helpqueue{} {
	  m_free.resize(N - 1);
	  std::iota(m_free.begin(), m_free.end(), 1);
	}

	explicit Common(LockFree &&lf) : m_algorithm{std::move(lf)}, m_helpqueue{} {
	  m_free.resize(N);
	  std::generate(m_free.begin(), m_free.end(), [n = 0]() mutable { return n++; });
	}
  };

 public: //< Construction API
  explicit WaitFreeSimulatorHandle(LockFree algorithm)
	  : m_id{0}, m_common_data{std::make_shared<Common>(algorithm)} {
  }

  // TODO: Check notes
  auto Fork() -> std::optional<WaitFreeSimulatorHandle<LockFree, N>> {
	auto lock = std::lock_guard<std::mutex>{m_common_data->m_free_lock};
	if (m_common_data->m_free.empty()) {
	  return {};
	}
	auto next_id = m_common_data->m_free.back();
	m_common_data->m_free.pop_back();
	return WaitFreeSimulatorHandle{next_id, m_common_data};
  }

  /// TODO: Should be moved to destructor, but the id is retired in other situations as well - thus, the additional function.
  void Retire() {
	const auto lock = std::lock_guard{m_common_data->m_free_lock};
	m_common_data->m_free.push_back(m_id);
  }

 public: //< Perform operation API

  /// \brief Runs the actual simulation
  /// \param input The given input
  /// \return The output of the operation
  auto simulate(const typename LockFree::Input &input) -> typename LockFree::Output {}

  /// \brief Checks whether other threads need help with a certain operation and tries to help them
  auto help() -> void {}

  /// \brief Helps a specific operation
  /// \note After exiting this function the operation encapsulation in `op_box` will be completed
  /// \param op_box	The operation box containing a ptr to the operation which requires help
  auto help(const OperationRecordBox &op_box) -> void {}

 private:
  WaitFreeSimulatorHandle(Id_type id, std::shared_ptr<Common> common_ptr)
	  : m_common_data{common_ptr}, m_id{id} {}

 private:
  std::shared_ptr<Common> m_common_data;
  Id_type m_id;
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
