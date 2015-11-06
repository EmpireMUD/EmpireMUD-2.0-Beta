/* ************************************************************************
*   File: archetypes.c                                    EmpireMUD 2.0b3 *
*  Usage: DB and OLC for character creation archetype                     *
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

// local defaults
const char *default_archetype_name = "unnamed archetype";
const char *default_archetype_desc = "no description";
const char *default_archetype_rank = "Adventurer";

// external consts
extern const char *archetype_flags[];
extern int attribute_display_order[NUM_ATTRIBUTES];
extern struct attribute_data_type attributes[NUM_ATTRIBUTES];
extern const struct wear_data_type wear_data[NUM_WEARS];

// external funcs


 //////////////////////////////////////////////////////////////////////////////
//// HELPERS /////////////////////////////////////////////////////////////////


 //////////////////////////////////////////////////////////////////////////////
//// UTILITIES ///////////////////////////////////////////////////////////////

/**
* Checks for common archetype problems and reports them to ch.
*
* @param archetype_data *arch The item to audit.
* @param char_data *ch The person to report to.
* @return bool TRUE if any problems were reported; FALSE if all good.
*/
bool audit_archetype(archetype_data *arch, char_data *ch) {
	bool problem = FALSE;
	
	if (ARCHETYPE_FLAGGED(arch, ARCH_IN_DEVELOPMENT)) {
		olc_audit_msg(ch, GET_ARCH_VNUM(arch), "IN-DEVELOPMENT");
		problem = TRUE;
	}
	if (!GET_ARCH_NAME(arch) || !*GET_ARCH_NAME(arch) || !str_cmp(GET_ARCH_NAME(arch), default_archetype_name)) {
		olc_audit_msg(ch, GET_ARCH_VNUM(arch), "No name set");
		problem = TRUE;
	}
	
	return problem;
}


/**
* For the .list command.
*
* @param archetype_data *arch The thing to list.
* @param bool detail If TRUE, provide additional details
* @return char* The line to show (without a CRLF).
*/
char *list_one_archetype(archetype_data *arch, bool detail) {
	static char output[MAX_STRING_LENGTH];
	
	if (detail) {
		// TODO list skills?
		snprintf(output, sizeof(output), "[%5d] %s", GET_ARCH_VNUM(arch), GET_ARCH_NAME(arch));
	}
	else {
		snprintf(output, sizeof(output), "[%5d] %s", GET_ARCH_VNUM(arch), GET_ARCH_NAME(arch));
	}
		
	return output;
}


/**
* Searches for all uses of an archetype and displays them.
*
* @param char_data *ch The player.
* @param any_vnum vnum The archetype vnum.
*/
void olc_search_archetype(char_data *ch, any_vnum vnum) {
	char buf[MAX_STRING_LENGTH];
	archetype_data *arch = archetype_proto(vnum);
	int size, found;
	
	if (!arch) {
		msg_to_char(ch, "There is no archetype %d.\r\n", vnum);
		return;
	}
	
	found = 0;
	size = snprintf(buf, sizeof(buf), "Occurrences of archetype %d (%s):\r\n", vnum, GET_ARCH_NAME(arch));
	
	// archetype are not actually used anywhere else
	
	if (found > 0) {
		size += snprintf(buf + size, sizeof(buf) - size, "%d location%s shown\r\n", found, PLURAL(found));
	}
	else {
		size += snprintf(buf + size, sizeof(buf) - size, " none\r\n");
	}
	
	page_string(ch->desc, buf, TRUE);
}


// Simple vnum sorter for the archetype hash
int sort_archetypes(archetype_data *a, archetype_data *b) {
	return GET_ARCH_VNUM(a) - GET_ARCH_VNUM(b);
}


// typealphabetic sorter for sorted_archetypes
int sort_archetypes_by_data(archetype_data *a, archetype_data *b) {
	// TODO what to sort by other than name?
	return strcmp(GET_ARCH_NAME(a), GET_ARCH_NAME(b));
}


 //////////////////////////////////////////////////////////////////////////////
//// DATABASE ////////////////////////////////////////////////////////////////

/**
* Puts an archetype into the hash table.
*
* @param archetype_data *arch The archetype data to add to the table.
*/
void add_archetype_to_table(archetype_data *arch) {
	archetype_data *find;
	any_vnum vnum;
	
	if (arch) {
		vnum = GET_ARCH_VNUM(arch);
		HASH_FIND_INT(archetype_table, &vnum, find);
		if (!find) {
			HASH_ADD_INT(archetype_table, vnum, arch);
			HASH_SORT(archetype_table, sort_archetypes);
		}
		
		// sorted table
		HASH_FIND(sorted_hh, sorted_archetypes, &vnum, sizeof(int), find);
		if (!find) {
			HASH_ADD(sorted_hh, sorted_archetypes, vnum, sizeof(int), arch);
			HASH_SRT(sorted_hh, sorted_archetypes, sort_archetypes_by_data);
		}
	}
}


/**
* @param any_vnum vnum Any archetype vnum
* @return archetype_data* The archetype, or NULL if it doesn't exist
*/
archetype_data *archetype_proto(any_vnum vnum) {
	archetype_data *arch;
	
	if (vnum < 0 || vnum == NOTHING) {
		return NULL;
	}
	
	HASH_FIND_INT(archetype_table, &vnum, arch);
	return arch;
}


/**
* Removes an archetype from the hash table.
*
* @param archetype_data *arch The archetype data to remove from the table.
*/
void remove_archetype_from_table(archetype_data *arch) {
	HASH_DEL(archetype_table, arch);
	HASH_DELETE(sorted_hh, sorted_archetypes, arch);
}


/**
* Initializes a new archetype. This clears all memory for it, so set the vnum
* AFTER.
*
* @param archetype_data *arch The archetype to initialize.
*/
void clear_archetype(archetype_data *arch) {
	memset((char *) arch, 0, sizeof(archetype_data));
	int iter;
	
	GET_ARCH_VNUM(arch) = NOTHING;
	
	// default attributes to 1
	for (iter = 0; iter < NUM_ATTRIBUTES; ++iter) {
		GET_ARCH_ATTRIBUTE(arch, iter) = 1;
	}
}


/**
* Copies a list of archetype gear.
*
* @param struct archetype_gear *input The list to copy.
* @return struct archetype_gear* The copied list.
*/
struct archetype_gear *copy_archetype_gear(struct archetype_gear *input) {
	struct archetype_gear *new, *last, *list, *iter;
	
	last = NULL;
	list = NULL;
	for (iter = input; iter; iter = iter->next) {
		CREATE(new, struct archetype_gear, 1);
		*new = *iter;
		new->next = NULL;
		
		if (last) {
			last->next = new;
		}
		else {
			list = new;
		}
		last = new;
	}
	
	return list;
}


/**
* Copies a list of archetype skills.
*
* @param struct archetype_skill *input The list to copy.
* @return struct archetype_skill* The copied list.
*/
struct archetype_skill *copy_archetype_skills(struct archetype_skill *input) {
	struct archetype_skill *new, *last, *list, *iter;
	
	last = NULL;
	list = NULL;
	for (iter = input; iter; iter = iter->next) {
		CREATE(new, struct archetype_skill, 1);
		*new = *iter;
		new->next = NULL;
		
		if (last) {
			last->next = new;
		}
		else {
			list = new;
		}
		last = new;
	}
	
	return list;
}


/**
* Frees a list of archetype gear.
*
* @param struct archetype_gear *list The start of the list to free.
*/
void free_archetype_gear(struct archetype_gear *list) {
	struct archetype_gear *gear;
	while ((gear = list)) {
		list = gear->next;
		free(gear);
	}
}


/**
* Frees a list of archetype skills.
*
* @param struct archetype_skill *list The start of the list to free.
*/
void free_archetype_skills(struct archetype_skill *list) {
	struct archetype_skill *sk;
	while ((sk = list)) {
		list = sk->next;
		free(sk);
	}
}


/**
* frees up memory for an archetype data item.
*
* See also: olc_delete_archetype
*
* @param archetype_data *arch The archetype data to free.
*/
void free_archetype(archetype_data *arch) {
	archetype_data *proto = archetype_proto(GET_ARCH_VNUM(arch));
	
	if (GET_ARCH_NAME(arch) && (!proto || GET_ARCH_NAME(arch) != GET_ARCH_NAME(proto))) {
		free(GET_ARCH_NAME(arch));
	}
	if (GET_ARCH_DESC(arch) && (!proto || GET_ARCH_DESC(arch) != GET_ARCH_DESC(proto))) {
		free(GET_ARCH_DESC(arch));
	}
	if (GET_ARCH_MALE_RANK(arch) && (!proto || GET_ARCH_MALE_RANK(arch) != GET_ARCH_MALE_RANK(proto))) {
		free(GET_ARCH_MALE_RANK(arch));
	}
	if (GET_ARCH_FEMALE_RANK(arch) && (!proto || GET_ARCH_FEMALE_RANK(arch) != GET_ARCH_FEMALE_RANK(proto))) {
		free(GET_ARCH_FEMALE_RANK(arch));
	}
	
	if (GET_ARCH_GEAR(arch) && (!proto || GET_ARCH_GEAR(arch) != GET_ARCH_GEAR(proto))) {
		free_archetype_gear(GET_ARCH_GEAR(arch));
	}
	if (GET_ARCH_SKILLS(arch) && (!proto || GET_ARCH_SKILLS(arch) != GET_ARCH_SKILLS(proto))) {
		free_archetype_skills(GET_ARCH_SKILLS(arch));
	}
	
	free(arch);
}


/**
* Read one archetype from file.
*
* @param FILE *fl The open .arch file
* @param any_vnum vnum The archetype vnum
*/
void parse_archetype(FILE *fl, any_vnum vnum) {
	char line[256], error[256], str_in[256];
	struct archetype_gear *gear, *last_gear = NULL;
	struct archetype_skill *sk, *last_sk = NULL;
	archetype_data *arch, *find;
	int int_in[4];
	
	CREATE(arch, archetype_data, 1);
	clear_archetype(arch);
	GET_ARCH_VNUM(arch) = vnum;
	
	HASH_FIND_INT(archetype_table, &vnum, find);
	if (find) {
		log("WARNING: Duplicate archetype vnum #%d", vnum);
		// but have to load it anyway to advance the file
	}
	add_archetype_to_table(arch);
		
	// for error messages
	sprintf(error, "archetype vnum %d", vnum);
	
	// lines 1-4: name, desc, male rank, female rank
	GET_ARCH_NAME(arch) = fread_string(fl, error);
	GET_ARCH_DESC(arch) = fread_string(fl, error);
	GET_ARCH_MALE_RANK(arch) = fread_string(fl, error);
	GET_ARCH_FEMALE_RANK(arch) = fread_string(fl, error);
	
	// line 5: flags
	if (!get_line(fl, line) || sscanf(line, "%s", str_in) != 1) {
		log("SYSERR: Format error in line 5 of %s", error);
		exit(1);
	}
	
	GET_ARCH_FLAGS(arch) = asciiflag_conv(str_in);
		
	// optionals
	for (;;) {
		if (!get_line(fl, line)) {
			log("SYSERR: Format error in %s, expecting alphabetic flags", error);
			exit(1);
		}
		switch (*line) {
			case 'A': {	// attributes: type level
				if (!get_line(fl, line) || sscanf(line, "%d %d", &int_in[0], &int_in[1]) != 2) {
					log("SYSERR: format error in A line of %s", error);
					exit(1);
				}
				
				// only accept valid attributes
				if (int_in[0] >= 0 && int_in[0] < NUM_ATTRIBUTES) {
					GET_ARCH_ATTRIBUTE(arch, int_in[0]) = int_in[1];
				}
				break;
			}
			case 'G': {	// gear: loc vnum
				if (!get_line(fl, line) || sscanf(line, "%d %d", &int_in[0], &int_in[1]) != 2) {
					log("SYSERR: format error in G line of %s", error);
					exit(1);
				}
				
				// only accept valid positions
				if (int_in[0] >= -1 && int_in[0] < NUM_WEARS) {
					CREATE(gear, struct archetype_gear, 1);
					gear->wear = int_in[0];
					gear->vnum = int_in[1];
				
					// append
					if (last_gear) {
						last_gear->next = gear;
					}
					else {
						GET_ARCH_GEAR(arch) = gear;
					}
					last_gear = gear;
				}
				break;
			}
			case 'K': {	// skill: number level
				if (!get_line(fl, line) || sscanf(line, "%d %d", &int_in[0], &int_in[1]) != 2) {
					log("SYSERR: format error in K line of %s", error);
					exit(1);
				}
				
				// only accept valid skills
				if (int_in[0] >= 0 && int_in[0] < NUM_SKILLS) {
					CREATE(sk, struct archetype_skill, 1);
					sk->skill = int_in[0];
					sk->level = int_in[1];
				
					// append
					if (last_sk) {
						last_sk->next = sk;
					}
					else {
						GET_ARCH_SKILLS(arch) = sk;
					}
					last_sk = sk;
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


// writes entries in the archetype index
void write_archetype_index(FILE *fl) {
	archetype_data *arch, *next_arch;
	int this, last;
	
	last = -1;
	HASH_ITER(hh, archetype_table, arch, next_arch) {
		// determine "zone number" by vnum
		this = (int)(GET_ARCH_VNUM(arch) / 100);
	
		if (this != last) {
			fprintf(fl, "%d%s\n", this, ARCH_SUFFIX);
			last = this;
		}
	}
}


/**
* Outputs one archetype in the db file format, starting with a #VNUM and
* ending with an S.
*
* @param FILE *fl The file to write it to.
* @param archetype_data *arch The thing to save.
*/
void write_archetype_to_file(FILE *fl, archetype_data *arch) {
	struct archetype_gear *gear;
	struct archetype_skill *sk;
	int iter;
	
	if (!fl || !arch) {
		syslog(SYS_ERROR, LVL_START_IMM, TRUE, "SYSERR: write_archetype_to_file called without %s", !fl ? "file" : "archetype");
		return;
	}
	
	fprintf(fl, "#%d\n", GET_ARCH_VNUM(arch));
	
	// 1-4. strings
	fprintf(fl, "%s~\n", NULLSAFE(GET_ARCH_NAME(arch)));
	fprintf(fl, "%s~\n", NULLSAFE(GET_ARCH_DESC(arch)));
	fprintf(fl, "%s~\n", NULLSAFE(GET_ARCH_MALE_RANK(arch)));
	fprintf(fl, "%s~\n", NULLSAFE(GET_ARCH_FEMALE_RANK(arch)));
	
	// 5. flags
	fprintf(fl, "%s\n", bitv_to_alpha(GET_ARCH_FLAGS(arch)));
	
	// 'A': attributes
	for (iter = 0; iter < NUM_ATTRIBUTES; ++iter) {
		if (GET_ARCH_ATTRIBUTE(arch, iter) != 1) {
			fprintf(fl, "A\n%d %d\n", iter, GET_ARCH_ATTRIBUTE(arch, iter));
		}
	}
	
	// G: gear
	for (gear = GET_ARCH_GEAR(arch); gear; gear = gear->next) {
		fprintf(fl, "G\n%d %d\n", gear->wear, gear->vnum);
	}
	
	// K: skills
	for (sk = GET_ARCH_SKILLS(arch); sk; sk = sk->next) {
		fprintf(fl, "K\n%d %d\n", sk->skill, sk->level);
	}
	
	// end
	fprintf(fl, "S\n");
}


 //////////////////////////////////////////////////////////////////////////////
//// OLC HANDLERS ////////////////////////////////////////////////////////////


/**
* Creates a new archetype entry.
* 
* @param any_vnum vnum The number to create.
* @return archetype_data* The new archetype's prototype.
*/
archetype_data *create_archetype_table_entry(any_vnum vnum) {
	archetype_data *arch;
	
	// sanity
	if (archetype_proto(vnum)) {
		log("SYSERR: Attempting to insert archetype at existing vnum %d", vnum);
		return archetype_proto(vnum);
	}
	
	CREATE(arch, archetype_data, 1);
	clear_archetype(arch);
	GET_ARCH_VNUM(arch) = vnum;
	GET_ARCH_NAME(arch) = str_dup(default_archetype_name);
	GET_ARCH_DESC(arch) = str_dup(default_archetype_desc);
	GET_ARCH_MALE_RANK(arch) = str_dup(default_archetype_rank);
	GET_ARCH_FEMALE_RANK(arch) = str_dup(default_archetype_rank);
	add_archetype_to_table(arch);

	// save index and archetype file now
	save_index(DB_BOOT_ARCH);
	save_library_file_for_vnum(DB_BOOT_ARCH, vnum);

	return arch;
}


/**
* WARNING: This function actually deletes an archetype.
*
* @param char_data *ch The person doing the deleting.
* @param any_vnum vnum The vnum to delete.
*/
void olc_delete_archetype(char_data *ch, any_vnum vnum) {
	archetype_data *arch;
	
	if (!(arch = archetype_proto(vnum))) {
		msg_to_char(ch, "There is no such archetype %d.\r\n", vnum);
		return;
	}
	
	// remove it from the hash table first
	remove_archetype_from_table(arch);

	// save index and archetype file now
	save_index(DB_BOOT_ARCH);
	save_library_file_for_vnum(DB_BOOT_ARCH, vnum);
	
	syslog(SYS_OLC, GET_INVIS_LEV(ch), TRUE, "OLC: %s has deleted archetype %d", GET_NAME(ch), vnum);
	msg_to_char(ch, "Archetype %d deleted.\r\n", vnum);
	
	free_archetype(arch);
}


/**
* Function to save a player's changes to an archetype (or a new one).
*
* @param descriptor_data *desc The descriptor who is saving.
*/
void save_olc_archetype(descriptor_data *desc) {	
	archetype_data *proto, *arch = GET_OLC_ARCHETYPE(desc);
	any_vnum vnum = GET_OLC_VNUM(desc);
	UT_hash_handle hh, sorted;

	// have a place to save it?
	if (!(proto = archetype_proto(vnum))) {
		proto = create_archetype_table_entry(vnum);
	}
	
	// free prototype strings and pointers
	if (GET_ARCH_NAME(proto)) {
		free(GET_ARCH_NAME(proto));
	}
	if (GET_ARCH_DESC(proto)) {
		free(GET_ARCH_DESC(proto));
	}
	if (GET_ARCH_MALE_RANK(proto)) {
		free(GET_ARCH_MALE_RANK(proto));
	}
	if (GET_ARCH_FEMALE_RANK(proto)) {
		free(GET_ARCH_FEMALE_RANK(proto));
	}
	free_archetype_gear(GET_ARCH_GEAR(proto));
	free_archetype_skills(GET_ARCH_SKILLS(proto));
	
	// sanity
	if (!GET_ARCH_NAME(arch) || !*GET_ARCH_NAME(arch)) {
		if (GET_ARCH_NAME(arch)) {
			free(GET_ARCH_NAME(arch));
		}
		GET_ARCH_NAME(arch) = str_dup(default_archetype_name);
	}
	if (!GET_ARCH_DESC(arch) || !*GET_ARCH_DESC(arch)) {
		if (GET_ARCH_DESC(arch)) {
			free(GET_ARCH_DESC(arch));
		}
		GET_ARCH_DESC(arch) = str_dup(default_archetype_desc);
	}
	if (!GET_ARCH_MALE_RANK(arch) || !*GET_ARCH_MALE_RANK(arch)) {
		if (GET_ARCH_MALE_RANK(arch)) {
			free(GET_ARCH_MALE_RANK(arch));
		}
		GET_ARCH_MALE_RANK(arch) = str_dup(default_archetype_rank);
	}
	if (!GET_ARCH_FEMALE_RANK(arch) || !*GET_ARCH_FEMALE_RANK(arch)) {
		if (GET_ARCH_FEMALE_RANK(arch)) {
			free(GET_ARCH_FEMALE_RANK(arch));
		}
		GET_ARCH_FEMALE_RANK(arch) = str_dup(default_archetype_rank);
	}

	// save data back over the proto-type
	hh = proto->hh;	// save old hash handle
	sorted = proto->sorted_hh;
	*proto = *arch;	// copy over all data
	proto->vnum = vnum;	// ensure correct vnum
	proto->hh = hh;	// restore old hash handle
	proto->sorted_hh = sorted;
		
	// and save to file
	save_library_file_for_vnum(DB_BOOT_ARCH, vnum);

	// ... and re-sort
	HASH_SRT(sorted_hh, sorted_archetypes, sort_archetypes_by_data);
}


/**
* Creates a copy of an archetype, or clears a new one, for editing.
* 
* @param archetype_data *input The archetype to copy, or NULL to make a new one.
* @return archetype_data* The copied archetype.
*/
archetype_data *setup_olc_archetype(archetype_data *input) {
	archetype_data *new;
	
	CREATE(new, archetype_data, 1);
	clear_archetype(new);
	
	if (input) {
		// copy normal data
		*new = *input;

		// copy things that are pointers
		GET_ARCH_NAME(new) = GET_ARCH_NAME(input) ? str_dup(GET_ARCH_NAME(input)) : NULL;
		GET_ARCH_DESC(new) = GET_ARCH_DESC(input) ? str_dup(GET_ARCH_DESC(input)) : NULL;
		GET_ARCH_MALE_RANK(new) = GET_ARCH_MALE_RANK(input) ? str_dup(GET_ARCH_MALE_RANK(input)) : NULL;
		GET_ARCH_FEMALE_RANK(new) = GET_ARCH_FEMALE_RANK(input) ? str_dup(GET_ARCH_FEMALE_RANK(input)) : NULL;
		
		// copy lists
		GET_ARCH_GEAR(new) = copy_archetype_gear(GET_ARCH_GEAR(input));
		GET_ARCH_SKILLS(new) = copy_archetype_skills(GET_ARCH_SKILLS(input));
	}
	else {
		// brand new: some defaults
		GET_ARCH_NAME(new) = str_dup(default_archetype_name);
		GET_ARCH_DESC(new) = str_dup(default_archetype_desc);
		GET_ARCH_MALE_RANK(new) = str_dup(default_archetype_rank);
		GET_ARCH_FEMALE_RANK(new) = str_dup(default_archetype_rank);
		GET_ARCH_FLAGS(new) = ARCH_IN_DEVELOPMENT;
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
* @param archetype_data *arch The archetype to display.
*/
void do_stat_archetype(char_data *ch, archetype_data *arch) {
	char buf[MAX_STRING_LENGTH], part[MAX_STRING_LENGTH];
	struct archetype_gear *gear;
	struct archetype_skill *sk;
	int iter, num, pos, total;
	size_t size;
	
	if (!arch) {
		return;
	}
	
	// first line
	size = snprintf(buf, sizeof(buf), "VNum: [\tc%d\t0], Name: \tc%s\t0\r\n", GET_ARCH_VNUM(arch), GET_ARCH_NAME(arch));
	
	size += snprintf(buf + size, sizeof(buf) - size, "Description: %s\r\n", GET_ARCH_DESC(arch));
	size += snprintf(buf + size, sizeof(buf) - size, "Ranks: [\ta%s\t0/\tp%s\t0]\r\n", GET_ARCH_MALE_RANK(arch), GET_ARCH_FEMALE_RANK(arch));
	
	sprintbit(GET_ARCH_FLAGS(arch), archetype_flags, part, TRUE);
	size += snprintf(buf + size, sizeof(buf) - size, "Flags: \tg%s\t0\r\n", part);
		
	// attributes
	total = 0;
	for (iter = 0; iter < NUM_ATTRIBUTES; ++iter) {
		total += GET_ARCH_ATTRIBUTE(arch, iter);
	}
	size += snprintf(buf + size, sizeof(buf) - size, "Attributes: [\tc%d total attributes\t0]\r\n", total);
	for (iter = 0; iter < NUM_ATTRIBUTES; ++iter) {
		pos = attribute_display_order[iter];
		snprintf(part, sizeof(part), "%s  [\tg%2d\t0]", attributes[pos].name, GET_ARCH_ATTRIBUTE(arch, pos));
		size += snprintf(buf + size, sizeof(buf) - size, "  %-27.27s%s", part, !((iter + 1) % 3) ? "\r\n" : "");
	}
	if (iter % 3) {
		strcat(buf, "\r\n");
	}
	
	// skills
	total = 0;
	for (sk = GET_ARCH_SKILLS(arch); sk; sk = sk->next) {
		total += sk->level;
	}
	size += snprintf(buf + size, sizeof(buf) - size, "Skills: [\tc%d total skill points\t0]\r\n", total);
	for (sk = GET_ARCH_SKILLS(arch); sk; sk = sk->next) {
		size += snprintf(buf + size, sizeof(buf) - size, "  %s: \tg%d\t0\r\n", skill_data[sk->skill].name, sk->level);
	}
	
	// gear
	size += snprintf(buf + size, sizeof(buf) - size, "Gear:\r\n");
	for (gear = GET_ARCH_GEAR(arch), num = 1; gear; gear = gear->next, ++num) {
		size += snprintf(buf + size, sizeof(buf) - size, " %2d. %s: [\ty%d\t0] \ty%s\t0\r\n", num, gear->wear == -1 ? "inventory" : wear_data[gear->wear].name, gear->vnum, get_obj_name_by_proto(gear->vnum));
	}
	
	page_string(ch->desc, buf, TRUE);
}


/**
* This is the main recipe display for archetype OLC. It displays the user's
* currently-edited archetype.
*
* @param char_data *ch The person who is editing an archetype and will see its display.
*/
void olc_show_archetype(char_data *ch) {
	archetype_data *arch = GET_OLC_ARCHETYPE(ch->desc);
	char buf[MAX_STRING_LENGTH], lbuf[MAX_STRING_LENGTH];
	struct archetype_gear *gear;
	struct archetype_skill *sk;
	int num, iter, pos, total;
	
	if (!arch) {
		return;
	}
	
	*buf = '\0';
	
	sprintf(buf + strlen(buf), "[&c%d&0] &c%s&0\r\n", GET_OLC_VNUM(ch->desc), !archetype_proto(GET_ARCH_VNUM(arch)) ? "new archetype" : GET_ARCH_NAME(archetype_proto(GET_ARCH_VNUM(arch))));
	sprintf(buf + strlen(buf), "<&yname&0> %s\r\n", NULLSAFE(GET_ARCH_NAME(arch)));
	sprintf(buf + strlen(buf), "<&ydescription&0> %s\r\n", NULLSAFE(GET_ARCH_DESC(arch)));
	
	sprintbit(GET_ARCH_FLAGS(arch), archetype_flags, lbuf, TRUE);
	sprintf(buf + strlen(buf), "<&yflags&0> %s\r\n", lbuf);
	
	sprintf(buf + strlen(buf), "<&ymalerank&0> %s\r\n", NULLSAFE(GET_ARCH_MALE_RANK(arch)));
	sprintf(buf + strlen(buf), "<&yfemalerank&0> %s\r\n", NULLSAFE(GET_ARCH_FEMALE_RANK(arch)));
	
	// attributes
	total = 0;
	for (iter = 0; iter < NUM_ATTRIBUTES; ++iter) {
		total += GET_ARCH_ATTRIBUTE(arch, iter);
	}
	sprintf(buf + strlen(buf), "Attributes: <&yattribute&0> (%d total attributes)\r\n", total);
	for (iter = 0; iter < NUM_ATTRIBUTES; ++iter) {
		pos = attribute_display_order[iter];
		sprintf(lbuf, "%s  [%2d]", attributes[pos].name, GET_ARCH_ATTRIBUTE(arch, pos));
		sprintf(buf + strlen(buf), "  %-23.23s%s", lbuf, !((iter + 1) % 3) ? "\r\n" : "");
	}
	if (iter % 3) {
		strcat(buf, "\r\n");
	}
	
	// skills
	total = 0;
	for (sk = GET_ARCH_SKILLS(arch); sk; sk = sk->next) {
		total += sk->level;
	}
	sprintf(buf + strlen(buf), "Skills: <&yskills&0> (%d total skill points)\r\n", total);
	for (sk = GET_ARCH_SKILLS(arch); sk; sk = sk->next) {
		sprintf(buf + strlen(buf), "  %s: %d\r\n", skill_data[sk->skill].name, sk->level);
	}
	
	// gear
	sprintf(buf + strlen(buf), "Gear: <&ygear&0>\r\n");
	for (gear = GET_ARCH_GEAR(arch), num = 1; gear; gear = gear->next, ++num) {
		sprintf(buf + strlen(buf), " %2d. %s: [%d] %s\r\n", num, gear->wear == -1 ? "inventory" : wear_data[gear->wear].name, gear->vnum, get_obj_name_by_proto(gear->vnum));
	}
	
	page_string(ch->desc, buf, TRUE);
}


/**
* Searches the archetype db for a match, and prints it to the character.
*
* @param char *searchname The search string.
* @param char_data *ch The player who is searching.
* @return int The number of matches shown.
*/
int vnum_archetype(char *searchname, char_data *ch) {
	archetype_data *iter, *next_iter;
	int found = 0;
	
	HASH_ITER(hh, archetype_table, iter, next_iter) {
		if (multi_isname(searchname, GET_ARCH_NAME(iter)) || multi_isname(searchname, GET_ARCH_MALE_RANK(iter)) || multi_isname(searchname, GET_ARCH_FEMALE_RANK(iter)) || multi_isname(searchname, GET_ARCH_DESC(iter))) {
			// TODO show skills/attrs?
			msg_to_char(ch, "%3d. [%5d] %s\r\n", ++found, GET_ARCH_VNUM(iter), GET_ARCH_NAME(iter));
		}
	}
	
	return found;
}


 //////////////////////////////////////////////////////////////////////////////
//// OLC MODULES /////////////////////////////////////////////////////////////

/*
You begin editing archetype 1:
[1] new archetype
<name> unnamed archetype
<description> no description
<flags> IN-DEVELOPMENT 
<malerank> Adventurer
<femalerank> Adventurer
Attributes: <attribute> (6 total attributes)
  Strength  [ 1]           Dexterity  [ 1]          Charisma  [ 1]         
  Greatness  [ 1]          Intelligence  [ 1]       Wits  [ 1]             
Skills: <skills> (0 total skill points)
Gear: <gear>
*/

OLC_MODULE(archedit_apply) {
	archetype_data *arch = GET_OLC_ARCHETYPE(ch->desc);
	/*
	char arg1[MAX_INPUT_LENGTH], arg2[MAX_INPUT_LENGTH], arg3[MAX_INPUT_LENGTH];
	char num_arg[MAX_INPUT_LENGTH], type_arg[MAX_INPUT_LENGTH], val_arg[MAX_INPUT_LENGTH];
	struct augment_apply *apply, *next_apply, *change, *temp;
	int loc, num, iter;
	bool found;
	
	// arg1 arg2 arg3
	half_chop(argument, arg1, buf);
	half_chop(buf, arg2, arg3);
	
	if (is_abbrev(arg1, "remove")) {
		if (!*arg2) {
			msg_to_char(ch, "Remove which apply (number)?\r\n");
		}
		else if (!str_cmp(arg2, "all")) {
			free_augment_applies(GET_AUG_APPLIES(aug));
			GET_AUG_APPLIES(aug) = NULL;
			msg_to_char(ch, "You remove all the applies.\r\n");
		}
		else if (!isdigit(*arg2) || (num = atoi(arg2)) < 1) {
			msg_to_char(ch, "Invalid apply number.\r\n");
		}
		else {
			found = FALSE;
			for (apply = GET_AUG_APPLIES(aug); apply && !found; apply = next_apply) {
				next_apply = apply->next;
				if (--num == 0) {
					found = TRUE;
					
					msg_to_char(ch, "You remove the %d to %s.\r\n", apply->weight, apply_types[apply->location]);
					REMOVE_FROM_LIST(apply, GET_AUG_APPLIES(aug), next);
					free(apply);
				}
			}
			
			if (!found) {
				msg_to_char(ch, "Invalid apply number.\r\n");
			}
		}
	}
	else if (is_abbrev(arg1, "add")) {
		num = atoi(arg2);
		
		if (!*arg2 || !*arg3 || !isdigit(*arg2) || num <= 0) {
			msg_to_char(ch, "Usage: apply add <value> <apply>\r\n");
		}
		else if ((loc = search_block(arg3, apply_types, FALSE)) == NOTHING) {
			msg_to_char(ch, "Invalid apply.\r\n");
		}
		else {
			CREATE(apply, struct augment_apply, 1);
			apply->location = loc;
			apply->weight = num;
			
			// append to end
			if ((temp = GET_AUG_APPLIES(aug))) {
				while (temp->next) {
					temp = temp->next;
				}
				temp->next = apply;
			}
			else {
				GET_AUG_APPLIES(aug) = apply;
			}
			
			msg_to_char(ch, "You add %d to %s.\r\n", num, apply_types[loc]);
		}
	}
	else if (is_abbrev(arg1, "change")) {
		strcpy(num_arg, arg2);
		half_chop(arg3, type_arg, val_arg);
		
		if (!*num_arg || !isdigit(*num_arg) || !*type_arg || !*val_arg) {
			msg_to_char(ch, "Usage: apply change <number> <value | apply> <new value>\r\n");
			return;
		}
		
		// find which one to change
		if (!isdigit(*num_arg) || (num = atoi(num_arg)) < 1) {
			msg_to_char(ch, "Invalid apply number.\r\n");
			return;
		}
		change = NULL;
		for (apply = GET_AUG_APPLIES(aug); apply && !change; apply = apply->next) {
			if (--num == 0) {
				change = apply;
				break;
			}
		}
		if (!change) {
			msg_to_char(ch, "Invalid apply number.\r\n");
		}
		else if (is_abbrev(type_arg, "value")) {
			num = atoi(val_arg);
			if ((!isdigit(*val_arg) && *val_arg != '-') || num == 0) {
				msg_to_char(ch, "Invalid value '%s'.\r\n", val_arg);
			}
			else {
				change->weight = num;
				msg_to_char(ch, "Apply %d changed to value %d.\r\n", atoi(num_arg), num);
			}
		}
		else if (is_abbrev(type_arg, "apply")) {
			if ((loc = search_block(val_arg, apply_types, FALSE)) == NOTHING) {
				msg_to_char(ch, "Invalid apply.\r\n");
			}
			else {
				change->location = loc;
				msg_to_char(ch, "Apply %d changed to %s.\r\n", atoi(num_arg), apply_types[loc]);
			}
		}
		else {
			msg_to_char(ch, "You can only change the value or apply.\r\n");
		}
	}
	else {
		msg_to_char(ch, "Usage: apply add <value> <apply>\r\n");
		msg_to_char(ch, "Usage: apply change <number> <value | apply> <new value>\r\n");
		msg_to_char(ch, "Usage: apply remove <number | all>\r\n");
		
		msg_to_char(ch, "Available applies:\r\n");
		for (iter = 0; *apply_types[iter] != '\n'; ++iter) {
			msg_to_char(ch, " %-24.24s%s", apply_types[iter], ((iter % 2) ? "\r\n" : ""));
		}
		if ((iter % 2) != 0) {
			msg_to_char(ch, "\r\n");
		}
	}
	*/
}


OLC_MODULE(archedit_flags) {
	archetype_data *arch = GET_OLC_ARCHETYPE(ch->desc);
	bool had_indev = IS_SET(GET_ARCH_FLAGS(arch), ARCH_IN_DEVELOPMENT) ? TRUE : FALSE;
	
	GET_ARCH_FLAGS(arch) = olc_process_flag(ch, argument, "archetype", "flags", archetype_flags, GET_ARCH_FLAGS(arch));
	
	// validate removal of IN-DEVELOPMENT
	if (had_indev && !IS_SET(GET_ARCH_FLAGS(arch), ARCH_IN_DEVELOPMENT) && GET_ACCESS_LEVEL(ch) < LVL_UNRESTRICTED_BUILDER && !OLC_FLAGGED(ch, OLC_FLAG_CLEAR_IN_DEV)) {
		msg_to_char(ch, "You don't have permission to remove the IN-DEVELOPMENT flag.\r\n");
		SET_BIT(GET_ARCH_FLAGS(arch), ARCH_IN_DEVELOPMENT);
	}
}


OLC_MODULE(archedit_name) {
	archetype_data *arch = GET_OLC_ARCHETYPE(ch->desc);
	olc_process_string(ch, argument, "name", &GET_ARCH_NAME(arch));
}
