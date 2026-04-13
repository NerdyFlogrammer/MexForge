// Tests for runners — AutoObjectRunner, LambdaObjectRunner, LambdaFreeRunner, CustomRunner
//
// Each runner is the core dispatch mechanism: it takes a MATLAB ArgumentList,
// unmarshals types, calls the C++ function, and writes the result back.
// These tests verify the full dispatch loop using mock MATLAB types.

#include "../include/mexforge/core/runner.hpp"
#include <cassert>
#include <cmath>
#include <iostream>
#include <optional>
#include <string>

// ---- Simple test class -------------------------------------------------------

class Counter {
public:
    explicit Counter(int32_t start = 0) : value_(start) {}

    int32_t get() const { return value_; }
    void set(int32_t v) { value_ = v; }
    int32_t add(int32_t a, int32_t b) const { return a + b; }
    double scale(double x) const { return x * value_; }
    std::string name(std::string prefix) const { return prefix + "_counter"; }
    void reset() { value_ = 0; }
    double addOptional(double a, std::optional<double> b) const {
        return b.has_value() ? a + *b : a;
    }

private:
    int32_t value_;
};

// ---- helpers -----------------------------------------------------------------

static auto make_engine() {
    return std::make_shared<matlab::engine::MATLABEngine>();
}

// Logger is not copyable (owns an ofstream) — allocate on heap and pass by pointer
static std::unique_ptr<mexforge::Logger> make_logger(
    std::shared_ptr<matlab::engine::MATLABEngine> engine) {
    auto logger = std::make_unique<mexforge::Logger>(engine);
    logger->setLevel(mexforge::LogLevel::Off);
    return logger;
}

// Build ArgumentList: [func_name, obj_id, args...]
template<typename... Ts>
static matlab::mex::ArgumentList make_inputs(uint32_t objId, Ts... vals) {
    matlab::data::ArrayFactory factory;
    std::vector<matlab::data::Array> args;
    args.push_back(factory.createScalar(std::string("test_func")));
    args.push_back(factory.createScalar(objId));
    (args.push_back(factory.createScalar(vals)), ...);
    return matlab::mex::ArgumentList(std::move(args));
}

// Build ArgumentList for free runners: [func_name, args...]
template<typename... Ts>
static matlab::mex::ArgumentList make_free_inputs(Ts... vals) {
    matlab::data::ArrayFactory factory;
    std::vector<matlab::data::Array> args;
    args.push_back(factory.createScalar(std::string("test_func")));
    (args.push_back(factory.createScalar(vals)), ...);
    return matlab::mex::ArgumentList(std::move(args));
}

static matlab::mex::ArgumentList make_single_output() {
    std::vector<matlab::data::Array> out(1);
    return matlab::mex::ArgumentList(std::move(out));
}

static matlab::mex::ArgumentList make_no_output() {
    return matlab::mex::ArgumentList{};
}

// ---- AutoObjectRunner -------------------------------------------------------

void test_auto_add() {
    mexforge::ObjectStore<Counter> store;
    uint32_t id = static_cast<uint32_t>(store.emplace(0));

    mexforge::AutoObjectRunner<Counter, &Counter::add> runner(store);

    auto engine = make_engine();
    auto logger = make_logger(engine);
    auto inputs = make_inputs(id, int32_t{3}, int32_t{7});
    auto outputs = make_single_output();

    runner.run(outputs, inputs, engine, *logger);

    int32_t result = mexforge::from_matlab<int32_t>(outputs[0]);
    assert(result == 10);
    std::cout << "  [PASS] AutoObjectRunner: add(3, 7) == 10\n";
}

void test_auto_get() {
    mexforge::ObjectStore<Counter> store;
    uint32_t id = static_cast<uint32_t>(store.emplace(42));

    mexforge::AutoObjectRunner<Counter, &Counter::get> runner(store);

    auto engine = make_engine();
    auto logger = make_logger(engine);
    auto inputs = make_inputs(id);
    auto outputs = make_single_output();

    runner.run(outputs, inputs, engine, *logger);

    int32_t result = mexforge::from_matlab<int32_t>(outputs[0]);
    assert(result == 42);
    std::cout << "  [PASS] AutoObjectRunner: get() == 42\n";
}

void test_auto_void_method() {
    mexforge::ObjectStore<Counter> store;
    uint32_t id = static_cast<uint32_t>(store.emplace(99));

    mexforge::AutoObjectRunner<Counter, &Counter::reset> runner(store);

    auto engine = make_engine();
    auto logger = make_logger(engine);
    auto inputs = make_inputs(id);
    auto outputs = make_no_output();

    runner.run(outputs, inputs, engine, *logger);

    assert(store.get_ref(id).get() == 0);
    std::cout << "  [PASS] AutoObjectRunner: reset() sets value to 0\n";
}

void test_auto_set_and_get() {
    mexforge::ObjectStore<Counter> store;
    uint32_t id = static_cast<uint32_t>(store.emplace(0));

    mexforge::AutoObjectRunner<Counter, &Counter::set> setter(store);
    mexforge::AutoObjectRunner<Counter, &Counter::get> getter(store);

    auto engine = make_engine();
    auto logger = make_logger(engine);

    auto set_inputs = make_inputs(id, int32_t{77});
    auto no_out = make_no_output();
    setter.run(no_out, set_inputs, engine, *logger);

    auto get_inputs = make_inputs(id);
    auto outputs = make_single_output();
    getter.run(outputs, get_inputs, engine, *logger);

    int32_t result = mexforge::from_matlab<int32_t>(outputs[0]);
    assert(result == 77);
    std::cout << "  [PASS] AutoObjectRunner: set(77) then get() == 77\n";
}

void test_auto_string_return() {
    mexforge::ObjectStore<Counter> store;
    uint32_t id = static_cast<uint32_t>(store.emplace(0));

    mexforge::AutoObjectRunner<Counter, &Counter::name> runner(store);

    auto engine = make_engine();
    auto logger = make_logger(engine);
    matlab::data::ArrayFactory factory;
    std::vector<matlab::data::Array> args;
    args.push_back(factory.createScalar(std::string("test_func")));
    args.push_back(factory.createScalar(uint32_t{id}));
    args.push_back(factory.createScalar(std::string("my")));
    auto inputs = matlab::mex::ArgumentList(std::move(args));
    auto outputs = make_single_output();

    runner.run(outputs, inputs, engine, *logger);

    std::string result = mexforge::from_matlab<std::string>(outputs[0]);
    assert(result == "my_counter");
    std::cout << "  [PASS] AutoObjectRunner: name('my') == 'my_counter'\n";
}

// ---- LambdaObjectRunner -----------------------------------------------------

void test_lambda_multiply() {
    mexforge::ObjectStore<Counter> store;
    uint32_t id = static_cast<uint32_t>(store.emplace(5)); // Counter value = 5

    auto lambda = [](Counter& c, double x) -> double { return c.scale(x); };

    mexforge::LambdaObjectRunner<Counter, decltype(lambda), double> runner(store, lambda);

    auto engine = make_engine();
    auto logger = make_logger(engine);
    auto inputs = make_inputs(id, 3.0);
    auto outputs = make_single_output();

    runner.run(outputs, inputs, engine, *logger);

    double result = mexforge::from_matlab<double>(outputs[0]);
    assert(std::abs(result - 15.0) < 1e-10); // 5 * 3 = 15
    std::cout << "  [PASS] LambdaObjectRunner: scale(3.0) with value=5 == 15.0\n";
}

void test_lambda_custom_logic() {
    mexforge::ObjectStore<Counter> store;
    uint32_t id = static_cast<uint32_t>(store.emplace(10));

    // Lambda that combines get() + argument
    auto lambda = [](Counter& c, int32_t delta) -> int32_t {
        return static_cast<int32_t>(c.get()) + delta;
    };

    mexforge::LambdaObjectRunner<Counter, decltype(lambda), int32_t> runner(store, lambda);

    auto engine = make_engine();
    auto logger = make_logger(engine);
    auto inputs = make_inputs(id, int32_t{5});
    auto outputs = make_single_output();

    runner.run(outputs, inputs, engine, *logger);

    int32_t result = mexforge::from_matlab<int32_t>(outputs[0]);
    assert(result == 15); // 10 + 5
    std::cout << "  [PASS] LambdaObjectRunner: get() + 5 == 15\n";
}

// ---- LambdaFreeRunner -------------------------------------------------------

void test_lambda_free_add() {
    auto lambda = [](double a, double b) -> double { return a + b; };

    mexforge::LambdaFreeRunner<decltype(lambda), double, double> runner(lambda);

    auto engine = make_engine();
    auto logger = make_logger(engine);
    auto inputs = make_free_inputs(4.0, 6.0);
    auto outputs = make_single_output();

    runner.run(outputs, inputs, engine, *logger);

    double result = mexforge::from_matlab<double>(outputs[0]);
    assert(std::abs(result - 10.0) < 1e-10);
    std::cout << "  [PASS] LambdaFreeRunner: add(4.0, 6.0) == 10.0\n";
}

void test_lambda_free_string() {
    auto lambda = [](std::string prefix, std::string suffix) -> std::string {
        return prefix + suffix;
    };

    mexforge::LambdaFreeRunner<decltype(lambda), std::string, std::string> runner(lambda);

    auto engine = make_engine();
    auto logger = make_logger(engine);
    matlab::data::ArrayFactory factory;
    std::vector<matlab::data::Array> args;
    args.push_back(factory.createScalar(std::string("func")));
    args.push_back(factory.createScalar(std::string("Mex")));
    args.push_back(factory.createScalar(std::string("Forge")));
    auto inputs = matlab::mex::ArgumentList(std::move(args));
    auto outputs = make_single_output();

    runner.run(outputs, inputs, engine, *logger);

    std::string result = mexforge::from_matlab<std::string>(outputs[0]);
    assert(result == "MexForge");
    std::cout << "  [PASS] LambdaFreeRunner: concat('Mex', 'Forge') == 'MexForge'\n";
}

void test_lambda_free_returns_uint32() {
    auto lambda = [](uint32_t x) -> uint32_t { return x * 2; };

    mexforge::LambdaFreeRunner<decltype(lambda), uint32_t> runner(lambda);

    auto engine = make_engine();
    auto logger = make_logger(engine);
    auto inputs = make_free_inputs(uint32_t{21});
    auto outputs = make_single_output();

    runner.run(outputs, inputs, engine, *logger);

    uint32_t result = mexforge::from_matlab<uint32_t>(outputs[0]);
    assert(result == 42u);
    std::cout << "  [PASS] LambdaFreeRunner: uint32 * 2 == 42\n";
}

// ---- CustomRunner -----------------------------------------------------------

class SumRunner : public mexforge::CustomRunner<Counter> {
public:
    using CustomRunner::CustomRunner;

    void execute(Counter& counter, matlab::mex::ArgumentList outputs,
                 matlab::mex::ArgumentList inputs, matlab::data::ArrayFactory& factory,
                 mexforge::Logger& /*logger*/) override {
        double extra = mexforge::from_matlab<double>(inputs[0]);
        double result = counter.get() + extra;
        outputs[0] = factory.createScalar(result);
    }
};

void test_custom_runner() {
    mexforge::ObjectStore<Counter> store;
    uint32_t id = static_cast<uint32_t>(store.emplace(100));

    SumRunner runner(store);

    auto engine = make_engine();
    auto logger = make_logger(engine);
    auto inputs = make_inputs(id, 23.0);
    auto outputs = make_single_output();

    runner.run(outputs, inputs, engine, *logger);

    double result = mexforge::from_matlab<double>(outputs[0]);
    assert(std::abs(result - 123.0) < 1e-10); // 100 + 23
    std::cout << "  [PASS] CustomRunner: get() + 23 == 123.0\n";
}

// ---- optional args ----------------------------------------------------------

void test_optional_arg_present() {
    mexforge::ObjectStore<Counter> store;
    uint32_t id = static_cast<uint32_t>(store.emplace(0));

    mexforge::AutoObjectRunner<Counter, &Counter::addOptional> runner(store);

    auto engine = make_engine();
    auto logger = make_logger(engine);
    auto inputs = make_inputs(id, 10.0, 5.0);
    auto outputs = make_single_output();

    runner.run(outputs, inputs, engine, *logger);

    double result = mexforge::from_matlab<double>(outputs[0]);
    assert(std::abs(result - 15.0) < 1e-10);
    std::cout << "  [PASS] AutoObjectRunner: optional arg present -> 10 + 5 == 15\n";
}

void test_optional_arg_absent() {
    mexforge::ObjectStore<Counter> store;
    uint32_t id = static_cast<uint32_t>(store.emplace(0));

    mexforge::AutoObjectRunner<Counter, &Counter::addOptional> runner(store);

    auto engine = make_engine();
    auto logger = make_logger(engine);
    auto inputs = make_inputs(id, 10.0); // Only required arg
    auto outputs = make_single_output();

    runner.run(outputs, inputs, engine, *logger);

    double result = mexforge::from_matlab<double>(outputs[0]);
    assert(std::abs(result - 10.0) < 1e-10);
    std::cout << "  [PASS] AutoObjectRunner: optional arg absent -> 10.0\n";
}

// ---- multiple objects in store ----------------------------------------------

void test_multiple_objects() {
    mexforge::ObjectStore<Counter> store;
    uint32_t id0 = static_cast<uint32_t>(store.emplace(10));
    uint32_t id1 = static_cast<uint32_t>(store.emplace(20));
    uint32_t id2 = static_cast<uint32_t>(store.emplace(30));

    mexforge::AutoObjectRunner<Counter, &Counter::get> runner(store);

    auto engine = make_engine();
    auto logger = make_logger(engine);

    for (auto [id, expected] : std::initializer_list<std::pair<uint32_t, int>>{
             {id0, 10}, {id1, 20}, {id2, 30}}) {
        auto inputs = make_inputs(id);
        auto outputs = make_single_output();
        runner.run(outputs, inputs, engine, *logger);
        int32_t result = mexforge::from_matlab<int32_t>(outputs[0]);
        assert(result == expected);
    }

    std::cout << "  [PASS] AutoObjectRunner: multiple objects isolated correctly\n";
}

// ---- AutoFreeRunner (Tier 1b) -----------------------------------------------

static double free_square(double x) { return x * x; }
static std::string free_greet(std::string name) { return "hello_" + name; }
static void free_noop() {}

void test_auto_free_runner_scalar() {
    mexforge::AutoFreeRunner<&free_square> runner;

    auto engine = make_engine();
    auto logger = make_logger(engine);
    auto inputs = make_free_inputs(5.0);
    auto outputs = make_single_output();

    runner.run(outputs, inputs, engine, *logger);

    double result = mexforge::from_matlab<double>(outputs[0]);
    assert(std::abs(result - 25.0) < 1e-10);
    std::cout << "  [PASS] AutoFreeRunner: square(5.0) == 25.0\n";
}

void test_auto_free_runner_string() {
    mexforge::AutoFreeRunner<&free_greet> runner;

    auto engine = make_engine();
    auto logger = make_logger(engine);
    matlab::data::ArrayFactory factory;
    std::vector<matlab::data::Array> args;
    args.push_back(factory.createScalar(std::string("func")));
    args.push_back(factory.createScalar(std::string("world")));
    auto inputs = matlab::mex::ArgumentList(std::move(args));
    auto outputs = make_single_output();

    runner.run(outputs, inputs, engine, *logger);

    std::string result = mexforge::from_matlab<std::string>(outputs[0]);
    assert(result == "hello_world");
    std::cout << "  [PASS] AutoFreeRunner: greet('world') == 'hello_world'\n";
}

void test_auto_free_runner_void() {
    mexforge::AutoFreeRunner<&free_noop> runner;

    auto engine = make_engine();
    auto logger = make_logger(engine);
    auto inputs = make_free_inputs();  // no args
    auto outputs = make_no_output();

    // Should not crash
    runner.run(outputs, inputs, engine, *logger);
    std::cout << "  [PASS] AutoFreeRunner: void function no crash\n";
}

// ---- CustomRunner with vector I/O -------------------------------------------

class VectorSumRunner : public mexforge::CustomRunner<Counter> {
public:
    using CustomRunner::CustomRunner;

    void execute(Counter& counter, matlab::mex::ArgumentList outputs,
                 matlab::mex::ArgumentList inputs, matlab::data::ArrayFactory& factory,
                 mexforge::Logger& /*logger*/) override {
        auto data = mexforge::from_matlab<std::vector<double>>(inputs[0]);
        double sum = 0.0;
        for (double v : data)
            sum += v;
        sum += counter.get(); // Add object state
        outputs[0] = factory.createScalar(sum);
    }
};

void test_custom_runner_vector_input() {
    mexforge::ObjectStore<Counter> store;
    uint32_t id = static_cast<uint32_t>(store.emplace(int32_t{10}));

    VectorSumRunner runner(store);

    auto engine = make_engine();
    auto logger = make_logger(engine);

    // Build inputs: [func_name, obj_id, vector_array]
    matlab::data::ArrayFactory factory;
    std::vector<matlab::data::Array> args;
    args.push_back(factory.createScalar(std::string("vec_sum")));
    args.push_back(factory.createScalar(id));
    std::vector<double> data = {1.0, 2.0, 3.0, 4.0};
    auto vec_arr = mexforge::to_matlab(factory, data);
    args.push_back(vec_arr);
    auto inputs = matlab::mex::ArgumentList(std::move(args));
    auto outputs = make_single_output();

    runner.run(outputs, inputs, engine, *logger);

    double result = mexforge::from_matlab<double>(outputs[0]);
    assert(std::abs(result - 20.0) < 1e-10); // 1+2+3+4 + 10 = 20
    std::cout << "  [PASS] CustomRunner: vector sum + object state == 20\n";
}

// ---- Error paths ------------------------------------------------------------

void test_wrong_arg_count_recorded() {
    mexforge::ObjectStore<Counter> store;
    uint32_t id = static_cast<uint32_t>(store.emplace(0));

    mexforge::AutoObjectRunner<Counter, &Counter::add> runner(store);

    auto engine = make_engine();
    auto logger = make_logger(engine);

    // add(a, b) requires 2 args — pass only 1
    auto inputs = make_inputs(id, int32_t{5}); // only one arg
    auto outputs = make_no_output();

    runner.run(outputs, inputs, engine, *logger);

    // Mock engine should have recorded an error feval call
    assert(!engine->calls.empty());
    std::cout << "  [PASS] wrong arg count -> engine error recorded\n";
}

void test_invalid_object_id_throws() {
    mexforge::ObjectStore<Counter> store;
    // Do NOT emplace anything

    mexforge::AutoObjectRunner<Counter, &Counter::get> runner(store);

    auto engine = make_engine();
    auto logger = make_logger(engine);
    auto inputs = make_inputs(uint32_t{99}); // non-existent ID
    auto outputs = make_single_output();

    bool threw = false;
    try {
        runner.run(outputs, inputs, engine, *logger);
    } catch (const std::runtime_error&) {
        threw = true;
    }
    assert(threw);
    std::cout << "  [PASS] invalid object ID throws runtime_error\n";
}

int main() {
    std::cout << "Runner tests:\n";

    test_auto_add();
    test_auto_get();
    test_auto_void_method();
    test_auto_set_and_get();
    test_auto_string_return();

    test_lambda_multiply();
    test_lambda_custom_logic();

    test_lambda_free_add();
    test_lambda_free_string();
    test_lambda_free_returns_uint32();

    test_custom_runner();

    test_optional_arg_present();
    test_optional_arg_absent();

    test_multiple_objects();

    test_auto_free_runner_scalar();
    test_auto_free_runner_string();
    test_auto_free_runner_void();

    test_custom_runner_vector_input();

    test_wrong_arg_count_recorded();
    test_invalid_object_id_throws();

    std::cout << "All runner tests passed.\n\n";
    return 0;
}
