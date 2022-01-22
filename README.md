# Overview

This is clownmdemu, a Sega Mega Drive (a.k.a. Sega Genesis) emulator.

It is currently in the very early stages of development: it can run some games,
but many standard features of the Mega Drive are currently unemulated (see
`TODO.md` for more information).


# Design

clownmdemu's code adheres to the following principles, emphasising minimalism
and portability:

* Use C89. This is required in order to support as many compilers as possible.
  Ideally, the code should be valid C++ as well, in order to support both C and
  C++ compilers.

* Use integer-only logic. The Sega Mega Drive/Sega Genesis had no support for
  floating point types. Therefore, emulating it should not require floating
  point types. Additionally, floating point types are prone to rounding error
  and other precision issues, so I don't trust them. I am also concerned about
  their performance compared to integers, especially on lower-end hardware.

* Do not use dynamic memory. Memory allocation is slow, will cause memory leaks
  when not correctly freed, and can cause software to fail mid-execution when
  memory is exhausted, which is more effort to account for than it's worth.

* Do not depend on integer type sizes. Do not assume that 'char' is 8-bit,
  'short' is 16-bit, 'int' is 32-bit, and 'long' is 32-bit (or 64-bit): the C
  standard does not guarantee any of this. You should only assume that 'char'
  is 8-bit, 'short' and 'int' are 16-bit, and that 'long' is 32-bit, *at a
  minimum*, as the C standard *does* guarantee.

* Do not rely on undefined behaviour. This should go without saying. This
  includes assuming that 'char' is signed by default.

* Do not rely on C language extensions. Again, this should go without saying.

* Do not rely on endianness. Another no-brainer: big-endian CPUs *do* exist,
  and *are* worth supporting.

* Do not rely on signed number representation. That is to say, do not assume
  that negative numbers are represented in binary as two's complement.

* Use no global state: all state should be kept in a struct that functions
  access through a pointer, allowing for such things as simple fast save-state
  support, as well as the possibility of running multiple instances of the
  emulator at once. Note that the state must not contain pointers, so that it
  is relocatable and position-independent. Likewise, the state must not depend
  on outside state, so that it is useable across multiple executions of the
  emulator.

Additionally, clownmdemu itself is implemented as a library, with all
platform-specific logic being relegated to a separate frontend program.

clownmdemu attempts to balance correctness with performance, acting as a more
high-level emulator than accuracy-focussed alternatives may.


# Frontend

An example frontend is provided in the `frontend` directory; it is written in
C++98 and leverages the SDL2, Dear ImGui, and tinyfiledialogs libraries.

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

Hotkeys:
- Space = Fast-forward
- Tab   = Soft reset
- F1    = Toggle which joypad the keyboard controls
- F5    = Create save state
- F9    = Load save state
- F11   = Toggle fullscreen


# Compiling

clownmdemu can be built using CMake, however it should not be hard to create
your own build system if necessary, as the emulator's build process is not
complicated.


# Licence

clownmdemu is free software, licensed under the GPLv3 (or any later version).
See `LICENCE.txt` for more information.
