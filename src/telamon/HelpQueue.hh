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
#include <thread>
#include <typeinfo>
#include <optional>
#include <ranges>

#ifdef TEL_LOGGING
#include <extern/loguru/loguru.hpp>

thread_local std::thread::id current_thread_id = std::this_thread::get_id();
#endif

/// \brief This module contains the implementation of a wait-free queue used as an underlying structure in the simulation - "help queue"
namespace helpqueue {

/// \brief This is the main class representing the help queue
template<typename T, const int N = 16>
class HelpQueue {
 public:
  struct Node;
  struct OperationDescription;
  enum class Operation : int { enqueue };

 public:
  constexpr HelpQueue () {
#ifdef TEL_LOGGING
  	  loguru::add_file("helpqueue.log", loguru::Append, loguru::Verbosity_MAX);
#endif

	  m_head.store(Node::SENTITEL_NODE.get());
	  m_tail.store(Node::SENTITEL_NODE.get());

	  std::ranges::for_each(m_states, [] (auto &state) {
		state.store(OperationDescription::EMPTY.get());
	  });
  }

 public:
  ///
  /// \brief Enqueue an element to the tail of the queue
  /// \param element The element to be enqueued
  /// \param enqueuer The id of the thread which enqueues the element
  void push_back (const int enqueuer, T element) {
#ifdef TEL_LOGGING
	  LOG_S(INFO)
	  << "Thread '" << current_thread_id << "': push_back with value " << element << " type T = [" << typeid(T).name()
	  << "]\n";
#endif
	  int phase = max_phase().value_or(-1) + 1;
#ifdef TEL_LOGGING
	  LOG_S(INFO) << "Thread '" << current_thread_id << "': Calculated phase = " << phase << '\n';
#endif
	  // TODO: Change `new` when hazard pointers are used
	  auto *node = new Node{element, enqueuer};
	  auto *description = new OperationDescription{phase, true, Operation::enqueue, node};
	  m_states.at(enqueuer).store(description);

#ifdef TEL_LOGGING
	  LOG_S(INFO) << "Thread '" << current_thread_id
				  << "': State updated with initial OperationDescription for push_back operation.\n";
#endif

	  help_others(phase);
	  help_finish_enqueue();
  }

  ///
  /// \brief Peek the head of the queue
  /// \return The value of the head if one is present and empty if not
  std::optional<T> peek_front () const {
#ifdef TEL_LOGGING
	  LOG_S(INFO) << "Thread '" << current_thread_id << "': Peeking the front of the help queue." << '\n';
#endif
	  auto next = m_head.load()->next().load();

	  if (!next) {
#ifdef TEL_LOGGING
		  LOG_S(INFO) << "Thread '" << current_thread_id << "': The help queue is empty." << '\n';
#endif
		  return std::nullopt;
	  }
#ifdef TEL_LOGGING
	  LOG_S(INFO) << "Thread '" << current_thread_id << "': The help queue's head points to data = " << next->data() << '\n';
#endif
	  return {next->data()};
  }

  ///
  /// \brief Dequeues iff the given value is the same as the value at the head
  /// of the queue
  /// \param expected_head The value which the head is expected to be
  /// \return Whether the dequeue succeeded or not
  bool try_pop_front (T expected_head) {
#ifdef TEL_LOGGING
	  LOG_S(INFO) << "Thread '" << current_thread_id << "': Trying to pop the front of the help queue (expecting data = " << expected_head << ")\n";
#endif
	  auto head_ptr = m_head.load();
	  auto next_ptr = head_ptr->next().load();
	  if (!next_ptr || next_ptr->data() != expected_head) {

#ifdef TEL_LOGGING
		  LOG_S(INFO) << "Thread '" << current_thread_id << "': The help queue is empty." << '\n';
#endif
		  return false;
	  }

	  if (m_head.compare_exchange_strong(head_ptr, next_ptr)) {
		  help_finish_enqueue();
		  head_ptr->set_next(nullptr);
#ifdef TEL_LOGGING
		  LOG_S(INFO) << "Thread '" << current_thread_id << "': CAS during try_pop_front was successful" << '\n';
#endif
		  return true;
	  }
#ifdef TEL_LOGGING
	  LOG_S(INFO) << "Thread '" << current_thread_id << "': CAS during try_pop_front FAILED" << '\n';
#endif
	  return false;
  }

 private:  //< Helper functions

  bool is_pending (int state_id, int phase_limit) {
	  auto state_ptr = m_states.at(state_id).load();
	  return state_ptr->pending() && state_ptr->phase() <= phase_limit;
  }

  ///
  /// \brief Performs the finishing touches of the push_back operation
  /// \details This is the routine which actually modifies the structure and values of any pointers. It fetches the
  /// values and performs simple checks to be sure that no other thread has already performed the updates. Following
  /// that, the operation state gets updated and its value as well as the tail pointer are CAS-ed with the new values.
  void help_finish_enqueue () {
#ifdef TEL_LOGGING
	  LOG_S(INFO) << "Thread '" << current_thread_id << "' in helping to finish push_back operation.\n";
#endif
	  auto tail_ptr = m_tail.load();
	  auto next_ptr = tail_ptr->next().load();
	  if (!next_ptr) {
#ifdef TEL_LOGGING
		  LOG_S(INFO) << "Thread '" << current_thread_id << "': Tail pointer correctly put.\n";
#endif
		  return;
	  }

	  // Id's value is valid since next cannot be Node::SENTINEL
	  auto id = next_ptr->enqueuer_id();
	  auto /* std::atomic<OperationDescription*> */ old_state_ptr = m_states.at(id).load();

	  if (tail_ptr != m_tail.load()) {
#ifdef TEL_LOGGING
		  LOG_S(INFO) << "Thread '" << current_thread_id << "': Tail pointer updated.\n";
#endif
		  return;
	  }

	  if (old_state_ptr->node() != next_ptr) {
#ifdef TEL_LOGGING
		  LOG_S(INFO) << "Thread '" << current_thread_id << "': The thread which started this operation has already changed the node it is working "
															"on, thus this operation has already finished.\n";
#endif
		  return;
	  }

	  // TODO: Change `new` when proper memory reclamation scheme is added (hazard pointers).
	  auto updated_state_ptr = new OperationDescription{
		  old_state_ptr->phase(),
		  false,
		  Operation::enqueue,
		  old_state_ptr->node()
	  };

	  // Update
#ifdef TEL_LOGGING
	  LOG_S(INFO) << "Thread '" << current_thread_id << "': Performing CAS-es on the state and on the tail.\n";
#endif
	  (void) m_states.at(id).compare_exchange_weak(old_state_ptr, updated_state_ptr);
	  (void) m_tail.compare_exchange_strong(tail_ptr, next_ptr);
  }

  /// \brief Help another thread perform a certain operation on the help queue
  /// \param state_idx The index in the state array corresponding to the operation
  /// \param helper_phase The phase of the helper
  /// \details The first thing that the function does is that it acquires the pair of (tail, tail.next) pointers and
  /// performs checks on them. This is done in order to observe whether modifications (from another thread) have been
  /// made during this function's execution. After all of them have passed and it is sure that the pair is consistent,
  /// the helping thread tries to update the operation descriptor and then CAS the new value by finishing the push_back operation.
  void help_enqueue (int state_idx, int helper_phase) {
#ifdef TEL_LOGGING
	  LOG_S(INFO) << "Thread '" << current_thread_id << "': Starting to help Thread '" << state_idx << "' with by having phase = " << helper_phase
	  << '\n';
#endif
	  while (is_pending(state_idx, helper_phase)) {

		  auto *tail_ptr = m_tail.load();
		  auto &tail = *tail_ptr;
		  auto *next_ptr = tail_ptr->next().load();

		  if (tail_ptr != m_tail.load()) {
#ifdef TEL_LOGGING
			  LOG_S(INFO) << "Thread '" << current_thread_id << "': Tail pointer modified. Retrying ...\n";
#endif
			  continue;
		  }

		  if (next_ptr != nullptr) {
#ifdef TEL_LOGGING
			  LOG_S(INFO) << "Thread '" << current_thread_id << "': Tail pointer outdated. Retrying ...\n";
#endif
			  help_finish_enqueue();
			  continue;
		  }

		  if (!is_pending(state_idx, helper_phase)) {
#ifdef TEL_LOGGING
			  LOG_S(INFO) << "Thread '" << current_thread_id << "': Operation already executed. Done.\n";
#endif
			  return;
		  }

		  auto *state_ptr = m_states.at(state_idx).load();
		  auto state = *state_ptr;
		  if (!state.pending()) {
#ifdef TEL_LOGGING
			  LOG_S(INFO) << "Thread '" << current_thread_id << "': Operation already executed. Done.\n";
#endif
			  return;
		  }

		  auto *new_next_ptr = state_ptr->node();
		  if (tail.next().compare_exchange_strong(next_ptr, new_next_ptr)) {
#ifdef TEL_LOGGING
			  LOG_S(INFO) << "Thread '" << current_thread_id << "' in helping Thread '" << state_idx
						  << "': CAS on tail, next and node.\n";
#endif
			  return help_finish_enqueue();
		  }
	  }
  }

  void help_others (int helper_phase) {
#ifdef TEL_LOGGING
	  LOG_S(INFO)
	  << "Thread '" << current_thread_id << "': Helping others with helper phase = " << helper_phase << '\n';
#endif
	  int i = 0;
	  for (auto &atomic_state : m_states) {
		  auto state = atomic_state.load();
		  if (state->pending() && state->phase() <= helper_phase) {
			  if (state->operation() == Operation::enqueue) {
#ifdef TEL_LOGGING
				  LOG_S(INFO)
				  << "Thread '" << current_thread_id << "': Found operation which needs help - Thread '" << i
				  << "' which is performing push_back with phase = " << helper_phase << '\n';
#endif
				  help_enqueue(i, helper_phase);
			  }
		  }
		  ++i;
	  }
  }

  [[nodiscard]] std::optional<int> max_phase () const {
	  auto it = std::max_element(m_states.begin(), m_states.end(), [] (const auto &state1, const auto &state2) {
		return state1.load()->phase() < state2.load()->phase();
	  });
	  if (it != m_states.end()) {
		  return it->load()->phase();
	  }
	  return {};
  }

 private:
  std::atomic<Node *> m_head;
  std::atomic<Node *> m_tail;
  std::array<std::atomic<OperationDescription *>, N> m_states;
};

///
/// \brief The class which represents a node element of the queue
///
template<typename T, const int N>
struct HelpQueue<T, N>::Node {
 public:
  /// Default construction of sentitel node
  Node () : m_is_sentitel{true} {}

  /// Construction of a value in node
  template<typename ...Args>
  explicit Node (int enqueuer, Args &&... args) : m_enqueuer_id{enqueuer}, m_data{std::forward<Args>(args)...} {}

  /// Construction of node with copyable data
  Node (T data, int enqueuer)
	  : m_data{data}, m_enqueuer_id{enqueuer}, m_next(nullptr) {}

  bool operator== (const Node &rhs) const {
	  return std::tie(m_is_sentitel, m_data, m_next, m_enqueuer_id) ==
		  std::tie(rhs.m_is_sentitel, rhs.m_data, rhs.m_next,
		           rhs.m_enqueuer_id);
  }
  bool operator!= (const Node &rhs) const { return !(rhs == *this); }

 public:
  [[nodiscard]] bool is_sentitel () const { return m_is_sentitel; }

  [[nodiscard]] bool has_data () const { return m_data.has_value(); }

  [[nodiscard]] const T &data () const { return m_data.value(); }

  [[nodiscard]] std::atomic<Node *> &next () { return m_next; }

  void set_next (Node *ptr) { m_next.store(ptr); }

  [[nodiscard]] int enqueuer_id () const { return m_enqueuer_id; }

  const inline static auto SENTITEL_NODE = std::make_unique<Node>();

 private:
  const bool m_is_sentitel = false;
  std::optional<T> m_data{};
  std::atomic<Node *> m_next;
  const int m_enqueuer_id{-1};
};

/// \brief Operation description for the queue used when the queue itself needs "helping"
template<typename T, const int N>
struct HelpQueue<T, N>::OperationDescription {
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
  [[nodiscard]] Node *node () { return m_node; }
  [[nodiscard]] int phase () const { return m_phase; }

  const inline static auto EMPTY = std::make_unique<OperationDescription>();

 private:
  bool m_is_empty = false;
  bool m_pending{};
  Operation m_operation{};
  Node *m_node;
  int m_phase;
};

}  // namespace helpqueue

#endif    // TELAMON_HELP_QUEUE_HH
