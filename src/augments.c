/* ************************************************************************
*   File: augments.c                                      EmpireMUD 2.0b5 *
*  Usage: DB and OLC for augments (item enchantments)                     *
*                                                                         *
*  EmpireMUD code base by Paul Clarke, (C) 2000-2024                      *
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
#include "constants.h"

/**
* Contents:
*   Helpers
*   Utilities
*   Database
*   OLC Handlers
*   Displays
*   Edit Modules
*/

// locals
const char *default_aug_name = "unnamed augment";


 //////////////////////////////////////////////////////////////////////////////
//// HELPERS /////////////////////////////////////////////////////////////////

/**
* Used for choosing an augment that's valid for the player.
*
* @param char_data *ch The person trying to augment.
* @param char *name The argument.
* @param int type AUGMENT_x type.
* @return augment_data* The matching augment, or NULL if none.
*/
augment_data *find_augment_by_name(char_data *ch, char *name, int type) {
	augment_data *aug, *next_aug, *partial = NULL;
	
	HASH_ITER(sorted_hh, sorted_augments, aug, next_aug) {
		if (GET_AUG_TYPE(aug) != type) {
			continue;
		}
		if (AUGMENT_FLAGGED(aug, AUG_IN_DEVELOPMENT) && !IS_IMMORTAL(ch)) {
			continue;
		}
		if (GET_AUG_ABILITY(aug) != NO_ABIL && !has_ability(ch, GET_AUG_ABILITY(aug))) {
			continue;
		}
		if (GET_AUG_REQUIRES_OBJ(aug) != NOTHING && !has_required_object(ch, GET_AUG_REQUIRES_OBJ(aug))) {
			continue;
		}
		
		// matches:
		if (!str_cmp(name, GET_AUG_NAME(aug))) {
			// perfect match
			return aug;
		}
		if (!partial && is_multiword_abbrev(name, GET_AUG_NAME(aug))) {
			// probable match
			partial = aug;
		}
	}
	
	// no exact match...
	return partial;
}


/**
* Checks targeting flags on augments.
*
* @param char_data *ch The person who will get any error messages.
* @param obj_data *obj The item to validate.
* @param augment_data *aug The augment we're trying to apply.
* @param bool send_messages If TRUE, sends the error message to ch.
* @return bool TRUE if successful, FALSE if an error was sent.
*/
bool validate_augment_target(char_data *ch, obj_data *obj, augment_data *aug, bool send_messages) {
	char buf[MAX_STRING_LENGTH], part[MAX_STRING_LENGTH];
	bitvector_t partial_wear, flags;
	int iter;
	
	flags = GET_AUG_FLAGS(aug) | augment_info[GET_AUG_TYPE(aug)].default_flags;
	
	// wear-based targeting
	partial_wear = GET_AUG_WEAR_FLAGS(aug) & ~ITEM_WEAR_TAKE;
	if (partial_wear != NOBITS && !CAN_WEAR(obj, partial_wear)) {
		if (send_messages) {
			prettier_sprintbit(partial_wear, wear_bits, part);
			snprintf(buf, sizeof(buf), "You can only use that %s on items that are worn on: %s.\r\n", augment_info[GET_AUG_TYPE(aug)].noun, part);
			for (iter = 1; iter < strlen(buf); ++iter) {
				buf[iter] = LOWER(buf[iter]);	// lowercase both parts of the string
			}
			send_to_char(buf, ch);
		}
		return FALSE;
	}
	
	if (IS_SET(flags, AUG_ARMOR) && !IS_ARMOR(obj)) {
		if (send_messages) {
			msg_to_char(ch, "You can only put that %s on armor.\r\n", augment_info[GET_AUG_TYPE(aug)].noun);
		}
		return FALSE;
	}
	if (IS_SET(flags, AUG_SHIELD) && !IS_SHIELD(obj)) {
		if (send_messages) {
			msg_to_char(ch, "You can only put that %s on a shield.\r\n", augment_info[GET_AUG_TYPE(aug)].noun);
		}
		return FALSE;
	}
	
	return TRUE;
}


/**
* Counts the words of text in an augment's strings.
*
* @param augment_data *aug The augment whose strings to count.
* @return int The number of words in the augment's strings.
*/
int wordcount_augment(augment_data *aug) {
	int count = 0;
	
	count += wordcount_string(GET_AUG_NAME(aug));
	
	return count;
}


 //////////////////////////////////////////////////////////////////////////////
//// UTILITIES ///////////////////////////////////////////////////////////////

/**
* Checks for common augment problems and reports them to ch.
*
* @param augment_data *aug The item to audit.
* @param char_data *ch The person to report to.
* @return bool TRUE if any problems were reported; FALSE if all good.
*/
bool audit_augment(augment_data *aug, char_data *ch) {
	char temp[MAX_STRING_LENGTH];
	struct apply_data *app;
	bool problem = FALSE;
	
	if (AUGMENT_FLAGGED(aug, AUG_IN_DEVELOPMENT)) {
		olc_audit_msg(ch, GET_AUG_VNUM(aug), "IN-DEVELOPMENT");
		problem = TRUE;
	}
	if (GET_AUG_TYPE(aug) == AUGMENT_NONE) {
		olc_audit_msg(ch, GET_AUG_VNUM(aug), "Type not set");
		problem = TRUE;
	}
	if (!GET_AUG_NAME(aug) || !*GET_AUG_NAME(aug) || !str_cmp(GET_AUG_NAME(aug), default_aug_name)) {
		olc_audit_msg(ch, GET_AUG_VNUM(aug), "No name set");
		problem = TRUE;
	}
	
	strcpy(temp, GET_AUG_NAME(aug));
	strtolower(temp);
	if (strcmp(GET_AUG_NAME(aug), temp)) {
		olc_audit_msg(ch, GET_AUG_VNUM(aug), "Non-lowercase name");
		problem = TRUE;
	}
	
	for (app = GET_AUG_APPLIES(aug); app; app = app->next) {
		if (app->location == APPLY_NONE || app->weight == 0) {
			olc_audit_msg(ch, GET_AUG_VNUM(aug), "Invalid apply: %d to %s", app->weight, apply_types[app->location]);
			problem = TRUE;
		}
	}
	
	if (GET_AUG_ABILITY(aug) == NO_ABIL && GET_AUG_REQUIRES_OBJ(aug) == NOTHING) {
		olc_audit_msg(ch, GET_AUG_VNUM(aug), "Requires no ability or object");
		problem = TRUE;
	}
	if (GET_AUG_REQUIRES_OBJ(aug) != NOTHING && !obj_proto(GET_AUG_REQUIRES_OBJ(aug))) {
		olc_audit_msg(ch, GET_AUG_VNUM(aug), "Requires-object does not exist");
		problem = TRUE;
	}
	
	if (!GET_AUG_RESOURCES(aug)) {
		olc_audit_msg(ch, GET_AUG_VNUM(aug), "No resources required");
		problem = TRUE;
	}
	
	// AUG_x: any new targeting flags must be added here
	if (GET_AUG_WEAR_FLAGS(aug) == NOBITS && !AUGMENT_FLAGGED(aug, AUG_ARMOR | AUG_SHIELD)) {
		olc_audit_msg(ch, GET_AUG_VNUM(aug), "No targeting flags");
		problem = TRUE;
	}
	
	return problem;
}


/**
* For the .list command.
*
* @param augment_data *aug The thing to list.
* @param bool detail If TRUE, provide additional details
* @return char* The line to show (without a CRLF).
*/
char *list_one_augment(augment_data *aug, bool detail) {
	static char output[MAX_STRING_LENGTH];
	char part[MAX_STRING_LENGTH], applies[MAX_STRING_LENGTH];
	struct apply_data *app;
	ability_data *abil;
	
	if (detail) {
		// ability required
		if (GET_AUG_ABILITY(aug) == NO_ABIL) {
			*part = '\0';
		}
		else {
			sprintf(part, " (%s", get_ability_name_by_vnum(GET_AUG_ABILITY(aug)));
			if ((abil = find_ability_by_vnum(GET_AUG_ABILITY(aug))) && ABIL_ASSIGNED_SKILL(abil) != NULL) {
				sprintf(part + strlen(part), " - %s %d", SKILL_ABBREV(ABIL_ASSIGNED_SKILL(abil)), ABIL_SKILL_LEVEL(abil));
			}
			strcat(part, ")");
		}
		
		// applies
		*applies = '\0';
		for (app = GET_AUG_APPLIES(aug); app; app = app->next) {
			sprintf(applies + strlen(applies), "%s%d to %s", (app == GET_AUG_APPLIES(aug)) ? " " : ", ", app->weight, apply_types[app->location]);
		}
		
		snprintf(output, sizeof(output), "[%5d] %s%s%s", GET_AUG_VNUM(aug), GET_AUG_NAME(aug), part, applies);
	}
	else {
		snprintf(output, sizeof(output), "[%5d] %s", GET_AUG_VNUM(aug), GET_AUG_NAME(aug));
	}
		
	return output;
}


/**
* Searches for all uses of an augment and displays them.
*
* @param char_data *ch The player.
* @param any_vnum vnum The augment vnum.
*/
void olc_search_augment(char_data *ch, any_vnum vnum) {
	augment_data *aug = augment_proto(vnum);
	int found;
	
	if (!aug) {
		msg_to_char(ch, "There is no augment %d.\r\n", vnum);
		return;
	}
	
	found = 0;
	build_page_display(ch, "Occurrences of augment %d (%s):", vnum, GET_AUG_NAME(aug));
	
	// augments are not actually used anywhere else
	
	if (found > 0) {
		build_page_display(ch, "%d location%s shown", found, PLURAL(found));
	}
	else {
		build_page_display_str(ch, " none");
	}
	
	send_page_display(ch);
}


// Simple vnum sorter for the augment hash
int sort_augments(augment_data *a, augment_data *b) {
	return GET_AUG_VNUM(a) - GET_AUG_VNUM(b);
}


// typealphabetic sorter for sorted_augments
int sort_augments_by_data(augment_data *a, augment_data *b) {
	ability_data *a_abil, *b_abil;
	int a_level, b_level;
	
	if (GET_AUG_TYPE(a) != GET_AUG_TYPE(b)) {
		return GET_AUG_TYPE(a) - GET_AUG_TYPE(b);
	}
	
	a_abil = find_ability_by_vnum(GET_AUG_ABILITY(a));
	b_abil = find_ability_by_vnum(GET_AUG_ABILITY(b));
	a_level = a_abil ? ABIL_SKILL_LEVEL(a_abil) : 0;
	b_level = b_abil ? ABIL_SKILL_LEVEL(b_abil) : 0;
	
	// reverse level sort
	if (a_level != b_level) {
		return b_level - a_level;
	}
	
	return strcmp(NULLSAFE(GET_AUG_NAME(a)), NULLSAFE(GET_AUG_NAME(b)));
}


 //////////////////////////////////////////////////////////////////////////////
//// DATABASE ////////////////////////////////////////////////////////////////

/**
* Puts an augment into the hash table.
*
* @param augment_data *aug The augment data to add to the table.
*/
void add_augment_to_table(augment_data *aug) {
	augment_data *find;
	any_vnum vnum;
	
	if (aug) {
		vnum = GET_AUG_VNUM(aug);
		HASH_FIND_INT(augment_table, &vnum, find);
		if (!find) {
			HASH_ADD_INT(augment_table, vnum, aug);
			HASH_SORT(augment_table, sort_augments);
		}
		
		// sorted table
		HASH_FIND(sorted_hh, sorted_augments, &vnum, sizeof(int), find);
		if (!find) {
			HASH_ADD(sorted_hh, sorted_augments, vnum, sizeof(int), aug);
			HASH_SRT(sorted_hh, sorted_augments, sort_augments_by_data);
		}
	}
}


/**
* @param any_vnum vnum Any augment vnum
* @return augment_data* The augment, or NULL if it doesn't exist
*/
augment_data *augment_proto(any_vnum vnum) {
	augment_data *aug;
	
	if (vnum < 0 || vnum == NOTHING) {
		return NULL;
	}
	
	HASH_FIND_INT(augment_table, &vnum, aug);
	return aug;
}


/**
* Removes an augment from the hash table.
*
* @param augment_data *aug The augment data to remove from the table.
*/
void remove_augment_from_table(augment_data *aug) {
	HASH_DEL(augment_table, aug);
	HASH_DELETE(sorted_hh, sorted_augments, aug);
}


/**
* Initializes a new augment. This clears all memory for it, so set the vnum
* AFTER.
*
* @param augment_data *aug The augment to initialize.
*/
void clear_augment(augment_data *aug) {
	memset((char *) aug, 0, sizeof(augment_data));
	
	GET_AUG_VNUM(aug) = NOTHING;
	GET_AUG_ABILITY(aug) = NO_ABIL;
	GET_AUG_REQUIRES_OBJ(aug) = NOTHING;
}


/**
* frees up memory for an augment data item.
*
* See also: olc_delete_augment
*
* @param augment_data *aug The augment data to free.
*/
void free_augment(augment_data *aug) {
	augment_data *proto = augment_proto(GET_AUG_VNUM(aug));
	
	if (GET_AUG_NAME(aug) && (!proto || GET_AUG_NAME(aug) != GET_AUG_NAME(proto))) {
		free(GET_AUG_NAME(aug));
	}
	
	if (GET_AUG_APPLIES(aug) && (!proto || GET_AUG_APPLIES(aug) != GET_AUG_APPLIES(proto))) {
		free_apply_list(GET_AUG_APPLIES(aug));
	}
	
	if (GET_AUG_RESOURCES(aug) && (!proto || GET_AUG_RESOURCES(aug) != GET_AUG_RESOURCES(proto))) {
		free_resource_list(GET_AUG_RESOURCES(aug));
	}
	
	free(aug);
}


/**
* Read one augment from file.
*
* @param FILE *fl The open .aug file
* @param any_vnum vnum The augment vnum
*/
void parse_augment(FILE *fl, any_vnum vnum) {
	char line[256], error[256], str_in[256], str_in2[256];
	augment_data *aug, *find;
	int int_in[4];
	
	CREATE(aug, augment_data, 1);
	clear_augment(aug);
	GET_AUG_VNUM(aug) = vnum;
	
	HASH_FIND_INT(augment_table, &vnum, find);
	if (find) {
		log("WARNING: Duplicate augment vnum #%d", vnum);
		// but have to load it anyway to advance the file
	}
	add_augment_to_table(aug);
		
	// for error messages
	sprintf(error, "augment vnum %d", vnum);
	
	// line 1
	GET_AUG_NAME(aug) = fread_string(fl, error);
	
	// line 2: type flags wear-flags ability
	if (!get_line(fl, line) || sscanf(line, "%d %s %s %d %d", &int_in[0], str_in, str_in2, &int_in[1], &int_in[2]) != 5) {
		log("SYSERR: Format error in line 2 of %s", error);
		exit(1);
	}
	
	GET_AUG_TYPE(aug) = int_in[0];
	GET_AUG_FLAGS(aug) = asciiflag_conv(str_in);
	GET_AUG_WEAR_FLAGS(aug) = asciiflag_conv(str_in2);
	GET_AUG_ABILITY(aug) = int_in[1];
	GET_AUG_REQUIRES_OBJ(aug) = int_in[2];
		
	// optionals
	for (;;) {
		if (!get_line(fl, line)) {
			log("SYSERR: Format error in %s, expecting alphabetic flags", error);
			exit(1);
		}
		switch (*line) {
			case 'A': {	// applies
				parse_apply(fl, &GET_AUG_APPLIES(aug), error);
				break;
			}
			
			case 'R': {	// resources
				parse_resource(fl, &GET_AUG_RESOURCES(aug), error);
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


// writes entries in the augment index
void write_augments_index(FILE *fl) {
	augment_data *aug, *next_aug;
	int this, last;
	
	last = -1;
	HASH_ITER(hh, augment_table, aug, next_aug) {
		// determine "zone number" by vnum
		this = (int)(GET_AUG_VNUM(aug) / 100);
	
		if (this != last) {
			fprintf(fl, "%d%s\n", this, AUG_SUFFIX);
			last = this;
		}
	}
}


/**
* Outputs one augment item in the db file format, starting with a #VNUM and
* ending with an S.
*
* @param FILE *fl The file to write it to.
* @param augment_data *aug The thing to save.
*/
void write_augment_to_file(FILE *fl, augment_data *aug) {
	char temp[256], temp2[256];
	
	if (!fl || !aug) {
		syslog(SYS_ERROR, LVL_START_IMM, TRUE, "SYSERR: write_augment_to_file called without %s", !fl ? "file" : "augment");
		return;
	}
	
	fprintf(fl, "#%d\n", GET_AUG_VNUM(aug));
	
	// 1. name
	fprintf(fl, "%s~\n", NULLSAFE(GET_AUG_NAME(aug)));
	
	// 2. type flags wear-flags ability requires-obj
	strcpy(temp, bitv_to_alpha(GET_AUG_FLAGS(aug)));
	strcpy(temp2, bitv_to_alpha(GET_AUG_WEAR_FLAGS(aug)));
	fprintf(fl, "%d %s %s %d %d\n", GET_AUG_TYPE(aug), temp, temp2, GET_AUG_ABILITY(aug), GET_AUG_REQUIRES_OBJ(aug));
	
	// 'A': applies
	write_applies_to_file(fl, GET_AUG_APPLIES(aug));
	
	// 'R': resources
	write_resources_to_file(fl, 'R', GET_AUG_RESOURCES(aug));
	
	// end
	fprintf(fl, "S\n");
}


 //////////////////////////////////////////////////////////////////////////////
//// OLC HANDLERS ////////////////////////////////////////////////////////////


/**
* Creates a new augment entry.
* 
* @param any_vnum vnum The number to create.
* @return augment_data* The new augment's prototype.
*/
augment_data *create_augment_table_entry(any_vnum vnum) {
	augment_data *aug;
	
	// sanity
	if (augment_proto(vnum)) {
		log("SYSERR: Attempting to insert augment at existing vnum %d", vnum);
		return augment_proto(vnum);
	}
	
	CREATE(aug, augment_data, 1);
	clear_augment(aug);
	GET_AUG_VNUM(aug) = vnum;
	GET_AUG_NAME(aug) = str_dup(default_aug_name);
	add_augment_to_table(aug);

	// save index and augment file now
	save_index(DB_BOOT_AUG);
	save_library_file_for_vnum(DB_BOOT_AUG, vnum);

	return aug;
}


/**
* WARNING: This function actually deletes an augment.
*
* @param char_data *ch The person doing the deleting.
* @param any_vnum vnum The vnum to delete.
*/
void olc_delete_augment(char_data *ch, any_vnum vnum) {
	augment_data *aug;
	char name[256];
	
	if (!(aug = augment_proto(vnum))) {
		msg_to_char(ch, "There is no such augment %d.\r\n", vnum);
		return;
	}
	
	snprintf(name, sizeof(name), "%s", NULLSAFE(GET_AUG_NAME(aug)));
	
	// remove it from the hash table first
	remove_augment_from_table(aug);

	// save index and augment file now
	save_index(DB_BOOT_AUG);
	save_library_file_for_vnum(DB_BOOT_AUG, vnum);
	
	syslog(SYS_OLC, GET_INVIS_LEV(ch), TRUE, "OLC: %s has deleted augment %d %s", GET_NAME(ch), vnum, name);
	msg_to_char(ch, "Augment %d (%s) deleted.\r\n", vnum, name);
	
	free_augment(aug);
}


/**
* Function to save a player's changes to an augment (or a new one).
*
* @param descriptor_data *desc The descriptor who is saving.
*/
void save_olc_augment(descriptor_data *desc) {	
	augment_data *proto, *aug = GET_OLC_AUGMENT(desc);
	any_vnum vnum = GET_OLC_VNUM(desc);
	UT_hash_handle hh, sorted;

	// have a place to save it?
	if (!(proto = augment_proto(vnum))) {
		proto = create_augment_table_entry(vnum);
	}
	
	// free prototype strings and pointers
	if (GET_AUG_NAME(proto)) {
		free(GET_AUG_NAME(proto));
	}
	free_apply_list(GET_AUG_APPLIES(proto));
	free_resource_list(GET_AUG_RESOURCES(proto));
	
	// sanity
	if (!GET_AUG_NAME(aug) || !*GET_AUG_NAME(aug)) {
		if (GET_AUG_NAME(aug)) {
			free(GET_AUG_NAME(aug));
		}
		GET_AUG_NAME(aug) = str_dup(default_aug_name);
	}

	// save data back over the proto-type
	hh = proto->hh;	// save old hash handle
	sorted = proto->sorted_hh;
	*proto = *aug;	// copy over all data
	proto->vnum = vnum;	// ensure correct vnum
	proto->hh = hh;	// restore old hash handle
	proto->sorted_hh = sorted;
		
	// and save to file
	save_library_file_for_vnum(DB_BOOT_AUG, vnum);

	// ... and re-sort
	HASH_SRT(sorted_hh, sorted_augments, sort_augments_by_data);
}


/**
* Creates a copy of an augment, or clears a new one, for editing.
* 
* @param augment_data *input The augment to copy, or NULL to make a new one.
* @return augment_data* The copied augment.
*/
augment_data *setup_olc_augment(augment_data *input) {
	augment_data *new;
	
	CREATE(new, augment_data, 1);
	clear_augment(new);
	
	if (input) {
		// copy normal data
		*new = *input;

		// copy things that are pointers
		GET_AUG_NAME(new) = GET_AUG_NAME(input) ? str_dup(GET_AUG_NAME(input)) : NULL;
		
		// copy lists
		GET_AUG_APPLIES(new) = copy_apply_list(GET_AUG_APPLIES(input));
		GET_AUG_RESOURCES(new) = copy_resource_list(GET_AUG_RESOURCES(input));
	}
	else {
		// brand new: some defaults
		GET_AUG_NAME(new) = str_dup(default_aug_name);
		GET_AUG_FLAGS(new) = AUG_IN_DEVELOPMENT;
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
* @param augment_data *aug The augment to display.
*/
void do_stat_augment(char_data *ch, augment_data *aug) {
	char part[MAX_STRING_LENGTH];
	struct apply_data *app;
	ability_data *abil;
	int num;
	struct page_display *pd;
	
	if (!aug) {
		return;
	}
	
	// first line
	build_page_display(ch, "VNum: [\tc%d\t0], Name: \tc%s\t0", GET_AUG_VNUM(aug), GET_AUG_NAME(aug));
	
	snprintf(part, sizeof(part), "%s", (GET_AUG_ABILITY(aug) == NO_ABIL ? "none" : get_ability_name_by_vnum(GET_AUG_ABILITY(aug))));
	if ((abil = find_ability_by_vnum(GET_AUG_ABILITY(aug))) && ABIL_ASSIGNED_SKILL(abil) != NULL) {
		snprintf(part + strlen(part), sizeof(part) - strlen(part), " (%s %d)", SKILL_ABBREV(ABIL_ASSIGNED_SKILL(abil)), ABIL_SKILL_LEVEL(abil));
	}
	build_page_display(ch, "Type: [\ty%s\t0], Requires Ability: [\ty%s\t0]", augment_types[GET_AUG_TYPE(aug)], part);
	
	if (GET_AUG_REQUIRES_OBJ(aug) != NOTHING) {
		build_page_display(ch, "Requires item: [%d] \tg%s\t0", GET_AUG_REQUIRES_OBJ(aug), skip_filler(get_obj_name_by_proto(GET_AUG_REQUIRES_OBJ(aug))));
	}
	
	sprintbit(GET_AUG_FLAGS(aug), augment_flags, part, TRUE);
	build_page_display(ch, "Flags: \tg%s\t0", part);
	
	sprintbit(GET_AUG_WEAR_FLAGS(aug), wear_bits, part, TRUE);
	build_page_display(ch, "Targets wear location: \ty%s\t0", part);
	
	// applies
	pd = build_page_display(ch, "Applies: ");
	for (app = GET_AUG_APPLIES(aug), num = 0; app; app = app->next, ++num) {
		append_page_display_line(pd, "%s%d to %s", num ? ", " : "", app->weight, apply_types[app->location]);
	}
	if (!GET_AUG_APPLIES(aug)) {
		append_page_display_line(pd, "none");
	}
	
	// resources
	build_page_display_str(ch, "Resource cost:");
	show_resource_display(ch, GET_AUG_RESOURCES(aug), FALSE);
	
	send_page_display(ch);
}


/**
* This is the main recipe display for augment OLC. It displays the user's
* currently-edited augment.
*
* @param char_data *ch The person who is editing an augment and will see its display.
*/
void olc_show_augment(char_data *ch) {
	augment_data *aug = GET_OLC_AUGMENT(ch->desc);
	char lbuf[MAX_STRING_LENGTH];
	struct apply_data *app;
	ability_data *abil;
	int num;
	
	if (!aug) {
		return;
	}
	
	build_page_display(ch, "[%s%d\t0] %s%s\t0", OLC_LABEL_CHANGED, GET_OLC_VNUM(ch->desc), OLC_LABEL_UNCHANGED, !augment_proto(GET_AUG_VNUM(aug)) ? "new augment" : GET_AUG_NAME(augment_proto(GET_AUG_VNUM(aug))));
	build_page_display(ch, "<%sname\t0> %s", OLC_LABEL_STR(GET_AUG_NAME(aug), default_aug_name), NULLSAFE(GET_AUG_NAME(aug)));
	
	build_page_display(ch, "<%stype\t0> %s", OLC_LABEL_VAL(GET_AUG_TYPE(aug), 0), augment_types[GET_AUG_TYPE(aug)]);

	sprintbit(GET_AUG_FLAGS(aug), augment_flags, lbuf, TRUE);
	build_page_display(ch, "<%sflags\t0> %s", OLC_LABEL_VAL(GET_AUG_FLAGS(aug), AUG_IN_DEVELOPMENT), lbuf);
	
	sprintbit(GET_AUG_WEAR_FLAGS(aug), wear_bits, lbuf, TRUE);
	build_page_display(ch, "<%swear\t0> %s", OLC_LABEL_VAL(GET_AUG_WEAR_FLAGS(aug), NOBITS), lbuf);
	
	// ability required
	if (GET_AUG_ABILITY(aug) == NO_ABIL || !(abil = find_ability_by_vnum(GET_AUG_ABILITY(aug)))) {
		strcpy(buf1, "none");
	}
	else {
		sprintf(buf1, "%s", ABIL_NAME(abil));
		if (ABIL_ASSIGNED_SKILL(abil)) {
			sprintf(buf1 + strlen(buf1), " (%s %d)", SKILL_NAME(ABIL_ASSIGNED_SKILL(abil)), ABIL_SKILL_LEVEL(abil));
		}
	}
	build_page_display(ch, "<%srequiresability\t0> %s", OLC_LABEL_VAL(GET_AUG_ABILITY(aug), NO_ABIL), buf1);

	build_page_display(ch, "<%srequiresobject\t0> %d - %s", OLC_LABEL_VAL(GET_AUG_REQUIRES_OBJ(aug), NOTHING), GET_AUG_REQUIRES_OBJ(aug), GET_AUG_REQUIRES_OBJ(aug) == NOTHING ? "none" : get_obj_name_by_proto(GET_AUG_REQUIRES_OBJ(aug)));
	
	// applies
	build_page_display(ch, "Attribute applies: <%sapply\t0>", OLC_LABEL_PTR(GET_AUG_APPLIES(aug)));
	for (app = GET_AUG_APPLIES(aug), num = 1; app; app = app->next, ++num) {
		build_page_display_col(ch, 2, FALSE, " %2d. %d to %s", num, app->weight, apply_types[app->location]);
	}
	
	// resources
	build_page_display(ch, "Resources required: <%sresource\t0>", OLC_LABEL_PTR(GET_AUG_RESOURCES(aug)));
	if (GET_AUG_RESOURCES(aug)) {
		show_resource_display(ch, GET_AUG_RESOURCES(aug), FALSE);
	}
	
	send_page_display(ch);
}


/**
* Searches the augment db for a match, and prints it to the character.
*
* @param char *searchname The search string.
* @param char_data *ch The player who is searching.
* @return int The number of matches shown.
*/
int vnum_augment(char *searchname, char_data *ch) {
	augment_data *iter, *next_iter;
	int found = 0;
	
	HASH_ITER(hh, augment_table, iter, next_iter) {
		if (multi_isname(searchname, GET_AUG_NAME(iter))) {
			build_page_display(ch, "%3d. [%5d] %s (%s)", ++found, GET_AUG_VNUM(iter), GET_AUG_NAME(iter), augment_types[GET_AUG_TYPE(iter)]);
		}
	}
	
	send_page_display(ch);
	return found;
}


 //////////////////////////////////////////////////////////////////////////////
//// OLC MODULES /////////////////////////////////////////////////////////////

OLC_MODULE(augedit_ability) {
	augment_data *aug = GET_OLC_AUGMENT(ch->desc);
	ability_data *abil;
	
	if (!*argument) {
		msg_to_char(ch, "Require what ability (or 'none')?\r\n");
	}
	else if (!str_cmp(argument, "none")) {
		GET_AUG_ABILITY(aug) = NO_ABIL;
		
		if (PRF_FLAGGED(ch, PRF_NOREPEAT)) {
			send_config_msg(ch, "ok_string");
		}
		else {
			msg_to_char(ch, "It will require no ability.\r\n");
		}
	}
	else if (!(abil = find_ability(argument))) {
		msg_to_char(ch, "Invalid ability '%s'.\r\n", argument);
	}
	else {
		GET_AUG_ABILITY(aug) = ABIL_VNUM(abil);
		
		if (PRF_FLAGGED(ch, PRF_NOREPEAT)) {
			send_config_msg(ch, "ok_string");
		}
		else {
			msg_to_char(ch, "It now requires the %s ability.\r\n", ABIL_NAME(abil));
		}
	}
}


OLC_MODULE(augedit_apply) {
	augment_data *aug = GET_OLC_AUGMENT(ch->desc);
	olc_process_applies(ch, argument, &GET_AUG_APPLIES(aug));
}


OLC_MODULE(augedit_flags) {
	augment_data *aug = GET_OLC_AUGMENT(ch->desc);
	bool had_indev = IS_SET(GET_AUG_FLAGS(aug), AUG_IN_DEVELOPMENT) ? TRUE : FALSE;
	
	GET_AUG_FLAGS(aug) = olc_process_flag(ch, argument, "augment", "flags", augment_flags, GET_AUG_FLAGS(aug));
	
	// validate removal of IN-DEVELOPMENT
	if (had_indev && !IS_SET(GET_AUG_FLAGS(aug), AUG_IN_DEVELOPMENT) && GET_ACCESS_LEVEL(ch) < LVL_UNRESTRICTED_BUILDER && !OLC_FLAGGED(ch, OLC_FLAG_CLEAR_IN_DEV)) {
		msg_to_char(ch, "You don't have permission to remove the IN-DEVELOPMENT flag.\r\n");
		SET_BIT(GET_AUG_FLAGS(aug), AUG_IN_DEVELOPMENT);
	}
}


OLC_MODULE(augedit_name) {
	augment_data *aug = GET_OLC_AUGMENT(ch->desc);
	olc_process_string(ch, argument, "name", &GET_AUG_NAME(aug));
}


OLC_MODULE(augedit_requiresobject) {
	augment_data *aug = GET_OLC_AUGMENT(ch->desc);
	obj_vnum old = GET_AUG_REQUIRES_OBJ(aug);
	
	if (!str_cmp(argument, "none") || atoi(argument) == NOTHING) {
		GET_AUG_REQUIRES_OBJ(aug) = NOTHING;
		if (PRF_FLAGGED(ch, PRF_NOREPEAT)) {
			send_config_msg(ch, "ok_string");
		}
		else {
			msg_to_char(ch, "It no longer requires an object to see it in the %s list.\r\n", augment_types[GET_AUG_TYPE(aug)]);
		}
	}
	else {
		GET_AUG_REQUIRES_OBJ(aug) = olc_process_number(ch, argument, "object vnum", "requiresobject", 0, MAX_VNUM, GET_AUG_REQUIRES_OBJ(aug));
		if (!obj_proto(GET_AUG_REQUIRES_OBJ(aug))) {
			GET_AUG_REQUIRES_OBJ(aug) = old;
			msg_to_char(ch, "There is no object with that vnum. Old value restored.\r\n");
		}
		else if (!PRF_FLAGGED(ch, PRF_NOREPEAT)) {
			msg_to_char(ch, "It now requires %s.\r\n", get_obj_name_by_proto(GET_AUG_REQUIRES_OBJ(aug)));
		}
	}
}


OLC_MODULE(augedit_resource) {
	augment_data *aug = GET_OLC_AUGMENT(ch->desc);
	olc_process_resources(ch, argument, &GET_AUG_RESOURCES(aug));
}


OLC_MODULE(augedit_type) {
	augment_data *aug = GET_OLC_AUGMENT(ch->desc);
	GET_AUG_TYPE(aug) = olc_process_type(ch, argument, "type", "type", augment_types, GET_AUG_TYPE(aug));
}


OLC_MODULE(augedit_wear) {
	augment_data *aug = GET_OLC_AUGMENT(ch->desc);	
	GET_AUG_WEAR_FLAGS(aug) = olc_process_flag(ch, argument, "wear", "wear", wear_bits, GET_AUG_WEAR_FLAGS(aug));
}
