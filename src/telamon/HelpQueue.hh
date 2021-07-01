/*
 * \file HelpQueue.hh
 * \brief Provides an implementation of a wait-free queue with support for
 * limited operations
 */
#ifndef TELAMON_HELP_QUEUE_HH
#define TELAMON_HELP_QUEUE_HH

#include <algorithm>
#include <atomic>
#include <memory>
#include <numeric>
#include <optional>
//#include <ranges> // TODO: clang-12 does not compile this properly

namespace helpqueue {

template <typename T, const int N = 16>
class HelpQueue {
 public:
  struct Node;
  struct OperationDescription;
  enum class Operation : int { enqueue };

 public:
  constexpr HelpQueue () {
    m_head.store (Node::SENTITEL_NODE.get ());
    m_tail.store (Node::SENTITEL_NODE.get ());
    std::for_each (m_states.begin (), m_states.end (), [] (auto &state) {
      state.store (OperationDescription::EMPTY.get ());
    });

    // TODO: Clang does not compile the ranges header
    // std::ranges::for_each (m_states, [] (auto &state) {
    //  state.store (OperationDescription::EMPTY.get ());
    // });
  }

 public:
  ///
  /// \brief Enqueue an element to the tail of the queue
  /// \param element The element to be enqueued
  /// \param enqueuer The id of the thread which enqueues the element
  void enqueue (T element, const int enqueuer) {
    int phase = max_phase ().value_or (0) + 1;
    // TODO: Change `new` when hazard pointers are used
    auto *node = new Node{element, enqueuer};
    auto *description = new OperationDescription{phase, true, Operation::enqueue, node};
    m_states.at (enqueuer).store (description);
    help_others (phase);
    help_finish <Operation::enqueue> ();
  }

  ///
  /// \brief Peek the head of the queue
  /// \return The value of the head if one is present and empty if not
  std::optional <T> peek () const {
    auto head = m_head.load ();
    if (head->is_sentitel () || !head->next ()) {
      return {};
    }
    return {head->data ()};
  }

  ///
  /// \brief Dequeues iff the given value is the same as the value at the head
  /// of the queue
  /// \param expected_head The value which the head is expected to be
  /// \return Whether the dequeue succeeded or not
  bool conditionalDequeue (T _expected_head) { return false; }

 private:  //< Helper functions

  bool is_pending (int state_id, int phase_limit) {
    auto state_ptr = m_states.at (state_id).load ();
    return state_ptr->pending () && state_ptr->phase () <= phase_limit;
  }

  template <Operation operation>
  void help_finish ();

  ///
  /// \brief Performs the finishing touches of the enqueue operation
  /// \details This is the routine which actually modifies the structure and values of any pointers. It fetches the
  /// values and performs simple checks to be sure that no other thread has already performed the updates. Following
  /// that, the operation state gets updated and its value as well as the tail pointer are CAS-ed with the new values.
  template <>
  void help_finish <Operation::enqueue> () {
    auto tail_ptr = m_tail.load ();
    auto next_ptr = tail_ptr->next ().load ();
    if (!next_ptr) {
      // Tail pointer is correctly put.
      return;
    }

    // Id's value is valid since next cannot be Node::SENTINEL
    auto id = next_ptr->enqueuer_id ();
    auto /* std::atomic<OperationDescription*> */ old_state_ptr = m_states.at (id).load ();

    if (tail_ptr != m_tail.load ()) {
      // Tail pointer has just been updated.
      return;
    }

    if (old_state_ptr->node () != next_ptr) {
      // The thread which started this operation has already changed the node it is working on, thus this operation has
      // already finished.
      return;
    }

    // TODO: Change `new` when proper memory reclamation scheme is added (hazard pointers).
    auto updated_state_ptr = new OperationDescription{
        old_state_ptr->phase (),
        false,
        Operation::enqueue,
        old_state_ptr->node ()
    };

    // Update
    m_states.at (id).compare_exchange_strong (old_state_ptr, updated_state_ptr);
    m_tail.compare_exchange_strong (tail_ptr, next_ptr);

  }

  template <Operation operation>
  void help (int state_idx, int helper_phase);

  ///
  /// \brief Help another thread perform a certain operation on the help queue
  /// \param state_idx The index in the state array corresponding to the operation
  /// \param helper_phase The phase of the helper
  /// \details The first thing that the function does is that it acquires the pair of (tail, tail.next) pointers and
  /// performs checks on them. This is done in order to observe whether modifications (from another thread) have been
  /// made during this function's execution. After all of them have passed and it is sure that the pair is consistent,
  /// the helping thread tries to update the operation descriptor and then CAS the new value by finishing the enqueue operation.
  template <>
  void help <Operation::enqueue> (int state_idx, int helper_phase) {
    while (is_pending (state_idx, helper_phase)) {
      auto tail_ptr = m_tail.load ();
      auto next_ptr = tail_ptr->next_mut ().load();

      if (tail_ptr != m_tail.load ()) {
        continue;
      }

      if (next_ptr != nullptr) {
        // Outdated tail
        help_finish <Operation::enqueue> ();
        continue;
      }

      if (!is_pending (state_idx, helper_phase)) {
        // The operation has already been executed.
        return;
      }

      auto state_ptr = m_states.at (state_idx).load ();
      if (!state_ptr->pending ()) {
        return;
      }

      auto &tail = *tail_ptr;
      auto *node_ptr = state_ptr->node ().load();
      if (tail.next_mut ().compare_exchange_strong (next_ptr, node_ptr)) {
        // Successful modification by CAS
        return help_finish <Operation::enqueue> ();
      }
    }
  }

  void help_others (int helper_phase) {
    int i = 0;
    for (auto &atomic_state : m_states) {
	  auto state = atomic_state.load();
      if (state->pending () && state->phase () <= helper_phase) {
		  switch (state->operation()) {
			  case Operation::enqueue:
				help <Operation::enqueue> (i, helper_phase);
				break;
		  }
      }
      ++i;
    }
  }

  [[nodiscard]] std::optional <int> max_phase () const {
    auto it = std::max_element (m_states.begin (),
                                m_states.end (),
                                [] (const auto &state1, const auto &state2) {
                                  return state1.load ()->phase () < state2.load ()->phase ();
                                });
    if (it != m_states.end ()) {
      return it->load ()->phase ();
    }
    return {};
  }

 private:
  std::atomic <Node *> m_head;
  std::atomic <Node *> m_tail;
  std::array <std::atomic <OperationDescription *>, N> m_states;
};

///
/// \brief The class which represents a node element of the queue
///
template <typename T, const int N>
struct HelpQueue <T, N>::Node {
 public:
  ///
  /// Default construction of sentitel node
  constexpr Node () : m_is_sentitel{true}, m_enqueuer_id{-1 /* unused */} {}

  ///
  /// Construction of node with copyable data
  constexpr Node (const T &data, int enqueuer)
      : m_data{data}, m_enqueuer_id{enqueuer} {}

  ///
  /// Construction of node with only movable data
  constexpr Node (T &&data, int enqueuer)
      : m_data{data}, m_enqueuer_id{enqueuer} {}

  bool operator== (const Node &rhs) const {
    return std::tie (m_is_sentitel, m_data, m_next, m_enqueuer_id) ==
        std::tie (rhs.m_is_sentitel, rhs.m_data, rhs.m_next,
                  rhs.m_enqueuer_id);
  }
  bool operator!= (const Node &rhs) const { return !(rhs == *this); }

 public:
  [[nodiscard]] bool is_sentitel () const { return m_is_sentitel; }

  [[nodiscard]] T data () const { return m_data; }

  [[nodiscard]] std::atomic <Node *> &next_mut () { return m_next; }

  [[nodiscard]] const std::atomic <Node *> &next () const { return m_next; }

  [[nodiscard]] int enqueuer_id () const { return m_enqueuer_id; }

  const inline static auto SENTITEL_NODE = std::make_unique <Node> ();

 private:
  const bool m_is_sentitel = false;
  T m_data;
  std::atomic <Node *> m_next;
  const int m_enqueuer_id;
};

///
/// Operation description for the queue used when the queue itself needs
/// "helping"
///
template <typename T, const int N>
struct HelpQueue <T, N>::OperationDescription {
 public:
  ///
  /// Empty construction
  constexpr OperationDescription () : m_is_empty{true} {}

  ///
  /// Default construction
  constexpr OperationDescription (int phase, bool pending, Operation operation, Node *node)
      : m_phase{phase},
        m_pending{pending},
        m_operation{operation},
        m_node{node} {}

 public:
  [[nodiscard]] bool is_empty () const { return m_is_empty; }
  [[nodiscard]] bool pending () const { return m_pending; }
  [[nodiscard]] Operation operation () const { return m_operation; }
  [[nodiscard]] std::atomic <Node *> &node () { return m_node; }
  [[nodiscard]] int phase () const { return m_phase; }

  const inline static auto EMPTY = std::make_unique <OperationDescription> ();

 private:
  bool m_is_empty = false;
  bool m_pending{};
  Operation m_operation{};
  std::atomic <Node *> m_node;
  int m_phase;
};

}  // namespace helpqueue

#endif    // TELAMON_HELP_QUEUE_HH
