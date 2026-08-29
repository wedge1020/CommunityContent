// Walbloks (borkin8r, no license specified) - a stacking game built around a
// single dropper block that bounces endlessly left and right along a rail.
// Button A or B releases it; the block falls until it lands on the floor or
// on top of an already-frozen block. Each landing raises the highest stack
// point, and once the stack reaches the rail the dropper is pushed up a row.
// The game ends when the dropper is forced off the top of the screen. Score
// is simply the number of blocks dropped.
//
// Every real `gb.x.y(...)` call site is mechanically rewritten to a plain
// `gbY(...)` function call (see gamePong.c's own header comment for why -
// this dialect has no classes/methods/operator overloading). Every global
// symbol got a `wbk`-prefixed name (this cartridge has no linker - every
// ported game shares one flat global namespace).
//
// Upstream's own `struct DroppedBlocks { int x[N]; int y[N]; int vy[N]; }`
// is a single struct of three parallel arrays rather than an array of
// structs, so it flattens to three plain arrays here with no change of
// meaning at all.
//
// BLOCKING TITLE SCREEN -> AN EXPLICIT STATE: real `gb.titleScreen(F(
// "Walbloks"))` blocks until Button A, and is re-entered whenever Button C
// is pressed. Hand-rolled here as one more explicit state, matching the
// treatment every other port in this cartridge gives that same call.
//
// REAL UPSTREAM QUIRKS, PRESERVED EXACTLY:
// - Only one block may be in flight at a time: `canDrop()` refuses while any
//   block still has a non-zero velocity, so the dropper cannot be spammed.
// - A block that lands on another is snapped to `y - (y % 4)`, a multiple of
//   FOUR, even though every block is EIGHT pixels tall and the grid is built
//   in eights everywhere else. Stacks therefore half-overlap rather than sit
//   flush, which is a real and very visible part of how the game plays.
// - `score` (initialised to 100) and `count` are both maintained and never
//   read by anything - the on-screen "Score:" readout prints
//   `dropped_block_count` instead. Kept as the dead state upstream ships.
// - The game-over test is `dropper_y > DROPPER_SIZE / 2` (4), not a test
//   against 0, so the run ends while the dropper is still partly on screen.
// - On game over the dropper is parked at `-DROPPER_SIZE` every single tick
//   and still drawn, which is what keeps it off screen behind the
//   "Game Over" text.

#include "../gamebuinoShim.h"

#define WBK_DROPPER_SIZE 8
#define WBK_MAX_NUM_BLOCKS 60 // (LCDHEIGHT / 8) * (LCDWIDTH / 8) = 6 * 10

#define WBK_STATE_TITLE 0
#define WBK_STATE_PLAY  1

int wbkDropperX;
int wbkDropperY;
int wbkDropperVX;
int wbkDropVY;
int wbkDroppedBlockCount;
int wbkScore;
int wbkCount;
int wbkDroppedHeighestPoint;
int wbkState;

int[WBK_MAX_NUM_BLOCKS] wbkBlockX;
int[WBK_MAX_NUM_BLOCKS] wbkBlockY;
int[WBK_MAX_NUM_BLOCKS] wbkBlockVY;

void wbkResetGame()
{
    for( int i = 0; i < WBK_MAX_NUM_BLOCKS; i++ )
    {
        wbkBlockX[ i ] = -1;
        wbkBlockY[ i ] = -1;
        wbkBlockVY[ i ] = 0;
    }

    wbkDropperX = LCDWIDTH / 2;
    wbkDropperY = LCDHEIGHT - ( WBK_DROPPER_SIZE * 2 );
    wbkDroppedBlockCount = 0;
    wbkDroppedHeighestPoint = LCDHEIGHT;
    wbkCount = 0;
    wbkScore = 100;
}

bool wbkCanDrop()
{
    if( wbkDroppedBlockCount + 1 >= WBK_MAX_NUM_BLOCKS )
      return false;

    for( int i = 0; i < wbkDroppedBlockCount; i++ )
      if( wbkBlockVY[ i ] != 0 )
        return false;

    return true;
}

// Stands in for the blocking gb.titleScreen(F("Walbloks")) call.
void wbkUpdateTitle()
{
    gbCursorX = 21;
    gbCursorY = 14;
    gbPrintString( "Walbloks" );

    gbCursorX = 24;
    gbCursorY = 30;
    gbPrintString( "PRESS A" );

    if( gbPressed( BTN_A ) )
    {
        gbPlayOK();
        wbkState = WBK_STATE_PLAY;
    }
}

void wbkUpdatePlay()
{
    if( gbPressed( BTN_C ) )
    {
        wbkState = WBK_STATE_TITLE;
        return;
    }

    // check for gameover
    if( wbkDropperY > WBK_DROPPER_SIZE / 2 )
    {
        wbkCount++;

        //////////////////////////////////////////
        // update dropper
        wbkDropperX = wbkDropperX + wbkDropperVX;

        if( wbkDropperX < 0 || wbkDropperX > LCDWIDTH - WBK_DROPPER_SIZE )
        {
            wbkDropperVX = -wbkDropperVX;
            gbPlayTick();
        }

        //////////////////////////////////////////
        // update droppedblocks
        for( int i = 0; i < wbkDroppedBlockCount; i++ )
        {
            if( wbkBlockVY[ i ] == 0 ) // filter offscreen and frozen blocks
              continue;

            wbkBlockY[ i ] = wbkBlockY[ i ] + wbkDropVY;

            if( wbkBlockY[ i ] >= LCDHEIGHT - WBK_DROPPER_SIZE )
            {
                // stop from falling through bottom
                wbkBlockVY[ i ] = 0;
                wbkBlockY[ i ] = LCDHEIGHT - WBK_DROPPER_SIZE;

                if( wbkDroppedHeighestPoint > LCDHEIGHT - WBK_DROPPER_SIZE )
                  wbkDroppedHeighestPoint = LCDHEIGHT - WBK_DROPPER_SIZE;
            }
            else
            {
                // check for frozen block collision
                for( int j = 0; j < wbkDroppedBlockCount; j++ )
                  if( i != j && wbkBlockVY[ j ] == 0 )
                  {
                      // check horizontal and vertical overlap
                      if( gbCollideRectRect( wbkBlockX[ i ], wbkBlockY[ i ],
                                             WBK_DROPPER_SIZE, WBK_DROPPER_SIZE,
                                             wbkBlockX[ j ], wbkBlockY[ j ],
                                             WBK_DROPPER_SIZE, WBK_DROPPER_SIZE ) )
                      {
                          // The snap really is to a multiple of 4, not 8 -
                          // see the header comment.
                          int adjustedFrozenHeight = wbkBlockY[ i ] - ( wbkBlockY[ i ] % 4 );

                          wbkBlockVY[ i ] = 0;

                          if( wbkDroppedHeighestPoint > adjustedFrozenHeight )
                            wbkDroppedHeighestPoint = adjustedFrozenHeight;

                          wbkBlockY[ i ] = adjustedFrozenHeight;
                      }
                  }
            }
        }

        // move dropper up if there is no space between heighest point and dropper
        if( wbkDropperY + WBK_DROPPER_SIZE >= wbkDroppedHeighestPoint )
          wbkDropperY = wbkDropperY - WBK_DROPPER_SIZE;

        ////////////////////////////////////
        // handle input
        if( ( gbPressed( BTN_A ) || gbPressed( BTN_B ) )
         && wbkCanDrop() && wbkDropperY > WBK_DROPPER_SIZE / 2 )
        {
            wbkBlockX[ wbkDroppedBlockCount ] = wbkDropperX;
            wbkBlockY[ wbkDroppedBlockCount ] = wbkDropperY + WBK_DROPPER_SIZE;
            wbkBlockVY[ wbkDroppedBlockCount ] = 1;
            wbkDroppedBlockCount++;
            gbPlayTick();
        }
    }
    else
    {
        wbkDropperY = -WBK_DROPPER_SIZE;

        gbFontSize = 1;
        gbCursorX = 44;
        gbCursorY = 0;
        gbPrintString( "Game Over" );

        if( gbPressed( BTN_A ) || gbPressed( BTN_B ) )
          wbkResetGame();
    }

    gbFontSize = 1;
    gbCursorX = 0;
    gbCursorY = 0;
    gbPrintString( "Score: " );
    gbPrintNumber( wbkDroppedBlockCount );

    //////////////////////////////////////
    // draw dropper
    gbFillRect( wbkDropperX, wbkDropperY, WBK_DROPPER_SIZE, WBK_DROPPER_SIZE );

    /////////////////////////////////////
    // draw dropped blocks
    for( int i = 0; i < wbkDroppedBlockCount; i++ )
      gbFillRect( wbkBlockX[ i ], wbkBlockY[ i ], WBK_DROPPER_SIZE, WBK_DROPPER_SIZE );
}

void gameWalbloks_init()
{
    gbBegin();
    gbSetFrameRate( 20 );
    gbPickRandomSeed(); // no-op, see gamebuinoShim.h's own header comment

    wbkDropperVX = 1;
    wbkDropVY = 1;
    wbkState = WBK_STATE_TITLE;

    wbkResetGame();
}

void gameWalbloks_update()
{
    if( !gbUpdate() ) return;

    if( wbkState == WBK_STATE_TITLE ) wbkUpdateTitle();
    else                              wbkUpdatePlay();

    gbRenderFrame();
}
