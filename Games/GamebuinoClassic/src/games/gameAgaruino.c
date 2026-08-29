// Agaruino (ogbaba, GPLv3 - github.com/ogbaba/Agaruino). A real agar.io
// clone for Gamebuino Classic: steer a blob around a 100x100 world eating
// smaller blobs to grow, avoiding bigger ones that can eat you right back.
// The second game ported for this project (after Pong Solo), picked
// specifically as the smallest/cleanest real candidate staged in
// `more games/` (183 lines, 1 file) - see `more games/DISCOVERED_GAMES.md`'s
// own "Porting priority audit" for why.
//
// Every real `gb.x.y(...)` call site was mechanically rewritten to a plain
// `gbY(...)` function call (see gamePong.c's own header comment for why -
// this dialect has no classes/methods). `random(N)` became `arand(N)`
// (this dialect's own established RNG helper). Upstream's own `byte
// est_ia, est_nourr;` fields became plain `bool` here instead - a genuine
// primitive type in this dialect (unlike real C++, which is why upstream
// reached for Arduino's own byte-as-boolean idiom in the first place), and
// a more accurate match for what are really just true/false flags.
//
// Upstream's own global names were all in French (`joueur(s)` = player(s),
// `boule` = ball/blob, `taille` = size, `manger`/`repas` = eat/meals,
// `gagner`/`gagne` = win/won, `afficher` = display, `deplacements` =
// movements) - kept as the basis for this port's own identifiers (English
// translations added as comments instead of renaming outright) but every
// global symbol was given an `agar`-prefixed name, since Vircon32 has no
// linker and every game in this single compiled cartridge shares one flat
// global namespace - generic names like upstream's own bare `menu()`/
// `jouer()`/`afficher()` would risk colliding with a future game's own
// identically-named globals.
//
// Two real upstream quirks found while reading the source closely, handled
// differently on purpose:
// - `accelerer()`'s own vy-negative-clamp line read `max(joueurs->vy + vy,
//   -vmax)` - `joueurs` (the whole global array, decaying to a pointer to
//   element 0) instead of the intended `joueur` parameter, a plain typo
//   that only ever mattered for AI-controlled balls (element 0 is always
//   the human player here, so the bug was a no-op whenever `accelerer()`
//   ran for the player itself). Normalized to the obviously-intended
//   `agarAccelerate()` below (uses the passed-in ball for both axes) rather
//   than faithfully reproducing a one-character typo that would otherwise
//   just look like a fresh mistake in this port.
// - `afficher()`'s own on-screen-culling test used `||` where an `&&` (or
//   the negation of "fully outside") would normally be expected - the
//   practical effect is that a ball positioned exactly at the camera's own
//   focus point (i.e. the player's own ball, always drawn relative to
//   itself) satisfies none of the four "is outside the viewport" checks
//   and is therefore never actually drawn. This one **is** preserved
//   exactly as upstream wrote it below (`agarDraw()`): the player's own
//   blob is genuinely invisible on real hardware too, so keeping it
//   matches actual original gameplay rather than "fixing" something that
//   was never broken from a porting standpoint.
//
// A REAL FLOAT-DISPLAY BUG, FOUND AND FIXED (not preserved): upstream's own
// `size` field is a genuine `float`, and `afficher()` calls real
// `gb.display.println(joueurs[num_joueur].taille);` directly on it - real
// Arduino `Print::println(float)` defaults to 2 decimal places, so real
// hardware shows a genuine fractional readout like "Taille : 3.75", not a
// whole number. This shim's own `gbPrintNumber()` only ever accepted a
// plain `int` (no float-printing primitive existed at all at the time this
// game was first ported), so the original port cast the value to `int`
// before printing, silently truncating away the entire fractional part
// every tick. Found via a direct side-by-side comparison against a real
// emulator screenshot. Fixed by calling the shared `gbPrintFloat()`
// primitive (`gamebuinoShim.h`/`.c`, a direct port of real Arduino's own
// `Print::printFloat()` algorithm) - promoted there rather than kept as a
// local helper once `gameMotoCross.c` was found to have already
// independently documented hitting the identical "no float-print
// primitive exists" wall for its own real upstream `print(player1.vx)`
// debug readout (see that file's own header comment, since fixed the same
// way). The on-screen label text was also corrected from this port's own
// "TAILLE " to upstream's own real, literal "Taille : " while this was
// being fixed.

struct AgarBall
{
    bool isAI;
    bool isFood;   // controls respawn size after being eaten: 1 -> small, 0 -> medium
    float size;
    float vx, vy;
    float x, y;
};

#define AGAR_MAP_W 100
#define AGAR_MAP_H 100
#define AGAR_MAX_SPEED 1.2
#define AGAR_PLAYER_COUNT 16

// Solo play: the human is always element 0 of agarBalls[].
int agarPlayerIndex = 0;
AgarBall[AGAR_PLAYER_COUNT] agarBalls;

enum AgarState
{
    AGAR_STATE_MENU = 0,
    AGAR_STATE_PLAY = 1
};

int agarState;
bool agarWon;

void agarBeginMenu()
{
    agarState = AGAR_STATE_MENU;
}

void agarBeginPlay()
{
    // Scatter every ball across the map. Every slot but the player's own
    // becomes, 50/50, either a small stationary-ish "food" blob or a
    // medium AI-controlled hunter; the player always starts medium-sized.
    int i;
    for( i = 0; i < AGAR_PLAYER_COUNT; i++ )
    {
        agarBalls[ i ].x = arand( AGAR_MAP_W );
        agarBalls[ i ].y = arand( AGAR_MAP_H );
        agarBalls[ i ].vx = 0;
        agarBalls[ i ].vy = 0;

        if( i != agarPlayerIndex )
        {
            if( arand( 2 ) )
            {
                agarBalls[ i ].isFood = true;
                agarBalls[ i ].isAI = false;
                agarBalls[ i ].size = 1;
            }
            else
            {
                agarBalls[ i ].isAI = true;
                agarBalls[ i ].isFood = false;
                agarBalls[ i ].size = 2;
            }
        }
        else
        {
            agarBalls[ agarPlayerIndex ].isAI = false;
            agarBalls[ agarPlayerIndex ].isFood = false;
            agarBalls[ agarPlayerIndex ].size = 2;
        }
    }

    agarState = AGAR_STATE_PLAY;
    agarWon = false;
}

void agarUpdateMenu()
{
    gbCursorX = 2;
    gbCursorY = 2;
    gbPrintString( "AGARUINO !" );

    gbCursorX = 2;
    gbCursorY = 14;
    gbPrintString( "A LANCER" );

    if( agarWon )
    {
        gbCursorX = 2;
        gbCursorY = 26;
        gbPrintString( "GAGNE !" );
    }

    if( gbPressed( BTN_A ) )
      agarBeginPlay();
}

void agarDraw()
{
    gbCursorX = 0;
    gbCursorY = 0;
    gbPrintString( "Taille : " );
    gbPrintFloat( agarBalls[ agarPlayerIndex ].size, 2 );

    int i;
    for( i = 0; i < AGAR_PLAYER_COUNT; i++ )
    {
        // Preserved exactly as upstream wrote it (see this file's own
        // header comment) - this is an "is at least one edge outside the
        // viewport" test, not "is on screen", so the player's own ball
        // (always dead-center relative to itself) never satisfies any of
        // these four conditions and is therefore never actually drawn,
        // matching real hardware's own original behavior.
        if( ( agarBalls[ i ].x - agarBalls[ i ].size < agarBalls[ agarPlayerIndex ].x - LCDWIDTH / 2 )
            || ( agarBalls[ i ].x + agarBalls[ i ].size > agarBalls[ agarPlayerIndex ].x + LCDWIDTH / 2 )
            || ( agarBalls[ i ].y - agarBalls[ i ].size < agarBalls[ agarPlayerIndex ].y - LCDHEIGHT / 2 )
            || ( agarBalls[ i ].y + agarBalls[ i ].size < agarBalls[ agarPlayerIndex ].y + LCDHEIGHT / 2 ) )
        {
            gbFillCircle
            (
                (int)( agarBalls[ i ].x - agarBalls[ agarPlayerIndex ].x + LCDWIDTH / 2 ),
                (int)( agarBalls[ i ].y - agarBalls[ agarPlayerIndex ].y + LCDHEIGHT / 2 ),
                (int)agarBalls[ i ].size
            );
        }
    }
}

// Adds (ax, ay) to a ball's own velocity, then clamps both axes to a
// per-ball max speed that shrinks as the ball grows (a bigger blob moves
// sluggishly) - normalized from upstream's own min()/max() calls (see
// this file's own header comment for the one-character typo found there).
void agarAccelerate( AgarBall* ball, float ax, float ay )
{
    float vmax = AGAR_MAX_SPEED - ball->size / 20.0;

    ball->vx = ball->vx + ax;
    if( ball->vx > vmax ) ball->vx = vmax;
    if( ball->vx < -vmax ) ball->vx = -vmax;

    ball->vy = ball->vy + ay;
    if( ball->vy > vmax ) ball->vy = vmax;
    if( ball->vy < -vmax ) ball->vy = -vmax;
}

void agarHandleInput()
{
    if( gbRepeat( BTN_DOWN, 10 ) )
      agarAccelerate( &agarBalls[ agarPlayerIndex ], 0, 0.5 );
    if( gbRepeat( BTN_UP, 10 ) )
      agarAccelerate( &agarBalls[ agarPlayerIndex ], 0, -0.5 );
    if( gbRepeat( BTN_RIGHT, 10 ) )
      agarAccelerate( &agarBalls[ agarPlayerIndex ], 0.5, 0 );
    if( gbRepeat( BTN_LEFT, 10 ) )
      agarAccelerate( &agarBalls[ agarPlayerIndex ], -0.5, 0 );

    if( gbPressed( BTN_C ) )
      agarBeginMenu();
}

// predator eats prey if (and only if) it's genuinely bigger - prey
// respawns at a random spot, at its own "resting" size (small for a food
// blob, medium for a hunter, matching agarBeginPlay()'s own initial sizes)
void agarEat( AgarBall* predator, AgarBall* prey )
{
    if( predator->size > prey->size )
    {
        predator->size = predator->size + prey->size / 4;

        prey->x = arand( AGAR_MAP_W );
        prey->y = arand( AGAR_MAP_H );
        if( prey->isFood )
          prey->size = 1;
        else
          prey->size = 2;

        gbPlayOK();
    }
}

void agarHandleEating()
{
    int i, j;
    for( i = 0; i < AGAR_PLAYER_COUNT; i++ )
    {
        for( j = 0; j < AGAR_PLAYER_COUNT; j++ )
        {
            if( i == j ) continue;

            if( gbCollidePointRect
                (
                    (int)agarBalls[ j ].x, (int)agarBalls[ j ].y,
                    (int)( agarBalls[ i ].x - agarBalls[ i ].size ),
                    (int)( agarBalls[ i ].y - agarBalls[ i ].size ),
                    (int)( agarBalls[ i ].size * 2 ), (int)( agarBalls[ i ].size * 2 )
                )
                && ( agarBalls[ i ].size > agarBalls[ j ].size ) )
              agarEat( &agarBalls[ i ], &agarBalls[ j ] );
        }
    }
}

void agarMoveAI( AgarBall* ball )
{
    if( arand( 10 ) == 0 )
      agarAccelerate( ball, (float)arand( 2 ) - 0.5, (float)arand( 2 ) - 0.5 );
}

void agarHandleMovement()
{
    int i;
    for( i = 0; i < AGAR_PLAYER_COUNT; i++ )
    {
        if( agarBalls[ i ].isAI )
          agarMoveAI( &agarBalls[ i ] );

        agarBalls[ i ].x = agarBalls[ i ].x + agarBalls[ i ].vx;
        agarBalls[ i ].y = agarBalls[ i ].y + agarBalls[ i ].vy;

        if( agarBalls[ i ].x > AGAR_MAP_W ) agarBalls[ i ].x = AGAR_MAP_W;
        if( agarBalls[ i ].x < 0 ) agarBalls[ i ].x = 0;
        if( agarBalls[ i ].y > AGAR_MAP_H ) agarBalls[ i ].y = AGAR_MAP_H;
        if( agarBalls[ i ].y < 0 ) agarBalls[ i ].y = 0;
    }
}

// Anyone reaching this size ends the round - not necessarily the player:
// an AI hunter can win it too, matching upstream's own generic "someone
// won" message (see agarUpdateMenu()'s own "GAGNE !" line) rather than
// tracking which specific ball crossed the threshold.
void agarCheckWin()
{
    int i;
    for( i = 0; i < AGAR_PLAYER_COUNT; i++ )
    {
        if( agarBalls[ i ].size > 20 )
        {
            agarBeginMenu();
            agarWon = true;
        }
    }
}

void agarUpdatePlay()
{
    agarHandleInput();
    agarHandleMovement();
    agarHandleEating();
    agarCheckWin();
    agarDraw();
}

void gameAgaruino_init()
{
    gbBegin();
    agarBeginMenu();
    agarWon = false;
}

void gameAgaruino_update()
{
    if( !gbUpdate() ) return;

    if( agarState == AGAR_STATE_MENU ) agarUpdateMenu();
    else agarUpdatePlay();

    gbRenderFrame();
}
