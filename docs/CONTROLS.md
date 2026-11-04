# Controls

The game accepts mouse, keyboard, touch, SDL gamepads, joysticks, and the PS5
DualSense. The pointer bridge is viewport-aware: window coordinates are mapped
through the centered 4:3 letterbox before they reach the game, and clicks in the
black bars are ignored. Android additionally draws its own controls in those
unused bars and runs the activity immersive full-screen, with the status and
navigation bars hidden until a deliberate edge swipe reveals them temporarily.

## Keyboard and mouse

| Input | Action |
| --- | --- |
| Mouse / touchpad motion | Move the game pointer |
| Left mouse button | Left click / select |
| Right mouse button | Right click / alternate action |
| Arrow keys | Move the game cursor |
| Enter | Select / confirm |
| Escape | Back / cancel in the game's normal command paths |
| Backspace, Delete, Home, End, Page Up/Down, Tab | Passed to the game's text and list controls |
| Letters, digits, punctuation | Normal text entry, including high-score names |
| `F` | Toggle borderless desktop fullscreen |
| `H` | Toggle high-resolution and original assets |
| Touch pointer (Android) | Tap or drag inside the game viewport; delivered as the selected left or right click |
| Click mode (Android) | Tap `L CLICK`/`R CLICK` to toggle whether touch input becomes a left or right click; left is the default |
| Rotation lock (Android) | Tap `LOCK` to pin the current orientation; the label changes to `UNLOCK` to resume sensor rotation. The lock state and pinned orientation are remembered for later launches |
| Exit app (Android) | Tap `EXIT` in the unused area around the game |
| On-screen keyboard (Android) | Tap `KEYBOARD` to show or hide the IME |

`F` and `H` are game text while a typed-token field is active, so names
containing those letters still work. The fullscreen hotkey exists on desktop
only; the PS5 output is already a centered 4:3 surface. Android also leaves
`F` and `H` as ordinary text and has no fullscreen toggle.

The initial desktop window is 640x480 at render scale 4, with a 4:3 aspect
lock. Resizing or maximizing produces the largest centered 4:3 viewport that
fits the display. The canvas is never stretched.

## Android touch controls

Android runs the activity immersive full-screen and presents the same centered
4:3 game view on the whole display. The status and navigation bars are hidden;
a deliberate edge swipe may reveal them temporarily. On a wide phone, **EXIT**
and **KEYBOARD** share the left side bar while
**LOCK** and **L CLICK/R CLICK** share the right side bar. A taller layout uses
a single row in the bottom or top bar. A display with no letterbox falls back
to a compact row at the top of the view. The four controls are:

- **EXIT** closes the game through the normal shutdown path, so a live save is
  written before the activity finishes.
- **KEYBOARD** toggles the Android input method editor (IME). The IME is hidden
  at startup, even when a hardware keyboard is present, and is only shown by
  this control.
- **LOCK** pins the current portrait or landscape orientation. The label changes
  to **UNLOCK** and the closed-padlock icon becomes an open padlock; tapping it
  resumes following the device sensors. This is an in-app lock and does not
  change the system rotation setting. The lock state and the exact pinned
  orientation are saved, so the next launch restores the same configuration;
  an unlocked launch follows the sensors again.
- **L CLICK** selects left-click mode for touch input. It is active at startup
  and its mouse icon fills the selected top button while outlining the other
  one. Tapping it changes the label to **R CLICK**, fills the right mouse
  button, and delivers touch input as right clicks. Tapping again returns to
  left-click mode.

Touches inside the game viewport become the normal game pointer, including
clicks and drags, and are delivered as the selected click mode. A physical
mouse or pen keeps its own buttons. Touches on a virtual control are consumed
before the game sees them. The controls are clamped to the reported safe area
so a cutout or system inset does not cover them.

The IME receives text through SDL's text-input path. While it is open, the
activity resizes around the IME; the game view remains centered and is never
stretched, and all four controls stay reachable. Full-screen mode keeps the
system bars hidden without setting `FLAG_FULLSCREEN` or the `LAYOUT_*`
system-UI flags, so `adjustResize` continues to shrink the SDL surface around
the keyboard. The soft keyboard's Enter key stays visible and is delivered to
the game as the same carriage-return action as a desktop Enter key; it does not
dismiss the IME. Tap `KEYBOARD` again to hide the keyboard. The game's own
command keys and high-score name entry use the same keyboard path as the desktop
build.

## Gamepad

| Input | Action |
| --- | --- |
| Left stick, right stick, or d-pad | Move the game pointer |
| South / A | Left click |
| East / B | Right click |
| Touchpad button | Left click |
| PS5 touchpad contact | Absolute pointer movement |
| R3 (right-stick click) | Toggle high-resolution assets |
| L3 (left-stick click) | Open the PS5 system keyboard |
| Select / Options / Back / Start | Open the PS5 system keyboard |

On PS5, both Back and Start are exposed as the Options button because the
console reserves Create/Share. Either semantic opens the system keyboard. The
keyboard opens on the press edge only; holding the button does not reopen it.

A controller-driven pointer hides the host arrow and draws a high-contrast
crosshair. Mouse or touchpad motion returns to the normal arrow. Controller
hot-plugging is supported.

## PS5 system keyboard

The native PS5 build uses the system IME. L3 or Select/Options opens the
keyboard. While a typed-token field is active, the dialog edits that field; on
other screens the confirmed ASCII text is delivered one byte per frame to the
game's normal text input, such as the high-score name prompt.

The IME module is requested during startup, before the loader closes the title
sandbox. The dialog layout is validated against the SDK's `sceImeDialog`
structure; keyboard entry still requires hardware confirmation on the target
firmware. See [PS5.md](PS5.md).

## Fullscreen and aspect ratio

Desktop fullscreen is borderless. The 4:3 game image is centered and the unused
area is black. The 4x framebuffer is downsampled or upsampled as needed while
preserving the 4:3 canvas. The saved window size and position are restored when
fullscreen is left.

Android uses the whole display with the status and navigation bars hidden and
the same centered 4:3 presentation. The virtual controls are drawn over the
unused bars; the game canvas itself is never scaled to fill a wide screen. The
activity follows all four sensor orientations while unlocked, so the centered
canvas stays usable in portrait and either landscape direction. **LOCK** pins
the current orientation and **UNLOCK** resumes sensor rotation; the lock is
local to the app. Android has no in-app back button and no fullscreen toggle:
full-screen mode is always on, while the platform navigation controls remain
available through an edge swipe when the system provides them.

`NW_WINDOW_RESIZABLE=0` restores a fixed 640x480 desktop window.
`NW_WINDOW_SCALE=N` changes the initial desktop window scale without changing
the render scale.
