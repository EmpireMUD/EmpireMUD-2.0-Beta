/* ************************************************************************
*   File: faction.c                                       EmpireMUD 2.0b4 *
*  Usage: code related to factions, including DB and OLC                  *
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
#include "comm.h"
#include "interpreter.h"
#include "handler.h"
#include "db.h"
#include "skills.h"
#include "olc.h"

/**
* Contents:
*   Helpers
*   Utilities
*   Database
*   Reputation Handlers
*   OLC Handlers
*   Displays
*   Edit Modules
*/

// local data
const char *default_faction_name = "Unnamed Faction";

// local protos
void get_faction_relation_display(struct faction_relation *list, char *save_buffer);

// external consts
extern const char *faction_flags[];
extern const char *relationship_flags[];
extern struct faction_reputation_type reputation_levels[];

// external funcs


 /////////////////////////////////////////////////////////////////////////////
//// HELPERS ////////////////////////////////////////////////////////////////

/**
* Gets the name of a reputation by REP_ type.
*
* @param int type REP_ const
* @return const char* The name it uses.
*/
const char *get_reputation_name(int type) {
	const char *unknown = "UNKNOWN";
	int iter;
	
	for (iter = 0; *reputation_levels[iter].name != '\n'; ++iter) {
		if (reputation_levels[iter].type == type) {
			return reputation_levels[iter].name;
		}
	}
	
	// not found
	return unknown;
}


/**
* Finds a faction by ambiguous argument, which may be a vnum or a name.
* Names are matched by exact match first, or by multi-abbrev.
*
* @param char *argument The user input.
* @return faction_data* The faction, or NULL if it doesn't exist.
*/
faction_data *find_faction(char *argument) {
	faction_data *fct;
	any_vnum vnum;
	
	if (isdigit(*argument) && (vnum = atoi(argument)) >= 0 && (fct = find_faction_by_vnum(vnum))) {
		return fct;
	}
	else {
		return find_faction_by_name(argument);
	}
}


/**
* Look up a faction by multi-abbrev, preferring exact matches.
*
* @param char *name The faction name to look up.
* @return faction_data* The faction, or NULL if it doesn't exist.
*/
faction_data *find_faction_by_name(char *name) {
	faction_data *fct, *next_fct, *partial = NULL;
	
	if (!*name) {
		return NULL;	// shortcut
	}
	
	HASH_ITER(sorted_hh, sorted_factions, fct, next_fct) {
		// matches:
		if (!str_cmp(name, FCT_NAME(fct))) {
			// perfect match
			return fct;
		}
		if (!partial && is_multiword_abbrev(name, FCT_NAME(fct))) {
			// probable match
			partial = fct;
		}
	}
	
	// no exact match...
	return partial;
}


/**
* @param any_vnum vnum Any faction vnum
* @return faction_data* The faction, or NULL if it doesn't exist
*/
faction_data *find_faction_by_vnum(any_vnum vnum) {
	faction_data *fct;
	
	if (vnum < 0 || vnum == NOTHING) {
		return NULL;
	}
	
	HASH_FIND_INT(faction_table, &vnum, fct);
	return fct;
}


 //////////////////////////////////////////////////////////////////////////////
//// UTILITIES ///////////////////////////////////////////////////////////////

/**
* Checks for common faction problems and reports them to ch.
*
* @param faction_data *fct The item to audit.
* @param char_data *ch The person to report to.
* @return bool TRUE if any problems were reported; FALSE if all good.
*/
bool audit_faction(faction_data *fct, char_data *ch) {
	bool problem = FALSE;
	
	if (FACTION_FLAGGED(fct, FCT_IN_DEVELOPMENT)) {
		olc_audit_msg(ch, FCT_VNUM(fct), "IN-DEVELOPMENT");
		problem = TRUE;
	}
	if (!FCT_NAME(fct) || !*FCT_NAME(fct) || !str_cmp(FCT_NAME(fct), default_faction_name)) {
		olc_audit_msg(ch, FCT_VNUM(fct), "No name set");
		problem = TRUE;
	}
	
	return problem;
}


/**
* For the .list command.
*
* @param faction_data *fct The thing to list.
* @param bool detail If TRUE, provide additional details
* @return char* The line to show (without a CRLF).
*/
char *list_one_faction(faction_data *fct, bool detail) {
	static char output[MAX_STRING_LENGTH];
	
	if (detail) {
		snprintf(output, sizeof(output), "[%5d] %s", FCT_VNUM(fct), FCT_NAME(fct));
	}
	else {
		snprintf(output, sizeof(output), "[%5d] %s", FCT_VNUM(fct), FCT_NAME(fct));
	}
		
	return output;
}


/**
* Searches for all uses of an faction and displays them.
*
* @param char_data *ch The player.
* @param any_vnum vnum The faction vnum.
*/
void olc_search_faction(char_data *ch, any_vnum vnum) {
	char buf[MAX_STRING_LENGTH];
	faction_data *fct = find_faction_by_vnum(vnum);
	int size, found;
	
	if (!fct) {
		msg_to_char(ch, "There is no faction %d.\r\n", vnum);
		return;
	}
	
	found = 0;
	size = snprintf(buf, sizeof(buf), "Occurrences of faction %d (%s):\r\n", vnum, FCT_NAME(fct));
	
	// factions are not used anywhere else yet
	
	if (found > 0) {
		size += snprintf(buf + size, sizeof(buf) - size, "%d location%s shown\r\n", found, PLURAL(found));
	}
	else {
		size += snprintf(buf + size, sizeof(buf) - size, " none\r\n");
	}
	
	page_string(ch->desc, buf, TRUE);
}


// Simple vnum sorter for the faction hash
int sort_factions(faction_data *a, faction_data *b) {
	return FCT_VNUM(a) - FCT_VNUM(b);
}


// alphabetic sorter for sorted_factions
int sort_factions_by_data(faction_data *a, faction_data *b) {
	return strcmp(NULLSAFE(FCT_NAME(a)), NULLSAFE(FCT_NAME(b)));
}


 //////////////////////////////////////////////////////////////////////////////
//// DATABASE ////////////////////////////////////////////////////////////////

/**
* Puts a faction into the hash table.
*
* @param faction_data *fct The faction data to add to the table.
*/
void add_faction_to_table(faction_data *fct) {
	faction_data *find;
	any_vnum vnum;
	
	if (fct) {
		vnum = FCT_VNUM(fct);
		HASH_FIND_INT(faction_table, &vnum, find);
		if (!find) {
			HASH_ADD_INT(faction_table, vnum, fct);
			HASH_SORT(faction_table, sort_factions);
		}
		
		// sorted table
		HASH_FIND(sorted_hh, sorted_factions, &vnum, sizeof(int), find);
		if (!find) {
			HASH_ADD(sorted_hh, sorted_factions, vnum, sizeof(int), fct);
			HASH_SRT(sorted_hh, sorted_factions, sort_factions_by_data);
		}
	}
}


/**
* Removes a faction from the hash table.
*
* @param faction_data *fct The faction data to remove from the table.
*/
void remove_faction_from_table(faction_data *fct) {
	HASH_DEL(faction_table, fct);
	HASH_DELETE(sorted_hh, sorted_factions, fct);
}


/**
* Validates and links all the faction relationships on startup.
*/
void check_and_link_faction_relations(void) {
	struct faction_relation *rel, *next_rel;
	faction_data *fct, *next_fct;
	
	HASH_ITER(hh, faction_table, fct, next_fct) {
		LL_FOREACH_SAFE(FCT_RELATIONS(fct), rel, next_rel) {
			rel->ptr = find_faction_by_vnum(rel->vnum);
			
			if (!rel->ptr) {
				LL_DELETE(FCT_RELATIONS(fct), rel);
				free(rel);
			}
		}
	}
}


/**
* Initializes a new faction. This clears all memory for it, so set the vnum
* AFTER.
*
* @param faction_data *fct The faction to initialize.
*/
void clear_faction(faction_data *fct) {
	memset((char *) fct, 0, sizeof(faction_data));
	
	FCT_VNUM(fct) = NOTHING;
}


/**
* Duplicates a list of faction relationships, for editing.
*
* @param struct faction_relation *input The head of the list to copy.
* @return struct faction_relation* The copied list.
*/
struct faction_relation *copy_faction_relations(struct faction_relation *input) {
	struct faction_relation *el, *iter, *list = NULL;
	
	LL_FOREACH(input, iter) {
		CREATE(el, struct faction_relation, 1);
		*el = *iter;
		el->next = NULL;
		LL_APPEND(list, el);
	}
	
	return list;
}


/**
* @param struct faction_relation *list Frees the memory for this list.
*/
void free_faction_relations(struct faction_relation *list) {
	struct faction_relation *tmp, *next;
	
	LL_FOREACH_SAFE(list, tmp, next) {
		free(tmp);
	}
}


/**
* frees up memory for a faction data item.
*
* See also: olc_delete_faction
*
* @param faction_data *fct The faction data to free.
*/
void free_faction(faction_data *fct) {
	faction_data *proto = find_faction_by_vnum(FCT_VNUM(fct));
	
	if (FCT_NAME(fct) && (!proto || FCT_NAME(fct) != FCT_NAME(proto))) {
		free(FCT_NAME(fct));
	}
	if (FCT_DESCRIPTION(fct) && (!proto || FCT_DESCRIPTION(fct) != FCT_DESCRIPTION(proto))) {
		free(FCT_DESCRIPTION(fct));
	}
	
	if (FCT_RELATIONS(fct) && (!proto || FCT_RELATIONS(fct) != FCT_RELATIONS(proto))) {
		free_faction_relations(FCT_RELATIONS(fct));
	}
	
	free(fct);
}


/**
* Read one faction from file.
*
* @param FILE *fl The open .fct file
* @param any_vnum vnum The faction vnum
*/
void parse_faction(FILE *fl, any_vnum vnum) {
	char line[256], error[256], str_in[256];
	struct faction_relation *rel;
	faction_data *fct, *find;
	int int_in[4];
	
	CREATE(fct, faction_data, 1);
	clear_faction(fct);
	FCT_VNUM(fct) = vnum;
	
	HASH_FIND_INT(faction_table, &vnum, find);
	if (find) {
		log("WARNING: Duplicate faction vnum #%d", vnum);
		// but have to load it anyway to advance the file
	}
	add_faction_to_table(fct);
		
	// for error messages
	sprintf(error, "faction vnum %d", vnum);
	
	// lines 1-2: name, desc
	FCT_NAME(fct) = fread_string(fl, error);
	FCT_DESCRIPTION(fct) = fread_string(fl, error);
	
	// line 3: flags
	if (!get_line(fl, line) || sscanf(line, "%s", str_in) != 1) {
		log("SYSERR: Format error in line 3 of %s", error);
		exit(1);
	}
	
	FCT_FLAGS(fct) = asciiflag_conv(str_in);
	
	// line 4: rep levels
	if (!get_line(fl, line) || sscanf(line, "%d %d %d", &int_in[0], &int_in[1], &int_in[2]) != 3) {
		log("SYSERR: Format error in line 4 of %s", error);
		exit(1);
	}
	
	FCT_MIN_REP(fct) = int_in[0];
	FCT_MAX_REP(fct) = int_in[1];
	FCT_STARTING_REP(fct) = int_in[2];
	
	// optionals
	for (;;) {
		if (!get_line(fl, line)) {
			log("SYSERR: Format error in %s, expecting alphabetic flags", error);
			exit(1);
		}
		switch (*line) {
			case 'R': {	// relations
				if (sscanf(line, "R %d %s", &int_in[0], str_in) != 2) {
					log("SYSERR: Format error in line R of %s", error);
					exit(1);
				}
				
				CREATE(rel, struct faction_relation, 1);
				rel->vnum = int_in[0];
				rel->flags = asciiflag_conv(str_in);
				LL_APPEND(FCT_RELATIONS(fct), rel);
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


// writes entries in the faction index
void write_faction_index(FILE *fl) {
	faction_data *fct, *next_fct;
	int this, last;
	
	last = NO_WEAR;
	HASH_ITER(hh, faction_table, fct, next_fct) {
		// determine "zone number" by vnum
		this = (int)(FCT_VNUM(fct) / 100);
	
		if (this != last) {
			fprintf(fl, "%d%s\n", this, FCT_SUFFIX);
			last = this;
		}
	}
}


/**
* Outputs one faction in the db file format, starting with a #VNUM and
* ending with an S.
*
* @param FILE *fl The file to write it to.
* @param faction_data *fct The thing to save.
*/
void write_faction_to_file(FILE *fl, faction_data *fct) {
	struct faction_relation *rel;
	char temp[MAX_STRING_LENGTH];
	
	if (!fl || !fct) {
		syslog(SYS_ERROR, LVL_START_IMM, TRUE, "SYSERR: write_faction_to_file called without %s", !fl ? "file" : "faction");
		return;
	}
	
	fprintf(fl, "#%d\n", FCT_VNUM(fct));
	
	// 1. name
	fprintf(fl, "%s~\n", NULLSAFE(FCT_NAME(fct)));
	
	// 2. desc
	strcpy(temp, NULLSAFE(FCT_DESCRIPTION(fct)));
	strip_crlf(temp);
	fprintf(fl, "%s~\n", temp);
	
	// 3. flags
	fprintf(fl, "%s\n", bitv_to_alpha(FCT_FLAGS(fct)));
	
	// 4. rep
	fprintf(fl, "%d %d %d\n", FCT_MIN_REP(fct), FCT_MAX_REP(fct), FCT_STARTING_REP(fct));
	
	// 'R': relations
	LL_FOREACH(FCT_RELATIONS(fct), rel) {
		fprintf(fl, "R %d %s\n", rel->vnum, bitv_to_alpha(rel->flags));
	}
	
	// end
	fprintf(fl, "S\n");
}


 //////////////////////////////////////////////////////////////////////////////
//// REPUTATION HANDLERS /////////////////////////////////////////////////////

/**
* Comparison function to determine if one REP_ const is higher than another.
* This does NOT account for 'magnitude', where a negative rank is "higher"
* than another negative rank if its absolute value is larger.
*/
int compare_reptuation(int rep_a, int rep_b) {
	int ind_a, ind_b;
	
	ind_a = rep_const_to_index(rep_a);
	ind_b = rep_const_to_index(rep_b);
	
	if (rep_a < rep_b) {
		return -1;
	}
	else if (rep_a > rep_b) {
		return 1;
	}
	else {
		return 0;
	}
}


/**
* Finds a reputation const by name.
*
* @param char *name The name to find.
* @return int Any REP_ const, or NOTHING if not found.
*/
int get_reputation_by_name(char *name) {
	int iter, partial = NOTHING;
	
	for (iter = 0; *reputation_levels[iter].name != '\n'; ++iter) {
		if (!str_cmp(name, reputation_levels[iter].name)) {
			return reputation_levels[iter].type;
		}
		else if (is_abbrev(name, reputation_levels[iter].name)) {
			partial = iter;
		}
	}
	
	return partial != NOTHING ? reputation_levels[partial].type : NOTHING;
}


/**
* Detect the min/max values for reputation at startup.
*/
void init_reputation(void) {
	int iter;
	
	for (iter = 0; *reputation_levels[iter].name != '\n'; ++iter) {
		MAX_REPUTATION = MAX(MAX_REPUTATION, reputation_levels[iter].value);
		MIN_REPUTATION = MIN(MIN_REPUTATION, reputation_levels[iter].value);
	}
}


/**
* Converts a REP_ const into a reputation_levels[] index.
*
* @param int rep_const Any REP_ type.
* @return int Its location in the reputation_levels array, or NOTHING if not found.
*/
int rep_const_to_index(int rep_const) {
	int iter;
	
	for (iter = 0; *reputation_levels[iter].name != '\n'; ++iter) {
		if (reputation_levels[iter].type == rep_const) {
			return iter;
		}
	}
	
	return NOTHING;	// not found
}


 //////////////////////////////////////////////////////////////////////////////
//// OLC HANDLERS ////////////////////////////////////////////////////////////

/**
* Creates a new faction entry.
* 
* @param any_vnum vnum The number to create.
* @return faction_data* The new faction's prototype.
*/
faction_data *create_faction_table_entry(any_vnum vnum) {
	faction_data *fct;
	
	// sanity
	if (find_faction_by_vnum(vnum)) {
		log("SYSERR: Attempting to insert faction at existing vnum %d", vnum);
		return find_faction_by_vnum(vnum);
	}
	
	CREATE(fct, faction_data, 1);
	clear_faction(fct);
	FCT_VNUM(fct) = vnum;
	FCT_NAME(fct) = str_dup(default_faction_name);
	add_faction_to_table(fct);

	// save index and faction file now
	save_index(DB_BOOT_FCT);
	save_library_file_for_vnum(DB_BOOT_FCT, vnum);

	return fct;
}


/**
* WARNING: This function actually deletes a faction.
*
* @param char_data *ch The person doing the deleting.
* @param any_vnum vnum The vnum to delete.
*/
void olc_delete_faction(char_data *ch, any_vnum vnum) {
	faction_data *fct, *iter, *next_iter;
	
	if (!(fct = find_faction_by_vnum(vnum))) {
		msg_to_char(ch, "There is no such faction %d.\r\n", vnum);
		return;
	}
	
	// remove it from the hash table first
	remove_faction_from_table(fct);
	
	// remove from other factions
	
	// remove from live players
	/*
	LL_FOREACH(character_list, chiter) {
		if (IS_NPC(chiter)) {
			continue;
		}
		if (GET_CLASS(chiter) != fct) {
			continue;
		}
		update_class(chiter);
	}
	*/
	
	// remove from mobs, quests

	// save index and faction file now
	save_index(DB_BOOT_FCT);
	save_library_file_for_vnum(DB_BOOT_FCT, vnum);
	
	syslog(SYS_OLC, GET_INVIS_LEV(ch), TRUE, "OLC: %s has deleted faction %d", GET_NAME(ch), vnum);
	msg_to_char(ch, "Faction %d deleted.\r\n", vnum);
	
	free_faction(fct);
}


/**
* Function to save a player's changes to a faction (or a new one).
*
* @param descriptor_data *desc The descriptor who is saving.
*/
void save_olc_faction(descriptor_data *desc) {	
	faction_data *proto, *fct = GET_OLC_FACTION(desc);
	any_vnum vnum = GET_OLC_VNUM(desc);
	UT_hash_handle hh, sorted;

	// have a place to save it?
	if (!(proto = find_faction_by_vnum(vnum))) {
		proto = create_faction_table_entry(vnum);
	}
	
	// free prototype strings and pointers
	if (FCT_NAME(proto)) {
		free(FCT_NAME(proto));
	}
	free_faction_relations(FCT_RELATIONS(proto));
	
	// sanity
	if (!FCT_NAME(fct) || !*FCT_NAME(fct)) {
		if (FCT_NAME(fct)) {
			free(FCT_NAME(fct));
		}
		FCT_NAME(fct) = str_dup(default_faction_name);
	}

	// save data back over the proto-type
	hh = proto->hh;	// save old hash handle
	sorted = proto->sorted_hh;
	*proto = *fct;	// copy over all data
	proto->vnum = vnum;	// ensure correct vnum
	proto->hh = hh;	// restore old hash handle
	proto->sorted_hh = sorted;
		
	// and save to file
	save_library_file_for_vnum(DB_BOOT_FCT, vnum);

	// ... and re-sort
	HASH_SRT(sorted_hh, sorted_factions, sort_factions_by_data);
}


/**
* Creates a copy of a faction, or clears a new one, for editing.
* 
* @param faction_data *input The faction to copy, or NULL to make a new one.
* @return faction_data* The copied faction.
*/
faction_data *setup_olc_faction(faction_data *input) {
	faction_data *new;
	
	CREATE(new, faction_data, 1);
	clear_faction(new);
	
	if (input) {
		// copy normal data
		*new = *input;

		// copy things that are pointers
		FCT_NAME(new) = FCT_NAME(input) ? str_dup(FCT_NAME(input)) : NULL;
		
		// copy lists
		FCT_RELATIONS(new) = copy_faction_relations(FCT_RELATIONS(input));
	}
	else {
		// brand new: some defaults
		FCT_NAME(new) = str_dup(default_faction_name);
		FCT_FLAGS(new) = FCT_IN_DEVELOPMENT;
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
* @param faction_data *fct The faction to display.
*/
void do_stat_faction(char_data *ch, faction_data *fct) {
	char buf[MAX_STRING_LENGTH], part[MAX_STRING_LENGTH];
	size_t size;
	
	if (!fct) {
		return;
	}
	
	// first line
	size = snprintf(buf, sizeof(buf), "VNum: [\tc%d\t0], Name: \tc%s\t0\r\n%s", FCT_VNUM(fct), FCT_NAME(fct), NULLSAFE(FCT_DESCRIPTION(fct)));
	size += snprintf(buf + size, sizeof(buf) - size, "Min Reputation: \ty%s\t0, Max Reputation: \ty%s\t0, Starting Reputation: \ty%s\t0\r\n", get_reputation_name(FCT_MIN_REP(fct)), get_reputation_name(FCT_MAX_REP(fct)), get_reputation_name(FCT_STARTING_REP(fct)));
	
	sprintbit(FCT_FLAGS(fct), faction_flags, part, TRUE);
	size += snprintf(buf + size, sizeof(buf) - size, "Flags: \tg%s\t0\r\n", part);
	
	get_faction_relation_display(FCT_RELATIONS(fct), part);
	size += snprintf(buf + size, sizeof(buf) - size, "Relations:\r\n%s", part);
	
	page_string(ch->desc, buf, TRUE);
}


/**
* Gets the faction relation display for olc, stat, or other uses.
*
* @param struct faction_relation *list The list of relations to display.
* @param char *save_buffer A buffer to store the display to.
*/
void get_faction_relation_display(struct faction_relation *list, char *save_buffer) {
	struct faction_relation *iter;
	char lbuf[MAX_STRING_LENGTH];
	int count = 0;
	
	*save_buffer = '\0';
	
	LL_FOREACH(list, iter) {
		sprintbit(iter->flags, relationship_flags, lbuf, TRUE);
		sprintf(save_buffer + strlen(save_buffer), "%2d. [%5d] %s - %s\r\n", ++count, iter->vnum, FCT_NAME(iter->ptr), lbuf);
	}
}


/**
* This is the main recipe display for faction OLC. It displays the user's
* currently-edited faction.
*
* @param char_data *ch The person who is editing a faction and will see its display.
*/
void olc_show_faction(char_data *ch) {
	faction_data *fct = GET_OLC_FACTION(ch->desc);
	char buf[MAX_STRING_LENGTH], lbuf[MAX_STRING_LENGTH];
	
	if (!fct) {
		return;
	}
	
	*buf = '\0';
	
	sprintf(buf + strlen(buf), "[\tc%d\t0] \tc%s\t0\r\n", GET_OLC_VNUM(ch->desc), !find_faction_by_vnum(FCT_VNUM(fct)) ? "new faction" : FCT_NAME(find_faction_by_vnum(FCT_VNUM(fct))));
	sprintf(buf + strlen(buf), "<\tyname\t0> %s\r\n", NULLSAFE(FCT_NAME(fct)));
	sprintf(buf + strlen(buf), "<\tydescription\t0>\r\n%s", NULLSAFE(FCT_DESCRIPTION(fct)));
	
	sprintbit(FCT_FLAGS(fct), faction_flags, lbuf, TRUE);
	sprintf(buf + strlen(buf), "<\tyflags\t0> %s\r\n", lbuf);
	
	sprintf(buf + strlen(buf), "<\tyminreptuation\t0> %s\r\n", get_reputation_name(FCT_MIN_REP(fct)));
	sprintf(buf + strlen(buf), "<\tymaxreputation\t0> %s\r\n", get_reputation_name(FCT_MAX_REP(fct)));
	sprintf(buf + strlen(buf), "<\tystartingreuptation\t0> %s\r\n", get_reputation_name(FCT_STARTING_REP(fct)));
	
	get_faction_relation_display(FCT_RELATIONS(fct), lbuf);
	sprintf(buf + strlen(buf), "Relationships: <\tyrelation\t0>\r\n%s", FCT_RELATIONS(fct) ? lbuf : "");
	
	page_string(ch->desc, buf, TRUE);
}


/**
* Searches the faction db for a match, and prints it to the character.
*
* @param char *searchname The search string.
* @param char_data *ch The player who is searching.
* @return int The number of matches shown.
*/
int vnum_faction(char *searchname, char_data *ch) {
	faction_data *iter, *next_iter;
	int found = 0;
	
	HASH_ITER(hh, faction_table, iter, next_iter) {
		if (multi_isname(searchname, FCT_NAME(iter))) {
			msg_to_char(ch, "%3d. [%5d] %s\r\n", ++found, FCT_VNUM(iter), FCT_NAME(iter));
		}
	}
	
	return found;
}


 //////////////////////////////////////////////////////////////////////////////
//// OLC MODULES /////////////////////////////////////////////////////////////

OLC_MODULE(fedit_description) {
	faction_data *fct = GET_OLC_FACTION(ch->desc);
	
	if (ch->desc->str) {
		msg_to_char(ch, "You are already editing a string.\r\n");
	}
	else {
		sprintf(buf, "description for %s", FCT_NAME(fct));
		start_string_editor(ch->desc, buf, &FCT_NAME(fct), MAX_FACTION_DESCRIPTION);
	}
}


OLC_MODULE(fedit_flags) {
	faction_data *fct = GET_OLC_FACTION(ch->desc);
	bool had_indev = IS_SET(FCT_FLAGS(fct), FCT_IN_DEVELOPMENT) ? TRUE : FALSE;
	
	FCT_FLAGS(fct) = olc_process_flag(ch, argument, "faction", "flags", faction_flags, FCT_FLAGS(fct));
	
	// validate removal of IN-DEVELOPMENT
	if (had_indev && !IS_SET(FCT_FLAGS(fct), FCT_IN_DEVELOPMENT) && GET_ACCESS_LEVEL(ch) < LVL_UNRESTRICTED_BUILDER && !OLC_FLAGGED(ch, OLC_FLAG_CLEAR_IN_DEV)) {
		msg_to_char(ch, "You don't have permission to remove the IN-DEVELOPMENT flag.\r\n");
		SET_BIT(FCT_FLAGS(fct), FCT_IN_DEVELOPMENT);
	}
}


OLC_MODULE(fedit_maxreputation) {
	faction_data *fct = GET_OLC_FACTION(ch->desc);
	int rep, idx;
	
	if ((rep = get_reputation_by_name(argument)) == NOTHING || (idx = rep_const_to_index(rep)) == NOTHING) {
		msg_to_char(ch, "Unknown reputation level '%s'.\r\n", argument);
		return;
	}
	
	FCT_MAX_REP(fct) = rep;
	if (PRF_FLAGGED(ch, PRF_NOREPEAT)) {
		send_config_msg(ch, "ok_string");
	}
	else {
		msg_to_char(ch, "You set the maximum reputation to %s.\r\n", reputation_levels[idx].name);
	}
}


OLC_MODULE(fedit_minreputation) {
	faction_data *fct = GET_OLC_FACTION(ch->desc);
	int rep, idx;
	
	if ((rep = get_reputation_by_name(argument)) == NOTHING || (idx = rep_const_to_index(rep)) == NOTHING) {
		msg_to_char(ch, "Unknown reputation level '%s'.\r\n", argument);
		return;
	}
	
	FCT_MIN_REP(fct) = rep;
	if (PRF_FLAGGED(ch, PRF_NOREPEAT)) {
		send_config_msg(ch, "ok_string");
	}
	else {
		msg_to_char(ch, "You set the minimum reputation to %s.\r\n", reputation_levels[idx].name);
	}
}


OLC_MODULE(fedit_name) {
	faction_data *fct = GET_OLC_FACTION(ch->desc);
	olc_process_string(ch, argument, "name", &FCT_NAME(fct));
}


OLC_MODULE(fedit_relation) {
	faction_data *fct = GET_OLC_FACTION(ch->desc);
	// TODO
}


OLC_MODULE(fedit_startingreputation) {
	faction_data *fct = GET_OLC_FACTION(ch->desc);
	int rep, idx;
	
	if ((rep = get_reputation_by_name(argument)) == NOTHING || (idx = rep_const_to_index(rep)) == NOTHING) {
		msg_to_char(ch, "Unknown reputation level '%s'.\r\n", argument);
		return;
	}
	
	FCT_STARTING_REP(fct) = rep;
	if (PRF_FLAGGED(ch, PRF_NOREPEAT)) {
		send_config_msg(ch, "ok_string");
	}
	else {
		msg_to_char(ch, "You set the starting reputation to %s.\r\n", reputation_levels[idx].name);
	}
}
