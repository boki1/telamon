#ifndef TELAMON_SRC_TELAMON_VERSIONING_HH_
#define TELAMON_SRC_TELAMON_VERSIONING_HH_

//! \file 		Versioning.hh
//! \brief 		Definitions of ContentionFailureCounter, CasStatus, Cas, CasWithVersioning, VersionedAtomic
//! \details 	VersionedAtomic is used by the user to implement the required functions of CasWithVersioning,
//! 			requirement of the NormalizedRepresentation concept

namespace telamon_simulator {

/// \brief Measures the contention whic was encountered during simulation
/// \details Keeps an internal counter of the detected contention and responds according to it.
class ContentionFailureCounter {
 public:
  constexpr static inline int THRESHOLD = 2;
  constexpr static inline int FAST_PATH_RETRY_THRESHOLD = 3;

 public:
  auto detect () -> bool {
	  return (++m_counter > ContentionFailureCounter::THRESHOLD);
  }
  auto get () const noexcept -> int { return m_counter; }

 private:
  int m_counter{0};
};

/// \brief Represents the status of a CAS primitive
enum class CasStatus : char {
  Pending,
  Success,
  Failure
};

/// \brief 		Solves the ABA problem
/// \details 	See p.15 of the paper
/// \tparam Cas Cas primitive
/// \details 	About execute: Returns either a bool marking whether the CAS was executed successfully or an error marking
///     		there was contention during the execution
template<typename Cas>
concept CasWithVersioning = requires (Cas cas_, CasStatus status, ContentionFailureCounter &failures, CasStatus expected, CasStatus desired){
	{ cas_.has_modified_bit() } -> std::same_as<bool>;
	{ cas_.clear_bit() };
	{ cas_.state() } -> std::same_as<CasStatus>;
	{ cas_.set_state(status) };
	{ cas_.swap_state(expected, desired) } -> std::same_as<bool>;
	{ cas_.execute(failures) } -> std::same_as<nonstd::expected<bool, std::monostate>>;
};

namespace cas_versioning {

/// \note T has to implement comparison operators
template<typename T>
struct Cas {
  /// uint_least64_t is used to guarantee (minimize) the chance of the ABA problem occurring
  using VersionNum = uint_least64_t;

  T value;
  VersionNum version{0};

  explicit Cas (T &&t_value) : value{std::forward<T>(t_value)} {}
  Cas (T &&t_value, VersionNum t_version) : value{std::forward<T>(t_value)}, version{t_version} {}
};

/// \note T has to implement comparison operators
/// \copydetails Versioning.hh
template<typename T>
class [[maybe_unused]] VersionedAtomic {
 public:
  explicit VersionedAtomic (T &&value) : m_ptr{std::atomic(new Cas<T>{std::forward<T>(value)})} {}
  VersionedAtomic (const VersionedAtomic &) = delete;
  VersionedAtomic (VersionedAtomic &&) noexcept = default;

 public:
  /// \brief Load the value stored inside
  [[maybe_unused]] auto load () const noexcept -> Cas<T> * { return m_ptr.load(); }

  /// \brief Store a value inside
  [[maybe_unused]] auto store (T &&new_value) noexcept {
	  auto ptr = load();
	  auto actual = ptr->value;
	  auto actual_version = ptr->version;
	  if (actual == new_value) { return; }
	  auto new_ptr = new Cas<T>{std::forward<T>(new_value), actual_version + 1};
	  m_ptr.store(new_ptr);
  }

  /// \brief Apply a function to the value inside
  /// \tparam R 	The return type of the function. Also used as the return type of `transform`
  /// \param  fun 	The function applied to the value inside
  template<typename Fun/*, typename Ret*/>
  [[maybe_unused]] auto transform (Fun fun) /* -> Ret */ {
	  auto loaded = m_ptr.load();
	  return fun(loaded->value, loaded->version);
  }

  /// \brief Performs a CAS on the value stored inside
  /// \param expected 	The expected value
  /// \param desired 	The value which will placed
  /// \param failures	The contention counter
  /// \ret	 None   if failed (contention)
  ///		 False 	if some of the requirements were not met
  ///		 True 	if the CAS was performed successfully
  [[maybe_unused]] auto compare_exchange_weak (const T &expected, std::optional<typename Cas<T>::VersionNum> expected_version_opt,
                                               T &&desired, ContentionFailureCounter &failures) -> std::optional<bool> {
	  auto ptr = load();
	  auto actual = ptr->value;
	  auto actual_version = ptr->version;
	  if (expected != actual) {
		  return std::make_optional(false);
	  }

	  if (expected_version_opt && expected_version_opt.value() != actual_version) {
		  if (failures.detect()) { return std::nullopt; }       //< Contention
		  return std::make_optional(false);
	  }

	  if (actual == desired) {
		  return std::make_optional(true);
	  }

	  // TODO: Hazptr
	  auto new_ptr = new Cas<T>{std::forward<T>(desired), actual_version + 1};

	  auto cas_result = std::make_optional(m_ptr.compare_exchange_strong(ptr, new_ptr));
	  if (!cas_result && failures.detect()) { return std::nullopt; } //< Contention
	  return cas_result;
  }

  template<typename ...Args>
  [[maybe_unused]] auto compare_exchange_strong (Args &&... args) -> bool {
	  while (true) {
		  auto res = compare_exchange_weak(std::forward<Args>(args)...);
		  if (res == std::nullopt) continue;
		  return res.value();
	  }
  }

 private:
  std::atomic<Cas<T> *> m_ptr;
};

}
}

#endif //TELAMON_SRC_TELAMON_VERSIONING_HH_
