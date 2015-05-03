/* ************************************************************************
*   File: eedit.c                                         EmpireMUD 2.0b1 *
*  Usage: empire editor code                                              *
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
#include "interpreter.h"
#include "comm.h"
#include "handler.h"
#include "db.h"
#include "olc.h"

/**
* Contents:
*   Helpers
*   Control Structure
*   Editor Commands
*/

// helpful
#define EEDIT(name)		void (name)(char_data *ch, char *argument, empire_data *emp)

// externs
void eliminate_linkdead_players();

// locals
EEDIT(eedit_adjective);
EEDIT(eedit_banner);
EEDIT(eedit_description);
EEDIT(eedit_frontiertraits);
EEDIT(eedit_motd);
EEDIT(eedit_name);
EEDIT(eedit_privilege);
EEDIT(eedit_rank);
EEDIT(eedit_num_ranks);


 //////////////////////////////////////////////////////////////////////////////
//// HELPERS /////////////////////////////////////////////////////////////////

/**
* @param char *str The proposed banner string (color codes)
* @return bool TRUE if the banner is valid; FALSE otherwise
*/
bool check_banner_color_string(char *str) {
	const char *valid_colors = "rgybmcwRGYBMCW0u";
	
	bool ok = TRUE;
	int pos;
	char code = 0;
	
	for (pos = 0; pos < strlen(str); ++pos) {
		if (str[pos] == '&') {
			if (pos == (strlen(str)-1)) {
				// trailing &
				ok = FALSE;
			}
			else {
				code = str[++pos];
			}
		}
		else {
			ok = FALSE;
		}
	}
	
	// check the color code itself
	if (ok && code > 0) {
		if (!strchr(valid_colors, code)) {
			ok = FALSE;
		}
	}
	
	return ok;
}


/**
* @param char *newname The proposed empire name.
* @return bool TRUE if the name is ok.
*/
bool valid_empire_name(char *newname) {
	extern char *invalid_list[MAX_INVALID_NAMES];
	extern int num_invalid;

	char *ptr;
	char tempname[MAX_INPUT_LENGTH];
	bool ok = TRUE;
	empire_data *emp, *next_emp;
	int iter;
	
	// check for illegal & codes (anything other than &&)
	if ((ptr = strchr(newname, '&')) && *(ptr+1) != '&') {
		ok = FALSE;
	}

	// check empires for same name
	HASH_ITER(hh, empire_table, emp, next_emp) {
		if (!str_cmp(EMPIRE_NAME(emp), newname)) {
			ok = FALSE;
			break;
		}
	}
	
	// banned names
	if (ok && num_invalid > 0) {
		strcpy(tempname, newname);
		for (iter = 0 ; iter < strlen(tempname); ++iter) {
			tempname[iter] = LOWER(tempname[iter]);
		}
		
		// compare to banned names
		for (iter = 0; iter < num_invalid && ok; ++iter) {
			if (strstr(tempname, invalid_list[iter])) {
				ok = FALSE;
			}
		}
	}
	
	return ok;
}


/**
* Validates rank names for empires.
*
* @param char *newname The proposed name.
* @return bool TRUE if the name is ok, or FALSE otherwise.
*/
bool valid_rank_name(char *newname) {
	int iter;
	bool ok = TRUE;
	
	char *valid = " -&'";
	
	if ((strlen(newname) - 2*count_color_codes(newname)) > MAX_RANK_LENGTH) {
		ok = FALSE;
	}

	for (iter = 0; iter < strlen(newname) && ok; ++iter) {
		if (!isalnum(newname[iter]) && !strchr(valid, newname[iter])) {
			ok = FALSE;
		}
	}
	
	return ok;
}



 //////////////////////////////////////////////////////////////////////////////
//// CONTROL STRUCTURE ///////////////////////////////////////////////////////

const struct {
	char *command;
	EEDIT(*func);
	bitvector_t flags;
} eedit_cmd[] = {
	{ "adjective", eedit_adjective, NOBITS },
	{ "banner", eedit_banner, NOBITS },
	{ "description", eedit_description, NOBITS },
	{ "frontiertraits", eedit_frontiertraits, NOBITS },
	{ "motd", eedit_motd, NOBITS },
	{ "name", eedit_name, NOBITS },
	{ "privilege", eedit_privilege, NOBITS },
	{ "rank", eedit_rank, NOBITS },
	{ "ranks", eedit_num_ranks, NOBITS },
	
	// TODO: change leader -- add a generic function for doing it procedurally, so it can also be done on-delete

	// this goes last
	{ "\n", NULL, NOBITS }
};


ACMD(do_eedit) {
	bool imm_access = GET_ACCESS_LEVEL(ch) >= LVL_CIMPL || IS_GRANTED(ch, GRANT_EMPIRES);
	char *argptr, arg[MAX_INPUT_LENGTH];
	int iter, type = NOTHING;
	empire_data *emp;
	bool found;
	
	// optional first arg (empire) and empire detection
	argptr = any_one_word(argument, arg);
	if (!imm_access || !(emp = get_empire_by_name(arg))) {
		emp = GET_LOYALTY(ch);
		argptr = argument;
	}
	
	argptr = any_one_arg(argptr, arg);
	skip_spaces(&argptr);
	
	// find type?
	for (iter = 0; *eedit_cmd[iter].command != '\n' && type == NOTHING; ++iter) {
		if (is_abbrev(arg, eedit_cmd[iter].command)) {
			type = iter;
		}
	}
	
	if (IS_NPC(ch) || !emp) {
		msg_to_char(ch, "You are not in any empire.\r\n");
	}
	else if (GET_RANK(ch) < EMPIRE_NUM_RANKS(emp)) {
		msg_to_char(ch, "Your rank is too low to edit the empire.\r\n");
	}
	else if (!*arg || type == NOTHING) {
		msg_to_char(ch, "Available eedit commands: ");
		found = FALSE;
		for (iter = 0; *eedit_cmd[iter].command != '\n'; ++iter) {
			msg_to_char(ch, "%s%s", (found ? ", " : ""), eedit_cmd[iter].command);
			found = TRUE;
		}
		msg_to_char(ch, "%s\r\n", (found ? "" : "none"));
	}
	else {
		// pass to child function
		(eedit_cmd[type].func)(ch, argptr, emp);
		
		// always save
		save_empire(emp);
	}
}


 //////////////////////////////////////////////////////////////////////////////
//// EDITOR COMMANDS /////////////////////////////////////////////////////////

EEDIT(eedit_adjective) {
	if (!*argument) {
		msg_to_char(ch, "Set the empire's adjective form to what?\r\n");
	}
	else if (count_color_codes(argument) > 0) {
		msg_to_char(ch, "Empire adjective forms may not contain color codes. Set the banner instead.\r\n");
	}
	else if (strstr(argument, "%") != NULL) {
		msg_to_char(ch, "Empire adjective forms may not contain the percent sign (%%).\r\n");
	}
	else if (strlen(argument) > MAX_RANK_LENGTH) {
		msg_to_char(ch, "Adjective names are limited to %d characters.\r\n", MAX_RANK_LENGTH);
	}
	else {
		if (EMPIRE_ADJECTIVE(emp)) {
			free(EMPIRE_ADJECTIVE(emp));
		}
		EMPIRE_ADJECTIVE(emp) = str_dup(argument);
		
		log_to_empire(emp, ELOG_ADMIN, "%s has changed the empire's adjective form to %s", PERS(ch, ch, TRUE), EMPIRE_ADJECTIVE(emp));
		msg_to_char(ch, "The empire's adjective form is now: %s\r\n", EMPIRE_ADJECTIVE(emp));
	}
}


EEDIT(eedit_banner) {
	extern char *show_color_codes(char *string);
	
	if (!*argument) {
		msg_to_char(ch, "Set the empire banner to what (HELP COLOR)?\r\n");
	}
	else if (!check_banner_color_string(argument)) {
		msg_to_char(ch, "Invalid banner color (HELP COLOR).\r\n");
	}
	else {
		if (EMPIRE_BANNER(emp)) {
			free(EMPIRE_BANNER(emp));
		}
		EMPIRE_BANNER(emp) = str_dup(argument);

		log_to_empire(emp, ELOG_ADMIN, "%s has changed the banner color", PERS(ch, ch, TRUE));
		msg_to_char(ch, "The empire's banner is now: %s%s&0\r\n", EMPIRE_BANNER(emp), show_color_codes(EMPIRE_BANNER(emp)));
	}
}


EEDIT(eedit_description) {
	char buf[MAX_STRING_LENGTH];
	descriptor_data *desc;

	if (!ch->desc) {
		msg_to_char(ch, "You can't do that.\r\n");
		return;
	}
	if (ch->desc->str) {
		msg_to_char(ch, "You're already editing something else.\r\n");
		return;
	}
	
	// check nobody else is editing it
	for (desc = descriptor_list; desc; desc = desc->next) {
		if (desc != ch->desc && desc->str && desc->str == &EMPIRE_DESCRIPTION(emp)) {
			msg_to_char(ch, "Someone else is already editing the empire description.\r\n");
			return;
		}
	}
	
	sprintf(buf, "description for %s", EMPIRE_NAME(emp));
	start_string_editor(ch->desc, buf, &EMPIRE_DESCRIPTION(emp), MAX_EMPIRE_DESCRIPTION);
}


EEDIT(eedit_frontiertraits) {
	extern const char *empire_trait_types[];
	EMPIRE_FRONTIER_TRAITS(emp) = olc_process_flag(ch, argument, "frontier trait", NULL, empire_trait_types, EMPIRE_FRONTIER_TRAITS(emp));
}


EEDIT(eedit_motd) {
	char buf[MAX_STRING_LENGTH];
	descriptor_data *desc;

	if (!ch->desc) {
		msg_to_char(ch, "You can't do that.\r\n");
		return;
	}
	if (ch->desc->str) {
		msg_to_char(ch, "You're already editing something else.\r\n");
		return;
	}
	
	// check nobody else is editing it
	for (desc = descriptor_list; desc; desc = desc->next) {
		if (desc != ch->desc && desc->str && desc->str == &EMPIRE_MOTD(emp)) {
			msg_to_char(ch, "Someone else is already editing the empire motd.\r\n");
			return;
		}
	}
	
	sprintf(buf, "motd for %s", EMPIRE_NAME(emp));
	start_string_editor(ch->desc, buf, &EMPIRE_MOTD(emp), MAX_MOTD_LENGTH);
}


EEDIT(eedit_name) {
	if (!*argument) {
		msg_to_char(ch, "Set the empire name to what?\r\n");
	}
	else if (count_color_codes(argument) > 0) {
		msg_to_char(ch, "Empire names may not contain color codes. Set the banner instead.\r\n");
	}
	else if (strchr(argument, '%')) {
		msg_to_char(ch, "Empire names may not contain the percent sign (%%).\r\n");
	}
	else if (strchr(argument, '"')) {
		msg_to_char(ch, "Empire names may not contain a quotation mark (\").\r\n");
	}
	else if (!valid_empire_name(argument)) {
		msg_to_char(ch, "Invalid empire name.\r\n");
	}
	else {
		if (EMPIRE_NAME(emp)) {
			free(EMPIRE_NAME(emp));
		}
		EMPIRE_NAME(emp) = str_dup(CAP(argument));
		
		if (EMPIRE_ADJECTIVE(emp)) {
			free(EMPIRE_ADJECTIVE(emp));
		}
		EMPIRE_ADJECTIVE(emp) = str_dup(CAP(argument));
		
		log_to_empire(emp, ELOG_ADMIN, "%s has changed the empire name to %s", PERS(ch, ch, TRUE), EMPIRE_NAME(emp));
		msg_to_char(ch, "The empire's name is now: %s\r\n", EMPIRE_NAME(emp));
		msg_to_char(ch, "The adjective form was also changed (use 'eedit adjective' to change it).\r\n");
	}
}


EEDIT(eedit_privilege) {
	extern const char *priv[];

	int pr, rnk, iter;
	
	argument = any_one_word(argument, arg);
	skip_spaces(&argument);
	
	if (!*arg || !*argument) {
		msg_to_char(ch, "Usage: eedit privilege <type> <rank>\r\n");
		msg_to_char(ch, "Current privilege levels:\r\n");
		for (iter = 0; iter < NUM_PRIVILEGES; ++iter) {
			msg_to_char(ch, " %s: %s&0 (%d)\r\n", priv[iter], EMPIRE_RANK(emp, EMPIRE_PRIV(emp, iter)-1), EMPIRE_PRIV(emp, iter));
		}
	}
	else if ((rnk = find_rank_by_name(emp, argument)) == NOTHING) {
		msg_to_char(ch, "Invalid rank '%s'.\r\n", argument);
	}
	else if ((pr = search_block(arg, priv, FALSE)) == NOTHING) {
		msg_to_char(ch, "Invalid privilege '%s'.\r\n", arg);
	}
	else {
		EMPIRE_PRIV(emp, pr) = rnk + 1;	// 1-based, not 0-based

		log_to_empire(emp, ELOG_ADMIN, "%s has changed the %s privilege to rank %s%s (%d)", PERS(ch, ch, TRUE), priv[pr], EMPIRE_RANK(emp, rnk), EMPIRE_BANNER(emp), rnk+1);
		msg_to_char(ch, "You set the %s privilege to rank %s&0 (%d).\r\n", priv[pr], EMPIRE_RANK(emp, rnk), rnk+1);
	}
}


EEDIT(eedit_rank) {
	int rnk, iter;

	argument = any_one_word(argument, arg);
	skip_spaces(&argument);
	
	if (!*argument || !*arg) {
		msg_to_char(ch, "Usage: eedit rank <rank> <new name>\r\n");
		msg_to_char(ch, "Current ranks:\r\n");
		for (iter = 0; iter < EMPIRE_NUM_RANKS(emp); ++iter) {
			msg_to_char(ch, " %d. %s&0\r\n", iter+1, EMPIRE_RANK(emp, iter));
		}
	}
	else if ((rnk = find_rank_by_name(emp, arg)) == NOTHING) {
		msg_to_char(ch, "Invalid rank.\r\n");
	}
	else if (!valid_rank_name(argument)) {
		msg_to_char(ch, "Invalid rank name.\r\n");
	}
	else {
		if (EMPIRE_RANK(emp, rnk)) {
			free(EMPIRE_RANK(emp, rnk));
		}
		EMPIRE_RANK(emp, rnk) = str_dup(argument);
		
		log_to_empire(emp, ELOG_ADMIN, "%s has changed rank %d to %s%s", PERS(ch, ch, TRUE), rnk+1, EMPIRE_RANK(emp, rnk), EMPIRE_BANNER(emp));
		msg_to_char(ch, "You have changed rank %d to %s&0.\r\n", rnk+1, EMPIRE_RANK(emp, rnk));
	}
}


EEDIT(eedit_num_ranks) {
	extern const struct archetype_type archetype[];
	
	int pos, num, iter;
	struct char_file_u chdata;
	char_data *mem;
	bool is_file = FALSE;
	
	if (!*argument) {
		msg_to_char(ch, "How many ranks would you like to have (2-%d)?\r\n", MAX_RANKS);
	}
	else if ((num = atoi(argument)) < 2 || num > MAX_RANKS) {
		msg_to_char(ch, "You must choose a number of ranks between 2 and %d.\r\n", MAX_RANKS);
	}
	else {
		eliminate_linkdead_players();
		
		// update all players
		for (pos = 0; pos <= top_of_p_table; ++pos) {
			if (load_char(player_table[pos].name, &chdata) != NOBODY && !IS_SET(chdata.char_specials_saved.act, PLR_DELETED)) {
				if (chdata.player_specials_saved.empire == EMPIRE_VNUM(emp)) {
					if ((mem = find_or_load_player(player_table[pos].name, &is_file))) {
						if (GET_RANK(mem) == EMPIRE_NUM_RANKS(emp)) {
							// equal to old max
							GET_RANK(mem) = num;
						}
						else if (GET_RANK(mem) >= num) {
							// too high for new max
							GET_RANK(mem) = num - 1;
						}
						
						// save
						if (is_file) {
							store_loaded_char(mem);
							is_file = FALSE;
							mem = NULL;
						}
						else {
							SAVE_CHAR(mem);
						}
					}
		
					if (mem && is_file) {
						free_char(mem);
					}
				}
			}
		}
		
		// remove old rank names
		for (iter = num; iter < EMPIRE_NUM_RANKS(emp); ++iter) {
			if (EMPIRE_RANK(emp, iter)) {
				free(EMPIRE_RANK(emp, iter));
			}
			EMPIRE_RANK(emp, iter) = NULL;
		}
		
		// ensure all new ranks have names
		for (iter = 0; iter < num; ++iter) {
			if (!EMPIRE_RANK(emp, iter)) {
				EMPIRE_RANK(emp, iter) = str_dup((iter < (num-1)) ? "Follower" : archetype[(int) CREATION_ARCHETYPE(ch)].starting_rank[(int) GET_REAL_SEX(ch)]);
			}
		}
		
		// lower any privileges that are now too high
		for (iter = 0; iter < NUM_PRIVILEGES; ++iter) {
			if (EMPIRE_PRIV(emp, iter) > num) {
				EMPIRE_PRIV(emp, iter) = num;
			}
		}
		
		// update ranks
		EMPIRE_NUM_RANKS(emp) = num;
		
		log_to_empire(emp, ELOG_ADMIN, "%s has changed the number of ranks", PERS(ch, ch, TRUE));
		msg_to_char(ch, "The empire now has %d ranks.\r\n", EMPIRE_NUM_RANKS(emp));
	}
}
