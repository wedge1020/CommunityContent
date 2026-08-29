#include "menu.h"

void addGames()
{
    addGame( "PONG SOLO", "AURELIEN RODOT", NULL, &gamePong_init, &gamePong_update, NULL );
    addGame( "AGARUINO", "OGBABA", NULL, &gameAgaruino_init, &gameAgaruino_update, NULL );
    addGame( "SHUFFLEPUCK CAFE", "AWOT83", NULL, &gameShufflepuck_init, &gameShufflepuck_update, NULL );
    addGame( "TAQUIN", "RACKHAMLENOIR", NULL, &gameTaquin_init, &gameTaquin_update, NULL );
    addGame( "CRAZYCAR", "BAPTISTE POUGET", NULL, &gameCrazyCar_init, &gameCrazyCar_update, NULL );
    addGame( "MAZE", "ANDY O'NEILL", NULL, &gameMaze_init, &gameMaze_update, NULL );
    addGame( "SIMONBUINO", "FORKLIFT5", NULL, &gameSimonbuino_init, &gameSimonbuino_update, NULL );
    addGame( "CONDUIT", "ADEKTO", NULL, &gameConduit_init, &gameConduit_update, NULL );
    addGame( "FLAPPY BIRDO", "FORKLIFT5", NULL, &gameFlappyBirdo_init, &gameFlappyBirdo_update, NULL );
    addGame( "PARACHUTE", "JICEHEL", NULL, &gameParachute_init, &gameParachute_update, NULL );
    addGame( "SNAKE CLASSIC", "RIPPER121 / TNXEC2", NULL, &gameSnakeClassic_init, &gameSnakeClassic_update, NULL );
    addGame( "SNAKE ABC", "FRTHERY", NULL, &gameSnakeAbc_init, &gameSnakeAbc_update, NULL );
    addGame( "FIREMEN", "VICKING69", NULL, &gameFiremen_init, &gameFiremen_update, NULL );
    addGame( "CATCHER", "QUBIST", NULL, &gameCatcher_init, &gameCatcher_update, NULL );
    addGame( "UFO RACE", "RODOT", NULL, &gameUfoRace_init, &gameUfoRace_update, NULL );
    addGame( "MINESWEEPER", "DIRKSTEINDORF", NULL, &gameMinesweeper_init, &gameMinesweeper_update, NULL );
    addGame( "KILL RACE", "YODA ZHANG", NULL, &gameKillrace_init, &gameKillrace_update, NULL );
    addGame( "BLOCKDUDE", "SORUNOME", NULL, &gameBlockdude_init, &gameBlockdude_update, NULL );
    addGame( "LANDER", "YODA ZHANG", NULL, &gameLander_init, &gameLander_update, NULL );
    addGame( "PUNKT", "ANDY O'NEILL", NULL, &gamePunkt_init, &gamePunkt_update, NULL );
    addGame( "INVADERS", "YODA ZHANG", NULL, &gameInvaders_init, &gameInvaders_update, NULL );
    addGame( "ASTEROCKS", "YODA ZHANG", NULL, &gameAsterocks_init, &gameAsterocks_update, NULL );
    addGame( "GRUNIOZERCA", "ARKADIUSZ KAMINSKI", NULL, &gameGruniozerca_init, &gameGruniozerca_update, NULL );
    addGame( "VIDEO POKER", "MIKE DEL POZZO", NULL, &gameVideoPoker_init, &gameVideoPoker_update, NULL );
    addGame( "BLOB ATTACK", "TEAM ARG", NULL, &gameBlobAttack_init, &gameBlobAttack_update, NULL );
    addGame( "SMASH AND CRASH", "SKYRUNNER65", NULL, &gameSmash_init, &gameSmash_update, NULL );
    addGame( "2048", "JOSIAH WINSLOW", NULL, &game2048_init, &game2048_update, NULL );
    addGame( "ARMAGEDDON", "WUUFF", NULL, &gameArmageddon_init, &gameArmageddon_update, NULL );
    addGame( "SKIBUINO", "MIKE DEL POZZO", NULL, &gameSkibuino_init, &gameSkibuino_update, NULL );
    addGame( "MICROHEXAGON", "VALDENTHORANAR", NULL, &gameHexagon_init, &gameHexagon_update, NULL );
    addGame( "JEZZBALL", "RACKHAMLENOIR", NULL, &gameJezzball_init, &gameJezzball_update, NULL );
    addGame( "SHIPWREK", "YAWN-G", NULL, &gameShipwrek_init, &gameShipwrek_update, NULL );
    addGame( "PAQMAN", "YODA ZHANG", NULL, &gamePaqman_init, &gamePaqman_update, NULL );
    addGame( "BLOCKSBUINO", "FRTHERY", NULL, &gameBlocksBuino_init, &gameBlocksBuino_update, NULL );
    addGame( "WORLD'S HARDEST GAME", "SORUNOME", NULL, &gameWhg_init, &gameWhg_update, NULL );
    addGame( "CRAZYTOWN", "CLEMENT QUINTARD", NULL, &gameCrazyTown_init, &gameCrazyTown_update, NULL );
    addGame( "COPTER", "CLEMENT83", NULL, &gameCopter_init, &gameCopter_update, NULL );
    addGame( "ZOMBIESCAPE", "FRAKASSS", NULL, &gameZombiEscape_init, &gameZombiEscape_update, NULL );
    addGame( "GLACIGLACA", "CLEMENT83", NULL, &gameGlaciGlaca_init, &gameGlaciGlaca_update, NULL );
    addGame( "DESCENT INTO HELL", "ETIENNE72230", NULL, &gameDescent_init, &gameDescent_update, NULL );
    addGame( "CASTLE DEFENCE", "KH9282", NULL, &gameCastleDefence_init, &gameCastleDefence_update, NULL );
    addGame( "DIGGER", "SCMAR", NULL, &gameDigger_init, &gameDigger_update, NULL );
    addGame( "SUPER SPACE SHOOTER", "MSEVILGENIUS", NULL, &gameSuperSpaceShooter_init, &gameSuperSpaceShooter_update, NULL );
    addGame( "TETRINO", "J0FF", NULL, &gameTetrino_init, &gameTetrino_update, NULL );
    addGame( "ARTILLERY", "FRAKASSS", NULL, &gameArtillery_init, &gameArtillery_update, NULL );
    addGame( "CRABATOR", "RODOT", NULL, &gameCrabator_init, &gameCrabator_update, NULL );
    addGame( "B-RALLY", "SCMAR", NULL, &gameBRally_init, &gameBRally_update, NULL );
    addGame( "MARUINO", "AJSB113", NULL, &gameMaruino_init, &gameMaruino_update, NULL );
    addGame( "SUPER CRATE BUINO", "AURELIEN RODOT", NULL, &gameSuperCrateBuino_init, &gameSuperCrateBuino_update, NULL );
    addGame( "FIREBUINO", "LADBSOFT", NULL, &gameFirebuino_init, &gameFirebuino_update, NULL );
    addGame( "101 STARSHIPS", "ZOGLU", NULL, &gameStarships101_init, &gameStarships101_update, NULL );
    addGame( "SOLITAIRE", "ANDY O'NEILL", NULL, &gameSolitaire_init, &gameSolitaire_update, NULL );
    addGame( "A TO K", "CARLOS MARI", NULL, &gameA2K_init, &gameA2K_update, NULL );
    addGame( "SOKOBUINO", "MARTINSUSTEK", NULL, &gameSokobuino_init, &gameSokobuino_update, NULL );
    addGame( "DEATHMAZE", "MSEVILGENIUS", NULL, &gameDeathMaze_init, &gameDeathMaze_update, NULL );
    addGame( "ASTEROIDRIPPER", "RIPPER121", NULL, &gameAsteroidRipper_init, &gameAsteroidRipper_update, NULL );
    markUnfinished( addGame( "BANG! BANG!", "RACKHAMLENOIR", "Player 2 never shoots", &gameBangBang_init, &gameBangBang_update, NULL ) );
    addGame( "BREAKOUT RIPPER", "RIPPER121", NULL, &gameBreakoutRipper_init, &gameBreakoutRipper_update, NULL );
    addGame( "LIGHTS OUT AD", "94K", NULL, &gameLightsOutAD_init, &gameLightsOutAD_update, NULL );
    addGame( "THUNDER SHOOT", "AWOT83", NULL, &gameThunderShoot_init, &gameThunderShoot_update, NULL );
    addGame( "BUB", "SMOGHEAP", NULL, &gameBub_init, &gameBub_update, NULL );
    addGame( "T-REX QUEST", "AWOT83", NULL, &gameTrexQuest_init, &gameTrexQuest_update, NULL );
    addGame( "SHOOTBUINO", "FRTHERY", NULL, &gameShootBuino_init, &gameShootBuino_update, NULL );
    addGame( "SENET", "MAXIMILIAN TIMMERKAMP", NULL, &gameSenet_init, &gameSenet_update, NULL );
    addGame( "BOMBER", "CLEMENT83", NULL, &gameBomber_init, &gameBomber_update, NULL );
    addGame( "STICKFIGHTER", "CLEMENT83", NULL, &gameStickFighter_init, &gameStickFighter_update, NULL );
    addGame( "TRON", "CLEMENT83", NULL, &gameTron_init, &gameTron_update, NULL );
    addGame( "SPIN SPIN SPINBUINO!", "ZOGLU", NULL, &gameSpinSpinSpinbuino_init, &gameSpinSpinSpinbuino_update, NULL );
    addGame( "SNAKE 5110", "LADY AWESOME & MAKERSQUIRREL", NULL, &gameSnake5110_init, &gameSnake5110_update, NULL );
    addGame( "PONG LOCAL MULTIPLAYER", "QUBIST", NULL, &gamePongLocalMultiplayer_init, &gamePongLocalMultiplayer_update, NULL );
    // Unfinished game - still shown/playable, reddish list text as a
    // warning, plus a real reason shown as a second info line below the
    // author credit (white normally, red when unfinished - see
    // markUnfinished()/the info field in menu.c/.h). No real death/
    // game-over condition exists anywhere in real upstream (confirmed
    // directly, not assumed - see this file's own header comment).
    markUnfinished( addGame( "SAVE PRINCESSE", "CLEMENT83", "No Dying / Game Over", &gameSavePrincesse_init, &gameSavePrincesse_update, NULL ) );
    // Unfinished game - reddish list text (see comment above). No real
    // scoring/game-over condition exists anywhere in real upstream.
    markUnfinished( addGame( "MOTOCROSS", "CLEMENT83", "No Dying / Game Over", &gameMotoCross_init, &gameMotoCross_update, NULL ) );
    // Unfinished game - reddish list text (see comment near SAVE PRINCESSE
    // above). A real, confirmed walking-and-jumping toy with no scoring/
    // enemies/objective of any kind - genuinely just an engine demo.
    markUnfinished( addGame( "NO NAME PLATFORM GAME", "FRAKASSS", "Engine Demo", &gameNoNamePlatformGame_init, &gameNoNamePlatformGame_update, NULL ) );
    addGame( "STIJN'S PONG", "STIJN CAERTS", NULL, &gameStijnPong_init, &gameStijnPong_update, NULL );
    addGame( "STIJN'S SNAKE", "STIJN CAERTS", NULL, &gameStijnSnake_init, &gameStijnSnake_update, NULL );
    addGame( "MASTER KEBAB", "OGBABA", NULL, &gameMasterKebab_init, &gameMasterKebab_update, NULL );
    addGame( "AIMBUINO", "BAPTISTE POUGET", NULL, &gameAimbuino_init, &gameAimbuino_update, NULL );
    // Unfinished game - reddish list text (see comment near SAVE PRINCESSE
    // above). Ralph himself has no real player-driven input at all beyond
    // a debug-style camera pan - confirmed by reading real upstream's own
    // `loop()` fully (see this file's own header comment).
    markUnfinished( addGame( "RALPH", "CLEMENT83", "Non interactive", &gameRalph_init, &gameRalph_update, NULL ) );
    addGame( "FOOTLOL", "BAPTISTE POUGET", NULL, &gameFootlol_init, &gameFootlol_update, NULL );
    // Unfinished game - reddish list text (see comment near SAVE PRINCESSE
    // above). Confirmed directly against real upstream (516 lines across
    // all 3 real .ino files): zero score/enemy/battle/win/lose/game-over/
    // objective content anywhere - despite the "RPG" name, it's a pure
    // walkable-overworld-plus-two-building-interiors demo with nothing to
    // actually do, the same real category as NO NAME PLATFORM GAME above.
    markUnfinished( addGame( "MYRPG", "FRAKASSS", "Engine Demo", &gameMyRpg_init, &gameMyRpg_update, NULL ) );
    addGame( "PONG 2017", "YAWN-G", NULL, &gamePong2017_init, &gamePong2017_update, NULL );
    // Unfinished game - reddish list text (see comment near SAVE PRINCESSE
    // above). No real death/game-over handling exists anywhere in real
    // upstream (confirmed by a full grep sweep - see this file's own
    // header comment).
    markUnfinished( addGame( "DARKSHMUP", "CLEMENT83", "No Dying / Game Over", &gameDarkShmup_init, &gameDarkShmup_update, NULL ) );
    // Unfinished game - reddish list text: the ball can get stuck (see
    // comment near SAVE PRINCESSE above for the markUnfinished() mechanism).
    markUnfinished( addGame( "PINBALL", "CLEMENT83", "Ball can get stuck", &gamePinball_init, &gamePinball_update, NULL ) );
    markUnfinished( addGame( "ROBOT", "FRAKASSS", "No Game Over / only 2 levels", &gameRobot_init, &gameRobot_update, NULL ) );
    addGame( "ELVENTURE", "TRODOSS", NULL, &gameElventure_init, &gameElventure_update, NULL );
    // The porter credit ("WUUFF") is a second, independent info line
    // rather than embedded in the author string via '\n' - see menu.c's
    // own credit-drawing code for why (the info field now covers this
    // use case generally, not just Star Honor's own real combined
    // "original author / porter" credit).
    addGame( "STAR HONOR", "WENCESLAO VILLANUEVA JR", "WUUFF", &gameStarHonor_init, &gameStarHonor_update, NULL );
    addGame( "PETITMONSTRE", "CLEMENT83", NULL, &gamePetitMonstre_init, &gamePetitMonstre_update, NULL );
    addGame( "UNDER THE TOWER", "WUUFF", NULL, &gameUnderTheTower_init, &gameUnderTheTower_update, NULL );
    addGame( "BIGBLACKBOX", "STUDIOCRAFTAPPS", NULL, &gameBigBlackBox_init, &gameBigBlackBox_update, NULL );
    // Unfinished game - reddish list text (see comment near SAVE PRINCESSE
    // above). No win/lose/health/score/"game over" concept of any kind
    // exists anywhere in real upstream - a genuine flythrough tech demo.
    markUnfinished( addGame( "CRUISER", "MICHAEL SPECHT", "No Dying / Game Over", &gameCruiser_init, &gameCruiser_update, NULL ) );
    addGame( "COPTERSTRIKE", "FRAKASSS", NULL, &gameCopterStrike_init, &gameCopterStrike_update, NULL );
    addGame( "XONIX", "TNXEC2", NULL, &gameXonix_init, &gameXonix_update, NULL );
    addGame( "ANOTHER 2048", "GRAFMAKULADER2TE", NULL, &gameAnother2048_init, &gameAnother2048_update, NULL );
    addGame( "DARK TOWER", "MARCUS HUTCHINGS", NULL, &gameDarkTower_init, &gameDarkTower_update, NULL );
    addGame( "GEMGEM", "TNXEC2", NULL, &gameGemgem_init, &gameGemgem_update, NULL );
    // Unfinished game - reddish list text: a genuinely unfinished tech
    // demo (lorem-ipsum dialogue, no objective, no score, no ending - see
    // comment near SAVE PRINCESSE above for the markUnfinished() mechanism).
    markUnfinished( addGame( "COMMUNITY RPG", "SORUNOME", "Unfinished game", &gameCommunityRpg_init, &gameCommunityRpg_update, NULL ) );
    addGame( "FIFTEEN", "TNXEC2", NULL, &gameFifteen_init, &gameFifteen_update, NULL );
    // Registered under the same author handle as this cartridge's other
    // already-shipped game by this real person (Markus Klingler), "ANOTHER
    // 2048" - kept consistent rather than the porting agent's own literal
    // "MARKUS KLINGLER" choice, matching this project's own established
    // one-handle-per-author convention.
    addGame( "MOLE CONTROL", "GRAFMAKULADER2TE", NULL, &gameMoleControl_init, &gameMoleControl_update, NULL );
    // The repo is named "Strike-Down", but its own README titles the game
    // "Aerial-Assault" - registered under that real title, not the repo name.
    addGame( "AERIAL-ASSAULT", "SKYLARHYLAR", NULL, &gameAerialAssault_init, &gameAerialAssault_update, NULL );
    // Not a ported upstream game - a real, in-cartridge Sound-primitive
    // diagnostic tool (see gameSoundTest.c's own header comment). Marked
    // unfinished purely to visually flag it as a different kind of entry
    // from every other, real game in this list, not because it's an
    // incomplete game.
    markUnfinished( addGame( "SOUND TEST", "WILLEMS DAVY", "Diagnostic tool, not a game", &gameSoundTest_init, &gameSoundTest_update, NULL ) );

    // New games go here, at the very bottom - registration index is what
    // md_drawGameThumbnail() keys off, so inserting earlier in this list
    // would shift every later game's own thumbnail cell.
    addGame( "R.S.G.", "DEKAY", NULL, &gameRsg_init, &gameRsg_update, NULL );
    addGame( "DISCOVERY", "FIRSTKLAAS", NULL, &gameDiscovery_init, &gameDiscovery_update, NULL );
    addGame( "BOMBERMAN", "JOHNAKERMAN", NULL, &gameBomberman_init, &gameBomberman_update, NULL );
    addGame( "BRICKS!", "DRAKKER", NULL, &gameBricks_init, &gameBricks_update, NULL );
    addGame( "WOLFENDUINO 3D", "JHHOWARD", NULL, &gameWolfenduino_init, &gameWolfenduino_update, NULL );
    addGame( "GUESS A NUMBER", "L_ET_M", NULL, &gameGuessANumber_init, &gameGuessANumber_update, NULL );
    addGame( "WALBLOKS", "BORKIN8R", NULL, &gameWalbloks_init, &gameWalbloks_update, NULL );
    addGame( "SNAKE SIGMA", "SIGMA-SQUARED", NULL, &gameSnakeSigma_init, &gameSnakeSigma_update, NULL );
    addGame( "COPTER BY ANNY", "ANNYFM", NULL, &gameCopterAnny_init, &gameCopterAnny_update, NULL );
    addGame( "SIENA TOWN", "PATRYK KUCINSKI", NULL, &gameSienaTown_init, &gameSienaTown_update, NULL );
    addGame( "SUPER CRATE BUINO STONE", "4LAR", NULL, &gameSuperCrateBuinoStone_init, &gameSuperCrateBuinoStone_update, NULL );
}
