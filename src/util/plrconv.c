/* ************************************************************************
*   File: plrconv.c                                       EmpireMUD 2.0b1 *
*  Usage: convert player file structures without wiping                   *
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
#include <stdio.h>
#include <time.h>
#include <sys/types.h>

#include "../conf.h"
#include "../sysdep.h"

#include "../structs.h"
#include "../utils.h"


// WARNING: This file has been left in the state it was in last time I used
// it to convert a playerfile. Therefore, it is an example of how to do it,
// and NOT a finished utility. If you make a change to your playerfile, you
// will need to update this entire program. Email me if you need further
// assistance understanding what this does. -paul/ paul@empiremud.net


#define OLD_MAX_SKILLS		64

struct OLD_char_special_data_saved {
	long idnum;					/* player's idnum; -1 for mobiles			*/
	bitvector_t act;			/* act flag for NPC's; player flag for PC's	*/

	bitvector_t injuries;		/* Bitvectors including damage to the player*/

	bitvector_t affected_by;	/* Bitvector for spells/skills affected by	*/
	bitvector_t dsc_flags;		/* Bitvector for discipline affects			*/
};


struct OLD_player_special_data_saved {
	// account and player stuff
	int account_id;	// all characters with the same account_id belong to the same player
	char referred_by[MAX_REFERRED_BY_LENGTH];	// as entered in character creation
	char admin_notes[MAX_ADMIN_NOTES_LENGTH];	// for admin to leave notes about a player
	byte invis_level;	// level of invisibility
	byte immortal_level;	// stored so that if level numbers are changed, imms stay at the correct level
	bitvector_t grants;	// grant imm abilities
	ubyte bad_pws;	// number of bad password attempts

	// preferences
	bitvector_t pref;	// preference flags for PCs.
	byte mapsize;	// how big the player likes the map

	// empire
	long loyalty;	// The empire this player follows
	byte rank;	// Rank in the empire
	long pledge;	// Empire he's applying to
		time_t defect_timer;	// UNUSED -- but always some time in the past
	
	// misc player attributes
	ubyte apparent_age;	// for vampires	
	sh_int conditions[NUM_CONDS];	// Drunk, full, thirsty
	int resources[MAX_MATERIALS];	// God resources
	long ignore_list[MAX_IGNORES];	// players who can't message you
	char slash_channels[MAX_SLASH_CHANNELS][MAX_SLASH_CHANNEL_NAME_LENGTH];	// for storing /channels
	char custom_colors[MAX_CUSTOM_COLORS];	// for custom channel coloring, storing the letter part of the & code ('r' for &r)

	// some daily stuff
	int daily_cycle;	// Last update cycle registered
	ubyte daily_bonus_experience;	// boosted skill gain points
	long rewarded_today[MAX_REWARDS_PER_DAY];	// idnums, for ABIL_REWARD

	// action info
	int action;	// ACT_x
	int action_rotation;	// used to keep all player actions from happening on the same tick
	int action_timer;	// ticks to completion (use varies)
	room_vnum action_room;	// player location
	int action_vnum[NUM_ACTION_VNUMS];	// slots for storing action data (use varies)
	
	// locations and movement
	room_vnum load_room;	// Which room to place char in
	room_vnum load_room_check;	// this is the home room of the player's loadroom, used to check that they're still in the right place
	room_vnum last_room;	// useful when dead
	byte last_direction;	// used for walls and movement
	
	// timers
		time_t last_hestian_time;	// UNUSED! But definitely set to some time in the past
		time_t thief_timer;	// UNUSED -- but always set to some time in the past
		time_t pvp_timer;	// UNUSED -- but always set to some time in the past
	
	// skill/ability data
	byte creation_archetype;	// this is now stored permanently so later decisions can be made based on it
	byte skills[OLD_MAX_SKILLS];
	struct player_ability_data abilities[MAX_ABILITIES];
	bitvector_t noskill_flags;
	bitvector_t free_skill_resets;	// skill bits giving a free reset
	bool can_gain_new_skills;	// not required to keep skills at zero
	bool can_get_bonus_skills;	// can buy extra 75's
	sh_int skill_level;  // levels computed based on class skills
	ubyte class_progression;	// % of the way from SPECIALTY_SKILL_CAP to CLASS_SKILL_CAP
	ubyte class_role;	// ROLE_x chosen by the player
	sh_int character_class;  // character's class as determined by top skills
	
	// tracking for specific skills
	byte confused_dir;  // people without Navigation think this dir is north
	char disguised_name[MAX_DISGUISED_NAME_LENGTH];	// verbatim copy of name -- grabs custom mob names and empire names
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
	
	int olc_min_vnum;	// low range of olc permissions
	int olc_max_vnum;	// high range of olc permissions
	int tomb_room_vnum;	// location of player's chosen tomb
	int spare19;
	
	bitvector_t olc_flags;	// olc permissions
	bitvector_t bonus_traits;	// BONUS_x
	bitvector_t spare22;
	bitvector_t spare23;
	bitvector_t spare24;
};


struct OLD_char_file_u {
	char name[MAX_NAME_LENGTH+1];		/* first name						*/
	char lastname[MAX_NAME_LENGTH+1];	/* last name						*/
	char title[MAX_TITLE_LENGTH+1];		/* character's title				*/
	char description[MAX_PLAYER_DESCRIPTION];	// player's manual description
	char prompt[MAX_PROMPT_SIZE+1];	/* character's prompt				*/
	char poofin[MAX_POOFIN_LENGTH+1];	/* message when appearing			*/
	char poofout[MAX_POOFIN_LENGTH+1];	/* message when disappearing		*/
	byte sex;							/* sex (yes, please)				*/
	byte level;							/* character's level				*/
	time_t birth;						/* time of birth of character		*/
	int	played;							/* number of secs played in total	*/

	char pwd[MAX_PWD_LENGTH+1];		/* character's password				*/

	struct OLD_char_special_data_saved char_specials_saved;
	struct OLD_player_special_data_saved player_specials_saved;
	sbyte attributes[NUM_ATTRIBUTES];	// str, etc (natural)
	struct char_point_data points;
	struct affected_type affected[MAX_AFFECT];
	struct over_time_effect_type over_time_effects[MAX_AFFECT];
	struct cooldown_data cooldowns[MAX_COOLDOWNS];

	time_t last_logon;					/* time (in secs) of last logon		*/
	char host[MAX_HOST_LENGTH+1];			/* host of last logon				*/
};


// function for converting old char files to new ones -- must be replaced with each new playerconvert
void convert_char_file_u(struct char_file_u *to, struct OLD_char_file_u *from) {
	int iter;
	
	strcpy(to->name, from->name);
	strcpy(to->lastname, from->lastname);
	strcpy(to->title, from->title);
	strcpy(to->description, from->description);
	strcpy(to->prompt, from->prompt);
	strcpy(to->poofin, from->poofin);
	strcpy(to->poofout, from->poofout);
	to->sex = from->sex;
	to->access_level = from->level;
	to->birth = from->birth;
	to->played = from->played;

	strcpy(to->pwd, from->pwd);

	to->last_logon = from->last_logon;
	strcpy(to->host, from->host);
	
	to->points = from->points;
	
	for (iter = 0; iter < NUM_ATTRIBUTES; ++iter) {
		to->attributes[iter] = from->attributes[iter];
	}
	
	for (iter = 0; iter < MAX_AFFECT; ++iter) {
		to->affected[iter] = from->affected[iter];
	}
	
	for (iter = 0; iter < MAX_AFFECT; ++iter) {
		to->over_time_effects[iter] = from->over_time_effects[iter];
	}
	
	for (iter = 0; iter < MAX_COOLDOWNS; ++iter) {
		to->cooldowns[iter] = from->cooldowns[iter];
	}
	
	// char_special_data_saved
	to->char_specials_saved.idnum = (int)from->char_specials_saved.idnum;
	to->char_specials_saved.act = from->char_specials_saved.act;
	to->char_specials_saved.injuries = from->char_specials_saved.injuries;
	to->char_specials_saved.affected_by = from->char_specials_saved.affected_by;
	
	// player_special_data_saved
	to->player_specials_saved.account_id = from->player_specials_saved.account_id;
	strcpy(to->player_specials_saved.referred_by, from->player_specials_saved.referred_by);
	strcpy(to->player_specials_saved.admin_notes, from->player_specials_saved.admin_notes);	
	to->player_specials_saved.invis_level = from->player_specials_saved.invis_level;
	to->player_specials_saved.immortal_level = from->player_specials_saved.immortal_level;
	to->player_specials_saved.grants = from->player_specials_saved.grants;
	to->player_specials_saved.bonus_traits = from->player_specials_saved.bonus_traits;
	to->player_specials_saved.bad_pws = from->player_specials_saved.bad_pws;
	to->player_specials_saved.pref = from->player_specials_saved.pref;
	to->player_specials_saved.mapsize = from->player_specials_saved.mapsize;
	to->player_specials_saved.pledge = from->player_specials_saved.pledge;
	if (to->player_specials_saved.pledge == 0) {
		to->player_specials_saved.pledge = NOTHING;
	}
	to->player_specials_saved.empire = from->player_specials_saved.loyalty;
	if (to->player_specials_saved.empire == 0) {
		to->player_specials_saved.empire = NOTHING;
	}
	to->player_specials_saved.rank = from->player_specials_saved.rank;
	to->player_specials_saved.apparent_age = from->player_specials_saved.apparent_age;
	
	for (iter = 0; iter < NUM_CONDS; ++iter) {
		to->player_specials_saved.conditions[iter] = from->player_specials_saved.conditions[iter];
	}
	for (iter = 0; iter < MAX_MATERIALS; ++iter) {
		to->player_specials_saved.resources[iter] = from->player_specials_saved.resources[iter];
	}
	for (iter = 0; iter < MAX_IGNORES; ++iter) {
		to->player_specials_saved.ignore_list[iter] = from->player_specials_saved.ignore_list[iter];
	}
	for (iter = 0; iter < MAX_SLASH_CHANNELS; ++iter) {
		strcpy(to->player_specials_saved.slash_channels[iter], from->player_specials_saved.slash_channels[iter]);
	}
	for (iter = 0; iter < MAX_CUSTOM_COLORS; ++iter) {
		to->player_specials_saved.custom_colors[iter] = from->player_specials_saved.custom_colors[iter];
	}
	for (iter = 0; iter < MAX_REWARDS_PER_DAY; ++iter) {
		to->player_specials_saved.rewarded_today[iter] = from->player_specials_saved.rewarded_today[iter];
	}
	
	to->player_specials_saved.daily_cycle = from->player_specials_saved.daily_cycle;
	to->player_specials_saved.daily_bonus_experience = from->player_specials_saved.daily_bonus_experience;
	
	to->player_specials_saved.action = from->player_specials_saved.action;
	to->player_specials_saved.action_rotation = from->player_specials_saved.action_rotation;
	to->player_specials_saved.action_timer = from->player_specials_saved.action_timer;
	to->player_specials_saved.action_room = from->player_specials_saved.action_room;
	for (iter = 0; iter < NUM_ACTION_VNUMS; ++iter) {
		to->player_specials_saved.action_vnum[iter] = from->player_specials_saved.action_vnum[iter];
	}
	
	to->player_specials_saved.load_room = from->player_specials_saved.load_room;
	to->player_specials_saved.load_room_check = from->player_specials_saved.load_room_check;
	to->player_specials_saved.last_room = from->player_specials_saved.last_room;
	to->player_specials_saved.tomb_room = from->player_specials_saved.tomb_room_vnum;
	to->player_specials_saved.last_direction = from->player_specials_saved.last_direction;
	
	to->player_specials_saved.olc_min_vnum = from->player_specials_saved.olc_min_vnum;
	to->player_specials_saved.olc_max_vnum = from->player_specials_saved.olc_max_vnum;
	to->player_specials_saved.olc_flags = from->player_specials_saved.olc_flags;
	
	to->player_specials_saved.creation_archetype = from->player_specials_saved.creation_archetype;
	
	for (iter = 0; iter < MAX_SKILLS; ++iter) {
		to->player_specials_saved.skills[iter].level = from->player_specials_saved.skills[iter];
		to->player_specials_saved.skills[iter].exp = 0;
		to->player_specials_saved.skills[iter].resets = IS_SET(from->player_specials_saved.free_skill_resets, BIT(iter)) ? 1 : 0;
		to->player_specials_saved.skills[iter].noskill = IS_SET(from->player_specials_saved.noskill_flags, BIT(iter)) ? 1 : 0;
	}

	for (iter = 0; iter < MAX_ABILITIES; ++iter) {
		to->player_specials_saved.abilities[iter] = from->player_specials_saved.abilities[iter];
	}
	
	to->player_specials_saved.can_gain_new_skills = from->player_specials_saved.can_gain_new_skills;
	to->player_specials_saved.can_get_bonus_skills = from->player_specials_saved.can_get_bonus_skills;
	to->player_specials_saved.skill_level = from->player_specials_saved.skill_level;
	to->player_specials_saved.class_progression = from->player_specials_saved.class_progression;
	to->player_specials_saved.class_role = from->player_specials_saved.class_role;
	to->player_specials_saved.character_class = from->player_specials_saved.character_class;
	
	to->player_specials_saved.confused_dir = from->player_specials_saved.confused_dir;
	strcpy(to->player_specials_saved.disguised_name, from->player_specials_saved.disguised_name);
	to->player_specials_saved.disguised_sex = from->player_specials_saved.disguised_sex;
	to->player_specials_saved.morph = from->player_specials_saved.morph;
	to->player_specials_saved.mount_flags = from->player_specials_saved.mount_flags;
	to->player_specials_saved.mount_vnum = from->player_specials_saved.mount_vnum;		
	if (to->player_specials_saved.mount_vnum == 0) {
		to->player_specials_saved.mount_vnum = NOTHING;
	}
	to->player_specials_saved.using_poison = from->player_specials_saved.using_poison;
	
	to->player_specials_saved.spare25 = NOTHING;
	to->player_specials_saved.spare26 = NOTHING;
	to->player_specials_saved.spare27 = NOTHING;
	to->player_specials_saved.spare28 = NOTHING;
	to->player_specials_saved.spare29 = NOTHING;
}


// main() function
int main(int argc, char *argv[]) {
	struct OLD_char_file_u stOld;
	struct char_file_u stNew;
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
		fread(&stOld, sizeof(struct OLD_char_file_u), 1, ptOldHndl);
		
		convert_char_file_u(&stNew, &stOld);

		// log progress to screen
		if (stNew.access_level >= LVL_START_IMM)
			printf("*");
		else if (stNew.access_level >= LVL_APPROVED)
			printf("-");
		else
			printf(".");

	    fwrite(&stNew, sizeof(struct char_file_u), 1, ptNewHndl);
	}

	printf("\n");

	fclose(ptNewHndl);
	fclose(ptOldHndl);

	return 0;
}
