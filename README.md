![Logo](/assets/logo.png)

# Try It Yourself

You can try ClownMDEmu in your web browser at [clownmdemu.clownacy.com](http://clownmdemu.clownacy.com).

# Overview

This is ClownMDEmu, a Sega Mega Drive (a.k.a. Sega Genesis) emulator.

Some standard features of the Mega Drive are currently unemulated (see
`common/core/TODO.md` for more information).

![Minimal](/assets/screenshot-minimal.png)
![Debug](/assets/screenshot-debug.png)

The repository contains ClownMDEmu's standalone frontend; it is written in
C++11 and leverages the SDL2, Dear ImGui, FreeType, and inih libraries. On
Unix platforms, there is also an optional dependency on the Zenity and kdialog
projects.

## Controls

The default control scheme is as follows:

### Keyboard

- Up    = Up
- Down  = Down
- Left  = Left
- Right = Right
- Z     = A
- X     = B
- C     = C
- Enter = Start

### Controller

- Up    = Up
- Down  = Down
- Left  = Left
- Right = Right
- X     = A
- Y     = B
- B     = C
- A     = B
- Start = Start
- Back  = Toggle which Control Pad the controller controls
- LB    = Load save state
- RB    = Create save state
- LT    = Rewind
- RT    = Fast-forward
- RSB   = Toggle menu controls (see http://www.dearimgui.org/controls_sheets/imgui%20controls%20v6%20-%20Xbox.png)

### Hotkeys

- Pause = Pause
- Space = Fast-forward (unpaused), frame-advance (paused)
- R     = Rewind
- Tab   = Soft reset
- F1    = Toggle which Control Pad the keyboard controls
- F5    = Create save state
- F9    = Load save state
- F11   = Toggle fullscreen


# Licence

ClownMDEmu is free software, licensed under the AGPLv3 (or any later version).
See `LICENCE.txt` for more information.
