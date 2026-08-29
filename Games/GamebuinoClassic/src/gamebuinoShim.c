#include "gamebuinoShim.h"
#include "machineDependent.h"
#include "avrCompat.h"

// -----------------------------------------------------------------------------
// Font tables - real Gamebuino Classic bitmap fonts, ported directly from
// the real library's own utility/font5x7.c/font3x5.c/font3x3.c (column-
// major, LSB = top pixel, one array of `width` bytes per glyph, indexed
// directly by ASCII code 0-127 - the same custom low-range icon glyphs
// (heart/spade/sad/happy face/etc, replacing the usual unprintable control-
// character range 0-31) as real hardware, followed by standard printable
// ASCII from 32 up). font3x5 is real hardware's own default (gbBegin() sets
// it here, matching Display::Display()'s own real `setFont(font3x5)` call);
// font5x7 is the larger size several ported games (gameConduit.c,
// gameFlappyBirdo.c) switch to for titles/headings via a real setFont()
// call; font3x3 is included for API completeness even though no
// currently-ported game asks for it.
// -----------------------------------------------------------------------------

int[642] gbFont5x7 =
{
    5, 7, // raw glyph width/height (5x7) - gbSetFont() adds +1 to each for real inter-char/line spacing, matching upstream
    127, 65, 65, 65, 127, 62, 91, 79, 91, 62, 62, 107, 79, 107, 62, 24,
    60, 120, 60, 24, 24, 60, 126, 60, 24, 28, 87, 125, 87, 28, 28, 94,
    127, 94, 28, 0, 126, 67, 67, 126, 0, 126, 115, 115, 126, 0, 126, 127,
    127, 126, 127, 65, 65, 65, 127, 48, 72, 58, 6, 14, 6, 41, 121, 41,
    6, 127, 65, 65, 65, 127, 96, 112, 63, 2, 4, 42, 28, 54, 28, 42,
    127, 62, 28, 8, 0, 0, 8, 28, 62, 127, 20, 34, 127, 34, 20, 60,
    60, 60, 126, 255, 24, 66, 60, 129, 126, 62, 99, 117, 99, 62, 62, 97,
    107, 99, 62, 62, 99, 107, 107, 62, 4, 2, 127, 2, 4, 16, 32, 127,
    32, 16, 8, 8, 42, 28, 8, 8, 28, 42, 8, 8, 20, 62, 85, 65,
    34, 99, 117, 105, 117, 99, 16, 24, 28, 24, 16, 4, 12, 28, 12, 4,
    0, 0, 0, 0, 0, 0, 0, 95, 0, 0, 0, 7, 0, 7, 0, 20,
    127, 20, 127, 20, 4, 42, 127, 42, 16, 35, 19, 8, 100, 98, 54, 73,
    86, 32, 80, 0, 8, 7, 3, 0, 0, 28, 34, 65, 0, 0, 65, 34,
    28, 0, 42, 28, 62, 28, 42, 8, 8, 62, 8, 8, 0, 224, 96, 0,
    0, 8, 8, 8, 8, 8, 0, 96, 96, 0, 0, 96, 16, 8, 4, 3,
    62, 81, 73, 69, 62, 0, 66, 127, 64, 0, 98, 81, 73, 73, 70, 33,
    65, 73, 77, 51, 24, 20, 18, 127, 16, 39, 69, 69, 69, 57, 60, 74,
    73, 73, 48, 65, 33, 17, 9, 7, 54, 73, 73, 73, 54, 6, 73, 73,
    41, 30, 0, 0, 36, 0, 0, 0, 128, 100, 0, 0, 8, 20, 34, 65,
    0, 20, 20, 20, 20, 20, 0, 65, 34, 20, 8, 2, 1, 89, 9, 6,
    62, 65, 93, 89, 78, 124, 18, 17, 18, 124, 127, 73, 73, 73, 54, 62,
    65, 65, 65, 34, 127, 65, 65, 34, 28, 127, 73, 73, 73, 65, 127, 9,
    9, 9, 1, 62, 65, 65, 81, 50, 127, 8, 8, 8, 127, 0, 65, 127,
    65, 0, 32, 65, 65, 63, 1, 127, 8, 20, 34, 65, 127, 64, 64, 64,
    64, 127, 2, 12, 2, 127, 127, 2, 4, 8, 127, 62, 65, 65, 65, 62,
    127, 9, 9, 9, 6, 62, 65, 97, 65, 190, 127, 9, 25, 41, 70, 38,
    73, 73, 73, 50, 1, 1, 127, 1, 1, 63, 64, 64, 64, 63, 31, 32,
    64, 32, 31, 63, 64, 56, 64, 63, 99, 20, 8, 20, 99, 3, 4, 120,
    4, 3, 97, 81, 73, 69, 67, 0, 127, 65, 65, 0, 3, 4, 8, 16,
    96, 0, 65, 65, 127, 0, 4, 2, 1, 2, 4, 128, 128, 128, 128, 128,
    0, 3, 7, 8, 0, 32, 84, 84, 120, 64, 127, 40, 68, 68, 56, 56,
    68, 68, 68, 40, 56, 68, 68, 40, 127, 56, 84, 84, 84, 24, 8, 126,
    9, 9, 2, 24, 164, 164, 156, 120, 127, 8, 4, 4, 120, 0, 68, 125,
    64, 0, 0, 96, 128, 132, 125, 127, 16, 16, 40, 68, 0, 65, 127, 64,
    0, 124, 4, 120, 4, 120, 124, 8, 4, 4, 120, 56, 68, 68, 68, 56,
    252, 24, 36, 36, 24, 24, 36, 36, 24, 252, 124, 8, 4, 4, 8, 8,
    84, 84, 84, 32, 4, 63, 68, 68, 32, 60, 64, 64, 32, 124, 28, 32,
    64, 32, 28, 60, 64, 32, 64, 60, 68, 40, 16, 40, 68, 76, 144, 144,
    144, 124, 68, 100, 84, 76, 68, 0, 8, 54, 65, 0, 0, 0, 127, 0,
    0, 0, 65, 54, 8, 0, 24, 4, 8, 16, 12, 127, 65, 65, 65, 127,
};

int[386] gbFont3x5 =
{
    3, 5, // raw glyph width/height (3x5) - gbSetFont() adds +1 to each for real inter-char/line spacing, matching upstream
    63, 33, 63, 18, 8, 18, 10, 16, 10, 14, 28, 14, 12, 30, 12, 20,
    26, 20, 22, 31, 22, 30, 19, 30, 30, 27, 30, 30, 31, 30, 63, 33,
    63, 58, 47, 58, 23, 61, 23, 63, 33, 63, 24, 31, 2, 4, 10, 4,
    31, 14, 4, 4, 14, 31, 10, 31, 10, 14, 14, 31, 4, 17, 14, 46,
    37, 46, 47, 42, 46, 38, 41, 41, 2, 31, 2, 8, 31, 8, 21, 14,
    4, 4, 14, 21, 14, 21, 21, 27, 21, 27, 4, 6, 4, 4, 12, 4,
    0, 0, 0, 0, 23, 0, 3, 0, 3, 31, 10, 31, 22, 55, 26, 25,
    4, 19, 10, 21, 58, 0, 3, 0, 0, 14, 17, 17, 14, 0, 10, 4,
    10, 4, 14, 4, 0, 48, 0, 4, 4, 4, 0, 16, 0, 24, 4, 3,
    31, 17, 31, 18, 31, 16, 29, 21, 23, 17, 21, 31, 7, 4, 31, 23,
    21, 29, 31, 21, 29, 1, 1, 31, 31, 21, 31, 23, 21, 31, 0, 10,
    0, 0, 50, 0, 4, 10, 17, 10, 10, 10, 17, 10, 4, 1, 21, 2,
    14, 17, 23, 30, 5, 30, 31, 21, 10, 14, 17, 10, 31, 17, 14, 31,
    21, 17, 31, 5, 1, 14, 17, 29, 31, 4, 31, 17, 31, 17, 8, 16,
    15, 31, 4, 27, 31, 16, 16, 31, 6, 31, 30, 4, 15, 14, 17, 14,
    31, 9, 6, 14, 17, 46, 31, 5, 26, 18, 21, 9, 1, 31, 1, 31,
    16, 31, 15, 24, 15, 31, 12, 31, 27, 4, 27, 3, 28, 3, 25, 21,
    19, 0, 31, 17, 3, 4, 24, 17, 31, 0, 2, 1, 2, 32, 32, 32,
    0, 1, 2, 12, 18, 30, 31, 18, 12, 12, 18, 18, 12, 18, 31, 12,
    26, 20, 4, 30, 5, 36, 42, 30, 31, 2, 28, 20, 29, 16, 32, 32,
    29, 31, 8, 20, 17, 31, 16, 30, 4, 30, 30, 2, 28, 12, 18, 12,
    62, 10, 4, 12, 18, 62, 30, 4, 2, 20, 22, 10, 2, 15, 18, 14,
    16, 30, 14, 16, 14, 30, 8, 30, 18, 12, 18, 38, 40, 30, 50, 42,
    38, 4, 30, 33, 0, 31, 0, 33, 30, 4, 1, 2, 1, 63, 33, 63,
};

int[386] gbFont3x3 =
{
    3, 3, // raw glyph width/height (3x3) - gbSetFont() adds +1 to each for real inter-char/line spacing, matching upstream
    7, 5, 7, 5, 4, 5, 5, 4, 5, 3, 6, 3, 2, 7, 2, 2,
    1, 2, 2, 3, 2, 14, 9, 14, 14, 13, 14, 14, 15, 14, 7, 5,
    7, 5, 3, 7, 5, 15, 5, 7, 5, 7, 6, 7, 1, 2, 5, 2,
    0, 7, 2, 2, 7, 0, 2, 7, 2, 0, 2, 7, 2, 0, 7, 6,
    3, 6, 7, 7, 2, 2, 5, 5, 2, 1, 2, 2, 4, 2, 0, 5,
    2, 2, 5, 0, 6, 7, 5, 5, 7, 5, 2, 3, 2, 2, 6, 2,
    0, 0, 0, 0, 11, 0, 3, 0, 3, 7, 5, 7, 4, 7, 1, 5,
    2, 5, 7, 7, 4, 0, 3, 0, 0, 7, 5, 5, 7, 0, 5, 0,
    5, 2, 7, 2, 0, 12, 0, 2, 2, 2, 0, 4, 0, 4, 2, 1,
    7, 5, 7, 5, 7, 4, 1, 7, 4, 5, 7, 2, 3, 2, 7, 4,
    7, 1, 7, 6, 6, 1, 1, 7, 7, 7, 7, 3, 3, 7, 0, 5,
    0, 0, 13, 0, 2, 5, 0, 5, 5, 5, 0, 5, 2, 1, 7, 3,
    15, 9, 3, 6, 3, 6, 7, 7, 2, 2, 5, 5, 7, 7, 2, 7,
    7, 5, 7, 3, 1, 7, 5, 6, 7, 2, 7, 5, 7, 5, 4, 4,
    3, 7, 2, 5, 7, 4, 4, 7, 3, 7, 7, 1, 6, 7, 5, 7,
    7, 3, 3, 7, 5, 3, 7, 3, 6, 4, 7, 1, 1, 7, 1, 7,
    4, 7, 3, 4, 3, 7, 6, 7, 5, 2, 5, 1, 6, 1, 1, 7,
    4, 0, 7, 5, 1, 2, 4, 5, 7, 0, 2, 1, 2, 8, 8, 8,
    0, 3, 0, 6, 3, 6, 7, 7, 2, 2, 5, 5, 7, 5, 2, 7,
    7, 5, 7, 3, 1, 7, 5, 6, 7, 2, 7, 5, 7, 5, 4, 4,
    3, 7, 2, 5, 7, 4, 4, 7, 3, 7, 7, 1, 6, 7, 5, 7,
    7, 3, 3, 7, 5, 3, 7, 3, 6, 4, 7, 1, 1, 7, 1, 7,
    4, 7, 3, 4, 3, 7, 6, 7, 5, 2, 5, 1, 6, 1, 1, 7,
    4, 2, 7, 5, 0, 7, 0, 5, 7, 2, 6, 2, 3, 7, 5, 7,
};
// -----------------------------------------------------------------------------
// Small helpers
// -----------------------------------------------------------------------------

int gbMax( int a, int b )
{
    if( a > b ) return a;
    return b;
}

int gbMin( int a, int b )
{
    if( a < b ) return a;
    return b;
}

int gbAbsInt( int a )
{
    if( a < 0 ) return -a;
    return a;
}

// A plain 2^x helper for gbPlayNote()'s own pitch->frequency formula (no
// pow()/exp2() available - see this dialect's own math.h surface) -
// computed via exp()/ln(2), both of which this dialect's math.h does
// provide as real hardware calls.
float gbPow2( float x )
{
    return exp( x * 0.6931471805599453 );
}


// -----------------------------------------------------------------------------
// Framebuffer + primitive drawing - a genuine CPU-writable buffer (matching
// real Display::_displayBuffer[]'s own exact addressing formula:
// buf[x + (y/8)*LCDWIDTH], bit (y%8)), unlike the sibling tinyjoypad_vircon32
// project's own byte-stream-only games - see this file's own header comment.
// -----------------------------------------------------------------------------

int[ LCD_WIDTH * LCD_PAGES ] gbFrameBuffer;
int gbColor = 1; // 1 = black/on (matches real Gamebuino's own BLACK=1 default)

// Runtime toggle for GB_GRAY's own rendering mode (see gamebuinoShim.h's
// own Configuration section) - default off (real hardware's own
// checkerboard-dither behavior). Bound to Button R and flipped by
// portVircon32.c's own dispatch loop, same as pixelGridEnabled/audioMuted.
bool gbRealGrayColor = false;

// A second bit plane, addressed identically to gbFrameBuffer, marking
// exactly which "on" bits were drawn with GB_GRAY specifically while
// gbRealGrayColor is on (a strict subset of gbFrameBuffer's own "on"
// bits). Every drawing primitive clears the corresponding gbGrayBuffer
// bit(s) whenever it draws ANY other color while gbRealGrayColor is on, so
// a later BLACK/WHITE/INVERT draw over a previously-gray pixel correctly
// "un-grays" that exact pixel without disturbing any of its neighbors
// still marked gray - genuine per-pixel accuracy, not a whole-byte-column
// approximation. gbRenderFrame() reads this as a second, targeted draw
// pass (see md_drawColumnGray()) drawn on top of the normal black/white
// pass, using a real semi-transparent gray-tinted atlas
// (assets/columns_gray.png) so only the actual gray pixels are recolored,
// leaving true black pixels in the same byte untouched underneath.
int[ LCD_WIDTH * LCD_PAGES ] gbGrayBuffer;

// Set whenever any drawing primitive actually draws GB_GRAY with
// gbRealGrayColor on this frame, checked once at the top of
// gbRenderFrame()'s own second pass to skip the whole 504-cell
// gbGrayBuffer scan entirely on any frame/game that never draws gray with
// the toggle on at all - always false while gbRealGrayColor itself is off,
// so turning the toggle off costs neither this scan nor gbClear()'s own
// gbGrayBuffer-clear loop below, only the one already-paid-for
// gbRealGrayColor check every gray-drawing primitive already makes.
// Reset by gbClear() every tick, same as gbGrayBuffer itself.
bool gbAnyGrayDrawn;

// LCD-persistence rendering, the third GB_GRAY mode (see gamebuinoShim.h's
// own Configuration section). Real hardware's own PCD8544 panel does not
// switch a pixel instantly: a pixel toggled on and off on alternating ticks
// never fully reaches either state, so the checkerboard dither is seen as a
// steady middle shade rather than as a flicker. Reproduced the way the
// Simbuino emulator's own LcdDevice does it - integrate each pixel's own
// on-time, average it against the previous refresh's, and quantize to three
// levels: fully on, fully off, or in between. Here the "integral" is exactly
// one rendered tick per sample, so the average of two consecutive ticks has
// only three possible values, and the whole model collapses to two bitwise
// ops on the existing byte columns:
//
//   on in BOTH ticks       -> black   (cur & prev)
//   on in exactly ONE tick -> gray    (cur ^ prev)
//   on in NEITHER          -> white   (the cleared background)
//
// so it reuses gbRenderFrame()'s own two existing passes unchanged, just fed
// different byte values - no per-pixel work, and no third atlas. Unlike
// gbRealGrayColor this is not limited to GB_GRAY: anything a game itself
// flickers on alternating ticks smears the same way real hardware smears it.
bool gbLcdPersistence = false;

// The previous rendered tick's own gbFrameBuffer, the second sample the
// average above needs. Only maintained while gbLcdPersistence is on.
int[ LCD_WIDTH * LCD_PAGES ] gbPrevFrameBuffer;

// False whenever gbPrevFrameBuffer holds nothing meaningful - before the
// first tick after the mode is switched on, and after any tick rendered
// with it off. gbRenderFrame() primes the buffer from the current frame
// instead of averaging against stale content (which would smear a previous
// game's own last frame, or a full screen of gray, into the first tick).
bool gbLcdPersistencePrimed = false;

// Real hardware's own `bgcolor` - only ever read by gbDrawChar() below,
// matching real Display::drawChar()'s own real bgcolor check exactly (not
// used by gbFillRect()/gbDrawBitmap()/etc - real bgcolor genuinely only
// affects text). Real single-arg `setColor(c)` sets BOTH `color` and
// `bgcolor` to `c` (see real Display.cpp), which is why plain gbSetColor()
// below sets both too - that keeps every already-ported game's own
// existing "print transparent, on-color-only text" behavior exactly as it
// was, since `bgcolor == color` skips the off-bit draw entirely (see
// gbDrawChar()). Only a real two-arg `setColor(color, bg)` call
// (gbSetColorBg()) can make them diverge, opting into a real opaque
// background fill.
int gbBgColor = 1;

void gbSetColor( int color )
{
    gbColor = color;
    gbBgColor = color;
}

// Direct port of real Display::setColor(color, bg) (the two-argument
// overload) - `bg` different from `color` gives a printed glyph's own
// "off" pixels a real opaque background instead of staying transparent
// (see gbDrawChar()'s own use of `gbBgColor`).
void gbSetColorBg( int color, int bg )
{
    gbColor = color;
    gbBgColor = bg;
}

// Real GRAY checkerboard-dither test, ported directly from real
// `Display::drawPixel()`'s own formula (utility/Display.h): a pixel draws
// BLACK when `(gbFrameCount&1)` differs from `(x&1)^(y&1)`, WHITE
// otherwise - a genuine two-pixel-period spatial dither that also flips
// every other real tick (via `gbFrameCount`), matching real hardware's own
// default GRAY behavior exactly.
bool gbGrayIsBlack( int x, int y )
{
    return ( gbFrameCount & 1 ) != ( ( x & 1 ) ^ ( y & 1 ) );
}

void gbDrawPixel( int x, int y )
{
    if( x < 0 || x >= LCD_WIDTH || y < 0 || y >= LCD_HEIGHT )
      return;

    int idx = x + ( y / 8 ) * LCD_WIDTH;
    int bit = 1 << ( y % 8 );

    // gbAnyGrayDrawn gates this: skip entirely until something has actually
    // been drawn real gray THIS frame (gbGrayBuffer is provably all-zero
    // before that point, so there's nothing to un-gray yet) - see its own
    // comment above. Always false for a game that never draws GB_GRAY at
    // all, so this line costs nothing there even with gbRealGrayColor on.
    if( gbRealGrayColor && gbAnyGrayDrawn )
      gbGrayBuffer[ idx ] = gbGrayBuffer[ idx ] & ( 0xFF - bit );

    if( gbColor == 0 )
      gbFrameBuffer[ idx ] = gbFrameBuffer[ idx ] & ( 0xFF - bit );
    else if( gbColor == GB_INVERT )
      gbFrameBuffer[ idx ] = gbFrameBuffer[ idx ] ^ bit;
    else if( gbColor == GB_GRAY )
    {
        if( gbRealGrayColor )
        {
            gbFrameBuffer[ idx ] = gbFrameBuffer[ idx ] | bit;
            gbGrayBuffer[ idx ] = gbGrayBuffer[ idx ] | bit;
            gbAnyGrayDrawn = true;
        }
        else if( gbGrayIsBlack( x, y ) )
          gbFrameBuffer[ idx ] = gbFrameBuffer[ idx ] | bit;
        else
          gbFrameBuffer[ idx ] = gbFrameBuffer[ idx ] & ( 0xFF - bit );
    }
    else
      gbFrameBuffer[ idx ] = gbFrameBuffer[ idx ] | bit;
}

int gbGetPixel( int x, int y )
{
    if( x < 0 || x >= LCD_WIDTH || y < 0 || y >= LCD_HEIGHT )
      return 0;

    int idx = x + ( y / 8 ) * LCD_WIDTH;
    return ( gbFrameBuffer[ idx ] >> ( y % 8 ) ) & 1;
}

void gbClear()
{
    int i;
    for( i = 0; i < LCD_WIDTH * LCD_PAGES; i++ )
      gbFrameBuffer[ i ] = 0;

    // Only re-clear gbGrayBuffer if anything actually set a bit in it last
    // frame - already all-zero otherwise, which is always true while
    // gbRealGrayColor is off (gbAnyGrayDrawn is never set while it's off),
    // so toggling real gray color off costs this loop nothing either.
    if( gbAnyGrayDrawn )
      for( i = 0; i < LCD_WIDTH * LCD_PAGES; i++ )
        gbGrayBuffer[ i ] = 0;
    gbAnyGrayDrawn = false;
}

// Direct port of real Display::fillScreen(color) - including a real,
// confirmed hardware bug: the real function's own body is literally just
// `memset(_displayBuffer, 0xFFFF, LCDWIDTH*LCDHEIGHT/8)` - the `color`
// parameter is accepted but never actually read, so real
// `fillScreen(WHITE)`/`fillScreen(INVERT)`/etc all paint the screen solid
// BLACK regardless of what's passed, same as `fillScreen(BLACK)` would.
// `color` is kept as a parameter (not dropped) so call sites stay a
// literal match for real `gb.display.fillScreen(...)` calls.
void gbFillScreen( int color )
{
    color = color; // unused - matches real hardware's own dead parameter

    int i;
    for( i = 0; i < LCD_WIDTH * LCD_PAGES; i++ )
      gbFrameBuffer[ i ] = 0xFF;
}

// A Vircon32-specific performance shape: pixel output is identical to a
// naive "call gbDrawPixel() per pixel" loop (real hardware's own shipped
// Display::drawFastHLine() is exactly that naive per-pixel loop, since a
// real AVR's own per-call cost is small). Vircon32's own documented cost
// model is the opposite: ~1 instruction = 1 cycle, but every function call
// costs a flat ~10+ instructions of overhead regardless of how little work
// it does, and IDIV/IMOD are already as cheap as a shift (no benefit from
// avoiding them) - so the one real win here is eliminating the
// once-per-pixel gbDrawPixel() call, not the arithmetic inside it. y/the
// fixed page+bit for this whole horizontal run are computed once, then
// every pixel is a single inlined array read-modify-write - same clipping,
// same bit layout as gbDrawPixel().
void gbDrawFastHLine( int x, int y, int w )
{
    if( y < 0 || y >= LCD_HEIGHT ) return;

    int x0 = x;
    int x1 = x + w;
    if( x0 < 0 ) x0 = 0;
    if( x1 > LCD_WIDTH ) x1 = LCD_WIDTH;

    int rowBase = ( y / 8 ) * LCD_WIDTH;
    int bit = 1 << ( y % 8 );
    int mask = 0xFF - bit;
    int i;

    if( gbColor == 0 )
    {
        if( gbRealGrayColor && gbAnyGrayDrawn )
          for( i = x0; i < x1; i++ )
          {
              gbGrayBuffer[ rowBase + i ] = gbGrayBuffer[ rowBase + i ] & mask;
              gbFrameBuffer[ rowBase + i ] = gbFrameBuffer[ rowBase + i ] & mask;
          }
        else
          for( i = x0; i < x1; i++ )
            gbFrameBuffer[ rowBase + i ] = gbFrameBuffer[ rowBase + i ] & mask;
    }
    else if( gbColor == GB_INVERT )
    {
        if( gbRealGrayColor && gbAnyGrayDrawn )
          for( i = x0; i < x1; i++ )
          {
              gbGrayBuffer[ rowBase + i ] = gbGrayBuffer[ rowBase + i ] & mask;
              gbFrameBuffer[ rowBase + i ] = gbFrameBuffer[ rowBase + i ] ^ bit;
          }
        else
          for( i = x0; i < x1; i++ )
            gbFrameBuffer[ rowBase + i ] = gbFrameBuffer[ rowBase + i ] ^ bit;
    }
    else if( gbColor == GB_GRAY )
    {
        if( gbRealGrayColor )
        {
            gbAnyGrayDrawn = true;
            for( i = x0; i < x1; i++ )
            {
                gbFrameBuffer[ rowBase + i ] = gbFrameBuffer[ rowBase + i ] | bit;
                gbGrayBuffer[ rowBase + i ] = gbGrayBuffer[ rowBase + i ] | bit;
            }
        }
        else
          for( i = x0; i < x1; i++ )
          {
              if( gbGrayIsBlack( i, y ) )
                gbFrameBuffer[ rowBase + i ] = gbFrameBuffer[ rowBase + i ] | bit;
              else
                gbFrameBuffer[ rowBase + i ] = gbFrameBuffer[ rowBase + i ] & mask;
          }
    }
    else
    {
        if( gbRealGrayColor && gbAnyGrayDrawn )
          for( i = x0; i < x1; i++ )
          {
              gbGrayBuffer[ rowBase + i ] = gbGrayBuffer[ rowBase + i ] & mask;
              gbFrameBuffer[ rowBase + i ] = gbFrameBuffer[ rowBase + i ] | bit;
          }
        else
          for( i = x0; i < x1; i++ )
            gbFrameBuffer[ rowBase + i ] = gbFrameBuffer[ rowBase + i ] | bit;
    }
}

// Same Vircon32-specific rewrite as gbDrawFastHLine() above, same identical
// pixel output as the naive per-pixel version it replaces - here the byte
// index and bit both still change every step (a vertical run crosses LCD
// pages), so only the gbDrawPixel() call itself is eliminated, not the
// per-pixel div/mod (already free on this ISA either way).
void gbDrawFastVLine( int x, int y, int h )
{
    if( x < 0 || x >= LCD_WIDTH ) return;

    int y0 = y;
    int y1 = y + h;
    if( y0 < 0 ) y0 = 0;
    if( y1 > LCD_HEIGHT ) y1 = LCD_HEIGHT;

    int i, idx, bit;

    if( gbColor == 0 )
    {
        if( gbRealGrayColor && gbAnyGrayDrawn )
          for( i = y0; i < y1; i++ )
          {
              idx = x + ( i / 8 ) * LCD_WIDTH;
              bit = 1 << ( i % 8 );
              gbGrayBuffer[ idx ] = gbGrayBuffer[ idx ] & ( 0xFF - bit );
              gbFrameBuffer[ idx ] = gbFrameBuffer[ idx ] & ( 0xFF - bit );
          }
        else
          for( i = y0; i < y1; i++ )
          {
              idx = x + ( i / 8 ) * LCD_WIDTH;
              bit = 1 << ( i % 8 );
              gbFrameBuffer[ idx ] = gbFrameBuffer[ idx ] & ( 0xFF - bit );
          }
    }
    else if( gbColor == GB_INVERT )
    {
        if( gbRealGrayColor && gbAnyGrayDrawn )
          for( i = y0; i < y1; i++ )
          {
              idx = x + ( i / 8 ) * LCD_WIDTH;
              bit = 1 << ( i % 8 );
              gbGrayBuffer[ idx ] = gbGrayBuffer[ idx ] & ( 0xFF - bit );
              gbFrameBuffer[ idx ] = gbFrameBuffer[ idx ] ^ bit;
          }
        else
          for( i = y0; i < y1; i++ )
          {
              idx = x + ( i / 8 ) * LCD_WIDTH;
              bit = 1 << ( i % 8 );
              gbFrameBuffer[ idx ] = gbFrameBuffer[ idx ] ^ bit;
          }
    }
    else if( gbColor == GB_GRAY )
    {
        if( gbRealGrayColor )
        {
            gbAnyGrayDrawn = true;
            for( i = y0; i < y1; i++ )
            {
                idx = x + ( i / 8 ) * LCD_WIDTH;
                bit = 1 << ( i % 8 );
                gbFrameBuffer[ idx ] = gbFrameBuffer[ idx ] | bit;
                gbGrayBuffer[ idx ] = gbGrayBuffer[ idx ] | bit;
            }
        }
        else
          for( i = y0; i < y1; i++ )
          {
              idx = x + ( i / 8 ) * LCD_WIDTH;
              bit = 1 << ( i % 8 );
              if( gbGrayIsBlack( x, i ) )
                gbFrameBuffer[ idx ] = gbFrameBuffer[ idx ] | bit;
              else
                gbFrameBuffer[ idx ] = gbFrameBuffer[ idx ] & ( 0xFF - bit );
          }
    }
    else
    {
        if( gbRealGrayColor && gbAnyGrayDrawn )
          for( i = y0; i < y1; i++ )
          {
              idx = x + ( i / 8 ) * LCD_WIDTH;
              bit = 1 << ( i % 8 );
              gbGrayBuffer[ idx ] = gbGrayBuffer[ idx ] & ( 0xFF - bit );
              gbFrameBuffer[ idx ] = gbFrameBuffer[ idx ] | bit;
          }
        else
          for( i = y0; i < y1; i++ )
          {
              idx = x + ( i / 8 ) * LCD_WIDTH;
              bit = 1 << ( i % 8 );
              gbFrameBuffer[ idx ] = gbFrameBuffer[ idx ] | bit;
          }
    }
}

// Bresenham line - direct port of Adafruit_GFX's own algorithm (the same
// one Gamebuino Classic's real Display.cpp is itself built on).
void gbDrawLine( int x0, int y0, int x1, int y1 )
{
    int steep = gbAbsInt( y1 - y0 ) > gbAbsInt( x1 - x0 );
    int tmp;
    if( steep )
    {
        tmp = x0; x0 = y0; y0 = tmp;
        tmp = x1; x1 = y1; y1 = tmp;
    }
    if( x0 > x1 )
    {
        tmp = x0; x0 = x1; x1 = tmp;
        tmp = y0; y0 = y1; y1 = tmp;
    }

    int dx = x1 - x0;
    int dy = gbAbsInt( y1 - y0 );
    int err = dx / 2;
    int ystep = -1;
    if( y0 < y1 )
      ystep = 1;

    int x;
    int y = y0;
    for( x = x0; x <= x1; x++ )
    {
        if( steep )
          gbDrawPixel( y, x );
        else
          gbDrawPixel( x, y );

        err = err - dy;
        if( err < 0 )
        {
            y = y + ystep;
            err = err + dx;
        }
    }
}

void gbDrawRect( int x, int y, int w, int h )
{
    gbDrawFastHLine( x, y, w );
    gbDrawFastHLine( x, y + h - 1, w );
    gbDrawFastVLine( x, y, h );
    gbDrawFastVLine( x + w - 1, y, h );
}

void gbFillRect( int x, int y, int w, int h )
{
    int row;
    for( row = y; row < y + h; row++ )
      gbDrawFastHLine( x, row, w );
}

void gbDrawCircle( int x0, int y0, int r )
{
    int f = 1 - r;
    int ddF_x = 1;
    int ddF_y = -2 * r;
    int x = 0;
    int y = r;

    gbDrawPixel( x0, y0 + r );
    gbDrawPixel( x0, y0 - r );
    gbDrawPixel( x0 + r, y0 );
    gbDrawPixel( x0 - r, y0 );

    while( x < y )
    {
        if( f >= 0 )
        {
            y = y - 1;
            ddF_y = ddF_y + 2;
            f = f + ddF_y;
        }
        x = x + 1;
        ddF_x = ddF_x + 2;
        f = f + ddF_x;

        gbDrawPixel( x0 + x, y0 + y );
        gbDrawPixel( x0 - x, y0 + y );
        gbDrawPixel( x0 + x, y0 - y );
        gbDrawPixel( x0 - x, y0 - y );
        gbDrawPixel( x0 + y, y0 + x );
        gbDrawPixel( x0 - y, y0 + x );
        gbDrawPixel( x0 + y, y0 - x );
        gbDrawPixel( x0 - y, y0 - x );
    }
}

void gbFillCircle( int x0, int y0, int r )
{
    int x, y;
    for( y = -r; y <= r; y++ )
      for( x = -r; x <= r; x++ )
        if( x * x + y * y <= r * r )
          gbDrawPixel( x0 + x, y0 + y );
}

// Direct port of real Display::drawCircleHelper() - draws only the one (or
// more) quadrant(s) selected by `cornername` (bit 0x1=top-left, 0x2=top-
// right, 0x4=bottom-right, 0x8=bottom-left), used internally by
// gbDrawRoundRect() for its four corners. Not part of the real public
// Gamebuino API either (a protected Display member there) - exposed here
// only because it's this file's own natural building block, same
// treatment as every other internal helper in this file.
void gbDrawCircleHelper( int x0, int y0, int r, int cornername )
{
    int f = 1 - r;
    int ddF_x = 1;
    int ddF_y = -2 * r;
    int x = 0;
    int y = r;

    while( x < y )
    {
        if( f >= 0 )
        {
            y = y - 1;
            ddF_y = ddF_y + 2;
            f = f + ddF_y;
        }
        x = x + 1;
        ddF_x = ddF_x + 2;
        f = f + ddF_x;

        if( cornername & 0x4 )
        {
            gbDrawPixel( x0 + x, y0 + y );
            gbDrawPixel( x0 + y, y0 + x );
        }
        if( cornername & 0x2 )
        {
            gbDrawPixel( x0 + x, y0 - y );
            gbDrawPixel( x0 + y, y0 - x );
        }
        if( cornername & 0x8 )
        {
            gbDrawPixel( x0 - y, y0 + x );
            gbDrawPixel( x0 - x, y0 + y );
        }
        if( cornername & 0x1 )
        {
            gbDrawPixel( x0 - y, y0 - x );
            gbDrawPixel( x0 - x, y0 - y );
        }
    }
}

// Direct port of real Display::fillCircleHelper() - fills only the one (or
// two) quadrant(s) selected by `cornername` (bit 0x1=right half, 0x2=left
// half), `delta` extending the drawn vertical strips by that many extra
// pixels - used internally by gbFillRoundRect() for its four corners
// (shared with the middle strip via the real `h - 2*r - 1` extension).
void gbFillCircleHelper( int x0, int y0, int r, int cornername, int delta )
{
    int f = 1 - r;
    int ddF_x = 1;
    int ddF_y = -2 * r;
    int x = 0;
    int y = r;

    while( x < y )
    {
        if( f >= 0 )
        {
            y = y - 1;
            ddF_y = ddF_y + 2;
            f = f + ddF_y;
        }
        x = x + 1;
        ddF_x = ddF_x + 2;
        f = f + ddF_x;

        if( cornername & 0x1 )
        {
            gbDrawFastVLine( x0 + x, y0 - y, 2 * y + 1 + delta );
            gbDrawFastVLine( x0 + y, y0 - x, 2 * x + 1 + delta );
        }
        if( cornername & 0x2 )
        {
            gbDrawFastVLine( x0 - x, y0 - y, 2 * y + 1 + delta );
            gbDrawFastVLine( x0 - y, y0 - x, 2 * x + 1 + delta );
        }
    }
}

// Direct port of real Display::drawRoundRect().
void gbDrawRoundRect( int x, int y, int w, int h, int r )
{
    gbDrawFastHLine( x + r, y, w - 2 * r ); // Top
    gbDrawFastHLine( x + r, y + h - 1, w - 2 * r ); // Bottom
    gbDrawFastVLine( x, y + r, h - 2 * r ); // Left
    gbDrawFastVLine( x + w - 1, y + r, h - 2 * r ); // Right
    // four corners
    gbDrawCircleHelper( x + r, y + r, r, 1 );
    gbDrawCircleHelper( x + w - r - 1, y + r, r, 2 );
    gbDrawCircleHelper( x + w - r - 1, y + h - r - 1, r, 4 );
    gbDrawCircleHelper( x + r, y + h - r - 1, r, 8 );
}

// Direct port of real Display::fillRoundRect().
void gbFillRoundRect( int x, int y, int w, int h, int r )
{
    gbFillRect( x + r, y, w - 2 * r, h );
    // four corners
    gbFillCircleHelper( x + w - r - 1, y + r, r, 1, h - 2 * r - 1 );
    gbFillCircleHelper( x + r, y + r, r, 2, h - 2 * r - 1 );
}

// Direct port of real Display::drawTriangle().
void gbDrawTriangle( int x0, int y0, int x1, int y1, int x2, int y2 )
{
    gbDrawLine( x0, y0, x1, y1 );
    gbDrawLine( x1, y1, x2, y2 );
    gbDrawLine( x2, y2, x0, y0 );
}

// Direct port of real Display::fillTriangle() - sorts the three vertices by
// Y, then fills top-to-bottom via two scanline-crossing loops (one for the
// upper half against edges 0-1/0-2, one for the lower half against edges
// 1-2/0-2). No ternary/swap-macro in this dialect, so both are expanded to
// plain if/temp-variable form; otherwise unchanged from the real algorithm,
// including its own careful avoidance of the exact dy==0 cases this
// platform would hard-trap on (see real source's own comment on the
// y0==y1/y1==y2 flat-top/flat-bottom skip - reproduced exactly below).
void gbFillTriangle( int x0, int y0, int x1, int y1, int x2, int y2 )
{
    int a, b, y, last, tmp;
    int dx01, dy01, dx02, dy02, dx12, dy12, sa, sb;

    // Sort coordinates by Y order (y2 >= y1 >= y0)
    if( y0 > y1 ) { tmp = y0; y0 = y1; y1 = tmp; tmp = x0; x0 = x1; x1 = tmp; }
    if( y1 > y2 ) { tmp = y2; y2 = y1; y1 = tmp; tmp = x2; x2 = x1; x1 = tmp; }
    if( y0 > y1 ) { tmp = y0; y0 = y1; y1 = tmp; tmp = x0; x0 = x1; x1 = tmp; }

    if( y0 == y2 ) // Handle awkward all-on-same-line case as its own thing
    {
        a = x0; b = x0;
        if( x1 < a ) a = x1; else if( x1 > b ) b = x1;
        if( x2 < a ) a = x2; else if( x2 > b ) b = x2;
        gbDrawFastHLine( a, y0, b - a + 1 );
        return;
    }

    dx01 = x1 - x0;
    dy01 = y1 - y0;
    dx02 = x2 - x0;
    dy02 = y2 - y0;
    dx12 = x2 - x1;
    dy12 = y2 - y1;
    sa = 0;
    sb = 0;

    // For upper part of triangle, find scanline crossings for segments
    // 0-1 and 0-2. If y1==y2 (flat-bottomed triangle), the scanline y1 is
    // included here (and the second loop is skipped, avoiding a divide-by-
    // zero there); otherwise scanline y1 is skipped here and handled in
    // the second loop (which also avoids a divide-by-zero here if y0==y1).
    if( y1 == y2 ) last = y1; else last = y1 - 1;

    for( y = y0; y <= last; y++ )
    {
        a = x0 + sa / dy01;
        b = x0 + sb / dy02;
        sa = sa + dx01;
        sb = sb + dx02;
        if( a > b ) { tmp = a; a = b; b = tmp; }
        gbDrawFastHLine( a, y, b - a + 1 );
    }

    // For the lower part of the triangle, find scanline crossings for
    // segments 1-2 and 0-2. Skipped entirely if y1==y2.
    sa = dx12 * ( y - y1 );
    sb = dx02 * ( y - y0 );
    for( ; y <= y2; y++ )
    {
        a = x1 + sa / dy12;
        b = x0 + sb / dy02;
        sa = sa + dx12;
        sb = sb + dx02;
        if( a > b ) { tmp = a; a = b; b = tmp; }
        gbDrawFastHLine( a, y, b - a + 1 );
    }
}

// -----------------------------------------------------------------------------
// Bitmaps - see gamebuinoShim.h's own header comment on gbDrawBitmap() for
// the real byte-format details this is ported against.
// -----------------------------------------------------------------------------

// A Vircon32-specific performance shape: pixel output is identical to a
// naive "call gbDrawPixel() per set bit" loop. This is this shim's single
// hottest draw path (every ported game's own sprite/title/UI art goes
// through it), so it's the one place worth inlining the pixel write
// directly: per real
// hardware's own genuinely-shipped optimized Display::drawBitmap() (the
// axis-aligned/unrotated case, confirmed by reading the real source), the
// destination row's own page+bit only change once per source row, not
// once per pixel - hoisted out of the inner loop here too. Unlike real
// hardware, this does NOT chase the extra "avoid dividing" trick real
// AVR's own optimized version uses (a rolling bitmask instead of `i/8`,
// `i%8` for the *source* bitmap column) - Vircon32's own documented cost
// model makes IDIV/IMOD as cheap as a shift, so there is nothing to gain
// from that part; only the per-pixel gbDrawPixel() *call* was ever the
// real cost here. gbDrawBitmapRotated()'s own rotated/flipped case is left
// as a plain gbDrawPixel() loop, matching real hardware's own actual
// choice to leave that slower generic path alone too - it's already far
// less common than the unrotated case across every ported game.
void gbDrawBitmap( int x, int y, int* bitmap )
{
    int w = bitmap[ 0 ];
    int h = bitmap[ 1 ];
    int byteWidth = ( w + 7 ) / 8;
    int i, j, byteNum, bitNum, px, py, idx, bit, rowBase;

    for( j = 0; j < h; j++ )
    {
        py = y + j;
        if( py < 0 || py >= LCD_HEIGHT ) continue; // same per-row clip gbDrawPixel() would have applied per-pixel

        rowBase = ( py / 8 ) * LCD_WIDTH;
        bit = 1 << ( py % 8 );

        for( i = 0; i < w; i++ )
        {
            byteNum = i / 8;
            bitNum = i % 8;
            if( bitmap[ 2 + j * byteWidth + byteNum ] & ( 0x80 >> bitNum ) )
            {
                px = x + i;
                if( px >= 0 && px < LCD_WIDTH )
                {
                    idx = rowBase + px;
                    if( gbRealGrayColor && gbAnyGrayDrawn )
                      gbGrayBuffer[ idx ] = gbGrayBuffer[ idx ] & ( 0xFF - bit );

                    if( gbColor == 0 )
                      gbFrameBuffer[ idx ] = gbFrameBuffer[ idx ] & ( 0xFF - bit );
                    else if( gbColor == GB_INVERT )
                      gbFrameBuffer[ idx ] = gbFrameBuffer[ idx ] ^ bit;
                    else if( gbColor == GB_GRAY )
                    {
                        if( gbRealGrayColor )
                        {
                            gbFrameBuffer[ idx ] = gbFrameBuffer[ idx ] | bit;
                            gbGrayBuffer[ idx ] = gbGrayBuffer[ idx ] | bit;
                            gbAnyGrayDrawn = true;
                        }
                        else if( gbGrayIsBlack( px, py ) )
                          gbFrameBuffer[ idx ] = gbFrameBuffer[ idx ] | bit;
                        else
                          gbFrameBuffer[ idx ] = gbFrameBuffer[ idx ] & ( 0xFF - bit );
                    }
                    else
                      gbFrameBuffer[ idx ] = gbFrameBuffer[ idx ] | bit;
                }
            }
        }
    }
}

// A Vircon32-specific performance shape, matching real hardware's own
// pixel-for-pixel output exactly, including the same real hardware quirks
// this project's own callers already depend on (flip using the bitmap's
// pre-rotation w/h, vertical flip's own `h - l` asymmetry). For every
// rotation/flip combination, real hardware's own (k,l) formulas always
// make k (the dest x-offset) a function of exactly one of the two source
// loop variables {i,j}, and l (the dest y-offset - which LCD page+bit gets
// written) a function of the other one - true both before AND after flip
// is applied, since flip only ever remaps k using the *same* k, and l
// using the *same* l (never mixes the two). Whichever loop variable l
// depends on is made the OUTER loop here, so the destination page+bit is
// computed once per outer step instead of once per pixel, with the inner
// loop doing a plain inlined framebuffer read-modify-write - no
// gbDrawPixel() call - exactly like gbDrawBitmap()'s own fast path. This
// benefits every rotated/flipped draw in this project (gameFlappyBirdo.c's
// own pipes/bird, drawn every frame unconditionally; gameSimonbuino.c's
// own 3 rotated direction pads; gameParachute.c's own flipped shark
// fin/swimmer).
void gbDrawBitmapRotated( int x, int y, int* bitmap, int rotation, int flip )
{
    if( rotation == 0 && flip == 0 )
    {
        gbDrawBitmap( x, y, bitmap );
        return;
    }

    int w = bitmap[ 0 ];
    int h = bitmap[ 1 ];
    int byteWidth = ( w + 7 ) / 8;
    int i, j, k, l, byteNum, bitNum, px, py, idx, bit, rowBase;

    rotation = rotation % 4;
    flip = flip % 4;

    if( rotation == 1 || rotation == 3 )
    {
        // k depends only on j, l depends only on i - i is the outer loop.
        for( i = 0; i < w; i++ )
        {
            if( rotation == 1 ) l = w - i - 1;
            else l = i; // rotation 3

            if( flip & 2 ) l = h - l;

            py = y + l;
            if( py >= 0 && py < LCD_HEIGHT )
            {
                rowBase = ( py / 8 ) * LCD_WIDTH;
                bit = 1 << ( py % 8 );

                byteNum = i / 8;
                bitNum = i % 8;

                for( j = 0; j < h; j++ )
                {
                    if( bitmap[ 2 + j * byteWidth + byteNum ] & ( 0x80 >> bitNum ) )
                    {
                        if( rotation == 1 ) k = j;
                        else k = h - j - 1;

                        if( flip & 1 ) k = w - k - 1;

                        px = x + k;
                        if( px >= 0 && px < LCD_WIDTH )
                        {
                            idx = rowBase + px;
                            if( gbRealGrayColor && gbAnyGrayDrawn )
                              gbGrayBuffer[ idx ] = gbGrayBuffer[ idx ] & ( 0xFF - bit );

                            if( gbColor == 0 )
                              gbFrameBuffer[ idx ] = gbFrameBuffer[ idx ] & ( 0xFF - bit );
                            else if( gbColor == GB_INVERT )
                              gbFrameBuffer[ idx ] = gbFrameBuffer[ idx ] ^ bit;
                            else if( gbColor == GB_GRAY )
                            {
                                if( gbRealGrayColor )
                                {
                                    gbFrameBuffer[ idx ] = gbFrameBuffer[ idx ] | bit;
                                    gbGrayBuffer[ idx ] = gbGrayBuffer[ idx ] | bit;
                                    gbAnyGrayDrawn = true;
                                }
                                else if( gbGrayIsBlack( px, py ) )
                                  gbFrameBuffer[ idx ] = gbFrameBuffer[ idx ] | bit;
                                else
                                  gbFrameBuffer[ idx ] = gbFrameBuffer[ idx ] & ( 0xFF - bit );
                            }
                            else
                              gbFrameBuffer[ idx ] = gbFrameBuffer[ idx ] | bit;
                        }
                    }
                }
            }
        }
    }
    else
    {
        // rotation 0 or 2: k depends only on i, l depends only on j - j is the outer loop.
        for( j = 0; j < h; j++ )
        {
            if( rotation == 2 ) l = h - j - 1;
            else l = j;

            if( flip & 2 ) l = h - l;

            py = y + l;
            if( py >= 0 && py < LCD_HEIGHT )
            {
                rowBase = ( py / 8 ) * LCD_WIDTH;
                bit = 1 << ( py % 8 );

                for( i = 0; i < w; i++ )
                {
                    byteNum = i / 8;
                    bitNum = i % 8;
                    if( bitmap[ 2 + j * byteWidth + byteNum ] & ( 0x80 >> bitNum ) )
                    {
                        if( rotation == 2 ) k = w - i - 1;
                        else k = i;

                        if( flip & 1 ) k = w - k - 1;

                        px = x + k;
                        if( px >= 0 && px < LCD_WIDTH )
                        {
                            idx = rowBase + px;
                            if( gbRealGrayColor && gbAnyGrayDrawn )
                              gbGrayBuffer[ idx ] = gbGrayBuffer[ idx ] & ( 0xFF - bit );

                            if( gbColor == 0 )
                              gbFrameBuffer[ idx ] = gbFrameBuffer[ idx ] & ( 0xFF - bit );
                            else if( gbColor == GB_INVERT )
                              gbFrameBuffer[ idx ] = gbFrameBuffer[ idx ] ^ bit;
                            else if( gbColor == GB_GRAY )
                            {
                                if( gbRealGrayColor )
                                {
                                    gbFrameBuffer[ idx ] = gbFrameBuffer[ idx ] | bit;
                                    gbGrayBuffer[ idx ] = gbGrayBuffer[ idx ] | bit;
                                    gbAnyGrayDrawn = true;
                                }
                                else if( gbGrayIsBlack( px, py ) )
                                  gbFrameBuffer[ idx ] = gbFrameBuffer[ idx ] | bit;
                                else
                                  gbFrameBuffer[ idx ] = gbFrameBuffer[ idx ] & ( 0xFF - bit );
                            }
                            else
                              gbFrameBuffer[ idx ] = gbFrameBuffer[ idx ] | bit;
                        }
                    }
                }
            }
        }
    }
}

// -----------------------------------------------------------------------------
// Text
// -----------------------------------------------------------------------------

int gbCursorX = 0;
int gbCursorY = 0;
int gbFontSize = 1;
int* gbFontPtr;
int gbFontWidth;  // real per-glyph cell width  = raw glyph width  + 1 (real inter-char spacer column)
int gbFontHeight; // real per-glyph cell height = raw glyph height + 1 (real inter-line spacer row)

// Direct port of real Display::setFont() - font is one of gbFont5x7/
// gbFont3x5/gbFont3x3 (see this file's own Font tables section above).
// gbFontPtr keeps pointing at the array's own real {width,height} header
// (unlike real hardware's own `font` member, which setFont() advances past
// it) since this dialect's own gbDrawChar() below indexes with an explicit
// "+2" instead - simpler than carrying a second, pre-offset pointer.
void gbSetFont( int* font )
{
    gbFontPtr = font;
    gbFontWidth = font[ 0 ] + 1;
    gbFontHeight = font[ 1 ] + 1;
}

// Direct port of real Display::drawChar() - glyphs are indexed directly by
// ASCII code (0-127, matching every real font table's own real layout),
// `glyphCols` (gbFontWidth-1) column-major bytes per glyph, LSB-top like
// every other bitmap in this shim. The real inter-char spacer column (the
// last of gbFontWidth columns) is always blank, exactly like real hardware -
// see this file's own Font tables comment for why no such forcing is needed
// vertically (the real per-font byte values already leave the real
// inter-line spacer row's own bit unset).
// Draws one real glyph pixel at its own final on-screen (px,py) - a size-1
// single pixel or a size-2 2x2 block, matching gbFontSize exactly like real
// Display::drawChar()'s own size handling. `px`/`py` must already be the
// real, final top-left screen pixel (the caller pre-scales only the glyph-
// local column/row offset by gbFontSize before adding it to the unscaled
// cursor position - scaling the already-cursor-summed coordinate here
// instead would double-count the cursor position itself). Shared by
// gbDrawChar() below for both its "on" pixels (gbColor) and, when a real
// opaque background is set (gbBgColor != gbColor - see gbSetColorBg()), its
// "off" pixels too.
void gbDrawCharPixel( int px, int py )
{
    // Real Display::drawChar() draws a size-1 glyph pixel with drawPixel()
    // and any larger one with fillRect(x + i*size, y + j*size, size, size) -
    // so every size works, not just 1 and 2. Reproduced exactly: the size-1
    // fast path is real hardware's own, and every other size fills a
    // size x size block the same way.
    if( gbFontSize == 1 )
      gbDrawPixel( px, py );
    else
      gbFillRect( px, py, gbFontSize, gbFontSize );
}

void gbDrawChar( int ch, int x, int y )
{
    int glyphCols = gbFontWidth - 1;
    int col, row, line, savedColor;

    if( ch < 0 || ch > 127 ) return; // outside every real font's own 0-127 range - a real no-op, not a bad read

    // Draws "on" bits in whatever color gbSetColor() last set - like every
    // other gbDraw*() primitive in this shim (gbDrawBitmap included) and
    // like real Display::drawChar() itself. "Off" bits are drawn in
    // gbBgColor whenever it differs from gbColor (a real gbSetColorBg()
    // call) - direct port of real Display::drawChar()'s own
    // `bgcolor != color` check.

    for( col = 0; col < gbFontWidth; col = col + 1 )
    {
        if( col == glyphCols )
          line = 0;
        else
          line = gbFontPtr[ 2 + ch * glyphCols + col ];

        for( row = 0; row < gbFontHeight; row = row + 1 )
        {
            if( ( line & 1 ) == 1 )
              gbDrawCharPixel( x + col * gbFontSize, y + row * gbFontSize );
            else if( gbBgColor != gbColor )
            {
                savedColor = gbColor;
                gbColor = gbBgColor;
                gbDrawCharPixel( x + col * gbFontSize, y + row * gbFontSize );
                gbColor = savedColor;
            }
            line = line >> 1;
        }
    }
}

// Direct port of real Display::write()'s own per-character loop (the part
// of it every ported game's own gb.display.print()/println() call site was
// mechanically rewritten to - see gamePong.c's own header comment for the
// general rewrite pattern). '\n' (ASCII 10) moves to the next real text
// line (cursorX back to 0, cursorY down by one real font cell) rather than
// being drawn as a glyph, exactly like real hardware.
void gbPrintString( int* text )
{
    int i = 0;

    while( text[ i ] != 0 )
    {
        if( text[ i ] == 10 )
        {
            gbCursorY = gbCursorY + gbFontSize * gbFontHeight;
            gbCursorX = 0;
        }
        else
        {
            gbDrawChar( text[ i ], gbCursorX, gbCursorY );
            gbCursorX = gbCursorX + gbFontSize * gbFontWidth;
        }
        i = i + 1;
    }
}

void gbPrintNumber( int value )
{
    int[16] numText;
    itoa( value, numText, 10 );
    gbPrintString( numText );
}

// Direct port of real Arduino Print::printFloat() - see gamebuinoShim.h's
// own doc comment. Originally implemented locally in gameAgaruino.c/
// gameMotoCross.c (each independently, before either knew the other
// needed it) - promoted here once both were found to have hit the
// identical "no float-print primitive exists" wall.
void gbPrintFloat( float value, int decimals )
{
    int i;
    float rounding;
    int intPart;
    float remainder;
    int digit;

    if( value < 0 )
    {
        gbPrintString( "-" );
        value = -value;
    }

    rounding = 0.5;
    for( i = 0; i < decimals; i = i + 1 )
      rounding = rounding / 10.0;
    value = value + rounding;

    intPart = (int)value;
    remainder = value - (float)intPart;
    gbPrintNumber( intPart );

    if( decimals > 0 )
      gbPrintString( "." );

    for( i = 0; i < decimals; i = i + 1 )
    {
        remainder = remainder * 10.0;
        digit = (int)remainder;
        gbPrintNumber( digit );
        remainder = remainder - digit;
    }
}

// -----------------------------------------------------------------------------
// Popup - direct port of real Gamebuino::popup()/updatePopup() (see this
// primitive's own doc comment in gamebuinoShim.h). gbUpdatePopup() is
// called automatically from gbRenderFrame() below, never by game code
// directly - matching real hardware's own automatic call from inside
// Gamebuino::update() itself.
// -----------------------------------------------------------------------------

int* gbPopupText;
int gbPopupTimeLeft = 0;

void gbPopup( int* text, int duration )
{
    gbPopupText = text;
    gbPopupTimeLeft = duration + 12;
}

void gbUpdatePopup()
{
    if( gbPopupTimeLeft > 0 )
    {
        int yOffset = 0;
        if( gbPopupTimeLeft < 12 )
          yOffset = 12 - gbPopupTimeLeft;

        gbFontSize = 1;
        gbSetColor( 0 ); // WHITE
        gbFillRoundRect( 0, LCD_HEIGHT - gbFontHeight + yOffset - 3, 84, gbFontHeight + 3, 3 );
        gbSetColor( 1 ); // BLACK
        gbDrawRoundRect( 0, LCD_HEIGHT - gbFontHeight + yOffset - 3, 84, gbFontHeight + 3, 3 );
        gbCursorX = 4;
        gbCursorY = LCD_HEIGHT - gbFontHeight + yOffset - 1;
        gbPrintString( gbPopupText );
        gbPopupTimeLeft = gbPopupTimeLeft - 1;
    }
}

// -----------------------------------------------------------------------------
// Buttons
// -----------------------------------------------------------------------------

int[7] gbBtnHeld;
int[7] gbBtnPrevHeld;

int gbButtonLevel( int button )
{
    if( button == BTN_UP ) return md_inputUp();
    if( button == BTN_DOWN ) return md_inputDown();
    if( button == BTN_LEFT ) return md_inputLeft();
    if( button == BTN_RIGHT ) return md_inputRight();
    if( button == BTN_A ) return md_inputA();
    if( button == BTN_B ) return md_inputB();
    if( button == BTN_C ) return md_inputC();
    return 0;
}

void gbUpdateButtons()
{
    int i;
    for( i = 0; i < 7; i++ )
    {
        gbBtnPrevHeld[ i ] = gbBtnHeld[ i ];
        if( gbButtonLevel( i ) )
          gbBtnHeld[ i ] = gbBtnHeld[ i ] + 1;
        else
          gbBtnHeld[ i ] = 0;
    }
}

bool gbPressed( int button ) { return gbBtnHeld[ button ] == 1; }
bool gbReleased( int button ) { return gbBtnHeld[ button ] == 0 && gbBtnPrevHeld[ button ] > 0; }
bool gbHeld( int button, int frames ) { return gbBtnHeld[ button ] >= frames; }
// Direct port of real Buttons::timeHeld() - a plain passthrough of the same
// per-tick hold counter gbHeld()/gbPressed() already maintain internally.
int gbTimeHeld( int button ) { return gbBtnHeld[ button ]; }

// Direct port of real Buttons::repeat(button, period). Real hardware's own
// held-button state starts at 1 on its first held frame and increments
// every frame it stays held - matching this shim's own gbBtnHeld[] exactly
// (see gbPressed()'s own identical `==1` "just pressed" check), so the two
// counters line up frame-for-frame. Real period<=1 means "fire on every
// single held frame" (a genuine continuous repeat, not just the first).
// For period>1, real hardware's own exact modulo target is 1, not 0
// (`state % period == 1`): fires on the very first held frame, then every
// `period` frames after that.
bool gbRepeat( int button, int period )
{
    if( period <= 1 )
      return gbBtnHeld[ button ] >= 1;

    return ( gbBtnHeld[ button ] % period ) == 1;
}

// -----------------------------------------------------------------------------
// Core / lifecycle
// -----------------------------------------------------------------------------

// 20, not 30 - confirmed directly against the real Gamebuino::begin()
// source (Gamebuino.cpp: `timePerFrame = 50;`, i.e. 1000/50 = 20fps, set
// directly rather than by calling setFrameRate()), not assumed from
// documentation. Easy to get wrong: most of the setFrameRate() writeups
// findable online are actually for the later META library
// (Gamebuino-Meta.h), which defaults to 25fps instead - a distinct board
// this project has no relationship to. This project's own earlier default
// here was 30 (silently wrong) until this was checked against the real
// source - Pong Solo's own upstream `Pong.ino` never calls
// `gb.setFrameRate()` itself, so it was actually running its entire
// gameplay logic (ball/paddle speed, AI tracking) 1.5x too fast versus
// real hardware the whole time this shim defaulted to 30.
int gbFrameRateFps = 20;
int gbTickAccum = 0;

// Real Gamebuino::frameCount (a real, public `uint32_t` member) - real
// hardware increments it once per real logic tick inside `update()`
// (see gbUpdate()'s own matching `gbFrameCount = gbFrameCount + 1`) and
// notably never explicitly resets it to 0 in its own constructor (that
// line is commented out in the real source) - relying on static zero-
// initialization at power-on instead, something with no equivalent here
// since one cartridge session can launch many different games one after
// another, unlike real hardware's own single power-on. Reset to 0 in
// gbBegin() below instead, so every freshly-launched game starts counting
// from a predictable 0 rather than inheriting a large, arbitrary value
// left over from whatever game ran before it - a real, considered
// adaptation, not a preserved-bug case, since real hardware never had
// this scenario to begin with.
int gbFrameCount = 0;

void gbBegin()
{
    gbFrameRateFps = 20;
    gbTickAccum = 0;
    gbFrameCount = 0;
    gbPopupTimeLeft = 0; // same cross-game-launch reset rationale as gbFrameCount above
    gbLcdPersistencePrimed = false; // a fresh game must not have the previous one's own last frame smeared into its first tick - see gbLcdPersistencePrimed's own comment
    gbClear();
    gbCursorX = 0;
    gbCursorY = 0;
    gbFontSize = 1;
    gbSetFont( gbFont3x5 ); // real Display::Display()'s own default font
    gbColor = 1;
    gbRecomputeSoundPrescaler();
    gbInitSoundEngine(); // same cross-game-launch reset rationale as gbFrameCount/gbPopupTimeLeft above - a fresh game must never inherit a previous game's own leftover pattern/track/note/voice state
}

// Real Gamebuino::setFrameRate(uint8_t fps) has no explicit clamp of its
// own beyond its parameter's real 8-bit range (0-255, and passing 0 would
// divide-by-zero via its own `timePerFrame = 1000 / fps` - not something
// any real game actually does) - but this shim's own fps is a full-range
// int, and more importantly Vircon32's own engine physically only ticks
// at 60fps, so requesting anything above 60 here can never mean anything
// (gbUpdate()'s own accumulator would just return true on every single
// engine tick, identical to requesting exactly 60) - clamped to [1, 60]
// for that reason, a shim-specific gate real hardware has no equivalent
// need for.
void gbSetFrameRate( int fps )
{
    if( fps < 1 ) fps = 1;
    if( fps > 60 ) fps = 60;
    gbFrameRateFps = fps;
    gbRecomputeSoundPrescaler();
}

void gbPickRandomSeed()
{
    // no-op - see gamebuinoShim.h's own header comment
}

// Whole-tick throttle to the configured frame rate, matching the sibling
// tinyjoypad_vircon32 project's own Bresenham-style accumulator technique
// (needed since 60 doesn't divide evenly by every possible fps a game might
// request, e.g. the real default of 20 divides cleanly (60/20=3), but a
// game asking for 25 would not). Returns true on the one real 60fps engine
// tick that should run this logic tick, and - only on that same tick -
// samples button state and clears the framebuffer fresh (matching real
// Gamebuino's own default persistence=false behavior).
bool gbUpdate()
{
    gbTickAccum = gbTickAccum + gbFrameRateFps;
    if( gbTickAccum < 60 )
      return false;
    gbTickAccum = gbTickAccum - 60;

    gbUpdateButtons();
    gbClear();
    gbCursorX = 0;
    gbCursorY = 0;
    gbFrameCount = gbFrameCount + 1;

    // Real Gamebuino::update() resets color/bgcolor to (BLACK,WHITE) - a
    // genuine OPAQUE white text background - at the tail of every single
    // frame, via its own automatic displayBattery() call (confirmed
    // directly in Gamebuino.cpp: display.setColor(BLACK,WHITE) runs
    // unconditionally there whenever battery.thresholds[0] is nonzero,
    // which it always is under normal settings - BAT_LVL_CRITIC=3500 by
    // default). This shim has no battery indicator to draw and so never
    // had a reason to call anything at that same point - but the color
    // reset is real, load-bearing behavior independent of whether a
    // battery icon is ever drawn, and no ported game compensates for it by
    // re-calling setColor() every frame the way real hardware silently
    // requires. Reproduced here directly (matching gbCursorX/Y's own
    // identical per-frame reset just above) rather than per-game: a game
    // that wants transparent text still needs to call gbSetColor(color)
    // itself each frame it draws that way, exactly like real hardware -
    // found via a direct live user report against gameCastleDefence.c
    // (see that file's own header comment), but real for every game.
    gbColor = 1;
    gbBgColor = 0;

    return true;
}

// Streams the framebuffer to the GPU via the pre-baked column atlas - call
// once, at the very end of a game's own update function, after all of that
// tick's drawing is done (matching every other game in the sibling
// tinyjoypad_vircon32 project's own established "clear, draw, render"
// shape - see gamePong.c's own header comment).
// The LCD-persistence render path - see gbLcdPersistence's own comment for
// the model. Called by gbRenderFrame() below, which has already cleared the
// screen; both passes are gated at the call site exactly like the normal
// path's are, so a byte column that is blank in the relevant sense costs one
// compare rather than a call.
void gbRenderFramePersistent()
{
    int col, page, i, cur, prev;

    if( !gbLcdPersistencePrimed )
    {
        for( i = 0; i < LCD_WIDTH * LCD_PAGES; i++ )
          gbPrevFrameBuffer[ i ] = gbFrameBuffer[ i ];

        gbLcdPersistencePrimed = true;
    }

    // Black: lit on both this tick and the last one.
    for( page = 0; page < LCD_PAGES; page++ )
      for( col = 0; col < LCD_WIDTH; col++ )
      {
        i = col + page * LCD_WIDTH;
        cur = gbFrameBuffer[ i ] & 0xFF;
        prev = gbPrevFrameBuffer[ i ] & 0xFF;

        if( ( cur & prev ) != 0 )
          md_drawColumn( col, page, cur & prev );
      }

    // Gray: lit on exactly one of the two, drawn on top of the pass above
    // (the gray atlas is transparent wherever its own bits are off, so the
    // black pixels underneath are left alone).
    for( page = 0; page < LCD_PAGES; page++ )
      for( col = 0; col < LCD_WIDTH; col++ )
      {
        i = col + page * LCD_WIDTH;
        cur = gbFrameBuffer[ i ] & 0xFF;
        prev = gbPrevFrameBuffer[ i ] & 0xFF;

        if( ( cur ^ prev ) != 0 )
          md_drawColumnGray( col, page, cur ^ prev );
      }

    for( i = 0; i < LCD_WIDTH * LCD_PAGES; i++ )
      gbPrevFrameBuffer[ i ] = gbFrameBuffer[ i ];
}

void gbRenderFrame()
{
    gbUpdatePopup(); // real hardware's own automatic tail-of-update() call - see gbPopup()'s own doc comment
    gbUpdateSoundTracker(); // real Gamebuino::update()'s own automatic sound.updateTrack()/updatePattern()/updateNote() tail call - see gbUpdateSoundTracker()'s own doc comment
    md_beginFrame();
    int col, page, value;

    if( gbLcdPersistence )
    {
        gbRenderFramePersistent();
        return;
    }

    // Any tick rendered without the persistence mode leaves
    // gbPrevFrameBuffer stale - see gbLcdPersistencePrimed's own comment.
    gbLcdPersistencePrimed = false;

    for( page = 0; page < LCD_PAGES; page++ )
      for( col = 0; col < LCD_WIDTH; col++ )
      {
        // Gated here at the call site, with the exact same 0xFF mask
        // md_drawColumn() itself applies internally (it already self-gates
        // a blank column - see its own header comment) - a blank column
        // costs one array read + one compare instead of paying a full
        // function call's own real ~10+ instruction overhead, the same "a
        // self-gated function still costs a full call every time it's
        // invoked; gate the call site too" lesson the sibling
        // tinyjoypad_vircon32 project's own OPTIMIZATIONS.md documents
        // repeatedly.
        value = gbFrameBuffer[ col + page * LCD_WIDTH ] & 0xFF;
        if( value != 0 )
          md_drawColumn( col, page, value );
      }

    // Second, targeted pass: draws a real gray-tinted tile on top of the
    // pass above, but ONLY for the specific pixels gbGrayBuffer marks as
    // real GB_GRAY (a strict subset of what the pass above already drew as
    // black) - real per-pixel accuracy, not a whole-byte-column
    // approximation. gbAnyGrayDrawn skips this whole 504-cell scan on any
    // frame that drew no gray at all - always true while gbRealGrayColor is
    // off, since no drawing primitive ever sets it in that case - so this
    // costs nothing while the toggle is off. Otherwise gated exactly like
    // the pass above: a cell with no gray content at all costs one array
    // read + one compare, not a call.
    if( gbAnyGrayDrawn )
    {
        int grayValue;
        for( page = 0; page < LCD_PAGES; page++ )
          for( col = 0; col < LCD_WIDTH; col++ )
          {
            grayValue = gbGrayBuffer[ col + page * LCD_WIDTH ] & 0xFF;
            if( grayValue != 0 )
              md_drawColumnGray( col, page, grayValue );
          }
    }
}

// -----------------------------------------------------------------------------
// Sound - a direct port of real Gamebuino Classic's own single-oscillator-
// per-channel tracker engine (Sound.h/.cpp): notes, patterns (note
// sequences plus volume/instrument/slide/arpeggio/tremolo commands), and
// tracks (pattern sequences), across up to MAX_SOUND_CHANNELS real
// channels - matching real hardware's own documented "0 to 4" NUM_CHANNELS
// range; every real game found calling into this API directly uses
// channel index 0-3 (confirmed via a real grep sweep of `more games/`).
// Unlike real hardware (genuinely one oscillator per channel, no headroom
// to spare), every one of Vircon32's own 16 real SPU channels sits
// underneath this (md_playTone()'s own pool, shared with the one-shot
// tones below), so 4 tracker channels plus every game's own occasional
// one-shot blip comfortably coexist.
//
// gbHalfPeriods[]/GB_NUM_PITCH mirror real Sound.cpp's own 36-entry
// _halfPeriods table exactly (EXTENDED_NOTE_RANGE's own real default of
// 0) - a pitch argument anywhere in this API is a direct 0-35 index into
// it, NOT a MIDI note number (confirmed directly against Elventure's own
// real sound_data.h note-name header: NOTE_C3=14, NOTE_C4=26 - a real
// octave apart, exactly matching playOK()'s own two real notes below and
// this table's own halfPeriod-doubling relationship between those two
// indices). Real audible frequency = GB_SOUND_ISR_HZ / (2*halfPeriod).
//
// GB_SOUND_ISR_HZ is the real Timer1 compare-match ISR rate that drives
// Sound::generateOutput() - 16000000 / (280+1) = 16MHz (real hardware's
// own confirmed CPU clock - see Simbuino, a real cycle-accurate AVR
// simulator, whose own AtmelProcessor.cs hardcodes
// `public const int ClockSpeed = 16000000`) divided by real CTC-mode
// OCR1A+1 (Sound::begin()'s own `OCR1A=280; TCCR1B|=(1<<WGM12);
// TCCR1B|=(1<<CS10);` - CTC mode, prescaler 1, so one real ISR fires every
// OCR1A+1=281 clock cycles). NOT the "15000 times per second" a real
// source comment in Sound.cpp itself states (a real, confirmed-wrong
// approximation in the comment, not the code) - this shim originally
// trusted that comment at face value, but a live user-recorded comparison
// against Simbuino's own real audio output (both playing 101Starships'
// own real background music) measured every dominant frequency
// consistently ~3.8x higher on Simbuino than this shim produced -
// matching 56939.5/15000=3.796 to within 0.08%, decisively confirming the
// comment (not the derivation from real register values) was the actual
// error.
#define GB_SOUND_ISR_HZ ( 16000000.0 / 281.0 )
#define MAX_SOUND_CHANNELS 4
#define GB_NUM_PITCH 36

int[ GB_NUM_PITCH ] gbHalfPeriods = { 246,232,219,207,195,184,174,164,155,146,138,130,123,116,110,104,98,92,87,82,78,73,69,65,62,58,55,52,49,46,44,41,39,37,35,33 };

// Real Sound.h's own CMD_* constants (Sound::command()'s own cmd argument).
#define GB_CMD_VOLUME     0
#define GB_CMD_INSTRUMENT 1
#define GB_CMD_SLIDE      2
#define GB_CMD_ARPEGGIO   3
#define GB_CMD_TREMOLO    4

// Real Sound.cpp's own default instrument set (index 0 = square wave,
// index 1 = noise) - {length/loop header, one 16-bit step word per real
// step}, byte-for-byte identical to real Sound.cpp's own
// squareWaveInstrument/noiseInstrument. A channel that never gets its own
// gbChangeInstrumentSet() call still has these two, matching real
// Sound::begin()'s own wiring (gbInitSoundEngine() below calls
// gbChangeInstrumentSet()+gbSoundCommand(GB_CMD_INSTRUMENT,0,...) for
// every channel, the same way).
int[ 2 ] gbSquareWaveInstrument = { 0x0101, 0x03F7 };
int[ 2 ] gbNoiseInstrument      = { 0x0101, 0x03FF };
int*[ 2 ] gbDefaultInstruments = { gbSquareWaveInstrument, gbNoiseInstrument };

// Per-channel tracker state - parallel arrays indexed 0..MAX_SOUND_CHANNELS-1,
// direct translations of Sound.h's own private per-channel fields.
int*[ MAX_SOUND_CHANNELS ] gbTrackData;
int[ MAX_SOUND_CHANNELS ] gbTrackCursor;
bool[ MAX_SOUND_CHANNELS ] gbTrackIsPlaying;
int**[ MAX_SOUND_CHANNELS ] gbPatternSet;
int[ MAX_SOUND_CHANNELS ] gbTrackPatternPitch;

int*[ MAX_SOUND_CHANNELS ] gbPatternData;
int**[ MAX_SOUND_CHANNELS ] gbInstrumentSet;
bool[ MAX_SOUND_CHANNELS ] gbPatternLooping;
int[ MAX_SOUND_CHANNELS ] gbPatternCursor;
bool[ MAX_SOUND_CHANNELS ] gbPatternIsPlaying;

int[ MAX_SOUND_CHANNELS ] gbNotePitch;
int[ MAX_SOUND_CHANNELS ] gbNoteDuration;
int[ MAX_SOUND_CHANNELS ] gbNoteVolume;
bool[ MAX_SOUND_CHANNELS ] gbNotePlaying;

int[ MAX_SOUND_CHANNELS ] gbCommandsCounter;
int[ MAX_SOUND_CHANNELS ] gbVolumeSlideStepDuration;
int[ MAX_SOUND_CHANNELS ] gbVolumeSlideStepSize;
int[ MAX_SOUND_CHANNELS ] gbArpeggioStepDuration;
int[ MAX_SOUND_CHANNELS ] gbArpeggioStepSize;
int[ MAX_SOUND_CHANNELS ] gbTremoloStepDuration;
int[ MAX_SOUND_CHANNELS ] gbTremoloStepSize;

int*[ MAX_SOUND_CHANNELS ] gbInstrumentData;
int[ MAX_SOUND_CHANNELS ] gbInstrumentLength;
int[ MAX_SOUND_CHANNELS ] gbInstrumentLooping;
int[ MAX_SOUND_CHANNELS ] gbInstrumentCursor;
int[ MAX_SOUND_CHANNELS ] gbInstrumentNextChange;

int[ MAX_SOUND_CHANNELS ] gbStepVolume;
int[ MAX_SOUND_CHANNELS ] gbStepPitch;
bool[ MAX_SOUND_CHANNELS ] gbStepNoise;

// Which real Vircon32 SPU channel (md_trackerVoiceStart()'s own return
// value) is currently sounding this tracker channel's active note, or -1
// if none. Real hardware's own oscillator is retuned continuously in
// place while a note sustains (Sound::generateOutput()'s own ISR just
// updates _chanHalfPeriod/_chanOutputVolume every tick, never restarting
// the waveform) - this shim retunes the same underlying voice the same
// way instead of starting a fresh voice every single tick, which would
// sound like a stutter of separate attacks rather than one continuous
// note.
int[ MAX_SOUND_CHANNELS ] gbActiveVoice;

// Real Sound::prescaler - real Gamebuino::setFrameRate(fps) recomputes
// this as max(1, fps/20) every time a game changes its own frame rate,
// and every real note/instrument-step duration is multiplied by it -
// keeping wall-clock note/effect timing consistent across games that
// configure different frame rates, since "duration" is otherwise counted
// in real per-tick units (one tick = one real display frame) which would
// otherwise tick down faster on a higher-fps game and slower on a
// lower-fps one. Recomputed here for the identical reason, called from
// both gbBegin() (matching real hardware's own implicit fps=20-at-
// startup default) and gbSetFrameRate() (matching every later call the
// same way real Gamebuino::setFrameRate() does).
int gbSoundPrescaler = 1;

void gbRecomputeSoundPrescaler()
{
    gbSoundPrescaler = gbFrameRateFps / 20;
    if( gbSoundPrescaler < 1 )
      gbSoundPrescaler = 1;
}

// Real Sound::command() (Sound.cpp) - sets per-channel volume/instrument/
// slide/arpeggio/tremolo state, read back by gbUpdateNoteChannel() below
// every tick a note is sounding. X/Y match real hardware's own real
// argument shape (X unsigned 0-31, Y signed via a -16 offset) - real
// upstream code calls this directly (e.g. Copter.ino's own real
// `playsoundfx()`), so this is real, load-bearing public API, not just an
// internal helper.
void gbSoundCommand( int cmd, int X, int Y, int channel )
{
    if( channel < 0 || channel >= MAX_SOUND_CHANNELS )
      return;

    if( cmd == GB_CMD_VOLUME )
    {
        if( X < 0 ) X = 0;
        if( X > 10 ) X = 10;
        gbNoteVolume[ channel ] = X;
    }
    else if( cmd == GB_CMD_INSTRUMENT )
    {
        gbInstrumentData[ channel ] = gbInstrumentSet[ channel ][ X ];
        gbInstrumentLength[ channel ] = gbInstrumentData[ channel ][ 0 ] & 0x00FF;

        int loopRaw = ( gbInstrumentData[ channel ][ 0 ] >> 8 ) & 0x00FF;
        if( loopRaw > gbInstrumentLength[ channel ] )
          loopRaw = gbInstrumentLength[ channel ]; // real min(loopRaw, instrumentLength) check
        gbInstrumentLooping[ channel ] = loopRaw;
    }
    else if( cmd == GB_CMD_SLIDE )
    {
        gbVolumeSlideStepDuration[ channel ] = X * gbSoundPrescaler;
        gbVolumeSlideStepSize[ channel ] = Y;
    }
    else if( cmd == GB_CMD_ARPEGGIO )
    {
        gbArpeggioStepDuration[ channel ] = X * gbSoundPrescaler;
        gbArpeggioStepSize[ channel ] = Y;
    }
    else if( cmd == GB_CMD_TREMOLO )
    {
        gbTremoloStepDuration[ channel ] = X * gbSoundPrescaler;
        gbTremoloStepSize[ channel ] = Y;
    }
}

// Real Sound::stopNote(channel) - ends a note immediately (no fade-out
// envelope of its own beyond whatever md_trackerVoiceStop() itself does).
void gbStopNoteChannel( int channel )
{
    if( channel < 0 || channel >= MAX_SOUND_CHANNELS )
      return;

    gbNotePlaying[ channel ] = false;
    gbNoteDuration[ channel ] = 0;
    gbInstrumentCursor[ channel ] = 0;
    gbCommandsCounter[ channel ] = 0;

    if( gbActiveVoice[ channel ] >= 0 )
    {
        md_trackerVoiceStop( gbActiveVoice[ channel ] );
        gbActiveVoice[ channel ] = -1;
    }
}

void gbStopNoteAll()
{
    int i;
    for( i = 0; i < MAX_SOUND_CHANNELS; i++ )
      gbStopNoteChannel( i );
}

// Real Sound::playNote(pitch, duration, channel) - starts a single note on
// one channel, using whatever instrument/volume/slide/arpeggio/tremolo
// state that channel's own last gbSoundCommand() calls last set (a fresh
// channel defaults to the real square-wave instrument at full volume,
// matching gbInitSoundEngine() below). pitch is a direct 0-35
// _halfPeriods index (see this section's own header comment above) -
// pitch 63 is real hardware's own "rest"/silent-note sentinel. duration is
// in real display frames (scaled by gbSoundPrescaler the same way real
// hardware's own noteDuration=duration*prescaler is).
void gbPlayNoteChannel( int pitch, int duration, int channel )
{
    if( channel < 0 || channel >= MAX_SOUND_CHANNELS )
      return;

    gbNotePitch[ channel ] = pitch;
    gbNoteDuration[ channel ] = duration * gbSoundPrescaler;
    gbInstrumentNextChange[ channel ] = 0;
    gbInstrumentCursor[ channel ] = 0;
    gbNotePlaying[ channel ] = true;
    gbCommandsCounter[ channel ] = 0;
}

// Real Sound::updateNote(channel) - called automatically once per real
// tick (see gbUpdateSoundTracker() below) for every channel with a note
// currently sounding; steps the active instrument's own envelope and any
// running slide/arpeggio/tremolo effect, then retunes that channel's real
// underlying voice to match. A deliberate, minor simplification versus
// real hardware: an instrument that exhausts its own steps with no loop
// point configured (gbInstrumentLooping==0 - never true for either real
// default instrument, both of which loop) stops the note immediately
// rather than falling through to also (harmlessly, on real hardware)
// recompute one more real output value for the same tick.
// Reproduces real AVR's own signed 8-bit (int8_t) narrowing-on-assignment
// for a value this dialect's always-32-bit int would otherwise never
// narrow - see gbUpdateNoteChannel()'s own doc comment for why this
// matters here (a real, crash-causing divergence found and fixed via a
// live user report).
int gbNarrowInt8( int value )
{
    value = value & 0xFF;
    if( value > 127 )
      value = value - 256;
    return value;
}

void gbUpdateNoteChannel( int i )
{
    if( !gbNotePlaying[ i ] )
      return;

    if( gbNoteDuration[ i ] == 0 )
    {
        gbStopNoteChannel( i );
        return;
    }
    gbNoteDuration[ i ] = gbNoteDuration[ i ] - 1;

    if( gbInstrumentNextChange[ i ] == 0 )
    {
        int thisStep = gbInstrumentData[ i ][ 1 + gbInstrumentCursor[ i ] ];

        gbStepVolume[ i ] = thisStep & 0x0007;
        thisStep = thisStep >> 3;

        gbStepNoise[ i ] = ( ( thisStep & 0x0001 ) != 0 );
        thisStep = thisStep >> 1;

        int stepDuration = thisStep & 0x003F;
        thisStep = thisStep >> 6;
        gbStepPitch[ i ] = thisStep;

        gbInstrumentNextChange[ i ] = stepDuration * gbSoundPrescaler;

        gbInstrumentCursor[ i ] = gbInstrumentCursor[ i ] + 1;
        if( gbInstrumentCursor[ i ] >= gbInstrumentLength[ i ] )
        {
            if( gbInstrumentLooping[ i ] > 0 )
              gbInstrumentCursor[ i ] = gbInstrumentLength[ i ] - gbInstrumentLooping[ i ];
            else
              gbStopNoteChannel( i );
        }
    }

    if( !gbNotePlaying[ i ] )
      return; // gbStopNoteChannel() above may have just ended the note this same tick

    gbInstrumentNextChange[ i ] = gbInstrumentNextChange[ i ] - 1;
    gbCommandsCounter[ i ] = gbCommandsCounter[ i ] + 1;

    // Real outputPitch[]/outputVolume[] are uint8_t/int8_t (Sound.h), and
    // EVERY assignment to them (including the += below) narrows the real
    // result to that real width before anything downstream ever reads it -
    // in particular, real outputPitch[i] is guaranteed 0-255 by the time
    // the final "(x+NUM_PITCH)%NUM_PITCH" wrap runs, making that wrap
    // always safe. This dialect's `int` never narrows, so a real, genuinely
    // reachable case - a large negative arpeggio step compounding over
    // many ticks against a long-sustained note (confirmed live: Copter's
    // own real machine-gun soundfx, arpeggio step -46 every 2 ticks over a
    // 20-tick note) - drove outputPitch deep into negative territory
    // instead of wrapping, indexing gbHalfPeriods[] out of bounds and
    // crashing on whatever garbage word it read as a halfPeriod (a real
    // divide-by-zero trap this platform enforces that real hardware simply
    // doesn't have). Fixed by reproducing the real uint8_t/int8_t
    // narrowing explicitly (&0xFF / a signed 8-bit narrow) at each real
    // assignment point, matching this project's own established "replicate
    // real AVR narrow-int behavior explicitly" precedent (see CLAUDE.md's
    // own EEPROM-narrowing audit) rather than leaving this dialect's wider
    // int to diverge.
    int outputPitch = gbNotePitch[ i ] + gbStepPitch[ i ] + gbTrackPatternPitch[ i ];
    outputPitch = outputPitch & 0xFF;
    if( gbArpeggioStepDuration[ i ] != 0 )
    {
        outputPitch = outputPitch + ( gbCommandsCounter[ i ] / gbArpeggioStepDuration[ i ] ) * gbArpeggioStepSize[ i ];
        outputPitch = outputPitch & 0xFF;
    }
    outputPitch = ( outputPitch + GB_NUM_PITCH ) % GB_NUM_PITCH; // now always safe: outputPitch is 0-255 here, so this is always 0-35

    int outputVolume = gbNoteVolume[ i ];
    outputVolume = gbNarrowInt8( outputVolume );
    if( gbVolumeSlideStepDuration[ i ] != 0 )
    {
        outputVolume = outputVolume + ( gbCommandsCounter[ i ] / gbVolumeSlideStepDuration[ i ] ) * gbVolumeSlideStepSize[ i ];
        outputVolume = gbNarrowInt8( outputVolume );
    }
    if( gbTremoloStepDuration[ i ] != 0 )
    {
        outputVolume = outputVolume + ( ( gbCommandsCounter[ i ] / gbTremoloStepDuration[ i ] ) % 2 ) * gbTremoloStepSize[ i ];
        outputVolume = gbNarrowInt8( outputVolume );
    }
    if( outputVolume < 0 ) outputVolume = 0;
    if( outputVolume > 9 ) outputVolume = 9;
    if( gbNotePitch[ i ] == 63 ) outputVolume = 0; // real hardware's own "rest" pitch sentinel

    float freqHz = GB_SOUND_ISR_HZ / ( 2.0 * (float)gbHalfPeriods[ outputPitch ] );

    // Normalized 0..1, not real hardware's own literal 8-bit PWM duty
    // math (`(outputVolume*chanVolumes*stepVolume << globalVolume) / 128`)
    // - chanVolumes/globalVolume exist on real hardware purely to keep
    // several real channels summed into ONE shared physical PWM output
    // from clipping, which doesn't apply here (each Vircon32 SPU channel
    // mixes independently in hardware, and this project's own separate
    // set_global_volume()-based mute toggle already covers project-wide
    // volume). outputVolume (0-9) and stepVolume (0-7) alone still carry
    // every real per-note/per-instrument-step dynamic this engine
    // produces (CMD_VOLUME, volume-slide, tremolo, and each instrument
    // step's own real stepVolume field).
    float volume01 = ( (float)( outputVolume * gbStepVolume[ i ] ) ) / ( 9.0 * 7.0 );
    if( volume01 < 0.0 ) volume01 = 0.0;
    if( volume01 > 1.0 ) volume01 = 1.0;

    if( gbActiveVoice[ i ] < 0 )
      gbActiveVoice[ i ] = md_trackerVoiceStart( freqHz, volume01 );
    else
      md_trackerVoiceRetune( gbActiveVoice[ i ], freqHz, volume01 );
}

// Real Sound::stopPattern(channel).
void gbStopPattern( int channel )
{
    if( channel < 0 || channel >= MAX_SOUND_CHANNELS )
      return;

    gbStopNoteChannel( channel );
    gbPatternIsPlaying[ channel ] = false;
}

void gbStopPatternAll()
{
    int i;
    for( i = 0; i < MAX_SOUND_CHANNELS; i++ )
      gbStopPattern( i );
}

// Real Sound::playPattern(pattern, channel) - starts a real pattern (a
// 0-terminated array of packed 16-bit note/command words - see the doc
// comment on gbPlayOK() further below for the exact real bit layout) on
// one channel. Resets that channel's own volume to 9 (real hardware's own
// max) and clears any running slide/arpeggio/tremolo effect, matching
// real Sound::playPattern() exactly - a pattern's own command words are
// then free to set up whatever state its own first note actually wants.
void gbPlayPattern( int* pattern, int channel )
{
    if( channel < 0 || channel >= MAX_SOUND_CHANNELS )
      return;

    gbStopPattern( channel );
    gbPatternData[ channel ] = pattern;
    gbPatternCursor[ channel ] = 0;
    gbPatternIsPlaying[ channel ] = true;
    gbNoteVolume[ channel ] = 9;
    gbVolumeSlideStepDuration[ channel ] = 0;
    gbArpeggioStepDuration[ channel ] = 0;
    gbTremoloStepDuration[ channel ] = 0;
}

void gbSetPatternLooping( bool loop, int channel )
{
    if( channel < 0 || channel >= MAX_SOUND_CHANNELS )
      return;
    gbPatternLooping[ channel ] = loop;
}

// Real Sound::updateTrack(channel) - advances to a track's own next
// pattern (looked up by ID through that channel's own gbChangePatternSet()
// array) once the previous one finishes. patternPitch is a real signed
// per-pattern transposition, packed into the track word's own upper byte
// (0-255 unsigned on the wire) - real hardware stores it into a genuine
// signed int8_t field, narrowing 128-255 into a negative value the same
// way this project's own EEPROM-narrowing audit already documented
// elsewhere (see CLAUDE.md); reproduced explicitly here since it's real,
// load-bearing transposition behavior, not an incidental byte width.
void gbAdvanceTrackChannel( int i )
{
    if( !gbTrackIsPlaying[ i ] )
      return;
    if( gbPatternIsPlaying[ i ] )
      return;

    int data = gbTrackData[ i ][ gbTrackCursor[ i ] ];
    if( data == 0xFFFF )
    {
        gbTrackIsPlaying[ i ] = false;
        return;
    }

    int patternID = data & 0x00FF;
    int rawPitchByte = ( data >> 8 ) & 0x00FF;
    if( rawPitchByte > 127 )
      rawPitchByte = rawPitchByte - 256; // real int8_t narrowing
    gbTrackPatternPitch[ i ] = rawPitchByte;

    gbPlayPattern( gbPatternSet[ i ][ patternID ], i );
    gbTrackCursor[ i ] = gbTrackCursor[ i ] + 1;
}

// Real Sound::updatePattern(channel) - called automatically once per real
// tick for every channel with a pattern currently playing (see
// gbUpdateSoundTracker() below); once the current note finishes, reads and
// applies every command word at the pattern cursor (a real command word
// has bit0 set - see this function's own real bit-unpacking below, matching
// gbPlayOK()'s own doc comment further down), then plays the note word
// immediately following them.
void gbUpdatePatternChannel( int i )
{
    if( !gbPatternIsPlaying[ i ] )
      return;
    if( gbNoteDuration[ i ] != 0 )
      return;

    int data = gbPatternData[ i ][ gbPatternCursor[ i ] ];

    if( data == 0 )
    {
        if( gbPatternLooping[ i ] )
        {
            gbPatternCursor[ i ] = 0;
            data = gbPatternData[ i ][ gbPatternCursor[ i ] ];
        }
        else
        {
            gbPatternIsPlaying[ i ] = false;
            if( gbTrackIsPlaying[ i ] )
            {
                gbAdvanceTrackChannel( i );
                data = gbPatternData[ i ][ gbPatternCursor[ i ] ];
            }
            else
            {
                gbStopNoteChannel( i );
                return;
            }
        }
    }

    while( ( data & 0x0001 ) != 0 )
    {
        data = data >> 2;
        int cmd = data & 0x000F;
        data = data >> 4;
        int X = data & 0x001F;
        data = data >> 5;
        int Y = data - 16;
        gbSoundCommand( cmd, X, Y, i );
        gbPatternCursor[ i ] = gbPatternCursor[ i ] + 1;
        data = gbPatternData[ i ][ gbPatternCursor[ i ] ];
    }
    data = data >> 2;

    int pitch = data & 0x003F;
    data = data >> 6;
    int duration = data;

    gbPlayNoteChannel( pitch, duration, i );
    gbPatternCursor[ i ] = gbPatternCursor[ i ] + 1;
}

// Real Sound::stopTrack(channel).
void gbStopTrack( int channel )
{
    if( channel < 0 || channel >= MAX_SOUND_CHANNELS )
      return;
    gbTrackIsPlaying[ channel ] = false;
    gbStopPattern( channel );
}

void gbStopTrackAll()
{
    int i;
    for( i = 0; i < MAX_SOUND_CHANNELS; i++ )
      gbStopTrack( i );
}

// Real Sound::playTrack(track, channel) - starts a real track (a
// 0xFFFF-terminated array of packed pattern-ID+transposition words) on one
// channel; that channel needs a real gbChangePatternSet() call first so
// gbAdvanceTrackChannel() above knows which real pattern each ID refers
// to, matching real upstream's own identical `changePatternSet()`-before-
// `playTrack()` call order.
void gbPlayTrack( int* track, int channel )
{
    if( channel < 0 || channel >= MAX_SOUND_CHANNELS )
      return;

    gbStopTrack( channel );
    gbTrackCursor[ channel ] = 0;
    gbTrackData[ channel ] = track;
    gbTrackIsPlaying[ channel ] = true;
}

// Real Sound::changePatternSet(patterns, channel)/changeInstrumentSet(
// instruments, channel) - registers the real array-of-pattern-pointers (or
// array-of-instrument-pointers) a later gbPlayTrack()/gbSoundCommand(
// GB_CMD_INSTRUMENT,...) call looks IDs up against.
void gbChangePatternSet( int** patterns, int channel )
{
    if( channel < 0 || channel >= MAX_SOUND_CHANNELS )
      return;
    gbPatternSet[ channel ] = patterns;
}

void gbChangeInstrumentSet( int** instruments, int channel )
{
    if( channel < 0 || channel >= MAX_SOUND_CHANNELS )
      return;
    gbInstrumentSet[ channel ] = instruments;
}

// Real Sound::begin()'s own per-channel default wiring (the real square-
// wave instrument, selected as each channel's own starting instrument) -
// called once per game launch from gbBegin(), so a freshly-launched game
// never inherits a previous game's own leftover pattern/track/note/voice
// state (no equivalent scenario exists on real hardware, which only ever
// powers on once per session - the same adaptation gbFrameCount/
// gbPopupTimeLeft already make in gbBegin() for the identical reason).
void gbInitSoundEngine()
{
    int i;
    for( i = 0; i < MAX_SOUND_CHANNELS; i++ )
    {
        // A real, previously-live bug: this used to overwrite
        // gbActiveVoice[i] with -1 directly, without ever telling the
        // backend to actually stop whatever voice was still sounding
        // there - harmless on a port with a large shared voice pool (a
        // leaked voice is just one wasted slot among many), but a real,
        // audible "stuck on the same tone forever" bug on a port with a
        // small DEDICATED tracker pool sized to exactly MAX_SOUND_CHANNELS
        // (confirmed live on the sibling gamebuino_classic_sdl project's
        // own Playdate port, whose 4-slot pool leaves zero slack for even
        // one leaked voice before the next 4-simultaneous-channel moment
        // steals it out from under its real owner). gbStopNoteChannel()
        // already does this correctly (guards on gbActiveVoice[i]>=0
        // before calling md_trackerVoiceStop()) - call it instead of
        // hand-rolling the same reset without the stop.
        gbStopNoteChannel( i );

        gbTrackIsPlaying[ i ] = false;
        gbPatternIsPlaying[ i ] = false;
        gbPatternLooping[ i ] = false;
        gbVolumeSlideStepDuration[ i ] = 0;
        gbArpeggioStepDuration[ i ] = 0;
        gbTremoloStepDuration[ i ] = 0;
        gbTrackPatternPitch[ i ] = 0;

        gbChangeInstrumentSet( gbDefaultInstruments, i );
        gbSoundCommand( GB_CMD_INSTRUMENT, 0, 0, i ); // real Sound::begin()'s own default square-wave instrument
    }
}

// Real Gamebuino::update()'s own automatic sound.updateTrack()/
// updatePattern()/updateNote() tail call - see gbRenderFrame()'s own call
// site above. Each of the three real sub-steps loops over every channel
// on its own before the next sub-step starts (matching real hardware's own
// exact call order), not interleaved per-channel.
void gbUpdateSoundTracker()
{
    int i;
    for( i = 0; i < MAX_SOUND_CHANNELS; i++ )
      gbAdvanceTrackChannel( i );
    for( i = 0; i < MAX_SOUND_CHANNELS; i++ )
      gbUpdatePatternChannel( i );
    for( i = 0; i < MAX_SOUND_CHANNELS; i++ )
      gbUpdateNoteChannel( i );
}

// gbPlayNote(pitch, duration) - the common single-channel convenience
// form real upstream code almost always actually calls (channel 0).
// pitch is a direct 0-35 _halfPeriods index - see this section's own
// header comment above for how this was confirmed (Elventure's own real
// sound_data.h note-name constants, cross-checked against the real
// playOK()/playCancel() pattern data decoded below).
void gbPlayNote( int pitch, int duration )
{
    gbPlayNoteChannel( pitch, duration, 0 );
}

// Real hardware's own exact playOK()/playCancel()/playTick() patterns
// (Sound.cpp: playOKPattern={0x0005,0x138,0x168,0x0000}, playCancelPattern
// ={0x0005,0x168,0x138,0x0000}, playTickP={0x0045,0x168,0x0000}), decoded
// by hand against the real pattern-word bit layout (Sound::updatePattern())
// and the real 36-entry _halfPeriods table (EXTENDED_NOTE_RANGE's own
// default of 0): each note word packs a pitch (bits 2-7, a direct 0-35
// index into _halfPeriods) and a duration (bits 8-15, in real display
// frames) after a leading 2-bit command-flag/reserved pair; a command word
// (bit0 set) instead selects an instrument - 0 (square) for OK/Cancel, 1
// (noise) for Tick - before the note(s) that follow it. Real audible
// frequency = GB_SOUND_ISR_HZ / (2*halfPeriod) - see that constant's own
// doc comment above for the real ISR-rate derivation:
//   OK:     pitch 14 (halfPeriod 110 -> 258.82Hz) then pitch 26
//           (halfPeriod 55 -> 517.63Hz) - a rising 2-note blip.
//   Cancel: the same two notes, reversed - a falling 2-note blip.
//   Tick:   pitch 26 (517.63Hz) played through the real noise instrument -
//           a pseudorandom-amplitude buzz at that underlying rate, not a
//           clean tone; this shim's own plain single-cycle-wavetable tone
//           engine (PlayNote) has no equivalent for real noise, so it
//           plays as a plain tone at the real pitch/duration instead.
// Duration is real display frames, not a fixed wall-clock time - scales
// with gbFrameRateFps exactly like gbPlayNote() itself already does,
// matching real hardware's own frame-tied timing precisely.
//
// Real hardware plays every pattern's notes strictly sequentially (one
// voice, one note at a time) - this shim's own genuinely multi-voice
// md_playTone() instead starts both of OK/Cancel's two notes at once, so
// each plays as a single short overlapping 2-note chord (a rising/falling
// octave dyad, since 517.63/258.82 = 2.0 exactly) rather than a true
// two-step melody.
// Real playOKPattern/playCancelPattern/playTickP (Sound.cpp) - each word is
// either a NOTE (pitch<<2 | duration<<8) or a COMMAND (bit0 set), decoded by
// hand against the real bit layout gbSoundCommand()/gbUpdatePatternChannel()
// both already implement: 0x0005 selects instrument 0 (square), 0x0045
// selects instrument 1 (noise), 0x0138 is NOTE(pitch=14,duration=1), 0x0168
// is NOTE(pitch=26,duration=1). Real playOK() plays pitch14-then-pitch26 as
// a genuine two-note SEQUENCE (not a simultaneous chord); playCancel() is
// the exact same two notes in reverse order - the two only actually sound
// different from each other because they're sequenced in time, matching
// real `Sound::playOK()`/`playCancel()`, both of which call
// `playPattern(...,0)` directly.
int[4] gbPlayOKPattern     = { 0x0005, 0x0138, 0x0168, 0x0000 };
int[4] gbPlayCancelPattern = { 0x0005, 0x0168, 0x0138, 0x0000 };
int[3] gbPlayTickPattern   = { 0x0045, 0x0168, 0x0000 };

void gbPlayTick()
{
    gbPlayPattern( gbPlayTickPattern, 0 );
}

void gbPlayOK()
{
    gbPlayPattern( gbPlayOKPattern, 0 );
}

void gbPlayCancel()
{
    gbPlayPattern( gbPlayCancelPattern, 0 );
}

// -----------------------------------------------------------------------------
// Collision helpers - direct ports of Gamebuino::collide*()
// -----------------------------------------------------------------------------

bool gbCollidePointRect( int x1, int y1, int x2, int y2, int w, int h )
{
    return ( x1 >= x2 ) && ( x1 < x2 + w ) && ( y1 >= y2 ) && ( y1 < y2 + h );
}

bool gbCollideRectRect( int x1, int y1, int w1, int h1, int x2, int y2, int w2, int h2 )
{
    return ( x1 + w1 > x2 ) && ( x1 < x2 + w2 ) && ( y1 + h1 > y2 ) && ( y1 < y2 + h2 );
}

// Direct port of real Display::getBitmapPixel() - see gbCollideBitmapBitmap()'s
// own comment and gamebuinoShim.h's own doc comment on both.
bool gbGetBitmapPixel( int* bitmap, int x, int y )
{
    int w = bitmap[ 0 ];
    int byteWidth = ( w + 7 ) / 8;
    return ( bitmap[ 2 + y * byteWidth + ( x / 8 ) ] & ( 0x80 >> ( x % 8 ) ) ) != 0;
}

// Direct port of real Gamebuino::collideBitmapBitmap() (utility/Display.cpp/
// Gamebuino.cpp) - same bounding-rect-reject-first optimization, same
// per-pixel AND-of-both-bitmaps overlap test over just the overlapping
// sub-rectangle.
bool gbCollideBitmapBitmap( int x1, int y1, int* b1, int x2, int y2, int* b2 )
{
    int w1 = b1[ 0 ];
    int h1 = b1[ 1 ];
    int w2 = b2[ 0 ];
    int h2 = b2[ 1 ];
    int xmin, ymin, xmax, ymax, x, y;

    if( !gbCollideRectRect( x1, y1, w1, h1, x2, y2, w2, h2 ) )
      return false;

    if( x1 >= x2 ) xmin = 0; else xmin = x2 - x1;
    if( y1 >= y2 ) ymin = 0; else ymin = y2 - y1;
    if( x1 + w1 >= x2 + w2 ) xmax = x2 + w2 - x1; else xmax = w1;
    if( y1 + h1 >= y2 + h2 ) ymax = y2 + h2 - y1; else ymax = h1;

    for( y = ymin; y < ymax; y++ )
    {
        for( x = xmin; x < xmax; x++ )
        {
            if( gbGetBitmapPixel( b1, x, y ) && gbGetBitmapPixel( b2, x1 + x - x2, y1 + y - y2 ) )
              return true;
        }
    }
    return false;
}

