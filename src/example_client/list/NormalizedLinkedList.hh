#ifndef NORMALIZED_HARRIS_LINKED_LIST_TELAMON_CLIENT_H
#define NORMALIZED_HARRIS_LINKED_LIST_TELAMON_CLIENT_H

#include <atomic>
#include <utility>
#include <optional>
#include <concepts>
#include <ranges>
#include <limits>
#include <array>

#include <extern/expected_lite/expected.hpp>

#include <telamon/WaitFreeSimulator.hh>
#include <telamon/Versioning.hh>
namespace tsim = telamon_simulator;

namespace normalizedlinkedlist {

/// \brief 		Implementation of Harris' Linked list
/// \details 	This is the original paper https://www.microsoft.com/en-us/research/wp-content/uploads/2001/10/2001-disc.pdf
/// \tparam 	T Has to be either an integral type or a floating type
template<typename T> requires std::integral<T> || std::floating_point<T>
class LinkedList {
 public:
  struct MarkMeta {
	// Denotes whether the node logically removed
	bool marked = false;
	bool operator== (const MarkMeta &rhs) const { return marked == rhs.marked; }
  };

  class Node {
   public:
	using SuccessorLink = tsim::versioning::VersionedAtomic<Node *, MarkMeta>;
	explicit Node (const T &value, Node *next = nullptr) : m_value{value}, m_next{next} {}

   public:
	[[nodiscard]] bool is_removed () const noexcept {
		return m_next.transform([] (auto _value, auto _version, MarkMeta _meta) {
		  return _meta.marked;
		});
	}
	[[nodiscard]] auto value () const noexcept -> T {
		return m_value;
	}

	[[nodiscard]] auto next_atomic () noexcept -> SuccessorLink & { return m_next; }

	[[nodiscard]] auto next () const noexcept -> Node * { return m_next.load()->value; }

	bool mark (bool t_mark = true) noexcept {
		auto val = next();
		tsim::ContentionFailureCounter failures;
		return m_next.template compare_exchange_strong(val, version(), val, MarkMeta{t_mark}, failures);
	}

	void set_next (Node *t_next) noexcept { m_next.store(t_next); }

	[[nodiscard]] auto version () const noexcept -> tsim::versioning::VersionNum {
		return m_next.transform([] (auto _value, tsim::versioning::VersionNum _version, auto _meta) {
		  return _version;
		});
	}

	[[nodiscard]] auto meta () const noexcept -> MarkMeta {
		return m_next.transform([] (auto _value, auto _version, MarkMeta _meta) {
		  return _meta;
		});
	}

   private:
	T m_value;
	SuccessorLink m_next;
  };

 public:
  LinkedList ()     // TODO: Hazptr
	  : m_head{new Node{std::numeric_limits<T>::min()}},
	    m_tail{new Node{std::numeric_limits<T>::max()}},
	    m_size{0} {
	  m_head->set_next(m_tail);
  }

 public:
  auto search (T value) -> std::pair<Node &, Node &> {
	  tsim::ContentionFailureCounter failures{};
	  Node *left_ptr{nullptr};
	  Node *left_next{nullptr};
	  while (true) {
		  Node *right_ptr{nullptr};
		  Node *current = head();
		  Node *next = head()->next();

		  /// 1. Find left and right pointers
		  for (/* empty */; is_removed(next) || current->value() < value; next = current->next()) {
			  if (!is_removed(next)) {
				  left_ptr = current;
				  left_next = next;
			  }
			  current = next;
			  if (current == tail()) {
				  break;
			  }
		  }
		  right_ptr = current;

		  /// 2. Check nodes are adjacent
		  if (left_next == right_ptr) {
			  if (right_ptr != tail() && right_ptr->next()->is_removed()) continue;
			  return std::pair<Node &, Node &>{*left_ptr, *right_ptr};
		  }

		  /// 3. Remove one or more marked nodes
		  if (left_ptr && left_ptr->next_atomic().compare_exchange_strong(left_next, left_ptr->version(), right_ptr, left_ptr->meta(), failures)) {
			  m_deleted.fetch_add(1);
		  }
		  if (right_ptr != m_tail && right_ptr->next()->is_removed()) continue;
		  return std::pair<Node &, Node &>{*left_ptr, *right_ptr};
	  }
  }

  auto appears (T value) -> bool {
	  auto *const tail_ = tail();
	  for (auto *it = head()->next(); it != tail_; it = it->next()) {
		  if (is_removed(it)) { continue; }
		  auto actual = it->value();
		  if (actual > value) { break; }
		  if (actual == value) { return true; }
	  }
	  return false;
  }

  [[nodiscard]] auto size () -> std::size_t {
	  return count_if([&] (const auto *it) {
		return !is_removed(it);
	  });
  }

  [[nodiscard]] auto removed_not_deleted () const noexcept -> std::size_t {
	  return count_if([&] (auto *it) {
		return is_removed(it);
	  });
  }

  template<typename Predicate>
  auto count_if (Predicate fun) const noexcept -> std::size_t {
	  auto *const tail_ = tail();
	  size_t count_ = 0;
	  for (auto *it = head()->next(); it != tail_; it = it->next()) {
		  if (fun(it)) {
			  ++count_;
		  }
	  }
	  return count_;
  }

  [[maybe_unused]] [[nodiscard]] auto removed_and_deleted () const noexcept -> std::size_t { return m_deleted; }

 public:
  [[nodiscard]] auto tail () const noexcept -> Node * { return m_tail; }
  [[nodiscard]] auto head () const noexcept -> Node * { return m_head; }

  [[nodiscard]] static auto is_removed (const Node *const node) noexcept {
	  if (!node) {
		  return true;
	  }
	  return node->is_removed();
  }

 private:
  Node *m_head;
  Node *m_tail;
  std::atomic<std::size_t> m_size;
  std::atomic<std::size_t> m_deleted{0};

 public:
  class NormalizedInsert {
   public:
	class CasDescriptor {
	 public:
	  CasDescriptor (tsim::versioning::VersionedAtomic<Node *, MarkMeta> &t_target, Node *t_expected, Node *t_desired)
		  : m_target{t_target},
		    m_expected{t_expected},
		    m_desired{t_desired} {}

	  CasDescriptor (const CasDescriptor &rhs)
		  : m_target{rhs.m_target},
		    m_expected{rhs.m_expected},
		    m_desired{rhs.m_desired},
		    m_state{rhs.m_state.load()} {}

	 public:
	  [[nodiscard]] auto has_modified_bit () const noexcept -> bool {
		  return m_target.has_modified_bit();
	  }

	  auto clear_bit () noexcept {
		  return m_target.clear_modified_bit();
	  }

	  [[nodiscard]] auto state () const noexcept -> tsim::CasStatus { return m_state.load(); }

	  auto set_state (tsim::CasStatus new_state) noexcept { m_state.store(new_state); }

	  [[nodiscard]] auto swap_state (tsim::CasStatus expected, tsim::CasStatus desired) noexcept -> bool {
		  return m_state.compare_exchange_strong(expected, desired);
	  }

	  [[nodiscard]] auto execute (tsim::ContentionFailureCounter &failures) noexcept -> nonstd::expected<bool, std::monostate> {
		  return m_target.compare_exchange_strong(m_expected, m_target.version(), m_desired, m_desired->meta(), failures);
	  }

	 private:
	  std::atomic<tsim::CasStatus> m_state{tsim::CasStatus::Pending};
	  tsim::versioning::VersionedAtomic<Node *, MarkMeta> &m_target;
	  Node *m_expected;
	  Node *m_desired;
	};
	static_assert(std::is_copy_constructible_v<CasDescriptor>, "Commit type has to be copy-constructible.");
	static_assert(tsim::CasWithVersioning<CasDescriptor>, "Commit type has implement versioning.");

	using Input = T;
	using Output = bool;
	using Commit = std::array<CasDescriptor, 1>;

	explicit NormalizedInsert (LinkedList &t_lf) : m_lockfree{t_lf} {}

   public:
	auto generator (const Input &inp, tsim::ContentionFailureCounter &failures) -> std::optional<Commit> {
		auto[left, right] = m_lockfree.search(inp);
		if (right.value() == inp) { return std::nullopt; }
		auto *new_node = new Node{inp, &right};
		Commit cdesc{CasDescriptor(left.next_atomic(), &right, new_node)};
		return std::make_optional<Commit>(cdesc);
	}

	auto wrap_up (const nonstd::expected<std::monostate, std::optional<int>> &executed, const Commit &desc, tsim::ContentionFailureCounter &failures)
	-> nonstd::expected<std::optional<Output>, std::monostate> {
		if (desc.empty()) {
			return std::make_optional(false);
		}
		if (executed.has_value()) {
			return std::make_optional(true);
		}
		(void) failures;
		return nonstd::make_unexpected(std::monostate{});
	}

	/// \brief Client implementation for the fast-path algorithm
	auto fast_path (const Input &inp, tsim::ContentionFailureCounter &failures) -> std::optional<Output> {
		auto[left, right] = m_lockfree.search(inp);
		if (&right != m_lockfree.tail() && right.value() == inp) {
			return std::make_optional(false);   //< Already present
		}
		auto *new_node = new Node{inp, &right};
		auto _left_next_version = left.next_atomic().transform([] (auto a, auto version, auto meta) { return version; });
		if (left.next_atomic().compare_exchange_strong(&right, _left_next_version, new_node, right.meta(), failures)) {
			m_lockfree.m_size.fetch_add(1);
			return std::make_optional(true);
		}

		(void) failures;
		return std::nullopt;
	}

   private:
	LinkedList<T> &m_lockfree;
  };
  static_assert(tsim::NormalizedRepresentation<NormalizedInsert>, "Insert is not normalized.");

  class NormalizedRemove {
   public:
	class CasDescriptor {
	 public:
	  CasDescriptor (tsim::versioning::VersionedAtomic<Node *, MarkMeta> &t_target, Node *t_expected, Node *t_desired)
		  : m_target{t_target},
		    m_expected{t_expected},
		    m_desired{t_desired} {}

	  CasDescriptor (const CasDescriptor &rhs)
		  : m_target{rhs.m_target},
		    m_expected{rhs.m_expected},
		    m_desired{rhs.m_desired},
		    m_state{rhs.m_state.load()} {}

	 public:
	  [[nodiscard]] auto has_modified_bit () const noexcept -> bool {
		  return m_target.has_modified_bit();
	  }

	  auto clear_bit () noexcept {
		  return m_target.clear_modified_bit();
	  }

	  [[nodiscard]] auto state () noexcept -> tsim::CasStatus { return m_state.load(); }

	  auto set_state (tsim::CasStatus t_state) noexcept {
		  m_state.store(t_state);
	  }

	  [[nodiscard]] auto swap_state (tsim::CasStatus expected, tsim::CasStatus desired) noexcept -> bool {
		  return m_state.compare_exchange_strong(expected, desired);
	  }

	  [[nodiscard]] auto execute (tsim::ContentionFailureCounter &failures) noexcept -> nonstd::expected<bool, std::monostate> {
		  return m_target.template compare_exchange_strong(m_expected, m_target.version(), m_desired, m_desired->meta(), failures);
	  }

	 private:
	  std::atomic<tsim::CasStatus> m_state{tsim::CasStatus::Pending};
	  tsim::versioning::VersionedAtomic<Node *, MarkMeta> &m_target;
	  Node *m_expected;
	  Node *m_desired;
	};
	static_assert(tsim::CasWithVersioning<CasDescriptor>, "Commit type has to implement versioning.");
	static_assert(std::is_copy_constructible_v<CasDescriptor>, "Commit type has to be copy-constructible.");

   public:
	using Input = T;
	using Output = bool;
	using Commit = std::array<CasDescriptor, 1>;

	explicit NormalizedRemove (LinkedList<T> &t_lf) : m_lockfree{t_lf} {}
   public:

	auto generator (const Input &inp, tsim::ContentionFailureCounter &failures) -> std::optional<Commit> {
		auto[left, right] = m_lockfree.search(inp);
		if (right.value() != inp) { return std::nullopt; }
		auto *updated_node = new Node{inp, &right};
		updated_node->mark();
		auto &left_next = left.next_atomic();
		auto commit_ = Commit{CasDescriptor{left_next, &right, updated_node}};
		return std::make_optional<Commit>(commit_);
	}

	auto wrap_up (const nonstd::expected<std::monostate, std::optional<int>> &executed,
	              const Commit &desc,
	              tsim::ContentionFailureCounter &failures) -> nonstd::expected<std::optional<Output>, std::monostate> {
		if (desc.empty()) {
			return std::make_optional(false);
		}
		if (executed.has_value()) {
			return std::make_optional(true);
		}
		return nonstd::make_unexpected(std::monostate{});
	}

	auto fast_path (const Input &inp, tsim::ContentionFailureCounter &failures) -> std::optional<Output> {
		auto[left, right] = m_lockfree.search(inp);
		if (&right == m_lockfree.tail() || right.value() != inp) {
			return std::make_optional(false);
		}
		if (is_removed(&right)) {
			// Already logically removed
			return std::make_optional(false);
		}
		auto *updated_node = new Node{inp, right.next()};
		if (!updated_node->mark()) { return false; }
		auto left_next_version = left.next_atomic().transform([] (auto _v, auto version, auto _m) { return version; });
		auto left_next_meta = left.next_atomic().transform([] (auto _v, auto _ve, auto meta) { return meta; });
		if (!left.next_atomic().compare_exchange_strong(&right, left_next_version, updated_node, left_next_meta, failures)) {
			return std::make_optional(false);
		}

		m_lockfree.m_size.fetch_sub(1);
		return std::make_optional(true);
	}

   private:
	LinkedList<T> &m_lockfree;
  };
  static_assert(tsim::NormalizedRepresentation<NormalizedRemove>, "Remove is not normalized.");

  friend NormalizedInsert;
  friend NormalizedRemove;
};

}// namespace linkedlist

#endif// NORMALIZED_HARRIS_LINKED_LIST_TELAMON_CLIENT_H
