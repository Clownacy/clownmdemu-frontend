# Overview

This is clownmdemu, a Sega Mega Drive (a.k.a. Sega Genesis) emulator.

It is currently in the very early stages of development: it can run some games,
but many standard features of the Mega Drive are currently unemulated (see
`TODO.md` for more information).


# Design

clownmdemu's design emphasises minimalism and portability: it is written in C89,
integer-only, and aims to have no dependencies on non-portable elements such as
integer type sizes, undefined behaviour, language extensions, or signed number
representation. Additionally, clownmdemu itself is implemented as a library,
with all platform-specific logic being relgated to a separate frontend program.

clownmdemu attempts to balance correctness with performance, acting as a more
high-level emulator than accuracy-focussed alternatives may.


# Frontend

An example frontend is provided in the `frontend` directory; it is written in
C99 and leverages the SDL2 library.

To run a game, the path to the game file must be passed to the frontend as a
parameter (this can be achieved by dragging and dropping the file onto the
executable).

The control scheme is currently hardcoded to the following layout:

Joypad 1:
- W = Up
- S = Down
- A = Left
- D = Right
- O = A
- P = B
- [ = C
- Enter = Start

Joypad 2:
- Up    = Up
- Down  = Down
- Left  = Left
- Right = Right
- Z = A
- X = B
- C = C
- V = Start

Hotkeys:
- Space = Fastforward
- Tab   = Soft reset
- F5    = Create save state
- F9    = Load save state


# Compiling

clownmdemu can be built using CMake, however it should not be hard to create
your own build system if necessary, as the emulator's build process is not
complicated.


# Licence

clownmdemu is free software, licensed under the GPLv3 (or any later version).
See `LICENCE.txt` for more information.
