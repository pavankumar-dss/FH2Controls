# FH2Controls

Keyboard and controller bindings for **Forza Horizon 2 on PC (Azimuth)**, from one ini file.
It needs no driver, no virtual controller and no in-game capture screens.

- **Play on the keyboard.** The mod gives the game a controller of its own, made from your keys. Windows never sees a device, so there's no ViGEmBus, no "device connected" sound and no Game Bar popup.
- **Keys for the game's actions.** Bind a key to *ShiftUp*, *Clutch* or *Map* and it does that on the road, whatever button the action is on. In the menus, keys work as controller buttons. So ↑ can shift up on the road and be d-pad up in a menu, and Space can be the handbrake and A, without one leaking into the other.
- **Your keys on screen.** The game's button prompts (help bars, HUD, tabs) show the key you bound instead of the Xbox icon.
- **Mouse look.** While you drive, the mouse turns the view. Leave it still and the view returns ahead.
- **Launch control.** Hold the launch key and the gas at a standstill and the revs are held at a share of the redline. Let go of the key to launch.
- **A controller layout.** `[Controller]` sets which action each button of a real controller has. It replaces the CustomBindings mod.
- **A Map button.** It opens the quick map while you drive, from a key or a controller button. It replaces the MapButton mod.
- **Live editing.** Edit the ini while the game runs: keyboard changes apply within a second.

Keyboard only for the keyboard parts; a real controller keeps working alongside.

## Install

Needs Azimuth 1.0.3 (it doesn't depend on Azimuth's internals, so other versions should work too) and the FH2 Mod Platform 2.8.0 or newer.

1. Close the game.
2. Extract the release zip into the folder with `azimuth.exe`, keeping the folders:
   - `fh2-mods\FH2Controls.dll`: this mod.
   - `DWrite.dll`: the FH2 Mod Platform, shared by FH2 mods. If you already have a newer copy, keep the newer one.
3. Start the game. The first time, the mod writes `fh2-mods\FH2Controls.ini`, with defaults. If you used CustomBindings, your controller layout is copied into it.
4. If you used **KeyboardDrive**, **CustomBindings** or **MapButton**, delete their DLLs from `fh2-mods`: FH2Controls does their jobs. Close **Keyvert** too.

## FH2Controls.ini

```ini
[Settings]
Enabled = 1
SteeringTime = 0.25     ; seconds from straight ahead to full lock
ReturnTime = 0.15       ; seconds back to straight ahead
PedalTime = 0.10        ; seconds for gas and brake to go full and back
LaunchRPM = 0.60        ; launch control: the revs held, as a share of the redline
KeyPrompts = 1          ; button prompts show your keys
MouseLook = 1
MouseSensitivity = 1.0
MouseLookReturn = 0.6   ; seconds still before the view returns ahead (0 = it stays)

[Keyboard]
; <what> = <key>[, <key>...]   another line for the same thing adds more keys
Gas = W
Brake = S
SteerLeft = A
SteerRight = D
Handbrake = Space
LaunchControl = Space
ShiftUp = E
ShiftDown = Q
Map = M
Pause = Escape          ; on the road only: Escape can be B (back) in the menus
A = Enter, Space
B = Backspace
LB = [
RB = ]
DPadUp = Up
...

[Controller]
Custom = 1              ; use this layout in place of the one picked in Settings > Controls
ButtonA = Handbrake
RightThumbButton = Map
...
```

**What a key can be bound to:**
- **Actions** work on the road only: `Gas`, `Brake`, `SteerLeft`, `SteerRight`, `Handbrake`, `Clutch`, `ShiftUp`, `ShiftDown`, `SwitchCamera`, `Rewind`, `Map`, `LookForward`, `LookBack`, `LookLeft`, `LookRight`, `LookStraightBack`, `PhotoMode`, `Telemetry`, `RadioLeft`, `RadioRight`, `MiniLeaderboard`, `Pause`, `LaunchControl`.
- **Controller inputs** are for the menus, and for anything else that reads the controller, like other mods: `A`, `B`, `X`, `Y`, `LB`, `RB`, `LT`, `RT`, `LS`, `RS`, `View`, `Menu`, `DPadUp/Down/Left/Right`, `LeftStickUp/Down/Left/Right`, `RightStickUp/Down/Left/Right`.

**Key names:** `A`–`Z`, `0`–`9`, `F1`–`F12`, `Space`, `Enter`, `Escape`, `Backspace`, `Tab`, `Up`, `Down`, `Left`, `Right`, `LShift`, `RShift`, `LCtrl`, `RCtrl`, `LAlt`, `RAlt`, `CapsLock`, `Insert`, `Delete`, `Home`, `End`, `PageUp`, `PageDown`, `Num0`–`Num9`, `NumPlus`, `NumMinus`, `NumMultiply`, `NumDivide`, `NumPeriod`, `[` `]` `;` `'` `,` `.` `/` `\` `-` `=` `` ` ``.

`fh2-mods\platform.log` lists what the mod read, and any line it didn't understand.

**On the Mods page** (Pause > Settings > MODS) you'll find: Keyboard on/off (plus a hotkey), steering and pedal speeds, mouse look and its speed, and the launch control revs.

## How it works

- **The keyboard controller.** Azimuth gives the game the controllers it gets from Windows.Gaming.Input. FH2Controls wraps Azimuth's `RoGetActivationFactory("Windows.Gaming.Input.Gamepad")`, so the list Azimuth sees has one more pad, read from the keys. Mods that read XInput (NitroSystem's nitro button) see it as pad 0.
- **Actions.** The game asks for its actions through `IsActionPressed` and its analog counterpart. On the road, a key bound to an action presses whichever input the game has that action on, read from the game's own bindings every frame. An action on no input, such as Map, is answered directly.
- **The controller layout** goes in as the game reads `ControllerLayouts`.
- **Button prompts.** The game loads its button icons as textures. For a `ControllerIcons` texture the mod hands the bitmap a keycap it draws itself, through the same upload path the game uses for gamerpics.
- **Mouse look.** The mouse is read through a shared DirectInput device, and its yaw goes into the game's driving look after the game has updated it.
- **Launch control** reads the revs from the car's physics object, the values the HUD's rev counter shows, and works the gas so they hold.
- **Safety checks.** Every game address is checked byte for byte before it's hooked. On a mismatch that part stays off and the log says so.

## Building

Visual Studio Build Tools (x64): `build.cmd`. Tests: `tests\run.cmd` builds and runs the harness against the DLL, with the game's exe mapped (not run) so the hooks are checked against it.

## Credits

- The FH2 Mod Platform by Trinculo54 (MIT), which includes MinHook (BSD 2-Clause).
- The button-prompt and mouse-look techniques follow the notes in Extra Options PR #101 by byzyn4ik (native keyboard & mouse for Extra Options). This is a separate implementation.
- CustomBindings and MapButton showed what players want from a layout and a Map button.

## AI disclosure

This mod was built with Claude, Anthropic's AI assistant, which did most of the reverse engineering, code and documentation. The author directed the work and tested it in game.

## License

MIT, see [LICENSE](LICENSE).
