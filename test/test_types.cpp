// Tests for type traits — compile-time checks
#include "../include/mexforge/core/types.hpp"
#include <cassert>
#include <iostream>
#include <optional>
#include <vector>

void test_optional_detection() {
    static_assert(!mexforge::is_optional_v<double>);
    static_assert(!mexforge::is_optional_v<int>);
    static_assert(!mexforge::is_optional_v<std::string>);
    static_assert(mexforge::is_optional_v<std::optional<double>>);
    static_assert(mexforge::is_optional_v<std::optional<int>>);
    static_assert(mexforge::is_optional_v<std::optional<std::string>>);

    std::cout << "  [PASS] optional_detection\n";
}

void test_unwrap_optional() {
    static_assert(std::is_same_v<mexforge::unwrap_optional_t<double>, double>);
    static_assert(std::is_same_v<mexforge::unwrap_optional_t<std::optional<double>>, double>);
    static_assert(std::is_same_v<mexforge::unwrap_optional_t<std::optional<std::string>>, std::string>);

    std::cout << "  [PASS] unwrap_optional\n";
}

void test_vector_detection() {
    static_assert(!mexforge::is_vector_v<double>);
    static_assert(!mexforge::is_vector_v<std::string>);
    static_assert(mexforge::is_vector_v<std::vector<double>>);
    static_assert(mexforge::is_vector_v<std::vector<int>>);

    std::cout << "  [PASS] vector_detection\n";
}

void test_arg_list_count() {
    using A1 = mexforge::ArgList<double, int, std::string>;
    static_assert(A1::total == 3);
    static_assert(A1::required_count() == 3);

    using A2 = mexforge::ArgList<double, std::optional<int>, std::optional<std::string>>;
    static_assert(A2::total == 3);
    static_assert(A2::required_count() == 1);

    using A3 = mexforge::ArgList<>;
    static_assert(A3::total == 0);
    static_assert(A3::required_count() == 0);

    std::cout << "  [PASS] arg_list_count\n";
}

int main() {
    std::cout << "Type traits tests:\n";
    test_optional_detection();
    test_unwrap_optional();
    test_vector_detection();
    test_arg_list_count();
    std::cout << "All type traits tests passed.\n\n";
    return 0;
}
