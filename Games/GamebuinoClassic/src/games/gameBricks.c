// Bricks! (Drakker, no license specified) - a full-featured Breakout with
// four hand-built levels, six brick types, bomb and mine bricks that
// chain-detonate their neighbours, ten collectable power-ups, and both
// rockets and twin lasers as secondary weapons.
//
// The original drakker.org site is long dead; this source was recovered from
// a Wayback Machine snapshot of that page (see the sibling
// gamebuino_classic_source_codes archive project), which is also why the
// repo carries no license file of any kind.
//
// CONTROLS: Left/Right move the bar, Up/Down grow and shrink it by hand
// (a real cheat/debug control upstream ships enabled), Button A fires a
// rocket, Button B fires lasers, Button C pauses. On the pause screen and
// the game-over screen Button A restarts.
//
// Every real `gb.x.y(...)` call site is mechanically rewritten to a plain
// `gbY(...)` function call (see gamePong.c's own header comment for why -
// this dialect has no classes/methods/operator overloading). Every global
// symbol got a `brk`-prefixed name (this cartridge has no linker - every
// ported game shares one flat global namespace). Upstream's own
// `random(a,b)` calls became `a + arand(b-a)`, this project's own
// established ranged-random rewrite convention (see gameFlappyBirdo.c), and
// every sprite table was converted from its own `B`-binary PROGMEM form to
// hex byte-for-byte by script rather than hand-transcribed.
//
// STRUCTS RETURNED BY VALUE -> OUT-PARAMETERS: upstream's own `structs.h`
// exists specifically to let `isThereABrickHere()` return a two-field
// `cursorStruct` by value (its own header comment says as much). A two-word
// struct return is over this dialect's real one-word function-return limit,
// so that function writes through an out-pointer instead and its callers
// keep local x/y pairs - the same treatment gameStarHonor.c already applies
// to its own `Vector2d`.
//
// THE BRICK GRID: upstream keeps four level tables plus a live grid built
// from an array of row POINTERS, and switches level by copying a level table
// over the live rows in place. Flattened here to one live `brkBricks[80]`
// array plus four flat level tables copied into it, which behaves
// identically without needing an array of pointers.
//
// SPRITE SETS: upstream builds ~30 two-entry pointer tables in `setup()`
// (`brick1[0]`, `brick1[1]`, ...) so a brick can pick a tile by the low bit
// of the frame counter, faking a gray dither. Kept exactly, as small
// two-entry index tables of the real sprite arrays.
//
// REAL UPSTREAM QUIRKS, PRESERVED EXACTLY:
// - `brickHit()`'s `case 2:`/`case 3:` deliberately falls through into
//   `case 5:` (there is no `break`), so a non-powerful hit on a type-2 or
//   type-3 brick decrements it and then immediately re-tests it as a type-5.
//   Reproduced literally rather than "corrected" into separate cases.
// - `moveItems()`'s own slow-ball item (type 8) computes both its Y clamps
//   from `xvelo` rather than `yvelo`, so collecting it copies horizontal
//   speed onto the vertical axis instead of slowing it.
// - The warp item (type 9) reads `balls[idx].xvelo` to choose which side to
//   draw its trail on, where `idx` is the ITEM index, not the ball index
//   `id` the surrounding loop uses.
// - The exploding-ball item (type 10) loops `id < countBalls()` rather than
//   over every slot, so with balls in non-contiguous slots it can arm fewer
//   balls than are actually in play.
// - `spawnItem()` looks for a free slot by testing `type == -1`, but the
//   level-change code frees items by writing to `.x` instead - a different
//   field entirely - so items really do survive a level transition.
// - `explosionStruct.duration` is never reset by the level-change code
//   either, and `countBricks()` deliberately ignores brick types 5 and 6
//   (the indestructible ones) so a level can still be completed with them
//   left standing.
// - The `explosion` sprite table contains two seven-digit `B` literals where
//   every other row has eight, so those two rows really are shifted right by
//   one bit. avr-gcc accepts them as 7-bit values; the byte-for-byte
//   conversion here reproduces the same numbers.
// - Up/Down resizing the bar by hand is a real shipped control, not a debug
//   leftover behind a flag, and is kept.
//
// BOUNDED WHERE THIS PLATFORM CANNOT ABSORB AN OVERRUN (documented rather
// than silently changed):
// - The level-change copy loops run `iy <= 8` over a grid only 8 rows tall,
//   writing one full row past the end of the live grid. Bounded to `< 8`.
// - The same level-change block clears `items[0]` through `items[9]` from an
//   array of only 8, writing two entries past the end. Bounded to the real 8.
// - `isThereABrickHere()` derives its grid indices by division with no range
//   check. Tracing every real caller shows the play-area bounds keep them in
//   range, but an out-of-range read here would pull an unrelated global
//   rather than harmless adjacent AVR SRAM, so it range-checks and reports
//   "no brick" outside the grid.
//
// The blocking `gb.titleScreen(F("Bricks! by Drakker\n\n\n4 LEVELS TEST"),
// logo)` call is hand-rolled as an explicit state, matching every other port
// in this cartridge (see gameFifteen.c). Upstream's own `logo` is a
// deliberately blank 64x1 placeholder - its own comment says "Blank logo for
// now" - so this port draws the real title text alone, which is exactly what
// real hardware shows.

#include "../gamebuinoShim.h"

// Fixed play area, defaults to Gamebuino display size.
#define BRK_PLAY_AREA_LEFT 2
#define BRK_PLAY_AREA_RIGHT 82
#define BRK_PLAY_AREA_TOP 2
#define BRK_PLAY_AREA_BOTTOM 48

// Shape of the play area: 10x8 bricks
#define BRK_BRICKS_NB_WIDTH 10
#define BRK_BRICKS_NB_HEIGHT 8
#define BRK_BRICK_THICKNESS 3
#define BRK_BRICK_WIDTH 8
#define BRK_BREAK_ANIMATION_FRAME -10

// Items parameters
#define BRK_NB_ITEMS_MAX 8
#define BRK_NB_ROCKETS_MAX 3
#define BRK_NB_ROCKETS_SIMULTANEOUS_MAX 1
#define BRK_NB_LASERS_MAX 30
#define BRK_NB_LASERS_SIMULTANEOUS_MAX 6
#define BRK_NB_LASERS_PER_ITEM 10

// Bar/Player parameters
#define BRK_PLAYER_HEIGHT 44
#define BRK_BAR_MIN_WIDTH 9
#define BRK_BAR_MAX_WIDTH 24

// Status display related
#define BRK_STATUS_POS_X 66
#define BRK_STATUS_POS_Y 32

// Ball parameters
#define BRK_NB_BALLS_MAX 5
#define BRK_NB_ANGLES 12
#define BRK_MAX_VELO 1.0
#define BRK_MIN_VELO 0.3
#define BRK_BALL_VELO_ITEM_MOD 0.1

// Explosion parameters
#define BRK_NB_EXPLOSIONS_MAX 5
#define BRK_EXPLOSIONS_DURATION 6

#define BRK_STATE_TITLE 0
#define BRK_STATE_PLAY  1

// ---------------------------------------------------------------------------
// State - upstream's own structs, flattened to parallel arrays
// ---------------------------------------------------------------------------

float[BRK_NB_BALLS_MAX] brkBallX;
float[BRK_NB_BALLS_MAX] brkBallY;
float[BRK_NB_BALLS_MAX] brkBallXvelo;
float[BRK_NB_BALLS_MAX] brkBallYvelo;
int[BRK_NB_BALLS_MAX] brkBallAngle;
int[BRK_NB_BALLS_MAX] brkBallExplode;

float brkPlayerX;
float brkPlayerXvelo;
int brkPlayerWidth;
int brkPlayerLives;
int brkPlayerMegaBall;
int brkPlayerRockets;
int brkPlayerLasers;

int[BRK_NB_ITEMS_MAX] brkItemType;
int[BRK_NB_ITEMS_MAX] brkItemX;
int[BRK_NB_ITEMS_MAX] brkItemY;

int[BRK_NB_LASERS_SIMULTANEOUS_MAX] brkLaserX;
float[BRK_NB_LASERS_SIMULTANEOUS_MAX] brkLaserY;

int[BRK_NB_ROCKETS_SIMULTANEOUS_MAX] brkRocketX;
float[BRK_NB_ROCKETS_SIMULTANEOUS_MAX] brkRocketY;

int[BRK_NB_EXPLOSIONS_MAX] brkExplosionX;
int[BRK_NB_EXPLOSIONS_MAX] brkExplosionY;
int[BRK_NB_EXPLOSIONS_MAX] brkExplosionDuration;

int brkFrame;
bool brkPause;
int brkCurrentLevel;
float brkBallAcceleration;
int brkIsReady;
int brkState;

// The live brick grid, indexed [y * BRK_BRICKS_NB_WIDTH + x].
int[80] brkBricks;

// Real upstream's own sprite tables, converted from their own B-binary
// PROGMEM form to hex byte-for-byte by script. Two rows of the explosion
// sprite really are written with only seven binary digits upstream, so
// they carry the same shifted values avr-gcc gives them.

// brick1_1: 8x3
int[5] brkBrick1_1 =
{
    0x08, 0x03, 0xAA, 0x01, 0xFF
};

// brick1_2: 8x3
int[5] brkBrick1_2 =
{
    0x08, 0x03, 0x55, 0x81, 0x7F
};

// brick2_1: 8x3
int[5] brkBrick2_1 =
{
    0x08, 0x03, 0xAA, 0x55, 0xFF
};

// brick2_2: 8x3
int[5] brkBrick2_2 =
{
    0x08, 0x03, 0x55, 0xAB, 0x7F
};

// brick3_1: 8x3
int[5] brkBrick3_1 =
{
    0x08, 0x03, 0xAA, 0x7F, 0xFF
};

// brick3_2: 8x3
int[5] brkBrick3_2 =
{
    0x08, 0x03, 0x55, 0xFF, 0x7F
};

// brick4_1: 8x3
int[5] brkBrick4_1 =
{
    0x08, 0x03, 0xAA, 0x1D, 0xFF
};

// brick4_2: 8x3
int[5] brkBrick4_2 =
{
    0x08, 0x03, 0x55, 0xB9, 0x7F
};

// brick5_1: 8x3
int[5] brkBrick5_1 =
{
    0x08, 0x03, 0xAA, 0x75, 0xFF
};

// brick5_2: 8x3
int[5] brkBrick5_2 =
{
    0x08, 0x03, 0x55, 0xAF, 0x7F
};

// brick6_2: 8x3
int[5] brkBrick6_2 =
{
    0x08, 0x03, 0x55, 0xA5, 0x7F
};

// brick6_21: 8x3
int[5] brkBrick6_21 =
{
    0x08, 0x03, 0xAA, 0x00, 0x55
};

// brickBomb_1: 8x3
int[5] brkBrickBomb_1 =
{
    0x08, 0x03, 0xFF, 0x81, 0xFF
};

// brickBomb_2: 8x3
int[5] brkBrickBomb_2 =
{
    0x08, 0x03, 0xFF, 0xC3, 0xFF
};

// brickBomb_3: 8x3
int[5] brkBrickBomb_3 =
{
    0x08, 0x03, 0xFF, 0xE7, 0xFF
};

// brickBomb_4: 8x3
int[5] brkBrickBomb_4 =
{
    0x08, 0x03, 0xFF, 0xFF, 0xFF
};

// brickMine_1: 8x3
int[5] brkBrickMine_1 =
{
    0x08, 0x03, 0xFF, 0x99, 0xFF
};

// brickMine_2: 8x3
int[5] brkBrickMine_2 =
{
    0x08, 0x03, 0xFF, 0xDB, 0xFF
};

// brickMine_3: 8x3
int[5] brkBrickMine_3 =
{
    0x08, 0x03, 0xFF, 0xFF, 0xFF
};

// brickMine_4: 8x3
int[5] brkBrickMine_4 =
{
    0x08, 0x03, 0xFF, 0xBD, 0xFF
};

// break1_1: 8x3
int[5] brkBreak1_1 =
{
    0x08, 0x03, 0x00, 0x01, 0x7F
};

// break1_2: 8x3
int[5] brkBreak1_2 =
{
    0x08, 0x03, 0xFF, 0xFF, 0xFF
};

// break2_2: 8x3
int[5] brkBreak2_2 =
{
    0x08, 0x03, 0x5A, 0xA5, 0x5A
};

// break3_2: 8x3
int[5] brkBreak3_2 =
{
    0x08, 0x03, 0x0A, 0x56, 0x4C
};

// break4_1: 8x3
int[5] brkBreak4_1 =
{
    0x08, 0x03, 0x00, 0x01, 0x52
};

// break4_2: 8x3
int[5] brkBreak4_2 =
{
    0x08, 0x03, 0x4A, 0x90, 0x28
};

// break5_2: 8x3
int[5] brkBreak5_2 =
{
    0x08, 0x03, 0x00, 0x04, 0x00
};

// picWall1: 8x2
int[4] brkPicWall1 =
{
    0x08, 0x02, 0xEB, 0xFF
};

// picWall2: 8x2
int[4] brkPicWall2 =
{
    0x08, 0x02, 0x56, 0xFF
};

// barSide1: 8x4
int[6] brkBarSide1 =
{
    0x08, 0x04, 0x7F, 0xA0, 0xE0, 0x7F
};

// barSide2: 8x4
int[6] brkBarSide2 =
{
    0x08, 0x04, 0x7F, 0xE0, 0xFF, 0x7F
};

// barMid1: 8x4
int[6] brkBarMid1 =
{
    0x08, 0x04, 0xFF, 0x00, 0x00, 0xFF
};

// barMid2: 8x4
int[6] brkBarMid2 =
{
    0x08, 0x04, 0xFF, 0x00, 0xFF, 0xFF
};

// rocketSprite1: 8x3
int[5] brkRocketSprite1 =
{
    0x08, 0x03, 0x1A, 0xFD, 0x1A
};

// rocketSprite2: 8x3
int[5] brkRocketSprite2 =
{
    0x08, 0x03, 0x38, 0xF6, 0x38
};

// explosion: 16x12
int[26] brkExplosionBitmap =
{
    0x10, 0x0C, 0x30, 0x20, 0x1C, 0x70, 0x0F, 0x98,
    0x78, 0x0C, 0xC0, 0x07, 0xC0, 0x06, 0x60, 0x0C,
    0x21, 0x78, 0x31, 0xBC, 0x27, 0x07, 0x2C, 0x00,
    0x38, 0x00
};

// Real upstream's own 12-entry angle table (its two commented-out entries
// at each end are left out here too, exactly as they are upstream).
float[12] brkAngleX = { 0.939692620786, 0.906307787037, 0.866025403784, 0.819152044289, 0.766044443119, 0.707106781187, 0.642787609687, 0.573576436351, 0.5, 0.422618261741, 0.342020143326, 0.258819045103 };
float[12] brkAngleY = { 0.342020143326, 0.422618261741, 0.5, 0.573576436351, 0.642787609687, 0.707106781187, 0.766044443119, 0.819152044289, 0.866025403784, 0.906307787037, 0.939692620786, 0.965925826289 };

int[80] brkLevel1 =
{
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 1, 1, 1, 1, 1, 1, 1, 1, 0,
    0, 1, 4, 1, 4, 4, 1, 4, 1, 0,
    0, 1, 4, 1, 4, 4, 1, 4, 1, 0,
    0, 1, 1, 1, 1, 1, 1, 1, 1, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

int[80] brkLevel2 =
{
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 5, 5, 4, 4, 4, 4, 5, 5, 0,
    0, 3, 4, 3, 3, 3, 3, 4, 3, 0,
    0, 2, 2, 4, 4, 4, 4, 2, 2, 0,
    0, 1, 1, 0, 0, 0, 0, 1, 1, 0,
    0, 108, 108, 0, 0, 0, 0, 108, 108, 0,
    0, 1, 1, 0, 0, 0, 0, 1, 1, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

int[80] brkLevel3 =
{
    4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
    0, 5, 5, 3, 1, 1, 3, 5, 5, 0,
    0, 0, 3, 3, 100, 100, 3, 3, 0, 0,
    0, 2, 1, 2, 0, 0, 2, 1, 2, 0,
    0, 5, 4, 5, 1, 1, 5, 4, 5, 0,
    0, 6, 4, 6, 3, 3, 6, 4, 6, 0,
    0, 5, 4, 5, 4, 4, 5, 4, 5, 0,
    0, 5, 6, 5, 0, 0, 5, 6, 5, 0
};

int[80] brkLevel4 =
{
    4, 108, 4, 108, 4, 108, 4, 108, 4, 108,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 2, 4, 2, 0, 0, 2, 4, 2, 0,
    0, 2, 100, 2, 0, 0, 2, 100, 2, 0,
    6, 5, 4, 4, 6, 6, 4, 4, 5, 6,
    0, 1, 1, 1, 0, 0, 1, 1, 1, 0,
    0, 1, 4, 1, 4, 4, 1, 4, 1, 0,
    0, 1, 4, 1, 0, 0, 1, 4, 1, 0
};

// Upstream builds these two-entry tile tables in setup() so a brick can pick
// its tile from the low bit of the frame counter, faking a gray dither.
int*[2] brkWall;
int*[2] brkBrick1;
int*[2] brkBrick2;
int*[2] brkBrick3;
int*[2] brkBrick4;
int*[2] brkBrick5;
int*[2] brkBrick6;
int*[2] brkBrick100;
int*[2] brkBrick101;
int*[2] brkBrick102;
int*[2] brkBrick103;
int*[2] brkBrick104;
int*[2] brkBrick105;
int*[2] brkBrick106;
int*[2] brkBrick108;
int*[2] brkBrick109;
int*[2] brkBrick110;
int*[2] brkBrick111;
int*[2] brkBrick112;
int*[2] brkBrick113;
int*[2] brkBrick114;
int*[2] brkBreak1;
int*[2] brkBreak2;
int*[2] brkBreak3;
int*[2] brkBreak4;
int*[2] brkBreak5;
int*[2] brkBarSide;
int*[2] brkBarMid;
int*[2] brkRocket;

// Direct port of the tile-table assignments in upstream's own setup().
void brkInitSprites()
{
    brkWall[ 0 ] = brkPicWall1;
    brkWall[ 1 ] = brkPicWall2;
    brkBrick1[ 0 ] = brkBrick1_1;
    brkBrick1[ 1 ] = brkBrick1_2;
    brkBrick2[ 0 ] = brkBrick2_1;
    brkBrick2[ 1 ] = brkBrick2_2;
    brkBrick3[ 0 ] = brkBrick3_1;
    brkBrick3[ 1 ] = brkBrick3_2;
    brkBrick4[ 0 ] = brkBrick4_1;
    brkBrick4[ 1 ] = brkBrick4_2;
    brkBrick5[ 0 ] = brkBrick5_1;
    brkBrick5[ 1 ] = brkBrick5_2;
    brkBrick6[ 0 ] = brkBrick1_1;
    brkBrick6[ 1 ] = brkBrick6_2;
    brkBrick100[ 0 ] = brkBrickBomb_1;
    brkBrick100[ 1 ] = brkBrickBomb_1;
    brkBrick101[ 0 ] = brkBrickBomb_1;
    brkBrick101[ 1 ] = brkBrickBomb_2;
    brkBrick102[ 0 ] = brkBrickBomb_2;
    brkBrick102[ 1 ] = brkBrickBomb_3;
    brkBrick103[ 0 ] = brkBrickBomb_3;
    brkBrick103[ 1 ] = brkBrickBomb_4;
    brkBrick104[ 0 ] = brkBrickBomb_3;
    brkBrick104[ 1 ] = brkBrickBomb_4;
    brkBrick105[ 0 ] = brkBrickBomb_3;
    brkBrick105[ 1 ] = brkBrickBomb_3;
    brkBrick106[ 0 ] = brkBrickBomb_2;
    brkBrick106[ 1 ] = brkBrickBomb_2;
    brkBrick108[ 0 ] = brkBrickMine_1;
    brkBrick108[ 1 ] = brkBrickMine_1;
    brkBrick109[ 0 ] = brkBrickMine_1;
    brkBrick109[ 1 ] = brkBrickMine_2;
    brkBrick110[ 0 ] = brkBrickMine_2;
    brkBrick110[ 1 ] = brkBrickMine_3;
    brkBrick111[ 0 ] = brkBrickMine_3;
    brkBrick111[ 1 ] = brkBrickMine_4;
    brkBrick112[ 0 ] = brkBrickMine_3;
    brkBrick112[ 1 ] = brkBrickMine_4;
    brkBrick113[ 0 ] = brkBrickMine_3;
    brkBrick113[ 1 ] = brkBrickMine_3;
    brkBrick114[ 0 ] = brkBrickMine_2;
    brkBrick114[ 1 ] = brkBrickMine_2;
    brkBreak1[ 0 ] = brkBreak1_1;
    brkBreak1[ 1 ] = brkBreak1_2;
    brkBreak2[ 0 ] = brkBreak1_1;
    brkBreak2[ 1 ] = brkBreak2_2;
    brkBreak3[ 0 ] = brkBreak1_1;
    brkBreak3[ 1 ] = brkBreak3_2;
    brkBreak4[ 0 ] = brkBreak4_1;
    brkBreak4[ 1 ] = brkBreak4_2;
    brkBreak5[ 0 ] = brkBreak4_1;
    brkBreak5[ 1 ] = brkBreak5_2;
    brkBarSide[ 0 ] = brkBarSide1;
    brkBarSide[ 1 ] = brkBarSide2;
    brkBarMid[ 0 ] = brkBarMid1;
    brkBarMid[ 1 ] = brkBarMid2;
    brkRocket[ 0 ] = brkRocketSprite1;
    brkRocket[ 1 ] = brkRocketSprite2;
}

// ---------------------------------------------------------------------------
// Balls
// ---------------------------------------------------------------------------

void brkChangeBallAngle( int increment, int id )
{
    int a = brkBallAngle[ id ] + increment;

    if( a > BRK_NB_ANGLES - 1 ) a = BRK_NB_ANGLES - 1;
    if( a < 0 ) a = 0;

    brkBallAngle[ id ] = a;
}

int brkCountBalls()
{
    int count = 0;

    for( int idx = 0; idx < BRK_NB_BALLS_MAX; idx++ )
      if( brkBallX[ idx ] > -1.0 )
        count++;

    return count;
}

// Upstream's own spawnBall(ballStruct, randomBall = 0). The struct parameter
// became explicit fields; the default argument became an explicit one.
void brkSpawnBall( float x, float y, float xvelo, float yvelo, int angle, bool randomBall )
{
    int ballFound = -1;
    int randomBallId;
    int oldAngle;

    for( int idx = 0; idx < BRK_NB_BALLS_MAX && ballFound == -1; idx++ )
      if( brkBallX[ idx ] == -1 )
        ballFound = idx;

    if( ballFound <= -1 )
      return;

    if( randomBall )
    {
        randomBallId = -1;

        for( int idx = 0; idx < BRK_NB_BALLS_MAX && randomBallId == -1; idx++ )
          if( brkBallX[ idx ] != -1 )
            randomBallId = idx;

        if( randomBallId == -1 )
          return;

        brkBallX[ ballFound ]       = brkBallX[ randomBallId ];
        brkBallY[ ballFound ]       = brkBallY[ randomBallId ];
        brkBallXvelo[ ballFound ]   = brkBallXvelo[ randomBallId ];
        brkBallYvelo[ ballFound ]   = brkBallYvelo[ randomBallId ];
        brkBallAngle[ ballFound ]   = brkBallAngle[ randomBallId ];
        brkBallExplode[ ballFound ] = brkBallExplode[ randomBallId ];

        oldAngle = brkBallAngle[ ballFound ];

        while( oldAngle == brkBallAngle[ ballFound ] )
          brkChangeBallAngle( -5 + arand( 11 ), ballFound );
    }
    else
    {
        brkBallX[ ballFound ]       = x;
        brkBallY[ ballFound ]       = y;
        brkBallXvelo[ ballFound ]   = xvelo;
        brkBallYvelo[ ballFound ]   = yvelo;
        brkBallAngle[ ballFound ]   = angle;
        brkBallExplode[ ballFound ] = 0;
    }
}

// ---------------------------------------------------------------------------
// Bricks
// ---------------------------------------------------------------------------

// Upstream returns a cursorStruct by value; this writes through out-params
// instead, reporting x = -1 for "no brick here".
void brkIsThereABrickHere( int X, int Y, int* outX, int* outY )
{
    int brickX;
    int brickY;

    *outX = -1;
    *outY = 0;

    if( Y >= ( BRK_PLAY_AREA_TOP + ( BRK_BRICKS_NB_HEIGHT * BRK_BRICK_THICKNESS ) ) )
      return;

    brickX = ( X - BRK_PLAY_AREA_LEFT ) / BRK_BRICK_WIDTH;
    brickY = ( Y - BRK_PLAY_AREA_TOP ) / BRK_BRICK_THICKNESS;

    // Range check - see the header comment.
    if( brickX < 0 || brickX >= BRK_BRICKS_NB_WIDTH ) return;
    if( brickY < 0 || brickY >= BRK_BRICKS_NB_HEIGHT ) return;

    if( brkBricks[ brickY * BRK_BRICKS_NB_WIDTH + brickX ] > 0 )
    {
        *outX = brickX;
        *outY = brickY;
    }
}

int brkCountBricks()
{
    int nbBricks = 0;

    for( int x = 0; x < BRK_BRICKS_NB_WIDTH; x++ )
      for( int y = 0; y < BRK_BRICKS_NB_HEIGHT; y++ )
      {
          int b = brkBricks[ y * BRK_BRICKS_NB_WIDTH + x ];

          // Types 5 and 6 are indestructible and deliberately not counted.
          if( b > 0 && b != 5 && b != 6 )
            nbBricks++;
      }

    return nbBricks;
}

void brkSpawnExplosion( int x, int y )
{
    int explosionFound = -1;
    int explosionFoundDuration = 255;

    for( int idx = 0; idx < BRK_NB_EXPLOSIONS_MAX; idx++ )
      if( brkExplosionDuration[ idx ] < explosionFoundDuration )
      {
          explosionFound = idx;
          explosionFoundDuration = brkExplosionDuration[ idx ];
      }

    if( explosionFound > -1 )
    {
        brkExplosionX[ explosionFound ] = x - 8;
        brkExplosionY[ explosionFound ] = y - 6;
        brkExplosionDuration[ explosionFound ] = BRK_EXPLOSIONS_DURATION;
    }
}

void brkSpawnItem( int X, int Y )
{
    int itemPos = -1;
    int it;

    for( int idx = 0; idx < BRK_NB_ITEMS_MAX && itemPos == -1; idx++ )
      if( brkItemType[ idx ] == -1 )
        itemPos = idx;

    if( itemPos == -1 )
      return;

    it = arand( 63 );
    brkItemX[ itemPos ] = X;
    brkItemY[ itemPos ] = Y;

    if( it < 5 )       brkItemType[ itemPos ] = 7;
    else if( it < 10 ) brkItemType[ itemPos ] = 8;
    else if( it < 12 ) brkItemType[ itemPos ] = 6;
    else if( it < 14 ) brkItemType[ itemPos ] = 10;
    else if( it < 19 ) brkItemType[ itemPos ] = 3;
    else if( it < 23 ) brkItemType[ itemPos ] = 9;
    else if( it < 38 ) brkItemType[ itemPos ] = 1;
    else if( it < 48 ) brkItemType[ itemPos ] = 2;
    else if( it < 56 ) brkItemType[ itemPos ] = 4;
    else               brkItemType[ itemPos ] = 5;
}

void brkBrickExplosion( int idxX, int idxY, int power );

// Upstream's own brickHit(). The `case 2:`/`case 3:` fallthrough into
// `case 5:` is real and deliberately reproduced - see the header comment.
void brkBrickHit( int idxX, int idxY, bool powerfull )
{
    int cell = idxY * BRK_BRICKS_NB_WIDTH + idxX;
    int type = brkBricks[ cell ];
    int bX;
    int bY;

    if( type == 1 )
    {
        brkBricks[ cell ] = BRK_BREAK_ANIMATION_FRAME;
    }
    else if( type == 2 || type == 3 )
    {
        if( powerfull ) brkBricks[ cell ] = BRK_BREAK_ANIMATION_FRAME;
        else brkBricks[ cell ]--;

        // Real upstream has no break here, so the type-5 test runs next.
        if( powerfull ) brkBricks[ cell ] = BRK_BREAK_ANIMATION_FRAME;
    }
    else if( type == 5 )
    {
        if( powerfull ) brkBricks[ cell ] = BRK_BREAK_ANIMATION_FRAME;
    }
    else if( type == 6 )
    {
        if( powerfull ) brkBricks[ cell ] = BRK_BREAK_ANIMATION_FRAME;
        else brkBricks[ cell ] = -21;
    }
    else if( type == 4 )
    {
        brkBricks[ cell ] = BRK_BREAK_ANIMATION_FRAME;
        bX = idxX * BRK_BRICK_WIDTH + BRK_PLAY_AREA_LEFT + 4;
        bY = idxY * BRK_BRICK_THICKNESS + BRK_PLAY_AREA_TOP + 1;
        brkSpawnItem( bX, bY );
    }
    else if( type >= 100 && type <= 106 )
    {
        brkBricks[ cell ] = BRK_BREAK_ANIMATION_FRAME;
        brkBrickExplosion( idxX, idxY, 2 );
        bX = idxX * BRK_BRICK_WIDTH + BRK_PLAY_AREA_LEFT + 4;
        bY = idxY * BRK_BRICK_THICKNESS + BRK_PLAY_AREA_TOP + 1;
        brkSpawnExplosion( bX, bY );
    }
    else if( type >= 108 && type <= 114 )
    {
        brkBricks[ cell ] = BRK_BREAK_ANIMATION_FRAME;
        brkBrickExplosion( idxX, idxY, 1 );
        bX = idxX * BRK_BRICK_WIDTH + BRK_PLAY_AREA_LEFT + 4;
        bY = idxY * BRK_BRICK_THICKNESS + BRK_PLAY_AREA_TOP + 1;
        brkSpawnExplosion( bX, bY );
    }
}

void brkBrickExplosion( int idxX, int idxY, int power )
{
    for( int bx = -power; bx <= power; bx++ )
      for( int by = -power; by <= power; by++ )
        if( idxX + bx >= 0 && idxX + bx < BRK_BRICKS_NB_WIDTH
         && idxY + by >= 0 && idxY + by < BRK_BRICKS_NB_HEIGHT )
        {
            int lo = 11;
            int hi = ( power * 20 ) + 2;
            int roll;

            if( hi <= lo ) roll = lo;
            else roll = lo + arand( hi - lo );

            if( roll > ( gbAbsInt( bx ) + gbAbsInt( by ) ) * 10 )
              brkBrickHit( idxX + bx, idxY + by, true );
        }
}

// ---------------------------------------------------------------------------
// Ball physics - direct port of upstream's own moveBalls()
// ---------------------------------------------------------------------------

void brkMoveBalls()
{
    for( int idx = 0; idx < BRK_NB_BALLS_MAX; idx++ )
    {
        bool bounceUp;
        bool bounceDown;
        bool bounceRight;
        bool bounceLeft;
        int hitXx, hitXy, hitYx, hitYy;
        float ax, ay, stepX, stepY;
        int ballXL, ballYU;

        if( brkBallX[ idx ] == -1 )
          continue;

        ax = brkAngleX[ brkBallAngle[ idx ] ];
        ay = brkAngleY[ brkBallAngle[ idx ] ];
        stepX = brkBallXvelo[ idx ] * ax;
        stepY = brkBallYvelo[ idx ] * ay;

        if( brkBallY[ idx ] + stepY >= BRK_PLAY_AREA_BOTTOM )
        {
            brkSpawnExplosion( (int)brkBallX[ idx ], (int)brkBallY[ idx ] );
            brkBallX[ idx ] = -1;
            continue;
        }

        bounceRight = false;
        bounceLeft = false;
        bounceUp = false;
        bounceDown = false;

        hitXx = -1; hitXy = 0;
        hitYx = -1; hitYy = 0;

        // Ball X collision detection
        if( brkBallX[ idx ] + stepX >= BRK_PLAY_AREA_RIGHT )
        {
            bounceRight = true;
        }
        else if( brkBallX[ idx ] + stepX <= BRK_PLAY_AREA_LEFT )
        {
            bounceLeft = true;
        }
        else
        {
            brkIsThereABrickHere( (int)floor( brkBallX[ idx ] + stepX ),
                                  (int)floor( brkBallY[ idx ] ), &hitXx, &hitXy );

            if( hitXx > -1 && brkPlayerMegaBall == 0 )
            {
                if( brkBallX[ idx ] + stepX >= ceil( brkBallX[ idx ] ) ) bounceRight = true;
                else bounceLeft = true;
            }
        }

        // Ball Y collision detection
        if( brkBallY[ idx ] + stepY >= BRK_PLAY_AREA_BOTTOM && idx == 0 )
        {
            bounceDown = true;
        }
        else if( brkBallY[ idx ] + stepY <= BRK_PLAY_AREA_TOP )
        {
            bounceUp = true;
        }
        else
        {
            brkIsThereABrickHere( (int)floor( brkBallX[ idx ] ),
                                  (int)floor( brkBallY[ idx ] + stepY ), &hitYx, &hitYy );

            if( hitYx > -1 && brkPlayerMegaBall == 0 )
            {
                if( brkBallY[ idx ] + stepY >= ceil( brkBallY[ idx ] ) ) bounceDown = true;
                else bounceUp = true;
            }
        }

        // Are we hitting a brick directly in the corner?
        if( hitXx == -1 && hitYx == -1 && !bounceDown && !bounceUp && !bounceLeft && !bounceRight )
        {
            brkIsThereABrickHere( (int)floor( brkBallX[ idx ] + stepX ),
                                  (int)floor( brkBallY[ idx ] + stepY ), &hitXx, &hitXy );

            if( hitXx > -1 && brkPlayerMegaBall == 0 )
            {
                if( stepX >= stepY )
                {
                    if( brkBallY[ idx ] + stepY >= ceil( brkBallY[ idx ] ) ) bounceDown = true;
                    else bounceUp = true;
                }
                else
                {
                    if( brkBallX[ idx ] + stepX >= ceil( brkBallX[ idx ] ) ) bounceRight = true;
                    else bounceLeft = true;
                }
            }
        }

        // Ball collision with the bar and resulting angle change
        if( brkBallY[ idx ] >= BRK_PLAYER_HEIGHT - 1
         && brkBallY[ idx ] <= BRK_PLAY_AREA_BOTTOM - 1
         && brkBallX[ idx ] >= brkPlayerX
         && brkBallX[ idx ] < brkPlayerX + brkPlayerWidth
         && brkBallYvelo[ idx ] > 0 )
        {
            bounceDown = true;

            if( brkBallX[ idx ] >= brkPlayerX + brkPlayerWidth - 1 && brkBallXvelo[ idx ] >= 0 )
              brkChangeBallAngle( -4, idx );
            else if( brkBallX[ idx ] >= brkPlayerX + brkPlayerWidth - 1 && brkBallXvelo[ idx ] < 0 )
              brkChangeBallAngle( 4, idx );
            else if( brkBallX[ idx ] >= brkPlayerX + brkPlayerWidth - 2 && brkBallXvelo[ idx ] >= 0 )
              brkChangeBallAngle( -3, idx );
            else if( brkBallX[ idx ] >= brkPlayerX + brkPlayerWidth - 2 && brkBallXvelo[ idx ] < 0 )
              brkChangeBallAngle( 3, idx );
            else if( brkBallX[ idx ] >= brkPlayerX + brkPlayerWidth - 3 && brkBallXvelo[ idx ] >= 0 )
              brkChangeBallAngle( -2, idx );
            else if( brkBallX[ idx ] >= brkPlayerX + brkPlayerWidth - 3 && brkBallXvelo[ idx ] < 0 )
              brkChangeBallAngle( 2, idx );
            else if( brkBallX[ idx ] >= brkPlayerX + brkPlayerWidth - 4 && brkBallXvelo[ idx ] >= 0 )
              brkChangeBallAngle( -1, idx );
            else if( brkBallX[ idx ] >= brkPlayerX + brkPlayerWidth - 4 && brkBallXvelo[ idx ] < 0 )
              brkChangeBallAngle( 1, idx );

            // The angle may have changed, so the step has to be recomputed
            // before it is used to reposition the ball below.
            ax = brkAngleX[ brkBallAngle[ idx ] ];
            ay = brkAngleY[ brkBallAngle[ idx ] ];
            stepX = brkBallXvelo[ idx ] * ax;
            stepY = brkBallYvelo[ idx ] * ay;
        }

        if( bounceRight )
        {
            brkBallX[ idx ] = ceil( brkBallX[ idx ] ) - stepX;
            brkBallXvelo[ idx ] = -fabs( brkBallXvelo[ idx ] );
        }
        else if( bounceLeft )
        {
            brkBallX[ idx ] = floor( brkBallX[ idx ] ) - stepX;
            brkBallXvelo[ idx ] = fabs( brkBallXvelo[ idx ] );
        }

        if( bounceDown )
        {
            brkBallY[ idx ] = ceil( brkBallY[ idx ] ) - stepY;
            brkBallYvelo[ idx ] = -fabs( brkBallYvelo[ idx ] );
        }
        else if( bounceUp )
        {
            brkBallY[ idx ] = floor( brkBallY[ idx ] ) - stepY;
            brkBallYvelo[ idx ] = fabs( brkBallYvelo[ idx ] );
        }

        if( bounceLeft || bounceRight || bounceUp || bounceDown )
          gbPlayNoteChannel( 40, 1, 0 );

        if( !bounceRight && !bounceLeft ) brkBallX[ idx ] = brkBallX[ idx ] + stepX;
        if( !bounceUp && !bounceDown ) brkBallY[ idx ] = brkBallY[ idx ] + stepY;

        if( stepX >= 0 ) ballXL = (int)floor( brkBallX[ idx ] - 1 );
        else ballXL = (int)floor( brkBallX[ idx ] );

        if( stepY >= 0 ) ballYU = (int)floor( brkBallY[ idx ] - 1 );
        else ballYU = (int)floor( brkBallY[ idx ] );

        gbFillRect( ballXL, ballYU, 2, 2 );

        if( brkPlayerMegaBall > 0 && brkFrame == 1 )
          gbDrawLine( (int)( brkBallX[ idx ] + brkBallXvelo[ idx ] * 4 * ax ),
                      (int)( brkBallY[ idx ] + brkBallYvelo[ idx ] * 4 * ay ),
                      (int)( brkBallX[ idx ] - brkBallXvelo[ idx ] * 8 * ax ),
                      (int)( brkBallY[ idx ] - brkBallYvelo[ idx ] * 8 * ay ) );

        if( brkBallXvelo[ idx ] > 0 )
        {
            brkBallXvelo[ idx ] = brkBallXvelo[ idx ] + brkBallAcceleration;
            if( brkBallXvelo[ idx ] > BRK_MAX_VELO ) brkBallXvelo[ idx ] = BRK_MAX_VELO;
        }
        else
        {
            brkBallXvelo[ idx ] = brkBallXvelo[ idx ] - brkBallAcceleration;
            if( brkBallXvelo[ idx ] < -BRK_MAX_VELO ) brkBallXvelo[ idx ] = -BRK_MAX_VELO;
        }

        if( brkBallYvelo[ idx ] > 0 )
        {
            brkBallYvelo[ idx ] = brkBallYvelo[ idx ] + brkBallAcceleration;
            if( brkBallYvelo[ idx ] > BRK_MAX_VELO ) brkBallYvelo[ idx ] = BRK_MAX_VELO;
        }
        else
        {
            brkBallYvelo[ idx ] = brkBallYvelo[ idx ] - brkBallAcceleration;
            if( brkBallYvelo[ idx ] < -BRK_MAX_VELO ) brkBallYvelo[ idx ] = -BRK_MAX_VELO;
        }

        if( hitYx > -1 )
        {
            if( brkBallExplode[ idx ] == 1 )
            {
                brkBrickExplosion( hitYx, hitYy, 2 );
                brkSpawnExplosion( (int)brkBallX[ idx ], (int)brkBallY[ idx ] );
                brkBallExplode[ idx ] = 0;
            }
            else
            {
                brkBrickHit( hitYx, hitYy, brkPlayerMegaBall != 0 );
            }
        }

        if( hitXx > -1 )
        {
            if( brkBallExplode[ idx ] == 1 )
            {
                brkBrickExplosion( hitXx, hitXy, 2 );
                brkSpawnExplosion( (int)brkBallX[ idx ], (int)brkBallY[ idx ] );
                brkBallExplode[ idx ] = 0;
            }
            else
            {
                brkBrickHit( hitXx, hitXy, brkPlayerMegaBall != 0 );
            }
        }
    }

    if( brkPlayerMegaBall > 0 )
      brkPlayerMegaBall--;
}

// ---------------------------------------------------------------------------
// Drawing
// ---------------------------------------------------------------------------

// Draw the walls around the play field
void brkDrawBorders()
{
    for( int idx = 0; idx < BRK_PLAY_AREA_BOTTOM; idx = idx + 8 )
    {
        gbDrawBitmapRotated( 0, idx, brkWall[ brkFrame ], 1, 0 );                    // ROTCCW
        gbDrawBitmapRotated( BRK_PLAY_AREA_RIGHT, idx, brkWall[ brkFrame ], 3, 0 );  // ROTCW
    }

    for( int idx = 2; idx < BRK_PLAY_AREA_RIGHT; idx = idx + 8 )
      gbDrawBitmap( idx, 0, brkWall[ brkFrame ] );
}

void brkDrawBrickAt( int x, int y, int* bitmap )
{
    gbDrawBitmap( ( x * BRK_BRICK_WIDTH ) + BRK_PLAY_AREA_LEFT,
                  ( y * BRK_BRICK_THICKNESS ) + BRK_PLAY_AREA_TOP, bitmap );
}

void brkDrawBricks()
{
    for( int x = 0; x < BRK_BRICKS_NB_WIDTH; x++ )
      for( int y = 0; y < BRK_BRICKS_NB_HEIGHT; y++ )
      {
          int cell = y * BRK_BRICKS_NB_WIDTH + x;
          int t = brkBricks[ cell ];

          if( t == 0 )
            continue;

          if( t == 1 )      brkDrawBrickAt( x, y, brkBrick1[ brkFrame ] );
          else if( t == 2 ) brkDrawBrickAt( x, y, brkBrick2[ brkFrame ] );
          else if( t == 3 ) brkDrawBrickAt( x, y, brkBrick3[ brkFrame ] );
          else if( t == 4 ) brkDrawBrickAt( x, y, brkBrick4[ brkFrame ] );
          else if( t == 5 ) brkDrawBrickAt( x, y, brkBrick5[ brkFrame ] );
          else if( t == 6 ) brkDrawBrickAt( x, y, brkBrick6[ brkFrame ] );

          // Bomb-brick animation
          else if( t == 100 ) { brkDrawBrickAt( x, y, brkBrick100[ brkFrame ] ); if( brkFrame == 0 ) brkBricks[ cell ]++; }
          else if( t == 101 ) { brkDrawBrickAt( x, y, brkBrick101[ brkFrame ] ); if( brkFrame == 1 ) brkBricks[ cell ]++; }
          else if( t == 102 ) { brkDrawBrickAt( x, y, brkBrick102[ brkFrame ] ); if( brkFrame == 0 ) brkBricks[ cell ]++; }
          else if( t == 103 ) { brkDrawBrickAt( x, y, brkBrick103[ brkFrame ] ); if( brkFrame == 1 ) brkBricks[ cell ]++; }
          else if( t == 104 ) { brkDrawBrickAt( x, y, brkBrick104[ brkFrame ] ); if( brkFrame == 0 ) brkBricks[ cell ]++; }
          else if( t == 105 ) { brkDrawBrickAt( x, y, brkBrick105[ brkFrame ] ); if( brkFrame == 1 ) brkBricks[ cell ]++; }
          else if( t == 106 ) { brkDrawBrickAt( x, y, brkBrick106[ brkFrame ] ); if( brkFrame == 0 ) brkBricks[ cell ] = 100; }

          // Mine-brick animation
          else if( t == 108 ) { brkDrawBrickAt( x, y, brkBrick108[ brkFrame ] ); if( brkFrame == 1 ) brkBricks[ cell ]++; }
          else if( t == 109 ) { brkDrawBrickAt( x, y, brkBrick109[ brkFrame ] ); if( brkFrame == 0 ) brkBricks[ cell ]++; }
          else if( t == 110 ) { brkDrawBrickAt( x, y, brkBrick110[ brkFrame ] ); if( brkFrame == 1 ) brkBricks[ cell ]++; }
          else if( t == 111 ) { brkDrawBrickAt( x, y, brkBrick111[ brkFrame ] ); if( brkFrame == 0 ) brkBricks[ cell ]++; }
          else if( t == 112 ) { brkDrawBrickAt( x, y, brkBrick112[ brkFrame ] ); if( brkFrame == 1 ) brkBricks[ cell ]++; }
          else if( t == 113 ) { brkDrawBrickAt( x, y, brkBrick113[ brkFrame ] ); if( brkFrame == 0 ) brkBricks[ cell ]++; }
          else if( t == 114 ) { brkDrawBrickAt( x, y, brkBrick114[ brkFrame ] ); if( brkFrame == 1 ) brkBricks[ cell ] = 108; }

          // Break animation, counting up towards zero
          else if( t == -10 || t == -9 ) { brkDrawBrickAt( x, y, brkBreak1[ brkFrame ] ); brkBricks[ cell ]++; }
          else if( t == -8 || t == -7 )  { brkDrawBrickAt( x, y, brkBreak2[ brkFrame ] ); brkBricks[ cell ]++; }
          else if( t == -6 || t == -5 )  { brkDrawBrickAt( x, y, brkBreak3[ brkFrame ] ); brkBricks[ cell ]++; }
          else if( t == -4 || t == -3 )  { brkDrawBrickAt( x, y, brkBreak4[ brkFrame ] ); brkBricks[ cell ]++; }
          else if( t == -2 || t == -1 )  { brkDrawBrickAt( x, y, brkBreak5[ brkFrame ] ); brkBricks[ cell ]++; }

          // Regenerating type-6 brick, counting down towards -20
          else if( t == -20 ) { brkDrawBrickAt( x, y, brkBreak1[ brkFrame ] ); brkBricks[ cell ] = 6; }
          else if( t == -19 ) { brkDrawBrickAt( x, y, brkBreak1[ brkFrame ] ); brkBricks[ cell ]--; }
          else if( t == -18 || t == -17 ) { brkDrawBrickAt( x, y, brkBreak2[ brkFrame ] ); brkBricks[ cell ]--; }
          else if( t == -16 || t == -15 ) { brkDrawBrickAt( x, y, brkBreak3[ brkFrame ] ); brkBricks[ cell ]--; }
          else if( t == -14 || t == -13 ) { brkDrawBrickAt( x, y, brkBreak4[ brkFrame ] ); brkBricks[ cell ]--; }
          else if( t == -12 || t == -11 ) { brkDrawBrickAt( x, y, brkBreak5[ brkFrame ] ); brkBricks[ cell ]--; }

          else if( t == -21 )
          {
              if( brkFrame == 0 )
              {
                  if( arand( 1000 ) == 0 )
                    brkBricks[ cell ] = -11;
              }
              else
              {
                  brkDrawBrickAt( x, y, brkBrick6_21 );
              }
          }
      }
}

void brkDrawExplosions()
{
    for( int idx = 0; idx < BRK_NB_EXPLOSIONS_MAX; idx++ )
      if( brkExplosionX[ idx ] != -99 )
      {
          gbSetColor( 2 ); // INVERT
          gbDrawBitmapRotated( brkExplosionX[ idx ], brkExplosionY[ idx ],
                               brkExplosionBitmap, arand( 4 ), arand( 4 ) );
          gbSetColor( 1 ); // BLACK

          brkExplosionDuration[ idx ]--;

          if( brkExplosionDuration[ idx ] < 1 )
            brkExplosionX[ idx ] = -99;

          if( brkFrame == 0 )
            gbPlayNoteChannel( arand( 2 ), 1, 0 );
      }
}

// ---------------------------------------------------------------------------
// Items
// ---------------------------------------------------------------------------

void brkMoveItems()
{
    for( int idx = 0; idx < BRK_NB_ITEMS_MAX; idx++ )
    {
        if( brkItemType[ idx ] == -1 )
          continue;

        if( brkFrame == 1 )
          brkItemY[ idx ]++;

        if( brkItemY[ idx ] > BRK_PLAY_AREA_BOTTOM + 2 )
        {
            brkItemType[ idx ] = -1;
            continue;
        }

        if( brkItemY[ idx ] >= BRK_PLAYER_HEIGHT - 3
         && brkItemY[ idx ] <= BRK_PLAY_AREA_BOTTOM
         && brkItemX[ idx ] >= brkPlayerX + BRK_PLAY_AREA_LEFT - 1
         && brkItemX[ idx ] <= brkPlayerX + BRK_PLAY_AREA_LEFT + brkPlayerWidth + 1 )
        {
            int t = brkItemType[ idx ];

            if( t == 1 )
            {
                if( brkPlayerWidth < BRK_BAR_MAX_WIDTH )
                  brkPlayerWidth++;
            }
            else if( t == 2 )
            {
                if( brkPlayerWidth > BRK_BAR_MIN_WIDTH )
                {
                    brkPlayerWidth = brkPlayerWidth - 2;
                    if( brkPlayerWidth < BRK_BAR_MIN_WIDTH )
                      brkPlayerWidth = BRK_BAR_MIN_WIDTH;
                }
            }
            else if( t == 3 )
            {
                brkSpawnBall( 0, 0, 0, 0, 0, true );
            }
            else if( t == 4 )
            {
                brkPlayerLasers = brkPlayerLasers + BRK_NB_LASERS_PER_ITEM;
                if( brkPlayerLasers > BRK_NB_LASERS_MAX )
                  brkPlayerLasers = BRK_NB_LASERS_MAX;
            }
            else if( t == 5 )
            {
                brkPlayerRockets = brkPlayerRockets + 1;
                if( brkPlayerRockets > BRK_NB_ROCKETS_MAX )
                  brkPlayerRockets = BRK_NB_ROCKETS_MAX;
            }
            else if( t == 6 || t == 7 )
            {
                // Type 6 deliberately falls through into type 7 upstream, so
                // the mega ball also speeds every ball up.
                if( t == 6 )
                {
                    brkPlayerMegaBall = brkPlayerMegaBall + ( 115 - ( 10 * brkCountBalls() ) );
                    brkItemType[ idx ] = 7;
                }

                for( int id = 0; id < BRK_NB_BALLS_MAX; id++ )
                  if( brkBallX[ id ] > -1 )
                  {
                      if( brkBallXvelo[ id ] >= 0 )
                      {
                          brkBallXvelo[ id ] = brkBallXvelo[ id ] + BRK_BALL_VELO_ITEM_MOD;
                          if( brkBallXvelo[ id ] > BRK_MAX_VELO ) brkBallXvelo[ id ] = BRK_MAX_VELO;
                      }
                      else
                      {
                          brkBallXvelo[ id ] = brkBallXvelo[ id ] - BRK_BALL_VELO_ITEM_MOD;
                          if( brkBallXvelo[ id ] < -BRK_MAX_VELO ) brkBallXvelo[ id ] = -BRK_MAX_VELO;
                      }

                      if( brkBallYvelo[ id ] >= 0 )
                      {
                          brkBallYvelo[ id ] = brkBallYvelo[ id ] + BRK_BALL_VELO_ITEM_MOD;
                          if( brkBallYvelo[ id ] > BRK_MAX_VELO ) brkBallYvelo[ id ] = BRK_MAX_VELO;
                      }
                      else
                      {
                          brkBallYvelo[ id ] = brkBallYvelo[ id ] - BRK_BALL_VELO_ITEM_MOD;
                          if( brkBallYvelo[ id ] < -BRK_MAX_VELO ) brkBallYvelo[ id ] = -BRK_MAX_VELO;
                      }
                  }
            }
            else if( t == 8 )
            {
                // Both Y clamps really do read xvelo upstream - see the
                // header comment.
                for( int id = 0; id < BRK_NB_BALLS_MAX; id++ )
                  if( brkBallX[ id ] > -1 )
                  {
                      if( brkBallXvelo[ id ] <= 0 )
                      {
                          brkBallXvelo[ id ] = brkBallXvelo[ id ] + BRK_BALL_VELO_ITEM_MOD;
                          if( brkBallXvelo[ id ] > -BRK_MIN_VELO ) brkBallXvelo[ id ] = -BRK_MIN_VELO;
                      }
                      else
                      {
                          brkBallXvelo[ id ] = brkBallXvelo[ id ] - BRK_BALL_VELO_ITEM_MOD;
                          if( brkBallXvelo[ id ] < BRK_MIN_VELO ) brkBallXvelo[ id ] = BRK_MIN_VELO;
                      }

                      if( brkBallYvelo[ id ] <= 0 )
                      {
                          brkBallYvelo[ id ] = brkBallXvelo[ id ] + BRK_BALL_VELO_ITEM_MOD;
                          if( brkBallYvelo[ id ] > -BRK_MIN_VELO ) brkBallYvelo[ id ] = -BRK_MIN_VELO;
                      }
                      else
                      {
                          brkBallYvelo[ id ] = brkBallXvelo[ id ] - BRK_BALL_VELO_ITEM_MOD;
                          if( brkBallYvelo[ id ] < BRK_MIN_VELO ) brkBallYvelo[ id ] = BRK_MIN_VELO;
                      }
                  }
            }
            else if( t == 9 )
            {
                for( int id = 0; id < BRK_NB_BALLS_MAX; id++ )
                  if( brkBallX[ id ] > -1 )
                  {
                      int tmpWarp = 0;
                      int probeX, probeY;

                      brkBallYvelo[ id ] = -fabs( brkBallYvelo[ id ] );

                      brkIsThereABrickHere( (int)brkBallX[ id ], (int)( brkBallY[ id ] + tmpWarp ),
                                            &probeX, &probeY );

                      while( probeX == -1 && brkBallY[ id ] + tmpWarp > BRK_PLAY_AREA_TOP )
                      {
                          tmpWarp--;
                          brkIsThereABrickHere( (int)brkBallX[ id ], (int)( brkBallY[ id ] + tmpWarp ),
                                                &probeX, &probeY );
                      }

                      gbDrawLine( (int)brkBallX[ id ], (int)brkBallY[ id ],
                                  (int)brkBallX[ id ], (int)( brkBallY[ id ] + tmpWarp + 1 ) );

                      // `idx` here is the ITEM index, not the ball index -
                      // real upstream, preserved.
                      if( brkBallXvelo[ idx ] > 0 )
                        gbDrawLine( (int)brkBallX[ id ] - 1, (int)brkBallY[ id ],
                                    (int)brkBallX[ id ] - 1, (int)( brkBallY[ id ] + tmpWarp + 1 ) );
                      else
                        gbDrawLine( (int)brkBallX[ id ] + 1, (int)brkBallY[ id ],
                                    (int)brkBallX[ id ] + 1, (int)( brkBallY[ id ] + tmpWarp + 1 ) );

                      brkBallY[ id ] = brkBallY[ id ] + tmpWarp + 1;
                      gbPlayNoteChannel( 50, 3, 0 );
                  }
            }
            else if( t == 10 )
            {
                // Really loops to countBalls(), not to NB_BALLS_MAX.
                int n = brkCountBalls();

                for( int id = 0; id < n; id++ )
                  if( brkBallX[ id ] > -1 )
                    brkBallExplode[ id ] = 1;
            }
            else if( t <= -2 && t >= -10 )
            {
                gbDrawCircle( brkItemX[ idx ], brkItemY[ idx ], gbAbsInt( t ) );
                brkItemType[ idx ]--;
            }
            else if( t == -11 )
            {
                brkItemType[ idx ] = -1;
            }

            if( brkItemType[ idx ] > -1 )
            {
                brkItemType[ idx ] = -2;
                gbPlayNoteChannel( 23, 1, 0 );
            }
        }
        else
        {
            int t = brkItemType[ idx ];

            gbCursorX = brkItemX[ idx ] - 2;
            gbCursorY = brkItemY[ idx ];

            if( t == 1 )       gbPrintString( "E" );
            else if( t == 2 )  gbPrintString( "C" );
            else if( t == 3 )  gbPrintString( "B" );
            else if( t == 4 )  gbPrintString( "L" );
            else if( t == 5 )  gbPrintString( "R" );
            else if( t == 6 )  gbPrintString( "M" );
            else if( t == 7 )  gbPrintString( "F" );
            else if( t == 8 )  gbPrintString( "S" );
            else if( t == 9 )  gbPrintString( "W" );
            else if( t == 10 ) gbPrintString( "X" );
        }
    }
}

// ---------------------------------------------------------------------------
// Player, lasers and rockets
// ---------------------------------------------------------------------------

void brkMovePlayer()
{
    if( gbRepeat( BTN_RIGHT, 1 ) )
    {
        brkPlayerXvelo = brkPlayerXvelo + 0.5;
        if( brkPlayerXvelo < 1 ) brkPlayerXvelo = 1;
    }
    else if( gbRepeat( BTN_LEFT, 1 ) )
    {
        brkPlayerXvelo = brkPlayerXvelo - 0.5;
        if( brkPlayerXvelo > -1 ) brkPlayerXvelo = -1;
    }
    else
    {
        brkPlayerXvelo = 0;
    }

    if( brkPlayerX + brkPlayerXvelo < BRK_PLAY_AREA_LEFT )
    {
        brkPlayerX = BRK_PLAY_AREA_LEFT;
        brkPlayerXvelo = 0;
    }
    else if( brkPlayerX + brkPlayerXvelo > BRK_PLAY_AREA_RIGHT - brkPlayerWidth )
    {
        brkPlayerX = BRK_PLAY_AREA_RIGHT - brkPlayerWidth;
        brkPlayerXvelo = 0;
    }
    else
    {
        brkPlayerX = brkPlayerX + brkPlayerXvelo;
    }

    gbDrawBitmap( (int)floor( brkPlayerX ), BRK_PLAYER_HEIGHT, brkBarSide[ brkFrame ] );
    gbDrawBitmapRotated( (int)floor( brkPlayerX ) + brkPlayerWidth - 9, BRK_PLAYER_HEIGHT,
                         brkBarSide[ brkFrame ], 0, 1 ); // NOROT, FLIPH

    if( brkPlayerWidth > 16 && brkPlayerWidth < 20 )
      gbDrawBitmap( (int)floor( brkPlayerX ) + 3, BRK_PLAYER_HEIGHT, brkBarMid[ brkFrame ] );
    else if( brkPlayerWidth >= 20 )
      gbDrawBitmap( (int)floor( brkPlayerX ) + 8, BRK_PLAYER_HEIGHT, brkBarMid[ brkFrame ] );
}

void brkLaunchRocket()
{
    bool done = false;

    for( int idx = 0; idx < BRK_NB_ROCKETS_SIMULTANEOUS_MAX && !done; idx++ )
      if( brkRocketX[ idx ] == -1 && brkPlayerRockets > 0 )
      {
          brkRocketX[ idx ] = (int)( brkPlayerX + ( brkPlayerWidth / 2 ) );
          brkRocketY[ idx ] = BRK_PLAYER_HEIGHT;
          brkPlayerRockets--;
          done = true;
      }
}

void brkShootLasers()
{
    int laser1 = -1;
    int laser2 = -1;

    for( int idx = 0; idx < BRK_NB_LASERS_SIMULTANEOUS_MAX && ( laser1 == -1 || laser2 == -1 ); idx++ )
    {
        if( brkLaserX[ idx ] == -1 && laser1 == -1 ) laser1 = idx;
        else if( brkLaserX[ idx ] == -1 && laser2 == -1 ) laser2 = idx;
    }

    if( laser1 > -1 && laser2 > -1 && brkPlayerLasers >= 2 )
    {
        brkLaserX[ laser1 ] = (int)( brkPlayerX + 2 );
        brkLaserY[ laser1 ] = BRK_PLAYER_HEIGHT;
        brkLaserX[ laser2 ] = (int)( brkPlayerX + brkPlayerWidth - 3 );
        brkLaserY[ laser2 ] = BRK_PLAYER_HEIGHT;
        gbPlayNoteChannel( 49, 1, 0 );

        brkPlayerLasers = brkPlayerLasers - 2;
        if( brkPlayerLasers < 0 ) brkPlayerLasers = 0;
    }
}

void brkMoveLasers()
{
    for( int idx = 0; idx < BRK_NB_LASERS_SIMULTANEOUS_MAX; idx++ )
      if( brkLaserX[ idx ] > -1 )
      {
          int hitX, hitY;

          brkLaserY[ idx ]--;
          brkIsThereABrickHere( brkLaserX[ idx ], (int)brkLaserY[ idx ], &hitX, &hitY );

          if( hitX > -1 )
          {
              brkBrickHit( hitX, hitY, false );
              brkLaserX[ idx ] = -1;
              gbPlayNoteChannel( 54, 1, 0 );
          }
          else if( brkLaserY[ idx ] < BRK_PLAY_AREA_TOP )
          {
              brkLaserX[ idx ] = -1;
              gbPlayNoteChannel( 52, 1, 0 );
          }
      }

    for( int idx = 0; idx < BRK_NB_LASERS_SIMULTANEOUS_MAX; idx++ )
      if( brkLaserX[ idx ] > -1 )
      {
          int bottom = (int)brkLaserY[ idx ] + 2;
          if( bottom > BRK_PLAYER_HEIGHT ) bottom = BRK_PLAYER_HEIGHT;

          gbDrawLine( brkLaserX[ idx ], (int)brkLaserY[ idx ], brkLaserX[ idx ], bottom );
      }
}

void brkMoveRockets()
{
    for( int idx = 0; idx < BRK_NB_ROCKETS_SIMULTANEOUS_MAX; idx++ )
      if( brkRocketX[ idx ] > -1 )
      {
          int hitX, hitY;

          brkRocketY[ idx ] = brkRocketY[ idx ]
                            - ( ( BRK_PLAY_AREA_BOTTOM - brkRocketY[ idx ] ) / 32 ) - 0.2;

          brkIsThereABrickHere( brkRocketX[ idx ], (int)brkRocketY[ idx ], &hitX, &hitY );

          if( hitX > -1 )
          {
              brkBrickExplosion( hitX, hitY, 2 );
              brkSpawnExplosion( brkRocketX[ idx ] + 1, (int)brkRocketY[ idx ] );
              brkRocketX[ idx ] = -1;
          }
          else if( brkRocketY[ idx ] < BRK_PLAY_AREA_TOP )
          {
              int col = ( brkRocketX[ idx ] - BRK_PLAY_AREA_LEFT ) / BRK_BRICK_WIDTH;

              brkBrickExplosion( col, 0, 2 );
              brkSpawnExplosion( brkRocketX[ idx ] + 1, (int)brkRocketY[ idx ] );
              brkRocketX[ idx ] = -1;
          }
      }

    for( int idx = 0; idx < BRK_NB_ROCKETS_SIMULTANEOUS_MAX; idx++ )
      if( brkRocketX[ idx ] > -1 )
        gbDrawBitmapRotated( brkRocketX[ idx ] - 1, (int)brkRocketY[ idx ],
                             brkRocket[ brkFrame ], 3, 0 ); // ROTCW, NOFLIP
}

void brkDisplayStatus( bool all )
{
    if( brkPlayerRockets > 0 || all )
    {
        gbCursorX = BRK_STATUS_POS_X;
        gbCursorY = BRK_STATUS_POS_Y;
        gbPrintString( "R: " );
        gbPrintNumber( brkPlayerRockets );
    }

    if( brkPlayerLasers > 0 || all )
    {
        gbCursorX = BRK_STATUS_POS_X;
        gbCursorY = BRK_STATUS_POS_Y + 6;
        gbPrintString( "L:" );

        if( brkPlayerLasers < 10 )
          gbPrintString( " " );

        gbPrintNumber( brkPlayerLasers );
    }
}

// ---------------------------------------------------------------------------
// Level handling and screens
// ---------------------------------------------------------------------------

void brkLoadLevel( int* table )
{
    for( int i = 0; i < 80; i++ )
      brkBricks[ i ] = table[ i ];
}

void brkInitGame()
{
    brkSpawnBall( 42, BRK_PLAYER_HEIGHT, 0.65, -0.65, 8, false );
    brkIsReady = 0;
}

// Real upstream's own single-character button-icon glyph, ASCII 21 - printed
// in font5x7 before switching back to font3x5.
void brkPrintButtonAIcon( int x, int y )
{
    gbSetFont( gbFont5x7 );
    gbDrawChar( 21, x, y );
    gbSetFont( gbFont3x5 );
}

void brkGameOver()
{
    brkDrawBorders();
    brkDrawBricks();

    gbFontSize = 2;
    gbCursorX = 7;
    gbCursorY = BRK_STATUS_POS_Y - 4;
    gbPrintString( "GAME OVER" );
    gbFontSize = 1;

    brkPrintButtonAIcon( 23, 41 );

    gbCursorX = 29;
    gbCursorY = 42;
    gbPrintString( "Extra Ball" );

    if( gbPressed( BTN_A ) )
    {
        brkInitGame();
        brkPlayerX = 22;
    }

    brkDrawExplosions();
}

void brkLevelCompleted()
{
    brkDrawBorders();
    brkDrawBricks();
    gbCursorX = 11;
    gbCursorY = BRK_STATUS_POS_Y - 4;
    gbPrintString( "A WINNER IS YOU!" );
}

void brkPauseMenu()
{
    gbCursorX = 28;
    gbCursorY = 12;
    gbPrintString( "BRICKS!" );
    gbCursorX = 29;
    gbCursorY = 22;
    gbPrintString( "PAUSED" );

    brkPrintButtonAIcon( 4, 37 );

    gbCursorX = 10;
    gbCursorY = 38;
    gbPrintString( "Title Screen" );

    brkDisplayStatus( true );
    brkDrawBorders();

    if( gbPressed( BTN_A ) )
      brkState = BRK_STATE_TITLE;
}

// Stands in for the blocking gb.titleScreen(F("Bricks! by Drakker..."), logo)
// call. Upstream's own logo really is a blank 64x1 placeholder, so the title
// text is all real hardware shows here too.
void brkTitleScreen()
{
    gbCursorX = 4;
    gbCursorY = 8;
    gbPrintString( "Bricks! by Drakker" );
    gbCursorX = 4;
    gbCursorY = 24;
    gbPrintString( "4 LEVELS TEST" );
    gbCursorX = 4;
    gbCursorY = 40;
    gbPrintString( "Press A" );

    if( gbPressed( BTN_A ) )
    {
        gbPlayOK();
        brkState = BRK_STATE_PLAY;
    }
}

void brkNextLevel()
{
    brkCurrentLevel++;

    if( brkCurrentLevel == 2 )      brkLoadLevel( brkLevel2 );
    else if( brkCurrentLevel == 3 ) brkLoadLevel( brkLevel3 );
    else if( brkCurrentLevel == 4 ) brkLoadLevel( brkLevel4 );

    for( int i = 0; i < BRK_NB_BALLS_MAX; i++ )
      brkBallX[ i ] = -1;

    // Upstream clears items by writing to .x, a field spawnItem() never
    // tests, and runs the loop two entries past the end of the array - the
    // write is kept (it really does nothing), the overrun is not.
    for( int i = 0; i < BRK_NB_ITEMS_MAX; i++ )
      brkItemX[ i ] = -1;

    for( int i = 0; i < BRK_NB_LASERS_SIMULTANEOUS_MAX; i++ )
      brkLaserX[ i ] = -1;

    for( int i = 0; i < BRK_NB_ROCKETS_SIMULTANEOUS_MAX; i++ )
      brkRocketX[ i ] = -1;

    for( int i = 0; i < BRK_NB_EXPLOSIONS_MAX; i++ )
      brkExplosionX[ i ] = -99;

    brkInitGame();
    brkPlayerX = 22;
    brkIsReady = 0;
}

// ---------------------------------------------------------------------------
// Entry points
// ---------------------------------------------------------------------------

void gameBricks_init()
{
    gbBegin();
    gbPickRandomSeed(); // no-op, see gamebuinoShim.h's own header comment
    gbSetFrameRate( 41 );
    gbSetFont( gbFont3x5 );
    gbFontSize = 1;

    brkPlayerX = 22;
    brkPlayerXvelo = 0;
    brkPlayerWidth = 16;
    brkPlayerLives = 3;
    brkPlayerMegaBall = 0;
    brkPlayerRockets = 0;
    brkPlayerLasers = 0;

    for( int i = 0; i < BRK_NB_BALLS_MAX; i++ )
    {
        brkBallX[ i ] = -1;
        brkBallY[ i ] = 0;
        brkBallXvelo[ i ] = 0;
        brkBallYvelo[ i ] = 0;
        brkBallAngle[ i ] = 0;
        brkBallExplode[ i ] = 0;
    }

    for( int i = 0; i < BRK_NB_ITEMS_MAX; i++ )
    {
        brkItemType[ i ] = -1;
        brkItemX[ i ] = 0;
        brkItemY[ i ] = 0;
    }

    for( int i = 0; i < BRK_NB_LASERS_SIMULTANEOUS_MAX; i++ )
    {
        brkLaserX[ i ] = -1;
        brkLaserY[ i ] = -1;
    }

    for( int i = 0; i < BRK_NB_ROCKETS_SIMULTANEOUS_MAX; i++ )
    {
        brkRocketX[ i ] = -1;
        brkRocketY[ i ] = -1;
    }

    for( int i = 0; i < BRK_NB_EXPLOSIONS_MAX; i++ )
    {
        brkExplosionX[ i ] = -99;
        brkExplosionY[ i ] = 0;
        brkExplosionDuration[ i ] = 0;
    }

    brkFrame = 0;
    brkPause = false;
    brkCurrentLevel = 1;
    brkBallAcceleration = 0.00001;
    brkIsReady = 0;
    brkState = BRK_STATE_TITLE;

    brkInitSprites();
    brkLoadLevel( brkLevel1 );
    brkInitGame();
}

void gameBricks_update()
{
    if( !gbUpdate() ) return;

    if( brkState == BRK_STATE_TITLE )
    {
        brkTitleScreen();
        gbRenderFrame();
        return;
    }

    brkFrame = gbFrameCount & 1;

    if( gbPressed( BTN_C ) )
      brkPause = !brkPause;

    if( !brkPause && brkCountBalls() > 0 && brkCountBricks() > 0 )
    {
        if( brkFrame == 1 )
          brkDisplayStatus( false );

        if( gbPressed( BTN_B ) ) brkShootLasers();
        if( gbPressed( BTN_A ) ) brkLaunchRocket();

        if( gbPressed( BTN_UP ) )
        {
            brkPlayerWidth++;
            if( brkPlayerWidth > BRK_BAR_MAX_WIDTH ) brkPlayerWidth = BRK_BAR_MAX_WIDTH;
        }

        if( gbPressed( BTN_DOWN ) )
        {
            brkPlayerWidth--;
            if( brkPlayerWidth < BRK_BAR_MIN_WIDTH ) brkPlayerWidth = BRK_BAR_MIN_WIDTH;
        }

        brkMoveItems();
        brkMoveLasers();
        brkMoveRockets();
        brkDrawBorders();
        brkDrawBricks();
        brkMoveBalls();
        brkMovePlayer();
        brkDrawExplosions();

        if( brkCountBalls() < 1 )
        {
            brkSpawnExplosion( (int)brkPlayerX, BRK_PLAYER_HEIGHT + 2 );
            brkSpawnExplosion( (int)brkPlayerX + brkPlayerWidth, BRK_PLAYER_HEIGHT + 1 );
            brkSpawnExplosion( (int)brkPlayerX + ( brkPlayerWidth / 2 ), BRK_PLAYER_HEIGHT + 2 );
        }
    }
    else if( brkPause )
    {
        brkPauseMenu();
    }
    else if( brkCountBricks() == 0 )
    {
        if( brkCurrentLevel < 4 )
        {
            brkLevelCompleted();

            brkPrintButtonAIcon( 21, 41 );

            gbCursorX = 27;
            gbCursorY = 42;
            gbPrintString( "Next Level" );

            if( gbPressed( BTN_A ) )
              brkIsReady = 1;

            if( brkIsReady == 1 )
              brkNextLevel();
        }
        else
        {
            brkLevelCompleted();
            gbCursorX = 9;
            gbCursorY = BRK_STATUS_POS_Y + 6;
            gbPrintString( "The End (for now)" );
        }
    }
    else
    {
        brkGameOver();
    }

    gbRenderFrame();
}
