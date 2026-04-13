// Tests for MethodTraits — compile-time signature extraction
#include "../include/mexforge/core/method_traits.hpp"
#include <cassert>
#include <iostream>
#include <string>
#include <tuple>
#include <optional>

class MyClass {
public:
    double getValue() const { return 0.0; }
    void setValue(double v) { (void)v; }
    std::string compute(double a, int b) { (void)a; (void)b; return ""; }
    int add(int a, int b) const { return a + b; }
};

double freeFunc(int a, std::string b) { (void)a; (void)b; return 0.0; }

void test_const_method() {
    using T = mexforge::MethodTraits<decltype(&MyClass::getValue)>;
    static_assert(std::is_same_v<T::ReturnType, double>);
    static_assert(std::is_same_v<T::ClassType, MyClass>);
    static_assert(T::arity == 0);
    static_assert(T::is_const);

    std::cout << "  [PASS] const_method\n";
}

void test_non_const_method() {
    using T = mexforge::MethodTraits<decltype(&MyClass::setValue)>;
    static_assert(std::is_same_v<T::ReturnType, void>);
    static_assert(T::arity == 1);
    static_assert(!T::is_const);
    static_assert(std::is_same_v<std::tuple_element_t<0, T::ArgTypes>, double>);

    std::cout << "  [PASS] non_const_method\n";
}

void test_multi_arg_method() {
    using T = mexforge::MethodTraits<decltype(&MyClass::compute)>;
    static_assert(std::is_same_v<T::ReturnType, std::string>);
    static_assert(T::arity == 2);
    static_assert(std::is_same_v<std::tuple_element_t<0, T::ArgTypes>, double>);
    static_assert(std::is_same_v<std::tuple_element_t<1, T::ArgTypes>, int>);

    std::cout << "  [PASS] multi_arg_method\n";
}

void test_free_function() {
    using T = mexforge::MethodTraits<decltype(&freeFunc)>;
    static_assert(std::is_same_v<T::ReturnType, double>);
    static_assert(std::is_same_v<T::ClassType, void>);
    static_assert(T::arity == 2);

    std::cout << "  [PASS] free_function\n";
}

void test_fn_traits_nttp() {
    using T = mexforge::FnTraits<&MyClass::add>;
    static_assert(std::is_same_v<T::ReturnType, int>);
    static_assert(T::arity == 2);
    static_assert(T::is_const);

    std::cout << "  [PASS] fn_traits_nttp\n";
}

void test_lambda() {
    auto lam = [](double x, int y) -> std::string {
        (void)x; (void)y;
        return "";
    };
    using T = mexforge::MethodTraits<decltype(lam)>;
    static_assert(std::is_same_v<T::ReturnType, std::string>);
    static_assert(T::arity == 2);

    std::cout << "  [PASS] lambda\n";
}

void test_arg_type_helper() {
    using Arg0 = mexforge::fn_arg_t<&MyClass::compute, 0>;
    using Arg1 = mexforge::fn_arg_t<&MyClass::compute, 1>;
    static_assert(std::is_same_v<Arg0, double>);
    static_assert(std::is_same_v<Arg1, int>);

    std::cout << "  [PASS] arg_type_helper\n";
}

int main() {
    std::cout << "MethodTraits tests:\n";
    test_const_method();
    test_non_const_method();
    test_multi_arg_method();
    test_free_function();
    test_fn_traits_nttp();
    test_lambda();
    test_arg_type_helper();
    std::cout << "All MethodTraits tests passed.\n\n";
    return 0;
}
