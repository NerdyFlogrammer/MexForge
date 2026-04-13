#ifndef MEXFORGE_CORE_MARSHALLER_HPP
#define MEXFORGE_CORE_MARSHALLER_HPP

#include "types.hpp"

#include <stdexcept>
#include <string>
#include <sstream>

namespace mexforge {

// ============================================================================
// from_matlab: Convert MATLAB Array -> C++ type
// ============================================================================

template<typename T, typename Enable = void>
struct FromMatlab {
    static T convert(const matlab::data::Array& /*arr*/) {
        static_assert(!std::is_same_v<T, T>,
            "No FromMatlab specialization for this type. "
            "Provide a specialization or use a StructMarshaller.");
    }
};

// Scalars: double, float, int32, uint32, int64, uint64, bool
template<typename T>
struct FromMatlab<T, std::enable_if_t<std::is_arithmetic_v<T> && !std::is_same_v<T, bool>>> {
    static T convert(const matlab::data::Array& arr) {
        const matlab::data::TypedArray<T> typed = arr;
        return typed[0];
    }
};

template<>
struct FromMatlab<bool> {
    static bool convert(const matlab::data::Array& arr) {
        const matlab::data::TypedArray<bool> typed = arr;
        return typed[0];
    }
};

// std::string
template<>
struct FromMatlab<std::string> {
    static std::string convert(const matlab::data::Array& arr) {
        const matlab::data::StringArray strArr = arr;
        return std::string(strArr[0]);
    }
};

// std::complex<T>
template<typename T>
struct FromMatlab<std::complex<T>> {
    static std::complex<T> convert(const matlab::data::Array& arr) {
        const matlab::data::TypedArray<std::complex<T>> typed = arr;
        return typed[0];
    }
};

// std::vector<T> (from MATLAB array)
template<typename T>
struct FromMatlab<std::vector<T>> {
    static std::vector<T> convert(const matlab::data::Array& arr) {
        const matlab::data::TypedArray<T> typed = arr;
        std::vector<T> result;
        result.reserve(arr.getNumberOfElements());
        for (auto elem : typed) {
            result.push_back(elem);
        }
        return result;
    }
};

// std::optional<T>: delegates to FromMatlab<T>
template<typename T>
struct FromMatlab<std::optional<T>> {
    static std::optional<T> convert(const matlab::data::Array& arr) {
        return FromMatlab<T>::convert(arr);
    }
};

// matlab::data::Array passthrough (for raw access)
template<>
struct FromMatlab<matlab::data::Array> {
    static matlab::data::Array convert(const matlab::data::Array& arr) {
        return arr;
    }
};

// matlab::data::StructArray passthrough
template<>
struct FromMatlab<matlab::data::StructArray> {
    static matlab::data::StructArray convert(const matlab::data::Array& arr) {
        return static_cast<matlab::data::StructArray>(arr);
    }
};

// ============================================================================
// to_matlab: Convert C++ type -> MATLAB Array
// ============================================================================

template<typename T, typename Enable = void>
struct ToMatlab {
    static matlab::data::Array convert(matlab::data::ArrayFactory& /*factory*/, const T& /*val*/) {
        static_assert(!std::is_same_v<T, T>,
            "No ToMatlab specialization for this type.");
    }
};

// Scalars
template<typename T>
struct ToMatlab<T, std::enable_if_t<std::is_arithmetic_v<T>>> {
    static matlab::data::Array convert(matlab::data::ArrayFactory& factory, const T& val) {
        return factory.createScalar(val);
    }
};

// std::string
template<>
struct ToMatlab<std::string> {
    static matlab::data::Array convert(matlab::data::ArrayFactory& factory, const std::string& val) {
        return factory.createScalar(val);
    }
};

// std::complex<T>
template<typename T>
struct ToMatlab<std::complex<T>> {
    static matlab::data::Array convert(matlab::data::ArrayFactory& factory, const std::complex<T>& val) {
        return factory.createScalar(val);
    }
};

// std::vector<T>
template<typename T>
struct ToMatlab<std::vector<T>> {
    static matlab::data::Array convert(matlab::data::ArrayFactory& factory, const std::vector<T>& val) {
        return factory.createArray({1, val.size()}, val.begin(), val.end());
    }
};

// matlab::data::Array passthrough
template<>
struct ToMatlab<matlab::data::Array> {
    static matlab::data::Array convert(matlab::data::ArrayFactory&, const matlab::data::Array& val) {
        return val;
    }
};

// matlab::data::StructArray passthrough
template<>
struct ToMatlab<matlab::data::StructArray> {
    static matlab::data::Array convert(matlab::data::ArrayFactory&, const matlab::data::StructArray& val) {
        return val;
    }
};

// void (no output)
template<>
struct ToMatlab<void> {
    // Not called - handled by runner
};

// ============================================================================
// Convenience functions
// ============================================================================

template<typename T>
T from_matlab(const matlab::data::Array& arr) {
    return FromMatlab<T>::convert(arr);
}

template<typename T>
matlab::data::Array to_matlab(matlab::data::ArrayFactory& factory, const T& val) {
    return ToMatlab<T>::convert(factory, val);
}

// ============================================================================
// StructMarshaller: User-specializable for custom struct conversion
//
// Usage:
//   template<>
//   struct StructMarshaller<MyStruct> {
//       static MyStruct from_matlab(const matlab::data::StructArray& arr) { ... }
//       static matlab::data::StructArray to_matlab(ArrayFactory& f, const MyStruct& s) { ... }
//   };
// ============================================================================

template<typename T>
struct StructMarshaller {
    // Users specialize this for their structs
    static_assert(!std::is_same_v<T, T>,
        "Provide a StructMarshaller specialization for your struct type.");
};

} // namespace mexforge

#endif // MEXFORGE_CORE_MARSHALLER_HPP
