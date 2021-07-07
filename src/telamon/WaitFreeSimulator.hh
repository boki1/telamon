#ifndef TELAMON_WAIT_FREE_SIMULATOR_HH_
#define TELAMON_WAIT_FREE_SIMULATOR_HH_

#include <atomic>
#include <memory>
#include <concepts>
#include <variant>
#include <vector>
#include <thread>
#include <algorithm>
#include <mutex>
#include <numeric>
#include <utility>
#include <type_traits>
#include <optional>
#include <span>

#include <extern/expected_lite/expected.hpp>

#include "HelpQueue.hh"
#include "OperationHelping.hh"

/// \brief Used by std::visit for the helping operation in the simulator
template<class... T>
struct OverloadedVisitor : T ... { using T::operator()...; };

namespace telamon_simulator {

template<NormalizedRepresentation LockFree, const int N = 16>
class WaitFreeSimulatorHandle {
 public:
  using Id = int;
  using Input = typename LockFree::Input;
  using Output = typename LockFree::Output;
  using CommitDescriptor = typename LockFree::CommitDescriptor;

  using OpRecord = OperationRecord<LockFree>;
  using OpBox = OperationRecordBox<LockFree>;

  template<typename T, typename Err = std::monostate>
  using OptionalResultOrError = nonstd::expected<std::optional<T>, Err>;

 private:
  /// \brief Encapsulates the data shared between all simulator handles
  struct MetaData;

 public: //< Construction API
  explicit WaitFreeSimulatorHandle (LockFree algorithm)
	  : m_id{0}, m_meta{std::make_shared<MetaData>(algorithm)} {
	  static_assert(N > 0, "N has to be a positive integer.");
  }

  // TODO: Check notes
  auto Fork () -> std::optional<WaitFreeSimulatorHandle<LockFree, N>> {
	  const auto lock = std::lock_guard<std::mutex>{m_meta->m_free_lock};
	  if (m_meta->m_free.empty()) {
		  return {};
	  }
	  auto next_id = m_meta->m_free.back();
	  m_meta->m_free.pop_back();
	  return WaitFreeSimulatorHandle{next_id, m_meta};
  }

  /// TODO: Check notes. Should be moved to destructor, but the id is retired in other situations as well - thus, the additional function.
  void Retire () {
	  const auto lock = std::lock_guard{m_meta->m_free_lock};
	  m_meta->m_free.push_back(m_id);
  }

 public: //< Perform operation API

  /// \brief 	Runs the actual simulation
  /// \param 	input The given input
  /// \details	First, the operation is executed as if it was lock-free (the fast-path). If it fails FAST_PATH_RETRY_THRESHOLD number of times or if
  ///           the contention threshold is reached, the fast-path is abandoned and the operation is switched to the slow-path, which asks the other
  /// 			executing threads for help.
  /// \return 	The output of the operation
  auto simulate (const Input &input) -> Output {
	  auto contention_counter = ContentionFailureCounter{};
	  try_help_others();

	  for (int i = 0; i < ContentionFailureCounter::FAST_PATH_RETRY_THRESHOLD; ++i) {
		  auto fp_result = fast_path(input, contention_counter);
		  if (fp_result.has_value()) {
			  return fp_result.value();
		  }

		  if (contention_counter.detect()) {
			  break;
		  }
	  }

	  return slow_path(input);
  }

 private: //< Internal operations
  /// \brief 	Checks whether other threads need help with a certain operation and tries to help them
  auto try_help_others () -> void {
	  auto meta = atomic_load(&m_meta);
	  auto front = meta->m_helpqueue.peek_front();
	  if (front.has_value()) {
		  help(front.value());
	  }
  }

  auto help_precas (OpBox &op_box, const OpRecord &op, typename OpRecord::PreCas &state) -> OptionalResultOrError<OpRecord> {
	  auto meta = atomic_load(&m_meta);
	  ContentionFailureCounter failures;
	  auto desc = meta->m_algorithm.generator(op.input(), failures);
	  if (desc.has_value()) {
		  return desc.value();
	  }
	  return nonstd::unexpect;
  }

  auto help_postcas (OpBox &op_box, const OpRecord &op, typename OpRecord::PostCas &state) -> OptionalResultOrError<OpRecord> {}

  auto help_executingcas (OpBox &op_box, const OpRecord &op, typename OpRecord::ExecutingCas &state) -> OptionalResultOrError<OpRecord> {}

  /// \brief 	Helps a specific operation
  /// \note 	After exiting this function the operation encapsulation in `op_box` will be completed
  /// \param 	op_box	The operation box containing a ptr to the operation which requires help
  /// \details 	Implemented using the state of the operation and keep track of any modifications which occur during its processing
  auto help (OperationRecordBox<LockFree> &op_box) -> void {
	  while (true) {
		  auto op = op_box.ptr();

		  auto updated_op = std::visit(OverloadedVisitor{
			  [&] (const typename OpRecord::PreCas &arg) -> OptionalResultOrError<OpRecord> {
				return help_precas(op_box, op, arg);
			  },
			  [&] (const typename OpRecord::Postcas &arg) -> OptionalResultOrError<OpRecord> {
				return help_postcas(op_box, op, arg);
			  },
			  [&] (const typename OpRecord::ExecutingCas &arg) -> OptionalResultOrError<OpRecord> {
				return help_executingcas(op_box, op, arg);
			  },
			  [&] (const typename OpRecord::Completed &arg) -> OptionalResultOrError<OpRecord> {
				auto meta = atomic_load(&m_meta);
				meta->m_algorithm.try_pop_front(op_box);
				return {};
			  }
		  }, op->state());

		  if (!updated_op) {
			  break;
		  }

		  // Perform CAS

	  }
  }

  /// \brief 	Make progress on each of the CAS-es required by the specific operation based on their state
  /// \param 	cas_list List of the CAS-es required by the specific operation
  /// \return 	TODO figure out
  auto commit (std::span<std::monostate> /* TODO */ cas_list) -> void {}

  /// \brief 	The slow-path
  /// \details 	The slow-path begins as the thread-owner of the operation enqueues a succinct description of the operation it has failed to complete
  /// 			in the fast path (an OperationRecordBox).
  auto slow_path (const Input &input) -> Output {
	  // Enqueue description of the operation
	  auto meta = atomic_load(&m_meta);
	  auto op_box = OperationRecordBox<LockFree>{};
	  meta->m_helpqueue.push_back(m_id, op_box);

	  // Help until operation is complete
	  using StateCompleted = typename OperationRecord<LockFree>::Completed;
	  while (true) {
		  auto updated_state = op_box.ptr()->state();
		  if (std::holds_alternative<StateCompleted>(updated_state)) {
			  auto sp_result = std::get<StateCompleted>(updated_state);
			  return sp_result.output;
		  }
		  try_help_others();
	  }
  }

  /// \brief The fast-path. Directly invokes the fast_path of the algorithm being executed
  auto fast_path (const Input &input, ContentionFailureCounter &contention_counter) -> nonstd::expected<Output, ContentionFailureCounter> {
	  auto meta = atomic_load(&m_meta);
	  return meta->m_algorithm.fast_path(input, contention_counter);
  }

 private:
  WaitFreeSimulatorHandle (Id id, std::shared_ptr<MetaData> common_ptr)
	  : m_meta{common_ptr}, m_id{id} {}

 private:
  std::shared_ptr<MetaData> m_meta{};
  Id m_id;
};

template<NormalizedRepresentation LockFree, const int N>
struct WaitFreeSimulatorHandle<LockFree, N>::MetaData {
  LockFree m_algorithm;
  helpqueue::HelpQueue<OperationRecordBox<LockFree>, N> m_helpqueue;
  std::vector<Id> m_free;
  std::mutex m_free_lock;

  explicit MetaData (const LockFree &lf) : m_algorithm{lf}, m_helpqueue{} {
	  m_free.resize(N - 1);
	  std::iota(m_free.begin(), m_free.end(), 1);
  }

  explicit MetaData (LockFree &&lf) : m_algorithm{std::move(lf)}, m_helpqueue{} {
	  m_free.resize(N);
	  std::generate(m_free.begin(), m_free.end(), [n = 0] () mutable { return n++; });
  }
};

}

#endif // TELAMON_WAIT_FREE_SIMULATOR_HH_
