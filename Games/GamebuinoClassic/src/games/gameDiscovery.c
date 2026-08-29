// Discovery (FirstKlaas, no license specified in the default branch - an
// unmerged branch of the repo adds an LGPLv3 LICENSE file that was never
// merged, noted here rather than resolved either way). A Star-Trek-themed
// side-on shooter written by the author with and for his own children, which
// is also why every comment in the real source is in German.
//
// The USS Discovery holds the left edge of the screen, moving only up and
// down, while Klingon ships stream in from the right at randomised speeds.
// Button A fires a missile (15 points a hit); Button B raises the deflector
// shield, which both blocks firing and turns a collision from a fatal hit
// into a harmless one that simply respawns the Klingon. Every 100 points is
// a level, and each level nudges the Discovery one pixel further right (up
// to a cap), shortening the reaction distance.
//
// Every real `gb.x.y(...)` call site is mechanically rewritten to a plain
// `gbY(...)` function call (see gamePong.c's own header comment for why -
// this dialect has no classes/methods/operator overloading). Every global
// symbol got a `disc`-prefixed name (this cartridge has no linker - every
// ported game shares one flat global namespace). Upstream's own
// `random(a,b)` calls became `a + arand(b-a)`, this project's own
// established ranged-random rewrite convention (see gameFlappyBirdo.c).
//
// Upstream's own `Game`/`Discovery`/`KlingonShip`/`Missile` structs are kept
// as real structs here (this dialect supports them - see gameBomber.c's own
// already-proven struct-array-plus-pointer pattern), with only the field
// names carried over verbatim. All four sprite tables were converted from
// upstream's own `B`-binary PROGMEM tables to hex byte-for-byte by script,
// not hand-transcribed - their two-byte width/height headers are written as
// `#define`d macros upstream, resolved to their real numeric values during
// that conversion.
//
// A REAL OUT-OF-BOUNDS WRITE, FIXED BECAUSE THIS PLATFORM CANNOT ABSORB IT:
// upstream's own `findNextMissile()` is declared `byte` (unsigned) but
// returns `-1` when every missile slot is busy, which converts to 255. Its
// only caller then tests `if (index < 0) return;` - a comparison that is
// always false for an unsigned type, so the guard never fires and
// `missiles[255]` gets written. Real AVR absorbs that as a stray write into
// adjacent SRAM; here it would land in an unrelated global, so
// `discLaunchMissile()` tests `index >= DISC_NUM_MISSILES` instead. Reaching
// it needs ten shots in flight at once (about 32 ticks of travel each), which
// is tight but genuinely possible with fast enough tapping - a real bug, not
// a theoretical one.
//
// REAL UPSTREAM QUIRKS, PRESERVED EXACTLY:
// - The downward movement clamp is `LCDHEIGHT - discovery.w`, using the
//   ship's own `w` field (8) where `h` (14) reads like the intended one, so
//   the Discovery can descend slightly further than its own sprite height
//   would suggest.
// - Three different hitboxes describe the same Klingon ship: the
//   player-collision test passes its HEIGHT as the width and its WIDTH as the
//   height (15x8 - correct for a sprite drawn rotated, which it is), while
//   the missile-collision test hardcodes a third size again (14x8).
// - `checkForCollisions()` respawns a shielded collision's Klingon without
//   awarding the five points its own German comment describes; the comment
//   documents an intent the code never implements.
// - Losing the last life sets `is_running = false`, dropping straight back to
//   the title screen with no game-over screen and no score summary of any
//   kind. There is no high score, in upstream or here.
//
// STARTING THE GAME TAKES BUTTON C, NOT BUTTON A: real `loop()` re-enters the
// blocking `gb.titleScreen()` call on every tick while the game is not
// running, and `titleScreen()` itself returns only on Button A - at which
// point `loop()` immediately returns and the next tick simply re-enters it
// and redraws the same screen. The only thing that ever actually starts a
// game is the Button C test at the top of `loop()`, which runs before that
// re-entry. So on real hardware Button A visibly does nothing on the title
// screen and Button C starts play. That is reproduced exactly here; the
// hand-rolled title screen just states it, since a blocking call that always
// redraws the same pixels is indistinguishable from a state that draws them.
//
// DISPLAY PERSISTENCE, DELIBERATELY NOT EMULATED: upstream sets
// `display.persistence = true` for the title screen and `false` for
// gameplay. Gameplay - the only part that draws anything of its own - is
// therefore already running with the same every-tick clear this shim does
// unconditionally, so nothing about the played game depends on it.

#include "../gamebuinoShim.h"

#define DISC_NUM_KLINGONSHIPS 6
#define DISC_NUM_MISSILES 10
#define DISC_SPRITE_DISCOVERY_HEIGHT 8
#define DISC_SPRITE_DISCOVERY_WIDTH  16
#define DISC_SPRITE_KLINGONSHIP_HEIGHT 15
#define DISC_SPRITE_KLINGONSHIP_WIDTH  8

#define DISC_LEVEL_JUMP_DX 1
#define DISC_DAMAGE_COLLISION 50
#define DISC_NUMBER_OF_EXPLOSION_FRAMES 25

// ---------------------------------------------------------------------------
// Upstream's own structs, field names unchanged
// ---------------------------------------------------------------------------

struct DiscGame
{
    int number_of_lives;
    int max_number_of_lives;
    bool is_running;
};

struct DiscKlingonShip
{
    int x, y;
    int exploding, vx, vy;
};

struct DiscShip
{
    int x, y, x0;
    int exploding, shielded, damage, vx, vy, h, w, shield_dx, shield_dy, lvl;
    int score;
};

struct DiscMissile
{
    int x, y;
    int w, h, isActive, vx;
};

DiscGame discGame;
DiscShip discovery;
DiscKlingonShip[DISC_NUM_KLINGONSHIPS] discKlingonShips;
DiscMissile[DISC_NUM_MISSILES] discMissiles;

// Real upstream's own sprite tables (a_sprites.ino), converted from its
// own B-binary PROGMEM tables to hex byte-for-byte by script. The two-byte
// width/height header of each is written as #define'd macros upstream and
// was resolved to its real numeric value during that conversion.

// klingonship: 8x15
int[17] discKlingonship =
{
    0x08, 0x0F, 0x38, 0x54, 0x38, 0x38, 0x10, 0x38,
    0x6C, 0x44, 0x6C, 0x7C, 0x7C, 0x92, 0x92, 0x92,
    0x92
};

// SPRITE_DISCOVERY: 16x8
int[18] discSpriteDiscovery =
{
    0x10, 0x08, 0x60, 0x20, 0x13, 0x54, 0x28, 0x5A,
    0x77, 0xCA, 0x77, 0xCA, 0x28, 0x5A, 0x13, 0x54,
    0x60, 0x20
};

// SPRITE_LIVE: 5x4
int[6] discSpriteLive =
{
    0x05, 0x04, 0x50, 0xF8, 0x70, 0x20
};

// SPRITE_DISCOVERY_SHIELD: 8x12
int[14] discSpriteDiscoveryShield =
{
    0x08, 0x0C, 0x60, 0x18, 0x04, 0x02, 0x01, 0x01,
    0x01, 0x01, 0x02, 0x04, 0x18, 0x60
};

// ---------------------------------------------------------------------------
// Klingon ships - direct port of upstream's own b_klingon.ino
// ---------------------------------------------------------------------------

void discDrawKlingonShips()
{
    for( int i = 0; i < DISC_NUM_KLINGONSHIPS; i++ )
      gbDrawBitmapRotated( discKlingonShips[ i ].x, discKlingonShips[ i ].y,
                           discKlingonship, 1, 0 ); // ROTCCW, NOFLIP
}

// Materialises a new Klingon ship in the neutral zone (off the right edge).
void discSpawnKlingonShip( int i )
{
    discKlingonShips[ i ].x = LCDWIDTH + arand( 127 - LCDWIDTH );
    discKlingonShips[ i ].y = arand( LCDHEIGHT - DISC_SPRITE_KLINGONSHIP_WIDTH );
    discKlingonShips[ i ].vx = 1 + arand( 2 );
    discKlingonShips[ i ].vy = 0;
    discKlingonShips[ i ].exploding = false;
}

void discInitKlingonShips()
{
    for( int i = 0; i < DISC_NUM_KLINGONSHIPS; i++ )
      discSpawnKlingonShip( i );
}

void discUpdateKlingonShip( int i )
{
    discKlingonShips[ i ].x = discKlingonShips[ i ].x - discKlingonShips[ i ].vx;
    discKlingonShips[ i ].y = discKlingonShips[ i ].y - discKlingonShips[ i ].vy;

    if( discKlingonShips[ i ].x < -16 )
      discSpawnKlingonShip( i );
}

void discUpdateKlingonShips()
{
    for( int i = 0; i < DISC_NUM_KLINGONSHIPS; i++ )
      discUpdateKlingonShip( i );
}

// ---------------------------------------------------------------------------
// The Discovery - direct port of upstream's own c_discovery.ino
// ---------------------------------------------------------------------------

int discGetShieldX() { return discovery.x + discovery.shield_dx; }
int discGetShieldY() { return discovery.y + discovery.shield_dy; }

void discDrawLives()
{
    for( int i = 0; i < discGame.number_of_lives; i++ )
      gbDrawBitmap( 50 + ( i * 6 ), LCDHEIGHT - 5, discSpriteLive );
}

void discDrawDiscovery()
{
    if( discovery.damage == DISC_NUMBER_OF_EXPLOSION_FRAMES )
      discovery.exploding = false;

    if( discovery.exploding )
    {
        discovery.damage++;

        for( int i = 0; i < 15; i++ )
          if( ( discovery.damage - ( 4 * i ) ) > 1 )
            gbDrawCircle( discovery.x + ( DISC_SPRITE_DISCOVERY_WIDTH / 2 ),
                          discovery.y + ( DISC_SPRITE_DISCOVERY_HEIGHT / 2 ),
                          discovery.damage - ( 4 * i ) );
    }
    else
    {
        gbDrawBitmapRotated( discovery.x, discovery.y, discSpriteDiscovery, 0, 0 ); // NOROT, NOFLIP

        if( discovery.shielded )
          gbDrawBitmapRotated( discGetShieldX(), discGetShieldY(),
                               discSpriteDiscoveryShield, 0, 0 ); // NOROT, NOFLIP
    }
}

// Upstream passes the Klingon's HEIGHT as the rect width and its WIDTH as the
// height - correct for a sprite drawn rotated, and left exactly as written.
bool discCheckForCollisionWithKlingonShip( int i )
{
    if( discovery.exploding )
      return false;

    return gbCollideRectRect( discovery.x, discovery.y, 16, 8,
                              discKlingonShips[ i ].x, discKlingonShips[ i ].y,
                              DISC_SPRITE_KLINGONSHIP_HEIGHT, DISC_SPRITE_KLINGONSHIP_WIDTH );
}

void discAddPoints( int points )
{
    discovery.score = discovery.score + points;
    discovery.lvl = discovery.score / 100;

    discovery.x = discovery.x0 + ( discovery.lvl * DISC_LEVEL_JUMP_DX );

    if( discovery.x > 10 * DISC_LEVEL_JUMP_DX )
      discovery.x = 10 * DISC_LEVEL_JUMP_DX;
}

void discRemovePoints( int points )
{
    if( discovery.score < points )
      discAddPoints( -1 * discovery.score );
    else
      discAddPoints( -1 * points );
}

void discShootDiscovery()
{
    if( discovery.exploding )
      return;

    discovery.exploding = true;
    discovery.damage = 0;
    discRemovePoints( DISC_DAMAGE_COLLISION );

    if( discGame.number_of_lives > 1 )
      discGame.number_of_lives--;
    else
      discGame.is_running = false;
}

// Checks the Discovery against every Klingon ship. With shields up the
// Klingon is simply respawned in the neutral zone; with shields down the
// Discovery is destroyed and loses points.
void discCheckForCollisions()
{
    // No need to test for collisions while already exploding.
    if( discovery.exploding )
      return;

    for( int i = 0; i < DISC_NUM_KLINGONSHIPS; i++ )
    {
        if( discCheckForCollisionWithKlingonShip( i ) )
        {
            if( discovery.shielded )
            {
                discSpawnKlingonShip( i );
            }
            else
            {
                discShootDiscovery();
                gbPlayTick();
            }
        }
    }
}

void discInitDiscovery()
{
    discovery.x0        = 5;
    discovery.x         = discovery.x0;
    discovery.y         = LCDHEIGHT / 2;
    discovery.vx        = 0;
    discovery.vy        = 2;
    discovery.h         = 14;
    discovery.w         = 8;
    discovery.shield_dx = 11;
    discovery.shield_dy = -2;
    discovery.exploding = false;
    discovery.shielded  = false;
    discovery.score     = 0;
    discovery.damage    = 0;
    discovery.lvl       = 0;
}

// ---------------------------------------------------------------------------
// Missiles - direct port of upstream's own d_missile.ino
// ---------------------------------------------------------------------------

// Upstream returns -1 here through an unsigned `byte`, which its caller then
// fails to detect - see the header comment. The sentinel is the array length
// instead, which the caller really does test for.
int discFindNextMissile()
{
    for( int i = 0; i < DISC_NUM_MISSILES; i++ )
      if( !discMissiles[ i ].isActive )
        return i;

    return DISC_NUM_MISSILES;
}

void discInitMissiles()
{
    for( int i = 0; i < DISC_NUM_MISSILES; i++ )
    {
        discMissiles[ i ].x  = 0;
        discMissiles[ i ].y  = 0;
        discMissiles[ i ].vx = 2;
        discMissiles[ i ].w  = 4;
        discMissiles[ i ].h  = 2;
        discMissiles[ i ].isActive = false;
    }
}

void discLaunchMissile()
{
    int index;

    // Being shot down means you cannot shoot.
    if( discovery.exploding )
      return;

    index = discFindNextMissile();

    if( index >= DISC_NUM_MISSILES )
      return;

    discMissiles[ index ].y = discovery.y + ( DISC_SPRITE_DISCOVERY_HEIGHT / 2 );
    discMissiles[ index ].x = discovery.x + 16;
    discMissiles[ index ].isActive = true;
}

void discDrawMissilesAndUpdatePosition()
{
    for( int i = 0; i < DISC_NUM_MISSILES; i++ )
    {
        if( !discMissiles[ i ].isActive )
          continue;

        gbFillRect( discMissiles[ i ].x, discMissiles[ i ].y,
                    discMissiles[ i ].w, discMissiles[ i ].h );

        discMissiles[ i ].x = discMissiles[ i ].x + discMissiles[ i ].vx;

        // Test against the Klingon ships.
        for( int k = 0; k < DISC_NUM_KLINGONSHIPS; k++ )
        {
            if( gbCollideRectRect( discMissiles[ i ].x, discMissiles[ i ].y,
                                   discMissiles[ i ].w, discMissiles[ i ].h,
                                   discKlingonShips[ k ].x, discKlingonShips[ k ].y, 14, 8 ) )
            {
                // Klingon ship hit
                if( !discKlingonShips[ k ].exploding )
                {
                    gbPlayCancel();
                    discMissiles[ i ].isActive = false;
                    discSpawnKlingonShip( k );
                    discAddPoints( 15 );
                }
            }
        }

        // Once a missile runs off the right edge, return it to the arsenal.
        if( discMissiles[ i ].x > LCDWIDTH )
          discMissiles[ i ].isActive = false;
    }
}

// ---------------------------------------------------------------------------
// Setup and main loop - direct ports of upstream's own x_setup.ino/y_loop.ino
// ---------------------------------------------------------------------------

void discInitGame()
{
    discGame.number_of_lives = 5;
    discGame.max_number_of_lives = 6;
    discGame.is_running = false;
    discInitDiscovery();
    discInitKlingonShips();
    discInitMissiles();
}

// Stands in for the blocking gb.titleScreen(F("FirstKlaas"), SPRITE_DISCOVERY)
// call real upstream re-enters on every tick while the game is not running.
void discTitleScreen()
{
    gbDrawBitmap( ( LCDWIDTH - DISC_SPRITE_DISCOVERY_WIDTH ) / 2, 6, discSpriteDiscovery );

    gbCursorX = 0;
    gbCursorY = 20;
    gbPrintString( "   DISCOVERY\n" );
    gbPrintString( "   FirstKlaas\n" );

    gbCursorX = 0;
    gbCursorY = 40;
    gbPrintString( "  C: START GAME" );
}

void gameDiscovery_init()
{
    gbBegin();
    gbPickRandomSeed(); // no-op, see gamebuinoShim.h's own header comment

    discInitGame();
}

void gameDiscovery_update()
{
    if( !gbUpdate() ) return;

    if( gbPressed( BTN_C ) )
    {
        if( discGame.is_running )
        {
            discGame.is_running = false;
        }
        else
        {
            discInitGame();
            discGame.is_running = true;
        }
    }

    if( !discGame.is_running )
    {
        discTitleScreen();
        gbRenderFrame();
        return;
    }

    if( gbPressed( BTN_A ) )
      if( !discovery.shielded )
        discLaunchMissile();

    if( gbRepeat( BTN_B, 1 ) )
      discovery.shielded = true;
    else
      discovery.shielded = false;

    // Move the player
    if( gbRepeat( BTN_UP, 1 ) )
    {
        discovery.y = discovery.y - discovery.vy;

        if( discovery.y < 1 )
          discovery.y = 1;
    }

    if( gbRepeat( BTN_DOWN, 1 ) )
    {
        discovery.y = discovery.y + discovery.vy;

        // Upstream really does clamp against the ship's own `w`, not `h`.
        if( discovery.y > LCDHEIGHT - discovery.w )
          discovery.y = LCDHEIGHT - discovery.w;
    }

    discCheckForCollisions();
    discDrawMissilesAndUpdatePosition();
    discDrawDiscovery();
    discDrawKlingonShips();
    discDrawLives();
    discUpdateKlingonShips();

    gbCursorX = 20;
    gbCursorY = 2;
    gbPrintNumber( discovery.score );
    gbCursorX = 45;
    gbCursorY = 2;
    gbPrintString( "Lvl:" );
    gbCursorX = 60;
    gbCursorY = 2;
    gbPrintNumber( discovery.lvl + 1 );

    gbRenderFrame();
}
