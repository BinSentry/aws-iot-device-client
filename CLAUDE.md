# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build Commands

### Building the main application:
```bash
mkdir build && cd build
cmake ../
cmake --build . --target aws-iot-device-client
```

### Building and running tests:
```bash
cmake --build . --target test-aws-iot-device-client
./test/test-aws-iot-device-client
```

### Running specific tests:
```bash
./test/test-aws-iot-device-client --gtest_filter="*TestName*"
```

### IDE build (for cmake-build-debug directory):
```bash
cmake --build /path/to/cmake-build-debug --target test-aws-iot-device-client -j 18
```

## Architecture Overview

### Core Architecture
- **main.cpp**: Application entry point that initializes features and manages the main execution loop
- **Feature Interface**: All IoT features implement the `Feature` abstract class with `start()`, `stop()`, and `getName()` methods
- **FeatureRegistry**: Centralized registry that manages all active features and orchestrates their lifecycle
- **SharedCrtResourceManager**: Manages shared AWS CRT resources (MQTT connections, allocators, etc.)
- **Config System**: JSON-based configuration loading via `PlainConfig` class

### Feature System
The codebase uses a modular feature architecture where each AWS IoT service is implemented as a separate feature:

- **Jobs Feature**: Handles AWS IoT Jobs for remote device management
- **Device Defender Feature**: Collects and reports device metrics
- **Fleet Provisioning Feature**: Manages device certificate provisioning
- **Secure Tunneling Feature**: Enables secure access to devices
- **Shadow Features**: Manages device state via AWS IoT Device Shadows
- **Sensor Publish Feature**: Publishes sensor data over MQTT
- **Custom Features**: User-defined features (like BinSentry S3 Presigned URL)

### Build System Features
Features can be conditionally compiled using CMake options:
- `EXCLUDE_JOBS`, `EXCLUDE_DD`, `EXCLUDE_ST`, `EXCLUDE_FP`, etc.
- Custom features controlled by options like `EXCLUDE_CUSTOM` and `EXCLUDE_BINSENTRY_COMM_BIN_S3_PRESIGNED_URL`

### Key Directories
- `source/`: Main application code
- `source/config/`: Configuration management
- `source/util/`: Utility functions (file handling, logging, MQTT utilities)
- `source/logging/`: Logging framework with multiple output targets
- `source/custom/`: Custom feature implementations
- `test/`: Google Test-based unit tests
- `docs/`: Comprehensive documentation

### Testing Architecture
- Uses Google Test framework with Google Mock for mocking
- Test files mirror source structure in `test/` directory
- Tests can be built conditionally based on feature inclusion
- Custom features require proper CMakeLists.txt integration for both source and test files

### Configuration System
- JSON-based configuration loaded via `PlainConfig::LoadFromJson()`
- Common config elements: endpoint, certificates, thing-name, logging settings
- Feature-specific configuration sections

### MQTT and AWS Integration
- Built on aws-iot-device-sdk-cpp-v2
- MQTT connection management via SharedCrtResourceManager
- QoS levels typically use `AWS_MQTT_QOS_AT_LEAST_ONCE`
- Topic naming follows AWS IoT conventions

## Development Notes

- All source files must end with an empty line
- When adding custom features, ensure both source files and test files are properly integrated in CMakeLists.txt
- Use the existing logging framework (`LoggerFactory`) instead of direct console output
- Follow the established namespace structure: `Aws::Iot::DeviceClient::[FeatureArea]`
- Custom features should be added under `source/custom/` with appropriate subdirectories

## Testing Requirements
- Always verify code changes by running relevant tests
- Build and run tests using cmake target `test-aws-iot-device-client`
- For custom features, tests may require additional dependencies (e.g., sdbus for D-Bus integration)
- Tests should use proper mocking to isolate units under test