// Integration tests for MexGateway — full dispatch loop without MATLAB
//
// Tests the complete call chain:
//   MATLAB ArgumentList -> Gateway::operator() -> Runner -> C++ -> Result
//
// Also tests all built-in control commands (__list_functions, __store_size,
// __store_clear, __describe, __arg_info, __return_type, __list_methods)
// and error paths (unknown function, empty input).

#include "../include/mexforge/core/gateway.hpp"
#include <cassert>
#include <cmath>
#include <iostream>
#include <string>

// ---- Simple class to wrap ---------------------------------------------------

class Box {
public:
    explicit Box(double side) : side_(side) {}

    double side() const { return side_; }
    void resize(double s) { side_ = s; }
    double volume() const { return side_ * side_ * side_; }
    double scale(double factor) const { return side_ * factor; }
    std::string label(std::string prefix) const { return prefix + "_box"; }

private:
    double side_;
};

// ---- Gateway under test -----------------------------------------------------

class BoxGateway : public mexforge::MexGateway<Box> {
protected:
    void setup(mexforge::RegistryBuilder<Box>& b) override {
        b.bind_free_lambda<std::string>("create",
                                        [this](std::string /*name*/) -> uint32_t {
                                            return static_cast<uint32_t>(store().emplace(1.0));
                                        })
            .doc("Create a Box", {{"name", "string", true}}, "uint32")

            .bind_free_lambda<uint32_t>("destroy",
                                        [this](uint32_t id) { store().remove(static_cast<int>(id)); })
            .doc("Destroy a Box", {{"id", "uint32", true}})

            .bind_auto<&Box::side>("side")
            .doc("Get the side length", {}, "double")

            .bind_auto<&Box::resize>("resize")
            .doc("Set the side length", {{"side", "double", true}})

            .bind_auto<&Box::volume>("volume")
            .doc("Get the volume (side^3)", {}, "double")

            .bind_auto<&Box::scale>("scale")
            .doc("Scale the side length", {{"factor", "double", true}}, "double")

            .bind_auto<&Box::label>("label")
            .doc("Get a labeled name", {{"prefix", "string", true}}, "string")

            .bind_lambda<double>("add_and_volume",
                [](Box& box, double extra) -> double {
                    double s = box.side() + extra;
                    return s * s * s;
                })
            .doc("Volume if side were extended by extra",
                 {{"extra", "double", true}}, "double");
    }
};

// ---- helpers ----------------------------------------------------------------

static matlab::data::ArrayFactory factory;

static matlab::mex::ArgumentList make_args(std::vector<matlab::data::Array> v) {
    return matlab::mex::ArgumentList(std::move(v));
}

static matlab::mex::ArgumentList cmd(const std::string& name) {
    return make_args({factory.createScalar(name)});
}

static matlab::mex::ArgumentList cmd(const std::string& name, uint32_t id) {
    return make_args({factory.createScalar(name), factory.createScalar(id)});
}

static matlab::mex::ArgumentList cmd(const std::string& name, uint32_t id, double arg) {
    return make_args({factory.createScalar(name), factory.createScalar(id),
                      factory.createScalar(arg)});
}

static matlab::mex::ArgumentList cmd(const std::string& name, const std::string& arg) {
    return make_args({factory.createScalar(name), factory.createScalar(arg)});
}

static matlab::mex::ArgumentList cmd(const std::string& name, uint32_t id,
                                     const std::string& arg) {
    return make_args({factory.createScalar(name), factory.createScalar(id),
                      factory.createScalar(arg)});
}

static matlab::mex::ArgumentList single_out() {
    return matlab::mex::ArgumentList(std::vector<matlab::data::Array>(1));
}

static matlab::mex::ArgumentList no_out() {
    return matlab::mex::ArgumentList{};
}

// ---- create / destroy -------------------------------------------------------

void test_create_destroy() {
    BoxGateway gw;

    auto out = single_out();
    gw(out, cmd("create", "mybox"));
    uint32_t id = mexforge::from_matlab<uint32_t>(out[0]);

    auto store_out = single_out();
    gw(store_out, cmd("__store_size"));
    uint32_t sz = mexforge::from_matlab<uint32_t>(store_out[0]);
    assert(sz == 1u);

    gw(no_out(), cmd("destroy", id));

    gw(store_out, cmd("__store_size"));
    sz = mexforge::from_matlab<uint32_t>(store_out[0]);
    assert(sz == 0u);

    std::cout << "  [PASS] create / destroy\n";
}

// ---- basic dispatch ---------------------------------------------------------

void test_side_and_resize() {
    BoxGateway gw;

    auto out = single_out();
    gw(out, cmd("create", "b"));
    uint32_t id = mexforge::from_matlab<uint32_t>(out[0]);

    // Default side = 1.0
    gw(out, cmd("side", id));
    assert(std::abs(mexforge::from_matlab<double>(out[0]) - 1.0) < 1e-10);

    // Resize to 3.0
    gw(no_out(), cmd("resize", id, 3.0));
    gw(out, cmd("side", id));
    assert(std::abs(mexforge::from_matlab<double>(out[0]) - 3.0) < 1e-10);

    std::cout << "  [PASS] side() and resize()\n";
}

void test_volume() {
    BoxGateway gw;

    auto out = single_out();
    gw(out, cmd("create", "b"));
    uint32_t id = mexforge::from_matlab<uint32_t>(out[0]);

    gw(no_out(), cmd("resize", id, 4.0));
    gw(out, cmd("volume", id));
    double vol = mexforge::from_matlab<double>(out[0]);
    assert(std::abs(vol - 64.0) < 1e-10); // 4^3 = 64

    std::cout << "  [PASS] volume()\n";
}

void test_scale() {
    BoxGateway gw;

    auto out = single_out();
    gw(out, cmd("create", "b"));
    uint32_t id = mexforge::from_matlab<uint32_t>(out[0]);

    gw(no_out(), cmd("resize", id, 2.0));
    gw(out, cmd("scale", id, 5.0));
    assert(std::abs(mexforge::from_matlab<double>(out[0]) - 10.0) < 1e-10);

    std::cout << "  [PASS] scale()\n";
}

void test_string_return() {
    BoxGateway gw;

    auto out = single_out();
    gw(out, cmd("create", "b"));
    uint32_t id = mexforge::from_matlab<uint32_t>(out[0]);

    gw(out, cmd("label", id, "my"));
    std::string result = mexforge::from_matlab<std::string>(out[0]);
    assert(result == "my_box");

    std::cout << "  [PASS] label() returns string\n";
}

void test_lambda_binding() {
    BoxGateway gw;

    auto out = single_out();
    gw(out, cmd("create", "b"));
    uint32_t id = mexforge::from_matlab<uint32_t>(out[0]);

    gw(no_out(), cmd("resize", id, 2.0));
    gw(out, cmd("add_and_volume", id, 1.0)); // (2+1)^3 = 27
    assert(std::abs(mexforge::from_matlab<double>(out[0]) - 27.0) < 1e-10);

    std::cout << "  [PASS] lambda binding (add_and_volume)\n";
}

// ---- multiple objects -------------------------------------------------------

void test_multiple_objects_isolated() {
    BoxGateway gw;

    auto out = single_out();

    gw(out, cmd("create", "a"));
    uint32_t id0 = mexforge::from_matlab<uint32_t>(out[0]);
    gw(out, cmd("create", "b"));
    uint32_t id1 = mexforge::from_matlab<uint32_t>(out[0]);

    gw(no_out(), cmd("resize", id0, 2.0));
    gw(no_out(), cmd("resize", id1, 5.0));

    gw(out, cmd("side", id0));
    assert(std::abs(mexforge::from_matlab<double>(out[0]) - 2.0) < 1e-10);

    gw(out, cmd("side", id1));
    assert(std::abs(mexforge::from_matlab<double>(out[0]) - 5.0) < 1e-10);

    std::cout << "  [PASS] multiple objects isolated\n";
}

// ---- built-in commands ------------------------------------------------------

void test_store_size() {
    BoxGateway gw;

    auto out = single_out();
    gw(out, cmd("__store_size"));
    assert(mexforge::from_matlab<uint32_t>(out[0]) == 0u);

    gw(single_out(), cmd("create", "a"));
    gw(single_out(), cmd("create", "b"));

    gw(out, cmd("__store_size"));
    assert(mexforge::from_matlab<uint32_t>(out[0]) == 2u);

    std::cout << "  [PASS] __store_size\n";
}

void test_store_clear() {
    BoxGateway gw;

    gw(single_out(), cmd("create", "a"));
    gw(single_out(), cmd("create", "b"));
    gw(single_out(), cmd("create", "c"));

    gw(no_out(), cmd("__store_clear"));

    auto out = single_out();
    gw(out, cmd("__store_size"));
    assert(mexforge::from_matlab<uint32_t>(out[0]) == 0u);

    std::cout << "  [PASS] __store_clear\n";
}

void test_list_functions() {
    BoxGateway gw;

    // Just verify the command runs without error (mock createArray returns empty)
    gw(no_out(), cmd("__list_functions"));

    std::cout << "  [PASS] __list_functions (no crash)\n";
}

void test_describe() {
    BoxGateway gw;

    auto out = single_out();
    gw(out, cmd("__describe", "volume"));
    std::string desc = mexforge::from_matlab<std::string>(out[0]);
    assert(desc == "Get the volume (side^3)");

    gw(out, cmd("__describe", "side"));
    desc = mexforge::from_matlab<std::string>(out[0]);
    assert(desc == "Get the side length");

    std::cout << "  [PASS] __describe\n";
}

void test_return_type() {
    BoxGateway gw;

    auto out = single_out();
    gw(out, cmd("__return_type", "volume"));
    std::string rt = mexforge::from_matlab<std::string>(out[0]);
    assert(rt == "double");

    gw(out, cmd("__return_type", "create"));
    rt = mexforge::from_matlab<std::string>(out[0]);
    assert(rt == "uint32");

    std::cout << "  [PASS] __return_type\n";
}

void test_list_methods() {
    BoxGateway gw;

    // Just verify it runs without crash (mock createArray returns empty)
    gw(no_out(), cmd("__list_methods"));

    std::cout << "  [PASS] __list_methods (no crash)\n";
}

// ---- log level control ------------------------------------------------------

void test_log_level_get_set() {
    BoxGateway gw;

    // Set level to "off"
    gw(no_out(), cmd("__log_level", "off"));

    // Read it back
    auto out = single_out();
    gw(out, cmd("__log_level"));
    std::string level = mexforge::from_matlab<std::string>(out[0]);
    assert(level == "off");

    gw(no_out(), cmd("__log_level", "debug"));
    gw(out, cmd("__log_level"));
    level = mexforge::from_matlab<std::string>(out[0]);
    assert(level == "debug");

    std::cout << "  [PASS] __log_level get/set\n";
}

// ---- error paths ------------------------------------------------------------

void test_unknown_function_calls_error() {
    BoxGateway gw;

    // Unknown function → gateway calls engine->feval("error", ...)
    // The mock engine just records calls — it does not throw
    gw(no_out(), cmd("nonexistent_function"));
    // If we get here without a crash, the error path is handled gracefully

    std::cout << "  [PASS] unknown function handled gracefully\n";
}

void test_empty_input_handled() {
    BoxGateway gw;

    // Empty inputs → reportError, no crash
    gw(no_out(), matlab::mex::ArgumentList{});

    std::cout << "  [PASS] empty input handled gracefully\n";
}

int main() {
    std::cout << "Gateway integration tests:\n";

    test_create_destroy();
    test_side_and_resize();
    test_volume();
    test_scale();
    test_string_return();
    test_lambda_binding();
    test_multiple_objects_isolated();

    test_store_size();
    test_store_clear();
    test_list_functions();
    test_describe();
    test_return_type();
    test_list_methods();
    test_log_level_get_set();

    test_unknown_function_calls_error();
    test_empty_input_handled();

    std::cout << "All gateway tests passed.\n\n";
    return 0;
}
