/**
 * \file NormalizedRepresentation.hh
 * \brief Provides the foundational structure of a normalized structure which the simulated algorithm is required to adhere to
 */
#ifndef TELAMON_NORMALIZED_REPRESENTATION_HH
#define TELAMON_NORMALIZED_REPRESENTATION_HH

#include <concepts>
#include <variant>
#include <vector>
#include <thread>
#include <algorithm>
#include <ranges>
#include <mutex>
#include <numeric>
#include <utility>
#include <iterator>
#include <optional>

#include <nonstd/expected.hpp>

#include "Versioning.hh"

/// \brief This module encapsulates the implementation of the simulator
namespace telamon_simulator {

/// \brief Requires commit to be iterable and its items to satisfy CasWithVersioning
/// \tparam Commit Structure which represents a commit point
template<typename Commit>
concept Commits = requires (Commit desc) {
	requires std::ranges::input_range<Commit>;
	requires CasWithVersioning<std::ranges::range_value_t<Commit>>;
};

/// \brief   Here are the operations which are required to be described in the lock-free algorithm in order to use the
/// 	     simulation. There are 3 types which the lock-free has to define according to its specifics as well as 3 functions.
/// \tparam  LockFree The lock-free algorithm which is being simulated
/// \details The`generator` and `wrap_up` functions correspond to the first and third stage of the algorithm operation.
/// 		 The fast path represents the steps which are used when the operation in executed as lock-free.
template<typename LockFree>
concept NormalizedRepresentation = requires (LockFree lf,
                                             ContentionFailureCounter &contention,
                                             const typename LockFree::Input &inp,
                                             const typename LockFree::Commit &desc,
                                             const nonstd::expected<std::monostate, std::optional<int>> &executed
){
	typename LockFree::Input;
	typename LockFree::Output;
	typename LockFree::Commit;

	requires Commits<typename LockFree::Commit>;
	requires std::is_copy_constructible_v<typename LockFree::Commit>;

	{ lf.generator(inp, contention) } -> std::same_as<std::optional<typename LockFree::Commit>>;
	{ lf.wrap_up(executed, desc, contention) } -> std::same_as<nonstd::expected<std::optional<typename LockFree::Output>, std::monostate>>;
	{ lf.fast_path(inp, contention) } -> std::same_as<std::optional<typename LockFree::Output>>;
};

}

#endif // TELAMON_NORMALIZED_REPRESENTATION_HH
