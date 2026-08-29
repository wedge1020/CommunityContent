// Super Crate Buino (Aurelien Rodot, license: none specified -
// github.com/Rodot/Super-Crate-Buino). A real action-platformer inspired by
// the PC game "Super Crate Box": one arena per map, a steady stream of
// enemies spawns from the top-center and chases the player, and the only
// way to score is to keep grabbing crates that fall from the sky - each
// crate re-rolls the player's current weapon at random (from whatever has
// been unlocked so far) and awards one point. Reaching per-map score
// thresholds unlocks new weapons and, eventually, the next of 5 real maps.
// Dying (touching an active enemy, or a real DISK bullet) resets score to
// zero and respawns on the same map.
//
// STRUCTURAL FLATTENING: real upstream is a small real C++ class hierarchy
// (`Box` as a base class with virtual `getWidth()`/`getHeight()`/
// `getGravity()`/`getMaxSpeed()`/`getXFriction()`/`getYFriction()`/
// `getXBounce()`/`getYBounce()`, and `Bullet`/`Player`/`Enemy`/`Crate` all
// inheriting it and overriding a subset) - this dialect has no classes or
// inheritance at all (see VIRCON32_C_DIALECT.md), so every entity's real
// per-subtype "virtual getter" became a plain function taking whatever
// state it needs as parameters (`scbsBulletGetWidth( subtype, vx, timeLeft )`
// etc), and the one real shared physics step (`Box::update()`) became
// `scbsBoxUpdate()`, a single function taking every entity's current x/y/vx/
// vy/dir as OUT-parameters (the "everything by pointer" idiom
// VIRCON32_C_DIALECT.md documents for any >1-word return) plus that
// entity's own already-computed gravity/friction/bounce/maxSpeed values -
// called identically by Bullet/Player/Enemy/Crate's own update functions
// below, exactly mirroring how each of their real C++ counterparts called
// the one real inherited `Box::update()`. `Bullet`/`Enemy` (both used as
// real arrays upstream) became `struct ScbsBullet`/`struct ScbsEnemy` arrays;
// `Player`/`Crate`/`World`/`Weapon` (each only ever a single real instance)
// became plain flat globals. `Weapon.shooter` is always the single real
// player instance in upstream (set once in `Player::init()` and never
// reassigned), so every `shooter->x`/`shooter->getWidth()` call below reads
// the player's own globals directly instead of carrying a "shooter"
// reference of any kind - a simplification that loses nothing, matching
// this project's own established "flatten a real single-instance
// relationship" precedent.
//
// Every real `gb.x.y(...)` call site is mechanically rewritten to a plain
// `gbY(...)` function call (see gamePong.c's own header comment - this
// dialect has no classes/methods). `random(N)`/`random(a,b)` became
// `arand(N)`/`scbsArduinoRandom(a,b)` (the latter a direct, deliberate port
// of real Arduino core's own `random(long,long)` - see its own comment
// below for why this exact port, not a naive `a+arand(b-a)` rewrite,
// matters for two real, verified upstream quirks). `byte`/`char` became
// plain `int` throughout (no genuinely-boolean `byte` usage here the way
// gameAgaruino.c had). Every real `switch` statement became an if/else-if
// chain (this dialect's own `switch` support remains unproven - matching
// the same caution gamePong.c/gameTaquin.c/gameUfoRace.c already
// established), which also made every real (and there are several - see
// below) upstream fallthrough behavior something that had to be traced
// through by hand and reproduced explicitly rather than something the
// if/else-if rewrite could get by accident.
//
// REAL BITMAP ART RESTORED VERBATIM: every one of upstream's real
// `PROGMEM` byte tables (the title `logo`, the 5 real tile-bitmaps-encoded-
// as-bitmaps world maps, all 7 real ground/platform/wall tile textures used
// across them, all 13 real weapon sprites plus their 5 real white-highlight
// overlays, the 5-frame player sprite, the 5-frame small-enemy sprite, the
// 6-frame big-enemy sprite, and the crate sprite) was copied byte-for-byte
// into plain `int[]` arrays below - hex literals copied directly, and the
// 5 real world maps (upstream's own `B########`-style binary literals, one
// per byte) converted to the equivalent hex value one-for-one via a small
// verification script (counting each map's own real declared width/height
// against its own real total element count, catching any transcription
// mistake immediately rather than trusting it by eye - every one of the 5
// matched its own expected count exactly). `largeChecker` (a real declared
// `PROGMEM` tile bitmap) and `revolver_sound`/`player_damage_sound` (two
// real declared `PROGMEM` sound patterns) are never referenced anywhere
// else in the real upstream source - confirmed via a direct grep, not
// assumed - so all three are genuinely dead upstream constants, correctly
// omitted here rather than ported as unused data.
//
// MAPS ARE REAL BITMAPS, REUSED DIRECTLY AS BITMAPS: upstream's own real
// `World::tileAtPosition()`/`World::draw()` call `gb.display.
// getBitmapPixel(tiles, x, y)` directly on the very same `map0..map4[]`
// byte arrays also passed to `gb.display.drawBitmap()` for the map-select
// preview - a real, deliberate upstream dual-use of one data format (this
// shim's own `gbGetBitmapPixel()`/`gbDrawBitmap()` share the exact same
// `[width,height,then packed rows]` layout real `Display::getBitmapPixel()`/
// `drawBitmap()` do), so `scbsMap0..scbsMap4[]` below are genuine drop-in
// replacements needing no reinterpretation, and `scbsTileAtPosition()`/
// `scbsWorldDraw()` call `gbGetBitmapPixel()` on them exactly like upstream.
//
// EEPROM: upstream's own real `loadEEPROM()`/`saveEEPROM()`/
// `EEPROMreadInt()`/`EEPROMwriteInt()`/`cleanEEPROM()` (a real 2-byte
// token + per-map best-score table + unlocked-weapons/unlocked-maps bytes,
// with a real "only write if the stored value is stale" guard to limit
// wear) port directly onto this shim's own `eeprom_read_byte()`/
// `eeprom_write_byte()` (real address range 0-1023, matching
// `cleanEEPROM()`'s own real 1024-byte wipe loop) - see `scbsLoadEeprom()`/
// `scbsSaveEeprom()` below, called from this game's own `init()` and from
// every real call site upstream calls them from (every scoring death, the
// pause-menu's own "B: SAVE & QUIT", and once per real gameplay tick from
// the main PLAY state, exactly matching upstream's own `saveEEPROM()`
// call inside `loop()`'s own `if (gb.update())` block).
//
// A REAL CROSS-GAME-LAUNCH RESET THIS PORT NEEDS THAT UPSTREAM NEVER DID:
// this game's own local popup state (`scbsPopupTimeLeft`/`scbsPopupText` -
// see "POPUP" below for why it is local rather than the shared shim's own
// `gbPopup()`) is explicitly zeroed in `gameSuperCrateBuinoStone_init()`, along
// with `scbsShakeTimeLeft`. Real hardware only ever runs one sketch per
// power-on, so upstream never needed this; this cartridge can relaunch the
// same game many times in one session, and without this reset a stale
// nonzero popup timer left over from a previous play session (e.g. a
// "GAME OVER!" message that was still mid-slide-out when the player backed
// out to the top-level menu) would incorrectly resume animating on the
// next launch - the same class of fix `gbFrameCount`/the shared shim's own
// `gbPopupTimeLeft` already needed, documented in this project's own
// CLAUDE.md, just applied here to a second, game-local popup timer that
// mechanism doesn't cover.
//
// POPUP - A LOCAL REIMPLEMENTATION, NOT THE SHARED `gbPopup()`: matching
// `gameMinesweeper.c`'s/`game2048.c`'s own established precedent for
// deliberately NOT migrating to the shared shim primitive when a game has
// a genuine, specific extra requirement it doesn't cover - here, real
// `Player::kill()` explicitly force-cancels whatever popup is currently
// showing (`popupTimeLeft = 0;`) before deciding whether to show a fresh
// "NEW HIGHSCORE!" one, and the shared `gbPopup()`'s own internal timer
// isn't exposed for a game to reset directly. `scbsPopup()`/
// `scbsUpdatePopup()` below are instead a direct, hand-ported copy of real
// upstream's own `popup()`/`updatePopup()` (the same real slide-in-from-
// the-bottom, auto-dismissing bordered box, built from the same
// `gbDrawRect()`/`gbFillRect()`/`gbPrintString()` primitives), giving this
// game the real force-cancel capability upstream's own upstream code
// relies on.
//
// SOUND - REAL PATTERNS, PLAYED FOR REAL: every real upstream
// `gb.sound.playPattern(name_sound, channel)` call (13 real distinct
// weapon-fire/explosion/enemy/jump/pickup effects) now plays for real via
// `gbPlayPattern()` - the real tracker/pattern engine gamebuinoShim.c/.h
// implements (see that file's own Sound section header comment) - using
// the real, byte-for-byte pattern data (`scbsBlastSound[]`/
// `scbsRocketSound[]`/`scbsMachinegunSound[]`/`scbsGrenadeSound[]`/
// `scbsShotgunSound[]`/`scbsLaserSound[]`/`scbsClubSound[]`/`scbsJumpSound[]`/
// `scbsEnemyFeltSound[]`/`scbsEnemyDeathSound[]`/`scbsPowerUpSound[]`, see the
// "Sound patterns" section below), each played on the exact real channel
// upstream's own call site uses (0 for every weapon-fire/blast effect, 1
// for jump/enemy-death, 2 for enemy-felt/two of the three `power_up_sound`
// sites - real upstream's own `Weapon::shoot()`/`Bullet::update()`/
// `Player::update()`/`Enemy::update()`/`Crate::update()` call sites were
// each individually traced to confirm both the pattern name and channel
// number). No `changeInstrumentSet()`/`command(CMD_INSTRUMENT,...)` call
// precedes any of them upstream either, so the engine's own real default
// square-wave instrument on every channel is already correct.
// `playTick()`/`playOK()` (PISTOL/AKIMBO/RIFLE fire, and picking up a
// crate) were already real one-shot calls, unchanged.
// `player_damage_sound[]`/`revolver_sound[]` (2 of the 13 real declared
// constants) are genuinely never referenced anywhere in real upstream
// source (confirmed via grep), so, matching this file's own already-
// established treatment of `largeChecker`, they're correctly omitted here
// too - only the 11 real, actually-called patterns are ported.
// `gb.sound.chanVolumes[2] = 1;` (upstream's own setup()-time comment:
// "this game requires 3 channels...") is dropped outright, matching the
// engine's own deliberate, documented design (see gamebuinoShim.c's own
// comment on `gbUpdateNoteChannel()`): real hardware's `chanVolumes`/
// `globalVolume` exist purely to keep several real channels summed into
// ONE shared physical PWM output from clipping, which doesn't apply here
// (each Vircon32 SPU channel mixes independently in hardware). Worth
// noting: on real hardware this specific call is also provably a no-op -
// `VOLUME_CHANNEL_MAX` (real `Sound.cpp`'s own real per-channel default,
// `255/NUM_CHANNELS/7/9`) evaluates to exactly `1` for any of upstream's
// own commented "requires 3 channels" value, this project's real
// `MAX_SOUND_CHANNELS` of 4, or the real library's own default of 1 - so
// `chanVolumes[2]` already defaults to the same `1` this line explicitly
// (and redundantly) sets it to on real hardware too. `gb.pickRandomSeed()`/
// `gb.battery.show = false;` are dropped, matching gamePong.c's own
// identical treatment of both.
//
// SEVERAL REAL UPSTREAM BUGS/QUIRKS, TRACED THROUGH (NOT ASSUMED) AND
// PRESERVED EXACTLY, PER THIS PROJECT'S OWN ESTABLISHED NORM:
//
// 1) `Box::update()`'s own real Y-axis collision-resolution block is gated
//    on `if (getXBounce() >= 0)` - the SAME condition guarding the X-axis
//    block above it, not `getYBounce() >= 0` (confirmed directly against
//    the real source, not assumed a typo away). Traced through for every
//    entity that ever returns a negative `getXBounce()` (dead Player,
//    Bullet subtypes CLUB/EXPLOSION/LASER, dead Enemy): every one of them
//    ends up with ZERO real Y-axis world collision at all while that
//    condition holds, letting a dead player's/dead enemy's ragdoll (and a
//    club swing's/explosion's/laser bolt's own bullet hitbox) fall or fly
//    straight through floors and walls - a real, visible, load-bearing
//    gameplay effect (the death "fling" clipping through the map), not an
//    inert corner case, so `scbsBoxUpdate()` below gates both its X and Y
//    resolution blocks on the same `xBounce` parameter, exactly like real
//    `Box::update()` does.
//
// 2) `Weapon::addBullet()`'s own real screen-shake `switch` has a genuine
//    fallthrough: `case W_SNIPER: case W_REVOLVER: shakeTimeLeft=4;
//    shakeAmplitude=4; case W_MACHINEGUN: shakeTimeLeft=2;
//    shakeAmplitude=1;` - SNIPER/REVOLVER's own distinct heavier shake
//    value is set, then immediately overwritten by falling into
//    MACHINEGUN's own weaker one (no `break`). Net real effect: all three
//    weapons produce the exact same (weaker) shake. `scbsWeaponAddBullet()`
//    below reproduces this by setting the MACHINEGUN values directly for
//    all three subtypes, rather than the SNIPER/REVOLVER values the
//    upstream `case` labels suggest were intended.
//
// 3) The same function's own real initial-bullet-speed `switch` has a
//    second fallthrough: `case W_MACHINEGUN: vx=...; vy=random(-16,17);
//    shooter->vx -= shooter->dir*32; case W_SHOTGUN: vx=...;
//    vy=random(-10,11); break;` - MACHINEGUN's own distinct vy spread
//    (-16..17) is computed (and the real player-recoil side effect fires,
//    using the pre-fallthrough `shooter->dir`), then immediately discarded
//    by falling into SHOTGUN's own fresh recompute. Net real effect:
//    MACHINEGUN bullets get the exact same vx/vy DISTRIBUTION as SHOTGUN
//    bullets (just an independent random draw), while still applying the
//    real recoil kick SHOTGUN itself never gets. Reproduced exactly below.
//
// 4) The same function's own real horizontal-offset `switch` has a THIRD
//    fallthrough: `case W_SHELL: x-=16; case W_ROCKET: case W_CLUB:
//    x-=dir*32; case W_MINE: break; default: x+=dir*46;` - SHELL genuinely
//    gets BOTH its own `-16` offset AND the ROCKET/CLUB `-dir*32` offset
//    applied on top of it (not a bug exactly, but a real cascade worth
//    calling out) - reproduced exactly (`x -= 16; x -= dir*32;` for SHELL
//    specifically, both applied).
//
// 5) Real `Bullet::getDamage()`'s own `switch (subtype) { return 1; case
//    W_REVOLVER: ... }` has a leading `return 1;` statement with no `case`
//    label of its own, positioned before the first real case label - under
//    real C/C++ switch semantics this is unreachable dead code (a `switch`
//    jumps straight to whichever label matches; nothing ever "falls into"
//    the very top of the body unless a label sits there) - the function's
//    real reachable behavior is exactly its own explicit `case`s plus its
//    own `default: return 1;` at the bottom. `scbsBulletGetDamage()` below
//    implements only the real reachable logic.
//
// 6) Real Arduino core's own `random(long howsmall, long howbig)`
//    (`WMath.cpp`) has a real, defined short-circuit: if `howsmall >=
//    howbig`, it returns `howsmall` UNMODIFIED, with no random draw at
//    all. `scbsArduinoRandom()` below is a direct, deliberate port of that
//    exact function (not a naive `min + arand(max-min)` rewrite) because
//    two real upstream call sites depend on this short-circuit for their
//    own real, verified behavior, not just a hypothetical edge case:
//      - `EnemiesEngine::update()`'s own real `enemies[i].vy =
//        random(-48, -64);` (arguments reversed from the "obviously
//        intended" `random(-64,-48)`, presumably a real upstream typo) -
//        under the real short-circuit this ALWAYS evaluates to exactly
//        `-48`, not a randomized upward launch speed at all. A killed
//        enemy's own vertical launch velocity is therefore a real
//        constant on real hardware too, not a range - preserved exactly.
//      - `Crate::update()`'s own real weapon-reroll `random(1,
//        unlockedWeapons+1)`: while `unlockedWeapons==0` (true for every
//        map before its own first weapon unlock), `howsmall(1) >=
//        howbig(1)` holds, so this also always evaluates to the constant
//        `1` - though this particular case is provably inert either way,
//        since the surrounding `% (unlockedWeapons+1)` is `% 1`, which is
//        always `0` regardless of the random draw's own value.
//
// 7) Because of bug/quirk 6 immediately above, real upstream's own weapon-
//    name-popup `switch (player.weapon.subtype)` (in `Crate::update()`)
//    has no `case W_PISTOL:` at all - and since the reroll can only ever
//    actually land on subtype 0 (PISTOL) while `unlockedWeapons==0`
//    (every subsequent reroll's own real range is `[1,unlockedWeapons]`,
//    never touching 0 again), this is exactly the one subtype the reroll
//    can produce that has no matching popup case. Net real effect: the
//    very first crate pickup on a fresh map0 (before anything is unlocked)
//    silently shows NO weapon-name popup at all (score still increments,
//    the popup logic below just has nothing to show) - preserved exactly,
//    no `case`/`else if` added for PISTOL in `scbsCrateUpdate()` below.
//
// 8) `loop()`'s own real camera-follow gating literally reads
//    `world.getWidth()*SCALE <= LCDWIDTH` / `...getHeight()*SCALE <=
//    LCDHEIGHT` - `getWidth()`/`getHeight()` already have one real `SCALE`
//    factor baked in (`SPRITE_SIZE*tiles_wide*SCALE`), so multiplying by
//    `SCALE` again yields a number in the tens of thousands for every one
//    of the 5 real shipped maps, meaning this condition is realistically
//    always false (a likely real upstream typo - probably meant `/SCALE`)
//    - but verified provably inert for all 5 real maps either way (even
//    the "corrected" divide-based version stays false for every one of
//    them too, since even the smallest map's own screen-space width/height
//    already exceeds LCDWIDTH/LCDHEIGHT), so ported literally
//    (`scbsWorldGetWidth() * SCBS_SCALE <= LCDWIDTH`) rather than "fixed".
//
// 9) `World::draw()`'s own real edge/corner-tile detection runs two
//    independent, non-exclusive `if` checks (right-neighbor-empty sets
//    `flip=FLIPH; offset=2`; left-neighbor-empty sets `bitmap=edge` again
//    but does NOT reset `flip`/`offset`) - for the (real, if rare) case of
//    an isolated single-tile-wide platform where BOTH neighbors are empty,
//    the tile is drawn using the FIRST check's own flip/offset regardless.
//    Ported as the same two sequential, non-exclusive `if`s (no `else`,
//    no reset) in `scbsWorldDraw()` below.
//
// 10) `loop()`'s own real camera-smoothing code (a 3-line `cameraX =
//     (3*cameraX+x)/4` EMA blend) is entirely commented out in upstream's
//     own shipped source (`//int x = ...; //cameraX = ...`) - real
//     shipped behavior is an instant, unsmoothed camera snap every tick
//     (the two lines immediately below it, which are NOT commented out).
//     Not ported here either, matching the real shipped behavior exactly
//     (this is not a simplification on this port's part - the dead code
//     was already dead in the real, distributed upstream source).
//
// DROPPED WITH NO OBSERVABLE EFFECT: `World::draw()`'s own real `byte
// tileNumber = 1; //platform by default` is assigned once and never read
// again anywhere in that function - genuinely dead, dropped here too.

#define SCBS_SCALE 8
#define SCBS_SPRITE_SIZE 6

#define SCBS_NUM_MAPS 5

// Stone Edition's own four mode-select entries.
#define SCBS_NUM_MODES 4
#define SCBS_NUM_BULLETS 20
#define SCBS_NUM_ENEMIES 20
#define SCBS_NUM_WEAPONS 13
#define SCBS_NUM_THRESHOLDS 5

// weapon/bullet subtypes - real upstream `W_*`/`E_*` defines
#define SCBS_W_PISTOL 0
#define SCBS_W_RIFLE 1
#define SCBS_W_SHOTGUN 2
#define SCBS_W_ROCKET 3
#define SCBS_W_CLUB 4
#define SCBS_W_REVOLVER 5
#define SCBS_W_MINE 6
#define SCBS_W_SNIPER 7
#define SCBS_W_MACHINEGUN 8
#define SCBS_W_GRENADE 9
#define SCBS_W_AKIMBO 10
#define SCBS_W_DISK 11
#define SCBS_W_LASER 12
#define SCBS_W_EXPLOSION 13
#define SCBS_W_SHELL 14

#define SCBS_E_SMALL 0
#define SCBS_E_BIG 1

#define SCBS_SCORETHRESHOLD_1 4
#define SCBS_SCORETHRESHOLD_2 8
#define SCBS_SCORETHRESHOLD_3 12
#define SCBS_SCORETHRESHOLD_4 14
#define SCBS_SCORETHRESHOLD_5 16

#define SCBS_EEPROM_TOKEN 0xCAB2
#define SCBS_EEPROM_WEAPONS_OFFSET 4
#define SCBS_EEPROM_MAPS_OFFSET 6
#define SCBS_EEPROM_SCORE_OFFSET 32

// -----------------------------------------------------------------------------
// Real upstream bitmap art, byte-for-byte (see this file's own header
// comment on how the 5 maps' own real `B########`-style binary literals
// were converted to hex, and how everything else needed no conversion at
// all since it was already declared in hex).
// -----------------------------------------------------------------------------

// Stone Edition's own title logo and its four mode-select logos,
// converted from upstream's own PROGMEM tables to hex byte-for-byte.

// logo: 64x30
int[242] scbsLogoBitmap =
{
    0x40, 0x1E, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF,
    0xFF, 0xF0, 0x00, 0x00, 0x00, 0x81, 0x32, 0x04,
    0x08, 0x10, 0x00, 0x00, 0x00, 0x9F, 0x32, 0x64,
    0xF9, 0x90, 0x00, 0x00, 0x00, 0x81, 0x32, 0x04,
    0x38, 0x30, 0x00, 0x00, 0x00, 0xF9, 0x32, 0x7C,
    0xF9, 0x90, 0x00, 0x00, 0x00, 0x81, 0x02, 0x7C,
    0x09, 0x90, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF,
    0xFF, 0xF0, 0x00, 0x00, 0x00, 0x81, 0x02, 0x04,
    0x08, 0x10, 0x00, 0x00, 0x00, 0x9F, 0x32, 0x67,
    0x39, 0xF0, 0x00, 0x00, 0x00, 0x9F, 0x06, 0x07,
    0x38, 0x70, 0x00, 0x00, 0x00, 0x9F, 0x32, 0x67,
    0x39, 0xF0, 0x00, 0x00, 0x00, 0x81, 0x32, 0x67,
    0x38, 0x10, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF,
    0xFF, 0xF0, 0x00, 0x00, 0x00, 0x60, 0x4C, 0x93,
    0x20, 0x60, 0x60, 0x00, 0x00, 0x26, 0x4C, 0x91,
    0x26, 0x40, 0x01, 0xF8, 0x00, 0x20, 0xCC, 0x92,
    0x26, 0x40, 0x0D, 0xF8, 0x20, 0xA6, 0x4C, 0x93,
    0x26, 0x40, 0x81, 0x0F, 0xE4, 0x20, 0x40, 0x93,
    0x20, 0x40, 0x01, 0xBF, 0x80, 0x3F, 0xFF, 0xFF,
    0xFF, 0xC0, 0x01, 0xF8, 0x00, 0x07, 0xFF, 0xFF,
    0xFE, 0x00, 0x01, 0xF8, 0x00, 0x00, 0xE0, 0x60,
    0x70, 0x00, 0x01, 0xF8, 0x00, 0x00, 0x27, 0xE7,
    0xC0, 0x00, 0x01, 0x98, 0x00, 0x00, 0x20, 0x61,
    0xC0, 0x00, 0xFF, 0xFF, 0xF0, 0x00, 0x3E, 0x67,
    0xC0, 0x00, 0x00, 0x00, 0x08, 0x00, 0x20, 0x60,
    0x40, 0x00, 0x00, 0x00, 0x08, 0x00, 0x3F, 0xFF,
    0xC0, 0x00, 0xC3, 0x0C, 0x08, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x34, 0xD3, 0x48, 0x00, 0x00, 0x00,
    0x00, 0x00, 0xCB, 0x2C, 0xB8, 0x00, 0x00, 0x00,
    0x00, 0x00
};

// scb_classic: 64x30
int[242] scbsModeClassic =
{
    0x40, 0x1E, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0xE8, 0x4E, 0xEE, 0xE0,
    0x00, 0x00, 0x00, 0x00, 0x88, 0xA8, 0x84, 0x80,
    0x00, 0x00, 0x00, 0x00, 0x88, 0xEE, 0xE4, 0x80,
    0x00, 0x00, 0x00, 0x00, 0x88, 0xA2, 0x24, 0x80,
    0x00, 0x00, 0x00, 0x00, 0xEE, 0xAE, 0xEE, 0xE0,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x07, 0xFF, 0xFF, 0xFF, 0xFC,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03,
    0xF0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02,
    0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03,
    0xB0, 0x00, 0x18, 0x00, 0x00, 0x03, 0xFE, 0x03,
    0x70, 0x00, 0x00, 0x7E, 0x00, 0x03, 0xFE, 0x02,
    0x10, 0x00, 0x03, 0x7E, 0x08, 0x02, 0xDE, 0x03,
    0xF0, 0x00, 0x60, 0x43, 0xF9, 0x82, 0xDE, 0x3F,
    0x7E, 0x00, 0x00, 0x6F, 0xE0, 0x1B, 0xFE, 0x21,
    0x42, 0x00, 0x00, 0x7E, 0x00, 0x03, 0xFE, 0x3B,
    0x76, 0x00, 0x80, 0x7E, 0x00, 0x03, 0xFE, 0x37,
    0x6E, 0x00, 0x00, 0x7E, 0x00, 0x03, 0x8E, 0x21,
    0x42, 0x00, 0x00, 0x66, 0x00, 0x03, 0x8E, 0x3F,
    0x7E, 0x00, 0x1F, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xF8, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x08, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x08, 0x10, 0xC3, 0x0C, 0x30, 0xC3, 0x0C,
    0x30, 0xC8, 0x0D, 0x34, 0xD3, 0x4D, 0x34, 0xD3,
    0x4D, 0x30, 0x02, 0xCB, 0x2C, 0xB2, 0xCB, 0x2C,
    0xB2, 0xC0
};

// scb_defence: 64x30
int[242] scbsModeDefence =
{
    0x40, 0x1E, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x01, 0xCE, 0xEE, 0x97, 0x70,
    0x00, 0x00, 0x00, 0x01, 0x28, 0x88, 0xD4, 0x40,
    0x00, 0x00, 0x00, 0x01, 0x2E, 0xEE, 0xB4, 0x70,
    0x00, 0x00, 0x00, 0x01, 0x28, 0x88, 0x94, 0x40,
    0x00, 0x00, 0x00, 0x01, 0xCE, 0x8E, 0x97, 0x70,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x1F, 0xFF, 0xFF, 0xFF, 0xFF,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x03, 0xFE, 0x0C, 0x00, 0x00, 0x01,
    0xFF, 0x00, 0x03, 0xFE, 0x00, 0x3F, 0x00, 0x01,
    0xFF, 0x00, 0x03, 0xDA, 0x01, 0xBF, 0x04, 0x01,
    0x6F, 0x00, 0x03, 0xDA, 0x30, 0x21, 0xFC, 0xC1,
    0x6F, 0x00, 0x03, 0xFE, 0x00, 0x37, 0xF0, 0x0D,
    0xFF, 0x00, 0x03, 0xFE, 0x00, 0x3F, 0x00, 0x01,
    0xFF, 0x00, 0x03, 0xFE, 0x00, 0x3F, 0x00, 0x01,
    0xFF, 0x00, 0x03, 0x8E, 0x00, 0x3F, 0x00, 0x01,
    0xC7, 0x00, 0x03, 0x8E, 0x00, 0x33, 0x00, 0x01,
    0xC7, 0x00, 0x1F, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xF8, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x08, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x08, 0x10, 0xC3, 0x0C, 0x30, 0xC3, 0x0C,
    0x30, 0xC8, 0x0D, 0x34, 0xD3, 0x4D, 0x34, 0xD3,
    0x4D, 0x30, 0x02, 0xCB, 0x2C, 0xB2, 0xCB, 0x2C,
    0xB2, 0xC0
};

// scb_mpdm: 64x30
int[242] scbsModeMpdm =
{
    0x40, 0x1E, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x11, 0x71, 0xC8, 0x80,
    0x00, 0x00, 0x00, 0x00, 0x1B, 0x55, 0x2D, 0x80,
    0x00, 0x00, 0x00, 0x00, 0x15, 0x71, 0x2A, 0x80,
    0x00, 0x00, 0x00, 0x00, 0x11, 0x45, 0x28, 0x80,
    0x00, 0x00, 0x00, 0x00, 0x11, 0x41, 0xC8, 0x80,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x03, 0xFF, 0xFF, 0xFF, 0xFC,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x06, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x1F, 0x80, 0x00, 0x00, 0x11,
    0xFA, 0x00, 0x00, 0xDF, 0x82, 0x00, 0x00, 0x1F,
    0xFE, 0x00, 0x18, 0x10, 0xFE, 0x60, 0x00, 0x1F,
    0xFE, 0x00, 0x00, 0x1B, 0xF8, 0x00, 0x00, 0x1F,
    0xFE, 0x00, 0x00, 0x1F, 0x80, 0x00, 0x00, 0x11,
    0xFA, 0x00, 0x00, 0x1F, 0x80, 0x00, 0x00, 0x01,
    0xF8, 0x00, 0x00, 0x1F, 0x80, 0x00, 0x00, 0x01,
    0x98, 0x00, 0x00, 0x19, 0x80, 0x00, 0x00, 0x01,
    0x98, 0x00, 0x1F, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xF8, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x08, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x08, 0x10, 0xC3, 0x0C, 0x30, 0xC3, 0x0C,
    0x30, 0xC8, 0x0D, 0x34, 0xD3, 0x4D, 0x34, 0xD3,
    0x4D, 0x30, 0x02, 0xCB, 0x2C, 0xB2, 0xCB, 0x2C,
    0xB2, 0xC0
};

// scb_story: 64x30
int[242] scbsModeStory =
{
    0x40, 0x1E, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x03, 0xB9, 0x32, 0x80,
    0x00, 0x00, 0x00, 0x00, 0x02, 0x12, 0xAA, 0x80,
    0x00, 0x00, 0x00, 0x00, 0x03, 0x92, 0xB3, 0x80,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x92, 0xA8, 0x80,
    0x00, 0x00, 0x00, 0x00, 0x03, 0x91, 0x2B, 0x80,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x3F, 0xFF, 0xFF, 0xF8,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x3F, 0x9F, 0xC0, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x40, 0x60, 0x20, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x5F, 0x6D, 0xA0, 0x80,
    0x00, 0x00, 0x00, 0x00, 0x51, 0x60, 0x20, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x5F, 0x6E, 0xA0, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x40, 0x60, 0x20, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x5D, 0x6B, 0xA0, 0xC0,
    0x00, 0x00, 0x00, 0x00, 0x40, 0x60, 0x20, 0x03,
    0xF0, 0x00, 0x00, 0x00, 0x5B, 0x6E, 0xA0, 0x1B,
    0xF0, 0x40, 0x00, 0x00, 0x40, 0x60, 0x23, 0x02,
    0x1F, 0xCC, 0x00, 0x00, 0x57, 0x6F, 0x20, 0x03,
    0x7F, 0x00, 0x00, 0x00, 0x40, 0x60, 0x20, 0x03,
    0xF0, 0x00, 0x00, 0x00, 0x3F, 0x9F, 0xC0, 0x03,
    0xF0, 0x00, 0x00, 0x00, 0x00, 0x60, 0x00, 0x03,
    0xF0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03,
    0x30, 0x00, 0x1F, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xF8, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x08, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x08, 0x10, 0xC3, 0x0C, 0x30, 0xC3, 0x0C,
    0x30, 0xC8, 0x0D, 0x34, 0xD3, 0x4D, 0x34, 0xD3,
    0x4D, 0x30, 0x02, 0xCB, 0x2C, 0xB2, 0xCB, 0x2C,
    0xB2, 0xC0
};

// 5 real maps - each encoded as a real bitmap (width tiles, height tiles,
// then packed rows, MSB-first) reused directly by both `gbDrawBitmap()`
// (the map-select preview) and `gbGetBitmapPixel()` (real tile collision) -
// see this file's own header comment.
int[22] scbsMap0 = {
    16, 10,
    0xFE, 0x7F, 0x80, 0x1, 0x80, 0x1, 0x8F, 0xF1, 0x80, 0x1, 0x80, 0x1, 0xFC, 0x3F, 0x80, 0x1,
    0x80, 0x1, 0x9F, 0xF9,
};

int[50] scbsMap1 = {
    24, 16,
    0xFF, 0xE7, 0xFF, 0x80, 0x0, 0x1, 0x80, 0x0, 0x1, 0x80, 0x0, 0x1, 0x80, 0x0, 0x1, 0x83,
    0xFF, 0xC1, 0x80, 0x0, 0x1, 0x80, 0x0, 0x1, 0xFC, 0x0, 0x3F, 0x80, 0x0, 0x1, 0x80, 0x0,
    0x1, 0x87, 0xFF, 0xE1, 0x80, 0x0, 0x1, 0x80, 0x0, 0x1, 0xF8, 0x0, 0x1F, 0xFF, 0xC3, 0xFF,
};

int[50] scbsMap2 = {
    24, 16,
    0xFF, 0xE7, 0xFF, 0x80, 0x0, 0x1, 0x80, 0x0, 0x1, 0x80, 0x3C, 0x1, 0x80, 0x0, 0x1, 0x80,
    0x0, 0x1, 0x87, 0xE7, 0xE1, 0x80, 0x0, 0x1, 0x80, 0x0, 0x1, 0xFC, 0x18, 0x3F, 0xE0, 0x0,
    0x7, 0xE0, 0x0, 0x7, 0xE0, 0xFF, 0x7, 0xE0, 0x0, 0x7, 0xE0, 0x0, 0x7, 0xFF, 0xC3, 0xFF,
};

int[50] scbsMap3 = {
    24, 16,
    0xFF, 0xE7, 0xFF, 0x80, 0x0, 0x1, 0x80, 0x0, 0x1, 0x80, 0x0, 0x1, 0x83, 0xFF, 0xC1, 0x80,
    0x0, 0x1, 0x80, 0x0, 0x1, 0xF8, 0x0, 0x1F, 0x80, 0x0, 0x1, 0x80, 0x0, 0x1, 0x8F, 0xC3,
    0xF1, 0x80, 0x0, 0x1, 0x80, 0x0, 0x1, 0xF0, 0x3C, 0xF, 0xF3, 0xFF, 0xCF, 0xF3, 0xFF, 0xCF,
};

int[98] scbsMap4 = {
    32, 24,
    0xFF, 0xFE, 0x7F, 0xFF, 0x80, 0x0, 0x0, 0x1, 0x80, 0x0, 0x0, 0x1, 0xF0, 0x7, 0xE0, 0xF,
    0x80, 0x0, 0x0, 0x1, 0x80, 0x0, 0x0, 0x1, 0x87, 0xFC, 0x3F, 0xE1, 0x80, 0x0, 0x0, 0x1,
    0x80, 0x0, 0x0, 0x1, 0xF0, 0x3, 0xC0, 0x1, 0x80, 0x3, 0xC1, 0xFF, 0x80, 0x3, 0xC0, 0x3F,
    0x8F, 0xFF, 0xC0, 0x3, 0x80, 0x3, 0xFC, 0x3, 0x80, 0x0, 0x0, 0x3, 0xFC, 0x0, 0x0, 0x3,
    0xC0, 0xC, 0x1, 0xFF, 0xC0, 0xC, 0x0, 0x3F, 0xCF, 0xFC, 0x3, 0xF, 0xC0, 0xF, 0x3, 0xF,
    0xC0, 0x3, 0xC0, 0xF, 0xFC, 0x0, 0x0, 0xF, 0xFF, 0xC0, 0x0, 0xF, 0xFF, 0xFF, 0xFF, 0xF,
};

int*[SCBS_NUM_MAPS] scbsMaps = { scbsMap0, scbsMap1, scbsMap2, scbsMap3, scbsMap4 };

// real tile textures (8x6 each)
int[8] scbsBricks = { 8, 6, 0xFC, 0x24, 0xFC, 0x90, 0xFC, 0x48 };
int[8] scbsGrass = { 8, 6, 0xFC, 0x0, 0x0, 0x18, 0xA4, 0x58 };
int[8] scbsGrassEdge = { 8, 6, 0x7C, 0x80, 0x80, 0x80, 0x94, 0xE8 };
int[8] scbsBeam = { 8, 6, 0xFC, 0x10, 0x28, 0x44, 0x80, 0xFC };
int[8] scbsRoundPlatform = { 8, 6, 0xFC, 0x0, 0x0, 0x0, 0xFC, 0xFC };
int[8] scbsRoundPlatformEdge = { 8, 6, 0x7C, 0xE0, 0xC0, 0xE0, 0xFC, 0x7C };
int[8] scbsBlackWall = { 8, 6, 0xF4, 0xFC, 0xBC, 0xF8, 0xFC, 0xDC };
int[8] scbsCrateBitmap = { 8, 6, 0xFC, 0x84, 0xEC, 0xDC, 0x84, 0xFC };

// real weapon sprites (24 wide) and their real white-highlight overlays
int[20] scbsClubBitmap = { 24, 6, 0xC0, 0x0, 0x0, 0xF0, 0x0, 0x0, 0x7C, 0x0, 0x0, 0x1F, 0x0, 0x0, 0x7, 0x80, 0x0, 0x1, 0x80, 0x0 };
int[11] scbsPistolBitmap = { 24, 3, 0x0, 0x0, 0xF0, 0x0, 0x1, 0xE0, 0x0, 0x1, 0x0 };
int[17] scbsLaserBitmap = { 24, 5, 0x0, 0x0, 0x80, 0x0, 0xFF, 0xA8, 0x0, 0xFF, 0xFC, 0x0, 0xFF, 0xA8, 0x0, 0x0, 0x80 };
int[14] scbsRevolverBitmap = { 24, 4, 0x0, 0x0, 0x80, 0x0, 0x0, 0xF8, 0x0, 0x1, 0xF8, 0x0, 0x1, 0xC0 };
int[14] scbsRifleBitmap = { 24, 4, 0x0, 0x0, 0x4, 0x0, 0x21, 0xFC, 0x0, 0x37, 0xF0, 0x0, 0x31, 0x0 };
int[11] scbsRifleWhiteBitmap = { 24, 3, 0x0, 0x0, 0x0, 0x0, 0x1E, 0x0, 0x0, 0x8, 0x0 };
int[14] scbsSniperBitmap = { 24, 4, 0x0, 0x0, 0xC0, 0x0, 0xFF, 0xFE, 0x0, 0xFF, 0xE0, 0x0, 0xE3, 0x0 };
int[14] scbsShotgunBitmap = { 24, 4, 0x0, 0x0, 0x10, 0x0, 0x63, 0xF0, 0x0, 0x7F, 0xF0, 0x0, 0x7C, 0x0 };
int[8] scbsShotgunWhiteBitmap = { 24, 2, 0x0, 0x0, 0x0, 0x0, 0x1C, 0x0 };
int[17] scbsMachinegunBitmap = { 24, 5, 0x0, 0x0, 0x10, 0x0, 0xC7, 0xF0, 0x0, 0xC7, 0xF0, 0x0, 0xC7, 0xC0, 0x0, 0xFC, 0x0 };
int[14] scbsMachinegunWhiteBitmap = { 24, 4, 0x0, 0x0, 0x0, 0x0, 0x38, 0x0, 0x0, 0x38, 0x0, 0x0, 0x38, 0x0 };
int[17] scbsDiskBitmap = { 24, 5, 0x0, 0x0, 0x20, 0x0, 0xFF, 0xF0, 0x0, 0xFC, 0x0, 0x0, 0xFC, 0x0, 0x0, 0xFF, 0xF0 };
int[17] scbsRocketBitmap = { 24, 5, 0x1, 0x0, 0x20, 0x1, 0xFF, 0xE0, 0x1, 0xD7, 0xE0, 0x1, 0xFF, 0xE0, 0x1, 0x0, 0x20 };
int[14] scbsGrenadeBitmap = { 24, 4, 0x0, 0xC7, 0xE0, 0x0, 0xFF, 0xE0, 0x0, 0xC7, 0xE0, 0x0, 0x7C, 0x0 };
int[11] scbsGrenadeWhiteBitmap = { 24, 3, 0x0, 0x38, 0x0, 0x0, 0x0, 0x0, 0x0, 0x38, 0x0 };

// real player sprite (5 walk-cycle frames, 8x9 each)
int[5][11] scbsPlayerBitmap = {
    { 8, 9, 0x0, 0x7E, 0x7E, 0x7E, 0x7E, 0x7E, 0x7E, 0x7E, 0x66 },
    { 8, 9, 0x7E, 0x7E, 0x7E, 0x7E, 0x7E, 0x7E, 0x3C, 0x3C, 0xC },
    { 8, 9, 0x7E, 0x7E, 0x7E, 0x7E, 0x7E, 0x7E, 0x38, 0x38, 0x18 },
    { 8, 9, 0x0, 0x7E, 0x7E, 0x7E, 0x7E, 0x7E, 0x1C, 0x1C, 0x18 },
    { 8, 9, 0x0, 0x7E, 0x7E, 0x7E, 0x7E, 0x7E, 0x7E, 0x3C, 0x30 },
};

// real small-enemy sprite (5 frames, 8x8 each)
int[5][10] scbsSmallEnemyBitmap = {
    { 8, 8, 0x0, 0x7E, 0x6C, 0x6C, 0x7E, 0x7E, 0x7E, 0x66 },
    { 8, 8, 0x7E, 0x6C, 0x6C, 0x7E, 0x7E, 0x3C, 0x3C, 0xC },
    { 8, 8, 0x7E, 0x6C, 0x6C, 0x7E, 0x7E, 0x38, 0x38, 0x18 },
    { 8, 8, 0x7E, 0x6C, 0x6C, 0x7E, 0x7E, 0x1C, 0x1C, 0x18 },
    { 8, 8, 0x0, 0x7E, 0x6C, 0x6C, 0x7E, 0x7E, 0x3C, 0x30 },
};

// real big-enemy sprite (6 frames, 16x10 each)
int[6][22] scbsBigEnemyBitmap = {
    { 16, 10, 0x0, 0x0, 0x3F, 0xE0, 0x3F, 0xE0, 0x3D, 0xA0, 0x3D, 0xA0, 0x3F, 0xE0, 0x3F, 0xE0, 0x3F, 0xE0, 0x38, 0xE0, 0x38, 0xE0 },
    { 16, 10, 0x0, 0x0, 0x3F, 0xE0, 0x3F, 0xE0, 0x3D, 0xA0, 0x3D, 0xA0, 0x3F, 0xE0, 0x3F, 0xE0, 0x3F, 0xE0, 0x3F, 0xE0, 0x1, 0xC0 },
    { 16, 10, 0x3F, 0xE0, 0x3F, 0xE0, 0x3D, 0xA0, 0x3D, 0xA0, 0x3F, 0xE0, 0x3F, 0xE0, 0x3F, 0xE0, 0x3F, 0xE0, 0xF, 0x0, 0x7, 0x0 },
    { 16, 10, 0x3F, 0xE0, 0x3F, 0xE0, 0x3D, 0xA0, 0x3D, 0xA0, 0x3F, 0xE0, 0x3F, 0xE0, 0x3F, 0xE0, 0x3F, 0xE0, 0x7, 0x80, 0x7, 0x0 },
    { 16, 10, 0x3F, 0xE0, 0x3F, 0xE0, 0x3D, 0xA0, 0x3D, 0xA0, 0x3F, 0xE0, 0x3F, 0xE0, 0x3F, 0xE0, 0x3F, 0xE0, 0xF, 0x80, 0xE, 0x0 },
    { 16, 10, 0x3F, 0xE0, 0x3F, 0xE0, 0x3D, 0xA0, 0x3D, 0xA0, 0x3F, 0xE0, 0x3F, 0xE0, 0x3F, 0xE0, 0x3F, 0xE0, 0x1D, 0xC0, 0x1C, 0x0 },
};

// Kept although nothing reads it: its only reader upstream was the
// "Next unlock:" progress line, which this fork comments out. The table
// itself is still present in the fork's own source too.
int[SCBS_NUM_THRESHOLDS] scbsScoreThresholds = {
    SCBS_SCORETHRESHOLD_1, SCBS_SCORETHRESHOLD_2, SCBS_SCORETHRESHOLD_3, SCBS_SCORETHRESHOLD_4, SCBS_SCORETHRESHOLD_5
};

// real upstream `"\21 Select map \20"` - \21/\20 (octal, ASCII 17/16) are
// real Gamebuino icon glyphs (the same low-ASCII custom-icon range other
// ported games' own restored text already uses) - built as an explicit
// int array since a quoted string literal can't hold a non-printable
// low-ASCII code.
// Real MainMenu()'s own centred label: two D-pad arrow icon glyphs (ASCII 17 and 16) around the word Select.
int[15] scbsSelectModeText = { 17, 32, 32, 32, 83, 101, 108, 101, 99, 116, 32, 32, 32, 16, 0 };

int[15] scbsSelectMapText = { 17, 32, 83, 101, 108, 101, 99, 116, 32, 109, 97, 112, 32, 16, 0 };

// -----------------------------------------------------------------------------
// Global state - see this file's own header comment on why every real
// single-instance upstream class (World/Player/Weapon/Crate) is flattened
// to plain globals here, while Bullet/Enemy (real upstream arrays) become
// struct arrays.
// -----------------------------------------------------------------------------

int scbsCameraX, scbsCameraY, scbsShakeTimeLeft, scbsShakeAmplitude;

int scbsPopupTimeLeft;
int* scbsPopupText;

// World - the currently selected/playing map (also the currently
// highlighted map while on the map-select screen - matches real upstream's
// own single `mapNumber` field serving both purposes).
int* scbsTiles;
int* scbsWallBitmap;
int* scbsPlatformBitmap;
int* scbsEdgeBitmap;
int scbsHasEdge;
int scbsWorldMapNumber;
int[SCBS_NUM_MAPS] scbsScore;
int scbsUnlockedWeapons;
int scbsUnlockedMaps;
int scbsChooseMapIndex;

struct ScbsBullet
{
    int x, y, vx, vy, dir;
    int subtype;
    int timeLeft;
};
ScbsBullet[SCBS_NUM_BULLETS] scbsBullets;

int scbsWeaponSubtype;
int scbsWeaponCooldown;

int scbsPlayerX, scbsPlayerY, scbsPlayerVx, scbsPlayerVy, scbsPlayerDir;
int scbsPlayerScore;
int scbsPlayerDead;
int scbsPlayerJumping;
int scbsPlayerDoubleJumped;

struct ScbsEnemy
{
    int x, y, vx, vy, dir;
    int subtype;
    int active;
    int health;
};
ScbsEnemy[SCBS_NUM_ENEMIES] scbsEnemies;
int scbsNextSpawnCount;

int scbsCrateX, scbsCrateY, scbsCrateVx, scbsCrateVy, scbsCrateDir;

int scbsGameOverCount;

enum ScbsState
{
    SCBS_STATE_TITLE = 0,
    SCBS_STATE_CHOOSEMAP = 1,
    SCBS_STATE_PLAY = 2,
    SCBS_STATE_PAUSED = 3,
    SCBS_STATE_GAMEOVER = 4,
    SCBS_STATE_MAINMENU = 5,
    SCBS_STATE_SMENU = 6,
    SCBS_STATE_GUNMENU = 7,
    SCBS_STATE_SYSINFO = 8
};
int scbsState;

// Stone Edition's own additions: the mode-select cursor, the weapon
// cheat toggle, and the two hand-rolled menu cursors that stand in for
// real gb.menu() (this shim has no equivalent - the same hand-rolled
// treatment gameConduit.c established).
int scbsMenuNumber;
int scbsCheatGun;
int scbsUnlocMap;
int scbsSMenuIndex;
int scbsGunMenuIndex;


// -----------------------------------------------------------------------------
// Small helpers - Arduino macro stand-ins this dialect has no equivalent
// for (no ternary operator, no macros with side-effect-bearing arguments).
// -----------------------------------------------------------------------------

int scbsConstrain( int amt, int lo, int hi )
{
    if( amt < lo ) return lo;
    if( amt > hi ) return hi;
    return amt;
}

// real Arduino `map(value, fromLo, fromHi, toLo, toHi)`.
int scbsMap( int value, int fromLo, int fromHi, int toLo, int toHi )
{
    return ( value - fromLo ) * ( toHi - toLo ) / ( fromHi - fromLo ) + toLo;
}

// Direct port of real Arduino core's own `random(long howsmall, long
// howbig)` (`WMath.cpp`), INCLUDING its own real `howsmall >= howbig`
// short-circuit (returns `howsmall` unmodified, no random draw at all) -
// see this file's own header comment (quirks 6/7) for two real, verified
// upstream call sites whose own real behavior depends on this exact
// short-circuit, not a generic "clamp then randomize" rewrite.
int scbsArduinoRandom( int minVal, int maxVal )
{
    if( minVal >= maxVal )
      return minVal;
    return minVal + arand( maxVal - minVal );
}

int scbsToScreenX( int x )
{
    return x / SCBS_SCALE - scbsCameraX;
}

int scbsToScreenY( int y )
{
    return y / SCBS_SCALE - scbsCameraY;
}

// Direct port of real `Box::isOffScreen()`.
int scbsIsOffScreen( int x, int y, int w, int h )
{
    if( ( scbsToScreenX( x ) + scbsToScreenX( x + w ) ) < 0 ) return 1;
    if( scbsToScreenX( x ) > LCDWIDTH ) return 1;
    if( ( scbsToScreenY( y ) + scbsToScreenY( y + h ) ) < 0 ) return 1;
    if( scbsToScreenY( y ) > LCDHEIGHT ) return 1;
    return 0;
}

// Direct port of real `Box::draw()` (the generic fillRect fallback - only
// `Bullet` ever actually uses this one; `Player`/`Enemy`/`Crate` all draw
// their own real bitmap instead).
void scbsBoxDrawRect( int x, int y, int w, int h )
{
    if( scbsIsOffScreen( x, y, w, h ) ) return;
    gbFillRect( scbsToScreenX( x ), scbsToScreenY( y ), w / SCBS_SCALE, h / SCBS_SCALE );
}

// Direct port of real `World::tileAtPosition()`.
int scbsTileAtPosition( int x, int y )
{
    int tileX = ( x - SCBS_SCALE / 2 ) / SCBS_SPRITE_SIZE / SCBS_SCALE;
    int tileY = ( y - SCBS_SCALE / 2 ) / SCBS_SPRITE_SIZE / SCBS_SCALE;
    int w = scbsTiles[ 0 ];
    int h = scbsTiles[ 1 ];

    if( tileX < 0 || tileX >= w || tileY < 0 || tileY >= h )
      return 0;

    if( gbGetBitmapPixel( scbsTiles, tileX, tileY ) )
      return 1;
    return 0;
}

// Direct port of real `World::solidCollisionAtPosition()`.
int scbsWorldSolidCollision( int x, int y, int w, int h )
{
    if( scbsTileAtPosition( x, y + h ) ) return 1;
    if( scbsTileAtPosition( x + w, y + h ) ) return 1;
    if( scbsTileAtPosition( x + w, y ) ) return 1;
    if( scbsTileAtPosition( x, y ) ) return 1;

    if( w > SCBS_SPRITE_SIZE * SCBS_SCALE )
    {
        if( scbsTileAtPosition( x + w / 2, y ) ) return 1;
        if( scbsTileAtPosition( x + w / 2, y + h ) ) return 1;
    }
    if( h > SCBS_SPRITE_SIZE * SCBS_SCALE )
    {
        if( scbsTileAtPosition( x, y + h / 2 ) ) return 1;
        if( scbsTileAtPosition( x + w, y + h / 2 ) ) return 1;
    }
    return 0;
}

// Direct ports of real `World::getWidth()`/`getHeight()`.
int scbsWorldGetWidth()
{
    return SCBS_SPRITE_SIZE * scbsTiles[ 0 ] * SCBS_SCALE;
}

int scbsWorldGetHeight()
{
    return SCBS_SPRITE_SIZE * scbsTiles[ 1 ] * SCBS_SCALE;
}

// Direct port of real `World::addScore()`.
int scbsWorldAddScore( int newScore )
{
    if( newScore > scbsScore[ scbsWorldMapNumber ] )
    {
        scbsScore[ scbsWorldMapNumber ] = newScore;
        return 1;
    }
    return 0;
}

// -----------------------------------------------------------------------------
// EEPROM - direct port of real `loadEEPROM()`/`saveEEPROM()`/
// `EEPROMreadInt()`/`EEPROMwriteInt()`/`cleanEEPROM()` onto this shim's own
// `eeprom_read_byte()`/`eeprom_write_byte()` - see this file's own header
// comment.
// -----------------------------------------------------------------------------

int scbsEepromReadInt( int addr )
{
    int value = eeprom_read_byte( addr + 1 ) & 0xFF;
    value = value + ( ( eeprom_read_byte( addr ) << 8 ) & 0xFF00 );
    return value;
}

void scbsEepromWriteInt( int addr, int value )
{
    eeprom_write_byte( addr + 1, value & 0xFF );
    eeprom_write_byte( addr, ( value >> 8 ) & 0xFF );
}

void scbsCleanEeprom()
{
    int i;
    for( i = 0; i < 1024; i++ )
      if( eeprom_read_byte( i ) )
        eeprom_write_byte( i, 0 );
}

void scbsLoadEeprom()
{
    if( scbsEepromReadInt( 0 ) != SCBS_EEPROM_TOKEN )
    {
        scbsCleanEeprom();
        scbsUnlockedWeapons = 0;
        scbsUnlockedMaps = 0;
        scbsWorldMapNumber = 0;
        int i;
        for( i = 0; i < SCBS_NUM_MAPS; i++ )
          scbsScore[ i ] = 0;
        return;
    }

    int i;
    for( i = 0; i < SCBS_NUM_MAPS; i++ )
      scbsScore[ i ] = scbsEepromReadInt( i * 2 + SCBS_EEPROM_SCORE_OFFSET );

    scbsUnlockedWeapons = eeprom_read_byte( SCBS_EEPROM_WEAPONS_OFFSET );
    scbsUnlockedMaps = eeprom_read_byte( SCBS_EEPROM_MAPS_OFFSET );
    scbsWorldMapNumber = scbsUnlockedMaps; // real upstream - select the last unlocked map by default
}

void scbsSaveEeprom()
{
    scbsEepromWriteInt( 0, SCBS_EEPROM_TOKEN );

    int i;
    for( i = 0; i < SCBS_NUM_MAPS; i++ )
      if( scbsEepromReadInt( i * 2 + SCBS_EEPROM_SCORE_OFFSET ) < scbsScore[ i ] )
        scbsEepromWriteInt( i * 2 + SCBS_EEPROM_SCORE_OFFSET, scbsScore[ i ] );

    if( eeprom_read_byte( SCBS_EEPROM_WEAPONS_OFFSET ) < scbsUnlockedWeapons )
      eeprom_write_byte( SCBS_EEPROM_WEAPONS_OFFSET, scbsUnlockedWeapons );
    if( eeprom_read_byte( SCBS_EEPROM_MAPS_OFFSET ) < scbsUnlockedMaps )
      eeprom_write_byte( SCBS_EEPROM_MAPS_OFFSET, scbsUnlockedMaps );
}

// -----------------------------------------------------------------------------
// Popup - see this file's own header comment on why this is a local
// reimplementation rather than the shared shim's own `gbPopup()`.
// -----------------------------------------------------------------------------

void scbsPrintCentered( int* text )
{
    gbCursorX = ( LCDWIDTH / 2 ) - ( strlen( text ) * gbFontSize * gbFontWidth / 2 );
    gbPrintString( text );
}

void scbsPopup( int* text, int duration )
{
    scbsPopupText = text;
    scbsPopupTimeLeft = duration + 12;
}

void scbsUpdatePopup()
{
    if( scbsPopupTimeLeft )
    {
        int yOffset = 0;
        if( scbsPopupTimeLeft < 12 )
          yOffset = scbsPopupTimeLeft - 12;

        int width = strlen( scbsPopupText ) * gbFontSize * gbFontWidth;
        gbFontSize = 1;
        gbSetColor( GB_BLACK );
        gbDrawRect( LCDWIDTH / 2 - width / 2 - 2, yOffset - 1, width + 2, gbFontHeight + 2 );
        gbSetColor( GB_WHITE );
        gbFillRect( LCDWIDTH / 2 - width / 2 - 1, yOffset - 1, width + 1, gbFontHeight + 1 );
        gbSetColor( GB_BLACK );
        gbCursorY = yOffset;
        scbsPrintCentered( scbsPopupText );
        scbsPopupTimeLeft = scbsPopupTimeLeft - 1;
    }
}

// -----------------------------------------------------------------------------
// Sound patterns - real upstream `PROGMEM uint16_t[]` pattern data, copied
// byte-for-byte (verified against the real source directly, not retyped
// from memory - each array's own element count was cross-checked against
// the real source line's own comma-separated value count). Played through
// gbPlayPattern() on the real channel each real call site below uses -
// `player_damage_sound`/`revolver_sound` are genuinely never referenced
// anywhere in real upstream (confirmed via grep), so, matching this file's
// own established treatment of `largeChecker`, they're correctly omitted.
// -----------------------------------------------------------------------------

int[ 3 ] scbsGrenadeSound = { 0x0045, 0x012C, 0x0000 };
int[ 7 ] scbsMachinegunSound = { 0x0045, 0x140, 0x8141, 0x7849, 0x788D, 0x52C, 0x0000 };
int[ 5 ] scbsRocketSound = { 0x8045, 0x8001, 0x8889, 0x3C5C, 0x0000 };
int[ 5 ] scbsBlastSound = { 0x0045, 0x7849, 0x784D, 0xA28, 0x0000 };
int[ 17 ] scbsPowerUpSound = { 0x0005, 0x140, 0x150, 0x15C, 0x170, 0x180, 0x16C, 0x154, 0x160, 0x174, 0x184, 0x14C, 0x15C, 0x168, 0x17C, 0x18C, 0x0000 };
int[ 3 ] scbsEnemyDeathSound = { 0x0045, 0x184, 0x0000 };
int[ 5 ] scbsJumpSound = { 0x0005, 0x7049, 0x884D, 0x354, 0x0000 };
int[ 5 ] scbsEnemyFeltSound = { 0x8005, 0x8001, 0x8849, 0xF20, 0x0000 };
int[ 4 ] scbsShotgunSound = { 0x0045, 0x7049, 0x334, 0x0000 };
int[ 5 ] scbsLaserSound = { 0x0005, 0x784D, 0x7849, 0x670, 0x0000 };
int[ 5 ] scbsClubSound = { 0x8005, 0x784D, 0x7849, 0x318, 0x0000 };

// -----------------------------------------------------------------------------
// Box physics - the one real shared step every entity below calls, see
// this file's own header comment (quirk 1) on the real Y-axis-gated-on-
// xBounce behavior this reproduces exactly.
// -----------------------------------------------------------------------------

int scbsBoxUpdate( int* x, int* y, int* vx, int* vy, int* dir, int width, int height, int gravity, int maxSpeed, int xFriction, int yFriction, int xBounce, int yBounce )
{
    *vy = *vy + gravity;
    *vx = ( *vx * ( 100 - xFriction ) ) / 100;
    *vy = ( *vy * ( 100 - yFriction ) ) / 100;
    *vx = scbsConstrain( *vx, -maxSpeed, maxSpeed );
    *vy = scbsConstrain( *vy, -maxSpeed, maxSpeed );

    int collided = 0;

    *x = *x + *vx;
    if( xBounce >= 0 )
    {
        int vxdir = -1;
        if( *vx > 0 ) vxdir = 1;

        if( scbsWorldSolidCollision( *x, *y, width, height ) )
        {
            collided = 1;
            while( scbsWorldSolidCollision( *x, *y, width, height ) )
              *x = *x - vxdir;
            *vx = -( *vx * xBounce ) / 100;
        }
    }

    *y = *y + *vy;
    if( xBounce >= 0 ) // real upstream gates the Y block on getXBounce() too - see header comment (quirk 1)
    {
        int vydir = -1;
        if( *vy > 0 ) vydir = 1;

        if( scbsWorldSolidCollision( *x, *y, width, height ) )
        {
            collided = 1;
            while( scbsWorldSolidCollision( *x, *y, width, height ) )
              *y = *y - vydir;
            *vy = -( *vy * yBounce ) / 100;
        }
    }

    if( *vx > 0 ) *dir = 1;
    if( *vx < 0 ) *dir = -1;

    return collided;
}

// -----------------------------------------------------------------------------
// Bullet - real per-subtype "virtual getters" as plain functions.
// -----------------------------------------------------------------------------

int scbsBulletGetWidth( int subtype, int vx, int timeLeft )
{
    if( subtype == SCBS_W_CLUB ) return 96;
    if( subtype == SCBS_W_PISTOL || subtype == SCBS_W_AKIMBO || subtype == SCBS_W_RIFLE || subtype == SCBS_W_SHOTGUN )
      return scbsConstrain( gbAbsInt( vx ), 8, 16 );
    if( subtype == SCBS_W_MACHINEGUN )
      return scbsConstrain( gbAbsInt( vx ), 8, 24 );
    if( subtype == SCBS_W_REVOLVER || subtype == SCBS_W_SNIPER )
      return scbsConstrain( gbAbsInt( vx ), 8, 32 );
    if( subtype == SCBS_W_DISK )
      return gbMin( timeLeft * 8, 48 );
    if( subtype == SCBS_W_LASER ) return 96;
    if( subtype == SCBS_W_GRENADE ) return 24;
    if( subtype == SCBS_W_ROCKET ) return 48;
    if( subtype == SCBS_W_MINE ) return 32;
    if( subtype == SCBS_W_EXPLOSION ) return 256;
    if( subtype == SCBS_W_SHELL ) return 16;
    return 32;
}

int scbsBulletGetHeight( int subtype, int vx, int timeLeft )
{
    if( subtype == SCBS_W_CLUB ) return 16;
    if( subtype == SCBS_W_REVOLVER ) return gbMax( scbsBulletGetWidth( subtype, vx, timeLeft ) / 2, 8 );
    if( subtype == SCBS_W_SNIPER || subtype == SCBS_W_SHELL || subtype == SCBS_W_LASER ) return 8;
    if( subtype == SCBS_W_DISK || subtype == SCBS_W_MINE ) return 16;
    if( subtype == SCBS_W_ROCKET ) return 24;
    return scbsBulletGetWidth( subtype, vx, timeLeft );
}

int scbsBulletGetGravity( int subtype, int vx )
{
    if( subtype == SCBS_W_GRENADE || subtype == SCBS_W_MINE || subtype == SCBS_W_SHELL ) return 5;
    if( subtype == SCBS_W_CLUB || subtype == SCBS_W_DISK || subtype == SCBS_W_LASER || subtype == SCBS_W_ROCKET || subtype == SCBS_W_EXPLOSION ) return 0;
    if( gbAbsInt( vx ) > 16 ) return 0;
    return 2;
}

int scbsBulletGetXFriction( int subtype )
{
    if( subtype == SCBS_W_SHOTGUN ) return 10;
    if( subtype == SCBS_W_CLUB || subtype == SCBS_W_DISK || subtype == SCBS_W_LASER ) return 0;
    if( subtype == SCBS_W_ROCKET ) return -20; // negative so it accelerates - matches real upstream comment
    if( subtype == SCBS_W_EXPLOSION ) return 100;
    return 5; // real Box::getXFriction() default
}

int scbsBulletGetXBounce( int subtype )
{
    if( subtype == SCBS_W_DISK ) return 100;
    if( subtype == SCBS_W_CLUB || subtype == SCBS_W_EXPLOSION || subtype == SCBS_W_LASER ) return -1; // don't collide the world
    if( subtype == SCBS_W_GRENADE || subtype == SCBS_W_SHELL ) return 80;
    if( subtype == SCBS_W_ROCKET ) return 0;
    return 30;
}

int scbsBulletGetYBounce( int subtype )
{
    if( subtype == SCBS_W_GRENADE || subtype == SCBS_W_SHELL )
      return scbsBulletGetXBounce( subtype );
    return 0;
}

// real `Bullet::getDamage()`'s own leading, label-less `return 1;` is
// unreachable dead code under real switch semantics - see this file's own
// header comment (quirk 5). This implements only the real reachable logic.
int scbsBulletGetDamage( int subtype )
{
    if( subtype == SCBS_W_REVOLVER || subtype == SCBS_W_CLUB ) return 2;
    if( subtype == SCBS_W_SNIPER || subtype == SCBS_W_DISK || subtype == SCBS_W_EXPLOSION || subtype == SCBS_W_LASER ) return 10;
    if( subtype == SCBS_W_GRENADE || subtype == SCBS_W_ROCKET || subtype == SCBS_W_MINE || subtype == SCBS_W_SHELL ) return 0;
    return 1;
}

int scbsBulletGetMaxTimeLeft( int subtype )
{
    if( subtype == SCBS_W_CLUB ) return 2;
    if( subtype == SCBS_W_SHELL ) return 20;
    if( subtype == SCBS_W_MINE || subtype == SCBS_W_DISK ) return 100;
    if( subtype == SCBS_W_EXPLOSION ) return 5;
    if( subtype == SCBS_W_GRENADE || subtype == SCBS_W_ROCKET ) return 40;
    return 25;
}

int scbsBulletExplodes( int subtype )
{
    if( subtype == SCBS_W_GRENADE || subtype == SCBS_W_ROCKET || subtype == SCBS_W_MINE ) return 1;
    return 0;
}

int scbsBulletDestroyOnWorldContact( int subtype )
{
    if( subtype == SCBS_W_ROCKET ) return 1;
    return 0;
}

int scbsBulletDestroyOnEnemyContact( int subtype )
{
    if( subtype == SCBS_W_CLUB || subtype == SCBS_W_DISK || subtype == SCBS_W_LASER || subtype == SCBS_W_EXPLOSION || subtype == SCBS_W_SHELL ) return 0;
    return 1;
}

int scbsBulletDamagePlayer( int subtype )
{
    if( subtype == SCBS_W_DISK ) return 1;
    return 0;
}

// Direct port of real `Bullet::update()`.
void scbsBulletUpdate( int i )
{
    if( scbsBullets[ i ].timeLeft == 0 ) return;

    int subtype = scbsBullets[ i ].subtype;
    int width = scbsBulletGetWidth( subtype, scbsBullets[ i ].vx, scbsBullets[ i ].timeLeft );
    int height = scbsBulletGetHeight( subtype, scbsBullets[ i ].vx, scbsBullets[ i ].timeLeft );
    int gravity = scbsBulletGetGravity( subtype, scbsBullets[ i ].vx );
    int xFriction = scbsBulletGetXFriction( subtype );
    int xBounce = scbsBulletGetXBounce( subtype );
    int yBounce = scbsBulletGetYBounce( subtype );

    int collided = scbsBoxUpdate( &scbsBullets[ i ].x, &scbsBullets[ i ].y, &scbsBullets[ i ].vx, &scbsBullets[ i ].vy, &scbsBullets[ i ].dir,
                                  width, height, gravity, 128, xFriction, 5, xBounce, yBounce );
    scbsBullets[ i ].timeLeft = scbsBullets[ i ].timeLeft - 1;

    if( scbsBulletDestroyOnWorldContact( subtype ) && collided == 1 )
      scbsBullets[ i ].timeLeft = 0;

    if( scbsBullets[ i ].timeLeft == 0 && scbsBulletExplodes( subtype ) )
    {
        scbsBullets[ i ].subtype = SCBS_W_EXPLOSION;
        // offset the explosion so it's centered - real upstream computes
        // getWidth()/getHeight() AFTER the subtype change above, so both
        // read the real W_EXPLOSION values (256/256), not the original
        // subtype's own.
        scbsBullets[ i ].x = scbsBullets[ i ].x - scbsBulletGetWidth( SCBS_W_EXPLOSION, scbsBullets[ i ].vx, scbsBullets[ i ].timeLeft ) / 2;
        scbsBullets[ i ].y = scbsBullets[ i ].y - scbsBulletGetHeight( SCBS_W_EXPLOSION, scbsBullets[ i ].vx, scbsBullets[ i ].timeLeft ) / 2;
        scbsBullets[ i ].timeLeft = 8;
        scbsShakeTimeLeft = 10;
        scbsShakeAmplitude = 2;
        gbPlayPattern( scbsBlastSound, 0 ); // real blast_sound, channel 0
    }
}

// Direct port of real `Bullet::draw()`.
void scbsBulletDraw( int i )
{
    if( scbsBullets[ i ].timeLeft == 0 ) return;
    if( scbsBullets[ i ].subtype == SCBS_W_LASER )
      gbSetColor( GB_INVERT );

    int width = scbsBulletGetWidth( scbsBullets[ i ].subtype, scbsBullets[ i ].vx, scbsBullets[ i ].timeLeft );
    int height = scbsBulletGetHeight( scbsBullets[ i ].subtype, scbsBullets[ i ].vx, scbsBullets[ i ].timeLeft );
    scbsBoxDrawRect( scbsBullets[ i ].x, scbsBullets[ i ].y, width, height );
}

// -----------------------------------------------------------------------------
// Weapon - always owned by the single player instance (see header comment
// on why no "shooter" reference is carried at all).
// -----------------------------------------------------------------------------

int scbsWeaponGetMaxCooldown( int subtype )
{
    if( subtype == SCBS_W_CLUB ) return 10;
    if( subtype == SCBS_W_PISTOL || subtype == SCBS_W_AKIMBO || subtype == SCBS_W_REVOLVER ) return 0;
    if( subtype == SCBS_W_SNIPER ) return 7;
    if( subtype == SCBS_W_SHOTGUN ) return 11;
    if( subtype == SCBS_W_RIFLE ) return 2;
    if( subtype == SCBS_W_MACHINEGUN ) return 1;
    if( subtype == SCBS_W_DISK ) return 19;
    if( subtype == SCBS_W_LASER ) return 30;
    if( subtype == SCBS_W_GRENADE || subtype == SCBS_W_ROCKET || subtype == SCBS_W_MINE ) return 19;
    return 5;
}

int scbsWeaponIsAutomatic( int subtype )
{
    if( subtype == SCBS_W_RIFLE || subtype == SCBS_W_MACHINEGUN ) return 1;
    return 0;
}

void scbsWeaponInit()
{
    scbsWeaponCooldown = 0;
    int i;
    for( i = 0; i < SCBS_NUM_BULLETS; i++ )
      scbsBullets[ i ].timeLeft = 0;
}

// Direct port of real `Weapon::addBullet()` - see this file's own header
// comment (quirks 2/3/4) for the three real fallthrough behaviors
// reproduced exactly below.
void scbsWeaponAddBullet( int x, int y, int dir, int subtype )
{
    int i;
    for( i = 0; i < SCBS_NUM_BULLETS; i++ )
    {
        if( scbsBullets[ i ].timeLeft != 0 ) continue;

        scbsBullets[ i ].subtype = subtype;
        scbsBullets[ i ].timeLeft = scbsBulletGetMaxTimeLeft( subtype );

        // screen shake - quirk 2: SNIPER/REVOLVER's own heavier shake is
        // always overwritten by MACHINEGUN's own weaker values.
        if( subtype == SCBS_W_SNIPER || subtype == SCBS_W_REVOLVER || subtype == SCBS_W_MACHINEGUN )
        {
            scbsShakeTimeLeft = 2;
            scbsShakeAmplitude = 1;
        }

        // initial bullet speed - quirk 3: MACHINEGUN's own vx/vy is
        // computed (firing the real player-recoil side effect below), then
        // discarded by falling into SHOTGUN's own fresh recompute.
        if( subtype == SCBS_W_CLUB )
        {
            scbsBullets[ i ].vx = dir * 32;
            scbsBullets[ i ].vy = scbsPlayerVy;
        }
        else if( subtype == SCBS_W_MACHINEGUN || subtype == SCBS_W_SHOTGUN )
        {
            if( subtype == SCBS_W_MACHINEGUN )
              scbsPlayerVx = scbsPlayerVx - scbsPlayerDir * 32; // player recoil
            scbsBullets[ i ].vx = ( dir * 48 ) + scbsArduinoRandom( -8, 9 );
            scbsBullets[ i ].vy = scbsArduinoRandom( -10, 11 );
        }
        else if( subtype == SCBS_W_DISK )
        {
            scbsBullets[ i ].vx = dir * 26;
            scbsBullets[ i ].vy = 0;
        }
        else if( subtype == SCBS_W_LASER )
        {
            scbsBullets[ i ].vx = dir * 50;
            scbsBullets[ i ].vy = 0;
        }
        else if( subtype == SCBS_W_GRENADE )
        {
            scbsBullets[ i ].vx = ( dir * 32 ) + scbsPlayerVx / 2;
            scbsBullets[ i ].vy = -32 + scbsPlayerVy / 2;
        }
        else if( subtype == SCBS_W_ROCKET )
        {
            scbsBullets[ i ].vx = dir * 16;
            scbsBullets[ i ].vy = 0;
        }
        else if( subtype == SCBS_W_MINE )
        {
            scbsBullets[ i ].vx = 0;
            scbsBullets[ i ].vy = 0;
        }
        else if( subtype == SCBS_W_SHELL )
        {
            scbsBullets[ i ].vx = -dir * scbsArduinoRandom( 16, 24 );
            scbsBullets[ i ].vy = scbsPlayerVy - scbsArduinoRandom( 16, 24 );
        }
        else
        {
            scbsBullets[ i ].vx = ( dir * 64 ) + scbsArduinoRandom( -8, 9 );
            scbsBullets[ i ].vy = scbsArduinoRandom( 0, 11 ) - 5;
        }

        // vertical offset
        if( subtype == SCBS_W_SHOTGUN || subtype == SCBS_W_LASER || subtype == SCBS_W_DISK )
          y = y + 32;
        else if( subtype == SCBS_W_ROCKET )
          y = y + 16;
        else
          y = y + 24;

        // horizontal offset - quirk 4: SHELL gets BOTH its own -16 offset
        // and the ROCKET/CLUB -dir*32 offset, applied on top of it.
        if( subtype == SCBS_W_SHELL )
        {
            x = x - 16;
            x = x - dir * 32;
        }
        else if( subtype == SCBS_W_ROCKET || subtype == SCBS_W_CLUB )
        {
            x = x - dir * 32;
        }
        else if( subtype == SCBS_W_MINE )
        {
            // no offset - matches real upstream's own empty case
        }
        else
        {
            x = x + dir * 46;
        }

        if( dir > 0 )
          x = x + 48; // shooter->getWidth() - the player's own real constant width
        else
          x = x - scbsBulletGetWidth( subtype, scbsBullets[ i ].vx, scbsBullets[ i ].timeLeft );

        x = x + scbsPlayerVx / 2;
        scbsBullets[ i ].x = x;
        scbsBullets[ i ].y = y;

        return;
    }
}

// Direct port of real `Weapon::shoot()`.
void scbsWeaponShoot()
{
    scbsWeaponCooldown = scbsWeaponGetMaxCooldown( scbsWeaponSubtype );
    scbsWeaponAddBullet( scbsPlayerX, scbsPlayerY, scbsPlayerDir, scbsWeaponSubtype );

    if( scbsWeaponSubtype == SCBS_W_SHOTGUN )
    {
        scbsWeaponAddBullet( scbsPlayerX, scbsPlayerY, scbsPlayerDir, scbsWeaponSubtype );
        scbsWeaponAddBullet( scbsPlayerX, scbsPlayerY, scbsPlayerDir, scbsWeaponSubtype );
        scbsWeaponAddBullet( scbsPlayerX, scbsPlayerY, scbsPlayerDir, scbsWeaponSubtype );
        scbsWeaponAddBullet( scbsPlayerX, scbsPlayerY, scbsPlayerDir, scbsWeaponSubtype );
    }
    if( scbsWeaponSubtype == SCBS_W_AKIMBO )
      scbsWeaponAddBullet( scbsPlayerX, scbsPlayerY, -scbsPlayerDir, scbsWeaponSubtype );

    if( scbsWeaponSubtype == SCBS_W_RIFLE || scbsWeaponSubtype == SCBS_W_SNIPER || scbsWeaponSubtype == SCBS_W_SHOTGUN )
      scbsWeaponAddBullet( scbsPlayerX, scbsPlayerY, scbsPlayerDir, SCBS_W_SHELL );

    if( scbsWeaponSubtype == SCBS_W_ROCKET )
      gbPlayPattern( scbsRocketSound, 0 );
    else if( scbsWeaponSubtype == SCBS_W_REVOLVER || scbsWeaponSubtype == SCBS_W_MACHINEGUN || scbsWeaponSubtype == SCBS_W_SNIPER )
      gbPlayPattern( scbsMachinegunSound, 0 );
    else if( scbsWeaponSubtype == SCBS_W_GRENADE || scbsWeaponSubtype == SCBS_W_DISK )
      gbPlayPattern( scbsGrenadeSound, 0 );
    else if( scbsWeaponSubtype == SCBS_W_SHOTGUN )
      gbPlayPattern( scbsShotgunSound, 0 );
    else if( scbsWeaponSubtype == SCBS_W_MINE )
    {
        // no sound - matches real upstream's own empty case
    }
    else if( scbsWeaponSubtype == SCBS_W_PISTOL || scbsWeaponSubtype == SCBS_W_AKIMBO || scbsWeaponSubtype == SCBS_W_RIFLE )
      gbPlayTick();
    else if( scbsWeaponSubtype == SCBS_W_LASER )
      gbPlayPattern( scbsLaserSound, 0 );
    else if( scbsWeaponSubtype == SCBS_W_CLUB )
      gbPlayPattern( scbsClubSound, 0 );
}

void scbsWeaponUpdate()
{
    int i;
    for( i = 0; i < SCBS_NUM_BULLETS; i++ )
      scbsBulletUpdate( i );

    if( scbsWeaponCooldown > 0 )
      scbsWeaponCooldown = scbsWeaponCooldown - 1;
    else
    {
        if( scbsWeaponIsAutomatic( scbsWeaponSubtype ) )
        {
            if( gbRepeat( BTN_A, 1 ) ) scbsWeaponShoot();
        }
        else
        {
            if( gbPressed( BTN_A ) ) scbsWeaponShoot();
        }
    }
}

// Direct port of real `Weapon::draw()`.
void scbsWeaponDraw()
{
    int bx = scbsToScreenX( scbsPlayerX ) - 9;
    int by = scbsToScreenY( scbsPlayerY );
    int flip = 0; // NOFLIP
    if( scbsPlayerDir <= 0 ) flip = 1; // FLIPH

    int* bitmap = scbsClubBitmap;      // harmless placeholder - only read when hasBitmap is true
    int* bitmapWhite = scbsClubBitmap; // harmless placeholder - only read when hasBitmapWhite is true
    int hasBitmap = 0;
    int hasBitmapWhite = 0;

    if( scbsWeaponSubtype == SCBS_W_CLUB )
    {
        if( scbsWeaponCooldown > 8 )
        {
            hasBitmap = 0; // don't draw the club when already in use
        }
        else
        {
            bitmap = scbsClubBitmap;
            hasBitmap = 1;
            bx = bx + scbsPlayerDir * scbsWeaponCooldown / 2; // sliding back the club
        }
        by = by - 2;
    }
    else if( scbsWeaponSubtype == SCBS_W_PISTOL || scbsWeaponSubtype == SCBS_W_AKIMBO )
    {
        bitmap = scbsPistolBitmap; hasBitmap = 1;
        by = by + 3;
    }
    else if( scbsWeaponSubtype == SCBS_W_REVOLVER )
    {
        bitmap = scbsRevolverBitmap; hasBitmap = 1;
        by = by + 2;
    }
    else if( scbsWeaponSubtype == SCBS_W_SNIPER )
    {
        bitmap = scbsSniperBitmap; hasBitmap = 1;
        bitmapWhite = scbsShotgunWhiteBitmap; hasBitmapWhite = 1;
        by = by + 2;
        if( scbsWeaponCooldown > 4 ) bx = bx - scbsPlayerDir;
    }
    else if( scbsWeaponSubtype == SCBS_W_RIFLE )
    {
        bitmap = scbsRifleBitmap; hasBitmap = 1;
        bitmapWhite = scbsRifleWhiteBitmap; hasBitmapWhite = 1;
        by = by + 2;
        bx = bx - scbsPlayerDir * scbsWeaponCooldown;
    }
    else if( scbsWeaponSubtype == SCBS_W_SHOTGUN )
    {
        bitmap = scbsShotgunBitmap; hasBitmap = 1;
        bitmapWhite = scbsShotgunWhiteBitmap; hasBitmapWhite = 1;
        by = by + 3;
        bx = bx - scbsPlayerDir * scbsWeaponCooldown / 4;
    }
    else if( scbsWeaponSubtype == SCBS_W_MACHINEGUN )
    {
        bitmap = scbsMachinegunBitmap; hasBitmap = 1;
        bitmapWhite = scbsMachinegunWhiteBitmap; hasBitmapWhite = 1;
        by = by + 3;
    }
    else if( scbsWeaponSubtype == SCBS_W_DISK )
    {
        bitmap = scbsDiskBitmap; hasBitmap = 1;
        by = by + 2;
    }
    else if( scbsWeaponSubtype == SCBS_W_LASER )
    {
        bitmap = scbsLaserBitmap; hasBitmap = 1;
        by = by + 2;
    }
    else if( scbsWeaponSubtype == SCBS_W_GRENADE )
    {
        bitmap = scbsGrenadeBitmap; hasBitmap = 1;
        bitmapWhite = scbsGrenadeWhiteBitmap; hasBitmapWhite = 1;
        by = by + 4;
    }
    else if( scbsWeaponSubtype == SCBS_W_ROCKET )
    {
        bitmap = scbsRocketBitmap; hasBitmap = 1;
        by = by + 1;
    }
    // default (MINE): no weapon sprite drawn - matches real upstream's own default case

    if( hasBitmap )
      gbDrawBitmapRotated( bx, by, bitmap, 0, flip ); // NOROT
    if( hasBitmapWhite )
    {
        gbSetColor( GB_WHITE );
        gbDrawBitmapRotated( bx, by, bitmapWhite, 0, flip );
        gbSetColor( GB_BLACK );
    }

    if( scbsWeaponSubtype == SCBS_W_AKIMBO ) // draw the symmetric pistol in the akimbo case
    {
        int mirrorFlip = ( flip + 1 ) % 2;
        if( hasBitmap )
          gbDrawBitmapRotated( bx, by, bitmap, 0, mirrorFlip );
        if( hasBitmapWhite )
        {
            gbSetColor( GB_WHITE );
            gbDrawBitmapRotated( bx, by, bitmapWhite, 0, mirrorFlip );
            gbSetColor( GB_BLACK );
        }
    }

    if( scbsWeaponSubtype == SCBS_W_LASER ) // reloading line on the laser
    {
        gbSetColor( GB_WHITE );
        gbDrawFastHLine( scbsToScreenX( scbsPlayerX ), scbsToScreenY( scbsPlayerY ) + 4, 6 - scbsWeaponCooldown / 5 );
        gbSetColor( GB_BLACK );
    }

    if( scbsWeaponSubtype == SCBS_W_DISK || scbsWeaponSubtype == SCBS_W_MINE ) // refill animation
    {
        if( scbsPlayerDir > 0 )
          gbFillRect( scbsToScreenX( scbsPlayerX ) + 6, scbsToScreenY( scbsPlayerY ) + 4, 4 - scbsWeaponCooldown / 4, 2 );
        else
          gbFillRect( scbsToScreenX( scbsPlayerX ) + scbsWeaponCooldown / 4 - 4, scbsToScreenY( scbsPlayerY ) + 4, 4, 2 );
    }
}

// -----------------------------------------------------------------------------
// Player
// -----------------------------------------------------------------------------

int scbsPlayerGetXFriction()
{
    if( scbsPlayerDead ) return 10;
    return 40;
}

int scbsPlayerGetXBounce()
{
    if( scbsPlayerDead ) return -1;
    return 0;
}

// Direct port of real `Player::init()`.
void scbsPlayerInit()
{
    scbsPlayerX = 128;
    scbsPlayerY = 150;
    scbsPlayerDir = 1;
    scbsPlayerScore = 0;
    scbsPlayerDead = 0;
    scbsWeaponInit();
}

// Direct port of real `Player::kill()`.
void scbsPlayerKill( int dir )
{
    scbsPlayerDead = 1;
    scbsPlayerVx = dir * 32;
    scbsPlayerVy = -32;
    scbsPopupTimeLeft = 0;
    if( scbsWorldAddScore( scbsPlayerScore ) )
      scbsPopup( "NEW HIGHSCORE!", 40 );
    scbsSaveEeprom();
}

// Direct port of real `Player::update()`.
void scbsPlayerUpdate()
{
    if( !scbsPlayerDead )
    {
        if( gbRepeat( BTN_LEFT, 1 ) )
        {
            scbsPlayerDir = -1;
            scbsPlayerVx = scbsPlayerVx + 16 * scbsPlayerDir;
        }
        if( gbRepeat( BTN_RIGHT, 1 ) )
        {
            scbsPlayerDir = 1;
            scbsPlayerVx = scbsPlayerVx + 16;
        }
        // real `gb.buttons.timeHeld(BTN_DOWN) > 40` -> "held at least 41 ticks"
        if( gbRepeat( BTN_UP, 10 ) && gbHeld( BTN_DOWN, 41 ) )
        {
            scbsWeaponSubtype = scbsWeaponSubtype + 1;
            scbsWeaponSubtype = scbsWeaponSubtype % SCBS_NUM_WEAPONS;
            scbsPlayerScore = 0;
            scbsPopup( "WEAPON CHEAT", 20 );
        }

        if( scbsPlayerY > scbsWorldGetHeight() )
          scbsPlayerKill( scbsPlayerDir );

        if( scbsWorldSolidCollision( scbsPlayerX, scbsPlayerY + 1, 48, 72 ) )
          scbsPlayerDoubleJumped = 0;

        if( gbPressed( BTN_B ) )
        {
            if( scbsWorldSolidCollision( scbsPlayerX, scbsPlayerY + 1, 48, 72 ) )
            {
                scbsPlayerVy = -32;
                scbsPlayerJumping = 1;
                gbPlayPattern( scbsJumpSound, 1 ); // real jump_sound, channel 1
            }
            else if( !scbsPlayerDoubleJumped )
            {
                scbsPlayerVy = -32;
                scbsPlayerDoubleJumped = 1;
                scbsPlayerJumping = 1;
                gbPlayPattern( scbsJumpSound, 1 );
            }
        }
        // real `(timeHeld(B) > 0) && (timeHeld(B) < 5)` -> held for between 1 and 4 ticks inclusive
        if( gbHeld( BTN_B, 1 ) && !gbHeld( BTN_B, 5 ) && scbsPlayerVy < 0 && scbsPlayerJumping )
        {
            if( scbsPlayerDoubleJumped )
              scbsPlayerVy = scbsPlayerVy - 6;
            else
              scbsPlayerVy = scbsPlayerVy - 12;
        }
        if( scbsPlayerVy > 0 )
          scbsPlayerJumping = 0;
    }

    scbsWeaponUpdate();

    int d = scbsPlayerDir;
    int xFriction = scbsPlayerGetXFriction();
    int xBounce = scbsPlayerGetXBounce();
    scbsBoxUpdate( &scbsPlayerX, &scbsPlayerY, &scbsPlayerVx, &scbsPlayerVy, &scbsPlayerDir, 48, 72, 8, 128, xFriction, 5, xBounce, 0 );
    scbsPlayerDir = d; // real upstream override - direction depends only on input, not Box::update()'s own vx-based recalculation
}

// Direct port of real `Player::draw()`.
void scbsPlayerDraw()
{
    if( scbsIsOffScreen( scbsPlayerX, scbsPlayerY, 48, 72 ) ) return;

    int flip = 0;
    if( scbsPlayerDir <= 0 ) flip = 1;

    int frame = ( scbsPlayerDir * scbsPlayerX / 32 + 255 ) % 5;
    if( scbsPlayerVx == 0 ) frame = 0;
    if( !scbsWorldSolidCollision( scbsPlayerX, scbsPlayerY + 1, 48, 72 ) ) // in the air
    {
        if( scbsPlayerVy < 0 ) frame = 4;
        else frame = 1;
    }

    gbDrawBitmapRotated( scbsToScreenX( scbsPlayerX ) - 1, scbsToScreenY( scbsPlayerY ), scbsPlayerBitmap[ frame ], 0, flip ); // NOROT
    scbsWeaponDraw();
}

// -----------------------------------------------------------------------------
// Enemy / EnemiesEngine
// -----------------------------------------------------------------------------

int scbsEnemyGetWidth( int subtype )
{
    if( subtype == SCBS_E_SMALL ) return 48;
    return 72;
}

int scbsEnemyGetHeight( int subtype )
{
    if( subtype == SCBS_E_SMALL ) return 64;
    return 80;
}

int scbsEnemyGetGravity( int health )
{
    if( health > 0 ) return 4;
    return 10;
}

int scbsEnemyGetXBounce( int health )
{
    if( health > 0 ) return 100;
    return -1;
}

int scbsEnemyGetMaxHealth( int subtype )
{
    if( subtype == SCBS_E_SMALL ) return 2;
    return 10;
}

void scbsEnemiesInit()
{
    scbsNextSpawnCount = 10;
    int i;
    for( i = 0; i < SCBS_NUM_ENEMIES; i++ )
    {
        scbsEnemies[ i ].active = 0;
        scbsEnemies[ i ].health = 0;
    }
}

// Direct port of real `EnemiesEngine::addEnemy()`.
void scbsEnemiesAdd()
{
    int i;
    for( i = 0; i < SCBS_NUM_ENEMIES; i++ )
    {
        if( scbsEnemies[ i ].active ) continue;

        scbsEnemies[ i ].active = 1;
        if( scbsArduinoRandom( 0, 6 ) == 0 && scbsWorldMapNumber != 0 ) // randomly spawn a few big monsters
          scbsEnemies[ i ].subtype = SCBS_E_BIG;
        else
          scbsEnemies[ i ].subtype = SCBS_E_SMALL;

        scbsEnemies[ i ].health = scbsEnemyGetMaxHealth( scbsEnemies[ i ].subtype );
        scbsEnemies[ i ].x = scbsWorldGetWidth() / 2 - scbsEnemyGetWidth( scbsEnemies[ i ].subtype ) / 2;
        scbsEnemies[ i ].y = 0;
        scbsEnemies[ i ].vx = scbsArduinoRandom( 0, 2 ) * 20 - 10;
        scbsEnemies[ i ].vy = 0;
        return;
    }
}

// Direct port of real `Enemy::update()` - declared `int` upstream but never
// actually returns a value on any path (a harmless real upstream
// declaration quirk - its own return value is never read anywhere it's
// called from either), ported as `void`.
void scbsEnemyUpdate( int i )
{
    if( !scbsEnemies[ i ].active ) return;

    int subtype = scbsEnemies[ i ].subtype;
    int width = scbsEnemyGetWidth( subtype );
    int height = scbsEnemyGetHeight( subtype );
    int gravity = scbsEnemyGetGravity( scbsEnemies[ i ].health );
    int xBounce = scbsEnemyGetXBounce( scbsEnemies[ i ].health );

    scbsBoxUpdate( &scbsEnemies[ i ].x, &scbsEnemies[ i ].y, &scbsEnemies[ i ].vx, &scbsEnemies[ i ].vy, &scbsEnemies[ i ].dir,
                  width, height, gravity, 32, 0, 5, xBounce, 0 );

    if( scbsEnemies[ i ].y > scbsWorldGetHeight() )
    {
        if( scbsEnemies[ i ].health > 0 ) // respawn in "angry" mode when it falls off the bottom of the map
        {
            scbsEnemies[ i ].x = scbsWorldGetWidth() / 2 - width / 2;
            scbsEnemies[ i ].y = 0;
            scbsEnemies[ i ].vx = scbsEnemies[ i ].dir * 20;
            gbPlayPattern( scbsEnemyFeltSound, 2 ); // real enemy_felt_sound, channel 2
        }
        else
          scbsEnemies[ i ].active = 0;
    }
}

// Direct port of real `EnemiesEngine::update()`.
void scbsEnemiesUpdate()
{
    int j, i;
    for( j = 0; j < SCBS_NUM_BULLETS; j++ )
    {
        if( scbsBullets[ j ].timeLeft <= 0 ) continue;

        for( i = 0; i < SCBS_NUM_ENEMIES; i++ )
        {
            if( scbsEnemies[ i ].health <= 0 ) continue;

            // skip bullets with a low speed (falling particles) except
            // explosions/mines/grenades - real upstream `break`s out of
            // the enemy loop here (harmless either way, since this
            // condition never depends on `i`).
            if( gbAbsInt( scbsBullets[ j ].vx ) < 20 &&
                !( scbsBullets[ j ].subtype == SCBS_W_EXPLOSION || scbsBullets[ j ].subtype == SCBS_W_MINE || scbsBullets[ j ].subtype == SCBS_W_GRENADE ) )
              break;

            int ew = scbsEnemyGetWidth( scbsEnemies[ i ].subtype );
            int eh = scbsEnemyGetHeight( scbsEnemies[ i ].subtype );
            int bw = scbsBulletGetWidth( scbsBullets[ j ].subtype, scbsBullets[ j ].vx, scbsBullets[ j ].timeLeft );
            int bh = scbsBulletGetHeight( scbsBullets[ j ].subtype, scbsBullets[ j ].vx, scbsBullets[ j ].timeLeft );

            if( gbCollideRectRect( scbsEnemies[ i ].x, scbsEnemies[ i ].y, ew, eh, scbsBullets[ j ].x, scbsBullets[ j ].y, bw, bh ) )
            {
                if( scbsBulletExplodes( scbsBullets[ j ].subtype ) )
                  scbsBullets[ j ].timeLeft = 1;
                if( scbsBulletDestroyOnEnemyContact( scbsBullets[ j ].subtype ) )
                  scbsBullets[ j ].vx = ( scbsBullets[ j ].vx * scbsBulletGetXBounce( scbsBullets[ j ].subtype ) ) / 100;

                scbsEnemies[ i ].health = scbsEnemies[ i ].health - scbsBulletGetDamage( scbsBullets[ j ].subtype );

                if( scbsEnemies[ i ].health <= 0 ) // make the enemy jump when dead
                {
                    int dir;
                    if( scbsBullets[ j ].subtype == SCBS_W_EXPLOSION ) // fly away from the explosive
                    {
                        if( ( ( scbsEnemies[ i ].x + ew / 2 ) - ( scbsBullets[ j ].x + bw / 2 ) ) > 0 )
                          dir = 1;
                        else
                          dir = -1;
                    }
                    else // fly in the same direction as the incoming bullet
                    {
                        if( scbsBullets[ j ].vx > 0 )
                          dir = 1;
                        else
                          dir = -1;
                    }

                    scbsEnemies[ i ].vx = dir * scbsArduinoRandom( 24, 32 );
                    // real `random(-48,-64)` - see header comment (quirk 6): min>=max collapses to the constant -48, not a range.
                    scbsEnemies[ i ].vy = scbsArduinoRandom( -48, -64 );
                    gbPlayPattern( scbsEnemyDeathSound, 1 ); // real enemy_death_sound, channel 1
                }
                else if( scbsBullets[ j ].subtype == SCBS_W_CLUB ) // go away from the player when hit by a club
                {
                    int dir = 1;
                    if( ( scbsEnemies[ i ].x + ew / 2 ) - ( scbsPlayerX + 48 / 2 ) > 0 )
                      dir = 1;
                    else
                      dir = -1;
                    scbsEnemies[ i ].vx = dir * gbAbsInt( scbsEnemies[ i ].vx );
                }
            }
        }
    }

    scbsNextSpawnCount = scbsNextSpawnCount - 1;
    if( scbsNextSpawnCount == 0 ) // spawn rate increases slowly depending on score
    {
        scbsNextSpawnCount = scbsMap( scbsPlayerScore, 0, 50, 60, 30 );
        scbsNextSpawnCount = gbMax( scbsNextSpawnCount, 10 );
        scbsEnemiesAdd();
    }

    for( i = 0; i < SCBS_NUM_ENEMIES; i++ )
      scbsEnemyUpdate( i );
}

// Direct port of real `Enemy::draw()`.
void scbsEnemyDraw( int i )
{
    int subtype = scbsEnemies[ i ].subtype;
    int width = scbsEnemyGetWidth( subtype );
    int height = scbsEnemyGetHeight( subtype );
    if( scbsIsOffScreen( scbsEnemies[ i ].x, scbsEnemies[ i ].y, width, height ) ) return;

    int flip = 0;
    if( scbsEnemies[ i ].dir <= 0 ) flip = 1;

    if( subtype == SCBS_E_SMALL )
    {
        int frame = ( scbsEnemies[ i ].dir * scbsEnemies[ i ].x / 16 + 255 ) % 5;
        gbDrawBitmapRotated( scbsToScreenX( scbsEnemies[ i ].x ) - 1, scbsToScreenY( scbsEnemies[ i ].y ), scbsSmallEnemyBitmap[ frame ], 0, flip );
    }
    else
    {
        int frame = ( scbsEnemies[ i ].dir * scbsEnemies[ i ].x / 16 + 255 ) % 6;
        gbDrawBitmapRotated( scbsToScreenX( scbsEnemies[ i ].x ) - 4, scbsToScreenY( scbsEnemies[ i ].y ), scbsBigEnemyBitmap[ frame ], 0, flip );
    }
}

void scbsEnemiesDraw()
{
    int i;
    for( i = 0; i < SCBS_NUM_ENEMIES; i++ )
      if( scbsEnemies[ i ].active )
        scbsEnemyDraw( i );
}

// -----------------------------------------------------------------------------
// Crate
// -----------------------------------------------------------------------------

// Direct port of real `Crate::init()`.
void scbsCrateInit()
{
    scbsCrateVy = 0;
    int goodSpot;
    do
    {
        scbsCrateX = scbsArduinoRandom( SCBS_SPRITE_SIZE * SCBS_SCALE, scbsWorldGetWidth() - SCBS_SPRITE_SIZE * SCBS_SCALE - 48 );
        scbsCrateY = scbsArduinoRandom( SCBS_SPRITE_SIZE * SCBS_SCALE, scbsWorldGetHeight() - SCBS_SPRITE_SIZE * SCBS_SCALE - 48 );

        goodSpot = 1;
        if( gbAbsInt( scbsPlayerX - scbsCrateX ) < 128 || gbAbsInt( scbsPlayerY - scbsCrateY ) < 128 )
          goodSpot = 0; // too close to the player
        if( scbsCrateX > ( scbsWorldGetWidth() / 2 - 128 ) && scbsCrateX < ( scbsWorldGetWidth() / 2 + 128 ) && scbsCrateY < 336 )
          goodSpot = 0; // avoid the top central zone where mobs spawn
    }
    while( !goodSpot );
}

// Direct port of real `Crate::update()` - see this file's own header
// comment (quirks 6/7) on the weapon-reroll edge case and the missing
// PISTOL popup case.
void scbsCrateUpdate()
{
    scbsBoxUpdate( &scbsCrateX, &scbsCrateY, &scbsCrateVx, &scbsCrateVy, &scbsCrateDir, 48, 48, 8, 128, 5, 5, 100, 100 );

    if( scbsCrateY > scbsWorldGetHeight() ) // reinit the crate if it fell out of the world
      scbsCrateInit();

    if( gbCollideRectRect( scbsCrateX, scbsCrateY, 48, 48, scbsPlayerX, scbsPlayerY, 48, 72 ) )
    {
        gbPlayOK();

        // With the weapon cheat on, upstream skips BOTH the weapon re-roll
        // and the score increment - crates stop being worth anything, which
        // is the point of the cheat.
        if( scbsCheatGun == 0 )
        {
            // add a random value to the weapon type, inferior to the number
            // of unlocked weapons, to avoid picking the same weapon twice
            scbsWeaponSubtype = ( scbsWeaponSubtype + scbsArduinoRandom( 1, scbsUnlockedWeapons + 1 ) ) % ( scbsUnlockedWeapons + 1 );
            scbsPlayerScore = scbsPlayerScore + 1;
        }

        if( scbsWeaponSubtype == SCBS_W_CLUB ) scbsPopup( "CLUB", 20 );
        else if( scbsWeaponSubtype == SCBS_W_AKIMBO ) scbsPopup( "AKIMBO", 20 );
        else if( scbsWeaponSubtype == SCBS_W_REVOLVER ) scbsPopup( "REVOLVER", 20 );
        else if( scbsWeaponSubtype == SCBS_W_SNIPER ) scbsPopup( "SNIPER", 20 );
        else if( scbsWeaponSubtype == SCBS_W_SHOTGUN ) scbsPopup( "SHOTGUN", 20 );
        else if( scbsWeaponSubtype == SCBS_W_RIFLE ) scbsPopup( "RIFLE", 20 );
        else if( scbsWeaponSubtype == SCBS_W_MACHINEGUN ) scbsPopup( "MACHINEGUN", 20 );
        else if( scbsWeaponSubtype == SCBS_W_DISK ) scbsPopup( "DISK", 20 );
        else if( scbsWeaponSubtype == SCBS_W_LASER ) scbsPopup( "LASER", 20 );
        else if( scbsWeaponSubtype == SCBS_W_GRENADE ) scbsPopup( "GRENADE", 20 );
        else if( scbsWeaponSubtype == SCBS_W_ROCKET ) scbsPopup( "ROCKET", 20 );
        else if( scbsWeaponSubtype == SCBS_W_MINE ) scbsPopup( "MINE", 20 );
        // no case for SCBS_W_PISTOL - see this file's own header comment (quirk 7)

        if( scbsWorldMapNumber == 0 )
        {
            if( scbsPlayerScore == SCBS_SCORETHRESHOLD_1 )
            {
                if( scbsUnlockedWeapons < SCBS_W_RIFLE )
                {
                    scbsUnlockedWeapons = SCBS_W_RIFLE;
                    scbsWeaponSubtype = SCBS_W_RIFLE;
                    scbsPopup( "RIFLE UNLOCKED!", 40 );
                    gbPlayPattern( scbsPowerUpSound, 2 ); // real power_up_sound, channel 2
                }
            }
            else if( scbsPlayerScore == SCBS_SCORETHRESHOLD_2 )
            {
                if( scbsUnlockedWeapons < SCBS_W_SHOTGUN )
                {
                    scbsUnlockedWeapons = SCBS_W_SHOTGUN;
                    scbsWeaponSubtype = SCBS_W_SHOTGUN;
                    scbsPopup( "SHOTGUN UNLOCKED!", 40 );
                    gbPlayPattern( scbsPowerUpSound, 2 );
                }
            }
            else if( scbsPlayerScore == SCBS_SCORETHRESHOLD_3 )
            {
                if( scbsUnlockedMaps < 1 )
                {
                    scbsUnlockedMaps = 1;
                    scbsPopup( "NEW MAP UNLOCKED!", 40 );
                    gbPlayPattern( scbsPowerUpSound, 0 );
                }
            }
        }
        else if( scbsWorldMapNumber == 1 )
        {
            if( scbsPlayerScore == SCBS_SCORETHRESHOLD_1 )
            {
                if( scbsUnlockedWeapons < SCBS_W_ROCKET )
                {
                    scbsUnlockedWeapons = SCBS_W_ROCKET;
                    scbsWeaponSubtype = SCBS_W_ROCKET;
                    scbsPopup( "ROCKETS UNLOCKED!", 40 );
                    gbPlayPattern( scbsPowerUpSound, 0 );
                }
            }
            else if( scbsPlayerScore == SCBS_SCORETHRESHOLD_2 )
            {
                if( scbsUnlockedWeapons < SCBS_W_CLUB )
                {
                    scbsUnlockedWeapons = SCBS_W_CLUB;
                    scbsWeaponSubtype = SCBS_W_CLUB;
                    scbsPopup( "CLUB UNLOCKED!", 40 );
                    gbPlayPattern( scbsPowerUpSound, 0 );
                }
            }
            else if( scbsPlayerScore == SCBS_SCORETHRESHOLD_3 )
            {
                if( scbsUnlockedWeapons < SCBS_W_REVOLVER )
                {
                    scbsUnlockedWeapons = SCBS_W_REVOLVER;
                    scbsWeaponSubtype = SCBS_W_REVOLVER;
                    scbsPopup( "REVOLVER UNLOCKED!", 40 );
                    gbPlayPattern( scbsPowerUpSound, 0 );
                }
            }
            else if( scbsPlayerScore == SCBS_SCORETHRESHOLD_4 )
            {
                if( scbsUnlockedWeapons < SCBS_W_MINE )
                {
                    scbsUnlockedWeapons = SCBS_W_MINE;
                    scbsWeaponSubtype = SCBS_W_MINE;
                    scbsPopup( "MINES UNLOCKED!", 40 );
                    gbPlayPattern( scbsPowerUpSound, 0 );
                }
            }
            else if( scbsPlayerScore == SCBS_SCORETHRESHOLD_5 )
            {
                if( scbsUnlockedMaps < 2 )
                {
                    scbsUnlockedMaps = 2;
                    scbsPopup( "NEW MAP UNLOCKED!", 40 );
                    gbPlayPattern( scbsPowerUpSound, 0 );
                }
            }
        }
        else if( scbsWorldMapNumber == 2 )
        {
            if( scbsPlayerScore == SCBS_SCORETHRESHOLD_1 )
            {
                if( scbsUnlockedWeapons < SCBS_W_SNIPER )
                {
                    scbsUnlockedWeapons = SCBS_W_SNIPER;
                    scbsWeaponSubtype = SCBS_W_SNIPER;
                    scbsPopup( "SNIPER UNLOCKED!", 40 );
                    gbPlayPattern( scbsPowerUpSound, 0 );
                }
            }
            else if( scbsPlayerScore == SCBS_SCORETHRESHOLD_2 )
            {
                if( scbsUnlockedWeapons < SCBS_W_MACHINEGUN )
                {
                    scbsUnlockedWeapons = SCBS_W_MACHINEGUN;
                    scbsWeaponSubtype = SCBS_W_MACHINEGUN;
                    scbsPopup( "MACHINEGUN UNLOCKED!", 40 );
                    gbPlayPattern( scbsPowerUpSound, 0 );
                }
            }
            else if( scbsPlayerScore == SCBS_SCORETHRESHOLD_3 )
            {
                if( scbsUnlockedWeapons < SCBS_W_GRENADE )
                {
                    scbsUnlockedWeapons = SCBS_W_GRENADE;
                    scbsWeaponSubtype = SCBS_W_GRENADE;
                    scbsPopup( "GRENADES UNLOCKED!", 40 );
                    gbPlayPattern( scbsPowerUpSound, 0 );
                }
            }
            else if( scbsPlayerScore == SCBS_SCORETHRESHOLD_4 )
            {
                if( scbsUnlockedWeapons < SCBS_W_AKIMBO )
                {
                    scbsUnlockedWeapons = SCBS_W_AKIMBO;
                    scbsWeaponSubtype = SCBS_W_AKIMBO;
                    scbsPopup( "AKIMBO UNLOCKED!", 40 );
                    gbPlayPattern( scbsPowerUpSound, 0 );
                }
            }
            else if( scbsPlayerScore == SCBS_SCORETHRESHOLD_5 )
            {
                if( scbsUnlockedMaps < 3 )
                {
                    scbsUnlockedMaps = 3;
                    scbsPopup( "NEW MAP UNLOCKED!", 40 );
                    gbPlayPattern( scbsPowerUpSound, 0 );
                }
            }
        }
        else if( scbsWorldMapNumber == 3 ) // real upstream has no sound call at all on any of these 3 unlocks
        {
            if( scbsPlayerScore == SCBS_SCORETHRESHOLD_3 )
            {
                if( scbsUnlockedWeapons < SCBS_W_DISK )
                {
                    scbsUnlockedWeapons = SCBS_W_DISK;
                    scbsWeaponSubtype = SCBS_W_DISK;
                    scbsPopup( "DISK UNLOCKED!", 40 );
                }
            }
            else if( scbsPlayerScore == SCBS_SCORETHRESHOLD_4 )
            {
                if( scbsUnlockedWeapons < SCBS_W_LASER )
                {
                    scbsUnlockedWeapons = SCBS_W_LASER;
                    scbsWeaponSubtype = SCBS_W_LASER;
                    scbsPopup( "LASER UNLOCKED!", 40 );
                }
            }
            else if( scbsPlayerScore == SCBS_SCORETHRESHOLD_5 )
            {
                if( scbsUnlockedMaps < 4 )
                {
                    scbsUnlockedMaps = 4;
                    scbsPopup( "LAST MAP UNLOCKED!", 40 );
                }
            }
        }

        scbsCrateInit(); // move the crate
    }
}

void scbsCrateDraw()
{
    if( scbsIsOffScreen( scbsCrateX, scbsCrateY, 48, 48 ) ) return;
    gbDrawBitmap( scbsToScreenX( scbsCrateX ), scbsToScreenY( scbsCrateY ), scbsCrateBitmap );
}

// -----------------------------------------------------------------------------
// World map-select tile assignment, gameplay tile rendering, compass
// -----------------------------------------------------------------------------

// Direct port of the real per-map `switch` inside `World::chooseMap()` that
// picks which real wall/platform/edge textures a map uses.
void scbsSelectMapTiles( int mapIndex )
{
    scbsTiles = scbsMaps[ mapIndex ];
    scbsWorldMapNumber = mapIndex;

    if( mapIndex == 1 )
    {
        scbsWallBitmap = scbsBricks;
        scbsPlatformBitmap = scbsBeam;
        scbsHasEdge = 0;
    }
    else if( mapIndex == 3 )
    {
        scbsWallBitmap = scbsBlackWall;
        scbsPlatformBitmap = scbsRoundPlatform;
        scbsEdgeBitmap = scbsRoundPlatformEdge;
        scbsHasEdge = 1;
    }
    else // 0, 2, 4
    {
        scbsWallBitmap = scbsBricks;
        scbsPlatformBitmap = scbsGrass;
        scbsEdgeBitmap = scbsGrassEdge;
        scbsHasEdge = 1;
    }
}

// Direct port of real `World::draw()` - see this file's own header comment
// (quirk 9) on the preserved non-exclusive edge/corner-tile `if`s.
void scbsWorldDraw()
{
    int xMin = scbsCameraX / SCBS_SPRITE_SIZE;
    int xMax = LCDWIDTH / SCBS_SPRITE_SIZE + scbsCameraX / SCBS_SPRITE_SIZE + 1;
    int yMin = scbsCameraY / SCBS_SPRITE_SIZE;
    int yMax = LCDHEIGHT / SCBS_SPRITE_SIZE + scbsCameraY / SCBS_SPRITE_SIZE + 1;

    int w = scbsTiles[ 0 ];
    int h = scbsTiles[ 1 ];

    int x, y;
    for( y = yMin; y < yMax; y++ )
    {
        for( x = xMin; x < xMax; x++ )
        {
            if( x < 0 || x >= w || y < 0 || y >= h ) continue;
            if( !gbGetBitmapPixel( scbsTiles, x, y ) ) continue;

            int flip = 0;
            int offset = 0;
            int* bitmap = scbsPlatformBitmap;

            if( y >= scbsWorldGetHeight() / SCBS_SPRITE_SIZE / SCBS_SCALE - 1 || y <= 0 || gbGetBitmapPixel( scbsTiles, x, y - 1 ) )
            {
                bitmap = scbsWallBitmap;
            }
            else if( scbsHasEdge )
            {
                if( y > 0 && !gbGetBitmapPixel( scbsTiles, x + 1, y ) )
                {
                    bitmap = scbsEdgeBitmap; // platform corner
                    flip = 1; // FLIPH
                    offset = 2;
                }
                if( y > 0 && !gbGetBitmapPixel( scbsTiles, x - 1, y ) )
                {
                    bitmap = scbsEdgeBitmap; // platform corner - see header comment (quirk 9): flip/offset are NOT reset here
                }
            }

            gbDrawBitmapRotated( x * SCBS_SPRITE_SIZE - scbsCameraX - offset, y * SCBS_SPRITE_SIZE - scbsCameraY, bitmap, 0, flip ); // NOROT
        }
    }
}

// Direct port of real `drawCompass()`.
void scbsDrawCompass()
{
    int x = ( scbsCrateX + 48 / 2 - scbsPlayerX - 48 / 2 ) / SCBS_SCALE;
    int y = ( scbsCrateY + 48 / 2 - scbsPlayerY - 72 / 2 ) / SCBS_SCALE; // real upstream reuses crate.getWidth() here too - inert, since Crate's width==height==48
    int dist = (int)sqrt( (float)( x * x + y * y ) );
    if( dist > 20 )
    {
        int dx = scbsToScreenX( scbsPlayerX + 48 / 2 ) + ( 16 * x / dist );
        int dy = scbsToScreenY( scbsPlayerY + 72 / 2 ) + ( 16 * y / dist );
        gbDrawLine( dx, dy, dx + x / 8, dy + y / 8 );
    }
}

// Direct port of real `drawAll()`.
void scbsDrawAll()
{
    scbsWorldDraw();
    scbsCrateDraw();
    scbsEnemiesDraw();
    scbsPlayerDraw();
    if( !scbsPlayerDead && scbsWorldMapNumber )
      scbsDrawCompass();

    int i;
    for( i = 0; i < SCBS_NUM_BULLETS; i++ )
    {
        if( scbsBullets[ i ].subtype == SCBS_W_EXPLOSION )
          gbSetColor( GB_INVERT );
        scbsBulletDraw( i );
        gbSetColor( GB_BLACK );
    }

    gbSetColorBg( GB_BLACK, GB_WHITE );
    gbPrintNumber( scbsPlayerScore );

    scbsUpdatePopup();
}

// Direct port of real `initGame()`.
void scbsInitGame()
{
    scbsPlayerInit();
    scbsEnemiesInit();
    scbsCrateInit();
    scbsWeaponSubtype = 0;
    scbsShakeTimeLeft = 0;
}

// -----------------------------------------------------------------------------
// States - real upstream's own blocking loops (`mainMenu()`'s
// `gb.titleScreen(logo)`, `World::chooseMap()`, `gamePaused()`, and
// `loop()`'s own post-death `while(1)`) flattened into explicit states,
// matching this project's own established "blocking loop -> explicit
// resumable state" treatment (see gamePong.c's own header comment).
// -----------------------------------------------------------------------------

void scbsBeginTitle()
{
    scbsMenuNumber = 0;
    scbsCheatGun = 0;
    scbsUnlocMap = 0;
    scbsSMenuIndex = 0;
    scbsGunMenuIndex = 0;
    scbsState = SCBS_STATE_TITLE;
}

// == real `mainMenu()`'s own `gb.titleScreen(logo)` - dismissed by a
// genuine fresh Button A press (this engine's own menu-select button,
// matching real `titleScreen()`'s own real dismiss button).
void scbsUpdateTitle()
{
    // real Gamebuino::titleScreen()'s own real left/bottom anchor (x=0,
    // y=LCDHEIGHT-logoHeight) - already confirmed directly against the
    // real source during gameUfoRace.c's own port.
    gbDrawBitmap( 0, LCDHEIGHT - 30, scbsLogoBitmap );
    gbCursorX = 28;
    gbCursorY = 2;
    gbPrintString( "PRESS A" );

    if( gbPressed( BTN_A ) )
    {
        // Real setup() calls mainMenu() and then world.MainMenu() - the mode
        // select, not the map select, is what follows the title screen here.
        scbsState = SCBS_STATE_MAINMENU;
        scbsChooseMapIndex = scbsWorldMapNumber;
    }
}

// == real `World::chooseMap()`'s own per-tick body.
void scbsUpdateChooseMap()
{
    scbsSelectMapTiles( scbsChooseMapIndex );

    gbCursorY = LCDHEIGHT - 17;
    scbsPrintCentered( scbsSelectMapText );
    gbCursorX = 24;
    gbCursorY = LCDHEIGHT - 11;
    gbPrintString( "Score: " );
    gbPrintNumber( scbsScore[ scbsChooseMapIndex ] );

    // draw the map centered on the screen
    gbDrawBitmap( LCDWIDTH / 2 - scbsTiles[ 0 ] / 2, LCDHEIGHT / 2 - scbsTiles[ 1 ] / 2 - 5, scbsTiles );

    int x, y;
    for( x = SCBS_SPRITE_SIZE; x < LCDWIDTH - SCBS_SPRITE_SIZE; x = x + SCBS_SPRITE_SIZE )
      gbDrawBitmap( x, 0, scbsPlatformBitmap );
    for( y = SCBS_SPRITE_SIZE; y < LCDHEIGHT; y = y + SCBS_SPRITE_SIZE )
    {
        gbDrawBitmap( 0, y, scbsWallBitmap );
        gbDrawBitmap( LCDWIDTH - SCBS_SPRITE_SIZE, y, scbsWallBitmap );
    }
    if( scbsHasEdge )
    {
        gbDrawBitmap( 0, 0, scbsEdgeBitmap );
        gbDrawBitmapRotated( LCDWIDTH - SCBS_SPRITE_SIZE - 2, 0, scbsEdgeBitmap, 0, 1 ); // NOROT, FLIPH
    }
    else
    {
        gbDrawBitmap( 0, 0, scbsPlatformBitmap );
        gbDrawBitmap( LCDWIDTH - SCBS_SPRITE_SIZE, 0, scbsPlatformBitmap );
    }

    // Stone Edition comments out both of the original's blinking notices here
    // (the "Next unlock:" progress line and the "LOCKED!" overlay) and the
    // `<= unlockedMaps` guard below it, marking the latter "crack map" - so
    // every map is playable from the very first run. Reproduced by leaving
    // all three out, exactly as this fork ships.

    if( gbPressed( BTN_A ) )
    {
        scbsInitGame();
        scbsState = SCBS_STATE_PLAY;
        return;
    }
    if( gbPressed( BTN_RIGHT ) )
      scbsChooseMapIndex = ( scbsChooseMapIndex + 1 ) % SCBS_NUM_MAPS;
    if( gbPressed( BTN_LEFT ) )
      scbsChooseMapIndex = ( scbsChooseMapIndex - 1 + SCBS_NUM_MAPS ) % SCBS_NUM_MAPS;
    if( gbPressed( BTN_C ) )
      scbsState = SCBS_STATE_MAINMENU;
    if( gbPressed( BTN_B ) )
    {
        scbsSMenuIndex = 0;
        scbsState = SCBS_STATE_SMENU;
    }
}

// == real `loop()`'s own `if (gb.update())` body.
void scbsUpdatePlay()
{
    if( gbPressed( BTN_C ) )
    {
        scbsState = SCBS_STATE_PAUSED;
        return;
    }

    scbsCrateUpdate();
    scbsPlayerUpdate();
    scbsEnemiesUpdate();
    scbsSaveEeprom(); // real upstream - checks whether values changed before writing, so this won't wear out a real EEPROM

    // real upstream literally checks `getWidth()*SCALE<=LCDWIDTH` here - see
    // this file's own header comment (quirk 8) on why this is ported
    // literally despite reading like a typo.
    if( scbsWorldGetWidth() * SCBS_SCALE <= LCDWIDTH )
      scbsCameraX = 0;
    else
    {
        scbsCameraX = ( scbsPlayerX + 48 / 2 ) / SCBS_SCALE - LCDWIDTH / 2;
        scbsCameraX = scbsConstrain( scbsCameraX, 0, scbsWorldGetWidth() / SCBS_SCALE - LCDWIDTH );
    }
    if( scbsWorldGetHeight() * SCBS_SCALE <= LCDHEIGHT )
      scbsCameraY = 0;
    else
    {
        scbsCameraY = ( scbsPlayerY + 72 / 2 ) / SCBS_SCALE - LCDHEIGHT / 2;
        scbsCameraY = scbsConstrain( scbsCameraY, 0, scbsWorldGetHeight() / SCBS_SCALE - LCDHEIGHT - SCBS_SPRITE_SIZE / 2 );
    }

    if( scbsShakeTimeLeft > 0 )
    {
        scbsShakeTimeLeft = scbsShakeTimeLeft - 1;
        scbsCameraX = scbsCameraX + scbsArduinoRandom( -1, 2 ) * scbsShakeAmplitude;
        scbsCameraY = scbsCameraY + scbsArduinoRandom( -1, 2 ) * scbsShakeAmplitude;
    }

    scbsDrawAll();

    int i;
    for( i = 0; i < SCBS_NUM_ENEMIES; i++ ) // player - monster collisions
    {
        if( scbsEnemies[ i ].health <= 0 ) continue;
        if( gbCollideRectRect( scbsEnemies[ i ].x, scbsEnemies[ i ].y, scbsEnemyGetWidth( scbsEnemies[ i ].subtype ), scbsEnemyGetHeight( scbsEnemies[ i ].subtype ),
                                scbsPlayerX, scbsPlayerY, 48, 72 ) )
        {
            int dir = 1;
            if( ( ( scbsEnemies[ i ].x + scbsEnemyGetWidth( scbsEnemies[ i ].subtype ) / 2 ) - ( scbsPlayerX + 48 / 2 ) ) > 0 )
              dir = -1;
            scbsPlayerDead = 1;
            scbsPlayerKill( dir );
            break;
        }
    }

    for( i = 0; i < SCBS_NUM_BULLETS; i++ ) // player - bullet collisions
    {
        if( scbsBullets[ i ].timeLeft <= 0 ) continue;
        if( scbsBulletDamagePlayer( scbsBullets[ i ].subtype ) &&
            gbCollideRectRect( scbsBullets[ i ].x, scbsBullets[ i ].y,
                                scbsBulletGetWidth( scbsBullets[ i ].subtype, scbsBullets[ i ].vx, scbsBullets[ i ].timeLeft ),
                                scbsBulletGetHeight( scbsBullets[ i ].subtype, scbsBullets[ i ].vx, scbsBullets[ i ].timeLeft ),
                                scbsPlayerX, scbsPlayerY, 48, 72 ) )
          scbsPlayerKill( scbsBullets[ i ].dir );
    }

    if( scbsPlayerDead )
    {
        scbsGameOverCount = 20;
        if( !scbsPopupTimeLeft ) // real upstream - only shown if the "NEW HIGHSCORE!" popup isn't already showing
          scbsPopup( "GAME OVER!", 20 );
        scbsState = SCBS_STATE_GAMEOVER;
    }
}

// == real `gamePaused()`.
void scbsUpdatePaused()
{
    scbsDrawAll();
    gbSetColorBg( GB_BLACK, GB_WHITE );
    gbCursorX = 0;
    gbCursorY = 0;
    gbPrintString( "GAME PAUSED\n" );

    if( scbsCheatGun == 1 )
      gbPrintString( "A: CHANGE GUN\n" );

    gbPrintString( "B: SAVE & QUIT\nC: RESUME" );

    if( scbsCheatGun == 1 && gbPressed( BTN_A ) )
    {
        scbsGunMenuIndex = 0;
        scbsState = SCBS_STATE_GUNMENU;
        return;
    }

    if( gbPressed( BTN_C ) )
    {
        scbsState = SCBS_STATE_PLAY;
        return;
    }
    if( gbPressed( BTN_B ) )
    {
        scbsWorldAddScore( scbsPlayerScore );
        scbsSaveEeprom();
        scbsState = SCBS_STATE_CHOOSEMAP;
        scbsChooseMapIndex = scbsWorldMapNumber;
    }
}

// == real `loop()`'s own post-death `while(1)` (the ragdoll-fling window).
void scbsUpdateGameOver()
{
    scbsPlayerUpdate();
    scbsEnemiesUpdate();
    scbsDrawAll();

    scbsGameOverCount = scbsGameOverCount - 1;
    if( scbsGameOverCount <= 0 || gbPressed( BTN_C ) )
    {
        scbsInitGame();
        scbsState = SCBS_STATE_PLAY;
    }
}

void gameSuperCrateBuinoStone_init()
{
    gbBegin();
    scbsLoadEeprom();
    // this game's own local popup state needs a fresh reset on every
    // launch, not just real hardware's own single power-on - see this
    // file's own header comment.
    scbsPopupTimeLeft = 0;
    scbsShakeTimeLeft = 0;
    scbsBeginTitle();
}

// -----------------------------------------------------------------------------
// Stone Edition's own added screens. Real upstream drives its two lists with
// `gb.menu()`, a blocking Gamebuino widget this shim has no equivalent for,
// so both are hand-rolled as explicit states - the same treatment
// gameConduit.c's own condUpdateMenu() established for the identical gap.
// -----------------------------------------------------------------------------

// == real `World::MainMenu()`. Four mode logos, of which only the first is
// live: upstream's own A-press handlers for Defence/MPDM/Story are commented
// out, and all three blink a "LOCKED!" overlay instead. `chooseMapDef()`
// exists in the source but is reachable only from one of those commented-out
// branches, so it is genuinely dead and is not ported.
void scbsUpdateMainMenu()
{
    int x, y;
    int* logo = scbsModeClassic;

    scbsSelectMapTiles( 0 );

    if( scbsMenuNumber == 1 )      logo = scbsModeDefence;
    else if( scbsMenuNumber == 2 ) logo = scbsModeMpdm;
    else if( scbsMenuNumber == 3 ) logo = scbsModeStory;

    gbCursorY = LCDHEIGHT - 7;
    scbsPrintCentered( scbsSelectModeText );

    gbDrawBitmap( 10, 10, logo );

    for( x = SCBS_SPRITE_SIZE; x < LCDWIDTH - SCBS_SPRITE_SIZE; x = x + SCBS_SPRITE_SIZE )
      gbDrawBitmap( x, 0, scbsPlatformBitmap );

    for( y = SCBS_SPRITE_SIZE; y < LCDHEIGHT; y = y + SCBS_SPRITE_SIZE )
    {
        gbDrawBitmap( 0, y, scbsWallBitmap );
        gbDrawBitmap( LCDWIDTH - SCBS_SPRITE_SIZE, y, scbsWallBitmap );
    }

    gbDrawBitmap( 0, 0, scbsPlatformBitmap );
    gbDrawBitmap( LCDWIDTH - SCBS_SPRITE_SIZE, 0, scbsPlatformBitmap );

    if( scbsMenuNumber == 1 || scbsMenuNumber == 2 || scbsMenuNumber == 3 )
    {
        if( ( gbFrameCount % 10 ) > 3 ) // make it blink
        {
            gbSetColor( GB_BLACK );
            gbFillRect( 28, 15, 28, 7 );
            gbSetColor( GB_WHITE );
            gbCursorX = 29;
            gbCursorY = 16;
            gbPrintString( "LOCKED!" );
            gbSetColor( GB_BLACK );
        }
    }

    if( gbPressed( BTN_A ) && scbsMenuNumber == 0 )
    {
        scbsChooseMapIndex = scbsWorldMapNumber;
        scbsState = SCBS_STATE_CHOOSEMAP;
        return;
    }

    if( gbPressed( BTN_RIGHT ) )
      scbsMenuNumber = ( scbsMenuNumber + 1 ) % SCBS_NUM_MODES;

    if( gbPressed( BTN_LEFT ) )
      scbsMenuNumber = ( scbsMenuNumber - 1 + SCBS_NUM_MODES ) % SCBS_NUM_MODES;

    if( gbPressed( BTN_C ) )
      scbsBeginTitle();
}

// == real `smenu()`. Upstream's own "Reset score" case has an empty body, so
// selecting it really does nothing at all - preserved.
void scbsUpdateSMenu()
{
    gbSetColorBg( GB_BLACK, GB_WHITE );
    gbCursorX = 0;
    gbCursorY = 0;
    gbPrintString( "S-MENU\n" );

    if( scbsSMenuIndex == 0 ) gbPrintString( ">Cheats\n" );
    else                      gbPrintString( " Cheats\n" );

    if( scbsSMenuIndex == 1 ) gbPrintString( ">Stystem info\n" );
    else                      gbPrintString( " Stystem info\n" );

    if( scbsSMenuIndex == 2 ) gbPrintString( ">Reset score\n" );
    else                      gbPrintString( " Reset score\n" );

    if( gbPressed( BTN_UP ) )
      scbsSMenuIndex = ( scbsSMenuIndex - 1 + 3 ) % 3;

    if( gbPressed( BTN_DOWN ) )
      scbsSMenuIndex = ( scbsSMenuIndex + 1 ) % 3;

    if( gbPressed( BTN_A ) )
    {
        if( scbsSMenuIndex == 0 )
        {
            // Cheats
            if( scbsCheatGun == 0 )
            {
                scbsCheatGun = 1;
                scbsPopup( "Cheats ON", 20 );
            }
            else
            {
                scbsCheatGun = 0;
                scbsPopup( "Cheats OFF", 20 );
            }

            scbsState = SCBS_STATE_CHOOSEMAP;
            return;
        }

        if( scbsSMenuIndex == 1 )
        {
            scbsState = SCBS_STATE_SYSINFO;
            return;
        }

        // "Reset score" is an empty case upstream - see the comment above.
        scbsState = SCBS_STATE_CHOOSEMAP;
        return;
    }

    if( gbPressed( BTN_C ) || gbPressed( BTN_B ) )
      scbsState = SCBS_STATE_CHOOSEMAP;
}

// == real `displaySystemInfo()`. Upstream reads battery voltage/level,
// ambient light and the backlight value straight off real hardware; none of
// those sensors exist here, so those four lines are dropped and only the
// readouts this platform genuinely has are shown. Button C exits, exactly as
// upstream.
void scbsUpdateSysInfo()
{
    gbSetColorBg( GB_BLACK, GB_WHITE );
    gbCursorX = 0;
    gbCursorY = 0;
    gbPrintString( "SYSTEM INFO\n" );
    gbPrintString( "Map: " );
    gbPrintNumber( scbsWorldMapNumber );
    gbPrintString( "\nScore: " );
    gbPrintNumber( scbsScore[ scbsWorldMapNumber ] );
    gbPrintString( "\nGuns: " );
    gbPrintNumber( scbsUnlockedWeapons + 1 );
    gbPrintString( "\nCheats: " );

    if( scbsCheatGun ) gbPrintString( "ON" );
    else               gbPrintString( "OFF" );

    if( gbPressed( BTN_C ) )
    {
        gbPlayCancel();
        scbsState = SCBS_STATE_SMENU;
    }
}

// == real `gunmenu()` (change_gun.ino) - the weapon cheat's own picker, in
// upstream's own listed order, which is NOT the same as its own weapon-id
// order (Machinegun sits ninth in the list but is a low id).
int[13] scbsGunMenuOrder =
{
    SCBS_W_PISTOL, SCBS_W_RIFLE, SCBS_W_SHOTGUN, SCBS_W_ROCKET,
    SCBS_W_CLUB, SCBS_W_REVOLVER, SCBS_W_MINE, SCBS_W_SNIPER,
    SCBS_W_MACHINEGUN, SCBS_W_GRENADE, SCBS_W_AKIMBO, SCBS_W_DISK,
    SCBS_W_LASER
};

void scbsPrintGunName( int subtype )
{
    if( subtype == SCBS_W_PISTOL )          gbPrintString( "Pistol" );
    else if( subtype == SCBS_W_RIFLE )      gbPrintString( "Rifle" );
    else if( subtype == SCBS_W_SHOTGUN )    gbPrintString( "Shotgun" );
    else if( subtype == SCBS_W_ROCKET )     gbPrintString( "Rocket" );
    else if( subtype == SCBS_W_CLUB )       gbPrintString( "Club" );
    else if( subtype == SCBS_W_REVOLVER )   gbPrintString( "Revolver" );
    else if( subtype == SCBS_W_MINE )       gbPrintString( "Mine" );
    else if( subtype == SCBS_W_SNIPER )     gbPrintString( "Sniper" );
    else if( subtype == SCBS_W_MACHINEGUN ) gbPrintString( "Machinegun" );
    else if( subtype == SCBS_W_GRENADE )    gbPrintString( "Grende" );
    else if( subtype == SCBS_W_AKIMBO )     gbPrintString( "Akimbo" );
    else if( subtype == SCBS_W_DISK )       gbPrintString( "Disk" );
    else                                    gbPrintString( "Laser" );
}

void scbsUpdateGunMenu()
{
    int first = scbsGunMenuIndex - 2;
    int i;

    if( first < 0 ) first = 0;
    if( first > 13 - 5 ) first = 13 - 5;

    gbSetColorBg( GB_BLACK, GB_WHITE );
    gbCursorX = 0;
    gbCursorY = 0;
    gbPrintString( "CHANGE GUN\n" );

    for( i = first; i < first + 5; i++ )
    {
        if( i == scbsGunMenuIndex ) gbPrintString( ">" );
        else                        gbPrintString( " " );

        scbsPrintGunName( scbsGunMenuOrder[ i ] );
        gbPrintString( "\n" );
    }

    if( gbPressed( BTN_UP ) )
      scbsGunMenuIndex = ( scbsGunMenuIndex - 1 + 13 ) % 13;

    if( gbPressed( BTN_DOWN ) )
      scbsGunMenuIndex = ( scbsGunMenuIndex + 1 ) % 13;

    if( gbPressed( BTN_A ) )
    {
        scbsWeaponSubtype = scbsGunMenuOrder[ scbsGunMenuIndex ];
        scbsWeaponInit();
        scbsState = SCBS_STATE_PAUSED;
        return;
    }

    if( gbPressed( BTN_C ) || gbPressed( BTN_B ) )
      scbsState = SCBS_STATE_PAUSED;
}

void gameSuperCrateBuinoStone_update()
{
    if( !gbUpdate() ) return;

    if( scbsState == SCBS_STATE_TITLE ) scbsUpdateTitle();
    else if( scbsState == SCBS_STATE_MAINMENU ) scbsUpdateMainMenu();
    else if( scbsState == SCBS_STATE_SMENU ) scbsUpdateSMenu();
    else if( scbsState == SCBS_STATE_SYSINFO ) scbsUpdateSysInfo();
    else if( scbsState == SCBS_STATE_GUNMENU ) scbsUpdateGunMenu();
    else if( scbsState == SCBS_STATE_CHOOSEMAP ) scbsUpdateChooseMap();
    else if( scbsState == SCBS_STATE_PLAY ) scbsUpdatePlay();
    else if( scbsState == SCBS_STATE_PAUSED ) scbsUpdatePaused();
    else scbsUpdateGameOver();

    gbRenderFrame();
}
