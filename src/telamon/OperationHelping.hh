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
	typename LockFree::CommitDescriptor cas_list;
	explicit ExecutingCas (const typename LockFree::CommitDescriptor &t_cas_list) : cas_list{t_cas_list} {}
  };
  struct PostCas {
	typename LockFree::CommitDescriptor cas_list;
	nonstd::expected<std::monostate, std::optional<int>> executed;
  };
  struct Completed { typename LockFree::Output output; };

  using OperationState = std::variant<PreCas, ExecutingCas, PostCas, Completed>;

 public:
  OperationRecord (std::thread::id t_owner, OperationState t_state, const typename LockFree::Input &t_input)
	  : m_owner{t_owner},
	    m_state{t_state},
	    m_input{t_input} {}

  OperationRecord (const OperationRecord &copy, const OperationState &state)
	  : m_owner{copy.m_owner},
	    m_input{copy.m_input},
	    m_state{state} {}

 public:
  [[nodiscard]] auto owner () const noexcept -> std::thread::id { return m_owner; }
  [[nodiscard]] auto state () const noexcept -> const OperationState & { return m_state; }
  [[nodiscard]] auto input () const noexcept -> const typename LockFree::Input & { return m_input; }

  [[maybe_unused]] void set_state (const OperationState &t_state) noexcept { m_state = t_state; }

 private:
  std::thread::id m_owner;
  OperationState m_state;
  const typename LockFree::Input &m_input;
};

template<typename LockFree> requires NormalizedRepresentation<LockFree>
class OperationRecordBox {
 public:

//  template<typename... Args>
//  explicit OperationRecordBox (Args &&... args) : m_ptr{new OperationRecord<LockFree>{std::forward<Args>(args)...}} {}

  OperationRecordBox (std::thread::id t_owner, const typename OperationRecord<LockFree>::OperationState &t_state, const typename LockFree::Input
  &t_input) : m_ptr{new OperationRecord<LockFree>{t_owner, t_state, t_input}} {}

  OperationRecordBox (OperationRecordBox &&) noexcept = delete;
  OperationRecordBox (const OperationRecordBox &rhs) noexcept
	  : m_ptr{rhs.m_ptr.load()} {}

  auto ptr () const -> OperationRecord<LockFree> * { return m_ptr.load(); }
  [[maybe_unused]] auto atomic_ptr () const -> std::atomic<OperationRecord<LockFree> *> { return m_ptr; }
  [[maybe_unused]] auto nonatomic_ptr () const -> OperationRecord<LockFree> * { return m_ptr; }

  /// \brief Atomically swaps the pointer m_ptr with pointer the given box record
  auto swap (OperationRecordBox desired, OperationRecordBox *expected_ptr) -> bool {
	  auto desired_ptr = new OperationRecord{std::move(desired)};        //< TODO: Use folly/Hazptr
	  return m_ptr.compare_exchange_strong(expected_ptr, desired_ptr);
  }

 private:
  // TODO: Use folly/Hazptr
  std::atomic<OperationRecord<LockFree> *> m_ptr;
};

//template<NormalizedRepresentation LockFree>
//class OperationRecord<LockFree>::StateMachineVisitor {
//
//  auto operator() (const PreCas &status) -> OperationRecordBox<LockFree> {}
//  auto operator() (const ExecuteCas &status) -> OperationRecordBox<LockFree> {}
//  auto operator() (const PostCas &status) -> OperationRecordBox<LockFree> {}
//  auto operator() (const Completed &status) -> OperationRecordBox<LockFree> {}
//
//};


}

#endif // TELAMON_OPERATION_HELPING_HH
