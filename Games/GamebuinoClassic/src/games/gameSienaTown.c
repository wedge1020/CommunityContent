// Siena Town (Patryk Kuciński "Kuciniak", kucinskipatryk.site, 2019, no
// license specified) - a western quick-draw game. A sheriff holds the left
// edge of a three-lane street; a bandit fires from the right and a civilian
// wanders between the same three lanes. Up/Down change lane, Button A
// shoots: line up with the bandit for a point, hit the lane the civilian is
// standing in and lose a life instead. Incoming bullets cost a life if they
// reach your lane. Three lives, then a game-over screen.
//
// Every real `gb.x.y(...)` call site is mechanically rewritten to a plain
// `gbY(...)` function call (see gamePong.c's own header comment for why -
// this dialect has no classes/methods/operator overloading). Every global
// symbol got a `sien`-prefixed name (this cartridge has no linker - every
// ported game shares one flat global namespace). Upstream's own
// `rand() % n` calls became `arand(n)`, and its four 8x8 sprites plus the
// 64x36 title logo were converted from their own `0b`-binary and hex PROGMEM
// tables to hex byte-for-byte by script, not hand-transcribed.
//
// BLOCKING TITLE SCREEN -> AN EXPLICIT STATE: real `gb.titleScreen(F("
// Siena Town"), Logo)` blocks until Button A. Hand-rolled here as one more
// explicit state drawing the game's own real logo.
//
// THE LIVES READOUT is drawn by printing ASCII 3 once per remaining life -
// one of real Gamebuino's own low-ASCII icon glyphs (a heart), which a
// quoted string literal in this dialect cannot hold. Built as an explicit
// 0-terminated int array instead, matching the treatment gameTaquin.c/
// gameSimonbuino.c already established for the same gap.
//
// A REAL, GAME-DEFINING UPSTREAM BUG, FIXED ON DIRECT REQUEST: `loop()`
// opens with `Topscore = 0;` on EVERY SINGLE TICK, immediately overwriting
// the value read out of EEPROM at startup. On real hardware that means the
// "Top Score" menu entry always reads 0, and the game-over test
// `Points > Topscore` is always true - every run ends on "New record!" no
// matter how badly it went. The score really was written to EEPROM; it was
// simply wiped again before anything could ever read it back.
//
// That single line is removed here, so the stored top score survives, the
// Top Score screen shows it, and "New record!" only appears on a genuine
// one. Everything else about the scoring path is upstream's own, untouched:
// the EEPROM address, the read at startup, the write on game over and the
// `Record` latch. Initially preserved as real cartridge behaviour, then
// fixed when the user reported it - see BUGS.md.
//
// OTHER REAL UPSTREAM QUIRKS, PRESERVED:
// - The civilian and the bandit are re-rolled by independent random chances
//   (`rand() % 30 == 1` and `rand() % 24 == 2`) evaluated every tick, so
//   both can teleport lanes mid-flight, including into the lane you are
//   already aiming at.
// - The incoming bullet's own hit test is `ShootPosX == 6`, an exact
//   equality against a position that decrements by one each tick, so it can
//   only ever register at that precise pixel column.
// - Shooting is tested with `SheriffPosY == BanditPosY * 10`, comparing the
//   sheriff's own pixel row against the bandit's lane index times ten -
//   which works only because the sheriff's three positions are exactly 10,
//   20 and 30.
// - The game-over branch resets the sheriff, bandit and lives every tick it
//   is displayed rather than once on entry.
//
// A BOUNDED RETRY: upstream re-rolls the bandit's lane with
// `while (BanditPos == BanditPosY) BanditPosY = rand() % 3 + 1;` - an
// unbounded spin that only ends when the roll differs. On real AVR that
// always resolves quickly; here an unbounded loop is a genuine hang risk if
// the generator ever repeated, so it is capped at 100 attempts with the last
// roll kept. In practice it exits on the first or second try, exactly as on
// real hardware.

#include "../gamebuinoShim.h"

#define SIEN_STATE_TITLE 0
#define SIEN_STATE_GAME  1

// Real hardware stores the top score in EEPROM address 0.
#define SIEN_EEPROM_ADDR 0

// Real upstream's own sprites and title logo, converted from their own
// 0b-binary / hex PROGMEM tables to hex byte-for-byte by script.

// Logo: 64x36
int[290] sienLogoBitmap =
{
    0x40, 0x24, 0x00, 0x00, 0x3F, 0xF8, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x3F, 0xF8, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x3F, 0xFC, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x3F, 0xFC, 0x00, 0x00,
    0xC0, 0x00, 0x00, 0x00, 0x7F, 0xFC, 0x00, 0x00,
    0x40, 0x00, 0x00, 0x03, 0xFF, 0xFF, 0xFF, 0x00,
    0x40, 0x00, 0x00, 0x07, 0xFF, 0xFF, 0xFF, 0x80,
    0xC0, 0x00, 0x00, 0x07, 0xFF, 0xFF, 0xFF, 0xE3,
    0x80, 0x00, 0x00, 0x0F, 0xFF, 0xFF, 0xFF, 0xF2,
    0x00, 0x00, 0x00, 0x1F, 0xFF, 0xFF, 0xFF, 0xF2,
    0x00, 0x00, 0x00, 0x1F, 0xFF, 0xFF, 0xFF, 0xF3,
    0x00, 0x00, 0x00, 0x1F, 0xFF, 0xFF, 0xFF, 0xF1,
    0x80, 0x00, 0x00, 0x1F, 0xFF, 0xFF, 0xFF, 0xF1,
    0x80, 0x00, 0x00, 0x0F, 0xF8, 0x7F, 0xB0, 0x01,
    0x00, 0x00, 0x00, 0x00, 0x18, 0x00, 0x10, 0x07,
    0x00, 0x00, 0x00, 0x00, 0x10, 0x00, 0x18, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x13, 0xCF, 0x08, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x19, 0x86, 0x08, 0x3E,
    0x00, 0x00, 0x00, 0x00, 0x08, 0x00, 0x08, 0x12,
    0x00, 0x00, 0x00, 0x00, 0x08, 0x20, 0x08, 0x12,
    0x00, 0x00, 0x00, 0x00, 0x08, 0x30, 0x18, 0x12,
    0x00, 0x00, 0x00, 0x00, 0x1C, 0x00, 0x38, 0x12,
    0xC0, 0x00, 0x00, 0x00, 0x17, 0xFF, 0xF8, 0x13,
    0x60, 0x00, 0x00, 0x00, 0x18, 0x00, 0x08, 0x13,
    0xE0, 0x00, 0x00, 0x00, 0x08, 0x00, 0x08, 0x13,
    0xF0, 0x00, 0x00, 0x00, 0x0B, 0x24, 0x88, 0x17,
    0x90, 0x00, 0x00, 0x00, 0x08, 0x60, 0x88, 0x17,
    0x90, 0x00, 0x00, 0x00, 0x08, 0x00, 0x88, 0x1F,
    0xF0, 0x00, 0x00, 0x00, 0x0B, 0x02, 0x08, 0x0F,
    0xC0, 0x00, 0x00, 0x00, 0x09, 0x32, 0x0C, 0x0F,
    0xC0, 0x00, 0x00, 0x00, 0x08, 0x00, 0x44, 0x0F,
    0xC0, 0x00, 0x00, 0x00, 0xF8, 0x40, 0x6F, 0x8F,
    0xC0, 0x00, 0x00, 0x01, 0x8C, 0x64, 0x1C, 0xCF,
    0xC0, 0x00, 0x00, 0x03, 0x04, 0x06, 0x30, 0x7F,
    0x80, 0x00, 0x00, 0x06, 0x06, 0x00, 0xE0, 0x3F,
    0x00, 0x00, 0x00, 0x0C, 0x02, 0x01, 0x80, 0x7F,
    0x80, 0x00
};

// Sheriff: 8x8
int[10] sienSheriffBitmap =
{
    0x08, 0x08, 0x70, 0xF8, 0x70, 0x2E, 0x38, 0x20,
    0x50, 0x50
};

// Bandit: 8x8
int[10] sienBanditBitmap =
{
    0x08, 0x08, 0x38, 0x38, 0x38, 0x08, 0xF0, 0x50,
    0x38, 0x44
};

// Civilian: 8x8
int[10] sienCivilianBitmap =
{
    0x08, 0x08, 0x38, 0xBA, 0x92, 0x7C, 0x10, 0x38,
    0x44, 0x44
};

// Shoot: 8x8
int[10] sienShootBitmap =
{
    0x08, 0x08, 0x00, 0x00, 0x00, 0x00, 0x40, 0x00,
    0x00, 0x00
};

// "\3" - one real heart icon glyph per remaining life.
int[2] sienHeartGlyph = { 3, 0 };

int sienSheriffPosY;
int sienBanditPosY;
int sienCivilianPosY;
int sienBanditPos;
bool sienBanditActive;
bool sienCivilianActive;
int sienLives;
int sienPoints;
int sienShootPosY;
int sienShootPosX;
bool sienBullet;
int sienChoice;
int sienMenu;
int sienTopscore;
bool sienRecord;
int sienState;

// Stands in for the blocking gb.titleScreen(F("     Siena Town"), Logo) call.
void sienUpdateTitle()
{
    gbDrawBitmap( ( LCDWIDTH - 64 ) / 2, 2, sienLogoBitmap );

    gbCursorX = 24;
    gbCursorY = 42;
    gbPrintString( "PRESS A" );

    if( gbPressed( BTN_A ) )
    {
        gbPlayOK();
        sienState = SIEN_STATE_GAME;
    }
}

void sienUpdateGame()
{
    // Upstream has `Topscore = 0;` here, wiping the EEPROM-loaded value on
    // every single tick - removed, see the header comment.

    // MENU Choice
    if( sienChoice == 0 )
    {
        if( gbPressed( BTN_UP ) )
        {
            sienMenu--;

            if( sienMenu < 1 )
              sienMenu = 4;
        }

        if( gbPressed( BTN_DOWN ) )
        {
            sienMenu++;

            if( sienMenu > 4 )
              sienMenu = 1;
        }

        gbPrintString( "        Menu\n" );
        gbPrintString( "\n" );

        if( sienMenu == 1 ) gbPrintString( "  New Game\n" );
        else                gbPrintString( " New Game\n" );

        if( sienMenu == 2 ) gbPrintString( "  Instruction\n" );
        else                gbPrintString( " Instruction\n" );

        if( sienMenu == 3 ) gbPrintString( "  Credits\n" );
        else                gbPrintString( " Credits\n" );

        if( sienMenu == 4 ) gbPrintString( "  Top Score\n" );
        else                gbPrintString( " Top Score\n" );

        if( gbPressed( BTN_A ) )
          sienChoice = sienMenu;
    }

    // Instruction
    if( sienChoice == 2 )
    {
        gbPrintString( "     Instruction\n" );
        gbPrintString( "\n" );
        gbPrintString( " Kill bandits\n" );
        gbPrintString( " Don't kill civilians\n" );
        gbPrintString( " Earn points\n" );
        gbPrintString( " Don't get killed\n" );

        if( gbPressed( BTN_B ) )
          sienChoice = 0;
    }

    // Credits
    if( sienChoice == 3 )
    {
        gbPrintString( "       Credits\n" );
        gbPrintString( "     Siena Town\n" );
        gbPrintString( "\n" );
        gbPrintString( " Patryk Kucinski\n" );
        gbPrintString( " 'Kuciniak'\n" );
        gbPrintString( " 2019\n" );

        if( gbPressed( BTN_B ) )
          sienChoice = 0;
    }

    // Top Score
    if( sienChoice == 4 )
    {
        gbPrintString( "      Top Score\n" );
        gbPrintString( "\n" );
        gbPrintString( " Score:" );
        gbPrintNumber( sienTopscore );

        if( gbPressed( BTN_B ) )
          sienChoice = 0;
    }

    // GAME
    if( sienChoice == 1 )
    {
        int draw = 0;
        int attempts;

        gbDrawBitmap( 2, sienSheriffPosY, sienSheriffBitmap );

        gbPrintNumber( sienPoints );
        gbPrintString( "  " );

        // LIVES
        while( draw < sienLives )
        {
            gbPrintString( sienHeartGlyph );
            draw++;
        }

        // Control
        if( gbPressed( BTN_UP ) )
        {
            sienSheriffPosY = sienSheriffPosY - 10;

            if( sienSheriffPosY < 10 )
              sienSheriffPosY = 10;
        }

        if( gbPressed( BTN_DOWN ) )
        {
            sienSheriffPosY = sienSheriffPosY + 10;

            if( sienSheriffPosY > 30 )
              sienSheriffPosY = 30;
        }

        // Bandit move
        if( sienBanditActive == false )
        {
            sienBanditPos = sienBanditPosY;
            attempts = 0;

            // Bounded retry - see the header comment.
            while( sienBanditPos == sienBanditPosY && attempts < 100 )
            {
                sienBanditPosY = 1 + arand( 3 );
                attempts++;
            }

            sienBanditActive = true;
        }

        // Bandit draw
        gbDrawBitmap( 72, sienBanditPosY * 10, sienBanditBitmap );

        if( arand( 30 ) == 1 )
          sienCivilianActive = false;

        // Bullet move
        if( sienBullet == false )
        {
            sienShootPosY = sienBanditPosY;
            sienBullet = true;
        }

        gbDrawBitmap( sienShootPosX, sienShootPosY * 10, sienShootBitmap );

        sienShootPosX--;

        // Bullet detection
        if( sienShootPosX == 6 && sienShootPosY * 10 == sienSheriffPosY && sienBullet == true )
        {
            sienLives = sienLives - 1;
            sienBullet = false;
            sienShootPosX = 68;
        }

        if( sienShootPosX == 0 && sienBullet == true )
        {
            sienBullet = false;
            sienShootPosX = 68;
        }

        // Civil move
        if( sienCivilianActive == false )
        {
            sienCivilianPosY = 1 + arand( 3 );
            sienCivilianActive = true;
        }

        // Civil draw
        gbDrawBitmap( 50, sienCivilianPosY * 10, sienCivilianBitmap );

        if( arand( 24 ) == 2 )
          sienBanditActive = false;

        // Shoot detection
        if( gbPressed( BTN_A ) && sienSheriffPosY == sienBanditPosY * 10
         && sienSheriffPosY != sienCivilianPosY * 10 )
        {
            gbPopup( "Shoot!", 20 );
            sienPoints = sienPoints + 1;
            sienBanditActive = false;
            gbPlayTick();
        }

        if( gbPressed( BTN_A ) && sienSheriffPosY == sienCivilianPosY * 10 )
        {
            gbPopup( "Civilian down!", 20 );
            sienLives = sienLives - 1;
        }

        if( sienLives == 0 )
          sienChoice = 5;
    }
    // Game Over
    else if( sienChoice == 5 )
    {
        if( sienPoints > sienTopscore || sienRecord == true )
        {
            sienTopscore = sienPoints;
            eeprom_write_byte( SIEN_EEPROM_ADDR, sienTopscore );
            sienRecord = true;

            gbPrintString( "      Game Over\n" );
            gbPrintString( "\n" );
            gbPrintString( " New record!\n" );
            gbPrintString( " Your score:" );
        }
        else
        {
            gbPrintString( "      Game Over\n" );
            gbPrintString( "\n" );
            gbPrintString( " Your score:" );
        }

        gbPrintNumber( sienPoints );
        gbPrintString( "\n" );
        gbPrintString( "\n" );
        gbPrintString( " A-Menu\n" );
        gbPrintString( " B-Restart\n" );

        if( gbPressed( BTN_A ) )
        {
            sienChoice = 0;
            sienPoints = 0;
            sienRecord = false;
        }

        if( gbPressed( BTN_B ) )
        {
            sienChoice = 1;
            sienPoints = 0;
            sienRecord = false;
        }

        // Reset values
        sienSheriffPosY = 20;
        sienBanditPosY = 2;
        sienBanditActive = false;
        sienCivilianActive = false;
        sienLives = 3;
    }
}

void gameSienaTown_init()
{
    gbBegin();
    gbPickRandomSeed(); // no-op, see gamebuinoShim.h's own header comment

    sienSheriffPosY = 20;
    sienBanditPosY = 2;
    sienCivilianPosY = 0;
    sienBanditPos = 0;
    sienBanditActive = false;
    sienCivilianActive = false;
    sienLives = 3;
    sienPoints = 0;
    sienShootPosY = 0;
    sienShootPosX = 68;
    sienBullet = false;
    sienChoice = 0;
    sienMenu = 1;
    sienRecord = false;
    sienState = SIEN_STATE_TITLE;

    // Upstream reads this once at global-init time; it is wiped again on
    // every tick (see the header comment), but the read itself is real.
    sienTopscore = eeprom_read_byte( SIEN_EEPROM_ADDR );

    if( sienTopscore == 255 )
      sienTopscore = 0;
}

void gameSienaTown_update()
{
    if( !gbUpdate() ) return;

    if( sienState == SIEN_STATE_TITLE ) sienUpdateTitle();
    else                                sienUpdateGame();

    gbRenderFrame();
}
