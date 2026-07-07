# GitHub Copilot Instructions for StarterCpp

This document provides context and guidelines for GitHub Copilot when working with the StarterCpp project.

## Project Overview

StarterCpp is a C++20 starter project template using:
- **Build System**: CMake 3.25+ with presets
- **Dependency Management**: Container/system packages
- **Compiler**: GCC 13+ or Clang 15+ (Linux container)
- **Testing**: Google Test
- **Dependencies**: spdlog, protobuf, ZeroMQ (cppzmq), CZMQ, Zyre, Eclipse Cyclone DDS, Crow

## Project Structure

```
StarterCpp/
├── src/
│   ├── apps/               # Executables
│   │   ├── Omniscope/          # Web-based IPC traffic inspector
│   │   │   ├── ITransport.h          # Abstract transport interface
│   │   │   ├── TransportDds.h/.cpp    # DDS transport (pImpl)
│   │   │   ├── TransportZyre.h/.cpp   # Zyre protobuf transport (pImpl)
│   │   │   ├── PlaybackEngine.h/.cpp # Recording playback engine
│   │   │   ├── OmniscopeApp.h/.cpp   # Crow HTTP/WS orchestrator (pImpl)
│   │   │   ├── CrowCompat.h          # C++20 atomic shim for Crow
│   │   │   ├── main.cpp              # Entry point
│   │   │   └── web/monitor.html      # Embedded single-page UI
│   │   ├── TestApps/           # Test/demo executables
│   │   │   ├── ZyrePublisherTest.cpp
│   │   │   ├── ZyreSubscriberTest.cpp
│   │   │   ├── HighBandwidthPublisherTester.cpp
│   │   │   ├── HighBandwidthSubscriberTester.cpp
│   │   │   ├── DDSPublisherTest.cpp
│   │   │   ├── DDSSubscriberTest.cpp
│   │   │   └── Vita49RoundTripTest.cpp
│   │   └── Tools/              # Utility/benchmark executables
│   │       ├── Vita49PerfBenchmark.cpp
│   │       └── Vita49FileCodec.cpp
│   └── libs/               # Libraries
│       ├── CommonUtils/    # Common utility library
│       │   ├── GeneralLogger.h/.cpp  # Async spdlog wrapper with macros
│       │   ├── Timer.h/.cpp          # Basic timer class
│       │   ├── SnoozableTimer.h/.cpp # Timer with snooze capability
│       │   └── DataHandler.h         # Data handling (header-only)
│       ├── PubSub/         # Publish-Subscribe library (Zyre + UDP multicast)
│       │   ├── ZyreNode.h/.cpp            # Base Zyre node class
│       │   ├── ZyrePublisher.h/.cpp       # Zyre-based publisher
│       │   ├── ZyreSubscriber.h/.cpp      # Zyre-based subscriber
│       │   ├── HighBandwidthPublisher.h/.cpp   # UDP multicast publisher
│       │   └── HighBandwidthSubscriber.h/.cpp  # UDP multicast subscriber
│       ├── CycloneDDS/     # DDS pub/sub library (Eclipse Cyclone DDS)
│       │   ├── DDSTopicConfig.h       # Centralized topic/QoS registry
│       │   ├── CycloneDDSConfig.h/.cpp # DDS domain/participant configuration
│       │   ├── DDSPublisher.h         # Template DDS publisher (header-only)
│       │   ├── DDSSubscriber.h        # Template DDS subscriber (header-only)
│       │   ├── generate_dds_json_helpers.py  # IDL-to-JSON helper generator
│       │   └── idl/                   # IDL message definitions
│       │       ├── SensorData.idl
│       │       ├── Command.idl
│       │       ├── TrackData.idl
│       │       └── MessageHeader.idl
│       ├── Vita49_2/       # VITA 49.2 signal data packet codec
│       │   ├── PacketHeader.h/.cpp
│       │   ├── SignalDataPacket.h/.cpp
│       │   ├── ContextPacket.h/.cpp
│       │   ├── Vita49Codec.h/.cpp
│       │   ├── Vita49Types.h
│       │   └── ByteSwap.h
│       └── proto/          # Protocol buffer library
│           ├── ProtoTopicConfig.h        # Constexpr topic enum + string registry
│           ├── ProtoJsonDispatch.h       # Runtime proto binary↔JSON dispatch
│           └── proto-messages/ # .proto source files
│               ├── sensor_reading.proto
│               ├── sensor_data_batch.proto
│               ├── command.proto
│               ├── command_response.proto
│               ├── application_config.proto
│               └── message_header.proto
├── tests/                  # Unit tests
│   ├── CommonUtilsTests/   # Tests for CommonUtils library
│   ├── ProtoTests/         # Tests for Proto library
│   ├── PubSubTests/        # Tests for PubSub library
│   ├── DDSTests/           # Tests for CycloneDDS library
│   └── Vita49_2Tests/      # Tests for Vita49_2 library
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

### Adding a New PubSub Class

1. Create `src/libs/PubSub/NewClass.h`
2. Create `src/libs/PubSub/NewClass.cpp` (auto-discovered via `file(GLOB)`)
3. Create `tests/PubSubTests/NewClassUt.cpp` (auto-discovered via `file(GLOB)`)
4. Re-run CMake configure to pick up new files

### Adding a New CycloneDDS Class

1. Create `src/libs/CycloneDDS/NewClass.h`
2. Create `src/libs/CycloneDDS/NewClass.cpp` (CycloneDDSLib is a STATIC library)
3. Create `tests/DDSTests/NewClassUt.cpp` (auto-discovered via `file(GLOB)`)
3. Re-run CMake configure to pick up new files

### Adding a New DDS IDL Message

1. Create or edit file in `src/libs/CycloneDDS/idl/` directory
2. IDL files are auto-discovered via glob in `src/libs/CycloneDDS/CMakeLists.txt`
3. Include generated header as `#include "MessageName.hpp"`
4. Re-run CMake configure to pick up new files

### Adding a New Vita49_2 Class

1. Create `src/libs/Vita49_2/NewClass.h`
2. Create `src/libs/Vita49_2/NewClass.cpp` (auto-discovered via `file(GLOB)`)
3. Create `tests/Vita49_2Tests/NewClassUt.cpp` (auto-discovered via `file(GLOB)`)
4. Re-run CMake configure to pick up new files

### Adding a New Proto Message

1. Create or edit file in `src/libs/proto/proto-messages/` directory
2. Proto files are auto-discovered via glob in `src/libs/proto/CMakeLists.txt`
3. Include generated header as `#include "message_name.pb.h"`

### Adding a New Application

1. Create `src/apps/new_app_main.cpp`
2. Add to `src/apps/CMakeLists.txt`:
   ```cmake
   add_executable(new_app new_app_main.cpp)
   target_link_libraries(new_app PRIVATE CommonUtils ...)
   ```

### Adding a New Omniscope Transport

1. Create `src/apps/Omniscope/NewTransport.h` and `NewTransport.cpp`
2. Implement the `Omniscope::ITransport` interface (see `TransportDds` for reference)
3. Instantiate in `src/apps/Omniscope/main.cpp` and push into the transports vector
4. Source files are auto-discovered via `file(GLOB)` in `Omniscope/CMakeLists.txt`
5. Re-run CMake configure to pick up new files

## Build Commands

```bash
# Configure
cmake --preset container-debug

# Build
cmake --build --preset container-debug

# Test
ctest --preset container-debug

# Coverage
cmake --build --preset coverage --target CommonUtilsCoverage
```

## CMake Targets

- `CommonUtils` - CommonUtils shared library
- `PubSubLib` - PubSub shared library (Zyre and HighBandwidth messaging)
- `ProtoLib` - Protobuf library (alias: `StarterCpp::proto`)
- `CycloneDDSLib` - DDS library (STATIC, wrappers + generated IDL types + JSON helpers)
- `DDSMessages` - Generated IDL C++ types (linked by CycloneDDSLib)
- `Vita49_2` - VITA 49.2 signal data packet codec shared library
- `ZyrePublisher` - Zyre publisher test application
- `ZyreSubscriber` - Zyre subscriber test application
- `HighBandwidthPublisher` - UDP multicast publisher test application
- `HighBandwidthSubscriber` - UDP multicast subscriber test application
- `DDSPublisher` - DDS publisher test application
- `DDSSubscriber` - DDS subscriber test application
- `Vita49RoundTripTest` - VITA 49.2 round-trip test application
- `Vita49PerfBenchmark` - VITA 49.2 performance benchmark application
- `Vita49FileCodec` - VITA 49.2 file codec application
- `Omniscope` - Web-based IPC traffic inspector (Crow HTTP/WebSocket)
- `CommonUtilsTests` - CommonUtils unit tests
- `ProtoTests` - Proto unit tests
- `PubSubTests` - PubSub unit tests
- `DDSTests` - DDS unit tests
- `Vita49_2Tests` - VITA 49.2 unit tests
- `coverage` - Unified coverage report target (when `ENABLE_COVERAGE=ON`)
- `CommonUtilsCoverage` - CommonUtils coverage report target (when `ENABLE_COVERAGE=ON`)
- `PubSubCoverage` - PubSub coverage report target (when `ENABLE_COVERAGE=ON`)

## Dependencies Available

When suggesting code, these libraries are available:

| Library | Include | Namespace/Usage |
|---------|---------|------------------|
| spdlog | `<spdlog/spdlog.h>` | `spdlog::info()` or `CommonUtils::GeneralLogger` |
| protobuf | `"message.pb.h"` | `messages::MessageType` |
| ZeroMQ | `<zmq.hpp>` | `zmq::context_t`, `zmq::socket_t` |
| CZMQ | `<czmq.h>` | `zsock_t`, `zactor_t` |
| Zyre | `<zyre.h>` | `zyre_t` |
| Cyclone DDS | `<dds/dds.hpp>` | `dds::domain::DomainParticipant`, `dds::pub::DataWriter` |
| CycloneDDS wrappers | `"CycloneDDS/DDSPublisher.h"` | `CycloneDDS::DDSPublisher<T>`, `CycloneDDS::DDSSubscriber<T>` |
| CycloneDDS config | `"CycloneDDS/DDSTopicConfig.h"` | `CycloneDDS::DDSTopicConfig`, `CycloneDDS::TopicEntry` |
| Crow | `<crow.h>` | `crow::SimpleApp`, `crow::json::wvalue` (include via `CrowCompat.h` for C++20) |
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
