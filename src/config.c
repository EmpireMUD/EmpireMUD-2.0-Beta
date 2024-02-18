/* ************************************************************************
*   File: config.c                                        EmpireMUD 2.0b5 *
*  Usage: Configuration of various aspects of CircleMUD operation         *
*                                                                         *
*  EmpireMUD code base by Paul Clarke, (C) 2000-2024                      *
*  All rights reserved.  See license.doc for complete information.        *
*                                                                         *
*  EmpireMUD based upon CircleMUD 3.0, bpl 17, by Jeremy Elson.           *
*  CircleMUD (C) 1993, 94 by the Trustees of the Johns Hopkins University *
*  CircleMUD is based on DikuMUD, Copyright (C) 1990, 1991.               *
************************************************************************ */

#define __CONFIG_C__

#include "conf.h"
#include "sysdep.h"

#include "structs.h"
#include "utils.h"
#include "comm.h"
#include "db.h"
#include "interpreter.h"
#include "olc.h"

// TODO convert more of these configs to the in-game config system
// TODO add string-array config type (default channels, could have add/remove)
// TODO add double-array (empire score levels)


/**
* Contents:
*   EmpireMUD Configs
*   Empire Configs
*   Immortal Configs
*   Message Configs
*   Operation Options
*   Player Configs
*   Config System: Data
*   Config System: Editors
*   Config System: Custom Editors
*   Config System: Show Funcs
*   Config System: Handlers
*   Config System: I/O
*   Config System: Utils
*   Config System: Setup
*   Config System: Command
*/

// external funcs
void ensure_storage_timers(any_vnum only_vnum);
void ensure_home_storage_timers(char_data *ch, any_vnum only_vnum);
void set_inherent_ptech(int ptech);

// locals
#define deprecated_config(set, name, type)	{ init_config((set), (name), (type), "deprecated: do not set"); init_config_flags((name), CONF_FLAG_DEPRECATED); }


 //////////////////////////////////////////////////////////////////////////////
//// EMPIREMUD CONFIGS ///////////////////////////////////////////////////////

// slash-channels a player joins automatically upon creation (\n-terminated list)
const char *default_channels[] = { "newbie", "ooc", "recruit", "trade", "grats", "death", "progress", "events", "\n" };


// list of promo funcs
PROMO_APPLY(promo_countdemonet);
PROMO_APPLY(promo_facebook);
PROMO_APPLY(promo_skillups);


// list of active promo codes: CAUTION: You should only add to the end, not
// rearrange this list, as promo codes are stored by number.
struct promo_code_list promo_codes[] = {
	// add promo codes here to track ad campaigns
	// { "code", expired, func },
	
	// do not change the order
	{ "none",	TRUE,	NULL },	// default
	{ "skillups", FALSE, promo_skillups },
	{ "countdemonet", FALSE, promo_countdemonet },
	{ "facebook", FALSE, promo_facebook },
	
	// last
	{ "\n", FALSE, NULL }
};


 //////////////////////////////////////////////////////////////////////////////
//// EMPIRE CONFIGS //////////////////////////////////////////////////////////

// score configs: empires get a score point in a category if they are over (avg x score_level[n])
// if there are 10 entries (before the -1), that means up to 10 points available
const double score_levels[] = { 0.0625, 0.125, 0.25, 0.5, 1.0, 1.5, 2.0, 4.0, 8.0, 16.0, -1 };	// terminate with -1

// techs that only work if they're on the same island (these should not be techs that come from player abilities)
const int techs_requiring_same_island[] = { TECH_SEAPORT, NOTHING };	// terminate with a NOTHING


 //////////////////////////////////////////////////////////////////////////////
//// IMMORTAL CONFIGS ////////////////////////////////////////////////////////

// files that can be loaded by imms, for do_file in act.immortal.c
struct file_lookup_struct file_lookup[] = {
	{ "none", LVL_TOP+1, "Does Nothing" },
	{ "bug", LVL_ASST, "../lib/misc/bugs" },
	{ "typo", LVL_ASST, "../lib/misc/typos" },
	{ "ideas", LVL_ASST, "../lib/misc/ideas" },
	{ "xnames", LVL_CIMPL, "../lib/misc/xnames" },
	{ "syslog", LVL_CIMPL, "../syslog" },
	{ "oldsyslog", LVL_CIMPL, "../log/syslog.old" },
	{ "crash", LVL_CIMPL, "../syslog.CRASH" },
	{ "config", LVL_ASST, "../log/config" },
	{ "abuse", LVL_CIMPL, "../log/abuse" },
	{ "ban", LVL_ASST, "../log/ban" },
	{ "godcmds", LVL_CIMPL, "../log/godcmds" },
	{ "validation", LVL_ASST, "../log/validation" },
	{ "death", LVL_START_IMM, "../log/rip" },
	{ "badpw", LVL_START_IMM, "../log/badpw" },
	{ "delete", LVL_ASST, "../log/delete" },
	{ "newplayers", LVL_START_IMM, "../log/newplayers" },
	{ "syserr", LVL_CIMPL, "../log/syserr" },
	{ "level", LVL_ASST, "../log/levels" },
	{ "olc", LVL_ASST, "../log/olc" },
	{ "diplomacy", LVL_ASST, "../log/diplomacy" },
	{ "scripterr", LVL_ASST, "../log/scripterr" },

	// last
	{ "\n", 0, "\n" }
};


// TEXT_FILE_x: text file loading (and optional editing via do_tedit in act.immortal.c)
struct text_file_data_type text_file_data[NUM_TEXT_FILE_STRINGS] = {
	// name, filename, can-edit, level-to-edit, max-size
	{ "credits", LIB_TEXT"credits", TRUE, LVL_CIMPL, MAX_STRING_LENGTH },
	{ "godlist", LIB_TEXT"godlist", FALSE, 0, 0 },
	{ "handbook", LIB_TEXT"handbook", TRUE, LVL_CIMPL, MAX_STRING_LENGTH },
	{ "helpscreen", LIB_TEXT_HELP"screen", TRUE, LVL_GOD, MAX_STRING_LENGTH },
	{ "helpscreenreader", LIB_TEXT_HELP"screen-screenreader", TRUE, LVL_GOD, MAX_STRING_LENGTH },
	{ "imotd", LIB_TEXT"imotd", TRUE, LVL_GOD, MAX_MOTD_LENGTH },
	{ "info", LIB_TEXT"info", TRUE, LVL_GOD, MAX_STRING_LENGTH },
	{ "motd", LIB_TEXT"motd", TRUE, LVL_GOD, MAX_MOTD_LENGTH },
	{ "news", LIB_TEXT"news", TRUE, LVL_GOD, MAX_STRING_LENGTH },
	{ "policy", LIB_TEXT"policies", TRUE, LVL_CIMPL, MAX_STRING_LENGTH },
	{ "shortcredits", LIB_TEXT"credits.short", TRUE, LVL_GOD, MAX_STRING_LENGTH },
	{ "wizlist", LIB_TEXT"wizlist", FALSE, 0, 0 },
};


 //////////////////////////////////////////////////////////////////////////////
//// OPERATION OPTIONS ///////////////////////////////////////////////////////

/**
* Core configs:
* Some configurations can't be done in the config system because they are
* needed before it boots up.
*/
ush_int DFLT_PORT = 4000;	// default port (WARNING: autorun overrides this)
const char *DFLT_DIR = "lib";	// default directory to use as data directory
const char *LOGNAME = NULL;	// using NULL outputs to stderr, otherwise outputs to a file
int max_playing = 300;	// maximum number of players allowed before game starts to turn people away


// if you have a player whose connection causes lag when they first connect,
// adding their IPv4 address here will prevent the mud from doing a nameserver
// lookup. This will do partial-matching if you omit the end of the IP address.
// For example, "192.168." will match anything starting with that.
// Note: as of b5.35, IPs are automatically listed after resolving slowly once
// per uptime. -paul
const char *slow_nameserver_ips[] = {
	"\n"	// put this last
};


/**
* This is a list of known hosts that won't pass through a user's real IP. Some
* players use this to violate the game's multiplayer rules. If you have the
* 'restrict_anonymous_hosts' config turned on (default), characters from these
* hosts won't auto-approve and characters who are approved cannot play from
* these hosts.
*/
const char *anonymous_public_hosts[] = {
	"mudconnector.com",
	"ec2-34-228-95-236.compute-1.amazonaws.com",	// mudportal.com
	"ec2-3-128-186-25.us-east-2.compute.amazonaws.com",	// mudverse
	"107.170.209.113",	// mudslinger
	"\n"
};


 //////////////////////////////////////////////////////////////////////////////
//// PLAYER CONFIGS //////////////////////////////////////////////////////////

// base energy pools: health, move, mana, blood
const int base_player_pools[NUM_POOLS] = { 50, 100, 50, 100 };

// attributes that can't go below 1 (terminate list with NOTHING)
const int primary_attributes[] = { STRENGTH, CHARISMA, INTELLIGENCE, NOTHING };

// skill levels at which you gain an ability
int master_ability_levels[] = { 1, 5, 10, 15, 20, 25, 30, 40, 50, 60, 70, 75, 75, 80, 90, 100, -1 };

// universal wait (command lag) after abilities with cooldowns
const int universal_wait = 1.25 RL_SEC;

// lore cleanup -- terminate the list with a -1
const int remove_lore_types[] = { LORE_PLAYER_KILL, LORE_PLAYER_DEATH, LORE_TOWER_DEATH, -1 };

// used in several places to represent the lowest to-hit a player could have
const int base_hit_chance = 50;

// how much hit/dodge 1 point of Dexterity gives
const double hit_per_dex = 5.0;

// rounds up when scaling mobs/gear if this is > 1; ignored if 0 or 1
const int round_level_scaling_to_nearest = 5;

// Books must contain one of these terms (prevents abuse like naming the book "a gold bar")
const char *book_name_list[] = {
	"book",
	"booklet",
	"brochure",
	"codex",
	"compendium",
	"diary",
	"journal",
	"magazine",
	"manual",
	"novel",
	"pamphlet",
	"paper",
	"scroll",
	"tablet",
	"text",
	"tome",
	"\n"
};


/**
* Any player technologies that are 'inherent' -- that is, don't require an
* ability to use them, can be configured here. All players will have access to
* these.
*/
void init_inherent_player_techs(void) {
	// these default 'inherent' techs can be removed if you want to add an
	// ability for these things
	set_inherent_ptech(PTECH_CALENDAR);
	set_inherent_ptech(PTECH_CLOCK);
	set_inherent_ptech(PTECH_CUSTOMIZE_VEHICLE);
}


 //////////////////////////////////////////////////////////////////////////////
//// CONFIG SYSTEM: DATA /////////////////////////////////////////////////////

// CONFIG_x: groupings for config command
#define CONFIG_GAME  0
#define CONFIG_ACTIONS  1
#define CONFIG_CITY  2
#define CONFIG_EMPIRE  3
#define CONFIG_ITEMS  4
#define CONFIG_MOBS  5
#define CONFIG_OTHER  6
#define CONFIG_PLAYERS  7
#define CONFIG_SKILLS  8
#define CONFIG_SYSTEM  9
#define CONFIG_TRADE  10
#define CONFIG_WAR  11
#define CONFIG_WORLD  12
#define CONFIG_APPROVAL  13
#define CONFIG_MAIL  14


// data types of configs (search CONFTYPE_x for places to configure)
#define CONFTYPE_RESERVED  0	// 0 will indicate an unconfigured type
#define CONFTYPE_BITVECTOR  1
#define CONFTYPE_BOOL  2
#define CONFTYPE_DOUBLE  3
#define CONFTYPE_INT  4
#define CONFTYPE_INT_ARRAY  5	// TODO: this is not really supported yet
#define CONFTYPE_SHORT_STRING  6 // max length ~ 128
#define CONFTYPE_LONG_STRING  7	// paragraph length
#define CONFTYPE_TYPE  8	// shows a list from a const char array


// CONFIG_x groupings for the config command
const char *config_groups[] = {
	"Game",
	"Actions",
	"City",
	"Empire",
	"Items",
	"Mobs",
	"Other",
	"Players",
	"Skills",
	"System",
	"Trade",
	"War",
	"World",
	"Approval",
	"Mail",
	"\n"
};


// CONFTYPE_x data types for configs
const char *config_types[] = {
	"RESERVED",
	"bitvector",
	"bool",
	"double",
	"int",
	"int array",
	"short string",
	"long string",
	"\n"
};


// LASTNAME_x: config players lastname_mode: determines how players get last names
const char *lastname_modes[] = {
	"set-at-creation",
	"change-any-time",
	"choose-from-list",
	"\n"
};


// PK_x: Player killing options for config_get_bitvector("pk_mode")
const char *pk_modes[] = {
	"OPEN",		// 0
	"WAR",
	"TRESPASSERS",
	"DISTRUST",
	"EMPIRE-OFFENSES",
	"PERSONAL-OFFENSES",	// 5
	"\n"
};


// WHO_LIST_SORT_x: config game who_list_sort [type]
const char *who_list_sort_types[] = {
	"role-then-level",
	"level",
	"\n"
};


struct config_type *config_table = NULL;	// hash table of configs


 //////////////////////////////////////////////////////////////////////////////
//// CONFIG SYSTEM: HANDLERS /////////////////////////////////////////////////

/**
* Load a config's whole entry by key. You should use the type functions
* config_get_int(), etc. for loading the data itself.
*
* Note that when exact=TRUE, this function does a hash lookup and is much
* faster than exact=FALSE, which checks the whole table for abbrevs.
*
* @param char *key The config key.
* @param bool exact If TRUE, does not allow abbrevs.
* @return struct config_type* The config, if it exists.
*/
struct config_type *get_config_by_key(char *key, bool exact) {
	struct config_type *cnf;
	
	if (exact) {
		HASH_FIND_STR(config_table, key, cnf);
		return cnf;
	}
	else {
		struct config_type *next_cnf, *abbrev = NULL;
		
		HASH_ITER(hh, config_table, cnf, next_cnf) {
			if (!str_cmp(key, cnf->key)) {
				return cnf;	// exact match
			}
			else if (!abbrev && is_abbrev(key, cnf->key)) {
				abbrev = cnf;
			}
		}
		
		return abbrev;	// if any (may be NULL)
	}
}


/**
* Load a global config as a bitvector.
*
* @param char *key The one-word string key for the config system.
* @return bitvector_t The value associated with that key (default: NOBITS).
*/
bitvector_t config_get_bitvector(char *key) {
	struct config_type *cnf;
	
	if (!key) {
		log("SYSERR: config_get_bitvector called with no key.");
		return NOBITS;
	}
	
	// maybe?
	cnf = get_config_by_key(key, TRUE);
	
	if (!cnf) {
		syslog(SYS_ERROR, LVL_START_IMM, TRUE, "SYSERR: config_get_bitvector called with invalid key '%s'", key);
		return NOBITS;
	}
	if (cnf->type != CONFTYPE_BITVECTOR) {
		syslog(SYS_ERROR, LVL_START_IMM, TRUE, "SYSERR: config_get_bitvector with non-bitvector key '%s'", key);
		return NOBITS;
	}
	
	// valid data!
	return cnf->data.bitvector_val;
}


/**
* Load a global config as an bool.
*
* @param char *key The one-word string key for the config system.
* @return bool The value associated with that key (default: FALSE).
*/
bool config_get_bool(char *key) {
	struct config_type *cnf;
	
	if (!key) {
		log("SYSERR: config_get_bool called with no key.");
		return FALSE;
	}
	
	// maybe?
	cnf = get_config_by_key(key, TRUE);
	
	if (!cnf) {
		syslog(SYS_ERROR, LVL_START_IMM, TRUE, "SYSERR: config_get_bool called with invalid key '%s'", key);
		return FALSE;
	}
	if (cnf->type != CONFTYPE_BOOL) {
		syslog(SYS_ERROR, LVL_START_IMM, TRUE, "SYSERR: config_get_bool with non-int key '%s'", key);
		return FALSE;
	}
	
	// valid data!
	return cnf->data.bool_val;
}


/**
* Load a global config as a double.
*
* @param char *key The one-word string key for the config system.
* @return double The value associated with that key (default: 1.0).
*/
double config_get_double(char *key) {
	struct config_type *cnf;
	
	if (!key) {
		log("SYSERR: config_get_double called with no key.");
		return 1.0;
	}
	
	// maybe?
	cnf = get_config_by_key(key, TRUE);
	
	if (!cnf) {
		syslog(SYS_ERROR, LVL_START_IMM, TRUE, "SYSERR: config_get_double called with invalid key '%s'", key);
		return 1.0;
	}
	if (cnf->type != CONFTYPE_DOUBLE) {
		syslog(SYS_ERROR, LVL_START_IMM, TRUE, "SYSERR: config_get_double with non-double key '%s'", key);
		return 1.0;
	}
	
	// valid data!
	return cnf->data.double_val;
}


/**
* Load a global config as an int.
*
* @param char *key The one-word string key for the config system.
* @return int The value associated with that key (default: 1).
*/
int config_get_int(char *key) {
	struct config_type *cnf;
	
	if (!key) {
		log("SYSERR: config_get_int called with no key.");
		return 1;
	}
	
	// maybe?
	cnf = get_config_by_key(key, TRUE);
	
	if (!cnf) {
		syslog(SYS_ERROR, LVL_START_IMM, TRUE, "SYSERR: config_get_int called with invalid key '%s'", key);
		return 1;
	}
	if (cnf->type != CONFTYPE_INT) {
		syslog(SYS_ERROR, LVL_START_IMM, TRUE, "SYSERR: config_get_int with non-int key '%s'", key);
		return 1;
	}
	
	// valid data!
	return cnf->data.int_val;
}


/**
* Load a global config as an int array.
*
* @param char *key The one-word string key for the config system.
* @param int *array_size A bound var to store the size of the array.
* @return int* The array of ints associated with that key (default: { -1 }).
*/
int *config_get_int_array(char *key, int *array_size) {
	static int default_array[] = { NOTHING };
	struct config_type *cnf;
	
	*array_size = 1;	// default
	
	if (!key) {
		log("SYSERR: config_get_int_array called with no key.");
		return default_array;
	}
	
	// maybe?
	cnf = get_config_by_key(key, TRUE);
	
	if (!cnf) {
		syslog(SYS_ERROR, LVL_START_IMM, TRUE, "SYSERR: config_get_int_array called with invalid key '%s'", key);
		return default_array;
	}
	if (cnf->type != CONFTYPE_INT_ARRAY) {
		syslog(SYS_ERROR, LVL_START_IMM, TRUE, "SYSERR: config_get_int_array with non-int-array key '%s'", key);
		return default_array;
	}
	
	// valid data!
	*array_size = cnf->data_size;
	return cnf->data.int_array;
}


/**
* Load a global config as an string.
*
* @param char *key The one-word string key for the config system.
* @return char* A pointer to the string associated with that key (default: "").
*/
const char *config_get_string(char *key) {
	struct config_type *cnf;
	
	static char *default_string = "";
	
	if (!key) {
		log("SYSERR: config_get_string called with no key.");
		return default_string;
	}
	
	// maybe?
	cnf = get_config_by_key(key, TRUE);
	
	if (!cnf) {
		syslog(SYS_ERROR, LVL_START_IMM, TRUE, "SYSERR: config_get_string called with invalid key '%s'", key);
		return default_string;
	}
	if (cnf->type != CONFTYPE_SHORT_STRING && cnf->type != CONFTYPE_LONG_STRING) {
		syslog(SYS_ERROR, LVL_START_IMM, TRUE, "SYSERR: config_get_string with non-string key '%s'", key);
		return default_string;
	}
	
	// valid data!
	return cnf->data.string_val ? cnf->data.string_val : default_string;
}


/**
* Load a global config as an int, from a 'type' type.
*
* @param char *key The one-word string key for the config system.
* @return int The value associated with that key (default: 0).
*/
int config_get_type(char *key) {
	struct config_type *cnf;
	
	if (!key) {
		log("SYSERR: config_get_type called with no key.");
		return 0;
	}
	
	// maybe?
	cnf = get_config_by_key(key, TRUE);
	
	if (!cnf) {
		syslog(SYS_ERROR, LVL_START_IMM, TRUE, "SYSERR: config_get_type called with invalid key '%s'", key);
		return 0;
	}
	if (cnf->type != CONFTYPE_TYPE) {
		syslog(SYS_ERROR, LVL_START_IMM, TRUE, "SYSERR: config_get_type with non-type key '%s'", key);
		return 0;
	}
	
	// valid data!
	return cnf->data.int_val;
}


 //////////////////////////////////////////////////////////////////////////////
//// CONFIG SYSTEM: EDITORS //////////////////////////////////////////////////

// basic handler for attack types
CONFIG_HANDLER(config_edit_attack) {
	attack_message_data *old_amd, *new_amd;
	int input;
	
	// basic sanitation
	if (!ch || !config) {
		log("SYSERR: config_edit_attack called without %s", ch ? "config" : "ch");
		msg_to_char(ch, "Error editing type.\r\n");
		return;
	}
	if (config->type != CONFTYPE_INT) {
		log("SYSERR: config_edit_attack called on non-int key %s", config->key);
		msg_to_char(ch, "Error editing type.\r\n");
		return;
	}
	
	if (sscanf(argument, "%d", &input) != 1) {
		msg_to_char(ch, "Invalid argument '%s', expecting a number.\r\n", argument);
		return;
	}
	
	if (!(new_amd = real_attack_message(input))) {
		msg_to_char(ch, "Invalid attack message vnum '%s'.\r\n", argument);
		return;
	}
	
	old_amd = real_attack_message(config->data.int_val);
	config->data.int_val = input;
	
	if (old_amd != new_amd) {
		save_config_system();
		syslog(SYS_CONFIG, GET_INVIS_LEV(ch), TRUE, "CONFIG: %s set %s to %s, from %s", GET_NAME(ch), config->key, ATTACK_NAME(new_amd), old_amd ? ATTACK_NAME(old_amd) : "UNKNOWN");
		msg_to_char(ch, "%s: set to %s, from %s.\r\n", config->key, ATTACK_NAME(new_amd), old_amd ? ATTACK_NAME(old_amd) : "UNKNOWN");
	}
	else {
		send_config_msg(ch, "ok_string");
	}
}


// Custom config handler for editing a bitvector list, for CONFTYPE_BITVECTOR.
CONFIG_HANDLER(config_edit_bitvector) {
	char add_buf[MAX_STRING_LENGTH], remove_buf[MAX_STRING_LENGTH], buf[MAX_STRING_LENGTH];
	bitvector_t old, new, removed, added;
	int size;
	
	// basic sanitation
	if (!ch || !config) {
		log("SYSERR: config_edit_bitvector called without %s", ch ? "config" : "ch");
		msg_to_char(ch, "Error editing type.\r\n");
		return;
	}
	if (config->type != CONFTYPE_BITVECTOR) {
		log("SYSERR: config_edit_bitvector called on non-bitvector key %s", config->key);
		msg_to_char(ch, "Error editing type.\r\n");
		return;
	}
	if (!config->custom_data) {
		log("SYSERR: config_edit_bitvector called without custom data for key %s", config->key);
		msg_to_char(ch, "Error editing type.\r\n");
		return;
	}
		
	// process (olc processor sends all messages)
	old = config->data.bitvector_val;
	snprintf(buf, sizeof(buf), "config %s", config->key);
	new = olc_process_flag(ch, argument, config->key, buf, (const char **)config->custom_data, old);
	
	// no change
	if (old == new) {
		return;
	}
	
	// update
	config->data.bitvector_val = new;
	
	// diffs
	removed = old & ~new;	// anything in old but not in new
	added = new & ~old;	// anything in new but not in old
	
	// build log
	size = snprintf(buf, sizeof(buf), "CONFIG: %s updated %s: ", GET_NAME(ch), config->key);
	if (added) {
		sprintbit(added, (const char **)config->custom_data, add_buf, TRUE);
		size += snprintf(buf + size, sizeof(buf) - size, "added %s", add_buf);
	}
	if (removed) {
		sprintbit(removed, (const char **)config->custom_data, remove_buf, TRUE);
		size += snprintf(buf + size, sizeof(buf) - size, "%sremoved %s", added ? "/ " : "", remove_buf);
	}

	syslog(SYS_CONFIG, GET_INVIS_LEV(ch), TRUE, "%s", buf);
	save_config_system();
}


// basic handler for bool
CONFIG_HANDLER(config_edit_bool) {
	bool input, old;
	
	// basic sanitation
	if (!ch || !config) {
		log("SYSERR: config_edit_bool called without %s", ch ? "config" : "ch");
		msg_to_char(ch, "Error editing type.\r\n");
		return;
	}
	
	if (!str_cmp(argument, "yes") || !str_cmp(argument, "true") || !str_cmp(argument, "on")) {
		input = TRUE;
	}
	else if (!str_cmp(argument, "no") || !str_cmp(argument, "false") || !str_cmp(argument, "off")) {
		input = FALSE;
	}
	else {
		msg_to_char(ch, "Invalid argument '%s', expecting true/false.\r\n", argument);
		return;
	}
	
	old = config->data.bool_val;
	config->data.bool_val = input;
	
	if (old != input) {
		save_config_system();
		syslog(SYS_CONFIG, GET_INVIS_LEV(ch), TRUE, "CONFIG: %s set %s to %s, from %s", GET_NAME(ch), config->key, config->data.bool_val ? "true" : "false", old ? "true" : "false");
		msg_to_char(ch, "%s: set to %s, from %s.\r\n", config->key, config->data.bool_val ? "true" : "false", old ? "true" : "false");
	}
	else {
		send_config_msg(ch, "ok_string");
	}
}


// basic handler for building
CONFIG_HANDLER(config_edit_building) {
	bld_data *old_bld, *new_bld;
	int input;
	
	// basic sanitation
	if (!ch || !config) {
		log("SYSERR: config_edit_building called without %s", ch ? "config" : "ch");
		msg_to_char(ch, "Error editing type.\r\n");
		return;
	}
	if (config->type != CONFTYPE_INT) {
		log("SYSERR: config_edit_building called on non-int key %s", config->key);
		msg_to_char(ch, "Error editing type.\r\n");
		return;
	}
	
	if (sscanf(argument, "%d", &input) != 1) {
		msg_to_char(ch, "Invalid argument '%s', expecting a number.\r\n", argument);
		return;
	}
	
	if (!(new_bld = building_proto(input))) {
		msg_to_char(ch, "Invalid building vnum '%s'.\r\n", argument);
		return;
	}
	
	old_bld = building_proto(config->data.int_val);
	config->data.int_val = input;
	
	if (old_bld != new_bld) {
		save_config_system();
		syslog(SYS_CONFIG, GET_INVIS_LEV(ch), TRUE, "CONFIG: %s set %s to %s, from %s", GET_NAME(ch), config->key, GET_BLD_NAME(new_bld), old_bld ? GET_BLD_NAME(old_bld) : "UNKNOWN");
		msg_to_char(ch, "%s: set to %s, from %s.\r\n", config->key, GET_BLD_NAME(new_bld), old_bld ? GET_BLD_NAME(old_bld) : "UNKNOWN");
	}
	else {
		send_config_msg(ch, "ok_string");
	}
}


// basic handler for double
CONFIG_HANDLER(config_edit_double) {
	double input, old;
	
	// basic sanitation
	if (!ch || !config) {
		log("SYSERR: config_edit_double called without %s", ch ? "config" : "ch");
		msg_to_char(ch, "Error editing type.\r\n");
		return;
	}
	
	if (sscanf(argument, "%lf", &input) != 1) {
		msg_to_char(ch, "Invalid argument '%s', expecting a number.\r\n", argument);
		return;
	}
	
	old = config->data.double_val;
	config->data.double_val = input;
	
	if (old != input) {
		save_config_system();
		syslog(SYS_CONFIG, GET_INVIS_LEV(ch), TRUE, "CONFIG: %s set %s to %f, from %f", GET_NAME(ch), config->key, input, old);
		msg_to_char(ch, "%s: set to %f, from %f.\r\n", config->key, input, old);
	}
	else {
		send_config_msg(ch, "ok_string");
	}
}


// basic handler for int
CONFIG_HANDLER(config_edit_int) {
	int input, old;
	
	// basic sanitation
	if (!ch || !config) {
		log("SYSERR: config_edit_int called without %s", ch ? "config" : "ch");
		msg_to_char(ch, "Error editing type.\r\n");
		return;
	}
	
	if (sscanf(argument, "%d", &input) != 1) {
		msg_to_char(ch, "Invalid argument '%s', expecting a number.\r\n", argument);
		return;
	}
	
	old = config->data.int_val;
	config->data.int_val = input;
	
	if (old != input) {
		save_config_system();
		syslog(SYS_CONFIG, GET_INVIS_LEV(ch), TRUE, "CONFIG: %s set %s to %d, from %d", GET_NAME(ch), config->key, input, old);
		msg_to_char(ch, "%s: set to %d, from %d.\r\n", config->key, input, old);
	}
	else {
		send_config_msg(ch, "ok_string");
	}
}


// basic handler for long string
CONFIG_HANDLER(config_edit_long_string) {
	// basic sanitation
	if (!ch || !config) {
		log("SYSERR: config_edit_long_string called without %s", ch ? "config" : "ch");
		msg_to_char(ch, "Error editing type.\r\n");
		return;
	}
	if (!ch->desc) {
		return;	// no desc = no work
	}
	
	if (ch->desc->str) {
		msg_to_char(ch, "You are already editing a string.\r\n");
		return;
	}
	else {
		sprintf(buf, "text for %s", config->key);
		start_string_editor(ch->desc, buf, &config->data.string_val, MAX_CONFIG_TEXT, TRUE);
		ch->desc->save_config = config;
	}
}


// basic handler for sector
CONFIG_HANDLER(config_edit_sector) {
	sector_data *old_sect, *new_sect;
	int input;
	
	// basic sanitation
	if (!ch || !config) {
		log("SYSERR: config_edit_sector called without %s", ch ? "config" : "ch");
		msg_to_char(ch, "Error editing type.\r\n");
		return;
	}
	if (config->type != CONFTYPE_INT) {
		log("SYSERR: config_edit_sector called on non-int key %s", config->key);
		msg_to_char(ch, "Error editing type.\r\n");
		return;
	}
	
	if (sscanf(argument, "%d", &input) != 1) {
		msg_to_char(ch, "Invalid argument '%s', expecting a number.\r\n", argument);
		return;
	}
	
	if (!(new_sect = sector_proto(input))) {
		msg_to_char(ch, "Invalid sector vnum '%s'.\r\n", argument);
		return;
	}
	
	old_sect = sector_proto(config->data.int_val);
	config->data.int_val = input;
	
	if (old_sect != new_sect) {
		save_config_system();
		syslog(SYS_CONFIG, GET_INVIS_LEV(ch), TRUE, "CONFIG: %s set %s to %s, from %s", GET_NAME(ch), config->key, GET_SECT_NAME(new_sect), old_sect ? GET_SECT_NAME(old_sect) : "UNKNOWN");
		msg_to_char(ch, "%s: set to %s, from %s.\r\n", config->key, GET_SECT_NAME(new_sect), old_sect ? GET_SECT_NAME(old_sect) : "UNKNOWN");
	}
	else {
		send_config_msg(ch, "ok_string");
	}
}


// basic handler for short string
CONFIG_HANDLER(config_edit_short_string) {
	char old[MAX_STRING_LENGTH];
	
	// basic sanitation
	if (!ch || !config) {
		log("SYSERR: config_edit_short_string called without %s", ch ? "config" : "ch");
		msg_to_char(ch, "Error editing type.\r\n");
		return;
	}
	
	if (!*argument) {
		msg_to_char(ch, "Set it to what?\r\n");
		return;
	}
	
	strcpy(old, NULLSAFE(config->data.string_val));
	if (config->data.string_val) {
		free(config->data.string_val);
	}
	config->data.string_val = str_dup(argument);
	
	if (strcmp(old, argument)) {
		save_config_system();
		syslog(SYS_CONFIG, GET_INVIS_LEV(ch), TRUE, "CONFIG: %s set %s to \"%s\", from \"%s\"", GET_NAME(ch), config->key, argument, old);
		msg_to_char(ch, "%s: set to \"%s\", from \"%s\".\r\n", config->key, argument, old);
	}
	else {
		send_config_msg(ch, "ok_string");
	}
}


// Custom config handler for editing a 'type' type, resulting in an int, for CONFTYPE_TYPE.
CONFIG_HANDLER(config_edit_type) {
	char buf[256];
	int old, new;
	
	// basic sanitation
	if (!ch || !config) {
		log("SYSERR: config_edit_type called without %s", ch ? "config" : "ch");
		msg_to_char(ch, "Error editing type.\r\n");
		return;
	}
	if (config->type != CONFTYPE_TYPE) {
		log("SYSERR: config_edit_type called on non-type key %s", config->key);
		msg_to_char(ch, "Error editing type.\r\n");
		return;
	}
	if (!config->custom_data) {
		log("SYSERR: config_edit_type called without custom data for key %s", config->key);
		msg_to_char(ch, "Error editing type.\r\n");
		return;
	}
		
	// process (olc processor sends all messages)
	old = config->data.int_val;
	snprintf(buf, sizeof(buf), "config %s", config->key);
	new = olc_process_type(ch, argument, config->key, buf, (const char **)config->custom_data, old);
	
	// no change
	if (old == new) {
		return;
	}
	
	// update
	config->data.int_val = new;
	
	// build log
	syslog(SYS_CONFIG, GET_INVIS_LEV(ch), TRUE, "CONFIG: %s updated %s: set to %s from %s", GET_NAME(ch), config->key, ((const char **)config->custom_data)[new], ((const char **)config->custom_data)[old]);
	save_config_system();
}


 //////////////////////////////////////////////////////////////////////////////
//// CONFIG SYSTEM: CUSTOM EDITORS ///////////////////////////////////////////

// resets after setting
CONFIG_HANDLER(config_edit_autostore_time) {
	int old = config_get_int("autostore_time");
	obj_data *obj;
	
	// pass thru first...
	config_edit_int(ch, config, argument);
	
	if (config_get_int("autostore_time") != old) {
		// update all autostore times
		DL_FOREACH(object_list, obj) {
			if (find_stored_event(GET_OBJ_STORED_EVENTS(obj), SEV_OBJ_AUTOSTORE)) {
				cancel_stored_event(&GET_OBJ_STORED_EVENTS(obj), SEV_OBJ_AUTOSTORE);
				schedule_obj_autostore_check(obj, 0);
			}
		}
	}
}


// ensures everything has a timer if needed
CONFIG_HANDLER(config_edit_decay_in_storage) {
	bool old = config_get_bool("decay_in_storage");
	char_data *chiter;
	
	// pass thru first...
	config_edit_bool(ch, config, argument);
	
	if (config_get_bool("decay_in_storage") && config_get_bool("decay_in_storage") != old) {
		ensure_storage_timers(NOTHING);
		
		// update objs in home storage
		DL_FOREACH2(player_character_list, chiter, next_plr) {
			ensure_home_storage_timers(chiter, NOTHING);
		}
	}
}


// resets after setting
CONFIG_HANDLER(config_edit_mob_spawn_interval) {
	int old = config_get_int("mob_spawn_interval");
	
	// pass thru first...
	config_edit_int(ch, config, argument);
	
	if (config_get_int("mob_spawn_interval") != old) {
		reschedule_all_despawns();
	}
}


 //////////////////////////////////////////////////////////////////////////////
//// CONFIG SYSTEM: SHOW FUNCS ///////////////////////////////////////////////

// basic handler for attack types
CONFIG_HANDLER(config_show_attack) {
	attack_message_data *amd;
	
	// basic sanitation
	if (!ch || !config) {
		log("SYSERR: config_show_attack called without %s", ch ? "config" : "ch");
		msg_to_char(ch, "Error showing type.\r\n");
		return;
	}
	
	amd = real_attack_message(config->data.int_val);
	
	msg_to_char(ch, "Attack type: %d %s\r\n", config->data.int_val, amd ? ATTACK_NAME(amd) : "UNKNOWN");
}


// Custom config handler for showing a bitvector, for CONFTYPE_BITVECTOR
CONFIG_HANDLER(config_show_bitvector) {
	char buf[256];
	
	// basic sanitation
	if (!ch || !config) {
		log("SYSERR: config_show_bitvector called without %s", ch ? "config" : "ch");
		msg_to_char(ch, "Error showing type.\r\n");
		return;
	}
	if (config->type != CONFTYPE_BITVECTOR) {
		log("SYSERR: config_show_bitvector called on non-bitvector key %s", config->key);
		msg_to_char(ch, "Error showing type.\r\n");
		return;
	}
	if (!config->custom_data) {
		log("SYSERR: config_show_bitvector called without custom data for key %s", config->key);
		msg_to_char(ch, "Error showing type.\r\n");
		return;
	}

	// send it through the olc processor, which will display it all
	snprintf(buf, sizeof(buf), "config %s", config->key);
	olc_process_flag(ch, "", config->key, buf, (const char **)config->custom_data, config->data.bitvector_val);
}


// basic handler for bool type
CONFIG_HANDLER(config_show_bool) {
	// basic sanitation
	if (!ch || !config) {
		log("SYSERR: config_show_bool called without %s", ch ? "config" : "ch");
		msg_to_char(ch, "Error showing type.\r\n");
		return;
	}
	
	msg_to_char(ch, "Boolean value: %s\r\n", config->data.bool_val ? "true" : "false");
}


// basic handler for building
CONFIG_HANDLER(config_show_building) {
	bld_data *bld;
	
	// basic sanitation
	if (!ch || !config) {
		log("SYSERR: config_show_building called without %s", ch ? "config" : "ch");
		msg_to_char(ch, "Error showing type.\r\n");
		return;
	}
	
	bld = building_proto(config->data.int_val);
	
	msg_to_char(ch, "Current building: [%d] %s\r\n", config->data.int_val, bld ? GET_BLD_NAME(bld) : "UNKNOWN");
}


// basic handler for double type
CONFIG_HANDLER(config_show_double) {
	// basic sanitation
	if (!ch || !config) {
		log("SYSERR: config_show_double called without %s", ch ? "config" : "ch");
		msg_to_char(ch, "Error showing type.\r\n");
		return;
	}
	
	msg_to_char(ch, "Decimal value: %f\r\n", config->data.double_val);
}


// basic handler for int type
CONFIG_HANDLER(config_show_int) {
	// basic sanitation
	if (!ch || !config) {
		log("SYSERR: config_show_int called without %s", ch ? "config" : "ch");
		msg_to_char(ch, "Error showing type.\r\n");
		return;
	}
	
	msg_to_char(ch, "Integer value: %d\r\n", config->data.int_val);
}


// basic handler for long string type
CONFIG_HANDLER(config_show_long_string) {
	// basic sanitation
	if (!ch || !config) {
		log("SYSERR: config_show_long_string called without %s", ch ? "config" : "ch");
		msg_to_char(ch, "Error showing type.\r\n");
		return;
	}
	
	msg_to_char(ch, "Long string value:\r\n%s", config->data.string_val ? config->data.string_val : "<not set>\r\n");
}


// basic handler for sector
CONFIG_HANDLER(config_show_sector) {
	sector_data *sect;
	
	// basic sanitation
	if (!ch || !config) {
		log("SYSERR: config_show_sector called without %s", ch ? "config" : "ch");
		msg_to_char(ch, "Error showing type.\r\n");
		return;
	}
	
	sect = sector_proto(config->data.int_val);
	
	msg_to_char(ch, "Sector value: %d %s\r\n", config->data.int_val, sect ? GET_SECT_NAME(sect) : "UNKNOWN");
}


// basic handler for short string type
CONFIG_HANDLER(config_show_short_string) {
	// basic sanitation
	if (!ch || !config) {
		log("SYSERR: config_show_short_string called without %s", ch ? "config" : "ch");
		msg_to_char(ch, "Error showing type.\r\n");
		return;
	}
	
	msg_to_char(ch, "String value: %s\r\n", config->data.string_val);
}


// config handler for showing a 'type' type, for CONFTYPE_TYPE
CONFIG_HANDLER(config_show_type) {
	char buf[256];
	
	// basic sanitation
	if (!ch || !config) {
		log("SYSERR: config_show_type called without %s", ch ? "config" : "ch");
		msg_to_char(ch, "Error showing type.\r\n");
		return;
	}
	if (config->type != CONFTYPE_TYPE) {
		log("SYSERR: config_show_type called on non-type key %s", config->key);
		msg_to_char(ch, "Error showing type.\r\n");
		return;
	}
	if (!config->custom_data) {
		log("SYSERR: config_show_type called without custom data for key %s", config->key);
		msg_to_char(ch, "Error showing type.\r\n");
		return;
	}

	// send it through the olc processor, which will display it all
	snprintf(buf, sizeof(buf), "config %s", config->key);
	olc_process_type(ch, "", config->key, buf, (const char **)config->custom_data, config->data.int_val);
}


 //////////////////////////////////////////////////////////////////////////////
//// CONFIG SYSTEM: I/O //////////////////////////////////////////////////////

/**
* Frees 1 config entry. Usually only called during 'shutdown complete'.
*
* @param struct config_type *cnf The config_type entry to free.
*/
void free_config_type(struct config_type *cnf) {
	if (cnf->description) {
		free(cnf->description);
	}
	if (cnf->key) {
		free(cnf->key);
	}
	if (cnf->type == CONFTYPE_SHORT_STRING || cnf->type == CONFTYPE_LONG_STRING) {
		if (cnf->data.string_val) {
			free(cnf->data.string_val);
		}
	}
	free(cnf);
}


/**
* Read one config line that is an int array, starting with the array size.
*
* @param struct config_type *cnf The config item to read into.
* @param char *arg The argument from the config line.
*/
void read_config_int_array(struct config_type *cnf, char *arg) {
	char *remains, word[MAX_INPUT_LENGTH];
	int pos;
	
	// simple sanity
	if (!cnf || !arg) {
		return;
	}
	
	// first up will be the size
	remains = any_one_arg(arg, word);
	if (*word) {
		cnf->data_size = atoi(word);
	}
	else {
		log("SYSERR: read_config_int_array: %s: missing size arg", cnf->key);
		cnf->data_size = 0;
		return;
	}
	
	if (cnf->data_size <= 0) {
		cnf->data.int_array = NULL;
		return;
	}
	
	// ok
	CREATE(cnf->data.int_array, int, cnf->data_size);
	
	// read space-separated ints
	pos = 0;
	while (*remains && pos < cnf->data_size) {
		remains = any_one_arg(remains, word);
		if (*word) {
			cnf->data.int_array[pos++] = atoi(word);
		}
	}
	
	// ensure whole array (in case it ended early)
	for (; pos < cnf->data_size; ++pos) {
		cnf->data.int_array[pos] = 0;
	}
}


/**
* Boot the config system from the config file.
*/
void load_config_system_from_file(void) {
	char line[MAX_INPUT_LENGTH], key[MAX_INPUT_LENGTH], *arg;
	struct config_type *cnf;
	FILE *fl;
	
	// aaand read from file	
	if (!(fl = fopen(CONFIG_FILE, "r"))) {
		log("Unable to read config file %s, booting with no configs", CONFIG_FILE);
		return;
	}
	
	// iterate over whole file
	while (TRUE) {
		// get line, or unnatural end
		if (!get_line(fl, line)) {
			log("SYSERR: Format error in config system file");
			break;
		}
		
		// natural end!
		if (*line == '$') {
			break;
		}
		
		// comment / skip
		if (*line == '*') {
			continue;
		}
		
		arg = any_one_arg(line, key);
		skip_spaces(&arg);
		
		// junk line?
		if (!*key) {
			continue;
		}
		
		// look it up
		HASH_FIND_STR(config_table, key, cnf);
		
		// quick sanity
		if (!cnf) {
			log("SYSERR: Unknown config key: %s", key);
			continue;
		}
		
		// CONFTYPE_x: process argument by type
		switch (cnf->type) {
			case CONFTYPE_BITVECTOR: {
				cnf->data.bitvector_val = asciiflag_conv(arg);
				break;
			}
			case CONFTYPE_BOOL: {
				cnf->data.bool_val = (atoi(arg) > 0) || !str_cmp(arg, "yes") || !str_cmp(arg, "true") || !str_cmp(arg, "on");
				break;
			}
			case CONFTYPE_DOUBLE: {
				cnf->data.double_val = atof(arg);
				break;
			}
			case CONFTYPE_INT: {
				cnf->data.int_val = atoi(arg);
				break;
			}
			case CONFTYPE_INT_ARRAY: {
				read_config_int_array(cnf, arg);
				break;
			}
			case CONFTYPE_SHORT_STRING: {
				cnf->data.string_val = str_dup(arg);
				break;
			}
			case CONFTYPE_LONG_STRING: {
				if (atoi(arg) > 0) {	// only read if the # is not 0 (0 indicates null text)
					cnf->data.string_val = fread_string(fl, "config file");
				}
				else {
					cnf->data.string_val = NULL;
				}
				break;
			}
			case CONFTYPE_TYPE: {
				cnf->data.int_val = atoi(arg);
				break;
			}
			default: {
				log("Unable to load config system: %s: unloadable config type %d", cnf->key, cnf->type);
				break;
			}
		}
	}
	
	fclose(fl);
}


/**
* Writes an int array as: key size val val val
* For example:
*  techs_requiring_same_island 3 1 2 -1
*
* @param struct config_type *cnf The config element to write.
* @param FILE *fl The file open for writing.
*/
void write_config_int_array(struct config_type *cnf, FILE *fl) {
	int iter;
	
	// sanity
	if (!cnf || !fl || cnf->type != CONFTYPE_INT_ARRAY) {
		return;
	}
	
	fprintf(fl, "%s %d", cnf->key, cnf->data_size);
	for (iter = 0; iter < cnf->data_size; ++iter) {
		fprintf(fl, " %d", cnf->data.int_array[iter]);
	}
	fprintf(fl, "\n");
}


/**
* Save all game configs to file.
*/
void save_config_system(void) {
	char temp[MAX_STRING_LENGTH];
	struct config_type *cnf, *next_cnf;
	int last_set = -1;
	FILE *fl;
	
	if (block_all_saves_due_to_shutdown) {
		return;
	}
	
	if (!(fl = fopen(CONFIG_FILE TEMP_SUFFIX, "w"))) {
		syslog(SYS_ERROR, LVL_START_IMM, TRUE, "SYSERR: Unable to write %s", CONFIG_FILE TEMP_SUFFIX);
		return;
	}
	
	fprintf(fl, "* EmpireMUD Game Configs\n");
	fprintf(fl, "* See init_config_system() in config.c for adding new configs\n");
	
	HASH_ITER(hh, config_table, cnf, next_cnf) {
		// these are ordered by set
		if (cnf->set != last_set) {
			fprintf(fl, "*\n");
			fprintf(fl, "* %s configs\n", config_groups[cnf->set]);
			last_set = cnf->set;
		}
	
		// CONFTYPE_x
		switch (cnf->type) {
			case CONFTYPE_BITVECTOR: {
				fprintf(fl, "%s %s\n", cnf->key, bitv_to_alpha(cnf->data.bitvector_val));
				break;
			}
			case CONFTYPE_BOOL: {
				fprintf(fl, "%s %d\n", cnf->key, cnf->data.bool_val ? 1 : 0);
				break;
			}
			case CONFTYPE_DOUBLE: {
				fprintf(fl, "%s %f\n", cnf->key, cnf->data.double_val);
				break;
			}
			case CONFTYPE_INT: {
				fprintf(fl, "%s %d\n", cnf->key, cnf->data.int_val);
				break;
			}
			case CONFTYPE_INT_ARRAY: {
				write_config_int_array(cnf, fl);
				break;
			}
			case CONFTYPE_SHORT_STRING: {
				fprintf(fl, "%s %s\n", cnf->key, cnf->data.string_val);
				break;
			}
			case CONFTYPE_LONG_STRING: {
				if (cnf->data.string_val) {
					strcpy(temp, NULLSAFE(cnf->data.string_val));
					strip_crlf(temp);
					fprintf(fl, "%s 1\n%s~\n", cnf->key, temp);
				}
				else {
					fprintf(fl, "%s 0\n", cnf->key);
				}
				break;
			}
			case CONFTYPE_TYPE: {
				fprintf(fl, "%s %d\n", cnf->key, cnf->data.int_val);
				break;
			}
			default: {
				syslog(SYS_ERROR, LVL_START_IMM, TRUE, "Unable to save config system: unsavable config type %d", cnf->type);
				fclose(fl);
				return;
			}
		}
	}

	fprintf(fl, "$~\n");
	fclose(fl);
	rename(CONFIG_FILE TEMP_SUFFIX, CONFIG_FILE);	
}


 //////////////////////////////////////////////////////////////////////////////
//// CONFIG SYSTEM: UTILS ////////////////////////////////////////////////////

/**
* Adds an int to an int array for the config system, only if the int is not
* already in the array.
*
* @param int to_add The integer to add.
* @param int **array A pointer to the dynamically-allocated array (size may change).
* @param int *size The variable to which the old size is bound (may change).
* @return bool TRUE if the int was added, FALSE if it already existed.
*/
bool add_int_to_int_array(int to_add, int **array, int *size) {
	bool done = FALSE;
	int *new_array;
	int iter;
	
	// ensure does not exist
	if (*array) {
		for (iter = 0; iter < *size; ++iter) {
			if ((*array)[iter] == to_add) {
				return FALSE;
			}
		}
	}
	
	CREATE(new_array, int, *size + 1);
	for (iter = 0; iter < *size; ++iter) {
		if ((*array)[iter] < to_add) {
			new_array[iter] = (*array)[iter];
		}
		else {
			if (!done) {
				new_array[iter] = to_add;
				done = TRUE;
			}
			new_array[iter+1] = (*array)[iter];
		}
	}
	if (!done) {
		new_array[iter] = to_add;
	}
	
	if (*array) {
		free(*array);
	}
	*array = new_array;
	*size += 1;
	
	return TRUE;
}


/**
* Determines if an int exists in an int array.
*
* @param int to_find The value to search for.
* @param int *array The array to search.
* @param int size The length of the array.
* @return bool TRUE if the int is in the array, or FALSE.
*/
bool find_int_in_array(int to_find, int *array, int size) {
	int iter;
	for (iter = 0; iter < size; ++iter) {
		if (array[iter] == to_find) {
			return TRUE;
		}
	}
	
	return FALSE;
}


/**
* Removes an int from an int array for the config system, only if the int is
* already in the array.
*
* @param int to_remove The integer to remove.
* @param int **array A pointer to the dynamically-allocated array (size may change).
* @param int *size The variable to which the old size is bound (may change).
* @return bool TRUE if the int was removed, FALSE if it didn't exist.
*/
bool remove_int_from_int_array(int to_remove, int **array, int *size) {
	bool done = FALSE, found = FALSE;
	int *new_array;
	int iter;
	
	// ensure exists
	if (*array) {
		for (iter = 0; iter < *size; ++iter) {
			if ((*array)[iter] == to_remove) {
				found = TRUE;
			}
		}
	}
	
	// not in array
	if (!found) {
		return FALSE;
	}
	
	CREATE(new_array, int, MAX(1, *size - 1));
	for (iter = 0; iter < *size - 1; ++iter) {
		if (done || (*array)[iter] == to_remove) {
			new_array[iter] = (*array)[iter + 1];
			if ((*array)[iter] == to_remove) {
				done = TRUE;
			}
		}
		else {
			new_array[iter] = (*array)[iter];
		}
	}
		
	if (*array) {
		free(*array);
	}
	*array = new_array;
	*size -= 1;
	
	return TRUE;
}


/**
* Simple sorter for the configs hash
*
* @param struct config_type *a One element
* @param struct config_type *b Another element
* @return int Sort instruction of -1, 0, or 1
*/
int sort_configs(struct config_type *a, struct config_type *b) {
	if (a->set != b->set) {
		return a->set - b->set;
	}
	return str_cmp(a->key, b->key);
}


/**
* Set up a config element to be read from the conf file.
*
* @param int set CONFIG_ group.
* @param char *key Unique string key of this config (MUST be all one word).
* @param int type CONFTYPE_x expected data type.
* @param char *description String for contextual help.
*/
void init_config(int set, char *key, int type, char *description) {
	struct config_type *cnf;
	
	// no work
	if (!key || !*key) {
		return;
	}
	
	// don't insert duplicates
	HASH_FIND_STR(config_table, key, cnf);
	if (!cnf) {
		CREATE(cnf, struct config_type, 1);
		cnf->key = str_dup(key);
		HASH_ADD_STR(config_table, key, cnf);
	}
	
	// update data
	cnf->set = set;
	cnf->type = type;
	if (cnf->description) {
		free(cnf->description);
	}
	cnf->description = str_dup(description);
	
	// ensure no data
	memset((char *)(&cnf->data), 0, sizeof(union config_data_union));
	cnf->data_size = 0;
	
	// CONFTYPE_x
	switch (cnf->type) {
		case CONFTYPE_BOOL: {
			cnf->show_func = config_show_bool;
			cnf->edit_func = config_edit_bool;
			break;
		}
		case CONFTYPE_DOUBLE: {
			cnf->show_func = config_show_double;
			cnf->edit_func = config_edit_double;
			break;
		}
		case CONFTYPE_INT: {
			cnf->show_func = config_show_int;
			cnf->edit_func = config_edit_int;
			break;
		}
		case CONFTYPE_SHORT_STRING: {
			cnf->show_func = config_show_short_string;
			cnf->edit_func = config_edit_short_string;
			break;
		}
		case CONFTYPE_LONG_STRING: {
			cnf->show_func = config_show_long_string;
			cnf->edit_func = config_edit_long_string;
			break;
		}
		case CONFTYPE_BITVECTOR: {
			cnf->show_func = config_show_bitvector;
			cnf->edit_func = config_edit_bitvector;
			break;
		}
		case CONFTYPE_TYPE: {
			cnf->show_func = config_show_type;
			cnf->edit_func = config_edit_type;
			break;
		}
		case CONFTYPE_INT_ARRAY:
		default: {
			// currently requires a custom handler
			cnf->show_func = NULL;
			cnf->edit_func = NULL;
			break;
		}
	}
	
	// clear custom stuff
	cnf->custom_data = NULL;
	
	// sort of important
	HASH_SORT(config_table, sort_configs);
}


/**
* Attaches custom handlers to config entries.
*
* @param char *key The config key to update (must already be set up).
* @param CONFIG_HANDLER(*show_func) The function to show the config to a player.
* @param CONFIG_HANDLER(*edit_func) The function for the player to edit the config.
* @param void *custom_data Misc data used by show_func/edit_func.
*/
void init_config_custom(char *key, CONFIG_HANDLER(*show_func), CONFIG_HANDLER(*edit_func), void *custom_data) {
	struct config_type *cnf;
	
	// no work
	if (!key || !*key) {
		return;
	}
	
	// don't insert duplicates
	HASH_FIND_STR(config_table, key, cnf);
	if (!cnf) {
		log("SYSERR: init_config_custom: %s: no key found", key);
		return;
	}
	
	// add data
	cnf->show_func = show_func;
	cnf->edit_func = edit_func;
	cnf->custom_data = custom_data;
}


/**
* Marks flags on configs that need them.
*
* @param char *key The config key to update (must already be set up).
* @param bitvector_t Any CONF_FLAG_ to add.
*/
void init_config_flags(char *key, bitvector_t flags) {
	struct config_type *cnf;
	
	HASH_FIND_STR(config_table, key, cnf);
	if (!cnf) {
		log("SYSERR: init_config_flags: %s: no key found", key);
		return;
	}
	
	// add data
	cnf->config_flags |= flags;
}


 //////////////////////////////////////////////////////////////////////////////
//// CONFIG SYSTEM: SETUP ////////////////////////////////////////////////////

/**
* Boot up the game's config system.
*/
void init_config_system(void) {	
	if (config_table != NULL) {
		log("SYSERR: Trying to re-init the config system mid-game.\r\n");
		return;
	}

	// first set up all the config types
	
	// approval
	init_config(CONFIG_APPROVAL, "auto_approve", CONFTYPE_BOOL, "automatically approve players when created");
	init_config(CONFIG_APPROVAL, "approve_per_character", CONFTYPE_BOOL, "require approval for every character, rather than by account");
	init_config(CONFIG_APPROVAL, "need_approval_string", CONFTYPE_SHORT_STRING, "error message when an unapproved player uses a command");
	init_config(CONFIG_APPROVAL, "build_approval", CONFTYPE_BOOL, "build, upgrade, lay roads, roadsigns");
	init_config(CONFIG_APPROVAL, "chat_approval", CONFTYPE_BOOL, "slash-channels and global channels");
	init_config(CONFIG_APPROVAL, "craft_approval", CONFTYPE_BOOL, "all crafting commands");
	init_config(CONFIG_APPROVAL, "event_approval", CONFTYPE_BOOL, "participate in events");
	init_config(CONFIG_APPROVAL, "gather_approval", CONFTYPE_BOOL, "all resource gathering");
	init_config(CONFIG_APPROVAL, "join_empire_approval", CONFTYPE_BOOL, "pledge, enroll");
	init_config(CONFIG_APPROVAL, "manage_empire_approval", CONFTYPE_BOOL, "commands related to having an empire");
	init_config(CONFIG_APPROVAL, "quest_approval", CONFTYPE_BOOL, "quest command");
	init_config(CONFIG_APPROVAL, "restrict_anonymous_hosts", CONFTYPE_BOOL, "restricts approval of players from hosts like mudconnector.com");
	init_config(CONFIG_APPROVAL, "skill_gain_approval", CONFTYPE_BOOL, "gain any skill points");
	init_config(CONFIG_APPROVAL, "tell_approval", CONFTYPE_BOOL, "sending tells (except to immortals)");
	init_config(CONFIG_APPROVAL, "terraform_approval", CONFTYPE_BOOL, "excavate, fillin, chant of nature");
	init_config(CONFIG_APPROVAL, "title_approval", CONFTYPE_BOOL, "set own title");
	init_config(CONFIG_APPROVAL, "travel_approval", CONFTYPE_BOOL, "transport, portal, summon, vehicles");
	init_config(CONFIG_APPROVAL, "unapproved_greeting", CONFTYPE_LONG_STRING, "shown to unapproved characters on login");
	init_config(CONFIG_APPROVAL, "write_approval", CONFTYPE_BOOL, "boards, books, mail");

	// game configs
	init_config(CONFIG_GAME, "allow_extended_color_codes", CONFTYPE_BOOL, "if on, players can use \t&[F000] and \t&[B000]");
	init_config(CONFIG_GAME, "automessage_color", CONFTYPE_SHORT_STRING, "color code for automessage (like &&c)");
	init_config(CONFIG_GAME, "godlist_header", CONFTYPE_LONG_STRING, "shown at the top of the godlist - note gods are not well-implemented");
	init_config(CONFIG_GAME, "hiring_builders", CONFTYPE_BOOL, "whether the mud is hiring builders");
	init_config(CONFIG_GAME, "hiring_coders", CONFTYPE_BOOL, "whether the mud is hiring coders");
	init_config(CONFIG_GAME, "mud_contact", CONFTYPE_SHORT_STRING, "email address of an admin");
	init_config(CONFIG_GAME, "mud_created", CONFTYPE_SHORT_STRING, "year the mud was created");
	init_config(CONFIG_GAME, "mud_hostname", CONFTYPE_SHORT_STRING, "your mud's hostname");
	init_config(CONFIG_GAME, "mud_icon", CONFTYPE_SHORT_STRING, "a 32x32 bmp, png, jpg, or gif less than 32kb");
	init_config(CONFIG_GAME, "mud_ip", CONFTYPE_SHORT_STRING, "ip address of your mud");
	init_config(CONFIG_GAME, "mud_location", CONFTYPE_SHORT_STRING, "location of the mud server");
	init_config(CONFIG_GAME, "mud_minimum_age", CONFTYPE_SHORT_STRING, "suggested minimum age to play");
	init_config(CONFIG_GAME, "mud_name", CONFTYPE_SHORT_STRING, "name of your mud");
	init_config(CONFIG_GAME, "mud_status", CONFTYPE_SHORT_STRING, "one of: Alpha, Closed Beta, Open Beta, Live");
	init_config(CONFIG_GAME, "mud_website", CONFTYPE_SHORT_STRING, "your mud's website");
	init_config(CONFIG_GAME, "name_rules", CONFTYPE_LONG_STRING, "shown during creation");
	init_config(CONFIG_GAME, "name_rules_lastname", CONFTYPE_LONG_STRING, "shown during creation for last names");
	init_config(CONFIG_GAME, "starting_year", CONFTYPE_INT, "base year");
	init_config(CONFIG_GAME, "welcome_message", CONFTYPE_SHORT_STRING, "message shown to all players on login");
	init_config(CONFIG_GAME, "ok_string", CONFTYPE_SHORT_STRING, "simple Ok message");
	init_config(CONFIG_GAME, "no_person", CONFTYPE_SHORT_STRING, "bad target error for no person");
	init_config(CONFIG_GAME, "huh_string", CONFTYPE_SHORT_STRING, "message for invalid command");
	init_config(CONFIG_GAME, "public_logins", CONFTYPE_BOOL, "login/out/alt display to mortlog instead of elog");
	init_config(CONFIG_GAME, "start_message", CONFTYPE_LONG_STRING, "shown to new characters on login");
	init_config(CONFIG_GAME, "who_list_sort", CONFTYPE_TYPE, "what order the who-list appears in");
		init_config_custom("who_list_sort", config_show_type, config_edit_type, who_list_sort_types);
	init_config(CONFIG_GAME, "wizlist_header", CONFTYPE_LONG_STRING, "shown at the top of the wizlist");

	// actions
	init_config(CONFIG_ACTIONS, "chore_distance", CONFTYPE_INT, "tiles away from home a citizen will work");
	init_config(CONFIG_ACTIONS, "chip_timer", CONFTYPE_INT, "ticks to chip flint");
	init_config(CONFIG_ACTIONS, "chop_timer", CONFTYPE_INT, "weapon damage to chop 1 tree");
	init_config(CONFIG_ACTIONS, "dig_base_timer", CONFTYPE_INT, "ticks, sped up by Finder and/or shovel");
	init_config(CONFIG_ACTIONS, "fishing_timer", CONFTYPE_INT, "time per fish, halved by high Survival");
	init_config(CONFIG_ACTIONS, "forage_base_timer", CONFTYPE_INT, "ticks, cut in half by Finder");
	init_config(CONFIG_ACTIONS, "gather_base_timer", CONFTYPE_INT, "ticks, halved by Finder");
	init_config(CONFIG_ACTIONS, "harvest_timer", CONFTYPE_INT, "ticks to harvest a crop, modified by Dexterity");
	init_config(CONFIG_ACTIONS, "mining_timer", CONFTYPE_INT, "weapon damage to mine 1 ore");
	init_config(CONFIG_ACTIONS, "panning_timer", CONFTYPE_INT, "ticks to pan one time");
	init_config(CONFIG_ACTIONS, "planting_base_timer", CONFTYPE_INT, "in game hours; planting cuts it in half up to 3 times");
	init_config(CONFIG_ACTIONS, "planting_workforce_timer", CONFTYPE_INT, "in game hours; how long crops take after workforce harvests them");
	init_config(CONFIG_ACTIONS, "tan_timer", CONFTYPE_INT, "ticks to tan skin, reduced by location");
	init_config(CONFIG_ACTIONS, "common_depletion", CONFTYPE_INT, "amount of resources you get from 1 tile, normally");
	init_config(CONFIG_ACTIONS, "short_depletion", CONFTYPE_INT, "depletion used by hunt, formerly by others");
	init_config(CONFIG_ACTIONS, "high_depletion", CONFTYPE_INT, "depletion in buildings with HIGH-DEPLETION");
	init_config(CONFIG_ACTIONS, "pick_base_timer", CONFTYPE_INT, "ticks to do pick interactions");
	init_config(CONFIG_ACTIONS, "prospecting_workforce_hours", CONFTYPE_INT, "real hours for workforce to prospect 1 tile (shorter than world reset)");
	init_config(CONFIG_ACTIONS, "shear_growth_time", CONFTYPE_INT, "real hours to regrow wool");
	init_config(CONFIG_ACTIONS, "trench_initial_value", CONFTYPE_INT, "negative starting value for excavate -- done when it counts up to 0");
	init_config(CONFIG_ACTIONS, "trench_gain_from_rain", CONFTYPE_INT, "amount of rain water per room update added to a trench");
	init_config(CONFIG_ACTIONS, "trench_fill_time", CONFTYPE_INT, "seconds before a trench is full");
	init_config(CONFIG_ACTIONS, "max_chore_resource_over_total", CONFTYPE_INT, "how much of a resource workers will gather if over the total cap");
	init_config(CONFIG_ACTIONS, "max_chore_resource_per_member", CONFTYPE_INT, "workforce resource cap per member");
	init_config(CONFIG_ACTIONS, "world_reset_hours", CONFTYPE_INT, "how many real hours between resets for depletions and empire maintenance");
	init_config(CONFIG_ACTIONS, "world_reset_message", CONFTYPE_SHORT_STRING, "shown to all players before the laggy reset of depletions and empire maintenance");

	// cities
	init_config(CONFIG_CITY, "players_per_city_point", CONFTYPE_INT, "how many members you need to earn each city point");
	init_config(CONFIG_CITY, "min_distance_between_cities", CONFTYPE_INT, "tiles between city centers");
	init_config(CONFIG_CITY, "min_distance_between_ally_cities", CONFTYPE_INT, "tiles between cities belonging to allies");
	init_config(CONFIG_CITY, "min_distance_from_city_to_starting_location", CONFTYPE_INT, "tiles between a city and a starting location");
	init_config(CONFIG_CITY, "cities_on_newbie_islands", CONFTYPE_BOOL, "whether or not cities can be founded on newbie islands");
	init_config(CONFIG_CITY, "city_overage_timeout", CONFTYPE_INT, "hours until cities decay from having too many city points spent");
	init_config(CONFIG_CITY, "disrepair_minor", CONFTYPE_INT, "percent of damage to show minor disrepair");
	init_config(CONFIG_CITY, "disrepair_major", CONFTYPE_INT, "percent of damage to show major disrepair");
	init_config(CONFIG_CITY, "disrepair_limit", CONFTYPE_INT, "years of disrepair before collapse");
	init_config(CONFIG_CITY, "disrepair_limit_unfinished", CONFTYPE_INT, "years of disrepair before unfinished buildings collapse");
	init_config(CONFIG_CITY, "max_out_of_city_portal", CONFTYPE_INT, "maximum distance a portal can travel outside of a city");
	init_config(CONFIG_CITY, "minutes_to_full_city", CONFTYPE_INT, "time it takes for a city to count for in-city-only tasks");

	// empire
	init_config(CONFIG_EMPIRE, "decay_in_storage", CONFTYPE_BOOL, "stored items still count down their decay timers");
		init_config_custom("decay_in_storage", config_show_bool, config_edit_decay_in_storage, NULL);
	init_config(CONFIG_EMPIRE, "homeless_citizen_speed", CONFTYPE_INT, "tiles of movement per real minute, for migrating homeless");
	init_config(CONFIG_EMPIRE, "land_per_greatness", CONFTYPE_INT, "base territory per 1 Greatness");
	init_config(CONFIG_EMPIRE, "land_frontier_modifier", CONFTYPE_DOUBLE, "portion of land that can be far from cities");
	init_config(CONFIG_EMPIRE, "land_min_cap", CONFTYPE_INT, "lowest possible claim cap, to prevent very low numbers");
	init_config(CONFIG_EMPIRE, "land_outside_city_modifier", CONFTYPE_DOUBLE, "portion of land that can be in the outskirts area of cities");
	init_config(CONFIG_EMPIRE, "building_population_timer", CONFTYPE_INT, "game hours per citizen move-in");
	init_config(CONFIG_EMPIRE, "frontier_timeout", CONFTYPE_INT, "after this many days, all frontier territory is lost; 0 to disable this");
	init_config(CONFIG_EMPIRE, "time_to_empire_delete", CONFTYPE_INT, "weeks until an empire is deleted");
	init_config(CONFIG_EMPIRE, "time_to_empire_emptiness", CONFTYPE_INT, "weeks until NPCs don't spawn");
	init_config(CONFIG_EMPIRE, "member_timeout_newbie", CONFTYPE_INT, "days until newbie times out");
	init_config(CONFIG_EMPIRE, "minutes_per_day_newbie", CONFTYPE_INT, "minutes played per day for noob status");
	init_config(CONFIG_EMPIRE, "member_timeout_full", CONFTYPE_INT, "days until full member times out");
	init_config(CONFIG_EMPIRE, "min_distrustful_distance", CONFTYPE_INT, "distance from starting locations where the distrustful flag starts working");
	init_config(CONFIG_EMPIRE, "minutes_per_day_full", CONFTYPE_INT, "minutes played per day for full member");
	init_config(CONFIG_EMPIRE, "member_timeout_max_threshold", CONFTYPE_INT, "hours, 1 week of playtime");
	init_config(CONFIG_EMPIRE, "newbie_island_day_limit", CONFTYPE_INT, "number of days old an empire can be before losing newbie island claims");
	init_config(CONFIG_EMPIRE, "outskirts_modifier", CONFTYPE_DOUBLE, "multiplier for city radius that determines outskirts");
	init_config(CONFIG_EMPIRE, "whole_empire_timeout", CONFTYPE_INT, "days to empire appearing idle");
	init_config(CONFIG_EMPIRE, "empire_log_ttl", CONFTYPE_INT, "how many days elogs last");
	init_config(CONFIG_EMPIRE, "redesignate_time", CONFTYPE_INT, "minutes until you can redesignate a room again");

	// items
	init_config(CONFIG_ITEMS, "auto_update_items", CONFTYPE_BOOL, "uses item version numbers to automatically update items");
	init_config(CONFIG_ITEMS, "autostore_time", CONFTYPE_INT, "minutes items last on the ground");
		init_config_custom("autostore_time", config_show_int, config_edit_autostore_time, NULL);
	init_config(CONFIG_ITEMS, "bound_item_junk_time", CONFTYPE_INT, "minutes bound items last on the ground before being junked");
	init_config(CONFIG_ITEMS, "long_autostore_time", CONFTYPE_INT, "minutes items last with the long-autostore bld flag");
	init_config(CONFIG_ITEMS, "min_timer_after_retrieve", CONFTYPE_INT, "hour ticks guaranteed after retrieving a resource with a low timer remaining");
	init_config(CONFIG_ITEMS, "room_item_limit", CONFTYPE_INT, "number of items allowed in buildings with item-limit flag");
	init_config(CONFIG_ITEMS, "scale_points_at_100", CONFTYPE_DOUBLE, "number of scaling points for a 100-scale item");
	init_config(CONFIG_ITEMS, "scale_food_fullness", CONFTYPE_DOUBLE, "fullness hours per scaling point");
	init_config(CONFIG_ITEMS, "scale_drink_capacity", CONFTYPE_DOUBLE, "drink hours per scaling point");
	init_config(CONFIG_ITEMS, "scale_coin_amount", CONFTYPE_DOUBLE, "coins per scaling point");
	init_config(CONFIG_ITEMS, "scale_pack_size", CONFTYPE_DOUBLE, "inventory size per scaling point");
	
	// mail
	init_config(CONFIG_MAIL, "mail_available_message", CONFTYPE_SHORT_STRING, "text shown when player has mail");
	init_config(CONFIG_MAIL, "mail_not_available_message", CONFTYPE_SHORT_STRING, "text shown when player does not have mail");
	init_config(CONFIG_MAIL, "mail_receive_message_to_char", CONFTYPE_SHORT_STRING, "text shown to player upon receiving mail, may contain ## to indicate what was received");
	init_config(CONFIG_MAIL, "mail_receive_message_to_room", CONFTYPE_SHORT_STRING, "text shown to room when someone receives mail; use $n for player name and ## to indicate what was received");
	init_config(CONFIG_MAIL, "mail_requires_building_to_receive", CONFTYPE_BOOL, "whether players must be somewhere with the MAIL function to receive");
	init_config(CONFIG_MAIL, "mail_requires_building_to_receive_message", CONFTYPE_SHORT_STRING, "text shown when player is not at a building");
	init_config(CONFIG_MAIL, "mail_requires_building_to_send", CONFTYPE_BOOL, "whether players must be somewhere with the MAIL function to send");
	init_config(CONFIG_MAIL, "mail_requires_building_to_send_message", CONFTYPE_SHORT_STRING, "text shown when player is not at a building");
	init_config(CONFIG_MAIL, "mail_send_message", CONFTYPE_SHORT_STRING, "text shown when player finishes writing mail");

	// mobs
	init_config(CONFIG_MOBS, "default_language_vnum", CONFTYPE_INT, "language (generic) mobs speak with by default");
	init_config(CONFIG_MOBS, "default_magical_attack", CONFTYPE_INT, "vnum of the default attack for magical damage");
		init_config_custom("default_magical_attack", config_show_attack, config_edit_attack, NULL);
	init_config(CONFIG_MOBS, "default_physical_attack", CONFTYPE_INT, "vnum of the default attack for physical damage");
		init_config_custom("default_physical_attack", config_show_attack, config_edit_attack, NULL);
	init_config(CONFIG_MOBS, "max_npc_attribute", CONFTYPE_INT, "how high primary attributes go on mobs");
	init_config(CONFIG_MOBS, "mob_spawn_interval", CONFTYPE_INT, "how often mobs spawn/last");
		init_config_custom("mob_spawn_interval", config_show_int, config_edit_mob_spawn_interval, NULL);
	init_config(CONFIG_MOBS, "mob_spawn_radius", CONFTYPE_INT, "distance from players that mobs spawn");
	init_config(CONFIG_MOBS, "mob_despawn_radius", CONFTYPE_INT, "distance from players to despawn mobs");
	init_config(CONFIG_MOBS, "npc_follower_limit", CONFTYPE_INT, "more npc followers than this causes aggro");
	init_config(CONFIG_MOBS, "num_duplicates_in_stable", CONFTYPE_INT, "number of npc duplicates allowed in a stable before some leave");
	init_config(CONFIG_MOBS, "spawn_limit_per_room", CONFTYPE_INT, "max NPCs that will spawn in a map room");
	init_config(CONFIG_MOBS, "mob_pursuit_timeout", CONFTYPE_INT, "minutes that mob pursuit memory lasts");
	init_config(CONFIG_MOBS, "mob_pursuit_distance", CONFTYPE_INT, "distance a mob will pursue (as the crow flies)");
	init_config(CONFIG_MOBS, "use_mob_stacking", CONFTYPE_BOOL, "whether or not mobs show as stacks on look");

/*	// this will take some testing to convert
	init_config(CONFIG_EMPIRE, "techs_requiring_same_island", CONFTYPE_INT_ARRAY, "techs that only work if they're on the same island (these should not be techs that come from player abilities)");
		init_config_custom("techs_requiring_same_island", config_show_typelist, config_edit_typelist, (void*)empire_tech_types);

*/
	// other
	init_config(CONFIG_OTHER, "test_config", CONFTYPE_INT, "this is a test config");
	
	// players
	init_config(CONFIG_PLAYERS, "bonus_trait_reset_message", CONFTYPE_LONG_STRING, "message shown when player hits hours_to_bonus_trait_reset and re-logs in");
	init_config(CONFIG_PLAYERS, "dailies_per_day", CONFTYPE_INT, "how many daily quests a player can complete each day");
	init_config(CONFIG_PLAYERS, "default_class_abbrev", CONFTYPE_SHORT_STRING, "abbreviation to show for unclassed players");
	init_config(CONFIG_PLAYERS, "default_class_name", CONFTYPE_SHORT_STRING, "name to show for unclassed players");
	init_config(CONFIG_PLAYERS, "delete_abandoned_players_after", CONFTYPE_INT, "days until deleting an account with only characters below 100 and very low playtime (0 = never)");
	init_config(CONFIG_PLAYERS, "delete_inactive_players_after", CONFTYPE_INT, "days an account can be inactive before auto-delete (0 = never)");
	init_config(CONFIG_PLAYERS, "delete_invalid_players_after", CONFTYPE_INT, "days to wait before deleting accounts with no level or who never logged in (0 = never)");
	init_config(CONFIG_PLAYERS, "exp_level_difference", CONFTYPE_INT, "levels a player can have above a mob and still gain exp from it");
	init_config(CONFIG_PLAYERS, "hours_to_advanced_bonus_traits", CONFTYPE_INT, "prior to this players can only choose newbie traits");
	init_config(CONFIG_PLAYERS, "hours_to_bonus_trait_reset", CONFTYPE_INT, "players get a free bonus trait reset after this long");
	init_config(CONFIG_PLAYERS, "hours_to_first_bonus_trait", CONFTYPE_INT, "how much playtime to get the first bonus trait");
	init_config(CONFIG_PLAYERS, "hours_to_second_bonus_trait", CONFTYPE_INT, "how much playtime to get the second bonus trait");
	init_config(CONFIG_PLAYERS, "lastname_mode", CONFTYPE_BITVECTOR, "how players choose their lastname, if at all");
		init_config_custom("lastname_mode", config_show_bitvector, config_edit_bitvector, lastname_modes);
	init_config(CONFIG_PLAYERS, "pool_bonus_amount", CONFTYPE_INT, "bonus trait amount for health/move/mana, multiplied by (level/25)");
	init_config(CONFIG_PLAYERS, "max_light_radius_base", CONFTYPE_INT, "maximum tiles away you can see unskilled in the dark based on the moon(s)");
	init_config(CONFIG_PLAYERS, "num_daily_skill_points", CONFTYPE_INT, "easy skillups per day");
	init_config(CONFIG_PLAYERS, "num_bonus_trait_daily_skills", CONFTYPE_INT, "bonus trait for skillups");
	init_config(CONFIG_PLAYERS, "idle_action_rent_time", CONFTYPE_INT, "how many minutes before a player performing an action is idle-rented");
	init_config(CONFIG_PLAYERS, "idle_rent_time", CONFTYPE_INT, "how many minutes before a player is idle-rented");
	init_config(CONFIG_PLAYERS, "idle_linkdead_rent_time", CONFTYPE_INT, "how many minutes before a linkdead player is idle-rented");
	init_config(CONFIG_PLAYERS, "line_of_sight", CONFTYPE_BOOL, "if on, terrain can block view on the map");
	init_config(CONFIG_PLAYERS, "map_memory_limit", CONFTYPE_INT, "how many tiles a player is allowed to remember (10k tiles = 1MB RAM)");
	init_config(CONFIG_PLAYERS, "max_capitals_in_name", CONFTYPE_INT, "how many uppercase letters can be in a player name (0 for unlimited)");
	init_config(CONFIG_PLAYERS, "max_home_store_uniques", CONFTYPE_INT, "how many unique (stackable) items can be in home storage");
	init_config(CONFIG_PLAYERS, "max_player_attribute", CONFTYPE_INT, "how high primary player attributes go");
	init_config(CONFIG_PLAYERS, "max_sleeping_regen_time", CONFTYPE_INT, "maximum seconds to regen to full when sleeping (min interval is 5)");
	init_config(CONFIG_PLAYERS, "remove_lore_after_years", CONFTYPE_INT, "game years to clean some lore types");
	init_config(CONFIG_PLAYERS, "default_map_size", CONFTYPE_INT, "distance from the player that can be seen");
	init_config(CONFIG_PLAYERS, "max_map_size", CONFTYPE_INT, "highest view radius a player may set");
	init_config(CONFIG_PLAYERS, "max_map_while_moving", CONFTYPE_INT, "maximum mapsize if the player is moving quickly");
	init_config(CONFIG_PLAYERS, "blood_starvation_level", CONFTYPE_INT, "how low blood gets before a vampire is starving");
	init_config(CONFIG_PLAYERS, "offer_time", CONFTYPE_INT, "seconds an offer is good for, for accept/reject");
	init_config(CONFIG_PLAYERS, "skills_at_max_level", CONFTYPE_INT, "how many skill trees players can have at 100");
	init_config(CONFIG_PLAYERS, "skills_at_specialty_level", CONFTYPE_INT, "how many skill trees players can have at 75 (not counting 100s)");
	init_config(CONFIG_PLAYERS, "skills_per_char", CONFTYPE_INT, "how many total skill trees players may learn at once");
	init_config(CONFIG_PLAYERS, "slash_message_log_days", CONFTYPE_INT, "number of days messages are kept in slash-channel and global history log files");
	init_config(CONFIG_PLAYERS, "temperature_discomfort", CONFTYPE_INT, "temperature after which penalties begin");
	init_config(CONFIG_PLAYERS, "temperature_extreme", CONFTYPE_INT, "temperature after which players take damage");
	init_config(CONFIG_PLAYERS, "temperature_from_fire", CONFTYPE_INT, "warmth value from certain lights");
	init_config(CONFIG_PLAYERS, "temperature_from_water", CONFTYPE_INT, "cooling value from water tiles");
	init_config(CONFIG_PLAYERS, "temperature_penalties", CONFTYPE_BOOL, "whether or not players must manage warmth and cooling");
	
	// skills
	init_config(CONFIG_SKILLS, "exp_from_workforce", CONFTYPE_DOUBLE, "amount of exp gained per chore completion");
	init_config(CONFIG_SKILLS, "morph_timer", CONFTYPE_INT, "ticks to morph between forms");
	init_config(CONFIG_SKILLS, "tracks_lifespan", CONFTYPE_INT, "minutes that a set of tracks lasts");
	init_config(CONFIG_SKILLS, "greater_enchantments_bonus", CONFTYPE_DOUBLE, "multiplier for augment scale points when the player has a mastery ability");
	init_config(CONFIG_SKILLS, "enchant_points_at_100", CONFTYPE_DOUBLE, "number of scale points for enchanting/augments at level 100");
	init_config(CONFIG_SKILLS, "min_exp_to_roll_skillup", CONFTYPE_INT, "minimum skill experience to roll for gains");
	init_config(CONFIG_SKILLS, "must_be_vampire", CONFTYPE_SHORT_STRING, "message for doing vampire things while not a vampire");
	init_config(CONFIG_SKILLS, "skill_swap_allowed", CONFTYPE_BOOL, "enables skill swap (immortals always can)");
	init_config(CONFIG_SKILLS, "skill_swap_min_level", CONFTYPE_INT, "level to use multi-spec");
	init_config(CONFIG_SKILLS, "summon_npc_limit", CONFTYPE_INT, "npc amount that blocks summons");

	// system
	init_config(CONFIG_SYSTEM, "autoreboot_minutes", CONFTYPE_INT, "minutes to schedule an automatic reboot at startup (0 if not using this)");
	init_config(CONFIG_SYSTEM, "log_eof_on_socket_read", CONFTYPE_BOOL, "if enabled, logs a warning if perform_socket_read gets an EOF, in case of problems");
	init_config(CONFIG_SYSTEM, "nameserver_is_slow", CONFTYPE_BOOL, "if enabled, system will not resolve numeric IPs");
	init_config(CONFIG_SYSTEM, "max_filesize", CONFTYPE_INT, "maximum size of bug, typo and idea files in bytes (to prevent bombing)");
	init_config(CONFIG_SYSTEM, "max_bad_pws", CONFTYPE_INT, "maximum number of password attempts before disconnection");
	init_config(CONFIG_SYSTEM, "reboot_warning_minutes", CONFTYPE_INT, "minutes before a reboot that players start getting alerts");
	init_config(CONFIG_SYSTEM, "use_autowiz", CONFTYPE_BOOL, "if on, automatically generates the wizlist");
	init_config(CONFIG_SYSTEM, "siteok_everyone", CONFTYPE_BOOL, "flags players siteok on creation, essentially inverting ban logic");
	init_config(CONFIG_SYSTEM, "log_losing_descriptor_without_char", CONFTYPE_BOOL, "somewhat spammy disconnect logs");

	// trade
	init_config(CONFIG_TRADE, "imports_per_day", CONFTYPE_INT, "how many max items an empire will import per 30 minute cycle");
	init_config(CONFIG_TRADE, "trading_post_max_hours", CONFTYPE_INT, "how long a trade can be posted for");
	init_config(CONFIG_TRADE, "trading_post_days_to_timeout", CONFTYPE_INT, "number of days to log in and collect");
	init_config(CONFIG_TRADE, "trading_post_fee", CONFTYPE_DOUBLE, "% cut of the sale price");
	
	// war
	init_config(CONFIG_WAR, "fire_extinguish_value", CONFTYPE_INT, "douse this value and the fire goes out");
	init_config(CONFIG_WAR, "pvp_timer", CONFTYPE_INT, "minutes from when you shut off pvp flag, until it's off");
	init_config(CONFIG_WAR, "stolen_object_timer", CONFTYPE_INT, "minutes an object is considered stolen");
	init_config(CONFIG_WAR, "hostile_flag_time", CONFTYPE_INT, "minutes that a person is killable after hostile/stealth actions");
	init_config(CONFIG_WAR, "death_release_minutes", CONFTYPE_INT, "minutes a person can sit without respawning");
	init_config(CONFIG_WAR, "deaths_before_penalty", CONFTYPE_INT, "how many times you can die in a short period without being penalized");
	init_config(CONFIG_WAR, "deaths_before_penalty_war", CONFTYPE_INT, "death limit while at war");
	init_config(CONFIG_WAR, "hostile_login_delay", CONFTYPE_INT, "seconds a person is stunned if they log in in enemy territory");
	init_config(CONFIG_WAR, "mutual_war_only", CONFTYPE_BOOL, "allows 'battle' but not 'war' diplomacy");
	init_config(CONFIG_WAR, "offense_min_to_war", CONFTYPE_INT, "number of offense points required before war is allowed");
	init_config(CONFIG_WAR, "offenses_for_free_war", CONFTYPE_INT, "number of offense points at which war cost drops to zero");
	init_config(CONFIG_WAR, "rogue_flag_time", CONFTYPE_INT, "in minutes, acts like hostile flag but for non-empire players");
	init_config(CONFIG_WAR, "pk_mode", CONFTYPE_BITVECTOR, "what conditions let one player attack another");
		init_config_custom("pk_mode", config_show_bitvector, config_edit_bitvector, pk_modes);
	init_config(CONFIG_WAR, "seconds_per_death", CONFTYPE_INT, "how long the penalty lasts per death over the limit");
	init_config(CONFIG_WAR, "steal_death_penalty", CONFTYPE_INT, "minutes a player cannot steal from an empire after being killed by them, 0 for none");
	init_config(CONFIG_WAR, "stun_immunity_time", CONFTYPE_INT, "seconds a person is immune to stuns after a stun wears off");
	init_config(CONFIG_WAR, "vehicle_siege_time", CONFTYPE_INT, "seconds between catapult/vehicle firing");
	init_config(CONFIG_WAR, "war_cost_max", CONFTYPE_INT, "empire coins to start a war at maximum level difference (not counting offenses)");
	init_config(CONFIG_WAR, "war_cost_min", CONFTYPE_INT, "empire coins to start a war at minimum level difference (not counting offenses)");
	init_config(CONFIG_WAR, "war_login_delay", CONFTYPE_INT, "seconds a person is stunned if they log in while at war");

	// world
	init_config(CONFIG_WORLD, "adjust_instance_limits", CONFTYPE_BOOL, "raises/lowers instance counts based on world size");
	init_config(CONFIG_WORLD, "default_interior", CONFTYPE_INT, "building room vnum to use for designate");
		init_config_custom("default_interior", config_show_building, config_edit_building, NULL);
	init_config(CONFIG_WORLD, "water_crop_distance", CONFTYPE_INT, "distance at which a crop marked requires-water can be planted from one");
	init_config(CONFIG_WORLD, "naturalize_newbie_islands", CONFTYPE_BOOL, "returns the newbie islands to nature when depletions reset");
	init_config(CONFIG_WORLD, "naturalize_unclaimable", CONFTYPE_BOOL, "if true, naturalize/remember will also work on unclaimable tiles");
	init_config(CONFIG_WORLD, "nearby_sector_distance", CONFTYPE_INT, "distance for the near-sector evolution");
	init_config(CONFIG_WORLD, "ignore_island_levels", CONFTYPE_BOOL, "instancer will ignore island levels when spawning adventures");
	init_config(CONFIG_WORLD, "interlink_distance", CONFTYPE_INT, "how far apart two interlinked buildings can be");
	init_config(CONFIG_WORLD, "interlink_river_limit", CONFTYPE_INT, "how many intervening tiles may be river");
	init_config(CONFIG_WORLD, "interlink_mountain_limit", CONFTYPE_INT, "how many intervening tiles may be mountain");
	init_config(CONFIG_WORLD, "newbie_adventure_cap", CONFTYPE_INT, "highest adventure min-level that can spawn on newbie islands");
	init_config(CONFIG_WORLD, "no_redundant_island_names", CONFTYPE_BOOL, "prevents player-set island names from starting with Island, Isle, The Island, or The Isle");
	init_config(CONFIG_WORLD, "use_local_time", CONFTYPE_BOOL, "each part of the world gets its own time zone instead of one global time");
	init_config(CONFIG_WORLD, "vehicle_max_per_tile", CONFTYPE_INT, "maximum number of zero-size vehicles per tile");
	init_config(CONFIG_WORLD, "vehicle_size_per_tile", CONFTYPE_INT, "maximum total vehicle size on any map tile");
	
	// TODO sector types should be audited on startup to ensure they exist -pc
	init_config(CONFIG_WORLD, "default_building_sect", CONFTYPE_INT, "vnum of sector used by build command");
		init_config_custom("default_building_sect", config_show_sector, config_edit_sector, NULL);
	init_config(CONFIG_WORLD, "default_inside_sect", CONFTYPE_INT, "vnum of sector used by designate command");
		init_config_custom("default_inside_sect", config_show_sector, config_edit_sector, NULL);
	init_config(CONFIG_WORLD, "default_adventure_sect", CONFTYPE_INT, "vnum of sector used by instancing system");
		init_config_custom("default_adventure_sect", config_show_sector, config_edit_sector, NULL);
	init_config(CONFIG_WORLD, "default_land_sect", CONFTYPE_INT, "vnum of sector for basic land");
		init_config_custom("default_land_sect", config_show_sector, config_edit_sector, NULL);

	// deprecated configs: no longer used/shown
	deprecated_config(CONFIG_GAME, "newyear_message", CONFTYPE_SHORT_STRING);
	deprecated_config(CONFIG_ACTIONS, "chop_depletion", CONFTYPE_INT);
	deprecated_config(CONFIG_ACTIONS, "garden_depletion", CONFTYPE_INT);
	deprecated_config(CONFIG_ACTIONS, "gather_depletion", CONFTYPE_INT);
	deprecated_config(CONFIG_ACTIONS, "pick_depletion", CONFTYPE_INT);
	deprecated_config(CONFIG_ACTIONS, "max_chore_resource", CONFTYPE_INT);
	deprecated_config(CONFIG_ACTIONS, "max_chore_resource_skilled", CONFTYPE_INT);
	deprecated_config(CONFIG_ACTIONS, "tavern_brew_time", CONFTYPE_INT);
	deprecated_config(CONFIG_ACTIONS, "tavern_timer", CONFTYPE_INT);
	deprecated_config(CONFIG_ACTIONS, "trench_full_value", CONFTYPE_INT);
	deprecated_config(CONFIG_CITY, "bonus_city_point_techs", CONFTYPE_INT);
	deprecated_config(CONFIG_CITY, "bonus_city_point_wealth", CONFTYPE_INT);
	deprecated_config(CONFIG_EMPIRE, "land_per_tech", CONFTYPE_INT);
	deprecated_config(CONFIG_EMPIRE, "land_per_wealth", CONFTYPE_DOUBLE);
	deprecated_config(CONFIG_SKILLS, "potion_apply_per_100", CONFTYPE_DOUBLE);
	deprecated_config(CONFIG_SKILLS, "potion_heal_scale", CONFTYPE_DOUBLE);
	deprecated_config(CONFIG_WAR, "burn_down_time", CONFTYPE_INT);
	deprecated_config(CONFIG_WORLD, "generic_facing", CONFTYPE_BITVECTOR);
	deprecated_config(CONFIG_WORLD, "arctic_percent", CONFTYPE_DOUBLE);
	deprecated_config(CONFIG_WORLD, "tropics_percent", CONFTYPE_DOUBLE);
	deprecated_config(CONFIG_WORLD, "ocean_pool_size", CONFTYPE_INT);

	// last
	load_config_system_from_file();
}


 //////////////////////////////////////////////////////////////////////////////
//// CONFIG SYSTEM: COMMAND //////////////////////////////////////////////////

ACMD(do_config) {
	char output[MAX_STRING_LENGTH*2], line[MAX_STRING_LENGTH], part[MAX_INPUT_LENGTH];
	char *val_arg = NULL, arg1[MAX_INPUT_LENGTH], arg2[MAX_INPUT_LENGTH];
	struct config_type *cnf = NULL, *next_cnf;
	int set = NOTHING, iter, size, lsize, count;
	bool verbose = FALSE;
	
	// basic safety
	if (IS_NPC(ch) || !ch->desc) {
		msg_to_char(ch, "You can't do that right now.\r\n");
		return;
	}
	
	// arg 1: may be <type>, may be <key>, may be nothing
	argument = any_one_word(argument, arg1);
	skip_spaces(&argument);
	
	if (*arg1 && (set = search_block(arg1, config_groups, FALSE)) != NOTHING) {
		// will use: <type> [key] [value]
		verbose = (!str_cmp(argument, "-v") ? TRUE : FALSE);
		
		// or, for backwards-compatibility, may also be: <type> <key> [value]
		if (!verbose && *argument) {
			val_arg = any_one_word(argument, arg2);
			skip_spaces(&val_arg);
			if (!(cnf = get_config_by_key(arg2, (*argument && !verbose) ? TRUE : FALSE)) || cnf->set != set) {
				msg_to_char(ch, "Invalid key %s in %s configs (keys must be exact).\r\n", arg2, config_groups[set]);
				return;
			}
		}
	}
	else if (*arg1 && (cnf = get_config_by_key(arg1, (*argument ? TRUE : FALSE)))) {
		// now accepts abbrevs ONLY if there's no value
		// will use: <key> [value]
		val_arg = argument;
	}
	else {	// no arg or invalid arg
		if (*arg1) {
			msg_to_char(ch, "Invalid config type or key '%s' (keys must be exact).\r\n", arg1);
		}
		
		msg_to_char(ch, "Usage: config <type> [-v]\r\n");
		msg_to_char(ch, "Usage: config <key> [value]\r\n");
		msg_to_char(ch, "Valid types:\r\n");
		count = 0;
		for (iter = 0; *config_groups[iter] != '\n'; ++iter) {
			msg_to_char(ch, " %-22.22s%s", config_groups[iter], (PRF_FLAGGED(ch, PRF_SCREEN_READER) || !(++count % 3)) ? "\r\n" : "");
		}
		if (!PRF_FLAGGED(ch, PRF_SCREEN_READER) && count % 3) {
			msg_to_char(ch, "\r\n");
		}
		return;
	}
	
	// show whole set?
	if (set != NOTHING && (verbose || !cnf)) {
		// only set given: display that set
		size = snprintf(output, sizeof(output), "%s configs:\r\n", config_groups[set]);
		HASH_ITER(hh, config_table, cnf, next_cnf) {
			if (cnf->set != set || IS_SET(cnf->config_flags, CONF_FLAG_DEPRECATED)) {
				continue;
			}
			
			// CONFTYPE_x for displays (must set line)
			lsize = 0;
			switch (cnf->type) {
				case CONFTYPE_BITVECTOR: {
					if (cnf->custom_data) {
						sprintbit(cnf->data.bitvector_val, (const char **)cnf->custom_data, part, TRUE);
					}
					else {
						strcpy(part, bitv_to_alpha(cnf->data.bitvector_val));
					}
					lsize = snprintf(line, sizeof(line), "%s", part);
					break;
				}
				case CONFTYPE_BOOL: {
					lsize = snprintf(line, sizeof(line), "%s", cnf->data.bool_val ? "true" : "false");
					break;
				}
				case CONFTYPE_DOUBLE: {
					lsize = snprintf(line, sizeof(line), "%f", cnf->data.double_val);
					break;
				}
				case CONFTYPE_INT: {
					lsize = snprintf(line, sizeof(line), "%d", cnf->data.int_val);
					break;
				}
				case CONFTYPE_INT_ARRAY: {
					for (iter = 0; iter < cnf->data_size; ++iter) {
						lsize += snprintf(line + lsize, sizeof(line) - lsize, "%d ", cnf->data.int_array[iter]);
					}
					// remove trailing space?
					if (iter > 0 && lsize > 0 && line[lsize - 1] == ' ') {
						line[--lsize] = '\0';
					}
					break;
				}
				case CONFTYPE_SHORT_STRING: {
					lsize = snprintf(line, sizeof(line), "%s", cnf->data.string_val);
					break;
				}
				case CONFTYPE_LONG_STRING: {
					lsize = snprintf(line, sizeof(line), "<%s>", cnf->data.string_val ? "set" : "not set");
					break;
				}
				case CONFTYPE_TYPE: {
					if (cnf->custom_data) {
						sprinttype(cnf->data.int_val, (const char **)cnf->custom_data, part, sizeof(part), "???");
						lsize = snprintf(line, sizeof(line), "%s", part);
					}
					else {
						lsize = snprintf(line, sizeof(line), "%d", cnf->data.int_val);
					}
					break;
				}
				default: {
					syslog(SYS_ERROR, GET_INVIS_LEV(ch), TRUE, "SYSERR: config: %s: unable to display unknown type %d", cnf->key, cnf->type);
					lsize = snprintf(line, sizeof(line), "ERROR");
					break;
				}
			}
			
			if (verbose && cnf->description && *cnf->description) {
				lsize += snprintf(line + lsize, sizeof(line) - lsize, " &c%s&0", cnf->description);
			}
			
			size += snprintf(output + size, sizeof(output) - size, "&y%s&0: %s\r\n", cnf->key, line);
			
			// hit limit
			if (size >= sizeof(output)) {
				break;
			}
		}
		
		page_string(ch->desc, output, TRUE);
		return;
	}
	else if (cnf) {
		// player is viewing or changing a cnf (val_arg will be set if changing)
		
		if (*val_arg) {
			// any argument: edit
			if (cnf->edit_func != NULL) {
				(cnf->edit_func)(ch, cnf, val_arg);
			}
			else {
				msg_to_char(ch, "Editing is not implemented for the %s config.\r\n", cnf->key);
			}	
		}
		else {
			// no argument: show
			msg_to_char(ch, "&y%s&0: &c%s&0\r\n", cnf->key, cnf->description);
			
			if (cnf->show_func != NULL) {
				(cnf->show_func)(ch, cnf, val_arg);
			}
			else {
				msg_to_char(ch, "Detail is not implemented for the %s config.\r\n", cnf->key);
			}
		}
	}
	else {
		// unknown arg?
		msg_to_char(ch, "Invalid config command.\r\n");
	}
}
