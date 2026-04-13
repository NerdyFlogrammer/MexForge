#ifndef MEXFORGE_CORE_GATEWAY_HPP
#define MEXFORGE_CORE_GATEWAY_HPP

#include "MatlabDataArray.hpp"
#include "logger.hpp"
#include "marshaller.hpp"
#include "mex.hpp"
#include "mexAdapter.hpp"
#include "object_store.hpp"
#include "registry.hpp"

#include <algorithm>
#include <memory>
#include <string>

namespace mexforge {

// ============================================================================
// MexGateway: MEX entry point base class
//
// Users inherit from this and implement setup().
// The gateway handles:
//   - Function dispatch
//   - Built-in control commands (log_level, log_file, etc.)
//   - Error handling
//
// Usage:
//   class MyMex : public MexGateway<MyClass> {
//       void setup(RegistryBuilder<MyClass>& builder) override {
//           builder
//               .bind_auto<&MyClass::getRate>("get_rate")
//               .bind_auto<&MyClass::setRate>("set_rate");
//       }
//   };
//   MEX_ENTRY(MyMex)
// ============================================================================

template<typename ObjType> class MexGateway : public matlab::mex::Function {
public:
    // Lazy initialization: setup() is called on first operator() invocation,
    // not in the constructor. This avoids undefined behavior from calling a
    // pure virtual function during base class construction.
    MexGateway() : logger_(getEngine()), initialized_(false) {}

    void operator()(matlab::mex::ArgumentList outputs, matlab::mex::ArgumentList inputs) override {
        if (!initialized_) {
            RegistryBuilder<ObjType> builder(store_);
            setup(builder);
            registry_ = builder.build();
            initialized_ = true;
            logger_.info("MexForge: Initialized with ", std::to_string(registry_.size()),
                         " bindings");
        }

        if (inputs.empty()) {
            reportError("MexForge: No function name provided");
            return;
        }

        const std::string command = FromMatlab<std::string>::convert(inputs[0]);

        // Handle built-in control commands
        if (handleControlCommand(command, outputs, inputs)) {
            return;
        }

        // Dispatch to registered function
        if (!registry_.exists(command)) {
            reportError("MexForge: Unknown function '" + command + "'");
            return;
        }

        try {
            auto& runner = registry_.get(command);
            runner.run(outputs, inputs, getEngine(), logger_);
        } catch (const std::exception& e) {
            logger_.error("Exception in '", command, "': ", e.what());
            reportError(std::string("MexForge [") + command + "]: " + e.what());
        }
    }

protected:
    // Override this to register your bindings
    virtual void setup(RegistryBuilder<ObjType>& builder) = 0;

    // Access to internals for advanced use
    ObjectStore<ObjType>& store() { return store_; }
    Logger& logger() { return logger_; }
    Registry& registry() { return registry_; }

private:
    bool handleControlCommand(const std::string& cmd, matlab::mex::ArgumentList outputs,
                              matlab::mex::ArgumentList inputs) {
        if (cmd == "__log_level") {
            if (inputs.size() > 1) {
                auto level = FromMatlab<std::string>::convert(inputs[1]);
                logger_.setLevel(level);
                logger_.info("Log level set to: ", level);
            } else if (!outputs.empty()) {
                outputs[0] = factory_.createScalar(std::string(log_level_str(logger_.getLevel())));
            }
            return true;
        }

        if (cmd == "__log_file") {
            if (inputs.size() > 1) {
                auto path = FromMatlab<std::string>::convert(inputs[1]);
                logger_.setLogFile(path);
                logger_.info("Log file set to: ", path);
            } else {
                logger_.closeLogFile();
            }
            return true;
        }

        if (cmd == "__log_matlab") {
            if (inputs.size() > 1) {
                auto enable = FromMatlab<bool>::convert(inputs[1]);
                logger_.enableMatlab(enable);
            }
            return true;
        }

        if (cmd == "__log_buffer") {
            if (inputs.size() > 1) {
                auto enable = FromMatlab<bool>::convert(inputs[1]);
                logger_.enableBuffered(enable);
            }
            return true;
        }

        if (cmd == "__log_flush") {
            logger_.flush();
            return true;
        }

        if (cmd == "__list_functions") {
            auto names = registry_.list();
            std::sort(names.begin(), names.end());
            if (!outputs.empty()) {
                outputs[0] = factory_.createArray({1, names.size()}, names.begin(), names.end());
            } else {
                for (const auto& name : names) {
                    logger_.info("  ", name);
                }
            }
            return true;
        }

        if (cmd == "__store_clear") {
            store_.clear();
            logger_.info("Object store cleared");
            return true;
        }

        if (cmd == "__store_size") {
            if (!outputs.empty()) {
                outputs[0] = factory_.createScalar(static_cast<uint32_t>(store_.size()));
            }
            return true;
        }

        if (cmd == "__describe") {
            if (inputs.size() > 1) {
                auto funcName = FromMatlab<std::string>::convert(inputs[1]);
                const auto& meta = registry_.get_meta(funcName);
                if (!outputs.empty()) {
                    outputs[0] = factory_.createScalar(meta.description);
                }
            }
            return true;
        }

        if (cmd == "__arg_info") {
            if (inputs.size() > 1) {
                auto funcName = FromMatlab<std::string>::convert(inputs[1]);
                const auto& meta = registry_.get_meta(funcName);
                if (!outputs.empty()) {
                    // Return struct array with fields: name, type, required
                    auto result = factory_.createStructArray({1, meta.args.size()},
                                                             {"name", "type", "required"});
                    for (size_t i = 0; i < meta.args.size(); ++i) {
                        result[i]["name"] = factory_.createScalar(meta.args[i].name);
                        result[i]["type"] = factory_.createScalar(meta.args[i].type);
                        result[i]["required"] = factory_.createScalar(meta.args[i].required);
                    }
                    outputs[0] = std::move(result);
                }
            }
            return true;
        }

        if (cmd == "__return_type") {
            if (inputs.size() > 1) {
                auto funcName = FromMatlab<std::string>::convert(inputs[1]);
                const auto& meta = registry_.get_meta(funcName);
                if (!outputs.empty()) {
                    outputs[0] = factory_.createScalar(meta.return_type);
                }
            }
            return true;
        }

        if (cmd == "__needs_object") {
            if (inputs.size() > 1) {
                auto funcName = FromMatlab<std::string>::convert(inputs[1]);
                const auto& meta = registry_.get_meta(funcName);
                if (!outputs.empty()) {
                    outputs[0] = factory_.createScalar(meta.needs_object);
                }
            }
            return true;
        }

        if (cmd == "__list_methods") {
            // Like __list_functions but only object methods (no internals, no free functions)
            auto names = registry_.list_with_meta();
            std::vector<std::string> methods;
            for (const auto& name : names) {
                if (registry_.has_meta(name) && registry_.get_meta(name).needs_object) {
                    methods.push_back(name);
                }
            }
            std::sort(methods.begin(), methods.end());
            if (!outputs.empty()) {
                outputs[0] =
                    factory_.createArray({1, methods.size()}, methods.begin(), methods.end());
            }
            return true;
        }

        return false;
    }

    void reportError(const std::string& msg) {
        auto engine = getEngine();
        engine->feval(u"error",
                      {factory_.createScalar("mexforge:error"), factory_.createScalar(msg)}, {});
    }

    ObjectStore<ObjType> store_;
    Registry registry_;
    Logger logger_;
    matlab::data::ArrayFactory factory_;
    bool initialized_;
};

} // namespace mexforge

// ============================================================================
// MEX_ENTRY: Macro to generate the MEX entry point
//
// Usage:
//   MEX_ENTRY(MyMexLibrary)
//
// This is intentionally minimal. The MexGateway base class does all the work.
// ============================================================================

#define MEX_ENTRY(GatewayClass)                                                                    \
    class MexFunction : public GatewayClass {                                                      \
    public:                                                                                        \
        using GatewayClass::GatewayClass;                                                          \
    };

#endif // MEXFORGE_CORE_GATEWAY_HPP
