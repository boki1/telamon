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

#include <extern/expected_lite/expected.hpp>

#include "HelpQueue.hh"
#include "OperationHelping.hh"

/// \brief Used by std::visit for the helping operation in the simulator
template<class... T>
struct OverloadedVisitor : T ... { using T::operator()...; };

namespace telamon_simulator {

namespace telamon_private {
template<NormalizedRepresentation LockFree, const int N = 16>
class WaitFreeSimulator {
  using Id = int;
  using Input = typename LockFree::Input;
  using Output = typename LockFree::Output;
  using Commit = typename LockFree::Commit;

  using OpRecord = OperationRecord<LockFree>;
  using OpBox = OperationRecordBox<LockFree>;
  using OpState = typename OperationRecord<LockFree>::OperationState;

  template<typename T, typename Err = std::monostate>
  using OptionalResultOrError = nonstd::expected<std::optional<T>, Err>;

 public:
  explicit WaitFreeSimulator (const LockFree &lf) : m_algorithm{lf}, m_helpqueue{} {}

  explicit WaitFreeSimulator (LockFree &&lf) : m_algorithm{std::move(lf)}, m_helpqueue{} {}

 public:

  /// \brief 	Runs the actual simulation
  /// \param 	input The given input
  /// \details	First, the operation is executed as if it was lock-free (the fast-path). If it fails FAST_PATH_RETRY_THRESHOLD number of times or if
  ///           the contention threshold is reached, the fast-path is abandoned and the operation is switched to the slow-path, which asks the other
  /// 			executing threads for help.
  /// \return 	The output of the operation
  auto run (const Id id, const Input &input) -> Output {
	  auto contention_counter = ContentionFailureCounter{};
	  try_help_others(id);

	  for (int i = 0; i < ContentionFailureCounter::FAST_PATH_RETRY_THRESHOLD; ++i) {
		  auto fp_result = fast_path(input, contention_counter);
		  if (fp_result.has_value()) {
			  return fp_result.value();
		  }
		  if (contention_counter.detect()) {
			  break;
		  }
	  }

	  return slow_path(id, input);
  }

  /// \brief 	Checks whether other threads need help with a certain operation and tries to help them
  auto try_help_others (const Id id) -> void {
	  auto front = m_helpqueue.peek_front();
	  if (front.has_value()) {
		  help(front.value());
	  }
  }

 private:
  /// \brief	Helps an operation in the precas stage
  auto help_precas (OpBox &op_box, const OpRecord &op, const typename OpRecord::PreCas &state) -> OptionalResultOrError<OpRecord *> {
	  auto failures = ContentionFailureCounter{};

	  // Generate CAS-list
	  auto desc = m_algorithm.generator(op.input(), failures);
	  if (!desc.has_value()) {
		  return nonstd::make_unexpected(std::monostate{});
	  }

	  auto updated_state = OpState{typename OpRecord::ExecutingCas(desc.value())};
	  return new OpRecord{op, updated_state};
  }

  /// \brief	Helps an operation in the postcas stage
  auto help_postcas (OpBox &op_box, const OpRecord &op, const typename OpRecord::PostCas &state) -> OptionalResultOrError<OpRecord *> {
	  auto failures = ContentionFailureCounter{};

	  auto result_opt = m_algorithm.wrap_up(state.executed, state.cas_list, failures);
	  if (!result_opt.has_value()) {
		  // Contention encountered. Try again.
		  return nonstd::make_unexpected(std::monostate{});
	  }

	  if (auto result = result_opt.value(); result.has_value()) {
		  // Operation has been successfully executed.
		  auto updated_state = OpState{typename OpRecord::Completed(result.value())};
		  return new OpRecord{op, updated_state};
	  }

	  // Operation failed and has to be restarted.
	  auto updated_state = OpState{typename OpRecord::PreCas{}};
	  return new OpRecord{op, updated_state};
  }

  /// \brief	Helps an operation in the stage during cas execution
  auto help_executingcas (OpBox &op_box, const OpRecord &op, typename OpRecord::ExecutingCas &state) -> OptionalResultOrError<OpRecord *, int> {
	  auto failures = ContentionFailureCounter{};

	  auto result = commit(state.cas_list, failures);
	  if (!result.has_value()) {
		  if (auto err = result.error(); err.has_value()) {
			  // Contention encounter. Try again.
			  return std::optional<OpRecord *>{};
		  } else {
			  // Failed at a specific CAS
			  return nonstd::make_unexpected(err.value());
		  }
	  }

	  auto updated_op = new OpRecord{op, typename OpRecord::PostCas(state.cas_list, result)};
	  return updated_op;
  }

/// \brief 	Helps a specific operation
/// \note 	After exiting this function the operation encapsulation in `op_box` will be completed
/// \param 	op_box	The operation box containing a ptr to the operation which requires help
/// \details 	Implemented using the state of the operation and keep track of any modifications which occur during its processing
  auto help (OperationRecordBox<LockFree> &op_box) -> void {
	  using HelperVisitResult = std::pair<bool, OptionalResultOrError<OpRecord *>>;
	  while (true) {
		  auto op_ptr = op_box.ptr();
		  const auto &op = *op_ptr;

		  auto[continue_, updated_op] = std::visit(OverloadedVisitor{
			  [&] (const typename OpRecord::PreCas &arg) -> HelperVisitResult {
				auto result = help_precas(op_box, op, arg);
				bool continue_ = !result.has_value(); //< If there is contention, try again (continue the outer loop)
				return std::make_pair(continue_, result);
			  },
			  [&] (const typename OpRecord::PostCas &arg) -> HelperVisitResult {
				auto result = help_postcas(op_box, op, arg);
				bool continue_ = !result.has_value(); //< If there is contention, try again (continue the outer loop)
				return std::make_pair(continue_, result);
			  },
			  [&] (const typename OpRecord::ExecutingCas &arg) -> HelperVisitResult {
				auto &mut_arg = *const_cast<typename OpRecord::ExecutingCas *>(&arg);
				auto result_ = help_executingcas(op_box, op, mut_arg);
				// continue_ is set iff the execution failed and _none_ of the CAS-es was successfully performed
				bool continue_ = result_.has_value() && !result_.value().has_value();
				// help_executingcas has a different return type and has to be "reformatted"

				// If continue_ is set then the value of result wil never be read
				if (continue_) { return std::make_pair(continue_, nullptr); }

				// value().value() is fine because we would have already returned if there wasn't a value in the optional<>
				if (result_.has_value()) { return std::make_pair(continue_, result_.value().value()); }

				auto unit = nonstd::make_unexpected(std::monostate{});
				return std::make_pair(continue_, unit);
			  },
			  [&] (const typename OpRecord::Completed &arg) -> HelperVisitResult {
				m_helpqueue.try_pop_front(op_box);
				return HelperVisitResult{};
			  }
		  }, op.state());

		  if (continue_) { continue; }
		  if (!updated_op.has_value()) { break; }   //< Completed

		  // Safety for calling value().value(): continue_ would be true and thus we wouldn't have reached this line
		  OpRecord *updated_op_ptr = updated_op.value().value();
		  if (!op_box.atomic_ptr().compare_exchange_strong(op_ptr, updated_op_ptr)) {
			  // Unsuccessful, therefore we can safely deallocate the OpRecord we created (It never got shared with other threads).
			  delete updated_op_ptr;
		  }
	  }
  }

/// \brief 	Make progress on each of the CAS-es required by the specific operation based on their state
/// \param 	cas_list List oreturn f the CAS-es required by the specific operation
/// \return 	Either a success or an error:
/// 				Success => The CAS was/were performed successfully
/// 				Error => Either there was contention during the CAS execution, or the CAS failed (the params were incorrect)
  auto commit (Commit &cas_list, ContentionFailureCounter &failures) -> nonstd::expected<std::monostate, std::optional<int>> {
	  for (int i = 0; auto &cas : cas_list) {
		  switch (auto state = cas.state()) {
			  case CasStatus::Failure: return nonstd::make_unexpected(i);
				  break;
			  case CasStatus::Success: cas.clear_bit();
				  break;
			  case CasStatus::Pending: {
				  if (auto result = cas.execute(failures); !result.has_value()) {
					  return nonstd::make_unexpected(std::nullopt);
				  }
				  if (cas.has_modified_bit()) {
					  cas.swap_state(CasStatus::Pending, CasStatus::Success);
					  if (cas.state() == CasStatus::Success) {
						  cas.clear_bit();
					  }
				  }
				  if (cas.state() != CasStatus::Success) {
					  cas.set_state(CasStatus::Failure);
					  return nonstd::make_unexpected(i);
				  }
				  break;
			  }
		  }
		  ++i;
	  }

	  return std::monostate{};
  }

/// \brief 		The slow-path
/// \details 	The slow-path begins as the thread-owner of the operation enqueues a succinct description of the operation it has failed to complete
/// 			in the fast path (an OperationRecordBox).
  auto slow_path (const Id id, const Input &input) -> Output {
	  // Enqueue description of the operation
	  auto op_box = OperationRecordBox<LockFree>{id, typename OpRecord::PreCas{}, input};
	  m_helpqueue.push_back(id, op_box);

	  // Help until operation is complete
	  using StateCompleted = typename OperationRecord<LockFree>::Completed;
	  while (true) {
		  auto updated_state = op_box.ptr()->state();
		  if (std::holds_alternative<StateCompleted>(updated_state)) {
			  auto sp_result = std::get<StateCompleted>(updated_state);
			  return sp_result.output;
		  }
		  try_help_others(id);
	  }
  }

/// \brief The fast-path. Directly invokes the fast_path of the algorithm being executed
  auto fast_path (const Input &input, ContentionFailureCounter &contention_counter) -> std::optional<Output> {
	  return m_algorithm.fast_path(input, contention_counter);
  }

 private:
  LockFree m_algorithm;
  helpqueue::HelpQueue<OperationRecordBox<LockFree>, N> m_helpqueue;
};

}

template<NormalizedRepresentation LockFree, const int N = 16>
class WaitFreeSimulatorHandle {
 public:
  using Id = int;
  using Input = typename LockFree::Input;
  using Output = typename LockFree::Output;
  using Commit = typename LockFree::Commit;

  using OpRecord = OperationRecord<LockFree>;
  using OpBox = OperationRecordBox<LockFree>;
  using OpState = typename OperationRecord<LockFree>::OperationState;

  template<typename T, typename Err = std::monostate>
  using OptionalResultOrError = nonstd::expected<std::optional<T>, Err>;

  using Simulator = telamon_private::WaitFreeSimulator<LockFree, N>;

 public:
  struct MetaData {
	std::vector<Id> m_free;
	std::mutex m_free_lock;
  };

 public: //< Construction API
  explicit WaitFreeSimulatorHandle (LockFree algorithm)
	  : m_id{0}, m_simulator{std::make_shared<Simulator>(algorithm)}, m_meta{std::make_shared<MetaData>()} {
	  // Safe to access m_meta without atomic load because it has never been shared
	  m_meta->m_free.resize(N - 1);
	  std::iota(m_meta->m_free.begin(), m_meta->m_free.end(), 1);
	  static_assert(N > 0, "N has to be a positive integer.");
  }

  auto fork () -> std::optional<WaitFreeSimulatorHandle<LockFree, N>> {
	  auto meta = std::atomic_load(&m_meta);
	  const auto lock = std::lock_guard<std::mutex>{meta->m_free_lock};
	  if (meta->m_free.empty()) {
		  return {};
	  }
	  auto next_id = meta->m_free.back();
	  meta->m_free.pop_back();
	  return WaitFreeSimulatorHandle{next_id, m_simulator, meta};
  }

  auto retire () -> void {
	  auto meta = std::atomic_load(&m_meta);
	  const auto lock = std::lock_guard{meta->m_free_lock};
	  meta->m_free.push_back(m_id);
  }

  auto submit (const Input &input) -> Output {
	  auto sim = std::atomic_load(&m_simulator);
	  return sim->run(m_id, input);
  }

  auto help () -> void {
	  auto sim = std::atomic_load(&m_simulator);
	  sim->try_help_others(m_id);
  }

 private:
  WaitFreeSimulatorHandle (Id id, std::shared_ptr<Simulator> t_simulator, std::shared_ptr<MetaData> t_meta)
	  : m_simulator{t_simulator}, m_id{id}, m_meta{t_meta} {}

 private:
  std::shared_ptr<Simulator> m_simulator{};
  std::shared_ptr<MetaData> m_meta{};
  Id m_id;
};

}

#endif // TELAMON_WAIT_FREE_SIMULATOR_HH_
