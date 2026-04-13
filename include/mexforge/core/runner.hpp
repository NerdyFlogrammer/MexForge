#ifndef MEXFORGE_CORE_RUNNER_HPP
#define MEXFORGE_CORE_RUNNER_HPP

#include "logger.hpp"
#include "marshaller.hpp"
#include "method_traits.hpp"
#include "object_store.hpp"
#include "types.hpp"

#include <functional>
#include <memory>
#include <string>
#include <tuple>
#include <utility>

namespace mexforge {

// Forward declaration
template<typename T> class MexLibrary;

// ============================================================================
// RunnerBase: Abstract base for all function runners
//
// Engine is passed as const shared_ptr& because MATLAB's API returns
// shared_ptr — we avoid an atomic refcount bump by not copying it.
// Logger is passed as reference — the Gateway owns it and outlives all calls.
// ============================================================================

class RunnerBase {
public:
    virtual ~RunnerBase() = default;

    virtual void run(matlab::mex::ArgumentList outputs, matlab::mex::ArgumentList inputs,
                     const std::shared_ptr<matlab::engine::MATLABEngine>& engine,
                     Logger& logger) = 0;

protected:
    matlab::data::ArrayFactory factory_;
};

// ============================================================================
// ArgumentValidator: Compile-time + runtime argument checking
// ============================================================================

namespace detail {

template<typename Tuple, size_t... Is> constexpr size_t count_required(std::index_sequence<Is...>) {
    size_t count = 0;
    ((count += is_optional_v<std::tuple_element_t<Is, Tuple>> ? 0 : 1), ...);
    return count;
}

inline void validate_arg_count(size_t provided, size_t required, size_t total,
                               const std::string& funcName,
                               const std::shared_ptr<matlab::engine::MATLABEngine>& engine) {
    if (provided < required || provided > total) {
        std::ostringstream oss;
        oss << "MexForge: '" << funcName << "' expects ";
        if (required == total)
            oss << required;
        else
            oss << required << "-" << total;
        oss << " arguments, got " << provided;

        matlab::data::ArrayFactory factory;
        engine->feval(
            u"error",
            {factory.createScalar("mexforge:invalidArgs"), factory.createScalar(oss.str())}, {});
    }
}

// Unmarshal arguments from MATLAB into a std::tuple
template<typename Tuple, size_t... Is>
Tuple unmarshal_args(matlab::mex::ArgumentList& inputs, size_t inputSize,
                     std::index_sequence<Is...>) {
    return Tuple{[&]() -> std::tuple_element_t<Is, Tuple> {
        using ArgType = std::tuple_element_t<Is, Tuple>;
        if constexpr (is_optional_v<ArgType>) {
            if (Is < inputSize) {
                using Inner = unwrap_optional_t<ArgType>;
                return FromMatlab<Inner>::convert(inputs[Is]);
            }
            return std::nullopt;
        } else {
            return FromMatlab<ArgType>::convert(inputs[Is]);
        }
    }()...};
}

// Call a member function with tuple args
template<typename Obj, typename R, typename... FnArgs, typename Tuple, size_t... Is>
R call_method_impl(Obj& obj, R (std::decay_t<Obj>::*fn)(FnArgs...), Tuple& args,
                   std::index_sequence<Is...>) {
    return (obj.*fn)(std::get<Is>(args)...);
}

template<typename Obj, typename R, typename... FnArgs, typename Tuple, size_t... Is>
R call_method_impl(Obj& obj, R (std::decay_t<Obj>::*fn)(FnArgs...) const, Tuple& args,
                   std::index_sequence<Is...>) {
    return (obj.*fn)(std::get<Is>(args)...);
}

template<typename Obj, auto Fn, typename Tuple> auto call_method(Obj& obj, Tuple& args) {
    constexpr size_t N = std::tuple_size_v<Tuple>;
    return call_method_impl(obj, Fn, args, std::make_index_sequence<N>{});
}

// Call a free function with tuple args
template<auto Fn, typename Tuple, size_t... Is>
auto call_free_impl(Tuple& args, std::index_sequence<Is...>) {
    return Fn(std::get<Is>(args)...);
}

template<auto Fn, typename Tuple> auto call_free(Tuple& args) {
    constexpr size_t N = std::tuple_size_v<Tuple>;
    return call_free_impl<Fn>(args, std::make_index_sequence<N>{});
}

// Call a lambda/functor with object + tuple args
template<typename Func, typename Obj, typename Tuple, size_t... Is>
auto call_lambda_impl(Func& fn, Obj& obj, Tuple& args, std::index_sequence<Is...>) {
    return fn(obj, std::get<Is>(args)...);
}

template<typename Func, typename Obj, typename Tuple>
auto call_lambda(Func& fn, Obj& obj, Tuple& args) {
    constexpr size_t N = std::tuple_size_v<Tuple>;
    return call_lambda_impl(fn, obj, args, std::make_index_sequence<N>{});
}

} // namespace detail

// ============================================================================
// Tier 1: AutoObjectRunner — automatic binding of a member function pointer
//
// Usage:
//   using GetRate = AutoObjectRunner<MyClass, &MyClass::getRate>;
// ============================================================================

template<typename ObjType, auto MethodPtr> class AutoObjectRunner : public RunnerBase {
    using Traits = FnTraits<MethodPtr>;
    using ArgsTuple = typename Traits::ArgTypes;
    using ReturnType = typename Traits::ReturnType;

    static constexpr size_t NumArgs = Traits::arity;
    static constexpr size_t RequiredArgs =
        detail::count_required<ArgsTuple>(std::make_index_sequence<NumArgs>{});

    ObjectStore<ObjType>& store_;

public:
    explicit AutoObjectRunner(ObjectStore<ObjType>& store) : store_(store) {}

    void run(matlab::mex::ArgumentList outputs, matlab::mex::ArgumentList inputs,
             const std::shared_ptr<matlab::engine::MATLABEngine>& engine, Logger& logger) override {
        // inputs[0] = function name (string), inputs[1] = object ID (uint32)
        // inputs[2..] = actual arguments
        detail::validate_arg_count(inputs.size() - 2, RequiredArgs, NumArgs,
                                   std::string("auto_method"), engine);

        uint32_t objId = FromMatlab<uint32_t>::convert(inputs[1]);
        auto& obj = store_.get_ref(objId);

        // Extract user arguments (skip name + object ID)
        auto userInputs =
            matlab::mex::ArgumentList(inputs.begin() + 2, inputs.end(), inputs.size() - 2);

        auto args = detail::unmarshal_args<ArgsTuple>(userInputs, userInputs.size(),
                                                      std::make_index_sequence<NumArgs>{});

        logger.debug("AutoObjectRunner: calling method on object ", objId);

        if constexpr (std::is_void_v<ReturnType>) {
            detail::call_method<ObjType, MethodPtr>(obj, args);
        } else {
            auto result = detail::call_method<ObjType, MethodPtr>(obj, args);
            if (outputs.size() > 0) {
                outputs[0] = to_matlab(factory_, result);
            }
        }
    }
};

// ============================================================================
// Tier 1b: AutoFreeRunner — automatic binding of a free/static function
// ============================================================================

template<auto FuncPtr> class AutoFreeRunner : public RunnerBase {
    using Traits = FnTraits<FuncPtr>;
    using ArgsTuple = typename Traits::ArgTypes;
    using ReturnType = typename Traits::ReturnType;

    static constexpr size_t NumArgs = Traits::arity;
    static constexpr size_t RequiredArgs =
        detail::count_required<ArgsTuple>(std::make_index_sequence<NumArgs>{});

public:
    void run(matlab::mex::ArgumentList outputs, matlab::mex::ArgumentList inputs,
             const std::shared_ptr<matlab::engine::MATLABEngine>& engine, Logger& logger) override {
        // inputs[0] = function name, inputs[1..] = arguments
        detail::validate_arg_count(inputs.size() - 1, RequiredArgs, NumArgs,
                                   std::string("free_function"), engine);

        auto userInputs =
            matlab::mex::ArgumentList(inputs.begin() + 1, inputs.end(), inputs.size() - 1);

        auto args = detail::unmarshal_args<ArgsTuple>(userInputs, userInputs.size(),
                                                      std::make_index_sequence<NumArgs>{});

        logger.debug("AutoFreeRunner: calling free function");

        if constexpr (std::is_void_v<ReturnType>) {
            detail::call_free<FuncPtr>(args);
        } else {
            auto result = detail::call_free<FuncPtr>(args);
            if (outputs.size() > 0) {
                outputs[0] = to_matlab(factory_, result);
            }
        }
    }
};

// ============================================================================
// Tier 2: LambdaObjectRunner — lambda/functor with custom logic
//
// Usage:
//   auto runner = LambdaObjectRunner<MyClass, Func, double, std::optional<int>>(
//       store, lambda);
// ============================================================================

template<typename ObjType, typename Func, typename... Args>
class LambdaObjectRunner : public RunnerBase {
    using ArgsTuple = std::tuple<Args...>;
    using ReturnType = std::invoke_result_t<Func, ObjType&, Args...>;

    static constexpr size_t NumArgs = sizeof...(Args);
    static constexpr size_t RequiredArgs =
        detail::count_required<ArgsTuple>(std::make_index_sequence<NumArgs>{});

    ObjectStore<ObjType>& store_;
    Func func_;

public:
    LambdaObjectRunner(ObjectStore<ObjType>& store, Func func)
        : store_(store), func_(std::move(func)) {}

    void run(matlab::mex::ArgumentList outputs, matlab::mex::ArgumentList inputs,
             const std::shared_ptr<matlab::engine::MATLABEngine>& engine, Logger& logger) override {
        detail::validate_arg_count(inputs.size() - 2, RequiredArgs, NumArgs,
                                   std::string("lambda_method"), engine);

        uint32_t objId = FromMatlab<uint32_t>::convert(inputs[1]);
        auto& obj = store_.get_ref(objId);

        auto userInputs =
            matlab::mex::ArgumentList(inputs.begin() + 2, inputs.end(), inputs.size() - 2);

        auto args = detail::unmarshal_args<ArgsTuple>(userInputs, userInputs.size(),
                                                      std::make_index_sequence<NumArgs>{});

        logger.debug("LambdaObjectRunner: calling on object ", objId);

        if constexpr (std::is_void_v<ReturnType>) {
            detail::call_lambda(func_, obj, args);
        } else {
            auto result = detail::call_lambda(func_, obj, args);
            if (outputs.size() > 0) {
                outputs[0] = to_matlab(factory_, result);
            }
        }
    }
};

// ============================================================================
// Tier 2b: LambdaFreeRunner — lambda without object
// ============================================================================

template<typename Func, typename... Args> class LambdaFreeRunner : public RunnerBase {
    using ArgsTuple = std::tuple<Args...>;
    using ReturnType = std::invoke_result_t<Func, Args...>;

    static constexpr size_t NumArgs = sizeof...(Args);
    static constexpr size_t RequiredArgs =
        detail::count_required<ArgsTuple>(std::make_index_sequence<NumArgs>{});

    Func func_;

public:
    explicit LambdaFreeRunner(Func func) : func_(std::move(func)) {}

    void run(matlab::mex::ArgumentList outputs, matlab::mex::ArgumentList inputs,
             const std::shared_ptr<matlab::engine::MATLABEngine>& engine, Logger& logger) override {
        detail::validate_arg_count(inputs.size() - 1, RequiredArgs, NumArgs,
                                   std::string("lambda_free"), engine);

        (void)logger; // Not used in auto-dispatch

        auto userInputs =
            matlab::mex::ArgumentList(inputs.begin() + 1, inputs.end(), inputs.size() - 1);

        auto args = detail::unmarshal_args<ArgsTuple>(userInputs, userInputs.size(),
                                                      std::make_index_sequence<NumArgs>{});

        if constexpr (std::is_void_v<ReturnType>) {
            std::apply(func_, args);
        } else {
            auto result = std::apply(func_, args);
            if (outputs.size() > 0) {
                outputs[0] = to_matlab(factory_, result);
            }
        }
    }
};

// ============================================================================
// Tier 3: CustomRunner — full control, user implements execute()
//
// Object is passed as reference — the ObjectStore owns the lifetime.
// Logger is passed as reference — the Gateway owns the lifetime.
//
// Usage:
//   class MyStreamHandler : public CustomRunner<MyDevice> {
//       void execute(
//           MyDevice& obj,
//           matlab::mex::ArgumentList outputs,
//           matlab::mex::ArgumentList inputs,
//           matlab::data::ArrayFactory& factory,
//           Logger& logger) override { ... }
//   };
// ============================================================================

template<typename ObjType> class CustomRunner : public RunnerBase {
    ObjectStore<ObjType>& store_;

public:
    explicit CustomRunner(ObjectStore<ObjType>& store) : store_(store) {}

    void run(matlab::mex::ArgumentList outputs, matlab::mex::ArgumentList inputs,
             const std::shared_ptr<matlab::engine::MATLABEngine>& /*engine*/,
             Logger& logger) override {
        uint32_t objId = FromMatlab<uint32_t>::convert(inputs[1]);
        auto& obj = store_.get_ref(objId);

        // Skip function name and object ID
        auto userInputs =
            matlab::mex::ArgumentList(inputs.begin() + 2, inputs.end(), inputs.size() - 2);

        execute(obj, outputs, userInputs, factory_, logger);
    }

    virtual void execute(ObjType& obj, matlab::mex::ArgumentList outputs,
                         matlab::mex::ArgumentList inputs, matlab::data::ArrayFactory& factory,
                         Logger& logger) = 0;
};

// ============================================================================
// Tier 3b: CustomFreeRunner — full control without object
// ============================================================================

class CustomFreeRunner : public RunnerBase {
public:
    void run(matlab::mex::ArgumentList outputs, matlab::mex::ArgumentList inputs,
             const std::shared_ptr<matlab::engine::MATLABEngine>& engine, Logger& logger) override {
        auto userInputs =
            matlab::mex::ArgumentList(inputs.begin() + 1, inputs.end(), inputs.size() - 1);
        execute(outputs, userInputs, factory_, engine, logger);
    }

    virtual void execute(matlab::mex::ArgumentList outputs, matlab::mex::ArgumentList inputs,
                         matlab::data::ArrayFactory& factory,
                         const std::shared_ptr<matlab::engine::MATLABEngine>& engine,
                         Logger& logger) = 0;
};

} // namespace mexforge

#endif // MEXFORGE_CORE_RUNNER_HPP
