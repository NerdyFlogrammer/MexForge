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

// ---- Metadata (.doc()) ------------------------------------------------------

void test_set_and_get_meta() {
    mexforge::Registry reg;
    reg.add("my_func", []() { return std::make_unique<MockRunnerA>(); });

    mexforge::FunctionMeta meta;
    meta.description = "Does something useful";
    meta.return_type = "double";
    meta.needs_object = true;
    meta.args = {{"x", "double", true}, {"y", "double", false}};

    reg.set_meta("my_func", meta);

    assert(reg.has_meta("my_func"));
    const auto& m = reg.get_meta("my_func");
    assert(m.description == "Does something useful");
    assert(m.return_type == "double");
    assert(m.needs_object == true);
    assert(m.args.size() == 2);
    assert(m.args[0].name == "x");
    assert(m.args[0].type == "double");
    assert(m.args[0].required == true);
    assert(m.args[1].name == "y");
    assert(m.args[1].required == false);

    std::cout << "  [PASS] set_and_get_meta\n";
}

void test_no_meta_returns_empty() {
    mexforge::Registry reg;
    reg.add("no_meta", []() { return std::make_unique<MockRunnerA>(); });

    assert(!reg.has_meta("no_meta"));
    const auto& m = reg.get_meta("no_meta");
    assert(m.description.empty());
    assert(m.args.empty());
    assert(m.return_type.empty());

    std::cout << "  [PASS] no_meta_returns_empty\n";
}

void test_list_with_meta() {
    mexforge::Registry reg;
    reg.add("a", []() { return std::make_unique<MockRunnerA>(); });
    reg.add("b", []() { return std::make_unique<MockRunnerA>(); });
    reg.add("c", []() { return std::make_unique<MockRunnerA>(); });

    mexforge::FunctionMeta m;
    m.description = "desc";
    reg.set_meta("a", m);
    reg.set_meta("c", m);

    auto names = reg.list_with_meta();
    assert(names.size() == 2);

    bool hasA = false, hasC = false;
    for (const auto& n : names) {
        if (n == "a") hasA = true;
        if (n == "c") hasC = true;
    }
    assert(hasA && hasC);

    std::cout << "  [PASS] list_with_meta\n";
}

void test_needs_object_flag() {
    mexforge::Registry reg;
    reg.add("obj_func", []() { return std::make_unique<MockRunnerA>(); });
    reg.add("free_func", []() { return std::make_unique<MockRunnerA>(); });

    mexforge::FunctionMeta obj_meta;
    obj_meta.needs_object = true;
    reg.set_meta("obj_func", obj_meta);

    mexforge::FunctionMeta free_meta;
    free_meta.needs_object = false;
    reg.set_meta("free_func", free_meta);

    assert(reg.get_meta("obj_func").needs_object == true);
    assert(reg.get_meta("free_func").needs_object == false);

    std::cout << "  [PASS] needs_object_flag\n";
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
    test_set_and_get_meta();
    test_no_meta_returns_empty();
    test_list_with_meta();
    test_needs_object_flag();
    std::cout << "All Registry tests passed.\n\n";
    return 0;
}
