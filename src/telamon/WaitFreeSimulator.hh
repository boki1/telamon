#ifndef TELAMON_WAIT_FREE_SIMULATOR_HH_
#define TELAMON_WAIT_FREE_SIMULATOR_HH_

#include <concepts>
#include <variant>
#include <vector>
#include <thread>
#include <algorithm>
#include <mutex>
#include <numeric>
#include <utility>
#include <utility>
#include <optional>
#include <span>

#include <extern/expected_lite/expected.hpp>

#include "HelpQueue.hh"
#include "OperationHelping.hh"

namespace telamon_simulator {

template<typename LockFree, const int N = 16, typename = std::enable_if<(N > 0)>> requires NormalizedRepresentation<LockFree>
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

  /// TODO: Check notes. Should be moved to destructor, but the id is retired in other situations as well - thus, the additional function.
  void Retire() {
	const auto lock = std::lock_guard{m_common_data->m_free_lock};
	m_common_data->m_free.push_back(m_id);
  }

 public: //< Perform operation API

  /// \brief Runs the actual simulation
  /// \param input The given input
  /// \return The output of the operation
  auto simulate(const typename LockFree::Input &input) -> typename LockFree::Output {}

 private: //< Internal operations
  /// \brief Checks whether other threads need help with a certain operation and tries to help them
  /// \details Implemented using the state of the operation and keep track of any modifications which occur during its processing
  auto help() -> void {}

  /// \brief Helps a specific operation
  /// \note After exiting this function the operation encapsulation in `op_box` will be completed
  /// \param op_box	The operation box containing a ptr to the operation which requires help
  auto help(const OperationRecordBox<LockFree> &op_box) -> void {}

  /// \TODO Remove template <typename U>
  /// \brief Make progress on each of the CAS-es required by the specific operation based on their state
  /// \param cas_list List of the CAS-es required by the specific operation
  /// \return TODO figure out
  template<typename U>
  auto commit(std::span<U> cas_list) -> void {}

 private:
  WaitFreeSimulatorHandle(Id_type id, std::shared_ptr<Common> common_ptr)
	  : m_common_data{common_ptr}, m_id{id} {}

 private:
  std::shared_ptr<Common> m_common_data;
  Id_type m_id;
};

}

#endif // TELAMON_WAIT_FREE_SIMULATOR_HH_
