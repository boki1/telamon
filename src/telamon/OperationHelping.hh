#ifndef TELAMON_OPERATION_HELPING_HH
#define TELAMON_OPERATION_HELPING_HH

#include <thread>
#include <variant>
#include <utility>

#include "NormalizedRepresentation.hh"

namespace telamon_simulator {

/// \brief A class which represents a single operation contained in a OperationRecordBox
template<NormalizedRepresentation LockFree>
class OperationRecord {
  using Output = typename LockFree::Output;
  using Input = typename LockFree::Input;
  using Commit = typename LockFree::Commit;
 public:
  /// \brief Meta data related to CAS which is still pending
  struct PreCas {
	PreCas () = default;
	PreCas (const PreCas &) = default;
  };
  /// \brief Meta data related to CAS which is going to be executed
  struct ExecutingCas {
	Commit cas_list;
	explicit ExecutingCas (Commit t_cas_list) : cas_list{std::move(t_cas_list)} {}
	ExecutingCas (const ExecutingCas &) = default;
  };
  /// \brief Meta data related to CAS which has already been executed
  struct PostCas {
	Commit cas_list;
	nonstd::expected<std::monostate, std::optional<int>> executed;
	PostCas (Commit t_cas_list, nonstd::expected<std::monostate, std::optional<int>> t_executed) : cas_list{t_cas_list},
	                                                                                               executed{std::move(t_executed)} {}
	PostCas (const PostCas &) = default;
  };
  /// \brief Meta data related to CAS which is complete.
  struct Completed {
	Output output;
	explicit Completed (Output t_output) : output{std::move(t_output)} {}
	Completed (const Completed &) = default;
  };

  using OperationState = std::variant<PreCas, ExecutingCas, PostCas, Completed>;

 public:
  OperationRecord (int t_owner, OperationState t_state, const typename LockFree::Input &t_input)
	  : m_owner{t_owner},
	    m_input{t_input},
	    m_state{std::move(t_state)} {}

  OperationRecord (const OperationRecord &copy, OperationState t_state)
	  : m_owner{copy.m_owner},
	    m_input{copy.m_input},
	    m_state{std::move(t_state)} {}

  OperationRecord (OperationRecord &&) noexcept = default;
  OperationRecord (const OperationRecord &) = default;

  bool operator== (const OperationRecord &rhs) const {
	  return std::tie(m_owner, m_input) == std::tie(rhs.m_owner, rhs.m_input);
  }
  bool operator!= (const OperationRecord &rhs) const {
	  return !(rhs == *this);
  }

 public:
  [[nodiscard]] auto owner () const noexcept -> int { return m_owner; }
  [[nodiscard]] auto state () const noexcept -> OperationState { return m_state; }
  [[nodiscard]] auto input () const noexcept -> const typename LockFree::Input & { return m_input; }

  [[maybe_unused]] void set_state (const OperationState &t_state) noexcept { m_state = t_state; }

 private:
  int m_owner;
  OperationState m_state;
  const typename LockFree::Input &m_input;

  static_assert(std::is_copy_constructible_v<OperationState>);
};

/// \brief A class which represents a single operation stored in the help queue
template<typename LockFree> requires NormalizedRepresentation<LockFree>
class OperationRecordBox {
 public:
  OperationRecordBox (int t_owner, typename OperationRecord<LockFree>::OperationState t_state, const typename LockFree::Input &t_input)
	  : m_ptr{new OperationRecord<LockFree>{t_owner, t_state, t_input}} {}

  OperationRecordBox (OperationRecordBox &&rhs) noexcept: m_ptr{std::move(rhs.m_ptr.load())} {}
  OperationRecordBox (const OperationRecordBox &rhs) noexcept: m_ptr{rhs.m_ptr.load()} {}

  bool operator== (const OperationRecordBox &rhs) const {
	  return *m_ptr.load() == *rhs.m_ptr.load();
  }

  bool operator!= (const OperationRecordBox &rhs) const {
	  return !(rhs == *this);
  }

  [[nodiscard]] auto state() const noexcept -> typename OperationRecord<LockFree>::OperationState { return m_ptr.load()->state(); }

  [[nodiscard]] auto ptr () const noexcept -> OperationRecord<LockFree> * { return m_ptr.load(); }

  [[maybe_unused]] auto atomic_ptr () noexcept -> std::atomic<OperationRecord<LockFree> *> & { return m_ptr; }

  [[maybe_unused]] auto nonatomic_ptr () const noexcept -> OperationRecord<LockFree> * { return m_ptr; }

  /// \brief Atomically swaps the pointer m_ptr with pointer the given box record
  auto swap (OperationRecordBox desired, OperationRecordBox *expected_ptr) -> bool {
	  auto desired_ptr = new OperationRecord{std::move(desired)};        //< TODO: Use folly/Hazptr
	  return m_ptr.compare_exchange_strong(expected_ptr, desired_ptr);
  }

 private:
  // TODO: Use folly/Hazptr
  std::atomic<OperationRecord<LockFree> *> m_ptr;
};

}

#endif // TELAMON_OPERATION_HELPING_HH
