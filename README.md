# Overview

This is clownmdemu, a Sega Mega Drive (a.k.a. Sega Genesis) emulator.

It is currently in the very early stages of development: it can run some games,
but many standard features of the Mega Drive are unemulated (see
`clownmdemu-frontend-common/clownmdemu/TODO.md` for more information).

![Minimal](/screenshot-minimal.png)
![Debug](/screenshot-debug.png)

The repository contains clownmdemu's standalone frontend; it is written in
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


# Licence

clownmdemu is free software, licensed under the AGPLv3 (or any later version).
See `LICENCE.txt` for more information.
