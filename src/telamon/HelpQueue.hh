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
  void enqueue (T element, const int enqueuer) {}

  ///
  /// \brief Peek the head of the queue
  /// \return The value of the head if one is present and empty if not
  std::optional <T> peek () {
    auto head = m_head.load ();
    if (!head->next ()) {
      return {};
    }
    return {head->data ()};
  }

  ///
  /// \brief Dequeues iff the given value is the same as the value at the head
  /// of the queue
  /// \param expected_head The value which the head is expected to be
  /// \return Whether the dequeue succeeded or not
  bool conditionalDequeue (T expected_head) { return false; }

 private:  //< Helper functions
  [[nodiscard]] std::optional <int> max_phase () const {
    auto max = std::max_element (m_states.begin (), m_states.empty (),
                                 [] (const auto &state1, const auto &state2) {
                                   return state1.load ()->m_phase <
                                       state2.load ()->m_phase;
                                 });

    if (max != m_states.end ()) {
      return max;
    }
    return {};
  }

  bool is_pending (int state_id, int phase_limit) {
    auto state = m_states.at (state_id).load ();
    return state.pending () && state.phase_limit () <= phase_limit;
  }

  template <Operation operation>
  void help_finish ();

  template <>
  void help_finish <Operation::enqueue> () {
    // TODO:
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
      auto next_ptr = tail_ptr.next ();

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

      auto state = m_states.at (state_idx).load ();
      if (!state.pending ()) {
        return;
      }

      auto tail = *tail_ptr;
      auto node = state.node ();
      if (tail.next ().compare_exchange (next_ptr, node)) {
        // Successful modification by CAS
        return help_finish <Operation::enqueue> ();
      }
    }
  }

  void help_others (int helper_phase) {
    for (auto &[i, state] : m_states) {
      if (state.pending () && state.helper_phase () <= helper_phase) {
        help <state.operation ()> (i, helper_phase);
      }
    }
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
  [[nodiscard]] bool is_sentitel () const { return is_sentitel; }

  [[nodiscard]] T data () const { return m_data; }

  [[nodiscard]] std::atomic <Node *> &next () const { return m_next; }

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
  constexpr OperationDescription (int phase, bool pending, Operation operation,
                                  Node *node)
      : m_phase{phase},
        m_pending{pending},
        m_operation{operation},
        m_node{node} {}

 public:
  [[nodiscard]] bool is_empty () const { return m_is_empty; }
  [[nodiscard]] bool pending () const { return m_pending; }
  [[nodiscard]] Operation operation () const { return m_operation; }
  [[nodiscard]] std::atomic <Node *> &node () const { return m_node; }
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
