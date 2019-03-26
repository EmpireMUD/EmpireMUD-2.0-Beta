/* ************************************************************************
*   File: eedit.c                                         EmpireMUD 2.0b5 *
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

// locals
EEDIT(eedit_adjective);
EEDIT(eedit_admin_flags);
EEDIT(eedit_banner);
EEDIT(eedit_change_leader);
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
	const char *valid_colors = "rgbymcwajloptvnRGBYMCWAJLOPTV0u";
	
	bool ok = TRUE;
	int pos, num_codes = 0;
	
	for (pos = 0; pos < strlen(str); ++pos) {
		if (str[pos] == '&') {
			if (pos == (strlen(str)-1)) {
				// trailing &
				ok = FALSE;
			}
			else if (strchr(valid_colors, str[pos+1])) {
				// this code is ok but count number of non-underlined codes
				if (str[pos+1] != 'u') {
					++num_codes;
				}
				
				// skip over
				++pos;
			}
			else {
				ok = FALSE;
			}
		}
		else {
			ok = FALSE;
		}
	}
	
	// do not allow multiple colors
	if (num_codes > 1) {
		ok = FALSE;
	}
	
	return ok;
}


/**
* Makes sure a potential name is unique.
*
* @param empire_data *for_emp The empire to be named (may be NULL) -- so we don't disqualify it for already having that name.
* @param char *name The name to check.
* @return bool TRUE if the name is valid/unique and FALSE if not.
*/
bool check_unique_empire_name(empire_data *for_emp, char *name) {
	empire_data *emp, *next_emp;
	
	// check empires for same name
	HASH_ITER(hh, empire_table, emp, next_emp) {
		if (emp == for_emp) {
			continue;
		}
		
		if (!str_cmp(EMPIRE_NAME(emp), name) || !str_cmp(EMPIRE_ADJECTIVE(emp), name)) {
			return FALSE;
		}
	}
	
	return TRUE;	// all good
}


/**
* @param char *newname The proposed empire name.
* @return bool TRUE if the name is ok.
*/
bool valid_empire_name(char *newname) {
	extern char *invalid_list[MAX_INVALID_NAMES];
	extern int num_invalid;

	char *ptr, tempname[MAX_INPUT_LENGTH];
	bool ok = TRUE;
	int iter;
	
	// check for illegal & codes (anything other than &&)
	if ((ptr = strchr(newname, '&')) && *(ptr+1) != '&') {
		ok = FALSE;
	}
	
	// check fill/reserved
	strcpy(tempname, newname);
	if (fill_word(tempname) || reserved_word(tempname)) {
		ok = FALSE;
	}
	
	// banned names
	if (ok && num_invalid > 0) {
		strcpy(tempname, newname);
		for (iter = 0 ; iter < strlen(tempname); ++iter) {
			tempname[iter] = LOWER(tempname[iter]);
		}
		
		// compare to banned names
		for (iter = 0; iter < num_invalid && ok; ++iter) {
			if (*invalid_list[iter] == '%') {	// leading % means substr
				if (strstr(tempname, invalid_list[iter] + 1)) {
					ok = FALSE;
				}
			}
			else {	// otherwise exact-match
				if (!str_cmp(tempname, invalid_list[iter])) {
					ok = FALSE;
				}
			}
		}

	}
	
	return ok;
}


/**
* Validates rank names for empires. This will send an error message.
*
* @param char_data *ch The person to send errors to.
* @param char *newname The proposed name.
* @return bool TRUE if the name is ok, or FALSE otherwise.
*/
bool valid_rank_name(char_data *ch, char *newname) {
	char *upos, *zpos, *npos, *lastpos;
	int iter;
	bool ok = TRUE;
	
	char *valid = " -&'";
	
	if (color_strlen(newname) > MAX_RANK_LENGTH) {
		ok = FALSE;
	}

	for (iter = 0; iter < strlen(newname) && ok; ++iter) {
		if (!isalnum(newname[iter]) && !strchr(valid, newname[iter])) {
			msg_to_char(ch, "You can't use %c in rank names.\r\n", newname[iter]);
			ok = FALSE;
		}
	}
	
	// check underline termination
	upos = reverse_strstr(newname, "&u");
	zpos = reverse_strstr(newname, "&0");
	npos = reverse_strstr(newname, "&n");
	lastpos = (zpos && npos) ? MAX(zpos, npos) : (zpos ? zpos : npos);
	if (upos && (!lastpos || lastpos < upos)) {
		msg_to_char(ch, "If you use an underline in a rank name, you must end it with \t&0.\r\n");
		ok = FALSE;
	}
	
	return ok;
}



 //////////////////////////////////////////////////////////////////////////////
//// CONTROL STRUCTURE ///////////////////////////////////////////////////////

#define EEDIT_FLAG_IMM_ONLY  BIT(0)	// only a cimpl or granted imm can do this


const struct {
	char *command;
	EEDIT(*func);
	bitvector_t flags;
} eedit_cmd[] = {
	{ "adjective", eedit_adjective, NOBITS },
	{ "adminflags", eedit_admin_flags, EEDIT_FLAG_IMM_ONLY },
	{ "banner", eedit_banner, NOBITS },
	{ "changeleader", eedit_change_leader, NOBITS },
	{ "description", eedit_description, NOBITS },
	{ "frontiertraits", eedit_frontiertraits, NOBITS },
	{ "motd", eedit_motd, NOBITS },
	{ "name", eedit_name, NOBITS },
	{ "privilege", eedit_privilege, NOBITS },
	{ "rank", eedit_rank, NOBITS },
	{ "ranks", eedit_num_ranks, NOBITS },
	
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
		if (!imm_access && IS_SET(eedit_cmd[iter].flags, EEDIT_FLAG_IMM_ONLY)) {
			continue;
		}
		
		if (is_abbrev(arg, eedit_cmd[iter].command)) {
			type = iter;
		}
	}
	
	if (!IS_APPROVED(ch) && config_get_bool("manage_empire_approval")) {
		send_config_msg(ch, "need_approval_string");
	}
	else if (IS_NPC(ch) || !emp) {
		msg_to_char(ch, "You are not in any empire.\r\n");
	}
	else if (!imm_access && GET_RANK(ch) < EMPIRE_NUM_RANKS(emp)) {
		msg_to_char(ch, "Your rank is too low to edit the empire.\r\n");
	}
	else if (!*arg || type == NOTHING) {
		msg_to_char(ch, "Available eedit commands: ");
		found = FALSE;
		for (iter = 0; *eedit_cmd[iter].command != '\n'; ++iter) {
			if (!imm_access && IS_SET(eedit_cmd[iter].flags, EEDIT_FLAG_IMM_ONLY)) {
				continue;
			}
			
			msg_to_char(ch, "%s%s", (found ? ", " : ""), eedit_cmd[iter].command);
			found = TRUE;
		}
		msg_to_char(ch, "%s\r\n", (found ? "" : "none"));
	}
	else if (!imm_access && IS_SET(eedit_cmd[type].flags, EEDIT_FLAG_IMM_ONLY)) {
		msg_to_char(ch, "You don't have permission to do that.\r\n");
	}
	else {
		// pass to child function
		(eedit_cmd[type].func)(ch, argptr, emp);
		EMPIRE_NEEDS_SAVE(emp) = TRUE;
	}
}


 //////////////////////////////////////////////////////////////////////////////
//// EDITOR COMMANDS /////////////////////////////////////////////////////////

EEDIT(eedit_adjective) {
	argument = trim(argument);
	
	if (ACCOUNT_FLAGGED(ch, ACCT_NOTITLE)) {
		msg_to_char(ch, "You are not allowed to change the empire's adjective.\r\n");
	}
	else if (is_at_war(emp)) {
		msg_to_char(ch, "You can't rename your empire while at war.\r\n");
	}
	else if (!*argument) {
		msg_to_char(ch, "Set the empire's adjective form to what?\r\n");
	}
	else if (color_code_length(argument) > 0 || strchr(argument, '&') != NULL) {
		msg_to_char(ch, "Empire adjective forms may not contain color codes or ampersands. Set the banner instead.\r\n");
	}
	else if (strstr(argument, "%") != NULL) {
		msg_to_char(ch, "Empire adjective forms may not contain the percent sign (%%).\r\n");
	}
	else if (strlen(argument) > MAX_RANK_LENGTH) {
		msg_to_char(ch, "Adjective names are limited to %d characters.\r\n", MAX_RANK_LENGTH);
	}
	else if (!strcmp(argument, EMPIRE_ADJECTIVE(emp))) {
		msg_to_char(ch, "That's already the adjective.\r\n");
	}
	else if (!check_unique_empire_name(emp, argument)) {
		msg_to_char(ch, "That name is already in use.\r\n");
	}
	else if (!valid_empire_name(argument)) {
		msg_to_char(ch, "Invalid empire adjective.\r\n");
	}
	else {
		if (EMPIRE_ADJECTIVE(emp)) {
			free(EMPIRE_ADJECTIVE(emp));
		}
		EMPIRE_ADJECTIVE(emp) = str_dup(argument);
		
		log_to_empire(emp, ELOG_ADMIN, "%s has changed the empire's adjective form to %s", PERS(ch, ch, TRUE), EMPIRE_ADJECTIVE(emp));
		msg_to_char(ch, "The empire's adjective form is now: %s\r\n", EMPIRE_ADJECTIVE(emp));
		
		if (emp != GET_LOYALTY(ch)) {
			syslog(SYS_GC, GET_INVIS_LEV(ch), TRUE, "ABUSE: %s has changed %s's adjective form to %s", GET_NAME(ch), EMPIRE_NAME(emp), EMPIRE_ADJECTIVE(emp));
		}
	}
}


EEDIT(eedit_admin_flags) {
	extern const char *empire_admin_flags[];
	
	bitvector_t old_flags = EMPIRE_ADMIN_FLAGS(emp);
	char buf[MAX_STRING_LENGTH];
	
	EMPIRE_ADMIN_FLAGS(emp) = olc_process_flag(ch, argument, "admin flags", NULL, empire_admin_flags, EMPIRE_ADMIN_FLAGS(emp));
	
	if (EMPIRE_ADMIN_FLAGS(emp) != old_flags) {
		if (emp != GET_LOYALTY(ch)) {
			prettier_sprintbit(EMPIRE_ADMIN_FLAGS(emp), empire_admin_flags, buf);
			syslog(SYS_GC, GET_INVIS_LEV(ch), TRUE, "ABUSE: %s has changed %s's admin flags to: %s", GET_NAME(ch), EMPIRE_NAME(emp), buf);
		}
	}
}


EEDIT(eedit_banner) {
	extern char *show_color_codes(char *string);
	
	if (!*argument) {
		msg_to_char(ch, "Set the empire banner to what (HELP COLOR)?\r\n");
	}
	else if (!check_banner_color_string(argument)) {
		msg_to_char(ch, "Invalid banner color (HELP COLOR) or too many color codes.\r\n");
	}
	else {
		if (EMPIRE_BANNER(emp)) {
			free(EMPIRE_BANNER(emp));
		}
		EMPIRE_BANNER(emp) = str_dup(argument);
		
		EMPIRE_BANNER_HAS_UNDERLINE(emp) = (strstr(EMPIRE_BANNER(emp), "&u") ? TRUE : FALSE);

		log_to_empire(emp, ELOG_ADMIN, "%s has changed the banner color", PERS(ch, ch, TRUE));
		msg_to_char(ch, "The empire's banner is now: %s%s&0\r\n", EMPIRE_BANNER(emp), show_color_codes(EMPIRE_BANNER(emp)));
		
		if (emp != GET_LOYALTY(ch)) {
			syslog(SYS_GC, GET_INVIS_LEV(ch), TRUE, "ABUSE: %s has changed %s's banner to %s%s&0", GET_NAME(ch), EMPIRE_NAME(emp), EMPIRE_BANNER(emp), show_color_codes(EMPIRE_BANNER(emp)));
		}
	}
}


EEDIT(eedit_change_leader) {
	bool imm_access = GET_ACCESS_LEVEL(ch) >= LVL_CIMPL || IS_GRANTED(ch, GRANT_EMPIRES);
	player_index_data *index;
	bool file = FALSE;
	char_data *victim = NULL;
	int old_leader;
	
	one_argument(argument, arg);
	
	if (!imm_access && GET_IDNUM(ch) != EMPIRE_LEADER(emp)) {
		msg_to_char(ch, "Only the current leader can change leadership.\r\n");
	}
	else if (!*arg) {
		msg_to_char(ch, "Change the leader to whom?\r\n");
	}
	else if (!(victim = find_or_load_player(arg, &file))) {
		send_to_char("There is no such player.\r\n", ch);
	}
	else if (!imm_access && victim == ch) {
		msg_to_char(ch, "You are already the leader!\r\n");
	}
	else if (IS_NPC(victim) || GET_LOYALTY(victim) != emp) {
		msg_to_char(ch, "That person is not in the empire.\r\n");
	}
	else {
		old_leader = EMPIRE_LEADER(emp);
		
		// promote new leader
		GET_RANK(victim) = EMPIRE_NUM_RANKS(emp);
		EMPIRE_LEADER(emp) = GET_IDNUM(victim);

		log_to_empire(emp, ELOG_MEMBERS, "%s is now the leader of the empire!", PERS(victim, victim, TRUE));
		msg_to_char(ch, "You make %s leader of the empire.\r\n", PERS(victim, victim, TRUE));
		
		remove_lore(victim, LORE_PROMOTED);
		add_lore(victim, LORE_PROMOTED, "Became leader of %s%s&0", EMPIRE_BANNER(emp), EMPIRE_NAME(emp));
		
		if (emp != GET_LOYALTY(ch)) {
			syslog(SYS_GC, GET_INVIS_LEV(ch), TRUE, "ABUSE: %s has changed %s's leader to %s", GET_NAME(ch), EMPIRE_NAME(emp), GET_NAME(victim));
		}

		// save now
		if (file) {
			store_loaded_char(victim);
			file = FALSE;
		}
		else {
			SAVE_CHAR(victim);
		}
		
		// demote old leader (at least, in lore)
		if ((index = find_player_index_by_idnum(old_leader)) && (victim = find_or_load_player(index->name, &file))) {
			if (GET_LOYALTY(victim) == emp) {
				remove_lore(victim, LORE_PROMOTED);
				add_lore(victim, LORE_PROMOTED, "Stepped down as leader of %s%s&0", EMPIRE_BANNER(emp), EMPIRE_NAME(emp));
			
				// save now
				if (file) {
					store_loaded_char(victim);
					file = FALSE;
				}
				else {
					SAVE_CHAR(victim);
				}
			}
			else if (file) {
				free_char(victim);
			}
		}
	}
	
	// clean up
	if (file && victim) {
		free_char(victim);
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
	start_string_editor(ch->desc, buf, &EMPIRE_DESCRIPTION(emp), MAX_EMPIRE_DESCRIPTION, TRUE);
	ch->desc->save_empire = EMPIRE_VNUM(emp);
}


EEDIT(eedit_frontiertraits) {
	extern const char *empire_trait_types[];
	
	bitvector_t old_traits = EMPIRE_FRONTIER_TRAITS(emp);
	char buf[MAX_STRING_LENGTH];
	
	EMPIRE_FRONTIER_TRAITS(emp) = olc_process_flag(ch, argument, "frontier trait", NULL, empire_trait_types, EMPIRE_FRONTIER_TRAITS(emp));
	
	if (EMPIRE_FRONTIER_TRAITS(emp) != old_traits) {
		prettier_sprintbit(EMPIRE_FRONTIER_TRAITS(emp), empire_trait_types, buf);
		log_to_empire(emp, ELOG_ADMIN, "%s has changed the frontier traits to %s", PERS(ch, ch, TRUE), buf);
		
		if (emp != GET_LOYALTY(ch)) {
			syslog(SYS_GC, GET_INVIS_LEV(ch), TRUE, "ABUSE: %s has changed %s's frontier traits to %s", GET_NAME(ch), EMPIRE_NAME(emp), buf);
		}
	}
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
	start_string_editor(ch->desc, buf, &EMPIRE_MOTD(emp), MAX_MOTD_LENGTH, TRUE);
	ch->desc->save_empire = EMPIRE_VNUM(emp);
}


EEDIT(eedit_name) {
	player_index_data *index, *next_index;
	char buf[MAX_STRING_LENGTH];
	bool file = FALSE;
	char_data *mem;
	
	argument = trim(argument);
	CAP(argument);
	
	if (ACCOUNT_FLAGGED(ch, ACCT_NOTITLE)) {
		msg_to_char(ch, "You are not allowed to change the empire's name.\r\n");
	}
	else if (is_at_war(emp)) {
		msg_to_char(ch, "You can't rename your empire while at war.\r\n");
	}
	else if (!*argument) {
		msg_to_char(ch, "Set the empire name to what?\r\n");
	}
	else if (!isalpha(*argument)) {
		msg_to_char(ch, "Empire names must begin with a letter.\r\n");
	}
	else if (color_code_length(argument) > 0 || strchr(argument, '&') != NULL) {
		msg_to_char(ch, "Empire names may not contain color codes or ampersands. Set the banner instead.\r\n");
	}
	else if (strchr(argument, '%')) {
		msg_to_char(ch, "Empire names may not contain the percent sign (%%).\r\n");
	}
	else if (strchr(argument, '"')) {
		msg_to_char(ch, "Empire names may not contain a quotation mark (\").\r\n");
	}
	else if (!strcmp(argument, EMPIRE_NAME(emp))) {
		msg_to_char(ch, "It's already called that.\r\n");
	}
	else if (!check_unique_empire_name(emp, argument)) {
		msg_to_char(ch, "That name is already in use.\r\n");
	}
	else if (!valid_empire_name(argument)) {
		msg_to_char(ch, "Invalid empire name.\r\n");
	}
	else {
		strcpy(buf, NULLSAFE(EMPIRE_NAME(emp)));
		if (EMPIRE_NAME(emp)) {
			free(EMPIRE_NAME(emp));
		}
		EMPIRE_NAME(emp) = str_dup(argument);
		
		if (EMPIRE_ADJECTIVE(emp)) {
			free(EMPIRE_ADJECTIVE(emp));
		}
		EMPIRE_ADJECTIVE(emp) = str_dup(argument);
		
		log_to_empire(emp, ELOG_ADMIN, "%s has changed the empire name to %s", PERS(ch, ch, TRUE), EMPIRE_NAME(emp));
		msg_to_char(ch, "The empire's name is now: %s\r\n", EMPIRE_NAME(emp));
		msg_to_char(ch, "The adjective form was also changed (use 'eedit adjective' to change it).\r\n");
		
		if (emp != GET_LOYALTY(ch)) {
			syslog(SYS_GC, GET_INVIS_LEV(ch), TRUE, "ABUSE: %s has changed %s's empire name to %s", GET_NAME(ch), buf, EMPIRE_NAME(emp));
		}
		
		// update lore for members
		HASH_ITER(idnum_hh, player_table_by_idnum, index, next_index) {
			if (index->loyalty != emp) {
				continue;
			}
			
			if ((mem = find_or_load_player(index->name, &file))) {
				remove_recent_lore(mem, LORE_JOIN_EMPIRE);
				add_lore(mem, LORE_JOIN_EMPIRE, "Empire became %s%s&0", EMPIRE_BANNER(emp), EMPIRE_NAME(emp));
				
				if (file) {
					store_loaded_char(mem);
				}
			}
		}
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
		
		if (emp != GET_LOYALTY(ch)) {
			syslog(SYS_GC, GET_INVIS_LEV(ch), TRUE, "ABUSE: %s has changed %s's %s privilege to rank %s&0 (%d)", GET_NAME(ch), EMPIRE_NAME(emp), priv[pr], EMPIRE_RANK(emp, rnk), rnk+1);
		}
	}
}


EEDIT(eedit_rank) {
	player_index_data *index, *next_index;
	char_data *mem;
	int rnk, iter;
	bool file;

	argument = any_one_word(argument, arg);
	argument = trim(argument);
	CAP(argument);
	
	if (ACCOUNT_FLAGGED(ch, ACCT_NOTITLE)) {
		msg_to_char(ch, "You are not allowed to change the empire's rank names.\r\n");
	}
	else if (!*argument || !*arg) {
		msg_to_char(ch, "Usage: eedit rank <rank> <new name>\r\n");
		msg_to_char(ch, "Current ranks:\r\n");
		for (iter = 0; iter < EMPIRE_NUM_RANKS(emp); ++iter) {
			msg_to_char(ch, " %d. %s&0\r\n", iter+1, EMPIRE_RANK(emp, iter));
		}
	}
	else if ((rnk = find_rank_by_name(emp, arg)) == NOTHING) {
		msg_to_char(ch, "Invalid rank.\r\n");
	}
	else if (!valid_rank_name(ch, argument)) {
		// sends own message
		// msg_to_char(ch, "Invalid rank name.\r\n");
	}
	else {
		if (EMPIRE_RANK(emp, rnk)) {
			free(EMPIRE_RANK(emp, rnk));
		}
		EMPIRE_RANK(emp, rnk) = str_dup(argument);
		
		log_to_empire(emp, ELOG_ADMIN, "%s has changed rank %d to %s%s", PERS(ch, ch, TRUE), rnk+1, EMPIRE_RANK(emp, rnk), EMPIRE_BANNER(emp));
		msg_to_char(ch, "You have changed rank %d to %s&0.\r\n", rnk+1, EMPIRE_RANK(emp, rnk));
		
		if (emp != GET_LOYALTY(ch)) {
			syslog(SYS_GC, GET_INVIS_LEV(ch), TRUE, "ABUSE: %s has changed %s's rank %d to %s&0", GET_NAME(ch), EMPIRE_NAME(emp), rnk+1, EMPIRE_RANK(emp, rnk));
		}
		
		// update lore for members
		HASH_ITER(idnum_hh, player_table_by_idnum, index, next_index) {
			if (index->loyalty != emp || rnk != (index->rank - 1)) {
				continue;
			}
			
			if ((mem = find_or_load_player(index->name, &file))) {
				remove_lore(mem, LORE_PROMOTED);	// only save most recent
				add_lore(mem, LORE_PROMOTED, "Became %s&0", EMPIRE_RANK(emp, rnk));
				
				if (file) {
					store_loaded_char(mem);
				}
			}
		}
	}
}


EEDIT(eedit_num_ranks) {
	player_index_data *index, *next_index;
	archetype_data *arch;
	int num, iter;
	char_data *mem;
	bool is_file = FALSE;
	
	if (!*argument) {
		msg_to_char(ch, "How many ranks would you like to have (2-%d)?\r\n", MAX_RANKS);
	}
	else if ((num = atoi(argument)) < 2 || num > MAX_RANKS) {
		msg_to_char(ch, "You must choose a number of ranks between 2 and %d.\r\n", MAX_RANKS);
	}
	else {
		// update all players
		HASH_ITER(idnum_hh, player_table_by_idnum, index, next_index) {
			if (index->loyalty != emp) {
				continue;
			}
			if (!(mem = find_or_load_player(index->name, &is_file))) {
				continue;
			}
			
			if (GET_RANK(mem) == EMPIRE_NUM_RANKS(emp)) {
				// equal to old max
				GET_RANK(mem) = num;
			}
			else if (GET_RANK(mem) >= num) {
				// too high for new max
				GET_RANK(mem) = num - 1;
			}
			
			update_player_index(index, mem);
			
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
		
		// remove old rank names
		for (iter = num; iter < EMPIRE_NUM_RANKS(emp); ++iter) {
			if (EMPIRE_RANK(emp, iter)) {
				free(EMPIRE_RANK(emp, iter));
			}
			EMPIRE_RANK(emp, iter) = NULL;
		}
		
		arch = archetype_proto(CREATION_ARCHETYPE(ch, ARCHT_ORIGIN));
		if (!arch) {
			arch = archetype_proto(0);	// default
		}
		
		// ensure all new ranks have names
		for (iter = 0; iter < num; ++iter) {
			if (!EMPIRE_RANK(emp, iter)) {
				EMPIRE_RANK(emp, iter) = str_dup((iter < (num-1)) ? "Follower" : (arch ? (GET_REAL_SEX(ch) == SEX_FEMALE ? GET_ARCH_FEMALE_RANK(arch) : GET_ARCH_MALE_RANK(arch)) : "Leader"));
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
		
		if (emp != GET_LOYALTY(ch)) {
			syslog(SYS_GC, GET_INVIS_LEV(ch), TRUE, "ABUSE: %s has changed %s's number of ranks to %d", GET_NAME(ch), EMPIRE_NAME(emp), EMPIRE_NUM_RANKS(emp));
		}
	}
}
