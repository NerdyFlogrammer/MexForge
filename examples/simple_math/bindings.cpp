// ============================================================================
// Example: MexForge bindings for the Calculator class
//
// This shows all 3 binding tiers in action.
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
        // inputs[0] = vector<double>, inputs[1] = function name
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
        // --- Lifecycle: create/destroy via lambda with store access ---
        b.bind_free_lambda<std::string>("create",
                                        [this](std::string name) -> uint32_t {
                                            return static_cast<uint32_t>(store().emplace(name));
                                        })
            .bind_free_lambda<uint32_t>(
                "destroy", [this](uint32_t id) { store().remove(static_cast<int>(id)); })

            // --- Tier 1: Auto-bind (1:1 method mapping, zero boilerplate) ---
            .bind_auto<&Calculator::getName>("get_name")
            .bind_auto<&Calculator::getMemory>("get_memory")
            .bind_auto<&Calculator::setMemory>("set_memory")
            .bind_auto<&Calculator::clearMemory>("clear_memory")
            .bind_auto<&Calculator::add>("add")
            .bind_auto<&Calculator::multiply>("multiply")
            .bind_auto<&Calculator::power>("power")

            // --- Tier 2: Lambda (custom logic, auto-marshalling) ---
            .bind_lambda<double, double, std::string>(
                "compute",
                [](Calculator& calc, double a, double b, std::string op) -> double {
                    return calc.compute(a, b, op);
                })

            .bind_lambda<double, double, int32_t>(
                "linspace",
                [](Calculator& calc, double start, double stop, int32_t n) -> std::vector<double> {
                    return calc.linspace(start, stop, n);
                })

            // --- Tier 3: Custom runner (full control) ---
            .bind_custom<ApplyRunner>("apply");
    }
};

// Generate MEX entry point
MEX_ENTRY(CalcMex)
