/*
 * @file HelpQueue.hh
 * @brief Provides an implementation of a wait-free queue with support for
 * limited operations
 */
#ifndef TELAMON_HELP_QUEUE_HH
#define TELAMON_HELP_QUEUE_HH

#include <atomic>
#include <memory>
#include <ranges>
#include <optional>
#include <algorithm>

namespace helpqueue {

template<typename T, const int N = 16>
class HelpQueue {
 public:
  struct Node;
  struct OperationDescription;

 public:
  constexpr HelpQueue() {
    m_head.store(Node::SENTITEL_NODE.get());
    m_tail.store(Node::SENTITEL_NODE.get());
    std::ranges::for_each(m_states, [](auto &state) { state.store(OperationDescription::EMPTY.get()); });
  }

 public:
  ///
  /// @brief Enqueue an element to the tail of the queue
  /// @param element The element to be enqueued
  /// @param enqueuer The id of the thread which enqueues the element
  void enqueue(T element, const int enqueuer) {}

  ///
  /// @brief Peek the head of the queue
  /// @return The value of the head if one is present and empty if not
  std::optional<T> peek() {
    auto head = m_head.load();
    if (!head->next()) {
      return {};
    }
    return {head->data()};
  }

  ///
  /// @brief Dequeues iff the given value is the same as the value at the head
  /// of the queue
  /// @param expected_head The value which the head is expected to be
  /// @return Whether the dequeue succeeded or not
  bool conditionalDequeue(T expected_head) { return false; }

 private:
  std::atomic<Node *> m_head;
  std::atomic<Node *> m_tail;
  std::array<std::atomic<OperationDescription *>, N> m_states;
};

///
/// @brief The class which represents a node element of the queue
///
template<typename T, const int N>
struct HelpQueue<T, N>::Node {
 public:
  ///
  /// Default construction of sentitel node
  constexpr Node()
      : m_is_sentitel{true}, m_enqueuer_id{-1 /* unused */} {}

  ///
  /// Construction of node with copyable data
  constexpr Node(const T &data, int enqueuer)
      : m_data{data}, m_enqueuer_id{enqueuer} {}

  ///
  /// Construction of node with only movable data
  constexpr Node(T &&data, int enqueuer)
      : m_data{data}, m_enqueuer_id{enqueuer} {}

  bool operator==(const Node &rhs) const {
    return std::tie(m_is_sentitel, m_data, m_next, m_enqueuer_id)
        ==std::tie(rhs.m_is_sentitel, rhs.m_data, rhs.m_next, rhs.m_enqueuer_id);
  }
  bool operator!=(const Node &rhs) const {
    return !(rhs==*this);
  }

 public:
  [[nodiscard]] bool is_sentitel() const {
    return is_sentitel;
  }

  [[nodiscard]] T data() const {
    return m_data;
  }

  [[nodiscard]] const Node *next() const {
    return m_next;
  }

  [[nodiscard]] int enqueuer_id() const {
    return m_enqueuer_id;
  }

  const inline static auto SENTITEL_NODE = std::make_unique<Node>();

 private:
  const bool m_is_sentitel = false;
  T m_data;
  std::atomic<Node *> m_next;
  const int m_enqueuer_id;
};

///
/// Operation description for the queue used when the queue itself needs
/// "helping"
///
template<typename T, const int N>
struct HelpQueue<T, N>::OperationDescription {
 public:
  ///
  /// Empty construction
  constexpr OperationDescription()
      : m_is_empty{true} {}

  ///
  /// Default construction
  constexpr OperationDescription(bool pending, bool enqueue, Node *node)
      : m_pending{pending},
        m_enqueue{enqueue},
        m_node{node} {}

 public:
  [[nodiscard]] bool is_empty() const {
    return m_is_empty;
  }
  [[nodiscard]] bool pending() const {
    return m_pending;
  }
  [[nodiscard]] bool enqueue() const {
    return m_enqueue;
  }
  const Node *node() const {
    return m_node;
  }

  const inline static auto EMPTY = std::make_unique<OperationDescription>();

 private:
  bool m_is_empty = false;
  bool m_pending{};
  bool m_enqueue{};
  std::atomic<Node *> m_node;
};

}

#endif    // TELAMON_HELP_QUEUE_HH
