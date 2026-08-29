# Gamebuino Classic → Vircon32 Port

## Goal

Port games from the [Gamebuino Classic](https://github.com/Gamebuino/Gamebuino-Classic)
library into a single Vircon32 cartridge, behind one shared game-select
menu - the same overall shape as the sibling `tinyjoypad_vircon32`
project (itself following crisp-game-lib-portable's own "one binary, many
games, addGame() menu" pattern), reusing that project's own menu system
and column-atlas rendering technique directly.

This file is project context for Claude Code. Read it before making
changes, and keep it updated as decisions get made.

## Why Gamebuino Classic

Picked as a follow-up to `tinyjoypad_vircon32` after the user asked for
other small-console porting project ideas and specifically pushed back on
Arduboy (a real, but meaningfully bigger step up in RAM/flash/game
complexity than TinyJoypad's own ATtiny85). Gamebuino Classic stays in the
same tier: an ATmega328-class 8-bit AVR chip, a small hobbyist game
library, real Arduino `.ino` C/C++ source (confirmed directly via the
official repo and several real community games before committing to this
project, not assumed) - the same source-translation methodology already
proven on `tinyjoypad_vircon32` applies here largely unchanged.

## Source platform facts (Gamebuino Classic)

Confirmed by reading the real library source directly
(`Gamebuino.h`/`utility/Display.h`/`Buttons.h`/`Sound.h`/`settings.c`),
not assumed from documentation:

- 84x48 PCD8544 (Nokia 5110) monochrome LCD, addressed exactly like
  SSD1306: one byte per (column, page), 8 vertical pixels per byte, LSB =
  top pixel. 6 pages (48/8).
- **A genuine CPU-writable framebuffer** (`Display::_displayBuffer[]`,
  real random-access read *and* write via `drawPixel()`/`getPixel()`) -
  unlike TinyJoypad's own byte-stream-only driver lineages, which never
  read pixels back. This project's own framebuffer (`gbFrameBuffer[]` in
  `gamebuinoShim.c`) matches the real addressing formula exactly:
  `buf[x + (y/8)*84]`, bit `y%8`.
- 7 real discrete digital buttons (Up/Down/Left/Right/A/B/C, real GPIO
  pins) - no analog voltage-ladder decoding needed, a simpler input
  surface than TinyJoypad's own scheme.
- A real, if modest, sound engine (`Sound.h`) - a tracker/pattern/note
  system, `NUM_CHANNELS` configurable (1 by default). This first pass
  only implements `playNote()`/`playTick()`/`playOK()`/`playCancel()` (a
  representative set of one-shot tones) - the full pattern/track player
  is out of scope for now, see "Open questions" below.
- `gb.update()` throttles to a configurable frame rate (**20fps default** -
  `Gamebuino::begin()` sets `timePerFrame = 50` directly, i.e. 1000/50 =
  20fps; `setFrameRate(fps)` itself has no explicit clamp of its own beyond
  its real 8-bit parameter range - see "A frame-rate default bug" below for
  a real mistake this project made by not checking this directly at
  first) and auto-clears the display buffer each cycle (unless
  `persistence` is set) - real games are structured as `if (gb.update())
  { ...logic and drawing...; }`, called every real loop() iteration.

## Target platform facts (Vircon32)

Identical to the sibling `tinyjoypad_vircon32` project - see that
project's own CLAUDE.md for the full writeup (GPU is a texture-region
blitter with no CPU-writable framebuffer, 15MHz CPU, 640x360 true-color
screen, 16-channel audio, etc). The same "only 256 possible column-byte
values, pre-bake them all as texture tiles" trick applies here unchanged,
just scaled to an 84-wide/6-page canvas instead of 128-wide/8-page (see
`tools/gen_column_atlas.py`).

## Architecture

Every feature `tinyjoypad_vircon32` *ended up with* has now been ported
here too, once this project had more than a single game's worth of reason
to want them - see "Thumbnail atlas, quit-confirmation dialog, pixel-grid
overlay, and global mute, ported from the sibling project" and "EEPROM
persistence, ported from the sibling project (1024 bytes, not 512)" below
for the write-ups:

- A menu thumbnail atlas - the credited-author line now sits *under* a
  real gameplay screenshot instead of standing in for one.
- A quit-confirmation dialog (Start, mid-game) - Start no longer instantly
  quits to the menu.
- A pixel-grid overlay toggle (Button L, mid-game).
- A global mute toggle (Button Y, everywhere).
- Memory-card/EEPROM persistence (`eepromShim.h`/`.c` +
  `machineDependent.h`'s own `md_card*()` primitives) - no game ported so
  far actually uses it yet (Pong has no high score to save), but the
  infrastructure is proven and ready for the first one that does (see
  `shipwrek` in `more games/DISCOVERED_GAMES.md`'s own porting-priority
  audit - it already ships a real `.eep` file upstream, making it a
  natural first consumer).

`src/machineDependent.h`/`src/portVircon32.c` - the `md_*` primitives
(video/input/audio), modeled directly on the sibling project's own files
of the same name and purpose.

`src/gamebuinoShim.h`/`src/gamebuinoShim.c` - reproduces the real
Gamebuino Classic API on top of `machineDependent.h`. **This dialect has
no classes/methods/operator overloading** (confirmed via the sibling
project's own `VIRCON32_C_DIALECT.md`), so real `gb.display.fillRect(...)`/
`gb.buttons.pressed(...)`/`gb.sound.playTick()` call syntax cannot be
preserved literally - every ported game needs its own `gb.x.y(...)` call
sites mechanically rewritten to a plain `gbY(...)` function call instead
(there is only ever one `gb` instance in any real cartridge anyway, so
flattening loses nothing - the same "flatten a real single-instance C++
library into plain C globals/functions" treatment already proven for the
sibling project's own `tinyJoypadShim.h`/`obonoCoreShim.h`). See
`src/games/gamePong.c`'s own header comment for a worked example of the
exact rewrites this needs.

Text rendering reuses the sibling project's own already-proven 8x8
"myfont" glyph table verbatim (copied, not re-derived) rather than porting
Gamebuino's own real variable-width 5x7/3x5 bitmap fonts - a deliberate,
documented simplification (see `gamebuinoShim.h`'s own header comment) -
this shim always renders with one fixed 8x8 table regardless of which
real font/size a game asks for, with `setFont()` a no-op and `fontSize`
supporting only 1 (native 8x8) and 2 (16x16, each glyph pixel doubled) -
the two sizes Gamebuino Classic games actually use in practice.

`src/avrCompat.h` - copied verbatim from the sibling project (no
TinyJoypad-specific content in it at all - `uint8_t`-family aliasing,
`PROGMEM`/`pgm_read_*` no-ops, `F()`/`__FlashStringHelper`, `arand()`).

`libs/PlayNote/` - copied verbatim from the sibling project (a generic,
reusable wavetable note-playing helper, not TinyJoypad-specific).

## A real bug found and fixed during Pong's own port: the menu-launch button bleeding into the game

The exact same class of bug the sibling project's own
`md_armInputFireGate()` exists to prevent, found here independently via a
Puppeteer screenshot rather than assumed to already be handled: launching
Pong from the menu (pressing A) landed straight in active gameplay
instead of showing the title screen first - the same physical A-press
used to *confirm* the menu selection was still held on the game's very
first real tick, and `gbPressed(BTN_A)` (an edge-detected "just pressed"
check) read it as a fresh press, instantly dismissing the title screen
before the player ever saw it.

**Fixed** by porting the sibling project's own fix directly:
`md_armInputAGate()` (a new `machineDependent.h`/`portVircon32.c`
primitive, mirroring `md_inputFireFrames()`'s own gate exactly) suppresses
`md_inputA()` until the physical button is actually released, called once
in `main()`'s own dispatch loop right when a game is launched. Verified
via Puppeteer: the title screen now holds correctly until a genuinely
fresh A-press dismisses it.

## Thumbnail atlas, quit-confirmation dialog, pixel-grid overlay, and global mute, ported from the sibling project

Prompted by a direct request to check this project against
`tinyjoypad_vircon32` for features it has that this one was missing.
Found four real gaps against that project's own
`portVircon32.c`/`menu.c`/CLAUDE.md and ported all four directly:

- **Menu thumbnail atlas** (`THUMBNAILS_TEXTURE_ID`/
  `md_getThumbnailCount()`/`md_drawGameThumbnail()` in `portVircon32.c`,
  wired into `menu.c` in the exact same layout as the sibling project's
  own) - a real gameplay screenshot, captured with Puppeteer (headless
  Chrome driving the Vircon32 web emulator - see
  `more games/DISCOVERED_GAMES.md`'s own methodology precedent for "verify
  against the real thing, don't assume"), not a mockup. The 640x360
  capture's own LCD-area crop (588x336 at `ORIGIN_X,ORIGIN_Y`) was scaled
  down to the sibling project's own 256x128 thumbnail cell size and
  composited into a new `assets/thumbnails.png` (a 4x1 grid - room for 3
  more games before a second texture is ever needed, the same "modest
  headroom" precedent as every one of the sibling project's own thumbnail
  atlases). Unlike `tools/gen_column_atlas.py`/`gen_pixelgrid.py`, this
  compositing step has no checked-in generator script, matching the
  sibling project's own precedent - it's a one-off manual step every time
  a new game's real screenshot needs baking in, not a deterministic
  pattern a script could regenerate from parameters alone.
- **Quit-confirmation dialog** (`confirmingQuit`/`drawConfirmQuitDialog()`
  in `portVircon32.c`) - Start, mid-game, now opens a YES/NO dialog
  (defaulting to NO) instead of instantly returning to the menu; the
  current game's own `update()` is not called at all while it's up, so
  gameplay is genuinely frozen behind it. A near-verbatim port of the
  sibling project's own dialog (same box position/size, same
  `md_drawSolidRect()` white-outline-black-interior trick reusing the
  column atlas's own region 255 solid tile), with Fire renamed to Button A
  throughout to match this project's own real Gamebuino button naming.
- **Pixel-grid overlay** (`pixelGridEnabled`/`drawPixelGridOverlay()`) -
  bound to Button L rather than the sibling project's own Button X, since
  X is already spoken for here (it's real Gamebuino Button C). A new
  588x336 `assets/pixelgrid.png` (7px grid lines, one per `TILE_SCALE`)
  was needed since the sibling project's own 640x320/5px asset is sized
  for its own different screen layout - generated via a real, checked-in
  `tools/gen_pixelgrid.py` (Pillow-based) rather than a one-off
  ImageMagick command the way the sibling project's own (unchecked-in)
  asset was made, so it can be regenerated if `TILE_SCALE` or the LCD
  dimensions ever change.
- **Global mute** (`audioMuted`, Button Y, `set_global_volume()`) - a
  direct, unmodified port; nothing about it was TinyJoypad-specific to
  begin with.

**The sibling project's own "toggle doesn't visually take effect" pixel-
grid bug (see its own CLAUDE.md) does not reproduce here**, reasoned
through rather than just assumed fixed by porting the same `onResume`-
forcing mechanism defensively: that bug was specific to `obonoCoreShim`-
lineage games, which can skip their own redraw indefinitely on an
unchanging frame (a dirty-flag gate). Every Gamebuino game here instead
redraws unconditionally through `gbUpdate()`/`gbRenderFrame()` every real
logic tick (`gbUpdate()`'s own `gbClear()` + full recomposite, matching
real hardware's own default `persistence=false` behavior) - so a stale
grid line or dialog frame left behind by a toggle/resume is naturally
painted over within at most one game tick (≤33ms at the default 30fps),
with no forced redraw needed for correctness. The `onResume` mechanism
(`menu.h`'s own field) is still wired up in both places for architectural
parity with the sibling project, in case a future game here ever adopts a
`persistence=true`-style skipped redraw - but every game today can, and
Pong does, pass `NULL`.

## EEPROM persistence, ported from the sibling project (1024 bytes, not 512)

Direct port of the sibling `tinyjoypad_vircon32` project's own
`eepromShim.h`/`.c` + `machineDependent.h`'s own `md_card*()` primitives -
same design throughout (a real Vircon32 memory card standing in for a real
EEPROM chip, one independent slot per game found/claimed by an open-
addressing hash table keyed by the game's own title rather than its
registration index, a magic number + checksum per slot to detect a
corrupted/torn write and fall back to fresh rather than trust garbage,
fresh cells defaulting to 255/0xFF to match real AVR EEPROM's own factory-
erased state) with exactly one real difference: **`EEPROM_SLOT_DATA_SIZE`
is 1024, not the sibling project's own 512** - real Gamebuino Classic
hardware runs on a genuine ATmega328(P), which has a real, full 1024-byte
EEPROM (confirmed against the real part's own datasheet), twice the
ATtiny85's 512 bytes that project's own constant was originally sized
around. A ported Gamebuino game's own `EEPROM.read()`/`EEPROM.write()`
call sites (mechanically renamed to this shim's own `eeprom_read_byte()`/
`eeprom_write_byte()`, since this dialect has no classes to preserve
Arduino's own `EEPROM` object's dot-call syntax) can therefore use the
real hardware's own full address range (0-1023) unmodified, with no risk
of this shim silently truncating an address a real cartridge would have
accepted.

`eepromSelectGame()` is wired into `portVircon32.c`'s own dispatch loop at
the same point the sibling project calls it - right after a game is
chosen from the menu, before that game's own `init()` runs - and this
project's own card signature (`"GAMEBUINOVIRCON01"`) is distinct text
from that project's own, so the two are never confused if the same
physical card is ever used with both cartridges. Verified via Puppeteer
against the current build: no regression to Pong's own menu/title/
gameplay flow with `eepromSelectGame()` now called on every launch (Pong
itself never calls into the new `eeprom_*()` functions, since it has no
high score to save - the emulator session used for verification also has
no memory card connected, exercising `eepromSelectGame()`'s own graceful
"card not connected" fallback path specifically, not the write path).
`shipwrek` (see `more games/DISCOVERED_GAMES.md`'s own porting-priority
audit) already ships a real `.eep` file upstream and is a natural first
real consumer whenever it gets ported.

## A frame-rate default bug: this shim defaulted to 30fps, real hardware defaults to 20

Found from a direct user question asking to double-check `gbBegin()`'s own
default frame rate against the real library rather than trust this
project's own prior assumption. Reading the real source directly
(`Gamebuino.cpp`'s own `Gamebuino::begin()`) settled it precisely:
`timePerFrame = 50;` is set directly there (not via a `setFrameRate(30)`
call, which is what this project had assumed without checking) - 1000ms /
50ms-per-frame = **20fps**, not 30. Easy to get wrong from documentation
alone: most `setFrameRate()` writeups findable online are actually for the
later META library (`Gamebuino-Meta.h`), a distinct board with its own
different 25fps default - neither number is this project's own real
target.

**Real impact, not just a cosmetic default**: Pong Solo's own upstream
`Pong.ino` never calls `gb.setFrameRate()` itself, so it was always meant
to run its entire gameplay logic (ball speed, paddle speed, opponent AI
tracking) at the real 20fps default - this shim's own wrong 30fps default
meant Pong had been running its whole game logic 1.5x too fast versus real
hardware ever since it was first ported, not just "slightly off." Fixed in
both `gbBegin()` (`gbFrameRateFps = 20`) and its own header comment in
`gamebuinoShim.h`.

**Also added while looking at this**: `gbSetFrameRate()` previously only
guarded against `fps <= 0`, with no upper bound at all. Real
`Gamebuino::setFrameRate(uint8_t fps)` has no explicit clamp of its own
either, beyond its parameter's real 8-bit range - but this shim's own
`fps` is a full-range `int`, and Vircon32's own engine physically only
ticks at 60fps regardless of what's requested (`gbUpdate()`'s own
accumulator would just return true on every single engine tick for
anything ≥60, identical to requesting exactly 60) - so `gbSetFrameRate()`
now clamps to `[1, 60]`, a shim-specific gate real hardware has no
equivalent need for, to keep any future ported game's own
`gb.setFrameRate(N)` call from behaving surprisingly for large `N`.

## Ten more games ported, `gbDrawBitmap()` added, and a project-wide color-inversion bug found and fixed

Prompted by "port next 10 games" (via parallel background agents, following
the porting-priority audit's own Tier 1/2 list) and a follow-up "create
drawBitmap in the shim" request once every game's own real bitmap art
turned out to matter for how recognizable each port actually looks.
`gbDrawBitmap()`/`gbDrawBitmapRotated()` were added to `gamebuinoShim.h`/
`.c`, ported bit-for-bit from real `Display::drawBitmap()` (including its
own real rotation/flip quirks - see the header comment on
`gbDrawBitmapRotated()`), then every already-ported game was revisited to
restore whatever real bitmap art it had been given a geometric placeholder
for instead (before this primitive existed).

**A project-wide color-inversion bug was found while restoring
FlappyBirdo's own art**: `tools/gen_column_atlas.py` had `lit=WHITE,
off=BLACK` baked into every one of the column atlas's 256 pre-baked tiles -
correct for the sibling `tinyjoypad_vircon32` project's own self-
illuminating OLED (lit pixel = light on dark), backwards for a real PCD8544
LCD (a reflective display - "set" pixel is dark ink on a light background).
Fixed in the generator (swap `lit`/`off`) and in `md_beginFrame()`'s own
`clear_screen()` call (black → white, so the "skip column value 0" fast
path stays correct against the new background) - regenerated
`assets/columns.png` from the fixed generator. This affected all 12 games
at once, not just FlappyBirdo.

Two more real, FlappyBirdo-specific bugs were found and fixed via direct
user visual reports during verification, both the same root cause (a
bitmap assumed "self-contained" without checking whether real upstream
draws a solid fill/mask layer underneath it first):
- **Pipes showed background bleed-through**: `flapPipeBitmap` is an
  outline-only sprite - real upstream draws a GRAY fill + WHITE highlight
  lines underneath it first (`drawPipes()`'s own real 3-layer draw order).
  Fixed by adding a BLACK `gbFillRect()` before the outline bitmap
  (GRAY substituted with BLACK, this project's own established
  convention for real hardware's dithered GRAY, which this shim has no
  equivalent for). The first attempt used the bitmap's own declared width
  (16) for the fill, which turned out to be 4px too wide - decoding the
  real bitmap bytes into ASCII art showed its real content only occupies
  the left 12 columns (matching the already-defined `FLAP_PIPEW` constant);
  the fill was corrected to that width once the extra 4px showed up as a
  visible white strip past the outline's own real right edge.
- **The bird had the same issue**: `flapBird1Bitmap`/`flapBird2Bitmap` are
  also outline-only, needing real upstream's own `bird1MaskBitmap`/
  `bird2MaskBitmap` drawn first. A plain rectangle was tried first as a
  stand-in mask, but produced a visible rectangular halo around the bird's
  actual (non-rectangular) silhouette - fixed by extracting and using the
  real mask bitmaps' own byte data instead of approximating with a
  rectangle.

An audit of every other ported game's own bitmap draw sites (Shufflepuck,
Maze, Simonbuino, SnakeClassic, Parachute, Conduit), prompted directly by
finding the two bugs above, found no further instances of the same
mistake - each of those either draws a genuinely self-contained/opaque
bitmap, already includes its own real mask/fill layer (Simonbuino's own
button masks, Conduit's own GRAY grid-overlay pixels), or draws full-frame
art with no background to bleed through.

A separate, unrelated FlappyBirdo bug was found via a live user report
("SLOW awarded 2 points per pipe instead of 1"): `flapUpdatePipes()`'s own
scoring check cast a pipe's real `float` x-position to `int` before
comparing it to `FLAP_PLAYERX` - on SLOW (a genuine 0.5px/frame step, see
below), a pipe's x crosses both a `.5` and a `.0` value on consecutive
frames, and truncating both to the same `int` fired `flapUpdateScore()`
twice per pipe. Fixed to a real float equality check
(`flapPipes[z].x == (float)FLAP_PLAYERX`), matching upstream's own real
`pipe[z].x == PLAYERX` comparison exactly (a genuine float equality, not a
cast-then-compare) - this also means FAST's own 2.0 step can legitimately
skip over `FLAP_PLAYERX` and miss a point for a given pipe, a preserved
upstream quirk, not a bug this port introduced.

**Ten games shipped this pass**: Shufflepuck Cafe (AWOT83, GPLv3), Taquin
(RackhamLeNoir, GPLv3), CrazyCar (Baptiste Pouget, GPLv3), Maze (Andy
O'Neill, MIT), Simonbuino (Jerom/Forklift5, unlicensed), Conduit (adekto,
MIT), FlappyBirdo (Forklift5, unlicensed), Parachute (Jicehel, unlicensed),
Snake Classic (Tnxec2, unlicensed), and Snake ABC (frthery, unlicensed) -
see each one's own header comment in `src/games/` for its own real porting
notes (upstream quirks preserved, dialect rewrites needed, etc), and the
Games table in `README.md` for licenses/sources. Every one of them is
registered in `menuGameList.c`, has a real Puppeteer-captured gameplay
thumbnail in `assets/thumbnails.png` (grown from a 4x1 to a 4x3 grid, 12
cells) and a real screenshot in `metadata/screenshots/`.

## Real Gamebuino fonts ported, replacing the borrowed 8x8 "myfont" table

Prompted by a direct request ("can we reuse the same font as on actual
gamebuino classic"), once font-width workarounds had piled up across
several ported games (Taquin's own two-digit tile numbers overlapping
their cell border, Conduit's/CrazyCar's own upstream text needing
shortening to fit, Simonbuino's own win message losing a real icon glyph -
all called out in this project's own earlier "Font fidelity" open
question). The real library's own `utility/font5x7.c`/`font3x5.c`/
`font3x3.c` (column-major, LSB-top, one real PROGMEM byte table per font,
indexed directly by ASCII 0-127 - including Gamebuino's own custom icon
glyphs replacing the usual control-character range 0-31) were ported
verbatim into `gamebuinoShim.c` as `gbFont5x7`/`gbFont3x5`/`gbFont3x3`,
`gbSetFont()` now takes one of them directly (previously a no-op), and
`gbDrawChar()`/`gbPrintString()` were rewritten as a direct port of real
`Display::drawChar()`/`Display::write()` - including real per-font inter-
char/line spacing (raw glyph size + 1, baked into `gbFontWidth`/
`gbFontHeight`) and real `'\n'` handling in `gbPrintString()` (a new
capability - the old fixed-step version had no line-break support at all).
`gbBegin()` now defaults to `gbFont3x5`, matching real hardware's own
`Display::Display()` default.

**A real bug this change surfaced, found via a live user report ("score
always drawn with black rectangle")**: `gbDrawChar()` used to hardcode
`gbColor = 1` internally regardless of the caller's own current color -
harmless while every ported game only ever printed BLACK-on-WHITE text,
but FlappyBirdo's own real `drawScore()` restoration (see below) needed a
genuine WHITE-on-BLACK digit (`gbSetColor(0)` before printing), and got an
invisible black-on-black digit instead. Fixed by having `gbDrawChar()`
respect whatever color `gbSetColor()` last set, like every other
`gbDraw*()` primitive in this shim (`gbDrawBitmap()` included) and like
real `Display::drawChar()` itself - audited every one of the 12 games' own
text call sites afterward to confirm none of them relied on the old
"always forces BLACK" behavior (only FlappyBirdo, Maze, and SnakeClassic
ever call `gbSetColor(0)` at all, and each of those three already restores
BLACK before its own next print call).

With real fonts now practical, several games' own real upstream text
layouts were restored verbatim instead of staying as font-width-driven
adaptations:
- **Pong Solo**: real `setFont(font5x7)`, called once at init and left set
  for the whole game (matching upstream exactly, title screen included).
- **FlappyBirdo**: the difficulty-select screen now matches upstream's own
  real `initDifficulty()` line-for-line (font5x7 "DIFFICULTY:" title,
  font3x5 hint text and per-difficulty "HIGH:" columns, the animated bird
  as upstream's own *only* real selection indicator - its own text arrow
  is dead code, commented out in the real source, so no ">"-style cursor
  is drawn here either). `drawScore()` also restored a genuine upstream
  behavior this port had dropped entirely: a filled BLACK outline box
  (widening once the score reaches double digits) with the score printed
  in WHITE on top, not plain BLACK text with no box.
- **Taquin**: tile numbers and the win screen now use upstream's own real
  font5x7/font3x5 split and exact centering formulas - no more two-digit
  tiles overlapping their cell border, and the win screen's own real
  "Press <arrow icon> to restart" line (the icon restored as a real glyph,
  ASCII 21, rather than substituted with plain "PRESS A" text).
- **Conduit**: `condStatsDraw()` now matches upstream's own real
  `StatsDraw()` positions/fonts exactly (font3x5 points at (64,43),
  font5x7 "WIN"/"GAME OVER"/move-counter) - `condUpdateMenu()`'s own PLAY
  GAME/EDIT LVL/RESET screen keeps its own bespoke layout since it's a
  hand-rolled replacement for upstream's own binary `gb.menu()` widget
  with no real per-pixel layout to restore.
- **CrazyCar**: the real game-over text ("Your score is: N" / "Press C")
  is restored verbatim - it used to be shortened to a "SCORE"/number/
  "PRESS C" layout to fit the old 8x8 font.
- **Simonbuino**: the real win message (`" CONGRATULATIONS!!\n     <reset
  icon> to reset"`) is restored verbatim, built as an explicit `int[]`
  array rather than a plain string literal since it needs both a real
  `'\n'` and a non-printable icon glyph (`\27`, ASCII 23) a quoted string
  literal can't hold directly - upstream's own odd `\    ` (backslash
  followed by 4 spaces, not a standard C escape) is reproduced as literal
  spaces, matching what real avr-gcc does with an unrecognized escape.

Every other game's own text (Agaruino, Shufflepuck, Maze, Snake Classic,
Snake ABC) never called `setFont()` upstream either, so it just inherits
the new real `gbFont3x5` default with no code changes needed - smaller and
closer to real hardware than the old 8x8 table by construction.

**Parachute** needed one more real primitive, found via a direct
side-by-side comparison against a real-hardware screenshot: its own
hand-tuned `gbCursorX = 48` score position (adapted for the old 8x8 font)
was restored to upstream's own real "Score :    " text + real `cursorX =
30`, but the restored text then showed the background bitmap's own real
palm-tree pixels bleeding through between glyphs - upstream's own
`setColor(BLACK, WHITE)` call (a real opaque WHITE text background, not
just BLACK ink) had no equivalent here, since this shim's `gbSetColor()`
only ever took one argument. Added `gbSetColorBg(color, bg)`, a direct port
of real `Display::setColor(color, bg)`'s own two-argument overload, and
`gbDrawChar()` now draws its own "off" bits in `gbBgColor` whenever it
differs from `gbColor` (matching real `Display::drawChar()`'s own
`bgcolor != color` check exactly) - `gbSetColor(color)` alone still sets
both to the same value, so every other already-ported game's own existing
transparent-text behavior is unaffected. Checked every other ported game's
own real upstream source for the same two-argument `setColor()` call
afterward: Maze's own `setColor(BLACK, BLACK)`/`setColor(WHITE, WHITE)`
always pass identical colors (and aren't even used for text), and Conduit
only ever uses the single-argument form - Parachute was the only real
instance of a genuine opaque-background text draw.

## Performance pass: eliminating per-pixel call overhead in the hot draw paths

Prompted by a direct question about whether FlappyBirdo/Parachute (the two
heaviest-drawing games) could ever hit Vircon32's own real per-frame cycle
budget. Measured first, before changing anything: both held a steady,
unbroken 60fps under Puppeteer even in the worst-case host environment
(headless Chrome, software-rendered SwiftShader GL) - no actual problem
today. But reading Vircon32's own documented performance model (15MHz ≈
250,000 instructions/frame at 60fps; `IMUL`/`IDIV` cost the same as a shift,
no benefit from avoiding them; but **every function call costs a flat
~10+2×argcount instructions of pure overhead**, `-O` flags are parsed and
ignored, so "call elimination in hot loops" is the one lever that actually
pays off on this ISA) found a real, systemic inefficiency worth fixing
anyway: every drawing primitive in this shim (`gbFillRect`,
`gbDrawFastHLine`/`VLine`, `gbDrawBitmap`, `gbDrawBitmapRotated`) called
`gbDrawPixel()` once per individual pixel - `gbFillRect` alone was a 3-deep
call chain (`gbFillRect` → `gbDrawFastHLine` → `gbDrawPixel`) with 3
separate call overheads paid per pixel, for arithmetic (bounds check + a
div/mod) that costs nothing extra on this platform.

Rewrote `gbDrawFastHLine()`/`gbDrawFastVLine()`, `gbDrawBitmap()` (the
axis-aligned/unrotated case - this shim's single hottest path, used by
every ported game's own sprite/title/UI art), and `gbDrawBitmapRotated()`
to compute the destination LCD page+bit once per row/column instead of
once per pixel, then write directly into `gbFrameBuffer[]` inline - zero
`gbDrawPixel()` calls left in any of the hot loops. `gbDrawBitmapRotated()`
was the trickiest of the four: for every one of its 4 rotations, the
destination x-offset (`k`) always turns out to be a function of exactly one
of the two source loop variables, and the destination y-offset (`l` - which
LCD page+bit gets written) a function of the other one, true both before
and after flip is applied (flip only ever remaps `k` using `k` itself, and
`l` using `l` itself, never mixing the two) - so whichever loop variable `l`
depends on is made the outer loop, hoisting the page+bit computation out of
the inner one. All of this is a **pure Vircon32-specific performance
rewrite, not a behavioral change** - real hardware's own actual shipped
`Display.cpp` code is the exact naive per-pixel version this replaced for
`fillRect`/`drawFastHLine`/`drawFastVLine` (a real AVR's own per-call cost
is small, so there was never a reason for real hardware to avoid it), and
gbDrawBitmap()'s own optimization mirrors real hardware's own *actual*
optimized code for that one function specifically (confirmed by reading
`Display.cpp` directly - the naive version is sitting right there in a
comment above it).

Verified via a full 12-game Puppeteer regression pass, diffed pixel-for-
pixel against pre-optimization captures: 8 of 12 games render byte-for-byte
identical output (Conduit, CrazyCar, Parachute, Shufflepuck, Simonbuino,
Snake ABC, Snake Classic, Taquin); the remaining 4 (Agaruino, FlappyBirdo,
Maze, Pong) only differ because of each one's own inherent gameplay
randomness between two independent captures (confirmed directly - e.g.
Taquin's own shuffle, Maze's own layout, ball/bird position - not a
rendering difference), and of the 4 games that actually call
`gbDrawBitmapRotated()` (FlappyBirdo, Parachute, Shufflepuck, Simonbuino),
3 are pixel-identical and FlappyBirdo's own pipes/bird were additionally
checked directly by eye.

**Checked the sibling tinyjoypad_vircon32 project's own `OPTIMIZATIONS.md`
directly for any of its per-game lessons that might transfer here.** Most
don't: that project's entire optimization story is built around eliminating
O(pixels x objects) work in a per-pixel *rendering callback* (the engine
asks "what color is pixel N", and a naive game rescans every object per
pixel) - this project has no equivalent to eliminate, since games here draw
into a real CPU-writable framebuffer with explicit imperative calls
(`gbFillRect`/`gbDrawBitmap`/etc), the same shape that project's own
"composite per row/object instead of per pixel" fixes were themselves
converging *toward*. One lesson did transfer directly, though: "a self-
gated function still costs a full call every time it's invoked - gate the
call site too, not just the function body" (documented there repeatedly,
e.g. Falling Blocks/Astro Barrier/Tiny Mania). `gbRenderFrame()` was exactly
this - it called `md_drawColumn()` unconditionally for all 504 column x
page cells every frame, and `md_drawColumn()` already self-gates a blank
column (`if (value==0) return`) but only *after* paying that call's own
~10+ instruction overhead. Both FlappyBirdo and Parachute have large blank-
sky regions, so a real fraction of those 504 calls/frame were guaranteed
no-ops paying for nothing. Fixed by gating at the call site instead (the
exact same `& 0xFF` zero-check `md_drawColumn()` itself uses, just moved
earlier) - a blank column now costs one array read + one compare instead of
a full call. Verified with the same full 12-game regression pass: the 5
games with no meaningful runtime randomness (Conduit, CrazyCar, Parachute,
Pong, Shufflepuck) render byte-for-byte identical to the pre-fix captures;
the other 7 differ only by gameplay state (spot-checked FlappyBirdo/Maze/
Simonbuino directly - clean rendering throughout, e.g. Simonbuino's own
diff is simply a different, still-correct pad lit in its pattern sequence).

## Ten more games ported (batch 2), two real shim bugs found via parallel agents

Prompted by a second "port next 10 games" request, using the same parallel-
background-agent workflow as the first batch, now with every lesson from
this project's own history folded into each agent's own shared prompt
(real bitmap/font restoration up front, the mask/fill bleed-through bug
class, unique naming prefixes, the exact dialect gotchas). All 10 agents
were also told explicitly to flag anywhere they had to work around
something the shim didn't support, per a direct follow-up request to check
for exactly that once they finished.

**Two genuine shim bugs were found this way, not just documented
limitations**:
- **`gbDrawChar()` existed in `gamebuinoShim.c` from the real-font work
  earlier this session, but was never declared in `gamebuinoShim.h`** -
  every `gbPrintString()` call already used it internally, but no game file
  could call it directly. `gameMinesweeper.c`'s own port (needing real
  upstream `gb.display.drawChar(x,y,c,size)` for its smiley/digit/flag
  glyphs) had no way to know the real primitive already existed, so it
  built a local `mineDrawChar()` wrapper around `gbPrintString()` instead.
  Fixed by adding the missing declaration to the header and switching
  `gameMinesweeper.c` back to the real, direct `gbDrawChar()` call.
- **`gbRepeat(button, period)` didn't match real `Buttons::repeat()`** -
  confirmed by reading the real `Buttons.cpp` source directly
  (`more games/Gamebuino-Classic/utility/Buttons.cpp`). Real hardware
  treats `period <= 1` as "fire on every single held frame" (a genuine
  continuous repeat); this shim's own version instead fired once on the
  first held frame and then never again for `period <= 0` - the opposite
  behavior. Real hardware's own modulo target for `period > 1` is also
  `state % period == 1`, not `== 0` - this shim's old formula fired the
  very first repeat one frame early relative to real hardware on every
  cycle after the first. `gameAsterocks.c`'s own real ship controls
  (`gb.buttons.repeat(BTN_x, 0)` for thrust/hyperspace/rotation - a literal
  upstream call, confirmed directly in `specific.ino`) needed the real
  period-0 continuous-repeat behavior and, since it couldn't touch the
  shared shim file itself, worked around it locally with `gbHeld(btn, 1)`.
  Fixed `gbRepeat()` itself to match real hardware exactly (verified this
  doesn't regress any of the 12 already-shipped games' own `gbRepeat()`
  calls: `period == 1` behaves identically to before, `period > 1` shifts
  by one frame on repeat cycles after the first - imperceptible for the
  menu/cursor-style navigation every current user of it needs it for), then
  restored `gameAsterocks.c`'s own call sites to the real, literal
  `gbRepeat(BTN_x, 0)` upstream itself calls.

A third flagged item (`gameCatcher.c`'s own possible Button-C soft-lock,
where a C-release during upstream's own level-up/title screens might leave
a stuck "restart" bit) was investigated against the real `Buttons.cpp`
source too and confirmed to be genuine, load-bearing real-hardware
behavior, not a porting artifact - `Buttons::update()` is called
unconditionally every real tick just like this shim's own
`gbUpdateButtons()`, and real `released()` is exactly as single-tick-pulse
as this shim's own `gbReleased()`. Preserved as-is, per this project's own
norm, with the port's own header comment updated to state this definitively
instead of leaving it as an open uncertainty.

**Ten games shipped this pass**: Firemen (Vicking69, GPLv2 - see the
License section in `README.md` for a real, unresolved GPLv2/GPLv3
compatibility concern this specific game raises), Gamebuino-Catcher
(qubist, unlicensed), UFO-Race (Rodot, unlicensed - this project's first
genuine functional EEPROM consumer, ahead of the previously-expected
`shipwrek`), Gamebuino-Minesweeper (dirksteindorf, unlicensed),
yoda-killrace (Yoda Zhang, unlicensed - first of this author's 5 `yoda-*`
games to be ported), blockdude-gamebuino (Sorunome, unlicensed),
yoda-lander (Yoda Zhang, unlicensed), gamebuino-punkt (Andy O'Neill, MIT),
yoda-invaders (Yoda Zhang, unlicensed), and yoda-asterocks (Yoda Zhang,
unlicensed) - see each one's own header comment in `src/games/` for its
own real porting notes, and the Games table in `README.md` for sources.
Every one is registered in `menuGameList.c`, verified crash-free via a full
22-game Puppeteer smoke test, and has a real gameplay thumbnail (the
thumbnail atlas grew from a 4x3 to a 4x6 grid, 24 cells) and screenshot.
Tier 1 of the porting-priority audit is now fully shipped (`firemen` was
its last remaining entry); this batch also covers most of Tier 2.

**A menu-navigation gotcha worth remembering for future verification
sessions**: `menu.c` displays games in real alphabetical order (a
selection sort by title, confirmed directly in its own source), not
`addGames()`'s own registration order - a Puppeteer script that assumes
"Nth `addGame()` call = Nth menu position" will silently capture the wrong
game once enough titles are registered to make the two orders diverge
(registration order and alphabetical order only coincidentally agree for
a handful of early entries). Caught only after a smoke-test screenshot
"for firemen" turned out to show Minesweeper's own board.

## Twenty more games ported (batch 3), five more shim primitives promoted, MAX_GAMES raised, and a real GPU texture size ceiling discovered

Prompted by a third "port next 20 games" request (double the size of batch 2),
using the same parallel-background-agent workflow, with the same explicit
"check for and fix shim workarounds afterward" instruction carried forward.
All 20 agents finished; reviewing their reports found five real, genuine
shim gaps (not just documented limitations) - each one independently
reinvented by multiple games, the same "promote once three-plus ports hit
the exact same wall" bar this project has used since the `gbDrawChar()`/
`gbRepeat()` fixes in batch 2:

- **`gbFrameCount`** (a real, free-running frame counter mirroring
  `gb.frameCount`) - independently reinvented by 8 different games across
  batches 2 and 3 before being promoted. Added as a real shim global,
  incremented once per real logic tick inside `gbUpdate()` (matching real
  hardware's own placement) and reset in `gbBegin()` (a shim-specific
  addition real hardware never needed, since one cartridge session here
  runs many different games sequentially). Every game that had built its
  own local substitute (`gameCatcher.c`, `gameBlobAttack.c`,
  `gameCopter.c`, `gameDigger.c`, `gameGlaciGlaca.c`, `gameGruniozerca.c`,
  `gamePunkt.c`) had its own local counter removed and every read switched
  to the real primitive - `gameKillrace.c`'s own similarly-named
  `killManFrameCounter` was checked and correctly left alone, since it's a
  genuinely distinct per-level animation-cycle timer, not a `gb.frameCount`
  stand-in.
- **`GB_INVERT` bitmap-level bug**: `GB_INVERT` (a real third XOR draw
  color, added earlier this session) was documented as supported by
  `gbDrawBitmap()`, but that function's own body hadn't actually landed the
  check yet at the moment a concurrent agent (`gameDescent.c`) went looking
  for it - a real, transient one-primitive-at-a-time rollout gap, not a
  design mistake. Confirmed fixed by the time all agents finished reading
  the shim; `gameCrazyTown.c`'s own real INVERT call site (a "TAXI" badge
  label) was restored to `GB_INVERT` directly once confirmed working,
  replacing its own provably-equivalent-only-for-that-one-case WHITE
  substitution.
- **Real `Display::fillScreen()` hardware bug** (color argument silently
  ignored, always fills solid black) - found while investigating
  `gameHexagon.c`'s own flagged "no `fillScreen(INVERT)`" gap. Reading real
  `Display.cpp` directly settled it: upstream's own `fillScreen(INVERT)`
  never actually inverts anything on real hardware either, so the "right"
  fix was to make `gbFillScreen()` match this real bug (not invent real
  INVERT-fill support the hardware itself doesn't have), then simplify
  `gameHexagon.c`'s own local per-pixel invert-flash workaround down to a
  plain `gbFillScreen(1)` call - correct, not a downgrade.
- **`gbCollideBitmapBitmap()`/`gbGetBitmapPixel()`** (real pixel-perfect
  bitmap-vs-bitmap collision, ported from `Gamebuino::collideBitmapBitmap()`/
  `Display::getBitmapPixel()`) - `gameDescent.c`'s own single most-used
  primitive (9 real call sites), with no shim equivalent at all. Promoted
  directly; `gameDescent.c`'s own local version (plus its own now-redundant
  `gbAbsInt()`/`gbAbsFloat()` workarounds - `gbAbsInt()` was already fixed
  earlier this session, and `fabs()` from `math.h` was already proven
  working by `gameCopter.c`/`gameCrazyTown.c`/`gameHexagon.c`) was removed
  in favor of the real primitives.
- **`gbFillTriangle()`/`gbDrawRoundRect()`/`gbFillRoundRect()`** (direct
  ports of real `Display::fillTriangle()`/`drawRoundRect()`/
  `fillRoundRect()`, the latter two built on new internal
  `gbDrawCircleHelper()`/`gbFillCircleHelper()` quadrant helpers matching
  real `drawCircleHelper()`/`fillCircleHelper()`) - each flagged
  independently by 2-3 games (`gameHexagon.c`'s filled wall wedges;
  `gameBlocksBuino.c`'s/`gameSnakeAbc.c`'s dropped decorative menu arrows;
  `gameCrazyTown.c`'s/`gameWhg.c`'s square-cornered rect approximations).
  All five affected games were restored to the real primitives at their
  real upstream coordinates/radii once added.
- **`gbPopup()`** (a direct port of real `Gamebuino::popup()`/
  `updatePopup()` - a small auto-dismissing, slide-in bordered text box)
  reinvented independently by 7 different games across batches 2 and 3
  (`gameMaze.c`, `gameUfoRace.c`, `gameMinesweeper.c`, `game2048.c`,
  `gameGruniozerca.c`, `gameShipwrek.c`, `gameCastleDefence.c`) - the
  clearest, most-repeated gap found this pass. Promoted as a real
  `gbPopup(text, duration)` + an internal `gbUpdatePopup()` called
  automatically from `gbRenderFrame()` (matching real hardware's own
  automatic call from inside `Gamebuino::update()` - a game only ever
  calls `gbPopup()` itself, exactly like real hardware's own one-call
  contract). `gbPopupTimeLeft` is reset in `gbBegin()` for the same
  cross-game-launch reason `gbFrameCount` already is. Migrated
  `gameMaze.c`/`gameUfoRace.c`/`gameShipwrek.c`/`gameGruniozerca.c`/
  `gameCastleDefence.c` to the real primitive, restoring each one's own
  real upstream duration exactly. **Deliberately left un-migrated**:
  `gameMinesweeper.c` (its own persistent, non-fading WON/LOST message box
  is genuinely correct for a game whose `game_state` freezes indefinitely -
  switching to the real primitive's own timed auto-dismiss would make the
  message vanish while the game stays frozen underneath, a real regression,
  not a fix) and `game2048.c` (its own local overlay deliberately force-
  locks the font to the smallest size so its two longest milestone strings
  still mostly fit - the generic primitive draws in whatever font happens
  to be active, a real risk this port can't verify without a full
  rebuild+playtest across every state transition).

**A real, silent integration bug found while wiring up the 20 new games**:
`menu.c`'s own `MAX_GAMES` was still `32` - `addGame()` silently drops any
call once `gameCount` reaches this cap, with no error of any kind. Adding
all 20 new games on top of the already-registered 22 would have quietly
dropped the last 10 with zero indication anything was wrong. Caught before
it could bite, not after - raised to 48 (matching this project's own
"modest headroom" convention elsewhere, e.g. the thumbnail atlas's own
spare cells), with `eepromShim.c`'s own stale comment referencing the old
32 value fixed too (its own hash-table sizing is derived from the same
constant, so this was a documentation fix, not a behavior change there).

**A real Vircon32 GPU texture size ceiling discovered empirically**: naively
growing the existing 1024x768 thumbnail atlas taller (to 4x11, 1024x1408,
the seemingly obvious next step for 44 cells) was tried first and rejected
outright by `packrom` ("texture size is larger than allowed by Vircon32
GPU") - a real, hard limit, not a bug in this project's own tooling. The
actual ceiling is 1024x1024 (confirmed directly, not assumed). Fixed by
growing the existing texture only up to that real ceiling (4x8, 1024x1024,
32 cells - registration indices 0-31) and adding a genuine second texture,
`THUMBNAILS2_TEXTURE_ID` (id 3, appended after `PIXELGRID_TEXTURE_ID`
rather than inserted before it, matching this project's own "append, don't
insert" precedent for texture ids), sized 1024x384 (4x3, 12 cells) for
registration indices 32 and up - the two-texture design this file's own
much earlier comment had already anticipated as the likely next step, just
triggered by a real size ceiling rather than running out of an arbitrarily-
large single texture. `md_drawGameThumbnail()` now branches on
`THUMBNAIL_SPLIT` (32) to pick the right texture/region; `rom.xml`/
`Make.bat`/`Make.sh` all updated to pack/convert the new `thumbnails2.png`
alongside the original.

**Every one of the 20 new games required a real, individually-worked-out
Puppeteer input sequence to reach genuine gameplay** (not just a title
screen) for its own thumbnail/screenshot capture - several needed more than
a simple "launch + one A press" (`gameCastleDefence.c`'s own weapon-select
flow needed 4 real button presses across 3 different inputs;
`gameBlobAttack.c`'s/`gameSmash.c`'s/`gameCrazyTown.c`'s own internal main
menus needed real D-pad navigation to reach the actual "play" option rather
than whatever the menu defaults to; `gameDescentIntoHell`'s own real 10fps,
20-tick intro animation needed a longer real-time wait than every other
game's own title-dismiss). All 20 confirmed crash-free and genuinely
playable this way, with a real gameplay screenshot (not a placeholder or a
menu screen) captured for every one.

**Twenty games shipped this pass**: Gruniozerca (arhneu / Arkadiusz "Dark
Archon" Kaminski, Unlicense), Video Poker (Mike Del Pozzo, GPLv3), Blob
Attack (LudumDareDevelopment, unlicensed), Smash-and-Crash (Skyrunner65,
unlicensed), Gamebuino2048 (Josiah Winslow, unlicensed - recovered via
direct Mediafire download, no live repo), Armageddon (wuuff, GPLv3),
Skibuino (Mike Del Pozzo, GPLv3), microHexagon (valdenthoranar, unlicensed),
Jezzball (RackhamLeNoir, GPLv3), shipwrek (yawn-g, unlicensed - genuinely
has zero real EEPROM calls upstream despite shipping a `.eep` stub, see its
own header comment), Paqman (Yoda Zhang, unlicensed), BlocksBuino (frthery,
unlicensed), World's Hardest Game (Sorunome, unlicensed), CrazyTown
(Clement83/Clement Quintard, unlicensed), Copter (Clement83, unlicensed),
ZombiEscape (Frakasss, unlicensed), GlaciGlaca (Clement83, unlicensed),
Descent into Hell (etienne72230, unlicensed), Castle Defence (kh9282,
unlicensed), and Digger (scmar, unlicensed) - see each one's own header
comment in `src/games/` for its own real porting notes, and the Games table
in `README.md` for licenses/sources. Every one is registered in
`menuGameList.c`, verified crash-free and genuinely playable via Puppeteer,
and has a real gameplay thumbnail (the thumbnail atlas grew from a single
4x6/24-cell texture to a 4x8/32-cell primary texture plus a new 4x3/12-cell
second texture) and a real screenshot in `metadata/screenshots/`.

## Real GRAY color, plus an opt-in real-solid-gray rendering mode

Prompted by a direct question ("can we make GRAY color usage in games
actually draw a grey color"). Reading real `Display::drawPixel()` directly
(`utility/Display.h`) settled exactly how real hardware fakes a third shade
on a strictly 1-bit LCD: a checkerboard dither that also flips with the
frame counter (`(gbFrameCount&1) != ((x&1)^(y&1))`), enabled by default on
real hardware (`ENABLE_GRAYSCALE=1` in `settings.c`, not an optional
feature as some online writeups suggest). Added as a real `GB_GRAY` color
constant, wired into all 6 real drawing paths
(`gbDrawPixel`/`gbDrawFastHLine`/`gbDrawFastVLine`/`gbDrawBitmap`/both
`gbDrawBitmapRotated` loop variants) - everything built on top (circles,
triangles, rounded rects, `gbDrawLine`, `gbDrawChar`) inherits it for free.

Then swept every already-ported game that had documented a "GRAY has no
equivalent, substituted with BLACK/WHITE" workaround and restored the real
call, checking real upstream source for each: `gameConduit.c` (grid
overlay), `gameCopter.c` (city skyline), `gameHexagon.c` (connecting
spokes), `gamePunkt.c` (target halo), `gameSimonbuino.c` (direction-pad
masks), `gameShipwrek.c` (6 sites - boat overlays, sunk-boat reveal,
waterline band, aim crosshair), and `gameFlappyBirdo.c` (the biggest one:
restored the real top-of-sky GRAY band plus its own real `skyMaskBitmap` -
never even ported before, extracted fresh from upstream - and the real
3-layer pipe draw and 4-layer ground band, both of which had been
significantly simplified in earlier passes). One real coordinate bug was
caught and fixed while restoring the pipe highlights: the top pipe's own
horizontal highlight line belongs at its real gap-facing opening edge
(`y - PIPEGAPV - 1`), not near its far top edge - confirmed against
`Pipes.ino` directly rather than assumed symmetric with the bottom pipe.

**A follow-up request** ("add a config DEFINE to actually draw a grey color
on real screen output instead of the flickering") led to a real second
rendering mode, `GB_REAL_GRAY_COLOR` (a plain `#define`/comment-out toggle
in `gamebuinoShim.h`'s own Configuration section - this dialect's
preprocessor only supports `#ifdef`/`#ifndef`, confirmed via the sibling
project's own `VIRCON32_C_DIALECT.md`, so it's a presence/absence flag, not
a `0`/`1` value tested with `#if`). Off by default, matching real
hardware's own authentic flicker; when defined, GRAY renders as a real,
solid, flat gray instead.

A genuine engineering question came up directly: could this be done in a
single render pass instead of two? Investigated and answered concretely,
not assumed: Vircon32's GPU only blits whole pre-baked 8-pixel-tall tiles
selected by one byte value - a true single-pass version would need one
tile per 3-color (white/black/gray) combination of 8 pixels, 3⁸=6561
combinations per column versus the 256 actually affordable (the same real
GPU texture-size ceiling already hit once this session, packing 42
thumbnails). A real single-pass alternative exists at whole-byte-column
granularity (any BLACK draw anywhere in a column reverts that whole column
to normal rendering) but was rejected after presenting the real trade-off
directly: FlappyBirdo's black pipe outline and Simonbuino's black button
icons both share real byte-columns with GRAY fill, so byte-granularity
would visibly flatten the dither exactly where those games' own bitmap
detail matters most. Kept the real two-pass design instead: a parallel
`gbGrayBuffer[]` bit-plane (addressed identically to `gbFrameBuffer[]`,
marking exactly which "on" bits are specifically gray - every drawing
primitive clears the corresponding bit whenever it draws any other color,
so a later BLACK/WHITE/INVERT draw over a previously-gray pixel "un-grays"
that exact pixel without disturbing its neighbors), rendered via a second,
targeted `gbRenderFrame()` pass (`md_drawColumnGray()`) using a real
semi-transparent gray-tinted atlas (`assets/columns_gray.png`,
`tools/gen_column_atlas_gray.py` - same 256-tile-per-byte-value layout as
the existing column atlas, but "on" bits are opaque mid-gray and "off"
bits are fully transparent, so only the actual gray pixels get recolored,
leaving true black pixels in the same byte-column untouched underneath) -
drawn on a new `GRAYCOLUMNS_TEXTURE_ID` (id 4, appended after
`THUMBNAILS2_TEXTURE_ID`, matching this project's own "append, don't
insert" texture-id precedent). Gated exactly like the existing black/white
pass (a cell with no gray content at all costs one array read + one
compare, not a draw call), so the extra pass only costs anything for
cells that actually contain real gray content - a small, bounded subset
of the 504 total per frame in every game checked.

Verified both modes via Puppeteer against `gameFlappyBirdo.c`/
`gameCopter.c`/`gameHexagon.c`/`gameSimonbuino.c`: default mode shows the
real checkerboard dither, the real-gray mode shows a real solid gray
with the black pipe outline still crisp on top - confirming genuine
per-pixel accuracy, not the byte-level approximation that was considered
and rejected.

**Promoted from a compile-time `#define` to a real runtime toggle**,
bound to Button R, per a direct follow-up request to let a player switch
modes live rather than needing a separate build. `gbRealGrayColor`
(`gamebuinoShim.h`/`.c`) is a plain `bool` global, flipped by
`portVircon32.c`'s own dispatch loop the exact same way Button L/Y flip
`pixelGridEnabled`/`audioMuted` - `gbGrayBuffer`/`gbAnyGrayDrawn` are now
unconditionally allocated (no `#ifdef` at all) since the flag can change
mid-session, and every one of the 6 real drawing primitives branches on
`gbRealGrayColor` at runtime instead of at compile time.

A direct follow-up question ("does this have a big performance cost?")
led to a real, measurable finding, not just a reassurance: the *first*
version of this runtime toggle still cost something even in games that
never draw `GB_GRAY` at all, whenever the toggle was left on - every
draw of every OTHER color (BLACK/WHITE/INVERT) unconditionally cleared
its own `gbGrayBuffer` bit(s) too, to un-gray any pixel a previous gray
draw might have left there, and that clear ran regardless of whether
this particular game ever draws gray in the first place. Confirmed
directly (not just reasoned about) via a live report of visibly higher
emulator CPU usage in non-gray games while the toggle was on. Fixed by
gating every one of those clear sites on `gbAnyGrayDrawn` too, not just
`gbRealGrayColor`: `gbGrayBuffer` is provably still all-zero at any point
in a frame before the first real gray draw happens (nothing has set a
bit in it yet), so the clear is a safe no-op to skip until that first
gray draw actually occurs - and for a game that never draws gray at all,
`gbAnyGrayDrawn` never becomes true for the entire session, so every one
of those clear sites costs nothing, ever, in that game, independent of
whether the global toggle is on or off. Re-verified via Puppeteer
(FlappyBirdo's own sky/city/pipes, toggled both directions mid-game,
repeated across several independent runs) that this second-pass
optimization didn't regress the real per-pixel accuracy the two-pass
design exists for.

Net performance shape: zero cost in every game while the toggle is off
(no primitive ever sets `gbAnyGrayDrawn`, so the clear sites and
`gbRenderFrame()`'s own second pass both skip entirely); zero *added*
cost in a game that never draws `GB_GRAY` regardless of the toggle's own
state, for the same reason; and only in a game that actually draws real
gray, with the toggle on, does the real per-pixel bookkeeping cost
anything - bounded to the small subset of the 504 total column/page
cells that actually contain gray content, same order of magnitude as the
existing black/white pass's own already-established per-cell gating.

## Ten more games ported (batch 4), a real cross-game text-positioning bug found and fixed, and the rest of Tier 3 cleared

Prompted by a fourth "port next 10 games" request, covering the entire
remainder of the porting-priority audit's own Tier 3 list in one pass
(`Crabator`, `Gamebuino-SuperSpaceShooter`, `B-Rally`, `Artillery`,
`gamebuino-solitaire`, `makerbuino-firebuino`, `101Starships`, `tetrino`,
`Maruino`, `Super-Crate-Buino`) - the same parallel-background-agent
workflow as batches 2/3, but with a real workflow change forced by scale:
with 10 agents genuinely running concurrently and no git repository in
this project to give each one an isolated worktree, letting all 10 write
directly into the shared project tree (`menuGameList.c`, the thumbnail
atlas, `README.md`, the shared shim) risked real concurrent-write
corruption. Fixed by giving each agent its own full, private copy of the
project (`src`/`assets`/`libs`/`metadata`/`tools`/build scripts plus only
that agent's own one staged game directory) to build and Puppeteer-verify
in - each agent registers its game freely in its *own* copy's
`menuGameList.c` for its own testing, but the orchestrating session
integrates every finished port into the real tree afterward, one at a
time, in a fixed order - the exact same "agents produce isolated
deliverables, one session integrates" shape this project's own batch-3
`MAX_GAMES`/thumbnail-ceiling lessons had already been pointing toward,
just now load-bearing instead of optional.

**Two real shared-shim bugs found this pass, both fixed and pixel-
verified**, not just documented limitations:
- **`gbDrawCharPixel()`'s own real fontSize=2 positioning bug**,
  independently found and fixed by two separate agents (`makerbuino-
  firebuino` and `101Starships`) hitting the exact same wall - real,
  confirmed proof this project's own "promote once multiple ports hit the
  same bug" bar still applies to bug*fixes*, not just missing primitives.
  The function scaled its own already-cursor-summed pixel coordinate by
  `gbFontSize` (`(x+col)*2`) instead of scaling only the glyph-local
  offset before adding it to the unscaled cursor position (`x+col*2`) -
  identical only when the cursor sits at `(0,0)`, silently shifting (or,
  once `x*2` alone exceeds the LCD's own real width/height, pushing
  entirely off-screen) any size-2 text placed anywhere else. Fixed at the
  two `gbDrawChar()` call sites rather than inside `gbDrawCharPixel()`
  itself (keeping that function's own simple "already-final-pixel" single
  responsibility, matching one of the two independently-submitted fixes).
  **Real, player-visible impact on an already-shipped game**: Pong Solo's
  own score digits (`gbCursorX=15`/`57`, fontSize 2) had been rendering at
  the wrong horizontal position - `(15+0)*2=30` and `(57+0)*2=114` (fully
  off the real 84px-wide screen) instead of the real, correct `15`/`57` -
  since the very session this shim's real fonts were ported. Confirmed
  both the bug and the fix directly, not just by reasoning about the
  formula: measuring the actual on-screen pixel columns of Pong's own
  score digits before and after landed exactly on the buggy vs. correct
  predicted positions. `gameDescent.c`'s own size-2 "Game Over!" text
  (`gbCursorX=0`) was unaffected on the X axis (the bug is a no-op at
  `x=0`) but was still shifted vertically (`gbCursorY=10`); `gameCrabator.c`'s
  own "LET'S GO!" intro text (`gbCursorX=6`) re-verified correct after the
  fix. No other already-shipped game calls `gbFontSize=2` at all.
- **`gbTimeHeld()`** (a direct port of real `Buttons::timeHeld()` - the
  actual real per-tick hold-duration counter, not just a threshold check)
  was missing entirely; `Maruino`'s own real variable-height jump
  (`gb.buttons.timeHeld(BTN_A)`, taller the longer A is held for the first
  few ticks) needed the real number itself. A one-line passthrough of the
  same `gbBtnHeld[]` counter `gbHeld()`/`gbPressed()` already maintain
  internally - promoted directly, no other games affected.

Every one of the other 8 finished ports reported zero shim gaps at all -
confirmed independently by diffing each agent's own isolated
`gamebuinoShim.h`/`.c` copy against the pre-batch original rather than
just trusting each report's own text, the same "verify, don't just trust
the summary" standard this project has applied to every other claim
throughout its history. Every local per-game workaround found this way
(one-shot-tone sound approximation, hand-built title screens, no on-
screen keyboard, `Super-Crate-Buino`'s own local `gbPopup()`
reimplementation for its own real force-cancel-on-death need) matches an
already-established precedent from an earlier batch, not a new gap.

**Real structural firsts this batch**: `Gamebuino-SuperSpaceShooter` and
`gamebuino-solitaire` are this project's first two ports with **no
upstream `.ino` file at all** - genuine, class-based C++ (`Bullet`/
`BulletManager`/`Enemy`/`EnemyManager`/`Player`/`EffectsManager` for the
former; `Card`/`Pile`/`UndoStack` for the latter) - both flattened into
plain data-only structs plus free functions taking an explicit index or
pointer, the same "flatten a real single-instance C++ library into plain
C" treatment this project has always applied to the Gamebuino API itself,
here extended to a *game's own* class hierarchy for the first time.
`gamebuino-solitaire` additionally avoided two still-unproven dialect
patterns out of caution rather than confirmed rejection (an array-typed
struct member, and a pointer-typed struct member) - flagged directly in
its own header comment as a defensive choice, not a wall actually hit.
`B-Rally`'s own real ADXL345 tilt-steering turned out to need no control
redesign at all once read fully: real upstream already ships a complete,
independent digital-button fallback control path for any cartridge
lacking the accelerometer chip, used verbatim here unmodified.

**Ten games shipped this pass**: Crabator (Rodot, unlicensed), Super
Space Shooter (msevilgenius, unlicensed), B-Rally (scmar, MIT), Artillery
(Frakasss, unlicensed), Solitaire (Andy O'Neill, MIT - corrected from an
earlier audit pass's mistaken "GPLv3"), FireBuino! (LADBSoft, LGPLv3 -
corrected from an earlier audit pass's mistaken "None specified"), 101
Starships (Zoglu, unlicensed - recovered via direct zoglu.net download,
no GitHub repo), Tetrino (j0ff, MIT), Maruino (ajsb113, unlicensed -
Dropbox download, no GitHub repo), and Super Crate Buino (Aurelien Rodot,
unlicensed) - see each one's own header comment in `src/games/` for its
own real porting notes, and the Games table in `README.md` for licenses/
sources. Every one is registered in `menuGameList.c`, verified crash-free
and genuinely playable via Puppeteer (each agent's own isolated build,
re-confirmed again post-integration against the real shared build), and
has a real gameplay thumbnail (the second thumbnail atlas grown from a
4x3/12-cell texture to a 4x6/24-cell one, 1024x768, still comfortably
under the real 1024x1024 GPU ceiling) and a real screenshot in
`metadata/screenshots/`. `MAX_GAMES` raised from 48 to 56 for the same
"modest headroom past the real current total" reason as every previous
raise. This clears the entire remainder of the porting-priority audit's
own Tier 3 list - see `more games/DISCOVERED_GAMES.md`'s own "Recommended
next pick" for what (Tier 4 and the genuinely-hard cases) is left.

## Twelve more games ported (batch 5) - a direct user download recovery, and every real "easy" find cleared

Prompted by a direct request to check the archived Gamebuino community
wiki for what titles were still missing, followed by the user directly
supplying real manual downloads (`E:\Downloads\gamebuino`) for 7 of the
~13 wiki entries every earlier automated search pass had given up on, plus
one more (`bub.zip`) that corrected an earlier real mistake (a previously
"recovered" clone had turned out to be empty - see `more games/
DISCOVERED_GAMES.md`'s own "user-driven recovery pass" section for the
full per-game verification). Combined with 4 more real candidates found by
directly auditing this project's own staging history for entries that
were cloned but never actually triaged into any tier (`Gamebuino-TREX-
QUEST`, `Gamebuino-Thunder-Shoot`, `ShootBuino`, `gamebuino-bangbang`),
this made for a real 12-game batch of exactly the kind of small, low-risk,
single-or-few-file candidates this project's own Tier 1/2 audit criteria
were built around - a genuine return to "easy" ports after batch 4
cleared the harder remainder of Tier 3.

Same parallel-isolated-copy workflow as batch 4 (each of the 12 agents
got its own full private repo copy plus its own one staged game, to build
and Puppeteer-verify independently with zero shared-file conflict risk;
the orchestrating session integrated each finished port into the real
tree afterward, one at a time). Diffed every one of the 12 agents' own
isolated `gamebuinoShim.h`/`.c` copies against a known-clean baseline
after the fact (not just trusted each report's own "no shim gaps" claim)
- confirmed genuinely clean across the board this time: zero shared-shim
edits anywhere in this batch, unlike batch 4's two real fixes. Every
local per-game workaround found this way (Senet's own local text-wrap
helper, several games' one-shot-tone sound approximations) matches an
already-established scope-limit precedent from an earlier batch, not a
new gap.

**One real shim documentation gap found and fixed, via a genuine self-
inflicted bug one agent hit and diagnosed**: `gbRenderFrame()` - the real
primitive every ported game's own `_update()` function must call exactly
once, as its last statement, to actually stream the framebuffer to the
GPU - was never declared in `gamebuinoShim.h`'s own primitive list at
all (only mentioned in passing inside a couple of unrelated comments).
The Senet port initially rendered as a permanently blank screen on every
launch, with every draw call still running correctly - a real, easy
mistake to make with a genuinely unhelpful symptom, found only by direct
bisection against an already-proven-working game's own file structure.
Fixed by adding a real declaration plus a doc comment spelling out the
exact consequence of forgetting the call, so a future porter skimming
the header (rather than an existing game's own source) won't miss it.

**A real, resolved false alarm, not a genuine candidate**: a fourth
wiki entry, ripper121's own `snake`, was initially treated as still
missing - directly diffing a user-supplied `snake_ripper.ino` against
the real upstream source this project's own already-shipped `Snake
Classic` (`Tnxec2/snake-gamebuino-classic`) was built from proved
Tnxec2's repo is a direct fork of ripper121's original (every sprite/
struct/function body identical; Tnxec2's fork even still shows
Ripper121's own unchanged `gb.titleScreen(F("Snake by Ripper121"),
logo)` string) - not a separate game to port again, just a missing
credit. `gameSnakeClassic.c`'s own header comment, `README.md`, and its
`menuGameList.c` credit line were all corrected to name Ripper121 as the
real original author alongside Tnxec2's own fork.

**Two real author/license corrections found by reading real source
directly, not trusting an earlier audit pass's own notes**:
`gamebuino-solitaire` is real MIT (not the "GPLv3" an earlier pass had
guessed), and `makerbuino-firebuino` is real LGPLv3 (not "None
specified") - both already corrected during batch 4, carried forward
here as the same "read the real file before trusting a summary"
discipline this batch applied to Senet's own real author name too
(`Maximilian Timmerkamp`, per every real source file's own copyright
header - not "DelphiMarkus", which turns out to be only the Bitbucket
account name the game was hosted under, not the author).

**A second thumbnail texture reaches its own real, confirmed 1024x1024
GPU ceiling with zero cells to spare**: grown from 4x6 (24 cells) to its
own maximum 4x8 (32 cells) to fit this batch's own real registered-game
count of 64 (indices 32-63 - exactly 32 needed, exactly 32 available). A
third thumbnail texture will be needed for the very next game ported
after this batch, not a future growth spurt - `MAX_GAMES` was also
raised from 56 to 72 for the same reason.

**Twelve games shipped this pass**: A to K (Carlos Mari, CC-BY 4.0 - a
real 2048 letter-matching variant, recovered via direct download),
Sokobuino (martinsustek, unlicensed - a real 600-level Sokoban clone),
DeathMaze (msevilgenius, unlicensed), AsteroidRipper (ripper121,
unlicensed - a second, unrelated real Asteroids clone alongside the
already-shipped `gameAsterocks.c`, given a deliberately distinct `astr`
prefix), Bang! Bang! (RackhamLeNoir, GPLv3 - the smallest real candidate
found across every discovery pass so far, 483 lines), Breakout Ripper
(ripper121, unlicensed), Lights Out AD (94k, WTFPL), Thunder Shoot
(Awot83, GPLv3), Bub (smogheap, GPLv3 - a real Bubble-Bobble-style
platformer), T-Rex Quest (Awot83, GPLv3), ShootBuino (frthery,
unlicensed), and Senet (Maximilian Timmerkamp, Apache 2.0 - a real
ancient-Egyptian board game, ported with its own real I2C two-cartridge
multiplayer mode deliberately dropped, keeping its real single-player-
vs-AI and local hot-seat modes intact) - see each one's own header
comment in `src/games/` for its own real porting notes, and the Games
table in `README.md` for full licenses/sources. Every one is registered
in `menuGameList.c`, verified crash-free and genuinely playable via
Puppeteer (each agent's own isolated build, then re-confirmed again
independently against the real shared build during integration), and has
a real gameplay thumbnail and screenshot. This exhausts every currently-
known "easy" candidate - see `more games/DISCOVERED_GAMES.md`'s own
updated tier tables for what's left (Tier 4's real engineering-cost
cases: `cruiser`, `BigBlackBox`, `CopterStrike`, `Bomber`/`StickFighter`).

## Bomber/StickFighter/Tron ported (batch 6) - the "needs an AI redesign" assumption was wrong, checked directly

Prompted by a direct question ("check if bomber stickfighter and trons got
a cpu player or not") after these three had sat in Tier 4 since batch 3's
own porting-priority audit under a "genuine two-cart IR-link multiplayer,
needs a single-player/AI redesign" label - a label that had never actually
been checked against each game's own real source, just inferred from the
presence of `master.ino`/`slave.ino`. Reading all three directly found the
assumption was simply wrong: every one of them already ships a real,
working single-player mode against a real upstream AI opponent (Tron's
own `ia()` - random-walk collision avoidance; Bomber's own
`monstre1`/`monstre2` - real bomb-placement/avoidance heuristics;
StickFighter's own `moveIAPlayer()` - a real, non-trivial fighting AI).
Followed immediately by a direct "port Bomber/StickFighter/Tron" request,
using the same parallel-isolated-copy workflow as batches 4/5 (three
agents this time, one per game, each with its own full private repo copy)
- every agent given the same explicit instruction: port the single-player
path completely and faithfully, drop the real multiplayer path entirely,
per this project's own established "drop the real hardware/mode-specific
option, keep the hardware-independent one" precedent (`gameBRally.c`'s
own accelerometer fallback, `gameSenet.c`'s own I2C-mode removal).

All three real menus already offered - or defaulted to - the single-
player mode by name ("1 player"/"Sigle player" [a real upstream typo,
normalized]/"solo"), with the two-cart IR-link mode as simply the *other*
option alongside it - so each port's own scope reduced to "remove the
Host/Join menu entries and every function only reachable from them,
port the real AI unmodified." All three collapsed their real `isMaster`/
`isOnePlayer`/network-branch dead code outright rather than leaving
permanently-unreachable branches behind, matching this project's own
long-standing "flatten a real single-instance/networked API into plain
single-player C" convention. Since only one real choice survives once
Host/Join are gone, Bomber and Tron both skip their own former mode-
picker screen entirely and go straight from the title screen into
single-player start (matching Pong Solo's own one-A-press precedent);
StickFighter keeps its own real 3-item bitmap-carousel main menu, just
shrunk to 2 items (solo/option) since that carousel's own real formula
needed no special-casing for a smaller item count.

**A genuine, previously-unproven single-player-vs-AI feature was
confirmed reachable and ported for StickFighter**: `stateGame==2`
("option" per upstream's own header comment) turned out to be a real
AI-vs-AI attract/credits screen, not an options menu - both fighters
AI-driven via `moveIAPlayer()` while a real "Design by Quirby64 / Programme
by Clement" credits block scrolls over the fight in progress. Confirmed
by reading the full branch directly (not trusting the porting task's own
summary) and ported since it has no multiplayer dependency at all.

**A real, first-of-its-kind shim limitation was hit and worked around
locally, not promoted**: Tron's entire mechanic depends on real
`display.persistence` - the drawn trail IS the collision surface, checked
every tick via `getPixel()`, and must never be cleared except by a fresh
game start. Every one of the 66 games shipped before this one redraws its
whole scene from scratch every tick and never needed persistence, so
`gbUpdate()`'s own unconditional per-tick `gbClear()` was never a problem
until now. Fixed inside `gameTron.c` alone (not the shared shim, since
this is the first and so far only game needing true persistence): a local
`tronTrail[]` bitplane, addressed exactly like the shim's own internal
`gbFrameBuffer[]`, that this file maintains itself and re-blits onto the
real framebuffer every tick before drawing that tick's own new rider
positions - functionally identical to real hardware's own persistent
framebuffer, just rebuilt every tick instead of genuinely never cleared.

**A real, previously-latent dialect bug was found and fixed while porting
Tron**: declaring two struct-typed globals on one comma-separated line
(`TronSnake tronP1, tronP2;`) silently corrupts unrelated global memory -
writes through the second variable's own fields landed in unrelated
global state (empirically traced to `gbFrameBuffer[]` itself). This is
the same family of bug `VIRCON32_C_DIALECT.md` already documents for
comma-separated pointer declarations (`int* a, b, c;`), just for aggregate
struct types instead - worth a documentation addition to that reference
file the next time it's touched, since no other game shipped in this
cartridge happened to declare two struct-typed globals on one comma line
before Tron (confirmed via a direct grep sweep), leaving this one latent
and unhit until now. Fixed trivially by splitting into two separate
single-variable declarations, with zero other changes needed.

Every other real shim primitive needed by any of the three (`gbDrawBitmap`,
`gbGetPixel`, `gbCollideRectRect`, `gbFillRect`/`gbDrawFastHLine`/`VLine`,
`gbPrintString`/`gbPrintNumber`, `gbRepeat`/`gbPressed`, `gbPlayNote`,
`gbFrameCount`, `arand`) already existed - no other shim gap was found.
Confirmed by diffing, not just trusting each agent's own report: every
shared file (`gamebuinoShim.h`/`.c`, `machineDependent.h`, `menu.c`/`.h`,
`eepromShim.h`/`.c`, `avrCompat.h`, `portVircon32.c`) is byte-identical
across all three isolated copies and against the pre-batch-6 real project -
`main.c` is the only file that differs between the three, and only by each
agent's own expected single `#include` line for its own game, needed to
build/test its own isolated copy at all. Both games' own "sound
approximated to one-shot tones" mentions (Bomber, StickFighter) are the
same already-established, already-documented scope limit reused verbatim
from `gameArmageddon.c`/`gameCopter.c`, not a new gap either.

**A third thumbnail texture was needed the moment Bomber (the 65th real
game) was integrated** - both existing thumbnail textures were already at
the real, confirmed 1024x1024 GPU ceiling with zero cells to spare (see
"Twenty more games ported (batch 3)" above for the first time this
ceiling was hit). `THUMBNAILS3_TEXTURE_ID` (id 5, appended after
`GRAYCOLUMNS_TEXTURE_ID` rather than inserted earlier, matching this
project's own "append, don't insert" texture-id precedent) is a new,
small 4x3/12-cell (1024x384) texture holding registration indices 64 and
up, with `md_drawGameThumbnail()`'s own two-way split (`THUMBNAIL_SPLIT`)
extended to a three-way split (`THUMBNAIL_SPLIT`/`THUMBNAIL_SPLIT2`) to
route to whichever of the three textures a given game index falls into.
`rom.xml`/`Make.bat`/`Make.sh` all updated to pack/convert the new
`thumbnails3.png` alongside the first two.

**Three games shipped this pass**: Bomber, StickFighter (art by Quirby64),
and Tron - all three by Clement83 (already this cartridge's most-shipped
single author, having also given it Copter/GlaciGlaca/CrazyTown) - see
each one's own header comment in `src/games/` for its own full real
porting notes, and the Games table in `README.md` for sources. Every one
is registered in `menuGameList.c`, verified crash-free and genuinely
playable via Puppeteer with the real AI opponent confirmed acting on its
own with zero player input (each agent's own isolated build, then
re-verified again independently against the real shared build during
integration, including a full menu-position/thumbnail-render check for
all three), and has a real gameplay thumbnail and screenshot. This clears
the last three entries anywhere in `more games/` that weren't already
either shipped or a genuine Tier 4 engineering-cost case.

## Five more games ported (seventh discovery pass) - including Wolfenduino
3D, a real raycaster, and the two dialect hazards it exposed

Prompted by a direct request to re-check the sibling
`gamebuino_classic_source_codes` archive for newly added games and port the
ones with real source. Diffing that project's own `games/` tree against this
project's own `more games/` found seven genuinely new, source-containing
candidates; `Gamebookuino` and `LunarRun` were excluded directly by the user,
leaving five: `RSG-Gamebuino`, `discovery`, `Gamebuino-Bomberman`,
`Bricks-Drakker` and `Wolfenduino`.

**Four of the five were ordinary ports.** R.S.G. (deKay) is a minimal
turn-based RPG; Discovery (FirstKlaas) a Star-Trek-themed side shooter;
Bomberman (JohnAkerman) a single-screen Bomberman genuinely unrelated to the
already-shipped `gameBomber.c` (hence the separate `bman` prefix beside that
file's own `bomb`); Bricks! (Drakker) a full-featured Breakout with four
levels, bomb/mine bricks, ten power-ups and both rockets and lasers. Each one
needed only primitives this shim already had. Three real out-of-bounds writes
were bounded because this platform cannot absorb them the way real AVR does:
Discovery's own `findNextMissile()` returns -1 through an unsigned `byte` and
its caller's `index < 0` guard can therefore never fire, and Bricks!' own
level-change block runs its grid copy one row and its item clear two entries
past the end of their arrays.

**Wolfenduino 3D is a different scale entirely** - 4,100 lines of real C++
across nine interdependent classes, a textured raycaster with a depth buffer,
sliding doors, secret push-walls, guard AI and a ten-level map set. Two
dialect hazards would each have silently destroyed it, and both were found by
reading rather than by debugging a broken build:

- **This dialect's `>>` is a logical shift** (VIRCON32_C_DIALECT.md section
  6), and Wolfenduino's whole fixed-point layer is `FIXED_TO_INT(x) ((x) >> 7)`
  and `WORLD_TO_CELL(x) ((x) >> 5)` applied to genuinely signed values - every
  view-space coordinate behind or left of the camera is negative. Ported
  naively, those shifts turn small negatives into huge positives and the
  projection collapses. A `WOLF_ASR()` function-like macro (confirmed
  supported by the current compiler, unlike what the older guide says)
  reproduces a real sign-extending shift branchlessly, and every shift that
  can see a negative goes through it.
- **Real AVR `int` is 16 bits and this dialect's is 32.** Upstream leans on
  that width for view-space coordinates, wall widths and interpolation errors.
  `WOLF_I16()`/`WOLF_I8()` reproduce the narrowing at each point upstream
  assigns to a narrow type.

**The SD-card payload is baked in**, per a direct instruction: real hardware
streams `WOLF3D.DAT` (163,840 bytes, ten 64x64 levels each stored row-major
and again column-major) through petit-FatFs. The whole file is embedded as
`src/games/gameWolfenduinoData.h`, packed four bytes per word and unpacked by
`wolfMapByte()`; the streaming window, the 16x16 buffer and the dual layout
are all unchanged, so the map is read in the same order and chunks as on real
hardware - only the byte source differs. Empirically confirmed first that the
compiler accepts an array that large.

**One place this platform genuinely cannot follow upstream**:
`shouldShootPlayer()` computes `16 / getPlayerCellDistance()`, and that
distance is 0 whenever a guard shares the player's cell - which happens
constantly. Real AVR returns garbage and carries on; Vircon32 hard-traps (the
same crash class already found in `cruiser` and `gameFifteen.c`), so the
divisor is clamped to 1.

**A real, significant upstream bug, FIXED on direct request**:
`Map::streamData()` applies its per-level file offset only in the desktop
`STANDARD_FILE_STREAMING` branch. The `PETIT_FATFS_FILE_STREAMING` branch the
real cartridge compiles never adds it, so every level streamed level 0's data
- nine of the ten shipped levels were unreachable on real hardware. Initially
preserved as genuine cartridge behaviour, then fixed when the user asked for
it: `wolfStreamData()` now adds that same offset, which is upstream's own
line restored to the branch it was missing from rather than an invention.
Verified by temporarily forcing a start on level 3 and confirming a visibly
different map (different wall textures, layout and decorations), plus a
direct check that all ten 16KB blocks in `wolf3d.dat` genuinely differ. One
port-specific addition came with it - the level counter now wraps to 0 at the
end of the set, since with the levels wired up, finishing the last one would
otherwise stream past the end of the payload into an empty map with no walls
and no player start. Starting a NEW GAME still does not reset the counter,
which is real upstream behaviour left alone.

Display persistence was deliberately not emulated anywhere in this batch (a
direct instruction), with one narrow local exception: Wolfenduino's death
effect sprinkles pixels over the frozen previous frame for 30 ticks without
clearing, so that one state keeps a local 504-word snapshot and re-blits it.
R.S.G. needed the same treatment differently - its own `attacking()` and
`baddieattacking()` are literally empty function bodies that rely on the
previous tick's screen still being there, so their drawing was moved into the
per-tick state functions instead.

All five verified via Puppeteer: menus, level loading, real gameplay, and for
Wolfenduino specifically the raycast view (textured walls, door frames,
floor/ceiling dither, weapon and decoration sprites), movement/turning/
collision, shooting with the ammo counter decrementing, guard AI dealing real
damage, and a full death-and-restart cycle - at a steady 60fps under headless
software rendering, with zero page errors. `THUMBNAIL_COUNT` raised 100->105
and `assets/thumbnails4.png` grown from 4x3 to 4x6 (24 cells, 1024x768).
`SOUND TEST`'s own screenshot and thumbnail were also recaptured this pass -
the originals had the emulator's own FPS/Fullscreen chrome baked in, which
the capture harness now hides. **One hundred and four games shipped**, plus
the SOUND TEST diagnostic tool.

**A workflow note worth keeping**: new `addGame()` calls must go at the very
BOTTOM of `menuGameList.c`. Registration index is what
`md_drawGameThumbnail()` keys off, so inserting anywhere earlier silently
shifts every later game's own thumbnail cell - caught here after R.S.G. was
first added above the SOUND TEST entry.


## Six more games ported (eighth discovery pass)

The sibling `gamebuino_classic_source_codes` archive picked up a commit
adding 20 more games, which a fresh diff turned into 23 real, unported,
source-containing candidates (all confirmed `<Gamebuino.h>` Classic, none
with an SD/accelerometer/two-cart dependency). Six were picked directly:
Guess A Number (l_et_m), Walbloks (borkin8r), Snake (Sigma-Squared), Copter
(by Anny - annyfm), Siena Town (Patryk Kuciński) and Super Crate Buino:
Stone Edition (4LAR). **110 games shipped**; `MAX_GAMES` 112 -> 120,
`THUMBNAIL_COUNT` 105 -> 111, filling `thumbnails4.png` to 23 of its 24
cells.

**A real shared-shim fidelity gap this batch closed**: Snake (Sigma-Squared)
prints its game-over screen at `fontSize = 3`, and `gbDrawCharPixel()` only
ever handled sizes 1 and 2 - a documented simplification no earlier port had
pushed past. Real `Display::drawChar()` has no such limit; it draws any size
with `fillRect(x + i*size, y + j*size, size, size)`. The shim now does
exactly that above size 1, keeping real hardware's own `drawPixel()` fast
path at size 1.

**Copter (by Anny) is genuinely distinct** from the already-shipped Clement83
Copter - different author, 270 vs 956 lines, its own corridor generator -
hence the separate `copa` prefix beside that file's own `copt`.

**Stone Edition was built as a delta, not a rewrite.** It is a fork of the
already-shipped Super Crate Buino, and diffing the two upstreams showed only
~300 changed lines. The shipped port was cloned under a `scbs`/`SCBS_`/`Scbs`
prefix and the fork's real changes applied on top: a four-entry mode-select
screen (only Classic live - the other three blink "LOCKED!" and their
handlers are commented out upstream, and the `chooseMapDef()` one of them
would call is unreachable dead code), every map unlocked from the first run,
a Button-B s-menu, and a weapon cheat that suppresses both the crate weapon
re-roll and the score increment. `gb.menu()` has no shim equivalent so both
of its lists are hand-rolled, and `displaySystemInfo()`'s battery/light/
backlight readouts have no sensors here and were dropped.

**Two real save-breaking bugs fixed**, both the established fresh-cell class:
Snake (Sigma-Squared) read a factory-erased EEPROM byte as 255 - the maximum
a `byte` score can reach - so `score > max_score` could never be true and a
fresh cartridge could never save its first high score. And Siena Town's own
`loop()` opens with `Topscore = 0;` on every tick, wiping the EEPROM value
before anything reads it, so the Top Score screen always showed 0 and every
run ended on "New record!". The first was fixed during the port; the second
was preserved at first and fixed on direct user report, then confirmed
working by the user.

All six verified via Puppeteer with real gameplay screenshots and thumbnails,
then backported to the sibling `gamebuino_classic_sdl` project (SDL3, SDL2
and Playdate), with `-ms` screenshot scripts, `.gbu` stubs and thumbnails
regenerated there.


## Toggle status badges, a transient change toast, and a real
md_drawSolidRect() color bug found by building them

Prompted directly: show the state of the three global toggles somewhere in
the menu, and flash a short indication whenever one is flipped - including
mid-game, which means the main loop has to draw it after the game's own
frame rather than any game doing it itself.

**Menu badges** (`md_drawToggleStatusIcons()`, called from `menu.c`'s own
draw): `SND` / `GRAY` / `GRID` in the top-right corner, white when the
toggle is on and `color_darkgray` when it is off, so the row reads as
lit/unlit at a glance. The first attempt drew an enabled badge as black
text on a filled white box; that was replaced with plain color on direct
feedback, which is both clearer and what exposed the bug below.

**The toast** (`showStatusToast()`/`drawStatusToast()` in `portVircon32.c`)
is a white-outlined black box at the bottom centre, up for
`STATUS_TOAST_FRAMES` (60 = one real second at the loop's own 60fps),
drawn as the very last thing in the frame - after the game's finished
frame, after the pixel-grid overlay and after the quit dialog - so it
genuinely lands on top of whatever is already there. Each of the three
toggles sets it.

**The pixel-grid toggle is no longer gated to gameplay.** It used to only
be read while a game was running, on the reasoning that toggling it in the
menu would be an invisible no-op. With the badges showing its state that is
no longer true, so Button L is now accepted everywhere like the sound and
gray toggles; the overlay itself still only DRAWS over a running game, and
the `onResume()` call is guarded on a game actually being loaded.

**A real, pre-existing rendering bug this surfaced**: `md_drawSolidRect()`
selected column-atlas region **255** as its base tile and modulated it with
`set_multiply_color()`. A region's pixels are multiplied by that color, so
the base has to be white for the requested color to survive - and region
255 is the all-"lit" tile, which on this reflective-LCD atlas has been
solid BLACK ever since the color-inversion fix (see "Ten more games ported
... a project-wide color-inversion bug" above). Black multiplied by
anything stays black, so `md_drawSolidRect()` silently painted black
whatever color it was asked for. The quit dialog had been drawing its own
"white outline, black interior" as a plain black box with no outline at all
for that entire time, and nobody noticed because black-on-white-LCD with
white text is still perfectly legible. Fixed by selecting region **0**
instead (the all-"off" tile, solid white on this atlas), which makes the
multiply color mean what it says. Confirmed directly by sampling
`assets/columns.png` - region 0's centre pixel is (255,255,255), region
255's is (0,0,0) - and then visually: the quit dialog now has its real
white border.

Verified via Puppeteer: badges in all states, all three toggles flipped
from the menu, the toast drawn over live gameplay for both sound and gray,
the grid toggled from the menu and confirmed still on once a game launched,
and the quit dialog's restored outline.

**Backported to the SDL/Playdate sibling, but NOT as a one-to-one
translation.** An initial reading of that project was wrong and was
corrected directly by the user: it does not have "only the gray toggle" -
it has MORE runtime state than this build does, just spelled differently,
so a grep for `mute`/`grid` in `gamesMain.c` finds nothing. What it
actually has (`src/sdl3/gamebuinoSDL3.h`, `sdlBackend.c`):

- mute (`gMuted`, North face button / `S`) - a real live toggle, not the
  `-ns` startup flag,
- real-solid-gray (`gbRealGrayColor`, Button R / `V`), the one toggle both
  builds share,
- a combined **effect cycle** (`gEffectState`, `G` plus L/W/Z): a 3-bit
  counter stepping through all 8 combinations of pixel-grid, **glow** and
  **CRT**. Pixel-grid has no button of its own there at all - it is one bit
  of this cycle,
- analog volume up/down on the L2/R2 triggers (`gVolume`, separate from
  mute).

So the row there is five badges, not three (`SND`/`GRAY`/`GRID`/`GLOW`/
`CRT`, same white-vs-dark-gray convention, drawn via a new
`MD_COLOR_DARKGRAY`), and the effect cycle's own toast has to report the
resulting COMBINATION rather than a single named toggle - `showEffectToast()`
builds `"EFFECTS: GRID+CRT"` / `"EFFECTS: NONE"` from whichever bits are
actually set, so what it says always matches what is on screen. Both SDL
backends carry an identical copy. Their own `md_drawSolidRect()` fills real
pixels instead of modulating an atlas tile, so neither ever had the color
bug fixed above.

Two more real differences worth knowing about that project:

- **All three of its toggles were gated to gameplay**, not just the grid -
  fixed there the same way and for the same reason as here.
- **Playdate needed its own design, not a copy.** It does not use
  `gameworld/menu.c`'s own `menu_update()` at all (it has a from-scratch
  menu), so `md_drawToggleStatusIcons()` is a deliberately empty stub there
  and a local `menuDrawStatusIcon()` draws the real badge instead. Only ONE
  of the five toggles exists on that port (the pixel grid, flipped from the
  system menu's own checkmark item), and 400x240 has nowhere near the width
  for the SDL word labels, so it is a single letter `G`, boxed when on and
  outlined when off - a 1-bit panel has no dim gray to fall back on. The
  toast matters more there than anywhere else: the checkmark is flipped
  inside the system menu, which then closes over the game, so without it
  there is no confirmation the press did anything at all.


## Solid gray on by default, and every screenshot/thumbnail rebuilt from the
SDL port's own captures

Prompted directly. `gbRealGrayColor` still initializes to `false` in
`gamebuinoShim.c` (real hardware's own dither behavior, so the shim keeps
saying what real hardware does), and `main()` now sets it `true` at startup
instead - the same shape the sibling SDL build already used
(`gamesMain_init()`), rather than changing the shim's own documented
default. Button R still flips it live, both ways. The reason is the
display, not the emulation: real hardware's dither relies on a persistent
LCD, and on a stable modern screen it reads as a flicker rather than as
gray.

Every one of the 111 screenshots and all four thumbnail atlases were then
rebuilt to match, **without recapturing anything**, by reusing the sibling
`gamebuino_classic_sdl` project's own already-captured
`metadata/screenshots/<TITLE>.bmp` set - that port defaults to solid gray,
so its captures already show the wanted rendering. Three things made the
reuse exact rather than approximate, each checked rather than assumed:

- **The two projects' registration orders are identical** - all 111 titles
  compared position-by-position, zero mismatches. Registration index is
  what `md_drawGameThumbnail()` keys off, so this had to hold before any
  atlas cell could be trusted.
- **The capture geometry is identical** - both are 640x360 with the LCD at
  `(26,12)` sized `588x336` (`ORIGIN_X`/`ORIGIN_Y`, `TILE_SCALE` 7). Only
  the surround differs: this project's own web-emulator captures carry a
  dark 1px rounded frame the SDL window has no equivalent of. So only the
  LCD rectangle was transplanted, pasted into each existing screenshot at
  `(26,12)`, leaving this project's own frame intact.
- **The two ports draw a different gray** - SDL fills `(128,128,128)`
  (`gGrayPixel`), this cartridge blits `assets/columns_gray.png`, whose lit
  bits are `(150,150,150)` (`tools/gen_column_atlas_gray.py`'s own `GRAY`).
  Every transplanted crop therefore remaps `128 -> 150`; 11 of the 111
  actually contained solid gray. Confirmed against the real thing rather
  than trusted: a live Puppeteer capture of B-Rally on the rebuilt ROM
  contains exactly three colors - white, black and `(150,150,150)`.

Thumbnail cells are `crop.resize((256,128), LANCZOS)`. Note this is a
deliberately non-uniform squeeze (588x336 is 1.75, the cell is 2.0) and a
*smoothing* filter, both matching what the existing cells already were -
confirmed by their own intermediate-value color counts, and NOT the same
convention as the SDL port's own `thumb_NN.bmp` (224x128, NEAREST, aspect
preserved, because `SDL_BlitSurface` does not scale). The two are separate
pipelines; do not carry one's numbers over to the other.

Verified via Puppeteer against the rebuilt ROM: the menu's own `GRAY` badge
lit at boot with no key pressed, B-Rally's own rebuilt thumbnail showing
its flat gray road, and its live gameplay matching.

## A third GB_GRAY mode: real LCD persistence, modelled on Simbuino

Prompted directly, with `github\simbuino` named as the reference. Reading
its own `LcdDevice.cs` settled exactly what that emulator does, rather than
guessing from how it looks: `SetPixel()` accumulates each pixel's own
on-time weighted by AVR clock cycles, `Refresh()` divides that by the
elapsed cycles to get a duty value, averages it against the PREVIOUS
refresh's own value, and quantizes the result to three levels (`< 64` off,
`> 190` on, anything between a mid shade). That is a two-sample moving
average of how long each pixel was actually lit - a model of the panel not
settling, not a filter over the image.

Here one rendered tick is exactly one sample (`gbRenderFrame()` runs once
per real logic tick, and the dither alternates on `gbFrameCount`), so the
average of two consecutive ticks has only three possible values and the
whole model collapses to two bitwise ops on the byte columns this
cartridge already blits:

- lit in BOTH ticks -> black (`cur & prev`)
- lit in exactly ONE -> gray (`cur ^ prev`)
- lit in NEITHER -> white (the cleared background)

So `gbRenderFramePersistent()` reuses `gbRenderFrame()`'s own two existing
passes and both atlases unchanged, just fed different byte values - no
per-pixel work, no third texture, and the same call-site gating. The only
new state is `gbPrevFrameBuffer[]` (one 504-int copy per tick, only while
the mode is on) plus `gbLcdPersistencePrimed`, which stops a stale previous
frame - the menu's, or another game's - from being smeared into the first
tick after the mode is switched on or a game is launched (`gbBegin()`
clears it, alongside `gbFrameCount`/`gbPopupTimeLeft`).

**This is genuinely broader than `gbRealGrayColor`, not a second way to
draw the same thing.** That flag recolors pixels a game explicitly drew as
`GB_GRAY`; this one smears whatever actually alternates - `GB_GRAY`'s own
dither included, but equally a game's own blinking sprite or invert flash,
exactly as the real panel does. The two are therefore mutually exclusive:
LCD mode needs `GB_GRAY` to dither normally to have anything to average, so
`gbRealGrayColor` must be off for it.

Button R now cycles all three (GRAY1 solid -> GRAY2 LCD -> GRAY0 off ->
...) instead of toggling one flag, and the menu badge names the active mode
rather than only lighting up - `GRAY0` (dim) / `GRAY1` / `GRAY2`, always
five characters so the right-aligned badge row does not shift as it cycles.

Verified via Puppeteer on B-Rally, whose road is a large `GB_GRAY` band:
GRAY0 shows the raw checkerboard, GRAY1 a flat `(150,150,150)` road, GRAY2
the same flat road plus a one-tick gray trailing edge on the moving car -
the smear that is the whole point. Color histograms confirm it: two colors
in GRAY0, three in both GRAY1 and GRAY2. The user separately confirmed it
live on Bricks!.

Backported to the sibling `gamebuino_classic_sdl` project in the same
session - the shim change is identical there (only the array-declaration
dialect differs), and that project needed the extra plumbing its own
structure implies: `md_setRealGrayEnabled( bool )` became
`md_setGrayMode( int )` across all three of its backends, its `-gray` CLI
flag now takes `0|1|2`, and `gamesMain_setRealGrayColor( bool )` became
`gamesMain_setGrayMode( int )`. See that project's own CLAUDE.md for the
full writeup.

## Status

- `src/gamebuinoShim.h`/`.c` - the compatibility shim, proven against 64
  real, independent games now.
- `src/games/gamePong.c` - Pong Solo (Aurelien Rodot, LGPLv3), the
  official Gamebuino Classic library's own bundled `2.Intermediate/Pong`
  example. Ported and verified via Puppeteer: menu registration (with a
  real gameplay thumbnail), the title screen (holds correctly, dismissed
  by a genuine A-press - see the bug writeup above), active gameplay
  (paddle movement, ball physics/collision, opponent AI tracking, score
  display updating correctly on a scoring event), the quit-confirmation
  dialog (opens/cancels/confirms correctly), the pixel-grid overlay
  (toggles on and cleanly back off), and the mute toggle. A real, complete,
  working port - not just a static-screen smoke test. Does not itself use
  the new EEPROM shim (no high score to save), but its launch now
  exercises `eepromSelectGame()`'s own no-card-connected fallback path
  every time, confirmed via Puppeteer to cause no regression. Its own
  ball/paddle/AI speed changed for real once the 20fps default bug above
  was fixed - it had been running 1.5x too fast versus real hardware the
  entire time this shim defaulted to 30fps.
- `src/games/gameAgaruino.c` - Agaruino (ogbaba, GPLv3), a real agar.io
  clone - the second game ported, picked as the smallest/cleanest real
  candidate found by the porting-priority audit (see
  `more games/DISCOVERED_GAMES.md`). Ported and verified via Puppeteer:
  menu registration (with a real gameplay thumbnail), its own internal
  menu screen (holds correctly, dismissed by a genuine A-press), active
  gameplay (movement, eating/growing, the "TAILLE N" size readout
  updating), returning to its own menu via Button C, and returning to the
  cartridge's own top-level menu via the quit-confirmation dialog. Two
  real upstream quirks were found while reading the source and handled
  differently on purpose (see `gameAgaruino.c`'s own header comment for
  the full reasoning): a one-character `joueurs`/`joueur` typo in
  upstream's own velocity-clamping code was normalized away (silently
  reproducing a typo would just look like a fresh mistake in this port),
  while a backwards on-screen-culling test that makes the player's own
  blob permanently invisible (confirmed via Puppeteer - the player's own
  ball genuinely never renders, matching real hardware) was preserved
  exactly as upstream wrote it, since that one is real, load-bearing
  original gameplay behavior, not an internal-only slip.
- `src/games/gameShufflepuck.c`/`gameTaquin.c`/`gameCrazyCar.c`/
  `gameMaze.c`/`gameSimonbuino.c`/`gameConduit.c`/`gameFlappyBirdo.c`/
  `gameParachute.c`/`gameSnakeClassic.c`/`gameSnakeAbc.c` - the ten games
  ported since Pong/Agaruino - see "Ten more games ported..." and "Real
  Gamebuino fonts ported..." above for the full history (real bitmap art/
  fonts restored, the color-inversion bug, and every other bug found along
  the way), and each file's own header comment for its own per-game real
  porting notes.
- `src/games/gameFiremen.c`/`gameCatcher.c`/`gameUfoRace.c`/
  `gameMinesweeper.c`/`gameKillrace.c`/`gameBlockdude.c`/`gameLander.c`/
  `gamePunkt.c`/`gameInvaders.c`/`gameAsterocks.c` - the ten batch-2 games -
  see "Ten more games ported (batch 2)..." above for the full history (the
  `gbDrawChar()`/`gbRepeat()` shim bugs found along the way), and each
  file's own header comment for its own per-game real porting notes.
- `src/games/gameGruniozerca.c`/`gameVideoPoker.c`/`gameBlobAttack.c`/
  `gameSmash.c`/`game2048.c`/`gameArmageddon.c`/`gameSkibuino.c`/
  `gameHexagon.c`/`gameJezzball.c`/`gameShipwrek.c`/`gamePaqman.c`/
  `gameBlocksBuino.c`/`gameWhg.c`/`gameCrazyTown.c`/`gameCopter.c`/
  `gameZombiEscape.c`/`gameGlaciGlaca.c`/`gameDescent.c`/
  `gameCastleDefence.c`/`gameDigger.c` - the twenty batch-3 games - see
  "Twenty more games ported (batch 3)..." above for the full history (the
  five shim primitives promoted along the way, the `MAX_GAMES` bug, and the
  real GPU texture size ceiling), and each file's own header comment for
  its own per-game real porting notes.
- `src/games/gameSuperSpaceShooter.c`/`gameTetrino.c`/`gameArtillery.c`/
  `gameCrabator.c`/`gameBRally.c`/`gameMaruino.c`/`gameSuperCrateBuino.c`/
  `gameFirebuino.c`/`gameStarships101.c`/`gameSolitaire.c` - the ten
  batch-4 games - see "Ten more games ported (batch 4)..." above for the
  full history (the parallel-isolated-copy workflow this and every later
  batch reuses, the real `gbDrawCharPixel()` fontSize=2 positioning bug
  found and fixed, and `gbTimeHeld()` promoted), and each file's own
  header comment for its own per-game real porting notes.
- `src/games/gameA2K.c`/`gameSokobuino.c`/`gameDeathMaze.c`/
  `gameAsteroidRipper.c`/`gameBangBang.c`/`gameBreakoutRipper.c`/
  `gameLightsOutAD.c`/`gameThunderShoot.c`/`gameBub.c`/`gameTrexQuest.c`/
  `gameShootBuino.c`/`gameSenet.c` - the twelve batch-5 games - see
  "Twelve more games ported (batch 5)..." above for the full history (the
  user-driven recovery pass, the `gbRenderFrame()` documentation gap found
  and fixed, and the second thumbnail texture reaching its own real
  1024x1024 ceiling), and each file's own header comment for its own
  per-game real porting notes.
- `src/games/gameBomber.c`/`gameStickFighter.c`/`gameTron.c` - the three
  batch-6 games - see "Bomber/StickFighter/Tron ported (batch 6)..."
  above for the full history (the "needs an AI redesign" assumption
  corrected, the real Tron persistence workaround, the real
  comma-separated-struct-globals dialect bug found and fixed, and the
  third thumbnail texture), and each file's own header comment for its
  own per-game real porting notes.
- `metadata/menu.png`/`metadata/screenshots/*.png` - real captured
  screenshots (the same Puppeteer sessions used to build each game's own
  thumbnail-atlas cell), one per game, referenced from `README.md`'s own
  Games table, matching the sibling project's own metadata layout.
- `src/games/gameSpinSpinSpinbuino.c`/`gameSnake5110.c` - two more real
  Tier 2 games, found by a "status report on missing games" audit rather
  than a discovery pass - see "Spin Spin Spinbuino and Snake 5110
  ported..." above for the full history (the real `gbBegin()`-per-game and
  no-ternary-operator lessons found along the way), and each file's own
  header comment for its own per-game real porting notes.
- `src/games/gamePongLocalMultiplayer.c`/`gameSavePrincesse.c`/
  `gameMotoCross.c`/`gameNoNamePlatformGame.c`/`gameStijnPong.c`/
  `gameStijnSnake.c`/`gameMasterKebab.c`/`gameAimbuino.c`/`gameRalph.c`/
  `gameFootlol.c` - the nine games that clear the fifth discovery pass's
  own Tier 1 - see "Nine more games ported..." above for the full history
  (the manual-then-agent workflow switch, per-game upstream quirks found,
  the real Aimbuino button-debounce bug found and fixed, and the
  highscore/narrow-int/shift follow-up sweep that found nothing else
  needing a fix), and each file's own header comment for its own per-game
  real porting notes.
- `src/games/gameMyRpg.c`/`gamePong2017.c`/`gameDarkShmup.c`/
  `gamePinball.c`/`gameRobot.c` - the five games that clear the fifth
  discovery pass's own remaining Tier 2 candidates - see "Five more games
  ported (all of Tier 2)..." above for the full history (the real
  divide-by-zero crash guard added to PinBall, the real "Frakasss/
  DarkShmup" fabrication correctly ruled out before porting the genuine
  Clement83 repo, and the screenshot-format sweep that followed), and each
  file's own header comment for its own per-game real porting notes.
- `src/games/gamePetitMonstre.c`/`gameElventure.c`/`gameUnderTheTower.c`/
  `gameStarHonor.c` - the four games that clear the fifth discovery
  pass's own remaining Tier 3 candidates - see "Four more games ported
  (all remaining Tier 3)..." above for the full history (the real
  PetitMonstre soft-lock found and fixed, UnderTheTower's own several
  real OOB/infinite-loop fixes and narrow-int EEPROM audit, and both real
  `Pirates` lessons checked-and-confirmed-absent across all four), and
  each file's own header comment for its own per-game real porting notes.
- `src/games/gameCruiser.c`/`gameBigBlackBox.c` - the two games that
  clear 2 of Tier 4's original 4 engineering-cost cases - see "cruiser
  and BigBlackBox ported..." above for the full history (the "own copy of
  the Gamebuino library" concern checked directly and found unfounded for
  both, the real division-by-zero crash found via a live user report and
  fixed in `cruiser`, and the new dialect facts the port surfaced), and
  each file's own header comment for its own per-game real porting notes.
- `src/games/gameCopterStrike.c` - the game that clears the porting-
  priority audit's own last real candidate - see "Duel ruled out,
  CopterStrike's real duplicate-folder structure analyzed and ported..."
  below for the full history (why its 6 near-identical engine-file copies
  exist, and the consolidated 4-mission single-cartridge design that came
  out of that analysis), and the file's own header comment for the full
  per-mission porting notes.
- `src/games/gameXonix.c`/`gameAnother2048.c`/`gameDarkTower.c`/
  `gameGemgem.c`/`gameCommunityRpg.c` - the five games that clear the
  sixth discovery pass (sourced from the sibling
  `gamebuino_classic_source_codes` archive project) - see "Five more games
  ported (sixth discovery pass)..." below for the full history (the
  real DATA.DAT/SOUND.DAT investigation in `gamebuino-community-rpg`,
  DarkTower's own hand-rolled-polymorphism flattening, and the three
  candidates excluded directly by the user), and each file's own header
  comment for its own per-game real porting notes.
- `src/games/gameFifteen.c` - a real fourth game found under an author
  already mined twice, uncovered by fixing a real WebFetch-truncation
  tooling gap - see "Fifteen ported..." below for the full history, and
  the file's own header comment for its own per-game real porting notes.
- `src/games/gameMoleControl.c`/`gameAerialAssault.c` - two more real
  games found in the sibling archive project, the last two currently
  known anywhere - see "Mole Control and Aerial-Assault ported..." below
  for the full history (the real AVR-narrowing restart bug preserved in
  Mole Control, the real Controls-menu bug fixed in Aerial-Assault on
  direct request, and the Puppeteer persistent-session verification
  technique this pair's own integration mistakes motivated), and each
  file's own header comment for its own per-game real porting notes.
- **Ninety-nine games shipped so far** - see the Games table in
  `README.md`. Every currently-known normal-effort candidate is now
  shipped, including Bomber/StickFighter/Tron (see "A closer look at
  Bomber/StickFighter/Tron..." below for the discovery that corrected
  their earlier "needs an AI redesign" framing, and "Bomber/StickFighter/
  Tron ported (batch 6)" above for the actual port), the two Tier 2
  stragglers above, and all of the fifth discovery pass's own Tier 1/2/3
  entries. `Pirates` (also that pass's own Tier 3) was ported and then
  reverted - see "Pirates ported, then reverted" above for the full
  investigation (a real, pre-existing combat-breaking collision-direction
  bug in the original game that repeated fixes could not resolve to the
  user's satisfaction) - it is no longer a candidate at all.
  `cruiser`/`BigBlackBox`/`CopterStrike` (Tier 4) are also now ported -
  once actually checked, none of the three needed any new shim work at
  all, correcting the original audit's "from-scratch API reimplementation"/
  "needs real consolidation work" framing for all three. `Duel` was
  checked directly and ruled out permanently (a genuine two-cart-only
  fighting game with no AI to port at all - see `more games/
  DISCOVERED_GAMES.md`'s own "Excluded from porting entirely" section).
  A sixth discovery pass, run from the sibling
  `gamebuino_classic_source_codes` archive project, then found 8 more
  real games beyond this project's own five earlier passes - 5 shipped
  (see above), 3 excluded directly by the user
  (`PAK-MAN_MAKERbuino`/`Frogger_MAKERbuino`/`minesw-gameguino`). No
  further "normal-effort" candidates are currently known.

## A closer look at Bomber/StickFighter/Tron: none of them actually need an AI redesign

Prompted by a direct question ("check if bomber stickfighter and trons got
a cpu player or not") after these three had sat in "genuine two-cart
IR-link multiplayer, needs a single-player/AI redesign" limbo since batch
3's own porting-priority audit - that framing had never actually been
checked against each game's own real source, just inferred from the
presence of `master.ino`/`slave.ino` files. Reading all three directly
settled it differently: **every one of them already ships a real,
working single-player mode against a real upstream AI opponent**, no
redesign needed at all -

- **Tron**: its own real `gb.menu()` offers "1 player" as option 0,
  routing to `ia(&p2)` (`player.ino`) - a real, if simple, random-walk
  collision-avoidance AI (checks the pixel ahead via `getPixel()`, picks a
  new direction when it would crash into a trail, with a small random
  chance to change direction unprompted too).
- **Bomber**: its own real title menu offers "Sigle player" [sic, a real
  upstream typo] as option 0 (`isSingle = true`), pitting the human
  player against two real AI-controlled `monstre1`/`monstre2` opponents
  with genuine bomb-placement/avoidance heuristics
  (`MonsterCanDropBombe()`/`chercherCheminPossible()`/`evaluateCase()`
  scores each adjacent tile before deciding to move or drop a bomb - not
  a stub).
- **StickFighter**: its own real main menu defaults to a "solo" mode
  (`focusItem = 0`, `isOnePlayer = true`) - `moveIAPlayer()` is a genuine,
  non-trivial fighting-game AI (attack/kick/block/retreat decisions
  weighted by distance, life difference, and a real `diffculty` constant
  that scales aggression), used both for the real solo opponent and, in a
  separate demo/attract-mode game state, to drive *both* fighters at
  once for a title-screen showcase.

All three moved from Tier 4 into Tier 3 in `more games/DISCOVERED_GAMES.md`
accordingly (similar size to what's already there, and - like `B-Rally`'s
own accelerometer fallback precedent - each one's own real two-cartridge
IR-link mode is simply the *other* menu option alongside a genuinely
complete, already-working single-player one, not the only way to play).
No code was ported this pass - this was a status/triage correction only,
prompted directly rather than found incidentally.

## `more games/` - staged source for future ports

A large, actively-growing staging directory, built across several
discovery passes - see `more games/DISCOVERED_GAMES.md` for the full,
detailed writeup of every pass (exact search queries, per-repo
verification steps, what was checked and ruled out and why); this section
is just a summary of where things stand, kept up to date here so a fresh
session doesn't need to re-read that whole history before deciding what
to do next.

**~97 real, source-containing directories staged in total**, from five
distinct discovery passes:

1. **The archived Gamebuino community wiki** (`web.archive.org`'s capture
   of `legacy.gamebuino.com/wiki/index.php?title=Games`, now dead on the
   live web) - the original, most complete index, 75 entries. 47 had a
   real GitHub/Bitbucket repo and were cloned directly; 2 repo links
   turned out to be genuinely unreachable (`senet` - Bitbucket now
   requires auth; `lights Out AD` - a dead `git://` daemon); the
   remaining ~27 had no repo at all, only a direct download link (forum/
   Mediafire/Dropbox/Facebook/personal-site).
2. **Direct zip/file recovery** for the "no repo" entries above - fetched
   the actual download (live, or via a second author-GitHub-account
   search when the first link was dead) rather than giving up at the
   first dead link. Recovered 10 more this way (SpinSpinSpinbuino,
   101Starships, 5 "yodasvideoarcade" games, Maruino, Gamebuino2048) plus
   2 more via a second search pass that found the same game hosted
   somewhere other than the wiki's own dead link (BigBlackBox,
   Gamebuino-TREX-QUEST).
3. **A third pass, broad general web search** (not GitHub-scoped) for the
   still-unrecovered titles, re-checking each one via a different route
   (author's live current gamebuino.com profile, full GitHub repo
   listings, alternate forum mirrors) - found nothing newly downloadable,
   but firmed up *why* several are dead (e.g. one author explicitly
   disabled downloads on the current site; one forum thread was never
   archived at all, confirmed via the Wayback CDX index).
4. **A fourth pass, direct GitHub API search** (`search/repositories` for
   `gamebuino classic` + the `gamebuino` topic, outside the wiki's own
   list entirely) - found **8 more real, confirmed-Classic games**
   (`firemen`, `Agaruino`, `Parachute_Gamebuino`, `skibuino`,
   `videopoker-gamebuino`, `snake-gamebuino-classic`,
   `Gamebuino-Classic-Snake-5110`, `cruiser`) - every candidate was
   verified by actually reading its real `#include` line
   (`<Gamebuino.h>` = Classic, `<Gamebuino-Meta.h>` = META) before
   staging, since Classic and META share the same "gamebuino" name/topic
   and several promising-looking hits (by description or game name alone)
   turned out to be META and were correctly excluded.
5. **A fifth pass, two parallel background agents** (one GitHub-scoped,
   one broad-web-scoped, requested directly: "dispatch agents to do a
   deepseek to find more gamebuino classic games") - found **19 more
   real, confirmed-Classic games** (3 more from `wuuff`, 8 more from
   `Clement83`, 3 more from `ogbaba` - 2 credited to `CRAZYCAR-Gamebuino`'s
   own real author Baptiste Pouget despite being hosted under a different
   account, 3 more from `Frakasss`, plus one new author each for `qubist`/
   `yawn-g`/a genuinely new author `StijnCaerts`) and one confirmed-not-
   portable two-cart-only find (`Duel`, Clement83 - genuine multiplayer
   with no AI, unlike Bomber/StickFighter/Tron). **A real, confirmed
   fabrication was caught and corrected this pass** - the GitHub-scoped
   agent's own final report included one entirely invented repo
   (`Frakasss/DarkShmup`, complete with a specific line count and
   description) that a direct check (`git ls-remote`, the real author's
   own live repo listing) proved never existed at all; every other claim
   from both agents' reports was individually re-verified against the
   real repo/LICENSE file directly before being trusted, not taken from
   either report's own summary - see `more games/DISCOVERED_GAMES.md`'s
   own "A fifth discovery pass..." section for the complete writeup.

**~13 titles remain confirmed unrecoverable** after all five passes (each
with a specific, documented reason - dead host, auth wall, never
archived, nonexistent author account, etc.) - see
`more games/DISCOVERED_GAMES.md`'s own "Confirmed still unrecoverable"
sections for the full list; not worth re-attempting without a genuinely
new lead.

**A full porting-priority audit of every staged directory has now been
done** - see `more games/DISCOVERED_GAMES.md`'s own "Porting priority
audit" section for the complete, tiered writeup (every directory's real
source confirmed to exist - several looked source-less at a shallow
top-level listing but have a real `.ino` nested in a `src/`-style
subfolder, e.g. `Crabator`/`UFO-Race`/`Digger`/`armageddon` - plus a line
count, a real-`<Gamebuino.h>`-vs-reimplementation check, and a scan for
accelerometer/multiplayer-IR-link red flags, for all ~60 real candidates).
This is a first-pass line-count/grep audit, not a full read-for-
playability of every game - that step still happens per-game at actual
porting time, same as Pong's own history. Summary:

- **Tier 1 is now fully shipped** - every entry (`Agaruino`,
  `Gamebuino-Shufflepuck_cafe`, `gamebuino-taquin`, `CRAZYCAR-Gamebuino`,
  `firemen`, `gamebuino-maze`, `Simonbuino`, `conduit`, `FlappyBirdo`,
  `Parachute_Gamebuino`, `snake-gamebuino-classic`, `SnakeAbcBuino`) is
  ported - see the Games table in `README.md`.
- **Tier 2 is now fully shipped**: `Gamebuino-Catcher`, `UFO-Race`,
  `Gamebuino-Minesweeper`, `blockdude-gamebuino`, `gamebuino-punkt`, all 5
  `yoda-*` arcade clones (`yoda-killrace`, `yoda-lander`, `yoda-invaders`,
  `yoda-asterocks`, `yoda-paqman`), `Smash-and-Crash`, `CrazyTown`,
  `Blob-Attack`, `gamebuino-jezzball`, `Digger`, `microhexagon`,
  `armageddon`, `Gamebuino2048`, `ZombiEscape`, `GlaciGlaca`, `Copter`,
  `gruniozerca-gamebuino`, `skibuino`, `videopoker-gamebuino`, and
  `Worlds-Hardest-Game-Gamebuino` (`Tron` is excluded - genuine two-cart
  IR-link multiplayer, same as `Bomber`/`StickFighter` below) - see "Ten
  more games ported (batch 2)..."/"Twenty more games ported (batch 3)..."
  above. Tier 3 has also started: `shipwrek` (confirmed to have zero real
  EEPROM calls after all, despite shipping a `.eep` stub - see that
  section above), `CastleDefence`, and `DescentIntoHeel` are shipped too;
  the rest of Tier 3 remains a natural pick for a future session - full
  list and per-game notes in `DISCOVERED_GAMES.md`.
- **Real engineering cost before porting can start**: `cruiser` (its own
  from-scratch `port/Gamebuino.h` reimplementation, not the real
  library - the most structurally interesting find, a genuine 3D
  shooter, but the highest cost for that reason), `BigBlackBox` (ships an
  even bigger complete from-scratch API reimplementation), `CopterStrike`
  (10k+ lines, but heavily duplicated across near-identical mission
  subfolders - needs consolidation before its real size is even known),
  and `Bomber`/`StickFighter`/`Tron` (genuine two-cart IR-link
  multiplayer - meaningless as-is on one emulated cartridge without an
  AI-opponent redesign).
- **`B-Rally`** has real ADXL345 accelerometer tilt controls (`Wire.h`
  confirmed) - portable, but needs a control-scheme redesign (tilt →
  D-pad) first.
- **Stale note, corrected**: the "Tier 1/2 fully shipped" claims above
  predate the fifth discovery pass - see that section above and
  `DISCOVERED_GAMES.md`'s own tier tables for the current, accurate
  picture (19 more real Tier 1/2/3 candidates now staged and unported,
  plus one new Tier-4-style case, `Duel`).
- **Confirmed not real games / not portable**: `Gamebuino-Classic-Games-
  Compilation`/`FeroBoh/Gamebuino-Classic_Games` (compiled-binary-only SD
  compilations), `Metalog` (logic simulator), `GambiPaint` (drawing
  tool), `yoda-fxsynth` (an FX synth toy, not a game), `PlayBuino` (a
  Game & Watch ROM player/converter, not an original game), and `bub`
  (the staged directory turned out to contain only a README and git
  plumbing, no code at all - this file's own earlier "recovered" listing
  for it was wrong).

## A real Simonbuino bug found via a direct user report: a fabricated turn indicator instead of upstream's own real one

Prompted by "fix simonbuino center display (score is not on correct
position)". Reading real `Buttons.ino` directly (rather than trusting
this port's own prior comment, which claimed the center display had been
deliberately adapted) settled it: upstream's own real `drawButtons()` has
no "CPU"/"YOU" text label of any kind - the whose-turn indicator is a
single 2x2 BLACK dot at one of two fixed positions, `(37,25)` while the
CPU plays back the sequence or `(45,25)` once it's the player's own turn
(with upstream's own real `buttonsShow()` call sitting inline in that same
else-branch), and the sequence-length counter is one real
`cursorX = 39; cursorY = 18; print(melody_step - 1);` call - not the two
separate, differently-positioned/differently-scaled calls
(`gbPrintString("C"/"Y")` at (38,16), `gbPrintNumber(...)` at (34,24))
this port had instead, which turn out to have been invented outright
(this port's own header comment admitted as much - a leftover from an
early pre-bitmap-restoration pass, never corrected against real source
once the real pad art was restored). Fixed `simonDrawButtons()` to the
real, literal dot-plus-single-print version at upstream's own exact real
coordinates.

## A project-wide EEPROM audit: real AVR int-narrowing vs this dialect's always-32-bit int, one real save-breaking bug found and fixed

Prompted by a direct, sharp user question: does composing a multi-byte
EEPROM value out of freshly-erased 0xFF bytes produce a real, observable
difference between real hardware (which has genuine 8/16-bit signed
integer types that narrow on assignment) and this dialect (whose `int` is
always a full 32-bit word that never narrows), and could that break a
`newScore > highscore`-style check in any already-shipped game? A
real, well-founded question, not a hypothetical - and the honest answer
required tracing actual C integer-promotion rules through every real
upstream EEPROM-consuming game's own source, not just this shim's code,
since `eepromShim.c`'s own `eeprom_read_word()`/`eeprom_read_dword()`
already return correctly-widened values - the risk lives entirely in how
each **game's own upstream code** recomposes/compares them.

**The core mechanism, confirmed by hand-tracing real C promotion rules**:
composing a 16-bit value from two 0xFF bytes (`lsb + (msb<<8)`, upstream's
own universal idiom across many of these games) computes an intermediate
`unsigned int` value of 65535 on real AVR - but if the destination
variable/return type is a genuine signed 16-bit `int` (AVR's own native
`int` width), that 65535 narrows to **-1** at the point of assignment/
return, an implementation-defined-but-universal-in-practice two's
complement reinterpretation. This dialect's `int` is always a full 32-bit
word (confirmed directly in `VIRCON32_C_DIALECT.md`) and never narrows at
any assignment/return boundary, so the identical formula here always
stays +65535. Whether this divergence actually matters downstream depends
entirely on what real upstream does with the value next - audited all 25
real EEPROM-consuming games in `src/games/` individually, tracing actual
declared types through the real comparison expressions, not assuming:

- **Explicit magic-byte/token gate before ever trusting a score** (the
  large majority: `game2048.c`, `gameArmageddon.c` [see its own header
  comment - already fixed a related real "permanently locked on a fresh
  cartridge" bug directly], `gameBRally.c`, `gameDeathMaze.c`,
  `gameDigger.c`, `gameFiremen.c`, `gameHexagon.c`, `gameJezzball.c`,
  `gamePunkt.c`, `gameSokobuino.c`, `gameSolitaire.c`,
  `gameSuperCrateBuino.c`) - a dedicated signature/token byte (or an
  explicit `allFresh`-style raw-byte check) is read and compared BEFORE
  the composed score is ever trusted for a real gameplay comparison, so
  the signed/unsigned question never gets a chance to matter - genuinely
  safe on both platforms regardless of int width.
- **Single-byte-only values** (`gameBlobAttack.c`, `gameGruniozerca.c`) -
  a lone byte is 0-255 in every representation regardless of
  signedness, so there is no multi-byte composition step for a width
  mismatch to hide in at all - safe by construction.
- **An explicit upstream `==0xFFFF`-style reset check already present**
  (`gameCrabator.c`, `gameDescent.c`) - real upstream itself already
  detects the freshly-erased case and resets to 0 before any comparison;
  this check's own literal `0xFFFF` happens to compare correctly on both
  platforms (real hardware's stored -1 gets reinterpreted back to
  unsigned 65535 for the `==` comparison against an unsigned-typed
  literal; this dialect's own un-narrowed +65535 matches the literal
  directly) - genuinely safe, not accidentally so.
- **Real upstream already declared the value (or the comparison's other
  operand) as an explicitly unsigned/wide-enough type**
  (`gameBlockdude.c`'s own `unsigned int`, `gameStarships101.c`'s own
  `unsigned int` + an explicit `>60000` reset, `gameVideoPoker.c`'s own
  `long` - already the same 32-bit width as this dialect's `int`, so no
  narrower-type boundary exists to diverge at in the first place, plus
  its own explicit `<5` reset) - genuinely safe.
- **A subtle but real "matches real hardware anyway" case**
  (`gameUfoRace.c`, `gameCrazyTown.c`) - real upstream's own comparison
  mixes the narrowed/negative stored highscore against an
  `unsigned int`-typed "new score" parameter, and C's usual arithmetic
  conversions promote the signed side to unsigned for the comparison,
  reinterpreting real hardware's own stored -1 back to 65535 - the exact
  same effective number this dialect never narrowed away from in the
  first place. `gameUfoRace.c`'s own pre-existing header comment claimed
  this outright as "matching real hardware" without ever tracing the
  actual signed/unsigned mechanics - technically imprecise (the raw
  *stored* value really is -1 on real hardware, not 65535, so the fresh
  table would visually print "-1" there vs "65535" here, a real but
  purely cosmetic display-only difference), corrected to spell out the
  real mechanism precisely rather than leave the imprecise claim
  standing. `gameCrazyTown.c`'s own version is a genuine pre-existing
  *upstream* bug (a real cartridge's own fresh-EEPROM highscore table can
  never register a first save either, for the identical reason) - at the
  time left faithfully preserved, not treated as a porting regression to
  fix; later fixed anyway on direct request, see "CrazyTown's
  fresh-EEPROM highscore display fixed..." further below.
- **The one real, confirmed, previously-undocumented save-breaking
  regression**: `gameSkibuino.c`. Real upstream's own `int
  getHighScore()` composes the identical `unsigned int`-valued 65535
  intermediate, but its *return type* is a genuine signed 16-bit `int`,
  narrowing it to -1 at the `return` statement itself - and unlike
  `gameUfoRace.c`/`gameCrazyTown.c`, neither `highScore` nor
  `metersTraveled` is ever unsigned anywhere in real upstream's own
  `if(metersTraveled > highScore)` check, so nothing launders the sign
  back on real hardware: any real non-negative distance genuinely beats a
  real -1, and a truly fresh cartridge's first-ever run always saves
  correctly. This dialect's own un-narrowed +65535 sentinel is instead an
  "impossibly high" ceiling no real distance could plausibly beat -
  silently and permanently blocking a fresh cartridge's very first
  highscore save, not a cosmetic quirk. Fixed directly in
  `skiGetHighScore()` (detect the genuinely-fresh raw-byte case and return
  a plain 0 instead of the composed 65535, matching real hardware's own
  actual functional outcome rather than its literal bit pattern) -
  verified via Puppeteer against the real integrated build: a fresh
  cartridge's very first crash (24M traveled) now correctly shows "New
  Best!", which the unfixed version could never have shown regardless of
  distance travelled.

No new shared shim primitive was needed or added - this was entirely a
per-game correctness question about how each game's own upstream C code
happens to declare and compare its own persisted values, not a gap in
`eepromShim.c` itself (its own `eeprom_read_word()`/`_dword()` already do
the only thing they reasonably can: return a correctly-widened value with
no information lost, matching real avr-libc's own equally-unsigned
`eeprom_read_word()`/`_dword()` return types exactly). Nothing else in
`eepromShim.c`/`.h` needed to change.

## A follow-up targeted sweep: narrow-int underflow and logical-vs-arithmetic right shift, across all 67 games

Prompted directly ("we did not verify for 8bit vs 32 bit problems during
porting of games") after the EEPROM audit above landed - a fair point:
that audit was narrowly scoped to EEPROM/highscore code specifically,
found via a targeted question, not a general sweep. The same underlying
class of risk (real AVR types that narrow/wrap differently than this
dialect's always-32-bit `int` - confirmed via `avrCompat.h`'s own
`typedef int uint8_t/int8_t/uint16_t/int16_t/uint32_t/int32_t` aliasing)
could in principle exist anywhere a ported game does arithmetic on a
value real upstream declared as a narrower type. Given the scale (67
games), asked directly which approach to take rather than guess - a
targeted grep sweep for the highest-risk patterns, not a full re-read of
every game's own source line-by-line (that remains an option for later if
this sweep's own findings warranted it).

**Category 1 - narrow-type subtraction/underflow** (a `byte`/`uint8_t`
counter that real hardware would wrap positive on underflow, this dialect
instead goes negative, or vice versa for `int8_t`). Swept upstream source
in `more games/` for `byte`/`uint8_t`/`int8_t` health/life/ammo/timer/
counter-style declarations combined with decrement/subtraction, then
checked each shipped game's own real comparison logic against the actual
declared range:
- `gameCopter.c` (`int8_t hp`) - real upstream's own damage subtraction
  is immediately followed by an explicit `if(hp<0) hp=0;` clamp (upstream's
  own code even has a comment about this exact hazard: `// ennemie anti
  bug de l'integer negatif`) - both `hp` and `damage` are `int8_t` (max
  127), so a single hit can reach at most -117, never wrapping past
  `int8_t`'s own -128..127 range on real hardware either - safe on both
  platforms, ported faithfully.
- `gameArtillery.c` (`byte life`) - real upstream decrements by exactly 1
  and checks `<=0`, not `==0` - even in the hypothetical case of running
  the decrement again on an already-dead unit, this dialect's own
  negative result still correctly satisfies `<=0` (real hardware's own
  `byte` would instead wrap positive to 255, arguably a worse outcome
  there) - not a porting regression either way.
- `gameCastleDefence.c` (`byte Ammo`) - checked upstream's own real
  comparisons (`<=` throughout, no exact-equality sentinel) and found no
  underflow-reliant decrement path at all.
- No other shipped game's own narrow-type gameplay counter showed a
  real, exploitable divergence under this pattern.

**Category 2 - logical vs. arithmetic right shift on a negative value**
(`VIRCON32_C_DIALECT.md` states directly: this dialect's `>>` is a
*logical* shift, zero-fill, never sign-extending - a real, previously-hit
class of bug already, per `avrCompat.h`'s own header comment describing a
real bug in the sibling `tinyjoypad_vircon32` project, `hollowseeker`'s
own cave-generation formula, which is exactly why `arand()` exists as a
safe alternative to `rand() % n`). Swept every real `>>` usage in both
`more games/` upstream source and every shipped `src/games/*.c` file:
- The overwhelming majority are provably safe regardless of sign: either
  applied to inherently non-negative data (bitmap/mask bytes, packed
  level tiles, monotonic `gbFrameCount`-derived elapsed time, squared
  values, compile-time-verifiable positive board-size constants), or
  immediately followed by a mask (`&0xFF`, `&0x3`, `&1`) that discards
  exactly the high bits where a logical vs. arithmetic shift would
  actually differ - masking after a shift makes the fill-bit choice
  irrelevant, since the affected bits never survive to the final value.
- **One real, genuine instance found - already caught and correctly
  fixed during its own port, not a live bug**: `gameSolitaire.c`'s own
  win-animation "bounce" effect uses a real 8.8 fixed-point coordinate
  pair whose velocity is explicitly signed (`sign * (...)`, `sign` can be
  -1) and accumulates into a coordinate that can genuinely go negative
  (upstream's own real `bounce.x >> 8`/`bounce.y >> 8` pixel-coordinate
  extraction relies on real AVR's arithmetic right shift to sign-extend
  correctly). This dialect's own logical shift would instead turn a
  small negative fixed-point coordinate into an enormous positive
  garbage value. Already fixed correctly during Solitaire's own original
  port (see `gameSolitaire.c`'s own header comment, written at the time)
  by using plain integer division (`/ 256`) instead of `>> 8` for both
  coordinate extractions - confirmed still in place and correctly
  reasoned through, not something this sweep needed to change.
- No other shipped game showed an unmasked, unguarded right shift
  applied to a value that can realistically go negative.

Net result of this sweep: no new bugs found beyond the EEPROM section
above - Solitaire's own porting agent had already independently caught
and fixed the one real instance of the shift-specific hazard, and the
narrow-type-underflow sweep found every shipped game's own comparison
logic already safe by construction (explicit clamps, `<=` rather than
`==` sentinels, or ranges that can't realistically reach the danger
zone). This is a targeted sweep, not an exhaustive one - a full
per-game re-read (the alternative offered and not chosen) remains
available if a future report suggests this class of bug is still
lurking somewhere this pass didn't check.

## Seven real, previously-session-only highscores given real EEPROM persistence

Prompted directly ("could you check which games show highscores somewhere
but don't save them"). Swept every game with no `eeprom_*()` call at all
for anything display-labeled as a highscore/best score, then verified
each real hit by hand (a false-positive "WHO'S THE BEST?" flavor-text
string in `gameArtillery.c`; a false-positive "RECORD achieved" win
message in `gameSimonbuino.c` - a fixed max-sequence-length win
condition, not a growing persisted value; several genuine "no highscore
concept at all" games already correctly documented as such -
`gameGlaciGlaca.c`/`gameSenet.c`/`gameShipwrek.c`/`gameSnakeClassic.c`/
`gameAsteroidRipper.c`; `gameParachute.c`'s own real upstream `long
highscore;`/`#include <EEPROM.h>` that's genuinely never wired to
anything, not even a display, so there's nothing currently shown to
persist). Real, genuine hits shared the same shape throughout - a real
in-session highscore, tracked and displayed, lost the instant the
cartridge reboots: `gameAsterocks.c`, `gameInvaders.c`, `gameLander.c`,
`gamePaqman.c`, `gameKillrace.c` (all five `yoda-*` arcade clones sharing
this project's own already-noted common author/file-split convention),
`gameFlappyBirdo.c` (a per-difficulty array of three), and
`gameShootBuino.c` (two real top-5 tables - score and combo chain). None
of the seven had real EEPROM persistence upstream *as actually shipped*
either (`gameShootBuino.c`'s own case is more specific - see below) -
this is a genuine enhancement past real hardware, done on direct request
once the display gap was found, not a restoration of dropped shipped
behavior.

**A real self-inflicted miss, caught by the user, not this session's own
review**: the first pass swept only games with zero matches for
`eeprom_*(` anywhere in the file - `gameKillrace.c` and
`gameShootBuino.c` both got excluded by that filter despite having no
real EEPROM calls at all, because each one's own header comment
*mentions* `eeprom_read_byte()`/`eeprom_write_byte()` by name while
explaining why no such call was actually made - a real string match, but
inside a comment, not a real call site. Told directly ("you did not check
correctly killrace is another game displaying highscores"), then asked
to re-sweep for the same mistake elsewhere rather than patch the one
instance and stop - found `gameShootBuino.c` was the only other false
inclusion (confirmed by checking, for every game in the original EEPROM
list, whether the matching line was inside a `//` comment or a genuine
call), and `gameSimonbuino.c` as one more keyword false-positive requiring
its own manual read to rule out. `gameKillrace.c` fixed identically to
the original five. `gameShootBuino.c` needed its own real design decision
(see below), since it's a genuinely different situation from the other
six.

**`gameShootBuino.c` is not quite the same case as the other six** - its
own real upstream `highscore.ino` already contains a complete, real
`#if SAVE_EEPROM` implementation (`SAVE_EEPROM` `#define`d 0, so none of
it compiles into the actual shipped game), spelling out a real intended
EEPROM layout: `highscore[i]` at address `i*2`, `highchain[i]` at address
`NUM_HIGHSCORE*2 + i*2`. Reused that real layout directly rather than
inventing a fresh one, but deliberately did NOT reproduce that dormant
code's own real fresh-cell check (`highscore[thisScore]==0`) - tracing it
through shows the exact same already-documented dead-check bug as
`gameUfoRace.c`'s own real `initHighscore()` (a genuinely fresh EEPROM
composes to 65535, never 0, so this check could never have fired even if
`SAVE_EEPROM` had ever actually been enabled and shipped). Since this
code was never compiled/tested upstream either, there was no "preserve
real shipped behavior" obligation to honor a check that was arguably
already a latent bug on real hardware too - used this project's own
established, already-proven-safe `==0xFFFF` check instead, matching every
other table added this pass. `sbuinoSaveHighscore()` gained a new
`eepromBase` parameter (matching real upstream's own real `eeprom_index`
parameter of the same real, if dormant, function) and now rewrites all 5
entries of whichever table changed on every new high score, exactly
matching real upstream's own real (if never-compiled) save loop.

Added the exact same already-proven-safe pattern this project has used
successfully in `gameCrabator.c`/`gameDescent.c`/`gameCastleDefence.c`/
`gameFirebuino.c` for a while now: `eeprom_read_word(addr)` at init time
with an explicit `==0xFFFF` fresh-EEPROM-cell check (resetting to 0
rather than trusting the raw 65535 sentinel - see the EEPROM audit
section above for exactly why that check matters), and
`eeprom_write_word(addr, value)` at the exact point each game's own
existing `if(score>highscore) highscore=score;` line already updates the
value in memory - a natural one-shot write per new high score, not a
per-frame write, since the guard condition becomes false again the
instant the save happens.

**A real, easy-to-miss trap the user flagged directly before any code was
written**: `gameInvaders.c`'s and `gameLander.c`'s own `init()` functions
each had a real, explicit `invHighscore = 0;`/`landHighScore = 0;` line
(upstream's own real `setup()`-time reset, faithfully preserved when
originally ported) - simply *adding* an `eeprom_read_word()` load
elsewhere without also replacing these two lines would have made the new
persistence a complete no-op, silently overwritten back to 0 on every
single launch immediately after loading the real saved value.
`gameAsterocks.c`/`gamePaqman.c` had no such reset line to begin with (a
plain, never-explicitly-initialized global), so those two only needed the
load added, not a reset replaced.

**A real, initially-wrong claim by this session about the test
environment, corrected directly by the user**: initially assumed the
browser-based web emulator used for every Puppeteer verification in this
project's history has no memory card support at all (based on a stale
claim in `webemulator/Readme.md`), making end-to-end persistence
untestable here. Told directly to read the actual code instead of
trusting the doc - doing so found the real, working mechanism:
`persist_storage.js` mounts a real IndexedDB-backed directory before
`main()` ever runs, and `MainWeb.cpp`'s own `GetMemoryCardPath()`/
`Console.CreateMemoryCard()`/`Console.LoadMemoryCard()` unconditionally
create and load a real per-cartridge `.memc` file from it on every boot,
with a periodic (every 120 frames, ~2s) background sync back to
IndexedDB. So this claim was wrong, not the code - the web emulator
genuinely does support real, working memory-card persistence, and this
project's own `eepromShim.c` sits on top of a real card whenever this
emulator is used, not just the "no card connected" fallback path every
earlier EEPROM verification note in this file had assumed was the only
thing being exercised.

Verified end-to-end by the user directly (not just by this session's own
code review) after this session's own attempt to reach a natural
"game over" in Invaders/Paqman via automated play turned out to be far
slower than expected (surviving 3 lives, or a scroll-gated HUD in
Paqman's own case, made a scripted verification impractical in reasonable
time) - confirmed working as intended for the original five.
`gameKillrace.c`/`gameShootBuino.c` (added in the follow-up correction
above) use the exact same already-proven mechanism and pattern, but
weren't separately re-verified end-to-end the same way - build-clean and
code-reviewed against the same already-working design, not yet
independently played through by the user.

## A real, complete inability-to-jump bug in Smash and Crash, and the same root cause found lurking in two more games

Prompted directly by a live user report ("i can't seem to jump at all") -
verified as genuinely reproducible via Puppeteer first, not assumed: held
Button A continuously for 80+ frames while standing on solid ground, well
past the real `gbRepeat(BTN_A,20)` threshold multiple times over, and the
player never left the platform.

**Root cause, traced precisely**: this file's own `smashPlayerJump` gate
(landing detection) used `gbCollideRectRect()` against each platform's
plain bounding box, an earlier substitution made before this shim had a
real `gbCollideBitmapBitmap()` primitive at all (promoted to the shared
shim only later in the same batch, once `gameDescent.c`'s own port needed
it - this file was never revisited afterward). The file's own header
comment had reasoned this substitution was exact for the platform sprites
specifically (genuinely solid-filled bitmaps, every byte 0xFF) - true,
but it never checked the PLAYER's own bitmap, which has real transparent
pixels, including a fully empty bottom row (every column's real byte has
bit 7 clear). A rect-rect test still treats the player as a solid 8x8
block regardless: the instant a jump moved the player up by its own real
1px impulse, the coarser rect test still reported "still touching" (the
bounding boxes still technically overlapped by several pixels) even
though the real, pixel-perfect test upstream actually uses would not have
- the player's own real empty bottom row means no actual "on" pixel ever
touches the platform's own real top row at that offset. The immediate
re-collision snapped the player straight back to standing within the same
frame the jump impulse was applied, before it could ever become visible -
a complete, permanent inability to jump, not a subtle timing quirk.

**Fixed by restoring the real `gbCollideBitmapBitmap()` calls project-wide
in `gameSmash.c`** (26 call sites across all 4 maps' own platform-landing,
hazard-vs-player, and hazard-vs-platform checks, all originally real
`gb.collideBitmapBitmap()` calls upstream), matching upstream's own exact
call shape one-for-one. Re-verified via Puppeteer: a single A-press now
visibly lifts the player off the platform, and holding it produces
genuine, sustained upward flight.

**Prompted directly to check whether any other already-shipped game had
the same substitution** ("could you check upstream if any other games
used that call"). Swept every real upstream directory for
`collideBitmapBitmap` usage and cross-checked each corresponding port:

- `gameCrazyCar.c`, `gameDescent.c` (the primitive's own original
  promoter), `gameStarships101.c`, `gameThunderShoot.c`, `gameTrexQuest.c`
  (a real, deliberate MIX of `gbCollideBitmapBitmap()`/`gbCollideRectRect()`
  - confirmed correct, since real upstream itself calls both real
  primitives at different sites), and `gameMaruino.c` were all already
  clean.
- **`gameFlappyBirdo.c`**: a real, undocumented instance of the exact same
  substitution (bird-vs-pipe death detection), with no explanatory
  comment at all - likely from the same "ported before the primitive
  existed, never revisited" gap. Fixed identically.
- **`gameSkibuino.c`**: a real instance with its own explicit, wrong
  justification - this file's own header comment claimed real Gamebuino
  Classic's own `collideBitmapBitmap()` is itself nothing more than a
  `collideRectRect()` test (no genuine per-pixel comparison at all),
  explicitly flagged at the time as "recalled-not-re-read-this-session".
  Checked directly against the real library source
  (`more games/Gamebuino-Classic/Gamebuino.cpp`) to settle it for certain:
  `Gamebuino::collideBitmapBitmap()` genuinely does real per-overlap-pixel
  testing via `getBitmapPixel()` on both bitmaps -
  `collideRectRect()` is only used first, as an early-exit bounding-box
  pre-check, not a replacement for the real per-pixel loop. The recalled
  claim was simply wrong. Fixed by adding a new local
  `skiGetSpriteBitmap()` lookup (mirroring `skiGetSpriteWH()`'s/
  `skiDrawAll()`'s own identical dispatch structure) so
  `skiCollideBitmapBitmap()` can call the real `gbCollideBitmapBitmap()`
  primitive directly instead of approximating with rectangles.

Both fixes were smoke-tested via Puppeteer (real gameplay reached, no
crash, no `pageerror`) but not separately replayed to reproduce a
specific pre-fix visual difference the way Smash and Crash's own jump bug
was - unlike the jump gate, neither of these two collision checks is a
discrete state re-evaluated every frame in a way that could silently
cancel an action outright, so the practical impact is a real but more
modest hitbox-precision difference from real hardware, not a "completely
unplayable" bug.

## A real unparenthesized-macro bug in Skibuino's own camera, found via a direct real-hardware screenshot, then swept project-wide

Prompted directly by a live user report ("in our port the players seems
to be drawn/moved too much to the bottom of the screen compared to
upstream"), then settled definitively by the user providing a real
screenshot of the actual original game running - direct visual ground
truth, not a formula re-derivation alone.

**Root cause**: real upstream's own `#define CAMERAYOFFSET (LCDHEIGHT/2)
+ 10` has parens only around `LCDHEIGHT/2`, not around the whole
expression. Its one real call site, `cameraY = player->y -
CAMERAYOFFSET;`, textually substitutes to `player->y - (LCDHEIGHT/2) +
10`, which parses as `(player->y - 24) + 10` = `player->y - 14` - the
trailing `+10` escapes the subtraction entirely, because it was never
inside the macro's own parens to begin with. This port's own
`SKI_CAMERAYOFFSET` had instead been written as a single, fully
parenthesized `(LCDHEIGHT/2 + 10)` - a natural-looking "cleanup" that
silently changes the result to `player->y - 34`, more than double the
real offset. On a 48px-tall screen this is the difference between the
player sitting ~29% down (real hardware, confirmed by the user's own
screenshot) and ~71% down (this port's previous, wrong behavior) - a
real, significant, directly-visible divergence, not a rounding error.
Fixed by reproducing real upstream's own exact macro text (including the
missing outer parens) rather than hardcoding the resulting number, so it
stays correct if `LCDHEIGHT` itself is ever revisited. Re-verified via
Puppeteer: the player now sits at approximately the same on-screen height
as the reference screenshot.

**A near-miss worth documenting**: mid-investigation, before checking
real usage sites carefully, this session almost "fixed" a second,
different macro pair in `gamePunkt.c` (`BOARD_WIDTH`/`HORIZ_CONST_TIME`/
`VERT_CONST_TIME`, also unparenthesized upstream) based on the same
surface pattern - but a careful line-by-line trace of every real usage
site showed none of them are actually affected: real upstream's own
`HORIZ_CONST_TIME` is only ever used as the *leftmost* operand of a `+`/
`-` chain (safe regardless of the macro's own internal grouping - `(A-B)
+ C` and `A-B+C` are the same value) or as the entire right-hand side of
a real `-=` compound assignment mechanically rewritten to `x = x -
MACRO;` (also safe, since `-=` already implicitly groups its whole RHS
the same way full parens would, so a fully-parenthesized macro body
faithfully reproduces it). The genuinely dangerous shape - the one that
actually bit Skibuino - is specifically a macro used as one operand
*embedded inside a larger plain expression* (not a `-=`-derived rewrite),
where a trailing/leading term can end up on the wrong side of the
surrounding operator. Caught before any incorrect edit was made, by
tracing actual usage instead of trusting the surface pattern alone.

**Prompted directly to check every game for the same class of bug**
("can you check all upstream defines in all games against ours for
similar bugs" / "make defines exactly as upstream"). Built a systematic
script comparing every real upstream `#define` (across all 67 shipped
games' own real source, using an authoritative game-to-upstream-directory
map) against its corresponding ported macro, flagging any case where
upstream leaves a multi-term expression unparenthesized but the port
wraps it in a single enclosing pair - the exact shape of the Skibuino
bug. Also ran the same check in reverse (upstream fully parenthesized,
port not) to catch the opposite direction. Found and fixed, matching
real upstream's exact macro text in every case (per the direct
instruction to match exactly rather than rely on per-site safety
analysis): `gameSkibuino.c`'s own `SKI_CAMERAXOFFSET`/`SKI_XSTARTPOS`/
`SKI_YSTARTPOS`/`SKI_SCROLLPOS`/`SKI_XLIMITR` (in addition to
`SKI_CAMERAYOFFSET` above - none of these 5 turned out to be
behavior-affecting at their own real usage sites either, but matched
exactly anyway rather than trusting that analysis to hold forever), and
`gamePunkt.c`'s own `PUNKT_BOARD_WIDTH`/`PUNKT_HORIZ_CONST_TIME`/
`PUNKT_VERT_CONST_TIME` (the ones investigated and ruled out above -
matched exactly anyway, for the same reason). Re-ran both scripts after
every fix until they reported zero remaining candidates across all 67
games. Rebuilt clean; Skibuino re-verified via Puppeteer showing the
corrected on-screen player height.

## Spin Spin Spinbuino and Snake 5110 ported - the two real staged games a "status report" audit found

Prompted directly ("port spinbuino and the other game we just staged"),
following up on the two real Tier 2 candidates found by an earlier
"status report on missing games" audit (`more games/DISCOVERED_GAMES.md`'s
own "Two more real staged directories missed..." section) - both had
already been staged and confirmed genuinely portable, just never actually
ported.

**Spin Spin Spinbuino!** (Charly Piva "Zoglu" / Margot Piva "Isil",
zoglu.net) - `src/games/gameSpinSpinSpinbuino.c`. A real-time dexterity/
avoidance game: a constantly-spinning two-headed baton, steered by the
D-pad through 8 real hand-designed tile-mask levels toward a goal marker
without letting either tip touch a wall, timed against a real per-level
target. All 8 real levels (a 32-row x 8-byte MSB-first tile bitmap each,
256 bytes) and their own real settings arrays (start/goal position,
spring count/position/facing) were extracted byte-for-byte from upstream's
own `level_design.ino` via a small script (every `B01111111`-style Arduino
binary literal converted straight to `0x7F` hex, this dialect having no
such literal syntax) rather than hand-transcribed - verified against each
array's own real declared byte count first. Icon-glyph text strings
(`"\25Next \26Retry \27Menu"` and similar - real Gamebuino font5x7 button-
icon glyphs, ASCII 21/22/23) were ported as explicit 0-terminated `int[]`
character-code arrays, the same treatment `gameTaquin.c`'s/
`gameSimonbuino.c`'s own header comments already established for the
identical real gap (a quoted string literal can't hold them directly).

**Snake 5110** (Lady Awesome & MakerSquirrel, CC-BY-SA 2018 per the source
file's own header - though the repo's own top-level `LICENSE` is GPLv3, a
real, unresolved conflict not resolved here) - `src/games/gameSnake5110.c`,
registered in the menu as "SNAKE 5110". A real Snake game with two
genuinely distinct modes: a faithful classic mode (solid walls, single
food type), and a "new" mode with a real, distinctive mechanic nothing
else in this catalog has - the outer wall is built from individually
removable segments, torn down one at a time by eating a special "wall"
prey, letting the snake wrap through the resulting gaps to the opposite
edge, alongside two more prey types (instant +3 growth, or -4 shrink).
Confirmed via direct diff against this project's own already-shipped
Snake Classic (Ripper121/Tnxec2's single-file procedural port) to be a
genuinely different, unrelated codebase (class-based `Coordinate`/`Snake`
structs across separate `.h`/`.cpp` files, different author, different
mechanics) before porting - not a duplicate. Upstream's own real
`Coordinate`/`Snake` classes were flattened into plain globals/parallel
`int[]` arrays, matching this project's own established "flatten a single-
instance C++ class into plain C" treatment (see gamePunkt.c's own header
comment). Two real, deliberate simplifications, both matching an already-
established precedent elsewhere in this project: `gb.menu()` (no
equivalent in this shim) was hand-rolled, the same treatment
`gameConduit.c`'s own `condUpdateMenu()` already established; highscore
name entry (`gb.getDefaultName()`/`gb.keyboard()`) was dropped outright,
matching `gameArmageddon.c`'s own already-documented precedent - this
port's own highscore tables persist scores only. The real 64x36
`Snake5110Logo` title bitmap was restored and drawn via `gbDrawBitmap()`
at upstream's own real `titleScreen()` anchor. Two real, upstream-genuine
bugs were preserved deliberately, not "fixed": `Coordinate::setOffBounds()`'s
own real `{ m_x = -2; m_y -2; }` typo (a missing `=` that makes the `m_y`
assignment a no-op, harmless in practice since `isInArena()` already
returns false from `m_x` alone), and `deleteRandomWallElement()`'s own
inverted "last wall on this axis" special case, which means New mode's
outer wall can never be fully cleared on either axis - one segment per
axis always remains standing, a real functional gameplay quirk upstream
itself ships, not a porting regression.

**Two real, project-wide shim/dialect lessons found while porting these
two**, both significant enough to be worth remembering for any future
port, neither specific to either game:

- **This dialect has no ternary operator at all** (`VIRCON32_C_DIALECT.md`
  already documents this, but it hadn't actually bitten a real port here
  until now) - `gameSnake5110.c`'s own first draft used `a ? b : c` in
  over a dozen places (direction/mode selection, highscore-table
  selection, menu-label highlighting) before the compiler flagged every
  one; all rewritten to explicit `if`/`else`, including a small
  `snkPickDirection()` helper replacing a real nested-ternary chain
  upstream itself only needed because C++ allows chaining them at all.
- **Every game's own `_init()` must call `gbBegin()` itself** - confirmed
  by grepping all 69 games: every single one already does this as its own
  first real statement, a requirement this project's own documentation
  had never actually spelled out explicitly. Found the hard way:
  `gameSpinSpinSpinbuino.c`'s own first working draft omitted it
  entirely, producing a genuinely confusing symptom to debug from cold -
  a fully blank white screen on every state (title, menu, gameplay) with
  no JS console error at all, `gbFillRect()`-style primitives still
  visibly working (confirmed via a deliberately minimal diagnostic
  build), but every single `gbPrintString()` call silently drawing
  nothing. Root cause: `gbFontPtr` (the active font's own data pointer)
  is only ever assigned inside `gbSetFont()`, which `gbBegin()` itself
  calls once with the real default font - skip `gbBegin()` and
  `gbFontPtr` stays at whatever a *previous* game's own session left it
  (or unset entirely, on a truly cold cartridge boot), silently breaking
  every text draw while every non-text primitive keeps working fine,
  since those don't depend on `gbFontPtr` at all. Found via a systematic
  bisection (strip `init()` to nothing, then strip `update()`'s own state
  dispatch down to one direct function call, then strip that function's
  own body down one call at a time) rather than guesswork, once a direct
  comparison against an already-working game's own `_init()` (mid-
  bisection) surfaced the missing call by contrast.

Both games verified via Puppeteer: real menu registration (alphabetical
position recomputed against the new, larger game list - now 69 entries),
title screen (real bitmap art for both), full state-machine navigation
(Spin Spin Spinbuino's own level-select circles and level 0 gameplay with
real wall collision/timer/scrolling; Snake 5110's own difficulty screen,
both game modes, and a real end-to-end wallcrash-to-gameover-to-highscore-
to-menu chain in classic mode), with no `pageerror` at any point. Each has
a real gameplay thumbnail (the third thumbnail atlas grows from 5 to 7 of
its own 12 cells, `THUMBNAIL_COUNT` 67->69) and screenshot.
`more games/DISCOVERED_GAMES.md` updated to mark both ported; `MAX_GAMES`
(72) has 3 slots of headroom left, no raise needed yet.

## Nine more games ported - all of the fifth discovery pass's own Tier 1, a real button-debounce bug found and fixed in Aimbuino

Prompted directly ("port all staged tier 1 games") - the 9 real,
Tier-1-sized candidates the fifth discovery pass had added (see "`more
games/` - staged source for future ports" above): `Gamebuino-
PongLocalMultiplayer` (qubist), `SavePrincesse` (Clement83), `MotoCross`
(Clement83), `StijnCaerts-Gamebuino` (Stijn Caerts - one repo, two real
games), `NoNamePlatformGame` (Frakasss), `MasterKebab` (ogbaba),
`Aimbuino` (Baptiste Pouget, hosted under ogbaba's account), `Ralph`
(Clement83), and `FOOTLOL-Gamebuino` (Baptiste Pouget, hosted under
ogbaba's account).

**Started manually, switched to the established parallel-agent workflow
mid-batch on direct instruction ("use agents ffs")**: `gamePong
LocalMultiplayer.c`/`gameSavePrincesse.c` were already fully written and
verified by the time this instruction landed, so those two were kept as
my own manual work and the remaining 7 were ported the same
parallel-isolated-copy way batches 4-6 already established (each agent
gets its own full private project copy plus its own one staged game
folder, ports and verifies independently via a clean `compile` pass, the
orchestrating session integrates one at a time afterward against the
real shared build).

**Real per-game findings**:
- **`Gamebuino-PongLocalMultiplayer`**: upstream's own draw calls sit
  OUTSIDE `if(gb.update())` - moved inside this shim's mandatory
  `gbUpdate()` gate. A real title-text overflow was caught via Puppeteer
  (`"PONGLOCALMULTI"`, 14 chars at font5x7 from `cursorX=4`, 4px past
  LCDWIDTH) and shortened to `"PONG 2-PLAYER"` (13 chars, fits).
- **`SavePrincesse`**: French text/comments preserved verbatim throughout
  (including the win screen's own `"Youhou !"`/`"appuis sur A"`). A real
  upstream quirk was preserved rather than "fixed": collision hit-tests
  always use the attacking-pose bitmaps regardless of which pose is
  actually drawn.
- **`MotoCross`**: the rider is fixed at `x=0` for the whole game (real
  upstream never assigns `player1.x` anywhere else) and there's no
  scoring/game-over at all - both preserved as genuinely dead-but-shipped
  upstream behavior, not gaps in the port. Two real upstream logic bugs
  kept bit-for-bit (a duplicate dead `else if` arm; a bare-int condition
  standing in for a missing comparison, confirmed this dialect accepts an
  int directly as a boolean).
- **`StijnCaerts-Gamebuino`**: a real, verified-by-reading-not-assuming
  finding - the repo's own Pong is actually single-player-vs-CPU, not
  local 2-player the way the folder name suggested; ported what's
  genuinely there rather than the folder name's own implication. A real
  upstream bug preserved: `resetBall()` uses `LCDWIDTH` instead of
  `LCDHEIGHT` for Y-centering. The repo's own Snake is a genuinely
  incomplete tutorial stub with no direction/growth/food logic at all -
  real, playable gameplay was built guided by upstream's own
  declared-but-unused globals (`vx`/`vy`/`score`), explicitly documented
  in the port's own header comment as invented, not extracted, since
  there was no real upstream logic to extract in the first place. The
  external `ivanseidel/LinkedList` dependency was flattened to plain
  parallel arrays (`ssnkBodyX[200]`/`ssnkBodyY[200]`), matching
  `gameSnake5110.c`'s own already-established precedent for the same
  problem.
- **`NoNamePlatformGame`**: a real platformer despite its own
  placeholder-looking name; no scoring/enemies/win-lose condition at all.
  Sound approximated via `gbPlayNote()`, matching this project's own
  long-standing one-shot-tone scope limit. Three real upstream quirks
  preserved (dead globals, unreachable eye-blink branches, a genuine
  double call to `outpt_drawLandscape()`).
- **`MasterKebab`**: a real EEPROM fresh-cell bug found and fixed, the
  same class as the historical Skibuino bug - upstream's own
  `!partie.premiere_partie` sentinel only works against a fresh in-RAM
  struct (reads 0), not a genuinely fresh/erased EEPROM cell (reads
  0xFFFFFFFF as a dword); fixed via an explicit
  `kebabPremierePartie != 1111` check against upstream's own real
  sentinel value, and extended the fresh-reset branch to zero 3 more
  fields upstream's own fresh-branch never reset either. The real
  upstream `struct`+`memcpy`-to-EEPROM pattern was flattened to 7 named
  globals plus `eeprom_read_dword()`/`eeprom_write_dword()`, matching this
  project's own established "flatten a struct into named globals"
  treatment.
- **`Ralph`**: verified directly, not assumed - the player only pans a
  debug-style camera, Ralph himself auto-punches with no player input at
  all. A real switch-fallthrough double-draw and a real 88px-wide bitmap
  drawn on an 84px-wide LCD were both confirmed safe (the fallthrough is
  genuinely intentional upstream layering; `gbDrawBitmap()` already clips
  per-pixel, so the 4px overflow is silently and correctly dropped) and
  kept as-is. Genuinely dead upstream code (an unused splash-screen
  bitmap, an unreachable intro state, empty function bodies) was dropped.
- **`FOOTLOL-Gamebuino`**: a real 3v3 physics football game, hotseat
  turn-based - both teams share the same controls, the same shape as
  Bomber/StickFighter/Tron's own local-hotseat treatment. The real
  on-screen title is "Footuino", not "Footlol" - preserved as upstream
  actually displays it. A real, *necessary* fix (not a discretionary
  cleanup): upstream's own hand-rolled `atan(dy/dx)` hard-traps on
  div-by-zero for a same-X collision, a genuinely reachable case in normal
  play - replaced with `atan2(dy,dx)` plus a defensive `dx==0&&dy==0`
  guard against `atan2`'s own undefined `(0,0)` case, which also
  incidentally corrected a minor upstream quadrant bug as a side effect.
  Real upstream `Joueur`/`Balle` structs flattened to parallel float
  arrays.

**A real button-bleed-through bug in Aimbuino, found via live user
testing** ("aimbuino needs a debounce on the A Button going from
titlescreen to gameplay") - matching a confusing "always lands on
LEADERBOARD instead of FREE MODE" result already independently observed
during this port's own Puppeteer verification, but not yet root-caused at
the time. Aimbuino chains three separate Button-A-driven state
transitions in a row: title screen -> internal menu, menu selection ->
gameplay, and, once in gameplay, `aimbViser()`'s own aim-charge/launch
gesture (`gbRepeat(BTN_A,1)` to charge, `gbReleased(BTN_A)` to launch). A
real physical A-press held across a state transition is still "held" the
instant the next state's own update function starts reading `BTN_A`
again that same logic tick (a real hold can span multiple 20fps ticks) -
so the exact same press that dismissed the title screen could also
immediately confirm whichever menu item the cursor defaulted to, and the
exact same press that confirmed a menu item could immediately register as
the start of (or even the release-triggered launch of) an aim-charge in
gameplay. Distinct from the cartridge-level `md_armInputAGate()` fix,
which only guards the menu-launch transition once per game launch - this
needed a local, reusable gate applied at all three of Aimbuino's own
internal Button-A-driven transitions.

Fixed with a new `aimbAGated` flag plus `aimbConsumeA()` helper (only
reports a fresh press once Button A has actually been observed released
since the gate was last armed), used at the title->menu and menu->
playing/leaderboard transitions, and the same gating logic applied
directly to `aimbViser()`'s own two raw `BTN_A` checks for the third
transition point. Verified via Puppeteer end-to-end: a clean
title->menu->FREE MODE sequence (one press per transition) now correctly
reaches genuine aiming gameplay with the ball still at its initial
position and no phantom charge/launch, and a real aim-charge-and-release
gesture afterward correctly transitions the ball into flight. Aimbuino's
own thumbnail/screenshot were recaptured afterward (the previous ones,
captured before this bug was found, had landed on the leaderboard screen
instead of real gameplay) - the new ones show the actual aiming HUD
(score/high-score, target, ball, aim line).

**Two more thumbnail-atlas slots used**: the third thumbnail texture grew
from 4x3 (12 cells) to 4x5 (20 cells), 1024x384 to 1024x640, still well
under the real 1024x1024 GPU ceiling. `MAX_GAMES` raised from 72 to 88 for
the same "modest headroom past the real current total" reason as every
previous raise (the real total is 79 registrations after this batch, 10
new `addGame()` calls counting Stijn's bundle as 2 separate games).

**A follow-up sweep of all 9 new games for the same three bug classes this
project has already found real instances of elsewhere** (a displayed-but-
unpersisted highscore, real AVR narrow-int-underflow divergence, and
unmasked `>>` on a value that can go negative) turned up nothing needing a
fix: none of the 9 display any highscore/best-score concept at all except
`MasterKebab` (real EEPROM money/state save, already implemented) and
`Aimbuino` (real EEPROM leaderboard, already implemented per its own fix
above) - the rest (Pong Local Multiplayer, Stijn's Pong) only ever show a
live, resets-every-match score, matching their own real upstream, which
has no highscore concept either. None of the 9 ported files nor their real
upstream sources use `>>` anywhere at all. The one real narrow-type
(`byte`) usage worth tracing - `NoNamePlatformGame`'s own
`player.x_screen`, decremented by up to 2 per tick with only an `>0` guard
- can reach -1 on real hardware (wrapping to 255, a byte) versus this
port's own plain, non-narrowing `int` (staying -1); traced the actual
consequence on both platforms rather than assuming it matters: real
hardware's wrap briefly draws the sprite far off its own real 84px-wide
screen for one tick before naturally decrementing back down over several
frames, while this port's own -1 instead fails its own `>0` movement guard
for exactly one tick (holding `BTN_LEFT` further does nothing) but is
immediately fixed by the very next `BTN_RIGHT` press - a real but minor,
self-recovering one-frame cosmetic quirk on both platforms, not a
save-breaking or soft-locking divergence, so left as-is per this project's
own established bar for when a platform difference needs an active fix
(the Skibuino highscore case) versus just needing to be traced and
understood (the UfoRace/CrazyTown highscore-display cases).

**Seventy-eight games shipped this pass** (69 existing + 9 new) - see
each new file's own header comment in `src/games/` for its own full
per-game porting notes, and the Games table in `README.md` for licenses/
sources. Every one is registered in `menuGameList.c`, verified crash-free
and genuinely playable via Puppeteer, and has a real gameplay thumbnail
and screenshot. This clears all of the fifth discovery pass's own Tier 1
entries; `wuuff`'s own three unported RPGs (`under-the-tower`/
`StarHonor-gamebuino`/`elventure-gamebuino`) are the most substantial
remaining candidates - see `more games/DISCOVERED_GAMES.md`'s own
"Recommended next pick" section.

## Five more games ported (all of Tier 2) - a real crash-preventing fix in PinBall, a real class-flattening precedent reused cleanly in DarkShmup

Prompted directly ("port all tier 2 games using agents") - the 5 real,
Tier-2-sized candidates still unported after the previous Tier 1 batch:
`MyRPG` (Frakasss), `PinBall` (Clement83), `pong-2017` (yawn-g),
`DarkShmup` (Clement83 - a real, distinct repo, confirmed via
`git ls-remote` before dispatch, unrelated to the earlier fabricated
"Frakasss/DarkShmup" claim), and `Robot` (Frakasss). Used the established
parallel-isolated-copy agent workflow directly from the start this time
(no manual-then-agent switch needed, unlike the previous batch) - 5
agents, each with its own full private project copy plus its own one
staged game folder, each verified via a clean `compile` pass in isolation,
integrated into the shared tree one at a time as each finished.

**Real per-game findings**:
- **MyRPG**: a top-down overworld walker with real per-pixel collision
  (reads the just-drawn framebuffer back via `getPixel()` instead of a
  separate solidity map). Upstream has no title screen and never reads
  Button A at all (`gb.titleScreen()` is commented out in real `setup()`)
  - this port matches that exactly, starting gameplay immediately with no
  title state. Two real, asymmetric upstream bugs preserved: `output_map()`
  never syncs `map_previous` back after a LEFT/DOWN slide (only RIGHT/UP
  do), and a real vignette-style black-border wipe on house entry/exit
  ported call-for-call in upstream's own exact draw order.
- **PinBall**: a real-time pinball table (flippers, pull-back launch
  spring, bumpers, a shake/nudge mechanic). A real, platform-forced fix
  (not a preference): `pinbGetNormale()`'s own final normalize-divide can
  hit a genuine zero-length vector when the ball's center is collinear
  with a wall segment - real AVR silently produces Infinity/NaN there, but
  Vircon32 hard-traps the CPU on a float divide-by-zero
  (`VIRCON32_C_DIALECT.md` section 17.3), so an unguarded port would crash
  the emulator outright the first time this reachable state occurs.
  Guarded with an explicit `norme == 0` check (returns a zero vector, no
  bounce that frame) rather than ported as-is. Also preserved: upstream's
  own `#define FROTTEMENT 0.98;` has a genuine trailing-semicolon typo
  baked into the macro body, harmless at both real call sites, reproduced
  exactly.
- **pong-2017** ("Pong Revisited"): single-player Pong vs a ball-tracking
  AI plus a real power-up/"tricks" menu. A real, load-bearing upstream
  color-state leak preserved: `drawBackground()` sets GRAY for the net and
  never resets to BLACK, so player names/life gauges/round bars/the active-
  trick HUD all draw in GRAY on real hardware too - reproduced by simply
  never inserting an extra `gbSetColor()` call anywhere upstream doesn't
  have one. 3 of 5 real placed tricks have zero actual gameplay effect (a
  real `switch` with no matching `case`), confirmed by direct read, not
  assumed. The unrelated `other/test-i2c/` subfolder (a standalone I2C
  hardware test sketch, not part of the real game) was correctly left
  unread; the main sketch's own dead `#include <Wire.h>` and
  never-called `masterWrite()` forward declaration were simply dropped.
  No highscore concept exists upstream at all - none was invented.
- **DarkShmup**: a vertical shoot-'em-up with a real "dark world" swap
  mechanic (enemies belong to one of two dimensions, only one
  visible/hittable at a time). Like `gameSuperSpaceShooter.c`, real
  upstream here has no gameplay logic in its own class files at all -
  `StarShip.h`/`StarShipPlayer.h`/`Bullet.h`/`Explosion.h` are genuinely
  pure, method-free data-only C++ classes (each matching `.cpp` file is a
  single `#include` line) - making the flattening simpler than
  `gameSuperSpaceShooter.c`'s own (no method bodies to relocate, only
  mechanical class->struct/pointer->index rewrites). Real narrow-AVR-type
  behavior emulated deliberately where upstream's own comparisons depend
  on it: `StarShipPlayer::Life` (real `uint8_t`) wraps via an explicit
  `& 255` on its one decrement site, `Score`/`OldScore` (real
  `unsigned int`) via `& 65535` on every increment - both real, intentional
  reproductions of AVR narrow-int wraparound this dialect's own
  always-32-bit `int` would otherwise silently skip. A real missing
  `break` in `UpdatePosVaisseauEnnemie()`'s `switch` (skin-2 boss enemies
  get two update functions applied every tick) preserved exactly.
- **Robot**: a run-and-gun platformer across 5 worlds each ending in a
  boss fight, 7 enemy types. Genuinely plain `.ino`/struct-based
  throughout, no C++ classes anywhere (verified directly) - ported as real
  named `RoboXxx` structs with real struct-array globals, reusing
  `gameBomber.c`'s own already-proven struct-array-plus-pointer-parameter
  pattern rather than flattening into parallel arrays. A real upstream
  copy-paste bug preserved: the Tesla-tower enemy (type 4) reuses its own
  `x_min`/`x_max` fields as an unrelated (x,y) coordinate pair in both its
  draw call and its bullet-collision test, visually wrong but never
  crashing (this shim's drawing/fill primitives all clip safely). Real 2D
  PROGMEM sprite tables (several with implicit trailing-zero-fill rows
  shorter than their own declared width) were converted to hex byte-for-
  byte via a small script, correctly zero-padding short rows to match -
  verified this matters for real gameplay, not just tidiness (a rocket
  enemy's own animation frame is genuinely drawn from a partially-implicit
  row).

**No new shared shim primitive gaps were found this batch** - every game
needed only primitives already promoted in earlier batches
(`gbCollideRectRect`/`gbCollideBitmapBitmap`, `gbDrawBitmap`/
`gbDrawBitmapRotated`, `gbFillRect`/`gbDrawFastHLine`/`VLine`,
`gbPrintString`/`gbPrintNumber`, `gbRepeat`/`gbPressed`/`gbHeld`,
`gbPlayNote`/`gbPlayTick`/`gbPlayOK`/`gbPlayCancel`, `gbFrameCount`,
`arand`, `eeprom_read/write_word`). Confirmed via each agent's own direct
report rather than just trusted.

**EEPROM/highscore audit**: none of the 5 games needed new persistence -
`MyRPG`/`PinBall`/`pong-2017`/`Robot` genuinely have no highscore concept
at all in real upstream (confirmed by direct source read in each case,
not assumed), and `DarkShmup`'s own `Score` is purely session-only with no
`EEPROM.h` include anywhere. Matches this project's own "don't invent a
highscore concept real upstream never had" precedent.

**Thumbnail atlas headroom exhausted, then regrown**: the third thumbnail
texture's 4x5/20-cell capacity (added during the previous Tier 1 batch)
turned out to be exactly, precisely filled by this batch's own 5 new
games (registration indices 79-83, local atlas indices 15-19 - the last 5
of exactly 20 available cells) - zero spare cells left the moment they
were placed. Grown immediately to 4x6 (24 cells, 1024x768, still well
under the real 1024x1024 GPU ceiling) for the same "modest headroom past
the real current total" reason as every earlier growth. `MAX_GAMES`
raised from 88 to 96 (was down to its own last 4 spare slots).

**Prompted directly to also check whether any already-shipped games
display a highscore without saving it, and to sweep for 32-bit/8-bit
narrowing and `>>`-on-negative bugs across the same 5 new games** - same
methodology as the earlier project-wide sweeps (see "A follow-up targeted
sweep..." above): none of the 5 display an unpersisted highscore (none
have a highscore concept at all, per the audit above), and grep found
zero `>>` usage anywhere across all 5 ported files. No fix needed.

**A separate, direct follow-up request found and fixed a real, long-
standing screenshot-format inconsistency**: `metadata/screenshots/`
mixed two different capture conventions - the vast majority (72 of 84)
are the raw, uncropped 640x360 Puppeteer capture (full browser viewport,
menu chrome and all), but 12 files (the Tier 1 batch's own `AIMBUINO`,
plus this Tier 2 batch's own 5 new games, plus `BOMBER`/`STICKFIGHTER`/
`TRON`/`B-RALLY` from earlier batches) had instead been saved as the
588x336 LCD-only crop used for the thumbnail-atlas compositing step - a
real, accidental convention drift, not a deliberate second format.
Re-captured all 12 as genuine full 640x360 raw screenshots (recapturing
`BOMBER`/`STICKFIGHTER`/`TRON` fresh via Puppeteer, since no raw capture
of those three survived from their own original porting session) and
overwrote each file - all 84 screenshots are now confirmed uniformly
640x360.

**Two more real screenshot/thumbnail redos, both prompted directly**:
`SIMONBUINO`'s own screenshot/thumbnail was showing its title-screen logo
bitmap, not real gameplay - recaptured showing the actual 4-pad Simon
board mid-sequence. `B-RALLY`'s own screenshot showed a genuine but
uninteresting frame (the pre-race countdown, `Spd:0`, car stationary, the
bottom third of the screen blank) - recaptured after waiting out the real
60-tick countdown and holding the accelerator, now showing the car
actively driving on a curving road at speed. Both re-composited into
their respective thumbnail atlas cells and re-verified via Puppeteer.

**Five games shipped this pass**: MyRPG (Frakasss, none specified),
PinBall (Clement83, none specified), pong-2017 (yawn-g, none specified),
DarkShmup (Clement83, none specified), and Robot (Frakasss, none
specified) - see each file's own header comment in `src/games/` for its
own full real porting notes, and the Games table in `README.md` for
sources. Every one is registered in `menuGameList.c`, verified crash-free
and genuinely playable via Puppeteer (zero `pageerror`s across all 5), and
has a real gameplay thumbnail and screenshot. This clears the entirety of
the "5 unported Tier 2 candidates" list - the fifth discovery pass's own
Tier 1 and Tier 2 are now both fully shipped; only Tier 3's remaining
entries (most substantially `wuuff`'s three unported RPGs) and Tier 4's
genuine engineering-cost cases remain in `more games/`.

## Pirates ported, then reverted - a real Tier 3 "SD-card porting infrastructure" concern turned out to be entirely dead code, but a genuine combat-breaking design flaw could not be resolved

Prompted directly by a follow-up question on `Pirates`' own Tier 3 audit
note ("what does it read from sdcard? sprites? if so can't they be
converted to normal c array loading stuff") - investigated before writing
any code, rather than porting around the assumed dependency or trusting
the audit note's own "genuine new porting-infrastructure need" framing at
face value. Reading the real source directly settled it precisely: the
sprites (`SpritesP1`/`SpritesP1Mask`/`SpritesP2`/`SpritesP2Mask`,
`player1_sprites`/`player2_sprites`) are NOT SD-loaded at all - they're
already real, baked-in PROGMEM hex-byte arrays sitting directly in
`Pirates.ino`, exactly the same format every other bitmap in this project
already converts. The only real SD usage anywhere in the repo is a tiny
cross-*sketch* handoff (`PIRATE.JC`, one integer - which character was
selected on a separate menu cartridge) - and both real `checkDataFile()`
implementations that would have read it back (`Pirates.ino`'s own and
`pirateGame/pirateGame.ino`'s own copy) open with a literal, unconditional
`selectedCharacter = 0;//ONLY for simbuino test` followed by
`return;//ONLY for simbuino test` - real, confirmed dead code in every
compilable variant of this game, not a porting assumption. This
substantially lowered Pirates' real porting cost versus the audit's own
earlier guess, and a direct "port the pirate game with what you learned"
request followed immediately.

**A genuinely unusual repo shape, worked through rather than picked
around**: `more games/Pirates/` contains three real, separately-buildable
Arduino sketches sharing the same combat code - a self-contained top-level
build (`Pirates.ino`/`Player.ino`/`arena.ino`/`finalScreen.ino`, no title/
menu screen at all, straight into combat) meant for solo testing, a
near-duplicate `pirateGame/` meant to be flashed as a separate physical
cartridge from `pirateMenu/`, and `pirateMenu/` itself - a real, complete,
working character-select grid (a real 16-item bitmap menu, D-pad
navigation with wraparound) plus its own title bitmap, that on real
hardware would flash `pirateGame` onto a second cartridge via the (now-
confirmed non-functional) SD handoff once a fighter was chosen. Rather
than pick only one sketch, this port combines the best real parts of all
three: the top-level sketch's own complete combat/arena/win-lose logic
(the most complete self-contained variant) preceded by `pirateMenu`'s own
real title screen and character-select grid - genuinely navigable exactly
like upstream, but (matching the confirmed dead-code finding precisely)
the fight that follows always uses sprite-set 0 regardless of what was
highlighted, exactly like every real, compilable version of this game
already does. Not a shortcut this port took - the honest, verified real
behavior, stated as such in the file's own header comment rather than
silently "fixed" into a functional selection real upstream never had.

**A real, genuinely necessary platform-forced fix, found and fixed by the
porting agent, not just inherited from the brief**: `player1_sprites`/
`player2_sprites` only define frame-rect rows for `currentState` values
0-3, but real gameplay reaches state 7 (mid-air with no attack timer
running) and state 9 (KO - reached at the end of essentially every real
match) - an out-of-bounds `pgm_read_byte()` on real AVR reads whatever
happens to sit next in flash (undefined but harmless-in-practice on real
hardware); the equivalent OOB read on this platform would pull an
arbitrary unrelated global as a sprite width/height feeding an unbounded
pixel-blit loop, a real crash/hang risk, not a cosmetic difference. Fixed
with a small `piratClampSpriteState()` helper (state 7 clamped to the real
jump pose, state 9/anything else clamped to idle), applied at all four
real lookup sites - the one and only behavior change from real upstream in
this entire port, made because it was required for correctness on this
platform, not because it looked wrong.

**Real upstream quirks preserved deliberately, several only found by
direct diffing rather than assumed**: a byte-diff of `SpritesP1`/
`SpritesP1Mask` against `SpritesP2`/`SpritesP2Mask` (and of
`player1_sprites` against `player2_sprites`) showed they're completely
byte-identical - real upstream's own `drawBitmapCustom()` always reads
from sheet "1" regardless of which player is being drawn, a real quirk
with zero visible effect since the two sheets are provably the same data;
ported by declaring the shared sprite tables once rather than duplicating
identical data, matching upstream's own real, if accidental, outcome
while saving genuine ROM space. `addToCombo()`'s real body always returns
false (dead combo-tracking, ported anyway for fidelity), `bottomFigther()`/
`kickFigther()` (Button B / Down) are real, deliberate no-op stubs (a
never-finished "kick" feature, state 5/6, preserved as an inert but still-
called dead code path exactly like upstream), the real `Ayouken`/
`animSprite` special-move struct is entirely commented out at its own only
real instantiation site upstream and was dropped rather than ported dead-
on-arrival, and the real AI's own `highFigther()` call is commented out
too (the AI opponent never jumps, by design, not a missing feature).

**Necessary deviations from upstream, both explicitly platform-forced**:
Button C / the real `load_game("PIRATES")` multi-cart SD-flash call was
dropped entirely (no Vircon32 equivalent for "flash a different physical
cartridge" exists, matching this project's own established treatment for
this class of real-hardware-only call) - this cartridge's own global
Start-button quit-confirmation dialog already provides the equivalent
"return to the shared menu" functionality project-wide. The final
screen's own real "return to menu" choice (which used to trigger that
same dropped call) now returns to this game's own title screen instead,
the closest real equivalent left once the multi-cart call is gone.

**`drawBitmapMask()`/`drawBitmapCustom()`**: no direct shim primitive
equivalent exists for a source-sub-rectangle blit - built a local
`piratDrawBitmapMask()` on top of the shim's existing
`gbGetBitmapPixel()`/`gbDrawPixel()`, faithfully reproducing real
upstream's own per-pixel mask-composite loop including a real, asymmetric
flip-coordinate quirk (`k = dst_w - k`, not `dst_w - k - 1`) rather than
"cleaning it up" into the more obviously-symmetric version. No shared shim
gap was found - every other primitive needed already existed.

Verified via Puppeteer: real title screen (the ship logo bitmap), the real
16-item character-select grid (fully navigable, a visible selection
cursor), and active 1-vs-AI combat (both fighter sprites, health bars, the
real arena background) all confirmed reachable with zero `pageerror`s.
`MAX_GAMES` (96) had ample headroom; the third thumbnail texture's 24-cell
capacity (added during the Tier 2 batch) had exactly one spare cell left,
used by this game with zero to spare afterward - the very next new game
will need another texture growth. `THUMBNAIL_COUNT` raised 84->85.

### A real, live user report: no hit ever lands, and the game runs in visible slow motion

Prompted directly by live play ("when actually fighting i can't seem to
get a hit or get health reduced from the enemy player, even if i don't
act at all my health does not decrease either eventually well always get
a time over"). Traced to two real, separate bugs, both confirmed present
in real upstream's own unmodified source (not introduced by porting):

**Bug 1 - none of the three real collision boxes account for facing
direction.** `gestionAttack()`'s own attacker-reach box (`[posX-4,
posX+10]`) and defender-hurtbox (`[posX, posX+6]`), and `updPlayer()`'s
own body-block/wall-collision box, are all written as a fixed offset in
the +X direction from `posX` - correct only for a fighter facing
`NOFLIP` (rightward). Fighters swap `NOFLIP`/`FLIPH` every tick based on
which one is currently on the left/right (`if(Player1.posX<Player2.posX)
...`), and `drwPlayer()`'s own real sprite-draw call already mirrors
correctly for `FLIPH` (`dst_x = (dir==NOFLIP)? posX : posX-dst_w`) - but
none of the three collision boxes do the same mirroring, so whichever
fighter is currently on the right (facing left) has both their own
attacks aimed away from their opponent and an unhittable hurtbox
positioned in the empty space behind them, not on their own real,
mirrored sprite. Confirmed identical in real upstream `Player.ino` line
159/338 - a genuine, pre-existing bug in the original game, not a porting
mistake. Fixed with two new helpers, `piratBodyLeftX()`/
`piratAttackReachLeftX()`, mirroring each box around `posX` for a
`FLIPH` fighter exactly the way `drwPlayer()` already mirrors the sprite
itself.

**A second, deeper problem this first fix alone did not solve**: even
correctly mirrored, the body-block/wall-collision box uses each
fighter's own *full* sprite width (~25-32px) for both sides, so two
fighters physically stop roughly `play_w+other_w` apart - upwards of
55-60px on an 84px-wide screen - long before the ~16px real punch reach
could ever connect. Hand-tracing the *original, unmirrored* upstream
formula shows the identical problem already exists there too (its own
single, unmirrored width alone already exceeds the 16px punch reach) -
a second, real, pre-existing design flaw in the original game, not a
result of the direction-mirror fix. Since real upstream itself seems
to have shipped a fighting game whose two fighters can never actually
stand close enough to land a punch via ordinary movement, "preserve real
upstream behavior" and "make the game's own core mechanic reachable at
all" were in direct conflict here. Per the user's own explicit choice
("Fix it") when this was raised directly, the body-block was narrowed to
the same 6px width the hurtbox already uses on both sides (rather than
disabling body-blocking outright), so "solid enough to feel like a body"
and "close enough to land a punch" became the same real distance instead
of two disconnected ones.

**A related, separate real bug, found from a direct live report ("the
performance rewrite broke gray color stuff when enabled") after a first
performance fix**: `piratDrawBitmapMask()` originally called 2-3 real
shim primitives (`gbGetBitmapPixel()`/`gbSetColor()`/`gbDrawPixel()`) per
individual pixel, for two full fighter sprites (~1000+ pixels each) plus
a health-bar-border mask, every single tick - a live report of the
browser tab pegging one CPU core at 100% with movement/combat both
visibly running in slow motion (confirmed via this shim's own
`gbFrameCount`-delta timing: an effective ~16-20 ticks/sec instead of the
real upstream `gb.setFrameRate(41)` target) traced directly to this one
function's own real per-call overhead (this dialect's documented flat
~10+2×argcount instructions per call, paid per pixel, blowing well past
the real ~250,000-instructions-per-frame-at-60fps budget - the same
lesson already applied project-wide to the shared shim's own
`gbDrawBitmap()`, but never applied to this game's own *local* masking
helper). Rewrote it to inline the same bit-unpacking `gbGetBitmapPixel()`
itself does and write `gbFrameBuffer[]` directly, matching
`gbDrawPixel()`'s own real addressing formula - **but the rewrite
initially skipped `gbDrawPixel()`'s own real gray-buffer "un-gray"
bookkeeping** (`if(gbRealGrayColor && gbAnyGrayDrawn)
gbGrayBuffer[idx]&=~bit;`, which real `gbDrawPixel()` runs on *every*
color draw, not just gray ones, to clear any stale gray-bit a previous
GRAY draw left at that exact pixel) - a fighter sprite drawn on top of
the arena's own real GB_GRAY-dithered background left stale gray bits
behind it, which `gbRenderFrame()`'s own second gray-tinted pass then
incorrectly re-tinted over solid sprite pixels. Fixed by adding the same
bookkeeping the direct-framebuffer rewrite otherwise correctly copied
from `gbDrawPixel()`'s own real logic.

**The combat-reach fix and the performance fix were both real,
verified-correct code changes** (confirmed via careful hand-tracing of
concrete coordinate numbers for the reach fix, and via directly measured
`gbFrameCount` deltas for the performance fix) - but repeated live
testing after each fix still found the same "no hit ever lands" symptom,
and further code-level re-verification (re-reading every line of the
attack/hurtbox/body-block chain multiple times, checking for parameter-
order mistakes, stale life-reset code paths, sprite-rect indexing errors)
found no further error. Given the user's own direct, repeated live
testing is the authoritative signal here (automated Puppeteer testing in
this specific headless/software-rendered environment could not reliably
reproduce close-range combat at all, due to the same real frame-rate
throttling under SwiftShader software GL rendering that this session
also measured directly), and given this game's own combat loop had by
this point required three separate real bug fixes without confirmed
resolution, the user made the direct call: **remove Pirates from the
cartridge entirely and mark it no longer a candidate** (see `more games/
DISCOVERED_GAMES.md`'s own "Excluded from porting entirely" section for
the corresponding audit-file update). `src/games/gamePirates.c` was
deleted; its `#include`/`addGame()` lines removed from `src/main.c`/
`menuGameList.c`; `THUMBNAIL_COUNT` reverted 85->84 and its own thumbnail-
atlas cell cleared; `README.md`'s game count reverted to 84 and its own
table row removed. **Eighty-four games remain shipped.**

## A real Spin Spin Spinbuino porting bug found via a live user screenshot comparison: two swapped gbSetColor() calls

Prompted directly by the user pasting a real reference screenshot next to
this port's own broken menu screen ("check spin spin buino something is
off in our port") - the level-select menu was missing all of its own text
(both the level-name line and the "Your record:"/"Go for it!" line), and
the icon carousel showed only a couple of solid black blobs with no
connecting lines between them, instead of the real small circle icons
joined by dotted lines.

Reproduced directly via Puppeteer first, then traced to one exact root
cause by comparing this port's own `menu.ino`-derived code side-by-side
with the real upstream `more games/SpinSpinSpinbuino/spin/menu.ino`: the
node-connector block (drawn once per visible level icon, right before the
icon itself) is supposed to punch a WHITE gap at the node position then
restore BLACK for everything drawn afterward -

```c
gb.display.setColor(WHITE);
gb.display.fillRect(drawX+1,drawY+1,6,6);
gb.display.setColor(BLACK);
```

but this port's own translation had the two `gbSetColor()` arguments
swapped (`gbSetColor(1)` / BLACK first, `gbSetColor(0)` / WHITE last) -
the exact opposite of upstream. Since `gbColor` is a persistent global
never reset by `gbUpdate()`'s own per-tick `gbClear()`, once this ran even
once the color stayed WHITE for every subsequent draw call for the rest
of that tick *and* into every following tick - silently blanking the
connecting lines (drawn before the swap, inheriting the previous tick's
already-leaked WHITE), the node icons themselves, and both text prints
(`spinNomNiveau()`'s own level-name line and the "Your record"/"Go for
it!" line), matching every symptom the user reported (missing top text,
the "Unlock/Erase records" entry never appearing when navigating all the
way left, no connecting lines) from one single swapped pair of arguments.
Fixed by swapping the two calls back to match upstream exactly; verified
via Puppeteer that the menu now renders identically to the user's own
real reference screenshot (text, connecting lines, and icons all visible).

**A related, separately-reported concern investigated and confirmed to be
real, intentional (if arguably confusingly labeled) upstream design, not a
second bug**: using the "Unlock/Erase records" entry appears to
unconditionally unlock every level and wipe every highscore the instant
Button A is pressed, with no real way to select the on-screen "No way!"
option via A. Checked directly against real upstream `credits.ino`:
`updateCredits()`'s own real `-2` branch draws both a "\25Let's do this!"
and a "\26No way!" line, but has no selection-cursor state of any kind -
real upstream's own `if(gb.buttons.pressed(BTN_A))` unconditionally wipes
every record and unlocks every level regardless of which label is
visually highlighted, exactly like this port's already-faithful
`spinUpdateCredits()`; only Button B or C actually decline. This port's
own `spinUnlockYes`/`spinUnlockNo` icon-glyph arrays were also confirmed
byte-for-byte faithful to real upstream's own `\25`/`\26` icon codes.
Nothing was changed here - a real, if slightly surprising, original-game
quirk, preserved correctly.

## Four more games ported (all remaining Tier 3) - the fifth discovery pass's own tier tables are now fully cleared, `Pirates` aside

Prompted directly ("port all remaining tier 3 games using agents"),
immediately following the `Pirates` revert - the 4 real, substantial
RPGs left in Tier 3 after `Pirates` was excluded: `PetitMonstre`
("Futuromon", Clement83), `Elventure` (GPLv3, real original by trodoss/
TEAM a.r.g., this specific Gamebuino Classic port by wuuff), `UnderTheTower`
(GPLv3, wuuff), and `StarHonor` (MIT, a real port of Wenceslao Villanueva
Jr's original Arduboy game, Gamebuino port also by wuuff). All four are
genuinely large (2900-3700 lines each) - the biggest batch of individual
games by average size this project has ported in one pass. Used the same
parallel-isolated-copy workflow as every batch since batch 4, with one
new addition to every agent's own prompt: both real, hard-won lessons
from the `Pirates` investigation immediately before this batch (direction-
unaware collision math, and the real performance risk of any custom
per-pixel bitmap-masking loop) were spelled out explicitly up front, not
left to be independently rediscovered.

**Both `Pirates` lessons were checked deliberately in all four ports, and
none of the four actually needed either fix** - a real, confirmed absence,
not a shortcut: `PetitMonstre` and `UnderTheTower` have no facing-
dependent collision or custom bitmap-masking at all; `Elventure`'s own
`bitmap_funcs.cpp` turned out to have its own real per-pixel masking code
already dead - the live, compiled body of every one of its own
`overlaybitmap()`/`erasebitmap()`/`eraseBitmapRect()` functions is just a
direct `gb.display.drawBitmap()`/`fillRect()`/`setColor()` call, with the
real per-pixel compositing logic sitting inert inside a `/* ... */` block
comment (dead leftover from the unrelated "Parachute" sketch this file
was borrowed from); `StarHonor`'s own `StarField` draws 20 individual
stars via one real `gbDrawPixel()` call per star per tick - the intended,
correctly-cheap use of that primitive, not a masked-blit loop in
disguise. This is genuine, checked-not-assumed confirmation that the
`Pirates` bugs were specific to that one game's own real upstream design,
not a systemic risk lurking in every large game - but worth having
checked explicitly rather than trusted by default, given how severe
`Pirates`' own version of both problems turned out to be.

**PetitMonstre** ("Futuromon") - a real Pokémon-style monster-catching/
battling RPG: overworld exploration with random wild-monster/rival-
trainer encounters, turn-based combat (6 elemental types, catch mechanic,
XP/leveling), a Team/Futurodex status menu. The single biggest porting
challenge in this game specifically was structural, not data-related:
real upstream's combat is built almost entirely from nested blocking
`while(true){if(gb.update()){...}}` loops (one per sub-phase - intro
flash, monster arrival, menus, attack/death animations, end-of-fight
pause), with one dispatcher call running an entire multi-round fight
synchronously across potentially hundreds of real ticks before ever
returning - flattened into a two-level state machine (`petmState` plus a
21-phase nested `petmCombatPhase`), preserving every real per-phase tick
count and RNG call exactly. **A real, confirmed permanent soft-lock was
found and fixed**: Game Over's own real `A`-press restart path calls
`InitialisationGame()` (which internally sets state to the start scene)
but `GameOverScreen()` itself then returns 0, and the dispatcher
unconditionally overwrites that state with the returned 0 - dropping the
player into exploration with a completely empty team, after which the
next encounter's own team-select menu (`while(Vie<=0)`) can never
resolve. Fixed by routing the restart to the start-scene state directly,
per this project's own established "don't preserve a real infinite loop/
soft-lock" exception to "preserve real upstream behavior." A real, more
minor upstream data-table numbering mismatch (`Monster::Type`'s own
Feux/Eau/Terre/Plante ordering disagrees with `GetAttakByPatternNumero()`'s
own switch ordering on indices 2/3, so a real Terre-type monster fights
with Plante's own move-set and vice versa) was preserved exactly, along
with a real broken-onslaught-damage-roll formula (a discarded comma-
expression operand) and a real gap in `HaveBonusAttak()`'s own type-
effectiveness check. No EEPROM/sound exists upstream to port (confirmed
via grep - neither is ever called).

**Elventure** - a real, top-down Zelda-like action-adventure: a single
elf explores a 128-room scrolling overworld/underworld map with a
throwable sword, fighting roaming monsters, collecting hearts and 4 real
quest items (winning requires any 3, not necessarily distinct types, per
upstream's own literal slot-counting logic). Real upstream sets
`persistence=true` and hand-manages incremental erase/redraw as a pure
AVR CPU optimization - never reading pixels back for game logic (unlike
`gameTron.c`, this cartridge's one genuine pixel-readback persistence
case) - so this port simply redraws the whole current room+HUD+elf+room-
elements fresh every tick instead, provably pixel-identical output at
upstream's own throttled `gb.setFrameRate(10)`. Real upstream's own
`play_song()` has a literal `return;` as its first statement ("NOTE
EARLY RETURN!! Currently NOT playing any music.") - the entire real
melody/tempo/pattern system is dead code in the real shipped build,
dropped entirely; only the real one-shot `playCancel/Tick/OK()` calls
(reachable via a separate, live code path) were kept. A real, deliberately
preserved upstream map-scroll bounds check only tests one edge of two
(`elvScrollMap()`'s own DOWN/RIGHT checks only test `<128`, never the
real row/column edge) - matching this project's own already-established
`gameUfoRace.c` precedent for the identical class of real, harmless-in-
practice unguarded read. A real pre-existing LICENSE-vs-header-comment
discrepancy (top-level `LICENSE` says GPLv3, `ELV_TV_v10.ino`'s own
header comment says "version 2... or any later version") was flagged
rather than silently resolved either way, the same treatment this
project's README already gives an analogous real concern for
`gameFiremen.c`.

**UnderTheTower** - a real turn-based RPG: an overworld town, real
door-triggered dungeon transitions, 10 real procedurally-generated
dungeons (a recursive BSP-style room splitter, ported unmodified), a
3-member party recruited over the story, menu-driven battle
(attack/ability/item/run), and real EEPROM save/load. **Several real
upstream off-by-something bugs were found and fixed because they risked
genuine out-of-bounds reads or infinite loops on this platform** (all
documented in the file's own header comment, not silently patched): a
`COMPRESSED_SIZE` constant (2652) that didn't match the real `world[]`
array's own actual measured size (2630) - bounded and given a safe
fallback tile instead of reading past the array; a `NUM_DUNGEONS`
constant (18) that didn't match the real, live `dungeons[]` table (12
real entries - the rest exist only inside a `/* */`-commented alternate
revision) - used the real, live count instead; an unbounded proximity
check that could read one row past a 16x16 dungeon array - clamped;
three real unbounded `while(1)` exit-placement retry loops in
`mapexits()` - capped at 1000 attempts with a safe fallback, since an
infinite loop is a genuine hang on this platform where real AVR hardware
would eventually just get lucky (or not, and hang there too, harmlessly
indistinguishable from working since nothing else can preempt it) -
matching this project's own established "don't preserve a real infinite
loop" exception. A real narrow-int EEPROM audit (matching this project's
own established methodology) found one genuine divergence: `game_status[]`
is real `int8_t` starting at -1 - this dialect's always-32-bit `int` would
load a saved fresh-cell sentinel back as +255 instead of -1, so a new
`uttNarrowS8()` helper replicates the real AVR narrowing on load. Real
upstream quirks preserved as confirmed-harmless: `load_enemy_data()`'s own
boss-fight branch resets `meta_mode` back to a normal dungeon spawn pool
for any companion enemy in the same fight (not a second boss copy), and
`restore_game()` never resets the screen-wipe `transition` counter, so
loading a save skips the wipe-in animation.

**StarHonor** - a real roguelike space adventure, MIT-licensed, the
Gamebuino Classic port (by wuuff) of Wenceslao Villanueva Jr's original
Arduboy game. A real structural wrinkle unique to this game in the batch:
the repo also ships `ArduboyCustom.cpp/h`/`coreCustom.cpp/h`, suggesting a
second compatibility-shim layer - checked directly rather than assumed,
and both turned out to be genuinely empty (`coreCustom.h` is just an
empty include guard); `Globals.cpp` shows the real wiring is already a
direct `Gamebuino arduboy;` instance with every call site already calling
straight into the real library, so there was no second translation layer
to route around. `Vector2d` (a real x/y float pair with `+`/`-`/`*`
operator overloads used constantly throughout real upstream) was
flattened to a `StarVec2` struct plus explicit-pointer functions
(`starVecAdd`/`Sub`/`Scale`/`Normalize`/`Rotate`/etc) - a 2-word struct
return is over this dialect's real 1-word function-return limit. **Two
real upstream bugs were found and fixed as genuine crash risks, not
preference**: `Neutral_Response[random(0,5)]` indexes a real 4-entry
array with a 5-wide random range (fixed to a correct 0-4 range), and five
of `CopyIntoBuffer()`'s own real call sites pass a copy length longer
than the real source string's own actual length (found via an offline
script cross-checking every real call site's length argument against its
own string's real length) - harmless on real AVR PROGMEM, but the
resulting over-read on this platform could hand `gbDrawChar()`/
`gbPrintString()` an arbitrary adjacent global's bit pattern as a
"character code," a real potential font-table OOB index. Despite being
flagged in this project's own earlier audit as a "real EEPROM save"
candidate, a full grep across all 22 real upstream files found no
`EEPROM.read()`/`.write()` call anywhere - only a dead `#include
<EEPROM.h>` line - so no save/load logic exists to port, matching this
project's own already-established `gameShipwrek.c` precedent for the
identical situation (a real `.eep`-adjacent artifact with nothing live
behind it).

All four verified crash-free and genuinely playable via Puppeteer (real
title screens, real gameplay states reached, zero `pageerror`s) - one
real, live navigation gotcha worth remembering: `StarHonor`'s own title
screen dismisses on Button B (`starBButton`), not Button A, a real,
faithful detail that looked like a bug during verification until traced
directly to `starGetInput()`'s own real button-latching logic. Each has a
real gameplay thumbnail and screenshot. The third thumbnail texture's own
24-cell capacity (4x6, added during the Tier 2 batch) is now **exactly,
completely full - zero cells to spare** - the very next new game ported
into this cartridge will need a fourth thumbnail texture.
`THUMBNAIL_COUNT` raised 84->88 across the batch (accounting for
`Pirates`' own prior 84->85 that was reverted back down before this batch
began). **Eighty-eight games now shipped.** This exhausts every currently-
known normal-effort candidate in `more games/` - only Tier 4's genuine
engineering-cost cases (`cruiser`, `BigBlackBox`, `CopterStrike`, `Duel`)
remain unstarted; see that section of `more games/DISCOVERED_GAMES.md`
for the full picture.

## cruiser and BigBlackBox ported - Tier 4's "own copy of the Gamebuino library" concern checked directly and found unfounded for both

Prompted directly ("for the games in tier 4 having their own copy /
changed gamebuino lib verify what is different against the original lib
and see if it really requires a complete new shim for those"), followed
immediately by "port both games then" once the investigation settled the
question. Both `cruiser` and `BigBlackBox` had sat in Tier 4 since the
original porting-priority audit under a "ships its own from-scratch
Gamebuino API reimplementation" label - a label that, like
Bomber/StickFighter/Tron's own earlier "needs an AI redesign" label
(see "A closer look at Bomber/StickFighter/Tron" above), had never
actually been checked against the real source, just inferred from the
presence of extra files that looked library-shaped.

- **`cruiser`'s own `port/` folder** turned out to be a completely
  separate, unrelated desktop/GLUT development harness - its own real
  `main()`, gated behind a `PORT_ENABLED` `#define` the real cartridge
  build never sets. The real game, `cruiser.ino`, already `#include
  <Gamebuino.h>` directly and needed nothing beyond a normal port.
- **`BigBlackBox`'s own `lib_*` files** (`lib_Gamebuino.cpp/.h`/
  `lib_Display`/`lib_Sound`/`lib_Buttons`/`lib_Backlight`/`lib_Battery`/
  `lib_font3x3/3x5/5x7`) turned out to be the real, genuine LGPL Gamebuino
  Classic library, merely vendored locally under a `lib_` prefix -
  confirmed via direct byte-level comparison against the real library
  source (identical method signatures, byte-for-byte identical font
  data), not a different API surface at all. Its real line count is 2568,
  correcting an earlier audit pass's mistaken "5,662" estimate (that
  figure double-counted the vendored library files as if they were
  original game code).

Neither game needed any new shim primitive - both used the same
already-proven `gamebuinoShim.h`/`.c` surface every other game in this
cartridge already shares. Ported using the same parallel-isolated-copy
workflow as every batch since batch 4.

**`cruiser`** (Michael Specht, no license specified) - a real
portal-rendering 3D shooter/tech demo: fixed-point 3D math, Sutherland-
Hodgman frustum clipping, sub-pixel Bresenham line drawing, real portal-
based room traversal. No enemies, no score - a genuine flythrough
tech-demo of the engine itself, not an arcade game. **A real,
live division-by-zero crash was found post-launch via a direct user
report** (with a screenshot of the actual Vircon32 emulator's own
division-by-zero trap screen) - this platform hard-traps on integer
divide-by-zero, unlike real AVR hardware, which just produces a garbage
result and keeps running. Root-caused to two genuinely unguarded
divisions the porting agent's own report had already flagged as
theoretical risks: `cruiVec3dNormalize()`'s own `16777216 / len` (a
zero-length vector, e.g. a vertex that projects exactly onto the camera)
and `cruiProjectVertex()`'s own `-16777216 / pz` (a vertex exactly on the
camera's own z-plane). Both fixed with a direct zero-guard matching this
project's own already-established `pinbGetNormale()` precedent
(`gamePinBall.c`) - `cruiVec3dNormalize()` returns the vector unchanged
on a zero length, `cruiProjectVertex()` substitutes `pz=1` to avoid the
trap while keeping the projection formula's own shape intact. A third
division site (the frustum-clip interpolation, `( -d1 << 8 ) / ( d0 - d1
)`) was checked and proven safe by construction rather than also guarded
defensively - it's only ever reached when `flag0 != flag1`, which
guarantees `d0-d1` can never be zero. Verified via an aggressive Puppeteer
stress test (10 cycles of thrust/turn/fire plus 3 seconds of continuous
thrust, deliberately trying to fly into walls and reproduce edge-case
vertex geometry) showing no further crash.

**`BigBlackBox`** (STUDIOCRAFTapps, a custom "keep this credit, don't
sell it" license quoted verbatim in the file's own header comment) - a
real 13-level physics-platformer: squash-and-stretch jump physics,
wall-jumping, teleporters, keys/locked doors. Real EEPROM-saved level-
unlock progress, given this project's own already-established fresh-cell
sentinel-check pattern (`LevelsUnlock` checked against `0xFFFF` before
being trusted). Sound approximated to one-shot tones via `bbbSfx()`,
matching this project's own long-established scope limit for every
game's own real tracker/pattern music.

Both registered in `menuGameList.c` (`"CRUISER"`/`"MICHAEL SPECHT"`,
`"BIGBLACKBOX"`/`"STUDIOCRAFTAPPS"`), verified crash-free and genuinely
playable via Puppeteer, and given a real gameplay thumbnail and
screenshot. The third thumbnail texture was already completely full (see
"Four more games ported (all remaining Tier 3)" above) - a fourth
texture, `THUMBNAILS4_TEXTURE_ID` (id 6, appended after
`GRAYCOLUMNS_TEXTURE_ID`'s own id 4 and `THUMBNAILS3_TEXTURE_ID`'s own id
5, matching this project's own "append, don't insert" texture-id
precedent), a 4x3/12-cell (1024x384) texture, was added, with
`md_drawGameThumbnail()`'s own three-way split extended to a four-way
one. `THUMBNAIL_COUNT` raised 88->90. **Ninety games now shipped** - this
clears 2 of Tier 4's original 4 engineering-cost cases, leaving only
`CopterStrike` (real consolidation work across its own heavily-duplicated
mission subfolders) and `Duel` (a genuine two-cart-only multiplayer game
with no AI at all, unlike Bomber/StickFighter/Tron) as real remaining
candidates - see `more games/DISCOVERED_GAMES.md`'s own updated Tier 4
section for the full picture.

A real, previously-undocumented set of Vircon32 C-dialect facts surfaced
during `cruiser`'s own port, flagged by that agent for a future
`VIRCON32_C_DIALECT.md` update: a bare `0` cannot be used as a null-
pointer constant for any pointer type (needs an explicit cast, e.g.
`(Type*)0`), and a bare array name cannot be the left operand of pointer
arithmetic (`arr + offset` fails to compile; `&arr[offset]` works). Also
confirmed a real, useful pattern: a struct can contain a plain array
member, a nested named-struct member, and an array-of-struct member,
provided variables of that struct type are declared without the `struct`
keyword at usage sites (`Type var;`, not `struct Type var;`).

**One of that agent's own four flagged findings was itself wrong, caught
and corrected directly on a follow-up "verify it" request** ("function
pointers actually do work the crisp game lib port for vircon32 uses
them (check c:\github) verify it then apply needed corrections"):
function pointers genuinely DO work on this platform - proven, not just
claimed, by two independent pieces of real, already-working code checked
directly: `crisp-game-lib-portable-vircon32`'s own `onResetGame`/
`onSaveData` hooks (`cglp.h`/`cglp.c`), and, closer to home, this very
cartridge's own entire menu/game-dispatch table (`menu.h`'s
`GameFunc* init`/`update`/`onResume`, driving all ~90 games via real
`&function` assignments in `menuGameList.c`'s own `addGame()` calls) -
function pointers have been this project's own load-bearing dispatch
mechanism since game #1, not an untested feature. What the cruiser
agent's own probe actually found is narrower and real: the *raw,
standard-C inline* declaration syntax (`int (*fp)(int);` - the literal
form `VIRCON32_C_DIALECT.md`'s own type/name table listed, which the
agent tried verbatim) is rejected, for the same reason `int arr[10];`
is rejected in favor of `int[10] arr;` - the dialect's `<type> <name>`
rule applies to function-pointer types too. The real, working idiom
(already proven by both `menu.h` and `cglp.h`) is to `typedef` the
function's signature into a named type first, then declare pointer
variables of that type (`typedef void(void) GameFunc; GameFunc* init;`),
assigned via explicit `&function`. `VIRCON32_C_DIALECT.md`'s own
"Function pointers" section (§2) and `gameCruiser.c`'s own header
comment were both corrected to state this precisely instead of the
false "don't work at all" claim - and the corrected dialect doc was
synced to its identical copy in the sibling `tinyjoypad_vircon32`
project too, since both carry the same file verbatim. The other three
findings (the null-pointer-constant cast requirement, the array-name
pointer-arithmetic restriction, and the struct-composition pattern) were
re-checked against this correction pass and hold up - only the function-
pointer claim was a genuine false negative.

## Duel ruled out, CopterStrike's real duplicate-folder structure analyzed and ported - the porting-priority audit's last real candidate

Prompted directly ("rule out duel then port copterstrike but analyze the
duplicates first as to why it happened"). `Duel` (Clement83, 486 lines)
was checked directly - confirmed via a full read of `Duel.ino`/
`master.ino`/`slave.ino` to be genuine two-cartridge `Wire.h` master/
slave multiplayer with no AI and no single-player fallback of any kind,
unlike Bomber/StickFighter/Tron (which looked the same at a glance but
each turned out to ship a real solo AI mode once actually read) - `Duel`
genuinely has no such path, so porting it would mean writing an entire
fighting AI from scratch with no real upstream logic to base it on, out
of scope for this project's "port real upstream behavior" methodology.
Ruled out permanently and moved to `more games/DISCOVERED_GAMES.md`'s own
"Excluded from porting entirely" section.

**`CopterStrike`'s own "10,623 lines/27 files... needs real consolidation
work" audit note was a shallow line-count read, never actually
investigated** - reading every real file directly (confirmed via
`git remote`/`git log`: a single `Frakasss/CopterStrike` commit, "Add
files via upload," clean working tree, so the on-disk structure genuinely
is what the author committed, not a staging artifact) found the real
reason `Function.ino`/`Output.ino`/`Sprites.ino` appear six times splits
into two entirely different causes - see `more games/DISCOVERED_GAMES.md`'s
own "The CopterStrike duplicate-folder analysis" section for the full
writeup:

1. **Pure accidental duplication**: the repo root build and the triple-
   nested `CopterStrike/CopterStrike/CopterStrike/` folder are
   byte-identical - a real Arduino-IDE sketch-folder-naming artifact from
   the author's own local nested project layout, preserved verbatim by a
   one-shot "Add files via upload" commit rather than a clean git
   history. Purely redundant; ignored entirely.
2. **Real, deliberate per-mission forks**: `missions/{convoi,desert,
   forest,searchDoc}/` each carry a genuinely different-sized copy of the
   shared engine plus their own mission-specific entry point and their
   own separately-compiled `.HEX` - because Arduino sketches compile
   per-folder with no cross-folder `#include`, and because a real
   ATmega328p's 32KB flash budget couldn't fit every mission's own full
   sprite/logic set in one binary, forcing each mission to become its own
   standalone, trimmed cartridge build. Vircon32 has no equivalent flash
   pressure, so this real hardware-forced split doesn't need preserving.

The repo root build turned out to already be a real, working **two-
mission cartridge** ("Desert Strike"/"Forest Strike", selectable from one
in-game menu with an Easy/Normal/Hard difficulty picker) - the author's
own actual merged release. `missions/convoi/`/`missions/searchDoc/` are
two more real, distinct missions (own unique objective/sprites) never
folded into that merged build; `missions/mer/` has sprite assets only, no
code, an abandoned mission, dropped from scope; `copterMenu/` is a real
SD-card multi-cartridge-flash loader with no Vircon32 equivalent (the
same class of real-hardware-only feature already dropped during the
`Pirates` port's own `load_game()` call), also dropped.

**Ported as one unified 4-mission cartridge** - Desert Strike and Forest
Strike from the root build's own merged engine, extended with Convoi's
and SearchDoc's own real, distinct mission logic/art as two more
selectable entries on the same mission-select menu (`case`s 2/3 added to
`outpt_menu()`'s own mission-preview screen, now paging two boxes at a
time) - a genuine single-binary 4-mission release no real physical
CopterStrike cartridge ever shipped, built entirely from real upstream
mission code with nothing invented, just no longer artificially
fragmented by an AVR flash-size ceiling Vircon32 doesn't share. Ported
via this project's own established parallel-isolated-copy agent workflow
(one agent, one game, given the full duplicate-folder analysis above as
explicit instructions rather than left to rediscover it) - the resulting
diff against the shared project tree was exactly 3 files (`main.c`'s own
`#include`, `menuGameList.c`'s own `addGame()`, and the new
`gameCopterStrike.c` itself), with zero shared-shim edits: every real
primitive needed (`gbDrawBitmap`/`gbDrawBitmapRotated`, `gbFillRect`/
`gbDrawRect`/`gbDrawLine`, `gbPrintString`/`gbPrintNumber`, `gbPressed`/
`gbRepeat`, `gbCollidePointRect`, `gbFrameCount`, `gbAbsInt`, `gbPlayNote`,
`arand`) already existed.

Prefixed `cstr` throughout (`gameCopter.c`, an unrelated already-shipped
game, already owns `copt`). Real upstream quirks preserved deliberately
(each with its own inline comment in the file): a dead tower-fire-rate
`switch` case left over from the standalone per-mission sprite numbering;
a triple-explosion special case that only fires for the desert village,
never forest; every mobile "tank" unit's spawn-building index colliding
on a real upstream copy-paste bug; a dead mobile-unit money-award branch;
Convoi's road-explosion animation only ever showing frames 0-4 of 7; and
the forest heliport's own block writing the same array cell five times
instead of five different ones. A handful of real one-past-the-end array
reads (three loops, plus one already-corrupted index inherited from the
tank spawn-building bug above) were bounded rather than left to read
whatever unrelated global sits next on this platform - the same
"preserve real behavior unless it would crash/corrupt state specifically
on Vircon32" bar this project has applied consistently since Pirates/
UnderTheTower. One genuinely necessary rewrite: `updateFriendMobile()`'s
real `sqrt(pow(dx,2)+pow(dy,2)) < 3` proximity test becomes the exactly-
equivalent integer form `dx*dx + dy*dy < 9`, since this platform hard-
traps on `sqrt` of a negative value the way real AVR silently doesn't.
Sound collapses to this project's already-established one-shot-tone
approximation, deliberately emitting at most one tone per tick (matching
real hardware's own single sound channel) rather than the many
simultaneous voices a naive per-call translation of `outpt_animBoom()`'s
own explosion loop would otherwise fire.

Verified via Puppeteer against the real integrated build: menu
registration at the correct real alphabetical position (right after
`COPTER`, a different, unrelated already-shipped game), the real logo
title screen, the extended 4-mission/2-page select screen (Desert Strike/
Forest Strike/Convoi/SearchDoc, each with its own real preview art and a
working Easy/Normal/Hard picker), and genuine gameplay reached in all
three missions checked directly (Desert Strike, Convoi, SearchDoc) with
zero `pageerror`s - HUD, buildings, heliport and the player's own copter
sprite all rendering correctly. Author confirmed as **Frakasss**
(`github.com/Frakasss/CopterStrike` per `.git/config`) with no license
file anywhere in the repo (registered accordingly - `"None specified"` in
`README.md`, matching `cruiser`'s own identical situation). No EEPROM/
highscore exists anywhere in this repo (confirmed by grep across every
sketch) - none was invented. The fourth thumbnail texture (still mostly
empty after `cruiser`/`BigBlackBox`) took the new thumbnail with no
growth needed; `THUMBNAIL_COUNT` raised 90->91. **Ninety-one games now
shipped** - this was the porting-priority audit's own last real
candidate; only `CopterStrike`'s own sibling case `Duel` remains in `more
games/`, and that one is permanently excluded (see above), not merely
unstarted.

**A direct follow-up audit request** ("check if all games/dirs/subdirs
in more games really got a port except for the excluded games") verified
this claim rather than just trusted it: all 97 real directories in `more
games/` were cross-checked against every ported `.c` file's own header
comment (not just directory-name matching - 8 initial non-matches were
individually re-checked and confirmed genuinely ported under a different
name in their own header). Found the port coverage itself is complete
(90 directories map to exactly 91 shipped games, `StijnCaerts-Gamebuino`
alone yielding two), but also found a real gap in the *triage process*:
`Gamebuino-Classic/examples/` (the official library, already staged as
`Pong Solo`'s own real source) bundles more example sketches that were
never entered into the porting-priority audit or the excluded list at
all, since they live inside that library folder rather than their own
dedicated top-level staged directory - `PongMulti` (a real, genuine
two-cartridge-only Pong variant with no AI, the same shape as `Duel`,
and redundant besides given this cartridge's own three already-shipped
Pong variants) and `Conway` (Conway's Game of Life, a pure demo with no
player-driven objective, the same non-game category as `Metalog`/
`GambiPaint`), plus a handful of tutorial/physics-demo/reference
sketches in the same folder that are even less game-shaped. Added to
`more games/DISCOVERED_GAMES.md`'s own "Excluded from porting entirely"
section with the same reasoning. No code changed - a documentation-
completeness fix, not a porting gap.

## Two real Star Honor bugs found via live user reports: an endless fight, and a menu-wide buffer overflow

**Endless combat, prompted directly** ("in star honor game when in a
fight with an enemy shouldn't the player win the fight when enemies
defense is lower or equal to zero (verify upstream) it seems enemy
fights are endless in our port" - later clarified live: "the enemy
(planet's) defense keeps going further negative"). Verified against real
upstream first, not assumed: `StarHonor.ino`'s own `EncouterUpdate()`
combat-calc `switch` case has a real `break;` immediately after setting
the victory sequence, which exits the case entirely and skips the shield-
damage calculation plus the case's own trailing `nextSequence = 9`
overwrite below it - the only thing that makes a real win actually stick.
This dialect has no `switch`, so that case became an `if`/`else if`
chain in `gameStarHonor.c`, and the mechanical conversion missed that the
trailing `nextSequence = 9` needed to move inside the win-check's own
`else` to reproduce the dropped `break`'s effect - left unconditional, it
silently overwrote the just-set victory sequence back to 9 (the damage-
report screen) every single round, so the fight could never end and the
planet's own persisted defense value kept drifting further negative
forever, exactly matching the live report. Fixed by nesting the trailing
two lines inside the `else`, matching real upstream's own early-exit
control flow exactly. Rebuilt clean and confirmed fixed via the user's
own live play.

**A second, unrelated bug found immediately after** ("could you check why
with the star honor game in the menu the creator text is drawn too much
up compared to all other games", with a screenshot). Traced to a real
buffer overflow in the *shared* `menu.c`, not anything in
`gameStarHonor.c` itself: the credit-line buffer (`int[32] authorText`,
built via `strcpy("BY ")` + `strcat(author)`) is sized for "BY " plus a
28-character author name plus a null terminator - Star Honor's own real,
full credit (`"WENCESLAO VILLANUEVA JR / WUUFF"`, this project's own
"original author / porter" combined-credit convention, 31 characters)
needs 35 slots total, silently overflowing the 32-slot buffer by 3 and
corrupting `blockY` (the next local declared above it, never reassigned
afterward) - shifting the whole credit line upward instead of producing
visibly garbled text, which is what made it look like a StarHonor-
specific rendering bug rather than a shared-buffer one. Confirmed no
other game's credit line is anywhere close to overflowing (a project-wide
length sweep found the next-longest, Snake 5110's `"LADY AWESOME &
MAKERSQUIRREL"`, lands exactly at the 32-slot boundary with zero slack) -
Star Honor is the sole real trigger. Fixed by widening `authorText` to
`int[64]`, matching the sibling `labelText` buffer's own already-generous
size a few lines above it in the same function, rather than bumped to the
bare minimum. Rebuilt clean; confirmed fixed via both a Puppeteer
screenshot and the user's own live check.

**A direct follow-up request** ("can the star honor credit line contain a
\n somewhere in the middle as the text is rather long") added real
two-line support to `menu.c`'s own credit-line drawing, rather than just
leaving the single wide line the buffer fix above already made safe.
Confirmed first that this dialect's string literals genuinely support a
real `\n` escape (already proven by many existing `gbPrintString("\n...")`
calls project-wide) - but `print_at()` is a separate, lower-level Vircon32
BIOS text primitive, not this shim's own hand-rolled `gbPrintString()` (the
one actually documented to handle `'\n'` itself), so nothing guarantees it
also splits lines on an embedded `'\n'`. Rather than assume, `menu.c` now
scans `authorText` for one explicitly and, if found, builds two separate
single-line strings and issues two separate `print_at()` calls at two
rows - `blockHeight`/`blockY`'s own vertical-centering math now accounts
for a real 1-or-2 line count instead of always assuming one line. Every
other game's credit line (no `'\n'`) takes the untouched original
single-line path - confirmed via a direct Puppeteer regression screenshot
showing `CopterStrike`'s own credit line unchanged. Star Honor's own
author string in `menuGameList.c` now reads `"WENCESLAO VILLANUEVA JR
/\nWUUFF"`, splitting at the natural "original author / porter" boundary
this project's own combined-credit convention already implies. Confirmed
fixed via both a Puppeteer screenshot (two real, independently-centered
lines) and the user's own live check.

## Unfinished games marked with reddish menu text, not hidden - a real `enabled` flag revised after a direct follow-up

Prompted directly ("can you comment out the adding of the games for
ralph, cruiser, dark shoot em up, save the princes, motorcorss and
mention in comment unifinished game (will this affect the tumbnails ?)").
The question itself was the right one to ask before touching anything:
traced `menu.c`'s own thumbnail lookup directly and confirmed
`md_drawGameThumbnail()` is keyed to raw registration index (the literal
order `addGame()` is called in `menuGameList.c`), not to name or
alphabetical position - `displayOrder[i] = i` initially, and
`selectedGameIndex = displayOrder[selection]` feeds straight into the
thumbnail draw call. Simply commenting out an `addGame()` call in the
*middle* of the list (these five are scattered from registration #71 of
92 through #93, not conveniently at the tail) would shift every later
call down by one registration slot each - but the thumbnail atlas cells
are physically pre-composited images baked to the *old* index positions,
so roughly 19 other already-shipped games (everything from Save
Princesse onward, including `CopterStrike` itself) would have silently
started showing the wrong thumbnail. Presented this concretely rather
than guessing at what the user wanted, and asked directly - the user
initially picked hiding the games outright via a new `bool enabled` field
(defaulted `true`, filtering `menu_buildDisplayOrder()`'s own output and
a new `displayCount` driving every navigation/pagination check instead of
`gameCount`) over accepting broken thumbnails or redoing ~19 thumbnail
compositions - `Save Princesse`, `MotoCross`, `Ralph`, `DarkShmup`,
`Cruiser`, then, via two more follow-ups in the same conversation, `No
Name Platform Game`/`MyRPG` and `PinBall` ("ball can get stuck") joined
the same treatment, all 8 wrapped as `disableGame( addGame(...) )`.

**A direct follow-up reversed the hide-outright decision**: ("actually do
show the disabled games in the menu but make their text color redish to
indicate it are unfinished games"). Reworked the same mechanism rather
than bolting a second one on: `enabled` became `unfinished` (semantics
flipped - `false` by default, still meaning "nothing special" either way,
just read oppositely), `disableGame()` became `markUnfinished()`,
`menu_buildDisplayOrder()`/`menu_update()` reverted to their original
unfiltered `gameCount`-driven behavior (the `displayCount` global and its
filtering removed entirely - every game shows, in its normal alphabetical
position, fully selectable and playable), and the per-entry label-drawing
loop in `menu_update()` now checks `games[displayOrder[idx]].unfinished`
and brackets that one `print_at()` call with `set_multiply_color(
color_red )` / `set_multiply_color( color_white )` - real, already-
available Vircon32 SDK primitives (`video.h`), with `color_white`
restored immediately after so no later draw call in the same frame (the
next list entry, the thumbnail, the credit line) inherits the wrong
color, per `VIRCON32_C_DIALECT.md`'s own §17.5 warning that `print_at()`'s
color is persistent GPU state, not reset per call. `menuGameList.c`'s own
comments were updated to match ("still shown/playable, red list text"
instead of "hidden from the menu"); the `addGame()` calls themselves
never changed shape across either version of the mechanism.

Verified via Puppeteer both times: after the hide-based version, paged
through the entire menu confirming none of the disabled titles appeared
and the list numbering had no gaps, plus a `B-Rally` thumbnail/credit
spot-check (registration index sitting well past several disabled
entries) proving no other game's thumbnail had shifted. After the switch
to red text, re-verified the full list is back to all 91 games with
continuous 1-91 numbering (11 pages), all 8 marked entries rendering in
`color_red` (`MOTOCROSS`/`NO NAME PLATFORM GAME`, `CRUISER`/`DARKSHMUP` on
adjacent pages, `PINBALL`/`RALPH`/`SAVE PRINCESSE` together on one page)
while every neighboring game stays white, the `>` selection cursor stays
white even when parked on a red entry, and thumbnails/credits for both
marked and unmarked games (`DarkShmup`'s own real gameplay screenshot
while selected, `Cruiser`'s own, `Maze`'s own) render exactly as before -
direct proof the color change is purely cosmetic and touches nothing else.
None of the 8 games were reverted/deleted the way `Pirates` was - their
real, complete ported source is still sitting in `src/games/`, `#include`d
in `main.c`, and fully playable; `markUnfinished()`'s own comment is the
only thing distinguishing them from any other shipped game today.

## Five more games ported (sixth discovery pass) - sourced from the sibling archive project, three candidates excluded directly by the user

Prompted directly ("exclude PAK-MAN_MAKERbuino from candidates and
dispatch agents to port the new staged games", followed by "also exclude
frogger and minsw-gameguino") after a fresh two-agent deep-search pass run
from the sibling `c:\github\gamebuino_classic_source_codes` archive
project surfaced 8 new real Gamebuino Classic games beyond every one of
this project's own five earlier discovery passes (that archive project's
own `CLAUDE.md` has the full per-entry verification writeup). Three
excluded on direct user instruction, reasons not further second-guessed:
`PAK-MAN_MAKERbuino` and `minesw-gameguino` are each plausibly genre-
redundant with an already-shipped game here (`gamePaqman.c`/
`gameMinesweeper.c`), and `Frogger_MAKERbuino` was excluded in the same
follow-up message as `minesw-gameguino` - all three noted in `more
games/DISCOVERED_GAMES.md` as likely-but-unconfirmed reasoning, not
invented justification.

The remaining 5 were staged into `more games/` (copied from the archive
project) and ported using this project's own established parallel-
isolated-copy agent workflow - five agents, one game each, each given the
full project context plus per-game risk flags worked out before dispatch
rather than left to rediscover:

- **`gemgem-gamebuino`** (Tnxec2) - a Bejeweled-style match-3 with a real
  EEPROM save/pause. Ported with a genuine fresh-cell sentinel built from
  upstream's own real 20-byte magic-string check (`"GEMGEM GAMEBUINO V1"`)
  - a factory-erased card reads `0xFF` and mismatches the very first
  character, so upstream's own real validation already behaves exactly
  like this project's own established sentinel pattern without needing
  translation. The struct-of-fields EEPROM layout was written out as
  explicit named byte offsets rather than a literal struct copy, since
  this dialect's `sizeof` counts words, not bytes.
- **`xonix-gamebuino`** (Tnxec2) - a real Xonix clone (claim territory by
  enclosing it with a trail while dodging roaming enemies). Confirmed its
  own `PrintUtils.ino` helper file is the same one already shipped inside
  `gameSnakeClassic.c` (Tnxec2's own reusable popup/text helper, shared
  verbatim across his own titles) - only the one real piece of it Xonix
  actually calls was ported, the rest correctly left out as genuinely dead
  code upstream itself never reaches either.
- **`DarkTower`** (Marcus Hutchings, GPLv3) - a real text-based adventure,
  the first game this project has ported built on upstream's own hand-
  rolled polymorphism (a value-typed `event` "class" whose seven
  "virtuals" are plain function-pointer members, deliberately avoiding a
  real vtable to save AVR SRAM) - flattened into a tagged struct plus
  free functions dispatching on a `kind` field, the same shape every
  other class-based port here already uses. Two genuine real-hardware
  8-bit-wraparound dependencies were found and preserved deliberately
  (not just the usual "narrow type audited" pass): a layout offset that
  *relies on* underflowing back around on a short message, and an event-
  stack index whose real AVR `uint8_t` wrap (`0→255→1`) is load-bearing
  for the very first "return to previous event" - a plain `int` would
  have produced a genuine out-of-bounds index instead.
- **`another2048`** (grafMakulaDer2te) - confirmed and ported as a
  genuinely distinct second 2048-style game, shipped side by side with
  the already-existing `game2048.c` the same way this project already
  ships two unrelated Snake games and two unrelated Asteroids clones.
  Real, documented differences: no tile sprites (plain ruled grid + text),
  score counts spawns not merges, a fixed (not random) opening position,
  win at 16384 not 2048, no lose condition at all (a real, upstream-
  documented design choice - a stuck board just stops responding), and
  tiles can chain-merge more than once in a single move. Registered as
  "ANOTHER 2048" specifically to stay visually distinct from the existing
  plain "2048" entry in the menu.
- **`gamebuino-community-rpg`** (Sorunome) - the highest-risk candidate in
  this batch, flagged for investigation before porting: real upstream
  reads two files, `DATA.DAT`/`SOUND.DAT`, off a real SD card at runtime.
  Investigated and resolved differently for each: `DATA.DAT` (7,873
  bytes) is genuine build-time-authored, portable world content (sprites/
  tilemaps/scripts, produced by the author's own web-based level editor
  and fetched at build time) - embedded directly as a byte array and
  verified byte-for-byte round-trip identical to the real file, the SD
  read itself being nothing more than how a real, flash-starved AVR
  reached content that didn't fit in its own 32KB. `SOUND.DAT`, by
  contrast, is genuinely not portable at all: two tracker songs upstream
  *writes into the MCU's own flash at runtime* via a raw
  `write_flash_page()` call at a hardcoded post-sketch address - a self-
  programming bootloader hack this platform has no analogue for, and one
  that upstream's own real `gb.sound.playTrack(0)` call site doesn't even
  match the stock two-argument library signature for (a real, undocumented
  dependency on a modified Gamebuino library not present in the repo).
  Dropped entirely; the game makes no other sound call, so nothing else
  was lost. Confirmed no EEPROM/save exists at all (dead `#include` only).
  **A second real modified-library dependency was found and resolved with
  a documented, reasoned deviation**: every real upstream button check
  uses `gb.buttons.pressed()` (real hardware's own edge-triggered API),
  but ported literally that way the game is provably unplayable - the
  real movement/friction/attack-charge formulas only make sense against a
  continuously-held read (confirmed by tracing the actual velocity-clamp
  and surface-friction constants through both interpretations). Ported
  using `gbHeld()` instead, with the reasoning stated directly in the
  file's own header rather than silently deviating from a literal
  translation. **Recommended by the porting agent, and accepted, as a
  `markUnfinished()` candidate**: genuinely unfinished even by this
  project's own already-generous bar - lorem-ipsum placeholder dialogue,
  no objective, no score, no ending, and two of the game's own three real
  world areas are unreachable in the shipped data at all.

**Menu-index navigation gotcha hit again during verification, same root
cause as batch 2's own documented lesson**: Puppeteer scripts computed
each game's own alphabetical menu position once per script run rather
than recomputing after each new game was integrated - since the menu
sorts alphabetically and each newly-added game can shift every later
title's own position by one, a stale position (computed before a later
game in the same batch was added) silently lands the cursor one or more
entries short. Caught and corrected each time by re-deriving the position
fresh via a direct alphabetical sort of the current `menuGameList.c`
rather than reusing an earlier count.

All 5 verified crash-free and genuinely playable via Puppeteer (real
title screens where upstream has them, real gameplay reached, zero
`pageerror`s), each with a real gameplay thumbnail and screenshot.
`MAX_GAMES` raised 96->104 and `THUMBNAIL_COUNT` raised 91->96 across the
batch - the fourth thumbnail texture (added during the CopterStrike/
cruiser/BigBlackBox pass) had ample spare capacity, no new texture needed.
**Ninety-six games now shipped.**

## A second `info` credit-line field added to the menu, generalizing Star Honor's own two-line hack

Prompted directly ("create a second 'info' parmeter besides author for
addgame calls and for the game where we had added a new line split on
the author add the 2nd line part in the new info field... write reason
like just 'Unifinshed game', 'Ball can get stuck'... display that info
entry below author if it's tagged unfinished draw it in red else white").
Generalized rather than special-cased: `addGame()` gained a real sixth
parameter, `int* info` (between `author` and `init`), and `struct Game`
(`menu.h`) a matching `info` field - a second, independent credit-line
shown directly below "BY \<author\>", drawn in `color_white` normally and
`color_red` whenever that game is also `markUnfinished()`-flagged (same
persistent-GPU-state save/restore discipline as the existing red list-
text code). Star Honor's own real `'\n'`-embedded-in-author-string hack
(see "A real Spin Spin Spinbuino porting bug..." - no, see the credit-
line-splitting section above) was retired in favor of this: its author
string reverted to a plain `"WENCESLAO VILLANUEVA JR"`, with `"WUUFF"`
(the porter credit) now passed as a genuine `info` argument instead of a
manually-scanned-for `'\n'` - the same mechanism now serves both real use
cases (a porter-credit continuation, or an unfinished-game reason) rather
than one hard-coded to only the credit-line case.

All ~97 existing `addGame()` call sites in `menuGameList.c` were
mechanically updated (`sed` inserting a `NULL,` right after every
author string, verified against the real call count before any manual
edit) with the 9 already-`markUnfinished()`-flagged games then hand-
given a real, specific reason instead of `NULL`: `SAVE PRINCESSE`/
`DARKSHMUP`/`CRUISER` -> `"No Dying / Game Over"` (confirmed directly
against each one's own already-documented real upstream finding - no
death/game-over/win-lose condition exists in any of the three);
`NO NAME PLATFORM GAME` -> `"Engine Demo"`; `PINBALL` -> `"Ball can get
stuck"`; `MOTOCROSS` -> `"No Dying / Game Over"`. **`RALPH` and `MYRPG`
were initially left with the generic `"Unfinished game"` fallback, then
refined on a direct follow-up** ("for Ralph write 'Non interactive' and
for 'MyRPG' the more suitable engine demo or so") - `RALPH` ->
`"Non interactive"` (matching its own already-documented real limitation:
no player-driven input at all beyond a debug-style camera pan), `MYRPG`
-> `"Engine Demo"`. The `MyRPG` relabeling followed a direct verification
request ("check if myrpg is really unifinished at all") - re-read its own
real upstream source directly rather than trusting the earlier
classification: zero matches for score/enemy/battle/win/lose/game-over/
objective/goal/item/treasure anywhere across all 516 real lines,
confirming it's genuinely just a walkable-overworld-plus-two-building-
interiors demo with nothing to actually do, the same real category as
`NO NAME PLATFORM GAME` - not a stale/mistaken carry-over flag.

Verified via Puppeteer across all three real cases: a normal finished
game (`CopterStrike`) shows no second line at all; an unfinished game
(`PinBall`) shows "Ball can get stuck" in red under "BY CLEMENT83"; a
finished game with a porter-credit continuation (`Star Honor`) shows
"WUUFF" in white under "BY WENCESLAO VILLANUEVA JR" - confirming the
color logic follows the `unfinished` flag alone, independent of what the
info text actually says. A real, repeated Puppeteer-navigation gotcha hit
throughout this verification pass, worth remembering: rapid synthetic
`ArrowDown` keypresses (even 130ms held / 220ms apart) are consistently
dropped by 1-2 over a ~35-60 press run in this headless/SwiftShader
environment - not a menu bug, confirmed by the selection always landing
short, never past, the intended target, and by the fact that a slow
one-at-a-time "nudge and screenshot" loop from a known-close position
always lands correctly. Prefer that nudge approach over trusting a single
large up-front press count whenever a Puppeteer script needs to land on
a specific, far-down menu entry precisely.

## Fifteen ported - a real fourth game found under an author already mined twice, by fixing a real tool gap first

Prompted directly ("seems we have another game https://github.com/
Tnxec2/fifteen check all tnxec2 repo's not just 1st page or repo name
listings", then "grab fifteen & port it"). See "A real methodology gap
found..." in the sibling `gamebuino_classic_source_codes` project's own
`CLAUDE.md` for the full root-cause writeup (a summarized web-fetch tool
had been silently truncating this author's own 34-repo GitHub listing
across multiple calls, each returning a different incomplete subset,
none of which ever included `xonix-gamebuino` even though that repo is
definitely real and already shipped here - fixed by pulling the raw
GitHub API JSON directly instead of trusting a summarized fetch for
anything needing an exhaustive, exact enumeration). That full sweep found
one real, previously-missed fourth game from this same author (already
the source of `gemgem-gamebuino`/`xonix-gamebuino`/the excluded
`minesw-gameguino`) - `fifteen`, a classic sliding 15-puzzle, 450 lines,
real EEPROM save, no license specified - and ruled out a second candidate,
`tetrino`, as a false alarm: its own real title screen literally reads
`"Tetrino by Joff (STC)"`, confirming it's Tnxec2's own build of the
exact same game already shipped here as `gameTetrino.c` (J0FF), not an
independent codebase.

Ported with the prefix `ftn`, following the same real `GAME_ID`-struct
EEPROM idiom already proven in this author's own `gemgem-gamebuino` port
(a genuine 18-byte magic-string fresh-cell sentinel, `"FIFTEEN
GAMEBUINO"`, already sound against a factory-erased card without
needing translation). Upstream's three real blocking constructs
(`gb.titleScreen`, `gameMenu()`'s `while(1)`, `chooseMap()`'s `while(1)`)
became four explicit states - the level picker specifically needed a
real `ftnChooseMapDepth` counter to reproduce upstream's own genuine
*recursive* self-call when B/C is pressed inside it (confirming a size
there returns into the previous picker, not straight to the game/menu).
**One real, necessary Vircon32-forced deviation**: a genuinely reachable
`dimension == 0` state (pressing "Load saved" with no valid save on the
card) hits `LCDWIDTH / dimension` in `drawField()` - real AVR silently
returns garbage and carries on, but this platform hard-traps on integer
divide-by-zero (the same class of crash already found and fixed in
`cruiser`), so the divisor is clamped to 1. The visible result is
unchanged either way: with `dimension` genuinely 0 the tile-grid loop
never runs, so the same lone border rectangle real hardware draws is all
that appears - confirmed directly via Puppeteer against a fresh card
with no save (exactly this state, reached by simply pressing A on
"Load saved" as the menu's own real default selection).

Real upstream quirks preserved deliberately (all in the file's own
header comment): `isSolvable()` is not the real 15-puzzle parity rule at
all (ignores the blank's own row, never examines the last cell) - a deal
can genuinely come out unsolvable, Button B just reshuffles, a real
design choice not fixed into "always solvable"; the local popup's own
`uint8_t` underflow hides its last 12 ticks instead of sliding (same
class of bug already documented for `gemgem-gamebuino`'s own popup);
`chooseMap()`'s own two range clamps run unconditionally every tick
outside their own `if(pressed)` bodies, which is load-bearing (it snaps
a genuinely-zero dimension to 5 after one nameless frame).

Verified via Puppeteer: real title screen (a real 64x30 logo bitmap),
the Load-saved/New-Game menu (defaulting to "Load saved," reaching the
real div-by-zero-guarded empty-board state exactly as predicted before
ever being tested), and genuine gameplay reached via New Game -> size
picker -> a real 3x3 sliding board with correct tile movement, zero
`pageerror`s throughout. `THUMBNAIL_COUNT` raised 96->97 (still well
within the fourth thumbnail texture's own spare capacity).
**Ninety-seven games now shipped.**

## Mole Control and Aerial-Assault ported - two more real games found in the sibling archive project, a real Controls-menu bug fixed on direct request, and a Puppeteer navigation-flakiness fix

Prompted directly ("check that repo again and port the 2 newly found
games", clarified sharply as "the gamebuino sources repo dumbass" once an
initial reply mistakenly re-checked Tnxec2's live GitHub account instead)
- re-diffing the sibling `gamebuino_classic_source_codes` archive
project's own `games/`/`tools/` trees against this project's own `more
games/` found two real, not-yet-staged directories: `mole-control`
(grafMakulaDer2te, the same real author already shipped here as `ANOTHER
2048`) and `Strike-Down` (SkylarHylar - whose own README titles the game
"Aerial-Assault", the real title this cartridge registers it under, not
the repo name). Both ported using the same parallel-isolated-copy agent
workflow established since batch 4.

**Mole Control** (`src/games/gameMoleControl.c`, prefix `mole`) - a
whack-a-mole game: a 3x3 grid of holes, a hammer cursor moved by the
D-pad and swung with Button A, moles spawning on a fixed timer that
speeds up every few points. Two real findings worth remembering:
- **A real, deliberately-preserved AVR-narrowing restart bug.** Upstream's
  own Button-C restart sets three real `int16_t` timing variables to
  `<duration> + millis()` instead of the plain duration - on real
  hardware this narrows modulo 65536, going enormous-and-positive for an
  early restart or wrapping negative for a later one (spawning a mole
  every tick and draining all lives in under a second). This dialect's
  always-32-bit `int` would never narrow the same way, so reproducing it
  needed an explicit `& 0xFFFF` then a signed reinterpretation of values
  ≥32768 - done on purpose so the restart is exactly as broken here as on
  a real cartridge, not accidentally fixed by this platform's wider `int`.
- **A real, byte-for-byte-provable performance rewrite.** Upstream's own
  `drawGamePad()` draws its entire 6-field score/level/lives readout
  *inside* its 3x3 hole loop - 9x redundant on real hardware, but a real
  ~120,000-instruction tax here (every call costs a flat overhead on this
  ISA - see the project's own earlier performance-pass section). Hoisted
  out of the loop; proven output-identical rather than just probably
  equivalent, since every draw in this game is a transparent BLACK-only
  OR into the framebuffer with no `setColor()` call anywhere outside the
  game-over box, making draw order and repetition both provably
  irrelevant to the final pixels.

**Aerial-Assault** (`src/games/gameAerialAssault.c`, prefix `aer`) - a
real Joust clone: flap upward to gain height, and a plain Y-coordinate
comparison at collision time decides who wins (higher player kills the
enemy into a collectible egg; otherwise the player dies). The porting
agent documented 12 real preserved upstream bugs/quirks in the file's own
header comment (a `case 2:` Controls-menu fallthrough with no `break;`,
four `wait==0||10`-style conditions that parse as always-true and
silently defeat their own intended throttles, an inverted/misplaced lives
readout, mismatched flip-vs-collision sprite handling, and others) - see
the file itself for the complete list.

**One of those 12 was fixed anyway, on direct live-testing request**
("in areal assualt press B on controls screen lands me on petrodactyl
option instead of options", then "fix the bug about controls ffs" once
told it was a confirmed, faithfully-preserved real upstream bug). This is
a deliberate, requested exception to this project's own normal "preserve
real upstream bugs unless they'd crash/hang/softlock" bar - the user
judged this particular one a genuine live-testing annoyance worth fixing
despite being real, verified original-game behavior, not a porting
mistake. `aerUpdateControls()`'s own B-handler now calls `aerBeginMenu()`
directly instead of falling through to the Options state, matching every
other sub-screen's own B-handler shape (`aerUpdateStatus()` right above
it) - the file's own header comment for quirk #1 was updated to say so
explicitly, flagged as the one item in that list that is NOT reproduced
bit-for-bit, unlike the other 11.

**Two real integration mistakes were made and self-caught while wiring
both games in, worth recording since they're the kind of off-by-one this
project has hit before**: registration-index arithmetic was done from a
stale mental tally (assuming Mole Control was the 98th registered game
when it was actually the 97th, 0-indexed), which both set
`THUMBNAIL_COUNT` one too high (100 instead of the real 99) and placed
Mole Control's own thumbnail cell one column off in `thumbnails4.png`
(painted at local index 10 instead of its real 9) - caught by a direct
user report ("i only see games till 99 in the meny") rather than internal
review, traced by recounting the real `addGame()` line total
(`grep -c` against `menuGameList.c`) instead of trusting an earlier
miscount, and fixed by correcting `THUMBNAIL_COUNT` back to 99 and
re-compositing Mole Control's cell at its real local index
(`97 - THUMBNAIL_SPLIT3(88) = 9`, col 1 row 2), with Aerial-Assault
correctly taking the freed local index 10 (col 2 row 2) as the genuine
99th and last game.

**A real Puppeteer methodology improvement, worth keeping for future
verification sessions**: the already-documented "rapid ArrowDown presses
silently drop" flakiness (see the `info`-field section above) kept
recurring even with the established "navigate close, then nudge
one-at-a-time" workaround, because each nudge round restarted a fresh
headless Chrome + fresh cold ROM load, so a press count confirmed correct
in one run was not reproducible in the next (drops are non-deterministic
per run, not per navigation path). Fixed by keeping ONE headless Chrome
alive across an entire verification session instead
(`--remote-debugging-port`, `puppeteer.connect({browserURL})` from small
single-purpose scripts that each press one key or wait and screenshot,
then disconnect without closing the browser) - this lets each keypress be
individually confirmed via screenshot before sending the next, with the
game state genuinely persisting between script invocations rather than
resetting. Two real gotchas hit while building this: `browser.pages()`
can return more than one tab (a stray `about:blank` ranked last, so
`pages[pages.length-1]` silently grabbed the wrong page - fixed by
selecting `pages.find(p => p.url().includes('index.html'))` instead), and
a reconnected page's viewport does not persist from an earlier connection
(each new `puppeteer.connect()` needs its own explicit
`page.setViewport({width:640,height:360})` call, confirmed by a screenshot
coming back 800x600 instead of the expected 640x360, which silently threw
off the fixed-pixel thumbnail-crop coordinates used everywhere else in
this project until caught).

Verified via this new persistent-session technique: Mole Control's real
title screen, gameplay (hammer cursor, spawning moles, live HUD), and its
corrected thumbnail cell in the actual in-game menu (not just the atlas
file); Aerial-Assault's real "Strike Down" title bitmap, its real 5-item
`gb.menu()`, live gameplay (player, platforms, an airborne enemy) for its
own thumbnail, and the Controls-menu fix landing on the main menu instead
of Options. Both registered in `menuGameList.c`
(`MOLE CONTROL`/`GRAFMAKULADER2TE`, `AERIAL-ASSAULT`/`SKYLARHYLAR`, both
`info = NULL`, neither marked unfinished), with real gameplay thumbnails
composited into `thumbnails4.png` and screenshots saved to
`metadata/screenshots/`. `MAX_GAMES` raised from 104 to 112 for the usual
"modest headroom past the real current total" reason (99 real games
registered after this pair, was down to its last 4 spare slots).
**Ninety-nine games now shipped** - this exhausts every currently-known
staged candidate in both this project's own `more games/` and the sibling
archive project as of this session.

## A full project-wide audit of preserved gameplay bugs, and a batch fix of the worst offenders - `BUGS.md` added

Prompted directly ("for all games list every not fixed upstream bug we
deliberately left in that could affect player experiance or gameplay in a
negative way"). This project's own history had already documented dozens
of individually-found preserved bugs across many games' own header
comments, but no single place listed them all together - answering the
question properly meant a real, systematic sweep of all 99 games, not a
recall of what earlier sessions happened to have already written up.
Dispatched 4 parallel read-only agents (the same `Explore` agent type this
project uses for broad-fan-out reads), each assigned ~25 game files, with
explicit inclusion/exclusion criteria (unfair hitboxes, broken scoring,
non-functional controls/options, AI that malfunctions, unwinnable/dead-end
states, misleading readouts - excluding purely cosmetic issues, dead code
with zero effect, dropped hardware/multiplayer features, sound
approximations, and anything already fixed during porting). Compiled their
combined findings into one alphabetized list covering roughly 60 games.

**A direct follow-up then asked to fix a specific, hand-picked subset of
that list** - 14 games, most fixed in full, three (World's Hardest Game,
Descent Into Hell, Catcher) with an explicit "only this one bug" scope
against a game that had multiple findings, and Aerial-Assault's own
already-fixed Controls bug correctly excluded from the list (that fix
predates this session). All 14 fixed directly in their own game files,
each with its own header comment updated in place from "preserved
verbatim" to "FIXED, NOT PRESERVED" plus a description of the real
mechanism replaced - not just patched silently:

- **Digger**: the real Hard Mode "world renders invisible from the second
  frame on" color-persistence bug (`gbSetColor(1)` added before the sprite
  loop), and `GAMEOVER` now calls `digInitWorld()` so a new life starts on
  a fresh board instead of the previous run's half-dug one.
- **Skibuino**: quitting to the title screen from the pause menu now saves
  a genuinely higher distance before clearing state (previously only a
  crash/game-over path saved).
- **Firemen**: the same-tick Button-C double-read that skipped the Game
  Over screen entirely is now a genuine deferred state transition instead
  of a synchronous same-tick call.
- **Blockdude**: a fresh-EEPROM 65535 sentinel is now normalized to 0
  before the "completed" check, and `doLift`/`liftBlock` are reset on the
  level auto-advance path (no more phantom carried block).
- **Blob Attack**: the pause screen's mispositioned "B to play" label is
  now explicitly centered under the icon, and the high score is now saved
  as a full EEPROM word (with a fresh-cell sentinel) instead of a single
  truncated byte.
- **Castle Defence** (4 bugs, the largest single-game fix in this batch):
  a case for exactly 1 HP plus a `<=0` (not `==0`) game-over check fixes a
  real permanent-negative-HP soft-lock; the Game Over slide animation now
  converges to 0 from either sign instead of only from a negative starting
  camera offset (a second real permanent soft-lock); the monster-toughness
  "floor" guard's operator-precedence bug (`!x <= 40` instead of `x > 40`)
  is fixed so toughness variance genuinely levels off; the rifle shop's
  life-recovery text is now correctly nested so it no longer shows
  alongside a stray "Impossible!" for a player who doesn't own the rifle.
- **CrazyTown**: the `abs(player.v<0)` comparison-instead-of-`abs(value)`
  typo that permanently zeroed the distance-driven accumulator (and with
  it, the entire drive-efficiency scoring bonus) is fixed to `fabs()`.
- **World's Hardest Game**: *only* the fresh-EEPROM-byte "every level
  shows completed" bug was fixed, per the user's own explicit scope - the
  separate "tries counter doesn't reset on auto-advance" bug from the same
  game was deliberately left preserved.
- **Descent Into Hell**: *only* the monster-spawn retry loop's
  assignment-instead-of-comparison bug (`while(place_ok=false)`) was
  fixed, per the user's own explicit scope - the other three preserved
  Descent bugs (HP-scaling OOB write, Map-screen restart skipping a fresh
  dungeon, invuln-flicker draw color) were left alone.
- **Catcher**: *only* the Button-C stuck-restart soft-lock risk was fixed
  (a `catchPadHit = 0` reset added to `catchResetGame()`, the exact
  defensive reset this file's own header comment had already flagged as
  missing) - the diagonal-input and shifted-sprite-collision bugs from the
  same game were left preserved, per the user's own explicit scope.
- **Jezzball**: the EEPROM "reset magic bytes" loop's hardcoded address-0
  typo (writing `magic[j]` to address `0` three times instead of address
  `j`) is fixed, so a saved high score can now genuinely survive a reload
  for the first time.
- **Maruino**: the "enter a code" screen's Button C now genuinely returns
  to the menu, matching what its own on-screen text has always claimed.
- **AsteroidRipper**: *only* the pause-freezes-the-whole-screen-with-no-
  indication bug was fixed (a centered "PAUSED" label added) - the bullet-
  direction-offset and level-complete-after-death-check bugs from the same
  game were left preserved, per the user's own explicit scope.
- **Bang! Bang!**: the pause screen's leftover "TAQUIN" title (a genuine
  upstream copy-paste bug from the same author's other game) now reads
  "BANG! BANG!" instead.

Every fix stayed narrowly scoped to exactly what was asked - several games
in this batch had multiple documented preserved bugs, and only the
specifically-named one(s) were touched, leaving the rest exactly as
before (still documented as preserved, both in `BUGS.md` and each file's
own header comment).

**`BUGS.md` added at the project root** - a new top-level reference,
alongside `README.md`/`CLAUDE.md`/`VIRCON32_C_DIALECT.md`, listing every
currently-known preserved gameplay bug (grouped by game, reconstructed
from the original 4-agent sweep minus the 14 games' worth of entries fixed
this same session) and every fixed bug (with what changed and why),
prompted by a direct follow-up request once the fixes themselves were
done. `README.md`'s own "Real upstream bugs are preserved, not fixed"
bullet was reworded to reflect the new "preserved by default, with a
smaller fixed-on-request set" reality and now links to it directly.

Rebuilt clean after all 14 fixes (`BUILD SUCCESSFUL`, no compile errors) -
per a direct instruction, this batch was not additionally smoke-tested via
Puppeteer before considering it done.

## Three more bug fixes, a real emulator-vs-source discrepancy investigated (and left open), and a shared float-printing primitive promoted

Direct follow-ups after the `BUGS.md` batch above, working from the same
"fix specific named bugs, verify the rest stays untouched" discipline.

**ShootBuino** (`gameShootBuino.c`): two more bugs fixed on direct
request, both already documented as preserved in `BUGS.md` at the time -
`player_life` is now reset in `sbuinoInitGame()` (previously only ever
initialized once, so dying once meant every subsequent restart began at 0
life and died on the first hit for the rest of the session), and invader
bullets (previously drawn via a real upstream `fillRect(...,1,-2)` -
a negative height that draws nothing at all) now draw a real 2px vertical
bullet ending at their own logical Y position.

**Super Crate Buino** (`gameSuperCrateBuino.c`): the map-select "LOCKED!"
blink's own color-leak bug (real upstream sets WHITE for the blink text
and never resets to BLACK, so every other element that screen draws -
header/score/map-preview/border - was theorized to draw invisibly after
viewing even one locked map) was fixed with a color reset right after
drawing "LOCKED!". **Then a real, unresolved discrepancy surfaced**: asked
directly whether this ever actually happened on real hardware, the user
checked two independent real tools (Simbuino, then `gbsim` - a cycle-
accurate AVR simulator running the literal compiled library code, tested
with a freshly-cleared EEPROM) against real upstream's own unmodified
source, and neither showed the bug at all. An exhaustive re-trace of every
real `Display`-class function that touches `color` (`begin`/`clear`/
`update`/`drawPixel`/`drawChar`/`drawBitmap`/`fillRect`/the 4-arg rotated-
bitmap overload's own NOROT/NOFLIP fast path/`Gamebuino::update()`) found
no reset mechanism anywhere that would explain it. The fix was kept (the
user's own explicit call, `"leave it open, don't revert yet"`) but
documented as a genuine open discrepancy rather than a settled bug, in
both the file's own header comment (quirk 10) and `BUGS.md`.

**The identical open-discrepancy shape then recurred independently in
Digger**, prompted by the user asking how to reach Hard Mode and then
reporting the exact same "never happened on real hardware" result for the
already-shipped Hard-Mode-invisible-world fix. This became a much longer,
multi-round investigation, each round driven by a specific, sharp
follow-up question from the user rather than open-ended digging:
- **"What if `gb.menu()` resets color at the end of the frame?"** - traced
  the real `Gamebuino::menu()` widget directly and found a genuine,
  previously-undocumented fact: its own per-tick draw body ends with
  `setColor(BLACK)` as its literal last action, every tick it's open
  (including its own multi-tick slide-out exit animation) - so real
  `color` is provably BLACK by the time it returns control to the game,
  regardless of what it was set to before the menu opened. Confirmed this
  port's own hand-rolled `digDrawMenuList()` already provides the
  equivalent guarantee via a different but equally valid ordering (resets
  at the *start* of each menu-draw tick rather than the end).
- **"What if we explicitly `gbSetColor(1)` at the end of menu usage?"** -
  tested directly rather than reasoned about: temporarily removed the real
  fix and added exactly that instead. The bug still occurred identically,
  screenshotted directly - ruling out "leftover color from before the
  menu" as the mechanism entirely. The corruption regenerates itself fresh
  every second Hard Mode tick from `drawWorld()`'s own trailing HUD code
  alone, with no dependency on prior state - confirming the fix has to
  live inside `drawWorld()` itself, which is where it already was.
- **"Could something be corrupting memory?"** - a genuinely different
  class of hypothesis, taken seriously given this project's own real
  precedent (Tron's comma-separated-struct-globals bug corrupting
  `gbFrameBuffer` itself). Checked the actual compiled global memory
  layout directly (`obj/main.asm`) rather than just source: `gbColor`
  (word 2825) sits **immediately adjacent** to the end of
  `gbFrameBuffer[504]` (words 2321-2824) with zero padding - a real,
  separately-noteworthy structural fragility (any one-past-the-end write
  to `gbFrameBuffer[]` anywhere in the whole codebase would silently
  corrupt `gbColor`). Audited every array-indexing site in `gameDigger.c`
  itself for a genuine out-of-bounds write (the already-guarded `y+2`
  CHANGER case, the `x±1` falling-rock diagonal checks, the 8-neighbor
  `digMonsterDie()` reads) and found nothing unguarded; checked
  `gbDrawBitmapRotated()`'s own more complex indexing (correct, and not
  even called by Digger); checked whether the confirm-quit dialog/pixel-
  grid overlay (drawn from `portVircon32.c`, outside the game's own code)
  could interleave a stray write - both draw via raw Vircon32 GPU calls
  that bypass `gbFrameBuffer[]` entirely, ruled out.
- **Decisive test**: added a temporary diagnostic overlay printing
  `gbColor`'s own raw entry-time value on screen every tick (with the real
  fix removed again) - caught and fixed a real mistake in the diagnostic
  itself along the way (its own `gbSetColor()` call, used to force the
  diagnostic text visible, was accidentally overwriting the very state
  being measured, invalidating the first run - fixed by saving/restoring
  `gbColor`/`gbBgColor` around the diagnostic print). The corrected result
  read exactly `C:0` on the corrupted tick - one of the two legitimate
  values this code ever uses, not garbage. **Memory corruption is
  conclusively ruled out** - the mechanism inside this port is exactly the
  ordinary, already-traced color-leak logic, nothing more exotic. All
  temporary diagnostic code was then removed and the real fix restored;
  both `gameDigger.c`'s header comment and `BUGS.md` document the full
  investigation and its decisive conclusion. The one thing that remains
  genuinely open, after all of this, is narrower than where it started:
  *why does real hardware's identical source-level logic not reach the
  same, otherwise-fully-explained state* - a real-execution-timing
  question, not a memory-safety one.

**A real, independently-confirmed instance of the same general "color
leak" bug class, found on direct request to check for similar cases**:
Pong 2017's own already-documented preserved bug (real upstream's
`drawBackground()` sets GRAY for the center net and never resets to BLACK,
so every other HUD element that function draws - player names, life
gauges, round-win bars, trick icons - renders in GRAY too) was
independently confirmed as genuinely real, not just theorized, when the
user provided a live screenshot from an actual emulator showing exactly
that dithered-gray HUD text with real upstream's own unmodified source.
Unlike the Super Crate Buino/Digger cases, this one's `BUGS.md` entry and
the file's own header comment could state plainly that it's confirmed,
not open - left preserved, not fixed, since it's genuine shipped upstream
behavior.

**A new shared shim primitive promoted: `gbPrintFloat()`**, prompted by a
live user report with a real emulator screenshot ("in agaruino the
'taille' displayed floats yet in our port i only see integer numbers").
Traced to a real, genuine bug: real upstream's own `taille` (size) field
is a genuine `float`, and `afficher()` calls `gb.display.println()`
directly on it - real Arduino's own default float printing shows 2
decimal places (e.g. "Taille : 3.75"), but this shim had no float-print
primitive at all, so the original port cast the value to `int` before
printing, silently truncating the entire fractional part. Initially fixed
with a game-local `agarPrintFloat()` helper - then, asked directly "why
did we not implement it directly in our shim? other games could have
similar issues," promoted properly instead: a real, project-wide sweep
(every plain `float` local, every `float` struct field, and every
member-access `gbPrintNumber()` call site across all 99 games, each
struct definition checked directly rather than assumed) found exactly one
other real instance - `gameMotoCross.c`'s own real upstream debug speed
readout, which its own porting agent had *already* correctly identified
and documented as hitting the identical "no float-print primitive exists"
wall, months earlier, without it ever being acted on. Two real games
hitting the same wall crossed this project's own established "promote
once multiple games need it" bar, so `gbPrintFloat()` (a direct port of
real Arduino's own `Print::printFloat()` rounding/digit-extraction
algorithm) was added to the shared shim (`gamebuinoShim.h`/`.c`), and both
Agaruino and MotoCross were switched to call it - MotoCross's own real
fractional speed readout is now genuinely restored, not just documented as
missing. `gameHexagon.c`'s own four `gbPrintNumber((int)(...))`-shaped
call sites were checked directly during the sweep and confirmed to be a
different, already-correct hand-rolled decimal-digit-extraction pattern,
not the same bug.

## Bang! Bang! marked unfinished (player 2 never shoots), and a real BlocksBuino border-rendering bug found and fixed

Two more direct follow-ups. First, a factual question ("in bang bang does
the 2nd player ever shoot") - traced directly against the real source:
`gamestate` is never assigned `PLAYER2AIMING`/`PLAYER2SHOOTING` anywhere in
the file, so player 2's own cannon is real, permanently dead code, not a
subtle AI/timing gap. Marked unfinished in the cartridge's own menu (red
list text, `info = "Player 2 never shoots"`) via `markUnfinished()`, per
direct request - the game's own already-documented preserved-bug entry in
`BUGS.md` was updated to note this rather than left silently stale.

Second, a live user report with two comparison screenshots ("in
blocksbuino the side level lines are never drawn with our port") -
`gameBlocksBuino.c` already had a header comment (from an earlier porting
session) claiming the playfield's own double-line side border draws
nothing at all, "on real hardware or here," reasoning from real upstream's
own literal `drawRect(field_x,field_y,field_w,field_h)` math: `field_y=49`
sits one row below the real 48px-tall screen, and `field_h=-49` is
negative, so real `Display::drawFastVLine()`'s own `for(i=0;i<h;i++)` loop
never executes and both horizontal edges land off-screen too - a claim
re-verified directly against the real `Display.cpp` source and found
mathematically correct on its own terms. That conclusion was nonetheless
wrong: a real screenshot bundled in upstream's own repo
(`pictures/BlocksBuino.png`) and the user's own tested real emulator both
clearly show a full-height double-line border down each side of the field
- no `DISPLAY_ROT` override, no alternate git history (the repo's own
shallow clone has exactly one commit), and no viable alternate code path
(the file's only other candidate, a commented-out `drawLine(field_x2,...)`
pair, references a variable that's never declared anywhere and couldn't
compile if uncommented) could explain the gap. Given unambiguous real-
world ground truth from two independent sources and no way to reconcile it
with the literal upstream math, `blkDrawField()` was fixed pragmatically
rather than left "correctly" broken: the 4 real vertical edges are now
drawn directly (bypassing `gbDrawRect()`, which would also draw the 2
horizontal edges) by reading `field_y`/`field_h` as "anchored at the
bottom edge, height extending upward" - the real top of each vertical line
is `field_y+field_h` (0), the real length is `-field_h` (49) - a
normalization real `Display::drawFastVLine()` itself never performs, but
the only geometry that reproduces the observed border. A first attempt
went through `gbDrawRect()` for both rects (applying the same
normalization to the horizontal edges too), which drew an unwanted extra
line across the very top of the field that neither real source showed -
caught via a second direct live user report and corrected to the
4-VLine-only form, which finally matched exactly.

**A real, previously-undiscovered stale-ROM trap in this project's own
Puppeteer verification setup was found and fixed while re-testing this
fix**: the long-lived `python -m http.server 8991` process this session's
screenshots were being captured against (and, per the project's own
established "keep one headless Chrome alive across a verification
session" technique, had been running since a much earlier point in this
session) was serving from `c:/github/WebEmulator/DesktopEmulator/WebBuild`
- a real, separate emulator-build project outside this repository
entirely, whose own `game.v32` was a stale copy dating from several days
before this session's own work even started. Every rebuild this session
had produced was landing correctly in this project's own `bin/`, but nothing
was copying it into that external serving directory, so every Puppeteer
capture was silently exercising old, unrelated code no matter how many
times the project itself was rebuilt - explaining why an initial capture
of the corrected border showed no change at all, contradicting the user's
own live (differently-served) test showing it fixed. Fixed by copying the
freshly built `bin/gamebuino_classic.v32` to that directory's own
`game.v32` and pointing the capture scripts at its real entry file
(`Vircon32Web.html`, not `index.html` - the two are different emulator
web-shell builds with different element/file layouts, and no `index.html`
matching this project's own established capture-script assumptions
actually exists anywhere in `c:/github/WebEmulator`). See
[[webemu-server-serves-external-stale-rom]] for the durable fact worth
remembering here.

Both fixes verified via Puppeteer against the corrected setup: Bang! Bang!
shows in red at its correct alphabetical menu position with the new info
line; BlocksBuino's in-game border now renders as two vertical lines close
together on each side of the field, full height, no horizontal cap,
matching both the real upstream screenshot and the user's own tested
emulator exactly. BlocksBuino's own screenshot
(`metadata/screenshots/BLOCKSBUINO.png`) and thumbnail-atlas cell
(`assets/thumbnails2.png`, registration index 33, local cell 1) were both
recaptured against real gameplay showing the corrected border and
recomposited/rebuilt.

## A real, project-wide missing primitive found: `gbUpdate()` never reproduced real hardware's own per-frame color reset - Castle Defence, Digger, Super Crate Buino, and Solitaire all affected

Prompted by a direct live report with a screenshot ("in castle defense the
upstream 'new high score' text seemed to be drawn with a white background
... i don't see that white background on the text"). Initial source-level
tracing (real `Display::setColor()`/`drawChar()`, `Print::write()`, the
real AVR core's own `Print.cpp`, `Gamebuino.h`'s own `Display display;`
member) consistently indicated the game's title screen already normalizes
color to transparent, and no other upstream call site changes it - a real
trace, but an incomplete one, and the user correctly refused to accept it
as a reason not to fix the reported symptom. Two direct screenshots (the
Game Over screen, and separately the "GO!!" pre-round text) confirmed the
same real, live discrepancy in both places, and every text draw sitting
over Castle Defence's own busy castle-wall/shop-staircase bitmap art
(`cdefUpdateReady()`'s "Ready..?"/"GO!!", `cdefLevelUp()`'s "LEVEL UP!"/
"DANGER", `cdefDisplayMessage()`'s shop price tags, and the whole Game
Over screen) was fixed with an explicit opaque-background call at each
site.

**The real root cause was then found directly, on a specific direct
question** ("in displaybattery it exlictly sets display.setColor(BLACK,
WHITE); again so normally that may reset it"): real `Gamebuino::update()`
(`Gamebuino.cpp`) calls `displayBattery()` automatically at the tail of
*every single frame*, and `displayBattery()` itself unconditionally calls
`display.setColor(BLACK, WHITE)` whenever `battery.thresholds[0]` is
nonzero - which it always is under normal settings (`ENABLE_BATTERY=1`,
`BAT_LVL_CRITIC=3500` by default, confirmed directly in `settings.c`/
`Gamebuino.cpp`). This is a real, automatic, every-frame color reset real
hardware has always had, called from inside the exact `update()` function
this shim's own `gbUpdate()` is a port of - and one every earlier trace
this session (and, it turns out, the original Digger/Super Crate Buino
investigations from an earlier session) had missed, despite explicitly
listing `Gamebuino::update()` as one of the functions already "re-checked
with no reset found anywhere." Confirmed directly from the real AVR core's
own `Print.cpp` (found via a follow-up direct question, "did you actually
check arduino's print function implementation ?") that no alternate
formatting/print path exists that could explain a different mechanism -
every `print()`/`println()` overload funnels through the same single-byte
`write()` this shim's own `gbDrawChar()` already faithfully mirrors.

**Fixed at the root, not per-game**, per direct user choice once the
scope was made clear (three options offered: root fix only, keep it
scoped to Castle Defence, or root fix plus a full per-game audit - "Fix
gbUpdate() at the root" was chosen). `gbUpdate()` now resets
`gbColor`/`gbBgColor` to `(1, 0)` - BLACK ink, WHITE opaque background -
at the start of every frame, alongside its own existing cursor reset. A
game that wants transparent text still needs to call `gbSetColor(color)`
itself each frame it draws that way, exactly like real hardware silently
requires (Castle Defence's own title screen already does this, matching
real `titleScreen()`'s own identical `setColor(BLACK)` call). Once the
root fix landed, all 8 of Castle Defence's own per-site
`gbSetColorBg()`/`gbSetColor()` pairs became redundant (nothing else in
that file ever changes color differently) and were reverted back to plain
print calls, per direct instruction ("also undo the fixes we did here").

**This single missing mechanism also fully explains two previously-shipped
"open, unresolved discrepancy" writeups from an earlier session**
(`gameDigger.c`'s Hard Mode Puzzle and `gameSuperCrateBuino.c`'s "LOCKED!"
map-select screen) - both had a real, live-confirmed bug in this port that
independent testing against real upstream's own unmodified source (via
Simbuino and a cycle-accurate AVR simulator) could never reproduce, with
no explanation found despite an exhaustive re-trace at the time. The
missing mechanism is exactly `displayBattery()`'s own automatic per-frame
reset: a leaked color in this port used to persist across every
subsequent frame indefinitely (nothing ever reset it), while the
identical leak on real hardware self-healed within a single frame,
invisibly. Verified directly, not just theorized: both games' own local
per-site color-reset workarounds were temporarily disabled, rebuilt, and
re-tested live by the user - in both cases the root `gbUpdate()` fix alone
fully resolved the original symptom with no local workaround needed, so
both were removed as redundant (Digger's `digDrawWorld()` no longer calls
`gbSetColor(1)` before its sprite loop; Super Crate Buino's
`scbUpdateChooseMap()` no longer resets color after drawing "LOCKED!").
Both files' own large historical investigation write-ups (the Simbuino/
gbsim testing, the memory-corruption diagnostic, the "open mystery"
framing) were removed entirely from the source comments and from
`BUGS.md` - per direct instruction, game files and `BUGS.md` describe
current, correct behavior only, with no trace of a bug that no longer
exists; the full forensic history lives here instead.

**A third, previously-undocumented instance of the identical pattern was
found and fixed the same way, found via a project-wide sweep for the same
comment phrasing**: `gameSolitaire.c`'s own hand-rolled pause menu
(`soliUpdatePauseMenu()`) explicitly reset color to BLACK at its own top,
with a comment explaining that `soliDrawCursorAt()`'s own trailing WHITE
cursor-highlight draw would otherwise leak forward and make the pause
menu's own text invisible. Tested the same way (temporarily disabled,
rebuilt, live-verified by the user - "seems fixed also") and confirmed
redundant once the root fix is in place; removed. A grep sweep across all
99 games for the same phrasing (comments mentioning color persisting into
"the next/previous frame") found no further instances, and no other game
carries this project's own "OPEN, UNRESOLVED" tag for a color-related
discrepancy - though this was a targeted keyword sweep, not an exhaustive
per-game audit, so a differently-worded instance could still exist
unnoticed somewhere in the other 95 games.

## CrazyTown's fresh-EEPROM highscore display fixed, then its underlying "first highscore never registers" bug fixed too on direct follow-up request

Prompted by a live screenshot ("in crazy town on a fresh memory card
highscores are high it should normally be -1 it probably needs the
sentinel stuff from other ports"). Traced precisely: real `highscore[]` is
a genuine 16-bit signed AVR `int`, so composing a fresh `(0xFF,0xFF)`
EEPROM cell (`lsb + ((msb<<8)&0xFF00)`) narrows to the bit pattern
`0xFFFF`, which as a signed 16-bit value is `-1` - confirmed by hand-
tracing the actual integer promotion/overflow rules, not assumed. This
dialect's `int` is always 32-bit and never narrows, so the identical
formula gives `+65535` instead, matching the screenshot exactly. First
fix attempt matched `gameUfoRace.c`'s own already-shipped precedent for
the identical composition (leave the stored/compared value at 65535,
special-case only the printed value to show `-1`) - correct for UfoRace,
but a direct follow-up question ("will highscores still work then with
score > highscore ?") surfaced that this doesn't generalize: the two
games' own real comparisons run in *opposite* directions.
`gameUfoRace.c`'s own `ufoTime < ufoHighscore[last]` (lower lap time
wins) makes the 65535 sentinel a trivially-easy-to-beat ceiling, so a
fresh cartridge registers its first highscore fine there. CrazyTown's own
`townScoreTotal > townHighscore[last]` (higher score wins) makes the
identical sentinel an impossibly-high floor no real score can ever
exceed - confirmed as a genuine, pre-existing **upstream** bug, not a
porting regression, by checking real `leScoreTotal`'s own declared type
(`unsigned int`, `CrazyTown.ino:37`) - comparing it against the signed
`highscore[]` promotes the signed side to unsigned, reinterpreting real
hardware's own stored `-1` back to `65535` for that comparison too,
identical to this port's own un-narrowed value. This matches this
project's own earlier EEPROM audit (see "A project-wide EEPROM audit..."
above), which had already found and documented this exact case as
"faithfully preserved, not a porting regression to fix."

Per a direct, explicit follow-up request ("fix crazytown then ffs i
explictly ask") once this was made clear, the preserved-bug decision was
overridden: `townInitHighscore()` now resets a detected-fresh
(`==0xFFFF`) cell to `0` instead of leaving it at `65535` - the same
established fresh-cell-sentinel pattern already used throughout this
project (`gameCrabator.c`/`gameDescent.c`/`gameCastleDefence.c`/etc.),
simpler than and superseding the UfoRace-style print-only special case
once the underlying stored value itself is corrected (a reset-to-0 cell
displays as a sensible "0" on its own, with no separate display-time
special-casing needed). `gameUfoRace.c` itself was deliberately left
exactly as it already was - its own fresh-cell behavior is genuinely
correct as shipped, needing no change.

## Gruniozerca's fresh-EEPROM 255 top score fixed - a real display quirk that, unusually, genuinely does match real hardware

Prompted by a live report ("on a fresh memory card gruniozerca highscore
is set to 255 or other invalid value it probably needs the eeprom stuff
like other games"). Unlike the CrazyTown/UfoRace cases above, tracing this
one found no narrow-int divergence at all: real upstream's own `topscore`
is declared a plain `int` (`int topscore = EEPROM.read(0);`), and a single
fresh EEPROM byte (255) fits an `int` without any overflow/narrowing
regardless of platform - real hardware genuinely shows "255" on a brand
new save too, exactly matching what this port already did. The existing
header comment's own "matches real hardware, preserved deliberately"
claim held up under re-verification, unlike CrazyTown's identical-sounding
claim which turned out to only be true for the gameplay comparison, not
the display.

Fixed anyway, per the same direct override precedent CrazyTown just set -
a fresh save showing an already-maxed-out score looks broken to a player
regardless of real-hardware fidelity. `gameGruniozerca_init()` now resets
a freshly-read 255 to 0. Documented, not silently accepted, a real
trade-off unique to this game's own data layout: unlike a 16-bit sentinel
(65535) that's comfortably outside any plausible real score, 255 is
*both* the fresh-EEPROM sentinel and the literal maximum of upstream's own
single-byte-range design, so a genuinely-earned top score of exactly 255
would also read back as 255 and get incorrectly reset - accepted as a
real but vanishingly unlikely edge case (requires exactly 255, not 256+,
not 254-) rather than solved properly with a second EEPROM byte as a
"has been played" flag, which felt like over-engineering for a single-life
casual score.

A second, genuine one-directional divergence was found and deliberately
NOT reproduced while investigating this: this shim's own `eeprom_write_byte()`
stores a full, unmasked `int` (`currentSlot.data[]` is `int[]`, no `&
0xFF` anywhere), while real `EEPROM.write(uint8_t)` truncates to a single
byte - so a real player who legitimately scores above 255 has their save
silently wrap/corrupt on real hardware (e.g. 300 truncates to 44), while
this port persists the true, correct value. A real upstream limitation,
but a strictly worse one with no reason to deliberately reproduce it here
- left as an accidental improvement, not treated as a divergence needing
a fix.

## Lights Out AD's "You won!" timer fixed - another mistaken "matches real hardware" claim caught by a live report

Prompted by a live report ("on the 'you won' screen the time keeps
increasing instead of remaining static like on real hardware"). This
file's own header comment already claimed the opposite - that the
displayed time is recomputed every tick "matching upstream's own real
`gb.frameCount - startT` call sitting directly inside the loop" - a claim
that, like a handful of others found this session, turned out to be wrong
on direct re-verification, not just re-trusted. Real upstream's own
`won()` computes `int time = gb.frameCount - startT;` exactly once, as
its own very first statement, **before** its own blocking `while(1)`
display loop even begins - not inside that loop at all. Real hardware
therefore shows a genuine static snapshot of the finish time; this port's
own per-tick state-machine architecture calls `loutUpdateWon()` every
single frame, and the earlier, mistaken port recomputed `gbFrameCount -
loutStartT` fresh on every one of those calls, making the displayed time
visibly climb for as long as the player lingered reading the win screen.

Fixed by adding a real snapshot variable, `loutWonTime`, computed once in
`loutBeginWon()` (the exact point in this port's own state machine that
corresponds to real `won()`'s own function entry, right after the win
condition is confirmed and right where real upstream's own `time =
gb.frameCount - startT` line sits) - `loutUpdateWon()` now prints that
cached value every tick instead of recomputing it.

## A real D-pad input-bleed bug found in the menu's own quit-to-menu transition

Prompted by a direct question ("can button state be cleared so left over
inputs won't affect the menu position from the current game we quited").
Checked rather than assumed, and found a genuine gap: `menu_init()` (called
right after confirming Quit in the dialog, per `portVircon32.c`'s own
`currentGameIndex = -1; menu_init();`) used to unconditionally reset
`prevUp`/`prevDown`/`prevLeft`/`prevRight` to `false`, with no regard for
whatever the D-pad's own real physical state already was at that exact
moment. Button A itself was already safe on this exact path -
`md_armInputAGate()` (armed by `portVircon32.c` immediately before
`menu_init()` runs) makes `md_inputA()` itself report "released" until the
real button genuinely is, so `menu.c`'s own `a = md_inputA()` read already
couldn't see a leftover press - but no equivalent gate exists anywhere for
Up/Down/Left/Right. A player still holding, say, Right (moving their
character) at the exact moment they confirmed Quit would have `prevRight`
forced to `false` while the real button stayed `true` - manufacturing a
false "just pressed" edge on the very next `menu_update()` tick and
instantly paging the just-reopened menu sideways, with no new input from
the player at all.

Fixed by having `menu_init()` sample each button's own real current state
(`md_inputUp()`/`Down()`/`Left()`/`Right()`) instead of assuming released -
the exact same "arm against whatever's already held" idea
`portVircon32.c` already uses for `prevConfirmLeft`/`Right`/`A` right
before opening the quit dialog itself, just applied to the menu's own
return path too. `menu_init()`'s other call site (cartridge boot, before
any game has ever run) is unaffected in practice - a player could
technically be holding a direction at the exact moment the ROM finishes
loading, but that's a far narrower window than the every-single-quit case
this was actually found for.

## Audio authenticity pass: a real square-wave timbre, and exact real playOK/playCancel/playTick pitches

Prompted directly ("can audio be made (more) authentic"), scoped down via
a direct multi-select question to two of three real, independently-sized
options found by investigation (a third - porting the real single-channel
tracker/pattern/track/instrument engine itself - was surfaced but not
chosen this session; see the Sound bullet under "Open questions" below).

**Timbre**: `md_playTone()` (via `libs/PlayNote`) was playing every note
in every game through `sounds/wt_saw.wav`, a plain sawtooth single-cycle
sample - PlayNote's own generic default, never swapped for anything
Gamebuino-specific. Real hardware's own default instrument
(`Sound.cpp`'s `squareWaveInstrument`) drives a genuine 2-level PWM square
wave instead (`Sound::generateOutput()`'s own `_chanState[]` toggle has no
duty-cycle control of its own - a plain, symmetric 50% duty cycle,
confirmed by reading the real ISR directly rather than assumed). Replaced
with a new `sounds/wt_square.wav` (`tools/gen_square_wavetable.py` - a
real, checked-in generator, matching this project's own
`gen_column_atlas.py`/`gen_pixelgrid.py` precedent rather than a one-off
manual asset), same 256-sample/44100Hz/mono/16-bit format as the sample it
replaces (inspected directly, not assumed) so no other code needed to
change - `Make.bat`/`Make.sh`/`rom.xml`/`portVircon32.c`'s own
`WAVETABLE_SOUND_ID` comment updated to reference it. A single global
asset swap - every one of the 99 games' own `gbPlayNote`/`gbPlayTick`/
`gbPlayOK`/`gbPlayCancel` calls sound authentically squarer/buzzier at
once, no per-game changes needed.

**Exact playOK()/playCancel()/playTick() pitches**: these three were
previously hand-guessed frequencies/durations (900/1400Hz etc, picked
without reference to real hardware). Real hardware plays them from real,
tiny pattern data instead - decoded by hand against
`Sound::updatePattern()`'s own real bit-packed word format (a leading
2-bit command-flag/reserved pair, then either a 4-bit cmd + 5-bit X + 5-bit
Y command word, or a 6-bit pitch + 8-bit duration note word) and the real
36-entry `_halfPeriods` pitch table (`EXTENDED_NOTE_RANGE`'s own real
default of 0): `playOKPattern={0x0005,0x138,0x168,0x0000}` decodes to
select-instrument-0(square) then pitch 14 (halfPeriod 110) then pitch 26
(halfPeriod 55), 1 frame each; `playCancelPattern` is the same two notes
reversed; `playTickP={0x0045,0x168,0x0000}` selects instrument 1 (noise,
not square) then pitch 26, 1 frame. Real audible frequency =
`15000 / (2*halfPeriod)` - `Sound::generateOutput()`'s own directly-stated
real 15kHz ISR rate, toggling the output every halfPeriod ISR ticks -
giving OK a real rising 68.18Hz->136.36Hz blip, Cancel the same falling,
and Tick a 136.36Hz tone (the real noise instrument's own pseudorandom-
amplitude buzz has no equivalent in this shim's plain single-cycle-
wavetable tone engine, so it plays as a plain tone at the real pitch/
duration instead - a documented, honest simplification, not a silent
approximation). Duration is real display frames (`1.0/gbFrameRateFps`),
matching `gbPlayNote()`'s own already-correct frame-relative scaling, not
a fixed wall-clock constant.

**A direct "do sounds get collapsed" follow-up, prompted by the two-call-
per-effect shape this fix produces, was checked against the sibling
tinyjoypad_vircon32 project's own real "burst collapses to one tone" bug
history (a whole multi-game saga in that project's own CLAUDE.md) rather
than assumed either way**: that project's own `md_playTone()` originally
forced every call through one shared, manually-tracked voice
(`playnote_stop_all()` before every new tone), so a burst of N calls with
no real time between them was only ever audible as the last one - later
fixed there to a genuinely multi-voice design (a 16-element
`audioStopAtFrame[]` array, one per real SPU channel, no
`playnote_stop_all()` inside `md_playTone()` itself, since
`playnote_start()`'s own `play_sound()` call already picks a free channel
internally). Diffed this project's own current `md_playTone()`/
`md_stopTone()`/`md_updateAudio()` directly against that project's fixed
version: word-for-word identical - this project was evidently already
built on the corrected design, never the buggy one. Confirmed, not just
assumed: `gbPlayOK()`/`gbPlayCancel()`'s two `md_playTone()` calls each
land on a genuinely separate channel and really do sound together (a
rising/falling octave dyad, matching the doc comment above), no fix
needed.

**A real, separate, out-of-scope-for-this-session finding surfaced while
deriving the pitch table above, left for a future session**: this shim's
own general-purpose `gbPlayNote(pitch, duration)` (called directly by 32
already-shipped games) computes frequency as a MIDI-relative
`440*2^((pitch-45)/12)`, but real `Sound::playNote(pitch,...)` treats
`pitch` as a direct 0-35 index into `_halfPeriods` - a different
convention. Numerically the two formulas track fairly closely over that
0-35 range (a consistent ~7-9% sharp bias, not an order-of-magnitude
mismatch - e.g. pitch 14 gives 73.35Hz here vs a real 68.18Hz), so this is
a real, quantified, small-but-nonzero inaccuracy for any of those 32
games' own direct `gb.sound.playNote()` calls, not a broken one - worth a
dedicated pass (deriving the exact real per-pitch table directly rather
than a formula, the same way this session did for OK/Cancel/Tick) if
audio authenticity is revisited again.

## A full real tracker/pattern/track engine, restored across 34 games, and a real division-by-zero crash found and fixed via a live user report

Direct follow-up ("now also do 3") picking up the third, larger option the
previous audio-authenticity pass had surfaced but not built: a real port of
`Sound.h`/`Sound.cpp`'s own single-oscillator-per-channel tracker engine -
`playPattern()`/`playTrack()`/`command()`/instrument envelopes/volume-
slide/arpeggio/tremolo - none of which this shim had any equivalent for
before, so every game calling them directly (confirmed via a real grep
sweep of `more games/`: ~34 already-shipped games do) had its real music/
sound-effect sequences dropped or approximated to a single stand-in tone.

**The engine** (`gamebuinoShim.h`/`.c`'s own Sound section): `gbPlayPattern`/
`gbPlayTrack`/`gbSoundCommand`/`gbChangePatternSet`/`gbChangeInstrumentSet`/
`gbPlayNoteChannel`, plus an internal `gbUpdateSoundTracker()` called
automatically once per real tick from `gbRenderFrame()` (matching real
`Gamebuino::update()`'s own automatic `sound.updateTrack()`/
`updatePattern()`/`updateNote()` tail call) - a direct, function-for-
function port of real `Sound::playPattern`/`playTrack`/`command`/`playNote`/
`updateTrack`/`updatePattern`/`updateNote`/`generateOutput`, across
`MAX_SOUND_CHANNELS=4` (matching real hardware's own documented "0 to 4"
range - every real game found calling this API directly uses channel 0-3).
Real instrument-envelope stepping, volume-slide/arpeggio/tremolo command
state, and a real `prescaler` (`gbSoundPrescaler = max(1,fps/20)`,
recomputed on every `gbSetFrameRate()` call exactly like real
`Gamebuino::setFrameRate()` does) that keeps note/effect timing wall-clock-
consistent across games configured at different frame rates, are all
faithfully reproduced. Since Vircon32's own hardware model has no
equivalent to real hardware's own continuously-retuned single oscillator,
two new primitives (`md_trackerVoiceStart`/`Retune`/`Stop`,
`machineDependent.h`/`portVircon32.c`) give a tracker channel a genuinely
sustained, continuously-retuned voice (reusing PlayNote's own
`select_channel`/`set_channel_speed`/`set_channel_volume` primitives
directly, bypassing its fade system) rather than restarting a fresh
one-shot voice every tick, which would sound like a stutter of separate
attacks instead of one continuous note.

**`gbPlayNote()`'s own previously-flagged pitch-formula bug (see the
"Audio authenticity pass" section above) is fixed as a direct consequence**
of building this engine on the real `_halfPeriods`-table-based frequency
formula throughout - the old MIDI-relative `440*2^((pitch-45)/12)`
approximation is gone entirely, so all 32 games that call `gbPlayNote()`
directly now get the exact real frequency, not just a close one.

**Validated with a real pilot before scaling out**: Master Kebab
(`gameMasterKebab.c`, `gb.sound.playPattern(musique,1)`) was migrated by
hand first and verified via Puppeteer (real gameplay reached, the engine's
own per-tick `gbUpdateSoundTracker()` running continuously with zero
crashes) before dispatching further work.

**34 more games migrated via 15 parallel background agents** (this
project's own established isolated-worktree workflow, `isolation:
"worktree"`), each given the engine's full API reference plus its own
real upstream source location: 101Starships (real background music via
2 real tracks, the most substantial single restoration), Crabator (7 real
pattern sites), Super-Crate-Buino (26 sites), UFO-Race, Solitaire,
SpinSpinSpinbuino, Sokobuino, Copter, FlappyBirdo, Bomber, shipwrek,
BigBlackBox, Robot, Artillery, NoNamePlatformGame, BlocksBuino, Simonbuino,
Smash-and-Crash, SnakeAbc, StickFighter, armageddon, bub, skibuino,
Tetrino, Video Poker, ShootBuino, CopterStrike, Asterocks, Invaders,
Killrace, Lander, Paqman, and Aerial-Assault - each restoring its own real
`soundfx[][]` table and/or pattern/track/instrument data byte-for-byte
against real upstream source, replacing whatever one-shot-tone
approximation or dropped call the original port had. Two games
(gamebuino-community-rpg, Elventure) were checked and confirmed to have no
live restorable sound call at all - both route entirely through already-
documented dead/unportable code paths (Elventure's real `play_song()` has
`return;` as its literal first statement; gamebuino-community-rpg's real
music is a self-programming-flash hack this platform has no equivalent
for) - correctly left unchanged rather than reviving dead code.
**Sokobuino's own agent found a real, previously-unnoticed upstream bug**
along the way: real `TriggerFx()` calls `gb.sound.playTrack()` on data
that's actually `0x0000`-terminated (real pattern-shaped data, not the
`0xFFFF`-terminated shape a real track needs) - a genuine upstream mixup
between two same-signature functions, decoded and ported as the real
pattern data it clearly is rather than reproduced as the unreproducible
(and, ported literally, NULL-pointer-crash-risking) undefined behavior
real hardware would hit.

**Integration**: each agent's own finished file was pulled directly onto
`main` from its own worktree branch (`git show <branch>:<path>`, not a
full branch merge, since several agents had independently fast-forward-
merged the base engine commit into their own branch in different orders -
pulling just the final file content sidesteps that entirely), followed by
one consolidated rebuild across all 34 changed files together.

**A real division-by-zero crash, found via a live user report immediately
after integration, traced and fixed**: Copter crashed on real,
sustained machine-gun fire. Root cause: real `Sound.h`'s own
`outputPitch[]`/`outputVolume[]` are `uint8_t`/`int8_t` - every real
assignment to them (including a `+=`) narrows the result to that real
width *before* the real `(x+NUM_PITCH)%NUM_PITCH` wrap ever runs,
guaranteeing that wrap is always safe on real hardware. This dialect's
`int` never narrows, so a real, genuinely reachable case - Copter's own
machine-gun `soundfx` row decodes to a real arpeggio step of -46 every 2
ticks (`gbSoundPrescaler`=2 at 41fps) over a real 20-tick note - compounds
`outputPitch` deep into negative territory well before the final wrap
instead of narrowing back into range first, indexing `gbHalfPeriods[]`
out of bounds and feeding whatever garbage word sat there into
`freqHz = 15000/(2*halfPeriod)` as a real, live divide-by-zero - a trap
this platform enforces that real hardware simply has no equivalent for
(a wrapped, in-range `uint8_t` index is never garbage on real hardware to
begin with). Fixed by adding a new `gbNarrowInt8()` helper and reproducing
the real narrowing explicitly at each real assignment point (`&0xFF` for
`outputPitch`, `gbNarrowInt8()` for `outputVolume`) - the same
"replicate real AVR narrow-int behavior explicitly" precedent this
project has already established repeatedly (see the EEPROM-narrowing
audit elsewhere in this file). Verified fixed both by the user's own live
retest and a Puppeteer session holding Copter's fire button continuously
for 2 real seconds (repeatedly re-triggering the exact soundfx that
crashed) with zero errors.

Per direct instruction, the entire effort (the engine, the pitch-formula
fix, all 34 game migrations, and the Copter crash fix) was squashed into
a single commit on `main`, with every agent worktree and branch deleted
afterward - no per-agent branch history left referenced anywhere.

## A real ISR-rate bug found via a live recorded-audio comparison against Simbuino: the "15000 times per second" comment was wrong by ~3.8x

Prompted by the user recording ~108 seconds of real gameplay audio (101
Starships' own background music, once through this project's own port,
once through Simbuino - a real, independent, cycle-accurate AVR
simulator, `github.com/Myndale/Simbuino`, temporarily cloned to
investigate) and asking directly why the two sounded different.

FFT analysis of the recording (`numpy`, a plain Hanning-windowed
`rfft` over two ~15-second segments, one from each source) found two
clean, simultaneous dominant-frequency clusters in both recordings - a
real match on musical content (this project's own port and Simbuino
agree on WHICH two notes 101Starships' own two background-music channels
are playing at any given moment, and the ratio between those two notes
matches to within 0.01% between the two recordings) - but every
Simbuino frequency measured a consistent **~3.8x higher** than this
project's own corresponding one.

Root cause, confirmed independently two different ways rather than
guessed at: this shim's real audible-frequency formula
(`freqHz = 15000 / (2*halfPeriod)`) had trusted a real comment sitting
directly in real `Sound.cpp` ("this function runs 15 000 times per
second") as ground truth for `Sound::generateOutput()`'s own real
Timer1-ISR rate, reasoned at the time as "the actual firmware author's
own stated fact... more reliable than my own uncertain re-derivation
from register values with an assumed F_CPU." That reasoning turned out
to be backwards - the comment itself was the wrong figure, not the
derivation:
- **Simbuino's own source** (`AtmelProcessor.cs`) hardcodes
  `public const int ClockSpeed = 16000000` - a real, independently-
  authored, cycle-accurate simulation's own confirmed real Gamebuino
  Classic CPU clock (16MHz), not something this project had verified
  directly before.
- Combined with real `Sound::begin()`'s own already-read timer setup
  (`OCR1A=280`, CTC mode via `WGM12`, prescaler 1 via `CS10`), the real
  ISR period is `OCR1A+1=281` clock cycles, giving a real rate of
  `16000000/281 = 56939.5 Hz` - **not** 15000.
- `56939.5/15000 = 3.796`, matching the measured ~3.8x audio ratio to
  within 0.08% - a decisive, independent confirmation from a completely
  different real source (recorded audio) than the one that exposed the
  register-derived value in the first place (Simbuino's own source).

Fixed by replacing the hardcoded `15000.0` throughout the tracker
engine's own frequency formula with a new, properly-derived
`GB_SOUND_ISR_HZ` constant (`16000000.0/281.0`), and recomputing
`gbPlayTick()`/`gbPlayOK()`/`gbPlayCancel()`'s own real derived
frequencies (68.18Hz/136.36Hz -> 258.82Hz/517.63Hz - the same real
`_halfPeriods` table indices and octave relationship as before, just at
the real, corrected absolute pitch). This is a single, centralized fix
(one constant, one formula, in `gbUpdateNoteChannel()`) that corrects
every one of the 34 already-migrated games' own real tracker/pattern
music and sound effects at once, with no per-game changes needed -
ported identically to the sibling `gamebuino_classic_sdl` project's own
copy of the same shared engine code.

**A related discipline lesson, worth keeping in mind for any future
similar situation**: the original "trust the real source's own comment
over my own re-derivation" reasoning was a real, considered judgment
call at the time, not a careless assumption - but it turned out to be
exactly backwards, because a source comment is still just prose written
by a human, with no compiler checking it against the actual code the
way the register values themselves are enforced. The register-derived
value was always the more authoritative one; it should have been
computed and cross-checked rather than deferred to the comment
"deferred to be extra literal-source-faithful," even though checking it
turned out to require exactly this kind of independent verification
(a second real source - Simbuino's own hardcoded clock constant - or a
live recorded-audio comparison) that wasn't obviously available to hand
until directly asked.

## A real SoundTest diagnostic tool added, which immediately surfaced a real, pre-existing playOK()/playCancel() bug: two simultaneous tones instead of a real sequenced pattern

Prompted by "can you make me an ino to test the default sounds and other
sounds" - a real Arduino sketch (`more games/SoundTest/SoundTest.ino`,
built on the real `<Gamebuino.h>` library directly, not this project's own
shim) exercising `playOK()`/`playCancel()`/`playTick()` plus a raw
`playNote()` sweep across the real 0-35 pitch range, for flashing to real
hardware/Simbuino to record fresh reference audio. Immediately followed by
"port soundtest as a game and add it to the menu" - ported as a genuine
14th-registered-in-this-batch, in-cartridge diagnostic tool
(`src/games/gameSoundTest.c`, `markUnfinished()`-flagged with
`"Diagnostic tool, not a game"` as its own `info` line, since it's not a
real upstream game at all) offering the identical test list via UP/DOWN
select + A play, so the same tones can be replayed from directly inside
the port with no separate hardware/build needed.

**Using it immediately surfaced a real, live, previously-undetected bug**:
"playok and playcancel sound the same in our port." Traced directly against
real `Sound.cpp`: `Sound::playOK()`/`playCancel()` both call
`playPattern(...,0)` on a real, genuine 2-note SEQUENCE (`playOKPattern
= {0x0005,0x138,0x168,0x0000}` decodes to select-instrument-0 then
NOTE(pitch=14,dur=1) then NOTE(pitch=26,dur=1); `playCancelPattern` is the
identical two notes in reverse order) - the two sounds are only audibly
different from each other *because* they're sequenced in time (rising vs
falling). This shim's own `gbPlayOK()`/`gbPlayCancel()` had instead always
called `md_playTone()` **twice, back-to-back with no delay between the two
calls**, at the two correct frequencies but with no timing offset at all -
producing two *simultaneous* tones (a chord) rather than a sequence. Since
a two-note chord sounds identical regardless of which order the two
`md_playTone()` calls happen to run in, `gbPlayOK()` and `gbPlayCancel()`
had been genuinely, audibly indistinguishable from each other since the
very session this pair was first implemented - a real bug hiding behind an
earlier investigation's own mistaken "do sounds get collapsed" conclusion
(see "A full real tracker/pattern/track engine..." above), which had
checked only whether the two `md_playTone()` calls landed on separate
channels and sounded together at all, not whether "together" should have
meant "simultaneously" in the first place.

**Fixed by switching both (and `gbPlayTick()`, for consistency) to the
already-existing, already-proven `gbPlayPattern()` tracker engine**,
reproducing the real pattern arrays verbatim
(`gbPlayOKPattern`/`gbPlayCancelPattern`/`gbPlayTickPattern`, decoded by
hand against the real bit layout `gbSoundCommand()`/
`gbUpdatePatternChannel()` already implement) instead of the ad-hoc
two-direct-tone-call hack that predated the tracker engine's own
existence in this codebase - `gbPlayPattern()` already resets volume/
slide/arpeggio/tremolo state and stops whatever was previously playing on
the target channel, so no extra setup was needed. Applied identically in
the sibling `gamebuino_classic_sdl` project's own copy of the same shared
engine code (all three of its SDL2/SDL3/Playdate builds rebuilt clean
afterward).

## Open questions

- **Sound**: fully resolved - no longer an open question. Real square-wave
  timbre, real derived `gbPlayTick`/`OK`/`Cancel` pitches, a real
  `gbPlayNote()` pitch formula (the previously-flagged MIDI-relative
  approximation is gone, replaced by the same real `_halfPeriods`-table
  formula the full engine uses), and a real `playPattern`/`playTrack`/
  `command`/instrument-envelope tracker engine, restored across 34 already-
  shipped games - see "A full real tracker/pattern/track engine..." above
  for the complete writeup (including a real division-by-zero crash found
  and fixed via a live user report). The one remaining, deliberately-out-
  of-scope gap: real per-instrument-step *duty-cycle*/waveform-shape
  variety beyond the shared default square/noise pair - any game that
  registers its own fully custom instrument set still gets it faithfully
  (the engine reads real instrument data generically), this is only about
  never having found a real shipped game that needed anything beyond the
  two real defaults to sound correct.
- **Font fidelity**: resolved - see "Real Gamebuino fonts ported" below.
- **Feature parity with the sibling project**: no longer an open
  question - every feature that project ended up building (thumbnail
  atlas, quit-confirmation dialog, pixel-grid overlay, global mute
  toggle, EEPROM persistence) has now been ported here too - see
  "Architecture" above.
