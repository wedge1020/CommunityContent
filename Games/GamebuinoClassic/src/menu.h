#ifndef MENU_H
#define MENU_H

// Game-select menu - adapted directly from the sibling tinyjoypad_vircon32
// project's own menu.c/menu.h (same overall design: Vircon32's own built-in
// BIOS font via print_at(), plus that project's own thumbnail-atlas layout -
// see machineDependent.h's own md_getThumbnailCount()/md_drawGameThumbnail()
// and portVircon32.c's own THUMBNAILS_TEXTURE_ID for the asset side).

typedef void(void) GameFunc;

struct Game
{
    int* title;
    // Original game's author/credit (e.g. "AURELIEN RODOT") - shown as
    // "BY <author>" under the menu.
    int* author;
    // Optional (pass NULL if not needed) - a second line shown directly
    // below the author credit. Two real, independent uses share this one
    // field: a porter credit continuation for a real combined "original
    // author / porter" attribution too long for one line (e.g. Star
    // Honor's own "WUUFF"), or a short reason for a game flagged via
    // markUnfinished() below (e.g. "Ball can get stuck"). Drawn in
    // color_white normally, color_red when this game is unfinished - see
    // menu_update()'s own drawing code.
    int* info;
    GameFunc* init;
    GameFunc* update;
    // Optional (pass 0/NULL if not needed) - called by portVircon32.c's own
    // quit-confirmation dialog (on resume) and pixel-grid toggle, to force
    // one fresh redraw. Matching the sibling tinyjoypad_vircon32 project's
    // own onResume hook - NULL is fine (and is all any Gamebuino game needs
    // today) for any game whose own update() always redraws unconditionally
    // rather than skipping frames where nothing changed; see portVircon32.c's
    // own comment above drawPixelGridOverlay() for why that's true of every
    // gamebuinoShim-based game here.
    GameFunc* onResume;
    // Still fully registered/playable/thumbnailed like any other game
    // (see markUnfinished() below) - just drawn with reddish list text as
    // a visual "known incomplete" warning. Defaults to false in addGame().
    bool unfinished;
};

extern int gameCount;

// Returns the index this game was registered at (or -1 if MAX_GAMES was
// already reached) - pass straight into markUnfinished() below if needed.
int addGame( int* title, int* author, int* info, GameFunc* init, GameFunc* update, GameFunc* onResume );
// Flags a registered game as unfinished - still shown, selectable, and
// playable exactly like any other game (registration index and thumbnail
// mapping untouched), just drawn with reddish list text - see the real
// use case in menuGameList.c.
void markUnfinished( int index );
Game* menu_getGame( int index );
void menu_init();

// draws the menu and handles its own navigation input; returns the game
// just chosen (A pressed on it) this frame, or -1 if none was
int menu_update();

#endif
