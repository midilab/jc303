# A JUCE port of Open303

A Cmake JUCE port of [Robin Schmidt`s Open303](https://github.com/RobinSchmidt/Open303), a Roland TB-303 clone plugin. 

![Image of Roland TB-303](https://raw.githubusercontent.com/midilab/jc303/main/img/Roland_TB-303_Panel.jpg)  

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

1. Binary release for MacOS, Windows and Linux  
2. Graphical User Interface  
3. Filter type select  
4. Step Sequencer  
