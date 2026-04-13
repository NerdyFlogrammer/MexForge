// Mock MATLAB Data Array headers for unit testing (no MATLAB required)
// Provides minimal but functional implementations for testing marshalling,
// registry, object store, and runner logic.
//
// Key design decisions:
// - Array holds either a scalar (std::any) or a vector (via set_vector)
// - TypedArray iterator walks all elements for vector round-trip testing
// - ArrayFactory::createArray stores a real std::vector<T>
// - StructArray supports field read/write for __arg_info testing
// - ArgumentList uses shared_ptr storage (mirrors MATLAB's ref-counted API)
#ifndef MOCK_MATLAB_DATA_ARRAY_HPP
#define MOCK_MATLAB_DATA_ARRAY_HPP

#include <any>
#include <complex>
#include <cstddef>
#include <cstdint>
#include <map>
#include <stdexcept>
#include <string>
#include <variant>
#include <vector>

namespace matlab {
namespace data {

enum class ArrayType {
    DOUBLE,
    SINGLE,
    INT8,
    UINT8,
    INT16,
    UINT16,
    INT32,
    UINT32,
    INT64,
    UINT64,
    LOGICAL,
    CHAR,
    MATLAB_STRING,
    STRUCT,
    COMPLEX_DOUBLE,
    COMPLEX_SINGLE,
    COMPLEX_INT8,
    COMPLEX_INT16,
    COMPLEX_INT32,
    COMPLEX_INT64,
    COMPLEX_UINT8,
    COMPLEX_UINT16,
    COMPLEX_UINT32,
    COMPLEX_UINT64
};

using MATLABString = std::string;

// ============================================================================
// Array: holds either a scalar or a vector of any type
// ============================================================================

class Array {
public:
    Array() = default;

    template<typename T>
    explicit Array(T val) : value_(std::move(val)), num_elements_(1) {}

    // Store a vector (called by ArrayFactory::createArray)
    template<typename T>
    void set_vector(std::vector<T> vec) {
        num_elements_ = vec.size();
        value_ = std::move(vec);
    }

    size_t getNumberOfElements() const { return num_elements_; }
    ArrayType getType() const { return type_; }

    // Nested subscript proxy for struct-field access: arr[i]["field"]
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

    template<typename T>
    Array& operator=(T val) {
        value_ = std::move(val);
        num_elements_ = 1;
        return *this;
    }

    const std::any& stored_value() const { return value_; }

protected:
    std::any value_;
    size_t num_elements_ = 0;
    ArrayType type_ = ArrayType::DOUBLE;
};

// ============================================================================
// TypedArray<T>: indexed + iterable access over scalar or vector
// ============================================================================

template<typename T>
class TypedArray : public Array {
public:
    TypedArray() = default;
    TypedArray(const Array& arr) : Array(arr) {}

    T operator[](size_t i) const {
        if (!stored_value().has_value())
            return T{};
        // Vector storage
        if (const auto* vec = std::any_cast<std::vector<T>>(&stored_value())) {
            return i < vec->size() ? (*vec)[i] : T{};
        }
        // Scalar storage (only index 0 makes sense)
        try {
            return std::any_cast<T>(stored_value());
        } catch (...) {
            return T{};
        }
    }

    struct Iterator {
        const TypedArray<T>* arr;
        size_t idx;
        T operator*() const { return (*arr)[idx]; }
        Iterator& operator++() {
            ++idx;
            return *this;
        }
        bool operator!=(const Iterator& other) const { return idx != other.idx; }
    };
    Iterator begin() const { return {this, 0}; }
    Iterator end() const { return {this, getNumberOfElements()}; }
};

// ============================================================================
// StringArray
// ============================================================================

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

// ============================================================================
// StructArray: supports field read/write for __arg_info testing
// ============================================================================

class StructArray : public Array {
public:
    StructArray() = default;
    StructArray(const Array& /*arr*/) {}

    // Element proxy: supports arr[i]["field"] = value and arr[i]["field"]
    struct ElementProxy {
        std::map<std::string, Array>& fields;

        Array& operator[](const std::string& key) { return fields[key]; }
        const Array& operator[](const std::string& key) const {
            static Array empty;
            auto it = fields.find(key);
            return it != fields.end() ? it->second : empty;
        }
    };

    ElementProxy operator[](size_t i) {
        if (i >= rows_.size())
            rows_.resize(i + 1);
        return ElementProxy{rows_[i]};
    }

    size_t size() const { return rows_.size(); }

private:
    std::vector<std::map<std::string, Array>> rows_;
};

// ============================================================================
// ArrayFactory
// ============================================================================

class ArrayFactory {
public:
    template<typename T>
    Array createScalar(const T& val) {
        return Array(val);
    }

    Array createScalar(const std::string& val) { return Array(val); }

    // Store the iterator range as a std::vector<T> inside an Array
    template<typename It>
    Array createArray(std::initializer_list<size_t> /*shape*/, It begin, It end) {
        using T = typename std::iterator_traits<It>::value_type;
        Array arr;
        arr.set_vector(std::vector<T>(begin, end));
        return arr;
    }

    StructArray createStructArray(std::initializer_list<size_t> shape,
                                  std::initializer_list<std::string> /*fields*/) {
        StructArray sa;
        size_t n = shape.size() > 1 ? *(shape.begin() + 1) : 1;
        (void)n;
        return sa;
    }
};

} // namespace data
} // namespace matlab

#endif // MOCK_MATLAB_DATA_ARRAY_HPP
