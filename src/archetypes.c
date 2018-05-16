/* ************************************************************************
*   File: archetypes.c                                    EmpireMUD 2.0b5 *
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
*   Character Creation
*   OLC Handlers
*   Displays
*   Edit Modules
*/

// local data
#define GEAR_ERROR  -123	// arbitrary number that's not -1/NO_WEAR thru NUM_WEARS
const char *default_archetype_name = "unnamed archetype";
const char *default_archetype_desc = "no description";
const char *default_archetype_rank = "Adventurer";

// local protos
void free_archetype_gear(struct archetype_gear *list);
void get_archetype_gear_display(struct archetype_gear *list, char *save_buffer);

// external consts
extern const char *archetype_flags[];
extern struct archetype_menu_type archetype_menu[];
extern const char *archetype_types[];
extern int attribute_display_order[NUM_ATTRIBUTES];
extern struct attribute_data_type attributes[NUM_ATTRIBUTES];
extern const char *olc_type_bits[NUM_OLC_TYPES+1];
extern const struct wear_data_type wear_data[NUM_WEARS];

// external funcs


 //////////////////////////////////////////////////////////////////////////////
//// HELPERS /////////////////////////////////////////////////////////////////

/**
* Gives a player creation lore if they have an archetype that includes it.
*
* @param char_data *ch The person to give lore to.
*/
void add_archetype_lore(char_data *ch) {
	char temp[MAX_STRING_LENGTH];
	archetype_data *arch;
	char *str;
	int iter;
	
	for (iter = 0; iter < NUM_ARCHETYPE_TYPES; ++iter) {
		if (!(arch = archetype_proto(CREATION_ARCHETYPE(ch, iter)))) {
			continue;
		}
		if (!GET_ARCH_LORE(arch) || !*GET_ARCH_LORE(arch)) {
			continue;
		}
		
		strcpy(temp, GET_ARCH_LORE(arch));
		
		// he/she
		if (strstr(temp, "$e")) {
			str = str_replace("$e", REAL_HSSH(ch), temp);
			strcpy(temp, str);
			free(str);
		}
		// his/her
		if (strstr(temp, "$s")) {
			str = str_replace("$s", REAL_HSHR(ch), temp);
			strcpy(temp, str);
			free(str);
		}
		// him/her
		if (strstr(temp, "$m")) {
			str = str_replace("$m", REAL_HMHR(ch), temp);
			strcpy(temp, str);
			free(str);
		}
		
		add_lore(ch, LORE_CREATED, "%s", CAP(temp));
	}
}


/**
* @param int type Any ARCHT_ const.
* @return bool TRUE if there's at least one live archetype of that type; otherwise FALSE.
*/
bool archetype_exists(int type) {
	archetype_data *arch, *next_arch;
	
	HASH_ITER(hh, archetype_table, arch, next_arch) {
		if (GET_ARCH_TYPE(arch) == type && !ARCHETYPE_FLAGGED(arch, ARCH_IN_DEVELOPMENT)) {
			return TRUE;
		}
	}
	
	return FALSE;
}


/**
* Audits archetypes on startup. Erroring entries are set IN-DEVELOPMENT.
*/
void check_archetypes(void) {
	struct archetype_skill *arsk, *next_arsk;
	archetype_data *arch, *next_arch;
	bool error;
	
	HASH_ITER(hh, archetype_table, arch, next_arch) {
		error = FALSE;
		
		LL_FOREACH_SAFE(GET_ARCH_SKILLS(arch), arsk, next_arsk) {
			if (!find_skill_by_vnum(arsk->skill)) {
				log("- Archetype [%d] %s has invalid skill %d", GET_ARCH_VNUM(arch), GET_ARCH_NAME(arch), arsk->skill);
				error = TRUE;
				LL_DELETE(GET_ARCH_SKILLS(arch), arsk);
				free(arsk);
			}
		}
		
		if (error) {
			SET_BIT(GET_ARCH_FLAGS(arch), ARCH_IN_DEVELOPMENT);
		}
	}
}


/**
* Look up an active archetype by name, preferring exact matches.
*
* @param int type Which ARCHT_ to search.
* @param char *name The archetype name to look up.
*/
archetype_data *find_archetype_by_name(int type, char *name) {
	archetype_data *arch, *next_arch, *partial = NULL;
	
	HASH_ITER(sorted_hh, sorted_archetypes, arch, next_arch) {
		if (type != GET_ARCH_TYPE(arch) || ARCHETYPE_FLAGGED(arch, ARCH_IN_DEVELOPMENT)) {
			continue;
		}
		
		// matches:
		if (!str_cmp(name, GET_ARCH_NAME(arch))) {
			// perfect match
			return arch;
		}
		if (!partial && is_multiword_abbrev(name, GET_ARCH_NAME(arch))) {
			// probable match
			partial = arch;
		}
	}
	
	// no exact match...
	return partial;
}


/**
* Find a matching gear slot, preferring exact matches.
*
* @param char *name The name to look up.
* @return int The slot found (NO_WEAR for inventory, GEAR_ERROR if no match).
*/
int find_gear_slot_by_name(char *name) {
	int iter, partial = GEAR_ERROR;
	
	// special case for giving to inventory
	if (!str_cmp(name, "inventory")) {
		return NO_WEAR;
	}
	else if (is_abbrev(name, "inventory")) {
		partial = NO_WEAR;
	}
	
	for (iter = 0; iter < NUM_WEARS; ++iter) {
		if (!str_cmp(name, wear_data[iter].name)) {
			return iter;	// exact match
		}
		else if (partial == GEAR_ERROR && is_abbrev(name, wear_data[iter].name)) {
			// only want first partial match
			partial = iter;
		}
	}
		
	// no exact match
	return partial;
}


/**
* Copies gear locations that are not already full.
*
* @param struct archetype_gear **list The list to merge copies into.
* @param struct archetype_gear *from The list to copy from.
*/
void smart_copy_gear(struct archetype_gear **list, struct archetype_gear *from) {
	struct archetype_gear *old, *find, *gear, *last;
	int part_count[NUM_WEARS], iter;
	bool found;
	
	// for multi-slot wears
	for (iter = 0; iter < NUM_WEARS; ++iter) {
		part_count[iter] = 0;
	}
	
	// find end
	if ((last = *list)) {
		while (last->next) {
			last = last->next;
		}
	}
	
	for (old = from; old; old = old->next) {
		// only check on non-inventory
		if (old->wear != NO_WEAR) {
			// ensure the slot is not already filled
			found = FALSE;
			for (find = *list; find && !found; find = find->next) {
				if (find->wear != old->wear) {
					continue;
				}
				
				// check if it's cascadable
				if (wear_data[find->wear].cascade_pos != NO_WEAR) {
					if (++part_count[find->wear] > 1) {
						found = TRUE;
					}
				}
				else {
					found = TRUE;
				}
			}
			
			// does the list already have this gear slot? if so, skip
			if (found) {
				continue;
			}
		}
		
		// ok copy it
		CREATE(gear, struct archetype_gear, 1);
		*gear = *old;
		gear->next = NULL;
		
		if (last) {
			last->next = gear;
		}
		else {
			*list = gear;
		}
		last = gear;
	}
}


/**
* Validates the male/female rank names.
*
* @param char_data *ch The player to send errors to.
* @param char *argument The proposed rank name.
* @return TRUE if it's ok, or FALSE if not.
*/
bool valid_default_rank(char_data *ch, char *argument) {
	extern bool valid_rank_name(char_data *ch, char *newname);

	if (color_code_length(argument) > 0 || strchr(argument, '&') != NULL) {
		msg_to_char(ch, "Default ranks may not contain color codes.\r\n");
	}
	else if (strchr(argument, '%')) {
		msg_to_char(ch, "Default ranks may not contain a percent sign (%%).\r\n");
	}
	else if (strchr(argument, '"')) {
		msg_to_char(ch, "Default rank names may not contain a quotation mark (\").\r\n");
	}
	else if (!valid_rank_name(ch, argument)) {
		// sends own message
	}
	else {
		return TRUE;
	}
	
	return FALSE;
}


 //////////////////////////////////////////////////////////////////////////////
//// UTILITIES ///////////////////////////////////////////////////////////////

/**
* OLC processor for archetype gear, which is also used by globals.
*
* @param char_data *ch The player using OLC.
* @param char *argument The arguments the player entered.
* @param struct archetype_gear **list A pointer to a gear list to modify.
*/
void archedit_process_gear(char_data *ch, char *argument, struct archetype_gear **list) {
	char cmd_arg[MAX_INPUT_LENGTH], slot_arg[MAX_INPUT_LENGTH], num_arg[MAX_INPUT_LENGTH], type_arg[MAX_INPUT_LENGTH], val_arg[MAX_INPUT_LENGTH];
	struct archetype_gear *gear, *next_gear, *change, *temp, *copyfrom;
	char buf[MAX_STRING_LENGTH];
	archetype_data *copyarch;
	bitvector_t findtype;
	int loc, num;
	bool found;
	
	argument = any_one_arg(argument, cmd_arg);
	skip_spaces(&argument);
	
	if (is_abbrev(cmd_arg, "remove")) {
		if (!*argument) {
			msg_to_char(ch, "Remove which gear (number)?\r\n");
		}
		else if (!str_cmp(argument, "all")) {
			free_archetype_gear(*list);
			*list = NULL;
			msg_to_char(ch, "You remove all the gear.\r\n");
		}
		else if (!isdigit(*argument) || (num = atoi(argument)) < 1) {
			msg_to_char(ch, "Invalid gear number.\r\n");
		}
		else {
			found = FALSE;
			for (gear = *list; gear && !found; gear = next_gear) {
				next_gear = gear->next;
				if (--num == 0) {
					found = TRUE;
					
					msg_to_char(ch, "You remove %s.\r\n", get_obj_name_by_proto(gear->vnum));
					REMOVE_FROM_LIST(gear, *list, next);
					free(gear);
				}
			}
			
			if (!found) {
				msg_to_char(ch, "Invalid gear number.\r\n");
			}
		}
	}
	else if (is_abbrev(cmd_arg, "add")) {
		argument = any_one_arg(argument, slot_arg);
		argument = any_one_arg(argument, num_arg);
		num = atoi(num_arg);
		loc = NO_WEAR;	// this indicates inventory
		
		if (!*slot_arg || !*num_arg || !isdigit(*num_arg) || num <= 0) {
			msg_to_char(ch, "Usage: gear add <inventory | slot> <vnum>\r\n");
		}
		else if ((loc = find_gear_slot_by_name(slot_arg)) == GEAR_ERROR) {
			msg_to_char(ch, "Invalid gear slot '%s'.\r\n", slot_arg);
		}
		else if (!obj_proto(num)) {
			msg_to_char(ch, "Invalid object vnum %d.\r\n", num);
		}
		else {
			CREATE(gear, struct archetype_gear, 1);
			gear->wear = loc;
			gear->vnum = num;
			
			// append to end
			if ((temp = *list)) {
				while (temp->next) {
					temp = temp->next;
				}
				temp->next = gear;
			}
			else {
				*list = gear;
			}
			
			msg_to_char(ch, "You add %s (%s).\r\n", get_obj_name_by_proto(num), loc == NO_WEAR ? "inventory" : wear_data[loc].name);
		}
	}
	else if (is_abbrev(cmd_arg, "change")) {
		argument = any_one_arg(argument, num_arg);
		argument = any_one_arg(argument, type_arg);
		argument = any_one_arg(argument, val_arg);
		
		if (!*num_arg || !isdigit(*num_arg) || !*type_arg || !*val_arg) {
			msg_to_char(ch, "Usage: gear change <number> <slot | vnum> <new value>\r\n");
			return;
		}
		
		// find which one to change
		if (!isdigit(*num_arg) || (num = atoi(num_arg)) < 1) {
			msg_to_char(ch, "Invalid gear number.\r\n");
			return;
		}
		change = NULL;
		for (gear = *list; gear && !change; gear = gear->next) {
			if (--num == 0) {
				change = gear;
				break;
			}
		}
		if (!change) {
			msg_to_char(ch, "Invalid gear number.\r\n");
		}
		else if (is_abbrev(type_arg, "vnum")) {
			num = atoi(val_arg);
			if (!obj_proto(num)) {
				msg_to_char(ch, "Invalid object vnum '%s'.\r\n", val_arg);
			}
			else {
				change->vnum = num;
				msg_to_char(ch, "Gear %d changed to vnum %d (%s).\r\n", atoi(num_arg), num, get_obj_name_by_proto(num));
			}
		}
		else if (is_abbrev(type_arg, "slot")) {
			if ((loc = find_gear_slot_by_name(val_arg)) == GEAR_ERROR) {
				msg_to_char(ch, "Invalid gear slot '%s'.\r\n", val_arg);
			}
			else {
				change->wear = loc;
				msg_to_char(ch, "Gear %d changed to %s.\r\n", atoi(num_arg), loc == NO_WEAR ? "inventory" : wear_data[loc].name);
			}
		}
		else {
			msg_to_char(ch, "You can only change the slot or vnum.\r\n");
		}
	}
	else if (is_abbrev(cmd_arg, "copy")) {
		argument = any_one_arg(argument, type_arg);
		argument = any_one_arg(argument, num_arg);
		copyfrom = NULL;
		
		if (!*type_arg || !*num_arg) {
			msg_to_char(ch, "Usage: gear copy <from type> <from vnum>\r\n");
		}
		else if ((findtype = find_olc_type(type_arg)) == 0) {
			msg_to_char(ch, "Unknown olc type '%s'.\r\n", type_arg);
		}
		else if (!isdigit(*num_arg)) {
			sprintbit(findtype, olc_type_bits, buf, FALSE);
			msg_to_char(ch, "Copy spawns from which %s?\r\n", buf);
		}
		else if ((num = atoi(num_arg)) < 0) {
			msg_to_char(ch, "Invalid vnum.\r\n");
		}
		else {
			sprintbit(findtype, olc_type_bits, buf, FALSE);
			
			switch (findtype) {
				case OLC_ARCHETYPE: {
					if ((copyarch = archetype_proto(num))) {
						copyfrom = GET_ARCH_GEAR(copyarch);
					}
					break;
				}
				case OLC_GLOBAL: {
					struct global_data *glb;
					if ((glb = global_proto(num))) {
						copyfrom = GET_GLOBAL_GEAR(glb);
					}
					break;
				}
				default: {
					msg_to_char(ch, "You can't copy gear from %ss.\r\n", buf);
					return;
				}
			}
			
			if (!copyfrom) {
				msg_to_char(ch, "Invalid %s vnum '%s'.\r\n", buf, num_arg);
			}
			else {
				smart_copy_gear(list, copyfrom);
				msg_to_char(ch, "Gear copied from %s %d.\r\n", buf, num);
			}
		}
	}
	else {
		msg_to_char(ch, "Usage: gear add <inventory | slot> <obj vnum>\r\n");
		msg_to_char(ch, "       gear copy <from type> <from vnum>\r\n");
		msg_to_char(ch, "       gear change <number> <slot | vnum> <new value>\r\n");
		msg_to_char(ch, "       gear remove <number | all>\r\n");
	}
}


/**
* Checks for common archetype problems and reports them to ch.
*
* @param archetype_data *arch The item to audit.
* @param char_data *ch The person to report to.
* @return bool TRUE if any problems were reported; FALSE if all good.
*/
bool audit_archetype(archetype_data *arch, char_data *ch) {
	int gear_count[NUM_WEARS], iter, pos, total;
	struct archetype_gear *gear;
	struct archetype_skill *sk;
	bool problem = FALSE;
	obj_data *proto;
	
	if (ARCHETYPE_FLAGGED(arch, ARCH_IN_DEVELOPMENT)) {
		olc_audit_msg(ch, GET_ARCH_VNUM(arch), "IN-DEVELOPMENT");
		problem = TRUE;
	}
	if (!GET_ARCH_NAME(arch) || !*GET_ARCH_NAME(arch) || !str_cmp(GET_ARCH_NAME(arch), default_archetype_name)) {
		olc_audit_msg(ch, GET_ARCH_VNUM(arch), "No name set");
		problem = TRUE;
	}
	
	if (strlen(NULLSAFE(GET_ARCH_DESC(arch))) > 60) {	// arbitrary number
		olc_audit_msg(ch, GET_ARCH_VNUM(arch), "Description is long");
		problem = TRUE;
	}
	
	if (GET_ARCH_LORE(arch) && *GET_ARCH_LORE(arch)) {
		for (iter = 0; iter < strlen(GET_ARCH_LORE(arch)); ++iter) {
			if (*(GET_ARCH_LORE(arch) + iter) == '$' && !strchr("esm", *(GET_ARCH_LORE(arch) + iter + 1))) {
				olc_audit_msg(ch, GET_ARCH_VNUM(arch), "Lore contains bad $ code");
				problem = TRUE;
				break;	// report only 1
			}
		}
		
		if (ispunct(*(GET_ARCH_LORE(arch) + strlen(GET_ARCH_LORE(arch)) - 1))) {
			olc_audit_msg(ch, GET_ARCH_VNUM(arch), "Lore ends with punctuation");
			problem = TRUE;
		}
	}
	
	// check for overloaded gear slots
	for (iter = 0; iter < NUM_WEARS; ++iter) {
		gear_count[iter] = 0;
	}
	for (gear = GET_ARCH_GEAR(arch); gear; gear = gear->next) {
		// only check on non-inventory
		if (gear->wear != NO_WEAR) {
			pos = gear->wear;
			
			// check item matches pos
			if (!(proto = obj_proto(gear->vnum))) {
				olc_audit_msg(ch, GET_ARCH_VNUM(arch), "Missing gear vnum %d", gear->vnum);
				problem = TRUE;
			}
			else if (!CAN_WEAR(proto, wear_data[pos].item_wear)) {
				olc_audit_msg(ch, GET_ARCH_VNUM(arch), "Item %s can't be worn on %s", GET_OBJ_SHORT_DESC(proto), wear_data[pos].name);
				problem = TRUE;
			}
			
			while (gear_count[pos] > 0 && wear_data[pos].cascade_pos != NO_WEAR) {
				pos = wear_data[pos].cascade_pos;
			}
			++gear_count[pos];
		}
	}
	for (iter = 0; iter < NUM_WEARS; ++iter) {
		if (gear_count[iter] > 1) {
			olc_audit_msg(ch, GET_ARCH_VNUM(arch), "Duplicate gear for %s slot", wear_data[iter].name);
			problem = TRUE;
		}
	}
	
	// attributes
	total = 0;
	for (iter = 0; iter < NUM_ATTRIBUTES; ++iter) {
		total += GET_ARCH_ATTRIBUTE(arch, iter);
		if (GET_ARCH_ATTRIBUTE(arch, iter) == 0) {
			olc_audit_msg(ch, GET_ARCH_VNUM(arch), "%s is 0", attributes[iter].name);
			problem = TRUE;
		}
	}
	if (total != 11) {	// this number is arbitrary
		olc_audit_msg(ch, GET_ARCH_VNUM(arch), "Attributes total %d", total);
		problem = TRUE;
	}
	
	// skills
	total = 0;
	for (sk = GET_ARCH_SKILLS(arch); sk; sk = sk->next) {
		total += sk->level;
	}
	if (total != 30) {	// this number is arbitrary
		olc_audit_msg(ch, GET_ARCH_VNUM(arch), "Skill points total %d", total);
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
		int iter, atts = 0, skills = 0;
		char buf[MAX_STRING_LENGTH];
		struct archetype_skill *sk;
		
		// count attribute points
		for (iter = 0; iter < NUM_ATTRIBUTES; ++iter) {
			atts += GET_ARCH_ATTRIBUTE(arch, iter);
		}
		
		// build skill string and count
		strcpy(buf, " (");
		for (sk = GET_ARCH_SKILLS(arch); sk; sk = sk->next) {
			skills += sk->level;
			sprintf(buf + strlen(buf), "%s%s", (sk == GET_ARCH_SKILLS(arch)) ? "" : ", ", get_skill_abbrev_by_vnum(sk->skill));
		}
		strcat(buf, ")");
		
		snprintf(output, sizeof(output), "[%5d] %s: %s%s [%d/%d]", GET_ARCH_VNUM(arch), archetype_types[GET_ARCH_TYPE(arch)], GET_ARCH_NAME(arch), GET_ARCH_SKILLS(arch) ? buf : "", atts, skills);
	}
	else {
		snprintf(output, sizeof(output), "[%5d] %s: %s", GET_ARCH_VNUM(arch), archetype_types[GET_ARCH_TYPE(arch)], GET_ARCH_NAME(arch));
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
	if (GET_ARCH_TYPE(a) != GET_ARCH_TYPE(b)) {
		return GET_ARCH_TYPE(a) - GET_ARCH_TYPE(b);
	}
	else {
		return strcmp(NULLSAFE(GET_ARCH_NAME(a)), NULLSAFE(GET_ARCH_NAME(b)));
	}
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
		GET_ARCH_ATTRIBUTE(arch, iter) = 0;
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
* Parse a 'G' gear tag.
*
* @param FILE *fl A file open for reading, having just read the 'G' line.
* @param struct archetype_gear **list The list to save the result to.
* @param char *error The string describing the item being read, in case something goes wrong.
*/
void parse_archetype_gear(FILE *fl, struct archetype_gear **list, char *error) {
	struct archetype_gear *gear, *last;
	char line[256];
	int int_in[2];

	if (!get_line(fl, line) || sscanf(line, "%d %d", &int_in[0], &int_in[1]) != 2) {
		log("SYSERR: format error in G line of %s", error);
		exit(1);
	}
	
	// only accept valid positions
	if (int_in[0] >= NO_WEAR && int_in[0] < NUM_WEARS) {
		CREATE(gear, struct archetype_gear, 1);
		gear->wear = int_in[0];
		gear->vnum = int_in[1];
	
		// append
		if ((last = *list)) {
			while (last->next) {
				last = last->next;
			}
			last->next = gear;
		}
		else {
			*list = gear;
		}
	}
}


/**
* Read one archetype from file.
*
* @param FILE *fl The open .arch file
* @param any_vnum vnum The archetype vnum
*/
void parse_archetype(FILE *fl, any_vnum vnum) {
	char line[256], error[256], str_in[256];
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
	
	// lines 1-5: name, desc, lore, male rank, female rank
	GET_ARCH_NAME(arch) = fread_string(fl, error);
	GET_ARCH_DESC(arch) = fread_string(fl, error);
	GET_ARCH_LORE(arch) = fread_string(fl, error);
	GET_ARCH_MALE_RANK(arch) = fread_string(fl, error);
	GET_ARCH_FEMALE_RANK(arch) = fread_string(fl, error);
	
	// line 6: type flags 
	// NOTE: prior to b4.33 this line was only 'flags'
	if (!get_line(fl, line)) {
		log("SYSERR: Format error: missing line 5 of %s", error);
		exit(1);
	}
	else if (sscanf(line, "%d %s", &int_in[0], str_in) == 2) {
		GET_ARCH_TYPE(arch) = int_in[0];
		GET_ARCH_FLAGS(arch) = asciiflag_conv(str_in);
	}
	else if (sscanf(line, "%s", str_in) == 1) {
		GET_ARCH_TYPE(arch) = ARCHT_ORIGIN;
		GET_ARCH_FLAGS(arch) = asciiflag_conv(str_in);
	}
	else {
		log("SYSERR: Format error in line 5 of %s", error);
		exit(1);
	}
		
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
				parse_archetype_gear(fl, &GET_ARCH_GEAR(arch), error);
				break;
			}
			case 'K': {	// skill: number level
				if (!get_line(fl, line) || sscanf(line, "%d %d", &int_in[0], &int_in[1]) != 2) {
					log("SYSERR: format error in K line of %s", error);
					exit(1);
				}
				
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
	
	last = NO_WEAR;
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
* Writes the 'G' tag with archetype gear to an open file.
*
* @param FILE *fl The open file to write to.
* @param struct archetype_gear *list The list to save.
*/
void write_archetype_gear_to_file(FILE *fl, struct archetype_gear *list) {
	struct archetype_gear *gear;
	
	// G: gear
	for (gear = list; gear; gear = gear->next) {
		fprintf(fl, "G\n%d %d  # %s\n", gear->wear, gear->vnum, get_obj_name_by_proto(gear->vnum));
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
	struct archetype_skill *sk;
	int iter;
	
	if (!fl || !arch) {
		syslog(SYS_ERROR, LVL_START_IMM, TRUE, "SYSERR: write_archetype_to_file called without %s", !fl ? "file" : "archetype");
		return;
	}
	
	fprintf(fl, "#%d\n", GET_ARCH_VNUM(arch));
	
	// 1-5. strings
	fprintf(fl, "%s~\n", NULLSAFE(GET_ARCH_NAME(arch)));
	fprintf(fl, "%s~\n", NULLSAFE(GET_ARCH_DESC(arch)));
	fprintf(fl, "%s~\n", NULLSAFE(GET_ARCH_LORE(arch)));
	fprintf(fl, "%s~\n", NULLSAFE(GET_ARCH_MALE_RANK(arch)));
	fprintf(fl, "%s~\n", NULLSAFE(GET_ARCH_FEMALE_RANK(arch)));
	
	// 6. flags
	fprintf(fl, "%d %s\n", GET_ARCH_TYPE(arch), bitv_to_alpha(GET_ARCH_FLAGS(arch)));
	
	// 'A': attributes
	for (iter = 0; iter < NUM_ATTRIBUTES; ++iter) {
		if (GET_ARCH_ATTRIBUTE(arch, iter) != 0) {
			fprintf(fl, "A\n%d %d\n", iter, GET_ARCH_ATTRIBUTE(arch, iter));
		}
	}
	
	// 'G': gear
	write_archetype_gear_to_file(fl, GET_ARCH_GEAR(arch));
	
	// K: skills
	for (sk = GET_ARCH_SKILLS(arch); sk; sk = sk->next) {
		fprintf(fl, "K\n%d %d\n", sk->skill, sk->level);
	}
	
	// end
	fprintf(fl, "S\n");
}


 //////////////////////////////////////////////////////////////////////////////
//// CHARACTER CREATION //////////////////////////////////////////////////////

/**
* Detailed info on an archetype.
*
* @param descriptor_data *desc The user to send it to.
* @param archetype_data *arch The archetype to show.
*/
void display_archetype_info(descriptor_data *desc, archetype_data *arch) {
	struct archetype_skill *sk;
	skill_data *skill;
	bool show;
	int iter;
	
	msg_to_desc(desc, "[\tc%s\t0] - %s\r\n", GET_ARCH_NAME(arch), GET_ARCH_DESC(arch));
	
	// check for attributes to show
	show = FALSE;
	for (iter = 0; iter < NUM_ATTRIBUTES; ++iter) {
		if (GET_ARCH_ATTRIBUTE(arch, iter) != 0) {
			show = TRUE;
			break;
		}
	}
	if (show) {
		msg_to_desc(desc, "\tyAttributes\t0:\r\n");
		for (iter = 0; iter < NUM_ATTRIBUTES; ++iter) {
			msg_to_desc(desc, " %s: %s%d\t0 (%s)\r\n", attributes[iter].name, HAPPY_COLOR(GET_ARCH_ATTRIBUTE(arch, iter), 2), GET_ARCH_ATTRIBUTE(arch, iter), attributes[iter].creation_description);
		}
	}
	
	// check for skills to show
	show = FALSE;
	for (sk = GET_ARCH_SKILLS(arch); sk; sk = sk->next) {
		if (sk->level != 0) {
			show = TRUE;
			break;
		}
	}
	if (show) {
		msg_to_desc(desc, "\tySkills\t0:\r\n");
		for (sk = GET_ARCH_SKILLS(arch); sk; sk = sk->next) {
			if ((skill = find_skill_by_vnum(sk->skill))) {
				msg_to_desc(desc, " %s: \tg%d\t0 (%s)\r\n", SKILL_NAME(skill), sk->level, SKILL_DESC(skill));
			}
		}
	}
}


/**
* List archetypes, or search.
*
* @param descriptor_data *desc The user to send it to.
* @param int type Which ARCH_ to display.
* @param char *argument All, basic, or search string.
*/
void display_archetype_list(descriptor_data *desc, int type, char *argument) {
	char buf[MAX_STRING_LENGTH], line[256];
	archetype_data *arch, *next_arch;
	bool basic = FALSE, all = FALSE;
	struct archetype_skill *sk;
	bool skill_match, any;
	size_t size;
	
	if (!*argument) {
		msg_to_desc(desc, "Usage: list <all | basic | keywords>\r\n");
		return;
	}
	else if (!str_cmp(argument, "basic")) {
		basic = TRUE;
	}
	else if (!str_cmp(argument, "all")) {
		all = TRUE;
	}
	
	size = 0;
	*buf = '\0';
	any = FALSE;
	
	HASH_ITER(sorted_hh, sorted_archetypes, arch, next_arch) {
		if (ARCHETYPE_FLAGGED(arch, ARCH_IN_DEVELOPMENT)) {
			continue;	// don't show in-dev
		}
		if (GET_ARCH_TYPE(arch) != type) {
			continue;
		}
		if (basic && !ARCHETYPE_FLAGGED(arch, ARCH_BASIC)) {
			continue;
		}
		
		// check skill match
		skill_match = FALSE;
		for (sk = GET_ARCH_SKILLS(arch); sk && !skill_match; sk = sk->next) {
			if (multi_isname(argument, get_skill_name_by_vnum(sk->skill))) {
				skill_match = TRUE;
			}
		}
		
		// match strings
		if (all || basic || skill_match || multi_isname(argument, GET_ARCH_NAME(arch)) || multi_isname(argument, GET_ARCH_DESC(arch))) {
			snprintf(line, sizeof(line), " %s%s\t0 - %s", ARCHETYPE_FLAGGED(arch, ARCH_BASIC) ? "\tc" : "\ty", GET_ARCH_NAME(arch), GET_ARCH_DESC(arch));
			any = TRUE;
			
			if (size + strlen(line) + 40 < sizeof(buf)) {
				size += snprintf(buf + size, sizeof(buf) - size, "%s\r\n", line);
			}
			else {
				size += snprintf(buf + size, sizeof(buf) - size, " ... and more\r\n");
				break;
			}
		}
	}
	
	if (!any) {
		size += snprintf(buf + size, sizeof(buf) - size, " There are no archetypes %s.\r\n", (all || basic) ? "available to list" : "with those keywords");
	}
	
	if (*buf) {
		page_string(desc, buf, TRUE);
	}
}


/**
* Prompt character for an archetype.
*
* @param descriptor_data *desc The user to send it to.
* @param int type_pos A position in the archetype_menu[] array.
*/
void display_archetype_menu(descriptor_data *desc, int type_pos) {
	msg_to_desc(desc, "\tcChoose your \%s\t0:\r\n%s", archetype_menu[type_pos].name, archetype_menu[type_pos].description);
	msg_to_desc(desc, "[ HINT: These are only your starting traits; you can still learn any skill ]\r\n");
	// msg_to_desc(desc, "Choose your %s (type its name), 'info <name>' for more information,\r\n", archetype_menu[type_pos].name);
	// msg_to_desc(desc, "or type 'list' for more options:\r\n");
	
	display_archetype_list(desc, archetype_menu[type_pos].type, "basic");
}


/**
* Process input at the archetype menu.
*
* @param descriptor_data *desc The user.
* @param char *argument What they typed.
*/
void parse_archetype_menu(descriptor_data *desc, char *argument) {
	void next_creation_step(descriptor_data *d);

	char arg1[MAX_INPUT_LENGTH], *arg2;
	archetype_data *arch;
	int pos;	// which submenu
	
	// setup/safety: which ARCHT_ we're on
	pos = SUBMENU(desc);
	if (pos > NUM_ARCHETYPE_TYPES || archetype_menu[pos].type == NOTHING) {
		// done!
		next_creation_step(desc);
		return;
	}
	else if (!archetype_exists(archetype_menu[pos].type)) {
		++SUBMENU(desc);
		parse_archetype_menu(desc, "");
		return;
	}
	
	// prepare to parse
	skip_spaces(&argument);
	arg2 = any_one_arg(argument, arg1);
	skip_spaces(&arg2);
	
	msg_to_desc(desc, "\r\n");
	
	if (!*arg1) {
		display_archetype_menu(desc, pos);
	}
	else if (is_abbrev(arg1, "info")) {
		if (!*arg2) {
			msg_to_desc(desc, "Usage: info <archetype name>\r\n");
		}
		else if (!(arch = find_archetype_by_name(archetype_menu[pos].type, arg2))) {
			msg_to_desc(desc, "Unknown %s '%s'.\r\n", archetype_menu[pos].name, argument);
		}
		else {
			display_archetype_info(desc, arch);
		}
	}
	else if (is_abbrev(arg1, "list")) {
		display_archetype_list(desc, archetype_menu[pos].type, arg2);
	}
	else {	// picking one
		if (!(arch = find_archetype_by_name(archetype_menu[pos].type, argument))) {
			msg_to_desc(desc, "Unknown %s '%s'. Try 'list' for more options.\r\n", archetype_menu[pos].name, argument);
		}
		else {
			// success!
			CREATION_ARCHETYPE(desc->character, archetype_menu[pos].type) = GET_ARCH_VNUM(arch);
			SET_BIT(PLR_FLAGS(desc->character), PLR_NEEDS_NEWBIE_SETUP);
			
			// on to the next archetype
			++SUBMENU(desc);
			parse_archetype_menu(desc, "");
			return;
		}
	}
	
	// still here? add a prompt
	if (STATE(desc) == CON_Q_ARCHETYPE) {
		msg_to_desc(desc, "\r\nType \tcinfo\t0, \tclist\t0, or %s %s name > ", AN(archetype_menu[pos].name), archetype_menu[pos].name);
	}
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
	struct archetype_skill *sk;
	int iter, pos, total;
	size_t size;
	
	if (!arch) {
		return;
	}
	
	// first line
	size = snprintf(buf, sizeof(buf), "VNum: [\tc%d\t0], Type: \ty%s\t0, Name: \tc%s\t0\r\n", GET_ARCH_VNUM(arch), archetype_types[GET_ARCH_TYPE(arch)], GET_ARCH_NAME(arch));
	size += snprintf(buf + size, sizeof(buf) - size, "Ranks: [\ta%s\t0/\tp%s\t0]\r\n", GET_ARCH_MALE_RANK(arch), GET_ARCH_FEMALE_RANK(arch));
	
	size += snprintf(buf + size, sizeof(buf) - size, "Description: %s\r\n", GET_ARCH_DESC(arch));
	
	if (GET_ARCH_LORE(arch) && *GET_ARCH_LORE(arch)) {
		size += snprintf(buf + size, sizeof(buf) - size, "Lore: \tc%s\t0 [on Month Day, Year.]\r\n", GET_ARCH_LORE(arch));
	}
	else {
		size += snprintf(buf + size, sizeof(buf) - size, "Lore: \tcnone\t0\r\n");
	}
	
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
		size += snprintf(buf + size, sizeof(buf) - size, "  %s: \tg%d\t0\r\n", get_skill_name_by_vnum(sk->skill), sk->level);
	}
	
	// gear
	size += snprintf(buf + size, sizeof(buf) - size, "Gear:\r\n");
	get_archetype_gear_display(GET_ARCH_GEAR(arch), part);
	size += snprintf(buf + size, sizeof(buf) - size, "%s", part);
	
	page_string(ch->desc, buf, TRUE);
}


/**
* Displays the archetype-gear data from a given list.
*
* @param struct archetype_gear *list Pointer to the start of a list of gear.
* @param char *save_buffer A buffer to store the result to.
*/
void get_archetype_gear_display(struct archetype_gear *list, char *save_buffer) {
	struct archetype_gear *gear;
	int num;
	*save_buffer = '\0';
	for (gear = list, num = 1; gear; gear = gear->next, ++num) {
		sprintf(save_buffer + strlen(save_buffer), " %2d. %s: [%d] %s\r\n", num, gear->wear == NO_WEAR ? "inventory" : wear_data[gear->wear].name, gear->vnum, get_obj_name_by_proto(gear->vnum));
	}
	if (!list) {
		sprintf(save_buffer + strlen(save_buffer), "  none\r\n");
	}
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
	struct archetype_skill *sk;
	int iter, pos, total;
	
	if (!arch) {
		return;
	}
	
	*buf = '\0';
	
	sprintf(buf + strlen(buf), "[%s%d\t0] %s%s\t0\r\n", OLC_LABEL_CHANGED, GET_OLC_VNUM(ch->desc), OLC_LABEL_UNCHANGED, !archetype_proto(GET_ARCH_VNUM(arch)) ? "new archetype" : GET_ARCH_NAME(archetype_proto(GET_ARCH_VNUM(arch))));
	sprintf(buf + strlen(buf), "<%stype\t0> %s\r\n", OLC_LABEL_VAL(GET_ARCH_TYPE(arch), 0), archetype_types[GET_ARCH_TYPE(arch)]);
	sprintf(buf + strlen(buf), "<%sname\t0> %s\r\n", OLC_LABEL_STR(GET_ARCH_NAME(arch), default_archetype_name), NULLSAFE(GET_ARCH_NAME(arch)));
	sprintf(buf + strlen(buf), "<%sdescription\t0> %s\r\n", OLC_LABEL_STR(GET_ARCH_DESC(arch), default_archetype_desc), NULLSAFE(GET_ARCH_DESC(arch)));
	sprintf(buf + strlen(buf), "<%slore\t0> %s [on Month Day, Year.]\r\n", OLC_LABEL_STR(GET_ARCH_LORE(arch), ""), (GET_ARCH_LORE(arch) && *GET_ARCH_LORE(arch)) ? GET_ARCH_LORE(arch) : "none");
	
	sprintbit(GET_ARCH_FLAGS(arch), archetype_flags, lbuf, TRUE);
	sprintf(buf + strlen(buf), "<%sflags\t0> %s\r\n", OLC_LABEL_VAL(GET_ARCH_FLAGS(arch), ARCH_IN_DEVELOPMENT), lbuf);
	
	sprintf(buf + strlen(buf), "<%smalerank\t0> %s\r\n", OLC_LABEL_STR(GET_ARCH_MALE_RANK(arch), default_archetype_rank), NULLSAFE(GET_ARCH_MALE_RANK(arch)));
	sprintf(buf + strlen(buf), "<%sfemalerank\t0> %s\r\n", OLC_LABEL_STR(GET_ARCH_FEMALE_RANK(arch), default_archetype_rank), NULLSAFE(GET_ARCH_FEMALE_RANK(arch)));
	
	// attributes
	total = 0;
	for (iter = 0; iter < NUM_ATTRIBUTES; ++iter) {
		total += GET_ARCH_ATTRIBUTE(arch, iter);
	}
	sprintf(buf + strlen(buf), "Attributes: <%sattribute\t0> (%d total attributes)\r\n", OLC_LABEL_UNCHANGED, total);
	for (iter = 0; iter < NUM_ATTRIBUTES; ++iter) {
		pos = attribute_display_order[iter];
		sprintf(lbuf, "%s%s\t0  [%2d]", OLC_LABEL_VAL(GET_ARCH_ATTRIBUTE(arch, pos), 0), attributes[pos].name, GET_ARCH_ATTRIBUTE(arch, pos));
		sprintf(buf + strlen(buf), "  %-27.27s%s", lbuf, !((iter + 1) % 3) ? "\r\n" : "");
	}
	if (iter % 3) {
		strcat(buf, "\r\n");
	}
	
	// skills
	total = 0;
	for (sk = GET_ARCH_SKILLS(arch); sk; sk = sk->next) {
		total += sk->level;
	}
	sprintf(buf + strlen(buf), "Starting skills: <%sstartingskill\t0> (%d total skill points)\r\n", OLC_LABEL_PTR(GET_ARCH_SKILLS(arch)), total);
	for (sk = GET_ARCH_SKILLS(arch); sk; sk = sk->next) {
		sprintf(buf + strlen(buf), "  %s: %d\r\n", get_skill_name_by_vnum(sk->skill), sk->level);
	}
	
	// gear
	sprintf(buf + strlen(buf), "Gear: <%sgear\t0>\r\n", OLC_LABEL_PTR(GET_ARCH_GEAR(arch)));
	if (GET_ARCH_GEAR(arch)) {
		get_archetype_gear_display(GET_ARCH_GEAR(arch), lbuf);
		strcat(buf, lbuf);
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

OLC_MODULE(archedit_attribute) {
	extern int get_attribute_by_name(char *name);

	archetype_data *arch = GET_OLC_ARCHETYPE(ch->desc), *copyfrom;
	char att_arg[MAX_INPUT_LENGTH], num_arg[MAX_INPUT_LENGTH];
	int att, num;
	
	argument = any_one_arg(argument, att_arg);
	argument = any_one_arg(argument, num_arg);
	
	if (!*att_arg || !*num_arg || !is_number(num_arg)) {
		msg_to_char(ch, "Usage: attribute <type> <number>\r\n");
		msg_to_char(ch, "       attribute <copy> <archetype vnum>\r\n");
	}
	else if (!str_cmp(att_arg, "copy")) {
		if (!(copyfrom = archetype_proto(atoi(num_arg)))) {
			msg_to_char(ch, "Invalid archetype vnum '%s'.\r\n", num_arg);
			return;
		}
		
		for (att = 0; att < NUM_ATTRIBUTES; ++att) {
			GET_ARCH_ATTRIBUTE(arch, att) = GET_ARCH_ATTRIBUTE(copyfrom, att);
		}
		
		msg_to_char(ch, "Attributes copied from archetype [%d] %s.\r\n", GET_ARCH_VNUM(copyfrom), GET_ARCH_NAME(copyfrom));
	}
	// not copying -- add a new one
	else if ((att = get_attribute_by_name(att_arg)) == -1) {
		msg_to_char(ch, "Unknown attribute '%s'.\r\n", att_arg);
	}
	else if ((num = atoi(num_arg)) < (-1 * config_get_int("max_player_attribute")) || num > config_get_int("max_player_attribute")) {
		msg_to_char(ch, "You must choose a number between -%d and %d.\r\n", config_get_int("max_player_attribute"), config_get_int("max_player_attribute"));
	}
	else {
		GET_ARCH_ATTRIBUTE(arch, att) = num;
		
		if (PRF_FLAGGED(ch, PRF_NOREPEAT)) {
			send_config_msg(ch, "ok_string");
		}
		else {
			msg_to_char(ch, "You set the %s to %d.\r\n", attributes[att].name, num);
		}
	}
}


OLC_MODULE(archedit_description) {
	archetype_data *arch = GET_OLC_ARCHETYPE(ch->desc);
	olc_process_string(ch, argument, "description", &GET_ARCH_DESC(arch));
}


OLC_MODULE(archedit_femalerank) {
	archetype_data *arch = GET_OLC_ARCHETYPE(ch->desc);
	
	// valid_default_rank sends its own errors
	if (valid_default_rank(ch, argument)) {
		olc_process_string(ch, argument, "female rank", &GET_ARCH_FEMALE_RANK(arch));
	}
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


OLC_MODULE(archedit_gear) {
	archetype_data *arch = GET_OLC_ARCHETYPE(ch->desc);
	archedit_process_gear(ch, argument, &GET_ARCH_GEAR(arch));
}


OLC_MODULE(archedit_lore) {
	archetype_data *arch = GET_OLC_ARCHETYPE(ch->desc);
	
	if (!str_cmp(argument, "none")) {
		if (GET_ARCH_LORE(arch)) {
			free(GET_ARCH_LORE(arch));
			GET_ARCH_LORE(arch) = NULL;
		}
		msg_to_char(ch, "It now adds no lore.\r\n");
	}
	else {
		delete_doubledollar(argument);
		olc_process_string(ch, argument, "lore", &GET_ARCH_LORE(arch));
	}
}


OLC_MODULE(archedit_malerank) {
	archetype_data *arch = GET_OLC_ARCHETYPE(ch->desc);
	
	// valid_default_rank sends its own errors
	if (valid_default_rank(ch, argument)) {
		olc_process_string(ch, argument, "male rank", &GET_ARCH_MALE_RANK(arch));
	}
}


OLC_MODULE(archedit_name) {
	archetype_data *arch = GET_OLC_ARCHETYPE(ch->desc);
	olc_process_string(ch, argument, "name", &GET_ARCH_NAME(arch));
}


OLC_MODULE(archedit_skill) {	
	archetype_data *arch = GET_OLC_ARCHETYPE(ch->desc);
	char cmd_arg[MAX_INPUT_LENGTH], skill_arg[MAX_INPUT_LENGTH], num_arg[MAX_INPUT_LENGTH];
	struct archetype_skill *sk, *next_sk, *temp;
	skill_data *skill;
	bool found;
	int num;
	
	argument = any_one_arg(argument, cmd_arg);
	argument = any_one_word(argument, skill_arg);
	argument = any_one_arg(argument, num_arg);
	
	if (!*cmd_arg || !*skill_arg) {
		msg_to_char(ch, "Usage: skill add <skill> <level>\r\n");
		msg_to_char(ch, "       skill change <skill> <level>\r\n");
		msg_to_char(ch, "       skill remove <skill>\r\n");
	}
	else if (!(skill = find_skill(skill_arg))) {
		msg_to_char(ch, "Unknown skill '%s'.\r\n", skill_arg);
	}
	else if (is_abbrev(cmd_arg, "add") || is_abbrev(cmd_arg, "change")) {
		// add and change are actually the same
		if (!*num_arg || !isdigit(*num_arg) || (num = atoi(num_arg)) < 0 || num > CLASS_SKILL_CAP) {
			msg_to_char(ch, "Invalid skill level '%s'.\r\n", num_arg);
			return;
		}
		
		// attempt to find an existing entry
		found = FALSE;
		for (sk = GET_ARCH_SKILLS(arch); sk; sk = next_sk) {
			next_sk = sk->next;
			if (sk->skill != SKILL_VNUM(skill)) {
				continue;
			}
			
			// found it!
			found = TRUE;
			sk->level = num;
			
			if (num == 0) {
				REMOVE_FROM_LIST(sk, GET_ARCH_SKILLS(arch), next);
				free(sk);
			}
		}
		
		if (!found && num > 0) {
			CREATE(sk, struct archetype_skill, 1);
			sk->skill = SKILL_VNUM(skill);
			sk->level = num;
			
			// append
			if ((temp = GET_ARCH_SKILLS(arch))) {
				while (temp->next) {
					temp = temp->next;
				}
				temp->next = sk;
			}
			else {
				GET_ARCH_SKILLS(arch) = sk;
			}
		}
		
		if (PRF_FLAGGED(ch, PRF_NOREPEAT)) {
			send_config_msg(ch, "ok_string");
		}
		else {
			msg_to_char(ch, "You set the starting %s level to %d.\r\n", SKILL_NAME(skill), num);
		}
	}
	else if (is_abbrev(cmd_arg, "remove")) {
		// attempt to find an entries
		found = FALSE;
		for (sk = GET_ARCH_SKILLS(arch); sk; sk = next_sk) {
			next_sk = sk->next;
			if (sk->skill != SKILL_VNUM(skill)) {
				continue;
			}
			
			// found it!
			REMOVE_FROM_LIST(sk, GET_ARCH_SKILLS(arch), next);
			free(sk);
		}
		
		if (PRF_FLAGGED(ch, PRF_NOREPEAT)) {
			send_config_msg(ch, "ok_string");
		}
		else {
			msg_to_char(ch, "The archetype will grant no starting %s level.\r\n", SKILL_NAME(skill));
		}
	}
	else {
		msg_to_char(ch, "Valid commands are: add, change, remove.\r\n");
	}
}


OLC_MODULE(archedit_type) {
	archetype_data *arch = GET_OLC_ARCHETYPE(ch->desc);
	GET_ARCH_TYPE(arch) = olc_process_type(ch, argument, "type", "type", archetype_types, GET_ARCH_TYPE(arch));
}
