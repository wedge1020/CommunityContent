// Wolfenduino 3D (jhhoward, no license specified) - a genuine
// Wolfenstein-3D-style raycaster for the Gamebuino Classic, and by a wide
// margin the most technically involved game in this cartridge: a portal-free
// cell renderer with textured walls, perspective-projected sprites, a depth
// (w) buffer, sliding doors, secret push-walls, patrolling guards with
// line-of-sight AI, four weapons, collectable items and a ten-level map set.
//
// Every real `gb.x.y(...)` call site is mechanically rewritten to a plain
// `gbY(...)` function call (see gamePong.c's own header comment for why -
// this dialect has no classes/methods/operator overloading). Every global
// symbol got a `wolf`-prefixed name (this cartridge has no linker - every
// ported game shares one flat global namespace).
//
// CLASS HIERARCHY -> PLAIN GLOBALS AND STRUCT ARRAYS: upstream is nine real
// C++ classes (`Engine`, `Renderer`, `Player`, `Map`, `Menu`, `Actor`,
// `Door`, `Item`, `Vector2`) plus a `PlatformBase`. Five of them are
// single-instance, so their members became plain prefixed globals; `Actor`,
// `Door` and `Item` became struct arrays. `Vector2` is unused by the
// Gamebuino build and was dropped. Upstream's own bitfield structs
// (`Actor::flags`, `Player::weapon`, and the `union` overlaying
// `Player::inventory` with `inventoryFlags`) became plain int fields, with
// the union's own "clear every inventory flag at once" use reproduced by
// clearing each field.
//
// THE SD-CARD PAYLOAD IS BAKED IN: real hardware streams its map data from
// `WOLF3D.DAT` on an SD card through petit-FatFs, sixteen cells at a time,
// because 160KB could never fit in an ATmega328's flash. That whole file is
// embedded here instead (`gameWolfenduinoData.h`, generated verbatim from
// the real `wolf3d.dat` by script - never hand-transcribed), packed four
// bytes per 32-bit word and unpacked by `wolfMapByte()`. The streaming
// window, the 16x16 buffer and the row/column dual layout are all kept
// exactly as upstream wrote them, so the map is read in the same order and
// in the same chunks - only the underlying byte source changed. Every other
// asset (wall textures, weapon/guard/item/decoration sprites, the font, the
// trig table and the five sound patterns) was already a baked-in PROGMEM
// table upstream and was converted byte-for-byte the same way.
//
// TWO DIALECT HAZARDS THAT WOULD SILENTLY WRECK A RAYCASTER, BOTH HANDLED
// EXPLICITLY:
//
//  1. THIS DIALECT'S `>>` IS A *LOGICAL* SHIFT (zero-fill), not arithmetic
//     (VIRCON32_C_DIALECT.md section 6). Wolfenduino's entire fixed-point
//     layer is built on `FIXED_TO_INT(x) ((x) >> 7)` and
//     `WORLD_TO_CELL(x) ((x) >> 5)` applied to genuinely signed values -
//     every view-space coordinate behind or left of the camera is negative.
//     Ported naively, those shifts turn small negatives into huge positives
//     and the projection collapses. `WOLF_ASR()` reproduces a real
//     sign-extending shift, and every shift that can see a negative goes
//     through it. Shifts on provably unsigned data (the random generator,
//     the bit-pair reader, texture bytes) use plain `>>`, exactly as
//     upstream does.
//
//  2. REAL AVR `int` IS 16 BITS AND THIS DIALECT'S IS 32. Upstream leans on
//     that width: view-space coordinates, wall widths and interpolation
//     errors are all `int16_t`, and several of them genuinely overflow and
//     wrap on real hardware. `WOLF_I16()`/`WOLF_I8()` reproduce that
//     narrowing at each point upstream assigns to a narrow type, so the
//     arithmetic wraps where the real cartridge wraps.
//
// THE ONE PLACE THIS PLATFORM CANNOT FOLLOW UPSTREAM: `shouldShootPlayer()`
// computes `16 / getPlayerCellDistance()`, and that distance is genuinely 0
// whenever a guard stands in the player's own cell - which happens
// constantly, since guards walk right up to the player. Real AVR returns
// garbage from a divide by zero and carries on; Vircon32 hard-traps the CPU
// (the same crash class already found in `cruiser` and `gameFifteen.c`), so
// the divisor is clamped to a minimum of 1. That yields the same "always
// shoot at point-blank range" behaviour the unclamped division approximates
// on real hardware, without the trap.
//
// A REAL, SIGNIFICANT UPSTREAM BUG, FIXED RATHER THAN PRESERVED:
// `Map::streamData()` applies the per-level file offset (`offset +=
// currentLevel * MAP_SIZE * MAP_SIZE * 4`) ONLY in its
// `STANDARD_FILE_STREAMING` branch - the Windows/SDL build. The
// `PETIT_FATFS_FILE_STREAMING` branch, which is the one the real Gamebuino
// cartridge compiles, never adds it, so on real hardware every level streams
// level 0's data: finishing a level increments the counter and reloads the
// same map, and nine of the ten shipped levels can never be reached.
// `wolfStreamData()` adds the offset here, matching what upstream's own
// desktop build does with the same data - this is upstream's own line,
// restored to the branch it was missing from, not an invention. Fixed on
// direct request rather than preserved; see BUGS.md.
//
// One consequence upstream never had to handle: with the levels genuinely
// wired up, finishing the last one would run past the end of the payload and
// stream an empty map with no walls and no player start. The level counter
// therefore wraps back to 0 at the end of the set - a small port-specific
// addition, flagged as such rather than passed off as upstream behaviour.
//
// Starting a NEW GAME does NOT reset the level counter, which IS upstream
// behaviour: `Engine::init()` zeroes it once at boot and nothing else ever
// does, so a new game continues from whichever level was reached. That was
// invisible on real hardware for exactly the reason above, and is left
// as upstream wrote it.
//
// OTHER REAL UPSTREAM QUIRKS, PRESERVED:
// - `Map::streamIn()`'s difficulty check falls through from the Hard case
//   into Medium into Easy with no `break`, so a Hard-only guard that passes
//   its difficulty test is then re-tested against Medium's. Reproduced as
//   the same cascade of `if`s without early exits.
// - `Actor::dropItem()`'s fallback search loops `i < cellX + 1` rather than
//   `<=`, so it only ever examines the cell up-and-left of the actor rather
//   than the full 3x3 neighbourhood.
// - `Renderer::drawInt()` blanks leading digits by testing `val > 0`, so a
//   value of exactly 0 prints as a single "0" and 100 prints correctly, but
//   the tens digit of a value like 100 is drawn from the already-divided
//   value rather than the original.
// - The QUIT menu entry calls `gb.changeGame()` upstream, which flashes the
//   SD-card loader. There is no Vircon32 equivalent (the same real-hardware
//   -only call already dropped in `gamePirates.c`), so it is inert here;
//   this cartridge's own Start-button quit dialog does that job instead.
//
// DISPLAY PERSISTENCE IS NOT EMULATED, WITH ONE LOCAL EXCEPTION: upstream
// sets `display.persistence = true`, but every game state repaints the whole
// screen each tick, so nothing depends on it - except the death effect,
// which sprinkles random pixels over the frozen last frame for 30 ticks
// without ever clearing. That one state keeps a local 504-word snapshot of
// the previous frame and re-blits it before adding each tick's pixels,
// reproducing the accumulation exactly without any shim-level persistence.

#include "../gamebuinoShim.h"
#include "gameWolfenduinoData.h"

// ---------------------------------------------------------------------------
// Dialect helpers
// ---------------------------------------------------------------------------

// Arithmetic (sign-extending) shift right. This dialect's own `>>` zero-fills,
// so a negative value would otherwise become a large positive one.
#define WOLF_ASR(v, n) ( ( (v) >> (n) ) | ( -( (v) < 0 ) << ( 32 - (n) ) ) )

// Narrow to real AVR widths, reproducing the wrap upstream's own int16_t /
// int8_t / uint8_t variables get on assignment.
#define WOLF_I16(v) ( ( ( (v) & 0xFFFF ) ^ 0x8000 ) - 0x8000 )
#define WOLF_I8(v)  ( ( ( (v) & 0xFF ) ^ 0x80 ) - 0x80 )
#define WOLF_U8(v)  ( (v) & 0xFF )
#define WOLF_U16(v) ( (v) & 0xFFFF )

#define WOLF_DISPLAYWIDTH 84
#define WOLF_DISPLAYHEIGHT 48
#define WOLF_HALF_DISPLAYWIDTH 42
#define WOLF_HALF_DISPLAYHEIGHT 24

#define WOLF_FIXED_SHIFT 7
#define WOLF_FIXED_ONE 128
#define WOLF_DEGREES_90 64
#define WOLF_DEGREES_180 128
#define WOLF_DEGREES_270 192
#define WOLF_DEGREES_360 256

#define WOLF_CELL_SIZE 32
#define WOLF_CELL_SIZE_SHIFT 5
#define WOLF_MAP_SIZE 64
#define WOLF_MAP_BUFFER_SIZE 16

#define WOLF_TEXTURE_SIZE 16
#define WOLF_TEXTURE_STRIDE 4

#define WOLF_CLIP_PLANE 1
#define WOLF_MOVEMENT 7
#define WOLF_TURN 3
#define WOLF_MIN_WALL_DISTANCE 8
#define WOLF_MAX_DOORS 12
#define WOLF_DOOR_FRAME_TEXTURE 19
#define WOLF_MAX_ACTIVE_ACTORS 2
#define WOLF_MAX_ACTIVE_ITEMS 10
#define WOLF_EMPTY_ITEM_SLOT 255
#define WOLF_DYNAMIC_ITEM_ID 254
#define WOLF_ACTOR_HITBOX_SIZE 16
#define WOLF_MIN_ACTOR_DISTANCE 32

#define WOLF_FIRST_FONT_GLYPH 32
#define WOLF_LAST_FONT_GLYPH 95
#define WOLF_FONT_WIDTH 3
#define WOLF_FONT_HEIGHT 5
#define WOLF_FONT_GLYPH_BYTE_SIZE 2

// NEAR_PLANE = DISPLAYWIDTH * 222 / 256
#define WOLF_NEAR_PLANE 72

#define WOLF_DOOR_MAX_OPEN 63
// Ten levels of 64x64 cells, each stored twice (row-major then
// column-major) at one byte for the tile and one for its metadata.
#define WOLF_LEVEL_BYTES ( WOLF_MAP_SIZE * WOLF_MAP_SIZE * 4 )

#define WOLF_MAP_OUT_OF_BOUNDS 255
#define WOLF_NULL_QUEUE_ITEM 255
#define WOLF_RENDER_QUEUE_CAPACITY 8

// Input bits, matching upstream's own Platform.h exactly.
#define WOLF_IN_UP    1
#define WOLF_IN_RIGHT 2
#define WOLF_IN_DOWN  4
#define WOLF_IN_LEFT  8
#define WOLF_IN_A     16
#define WOLF_IN_B     32
#define WOLF_IN_C     64

// Game states
#define WOLF_STATE_MENU 0
#define WOLF_STATE_PAUSEMENU 1
#define WOLF_STATE_LOADING 2
#define WOLF_STATE_PLAYING 3
#define WOLF_STATE_DEAD 4
#define WOLF_STATE_FINISHEDLEVEL 5
#define WOLF_STATE_STARTINGLEVEL 6

// Tile types (upstream's own TileTypes.h enum, resolved to its real values)
#define WOLF_TILE_EMPTY 0
#define WOLF_TILE_FIRSTWALL 1
#define WOLF_TILE_EXITSWITCHWALL 21
#define WOLF_TILE_LASTWALL 22
#define WOLF_TILE_FIRSTDOOR 23
#define WOLF_TILE_DOOR_ELEVATOR_H 29
#define WOLF_TILE_DOOR_ELEVATOR_V 30
#define WOLF_TILE_LASTDOOR 30
#define WOLF_TILE_FIRSTACTOR 31
#define WOLF_TILE_ACTOR_GUARD_EASY 31
#define WOLF_TILE_ACTOR_GUARD_MEDIUM 32
#define WOLF_TILE_ACTOR_GUARD_HARD 33
#define WOLF_TILE_LASTACTOR 36
#define WOLF_TILE_PLAYERSTART_NORTH 37
#define WOLF_TILE_PLAYERSTART_WEST 40
#define WOLF_TILE_FIRSTITEM 41
#define WOLF_TILE_ITEM_CLIP 41
#define WOLF_TILE_ITEM_FIRSTAID 42
#define WOLF_TILE_ITEM_FOOD 44
#define WOLF_TILE_ITEM_MACHINEGUN 49
#define WOLF_TILE_ITEM_KEY2 52
#define WOLF_TILE_LASTITEM 52
#define WOLF_TILE_FIRSTBLOCKINGDECORATION 53
#define WOLF_TILE_LASTBLOCKINGDECORATION 58
#define WOLF_TILE_FIRSTDECORATION 59
#define WOLF_TILE_LASTDECORATION 63
#define WOLF_TILE_SECRETPUSHWALL 64

// Door types
#define WOLF_DOORTYPE_NONE 0
#define WOLF_DOORTYPE_SECRETPUSHWALL 9

// Door states
#define WOLF_DOORSTATE_IDLE 0
#define WOLF_DOORSTATE_OPENING 1
#define WOLF_DOORSTATE_CLOSING 2
#define WOLF_DOORSTATE_PUSHNORTH 3
#define WOLF_DOORSTATE_FIRSTPUSH 3

#define WOLF_DIRECTION_NONE -1
#define WOLF_DIRECTION_NORTH 0
#define WOLF_DIRECTION_EAST 1
#define WOLF_DIRECTION_SOUTH 2
#define WOLF_DIRECTION_WEST 3

// Actor types / states
#define WOLF_ACTORTYPE_EMPTY 0
#define WOLF_ACTORTYPE_GUARD 1

#define WOLF_ACTORSTATE_IDLE 0
#define WOLF_ACTORSTATE_ACTIVE 1
#define WOLF_ACTORSTATE_INJURED 2
#define WOLF_ACTORSTATE_AIMING 3
#define WOLF_ACTORSTATE_SHOOTING 4
#define WOLF_ACTORSTATE_RECOILING 5
#define WOLF_ACTORSTATE_DYING 6
#define WOLF_ACTORSTATE_DEAD 7

// Weapons
#define WOLF_WEAPON_KNIFE 0
#define WOLF_WEAPON_PISTOL 1
#define WOLF_WEAPON_MACHINEGUN 2
#define WOLF_WEAPON_CHAINGUN 3

// Difficulty
#define WOLF_DIFF_BABY 0
#define WOLF_DIFF_EASY 1
#define WOLF_DIFF_MEDIUM 2
#define WOLF_DIFF_HARD 3

// Sound ids, matching upstream's own Sounds.h order.
#define WOLF_SOUND_OPENDOOR 0
#define WOLF_SOUND_CLOSEDOOR 1
#define WOLF_SOUND_ATTACKPISTOL 2
#define WOLF_SOUND_GUARDATTACK 3
#define WOLF_SOUND_COLLECTAMMO 4

// ---------------------------------------------------------------------------
// State
// ---------------------------------------------------------------------------

struct WolfActor
{
    int spawnId;
    int type;
    int x, z;
    int state;
    int frame;
    int hp;
    int targetCellX, targetCellZ;
    int persistent, frozen, alive;
};

struct WolfDoor
{
    int type;
    int x, z;
    int open;
    int state;
    int texture;
};

struct WolfItem
{
    int type;
    int x, z;
    int spawnId;
};

struct WolfQueueItem
{
    int* frame;      // into a wolfXxxFrames table
    int* data;       // into the matching wolfXxxData table
    int x, w;
    int next;
};

WolfActor[WOLF_MAX_ACTIVE_ACTORS] wolfActors;
WolfDoor[WOLF_MAX_DOORS] wolfDoors;
WolfItem[WOLF_MAX_ACTIVE_ITEMS] wolfItems;
WolfQueueItem[WOLF_RENDER_QUEUE_CAPACITY] wolfRenderQueue;

// Engine
int wolfGameState;
int wolfDifficulty;
int wolfEngineFrameCount;

// Platform
int wolfInputState;
int wolfMuted;

// Player
int wolfPlayerX, wolfPlayerZ;
int wolfPlayerDirection;
int wolfPlayerHp;
int wolfPlayerKiller;
int wolfTicksSinceStrafePressed;
int wolfHasMachineGun, wolfHasChainGun, wolfHasKey1, wolfHasKey2;
int wolfWeaponType, wolfWeaponAmmo, wolfWeaponFrame, wolfWeaponTime;
int wolfWeaponDebounce, wolfWeaponShooting;

// Map
int wolfBufferX, wolfBufferZ;
int wolfCurrentLevel;
int[WOLF_MAP_BUFFER_SIZE * WOLF_MAP_BUFFER_SIZE] wolfMapBuffer;
int[32] wolfItemState;
int[32] wolfActorState;
int[WOLF_MAP_BUFFER_SIZE * 2] wolfStreamBuffer;
int wolfMapLoaded;

// Renderer
int wolfDamageIndicator;
int wolfViewX, wolfViewZ;
int wolfViewCellX, wolfViewCellZ;
int wolfViewRotCos, wolfViewRotSin;
int wolfViewClipCos, wolfViewClipSin;
int[WOLF_DISPLAYWIDTH] wolfWBuffer;
int wolfRenderQueueHead;

// Menu
int wolfCurrentMenu;
int wolfCurrentSelection;
int wolfDebounceInput;

// Random generator state (upstream's own function-static seed)
int wolfRandVal;

// The death effect paints over the frozen previous frame - see the header.
int[504] wolfDeathBuffer;

// Upstream's own PushWallDirections table: north, east, south, west.
int[8] wolfPushWallDirections = { 0, -1, 1, 0, 0, 1, -1, 0 };

// ---------------------------------------------------------------------------
// Data access
// ---------------------------------------------------------------------------

// wolf3d.dat is packed four bytes to a word - see gameWolfenduinoData.h.
int wolfMapByte( int index )
{
    int word = wolfMapData[ index >> 2 ];
    return ( word >> ( ( index & 3 ) * 8 ) ) & 0xFF;
}

// ---------------------------------------------------------------------------
// Framebuffer access - direct ports of upstream's own GamebuinoPlatform.h
// inlines. Note upstream's own colour convention is inverted relative to this
// shim's: colour 1 means "light" (bit cleared), colour 0 means "dark".
// ---------------------------------------------------------------------------

void wolfSetPixel( int x, int y )
{
    if( x < 0 || x >= WOLF_DISPLAYWIDTH || y < 0 || y >= WOLF_DISPLAYHEIGHT )
      return;

    gbFrameBuffer[ ( ( y >> 3 ) * WOLF_DISPLAYWIDTH ) + x ] =
      gbFrameBuffer[ ( ( y >> 3 ) * WOLF_DISPLAYWIDTH ) + x ] | ( 1 << ( y & 7 ) );
}

void wolfClearPixel( int x, int y )
{
    if( x < 0 || x >= WOLF_DISPLAYWIDTH || y < 0 || y >= WOLF_DISPLAYHEIGHT )
      return;

    gbFrameBuffer[ ( ( y >> 3 ) * WOLF_DISPLAYWIDTH ) + x ] =
      gbFrameBuffer[ ( ( y >> 3 ) * WOLF_DISPLAYWIDTH ) + x ] & ~( 1 << ( y & 7 ) );
}

void wolfDrawPixel( int x, int y, int colour )
{
    if( colour )
      wolfClearPixel( x, y );
    else
      wolfSetPixel( x, y );
}

void wolfClearDisplay( int colour )
{
    int value = 0xFF;

    if( colour )
      value = 0;

    for( int i = 0; i < 504; i++ )
      gbFrameBuffer[ i ] = value;
}

// ---------------------------------------------------------------------------
// FixedMath - direct port of upstream's own FixedMath.cpp
// ---------------------------------------------------------------------------

int wolfSin( int x )
{
    x = WOLF_U8( x );

    if( x <= WOLF_DEGREES_90 )
      return wolfTrigLUT[ x ];

    if( x <= WOLF_DEGREES_180 )
      return wolfTrigLUT[ WOLF_DEGREES_180 - x ];

    if( x <= WOLF_DEGREES_270 )
      return -1 * wolfTrigLUT[ x - WOLF_DEGREES_180 ];

    return -1 * wolfTrigLUT[ WOLF_DEGREES_360 - x ];
}

int wolfCos( int x )
{
    return wolfSin( WOLF_U8( WOLF_DEGREES_90 + x ) );
}

int wolfClamp( int x, int lower, int upper )
{
    if( x < lower ) return lower;
    if( x > upper ) return upper;
    return x;
}

int wolfGetRandomNumber16()
{
    int lsb = wolfRandVal & 1;

    wolfRandVal = ( wolfRandVal >> 1 ) & 0xFFFF;

    if( lsb == 1 )
      wolfRandVal = wolfRandVal ^ 0xB400;

    return WOLF_U16( wolfRandVal - 1 );
}

int wolfGetRandomNumber()
{
    return wolfGetRandomNumber16() & 0xFF;
}

int wolfAbs( int v )
{
    if( v < 0 ) return -v;
    return v;
}

// ---------------------------------------------------------------------------
// Sound
// ---------------------------------------------------------------------------

void wolfPlaySound( int id )
{
    if( wolfMuted )
      return;

    if( id == 0 )      gbPlayPattern( wolfAudio00, 0 );
    else if( id == 1 ) gbPlayPattern( wolfAudio01, 0 );
    else if( id == 2 ) gbPlayPattern( wolfAudio02, 0 );
    else if( id == 3 ) gbPlayPattern( wolfAudio03, 0 );
    else               gbPlayPattern( wolfAudio04, 0 );
}

// ---------------------------------------------------------------------------
// Map - direct port of upstream's own Map.cpp
// ---------------------------------------------------------------------------

bool wolfIsValid( int x, int z )
{
    if( x < wolfBufferX || z < wolfBufferZ
     || x >= wolfBufferX + WOLF_MAP_BUFFER_SIZE || z >= wolfBufferZ + WOLF_MAP_BUFFER_SIZE )
      return false;

    return true;
}

int wolfGetTileFast( int cellX, int cellZ )
{
    return wolfMapBuffer[ ( cellZ & 0xF ) * WOLF_MAP_BUFFER_SIZE + ( cellX & 0xF ) ];
}

int wolfGetTile( int x, int z )
{
    if( !wolfIsValid( x, z ) )
      return WOLF_MAP_OUT_OF_BOUNDS;

    return wolfGetTileFast( x, z );
}

bool wolfIsDoor( int cellX, int cellZ )
{
    int tile = wolfGetTile( cellX, cellZ );
    return tile >= WOLF_TILE_FIRSTDOOR && tile <= WOLF_TILE_LASTDOOR;
}

bool wolfIsSolid( int cellX, int cellZ )
{
    int tile = wolfGetTile( cellX, cellZ );
    return tile >= WOLF_TILE_FIRSTWALL && tile <= WOLF_TILE_LASTWALL
        && tile != WOLF_MAP_OUT_OF_BOUNDS;
}

bool wolfIsBlocked( int cellX, int cellZ )
{
    int tile = wolfGetTile( cellX, cellZ );

    if( tile >= WOLF_TILE_FIRSTWALL && tile <= WOLF_TILE_LASTWALL )
      return true;

    if( tile >= WOLF_TILE_FIRSTBLOCKINGDECORATION && tile <= WOLF_TILE_LASTBLOCKINGDECORATION )
      return true;

    for( int n = 0; n < WOLF_MAX_DOORS; n++ )
      if( wolfDoors[ n ].type != WOLF_DOORTYPE_NONE
       && wolfDoors[ n ].x == cellX && wolfDoors[ n ].z == cellZ
       && ( wolfDoors[ n ].type == WOLF_DOORTYPE_SECRETPUSHWALL || wolfDoors[ n ].open < 16 ) )
        return true;

    return false;
}

bool wolfIsItemCollected( int spawnId )
{
    int index = spawnId / 8;
    int mask = 1 << ( spawnId - ( index * 8 ) );
    return ( wolfItemState[ index ] & mask ) != 0;
}

void wolfMarkItemCollected( int spawnId )
{
    int index = spawnId / 8;
    int mask = 1 << ( spawnId - ( index * 8 ) );
    wolfItemState[ index ] = wolfItemState[ index ] | mask;
}

bool wolfIsActorKilled( int spawnId )
{
    int index = spawnId / 8;
    int mask = 1 << ( spawnId - ( index * 8 ) );
    return ( wolfActorState[ index ] & mask ) != 0;
}

void wolfMarkActorKilled( int spawnId )
{
    int index = spawnId / 8;
    int mask = 1 << ( spawnId - ( index * 8 ) );
    wolfActorState[ index ] = wolfActorState[ index ] | mask;
}

// Reads one row or column of (tile, metadata) pairs out of the baked-in map
// payload. The per-level offset is applied here - upstream's own Gamebuino
// branch omits it, which is what strands that build on level 0 forever; see
// the header comment.
void wolfStreamData( int orientation, int x, int z, int length )
{
    int offset;

    if( orientation == 0 ) // horizontal
      offset = ( z * WOLF_MAP_SIZE + x ) * 2;
    else
      offset = ( WOLF_MAP_SIZE * WOLF_MAP_SIZE * 2 ) + ( x * WOLF_MAP_SIZE + z ) * 2;

    offset = offset + wolfCurrentLevel * WOLF_MAP_SIZE * WOLF_MAP_SIZE * 4;

    for( int i = 0; i < length * 2; i++ )
    {
        int at = offset + i;

        if( at < 0 || at >= WOLF_MAP_BYTES )
          wolfStreamBuffer[ i ] = 0;
        else
          wolfStreamBuffer[ i ] = wolfMapByte( at );
    }
}

bool wolfPlaceItem( int type, int x, int z, int spawnId );
void wolfSpawnActor( int spawnId, int actorType, int cellX, int cellZ );
void wolfStreamInDoor( int type, int metadata, int x, int z );

int wolfStreamIn( int tile, int metadata, int x, int z )
{
    if( tile >= WOLF_TILE_FIRSTDOOR && tile <= WOLF_TILE_LASTDOOR )
    {
        int textureId = 18;

        if( tile == WOLF_TILE_DOOR_ELEVATOR_H || tile == WOLF_TILE_DOOR_ELEVATOR_V )
          textureId = 12;

        wolfStreamInDoor( tile - WOLF_TILE_FIRSTDOOR + 1, textureId, x, z );
    }
    else if( tile >= WOLF_TILE_FIRSTITEM && tile <= WOLF_TILE_LASTITEM )
    {
        if( !wolfIsItemCollected( metadata ) )
          wolfPlaceItem( tile, x, z, metadata );

        return WOLF_TILE_EMPTY;
    }
    else if( tile >= WOLF_TILE_FIRSTACTOR && tile <= WOLF_TILE_LASTACTOR )
    {
        if( !wolfIsActorKilled( metadata ) )
        {
            // Upstream's own switch has no breaks between the three guard
            // difficulty cases, so a Hard guard that clears its own test then
            // falls into Medium's, and so on - reproduced exactly.
            bool bail = false;

            if( tile == WOLF_TILE_ACTOR_GUARD_HARD )
            {
                if( wolfDifficulty < WOLF_DIFF_HARD )
                  bail = true;
            }

            if( !bail && ( tile == WOLF_TILE_ACTOR_GUARD_HARD || tile == WOLF_TILE_ACTOR_GUARD_MEDIUM ) )
            {
                if( wolfDifficulty < WOLF_DIFF_MEDIUM )
                  bail = true;
            }

            if( !bail && tile >= WOLF_TILE_ACTOR_GUARD_EASY && tile <= WOLF_TILE_ACTOR_GUARD_HARD )
              wolfSpawnActor( metadata, WOLF_ACTORTYPE_GUARD, x, z );
        }

        return WOLF_TILE_EMPTY;
    }
    else if( tile == WOLF_TILE_SECRETPUSHWALL )
    {
        if( wolfGameState == WOLF_STATE_LOADING )
          wolfStreamInDoor( WOLF_DOORTYPE_SECRETPUSHWALL, metadata, x, z );

        return WOLF_TILE_EMPTY;
    }

    return tile;
}

void wolfUpdateHorizontalSlice( int offsetZ )
{
    int targetZ = ( wolfBufferZ + offsetZ ) & 0xF;

    wolfStreamData( 0, wolfBufferX, wolfBufferZ + offsetZ, WOLF_MAP_BUFFER_SIZE );

    for( int x = 0; x < WOLF_MAP_BUFFER_SIZE; x++ )
    {
        int targetX = ( wolfBufferX + x ) & 0xF;
        int read = wolfStreamIn( wolfStreamBuffer[ x * 2 ], wolfStreamBuffer[ x * 2 + 1 ],
                                 wolfBufferX + x, wolfBufferZ + offsetZ );

        wolfMapBuffer[ targetZ * WOLF_MAP_BUFFER_SIZE + targetX ] = read;
    }
}

void wolfUpdateVerticalSlice( int offsetX )
{
    int targetX = ( wolfBufferX + offsetX ) & 0xF;

    wolfStreamData( 1, wolfBufferX + offsetX, wolfBufferZ, WOLF_MAP_BUFFER_SIZE );

    for( int z = 0; z < WOLF_MAP_BUFFER_SIZE; z++ )
    {
        int targetZ = ( wolfBufferZ + z ) & 0xF;
        int read = wolfStreamIn( wolfStreamBuffer[ z * 2 ], wolfStreamBuffer[ z * 2 + 1 ],
                                 wolfBufferX + offsetX, wolfBufferZ + z );

        wolfMapBuffer[ targetZ * WOLF_MAP_BUFFER_SIZE + targetX ] = read;
    }
}

void wolfUpdateActorFrozenState( int n );

void wolfUpdateEntireBuffer()
{
    for( int n = 0; n < WOLF_MAX_ACTIVE_ACTORS; n++ )
      if( wolfActors[ n ].type != WOLF_ACTORTYPE_EMPTY )
        wolfUpdateActorFrozenState( n );

    for( int n = 0; n < WOLF_MAP_BUFFER_SIZE; n++ )
      wolfUpdateHorizontalSlice( n );
}

void wolfUpdateBufferPosition( int newX, int newZ )
{
    if( newX < 0 ) newX = 0;
    if( newZ < 0 ) newZ = 0;
    if( newX > WOLF_MAP_SIZE - WOLF_MAP_BUFFER_SIZE ) newX = WOLF_MAP_SIZE - WOLF_MAP_BUFFER_SIZE;
    if( newZ > WOLF_MAP_SIZE - WOLF_MAP_BUFFER_SIZE ) newZ = WOLF_MAP_SIZE - WOLF_MAP_BUFFER_SIZE;

    if( wolfGameState == WOLF_STATE_LOADING
     || newX <= wolfBufferX - WOLF_MAP_BUFFER_SIZE || newX >= wolfBufferX + WOLF_MAP_BUFFER_SIZE
     || newZ <= wolfBufferZ - WOLF_MAP_BUFFER_SIZE || newZ >= wolfBufferZ + WOLF_MAP_BUFFER_SIZE )
    {
        wolfBufferX = newX;
        wolfBufferZ = newZ;
        wolfUpdateEntireBuffer();
        return;
    }

    if( wolfBufferX == newX && wolfBufferZ == newZ )
      return;

    while( wolfBufferX < newX )
    {
        wolfBufferX++;
        wolfUpdateVerticalSlice( WOLF_MAP_BUFFER_SIZE - 1 );
    }

    while( wolfBufferX > newX )
    {
        wolfBufferX--;
        wolfUpdateVerticalSlice( 0 );
    }

    while( wolfBufferZ < newZ )
    {
        wolfBufferZ++;
        wolfUpdateHorizontalSlice( WOLF_MAP_BUFFER_SIZE - 1 );
    }

    while( wolfBufferZ > newZ )
    {
        wolfBufferZ--;
        wolfUpdateHorizontalSlice( 0 );
    }
}

bool wolfIsFrustrumClipped( int x, int z );

void wolfStreamInDoor( int type, int metadata, int x, int z )
{
    int freeIndex = -1;

    for( int n = 0; n < WOLF_MAX_DOORS; n++ )
    {
        if( freeIndex == -1 && wolfDoors[ n ].type == WOLF_DOORTYPE_NONE )
          freeIndex = n;

        if( wolfDoors[ n ].type != WOLF_DOORTYPE_NONE
         && wolfDoors[ n ].x == x && wolfDoors[ n ].z == z )
          return; // Already streamed in
    }

    if( freeIndex == -1 )
      for( int n = 0; n < WOLF_MAX_DOORS; n++ )
        if( wolfDoors[ n ].type != WOLF_DOORTYPE_SECRETPUSHWALL
         && !wolfIsValid( wolfDoors[ n ].x, wolfDoors[ n ].z ) )
        {
            freeIndex = n;
            break;
        }

    if( freeIndex == -1 )
      for( int n = 0; n < WOLF_MAX_DOORS; n++ )
        if( wolfDoors[ n ].type != WOLF_DOORTYPE_SECRETPUSHWALL
         && wolfIsFrustrumClipped( wolfDoors[ n ].x, wolfDoors[ n ].z ) )
        {
            freeIndex = n;
            break;
        }

    if( freeIndex == -1 )
      return;

    wolfDoors[ freeIndex ].x = x;
    wolfDoors[ freeIndex ].z = z;
    wolfDoors[ freeIndex ].open = 0;
    wolfDoors[ freeIndex ].state = WOLF_DOORSTATE_IDLE;
    wolfDoors[ freeIndex ].type = type;
    wolfDoors[ freeIndex ].texture = metadata;
}

void wolfOpenDoorsAt( int x, int z, int direction )
{
    if( direction != WOLF_DIRECTION_NONE )
      if( wolfGetTile( x, z ) == WOLF_TILE_EXITSWITCHWALL )
        wolfGameState = WOLF_STATE_FINISHEDLEVEL;

    for( int n = 0; n < WOLF_MAX_DOORS; n++ )
      if( wolfDoors[ n ].type != WOLF_DOORTYPE_NONE
       && wolfDoors[ n ].x == x && wolfDoors[ n ].z == z )
      {
          if( wolfDoors[ n ].type == WOLF_DOORTYPE_SECRETPUSHWALL )
          {
              if( direction != WOLF_DIRECTION_NONE )
              {
                  int offX = wolfPushWallDirections[ direction * 2 ];
                  int offZ = wolfPushWallDirections[ direction * 2 + 1 ];

                  if( wolfIsValid( x + offX, z + offZ ) && !wolfIsSolid( x + offX, z + offZ ) )
                    wolfDoors[ n ].state = WOLF_DOORSTATE_FIRSTPUSH + direction;
              }
          }
          else
          {
              if( wolfDoors[ n ].state != WOLF_DOORSTATE_OPENING && wolfDoors[ n ].open == 0 )
                wolfPlaySound( WOLF_SOUND_OPENDOOR );

              wolfDoors[ n ].state = WOLF_DOORSTATE_OPENING;
          }

          return;
      }
}

void wolfUpdateDoor( int n )
{
    int state = wolfDoors[ n ].state;

    if( state >= WOLF_DOORSTATE_FIRSTPUSH && state <= WOLF_DOORSTATE_FIRSTPUSH + 3 )
    {
        wolfDoors[ n ].open++;

        if( wolfDoors[ n ].open == WOLF_CELL_SIZE )
        {
            int offX = wolfPushWallDirections[ ( state - WOLF_DOORSTATE_FIRSTPUSH ) * 2 ];
            int offZ = wolfPushWallDirections[ ( state - WOLF_DOORSTATE_FIRSTPUSH ) * 2 + 1 ];

            wolfDoors[ n ].open = 0;
            wolfDoors[ n ].x = wolfDoors[ n ].x + offX;
            wolfDoors[ n ].z = wolfDoors[ n ].z + offZ;

            if( !wolfIsValid( wolfDoors[ n ].x + offX, wolfDoors[ n ].z + offZ )
             || wolfIsSolid( wolfDoors[ n ].x + offX, wolfDoors[ n ].z + offZ ) )
              wolfDoors[ n ].state = WOLF_DOORSTATE_IDLE;
        }
    }
    else if( state == WOLF_DOORSTATE_OPENING )
    {
        if( wolfDoors[ n ].open < WOLF_DOOR_MAX_OPEN )
          wolfDoors[ n ].open++;
        else
          wolfDoors[ n ].state = WOLF_DOORSTATE_CLOSING;
    }
    else if( state == WOLF_DOORSTATE_CLOSING )
    {
        if( wolfDoors[ n ].open > 0 )
        {
            wolfDoors[ n ].open--;

            if( wolfDoors[ n ].open == 16 )
              wolfPlaySound( WOLF_SOUND_CLOSEDOOR );
        }
        else
        {
            wolfDoors[ n ].state = WOLF_DOORSTATE_IDLE;
        }
    }
}

void wolfMapUpdate()
{
    for( int n = 0; n < WOLF_MAX_DOORS; n++ )
      if( wolfDoors[ n ].type != WOLF_DOORTYPE_NONE )
        wolfUpdateDoor( n );
}

// Direct port of upstream's own isClearLine() - a Wolfenstein-3D-derived
// cell-walking line-of-sight test.
bool wolfIsClearLine( int x1, int z1, int x2, int z2 )
{
    int cellX1 = WOLF_ASR( x1, WOLF_CELL_SIZE_SHIFT );
    int cellX2 = WOLF_ASR( x2, WOLF_CELL_SIZE_SHIFT );
    int cellZ1 = WOLF_ASR( z1, WOLF_CELL_SIZE_SHIFT );
    int cellZ2 = WOLF_ASR( z2, WOLF_CELL_SIZE_SHIFT );

    int xdist = wolfAbs( cellX2 - cellX1 );
    int zdist;
    int partial, delta, deltafrac;
    int xfrac, zfrac;
    int xstep, zstep;
    int ltemp;
    int x, z;
    int tile;

    if( xdist > 0 )
    {
        if( cellX2 > cellX1 )
        {
            partial = ( ( cellX1 + 1 ) << WOLF_CELL_SIZE_SHIFT ) - x1;
            xstep = 1;
        }
        else
        {
            partial = x1 - ( cellX1 << WOLF_CELL_SIZE_SHIFT );
            xstep = -1;
        }

        deltafrac = wolfAbs( x2 - x1 );
        delta = z2 - z1;

        if( deltafrac == 0 )
          deltafrac = 1;

        ltemp = ( delta * WOLF_CELL_SIZE ) / deltafrac;

        if( ltemp > 0x7FFF ) zstep = 0x7FFF;
        else if( ltemp < -0x7FFF ) zstep = -0x7FFF;
        else zstep = ltemp;

        zfrac = z1 + ( ( zstep * partial ) / WOLF_CELL_SIZE );

        x = cellX1 + xstep;
        cellX2 = cellX2 + xstep;

        while( true )
        {
            z = WOLF_ASR( zfrac, WOLF_CELL_SIZE_SHIFT );
            zfrac = zfrac + zstep;

            tile = wolfGetTile( x, z );
            x = x + xstep;

            if( tile )
              if( tile >= WOLF_TILE_FIRSTWALL && tile <= WOLF_TILE_LASTWALL )
                return false;

            if( x == cellX2 )
              break;
        }
    }

    zdist = wolfAbs( cellZ2 - cellZ1 );

    if( zdist > 0 )
    {
        if( cellZ2 > cellZ1 )
        {
            partial = ( ( cellZ1 + 1 ) << WOLF_CELL_SIZE_SHIFT ) - z1;
            zstep = 1;
        }
        else
        {
            partial = z1 - ( cellZ1 << WOLF_CELL_SIZE_SHIFT );
            zstep = -1;
        }

        deltafrac = wolfAbs( z2 - z1 );
        delta = x2 - x1;

        if( deltafrac == 0 )
          deltafrac = 1;

        ltemp = ( delta * WOLF_CELL_SIZE ) / deltafrac;

        if( ltemp > 0x7FFF ) xstep = 0x7FFF;
        else if( ltemp < -0x7FFF ) xstep = -0x7FFF;
        else xstep = ltemp;

        xfrac = x1 + ( ( xstep * partial ) / WOLF_CELL_SIZE );

        z = cellZ1 + zstep;
        cellZ2 = cellZ2 + zstep;

        while( true )
        {
            x = WOLF_ASR( xfrac, WOLF_CELL_SIZE_SHIFT );
            xfrac = xfrac + xstep;

            tile = wolfGetTile( x, z );
            z = z + zstep;

            if( tile )
              if( tile >= WOLF_TILE_FIRSTWALL && tile <= WOLF_TILE_LASTWALL )
                return false;

            if( z == cellZ2 )
              break;
        }
    }

    return true;
}

bool wolfPlaceItem( int type, int x, int z, int spawnId )
{
    int slot = -1;

    for( int n = 0; n < WOLF_MAX_ACTIVE_ITEMS; n++ )
    {
        if( wolfItems[ n ].type == 0 )
          slot = n;
        else if( spawnId != WOLF_DYNAMIC_ITEM_ID && wolfItems[ n ].spawnId == spawnId )
          return false;
    }

    if( slot == -1 )
      for( int n = 0; n < WOLF_MAX_ACTIVE_ITEMS; n++ )
        if( !wolfIsValid( wolfItems[ n ].x, wolfItems[ n ].z ) )
        {
            slot = n;
            break;
        }

    if( slot == -1 )
      return false;

    wolfItems[ slot ].type = type;
    wolfItems[ slot ].spawnId = spawnId;
    wolfItems[ slot ].x = x;
    wolfItems[ slot ].z = z;

    return true;
}

void wolfMapInit()
{
    wolfMapLoaded = true;

    for( int n = 0; n < 32; n++ )
    {
        wolfItemState[ n ] = 0;
        wolfActorState[ n ] = 0;
    }

    for( int n = 0; n < WOLF_MAX_DOORS; n++ )
      wolfDoors[ n ].type = WOLF_DOORTYPE_NONE;

    for( int n = 0; n < WOLF_MAX_ACTIVE_ITEMS; n++ )
      wolfItems[ n ].type = 0;
}

// ---------------------------------------------------------------------------
// Actors - direct port of upstream's own Actor.cpp
// ---------------------------------------------------------------------------

void wolfQueueSprite( int* frames, int* data, int frameIndex, int x, int z );
void wolfPlayerDamage( int amount );

void wolfUpdateActorFrozenState( int n )
{
    int cellX = WOLF_ASR( wolfActors[ n ].x, WOLF_CELL_SIZE_SHIFT );
    int cellZ = WOLF_ASR( wolfActors[ n ].z, WOLF_CELL_SIZE_SHIFT );

    wolfActors[ n ].frozen = cellX < wolfBufferX || cellZ < wolfBufferZ
                          || cellX >= wolfBufferX + WOLF_MAP_BUFFER_SIZE
                          || cellZ >= wolfBufferZ + WOLF_MAP_BUFFER_SIZE;
}

void wolfActorInit( int n, int spawnId, int actorType, int cellX, int cellZ )
{
    wolfActors[ n ].spawnId = spawnId;
    wolfActors[ n ].type = actorType;
    wolfActors[ n ].state = WOLF_ACTORSTATE_IDLE;
    wolfActors[ n ].x = ( cellX << WOLF_CELL_SIZE_SHIFT ) + WOLF_CELL_SIZE / 2;
    wolfActors[ n ].z = ( cellZ << WOLF_CELL_SIZE_SHIFT ) + WOLF_CELL_SIZE / 2;
    wolfActors[ n ].targetCellX = cellX;
    wolfActors[ n ].targetCellZ = cellZ;
    wolfActors[ n ].hp = 25;
    wolfActors[ n ].frame = 0;
    wolfActors[ n ].persistent = 0;
    wolfActors[ n ].frozen = 0;
    wolfActors[ n ].alive = 1;
}

void wolfSpawnActor( int spawnId, int actorType, int cellX, int cellZ )
{
    // Check for an existing actor
    for( int n = 0; n < WOLF_MAX_ACTIVE_ACTORS; n++ )
      if( wolfActors[ n ].spawnId == spawnId )
        return;

    // Find an empty slot
    for( int n = 0; n < WOLF_MAX_ACTIVE_ACTORS; n++ )
      if( wolfActors[ n ].type == WOLF_ACTORTYPE_EMPTY )
      {
          wolfActorInit( n, spawnId, actorType, cellX, cellZ );
          return;
      }

    // Take over an existing slot that is currently frozen
    for( int n = 0; n < WOLF_MAX_ACTIVE_ACTORS; n++ )
      if( wolfActors[ n ].frozen && !wolfActors[ n ].persistent )
      {
          wolfActorInit( n, spawnId, actorType, cellX, cellZ );
          return;
      }
}

bool wolfActorIsPlayerColliding( int n )
{
    if( wolfActors[ n ].x >= wolfPlayerX - WOLF_MIN_ACTOR_DISTANCE
     && wolfActors[ n ].x <= wolfPlayerX + WOLF_MIN_ACTOR_DISTANCE
     && wolfActors[ n ].z >= wolfPlayerZ - WOLF_MIN_ACTOR_DISTANCE
     && wolfActors[ n ].z <= wolfPlayerZ + WOLF_MIN_ACTOR_DISTANCE )
      return true;

    return false;
}

int wolfGetPlayerCellDistance( int n )
{
    int dx = WOLF_ASR( wolfAbs( wolfPlayerX - wolfActors[ n ].x ), WOLF_CELL_SIZE_SHIFT );
    int dz = WOLF_ASR( wolfAbs( wolfPlayerZ - wolfActors[ n ].z ), WOLF_CELL_SIZE_SHIFT );

    if( dx > dz ) return dx;
    return dz;
}

bool wolfActorTryPickCell( int n, int newX, int newZ )
{
    if( wolfIsBlocked( newX, newZ ) && !wolfIsDoor( newX, newZ ) )
      return false;
    if( wolfIsBlocked( wolfActors[ n ].targetCellX, newZ ) && !wolfIsDoor( wolfActors[ n ].targetCellX, newZ ) )
      return false;
    if( wolfIsBlocked( newX, wolfActors[ n ].targetCellZ ) && !wolfIsDoor( newX, wolfActors[ n ].targetCellZ ) )
      return false;

    for( int m = 0; m < WOLF_MAX_ACTIVE_ACTORS; m++ )
      if( m != n && wolfActors[ m ].type != WOLF_ACTORTYPE_EMPTY && wolfActors[ m ].hp > 0 )
        if( wolfActors[ m ].targetCellX == newX && wolfActors[ m ].targetCellZ == newZ )
          return false;

    wolfActors[ n ].targetCellX = newX;
    wolfActors[ n ].targetCellZ = newZ;

    return true;
}

bool wolfActorTryPickCells( int n, int deltaX, int deltaZ )
{
    int tx = wolfActors[ n ].targetCellX;
    int tz = wolfActors[ n ].targetCellZ;

    if( wolfActorTryPickCell( n, tx + deltaX, tz + deltaZ ) ) return true;

    tx = wolfActors[ n ].targetCellX;
    tz = wolfActors[ n ].targetCellZ;
    if( wolfActorTryPickCell( n, tx + deltaX, tz ) ) return true;

    tx = wolfActors[ n ].targetCellX;
    tz = wolfActors[ n ].targetCellZ;
    if( wolfActorTryPickCell( n, tx, tz + deltaZ ) ) return true;

    tx = wolfActors[ n ].targetCellX;
    tz = wolfActors[ n ].targetCellZ;
    if( wolfActorTryPickCell( n, tx - deltaX, tz + deltaZ ) ) return true;

    tx = wolfActors[ n ].targetCellX;
    tz = wolfActors[ n ].targetCellZ;
    if( wolfActorTryPickCell( n, tx + deltaX, tz - deltaZ ) ) return true;

    return false;
}

void wolfActorPickNewTargetCell( int n )
{
    int playerCellX = WOLF_ASR( wolfPlayerX, WOLF_CELL_SIZE_SHIFT );
    int playerCellZ = WOLF_ASR( wolfPlayerZ, WOLF_CELL_SIZE_SHIFT );
    int deltaX = wolfClamp( playerCellX - wolfActors[ n ].targetCellX, -1, 1 );
    int deltaZ = wolfClamp( playerCellZ - wolfActors[ n ].targetCellZ, -1, 1 );
    int dodgeChance = wolfGetRandomNumber();

    if( deltaX == 0 )
    {
        if( dodgeChance < 64 ) deltaX = -1;
        else if( dodgeChance < 128 ) deltaX = 1;
    }
    else if( deltaZ == 0 )
    {
        if( dodgeChance < 64 ) deltaZ = -1;
        else if( dodgeChance < 128 ) deltaZ = 1;
    }

    wolfActorTryPickCells( n, deltaX, deltaZ );
}

bool wolfActorTryMove( int n )
{
    int movement = 1;
    int targetX, targetZ;
    int deltaX, deltaZ;

    if( wolfIsBlocked( wolfActors[ n ].targetCellX, wolfActors[ n ].targetCellZ ) )
    {
        wolfOpenDoorsAt( wolfActors[ n ].targetCellX, wolfActors[ n ].targetCellZ, WOLF_DIRECTION_NONE );
        return false;
    }

    targetX = ( wolfActors[ n ].targetCellX << WOLF_CELL_SIZE_SHIFT ) + WOLF_CELL_SIZE / 2;
    targetZ = ( wolfActors[ n ].targetCellZ << WOLF_CELL_SIZE_SHIFT ) + WOLF_CELL_SIZE / 2;

    // Upstream passes an int16 difference into clamp(int8_t, ...), so the
    // difference is narrowed to a byte first - reproduced here.
    deltaX = wolfClamp( WOLF_I8( targetX - wolfActors[ n ].x ), -movement, movement );
    deltaZ = wolfClamp( WOLF_I8( targetZ - wolfActors[ n ].z ), -movement, movement );

    wolfActors[ n ].x = wolfActors[ n ].x + deltaX;
    wolfActors[ n ].z = wolfActors[ n ].z + deltaZ;

    if( wolfActorIsPlayerColliding( n ) )
    {
        wolfActors[ n ].x = wolfActors[ n ].x - deltaX;
        wolfActors[ n ].z = wolfActors[ n ].z - deltaZ;
        return false;
    }

    if( wolfActors[ n ].x == targetX && wolfActors[ n ].z == targetZ )
      wolfActorPickNewTargetCell( n );

    return true;
}

bool wolfActorShouldShootPlayer( int n )
{
    int dist;
    int chance;

    if( !wolfIsClearLine( wolfActors[ n ].x, wolfActors[ n ].z, wolfPlayerX, wolfPlayerZ ) )
      return false;

    // Upstream divides by this distance unguarded; it is genuinely 0 whenever
    // a guard shares the player's cell, which traps here - see the header.
    dist = wolfGetPlayerCellDistance( n );
    if( dist < 1 )
      dist = 1;

    chance = 16 / dist;

    return wolfGetRandomNumber() < chance;
}

void wolfActorShootPlayer( int n )
{
    int dist;
    int hitchance;
    int damage;

    if( !wolfIsClearLine( wolfActors[ n ].x, wolfActors[ n ].z, wolfPlayerX, wolfPlayerZ ) )
      return;

    dist = wolfGetPlayerCellDistance( n );
    hitchance = 256 - dist * 16;

    if( wolfGetRandomNumber() >= hitchance )
      return;

    if( dist < 2 )      damage = wolfGetRandomNumber() >> 2;
    else if( dist < 4 ) damage = wolfGetRandomNumber() >> 3;
    else                damage = wolfGetRandomNumber() >> 4;

    if( damage > 0 )
    {
        wolfPlayerDamage( damage );

        if( wolfPlayerHp == 0 )
          wolfPlayerKiller = n;
    }
}

void wolfActorSwitchState( int n, int newState )
{
    wolfActors[ n ].state = newState;

    if( newState == WOLF_ACTORSTATE_INJURED )        wolfActors[ n ].frame = 5;
    else if( newState == WOLF_ACTORSTATE_DYING )     wolfActors[ n ].frame = 5;
    else if( newState == WOLF_ACTORSTATE_DEAD )      wolfActors[ n ].frame = 9;
    else if( newState == WOLF_ACTORSTATE_AIMING )    wolfActors[ n ].frame = 3;
    else if( newState == WOLF_ACTORSTATE_RECOILING ) wolfActors[ n ].frame = 3;
    else if( newState == WOLF_ACTORSTATE_SHOOTING )
    {
        wolfActors[ n ].frame = 4;
        wolfPlaySound( WOLF_SOUND_GUARDATTACK );
    }
}

bool wolfActorTryDropItem( int itemType, int cellX, int cellZ )
{
    if( wolfGetTile( cellX, cellZ ) == 0 )
    {
        wolfPlaceItem( itemType, cellX, cellZ, WOLF_DYNAMIC_ITEM_ID );
        return true;
    }

    return false;
}

void wolfActorDropItem( int n, int itemType )
{
    int cellX = WOLF_ASR( wolfActors[ n ].x, WOLF_CELL_SIZE_SHIFT );
    int cellZ = WOLF_ASR( wolfActors[ n ].z, WOLF_CELL_SIZE_SHIFT );

    if( wolfActorTryDropItem( itemType, cellX, cellZ ) )
      return;

    // Upstream's own bounds really are `< cellX + 1` rather than `<=`, so only
    // the up-and-left neighbour is ever examined - preserved.
    for( int i = cellX - 1; i < cellX + 1; i++ )
      for( int j = cellZ - 1; j < cellZ + 1; j++ )
        if( wolfActorTryDropItem( itemType, i, j ) )
          return;
}

void wolfActorDamage( int n, int amount )
{
    if( wolfActors[ n ].hp == 0 )
      return;

    if( amount > wolfActors[ n ].hp )
      wolfActors[ n ].hp = 0;
    else
      wolfActors[ n ].hp = wolfActors[ n ].hp - amount;

    if( wolfActors[ n ].hp == 0 )
    {
        wolfActorSwitchState( n, WOLF_ACTORSTATE_DYING );
        wolfMarkActorKilled( wolfActors[ n ].spawnId );
        wolfActorDropItem( n, WOLF_TILE_ITEM_CLIP );
    }
    else
    {
        wolfActorSwitchState( n, WOLF_ACTORSTATE_INJURED );
    }
}

void wolfActorUpdate( int n )
{
    bool updateFrame;
    int state;

    if( wolfActors[ n ].type == WOLF_ACTORTYPE_EMPTY )
      return;

    wolfUpdateActorFrozenState( n );

    if( wolfActors[ n ].frozen )
      return;

    updateFrame = ( wolfEngineFrameCount & 0x3 ) == 0;
    state = wolfActors[ n ].state;

    if( state == WOLF_ACTORSTATE_IDLE )
    {
        if( wolfIsClearLine( wolfActors[ n ].x, wolfActors[ n ].z, wolfPlayerX, wolfPlayerZ ) )
          wolfActorSwitchState( n, WOLF_ACTORSTATE_ACTIVE );
    }
    else if( state == WOLF_ACTORSTATE_ACTIVE )
    {
        if( wolfActorTryMove( n ) )
        {
            wolfActors[ n ].frame = ( wolfEngineFrameCount >> 2 ) & 0x3;

            if( wolfActors[ n ].frame == 3 )
              wolfActors[ n ].frame = 1;
        }
        else
        {
            wolfActors[ n ].frame = 1;
        }

        if( wolfActorShouldShootPlayer( n ) )
          wolfActorSwitchState( n, WOLF_ACTORSTATE_AIMING );
    }
    else if( state == WOLF_ACTORSTATE_AIMING )
    {
        if( updateFrame )
          wolfActorSwitchState( n, WOLF_ACTORSTATE_SHOOTING );
    }
    else if( state == WOLF_ACTORSTATE_SHOOTING )
    {
        if( updateFrame )
        {
            wolfActorShootPlayer( n );
            wolfActorSwitchState( n, WOLF_ACTORSTATE_RECOILING );
        }
    }
    else if( state == WOLF_ACTORSTATE_RECOILING )
    {
        if( updateFrame )
          wolfActorSwitchState( n, WOLF_ACTORSTATE_ACTIVE );
    }
    else if( state == WOLF_ACTORSTATE_INJURED )
    {
        if( updateFrame )
          wolfActorSwitchState( n, WOLF_ACTORSTATE_ACTIVE );
    }
    else if( state == WOLF_ACTORSTATE_DYING )
    {
        if( updateFrame )
        {
            wolfActors[ n ].frame++;

            if( wolfActors[ n ].frame == 9 )
              wolfActorSwitchState( n, WOLF_ACTORSTATE_DEAD );
        }
    }
}

void wolfActorDraw( int n )
{
    wolfQueueSprite( wolfGuardFrames, wolfGuardData, wolfActors[ n ].frame,
                     wolfActors[ n ].x, wolfActors[ n ].z );
}

// ---------------------------------------------------------------------------
// Renderer - direct port of upstream's own Renderer.cpp
// ---------------------------------------------------------------------------

bool wolfIsFrustrumClipped( int x, int z )
{
    if( ( wolfViewClipCos * ( x - wolfViewCellX ) - wolfViewClipSin * ( z - wolfViewCellZ ) ) < -WOLF_FIXED_ONE )
      return true;

    if( ( wolfViewClipSin * ( x - wolfViewCellX ) + wolfViewClipCos * ( z - wolfViewCellZ ) ) < -WOLF_FIXED_ONE )
      return true;

    return false;
}

void wolfInitWBuffer()
{
    for( int i = 0; i < WOLF_DISPLAYWIDTH; i++ )
      wolfWBuffer[ i ] = 0;
}

// Direct port of the Gamebuino-specific drawFloorAndCeiling(): the top three
// pages are cleared for the ceiling and the bottom three get a 0x55/0x00
// dither for the floor.
void wolfDrawFloorAndCeiling()
{
    int ofs;

    for( int i = 0; i < 3 * WOLF_DISPLAYWIDTH; i++ )
      gbFrameBuffer[ i ] = 0;

    ofs = 3 * WOLF_DISPLAYWIDTH;

    for( int y = 3; y < 6; y++ )
      for( int x = 0; x < WOLF_DISPLAYWIDTH; x = x + 2 )
      {
          gbFrameBuffer[ ofs ] = 0x55;
          ofs++;
          gbFrameBuffer[ ofs ] = 0x00;
          ofs++;
      }
}

// Draws one textured vertical strip. The bit-pair texture reader is inlined
// here rather than kept as upstream's own BitPairReader object - this is the
// single hottest loop in the game, and every function call on this ISA costs
// a flat overhead (see this project's own performance notes in CLAUDE.md).
void wolfDrawStrip( int x, int w, int u, int textureId )
{
    int halfW = w >> 1;
    int y1 = WOLF_HALF_DISPLAYHEIGHT - halfW;
    int y2 = WOLF_HALF_DISPLAYHEIGHT + halfW;
    int verror = halfW;

    int base = u * WOLF_TEXTURE_STRIDE + textureId * ( WOLF_TEXTURE_STRIDE * WOLF_TEXTURE_SIZE );
    int readPtr = base;
    int readOffset = 0;
    int lastRead = wolfWallTextures[ readPtr ];
    int texData = ( lastRead & 3 );

    readOffset = 2;

    for( int y = y1; y < y2; y++ )
    {
        if( y >= 0 && y < WOLF_DISPLAYHEIGHT )
        {
            if( texData == 1 )
            {
                wolfClearPixel( x, y );
            }
            else if( texData == 2 )
            {
                wolfSetPixel( x, y );
            }
            else if( texData == 0 )
            {
                if( ( x ^ y ) & 1 ) wolfClearPixel( x, y );
                else wolfSetPixel( x, y );
            }
            else
            {
                if( ( x & y ) & 1 ) wolfSetPixel( x, y );
                else wolfClearPixel( x, y );
            }
        }

        verror = verror - 15;

        while( verror < 0 )
        {
            texData = ( lastRead >> readOffset ) & 3;
            readOffset = readOffset + 2;

            if( readOffset == 8 )
            {
                readPtr++;

                if( readPtr < 1408 )
                  lastRead = wolfWallTextures[ readPtr ];
                else
                  lastRead = 0;

                readOffset = 0;
            }

            verror = verror + w;
        }
    }

    if( y2 < WOLF_DISPLAYHEIGHT )
      wolfSetPixel( x, y2 );
}

// Draws one side of a cell. This is the affine (non perspective-correct)
// variant, which is the one upstream actually compiles - its
// PERSPECTIVE_CORRECT_TEXTURE_MAPPING alternative is never defined.
void wolfDrawWall( int _x1, int _z1, int _x2, int _z2, int textureId, int _u1, int _u2 )
{
    int z2, x2, z1, x1;
    int vx1, vx2, sx1, sx2, firstx, lastx;
    int w1, w2, dx, werror, uerror, w;
    int u, du, ustep, dw, wstep;

    // Find the position of the wall edges relative to the eye. The products
    // are computed at full width (upstream casts to int32_t for exactly this
    // reason) and only the shifted result is narrowed back to 16 bits.
    z2 = WOLF_I16( WOLF_ASR( wolfViewRotCos * ( _x1 - wolfViewX ), WOLF_FIXED_SHIFT )
                 - WOLF_ASR( wolfViewRotSin * ( _z1 - wolfViewZ ), WOLF_FIXED_SHIFT ) );
    x2 = WOLF_I16( WOLF_ASR( wolfViewRotSin * ( _x1 - wolfViewX ), WOLF_FIXED_SHIFT )
                 + WOLF_ASR( wolfViewRotCos * ( _z1 - wolfViewZ ), WOLF_FIXED_SHIFT ) );
    z1 = WOLF_I16( WOLF_ASR( wolfViewRotCos * ( _x2 - wolfViewX ), WOLF_FIXED_SHIFT )
                 - WOLF_ASR( wolfViewRotSin * ( _z2 - wolfViewZ ), WOLF_FIXED_SHIFT ) );
    x1 = WOLF_I16( WOLF_ASR( wolfViewRotSin * ( _x2 - wolfViewX ), WOLF_FIXED_SHIFT )
                 + WOLF_ASR( wolfViewRotCos * ( _z2 - wolfViewZ ), WOLF_FIXED_SHIFT ) );

    // Clip to the front plane
    if( z1 < WOLF_CLIP_PLANE && z2 < WOLF_CLIP_PLANE )
      return;

    if( z1 < WOLF_CLIP_PLANE )
    {
        x1 = WOLF_I16( x1 + ( WOLF_CLIP_PLANE - z1 ) * ( x2 - x1 ) / ( z2 - z1 ) );
        z1 = WOLF_CLIP_PLANE;
    }
    else if( z2 < WOLF_CLIP_PLANE )
    {
        x2 = WOLF_I16( x2 + ( WOLF_CLIP_PLANE - z2 ) * ( x1 - x2 ) / ( z1 - z2 ) );
        z2 = WOLF_CLIP_PLANE;
    }

    // Apply the perspective projection
    vx1 = WOLF_I16( x1 * WOLF_NEAR_PLANE / z1 );
    vx2 = WOLF_I16( x2 * WOLF_NEAR_PLANE / z2 );

    // Transform the end points into screen space
    sx1 = WOLF_I16( ( WOLF_DISPLAYWIDTH / 2 ) + vx1 );
    sx2 = WOLF_I16( ( WOLF_DISPLAYWIDTH / 2 ) + vx2 ) - 1;

    // Clamp to the visible portion of the screen
    firstx = sx1;
    if( firstx < 0 ) firstx = 0;
    lastx = sx2;
    if( lastx > WOLF_DISPLAYWIDTH - 1 ) lastx = WOLF_DISPLAYWIDTH - 1;

    if( lastx < firstx )
      return;

    w1 = WOLF_I16( ( WOLF_CELL_SIZE * WOLF_NEAR_PLANE ) / z1 );
    w2 = WOLF_I16( ( WOLF_CELL_SIZE * WOLF_NEAR_PLANE ) / z2 );
    dx = sx2 - sx1;
    werror = WOLF_ASR( dx, 1 );
    uerror = werror;
    w = w1;
    u = _u1;

    if( w1 < w2 )
    {
        dw = w2 - w1;
        wstep = 1;
    }
    else
    {
        dw = w1 - w2;
        wstep = -1;
    }

    if( _u1 < _u2 )
    {
        du = _u2 - _u1;
        ustep = 1;
    }
    else
    {
        du = _u1 - _u2;
        ustep = -1;
    }

    for( int x = sx1; x <= sx2; x++ )
    {
        if( x >= 0 && x < WOLF_DISPLAYWIDTH && w > wolfWBuffer[ x ] )
        {
            if( w <= 255 )
              wolfWBuffer[ x ] = w;
            else
              wolfWBuffer[ x ] = 255;

            wolfDrawStrip( x, w, u, textureId );
        }

        werror = werror - dw;
        uerror = uerror - du;

        if( dx > 0 )
        {
            while( werror < 0 )
            {
                w = w + wstep;
                werror = werror + dx;
            }

            while( uerror < 0 )
            {
                u = u + ustep;
                uerror = uerror + dx;
            }
        }
    }
}

void wolfQueueSprite( int* frames, int* data, int frameIndex, int _x, int _z )
{
    int cellX = WOLF_ASR( _x, WOLF_CELL_SIZE_SHIFT );
    int cellZ = WOLF_ASR( _z, WOLF_CELL_SIZE_SHIFT );
    int zt, xt, vx, w, x;
    int newItem = WOLF_NULL_QUEUE_ITEM;

    if( wolfIsFrustrumClipped( cellX, cellZ ) )
      return;

    zt = WOLF_I16( WOLF_ASR( wolfViewRotCos * ( _x - wolfViewX ), WOLF_FIXED_SHIFT )
                 - WOLF_ASR( wolfViewRotSin * ( _z - wolfViewZ ), WOLF_FIXED_SHIFT ) );
    xt = WOLF_I16( WOLF_ASR( wolfViewRotSin * ( _x - wolfViewX ), WOLF_FIXED_SHIFT )
                 + WOLF_ASR( wolfViewRotCos * ( _z - wolfViewZ ), WOLF_FIXED_SHIFT ) );

    // Clip to the front plane
    if( zt < WOLF_CLIP_PLANE )
      return;

    vx = WOLF_I16( xt * WOLF_NEAR_PLANE / zt );

    if( vx <= -WOLF_DISPLAYWIDTH || vx >= WOLF_DISPLAYWIDTH )
      return;

    w = WOLF_I16( ( WOLF_CELL_SIZE * WOLF_NEAR_PLANE ) / zt );
    x = vx + WOLF_HALF_DISPLAYWIDTH;

    if( w > 255 )
      w = 255;

    for( int n = 0; n < WOLF_RENDER_QUEUE_CAPACITY; n++ )
      if( wolfRenderQueue[ n ].data == NULL )
      {
          newItem = n;
          break;
      }

    if( newItem == WOLF_NULL_QUEUE_ITEM )
    {
        if( w > wolfRenderQueue[ wolfRenderQueueHead ].w )
        {
            newItem = wolfRenderQueueHead;
            wolfRenderQueueHead = wolfRenderQueue[ wolfRenderQueueHead ].next;
        }
        else
        {
            return;
        }
    }

    wolfRenderQueue[ newItem ].x = x;
    wolfRenderQueue[ newItem ].w = w;
    wolfRenderQueue[ newItem ].frame = &frames[ frameIndex * 4 ];
    wolfRenderQueue[ newItem ].data = data;

    if( wolfRenderQueueHead == WOLF_NULL_QUEUE_ITEM )
    {
        wolfRenderQueueHead = newItem;
        wolfRenderQueue[ newItem ].next = WOLF_NULL_QUEUE_ITEM;
        return;
    }

    if( w < wolfRenderQueue[ wolfRenderQueueHead ].w )
    {
        wolfRenderQueue[ newItem ].next = wolfRenderQueueHead;
        wolfRenderQueueHead = newItem;
        return;
    }

    for( int item = wolfRenderQueueHead; item != WOLF_NULL_QUEUE_ITEM; item = wolfRenderQueue[ item ].next )
    {
        if( wolfRenderQueue[ item ].next == WOLF_NULL_QUEUE_ITEM )
        {
            wolfRenderQueue[ item ].next = newItem;
            wolfRenderQueue[ newItem ].next = WOLF_NULL_QUEUE_ITEM;
            break;
        }
        else if( w < wolfRenderQueue[ wolfRenderQueue[ item ].next ].w )
        {
            wolfRenderQueue[ newItem ].next = wolfRenderQueue[ item ].next;
            wolfRenderQueue[ item ].next = newItem;
            break;
        }
    }
}

void wolfDrawQueuedSprite( int id )
{
    int* frame = wolfRenderQueue[ id ].frame;
    int* data = wolfRenderQueue[ id ].data;
    int frameOffset = frame[ 0 ];
    int frameWidth = frame[ 1 ];
    int frameHeight = frame[ 2 ];
    int frameXOffset = frame[ 3 ];

    int w = wolfRenderQueue[ id ].w;
    int halfW = w >> 1;
    int y2 = WOLF_HALF_DISPLAYHEIGHT + halfW;
    int y1 = y2 - ( w * frameHeight ) / ( WOLF_CELL_SIZE / 2 );
    int dx = ( w * frameWidth ) / ( WOLF_CELL_SIZE / 2 );
    int sx1 = wolfRenderQueue[ id ].x - halfW + ( w * frameXOffset ) / ( WOLF_CELL_SIZE / 2 );
    int sx2 = sx1 + dx;
    int uerror = dx;
    int u = 0;
    int du = frameWidth;
    int ustep = 1;
    int v;

    for( int x = sx1; x <= sx2; x++ )
    {
        if( x >= 0 && x < WOLF_DISPLAYWIDTH && w > wolfWBuffer[ x ] )
        {
            int verror = halfW;
            int bitIndex = frameOffset + frameHeight * u;
            int readPtr = bitIndex >> 2;
            int readOffset = ( bitIndex - ( readPtr << 2 ) ) << 1;
            int lastRead = data[ readPtr ];
            int texData = ( lastRead >> readOffset ) & 3;

            readOffset = readOffset + 2;

            if( readOffset == 8 )
            {
                readPtr++;
                lastRead = data[ readPtr ];
                readOffset = 0;
            }

            v = 0;

            for( int y = y2; y >= y1 && y >= 0 && v < frameHeight; y-- )
            {
                if( y < WOLF_DISPLAYHEIGHT )
                {
                    if( texData == 1 )
                    {
                        wolfClearPixel( x, y );
                    }
                    else if( texData == 2 )
                    {
                        wolfSetPixel( x, y );
                    }
                    else if( texData == 3 )
                    {
                        if( ( x ^ y ) & 1 ) wolfClearPixel( x, y );
                        else wolfSetPixel( x, y );
                    }
                }

                verror = verror - 15;

                while( verror < 0 )
                {
                    texData = ( lastRead >> readOffset ) & 3;
                    readOffset = readOffset + 2;

                    if( readOffset == 8 )
                    {
                        readPtr++;
                        lastRead = data[ readPtr ];
                        readOffset = 0;
                    }

                    verror = verror + w;
                    v++;
                }
            }
        }

        uerror = uerror - du;

        if( dx > 0 )
          while( u < frameWidth - 1 && uerror < 0 )
          {
              u = u + ustep;
              uerror = uerror + dx;
          }
    }
}

void wolfDrawGlyph( int glyph, int x, int y )
{
    int ptr = glyph * WOLF_FONT_GLYPH_BYTE_SIZE;
    int readMask = 1;
    int read = wolfFontData[ ptr ];

    ptr++;

    for( int i = 0; i < WOLF_FONT_WIDTH; i++ )
      for( int j = 0; j < WOLF_FONT_HEIGHT; j++ )
      {
          int colour = 1;

          if( read & readMask )
            colour = 0;

          wolfDrawPixel( x + i, y + j, colour );

          readMask = ( readMask << 1 ) & 0xFF;

          if( readMask == 0 )
          {
              readMask = 1;

              if( ptr < 128 )
                read = wolfFontData[ ptr ];
              else
                read = 0;

              ptr++;
          }
      }

    for( int j = 0; j < WOLF_FONT_HEIGHT; j++ )
      wolfClearPixel( x + WOLF_FONT_WIDTH, y + j );
}

void wolfDrawString( int* str, int x, int y )
{
    int startX = x;
    int index = 0;
    int current;

    while( true )
    {
        current = str[ index ];
        index++;

        if( current >= WOLF_FIRST_FONT_GLYPH && current <= WOLF_LAST_FONT_GLYPH )
          wolfDrawGlyph( current - WOLF_FIRST_FONT_GLYPH, x, y );

        x = x + WOLF_FONT_WIDTH + 1;

        if( current == 10 )
        {
            x = startX;
            y = y + WOLF_FONT_HEIGHT + 1;
        }

        if( current == 0 )
          break;
    }
}

void wolfDrawInt( int val, int x, int y )
{
    for( int i = 0; i < 3; i++ )
    {
        int c = val % 10;

        if( val > 0 || i == 0 )
          wolfDrawGlyph( c + 48 - WOLF_FIRST_FONT_GLYPH, x, y );
        else
          wolfDrawGlyph( 32 - WOLF_FIRST_FONT_GLYPH, x, y );

        x = x - ( WOLF_FONT_WIDTH + 1 );
        val = val / 10;
    }
}

void wolfDrawDamage()
{
    if( wolfDamageIndicator > 0 )
    {
        wolfDamageIndicator--;

        for( int x = 0; x < WOLF_DISPLAYWIDTH; x++ )
        {
            wolfSetPixel( x, 0 );
            wolfSetPixel( x, WOLF_DISPLAYHEIGHT - 1 );
        }

        for( int y = 0; y < WOLF_DISPLAYHEIGHT; y++ )
        {
            wolfSetPixel( 0, y );
            wolfSetPixel( WOLF_DISPLAYWIDTH - 1, y );
        }
    }
}

void wolfDrawWeapon()
{
    int* frames = wolfPistolFrames;
    int* data = wolfPistolData;
    int frameOffset, frameWidth, frameHeight, x;
    int readPtr, readOffset, lastRead;

    if( wolfWeaponType == WOLF_WEAPON_KNIFE )
    {
        frames = wolfKnifeFrames;
        data = wolfKnifeData;
    }
    else if( wolfWeaponType == WOLF_WEAPON_MACHINEGUN )
    {
        frames = wolfMachinegunFrames;
        data = wolfMachinegunData;
    }

    frameOffset = frames[ wolfWeaponFrame * 4 ];
    frameWidth = frames[ wolfWeaponFrame * 4 + 1 ];
    frameHeight = frames[ wolfWeaponFrame * 4 + 2 ];
    x = WOLF_HALF_DISPLAYWIDTH - 8 + frames[ wolfWeaponFrame * 4 + 3 ];

    readPtr = frameOffset >> 2;
    readOffset = ( frameOffset - ( readPtr << 2 ) ) << 1;
    lastRead = data[ readPtr ];

    for( int i = 0; i < frameWidth; i++ )
      for( int j = frameHeight - 1; j >= 0; j-- )
      {
          int pixel = ( lastRead >> readOffset ) & 3;

          readOffset = readOffset + 2;

          if( readOffset == 8 )
          {
              readPtr++;
              lastRead = data[ readPtr ];
              readOffset = 0;
          }

          if( pixel )
          {
              int colour = 1;

              if( pixel - 1 )
                colour = 0;

              wolfDrawPixel( i + x, WOLF_DISPLAYHEIGHT - frameHeight + j, colour );
          }
      }
}

void wolfDrawCell( int cellX, int cellZ )
{
    int tile;
    int worldX, worldZ;
    int textureId;

    // Upstream calls gb.update() here as a hack to keep its own sound engine
    // ticking mid-render. This shim drives sound from gbRenderFrame() instead,
    // so the call is dropped rather than reproduced.
    if( wolfIsFrustrumClipped( cellX, cellZ ) )
      return;

    tile = wolfGetTileFast( cellX, cellZ );

    if( tile == 0 )
      return;

    worldX = cellX << WOLF_CELL_SIZE_SHIFT;
    worldZ = cellZ << WOLF_CELL_SIZE_SHIFT;

    if( tile >= WOLF_TILE_FIRSTDECORATION && tile <= WOLF_TILE_LASTDECORATION )
    {
        wolfQueueSprite( wolfDecorationsFrames, wolfDecorationsData,
                         tile - WOLF_TILE_FIRSTDECORATION,
                         worldX + WOLF_CELL_SIZE / 2, worldZ + WOLF_CELL_SIZE / 2 );
        return;
    }

    if( tile >= WOLF_TILE_FIRSTBLOCKINGDECORATION && tile <= WOLF_TILE_LASTBLOCKINGDECORATION )
    {
        wolfQueueSprite( wolfBlockingDecorationsFrames, wolfBlockingDecorationsData,
                         tile - WOLF_TILE_FIRSTBLOCKINGDECORATION,
                         worldX + WOLF_CELL_SIZE / 2, worldZ + WOLF_CELL_SIZE / 2 );
        return;
    }

    if( tile < WOLF_TILE_FIRSTWALL || tile > WOLF_TILE_LASTWALL )
      return;

    textureId = tile - WOLF_TILE_FIRSTWALL;

    if( wolfViewZ < worldZ )
    {
        if( wolfViewX > worldX )
        {
            // north west quadrant
            if( wolfIsDoor( cellX, cellZ - 1 ) )
              wolfDrawWall( worldX, worldZ, worldX + WOLF_CELL_SIZE, worldZ, WOLF_DOOR_FRAME_TEXTURE, 0, 15 );
            else if( !wolfIsSolid( cellX, cellZ - 1 ) )
              wolfDrawWall( worldX, worldZ, worldX + WOLF_CELL_SIZE, worldZ, textureId, 0, 15 );

            if( wolfViewX > worldX + WOLF_CELL_SIZE )
            {
                if( wolfIsDoor( cellX + 1, cellZ ) )
                  wolfDrawWall( worldX + WOLF_CELL_SIZE, worldZ, worldX + WOLF_CELL_SIZE, worldZ + WOLF_CELL_SIZE, WOLF_DOOR_FRAME_TEXTURE, 0, 15 );
                else if( !wolfIsSolid( cellX + 1, cellZ ) )
                  wolfDrawWall( worldX + WOLF_CELL_SIZE, worldZ, worldX + WOLF_CELL_SIZE, worldZ + WOLF_CELL_SIZE, textureId, 0, 15 );
            }
        }
        else
        {
            // north east quadrant
            if( wolfIsDoor( cellX, cellZ - 1 ) )
              wolfDrawWall( worldX, worldZ, worldX + WOLF_CELL_SIZE, worldZ, WOLF_DOOR_FRAME_TEXTURE, 0, 15 );
            else if( !wolfIsSolid( cellX, cellZ - 1 ) )
              wolfDrawWall( worldX, worldZ, worldX + WOLF_CELL_SIZE, worldZ, textureId, 0, 15 );

            if( wolfViewX < worldX )
            {
                if( wolfIsDoor( cellX - 1, cellZ ) )
                  wolfDrawWall( worldX, worldZ + WOLF_CELL_SIZE, worldX, worldZ, WOLF_DOOR_FRAME_TEXTURE, 0, 15 );
                else if( !wolfIsSolid( cellX - 1, cellZ ) )
                  wolfDrawWall( worldX, worldZ + WOLF_CELL_SIZE, worldX, worldZ, textureId, 0, 15 );
            }
        }
    }
    else
    {
        if( wolfViewX > worldX )
        {
            // south west quadrant
            if( wolfViewZ > worldZ + WOLF_CELL_SIZE )
            {
                if( wolfIsDoor( cellX, cellZ + 1 ) )
                  wolfDrawWall( worldX + WOLF_CELL_SIZE, worldZ + WOLF_CELL_SIZE, worldX, worldZ + WOLF_CELL_SIZE, WOLF_DOOR_FRAME_TEXTURE, 0, 15 );
                else if( !wolfIsSolid( cellX, cellZ + 1 ) )
                  wolfDrawWall( worldX + WOLF_CELL_SIZE, worldZ + WOLF_CELL_SIZE, worldX, worldZ + WOLF_CELL_SIZE, textureId, 0, 15 );
            }

            if( wolfViewX > worldX + WOLF_CELL_SIZE )
            {
                if( wolfIsDoor( cellX + 1, cellZ ) )
                  wolfDrawWall( worldX + WOLF_CELL_SIZE, worldZ, worldX + WOLF_CELL_SIZE, worldZ + WOLF_CELL_SIZE, WOLF_DOOR_FRAME_TEXTURE, 0, 15 );
                else if( !wolfIsSolid( cellX + 1, cellZ ) )
                  wolfDrawWall( worldX + WOLF_CELL_SIZE, worldZ, worldX + WOLF_CELL_SIZE, worldZ + WOLF_CELL_SIZE, textureId, 0, 15 );
            }
        }
        else
        {
            // south east quadrant
            if( wolfViewZ > worldZ + WOLF_CELL_SIZE )
            {
                if( wolfIsDoor( cellX, cellZ + 1 ) )
                  wolfDrawWall( worldX + WOLF_CELL_SIZE, worldZ + WOLF_CELL_SIZE, worldX, worldZ + WOLF_CELL_SIZE, WOLF_DOOR_FRAME_TEXTURE, 0, 15 );
                else if( !wolfIsSolid( cellX, cellZ + 1 ) )
                  wolfDrawWall( worldX + WOLF_CELL_SIZE, worldZ + WOLF_CELL_SIZE, worldX, worldZ + WOLF_CELL_SIZE, textureId, 0, 15 );
            }

            if( wolfViewX < worldX )
            {
                if( wolfIsDoor( cellX - 1, cellZ ) )
                  wolfDrawWall( worldX, worldZ + WOLF_CELL_SIZE, worldX, worldZ, WOLF_DOOR_FRAME_TEXTURE, 0, 15 );
                else if( !wolfIsSolid( cellX - 1, cellZ ) )
                  wolfDrawWall( worldX, worldZ + WOLF_CELL_SIZE, worldX, worldZ, textureId, 0, 15 );
            }
        }
    }
}

void wolfDrawBufferedCells()
{
    int xd, zd, x1, z1, x2, z2;

    if( wolfViewRotCos > 0 )
    {
        x1 = wolfBufferX;
        x2 = x1 + WOLF_MAP_BUFFER_SIZE;
        xd = 1;
    }
    else
    {
        x2 = wolfBufferX - 1;
        x1 = x2 + WOLF_MAP_BUFFER_SIZE;
        xd = -1;
    }

    if( wolfViewRotSin < 0 )
    {
        z1 = wolfBufferZ;
        z2 = z1 + WOLF_MAP_BUFFER_SIZE;
        zd = 1;
    }
    else
    {
        z2 = wolfBufferZ - 1;
        z1 = z2 + WOLF_MAP_BUFFER_SIZE;
        zd = -1;
    }

    if( wolfAbs( wolfViewRotCos ) < wolfAbs( wolfViewRotSin ) )
    {
        for( int z = z1; z != z2; z = z + zd )
          for( int x = x1; x != x2; x = x + xd )
            wolfDrawCell( x, z );
    }
    else
    {
        for( int x = x1; x != x2; x = x + xd )
          for( int z = z1; z != z2; z = z + zd )
            wolfDrawCell( x, z );
    }
}

void wolfDrawDoors()
{
    for( int n = 0; n < WOLF_MAX_DOORS; n++ )
    {
        int textureId = wolfDoors[ n ].texture;
        int doorX, doorZ, offset, worldX, worldZ;

        if( !wolfIsValid( wolfDoors[ n ].x, wolfDoors[ n ].z ) )
          continue;

        if( wolfDoors[ n ].type == WOLF_DOORTYPE_SECRETPUSHWALL )
        {
            doorX = wolfDoors[ n ].x << WOLF_CELL_SIZE_SHIFT;
            doorZ = wolfDoors[ n ].z << WOLF_CELL_SIZE_SHIFT;

            if( wolfDoors[ n ].state == WOLF_DOORSTATE_FIRSTPUSH )          doorZ = doorZ - wolfDoors[ n ].open;
            else if( wolfDoors[ n ].state == WOLF_DOORSTATE_FIRSTPUSH + 1 ) doorX = doorX + wolfDoors[ n ].open;
            else if( wolfDoors[ n ].state == WOLF_DOORSTATE_FIRSTPUSH + 2 ) doorZ = doorZ + wolfDoors[ n ].open;
            else if( wolfDoors[ n ].state == WOLF_DOORSTATE_FIRSTPUSH + 3 ) doorX = doorX - wolfDoors[ n ].open;

            if( wolfViewX < doorX )
              wolfDrawWall( doorX, doorZ + WOLF_CELL_SIZE, doorX, doorZ, textureId, 0, 15 );
            else if( wolfViewX > doorX )
              wolfDrawWall( doorX + WOLF_CELL_SIZE, doorZ, doorX + WOLF_CELL_SIZE, doorZ + WOLF_CELL_SIZE, textureId, 0, 15 );

            if( wolfViewZ > doorZ + WOLF_CELL_SIZE )
              wolfDrawWall( doorX + WOLF_CELL_SIZE, doorZ + WOLF_CELL_SIZE, doorX, doorZ + WOLF_CELL_SIZE, textureId, 0, 15 );
            else if( wolfViewZ < doorZ )
              wolfDrawWall( doorX, doorZ, doorX + WOLF_CELL_SIZE, doorZ, textureId, 0, 15 );

            continue;
        }

        offset = wolfDoors[ n ].open;

        if( offset >= 16 )
          continue;

        worldX = wolfDoors[ n ].x << WOLF_CELL_SIZE_SHIFT;
        worldZ = wolfDoors[ n ].z << WOLF_CELL_SIZE_SHIFT;

        if( ( wolfDoors[ n ].type & 0x1 ) == 0 )
        {
            worldX = worldX + WOLF_CELL_SIZE / 2;

            if( wolfViewX < worldX )
              wolfDrawWall( worldX, worldZ + WOLF_CELL_SIZE, worldX, worldZ + offset * 2, textureId, 0, 15 - offset );
            else
              wolfDrawWall( worldX, worldZ + offset * 2, worldX, worldZ + WOLF_CELL_SIZE, textureId, 15 - offset, 0 );
        }
        else
        {
            worldZ = worldZ + WOLF_CELL_SIZE / 2;

            if( wolfViewZ > worldZ )
              wolfDrawWall( worldX + WOLF_CELL_SIZE, worldZ, worldX + offset * 2, worldZ, textureId, 0, 15 - offset );
            else
              wolfDrawWall( worldX + offset * 2, worldZ, worldX + WOLF_CELL_SIZE, worldZ, textureId, 15 - offset, 0 );
        }
    }
}

void wolfDrawFrame()
{
    int hudHeight;

    wolfRenderQueueHead = WOLF_NULL_QUEUE_ITEM;

    for( int n = 0; n < WOLF_RENDER_QUEUE_CAPACITY; n++ )
      wolfRenderQueue[ n ].data = NULL;

    wolfViewX = wolfPlayerX;
    wolfViewZ = wolfPlayerZ;
    wolfViewRotCos = wolfCos( WOLF_U8( -wolfPlayerDirection ) );
    wolfViewRotSin = wolfSin( WOLF_U8( -wolfPlayerDirection ) );
    wolfViewClipCos = wolfCos( WOLF_U8( -wolfPlayerDirection + WOLF_DEGREES_90 / 2 ) );
    wolfViewClipSin = wolfSin( WOLF_U8( -wolfPlayerDirection + WOLF_DEGREES_90 / 2 ) );

    wolfViewCellX = WOLF_ASR( wolfPlayerX, WOLF_CELL_SIZE_SHIFT );
    wolfViewCellZ = WOLF_ASR( wolfPlayerZ, WOLF_CELL_SIZE_SHIFT );

    wolfInitWBuffer();
    wolfDrawFloorAndCeiling();
    wolfDrawBufferedCells();
    wolfDrawDoors();

    for( int n = 0; n < WOLF_MAX_ACTIVE_ACTORS; n++ )
      if( wolfActors[ n ].type != WOLF_ACTORTYPE_EMPTY && !wolfActors[ n ].frozen )
        wolfActorDraw( n );

    for( int n = 0; n < WOLF_MAX_ACTIVE_ITEMS; n++ )
      if( wolfItems[ n ].type != 0 )
      {
          int x = ( wolfItems[ n ].x << WOLF_CELL_SIZE_SHIFT ) + WOLF_CELL_SIZE / 2;
          int z = ( wolfItems[ n ].z << WOLF_CELL_SIZE_SHIFT ) + WOLF_CELL_SIZE / 2;

          wolfQueueSprite( wolfItemsFrames, wolfItemsData,
                           wolfItems[ n ].type - WOLF_TILE_FIRSTITEM, x, z );
      }

    for( int item = wolfRenderQueueHead; item != WOLF_NULL_QUEUE_ITEM; item = wolfRenderQueue[ item ].next )
      wolfDrawQueuedSprite( item );

    wolfDrawWeapon();
    wolfDrawDamage();

    // Draw HUD
    hudHeight = WOLF_DISPLAYHEIGHT - WOLF_FONT_HEIGHT;
    wolfDrawGlyph( 43 - WOLF_FIRST_FONT_GLYPH, 0, hudHeight );
    wolfDrawInt( wolfPlayerHp, ( WOLF_FONT_WIDTH + 1 ) * 3, hudHeight );
    wolfDrawGlyph( 42 - WOLF_FIRST_FONT_GLYPH, WOLF_DISPLAYWIDTH - ( WOLF_FONT_WIDTH + 1 ) * 4, hudHeight );
    wolfDrawInt( wolfWeaponAmmo, WOLF_DISPLAYWIDTH - ( WOLF_FONT_WIDTH + 1 ), hudHeight );
}

// ---------------------------------------------------------------------------
// Player - direct port of upstream's own Player.cpp (USE_SIMPLE_COLLISIONS is
// defined for this build, so the simple collision path is the real one)
// ---------------------------------------------------------------------------

void wolfPlayerDamage( int amount )
{
    wolfDamageIndicator = 5;

    if( amount > wolfPlayerHp )
      wolfPlayerHp = 0;
    else
      wolfPlayerHp = wolfPlayerHp - amount;
}

bool wolfIsPointColliding( int pointX, int pointZ )
{
    int cellX = WOLF_ASR( pointX, WOLF_CELL_SIZE_SHIFT );
    int cellZ = WOLF_ASR( pointZ, WOLF_CELL_SIZE_SHIFT );

    return wolfIsBlocked( cellX, cellZ );
}

bool wolfIsPlayerColliding()
{
    for( int n = 0; n < WOLF_MAX_ACTIVE_ACTORS; n++ )
      if( wolfActors[ n ].type != WOLF_ACTORTYPE_EMPTY && wolfActors[ n ].hp > 0
       && wolfActorIsPlayerColliding( n ) )
        return true;

    if( wolfIsPointColliding( wolfPlayerX - WOLF_MIN_WALL_DISTANCE, wolfPlayerZ - WOLF_MIN_WALL_DISTANCE ) ) return true;
    if( wolfIsPointColliding( wolfPlayerX + WOLF_MIN_WALL_DISTANCE, wolfPlayerZ - WOLF_MIN_WALL_DISTANCE ) ) return true;
    if( wolfIsPointColliding( wolfPlayerX + WOLF_MIN_WALL_DISTANCE, wolfPlayerZ + WOLF_MIN_WALL_DISTANCE ) ) return true;
    if( wolfIsPointColliding( wolfPlayerX - WOLF_MIN_WALL_DISTANCE, wolfPlayerZ + WOLF_MIN_WALL_DISTANCE ) ) return true;

    return false;
}

void wolfPlayerMove( int deltaX, int deltaZ )
{
    wolfPlayerX = wolfPlayerX + deltaX;
    wolfPlayerZ = wolfPlayerZ + deltaZ;

    if( wolfIsPlayerColliding() )
    {
        wolfPlayerZ = wolfPlayerZ - deltaZ;

        if( wolfIsPlayerColliding() )
        {
            wolfPlayerX = wolfPlayerX - deltaX;
            wolfPlayerZ = wolfPlayerZ + deltaZ;

            if( wolfIsPlayerColliding() )
              wolfPlayerZ = wolfPlayerZ - deltaZ;
        }
    }
}

void wolfShootWeapon()
{
    int rotCos, rotSin;
    int closestActor = -1;
    int actorDistance = 0;
    bool missed = false;

    wolfPlaySound( WOLF_SOUND_ATTACKPISTOL );

    if( wolfWeaponType != WOLF_WEAPON_KNIFE )
      wolfWeaponAmmo = WOLF_U8( wolfWeaponAmmo - 1 );

    rotCos = wolfCos( WOLF_U8( -wolfPlayerDirection ) );
    rotSin = wolfSin( WOLF_U8( -wolfPlayerDirection ) );

    for( int n = 0; n < WOLF_MAX_ACTIVE_ACTORS; n++ )
      if( wolfActors[ n ].type != WOLF_ACTORTYPE_EMPTY && wolfActors[ n ].hp > 0 )
      {
          int zt = WOLF_I16( WOLF_ASR( rotCos * ( wolfActors[ n ].x - wolfPlayerX ), WOLF_FIXED_SHIFT )
                           - WOLF_ASR( rotSin * ( wolfActors[ n ].z - wolfPlayerZ ), WOLF_FIXED_SHIFT ) );
          int xt = WOLF_I16( WOLF_ASR( rotSin * ( wolfActors[ n ].x - wolfPlayerX ), WOLF_FIXED_SHIFT )
                           + WOLF_ASR( rotCos * ( wolfActors[ n ].z - wolfPlayerZ ), WOLF_FIXED_SHIFT ) );

          if( zt > WOLF_CLIP_PLANE
           && xt > -WOLF_ACTOR_HITBOX_SIZE / 2 && xt < WOLF_ACTOR_HITBOX_SIZE / 2
           && ( zt < ( WOLF_CELL_SIZE << WOLF_FIXED_SHIFT ) || wolfWeaponType != WOLF_WEAPON_KNIFE ) )
          {
              if( closestActor == -1 || zt < actorDistance )
              {
                  closestActor = n;
                  actorDistance = zt;
              }
          }
      }

    if( closestActor != -1 )
    {
        if( wolfWeaponType == WOLF_WEAPON_KNIFE )
        {
            wolfActorDamage( closestActor, wolfGetRandomNumber() >> 4 );
        }
        else if( wolfIsClearLine( wolfPlayerX, wolfPlayerZ,
                                  wolfActors[ closestActor ].x, wolfActors[ closestActor ].z ) )
        {
            int dist = wolfGetPlayerCellDistance( closestActor );
            int damage = 0;

            if( dist < 2 )
            {
                damage = wolfGetRandomNumber() / 4;
            }
            else if( dist < 4 )
            {
                damage = wolfGetRandomNumber() / 6;
            }
            else
            {
                // Upstream jumps past the damage application with a `goto`
                // here; reproduced with a flag, since this dialect has no goto.
                if( ( wolfGetRandomNumber() / 12 ) < dist )
                  missed = true;
                else
                  damage = wolfGetRandomNumber() / 6;
            }

            if( !missed )
              wolfActorDamage( closestActor, damage );
        }
    }

    if( wolfWeaponType != WOLF_WEAPON_KNIFE && wolfWeaponAmmo == 0 )
    {
        wolfWeaponType = WOLF_WEAPON_KNIFE;
        wolfWeaponTime = 0;
        wolfWeaponFrame = 0;
        wolfWeaponShooting = false;
    }
}

void wolfUpdateWeapon()
{
    if( wolfInputState & WOLF_IN_B )
    {
        if( !wolfWeaponDebounce )
        {
            wolfWeaponDebounce = true;

            if( wolfWeaponShooting == false )
            {
                wolfWeaponShooting = true;
                wolfWeaponTime = 0;
            }
        }
    }
    else
    {
        wolfWeaponDebounce = false;
    }

    if( !wolfWeaponShooting )
      return;

    wolfWeaponTime = WOLF_U8( wolfWeaponTime + 1 );

    if( wolfWeaponTime == 2 )
    {
        wolfWeaponFrame = 1;
    }
    else if( wolfWeaponTime == 4 )
    {
        wolfWeaponFrame = 2;
        wolfShootWeapon();
    }
    else if( wolfWeaponTime == 6 )
    {
        if( wolfWeaponType == WOLF_WEAPON_MACHINEGUN )
          wolfWeaponFrame = 1;
        else
          wolfWeaponFrame = 3;
    }
    else if( wolfWeaponTime == 8 )
    {
        if( wolfWeaponType == WOLF_WEAPON_MACHINEGUN )
        {
            if( wolfInputState & WOLF_IN_B )
            {
                wolfWeaponTime = 2;
            }
            else
            {
                wolfWeaponFrame = 0;
                wolfWeaponShooting = false;
            }
        }
        else
        {
            wolfWeaponFrame = 1;
        }
    }
    else if( wolfWeaponTime == 10 )
    {
        wolfWeaponFrame = 0;
        wolfWeaponShooting = false;
    }
}

void wolfPlayerUpdate()
{
    int cos_dir = wolfCos( wolfPlayerDirection );
    int sin_dir = wolfSin( wolfPlayerDirection );
    int projectedX, projectedZ;

    if( wolfPlayerHp > 0 )
    {
        bool strafe = ( wolfInputState & WOLF_IN_A ) != 0;
        int movement = WOLF_MOVEMENT;
        int turn = WOLF_TURN;
        int deltaX = 0;
        int deltaZ = 0;
        int cellX, cellZ;

        if( wolfInputState == WOLF_IN_A )
        {
            if( !wolfTicksSinceStrafePressed )
            {
                wolfTicksSinceStrafePressed = 1;
            }
            else if( wolfTicksSinceStrafePressed > 1 )
            {
                wolfTicksSinceStrafePressed = 0;

                if( wolfWeaponType == WOLF_WEAPON_PISTOL )
                {
                    if( wolfHasMachineGun )    wolfWeaponType = WOLF_WEAPON_MACHINEGUN;
                    else if( wolfHasChainGun ) wolfWeaponType = WOLF_WEAPON_CHAINGUN;
                    else                       wolfWeaponType = WOLF_WEAPON_KNIFE;
                }
                else if( wolfWeaponType == WOLF_WEAPON_MACHINEGUN )
                {
                    if( wolfHasChainGun ) wolfWeaponType = WOLF_WEAPON_CHAINGUN;
                    else                  wolfWeaponType = WOLF_WEAPON_KNIFE;
                }
                else if( wolfWeaponType == WOLF_WEAPON_CHAINGUN )
                {
                    wolfWeaponType = WOLF_WEAPON_CHAINGUN;
                }
                else if( wolfWeaponAmmo > 0 )
                {
                    wolfWeaponType = WOLF_WEAPON_PISTOL;
                }
            }
        }
        else if( !wolfInputState )
        {
            if( wolfTicksSinceStrafePressed > 0 )
            {
                wolfTicksSinceStrafePressed++;

                if( wolfTicksSinceStrafePressed > 5 )
                  wolfTicksSinceStrafePressed = 0;
            }
        }
        else
        {
            wolfTicksSinceStrafePressed = 0;
        }

        wolfUpdateWeapon();

        if( wolfInputState & WOLF_IN_DOWN )
        {
            deltaX = deltaX - WOLF_ASR( movement * cos_dir, WOLF_FIXED_SHIFT );
            deltaZ = deltaZ - WOLF_ASR( movement * sin_dir, WOLF_FIXED_SHIFT );
        }

        if( wolfInputState & WOLF_IN_UP )
        {
            deltaX = deltaX + WOLF_ASR( movement * cos_dir, WOLF_FIXED_SHIFT );
            deltaZ = deltaZ + WOLF_ASR( movement * sin_dir, WOLF_FIXED_SHIFT );
        }

        if( wolfInputState & WOLF_IN_LEFT )
        {
            if( strafe )
            {
                deltaX = deltaX + WOLF_ASR( movement * sin_dir, WOLF_FIXED_SHIFT );
                deltaZ = deltaZ - WOLF_ASR( movement * cos_dir, WOLF_FIXED_SHIFT );
            }
            else
            {
                wolfPlayerDirection = WOLF_U8( wolfPlayerDirection - turn );
            }
        }

        if( wolfInputState & WOLF_IN_RIGHT )
        {
            if( strafe )
            {
                deltaX = deltaX - WOLF_ASR( movement * sin_dir, WOLF_FIXED_SHIFT );
                deltaZ = deltaZ + WOLF_ASR( movement * cos_dir, WOLF_FIXED_SHIFT );
            }
            else
            {
                wolfPlayerDirection = WOLF_U8( wolfPlayerDirection + turn );
            }
        }

        wolfPlayerMove( deltaX, deltaZ );

        cellX = WOLF_ASR( wolfPlayerX, WOLF_CELL_SIZE_SHIFT );
        cellZ = WOLF_ASR( wolfPlayerZ, WOLF_CELL_SIZE_SHIFT );

        wolfOpenDoorsAt( cellX, cellZ, WOLF_DIRECTION_NONE );

        if( wolfAbs( cos_dir ) > wolfAbs( sin_dir ) )
        {
            if( cos_dir > 0 ) wolfOpenDoorsAt( cellX + 1, cellZ, WOLF_DIRECTION_EAST );
            else              wolfOpenDoorsAt( cellX - 1, cellZ, WOLF_DIRECTION_WEST );
        }
        else
        {
            if( sin_dir > 0 ) wolfOpenDoorsAt( cellX, cellZ + 1, WOLF_DIRECTION_SOUTH );
            else              wolfOpenDoorsAt( cellX, cellZ - 1, WOLF_DIRECTION_NORTH );
        }

        // Collect any items
        for( int n = 0; n < WOLF_MAX_ACTIVE_ITEMS; n++ )
          if( wolfItems[ n ].type != 0 && wolfItems[ n ].x == cellX && wolfItems[ n ].z == cellZ )
          {
              bool collected = true;
              int type = wolfItems[ n ].type;

              if( type == WOLF_TILE_ITEM_MACHINEGUN )
              {
                  wolfWeaponAmmo = wolfWeaponAmmo + 8;
                  if( wolfWeaponAmmo > 99 ) wolfWeaponAmmo = 99;

                  wolfPlaySound( WOLF_SOUND_COLLECTAMMO );
                  wolfWeaponType = WOLF_WEAPON_MACHINEGUN;
                  wolfHasMachineGun = true;
              }
              else if( type == WOLF_TILE_ITEM_CLIP )
              {
                  if( wolfWeaponAmmo < 99 )
                  {
                      if( wolfWeaponAmmo == 0 && wolfWeaponType == WOLF_WEAPON_KNIFE )
                        wolfWeaponType = WOLF_WEAPON_PISTOL;

                      wolfWeaponAmmo = wolfWeaponAmmo + 8;
                      if( wolfWeaponAmmo > 99 ) wolfWeaponAmmo = 99;
                  }
                  else
                  {
                      collected = false;
                  }
              }
              else if( type == WOLF_TILE_ITEM_FIRSTAID )
              {
                  if( wolfPlayerHp < 100 )
                  {
                      wolfPlayerHp = wolfPlayerHp + 25;
                      if( wolfPlayerHp > 100 ) wolfPlayerHp = 100;
                  }
                  else collected = false;
              }
              else if( type == WOLF_TILE_ITEM_FOOD )
              {
                  if( wolfPlayerHp < 100 )
                  {
                      wolfPlayerHp = wolfPlayerHp + 10;
                      if( wolfPlayerHp > 100 ) wolfPlayerHp = 100;
                  }
                  else collected = false;
              }

              if( collected )
              {
                  wolfPlaySound( WOLF_SOUND_COLLECTAMMO );
                  wolfItems[ n ].type = 0;
                  wolfMarkItemCollected( wolfItems[ n ].spawnId );
              }
          }
    }
    else
    {
        int rotCos = wolfCos( WOLF_U8( -wolfPlayerDirection ) );
        int rotSin = wolfSin( WOLF_U8( -wolfPlayerDirection ) );
        int killer = wolfPlayerKiller;
        int xt = WOLF_I16( WOLF_ASR( rotSin * ( wolfActors[ killer ].x - wolfPlayerX ), WOLF_FIXED_SHIFT )
                         + WOLF_ASR( rotCos * ( wolfActors[ killer ].z - wolfPlayerZ ), WOLF_FIXED_SHIFT ) );
        int newXt;

        if( xt > 0 ) wolfPlayerDirection = WOLF_U8( wolfPlayerDirection + WOLF_TURN );
        else         wolfPlayerDirection = WOLF_U8( wolfPlayerDirection - WOLF_TURN );

        rotCos = wolfCos( WOLF_U8( -wolfPlayerDirection ) );
        rotSin = wolfSin( WOLF_U8( -wolfPlayerDirection ) );

        newXt = WOLF_I16( WOLF_ASR( rotSin * ( wolfActors[ killer ].x - wolfPlayerX ), WOLF_FIXED_SHIFT )
                        + WOLF_ASR( rotCos * ( wolfActors[ killer ].z - wolfPlayerZ ), WOLF_FIXED_SHIFT ) );

        if( ( xt < 0 && newXt >= 0 ) || ( xt > 0 && newXt <= 0 ) )
        {
            wolfGameState = WOLF_STATE_DEAD;
            wolfEngineFrameCount = 0;
        }

        wolfDamageIndicator = 5;
    }

    // Update the stream position
    projectedX = WOLF_ASR( wolfPlayerX, WOLF_CELL_SIZE_SHIFT ) + cos_dir / 19;
    projectedZ = WOLF_ASR( wolfPlayerZ, WOLF_CELL_SIZE_SHIFT ) + sin_dir / 19;

    wolfUpdateBufferPosition( projectedX - WOLF_MAP_BUFFER_SIZE / 2,
                              projectedZ - WOLF_MAP_BUFFER_SIZE / 2 );
}

void wolfPlayerInit()
{
    if( wolfPlayerHp == 0 )
    {
        wolfWeaponType = WOLF_WEAPON_PISTOL;
        wolfWeaponAmmo = 8;
        wolfPlayerHp = 100;
        wolfHasMachineGun = 0;
        wolfHasChainGun = 0;
        wolfHasKey1 = 0;
        wolfHasKey2 = 0;
    }

    wolfWeaponFrame = 0;
    wolfWeaponDebounce = false;

    // Find the player start tile
    for( int j = 0; j < WOLF_MAP_SIZE; j = j + WOLF_MAP_BUFFER_SIZE )
      for( int i = 0; i < WOLF_MAP_SIZE; i = i + WOLF_MAP_BUFFER_SIZE )
      {
          wolfUpdateBufferPosition( i, j );

          for( int a = 0; a < WOLF_MAP_BUFFER_SIZE; a++ )
            for( int b = 0; b < WOLF_MAP_BUFFER_SIZE; b++ )
            {
                int tile = wolfGetTileFast( b, a );

                if( tile >= WOLF_TILE_PLAYERSTART_NORTH && tile <= WOLF_TILE_PLAYERSTART_WEST )
                {
                    wolfPlayerX = ( ( i + b ) << WOLF_CELL_SIZE_SHIFT ) + WOLF_CELL_SIZE / 2;
                    wolfPlayerZ = ( ( j + a ) << WOLF_CELL_SIZE_SHIFT ) + WOLF_CELL_SIZE / 2;
                    wolfPlayerDirection =
                      WOLF_U8( ( tile - WOLF_TILE_PLAYERSTART_NORTH - 1 ) * WOLF_DEGREES_90 );
                }
            }
      }
}

// ---------------------------------------------------------------------------
// Menu - upstream's own const-void* menu tables, restructured into explicit
// id-driven tables (mixing string and function pointers in one array has no
// safe equivalent here).
// ---------------------------------------------------------------------------

#define WOLF_MENU_MAIN 0
#define WOLF_MENU_PAUSED 1
#define WOLF_MENU_DIFFICULTY 2

#define WOLF_ACT_NEWGAME 0
#define WOLF_ACT_SOUND 1
#define WOLF_ACT_QUIT 2
#define WOLF_ACT_CONTINUE 3
#define WOLF_ACT_SKILL_BABY 4
#define WOLF_ACT_SKILL_EASY 5
#define WOLF_ACT_SKILL_MEDIUM 6
#define WOLF_ACT_SKILL_HARD 7

int wolfNumMenuItems()
{
    if( wolfCurrentMenu == WOLF_MENU_MAIN ) return 3;
    if( wolfCurrentMenu == WOLF_MENU_PAUSED ) return 4;
    return 4;
}

int* wolfMenuTitle()
{
    if( wolfCurrentMenu == WOLF_MENU_DIFFICULTY )
      return "HOW TOUGH ARE YOU?";

    return "WOLFENDUINO 3D";
}

int* wolfMenuLabel( int index )
{
    if( wolfCurrentMenu == WOLF_MENU_MAIN )
    {
        if( index == 0 ) return "NEW GAME";
        if( index == 1 ) return "SOUND:";
        return "QUIT";
    }

    if( wolfCurrentMenu == WOLF_MENU_PAUSED )
    {
        if( index == 0 ) return "CONTINUE";
        if( index == 1 ) return "NEW GAME";
        if( index == 2 ) return "SOUND:";
        return "QUIT";
    }

    if( index == 0 ) return "CAN I PLAY, DADDY?";
    if( index == 1 ) return "DON'T HURT ME.";
    if( index == 2 ) return "BRING 'EM ON!";
    return "I AM DEATH\n  INCARNATE!";
}

int wolfMenuAction( int index )
{
    if( wolfCurrentMenu == WOLF_MENU_MAIN )
    {
        if( index == 0 ) return WOLF_ACT_NEWGAME;
        if( index == 1 ) return WOLF_ACT_SOUND;
        return WOLF_ACT_QUIT;
    }

    if( wolfCurrentMenu == WOLF_MENU_PAUSED )
    {
        if( index == 0 ) return WOLF_ACT_CONTINUE;
        if( index == 1 ) return WOLF_ACT_NEWGAME;
        if( index == 2 ) return WOLF_ACT_SOUND;
        return WOLF_ACT_QUIT;
    }

    if( index == 0 ) return WOLF_ACT_SKILL_BABY;
    if( index == 1 ) return WOLF_ACT_SKILL_EASY;
    if( index == 2 ) return WOLF_ACT_SKILL_MEDIUM;
    return WOLF_ACT_SKILL_HARD;
}

// True for the entry that shows the ON/OFF sound state beside its label.
bool wolfMenuIsSoundEntry( int index )
{
    if( wolfCurrentMenu == WOLF_MENU_MAIN ) return index == 1;
    if( wolfCurrentMenu == WOLF_MENU_PAUSED ) return index == 2;
    return false;
}

void wolfSwitchMenu( int newMenu )
{
    wolfCurrentMenu = newMenu;
    wolfCurrentSelection = 0;
    wolfDebounceInput = true;
}

void wolfStartLevel( bool resetPlayer )
{
    if( resetPlayer )
      wolfPlayerHp = 0;

    wolfGameState = WOLF_STATE_STARTINGLEVEL;
}

void wolfMenuDraw()
{
    int index = 0;
    int y = 12;
    int count = wolfNumMenuItems();

    wolfClearDisplay( 1 );
    wolfDrawString( wolfMenuTitle(), 5, 1 );

    while( index < count )
    {
        wolfDrawString( wolfMenuLabel( index ), 8, y );

        if( wolfMenuIsSoundEntry( index ) )
        {
            if( wolfMuted ) wolfDrawString( "OFF", 40, y );
            else            wolfDrawString( "ON", 40, y );
        }

        index++;
        y = y + 6;
    }

    wolfDrawGlyph( 42 - WOLF_FIRST_FONT_GLYPH, 2, 12 + wolfCurrentSelection * 6 );
}

void wolfMenuUpdate()
{
    if( !wolfDebounceInput )
    {
        if( wolfInputState & WOLF_IN_UP )
        {
            wolfCurrentSelection--;

            if( wolfCurrentSelection == -1 )
              wolfCurrentSelection = wolfNumMenuItems() - 1;
        }

        if( wolfInputState & WOLF_IN_DOWN )
        {
            wolfCurrentSelection++;

            if( wolfCurrentSelection == wolfNumMenuItems() )
              wolfCurrentSelection = 0;
        }

        if( wolfInputState & WOLF_IN_A )
        {
            int action = wolfMenuAction( wolfCurrentSelection );

            if( action == WOLF_ACT_NEWGAME )
            {
                wolfSwitchMenu( WOLF_MENU_DIFFICULTY );
            }
            else if( action == WOLF_ACT_SOUND )
            {
                wolfMuted = !wolfMuted;
            }
            else if( action == WOLF_ACT_QUIT )
            {
                // Upstream flashes the SD-card loader here; no equivalent
                // exists on this platform, so the entry is inert.
            }
            else if( action == WOLF_ACT_CONTINUE )
            {
                wolfGameState = WOLF_STATE_PLAYING;
            }
            else if( action == WOLF_ACT_SKILL_BABY )
            {
                wolfDifficulty = WOLF_DIFF_BABY;
                wolfStartLevel( true );
            }
            else if( action == WOLF_ACT_SKILL_EASY )
            {
                wolfDifficulty = WOLF_DIFF_EASY;
                wolfStartLevel( true );
            }
            else if( action == WOLF_ACT_SKILL_MEDIUM )
            {
                wolfDifficulty = WOLF_DIFF_MEDIUM;
                wolfStartLevel( true );
            }
            else if( action == WOLF_ACT_SKILL_HARD )
            {
                wolfDifficulty = WOLF_DIFF_HARD;
                wolfStartLevel( true );
            }
        }

        if( wolfInputState & WOLF_IN_C )
        {
            if( wolfGameState == WOLF_STATE_PAUSEMENU )
              wolfGameState = WOLF_STATE_PLAYING;
        }

        if( wolfInputState & WOLF_IN_B )
        {
            if( wolfCurrentMenu == WOLF_MENU_DIFFICULTY )
            {
                if( wolfGameState == WOLF_STATE_MENU )
                  wolfSwitchMenu( WOLF_MENU_MAIN );
                else
                  wolfSwitchMenu( WOLF_MENU_PAUSED );
            }
            else if( wolfCurrentMenu == WOLF_MENU_PAUSED )
            {
                wolfGameState = WOLF_STATE_PLAYING;
            }
        }
    }

    wolfDebounceInput = wolfInputState != 0;
}

// ---------------------------------------------------------------------------
// Engine - direct port of upstream's own Engine.cpp
// ---------------------------------------------------------------------------

void wolfStartingLevel()
{
    wolfGameState = WOLF_STATE_LOADING;

    for( int n = 0; n < WOLF_MAX_ACTIVE_ACTORS; n++ )
    {
        wolfActors[ n ].type = WOLF_ACTORTYPE_EMPTY;
        wolfActors[ n ].spawnId = 255;
    }

    wolfMapInit();
    wolfPlayerInit();
    wolfPlayerUpdate(); // To update the streaming position for the first frame

    wolfEngineFrameCount = 0;
    wolfGameState = WOLF_STATE_PLAYING;
}

void wolfEngineUpdate()
{
    if( wolfGameState == WOLF_STATE_PLAYING )
    {
        wolfPlayerUpdate();

        if( wolfPlayerHp > 0 )
        {
            wolfMapUpdate();

            for( int n = 0; n < WOLF_MAX_ACTIVE_ACTORS; n++ )
              wolfActorUpdate( n );
        }

        if( wolfInputState & WOLF_IN_C )
        {
            wolfGameState = WOLF_STATE_PAUSEMENU;
            wolfSwitchMenu( WOLF_MENU_PAUSED );
        }
    }
    else if( wolfGameState == WOLF_STATE_MENU || wolfGameState == WOLF_STATE_PAUSEMENU )
    {
        wolfMenuUpdate();
    }
    else if( wolfGameState == WOLF_STATE_FINISHEDLEVEL )
    {
        wolfCurrentLevel++;

        // Upstream never bounds this, because on the real cartridge the level
        // number was ignored entirely (see the header comment) and on the
        // desktop build nobody reached the end. With the levels genuinely
        // wired up, running off the end of the payload would stream an empty
        // map with no walls and no player start, so the set wraps instead.
        if( wolfCurrentLevel * WOLF_LEVEL_BYTES >= WOLF_MAP_BYTES )
          wolfCurrentLevel = 0;

        wolfStartLevel( false );
    }
    else if( wolfGameState == WOLF_STATE_STARTINGLEVEL )
    {
        wolfStartingLevel();
    }
    else if( wolfGameState == WOLF_STATE_DEAD )
    {
        if( wolfEngineFrameCount >= 30 )
          wolfStartLevel( true );
    }

    wolfEngineFrameCount++;
}

void wolfEngineDraw()
{
    if( wolfGameState == WOLF_STATE_MENU || wolfGameState == WOLF_STATE_PAUSEMENU )
    {
        wolfMenuDraw();
    }
    else if( wolfGameState == WOLF_STATE_PLAYING )
    {
        wolfDrawFrame();
    }
    else if( wolfGameState == WOLF_STATE_STARTINGLEVEL || wolfGameState == WOLF_STATE_LOADING )
    {
        wolfClearDisplay( 1 );
        wolfDrawString( "GET PSYCHED!", 20, 21 );
    }
    else if( wolfGameState == WOLF_STATE_DEAD )
    {
        if( wolfEngineFrameCount < 30 )
        {
            // Restore the frozen frame this effect paints over - the one place
            // upstream's own display persistence is load-bearing.
            for( int i = 0; i < 504; i++ )
              gbFrameBuffer[ i ] = wolfDeathBuffer[ i ];

            for( int n = 0; n < WOLF_DISPLAYWIDTH * ( WOLF_DISPLAYHEIGHT / 15 ); n++ )
            {
                int x = WOLF_U8( n + wolfGetRandomNumber16() + wolfGetRandomNumber16() ) % WOLF_DISPLAYWIDTH;
                int y = WOLF_U8( n + wolfGetRandomNumber16() + wolfGetRandomNumber16() ) % WOLF_DISPLAYHEIGHT;

                wolfSetPixel( x, y );
            }
        }
        else
        {
            wolfClearDisplay( 0 );
        }
    }

    // Keep a copy of the finished frame so the death effect above can paint
    // over it next tick.
    for( int i = 0; i < 504; i++ )
      wolfDeathBuffer[ i ] = gbFrameBuffer[ i ];
}

// ---------------------------------------------------------------------------
// Entry points
// ---------------------------------------------------------------------------

void wolfReadInput()
{
    wolfInputState = 0;

    if( gbHeld( BTN_A, 1 ) )     wolfInputState = wolfInputState | WOLF_IN_A;
    if( gbHeld( BTN_B, 1 ) )     wolfInputState = wolfInputState | WOLF_IN_B;
    if( gbHeld( BTN_C, 1 ) )     wolfInputState = wolfInputState | WOLF_IN_C;
    if( gbHeld( BTN_UP, 1 ) )    wolfInputState = wolfInputState | WOLF_IN_UP;
    if( gbHeld( BTN_RIGHT, 1 ) ) wolfInputState = wolfInputState | WOLF_IN_RIGHT;
    if( gbHeld( BTN_DOWN, 1 ) )  wolfInputState = wolfInputState | WOLF_IN_DOWN;
    if( gbHeld( BTN_LEFT, 1 ) )  wolfInputState = wolfInputState | WOLF_IN_LEFT;
}

void gameWolfenduino_init()
{
    gbBegin();
    gbSetFrameRate( 30 );

    wolfRandVal = 0xABC;
    wolfMuted = 0;
    wolfInputState = 0;

    wolfPlayerHp = 0;
    wolfPlayerX = 0;
    wolfPlayerZ = 0;
    wolfPlayerDirection = 0;
    wolfPlayerKiller = 0;
    wolfTicksSinceStrafePressed = 0;
    wolfWeaponType = WOLF_WEAPON_PISTOL;
    wolfWeaponAmmo = 8;
    wolfWeaponFrame = 0;
    wolfWeaponTime = 0;
    wolfWeaponDebounce = 0;
    wolfWeaponShooting = 0;
    wolfHasMachineGun = 0;
    wolfHasChainGun = 0;
    wolfHasKey1 = 0;
    wolfHasKey2 = 0;

    wolfDamageIndicator = 0;
    wolfRenderQueueHead = WOLF_NULL_QUEUE_ITEM;

    for( int n = 0; n < WOLF_MAX_ACTIVE_ACTORS; n++ )
    {
        wolfActors[ n ].type = WOLF_ACTORTYPE_EMPTY;
        wolfActors[ n ].spawnId = 255;
        wolfActors[ n ].hp = 0;
        wolfActors[ n ].frozen = 1;
        wolfActors[ n ].persistent = 0;
    }

    for( int n = 0; n < WOLF_MAX_DOORS; n++ )
      wolfDoors[ n ].type = WOLF_DOORTYPE_NONE;

    for( int n = 0; n < WOLF_MAX_ACTIVE_ITEMS; n++ )
      wolfItems[ n ].type = 0;

    for( int i = 0; i < 504; i++ )
      wolfDeathBuffer[ i ] = 0;

    // Upstream's own Map::initStreaming()
    wolfMapLoaded = false;
    wolfBufferX = 0;
    wolfBufferZ = 0;

    wolfSwitchMenu( WOLF_MENU_MAIN );
    wolfDifficulty = WOLF_DIFF_MEDIUM;
    wolfGameState = WOLF_STATE_MENU;
    wolfCurrentLevel = 0;
    wolfEngineFrameCount = 0;
}

void gameWolfenduino_update()
{
    if( !gbUpdate() ) return;

    wolfReadInput();
    wolfEngineUpdate();
    wolfEngineDraw();

    gbRenderFrame();
}
