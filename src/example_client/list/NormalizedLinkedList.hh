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

	void mark (bool t_mark = true) noexcept {
		// TODO: RCU??
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
		  for (auto marked = is_removed(next);
		       marked || current->value() < value;
		       next = current->next()) {
			  if (!marked) {
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

		  /// 3. Remove marked nodes
		  if (left_ptr->next_atomic().compare_exchange_strong(left_next, left_ptr->version(), right_ptr, left_ptr->meta(), failures)) {
			  if (right_ptr != m_tail && right_ptr->next()->is_removed()) continue;
			  return std::pair<Node &, Node &>{*left_ptr, *right_ptr};
		  }
	  }
  }

 public:
  [[nodiscard]] auto tail () const noexcept -> Node * { return m_tail; }
  [[nodiscard]] auto head () const noexcept -> Node * { return m_head; }
  [[nodiscard]] auto size () const noexcept -> std::size_t { return m_size.load(); }

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
		  return m_target.load()->modification_bit.load();
	  }

	  auto clear_bit () const noexcept {
		  if (state() != tsim::CasStatus::Success) {
			  return; //< Error?
		  }
		  auto expected = false;
		  auto _ = m_target.load()->modification_bit.compare_exchange_strong(expected, true);
	  }

	  [[nodiscard]] auto state () const noexcept -> tsim::CasStatus { return m_state.load(); }

	  auto set_state (tsim::CasStatus new_state) noexcept { m_state.store(new_state); }

	  [[nodiscard]] auto swap_state (tsim::CasStatus expected, tsim::CasStatus desired) noexcept -> bool {
		  return m_state.compare_exchange_strong(expected, desired);
	  }

	  [[nodiscard]] auto execute (tsim::ContentionFailureCounter &failures) noexcept -> nonstd::expected<bool, std::monostate> {
		  return m_target.compare_exchange_strong(m_expected, m_expected->version(), m_desired, m_expected->meta(), failures);
		  // return nonstd::make_unexpected(std::monostate{}); //< Delete?
	  }

	 private:
	  std::atomic<tsim::CasStatus> m_state{tsim::CasStatus::Pending};
	  tsim::versioning::VersionedAtomic<Node *, MarkMeta> m_target;
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
		auto *new_node = new Node{right};
		new_node->mark();
		Commit cdesc{CasDescriptor(left.next_atomic(), &right, new_node)};
		return std::make_optional<Commit>(cdesc);
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
		if (&right != m_lockfree.tail() && right.value() == inp) {
			return std::make_optional(false);   //< Already present
		}
		auto *new_node = new Node{right};
		new_node->mark();
		if (left.next_atomic().compare_exchange_strong(&right, right.version(), new_node, right.meta(), failures)) {
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
	  CasDescriptor (tsim::versioning::VersionedAtomic<Node *> &t_target, Node *t_expected, Node *t_desired)
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
		  return m_target.load()->modification_bit.load();
	  }

	  auto clear_bit () const noexcept {
		  bool expected = false;
		  m_target.load()->modification_bit.compare_exchange_strong(expected, true);
	  }

	  [[nodiscard]] auto state () noexcept -> tsim::CasStatus { return m_state.load(); }

	  auto set_state (tsim::CasStatus t_state) noexcept {
		  m_state.store(t_state);
	  }

	  [[nodiscard]] auto swap_state (tsim::CasStatus expected, tsim::CasStatus desired) noexcept -> bool {
		  return m_state.compare_exchange_strong(expected, desired);
	  }

	  [[nodiscard]] auto execute (tsim::ContentionFailureCounter &failures) noexcept -> nonstd::expected<bool, std::monostate> {
		  return m_target.template compare_exchange_strong(m_expected, m_expected->version(), m_desired, m_expected->meta(), failures);
	  }

	 private:
	  std::atomic<tsim::CasStatus> m_state{tsim::CasStatus::Pending};
	  tsim::versioning::VersionedAtomic<Node *, MarkMeta> m_target;
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
		auto &[left, right] = m_lockfree.search(inp);
		if (right.value() == inp) { return std::nullopt; }
		auto *update_node = new tsim::versioning::VersionedAtomic<Node, MarkMeta>{MarkMeta{true}, inp, &right};
		Commit cdesc{CasDescriptor(left->next_atomic(), &right, update_node)};
		return std::make_optional<Commit>(cdesc);
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
		Node *right_ptr = &right;
		if (is_removed(right_ptr)) {
			// Already logically removed
			return std::make_optional(false);
		}
		auto *updated_right_ptr = new Node{right_ptr->value(), true, right_ptr->next()};

		if (!left.next_atomic().compare_exchange_strong(right_ptr, right_ptr->version(), updated_right_ptr, right_ptr->meta(), failures)) {
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
