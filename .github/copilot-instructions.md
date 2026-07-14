# GitHub Copilot Instructions for ContainerizedCPP

This document provides context and guidelines for GitHub Copilot when working with the ContainerizedCPP project.

## Project Overview

ContainerizedCPP is a C++20 DDS showcase — a web-based DDS traffic inspector
with dynamic topic discovery, recording, and playback (OmniscopeDds) plus a
two-app QoS demonstration (RadarDDSDemo), all on a single DDS implementation.
- **Build System**: CMake 3.25+ with presets
- **Dependency Management**: Container/system packages
- **Compiler**: GCC 13+ or Clang 15+ (Linux container)
- **Testing**: Google Test
- **Dependencies**: spdlog, Eclipse Cyclone DDS, Crow

## Project Structure

```
ContainerizedCPP/
├── src/
│   ├── apps/               # Executables
│   │   ├── OmniscopeDds/       # Web-based DDS traffic inspector, dynamic discovery
│   │   │   ├── ITransport.h          # Abstract transport interface
│   │   │   ├── PlaybackEngine.h/.cpp # Recording playback engine
│   │   │   ├── OmniscopeApp.h/.cpp   # Crow HTTP/WS orchestrator (pImpl)
│   │   │   ├── TransportDds.h/.cpp   # Dynamic DDS discovery + raw-CDR capture/replay
│   │   │   ├── main.cpp
│   │   │   └── web/                  # monitor.html, style.css, app.js (embedded UI)
│   │   └── RadarDDSDemo/       # Two-app CycloneDDS QoS profile demo
│   │       ├── Radar.cpp             # Radar sensor node
│   │       ├── Workstation.cpp       # Operator workstation
│   │       ├── RadarTopics.h/.cpp    # Per-topic QoS registry
│   │       └── idl/                  # Own IDL (Command, RadarTrack, etc.)
│   └── libs/               # Libraries
│       ├── CommonUtils/    # Common utility library
│       │   ├── GeneralLogger.h/.cpp  # Async spdlog wrapper with macros
│       │   ├── Timer.h/.cpp          # Basic timer class
│       │   ├── SnoozableTimer.h/.cpp # Timer with snooze capability
│       │   └── DataHandler.h         # Data handling (header-only)
│       └── CycloneDDS/     # DDS pub/sub library (Eclipse Cyclone DDS)
│           ├── DDSTopicConfig.h       # Centralized topic/QoS registry
│           ├── DDSPublisher.h         # Template DDS publisher (header-only)
│           ├── DDSSubscriber.h        # Template DDS subscriber (header-only,
│           │                          # event-driven via WaitSet/StatusCondition)
│           ├── BlobSertype.h/.cpp     # Type-unaware raw-CDR sertype (OmniscopeDds)
│           └── BuiltinTopicReader.h/.cpp # Dynamic topic discovery (OmniscopeDds)
│               # No message IDL of its own — each app supplies its own
│               # (see src/apps/RadarDDSDemo/idl/, tests/DDSTests/idl/)
├── tests/                  # Unit tests
│   ├── CommonUtilsTests/   # Tests for CommonUtils library
│   └── DDSTests/           # Tests for CycloneDDS library (own idl/ dir)
├── docs/                   # Documentation
└── .github/                # CI/CD and this file
```

## Coding Conventions

### Style Guide

- **Indentation**: 3 spaces (no tabs)
- **Braces**: Allman style (opening brace on new line)
- **Line length**: 100 characters maximum
- **Naming**:
  - Classes: `PascalCase`
  - Functions/Methods: `camelCase`
  - Variables: `camelCase`
  - Member variables: `_` prefix (e.g., `_value`)
  - Static members: `s_` prefix (e.g., `s_instance`)

### Code Example

```cpp
namespace CommonUtils
{

class MyClass
{
public:
   MyClass();

   void processData(int value);
   int getValue() const;

private:
   void internalMethod();

   int _value;
   static int s_counter;
};

} // namespace CommonUtils
```

### Header Structure

```cpp
#ifndef MYCLASS_H_
#define MYCLASS_H_

// Project headers
// Third-party headers
// System headers

#include "CommonUtils/GeneralLogger.h"

#include <spdlog/spdlog.h>

#include <memory>
#include <string>

// ... class definition ...

#endif // MYCLASS_H_
```

## Common Tasks

### Adding a New CommonUtils Class

1. Create `src/libs/CommonUtils/NewClass.h`
2. Create `src/libs/CommonUtils/NewClass.cpp` (auto-discovered via `file(GLOB)`)
3. Create `tests/CommonUtilsTests/NewClassUt.cpp` (auto-discovered via `file(GLOB)`)
4. Re-run CMake configure to pick up new files

### Adding a New CycloneDDS Class

1. Create `src/libs/CycloneDDS/NewClass.h`
2. Create `src/libs/CycloneDDS/NewClass.cpp` (CycloneDDSLib is a STATIC library)
3. Create `tests/DDSTests/NewClassUt.cpp` (auto-discovered via `file(GLOB)`)
3. Re-run CMake configure to pick up new files

### Adding a New DDS IDL Message

CycloneDDSLib ships no message IDL of its own. Each app owns its IDL:

1. Create or edit `.idl` files in your app's own `idl/` directory
2. In your app's `CMakeLists.txt`, glob the files and call `idlcxx_generate(TARGET YourAppMessages FILES ${IDL_FILES})` (see `src/apps/RadarDDSDemo/CMakeLists.txt`)
3. Include generated header as `#include "MessageName.hpp"`
4. Re-run CMake configure to pick up new files

### Adding a New Application

1. Create `src/apps/new_app_main.cpp`
2. Add to `src/apps/CMakeLists.txt`:
   ```cmake
   add_executable(new_app new_app_main.cpp)
   target_link_libraries(new_app PRIVATE CommonUtils ...)
   ```

### Adding a New OmniscopeDds Transport

1. Create `src/apps/OmniscopeDds/NewTransport.h` and `NewTransport.cpp`
2. Implement the `Omniscope::ITransport` interface (see `src/apps/OmniscopeDds/TransportDds.h/.cpp` for reference)
3. Instantiate it and register via `app.addTransport(...)` in `src/apps/OmniscopeDds/main.cpp`
4. Add the new `.cpp` to the explicit source list in `add_executable(OmniscopeDds ...)` in `src/apps/OmniscopeDds/CMakeLists.txt`
5. Re-run CMake configure to pick up new files

## Build Commands

```bash
# Configure
cmake --preset debug-san

# Build
cmake --build --preset debug-san

# Test
ctest --preset debug-san

# Coverage
cmake --build --preset debug-coverage --target CommonUtilsCoverage
```

## CMake Targets

- `CommonUtils` - CommonUtils shared library
- `CycloneDDSLib` - DDS library (STATIC, type-agnostic wrappers — no message IDL of its own)
- `OmniscopeDds` - Web-based DDS traffic inspector with dynamic topic discovery, recording, and playback
- `DDSTestMessages` - DDSTests' own generated IDL C++ types
- `RadarDDSMessages` - RadarDDSDemo's generated IDL C++ types
- `RadarTopics` - RadarDDSDemo's per-topic QoS registry (STATIC)
- `RadarDDSWorkstation` - RadarDDSDemo operator workstation application
- `RadarDDSRadar` - RadarDDSDemo radar sensor node application
- `CommonUtilsTests` - CommonUtils unit tests
- `DDSTests` - DDS unit tests
- `coverage` - Unified coverage report target (when `ENABLE_COVERAGE=ON`)
- `CommonUtilsCoverage` - CommonUtils coverage report target (when `ENABLE_COVERAGE=ON`)

## Dependencies Available

When suggesting code, these libraries are available:

| Library | Include | Namespace/Usage |
|---------|---------|------------------|
| spdlog | `<spdlog/spdlog.h>` | `spdlog::info()` or `CommonUtils::GeneralLogger` |
| Cyclone DDS | `<dds/dds.hpp>` | `dds::domain::DomainParticipant`, `dds::pub::DataWriter` |
| CycloneDDS wrappers | `"CycloneDDS/DDSPublisher.h"` | `CycloneDDS::DDSPublisher<T>`, `CycloneDDS::DDSSubscriber<T>` |
| CycloneDDS config | `"CycloneDDS/DDSTopicConfig.h"` | `CycloneDDS::DDSTopicConfig`, `CycloneDDS::TopicEntry` |
| Crow | `<crow.h>` | `crow::SimpleApp`, `crow::json::wvalue` |
| Google Test | `<gtest/gtest.h>` | `TEST()`, `EXPECT_EQ()` |

## Testing Patterns

```cpp
#include <gtest/gtest.h>
#include "MyClass.h"

// Test naming: TestSuiteName, TestName
TEST(MyClassTest, MethodName_Condition_ExpectedResult)
{
   // Arrange
   CommonUtils::MyClass instance;

   // Act
   auto result = instance.doSomething();

   // Assert
   EXPECT_EQ(result, expected);
}

// For tests needing setup/teardown, use fixtures:
class MyClassFixture : public ::testing::Test
{
protected:
   void SetUp() override { }
   void TearDown() override { }

   CommonUtils::MyClass _instance;
};

TEST_F(MyClassFixture, MethodName_WithFixture_ExpectedResult)
{
   EXPECT_TRUE(_instance.isValid());
}
```

## Error Handling Patterns

- Use exceptions for recoverable errors
- Use `std::optional` for values that may not exist
- Log errors using `GPERROR()` macro from GeneralLogger
- Use RAII for resource management

## Thread Safety

- Use `std::mutex` with `std::lock_guard` or `std::unique_lock`
- Use `std::atomic` for simple flags/counters
- The `SnoozableTimer` class provides thread-safe snooze functionality
- The `GeneralLogger` provides thread-safe async logging

## Important Notes

1. **No raw pointers for ownership** - Use `std::unique_ptr` or `std::shared_ptr`
2. **Prefer `std::string_view`** for read-only string parameters
3. **Use `[[nodiscard]]`** for functions whose return value should not be ignored
4. **Mark destructors `override`** in derived classes
5. **Use `= default`/`= delete`** for special member functions

## File Modification Guidelines

When modifying files:
- Always run clang-format before committing
- Add corresponding unit tests for new functionality
- Update documentation if adding public API
- Follow existing patterns in the codebase
