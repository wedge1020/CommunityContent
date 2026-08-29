# Preserved vs. fixed upstream bugs

This project's default policy is to **preserve real upstream Gamebuino
Classic bugs**, even ones that look like mistakes, rather than silently
"clean them up" - a faithful port should behave like the real cartridge it
was built from. Bugs are only fixed when they'd crash, hang, or corrupt
state specifically on this platform (Vircon32 traps on things real AVR
hardware silently tolerates, like integer divide-by-zero), or - as of this
sweep - when a real bug was judged a genuine, negative hit to player
experience and fixed on direct request, overriding the default.

This file lists **every currently-known deliberately-preserved gameplay
bug** across all 110 games, and **every bug that was found and fixed
instead**, so both are documented in one place rather than scattered
across each game's own header comment. See each file's own header comment
in `src/games/` for the full technical detail behind any single entry.

Bugs that are purely cosmetic with no real gameplay consequence, dead code
with zero observable effect, or dropped real-hardware/multiplayer-only
features are not listed here at all - only bugs that actually change what
a player experiences.

## A special case: not a bug list at all

- **Stijn's Snake** (`gameStijnSnake.c`): real upstream here isn't a game
  with bugs to preserve or fix in the first place - both real source files
  (`SnakeStart.ino`/`Snake2.ino`) are genuinely incomplete tutorial stubs.
  Nothing anywhere in either file ever reads a direction button, moves a
  coordinate, spawns food, grows the snake, or ends a round - run as
  upstream actually shipped it, "Snake" is a single static pixel frozen at
  screen center, forever. This port completes a real, playable game
  instead, guided directly by what upstream left as evidence of its own
  intent (declared-but-never-used `vx`/`vy`/`score` globals, and a header
  comment describing real head-growth list semantics) rather than invented
  from nothing - documented in full in the file's own header comment under
  "INVENTED GAMEPLAY DETAILS." Not listed as a fixed or preserved bug
  above because there was no real upstream gameplay logic to diverge from
  either way.

## Fixed bugs

These were found to negatively affect gameplay and fixed, overriding this
project's normal "preserve real bugs" default - see each entry for what
changed. Every fix is documented in-place in its own file's header comment
too, marked "FIXED, NOT PRESERVED".

- **Agaruino** (`gameAgaruino.c`): The "Taille : " (size) readout is a real
  upstream `float`, and real hardware's own default float printing shows 2
  decimal places (e.g. "Taille : 3.75") - this shim had no float-printing
  primitive at all, so the port cast the value to `int` before printing,
  silently truncating away the entire fractional part every tick. Found via
  a direct side-by-side comparison against a real emulator screenshot.
  Fixed by adding a new shared `gbPrintFloat()` primitive (a direct port of
  real Arduino's own `Print::printFloat()` algorithm) and calling it here;
  the on-screen label text was also corrected from this port's own
  "TAILLE " to upstream's own real, literal "Taille : ".
- **AsteroidRipper** (`gameAsteroidRipper.c`): Pausing used to freeze the
  entire screen with no indication the game was paused rather than frozen/
  crashed. A "PAUSED" label is now drawn while paused.
- **Bang! Bang!** (`gameBangBang.c`): Pausing mid-game used to show a
  leftover "TAQUIN" title screen (a copy-paste bug from the same author's
  other game) instead of "BANG! BANG!". Fixed to show the real title.
- **BlocksBuino** (`gameBlocksBuino.c`): The playfield's own real double-
  line side border never drew at all - taken completely literally, real
  upstream's own `drawRect(field_x, field_y, field_w, field_h)` calls use
  an off-screen `field_y` (49, one row below the real 48px-tall screen)
  and a negative `field_h` (-49), which make every edge either off-screen
  or a no-op against the real `Display` class's own literal semantics. An
  earlier pass through this file took that math at face value and shipped
  it as a preserved no-op. That conclusion was wrong: a real screenshot
  bundled in upstream's own repo, and a live user report against a real
  emulator, both clearly show a full-height double-line border down each
  side of the field with no horizontal cap at top or bottom. Fixed by
  reading `field_y`/`field_h` as "anchored at the bottom edge, height
  extending upward" for the 4 real vertical edges only (matching the
  observed shape exactly), while leaving the 2 horizontal edges at their
  original, always-off-screen position.
- **Blob Attack** (`gameBlobAttack.c`):
  - The pause screen's "B to play" label had no cursor position set and
    landed wherever the last HUD text left it. Now centered under the
    pause icon.
  - The high score was saved as a single truncated EEPROM byte, wrapping
    any score over 255 on reload. Now saved as a full word with a
    fresh-cell sentinel check.
- **Blockdude** (`gameBlockdude.c`):
  - A fresh EEPROM cell decoded as 65535, which the "completed" check
    treated as `> 0` - every level falsely showed as completed on a fresh
    save. Fixed with a fresh-cell sentinel check.
  - Completing a level while still carrying a lifted block left a phantom
    floating block drawn at the start of the next level. `doLift`/
    `liftBlock` are now reset on the auto-advance path.
- **Castle Defence** (`gameCastleDefence.c`):
  - No HP-bar case for exactly 1 HP, and no fallback for negative HP,
    while a boss hit subtracts 2 at once - could skip the exact 0-HP
    Game Over check, leaving HP permanently negative with the run never
    ending. Fixed with a 1-HP case and a `<= 0` game-over check.
  - The Game Over slide-in animation only converged if the camera offset
    was already `<= 0` at death - a player who'd been climbing could
    leave it stuck permanently positive, meaning the Game Over screen (and
    its only exit) might never appear. Fixed to converge from either side.
  - An operator-precedence bug made a monster-toughness "floor" check
    always true, so toughness variance shrank unboundedly instead of
    leveling off. Fixed to the clearly-intended comparison.
  - The rifle's life-recovery shop text wasn't nested in its ownership
    check, so a player without the rifle saw both "Impossible!" and the
    life-recovery option stacked together. Fixed by nesting it correctly.
  - "Ready..?"/"GO!!" (pre-round), "LEVEL UP!"/"DANGER" (in-round banners),
    the shop's price-tag labels, and the whole Game Over screen ("GAME
    OVER", "please button", "Your Socre :", "NEW HIGHT SCORE") draw
    directly over busy bitmap art (the castle wall/shop staircase) and need
    a real opaque WHITE text background to stay legible instead of letting
    the art bleed through the letters - see `gbUpdate()`'s own doc comment
    in `gamebuinoShim.c` for the shared primitive that provides this.
- **Catcher** (`gameCatcher.c`): A missed single-tick button-release pulse
  on Button-C restart could leave the pad-input state stuck on "C",
  risking a repeating auto-restart loop. Fixed by explicitly clearing the
  pad-hit state on every reset.
- **CrazyTown** (`gameCrazyTown.c`):
  - A comparison-instead-of-`abs()` typo meant the "distance driven"
    accumulator never actually added anything - the distance readout was
    always "000000" and the drive-efficiency scoring bonus always resolved
    to its floor value. Fixed to use `fabs()`, the clearly-intended
    formula.
  - A fresh EEPROM cell composed to 65535 instead of 0, showing a
    nonsensical highscore value on a fresh memory card. Fixed with a
    fresh-cell sentinel check.
- **Descent Into Hell** (`gameDescent.c`): A monster-spawn retry loop's
  condition was an assignment instead of a comparison, so it always ran
  once - overlapping monster spawns were never actually retried. Fixed to
  a genuine comparison.
- **Digger** (`gameDigger.c`): Game-over reset never reloaded the level
  layout - a new life resumed on the previous run's half-dug board. Fixed
  to reload a fresh level.
- **Firemen** (`gameFiremen.c`): Quitting with Button C double-read the
  same press within one tick, skipping the Game Over screen (final score/
  highscore) entirely. Fixed by making the transition a genuine deferred
  state change instead of a same-tick synchronous call.
- **Gruniozerca** (`gameGruniozerca.c`): A fresh EEPROM cell composed to
  255, showing an already-maxed-out top score on a save that had never
  actually been played. Fixed with a fresh-cell sentinel check.
- **Jezzball** (`gameJezzball.c`): The EEPROM "reset magic bytes" routine
  had a hardcoded address-0 typo instead of using its own loop variable,
  so the high score could never actually persist across a save/reload.
  Fixed to write the correct address.
- **Lights Out AD** (`gameLightsOutAD.c`): The "You won!" screen's time
  readout was recomputed every frame instead of once on entry, so it kept
  counting up for as long as the player lingered on the screen instead of
  showing the real, static finish time. Fixed to snapshot the time once.
- **Maruino** (`gameMaruino.c`): The "enter a code" screen's own on-screen
  text said Button C returns to the menu, but only Button A was actually
  checked - C did nothing there. Fixed so C genuinely returns to the menu.
- **MotoCross** (`gameMotoCross.c`): Real upstream's own debug speed
  readout (`print(player1.vx)`, a real `float`) was ported with the same
  "no float-print primitive exists" gap as Agaruino above - the value was
  truncated to `int` before printing, missing the fractional digit real
  hardware would show. Fixed once `gbPrintFloat()` was promoted to the
  shared shim for Agaruino's own identical need - this file's own call
  site now prints the genuine fractional value.
- **ShootBuino** (`gameShootBuino.c`):
  - `player_life` was only ever initialized once and never reset on a new
    game - after dying once, every subsequent restart started at 0 life
    and died on the very first hit for the rest of the session. Fixed by
    resetting it in `sbuinoInitGame()`.
  - Invader bullets were drawn with a negative height (`fillRect(...,1,
    -2)`), which draws nothing at all - completely invisible bullets that
    could still hit the player. Fixed to draw a real 2px vertical bullet.
- **Skibuino** (`gameSkibuino.c`): Choosing "Title Screen" from the pause
  menu never saved the high score (only a crash/game-over did) - quitting
  mid-run silently discarded a genuinely higher distance. Fixed to save
  on that path too.
- **World's Hardest Game** (`gameWhg.c`): A fresh EEPROM byte decoded as
  255, and the "completed" check was `> 0` - every level falsely showed
  as completed on a fresh save. Fixed with a fresh-cell sentinel check.
  (The separate "tries" counter accumulating across levels instead of
  resetting per level is left preserved - see below.)

- **Wolfenduino 3D** (`gameWolfenduino.c`): **The game could only ever play
  its first level.** `Map::streamData()` adds the per-level offset into the
  map file (`offset += currentLevel * MAP_SIZE * MAP_SIZE * 4`) only in its
  desktop `STANDARD_FILE_STREAMING` branch; the `PETIT_FATFS_FILE_STREAMING`
  branch that the real Gamebuino cartridge actually compiles never adds it, so
  finishing a level incremented the counter and reloaded the same map forever
  - all ten levels ship in the data and nine were unreachable. Fixed by adding
  that same offset to `wolfStreamData()`: this is upstream's own line,
  restored to the branch it was missing from, not an invention. Verified by
  temporarily starting on level 3 and confirming a visibly different map
  (different wall textures, layout and decorations), and by checking directly
  that all ten 16KB level blocks in `wolf3d.dat` differ from one another. One
  port-specific addition came with it: the level counter now wraps to 0 at the
  end of the set, because with the levels genuinely wired up, finishing the
  last one would otherwise stream past the end of the payload into an empty
  map with no walls and no player start.

- **Siena Town** (`gameSienaTown.c`): **The top score was never readable and
  every run claimed a new record.** `loop()` opens with `Topscore = 0;` on
  every single tick, wiping the value read out of EEPROM at startup before
  anything can use it - so the "Top Score" menu entry always showed 0, and
  the game-over test `Points > Topscore` was always true, meaning every run
  ended on "New record!" regardless of the score. The score really was being
  written to EEPROM; it was simply erased again before it could ever be read
  back. Fixed by removing that one line, on direct report. Everything else in
  the scoring path is upstream's own and untouched: the EEPROM address, the
  startup read, the game-over write and the `Record` latch.

- **Snake (Sigma-Squared)** (`gameSnakeSigma.c`): A fresh cartridge could
  never record its first high score. `max_score` is a single EEPROM byte read
  raw, and a factory-erased cell reads 255 - the highest value a `byte` score
  can ever reach - so `score > max_score` could never be true. Fixed with
  this project's own established fresh-cell reset (255 -> 0), the same
  pattern already applied to Gruniozerca and CrazyTown. Same accepted
  trade-off as Gruniozerca: a genuinely earned score of exactly 255 would be
  reset too, which is vanishingly unlikely.

## Preserved bugs

Deliberately left as real, shipped upstream behavior. Grouped by game.

**101 Starships, Simonbuino, Tetrino, B-Rally, Sokobuino, Breakout Ripper,
Lights Out AD, Bub, Video Poker, ZombiEscape, Maze, 2048, Shufflepuck
Cafe, Conduit, StickFighter, Tron, UFO Race, Asterocks, Paqman, DeathMaze,
FlappyBirdo, Punkt, No Name Platform Game, Master Kebab** - no
gameplay-negative preserved bugs found in the original sweep. (Stijn's
Snake is its own special case - see above, not this list.)

- **Bomberman** (`gameBomberman.c`): The health bar is drawn with a
  negative rectangle height, so real `Display::fillRect()`'s own loop never
  runs and the bar is permanently invisible - the numeric health readout
  beside it is the only real indication of damage. The enemy's own
  think-and-damage step is gated on `frameCount % 25`, which is true 24
  ticks out of every 25 rather than one in 25, so standing next to the
  enemy drains all 100 health in about half a second. `renderEdges()`
  passes a bottom coordinate where a height belongs, drawing the death
  screen's border taller than intended.

- **Bricks!** (`gameBricks.c`): The slow-ball power-up computes both of its
  vertical clamps from the ball's horizontal velocity, so collecting it
  copies horizontal speed onto the vertical axis instead of slowing the
  ball down. The warp power-up picks which side to draw its trail on by
  indexing the ball array with the ITEM index rather than the ball index.
  The exploding-ball power-up loops to `countBalls()` instead of over every
  slot, so with balls in non-contiguous slots it can arm fewer balls than
  are actually in play. Changing level frees items by writing to a field
  the allocator never tests, so items really do survive a level
  transition. `brickHit()`'s type-2/type-3 case falls through into the
  type-5 case, re-testing a brick it just decremented.

- **Discovery** (`gameDiscovery.c`): A shielded collision respawns the
  Klingon without awarding the five points the code's own comment
  describes. The downward movement clamp uses the ship's `w` field where
  `h` was clearly meant, letting the Discovery descend further than its
  own sprite height suggests. Three different hitbox sizes describe the
  same Klingon ship across the two collision tests. Losing the last life
  drops straight back to the title screen with no game-over screen and no
  score summary.

- **R.S.G.** (`gameRsg.c`): The losing screen never re-selects a font, so
  "YOU'RE DEAD!!" renders in the small font left over from the previous
  screen rather than the large one the winning screen uses.

- **Wolfenduino 3D** (`gameWolfenduino.c`): The difficulty check when
  streaming guards in falls through from the Hard case into Medium and then
  Easy with no `break`, so a guard's difficulty is re-tested against the lower
  tiers after it has already passed its own. The item-drop fallback search
  loops `i < cellX + 1` rather than `<=`, so a dying guard only ever tries the
  cell up-and-left of itself rather than its full 3x3 neighbourhood. Starting
  a NEW GAME does not reset the level counter, so a new game continues from
  whichever level was last reached. (The level-streaming bug that used to be
  listed here was fixed instead - see the fixed-bugs section.)

- **Copter (by Anny)** (`gameCopterAnny.c`): `did_crash()` runs twice per
  tick, once to gate the update and once to pick the score font, each walking
  all 21 corridor slices. The vertical-drift timer multiplies by the game
  speed twice where every other rate multiplies once, so drift changes come
  far less often than the surrounding code suggests. After a crash the game
  simply freezes mid-flight with no game-over screen at all.

- **Siena Town** (`gameSienaTown.c`): The bandit and the civilian are each
  re-rolled by an independent per-tick random chance, so both can teleport
  between lanes mid-shot, including into the lane you are already aiming at.
  The incoming bullet's hit test is an exact equality against one pixel
  column, so it can only ever register there. (The top-score bug that used to
  be listed here was fixed instead - see the fixed-bugs section.)

- **Super Crate Buino: Stone Edition** (`gameSuperCrateBuinoStone.c`): Three
  of the four entries on its own mode-select screen (Defence, MPDM, Story)
  blink "LOCKED!" and do nothing - their handlers are commented out in the
  fork's own source, and the `chooseMapDef()` function one of them would have
  called is unreachable dead code. The s-menu's "Reset score" entry has an
  empty body and likewise does nothing. Every map is unlocked from the first
  run (the fork removes the guard, marking it "crack map"), so the unlock
  progression still tracked in EEPROM no longer gates anything.

- **Walbloks** (`gameWalbloks.c`): A block landing on another snaps to a
  multiple of FOUR pixels even though every block is eight pixels tall, so
  stacks half-overlap rather than sitting flush - very visible, and a real
  part of how the game plays. `score` and `count` are both maintained and
  never read; the on-screen "Score:" prints the block count instead.

- **A to K** (`gameA2K.c`): Every D-pad press spawns a new tile even when
  the move changed nothing on the board, speeding up the fill-the-board
  loss condition.
- **Aerial-Assault** (`gameAerialAssault.c`):
  - Four `wait==0||10`-style throttle checks are unconditionally true -
    the pterodactyl and floater move/react far more aggressively than
    intended.
  - Lives readout is backwards (`<` instead of `>=`).
  - The "Pterodactyl" option toggle does nothing.
  - Options screen Up/Down are inverted.
  - Several enemy/bridge sprites are drawn flipped but collision-tested
    unflipped.
  - Removed platforms still "catch" the player in an invisible floating
    state.
  - The Hunter enemy's landing rules disagree with the player's own.
  - Two `random()` exclusive-upper-bound bugs permanently disable the
    floater's leftward movement and one of three spawn points.
  - (The Controls-menu `case 2:` fallthrough bug was already fixed in an
    earlier session, on direct request - not preserved.)
- **Agaruino** (`gameAgaruino.c`): The viewport-culling test uses `||`
  where it needs `&&`, so the player's own blob is never drawn -
  permanently invisible.
- **Aimbuino** (`gameAimbuino.c`): The aim-reset angle computes `(1/4)*PI`
  as integer division (truncates to 0) - every fresh shot starts aimed
  due-right instead of 45°-up-right.
- **Armageddon** (`gameArmageddon.c`): The enemy-missile launch-chance-
  scales-with-difficulty formula is dead code - frequency never actually
  increases as the game progresses.
- **Artillery** (`gameArtillery.c`):
  - An operator-precedence bug in the "unstick from terrain" check can
    push even a dead player upward.
  - The CPU AI's aim estimate uses the wrong lookup table and never tries
    max-elevation shots.
- **AsteroidRipper** (`gameAsteroidRipper.c`):
  - Bullets fire offset 45° from the ship's actual heading, with X/Y
    velocity also transposed - aiming is unreliable.
  - Level-complete is checked after that tick's collisions and
    unconditionally clears game-over - dying on the same hit that clears
    the last rock shows "Next Level" instead of "Game Over."
  - (The blank-screen pause bug is now fixed - see above.)
- **Bang! Bang!** (`gameBangBang.c`): The advertised second player-cannon
  (cannon2) never aims or shoots - always a static sinking target, no
  real duel. Confirmed directly from the real source, not just inferred:
  `PLAYER2AIMING`/`PLAYER2SHOOTING` are defined constants that `gamestate`
  is never once assigned anywhere in the file - every real transition
  only ever goes between `PLAYER1AIMING`/`PLAYER1SHOOTING`/`END`, and the
  one `PLAYER2SHOOTING` reference left in the source is inside a
  commented-out line. Marked unfinished in the menu (red text, info
  "Player 2 never shoots") rather than fixed, since there's no real
  upstream player-2 AI logic to restore - it was never implemented at
  all, on real hardware either. (The leftover "TAQUIN" pause title is now
  fixed - see above.)
- **BigBlackBox** (`gameBigBlackBox.c`): A patrol enemy's direction-
  reversal is two independent `if`s instead of `if`/`else`, so it can take
  one extra step right after reversing - inconsistent patrol timing.
- **BlocksBuino** (`gameBlocksBuino.c`):
  - T/S/S-mirrored pieces have one cell already inside the well a tick
    before the rest of the piece.
  - Single-line clears always score a flat 1 point regardless of level,
    while multi-line clears scale with level.
- **CopterStrike** (`gameCopterStrike.c`):
  - Every mobile tank unit's spawn-building index is a copy-paste bug
    always pointing at slot 0, corrupting infantry unit 0's own building
    assignment.
  - The money-award check for destroying certain mobile units compares the
    wrong sprite ID - that payout path is dead.
- **Copter** (`gameCopter.c`): Mid-game restart never resets buildings'
  drifted X/Y positions - a new attempt starts with stale scenery.
- **CrazyCar** (`gameCrazyCar.c`): Restart after game-over resets the car
  and lanes but not obstacle X positions or scroll offsets.
- **DarkShmup** (`gameDarkShmup.c`):
  - No death/game-over handling at all - health wraps via 8-bit overflow
    instead of ending the run.
  - Score silently wraps to 0 after 65535.
  - Missing `break` makes the boss run both its own AND another enemy
    type's update logic every tick.
  - Inverted loop-break condition silently drops roughly a quarter of
    enemy-wave spawn events.
- **DarkTower** (`gameDarkTower.c`): The well is one-way, and the rope
  needed to retrieve the crystal only appears in the cellar's object list
  on the first visit - climbing out without grabbing it first permanently
  locks the player out of finishing the game.
- **Descent Into Hell** (`gameDescent.c`):
  - Level-up monster-HP-scaling writes to an out-of-bounds table no
    monster reads - monsters never actually get tougher with level.
  - Restarting from the Map screen reuses the exact same dungeon instead
    of generating a fresh one.
  - During invulnerability flicker, bullets render in XOR/INVERT instead
    of solid black on alternating ticks.
  - (The monster-spawn retry-loop bug is now fixed - see above.)
- **Fifteen** (`gameFifteen.c`):
  - The solvability check isn't the real 15-puzzle parity rule - a
    freshly shuffled board can be genuinely unsolvable as dealt.
  - "Load saved" with no save drops the player into an empty, unmovable
    zero-dimension board until B or C is pressed.
- **Firebuino!** (`gameFirebuino.c`): Pressing both equivalent buttons for
  one direction on the same tick moves the stretcher two steps instead of
  one.
- **Footlol** (`gameFootlol.c`): Both collision-response blocks mix the
  current object's own `vx` with the *other* object's `vy` - inconsistent,
  asymmetric bounce physics.
- **GemGem** (`gameGemgem.c`):
  - "Load saved" restores the board but never resets the score.
  - The move-hint disables itself the instant it finds a hint, so hints
    never actually stay visible during live play.
- **GlaciGlaca** (`gameGlaciGlaca.c`): Restart only resets ice-cream
  flavor stock, never cone/cornet/luxe-cornet stock.
- **Gruniozerca** (`gameGruniozerca.c`): The acceleration counter keeps
  incrementing while pinned against a screen edge - holding into a wall
  then reversing snaps straight to top speed.
- **KillRace** (`gameKillrace.c`): `handledeath()` is called twice per
  tick during the crash animation, halving its intended duration.
- **Lander** (`gameLander.c`): Same double-call bug as KillRace - the
  death animation and "SHIPS LEFT" countdown play back twice as fast.
- **MicroHexagon** (`gameHexagon.c`):
  - `abs(rot_speed < SPEED_CAP)` applies `abs()` to the comparison's
    boolean result, not the speed - the rotation-speed cap is a no-op.
  - A stray comparison instead of assignment means the screen's pulse
    timer never resets after the first pulse.
- **Minesweeper** (`gameMinesweeper.c`): Mine placement uses an exclusive-
  upper-bound random range on both axes - the rightmost column and bottom
  row can never contain a mine.
- **Mole Control** (`gameMoleControl.c`):
  - Restart sets spawn/timeout deadlines to `duration + current time`,
    narrowed to signed 16-bit - restarting after ~30 seconds wraps
    negative, draining all lives in under a second.
  - The intended random spawn-interval range is dead code - moles always
    spawn at the fastest interval.
- **MotoCross** (`gameMotoCross.c`): Holding LEFT in the wrong pose can
  incorrectly fall through to the "fallen off" (KO) pose every 10th frame.
- **PinBall** (`gamePinball.c`): Restart resets ball/lives/flippers but
  never score or leftover velocity; losing all lives just wraps the life
  counter back to 2 instead of ending the game.
- **Pong 2017** (`gamePong2017.c`):
  - Draw color is never reset after drawing the net - HUD elements
    (player names, life gauges, round-win bars, trick icons) render gray
    instead of black. Unlike Super Crate Buino's superficially similar
    case above, this one is confirmed, not just theorized - independently
    verified live by the user via a real emulator screenshot showing the
    same gray rendering with real upstream's own unmodified source.
  - 3 of 5 selectable "tricks" have menu icons/timers but their actual
    effect was never implemented.
  - A trick's active-frame counter over-increments on its own reset tick.
- **Robot** (`gameRobot.c`):
  - The tesla-tower enemy's draw position and bullet hit-test both reuse
    the wrong fields as coordinates.
  - The death check is never called during boss fights - health can go
    arbitrarily negative with no consequence.
  - No real Game Over state exists at all - losing a life just resets the
    current level, with lives never checked for reaching 0. Only 2 of the
    5 worlds have a genuinely unique layout (the other 3 all reuse the
    same map data). Marked unfinished in the cartridge's own menu (red
    list text, info "No Game Over / only 2 levels").
- **SavePrincesse** (`gameSavePrincesse.c`): Hit/attack collision always
  uses the wider "attacking pose" hitbox regardless of the pose actually
  drawn.
- **Senet** (`gameSenet.c`): The winner screen always prints "CPU WON"
  whenever player 1 doesn't win, with no mode check.
- **Shipwrek** (`gameShipwrek.c`): Game-over is only checked once per full
  turn - a fast player can sneak in an extra bonus turn.
- **Smash and Crash** (`gameSmash.c`):
  - Float-to-int truncation makes the arrow move slower and the leftward
    ball twice as fast as intended.
  - `random(0,1)`'s exclusive upper bound means the ball never moves
    right.
  - On the Ridge map, two of four "Arrow Collision" resets test the wrong
    object.
  - On the Tower map, a bogus fifth collision branch creates an invisible
    "soft ceiling."
  - On the Tower map, meteor-vs-platform collision only checks 3 of 5
    platforms.
- **Snake 5110** (`gameSnake5110.c`): The "remove last wall segment on an
  axis" check has an inverted condition - one wall segment per axis can
  never be cleared.
- **Snake ABC** (`gameSnakeAbc.c`): Letter-pickup collision checks the
  head's pre-move position - the snake must sit visibly on the letter for
  one extra move before it registers.
- **Snake Classic** (`gameSnakeClassic.c`):
  - Up/Down are never read for movement - only Left/Right steer the snake.
  - Starting-position calc swaps the X/Y axis feed - the snake starts near
    the bottom edge instead of centered.
- **Solitaire** (`gameSolitaire.c`): Because `persistence` is forced true
  before any frame draws, the win-condition path meant to trigger the full
  celebration and auto-return to title is unreachable.
- **Star Honor** (`gameStarHonor.c`): The repair-completion message table
  has "Engines" and "Shields" swapped.
- **Stijn's Pong** (`gameStijnPong.c`): `resetBall()` recenters using
  `LCDWIDTH` instead of `LCDHEIGHT` - the ball re-spawns near the bottom
  of the screen instead of vertical center.
- **Super Crate Buino** (`gameSuperCrateBuino.c`):
  - Y-axis collision is incorrectly gated on the X-bounce value - some
    bullet types never collide with floors/walls.
  - A `switch` fallthrough gives the Machine Gun the Shotgun's velocity
    spread.
  - The very first crate grab on a fresh map always rerolls to Pistol, but
    the popup switch has no Pistol case - no feedback on that pickup.
  - (The map-select "LOCKED!" screen-goes-invisible bug is now fixed - see
    above.)
- **Super Space Shooter** (`gameSuperSpaceShooter.c`): There is no real
  enemy spawner - a "temporary testing" button is the only way any enemy
  ever appears.
- **Thunder Shoot** (`gameThunderShoot.c`):
  - Car 3's bullet-collision reuses car 2's random gap offset.
  - The chargeable power-bar beam weapon never actually damages anything.
- **T-Rex Quest** (`gameTrexQuest.c`):
  - One trex-vs-vehicle collision check compares a mismatched coordinate
    pair.
  - Game Over and Level Up checks aren't mutually exclusive.
- **Taquin** (`gameTaquin.c`): A blocked D-pad press still increments the
  move counter even though nothing moved.
- **World's Hardest Game** (`gameWhg.c`): The "tries" counter only resets
  when a level is freshly picked from the menu, not on auto-advance after
  winning - tries accumulate across levels instead of resetting per level.
  (The fresh-EEPROM false-"completed" bug is now fixed - see above.)
- **Xonix** (`gameXonix.c`): An enemy's bounce logic only tests orthogonal
  neighbor cells before a diagonal move - it can briefly step onto claimed
  territory that should be safe.
