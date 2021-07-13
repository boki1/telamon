#ifndef TELAMON_SRC_TELAMON_VERSIONING_HH_
#define TELAMON_SRC_TELAMON_VERSIONING_HH_

//! \file 		Versioning.hh
//! \brief 		Definitions of ContentionFailureCounter, CasStatus, CasDescriptor, CasWithVersioning, VersionedAtomic
//! \details 	VersionedAtomic is used by the user to implement the required functions of CasWithVersioning,
//! 			requirement of the NormalizedRepresentation concept

namespace telamon_simulator {

/// \brief Measures the contention which was encountered during simulation
/// \details Keeps an internal counter of the detected contention and responds according to it.
class ContentionFailureCounter {
 public:
  constexpr static inline int THRESHOLD = 2;
  constexpr static inline int FAST_PATH_RETRY_THRESHOLD = 3;

 public:
  auto detect () -> bool {
	  return (++m_counter > ContentionFailureCounter::THRESHOLD);
  }
  [[nodiscard]] auto get () const noexcept -> int { return m_counter; }

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
/// \tparam Cas CasDescriptor primitive
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

namespace versioning {

/// uint_least64_t is used to guarantee (minimize) the chance of the ABA problem occurring
using VersionNum = uint_least64_t;

namespace telamon_private {
template<typename ValType>
struct ReferencedBase {
  ValType value;
  VersionNum version{0};
  explicit ReferencedBase (ValType &&t_value, VersionNum t_version = 0, bool mod = false)
	  : value{std::move(t_value)},
	    version{t_version} {}
};
}

/// \brief Used to represent a value which is referenced by a "node" from the structure but with additional meta data(Meta) and versioning(VersionNum)
/// \note T has to implement comparison operators
template<typename ValType, typename Meta=void>
struct Referenced : telamon_private::ReferencedBase<ValType> {
  Meta meta;

  explicit Referenced (ValType t_value, Meta t_meta, VersionNum t_version = 0)
	  : telamon_private::ReferencedBase<ValType>(std::move(t_value), t_version),
	    meta{std::forward<Meta>(t_meta)} {}

  Referenced (const Referenced &rhs)
	  : meta{rhs.meta}, telamon_private::ReferencedBase<ValType>{rhs.value, rhs.version} {}
};

template<typename ValType>
struct Referenced<ValType, void> : telamon_private::ReferencedBase<ValType> {
  explicit Referenced (ValType t_value, VersionNum t_version = 0)
	  : telamon_private::ReferencedBase<ValType>(std::move(t_value), t_version) {}

  Referenced (const Referenced &rhs)
	  : telamon_private::ReferencedBase<ValType>{rhs.value, rhs.version} {}
};

/// \note T has to implement comparison operators
/// \copydetails Versioning.hh
template<typename ValType, typename Meta=void>
class [[maybe_unused]] VersionedAtomic {
 public:

  template<typename ...Args>
  explicit VersionedAtomic (Meta meta, Args &&... args)
	  : m_ptr{std::atomic(new Referenced<ValType, Meta>{ValType{std::forward<Args>(args)...}, std::move(meta)})} {}

  [[maybe_unused]] explicit VersionedAtomic (ValType value, Meta meta = {})
	  : m_ptr{std::atomic(new Referenced<ValType, Meta>{std::move(value), std::move(meta)})} {}

  VersionedAtomic (const VersionedAtomic &rhs)
	  : m_ptr{rhs.m_ptr.load()} {}

 public:
  /// \brief Load the value stored inside
  [[maybe_unused]] auto load () const noexcept -> Referenced<ValType, Meta> * { return m_ptr.load(); }

  /// \brief Store a value inside
  [[maybe_unused]] auto store (ValType new_value, std::optional<Meta> new_meta = {}) noexcept {
	  auto ptr = load();
	  auto actual = ptr->value;
	  auto actual_version = ptr->version;
	  if (actual == new_value) { return; }
	  auto new_ptr = new Referenced<ValType, Meta>{
		  std::move(new_value),
		  (new_meta.has_value() ? new_meta.value() : ptr->meta),
		  actual_version + 1
	  };
	  m_ptr.store(new_ptr);
  }

  /// \brief Apply a function to the value inside
  /// \tparam R 	The return type of the function. Also used as the return type of `transform`
  /// \param  fun 	The function applied to the value inside
  template<typename Fun/*, typename Ret*/>
  [[maybe_unused]] auto transform (Fun fun) const {
	  auto loaded = m_ptr.load();
	  return fun(loaded->value, loaded->version, loaded->meta);
  }

  [[nodiscard]] auto version () const noexcept -> VersionNum { return m_ptr.load()->version; }

  /// \brief Performs a CAS on the value stored inside
  /// \param expected 	The expected value
  /// \param desired 	The value which will placed
  /// \param failures	The contention counter
  /// \ret	 None   if failed (contention)
  ///		 False 	if some of the requirements were not met
  ///		 True 	if the CAS was performed successfully
  [[maybe_unused]] auto compare_exchange_weak (const ValType &expected,
                                               std::optional<versioning::VersionNum> expected_version_opt,
                                               ValType desired,
                                               Meta desired_meta,
                                               ContentionFailureCounter &failures) -> std::optional<bool> {
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
	  auto new_ref = new Referenced<ValType, Meta>{std::move(desired), std::move(desired_meta), actual_version + 1};

	  auto cas_result = std::make_optional(m_ptr.compare_exchange_strong(ptr, new_ref));
	  if (!cas_result && failures.detect()) { return std::nullopt; } //< Contention
	  if (cas_result && cas_result.value() == true) { m_modified_bit.store(true); }
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

  [[nodiscard]] auto has_modified_bit () const noexcept -> bool { return m_modified_bit.load(); }

  void clear_modified_bit () noexcept {
	  auto expected = false;
	  auto _ = m_modified_bit.compare_exchange_strong(expected, true);
  }

 private:
  std::atomic<Referenced<ValType, Meta> *> m_ptr{};
  std::atomic<bool> m_modified_bit{false};
};

template<typename ValType>
class VersionedAtomic<ValType, void> {
 public:
  explicit VersionedAtomic (ValType &&value) : m_ptr{std::atomic(new Referenced<ValType>{std::forward<ValType>(value)})} {}
  VersionedAtomic (const VersionedAtomic &) = delete;
  VersionedAtomic (VersionedAtomic &&) noexcept = default;

 public:
  /// \brief Load the value stored inside
  [[maybe_unused]] auto load () const noexcept -> Referenced<ValType> * { return m_ptr.load(); }

  /// \brief Store a value inside
  [[maybe_unused]] auto store (ValType new_value) noexcept {
	  auto ptr = load();
	  auto actual = ptr->value;
	  auto actual_version = ptr->version;
	  if (actual == new_value) { return; }
	  auto new_ptr = new Referenced<ValType>{std::move(new_value), actual_version + 1};
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
  [[maybe_unused]] auto compare_exchange_weak (const ValType &expected, std::optional<versioning::VersionNum> expected_version_opt,
                                               ValType &&desired, ContentionFailureCounter &failures) -> std::optional<bool> {
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
	  auto new_ptr = new Referenced<ValType>{std::move(desired), actual_version + 1};

	  auto cas_result = std::make_optional(m_ptr.compare_exchange_strong(ptr, new_ptr));
	  if (!cas_result && failures.detect()) { return std::nullopt; } //< Contention
	  if (cas_result) { m_modified_bit.store(true); }
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

  [[nodiscard]] auto has_modified_bit () const noexcept -> bool { return m_modified_bit.load(); }

  void clear_modified_bit () noexcept {
	  auto expected = false;
	  auto _ = m_modified_bit.compare_exchange_strong(expected, true);
  }

 private:
  std::atomic<Referenced<ValType> *> m_ptr{};
  std::atomic<bool> m_modified_bit{false};
};

}
}

#endif //TELAMON_SRC_TELAMON_VERSIONING_HH_
