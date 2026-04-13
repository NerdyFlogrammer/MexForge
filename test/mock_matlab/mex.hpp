// Mock MATLAB MEX headers for unit testing (no MATLAB required)
// Provides functional ArgumentList and Engine for testing dispatch logic.
#ifndef MOCK_MEX_HPP
#define MOCK_MEX_HPP

#include "MatlabDataArray.hpp"
#include <memory>
#include <vector>
#include <string>

namespace matlab {
namespace engine {

class MATLABEngine {
public:
    // Track feval calls for test assertions
    struct Call {
        std::u16string func;
        std::vector<matlab::data::Array> args;
    };

    matlab::data::Array feval(
        const std::u16string& func,
        std::vector<matlab::data::Array> args,
        std::vector<matlab::data::Array> = {})
    {
        calls.push_back({func, std::move(args)});
        return {};
    }

    std::vector<Call> calls;
};

} // namespace engine

namespace mex {

// Functional ArgumentList backed by shared storage.
//
// In the real MATLAB C++ API, ArgumentList is a reference-counted proxy — copies
// refer to the same underlying arrays. The mock mirrors this via shared_ptr so
// that writes through a by-value copy (as in runner::run()) are visible to the
// caller's ArgumentList instance.
class ArgumentList {
public:
    ArgumentList() : data_(std::make_shared<std::vector<matlab::data::Array>>()) {}

    explicit ArgumentList(std::vector<matlab::data::Array> data)
        : data_(std::make_shared<std::vector<matlab::data::Array>>(std::move(data))) {}

    ArgumentList(
        const matlab::data::Array* begin,
        const matlab::data::Array* end,
        size_t /*size*/)
        : data_(std::make_shared<std::vector<matlab::data::Array>>(begin, end)) {}

    matlab::data::Array& operator[](size_t i) { return (*data_)[i]; }
    const matlab::data::Array& operator[](size_t i) const { return (*data_)[i]; }

    size_t size() const { return data_->size(); }
    bool empty() const { return data_->empty(); }

    const matlab::data::Array* begin() const { return data_->data(); }
    const matlab::data::Array* end() const { return data_->data() + data_->size(); }

private:
    std::shared_ptr<std::vector<matlab::data::Array>> data_;
};

class Function {
public:
    virtual ~Function() = default;
    virtual void operator()(ArgumentList outputs, ArgumentList inputs) = 0;

protected:
    std::shared_ptr<matlab::engine::MATLABEngine> getEngine() {
        if (!engine_) {
            engine_ = std::make_shared<matlab::engine::MATLABEngine>();
        }
        return engine_;
    }

private:
    std::shared_ptr<matlab::engine::MATLABEngine> engine_;
};

} // namespace mex
} // namespace matlab

#endif // MOCK_MEX_HPP
