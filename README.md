# A JUCE port of Open303

A Cmake JUCE port of [Robin Schmidt`s Open303](https://github.com/RobinSchmidt/Open303), a Roland TB-303 clone plugin. 

![Image of Roland TB-303](https://raw.githubusercontent.com/midilab/jc303/main/img/Roland_TB-303_Panel.jpg)  


## Download

This very first version features a headless plugin (user interface planned for upcoming releases), with direct control mapping on your DAW. All platforms, Windows, Linux and MacOS will have VST2, VST3 and LV2 plugins format avaliable to install.

MacOS Intel x64 and M1: [jc303-macos-universal-plugins.zip](https://github.com/midilab/jc303/releases/download/v0.9/jc303-macos-universal-plugins.zip)  
Windows Intel x64: [jc303-win64-plugins.zip](https://github.com/midilab/jc303/releases/download/v0.9/jc303-win64-plugins.zip)  
Linux Intel x64: [jc303-linux64-plugins.zip](https://github.com/midilab/jc303/releases/download/v0.9/jc303-linux64-plugins.zip)  
  
**Installation**: The platform zip pack will contain a folder per plugin format, just pick the format you want to install and copy the content of the folder to your OS plugin format folder.  

**Know issues**: MacOS AU not loading in M1 tested with ableton11, all other MacOS plugins format tested and working fine on M1!  

## Generating IDE project

To generate an **Xcode** project, run:
```sh
cmake -B build -G Xcode -D CMAKE_OSX_ARCHITECTURES=arm64\;x86_64 -D CMAKE_OSX_DEPLOYMENT_TARGET=10.13
```
The `-D CMAKE_OSX_ARCHITECTURES=arm64\;x86_64` flag is required to build universal binaries.

The `-D CMAKE_OSX_DEPLOYMENT_TARGET=10.13` flag sets the minimum MacOS version to be supported.

---

To generate a **Visual Studio 2022 (17)** project, run:
```sh
cmake -B build -G "Visual Studio 17"
```

---

To generate a **Linux CMake** project, run:
```sh
cmake -B build
```

## Building

To build the generated IDE project from the command line, run:
```sh
cmake --build build --config Release
```

## Roadmap

1. ~~Binary release for MacOS, Windows and Linux~~  
2. Graphical User Interface  
3. Filter type select  
4. Step Sequencer  
