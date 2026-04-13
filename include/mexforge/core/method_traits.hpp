#ifndef MEXFORGE_CORE_METHOD_TRAITS_HPP
#define MEXFORGE_CORE_METHOD_TRAITS_HPP

#include <tuple>
#include <type_traits>

namespace mexforge {

// ============================================================================
// MethodTraits: Compile-time extraction of method signatures
//
// Given a member function pointer, extracts:
//   - ReturnType
//   - ClassType (the owning class)
//   - ArgTypes (as tuple)
//   - Arity
//   - is_const
// ============================================================================

template<typename T>
struct MethodTraits;

// Non-const member function
template<typename R, typename C, typename... Args>
struct MethodTraits<R(C::*)(Args...)> {
    using ReturnType = R;
    using ClassType = C;
    using ArgTypes = std::tuple<Args...>;
    static constexpr size_t arity = sizeof...(Args);
    static constexpr bool is_const = false;
};

// Const member function
template<typename R, typename C, typename... Args>
struct MethodTraits<R(C::*)(Args...) const> {
    using ReturnType = R;
    using ClassType = C;
    using ArgTypes = std::tuple<Args...>;
    static constexpr size_t arity = sizeof...(Args);
    static constexpr bool is_const = true;
};

// Free function / static member
template<typename R, typename... Args>
struct MethodTraits<R(*)(Args...)> {
    using ReturnType = R;
    using ClassType = void;
    using ArgTypes = std::tuple<Args...>;
    static constexpr size_t arity = sizeof...(Args);
    static constexpr bool is_const = false;
};

// std::function and lambdas via operator()
template<typename R, typename C, typename... Args>
struct MethodTraits<R(C::*)(Args...) const> ;  // Already defined above

// Deduce from lambda/functor by looking at operator()
template<typename T>
struct MethodTraits : MethodTraits<decltype(&T::operator())> {};

// ============================================================================
// FunctionTraits: For non-member function pointers (NTTP)
//
// Usage:
//   constexpr auto traits = FnTraits<&MyClass::getValue>();
//   using Ret = typename decltype(traits)::ReturnType;
// ============================================================================

template<auto Fn>
struct FnTraits : MethodTraits<decltype(Fn)> {};

// ============================================================================
// Helpers: Extract Nth argument type
// ============================================================================

template<size_t N, typename MethodPtr>
using arg_type_t = std::tuple_element_t<N, typename MethodTraits<MethodPtr>::ArgTypes>;

template<auto Fn, size_t N>
using fn_arg_t = std::tuple_element_t<N, typename FnTraits<Fn>::ArgTypes>;

} // namespace mexforge

#endif // MEXFORGE_CORE_METHOD_TRAITS_HPP
