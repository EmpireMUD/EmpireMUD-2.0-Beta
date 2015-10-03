/* ************************************************************************
*   File: constants.c                                     EmpireMUD 2.0b2 *
*  Usage: Numeric and string contants used by the MUD                     *
*                                                                         *
*  EmpireMUD code base by Paul Clarke, (C) 2000-2015                      *
*  All rights reserved.  See license.doc for complete information.        *
*                                                                         *
*  EmpireMUD based upon CircleMUD 3.0, bpl 17, by Jeremy Elson.           *
*  CircleMUD (C) 1993, 94 by the Trustees of the Johns Hopkins University *
*  CircleMUD is based on DikuMUD, Copyright (C) 1990, 1991.               *
************************************************************************ */

#include "conf.h"
#include "sysdep.h"

#include "structs.h"
#include "skills.h"
#include "utils.h"
#include "interpreter.h"	/* alias_data */
#include "vnums.h"
#include "olc.h"

/**
* Contents:
*   EmpireMUD Constants
*   Adventure Constants
*   Player Constants
*   Direction And Room Constants
*   Character Constants
*   Craft Recipe Constants
*   Empire Constants
*   Mob Constants
*   Item Contants
*   OLC Constants
*   Room/World Constants
*   Skill Constants
*   Trigger Constants
*   Misc Constants
*/

// external funcs
void afk_notify(char_data *ch);
void tog_informative(char_data *ch);
void tog_mapcolor(char_data *ch);
void tog_political(char_data *ch);


 //////////////////////////////////////////////////////////////////////////////
//// EMPIREMUD CONSTANTS /////////////////////////////////////////////////////

// Shown on the "version" command and sent over MSSP
const char *version = "EmpireMUD 2.0 beta 2";


// data for the built-in game levels -- this adapts itself if you reduce the number of immortal levels
const char *level_names[][2] = {
		{ "N/A", "Not Started" },
		{ "UNAPP", "Unapproved" },
		{ "APPR", "Approved" },
		{ "GOD", "God" },
	#if (LVL_START_IMM < LVL_TOP)
		{ "IMM", "Immortal" },
		#if (LVL_ASST < LVL_TOP && LVL_ASST > LVL_START_IMM)
			{ "ASST", "Assistant" },
			#if (LVL_CIMPL < LVL_TOP && LVL_CIMPL > LVL_ASST)
				{ "CIMPL", "Co-Implementor" },
			#endif
		#endif
	#endif
		{ "IMPL", "Implementor" }
};


// reboot messages information
// TODO could auto-detect number of strings (consider adding a count_strings function)
// TODO could move to a file
const int num_of_reboot_strings = 3;
const char *reboot_strings[] = {
	"   EmpireMUD is performing a reboot. This process generally takes ten to\r\n"
	"fifteen seconds and will not disconnect you. Most character actions are\r\n"
	"not affected, although fighting will stop.\r\n",

	"Q. What is a reboot?\r\n"
	"A. A reboot allows the mud to reload code without disconnecting players.\r\n"
	"This means that you won't have to go through the hassle of logging in again.\r\n",

	"Q. What if I lose something in the reboot?\r\n"
	"A. Nothing is lost because your character and equipment are saved when the\r\n"
	"reboot happens. The world is also saved so if you were building or chopping\r\n"
	"you won't have to start over.\r\n",

	"\n"
};


// for the reboot_control -- SCMD_REBOOT, SCMD_SHUTDOWN
const char *reboot_type[] = { "reboot", "shutdown" };


 //////////////////////////////////////////////////////////////////////////////
//// ADVENTURE CONSTANTS /////////////////////////////////////////////////////

// ADV_x
const char *adventure_flags[] = {
	"IN-DEVELOPMENT",
	"LOCK-LEVEL-ON-ENTER",
	"LOCK-LEVEL-ON-COMBAT",
	"!NEARBY",
	"ROTATABLE",
	"CONFUSING-RANDOMS",
	"!NEWBIE",
	"NEWBIE-ONLY",
	"\n"
};


// ADV_LINKF_x
const char *adventure_link_flags[] = {
	"CLAIMED-OK",
	"CITY-ONLY",
	"!CITY",
	"\n"
};


// ADV_LINK_x
const char *adventure_link_types[] = {
	"BDG-EXISTING",
	"BDG-NEW",
	"PORTAL-WORLD",
	"PORTAL-BDG-EXISTING",
	"PORTAL-BDG-NEW",
	"TIME-LIMIT",
	"NOT-NEAR-SELF",
	"\n"
};


// ADV_SPAWN_x
const char *adventure_spawn_types[] = {
	"MOB",
	"OBJ",
	"\n"
};


// INST_x
const char *instance_flags[] = {
	"COMPLETED",
	"\n"
};


// RMT_x
const char *room_template_flags[] = {
	"OUTDOOR",
	"DARK",
	"LIGHT",
	"!MOB",
	"PEACEFUL",
	"NEED-BOAT",
	"!TELEPORT",
	"LOOK-OUT",
	"!LOCATION",
	"\n"
};


 //////////////////////////////////////////////////////////////////////////////
//// PLAYER CONSTANTS ////////////////////////////////////////////////////////

// gear lists MUST be terminated with this
#define GEAR_END  { NOWHERE, NOTHING }

// starting player equipment and skills -- just don't change the order or insert new ones anywhere but the end (these are stored in the playerfile)
const struct archetype_type archetype[] = {
	// dummy row so options numbered 1+ match up
	{ "\r", "\r", { "Adventurer", "Adventurer", "Adventurer" }, 0, 0, 0, 0, { 0, 0, 0, 0, 0, 0 }, { GEAR_END } },
	
	/*
	* name, description, { Neuter rank, Male rank, Female rank },
	* skill, level,  skill (or NO_SKILL), level
	* strength, dexterity,  charisma, greatness,  intelligence, wits
	*/

	{ "Noble Birth", "raised to rule (Empire, Battle, Greatness)", { "Lord", "Lord", "Lady" },
		SKILL_EMPIRE, 20,  SKILL_BATTLE, 10,
		{ 2, 1,  2, 3,  1, 2 },
		{ { WEAR_HEAD, o_BRIMMED_CAP }, { WEAR_CLOTHES, o_WAISTCOAT }, { WEAR_LEGS, o_BREECHES }, { WEAR_FEET, o_SANDALS }, { WEAR_WIELD, o_SHORT_SWORD }, { WEAR_HOLD, o_NEWBIE_TORCH }, GEAR_END }
	},
	
	{ "Skilled Apprentice", "trained by a master artisan (Trade, Dexterity)", { "Tradesman", "Tradesman", "Tradesman" },
		SKILL_TRADE, 30,  NO_SKILL, 0,
		{ 2, 3,  2, 1,  2, 1 },
		{ { WEAR_HEAD, o_COIF }, { WEAR_CLOTHES, o_SHIRT }, { WEAR_LEGS, o_BREECHES }, { WEAR_FEET, o_SANDALS }, { WEAR_WIELD, o_SHORT_SWORD }, { WEAR_HOLD, o_NEWBIE_TORCH }, GEAR_END }
	},
	
	{ "Promising Squire", "built for battle (Battle, Survival, Strength)", { "Sir", "Sir", "Lady" },
		SKILL_BATTLE, 20,  SKILL_SURVIVAL, 10,
		{ 3, 2,  1, 2,  1, 2 },
		{ { WEAR_HEAD, o_COIF }, { WEAR_CLOTHES, o_SHIRT }, { WEAR_LEGS, o_BREECHES }, { WEAR_FEET, o_SANDALS }, { WEAR_WIELD, o_SHORT_SWORD }, { WEAR_HOLD, o_NEWBIE_TORCH }, GEAR_END }
	},
	
	{ "Rugged Woodsman", "cleaved from stone (Survival, Trade, Dexterity)", { "Adventurer", "Adventurer", "Adventurer" },
		SKILL_SURVIVAL, 20,  SKILL_TRADE, 10,
		{ 2, 3,  1, 1,  2, 2 },
		{ { WEAR_HEAD, o_LEATHER_HOOD }, { WEAR_CLOTHES, o_LEATHER_JACKET }, { WEAR_LEGS, o_PANTS }, { WEAR_FEET, o_MOCCASINS }, { WEAR_WIELD, o_WOODSMANS_AXE }, { WEAR_HOLD, o_NEWBIE_TORCH }, GEAR_END }
	},
	
	{ "Tribal Leader", "rise from nothing (Empire, Survival, Wits)", { "Elder", "Elder", "Elder" },
		SKILL_EMPIRE, 15, SKILL_SURVIVAL, 15,
		{ 2, 1,  1, 2,  2, 3 },
		{ { WEAR_HEAD, o_LEATHER_HOOD }, { WEAR_CLOTHES, o_SPOTTED_ROBE }, { WEAR_LEGS, o_PANTS }, { WEAR_FEET, o_MOCCASINS }, { WEAR_WIELD, o_STONE_AXE }, { WEAR_HOLD, o_NEWBIE_TORCH }, GEAR_END }
	},
	
	{ "Street Urchin", "the shadows of society (Stealth, Battle, Charisma)", { "Adventurer", "Adventurer", "Adventurer" },
		SKILL_STEALTH, 20,  SKILL_BATTLE, 10,
		{ 2, 2,  3, 1,  1, 2 },
		{ { WEAR_HEAD, o_CLOTH_HOOD }, { WEAR_CLOTHES, o_SHIRT }, { WEAR_LEGS, o_BREECHES }, { WEAR_FEET, o_SANDALS }, { WEAR_WIELD, o_SHIV }, { WEAR_HOLD, o_NEWBIE_TORCH }, GEAR_END }
	},
		
	{ "Druidic Commune", "wield ancient magic (Natural Magic, Survival, Intelligence)", { "Elder", "Elder", "Elder" },
		SKILL_NATURAL_MAGIC, 20, SKILL_SURVIVAL, 10,
		{ 1, 2,  2, 1,  3, 2 },
		{ { WEAR_HEAD, o_LEATHER_HOOD }, { WEAR_CLOTHES, o_ROBE }, { WEAR_FEET, o_SANDALS }, { WEAR_WIELD, o_STONE_AXE }, { WEAR_HOLD, o_NEWBIE_TORCH }, GEAR_END }
	},
		
	{ "Novice Sorcerer", "master the rituals (High Sorcery, Battle, Intelligence)", { "Master", "Master", "Mistress" },
		SKILL_HIGH_SORCERY, 20, SKILL_BATTLE, 10,
		{ 1, 2,  1, 2,  3, 2 },
		{ { WEAR_HEAD, o_CLOTH_HOOD }, { WEAR_CLOTHES, o_ROBE }, { WEAR_FEET, o_SANDALS }, { WEAR_WIELD, o_STAFF }, { WEAR_HOLD, o_NEWBIE_TORCH }, GEAR_END }
	},

	{ "Lone Vampire", "empowered by blood (Vampire, Battle, Wits)", { "Hunter", "Hunter", "Hunter" },
		SKILL_VAMPIRE, 20,  SKILL_BATTLE, 10,
		{ 2, 1,  2, 1,  2, 3 },
		{ { WEAR_CLOTHES, o_SHIRT }, { WEAR_LEGS, o_BREECHES }, { WEAR_FEET, o_SANDALS }, { WEAR_WIELD, o_SHORT_SWORD }, { WEAR_HOLD, o_NEWBIE_TORCH }, GEAR_END }
	},

	// end
	{ "\n", "\n", { "\n", "\n", "\n" }, 0, 0, 0, 0, { 0, 0, 0, 0, 0, 0 }, { GEAR_END } }
};


// BONUS_x
const char *bonus_bits[] = {
	"STRENGTH",
	"DEXTERITY",
	"CHARISMA",
	"GREATNESS",
	"INTELLIGENCE",
	"WITS",
	"HEALTH",
	"MOVES",
	"MANA",
	"HEALTH-REGEN",
	"MOVE-REGEN",
	"MANA-REGEN",
	"FAST-CHORES",
	"EXTRA-DAILY-SKILLS",
	"INVENTORY",
	"FASTER",
	"\n"
};


// BONUS_x
const char *bonus_bit_descriptions[] = {
	"+1 Strength",
	"+1 Dexterity",
	"+1 Charisma",
	"+1 Greatness",
	"+1 Intelligence",
	"+1 Wits",
	"Extra health",
	"Extra moves",
	"Extra mana",
	"Faster health regeneration",
	"Faster move regeneration",
	"Faster mana regeneration",
	"Faster chores (e.g. chopping)",
	"Extra daily bonus experience",
	"Larger inventory",
	"Faster walking",
	"\n"
};


// CUSTOM_COLOR_x
const char *custom_color_types[] = {
	"emote",
	"esay",
	"gsay",
	"oocsay",
	"say",
	"slash-channels",
	"tell",
	"status",
	"\n"
};


// GRANT_x
const char *grant_bits[] = {
	"advance",
	"ban",
	"clearabilities",
	"dc",
	"echo",
	"editnotes",	// 5
	"empires",
	"force",
	"freeze",
	"gecho",
	"instance",	// 10
	"load",
	"mute",
	"olc",
	"olc-controls",
	"page",	// 15
	"purge",
	"reboot",
	"reload",
	"restore",
	"send",	// 20
	"set",
	"shutdown",
	"snoop",
	"switch",
	"tedit",	// 25
	"transfer",
	"unbind",
	"users",
	"wizlock",
	"rescale",	// 30
	"authorize",
	"forgive",
	"hostile",
	"slay",
	"island",	// 35
	"\n"
};


/* PLR_x */
const char *player_bits[] = {
	"FROZEN",
	"WRITING",
	"MAILING",
	"DONTSET",
	"SITEOK",
	"MUTED",
	"NOTITLE",
	"DELETED",
	"LOADRM",
	"!WIZL",
	"!DEL",
	"INVST",
	"IPMASK",
	"DISGUISED",
	"VAMPIRE",
	"MULTI",
	"NEEDS-NEWBIE-SETUP",
	"!RESTICT",
	"KEEP-LOGIN",
	"EXTRACTED",
	"ADV-SUMMON"
	"\n"
};


/* PRF_x */
const char *preference_bits[] = {
	"AFK",
	"COMPACT",
	"DEAF",
	"!TELL",
	"POLIT",
	"RP",
	"MORTLOG",
	"!REP",
	"LIGHT",
	"INCOGNITO",
	"!WIZ",
	"!MCOL",
	"!HASSLE",
	"!IDLE-OUT",
	"ROOMFLAGS",
	"!CHANNEL-JOINS",
	"AUTOKILL",
	"SCRL",
	"BRIEF",
	"BOTHER",
	"AUTORECALL",
	"!GOD",
	"PVP",
	"INFORMATIVE",
	"!SPAM",
	"SCREENREADER",
	"STEALTHABLE",
	"WIZHIDE",
	"\n"
};


// PRF_x: for do_toggle, this controls the "toggle" command and these are displayed in rows of 3	
const struct toggle_data_type toggle_data[] = {
	// name, type, prf, level
	{ "pvp", TOG_ONOFF, PRF_ALLOW_PVP, 0, NULL },
	{ "mortlog", TOG_ONOFF, PRF_MORTLOG, 0, NULL },
	{ "autokill", TOG_ONOFF, PRF_AUTOKILL, 0, NULL },
	
	// these are shown in rows of 3
	{ "tell", TOG_OFFON, PRF_NOTELL, 0, NULL },
	{ "scrolling", TOG_ONOFF, PRF_SCROLLING, 0, NULL },
	{ "bother", TOG_ONOFF, PRF_BOTHERABLE, 0, NULL },
	
	{ "shout", TOG_OFFON, PRF_DEAF, 0, NULL },
	{ "brief", TOG_ONOFF, PRF_BRIEF, 0, NULL },
	{ "political", TOG_ONOFF, PRF_POLITICAL, 0, tog_political },
	
	{ "autorecall", TOG_ONOFF, PRF_AUTORECALL, 0, NULL },
	{ "compact", TOG_ONOFF, PRF_COMPACT, 0, NULL },
	{ "informative", TOG_ONOFF, PRF_INFORMATIVE, 0, tog_informative },
	
	{ "map-color", TOG_OFFON, PRF_NOMAPCOL, 0, tog_mapcolor },
	{ "no-repeat", TOG_ONOFF, PRF_NOREPEAT, 0, NULL },
	{ "screen-reader", TOG_ONOFF, PRF_SCREEN_READER, 0, NULL },
	
	{ "action-spam", TOG_OFFON, PRF_NOSPAM, 0, NULL },
	{ "rp", TOG_ONOFF, PRF_RP, 0, NULL },
	{ "afk", TOG_ONOFF, PRF_AFK, 0, afk_notify },
	
	{ "channel-joins", TOG_OFFON, PRF_NO_CHANNEL_JOINS, 0, NULL },
	{ "stealthable", TOG_ONOFF, PRF_STEALTHABLE, LVL_APPROVED, NULL },
	
	// imm section
	{ "wiznet", TOG_OFFON, PRF_NOWIZ, LVL_START_IMM, NULL },
	{ "holylight", TOG_ONOFF, PRF_HOLYLIGHT, LVL_START_IMM, NULL },
	{ "roomflags", TOG_ONOFF, PRF_ROOMFLAGS, LVL_START_IMM, NULL },
	
	{ "hassle", TOG_OFFON, PRF_NOHASSLE, LVL_START_IMM, NULL },
	{ "idle-out", TOG_OFFON, PRF_NO_IDLE_OUT, LVL_START_IMM, NULL },
	{ "incognito", TOG_ONOFF, PRF_INCOGNITO, LVL_START_IMM, NULL },
	
	{ "wizhide", TOG_ONOFF, PRF_WIZHIDE, LVL_START_IMM, NULL },
	
	// this goes last
	{ "\n", 0, NOBITS, 0, NULL }
};


/* CON_x */
const char *connected_types[] = {
	"Playing",	// 0
	"Disconnecting",
	"Get name",
	"Confirm name",
	"Get password",
	"Get new PW",	// 5
	"Confirm new PW",
	"Select sex",
		"UNUSED 1",
	"Reading MOTD",
	"Disconnecting",	// 10
	"Referral?",
	"Screen reader?",
	"Last name?",
	"Get last name",
	"Cnf last name",	// 15
	"Bookedit",
	"Have alt?",
	"Alt name",
	"Alt password",
	"Finish Creation",	// 20
	"Archetype",
	"Goodbye",
	"Choose bonus",
	"Add bonus",
	"Promo code?",	// 25
	"Confirm promo",
	"\n"
};


// SYS_x syslog types
const char *syslog_types[] = {
	"config",
	"death",
	"error",
	"gc",
	"info",
	"level",
	"login",
	"olc",
	"script",
	"system",
	"validation",
	"\n"
};


 //////////////////////////////////////////////////////////////////////////////
//// DIRECTION AND ROOM CONSTANTS ////////////////////////////////////////////

// cardinal directions for both display and detecting arguments -- NUM_OF_DIRS
const char *dirs[] = {
	"north",
	"east",
	"south",
	"west",
	"northwest",
	"northeast",
	"southwest",
	"southeast",
	"up",
	"down",
	"fore",
	"starboard",
	"port",
	"aft",
	"random",
	"\n"
};


// alternate direction names, to allow certain abbrevs in argument parsing -- NUM_OF_DIRS
const char *alt_dirs[] = {
	"n",
	"e",
	"s",
	"w",
	"nw",
	"ne",
	"sw",
	"se",
	"u",
	"d",
	"fore",
	"starboard",
	"port",
	"aft",
	"random",
	"\n"
};


// the direction I'm coming from, for walking messages -- NUM_OF_DIRS
const char *from_dir[] = {
	"the south",
	"the west",
	"the north",
	"the east",
	"the southeast",
	"the southwest",
	"the northeast",
	"the northwest",
	"below",
	"above",
	"the aft",
	"port",
	"starboard",
	"the front",
	"random",
	"\n"
};


// these are the arguments to real_shift() to shift one tile in a direction, e.g. real_shift(room, shift_dir[dir][0], shift_dir[dir][1]) -- NUM_OF_DIRS
const int shift_dir[][2] = {
	{ 0, 1 },	// north
	{ 1, 0 },	// east
	{ 0, -1},	// south
	{-1, 0 },	// west
	{-1, 1 },	// nw
	{ 1, 1 },	// ne
	{-1, -1},	// sw
	{ 1, -1},	// se
	{ 0, 0 },
	{ 0, 0 },
	{ 0, 0 },
	{ 0, 0 },
	{ 0, 0 },
	{ 0, 0 },
	{ 0, 0 }
};


// whether or not a direction can be used by designate
const bool can_designate_dir[NUM_OF_DIRS] = {
	TRUE,	// n
	TRUE,
	TRUE,
	TRUE,
	TRUE,	// nw
	TRUE,
	TRUE,
	TRUE,
	TRUE,	// up
	TRUE,	// down
	FALSE,	// fore
	FALSE,
	FALSE,
	FALSE,
	FALSE	// random
};


// whether or not you can flee in a given direction
const bool can_flee_dir[NUM_OF_DIRS] = {
	TRUE,	// n
	TRUE,
	TRUE,
	TRUE,
	TRUE,	// nw
	TRUE,
	TRUE,
	TRUE,
	FALSE,	// up
	FALSE,	// down
	TRUE,	// fore
	TRUE,
	TRUE,
	TRUE,
	FALSE	// random
};


/* EX_x */
const char *exit_bits[] = {
	"DOOR",
	"CLOSED",
	"\n"
};


// indicates the opposite of each direction
const int rev_dir[NUM_OF_DIRS] = {
	SOUTH,
	WEST,
	NORTH,
	EAST,
	SOUTHEAST,
	SOUTHWEST,
	NORTHEAST,
	NORTHWEST,
	DOWN,
	UP,
	AFT,
	PORT,
	STARBOARD,
	FORE,
	DIR_RANDOM
};


// for ABIL_NAVIGATION: confused_dir[which dir is north][reverse][which dir to translate]
// reverse=0 is for moving
// reverse=1 is for which way directions are displayed (this was very confusing to figure out)
const int confused_dirs[NUM_SIMPLE_DIRS][2][NUM_OF_DIRS] = {
	// NORTH (normal)
	{{ NORTH, EAST, SOUTH, WEST, NORTHWEST, NORTHEAST, SOUTHWEST, SOUTHEAST, UP, DOWN, FORE, STARBOARD, PORT, AFT, DIR_RANDOM },
	{ NORTH, EAST, SOUTH, WEST, NORTHWEST, NORTHEAST, SOUTHWEST, SOUTHEAST, UP, DOWN, FORE, STARBOARD, PORT, AFT, DIR_RANDOM }},
	
	// EAST (rotate left)
	{{ EAST, SOUTH, WEST, NORTH, NORTHEAST, SOUTHEAST, NORTHWEST, SOUTHWEST, UP, DOWN, FORE, STARBOARD, PORT, AFT, DIR_RANDOM },
	{ WEST, NORTH, EAST, SOUTH, SOUTHWEST, NORTHWEST, SOUTHEAST, NORTHEAST, UP, DOWN, FORE, STARBOARD, PORT, AFT, DIR_RANDOM }},
	
	// SOUTH (rotate 180)
	{{ SOUTH, WEST, NORTH, EAST, SOUTHEAST, SOUTHWEST, NORTHEAST, NORTHWEST, UP, DOWN, FORE, STARBOARD, PORT, AFT, DIR_RANDOM },
	{ SOUTH, WEST, NORTH, EAST, SOUTHEAST, SOUTHWEST, NORTHEAST, NORTHWEST, UP, DOWN, FORE, STARBOARD, PORT, AFT, DIR_RANDOM }},
	
	// WEST (rotate right)
	{{ WEST, NORTH, EAST, SOUTH, SOUTHWEST, NORTHWEST, SOUTHEAST, NORTHEAST, UP, DOWN, FORE, STARBOARD, PORT, AFT, DIR_RANDOM },
	{ EAST, SOUTH, WEST, NORTH, NORTHEAST, SOUTHEAST, NORTHWEST, SOUTHWEST, UP, DOWN, FORE, STARBOARD, PORT, AFT, DIR_RANDOM }}
};


// for ABIL_NAVIGATION: how_to_show_map[dir which is north for char][x=0,y=1]
// for each direction, whether the x/y coord goes from positive to negative (1) or negative to positive (-1)
int how_to_show_map[NUM_SIMPLE_DIRS][2] = {
	{ -1, 1 },	// north
	{ -1, -1 },	// east
	{ 1, -1 },	// south
	{ 1, 1 }	// west
};


// for ABIL_NAVIGATION: show_map_y_first[dir which is north for char]
// 1 = show y coordinate vertically, 0 = show x coord vertically
int show_map_y_first[NUM_SIMPLE_DIRS] = {
	1,	// N
	0,	// E
	1,	// S
	0	// W
};


 //////////////////////////////////////////////////////////////////////////////
//// CHARACTER CONSTANTS /////////////////////////////////////////////////////

// AFF_x (1/3) - flags
const char *affected_bits[] = {
	"BLIND",
	"MAJESTY",
	"INFRA",
	"SNEAK",
	"HIDE",
	"*CHARM",
	"INVIS",
	"!BATTLE",
	"SENSE-HIDE",
	"!PHYSICAL",
	"!TARGET",
	"!SEE",
	"FLY",
	"!ATTACK",
	"!HIGH-SORCERY",
	"DISARM",
	"HASTE",
	"ENTANGLED",
	"SLOW",
	"STUNNED",
	"STONED",
	"!BLOOD",
	"CLAWS",
	"DEATHSHROUD",
	"EARTHMELD",
	"MUMMIFY",
	"SOULMASK",
	"!NATURAL-MAGIC",
	"!STEALTH",
	"!VAMPIRE",
	"!STUN",
	"*ORDERED",
	"\n"
};

// AFF_x (2/3) - strings shown when you consider someone (empty for no-show)
const char *affected_bits_consider[] = {
	"",	// blind
	"$E has a majestic aura!",	// majesty
	"",	// infra
	"",	// sneak
	"",	// hide
	"",	// charm
	"",	// invis
	"$E is immune to Battle debuffs.",	// !battle
	"",	// sense hide
	"$E is immune to physical damage.",	// !physical
	"",	// no-target-in-room
	"",	// no-see-in-room
	"",	// fly
	"$E cannot be attacked.",	// !attack
	"$E is immune to High Sorcery debuffs.",	// !highsorc
	"",	// disarm
	"",	// haste
	"",	// entangled
	"",	// slow
	"",	// stunned
	"",	// stoned
	"",	// can't spend blood
	"",	// claws
	"",	// deathshroud
	"",	// earthmeld
	"",	// mummify
	"$E is soulmasked.",	// soulmask
	"$E is immune to Natural Magic debuffs.",	// !natmag
	"$E is immune to Stealth debuffs.",	// !stealth
	"$E is immune to Vampire debuffs.",	// !vampire
	"$E is immune to stuns.",	// !stun
	"",	// ordred
	"\n"
};

// AFF_x (3/3) - determines if an aff flag is "bad" for the bearer
const bool aff_is_bad[] = {
	TRUE,	// blind
	FALSE,
	FALSE,
	FALSE,
	FALSE,
	FALSE,
	FALSE,
	FALSE,
	FALSE,
	FALSE,
	FALSE,
	FALSE,
	FALSE,
	FALSE,
	FALSE,	// unused
	TRUE,	// disarm
	FALSE,
	TRUE,	// entangled
	TRUE,	// slow
	TRUE,	// stunned
	TRUE,	// stoned
	TRUE,	// !blood
	FALSE,
	FALSE,
	FALSE,
	FALSE,
	FALSE,
	FALSE,
	FALSE
};


// for prompt and diagnosis -- 0-10 relates to 0-100% health
const char *health_levels[] = {
	"nearly dead",	// 0
	"crippled",
	"mauled",
	"wounded",
	"wounded",
	"injured",	// 5
	"injured",
	"hurt",
	"hurt",
	"bruised",
	"healthy",	// 10
	"\n"
};


// for prompt -- 0-10 relates to 0-100% move
const char *move_levels[] = {
	"exhausted",	// 0
	"exhausted",
	"tired",
	"slowing",
	"tiring",
	"tiring",		// 5
	"going strong",
	"feeling good",
	"rested",
	"rested",
	"fully rested",	// 10
	"\n"
};


// for prompt -- 0-10 relates to 0-100% mana
const char *mana_levels[] = {
	"no mana",	// 0
	"very low mana",
	"low mana",
	"low mana",
	"half mana",
	"half mana",	// 5
	"half mana",
	"lots of mana",
	"high mana",
	"high mana",
	"full mana",	// 10
	"\n"
};


// for prompt -- 0-10 relates to 0-100% blood
const char *blood_levels[] = {
	"no blood",	// 0
	"extremely blood-starved",
	"blood-starved",
	"low blood",
	"half blood",
	"half blood",	// 5
	"half blood",
	"lots of blood",
	"high blood",
	"high blood",
	"satiated",	// 10
	"\n"
};


/* SEX_x */
const char *genders[] = {
	"neutral",
	"male",
	"female",
	"\n"
};


/* POS_x */
const char *position_types[] = {
	"Dead",
	"Mortally wounded",
	"Incapacitated",
	"Stunned",
	"Sleeping",
	"Resting",
	"Sitting",
	"Fighting",
	"Standing",
	"\n"
};


// POS_x
const int regen_by_pos[] = {
	0,
	0,
	0,
	1,
	4,
	3,
	2,
	1,
	1	// standing
};


/* INJ_x */
const char *injury_bits[] = {
	"TIED",
	"STAKED",
	"\n"
};


/* APPLY_x (1/3) */
const char *apply_types[] = {
	"NONE",
	"STRENGTH",
	"DEXTERITY",
	"HEALTH-REGEN",
	"CHARISMA",
	"GREATNESS",
	"MOVE-REGEN",
	"MANA-REGEN",
	"INTELLIGENCE",
	"WITS",
	"AGE",
	"MAX-MOVE",
	"RESIST-PHYSICAL",
	"BLOCK",
	"HEAL-OVER-TIME",
	"MAX-HEALTH",
	"MAX-MANA",
	"TO-HIT",
	"DODGE",
	"INVENTORY",
	"MAX-BLOOD",
	"BONUS-PHYSICAL",
	"BONUS-MAGICAL",
	"BONUS-HEALING",
	"RESIST-MAGICAL",
	"CRAFTING",
	"\n"
};


// APPLY_x (2/3) -- for rate_item (amount multiplied by the apply modifier to make each of these equal to 1)
const double apply_values[] = {
	0.01,	// "NONE",
	1,	// "STRENGTH",
	1,	// "DEXTERITY",
	0.5,	// "HEALTH-REGEN",
	1,	// "CHARISMA",
	1,	// "GREATNESS",
	0.5,	// "MOVE-REGEN",
	0.5,	// "MANA-REGEN",
	1,	// "INTELLIGENCE",
	1,	// "WITS",
	0.1,	// "AGE",
	0.025,	// "MAX-MOVE",
	0.5,	// RESIST-PHYSICAL
	0.25,	// "BLOCK",
	3,	// "HEAL-OVER-TIME",
	0.020,	// "HEALTH",
	0.025,	// "MAX-MANA",
	0.15,	// "TO-HIT",
	0.10,	// "DODGE",
	0.15,	// "INVENTORY",
	0.02,	// "BLOOD",
	1,	// BONUS-PHYSICAL
	1,	// BONUS-MAGICAL
	1,	// BONUS-HEALING
	0.5,	// RESIST-MAGICAL
	0.01	// CRAFTING
};


// APPLY_x (3/3) applies that are directly tied to attributes
const int apply_attribute[] = {
	NOTHING,
	STRENGTH,
	DEXTERITY,
	NOTHING,	// health-regen
	CHARISMA,
	GREATNESS,
	NOTHING,	// move-regen
	NOTHING,	// mana-regen
	INTELLIGENCE,
	WITS,
	NOTHING,	// age
	NOTHING,	// max-move
	NOTHING,	// resist-physical
	NOTHING,	// block
	NOTHING,	// heal-over-time
	NOTHING,	// max-health
	NOTHING,	// max-mana
	NOTHING,	// to-hit
	NOTHING,	// dodge
	NOTHING,	// inv
	NOTHING,	// blood
	NOTHING,	// bonus-phys
	NOTHING,	// bonus-mag
	NOTHING,	// bonus-heal
	NOTHING,	// resist-magical
	NOTHING	// crafting
};


// STRENGTH, etc
struct attribute_data_type attributes[NUM_ATTRIBUTES] = {
	{ "Strength", "Strength improves your melee damage and lets you chop trees faster" },
	{ "Dexterity", "Dexterity helps you hit opponents and dodge hits" },
	{ "Charisma", "Charisma improves your success with Stealth abilities" },
	{ "Greatness", "Greatness determines how much territory your empire can claim" },
	{ "Intelligence", "Intelligence improves your magical damage and healing" },
	{ "Wits", "Wits improves your speed and effectiveness in combat" }
};


// NUM_POOLS, HEALTH, MOVES, MANA, BLOOD (any other search terms?)
const char *pool_types[] = {
	"health",
	"move",
	"mana",
	"blood",
	"\n"
};


// NUM_POOLS, HEALTH, MOVES, MANA, BLOOD (any other search terms?)
const char *pool_abbrevs[] = {
	"H",
	"V",	// move
	"M",
	"B",
	"\n"
};


 //////////////////////////////////////////////////////////////////////////////
//// CRAFT RECIPE CONSTANTS //////////////////////////////////////////////////

// CRAFT_x
const char *craft_flags[] = {
	"POTTERY",
	"APIARIES-TECH",
	"GLASS-TECH",
	"GLASSBLOWER",
	"CARPENTER",
	"ALCHEMY",
	"SHARP-TOOL",
	"FIRE",
	"SOUP",
	"IN-DEVELOPMENT",
	"UPGRADE",
	"DISMANTLE-ONLY",
	"IN-CITY-ONLY",
	"\n"
};


// CRAFT_TYPE_x
const char *craft_types[] = {
	"UNDEFINED",
	"FORGE",
	"CRAFT",
	"COOK",
	"SEW",
	"MILL",
	"BREW",
	"MIX",
	"BUILD",
	"WEAVE",
	"WORKFORCE",
	"\n"
};


 //////////////////////////////////////////////////////////////////////////////
//// EMPIRE CONSTANTS ////////////////////////////////////////////////////////

// name, icon, radius, max population
struct city_metadata_type city_type[] = {
	{ "outpost", "&0-&?C1&0-", 5, 10 },
	{ "village", "&0-&?C2&0-", 10, 30 },
	{ "city", "&0-&?C3&0-", 15, 50 },
	{ "capital", "&0-&?C4&0-", 25, 150 },

	// this must go last	
	{ "\n", "\n", 0, 0 }
};


// ELOG_x
const char *empire_log_types[] = {
	"None",
	"Admin",
	"Diplomacy",
	"Hostility",
	"Members",
	"Territory",
	"Trade",
	"Logins",
	"Shipping",
	"\n"
};


// ELOG_x: Whether or not logs are shown to players online
const bool show_empire_log_type[] = {
	TRUE,	// none
	TRUE,	// admin
	TRUE,	// diplo
	TRUE,	// hostility
	TRUE,	// members
	TRUE,	// territory
	FALSE,	// trade
	TRUE,	// logins
	FALSE	// shipments
};


// EUS_x -- lowercase
const char *unique_storage_flags[] = {
	"vault",
	"\n"
};


// TECH_x
const char *techs[] = {
	"Glassblowing",
	"Lights",
	"Locks",
	"Apiaries",
	"Seaport",
	"Workforce",
	"Prominence",
	"Commerce",
	"Portals",
	"Master Portals",
	"Skilled Labor",
	"Trade Routes",
	"Exarch Crafts",
	"\n"
};


// ETRAIT_x
const char *empire_trait_types[] = {
	"Distrustful",
	"\n"
};


// PRIV_x
const char *priv[] = {
	"claim",
	"build",
	"harvest",
	"promote",
	"chop",
	"cede",
	"enroll",
	"withdraw",
	"diplomacy",
	"customize",
	"workforce",
	"stealth",
	"cities",
	"trade",
	"logs",
	"shipping",
	"homes",
	"storage",
	"warehouse",
	"\n"
};


// SCORE_x -- score types
const char *score_type[] = {
	"Wealth",
	"Territory",
	"Members",
	"Techs",
	"Inventory",
	"Greatness",
	"Diplomacy",
	"Fame",
	"Military",
	"Playtime",
	"\n"
};


// TRADE_x -- all lowercase
const char *trade_type[] = {
	"export",
	"import",
	"\n"
};

// TRADE_x
const char *trade_mostleast[] = {
	"",	// export
	"at most ",	// import
	"\n"
};

// TRADE_x
const char *trade_overunder[] = {
	"over",	// export
	"under",	// import
	"\n"
};


 //////////////////////////////////////////////////////////////////////////////
//// MOB CONSTANTS ///////////////////////////////////////////////////////////

/* MOB_x */
const char *action_bits[] = {
	"BRING-A-FRIEND",	// 0
	"SENTINEL",
	"AGGR",
	"ISNPC",
	"MOUNTABLE",
	"MILKABLE",	// 5
	"SCAVENGER",
	"UNDEAD",
	"TIED",
	"ANIMAL",
	"MOUNTAIN-WALK",	// 10
	"AQUATIC",
	"*PLURAL",
	"NO-ATTACK",
	"SPAWNED",
	"CHAMPION",	// 15
	"EMPIRE",
	"FAMILIAR",
	"*PICKPOCKETED",
	"CITYGUARD",
	"PURSUE",	// 20
	"HUMAN",
	"VAMPIRE",
	"CASTER",
	"TANK",
	"DPS",	// 25
	"HARD",
	"GROUP",
	"*EXTRACTED",
	"!LOOT",
	"!TELEPORT",
	"\n"
};


// MOB_MOVE_x
const char *mob_move_types[] = {
	"walks",
	"climbs",
	"flies",
	"paddles",
	"rides",
	"slithers",
	"swims",
	"scurries",
	"skitters",
	"creeps",
	"oozes",
	"runs",
	"gallops",
	"shambles",
	"trots",
	"hops",
	"waddles",
	"crawls",
	"flutters",
	"\n"
};


// NAMES_x
const char *name_sets[] = {
	"Citizens",
	"Country-folk",
	"Roman",
	"Northern",
	"\n"
};


 //////////////////////////////////////////////////////////////////////////////
//// ITEM CONSTANTS //////////////////////////////////////////////////////////

// WEAR_x -- keyword to target a pos (for search_block); also see wear_data (below)
const char *wear_keywords[] = {
	"head",
	"ears",
	"neck",
	"\r!",	// neck 2
	"clothes",
	"armor",
	"about",
	"arms",
	"wrists",
	"hands",
	"finger",
	"\r!",	// finger 2
	"waist",
	"legs",
	"feet",
	"pack",
	"saddle",
	"\r!",	// sheathed 1
	"\r!",	// sheathed 2
	"\r!",	// wield
	"\r!",	// ranged
	"\r!",	// hold
	"\n"
};


// WEAR_x -- data for each wear slot
const struct wear_data_type wear_data[NUM_WEARS] = {
	// eq tag,				 wear bit,		count-stats, gear-level-mod, cascade-pos, already-wearing, wear-message-to-room, wear-message-to-char
	{ "    <worn on head> ", ITEM_WEAR_HEAD, TRUE, 1.0, NO_WEAR, "You're already wearing $p on your head.", "$n wears $p on $s head.", "You wear $p on your head." },
	{ "    <worn on ears> ", ITEM_WEAR_EARS, TRUE, 1.0, NO_WEAR, "You're already wearing $p on your ears.", "$n pins $p onto $s ears.", "You pin $p onto your ears." },
	{ "<worn around neck> ", ITEM_WEAR_NECK, TRUE, 1.0, WEAR_NECK_2, "YOU SHOULD NEVER SEE THIS MESSAGE. PLEASE REPORT.", "$n wears $p around $s neck.", "You wear $p around your neck." },
	{ "<worn around neck> ", ITEM_WEAR_NECK, TRUE, 1.0, NO_WEAR, "You're already wearing enough around your neck.", "$n wears $p around $s neck.", "You wear $p around your neck." },
	{ " <worn as clothes> ", ITEM_WEAR_CLOTHES, TRUE, 0, NO_WEAR, "You're already wearing $p as clothes.", "$n wears $p as clothing.", "You wear $p as clothing." },
	{ "   <worn as armor> ", ITEM_WEAR_ARMOR, TRUE, 2.0, NO_WEAR, "You're already wearing $p as armor.", "$n wears $p as armor.", "You wear $p as armor." },
	{ " <worn about body> ", ITEM_WEAR_ABOUT, TRUE, 1.0, NO_WEAR, "You're already wearing $p about your body.", "$n wears $p about $s body.", "You wear $p around your body." },
	{ "    <worn on arms> ", ITEM_WEAR_ARMS, TRUE, 1.0, NO_WEAR, "You're already wearing $p on your arms.", "$n wears $p on $s arms.", "You wear $p on your arms." },
	{ "  <worn on wrists> ", ITEM_WEAR_WRISTS, TRUE, 1.0, NO_WEAR, "You're already wearing $p on your wrists.", "$n wears $p on $s wrists.", "You wear $p on your wrists." },
	{ "   <worn on hands> ", ITEM_WEAR_HANDS, TRUE, 1.0, NO_WEAR, "You're already wearing $p on your hands.", "$n puts $p on $s hands.", "You put $p on your hands." },
	{ "  <worn on finger> ", ITEM_WEAR_FINGER, TRUE, 1.0, WEAR_FINGER_L, "YOU SHOULD NEVER SEE THIS MESSAGE. PLEASE REPORT.", "$n slides $p on to $s right ring finger.", "You slide $p on to your right ring finger." },
	{ "  <worn on finger> ", ITEM_WEAR_FINGER, TRUE, 1.0, NO_WEAR, "You're already wearing something on both of your ring fingers.", "$n slides $p on to $s left ring finger.", "You slide $p on to your left ring finger." },
	{ "<worn about waist> ", ITEM_WEAR_WAIST, TRUE, 1.0, NO_WEAR, "You already have $p around your waist.", "$n wears $p around $s waist.", "You wear $p around your waist." },
	{ "    <worn on legs> ", ITEM_WEAR_LEGS, TRUE, 1.0, NO_WEAR, "You're already wearing $p on your legs.", "$n puts $p on $s legs.", "You put $p on your legs." },
	{ "    <worn on feet> ", ITEM_WEAR_FEET, TRUE, 1.0, NO_WEAR, "You're already wearing $p on your feet.", "$n wears $p on $s feet.", "You wear $p on your feet." },
	{ " <carried as pack> ", ITEM_WEAR_PACK, TRUE, 0.5, NO_WEAR, "You're already using $p.", "$n starts using $p.", "You start using $p." },
	{ "  <used as saddle> ", ITEM_WEAR_SADDLE, TRUE, 0, NO_WEAR, "You're already using $p.", "$n start using $p.", "You start using $p." },
	{ "          (sheath) ", ITEM_WEAR_WIELD, FALSE, 0, WEAR_SHEATH_2, "You've already got something sheathed.", "$n sheathes $p.", "You sheathe $p." },
	{ "          (sheath) ", ITEM_WEAR_WIELD, FALSE, 0, NO_WEAR, "You've already got something sheathed.", "$n sheathes $p.", "You sheathe $p." },
	{ "         <wielded> ", ITEM_WEAR_WIELD, 	TRUE, 2.0, NO_WEAR, "You're already wielding $p.", "$n wields $p.", "You wield $p." },
	{ "          <ranged> ", ITEM_WEAR_RANGED, TRUE, 0, NO_WEAR, "You're already using $p.", "$n uses $p.", "You use $p." },
	{ "            <held> ", ITEM_WEAR_HOLD, TRUE, 1.0, NO_WEAR, "You're already holding $p.", "$n grabs $p.", "You grab $p." }
};


/* ITEM_x (ordinal object types) */
const char *item_types[] = {
	"UNDEFINED",
	"WEAPON",
	"WORN",
	"OTHER",
	"CONTAINER",
	"DRINKCON",
	"FOOD",
	"BOAT",
	"PORTAL",
	"*BOARD",
	"*CORPSE",
	"COINS",
	"*",
	"*",
	"*MAIL",
	"WEALTH",
	"CART",
	"*SHIP",
	"*HELM",
	"*WINDOW",
	"MISSILE_WEAPON",
	"ARROW",
	"INSTRUMENT",
	"SHIELD",
	"PACK",
	"POTION",
	"POISON",
	"ARMOR",
	"BOOK",
	"\n"
};


// ITEM_WEAR_ (wear bitvector) -- also see wear_significance
const char *wear_bits[] = {
	"TAKE",
	"FINGER",
	"NECK",
	"CLOTHES",
	"HEAD",
	"LEGS",
	"FEET",
	"HANDS",
	"ARMS",
	"WRISTS",
	"ABOUT",
	"WAIST",
	"EARS",
	"WIELD",
	"HOLD",
	"RANGED",
	"ARMOR",
	"PACK",
	"SADDLE",
	"\n"
};


// ITEM_WEAR_x - position importance, for item scaling
const int wear_significance[] = {
	WEAR_POS_MINOR,	// take
	WEAR_POS_MINOR,	// finger
	WEAR_POS_MINOR,	// neck
	WEAR_POS_MINOR,	// clothes
	WEAR_POS_MINOR,	// head
	WEAR_POS_MINOR,	// legs
	WEAR_POS_MINOR,	// feet
	WEAR_POS_MINOR,	// hands
	WEAR_POS_MINOR,	// arms
	WEAR_POS_MINOR,	// wrists
	WEAR_POS_MINOR,	// about
	WEAR_POS_MINOR,	// waist
	WEAR_POS_MINOR,	// ears
	WEAR_POS_MAJOR,	// wield
	WEAR_POS_MAJOR,	// hold
	WEAR_POS_MAJOR,	// ranged
	WEAR_POS_MAJOR,	// armor
	WEAR_POS_MINOR,	// pack
	WEAR_POS_MAJOR	// saddle
};


// ITEM_WEAR_x -- for each wear flag, the first matching eq pos
int item_wear_to_wear[] = {
	NO_WEAR,	// special
	WEAR_FINGER_R,
	WEAR_NECK_1,
	WEAR_CLOTHES,
	WEAR_HEAD,
	WEAR_LEGS,
	WEAR_FEET,
	WEAR_HANDS,
	WEAR_ARMS,
	WEAR_WRISTS,
	WEAR_ABOUT,
	WEAR_WAIST,
	WEAR_EARS,
	WEAR_WIELD,
	WEAR_HOLD,
	WEAR_RANGED,
	WEAR_ARMOR,
	WEAR_PACK,
	WEAR_SADDLE
};


// OBJ_x (extra bits), part 1
const char *extra_bits[] = {
	"CHAIR",
	"PLANTABLE",
	"LIGHT",
	"SUPERIOR",
	"LARGE",
	"*CREATED",
	"1-USE",
	"SLOW",
	"FAST",
	"ENCHANTED",
	"JUNK",
	"CREATABLE",
	"SCALABLE",
	"TWO-HANDED",
	"BOE",
	"BOP",
	"STAFF",
	"UNCOLLECTED-LOOT",
	"*KEEP",
	"TOOL-PAN",
	"TOOL-SHOVEL",
	"!AUTOSTORE",
	"HARD-DROP",
	"GROUP-DROP",
	"GENERIC-DROP",
	"\n"
};


// OBJ_x (extra bits), part 2 -- shown in inventory/equipment list as flags
const char *extra_bits_inv_flags[] = {
	"",	// chair
	"",	// plantable
	"(light)",
	"(superior)",
	"(large)",
	"",	// created
	"",	// 1-use
	"",	// slow
	"",	// fast
	"(enchanted)",
	"",	// junk
	"",	// creatable
	"",	// scalable
	"(2h)",
	"(boe)",
	"(bop)",
	"",	// staff
	"",	// uncollected
	"(keep)",
	"",	// pan
	"",	// shovel
	"",	// !autostore
	"",	// hard-drop
	"",	// group-drop
	"",	// generic-drop
	"\n"
};


// OBJ_x (extra bits), part 3 -- the amount a flag modifies scale level by (1.0 = no mod)
const double obj_flag_scaling_bonus[] = {
	1.0,	// OBJ_CHAIR
	1.0,	// OBJ_PLANTABLE
	1.0,	// OBJ_LIGHT
	1.3333,	// OBJ_SUPERIOR
	1.0,	// OBJ_LARGE
	1.0,	// OBJ_CREATED
	1.0,	// OBJ_SINGLE_USE
	1.0,	// OBJ_SLOW (weapon attack speed)
	1.0,	// OBJ_FAST (weapon attack speed)
	1.3333,	// OBJ_ENCHANTED
	0.5,	// OBJ_JUNK
	1.0,	// OBJ_CREATABLE
	1.0,	// OBJ_SCALABLE
	1.5,	// OBJ_TWO_HANDED
	1.3333,	// OBJ_BIND_ON_EQUIP
	1.5,	// OBJ_BIND_ON_PICKUP
	1.0,	// OBJ_STAFF
	1.0,	// OBJ_UNCOLLECTED_LOOT
	1.0,	// OBJ_KEEP
	1.0,	// OBJ_TOOL_PAN
	1.0,	// OBJ_TOOL_SHOVEL
	1.0,	// OBJ_NO_AUTOSTORE
	1.2,	// OBJ_HARD_DROP
	1.4,	// OBJ_GROUP_DROP
	1.0	// OBJ_GENERIC_DROP
};


// MAT_x -- name, TRUE if it floats
const struct material_data materials[NUM_MATERIALS] = {
	{ "WOOD", TRUE },
	{ "ROCK", FALSE },
	{ "IRON", FALSE },
	{ "SILVER", FALSE },
	{ "GOLD", FALSE },
	{ "FLINT", FALSE },
	{ "CLAY", FALSE },
	{ "FLESH", TRUE },
	{ "GLASS", FALSE },
	{ "WAX", TRUE },
	{ "MAGIC", TRUE },
	{ "CLOTH", TRUE },
	{ "GEM", FALSE },
	{ "COPPER", FALSE },
	{ "BONE", TRUE },
	{ "HAIR", TRUE }
};


// ARMOR_x part 1 - names
const char *armor_types[NUM_ARMOR_TYPES+1] = {
	"mage",
	"light",
	"medium",
	"heavy",
	"\n"
};


// ARMOR_x part 2 - scale values
const double armor_scale_bonus[NUM_ARMOR_TYPES] = {
	1.2,	// mage
	1.2,	// light
	1.2,	// medium
	1.2		// heavy
};


/* CONT_x */
const char *container_bits[] = {
	"CLOSEABLE",
	"CLOSED",
	"\n"
};


/* LIQ_x */
const char *drinks[] = {
	"water",
	"lager",
	"wheat beer",
	"ale",
	"cider",
	"milk",
	"blood",
	"honey",
	"bean soup",
	"coffee",
	"green tea",
	"red wine",
	"white wine",
	"\n"
};


/* LIQ_x one-word alias for each drink */
const char *drinknames[] = {
	"water",
	"lager",
	"wheatbeer",
	"ale",
	"cider",
	"milk",
	"blood",
	"honey",
	"soup",
	"coffee",
	"tea",
	"wine",
	"wine",
	"\n"
};


// LIQ_x: this table is amount per "drink", in mud hours of thirst cured, etc
// effect on DRUNK, FULL, THIRST
int drink_aff[][3] = {
	{ 0, 0, 3 },	/* water	*/
	{ 2, 1, 1 },	/* larger	*/
	{ 3, 1, 1 },	/* wheatbeer	*/
	{ 2, 1, 1 },	/* ale		*/
	{ 3, 1, 1 },	/* cider	*/
	{ 0, 2, 2 },	/* milk		*/
	{ 0, 0, -1 },	/* blood	*/
	{ 0, 0, 1 },	/* honey	*/
	{ 0, 4, 0 },	// bean soup
	{ 0, 0, 1 },	// coffee
	{ 0, 0, 1 },	// green tea
	{ 4, 0, 1 },	// red wine
	{ 3, 0, 1 }	// white wine
};


// LIQ_x: color of the various drinks
const char *color_liquid[] = {
	"clear",
	"brown",
	"golden white",
	"golden",
	"golden",
	"white",
	"red",
	"golden",
	"soupy",
	"brown",
	"green",
	"red",
	"clear",
	"\n"
};


/* level of fullness for drink containers */
const char *fullness[] = {
	"less than half ",
	"about half ",
	"more than half ",
	""
};


// STORAGE_x
const char *storage_bits[] = {
	"WITHDRAW",
	"\n"
};


// OBJ_CUSTOM_x
const char *obj_custom_types[] = {
	"build-to-char",
	"build-to-room",
	"instrument-to-char",
	"instrument-to-room",
	"eat-to-char",
	"eat-to-room",
	"\n"
};


// Weapon attack texts -- TYPE_x
struct attack_hit_type attack_hit_info[NUM_ATTACK_TYPES] = {
	// * lower numbers are better for speeds (seconds between attacks)
	// name, singular, plural, { fast spd, normal spd, slow spd }, WEAPON_x, DAM_x
	{ "RESERVED", "hit", "hits", { 1.8, 2.0, 2.2 }, WEAPON_BLUNT, DAM_PHYSICAL },
	{ "slash", "slash", "slashes", { 2.6, 2.8, 3.0 }, WEAPON_SHARP, DAM_PHYSICAL },
	{ "slice", "slice", "slices", { 3.0, 3.2, 3.4 }, WEAPON_SHARP, DAM_PHYSICAL },
	{ "jab", "jab", "jabs", { 2.6, 2.8, 3.0 }, WEAPON_SHARP, DAM_PHYSICAL },
	{ "stab", "stab", "stabs", { 2.0, 2.2, 2.4 }, WEAPON_SHARP, DAM_PHYSICAL },
	{ "pound", "pound", "pounds", { 3.4, 3.6, 3.8 }, WEAPON_BLUNT, DAM_PHYSICAL },
	{ "hammer", "hammer", "hammers", { 3.4, 3.6, 3.8 }, WEAPON_BLUNT, DAM_PHYSICAL },
	{ "whip", "whip", "whips", { 2.8, 3.0, 3.2 }, WEAPON_BLUNT, DAM_PHYSICAL },
	{ "pick", "jab", "jabs", { 3.4, 3.6, 3.8 }, WEAPON_SHARP, DAM_PHYSICAL },
	{ "bite", "bite", "bites", { 2.2, 2.4, 2.6 }, WEAPON_SHARP, DAM_PHYSICAL },
	{ "claw", "claw", "claws", { 2.4, 2.6, 2.8 }, WEAPON_SHARP, DAM_PHYSICAL },
	{ "kick", "kick", "kicks", { 2.6, 2.8, 3.0 }, WEAPON_BLUNT, DAM_PHYSICAL },
	{ "fire", "burn", "burns", { 2.6, 2.8, 3.0 }, WEAPON_BLUNT, DAM_FIRE },
	{ "vampire claws", "claw", "claws", { 2.4, 2.6, 2.8 }, WEAPON_SHARP, DAM_PHYSICAL },
	{ "crush", "crush", "crushes", { 3.6, 3.8, 4.0 }, WEAPON_BLUNT, DAM_PHYSICAL },
	{ "hit", "hit", "hits", { 2.8, 3.0, 3.2 }, WEAPON_BLUNT, DAM_PHYSICAL },
	{ "magic fire", "blast", "blasts", { 3.6, 3.8, 4.0 }, WEAPON_MAGIC, DAM_MAGICAL },
	{ "lightning staff", "zap", "zaps", { 2.2, 2.5, 2.8 }, WEAPON_MAGIC, DAM_MAGICAL },
	{ "burn staff", "burn", "burns", { 2.6, 2.9, 3.2 }, WEAPON_MAGIC, DAM_MAGICAL },
	{ "agony staff", "agonize", "agonizes", { 3.3, 3.6, 3.9 }, WEAPON_MAGIC, DAM_MAGICAL },
	{ "magic frost", "chill", "chills", { 4.1, 4.3, 4.5 }, WEAPON_MAGIC, DAM_MAGICAL },
	{ "magic shock", "shock", "shocks", { 2.6, 2.8, 3.0 }, WEAPON_MAGIC, DAM_MAGICAL },
	{ "magic light", "flash", "flashes", { 2.8, 3.0, 3.2 }, WEAPON_MAGIC, DAM_MAGICAL },
	{ "sting", "sting", "stings", { 3.6, 3.8, 4.0 }, WEAPON_SHARP, DAM_PHYSICAL },
	{ "swipe", "swipe", "swipes", { 3.6, 3.8, 4.0 }, WEAPON_BLUNT, DAM_PHYSICAL },
	{ "tail swipe", "swipe", "swipes", { 4.0, 4.2, 4.4 }, WEAPON_BLUNT, DAM_PHYSICAL },
	{ "peck", "peck", "pecks", { 2.6, 2.8, 3.0 }, WEAPON_BLUNT, DAM_PHYSICAL },
	{ "gore", "gore", "gores", { 3.9, 4.1, 4.3 }, WEAPON_BLUNT, DAM_PHYSICAL },
	{ "mana blast", "blast", "blasts", { 2.8, 3.0, 3.2 }, WEAPON_MAGIC, DAM_MAGICAL }
};


// missile speeds
const double missile_weapon_speed[] = {
	3.0,
	2.4,
	2.0,
	
	// terminate the list
	-1
};


 //////////////////////////////////////////////////////////////////////////////
//// OLC CONSTANTS ///////////////////////////////////////////////////////////

// OLC_FLAG_x
const char *olc_flag_bits[] = {
	"ALL-VNUMS",
	"MAP-EDIT",
	"CLEAR-IN-DEV",
	"!CRAFT",
	"!MOBILE",
	"!OBJECT",
	"!BUILDING",
	"SECTORS",
	"!CROP",
	"!TRIGGER",
	"!ADVENTURE",
	"!ROOMTEMPLATE",
	"!GLOBAL",
	"\n"
};


// OLC_x types
const char *olc_type_bits[NUM_OLC_TYPES+1] = {
	"craft",
	"mobile",
	"object",
	"map",
	"building",
	"trigger",
	"crop",
	"sector",
	"adventure",
	"roomtemplate",
	"global",
	"\n"
};


 //////////////////////////////////////////////////////////////////////////////
//// ROOM/WORLD CONSTANTS ////////////////////////////////////////////////////

// BLD_ON_x
const char *bld_on_flags[] = {
	"water",
	"plains",
	"mountain",
	"forest",
	"desert",
	"river",
	"jungle",
	"not player made",
	"ocean",
	"oasis",
	"crops",
	"grove",
	"swamp",
	"any forest",
	"open building",
	"\n"
};


// BLD_x
const char *bld_flags[] = {
	"ROOM",
	"ALLOW-MOUNTS",
	"TWO-ENTRANCES",
	"OPEN",
	"CLOSED",
	"INTERLINK",
	"HERD",
	"DEDICATE",
	"DRINK",
	"!NPC",
	"BARRIER",
	"TAVERN",
	"LARGE-CITY-RADIUS",
	"MINE",
	"ATTACH-ROAD",
	"BURNABLE",
	"FORGE",
	"COOKING-FIRE",
	"ALCHEMIST",
	"STABLE",
	"LIBRARY",
	"APIARY",
	"GLASSBLOWER",
	"DOCKS",
	"PIGEON-POST",
	"MILL",
	"POTTER",
	"TAILOR",
	"BATHS",
	"SAIL",
	"TOMB",
	"MINT-COINS",
	"VAULT",
	"ITEM-LIMIT",
	"LONG-AUTOSTORE",
	"WAREHOUSE",
	"TRADE",
	"HIGH-DEPLETION",
	"PORTAL",
	"BEDROOM",
	"!DELETE",
	"SUMMON-PLAYER",
	"NEED-BOAT",
	"LOOK-OUT",
	"2ND-TERRITORY",
	"\n"
};


// CLIMATE_x
const char *climate_types[] = {
	"NONE",
	"TEMPERATE",
	"ARID",
	"TROPICAL",
	"\n"
};


// CROPF_x
const char *crop_flags[] = {
	"REQUIRES-WATER",
	"ORCHARD",
	"!WILD",
	"\n"
};


// DPLTN_x
const char *depletion_type[NUM_DEPLETION_TYPES] = {
	"dig",
	"forage",
	"gather",
	"pick",
	"fish",
	"quarry",
	"pan",
	"trapping"
};


// DES_x
const char *designate_flags[] = {
	"CRYPT",
	"VAULT",
	"FORGE",
	"TUNNEL",
	"HALL",
	"SKYBRIDGE",
	"THRONE",
	"ARMORY",
	"GREAT-HALL",
	"BATHS",
	"LABORATORY",
	"TOP-OF-TOWER",
	"HOUSEHOLD",
	"HAVEN",
	"\n"
};


// EVO_x world evolutions
const char *evo_types[] = {
	"CHOPPED-DOWN",
	"CROP-GROWS",
	"ADJACENT-ONE",
	"ADJACENT-MANY",
	"RANDOM",
	"TRENCH-START",
	"TRENCH-FULL",
	"NEAR-SECTOR",
	"PLANTS-TO",
	"MAGIC-GROWTH",
	"NOT-ADJACENT",
	"NOT-NEAR-SECTOR",
	"\n"
};


// EVO_x -- what type of data the evolution.value uses
const int evo_val_types[NUM_EVOS] = {
	EVO_VAL_NUMBER,	// chopped-down
	EVO_VAL_NONE,	// crop-grows
	EVO_VAL_SECTOR,	// adjacent-one
	EVO_VAL_SECTOR,	// adjacent-many
	EVO_VAL_NONE,	// random
	EVO_VAL_NONE,	// trench-start
	EVO_VAL_NONE,	// trench-full
	EVO_VAL_SECTOR,	// near-sector
	EVO_VAL_NONE,	// plants-to
	EVO_VAL_NONE,	// magic-growth
	EVO_VAL_SECTOR,	// not-adjacent
	EVO_VAL_SECTOR,	// not-near-sector
};


// ISLE_x -- island flags
const char *island_bits[] = {
	"NEWBIE",
	"!AGGRO",
	"\n"
};


// these must match up to mapout_color_tokens -- do not insert or change the order
const char *mapout_color_names[] = {
	"Starting Location",	// 0
	"Neutral",
	"Bright White",
	"Bright Red",
	"Bright Green",
	"Bright Yellow",	// 5
	"Bright Blue",
	"Bright Magenta",
	"Bright Cyan",
	"Dark Red",
	"Pale Green",	// 10
	"Yellow-Green",
	"Sea Green",
	"Medium Green",
	"Dark Green",
	"Olive Green",	// 15
	"Ice Blue",
	"Light Blue",
	"Medium Blue",
	"Deep Blue",
	"Light Tan",	// 20
	"Pale Yellow",
	"Peach",
	"Orange",
	"Yellow Brown",
	"Brown",	// 25
	"Medium Gray",
	"Dark Gray",
	"Dark Blue",
	"Dark Azure Blue",
	"Dark Magenta",	// 30
	"Dark Cyan",
	"Lime Green",
	"Dark Lime Green",
	"Dark Orange",
	"Pink",	// 35
	"Dark Pink",
	"Tan",
	"Violet",
	"Deep Violet",	// 39
	"\n"
};


// these must match up to mapout_color_names -- do not insert or change the order
// these must also match up to the map.php generator
const char mapout_color_tokens[] = {
	'*',	// "Starting Location",	// 0
	'?',	// "Neutral",
	'0',	// "Bright White",
	'1',	// "Bright Red",
	'2',	// "Bright Green",
	'3',	// "Bright Yellow",	// 5
	'4',	// "Bright Blue",
	'5',	// "Bright Magenta",
	'6',	// "Bright Cyan",
	'a',	// "Dark Red",
	'b',	// "Pale Green",	// 10
	'c',	// "Yellow-Green",
	'd',	// "Sea Green",
	'e',	// "Medium Green",
	'f',	// "Dark Green",
	'g',	// "Olive Green",	// 15
	'h',	// "Ice Blue",
	'i',	// "Light Blue",
	'j',	// "Medium Blue",
	'k',	// "Deep Blue",
	'l',	// "Light Tan",	// 20
	'm',	// "Pale Yellow",
	'n',	// "Peach",
	'o',	// "Orange",
	'p',	// "Yellow Brown",
	'q',	// "Brown",	// 25
	'r',	// "Medium Gray",
	's',	// "Dark Gray",
	't',	// "Dark Blue",
	'u',	// "Dark Azure Blue",
	'v',	// "Dark Magenta",	// 30
	'w',	// "Dark Cyan",
	'x',	// "Lime Green",
	'y',	// "Dark Lime Green",
	'z',	// "Dark Orange",
	'A',	// "Pink",	// 35
	'B',	// "Dark Pink",
	'C',	// "Tan",
	'D',	// "Violet",
	'E',	// "Deep Violet",	// 39
};


// this maps a banner color (the 'r' in "&r") to a mapout_color_token character ('1')
const char banner_to_mapout_token[][2] = {
	{ '0', '0' },
	{ 'n', '0' },
	// non-bright colors:
	{ 'r', 'a' },
	{ 'g', 'f' },
	{ 'b', 't' },
	{ 'y', 'p' },
	{ 'm', 'v' },
	{ 'c', 'w' },
	{ 'w', 's' },
	{ 'a', 'u' },
	{ 'j', 'g' },
	{ 'l', 'y' },
	{ 'o', 'z' },
	{ 'p', 'B' },
	{ 't', 'q' },
	{ 'v', 'E' },
	// bright colors:
	{ 'R', '1' },
	{ 'G', '2' },
	{ 'B', '4' },
	{ 'Y', '3' },
	{ 'M', '5' },
	{ 'C', '6' },
	{ 'W', '0' },
	{ 'A', 'j' },
	{ 'J', 'd' },
	{ 'L', 'x' },
	{ 'O', 'o' },
	{ 'P', 'A' },
	{ 'T', 'C' },
	{ 'V', 'D' },
	
	// last
	{ '\n', '\n' }
};


// special command list just for orchards
const char *orchard_commands = "chop, dig, gather, pick";


// ROAD_x
const char *road_types[] = {
	"ROAD",
	"BRIDGE",
	"SWAMPWALK",
	"\n"
};


/* ROOM_AFF_x: */
const char *room_aff_bits[] = {
	"DARK",
	"SILENT",
	"*HAS-INSTANCE",
	"CHAMELEON",
	"*TEMPORARY",
	"!EVOLVE",
	"*UNCLAIMABLE",
	"*PUBLIC",
	"*DISMANTLING",
	"!FLY",
	"*SHIP-PRESENT",
	"*PLAYER-MADE",
	"*!WORK",
	"!DISREPAIR",
	"*!DISMANTLE",
	"\n"
};


// ROOM_EXTRA_x
const char *room_extra_types[] = {
	"mine type",
	"mine amount",
	"crop type",
	"seed time",
	"tavern type",
	"tavern brewing time",
	"tavern available time",
	"ruins icon",
	"chop progress",
	"trench progress",
	"harvest progress",
	"garden workforce progress",
	"quarry workforce progress",
	"build recipe",
	"found time",
	"redesignate time",
	"ceded",
	"\n"
};


// used for BUILDING_RUINS
const char *ruins_icons[NUM_RUINS_ICONS] = {
	"..&0/]",
	"&0[\\&?..",
	"&0|\\&?..",
	"..&0/|",
	".&0-&?.&0]",
	"&0[&?.&0-&?.",
	"&0[&?__&0]"
};


// SECTF_x
const char *sector_flags[] = {
	"LOCK-ICON",
	"IS-ADVENTURE",
	"NON-ISLAND",
	"CHORE",
	"!CLAIM",
	"START-LOCATION",
	"FRESH-WATER",
	"OCEAN",
	"DRINK",
	"HAS-CROP-DATA",
	"CROP",
	"LAY-ROAD",
	"IS-ROAD",
	"CAN-MINE",
	"SHOW-ON-POLITICAL-MAPOUT",
	"MAP-BUILDING",
	"INSIDE-ROOM",
	"LARGE-CITY-RADIUS",
	"OBSCURE-VISION",
	"IS-TRENCH",
		"*",
	"ROUGH",
	"SHALLOW-WATER",
	"\n"
};


// SPAWN_x
const char *spawn_flags[] = {
	"NOCTURNAL",
	"DIURNAL",
	"CLAIMED",
	"UNCLAIMED",
	"CITY",
	"OUT-OF-CITY",
	"NORTHERN",
	"SOUTHERN",
	"EASTERN",
	"WESTERN",
	"\n"
};


// SPAWN_x -- short form for compressed display
const char *spawn_flags_short[] = {
	"NOCT",
	"DIA",
	"CLM",
	"!CLM",
	"CITY",
	"!CITY",
	"NORTH",
	"SOUTH",
	"EAST",
	"WEST",
	"\n"
};


// TILESET_x
const char *seasons[] = {
	"Season data not found.",	// TILESET_ANY (should never hit this case)
	"It is spring and everything is flowering.",
	"It's summer time and you're quite warm.",
	"It is autumn and leaves are falling from the trees.",
	"It's winter and you're a little bit cold.",
	"\n"
};


// TILESET_x
const char *icon_types[] = {
	"Any",
	"Spring",
	"Summer",
	"Autumn",
	"Winter",
	"\n"
};


// SKY_x -- mainly used by scripting
const char *weather_types[] = {
	"sunny",
	"cloudy",
	"raining",
	"lightning",
	"\n"
};


 //////////////////////////////////////////////////////////////////////////////
//// SKILL CONSTANTS /////////////////////////////////////////////////////////

/* ATYPE_x */
const char *affect_types[] = {
	"!RESERVED!",	// 0
	"fly",
	"entrancement",
	"darkness",
	"poisoned",
	"boost",	// 5
	"cut deep",
	"sap",
	"mana shield",
	"foresight",
	"earthmeld",	// 10
	"mummify",
	"earth armor",
	"bestow vigor",
	"weakness",
	"colorburst",	// 15
	"heartstop",
	"phoenix rite",
	"disarm",
	"shocked",
	"skybrand",  // 20
	"counterspell",
	"hasten",
	"rejuvenate",
	"entangle",
	"radiance",	// 25
	"inspire",
	"jabbed",
	"blind",
	"healing potion",
	"nature potion",	// 30
	"vigor",
	"enervate",
	"enervate",
	"siphon",
	"slow",	// 35
	"sunshock",
	"tripping",
	"siphoned",
	"affect",	// 39 -- DG Scripts affect
	"claws",	// 40
	"deathshroud",
	"soulmask",
	"majesty",
	"alacrity",
	"nightsight", // 45
	"death penalty",
	"bash",
	"terrify",
	"stunning blow",
	"stun immunity",	// 50
	"war delay",
	"unburdened",
	"shadow kick",
	"stagger jab",
	"shadowcage",	// 55
	"howl",
	"crucial jab",
	"diversion",
	"shadow jab",
	"confer",	// 60
	"conferred",
	"\n"
	};


// ATYPE_x -- empty string will send no message at all
const char *affect_wear_off_msgs[] = {
	"!RESERVED!",	// 0
	"You land quickly as your magical flight wears off.",
	"You no longer feel entranced.",
	"The blanket of darkness dissipates.",
	"Poison fades from your system.",
	"You suddenly feel weaker.",	// 5
	"You're no longer bleeding from your deep cuts.",
	"You are no longer stunned.",
	"Your mana shield fades.",
	"Your foresight ends.",
	"You rise from the ground!",	// 10
	"Your flesh returns to normal.",
	"Your plating of earth armor fades.",
	"Your heightened Fortitude fades.",
	"Your weakness fades.",
	"You are no longer distracted by colorburst.",	// 15
	"Your vitae is no longer hindered.",
	"Your phoenix rite expires.",
	"You recover and are no longer disarmed.",
	"The lightning shock wears off.",
	"The skybrand fades.",  // 20
	"Your counterspell expires.",
	"Your haste fades.",
	"The rejuvenation effect ends.",
	"You are no longer entangled in vines.",
	"Your radiant aura fades.",	// 25
	"Your inspiration fades.",
	"Your jab wound stops bleeding.",
	"You are no longer blinded by sand.",
	"Your healing potion effect ends.",
	"Your nature potion effect ends.",	// 30
	"Your vigor penalty ends.",
	"Your stamina begins to return.",
	"You are no longer gaining stamina from enervate.",
	"You are no longer receiving mana from the siphon.",
	"You no longer feel lethargic.",	// 35
	"You are no longer blinded by the light.",
	"Stuff isn't doing that thing anymore.",
	"Your mana is no longer siphoned.",
	"",	// 39 -- DG Scripts affect
	"Your claws retract.",	// 40
	"Your deathshroud fades.",
	"Your soulmask fades.",
	"You are no longer so majestic.",
	"Your alacrity fades.",
	"Your nightsight fades.",	// 45
	"Your death penalty ends.",
	"You are no longer stunned by that bash.",
	"You are no longer terrified.",
	"You are no longer dazed by that stunning blow.",
	"Your stun immunity expires.",	// 50
	"Your war delay ends and you are free to act.",
	"You feel the weight of the world return.",
	"You are no longer weakened by the shadow kick.",
	"You are no longer weakened by the stagger jab.",
	"The shadowcage fades and your focus returns.",	// 55
	"The terrifying howl fades from your mind.",
	"You are no longer weakened by the crucial jab.",
	"You are no longer distracted by the diversion.",
	"You are no longer weakened by the shadow jab.",
	"The power you were conferred has faded.",	// 60
	"Your conferred strength returns.",
	"\n"
};


// COOLDOWN_x
const char *cooldown_types[] = {
	"!RESERVED!",	// 0
	"respawn",
	"left empire",
	"hostile flag",
	"pvp flag",
	"pvp quit timer",	// 5
	"milk",
	"shear",
	"disarm",
	"outrage",
	"rescue",	// 10
	"kick",
	"bash",
	"colorburst",
	"enervate",
	"slow",	// 15
	"siphon",
	"mirrorimage",
	"sunshock",
	"teleport home",
	"city teleportation",	// 20
	"rejuvenate",
	"cleanse",
	"lightningbolt",
	"skybrand",
	"entangle",	// 25
	"heartstop",
	"summon humans",
	"summon animals",
	"summon guards",
	"summon bodyguard",	// 30
	"summon thugs",
	"summon swift",
	"reward",
	"search",
	"terrify",	// 35
	"darkness",
	"shadowstep",
	"backstab",
	"jab",
	"blind",	// 40
	"sap",
	"prick",
	"weaken",
	"moonrise",
	"alternate",	// 45
	"dispel",
	"bloodsweat",
	"earthmeld",
	"shadowcage",
	"howl",	// 50
	"diversion",
	"rogue flag",
	"portal sickness",	// 53
	"\n"
};


// DAM_x damage types
const char *damage_types[] = {
	"physical",
	"magical",
	"fire",
	"poison",
	"direct",
	"\n"
};


// for do_fish
const struct fishing_data_type fishing_data[] = {
	{ o_GLOWING_SEASHELL, 20 },	// formerly 12	// formerly 8.5
	{ o_WORN_STATUETTE, 0.25 },
	{ o_HESTIAN_TRINKET, 0.25 },
	{ o_TRINKET_OF_CONVEYANCE, 0.25 },
	{ o_SEA_JUNK, 5 },
	{ o_LINEFISH, 25 },
	{ o_ARROWFISH, 25 },

	// must come last -- any catch-all with 100% to grab the overflow
	{ o_BARBFISH, 100 }
};


// master mines data
const struct mine_data_type mine_data[] = {
	// type, name, vnum, min amt, max amt, chance, ability
	{ MINE_COPPER, "copper", o_COPPER, 20, 40, 20.0, NO_ABIL },
	{ MINE_SILVER, "silver", o_SILVER, 15, 25, 3.5, NO_ABIL },
	{ MINE_GOLD, "gold", o_GOLD, 10, 20, 0.5, ABIL_RARE_METALS },
	{ MINE_NOCTURNIUM, "nocturnium", o_NOCTURNIUM_ORE, 10, 20, 1.5, ABIL_RARE_METALS },
	{ MINE_IMPERIUM, "imperium", o_IMPERIUM_ORE, 10, 20, 1.5, ABIL_RARE_METALS },
	
	// put this as last real entry, since it's "default" if the others miss (-1 chance means "always")
	{ MINE_IRON, "iron", o_IRON_ORE, 30, 50, -1, NO_ABIL },
	
	{ NOTHING, "\n", NOTHING, 0, 0, 0, NO_ABIL }
};


// for do_smelt
const struct smelt_data_type smelt_data[] = {
	// from, amt			to, amt					workforce?

	// metals
	{ o_IRON_ORE, 2,		o_IRON_INGOT, 1,		TRUE },
	{ o_IMPERIUM_ORE, 2,	o_IMPERIUM_INGOT, 1,	TRUE },
	{ o_NOCTURNIUM_ORE, 2,	o_NOCTURNIUM_INGOT, 1,	TRUE },
	{ o_COPPER, 2,			o_COPPER_INGOT, 1,		TRUE },
	
	// preciouses
	{ o_GOLD_SMALL, 8,		o_GOLD_DISC, 1,			TRUE },
	{ o_SILVER, 2,			o_SILVER_DISC, 1,		TRUE },
	{ o_GOLD, 2,			o_GOLD_DISC, 1,			TRUE },
	{ o_SILVER_DISC, 2,		o_SILVER_BAR, 1,		TRUE },
	{ o_GOLD_DISC, 2,		o_GOLD_BAR, 1,			TRUE },
	{ o_COPPER_INGOT, 2,	o_COPPER_BAR, 1,		TRUE },

	// melt-down versions
	{ o_SILVER_BAR, 1,		o_SILVER, 4,			FALSE },
	{ o_GOLD_BAR, 1,		o_GOLD, 4,				FALSE },
	{ o_COPPER_BAR, 1,		o_COPPER_INGOT, 2,		FALSE },

	// last
	{ NOTHING, 0, NOTHING, 0, FALSE }
};


// for do_tan
// TODO tanning could change to interaction
const struct tanning_data_type tan_data[] = {
	{ o_SMALL_SKIN, o_SMALL_LEATHER },
	{ o_LARGE_SKIN, o_LARGE_LEATHER },
	
	// last
	{ NOTHING, NOTHING }
};


 //////////////////////////////////////////////////////////////////////////////
//// TRIGGER CONSTANTS ///////////////////////////////////////////////////////

// MTRIG_x -- mob trigger types
const char *trig_types[] = {
	"Global", 
	"Random",
	"Command",
	"Speech",
	"Act",
	"Death",	// 5
	"Greet",
	"Greet-All",
	"Entry",
	"Receive",
	"Fight",	// 10
	"HitPrcnt",
	"Bribe",
	"Load",
	"Memory",
	"Ability",	// 15
	"Leave",
	"Door",
	"Leave-All",
	"Fight-Charmed",
	"\n"
};

// MTRIG_x -- mob trigger argument types
const bitvector_t mtrig_argument_types[] = {
	NOBITS,	// global
	TRIG_ARG_PERCENT,	// random
	TRIG_ARG_COMMAND,	// command
	TRIG_ARG_PHRASE_OR_WORDLIST,	// speech
	TRIG_ARG_PHRASE_OR_WORDLIST,	// act
	TRIG_ARG_PERCENT,	// death
	TRIG_ARG_PERCENT,	// greet
	TRIG_ARG_PERCENT,	// greet all
	TRIG_ARG_PERCENT,	// entry
	TRIG_ARG_PERCENT,	// receive
	TRIG_ARG_PERCENT,	// fight
	TRIG_ARG_PERCENT,	// hit percent
	TRIG_ARG_COST,	// bribe
	TRIG_ARG_PERCENT,	// load
	TRIG_ARG_PERCENT,	// memory
	TRIG_ARG_PERCENT,	// ability
	TRIG_ARG_PERCENT,	// leave
	TRIG_ARG_PERCENT,	// door
	TRIG_ARG_PERCENT	// leave-all
};


// OTRIG_x -- obj trigger types
const char *otrig_types[] = {
	"Global",
	"Random",
	"Command",
	"*",
	"*",
	"Timer",
	"Get",
	"Drop",
	"Give",
	"Wear",
	"*",
	"Remove",
	"*",
	"Load",
	"*",
	"Ability",
	"Leave",
	"*",
	"Consume",
	"\n"
};

// OTRIG_x -- obj trigger argument types
const bitvector_t otrig_argument_types[] = {
	NOBITS,	// global
	TRIG_ARG_PERCENT,	// random
	TRIG_ARG_OBJ_WHERE,	// command
	NOBITS,	//
	NOBITS,	//
	NOBITS,	// timer
	TRIG_ARG_PERCENT,	// get
	TRIG_ARG_PERCENT,	// drop
	TRIG_ARG_PERCENT,	// give
	NOBITS,	// wear
	NOBITS,	//
	NOBITS,	// remove
	NOBITS,	// 
	TRIG_ARG_PERCENT,	// load
	NOBITS,	// 
	TRIG_ARG_PERCENT,	// ability
	TRIG_ARG_PERCENT,	// leave
	NOBITS,	// 
	TRIG_ARG_PERCENT	// consume
};


/* wld trigger types */
const char *wtrig_types[] = {
	"Global",
	"Random",
	"Command",
	"Speech",
	"Adventure Cleanup",
	"Zone Reset",
	"Enter",
	"Drop",
	"*",
	"*",
	"*",
	"*",
	"*",
	"*",
	"*",
	"Ability",
	"Leave",
	"Door",
	"*",
	"\n"
};

// WTRIG_x -- world trigger argument types
const bitvector_t wtrig_argument_types[] = {
	NOBITS,	// global
	TRIG_ARG_PERCENT,	// random
	TRIG_ARG_COMMAND,	// command
	TRIG_ARG_PHRASE_OR_WORDLIST,	// speech
	TRIG_ARG_PERCENT,	// adventure cleanup
	TRIG_ARG_PERCENT,	// zone reset
	TRIG_ARG_PERCENT,	// enter
	TRIG_ARG_PERCENT,	// drop
	NOBITS,	// 
	NOBITS,	// 
	NOBITS,	// 
	NOBITS,	// 
	NOBITS,	// 
	NOBITS,	// 
	NOBITS,	// 
	TRIG_ARG_PERCENT,	// ability
	TRIG_ARG_PERCENT,	// leave
	TRIG_ARG_PERCENT,	// door
	NOBITS,	// 
};


// x_TRIGGER
const char *trig_attach_types[] = {
	"Mobile",
	"Object",
	"Room",
	"*",	// rmt_trigger -- never set on an actual trigger
	"*",	// adv_trigger -- never set on an actual trigger
	"\n"
};


// x_TRIGGER -- get typenames by attach type
const char **trig_attach_type_list[] = {
	trig_types,
	otrig_types,
	wtrig_types,
	wtrig_types,	// RMT_TRIGGER (not really used)
	wtrig_types	// ADV_TRIGGER (not really used)
};


// x_TRIGGER -- argument types by attach type
const bitvector_t *trig_argument_type_list[] = {
	mtrig_argument_types,	// MOB_TRIGGER
	otrig_argument_types,	// OBJ_TRIGGER
	wtrig_argument_types,	// WLD_TRIGGER
	wtrig_argument_types,	// RMT_TRIGGER (not really used)
	wtrig_argument_types	// ADV_TRIGGER (not really used)
};


 //////////////////////////////////////////////////////////////////////////////
//// MISC CONSTANTS //////////////////////////////////////////////////////////

// for command-interpreting
const char *fill_words[] = {
	"in",
	"from",
	"with",
	"the",
	"on",
	"at",
	"to",
	"\n"
};


// GLOBAL_x types
const char *global_types[] = {
	"Mob Interactions",
	"\n"
};


// GLB_FLAG_X global flags
const char *global_flags[] = {
	"IN-DEVELOPMENT",
	"ADVENTURE-ONLY",
	"\n"
};


// INTERACT_x, see also interact_vnum_types, interact_attach_types
const char *interact_types[] = {
	"BUTCHER",
	"SKIN",
	"SHEAR",
	"BARDE",
	"LOOT",
	"DIG",
	"FORAGE",
	"FIND-HERB",
	"HARVEST",
	"GATHER",
	"ENCOUNTER",
	"LIGHT",
	"PICKPOCKET",
	"\n"
};


// INTERACT_x, see also interact_types, interact_vnum_types
const int interact_attach_types[NUM_INTERACTS] = {
	TYPE_MOB,
	TYPE_MOB,
	TYPE_MOB,
	TYPE_MOB,
	TYPE_MOB,
	TYPE_ROOM,	// dig
	TYPE_ROOM,
	TYPE_ROOM,
	TYPE_ROOM,
	TYPE_ROOM,
	TYPE_ROOM,
	TYPE_OBJ,	// light
	TYPE_MOB	// pickpocket
};


// INTERACT_x, see also interact_types, interact_attach_types
const byte interact_vnum_types[NUM_INTERACTS] = {
	TYPE_OBJ,
	TYPE_OBJ,
	TYPE_OBJ,
	TYPE_MOB,	// barde
	TYPE_OBJ,
	TYPE_OBJ,
	TYPE_OBJ,
	TYPE_OBJ,
	TYPE_OBJ,
	TYPE_OBJ,
	TYPE_MOB,	// encounter
	TYPE_OBJ,
	TYPE_OBJ	// pickpocket
};


// for command-interpreting
const char *reserved_words[] = {
	"a",
	"an",
	"some",
	"self",
	"me",
	"all",
	"room",
	"someone",
	"something",
	"\n"
};


// You can use any names you want for weekdays, but there are always 7
const char *weekdays[] = {
	"Monday",
	"Tuesday",
	"Wednesday",
	"Thursday",
	"Friday",
	"Saturday",
	"Sunday",
	"\n"
};


// You can use any names you want for the months, so long as there are 12
const char *month_name[] = {
	"January",				"February",					"March",
	"April",				"May",						"June",
	"July",					"August",					"September",
	"October",				"November",					"December",
	"\n"
};


// used for olc parsers
const char *offon_types[] = {
	"off",
	"on",
	"\n"
};


// SHUTDOWN_x
const char *shutdown_types[] = {
	"normal",
	"pause",
	"die",
	"\n"
};
