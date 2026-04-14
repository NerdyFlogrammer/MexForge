# MexForge

[![CI](https://github.com/nerdyflogrammer/MexForge/actions/workflows/ci.yml/badge.svg)](https://github.com/nerdyflogrammer/MexForge/actions/workflows/ci.yml)
[![C++17](https://img.shields.io/badge/C%2B%2B-17-blue.svg)](https://en.cppreference.com/w/cpp/17)
[![Header Only](https://img.shields.io/badge/header--only-yes-green.svg)]()
[![License](https://img.shields.io/badge/License-Apache_2.0-blue.svg)](LICENSE)
[![DOI](https://img.shields.io/badge/DOI-10.5281%2Fzenodo.19560403-blue.svg)](https://doi.org/10.5281/zenodo.19560403)
[![Buy Me a Coffee](https://img.shields.io/badge/Buy%20Me%20a%20Coffee-support-orange.svg)](https://buymeacoffee.com/nerdyflogrammer)

**A header-only C++17 library for wrapping C++ classes as MATLAB MEX interfaces — with minimal boilerplate.**

MexForge eliminates the tedious parts of MEX development. Instead of writing hundreds of lines of argument-parsing and type-conversion code, you declare bindings in a few lines and the library handles the rest: argument marshalling, type validation, object lifecycle, and error reporting — largely at compile time.

**Who is this for?**
- Researchers and engineers who have an existing C++ library and want to call it from MATLAB
- Anyone tired of writing repetitive `mxGetPr` / `mxGetM` / `mexErrMsgIdAndTxt` boilerplate
- Projects where performance matters and a Python bridge is not an option

---

## Installation

MexForge is header-only — no build step, no dependencies beyond a C++17 compiler and MATLAB.

```bash
git clone https://github.com/nerdyflogrammer/MexForge.git
```

Then point `mex` at the `include` directory:

```matlab
mex -I/path/to/MexForge/include CXXFLAGS='$CXXFLAGS -std=c++17' bindings.cpp
```

That's it. No CMake required for using the library (CMake is only used for running the test suite).

---

## Quick Start

### 1. Your C++ library (no MATLAB dependency)

```cpp
class Calculator {
public:
    explicit Calculator(const std::string& name) : name_(name) {}
    std::string getName() const { return name_; }
    double add(double a, double b) const { return a + b; }
    double power(double base, double exp) const { return std::pow(base, exp); }
private:
    std::string name_;
};
```

### 2. MexForge bindings (one file)

```cpp
#include <mexforge/mexforge.hpp>
#include "calculator.hpp"

class CalcMex : public mexforge::MexGateway<Calculator> {
protected:
    void setup(mexforge::RegistryBuilder<Calculator>& b) override {

        // Lifecycle
        b.bind_free_lambda<std::string>("create",
            [this](std::string name) -> uint32_t {
                return static_cast<uint32_t>(store().emplace(name));
            })
         .bind_free_lambda<uint32_t>("destroy",
            [this](uint32_t id) { store().remove(static_cast<int>(id)); })

        // Tier 1: Auto-bind — one line, zero boilerplate
         .bind_auto<&Calculator::getName>("get_name")
         .bind_auto<&Calculator::add>("add")
         .bind_auto<&Calculator::power>("power");
    }
};

MEX_ENTRY(CalcMex)
```

### 3. Build

```matlab
mex -I<path-to-mexforge>/include CXXFLAGS='$CXXFLAGS -std=c++17' bindings.cpp
```

### 4. Use from MATLAB

**Option A: Raw MEX calls**
```matlab
id = bindings("create", "myCalc");
bindings("get_name", id)       % "myCalc"
bindings("add", id, 2, 3)     % 5
bindings("power", id, 2, 10)  % 1024
bindings("destroy", id);
```

**Option B: Dynamic MexObject (no MATLAB wrapper code needed)**
```matlab
calc = mexforge.MexObject(@bindings, "myCalc");
calc.add(2, 3)                % 5 — auto-dispatched to C++
calc.power(2, 10)             % 1024
calc.availableMethods()       % list all methods with descriptions
calc.showHelp("add")          % show args, types, return type
clear calc;                   % automatic cleanup
```

---

## Features

- **Three binding tiers** — from zero-boilerplate to full control
- **Compile-time signature extraction** — argument types derived automatically from method pointers
- **Automatic marshalling** — bidirectional conversion between MATLAB and C++ types via `if constexpr`
- **Generic object store** — manage multiple instances of your wrapped class with `unique_ptr` ownership
- **Configurable logging** — compile-time elimination, runtime level control, buffered MATLAB output, file logging
- **Built-in control commands** — log level, function listing, store management — all callable from MATLAB
- **Header-only** — no build step for the library itself, just `#include` and go
- **Cross-platform** — Windows (MSVC), macOS (Clang/Xcode), Linux (GCC)
- **Pure C++17** — no compiler extensions, standard include guards throughout

---

## Binding Tiers

### Tier 1: `bind_auto` — Automatic 1:1 binding

Extracts the method signature at compile time. Arguments are marshalled automatically. No code to write.

```cpp
b.bind_auto<&MyClass::getRate>("get_rate");
b.bind_auto<&MyClass::setRate>("set_rate");
```

### Tier 2: `bind_lambda` — Custom logic with auto-marshalling

For methods that need argument transformation, optional parameter handling, or combining multiple calls.

```cpp
b.bind_lambda<double, double, std::string>("compute",
    [](MyClass& obj, double a, double b, std::string op) -> double {
        return obj.compute(a, b, op);
    });
```

### Tier 3: `bind_custom` — Full control

Subclass `CustomRunner<T>` for complex scenarios like buffer handling, dynamic type dispatch, or multi-output functions.

```cpp
class StreamHandler : public mexforge::CustomRunner<MyDevice> {
    void execute(MyDevice& dev,
                 matlab::mex::ArgumentList outputs,
                 matlab::mex::ArgumentList inputs,
                 matlab::data::ArrayFactory& factory,
                 mexforge::Logger& logger) override
    {
        // Full access to raw MATLAB arrays
    }
};

b.bind_custom<StreamHandler>("stream");
```

---

## Dynamic MexObject

`mexforge.MexObject` is a generic MATLAB class that wraps **any** MexForge library without writing a single line of MATLAB wrapper code. The C++ bindings define the interface — MATLAB discovers it at runtime.

```matlab
% Works with ANY MexForge library — no per-library .m file needed
obj = mexforge.MexObject(@my_mex_lib, constructor_args...);

% Methods are dispatched dynamically to C++
obj.someMethod(arg1, arg2);

% Introspection
obj.availableMethods()        % List all methods with descriptions
obj.showHelp("someMethod")    % Show args, types, return type

% Tab-completion support
mexforge.generate_signatures(@my_mex_lib);  % Generate autocomplete JSON
```

### Documentation via `.doc()`

Add metadata to your C++ bindings for help texts and type checking:

```cpp
b.bind_auto<&MyClass::setRate>("set_rate")
    .doc("Set the sample rate in Hz",
         {{"rate", "double", true}, {"channel", "int32", false}})

 .bind_auto<&MyClass::getRate>("get_rate")
    .doc("Get the current sample rate", {}, "double");
```

This enables:
- `obj.showHelp("set_rate")` → shows description, args, types
- `obj.checkArgs("set_rate", ...)` → validates before MEX call
- `mexforge.generate_signatures(...)` → tab-completion in MATLAB editor

### Full Live Editor Argument Hints

`MexObject` provides command-window tab-completion (method names via `obj.<Tab>`) out of the box. For **argument hints inside the Live Editor** (the `f(x, |` popup that shows parameter names and types), MATLAB's static analyzer needs a real `.m` file on the MATLAB path — it does not execute `addpath` calls inside scripts.

The workflow is:

**Step 1 — Generate once (after each recompile):**

```matlab
addpath('/path/to/MexForge/matlab');   % +mexforge package
addpath('/path/to/your/bindings/');    % where bindings.mex* lives

mexforge.generate_signatures(@bindings, '/path/to/your/bindings/');
% Creates:
%   bindings_obj.m            — standalone handle class with all method stubs
%   functionSignatures.json   — argument hints for the MATLAB editor
```

**Step 2 — Commit the generated files** alongside your bindings:

```
your_project/
├── bindings.cpp
├── bindings.mexmaca64      ← compiled MEX
├── bindings_obj.m          ← commit this  ✓
├── functionSignatures.json ← commit this  ✓
└── matlab/demo.m
```

**Step 3 — Use the generated class** instead of `MexObject`:

```matlab
calc = bindings_obj("myCalc");   % editor resolves the type statically
calc.add(2, 3)                   % Live Editor shows (a, b) hint
calc.compute(10, 3, "div")       % shows (a, b, op) hint
```

Why a standalone class (not a subclass of `MexObject`)?  
MATLAB's static analyzer stops resolving method signatures when it sees a custom `subsref` in a base class. `bindings_obj` is a plain `handle` subclass — no dynamic dispatch, just explicit method stubs — so the Live Editor can analyze it fully.

> **Note:** `bindings_obj.m` and `functionSignatures.json` are project-specific generated files. Commit them to your project repository. Do **not** add them to `.gitignore`.

---

## Logging

MexForge includes a logging system with compile-time elimination and runtime control.

```matlab
% Control from MATLAB
bindings("__log_level", "debug");      % Set runtime level
bindings("__log_level", "off");        % Disable (~1ns per call)
bindings("__log_file", "debug.log");   % Log to file (fast, no MATLAB roundtrip)
bindings("__log_matlab", false);       % Disable MATLAB console output
bindings("__log_buffer", true);        % Buffer mode: collect, flush once
bindings("__log_flush");               % Flush buffered messages
```

| Mode | Overhead | MATLAB controllable |
|---|---|---|
| Compile-time off | Zero (code eliminated) | No (build flag) |
| Runtime off | ~1ns (`if` check) | `__log_level`, `"off"` |
| File only | ~1 us | `__log_matlab`, `false` |
| Buffered MATLAB | ~1 us + flush | `__log_buffer`, `true` |
| Direct MATLAB | ~100 us/msg | Default |

Compile-time minimum level via build flag:

```
-DMEXFORGE_MIN_LOG_LEVEL=2   # Strip trace (0) and debug (1) from binary
```

---

## Built-in Control Commands

Every MexForge library automatically supports:

| Command | Description |
|---|---|
| `__log_level` | Get/set runtime log level |
| `__log_file` | Enable file logging |
| `__log_matlab` | Enable/disable MATLAB console output |
| `__log_buffer` | Enable/disable buffered logging |
| `__log_flush` | Flush buffered log messages |
| `__list_functions` | List all registered function names |
| `__store_clear` | Clear all stored objects |
| `__store_size` | Get number of stored objects |

---

## Type Mapping

| C++ Type | MATLAB Type | Automatic |
|---|---|---|
| `double`, `float` | `double`, `single` | Yes |
| `int32_t`, `uint32_t`, `int64_t`, `uint64_t` | `int32`, `uint32`, `int64`, `uint64` | Yes |
| `bool` | `logical` | Yes |
| `std::string` | `string` | Yes |
| `std::complex<T>` | `complex` | Yes |
| `std::vector<T>` | Array | Yes |
| `std::optional<T>` | Optional argument | Yes |
| Custom structs | `struct` | Via `StructMarshaller<T>` specialization |

---

## Project Structure

```
MexForge/
├── include/mexforge/
│   ├── mexforge.hpp              # Main include
│   └── core/
│       ├── types.hpp             # Type traits, optional/vector detection
│       ├── marshaller.hpp        # MATLAB <-> C++ conversion
│       ├── method_traits.hpp     # Compile-time signature extraction
│       ├── object_store.hpp      # Generic object lifecycle (unique_ptr)
│       ├── logger.hpp            # Multi-level logging system
│       ├── runner.hpp            # Tier 1/2/3 runners
│       ├── registry.hpp          # Function registry + metadata + fluent builder
│       └── gateway.hpp           # MEX entry point + control commands
├── matlab/+mexforge/
│   ├── MexObject.m               # Dynamic wrapper (no per-library code needed)
│   └── generate_signatures.m     # Tab-completion generator
├── test/
│   ├── mock_matlab/              # Mock MATLAB headers for testing without MATLAB
│   ├── test_types.cpp
│   ├── test_method_traits.cpp
│   ├── test_object_store.cpp
│   ├── test_logger.cpp
│   └── test_registry.cpp
├── examples/simple_math/         # Complete working example
│   ├── math_lib.hpp              # Pure C++ library
│   ├── bindings.cpp              # MexForge bindings (all 3 tiers + metadata)
│   ├── make.m                    # MATLAB build script (cross-platform)
│   └── matlab/
│       ├── +math/calculator.m    # Traditional wrapper (optional)
│       └── demo_dynamic.m        # Dynamic MexObject demo
└── CMakeLists.txt
```

---

## Requirements

- **C++17** compiler (MSVC 2017+, GCC 8+, Clang/Xcode 15+)
- **MATLAB R2018b+** (for building MEX files — not needed for running tests)

---

## Origin

This project builds on ideas from my master's thesis at Darmstadt University of Applied Sciences (h_da) in 2020. Back then, I needed a MEX interface for a specific C++ library (the [Ettus Research UHD](https://github.com/EttusResearch/uhd) driver for Software Defined Radio hardware) and ended up building a semi-generic abstraction layer around it — a factory pattern with template-based argument validation that made it possible to expose 200+ C++ functions to MATLAB with relatively little per-function code.

That project ([MatlabUhdApi](https://github.com/nerdyflogrammer/MatlabUhdApi)) worked, but the generic parts were still entangled with the UHD-specific code. MexForge is the evolution of that idea: a **standalone, library-agnostic framework** that lets you wrap *any* C++ library for MATLAB, using modern C++17 metaprogramming to push as much work as possible to compile time.

---

## Support

If MexForge saves you time, consider supporting the project:

[![Buy Me a Coffee](https://img.shields.io/badge/Buy%20Me%20a%20Coffee-support-orange?style=for-the-badge&logo=buy-me-a-coffee)](https://buymeacoffee.com/nerdyflogrammer)

---

## Citation

If you use MexForge in academic work, please cite it:

```bibtex
@software{westmeier2026mexforge,
  author  = {Westmeier, Florian},
  title   = {{MexForge}: A Header-Only C++17 Library for MATLAB MEX Interfaces},
  year    = {2026},
  url     = {https://github.com/nerdyflogrammer/MexForge},
  doi     = {10.5281/zenodo.19560403},
  license = {Apache-2.0}
}
```

A `CITATION.cff` is included — GitHub shows a **"Cite this repository"** button automatically.

---

## License

Apache 2.0 — see [LICENSE](LICENSE)
