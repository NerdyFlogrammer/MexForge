// Mock MATLAB Data Array headers for unit testing (no MATLAB required)
// Provides minimal but functional implementations for testing marshalling,
// registry, object store, and runner logic.
#ifndef MOCK_MATLAB_DATA_ARRAY_HPP
#define MOCK_MATLAB_DATA_ARRAY_HPP

#include <string>
#include <vector>
#include <complex>
#include <cstdint>
#include <cstddef>
#include <any>
#include <variant>
#include <stdexcept>

namespace matlab {
namespace data {

enum class ArrayType {
    DOUBLE, SINGLE, INT8, UINT8, INT16, UINT16,
    INT32, UINT32, INT64, UINT64,
    LOGICAL, CHAR, MATLAB_STRING, STRUCT,
    COMPLEX_DOUBLE, COMPLEX_SINGLE,
    COMPLEX_INT8, COMPLEX_INT16, COMPLEX_INT32, COMPLEX_INT64,
    COMPLEX_UINT8, COMPLEX_UINT16, COMPLEX_UINT32, COMPLEX_UINT64
};

using MATLABString = std::string;

// Minimal Array that can hold a value for testing
class Array {
public:
    Array() = default;

    template<typename T>
    explicit Array(T val) : value_(val) {}

    size_t getNumberOfElements() const { return 1; }
    ArrayType getType() const { return type_; }

    // Nested subscript: arr[0][0][0] returns stored value
    struct ValueProxy {
        std::any value;

        template<typename T>
        operator T() const {
            if (value.has_value()) {
                return std::any_cast<T>(value);
            }
            return T{};
        }
    };

    struct L1Proxy {
        std::any value;
        ValueProxy operator[](size_t) const { return {value}; }
    };

    L1Proxy operator[](size_t) const { return {value_}; }
    L1Proxy operator[](size_t) { return {value_}; }

    // Assignment from value
    template<typename T>
    Array& operator=(T val) { value_ = val; return *this; }

    const std::any& stored_value() const { return value_; }

private:
    std::any value_;
    ArrayType type_ = ArrayType::DOUBLE;
};

template<typename T>
class TypedArray : public Array {
public:
    TypedArray() = default;
    TypedArray(const Array& arr) : Array(arr) {}

    T operator[](size_t) const {
        if (stored_value().has_value()) {
            return std::any_cast<T>(stored_value());
        }
        return T{};
    }

    struct Iterator {
        bool done = true;
        T val{};
        T operator*() const { return val; }
        Iterator& operator++() { done = true; return *this; }
        bool operator!=(const Iterator& other) const { return done != other.done; }
    };
    Iterator begin() const { return {false, operator[](0)}; }
    Iterator end() const { return {true, T{}}; }
};

class StringArray : public Array {
public:
    StringArray() = default;
    StringArray(const Array& arr) : Array(arr) {}

    struct Proxy {
        std::any value;
        operator std::string() const {
            if (value.has_value()) {
                return std::any_cast<std::string>(value);
            }
            return "";
        }
    };
    Proxy operator[](size_t) const { return {stored_value()}; }
};

class StructArray : public Array {
public:
    StructArray() = default;
    StructArray(const Array&) {}

    struct FieldProxy {
        Array operator[](const std::string&) const { return {}; }
    };
    FieldProxy operator[](size_t) const { return {}; }
};

class ArrayFactory {
public:
    template<typename T>
    Array createScalar(const T& val) { return Array(val); }

    Array createScalar(const std::string& val) { return Array(val); }

    template<typename It>
    Array createArray(std::initializer_list<size_t>, It, It) { return {}; }

    StructArray createStructArray(std::initializer_list<size_t>,
                                  std::initializer_list<std::string>) { return {}; }
};

} // namespace data
} // namespace matlab

#endif // MOCK_MATLAB_DATA_ARRAY_HPP
