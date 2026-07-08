# Development Guide

This document covers the development workflow, coding standards, and contribution guidelines for the ContainerizedCPP project.

## Development Environment Setup

### Windows Host + Dev Containers

Use Docker Desktop with the WSL2 backend and develop inside the provided Dev
Container. Native host toolchain workflows are not the supported path for this project.

Prerequisites:

- Docker Desktop installed
- Docker Desktop setting "Use the WSL 2 based engine" enabled
- VS Code extension "Dev Containers" installed
- Repository opened in VS Code from a location available to Docker Desktop/WSL

Recommended first run:

1. In VS Code, run "Dev Containers: Rebuild and Reopen in Container"
2. Wait for the initial image build to complete
3. Run container presets:
   - `cmake --preset container-debug`
   - `cmake --build --preset container-debug`
   - `ctest --preset container-debug`

If the container fails during build, check the Dev Containers log first and verify Docker Desktop
has enough memory and disk allocated for a large first build.

### Recommended IDE

**Visual Studio Code** with these extensions:
- C/C++ (Microsoft)
- CMake Tools (Microsoft)
- clangd (LLVM)
- GitHub Copilot
- Error Lens

### VS Code Configuration

Create `.vscode/settings.json`:
```json
{
   "cmake.configureOnOpen": true,
   "cmake.buildDirectory": "${workspaceFolder}/build/container-debug",
   "C_Cpp.default.configurationProvider": "ms-vscode.cmake-tools",
   "editor.formatOnSave": true,
   "clang-format.executable": "clang-format",
   "[cpp]": {
      "editor.defaultFormatter": "xaver.clang-format"
   }
}
```

## Workflow

### 1. Create a Feature Branch

```bash
git checkout -b feature/my-feature
```

### 2. Make Changes

Follow the coding standards below.

### 3. Format Code

```bash
# Format all source files
find src tests -name "*.cpp" -o -name "*.hpp" | xargs clang-format -i
```

### 4. Run Static Analysis

```bash
# Rebuild with clang-tidy enabled
cmake --preset container-debug
cmake --build --preset container-debug
```

### 5. Run Tests

```bash
ctest --preset container-debug --output-on-failure
```

### 6. Check Coverage

```bash
cmake --preset container-coverage
cmake --build --preset container-coverage
cmake --build --preset container-coverage --target coverage
```

### 7. Commit and Push

```bash
git add .
git commit -m "feat: add my feature"
git push origin feature/my-feature
```

## Coding Standards

### File Organization

- Headers: `src/libs/<lib>/<Header>.h`
- Sources: `src/libs/<lib>/<Source>.cpp`
- Tests: `tests/<lib>Tests/<Source>Ut.cpp`

### Naming Conventions

| Element           | Style      | Example         |
| ----------------- | ---------- | --------------- |
| Classes/Structs   | PascalCase | `SensorReading` |
| Functions/Methods | camelCase  | `processData()` |
| Variables         | camelCase  | `sensorValue`   |
| Member Variables  | _ prefix   | `_value`        |
| Static Members    | s_ prefix  | `s_instance`    |
| Constants         | UPPER_CASE | `MAX_RETRIES`   |
| Namespaces        | PascalCase | `CommonUtils`   |
| Files             | PascalCase | `AsyncQueue.h`  |

### Code Style

```cpp
// Use braces on new line (Allman style)
void processData()
{
   if (condition)
   {
      // 3-space indentation
      doSomething();
   }
}

// Use m_ prefix for member variables
class MyClass
{
public:
   void setValue(int value)
   {
      _value = value;
   }

private:
   int _value;
};

// Use explicit types or auto with care
auto result = calculateValue();  // OK when type is obvious
std::vector<int> numbers;        // Prefer explicit for clarity

// Use nullptr, not NULL or 0
if (pointer == nullptr)
{
   // ...
}

// Use override for virtual methods
class Derived : public Base
{
public:
   void virtualMethod() override;
};
```

### Header Guidelines

```cpp
#ifndef MYCLASS_H_
#define MYCLASS_H_

// Include order:
// 1. Corresponding header (for .cpp files)
// 2. Project headers
// 3. Third-party headers
// 4. System headers

#include "MyClass.h"

#include "CommonUtils/GeneralLogger.h"
#include "proto/messages.pb.h"

#include <spdlog/spdlog.h>
#include <zmq.hpp>
#include <dds/dds.hpp>

#include <memory>
#include <string>
#include <vector>
```

### Documentation

```cpp
/**
 * @brief Short description of the function
 *
 * Longer description if needed.
 *
 * @param param1 Description of first parameter
 * @param param2 Description of second parameter
 * @return Description of return value
 * @throws ExceptionType When this exception is thrown
 *
 * @example
 * @code
 * auto result = myFunction(1, "test");
 * @endcode
 */
int myFunction(int param1, std::string_view param2);
```

## Adding New Components

### Adding a New CommonUtils Class

1. Create header: `src/libs/CommonUtils/NewClass.h`
2. Create source: `src/libs/CommonUtils/NewClass.cpp`
3. Files are auto-discovered via `file(GLOB)` in `src/libs/CommonUtils/CMakeLists.txt`
4. Create test: `tests/CommonUtilsTests/NewClassUt.cpp`
5. Tests are auto-discovered via `file(GLOB)` in `tests/CommonUtilsTests/CMakeLists.txt`
6. Re-run CMake configure to pick up new files

### Adding a New PubSub Class

1. Create header: `src/libs/PubSub/NewClass.h`
2. Create source: `src/libs/PubSub/NewClass.cpp`
3. Files are auto-discovered via `file(GLOB)` in `src/libs/PubSub/CMakeLists.txt`
4. Create test: `tests/PubSubTests/NewClassUt.cpp`
5. Tests are auto-discovered via `file(GLOB)` in `tests/PubSubTests/CMakeLists.txt`
6. Re-run CMake configure to pick up new files

### Adding a New CycloneDDS Class

1. Create header: `src/libs/CycloneDDS/NewClass.h`
2. Create source: `src/libs/CycloneDDS/NewClass.cpp` (CycloneDDSLib is a STATIC library)
3. Create test: `tests/DDSTests/NewClassUt.cpp`
3. Tests are auto-discovered via `file(GLOB)` in `tests/DDSTests/CMakeLists.txt`
4. Re-run CMake configure to pick up new files

### Adding a New DDS IDL Message

1. Create or edit file in `src/libs/CycloneDDS/idl/` directory
2. IDL files are auto-discovered via glob in `src/libs/CycloneDDS/CMakeLists.txt`
3. Include generated header as `#include "MessageName.hpp"`
4. Re-run CMake configure to pick up new files

### Adding a New Vita49_2 Class

1. Create header: `src/libs/Vita49_2/NewClass.h`
2. Create source: `src/libs/Vita49_2/NewClass.cpp`
3. Files are auto-discovered via `file(GLOB)` in `src/libs/Vita49_2/CMakeLists.txt`
4. Create test: `tests/Vita49_2Tests/NewClassUt.cpp`
5. Tests are auto-discovered via `file(GLOB)` in `tests/Vita49_2Tests/CMakeLists.txt`
6. Re-run CMake configure to pick up new files

### Adding a New Proto File

1. Create proto: `src/libs/proto/proto-messages/new_message.proto`
2. Files are auto-discovered via `file(GLOB)` in `src/libs/proto/CMakeLists.txt`
3. Re-run CMake configure to pick up new files

### Adding a New Application

1. Create source: `src/apps/new_app_main.cpp`
2. Add to `src/apps/CMakeLists.txt`:
   ```cmake
   add_executable(new_app
      new_app_main.cpp
   )

   target_link_libraries(new_app
      PRIVATE
         CommonUtils
         # other dependencies: PubSubLib, ProtoLib, CycloneDDSLib, Vita49_2
   )
   ```

### Adding a New Omniscope Transport

1. Create `src/apps/Omniscope/NewTransport.h` and `NewTransport.cpp`
2. Implement the `Omniscope::ITransport` interface (see `TransportDds` or `TransportZyre` for reference)
3. In `src/apps/Omniscope/main.cpp`, instantiate the transport and register it:
   ```cpp
   auto newTransport = std::make_unique<Omniscope::NewTransport>(/* ... */);
   app.addTransport(std::move(newTransport));
   ```
4. Source files are auto-discovered via `file(GLOB)` in `Omniscope/CMakeLists.txt`
5. Rebuild — the monitor automatically discovers topics from all registered transports

## Testing Guidelines

### Test Organization

```cpp
// Use Test Fixtures for common setup
class MyClassTest : public ::testing::Test
{
protected:
   void SetUp() override
   {
      // Common setup
   }

   void TearDown() override
   {
      // Common cleanup
   }

   MyClass m_instance;
};

// Name tests descriptively
TEST_F(MyClassTest, MethodName_Condition_ExpectedResult)
{
   // Arrange
   // ...

   // Act
   auto result = m_instance.method();

   // Assert
   EXPECT_EQ(result, expected);
}
```

### Test Coverage Goals

- Aim for 80%+ line coverage
- Cover all public methods
- Test edge cases and error conditions
- Test thread safety for concurrent classes

## Commit Message Format

Use conventional commits:

```
<type>(<scope>): <subject>

<body>

<footer>
```

Types:
- `feat`: New feature
- `fix`: Bug fix
- `docs`: Documentation
- `style`: Formatting
- `refactor`: Code restructuring
- `test`: Adding tests
- `chore`: Maintenance

Examples:
```
feat(common_utils): add snooze capability to Timer

fix(timer): prevent race condition on stop

docs(readme): update build instructions
```

## CI/CD Pipeline

The GitHub Actions workflow:

1. **Build**: Compiles on Linux
2. **Test**: Runs all unit tests
3. **Coverage**: Generates and uploads coverage reports
4. **Quality**: Checks code formatting

All checks must pass before merging.
