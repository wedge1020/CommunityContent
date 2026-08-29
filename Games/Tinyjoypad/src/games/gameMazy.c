// =============================================================================
// MAZY (inufuto, UIAPduino+SSD1306/CH32V003 edition, "Cate engine" family,
// license "None specified" - same as `UIAPduino_cracky`'s own situation) - a
// top-down maze game: guide Man through a hand-authored 6x6..12x12-room maze
// to the blinking Goal, picking up throwing knives lying on the floor and
// throwing them (Fire, in the direction currently faced) to kill chasing/
// patrolling Monsters before they touch you. 10 hand-authored mazes, 3
// lives, a countdown timer per stage (running out ends the attempt exactly
// like a monster collision does), and a per-second bonus-point tally once a
// stage is cleared. No persistent high score exists anywhere upstream (no
// EEPROM of any kind is used by this game at all - confirmed by reading the
// full source, unlike Cracky's own sibling port).
//
// **MANDATORY READ FIRST, per this port's own task brief**: `gameCracky.c`
// (same directory) is the reference implementation this file mirrors the
// shape of - shared architecture, dialect handling, and machineDependent
// call conventions all come directly from that already-debugged port. Unlike
// Cracky, this game has NO `Movable.cpp`/`Sprite.cpp`/`Stage.cpp` - its own
// `Map.cpp` owns maze storage/movement/collision directly, and its own
// `Vram.cpp`/`Status.cpp` write status text as literal raw pixel bytes
// (arbitrary column strides, not aligned to any uniform character-cell grid)
// rather than through an indexed-cell abstraction the way Cracky's status
// area could be - see "Rendering architecture" below for why this led to a
// genuinely different (and, for this game, better-fitting) technique than
// Cracky's own overlay-at-render-time trick.
//
// **Confirmed, unlike Cracky's own initial wrong-orientation detour, NO
// hardware display-orientation transform is needed here either** - applying
// the lesson directly rather than re-discovering it: `Vram.cpp`'s own
// `Locate()`/`SendUL()` addressing is identical in shape to Cracky's, and
// this port draws every composed byte directly at its own real (col,page)
// with no mirroring, no page reorder, no bit-reversal.
//
// **Rendering architecture - a real persistent pixel framebuffer, not a
// per-frame recompute-everything model like Cracky's**: `Vram.cpp`'s own
// `PrintC()`/`Put2C()`/`PutU()` write status text (SCORE/STAGE/TIME/lives/
// held-knife icons) and the "GAME OVER"/"TIME UP" messages as literal raw
// bytes at real pixel-column addresses that do NOT align to a uniform 4px
// grid in every case (`PrintHeldKnives()`'s own icon loop advances by
// `VramStep+1` = 5 real pixels per drawn icon, one wider than the other
// text's own 4px-per-glyph stride, to leave a 1px gap between knife icons) -
// a shape Cracky's own `crkStatusChar[8][8]` indexed-cell grid cannot
// represent faithfully. Rather than force an ill-fitting cell-grid
// abstraction onto this game's own genuinely pixel-addressed writes, this
// port instead keeps a real persistent `int[8][128] mzyFrame` raw-byte
// framebuffer (matching this project's own established "real pixel-
// addressable framebuffer" technique from Road Rush/Gilbert in the
// Downland/Tiny Arena) - every draw primitive (`mzyPrintC`/`mzyPut2C`/
// `mzyPutU`/`mzyComposeMapArea`) writes bytes directly into it via a single
// `mzySetByte(page,col,value)` chokepoint, and `mzyRender()` (called every
// real engine frame) just blits the whole thing unconditionally via
// `md_drawColumn()`. This reproduces upstream's own real "write once,
// persists on real VRAM until next explicit write" behavior for free, with
// no need for Cracky's own explicit overlay-check mechanism at all: the map
// area (columns 0-95, from `mzyVVram`) is only ever recomposed into
// `mzyFrame` when `mzyComposeMapArea()` is actually called (once per real
// logic tick during normal play, matching upstream's own `DrawAll()`
// cadence) - and confirmed by tracing every call site that the two raw
// message writes (`PrintGameOver()`/`PrintTimeUp()`, landing at columns
// 32-67, inside the map area) are ONLY ever issued at points in the real
// control flow where nothing calls `DrawAll()`/`mzyComposeMapArea()` again
// before the next full screen reset - so they simply persist correctly with
// zero extra bookkeeping, the same real "nothing touches those bytes again"
// reasoning Cracky's own header comment documents for its own analogous
// message-overlay case, just achieved architecturally instead of via an
// explicit runtime check.
//
// **Map storage/generation - a genuinely intricate stateful bit-packing
// algorithm, ported as a direct structural mirror rather than re-derived**
// (the same "faithfully copy an intricate stateful algorithm's own shape"
// reasoning Cracky's own header already established for a comparable case):
// `Map.cpp`'s `InitMap()` unpacks each stage's own compact per-room wall-bit
// stream into a full 64x64 monochrome bitmap (`Map[512]`, 1 bit = 1 wall
// pixel) via `RotBit()`/`RotBit2()` - two tiny stateful bit-cursor helpers
// that flush a completed byte to `Map[]` (or, for `RotBit2`, to BOTH the
// current row and the row 1 below it simultaneously) whenever their shared
// `maskBit` cursor wraps past bit 7. Reproduced with `mzyMapPtr`/
// `mzyMapByte`/`mzyMaskBit` as persistent globals (replacing the C++
// module-statics and the raw `ptr<byte> pMap` cursor with a plain integer
// index into `mzyMap[]`, matching this project's own "index instead of a
// real pointer" precedent). **A genuine, load-bearing byte-truncation
// reliance, not an accident** - `maskBit <<= 1` depends on real `uint8_t`
// wraparound (`0x80<<1` becomes `0x00`) to detect "just finished a byte";
// ported with an explicit `& 0xFF` mask on every `mzyMaskBit` update, the
// same fix shape as this whole project's very first documented bug.
// Verified the exact byte-consumption arithmetic (`total maze bits =
// StageWidth*StageHeight*2`, and the trailing `if (bitCount==8) --pStage;`
// backs out exactly one over-read whenever that total is a multiple of 8)
// against two real stages by hand-tracing the refill sequence before ever
// writing this port's own `mzyStageByte()`-based cursor replacement -
// confirmed the algorithm naturally lands the cursor on the correct next
// field (ManX) in both the exact-multiple-of-8 case (Stage0, 72 maze bits)
// and the non-exact case (Stage1, 98 maze bits) with no off-by-one needed
// on this port's own side.
//
// **Stage data - resolved by id instead of a real pointer**, matching this
// project's own established "resolve by id" pattern (Tiny Dungeon's own
// bitmap-array resolver): upstream's `constptr<byte> pStage` is a raw
// pointer that gets reassigned to whichever of 10 differently-sized stage
// byte arrays the current `CurrentStage % 10` selects, then walked forward
// one byte at a time across the whole `InitMap()` call - not portable as a
// literal incrementing pointer here, so ported as `mzyStageIndex` (0-9) +
// `mzyStageCursor` (an integer offset) + `mzyStageByte(idx,cursor)` (a
// plain if/else-if dispatch into 10 separately-declared, correctly-sized
// flat arrays `mzyStage0`..`mzyStage9`) - every `*pStage; ++pStage;` read
// upstream becomes one `mzyStageByte(...)` call followed by an explicit
// `mzyStageCursor = mzyStageCursor + 1;`. All 10 arrays extracted and
// byte-diff-verified via a small Python script (evaluating the real
// `(36&0xff),(36>>8)`-style literal expressions the source uses for the
// trailing StageTime word) before ever being pasted in, matching this
// project's own "byte-diff transcribed tables" discipline - all 10 matched
// their own expected total length (StageWidth+StageHeight header + the
// exact maze-bit byte count derived above + ManX/Y + GoalX/Y + a knife
// count/list + a monster count/list + a 2-byte StageTime) on the first
// extraction attempt.
//
// **Knife/Monster data encoding**: both share the same `(x, y|type<<6)`
// packed-byte-pair encoding read from the stage stream - `InitKnives()` is
// called BEFORE `InitMonsters()` in upstream's own `InitMap()` (confirmed
// directly, not assumed from name order), so a stage's own count+list pair
// immediately after GoalX/GoalY belongs to knives, and the SECOND
// count+list pair belongs to monsters. A knife's own y-byte top 2 bits
// (masked off via `&0x3f` before use, same as a monster's) are simply
// unused/dead for knives specifically (only monsters read `b>>6` as a real
// `type` field) - confirmed by grep, not guessed, and ported as a harmless
// read-and-discard rather than omitted, matching upstream's own real
// per-byte layout exactly.
//
// **A second, distinct family of `uint8_t`-wraparound reliance, found and
// fixed the same way as the mask-bit case above**: `Man.cpp`'s
// `UpdateBasePosition()` computes `margin = MapWidth - VVramWidth` (both
// `byte`) to clamp the camera's scroll offset - for any stage narrower/
// shorter than the 24x16 viewport (e.g. Stage0's own 19x19 map), this
// subtraction genuinely underflows on real 8-bit hardware, producing a
// LARGE positive wrapped value (e.g. 251) that makes the subsequent
// `x >= margin` check always false, so the camera is left unclamped
// (`BaseX = x`, the raw offset) rather than pinned to 0 - meaning a real
// player who walks into the right/bottom half of a small maze genuinely
// sees the camera scroll a few columns past the maze's own edge, revealing
// blank void beyond it. This is real, intended-by-the-actual-hardware
// behavior (not a hidden AVR-vs-wider-int mismatch bug the way this
// project's other narrow-type bugs have been) - reproducing it faithfully
// needs the OPPOSITE of masking-away the wraparound: an explicit `& 0xFF`
// on the subtraction is REQUIRED here so the same large-positive-wrap
// happens on this port's own full-width `int`s too, otherwise a naive
// direct port would compute a genuinely negative `margin`, making
// `x >= margin` always TRUE instead of always false - the opposite,
// visibly-wrong outcome (the camera would always pin to the wrapped
// "margin" value rather than tracking the player). Monster.cpp's own
// `TestHitMan()`/`CanMove()` proximity checks use the same
// unsigned-difference-plus-one-cast-to-byte trick (`static_cast<byte>(dx+1)
// < 3`, testing whether a signed difference is in {-1,0,+1}) - ported with
// the same explicit `& 0xFF` mask at each site.
//
// **Sound**: same 3-tone-channel tracker shape as Cracky's own Sound.cpp
// (module code is genuinely near-identical between the two games, just a
// different `Tempo` constant - 220 here vs Cracky's 160), so this port
// reuses the exact same architecture: every call routes straight to
// `md_playTone(freqHz, durationSeconds)` (this shim's own multi-voice
// design already makes a dedicated per-game channel scheme unnecessary),
// three independent frame-stepped sequencer slots (0=one-shot SFX -
// Loose/Hit/Beep/Get; 1=jingle/BGM-voice-A - Start/Clear/GameOver reused
// for both, exactly like upstream's own channel 1; 2=BGM-voice-B) advancing
// every real engine frame regardless of the coarser `MZY_TICK_DIVISOR`
// gameplay-tick gate, and `mzyNoteFrames(length) = round(length *
// (300/Tempo))` derived directly from `SoundHandler()`'s own real tempo
// formula (`time -= Tempo; if(time<=0){ time+=600/2; Next(); }`, i.e. a
// channel advances once every `300/Tempo` real 60Hz ticks on average) -
// `300/220 ≈ 1.364` here, vs Cracky's own `300/160 = 1.875`. All 9 melodies
// (Loose/Hit/Beep/Get/Start/Clear/GameOver/2 BGM voices) byte-diff-
// extracted via a small Python script evaluating the real `NoteLength`/
// `Scale` enum expressions (`N8`, `N2+N8`, `A4`, etc) rather than hand-
// converted, matching this project's own established anti-transcription-
// bug discipline.
//
// **The blocking upstream control flow (one big `goto`-chained `while(true)`
// / `do-while` in `Main()`, several real `WaitTimer()`/`WaitMelody()`
// blocking waits) rewritten as an explicit frame-stepped state machine**,
// the same treatment every port in this project needs:
// MZY_STATE_TITLE (`Title()`'s own internal key-poll loop, with the real
// blocking `while(ScanKeys()!=0);` wait-for-release replaced by proper
// edge detection, matching Cracky's own already-established fix for the
// identical pattern), MZY_STATE_START_JINGLE (the blocking `Sound_Start()`
// held before play begins), MZY_STATE_PLAYING (the main tick-gated loop,
// `MZY_TICK_DIVISOR=6` derived directly from `DrawAll()`'s own real
// `WaitTimer(6)` - a genuine fixed ~10-logic-ticks/second rate, not an
// AVR-performance-compromise the way several other ports' own throttles
// have been), MZY_STATE_TIMEUP_WAIT (`PrintTimeUp()` + the real
// `repeat(15){Sound_Loose();}` sequential-note burst), MZY_STATE_LOSE_ANIM
// (`EndMan()`'s own real 10-iteration blink-and-redraw loop, each iteration
// gated to the same `MZY_TICK_DIVISOR` real-time pace via its own
// `WaitTimer(6)`), MZY_STATE_GAMEOVER_JINGLE, MZY_STATE_CLEAR_JINGLE (the
// blocking `Sound_Clear()` played once a stage's Goal is reached), and
// MZY_STATE_BONUS_TALLY (the real `while(StageTime!=0){AddScore(1); ...;
// Sound_Beep(); WaitTimer(2);}` per-second bonus-point countdown, converted
// to one decrement+beep per real completed wait, matching this project's
// own HollowSeeker/Ardumania bonus-tally precedent).
//
// **A real rendering bug, found and fixed during a later, dedicated
// verification pass (see that pass's own writeup further below): an
// earlier revision of this port took the same "skip the bitmap logo, draw
// plain text instead" shortcut Cracky's own header describes for its
// "CRACKY"/"INUFU" logo - but unlike Cracky's own title word, "MAZY"
// cannot actually be spelled in the font that shortcut relies on.**
// `mzyPrintC()`'s own `mzyAsciiIndex()` linear-searches a fixed 27-
// character `AsciiTable` (" 0123456789>ACEFGIMNOPRSTUV") and silently
// falls back to the blank/space glyph for anything not found in it,
// exactly matching upstream's own `PrintC()` - and that table has no 'Z'
// and no 'Y' at all. Printing "MAZY" via `mzyPrintS()` therefore rendered
// as "MA" followed by two blank glyphs, immediately visible the moment the
// title screen was actually looked at (not caught at initial port time,
// since the file was only test-compiled, never screenshotted, before this
// pass). **Fixed** by restoring upstream's own real mechanism instead of
// patching around the font: the title screen's genuine 64-byte hand-drawn
// "MAZY" bitmap logo (4 letters x 4x4 VVram cells each, built from 16
// special "logo" glyphs at the very start of `CharPattern[]`, each a
// 4x4-pixel sub-block combinable into large letter shapes) writes raw
// VVram cell VALUES directly - it never goes through `PrintC`/`AsciiTable`
// at all, sidestepping the font limitation entirely rather than working
// around it. `mzyCharPattern[0..31]` (the "logo" range, previously left
// zero/blank under the old shortcut) and a new `mzyTitleBytes[64]` table
// were both byte-diff-verified against `Chars.cpp`/`Status.cpp` via a
// Python script before being pasted in, matching this project's own
// established anti-transcription-bug discipline; `mzyBeginTitle()`'s own
// logo-drawing loop is a direct structural mirror of upstream's own
// nested `repeat(4){repeat(4){repeat(4){...}}}` pointer-walk, and "MINI"
// (a real word using only M/I/N, all present in `AsciiTable` - not
// affected by this bug) was repositioned from column 0 to its real
// upstream column 64, drawn via `mzyPrintS()` *after* the logo's own
// `mzyComposeMapArea()` call rather than before - matching upstream's own
// exact `VVramToVram()`-then-`PrintS()` order, since the map-area
// composite would otherwise overwrite (erase) any text drawn into
// `mzyFrame` ahead of it. Every other status/title string in this file
// was independently re-checked against the same `AsciiTable` afterward
// (via the identical script) and confirmed clean - this was the only
// occurrence of this bug class in the file. Index 0 doubles as both
// `MZY_CHAR_SPACE` (blank map tile) and the logo's own top-left sub-block
// glyph, which are naturally both all-zero upstream too - populating the
// logo range introduces no conflict with the main map/status rendering.
//
// **Two more genuine upstream quirks, checked and preserved rather than
// "fixed"**: (1) `EndMan()`'s own loop-counter variable (`byte i;`,
// declared OUTSIDE the `repeat(10)` macro's own internal counter and read
// via `Patterns[i]` BEFORE ever being assigned on the very first
// iteration) is genuinely uninitialized in the original C++ source - a
// real out-of-bounds-adjacent risk that happens to be harmless on real
// AVR/RISC-V flash (silently reads whatever 4-entry-table-adjacent byte
// follows) but is a genuine memory-safety concern on this platform; fixed
// with an explicit `mzyAnimStep = 0;` before the loop begins, the same
// "preserve behavior, guard the crash" treatment this project has applied
// to comparable uninitialized-local bugs elsewhere (e.g. Tiny Lander's own
// `SetLandingMap()` fix). (2) `Score`/`CurrentStage`/`RemainCount` are
// deliberately NOT reset when the game-over path `goto title`s back to the
// title screen (only `Score`/`RemainCount` get reset AFTER a fresh
// selection is made) - this is load-bearing for the real "CONTINUE" menu
// option (`selection==1` skips resetting `CurrentStage`, letting a
// continued game resume on the same stage the player died on) - preserved
// exactly. The ONE targeted reset this port does add before showing the
// title screen, matching Cracky's own already-shipped, already-explained
// fix for the identical root cause: `mzyStageTime` (a per-life countdown
// value with no "final score" meaning worth showing) and
// `mzyHeldKnifeCount` are reset to 0 before `mzyPrintStatus()` runs on the
// title screen, so a fresh game-over doesn't show a stale leftover
// countdown/knife-icon state alongside the (deliberately preserved) real
// Score/Stage/RemainCount values.
//
// **A dedicated, later verification pass** (this file had only ever been
// test-compiled before, never screenshot-tested or played, as part of a
// larger parallel porting batch) re-audited every upstream file line by
// line against this port and byte-diff-verified every data table via
// Python scripts: `Main.cpp`'s own control flow (including the subtle
// upstream priority quirk where a same-tick TimeUp-to-0 takes precedence
// over a same-tick StageClear, via `goto lose` skipping the
// `while(!StageClear)` check entirely - confirmed the port's own
// `mzyUpdatePlaying()` checks/returns on TimeUp before ever reaching its
// own StageClear check, matching exactly), `Man.cpp`, `Knife.cpp`,
// `Monster.cpp` (including the `goto move`/shared-label dedup in upstream
// `MoveMonsters()`, confirmed the port's own `mzyMonsterCommitMove()`
// helper reproduces it faithfully for all 3 monster types), `Map.cpp`'s
// own `RotBit`/`RotBit2` bit-unpacker, `Math.cpp` (`Rnd()` reconfirmed
// dead/unused via grep), `Sound.cpp`/`Timer.cpp` (confirmed `SoundHandler`
// runs at a genuine, real 60Hz `SysTick` rate, not an AVR-performance
// compromise - matching this file's own already-documented `mzyNoteFrames`
// derivation), `Vram.cpp`/`VVram.cpp`/`Print.cpp`/`Status.cpp`'s own exact
// pixel-addressing formulas (including `PrintHeldKnives()`'s asymmetric
// `VramStep+1`-vs-plain-`VramStep` icon/blank stride, confirmed
// byte-exact), and every one of `AsciiPattern`/`CharPattern`/all 10
// `Stages[]`/`Frequencies`/all 9 melody tables (all confirmed byte-exact
// on the first extraction attempt, except the logo-range `CharPattern`
// bytes and the missing `TitleBytes[]` table - see the bug fix above).
// The one real bug this pass found (the "MAZY" title-logo font bug, see
// above) was fixed and then verified live via this project's own
// Puppeteer/WebGL test harness in an isolated environment (a separate
// `WebBuild` copy and port, to avoid colliding with other concurrently-
// running verification agents sharing the same repo): the corrected
// bitmap logo renders correctly, selection toggling between START/
// CONTINUE works, and a full real playthrough was exercised end-to-end -
// launching a game, moving, picking up and throwing a knife (confirmed
// via the held-knife-icon count updating), and then letting the stage
// timer run out three consecutive times un-played (deliberately, to
// exercise every state transition without depending on ever landing a
// monster kill) confirmed each TimeUp correctly: plays the "TIME UP"
// message, decrements `RemainCount` and correctly redraws a shorter
// life-icon row, restarts the same stage with a freshly reset timer/maze
// for the first two losses, and on the third correctly shows "GAME OVER",
// plays the game-over jingle, and returns cleanly to the title screen
// with `Score`/`CurrentStage` preserved (matching the deliberately-
// preserved-for-CONTINUE behavior already documented above) and no
// life-icons shown (since `RemainCount` is genuinely 0 at that point,
// matching upstream's own `if (RemainCount > 1)` gate exactly). Not
// independently exercised this pass: an actual monster kill (score
// increase) and a real stage-clear/bonus-tally/next-stage transition -
// both reuse logic paths already traced and confirmed correct against
// upstream by direct code reading (see `mzyMonsterHitKnife()`/
// `mzyUpdateClearJingle()`/`mzyUpdateBonusTally()` above), so risk is low,
// but worth a direct check if either is ever reported as off.
//
// **Re-audited against Cracky's own later "wide title-screen text" fix,
// found NOT to need it - a genuine architectural difference, not an
// oversight.** A real user-supplied hardware photo of Cracky proved that
// port's own original `crkStatusChar[8][8]` status-text model was wrong -
// it only ever modeled upstream's real narrow status-label zone (columns
// 24-31 in char-cell units) but then also crammed the *title screen's*
// own much wider text (columns 8-23) into that same narrow grid, causing
// text to overflow/truncate/collide with SCORE/STAGE/TIME. This game's
// own port never had that problem to begin with: `mzyFrame` was built
// from day one as a real, full 128-real-pixel-column persistent
// framebuffer (see "Rendering architecture" above), and every text call
// site in this file already writes at upstream's own real, literal pixel
// columns (`MZY_STATUS_COL`=96 for the status zone, matching upstream's
// `LeftX=24 * VramStep=4`; `9*4`/`16*4`/`12*4`/`8*4` for START/CONTINUE/
// MINI/the credit line/the selection cursor, matching upstream
// `Status.cpp`'s own literal `ArrowX=9`/`16`/`12`/`8` column constants
// exactly - re-checked side by side against the real upstream source
// during this audit, not just trusted from memory). Confirmed both by
// this direct source comparison and by a live screenshot from an
// isolated Puppeteer/WebGL test instance: the title screen renders
// "MAZY" (the real bitmap logo), "MINI", "▶START"/"CONTINUE" (the full
// 8-letter word, never truncated), and "INUFUTO 2026" all fully spelled
// out and non-overlapping, with SCORE/STAGE/TIME/the life icons intact
// on the right - matching the shape of Cracky's own now-corrected real-
// hardware-photo reference layout. No code change was needed here.
//
// Entry points: `gameMazy_init()` / `gameMazy_update()`.
// =============================================================================

// -----------------------------------------------------------------------------
//   Chars.h
// -----------------------------------------------------------------------------

#define MZY_CHAR_SPACE 0x00
#define MZY_CHAR_KN_LEFT 0x10
#define MZY_CHAR_KN_RIGHT 0x11
#define MZY_CHAR_KN_UP 0x12
#define MZY_CHAR_KN_DOWN 0x13
#define MZY_CHAR_WALL 0x14
#define MZY_CHAR_MAN_LEFT 0x15
#define MZY_CHAR_MAN_LEFT_STOP 0x15
#define MZY_CHAR_MAN_RIGHT 0x25
#define MZY_CHAR_MAN_RIGHT_STOP 0x25
#define MZY_CHAR_MAN_UP 0x35
#define MZY_CHAR_MAN_UP_STOP 0x35
#define MZY_CHAR_MAN_DOWN 0x45
#define MZY_CHAR_MAN_DOWN_STOP 0x45
#define MZY_CHAR_MAN_DIE2 0x55
#define MZY_CHAR_MAN_DIE3 0x59
#define MZY_CHAR_CHASER 0x5D
#define MZY_CHAR_DISTURBER 0x61
#define MZY_CHAR_GOAL 0x65
#define MZY_CHAR_BANG 0x69
#define MZY_CHAR_REMAIN MZY_CHAR_MAN_LEFT

// -----------------------------------------------------------------------------
//   ScanKeys.h
// -----------------------------------------------------------------------------

#define MZY_KEYS_LEFT 0x01
#define MZY_KEYS_RIGHT 0x02
#define MZY_KEYS_UP 0x04
#define MZY_KEYS_DOWN 0x08
#define MZY_KEYS_DIR ( MZY_KEYS_LEFT | MZY_KEYS_RIGHT | MZY_KEYS_UP | MZY_KEYS_DOWN )
#define MZY_KEYS_BUTTON0 0x10

// -----------------------------------------------------------------------------
//   Direction indices - shared by Man.cpp's Directions[] and Knife.cpp's
//   Direction_Left/Right/Up/Down + Vectors[] (confirmed identical numbering
//   and identical per-direction (dx,dy) offsets in both upstream files, so
//   this port uses one shared table for both rather than duplicating it).
// -----------------------------------------------------------------------------

#define MZY_DIR_LEFT 0
#define MZY_DIR_RIGHT 1
#define MZY_DIR_UP 2
#define MZY_DIR_DOWN 3

// -----------------------------------------------------------------------------
//   Map.h
// -----------------------------------------------------------------------------

#define MZY_MAX_MAP_WIDTH 64
#define MZY_MAX_MAP_HEIGHT 64
#define MZY_MAP_STEP 8
#define MZY_MAP_SIZE ( MZY_MAP_STEP * MZY_MAX_MAP_HEIGHT )

// -----------------------------------------------------------------------------
//   VVram.h / Vram.h
// -----------------------------------------------------------------------------

#define MZY_VVRAM_WIDTH 24
#define MZY_VVRAM_HEIGHT 16
#define MZY_STATUS_COL 96

// -----------------------------------------------------------------------------
//   Knife.h
// -----------------------------------------------------------------------------

#define MZY_KNIFE_STATUS_NONE 0
#define MZY_KNIFE_STATUS_FLOOR 1
#define MZY_KNIFE_STATUS_HELD 2
#define MZY_KNIFE_STATUS_FLYING 3
#define MZY_KNIFE_COUNT 4

struct MzyKnife
{
    int x, y;
    int status;
    int direction;
};

// -----------------------------------------------------------------------------
//   Monster.h
// -----------------------------------------------------------------------------

#define MZY_MONSTER_STATUS_NONE 0
#define MZY_MONSTER_STATUS_LIVE 1
#define MZY_MONSTER_STATUS_END 2
#define MZY_MONSTER_TYPE_HORIZONTAL 0
#define MZY_MONSTER_TYPE_VERTICAL 1
#define MZY_MONSTER_TYPE_CHASE 2
#define MZY_MONSTER_COUNT 8

struct MzyMonster
{
    int x, y;
    int type;
    int status;
    int clock;
};

// -----------------------------------------------------------------------------
//   Stages.h
// -----------------------------------------------------------------------------

#define MZY_STAGE_COUNT 10

// -----------------------------------------------------------------------------
//   Sound.h - NoteLength/Scale, ported as real expressions matching
//   upstream's own enum exactly (Tempo=220, differs from Cracky's 160).
// -----------------------------------------------------------------------------

#define MZY_N8 6
#define MZY_N4 ( MZY_N8 * 2 )
#define MZY_N4P ( MZY_N4 * 3 / 2 )
#define MZY_N2 ( MZY_N4 * 2 )

#define MZY_D3 11
#define MZY_F2 2
#define MZY_G2 4
#define MZY_A2 6
#define MZY_C3 9
#define MZY_C4 21
#define MZY_D4 23
#define MZY_E4 25
#define MZY_F4 26
#define MZY_G4 28
#define MZY_A4 30
#define MZY_A3 18
#define MZY_B4 32
#define MZY_C5 33

#define MZY_TEMPO 220

#define MZY_MELODY_NONE 0
#define MZY_MELODY_LOOSE 1
#define MZY_MELODY_HIT 2
#define MZY_MELODY_BEEP 3
#define MZY_MELODY_GET 4
#define MZY_MELODY_START 5
#define MZY_MELODY_CLEAR 6
#define MZY_MELODY_GAMEOVER 7
#define MZY_MELODY_BGM1 8
#define MZY_MELODY_BGM2 9

// -----------------------------------------------------------------------------
//   Data tables - byte-diff-extracted via a small Python script from the
//   real upstream source (Chars.cpp / Sound.cpp / Stages.cpp), not hand-
//   copied.
// -----------------------------------------------------------------------------

// AsciiPattern - " 0123456789>ACEFGIMNOPRSTUV", 4 bytes/glyph. Byte-for-byte
// identical to Cracky's own copy of the same shared "Cate engine" font
// asset (confirmed by direct comparison), not just assumed so.
int[108] mzyAsciiPattern = {
    0x00, 0x00, 0x00, 0x00, 0x1f, 0x11, 0x1f, 0x00,
    0x00, 0x00, 0x1f, 0x00, 0x1d, 0x15, 0x17, 0x00,
    0x15, 0x15, 0x1f, 0x00, 0x07, 0x04, 0x1f, 0x00,
    0x17, 0x15, 0x1d, 0x00, 0x1f, 0x15, 0x1d, 0x00,
    0x01, 0x1d, 0x03, 0x00, 0x1f, 0x15, 0x1f, 0x00,
    0x17, 0x15, 0x1f, 0x00, 0x1f, 0x0e, 0x04, 0x00,
    0x1e, 0x09, 0x1e, 0x00, 0x0e, 0x11, 0x0a, 0x00,
    0x1f, 0x15, 0x11, 0x00, 0x1f, 0x05, 0x01, 0x00,
    0x0e, 0x11, 0x0d, 0x00, 0x11, 0x1f, 0x11, 0x00,
    0x1f, 0x06, 0x1f, 0x00, 0x1f, 0x01, 0x1e, 0x00,
    0x0e, 0x11, 0x0e, 0x00, 0x1f, 0x05, 0x07, 0x00,
    0x1f, 0x05, 0x1a, 0x00, 0x16, 0x15, 0x0d, 0x00,
    0x01, 0x1f, 0x01, 0x00, 0x1f, 0x10, 0x1f, 0x00,
    0x0f, 0x10, 0x0f, 0x00,
};

// CharPattern - 109 map-tile glyphs, 2 bytes/glyph. Indices 0-15 ("logo",
// the title-screen bitmap font, real data restored - see the header
// comment's own bug-fix note on why a plain-text substitute is unusable
// here) plus every real gameplay glyph (0x10 = MZY_CHAR_KN_LEFT and up) are
// all present and byte-exact against upstream. Index 0 doubles as
// MZY_CHAR_SPACE (blank map tile) and the logo's own top-left sub-block,
// which are naturally both all-zero upstream too - no conflict.
int[218] mzyCharPattern = {
    0x00, 0x00, 0x33, 0x00, 0xcc, 0x00, 0xff, 0x00,
    0x00, 0x33, 0x33, 0x33, 0xcc, 0x33, 0xff, 0x33,
    0x00, 0xcc, 0x33, 0xcc, 0xcc, 0xcc, 0xff, 0xcc,
    0x00, 0xff, 0x33, 0xff, 0xcc, 0xff, 0xff, 0xff,
    0x62, 0x66, 0x66, 0x26, 0xf0, 0x0e, 0xf0, 0x07,
    0xa5, 0xa5, 0x80, 0xf5, 0x7d, 0x08, 0x10, 0x3c,
    0xc3, 0x01, 0x00, 0xf5, 0xfd, 0x00, 0x90, 0x34,
    0x43, 0x05, 0x00, 0x75, 0x7d, 0x00, 0x00, 0xd0,
    0xc1, 0x00, 0x00, 0xf5, 0x7d, 0x00, 0x10, 0x2d,
    0x43, 0x05, 0x80, 0xd7, 0x5f, 0x08, 0x10, 0x3c,
    0xc3, 0x01, 0x00, 0xdf, 0x5f, 0x00, 0x50, 0x34,
    0x43, 0x09, 0x00, 0xd7, 0x57, 0x00, 0x00, 0x1c,
    0x0d, 0x00, 0x00, 0xd7, 0x5f, 0x00, 0x50, 0x34,
    0xd2, 0x01, 0x8c, 0xac, 0x8a, 0x44, 0x23, 0x53,
    0x15, 0x22, 0x8c, 0x8c, 0x8a, 0x24, 0x23, 0x33,
    0x15, 0x06, 0x8c, 0x0c, 0x08, 0x88, 0x23, 0x13,
    0x01, 0x22, 0x8c, 0x8c, 0x86, 0x44, 0x23, 0x13,
    0x15, 0x06, 0x4c, 0xac, 0x8a, 0x44, 0x13, 0x53,
    0x15, 0x22, 0x4c, 0xcc, 0x8a, 0x06, 0x13, 0x13,
    0x15, 0x42, 0x4c, 0x8c, 0x08, 0x44, 0x13, 0x03,
    0x01, 0x11, 0x4c, 0x8c, 0x8a, 0x06, 0x13, 0x13,
    0x16, 0x22, 0x80, 0xc3, 0x3c, 0x08, 0x10, 0xbe,
    0xaf, 0x01, 0x44, 0xa8, 0xca, 0xc8, 0x22, 0x51,
    0x35, 0x32, 0x60, 0x69, 0x68, 0x69, 0xc0, 0x36,
    0x3f, 0xc6, 0xc0, 0x3e, 0x3f, 0xce, 0xc0, 0x37,
    0x3f, 0xc7, 0xf0, 0xff, 0x7f, 0x0f, 0xf0, 0xff,
    0xef, 0x0f, 0x20, 0x84, 0x8e, 0x24, 0x90, 0x35,
    0x3f, 0x95,
};

// TitleBytes - the real upstream title-screen "MAZY" bitmap logo, 4 letters
// x 4 rows x 4 cols of raw VVram-cell values (each indexing into
// mzyCharPattern's own "logo" range, 0-15). Byte-diff-verified against
// Status.cpp's own Title() function. Used instead of plain text specifically
// because the status-text AsciiTable font has no 'Z' or 'Y' glyph at all -
// see the header comment's own bug-fix note.
int[64] mzyTitleBytes = {
    0x0c, 0x07, 0x07, 0x0b,
    0x0c, 0x03, 0x03, 0x0f,
    0x0c, 0x03, 0x03, 0x0f,
    0x04, 0x01, 0x01, 0x05,
    0x00, 0x0e, 0x0d, 0x02,
    0x0c, 0x03, 0x00, 0x0f,
    0x0c, 0x07, 0x05, 0x0f,
    0x04, 0x01, 0x00, 0x05,
    0x04, 0x05, 0x0d, 0x07,
    0x00, 0x08, 0x07, 0x00,
    0x08, 0x07, 0x00, 0x00,
    0x04, 0x05, 0x05, 0x05,
    0x0c, 0x03, 0x0c, 0x03,
    0x04, 0x0b, 0x0e, 0x01,
    0x00, 0x0c, 0x03, 0x00,
    0x00, 0x04, 0x01, 0x00,
};

// Standard equal-tempered note frequencies, E2..G5 (Scale enum values 1-40).
// Identical table to Cracky's own crkFrequencies (same shared engine).
int[40] mzyFrequencies = {
    82, 87, 92, 98, 104, 110, 117, 123, 131, 139,
    147, 156, 165, 175, 185, 196, 208, 220, 233, 247,
    262, 277, 294, 311, 330, 349, 370, 392, 415, 440,
    466, 494, 523, 554, 587, 622, 659, 698, 740, 784,
};

int[3] mzyMelodyLoose = { 1, MZY_A3, 0 };
int[17] mzyMelodyHit = {
    1, MZY_F4, 1, MZY_G4, 1, MZY_A4, 1, MZY_B4, 1, MZY_C5,
    1, 35, 1, 37, 1, 38, 0,
};
int[3] mzyMelodyBeep = { 1, MZY_A4, 0 };
int[3] mzyMelodyGet = { 1, MZY_A4, 0 };
int[11] mzyMelodyStart = {
    MZY_N8, MZY_C4, MZY_N8, MZY_G4, MZY_N8, MZY_E4, MZY_N8, MZY_G4, MZY_N2, MZY_C5, 0,
};
int[21] mzyMelodyClear = {
    MZY_N8, MZY_C4, MZY_N8, MZY_E4, MZY_N8, MZY_G4, MZY_N8, MZY_D4, MZY_N8, MZY_F4,
    MZY_N8, MZY_A4, MZY_N8, MZY_E4, MZY_N8, MZY_G4, MZY_N8, MZY_B4, MZY_N4P, MZY_C5, 0,
};
int[23] mzyMelodyGameOver = {
    MZY_N8, MZY_C5, MZY_N8, MZY_G4, MZY_N8, MZY_E4, MZY_N8, MZY_C5, MZY_N8, MZY_B4,
    MZY_N8, MZY_G4, MZY_N8, MZY_E4, MZY_N8, MZY_B4, MZY_N4, MZY_A4, MZY_N4, MZY_B4,
    MZY_N2, MZY_C5, 0,
};
int[89] mzyMelodyBgm1 = {
    6, 23, 6, 26, 6, 28, 6, 30, 6, 30, 6, 30, 6, 30, 6, 30,
    6, 30, 6, 30, 6, 30, 6, 28, 6, 28, 6, 28, 6, 28, 6, 28,
    6, 28, 6, 30, 6, 28, 24, 26, 24, 28, 30, 30, 6, 23, 6, 26,
    6, 28, 6, 30, 6, 30, 6, 30, 6, 30, 6, 30, 6, 30, 6, 30,
    6, 30, 6, 28, 6, 28, 6, 28, 6, 28, 6, 28, 6, 28, 6, 30,
    6, 28, 24, 26, 24, 28, 30, 26, 255,
};
int[41] mzyMelodyBgm2 = {
    18, 0, 18, 2, 18, 2, 12, 2, 18, 9, 18, 9, 12, 9, 24, 2,
    24, 4, 30, 6, 18, 0, 18, 6, 18, 6, 12, 6, 18, 4, 18, 4,
    12, 4, 24, 2, 24, 9, 30, 2, 255,
};

// Stage data - each stage's own byte stream: [StageWidth, StageHeight],
// maze wall bits (RotBit/RotBit2-consumed, StageWidth*StageHeight*2 bits),
// [ManX_raw, ManY_raw], [GoalX_raw, GoalY_raw], [knifeCount, (x,y|dir<<6)...],
// [monsterCount, (x,y|type<<6)...], [StageTimeLo, StageTimeHi]. Extracted
// and byte-diff-verified via script; every stage's own total element count
// was independently confirmed to match this exact field layout.
int[27] mzyStage0 = {
    6, 6, 164, 4, 159, 45, 133, 214, 96, 69, 255, 0, 0, 5, 5, 2,
    2, 1, 0, 5, 2, 5, 131, 4, 68, 36, 0,
};
int[31] mzyStage1 = {
    7, 7, 224, 91, 145, 174, 86, 93, 81, 78, 89, 80, 7, 254, 3, 5,
    2, 0, 0, 1, 6, 2, 3, 2, 64, 6, 128, 1, 133, 49, 0,
};
int[34] mzyStage2 = {
    8, 8, 212, 85, 161, 106, 166, 185, 150, 106, 144, 46, 249, 172, 247, 16,
    132, 255, 7, 0, 3, 5, 1, 7, 2, 3, 0, 128, 1, 5, 4, 135,
    64, 0,
};
int[41] mzyStage3 = {
    9, 9, 180, 45, 17, 180, 134, 183, 229, 88, 218, 67, 241, 146, 92, 75,
    208, 29, 78, 35, 196, 255, 3, 0, 3, 8, 8, 2, 1, 3, 8, 3,
    3, 2, 129, 7, 134, 8, 134, 81, 0,
};
int[43] mzyStage4 = {
    10, 9, 68, 75, 85, 107, 69, 89, 179, 117, 54, 46, 132, 202, 213, 181,
    107, 26, 10, 198, 166, 73, 52, 255, 15, 0, 0, 8, 4, 1, 4, 8,
    4, 3, 128, 5, 128, 7, 66, 9, 4, 90, 0,
};
int[53] mzyStage5 = {
    10, 9, 12, 23, 39, 115, 105, 94, 202, 68, 126, 13, 37, 179, 98, 191,
    204, 8, 186, 139, 186, 27, 64, 254, 15, 9, 0, 0, 0, 4, 4, 2,
    5, 2, 9, 5, 9, 7, 6, 0, 66, 0, 133, 0, 134, 0, 135, 1,
    136, 2, 136, 90, 0,
};
int[56] mzyStage6 = {
    11, 10, 100, 172, 41, 83, 105, 150, 81, 215, 22, 85, 173, 80, 221, 66,
    121, 105, 37, 194, 234, 103, 147, 164, 221, 54, 10, 3, 255, 15, 3, 5,
    8, 4, 2, 2, 2, 8, 7, 7, 5, 130, 10, 130, 6, 132, 10, 133,
    8, 70, 2, 137, 5, 9, 110, 0,
};
int[61] mzyStage7 = {
    12, 11, 216, 232, 210, 164, 89, 75, 89, 204, 114, 130, 233, 171, 107, 76,
    25, 162, 154, 89, 143, 76, 238, 163, 141, 73, 181, 201, 170, 138, 94, 30,
    16, 248, 255, 4, 1, 11, 7, 4, 2, 0, 11, 0, 5, 5, 7, 8,
    5, 3, 133, 11, 133, 8, 134, 8, 135, 8, 136, 132, 0,
};
int[70] mzyStage8 = {
    12, 12, 36, 239, 26, 52, 217, 104, 172, 61, 74, 76, 106, 183, 52, 233,
    180, 233, 202, 146, 44, 91, 138, 84, 169, 210, 230, 190, 18, 65, 174, 27,
    48, 173, 53, 5, 250, 255, 11, 0, 2, 0, 4, 5, 2, 10, 2, 11,
    8, 9, 11, 8, 6, 130, 0, 3, 2, 3, 4, 3, 8, 3, 0, 133,
    5, 133, 11, 74, 144, 0,
};
int[70] mzyStage9 = {
    12, 12, 117, 200, 112, 196, 88, 167, 186, 221, 24, 135, 138, 222, 213, 77,
    69, 175, 15, 26, 156, 104, 218, 44, 57, 183, 7, 143, 30, 153, 200, 214,
    72, 73, 146, 164, 250, 255, 10, 9, 0, 11, 4, 2, 0, 6, 0, 5,
    9, 11, 11, 8, 8, 128, 8, 129, 7, 130, 4, 131, 8, 131, 3, 133,
    2, 137, 1, 75, 144, 0,
};

// Direction tables shared by Man (movement) and Knife (flight) - both
// upstream files independently define the exact same 4 (dx,dy) offsets in
// the exact same Left/Right/Up/Down order, confirmed by direct comparison.
int[4] mzyDirDx = { -1, 1, 0, 0 };
int[4] mzyDirDy = { 0, 0, -1, 1 };
int[4] mzyDirPattern = { MZY_CHAR_MAN_LEFT, MZY_CHAR_MAN_RIGHT, MZY_CHAR_MAN_UP, MZY_CHAR_MAN_DOWN };
int[4] mzyDirKeyMask = { MZY_KEYS_LEFT, MZY_KEYS_RIGHT, MZY_KEYS_UP, MZY_KEYS_DOWN };

// -----------------------------------------------------------------------------
//   Global state
// -----------------------------------------------------------------------------

int mzyScore;
int mzyRemainCount;
int mzyTimeRate;
int mzyCurrentStage;
int mzyStageTime;
bool mzyStageClear;
bool mzyManLost;
int mzyClock;

int mzyBaseX, mzyBaseY;
int mzyGoalX, mzyGoalY;
int mzyStageWidth, mzyStageHeight;
int mzyMapWidth, mzyMapHeight;

int[MZY_MAP_SIZE] mzyMap;
int mzyMaskBit, mzyMapByte, mzyMapPtr;

int mzyStageIndex;
int mzyStageCursor;

int[MZY_VVRAM_HEIGHT][MZY_VVRAM_WIDTH] mzyVVram;
int[8][128] mzyFrame;

int mzyManX, mzyManY;
int mzyManPrevDir;
int mzyManPattern;
int mzyManSeqIndex;
bool mzyManButtonOn;

bool mzyGoalBlink;
bool mzyKnifeBlink;
int mzyHeldKnifeCount;

MzyKnife[MZY_KNIFE_COUNT] mzyKnives;
MzyMonster[MZY_MONSTER_COUNT] mzyMonsters;

int[3] mzySeqMelody;
int[3] mzySeqPos;
int[3] mzySeqWait;
int[3] mzySeqActive;

#define MZY_TICK_DIVISOR 6
int mzyTickCounter;

#define MZY_STATE_TITLE 0
#define MZY_STATE_START_JINGLE 1
#define MZY_STATE_PLAYING 2
#define MZY_STATE_TIMEUP_WAIT 3
#define MZY_STATE_LOSE_ANIM 4
#define MZY_STATE_GAMEOVER_JINGLE 5
#define MZY_STATE_CLEAR_JINGLE 6
#define MZY_STATE_BONUS_TALLY 7
int mzyState;
int mzyWaitFrames;
int mzyAnimStep;
int mzyTimeUpStep;
int mzySelection;
bool mzySelectionChanged;
bool mzyPrevLeft, mzyPrevRight, mzyPrevUp, mzyPrevDown, mzyPrevFire;
bool mzyPendingContinue;


// -----------------------------------------------------------------------------
//   Stages.cpp - resolve-by-id stage byte stream (replaces the raw
//   incrementing `constptr<byte> pStage`).
// -----------------------------------------------------------------------------

int mzyStageByte( int idx, int cursor )
{
    if( idx == 0 ) return mzyStage0[ cursor ];
    if( idx == 1 ) return mzyStage1[ cursor ];
    if( idx == 2 ) return mzyStage2[ cursor ];
    if( idx == 3 ) return mzyStage3[ cursor ];
    if( idx == 4 ) return mzyStage4[ cursor ];
    if( idx == 5 ) return mzyStage5[ cursor ];
    if( idx == 6 ) return mzyStage6[ cursor ];
    if( idx == 7 ) return mzyStage7[ cursor ];
    if( idx == 8 ) return mzyStage8[ cursor ];
    return mzyStage9[ cursor ];
}


// -----------------------------------------------------------------------------
//   Math.cpp (Abs only - Rnd() is declared upstream but never actually
//   called anywhere in this game, confirmed by grep, so it's not ported)
// -----------------------------------------------------------------------------

int mzyAbs( int a, int b )
{
    if( a < b )
      return b - a;
    return a - b;
}

int mzyToMapSize( int a )
{
    return ( a << 1 ) + a + 1;
}


// -----------------------------------------------------------------------------
//   Vram.cpp / Print.cpp - raw persistent framebuffer + status-text
//   primitives. See header comment for why this port uses a real pixel
//   framebuffer rather than Cracky's own indexed status-cell grid.
// -----------------------------------------------------------------------------

void mzySetByte( int page, int col, int value )
{
    if( page < 0 || page > 7 ) return;
    if( col < 0 || col > 127 ) return;
    mzyFrame[ page ][ col ] = value & 0xFF;
}

int mzyAsciiIndex( int c )
{
    // AsciiTable = " 0123456789>ACEFGIMNOPRSTUV" - direct port of upstream
    // PrintC()'s own linear search (only 27 entries, no cost concern).
    int[27] table = {
        ' ', '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '>',
        'A', 'C', 'E', 'F', 'G', 'I', 'M', 'N', 'O', 'P', 'R', 'S', 'T', 'U', 'V',
    };
    int i;
    for( i = 0; i < 27; i = i + 1 )
    {
        if( table[ i ] == c )
          return i;
    }
    return 0;
}

int mzyPrintC( int page, int col, int c )
{
    int idx, i;
    idx = mzyAsciiIndex( c );
    for( i = 0; i < 4; i = i + 1 )
      mzySetByte( page, col + i, mzyAsciiPattern[ idx * 4 + i ] );
    return col + 4;
}

int mzyPrintS( int page, int col, int* s, int len )
{
    int i;
    for( i = 0; i < len; i = i + 1 )
      col = mzyPrintC( page, col, s[ i ] );
    return col;
}

int mzyPrintByteNumber2( int page, int col, int b )
{
    int d1;
    d1 = b / 10;
    if( d1 == 0 )
      col = mzyPrintC( page, col, ' ' );
    else
      col = mzyPrintC( page, col, d1 + '0' );
    col = mzyPrintC( page, col, ( b % 10 ) + '0' );
    return col;
}

int mzyPrintNumber5( int page, int col, int w )
{
    int i, d, rem, div;
    bool zeroVisible;
    rem = w;
    div = 10000;
    zeroVisible = false;
    for( i = 0; i < 4; i = i + 1 )
    {
        d = rem / div;
        rem = rem % div;
        if( d == 0 && !zeroVisible )
          col = mzyPrintC( page, col, ' ' );
        else
        {
            zeroVisible = true;
            col = mzyPrintC( page, col, d + '0' );
        }
        div = div / 10;
    }
    col = mzyPrintC( page, col, rem + '0' );
    return col;
}

int mzyPrintNumber3( int page, int col, int w )
{
    int i, d, rem, div;
    bool zeroVisible;
    rem = w;
    div = 100;
    zeroVisible = false;
    for( i = 0; i < 2; i = i + 1 )
    {
        d = rem / div;
        rem = rem % div;
        if( d == 0 && !zeroVisible )
          col = mzyPrintC( page, col, ' ' );
        else
        {
            zeroVisible = true;
            col = mzyPrintC( page, col, d + '0' );
        }
        div = div / 10;
    }
    col = mzyPrintC( page, col, rem + '0' );
    return col;
}

// Draws a 2x2-glyph sprite block (glyphs c,c+1 top row / c+2,c+3 bottom
// row) - spans exactly one page, 8 real columns. Direct port of upstream
// Put2C()'s own nibble-interleave math.
int mzyPut2C( int page, int col, int c )
{
    int i, upperByte, lowerByte, b0, b1;
    for( i = 0; i < 4; i = i + 1 )
    {
        upperByte = mzyCharPattern[ c * 2 + i ];
        lowerByte = mzyCharPattern[ ( c + 2 ) * 2 + i ];
        b0 = ( upperByte & 0x0f ) | ( lowerByte << 4 );
        b1 = ( upperByte >> 4 ) | ( lowerByte & 0xf0 );
        mzySetByte( page, col + i * 2, b0 );
        mzySetByte( page, col + i * 2 + 1, b1 );
    }
    return col + 8;
}

// Draws a single glyph's own top-half data with the bottom half forced
// blank (Char_Space as the "lower" nibble source) - direct port of
// upstream PutU(), used only for the held-knife icon row.
int mzyPutU( int page, int col, int c )
{
    int i, upperByte, lowerByte, b0, b1;
    for( i = 0; i < 2; i = i + 1 )
    {
        upperByte = mzyCharPattern[ c * 2 + i ];
        lowerByte = mzyCharPattern[ i ];
        b0 = ( upperByte & 0x0f ) | ( lowerByte << 4 );
        b1 = ( upperByte >> 4 ) | ( lowerByte & 0xf0 );
        mzySetByte( page, col + i * 2, b0 );
        mzySetByte( page, col + i * 2 + 1, b1 );
    }
    return col + 4;
}

void mzyPrintScore()
{
    int col;
    col = mzyPrintNumber5( 1, MZY_STATUS_COL + 2 * 4, mzyScore );
    mzyPrintC( 1, col, '0' );
}

void mzyPrintTime()
{
    mzyPrintNumber3( 5, MZY_STATUS_COL + 5 * 4, mzyStageTime );
}

void mzyPrintHeldKnives()
{
    int col, i, count;
    col = MZY_STATUS_COL;
    i = 0;
    count = mzyHeldKnifeCount;
    while( count != 0 )
    {
        col = mzyPutU( 6, col, MZY_CHAR_KN_LEFT ) + 1;
        i = i + 1;
        count = count - 1;
    }
    while( i != 4 )
    {
        col = mzyPutU( 6, col, MZY_CHAR_SPACE );
        i = i + 1;
    }
}

void mzyPrintStatus()
{
    int[5] sScore = { 'S', 'C', 'O', 'R', 'E' };
    int[5] sStage = { 'S', 'T', 'A', 'G', 'E' };
    int[4] sTime = { 'T', 'I', 'M', 'E' };
    int col, i;

    mzyPrintS( 0, MZY_STATUS_COL, sScore, 5 );
    mzyPrintS( 3, MZY_STATUS_COL, sStage, 5 );
    mzyPrintByteNumber2( 3, MZY_STATUS_COL + 6 * 4, mzyCurrentStage + 1 );
    mzyPrintS( 5, MZY_STATUS_COL, sTime, 4 );

    if( mzyRemainCount > 1 )
    {
        col = MZY_STATUS_COL;
        i = mzyRemainCount - 1;
        if( i > 2 )
        {
            col = mzyPut2C( 7, col, MZY_CHAR_REMAIN );
            col = mzyPrintC( 7, col, ' ' );
            col = mzyPrintC( 7, col, i + '0' );
        }
        else
        {
            while( i > 0 )
            {
                col = mzyPut2C( 7, col, MZY_CHAR_REMAIN );
                i = i - 1;
            }
        }
    }

    mzyPrintScore();
    mzyPrintTime();
    mzyPrintHeldKnives();
}

void mzyPrintGameOver()
{
    int[9] s = { 'G', 'A', 'M', 'E', ' ', 'O', 'V', 'E', 'R' };
    mzyPrintS( 4, 8 * 4, s, 9 );
}

void mzyPrintTimeUp()
{
    int[7] s = { 'T', 'I', 'M', 'E', ' ', 'U', 'P' };
    mzyPrintS( 4, 9 * 4, s, 7 );
}

void mzyAddScore( int pts )
{
    mzyScore = mzyScore + pts;
    mzyPrintScore();
}


// -----------------------------------------------------------------------------
//   Man.cpp - UpdateBasePosition()/InitMan() only here; MoveMan() is defined
//   further down since it needs Knife's StartKnife() first.
// -----------------------------------------------------------------------------

void mzyUpdateBasePosition()
{
    if( mzyManX < MZY_VVRAM_WIDTH / 2 )
      mzyBaseX = 0;
    else
    {
        int x, margin;
        // See header comment: a real, load-bearing byte-underflow reliance
        // on real hardware when MapWidth<VVramWidth (small stages) - the
        // explicit &0xFF masks below are required to reproduce it, not
        // just defensive.
        margin = ( mzyMapWidth - MZY_VVRAM_WIDTH ) & 0xFF;
        x = ( mzyManX - MZY_VVRAM_WIDTH / 2 ) & 0xFF;
        if( x >= margin )
          mzyBaseX = margin;
        else
          mzyBaseX = x;
    }
    if( mzyManY < MZY_VVRAM_HEIGHT / 2 )
      mzyBaseY = 0;
    else
    {
        int y, margin;
        margin = ( mzyMapHeight - MZY_VVRAM_HEIGHT ) & 0xFF;
        y = ( mzyManY - MZY_VVRAM_HEIGHT / 2 ) & 0xFF;
        if( y >= margin )
          mzyBaseY = margin;
        else
          mzyBaseY = y;
    }
}

void mzyInitMan()
{
    mzyManX = mzyToMapSize( mzyStageByte( mzyStageIndex, mzyStageCursor ) );
    mzyStageCursor = mzyStageCursor + 1;
    mzyManY = mzyToMapSize( mzyStageByte( mzyStageIndex, mzyStageCursor ) );
    mzyStageCursor = mzyStageCursor + 1;
    mzyUpdateBasePosition();
    mzyManPrevDir = MZY_DIR_RIGHT;
    mzyManPattern = mzyDirPattern[ mzyManPrevDir ];
    mzyManSeqIndex = 0;
}


// -----------------------------------------------------------------------------
//   Knife.cpp
// -----------------------------------------------------------------------------

int mzyDirectionToGoal( MzyKnife* pKnife )
{
    if( mzyAbs( mzyGoalX, pKnife->x ) >= mzyAbs( mzyGoalY, pKnife->y ) )
    {
        if( mzyGoalX < pKnife->x )
          return MZY_DIR_LEFT;
        return MZY_DIR_RIGHT;
    }
    else
    {
        if( mzyGoalY < pKnife->y )
          return MZY_DIR_UP;
        return MZY_DIR_DOWN;
    }
}

void mzyInitKnives()
{
    int i, count, xRaw, yByte;
    count = mzyStageByte( mzyStageIndex, mzyStageCursor );
    mzyStageCursor = mzyStageCursor + 1;
    for( i = 0; i < count; i = i + 1 )
    {
        xRaw = mzyStageByte( mzyStageIndex, mzyStageCursor );
        mzyStageCursor = mzyStageCursor + 1;
        yByte = mzyStageByte( mzyStageIndex, mzyStageCursor );
        mzyStageCursor = mzyStageCursor + 1;
        mzyKnives[ i ].x = mzyToMapSize( xRaw );
        mzyKnives[ i ].y = mzyToMapSize( yByte & 0x3f );
        mzyKnives[ i ].status = MZY_KNIFE_STATUS_FLOOR;
        mzyKnives[ i ].direction = mzyDirectionToGoal( &mzyKnives[ i ] );
    }
    for( i = count; i < MZY_KNIFE_COUNT; i = i + 1 )
      mzyKnives[ i ].status = MZY_KNIFE_STATUS_NONE;
    mzyHeldKnifeCount = 0;
    mzyKnifeBlink = true;
}


// -----------------------------------------------------------------------------
//   Monster.cpp
// -----------------------------------------------------------------------------

void mzyInitMonsters()
{
    int i, count, xRaw, yByte;
    count = mzyStageByte( mzyStageIndex, mzyStageCursor );
    mzyStageCursor = mzyStageCursor + 1;
    for( i = 0; i < count; i = i + 1 )
    {
        xRaw = mzyStageByte( mzyStageIndex, mzyStageCursor );
        mzyStageCursor = mzyStageCursor + 1;
        yByte = mzyStageByte( mzyStageIndex, mzyStageCursor );
        mzyStageCursor = mzyStageCursor + 1;
        mzyMonsters[ i ].x = mzyToMapSize( xRaw );
        mzyMonsters[ i ].y = mzyToMapSize( yByte & 0x3f );
        mzyMonsters[ i ].type = yByte >> 6;
        mzyMonsters[ i ].status = MZY_MONSTER_STATUS_LIVE;
        mzyMonsters[ i ].clock = 0;
    }
    for( i = count; i < MZY_MONSTER_COUNT; i = i + 1 )
      mzyMonsters[ i ].status = MZY_MONSTER_STATUS_NONE;
}


// -----------------------------------------------------------------------------
//   Map.cpp - RotBit/RotBit2/InitMap (the intricate maze bit-unpacker) +
//   CanMove1/CanMove2 (collision).
// -----------------------------------------------------------------------------

void mzyRotBit()
{
    mzyMaskBit = ( mzyMaskBit << 1 ) & 0xFF;
    if( mzyMaskBit == 0 )
    {
        mzyMap[ mzyMapPtr ] = mzyMapByte;
        mzyMapPtr = mzyMapPtr + 1;
        mzyMapByte = 0;
        mzyMaskBit = 1;
    }
}

void mzyRotBit2()
{
    mzyMaskBit = ( mzyMaskBit << 1 ) & 0xFF;
    if( mzyMaskBit == 0 )
    {
        mzyMap[ mzyMapPtr ] = mzyMapByte;
        mzyMap[ mzyMapPtr + MZY_MAP_STEP ] = mzyMapByte;
        mzyMapPtr = mzyMapPtr + 1;
        mzyMapByte = 0;
        mzyMaskBit = 1;
    }
}

void mzyInitMap()
{
    int i, count, timeRate;
    int xCount, yCount, sourceByte, bitCount;
    int mapRowStart;

    for( i = 0; i < MZY_MAP_SIZE; i = i + 1 )
      mzyMap[ i ] = 0;

    timeRate = 10;
    count = mzyCurrentStage;
    while( count >= MZY_STAGE_COUNT )
    {
        timeRate = ( timeRate - 1 ) & 0xFF;
        count = count - MZY_STAGE_COUNT;
    }
    mzyTimeRate = timeRate;
    mzyStageIndex = count;
    mzyStageCursor = 0;

    mzyStageWidth = mzyStageByte( mzyStageIndex, mzyStageCursor );
    mzyStageCursor = mzyStageCursor + 1;
    mzyMapWidth = mzyToMapSize( mzyStageWidth );
    mzyStageHeight = mzyStageByte( mzyStageIndex, mzyStageCursor );
    mzyStageCursor = mzyStageCursor + 1;
    mzyMapHeight = mzyToMapSize( mzyStageHeight );

    mapRowStart = 0;

    // Top border row - all walls.
    mzyMapPtr = mapRowStart;
    mzyMaskBit = 1;
    mzyMapByte = 0;
    xCount = mzyMapWidth;
    do
    {
        mzyMapByte = mzyMapByte | mzyMaskBit;
        mzyRotBit();
        xCount = xCount - 1;
    } while( xCount != 0 );
    mzyMap[ mzyMapPtr ] = mzyMapByte;
    mapRowStart = mapRowStart + MZY_MAP_STEP;

    sourceByte = mzyStageByte( mzyStageIndex, mzyStageCursor );
    mzyStageCursor = mzyStageCursor + 1;
    bitCount = 8;
    yCount = mzyStageHeight;
    do
    {
        // Sub-phase 1: builds this stage-row's top map-row AND the row
        // right below it simultaneously (RotBit2's own dual-write), one
        // vertical-wall-segment bit per stage-column.
        mzyMapPtr = mapRowStart;
        mzyMaskBit = 1;
        mzyMapByte = 1;
        mzyRotBit2();
        xCount = mzyStageWidth;
        do
        {
            mzyRotBit2();
            mzyRotBit2();
            if( ( sourceByte & 1 ) != 0 )
              mzyMapByte = mzyMapByte | mzyMaskBit;
            mzyRotBit2();
            sourceByte = sourceByte >> 1;
            bitCount = bitCount - 1;
            if( bitCount == 0 )
            {
                sourceByte = mzyStageByte( mzyStageIndex, mzyStageCursor );
                mzyStageCursor = mzyStageCursor + 1;
                bitCount = 8;
            }
            xCount = xCount - 1;
        } while( xCount != 0 );
        mzyMap[ mzyMapPtr ] = mzyMapByte;
        mzyMap[ mzyMapPtr + MZY_MAP_STEP ] = mzyMapByte;
        mapRowStart = mapRowStart + MZY_MAP_STEP * 2;

        // Sub-phase 2: builds one more map-row, one horizontal-wall-segment
        // pair (open = 2 clear bits, closed = 2 wall bits) per stage-column
        // plus the persistent per-column "post" bit.
        mzyMapPtr = mapRowStart;
        mzyMaskBit = 1;
        mzyMapByte = 1;
        mzyRotBit();
        xCount = mzyStageWidth;
        do
        {
            if( ( sourceByte & 1 ) != 0 )
            {
                mzyMapByte = mzyMapByte | mzyMaskBit;
                mzyRotBit();
                mzyMapByte = mzyMapByte | mzyMaskBit;
                mzyRotBit();
            }
            else
            {
                mzyRotBit();
                mzyRotBit();
            }
            mzyMapByte = mzyMapByte | mzyMaskBit;
            mzyRotBit();
            sourceByte = sourceByte >> 1;
            bitCount = bitCount - 1;
            if( bitCount == 0 )
            {
                sourceByte = mzyStageByte( mzyStageIndex, mzyStageCursor );
                mzyStageCursor = mzyStageCursor + 1;
                bitCount = 8;
            }
            xCount = xCount - 1;
        } while( xCount != 0 );
        mzyMap[ mzyMapPtr ] = mzyMapByte;

        mapRowStart = mapRowStart + MZY_MAP_STEP;
        yCount = yCount - 1;
    } while( yCount != 0 );
    if( bitCount == 8 )
      mzyStageCursor = mzyStageCursor - 1;

    mzyInitMan();

    mzyGoalX = mzyToMapSize( mzyStageByte( mzyStageIndex, mzyStageCursor ) );
    mzyStageCursor = mzyStageCursor + 1;
    mzyGoalY = mzyToMapSize( mzyStageByte( mzyStageIndex, mzyStageCursor ) );
    mzyStageCursor = mzyStageCursor + 1;
    mzyInitKnives();
    mzyInitMonsters();

    mzyStageTime = mzyStageByte( mzyStageIndex, mzyStageCursor ) +
                   ( mzyStageByte( mzyStageIndex, mzyStageCursor + 1 ) << 8 );
    mzyStageCursor = mzyStageCursor + 2;
}

// x,y are always non-negative in practice: mzyToMapSize()'s own minimum
// output is 1 (never 0), and every map's own border is solid wall, so
// nothing ever legitimately requests a negative coordinate here - matches
// upstream's own real byte (0-255) x/y parameters, no shift-safety concern.
bool mzyCanMove1( int x, int y )
{
    int mapIdx, bit;
    mapIdx = ( y << 3 ) + ( x >> 3 );
    bit = 1 << ( x & 7 );
    return ( mzyMap[ mapIdx ] & bit ) == 0;
}

bool mzyCanMove2( int x, int y )
{
    int mapIdx, bit, l, h;
    mapIdx = ( y << 3 ) + ( x >> 3 );
    bit = 3 << ( x & 7 );
    l = bit & 0xFF;
    h = ( bit >> 8 ) & 0xFF;
    return
        ( mzyMap[ mapIdx ] & l ) == 0 &&
        ( mzyMap[ mapIdx + 1 ] & h ) == 0 &&
        ( mzyMap[ mapIdx + MZY_MAP_STEP ] & l ) == 0 &&
        ( mzyMap[ mapIdx + MZY_MAP_STEP + 1 ] & h ) == 0;
}

// -----------------------------------------------------------------------------
//   VVram.cpp
// -----------------------------------------------------------------------------

void mzyVPut1( int x, int y, int c )
{
    x = x - mzyBaseX;
    y = y - mzyBaseY;
    if( x < 0 ) return;
    if( x >= MZY_VVRAM_WIDTH ) return;
    if( y < 0 ) return;
    if( y >= MZY_VVRAM_HEIGHT ) return;
    mzyVVram[ y ][ x ] = c;
}

void mzyVPut2( int x, int y, int cc )
{
    int width, height, rowY, i, j, xc;

    x = x - mzyBaseX;
    y = y - mzyBaseY;

    if( x <= -2 ) return;
    if( x >= MZY_VVRAM_WIDTH ) return;
    if( y <= -2 ) return;
    if( y >= MZY_VVRAM_HEIGHT ) return;

    width = 2;
    if( x >= MZY_VVRAM_WIDTH - 2 )
      width = MZY_VVRAM_WIDTH - x;
    else if( x < 0 )
    {
        width = width + x;
        cc = cc - x;
        x = 0;
    }

    height = 2;
    if( y >= MZY_VVRAM_HEIGHT - 2 )
      height = MZY_VVRAM_HEIGHT - y;
    else
    {
        while( y < 0 )
        {
            height = height - 1;
            y = y + 1;
            cc = cc + 2;
        }
    }

    rowY = y;
    for( j = 0; j < height; j = j + 1 )
    {
        xc = cc;
        for( i = 0; i < width; i = i + 1 )
        {
            mzyVVram[ rowY ][ x + i ] = xc;
            xc = xc + 1;
        }
        rowY = rowY + 1;
        cc = cc + 2;
    }
}

// Map.cpp's own DrawGoal() - placed here (needs mzyVPut2(), just above)
// rather than alongside CanMove1/CanMove2 further up, since this dialect
// requires strict define-before-use ordering.
void mzyDrawGoal()
{
    if( mzyGoalBlink )
    {
        mzyVPut2( mzyGoalX, mzyGoalY, MZY_CHAR_GOAL );
        mzyGoalBlink = false;
    }
    else
      mzyGoalBlink = true;
}

void mzyMapToVVram()
{
    int pMapBase, bit, y, x, pRow, c;
    pMapBase = mzyBaseY * MZY_MAP_STEP + ( mzyBaseX >> 3 );
    bit = 1 << ( mzyBaseX & 7 );
    for( y = 0; y < MZY_VVRAM_HEIGHT; y = y + 1 )
    {
        pRow = pMapBase;
        for( x = 0; x < MZY_VVRAM_WIDTH; x = x + 1 )
        {
            if( ( mzyMap[ pRow ] & bit ) != 0 )
              c = MZY_CHAR_WALL;
            else
              c = MZY_CHAR_SPACE;
            mzyVVram[ y ][ x ] = c;
            bit = ( bit << 1 ) & 0xFF;
            if( bit == 0 )
            {
                bit = 1;
                pRow = pRow + 1;
            }
        }
        pMapBase = pMapBase + MZY_MAP_STEP;
    }
}

void mzyDrawMan()
{
    mzyVPut2( mzyManX, mzyManY, mzyManPattern );
}


// -----------------------------------------------------------------------------
//   Monster.cpp - draw/move/collision (needs mzyCanMove2, defined above).
// -----------------------------------------------------------------------------

void mzyDrawMonsters()
{
    int i, c;
    for( i = 0; i < MZY_MONSTER_COUNT; i = i + 1 )
    {
        if( mzyMonsters[ i ].status == MZY_MONSTER_STATUS_LIVE )
        {
            if( mzyMonsters[ i ].type == MZY_MONSTER_TYPE_CHASE )
              c = MZY_CHAR_CHASER;
            else
              c = MZY_CHAR_DISTURBER;
            mzyVPut2( mzyMonsters[ i ].x, mzyMonsters[ i ].y, c );
        }
        else if( mzyMonsters[ i ].status == MZY_MONSTER_STATUS_END )
          mzyVPut2( mzyMonsters[ i ].x, mzyMonsters[ i ].y, MZY_CHAR_BANG );
    }
}

void mzyTestHitMan( int idx )
{
    if( ( ( mzyManX - mzyMonsters[ idx ].x + 1 ) & 0xFF ) < 3 &&
        ( ( mzyManY - mzyMonsters[ idx ].y + 1 ) & 0xFF ) < 3 )
      mzyManLost = true;
}

bool mzyMonsterCanMove( int idx, int x, int y )
{
    int j;
    if( !mzyCanMove2( x, y ) ) return false;
    for( j = 0; j < MZY_MONSTER_COUNT; j = j + 1 )
    {
        if( mzyMonsters[ j ].status == MZY_MONSTER_STATUS_LIVE && j != idx )
        {
            if( ( ( x - mzyMonsters[ j ].x + 1 ) & 0xFF ) < 3 &&
                ( ( y - mzyMonsters[ j ].y + 1 ) & 0xFF ) < 3 )
              return false;
        }
    }
    return true;
}

void mzyMonsterCommitMove( int idx, int x, int y )
{
    mzyMonsters[ idx ].x = x;
    mzyMonsters[ idx ].y = y;
    mzyTestHitMan( idx );
}

bool mzyMonsterHitKnife( int kx, int ky )
{
    int i;
    for( i = 0; i < MZY_MONSTER_COUNT; i = i + 1 )
    {
        if( mzyMonsters[ i ].status == MZY_MONSTER_STATUS_LIVE )
        {
            if( kx >= mzyMonsters[ i ].x && kx < mzyMonsters[ i ].x + 2 &&
                ky >= mzyMonsters[ i ].y && ky < mzyMonsters[ i ].y + 2 )
            {
                mzyMonsters[ i ].status = MZY_MONSTER_STATUS_END;
                mzyMonsters[ i ].clock = 0;
                if( mzyMonsters[ i ].type == MZY_MONSTER_TYPE_CHASE )
                  mzyAddScore( 30 );
                else
                  mzyAddScore( 10 );
                return true;
            }
        }
    }
    return false;
}

void mzyMoveMonsters()
{
    int i, x, y;
    for( i = 0; i < MZY_MONSTER_COUNT; i = i + 1 )
    {
        mzyMonsters[ i ].clock = ( mzyMonsters[ i ].clock + 1 ) & 0xFF;
        if( mzyMonsters[ i ].status == MZY_MONSTER_STATUS_LIVE )
        {
            mzyTestHitMan( i );
            x = mzyMonsters[ i ].x;
            y = mzyMonsters[ i ].y;
            if( mzyMonsters[ i ].type == MZY_MONSTER_TYPE_HORIZONTAL )
            {
                if( x > mzyManX ) x = x - 1;
                else if( x < mzyManX ) x = x + 1;
                if( mzyMonsterCanMove( i, x, y ) )
                  mzyMonsterCommitMove( i, x, y );
            }
            else if( mzyMonsters[ i ].type == MZY_MONSTER_TYPE_VERTICAL )
            {
                if( mzyManY < y ) y = y - 1;
                else if( mzyManY > y ) y = y + 1;
                if( mzyMonsterCanMove( i, x, y ) )
                  mzyMonsterCommitMove( i, x, y );
            }
            else
            {
                // Type_Chase (2) and any other value - matches upstream's
                // own `default:` case exactly.
                if( ( mzyMonsters[ i ].clock & 1 ) != 0 )
                {
                    bool committed;
                    committed = false;
                    if( x != mzyManX )
                    {
                        if( x > mzyManX ) x = x - 1; else x = x + 1;
                        if( mzyMonsterCanMove( i, x, y ) )
                        {
                            mzyMonsterCommitMove( i, x, y );
                            committed = true;
                        }
                        else
                          x = mzyMonsters[ i ].x;
                    }
                    if( !committed && mzyManY != y )
                    {
                        if( mzyManY < y ) y = y - 1; else y = y + 1;
                        if( mzyMonsterCanMove( i, x, y ) )
                          mzyMonsterCommitMove( i, x, y );
                    }
                }
            }
        }
        else if( mzyMonsters[ i ].status == MZY_MONSTER_STATUS_END )
        {
            if( mzyMonsters[ i ].clock == 4 )
              mzyMonsters[ i ].status = MZY_MONSTER_STATUS_NONE;
        }
    }
}


// -----------------------------------------------------------------------------
//   Sound sequencer - same frame-stepped shape as gameCracky.c's own (see
//   that file's own header comment for the full rationale); Tempo=220 here.
// -----------------------------------------------------------------------------

int mzyMelodyLength( int id )
{
    if( id == MZY_MELODY_LOOSE ) return 3;
    if( id == MZY_MELODY_HIT ) return 17;
    if( id == MZY_MELODY_BEEP ) return 3;
    if( id == MZY_MELODY_GET ) return 3;
    if( id == MZY_MELODY_START ) return 11;
    if( id == MZY_MELODY_CLEAR ) return 21;
    if( id == MZY_MELODY_GAMEOVER ) return 23;
    if( id == MZY_MELODY_BGM1 ) return 89;
    if( id == MZY_MELODY_BGM2 ) return 41;
    return 0;
}

int mzyMelodyValue( int id, int idx )
{
    if( id == MZY_MELODY_LOOSE ) return mzyMelodyLoose[ idx ];
    if( id == MZY_MELODY_HIT ) return mzyMelodyHit[ idx ];
    if( id == MZY_MELODY_BEEP ) return mzyMelodyBeep[ idx ];
    if( id == MZY_MELODY_GET ) return mzyMelodyGet[ idx ];
    if( id == MZY_MELODY_START ) return mzyMelodyStart[ idx ];
    if( id == MZY_MELODY_CLEAR ) return mzyMelodyClear[ idx ];
    if( id == MZY_MELODY_GAMEOVER ) return mzyMelodyGameOver[ idx ];
    if( id == MZY_MELODY_BGM1 ) return mzyMelodyBgm1[ idx ];
    if( id == MZY_MELODY_BGM2 ) return mzyMelodyBgm2[ idx ];
    return 0;
}

// SoundHandler()'s own real tempo: a channel advances once every
// (600/2)/MZY_TEMPO = 300/220 real 60Hz ticks - see header comment.
int mzyNoteFrames( int length )
{
    return (int)( length * ( 300.0 / 220.0 ) + 0.5 );
}

void mzyStartSeq( int channel, int melodyId )
{
    mzySeqMelody[ channel ] = melodyId;
    mzySeqPos[ channel ] = 0;
    mzySeqWait[ channel ] = 0;
    mzySeqActive[ channel ] = 1;
}

void mzyStopSeq( int channel )
{
    mzySeqActive[ channel ] = 0;
    mzySeqMelody[ channel ] = MZY_MELODY_NONE;
}

bool mzySeqPlaying( int channel )
{
    return mzySeqActive[ channel ] != 0;
}

void mzyAdvanceOneSeq( int channel )
{
    int length, note;

    if( mzySeqActive[ channel ] == 0 ) return;

    if( mzySeqWait[ channel ] > 0 )
    {
        mzySeqWait[ channel ] = mzySeqWait[ channel ] - 1;
        return;
    }

    length = mzyMelodyValue( mzySeqMelody[ channel ], mzySeqPos[ channel ] );
    if( length == 0 )
    {
        mzyStopSeq( channel );
        return;
    }
    if( length == 255 )
    {
        mzySeqPos[ channel ] = 0;
        length = mzyMelodyValue( mzySeqMelody[ channel ], 0 );
    }
    note = mzyMelodyValue( mzySeqMelody[ channel ], mzySeqPos[ channel ] + 1 );
    mzySeqPos[ channel ] = mzySeqPos[ channel ] + 2;
    mzySeqWait[ channel ] = mzyNoteFrames( length );
    if( note != 0 )
      md_playTone( (float)mzyFrequencies[ note - 1 ], (float)mzySeqWait[ channel ] / 60.0 );
}

void mzyAdvanceSound()
{
    mzyAdvanceOneSeq( 0 );
    mzyAdvanceOneSeq( 1 );
    mzyAdvanceOneSeq( 2 );
}

void mzyStartBgm()
{
    mzyStartSeq( 1, MZY_MELODY_BGM1 );
    mzyStartSeq( 2, MZY_MELODY_BGM2 );
}

void mzyStopBgm()
{
    mzyStopSeq( 1 );
    mzyStopSeq( 2 );
    md_stopTone();
}


// -----------------------------------------------------------------------------
//   Knife.cpp - mzyDrawKnives() only (needs just mzyVPut1(), already
//   defined above) - placed here, ahead of mzyMoveKnives()/mzyStartKnife()
//   further down, specifically because mzyDrawAll() (just below) needs it
//   and this dialect requires strict define-before-use ordering.
// -----------------------------------------------------------------------------

void mzyDrawKnives()
{
    int i;
    if( mzyKnifeBlink )
      mzyKnifeBlink = false;
    else
      mzyKnifeBlink = true;
    for( i = 0; i < MZY_KNIFE_COUNT; i = i + 1 )
    {
        if( mzyKnives[ i ].status == MZY_KNIFE_STATUS_FLOOR )
        {
            if( !mzyKnifeBlink )
              mzyVPut1( mzyKnives[ i ].x, mzyKnives[ i ].y, MZY_CHAR_KN_LEFT + mzyKnives[ i ].direction );
        }
        else if( mzyKnives[ i ].status == MZY_KNIFE_STATUS_FLYING )
          mzyVPut1( mzyKnives[ i ].x, mzyKnives[ i ].y, MZY_CHAR_KN_LEFT + mzyKnives[ i ].direction );
    }
}


// -----------------------------------------------------------------------------
//   Rendering
// -----------------------------------------------------------------------------

// Composes the 24x16 VVram glyph grid into mzyFrame's own columns 0-95
// (24 VVram cells x 4 real sub-columns each, 8 pages from 16 VVram rows
// paired 2-at-a-time) - direct port of upstream VVramToVram()'s own
// SendUL() nibble-interleaving, minus its dirty-tracking Backup[] array
// (this port always recomputes fresh, matching this project's own standing
// "always redraw the full frame" precedent).
void mzyComposeMapArea()
{
    int page, mapX, sub, upper, lower, upperByte, lowerByte, col, value;
    for( page = 0; page < 8; page = page + 1 )
    {
        for( mapX = 0; mapX < MZY_VVRAM_WIDTH; mapX = mapX + 1 )
        {
            upper = mzyVVram[ page * 2 ][ mapX ];
            lower = mzyVVram[ page * 2 + 1 ][ mapX ];
            for( sub = 0; sub < 4; sub = sub + 1 )
            {
                if( sub == 0 )
                {
                    upperByte = mzyCharPattern[ upper * 2 + 0 ];
                    lowerByte = mzyCharPattern[ lower * 2 + 0 ];
                    value = ( upperByte & 0x0f ) | ( lowerByte << 4 );
                }
                else if( sub == 1 )
                {
                    upperByte = mzyCharPattern[ upper * 2 + 0 ];
                    lowerByte = mzyCharPattern[ lower * 2 + 0 ];
                    value = ( upperByte >> 4 ) | ( lowerByte & 0xf0 );
                }
                else if( sub == 2 )
                {
                    upperByte = mzyCharPattern[ upper * 2 + 1 ];
                    lowerByte = mzyCharPattern[ lower * 2 + 1 ];
                    value = ( upperByte & 0x0f ) | ( lowerByte << 4 );
                }
                else
                {
                    upperByte = mzyCharPattern[ upper * 2 + 1 ];
                    lowerByte = mzyCharPattern[ lower * 2 + 1 ];
                    value = ( upperByte >> 4 ) | ( lowerByte & 0xf0 );
                }
                col = mapX * 4 + sub;
                mzySetByte( page, col, value );
            }
        }
    }
}

void mzyDrawAll()
{
    mzyMapToVVram();
    mzyDrawGoal();
    mzyDrawMan();
    mzyDrawMonsters();
    mzyDrawKnives();
    mzyComposeMapArea();
}

void mzyClearScreen()
{
    int page, col, y, x;
    for( page = 0; page < 8; page = page + 1 )
      for( col = 0; col < 128; col = col + 1 )
        mzyFrame[ page ][ col ] = 0;
    for( y = 0; y < MZY_VVRAM_HEIGHT; y = y + 1 )
      for( x = 0; x < MZY_VVRAM_WIDTH; x = x + 1 )
        mzyVVram[ y ][ x ] = 0;
}

void mzyRender()
{
    int page, col;
    md_beginFrame();
    for( page = 0; page < 8; page = page + 1 )
      for( col = 0; col < 128; col = col + 1 )
        md_drawColumn( col, page, mzyFrame[ page ][ col ] );
}


// -----------------------------------------------------------------------------
//   Knife.cpp - move/start (mzyDrawKnives() itself is defined earlier, just
//   before the Rendering section, since mzyDrawAll() needs it and this
//   dialect requires strict define-before-use ordering; mzyMoveKnives()/
//   mzyStartKnife() need mzyMonsterHitKnife()/mzyDrawAll()/the sound
//   sequencer, all already defined above by this point).
// -----------------------------------------------------------------------------

void mzyMoveKnives()
{
    int i, x, y, dir;
    for( i = 0; i < MZY_KNIFE_COUNT; i = i + 1 )
    {
        x = mzyKnives[ i ].x;
        y = mzyKnives[ i ].y;
        if( mzyKnives[ i ].status == MZY_KNIFE_STATUS_FLOOR )
        {
            if( x >= mzyManX && x < mzyManX + 2 && y >= mzyManY && y < mzyManY + 2 )
            {
                mzyKnives[ i ].status = MZY_KNIFE_STATUS_HELD;
                mzyHeldKnifeCount = mzyHeldKnifeCount + 1;
                mzyPrintHeldKnives();
                mzyStartSeq( 0, MZY_MELODY_GET );
            }
        }
        else if( mzyKnives[ i ].status == MZY_KNIFE_STATUS_FLYING )
        {
            dir = mzyKnives[ i ].direction;
            x = x + mzyDirDx[ dir ];
            y = y + mzyDirDy[ dir ];
            if( !mzyCanMove1( x, y ) )
            {
                mzyKnives[ i ].status = MZY_KNIFE_STATUS_FLOOR;
                mzyKnives[ i ].direction = mzyDirectionToGoal( &mzyKnives[ i ] );
            }
            else
            {
                mzyKnives[ i ].x = x;
                mzyKnives[ i ].y = y;
                if( mzyMonsterHitKnife( x, y ) )
                {
                    mzyDrawAll();
                    mzyStartSeq( 0, MZY_MELODY_HIT );
                    mzyKnives[ i ].status = MZY_KNIFE_STATUS_FLOOR;
                    mzyKnives[ i ].direction = mzyDirectionToGoal( &mzyKnives[ i ] );
                }
            }
        }
    }
}

void mzyStartKnife( int x, int y, int direction )
{
    int i;
    for( i = 0; i < MZY_KNIFE_COUNT; i = i + 1 )
    {
        if( mzyKnives[ i ].status == MZY_KNIFE_STATUS_HELD )
        {
            mzyKnives[ i ].x = x;
            mzyKnives[ i ].y = y;
            mzyKnives[ i ].status = MZY_KNIFE_STATUS_FLYING;
            mzyKnives[ i ].direction = direction;
            mzyHeldKnifeCount = mzyHeldKnifeCount - 1;
            mzyPrintHeldKnives();
            mzyStartSeq( 0, MZY_MELODY_GET );
            return;
        }
    }
}


// -----------------------------------------------------------------------------
//   Man.cpp - MoveMan() (needs mzyCanMove1/2, mzyUpdateBasePosition,
//   mzyStartKnife, all defined above).
// -----------------------------------------------------------------------------

void mzyMoveMan()
{
    int key, i, x, y, kx, ky, moved;

    if( mzyManLost ) return;

    key = 0;
    if( isLeftPressed() ) key = key | MZY_KEYS_LEFT;
    if( isRightPressed() ) key = key | MZY_KEYS_RIGHT;
    if( isUpPressed() ) key = key | MZY_KEYS_UP;
    if( isDownPressed() ) key = key | MZY_KEYS_DOWN;
    if( isFirePressed() ) key = key | MZY_KEYS_BUTTON0;

    x = mzyManX;
    y = mzyManY;
    moved = 0;
    for( i = 0; i < 4; i = i + 1 )
    {
        if( ( key & mzyDirKeyMask[ i ] ) != 0 )
        {
            x = mzyManX + mzyDirDx[ i ];
            y = mzyManY + mzyDirDy[ i ];
            if( mzyCanMove2( x, y ) )
            {
                mzyManPrevDir = i;
                moved = 1;
                break;
            }
        }
    }
    if( moved == 0 && ( key & MZY_KEYS_DIR ) != 0 )
    {
        x = mzyManX + mzyDirDx[ mzyManPrevDir ];
        y = mzyManY + mzyDirDy[ mzyManPrevDir ];
        if( mzyCanMove2( x, y ) )
          moved = 1;
    }

    if( moved != 0 )
    {
        mzyManX = x;
        mzyManY = y;
        mzyUpdateBasePosition();
        if( x == mzyGoalX && y == mzyGoalY )
          mzyStageClear = true;
        mzyManSeqIndex = ( mzyManSeqIndex + 1 ) & 3;
        if( mzyManSeqIndex == 3 )
          mzyManSeqIndex = 1;
        mzyManPattern = mzyDirPattern[ mzyManPrevDir ] + ( ( 1 + mzyManSeqIndex ) << 2 );
    }
    else
      mzyManPattern = mzyDirPattern[ mzyManPrevDir ];

    if( ( key & MZY_KEYS_BUTTON0 ) != 0 && !mzyManButtonOn )
    {
        kx = mzyManX + mzyDirDx[ mzyManPrevDir ];
        ky = mzyManY + mzyDirDy[ mzyManPrevDir ];
        if( mzyCanMove1( kx, ky ) )
        {
            mzyStartKnife( kx, ky, mzyManPrevDir );
            mzyManButtonOn = true;
        }
    }
    else
      mzyManButtonOn = false;
}


// -----------------------------------------------------------------------------
//   State machine
// -----------------------------------------------------------------------------

void mzyBeginTry()
{
    mzyClearScreen();
    mzyManLost = false;
    mzyClock = 0;
    mzyBaseX = 0;
    mzyBaseY = 0;
    mzyStageClear = false;
    mzyInitMap();
    mzyPrintStatus();
    mzyDrawAll();
    mzyStartSeq( 1, MZY_MELODY_START );
    mzyState = MZY_STATE_START_JINGLE;
}

void mzyUpdateStartJingle()
{
    if( !mzySeqPlaying( 1 ) )
    {
        mzyStartBgm();
        mzyTickCounter = 0;
        mzyState = MZY_STATE_PLAYING;
    }
    mzyRender();
}

void mzyBeginTitle()
{
    int[4] sMini = { 'M', 'I', 'N', 'I' };
    int[12] sCredit = { 'I', 'N', 'U', 'F', 'U', 'T', 'O', ' ', '2', '0', '2', '6' };
    int[5] sStart = { 'S', 'T', 'A', 'R', 'T' };
    int[8] sContinue = { 'C', 'O', 'N', 'T', 'I', 'N', 'U', 'E' };
    int letter, row, col, srcIdx;

    mzyClearScreen();
    // See header comment: a targeted, Cracky-precedented reset of only the
    // per-life leftover fields, leaving Score/CurrentStage/RemainCount
    // intact (load-bearing for the real CONTINUE feature).
    mzyStageTime = 0;
    mzyHeldKnifeCount = 0;
    mzyPrintStatus();

    // Real upstream "MAZY" bitmap logo (see header comment's own bug-fix
    // note) - 4 letters, each a 4x4 VVram-cell block of mzyTitleBytes[]
    // values, placed side by side starting at VVram row2,col4 (direct
    // structural mirror of upstream's own nested repeat(4){repeat(4){
    // repeat(4){...}}} pointer-walk). Composed into mzyFrame via
    // mzyComposeMapArea() BEFORE "MINI" is drawn, matching upstream's own
    // exact VVramToVram()-then-PrintS() order - otherwise the later full-
    // map-area composite would erase text drawn into mzyFrame first.
    srcIdx = 0;
    for( letter = 0; letter < 4; letter = letter + 1 )
    {
        for( row = 0; row < 4; row = row + 1 )
        {
            for( col = 0; col < 4; col = col + 1 )
            {
                mzyVVram[ 2 + row ][ 4 + letter * 4 + col ] = mzyTitleBytes[ srcIdx ];
                srcIdx = srcIdx + 1;
            }
        }
    }
    mzyComposeMapArea();
    mzyPrintS( 3, 16 * 4, sMini, 4 );
    mzyPrintS( 7, 12 * 4, sCredit, 12 );

    mzyPrintS( 5, 9 * 4, sStart, 5 );
    mzyPrintS( 6, 9 * 4, sContinue, 8 );

    mzySelection = 0;
    mzySelectionChanged = true;
    mzyPrevLeft = false; mzyPrevRight = false; mzyPrevUp = false; mzyPrevDown = false; mzyPrevFire = false;
    mzyState = MZY_STATE_TITLE;
}

void mzyUpdateTitle()
{
    bool left, right, up, down, fire, justDir, justFire;

    left = isLeftPressed(); right = isRightPressed();
    up = isUpPressed(); down = isDownPressed();
    fire = isFirePressed();

    justDir = ( ( left && !mzyPrevLeft ) || ( right && !mzyPrevRight ) ||
                ( up && !mzyPrevUp ) || ( down && !mzyPrevDown ) );
    justFire = ( fire && !mzyPrevFire );
    mzyPrevLeft = left; mzyPrevRight = right; mzyPrevUp = up; mzyPrevDown = down; mzyPrevFire = fire;

    if( mzySelectionChanged )
    {
        mzySelectionChanged = false;
        if( mzySelection == 0 )
          mzyPrintC( 5, 8 * 4, '>' );
        else
          mzyPrintC( 5, 8 * 4, ' ' );
        if( mzySelection == 1 )
          mzyPrintC( 6, 8 * 4, '>' );
        else
          mzyPrintC( 6, 8 * 4, ' ' );
    }

    if( justFire )
    {
        mzyPendingContinue = ( mzySelection == 1 );
        mzyScore = 0;
        if( !mzyPendingContinue )
          mzyCurrentStage = 0;
        mzyRemainCount = 3;
        mzyBeginTry();
        mzyRender();
        return;
    }
    if( justDir )
    {
        mzySelection = mzySelection ^ 1;
        mzySelectionChanged = true;
    }
    mzyRender();
}

void mzyUpdatePlaying()
{
    mzyTickCounter = mzyTickCounter + 1;
    if( mzyTickCounter < MZY_TICK_DIVISOR )
    {
        mzyRender();
        return;
    }
    mzyTickCounter = 0;

    mzyMoveKnives();
    mzyMoveMan();
    mzyMoveMonsters();
    mzyMoveKnives();

    mzyDrawAll();

    mzyClock = mzyClock + 1;
    if( mzyClock >= mzyTimeRate )
    {
        mzyClock = 0;
        mzyStageTime = mzyStageTime - 1;
        mzyPrintTime();
        if( mzyStageTime == 0 )
        {
            mzyStopBgm();
            mzyPrintTimeUp();
            mzyTimeUpStep = 0;
            mzyWaitFrames = 0;
            mzyState = MZY_STATE_TIMEUP_WAIT;
            mzyRender();
            return;
        }
    }

    if( mzyManLost )
    {
        mzyStopBgm();
        mzyAnimStep = 0;
        mzyWaitFrames = 0;
        mzyState = MZY_STATE_LOSE_ANIM;
        mzyRender();
        return;
    }

    if( mzyStageClear )
    {
        mzyStopBgm();
        mzyStartSeq( 1, MZY_MELODY_CLEAR );
        mzyState = MZY_STATE_CLEAR_JINGLE;
        mzyRender();
        return;
    }

    mzyRender();
}

void mzyUpdateTimeUpWait()
{
    if( mzyWaitFrames > 0 )
    {
        mzyWaitFrames = mzyWaitFrames - 1;
        mzyRender();
        return;
    }
    if( mzyTimeUpStep < 15 )
    {
        mzyStartSeq( 0, MZY_MELODY_LOOSE );
        mzyWaitFrames = mzyNoteFrames( 1 );
        mzyTimeUpStep = mzyTimeUpStep + 1;
        mzyRender();
        return;
    }

    mzyRemainCount = mzyRemainCount - 1;
    if( mzyRemainCount != 0 )
    {
        mzyBeginTry();
        mzyRender();
        return;
    }
    mzyPrintGameOver();
    mzyStartSeq( 1, MZY_MELODY_GAMEOVER );
    mzyState = MZY_STATE_GAMEOVER_JINGLE;
    mzyRender();
}

void mzyUpdateLoseAnim()
{
    int[4] patterns = { MZY_CHAR_MAN_LEFT_STOP, MZY_CHAR_MAN_DOWN_STOP, MZY_CHAR_MAN_DIE2, MZY_CHAR_MAN_DIE3 };

    if( mzyWaitFrames > 0 )
    {
        mzyWaitFrames = mzyWaitFrames - 1;
        mzyRender();
        return;
    }

    mzyStartSeq( 0, MZY_MELODY_LOOSE );
    mzyMapToVVram();
    mzyDrawGoal();
    mzyVPut2( mzyManX, mzyManY, patterns[ mzyAnimStep & 3 ] );
    mzyDrawMonsters();
    mzyDrawKnives();
    mzyComposeMapArea();

    mzyAnimStep = mzyAnimStep + 1;
    mzyWaitFrames = MZY_TICK_DIVISOR;

    if( mzyAnimStep >= 10 )
    {
        mzyRemainCount = mzyRemainCount - 1;
        if( mzyRemainCount != 0 )
        {
            mzyBeginTry();
            mzyRender();
            return;
        }
        mzyPrintGameOver();
        mzyStartSeq( 1, MZY_MELODY_GAMEOVER );
        mzyState = MZY_STATE_GAMEOVER_JINGLE;
        mzyRender();
        return;
    }
    mzyRender();
}

void mzyUpdateGameOverJingle()
{
    if( !mzySeqPlaying( 1 ) )
      mzyBeginTitle();
    else
      mzyRender();
}

void mzyUpdateClearJingle()
{
    if( !mzySeqPlaying( 1 ) )
    {
        mzyWaitFrames = 0;
        mzyState = MZY_STATE_BONUS_TALLY;
    }
    mzyRender();
}

void mzyUpdateBonusTally()
{
    if( mzyWaitFrames > 0 )
    {
        mzyWaitFrames = mzyWaitFrames - 1;
        mzyRender();
        return;
    }

    if( mzyStageTime != 0 )
    {
        mzyAddScore( 1 );
        mzyStageTime = mzyStageTime - 1;
        mzyPrintTime();
        mzyStartSeq( 0, MZY_MELODY_BEEP );
        mzyWaitFrames = mzyNoteFrames( 1 ) + 2;
        mzyRender();
        return;
    }

    mzyCurrentStage = mzyCurrentStage + 1;
    mzyBeginTry();
    mzyRender();
}


// -----------------------------------------------------------------------------
//   Entry points
// -----------------------------------------------------------------------------

void gameMazy_init()
{
    int i;

    mzyScore = 0;
    mzyCurrentStage = 0;
    mzyRemainCount = 3;
    mzyStageTime = 0;
    mzyHeldKnifeCount = 0;
    mzyGoalBlink = false;
    mzyKnifeBlink = true;

    for( i = 0; i < 3; i = i + 1 )
    {
        mzySeqActive[ i ] = 0;
        mzySeqMelody[ i ] = MZY_MELODY_NONE;
    }
    mzyTickCounter = 0;

    mzyBeginTitle();
}

void gameMazy_update()
{
    mzyAdvanceSound();

    if( mzyState == MZY_STATE_TITLE )
      mzyUpdateTitle();
    else if( mzyState == MZY_STATE_START_JINGLE )
      mzyUpdateStartJingle();
    else if( mzyState == MZY_STATE_PLAYING )
      mzyUpdatePlaying();
    else if( mzyState == MZY_STATE_TIMEUP_WAIT )
      mzyUpdateTimeUpWait();
    else if( mzyState == MZY_STATE_LOSE_ANIM )
      mzyUpdateLoseAnim();
    else if( mzyState == MZY_STATE_GAMEOVER_JINGLE )
      mzyUpdateGameOverJingle();
    else if( mzyState == MZY_STATE_CLEAR_JINGLE )
      mzyUpdateClearJingle();
    else if( mzyState == MZY_STATE_BONUS_TALLY )
      mzyUpdateBonusTally();
}
