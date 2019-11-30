/* ************************************************************************
*   File: constants.c                                     EmpireMUD 2.0b5 *
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
#include "utils.h"
#include "skills.h"
#include "interpreter.h"	/* alias_data */
#include "vnums.h"
#include "olc.h"

/**
* Contents:
*   EmpireMUD Constants
*   Ability Constants
*   Adventure Constants
*   Archetype Constants
*   Augment Constants
*   Class Constants
*   Player Constants
*   Direction And Room Constants
*   Character Constants
*   Craft Recipe Constants
*   Empire Constants
*   Event Constants
*   Faction Constants
*   Generic Constants
*   Mob Constants
*   Item Contants
*   OLC Constants
*   Progress Constants
*   Quest Constants
*   Room/World Constants
*   Shop Constants
*   Skill Constants
*   Social Constants
*   Trigger Constants
*   Misc Constants
*/

// external funcs
void afk_notify(char_data *ch);
void tog_informative(char_data *ch);
void tog_mapcolor(char_data *ch);
void tog_political(char_data *ch);
void tog_pvp(char_data *ch);


 //////////////////////////////////////////////////////////////////////////////
//// EMPIREMUD CONSTANTS /////////////////////////////////////////////////////

// Shown on the "version" command and sent over MSSP
const char *version = "EmpireMUD 2.0 beta 5";


// data for the built-in game levels -- this adapts itself if you reduce the number of immortal levels
const char *level_names[][2] = {
		{ "N/A", "Not Started" },
		{ "MORT", "Mortal" },
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
//// ABILITY CONSTANTS ///////////////////////////////////////////////////////

// ABILF_x: ability flags
const char *ability_flags[] = {
	"VIOLENT",	// 0
	"COUNTERSPELLABLE",
	"TOGGLE",
	"INVISIBLE",
	"!ENGAGE",
	"RANGED",	// 5
	"!ANIMAL",
	"!INVULNERABLE",
	"CASTER-ROLE",
	"HEALER-ROLE",
	"MELEE-ROLE",	// 10
	"TANK-ROLE",
	"RANGED-ONLY",
	"IGNORE-SUN",
	"\n"
};


// ABILT_x: ability type flags
const char *ability_type_flags[] = {
	"CRAFT",	// 0
	"BUFF",
	"DAMAGE",
	"DOT",
	"PLAYER-TECH",
/*
	"UNAFFECTS",
	"POINTS",
	"ALTER-OBJS",
	"GROUPS",
	"MASSES",
	"AREAS",
	"SUMMONS",
	"CREATIONS",
	"MANUAL",
	"ROOMS",
	"CRAFT",
*/
	"\n"
};


// ATAR_x: ability targeting flags
const char *ability_target_flags[] = {
	"IGNORE",	// 0
	"CHAR-ROOM",
	"CHAR-WORLD",
	"CHAR-CLOSEST",
	"FIGHT-SELF",
	"FIGHT-VICTIM",	// 5
	"SELF-ONLY",
	"NOT-SELF",
	"OBJ-INV",
	"OBJ-ROOM",
	"OBJ-WORLD",	// 10
	"OBJ-EQUIP",
	"VEH-ROOM",
	"VEH-WORLD",
	"\n"
};


// ABIL_CUSTOM_x
const char *ability_custom_types[] = {
	"self-to-char",	// 0
	"self-to-room",
	"targ-to-char",
	"targ-to-vict",
	"targ-to-room",
	"counterspell-to-char",	// 5
	"counterspell-to-vict",
	"counterspell-to-room",
	"fail-self-to-char",
	"fail-self-to-room",
	"fail-targ-to-char",	// 10
	"fail-targ-to-vict",
	"fail-targ-to-room",
	"\n"
};


// ADL_x: for adding to ability_data_list
const char *ability_data_types[] = {
	"PTECH",
	"EFFECT",
	"\n"
};


// ABIL_EFFECT_x: things that happen when an ability is used
const char *ability_effects[] = {
	"dismount",	// 0
	"\n"
};


// AGH_x: ability gain hooks
const char *ability_gain_hooks[] = {
	"ONLY-WHEN-AFFECTED",	// 0
	"MELEE",
	"RANGED",
	"DODGE",
	"BLOCK",
	"TAKE-DAMAGE",	// 5
	"PASSIVE-FREQUENT",
	"PASSIVE-HOURLY",
	"ONLY-DARK",
	"ONLY-LIGHT",
	"ONLY-VS-ANIMAL",	// 10
	"VAMPIRE-FEEDING",
	"MOVING",
	"\n"
};


 //////////////////////////////////////////////////////////////////////////////
//// ADVENTURE CONSTANTS /////////////////////////////////////////////////////

// ADV_x
const char *adventure_flags[] = {
	"IN-DEVELOPMENT",	// 0
	"LOCK-LEVEL-ON-ENTER",
	"LOCK-LEVEL-ON-COMBAT",
	"!NEARBY",
	"ROTATABLE",
	"CONFUSING-RANDOMS",	// 5
	"!NEWBIE",
	"NEWBIE-ONLY",
	"NO-MOB-CLEANUP",
	"EMPTY-RESET-ONLY",
	"CAN-DELAY-LOAD",	// 10
	"IGNORE-WORLD-SIZE",
	"IGNORE-ISLAND-LEVELS",
	"CHECK-OUTSIDE-FIGHTS",
	"GLOBAL-NEARBY",
	"\n"
};


// ADV_LINKF_x
const char *adventure_link_flags[] = {
	"CLAIMED-OK",	// 0
	"CITY-ONLY",
	"!CITY",
	"CLAIMED-ONLY",
	"CONTINENT-ONLY",
	"!CONTINENT",	// 5
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
	"PORTAL-CROP",
	"EVENT-RUNNING",
	"\n"
};


// ADV_SPAWN_x
const char *adventure_spawn_types[] = {
	"MOB",
	"OBJ",
	"VEH",
	"\n"
};


// INST_x
const char *instance_flags[] = {
	"COMPLETED",
	"NEEDS-LOAD",
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
	"*",
	"*",
	"\n"
};


 //////////////////////////////////////////////////////////////////////////////
//// ARCHETYPE CONSTANTS /////////////////////////////////////////////////////

// ARCH_x: archetype flags
const char *archetype_flags[] = {
	"IN-DEVELOPMENT",
	"BASIC",
	"\n"
};


// ARCHT_x (1/2): archetype types
const char *archetype_types[] = {
	"ORIGIN",
	"HOBBY",
	"\n"
};


// ARCHT_x (2/2): The order and contents of this array determine what players see during creation.
struct archetype_menu_type archetype_menu[] = {
	{ ARCHT_ORIGIN,
		"Origin",
		"Your character's origins, be they noble or meager, determine your attributes\r\n"
		"and starting skills. But remember, this is only the beginning of your path.\r\n"
	},
	{ ARCHT_HOBBY,
		"Hobby",
		"Round out your character by selecting a hobby. You will receive 5 extra skill\r\n"
		"points in whichever skill you choose.\r\n"
	},
	
	{ NOTHING, "\n", "\n" }	// this goes last
};


 //////////////////////////////////////////////////////////////////////////////
//// AUGMENT CONSTANTS ///////////////////////////////////////////////////////

// AUGMENT_x (1/2): augment types
const char *augment_types[] = {
	"None",
	"Enchantment",
	"Hone",
	"\n"
};


// AUGMENT_x (1/2): augment type data
const struct augment_type_data augment_info[] = {
	// noun, verb, apply-type, default-flags, use-obj-flag
	{ "augment", "augment", APPLY_TYPE_NATURAL, NOBITS, NOBITS },
	{ "enchantment", "enchant", APPLY_TYPE_ENCHANTMENT, NOBITS, OBJ_ENCHANTED },
	{ "hone", "hone", APPLY_TYPE_HONED, AUG_SELF_ONLY, NOBITS },
	
	{ "\n", "\n", 0, 0 }	// last
};


// AUG_x: augment flags
const char *augment_flags[] = {
	"IN-DEVELOPMENT",
	"SELF-ONLY",
	"ARMOR",
	"SHIELD",
	"\n"
};


 //////////////////////////////////////////////////////////////////////////////
//// CLASS CONSTANTS /////////////////////////////////////////////////////////

// CLASSF_x: class flags
const char *class_flags[] = {
	"IN-DEVELOPMENT",
	"\n"
};


 //////////////////////////////////////////////////////////////////////////////
//// PLAYER CONSTANTS ////////////////////////////////////////////////////////

// ACCT_x: Account flags
const char *account_flags[] = {
	"FROZEN",
	"MUTED",
	"SITEOK",
	"!TITLE",
	"MULTI-IP",
	"MULTI-CHAR",
	"APPR",
	"!CUSTOMIZE",
	"\n"
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
	"BLOOD",
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
	"Extra blood (for vampires)",
	"\n"
};


// COND_x: player conditions
const char *condition_types[] = {
	"Drunk",
	"Full",
	"Thirst",
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


// ATT_x: extra attributes
const char *extra_attribute_types[] = {
	"Bonus-Inventory",
	"Resist-Physical",
	"Block",
	"To-Hit",
	"Dodge",
	"Extra-Blood",	// 5
	"Bonus-Physical",
	"Bonus-Magical",
	"Bonus-Healing",
	"Heal-Over-Time",
	"Resist-Magical",	// 10
	"Crafting-Bonus",
	"Blood-Upkeep",
	"Age",
	"\n"
};


// FM_x: combat messages
const char *combat_message_types[] = {
	"my hits",	// 0
	"my misses",
	"hits against me",
	"misses against me",
	"ally hits",
	"ally misses",		// 5
	"hits against allies",
	"misses against allies",
	"hits against target",
	"misses against target",
	"hits against tank",	// 10
	"misses against tank",
	"other hits",
	"other misses",
	"autodiagnose",
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
	"approve",
	"forgive",
	"hostile",
	"slay",
	"island",	// 35
	"oset",
	"playerdelete",
	"unquest",
	"automessage",
	"peace",	// 40
	"unprogress",
	"events",
	"\n"
};


// MOUNT_x: mount flags
const char *mount_flags[] = {
	"riding",
	"aquatic",
	"flying",
	"\n"
};


/* PLR_x */
const char *player_bits[] = {
	"APPR",
	"WRITING",
	"MAILING",
	"DONTSET",
		"UNUSED",
		"UNUSED",
		"UNUSED",
		"UNUSED",
	"LOADRM",
	"!WIZL",
	"!DEL",
	"INVST",
	"IPMASK",
	"DISGUISED",
		"UNUSED",
		"UNUSED",
	"NEEDS-NEWBIE-SETUP",
	"!RESTICT",
	"KEEP-LOGIN",
	"EXTRACTED",
	"ADV-SUMMON",
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
	"AUTONOTES",
	"AUTODISMOUNT",
	"!EMPIRE",
	"CLEARMETERS",
	"!TUTORIALS",
	"!PAINT",
	"EXTRA-SPACING",
	"TRAVEL-LOOK",
	"AUTOCLIMB",
	"AUTOSWIM",
	"\n"
};


// ROLE_x (1/2): role names
const char *class_role[] = {
	"none",
	"Tank",
	"Melee",
	"Caster",
	"Healer",
	"Utility",
	"Solo",
	"\n"
};


// ROLE_x (2/2): role colors for who list
const char *class_role_color[] = {
	"\t0",
	"\ty",	// tank
	"\tr",	// melee
	"\ta",	// caster
	"\tj",	// healer
	"\tm",	// utility
	"\tw",	// solo
	"\n"
};


// PRF_x: for do_toggle, this controls the "toggle" command and these are displayed in rows of 3	
const struct toggle_data_type toggle_data[] = {
	// these are shown in rows of 3
	// name, type, prf, level, func
	
	{ "autoclimb", TOG_ONOFF, PRF_AUTOCLIMB, 0, NULL },
	{ "screen-reader", TOG_ONOFF, PRF_SCREEN_READER, 0, NULL },
	{ "afk", TOG_ONOFF, PRF_AFK, 0, afk_notify },
	
	{ "autodismount", TOG_ONOFF, PRF_AUTODISMOUNT, 0, NULL },
	{ "scrolling", TOG_ONOFF, PRF_SCROLLING, 0, NULL },
	{ "bother", TOG_ONOFF, PRF_BOTHERABLE, 0, NULL },
	
	{ "autokill", TOG_ONOFF, PRF_AUTOKILL, 0, NULL },
	{ "action-spam", TOG_OFFON, PRF_NOSPAM, 0, NULL },
	{ "clearmeters", TOG_ONOFF, PRF_CLEARMETERS, 0, NULL },
	
	{ "autorecall", TOG_ONOFF, PRF_AUTORECALL, 0, NULL },
	{ "brief", TOG_ONOFF, PRF_BRIEF, 0, NULL },
	{ "rp", TOG_ONOFF, PRF_RP, 0, NULL },
	
	{ "autoswim", TOG_ONOFF, PRF_AUTOSWIM, 0, NULL },
	{ "compact", TOG_ONOFF, PRF_COMPACT, 0, NULL },
	{ "pvp", TOG_ONOFF, PRF_ALLOW_PVP, 0, tog_pvp },
	
	{ "no-empire", TOG_ONOFF, PRF_NOEMPIRE, 0, NULL },
	{ "travel-look", TOG_ONOFF, PRF_TRAVEL_LOOK, 0, NULL },
	{ "mortlog", TOG_ONOFF, PRF_MORTLOG, 0, NULL },
	
	{ "tutorials",	TOG_OFFON, PRF_NO_TUTORIALS, 0, NULL },
	{ "extra-spacing",	TOG_ONOFF, PRF_EXTRA_SPACING, 0, NULL },
	{ "stealthable", TOG_ONOFF, PRF_STEALTHABLE, 0, NULL },
	
	{ "informative", TOG_ONOFF, PRF_INFORMATIVE, 0, tog_informative },
	{ "no-repeat", TOG_ONOFF, PRF_NOREPEAT, 0, NULL },
	{ "tell", TOG_OFFON, PRF_NOTELL, 0, NULL },
	
	{ "political", TOG_ONOFF, PRF_POLITICAL, 0, tog_political },
	{ "no-paint", TOG_ONOFF, PRF_NO_PAINT, 0, NULL },
	{ "shout", TOG_OFFON, PRF_DEAF, 0, NULL },
	
	{ "map-color", TOG_OFFON, PRF_NOMAPCOL, 0, tog_mapcolor },
	{ "channel-joins", TOG_OFFON, PRF_NO_CHANNEL_JOINS, 0, NULL },
	
	// imm section
	{ "wiznet", TOG_OFFON, PRF_NOWIZ, LVL_START_IMM, NULL },
	{ "holylight", TOG_ONOFF, PRF_HOLYLIGHT, LVL_START_IMM, NULL },
	{ "roomflags", TOG_ONOFF, PRF_ROOMFLAGS, LVL_START_IMM, NULL },
	
	{ "hassle", TOG_OFFON, PRF_NOHASSLE, LVL_START_IMM, NULL },
	{ "idle-out", TOG_OFFON, PRF_NO_IDLE_OUT, LVL_START_IMM, NULL },
	{ "incognito", TOG_ONOFF, PRF_INCOGNITO, LVL_START_IMM, NULL },
	
	{ "wizhide", TOG_ONOFF, PRF_WIZHIDE, LVL_START_IMM, NULL },
	{ "autonotes", TOG_ONOFF, PRF_AUTONOTES, LVL_START_IMM, NULL },
	
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
	"Cnf archetype",
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


// PTECH_x: player techs
const char *player_tech_types[] = {
	"RESERVED",	// 0
	"Armor-Heavy",
	"Armor-Light",
	"Armor-Mage",
	"Armor-Medium",
	"Block",	// 5
	"Block-Ranged",
	"Block-Magical",
	"Bonus-vs-Animals",
	"Butcher-Upgrade",
	"Customize-Building",	// 10
	"Deep-Mines",
	"Dual-Wield",
	"Fast-Wood-Processing",
	"Fastcasting",
	"Fast-Find",	// 15
	"Fish",
	"Forage",
	"Harvest-Upgrade",
	"Healing-Boost",
	"Hide-Upgrade",	// 20
	"Infiltrate",
	"Infiltrate-Upgrade",
	"Larger-Light-Radius",
	"Light-Fire",
	"Map-Invis",	// 25
	"Mill-Upgrade",
	"Navigation",
	"!Hunger",
	"!Poison",
	"!Thirst",	// 30
	"!Track-City",
	"!Track-Wild",
	"Pickpocket",
	"Poison",
	"Poison-Upgrade",	// 35
	"Portal",
	"Portal-Upgrade",
	"Ranged-Combat",
	"Riding",
	"Riding-Flying",	// 40
	"Riding-Upgrade",
	"Rough-Terrain",
	"See-Chars-In-Dark",
	"See-Objs-In-Dark",
	"See-Inventory",	// 45
	"Shear-Upgrade",
	"Steal-Upgrade",
	"Swimming",
	"Teleport-City",
	"Two-Handed-Weapons",	// 50
	"Where-Upgrade",
	"Dodge-Cap",
	"Skinning-Upgrade",
	"Barde",
	"Herd",	// 55
	"Milk",
	"Shear",
	"Tame",
	"Bite-Melee-Upgrade",
	"Bite-Tank-Upgrade",	// 60
	"Bite-Steal-Blood",
	"See-In-Dark-Outdoors",
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
	"empire",
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


// whether or not a direction can be used by designate, building version
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


// whether or not a direction can be used by designate, vehicle version
const bool can_designate_dir_vehicle[NUM_OF_DIRS] = {
	FALSE,	// n
	FALSE,
	FALSE,
	FALSE,
	FALSE,	// nw
	FALSE,
	FALSE,
	FALSE,
	TRUE,	// up
	TRUE,	// down
	TRUE,	// fore
	TRUE,
	TRUE,
	TRUE,
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


// whether or not a direction is "flat" (2D)
const bool is_flat_dir[NUM_OF_DIRS] = {
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


// for PTECH_NAVIGATION: confused_dir[which dir is north][reverse][which dir to translate]
// reverse=0 is for moving
// reverse=1 is for which way directions are displayed (this was very confusing to figure out)
const int confused_dirs[NUM_2D_DIRS][2][NUM_OF_DIRS] = {
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
	{ EAST, SOUTH, WEST, NORTH, NORTHEAST, SOUTHEAST, NORTHWEST, SOUTHWEST, UP, DOWN, FORE, STARBOARD, PORT, AFT, DIR_RANDOM }},
	
	// these rotations only work for adventures, not for the whole map:
	
	// NORTHWEST
	{{ NORTHWEST, NORTHEAST, SOUTHEAST, SOUTHWEST, WEST, NORTH, SOUTH, EAST, UP, DOWN, FORE, STARBOARD, PORT, AFT, DIR_RANDOM },
	{ SOUTHEAST, SOUTHWEST, NORTHWEST, NORTHEAST, EAST, SOUTH, NORTH, WEST, UP, DOWN, FORE, STARBOARD, PORT, AFT, DIR_RANDOM }},
	
	// NORTHEAST
	{{ NORTHEAST, SOUTHEAST, SOUTHWEST, NORTHWEST, NORTH, EAST, WEST, SOUTH, UP, DOWN, FORE, STARBOARD, PORT, AFT, DIR_RANDOM },
	{ SOUTHWEST, NORTHWEST, NORTHEAST, SOUTHEAST, SOUTH, WEST, EAST, NORTH, UP, DOWN, FORE, STARBOARD, PORT, AFT, DIR_RANDOM }},
	
	// SOUTHWEST
	{{ SOUTHWEST, NORTHWEST, NORTHEAST, SOUTHEAST, SOUTH, WEST, EAST, NORTH, UP, DOWN, FORE, STARBOARD, PORT, AFT, DIR_RANDOM },
	{ NORTHEAST, SOUTHEAST, SOUTHWEST, NORTHWEST, NORTH, EAST, WEST, SOUTH, UP, DOWN, FORE, STARBOARD, PORT, AFT, DIR_RANDOM }},
	
	// SOUTHEAST
	{{ SOUTHEAST, SOUTHWEST, NORTHWEST, NORTHEAST, EAST, SOUTH, NORTH, WEST, UP, DOWN, FORE, STARBOARD, PORT, AFT, DIR_RANDOM },
	{ NORTHWEST, NORTHEAST, SOUTHEAST, SOUTHWEST, WEST, NORTH, SOUTH, EAST, UP, DOWN, FORE, STARBOARD, PORT, AFT, DIR_RANDOM }}
};


// for PTECH_NAVIGATION: how_to_show_map[dir which is north for char][x=0,y=1]
// for each direction, whether the x/y coord goes from positive to negative (1) or negative to positive (-1)
int how_to_show_map[NUM_SIMPLE_DIRS][2] = {
	{ -1, 1 },	// north
	{ -1, -1 },	// east
	{ 1, -1 },	// south
	{ 1, 1 }	// west
};


// for PTECH_NAVIGATION: show_map_y_first[dir which is north for char]
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
	"!DRINK-BLOOD",
	"DISTRACTED",
	"HARD-STUNNED",
	"IMMUNE-DAMAGE",	// 35
	"!WHERE",
	"\n"
};

// AFF_x (2/3) - strings shown when you consider someone (empty for no-show)
const char *affected_bits_consider[] = {
	"",	// 0 - blind
	"$E has a majestic aura!",	// majesty
	"",	// infra
	"",	// sneak
	"",	// hide
	"",	// 5 - charm
	"",	// invis
	"$E is immune to Battle debuffs.",	// !battle
	"",	// sense hide
	"$E is immune to physical damage.",	// !physical
	"",	// 10 - no-target-in-room
	"",	// no-see-in-room
	"",	// fly
	"$E cannot be attacked.",	// !attack
	"$E is immune to High Sorcery debuffs.",	// !highsorc
	"",	// 15 - disarm
	"",	// haste
	"",	// entangled
	"",	// slow
	"",	// stunned
	"",	// 20 - stoned
	"",	// can't spend blood
	"",	// claws
	"",	// deathshroud
	"",	// earthmeld
	"",	// 25 - mummify
	"$E is soulmasked.",	// soulmask
	"$E is immune to Natural Magic debuffs.",	// !natmag
	"$E is immune to Stealth debuffs.",	// !stealth
	"$E is immune to Vampire debuffs.",	// !vampire
	"$E is immune to stuns.",	// 30 - !stun
	"",	// ordred
	"",	// !drink-blood
	"",	// distracted
	"",	// hard-stunned
	"",	// 35 - immune-damage
	"",	// !where
	"\n"
};

// AFF_x (3/3) - determines if an aff flag is "bad" for the bearer
const bool aff_is_bad[] = {
	TRUE,	// 0 - blind
	FALSE,
	FALSE,
	FALSE,
	FALSE,
	FALSE,	// 5
	FALSE,
	FALSE,
	FALSE,
	FALSE,
	FALSE,	// 10
	FALSE,
	FALSE,
	FALSE,
	FALSE,	// unused
	TRUE,	// 15 - disarm
	FALSE,
	TRUE,	// entangled
	TRUE,	// slow
	TRUE,	// stunned
	TRUE,	// 20 - stoned
	TRUE,	// !blood
	FALSE,
	FALSE,
	FALSE,
	FALSE,	// 25
	FALSE,
	FALSE,
	FALSE,
	FALSE,
	FALSE,	// 30
	FALSE,
	FALSE,
	TRUE,
	FALSE,	// hard-stunned (not 'bad' because it's uncleansable)
	FALSE,	// 35 - immune-damage
	FALSE,
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


// APPLY_TYPE_x: source of obj apply
const char *apply_type_names[] = {
	"natural",
	"enchantment",
	"honed",
	"superior",
	"hard-drop",
	"group-drop",
	"boss-drop",
	"\n"
};


/* APPLY_x (1/4) */
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
	"BLOOD-UPKEEP",
	"\n"
};


// APPLY_x (2/4) -- for rate_item (amount multiplied by the apply modifier to make each of these equal to 1)
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
	0.01,	// CRAFTING
	1,	// BLOOD-UPKEEP
};


// APPLY_x (3/4) applies that are directly tied to attributes
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
	NOTHING,	// crafting
	NOTHING	// blood-upkeep
};


// APPLY_x (4/4) if TRUE, this apply is not scaled (applied as-is)
const bool apply_never_scales[] = {
	FALSE,	// "NONE",
	FALSE,	// "STRENGTH",
	FALSE,	// "DEXTERITY",
	FALSE,	// "HEALTH-REGEN",
	FALSE,	// "CHARISMA",
	TRUE,	// "GREATNESS",
	FALSE,	// "MOVE-REGEN",
	FALSE,	// "MANA-REGEN",
	FALSE,	// "INTELLIGENCE",
	FALSE,	// "WITS",
	TRUE,	// "AGE",
	FALSE,	// "MAX-MOVE",
	FALSE,	// RESIST-PHYSICAL
	FALSE,	// "BLOCK",
	FALSE,	// "HEAL-OVER-TIME",
	FALSE,	// "HEALTH",
	FALSE,	// "MAX-MANA",
	FALSE,	// "TO-HIT",
	FALSE,	// "DODGE",
	FALSE,	// "INVENTORY",
	FALSE,	// "BLOOD",
	FALSE,	// BONUS-PHYSICAL
	FALSE,	// BONUS-MAGICAL
	FALSE,	// BONUS-HEALING
	FALSE,	// RESIST-MAGICAL
	TRUE,	// CRAFTING
	TRUE	// BLOOD-UPKEEP
};


// STRENGTH, etc (part 1)
struct attribute_data_type attributes[NUM_ATTRIBUTES] = {
	// Label, Description, low-stat error
	{ "Strength", "Strength improves your melee damage and lets you chop trees faster", "too weak" },
	{ "Dexterity", "Dexterity helps you hit opponents and dodge hits", "not agile enough" },
	{ "Charisma", "Charisma improves your success with Stealth abilities", "not charming enough" },
	{ "Greatness", "Greatness determines how much territory your empire can claim", "not great enough" },
	{ "Intelligence", "Intelligence improves your magical damage and healing", "not clever enough" },
	{ "Wits", "Wits improves your speed and effectiveness in combat", "too slow" }
};

// STRENGTH, etc (part 2)
int attribute_display_order[NUM_ATTRIBUTES] = {
	STRENGTH, CHARISMA, INTELLIGENCE,
	DEXTERITY, GREATNESS, WITS
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


// SIZE_x (1/2): character size, strings
const char *size_types[] = {
	"negligible",
	"tiny",
	"small",
	"normal",
	"large",
	"huge",	// 5
	"enormous",
	"\n"
};


// SIZE_x (1/2): character size, data
struct character_size_data size_data[] = {
	// max_blood, corpse_flags, can_take_corpse, show_on_map, corpse kws, corpse longdesc, body longdesc, show on look
	/* negligible */{ 100, NOBITS, TRUE, FALSE, "", "%s's corpse is festering on the ground.", "%s's body is lying here.", NULL },
	/* tiny */		{ 10, NOBITS, TRUE, FALSE, "tiny", "The tiny corpse of %s is rotting on the ground.", "%s's tiny body is lying here.", "$E is tiny!" },
	/* small */		{ 25, NOBITS, TRUE, FALSE, "", "%s's corpse is festering on the ground.", "%s's body is lying here.", "$E is small." },
	/* normal/human */	{ 100, OBJ_LARGE, TRUE, FALSE, "", "%s's corpse is festering on the ground.", "%s's body is lying here.", NULL },
	/* large */		{ 150, OBJ_LARGE, TRUE, FALSE, "large", "The large corpse of %s is rotting on the ground.", "%s's large body is lying here, rotting.", "$E is large." },
	/* huge */		{ 200, OBJ_LARGE, FALSE, FALSE, "huge", "The huge corpse of %s is festering here.", "The huge body of %s body is festering here.", "$E is huge!" },
	/* enormous */	{ 300, OBJ_LARGE, FALSE, TRUE, "enormous", "The enormous corpse of %s is rotting here.", "%s's enormous body is rotting here.", "$E is enormous!" },
};


 //////////////////////////////////////////////////////////////////////////////
//// CRAFT RECIPE CONSTANTS //////////////////////////////////////////////////

// CRAFT_x (1/2): flag names
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
	"VEHICLE",
	"SHIPYARD",
	"BLD-UPGRADED",
	"LEARNED",
	"BY-RIVER",
	"\n"
};


// CRAFT_x (2/2): how flags that show up on "craft info"
const char *craft_flag_for_info[] = {
	"pottery",
	"requires apiaries",
	"requires glassblowing",
	"requires glassblower building",
	"requires carpenter building",
	"alchemy",
	"requires sharp tool",
	"requires fire",
	"",	// soup
	"",	// in-dev
	"",	// upgrade
	"",	// dismantle-only
	"in-city only",
	"",	// vehicle
	"requires shipyard",
	"requires upgrade",
	"",	// learned
	"must be by a river",
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
	"MANUFACTURE",
	"SMELT",
	"PRESS",
	"\n"
};


 //////////////////////////////////////////////////////////////////////////////
//// EMPIRE CONSTANTS ////////////////////////////////////////////////////////

// name, icon, radius, show-to-others, is-capital
struct city_metadata_type city_type[] = {
	{ "outpost", "&0-&?C1&0-", 5, FALSE, FALSE },
	{ "village", "&0-&?C2&0-", 10, TRUE, FALSE },
	{ "city", "&0-&?C3&0-", 15, TRUE, FALSE },
	{ "capital", "&0-&?C4&0-", 25, TRUE, TRUE },

	// this must go last
	{ "\n", "\n", 0, FALSE, FALSE }
};


// DIPL_x: Diplomacy types
const char *diplomacy_flags[] = {
	"peace",	// 0
	"war",
	"thievery",
	"allied",
	"nonaggression",
	"trade",	// 5
	"distrust",
	"truce",
	"\n"
};


// OFFENSE_x: offense definitions
// note: weights are in relation to the offense_min_to_war and offenses_for_free_war
struct offense_info_type offense_info[NUM_OFFENSES] = {
	// name, weight
	{ "stealing", 15 },	// 0
	{ "attacked player", 5 },
	{ "guard tower", 1 },
	{ "killed player", 15 },
	{ "infiltrated", 2 },
	{ "attacked npc", 1 },	// 5
	{ "sieged building", 15 },
	{ "sieged vehicle", 15 },
	{ "burned building", 5 },
	{ "burned vehicle", 5 },
	{ "pickpocketed", 5 },	// 10
};


// ELOG_x (1/3)
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
	"Workforce",
	"Progress",
	"\n"
};


// ELOG_x (2/3): Whether or not logs are shown to players online
const bool show_empire_log_type[] = {
	TRUE,	// none
	TRUE,	// admin
	TRUE,	// diplo
	TRUE,	// hostility
	TRUE,	// members
	TRUE,	// territory
	FALSE,	// trade
	TRUE,	// logins
	FALSE,	// shipments
	FALSE,	// workforce
	TRUE,	// progress
};


// ELOG_x (3/3): Whether or not logs are shown on the base 'elog' command
const bool empire_log_request_only[] = {
	FALSE,	// none
	FALSE,	// admin
	FALSE,	// diplo
	FALSE,	// hostility
	FALSE,	// members
	FALSE,	// territory
	FALSE,	// trade
	TRUE,	// logins
	FALSE,	// shipments
	TRUE,	// workforce
	FALSE,	// progress
};


// EADM_x: empire admin flags
const char *empire_admin_flags[] = {
	"!WAR",
	"!STEAL",
	"CITY-CLAIMS-ONLY",
	"\n"
};


// EATT_x: empire attributes
const char *empire_attributes[] = {
	"Progress Pool",
	"Bonus City Points",
	"Max City Size",
	"Tty per 100 Wealth",
	"Tty per Greatness",
	"Workforce Cap",
	"Bonus Territory",
	"\n"
};


// ENEED_x: empire need types
const char *empire_needs_types[] = {
	"food for workforce",
	"\n"
};


// ENEED_STATUS_x: empire statuses
const char *empire_needs_status[] = {
	"UNSUPPLIED",
	"\n"
};


// EUS_x -- lowercase
const char *unique_storage_flags[] = {
	"vault",
	"\n"
};


// OFF_x: offense flags
const char *offense_flags[] = {
	"SEEN",
	"WAR",
	"AVENGED",
	"\n"
};


// TECH_x
const char *techs[] = {
	"Glassblowing",
	"City Lights",
	"Locks",
	"Apiaries",
	"Seaport",
	"Workforce",
	"Prominence",
	"Citizens",
	"Portals",
	"Master Portals",
	"Skilled Labor",
	"Trade Routes",
	"Exarch Crafts",
	"Deep Mines",
	"Rare Metals",
	"Bonus Experience",
	"Tunnels",
	"Fast Prospect",
	"Fast Excavate",
	"Hidden Progress",
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
	"progress",
	"\n"
};


// SCORE_x -- score types
const char *score_type[] = {
	"Community",
	"Defense",
	"Greatness",
	"Industry",
	"Inventory",
	"Members",
	"Playtime",
	"Prestige",
	"Territory",
	"Wealth",
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


// WF_PROB_x: Workforce problem logging
const char *wf_problem_types[] = {
	"no workers",
	"over limit",
	"depleted",
	"no resources",
	"already sheared",
	"delayed",
	"out of city",
	"\n"
};


 //////////////////////////////////////////////////////////////////////////////
//// EVENT CONSTANTS /////////////////////////////////////////////////////////

// EVT_x: event types
const char *event_types[] = {
};


// EVTF_x: event flags
const char *event_flags[] = {
	"IN-DEVELOPMENT",	// 0
	"CONTINUES",
	"\n"
};


// EVTS_x: event status
const char *event_status[] = {
	"not started",
	"running",
	"complete",
	"collected",
	"\n"
};


 //////////////////////////////////////////////////////////////////////////////
//// FACTION CONSTANTS ///////////////////////////////////////////////////////

// FCT_x: faction flags
const char *faction_flags[] = {
	"IN-DEVELOPMENT",
	"REP-FROM-KILLS",
	"\n"
};


// FCTR_x (1/2): relationship flags
const char *relationship_flags[] = {
	"SHARED-GAINS",
	"INVERSE-GAINS",
	"MUTUALLY-EXCLUSIVE",
	"UNLISTED",
	"\n"
};


// FCTR_x (2/2): relationship descriptions (shown to players)
const char *relationship_descs[] = {
	"Allied",
	"Enemies",
	"Mutually Exclusive",
	"",	// unlisted
	"\n"
};


// REP_x: faction reputation levels
struct faction_reputation_type reputation_levels[] = {
	// { type const, name, points to achieve this level } -> ASCENDING ORDER
	// note: you achieve the level when you reach the absolute value of its
	// points (-99 < x < 99 is neutral, +/-100 are the cutoffs for the first rank)
	
	{ REP_DESPISED, "Despised", "\tr", -1000 },
	{ REP_HATED, "Hated", "\tr", -750 },
	{ REP_LOATHED, "Loathed", "\to", -300 },
	{ REP_DISLIKED, "Disliked", "\ty", -100 },
	{ REP_NEUTRAL, "Neutral", "\tt", 0 },
	{ REP_LIKED, "Liked", "\tc", 100 },
	{ REP_ESTEEMED, "Esteemed", "\ta", 300 },
	{ REP_VENERATED, "Venerated", "\tg", 750 },
	{ REP_REVERED, "Revered", "\tg", 1000 },
	
	{ -1, "\n", "\t0", 0 },	// last
};


 //////////////////////////////////////////////////////////////////////////////
//// GENERIC CONSTANTS ///////////////////////////////////////////////////////

// GENERIC_x: generic types
const char *generic_types[] = {
	"UNKNOWN",
	"LIQUID",
	"ACTION",
	"COOLDOWN",
	"AFFECT",
	"CURRENCY",
	"\n"
};


// GEN_x: generic flags
const char *generic_flags[] = {
	// none yet defined
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
	"NO-CORPSE",
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
	"!TELEPORT",	// 30
	"!EXP",
	"!RESCALE",
	"SILENT",
	"\n"
};


// MOB_CUSTOM_x
const char *mob_custom_types[] = {
	"echo",
	"say",
	"say-day",
	"say-night",
	"echo-day",
	"echo-night",
	"\n"
};


// MOB_MOVE_x: mob/vehicle move types
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
	"drives",
	"sails",
	"rolls",
	"rattles",
	"skis",
	"slides",
	"soars",
	"lumbers",
	"floats",
	"lopes",
	"blows",
	"drifts",
	"bounces",
	"flows",
	"leaves",
	"shuffles",
	"marches",
	"sweeps",
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
	"\r!",	// share
	"\n"
};


// WEAR_x -- data for each wear slot
const struct wear_data_type wear_data[NUM_WEARS] = {
	// eq tag,				 name, wear bit,       count-stats, gear-level-mod, cascade-pos, already-wearing, wear-message-to-room, wear-message-to-char, allow-custom-msgs, save-to-eq-set
	{ "    <worn on head> ", "head", ITEM_WEAR_HEAD, TRUE, 1.0, NO_WEAR, "You're already wearing $p on your head.", "$n wears $p on $s head.", "You wear $p on your head.", TRUE, TRUE },
	{ "    <worn on ears> ", "ears", ITEM_WEAR_EARS, TRUE, 1.0, NO_WEAR, "You're already wearing $p on your ears.", "$n pins $p onto $s ears.", "You pin $p onto your ears.", TRUE, TRUE },
	{ "<worn around neck> ", "neck", ITEM_WEAR_NECK, TRUE, 1.0, WEAR_NECK_2, "YOU SHOULD NEVER SEE THIS MESSAGE. PLEASE REPORT.", "$n wears $p around $s neck.", "You wear $p around your neck.", TRUE, TRUE },
	{ "<worn around neck> ", "neck", ITEM_WEAR_NECK, TRUE, 1.0, NO_WEAR, "You're already wearing enough around your neck.", "$n wears $p around $s neck.", "You wear $p around your neck.", TRUE, TRUE },
	{ " <worn as clothes> ", "clothes", ITEM_WEAR_CLOTHES, TRUE, 0, NO_WEAR, "You're already wearing $p as clothes.", "$n wears $p as clothing.", "You wear $p as clothing.", TRUE, TRUE },
	{ "   <worn as armor> ", "armor", ITEM_WEAR_ARMOR, TRUE, 2.0, NO_WEAR, "You're already wearing $p as armor.", "$n wears $p as armor.", "You wear $p as armor.", TRUE, TRUE },
	{ " <worn about body> ", "body", ITEM_WEAR_ABOUT, TRUE, 1.0, NO_WEAR, "You're already wearing $p about your body.", "$n wears $p about $s body.", "You wear $p around your body.", TRUE, TRUE },
	{ "    <worn on arms> ", "arms", ITEM_WEAR_ARMS, TRUE, 1.0, NO_WEAR, "You're already wearing $p on your arms.", "$n wears $p on $s arms.", "You wear $p on your arms.", TRUE, TRUE },
	{ "  <worn on wrists> ", "wrists", ITEM_WEAR_WRISTS, TRUE, 1.0, NO_WEAR, "You're already wearing $p on your wrists.", "$n wears $p on $s wrists.", "You wear $p on your wrists.", TRUE, TRUE },
	{ "   <worn on hands> ", "hands", ITEM_WEAR_HANDS, TRUE, 1.0, NO_WEAR, "You're already wearing $p on your hands.", "$n puts $p on $s hands.", "You put $p on your hands.", TRUE, TRUE },
	{ "  <worn on finger> ", "finger", ITEM_WEAR_FINGER, TRUE, 1.0, WEAR_FINGER_L, "YOU SHOULD NEVER SEE THIS MESSAGE. PLEASE REPORT.", "$n slides $p on to $s right ring finger.", "You slide $p on to your right ring finger.", TRUE, TRUE },
	{ "  <worn on finger> ", "finger", ITEM_WEAR_FINGER, TRUE, 1.0, NO_WEAR, "You're already wearing something on both of your ring fingers.", "$n slides $p on to $s left ring finger.", "You slide $p on to your left ring finger.", TRUE, TRUE },
	{ "<worn about waist> ", "waist", ITEM_WEAR_WAIST, TRUE, 1.0, NO_WEAR, "You already have $p around your waist.", "$n wears $p around $s waist.", "You wear $p around your waist.", TRUE, TRUE },
	{ "    <worn on legs> ", "legs", ITEM_WEAR_LEGS, TRUE, 1.0, NO_WEAR, "You're already wearing $p on your legs.", "$n puts $p on $s legs.", "You put $p on your legs.", TRUE, TRUE },
	{ "    <worn on feet> ", "feet", ITEM_WEAR_FEET, TRUE, 1.0, NO_WEAR, "You're already wearing $p on your feet.", "$n wears $p on $s feet.", "You wear $p on your feet.", TRUE, TRUE },
	{ " <carried as pack> ", "pack", ITEM_WEAR_PACK, TRUE, 0.5, NO_WEAR, "You're already using $p.", "$n starts using $p.", "You start using $p.", TRUE, TRUE },
	{ "  <used as saddle> ", "saddle", ITEM_WEAR_SADDLE, TRUE, 0, NO_WEAR, "You're already using $p.", "$n start using $p.", "You start using $p.", TRUE, FALSE },
	{ "          (sheath) ", "sheath", ITEM_WEAR_WIELD, FALSE, 0, WEAR_SHEATH_2, "You've already got something sheathed.", "$n sheathes $p.", "You sheathe $p.", FALSE, TRUE },
	{ "          (sheath) ", "sheath", ITEM_WEAR_WIELD, FALSE, 0, NO_WEAR, "You've already got something sheathed.", "$n sheathes $p.", "You sheathe $p.", FALSE, TRUE },
	{ "         <wielded> ", "wield", ITEM_WEAR_WIELD, 	TRUE, 2.0, NO_WEAR, "You're already wielding $p.", "$n wields $p.", "You wield $p.", TRUE, TRUE },
	{ "          <ranged> ", "ranged", ITEM_WEAR_RANGED, TRUE, 0, NO_WEAR, "You're already using $p.", "$n uses $p.", "You use $p.", TRUE, TRUE },
	{ "            <held> ", "hold", ITEM_WEAR_HOLD, TRUE, 1.0, NO_WEAR, "You're already holding $p.", "$n grabs $p.", "You grab $p.", TRUE, TRUE },
	{ "          (shared) ", "shared", ITEM_WEAR_TAKE, FALSE, 0, NO_WEAR, "You're already sharing $p.", "$n shares $p.", "You share $p.", FALSE, FALSE }
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
	"RECIPE",
	"PORTAL",
	"*BOARD",
	"CORPSE",
	"COINS",
	"CURRENCY",
	"PAINT",
	"*MAIL",
	"WEALTH",
	"*",
	"*",
	"LIGHTER",
	"MINIPET",
	"MISSILE-WEAPON",
	"AMMO",
	"INSTRUMENT",
	"SHIELD",
	"PACK",
	"POTION",
	"POISON",
	"ARMOR",
	"BOOK",
	"\n"
};


// ITEM_WEAR_x (wear bitvector) -- also see wear_significance
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
	"UNIQUE",	// 0
	"PLANTABLE",
	"LIGHT",
	"SUPERIOR",
	"LARGE",
	"*CREATED",	// 5
	"1-USE",
	"SLOW",
	"FAST",
	"ENCHANTED",
	"JUNK",	// 10
	"CREATABLE",
	"SCALABLE",
	"TWO-HANDED",
	"BOE",
	"BOP",	// 15
	"STAFF",
	"UNCOLLECTED-LOOT",
	"*KEEP",
	"TOOL-PAN",
	"TOOL-SHOVEL",	// 20
	"!AUTOSTORE",
	"HARD-DROP",
	"GROUP-DROP",
	"GENERIC-DROP",
	"!STORE",	// 25
	"SEEDED",
	"\n"
};


// OBJ_x (extra bits), part 2 -- shown in inventory/equipment list as flags
const char *extra_bits_inv_flags[] = {
	"(unique)",	// unique
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
	"",	// no-store
	"",	// seeded
	"\n"
};


// OBJ_x (extra bits), part 3 -- the amount a flag modifies scale level by (1.0 = no mod)
const double obj_flag_scaling_bonus[] = {
	1.1,	// OBJ_UNIQUE
	1.0,	// OBJ_PLANTABLE
	1.0,	// OBJ_LIGHT
	1.6,	// OBJ_SUPERIOR
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
	1.0,	// OBJ_GENERIC_DROP
	1.0,	// OBJ_NO_STORE
	1.0,	// OBJ_SEEDED
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


// CMP_x: component types
const char *component_types[] = {
	"none",
	"adhesive",
	"bone",
	"block",
	"clay",
	"dye",		// 5
	"feathers",
	"fibers",
	"flour",
	"fruit",
	"fur",	// 10
	"gem",
	"grain",
	"handle",
	"herb",
	"leather",	// 15
	"lumber",
	"meat",
	"metal",
	"nails",
	"oil",	// 20
	"pillar",
	"rock",
	"seeds",
	"skin",
	"sapling",	// 25
	"textile",
	"vegetable",
	"rope",
	"paint",
	"\n"
};


// CMPF_x: component flags
const char *component_flags[] = {
	"animal",
	"bunch of",
	"desert",
	"fine",
	"hard",
	"large",	// 5
	"magic",
	"mundane",
	"plant",
	"poor",
	"rare",	// 10
	"raw",
	"refined",
	"single",
	"small",
	"soft",	// 15
	"temperate",
	"tropical",
	"common",
	"aquatic",
	"basic",
	"\n"
};


/* level of fullness for drink containers */
const char *fullness[] = {
	"less than half ",
	"about half ",
	"more than half ",
	""
};


// house painting (1/2)
const char *paint_colors[] = {
	"&0",	// none/normal
	"&b",	// blue
	"&r",	// red
	"&y",	// yellow
	"&g",	// green
	"&o",	// orange
	"&v",	// violet
	"&a",	// azure
	"&c",	// Cyan
	"&j",	// Jade
	"&l",	// Lime
	"&m",	// Magenta
	"&p",	// Pink
	"&t",	// Tan
	"&w",	// White
	"\n"
};


// house painting (2/2)
const char *paint_names[] = {
	"none",
	"Blue",
	"Red",
	"Yellow",
	"Green",
	"Orange",
	"Violet",
	"Azure",
	"Cyan",
	"Jade",
	"Lime",
	"Magenta",
	"Pink",
	"Tan",
	"White",
	"\n"
};


// RES_x: resource requirement types
const char *resource_types[] = {
	"object",
	"component",
	"liquid",
	"coins",
	"pool",
	"action",
	"currency",
	"\n"
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
	"craft-to-char",
	"craft-to-room",
	"wear-to-char",
	"wear-to-room",
	"remove-to-char",
	"remove-to-room",
	"longdesc",
	"longdesc-female",
	"longdesc-male",
	"\n"
};


// Weapon attack texts -- TYPE_x
struct attack_hit_type attack_hit_info[NUM_ATTACK_TYPES] = {
	// * lower numbers are better for speeds (seconds between attacks)
	// name, first-pers, 2nd-pers, noun, { fast spd, normal spd, slow spd }, WEAPON_, DAM_, disarmable
	{ "RESERVED", "hit", "hits", "hit", { 1.8, 2.0, 2.2 }, WEAPON_BLUNT, DAM_PHYSICAL, FALSE },
	{ "slash", "slash", "slashes", "slash", { 2.6, 2.8, 3.0 }, WEAPON_SHARP, DAM_PHYSICAL, TRUE },
	{ "slice", "slice", "slices", "swing", { 3.0, 3.2, 3.4 }, WEAPON_SHARP, DAM_PHYSICAL, TRUE },
	{ "jab", "jab", "jabs", "jab", { 2.6, 2.8, 3.0 }, WEAPON_SHARP, DAM_PHYSICAL, TRUE },
	{ "stab", "stab", "stabs", "stab", { 2.0, 2.2, 2.4 }, WEAPON_SHARP, DAM_PHYSICAL, TRUE },
	{ "pound", "pound", "pounds", "swing", { 3.4, 3.6, 3.8 }, WEAPON_BLUNT, DAM_PHYSICAL, TRUE },
	{ "hammer", "hammer", "hammers", "hammer", { 3.4, 3.6, 3.8 }, WEAPON_BLUNT, DAM_PHYSICAL, TRUE },
	{ "whip", "whip", "whips", "whip", { 2.8, 3.0, 3.2 }, WEAPON_BLUNT, DAM_PHYSICAL, TRUE },
	{ "pick", "jab", "jabs", "pick", { 3.4, 3.6, 3.8 }, WEAPON_SHARP, DAM_PHYSICAL, TRUE },
	{ "bite", "bite", "bites", "bite", { 2.2, 2.4, 2.6 }, WEAPON_SHARP, DAM_PHYSICAL, FALSE },
	{ "claw", "claw", "claws", "claw", { 2.4, 2.6, 2.8 }, WEAPON_SHARP, DAM_PHYSICAL, FALSE },
	{ "kick", "kick", "kicks", "kick", { 2.6, 2.8, 3.0 }, WEAPON_BLUNT, DAM_PHYSICAL, FALSE },
	{ "fire", "burn", "burns", "fire", { 2.6, 2.8, 3.0 }, WEAPON_BLUNT, DAM_FIRE, TRUE },
	{ "vampire claws", "claw", "claws", "claw", { 2.4, 2.6, 2.8 }, WEAPON_SHARP, DAM_PHYSICAL, FALSE },
	{ "crush", "crush", "crushes", "blow", { 3.6, 3.8, 4.0 }, WEAPON_BLUNT, DAM_PHYSICAL, FALSE },
	{ "hit", "hit", "hits", "hit", { 2.8, 3.0, 3.2 }, WEAPON_BLUNT, DAM_PHYSICAL, FALSE },
	{ "magic fire", "blast", "blasts", "blast", { 3.6, 3.8, 4.0 }, WEAPON_MAGIC, DAM_MAGICAL, TRUE },
	{ "lightning staff", "zap", "zaps", "staff", { 2.2, 2.5, 2.8 }, WEAPON_MAGIC, DAM_MAGICAL, TRUE },
	{ "burn staff", "burn", "burns", "staff", { 2.6, 2.9, 3.2 }, WEAPON_MAGIC, DAM_MAGICAL, TRUE },
	{ "agony staff", "agonize", "agonizes", "staff", { 3.3, 3.6, 3.9 }, WEAPON_MAGIC, DAM_MAGICAL, TRUE },
	{ "magic frost", "chill", "chills", "frost", { 4.1, 4.3, 4.5 }, WEAPON_MAGIC, DAM_MAGICAL, TRUE },
	{ "magic shock", "shock", "shocks", "shock", { 2.6, 2.8, 3.0 }, WEAPON_MAGIC, DAM_MAGICAL, TRUE },
	{ "magic light", "flash", "flashes", "light", { 2.8, 3.0, 3.2 }, WEAPON_MAGIC, DAM_MAGICAL, TRUE },
	{ "sting", "sting", "stings", "sting", { 3.6, 3.8, 4.0 }, WEAPON_SHARP, DAM_PHYSICAL, FALSE },
	{ "swipe", "swipe", "swipes", "swipe", { 3.6, 3.8, 4.0 }, WEAPON_BLUNT, DAM_PHYSICAL, FALSE },
	{ "tail swipe", "swipe", "swipes", "tail swipe", { 4.0, 4.2, 4.4 }, WEAPON_BLUNT, DAM_PHYSICAL, FALSE },
	{ "peck", "peck", "pecks", "peck", { 2.6, 2.8, 3.0 }, WEAPON_BLUNT, DAM_PHYSICAL, FALSE },
	{ "gore", "gore", "gores", "gore", { 3.9, 4.1, 4.3 }, WEAPON_BLUNT, DAM_PHYSICAL, FALSE },
	{ "mana blast", "blast", "blasts", "blast", { 2.8, 3.0, 3.2 }, WEAPON_MAGIC, DAM_MAGICAL, FALSE },
	{ "bow", "shoot", "shoots", "shot", { 2.2, 2.6, 3.2 }, WEAPON_SHARP, DAM_PHYSICAL, FALSE },
	{ "crossbow", "shoot", "shoots", "shot", { 3.7, 3.9, 4.3 }, WEAPON_SHARP, DAM_PHYSICAL, FALSE },
	{ "pistol", "shoot", "shoots", "shot", { 2.0, 2.4, 3.0 }, WEAPON_BLUNT, DAM_PHYSICAL, FALSE },
	{ "musket", "shoot", "shoots", "shot", { 3.6, 3.8, 4.2 }, WEAPON_BLUNT, DAM_PHYSICAL, FALSE },
	{ "fire breath", "breathe fire at", "breathes fire at", "fire breath", { 3.6, 3.8, 4.0 }, WEAPON_MAGIC, DAM_MAGICAL, FALSE },
};


// basic speed is the theoretical average weapon speed without wits/haste,
// and is used to apply bonus-physical/magical evenly by adjusting for speed
const double basic_speed = 4.0;	// seconds between attacks


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
	"!SECTORS",
	"!CROP",
	"!TRIGGER",
	"!ADVENTURE",
	"!ROOMTEMPLATE",
	"!GLOBAL",
	"!AUGMENT",
	"!ARCHETYPE",
	"!ABILITIES",
	"!CLASSES",
	"!SKILLS",
	"!VEHICLES",
	"!MORPHS",
	"!QUESTS",
	"!SOCIALS",
	"!FACTIONS",
	"!GENERICS",
	"!SHOPS",
	"!PROGRESS",
	"!EVENTS",
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
	"book",
	"augment",
	"archetype",
	"ability",
	"class",
	"skill",
	"vehicle",
	"morph",
	"quest",
	"social",
	"faction",
	"generic",
	"shop",
	"progression",
	"event",
	"\n"
};


 //////////////////////////////////////////////////////////////////////////////
//// PROGRESS CONSTANTS //////////////////////////////////////////////////////

// PROGRESS_x: progress types
const char *progress_types[] = {
	"UNDEFINED",
	"Community",
	"Industry",
	"Defense",
	"Prestige",
	"\n"
};


// PRG_x: progress flags
const char *progress_flags[] = {
	"IN-DEVELOPMENT",
	"PURCHASABLE",
	"SCRIPT-ONLY",
	"HIDDEN",
	"\n"
};


// PRG_PERK_x: progress perk types (should be all 1 word)
const char *progress_perk_types[] = {
	"Technology",
	"City-points",
	"Craft",
	"Max-city-size",
	"Wealth-territory-per-100",
	"Greatness-territory",
	"Workforce-cap",
	"Territory",
	"\n"
};


 //////////////////////////////////////////////////////////////////////////////
//// QUEST CONSTANTS /////////////////////////////////////////////////////////

// QST_x: quest flags
const char *quest_flags[] = {
	"IN-DEVELOPMENT",
	"REPEAT-PER-INSTANCE",
	"EXPIRES-AFTER-INSTANCE",
	"EXTRACT-TASK-OBJECTS",
	"DAILY",
	"EMPIRE-ONLY",
	"NO-GUESTS",
	"TUTORIAL",
	"GROUP-COMPLETION",
	"\n"
};


// QG_x: quest giver types
const char *quest_giver_types[] = {
	"BUILDING",	// 0
	"MOBILE",
	"OBJECT",
	"ROOM-TEMPLATE",
	"TRIGGER",
	"QUEST",	// 5
	"VEHICLE",
	"\n"
};


// QR_x: quest reward types
const char *quest_reward_types[] = {
	"BONUS-EXP",	// 0
	"COINS",
	"OBJECT",
	"SET-SKILL",
	"SKILL-EXP",
	"SKILL-LEVELS",	// 5
	"QUEST-CHAIN",
	"REPUTATION",
	"CURRENCY",
	"EVENT-POINTS",
	"\n",
};


 //////////////////////////////////////////////////////////////////////////////
//// ROOM/WORLD CONSTANTS ////////////////////////////////////////////////////

// BLD_ON_x
const char *bld_on_flags[] = {
	"water",	// 0
	"plains",
	"mountain",
	"full-forest",
	"desert",
	"river",	// 5
	"jungle",
	"not-player-made",
	"ocean",
	"oasis",
	"crops",	// 10
	"grove",
	"swamp",
	"any-forest",
	"open-building",
	"flat-terrain",	// 15
	"shallow-sea",
	"coast",
	"riverbank",
	"estuary",
	"lake",	// 20
	"base-terrain-allowed",
	"\n"
};


// BLD_x -- feel free to re-use the * flags, as they should no longer appear on anything
const char *bld_flags[] = {
	"ROOM",	// 0
	"ALLOW-MOUNTS",
	"TWO-ENTRANCES",
	"OPEN",
	"CLOSED",
	"INTERLINK",	// 5
	"HERD",
	"DEDICATE",
	"!RUINS",
	"!NPC",
	"BARRIER",	// 10
	"IN-CITY-ONLY",
	"LARGE-CITY-RADIUS",
	"!PAINT",
	"ATTACH-ROAD",
	"BURNABLE",	// 15
	"*",
	"*",
	"*",
	"*",
	"*",	// 20
	"*",
	"*",
	"*",
	"*",
	"*",	// 25
	"*",
	"*",
	"*",
	"SAIL",
	"*",	// 30
	"*",
	"*",
	"ITEM-LIMIT",
	"LONG-AUTOSTORE",
	"*",	// 35
	"*",
	"HIGH-DEPLETION",
	"*",
	"*",
	"!DELETE",	// 40
	"*",
	"NEED-BOAT",
	"LOOK-OUT",
	"2ND-TERRITORY",
	"*",	// 45
	"UPGRADED",
	"*",
	"\n"
};


// BLD_REL_x: relationships with other buildings
const char *bld_relationship_types[] = {
	"UPGRADES-TO",
	"STORES-LIKE",
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
	"NEWBIE-ONLY",
	"!NEWBIE",
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
	"trapping",
	"chop"
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
	"SHIP-MAIN",
	"SHIP-LARGE",
	"SHIP-EXTRA",
	"LAND-VEHICLE",
	"\n"
};


// EVO_x 1/3: world evolution names
const char *evo_types[] = {
	"CHOPPED-DOWN",	// 0
	"CROP-GROWS",
	"ADJACENT-ONE",
	"ADJACENT-MANY",
	"RANDOM",
	"TRENCH-START",	// 5
	"TRENCH-FULL",
	"NEAR-SECTOR",
	"PLANTS-TO",
	"MAGIC-GROWTH",
	"NOT-ADJACENT",	// 10
	"NOT-NEAR-SECTOR",
	"SPRING",
	"SUMMER",
	"AUTUMN",
	"WINTER",	// 15
	"BURNS-TO",
	"SPREADS-TO",
	"\n"
};


// EVO_x 2/3: what type of data the evolution.value uses
const int evo_val_types[NUM_EVOS] = {
	EVO_VAL_NONE,	// chopped-down
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
	EVO_VAL_NONE,	// spring
	EVO_VAL_NONE,	// summer
	EVO_VAL_NONE,	// autumn
	EVO_VAL_NONE,	// winter
	EVO_VAL_NONE,	// burns-to
	EVO_VAL_SECTOR,	// spreads-to
};


// EVO_x 3/3: evolution is over time (as opposed to triggered by an action)
bool evo_is_over_time[] = {
	FALSE,	// chopped
	FALSE,	// crop grows
	TRUE,	// adjacent-one
	TRUE,	// adjacent-many
	TRUE,	// random
	FALSE,	// trench-start
	FALSE,	// trench-full
	TRUE,	// near-sect
	FALSE,	// plants-to
	FALSE,	// magic-growth
	TRUE,	// not-adjacent
	TRUE,	// not-near-sector
	TRUE,	// spring
	TRUE,	// summer
	TRUE,	// autumn
	TRUE,	// winter
	FALSE,	// burns-to
	TRUE,	// spreads-to
};


// FNC_x: function flags (for buildings)
const char *function_flags[] = {
	"ALCHEMIST",
	"APIARY",
	"BATHS",
	"BEDROOM",
	"CARPENTER",
	"DIGGING",	// 5
	"DOCKS",
	"FORGE",
	"GLASSBLOWER",
	"GUARD-TOWER",
	"HENGE",	// 10
	"LIBRARY",
	"MAIL",
	"MILL",
	"MINE",
	"MINT",	// 15
	"PORTAL",
	"POTTER",
	"PRESS",
	"SAW",
	"SHIPYARD",	// 20
	"SMELT",
	"STABLE",
	"SUMMON-PLAYER",
	"TAILOR",
	"TANNERY",	// 25
	"TAVERN",
	"TOMB",
	"TRADING-POST",
	"VAULT",
	"WAREHOUSE",	// 30
	"DRINK-WATER",
	"COOKING-FIRE",
	"LARGER-NEARBY",
	"FISHING",
	"STORE-ALL", // 35
	"IN-CITY-ONLY",
	"\n"
};


// ISLE_x -- island flags
const char *island_bits[] = {
	"NEWBIE",
	"!AGGRO",
	"!CUSTOMIZE",
	"CONTINENT",
	"*",	// has-custom-desc (internal use only)
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
	"DARK",	// 0
	"SILENT",
	"*HAS-INSTANCE",
	"CHAMELEON",
	"*TEMPORARY",
	"!EVOLVE",	// 5
	"*UNCLAIMABLE",
	"*PUBLIC",
	"*DISMANTLING",
	"!FLY",
	"!WEATHER",	// 10
	"*IN-VEHICLE",
	"*!WORK",
	"!DISREPAIR",
	"*!DISMANTLE",
	"*INCOMPLETE",	// 15
	"!TELEPORT",
	"BRIGHT-PAINT",
	"FAKE-INSTANCE",
	"\n"
};


// ROOM_EXTRA_x
const char *room_extra_types[] = {
	"prospect empire",
	"mine amount",
	"fire remaining",
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
	"mine global vnum",
	"trench fill time",
	"trench original sector",
	"\n"
};


// used for BUILDING_RUINS_CLOSED
const char *closed_ruins_icons[NUM_RUINS_ICONS] = {
	"..&0/]",
	"&0[\\&?..",
	"&0|\\&?..",
	"..&0/|",
	".&0-&?.&0]",
	"&0[&?.&0-&?.",
	"&0[&?__&0]"
};


// used for BUILDING_RUINS_OPEN
const char *open_ruins_icons[NUM_RUINS_ICONS] = {
	".&0_i&?.",
	".&0[.&?.",
	".&0.v&?.",
	".&0/]&?.",
	".&0(\\&?.",
	".&0}\\.",
	"&0..}&?."
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
//// SHOP CONSTANTS //////////////////////////////////////////////////////////

// SHOP_x: shop flags
const char *shop_flags[] = {
	"IN-DEVELOPMENT",	// 0
	"\n"
};


 //////////////////////////////////////////////////////////////////////////////
//// SKILL CONSTANTS /////////////////////////////////////////////////////////

// DAM_x damage types
const char *damage_types[] = {
	"physical",
	"magical",
	"fire",
	"poison",
	"direct",
	"\n"
};


// DIFF_x (1/2): OLC labels for how difficult a roll is
const char *skill_check_difficulty[] = {
	"trivial (always passes)",
	"easy (always passes after 50 skill)",
	"medium (always passes at 100 skill)",
	"hard (can still fail at 100)",
	"rare (passes 10% of the time at 100)",
	"\n"
};


// DIFF_x (2/2): modifiers to your skill level before a skill check
double skill_check_difficulty_modifier[NUM_DIFF_TYPES] = {
	100,	// trivial (always passes)
	1.5,  // easy
	1,  // medium
	0.66,  // hard
	0.1  // rarely
};


// SKILLF_x: skill flags
const char *skill_flags[] = {
	"IN-DEVELOPMENT",
	"BASIC",
	"\n"
};


 //////////////////////////////////////////////////////////////////////////////
//// SOCIAL CONSTANTS ////////////////////////////////////////////////////////

// SOC_x: Social flags
const char *social_flags[] = {
	"IN-DEVELOPMENT",
	"HIDE-IF-INVIS",
	"\n"
};


// SOCM_x: social message string  { "Label", "command" }
const char *social_message_types[NUM_SOCM_MESSAGES][2] = {
	{ "No-arg to character", "n2char" },
	{ "No-arg to others", "n2other" },
	{ "Targeted to character", "t2char" },
	{ "Targeted to others", "t2other" },
	{ "Targeted to victim", "t2vict" },
	{ "Target not found", "tnotfound" },	// 5
	{ "Target-self to character", "s2char" },
	{ "Target-self to others", "s2other" }
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
	"Charmed",
	"Start-Quest",	// 20
	"Finish-Quest",
	"Player-in-Room",
	"Reboot",
	"Buy",
	"Kill",	// 25
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
	TRIG_ARG_PERCENT,	// leave-all
	NOBITS,	// charmed modifier
	NOBITS,	// start-quest
	NOBITS,	// finish-quest
	NOBITS,	// player-in-room
	NOBITS,	// reboot
	NOBITS,	// buy
	TRIG_ARG_PERCENT,	// kill
};


// OTRIG_x -- obj trigger types
const char *otrig_types[] = {
	"Global",	// 0
	"Random",
	"Command",
	"*",
	"*",
	"Timer",	// 5
	"Get",
	"Drop",
	"Give",
	"Wear",
	"*",	// 10
	"Remove",
	"*",
	"Load",
	"*",
	"Ability",	// 15
	"Leave",
	"*",
	"Consume",
	"Finish",
	"Start-Quest",	// 20
	"Finish-Quest",
	"Player-in-Room",
	"Reboot",
	"Buy",
	"Kill",	// 25
	"\n"
};

// OTRIG_x -- obj trigger argument types
const bitvector_t otrig_argument_types[] = {
	NOBITS,	// global
	TRIG_ARG_PERCENT,	// random
	TRIG_ARG_COMMAND | TRIG_ARG_OBJ_WHERE,	// command
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
	TRIG_ARG_PERCENT,	// consume
	TRIG_ARG_PERCENT,	// finish
	NOBITS,	// start-quest
	NOBITS,	// finish-quest
	NOBITS,	// player-in-room
	NOBITS,	// reboot
	TRIG_ARG_OBJ_WHERE,	// buy
	TRIG_ARG_PERCENT,	// kill
};


// VTRIG_x: vehicle trigger types
const char *vtrig_types[] = {
	"Global",
	"Random",
	"Command",
	"Speech",
	"*",	// 4
	"Destroy",
	"Greet",
	"*",	// 7
	"Entry",
	"*",	// 9
	"*",	// 10
	"*",	// 11
	"*",	// 12
	"Load",
	"*",	// 14
	"*",	// 15
	"Leave",
	"*",	// 17
	"*",	// 18
	"*",	// 19
	"Start-Quest",	// 20
	"Finish-Quest",
	"Player-in-Room",
	"Reboot",
	"Buy",
	"Kill",	// 25
	"\n"
};


// VTRIG_x: argument types for vehicle triggers
const bitvector_t vtrig_argument_types[] = {
	NOBITS,	// global
	TRIG_ARG_PERCENT,	// random
	TRIG_ARG_COMMAND,	// command
	TRIG_ARG_PHRASE_OR_WORDLIST,	// speech
	NOBITS,	// 4
	TRIG_ARG_PERCENT,	// destroy
	TRIG_ARG_PERCENT,	// greet
	NOBITS,	// 7
	TRIG_ARG_PERCENT,	// entry
	NOBITS,	// 9
	NOBITS,	// 10
	NOBITS,	// 11
	NOBITS,	// 12
	TRIG_ARG_PERCENT,	// load
	NOBITS,	// 14
	NOBITS,	// 15
	TRIG_ARG_PERCENT,	// leave
	NOBITS,	// 17
	NOBITS,	// 18
	NOBITS,	// 19
	NOBITS,	// start-quest
	NOBITS,	// finish-quest
	NOBITS,	// player-in-room
	NOBITS,	// reboot
	NOBITS,	// buy
	TRIG_ARG_PERCENT,	// kill
};


// WTRIG_x: wld trigger types
const char *wtrig_types[] = {
	"Global",	// 0
	"Random",
	"Command",
	"Speech",
	"Adventure Cleanup",
	"Zone Reset",	// 5
	"Enter",
	"Drop",
	"*",
	"*",
	"*",	// 10
	"*",
	"*",
	"Load",
	"Complete",
	"Ability",	// 15
	"Leave",
	"Door",
	"Dismantle",
	"*",
	"Start-Quest",	// 20
	"Finish-Quest",
	"Player-in-Room",
	"Reboot",
	"Buy",	// 24
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
	TRIG_ARG_PERCENT,	// load
	TRIG_ARG_PERCENT,	// complete
	TRIG_ARG_PERCENT,	// ability
	TRIG_ARG_PERCENT,	// leave
	TRIG_ARG_PERCENT,	// door
	NOBITS,	// dismantle
	NOBITS,	// 19
	NOBITS,	// start-quest
	NOBITS,	// finish-quest
	NOBITS,	// player-in-room
	NOBITS,	// reboot
	NOBITS,	// buy
};


// x_TRIGGER
const char *trig_attach_types[] = {
	"Mobile",
	"Object",
	"Room",
	"*RMT (use Room)",	// rmt_trigger -- never set on an actual trigger
	"*ADV (use Room)",	// adv_trigger -- never set on an actual trigger
	"Vehicle",
	"*BDG (use Room)",	// bdg_trigger -- actually just uses room triggers
	"*EMP",	// emp_trigger -- empires only store scripts, not triggers
	"\n"
};


// x_TRIGGER -- get typenames by attach type
const char **trig_attach_type_list[] = {
	trig_types,
	otrig_types,
	wtrig_types,
	wtrig_types,	// RMT_TRIGGER (not really used)
	wtrig_types,	// ADV_TRIGGER (not really used)
	vtrig_types,
	wtrig_types,	// BLD_TRIGGER (not really used)
	wtrig_types,	// EMP_TRIGGER (not really used)
};


// x_TRIGGER -- argument types by attach type
const bitvector_t *trig_argument_type_list[] = {
	mtrig_argument_types,	// MOB_TRIGGER
	otrig_argument_types,	// OBJ_TRIGGER
	wtrig_argument_types,	// WLD_TRIGGER
	wtrig_argument_types,	// RMT_TRIGGER (not really used)
	wtrig_argument_types,	// ADV_TRIGGER (not really used)
	vtrig_argument_types,	// VEH_TRIGGER
	wtrig_argument_types,	// BLD_TRIGGER (not really used)
	wtrig_argument_types,	// EMP_TRIGGER (not really used)
};


 //////////////////////////////////////////////////////////////////////////////
//// MISC CONSTANTS //////////////////////////////////////////////////////////

// AUTOMSG_x: automessage types
const char *automessage_types[] = {
	"one-time",
	"login",
	"repeating",
	"\n"
};


// for command-interpreting
const char *fill_words[] = {
	"in",
	"from",
	"with",
	"the",
	"on",
	"at",
	"to",
	"into",
	"of",
	"\n"
};


// GLOBAL_x types
const char *global_types[] = {
	"Mob Interactions",
	"Mine Data",
	"Newbie Gear",
	"\n"
};


// GLB_FLAG_X global flags
const char *global_flags[] = {
	"IN-DEVELOPMENT",
	"ADVENTURE-ONLY",
	"CUMULATIVE-PRC",
	"CHOOSE-LAST",
	"RARE",
	"\n"
};


// INTERACT_x, see also interact_vnum_types, interact_attach_types
const char *interact_types[] = {
	"BUTCHER",	// 0
	"SKIN",
	"SHEAR",
	"BARDE",
	"LOOT",
	"DIG",	// 5
	"FORAGE",
	"PICK",
	"HARVEST",
	"GATHER",
	"ENCOUNTER",	// 10
	"LIGHT",
	"PICKPOCKET",
	"MINE",
	"COMBINE",
	"SEPARATE",	// 15
	"SCRAPE",
	"SAW",
	"TAN",
	"CHIP",
	"CHOP",	// 20
	"FISH",
	"PAN",
	"QUARRY",
	"TAME",
	"SEED",	// 25
	"\n"
};


// INTERACT_RESTRICT_x: types of interaction restrictions
const char *interact_restriction_types[] = {
	"ability",
	"ptech",
	"tech",
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
	TYPE_MOB,	// pickpocket
	TYPE_MINE_DATA,	// mine
	TYPE_OBJ,	// combine
	TYPE_OBJ,	// separate
	TYPE_OBJ,	// scrape
	TYPE_OBJ,	// saw
	TYPE_OBJ,	// tan
	TYPE_OBJ,	// chip
	TYPE_ROOM,	// chop
	TYPE_ROOM,	// fish
	TYPE_ROOM,	// pan
	TYPE_ROOM,	// quarry
	TYPE_MOB,	// tame
	TYPE_OBJ,	// seed
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
	TYPE_OBJ,
	TYPE_OBJ,	// mine
	TYPE_OBJ,	// combine
	TYPE_OBJ,	// separate
	TYPE_OBJ,	// scrape
	TYPE_OBJ,	// saw
	TYPE_OBJ,	// tan
	TYPE_OBJ,	// chip
	TYPE_OBJ,	// chop
	TYPE_OBJ,	// fish
	TYPE_OBJ,	// pan
	TYPE_OBJ,	// quarry
	TYPE_MOB,	// tame
	TYPE_OBJ,	// seed
};


// MORPHF_x
const char *morph_flags[] = {
	"IN-DEVELOPMENT",
	"SCRIPT-ONLY",
	"ANIMAL",
	"VAMPIRE-ONLY",
	"TEMPERATE-AFFINITY",
	"ARID-AFFINITY",	// 5
	"TROPICAL-AFFINITY",
	"CHECK-SOLO",
	"!SLEEP",
	"GENDER-NEUTRAL",
	"CONSUME-OBJ",	// 10
	"!FASTMORPH",
	"!MORPH-MESSAGE",
	"\n"
};


// REQ_x (1/3): types of requirements (e.g. quest tasks)
const char *requirement_types[] = {
	"COMPLETED-QUEST",	// 0
	"GET-COMPONENT",
	"GET-OBJECT",
	"KILL-MOB",
	"KILL-MOB-FLAGGED",
	"NOT-COMPLETED-QUEST",	// 5
	"NOT-ON-QUEST",
	"OWN-BUILDING",
	"OWN-VEHICLE",
	"SKILL-LEVEL-OVER",
	"SKILL-LEVEL-UNDER",	// 10
	"TRIGGERED",
	"VISIT-BUILDING",
	"VISIT-ROOM-TEMPLATE",
	"VISIT-SECTOR",
	"HAVE-ABILITY",	// 15
	"REP-OVER",
	"REP-UNDER",
	"WEARING",
	"WEARING-OR-HAS",
	"GET-CURRENCY",	// 20
	"GET-COINS",
	"CAN-GAIN-SKILL",
	"CROP-VARIETY",
	"OWN-HOMES",
	"OWN-SECTOR",	// 25
	"OWN-BUILDING-FUNCTION",
	"OWN-VEHICLE-FLAGGED",
	"EMPIRE-WEALTH",
	"EMPIRE-FAME",
	"EMPIRE-GREATNESS",	// 30
	"DIPLOMACY",
	"HAVE-CITY",
	"EMPIRE-MILITARY",
	"EMPIRE-PRODUCED-OBJECT",
	"EMPIRE-PRODUCED-COMPONENT",	// 35
	"EVENT-RUNNING",
	"EVENT-NOT-RUNNING",
	"\n",
};


// REQ_x (2/3): requirement types that take a numeric arg
const bool requirement_amt_type[] = {
	REQ_AMT_NONE,	// completed quest
	REQ_AMT_NUMBER,	// get component
	REQ_AMT_NUMBER,	// get object
	REQ_AMT_NUMBER,	// kill mob
	REQ_AMT_NUMBER,	// kill mob flagged
	REQ_AMT_NONE,	// not completed quest
	REQ_AMT_NONE,	// not on quest
	REQ_AMT_NUMBER,	// own building
	REQ_AMT_NUMBER,	// own vehicle
	REQ_AMT_THRESHOLD,	// skill over
	REQ_AMT_THRESHOLD,	// skill under
	REQ_AMT_NONE,	// triggered
	REQ_AMT_NONE,	// visit building
	REQ_AMT_NONE,	// visit rmt
	REQ_AMT_NONE,	// visit sect
	REQ_AMT_NONE,	// have ability
	REQ_AMT_REPUTATION,	// faction-over
	REQ_AMT_REPUTATION,	// faction-under
	REQ_AMT_NONE,	// wearing
	REQ_AMT_NONE,	// wearing-or-has
	REQ_AMT_NUMBER,	// get currency
	REQ_AMT_NUMBER,	// get coins
	REQ_AMT_NONE,	// can gain skill
	REQ_AMT_NUMBER,	// crop variety
	REQ_AMT_NUMBER,	// own homes
	REQ_AMT_NUMBER,	// own sector
	REQ_AMT_NUMBER,	// own building function
	REQ_AMT_NUMBER,	// own vehicle flagged
	REQ_AMT_NUMBER,	// empire wealth
	REQ_AMT_NUMBER,	// empire fame
	REQ_AMT_NUMBER,	// empire greatness
	REQ_AMT_NUMBER,	// diplomacy
	REQ_AMT_NUMBER,	// have city
	REQ_AMT_NUMBER,	// empire military
	REQ_AMT_NUMBER,	// empire produced object
	REQ_AMT_NUMBER,	// empire produced component
	REQ_AMT_NONE,	// event running
	REQ_AMT_NONE,	// event not running
};


// REQ_x (3/3): types that require a quest tracker (can't be determined in realtime)
const bool requirement_needs_tracker[] = {
	FALSE,	// completed quest
	FALSE,	// get component
	FALSE,	// get object
	TRUE,	// kill mob
	TRUE,	// kill mob flagged
	FALSE,	// not completed quest
	FALSE,	// not on quest
	FALSE,	// own building
	FALSE,	// own vehicle
	FALSE,	// skill over
	FALSE,	// skill under
	TRUE,	// triggered
	TRUE,	// visit building
	TRUE,	// visit rmt
	TRUE,	// visit sect
	FALSE,	// have ability
	FALSE,	// faction-over
	FALSE,	// faction-under
	FALSE,	// wearing
	FALSE,	// wearing-or-has
	FALSE,	// get currency
	FALSE,	// get coins
	FALSE,	// can gain skill
	FALSE,	// crop variety
	FALSE,	// own homes
	FALSE,	// own sector
	FALSE,	// own building function
	FALSE,	// own vehicle flagged
	FALSE,	// empire wealth
	FALSE,	// empire fame
	FALSE,	// empire greatness
	FALSE,	// diplomacy
	FALSE,	// have city
	FALSE,	// empire military
	FALSE,	// empire produced object
	FALSE,	// empire produced component
	FALSE,	// event running
	FALSE,	// event not running
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
	"of",
	"someone",
	"something",
	"misc",
	"miscellaneous",
	"other",
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


// VEH_x: Vehicle flags
const char *vehicle_flags[] = {
	"*INCOMPLETE",
	"DRIVING",
	"SAILING",
	"FLYING",
	"ALLOW-ROUGH",
	"SIT",	// 5
	"IN",
	"BURNABLE",
	"CONTAINER",
	"SHIPPING",
	"CUSTOMIZABLE",	// 10
	"DRAGGABLE",
	"!BUILDING",
	"CAN-PORTAL",
	"LEADABLE",
	"CARRY-VEHICLES",	// 15
	"CARRY-MOBS",
	"SIEGE-WEAPONS",
	"ON-FIRE",
	"!LOAD-ONTO-VEHICLE",
	"VISIBLE-IN-DARK",	// 20
	"!CLAIM",
	"\n"
};

// VSPEED_x: Vehicle speed classes
const char *vehicle_speed_types[] = {
	"Very Slow",
	"Slow",
	"Normal",
	"Fast",
	"Very Fast",
	"\n"
};


// WAIT_x: Wait types for the command_lag() function.
const char *wait_types[] = {
	"NONE",	// 0
	"ABILITY",
	"COMBAT-ABILITY",
	"COMBAT-SPELL",
	"MOVEMENT",
	"SPELL",	// 5
	"OTHER",
	"\n"
};
