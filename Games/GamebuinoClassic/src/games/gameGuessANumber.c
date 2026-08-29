// Guess A Number (l_et_m, no license specified) - a minimal number-guessing
// game, and the smallest real game in this cartridge. The console picks a
// number between 0 and 100; Up/Down move your guess one step at a time,
// Button A submits it and the screen answers "Too small!", "Too big!" or
// "Found! Well done!". Button B resets the guess to 0, Button C returns to
// the title screen and picks a fresh number.
//
// Every real `gb.x.y(...)` call site is mechanically rewritten to a plain
// `gbY(...)` function call (see gamePong.c's own header comment for why -
// this dialect has no classes/methods/operator overloading). Every global
// symbol got a `gan`-prefixed name (this cartridge has no linker - every
// ported game shares one flat global namespace). Upstream's own
// `random(0, MAXNUMBER+1)` became `arand(MAXNUMBER+1)`, this project's own
// established ranged-random rewrite convention.
//
// The real source's own comments are part French, part English (the author
// wrote it that way); the French ones are kept verbatim where they carry
// real meaning.
//
// `randomSeed(analogRead(2))` has no equivalent here - there is no analog
// pin to read floating noise off - so it is dropped, exactly as every other
// port in this cartridge drops it (`gbPickRandomSeed()` is itself a
// documented no-op). Upstream's own comment on that line already says "a
// ameliorer !" ("to improve!"), so it was never a dependable seed anyway.
//
// BLOCKING TITLE SCREEN -> AN EXPLICIT STATE: real `gb.titleScreen(F("Guess
// my number"))` blocks until Button A. Hand-rolled here as one more explicit
// state, matching the treatment every other port in this cartridge gives
// that same call (see gameFifteen.c).
//
// A REAL UPSTREAM QUIRK, PRESERVED: once the number is found, `fini` stops
// the whole guess/input block - Up/Down/A/B all go dead and the current
// guess stops being drawn at all, leaving only the "Found! Well done!"
// line. Button C is the only way on from there, which is exactly what real
// hardware does.

#include "../gamebuinoShim.h"

#define GAN_MAXNUMBER 100

#define GAN_STATE_TITLE 0
#define GAN_STATE_PLAY  1

int ganRandomNumber;
int ganUserNumber;
int ganCurrentStatus; // 0 = ok, 1 = too small, 2 = too big, -1 = no guess yet
int ganFini;
int ganState;

void ganNewNumber()
{
    // le nombre a trouver
    ganRandomNumber = arand( GAN_MAXNUMBER + 1 );
    ganUserNumber = 0;
    ganFini = false;
    ganCurrentStatus = -1;
}

// Stands in for the blocking gb.titleScreen(F("Guess my number")) call.
void ganUpdateTitle()
{
    gbCursorX = 6;
    gbCursorY = 12;
    gbPrintString( "Guess my number" );

    gbCursorX = 24;
    gbCursorY = 32;
    gbPrintString( "PRESS A" );

    if( gbPressed( BTN_A ) )
    {
        gbPlayOK();
        ganState = GAN_STATE_PLAY;
    }
}

void ganDisplayCurrentStatus()
{
    if( ganCurrentStatus == 0 )      gbPrintString( "Found! Well done!\n" );
    else if( ganCurrentStatus == 1 ) gbPrintString( "Too small!\n" );
    else if( ganCurrentStatus == 2 ) gbPrintString( "Too big!\n" );
    else                             gbPrintString( "No guess!\n" );
}

void ganUpdatePlay()
{
    gbClear();

    gbPrintString( "I chose a number\n" );
    gbPrintString( "between 0 and " );
    gbPrintNumber( GAN_MAXNUMBER );
    gbPrintString( "\n" );
    gbPrintString( "now guess!\n" );

    ganDisplayCurrentStatus();

    if( !ganFini )
    {
        gbPrintNumber( ganUserNumber );
        gbPrintString( "\n" );

        if( gbPressed( BTN_A ) )
        {
            if( ganUserNumber == ganRandomNumber )
            {
                ganCurrentStatus = 0;
                ganFini = true;
                gbPlayOK();
            }
            else if( ganUserNumber < ganRandomNumber )
            {
                ganCurrentStatus = 1;
            }
            else
            {
                ganCurrentStatus = 2;
            }
        }

        if( gbPressed( BTN_B ) )
        {
            gbPlayCancel();
            ganUserNumber = 0;
        }

        // rester appuye 2 frames de suite
        if( gbRepeat( BTN_UP, 2 ) )
          if( ganUserNumber < GAN_MAXNUMBER )
            ganUserNumber = ganUserNumber + 1;

        if( gbRepeat( BTN_DOWN, 2 ) )
          if( ganUserNumber > 0 )
            ganUserNumber = ganUserNumber - 1;
    }

    if( gbPressed( BTN_C ) )
    {
        ganState = GAN_STATE_TITLE;
        ganNewNumber(); // un nouveau nombre
    }
}

void gameGuessANumber_init()
{
    gbBegin();
    gbPickRandomSeed(); // no-op, see gamebuinoShim.h's own header comment

    ganState = GAN_STATE_TITLE;
    ganNewNumber();
}

void gameGuessANumber_update()
{
    if( !gbUpdate() ) return;

    if( ganState == GAN_STATE_TITLE ) ganUpdateTitle();
    else                              ganUpdatePlay();

    gbRenderFrame();
}
