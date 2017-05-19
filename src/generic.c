/* ************************************************************************
*   File: generic.c                                       EmpireMUD 2.0b5 *
*  Usage: loading, saving, OLC, and functions for generics                *
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
*   Generic OLC Modules
*   Liquid OLC Modules
*/

// local data
const char *default_generic_name = "Unnamed Generic";
#define MAX_LIQUID_COND  (MAX_CONDITION / 15)	// approximate game hours of max cond

// external consts
extern const char *generic_flags[];
extern const char *generic_types[];

// external funcs


 //////////////////////////////////////////////////////////////////////////////
//// HELPERS /////////////////////////////////////////////////////////////////

/**
* Quick, safe lookup for a generic string. This checks that it's the expected
* type, and will return UNKNOWN if anything is out of place.
*
* @param any_vnum vnum The generic vnum to look up.
* @param int type Any GENERIC_ const that must match the vnum's type.
* @param int pos Which string position to fetch.
* @return const char* The string from the generic.
*/
const char *get_generic_string_by_vnum(any_vnum vnum, int type, int pos) {
	generic_data *gen = find_generic_by_vnum(vnum);
	
	if (!gen || GEN_TYPE(gen) != type || pos < 0 || pos >= NUM_GENERIC_STRINGS) {
		return "UNKNOWN";	// sanity
	}
	else if (!GEN_STRING(gen, pos)) {
		return "(null)";
	}
	else {
		return GEN_STRING(gen, pos);
	}
}


/**
* Quick, safe lookup for a generic value. This checks that it's the expected
* type, and will return 0 if anything is out of place.
*
* @param any_vnum vnum The generic vnum to look up.
* @param int type Any GENERIC_ const that must match the vnum's type.
* @param int pos Which value position to fetch.
* @return int The value from the generic.
*/
int get_generic_value_by_vnum(any_vnum vnum, int type, int pos) {
	generic_data *gen = find_generic_by_vnum(vnum);
	
	if (!gen || GEN_TYPE(gen) != type || pos < 0 || pos >= NUM_GENERIC_VALUES) {
		return 0;	// sanity
	}
	else {
		return GEN_VALUE(gen, pos);
	}
}


 //////////////////////////////////////////////////////////////////////////////
//// UTILITIES ///////////////////////////////////////////////////////////////

/**
* Checks for common generics problems and reports them to ch.
*
* @param generic_data *gen The item to audit.
* @param char_data *ch The person to report to.
* @return bool TRUE if any problems were reported; FALSE if all good.
*/
bool audit_generic(generic_data *gen, char_data *ch) {
	bool problem = FALSE;
	
	if (!GEN_NAME(gen) || !*GEN_NAME(gen) || !str_cmp(GEN_NAME(gen), default_generic_name)) {
		olc_audit_msg(ch, GEN_VNUM(gen), "No name set");
		problem = TRUE;
	}
	
	return problem;
}


/**
* For the .list command.
*
* @param generic_data *gen The thing to list.
* @param bool detail If TRUE, provide additional details
* @return char* The line to show (without a CRLF).
*/
char *list_one_generic(generic_data *gen, bool detail) {
	static char output[MAX_STRING_LENGTH];
	
	if (detail) {
		
		snprintf(output, sizeof(output), "[%5d] %s (%s)", GEN_VNUM(gen), GEN_NAME(gen), generic_types[GEN_TYPE(gen)]);
	}
	else {
		snprintf(output, sizeof(output), "[%5d] %s (%s)", GEN_VNUM(gen), GEN_NAME(gen), generic_types[GEN_TYPE(gen)]);
	}
		
	return output;
}


/**
* Searches for all uses of a generic and displays them.
*
* @param char_data *ch The player.
* @param any_vnum vnum The generic vnum.
*/
void olc_search_generic(char_data *ch, any_vnum vnum) {
	char buf[MAX_STRING_LENGTH];
	generic_data *gen = find_generic_by_vnum(vnum);
	int size, found;
	
	if (!gen) {
		msg_to_char(ch, "There is no generic %d.\r\n", vnum);
		return;
	}
	
	found = 0;
	size = snprintf(buf, sizeof(buf), "Occurrences of generic %d (%s):\r\n", vnum, GEN_NAME(gen));
	
	// generics are not actually used anywhere else
	
	if (found > 0) {
		size += snprintf(buf + size, sizeof(buf) - size, "%d location%s shown\r\n", found, PLURAL(found));
	}
	else {
		size += snprintf(buf + size, sizeof(buf) - size, " none\r\n");
	}
	
	page_string(ch->desc, buf, TRUE);
}


// Simple vnum sorter for the generics hash
int sort_generics(generic_data *a, generic_data *b) {
	return GEN_VNUM(a) - GEN_VNUM(b);
}


 //////////////////////////////////////////////////////////////////////////////
//// DATABASE ////////////////////////////////////////////////////////////////

/**
* Puts a generic into the hash table.
*
* @param generic_data *gen The generic data to add to the table.
*/
void add_generic_to_table(generic_data *gen) {
	generic_data *find;
	any_vnum vnum;
	
	if (gen) {
		vnum = GEN_VNUM(gen);
		HASH_FIND_INT(generic_table, &vnum, find);
		if (!find) {
			HASH_ADD_INT(generic_table, vnum, gen);
			HASH_SORT(generic_table, sort_generics);
		}
	}
}


/**
* Removes a generic from the hash table.
*
* @param generic_data *gen The generic data to remove from the table.
*/
void remove_generic_from_table(generic_data *gen) {
	HASH_DEL(generic_table, gen);
}


/**
* Initializes a new generic. This clears all memory for it, so set the vnum
* AFTER.
*
* @param generic_data *gen The generic to initialize.
*/
void clear_generic(generic_data *gen) {
	memset((char *) gen, 0, sizeof(generic_data));
	
	GEN_VNUM(gen) = NOTHING;
}


/**
* frees up memory for a generic data item.
*
* See also: olc_delete_generic
*
* @param generic_data *gen The generic data to free.
*/
void free_generic(generic_data *gen) {
	generic_data *proto = find_generic_by_vnum(GEN_VNUM(gen));
	int iter;
	
	if (GEN_NAME(gen) && (!proto || GEN_NAME(gen) != GEN_NAME(proto))) {
		free(GEN_NAME(gen));
	}
	
	for (iter = 0; iter < NUM_GENERIC_STRINGS; ++iter) {
		if (GEN_STRING(gen, iter) && (!proto || GEN_STRING(gen, iter) != GEN_STRING(proto, iter))) {
			free(GEN_STRING(gen, iter));
		}
	}
	
	free(gen);
}


/**
* Read one generic from file.
*
* @param FILE *fl The open .gen file
* @param any_vnum vnum The generic vnum
*/
void parse_generic(FILE *fl, any_vnum vnum) {
	void parse_requirement(FILE *fl, struct req_data **list, char *error_str);
	
	char line[256], error[256], str_in[256], *ptr;
	generic_data *gen, *find;
	int int_in[4];
	
	CREATE(gen, generic_data, 1);
	clear_generic(gen);
	GEN_VNUM(gen) = vnum;
	
	HASH_FIND_INT(generic_table, &vnum, find);
	if (find) {
		log("WARNING: Duplicate generic vnum #%d", vnum);
		// but have to load it anyway to advance the file
	}
	add_generic_to_table(gen);
		
	// for error messages
	sprintf(error, "generic vnum %d", vnum);
	
	// line 1: name
	GEN_NAME(gen) = fread_string(fl, error);
	
	// line 2: type flags
	if (!get_line(fl, line) || sscanf(line, "%d %s", &int_in[0], str_in) != 2) {
		log("SYSERR: Format error in line 2 of %s", error);
		exit(1);
	}
	
	GEN_TYPE(gen) = int_in[0];
	GEN_FLAGS(gen) = asciiflag_conv(str_in);
	
	// line 3: values
	if (!get_line(fl, line) || sscanf(line, "%d %d %d %d", &int_in[0], &int_in[1], &int_in[2], &int_in[3]) != 4) {
		log("SYSERR: Format error in line 3 of %s", error);
		exit(1);
	}
	
	GEN_VALUE(gen, 0) = int_in[0];
	GEN_VALUE(gen, 1) = int_in[1];
	GEN_VALUE(gen, 2) = int_in[2];
	GEN_VALUE(gen, 3) = int_in[3];
	
	// end
	fprintf(fl, "S\n");

	
	// optionals
	for (;;) {
		if (!get_line(fl, line)) {
			log("SYSERR: Format error in %s, expecting alphabetic flags", error);
			exit(1);
		}
		switch (*line) {
			case 'T': {	// string
				int_in[0] = atoi(line+1);
				ptr = fread_string(fl, error);
				
				if (int_in[0] >= 0 && int_in[0] < NUM_GENERIC_STRINGS) {
					GEN_STRING(gen, int_in[0]) = ptr;
				}
				else {
					log(" - error in %s: invalid string pos T%d", error, int_in[0]);
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
* @param any_vnum vnum Any generic vnum
* @return generic_data* The generic, or NULL if it doesn't exist
*/
generic_data *find_generic_by_vnum(any_vnum vnum) {
	generic_data *gen;
	
	if (vnum < 0 || vnum == NOTHING) {
		return NULL;
	}
	
	HASH_FIND_INT(generic_table, &vnum, gen);
	return gen;
}


// writes entries in the generic index
void write_generic_index(FILE *fl) {
	generic_data *gen, *next_gen;
	int this, last;
	
	last = -1;
	HASH_ITER(hh, generic_table, gen, next_gen) {
		// determine "zone number" by vnum
		this = (int)(GEN_VNUM(gen) / 100);
	
		if (this != last) {
			fprintf(fl, "%d%s\n", this, GEN_SUFFIX);
			last = this;
		}
	}
}


/**
* Outputs one generic item in the db file format, starting with a #VNUM and
* ending with an S.
*
* @param FILE *fl The file to write it to.
* @param generic_data *gen The thing to save.
*/
void write_generic_to_file(FILE *fl, generic_data *gen) {
	char temp[256];
	int iter;
	
	if (!fl || !gen) {
		syslog(SYS_ERROR, LVL_START_IMM, TRUE, "SYSERR: write_generic_to_file called without %s", !fl ? "file" : "generic");
		return;
	}
	
	fprintf(fl, "#%d\n", GEN_VNUM(gen));
	
	// 1. name
	fprintf(fl, "%s~\n", NULLSAFE(GEN_NAME(gen)));
	
	// 2. type flags
	strcpy(temp, bitv_to_alpha(GEN_FLAGS(gen)));
	fprintf(fl, "%d %s\n", GEN_TYPE(gen), temp);
	
	// 3. values -- need to adjust this if NUM_GENERIC_VALUES changes
	fprintf(fl, "%d %d %d %d\n", GEN_VALUE(gen, 0), GEN_VALUE(gen, 1), GEN_VALUE(gen, 2), GEN_VALUE(gen, 3));
	
	// 'T' strings
	for (iter = 0; iter < NUM_GENERIC_STRINGS; ++iter) {
		if (GEN_STRING(gen, iter) && *GEN_STRING(gen, iter)) {
			fprintf(fl, "T%d\n%s~\n", iter, GEN_STRING(gen, iter));
		}
	}
	
	// end
	fprintf(fl, "S\n");
}


 //////////////////////////////////////////////////////////////////////////////
//// OLC HANDLERS ////////////////////////////////////////////////////////////

/**
* Creates a new generic entry.
* 
* @param any_vnum vnum The number to create.
* @return generic_data* The new generic's prototype.
*/
generic_data *create_generic_table_entry(any_vnum vnum) {
	generic_data *gen;
	
	// sanity
	if (find_generic_by_vnum(vnum)) {
		log("SYSERR: Attempting to insert generic at existing vnum %d", vnum);
		return find_generic_by_vnum(vnum);
	}
	
	CREATE(gen, generic_data, 1);
	clear_generic(gen);
	GEN_VNUM(gen) = vnum;
	GEN_NAME(gen) = str_dup(default_generic_name);
	add_generic_to_table(gen);

	// save index and generic file now
	save_index(DB_BOOT_GEN);
	save_library_file_for_vnum(DB_BOOT_GEN, vnum);

	return gen;
}


/**
* WARNING: This function actually deletes a generic.
*
* @param char_data *ch The person doing the deleting.
* @param any_vnum vnum The vnum to delete.
*/
void olc_delete_generic(char_data *ch, any_vnum vnum) {
	generic_data *gen;
	
	if (!(gen = find_generic_by_vnum(vnum))) {
		msg_to_char(ch, "There is no such generic %d.\r\n", vnum);
		return;
	}
	
	// remove it from the hash table first
	remove_generic_from_table(gen);

	// save index and generic file now
	save_index(DB_BOOT_GEN);
	save_library_file_for_vnum(DB_BOOT_GEN, vnum);
	
	syslog(SYS_OLC, GET_INVIS_LEV(ch), TRUE, "OLC: %s has deleted generic %d", GET_NAME(ch), vnum);
	msg_to_char(ch, "Generic %d deleted.\r\n", vnum);
	
	free_generic(gen);
}


/**
* Function to save a player's changes to a generic (or a new one).
*
* @param descriptor_data *desc The descriptor who is saving.
*/
void save_olc_generic(descriptor_data *desc) {	
	generic_data *proto, *gen = GET_OLC_GENERIC(desc);
	any_vnum vnum = GET_OLC_VNUM(desc);
	UT_hash_handle hh;
	int iter;

	// have a place to save it?
	if (!(proto = find_generic_by_vnum(vnum))) {
		proto = create_generic_table_entry(vnum);
	}
	
	// free prototype strings and pointers
	if (GEN_NAME(proto)) {
		free(GEN_NAME(proto));
	}
	for (iter = 0; iter < NUM_GENERIC_STRINGS; ++iter) {
		if (GEN_STRING(proto, iter)) {
			free(GEN_STRING(proto, iter));
		}
	}
	
	// sanity
	if (!GEN_NAME(gen) || !*GEN_NAME(gen)) {
		if (GEN_NAME(gen)) {
			free(GEN_NAME(gen));
		}
		GEN_NAME(gen) = str_dup(default_generic_name);
	}
	for (iter = 0; iter < NUM_GENERIC_STRINGS; ++iter) {
		if (GEN_STRING(gen, iter) && !*GEN_STRING(gen, iter)) {
			free(GEN_STRING(gen, iter));
			GEN_STRING(gen, iter) = NULL;
		}
	}
	
	// save data back over the proto-type
	hh = proto->hh;	// save old hash handle
	*proto = *gen;	// copy over all data
	proto->vnum = vnum;	// ensure correct vnum
	proto->hh = hh;	// restore old hash handle
		
	// and save to file
	save_library_file_for_vnum(DB_BOOT_GEN, vnum);
}


/**
* Creates a copy of a generic, or clears a new one, for editing.
* 
* @param generic_data *input The generic to copy, or NULL to make a new one.
* @return generic_data* The copied generic.
*/
generic_data *setup_olc_generic(generic_data *input) {
	extern struct req_data *copy_requirements(struct req_data *from);
	
	generic_data *new;
	int iter;
	
	CREATE(new, generic_data, 1);
	clear_generic(new);
	
	if (input) {
		// copy normal data
		*new = *input;
		
		// copy things that are pointers
		GEN_NAME(new) = GEN_NAME(input) ? str_dup(GEN_NAME(input)) : NULL;
		for (iter = 0; iter < NUM_GENERIC_STRINGS; ++iter) {
			GEN_STRING(new, iter) = GEN_STRING(input, iter) ? str_dup(GEN_STRING(input, iter)) : NULL;
		}
	}
	else {
		// brand new: some defaults
		GEN_NAME(new) = str_dup(default_generic_name);
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
* @param generic_data *gen The generic to display.
*/
void do_stat_generic(char_data *ch, generic_data *gen) {
	char buf[MAX_STRING_LENGTH], part[MAX_STRING_LENGTH];
	size_t size;
	
	if (!gen) {
		return;
	}
	
	// first line
	size = snprintf(buf, sizeof(buf), "VNum: [\tc%d\t0], Name: \ty%s\t0, Type: \tc%s\t0\r\n", GEN_VNUM(gen), GEN_NAME(gen), generic_types[GEN_TYPE(gen)]);
	
	sprintbit(GEN_FLAGS(gen), generic_flags, part, TRUE);
	size += snprintf(buf + size, sizeof(buf) - size, "Flags: \tg%s\t0\r\n", part);
	
	// GENERIC_x
	switch (GEN_TYPE(gen)) {
		case GENERIC_LIQUID: {
			size += snprintf(buf + size, sizeof(buf) - size, "Liquid: \ty%s\t0, Color: \ty%s\t0\r\n", NULLSAFE(GET_LIQUID_NAME(gen)), NULLSAFE(GET_LIQUID_COLOR(gen)));
			size += snprintf(buf + size, sizeof(buf) - size, "Hunger: [\tc%d\t0], Thirst: [\tc%d\t0], Drunk: [\tc%d\t0]\r\n", GET_LIQUID_FULL(gen), GET_LIQUID_THIRST(gen), GET_LIQUID_DRUNK(gen));
			break;
		}
		case GENERIC_CURRENCY: {
			break;
		}
	}


	page_string(ch->desc, buf, TRUE);
}


/**
* This is the main recipe display for generic OLC. It displays the user's
* currently-edited generic.
*
* @param char_data *ch The person who is editing a generic and will see its display.
*/
void olc_show_generic(char_data *ch) {
	generic_data *gen = GET_OLC_GENERIC(ch->desc);
	char buf[MAX_STRING_LENGTH], lbuf[MAX_STRING_LENGTH];
	
	if (!gen) {
		return;
	}
	
	*buf = '\0';
	
	sprintf(buf + strlen(buf), "[\tc%d\t0] \tc%s\t0\r\n", GET_OLC_VNUM(ch->desc), !find_generic_by_vnum(GEN_VNUM(gen)) ? "new generic" : GEN_NAME(find_generic_by_vnum(GEN_VNUM(gen))));
	sprintf(buf + strlen(buf), "<\tyname\t0> %s\r\n", NULLSAFE(GEN_NAME(gen)));
	sprintf(buf + strlen(buf), "<\tytype\t0> %s\r\n", generic_types[GEN_TYPE(gen)]);
	
	sprintbit(GEN_FLAGS(gen), generic_flags, lbuf, TRUE);
	sprintf(buf + strlen(buf), "<\tyflags\t0> %s\r\n", lbuf);
	
	// GENERIC_x
	switch (GEN_TYPE(gen)) {
		case GENERIC_LIQUID: {
			sprintf(buf + strlen(buf), "<\tyliquid\t0> %s\r\n", GET_LIQUID_NAME(gen) ? GET_LIQUID_NAME(gen) : "(none)");
			sprintf(buf + strlen(buf), "<\tycolor\t0> %s\r\n", GET_LIQUID_COLOR(gen) ? GET_LIQUID_COLOR(gen) : "(none)");
			sprintf(buf + strlen(buf), "<\tyhunger\t0> %d hour%s\r\n", GET_LIQUID_FULL(gen), PLURAL(GET_LIQUID_FULL(gen)));
			sprintf(buf + strlen(buf), "<\tythirst\t0> %d hour%s\r\n", GET_LIQUID_THIRST(gen), PLURAL(GET_LIQUID_THIRST(gen)));
			sprintf(buf + strlen(buf), "<\tydrunk\t0> %d hour%s\r\n", GET_LIQUID_DRUNK(gen), PLURAL(GET_LIQUID_DRUNK(gen)));
			break;
		}
		case GENERIC_CURRENCY: {
			break;
		}
	}
		
	page_string(ch->desc, buf, TRUE);
}


/**
* Searches the generic db for a match, and prints it to the character.
*
* @param char *searchname The search string.
* @param char_data *ch The player who is searching.
* @return int The number of matches shown.
*/
int vnum_generic(char *searchname, char_data *ch) {
	generic_data *iter, *next_iter;
	int found = 0;
	
	HASH_ITER(hh, generic_table, iter, next_iter) {
		if (multi_isname(searchname, GEN_NAME(iter))) {
			msg_to_char(ch, "%3d. [%5d] %s\r\n", ++found, GEN_VNUM(iter), GEN_NAME(iter));
		}
	}
	
	return found;
}


 //////////////////////////////////////////////////////////////////////////////
//// GENERIC OLC MODULES /////////////////////////////////////////////////////

OLC_MODULE(genedit_flags) {
	generic_data *gen = GET_OLC_GENERIC(ch->desc);
	GEN_FLAGS(gen) = olc_process_flag(ch, argument, "generic", "flags", generic_flags, GEN_FLAGS(gen));
}


OLC_MODULE(genedit_name) {
	generic_data *gen = GET_OLC_GENERIC(ch->desc);
	olc_process_string(ch, argument, "name", &GEN_NAME(gen));
}


OLC_MODULE(genedit_type) {
	generic_data *gen = GET_OLC_GENERIC(ch->desc);
	int old = GEN_TYPE(gen), iter;
	
	GEN_TYPE(gen) = olc_process_type(ch, argument, "type", "type", generic_types, GEN_TYPE(gen));
	
	// reset values to defaults now
	if (old != GEN_TYPE(gen)) {
		for (iter = 0; iter < NUM_GENERIC_VALUES; ++iter) {
			GEN_VALUE(gen, iter) = 0;
		}
		for (iter = 0; iter < NUM_GENERIC_STRINGS; ++iter) {
			if (GEN_STRING(gen, iter)) {
				free(GEN_STRING(gen, iter));
				GEN_STRING(gen, iter) = NULL;
			}
		}
	}
}


 //////////////////////////////////////////////////////////////////////////////
//// LIQUID OLC MODULES //////////////////////////////////////////////////////

OLC_MODULE(genedit_color) {
	generic_data *gen = GET_OLC_GENERIC(ch->desc);
	
	if (GEN_TYPE(gen) != GENERIC_LIQUID) {
		msg_to_char(ch, "You can only change that on a LIQUID generic.\r\n");
	}
	else {
		olc_process_string(ch, argument, "color", &GEN_STRING(gen, GSTR_LIQUID_COLOR));
	}
}


OLC_MODULE(genedit_drunk) {
	generic_data *gen = GET_OLC_GENERIC(ch->desc);
	
	if (GEN_TYPE(gen) != GENERIC_LIQUID) {
		msg_to_char(ch, "You can only change that on a LIQUID generic.\r\n");
	}
	else {
		GEN_VALUE(gen, DRUNK) = olc_process_number(ch, argument, "drunk", "drunk", -MAX_LIQUID_COND, MAX_LIQUID_COND, GET_LIQUID_DRUNK(gen));
	}
}


OLC_MODULE(genedit_hunger) {
	generic_data *gen = GET_OLC_GENERIC(ch->desc);
	
	if (GEN_TYPE(gen) != GENERIC_LIQUID) {
		msg_to_char(ch, "You can only change that on a LIQUID generic.\r\n");
	}
	else {
		GEN_VALUE(gen, FULL) = olc_process_number(ch, argument, "hunger", "hunger", -MAX_LIQUID_COND, MAX_LIQUID_COND, GET_LIQUID_FULL(gen));
	}
}


OLC_MODULE(genedit_liquid) {
	generic_data *gen = GET_OLC_GENERIC(ch->desc);
	
	if (GEN_TYPE(gen) != GENERIC_LIQUID) {
		msg_to_char(ch, "You can only change that on a LIQUID generic.\r\n");
	}
	else {
		olc_process_string(ch, argument, "liquid", &GEN_STRING(gen, GSTR_LIQUID_NAME));
	}
}


OLC_MODULE(genedit_thirst) {
	generic_data *gen = GET_OLC_GENERIC(ch->desc);
	
	if (GEN_TYPE(gen) != GENERIC_LIQUID) {
		msg_to_char(ch, "You can only change that on a LIQUID generic.\r\n");
	}
	else {
		GEN_VALUE(gen, THIRST) = olc_process_number(ch, argument, "thirst", "thirst", -MAX_LIQUID_COND, MAX_LIQUID_COND, GET_LIQUID_THIRST(gen));
	}
}
