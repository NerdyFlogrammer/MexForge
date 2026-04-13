#ifndef MEXFORGE_CORE_GATEWAY_HPP
#define MEXFORGE_CORE_GATEWAY_HPP

#include "MatlabDataArray.hpp"
#include "logger.hpp"
#include "marshaller.hpp"
#include "mex.hpp"
#include "mexAdapter.hpp"
#include "object_store.hpp"
#include "registry.hpp"

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
    MexGateway() : logger_(getEngine()) {
        RegistryBuilder<ObjType> builder(store_);

        // User-defined bindings
        setup(builder);

        registry_ = builder.build();
        logger_.info("MexForge: Initialized with ", std::to_string(registry_.size()), " bindings");
    }

    void operator()(matlab::mex::ArgumentList outputs, matlab::mex::ArgumentList inputs) override {
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
            } else if (outputs.size() > 0) {
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
            if (outputs.size() > 0) {
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
            if (outputs.size() > 0) {
                outputs[0] = factory_.createScalar(static_cast<uint32_t>(store_.size()));
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
