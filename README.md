# A JUCE port of Open303

A Cmake JUCE port of [Robin Schmidt`s Open303](https://github.com/RobinSchmidt/Open303), a Roland TB-303 clone plugin.

![JC-303 Screenshot](https://raw.githubusercontent.com/midilab/jc303/main/img/jc303.png)

This software is licensed under the GNU General Public License version 3 (GPLv3).

The Open303 engine part of this software is also licensed under the MIT License.

## Download

Supports Windows, Linux and macOS. You may find CLAP, VST3, LV2 and AU formats available to download.

MacOS Intel x64 and M1: [jc303-macos-universal-plugins.zip](https://github.com/midilab/jc303/releases/download/v0.9.1/jc303-0.9.1-macos-universal-plugins.zip)
Windows Intel x64: [jc303-win64-plugins.zip](https://github.com/midilab/jc303/releases/download/v0.9.1/jc303-0.9.1-win64-plugins.zip)
Linux Intel x64: [jc303-linux64-plugins.zip](https://github.com/midilab/jc303/releases/download/v0.9.1/jc303-0.9.1-linux64-plugins.zip)

**Installation**: The platform zip pack will contain a folder per plugin format, just pick the format you want to install and copy the content of the folder to your OS plugin format folder.

**Known issues**: MacOS AU not loading in M1 tested with ableton11, all other MacOS plugins format tested and working fine on M1!

## Build

Generate the project first before you compile.

### Apple Xcode

To generate an **Xcode** project, run:

```sh
cmake -B build -G Xcode -D CMAKE_OSX_ARCHITECTURES=arm64\;x86_64 -D CMAKE_OSX_DEPLOYMENT_TARGET=10.13
```

The `-D CMAKE_OSX_ARCHITECTURES=arm64\;x86_64` flag is required to build universal binaries.

The `-D CMAKE_OSX_DEPLOYMENT_TARGET=10.13` flag sets the minimum MacOS version to be supported.

### Windows Visual Studio

To generate a **Visual Studio 2022 (17)** project, run:

```sh
cmake -B build -G "Visual Studio 17"
```

### GNU Linux

Install the dependecies:

```sh
sudo apt install build-essential gcc cmake libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev libfreetype6-dev libasound2-dev
```

To generate a **Linux CMake** project, run:

```sh
cmake -B build
```

## Compile

To compiled the generated IDE project from the command line, run:

```sh
cmake --build build --config Release
```

## Roadmap

1. ~~Binary release for MacOS, Windows and Linux~~
2. ~~Graphical User Interface~~
3. Filter type and internal parameters for engine tunning exposed to GUI
4. Preset Support
5. Step Sequencer
