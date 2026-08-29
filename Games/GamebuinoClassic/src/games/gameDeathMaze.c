// DeathMaze (msevilgenius, license: none specified - recovered via direct
// download, no live GitHub repo for this specific game; the same author's
// Gamebuino-SuperSpaceShooter is already ported into this cartridge as
// gameSuperSpaceShooter.c). A tiny one-way maze runner: the player only
// ever moves right (plus up/down to dodge into gaps), racing to cross a
// randomly-generated 16-column wall maze before a real-time-decaying
// score bottoms out, with a real EEPROM-backed high score.
//
// Every real `gb.x.y(...)` call site is mechanically rewritten to a plain
// `gbY(...)` function call (this dialect has no classes/methods). `byte`/
// `int8_t`/`int16_t` all become plain `int` (avrCompat.h aliasing/this
// dialect's own single 32-bit int type). `random(N)` became `arand(N)`.
// The real `Player` struct (a single-instance `{x,y}` pair) is flattened
// into plain globals (`dmazePlayerX`/`dmazePlayerY`), matching this
// project's own established "flatten a single-instance struct into plain
// globals" precedent. The real `logo[]` PROGMEM bitmap (64x28, real
// `Display::drawBitmap()` row-major/MSB-first format) is copied verbatim
// as `dmazeLogoBitmap`, with every real AVR `B`-binary-literal byte
// converted to its decimal value directly from the source (a small
// conversion script, cross-checked against the real 226-entry array
// size - 2 header words + 64/8*28 = 224 packed bytes - to rule out a
// transcription mistake, the same approach gameSuperSpaceShooter.c's own
// header comment documents for its own identically-sized 64x28 logo).
//
// STATE MACHINE: real upstream calls the real, blocking
// `gb.titleScreen(F(" msevilgenius's"), logo)` from two genuinely
// different real call sites with two different real follow-up behaviors,
// both preserved here as two distinct states (matching gameFiremen.c's
// own established "TITLE vs PAUSED" split for the identical shape):
//   - `setup()`'s own call (boot) is immediately followed by a real
//     `reset()` once it returns - ported as DMAZE_STATE_TITLE, whose
//     Button-A dismiss calls `dmazeReset()` before entering play.
//   - `loop()`'s own real `if(gb.buttons.pressed(BTN_C))
//     gb.titleScreen(...)` (mid-game) has NO reset() call after it at
//     all - the maze, player position and score are genuinely left
//     untouched, so this real call is a pause screen, not a restart
//     gesture. Ported as DMAZE_STATE_PAUSED, whose Button-A dismiss
//     returns straight to DMAZE_STATE_PLAY with no reset.
// Both states draw the identical real logo/text layout (upstream passes
// the exact same two arguments to both calls), reusing
// gameSuperSpaceShooter.c's own already-proven real title-screen
// recreation layout for a 64x28 logo (`(LCDWIDTH-64)/2, 2` for the logo,
// then the real title text at cursor `(1,32)` and a "PRESS A" dismiss
// hint at `(1,40)` - this shim has no generic titleScreen() widget of its
// own, so every ported game recreates the real layout by hand). The
// DMAZE_STATE_PLAY handler's own Button-C check calls `dmazeBeginPaused()`
// then returns immediately for that tick, matching real hardware's own
// blocking call (no gameplay logic or drawing below that check ever runs
// on the same tick the real title screen takes over).
//
// EEPROM: real upstream's own `readBest()`/`writeBest()` (`#include
// <EEPROM.h>`) pack a 16-bit best score into 2 raw bytes at addresses 0/1
// plus a sentinel byte 42 at address 2 - a fresh/never-written cell (real
// AVR EEPROM's own genuine factory-erased state, and this shim's own
// matching default - see eepromShim.c's own header comment) reads 0xFF
// per byte, never a real 0, so `readBest()`'s own `written != 42` check
// correctly falls back to a real 0 on a genuinely fresh card regardless
// (0xFF != 42) - a correction to this comment's own earlier, inaccurate
// "fresh reads 0" claim, not a code change; the actual gate has always
// been the independent sentinel byte, not the composed score's own
// value, so it was never affected by that inaccuracy. Ported directly to
// `eeprom_read_byte()`/`eeprom_write_byte()` at the same addresses (see
// gameUfoRace.c/gameShipwrek.c for other real examples of this exact shim
// pair).
// `writeBest()` only ever runs when the just-finished score beats the
// stored best (and the best starts at a real 0), so the value written is
// always a small non-negative number well under 15 bits in every real
// playthrough - the original's own `int16_t` split/reassembly (sign bit
// included) is therefore ported as a plain unsigned 0-65535 reassembly
// with no sign handling: traced through and confirmed the sign bit can
// never actually be set for any score this game can really produce.
//
// UPSTREAM QUIRKS - traced through, not assumed:
// - The real `if (gb.buttons.pressed(BTN_RIGHT)) { ... if (!finished)
//   player.x += 1; ... }` has a redundant, always-true inner `!finished`
//   check - the whole movement block it lives inside is already wrapped
//   in the same `if (!finished)` test one level up. Confirmed to have
//   zero observable effect (the inner check can never evaluate false
//   here) and simplified away rather than reproduced literally.
// - Real `score`/`bestScore` are `int16_t` and could in principle wrap
//   around plus/minus 32768 given enough real wall-hit penalties in one
//   very long attempt (each hit is -500, starting from +10000). Ported as
//   plain (32-bit) `int` with no wraparound at all - a deliberate,
//   documented widening (matching this project's own established `byte`/
//   `int8_t`/`int16_t` -> `int` precedent throughout), not a behavior
//   upstream's own gameplay logic (`score > -10000`, `score > bestScore`)
//   actually depends on wrapping to work correctly either way.
// - There is genuinely no LEFT-button handling anywhere in real
//   upstream - this is a real, deliberate one-way maze (finishing means
//   reaching `player.x >= MAZE_WIDTH*2`), not a missing feature.
// - The real `static byte moving = 0;` (a `loop()`-local static, retained
//   across ticks but only ever initialized once, ever) has no equivalent
//   in this dialect (no `static` - VIRCON32_C_DIALECT.md confirms this
//   directly). Ported as a plain global (`dmazeMoving`) instead, which
//   behaves identically here: a global's own initializer also runs
//   exactly once, at program start, and `dmazeReset()` never touches it,
//   matching real upstream's own `reset()` (which never touches `moving`
//   either).

#define DMAZE_WIDTH 16
#define DMAZE_HEIGHT 18
#define DMAZE_PENALTY 500

#define DMAZE_STATE_TITLE 0
#define DMAZE_STATE_PLAY 1
#define DMAZE_STATE_PAUSED 2

int dmazeState = DMAZE_STATE_TITLE;

int dmazePlayerX = 0;
int dmazePlayerY = 0;

int dmazeScore = 10000;
int dmazeBestScore = 0;
bool dmazeFinished = false;

int dmazeMoving = 0;

int[DMAZE_HEIGHT] dmazeMap;

// -----------------------------------------------------------------------------
// Real upstream title-screen logo (64x28), shown via the real, blocking
// `gb.titleScreen(F(" msevilgenius's"), logo)` - see this file's own header
// comment for the byte-format/conversion notes.
// -----------------------------------------------------------------------------
int[226] dmazeLogoBitmap = {
    64, 28, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    63, 15, 193, 31, 236, 49, 255, 252, 49, 140, 3, 131, 12, 49, 85, 84,
    48, 204, 2, 131, 12, 49, 21, 84, 48, 204, 6, 195, 12, 49, 80, 84,
    48, 207, 134, 195, 15, 241, 85, 84, 48, 204, 6, 195, 12, 49, 21, 84,
    48, 204, 15, 227, 12, 49, 69, 84, 48, 204, 12, 99, 12, 49, 85, 68,
    49, 140, 12, 99, 12, 49, 85, 20, 63, 15, 216, 51, 12, 49, 255, 252,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 224, 112, 64, 252, 252, 0,
    0, 0, 224, 112, 224, 12, 192, 0, 0, 0, 240, 240, 160, 24, 192, 0,
    0, 0, 208, 177, 176, 24, 192, 0, 0, 0, 208, 177, 176, 48, 248, 0,
    0, 0, 217, 49, 176, 48, 192, 0, 0, 0, 201, 51, 248, 96, 192, 0,
    0, 0, 207, 51, 24, 224, 192, 0, 0, 0, 198, 51, 24, 192, 192, 0,
    0, 0, 198, 54, 13, 252, 252, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

// == real drawMaze() ==
void dmazeDrawMaze()
{
    gbSetColor( GB_BLACK );
    gbDrawRect( 0, 0, 76, 40 );
    gbDrawRect( 1, 1, 74, 38 );

    for( int y = 0; y < DMAZE_HEIGHT; y = y + 1 )
    {
        for( int x = 0; x < DMAZE_WIDTH; x = x + 1 )
        {
            if( ( dmazeMap[ y ] >> ( DMAZE_WIDTH - ( x + 1 ) ) ) & 1 )
              gbDrawRect( ( x + 1 ) * 4, ( y + 1 ) * 2, 2, 2 );
        }
    }
}

// == real drawScores() ==
void dmazeDrawScores()
{
    gbSetColor( GB_BLACK );
    gbCursorX = 3;
    gbCursorY = LCDHEIGHT - 6;
    gbPrintString( "SCORE:" );
    gbPrintNumber( dmazeScore );
    gbPrintString( " BEST:" );
    gbPrintNumber( dmazeBestScore );
}

// == real drawPlayer(dontFlash) ==
void dmazeDrawPlayer( bool dontFlash )
{
    int x = dmazePlayerX * 2 + 2;
    int y = dmazePlayerY * 2 + 2;

    if( dontFlash || ( ( gbFrameCount / 2 ) % 2 ) )
    {
        gbSetColor( GB_BLACK );
        gbDrawRect( x, y, 2, 2 );
    }
}

// == real testClear(x,y) ==
bool dmazeTestClear( int x, int y )
{
    return !( ( dmazeMap[ y ] >> ( DMAZE_WIDTH - ( x + 1 ) ) ) & 1 );
}

// == real randomiseMaze() ==
void dmazeRandomiseMaze()
{
    // clear the map (make all the walls solid with no gaps)
    for( int i = 0; i < DMAZE_HEIGHT; i = i + 1 )
      dmazeMap[ i ] = 0xFFFF;

    // work across adding gaps as we go
    for( int i = DMAZE_WIDTH - 1; i >= 0; i = i - 1 )
    {
        int noOfGaps;

        // how many gaps in the column?
        int roll = arand( 10 );
        if( roll == 9 )
          noOfGaps = 3;
        else if( roll == 8 || roll == 7 )
          noOfGaps = 2;
        else
          noOfGaps = 1;

        // real upstream's own comment, verbatim: "excuse my crazy method
        // for this. the first line is a little confusing even to me. It
        // makes a number like: 1111111111011111 with the 0 corresponding
        // to the column we are modifying, then in the loop we bitwise AND
        // that with a row in the maze to create a gap in the wall"
        int row = ~( 1 << i );
        for( int j = 0; j < noOfGaps; j = j + 1 )
        {
            int gapRow = arand( DMAZE_HEIGHT );
            dmazeMap[ gapRow ] = dmazeMap[ gapRow ] & row;
        }
    }
}

// == real readBest() ==
int dmazeReadBest()
{
    int a = eeprom_read_byte( 0 );
    int b = eeprom_read_byte( 1 );
    int written = eeprom_read_byte( 2 );

    // does the eeprom contain a score?
    if( written != 42 )
      return 0;

    return b | ( a << 8 );
}

// == real writeBest(best) ==
void dmazeWriteBest( int best )
{
    eeprom_write_byte( 0, ( best >> 8 ) & 0xFF );
    eeprom_write_byte( 1, best & 0xFF );
    // flag for testing if score exists
    eeprom_write_byte( 2, 42 );
}

// == real reset() ==
void dmazeReset()
{
    dmazeRandomiseMaze();
    dmazePlayerX = 0;
    dmazePlayerY = DMAZE_HEIGHT / 2;
    dmazeScore = 10000;
    dmazeBestScore = dmazeReadBest();
    dmazeFinished = false;
}

void dmazeBeginPlay()
{
    dmazeState = DMAZE_STATE_PLAY;
}

void dmazeBeginPaused()
{
    dmazeState = DMAZE_STATE_PAUSED;
}

// Shared real titleScreen(F(" msevilgenius's"), logo) layout, reused by
// both real call sites - see this file's own header comment for why the
// anchors match gameSuperSpaceShooter.c's own already-proven real 64x28
// title-screen recreation exactly (same author, same real logo size).
void dmazeDrawTitleScreen()
{
    gbSetColor( GB_BLACK );
    gbDrawBitmap( ( LCDWIDTH - 64 ) / 2, 2, dmazeLogoBitmap );
    gbCursorX = 1;
    gbCursorY = 32;
    gbPrintString( " msevilgenius's" );
    gbCursorX = 1;
    gbCursorY = 40;
    gbPrintString( "PRESS A" );
}

// == real setup()'s own gb.titleScreen(...) call, boot case - dismissing
// runs the real reset() that followed it in setup() ==
void dmazeUpdateTitle()
{
    dmazeDrawTitleScreen();

    if( gbPressed( BTN_A ) )
    {
        dmazeReset();
        dmazeBeginPlay();
    }
}

// == real loop()'s own mid-game gb.titleScreen(...) call (Button C) - a
// real pause screen, dismissing resumes play with no reset at all ==
void dmazeUpdatePaused()
{
    dmazeDrawTitleScreen();

    if( gbPressed( BTN_A ) )
      dmazeBeginPlay();
}

void dmazeUpdatePlay()
{
    if( dmazeMoving )
      dmazeMoving = dmazeMoving - 1;

    // have we finished the maze?
    if( !dmazeFinished && dmazePlayerX >= DMAZE_WIDTH * 2 )
    {
        dmazeFinished = true;
        if( dmazeScore > dmazeBestScore )
        {
            dmazeBestScore = dmazeScore;
            dmazeWriteBest( dmazeBestScore );
        }
    }

    // C to return to (pause at) the title screen - real upstream's own
    // blocking titleScreen() call means nothing below this runs on the
    // same tick the real call would have taken over
    if( gbPressed( BTN_C ) )
    {
        dmazeBeginPaused();
        return;
    }

    // B to reset anytime
    if( gbPressed( BTN_B ) )
      dmazeReset();

    // press a button to reset when finished
    if( dmazeFinished && gbPressed( BTN_A ) )
      dmazeReset();

    if( dmazeScore > -10000 && !dmazeFinished )
      dmazeScore = dmazeScore - 5; // decrease score every frame

    // movement (only if we haven't got to the end)
    if( !dmazeFinished )
    {
        if( gbPressed( BTN_UP ) )
        {
            dmazeMoving = 6;
            if( dmazePlayerY > 0 && ( !( dmazePlayerX % 2 ) || dmazeTestClear( ( dmazePlayerX - 1 ) / 2, dmazePlayerY - 1 ) ) )
              dmazePlayerY = dmazePlayerY - 1;
            else
              dmazeScore = dmazeScore - DMAZE_PENALTY;
        }
        if( gbPressed( BTN_DOWN ) )
        {
            dmazeMoving = 6;
            if( dmazePlayerY + 1 < DMAZE_HEIGHT && ( !( dmazePlayerX % 2 ) || dmazeTestClear( ( dmazePlayerX - 1 ) / 2, dmazePlayerY + 1 ) ) )
              dmazePlayerY = dmazePlayerY + 1;
            else
              dmazeScore = dmazeScore - DMAZE_PENALTY;
        }
        if( gbPressed( BTN_RIGHT ) )
        {
            dmazeMoving = 6;
            if( ( dmazePlayerX % 2 ) || dmazeTestClear( dmazePlayerX / 2, dmazePlayerY ) )
              dmazePlayerX = dmazePlayerX + 1;
            else
              dmazeScore = dmazeScore - DMAZE_PENALTY;
        }
    }

    // drawing functions
    dmazeDrawMaze();
    dmazeDrawScores();
    dmazeDrawPlayer( dmazeMoving || dmazeFinished );
}

void gameDeathMaze_init()
{
    gbBegin();
    gbSetFont( gbFont3x5 ); // matches real upstream's own gb.display.setFont(font3x5) - already this shim's own default too
    gbPickRandomSeed(); // documented no-op - see gamebuinoShim.h
    dmazeState = DMAZE_STATE_TITLE;
}

void gameDeathMaze_update()
{
    if( !gbUpdate() ) return;

    if( dmazeState == DMAZE_STATE_TITLE )
      dmazeUpdateTitle();
    else if( dmazeState == DMAZE_STATE_PAUSED )
      dmazeUpdatePaused();
    else
      dmazeUpdatePlay();

    gbRenderFrame();
}
