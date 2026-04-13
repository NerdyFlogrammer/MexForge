// Tests for Registry — function registration, lookup, and caching
#include "../include/mexforge/core/registry.hpp"
#include <cassert>
#include <iostream>
#include <string>

// Simple mock runners for testing
class MockRunnerA : public mexforge::RunnerBase {
public:
    static int instanceCount;
    MockRunnerA() { ++instanceCount; }

    void run(
        matlab::mex::ArgumentList,
        matlab::mex::ArgumentList,
        const std::shared_ptr<matlab::engine::MATLABEngine>&,
        mexforge::Logger&) override {}
};
int MockRunnerA::instanceCount = 0;

class MockRunnerB : public mexforge::RunnerBase {
public:
    void run(
        matlab::mex::ArgumentList,
        matlab::mex::ArgumentList,
        const std::shared_ptr<matlab::engine::MATLABEngine>&,
        mexforge::Logger&) override {}
};

void test_add_and_exists() {
    mexforge::Registry reg;

    reg.add("func_a", []() { return std::make_unique<MockRunnerA>(); });
    reg.add("func_b", []() { return std::make_unique<MockRunnerB>(); });

    assert(reg.exists("func_a"));
    assert(reg.exists("func_b"));
    assert(!reg.exists("func_c"));
    assert(reg.size() == 2);

    std::cout << "  [PASS] add_and_exists\n";
}

void test_get_returns_reference() {
    mexforge::Registry reg;
    reg.add("func_a", []() { return std::make_unique<MockRunnerA>(); });

    auto& runner1 = reg.get("func_a");
    auto& runner2 = reg.get("func_a");

    // Same reference — cached, not recreated
    assert(&runner1 == &runner2);

    std::cout << "  [PASS] get_returns_reference\n";
}

void test_caching() {
    MockRunnerA::instanceCount = 0;
    mexforge::Registry reg;

    reg.add("cached", []() { return std::make_unique<MockRunnerA>(); });

    // First call creates the runner
    reg.get("cached");
    assert(MockRunnerA::instanceCount == 1);

    // Subsequent calls reuse the cached instance
    reg.get("cached");
    reg.get("cached");
    reg.get("cached");
    assert(MockRunnerA::instanceCount == 1);

    std::cout << "  [PASS] caching\n";
}

void test_get_unknown_throws() {
    mexforge::Registry reg;

    bool threw = false;
    try {
        reg.get("nonexistent");
    } catch (const std::runtime_error& e) {
        threw = true;
        std::string msg = e.what();
        assert(msg.find("nonexistent") != std::string::npos);
    }
    assert(threw);

    std::cout << "  [PASS] get_unknown_throws\n";
}

void test_list() {
    mexforge::Registry reg;

    reg.add("beta", []() { return std::make_unique<MockRunnerA>(); });
    reg.add("alpha", []() { return std::make_unique<MockRunnerA>(); });
    reg.add("gamma", []() { return std::make_unique<MockRunnerA>(); });

    auto names = reg.list();
    assert(names.size() == 3);

    // All names present (order is unspecified for unordered_map)
    bool hasAlpha = false, hasBeta = false, hasGamma = false;
    for (const auto& n : names) {
        if (n == "alpha") hasAlpha = true;
        if (n == "beta") hasBeta = true;
        if (n == "gamma") hasGamma = true;
    }
    assert(hasAlpha && hasBeta && hasGamma);

    std::cout << "  [PASS] list\n";
}

void test_overwrite() {
    MockRunnerA::instanceCount = 0;
    mexforge::Registry reg;

    reg.add("func", []() { return std::make_unique<MockRunnerA>(); });
    reg.add("func", []() { return std::make_unique<MockRunnerB>(); });

    assert(reg.size() == 1);

    // Should get MockRunnerB, not MockRunnerA
    reg.get("func");
    assert(MockRunnerA::instanceCount == 0);

    std::cout << "  [PASS] overwrite\n";
}

void test_empty_registry() {
    mexforge::Registry reg;

    assert(reg.size() == 0);
    assert(!reg.exists("anything"));
    assert(reg.list().empty());

    std::cout << "  [PASS] empty_registry\n";
}

int main() {
    std::cout << "Registry tests:\n";
    test_add_and_exists();
    test_get_returns_reference();
    test_caching();
    test_get_unknown_throws();
    test_list();
    test_overwrite();
    test_empty_registry();
    std::cout << "All Registry tests passed.\n\n";
    return 0;
}
