# Overview

This is clownmdemu, a Sega Mega Drive (a.k.a. Sega Genesis) emulator.

It is currently in the very early stages of development: it can run some games,
but many standard features of the Mega Drive are unemulated (see `TODO.md` for
more information).


# Design

clownmdemu's code adheres to the following principles, emphasising minimalism
and portability:

- Use C89. This is required in order to support as many compilers as possible.
  Ideally, the code should be valid C++ as well, in order to support both C and
  C++ compilers.

- Use integer-only logic. The Sega Mega Drive/Sega Genesis had no support for
  floating point types. Therefore, emulating it should not require floating
  point types. Additionally, floating point types are prone to rounding error
  and other precision issues, so I do not trust them. I am also concerned about
  their performance compared to integers, especially on lower-end hardware.

- Do not use dynamic memory. Memory allocation is slow, will cause memory leaks
  when not correctly freed, and can cause software to fail mid-execution when
  memory is exhausted, which is more effort to account for than it is worth.

- Use no global state. All state should be kept in a struct which functions
  access through a pointer, allowing for such things as simple fast save-state
  support as well as the possibility of running multiple instances of the
  emulator at once. Note that the state must not contain pointers, so that it
  is relocatable and position-independent. Likewise, the state must not depend
  on outside state, so that it is useable across multiple executions of the
  emulator.

- Operate within the guarantees of the C standard: no undefined behaviour and
  no implementation-defined behaviour.

  - Do not depend on integer type sizes. 'char', 'short', 'int', and 'long' may
    be different sizes on different platforms, so do not rely on their ability
    to hold (or not hold) values larger than their standard capacities:
    - -127 to 127 for 'signed char'.
    - 0 to 255 for 'unsigned char'.
    - -32767 to 32767 for 'short' and 'int'.
    - 0 to 65535 for 'unsigned short' and 'unsigned int'.
    - -2147483647 to 2147483647 for 'long'.
    - 0 to 4294967295 for 'unsigned long'.

  - Do not assume that 'char' is always signed by default: it is not. For
    instance, it is unsigned by default on ARM CPUs.

  - Do not rely on C language extensions.

  - Do not rely on endianness. Code should work correctly on both little-endian
    and big-endian CPUs.

  - Do not rely on signed number representation. That is to say, do not assume
    that negative numbers are represented in binary as two's complement.

clownmdemu itself is implemented as a library, with all platform-specific logic
being relegated to a separate frontend program.

clownmdemu attempts to balance correctness with performance, acting as a more
high-level emulator than accuracy-focussed alternatives may.

clownmdemu exposes a relatively low-level interface: audio from the FM and PSG
are output separately at their native sample rates, and video is output a
single scanline at a time in its native indexed format. This is to give the
frontend the most flexibility in how it can process the data for delivery to
the user. For instance, if the platform can play multiple separate audio
streams at once, then the frontend can skip the expensive steps of audio
resampling and mixing.


# Frontend

![Minimal](/screenshot-minimal.png)
![Debug](/screenshot-debug.png)

An example frontend is provided in the `frontend` directory; it is written in
C++98 and leverages the SDL2, Dear ImGui, FreeType, and tinyfiledialogs
libraries.

The control scheme is currently hardcoded to the following layout:

Keyboard:
- W  = Up
- S  = Down
- A  = Left
- D  = Right
- O  = A
- P  = B
- \[ = C
- Enter = Start

Controller:
- Up    = Up
- Down  = Down
- Left  = Left
- Right = Right
- X     = A
- Y     = B
- B     = C
- A     = B
- Start = Start
- Back  = Toggle which joypad the controller controls
- LB    = Rewind
- RB    = Fast-forward
- RSB   = Toggle menu controls (see http://www.dearimgui.org/controls_sheets/imgui%20controls%20v6%20-%20Xbox.png)

Hotkeys:
- Space = Fast-forward
- R     = Rewind
- Tab   = Soft reset
- F1    = Toggle which joypad the keyboard controls
- F5    = Create save state
- F9    = Load save state
- F11   = Toggle fullscreen


# libretro

A frontend that exposes clownmdemu as a libretro core is provided in the
`libretro` directory. It is written in C89 and should provide all of the same
features as the example frontend aside from the debug menus.


# Compiling

clownmdemu can be built using CMake, however it should not be hard to make it
use a different build system if necessary as the emulator's build process is
not complicated.

The reference frontend is also built using CMake.

The libretro core can be built using either CMake or GNU Make.

Be aware that this repo uses Git submodules: use `git submodule update --init`
to pull in these submodules before compiling either frontend.


# Licence

clownmdemu is free software, licensed under the AGPLv3 (or any later version).
See `LICENCE.txt` for more information.
