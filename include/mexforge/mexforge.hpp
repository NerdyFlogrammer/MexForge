#ifndef MEXFORGE_MEXFORGE_HPP
#define MEXFORGE_MEXFORGE_HPP

// ============================================================================
// MexForge — Generic C++ to MATLAB MEX Binding Library
//
// A header-only C++17 library for creating MEX interfaces with minimal
// boilerplate. Supports three binding tiers:
//
//   Tier 1: bind_auto     — automatic 1:1 method binding (zero boilerplate)
//   Tier 2: bind_lambda   — lambda with custom logic
//   Tier 3: bind_custom   — full control via subclassing
//
// Quick start:
//
//   #include <mexforge/mexforge.hpp>
//
//   class MyMex : public mexforge::MexGateway<MyClass> {
//       void setup(mexforge::RegistryBuilder<MyClass>& b) override {
//           b.bind_auto<&MyClass::getValue>("get_value")
//            .bind_auto<&MyClass::setValue>("set_value")
//            .bind_lambda<double>("compute",
//                [](MyClass& obj, double x) { return obj.compute(x); })
//            .bind_custom<MyCustomRunner>("stream");
//       }
//   };
//   MEX_ENTRY(MyMex)
//
// ============================================================================

#include "core/types.hpp"
#include "core/marshaller.hpp"
#include "core/method_traits.hpp"
#include "core/object_store.hpp"
#include "core/logger.hpp"
#include "core/runner.hpp"
#include "core/registry.hpp"
#include "core/gateway.hpp"

#endif // MEXFORGE_MEXFORGE_HPP
