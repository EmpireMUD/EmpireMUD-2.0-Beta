/* ************************************************************************
*   File: plrconv-20b2-to-20b3.c                          EmpireMUD 2.0b5 *
*  Usage: convert player file structures without wiping                   *
*                                                                         *
*  This converter updates playerfiles which were running 2.0b2 to the     *
*  format used in 2.0b3. YOU SHOULD ONLY USE THIS IF YOUR GAME WAS ON     *
*  2.0b2 AND YOU HAVE UPDATED AND COMPILED 2.0b3                          *
*                                                                         *
*  Instructions:                                                          *
*  1. Shut down your mud with 'shutdown die'.                             *
*  2. Create a backup of your game.                                       *
*  3. Compile version 2.0b3.                                              *
*  4. cd lib/etc                                                          *
*  5. mv players players.old                                              *
*  6. ./../../bin/plrconv-20b2-to-20b3 players.old players                *
*  7. Start up your mud. Note: if your playerfile was corrupted, the      *
*     player with ID #1 may still be able to log in. Verify that your     *
*     game works correctly.                                               *
*  8. If you have trouble with this update, email paul@empiremud.net      *
*                                                                         *
*  Credits: This is a cheap mock-up of play2to3.c which is included in    *
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
*   VERSION 2.0b2 STRUCTS
*   VERSION 2.0b3 STRUCTS
*   CONVERTER CODE
*/

#include <stdio.h>
#include <time.h>
#include <sys/types.h>

#include "../conf.h"
#include "../sysdep.h"

#include "../structs.h"
#include "../utils.h"


 //////////////////////////////////////////////////////////////////////////////
//// VERSION 2.0b2 STRUCTS ///////////////////////////////////////////////////

#define b2_MAX_NAME_LENGTH  20
#define b2_MAX_TITLE_LENGTH  100
#define b2_MAX_PLAYER_DESCRIPTION  4000
#define b2_MAX_PROMPT_SIZE  120
#define b2_MAX_POOFIN_LENGTH  80
#define b2_MAX_PWD_LENGTH  32
#define b2_NUM_ATTRIBUTES  6
#define b2_MAX_AFFECT  32
#define b2_MAX_COOLDOWNS  32
#define b2_MAX_HOST_LENGTH  45
#define b2_NUM_POOLS  4
#define b2_TOTAL_EXTRA_ATTRIBUTES  20
#define b2_MAX_REFERRED_BY_LENGTH  32
#define b2_MAX_ADMIN_NOTES_LENGTH  240
#define b2_NUM_CONDS  3
#define b2_MAX_MATERIALS  20
#define b2_MAX_IGNORES  15
#define b2_MAX_SLASH_CHANNELS  20
#define b2_MAX_SLASH_CHANNEL_NAME_LENGTH  16
#define b2_MAX_CUSTOM_COLORS  10
#define b2_MAX_REWARDS_PER_DAY  5
#define b2_NUM_ACTION_VNUMS  3
#define b2_MAX_SKILLS  24
#define b2_MAX_ABILITIES  400
#define b2_MAX_DISGUISED_NAME_LENGTH  32

struct b2_char_special_data_saved {
	int idnum;	// player's idnum; -1 for mobiles
	bitvector_t act;	// mob flag for NPCs; player flag for PCs

	bitvector_t injuries;	// Bitvectors including damage to the player

	bitvector_t affected_by;	// Bitvector for spells/skills affected by
};

struct b2_player_ability_data {
	bool purchased;	// whether or not the player bought it
	byte levels_gained;	// tracks for the cap on how many times one ability grants skillups
};

struct b2_player_skill_data {
	byte level;	// current skill level (0-100)
	double exp;	// experience gained in skill (0-100)
	byte resets;	// resets available
	bool noskill;	// if TRUE, do not gain
};

struct b2_player_special_data_saved {
	// account and player stuff
	int account_id;	// all characters with the same account_id belong to the same player
	char creation_host[b2_MAX_HOST_LENGTH+1];	// host they created from
	char referred_by[b2_MAX_REFERRED_BY_LENGTH];	// as entered in character creation
	char admin_notes[b2_MAX_ADMIN_NOTES_LENGTH];	// for admin to leave notes about a player
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
	sh_int conditions[b2_NUM_CONDS];	// Drunk, full, thirsty
	int resources[b2_MAX_MATERIALS];	// God resources
	int ignore_list[b2_MAX_IGNORES];	// players who can't message you
	char slash_channels[b2_MAX_SLASH_CHANNELS][b2_MAX_SLASH_CHANNEL_NAME_LENGTH];	// for storing /channels
	char custom_colors[b2_MAX_CUSTOM_COLORS];	// for custom channel coloring, storing the letter part of the & code ('r' for &r)

	// some daily stuff
	int daily_cycle;	// Last update cycle registered
	ubyte daily_bonus_experience;	// boosted skill gain points
	int rewarded_today[b2_MAX_REWARDS_PER_DAY];	// idnums, for ABIL_REWARD

	// action info
	int action;	// ACT_
	int action_cycle;	// time left before an action tick
	int action_timer;	// ticks to completion (use varies)
	room_vnum action_room;	// player location
	int action_vnum[b2_NUM_ACTION_VNUMS];	// slots for storing action data (use varies)
	
	// locations and movement
	room_vnum load_room;	// Which room to place char in
	room_vnum load_room_check;	// this is the home room of the player's loadroom, used to check that they're still in the right place
	room_vnum last_room;	// useful when dead
	room_vnum tomb_room;	// location of player's chosen tomb
	byte last_direction;	// used for walls and movement
	int recent_death_count;	// for death penalty
	long last_death_time;	// for death counts
	int last_corpse_id;	// DG Scripts obj id of last corpse
	
	// olc data
	any_vnum olc_min_vnum;	// low range of olc permissions
	any_vnum olc_max_vnum;	// high range of olc permissions
	bitvector_t olc_flags;	// olc permissions
	
	// skill/ability data
	byte creation_archetype;	// this is now stored permanently so later decisions can be made based on it
	struct b2_player_skill_data skills[b2_MAX_SKILLS];
	struct b2_player_ability_data abilities[b2_MAX_ABILITIES];
	bool can_gain_new_skills;	// not required to keep skills at zero
	bool can_get_bonus_skills;	// can buy extra 75's
	sh_int skill_level;  // levels computed based on class skills
	sh_int highest_known_level;	// maximum level ever achieved (used for gear restrictions)
	ubyte class_progression;	// % of the way from SPECIALTY_SKILL_CAP to CLASS_SKILL_CAP
	ubyte class_role;	// ROLE_x chosen by the player
	sh_int character_class;  // character's class as determined by top skills
	
	// tracking for specific skills
	byte confused_dir;  // people without Navigation think this dir is north
	char disguised_name[b2_MAX_DISGUISED_NAME_LENGTH];	// verbatim copy of name -- grabs custom mob names and empire names
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
	sh_int last_known_level;	// set on save/quit/alt -- TODO next pconvert, move this up with highest_known_level
	
	int spare15;
	int spare16;
	int spare17;
	int spare18;
	int recent_level_time;	// no longer used, but may have data set if you ran b2.9 or earlier -- TODO should be removed in the b2->b3 pconvert
	
	double spare20;
	double spare21;
	double spare22;
	double spare23;
	double spare24;
	
	// WARNING: in 2.0b1-b2, these were erroneously initialized to -1 -- TODO next pconvert, fix any spares from 25-34 that are misinitialized
	bitvector_t spare25; 
	bitvector_t spare26;
	bitvector_t spare27;
	bitvector_t spare28;
	bitvector_t spare29;
	
	// these are initialized to NOTHING/-1 in init_player() -- WARNING: in 2.0b1-b2, these were initialized to 0
	any_vnum spare30;
	any_vnum spare31;
	any_vnum spare32;
	any_vnum adventure_summon_return_location;	// where to send a player back to if they're outside an adventure
	any_vnum adventure_summon_return_map;	// map check location for the return loc -- TODO next pconvert, move both of these up
};

struct b2_affected_type {
	sh_int type;	// The type of spell that caused this
	int cast_by;	// player ID (positive) or mob vnum (negative)
	sh_int duration;	// For how long its effects will last
	int modifier;	// This is added to apropriate ability
	byte location;	// Tells which ability to change - APPLY_
	bitvector_t bitvector;	// Tells which bits to set - AFF_

	struct b2_affected_type *next;
};

struct b2_char_point_data {
	int current_pools[b2_NUM_POOLS];	// HEALTH, MOVE, MANA, BLOOD
	int max_pools[b2_NUM_POOLS];	// HEALTH, MOVE, MANA, BLOOD
	int deficit[b2_NUM_POOLS];	// HEALTH, MOVE, MANA, BLOOD
	
	int extra_attributes[b2_TOTAL_EXTRA_ATTRIBUTES];	// ATT_ (dodge, etc)
};

struct b2_over_time_effect_type {
	sh_int type;	// ATYPE_
	int cast_by;	// player ID (positive) or mob vnum (negative)
	sh_int duration;	// time in 5-second real-updates
	sh_int damage_type;	// DAM_x type
	sh_int damage;	// amount
	sh_int stack;	// damage is multiplied by this
	sh_int max_stack;	// how high it's allowed to stack

	struct b2_over_time_effect_type *next;
};

struct b2_cooldown_data {
	sh_int type;	// any COOLDOWN_ const
	time_t expire_time;	// time at which the cooldown has expired
	
	struct b2_cooldown_data *next;	// linked list
};

struct b2_char_file_u {
	char name[b2_MAX_NAME_LENGTH+1];	// first name
	char lastname[b2_MAX_NAME_LENGTH+1];	// last name
	char title[b2_MAX_TITLE_LENGTH+1];	// character's title
	char description[b2_MAX_PLAYER_DESCRIPTION];	// player's manual description
	char prompt[b2_MAX_PROMPT_SIZE+1];	// character's prompt
	char fight_prompt[b2_MAX_PROMPT_SIZE+1];	// character's prompt for combat
	char poofin[b2_MAX_POOFIN_LENGTH+1];	// message when appearing
	char poofout[b2_MAX_POOFIN_LENGTH+1];	// message when disappearing
	byte sex;	// sex (yes, please)
	byte access_level;	// character's level
	time_t birth;	// time of birth of character
	int	played;	// number of secs played in total

	char pwd[b2_MAX_PWD_LENGTH+1];	// character's password

	struct b2_char_special_data_saved char_specials_saved;
	struct b2_player_special_data_saved player_specials_saved;
	sh_int attributes[b2_NUM_ATTRIBUTES];	// str, etc (natural)
	struct b2_char_point_data points;
	struct b2_affected_type affected[b2_MAX_AFFECT];
	struct b2_over_time_effect_type over_time_effects[b2_MAX_AFFECT];
	struct b2_cooldown_data cooldowns[b2_MAX_COOLDOWNS];

	time_t last_logon;	// time (in secs) of last logon
	char host[b2_MAX_HOST_LENGTH+1];	// host of last logon
};


 //////////////////////////////////////////////////////////////////////////////
//// VERSION 2.0b3 STRUCTS ///////////////////////////////////////////////////

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
	int action;	// ACT_
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
	byte location;	// Tells which ability to change - APPLY_
	bitvector_t bitvector;	// Tells which bits to set - AFF_

	struct b3_affected_type *next;
};

struct b3_char_point_data {
	int current_pools[b3_NUM_POOLS];	// HEALTH, MOVE, MANA, BLOOD
	int max_pools[b3_NUM_POOLS];	// HEALTH, MOVE, MANA, BLOOD
	int deficit[b3_NUM_POOLS];	// HEALTH, MOVE, MANA, BLOOD
	
	int extra_attributes[b3_TOTAL_EXTRA_ATTRIBUTES];	// ATT_ (dodge, etc)
};

struct b3_over_time_effect_type {
	sh_int type;	// ATYPE_
	int cast_by;	// player ID (positive) or mob vnum (negative)
	sh_int duration;	// time in 5-second real-updates
	sh_int damage_type;	// DAM_ type
	sh_int damage;	// amount
	sh_int stack;	// damage is multiplied by this
	sh_int max_stack;	// how high it's allowed to stack

	struct b3_over_time_effect_type *next;
};

struct b3_cooldown_data {
	sh_int type;	// any COOLDOWN_ const
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
//// CONVERTER CODE //////////////////////////////////////////////////////////

#define PLRCONV_STRCPY(to_str, from_str, new_len)  { \
	strncpy((to_str), (from_str), (new_len)); \
	to_str[(new_len)-1] = '\0'; \
}


// function for converting old char files to new ones -- must be replaced with each new playerconvert
void convert_char_file_u(struct b3_char_file_u *to, struct b2_char_file_u *from) {
	int iter;
	
	PLRCONV_STRCPY(to->name, from->name, b3_MAX_NAME_LENGTH+1);
	PLRCONV_STRCPY(to->lastname, from->lastname, b3_MAX_NAME_LENGTH+1);
	PLRCONV_STRCPY(to->title, from->title, b3_MAX_TITLE_LENGTH+1);
	PLRCONV_STRCPY(to->description, from->description, b3_MAX_PLAYER_DESCRIPTION);
	PLRCONV_STRCPY(to->prompt, from->prompt, b3_MAX_PROMPT_SIZE+1);
	PLRCONV_STRCPY(to->fight_prompt, from->fight_prompt, b3_MAX_PROMPT_SIZE+1);
	PLRCONV_STRCPY(to->poofin, from->poofin, b3_MAX_POOFIN_LENGTH+1);
	PLRCONV_STRCPY(to->poofout, from->poofout, b3_MAX_POOFIN_LENGTH+1);
	to->sex = from->sex;
	to->access_level = from->access_level;
	to->birth = from->birth;
	to->played = from->played;
	PLRCONV_STRCPY(to->pwd, from->pwd, b3_MAX_PWD_LENGTH+1);
	to->last_logon = from->last_logon;
	PLRCONV_STRCPY(to->host, from->host, b3_MAX_HOST_LENGTH+1);

	// char_special_data_saved
	to->char_specials_saved.idnum = from->char_specials_saved.idnum;
	to->char_specials_saved.act = from->char_specials_saved.act;
	to->char_specials_saved.injuries = from->char_specials_saved.injuries;
	to->char_specials_saved.affected_by = from->char_specials_saved.affected_by;
	
	// player_special_data_saved
	to->player_specials_saved.account_id = from->player_specials_saved.account_id;
	PLRCONV_STRCPY(to->player_specials_saved.creation_host, from->player_specials_saved.creation_host, b3_MAX_HOST_LENGTH+1);
	PLRCONV_STRCPY(to->player_specials_saved.referred_by, from->player_specials_saved.referred_by, b3_MAX_REFERRED_BY_LENGTH);
	PLRCONV_STRCPY(to->player_specials_saved.admin_notes, from->player_specials_saved.admin_notes, b3_MAX_ADMIN_NOTES_LENGTH);
	to->player_specials_saved.invis_level = from->player_specials_saved.invis_level;
	to->player_specials_saved.immortal_level = from->player_specials_saved.immortal_level;
	to->player_specials_saved.grants = from->player_specials_saved.grants;
	to->player_specials_saved.syslogs = from->player_specials_saved.syslogs;
	to->player_specials_saved.bonus_traits = from->player_specials_saved.bonus_traits;
	to->player_specials_saved.bad_pws = from->player_specials_saved.bad_pws;
	to->player_specials_saved.promo_id = from->player_specials_saved.promo_id;
	to->player_specials_saved.pref = from->player_specials_saved.pref;
	to->player_specials_saved.last_tip = from->player_specials_saved.last_tip;
	to->player_specials_saved.mapsize = from->player_specials_saved.mapsize;
	to->player_specials_saved.pledge = from->player_specials_saved.pledge;
	to->player_specials_saved.empire = from->player_specials_saved.empire;
	to->player_specials_saved.rank = from->player_specials_saved.rank;
	to->player_specials_saved.apparent_age = from->player_specials_saved.apparent_age;
	for (iter = 0; iter < b3_NUM_CONDS; ++iter) {
		to->player_specials_saved.conditions[iter] = (iter < b2_NUM_CONDS) ? from->player_specials_saved.conditions[iter] : 0;
	}
	for (iter = 0; iter < b3_MAX_MATERIALS; ++iter) {
		to->player_specials_saved.resources[iter] = (iter < b2_MAX_MATERIALS) ? from->player_specials_saved.resources[iter] : 0;
	}
	for (iter = 0; iter < b3_MAX_IGNORES; ++iter) {
		to->player_specials_saved.ignore_list[iter] = (iter < b2_MAX_IGNORES) ? from->player_specials_saved.ignore_list[iter] : 0;
	}
	for (iter = 0; iter < b3_MAX_SLASH_CHANNELS; ++iter) {
		if (iter < b2_MAX_SLASH_CHANNELS) {
			PLRCONV_STRCPY(to->player_specials_saved.slash_channels[iter], from->player_specials_saved.slash_channels[iter], b3_MAX_SLASH_CHANNEL_NAME_LENGTH);
		}
		else {
			*(to->player_specials_saved.slash_channels[iter]) = '\0';
		}
	}
	for (iter = 0; iter < b3_MAX_CUSTOM_COLORS; ++iter) {
		to->player_specials_saved.custom_colors[iter] = (iter < b2_MAX_CUSTOM_COLORS) ? from->player_specials_saved.custom_colors[iter] : 0;
	}
	to->player_specials_saved.daily_cycle = from->player_specials_saved.daily_cycle;
	to->player_specials_saved.daily_bonus_experience = from->player_specials_saved.daily_bonus_experience;
	for (iter = 0; iter < b3_MAX_REWARDS_PER_DAY; ++iter) {
		to->player_specials_saved.rewarded_today[iter] = (iter < b2_MAX_REWARDS_PER_DAY) ? from->player_specials_saved.rewarded_today[iter] : -1;
	}
	to->player_specials_saved.action = from->player_specials_saved.action;
	to->player_specials_saved.action_cycle = from->player_specials_saved.action_cycle;
	to->player_specials_saved.action_timer = from->player_specials_saved.action_timer;
	to->player_specials_saved.action_room = from->player_specials_saved.action_room;
	for (iter = 0; iter < b3_NUM_ACTION_VNUMS; ++iter) {
		to->player_specials_saved.action_vnum[iter] = (iter < b2_NUM_ACTION_VNUMS) ? from->player_specials_saved.action_vnum[iter] : 0;
	}
	to->player_specials_saved.load_room = from->player_specials_saved.load_room;
	to->player_specials_saved.load_room_check = from->player_specials_saved.load_room_check;
	to->player_specials_saved.last_room = from->player_specials_saved.last_room;
	to->player_specials_saved.tomb_room = from->player_specials_saved.tomb_room;
	to->player_specials_saved.last_direction = from->player_specials_saved.last_direction;
	to->player_specials_saved.recent_death_count = from->player_specials_saved.recent_death_count;
	to->player_specials_saved.last_death_time = from->player_specials_saved.last_death_time;
	to->player_specials_saved.last_corpse_id = from->player_specials_saved.last_corpse_id;
	to->player_specials_saved.adventure_summon_return_location = from->player_specials_saved.adventure_summon_return_location;
	to->player_specials_saved.adventure_summon_return_map = from->player_specials_saved.adventure_summon_return_map;
	to->player_specials_saved.olc_min_vnum = from->player_specials_saved.olc_min_vnum;
	to->player_specials_saved.olc_max_vnum = from->player_specials_saved.olc_max_vnum;
	to->player_specials_saved.olc_flags = from->player_specials_saved.olc_flags;
	to->player_specials_saved.creation_archetype = from->player_specials_saved.creation_archetype;
	for (iter = 0; iter < b3_MAX_SKILLS; ++iter) {
		if (iter < b2_MAX_SKILLS) {
			to->player_specials_saved.skills[iter].level = from->player_specials_saved.skills[iter].level;
			to->player_specials_saved.skills[iter].exp = from->player_specials_saved.skills[iter].exp;
			to->player_specials_saved.skills[iter].resets = from->player_specials_saved.skills[iter].resets;
			to->player_specials_saved.skills[iter].noskill = from->player_specials_saved.skills[iter].noskill;
		}
		else {
			to->player_specials_saved.skills[iter].level = 0;
			to->player_specials_saved.skills[iter].exp = 0;
			to->player_specials_saved.skills[iter].resets = 0;
			to->player_specials_saved.skills[iter].noskill = FALSE;
		}
	}
	for (iter = 0; iter < b3_MAX_ABILITIES; ++iter) {
		if (iter < b2_MAX_ABILITIES) {
			to->player_specials_saved.abilities[iter].purchased = from->player_specials_saved.abilities[iter].purchased;
			to->player_specials_saved.abilities[iter].levels_gained = from->player_specials_saved.abilities[iter].levels_gained;
		}
		else {
			to->player_specials_saved.abilities[iter].purchased = FALSE;
			to->player_specials_saved.abilities[iter].levels_gained = 0;
		}
	}
	to->player_specials_saved.can_gain_new_skills = from->player_specials_saved.can_gain_new_skills;
	to->player_specials_saved.can_get_bonus_skills = from->player_specials_saved.can_get_bonus_skills;
	to->player_specials_saved.skill_level = from->player_specials_saved.skill_level;
	to->player_specials_saved.highest_known_level = from->player_specials_saved.highest_known_level;
	to->player_specials_saved.last_known_level = from->player_specials_saved.last_known_level;
	to->player_specials_saved.class_progression = from->player_specials_saved.class_progression;
	to->player_specials_saved.class_role = from->player_specials_saved.class_role;
	to->player_specials_saved.character_class = from->player_specials_saved.character_class;
	to->player_specials_saved.confused_dir = from->player_specials_saved.confused_dir;
	PLRCONV_STRCPY(to->player_specials_saved.disguised_name, from->player_specials_saved.disguised_name, b3_MAX_DISGUISED_NAME_LENGTH);
	to->player_specials_saved.disguised_sex = from->player_specials_saved.disguised_sex;
	to->player_specials_saved.morph = from->player_specials_saved.morph;
	to->player_specials_saved.mount_flags = from->player_specials_saved.mount_flags;
	to->player_specials_saved.mount_vnum = from->player_specials_saved.mount_vnum;
	to->player_specials_saved.using_poison = from->player_specials_saved.using_poison;
	
	// resetting spares
	to->player_specials_saved.spare1 = 0;
	to->player_specials_saved.spare2 = 0;
	to->player_specials_saved.spare3 = 0;
	to->player_specials_saved.spare4 = 0;
	to->player_specials_saved.spare5 = 0;
	to->player_specials_saved.spare6 = 0;
	to->player_specials_saved.spare7 = 0;
	to->player_specials_saved.spare8 = 0;
	to->player_specials_saved.spare9 = 0;
	to->player_specials_saved.spare10 = 0;
	to->player_specials_saved.spare11 = 0;
	to->player_specials_saved.spare12 = 0;
	to->player_specials_saved.spare13 = 0;
	to->player_specials_saved.spare14 = 0;
	to->player_specials_saved.spare15 = 0;
	to->player_specials_saved.spare16 = 0;
	to->player_specials_saved.spare17 = 0;
	to->player_specials_saved.spare18 = 0;
	to->player_specials_saved.spare19 = 0;
	to->player_specials_saved.spare20 = 0;
	to->player_specials_saved.spare21 = 0;
	to->player_specials_saved.spare22 = 0;
	to->player_specials_saved.spare23 = 0;
	to->player_specials_saved.spare24 = 0;
	to->player_specials_saved.spare25 = 0;
	to->player_specials_saved.spare26 = 0;
	to->player_specials_saved.spare27 = 0;
	to->player_specials_saved.spare28 = 0;
	to->player_specials_saved.spare29 = 0;
	to->player_specials_saved.spare30 = NOTHING;
	to->player_specials_saved.spare31 = NOTHING;
	to->player_specials_saved.spare32 = NOTHING;
	to->player_specials_saved.spare33 = NOTHING;
	to->player_specials_saved.spare34 = NOTHING;

	// attributes
	for (iter = 0; iter < b3_NUM_ATTRIBUTES; ++iter) {
		to->attributes[iter] = (iter < b2_NUM_ATTRIBUTES) ? from->attributes[iter] : 1;
	}
	
	// char_point_data
	for (iter = 0; iter < b3_NUM_POOLS; ++iter) {
		to->points.current_pools[iter] = (iter < b2_NUM_POOLS) ? from->points.current_pools[iter] : 0;
		to->points.max_pools[iter] = (iter < b2_NUM_POOLS) ? from->points.max_pools[iter] : 1;
	}
	for (iter = 0; iter < b3_TOTAL_EXTRA_ATTRIBUTES; ++iter) {
		to->points.extra_attributes[iter] = (iter < b2_TOTAL_EXTRA_ATTRIBUTES) ? from->points.extra_attributes[iter] : 0;
	}
	for (iter = 0; iter < b3_NUM_POOLS; ++iter) {
		to->points.deficit[iter] = (iter < b2_NUM_POOLS) ? from->points.deficit[iter] : 0;
	}

	// affected_type
	for (iter = 0; iter < b3_MAX_AFFECT; ++iter) {
		if (iter < b2_MAX_AFFECT) {
			to->affected[iter].type = from->affected[iter].type;
			to->affected[iter].cast_by = from->affected[iter].cast_by;
			to->affected[iter].duration = from->affected[iter].duration;
			to->affected[iter].modifier = from->affected[iter].modifier;
			to->affected[iter].location = from->affected[iter].location;
			to->affected[iter].bitvector = from->affected[iter].bitvector;
			to->affected[iter].next = 0;
		}
		else {
			to->affected[iter].type = 0;
			to->affected[iter].cast_by = 0;
			to->affected[iter].duration = 0;
			to->affected[iter].modifier = 0;
			to->affected[iter].location = APPLY_NONE;
			to->affected[iter].bitvector = 0;
			to->affected[iter].next = 0;
		}
	}
	
	// over_time_effect_type
	for (iter = 0; iter < b3_MAX_AFFECT; ++iter) {
		if (iter < b2_MAX_AFFECT) {
			to->over_time_effects[iter].type = from->over_time_effects[iter].type;
			to->over_time_effects[iter].cast_by = from->over_time_effects[iter].cast_by;
			to->over_time_effects[iter].duration = from->over_time_effects[iter].duration;
			to->over_time_effects[iter].damage_type = from->over_time_effects[iter].damage_type;
			to->over_time_effects[iter].damage = from->over_time_effects[iter].damage;
			to->over_time_effects[iter].stack = from->over_time_effects[iter].stack;
			to->over_time_effects[iter].max_stack = from->over_time_effects[iter].max_stack;
			to->over_time_effects[iter].next = 0;
		}
		else {
			to->over_time_effects[iter].type = 0;
			to->over_time_effects[iter].cast_by = 0;
			to->over_time_effects[iter].duration = 0;
			to->over_time_effects[iter].damage_type = 0;
			to->over_time_effects[iter].damage = 0;
			to->over_time_effects[iter].stack = 0;
			to->over_time_effects[iter].max_stack = 0;
			to->over_time_effects[iter].next = 0;
		}
	}
	
	// cooldown_data
	for (iter = 0; iter < b3_MAX_COOLDOWNS; ++iter) {
		if (iter < b2_MAX_COOLDOWNS) {
			to->cooldowns[iter].type = from->cooldowns[iter].type;
			to->cooldowns[iter].expire_time = from->cooldowns[iter].expire_time;
			to->cooldowns[iter].next = 0;
		}
		else {
			to->cooldowns[iter].type = 0;
			to->cooldowns[iter].expire_time = 0;
			to->cooldowns[iter].next = 0;
		}
	}
}


// main() function
int main(int argc, char *argv[]) {
	struct b2_char_file_u stOld;
	struct b3_char_file_u stNew;
	FILE *ptOldHndl;
	FILE *ptNewHndl;

	if (argc < 3) {
		printf("usage: plrconv [old player file] [new player file]\n");
		exit(1);
	}
	ptOldHndl = fopen(argv[1], "rb");
	if (ptOldHndl == NULL) {
		printf("unable to open source file \"%s\"\n", argv[1]);
		exit(1);
	}
	ptNewHndl = fopen(argv[2], "wb");
	if (ptNewHndl == NULL) {
		printf("unable to open destination file \"%s\"\n", argv[2]);
		exit(1);
	}

	while (!feof(ptOldHndl)) {
		fread(&stOld, sizeof(struct b2_char_file_u), 1, ptOldHndl);
		
		convert_char_file_u(&stNew, &stOld);

		// log progress to screen
		if (stNew.access_level >= LVL_START_IMM)
			printf("*");
		else if (stNew.access_level >= LVL_MORTAL)
			printf("-");
		else
			printf(".");

	    fwrite(&stNew, sizeof(struct b3_char_file_u), 1, ptNewHndl);
	}

	printf("\n");

	fclose(ptNewHndl);
	fclose(ptOldHndl);

	return 0;
}
