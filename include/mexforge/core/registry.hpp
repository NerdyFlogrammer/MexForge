#ifndef MEXFORGE_CORE_REGISTRY_HPP
#define MEXFORGE_CORE_REGISTRY_HPP

#include "runner.hpp"

#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <stdexcept>

namespace mexforge {

// ============================================================================
// Registry: Maps function names to runner factories
//
// Lazy instantiation — runners are created on first call, then cached.
// ============================================================================

class Registry {
public:
    using Factory = std::function<std::unique_ptr<RunnerBase>()>;

    void add(const std::string& name, Factory factory) {
        factories_[name] = std::move(factory);
    }

    bool exists(const std::string& name) const {
        return factories_.find(name) != factories_.end();
    }

    // Returns non-owning reference. Registry owns the runner lifetime.
    RunnerBase& get(const std::string& name) {
        // Check cache first
        auto cacheIt = cache_.find(name);
        if (cacheIt != cache_.end()) {
            return *cacheIt->second;
        }

        // Create and cache
        auto factoryIt = factories_.find(name);
        if (factoryIt == factories_.end()) {
            throw std::runtime_error("MexForge: Unknown function '" + name + "'");
        }

        auto& cached = cache_[name];
        cached = factoryIt->second();
        return *cached;
    }

    std::vector<std::string> list() const {
        std::vector<std::string> names;
        names.reserve(factories_.size());
        for (const auto& [name, _] : factories_) {
            names.push_back(name);
        }
        return names;
    }

    size_t size() const { return factories_.size(); }

private:
    std::unordered_map<std::string, Factory> factories_;
    std::unordered_map<std::string, std::unique_ptr<RunnerBase>> cache_;
};

// ============================================================================
// RegistryBuilder: Fluent API for building the registry
//
// Usage:
//   auto registry = RegistryBuilder<MyClass>(store)
//       .bind_auto<&MyClass::getRate>("get_rate")
//       .bind_auto<&MyClass::setRate>("set_rate")
//       .bind_lambda<double, std::optional<int>>("compute",
//           [](MyClass& obj, double x, std::optional<int> mode) { ... })
//       .bind_custom<MyStreamHandler>("stream")
//       .bind_free<&freeFunction>("find_devices")
//       .build();
// ============================================================================

template<typename ObjType>
class RegistryBuilder {
public:
    explicit RegistryBuilder(ObjectStore<ObjType>& store)
        : store_(store) {}

    // Tier 1: Auto-bind a member function
    template<auto MethodPtr>
    RegistryBuilder& bind_auto(const std::string& name) {
        registry_.add(name, [this]() {
            return std::make_unique<AutoObjectRunner<ObjType, MethodPtr>>(store_);
        });
        return *this;
    }

    // Tier 1b: Auto-bind a free/static function
    template<auto FuncPtr>
    RegistryBuilder& bind_free(const std::string& name) {
        registry_.add(name, []() {
            return std::make_unique<AutoFreeRunner<FuncPtr>>();
        });
        return *this;
    }

    // Tier 2: Lambda-bind with explicit argument types
    // Note: shared_ptr is required here because std::function must be copyable
    template<typename... Args, typename Func>
    RegistryBuilder& bind_lambda(const std::string& name, Func&& func) {
        auto fn = std::make_shared<std::decay_t<Func>>(std::forward<Func>(func));
        registry_.add(name, [this, fn]() {
            return std::make_unique<LambdaObjectRunner<ObjType, std::decay_t<Func>, Args...>>(
                store_, *fn);
        });
        return *this;
    }

    // Tier 2b: Lambda-bind free function (no object)
    template<typename... Args, typename Func>
    RegistryBuilder& bind_free_lambda(const std::string& name, Func&& func) {
        auto fn = std::make_shared<std::decay_t<Func>>(std::forward<Func>(func));
        registry_.add(name, [fn]() {
            return std::make_unique<LambdaFreeRunner<std::decay_t<Func>, Args...>>(*fn);
        });
        return *this;
    }

    // Tier 3: Custom runner class
    template<typename RunnerType>
    RegistryBuilder& bind_custom(const std::string& name) {
        registry_.add(name, [this]() {
            return std::make_unique<RunnerType>(store_);
        });
        return *this;
    }

    // Tier 3b: Custom free runner (no object store)
    template<typename RunnerType>
    RegistryBuilder& bind_custom_free(const std::string& name) {
        registry_.add(name, []() {
            return std::make_unique<RunnerType>();
        });
        return *this;
    }

    Registry build() { return std::move(registry_); }

private:
    ObjectStore<ObjType>& store_;
    Registry registry_;
};

} // namespace mexforge

#endif // MEXFORGE_CORE_REGISTRY_HPP
