# XenSequencer 🎶

![XenSequencer](/docs/img/title-screenshot.png)

[XenSequencer](https://github.com/a-n-t-h-o-n-y/XenSequencer) is a VST MIDI sequencer.It is opinionated and consists of a unique structure where sequences are built up by 'splitting' notes and sequences into copies of themselves. It supports various tunings and needs MPE support for polyphony in non-standard tunings. The interface is vim like, with keyboard only input and a command bar and keybindings for all actions.

## Documentation
- [User Guide](docs/user_guide.md)
- [Command Reference](docs/command_reference.md)
- [Keybindings Reference](docs/keybindings_reference.md)

## Building from Source

### Prerequisites
- A C++20 Compiler
- Boost Libraries
- CMake
- Git

### Build
```bash
git clone https://github.com/a-n-t-h-o-n-y/XenSequencer.git
cd XenSequencer
git submodule update --init --recursive
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . --target XenSequencer_VST3
```

## Installation
Move the XenSequencer VST to your system's VST3 folder. If building from source, the VST can be found in `XenSequencer/build/XenSequencer_artefacts/Release/VST3/`.

## Tests
Currently Unsupported.

```bash
cd build/
cmake --build . --target XenTests
./XenTests
```

## License
This project is licensed under the AGLPv3 License - see the [LICENSE](LICENSE) file for details.