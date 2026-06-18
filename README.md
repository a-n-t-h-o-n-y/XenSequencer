# XenSequencer 🎶

![XenSequencer](/docs/img/title-screenshot.png)

[XenSequencer](https://github.com/a-n-t-h-o-n-y/XenSequencer) (XS) is a VST MIDI sequencer. It intends to be a composition tool where user input is quick and ideas can be generated, iterated, and edited. It takes inspiration from the `vim` text editor, all input is keyboard based, with a built in `:` key command line and various insert modes. XS supports `.scl` tuning files via MPE.

## Documentation
- [User Guide](docs/user_guide.md)
- [Command Reference](docs/command_reference.md)
- [Keybindings Reference](docs/keybindings_reference.md)
- [Core Testing Plan](docs/core_testing_plan.md)

## Building from Source

### Prerequisites
- A C++20 Compiler
- CMake
- Git
- Ninja

### Development Build
```bash
git clone https://github.com/a-n-t-h-o-n-y/XenSequencer.git
cd XenSequencer
git submodule update --init --recursive
./configure.sh
cmake --build build
ctest --test-dir build
```

The development build uses `build/`, `Debug`, and the frontend dev server by default.

### Release VST Build
Build the frontend first so `../xen-frontend/dist/index.html` exists, then run:

```bash
./configure.sh release
cmake --build build-release --target XenSequencer_VST3
```

To use a different frontend dist directory:

```bash
./configure.sh release -DXEN_WEB_UI_DIST_DIR=/path/to/xen-frontend/dist
```

Compiler overrides can be passed through the environment or as CMake cache values:

```bash
CC=clang CXX=clang++ ./configure.sh
./configure.sh -DCMAKE_C_COMPILER=/path/to/clang -DCMAKE_CXX_COMPILER=/path/to/clang++
```

## Installation
Move the XenSequencer VST to your system's VST3 folder. If building from source, the release VST can be found in `XenSequencer/build-release/XenSequencer_artefacts/Release/VST3/`.

## License
This project is licensed under the AGLPv3 License - see the [LICENSE](LICENSE) file for details.
