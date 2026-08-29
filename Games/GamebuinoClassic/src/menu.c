#include "menu.h"
#include "machineDependent.h"
#include "video.h"
#include "string.h"

// Raised from 104 to 112 once Mole Control and Aerial-Assault (the last
// two games found via the sibling gamebuino_classic_source_codes archive
// project) brought the real registered-game count to 100, leaving only 4
// spare slots - addGame() silently drops any call once gameCount reaches
// this cap (see below), so this MUST stay ahead of the real total in
// menuGameList.c. 112 restores the same "modest headroom" margin this
// project already uses elsewhere (e.g. the thumbnail atlas) rather than
// jumping straight to a much larger number "just in case" - each unit here
// also costs one more full EEPROM slot's worth of card storage (see
// eepromShim.c's own header comment), so headroom isn't free.
#define MAX_GAMES 120

// How many entries fit in the vertical space between the list's start and
// the bottom of the screen - see the sibling tinyjoypad_vircon32 project's
// own menu.c for the exact same layout reasoning.
#define GAMES_PER_PAGE 9
#define LIST_AREA_TOP 140

int menuCenteredX( int* text )
{
    return ( screen_width - strlen( text ) * bios_character_width ) / 2;
}

int gameCount = 0;
Game[MAX_GAMES] games;
int selection = 0;

int[MAX_GAMES] displayOrder;
bool displayOrderBuilt = false;

bool prevUp = false;
bool prevDown = false;
bool prevA = false;
bool prevLeft = false;
bool prevRight = false;

int addGame( int* title, int* author, int* info, GameFunc* init, GameFunc* update, GameFunc* onResume )
{
    if( gameCount >= MAX_GAMES )
      return -1;

    games[ gameCount ].title = title;
    games[ gameCount ].author = author;
    games[ gameCount ].info = info;
    games[ gameCount ].init = init;
    games[ gameCount ].update = update;
    games[ gameCount ].onResume = onResume;
    games[ gameCount ].unfinished = false;
    gameCount++;
    return gameCount - 1;
}

void markUnfinished( int index )
{
    if( index < 0 || index >= gameCount )
      return;

    games[ index ].unfinished = true;
}

Game* menu_getGame( int index )
{
    return &games[ index ];
}

// Selection sort on displayOrder (by games[].title) - gameCount is always a
// small handful of entries, so O(n^2) costs nothing measurable here. Same
// technique as the sibling tinyjoypad_vircon32 project's own menu.c.
void menu_buildDisplayOrder()
{
    for( int i = 0; i < gameCount; i++ )
      displayOrder[ i ] = i;

    for( int i = 0; i < gameCount - 1; i++ )
    {
        int best = i;
        for( int j = i + 1; j < gameCount; j++ )
          if( strcmp( games[ displayOrder[ j ] ].title, games[ displayOrder[ best ] ].title ) < 0 )
            best = j;

        if( best != i )
        {
            int tmp = displayOrder[ i ];
            displayOrder[ i ] = displayOrder[ best ];
            displayOrder[ best ] = tmp;
        }
    }

    displayOrderBuilt = true;
}

// Real, live gap found via a direct question: menu_init() used to reset
// every prevX unconditionally to false, regardless of whatever the D-pad's
// own real physical state already was at that exact moment. Button A is
// already safe on the same "return from a just-quit game" path -
// md_armInputAGate() (armed by portVircon32.c right before this function
// runs) makes md_inputA() itself report "released" until the physical
// button genuinely is, so menu.c's own `a = md_inputA()` read below
// already can't see a leftover press. But Up/Down/Left/Right have no
// equivalent gate anywhere - so a player who was still holding, say,
// Right (moving their character) at the exact moment they confirmed Quit
// would have had prevRight forced to false here while the real button was
// still true, manufacturing a false "just pressed" edge on the very next
// tick and instantly paging the just-reopened menu sideways with no real
// new input from the player. Fixed by sampling each button's own real
// current state instead of assuming released - the same "arm against
// whatever's already held" idea portVircon32.c already uses for
// prevConfirmLeft/Right/A before opening the quit dialog itself.
void menu_init()
{
    prevUp = md_inputUp();
    prevDown = md_inputDown();
    prevA = md_inputA();
    prevLeft = md_inputLeft();
    prevRight = md_inputRight();

    if( !displayOrderBuilt )
      menu_buildDisplayOrder();
}

int menu_update()
{
    bool up = md_inputUp();
    bool down = md_inputDown();
    bool a = md_inputA();
    bool left = md_inputLeft();
    bool right = md_inputRight();

    if( down && !prevDown )
    {
        selection++;
        if( selection >= gameCount )
          selection = 0;
    }
    if( up && !prevUp )
    {
        selection--;
        if( selection < 0 )
          selection = gameCount - 1;
    }

    int totalPages = ( gameCount + GAMES_PER_PAGE - 1 ) / GAMES_PER_PAGE;

    if( right && !prevRight )
    {
        int currentPage = selection / GAMES_PER_PAGE;
        currentPage++;
        if( currentPage >= totalPages )
          currentPage = 0;
        selection = currentPage * GAMES_PER_PAGE;
        if( selection >= gameCount )
          selection = gameCount - 1;
    }
    if( left && !prevLeft )
    {
        int currentPage = selection / GAMES_PER_PAGE;
        currentPage--;
        if( currentPage < 0 )
          currentPage = totalPages - 1;
        selection = currentPage * GAMES_PER_PAGE;
        if( selection >= gameCount )
          selection = gameCount - 1;
    }

    bool justFired = ( a && !prevA );

    prevUp = up;
    prevDown = down;
    prevA = a;
    prevLeft = left;
    prevRight = right;

    // ---- draw ----
    clear_screen( color_black );
    print_at( menuCenteredX( "GAMEBUINO CLASSIC FOR VIRCON32" ), 40, "GAMEBUINO CLASSIC FOR VIRCON32" );
    print_at( menuCenteredX( "UP/DOWN: SELECT     A: PLAY" ), 80, "UP/DOWN: SELECT     A: PLAY" );

    // Current state of the three global toggles, parked in the corner where
    // nothing else is drawn.
    md_drawToggleStatusIcons();

    int currentPage = selection / GAMES_PER_PAGE;

    if( totalPages > 1 )
    {
        int[8] pageNumText;
        int[8] totalPagesText;
        int[48] pageHintText;
        itoa( currentPage + 1, pageNumText, 10 );
        itoa( totalPages, totalPagesText, 10 );
        strcpy( pageHintText, "LEFT/RIGHT: CHANGE PAGE " );
        strcat( pageHintText, pageNumText );
        strcat( pageHintText, "/" );
        strcat( pageHintText, totalPagesText );
        print_at( menuCenteredX( pageHintText ), 105, pageHintText );
    }

    int startIndex = currentPage * GAMES_PER_PAGE;

    int y = LIST_AREA_TOP;
    for( int i = 0; i < GAMES_PER_PAGE; i++ )
    {
        int idx = startIndex + i;
        if( idx >= gameCount )
          break;

        int x = 60;
        if( idx == selection )
          x = 40;

        if( idx == selection )
          print_at( x, y, ">" );

        int[8] numText;
        itoa( idx + 1, numText, 10 );
        int[64] labelText;
        if( idx + 1 < 10 )
          strcpy( labelText, "0" );
        else
          strcpy( labelText, "" );
        strcat( labelText, numText );
        strcat( labelText, ". " );
        strcat( labelText, games[ displayOrder[ idx ] ].title );

        // Unfinished games (see markUnfinished()) still show up in their
        // normal alphabetical position, still fully selectable/playable -
        // only the list text itself turns red, as a plain visual "known
        // incomplete" warning. print_at()'s own color comes from whatever
        // set_multiply_color() was last set to (persistent GPU state, not
        // reset per call - see VIRCON32_C_DIALECT.md's own §17.5), so this
        // sets red immediately before the label and white immediately
        // after, rather than leaving every later draw call in this frame
        // (the thumbnail, the credit line, the next page's own entries)
        // incorrectly red too.
        if( games[ displayOrder[ idx ] ].unfinished )
          set_multiply_color( color_red );

        print_at( x + 20, y, labelText );

        if( games[ displayOrder[ idx ] ].unfinished )
          set_multiply_color( color_white );

        y += 24;
    }

    // Real gameplay screenshot of the currently-selected game, in the margin
    // freed up on the right by keeping the list itself close to the left
    // edge - switches immediately whenever the selection moves, since it's
    // just read straight off `selection` every frame. Centered vertically
    // (as a group with the "BY <author>" line below it) within the list/
    // selection area (LIST_AREA_TOP down to the bottom of the screen)
    // rather than top-aligned with the list. Indexed through displayOrder[]
    // like the title above - the thumbnail atlas is keyed by registration
    // index, not by alphabetical position. Direct port of the sibling
    // tinyjoypad_vircon32 project's own identical layout.
    int selectedGameIndex = displayOrder[ selection ];
    if( selectedGameIndex < md_getThumbnailCount() )
    {
        // 32 used to be enough for every credit line, until Star Honor's
        // own real "original author / porter" combined credit needed 35
        // slots ("BY " + author + a null terminator) and silently
        // overflowed by 3 - corrupting the vertical-centering math below
        // (the next local declared in the pre-fix version) and shifting
        // the whole credit line upward instead of drawing garbled text, a
        // real, live-reported bug. Matched to the same generous `int[64]`
        // `labelText` already uses just above, rather than bumped to the
        // bare minimum - this project's own established "modest headroom
        // past the real current need" convention.
        int[64] authorText;
        strcpy( authorText, "BY " );
        strcat( authorText, games[ selectedGameIndex ].author );

        // A second, independent info line (Game.info - see menu.h) is
        // drawn directly below the author credit whenever a game supplies
        // one - either a porter-credit continuation (Star Honor's own
        // real "original author / porter" case) or a short unfinished-
        // game reason (e.g. "Ball can get stuck"). Two separate
        // single-line print_at() calls, not one call with an embedded
        // '\n' - print_at() is a raw Vircon32 BIOS primitive, not this
        // shim's own hand-rolled gbPrintString() (the one call in this
        // codebase actually documented to handle '\n'), so nothing
        // guarantees it would split a line on an embedded '\n' too.
        bool hasInfo = games[ selectedGameIndex ].info != NULL;

        int authorGapY = 8;
        int lineCount = 1;
        if( hasInfo ) lineCount = 2;
        int blockHeight = MD_THUMBNAIL_HEIGHT + authorGapY + bios_character_height * lineCount;
        int blockY = LIST_AREA_TOP + ( ( screen_height - LIST_AREA_TOP ) - blockHeight ) / 2;

        md_drawGameThumbnail( selectedGameIndex, 340, blockY );

        int authorX = 340 + ( MD_THUMBNAIL_WIDTH - strlen( authorText ) * bios_character_width ) / 2;
        print_at( authorX, blockY + MD_THUMBNAIL_HEIGHT + authorGapY, authorText );

        if( hasInfo )
        {
            // Same persistent-GPU-state reasoning as the list-text color
            // above: red while this game is unfinished, white otherwise,
            // and always restored to white afterward so no later draw
            // call in this same frame inherits the wrong color.
            if( games[ selectedGameIndex ].unfinished )
              set_multiply_color( color_red );

            int infoX = 340 + ( MD_THUMBNAIL_WIDTH - strlen( games[ selectedGameIndex ].info ) * bios_character_width ) / 2;
            print_at( infoX, blockY + MD_THUMBNAIL_HEIGHT + authorGapY + bios_character_height, games[ selectedGameIndex ].info );

            if( games[ selectedGameIndex ].unfinished )
              set_multiply_color( color_white );
        }
    }

    if( justFired )
      return selectedGameIndex;

    return -1;
}
