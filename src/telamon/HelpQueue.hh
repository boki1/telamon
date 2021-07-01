/*
 * @file HelpQueue.hh
 * @brief Provides an implementation of a wait-free queue with support for
 * limited operations
 */
#ifndef TELAMON_HELP_QUEUE_HH
#define TELAMON_HELP_QUEUE_HH

#include <atomic>
#include <memory>
#include <optional>

template <typename T, const int N = 16>
class HelpQueue {
   public:
    struct Node;
    struct OperationDescription;

   public:
    constexpr HelpQueue() {
	m_head = std::make_shared<Node>(HelpQueue::SENTITEL_NODE);
	m_tail = std::make_shared<Node>(HelpQueue::SENTITEL_NODE);
	m_states.fill(OperationDescription::EMPTY);
    }

   public:
    ///
    /// @brief Enqueue an element to the queue
    /// @param element The element to be enqueued
    /// @param enqueuer The id of the thread which enqueues the element
    void enqueue(T element, const int enqueuer) {}

    ///
    /// @brief Peek the head of the queue
    /// @return The value of the head if one is present and empty if not
    std::optional<T> peek() { return {}; }

    ///
    /// @brief Dequeues iff the given value is the same as the value at the head
    /// of the queue
    /// @param expected_head The value which the head is expected to be
    /// @return Whether the dequeue succeeded or not
    bool conditionalDequeue(T expected_head) { return false; }

   private:
    std::shared_ptr<Node> m_head;
    std::shared_ptr<Node> m_tail;
    std::array<OperationDescription, N> m_states;

   public:
    constexpr inline static auto SENTITEL_NODE = Node{};
};

///
/// @brief The class which represents a node element of the queue
///
template <typename T, const int N>
struct HelpQueue<T, N>::Node {
   public:
    ///
    /// Default construction of sentitel node
    constexpr Node() : is_sentitel{true} {}
    ///
    /// Construction of node with copyable data
    constexpr Node(const T &data, int enqueuer)
	: m_data{data}, m_enqueuer_id{enqueuer} {}
    ///
    /// Construction of node with only moveable data
    constexpr Node(T &&data, int enqueuer)
	: m_data{data}, m_enqueuer_id{enqueuer} {}

   private:
    const bool is_sentitel = false;
    T m_data;
    std::shared_ptr<Node> m_next;
    const int m_enqueuer_id;
};

///
/// Operation description for the queue used when the queue itself needs
/// "helping"
///
template <typename T, const int N>
struct HelpQueue<T, N>::OperationDescription {
   public:
    ///
    /// Empty construction
    constexpr OperationDescription() : m_is_empty{true} {}
    ///
    /// Default construction
    /// TODO: Should `node` be raw or shared_ptr?
    constexpr OperationDescription(bool pending, bool enqueue, Node *node) {
	m_pending = pending;
	m_enqueue = enqueue;
	m_node = std::make_shared(node);
    }

   private:
    bool m_is_empty = false;
    bool m_pending;
    bool m_enqueue;
    std::shared_ptr<Node> m_node;

   public:
    constexpr inline static auto EMPTY = OperationDescription{};
};

#endif	// TELAMON_HELP_QUEUE_HH
