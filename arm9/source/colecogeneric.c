// =====================================================================================
// Copyright (c) 2021-2023 Dave Bernazzani (wavemotion-dave)
//
// Copying and distribution of this emulator, its source code and associated
// readme files, with or without modification, are permitted in any medium without
// royalty provided this copyright notice is used and wavemotion-dave (Phoenix-Edition),
// Alekmaul (original port) and Marat Fayzullin (ColEM core) are thanked profusely.
//
// The ColecoDS emulator is offered as-is, without any warranty. Please see readme.md
// =====================================================================================
#include <nds.h>

#include <stdlib.h>
#include <stdio.h>
#include <fat.h>
#include <dirent.h>
#include <unistd.h>

#include "colecoDS.h"
#include "colecomngt.h"
#include "colecogeneric.h"

#include "options.h"
#include "topscreen.h"

#include "CRC32.h"
#include "printf.h"

typedef enum {FT_NONE,FT_FILE,FT_DIR} FILE_TYPE;
extern u8 bMSXBiosFound;

int countCV=0;
int ucGameAct=0;
int ucGameChoice = -1;
FICcoleco gpFic[MAX_ROMS];  
char szName[256];
char szFile[256];
u8 bForceMSXLoad = false;
u32 file_size = 0;

struct Config_t AllConfigs[MAX_CONFIGS];
struct Config_t myConfig __attribute((aligned(4))) __attribute__((section(".dtcm")));
struct GlobalConfig_t myGlobalConfig;
extern u32 file_crc;

u8 option_table=0;

const char szKeyName[MAX_KEY_OPTIONS][18] = {
  "P1 JOY UP",
  "P1 JOY DOWN",
  "P1 JOY LEFT",
  "P1 JOY RIGHT",
  "P1 BTN 1 (L/YLW)",
  "P1 BTN 2 (R/RED)",
  "P1 BTN 3 (PURP)",
  "P1 BTN 4 (BLUE)",
  "P1 KEYPAD #1",        
  "P1 KEYPAD #2",
  "P1 KEYPAD #3",        
  "P1 KEYPAD #4",
  "P1 KEYPAD #5",
  "P1 KEYPAD #6",
  "P1 KEYPAD #7",
  "P1 KEYPAD #8",
  "P1 KEYPAD #9",
  "P1 KEYPAD ##",
  "P1 KEYPAD #0",
  "P1 KEYPAD #*",
    
  "P2 JOY UP",
  "P2 JOY DOWN",
  "P2 JOY LEFT",
  "P2 JOY RIGHT",
  "P2 BTN 1 (L/YLW)",
  "P2 BTN 2 (R/RED)",
  "P2 BTN 3 (PURP)",
  "P2 BTN 4 (BLUE)",
  "P2 KEYPAD #1",        
  "P2 KEYPAD #2",
  "P2 KEYPAD #3",        
  "P2 KEYPAD #4",
  "P2 KEYPAD #5",
  "P2 KEYPAD #6",
  "P2 KEYPAD #7",
  "P2 KEYPAD #8",
  "P2 KEYPAD #9",
  "P2 KEYPAD ##",
  "P2 KEYPAD #0",
  "P2 KEYPAD #*",
    
  "SAC SPIN X+",          
  "SAC SPIN X-",          
  "SAC SPIN Y+",
  "SAC SPIN Y-",
  
  "KEYBOARD A", //45
  "KEYBOARD B",
  "KEYBOARD C",
  "KEYBOARD D",
  "KEYBOARD E",
  "KEYBOARD F",
  "KEYBOARD G",
  "KEYBOARD H",
  "KEYBOARD I",
  "KEYBOARD J",
  "KEYBOARD K",
  "KEYBOARD L",
  "KEYBOARD M",
  "KEYBOARD N",
  "KEYBOARD O",
  "KEYBOARD P",
  "KEYBOARD Q", // 60
  "KEYBOARD R",
  "KEYBOARD S",
  "KEYBOARD T",
  "KEYBOARD U",
  "KEYBOARD V",
  "KEYBOARD W",
  "KEYBOARD X",
  "KEYBOARD Y",
  "KEYBOARD Z",
    
  "KEYBOARD 0", // 70
  "KEYBOARD 1",
  "KEYBOARD 2",
  "KEYBOARD 3",
  "KEYBOARD 4",
  "KEYBOARD 5",
  "KEYBOARD 6",
  "KEYBOARD 7",
  "KEYBOARD 8",
  "KEYBOARD 9",
    
  "KEYBOARD SHIFT",
  "KEYBOARD CTRL",
  "KEYBOARD CODE",
  "KEYBOARD GRAPH",
    
  "KEYBOARD SPACE",
  "KEYBOARD RETURN",
  "KEYBOARD ESC",
    
  "KEYBOARD HOME",
  "KEYBOARD UP",
  "KEYBOARD DOWN",
  "KEYBOARD LEFT",
  "KEYBOARD RIGHT",
    
  "KEYBOARD PERIOD",
  "KEYBOARD COMMA",
  "KEYBOARD COLON",
  "KEYBOARD SEMI",
  "KEYBOARD QUOTE",
  "KEYBOARD SLASH",
  "KEYBOARD BSLASH",
  "KEYBOARD PLUS",
  "KEYBOARD MINUS",
  "KEYBOARD LBRACKET",
  "KEYBOARD RBRACKET",
  "KEYBOARD CARET",
  "KEYBOARD ASTERISK",
  "KEYBOARD ATSIGN",
  "KEYBOARD BS",
  "KEYBOARD TAB",
  "KEYBOARD INS",
  "KEYBOARD DEL",
  "KEYBOARD CLEAR",
  "KEYBOARD UNDO",
  "KEYBOARD MOVE",
  "KEYBOARD WILDCARD",
  "KEYBOARD STORE",
  "KEYBOARD PRINT",
  "KEYBOARD STOP",
  "KEYBOARD F1 (I)",
  "KEYBOARD F2 (II)",
  "KEYBOARD F3 (III)",
  "KEYBOARD F4 (IV)",
  "KEYBOARD F5 (V)",
  "KEYBOARD F6 (VI)",
  "KEYBOARD F7",
  "KEYBOARD F8",    
};

const u32 cv_no_mirror_games[] =
{
  0xc575a831,   //	2010 - The Graphic Action Game.rom
  0xfce3aa06,   //	Alcazar - The Forgotten Fortress.rom
  0x4ffb4e8c,   //	Alphabet Zoo.rom
  0x78a738af,   //	Amazing Bumpman.rom
  0x275c800e,   //	Antarctic Adventure.rom
  0x947437ec,   //	Aquattack.rom
  0x6f88fcf0,   //	Artillery Duel.rom
  0xd464e5e4,   //	BC's Quest for Tires II.rom
  0x4359a3e5,   //	BC's Quest for Tires.rom
  0x7a93c6e5,   //	Beamrider.rom
  0xdf65fc87,   //	Blockade Runner.rom
  0x9b547ba8,   //	Boulder Dash.rom
  0xb3044aa6,   //	Boulder Dash.sav
  0x829c967d,   //	Brain Strainers.rom
  0x00b37475,   //	Buck Rogers - Planet of Zoom.rom
  0x9e1fab59,   //	Bump 'n' Jump.rom
  0x91346341,   //	Burgertime.rom
  0x6af19e75,   //	Cabbage Patch Kids - Adv's in the Park.rom
  0x75b08d99,   //	Cabbage Patch Kids - Picture Show.rom
  0x5aa22d66,   //	Campaign '84.rom
  0x70f315c2,   //	Carnival.rom
  0x17edbfd4,   //	Centipede.rom
  0x030e0d48,   //	Choplifter!.rom
  0x5eeb81de,   //	Chuck Norris Superkicks.rom
  0x4e8b4c1d,   //	Congo Bongo.rom
  0x39d4215b,   //	Cosmic Avenger.rom
  0xab146fc6,   //	Cosmic Avenger.sav
  0x7c2c7c41,   //	Cosmic Crisis.rom
  0x890682b3,   //	Dam Busters, The.rom
  0xbff86a98,   //	Dance Fantasy.rom
  0x51fe49c8,   //	Decathlon.rom
  0x6cf594e5,   //	Defender.rom
  0x56c358a6,   //	Destructor.rom
  0x3c77198c,   //	Donkey Kong [16K].rom
  0x94c4cd4a,   //	Donkey Kong [24K].rom
  0xfd11508d,   //	Donkey Kong Junior.rom
  0xdcb8f9e8,   //	DragonFire.rom
  0x109699e2,   //	Dr. Seuss Fix-Up the Mix-Up Puzzler.rom
  0x4025ac94,   //	Dukes of Hazzard, The.rom
  0x4cd4e185,   //	Evolution.rom
  0xec9dfb07,   //	Facemaker.rom
  0xb3b767ae,   //	Fathom.rom
  0x2a49f507,   //	Flipper Slipper.rom
  0x65bbbcb4,   //	Fortune Builder.rom
  0x964db3bc,   //	Fraction Fever.rom
  0x8615c6e8,   //	Frantic Freddy.rom
  0x3cacddfb,   //	Frenzy.rom
  0x4229ea5a,   //	Frogger II - Threeedeep!.rom
  0x798002a2,   //	Frogger.rom
  0xd0110137,   //	Front Line.rom
  0x6a7a162a,   //	Galaxian.rom
  0xfdb75be6,   //	Gateway to Apshai.rom
  0x66c725ec,   //	Gateway to Apshai.sav
  0xff46becc,   //	Gorf.rom
  0x29fea563,   //	Gust Buster.rom
  0x27cbf26d,   //	Gyruss.rom
  0x987491ce,   //	Heist, The.rom
  0x685ab9b5,   //	H.E.R.O..rom
  0x2787516d,   //	Illusions.rom
  0xcc43ebf7,   //	It's Only Rock 'n' Roll.rom
  0xa3205f21,   //	James Bond 007.rom
  0xa5511418,   //	Jukebox.rom
  0x060c69e8,   //	Jumpman Junior.rom
  0x58e886d2,   //	Jungle Hunt.rom
  0x8c7b7803,   //	Ken Uston Blackjack-Poker.rom
  0x69fc2966,   //	Keystone Kapers.rom
  0x2c3097b8,   //	Lady Bug.rom
  0x1641044f,   //	Learning with Leeper.rom
  0x918f12c0,   //	Linking Logic.rom
  0xd54c581b,   //	Logic Levels.rom
  0x6a2d637b,   //	Looping.rom
  0xbab520ea,   //	Memory Manor.rom
  0x6a162c7d,   //	Meteoric Shower.rom
  0xb24f10fd,   //	Miner 2049er.rom
  0xd92b09c8,   //	Monkey Academy.rom
  0xced0d16e,   //	Montezuma's Revenge.rom
  0x8a303f5a,   //	Moonsweeper.rom
  0x0f4d800a,   //	Motocross Racer.rom
  0xc173bbec,   //	Mountain King.rom
  0xde47c29f,   //	Mouse Trap.rom
  0x461ab6ad,   //	Mr. Do!.rom
  0x546f2c54,   //	Mr. Do!'s Castle.rom
  0x4491a35b,   //	Nova Blast.rom
  0xadd10242,   //	Oil's Well.rom
  0x0a0511c7,   //	Omega Race.rom
  0xef50e1c5,   //	One-on-One.rom
  0x53b85e20,   //	Pepper II.rom
  0x851a455f,   //	Pitfall II - Lost Caverns.rom
  0xd4f180df,   //	Pitfall!.rom
  0x81be4f55,   //	Pitstop.rom
  0xa095454d,   //	Popeye.rom
  0x532f61ba,   //	Q-bert.rom
  0x79d90d26,   //	Q-Bert's QUBES.rom
  0xeec81c42,   //	Quest for Quintana Roo.rom
  0xfe897f9c,   //	River Raid.rom
  0xcdb8d3cf,   //	Robin Hood.rom
  0xaf49a0c3,   //	Rock 'n Bolt.rom
  0x37f144d3,   //	Rocky - Super Action Boxing.rom
  0xaf885d76,   //	Roc 'n Rope.rom
  0x0364fb81,   //	Rolloverture.rom
  0x3563286b,   //	Sammy Lightfoot.rom
  0x0ec63a5c,   //	Sector Alpha.rom
  0x4591f393,   //	Sewer Sam.rom
  0xdf84ffb5,   //	Sir Lancelot.rom
  0xb37d48bd,   //	Skiing.rom
  0x53d2651c,   //	Slither.rom
  0x8871e9fd,   //	Slurpy.rom
  0x97c0382d,   //	Smurf Paint 'n' Play Workshop.rom
  0x42850379,   //	Smurf Rescue in Gargamel's Castle.rom
  0xdf8de30f,   //	Space Fury.rom
  0x5bdf2997,   //	Space Panic.rom
  0xa6401c46,   //	Spectron.rom
  0x36478923,   //	Spy Hunter.rom
  0xac92862d,   //	Squish'em Featuring Sam.rom
  0x95824f1e,   //	Star Trek - Strategic Operations Sim.rom
  0x0e75b3bf,   //	Star Wars - The Arcade Game.rom
  0xd247a1c8,   //	Strike It!.rom
  0xa417f25f,   //	Subroc.rom
  0xb3bcf3f9,   //	Super Action Baseball.rom
  0x62619dc0,   //	Super Action Football.rom
  0xf84622d2,   //	Super Cobra.rom
  0x84350129,   //	Super Cross Force.rom
  0xaf6bbc7e,   //	Tank Wars.rom
  0x8afd7db2,   //	Tapper.rom
  0x7cd7a702,   //	Tarzan.rom
  0xc8bc1950,   //	Telly Turtle.rom
  0x1593f7df,   //	Threshold.rom
  0xb3a1eacb,   //	Time Pilot.rom
  0x1e14397e,   //	TOMARC The Barbarian.rom
  0xc1d5a702,   //	Tournament Tennis.rom
  0xbd6ab02a,   //	Turbo.rom
  0x0408f58c,   //	Tutankham.rom
  0xfdd52ca0,   //	Up'n Down.rom
  0x8e5a4aa3,   //	Venture.rom
  0x70142655,   //	Victory.rom
  0xfd25adb3,   //	War Games.rom
  0x261b7d56,   //	War Room.rom
  0x29ec00c9,   //	Wing War.rom
  0xb9ba2bb6,   //	Wizard of Id's Wizmath.rom
  0xf7a29c01,   //	Word Feud.rom
  0x8cb0891a,   //	Zaxxon.rom
  0x6e523e50,   //	Zenji.rom
  0x3678ab6f,   //  Escape from the Mindmaster
  0xc2e7f0e0,   //  Super DK Junior
  0xef25af90,   //  Super DK
  0xa40a07e8,   //  Pac-Man
  0x1038e0a1,   //  Dig-Dug
  0x2ebf88c4,   //  Power Lords - Quest for Volcan
  0x1bed9c5b,   //  Fall Guy 
  0xdb845695,  //	AE - Anti Environment Encounter (AE2012).rom
  0x5ba1a6c8,  //	Amazing Snake (2001) (Serge-Eric Tremblay).rom
  0xa66e5ed1,  //	Antarctic Adventure (1984) (Konami) (Prototype).rom
  0xaf86d22c,  //	AntiISDA Warrior (2004) (Ventzislav Tzvetkov).rom
  0xead5e824,  //	Arno Dash (2021).rom
  0x71d9d422,  //	Bagman (2015) (CollectorVision).rom
  0x223e7ddc,  //	Bank Panic (2011).rom
  0xdddd1396,  //	Black Onyx (2013).rom
  0xa399d4cd,  //	Bomb Jack.rom
  0x924a7d57,  //	booming_boy_cv_sgm.rom
  0x88d0a96f,  //	BTshort.rom
  0xb278ebde,  //	Burn Rubber (2012).rom
  0x53672097,  //	caos_begins_colecovision_sgm.rom
  0x10e6e6de,  //	Caterpillar (2011).rom
  0x4928b5f5,  //	Caverns of Titan.rom
  0x045c54d6,  //	champion-pro-wrestling-2020.rom
  0x55b36d53,  //	children_of_the_night_cv_sgm.rom
  0xbe3ef785,  //	Circus Charlie (2011).rom
  0x960f7086,  //	cold_blood_colecovision_sgm.rom
  0x43933306,  //	Cosmo Challenge (1997) (Red Bullet Software).rom
  0x03f9e365,  //	Cosmo Fighter 2 (1997) (Red Bullet Software).rom
  0xb4b9301f,  //	Cosmo Fighter 3 (2002) (Red Bullet Software).rom
  0xb248cd72,  //	C-SO (2018).rom
  0x3c0bba92,  //	DacMan (2000) (NewColeco).rom
  0x0246bdb1,  //	danger_tower_colecovision_sgm.rom
  0x1b3a8639,  //	Deflektor Kollection (2005) (NewColeco).rom
  0x2a9fcbfa,  //	Destructor Standard Controller Edition (2010).rom
  0x5576dec3,  //	Diamond Dash 2 (2021).rom
  0x77088cab,  //	digger_cv.rom
  0x12ceee08,  //	dragons_lair_colecovision_sgm.rom
  0x70d55091,  //	dungeonandtrolls.rom
  0x8d1b3636,  //	Explosion (2003) (NewColeco).rom
  0x48ff87cf,  //	Final Test Cartridge (19xx).col
  0x5ac80811,  //	flapee-byrd-2014.rom
  0x89875c52,  //	Flicky (2018).rom
  0xfd69012b,  //	Flora and the Ghost Mirror (2013) (NewColeco).rom
  0xf43b0b28,  //	Frantic (2008) (Scott Huggins).rom
  0xcb941eb4,  //	Frog Feast (2007).rom
  0x832586bf,  //	Frontline - Standard Controller Edition.rom
  0xb3212318,  //	Frostbite (2017).rom
  0x9f1045e6,  //	Galaga RevA.rom
  0x9a02fba5,  //	GamePack I (2002) (NewColeco).rom
  0xeab0ddd6,  //	GamePack II (2004) (NewColeco).rom
  0x5b4ad168,  //	GamePack - Vic-20 (2003) (NewColeco).rom
  0x652d533e,  //	gauntlet-2019.rom
  0x312980d5,  //	GhostBlaster (2010) (NewColeco) (Rev. A NTSC).rom
  0xfc935cdd,  //	ghostbusters-2018.rom
  0x1e79958a,  //	Girls Garden (2010).rom
  0x8acf1bcf,  //	Golgo 13 (2011) (Team PixelBoy).rom
  0x01581fa8,  //	goonies_colecovision_sgm.rom
  0xb2fb36a9,  //	GrailoftheGodbugfix2.rom
  0x81413ff6,  //	Gulkave (2010) (Team Pixelboy).rom
  0x27818d93,  //	Hang-On (2016) (CollectorVision) (SGM).rom
  0x9b891703,  //	helifire.rom
  0x87a49761,  //	heroes_arena_colecovision_sgm.rom
  0x5a6c2d2f,  //	Insane Pickin' Sticks VIII (2010) (NewColeco).rom
  0x69e3c673,  //	Jeepers Creepers - 30th Anniversary (2012) (NewColeco).rom
  0xae7614c3,  //	j-e-t-p-a-c-2017.rom
  0x62f325b3,  //	Joust (2014) (Team Pixelboy).rom
  0xf403fe04,  //	Jump or Die (2006) (NewColeco).rom
  0x5287998b,  //	Kaboom (2017) (Team Pixelboy).rom
  0x819a06e5,  //	Kevtris (1996) (Kevin Horton).rom
  0x9ce3b912,  //	king-balloon-2018.rom
  0x0a2ac883,  //	kings_valley_colecovision_sgm.rom
  0x278c5021,  //	klondike-solitaire-2021.rom
  0x1523bfbf,  //	knight_lore_colecovision_sgm.rom
  0x01cacd0d,  //	knightmare_colecovision_sgm.rom
  0x344cb482,  //	Konami's Ping Pong (2011) (Team PixelBoy).rom
  0xe8a91349,  //	kralizec_tetris_colecovision.rom
  0xa078f273,  //	Kung-Fu Master - 2016 (CollectorVision).rom
  0x6bc9a350,  //	Lock-n-Chase.rom
  0xa2128f74,  //	Lode Runner (2002) (Steve Begin).rom
  0xf4314bb1,  //	Magical Tree (2006).rom
  0x6ed6a2e1,  //	mahjong-solitaire-2021.rom
  0x2a1438c0,  //	Majikazo (2016).rom
  0x00d30431,  //	Mappy (2015) (Team Pixelboy) (SGM).rom
  0x11777b27,  //	mario-brothers-2009.rom
  0x90007079,  //	Matt Patrol (1984) (Atarisoft) (Prototype).rom
  0x44bfaf23,  //	Maze Maniac (2006) (Charles-Mathieu Boyer).rom
  0x53da40bc,  //	mecha_8_colecovision_sgm.rom
  0xb405591a,  //	mecha9_colecovision_sgm.rom
  0x5cd9d34a,  //	minesweeper-2021.rom
  0xdd730dbd,  //	missle-strike-2021.rom
  0x4e0edb24,  //	Miss Space Fury (2001).rom
  0xeb6be8ec,  //	Module Man (2013).rom
  0x4514d3f0,  //	Moon Patrol - Ikrananka - Final - WIP.rom
  0xac3cb427,  //	Moon Patrol Prototype.rom
  0x9ab11795,  //	Mr Chin (2008).rom
  0xd45475ac,  //	Multiverse (2019).rom
  0x9c059a51,  //	Nim (2000) (NewColeco).rom
  0x5fb0ed62,  //	Ninja Princess (2011).rom
  0xc177bfd4,  //	operation_wolf_colecovision_sgm.rom
  0x4aafdc07,  //	ozmawars.rom
  0xa82c9593,  //	Pacar (2017).rom
  0xf3ccacb3,  //	PAC_CV_101.ROM
  0xf3ccacb3,  //	Pac-Man Collection (2008 Opcode).rom
  0x18aced43,  //	pang.rom
  0xb47377fd,  //	pegged-2021.rom
  0x5a49b249,  //	pillars-2021.rom
  0x519bfe40,  //	Pitfall II Arcade (2010 Team Pixelboy).rom
  0x50998610,  //	pitman-2021.rom
  0x0e25ebd7,  //	Pooyan (2009).rom
  0xa59eaa2b,  //	Princess Quest (2012).rom
  0xf7052b06,  //	Pyramid Warp Battleship Clapton-2-2009.rom
  0xee530ad2,  //	qbiqs_cv_sgm.rom
  0x6da37da8,  //	Quest for the Golden Chalice (2012) (Team PixelBoy).rom
  0xfb5dd80d,  //	rallyx_colecovision_sgm.rom
  0x7e090dfb,  //	Remember the Flag (2017).rom
  0x2331b6f6,  //	reversi-and-diamond-dash-2004.rom
  0xd8caac4c,  //	RipCord (2016) (CollectorVision).rom
  0x570b9935,  //	Road Fighter (2007) (Opcode Games).rom
  0x99e66988,  //	Rockcutter32Kv.rom
  0xeaf7d6dc,  //	searchforthestolencrownjewels2.rom
  0x594b1235,  //	searchforthestolencrownjewels3.rom
  0xe339d9cc,  //	searchforthestolencrownjewels.rom
  0xb753a8ca,  //	secret_of_the_moai_cv_sgm.rom
  0x10e8dd09,  //	Shmup.rom
  0x89c2b16f,  //	Shouganai.rom
  0x2e4c28e2,  //	Side Trak (2012) (CollectorVision).rom
  0x54d54968,  //	Sky Jaguar (1984) (Konami-Opcode).rom
  0x92624cff,  //	Slither - Controller Hack (2010) (Daniel Bienvenu-NewColeco).rom
  0x9e1bea35,  //	Space Caverns (2004) (Scott Huggins) (Prototype).rom
  0x9badaa20,  //	Space Hunter (2005) (Guy Foster).rom
  0xec76ee9e,  //	Space Invaders Collection (full).rom
  0xaf9b178c,  //	Space Invasion (1998 John Dondzilla).rom
  0xfe521268,  //	Space Trainer (2005) (NewColeco).rom
  0xf7ed24e9,  //	Space_Venture_v3.1_(color).rom
  0x75f84889,  //	spelunker_colecovision_sgm.rom
  0xdeed811e,  //	spunky-s-super-car-2014.rom
  0xea3aa29e,  //	star-fire-2021.rom
  0xce3ff5c7,  //	Star Force (2011) (Patched).rom
  0xf7f18e6f,  //	Star Fortress (1997) (John Dondzila).rom
  0x08e7df91,  //	StarOcean.rom
  0x3e7d0520,  //	Star Soldier (2016) (CollectorVision) (SGM).rom
  0x153ac4ef,  //	Steamroller (1984) (Activision) (Prototype).rom
  0x342c73ca,  //	stone_of_wisdom_colecovision_sgm.rom
  0xf55f499e,  //	stray_cat_colecovision_sgm.rom
  0xb5be3448,  //	sudoku_colecovision.rom
  0xec76729e,  //	Super Action Soccer (2012) (Team PixelBoy).rom
  0x260cdf98,  //	super_pac_man_colecovision_sgm.rom
  0x1457c897,  //	Super Sonyk Arena v1.6 (Gameblabla).col
  0xae209065,  //	Super Space Acer (2011).rom
  0x81bfb02d,  //	Sydney Hunter and the Caverns of Death (2019) (CollectorVision).rom
  0xb5c92637,  //	Sydney Hunter and The Sacred Tribe (2017) (CollectorVision).rom
  0xd195e199,  //	Terra Attack (2007) (Scott Huggins).rom
  0x09e3fdda,  //	thexder_colecovision_sgm.rom
  0xa8b4b159,  //	Track and Field-2010.rom
  0xaad0f224,  //	traffic_jam_colecovision_sgm.rom
  0x6a0b954a,  //	Turbo Standard Controller Hack.rom
  0xe7e07a70,  //	twinbee_colecovision_sgm.rom
  0x28bdf665,  //	Txupinazo.rom
  0xbc8320a0,  //	uridium-2019.rom
  0xa7a8d25e,  //	Vanguard (2019) (CollectorVision).rom
  0x530c586f,  //	vexed-2021.rom
  0x4157b347,  //	Victory - Standard Controller Hack.rom
  0x7f06e25c,  //	War (2014).rom
  0xd642fb9e,  //	Waterville Rescue (2009).rom
  0xd9207f30,  //	Wizard of Wor (SGM).rom
  0x43505be0,  //	Wonder Boy (2012).rom
  0x471240bb,  //	Yie Ar Kung-fu (2005).rom
  0xe290a941,  //	Zanac (2015) (CollectorVision) (SGM).rom
  0xa5a90f63,  //	zaxxon_super_game_colecovision_sgm.rom
  0x44e6948c,  //	Zippy Race (2009).rom
  0x8027dad7,  //	zombie_incident_cv_sgm.rom
  0xc89d281d,  //	zombie-near-2012.rom
  0xFFFFFFFF,   //	End of list
};

/*********************************************************************************
 * Show A message with YES / NO
 ********************************************************************************/
u8 showMessage(char *szCh1, char *szCh2) {
  u16 iTx, iTy;
  u8 uRet=ID_SHM_CANCEL;
  u8 ucGau=0x00, ucDro=0x00,ucGauS=0x00, ucDroS=0x00, ucCho = ID_SHM_YES;
    
  BottomScreenOptions();

  DSPrint(16-strlen(szCh1)/2,10,6,szCh1);
  DSPrint(16-strlen(szCh2)/2,12,6,szCh2);
  DSPrint(8,14,6,("> YES <"));
  DSPrint(20,14,6,("  NO   "));
  while ((keysCurrent() & (KEY_TOUCH | KEY_LEFT | KEY_RIGHT | KEY_A ))!=0);

  while (uRet == ID_SHM_CANCEL) 
  {
    WAITVBL;
    if (keysCurrent() & KEY_TOUCH) {
      touchPosition touch;
      touchRead(&touch);
      iTx = touch.px;
      iTy = touch.py;
      if ( (iTx>8*8) && (iTx<8*8+7*8) && (iTy>14*8-4) && (iTy<15*8+4) ) {
        if (!ucGauS) {
          DSPrint(8,14,6,("> YES <"));
          DSPrint(20,14,6,("  NO   "));
          ucGauS = 1;
          if (ucCho == ID_SHM_YES) {
            uRet = ucCho;
          }
          else {
            ucCho  = ID_SHM_YES;
          }
        }
      }
      else
        ucGauS = 0;
      if ( (iTx>20*8) && (iTx<20*8+7*8) && (iTy>14*8-4) && (iTy<15*8+4) ) {
        if (!ucDroS) {
          DSPrint(8,14,6,("  YES  "));
          DSPrint(20,14,6,("> NO  <"));
          ucDroS = 1;
          if (ucCho == ID_SHM_NO) {
            uRet = ucCho;
          }
          else {
            ucCho = ID_SHM_NO;
          }
        }
      }
      else
        ucDroS = 0;
    }
    else {
      ucDroS = 0;
      ucGauS = 0;
    }
    
    if (keysCurrent() & KEY_LEFT){
      if (!ucGau) {
        ucGau = 1;
        if (ucCho == ID_SHM_YES) {
          ucCho = ID_SHM_NO;
          DSPrint(8,14,6,("  YES  "));
          DSPrint(20,14,6,("> NO  <"));
        }
        else {
          ucCho  = ID_SHM_YES;
          DSPrint(8,14,6,("> YES <"));
          DSPrint(20,14,6,("  NO   "));
        }
        WAITVBL;
      } 
    }
    else {
      ucGau = 0;
    }  
    if (keysCurrent() & KEY_RIGHT) {
      if (!ucDro) {
        ucDro = 1;
        if (ucCho == ID_SHM_YES) {
          ucCho  = ID_SHM_NO;
          DSPrint(8,14,6,("  YES  "));
          DSPrint(20,14,6,("> NO  <"));
        }
        else {
          ucCho  = ID_SHM_YES;
          DSPrint(8,14,6,("> YES <"));
          DSPrint(20,14,6,("  NO   "));
        }
        WAITVBL;
      } 
    }
    else {
      ucDro = 0;
    }  
    if (keysCurrent() & KEY_A) {
      uRet = ucCho;
    }
  }
  while ((keysCurrent() & (KEY_TOUCH | KEY_LEFT | KEY_RIGHT | KEY_A ))!=0);
  
  BottomScreenKeypad();  // Could be generic or overlay...
  
  return uRet;
}

void colecoDSModeNormal(void) {
  REG_BG3CNT = BG_BMP8_256x256;
  REG_BG3PA = (1<<8); 
  REG_BG3PB = 0;
  REG_BG3PC = 0;
  REG_BG3PD = (1<<8);
  REG_BG3X = 0;
  REG_BG3Y = 0;
}

//*****************************************************************************
// Put the top screen in refocused bitmap mode
//*****************************************************************************
void colecoDSInitScreenUp(void) {
  videoSetMode(MODE_5_2D | DISPLAY_BG3_ACTIVE);
  vramSetBankA(VRAM_A_MAIN_BG_0x06000000);
  vramSetBankB(VRAM_B_MAIN_SPRITE);
  colecoDSModeNormal();
}

// ----------------------------------------------------------------------------
// This stuff handles the 'random' screen snapshot at the top screen...
// ----------------------------------------------------------------------------
void showRandomPreviewSnaps(void) {
  u16 *pusEcran=(u16*) bgGetMapPtr(bg1);
  u32 uX,uY;

  if (vusCptVBL>=5*60) {
    u8 uEcran = rand() % 6;
    vusCptVBL = 0;
    if (uEcran>2) {
      uEcran-=3;
      for (uY=24;uY<33;uY++) {
        for (uX=0;uX<12;uX++) {
          *(pusEcran + (14+uX) + ((10+uY-24)<<5)) = *(bgGetMapPtr(bg0) + (uY+uEcran*9)*32 + uX+12);
        }
      }
    }
    else
    {
      for (uY=24;uY<33;uY++) {
        for (uX=0;uX<12;uX++) {
          *(pusEcran + (14+uX) + ((10+uY-24)<<5)) = *(bgGetMapPtr(bg0) + (uY+uEcran*9)*32 + uX);
        }
      }
    }
  }
}

/*********************************************************************************
 * Show The 14 games on the list to allow the user to choose a new game.
 ********************************************************************************/
void dsDisplayFiles(u16 NoDebGame, u8 ucSel) 
{
  u16 ucBcl,ucGame;
  u8 maxLen;
  char szName2[40];
  
  DSPrint(30,8,0,(NoDebGame>0 ? "<" : " "));
  DSPrint(30,21,0,(NoDebGame+14<countCV ? ">" : " "));
  siprintf(szName,"%03d/%03d FILES AVAILABLE     ",ucSel+1+NoDebGame,countCV);
  DSPrint(2,6,0, szName);
  for (ucBcl=0;ucBcl<14; ucBcl++) {
    ucGame= ucBcl+NoDebGame;
    if (ucGame < countCV) 
    {
      maxLen=strlen(gpFic[ucGame].szName);
      strcpy(szName,gpFic[ucGame].szName);
      if (maxLen>28) szName[28]='\0';
      if (gpFic[ucGame].uType == DIRECT) {
        siprintf(szName2, " %s]",szName);
        szName2[0]='[';
        siprintf(szName,"%-28s",szName2);
        DSPrint(1,8+ucBcl,(ucSel == ucBcl ? 2 :  0),szName);
      }
      else {
        siprintf(szName,"%-28s",strupr(szName));
        DSPrint(1,8+ucBcl,(ucSel == ucBcl ? 2 : 0 ),szName);
      }
    }
    else
    {
        DSPrint(1,8+ucBcl,(ucSel == ucBcl ? 2 : 0 ),"                            ");
    }
  }
}


// -------------------------------------------------------------------------
// Standard qsort routine for the coleco games - we sort all directory
// listings first and then a case-insenstive sort of all games.
// -------------------------------------------------------------------------
int colecoFilescmp (const void *c1, const void *c2) 
{
  FICcoleco *p1 = (FICcoleco *) c1;
  FICcoleco *p2 = (FICcoleco *) c2;

  if (p1->szName[0] == '.' && p2->szName[0] != '.')
      return -1;
  if (p2->szName[0] == '.' && p1->szName[0] != '.')
      return 1;
  if ((p1->uType == DIRECT) && !(p2->uType == DIRECT))
      return -1;
  if ((p2->uType == DIRECT) && !(p1->uType == DIRECT))
      return 1;
  return strcasecmp (p1->szName, p2->szName);        
}

/*********************************************************************************
 * Find files (COL / ROM) available - sort them for display.
 ********************************************************************************/
void colecoDSFindFiles(void) 
{
  u32 uNbFile;
  DIR *dir;
  struct dirent *pent;

  uNbFile=0;
  countCV=0;

  dir = opendir(".");
  while (((pent=readdir(dir))!=NULL) && (uNbFile<MAX_ROMS)) 
  {
    strcpy(szFile,pent->d_name);
      
    if(pent->d_type == DT_DIR) 
    {
      if (!( (szFile[0] == '.') && (strlen(szFile) == 1))) 
      {
        strcpy(gpFic[uNbFile].szName,szFile);
        gpFic[uNbFile].uType = DIRECT;
        uNbFile++;
        countCV++;
      }
    }
    else {
      if ((strlen(szFile)>4) && (strlen(szFile)<(MAX_ROM_LENGTH-4)) ) {
        if ( (strcasecmp(strrchr(szFile, '.'), ".rom") == 0) )  {
          strcpy(gpFic[uNbFile].szName,szFile);
          gpFic[uNbFile].uType = COLROM;
          uNbFile++;
          countCV++;
        }
        if ( (strcasecmp(strrchr(szFile, '.'), ".col") == 0) )  {
          strcpy(gpFic[uNbFile].szName,szFile);
          gpFic[uNbFile].uType = COLROM;
          uNbFile++;
          countCV++;
        }
        if ( (strcasecmp(strrchr(szFile, '.'), ".sg") == 0) )  {
          strcpy(gpFic[uNbFile].szName,szFile);
          gpFic[uNbFile].uType = COLROM;
          uNbFile++;
          countCV++;
        }
        if ( (strcasecmp(strrchr(szFile, '.'), ".sc") == 0) )  {
          strcpy(gpFic[uNbFile].szName,szFile);
          gpFic[uNbFile].uType = COLROM;
          uNbFile++;
          countCV++;
        }
        if ( (strcasecmp(strrchr(szFile, '.'), ".pv") == 0) )  {
          strcpy(gpFic[uNbFile].szName,szFile);
          gpFic[uNbFile].uType = COLROM;
          uNbFile++;
          countCV++;
        }
        if ( (strcasecmp(strrchr(szFile, '.'), ".m5") == 0) )  {
          strcpy(gpFic[uNbFile].szName,szFile);
          gpFic[uNbFile].uType = COLROM;
          uNbFile++;
          countCV++;
        }
        if ( (strcasecmp(strrchr(szFile, '.'), ".mtx") == 0) )  {
          strcpy(gpFic[uNbFile].szName,szFile);
          gpFic[uNbFile].uType = COLROM;
          uNbFile++;
          countCV++;
        }
        if ( (strcasecmp(strrchr(szFile, '.'), ".run") == 0) )  {
          strcpy(gpFic[uNbFile].szName,szFile);
          gpFic[uNbFile].uType = COLROM;
          uNbFile++;
          countCV++;
        }
        if ( (strcasecmp(strrchr(szFile, '.'), ".msx") == 0) )  {
          strcpy(gpFic[uNbFile].szName,szFile);
          gpFic[uNbFile].uType = COLROM;
          uNbFile++;
          countCV++;
        }
        if ( (strcasecmp(strrchr(szFile, '.'), ".cas") == 0) )  {
          strcpy(gpFic[uNbFile].szName,szFile);
          gpFic[uNbFile].uType = COLROM;
          uNbFile++;
          countCV++;
        }
        if ( (strcasecmp(strrchr(szFile, '.'), ".ddp") == 0) )  {
          strcpy(gpFic[uNbFile].szName,szFile);
          gpFic[uNbFile].uType = COLROM;
          uNbFile++;
          countCV++;
        }
        if ( (strcasecmp(strrchr(szFile, '.'), ".dsk") == 0) )  {
          strcpy(gpFic[uNbFile].szName,szFile);
          gpFic[uNbFile].uType = COLROM;
          uNbFile++;
          countCV++;
        }
        if ( (strcasecmp(strrchr(szFile, '.'), ".pen") == 0) )  {
          strcpy(gpFic[uNbFile].szName,szFile);
          gpFic[uNbFile].uType = COLROM;
          uNbFile++;
          countCV++;
        }
        if ( (strcasecmp(strrchr(szFile, '.'), ".com") == 0) )  {
          strcpy(gpFic[uNbFile].szName,szFile);
          gpFic[uNbFile].uType = COLROM;
          uNbFile++;
          countCV++;
        }
        if ( (strcasecmp(strrchr(szFile, '.'), ".cv") == 0) )  {
          strcpy(gpFic[uNbFile].szName,szFile);
          gpFic[uNbFile].uType = COLROM;
          uNbFile++;
          countCV++;
        }
      }
    }
  }
  closedir(dir);
    
  // ----------------------------------------------
  // If we found any files, go sort the list...
  // ----------------------------------------------
  if (countCV)
  {
    qsort (gpFic, countCV, sizeof(FICcoleco), colecoFilescmp);
  }    
}


// ----------------------------------------------------------------
// Let the user select a new game (rom) file and load it up!
// ----------------------------------------------------------------
u8 colecoDSLoadFile(void) 
{
  bool bDone=false;
  u16 ucHaut=0x00, ucBas=0x00,ucSHaut=0x00, ucSBas=0x00, romSelected= 0, firstRomDisplay=0,nbRomPerPage, uNbRSPage;
  s16 uLenFic=0, ucFlip=0, ucFlop=0;

  // Show the menu...
  while ((keysCurrent() & (KEY_TOUCH | KEY_START | KEY_SELECT | KEY_A | KEY_B))!=0);
  unsigned short dmaVal =  *(bgGetMapPtr(bg0b) + 24*32);
  dmaFillWords(dmaVal | (dmaVal<<16),(void*) bgGetMapPtr(bg1b)+5*32*2,32*19*2);
  DSPrint(7,5,0,"A=SELECT,  B=EXIT");

  colecoDSFindFiles();
    
  ucGameChoice = -1;

  nbRomPerPage = (countCV>=14 ? 14 : countCV);
  uNbRSPage = (countCV>=5 ? 5 : countCV);
  
  if (ucGameAct>countCV-nbRomPerPage)
  {
    firstRomDisplay=countCV-nbRomPerPage;
    romSelected=ucGameAct-countCV+nbRomPerPage;
  }
  else
  {
    firstRomDisplay=ucGameAct;
    romSelected=0;
  }
  dsDisplayFiles(firstRomDisplay,romSelected);
    
  // -----------------------------------------------------
  // Until the user selects a file or exits the menu...
  // -----------------------------------------------------
  while (!bDone)
  {
    if (keysCurrent() & KEY_UP)
    {
      if (!ucHaut)
      {
        ucGameAct = (ucGameAct>0 ? ucGameAct-1 : countCV-1);
        if (romSelected>uNbRSPage) { romSelected -= 1; }
        else {
          if (firstRomDisplay>0) { firstRomDisplay -= 1; }
          else {
            if (romSelected>0) { romSelected -= 1; }
            else {
              firstRomDisplay=countCV-nbRomPerPage;
              romSelected=nbRomPerPage-1;
            }
          }
        }
        ucHaut=0x01;
        dsDisplayFiles(firstRomDisplay,romSelected);
      }
      else {

        ucHaut++;
        if (ucHaut>10) ucHaut=0;
      }
      uLenFic=0; ucFlip=-50; ucFlop=0;     
    }
    else
    {
      ucHaut = 0;
    }
    if (keysCurrent() & KEY_DOWN)
    {
      if (!ucBas) {
        ucGameAct = (ucGameAct< countCV-1 ? ucGameAct+1 : 0);
        if (romSelected<uNbRSPage-1) { romSelected += 1; }
        else {
          if (firstRomDisplay<countCV-nbRomPerPage) { firstRomDisplay += 1; }
          else {
            if (romSelected<nbRomPerPage-1) { romSelected += 1; }
            else {
              firstRomDisplay=0;
              romSelected=0;
            }
          }
        }
        ucBas=0x01;
        dsDisplayFiles(firstRomDisplay,romSelected);
      }
      else
      {
        ucBas++;
        if (ucBas>10) ucBas=0;
      }
      uLenFic=0; ucFlip=-50; ucFlop=0;     
    }
    else {
      ucBas = 0;
    }
      
    // -------------------------------------------------------------
    // Left and Right on the D-Pad will scroll 1 page at a time...
    // -------------------------------------------------------------
    if (keysCurrent() & KEY_RIGHT)
    {
      if (!ucSBas)
      {
        ucGameAct = (ucGameAct< countCV-nbRomPerPage ? ucGameAct+nbRomPerPage : countCV-nbRomPerPage);
        if (firstRomDisplay<countCV-nbRomPerPage) { firstRomDisplay += nbRomPerPage; }
        else { firstRomDisplay = countCV-nbRomPerPage; }
        if (ucGameAct == countCV-nbRomPerPage) romSelected = 0;
        ucSBas=0x01;
        dsDisplayFiles(firstRomDisplay,romSelected);
      }
      else
      {
        ucSBas++;
        if (ucSBas>10) ucSBas=0;
      }
      uLenFic=0; ucFlip=-50; ucFlop=0;     
    }
    else {
      ucSBas = 0;
    }
      
    // -------------------------------------------------------------
    // Left and Right on the D-Pad will scroll 1 page at a time...
    // -------------------------------------------------------------
    if (keysCurrent() & KEY_LEFT)
    {
      if (!ucSHaut)
      {
        ucGameAct = (ucGameAct> nbRomPerPage ? ucGameAct-nbRomPerPage : 0);
        if (firstRomDisplay>nbRomPerPage) { firstRomDisplay -= nbRomPerPage; }
        else { firstRomDisplay = 0; }
        if (ucGameAct == 0) romSelected = 0;
        if (romSelected > ucGameAct) romSelected = ucGameAct;          
        ucSHaut=0x01;
        dsDisplayFiles(firstRomDisplay,romSelected);
      }
      else
      {
        ucSHaut++;
        if (ucSHaut>10) ucSHaut=0;
      }
      uLenFic=0; ucFlip=-50; ucFlop=0;     
    }
    else {
      ucSHaut = 0;
    }
    
    // -------------------------------------------------------------------------
    // They B key will exit out of the ROM selection without picking a new game
    // -------------------------------------------------------------------------
    if ( keysCurrent() & KEY_B )
    {
      bDone=true;
      while (keysCurrent() & KEY_B);
    }
      
    // -------------------------------------------------------------------
    // Any of these keys will pick the current ROM and try to load it...
    // -------------------------------------------------------------------
    if (keysCurrent() & KEY_A || keysCurrent() & KEY_Y || keysCurrent() & KEY_X)
    {
      if (gpFic[ucGameAct].uType != DIRECT)
      {
        bDone=true;
        ucGameChoice = ucGameAct;
        bForceMSXLoad = false;
        if (keysCurrent() & KEY_X) bForceMSXLoad=true;
        WAITVBL;
      }
      else
      {
        chdir(gpFic[ucGameAct].szName);
        colecoDSFindFiles();
        ucGameAct = 0;
        nbRomPerPage = (countCV>=14 ? 14 : countCV);
        uNbRSPage = (countCV>=5 ? 5 : countCV);
        if (ucGameAct>countCV-nbRomPerPage) {
          firstRomDisplay=countCV-nbRomPerPage;
          romSelected=ucGameAct-countCV+nbRomPerPage;
        }
        else {
          firstRomDisplay=ucGameAct;
          romSelected=0;
        }
        dsDisplayFiles(firstRomDisplay,romSelected);
        while (keysCurrent() & KEY_A);
      }
    }
    
    // --------------------------------------------
    // If the filename is too long... scroll it.
    // --------------------------------------------
    if (strlen(gpFic[ucGameAct].szName) > 29) 
    {
      ucFlip++;
      if (ucFlip >= 25) 
      {
        ucFlip = 0;
        uLenFic++;
        if ((uLenFic+28)>strlen(gpFic[ucGameAct].szName)) 
        {
          ucFlop++;
          if (ucFlop >= 15) 
          {
            uLenFic=0;
            ucFlop = 0;
          }
          else
            uLenFic--;
        }
        strncpy(szName,gpFic[ucGameAct].szName+uLenFic,28);
        szName[28] = '\0';
        DSPrint(1,8+romSelected,2,szName);
      }
    }
    showRandomPreviewSnaps();
    swiWaitForVBlank();
  }
    
  // Remet l'ecran du haut en mode bitmap
  while ((keysCurrent() & (KEY_TOUCH | KEY_START | KEY_SELECT | KEY_A | KEY_B | KEY_R | KEY_L | KEY_UP | KEY_DOWN))!=0);
  
  return 0x01;
}


// ---------------------------------------------------------------------------
// Write out the ColecoDS.DAT configuration file to capture the settings for
// each game.  This one file contains global settings + 400 game settings.
// ---------------------------------------------------------------------------
void SaveConfig(bool bShow)
{
    FILE *fp;
    int slot = 0;
    
    if (bShow) DSPrint(6,0,0, (char*)"SAVING CONFIGURATION");

    // Set the global configuration version number...
    myGlobalConfig.config_ver = CONFIG_VER;

    // If there is a game loaded, save that into a slot... re-use the same slot if it exists
    myConfig.game_crc = file_crc;
    
    if (myConfig.gameSpeed)  myConfig.vertSync = 0;      // If game speed isn't 100%, we can't sync to the DS 60Hz
    
    // Find the slot we should save into...
    for (slot=0; slot<MAX_CONFIGS; slot++)
    {
        if (AllConfigs[slot].game_crc == myConfig.game_crc)  // Got a match?!
        {
            break;                           
        }
        if (AllConfigs[slot].game_crc == 0x00000000)  // Didn't find it... use a blank slot...
        {
            break;                           
        }
    }

    // --------------------------------------------------------------------------
    // Copy our current game configuration to the main configuration database...
    // --------------------------------------------------------------------------
    if (myConfig.game_crc != 0x00000000)
    {
        memcpy(&AllConfigs[slot], &myConfig, sizeof(struct Config_t));
    }

    // --------------------------------------------------
    // Now save the config file out o the SD card...
    // --------------------------------------------------
    DIR* dir = opendir("/data");
    if (dir)
    {
        closedir(dir);  // Directory exists.
    }
    else
    {
        mkdir("/data", 0777);   // Doesn't exist - make it...
    }
    fp = fopen("/data/ColecoDS.DAT", "wb+");
    if (fp != NULL)
    {
        fwrite(&myGlobalConfig, sizeof(myGlobalConfig), 1, fp); // Write the global config
        fwrite(&AllConfigs, sizeof(AllConfigs), 1, fp);         // Write the array of all configurations
        fclose(fp);
    } else DSPrint(4,0,0, (char*)"ERROR SAVING CONFIG FILE");

    if (bShow) 
    {
        WAITVBL;WAITVBL;WAITVBL;WAITVBL;WAITVBL;
        DSPrint(4,0,0, (char*)"                        ");
    }
}

void MapPlayer2(void)
{
    myConfig.keymap[0]   = 20;    // NDS D-Pad mapped to CV Joystick UP
    myConfig.keymap[1]   = 21;    // NDS D-Pad mapped to CV Joystick DOWN
    myConfig.keymap[2]   = 22;    // NDS D-Pad mapped to CV Joystick LEFT
    myConfig.keymap[3]   = 23;    // NDS D-Pad mapped to CV Joystick RIGHT
    myConfig.keymap[4]   = 24;    // NDS A Button mapped to CV Button 1 (Yellow / Left Button)
    myConfig.keymap[5]   = 25;    // NDS B Button mapped to CV Button 2 (Red / Right Button)
    myConfig.keymap[6]   = 26;    // NDS X Button mapped to CV Button 3 (Purple / Super Action)
    myConfig.keymap[7]   = 27;    // NDS Y Button mapped to CV Button 4 (Blue / Super Action)
    myConfig.keymap[8]   = 81;    // NDS R      mapped to CTRL
    myConfig.keymap[9]   = 80;    // NDS L      mapped to SHIFT
    myConfig.keymap[10]  = 28;    // NDS Start  mapped to Keypad #1
    myConfig.keymap[11]  = 29;    // NDS Select mapped to Keypad #2
}

void MapPlayer1(void)
{
    myConfig.keymap[0]   = 0;    // NDS D-Pad mapped to CV Joystick UP
    myConfig.keymap[1]   = 1;    // NDS D-Pad mapped to CV Joystick DOWN
    myConfig.keymap[2]   = 2;    // NDS D-Pad mapped to CV Joystick LEFT
    myConfig.keymap[3]   = 3;    // NDS D-Pad mapped to CV Joystick RIGHT
    myConfig.keymap[4]   = 4;    // NDS A Button mapped to CV Button 1 (Yellow / Left Button)
    myConfig.keymap[5]   = 5;    // NDS B Button mapped to CV Button 2 (Red / Right Button)
    myConfig.keymap[6]   = 6;    // NDS X Button mapped to CV Button 3 (Purple / Super Action)
    myConfig.keymap[7]   = 7;    // NDS Y Button mapped to CV Button 4 (Blue / Super Action)
    myConfig.keymap[8]   = 81;   // NDS R      mapped to CTRL
    myConfig.keymap[9]   = 80;   // NDS L      mapped to SHIFT
    myConfig.keymap[10]  = 8;    // NDS Start  mapped to Keypad #1
    myConfig.keymap[11]  = 9;    // NDS Select mapped to Keypad #2
}

void MapQAOP(void)
{
    myConfig.keymap[0]   = 60;    // Q
    myConfig.keymap[1]   = 44;    // A
    myConfig.keymap[2]   = 58;    // O
    myConfig.keymap[3]   = 59;    // P
    myConfig.keymap[4]   = 84;    // Space
    myConfig.keymap[5]   = 84;    // Space
    myConfig.keymap[6]   = 92;    // Period 
    myConfig.keymap[7]   = 92;    // Period
    myConfig.keymap[8]   = 81;    // NDS R      mapped to CTRL
    myConfig.keymap[9]   = 80;    // NDS L      mapped to SHIFT
    myConfig.keymap[10]  = 71;    // 1
    myConfig.keymap[11]  = 72;    // 2
}

void MapWASD(void)
{
    myConfig.keymap[0]   = 66;    // W
    myConfig.keymap[1]   = 44;    // A
    myConfig.keymap[2]   = 62;    // S
    myConfig.keymap[3]   = 47;    // D
    myConfig.keymap[4]   = 84;    // Space
    myConfig.keymap[5]   = 84;    // Space
    myConfig.keymap[6]   = 84;    // Space
    myConfig.keymap[7]   = 84;    // Space
    myConfig.keymap[8]   = 81;    // NDS R      mapped to CTRL
    myConfig.keymap[9]   = 80;    // NDS L      mapped to SHIFT
    myConfig.keymap[10]  = 71;    // 1
    myConfig.keymap[11]  = 72;    // 2
}

void MapZCPeriod(void)
{
    myConfig.keymap[0]   = 60;    // Q
    myConfig.keymap[1]   = 44;    // A
    myConfig.keymap[2]   = 69;    // Z
    myConfig.keymap[3]   = 46;    // C
    myConfig.keymap[4]   = 92;    // Period
    myConfig.keymap[5]   = 92;    // Period
    myConfig.keymap[6]   = 84;    // Space
    myConfig.keymap[7]   = 84;    // Space
    myConfig.keymap[8]   = 81;    // NDS R      mapped to CTRL
    myConfig.keymap[9]   = 80;    // NDS L      mapped to SHIFT
    myConfig.keymap[10]  = 71;    // 1
    myConfig.keymap[11]  = 72;    // 2
}


void MapArrows(void)
{
    myConfig.keymap[0]   = 88;    // UP Arrow
    myConfig.keymap[1]   = 89;    // Down Arrow
    myConfig.keymap[2]   = 90;    // Left Arrow
    myConfig.keymap[3]   = 91;    // Right Arrow
    myConfig.keymap[4]   = 84;    // Space
    myConfig.keymap[5]   = 84;    // Space
    myConfig.keymap[6]   = 84;    // Space
    myConfig.keymap[7]   = 84;    // Space
    myConfig.keymap[8]   = 81;    // NDS R      mapped to CTRL
    myConfig.keymap[9]   = 80;    // NDS L      mapped to SHIFT
    myConfig.keymap[10]  = 71;    // 1
    myConfig.keymap[11]  = 72;    // 2
}


void SetDefaultGlobalConfig(void)
{
    // A few global defaults...
    memset(&myGlobalConfig, 0x00, sizeof(myGlobalConfig));
    myGlobalConfig.showBiosInfo   = 1;    // Show BIOS info at startup by default
    myGlobalConfig.showFPS        = 0;    // Don't show FPS counter by default
    myGlobalConfig.defaultMSX     = 1;    // Default to the MSX.ROM if available
    myGlobalConfig.emuText        = 1;    // Default is to show Emulator Text
    myGlobalConfig.msxCartOverlay = 1;    // Default is to show Keyboard for CART games
    myGlobalConfig.defSprites     = 0;    // Default is to show 32 sprites (real hardware is 4 per line)
}

void SetDefaultGameConfig(void)
{
    myConfig.game_crc    = 0;    // No game in this slot yet
    
    MapPlayer1();                // Default to Player 1 mapping
    
    myConfig.frameSkip   = (isDSiMode() ? 0:1);         // For DSi we don't need FrameSkip, but for older DS-LITE we turn on light frameskip
    myConfig.frameBlend  = 0;                           // No frame blending needed for most games
    myConfig.msxMapper   = GUESS;                       // MSX mapper takes its best guess
    myConfig.autoFire    = 0;                           // Default to no auto-fire on either button
    myConfig.isPAL       = 0;                           // Default to NTSC
    myConfig.overlay     = 0;                           // Default to normal CV overlay
    myConfig.maxSprites  = myGlobalConfig.defSprites;   // 0 means allow 32 sprites... 1 means limit to the original 4 sprites of the VDP
    myConfig.vertSync    = (isDSiMode() ? 1:0);         // Default is Vertical Sync ON for DSi and OFF for DS-LITE
    myConfig.spinSpeed   = 0;                           // Default spin speed is normal
    myConfig.touchPad    = 0;                           // Nothing special about the touch-pad by default
    myConfig.cpuCore     = 1;                           // Default to the more accurate CZ80 core
    myConfig.msxBios     = (bMSXBiosFound ? myGlobalConfig.defaultMSX:0); // Default to real MSX bios unless we can't find it
    myConfig.msxKey5     = 0;                           // Default key map for MSX key 5 (question mark)
    myConfig.dpad        = DPAD_NORMAL;                 // Normal DPAD use - mapped to joystick
    myConfig.memWipe     = 0;                           // Default to RANDOM memory
    myConfig.clearInt    = CPU_CLEAR_INT_AUTOMATICALLY; // By default clear VDP interrupts automatically
    myConfig.cvEESize    = C24XX_24C256;                // Default CV EEPROM size is 32K
    myConfig.ayEnvelope  = 0;                           // By default AY envelopes are enabled
    myConfig.mirrorRAM   = COLECO_RAM_NORMAL_MIRROR;    // By default use the normal Colecovision (and CreatiVision) memory mirrors
    myConfig.msxBeeper   = 0;                           // Assume no MSX beeper required - only a few games need this
    myConfig.cvisionLoad = 0;                           // Default to normal Legacy A/B load for CreatiVision games
    myConfig.gameSpeed   = 0;                           // Default is 100% game speed
    myConfig.reserved1   = 0;
    myConfig.reserved2   = 0;
    myConfig.reserved3   = 0;    
    myConfig.reserved4   = 0;    
    myConfig.reserved5   = 0;    
    myConfig.reserved6   = 0xA5;    // So it's easy to spot on an "upgrade"
    myConfig.reserved32  = 0x00000000;
  
    // ----------------------------------------------------------------------------------
    // A few games don't want more than 4 max sprites (they pull tricks that rely on it)
    // ----------------------------------------------------------------------------------
    if (file_crc == 0xee530ad2) myConfig.maxSprites  = 1;  // QBiqs
    if (file_crc == 0x275c800e) myConfig.maxSprites  = 1;  // Antartic Adventure
    if (file_crc == 0xa66e5ed1) myConfig.maxSprites  = 1;  // Antartic Adventure Prototype  
    if (file_crc == 0x6af19e75) myConfig.maxSprites  = 1;  // Adventures in the Park    
    if (file_crc == 0xbc8320a0) myConfig.maxSprites  = 1;  // Uridium 
    
    
    // -------------------------------------------
    // Turbo needs the Driving Module
    // -------------------------------------------
    if (file_crc == 0xbd6ab02a)      // Turbo (Sega)
    {
        myConfig.touchPad    = 1;    // Map the on-screen touch Keypad to P2 for Turbo
        myConfig.keymap[0]   = 20;   // NDS D-Pad mapped to P2 UP
        myConfig.keymap[1]   = 21;   // NDS D-Pad mapped to P2 DOWN 
        myConfig.keymap[2]   = 41;   // NDS D-Pad mapped to Spinner X
        myConfig.keymap[3]   = 40;   // NDS D-Pad mapped to Spinner X
    }
    
    
    // -------------------------------------------
    // Destructor needs the Driving Module...
    // -------------------------------------------
    if (file_crc == 0x56c358a6)      // Destructor (Coleco)
    {
        myConfig.touchPad    = 1;    // Map the on-screen touch Keypad to P2 for Destructor
        myConfig.keymap[0]   = 20;   // NDS D-Pad mapped to P2 UP
        myConfig.keymap[1]   = 21;   // NDS D-Pad mapped to P2 DOWN 
        myConfig.keymap[2]   = 41;   // NDS D-Pad mapped to Spinner X
        myConfig.keymap[3]   = 40;   // NDS D-Pad mapped to Spinner X
    }
    
    // -------------------------------------------
    // Dukes of Hazzard needs the Driving Module
    // -------------------------------------------
    if (file_crc == 0x4025ac94)      // Dukes of Hazzard (Coleco)
    {
        myConfig.touchPad    = 1;    // Map the on-screen touch Keypad to P2 for Dukes of Hazzard
        myConfig.keymap[0]   = 20;   // NDS D-Pad mapped to P2 UP
        myConfig.keymap[1]   = 21;   // NDS D-Pad mapped to P2 DOWN 
        myConfig.keymap[2]   = 41;   // NDS D-Pad mapped to Spinner X
        myConfig.keymap[3]   = 40;   // NDS D-Pad mapped to Spinner X
        myConfig.keymap[6]   = 23;   // NDS X Button mapped to P2 RIGHT for gear shift
        myConfig.keymap[7]   = 22;   // NDS Y Button mapped to P2 LEFT for gear shift
    }
    
    // -------------------------------------------
    // Slither needs Trackball Support (SpinX/Y)
    // -------------------------------------------
    if (file_crc == 0x53d2651c)      // Slither (Century)
    {
        myConfig.keymap[0]   = 42;   // NDS D-Pad mapped to Spinner Y
        myConfig.keymap[1]   = 43;   // NDS D-Pad mapped to Spinner Y
        myConfig.keymap[2]   = 41;   // NDS D-Pad mapped to Spinner X
        myConfig.keymap[3]   = 40;   // NDS D-Pad mapped to Spinner X
    }
    
    // ---------------------------------------------------------
    // Victory needs Trackball Support (SpinX/Y) plus Buttons
    // ---------------------------------------------------------
    if (file_crc == 0x70142655)      // Victory (Exidy)
    {
        myConfig.keymap[0]   = 42;   // NDS D-Pad mapped to Spinner Y
        myConfig.keymap[1]   = 43;   // NDS D-Pad mapped to Spinner Y
        myConfig.keymap[2]   = 41;   // NDS D-Pad mapped to Spinner X
        myConfig.keymap[3]   = 40;   // NDS D-Pad mapped to Spinner X
        
        myConfig.keymap[4]   = 4;    // NDS A Button mapped to P1 Button 1
        myConfig.keymap[5]   = 5;    // NDS B Button mapped to P1 Button 2 
        myConfig.keymap[6]   = 24;   // NDS X Button mapped to P2 Button 1
        myConfig.keymap[7]   = 25;   // NDS Y Button mapped to P2 Button 2         
    }
    
    if ((file_crc == 0xeec68527) ||     // SVI Crazy Teeth
        (file_crc == 0x1748aed7))       // SVI Burkensoft Game Pak 14 with MEGALONE
    {
        MapPlayer2();               // These games want P2 mapping...
    }
    
   
    // -----------------------------------------------------------
    // If we are DS-PHAT or DS-LITE running on slower CPU, we 
    // need to help the processor out a bit by turning off RAM
    // mirrors for the games that don't need them.
    // -----------------------------------------------------------
    if (!isDSiMode())
    {
        int idx=0;
        while (cv_no_mirror_games[idx] != 0xFFFFFFFF)
        {
            if (file_crc == cv_no_mirror_games[idx])
            {
                myConfig.mirrorRAM = COLECO_RAM_NO_MIRROR;
                break;
            }
            idx++;
        }
        
        myConfig.spinSpeed = 5;                                                 // Turn off Spinner... except for these games
        if (file_crc == 0x53d2651c)             myConfig.spinSpeed = 0;         // Slither
        if (file_crc == 0xbd6ab02a)             myConfig.spinSpeed = 0;         // Turbo
        if (file_crc == 0xd0110137)             myConfig.spinSpeed = 0;         // Front Line
        if (file_crc == 0x56c358a6)             myConfig.spinSpeed = 0;         // Destructor
        if (file_crc == 0x70142655)             myConfig.spinSpeed = 0;         // Victory
    }
    
    if (sg1000_mode == 2)                       myConfig.overlay = 16; // SC-3000 uses the full keyboard
    if (sg1000_mode == 2)                       myConfig.vertSync= 0;  // SC-3000 does not use vertical sync
    if (msx_mode == 1)                          myConfig.overlay = (myGlobalConfig.msxCartOverlay ? 10:0);  // MSX cart-based games follows the global default
    if (msx_mode == 2)                          myConfig.overlay = 10; // MSX with .cas defaults to full keyboard    
    if (msx_mode == 2)                          myConfig.msxBios = myGlobalConfig.defaultMSX;  // If loading cassette, must have real MSX bios
    if (adam_mode)                              myConfig.memWipe = 1;  // Adam defaults to clearing memory to a specific pattern.
    if (msx_mode && (file_size >= (64*1024)))   myConfig.vertSync= 0;  // For bankswiched MSX games, disable VSync to gain speed
    if (memotech_mode)                          myConfig.overlay = 11; // Memotech MTX default to full keyboard
    if (svi_mode)                               myConfig.overlay = 15; // SVI default to full keyboard
    if (einstein_mode)                          myConfig.overlay = 14; // Tatung Einstein defaults to full keyboard
    if (einstein_mode)                          myConfig.isPAL   = 1;  // Tatung Einstein defaults to PAL machine
    if (memotech_mode)                          myConfig.isPAL   = 1;  // Memotech defaults to PAL machine
    if (creativision_mode)                      myConfig.isPAL   = 1;  // Creativision defaults to PAL machine
    if (creativision_mode)                      myConfig.vertSync= 0;  // Creativision defaults to no vert sync
    
    if (file_crc == 0x9b547ba8)                 myConfig.cpuCore = 0;   // Colecovision Boulder Dash only works with the DrZ80 core
    if (file_crc == 0xb32c9e08)                 myConfig.cpuCore = 0;   // Sord M5 Mahjong (Jong Kyo) only works with the DrZ80 core
    if (file_crc == 0xa2edc01d)                 myConfig.cpuCore = 0;   // Sord M5 Mahjong (Jong Kyo) only works with the DrZ80 core
    
    // --------------------------------------------------------
    // These ADAM games use the larger full keyboard
    // --------------------------------------------------------
    if (file_crc == 0x6a879bc7)                 myConfig.overlay = 9;  // 2010 Graphic Adventure DSK
    if (file_crc == 0xf9257a68)                 myConfig.overlay = 9;  // 2010 Graphic Adventure[a1] DSK
    if (file_crc == 0xf7ff1a75)                 myConfig.overlay = 9;  // 2010 Graphic Adventure DDP
    if (file_crc == 0x3fe56492)                 myConfig.overlay = 9;  // Blank Media DDP
    if (file_crc == 0x45c120b5)                 myConfig.overlay = 9;  // Jeopardy DDP
    if (file_crc == 0xca1cc594)                 myConfig.overlay = 9;  // Family Fued DDP
    if (file_crc == 0xaa52c12f)                 myConfig.overlay = 9;  // ADAM SmartBASIC DDP
    if (file_crc == 0xe27df400)                 myConfig.overlay = 9;  // ADAM SmartBASIC [a1] DDP
    if (file_crc == 0x1a7aaf37)                 myConfig.overlay = 9;  // Temple of the Snow Dragon DDP
    if (file_crc == 0xf9257a68)                 myConfig.overlay = 9;  // 2010 - the text adventure game (1984) (coleco) [a1].dsk
    if (file_crc == 0x6a879bc7)                 myConfig.overlay = 9;  // 2010 - the text adventure game (1984) (coleco).dsk
    if (file_crc == 0x955cf8ae)                 myConfig.overlay = 9;  // adamcalc (1984) (coleco) [a1].dsk
    if (file_crc == 0x00c30589)                 myConfig.overlay = 9;  // adamcalc (1984) (coleco).dsk
    if (file_crc == 0x92ac82b4)                 myConfig.overlay = 9;  // adam graphics (199x) (maine adam library) [a1].dsk
    if (file_crc == 0xf335de5e)                 myConfig.overlay = 9;  // adam graphics (199x) (maine adam library).dsk
    if (file_crc == 0x83295fd8)                 myConfig.overlay = 9;  // adamlink i (1984) (coleco).dsk
    if (file_crc == 0x64f9486b)                 myConfig.overlay = 9;  // adamlink ii (1984) (coleco).dsk
    if (file_crc == 0xc900e8a8)                 myConfig.overlay = 9;  // adventure (1979) (scott adms) (aka - colossal cave).dsk
    if (file_crc == 0x3f6b4963)                 myConfig.overlay = 9;  // adventure pack 1 (1986) (adamagic software).dsk
    if (file_crc == 0x7471c625)                 myConfig.overlay = 9;  // adventure pack 2 (1986) (adamagic software).dsk
    if (file_crc == 0x2a9629be)                 myConfig.overlay = 9;  // adventure pack 3 (1986) (adamagic software).dsk
    if (file_crc == 0x1fd42c84)                 myConfig.overlay = 9;  // adventure pack i (1984) (victory software).dsk
    if (file_crc == 0x4c78d88c)                 myConfig.overlay = 9;  // adventure pack ii (1984) (victory software).dsk
    if (file_crc == 0x2ec89205)                 myConfig.overlay = 9;  // ballyhoo (1986) (infocom).dsk
    if (file_crc == 0xef284baf)                 myConfig.overlay = 9;  // cp-m 2.2 v1.50 (1984) (coleco).dsk
    if (file_crc == 0x4632a370)                 myConfig.overlay = 9;  // cutthroats (1984) (infocom).dsk
    if (file_crc == 0x9539e5f5)                 myConfig.overlay = 9;  // deadline (1982) (infocom) [a1].dsk
    if (file_crc == 0xd46ff3d1)                 myConfig.overlay = 9;  // deadline (1982) (infocom).dsk
    if (file_crc == 0x3749609c)                 myConfig.overlay = 9;  // enchanter (1983) (infocom).dsk
    if (file_crc == 0x9bb60875)                 myConfig.overlay = 9;  // family feud (1984) (coleco) [a1].dsk
    if (file_crc == 0x3060b235)                 myConfig.overlay = 9;  // family feud (1984) (coleco).dsk
    if (file_crc == 0x43828f0d)                 myConfig.overlay = 9;  // file manager v2.0 (1988) (ajm software).dsk
    if (file_crc == 0xa36afa87)                 myConfig.overlay = 9;  // file manager v2.1 (1988) (ajm software).dsk
    if (file_crc == 0xd39f1fb1)                 myConfig.overlay = 9;  // file manager v2.2 (1988) (ajm software).dsk
    if (file_crc == 0x3c1653e1)                 myConfig.overlay = 9;  // file manager v3.0 (1992) (ajm software).dsk
    if (file_crc == 0x9065853e)                 myConfig.overlay = 9;  // hitchhiker's guide to the galaxy, the (1984) (infocom) [a1].dsk
    if (file_crc == 0x76bae642)                 myConfig.overlay = 9;  // hitchhiker's guide to the galaxy, the (1984) (infocom).dsk
    if (file_crc == 0xb481e00b)                 myConfig.overlay = 9;  // infidel (1983) (infocom).dsk
    if (file_crc == 0xc8850be8)                 myConfig.overlay = 9;  // jeopardy (1984) (coleco) (prototype).dsk
    if (file_crc == 0xf822c7a8)                 myConfig.overlay = 9;  // jeopardy - disk#01 (1984) (coleco) (prototype).dsk
    if (file_crc == 0x5d0045a2)                 myConfig.overlay = 9;  // jeopardy - disk#02 (1984) (coleco) (prototype).dsk
    if (file_crc == 0xc4754e3e)                 myConfig.overlay = 9;  // leather goddess' of phobos (1986) (infocom).dsk
    if (file_crc == 0x5177032a)                 myConfig.overlay = 9;  // planetfall (1983) (infocom) [a1].dsk
    if (file_crc == 0xe9f6b71b)                 myConfig.overlay = 9;  // planetfall (1983) (infocom).dsk
    if (file_crc == 0x2371b3d5)                 myConfig.overlay = 9;  // plundered hearts (1987) (infocom).dsk
    if (file_crc == 0x455e72d8)                 myConfig.overlay = 9;  // seastalker (1984) (infocom) [a1].dsk
    if (file_crc == 0x3e298768)                 myConfig.overlay = 9;  // seastalker (1984) (infocom).dsk
    if (file_crc == 0x8866fcc9)                 myConfig.overlay = 9;  // smartbasic 1.x rev-20 (1991) (rich drushel software) [a1].dsk
    if (file_crc == 0x2368fcc5)                 myConfig.overlay = 9;  // smartbasic 1.x rev-20 (1991) (rich drushel software).dsk
    if (file_crc == 0x250f1fa5)                 myConfig.overlay = 9;  // smartbasic v1.0 (1983) (coleco) [a1].dsk
    if (file_crc == 0xe9b12204)                 myConfig.overlay = 9;  // smartbasic v1.0 (1983) (coleco).dsk
    if (file_crc == 0xc3755733)                 myConfig.overlay = 9;  // smartbasic v1.0 (1983)(coleco - lmi).dsk
    if (file_crc == 0xca37ac14)                 myConfig.overlay = 9;  // smartbasic v1.0 - d.e.i. patches (1986) (digital express inc.).dsk
    if (file_crc == 0x119736b5)                 myConfig.overlay = 9;  // smartbasic v1.0 (disk enhanced) (1983) (coleco - m.m.s.g.).dsk
    if (file_crc == 0x58e97cc8)                 myConfig.overlay = 9;  // smartbasic v1.0 with parallel printer driver (198x) (wayne motel - n.i.a.d. software) [a1].dsk
    if (file_crc == 0xb12b2e21)                 myConfig.overlay = 9;  // smartbasic v1.0 with parallel printer driver (198x) (wayne motel - n.i.a.d. software).dsk
    if (file_crc == 0xcf0002f1)                 myConfig.overlay = 9;  // smartbasic v1.2 (1983)(coleco - lmi).dsk
    if (file_crc == 0x893bdc71)                 myConfig.overlay = 9;  // smartbasic+ v1.3 (198x) (unknown) [a1].dsk
    if (file_crc == 0xcdb75d23)                 myConfig.overlay = 9;  // smartbasic+ v1.3 (198x) (unknown).dsk
    if (file_crc == 0x410b68c6)                 myConfig.overlay = 9;  // smartbasic+ v1.79 (1987) (sharon macfarlane).dsk
    if (file_crc == 0xa3c57b61)                 myConfig.overlay = 9;  // smartbasic v2.0 (1984) (coleco) (prototype) [a1].dsk
    if (file_crc == 0xb8b49c7b)                 myConfig.overlay = 9;  // smartbasic v2.0 (1984) (coleco) (prototype).dsk
    if (file_crc == 0x43fa7b0f)                 myConfig.overlay = 9;  // smartbasic v2.0 - 40 column (1984) (coleco - gary hoosier software) [a1].dsk
    if (file_crc == 0x63301b15)                 myConfig.overlay = 9;  // smartbasic v2.0 - 40 column (1984) (coleco - gary hoosier software).dsk
    if (file_crc == 0xccb5a561)                 myConfig.overlay = 9;  // smartbasic v2.1 & tools (1988) (coleco - sharon macfarlane).dsk
    if (file_crc == 0x07c07a56)                 myConfig.overlay = 9;  // sorcerer (198x) (infocom).dsk
    if (file_crc == 0xf27044d8)                 myConfig.overlay = 9;  // spellbreaker (1985) (infocom).dsk
    if (file_crc == 0xec88e6bb)                 myConfig.overlay = 9;  // starcross (1982) (infocom) [a1].dsk
    if (file_crc == 0xb4b1e0f3)                 myConfig.overlay = 9;  // starcross (1982) (infocom).dsk
    if (file_crc == 0xd092bbcb)                 myConfig.overlay = 9;  // stationfall (1987) (infocom).dsk
    if (file_crc == 0xdda503aa)                 myConfig.overlay = 9;  // suspect (1984) (infocom).dsk
    if (file_crc == 0x04e9d59e)                 myConfig.overlay = 9;  // suspended (1983) (infocom).dsk
    if (file_crc == 0x89f1cf13)                 myConfig.overlay = 9;  // wishbringer (1985) (infocom).dsk
    if (file_crc == 0x48a313ad)                 myConfig.overlay = 9;  // witness (1983) (infocom).dsk
    if (file_crc == 0xd890aad7)                 myConfig.overlay = 9;  // zork iii - the dungeon master (1982) (infocom) [a1].dsk
    if (file_crc == 0x5ce833b2)                 myConfig.overlay = 9;  // zork iii - the dungeon master (1982) (infocom).dsk
    if (file_crc == 0x3daf5073)                 myConfig.overlay = 9;  // zork ii - the wizard of frobozz (1981) (infocom) [a1].dsk
    if (file_crc == 0xc8ada76e)                 myConfig.overlay = 9;  // zork ii - the wizard of frobozz (1981) (infocom).dsk
    if (file_crc == 0x8dac567c)                 myConfig.overlay = 9;  // zork i - the great underground empire (1981) (infocom) [a1].dsk
    if (file_crc == 0xae4d50e9)                 myConfig.overlay = 9;  // zork i - the great underground empire (1981) (infocom).dsk
    
    
    // And some special BASIC carts that want full keyboards
    if (file_crc == 0x69a92b72)                 myConfig.overlay = 11; // PV-2000 BASIC uses keyboard
    if (file_crc == 0x47be051f)                 myConfig.overlay = 11; // Sord M5 BASIC uses keyboard
    if (file_crc == 0x107b5ddc)                 myConfig.overlay = 11; // Sord M5 BASIC uses keyboard
    if (file_crc == 0x2b1087cf)                 myConfig.overlay = 11; // Sord M5 BASIC uses keyboard
    if (file_crc == 0x4891613b)                 myConfig.overlay = 13; // Hanimex Pencil II BASIC uses simplified keyboard    

    // ----------------------------------------------------------------------------------
    // Some CreatiVision games that want the new CV overlay keypad/keybaord
    // ----------------------------------------------------------------------------------
    if (file_crc == 0x4aee923e)                 myConfig.overlay = 12; // BASIC 82A
    if (file_crc == 0x1849efd0)                 myConfig.overlay = 12; // BASIC 82B
    if (file_crc == 0x10409a1d)                 myConfig.overlay = 12; // BASIC 83A
    if (file_crc == 0x044adbe8)                 myConfig.overlay = 12; // BASIC 83C
    if (file_crc == 0x8258ee6c)                 myConfig.overlay = 12; // BASIC 83H
    if (file_crc == 0x8375203e)                 myConfig.overlay = 12; // CSL BIOS A
    if (file_crc == 0x77afd38b)                 myConfig.overlay = 12; // CSL BIOS B
    if (file_crc == 0x9e584ce2)                 myConfig.overlay = 12; // DIAG A
    if (file_crc == 0x4d92ff4e)                 myConfig.overlay = 12; // DIAG B
    if (file_crc == 0xadb11067)                 myConfig.overlay = 12; // DIAG DEMO
    if (file_crc == 0xc2ba6a99)                 myConfig.overlay = 12; // WERBENE
    if (file_crc == 0xf8383d33)                 myConfig.overlay = 12; // MUSIC MAKER
    
    
    // ----------------------------------------------------------------------------------
    // A bunch of CP/M games for the Adam need a special memory wipe to load properly...
    // ----------------------------------------------------------------------------------
    if (file_crc == 0x07c07a56)                 myConfig.memWipe = 2;  // sorcerer (198x) (infocom).dsk
    if (file_crc == 0xf27044d8)                 myConfig.memWipe = 2;  // spellbreaker (1985) (infocom).dsk
    if (file_crc == 0xec88e6bb)                 myConfig.memWipe = 2;  // starcross (1982) (infocom) [a1].dsk
    if (file_crc == 0xb4b1e0f3)                 myConfig.memWipe = 2;  // starcross (1982) (infocom).dsk
    if (file_crc == 0xd092bbcb)                 myConfig.memWipe = 2;  // stationfall (1987) (infocom).dsk
    if (file_crc == 0xdda503aa)                 myConfig.memWipe = 2;  // suspect (1984) (infocom).dsk
    if (file_crc == 0x04e9d59e)                 myConfig.memWipe = 2;  // suspended (1983) (infocom).dsk
    if (file_crc == 0x89f1cf13)                 myConfig.memWipe = 2;  // wishbringer (1985) (infocom).dsk
    if (file_crc == 0x48a313ad)                 myConfig.memWipe = 2;  // witness (1983) (infocom).dsk
    if (file_crc == 0xd890aad7)                 myConfig.memWipe = 2;  // zork iii - the dungeon master (1982) (infocom) [a1].dsk
    if (file_crc == 0x5ce833b2)                 myConfig.memWipe = 2;  // zork iii - the dungeon master (1982) (infocom).dsk
    if (file_crc == 0x3daf5073)                 myConfig.memWipe = 2;  // zork ii - the wizard of frobozz (1981) (infocom) [a1].dsk
    if (file_crc == 0xc8ada76e)                 myConfig.memWipe = 2;  // zork ii - the wizard of frobozz (1981) (infocom).dsk
    if (file_crc == 0x8dac567c)                 myConfig.memWipe = 2;  // zork i - the great underground empire (1981) (infocom) [a1].dsk
    if (file_crc == 0xae4d50e9)                 myConfig.memWipe = 2;  // zork i - the great underground empire (1981) (infocom).dsk
    
    // For ADAM .dsk games, we want to see if this is a CP/M game and adjust the memwipe
    if (adam_mode)
    {
        for (int i=0; i<0x1000; i++)
        {
            if ((ROM_Memory[i] == 'C') && (ROM_Memory[i+1] == 'P') && (ROM_Memory[i+2] == '/') && (ROM_Memory[i+3] == 'M')) // Look for CP/M
            {
                myConfig.memWipe = 2;  // Set to CPM clear pattern by default
                myConfig.overlay = 9;  // And most CPM games are going to want a full keyboard
                break;
            }
        }
    }
    
    // ---------------------------------------------------------------------------------------------
    // And we don't have the AY envelope quite right so a few games don't want to reset the indexes
    // ---------------------------------------------------------------------------------------------
    if (file_crc == 0x90f5f414)                 myConfig.ayEnvelope = 1; // MSX Warp-and-Warp
    if (file_crc == 0x5e169d35)                 myConfig.ayEnvelope = 1; // MSX Warp-and-Warp (alt)
    if (file_crc == 0xe66eaed9)                 myConfig.ayEnvelope = 1; // MSX Warp-and-Warp (alt)
    if (file_crc == 0x785fc789)                 myConfig.ayEnvelope = 1; // MSX Warp-and-Warp (alt)    
    if (file_crc == 0xe50d6e60)                 myConfig.ayEnvelope = 1; // MSX Warp-and-Warp (cassette)    
    if (file_crc == 0x73f37230)                 myConfig.ayEnvelope = 1; // MSX Killer Station
    
    // ----------------------------------------------------------------------------
    // For these machines, we default to clearing interrupts only on VDP read...
    // ----------------------------------------------------------------------------
    if (msx_mode || einstein_mode || svi_mode || memotech_mode) myConfig.clearInt = CPU_CLEAR_INT_ON_VDP_READ;
    
    // ---------------------------------------------------------------------------------
    // A few games don't work well with the clearing of interrupts on VDP and run 
    // better with auto-clear.  So we adjust those here...
    // ---------------------------------------------------------------------------------
    if (file_crc == 0xef339b82)                 myConfig.clearInt =  CPU_CLEAR_INT_AUTOMATICALLY; // MSX Ninja Kun - Bouken
    if (file_crc == 0xc9bcbe5a)                 myConfig.clearInt =  CPU_CLEAR_INT_AUTOMATICALLY; // MSX Ghostbusters
    if (file_crc == 0x9814c355)                 myConfig.clearInt =  CPU_CLEAR_INT_AUTOMATICALLY; // MSX Ghostbusters    
    if (file_crc == 0x90530889)                 myConfig.clearInt =  CPU_CLEAR_INT_AUTOMATICALLY; // MSX Soul of a Robot
    if (file_crc == 0x33221ad9)                 myConfig.clearInt =  CPU_CLEAR_INT_AUTOMATICALLY; // MSX Time Bandits    
    if (file_crc == 0x9dbdd4bc)                 myConfig.clearInt =  CPU_CLEAR_INT_AUTOMATICALLY; // MSX GP World (Sega)    
    if (file_crc == 0x7820e86c)                 myConfig.clearInt =  CPU_CLEAR_INT_AUTOMATICALLY; // MSX GP World (Sega)   
    if (file_crc == 0x6e8bb5fa)                 myConfig.clearInt =  CPU_CLEAR_INT_AUTOMATICALLY; // MSX Seleniak - Mark II
    if (file_crc == 0xb8ca3108)                 myConfig.clearInt =  CPU_CLEAR_INT_AUTOMATICALLY; // Memotech MTX Quazzia
    if (file_crc == 0xbd285566)                 myConfig.clearInt =  CPU_CLEAR_INT_AUTOMATICALLY; // Memotech MTX Caves of Orb
    if (file_crc == 0xe30fb8f7)                 myConfig.clearInt =  CPU_CLEAR_INT_AUTOMATICALLY; // Memotech MTX Pac Manor Rescue
    if (file_crc == 0xa2db030e)                 myConfig.clearInt =  CPU_CLEAR_INT_AUTOMATICALLY; // Memotech MTX Revenge of the Chamberoids    
    if (file_crc == 0x3c8500af)                 myConfig.clearInt =  CPU_CLEAR_INT_AUTOMATICALLY; // Memotech MTX Target Zone
    if (file_crc == 0xbde21de8)                 myConfig.clearInt =  CPU_CLEAR_INT_AUTOMATICALLY; // Memotech MTX Target Zone
    if (file_crc == 0x8f9f902e)                 myConfig.clearInt =  CPU_CLEAR_INT_AUTOMATICALLY; // Memotech MTX Target Zone    
    if (file_crc == 0xe3f495c4)                 myConfig.clearInt =  CPU_CLEAR_INT_AUTOMATICALLY; // Memotech MAGROM
    if (file_crc == 0x98240ee9)                 myConfig.clearInt =  CPU_CLEAR_INT_AUTOMATICALLY; // Memotech MAGROM
    
    
    // ---------------------------------------------------------------------------------
    // A few of the ZX Spectrum ports actually use the MSX beeper for sound. Go figure!
    // ---------------------------------------------------------------------------------
    if (file_crc == 0x1b8873ca)                 myConfig.msxBeeper = 1;     // MSX Avenger uses beeper
    if (file_crc == 0x111fc33b)                 myConfig.msxBeeper = 1;     // MSX Avenger uses beeper    
    if (file_crc == 0x690f9715)                 myConfig.msxBeeper = 1;     // MSX Batman (the movie) uses beeper
    if (file_crc == 0x3571f5d4)                 myConfig.msxBeeper = 1;     // MSX Master of the Universe uses beeper
    if (file_crc == 0x2142bd10)                 myConfig.msxBeeper = 1;     // MSX Future Knight

    // ------------------------------------------------------
    // A few games really want diagonal inputs enabled...
    // ------------------------------------------------------
    if (file_crc == 0x9d8fa05f)                 myConfig.dpad = DPAD_DIAGONALS;  // QOGO  needs diagonals
    if (file_crc == 0x9417ec36)                 myConfig.dpad = DPAD_DIAGONALS;  // QOGO2 needs diagonals    
    if (file_crc == 0x154702b2)                 myConfig.dpad = DPAD_DIAGONALS;  // QOGS2 needs diagonals    
    if (file_crc == 0x71600e71)                 myConfig.dpad = DPAD_DIAGONALS;  // QOGO  needs diagonals    
    
    if (file_crc == 0xf9934809)                 myConfig.dpad = DPAD_DIAGONALS;  // Reveal needs diagonals    
    
    if (file_crc == 0x0084b239)                 myConfig.isPAL = 1;     // Survivors Multi-Cart is PAL
    if (file_crc == 0x76a3d2e2)                 myConfig.isPAL = 1;     // Survivors MEGA-Cart is PAL
    
    if (file_crc == 0x07056b00)                 myConfig.isPAL   = 0;   // Memotech Pacman is an NTSC conversion
    if (file_crc == 0x8b28101a)                 myConfig.isPAL   = 0;   // Memotech Pacman is an NTSC conversion    
    if (file_crc == 0x87b9b54e)                 myConfig.isPAL   = 0;   // Memotech PowerPac is an NTSC conversion
    if (file_crc == 0xb8ed9f9e)                 myConfig.isPAL   = 0;   // Memotech PowerPac is an NTSC conversion    
    if (file_crc == 0xcac1f237)                 myConfig.isPAL   = 0;   // Memotech Telebunny is an NTSC conversion
    if (file_crc == 0xbd0e4513)                 myConfig.isPAL   = 0;   // Memotech Telebunny is an NTSC conversion
    if (file_crc == 0x24ae8ac0)                 myConfig.isPAL   = 0;   // Memotech Hustle Chummy is an NTSC conversion
    if (file_crc == 0x31ff229b)                 myConfig.isPAL   = 0;   // Memotech Hustle Chummy is an NTSC conversion
    if (file_crc == 0x025e77dc)                 myConfig.isPAL   = 0;   // Memotech OldMac is an NTSC conversion
    if (file_crc == 0x95e71c67)                 myConfig.isPAL   = 0;   // Memotech OldMac is an NTSC conversion
    
    if (file_crc == 0xdddd1396)                 myConfig.cvEESize = C24XX_256B; // Black Onyx is 256 bytes... Boxxle is 32K. Other EE are unknown...
    
    if (file_crc == 0x767a1f38)                 myConfig.maxSprites = 1;    // CreatiVision Sonic Invaders needs 4 sprites max
    if (file_crc == 0x011899cf)                 myConfig.maxSprites = 1;    // CreatiVision Sonic Invaders needs 4 sprites max (32K version)

    if (myConfig.isPAL)                         myConfig.vertSync = 0;      // If we are PAL, we can't sync to the DS 60Hz
    if (myConfig.gameSpeed)                     myConfig.vertSync = 0;      // If game speed isn't 100%, we can't sync to the DS 60Hz
}

// ----------------------------------------------------------
// Load configuration into memory where we can use it. 
// The configuration is stored in ColecoDS.DAT 
// ----------------------------------------------------------
void LoadConfig(void)
{
    FILE *fp;
    
    // -----------------------------------------------------------------
    // Start with defaults.. if we find a match in our config database
    // below, we will fill in the config with data read from the file.
    // -----------------------------------------------------------------
    SetDefaultGameConfig();
    
    fp = fopen("/data/ColecoDS.DAT", "rb");
    if (fp != NULL)
    {
        fread(&myGlobalConfig, sizeof(myGlobalConfig), 1, fp);  // Read Global Config
        fread(&AllConfigs, sizeof(AllConfigs), 1, fp);          // Read the full array of game configs
        fclose(fp);                                             // Close file - we work from memory now
        
        if (myGlobalConfig.config_ver != CONFIG_VER)
        {
            memset(&AllConfigs, 0x00, sizeof(AllConfigs));
            SetDefaultGameConfig();
            SetDefaultGlobalConfig();
            SaveConfig(FALSE);
        }
    }
    else    // Not found... init the entire database...
    {
        memset(&AllConfigs, 0x00, sizeof(AllConfigs));
        SetDefaultGameConfig();
        SetDefaultGlobalConfig();
        SaveConfig(FALSE);
    }}

// -------------------------------------------------------------------------
// Try to match our loaded game to a configuration my matching CRCs
// -------------------------------------------------------------------------
void FindConfig(void)
{
    // -----------------------------------------------------------------
    // Start with defaults.. if we find a match in our config database
    // below, we will fill in the config with data read from the file.
    // -----------------------------------------------------------------
    SetDefaultGameConfig();
    
    for (u16 slot=0; slot<MAX_CONFIGS; slot++)
    {
        if (AllConfigs[slot].game_crc == file_crc)  // Got a match?!
        {
            memcpy(&myConfig, &AllConfigs[slot], sizeof(struct Config_t));
            break;                           
        }
    }
}


// ------------------------------------------------------------------------------
// Options are handled here... we have a number of things the user can tweak
// and these options are applied immediately. The user can also save off 
// their option choices for the currently running game into the NINTV-DS.DAT
// configuration database. When games are loaded back up, NINTV-DS.DAT is read
// to see if we have a match and the user settings can be restored for the game.
// ------------------------------------------------------------------------------
struct options_t
{
    const char  *label;
    const char  *option[37];
    u8          *option_val;
    u8           option_max;
};

char msx_rom_str[16] = "MSX.ROM 64K";
char msx_rom_str_short[16] = "MSX";
const struct options_t Option_Table[3][20] =
{
    // Page 1
    {
        {"OVERLAY",        {"GENERIC", "WARGAMES", "MOUSETRAP", "GATEWAY", "SPY HUNTER", "FIX UP MIX UP", "BOULDER DASH", "QUINTA ROO", "2010", 
                            "ADAM KEYBOARD", "MSX KEYBOARD", "MTX KEYBOARD", "CREATIVISION", "ALPHA KEYBOARD", "EINSTEIN KBD", "SVI KEYBOARD", "SC-3000 KBD"},                                  &myConfig.overlay,    17},
        {"FRAME SKIP",     {"OFF", "SHOW 3/4", "SHOW 1/2"},                                                                                                                                     &myConfig.frameSkip,  3},
        {"FRAME BLEND",    {"OFF", "ON"},                                                                                                                                                       &myConfig.frameBlend, 2},
        {"VIDEO TYPE",     {"NTSC", "PAL"},                                                                                                                                                     &myConfig.isPAL,      2},
        {"MAX SPRITES",    {"32",  "4"},                                                                                                                                                        &myConfig.maxSprites, 2},
        {"VERT SYNC",      {"OFF", "ON"},                                                                                                                                                       &myConfig.vertSync,   2},    
        {"AUTO FIRE",      {"OFF", "B1 ONLY", "B2 ONLY", "BOTH"},                                                                                                                               &myConfig.autoFire,   4},
        {"TOUCH PAD",      {"PLAYER 1", "PLAYER 2"},                                                                                                                                            &myConfig.touchPad,   2},    
        {"JOYSTICK",       {"NORMAL", "DIAGONALS"},                                                                                                                                             &myConfig.dpad,       2},
        {"SPIN SPEED",     {"NORMAL", "FAST", "FASTEST", "SLOW", "SLOWEST", "OFF"},                                                                                                             &myConfig.spinSpeed,  6},
        {"MSX MAPPER",     {"GUESS","KONAMI 8K","ASCII 8K","KONAMI SCC","ASCII 16K","ZEMINA 8K","ZEMINA 16K","CROSSBLAIM","RESERVED","AT 0000H","AT 4000H","AT 8000H","64K LINEAR"},            &myConfig.msxMapper,  13},
        {"MSX BIOS",       {"C-BIOS 64K", msx_rom_str, "CX5M.ROM 32K", "HX-10.ROM 64K", "HB-10.ROM 16K", "FS1300.ROM 64K", "PV-7  8K"} ,                                                        &myConfig.msxBios,    7},
        {"MSX KEY ?",      {"DEFAULT","SHIFT","CTRL","ESC","F4","F5","6","7","8","9","A","B","C","D","E","F","G","H","I","J","K","L","M","N","O","P","Q","R","S","T","U","V","W","X","Y","Z"},  &myConfig.msxKey5,    36},
        {"RAM WIPE",       {"RANDOM", "CLEAR", "ADAM CPM"},                                                                                                                                     &myConfig.memWipe,    3},
        {"COLECO RAM",     {"NO MIRROR", "MIRRORED"},                                                                                                                                           &myConfig.mirrorRAM,  2},
        {NULL,             {"",      ""},                                                                                                                                                       NULL,                 1},
    },
    // Page 2
    {
        {"CPU INT",        {"CLEAR ON VDP", "AUTO CLEAR"},                                                                                                                                      &myConfig.clearInt,   2},
        {"MSX BEEPER",     {"OFF", "ON"},                                                                                                                                                       &myConfig.msxBeeper,  2},        
        {"CV EE SIZE",     {"128B", "256B", "512B", "1024B", "2048B", "4096B", "8192B", "16kB", "32kB"},                                                                                        &myConfig.cvEESize,   9},
        {"AY ENVELOPE",    {"NORMAL","NO RESET IDX"},                                                                                                                                           &myConfig.ayEnvelope, 2},
        {"Z80 CPU CORE",   {"DRZ80 (Faster)", "CZ80 (Better)"},                                                                                                                                 &myConfig.cpuCore,    2},
        {"GAME SPEED",     {"100%", "110%", "120%", "130%", "90%"},                                                                                                                             &myConfig.gameSpeed,  5},
        {"CVISION LOAD",   {"LEGACY (A/B)", "LINEAR", "32K BANKSWAP", "BIOS"},                                                                                                                  &myConfig.cvisionLoad,4},
        {NULL,             {"",      ""},                                                                                                                                                       NULL,                 1},
    },
    // Global Options
    {
        {"FPS",            {"OFF", "ON", "ON FULLSPEED"},                                                                                                                                       &myGlobalConfig.showFPS,        3},
        {"EMU TEXT",       {"OFF",  "ON"},                                                                                                                                                      &myGlobalConfig.emuText,        2},
        {"BIOS INFO",      {"HIDE", "SHOW"},                                                                                                                                                    &myGlobalConfig.showBiosInfo,   2},
        {"DEFAULT MSX",    {"C-BIOS 64K", msx_rom_str, "CX5M.ROM 32K", "HX-10.ROM 64K", "HB-10.ROM 16K", "FS1300.ROM 64K", "PV-7  8K"},                                                         &myGlobalConfig.defaultMSX,     7},
        {"MSX CART USE",   {"JOYPAD OVERLAY", "KEYBOARD OVL"},                                                                                                                                  &myGlobalConfig.msxCartOverlay, 2},
        {"DEF SPRITES",    {"32", "4"},                                                                                                                                                         &myGlobalConfig.defSprites,     2},
        {"SHOW BAD OPS",   {"NO", "YES", "FULL DEBUG"},                                                                                                                                         &myGlobalConfig.showBadOps,     3},
        
        {NULL,             {"",      ""},                                                                                                                                                       NULL,                           1},
    }
};              


// ------------------------------------------------------------------
// Display the current list of options for the user.
// ------------------------------------------------------------------
u8 display_options_list(bool bFullDisplay)
{
    char strBuf[35];
    int len=0;
    
    DSPrint(1,21, 0, (char *)"                              ");
    if (bFullDisplay)
    {
        while (true)
        {
            siprintf(strBuf, " %-12s : %-14s", Option_Table[option_table][len].label, Option_Table[option_table][len].option[*(Option_Table[option_table][len].option_val)]);
            DSPrint(1,5+len, (len==0 ? 2:0), strBuf); len++;
            if (Option_Table[option_table][len].label == NULL) break;
        }

        // Blank out rest of the screen... option menus are of different lengths...
        for (int i=len; i<16; i++) 
        {
            DSPrint(1,5+i, 0, (char *)"                               ");
        }
    }

    DSPrint(1,22, 0, (char *)"  B=EXIT, X=MORE, START=SAVE  ");
    return len;    
}


//*****************************************************************************
// Change Game Options for the current game
//*****************************************************************************
void colecoDSGameOptions(bool bIsGlobal)
{
    u8 optionHighlighted;
    u8 idx;
    bool bDone=false;
    int keys_pressed;
    int last_keys_pressed = 999;
    char strBuf[35];

    option_table = (bIsGlobal ? 2:0);
    
    idx=display_options_list(true);
    optionHighlighted = 0;
    while (keysCurrent() != 0)
    {
        WAITVBL;
    }
    while (!bDone)
    {
        keys_pressed = keysCurrent();
        if (keys_pressed != last_keys_pressed)
        {
            last_keys_pressed = keys_pressed;
            if (keysCurrent() & KEY_UP) // Previous option
            {
                siprintf(strBuf, " %-12s : %-14s", Option_Table[option_table][optionHighlighted].label, Option_Table[option_table][optionHighlighted].option[*(Option_Table[option_table][optionHighlighted].option_val)]);
                DSPrint(1,5+optionHighlighted,0, strBuf);
                if (optionHighlighted > 0) optionHighlighted--; else optionHighlighted=(idx-1);
                siprintf(strBuf, " %-12s : %-14s", Option_Table[option_table][optionHighlighted].label, Option_Table[option_table][optionHighlighted].option[*(Option_Table[option_table][optionHighlighted].option_val)]);
                DSPrint(1,5+optionHighlighted,2, strBuf);
            }
            if (keysCurrent() & KEY_DOWN) // Next option
            {
                siprintf(strBuf, " %-12s : %-14s", Option_Table[option_table][optionHighlighted].label, Option_Table[option_table][optionHighlighted].option[*(Option_Table[option_table][optionHighlighted].option_val)]);
                DSPrint(1,5+optionHighlighted,0, strBuf);
                if (optionHighlighted < (idx-1)) optionHighlighted++;  else optionHighlighted=0;
                siprintf(strBuf, " %-12s : %-14s", Option_Table[option_table][optionHighlighted].label, Option_Table[option_table][optionHighlighted].option[*(Option_Table[option_table][optionHighlighted].option_val)]);
                DSPrint(1,5+optionHighlighted,2, strBuf);
            }

            if (keysCurrent() & KEY_RIGHT)  // Toggle option clockwise
            {
                *(Option_Table[option_table][optionHighlighted].option_val) = (*(Option_Table[option_table][optionHighlighted].option_val) + 1) % Option_Table[option_table][optionHighlighted].option_max;
                siprintf(strBuf, " %-12s : %-14s", Option_Table[option_table][optionHighlighted].label, Option_Table[option_table][optionHighlighted].option[*(Option_Table[option_table][optionHighlighted].option_val)]);
                DSPrint(1,5+optionHighlighted,2, strBuf);
            }
            if (keysCurrent() & KEY_LEFT)  // Toggle option counterclockwise
            {
                if ((*(Option_Table[option_table][optionHighlighted].option_val)) == 0)
                    *(Option_Table[option_table][optionHighlighted].option_val) = Option_Table[option_table][optionHighlighted].option_max -1;
                else
                    *(Option_Table[option_table][optionHighlighted].option_val) = (*(Option_Table[option_table][optionHighlighted].option_val) - 1) % Option_Table[option_table][optionHighlighted].option_max;
                siprintf(strBuf, " %-12s : %-14s", Option_Table[option_table][optionHighlighted].label, Option_Table[option_table][optionHighlighted].option[*(Option_Table[option_table][optionHighlighted].option_val)]);
                DSPrint(1,5+optionHighlighted,2, strBuf);
            }
            if (keysCurrent() & KEY_START)  // Save Options
            {
                SaveConfig(TRUE);
            }
            if (keysCurrent() & (KEY_X)) // Toggle Table
            {
                option_table = (bIsGlobal ? 2: ((option_table + 1) % 2));
                idx=display_options_list(true);
                optionHighlighted = 0;
                while (keysCurrent() != 0)
                {
                    WAITVBL;
                }
            }
            if ((keysCurrent() & KEY_B) || (keysCurrent() & KEY_A))  // Exit options
            {
                option_table = 0;   // Reset for next time
                break;
            }
        }
        showRandomPreviewSnaps();
        swiWaitForVBlank();
    }

    // Give a third of a second time delay...
    for (int i=0; i<20; i++)
    {
        swiWaitForVBlank();
    }
    
    // We can't support PAL with Vertical Sync 
    if (myConfig.isPAL) myConfig.vertSync = 0;
    
    return;
}

//*****************************************************************************
// Change Keymap Options for the current game
//*****************************************************************************
void DisplayKeymapName(u32 uY) 
{
  char szCha[34];

  siprintf(szCha," PAD UP    : %-17s",szKeyName[myConfig.keymap[0]]);
  DSPrint(1, 6,(uY==  6 ? 2 : 0),szCha);
  siprintf(szCha," PAD DOWN  : %-17s",szKeyName[myConfig.keymap[1]]);
  DSPrint(1, 7,(uY==  7 ? 2 : 0),szCha);
  siprintf(szCha," PAD LEFT  : %-17s",szKeyName[myConfig.keymap[2]]);
  DSPrint(1, 8,(uY==  8 ? 2 : 0),szCha);
  siprintf(szCha," PAD RIGHT : %-17s",szKeyName[myConfig.keymap[3]]);
  DSPrint(1, 9,(uY== 9 ? 2 : 0),szCha);
  siprintf(szCha," KEY A     : %-17s",szKeyName[myConfig.keymap[4]]);
  DSPrint(1,10,(uY== 10 ? 2 : 0),szCha);
  siprintf(szCha," KEY B     : %-17s",szKeyName[myConfig.keymap[5]]);
  DSPrint(1,11,(uY== 11 ? 2 : 0),szCha);
  siprintf(szCha," KEY X     : %-17s",szKeyName[myConfig.keymap[6]]);
  DSPrint(1,12,(uY== 12 ? 2 : 0),szCha);
  siprintf(szCha," KEY Y     : %-17s",szKeyName[myConfig.keymap[7]]);
  DSPrint(1,13,(uY== 13 ? 2 : 0),szCha);
  siprintf(szCha," KEY R     : %-17s",szKeyName[myConfig.keymap[8]]);
  DSPrint(1,14,(uY== 14 ? 2 : 0),szCha);
  siprintf(szCha," KEY L     : %-17s",szKeyName[myConfig.keymap[9]]);
  DSPrint(1,15,(uY== 15 ? 2 : 0),szCha);
  siprintf(szCha," START     : %-17s",szKeyName[myConfig.keymap[10]]);
  DSPrint(1,16,(uY== 16 ? 2 : 0),szCha);
  siprintf(szCha," SELECT    : %-17s",szKeyName[myConfig.keymap[11]]);
  DSPrint(1,17,(uY== 17 ? 2 : 0),szCha);
}

u8 keyMapType = 0;
void SwapKeymap(void)
{
    keyMapType = (keyMapType+1) % 6;
    switch (keyMapType)
    {
        case 0: MapPlayer1();  break;
        case 1: MapPlayer2();  break;
        case 2: MapQAOP();     break;
        case 3: MapWASD();     break;
        case 4: MapZCPeriod(); break;
        case 5: MapArrows();   break;
    }
}


// ------------------------------------------------------------------------------
// Allow the user to change the key map for the current game and give them
// the option of writing that keymap out to a configuration file for the game.
// ------------------------------------------------------------------------------
void colecoDSChangeKeymap(void) 
{
  u32 ucHaut=0x00, ucBas=0x00,ucL=0x00,ucR=0x00,ucY= 6, bOK=0, bIndTch=0;

  // ------------------------------------------------------
  // Clear the screen so we can put up Key Map infomation
  // ------------------------------------------------------
  unsigned short dmaVal =  *(bgGetMapPtr(bg0b) + 24*32);
  dmaFillWords(dmaVal | (dmaVal<<16),(void*) bgGetMapPtr(bg1b)+5*32*2,32*19*2);
    
  // --------------------------------------------------
  // Give instructions to the user...
  // --------------------------------------------------
  DSPrint(1 ,19,0,("   D-PAD : CHANGE KEY MAP    "));
  DSPrint(1 ,20,0,("       B : RETURN MAIN MENU  "));
  DSPrint(1 ,21,0,("       X : SWAP KEYMAP TYPE  "));
  DSPrint(1 ,22,0,("   START : SAVE KEYMAP       "));
  DisplayKeymapName(ucY);
  
  // -----------------------------------------------------------------------
  // Clear out any keys that might be pressed on the way in - make sure
  // NDS keys are not being pressed. This prevents the inadvertant A key
  // that enters this menu from also being acted on in the keymap...
  // -----------------------------------------------------------------------
  while ((keysCurrent() & (KEY_TOUCH | KEY_B | KEY_A | KEY_X | KEY_UP | KEY_DOWN))!=0)
      ;
  WAITVBL;
 
  while (!bOK) {
    if (keysCurrent() & KEY_UP) {
      if (!ucHaut) {
        DisplayKeymapName(32);
        ucY = (ucY == 6 ? 17 : ucY -1);
        bIndTch = myConfig.keymap[ucY-6];
        ucHaut=0x01;
        DisplayKeymapName(ucY);
      }
      else {
        ucHaut++;
        if (ucHaut>10) ucHaut=0;
      } 
    }
    else {
      ucHaut = 0;
    }  
    if (keysCurrent() & KEY_DOWN) {
      if (!ucBas) {
        DisplayKeymapName(32);
        ucY = (ucY == 17 ? 6 : ucY +1);
        bIndTch = myConfig.keymap[ucY-6];
        ucBas=0x01;
        DisplayKeymapName(ucY);
      }
      else {
        ucBas++;
        if (ucBas>10) ucBas=0;
      } 
    }
    else {
      ucBas = 0;
    }  
      
    if (keysCurrent() & KEY_START) 
    {
        SaveConfig(true); // Save options
    }
      
    if (keysCurrent() & KEY_B) 
    {
      bOK = 1;  // Exit menu
    }
      
    if (keysCurrent() & KEY_LEFT) 
    {
        if (ucL == 0) {
          bIndTch = (bIndTch == 0 ? (MAX_KEY_OPTIONS-1) : bIndTch-1);
          ucL=1;
          myConfig.keymap[ucY-6] = bIndTch;
          DisplayKeymapName(ucY);
        }
        else {
          ucL++;
          if (ucL > 7) ucL = 0;
        }
    }
    else 
    {
        ucL = 0;
    }
      
    if (keysCurrent() & KEY_RIGHT) 
    {
        if (ucR == 0) 
        {
          bIndTch = (bIndTch == (MAX_KEY_OPTIONS-1) ? 0 : bIndTch+1);
          ucR=1;
          myConfig.keymap[ucY-6] = bIndTch;
          DisplayKeymapName(ucY);
        }
        else 
        {
          ucR++;
          if (ucR > 7) ucR = 0;
        }
    }
    else
    {
        ucR=0;
    }
      
    // Swap Player 1 and Player 2 keymap
    if (keysCurrent() & KEY_X)
    {
        SwapKeymap();
        bIndTch = myConfig.keymap[ucY-6];
        DisplayKeymapName(ucY);
        while (keysCurrent() & KEY_X) 
            ;
        WAITVBL
    }
    showRandomPreviewSnaps();
    swiWaitForVBlank();
  }
  while (keysCurrent() & KEY_B);
}


// ----------------------------------------------------------------------------------
// At the bottom of the main screen we show the currently selected filename and CRC
// ----------------------------------------------------------------------------------
void DisplayFileName(void)
{
    siprintf(szName,"%s",gpFic[ucGameChoice].szName);
    for (u8 i=strlen(szName)-1; i>0; i--) if (szName[i] == '.') {szName[i]=0;break;}
    if (strlen(szName)>30) szName[30]='\0';
    DSPrint((16 - (strlen(szName)/2)),21,0,szName);
    if (strlen(gpFic[ucGameChoice].szName) >= 35)   // If there is more than a few characters left, show it on the 2nd line
    {
        siprintf(szName,"%s",gpFic[ucGameChoice].szName+30);
        for (u8 i=strlen(szName)-1; i>0; i--) if (szName[i] == '.') {szName[i]=0;break;}
        if (strlen(szName)>30) szName[30]='\0';
        DSPrint((16 - (strlen(szName)/2)),22,0,szName);
    }
}

//*****************************************************************************
// Display colecoDSlus screen and change options "main menu"
//*****************************************************************************
void affInfoOptions(u32 uY) 
{
    DSPrint(2, 8,(uY== 8 ? 2 : 0),("         LOAD  GAME         "));
    DSPrint(2,10,(uY==10 ? 2 : 0),("         PLAY  GAME         "));
    DSPrint(2,12,(uY==12 ? 2 : 0),("     REDEFINE  KEYS         "));
    DSPrint(2,14,(uY==14 ? 2 : 0),("         GAME  OPTIONS      "));
    DSPrint(2,16,(uY==16 ? 2 : 0),("       GLOBAL  OPTIONS      "));
    DSPrint(2,18,(uY==18 ? 2 : 0),("         QUIT  EMULATOR     "));
}

// --------------------------------------------------------------------
// Some main menu selections don't make sense without a game loaded.
// --------------------------------------------------------------------
void NoGameSelected(u32 ucY)
{
    unsigned short dmaVal = *(bgGetMapPtr(bg1b)+24*32); 
    while (keysCurrent()  & (KEY_START | KEY_A));
    dmaFillWords(dmaVal | (dmaVal<<16),(void*) bgGetMapPtr(bg1b)+5*32*2,32*18*2);
    DSPrint(5,10,0,("   NO GAME SELECTED   ")); 
    DSPrint(5,12,0,("  PLEASE, USE OPTION  ")); 
    DSPrint(5,14,0,("      LOAD  GAME      "));
    while (!(keysCurrent()  & (KEY_START | KEY_A)));
    while (keysCurrent()  & (KEY_START | KEY_A));
    dmaFillWords(dmaVal | (dmaVal<<16),(void*) bgGetMapPtr(bg1b)+5*32*2,32*18*2);
    affInfoOptions(ucY);
}

/*********************************************************************************
 * Look for MSX 'AB' header in the ROM file or possibly 0xF331 for SVI ROMs
 ********************************************************************************/
void CheckRomHeaders(char *szGame)
{
  FILE* handle = fopen(szGame, "rb");  
  if (handle)
  {
      // ------------------------------------------------------------------------------------------
      // MSX Header Bytes:
      //  0 DEFB "AB" ; expansion ROM header
      //  2 DEFW initcode ; start of the init code, 0 if no initcode
      //  4 DEFW callstat; pointer to CALL statement handler, 0 if no such handler
      //  6 DEFW device; pointer to expansion device handler, 0 if no such handler
      //  8 DEFW basic ; pointer to the start of a tokenized basicprogram, 0 if no basicprogram
      // ------------------------------------------------------------------------------------------
      memset(ROM_Memory, 0xFF, 0x400A);
      fread((void*) ROM_Memory, 0x400A, 1, handle); 
      fclose(handle);
      
      // ---------------------------------------------------------------------
      // Do some auto-detection for game ROM. MSX games have 'AB' in their
      // header and we also want to track the INIT address for those ROMs
      // so we can take a better guess at mapping them into our Slot1 memory
      // ---------------------------------------------------------------------
      msx_init = 0x4000;
      msx_basic = 0x0000;
      if ((ROM_Memory[0] == 'A') && (ROM_Memory[1] == 'B'))
      {
          msx_mode = 1;      // MSX roms start with AB (might be in bank 0)
          msx_init = ROM_Memory[2] | (ROM_Memory[3]<<8);
          if (msx_init == 0x0000) msx_basic = ROM_Memory[8] | (ROM_Memory[8]<<8);
          if (msx_init == 0x0000)   // If 0, check for 2nd header... this might be a dummy
          {
              if ((ROM_Memory[0x4000] == 'A') && (ROM_Memory[0x4001] == 'B'))  
              {
                  msx_init = ROM_Memory[0x4002] | (ROM_Memory[0x4003]<<8);
                  if (msx_init == 0x0000) msx_basic = ROM_Memory[0x4008] | (ROM_Memory[0x4009]<<8);
              }
          }
      }
      else if ((ROM_Memory[0x4000] == 'A') && (ROM_Memory[0x4001] == 'B'))  
      {
          msx_mode = 1;      // MSX roms start with AB (might be in bank 1)
          msx_init = ROM_Memory[0x4002] | (ROM_Memory[0x4003]<<8);
          if (msx_init == 0x0000) msx_basic = ROM_Memory[0x4008] | (ROM_Memory[0x4009]<<8);
      }
      // Check for Spectravideo SVI Cart Header...
      else if ((ROM_Memory[0] == 0xF3) && (ROM_Memory[1] == 0x31))
      {
          if ((strstr(gpFic[ucGameChoice].szName, ".rom")) || (strstr(gpFic[ucGameChoice].szName, ".ROM")))  svi_mode = 2;       // Detected SVI Cartridge header...
      }
  }
}


void ReadFileCRCAndConfig(void)
{    
    u8 checkCOM = 0;
    u8 checkROM = 0;
    getfile_crc(gpFic[ucGameChoice].szName);
    
    u8 cas_load = 0;
    sg1000_mode = 0;
    pv2000_mode = 0;
    sordm5_mode = 0;
    memotech_mode = 0;
    pencil2_mode = 0;
    einstein_mode = 0;
    msx_mode = 0;
    svi_mode = 0;
    adam_mode = 0;
    creativision_mode = 0;
    coleco_mode = 0;
    
    keyMapType = 0;
    
    if (strstr(gpFic[ucGameChoice].szName, ".sg") != 0) sg1000_mode = 1;    // SG-1000 mode
    if (strstr(gpFic[ucGameChoice].szName, ".SG") != 0) sg1000_mode = 1;    // SG-1000 mode
    if (strstr(gpFic[ucGameChoice].szName, ".sc") != 0) sg1000_mode = 2;    // SC-3000 mode
    if (strstr(gpFic[ucGameChoice].szName, ".SC") != 0) sg1000_mode = 2;    // SC-3000 mode
    if (strstr(gpFic[ucGameChoice].szName, ".pv") != 0) pv2000_mode = 2;    // PV-2000 mode
    if (strstr(gpFic[ucGameChoice].szName, ".PV") != 0) pv2000_mode = 2;    // PV-2000 mode
    if (strstr(gpFic[ucGameChoice].szName, ".CV") != 0) creativision_mode = 1;
    if (strstr(gpFic[ucGameChoice].szName, ".cv") != 0) creativision_mode = 1;
    if (strstr(gpFic[ucGameChoice].szName, ".m5") != 0) sordm5_mode = 1;
    if (strstr(gpFic[ucGameChoice].szName, ".M5") != 0) sordm5_mode = 1;
    if (strstr(gpFic[ucGameChoice].szName, ".mtx") != 0) memotech_mode = 2;
    if (strstr(gpFic[ucGameChoice].szName, ".MTX") != 0) memotech_mode = 2;
    if (strstr(gpFic[ucGameChoice].szName, ".run") != 0) memotech_mode = 1;
    if (strstr(gpFic[ucGameChoice].szName, ".RUN") != 0) memotech_mode = 1;
    if (strstr(gpFic[ucGameChoice].szName, ".msx") != 0) msx_mode = 1;
    if (strstr(gpFic[ucGameChoice].szName, ".MSX") != 0) msx_mode = 1;
    if (strstr(gpFic[ucGameChoice].szName, ".cas") != 0) cas_load = 1;
    if (strstr(gpFic[ucGameChoice].szName, ".CAS") != 0) cas_load = 1;
    if (strstr(gpFic[ucGameChoice].szName, ".ddp") != 0) adam_mode = 1;
    if (strstr(gpFic[ucGameChoice].szName, ".DDP") != 0) adam_mode = 1;
    if (strstr(gpFic[ucGameChoice].szName, ".dsk") != 0) adam_mode = 1;
    if (strstr(gpFic[ucGameChoice].szName, ".DSK") != 0) adam_mode = 1;
    if (strstr(gpFic[ucGameChoice].szName, ".pen") != 0) pencil2_mode = 1;
    if (strstr(gpFic[ucGameChoice].szName, ".PEN") != 0) pencil2_mode = 1;
    if (strstr(gpFic[ucGameChoice].szName, ".com") != 0) checkCOM = 1;
    if (strstr(gpFic[ucGameChoice].szName, ".COM") != 0) checkCOM = 1;
    if (strstr(gpFic[ucGameChoice].szName, ".rom") != 0) checkROM = 1;
    if (strstr(gpFic[ucGameChoice].szName, ".ROM") != 0) checkROM = 1;
    if (strstr(gpFic[ucGameChoice].szName, ".col") != 0) checkROM = 1;  // Coleco types - check if MSX or SVI
    if (strstr(gpFic[ucGameChoice].szName, ".COL") != 0) checkROM = 1;  // Coleco types - check if MSX or SVI
    
    if (checkROM) CheckRomHeaders(gpFic[ucGameChoice].szName);   // See if we've got an MSX or SVI cart - this may set msx_mode=1 or svi_mode=2
    
    if (checkCOM)   // COM is usually Einstein... but we also support it for MTX for some games
    {
        if ( (file_crc == 0xb35a8beb)  ||  // SGM2M - alt
             (file_crc == 0x8e1dd825)  ||  // 1STLETTR.COM
             (file_crc == 0xefd652df)  ||  // ALPHA.COM
             (file_crc == 0x2fa8a871)  ||  // ANGLE.COM
             (file_crc == 0x640efa8d)  ||  // ASTROPAC.COM
             (file_crc == 0x1a50a1e3)  ||  // BUCK.COM
             (file_crc == 0xeb809665)  ||  // CSCPM.COM
             (file_crc == 0xd79a86b7)  ||  // DEFENDER.COM
             (file_crc == 0xac7195d4)  ||  // DT.COM
             (file_crc == 0xcd43d48d)  ||  // H-CHUMY.COM
             (file_crc == 0x62dfae2d)  ||  // L9ADV.COM
             (file_crc == 0xb52a7312)  ||  // NEMO.COM
             (file_crc == 0x7ad2b90e)  ||  // OBLOIDS.COM
             (file_crc == 0x36549adc)  ||  // OLDMAC.COM
             (file_crc == 0x2f3e6416)  ||  // ORBCPM.COM
             (file_crc == 0x8af83858)  ||  // PACMAN.COM
             (file_crc == 0x585478f4)  ||  // POWERPAC.COM
             (file_crc == 0x542051ce)  ||  // QUASAR.COM
             (file_crc == 0x5ed7b7d8)  ||  // REV.COM
             (file_crc == 0x1464e8b0)  ||  // RUN.COM
             (file_crc == 0x6b2d4eb9)  ||  // SASA.COM
             (file_crc == 0x7812bb8c)  ||  // SMG2M.COM
             (file_crc == 0x02fb1412)  ||  // SMG.COM
             (file_crc == 0xcbd19c59)  ||  // SPECTRON.COM
             (file_crc == 0x9f4b067d)  ||  // STAR.COM
             (file_crc == 0x6b67dd68)  ||  // TBUNNY.COM
             (file_crc == 0xcab321fc)  ||  // TLOAD.COM
             (file_crc == 0x8ff4ef96)  ||  // TURBO.COM
             (file_crc == 0x9db0a5d7)  ||  // TV.COM
             (file_crc == 0x46a242b0) )    // ZOMBNEAR.COM
        {
            memotech_mode = 3;  // Memotech MTX .COM file
        }
        else
        {
            einstein_mode = 1;  // Tatung Einstein .COM file
        }
    }
    
    // --------------------------------------------------------------------------
    // If a .cas file is picked, we need to figure out what machine it's for...
    // --------------------------------------------------------------------------
    if (cas_load)
    {
        FILE *fp;
        
        fp = fopen(gpFic[ucGameChoice].szName, "rb");
        if (fp != NULL)
        {
            char headerBytes[32];
            fread(headerBytes, 32, 1, fp);
            for (u8 i=0; i<30; i++)
            {
                if ((headerBytes[i] == 0x55) && (headerBytes[i+2] == 0x55) && (headerBytes[i+2] == 0x55))
                {
                    svi_mode = 1;
                    break;
                }
            }
            fclose(fp);
            if (svi_mode == 0) msx_mode = 2;        // if not SVI, assume MSX
        }
    }
    
    FindConfig();    // Try to find keymap and config for this file...
    
    // --------------------------------------------
    // A few special cases for the CreatiVision
    // --------------------------------------------
    if (file_crc == 0x8375203e) myConfig.cvisionLoad = 3;  // Special load of 16K CSL BIOS at C000-FFFF
    if (file_crc == 0x77afd38b) myConfig.cvisionLoad = 3;  // Special load of 16K CSL BIOS at C000-FFFF
}

// --------------------------------------------------------------------
// Let the user select new options for the currently loaded game...
// --------------------------------------------------------------------
void colecoDSChangeOptions(void) 
{
  u32 ucHaut=0x00, ucBas=0x00,ucA=0x00,ucY= 8, bOK=0;
  
  // Upper Screen Background
  videoSetMode(MODE_0_2D | DISPLAY_BG0_ACTIVE | DISPLAY_BG1_ACTIVE | DISPLAY_SPR_1D_LAYOUT | DISPLAY_SPR_ACTIVE);
  vramSetBankA(VRAM_A_MAIN_BG);
  vramSetBankB(VRAM_B_MAIN_SPRITE_0x06400000);
  bg0 = bgInit(0, BgType_Text8bpp, BgSize_T_256x512, 31,0);
  bg1 = bgInit(1, BgType_Text8bpp, BgSize_T_256x512, 29,0);
  bgSetPriority(bg0,1);bgSetPriority(bg1,0);
  decompress(topscreenTiles, bgGetGfxPtr(bg0), LZ77Vram);
  decompress(topscreenMap, (void*) bgGetMapPtr(bg0), LZ77Vram);
  dmaCopy((void*) topscreenPal,(void*) BG_PALETTE,256*2);
  unsigned short dmaVal =  *(bgGetMapPtr(bg0) + 51*32);
  dmaFillWords(dmaVal | (dmaVal<<16),(void*) bgGetMapPtr(bg1),32*24*2);
  DSPrint(28,23,1,"V");DSPrint(29,23,1,VERSIONCLDS);

  // Lower Screen Background
  BottomScreenOptions();

  affInfoOptions(ucY);
  
  if (ucGameChoice != -1) 
  { 
      DisplayFileName();
  }
  
  while (!bOK) {
    if (keysCurrent()  & KEY_UP) {
      if (!ucHaut) {
        affInfoOptions(32);
        ucY = (ucY == 8 ? 18 : ucY -2);
        ucHaut=0x01;
        affInfoOptions(ucY);
      }
      else {
        ucHaut++;
        if (ucHaut>10) ucHaut=0;
      } 
    }
    else {
      ucHaut = 0;
    }  
    if (keysCurrent()  & KEY_DOWN) {
      if (!ucBas) {
        affInfoOptions(32);
        ucY = (ucY == 18 ? 8 : ucY +2);
        ucBas=0x01;
        affInfoOptions(ucY);
      }
      else {
        ucBas++;
        if (ucBas>10) ucBas=0;
      } 
    }
    else {
      ucBas = 0;
    }  
    if (keysCurrent()  & KEY_A) {
      if (!ucA) {
        ucA = 0x01;
        switch (ucY) {
          case 8 :      // LOAD GAME
            colecoDSLoadFile();
            dmaFillWords(dmaVal | (dmaVal<<16),(void*) bgGetMapPtr(bg1b)+5*32*2,32*19*2);
            if (ucGameChoice != -1) 
            { 
                ReadFileCRCAndConfig(); // Get CRC32 of the file and read the config/keys
                DisplayFileName();      // And put up the filename on the bottom screen
            }
            ucY = 10;
            affInfoOptions(ucY);
            break;
          case 10 :     // PLAY GAME
            if (ucGameChoice != -1) 
            { 
              bOK = 1;
            }
            else 
            {    
                NoGameSelected(ucY);
            }
            break;
          case 12 :     // REDEFINE KEYS
            if (ucGameChoice != -1) 
            { 
                colecoDSChangeKeymap();
                dmaFillWords(dmaVal | (dmaVal<<16),(void*) bgGetMapPtr(bg1b)+5*32*2,32*18*2);
                affInfoOptions(ucY);
                DisplayFileName();
            }
            else 
            { 
                NoGameSelected(ucY);
            }
            break;
          case 14 :     // GAME OPTIONS
            if (ucGameChoice != -1) 
            { 
                colecoDSGameOptions(false);
                dmaFillWords(dmaVal | (dmaVal<<16),(void*) bgGetMapPtr(bg1b)+5*32*2,32*18*2);
                affInfoOptions(ucY);
                DisplayFileName();
            }
            else 
            {    
               NoGameSelected(ucY);
            }
            break;                
                
          case 16 :     // GLOBAL OPTIONS
            colecoDSGameOptions(true);
            dmaFillWords(dmaVal | (dmaVal<<16),(void*) bgGetMapPtr(bg1b)+5*32*2,32*18*2);
            affInfoOptions(ucY);
            DisplayFileName();
            break;
                
          case 18 :     // QUIT EMULATOR
            exit(1);
            break;
        }
      }
    }
    else
      ucA = 0x00;
    if (keysCurrent()  & KEY_START) {
      if (ucGameChoice != -1) 
      {
        bOK = 1;
      }
      else 
      {
        NoGameSelected(ucY);
      }
    }
    showRandomPreviewSnaps();
    swiWaitForVBlank();
  }
  while (keysCurrent()  & (KEY_START | KEY_A));
}

//*****************************************************************************
// Displays a message on the screen
//*****************************************************************************

void DSPrint(int iX,int iY,int iScr,char *szMessage) {
  u16 *pusEcran,*pusMap;
  u16 usCharac;
  char szTexte[128],*pTrTxt=szTexte;
  
  strcpy(szTexte,szMessage);
  strupr(szTexte);
  pusEcran=(u16*) (iScr != 1 ? bgGetMapPtr(bg1b) : bgGetMapPtr(bg1))+iX+(iY<<5);
  pusMap=(u16*) (iScr != 1 ? (iScr == 6 ? bgGetMapPtr(bg0b)+24*32 : (iScr == 0 ? bgGetMapPtr(bg0b)+24*32 : bgGetMapPtr(bg0b)+26*32 )) : bgGetMapPtr(bg0)+51*32 );
    
  while((*pTrTxt)!='\0' )
  {
    char ch = *pTrTxt;
    if (ch >= 'a' && ch <= 'z') ch -= 32; // Faster than strcpy/strtoupper
    usCharac=0x0000;
    if ((ch) == '|')
      usCharac=*(pusMap);
    else if (((ch)<' ') || ((ch)>'_'))
      usCharac=*(pusMap);
    else if((ch)<'@')
      usCharac=*(pusMap+(ch)-' ');
    else
      usCharac=*(pusMap+32+(ch)-'@');
    *pusEcran++=usCharac;
    pTrTxt++;
  }
}

/******************************************************************************
* Routine FadeToColor :  Fade from background to black or white
******************************************************************************/
void FadeToColor(unsigned char ucSens, unsigned short ucBG, unsigned char ucScr, unsigned char valEnd, unsigned char uWait) {
  unsigned short ucFade;
  unsigned char ucBcl;

  // Fade-out vers le noir
  if (ucScr & 0x01) REG_BLDCNT=ucBG;
  if (ucScr & 0x02) REG_BLDCNT_SUB=ucBG;
  if (ucSens == 1) {
    for(ucFade=0;ucFade<valEnd;ucFade++) {
      if (ucScr & 0x01) REG_BLDY=ucFade;
      if (ucScr & 0x02) REG_BLDY_SUB=ucFade;
      for (ucBcl=0;ucBcl<uWait;ucBcl++) {
        swiWaitForVBlank();
      }
    }
  }
  else {
    for(ucFade=16;ucFade>valEnd;ucFade--) {
      if (ucScr & 0x01) REG_BLDY=ucFade;
      if (ucScr & 0x02) REG_BLDY_SUB=ucFade;
      for (ucBcl=0;ucBcl<uWait;ucBcl++) {
        swiWaitForVBlank();
      }
    }
  }
}

void _putchar(char character) {}

// End of file
