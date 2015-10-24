/* ************************************************************************
*   File: plrconv-20b3-to-ascii.c                         EmpireMUD 2.0b3 *
*  Usage: convert player file to the ascii version                        *
*                                                                         *
*  This converter updates playerfiles which were running 2.0b3 to the     *
*  ascii version. It also merges in lore, variables, inventory, and       *
*  aliases. YOU SHOULD ONLY USE THIS IF YOUR GAME WAS ON 2.0b3 AND YOU    *
*  HAVE UPDATED AND COMPILED THE CURRENT VERSION.                         *
*                                                                         *
*  Instructions:                                                          *
*  1. Shut down your mud with 'shutdown die'.                             *
*  2. Create a backup of your game.                                       *
*  3. Compile this code.                                                  *
*  4. cd EmpireMUD/ -- Be sure to run this from the mud's home dir        *
*  5. ./bin/plrconv-20b3-to-ascii                                         *
*  6. Start up your mud.                                                  *
*  7. If you have trouble with this update, email paul@empiremud.net      *
*                                                                         *
*  Credits: This is looseley based on play2to3.c which is included in     *
*       CircleMUD 3.0 pbl 11, written by Edward Almasy, almasy@axis.com.  *
*       It has been tossed around rigorously, if you are going to use     *
*       it, heed the warning above.                                       *
*                                                                         *
*  EmpireMUD code base by Paul Clarke, (C) 2000-2015                      *
*  All rights reserved.  See license.doc for complete information.        *
*                                                                         *
*  EmpireMUD based upon CircleMUD 3.0, bpl 17, by Jeremy Elson.           *
*  CircleMUD (C) 1993, 94 by the Trustees of the Johns Hopkins University *
*  CircleMUD is based on DikuMUD, Copyright (C) 1990, 1991.               *
************************************************************************ */

/**
* Contents:
*   GLOBAL DATA AND CONSTANTS
*   VERSION 2.0b3 STRUCTS
*   HELPERS
*   CONVERTER CODE
*/

#include <stdio.h>
#include <time.h>
#include <sys/types.h>

#include "../conf.h"
#include "../sysdep.h"

#include "../uthash.h"
#include "../structs.h"
#include "../utils.h"


 //////////////////////////////////////////////////////////////////////////////
//// GLOBAL DATA AND CONSTANTS ///////////////////////////////////////////////

// local data
account_data *account_table = NULL;	// must build lists
int top_account_id = 0;	// for generating accounts

const char *attribute_types[] = {
	"Strength",
	"Dexterity",
	"Charisma",
	"Greatness",
	"Intelligence",
	"Wits",
	"\n"
};

const char *condition_types[] = {
	"Drunk",
	"Full",
	"Thirst",
	"\n"
};

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
	"\n"
};

const char *genders[] = {
	"neutral",
	"male",
	"female",
	"\n"
};

const char *pool_types[] = {
	"health",
	"move",
	"mana",
	"blood",
	"\n"
};


 //////////////////////////////////////////////////////////////////////////////
//// VERSION 2.0b3 STRUCTS ///////////////////////////////////////////////////

#define b3_PLR_FROZEN  BIT(0)
#define b3_PLR_SITEOK  BIT(4)
#define b3_PLR_MUTED  BIT(5)
#define b3_PLR_NOTITLE  BIT(6)
#define b3_PLR_DELETED  BIT(7)
#define b3_PLR_MULTIOK  BIT(15)

#define b3_MAX_NAME_LENGTH  20
#define b3_MAX_TITLE_LENGTH  100
#define b3_MAX_PLAYER_DESCRIPTION  4000
#define b3_MAX_PROMPT_SIZE  120
#define b3_MAX_POOFIN_LENGTH  80
#define b3_MAX_PWD_LENGTH  32
#define b3_NUM_ATTRIBUTES  6
#define b3_MAX_AFFECT  32
#define b3_MAX_COOLDOWNS  32
#define b3_MAX_HOST_LENGTH  45
#define b3_NUM_POOLS  4
#define b3_TOTAL_EXTRA_ATTRIBUTES  20
#define b3_MAX_REFERRED_BY_LENGTH  32
#define b3_MAX_ADMIN_NOTES_LENGTH  240
#define b3_NUM_CONDS  3
#define b3_MAX_MATERIALS  20
#define b3_MAX_IGNORES  15
#define b3_MAX_SLASH_CHANNELS  20
#define b3_MAX_SLASH_CHANNEL_NAME_LENGTH  16
#define b3_MAX_CUSTOM_COLORS  10
#define b3_MAX_REWARDS_PER_DAY  5
#define b3_NUM_ACTION_VNUMS  3
#define b3_MAX_SKILLS  24
#define b3_MAX_ABILITIES  400
#define b3_MAX_DISGUISED_NAME_LENGTH  32

struct b3_char_special_data_saved {
	int idnum;	// player's idnum; -1 for mobiles
	bitvector_t act;	// mob flag for NPCs; player flag for PCs

	bitvector_t injuries;	// Bitvectors including damage to the player

	bitvector_t affected_by;	// Bitvector for spells/skills affected by
};

struct b3_player_ability_data {
	bool purchased;	// whether or not the player bought it
	byte levels_gained;	// tracks for the cap on how many times one ability grants skillups
};

struct b3_player_skill_data {
	byte level;	// current skill level (0-100)
	double exp;	// experience gained in skill (0-100)
	byte resets;	// resets available
	bool noskill;	// if TRUE, do not gain
};

struct b3_player_special_data_saved {
	// account and player stuff
	int account_id;	// all characters with the same account_id belong to the same player
	char creation_host[b3_MAX_HOST_LENGTH+1];	// host they created from
	char referred_by[b3_MAX_REFERRED_BY_LENGTH];	// as entered in character creation
	char admin_notes[b3_MAX_ADMIN_NOTES_LENGTH];	// for admin to leave notes about a player
	byte invis_level;	// level of invisibility
	byte immortal_level;	// stored so that if level numbers are changed, imms stay at the correct level
	bitvector_t grants;	// grant imm abilities
	bitvector_t syslogs;	// which syslogs people want to see
	bitvector_t bonus_traits;	// BONUS_x
	ubyte bad_pws;	// number of bad password attemps
	int promo_id;	// entry in the promo_codes table

	// preferences
	bitvector_t pref;	// preference flags for PCs.
	int last_tip;	// for display_tip_to_character
	byte mapsize;	// how big the player likes the map

	// empire
	empire_vnum pledge;	// Empire he's applying to
	empire_vnum empire;	// The empire this player follows (when stored to file; live players use ch->loyalty)
	byte rank;	// Rank in the empire
	
	// misc player attributes
	ubyte apparent_age;	// for vampires	
	sh_int conditions[b3_NUM_CONDS];	// Drunk, full, thirsty
	int resources[b3_MAX_MATERIALS];	// God resources
	int ignore_list[b3_MAX_IGNORES];	// players who can't message you
	char slash_channels[b3_MAX_SLASH_CHANNELS][b3_MAX_SLASH_CHANNEL_NAME_LENGTH];	// for storing /channels
	char custom_colors[b3_MAX_CUSTOM_COLORS];	// for custom channel coloring, storing the letter part of the & code ('r' for &r)

	// some daily stuff
	int daily_cycle;	// Last update cycle registered
	ubyte daily_bonus_experience;	// boosted skill gain points
	int rewarded_today[b3_MAX_REWARDS_PER_DAY];	// idnums, for ABIL_REWARD

	// action info
	int action;	// ACT_x
	int action_cycle;	// time left before an action tick
	int action_timer;	// ticks to completion (use varies)
	room_vnum action_room;	// player location
	int action_vnum[b3_NUM_ACTION_VNUMS];	// slots for storing action data (use varies)
	
	// locations and movement
	room_vnum load_room;	// Which room to place char in
	room_vnum load_room_check;	// this is the home room of the player's loadroom, used to check that they're still in the right place
	room_vnum last_room;	// useful when dead
	room_vnum tomb_room;	// location of player's chosen tomb
	byte last_direction;	// used for walls and movement
	int recent_death_count;	// for death penalty
	long last_death_time;	// for death counts
	int last_corpse_id;	// DG Scripts obj id of last corpse
	room_vnum adventure_summon_return_location;	// where to send a player back to if they're outside an adventure
	room_vnum adventure_summon_return_map;	// map check location for the return loc
	
	// olc data
	any_vnum olc_min_vnum;	// low range of olc permissions
	any_vnum olc_max_vnum;	// high range of olc permissions
	bitvector_t olc_flags;	// olc permissions
	
	// skill/ability data
	byte creation_archetype;	// this is now stored permanently so later decisions can be made based on it
	struct b3_player_skill_data skills[b3_MAX_SKILLS];
	struct b3_player_ability_data abilities[b3_MAX_ABILITIES];
	bool can_gain_new_skills;	// not required to keep skills at zero
	bool can_get_bonus_skills;	// can buy extra 75's
	sh_int skill_level;  // levels computed based on class skills
	sh_int highest_known_level;	// maximum level ever achieved (used for gear restrictions)
	sh_int last_known_level;	// set on save/quit/alt
	ubyte class_progression;	// % of the way from SPECIALTY_SKILL_CAP to CLASS_SKILL_CAP
	ubyte class_role;	// ROLE_x chosen by the player
	sh_int character_class;  // character's class as determined by top skills
	
	// tracking for specific skills
	byte confused_dir;  // people without Navigation think this dir is north
	char disguised_name[b3_MAX_DISGUISED_NAME_LENGTH];	// verbatim copy of name -- grabs custom mob names and empire names
	byte disguised_sex;	// sex of the mob you're disguised as
	sh_int morph;	// MORPH_x form
	bitvector_t mount_flags;	// flags for the stored mount
	mob_vnum mount_vnum;	// stored mount
	byte using_poison;	// poison preference for Stealth

	/* spares below for future expansion.  You can change the names from
	   'sparen' to something meaningful, but don't change the order.  */

	byte spare0;
	byte spare1;
	byte spare2;
	byte spare3;
	byte spare4;
	
	ubyte spare5;
	ubyte spare6;
	ubyte spare7;
	ubyte spare8;
	ubyte spare9;
	
	sh_int spare10;
	sh_int spare11;
	sh_int spare12;
	sh_int spare13;
	sh_int spare14;
	
	int spare15;
	int spare16;
	int spare17;
	int spare18;
	int spare19;
	
	double spare20;
	double spare21;
	double spare22;
	double spare23;
	double spare24;
	
	bitvector_t spare25; 
	bitvector_t spare26;
	bitvector_t spare27;
	bitvector_t spare28;
	bitvector_t spare29;
	
	// these are initialized to NOTHING/-1 in init_player()
	any_vnum spare30;
	any_vnum spare31;
	any_vnum spare32;
	any_vnum spare33;
	any_vnum spare34;
};

struct b3_affected_type {
	sh_int type;	// The type of spell that caused this
	int cast_by;	// player ID (positive) or mob vnum (negative)
	sh_int duration;	// For how long its effects will last
	int modifier;	// This is added to apropriate ability
	byte location;	// Tells which ability to change - APPLY_x
	bitvector_t bitvector;	// Tells which bits to set - AFF_x

	struct b3_affected_type *next;
};

struct b3_char_point_data {
	int current_pools[b3_NUM_POOLS];	// HEALTH, MOVE, MANA, BLOOD
	int max_pools[b3_NUM_POOLS];	// HEALTH, MOVE, MANA, BLOOD
	int deficit[b3_NUM_POOLS];	// HEALTH, MOVE, MANA, BLOOD
	
	int extra_attributes[b3_TOTAL_EXTRA_ATTRIBUTES];	// ATT_x (dodge, etc)
};

struct b3_over_time_effect_type {
	sh_int type;	// ATYPE_x
	int cast_by;	// player ID (positive) or mob vnum (negative)
	sh_int duration;	// time in 5-second real-updates
	sh_int damage_type;	// DAM_x type
	sh_int damage;	// amount
	sh_int stack;	// damage is multiplied by this
	sh_int max_stack;	// how high it's allowed to stack

	struct b3_over_time_effect_type *next;
};

struct b3_cooldown_data {
	sh_int type;	// any COOLDOWN_x const
	time_t expire_time;	// time at which the cooldown has expired
	
	struct b3_cooldown_data *next;	// linked list
};

struct b3_char_file_u {
	char name[b3_MAX_NAME_LENGTH+1];	// first name
	char lastname[b3_MAX_NAME_LENGTH+1];	// last name
	char title[b3_MAX_TITLE_LENGTH+1];	// character's title
	char description[b3_MAX_PLAYER_DESCRIPTION];	// player's manual description
	char prompt[b3_MAX_PROMPT_SIZE+1];	// character's prompt
	char fight_prompt[b3_MAX_PROMPT_SIZE+1];	// character's prompt for combat
	char poofin[b3_MAX_POOFIN_LENGTH+1];	// message when appearing
	char poofout[b3_MAX_POOFIN_LENGTH+1];	// message when disappearing
	byte sex;	// sex (yes, please)
	byte access_level;	// character's level
	time_t birth;	// time of birth of character
	int	played;	// number of secs played in total

	char pwd[b3_MAX_PWD_LENGTH+1];	// character's password

	struct b3_char_special_data_saved char_specials_saved;
	struct b3_player_special_data_saved player_specials_saved;
	sh_int attributes[b3_NUM_ATTRIBUTES];	// str, etc (natural)
	struct b3_char_point_data points;
	struct b3_affected_type affected[b3_MAX_AFFECT];
	struct b3_over_time_effect_type over_time_effects[b3_MAX_AFFECT];
	struct b3_cooldown_data cooldowns[b3_MAX_COOLDOWNS];

	time_t last_logon;	// time (in secs) of last logon
	char host[b3_MAX_HOST_LENGTH+1];	// host of last logon
};


 //////////////////////////////////////////////////////////////////////////////
//// HELPERS /////////////////////////////////////////////////////////////////

/**
* Gets the filename/path for various name-divided file directories.
*
* @param char *orig_name The player name.
* @param char *filename A variable to write the filename to.
* @param char *prefix The portion before the name.
* @param char *suffix THe portion after the name.
* @return int 1=success, 0=fail
*/
int get_filename(char *orig_name, char *filename, char *prefix, char *suffix) {
	const char *middle;
	char name[64], *ptr;
	
	// bad in, bad out
	if (!*orig_name || !filename || !*prefix || !*suffix) {
		return 0;
	}
	
	strcpy(name, orig_name);
	for (ptr = name; *ptr; ++ptr) {
		*ptr = LOWER(*ptr);
	}

	if (LOWER(*name) <= 'e') {
		middle = "A-E";
	}
	else if (LOWER(*name) <= 'j') {
		middle = "F-J";
	}
	else if (LOWER(*name) <= 'o')
		middle = "K-O";
	}
	else if (LOWER(*name) <= 't') {
		middle = "P-T";
	}
	else if (LOWER(*name) <= 'z') {
		middle = "U-Z";
	}
	else {
		middle = "ZZZ";
	}

	/* If your compiler gives you shit about <= '', use this switch:
		switch (LOWER(*name)) {
			case 'a':  case 'b':  case 'c':  case 'd':  case 'e': {
				middle = "A-E";
				break;
			}
			case 'f':  case 'g':  case 'h':  case 'i':  case 'j': {
				middle = "F-J";
				break;
			}
			case 'k':  case 'l':  case 'm':  case 'n':  case 'o': {
				middle = "K-O";
				break;
			}
			case 'p':  case 'q':  case 'r':  case 's':  case 't': {
				middle = "P-T";
				break;
			}
			case 'u':  case 'v':  case 'w':  case 'x':  case 'y':  case 'z': {
				middle = "U-Z";
				break;
			}
			default: {
				middle = "ZZZ";
				break;
			}
		}
	*/

	sprintf(filename, "%s%s/%s.%s", prefix, middle, name, suffix);
	return (1);
}


/**
* This converts bitvector flags into the human-readable sequence used in db
* files, e.g. "adoO", where each letter represents a bit starting with a=1.
* If there are no bits, it returns the string "0".
*
* @param bitvector_t flags The bitmask to convert to alpha.
* @return char* The resulting string.
*/
char *bitv_to_alpha(bitvector_t flags) {
	static char output[65];
	int iter, pos;
	
	pos = 0;
	for (iter = 0; flags && iter <= 64; ++iter) {
		if (IS_SET(flags, BIT(iter))) {
			output[pos++] = (iter < 26) ? ('a' + iter) : ('A' + iter - 26);
		}
		
		// remove so we exhaust flags
		REMOVE_BIT(flags, BIT(iter));
	}
	
	// empty string?
	if (pos == 0) {
		output[pos++] = '0';
	}
	
	// terminate it like Ahnold
	output[pos] = '\0';
	
	return output;
}


/* Removes the \r from \r\n to prevent Windows clients from seeing extra linebreaks in descs */
void strip_crlf(char *buffer) {
	register char *ptr, *str;

	ptr = buffer;
	str = ptr;

	while ((*str = *ptr)) {
		str++;
		ptr++;
		if (*ptr == '\r') {
			ptr++;
		}
	}
}


/**
* @param account_data *a One element
* @param account_data *b Another element
* @return int Sort instruction of -1, 0, or 1
*/
int sort_accounts(account_data *a, account_data *b) {
	return a->id - b->id;
}


// most of these helpers are copied out of the regular game code, but some have
// been modified specifically for this converter.

/**
* Add an account to the hash table.
*
* @param account_data *acct The account to add.
*/
void add_account_to_table(account_data *acct) {
	int sort_accounts(account_data *a, account_data *b);
	
	account_data *find;
	int id;
	
	if (acct) {
		id = acct->id;
		HASH_FIND_INT(account_table, &id, find);
		if (!find) {
			HASH_ADD_INT(account_table, id, acct);
			HASH_SORT(account_table, sort_accounts);
		}
	}
}


/**
* @param int id The account to look up.
* @return account_data* The account from account_table, if any (or NULL).
*/
account_data *find_account(int id) {
	account_data *acct;	
	HASH_FIND_INT(account_table, &id, acct);
	return acct;
}


/**
* Writes the account index to file.
*
* @param FILE *fl The open index file.
*/
void write_account_index(FILE *fl) {
	account_data *acct, *next_acct;
	int this, last;
	
	last = -1;
	HASH_ITER(hh, account_table, acct, next_acct) {
		// determine "zone number" by vnum
		this = (int)(acct->id / 100);
	
		if (this != last) {
			fprintf(fl, "%d.acct\n", this);
			last = this;
		}
	}
}


/**
* Outputs one account in the db file format, starting with a #ID and ending
* in an S.
*
* @param FILE *fl The file to write it to.
* @param account_data *acct The account to save.
*/
void write_account_to_file(FILE *fl, account_data *acct) {
	char temp[MAX_STRING_LENGTH];
	struct account_player *plr;
	
	fprintf(fl, "#%d\n", acct->id);
	fprintf(fl, "%ld %s\n", acct->last_logon, bitv_to_alpha(acct->flags));
	
	strcpy(temp, NULLSAFE(acct->notes));
	strip_crlf(temp);
	fprintf(fl, "%s~\n", temp);
	
	// P: player
	for (plr = acct->players; plr; plr = plr->next) {
		if (plr->player || plr->name) {
			fprintf(fl, "P %s\n", plr->player ? plr->player->name : plr->name);
		}
	}
	
	// END
	fprintf(fl, "S\n");
}


 //////////////////////////////////////////////////////////////////////////////
//// CONVERTER CODE //////////////////////////////////////////////////////////


/**
* Takes an EmpireMUD 2.0b3 char_file_u record and opens and writes a new file
* for the player. It also imports aliases, equipment, inventory, lore, and
* variables.
*
* @param struct b3_char_file_u *cfu The char file. 
*/
void convert_char(struct b3_char_file_u *cfu) {
	char charname_lower[MAX_STRING_LENGTH], buf[MAX_STRING_LENGTH], temp[MAX_STRING_LENGTH];
	struct affected_type *af, *new_af, *next_af, *af_list;
	struct account_player *acct_player, *last_acct_player;
	struct player_slash_channel *slash;
	struct over_time_effect_type *dot;
	struct slash_channel *loadslash;
	struct slash_channel *channel;
	obj_data *char_eq[NUM_WEARS];
	char temp[MAX_STRING_LENGTH];
	struct trig_var_data *vars;
	struct cooldown_data *cool;
	struct alias_data *alias;
	struct offer_data *offer;
	struct coin_data *coin;
	struct lore_data *lore;
	FILE *pfile, *loadfile;
	account_data *acct;
	int iter;
	
	// get lowercase char name
	strcpy(charname_lower, cfu->name);
	for (iter = 0; iter < strlen(charname_lower); ++iter) {
		charname_lower[iter] = LOWER(charname_lower[iter]);
	}
	
	// reasons we would skip a player entirely
	if (!*charname_lower || IS_SET(cfu->char_specials_saved.act, b3_PLR_DELETED)) {
		return;
	}
	
	if (!get_filename(charname_lower buf, "lib/players/", ".plr")) {
		printf("convert_char: Unable to get filename for player: %s", charname_lower);
		exit(1);
	}
	strcat(buf, ".temp");
	if (!(fl = fopen(buf, "w"))) {
		printf("convert_char: unable to write file: %s\n", buf);
		exit(1);
	}
	
	// account first: create account if needed (or find an existing one)
	if (cfu->player_specials_saved.account_id <= 0 || !(acct = find_account(cfu->player_specials_saved.account_id))) {
		CREATE(acct, account_data, 1);
		if (cfu->player_specials_saved.account_id <= 0) {
			acct->id = ++top_account_id;
		}
		else {
			acct->id = cfu->player_specials_saved.account_id;
		}
		
		add_account_to_table(acct);
	}
	acct->last_logon = MAX(acct->last_logon, cfu->last_logon);
	// account: add player to account ONLY if they aren't already on it (some players have duplicates)
	for (acct_player = acct->players; acct_player; acct_player = acct_player->next) {
		if (!strcmp(acct_player->name, charname_lower)) {
			// duplicate! don't import
			fclose(fl);
			return;
		}
	}
	CREATE(acct_player, struct account_player, 1);
	acct_player->name = str_dup(charname_lower);
	if ((last_acct_player = acct->players)) {
		while (last_acct_player->next) {
			last_acct_player = last_acct_player->next;
		}
		last_acct_player->next = acct_player;
	}
	else {	// no list yet
		acct->players = acct_player;
	}
	// account: import flags and remove them from the character
	if (IS_SET(cfu->char_specials_saved.act, b3_PLR_FROZEN)) {
		SET_BIT(acct->flags, ACCT_FROZEN);
	}
	if (IS_SET(cfu->char_specials_saved.act, b3_PLR_SITEOK)) {
		SET_BIT(acct->flags, ACCT_SITEOK);
	}
	if (IS_SET(cfu->char_specials_saved.act, b3_PLR_MUTED)) {
		SET_BIT(acct->flags, ACCT_MUTED);
	}
	if (IS_SET(cfu->char_specials_saved.act, b3_PLR_NOTITLE)) {
		SET_BIT(acct->flags, ACCT_NOTITLE);
	}
	if (IS_SET(cfu->char_specials_saved.act, b3_PLR_MULTIOK)) {
		SET_BIT(acct->flags, ACCT_MULTI_IP);
	}
	REMOVE_BIT(cfu->char_specials_saved.act, b3_PLR_MULTIOK | b3_PLR_DELETED | b3_PLR_NOTITLE | b3_PLR_MUTED | b3_PLR_SITEOK | b3_PLR_FROZEN);
	// account: notes -- only importing from the first player we find notes on, not each player.
	if (*cfu->player_specials_saved.admin_notes && !acct->notes) {
		acct->notes = str_dup(cfu->player_specials_saved.admin_notes);
	}
	
	// BEGIN TAGS
	fprintf(fl, "Name: %s\n", cfu->name);
	fprintf(fl, "Password: %s\n", cfu->pwd);
	fprintf(fl, "Idnum: %d\n", cfu->char_specials_saved.idnum);
	fprintf(fl, "Account: %d\n", acct->id);
	if (cfu->player_specials_saved.empire != NOTHING) {
		fprintf(fl, "Empire: %d\n", cfu->player_specials_saved.empire);
		fprintf(fl, "Rank: %d\n", cfu->player_specials_saved.rank);
	}
	else {
		if (cfu->player_specials_saved.pledge != NOTHING) {
			fprintf(fl, "Pledge Empire: %d\n", cfu->player_specials_saved.pledge);
		}
	}
	fprintf(fl, "Last Host: %s\n", cfu->host);
	fprintf(fl, "Last Logon: %ld\n", cfu->last_logon);
	for (iter = 0; iter < b3_NUM_POOLS; ++iter) {
		fprintf(fl, "Current Pool: %s %d\n", pool_types[iter], cfu->points.current_pools[iter]);
		fprintf(fl, "Max Pool: %s %d\n", pool_types[iter], cfu->points.max_pools[iter]);
	}
	for (iter = 0; iter < b3_NUM_ABILITIES; ++iter) {
		if (cfu->player_specials_saved.abilities[iter].purchased || cfu->player_specials_saved.abilities[iter].levels_gained > 0) {
			fprintf(fl, "Ability: %d %d %d\n", iter, cfu->player_specials_saved.abilities[iter].purchased ? 1 : 0, cfu->player_specials_saved.abilities[iter].levels_gained);
		}
	}
	fprintf(fl, "Access Level: %d\n", cfu->access_level);
	if (cfu->player_specials_saved.action != ACT_NONE) {
		fprintf(fl, "Action: %d %d %d %d\n", cfu->player_specials_saved.action, cfu->player_specials_saved.action_cycle, cfu->player_specials_saved.action_timer, cfu->player_specials_saved.action_room);
		for (iter = 0; iter < b3_NUM_ACTION_VNUMS; ++iter) {
			fprintf(fl, "Action-vnum: %d %d\n", iter, cfu->player_specials_saved.action_vnum[iter]);
		}
	}
	if (cfu->player_specials_saved.adventure_summon_return_location != NOWHERE) {
		fprintf(fl, "Adventure Summon Loc: %d\n", cfu->player_specials_saved.adventure_summon_return_location);
		fprintf(fl, "Adventure Summon Map: %d\n", cfu->player_specials_saved.adventure_summon_return_map);
	}
	for (iter = 0; iter < b3_MAX_AFFECT; ++iter) {
		if (cfu->affected[iter].type > 0) {
			fprintf(fl, "Affect: %d %d %d %d %d %s\n", cfu->affected[iter].type, cfu->affected[iter].cast_by, cfu->affected[iter].duration, cfu->affected[iter].modifier, cfu->affected[iter].location, bitv_to_alpha(cfu->affected[iter].bitvector));
		}
	}
	fprintf(fl, "Affect Flags: %s\n", bitv_to_alpha(cfu->char_specials_saved.affected_by));
	if (cfu->player_specials_saved.apparent_age) {
		fprintf(fl, "Apparent Age: %d\n", cfu->player_specials_saved.apparent_age);
	}
	fprintf(fl, "Archetype: %d\n", cfu->player_specials_saved.creation_archetype);
	for (iter = 0; iter < b3_NUM_ATTRIBUTES; ++iter) {
		fprintf(fl, "Attribute: %s %d\n", attribute_types[iter], cfu->attributes[iter]);
	}
	if (cfu->player_specials_saved.bad_pws) {
		fprintf(fl, "Bad passwords: %d\n", cfu->player_specials_saved.bad_pws);
	}
	fprintf(fl, "Birth: %ld\n", cfu->birth);
	fprintf(fl, "Bonus Exp: %d\n", cfu->player_specials_saved.daily_bonus_experience);
	fprintf(fl, "Bonus Traits: %s\n", bitv_to_alpha(cfu->player_specials_saved.bonus_traits));
	
	// 'C'
	if (cfu->player_specials_saved.can_gain_new_skills) {
		fprintf(fl, "Can Gain New Skills: 1\n");
	}
	if (cfu->player_specials_saved.can_get_bonus_skills) {
		fprintf(fl, "Can Get Bonus Skills: 1\n");
	}
	fprintf(fl, "Class: %d\n", cfu->player_specials_saved.character_class);
	fprintf(fl, "Class Progression: %d\n", cfu->player_specials_saved.class_progression);
	fprintf(fl, "Class Role: %d\n", cfu->player_specials_saved.class_role);
	for (iter = 0; iter < b3_NUM_CUSTOM_COLORS; ++iter) {
		if (cfu->player_specials_saved.custom_colors[iter] > 0) {
			fprintf(fl, "Color: %s %c\n", custom_color_types[iter], cfu->player_specials_saved.custom_colors[iter]);
		}
	}
	for (iter = 0; iter < b3_NUM_CONDS; ++iter) {
		if (cfu->player_specials_saved.conditions[iter] != 0) {
			fprintf(fl, "Condition: %s %d\n", condition_types[iter], cfu->player_specials_saved.conditions[iter]);
		}
	}
	if (cfu->player_specials_saved.confused_dir != NO_DIR) {
		fprintf(fl, "Confused Direction: %d\n", cfu->player_specials_saved.confused_dir);
	}
	for (iter = 0; iter < b3_MAX_COOLDOWNS; ++iter) {
		if (cfu->cooldowns[iter].type > 0) {
			fprintf(fl, "Cooldown: %d %ld\n", cfu->cooldowns[iter].type, cfu->cooldowns[iter].expire_time);
		}
	}
	if (*cfu->player_specials_saved.creation_host) {
		fprintf(fl, "Creation Host: %s\n", cfu->player_specials_saved.creation_host);
	}
	fprintf(fl, "Daily Cycle: %d\n", cfu->player_specials_saved.daily_cycle);
	if (*cfu->description) {
		strcpy(temp, NULLSAFE(cfu->description));
		strip_crlf(temp);
		fprintf(fl, "Description:\n%s~\n", temp);
	}
	if (*cfu->player_specials_saved.disguised_name) {
		fprintf(fl, "Disguised Name: %s\n", cfu->player_specials_saved.disguised_name);
	}
	if (cfu->player_specials_saved.disguised_sex) {
		fprintf(fl, "Disguised Sex: %s\n", genders[(int) cfu->player_specials_saved.disguised_sex]);
	}
	for (iter = 0; iter < b3_MAX_AFFECT; ++iter) {
		if (cfu->over_time_effects[iter].type > 0) {
			fprintf(fl, "DoT Effect: %d %d %d %d %d %d %d\n", cfu->over_time_effects[iter].type, cfu->over_time_effects[iter].cast_by, cfu->over_time_effects[iter].duration, cfu->over_time_effects[iter].damage_type, cfu->over_time_effects[iter].damage, cfu->over_time_effects[iter].stack, cfu->over_time_effects[iter].max_stack);
		}
	}
	for (iter = 0; iter < NUM_EXTRA_ATTRIBUTES; ++iter) {
		if (cfu->points.extra_attributes[iter] > 0) {
			fprintf(fl, "Extra Attribute: %s %d\n", extra_attribute_types[iter], cfu->points.extra_attributes[iter]);
		}
	}
	if (*cfu->fight_prompt) {
		fprintf(fl, "Fight Prompt: %s\n", cfu->fight_prompt);
	}
	if (cfu->player_specials_saved.grants) {
		fprintf(fl, "Grants: %s\n", bitv_to_alpha(cfu->player_specials_saved.grants));
	}
	fprintf(fl, "Highest Known Level: %d\n", cfu->player_specials_saved.highest_known_level);
	for (iter = 0; iter < b3_MAX_IGNORES; ++iter) {
		if (cfu->player_specials_saved.ignore_list[iter] > 0) {
			fprintf(fl, "Ignore: %d\n", cfu->player_specials_saved.ignore_list[iter]);
		}
	}
	if (cfu->player_specials_saved.immortal_level) {
		fprintf(fl, "Immortal Level: %d\n", cfu->player_specials_saved.immortal_level);
	}
	fprintf(fl, "Injuries: %s\n", bitv_to_alpha(cfu->char_specials_saved.injuries));
	if (cfu->player_specials_saved.invis_level) {
		fprintf(fl, "Invis Level: %d\n", cfu->player_specials_saved.invis_level);
	}
	if (cfu->player_specials_saved.last_corpse_id > 0) {
		fprintf(fl, "Last Corpse Id: %d\n", cfu->player_specials_saved.last_corpse_id);
	}
	fprintf(fl, "Last Death: %ld\n", cfu->player_specials_saved.last_death_time);
	fprintf(fl, "Last Direction: %d\n", cfu->player_specials_saved.last_direction);
	fprintf(fl, "Last Known Level: %d\n", cfu->player_specials_saved.last_known_level);
	fprintf(fl, "Last Room: %d\n", cfu->player_specials_saved.last_room);
	if (cfu->player_specials_saved.last_tip) {
		fprintf(fl, "Last Tip: %d\n", cfu->player_specials_saved.last_tip);
	}
	if (*cfu->lastname) {
		fprintf(fl, "Lastname: %s\n", cfu->lastname);
	}
	fprintf(fl, "Load Room: %d\n", cfu->player_specials_saved.load_room);
	fprintf(fl, "Load Room Check: %d\n", cfu->player_specials_saved.load_room_check);
	if (cfu->player_specials_saved.mapsize) {
		fprintf(fl, "Mapsize: %d\n", cfu->player_specials_saved.mapsize);
	}
	if (cfu->player_specials_saved.morph != MORPH_NONE) {
		fprintf(fl, "Morph: %d\n", cfu->player_specials_saved.morph);
	}
	if (GET_MOUNT_FLAGS(ch) != NOBITS) {
		fprintf(fl, "Mount Flags: %s\n", bitv_to_alpha(GET_MOUNT_FLAGS(ch)));
	}
	if (GET_MOUNT_VNUM(ch) != NOTHING) {
		fprintf(fl, "Mount Vnum: %d\n", GET_MOUNT_VNUM(ch));
	}
	
	// 'N'
	if (GET_ADMIN_NOTES(ch)) {
		strcpy(temp, NULLSAFE(GET_ADMIN_NOTES(ch)));
		strip_crlf(temp);
		fprintf(fl, "Notes:\n%s~\n", temp);
	}
	
	// 'O'
	for (offer = GET_OFFERS(ch); offer; offer = offer->next) {
		fprintf(fl, "Offer: %d %d %d %ld %d\n", offer->from, offer->type, offer->location, offer->time, offer->data);
	}
	if (GET_OLC_MAX_VNUM(ch) > 0 || GET_OLC_MIN_VNUM(ch) > 0 || GET_OLC_FLAGS(ch) != NOBITS) {
		fprintf(fl, "OLC: %d %d %s\n", GET_OLC_MIN_VNUM(ch), GET_OLC_MAX_VNUM(ch), bitv_to_alpha(GET_OLC_FLAGS(ch)));
	}
	
	// 'P'
	fprintf(fl, "Played: %d\n", ch->player.time.played);
	fprintf(fl, "Player Flags: %s\n", bitv_to_alpha(PLR_FLAGS(ch)));
	if (POOFIN(ch)) {
		fprintf(fl, "Poofin: %s\n", POOFIN(ch));
	}
	if (POOFOUT(ch)) {
		fprintf(fl, "Poofout: %s\n", POOFOUT(ch));
	}
	if (PRF_FLAGS(ch)) {
		fprintf(fl, "Preferences: %s\n", bitv_to_alpha(PRF_FLAGS(ch)));
	}
	if (GET_PROMO_ID(ch)) {
		fprintf(fl, "Promo ID: %d\n", GET_PROMO_ID(ch));
	}
	if (GET_PROMPT(ch)) {
		fprintf(fl, "Prompt: %s\n", GET_PROMPT(ch));
	}
	
	// 'R'
	if (GET_RECENT_DEATH_COUNT(ch)) {
		fprintf(fl, "Recent Deaths: %d\n", GET_RECENT_DEATH_COUNT(ch));
	}
	if (GET_REFERRED_BY(ch)) {
		fprintf(fl, "Referred by: %s\n", GET_REFERRED_BY(ch));
	}
	for (iter = 0; iter < NUM_MATERIALS; ++iter) {
		if (GET_RESOURCE(ch, iter) != 0) {
			fprintf(fl, "Resource: %d %s\n", GET_RESOURCE(ch, iter), materials[iter].name);
		}
	}
	for (iter = 0; iter < MAX_REWARDS_PER_DAY; ++iter) {
		if (GET_REWARDED_TODAY(ch, iter) > 0) {
			fprintf(fl, "Rewarded: %d\n", GET_REWARDED_TODAY(ch, iter));
		}
	}
	
	// 'S'
	fprintf(fl, "Sex: %s\n", genders[(int) GET_REAL_SEX(ch)]);
	for (iter = 0; iter < NUM_SKILLS; ++iter) {
		fprintf(fl, "Skill: %d %d %.2f %d %d\n", iter, GET_SKILL(ch, iter), GET_SKILL_EXP(ch, iter), GET_FREE_SKILL_RESETS(ch, iter), NOSKILL_BLOCKED(ch, iter) ? 1 : 0);
	}
	fprintf(fl, "Skill Level: %d\n", GET_SKILL_LEVEL(ch));
	for (slash = GET_SLASH_CHANNELS(ch); slash; slash = slash->next) {
		if ((channel = find_slash_channel_by_id(slash->id))) {
			fprintf(fl, "Slash-channel: %s\n", channel->name);
		}
	}
	for (loadslash = LOAD_SLASH_CHANNELS(ch); loadslash; loadslash = loadslash->next) {
		if (loadslash->name) {
			// these are half-loaded slash channels and save in the same way
			fprintf(fl, "Slash-channel: %s\n", loadslash->name);
		}
	}
	if (SYSLOG_FLAGS(ch)) {
		fprintf(fl, "Syslog Flags: %s\n", bitv_to_alpha(SYSLOG_FLAGS(ch)));
	}
	
	// 'T'
	if (GET_TITLE(ch)) {
		fprintf(fl, "Title: %s\n", GET_TITLE(ch));
	}
	if (GET_TOMB_ROOM(ch) != NOWHERE) {
		fprintf(fl, "Tomb Room: %d\n", GET_TOMB_ROOM(ch));
	}
	
	// 'U'
	if (USING_POISON(ch)) {
		fprintf(fl, "Using Poison: %d\n", USING_POISON(ch));
	}
	
	// END MAIN TAGS: the rest of the data is delay-loaded
	fprintf(fl, "End Primary Data\n");
	
	// delayed: alias
	for (alias = GET_ALIASES(ch); alias; alias = alias->next) {
		fprintf(fl, "Alias: %d %ld %ld\n%s\n%s\n", alias->type, strlen(alias->alias), strlen(alias->replacement)-1, alias->alias, alias->replacement + 1);
	}
	
	// delayed: coin
	for (coin = GET_PLAYER_COINS(ch); coin; coin = coin->next) {
		fprintf(fl, "Coin: %d %d %ld\n", coin->amount, coin->empire_id, coin->last_acquired);
	}
	
	// delayed: lore
	for (lore = GET_LORE(ch); lore; lore = lore->next) {
		if (lore->text && *lore->text) {
			fprintf(fl, "Lore: %d %ld\n%s\n", lore->type, lore->date, lore->text);
		}
	}
	
	// delayed: script variables
	if (SCRIPT(ch) && SCRIPT(ch)->global_vars) {
		for (vars = SCRIPT(ch)->global_vars; vars; vars = vars->next) {
			if (*vars->name == '-') { // don't save if it begins with -
				continue;
			}
			
			fprintf(fl, "Variable: %s %ld\n%s\n", vars->name, vars->context, vars->value);
		}
	}
	
	// delayed: equipment
	for (iter = 0; iter < NUM_WEARS; ++iter) {
		if (char_eq[iter]) {
			Crash_save(char_eq[iter], fl, iter + 1);	// save at iter+1 because 0 == LOC_INVENTORY
		}
	}
	
	// delayed: inventory
	Crash_save(ch->carrying, fl, LOC_INVENTORY);
	
	// END DELAY-LOADED SECTION
	fprintf(fl, "End Player File\n");
	
	// re-apply: affects
	for (af = af_list; af; af = next_af) {
		next_af = af->next;
		affect_to_char(ch, af);
		free(af);
	}
	
	// re-apply: equipment
	for (iter = 0; iter < NUM_WEARS; ++iter) {
		if (char_eq[iter]) {
			#ifndef NO_EXTRANEOUS_TRIGGERS
				if (wear_otrigger(char_eq[iter], ch, iter)) {
			#endif
					// this line may depend on the above if
					equip_char(ch, char_eq[iter], iter);
			#ifndef NO_EXTRANEOUS_TRIGGERS
				}
				else {
					obj_to_char(char_eq[iter], ch);
				}
			#endif
		}
	}
	
	// affect_total(ch); // unnecessary, I think (?)


	// - move admin notes to acct
	// - move plr flags
	// - import lore
	// - import vars
	// - import objs
	// - import aliases
}


// main() function
int main(int argc, char *argv[]) {
	account_data *acct, *next_acct;
	struct b3_char_file_u stOld;
	FILE *ptOldHndl, *fl;
	int last_zone, this_zone;
	char fname[256];
	
	if (!(ptOldHndl = fopen("lib/etc/players", "rb"))) {
		printf("Unable to read file: lib/etc/players\n");
		exit(1);
	}
	
	// first determine top account
	while (!feof(ptOldHndl)) {
		fread(&stOld, sizeof(struct b3_char_file_u), 1, ptOldHndl);
		top_account_id = MAX(top_account_id, stOld.player_specials_saved.account_id);
	}
	rewind(ptOldHndl);
	
	while (!feof(ptOldHndl)) {
		fread(&stOld, sizeof(struct b3_char_file_u), 1, ptOldHndl);
		convert_char(&stOld);
		
		// log progress to screen
		if (stOld.access_level >= LVL_START_IMM) {
			printf("*");
		}
		else if (stOld.access_level >= LVL_APPROVED) {
			printf("-");
		}
		else {
			printf(".");
		}
	}
	
	printf("\n");
	fclose(ptOldHndl);
	
	// write the account index
	if (!(fl = fopen("lib/players/accounts/index", "w"))) {
		printf("Unable to write file: lib/players/accounts/index\n");
		exit(1);
	}
	write_account_index(fl);
	fprintf(fl, "$\n");
	fclose(fl);
	
	// write account files
	last_zone = -1;
	HASH_ITER(hh, account_table, acct, next_acct) {
		this_zone = (int)(acct->id / 100);
		
		if (last_zone != this_zone) {
			if (last_zone != -1) {
				fprintf(fl, "$\n");
				fclose(fl);
			}
			sprintf(fname, "lib/players/accounts/%d.acct", this_zone);
			if (!(fl = fopen(fname, "w"))) {
				printf("Unable to write file: %s\n", fname);
				exit(1);
			}
		}
		
		write_account_to_file(fl, acct);
	}
	if (last_zone != -1) {
		fprintf(fl, "$\n");
		fclose(fl);
	}
	
	// remove old-style files
	remove("lib/etc/players");
	remove("lib/plralias");
	remove("lib/plrlore");
	remove("lib/plrobjs");
	remove("lib/plrvars");
	
	return 0;
}
