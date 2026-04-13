// Tests for marshaller — from_matlab / to_matlab round-trips
//
// Covers every specialization in marshaller.hpp using the mock MATLAB headers.
// Each test validates that a C++ value survives a round-trip through the
// MATLAB array representation without loss.

#include "../include/mexforge/core/marshaller.hpp"
#include <cassert>
#include <cmath>
#include <complex>
#include <iostream>
#include <optional>
#include <string>
#include <vector>

// ---- helpers ----------------------------------------------------------------

static matlab::data::ArrayFactory factory;

template<typename T>
static matlab::data::Array make_array(const T& val) {
    return mexforge::to_matlab(factory, val);
}

// ---- from_matlab ------------------------------------------------------------

void test_from_double() {
    auto arr = matlab::data::Array(3.14);
    double val = mexforge::from_matlab<double>(arr);
    assert(std::abs(val - 3.14) < 1e-10);
    std::cout << "  [PASS] from_matlab<double>\n";
}

void test_from_float() {
    auto arr = matlab::data::Array(2.5f);
    float val = mexforge::from_matlab<float>(arr);
    assert(std::abs(val - 2.5f) < 1e-5f);
    std::cout << "  [PASS] from_matlab<float>\n";
}

void test_from_int32() {
    auto arr = matlab::data::Array(int32_t{-42});
    int32_t val = mexforge::from_matlab<int32_t>(arr);
    assert(val == -42);
    std::cout << "  [PASS] from_matlab<int32_t>\n";
}

void test_from_uint32() {
    auto arr = matlab::data::Array(uint32_t{99});
    uint32_t val = mexforge::from_matlab<uint32_t>(arr);
    assert(val == 99u);
    std::cout << "  [PASS] from_matlab<uint32_t>\n";
}

void test_from_int64() {
    auto arr = matlab::data::Array(int64_t{-1000000});
    int64_t val = mexforge::from_matlab<int64_t>(arr);
    assert(val == int64_t{-1000000});
    std::cout << "  [PASS] from_matlab<int64_t>\n";
}

void test_from_bool_true() {
    auto arr = matlab::data::Array(true);
    bool val = mexforge::from_matlab<bool>(arr);
    assert(val == true);
    std::cout << "  [PASS] from_matlab<bool> (true)\n";
}

void test_from_bool_false() {
    auto arr = matlab::data::Array(false);
    bool val = mexforge::from_matlab<bool>(arr);
    assert(val == false);
    std::cout << "  [PASS] from_matlab<bool> (false)\n";
}

void test_from_string() {
    auto arr = matlab::data::Array(std::string("hello world"));
    std::string val = mexforge::from_matlab<std::string>(arr);
    assert(val == "hello world");
    std::cout << "  [PASS] from_matlab<std::string>\n";
}

void test_from_string_empty() {
    auto arr = matlab::data::Array(std::string(""));
    std::string val = mexforge::from_matlab<std::string>(arr);
    assert(val.empty());
    std::cout << "  [PASS] from_matlab<std::string> (empty)\n";
}

void test_from_optional_present() {
    auto arr = matlab::data::Array(7.0);
    auto val = mexforge::from_matlab<std::optional<double>>(arr);
    assert(val.has_value());
    assert(std::abs(*val - 7.0) < 1e-10);
    std::cout << "  [PASS] from_matlab<std::optional<double>>\n";
}

void test_from_array_passthrough() {
    auto original = matlab::data::Array(42.0);
    auto val = mexforge::from_matlab<matlab::data::Array>(original);
    // Should return the array unchanged
    double recovered = mexforge::from_matlab<double>(val);
    assert(std::abs(recovered - 42.0) < 1e-10);
    std::cout << "  [PASS] from_matlab<Array> passthrough\n";
}

// ---- to_matlab --------------------------------------------------------------

void test_to_double() {
    auto arr = mexforge::to_matlab(factory, 1.23);
    double val = mexforge::from_matlab<double>(arr);
    assert(std::abs(val - 1.23) < 1e-10);
    std::cout << "  [PASS] to_matlab<double>\n";
}

void test_to_float() {
    auto arr = mexforge::to_matlab(factory, 9.5f);
    float val = mexforge::from_matlab<float>(arr);
    assert(std::abs(val - 9.5f) < 1e-5f);
    std::cout << "  [PASS] to_matlab<float>\n";
}

void test_to_int32() {
    auto arr = mexforge::to_matlab(factory, int32_t{-7});
    int32_t val = mexforge::from_matlab<int32_t>(arr);
    assert(val == -7);
    std::cout << "  [PASS] to_matlab<int32_t>\n";
}

void test_to_bool() {
    auto arr_t = mexforge::to_matlab(factory, true);
    auto arr_f = mexforge::to_matlab(factory, false);
    assert(mexforge::from_matlab<bool>(arr_t) == true);
    assert(mexforge::from_matlab<bool>(arr_f) == false);
    std::cout << "  [PASS] to_matlab<bool>\n";
}

void test_to_string() {
    auto arr = mexforge::to_matlab(factory, std::string("MexForge"));
    std::string val = mexforge::from_matlab<std::string>(arr);
    assert(val == "MexForge");
    std::cout << "  [PASS] to_matlab<std::string>\n";
}

void test_to_array_passthrough() {
    matlab::data::Array original(99.0);
    auto arr = mexforge::to_matlab(factory, original);
    double val = mexforge::from_matlab<double>(arr);
    assert(std::abs(val - 99.0) < 1e-10);
    std::cout << "  [PASS] to_matlab<Array> passthrough\n";
}

// ---- round-trips ------------------------------------------------------------

void test_roundtrip_double() {
    const double in = 2.718281828;
    auto arr = make_array(in);
    double out = mexforge::from_matlab<double>(arr);
    assert(std::abs(out - in) < 1e-10);
    std::cout << "  [PASS] round-trip double\n";
}

void test_roundtrip_string() {
    const std::string in = "round-trip test";
    auto arr = make_array(in);
    std::string out = mexforge::from_matlab<std::string>(arr);
    assert(out == in);
    std::cout << "  [PASS] round-trip string\n";
}

void test_roundtrip_int32() {
    const int32_t in = -12345;
    auto arr = make_array(in);
    int32_t out = mexforge::from_matlab<int32_t>(arr);
    assert(out == in);
    std::cout << "  [PASS] round-trip int32_t\n";
}

void test_roundtrip_bool() {
    auto arr_t = make_array(true);
    auto arr_f = make_array(false);
    assert(mexforge::from_matlab<bool>(arr_t) == true);
    assert(mexforge::from_matlab<bool>(arr_f) == false);
    std::cout << "  [PASS] round-trip bool\n";
}

// ---- edge cases -------------------------------------------------------------

void test_zero_values() {
    assert(std::abs(mexforge::from_matlab<double>(make_array(0.0))) < 1e-15);
    assert(mexforge::from_matlab<int32_t>(make_array(int32_t{0})) == 0);
    assert(mexforge::from_matlab<bool>(make_array(false)) == false);
    std::cout << "  [PASS] zero values\n";
}

void test_negative_values() {
    assert(mexforge::from_matlab<double>(make_array(-1.5)) < 0.0);
    assert(mexforge::from_matlab<int32_t>(make_array(int32_t{-100})) == -100);
    std::cout << "  [PASS] negative values\n";
}

void test_large_values() {
    const double large = 1e15;
    assert(std::abs(mexforge::from_matlab<double>(make_array(large)) - large) < 1.0);
    std::cout << "  [PASS] large values\n";
}

// ---- std::vector round-trips ------------------------------------------------

void test_vector_double_roundtrip() {
    const std::vector<double> in = {1.0, 2.0, 3.0, 4.0, 5.0};
    auto arr = mexforge::to_matlab(factory, in);
    assert(arr.getNumberOfElements() == 5);
    auto out = mexforge::from_matlab<std::vector<double>>(arr);
    assert(out.size() == in.size());
    for (size_t i = 0; i < in.size(); ++i)
        assert(std::abs(out[i] - in[i]) < 1e-10);
    std::cout << "  [PASS] vector<double> round-trip\n";
}

void test_vector_int32_roundtrip() {
    const std::vector<int32_t> in = {-10, 0, 7, 42, -1};
    auto arr = mexforge::to_matlab(factory, in);
    assert(arr.getNumberOfElements() == 5);
    auto out = mexforge::from_matlab<std::vector<int32_t>>(arr);
    assert(out.size() == in.size());
    for (size_t i = 0; i < in.size(); ++i)
        assert(out[i] == in[i]);
    std::cout << "  [PASS] vector<int32_t> round-trip\n";
}

void test_vector_single_element() {
    const std::vector<double> in = {3.14};
    auto arr = mexforge::to_matlab(factory, in);
    auto out = mexforge::from_matlab<std::vector<double>>(arr);
    assert(out.size() == 1);
    assert(std::abs(out[0] - 3.14) < 1e-10);
    std::cout << "  [PASS] vector single element\n";
}

void test_vector_empty() {
    const std::vector<double> in = {};
    auto arr = mexforge::to_matlab(factory, in);
    assert(arr.getNumberOfElements() == 0);
    auto out = mexforge::from_matlab<std::vector<double>>(arr);
    assert(out.empty());
    std::cout << "  [PASS] vector empty\n";
}

void test_vector_string_roundtrip() {
    const std::vector<std::string> in = {"alpha", "beta", "gamma"};
    auto arr = mexforge::to_matlab(factory, in);
    assert(arr.getNumberOfElements() == 3);
    auto out = mexforge::from_matlab<std::vector<std::string>>(arr);
    assert(out.size() == in.size());
    for (size_t i = 0; i < in.size(); ++i)
        assert(out[i] == in[i]);
    std::cout << "  [PASS] vector<string> round-trip\n";
}

// ---- std::complex round-trips -----------------------------------------------

void test_complex_double_roundtrip() {
    const std::complex<double> in = {3.0, 4.0};
    auto arr = mexforge::to_matlab(factory, in);
    auto out = mexforge::from_matlab<std::complex<double>>(arr);
    assert(std::abs(out.real() - 3.0) < 1e-10);
    assert(std::abs(out.imag() - 4.0) < 1e-10);
    std::cout << "  [PASS] complex<double> round-trip\n";
}

void test_complex_float_roundtrip() {
    const std::complex<float> in = {1.5f, -2.5f};
    auto arr = mexforge::to_matlab(factory, in);
    auto out = mexforge::from_matlab<std::complex<float>>(arr);
    assert(std::abs(out.real() - 1.5f) < 1e-5f);
    assert(std::abs(out.imag() - (-2.5f)) < 1e-5f);
    std::cout << "  [PASS] complex<float> round-trip\n";
}

void test_complex_zero() {
    const std::complex<double> in = {0.0, 0.0};
    auto arr = mexforge::to_matlab(factory, in);
    auto out = mexforge::from_matlab<std::complex<double>>(arr);
    assert(out.real() == 0.0 && out.imag() == 0.0);
    std::cout << "  [PASS] complex zero\n";
}

int main() {
    std::cout << "Marshaller tests:\n";

    test_from_double();
    test_from_float();
    test_from_int32();
    test_from_uint32();
    test_from_int64();
    test_from_bool_true();
    test_from_bool_false();
    test_from_string();
    test_from_string_empty();
    test_from_optional_present();
    test_from_array_passthrough();

    test_to_double();
    test_to_float();
    test_to_int32();
    test_to_bool();
    test_to_string();
    test_to_array_passthrough();

    test_roundtrip_double();
    test_roundtrip_string();
    test_roundtrip_int32();
    test_roundtrip_bool();

    test_zero_values();
    test_negative_values();
    test_large_values();

    test_vector_double_roundtrip();
    test_vector_int32_roundtrip();
    test_vector_single_element();
    test_vector_empty();
    test_vector_string_roundtrip();

    test_complex_double_roundtrip();
    test_complex_float_roundtrip();
    test_complex_zero();

    std::cout << "All marshaller tests passed.\n\n";
    return 0;
}
