/* ************************************************************************
*   File: social.c                                        EmpireMUD 2.0b5 *
*  Usage: social loading, saving, OLC, and processing                     *
*                                                                         *
*  EmpireMUD code base by Paul Clarke, (C) 2000-2015                      *
*  All rights reserved.  See license.doc for complete information.        *
*                                                                         *
*  EmpireMUD based upon CircleMUD 3.0, bpl 17, by Jeremy Elson.           *
*  CircleMUD (C) 1993, 94 by the Trustees of the Johns Hopkins University *
*  CircleMUD is based on DikuMUD, Copyright (C) 1990, 1991.               *
************************************************************************ */

#include <math.h>

#include "conf.h"
#include "sysdep.h"

#include "structs.h"
#include "utils.h"
#include "interpreter.h"
#include "db.h"
#include "comm.h"
#include "olc.h"
#include "skills.h"
#include "handler.h"


/**
* Contents:
*   Helpers
*   Utilities
*   Database
*   OLC Handlers
*   Displays
*   Edit Modules
*/

// local data
const char *default_social_command = "social";
const char *default_social_name = "Unnamed Social";
const int default_social_position = POS_RESTING;

// external consts
extern const char *position_types[];
extern const char *social_flags[];
extern const char *social_message_types[NUM_SOCM_MESSAGES][2];

// external funcs
void get_requirement_display(struct req_data *list, char *save_buffer);


 //////////////////////////////////////////////////////////////////////////////
//// HELPERS /////////////////////////////////////////////////////////////////

/**
* Handler for social message fields in the editor.
*
* @param char_data *ch The editing player.
* @param char *argument The argument typed.
* @param int msg Which SOCM_ const.
*/
void process_soc_msg_field(char_data *ch, char *argument, int msg) {
	social_data *soc = GET_OLC_SOCIAL(ch->desc);
	
	if (!str_cmp(argument, "none")) {
		if (SOC_MESSAGE(soc, msg)) {
			free(SOC_MESSAGE(soc, msg));
		}
		SOC_MESSAGE(soc, msg) = NULL;
		
		if (PRF_FLAGGED(ch, PRF_NOREPEAT)) {
			send_config_msg(ch, "ok_string");
		}
		else {
			msg_to_char(ch, "%s message removed.\r\n", social_message_types[msg][0]);
		}
	}
	else {
		olc_process_string(ch, argument, social_message_types[msg][0], &SOC_MESSAGE(soc, msg));
	}
}


/**
* Checks that a character meets all requirements for a social.
*
* @param char_data *ch The person to check.
* @param social_data *soc The social they want to use.
* @return bool TRUE if the character can do it, FALSE if not.
*/
bool validate_social_requirements(char_data *ch, social_data *soc) {
	extern bool meets_requirements(char_data *ch, struct req_data *list, struct instance_data *instance);
	return meets_requirements(ch, SOC_REQUIREMENTS(soc), NULL);
}


 //////////////////////////////////////////////////////////////////////////////
//// UTILITIES ///////////////////////////////////////////////////////////////

/**
* Checks for common social problems and reports them to ch.
*
* @param social_data *soc The item to audit.
* @param char_data *ch The person to report to.
* @return bool TRUE if any problems were reported; FALSE if all good.
*/
bool audit_social(social_data *soc, char_data *ch) {
	char temp[MAX_STRING_LENGTH];
	bool problem = FALSE;
	
	if (SOCIAL_FLAGGED(soc, SOC_IN_DEVELOPMENT)) {
		olc_audit_msg(ch, SOC_VNUM(soc), "IN-DEVELOPMENT");
		problem = TRUE;
	}
	if (!SOC_NAME(soc) || !*SOC_NAME(soc) || !str_cmp(SOC_NAME(soc), default_social_name)) {
		olc_audit_msg(ch, SOC_VNUM(soc), "No name set");
		problem = TRUE;
	}
	if (!SOC_COMMAND(soc) || !*SOC_COMMAND(soc) || !str_cmp(SOC_COMMAND(soc), default_social_command)) {
		olc_audit_msg(ch, SOC_VNUM(soc), "No command set");
		problem = TRUE;
	}
	if (strchr(SOC_COMMAND(soc), ' ')) {
		olc_audit_msg(ch, SOC_VNUM(soc), "Command contains a space");
		problem = TRUE;
	}
	
	strcpy(temp, SOC_COMMAND(soc));
	strtolower(temp);
	if (strcmp(SOC_COMMAND(soc), temp)) {
		olc_audit_msg(ch, SOC_VNUM(soc), "Non-lowercase social command");
		problem = TRUE;
	}
	
	// odd combos of messages
	if (!SOC_MESSAGE(soc, SOCM_NO_ARG_TO_CHAR)) {
		olc_audit_msg(ch, SOC_VNUM(soc), "Social needs n2char");
		problem = TRUE;
	}
	if (!SOC_MESSAGE(soc, SOCM_TARGETED_TO_CHAR) && (SOC_MESSAGE(soc, SOCM_TARGETED_TO_OTHERS) || SOC_MESSAGE(soc, SOCM_TARGETED_TO_VICTIM))) {
		olc_audit_msg(ch, SOC_VNUM(soc), "Social has t2other/t2vict but not t2char");
		problem = TRUE;
	}
	if (!SOC_MESSAGE(soc, SOCM_TARGETED_NOT_FOUND) && (SOC_MESSAGE(soc, SOCM_TARGETED_TO_CHAR) || SOC_MESSAGE(soc, SOCM_TARGETED_TO_OTHERS) || SOC_MESSAGE(soc, SOCM_TARGETED_TO_VICTIM))) {
		olc_audit_msg(ch, SOC_VNUM(soc), "Social has t2char/t2other/t2vict but not tnotfound");
		problem = TRUE;
	}
	if (SOC_MESSAGE(soc, SOCM_SELF_TO_OTHERS) && !SOC_MESSAGE(soc, SOCM_SELF_TO_CHAR)) {
		olc_audit_msg(ch, SOC_VNUM(soc), "Social has s2other but not s2char");
		problem = TRUE;
	}
	if (!SOC_MESSAGE(soc, SOCM_TARGETED_TO_CHAR) && (SOC_MESSAGE(soc, SOCM_SELF_TO_CHAR) || SOC_MESSAGE(soc, SOCM_SELF_TO_OTHERS))) {
		olc_audit_msg(ch, SOC_VNUM(soc), "Social has s2char/s2other but not t2char (required)");
		problem = TRUE;
	}
	
	return problem;
}


/**
* For the .list command.
*
* @param social_data *soc The thing to list.
* @param bool detail If TRUE, provide additional details
* @return char* The line to show (without a CRLF).
*/
char *list_one_social(social_data *soc, bool detail) {
	static char output[MAX_STRING_LENGTH];
	char buf[MAX_STRING_LENGTH];
	struct req_data *req;
	int count;
	
	if (detail) {
		if (SOC_REQUIREMENTS(soc)) {
			LL_COUNT(SOC_REQUIREMENTS(soc), req, count);
			snprintf(buf, sizeof(buf), " [%d requirements]", count);
		}
		else {
			*buf = '\0';
		}
		
		snprintf(output, sizeof(output), "[%5d] %s (%s)%s%s", SOC_VNUM(soc), SOC_NAME(soc), SOC_COMMAND(soc), buf, (SOCIAL_FLAGGED(soc, SOC_IN_DEVELOPMENT) ? " IN-DEV" : ""));
		// TODO could show in-dev flag
	}
	else {
		snprintf(output, sizeof(output), "[%5d] %s (%s)", SOC_VNUM(soc), SOC_NAME(soc), SOC_COMMAND(soc));
	}
		
	return output;
}


/**
* Searches for all uses of a social and displays them.
*
* @param char_data *ch The player.
* @param any_vnum vnum The social vnum.
*/
void olc_search_social(char_data *ch, any_vnum vnum) {
	char buf[MAX_STRING_LENGTH];
	social_data *soc = social_proto(vnum);
	int size, found;
	
	if (!soc) {
		msg_to_char(ch, "There is no social %d.\r\n", vnum);
		return;
	}
	
	found = 0;
	size = snprintf(buf, sizeof(buf), "Occurrences of social %d (%s):\r\n", vnum, SOC_NAME(soc));
	
	// socials are not actually used anywhere else
	
	if (found > 0) {
		size += snprintf(buf + size, sizeof(buf) - size, "%d location%s shown\r\n", found, PLURAL(found));
	}
	else {
		size += snprintf(buf + size, sizeof(buf) - size, " none\r\n");
	}
	
	page_string(ch->desc, buf, TRUE);
}


// Simple vnum sorter for the socials hash
int sort_socials(social_data *a, social_data *b) {
	return SOC_VNUM(a) - SOC_VNUM(b);
}


// typealphabetic sorter for sorted_socials
int sort_socials_by_data(social_data *a, social_data *b) {
	struct req_data *req;
	int a_reqs, b_reqs, diff;
	
	// name first
	diff = str_cmp(NULLSAFE(SOC_COMMAND(a)), NULLSAFE(SOC_COMMAND(b)));
	if (diff != 0) {
		return diff;
	}
	
	// number of requirements second
	LL_COUNT(SOC_REQUIREMENTS(a), req, a_reqs);
	LL_COUNT(SOC_REQUIREMENTS(b), req, b_reqs);
	if (a_reqs != b_reqs) {
		return b_reqs - a_reqs;	// descending
	}
	
	// lastly...?
	return 0;
}


 //////////////////////////////////////////////////////////////////////////////
//// DATABASE ////////////////////////////////////////////////////////////////

/**
* Puts a social into the hash table.
*
* @param social_data *soc The social data to add to the table.
*/
void add_social_to_table(social_data *soc) {
	social_data *find;
	any_vnum vnum;
	
	if (soc) {
		vnum = SOC_VNUM(soc);
		HASH_FIND_INT(social_table, &vnum, find);
		if (!find) {
			HASH_ADD_INT(social_table, vnum, soc);
			HASH_SORT(social_table, sort_socials);
		}
		
		// sorted table
		HASH_FIND(sorted_hh, sorted_socials, &vnum, sizeof(int), find);
		if (!find) {
			HASH_ADD(sorted_hh, sorted_socials, vnum, sizeof(int), soc);
			HASH_SRT(sorted_hh, sorted_socials, sort_socials_by_data);
		}
	}
}


/**
* Removes a social from the hash table.
*
* @param social_data *soc The social data to remove from the table.
*/
void remove_social_from_table(social_data *soc) {
	HASH_DEL(social_table, soc);
	HASH_DELETE(sorted_hh, sorted_socials, soc);
}


/**
* Initializes a new social. This clears all memory for it, so set the vnum
* AFTER.
*
* @param social_data *soc The social to initialize.
*/
void clear_social(social_data *soc) {
	memset((char *) soc, 0, sizeof(social_data));
	
	SOC_VNUM(soc) = NOTHING;
}


/**
* frees up memory for a social data item.
*
* See also: olc_delete_social
*
* @param social_data *soc The social data to free.
*/
void free_social(social_data *soc) {
	social_data *proto = social_proto(SOC_VNUM(soc));
	int iter;
	
	if (SOC_COMMAND(soc) && (!proto || SOC_COMMAND(soc) != SOC_COMMAND(proto))) {
		free(SOC_COMMAND(soc));
	}
	if (SOC_NAME(soc) && (!proto || SOC_NAME(soc) != SOC_NAME(proto))) {
		free(SOC_NAME(soc));
	}
	
	for (iter = 0; iter < NUM_SOCM_MESSAGES; ++iter) {
		if (SOC_MESSAGE(soc, iter) && (!proto || SOC_MESSAGE(soc, iter) != SOC_MESSAGE(proto, iter))) {
			free(SOC_MESSAGE(soc, iter));
		}
	}
	
	if (SOC_REQUIREMENTS(soc) && (!proto || SOC_REQUIREMENTS(soc) != SOC_REQUIREMENTS(proto))) {
		free_requirements(SOC_REQUIREMENTS(soc));
	}
	
	free(soc);
}


/**
* Read one social from file.
*
* @param FILE *fl The open .soc file
* @param any_vnum vnum The social vnum
*/
void parse_social(FILE *fl, any_vnum vnum) {
	void parse_requirement(FILE *fl, struct req_data **list, char *error_str);
	
	char line[256], error[256], str_in[256], *ptr;
	social_data *soc, *find;
	int int_in[4];
	
	CREATE(soc, social_data, 1);
	clear_social(soc);
	SOC_VNUM(soc) = vnum;
	
	HASH_FIND_INT(social_table, &vnum, find);
	if (find) {
		log("WARNING: Duplicate social vnum #%d", vnum);
		// but have to load it anyway to advance the file
	}
	add_social_to_table(soc);
		
	// for error messages
	sprintf(error, "social vnum %d", vnum);
	
	// lines 1-2: strings
	SOC_NAME(soc) = fread_string(fl, error);
	SOC_COMMAND(soc) = fread_string(fl, error);
	
	// line 3: type flags wear-flags ability
	if (!get_line(fl, line) || sscanf(line, "%s %d %d", str_in, &int_in[0], &int_in[1]) != 3) {
		log("SYSERR: Format error in line 23of %s", error);
		exit(1);
	}
	
	SOC_FLAGS(soc) = asciiflag_conv(str_in);
	SOC_MIN_CHAR_POS(soc) = int_in[0];
	SOC_MIN_VICT_POS(soc) = int_in[1];
	
	// optionals
	for (;;) {
		if (!get_line(fl, line)) {
			log("SYSERR: Format error in %s, expecting alphabetic flags", error);
			exit(1);
		}
		switch (*line) {
			case 'L': {	// requirements
				parse_requirement(fl, &SOC_REQUIREMENTS(soc), error);
				break;
			}
			
			case 'M': {	// messages
				int_in[0] = atoi(line+1);
				ptr = fread_string(fl, error);
				if (int_in[0] >= 0 && int_in[0] < NUM_SOCM_MESSAGES) {
					SOC_MESSAGE(soc, int_in[0]) = ptr;
				}
				else {	// not valid; throw it away
					free(ptr);
				}
				break;
			}
			
			// end
			case 'S': {
				return;
			}
			
			default: {
				log("SYSERR: Format error in %s, expecting alphabetic flags", error);
				exit(1);
			}
		}
	}
}


/**
* @param any_vnum vnum Any social vnum
* @return social_data* The social, or NULL if it doesn't exist
*/
social_data *social_proto(any_vnum vnum) {
	social_data *soc;
	
	if (vnum < 0 || vnum == NOTHING) {
		return NULL;
	}
	
	HASH_FIND_INT(social_table, &vnum, soc);
	return soc;
}


// writes entries in the social index
void write_socials_index(FILE *fl) {
	social_data *soc, *next_soc;
	int this, last;
	
	last = -1;
	HASH_ITER(hh, social_table, soc, next_soc) {
		// determine "zone number" by vnum
		this = (int)(SOC_VNUM(soc) / 100);
	
		if (this != last) {
			fprintf(fl, "%d%s\n", this, SOC_SUFFIX);
			last = this;
		}
	}
}


/**
* Outputs one social item in the db file format, starting with a #VNUM and
* ending with an S.
*
* @param FILE *fl The file to write it to.
* @param social_data *soc The thing to save.
*/
void write_social_to_file(FILE *fl, social_data *soc) {
	void write_requirements_to_file(FILE *fl, char letter, struct req_data *list);
	
	char temp[256];
	int iter;
	
	if (!fl || !soc) {
		syslog(SYS_ERROR, LVL_START_IMM, TRUE, "SYSERR: write_social_to_file called without %s", !fl ? "file" : "social");
		return;
	}
	
	fprintf(fl, "#%d\n", SOC_VNUM(soc));
	
	// 1. name
	fprintf(fl, "%s~\n", NULLSAFE(SOC_NAME(soc)));
	
	// 2. command
	fprintf(fl, "%s~\n", NULLSAFE(SOC_COMMAND(soc)));
	
	// 3. flags min-char-pos min-vict-pos
	strcpy(temp, bitv_to_alpha(SOC_FLAGS(soc)));
	fprintf(fl, "%s %d %d\n", temp, SOC_MIN_CHAR_POS(soc), SOC_MIN_VICT_POS(soc));
	
	// 'L' requires
	write_requirements_to_file(fl, 'L', SOC_REQUIREMENTS(soc));
	
	// 'M' messages
	for (iter = 0; iter < NUM_SOCM_MESSAGES; ++iter) {
		if (SOC_MESSAGE(soc, iter)) {
			fprintf(fl, "M%d\n%s~\n", iter, SOC_MESSAGE(soc, iter));
		}
	}
	
	// end
	fprintf(fl, "S\n");
}


 //////////////////////////////////////////////////////////////////////////////
//// OLC HANDLERS ////////////////////////////////////////////////////////////

/**
* Creates a new social entry.
* 
* @param any_vnum vnum The number to create.
* @return social_data* The new social's prototype.
*/
social_data *create_social_table_entry(any_vnum vnum) {
	social_data *soc;
	
	// sanity
	if (social_proto(vnum)) {
		log("SYSERR: Attempting to insert social at existing vnum %d", vnum);
		return social_proto(vnum);
	}
	
	CREATE(soc, social_data, 1);
	clear_social(soc);
	SOC_VNUM(soc) = vnum;
	SOC_COMMAND(soc) = str_dup(default_social_command);
	SOC_NAME(soc) = str_dup(default_social_name);
	add_social_to_table(soc);

	// save index and social file now
	save_index(DB_BOOT_SOC);
	save_library_file_for_vnum(DB_BOOT_SOC, vnum);

	return soc;
}


/**
* WARNING: This function actually deletes a social.
*
* @param char_data *ch The person doing the deleting.
* @param any_vnum vnum The vnum to delete.
*/
void olc_delete_social(char_data *ch, any_vnum vnum) {
	social_data *soc;
	
	if (!(soc = social_proto(vnum))) {
		msg_to_char(ch, "There is no such social %d.\r\n", vnum);
		return;
	}
	
	// remove it from the hash table first
	remove_social_from_table(soc);

	// save index and social file now
	save_index(DB_BOOT_SOC);
	save_library_file_for_vnum(DB_BOOT_SOC, vnum);
	
	syslog(SYS_OLC, GET_INVIS_LEV(ch), TRUE, "OLC: %s has deleted social %d", GET_NAME(ch), vnum);
	msg_to_char(ch, "Social %d deleted.\r\n", vnum);
	
	free_social(soc);
}


/**
* Function to save a player's changes to a social (or a new one).
*
* @param descriptor_data *desc The descriptor who is saving.
*/
void save_olc_social(descriptor_data *desc) {	
	social_data *proto, *soc = GET_OLC_SOCIAL(desc);
	any_vnum vnum = GET_OLC_VNUM(desc);
	UT_hash_handle hh, sorted;
	int iter;

	// have a place to save it?
	if (!(proto = social_proto(vnum))) {
		proto = create_social_table_entry(vnum);
	}
	
	// free prototype strings and pointers
	if (SOC_COMMAND(proto)) {
		free(SOC_COMMAND(proto));
	}
	if (SOC_NAME(proto)) {
		free(SOC_NAME(proto));
	}
	for (iter = 0; iter < NUM_SOCM_MESSAGES; ++iter) {
		if (SOC_MESSAGE(proto, iter)) {
			free(SOC_MESSAGE(proto, iter));
		}
	}
	free_requirements(SOC_REQUIREMENTS(proto));
	
	// sanity
	if (!SOC_COMMAND(soc) || !*SOC_COMMAND(soc)) {
		if (SOC_COMMAND(soc)) {
			free(SOC_COMMAND(soc));
		}
		SOC_COMMAND(soc) = str_dup(default_social_command);
	}
	if (!SOC_NAME(soc) || !*SOC_NAME(soc)) {
		if (SOC_NAME(soc)) {
			free(SOC_NAME(soc));
		}
		SOC_NAME(soc) = str_dup(default_social_name);
	}
	
	// save data back over the proto-type
	hh = proto->hh;	// save old hash handle
	sorted = proto->sorted_hh;
	*proto = *soc;	// copy over all data
	proto->vnum = vnum;	// ensure correct vnum
	proto->hh = hh;	// restore old hash handle
	proto->sorted_hh = sorted;
		
	// and save to file
	save_library_file_for_vnum(DB_BOOT_SOC, vnum);

	// ... and re-sort
	HASH_SRT(sorted_hh, sorted_socials, sort_socials_by_data);
}


/**
* Creates a copy of a social, or clears a new one, for editing.
* 
* @param social_data *input The social to copy, or NULL to make a new one.
* @return social_data* The copied social.
*/
social_data *setup_olc_social(social_data *input) {
	extern struct req_data *copy_requirements(struct req_data *from);
	
	social_data *new;
	int iter;
	
	CREATE(new, social_data, 1);
	clear_social(new);
	
	if (input) {
		// copy normal data
		*new = *input;

		// copy things that are pointers
		SOC_COMMAND(new) = SOC_COMMAND(input) ? str_dup(SOC_COMMAND(input)) : NULL;
		SOC_NAME(new) = SOC_NAME(input) ? str_dup(SOC_NAME(input)) : NULL;
		SOC_REQUIREMENTS(new) = copy_requirements(SOC_REQUIREMENTS(input));
		
		for (iter = 0; iter < NUM_SOCM_MESSAGES; ++iter) {
			SOC_MESSAGE(new, iter) = SOC_MESSAGE(input, iter) ? str_dup(SOC_MESSAGE(input, iter)) : NULL;
		}
	}
	else {
		// brand new: some defaults
		SOC_COMMAND(new) = str_dup(default_social_command);
		SOC_NAME(new) = str_dup(default_social_name);
		SOC_FLAGS(new) = SOC_IN_DEVELOPMENT;
		SOC_MIN_CHAR_POS(new) = default_social_position;
		SOC_MIN_VICT_POS(new) = default_social_position;
	}
	
	// done
	return new;
}


 //////////////////////////////////////////////////////////////////////////////
//// DISPLAYS ////////////////////////////////////////////////////////////////

/**
* For vstat.
*
* @param char_data *ch The player requesting stats.
* @param social_data *soc The social to display.
*/
void do_stat_social(char_data *ch, social_data *soc) {
	char buf[MAX_STRING_LENGTH], part[MAX_STRING_LENGTH];
	size_t size;
	int iter;
	
	if (!soc) {
		return;
	}
	
	// first line
	size = snprintf(buf, sizeof(buf), "VNum: [\tc%d\t0], Command: \tc%s\t0, Name: \tc%s\t0\r\n", SOC_VNUM(soc), SOC_COMMAND(soc), SOC_NAME(soc));
	
	size += snprintf(buf + size, sizeof(buf) - size, "Min actor position: \ty%s\t0, Min victim position: \ty%s\t0\r\n", position_types[SOC_MIN_CHAR_POS(soc)], position_types[SOC_MIN_VICT_POS(soc)]);
	
	sprintbit(SOC_FLAGS(soc), social_flags, part, TRUE);
	size += snprintf(buf + size, sizeof(buf) - size, "Flags: \tg%s\t0\r\n", part);
	
	get_requirement_display(SOC_REQUIREMENTS(soc), part);
	size += snprintf(buf + size, sizeof(buf) - size, "Requirements:\r\n%s", *part ? part : " none\r\n");
	
	size += snprintf(buf + size, sizeof(buf) - size, "Messages:\r\n");
	for (iter = 0; iter < NUM_SOCM_MESSAGES; ++iter) {
		size += snprintf(buf + size, sizeof(buf) - size, "\tc%s\t0: %s\r\n", social_message_types[iter][0], SOC_MESSAGE(soc, iter) ? SOC_MESSAGE(soc, iter) : "(none)");
	}
	
	page_string(ch->desc, buf, TRUE);
}


/**
* This is the main recipe display for social OLC. It displays the user's
* currently-edited social.
*
* @param char_data *ch The person who is editing a social and will see its display.
*/
void olc_show_social(char_data *ch) {
	social_data *soc = GET_OLC_SOCIAL(ch->desc);
	char buf[MAX_STRING_LENGTH], lbuf[MAX_STRING_LENGTH];
	int iter;
	
	if (!soc) {
		return;
	}
	
	*buf = '\0';
	
	sprintf(buf + strlen(buf), "[\tc%d\t0] \tc%s\t0\r\n", GET_OLC_VNUM(ch->desc), !social_proto(SOC_VNUM(soc)) ? "new social" : SOC_NAME(social_proto(SOC_VNUM(soc))));
	sprintf(buf + strlen(buf), "<\tyname\t0> %s\r\n", NULLSAFE(SOC_NAME(soc)));
	sprintf(buf + strlen(buf), "<\tycommand\t0> %s\r\n", NULLSAFE(SOC_COMMAND(soc)));
	
	sprintbit(SOC_FLAGS(soc), social_flags, lbuf, TRUE);
	sprintf(buf + strlen(buf), "<\tyflags\t0> %s\r\n", lbuf);
	
	sprintf(buf + strlen(buf), "<\tycharposition\t0> %s (minimum)\r\n", position_types[SOC_MIN_CHAR_POS(soc)]);
	sprintf(buf + strlen(buf), "<\tytargetposition\t0> %s (minimum)\r\n", position_types[SOC_MIN_VICT_POS(soc)]);
	
	get_requirement_display(SOC_REQUIREMENTS(soc), lbuf);
	sprintf(buf + strlen(buf), "Requirements: <\tyrequirements\t0>\r\n%s", lbuf);
	
	sprintf(buf + strlen(buf), "Messages:\r\n");
	for (iter = 0; iter < NUM_SOCM_MESSAGES; ++iter) {
		sprintf(buf + strlen(buf), "%s <\ty%s\t0>: %s\r\n", social_message_types[iter][0], social_message_types[iter][1], SOC_MESSAGE(soc, iter) ? SOC_MESSAGE(soc, iter) : "(none)");
	}
	
	page_string(ch->desc, buf, TRUE);
}


/**
* Searches the social db for a match, and prints it to the character.
*
* @param char *searchname The search string.
* @param char_data *ch The player who is searching.
* @return int The number of matches shown.
*/
int vnum_social(char *searchname, char_data *ch) {
	social_data *iter, *next_iter;
	int found = 0;
	
	HASH_ITER(hh, social_table, iter, next_iter) {
		if (multi_isname(searchname, SOC_NAME(iter)) || multi_isname(searchname, SOC_COMMAND(iter))) {
			msg_to_char(ch, "%3d. [%5d] %s (%s)\r\n", ++found, SOC_VNUM(iter), SOC_NAME(iter), SOC_COMMAND(iter));
		}
	}
	
	return found;
}


 //////////////////////////////////////////////////////////////////////////////
//// OLC MODULES /////////////////////////////////////////////////////////////

OLC_MODULE(socedit_charposition) {
	social_data *soc = GET_OLC_SOCIAL(ch->desc);
	SOC_MIN_CHAR_POS(soc) = olc_process_type(ch, argument, "position", "charposition", position_types, SOC_MIN_CHAR_POS(soc));
}


OLC_MODULE(socedit_command) {
	social_data *soc = GET_OLC_SOCIAL(ch->desc);
	
	if (strchr(argument, ' ')) {
		msg_to_char(ch, "Social commands must be all one word.\r\n");
	}
	else {
		olc_process_string(ch, argument, "command", &SOC_COMMAND(soc));
	}
}


OLC_MODULE(socedit_flags) {
	social_data *soc = GET_OLC_SOCIAL(ch->desc);
	bool had_indev = IS_SET(SOC_FLAGS(soc), SOC_IN_DEVELOPMENT) ? TRUE : FALSE;
	
	SOC_FLAGS(soc) = olc_process_flag(ch, argument, "social", "flags", social_flags, SOC_FLAGS(soc));
	
	// validate removal of IN-DEVELOPMENT
	if (had_indev && !IS_SET(SOC_FLAGS(soc), SOC_IN_DEVELOPMENT) && GET_ACCESS_LEVEL(ch) < LVL_UNRESTRICTED_BUILDER && !OLC_FLAGGED(ch, OLC_FLAG_CLEAR_IN_DEV)) {
		msg_to_char(ch, "You don't have permission to remove the IN-DEVELOPMENT flag.\r\n");
		SET_BIT(SOC_FLAGS(soc), SOC_IN_DEVELOPMENT);
	}
}


OLC_MODULE(socedit_name) {
	social_data *soc = GET_OLC_SOCIAL(ch->desc);
	olc_process_string(ch, argument, "name", &SOC_NAME(soc));
}


OLC_MODULE(socedit_requirements) {
	void olc_process_requirements(char_data *ch, char *argument, struct req_data **list, char *command, bool allow_tracker_types);
	
	social_data *soc = GET_OLC_SOCIAL(ch->desc);
	olc_process_requirements(ch, argument, &SOC_REQUIREMENTS(soc), "requirements", FALSE);
}


OLC_MODULE(socedit_targetposition) {
	social_data *soc = GET_OLC_SOCIAL(ch->desc);
	SOC_MIN_VICT_POS(soc) = olc_process_type(ch, argument, "position", "targetposition", position_types, SOC_MIN_VICT_POS(soc));
}


OLC_MODULE(socedit_n2char) {
	process_soc_msg_field(ch, argument, SOCM_NO_ARG_TO_CHAR);
}


OLC_MODULE(socedit_n2other) {
	process_soc_msg_field(ch, argument, SOCM_NO_ARG_TO_OTHERS);
}


OLC_MODULE(socedit_s2char) {
	process_soc_msg_field(ch, argument, SOCM_SELF_TO_CHAR);
}


OLC_MODULE(socedit_s2other) {
	process_soc_msg_field(ch, argument, SOCM_SELF_TO_OTHERS);
}


OLC_MODULE(socedit_t2char) {
	process_soc_msg_field(ch, argument, SOCM_TARGETED_TO_CHAR);
}


OLC_MODULE(socedit_t2vict) {
	process_soc_msg_field(ch, argument, SOCM_TARGETED_TO_VICTIM);
}


OLC_MODULE(socedit_t2other) {
	process_soc_msg_field(ch, argument, SOCM_TARGETED_TO_OTHERS);
}


OLC_MODULE(socedit_tnotfound) {
	process_soc_msg_field(ch, argument, SOCM_TARGETED_NOT_FOUND);
}
