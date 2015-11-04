/* ************************************************************************
*   File: augments.c                                      EmpireMUD 2.0b3 *
*  Usage: DB and OLC for augments (item enchantments)                     *
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

// external consts
extern const char *apply_types[];
extern const char *augment_types[];
extern const char *augment_flags[];
extern const char *wear_bits[];


// external funcs
extern struct resource_data *copy_resource_list(struct resource_data *input);
void free_resource_list(struct resource_data *list);


// local protos


 //////////////////////////////////////////////////////////////////////////////
//// HELPERS /////////////////////////////////////////////////////////////////


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
	bool problem = FALSE;

	if (AUGMENT_FLAGGED(aug, AUG_IN_DEVELOPMENT)) {
		olc_audit_msg(ch, GET_AUG_VNUM(aug), "IN-DEVELOPMENT");
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
	char abil[MAX_STRING_LENGTH];
	
	// ability required
	if (GET_AUG_ABILITY(aug) == NO_ABIL) {
		*abil = '\0';
	}
	else {
		sprintf(abil, " (%s", ability_data[GET_AUG_ABILITY(aug)].name);
		if (ability_data[GET_AUG_ABILITY(aug)].parent_skill != NO_SKILL) {
			sprintf(abil + strlen(abil), " - %s %d", skill_data[ability_data[GET_AUG_ABILITY(aug)].parent_skill].name, ability_data[GET_AUG_ABILITY(aug)].parent_skill_required);
		}
		strcat(abil, ")");
	}
	
	if (detail) {
		snprintf(output, sizeof(output), "[%5d] %s%s", GET_AUG_VNUM(aug), GET_AUG_NAME(aug), abil);
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
	char buf[MAX_STRING_LENGTH];
	augment_data *aug = augment_proto(vnum);
	int size, found;
	
	if (!aug) {
		msg_to_char(ch, "There is no augment %d.\r\n", vnum);
		return;
	}
	
	found = 0;
	size = snprintf(buf, sizeof(buf), "Occurrences of augment %d (%s):\r\n", vnum, GET_AUG_NAME(aug));
	
	// augments are not actually used anywhere else
	
	if (found > 0) {
		size += snprintf(buf + size, sizeof(buf) - size, "%d location%s shown\r\n", found, PLURAL(found));
	}
	else {
		size += snprintf(buf + size, sizeof(buf) - size, " none\r\n");
	}
	
	page_string(ch->desc, buf, TRUE);
}


/**
* Simple sorter for the augment hash
*
* @param augment_data *a One element
* @param augment_data *b Another element
* @return int Sort instruction of -1, 0, or 1
*/
int sort_augments(augment_data *a, augment_data *b) {
	return GET_AUG_VNUM(a) - GET_AUG_VNUM(b);
}


 //////////////////////////////////////////////////////////////////////////////
//// DATABASE ////////////////////////////////////////////////////////////////

/**
* Puts an augment into the hash table.
*
* @param augment_data *aug The augment data to add to the table.
*/
void add_augment_to_table(augment_data *aug) {
	extern int sort_augments(augment_data *a, augment_data *b);
	
	augment_data *find;
	any_vnum vnum;
	
	if (aug) {
		vnum = GET_AUG_VNUM(aug);
		HASH_FIND_INT(augment_table, &vnum, find);
		if (!find) {
			HASH_ADD_INT(augment_table, vnum, aug);
			HASH_SORT(augment_table, sort_augments);
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
}


/**
* Copies a list of augment applies.
*
* @param struct augment_apply *input The list to copy.
* @return struct augment_apply* The copied list.
*/
struct augment_apply *copy_augment_applies(struct augment_apply *input) {
	struct augment_apply *new, *last, *list, *iter;
	
	last = NULL;
	list = NULL;
	for (iter = input; iter; iter = iter->next) {
		CREATE(new, struct augment_apply, 1);
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
* Frees a list of augment applies.
*
* @param struct augment_apply *list The start of the list to free.
*/
void free_augment_applies(struct augment_apply *list) {
	struct augment_apply *app;
	while ((app = list)) {
		list = app->next;
		free(app);
	}
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
		free_augment_applies(GET_AUG_APPLIES(aug));
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
	void parse_resource(FILE *fl, struct resource_data **list, char *error_str);

	char line[256], error[256], str_in[256], str_in2[256];
	struct augment_apply *app, *last_app = NULL;
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
	if (!get_line(fl, line) || sscanf(line, "%d %s %s %d", &int_in[0], str_in, str_in2, &int_in[1]) != 3) {
		log("SYSERR: Format error in line 2 of %s", error);
		exit(1);
	}
	
	GET_AUG_TYPE(aug) = int_in[0];
	GET_AUG_FLAGS(aug) = asciiflag_conv(str_in);
	GET_AUG_WEAR_FLAGS(aug) = asciiflag_conv(str_in2);
	GET_AUG_ABILITY(aug) = int_in[1];
		
	// optionals
	for (;;) {
		if (!get_line(fl, line)) {
			log("SYSERR: Format error in %s, expecting alphabetic flags", error);
			exit(1);
		}
		switch (*line) {
			case 'A': {	// applies
				if (!get_line(fl, line) || sscanf(line, "%d %d", &int_in[0], &int_in[1]) != 2) {
					log("SYSERR: format error in A line of %s", error);
					exit(1);
				}
				
				CREATE(app, struct augment_apply, 1);
				app->location = int_in[0];
				app->weight = int_in[1];
				
				// append
				if (last_app) {
					last_app->next = app;
				}
				else {
					GET_AUG_APPLIES(aug) = app;
				}
				last_app = app;
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
	void write_resources_to_file(FILE *fl, struct resource_data *list);
	
	char temp[256], temp2[256];
	struct augment_apply *app;
	
	if (!fl || !aug) {
		syslog(SYS_ERROR, LVL_START_IMM, TRUE, "SYSERR: write_augment_to_file called without %s", !fl ? "file" : "augment");
		return;
	}
	
	fprintf(fl, "#%d\n", GET_AUG_VNUM(aug));
	
	// 1. name
	fprintf(fl, "%s~\n", NULLSAFE(GET_AUG_NAME(aug)));
	
	// 2. type flags wear-flags ability
	strcpy(temp, bitv_to_alpha(GET_AUG_FLAGS(aug)));
	strcpy(temp2, bitv_to_alpha(GET_AUG_WEAR_FLAGS(aug)));
	fprintf(fl, "%d %s %s %d\n", GET_AUG_TYPE(aug), temp, temp, GET_AUG_ABILITY(aug));
	
	// 'A': applies
	for (app = GET_AUG_APPLIES(aug); app; app = app->next) {
		fprintf(fl, "A\n%d %d\n", app->location, app->weight);
	}
	
	// 'R': resources
	write_resources_to_file(fl, GET_AUG_RESOURCES(aug));
	
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
	
	if (!(aug = augment_proto(vnum))) {
		msg_to_char(ch, "There is no such augment %d.\r\n", vnum);
		return;
	}
	
	// remove it from the hash table first
	remove_augment_from_table(aug);

	// save index and augment file now
	save_index(DB_BOOT_AUG);
	save_library_file_for_vnum(DB_BOOT_AUG, vnum);
	
	syslog(SYS_OLC, GET_INVIS_LEV(ch), TRUE, "OLC: %s has deleted augment %d", GET_NAME(ch), vnum);
	msg_to_char(ch, "Augment %d deleted.\r\n", vnum);
	
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
	UT_hash_handle hh;

	// have a place to save it?
	if (!(proto = augment_proto(vnum))) {
		proto = create_augment_table_entry(vnum);
	}
	
	// free prototype strings and pointers
	if (GET_AUG_NAME(proto)) {
		free(GET_AUG_NAME(proto));
	}
	free_augment_applies(GET_AUG_APPLIES(proto));
	free_resource_list(GET_AUG_RESOURCES(proto));
	
	// sanity
	if (!GET_AUG_NAME(aug) || !*GET_AUG_NAME(aug)) {
		if (GET_AUG_NAME(aug)) {
			free(GET_AUG_NAME(aug));
		}
		GET_AUG_NAME(aug) = str_dup("unnamed augment");
	}

	// save data back over the proto-type
	hh = proto->hh;	// save old hash handle
	*proto = *aug;	// copy over all data
	proto->vnum = vnum;	// ensure correct vnum
	proto->hh = hh;	// restore old hash handle
		
	// and save to file
	save_library_file_for_vnum(DB_BOOT_AUG, vnum);
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
		GET_AUG_APPLIES(new) = copy_augment_applies(GET_AUG_APPLIES(input));
		GET_AUG_RESOURCES(new) = copy_resource_list(GET_AUG_RESOURCES(input));
	}
	else {
		// brand new: some defaults
		GET_AUG_NAME(new) = str_dup("unnamed augment");
		GET_AUG_FLAGS(new) = AUG_IN_DEVELOPMENT;
	}
	
	// done
	return new;	
}


 //////////////////////////////////////////////////////////////////////////////
//// DISPLAYS ////////////////////////////////////////////////////////////////

/**
* This is the main recipe display for augment OLC. It displays the user's
* currently-edited augment.
*
* @param char_data *ch The person who is editing an augment and will see its display.
*/
void olc_show_augment(char_data *ch) {
	void get_resource_display(struct resource_data *list, char *save_buffer);
	
	augment_data *aug = GET_OLC_AUGMENT(ch->desc);
	char buf[MAX_STRING_LENGTH], lbuf[MAX_STRING_LENGTH];
	struct augment_apply *app;
	int num;
	
	if (!aug) {
		return;
	}
	
	*buf = '\0';
	
	sprintf(buf + strlen(buf), "[&c%d&0] &c%s&0\r\n", GET_OLC_VNUM(ch->desc), !augment_proto(GET_AUG_VNUM(aug)) ? "new augment" : GET_AUG_NAME(augment_proto(GET_AUG_VNUM(aug))));
	sprintf(buf + strlen(buf), "<&yname&0> %s\r\n", NULLSAFE(GET_AUG_NAME(aug)));
	
	sprintf(buf + strlen(buf), "<&ytype&0> %s\r\n", augment_types[GET_AUG_TYPE(aug)]);

	sprintbit(GET_AUG_FLAGS(aug), augment_flags, lbuf, TRUE);
	sprintf(buf + strlen(buf), "<&yflags&0> %s\r\n", lbuf);
	
	sprintbit(GET_AUG_WEAR_FLAGS(aug), wear_bits, lbuf, TRUE);
	sprintf(buf + strlen(buf), "<&ywear&0> %s\r\n", lbuf);
	
	// ability required
	if (GET_AUG_ABILITY(aug) == NO_ABIL) {
		strcpy(buf1, "none");
	}
	else {
		sprintf(buf1, "%s", ability_data[GET_AUG_ABILITY(aug)].name);
		if (ability_data[GET_AUG_ABILITY(aug)].parent_skill != NO_SKILL) {
			sprintf(buf1 + strlen(buf1), " (%s %d)", skill_data[ability_data[GET_AUG_ABILITY(aug)].parent_skill].name, ability_data[GET_AUG_ABILITY(aug)].parent_skill_required);
		}
	}
	sprintf(buf + strlen(buf), "<&yability&0> %s\r\n", buf1);
	
	// applies
	sprintf(buf + strlen(buf), "Attribute applies: <&yapply&0>\r\n");
	for (app = GET_AUG_APPLIES(aug), num = 1; app; app = app->next, ++num) {
		sprintf(buf + strlen(buf), " %2d. %d to %s\r\n", num, app->weight, apply_types[app->location]);
	}
	if (!GET_AUG_APPLIES(aug)) {
		strcat(buf, "  none\r\n");
	}
	
	// resources
	sprintf(buf + strlen(buf), "Resources required: <&yresource&0>\r\n");
	get_resource_display(GET_AUG_RESOURCES(aug), lbuf);
	strcat(buf, lbuf);
	
	page_string(ch->desc, buf, TRUE);
}


 //////////////////////////////////////////////////////////////////////////////
//// OLC MODULES /////////////////////////////////////////////////////////////

OLC_MODULE(augedit_ability) {
	extern int find_ability_by_name(char *name, bool allow_abbrev);
	
	augment_data *aug = GET_OLC_AUGMENT(ch->desc);
	int abil;
	
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
	else if ((abil = find_ability_by_name(argument, TRUE)) == NOTHING) {
		msg_to_char(ch, "Invalid ability '%s'.\r\n", argument);
	}
	else {
		GET_AUG_ABILITY(aug) = abil;
		
		if (PRF_FLAGGED(ch, PRF_NOREPEAT)) {
			send_config_msg(ch, "ok_string");
		}
		else {
			msg_to_char(ch, "It now requires the %s ability.\r\n", ability_data[abil].name);
		}
	}
}


OLC_MODULE(augedit_apply) {
	augment_data *aug = GET_OLC_AUGMENT(ch->desc);
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
			msg_to_char(ch, "Usage: apply add <value> <apply> [type]\r\n");
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


OLC_MODULE(augedit_resource) {
	void olc_process_resources(char_data *ch, char *argument, struct resource_data **list);
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
