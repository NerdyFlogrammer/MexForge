#ifndef MEXFORGE_CORE_REGISTRY_HPP
#define MEXFORGE_CORE_REGISTRY_HPP

#include "runner.hpp"

#include <functional>
#include <memory>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

namespace mexforge {

// ============================================================================
// Function metadata for introspection, help texts, and type checking
// ============================================================================

struct ArgMeta {
    std::string name;
    std::string type; // "double", "string", "int32", "struct", etc.
    bool required = true;
};

struct FunctionMeta {
    std::string description;
    std::vector<ArgMeta> args;
    std::string return_type;
    bool needs_object = true;
};

// ============================================================================
// Registry: Maps function names to runner factories + metadata
//
// Lazy instantiation — runners are created on first call, then cached.
// ============================================================================

class Registry {
public:
    using Factory = std::function<std::unique_ptr<RunnerBase>()>;

    void add(const std::string& name, Factory factory) { factories_[name] = std::move(factory); }

    void set_meta(const std::string& name, FunctionMeta meta) { meta_[name] = std::move(meta); }

    [[nodiscard]] bool exists(const std::string& name) const {
        return factories_.find(name) != factories_.end();
    }

    [[nodiscard]] bool has_meta(const std::string& name) const {
        return meta_.find(name) != meta_.end();
    }

    [[nodiscard]] const FunctionMeta& get_meta(const std::string& name) const {
        static const FunctionMeta empty{};
        auto it = meta_.find(name);
        return (it != meta_.end()) ? it->second : empty;
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

    [[nodiscard]] std::vector<std::string> list() const {
        std::vector<std::string> names;
        names.reserve(factories_.size());
        for (const auto& [name, _] : factories_) {
            names.push_back(name);
        }
        return names;
    }

    // Returns names of all functions that have metadata attached via set_meta().
    [[nodiscard]] std::vector<std::string> list_with_meta() const {
        std::vector<std::string> names;
        names.reserve(meta_.size());
        for (const auto& [name, _] : meta_) {
            names.push_back(name);
        }
        return names;
    }

    [[nodiscard]] size_t size() const { return factories_.size(); }

private:
    std::unordered_map<std::string, Factory> factories_;
    std::unordered_map<std::string, std::unique_ptr<RunnerBase>> cache_;
    std::unordered_map<std::string, FunctionMeta> meta_;
};

// ============================================================================
// RegistryBuilder: Fluent API for building the registry with metadata
//
// Usage:
//   RegistryBuilder<MyClass>(store)
//       .bind_auto<&MyClass::getRate>("get_rate")
//           .doc("Get the current sample rate", {{"chan", "int32", false}}, "double")
//       .bind_auto<&MyClass::setRate>("set_rate")
//           .doc("Set the sample rate", {{"rate", "double"}, {"chan", "int32", false}})
//       .build();
// ============================================================================

template<typename ObjType> class RegistryBuilder {
public:
    explicit RegistryBuilder(ObjectStore<ObjType>& store) : store_(store) {}

    // ---- Documentation (chainable after any bind call) ----

    RegistryBuilder& doc(const std::string& description, std::initializer_list<ArgMeta> args = {},
                         const std::string& return_type = "void") {
        if (!lastBound_.empty()) {
            FunctionMeta meta;
            meta.description = description;
            meta.args = args;
            meta.return_type = return_type;
            meta.needs_object = lastBoundNeedsObject_;
            registry_.set_meta(lastBound_, std::move(meta));
        }
        return *this;
    }

    // ---- Tier 1: Auto-bind a member function ----
    // Capture store_ as a raw pointer: the ObjectStore lives in MexGateway and
    // outlives the RegistryBuilder, so the pointer is always valid when the
    // factory lambda is called later.
    template<auto MethodPtr> RegistryBuilder& bind_auto(const std::string& name) {
        auto* s = &store_;
        registry_.add(name, [s]() {
            return std::make_unique<AutoObjectRunner<ObjType, MethodPtr>>(*s);
        });
        lastBound_ = name;
        lastBoundNeedsObject_ = true;
        return *this;
    }

    // ---- Tier 1b: Auto-bind a free/static function ----
    template<auto FuncPtr> RegistryBuilder& bind_free(const std::string& name) {
        registry_.add(name, []() { return std::make_unique<AutoFreeRunner<FuncPtr>>(); });
        lastBound_ = name;
        lastBoundNeedsObject_ = false;
        return *this;
    }

    // ---- Tier 2: Lambda-bind with explicit argument types ----
    template<typename... Args, typename Func>
    RegistryBuilder& bind_lambda(const std::string& name, Func&& func) {
        auto* s = &store_;
        auto fn = std::make_shared<std::decay_t<Func>>(std::forward<Func>(func));
        registry_.add(name, [s, fn]() {
            return std::make_unique<LambdaObjectRunner<ObjType, std::decay_t<Func>, Args...>>(
                *s, *fn);
        });
        lastBound_ = name;
        lastBoundNeedsObject_ = true;
        return *this;
    }

    // ---- Tier 2b: Lambda-bind free function (no object) ----
    template<typename... Args, typename Func>
    RegistryBuilder& bind_free_lambda(const std::string& name, Func&& func) {
        auto fn = std::make_shared<std::decay_t<Func>>(std::forward<Func>(func));
        registry_.add(name, [fn]() {
            return std::make_unique<LambdaFreeRunner<std::decay_t<Func>, Args...>>(*fn);
        });
        lastBound_ = name;
        lastBoundNeedsObject_ = false;
        return *this;
    }

    // ---- Tier 3: Custom runner class ----
    template<typename RunnerType> RegistryBuilder& bind_custom(const std::string& name) {
        auto* s = &store_;
        registry_.add(name, [s]() { return std::make_unique<RunnerType>(*s); });
        lastBound_ = name;
        lastBoundNeedsObject_ = true;
        return *this;
    }

    // ---- Tier 3b: Custom free runner (no object store) ----
    template<typename RunnerType> RegistryBuilder& bind_custom_free(const std::string& name) {
        registry_.add(name, []() { return std::make_unique<RunnerType>(); });
        lastBound_ = name;
        lastBoundNeedsObject_ = false;
        return *this;
    }

    Registry build() { return std::move(registry_); }

private:
    ObjectStore<ObjType>& store_;
    Registry registry_;
    std::string lastBound_;
    bool lastBoundNeedsObject_ = true;
};

} // namespace mexforge

#endif // MEXFORGE_CORE_REGISTRY_HPP
