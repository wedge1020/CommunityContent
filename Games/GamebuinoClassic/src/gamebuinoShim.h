#ifndef GAMEBUINO_SHIM_H
#define GAMEBUINO_SHIM_H

// -----------------------------------------------------------------------------
// Reproduces the real Gamebuino Classic library's own API surface
// (Gamebuino.h/utility/Display.h/Buttons.h/Sound.h) on top of machineDependent.h
// - the same "flatten a real single-instance C++ library into plain C
// globals/functions" treatment already proven for the sibling
// tinyjoypad_vircon32 project's own tinyJoypadShim.h/obonoCoreShim.h.
//
// This dialect has no classes/methods/operator overloading (confirmed via
// the sibling project's own VIRCON32_C_DIALECT.md) - real `gb.display.
// fillRect(...)`/`gb.buttons.pressed(...)`/`gb.sound.playTick()` call syntax
// cannot be preserved literally. Every ported game needs its own `gb.x.y(...)`
// call sites mechanically rewritten to a plain `gbY(...)` function call
// instead (there is only ever one `gb` instance in any real cartridge
// anyway, so flattening loses nothing) - see gamePong.c's own header comment
// for a worked example of the exact rewrites this needed.
//
// Real hardware has a genuine 84x48 PCD8544 (Nokia 5110) CPU-writable
// framebuffer (Display::_displayBuffer[]) - ported as a plain in-RAM
// int[LCD_WIDTH*LCD_PAGES] framebuffer here, matching every "genuine pixel
// framebuffer" port already solved in the sibling project (Gilbert in the
// Downland, Tiny Arena, Ardumania) rather than needing a page-byte stream
// reconstruction the way TinyJoypad's own driver lineages needed.
//
// Text rendering ports Gamebuino's own real font3x5/font5x7/font3x3 bitmap
// fonts directly (see this file's own Font tables section in
// gamebuinoShim.c) - real setFont()/print() semantics, including real
// per-font inter-char/line spacing.
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
//   Configuration
// -----------------------------------------------------------------------------
// GB_GRAY has two renderings. Real hardware's own is a checkerboard dither
// that flickers with the frame counter (see GB_GRAY's own doc comment
// below), reproduced exactly here. Vircon32's own screen is true-color,
// with no monochrome LCD to fake a third shade against, so the alternative
// is a real, solid, flat gray - `gbRealGrayColor` picks between them at
// runtime. This global itself initializes to false (real hardware's own
// behavior); main() turns it ON at startup, so the cartridge opens on the
// solid rendering - see portVircon32.c for why. It is bound to Button R and
// wired up in portVircon32.c's own dispatch loop the same way Button L/Y
// toggle the pixel-grid overlay/global mute, and draws as a second texture
// pass in gbRenderFrame()/md_drawColumnGray() - see gbGrayBuffer's own
// comment in gamebuinoShim.c and assets/columns_gray.png/
// tools/gen_column_atlas_gray.py. A deliberate Vircon32-specific visual
// enhancement beyond real hardware, not a compatibility requirement. Every
// drawing primitive branches on this flag at the exact same spot it already
// branches on GB_GRAY itself, so flipping it off costs one already-paid-for
// runtime check, not extra work - and gbGrayBuffer's own per-frame clear
// and gbRenderFrame()'s own second pass both self-gate on gbAnyGrayDrawn
// (see its own comment in gamebuinoShim.c), which never gets set at all
// while this flag is off, so neither one does any extra work either.
extern bool gbRealGrayColor;

// The third GB_GRAY rendering: real LCD persistence, reproducing what the
// Simbuino emulator's own LcdDevice shows - a pixel toggled on alternating
// ticks reads as a steady middle shade instead of a flicker, because the
// panel never fully settles either way. Mutually exclusive with
// gbRealGrayColor (that one recolors GB_GRAY draws specifically; this one
// smears whatever actually alternates, GB_GRAY's own dither included, so
// GB_GRAY must dither normally for it to work). portVircon32.c's own Button
// R cycles the three modes; see gbLcdPersistence's own comment in
// gamebuinoShim.c for the model and why it costs no extra per-pixel work.
extern bool gbLcdPersistence;

#define LCDWIDTH LCD_WIDTH
#define LCDHEIGHT LCD_HEIGHT

// Button IDs - real hardware has these wired to specific digital pins
// (see the real library's own utility/settings.c), irrelevant here since
// every gbXxx() input function goes straight through machineDependent.h's
// own md_input*() calls instead of reading a raw pin.
#define BTN_UP    0
#define BTN_DOWN  1
#define BTN_LEFT  2
#define BTN_RIGHT 3
#define BTN_A     4
#define BTN_B     5
#define BTN_C     6

// -----------------------------------------------------------------------------
//   Core / lifecycle
// -----------------------------------------------------------------------------

// Call once from a game's own init(). Frame rate defaults to 20 (matching
// real Gamebuino Classic's own out-of-the-box default - `Gamebuino::begin()`
// sets `timePerFrame = 50` directly, i.e. 1000/50 = 20fps - confirmed
// directly in the real Gamebuino.cpp, not assumed; easy to mix up with the
// later META library's own different 25fps default) - call
// gbSetFrameRate() afterward if a game needs a different rate.
void gbBegin();

void gbSetFrameRate( int fps );

// Real gb.update() both throttles to the configured frame rate AND clears+
// redraws the display buffer's own "changed since last frame" bookkeeping -
// ported as a plain whole-tick throttle (see gbUpdate()'s own definition):
// returns true on the one real engine tick that should actually run this
// frame's worth of game logic, matching the pattern every throttled game in
// the sibling project already uses. A ported game's own loop()-equivalent
// should be structured as `if( gbUpdate() ) { ... }`, exactly mirroring
// upstream's own `if (gb.update()) { ... }` shape.
bool gbUpdate();

// Streams the current framebuffer to the real GPU via the column atlas -
// every ported game's own `_update()` function MUST call this exactly once,
// as its very last statement, on every tick `gbUpdate()` returns true (see
// gamePong.c's own `gameXxx_update()` for the exact shape every other
// shipped game already follows: `if( !gbUpdate() ) return; ...draw...;
// gbRenderFrame();`). Forgetting this call is a real, easy-to-make mistake
// with a deceptively unhelpful symptom - every draw call still runs and
// writes into the framebuffer correctly, but nothing ever reaches the
// screen, so the game silently renders as a permanently blank display with
// no error of any kind.
void gbRenderFrame();

// Real Gamebuino::frameCount - a real, public tick counter, incremented
// once per real logic tick by gbUpdate() itself (see its own definition).
// Reset to 0 by gbBegin() (a real, considered adaptation for this
// project's own multi-game-per-session cartridge model - see gbBegin()'s
// own comment). Useful directly for animation/blink pacing (`frameCount %
// N`-style checks).
extern int gbFrameCount;

void gbPickRandomSeed(); // no-op - see this file's own header comment

// -----------------------------------------------------------------------------
//   Buttons
// -----------------------------------------------------------------------------

bool gbPressed( int button );  // true on the exact tick the button transitions to held
bool gbReleased( int button ); // true on the exact tick the button transitions to released
bool gbHeld( int button, int frames ); // true if held for at least `frames` real ticks
// Real Buttons::timeHeld() - the actual real tick count a button has been
// held, for games that need the number itself (variable jump height, a
// charge-up mechanic) rather than just a threshold check.
int gbTimeHeld( int button );
// true once immediately on press, then true again every `period` ticks
// while still held - matches real Buttons::repeat()'s own auto-repeat feel
bool gbRepeat( int button, int period );

// -----------------------------------------------------------------------------
//   Display
// -----------------------------------------------------------------------------

void gbClear();
// `color` is a real, confirmed no-op - real Display::fillScreen() always
// fills solid BLACK regardless of what color is passed (a genuine real
// hardware bug - see gamebuinoShim.c's own header comment on this
// function). Kept as a parameter anyway so call sites stay a literal
// match for real `gb.display.fillScreen(...)`.
void gbFillScreen( int color );

// Real hardware's own real color constants (Display.h: WHITE=0, BLACK=1,
// INVERT=2, GRAY=3) - GB_ prefixed since this dialect's flat global
// namespace has no per-file scoping to protect a bare `WHITE`/`BLACK` from
// colliding with some future ported game's own identically-named local.
// GB_GRAY is a real, genuine draw color too (real Display.h's own GRAY=3,
// enabled by default on real hardware via ENABLE_GRAYSCALE) - a checkerboard
// dither that flickers with the frame counter, not a flat fill (see
// gbSetColor()'s own doc comment below).
#define GB_WHITE 0
#define GB_BLACK 1
#define GB_INVERT 2
#define GB_GRAY 3

// Direct port of real Display::setColor(color) - color is one of
// GB_WHITE/GB_BLACK/GB_INVERT/GB_GRAY above. GB_INVERT toggles whatever's
// already on screen at each drawn pixel (a real XOR against the
// framebuffer, not a fixed color). GB_GRAY is a real checkerboard dither,
// ported directly from real `Display::drawPixel()`'s own formula: each
// pixel draws BLACK or WHITE depending on `(x&1)^(y&1)` XORed against the
// low bit of `gbFrameCount`, so the pattern also flips every other real
// tick - a genuine two-pixel-period spatial dither that also flickers over
// time, exactly like real hardware's own default (non-optional) GRAY
// behavior, not a flat gray fill (this display has no such thing - it's
// 1-bit). gbDrawPixel()/gbFillRect()/gbDrawFastHLine()/gbDrawFastVLine()/
// gbDrawBitmap()/gbDrawBitmapRotated()/gbDrawChar() (and everything built
// on top of them - circles, triangles, rounded rects, gbDrawLine()) all
// support both GB_INVERT and GB_GRAY, matching real hardware, where both
// are real `color` values read by the same low-level `drawPixel()` every
// other primitive is built on - gbDrawChar()'s own separate `gbBgColor`
// mechanism is unaffected, matching real hardware's own drawChar()
// likewise never checking for INVERT/GRAY itself (it just calls
// drawPixel()/fillRect(), which already do).
void gbSetColor( int color );

// Direct port of real Display::setColor(color, bg) (the two-argument
// overload) - only gbDrawChar() ever reads the background half of this
// (see its own header comment in gamebuinoShim.c): with `bg` different
// from `color`, a printed glyph's own "off" pixels are drawn in `bg`
// (a real opaque text background) instead of staying transparent.
void gbSetColorBg( int color, int bg );
void gbDrawPixel( int x, int y );
int gbGetPixel( int x, int y );
void gbDrawLine( int x0, int y0, int x1, int y1 );
void gbDrawFastHLine( int x, int y, int w );
void gbDrawFastVLine( int x, int y, int h );
void gbDrawRect( int x, int y, int w, int h );
void gbFillRect( int x, int y, int w, int h );
void gbDrawCircle( int x0, int y0, int r );
void gbFillCircle( int x0, int y0, int r );

// Direct ports of real Display::drawRoundRect()/fillRoundRect()/
// drawTriangle()/fillTriangle() (utility/Display.cpp).
void gbDrawRoundRect( int x, int y, int w, int h, int r );
void gbFillRoundRect( int x, int y, int w, int h, int r );
void gbDrawTriangle( int x0, int y0, int x1, int y1, int x2, int y2 );
void gbFillTriangle( int x0, int y0, int x1, int y1, int x2, int y2 );

// Real Gamebuino Classic's own Display::drawBitmap(), confirmed directly
// against the real Display.cpp source rather than assumed. `bitmap[0]`/
// `[1]` are the real width/height header bytes, then `ceil(width/8)` bytes
// per row (row-major, MSB-first) - this is a DIFFERENT byte layout from
// this shim's own `gbFrameBuffer[]` (column-page, LSB=top - see this
// file's own header comment) - the two must not be confused. Every real
// PROGMEM byte becomes one plain int cell here (matching every other byte
// table in this project, e.g. gbFont5x7/gbFont3x5). Only "on" bits are drawn, in
// whatever color gbSetColor() last set - "off" bits are fully transparent,
// leaving whatever's already on screen untouched, exactly like real
// hardware's own bitmap compositing.
void gbDrawBitmap( int x, int y, int* bitmap );

// rotation: 0=none, 1=CCW, 2=180, 3=CW (matches real NOROT/ROTCCW/ROT180/
// ROTCW). flip: 0=none, 1=horizontal, 2=vertical, 3=both (matches real
// NOFLIP/FLIPH/FLIPV/FLIPVH). Ported bit-for-bit from real
// Display::drawBitmap(x,y,bitmap,rotation,flip) - including that
// function's own real quirks, confirmed directly in the real source: flip
// is applied using the bitmap's ORIGINAL (pre-rotation) width/height
// rather than the rotated shape's own effective dimensions, and vertical
// flip mirrors via `h - l` (not `h - l - 1`, asymmetric with the
// horizontal case's own `w - k - 1`) - preserved exactly so a game relying
// on either behaves identically to real hardware, not "fixed" into
// different, never-actually-shipped behavior.
void gbDrawBitmapRotated( int x, int y, int* bitmap, int rotation, int flip );

// Text - real Gamebuino bitmap fonts (see this file's own header comment
// and gamebuinoShim.c's own Font tables section). gbFontWidth/gbFontHeight
// are the real per-glyph cell size (raw glyph size + 1, real inter-char/
// line spacing baked in exactly like real Display::setFont()) - read-only,
// kept up to date by gbSetFont(). gbFontSize is the real integer size
// multiplier (1 = native, 2 = each glyph pixel doubled - the two sizes
// Gamebuino Classic games actually use in practice).
extern int gbCursorX, gbCursorY, gbFontSize, gbFontWidth, gbFontHeight;
extern int[642] gbFont5x7; // real hardware's own larger font - {5,7} raw cell, ASCII 0-127
extern int[386] gbFont3x5; // real hardware's own default font - {3,5} raw cell, ASCII 0-127
extern int[386] gbFont3x3; // real hardware's own smallest font - {3,3} raw cell, ASCII 0-127
void gbSetFont( int* font ); // font: one of gbFont5x7/gbFont3x5/gbFont3x3 above
void gbPrintString( int* text );
void gbPrintNumber( int value );

// Direct port of real Arduino Print::printFloat() - real hardware's own
// default float-printing behavior whenever a game calls
// `gb.display.print()`/`println()` on a real `float` value directly, not
// through an explicit `String`/formatting call. Real algorithm: round by
// adding half a unit in the last requested decimal place, print the (now-
// rounded) integer part, then extract each decimal digit from the
// remainder in turn. `decimals` matches what a game's own real call site
// passed to `println(value, decimals)` - real Arduino's own implicit
// default when no explicit decimals argument is given (a plain
// `print(floatValue)`) is 2, matching this shim's own `gbPrintNumber()`
// convention of "the caller always states an explicit value". Promoted
// once two independently-ported games (gameAgaruino.c/gameMotoCross.c)
// both hit the identical "no float-print primitive exists" wall.
void gbPrintFloat( float value, int decimals );

// Direct port of real Gamebuino::popup()/updatePopup() (Gamebuino.cpp) - a
// small auto-dismissing bordered text box that slides in from the bottom
// edge over its final ~12 ticks, drawn on top of everything else already
// drawn that frame. Real hardware calls updatePopup() itself automatically
// at the tail of every real Gamebuino::update() (right before the
// framebuffer is sent to the display) - this shim reproduces that by
// calling it automatically from gbRenderFrame() itself, so a game only
// ever needs to call `gbPopup(text, duration)` and nothing else, matching
// real hardware's own one-call contract exactly.
void gbPopup( int* text, int duration );

// Draws one real glyph (ASCII 0-127) at (x,y) directly, in the currently
// selected font/size/color - the primitive gbPrintString() itself calls
// per character. Real Display::drawChar(x,y,c,size) takes size as its own
// 4th parameter; this shim instead reads the global gbFontSize, matching
// every other size-aware primitive here (gbPrintString/gbPrintNumber
// included) - set it before calling if you need size 2. Useful directly
// (not just via gbPrintString()) for a single non-text glyph, e.g. a
// game's own icon/digit HUD elements drawn one character at a time rather
// than as a string.
void gbDrawChar( int ch, int x, int y );

// -----------------------------------------------------------------------------
//   Sound
// -----------------------------------------------------------------------------
// A direct port of real Gamebuino Classic's own single-oscillator-per-
// channel tracker engine (Sound.h/.cpp) - notes, patterns (note sequences
// plus volume/instrument/slide/arpeggio/tremolo commands), and tracks
// (pattern sequences), across up to MAX_SOUND_CHANNELS real channels
// (0-3) - see gamebuinoShim.c's own header comment on this section for the
// full design writeup.
//
// pitch (everywhere in this API) is a direct 0-35 index into the real
// 36-entry _halfPeriods pitch table (EXTENDED_NOTE_RANGE's own real
// default of 0) - NOT a MIDI note number. duration is in real display
// frames (1 frame = 1 real gbUpdate()==true tick), scaled internally by
// gbSoundPrescaler to stay wall-clock-consistent across games configured
// at different frame rates, matching real Gamebuino::setFrameRate()'s own
// identical prescaler recompute.

// -- Patterns/tracks/instruments - the full tracker engine --

// Registers channel's own array of pattern pointers (indexed by the
// pattern-ID byte a track's own words reference) / instrument pointers
// (indexed by gbSoundCommand(GB_CMD_INSTRUMENT, id, ...)'s own X
// argument). A channel that never gets its own gbChangeInstrumentSet()
// call still has the real default square/noise pair (see
// gbInitSoundEngine()'s own doc comment in gamebuinoShim.c).
void gbChangePatternSet( int** patterns, int channel );
void gbChangeInstrumentSet( int** instruments, int channel );

// Starts a real pattern (a 0-terminated array of packed note/command
// words) on one channel - see gbPlayOK()'s own doc comment further below
// for the exact real word bit layout.
void gbPlayPattern( int* pattern, int channel );
void gbStopPattern( int channel );
void gbStopPatternAll();
void gbSetPatternLooping( bool loop, int channel );

// Starts a real track (a 0xFFFF-terminated array of packed pattern-ID +
// transposition words) on one channel - needs a real gbChangePatternSet()
// call on that same channel first.
void gbPlayTrack( int* track, int channel );
void gbStopTrack( int channel );
void gbStopTrackAll();

// Sets per-channel volume/instrument/slide/arpeggio/tremolo state, read
// back on every subsequent note played on that channel until changed
// again - real, load-bearing public API many real games call directly
// (not just from inside pattern data), e.g. a real, widely-reused
// "soundfx table" idiom shared across several already-ported games'
// own upstream source (Copter/FlappyBirdo/Bomber/shipwrek/BigBlackBox/
// Robot/etc's own `playsoundfx()`-shaped helpers): X is 0-31, Y is a
// signed -16..15 offset - matching real hardware's own exact argument
// shape.
#define GB_CMD_VOLUME     0
#define GB_CMD_INSTRUMENT 1
#define GB_CMD_SLIDE      2
#define GB_CMD_ARPEGGIO   3
#define GB_CMD_TREMOLO    4
void gbSoundCommand( int cmd, int X, int Y, int channel );

// Starts a single note on one channel directly, using whatever state that
// channel's own last gbSoundCommand() calls set.
void gbPlayNoteChannel( int pitch, int duration, int channel );
void gbStopNoteChannel( int channel );
void gbStopNoteAll();

// Internal - called automatically once per real tick from gbRenderFrame()/
// once per real game launch from gbBegin(). Not meant to be called by a
// game directly (same "automatic, not a game-facing call" contract as
// gbUpdatePopup()).
void gbRecomputeSoundPrescaler();
void gbInitSoundEngine();
void gbUpdateSoundTracker();

// -- One-shot tones - a small, separate primitive, not part of the
// tracker engine above (no instrument/pattern/track machinery, no per-
// tick stepping) --

// The common single-channel convenience form real upstream code almost
// always actually calls (channel 0) - equivalent to
// gbPlayNoteChannel(pitch, duration, 0).
void gbPlayNote( int pitch, int duration );
void gbPlayTick();
void gbPlayOK();
void gbPlayCancel();

// -----------------------------------------------------------------------------
//   Collision helpers - direct ports of Gamebuino::collide*()
// -----------------------------------------------------------------------------

bool gbCollidePointRect( int x1, int y1, int x2, int y2, int w, int h );
bool gbCollideRectRect( int x1, int y1, int w1, int h1, int x2, int y2, int w2, int h2 );

// Direct ports of real `Display::getBitmapPixel()`/`Gamebuino::
// collideBitmapBitmap()` (read from utility/Display.cpp/Gamebuino.cpp).
// `bitmap` uses this shim's own real bitmap format (bitmap[0]=width,
// bitmap[1]=height, then packed row bytes - the same format gbDrawBitmap()
// itself reads), so these two are genuine drop-in ports, not
// approximations: same bounding-rect-reject-first optimization as real
// hardware, same per-pixel AND-of-both-bitmaps overlap test.
bool gbGetBitmapPixel( int* bitmap, int x, int y );
bool gbCollideBitmapBitmap( int x1, int y1, int* b1, int x2, int y2, int* b2 );

// -----------------------------------------------------------------------------
//   Small Arduino-macro stand-ins upstream game code commonly relies on
// -----------------------------------------------------------------------------
// No ternary operator in this dialect (see the sibling project's own
// VIRCON32_C_DIALECT.md) - real functions instead of a `(a>b?a:b)` macro.

int gbMax( int a, int b );
int gbMin( int a, int b );

// Real Arduino `abs()` stand-in - used internally by gbDrawLine()'s own
// Bresenham implementation, and available directly to any game that needs
// a plain integer absolute value.
int gbAbsInt( int a );

#endif
