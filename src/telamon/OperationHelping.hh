#ifndef TELAMON_OPERATION_HELPING_HH
#define TELAMON_OPERATION_HELPING_HH

#include <thread>
#include <variant>
#include <utility>

#include "NormalizedRepresentation.hh"

namespace telamon_simulator {

template<NormalizedRepresentation LockFree>
class OperationRecord {
 public:
  struct PreCas { /* empty */ };
  struct ExecutingCas {
	typename LockFree::Commit cas_list;
  };
  struct PostCas {
	typename LockFree::Commit cas_list;
	nonstd::expected<std::monostate, std::optional<int>> executed;
  };
  struct Completed { typename LockFree::Output output; };

  using OperationState = std::variant<PreCas, ExecutingCas, PostCas, Completed>;

 public:
  OperationRecord (int t_owner, OperationState t_state, const typename LockFree::Input &t_input)
	  : m_owner{t_owner},
	    m_state{std::move(t_state)},
	    m_input{t_input} {}

  OperationRecord (const OperationRecord &copy, OperationState state)
	  : m_owner{copy.m_owner},
	    m_input{copy.m_input},
	    m_state{std::move(state)} {}

  OperationRecord (OperationRecord &&) noexcept = default;
  OperationRecord(const OperationRecord &) = delete;

 public:
  [[nodiscard]] auto owner () const noexcept -> int { return m_owner; }
  [[nodiscard]] auto state () const noexcept -> OperationState { return m_state; }
  [[nodiscard]] auto input () const noexcept -> const typename LockFree::Input & { return m_input; }

  [[maybe_unused]] void set_state (const OperationState &t_state) noexcept { m_state = t_state; }

 private:
  int m_owner;
  OperationState m_state;
  const typename LockFree::Input &m_input;
};

template<typename LockFree> requires NormalizedRepresentation<LockFree>
class OperationRecordBox {
 public:
  OperationRecordBox (int t_owner, typename OperationRecord<LockFree>::OperationState t_state, const typename LockFree::Input &t_input)
	  : m_ptr{new OperationRecord<LockFree>{t_owner, t_state, t_input}} {}

  OperationRecordBox (OperationRecordBox &&) noexcept = delete;
  OperationRecordBox (const OperationRecordBox &rhs) noexcept
	  : m_ptr{rhs.m_ptr.load()} {}

  bool operator== (const OperationRecordBox &rhs) const {
	  // TODO: Should this compare the values of ptr or the ptr itself
	  return m_ptr == rhs.m_ptr;
  }
  bool operator!= (const OperationRecordBox &rhs) const {
	  return !(rhs == *this);
  }

  auto ptr () const noexcept -> OperationRecord<LockFree> * { return m_ptr.load(); }
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
