// Copter (by Anny) - annyfm (Aneurin Barker Snook), no license specified. A
// one-button cave flyer: hold Button A or B to climb, release to fall, and
// stay inside a corridor that scrolls in from the right, narrows steadily as
// you go, and periodically pinches down into a much tighter gate. Touch
// either wall and the run freezes with your distance shown as the score;
// Button C restarts.
//
// This is a genuinely different game from the already-shipped `gameCopter.c`
// (Clement83's own Copter, 956 lines) - different author, roughly a third
// the size, and a different corridor generator - hence the separate `copa`
// prefix beside that file's own `copt`. The archive project had this one
// listed as unrecoverable for a long time before it turned up.
//
// Every real `gb.x.y(...)` call site is mechanically rewritten to a plain
// `gbY(...)` function call (see gamePong.c's own header comment for why -
// this dialect has no classes/methods/operator overloading). Every global
// symbol got a `copa`-prefixed name (this cartridge has no linker - every
// ported game shares one flat global namespace). Upstream's own
// `random(a, b)` calls became `a + arand(b - a)` and its own
// `range_random(float* r)` helper became `copaRangeRandom(lo, hi)` taking
// the two bounds directly - the four real two-element bound arrays it was
// called with are compile-time constants upstream, so nothing is lost.
//
// `random(a, b)` with `b <= a` returns `a` on real Arduino, and `arand(0)`
// returns 0 here, so the rewritten form matches that edge case exactly - it
// genuinely occurs, since the gate generator's own upper bound is
// `min(new_h, gate_size_max)` and can drop to the lower bound as the
// corridor narrows.
//
// `CorridorPart`/`Helicopter` are kept as real structs, with upstream's own
// whole-struct assignments (`corridor[i] = corridor[i+1]`, and the two
// `CorridorPart c; ...; corridor[x] = c;` build-and-store sequences) written
// out field by field instead - the same treatment gameTron.c's own header
// comment describes for aggregate assignment in this dialect.
//
// `randomSeed(progress * micros())` IS DROPPED: upstream reseeds the
// generator every single tick from a microsecond timer, which has no
// equivalent here. `arand()` is already a continuously-advancing PRNG, so
// the corridor still varies run to run; what is lost is only the
// wall-clock-derived entropy, exactly as every other port in this cartridge
// drops `randomSeed()`.
//
// BLOCKING TITLE SCREEN -> AN EXPLICIT STATE: real `gb.titleScreen(F("Copter
// by Anny"))` blocks until Button A, and is re-entered on every Button C
// press. Hand-rolled here as one more explicit state.
//
// REAL UPSTREAM QUIRKS, PRESERVED EXACTLY:
// - `did_crash()` is called twice per tick - once by the main loop to decide
//   whether to keep updating, and again by `draw_score()` to decide which
//   font to print the score in. Both calls walk all 21 corridor slices. Left
//   as written; the score font really is the crash indicator.
// - Once crashed, the corridor and helicopter simply stop updating while
//   both keep being drawn, so the game freezes mid-flight rather than
//   showing any game-over screen of its own.
// - The `debug` flag (which makes `did_crash()` always return false) ships
//   set to false and is kept as the inert switch upstream leaves it.
// - `yv_change_next += game_speed * range_random(...) * game_speed` squares
//   the speed factor where the other three rate calculations multiply by it
//   once, so vertical-drift changes come far less often than the surrounding
//   code suggests.

#include "../gamebuinoShim.h"

#define COPA_STATE_TITLE 0
#define COPA_STATE_PLAY  1

// --- config (upstream's own values) ---
#define COPA_GAME_SPEED 4
#define COPA_SIZE_MAX 44.0
#define COPA_SIZE_MIN 12.0
#define COPA_REDUCE_NEXT 30
#define COPA_REDUCE_MULT 0.9
#define COPA_YV_GRANULARITY 100.0
#define COPA_YV_CHANGE_RANGE 2.0
#define COPA_GATE_SIZE_MAX 20
#define COPA_GATE_SIZE_MIN 9
#define COPA_GATE_FIRST 60

#define COPA_MAX_PARTS 21 // LCDWIDTH / COPA_GAME_SPEED

// --- helicopter config ---
#define COPA_RISE_RATE 0.3
#define COPA_RISE_MAX 2.0
#define COPA_FALL_RATE 0.4
#define COPA_FALL_MAX -2.5

struct CopaCorridorPart
{
    float h;
    float y;
};

CopaCorridorPart[COPA_MAX_PARTS] copaCorridor;

int copaProgress;
float copaLastH;
float copaLastY;
float copaYv;
int copaYvChangeNext;
int copaGateNext;
int copaGateSize;
int copaGateLength;
int copaMaxParts;
int copaLastPart;
int copaState;

// Helicopter
int copaHeliSize;
int copaHeliX;
float copaHeliY;
float copaHeliYv;

// Upstream's own range_random(float* r) -> random(r[0], r[1]), with the two
// bounds passed directly.
float copaRangeRandom( float lo, float hi )
{
    int a = (int)lo;
    int b = (int)hi;

    if( b <= a )
      return (float)a;

    return (float)( a + arand( b - a ) );
}

void copaInitCorridor()
{
    copaMaxParts = LCDWIDTH / COPA_GAME_SPEED;
    copaLastPart = copaMaxParts - 1;

    // reset runtime vars
    copaProgress = 0;
    copaLastH = COPA_SIZE_MAX;
    copaLastY = LCDHEIGHT / 2;
    copaYv = 0;
    copaYvChangeNext = COPA_GAME_SPEED * COPA_REDUCE_NEXT;
    copaGateNext = COPA_GATE_FIRST;
    copaGateSize = 0;
    copaGateLength = 0;

    // reset corridor parts
    for( int i = 0; i < copaMaxParts; i++ )
    {
        copaCorridor[ i ].h = copaLastH;
        copaCorridor[ i ].y = copaLastY;
    }
}

void copaInitHeli()
{
    copaHeliSize = 3;
    copaHeliX = 4;
    copaHeliY = LCDHEIGHT / 2;
    copaHeliYv = 0;
}

void copaUpdateCorridor()
{
    float newH;
    float newY;
    float yTop, yBot;

    copaProgress++;

    // upstream reseeds from micros() here - see the header comment

    for( int i = 0; i < copaLastPart; i++ )
    {
        copaCorridor[ i ].h = copaCorridor[ i + 1 ].h;
        copaCorridor[ i ].y = copaCorridor[ i + 1 ].y;
    }

    newH = copaLastH;
    newY = copaLastY;

    // narrow gradually (ignore gates)
    if( copaProgress % ( COPA_GAME_SPEED * COPA_REDUCE_NEXT ) == 0 && newH > COPA_SIZE_MIN )
    {
        newH = newH * COPA_REDUCE_MULT;

        if( newH < COPA_SIZE_MIN )
          newH = COPA_SIZE_MIN;

        copaLastH = newH;
    }

    if( copaProgress >= copaGateNext )
    {
        // generate a gate
        float thisGateSizeMax = COPA_GATE_SIZE_MAX;

        copaGateNext = copaGateNext + (int)( COPA_GAME_SPEED * copaRangeRandom( 5, 15 ) );
        copaGateLength = (int)copaRangeRandom( 2, 5 );

        if( newH < thisGateSizeMax )
          thisGateSizeMax = newH;

        newH = copaRangeRandom( COPA_GATE_SIZE_MIN, thisGateSizeMax );

        yTop = copaLastY - ( copaLastH / 2 );
        yBot = copaLastY + ( copaLastH / 2 );
        newY = copaRangeRandom( yTop + ( newH / 2 ), yBot - ( newH / 2 ) );
    }
    else if( copaGateLength > 0 )
    {
        // gate remains fixed
        newH = copaCorridor[ copaLastPart ].h;
        newY = copaCorridor[ copaLastPart ].y;
        copaGateLength--;
    }
    else
    {
        // no gate, move corridor as normal
        if( copaProgress >= copaYvChangeNext )
        {
            // The squared game-speed factor here is upstream's own - see the
            // header comment.
            copaYvChangeNext = copaYvChangeNext
                             + (int)( COPA_GAME_SPEED * copaRangeRandom( 4, 12 ) * COPA_GAME_SPEED );
            copaYv = ( ( ( COPA_YV_GRANULARITY / 2 ) - arand( (int)COPA_YV_GRANULARITY ) )
                     / COPA_YV_GRANULARITY ) * COPA_YV_CHANGE_RANGE;
        }

        newY = newY + copaYv;

        yTop = newY - ( newH / 2 );
        yBot = newY + ( newH / 2 );

        if( yTop < 0 )
        {
            newY = newY - yTop;
            copaYv = -copaYv;
        }

        if( yBot > LCDHEIGHT )
        {
            newY = newY - ( yBot - LCDHEIGHT );
            copaYv = -copaYv;
        }

        copaLastY = newY;
    }

    copaCorridor[ copaLastPart ].h = newH;
    copaCorridor[ copaLastPart ].y = newY;
}

void copaDrawCorridor()
{
    for( int i = 0; i < copaMaxParts; i++ )
    {
        int x = i * COPA_GAME_SPEED;
        int yTop = (int)( copaCorridor[ i ].y - ( copaCorridor[ i ].h / 2 ) );
        int yBot = (int)( copaCorridor[ i ].y + ( copaCorridor[ i ].h / 2 ) );

        gbFillRect( x, 0, COPA_GAME_SPEED, yTop );
        gbFillRect( x, yBot, COPA_GAME_SPEED, LCDHEIGHT - yBot );
    }
}

bool copaDidCrash()
{
    // don't really need to collision test beyond heli.x, room for optimisation
    for( int i = 0; i < copaMaxParts; i++ )
    {
        int x = i * COPA_GAME_SPEED;
        int yTop = (int)( copaCorridor[ i ].y - ( copaCorridor[ i ].h / 2 ) );
        int yBot = (int)( copaCorridor[ i ].y + ( copaCorridor[ i ].h / 2 ) );

        if( gbCollideRectRect( copaHeliX, (int)copaHeliY, copaHeliSize, copaHeliSize,
                               x, 0, COPA_GAME_SPEED, yTop )
         || gbCollideRectRect( copaHeliX, (int)copaHeliY, copaHeliSize, copaHeliSize,
                               x, yBot, COPA_GAME_SPEED, LCDHEIGHT - yBot ) )
          return true;
    }

    return false;
}

void copaUpdateHeli()
{
    if( gbRepeat( BTN_A, 1 ) || gbRepeat( BTN_B, 1 ) )
    {
        copaHeliYv = copaHeliYv + COPA_RISE_RATE;

        if( copaHeliYv > COPA_RISE_MAX )
          copaHeliYv = COPA_RISE_MAX;
    }
    else
    {
        copaHeliYv = copaHeliYv - COPA_FALL_RATE;

        if( copaHeliYv < COPA_FALL_MAX )
          copaHeliYv = COPA_FALL_MAX;
    }

    copaHeliY = copaHeliY - copaHeliYv;

    if( ( copaHeliY + copaHeliSize ) > LCDHEIGHT )
      copaHeliY = LCDHEIGHT - copaHeliSize;
    else if( copaHeliY < 0 )
      copaHeliY = 0;
}

void copaDrawHeli()
{
    gbFillRect( copaHeliX, (int)copaHeliY, copaHeliSize, copaHeliSize );
}

void copaDrawScore()
{
    // The score's own font really is the crash indicator - see the header.
    if( copaDidCrash() )
      gbSetFont( gbFont5x7 );
    else
      gbSetFont( gbFont3x5 );

    gbPrintNumber( copaProgress );
}

// Stands in for the blocking gb.titleScreen(F("Copter by Anny")) call.
void copaUpdateTitle()
{
    gbSetFont( gbFont5x7 );
    gbCursorX = 8;
    gbCursorY = 12;
    gbPrintString( "Copter" );
    gbCursorX = 14;
    gbCursorY = 22;
    gbPrintString( "by Anny" );

    gbSetFont( gbFont3x5 );
    gbCursorX = 24;
    gbCursorY = 40;
    gbPrintString( "PRESS A" );

    if( gbPressed( BTN_A ) )
    {
        gbPlayOK();
        copaState = COPA_STATE_PLAY;
    }
}

void gameCopterAnny_init()
{
    gbBegin();
    gbSetFrameRate( 20 );
    gbSetFont( gbFont5x7 );
    gbPickRandomSeed(); // no-op, see gamebuinoShim.h's own header comment

    copaInitCorridor();
    copaInitHeli();
    copaState = COPA_STATE_TITLE;
}

void gameCopterAnny_update()
{
    if( !gbUpdate() ) return;

    if( copaState == COPA_STATE_TITLE )
    {
        copaUpdateTitle();
        gbRenderFrame();
        return;
    }

    if( gbPressed( BTN_C ) )
    {
        copaInitCorridor();
        copaInitHeli();
        copaState = COPA_STATE_TITLE;
        gbRenderFrame();
        return;
    }

    if( !copaDidCrash() )
    {
        copaUpdateCorridor();
        copaUpdateHeli();
    }

    copaDrawCorridor();
    copaDrawHeli();
    copaDrawScore();

    gbRenderFrame();
}
