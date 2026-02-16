# XenSequencer 🎶

![XenSequencer](/docs/img/title-screenshot.png)

[XenSequencer](https://github.com/a-n-t-h-o-n-y/XenSequencer) (XS) is a VST MIDI sequencer. It intends to be a composition tool where user input is quick and ideas can be generated, iterated, and edited. It takes inspiration from the `vim` text editor, all input is keyboard based, with a built in `:` key command line and various insert modes. XS supports `.scl` tuning files via MPE.

## Documentation
- [User Guide](docs/user_guide.md)
- [Command Reference](docs/command_reference.md)
- [Keybindings Reference](docs/keybindings_reference.md)

## Building from Source

### Prerequisites
- A C++20 Compiler
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

## License
This project is licensed under the AGLPv3 License - see the [LICENSE](LICENSE) file for details.