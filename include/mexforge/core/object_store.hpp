#ifndef MEXFORGE_CORE_OBJECT_STORE_HPP
#define MEXFORGE_CORE_OBJECT_STORE_HPP

#include <map>
#include <memory>
#include <stdexcept>
#include <string>

namespace mexforge {

// ============================================================================
// Type-agnostic object store for managing wrapped C++ instances.
//
// Objects are stored as unique_ptr — the store is the sole owner.
// Access is via reference (get_ref) to avoid unnecessary refcounting.
//
// Usage:
//   ObjectStore<MyDevice> store;
//   int id = store.emplace("arg1", 42);
//   MyDevice& ref = store.get_ref(id);
//   store.remove(id);
// ============================================================================

template<typename T>
class ObjectStore {
public:
    int add(std::unique_ptr<T> obj) {
        int id = nextId_++;
        objects_[id] = std::move(obj);
        return id;
    }

    template<typename... Args>
    int emplace(Args&&... args) {
        return add(std::make_unique<T>(std::forward<Args>(args)...));
    }

    // Non-owning reference access — throws if not found
    T& get_ref(int id) {
        auto it = objects_.find(id);
        if (it == objects_.end()) {
            throw std::runtime_error(
                "MexForge: Object with ID " + std::to_string(id) + " not found");
        }
        return *it->second;
    }

    const T& get_ref(int id) const {
        auto it = objects_.find(id);
        if (it == objects_.end()) {
            throw std::runtime_error(
                "MexForge: Object with ID " + std::to_string(id) + " not found");
        }
        return *it->second;
    }

    bool exists(int id) const {
        return objects_.find(id) != objects_.end();
    }

    void remove(int id) {
        objects_.erase(id);
    }

    void clear() {
        objects_.clear();
        nextId_ = 0;
    }

    size_t size() const { return objects_.size(); }

    // Iterate over all objects
    template<typename Func>
    void for_each(Func&& fn) {
        for (auto& [id, obj] : objects_) {
            fn(id, *obj);
        }
    }

private:
    std::map<int, std::unique_ptr<T>> objects_;
    int nextId_ = 0;
};

} // namespace mexforge

#endif // MEXFORGE_CORE_OBJECT_STORE_HPP
