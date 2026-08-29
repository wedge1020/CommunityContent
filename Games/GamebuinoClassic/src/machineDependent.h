#ifndef MACHINE_DEPENDENT_H
#define MACHINE_DEPENDENT_H

// -----------------------------------------------------------------------------
// The per-port interface gamebuinoShim.h is built on top of. Bodies live in
// portVircon32.c (Vircon32 has no linker - see the sibling tinyjoypad_vircon32
// project's own VIRCON32_C_DIALECT.md section 11 - so these are plain forward
// declarations; the real definitions just need to appear somewhere later in
// the single compiled file, which they do via main.c's include order).
//
// Modeled directly on tinyjoypad_vircon32's own machineDependent.h - same
// overall shape and, as of the memory-card section below, the same feature
// set too: the thumbnail atlas/quit-confirmation dialog/pixel-grid overlay/
// global mute toggle/EEPROM persistence that project has are all ported
// here - see portVircon32.c's own comments above main() (and above
// THUMBNAILS_TEXTURE_ID) for those.
//
// Real Gamebuino Classic hardware streams its own 84x48 PCD8544 (Nokia 5110)
// display through a genuine CPU-writable framebuffer
// (`Display::_displayBuffer[]`, real random-access read/write, not a
// one-byte-at-a-time stream the way TinyJoypad's SSD1306 driver lineages
// are) - but the underlying byte layout is the same "one byte = 8 vertical
// pixels of one column" PCD8544/SSD1306-family convention either way, so the
// exact same "only 256 possible byte values, pre-bake them all as texture
// tiles" trick applies unchanged - md_drawColumn() takes that byte value
// directly and blits one of 256 pre-baked regions (see
// tools/gen_column_atlas.py) instead of writing any pixels.
// -----------------------------------------------------------------------------

#define LCD_WIDTH  84
#define LCD_HEIGHT 48
#define LCD_PAGES  6

// =============================================================================
//   VIDEO
// =============================================================================

void md_initVideo();

// clears the screen to white (a real PCD8544 LCD's own natural background
// color - a "set" pixel is dark ink drawn on top of it, not light emitted
// against dark, the opposite of the sibling tinyjoypad_vircon32 project's
// own self-illuminating OLED target - see portVircon32.c's own
// md_beginFrame() comment) - called once at the start of every game frame,
// before that frame's md_drawColumn() calls
void md_beginFrame();

// col: 0..83 (LCD x). page: 0..5 (LCD y / 8). value: the raw PCD8544 column
// byte (0-255) - a value of 0 means "all 8 pixels off" and is a no-op, since
// the frame was already cleared to the same white by md_beginFrame()
void md_drawColumn( int col, int page, int value );

// Only ever called by gbRenderFrame() when the runtime gbRealGrayColor
// toggle is on (see gamebuinoShim.h's own Configuration section) - draws a
// real, semi-transparent gray-tinted tile on top of whatever
// md_drawColumn() already drew for this same (col,page) cell, recoloring
// only the specific "on" bits `value` marks (a strict subset of that same
// cell's own md_drawColumn() value) - see assets/columns_gray.png/tools/
// gen_column_atlas_gray.py.
void md_drawColumnGray( int col, int page, int value );

// A solid-color filled rectangle in real screen pixels (not LCD columns) -
// used only by the quit-confirmation dialog below, not by any Gamebuino game
// itself. Direct port of the sibling tinyjoypad_vircon32 project's own
// md_drawSolidRect() - see portVircon32.c's own header comment on its
// definition for the "tint+scale the column atlas's own solid tile" trick
// this uses instead of a dedicated rectangle-fill asset.
void md_drawSolidRect( int x, int y, int w, int h, int color );

// Pixel size of a game's menu thumbnail (assets/thumbnails.png) - shared
// here so callers (menu.c) can lay out around it (e.g. centering it
// vertically) without duplicating the actual asset dimensions. Same
// dimensions as the sibling tinyjoypad_vircon32 project's own thumbnails -
// not tied to this project's own LCD aspect ratio, just a fixed menu-layout
// size real gameplay screenshots get scaled/cropped to fit.
#define MD_THUMBNAIL_WIDTH  256
#define MD_THUMBNAIL_HEIGHT 128

// How many games have a pre-baked gameplay thumbnail (assets/thumbnails.png,
// one 256x128 real-gameplay screenshot per game, in the same order as
// addGames()). The menu uses this to skip drawing a thumbnail for any game
// index at or past it (e.g. a newly-added game before a thumbnail exists
// for it), rather than assuming every menu entry has one.
int md_getThumbnailCount();

// Draws gameIndex's pre-baked gameplay screenshot (256x128) with its
// top-left corner at (x, y). No-op if gameIndex is out of the thumbnail
// atlas's range - callers should still gate on md_getThumbnailCount()
// first rather than relying on this no-op alone, since drawing nothing
// there is a silent no-op, not an error.
void md_drawGameThumbnail( int gameIndex, int x, int y );

// Draws the three global-toggle status badges (sound / real solid gray /
// pixel grid) in the top-right corner. Menu use only - during gameplay the
// screen belongs to the game, and the platform layer's own transient toast
// reports a change instead.
void md_drawToggleStatusIcons();

// waits for vsync (wraps time.h's end_frame())
void md_endFrame();

// =============================================================================
//   INPUT
// =============================================================================
// Real Gamebuino Classic hardware has 7 discrete digital buttons (Up/Down/
// Left/Right/A/B/C, no analog ladder to decode) - a much simpler input
// surface than TinyJoypad's own voltage-divider scheme. Mapped directly onto
// Vircon32's own D-pad + first 3 face buttons.

bool md_inputLeft();
bool md_inputRight();
bool md_inputUp();
bool md_inputDown();
bool md_inputA(); // Vircon32 Button 1
bool md_inputB(); // Vircon32 Button 2
bool md_inputC(); // Vircon32 Button 3
bool md_inputStart(); // returns to the menu, mid-game - see main()'s dispatch loop

// Call once, right when a game is (re)launched from the menu, to suppress
// md_inputA() until the confirm press that launched it is physically
// released - otherwise that same press can bleed into the game's very
// first frame (e.g. instantly dismissing its own title screen) and be
// misread as the player's own input there. Direct port of the sibling
// tinyjoypad_vircon32 project's own md_armInputFireGate() - found to be
// necessary here too via the exact same symptom that motivated it there.
void md_armInputAGate();

// Raw held-frame counters, straight from Vircon32's own gamepad_*()
// registers - see the sibling tinyjoypad_vircon32 project's own
// machineDependent.h for the full "why" (a plain "== 1" edge check silently
// misses a press landing on a throttled-out frame for any game whose logic
// ticks slower than every real frame).
int md_inputLeftFrames();
int md_inputRightFrames();
int md_inputUpFrames();
int md_inputDownFrames();
int md_inputAFrames();

#define md_recentlyPressed(framesValue, window) ( (framesValue) >= 1 && (framesValue) <= (window) )

// =============================================================================
//   AUDIO
// =============================================================================

void md_initAudio();

// Starts playing freqHz for durationSeconds on the next free hardware
// channel - see the sibling tinyjoypad_vircon32 project's own
// machineDependent.h for the full design rationale (genuinely multi-voice,
// backed by PlayNote's own free-channel picker).
void md_playTone( float freqHz, float durationSeconds );

void md_stopTone();

// advances every channel's own scheduled auto-stop - call exactly once per
// frame, regardless of which game (if any) is running
void md_updateAudio();

// A real, continuously-retunable sustained tone, for gamebuinoShim.c's own
// tracker/pattern engine - a real note's pitch/volume can change smoothly
// while it's still sounding, driven by a real instrument envelope/slide/
// arpeggio/tremolo effect, unlike md_playTone()'s own fire-and-forget,
// fixed-duration one-shot model (which isn't a fit for that). Returns the
// real SPU channel now playing freqHz at the given 0..1 volume, or -1 if
// every channel is already busy; pass that same channel to
// md_trackerVoiceRetune()/md_trackerVoiceStop() for the rest of that
// note's life. Shares the same underlying 16-channel pool as md_playTone()
// (both call into PlayNote's own playnote_start(), which always hands
// back a genuinely free channel), so tracker voices and one-shot tones
// coexist safely.
int md_trackerVoiceStart( float freqHz, float volume );

// Retunes an already-started tracker voice in place - no new attack, no
// click, matching real hardware's own continuously-updated oscillator.
void md_trackerVoiceRetune( int channel, float freqHz, float volume );

// Ends a tracker voice for good (the note itself has finished, not just a
// mid-note retune).
void md_trackerVoiceStop( int channel );

// =============================================================================
//   MEMORY CARD (backs eepromShim.h's persistent per-game EEPROM emulation)
// =============================================================================
// Thin wrappers around Vircon32's own memcard.h (card_is_connected(),
// card_read_signature()/card_write_signature()/card_signature_matches(),
// card_read_data()/card_write_data()) - kept in this machine-dependent layer
// rather than called directly from eepromShim.c, the same reasoning as every
// other Vircon32-specific primitive here. Direct port of the sibling
// tinyjoypad_vircon32 project's own md_card*() primitives. offsetWords/
// sizeWords are in words (Vircon32 ints), not bytes - matching how
// card_read_data()/card_write_data() and sizeof() already work on this
// platform project-wide.

bool md_cardIsConnected();

// true only if the connected card's own 20-word signature matches this
// project's fixed signature (see eepromShim.c) - a card written by an
// unrelated program, or a blank card, both read as false here rather than
// risking a misread of foreign data.
bool md_cardHasOurSignature();

// stamps this project's fixed signature onto the connected card - called
// once, the first time anything is ever written to a fresh/foreign card.
void md_cardWriteSignature();

void md_cardReadData( void* dest, int offsetWords, int sizeWords );
void md_cardWriteData( void* src, int offsetWords, int sizeWords );

#endif
