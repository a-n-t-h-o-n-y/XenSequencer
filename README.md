# XenSequencer

![XenSequencer](/docs/img/XS_v0.1.png)

## Table of Contents
- [Overview](#overview)
- [Documentation](#documentation)
- [Building from Source](#building-from-source)
  - [Prerequisites](#prerequisites)
  - [Build Steps](#build-steps)
- [Installation](#installation)
- [Running Tests](#running-tests)
- [Plugin Architecture](#plugin-architecture)
- [License](#license)

## Overview
[XenSequencer](https://github.com/a-n-t-h-o-n-y/XenSequencer) is a monophonic MIDI sequencer VST designed to help generate sequences programmatically. The plugin features a recursive structure where sequences are built by splitting existing notes. Currently, it supports keyboard input and is compatible with some DAWs (tested with Ableton, Bitwig).

## Documentation
- [User Guide](docs/user_guide.md)
- [Command Reference](docs/command_reference.md)
- [Keyboard Shortcuts](keys.yml)

## Building from Source

### Prerequisites
- A C++20 compliant compiler
- CMake 3.16 or higher
- Git

### Build Steps
1. Clone the repository:
    ```bash
    git clone https://github.com/a-n-t-h-o-n-y/XenSequencer.git
    ```
1. Navigate to the project directory:
    ```bash
    cd XenSequencer
    ```
1. Initialize submodules (dependencies):
    ```bash
    git submodule update --init --recursive
    ```
1. Create a build directory and navigate to it:
    ```bash
    mkdir build && cd build
    ```
1. Run CMake to generate the build system:
    ```bash
    cmake .. -DCMAKE_BUILD_TYPE=Release
    ```
1. Build the project:
    ```bash
    cmake --build .
    ```

## Installation
Move the XenSequencer VST to your system's VST3 folder. If building from source, the VST can be found in `XenSequencer/build/XenSequencer_artefacts/Release/VST3/`.

## Running Tests
XenSequencer uses [Catch2](https://github.com/catchorg/Catch2) for unit testing. To run the tests, navigate to the build directory and run the following command:

```bash
cd build/
cmake --build . --target XenTests
./XenTests
```

## Plugin Architecture

- **Framework**: The plugin is built using [JUCE](https://github.com/juce-framework/JUCE), a framework for audio software.
  
- **Programming Style**: The codebase attempts to adhere to a functional programming style, utilizing structs and pure, free functions whenever possible.

- **Command System**: 
  - Core to the plugin's functionality is a Command System, similar to a command-line interface.
  - Takes in string commands from the user to modify the state timeline.
  - The state timeline is a thread-safe data structure holding the complete state of each modification within the sequence, as a history.

- **User Interaction**:
  - All user inputs are channeled through the Command System as strings.
  - Any change in the state timeline triggers a complete GUI rebuild, which is lightweight enough to not introduce performance issues.

- **MIDI Generation**:
  - MIDI sequences for the current phrase are generated in bulk upon any modification to the state timeline or DAW state.
  - These sequences are stored and later accessed each time `processBlock` is called on `XenProcessor`, writing to the MIDI buffer when needed.

## License
This project is licensed under the AGLPv3 License - see the [LICENSE](LICENSE) file for details.