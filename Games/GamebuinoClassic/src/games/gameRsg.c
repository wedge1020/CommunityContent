// R.S.G. (deKay - @deKay01, andyk@lofi-gaming.org.uk, no license specified),
// v1.01, (c) 2015 - a deliberately minimal turn-based RPG: twenty
// "Misterbaddiemen" of steadily increasing HP are fought one at a time with
// either a physical attack (Button A) or a magic attack (Button B), until
// either the twentieth one falls or the player's own HP reaches zero.
// Upstream describes it in its own source comment as a "Very simple,
// terrible RPG" - the whole game is four screens of text and two random
// damage rolls, with no map, no items and no movement of any kind.
//
// Every real `gb.x.y(...)` call site is mechanically rewritten to a plain
// `gbY(...)` function call (see gamePong.c's own header comment for why -
// this dialect has no classes/methods/operator overloading). Every global
// symbol got an `rsg`-prefixed name (this cartridge has no linker - every
// ported game shares one flat global namespace). Upstream's own
// `random(n)`/`random(n)+m` calls became `arand(n)`/`arand(n)+m`, this
// project's own established ranged-random rewrite convention (see
// gameFlappyBirdo.c's own header comment) - `arand(0)` returns 0, exactly
// matching real Arduino `random(0)`, which the nested
// `random(3+random(baddienumber/5))` damage roll relies on for the first
// five opponents.
//
// The real 64x36 title logo was converted from upstream's own `B`-binary
// PROGMEM byte table to hex byte-for-byte by script, not hand-transcribed -
// no bit reshuffling was needed, since this shim's own gbDrawBitmap() uses
// the exact same row-major/MSB-first layout real Display::drawBitmap() does.
//
// ATTACK TYPE: upstream's own `attack(String attacktype)` takes an Arduino
// `String` ("a" or "m") and compares it with `==`. This dialect has no
// String class, so the parameter became a plain int flag
// (RSG_ATTACK_PHYSICAL/RSG_ATTACK_MAGIC) - the two call sites are literal
// constants upstream too, so nothing is lost.
//
// THE ARRAY IS ONE ENTRY LONGER THAN UPSTREAM'S: real upstream allocates
// `new int[20]` and indexes it with `baddienumber`, which is incremented to
// exactly 20 the moment the twentieth opponent dies. Tracing every real
// path shows `misterbaddieman[20]` is never actually read once that happens
// (the ATTACK screen sets hebedead=2, BADDIEATTACK draws neither of its two
// branches for that value, and the next Button B goes straight to WINGAME
// without re-entering PLAYGAME's own `displayHUD()`), so real hardware never
// observes the one-past-the-end slot - but a stray read of it here would
// pull an unrelated global rather than harmless adjacent AVR SRAM, so the
// array is declared with 21 entries. Entries 0-19 are the real game;
// entry 20 is never written and never meaningfully read.
//
// BLOCKING TITLE SCREEN -> AN EXPLICIT STATE: real `gb.titleScreen(F(""),
// logo)` is a blocking `while(1)` loop (drawing the Gamebuino boot logo, the
// game's own logo, and real A/B/C hint glyphs, returning only once Button A
// is pressed). Hand-rolled here as one more explicit state drawing the
// game's own real logo plus a "PRESS A" prompt, matching the treatment every
// other port in this cartridge gives that same call (see gameFifteen.c).
//
// DISPLAY PERSISTENCE, DELIBERATELY NOT EMULATED: real upstream sets
// `gb.display.persistence = true` once in `setup()`, and leans on it for two
// screens - `attacking()` and `baddieattacking()` are literally empty
// function bodies, because the screen they show was already painted by
// `attack()`/`baddieattack()` during the previous tick's own input handling
// and simply stays there. This shim clears the framebuffer every tick, so
// the drawing was moved out of `attack()`/`baddieattack()` (which now only
// roll damage and update state) into `rsgAttacking()`/`rsgBaddieAttacking()`
// (which redraw that same screen from the stored roll every tick). The
// rendered result is identical; the one real difference is that the attack
// screen appears on the tick after the button press rather than the same
// one, a single frame at upstream's own 20fps default.
//
// Upstream's own `losing()` is the one screen that never calls `clear()` and
// never re-selects a font, so on real hardware it paints its text over
// whatever the BADDIEATTACK screen left behind, in whichever font was last
// selected (font3x5). Since nothing persists here, this port draws that
// screen on its own clean background - it still uses font3x5 for the first
// two lines, exactly as real hardware's own leftover font selection does, so
// the text itself matches; only the leftover pixels underneath are absent.
//
// A REAL UPSTREAM QUIRK, PRESERVED: pressing Button C returns to the logo
// title screen, and real `Gamebuino::titleScreen()` sets
// `display.persistence = false` as it starts - which upstream never sets
// back to true. On real hardware every subsequent ATTACK/BADDIEATTACK screen
// is therefore blank, since those two draw functions are empty and nothing
// repaints them. This port cannot reproduce that (it never relied on
// persistence in the first place, so its own attack screens keep working
// after a Button C press) - a deliberate, documented divergence in the
// player's favour, not an oversight.

#include "../gamebuinoShim.h"

// ---------------------------------------------------------------------------
// Game state
// ---------------------------------------------------------------------------

// Upstream's own gamestate constants, plus one for the hand-rolled logo
// screen that real gb.titleScreen() blocks on.
#define RSG_TITLES        0
#define RSG_PLAYGAME      1
#define RSG_WINGAME       2
#define RSG_LOSEGAME      3
#define RSG_INSTRUCTIONS  4
#define RSG_ATTACK        5
#define RSG_BADDIEATTACK  6
#define RSG_TITLESCREEN   7

#define RSG_ATTACK_PHYSICAL 0
#define RSG_ATTACK_MAGIC    1

int rsgGamestate;

// Upstream's own `int* misterbaddieman = new int[20]` - see the header
// comment above for why this one is declared with 21 entries.
int[21] rsgMisterbaddieman;

int rsgHp;
int rsgMp;
int rsgBaddienumber;
int rsgHebedead; // 0 = alive, 1 = this one died, 2 = they're all dead

// The most recent attack's own rolled result, kept so rsgAttacking() can
// repaint the same screen every tick (real hardware leaves it on the display
// instead - see the header comment).
int rsgLastDamage;
int rsgLastDamageText;  // one of the RSG_TEXT_* values below
int rsgLastKilled;      // true if that attack was the killing blow

#define RSG_TEXT_ATTACK      0
#define RSG_TEXT_BESTATTACK  1
#define RSG_TEXT_SUPERATTACK 2
#define RSG_TEXT_MAGIC       3
#define RSG_TEXT_NOMP        4

// The most recent opponent turn's own rolled damage, kept for the same reason.
int rsgBaddieDamage;

// ---------------------------------------------------------------------------
// Real upstream's own 64x36 title logo, converted from its own B-binary
// PROGMEM table to hex byte-for-byte.
// ---------------------------------------------------------------------------

int[290] rsgLogoBitmap =
{
    0x40, 0x24, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x0E, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x7F, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01,
    0xFF, 0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03,
    0xFF, 0xE0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x07,
    0xFF, 0xE0, 0x00, 0x00, 0x00, 0x00, 0xC0, 0x0F,
    0xE3, 0xE0, 0x00, 0x00, 0x00, 0x07, 0xF0, 0x0F,
    0x01, 0xC0, 0x00, 0x00, 0x00, 0x1F, 0xF8, 0x1E,
    0x00, 0x80, 0x00, 0x00, 0x00, 0x3F, 0xFC, 0x1C,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x78, 0x3C, 0x3C,
    0x00, 0x00, 0x00, 0x0F, 0xC0, 0xF0, 0x38, 0x38,
    0x00, 0x00, 0x00, 0xFF, 0xE0, 0xF0, 0x00, 0x38,
    0x00, 0x00, 0x07, 0xF1, 0xE0, 0xFC, 0x00, 0x78,
    0x00, 0x00, 0x0F, 0x00, 0xE0, 0x7F, 0xE0, 0x78,
    0x0F, 0xE0, 0x0F, 0x01, 0xE0, 0x3F, 0xF8, 0x78,
    0x0F, 0xE0, 0x0F, 0x1F, 0xC0, 0x1F, 0xFC, 0x78,
    0x0F, 0xE0, 0x0F, 0xFF, 0x80, 0x01, 0xFE, 0x78,
    0x0F, 0xE0, 0x0F, 0x7E, 0x00, 0x00, 0x1E, 0x38,
    0x0F, 0xE0, 0x0F, 0x07, 0x81, 0xE0, 0x0E, 0x38,
    0x00, 0xE0, 0x0F, 0x03, 0xC1, 0xE0, 0x0E, 0x3C,
    0x00, 0xE0, 0x0F, 0x01, 0xC0, 0xF0, 0x1E, 0x3E,
    0x00, 0xE0, 0x03, 0x01, 0xE0, 0xFE, 0xFE, 0x1F,
    0x01, 0xE0, 0x00, 0x00, 0xF0, 0x7F, 0xFC, 0x0F,
    0xC7, 0xE0, 0x00, 0x00, 0x00, 0x1F, 0xF8, 0x0F,
    0xFF, 0xE0, 0x00, 0x00, 0x00, 0x03, 0xF0, 0x07,
    0xFF, 0xE0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03,
    0xFF, 0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0xFF, 0x00, 0x00, 0x00, 0x00, 0x41, 0x00, 0x00,
    0x00, 0x08, 0x00, 0x00, 0x00, 0x41, 0x00, 0x00,
    0x00, 0x08, 0x00, 0x00, 0x01, 0xC9, 0x59, 0x43,
    0x30, 0xAA, 0x00, 0x00, 0x02, 0x55, 0x85, 0x44,
    0x48, 0xAC, 0x00, 0x00, 0x02, 0x51, 0x55, 0x44,
    0x48, 0xAA, 0x00, 0x00, 0x01, 0xCD, 0x4C, 0x93,
    0x32, 0x6A, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00,
    0x00, 0x00
};

// ---------------------------------------------------------------------------
// Real upstream text lines containing Gamebuino's own low-ASCII button icon
// glyphs (\25/\26/\27 - octal escapes for ASCII 21/22/23, the A/B/C button
// icons), which a quoted string literal in this dialect cannot hold - built
// as explicit 0-terminated int arrays instead, matching the treatment
// gameTaquin.c/gameSimonbuino.c already established for the same gap.
// ---------------------------------------------------------------------------

// "       \25 Start\n"
int[16] rsgTextStart = { 32, 32, 32, 32, 32, 32, 32, 21, 32, 83, 116, 97, 114, 116, 10, 0 };

// "   \26 Instructions\n\n"
int[20] rsgTextInstr =
{
    32, 32, 32, 22, 32, 73, 110, 115, 116, 114, 117, 99, 116, 105, 111, 110, 115, 10, 10, 0
};

// "\n(press \26)"
int[11] rsgTextPressB = { 10, 40, 112, 114, 101, 115, 115, 32, 22, 41, 0 };

// "\n(press \27)"
int[11] rsgTextPressC = { 10, 40, 112, 114, 101, 115, 115, 32, 23, 41, 0 };

// ---------------------------------------------------------------------------
// Setup - direct port of upstream's own initgame()
// ---------------------------------------------------------------------------

void rsgInitGame()
{
    int i;

    // Set up misterbaddiemans 0-19
    rsgMisterbaddieman[ 0 ] = arand( 8 ) + 5;

    for( i = 1; i < 20; i++ )
      rsgMisterbaddieman[ i ] = rsgMisterbaddieman[ i - 1 ] + arand( 4 ) + 1;

    // Set up hp and mp
    rsgHp = arand( 35 ) + 65;
    rsgMp = arand( 12 ) + 5;

    rsgBaddienumber = 0;
    gbClear();
}

// ---------------------------------------------------------------------------
// Screens - direct ports of upstream's own titles.ino/playgame.ino/endgame.ino
// ---------------------------------------------------------------------------

// Stands in for the blocking gb.titleScreen(F(""), logo) call real upstream
// makes from setup() and again whenever Button C is pressed.
void rsgUpdateTitleScreen()
{
    gbDrawBitmap( 10, 2, rsgLogoBitmap );

    gbSetFont( gbFont3x5 );
    gbCursorX = 24;
    gbCursorY = 41;
    gbPrintString( "PRESS A" );

    if( gbPressed( BTN_A ) )
    {
        gbPlayOK();
        gbSetFont( gbFont5x7 );
        rsgGamestate = RSG_TITLES;
    }
}

void rsgTitleScreenText()
{
    gbClear();
    gbSetFont( gbFont5x7 );
    gbPrintString( "    R.S.G.\n" );
    gbPrintString( "\n" );
    gbSetFont( gbFont3x5 );
    gbPrintString( rsgTextStart );
    gbPrintString( rsgTextInstr );
    gbPrintString( " (c)2015 deKay, v" );
    gbPrintString( "1.01" );
}

void rsgInstructions()
{
    gbClear();
    gbSetFont( gbFont5x7 );
    gbPrintString( " Instructions\n" );
    gbPrintString( "\n" );
    gbSetFont( gbFont3x5 );
    gbPrintString( "20 Misterbaddiemen\n" );
    gbPrintString( "must be fought with\n" );
    gbPrintString( "attacks or magic\n" );
    gbPrintString( "for no real reason!\n" );
}

// Upstream's own real prompt line is `"\n\25 Attack or \26 Magic?"` - built
// here as its own array for the same icon-glyph reason as the others.
int[22] rsgTextPrompt =
{
    10, 21, 32, 65, 116, 116, 97, 99, 107, 32, 111, 114, 32, 22, 32,
    77, 97, 103, 105, 99, 63, 0
};

void rsgDisplayHUD()
{
    gbClear();
    gbSetFont( gbFont3x5 );
    gbPrintString( "Misterbaddieman " );
    gbPrintNumber( rsgBaddienumber + 1 );
    gbPrintString( "\n" );
    gbPrintString( "\nHIM: HP " );
    gbPrintNumber( rsgMisterbaddieman[ rsgBaddienumber ] );
    gbPrintString( "\n" );
    gbPrintString( "YOU: HP " );
    gbPrintNumber( rsgHp );
    gbPrintString( "   MP " );
    gbPrintNumber( rsgMp );
    gbPrintString( "\n" );
    gbPrintString( "\n" );
    gbPrintString( rsgTextPrompt );
}

void rsgPlayRsg()
{
    if( rsgMisterbaddieman[ rsgBaddienumber ] == 0 )
      rsgBaddienumber++;

    rsgDisplayHUD();
}

// Direct port of upstream's own attack(), minus its drawing (see the header
// comment) - rolls the damage and updates state only.
void rsgAttack( int attacktype )
{
    int baddiedamage = 0;
    int edam = 0; // Obligatory cheese reference

    rsgGamestate = RSG_ATTACK;
    rsgHebedead = 0;
    rsgLastDamageText = RSG_TEXT_ATTACK;

    // Normal Attack - damage 0 to 5, with 8/12/25 Super Attack chance
    if( attacktype == RSG_ATTACK_PHYSICAL )
    {
        baddiedamage = arand( 6 );
        rsgLastDamageText = RSG_TEXT_ATTACK;

        // Super Attack?
        if( baddiedamage == 0 )
        {
            edam = arand( 5 );

            if( edam == 0 )
            {
                baddiedamage = 25;
                rsgLastDamageText = RSG_TEXT_BESTATTACK;
            }

            if( edam == 1 )
            {
                baddiedamage = 12;
                rsgLastDamageText = RSG_TEXT_SUPERATTACK;
            }

            if( edam == 2 )
            {
                baddiedamage = 8;
                rsgLastDamageText = RSG_TEXT_SUPERATTACK;
            }

            if( edam > 2 )
              baddiedamage = 0;
        }
    }

    // Magic Attack - damage 5 to 15
    if( attacktype == RSG_ATTACK_MAGIC )
    {
        rsgLastDamageText = RSG_TEXT_MAGIC;
        baddiedamage = arand( 11 ) + 5;

        if( rsgMp == 0 )
        {
            rsgLastDamageText = RSG_TEXT_NOMP;
            baddiedamage = 0;
        }

        rsgMp--;

        if( rsgMp < 0 )
          rsgMp = 0;
    }

    rsgMisterbaddieman[ rsgBaddienumber ] = rsgMisterbaddieman[ rsgBaddienumber ] - baddiedamage;

    if( rsgMisterbaddieman[ rsgBaddienumber ] < 0 )
      rsgMisterbaddieman[ rsgBaddienumber ] = 0;

    rsgLastDamage = baddiedamage;
    rsgLastKilled = false;

    if( rsgMisterbaddieman[ rsgBaddienumber ] == 0 )
    {
        rsgLastKilled = true;
        rsgBaddienumber++;
        rsgHebedead = 1;

        if( rsgBaddienumber == 20 )
          rsgHebedead = 2;
    }
}

// Draws the screen upstream's own attack() paints once and then leaves on a
// persistent display.
void rsgAttacking()
{
    gbClear();
    gbSetFont( gbFont5x7 );

    if( rsgLastDamageText == RSG_TEXT_ATTACK )           gbPrintString( "ATTACK!" );
    else if( rsgLastDamageText == RSG_TEXT_BESTATTACK )  gbPrintString( "BESTATTACK!" );
    else if( rsgLastDamageText == RSG_TEXT_SUPERATTACK ) gbPrintString( "SUPERATTACK!" );
    else if( rsgLastDamageText == RSG_TEXT_MAGIC )       gbPrintString( "MAGIC!" );
    else                                                 gbPrintString( "NO MP!" );

    gbPrintString( "\n" );
    gbPrintString( "Damage: " );
    gbPrintNumber( rsgLastDamage );
    gbPrintString( "\n" );
    gbSetFont( gbFont3x5 );
    gbPrintString( rsgTextPressB );

    if( rsgLastKilled )
    {
        gbSetFont( gbFont5x7 );
        gbPrintString( "\n\nHE'S DEAD!!" );
    }
}

// Direct port of upstream's own baddieattack(), minus its drawing.
void rsgBaddieAttack()
{
    rsgGamestate = RSG_BADDIEATTACK;
    rsgBaddieDamage = 0;

    if( rsgHebedead == 0 )
    {
        rsgBaddieDamage = arand( 3 + arand( rsgBaddienumber / 5 ) );
        rsgHp = rsgHp - rsgBaddieDamage;

        if( rsgHp < 0 )
          rsgHp = 0;
    }
}

void rsgBaddieAttacking()
{
    gbClear();

    if( rsgHebedead == 0 )
    {
        gbSetFont( gbFont5x7 );
        gbPrintString( "He attacks...\n" );
        gbPrintString( "Damage: " );
        gbPrintNumber( rsgBaddieDamage );
        gbPrintString( "\n" );
        gbSetFont( gbFont3x5 );
        gbPrintString( rsgTextPressB );
    }

    if( rsgHebedead == 1 )
    {
        gbSetFont( gbFont3x5 );
        gbPrintString( "Another assailant!\n" );
        gbPrintString( "Misterbaddieman " );
        gbPrintNumber( rsgBaddienumber + 1 );
        gbPrintString( "\n" );
        gbSetFont( gbFont3x5 );
        gbPrintString( rsgTextPressB );
    }

    // hebedead == 2 draws nothing at all, exactly like real upstream - that
    // screen is only ever shown for the single tick between the twentieth
    // opponent dying and Button B moving on to WINGAME.
}

void rsgWinning()
{
    gbClear();
    gbSetFont( gbFont5x7 );
    gbPrintString( "   YOU WIN!!\n\n" );
    gbPrintString( "     YAY!\n" );
    gbSetFont( gbFont3x5 );
    gbPrintString( rsgTextPressC );
}

// Real upstream's own losing() never calls clear() and never re-selects a
// font, inheriting font3x5 from the BADDIEATTACK screen it is always reached
// from - reproduced here by selecting that same font explicitly.
void rsgLosing()
{
    gbSetFont( gbFont3x5 );
    gbPrintString( " YOU'RE DEAD!!\n\n" );
    gbPrintString( "   Rubbish!\n" );
    gbSetFont( gbFont3x5 );
    gbPrintString( rsgTextPressC );
}

// ---------------------------------------------------------------------------
// Input - direct port of upstream's own keyPressed()
// ---------------------------------------------------------------------------

void rsgKeyPressed()
{
    if( rsgGamestate == RSG_TITLES )
    {
        if( gbPressed( BTN_A ) )
        {
            rsgGamestate = RSG_PLAYGAME;
            rsgInitGame();
        }

        if( gbPressed( BTN_B ) )
          rsgGamestate = RSG_INSTRUCTIONS;
    }
    else if( rsgGamestate == RSG_INSTRUCTIONS )
    {
        if( gbPressed( BTN_A ) )
        {
            rsgInitGame();
            rsgGamestate = RSG_TITLES;
        }
    }
    else if( rsgGamestate == RSG_PLAYGAME )
    {
        if( gbPressed( BTN_A ) )
          rsgAttack( RSG_ATTACK_PHYSICAL );

        if( gbPressed( BTN_B ) )
          rsgAttack( RSG_ATTACK_MAGIC );
    }
    else if( rsgGamestate == RSG_ATTACK )
    {
        if( gbPressed( BTN_B ) )
          rsgBaddieAttack();
    }
    else if( rsgGamestate == RSG_BADDIEATTACK )
    {
        if( gbPressed( BTN_B ) )
        {
            if( rsgHp == 0 )
            {
                rsgGamestate = RSG_LOSEGAME;
                return;
            }

            if( rsgHebedead == 2 )
              rsgGamestate = RSG_WINGAME;
            else
              rsgGamestate = RSG_PLAYGAME;
        }
    }
    else if( rsgGamestate == RSG_WINGAME )
    {
        if( gbPressed( BTN_B ) )
          rsgInitGame();
    }
    else if( rsgGamestate == RSG_LOSEGAME )
    {
        if( gbPressed( BTN_B ) )
          rsgInitGame();
    }
}

// ---------------------------------------------------------------------------
// Entry points
// ---------------------------------------------------------------------------

void gameRsg_init()
{
    gbBegin();
    gbSetFont( gbFont5x7 );
    gbPickRandomSeed(); // no-op, see gamebuinoShim.h's own header comment

    rsgGamestate = RSG_TITLESCREEN;
    rsgHebedead = 0;
    rsgLastDamage = 0;
    rsgLastDamageText = RSG_TEXT_ATTACK;
    rsgLastKilled = false;
    rsgBaddieDamage = 0;

    rsgInitGame();
}

void gameRsg_update()
{
    if( !gbUpdate() ) return;

    if( rsgGamestate == RSG_TITLESCREEN )
    {
        rsgUpdateTitleScreen();
        gbRenderFrame();
        return;
    }

    if( rsgGamestate == RSG_TITLES )            rsgTitleScreenText();
    else if( rsgGamestate == RSG_PLAYGAME )     rsgPlayRsg();
    else if( rsgGamestate == RSG_INSTRUCTIONS ) rsgInstructions();
    else if( rsgGamestate == RSG_ATTACK )       rsgAttacking();
    else if( rsgGamestate == RSG_BADDIEATTACK ) rsgBaddieAttacking();
    else if( rsgGamestate == RSG_WINGAME )      rsgWinning();
    else if( rsgGamestate == RSG_LOSEGAME )     rsgLosing();

    // If you don't do this you get stuck in PLAYGAME
    if( gbPressed( BTN_A ) || gbPressed( BTN_B ) )
    {
        rsgKeyPressed();
    }
    else if( gbPressed( BTN_C ) )
    {
        gbSetFont( gbFont5x7 );
        rsgGamestate = RSG_TITLESCREEN;
    }

    gbRenderFrame();
}
