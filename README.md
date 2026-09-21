# JC-303 Plugin

This is a Free Roland TB-303 clone plugin. A Cmake JUCE port of [Robin Schmidt`s Open303](https://github.com/RobinSchmidt/Open303) with added features.

![JC-303 Screenshot](https://raw.githubusercontent.com/midilab/jc303/main/img/jc303.png)

This software is licensed under the GNU General Public License version 3 (GPLv3).

The Open303 engine part of this software is also licensed under the MIT License.

## Download

Supports Windows, Linux and MacOS. You may find CLAP, VST3, LV2 and AU formats available to download. For VST2 plugin you need to compile it by your own self using vst2 sdk from Steinberg - vstsdk2.4.

MacOS Universal - Intel and ARM: [jc303-macos_universal-plugins.zip](https://github.com/midilab/jc303/releases/download/v0.13.0/jc303-0.13.0-macos_universal-plugins.zip)

Windows Intel x64: [jc303-windows_x64-plugins.zip](https://github.com/midilab/jc303/releases/download/v0.13.0/jc303-0.13.0-windows_x64-plugins.zip)

Linux Intel x64: [jc303-linux_x64-plugins.zip](https://github.com/midilab/jc303/releases/download/v0.13.0/jc303-0.13.0-linux_x64-plugins.zip)  

Linux ARM64: Soon...  

## Installation

The platform zip pack will contain a folder per plugin format, just pick the format you want to install and copy the content of the folder to your OS plugin format folder.

**MacOs De-Quarantine**: MacOs users needs to de-quarantine plugin before load it into any DAW.  
Open a terminal window and do the following
```shell
$ sudo xattr -rd com.apple.quarantine /Library/Audio/Plug-Ins/Components/JC303.component
```
This de-quarantine example is for AU, please do the same for other formats you'll be using

## Step Sequencer

JC303 ships with a full acid step sequencer ported from [Aciduino V2](https://midilab.co/aciduino). It is a 16-step monophonic sequencer clocked sample-accurately from the audio thread, playing the built-in 303 engine and sending MIDI out to the host. Note length is the authentic 303 half-step (50%) gate, and slide/tie bridge correctly from the last step back to the first. The sequencer panel and manual below target the `midilabv2` GUI theme.

### Layout

Each of the 16 steps is a column of controls:

| Row | Control | Meaning |
|--|--|--|
| `step` | LED | Selects the step to edit; also shows the playing step and the active pattern length |
| `rest` | toggle | Note ON (toggle up) or rest (toggle down) |
| `accent` | micro-toggle | Step plays with accent |
| `slide` | micro-toggle | Step slides into the next one |
| `tie` | micro-toggle | Step ties into the previous note |

Below the grid are the transport buttons (`PLAY/STOP`, `REC`, `REST`, `CLEAR`, `gen`), the generative mod knobs, and the on-screen octave keyboard.

### Editing steps

- **Left-click a step LED** to select the step to edit. The selected step blinks; the step currently playing is highlighted. Clicking a step at or beyond the active pattern length snaps the selection to the last active step.
- **Note/rest toggle**: each step is a note by default; toggle it off to make it a rest, on to re-activate it.
- **Accent / Slide / Tie** are per-step toggles. A tie carries the previous note over, and handles wrap-around from the last step to the first.
- The keyboard always shows the note of the selected step.

### Setting the pattern length

- **Right-click any step LED** to set the pattern length from 1 to 16 (the clicked step becomes the last step of the pattern). Steps beyond the length are kept but stay silent.
- Length can also be set with the `Length` item in the `SEQ` menu page.

### Seq keyboard

- **Click a key** to set the note of the selected step.
- **Mouse wheel over the keyboard** transposes the note of the selected step by one octave per notch (the octave of the keyboard follows the note).
- In record mode the rec cursor is the target for both actions.

### Record mode

Turn `REC` on to enter serial (step-by-step) entry. The record cursor starts at the currently selected step and advances one step per entered event, wrapping at the pattern length.

- **Keyboard**: press a key to record its note and advance. Existing accent/slide/tie flags on the step are kept.
- **External MIDI**: velocity **100 or above** records the step with accent; striking a second note while one is still held records a slide (legato); tapping the sustain pedal (CC64, 127 on) records a rest.
- **`REST` button**: in record mode it records a rest and advances the cursor; outside record mode it turns the selected step into a rest and moves the selection to the next step.
- **`CLEAR` button** resets the whole track to rests (all accents, slides and ties removed).

### Generative

The `gen` button generates a random pattern across all 16 steps using:

| Knob | Range (default) | Meaning |
|--|--|--|
| `FILL` | 0–100 (80) | % chance each step is ON (not a rest) |
| `ACC` | 0–100 (50) | % chance of accent on an ON step |
| `SLIDE` | 0–100 (30) | % chance of slide on an ON step |
| `TIE` | 0–100 (100) | % chance of tie on a rest step |
| `TONES` | 1–12 (4) | Number of pitch classes to snap to (1 = single note) |
| `LOW` | 0–127 (20) | Lowest MIDI note in the random pitch range |
| `RANGE` | 0–127 (48) | Range above `LOW` of the random pitches |

The sequencer is briefly muted while a pattern is generated so no stray notes play. Generating keeps the pattern length intact but overwrites the steps within it.

### Transport and clock

- **`PLAY/STOP`** starts and stops the sequencer.
- The `SEQ` menu page in the LCD exposes:
  - **Length** — pattern length (1–16).
  - **Tempo** — internal BPM (20–300); active only in `Internal` sync mode.
  - **Sync Mode** — `Internal` (free-running at Tempo), `Host` (follows the DAW transport position and tempo), or `Midi Clock` (follows external MIDI Start/Stop/Clock messages).
  - **Start Mode** — `Transport` (starts on host transport or MIDI Start) or `Note Trigger` (starts automatically on the first incoming note, even while stopped).

## Build

Generate the cmake project build files first for the OS of your choice.  

#### cmake options

| Variable | Description | Default |
|--|--|--|
| GUI | Select GUI theme interface to use | midilabv2 |
  
Available themes: midilabv2 (amadeusp is deprecated)  
  
To change JC303 GUI theme add the following to the first cmake call: -D GUI=midilabv2  
  
### Apple Xcode

To generate an **Xcode** project, run:

```sh
cmake -B build -G Xcode -D CMAKE_OSX_ARCHITECTURES=arm64\;x86_64 -D CMAKE_OSX_DEPLOYMENT_TARGET=10.13
```

The `-D CMAKE_OSX_ARCHITECTURES=arm64\;x86_64` flag is required to build universal binaries.

The `-D CMAKE_OSX_DEPLOYMENT_TARGET=10.13` flag sets the minimum MacOS version to be supported.

### Windows Visual Studio

We suggest to install [Cygwin](https://cygwin.com/install.html) with git and a terminal to compile JC303.  
  
Find the PATH of your visual studio compiler and cmake installation and add to your .profile  
  
```sh
# Visual Studio 18 / MSVC 14.51
export PATH="$PATH:/cygdrive/c/Program Files/Microsoft Visual Studio/18/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin"
export PATH="$PATH:/cygdrive/c/Program Files/Microsoft Visual Studio/18/Community/VC/Tools/MSVC/14.51.36231/bin/Hostx64/x64"
```
  
```sh
source .profile
```
  
To generate a **Visual Studio 2026 (18)** project, run:  
  
```sh
cmake -B build -G "Visual Studio 18" -A x64
```

### GNU Linux

Install the dependecies:

#### Ubuntu

```sh
sudo apt install build-essential gcc cmake libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev libfreetype6-dev libasound2-dev
```

To generate a **Linux CMake** project, run:

```sh
cmake -B build
```

## Compile

To compiled from the command line, run:

```sh
cmake --build build --config Release
```

## Docker build

Build x86_64 release (default behavior)
```sh
docker build --build-arg RELEASE_ARCH=X86_64 -t jc303-linux-builder-x86_64 .
```

Build arm64 release
```sh
docker build --build-arg RELEASE_ARCH=ARM64 -t jc303-linux-builder-arm64 .
```

Run – mounts current directory → /jc303
```sh
docker run -it --rm -v "$(pwd):/jc303" jc303-linux-builder
```

#### VST2 Plugin

No distribution of VST2 plugin binaries is allowed without a license, but if you have the sdk and the license to use it just copy the vstsdk2.4/ SDK folder to the root of this project before run cmake.

## Roadmap

1. ~~Binary release for MacOS, Windows and Linux~~
2. ~~Graphical User Interface~~
3. ~~Internal parameters for engine tunning -Inspired on Devilfish Mod~~
4. ~~Overdrive~~
5. ~~Step Sequencer~~
6. Sequencer Patterns
5. Preset Support
