#ifndef HARRIS_LINKED_LIST_TELAMON_CLIENT_H
#define HARRIS_LINKED_LIST_TELAMON_CLIENT_H

#include <atomic>
#include <utility>
#include <optional>
#include <concepts>
#include <ranges>
#include <limits>
#include <array>

#include <nonstd/expected.hpp>

namespace harrislinkedlist {

/// \brief 		Implementation of Harris' Linked list
/// \details 	This is the original paper https://www.microsoft.com/en-us/research/wp-content/uploads/2001/10/2001-disc.pdf
/// \tparam 	T Has to be either an integral type or a floating type
template<typename T> requires std::integral<T> || std::floating_point<T>
class LinkedList {
 public:
  class Node {
   public:
	explicit Node (const T &value, bool marked = false, Node *next = nullptr) : m_value{value}, m_mark{marked}, m_next{next} {}
	Node (const Node &rhs) : m_value{rhs.m_value}, m_next{rhs.m_next.load()}, m_mark{rhs.m_mark.load()} {}

   public:
	[[nodiscard]] bool is_removed () const noexcept { return m_mark.load(); }
	[[nodiscard]] auto value () const noexcept -> T { return m_value; }
	[[nodiscard]]auto next_atomic () noexcept -> std::atomic<Node *> & { return m_next; }
	[[nodiscard]] auto next () const noexcept -> Node * { return m_next.load(); }
	void mark (bool t_mark = true) noexcept { return m_mark.store(t_mark); } // TODO: CasDescriptor?
	void set_next (Node *t_next) noexcept { m_next.store(t_next); }

   private:
	T m_value;
	std::atomic<Node *> m_next;
	std::atomic<bool> m_mark;//< Marks whether the node has been \e logically deleted
  };

 public:
  LinkedList ()     // TODO: Hazptr
	  : m_head{new Node{std::numeric_limits<T>::min()}},
	    m_tail{new Node{std::numeric_limits<T>::max()}},
	    m_size{0} {
	  head()->set_next(tail());
  }

 public:
  /// \brief Insert an element into the linked list
  /// \param value	The value to be inserted
  auto insert (T value) -> bool {
	  auto *new_node = new Node{value};
	  while (true) {
		  auto[left, right] = search(value);
		  auto right_ptr = &right;
		  if (right_ptr != tail() && right.value() == value) {
			  if (right.is_removed()) {
				  right.mark(false);
				  break;
			  }
			  return false;   //< Already present
		  }
		  new_node->set_next(right_ptr);
		  std::atomic<Node *> &left_next_atom = left.next_atomic();
		  if (left_next_atom.compare_exchange_strong(right_ptr, new_node)) {
			  m_size.fetch_add(1);
			  break;
		  }
	  }

	  return true;
  }

  /// \brief Check whether an element appears in the linked list
  /// \param desired The value that is looked for
  auto appears (T desired) -> bool {
	  const auto *tail_ = tail();
	  for (auto *it = head()->next(); it != tail_; it = it->next()) {
		  if (is_removed(it)) { continue; }
		  auto actual = it->value();
		  if (actual > desired) { break; }
		  if (actual == desired) { return true; }
	  }
	  return false;
  }

  /// \brief Remove an element from the linked list
  /// \param value The value to be removed
  auto remove (T value) -> bool {
	  while (true) {
		  auto[left, right] = search(value);
		  if (&right == tail() || right.value() != value) {
			  return false;
		  }
		  Node *right_ptr = &right;
		  if (is_removed(right_ptr)) {
			  // Already logically removed
			  break;
		  }
		  auto *updated_right_ptr = new Node{right_ptr->value(), true, right_ptr->next()};

		  if (left.next_atomic().compare_exchange_strong(right_ptr, updated_right_ptr)) {
			  // Successful CAS
			  break;
		  }
	  }

	  m_size.fetch_sub(1);
	  return true;
  }

  auto search (T value) -> std::pair<Node &, Node &> {
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
//		  if (left_ptr->next_atomic().compare_exchange_strong(left_next, right_ptr)) {
//			  if (right_ptr != m_tail && right_ptr->next()->is_removed()) continue;
//			  return std::pair<Node &, Node &>{*left_ptr, *right_ptr};
//		  }
	  }
  }

 public:
  [[nodiscard]] auto tail () const noexcept -> Node * { return m_tail.load(); }
  [[nodiscard]] auto head () const noexcept -> Node * { return m_head.load(); }
  [[nodiscard]] auto size () const noexcept -> std::size_t { return m_size.load(); }

  static bool is_removed (Node *const node) noexcept {
	  if (!node) return false;
	  return node->is_removed();
  }

 private:
  std::atomic<Node *> m_head;
  std::atomic<Node *> m_tail;
  std::atomic<std::size_t> m_size;
};

}// namespace linkedlist

#endif// HARRIS_LINKED_LIST_TELAMON_CLIENT_H
