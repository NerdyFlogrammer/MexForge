// ============================================================================
// Example: MexForge bindings for the Calculator class
//
// This shows all 3 binding tiers in action, with metadata for
// auto-generated MATLAB wrappers, help texts, and type checking.
//
// Build with: mex -I../../include bindings.cpp
// ============================================================================

#include "math_lib.hpp"

#include <mexforge/mexforge.hpp>

// ============================================================================
// Tier 3 example: Custom runner for batch operations
// Full control over argument marshalling.
// ============================================================================

class ApplyRunner : public mexforge::CustomRunner<Calculator> {
public:
    using CustomRunner::CustomRunner; // Inherit constructor

    void execute(Calculator& calc, matlab::mex::ArgumentList outputs,
                 matlab::mex::ArgumentList inputs, matlab::data::ArrayFactory& factory,
                 mexforge::Logger& logger) override {
        auto data = mexforge::from_matlab<std::vector<double>>(inputs[0]);
        auto func = mexforge::from_matlab<std::string>(inputs[1]);

        logger.debug("ApplyRunner: applying '", func, "' to ", std::to_string(data.size()),
                     " elements");

        auto result = calc.apply(data, func);

        outputs[0] = factory.createArray({1, result.size()}, result.begin(), result.end());
    }
};

// ============================================================================
// Gateway: Wire it all together
// ============================================================================

class CalcMex : public mexforge::MexGateway<Calculator> {
protected:
    void setup(mexforge::RegistryBuilder<Calculator>& b) override {
        // --- Lifecycle ---
        b.bind_free_lambda<std::string>("create",
                                        [this](std::string name) -> uint32_t {
                                            return static_cast<uint32_t>(store().emplace(name));
                                        })
            .doc("Create a new Calculator instance", {{"name", "string", true}}, "uint32")

            .bind_free_lambda<uint32_t>(
                "destroy", [this](uint32_t id) { store().remove(static_cast<int>(id)); })
            .doc("Destroy a Calculator instance", {{"id", "uint32", true}})

            // --- Tier 1: Auto-bind ---
            .bind_auto<&Calculator::getName>("get_name")
            .doc("Get the calculator name", {}, "string")

            .bind_auto<&Calculator::getMemory>("get_memory")
            .doc("Get the stored memory value", {}, "double")

            .bind_auto<&Calculator::setMemory>("set_memory")
            .doc("Set the memory value", {{"value", "double", true}})

            .bind_auto<&Calculator::clearMemory>("clear_memory")
            .doc("Clear memory to zero")

            .bind_auto<&Calculator::add>("add")
            .doc("Add two numbers", {{"a", "double", true}, {"b", "double", true}}, "double")

            .bind_auto<&Calculator::multiply>("multiply")
            .doc("Multiply two numbers", {{"a", "double", true}, {"b", "double", true}}, "double")

            .bind_auto<&Calculator::power>("power")
            .doc("Raise base to exponent", {{"base", "double", true}, {"exponent", "double", true}},
                 "double")

            // --- Tier 2: Lambda ---
            .bind_lambda<double, double, std::string>(
                "compute",
                [](Calculator& calc, double a, double b, std::string op) -> double {
                    return calc.compute(a, b, op);
                })
            .doc("Compute a basic operation (add, sub, mul, div)",
                 {{"a", "double", true}, {"b", "double", true}, {"operation", "string", true}},
                 "double")

            .bind_lambda<double, double, int32_t>(
                "linspace",
                [](Calculator& calc, double start, double stop, int32_t n) -> std::vector<double> {
                    return calc.linspace(start, stop, n);
                })
            .doc("Generate linearly spaced vector",
                 {{"start", "double", true}, {"stop", "double", true}, {"n", "int32", true}},
                 "double[]")

            // --- Tier 3: Custom runner ---
            .bind_custom<ApplyRunner>("apply")
            .doc("Apply a function to each element (sin, cos, sqrt, square)",
                 {{"data", "double[]", true}, {"function", "string", true}}, "double[]");
    }
};

MEX_ENTRY(CalcMex)
