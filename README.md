# Overview

This is clownmdemu, a Sega Mega Drive (a.k.a. Sega Genesis) emulator.

It is currently in the very early stages of development: it can run some games,
but many standard features of the Mega Drive are currently unemulated (see
`TODO.md` for more information).


# Design

clownmdemu's design emphasises minimalism and portability: it is written in C89,
integer-only, uses no dynamic memory, and aims to have no dependencies on
non-portable elements such as integer type sizes, undefined behaviour, language
extensions, or signed number representation. Additionally, clownmdemu itself is
implemented as a library, with all platform-specific logic being relgated to a
separate frontend program.

clownmdemu attempts to balance correctness with performance, acting as a more
high-level emulator than accuracy-focussed alternatives may.

Another design goal of clownmdemu is to use no global state: all state is kept
in a struct that functions access through a pointer, allowing for such things
as simple fast save-state support, as well as the possibility of running
multiple instances of the emulator at once.


# Frontend

An example frontend is provided in the `frontend` directory; it is written in
C99 and leverages the SDL2 library.

The frontend will open a file picker to select the game file with, but the user
can also pass the path to the game file to the executable as a parameter (note
that this can be achieved by dragging and dropping the game file onto the
executable file).

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
- F1    = Toggle between NTSC and PAL
- F2    = Toggle which joypad the keyboard controls
- F3    = Toggle V-sync
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
