// Tests for ObjectStore — no MATLAB dependency needed
#include "../include/mexforge/core/object_store.hpp"
#include <cassert>
#include <iostream>
#include <string>

struct TestObj {
    std::string name;
    int value;
    TestObj(std::string n, int v) : name(std::move(n)), value(v) {}
};

void test_emplace_and_get() {
    mexforge::ObjectStore<TestObj> store;

    int id = store.emplace("obj1", 42);
    assert(id == 0);
    assert(store.size() == 1);

    auto& ref = store.get_ref(id);
    assert(ref.name == "obj1");
    assert(ref.value == 42);

    std::cout << "  [PASS] emplace_and_get\n";
}

void test_multiple_objects() {
    mexforge::ObjectStore<TestObj> store;

    int id0 = store.emplace("a", 1);
    int id1 = store.emplace("b", 2);
    int id2 = store.emplace("c", 3);

    assert(id0 == 0 && id1 == 1 && id2 == 2);
    assert(store.size() == 3);

    assert(store.get_ref(id0).name == "a");
    assert(store.get_ref(id1).name == "b");
    assert(store.get_ref(id2).name == "c");

    std::cout << "  [PASS] multiple_objects\n";
}

void test_remove() {
    mexforge::ObjectStore<TestObj> store;

    int id0 = store.emplace("x", 10);
    int id1 = store.emplace("y", 20);

    store.remove(id0);
    assert(store.size() == 1);
    assert(!store.exists(id0));
    assert(store.exists(id1));

    // get_ref on removed ID should throw
    bool threw = false;
    try {
        store.get_ref(id0);
    } catch (const std::runtime_error&) {
        threw = true;
    }
    assert(threw);

    std::cout << "  [PASS] remove\n";
}

void test_clear() {
    mexforge::ObjectStore<TestObj> store;

    store.emplace("a", 1);
    store.emplace("b", 2);
    assert(store.size() == 2);

    store.clear();
    assert(store.size() == 0);

    // IDs reset after clear
    int id = store.emplace("c", 3);
    assert(id == 0);

    std::cout << "  [PASS] clear\n";
}

void test_for_each() {
    mexforge::ObjectStore<TestObj> store;

    store.emplace("a", 1);
    store.emplace("b", 2);
    store.emplace("c", 3);

    int sum = 0;
    store.for_each([&sum](int /*id*/, TestObj& obj) {
        sum += obj.value;
    });
    assert(sum == 6);

    std::cout << "  [PASS] for_each\n";
}

void test_const_access() {
    mexforge::ObjectStore<TestObj> store;
    store.emplace("readonly", 99);

    const auto& constStore = store;
    const auto& ref = constStore.get_ref(0);
    assert(ref.name == "readonly");
    assert(ref.value == 99);

    std::cout << "  [PASS] const_access\n";
}

int main() {
    std::cout << "ObjectStore tests:\n";
    test_emplace_and_get();
    test_multiple_objects();
    test_remove();
    test_clear();
    test_for_each();
    test_const_access();
    std::cout << "All ObjectStore tests passed.\n\n";
    return 0;
}
