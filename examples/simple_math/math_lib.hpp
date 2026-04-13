#ifndef MEXFORGE_EXAMPLE_MATH_LIB_HPP
#define MEXFORGE_EXAMPLE_MATH_LIB_HPP

// ============================================================================
// Example: A simple C++ library with NO MATLAB dependency.
// This is what a user would wrap with MexForge.
// ============================================================================

#include <cmath>
#include <string>
#include <vector>
#include <stdexcept>
#include <optional>

class Calculator {
public:
    explicit Calculator(const std::string& name = "default")
        : name_(name), memory_(0.0) {}

    // --- Simple getters/setters (Tier 1 candidates) ---

    std::string getName() const { return name_; }
    double getMemory() const { return memory_; }
    void setMemory(double val) { memory_ = val; }
    void clearMemory() { memory_ = 0.0; }

    // --- Methods with optional args (Tier 2 candidates) ---

    double add(double a, double b) const { return a + b; }
    double multiply(double a, double b) const { return a * b; }

    double power(double base, double exp) const {
        return std::pow(base, exp);
    }

    // Store result in memory optionally
    double compute(double a, double b, const std::string& op) {
        double result = 0.0;
        if (op == "add") result = a + b;
        else if (op == "sub") result = a - b;
        else if (op == "mul") result = a * b;
        else if (op == "div") {
            if (b == 0.0) throw std::runtime_error("Division by zero");
            result = a / b;
        }
        else throw std::runtime_error("Unknown operation: " + op);

        memory_ = result;
        return result;
    }

    // --- Batch operations (complex, Tier 3 candidate) ---

    std::vector<double> linspace(double start, double stop, int n) const {
        if (n < 2) throw std::runtime_error("n must be >= 2");
        std::vector<double> result(n);
        double step = (stop - start) / (n - 1);
        for (int i = 0; i < n; ++i) {
            result[i] = start + i * step;
        }
        return result;
    }

    std::vector<double> apply(const std::vector<double>& data,
                               const std::string& func) const {
        std::vector<double> result(data.size());
        for (size_t i = 0; i < data.size(); ++i) {
            if (func == "sin") result[i] = std::sin(data[i]);
            else if (func == "cos") result[i] = std::cos(data[i]);
            else if (func == "sqrt") result[i] = std::sqrt(data[i]);
            else if (func == "square") result[i] = data[i] * data[i];
            else throw std::runtime_error("Unknown function: " + func);
        }
        return result;
    }

private:
    std::string name_;
    double memory_;
};

#endif // MEXFORGE_EXAMPLE_MATH_LIB_HPP
