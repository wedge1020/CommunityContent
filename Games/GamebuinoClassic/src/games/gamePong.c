// Pong Solo (Aurelien Rodot, LGPLv3 - the official Gamebuino Classic
// library's own bundled "2.Intermediate/Pong" example sketch,
// github.com/Gamebuino/Gamebuino-Classic). The first port for this
// project, picked specifically to prove the gamebuinoShim.h/.c
// compatibility layer against a small, complete, real game before
// porting anything larger - the same "prove the shim on 2-3 simple games
// first" discipline the sibling tinyjoypad_vircon32 project used for its
// own shim.
//
// A single-player Pong: move a paddle up/down, a simple AI opponent
// tracks the ball, first to 10 points resets the score.
//
// Every real `gb.x.y(...)` call site was mechanically rewritten to a
// plain `gbY(...)` function call (this dialect has no classes/methods -
// see gamebuinoShim.h's own header comment for the full reasoning);
// `gb.display.cursorX =`/`gb.display.cursorY =` ported unchanged, since
// gbCursorX/gbCursorY are themselves plain globals here too, and
// `gb.display.print(...)` became `gbPrintNumber()`/`gbPrintString()`
// (this dialect has no function overloading, so the real Print class's
// single overloaded print() needed splitting into two). `max()`/`min()`
// (Arduino macros, not
// available here) became `gbMax()`/`gbMin()` (real functions - no
// ternary operator in this dialect, so a `(a>b?a:b)`-style macro
// wouldn't have compiled either). `random(0, N)` (Arduino's ranged
// random) became `arand(N)` (this dialect's own established RNG
// helper - see avrCompat.h).
//
// Upstream's own blocking `gb.titleScreen(F("Pong Solo"))` - called once
// in setup(), and again from inside loop() as a genuine "pause the game"
// gesture when Button C is pressed - was converted into an explicit
// PONG_STATE_TITLE state (shown at boot, and re-entered on a C press
// mid-game), matching the "blocking loop -> explicit resumable state"
// treatment used throughout the sibling project. Real Gamebuino's own
// titleScreen() waits specifically for Button A, matching Vircon32's own
// menu-select button - reused directly here as the dismiss gesture.
//
// `gb.display.setFont(font5x7)` is restored as a real `gbSetFont(
// gbFont5x7)` call in gamePong_init() - upstream calls it once in setup()
// and never switches back, so it stays set for the whole game including
// the title screen text, exactly like real hardware (this shim originally
// had no second, smaller font to switch away from at all, before
// gbSetFont()/real fonts existed - see gamebuinoShim.h's own header
// comment). `gb.pickRandomSeed()` became
// `gbPickRandomSeed()`, a documented no-op (Vircon32's own `rand()` isn't
// manually seedable the same way, matching this whole project's own
// established precedent for every other upstream `randomSeed()`-style
// call). `gb.battery.show = false;` was dropped outright rather than
// built into a no-op battery-indicator system - purely cosmetic on real
// hardware, irrelevant here.

int pongPlayerScore = 0;
int pongPlayerH = 16;
int pongPlayerW = 3;
int pongPlayerX = 0;
int pongPlayerY;
int pongPlayerVy = 2;

int pongOpponentScore = 0;
int pongOpponentH = 16;
int pongOpponentW = 3;
int pongOpponentX;
int pongOpponentY;
int pongOpponentVy = 2;

int pongBallSize = 6;
int pongBallX;
int pongBallY;
int pongBallVx = 3;
int pongBallVy = 3;

enum PongState
{
    PONG_STATE_TITLE = 0,
    PONG_STATE_PLAY = 1
};

int pongState;

void pongResetPositions()
{
    pongPlayerY = ( LCDHEIGHT - pongPlayerH ) / 2;
    pongOpponentX = LCDWIDTH - pongOpponentW;
    pongOpponentY = ( LCDHEIGHT - pongOpponentH ) / 2;
    pongBallSize = 6;
    pongBallX = LCDWIDTH - pongBallSize - pongOpponentW - 1;
    pongBallY = ( LCDHEIGHT - pongBallSize ) / 2;
    pongBallVx = 3;
    pongBallVy = 3;
}

void pongBeginTitle()
{
    pongState = PONG_STATE_TITLE;
}

void pongBeginPlay()
{
    pongState = PONG_STATE_PLAY;
    gbFontSize = 2;
}

void pongUpdateTitle()
{
    gbSetColor( 1 );
    gbCursorX = 15;
    gbCursorY = 16;
    gbFontSize = 1;
    gbPrintString( "PONG SOLO" );
    gbCursorX = 8;
    gbCursorY = 32;
    gbPrintString( "PRESS A" );

    if( gbPressed( BTN_A ) )
      pongBeginPlay();
}

void pongUpdatePlay()
{
    // pause the game if C is pressed
    if( gbPressed( BTN_C ) )
    {
        pongBeginTitle();
        return;
    }

    // move the player
    if( gbRepeat( BTN_UP, 1 ) )
      pongPlayerY = gbMax( 0, pongPlayerY - pongPlayerVy );
    if( gbRepeat( BTN_DOWN, 1 ) )
      pongPlayerY = gbMin( LCDHEIGHT - pongPlayerH, pongPlayerY + pongPlayerVy );

    // move the ball
    pongBallX = pongBallX + pongBallVx;
    pongBallY = pongBallY + pongBallVy;

    // collision with the top border
    if( pongBallY < 0 )
    {
        pongBallY = 0;
        pongBallVy = -pongBallVy;
        gbPlayTick();
    }
    // collision with the bottom border
    if( ( pongBallY + pongBallSize ) > LCDHEIGHT )
    {
        pongBallY = LCDHEIGHT - pongBallSize;
        pongBallVy = -pongBallVy;
        gbPlayTick();
    }
    // collision with the player
    if( gbCollideRectRect( pongBallX, pongBallY, pongBallSize, pongBallSize, pongPlayerX, pongPlayerY, pongPlayerW, pongPlayerH ) )
    {
        pongBallX = pongPlayerX + pongPlayerW;
        pongBallVx = -pongBallVx;
        gbPlayTick();
    }
    // collision with the opponent
    if( gbCollideRectRect( pongBallX, pongBallY, pongBallSize, pongBallSize, pongOpponentX, pongOpponentY, pongOpponentW, pongOpponentH ) )
    {
        pongBallX = pongOpponentX - pongBallSize;
        pongBallVx = -pongBallVx;
        gbPlayTick();
    }
    // collision with the left side
    if( pongBallX < 0 )
    {
        pongOpponentScore = pongOpponentScore + 1;
        gbPlayCancel();
        pongBallX = LCDWIDTH - pongBallSize - pongOpponentW - 1;
        pongBallVx = gbAbsInt( pongBallVx );
        pongBallY = arand( LCDHEIGHT - pongBallSize );
    }
    // collision with the right side
    if( ( pongBallX + pongBallSize ) > LCDWIDTH )
    {
        pongPlayerScore = pongPlayerScore + 1;
        gbPlayOK();
        pongBallX = LCDWIDTH - pongBallSize - pongOpponentW - 16;
        pongBallVx = gbAbsInt( pongBallVx );
        pongBallY = arand( LCDHEIGHT - pongBallSize );
    }
    // reset score when 10 is reached
    if( ( pongPlayerScore == 10 ) || ( pongOpponentScore == 10 ) )
    {
        pongPlayerScore = 0;
        pongOpponentScore = 0;
    }

    // move the opponent
    if( ( pongOpponentY + ( pongOpponentH / 2 ) ) < ( pongBallY + ( pongBallSize / 2 ) ) )
    {
        pongOpponentY = pongOpponentY + pongOpponentVy;
        pongOpponentY = gbMin( LCDHEIGHT - pongOpponentH, pongOpponentY );
    }
    else
    {
        pongOpponentY = pongOpponentY - pongOpponentVy;
        pongOpponentY = gbMax( 0, pongOpponentY );
    }

    // draw the score
    gbFontSize = 2;
    gbCursorX = 15;
    gbCursorY = 16;
    gbPrintNumber( pongPlayerScore );

    gbCursorX = 57;
    gbCursorY = 16;
    gbPrintNumber( pongOpponentScore );

    // draw the ball / player / opponent
    gbSetColor( 1 );
    gbFillRect( pongBallX, pongBallY, pongBallSize, pongBallSize );
    gbFillRect( pongPlayerX, pongPlayerY, pongPlayerW, pongPlayerH );
    gbFillRect( pongOpponentX, pongOpponentY, pongOpponentW, pongOpponentH );
}

void gamePong_init()
{
    gbBegin();
    gbSetFont( gbFont5x7 ); // real upstream's own setup()-time `gb.display.setFont(font5x7)` - stays set for the whole game, title screen included
    pongResetPositions();
    pongBeginTitle();
}

void gamePong_update()
{
    if( !gbUpdate() ) return;

    if( pongState == PONG_STATE_TITLE ) pongUpdateTitle();
    else pongUpdatePlay();

    gbRenderFrame();
}
