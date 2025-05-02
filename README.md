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
C++20 and leverages the SDL3, Dear ImGui, and FreeType libraries.

## Controls

The default control scheme is as follows:

### Keyboard

| Input     | Action |
| --------- | ------ |
| Up        | Up     |
| Down      | Down   |
| Left      | Left   |
| Right     | Right  |
| Z         | A      |
| X         | B      |
| C         | C      |
| A         | X      |
| S         | Y      |
| D         | Z      |
| Enter     | Start  |
| Backspace | Mode   |

### Controller

| Input | Action                     |
| ----- | -------------------------- |
| Up    | Up                         |
| Down  | Down                       |
| Left  | Left                       |
| Right | Right                      |
| X     | A                          |
| A     | B                          |
| B     | C                          |
| LB    | X                          |
| Y     | Y                          |
| RB    | Z                          |
| Start | Start                      |
| Back  | Mode                       |
| LT    | Rewind                     |
| RT    | Fast-forward/frame advance |
| RSB   | [Toggle menu controls](http://www.dearimgui.org/controls_sheets/imgui%20controls%20v6%20-%20Xbox.png) |

### Hotkeys

| Input | Action                                          |
| ----- | ----------------------------------------------- |
| Pause | Pause                                           |
| Space | Fast-forward (unpaused), frame-advance (paused) |
| R     | Rewind                                          |
| Tab   | Soft reset                                      |
| F1    | Toggle which Control Pad the keyboard controls  |
| F5    | Create save state                               |
| F9    | Load save state                                 |
| F11   | Toggle fullscreen                               |

# Licence

ClownMDEmu is free software, licensed under the AGPLv3 (or any later version).
See `LICENCE.txt` for more information.
