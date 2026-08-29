// Snake (Sigma-Squared, no license specified) - a third, independent Snake
// alongside this cartridge's own already-shipped Snake Classic
// (Ripper121/Tnxec2) and Snake 5110 (Lady Awesome & MakerSquirrel), and
// genuinely unrelated to either: it plays on a half-resolution 42x24 grid
// with every cell drawn as a 2x2 block, wraps freely through all four edges
// instead of dying on them, starts ten segments long, grows three at a time,
// and animates its body with a checkerboard that flips every frame. Button A
// held is a real speed boost (one tick per frame instead of one in three).
//
// Every real `gb.x.y(...)` call site is mechanically rewritten to a plain
// `gbY(...)` function call (see gamePong.c's own header comment for why -
// this dialect has no classes/methods/operator overloading). Every global
// symbol got an `snsg`-prefixed name (this cartridge has no linker - every
// ported game shares one flat global namespace). Upstream's own
// `random(n)` calls became `arand(n)`.
//
// C++ CONSTRUCTS REWRITTEN: upstream's own `auto &x = snake[2*i]` reference
// bindings (used to alias a segment's own x/y cells in place) became direct
// indexed reads and writes, and its `String(score) + String(" | ") +
// max_score` Arduino-String concatenations became separate print calls -
// neither has an equivalent in this dialect, and neither changes what is
// drawn. Its own `enum GAME_STATE_T` became plain defines, and its
// `frame_delay = gb.buttons.repeat(BTN_A,1) ? 1 : 3;` an explicit if/else
// (this dialect has no ternary operator).
//
// A REAL FONT-SIZE GAP THIS GAME CLOSED: its game-over screen prints at
// `fontSize = 3`, and this shim's own gbDrawCharPixel() only ever handled
// sizes 1 and 2 - a documented simplification that no earlier port had
// needed past 2. Real Display::drawChar() has no such limit: it draws any
// size with `fillRect(x + i*size, y + j*size, size, size)`. gbDrawCharPixel()
// now does exactly that for every size above 1, keeping real hardware's own
// drawPixel() fast path for size 1 - a fidelity fix in the shared shim, not
// a workaround in this file.
//
// A REAL SAVE-BREAKING BUG, FIXED: `max_score` is a single EEPROM byte read
// raw at game start, and a factory-erased cell reads 255 - the maximum a
// `byte` score can ever reach. `if (score > max_score)` can then never be
// true, so a fresh cartridge could never record its first high score at all.
// Fixed with this project's own established fresh-cell reset (255 -> 0, see
// gameGruniozerca.c/gameCrazyTown.c for the same pattern applied to the same
// class of bug). The trade-off is the same one gameGruniozerca.c documents:
// 255 is both the fresh-cell sentinel and a legitimately reachable score, so
// a genuinely earned 255 would be reset too - accepted as vanishingly
// unlikely rather than solved with a second "has been played" byte.
//
// `EEPROM.update()` (write only if the value actually differs) has no
// separate equivalent here; `eeprom_write_byte()` is used instead, which is
// the same operation minus a flash-wear optimisation this platform's
// memory-card-backed store does not need.
//
// BLOCKING TITLE SCREEN -> AN EXPLICIT STATE: real `gb.titleScreen(F("Snake"),
// logo)` blocks until Button A, and is re-entered on Button C. Hand-rolled
// here as one more explicit state; the real 64x30 logo was converted from
// upstream's own PROGMEM table to hex byte-for-byte by script.

#include "../gamebuinoShim.h"

#define SNSG_INITIAL_SEG 10
#define SNSG_MAX_SEG 256
#define SNSG_SEG_ADDED 3
#define SNSG_WWIDTH 42  // LCDWIDTH / 2
#define SNSG_WHEIGHT 24 // LCDHEIGHT / 2

#define SNSG_PLAY 0
#define SNSG_GAMEOVER 1

#define SNSG_STATE_TITLE 0
#define SNSG_STATE_GAME  1

// Real hardware stores the high score in EEPROM address 0.
#define SNSG_EEPROM_ADDR 0

int snsgNumSeg;
int snsgFrameDelay;
int[2 * SNSG_MAX_SEG] snsgSnake;
int snsgVX, snsgVY;
int snsgFX, snsgFY;
int snsgScore;
int snsgMaxScore;
int snsgGameState;
int snsgState;

// Real upstream's own 64x30 title logo, converted from its own PROGMEM
// table to hex byte-for-byte by script.
int[242] snsgLogoBitmap =
{
    0x40, 0x1E, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xC0,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xA0,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xA0,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xC0,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xF8,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0xF4,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x9C,
    0x00, 0x00, 0x00, 0x00, 0x1C, 0x3D, 0xE3, 0x03,
    0x00, 0x00, 0x00, 0x08, 0x63, 0x42, 0x17, 0x07,
    0x00, 0x00, 0x00, 0x0F, 0xE1, 0xC3, 0x09, 0xFE,
    0x00, 0x00, 0x00, 0x08, 0x10, 0xA1, 0x8F, 0xFF,
    0x80, 0x00, 0x00, 0x0C, 0x08, 0xA1, 0x8F, 0x7F,
    0xC0, 0x00, 0x00, 0x0A, 0x18, 0xF1, 0x8B, 0xF2,
    0xE0, 0x00, 0x00, 0x0B, 0xF8, 0xE1, 0x89, 0xC1,
    0x80, 0x00, 0x00, 0x09, 0x60, 0xA1, 0x88, 0xE0,
    0x00, 0x00, 0x00, 0x08, 0x60, 0x81, 0x88, 0x60,
    0x00, 0x00, 0x00, 0x07, 0xA1, 0x81, 0x89, 0xD8,
    0x00, 0x00, 0x00, 0x00, 0x13, 0x42, 0x89, 0x34,
    0x00, 0x00, 0x00, 0x00, 0x04, 0x3C, 0x8A, 0xBE,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x8A, 0xBF,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x87, 0x01,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x01,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x81,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x40, 0x42,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x30, 0xBC,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1F, 0x06,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x07,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03,
    0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00
};

void snsgGameStart()
{
    int raw;

    snsgGameState = SNSG_PLAY;
    snsgFX = arand( SNSG_WWIDTH );
    snsgFY = arand( SNSG_WHEIGHT );
    snsgNumSeg = SNSG_INITIAL_SEG;

    for( int i = 0; i < snsgNumSeg; i++ )
    {
        snsgSnake[ 2 * i ] = SNSG_WWIDTH / 2 - i;
        snsgSnake[ 2 * i + 1 ] = SNSG_WHEIGHT / 2;
    }

    snsgVX = 1;
    snsgVY = 0;
    snsgScore = 0;

    raw = eeprom_read_byte( SNSG_EEPROM_ADDR );

    // A factory-erased cell reads 255, which no score could ever beat - see
    // the header comment.
    if( raw == 255 )
      raw = 0;

    snsgMaxScore = raw;
}

void snsgGameOver()
{
    snsgGameState = SNSG_GAMEOVER;

    if( snsgScore > snsgMaxScore )
    {
        snsgMaxScore = snsgScore;
        eeprom_write_byte( SNSG_EEPROM_ADDR, snsgMaxScore );
    }
}

void snsgInput()
{
    if( gbPressed( BTN_C ) )
    {
        gbPlayCancel();
        snsgState = SNSG_STATE_TITLE;
        return;
    }

    if( snsgGameState == SNSG_PLAY )
    {
        bool up = gbPressed( BTN_UP );
        bool down = gbPressed( BTN_DOWN );
        bool left = gbPressed( BTN_LEFT );
        bool right = gbPressed( BTN_RIGHT );

        if( up && snsgVY != 1 )
        {
            snsgVX = 0;
            snsgVY = -1;
        }
        else if( down && snsgVY != -1 )
        {
            snsgVX = 0;
            snsgVY = 1;
        }
        else if( right && snsgVX != -1 )
        {
            snsgVX = 1;
            snsgVY = 0;
        }
        else if( left && snsgVX != 1 )
        {
            snsgVX = -1;
            snsgVY = 0;
        }

        if( gbRepeat( BTN_A, 1 ) )
          snsgFrameDelay = 1;
        else
          snsgFrameDelay = 3;
    }
    else
    {
        if( gbPressed( BTN_A ) || gbPressed( BTN_B ) )
        {
            gbPlayOK();
            snsgGameStart();
        }
    }
}

void snsgInternalUpdate()
{
    int x, y;

    if( snsgGameState != SNSG_PLAY )
      return;

    for( int i = snsgNumSeg - 1; i >= 0; i-- )
    {
        if( i == 0 )
        {
            x = snsgSnake[ 0 ] + snsgVX;
            y = snsgSnake[ 1 ] + snsgVY;
            x = ( x + SNSG_WWIDTH ) % SNSG_WWIDTH;
            y = ( y + SNSG_WHEIGHT ) % SNSG_WHEIGHT;
            snsgSnake[ 0 ] = x;
            snsgSnake[ 1 ] = y;

            for( int j = 1; j < snsgNumSeg; j++ )
              if( x == snsgSnake[ 2 * j ] && y == snsgSnake[ 2 * j + 1 ] )
              {
                  snsgGameOver();
                  return;
              }

            if( x == snsgFX && y == snsgFY )
            {
                gbPlayTick();

                if( snsgNumSeg + SNSG_SEG_ADDED < SNSG_MAX_SEG )
                {
                    for( int j = 0; j < SNSG_SEG_ADDED; j++ )
                    {
                        snsgSnake[ 2 * ( snsgNumSeg + j ) ] = snsgFX;
                        snsgSnake[ 2 * ( snsgNumSeg + j ) + 1 ] = snsgFY;
                    }

                    snsgNumSeg = snsgNumSeg + SNSG_SEG_ADDED;
                }

                snsgFX = arand( SNSG_WWIDTH );
                snsgFY = arand( SNSG_WHEIGHT );
                snsgScore++;
            }
        }
        else
        {
            x = snsgSnake[ 2 * ( i - 1 ) ];
            y = snsgSnake[ 2 * ( i - 1 ) + 1 ];
            x = ( x + SNSG_WWIDTH ) % SNSG_WWIDTH;
            y = ( y + SNSG_WHEIGHT ) % SNSG_WHEIGHT;
            snsgSnake[ 2 * i ] = x;
            snsgSnake[ 2 * i + 1 ] = y;
        }
    }
}

void snsgUpdate()
{
    if( gbFrameCount % snsgFrameDelay == 0 )
      snsgInternalUpdate();
}

void snsgDraw2x2( int x, int y )
{
    gbDrawPixel( x, y );
    gbDrawPixel( x, y + 1 );
    gbDrawPixel( x + 1, y );
    gbDrawPixel( x + 1, y + 1 );
}

void snsgDraw()
{
    if( snsgGameState == SNSG_PLAY )
    {
        for( int i = 0; i < snsgNumSeg; i++ )
        {
            int x = snsgSnake[ i * 2 ];
            int y = snsgSnake[ i * 2 + 1 ];

            if( i == 0 )
            {
                snsgDraw2x2( 2 * x, 2 * y );
            }
            else if( gbFrameCount & 1 )
            {
                gbDrawPixel( 2 * x, 2 * y );
                gbDrawPixel( 2 * x + 1, 2 * y + 1 );
            }
            else
            {
                gbDrawPixel( 2 * x + 1, 2 * y );
                gbDrawPixel( 2 * x, 2 * y + 1 );
            }
        }

        gbSetColor( GB_BLACK );
        snsgDraw2x2( 2 * snsgFX, 2 * snsgFY );

        gbFontSize = 1;
        gbPrintNumber( snsgScore );
        gbPrintString( " | " );
        gbPrintNumber( snsgMaxScore );
    }
    else
    {
        gbFontSize = 3;
        gbPrintString( "Game\nOver!\n" );
        gbFontSize = 2;
        gbPrintString( "score: " );
        gbPrintNumber( snsgScore );
        gbPrintString( "\n" );
        gbFontSize = 1;
    }
}

// Stands in for the blocking gb.titleScreen(F("Snake"), logo) call.
void snsgUpdateTitle()
{
    gbFontSize = 1;
    gbDrawBitmap( ( LCDWIDTH - 64 ) / 2, 4, snsgLogoBitmap );

    gbCursorX = 24;
    gbCursorY = 40;
    gbPrintString( "PRESS A" );

    if( gbPressed( BTN_A ) )
    {
        gbPlayOK();
        snsgState = SNSG_STATE_GAME;
        snsgGameStart();
    }
}

void gameSnakeSigma_init()
{
    gbBegin();
    gbPickRandomSeed(); // no-op, see gamebuinoShim.h's own header comment

    snsgFrameDelay = 3;
    snsgState = SNSG_STATE_TITLE;
    snsgGameStart();
}

void gameSnakeSigma_update()
{
    if( !gbUpdate() ) return;

    if( snsgState == SNSG_STATE_TITLE )
    {
        snsgUpdateTitle();
        gbRenderFrame();
        return;
    }

    snsgInput();

    if( snsgState == SNSG_STATE_TITLE )
    {
        gbRenderFrame();
        return;
    }

    snsgUpdate();
    snsgDraw();

    gbRenderFrame();
}
