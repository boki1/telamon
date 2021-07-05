#ifndef TELAMON_OPERATION_HELPING_HH
#define TELAMON_OPERATION_HELPING_HH

#include <thread>
#include <variant>
#include <utility>

#include "NormalizedRepresentation.hh"

namespace telamon_simulator {

template<typename LockFree> requires NormalizedRepresentation<LockFree>
class OperationRecord {
 public:
  struct PreCas { /* empty */ };
  struct ExecuteCas { typename LockFree::CommitDescriptor cas_list; nonstd::expected<std::monostate, int> outcome; };
  struct PostCas { typename LockFree::CommitDescriptor cas_list; };
  struct Completed { typename LockFree::Output output; };
  using OperationState = std::variant<PreCas, ExecuteCas, PostCas, Completed>;

 public:
  OperationRecord(std::thread::id t_owner, OperationState t_state)
	  : m_owner{t_owner},
		m_state{t_state} {}

 private:
  std::thread::id m_owner;
  OperationState m_state;
};

template<typename LockFree> requires NormalizedRepresentation<LockFree>
class OperationRecordBox {
 public:

  template<typename... Args>
  explicit OperationRecordBox(Args &&... args) : m_ptr{new OperationRecord{std::forward<Args>(args)...}} {}
  OperationRecordBox(OperationRecordBox &&) noexcept = default;
  OperationRecordBox(OperationRecordBox &) noexcept = default;

  auto ptr() const -> OperationRecord<LockFree> * {
	return m_ptr;
  }

  /// \brief Atomically swaps the pointer m_ptr with pointer the given box record
  auto swap(OperationRecordBox desired, OperationRecordBox *expected_ptr) -> bool {
	auto desired_ptr = new OperationRecord{std::move(desired)};        //< TODO: Use folly/Hazptr
	return m_ptr.compare_exchange_strong(expected_ptr, desired_ptr);
  }

 private:
  // TODO: Use folly/Hazptr
  std::atomic<OperationRecord<LockFree> *> m_ptr;
};

}

#endif // TELAMON_OPERATION_HELPING_HH
