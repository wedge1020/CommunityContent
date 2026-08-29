// Bomberman (by "Limited" - JohnAkerman, no license specified), v2.0a. A
// single-screen Bomberman: one player and one enemy share a 21x12 tile maze
// of solid walls (drawn filled) and breakable blocks (drawn as outlines).
// Button A drops a bomb, which detonates five seconds later and clears any
// breakable block orthogonally adjacent to it - killing whichever of the two
// happens to be standing in the blast. The enemy homes in whenever it gets
// within 30 units and chips 10 health off the player on contact; the HUD
// tracks deaths, kills, the current enemy distance and health.
//
// This is a genuinely different codebase from the already-shipped
// `gameBomber.c` (Clement83's own two-player Bomber), sharing only the genre
// - hence the separate `bman` prefix alongside that file's own `bomb`.
//
// Every real `gb.x.y(...)` call site is mechanically rewritten to a plain
// `gbY(...)` function call (see gamePong.c's own header comment for why -
// this dialect has no classes/methods/operator overloading). Every global
// symbol got a `bman`-prefixed name (this cartridge has no linker - every
// ported game shares one flat global namespace).
//
// CLASS HIERARCHY -> PLAIN STRUCTS AND FREE FUNCTIONS: upstream is built on a
// real `Entity` base class with `Player` and `Enemy` deriving from it, plus
// standalone `Maze` and `Bomb` classes. This dialect has no classes or
// inheritance, so each became a plain struct plus free functions taking an
// explicit pointer - the same flattening this project already applies to
// gameSuperSpaceShooter.c and gameSolitaire.c. `Entity`'s own shared movement
// methods became `bmanMoveLeft(entity)`-style functions the player and enemy
// both call, preserving the real inheritance behaviour exactly.
//
// `Player` overriding `entitySpawn()` to reset ONLY health, never position,
// is real and preserved: respawning after a death leaves the player standing
// exactly where they died, while the enemy really does get relocated.
//
// TIMING: upstream times its bombs with `millis()` against a literal 5000ms
// fuse. This shim has no wall-clock source, so each bomb stores the
// `gbFrameCount` it was placed at and detonates after 105 ticks - exactly
// five seconds at upstream's own `gb.setFrameRate(21)`.
//
// THE MAZE TABLE IS DECLARED 21 WIDE BUT EVERY ROW LISTS ONLY 20 VALUES.
// That is real, and preserved byte-for-byte: C zero-fills the missing entry,
// so column 20 is empty floor along the entire height of the maze. Since
// column 19 is a solid wall on every row, that empty column sits permanently
// behind the right-hand wall and can never be reached - which is also what
// keeps the bomb-blast checks below from ever indexing past the row.
//
// REAL UPSTREAM QUIRKS, PRESERVED EXACTLY:
// - `if (gb.frameCount % 25)` gates both the enemy's own think/damage step and
//   the bomb's own drawing. That condition is TRUE for 24 ticks out of every
//   25 (it tests non-divisibility, not divisibility), so it reads like a
//   throttle but is really a one-tick-in-25 skip: the enemy re-evaluates
//   almost every tick, and a bomb briefly blinks out once a second.
// - The health bar is drawn with `fillRect(..., 2, -hBar)` - a negative
//   height, which real `Display::fillRect()`'s own `for(i=0; i<h; i++)` loop
//   never enters, so the bar is invisible on real hardware. This shim's own
//   gbFillRect() is the same direct port and behaves identically, so the call
//   is left exactly as upstream wrote it.
// - `renderEdges()` passes `LCDHEIGHT-1` as a rectangle HEIGHT where a
//   bottom coordinate was clearly meant, drawing edges taller than intended.
//   Only reachable on the death screen, and harmless - the draw clips.
// - `doDamage()`'s own `if (health <= (health - val))` underflow guard can
//   never fire: both sides promote to int, so it compares 100 <= 90. Health
//   only ever moves in steps of 10 from 100, so it lands on exactly 0 anyway
//   and the dead path is reached correctly.
// - Button B toggles a debug screen mid-game, exactly as upstream ships it.
//   Its own free-RAM and CPU-load readouts have no equivalent on this
//   platform and are the only two lines dropped; player position and the
//   version string are kept.
//
// BOUNDED WHERE THIS PLATFORM CANNOT ABSORB AN OVERRUN (both documented
// rather than silently changed):
// - `entitySpawn()` retries by calling ITSELF recursively until it lands on
//   an empty tile. On real AVR a deep run just burns stack it happens to have
//   spare; here an unbounded recursion is a genuine hang risk, so it became a
//   bounded loop of 200 attempts falling back to a known-empty tile. The maze
//   is mostly open floor, so a real run finds a tile within a handful of
//   tries.
// - `bombExplode()` probes `mazeLevel[tileY+-1][tileX+-1]` with no bounds
//   check of its own (its `isBreakable()` even carries a "TODO: check bounds"
//   comment upstream). Tracing real reachable positions shows the walls keep
//   every probe in range, but an out-of-range read here would pull an
//   unrelated global rather than harmless adjacent SRAM, so `bmanIsBreakable()`
//   range-checks its own indices and returns false outside the maze.
//   `isBreakable()` also falls off the end of its `switch` for any value
//   other than 0/1/2, returning whatever happened to be in the return
//   register; it returns false here.

#include "../gamebuinoShim.h"

#define BMAN_BOMB_MAX 3
#define BMAN_MAZE_W 21
#define BMAN_MAZE_H 12
#define BMAN_WALL_SIZE_X 4
#define BMAN_WALL_SIZE_Y 4

// Five seconds at upstream's own gb.setFrameRate(21) - see the header comment.
#define BMAN_BOMB_FUSE_TICKS 105

// ---------------------------------------------------------------------------
// Upstream's own Entity/Player/Enemy/Bomb classes, flattened to plain structs
// ---------------------------------------------------------------------------

struct BmanEntity
{
    bool active;
    int x, y, h, w, vy, vx;
};

struct BmanPlayer
{
    BmanEntity e;
    int playervxStart, playervyStart;
    int deaths, kills, health;
};

struct BmanEnemy
{
    BmanEntity e;
    int dist;
    int enemyMode;
};

struct BmanBomb
{
    int x, y;
    bool active;
    int startFrame;
};

BmanPlayer bmanPlayer;
BmanEnemy bmanEnemy;
BmanBomb[BMAN_BOMB_MAX] bmanBombs;

bool bmanDebug;
int bmanGameState; // 0 = playing, 1 = dead
bool bmanShowingTitle;

// Real upstream's own 21x12 maze (Maze.cpp). Every row of the real
// initializer lists only 20 values for a table declared 21 wide, so C
// zero-fills column 20 - reproduced exactly here, flattened to one
// dimension since this dialect indexes a 2D table less directly.
// Tile values: 0 = floor, 1 = solid wall, 2 = breakable block.
int[252] bmanMazeInitial =
{
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0,
    1, 0, 0, 0, 0, 0, 0, 0, 0, 2, 2, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0,
    1, 0, 0, 0, 0, 0, 0, 0, 0, 2, 2, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0,
    1, 0, 0, 0, 0, 0, 0, 0, 0, 2, 2, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0,
    1, 0, 0, 1, 0, 0, 1, 0, 0, 2, 2, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0,
    1, 0, 0, 0, 0, 0, 0, 0, 0, 2, 2, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0,
    1, 0, 0, 0, 0, 0, 0, 0, 0, 2, 2, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0,
    1, 0, 0, 1, 0, 0, 1, 0, 0, 2, 2, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0,
    1, 0, 0, 0, 0, 0, 0, 0, 0, 2, 2, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0,
    1, 0, 0, 0, 0, 0, 0, 0, 0, 2, 2, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0,
    1, 0, 0, 0, 0, 0, 0, 0, 0, 2, 2, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0
};

// Working copy - bombs really do clear breakable blocks permanently, so the
// table is mutated during play and restored on each fresh launch (one
// cartridge session here runs many games in sequence, which real hardware
// never had to account for).
int[252] bmanMazeLevel;

void bmanResetMaze()
{
    for( int i = 0; i < 252; i++ )
      bmanMazeLevel[ i ] = bmanMazeInitial[ i ];
}

// enemySprite: 4x4
int[6] bmanEnemySprite = { 0x04, 0x04, 0x60, 0xF0, 0xF0, 0x60 };

// Real upstream's own 64x30 title logo.
int[242] bmanLogoBitmap =
{
    0x40, 0x1E, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x18,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x7E,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x1F, 0xE1, 0xE2,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x7F, 0xF9, 0xC3,
    0x00, 0x00, 0x00, 0x00, 0x01, 0xFF, 0xFF, 0x83,
    0x00, 0x00, 0x00, 0x00, 0x03, 0xFF, 0xFF, 0x80,
    0x00, 0x00, 0x00, 0x00, 0x07, 0xFF, 0xFF, 0x80,
    0x00, 0x00, 0x00, 0x00, 0x0F, 0xFF, 0xFF, 0xC0,
    0x00, 0x00, 0x00, 0x00, 0x0F, 0xE0, 0x3F, 0xC0,
    0x00, 0x00, 0x00, 0x00, 0x1F, 0xE7, 0x8F, 0xE0,
    0x00, 0x00, 0x00, 0x00, 0x1F, 0xE7, 0xCF, 0xE0,
    0x00, 0x00, 0x00, 0x00, 0x1F, 0xE7, 0xCF, 0xE0,
    0x00, 0x00, 0x00, 0x00, 0x3F, 0xE7, 0xCF, 0xF0,
    0x00, 0x00, 0x00, 0x00, 0x3F, 0xE7, 0x9F, 0xF0,
    0x00, 0x00, 0x00, 0x00, 0x3F, 0xE0, 0x3F, 0xF0,
    0x00, 0x00, 0x00, 0x00, 0x3F, 0xE7, 0x8F, 0xF0,
    0x00, 0x00, 0x00, 0x00, 0x3F, 0xE7, 0xE7, 0xF0,
    0x00, 0x00, 0x00, 0x00, 0x3F, 0xE7, 0xE7, 0xF0,
    0x00, 0x00, 0x00, 0x00, 0x3F, 0xE7, 0xE7, 0xF0,
    0x00, 0x00, 0x00, 0x00, 0x1F, 0xE7, 0xC7, 0xE0,
    0x00, 0x00, 0x00, 0x00, 0x1F, 0xE7, 0x8F, 0xE0,
    0x00, 0x00, 0x00, 0x00, 0x1F, 0xE0, 0x3F, 0xE0,
    0x00, 0x00, 0x00, 0x00, 0x0F, 0xFF, 0xFF, 0xC0,
    0x00, 0x00, 0x00, 0x00, 0x0F, 0xFF, 0xFF, 0xC0,
    0x00, 0x00, 0x00, 0x00, 0x07, 0xFF, 0xFF, 0x80,
    0x00, 0x00, 0x00, 0x00, 0x03, 0xFF, 0xFF, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x01, 0xFF, 0xFE, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x7F, 0xF8, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x1F, 0xE0, 0x00,
    0x00, 0x00
};

// ---------------------------------------------------------------------------
// Maze - direct port of upstream's own Maze.cpp
// ---------------------------------------------------------------------------

int bmanToTileX( int x ) { return x / BMAN_WALL_SIZE_X; }
int bmanToTileY( int y ) { return y / BMAN_WALL_SIZE_Y; }

bool bmanIsTileEmpty( int x, int y )
{
    if( x < 0 || x >= BMAN_MAZE_W || y < 0 || y >= BMAN_MAZE_H )
      return false;

    return bmanMazeLevel[ y * BMAN_MAZE_W + x ] == 0;
}

void bmanRenderEdges()
{
    gbFillRect( 0, 0, LCDWIDTH - 4, 4 );                    // Top
    gbFillRect( 0, LCDHEIGHT - 4, LCDWIDTH - 4, LCDHEIGHT - 1 ); // Bottom
    gbFillRect( 0, 1, 4, LCDHEIGHT - 1 );                   // Left
    gbFillRect( LCDWIDTH - 8, 0, 4, LCDHEIGHT - 1 );        // Right
}

void bmanRenderMaze()
{
    for( int row = 0; row < BMAN_MAZE_H; row++ )
      for( int col = 0; col < BMAN_MAZE_W; col++ )
      {
          int tile = bmanMazeLevel[ row * BMAN_MAZE_W + col ];

          if( tile == 1 )
            gbFillRect( col * 4, row * 4, 4, 4 );
          else if( tile == 2 )
            gbDrawRect( col * 4, row * 4, 4, 4 );
      }
}

bool bmanCheckWallCollision( int xIn, int yIn )
{
    // Get elements around the player
    int tileX = bmanToTileX( xIn ) - 2;
    int tileY = bmanToTileY( yIn ) - 2;
    int tileXMax = tileX + 4;
    int tileYMax = tileY + 4;

    if( tileX < 0 ) tileX = 0;
    if( tileY < 0 ) tileY = 0;

    if( tileXMax > BMAN_MAZE_W ) tileXMax = BMAN_MAZE_W;
    if( tileYMax > BMAN_MAZE_H ) tileYMax = BMAN_MAZE_H;

    for( int row = tileY; row < tileYMax; row++ )
      for( int col = tileX; col < tileXMax; col++ )
      {
          if( bmanMazeLevel[ row * BMAN_MAZE_W + col ] == 0 )
            continue; // If empty skip

          if( gbCollideRectRect( xIn, yIn, bmanPlayer.e.w, bmanPlayer.e.h,
                                 col * BMAN_WALL_SIZE_X, row * BMAN_WALL_SIZE_Y,
                                 BMAN_WALL_SIZE_X, BMAN_WALL_SIZE_Y ) )
            return true;
      }

    return false;
}

// Upstream's own isBreakable() takes (y, x) in that order and carries its own
// "TODO: check bounds" comment - the bounds check is supplied here.
bool bmanIsBreakable( int y, int x )
{
    if( x < 0 || x >= BMAN_MAZE_W || y < 0 || y >= BMAN_MAZE_H )
      return false;

    return bmanMazeLevel[ y * BMAN_MAZE_W + x ] == 2;
}

// ---------------------------------------------------------------------------
// Entity - direct port of upstream's own Entity.cpp, shared by both actors
// ---------------------------------------------------------------------------

void bmanEntitySpawn( BmanEntity* e )
{
    // Upstream recurses until it lands on an empty tile - bounded here, see
    // the header comment.
    for( int attempt = 0; attempt < 200; attempt++ )
    {
        int randomX = 1 + arand( 19 );
        int randomY = 1 + arand( 11 );

        if( bmanIsTileEmpty( randomX, randomY ) )
        {
            e->x = randomX * 4;
            e->y = randomY * 4;
            return;
        }
    }

    // Fallback: tile (1,1) is open floor in the shipped maze.
    e->x = 4;
    e->y = 4;
}

int bmanGetDistance( int x1, int y1, int x2, int y2 )
{
    int dx = x1 - x2;
    int dy = y1 - y2;

    // Always non-negative, so this never asks sqrt() for a negative value.
    return (int)sqrt( (float)( dx * dx + dy * dy ) );
}

void bmanMoveLeft( BmanEntity* e )
{
    e->x = e->x - e->vx;
    if( e->x < 0 ) e->x = 0;

    if( bmanCheckWallCollision( e->x, e->y ) )
    {
        e->x = e->x + e->vx;
        if( e->x < 0 ) e->x = 0;
    }
}

void bmanMoveRight( BmanEntity* e )
{
    e->x = e->x + e->vx;
    if( e->x > LCDWIDTH - e->w ) e->x = LCDWIDTH - e->w;

    if( bmanCheckWallCollision( e->x, e->y ) )
    {
        e->x = e->x - e->vx;
        if( e->x > LCDWIDTH - e->w ) e->x = LCDWIDTH - e->w;
    }
}

void bmanMoveUp( BmanEntity* e )
{
    e->y = e->y - e->vy;
    if( e->y < 0 ) e->y = 0;

    if( bmanCheckWallCollision( e->x, e->y ) )
    {
        e->y = e->y + e->vy;
        if( e->y < 0 ) e->y = 0;
    }
}

void bmanMoveDown( BmanEntity* e )
{
    e->y = e->y + e->vy;
    if( e->y > LCDHEIGHT - e->h ) e->y = LCDHEIGHT - e->h;

    if( bmanCheckWallCollision( e->x, e->y ) )
    {
        e->y = e->y - e->vy;
        if( e->y > LCDHEIGHT - e->h ) e->y = LCDHEIGHT - e->h;
    }
}

// ---------------------------------------------------------------------------
// Player - direct port of upstream's own Player.cpp
// ---------------------------------------------------------------------------

void bmanPlayerInit()
{
    bmanPlayer.e.x = 4;
    bmanPlayer.e.y = 4;
    bmanPlayer.e.w = 4;
    bmanPlayer.e.h = 4;
    bmanPlayer.e.vx = 2;
    bmanPlayer.playervxStart = bmanPlayer.e.vx;
    bmanPlayer.e.vy = 2;
    bmanPlayer.playervyStart = bmanPlayer.e.vy;
    bmanPlayer.e.active = true;
    bmanPlayer.deaths = 0;
    bmanPlayer.kills = 0;
    bmanPlayer.health = 100;
}

// Upstream's own Player::entitySpawn() override really does reset health
// only, deliberately leaving the player where they died.
void bmanPlayerSpawn()
{
    bmanPlayer.health = 100;
}

void bmanRenderPlayer()
{
    gbFillRect( bmanPlayer.e.x, bmanPlayer.e.y, bmanPlayer.e.w, bmanPlayer.e.h );
    gbSetColor( 0 ); // WHITE
    gbDrawPixel( bmanPlayer.e.x + 1, bmanPlayer.e.y + 1 );
    gbDrawPixel( bmanPlayer.e.x + 2, bmanPlayer.e.y + 2 );
    gbSetColor( 1 ); // BLACK
}

void bmanSetDead()
{
    gbPlayCancel();
    bmanGameState = 1;
    bmanPlayer.deaths++;
}

void bmanDoDamage( int val )
{
    if( bmanPlayer.health <= ( bmanPlayer.health - val ) )
      bmanPlayer.health = 0;
    else
      bmanPlayer.health = bmanPlayer.health - val;

    if( bmanPlayer.health == 0 )
      bmanSetDead();
}

// ---------------------------------------------------------------------------
// Enemy - direct port of upstream's own Enemy.cpp
// ---------------------------------------------------------------------------

void bmanEnemyInit()
{
    bmanEnemy.e.w = 4;
    bmanEnemy.e.h = 4;
    bmanEnemy.e.vx = 1;
    bmanEnemy.e.vy = 1;
    bmanEnemy.e.active = true;
    bmanEnemy.dist = 0;
    bmanEnemy.enemyMode = 0;
    bmanEntitySpawn( &bmanEnemy.e );
}

void bmanRenderEnemy()
{
    gbDrawBitmap( bmanEnemy.e.x, bmanEnemy.e.y, bmanEnemySprite );
}

void bmanUpdateEnemy()
{
    bmanEnemy.dist = bmanGetDistance( bmanEnemy.e.x, bmanEnemy.e.y,
                                      bmanPlayer.e.x, bmanPlayer.e.y );

    // True for 24 ticks in every 25 - see the header comment.
    if( gbFrameCount % 25 )
    {
        if( bmanEnemy.dist < 30 )
          bmanEnemy.enemyMode = 1; // Seek
        else
          bmanEnemy.enemyMode = 0; // Idle

        if( bmanEnemy.dist <= 4 )
          bmanDoDamage( 10 );
    }

    if( bmanEnemy.enemyMode == 1 )
    {
        // Basic seek
        if( bmanPlayer.e.x > bmanEnemy.e.x )
          bmanMoveRight( &bmanEnemy.e );
        else if( bmanPlayer.e.x < bmanEnemy.e.x )
          bmanMoveLeft( &bmanEnemy.e );

        if( bmanPlayer.e.y > bmanEnemy.e.y )
          bmanMoveDown( &bmanEnemy.e );
        else if( bmanPlayer.e.y < bmanEnemy.e.y )
          bmanMoveUp( &bmanEnemy.e );
    }
}

void bmanEnemyDead()
{
    gbPlayCancel();
    bmanPlayer.kills++;
    bmanEntitySpawn( &bmanEnemy.e );
}

// ---------------------------------------------------------------------------
// Bombs - direct port of upstream's own Bomb.cpp plus Bomber.ino's own helpers
// ---------------------------------------------------------------------------

void bmanBombExplode( int x, int y )
{
    int tileX = bmanToTileX( x );
    int tileY = bmanToTileY( y );
    int tilePlayerX = bmanToTileX( bmanPlayer.e.x );
    int tilePlayerY = bmanToTileY( bmanPlayer.e.y );
    int tileEnemyX;
    int tileEnemyY;

    gbPlayOK();

    // Check player dead
    if( ( tilePlayerX == tileX + 1 && tilePlayerY == tileY )
     || ( tilePlayerX == tileX - 1 && tilePlayerY == tileY )
     || ( tilePlayerX == tileX && tilePlayerY == tileY - 1 )
     || ( tilePlayerX == tileX && tilePlayerY == tileY + 1 )
     || ( tilePlayerX == tileX && tilePlayerY == tileY ) )
    {
        bmanSetDead();
        return;
    }

    tileEnemyX = bmanToTileX( bmanEnemy.e.x );
    tileEnemyY = bmanToTileY( bmanEnemy.e.y );

    if( ( tileEnemyX == tileX + 1 && tileEnemyY == tileY )
     || ( tileEnemyX == tileX - 1 && tileEnemyY == tileY )
     || ( tileEnemyX == tileX && tileEnemyY == tileY - 1 )
     || ( tileEnemyX == tileX && tileEnemyY == tileY + 1 )
     || ( tileEnemyX == tileX && tileEnemyY == tileY ) )
    {
        bmanEnemyDead();
        return;
    }

    // Check Right
    if( bmanIsBreakable( tileY, tileX + 1 ) )
    {
        bmanMazeLevel[ tileY * BMAN_MAZE_W + ( tileX + 1 ) ] = 0;
        gbPlayTick();
    }

    // Check left
    if( bmanIsBreakable( tileY, tileX - 1 ) )
    {
        bmanMazeLevel[ tileY * BMAN_MAZE_W + ( tileX - 1 ) ] = 0;
        gbPlayTick();
    }

    // Check Up
    if( bmanIsBreakable( tileY - 1, tileX ) )
    {
        bmanMazeLevel[ ( tileY - 1 ) * BMAN_MAZE_W + tileX ] = 0;
        gbPlayTick();
    }

    // Check Down
    if( bmanIsBreakable( tileY + 1, tileX ) )
    {
        bmanMazeLevel[ ( tileY + 1 ) * BMAN_MAZE_W + tileX ] = 0;
        gbPlayTick();
    }
}

void bmanRenderBombs()
{
    for( int i = 0; i < BMAN_BOMB_MAX; i++ )
    {
        if( !bmanBombs[ i ].active )
          continue;

        // Same 24-in-25 gate as the enemy's own think step.
        if( gbFrameCount % 25 )
        {
            gbFillRect( bmanBombs[ i ].x + 1, bmanBombs[ i ].y, 2, 1 ); // top
            gbFillRect( bmanBombs[ i ].x, bmanBombs[ i ].y + 1, 1, 2 ); // left
            gbFillRect( bmanBombs[ i ].x + 1, bmanBombs[ i ].y + 3, 2, 1 ); // bottom
            gbFillRect( bmanBombs[ i ].x + 3, bmanBombs[ i ].y + 1, 1, 2 ); // right
        }
    }
}

void bmanUpdateBombs()
{
    for( int i = 0; i < BMAN_BOMB_MAX; i++ )
    {
        if( !bmanBombs[ i ].active )
          continue;

        if( ( gbFrameCount - bmanBombs[ i ].startFrame ) >= BMAN_BOMB_FUSE_TICKS )
        {
            bmanBombs[ i ].active = false;
            bmanBombExplode( bmanBombs[ i ].x, bmanBombs[ i ].y );
        }
    }
}

void bmanSetBomb( int x, int y )
{
    for( int i = 0; i < BMAN_BOMB_MAX; i++ )
    {
        if( bmanBombs[ i ].active )
          continue;

        bmanBombs[ i ].active = true;
        bmanBombs[ i ].x = ( x / 4 ) * 4;
        bmanBombs[ i ].y = ( y / 4 ) * 4;
        bmanBombs[ i ].startFrame = gbFrameCount;
        return;
    }
}

// ---------------------------------------------------------------------------
// Screens and input - direct port of upstream's own Bomber.ino
// ---------------------------------------------------------------------------

void bmanDeadMenu()
{
    gbCursorX = gbFontWidth + 8;
    gbCursorY = 8;
    gbPrintString( "You Died\n" );
    gbCursorX = gbFontWidth + 8;
    gbCursorY = 16;
    gbPrintString( "Press A to\n" );
    gbCursorX = gbFontWidth + 8;
    gbCursorY = 24;
    gbPrintString( "respawn\n" );
}

void bmanDebugRender()
{
    gbPrintString( "\nDebug Bomberman\n" );
    gbPrintString( "Player X:" );
    gbPrintNumber( bmanPlayer.e.x );
    gbPrintString( "\nPlayer Y:" );
    gbPrintNumber( bmanPlayer.e.y );
    gbPrintString( "\nVersion: 2.0a" );
}

void bmanRenderHud()
{
    int hBar;

    gbCursorX = LCDWIDTH - gbFontWidth + 1;
    gbCursorY = 8;
    gbPrintNumber( bmanPlayer.deaths );

    gbCursorX = LCDWIDTH - gbFontWidth + 1;
    gbCursorY = 16;
    gbPrintNumber( bmanPlayer.kills );

    gbCursorX = LCDWIDTH - gbFontWidth + 1 - 16;
    gbCursorY = 35;
    gbPrintNumber( bmanEnemy.dist );

    gbCursorX = LCDWIDTH - gbFontWidth + 1 - 66;
    gbCursorY = 35;
    gbPrintNumber( bmanPlayer.health );

    // Health bar. The height really is negated upstream, so nothing is drawn -
    // see the header comment.
    hBar = bmanPlayer.health / 7;
    if( hBar <= 0 )
      hBar = 0;

    gbFillRect( LCDWIDTH - gbFontWidth + 1, 40, 2, -hBar );
}

void bmanHandleInput()
{
    if( gbPressed( BTN_B ) )
      bmanDebug = !bmanDebug;

    if( gbPressed( BTN_C ) )
    {
        bmanShowingTitle = true;
        return;
    }

    if( bmanGameState == 0 ) // Playing
    {
        if( gbRepeat( BTN_LEFT, 1 ) )
          bmanMoveLeft( &bmanPlayer.e );
        else if( gbRepeat( BTN_RIGHT, 1 ) )
          bmanMoveRight( &bmanPlayer.e );
        else if( gbRepeat( BTN_UP, 1 ) )
          bmanMoveUp( &bmanPlayer.e );
        else if( gbRepeat( BTN_DOWN, 1 ) )
          bmanMoveDown( &bmanPlayer.e );

        if( gbPressed( BTN_A ) && !bmanDebug )
          bmanSetBomb( bmanPlayer.e.x + ( bmanPlayer.e.w / 2 ),
                       bmanPlayer.e.y + ( bmanPlayer.e.h / 2 ) );
    }
    else if( bmanGameState == 1 )
    {
        if( gbPressed( BTN_A ) )
        {
            // Respawn player
            bmanEntitySpawn( &bmanEnemy.e );
            bmanPlayerSpawn();
            bmanPlayer.e.active = true;
            bmanGameState = 0;
        }
    }
}

// Stands in for the blocking gb.titleScreen(F("Bomberman by Limited"), logo)
// call upstream makes from setup() and again on every Button C press. Real
// titleScreen() returns only on Button A, resuming whatever was going on
// underneath it - reproduced here with a flag rather than a game state.
void bmanTitleScreen()
{
    gbDrawBitmap( ( LCDWIDTH - 64 ) / 2, 0, bmanLogoBitmap );

    gbCursorX = 0;
    gbCursorY = 31;
    gbPrintString( "Bomberman by\nLimited\nPress A" );

    if( gbPressed( BTN_A ) )
    {
        gbPlayOK();
        bmanShowingTitle = false;
    }
}

void gameBomberman_init()
{
    gbBegin();
    gbPickRandomSeed(); // no-op, see gamebuinoShim.h's own header comment
    gbSetFrameRate( 21 );
    gbFontSize = 1;
    gbSetFont( gbFont3x5 );

    bmanDebug = false;
    bmanGameState = 0;
    bmanShowingTitle = true;

    bmanResetMaze();
    bmanPlayerInit();
    bmanEnemyInit();

    for( int i = 0; i < BMAN_BOMB_MAX; i++ )
      bmanBombs[ i ].active = false;
}

void gameBomberman_update()
{
    if( !gbUpdate() ) return;

    if( bmanShowingTitle )
    {
        bmanTitleScreen();
        gbRenderFrame();
        return;
    }

    bmanHandleInput();

    if( bmanDebug )
    {
        bmanDebugRender();
        gbRenderFrame();
        return;
    }

    if( bmanGameState == 0 )
    {
        bmanUpdateEnemy();
        bmanUpdateBombs();

        bmanRenderMaze();
        bmanRenderPlayer();
        bmanRenderEnemy();
        bmanRenderBombs();
        bmanRenderHud();
    }
    else if( bmanGameState == 1 ) // Dead
    {
        bmanRenderEdges();
        bmanDeadMenu();
        bmanRenderHud();
    }

    gbRenderFrame();
}
