#ifndef MEXFORGE_CORE_TYPES_HPP
#define MEXFORGE_CORE_TYPES_HPP

#include "mex.hpp"
#include "MatlabDataArray.hpp"

#include <optional>
#include <string>
#include <type_traits>
#include <vector>
#include <complex>
#include <cstdint>

namespace mexforge {

// ============================================================================
// Type traits for MATLAB <-> C++ mapping
// ============================================================================

// Primary trait: maps C++ types to MATLAB ArrayType
template<typename T, typename Enable = void>
struct MatlabTypeTrait;

template<>
struct MatlabTypeTrait<double> {
    static constexpr auto array_type = matlab::data::ArrayType::DOUBLE;
    using matlab_type = double;
};

template<>
struct MatlabTypeTrait<float> {
    static constexpr auto array_type = matlab::data::ArrayType::SINGLE;
    using matlab_type = float;
};

template<>
struct MatlabTypeTrait<int32_t> {
    static constexpr auto array_type = matlab::data::ArrayType::INT32;
    using matlab_type = int32_t;
};

template<>
struct MatlabTypeTrait<uint32_t> {
    static constexpr auto array_type = matlab::data::ArrayType::UINT32;
    using matlab_type = uint32_t;
};

template<>
struct MatlabTypeTrait<int64_t> {
    static constexpr auto array_type = matlab::data::ArrayType::INT64;
    using matlab_type = int64_t;
};

template<>
struct MatlabTypeTrait<uint64_t> {
    static constexpr auto array_type = matlab::data::ArrayType::UINT64;
    using matlab_type = uint64_t;
};

template<>
struct MatlabTypeTrait<bool> {
    static constexpr auto array_type = matlab::data::ArrayType::LOGICAL;
    using matlab_type = bool;
};

template<>
struct MatlabTypeTrait<std::string> {
    static constexpr auto array_type = matlab::data::ArrayType::MATLAB_STRING;
    using matlab_type = matlab::data::MATLABString;
};

template<typename T>
struct MatlabTypeTrait<std::complex<T>> {
    static constexpr auto array_type = matlab::data::ArrayType::COMPLEX_DOUBLE;
    using matlab_type = std::complex<T>;
};

template<>
struct MatlabTypeTrait<matlab::data::StructArray> {
    static constexpr auto array_type = matlab::data::ArrayType::STRUCT;
    using matlab_type = matlab::data::StructArray;
};

// ============================================================================
// Optional detection
// ============================================================================

template<typename T>
struct is_optional : std::false_type {};

template<typename T>
struct is_optional<std::optional<T>> : std::true_type {};

template<typename T>
inline constexpr bool is_optional_v = is_optional<T>::value;

// Unwrap optional: optional<T> -> T, T -> T
template<typename T>
struct unwrap_optional { using type = T; };

template<typename T>
struct unwrap_optional<std::optional<T>> { using type = T; };

template<typename T>
using unwrap_optional_t = typename unwrap_optional<T>::type;

// ============================================================================
// Vector detection
// ============================================================================

template<typename T>
struct is_vector : std::false_type {};

template<typename T>
struct is_vector<std::vector<T>> : std::true_type {};

template<typename T>
inline constexpr bool is_vector_v = is_vector<T>::value;

// ============================================================================
// Struct detection (user-extensible via specialization)
// ============================================================================

template<typename T, typename = void>
struct is_mex_struct : std::false_type {};

template<typename T>
inline constexpr bool is_mex_struct_v = is_mex_struct<T>::value;

// ============================================================================
// Argument descriptor (compile-time)
// ============================================================================

template<typename... Args>
struct ArgList {
    static constexpr size_t total = sizeof...(Args);

    static constexpr size_t required_count() {
        size_t count = 0;
        ((count += is_optional_v<Args> ? 0 : 1), ...);
        return count;
    }
};

} // namespace mexforge

#endif // MEXFORGE_CORE_TYPES_HPP
