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
#include "dg_scripts.h"
#include "vnums.h"


/**
* Contents:
*   Helpers
*   Utilities
*   Database
*   OLC Handlers
*   Displays
*   Generic OLC Modules
*   Action OLC Modules
*   Liquid OLC Modules
*/

// local data
const char *default_generic_name = "Unnamed Generic";
#define MAX_LIQUID_COND  (MAX_CONDITION / 15)	// approximate game hours of max cond

// external consts
extern const char *generic_flags[];
extern const char *generic_types[];

// external funcs
extern bool remove_thing_from_resource_list(struct resource_data **list, int type, any_vnum vnum);


 //////////////////////////////////////////////////////////////////////////////
//// HELPERS /////////////////////////////////////////////////////////////////

/**
* Gets a generic's name by vnum, safely.
*
* @param any_vnum vnum The generic vnum to look up.
* @return const char* The name, or UNKNOWN if none.
*/
const char *get_generic_name_by_vnum(any_vnum vnum) {
	generic_data *gen = find_generic_by_vnum(vnum);
	
	if (!gen) {
		return "UNKNOWN";	// sanity
	}
	else {
		return GEN_NAME(gen);
	}
}


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
		switch (GEN_TYPE(gen)) {
			case GENERIC_COOLDOWN: {
				snprintf(output, sizeof(output), "[%5d] %s (%s): %s", GEN_VNUM(gen), GEN_NAME(gen), generic_types[GEN_TYPE(gen)], NULLSAFE(GET_COOLDOWN_WEAR_OFF(gen)));
				break;
			}
			default: {
				snprintf(output, sizeof(output), "[%5d] %s (%s)", GEN_VNUM(gen), GEN_NAME(gen), generic_types[GEN_TYPE(gen)]);
				break;
			}
		}
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
	craft_data *craft, *next_craft;
	augment_data *aug, *next_aug;
	vehicle_data *veh, *next_veh;
	struct resource_data *res;
	bld_data *bld, *next_bld;
	obj_data *obj, *next_obj;
	int size, found;
	bool any;
	
	if (!gen) {
		msg_to_char(ch, "There is no generic %d.\r\n", vnum);
		return;
	}
	
	found = 0;
	size = snprintf(buf, sizeof(buf), "Occurrences of generic %d (%s):\r\n", vnum, GEN_NAME(gen));
	
	// augments
	HASH_ITER(hh, augment_table, aug, next_aug) {
		any = FALSE;
		for (res = GET_AUG_RESOURCES(aug); res && !any; res = res->next) {
			if (res->vnum == vnum && ((GEN_TYPE(gen) == GENERIC_ACTION && res->type == RES_ACTION) || (GEN_TYPE(gen) == GENERIC_LIQUID && res->type == RES_LIQUID))) {
				any = TRUE;
				++found;
				size += snprintf(buf + size, sizeof(buf) - size, "AUG [%5d] %s\r\n", GET_AUG_VNUM(aug), GET_AUG_NAME(aug));
			}
		}
	}
	
	// buildings
	HASH_ITER(hh, building_table, bld, next_bld) {
		any = FALSE;
		for (res = GET_BLD_YEARLY_MAINTENANCE(bld); res && !any; res = res->next) {
			if (res->vnum == vnum && ((GEN_TYPE(gen) == GENERIC_ACTION && res->type == RES_ACTION) || (GEN_TYPE(gen) == GENERIC_LIQUID && res->type == RES_LIQUID))) {
				any = TRUE;
				++found;
				size += snprintf(buf + size, sizeof(buf) - size, "BLD [%5d] %s\r\n", GET_BLD_VNUM(bld), GET_BLD_NAME(bld));
			}
		}
	}
	
	// crafts
	HASH_ITER(hh, craft_table, craft, next_craft) {
		any = FALSE;
		if (CRAFT_FLAGGED(craft, CRAFT_SOUP) && GET_CRAFT_OBJECT(craft) == vnum) {
			any = TRUE;
			++found;
			size += snprintf(buf + size, sizeof(buf) - size, "CFT [%5d] %s\r\n", GET_CRAFT_VNUM(craft), GET_CRAFT_NAME(craft));
		}
		for (res = GET_CRAFT_RESOURCES(craft); res && !any; res = res->next) {
			if (res->vnum == vnum && ((GEN_TYPE(gen) == GENERIC_ACTION && res->type == RES_ACTION) || (GEN_TYPE(gen) == GENERIC_LIQUID && res->type == RES_LIQUID))) {
				any = TRUE;
				++found;
				size += snprintf(buf + size, sizeof(buf) - size, "CFT [%5d] %s\r\n", GET_CRAFT_VNUM(craft), GET_CRAFT_NAME(craft));
			}
		}
	}
	
	// objects
	HASH_ITER(hh, object_table, obj, next_obj) {
		if (GEN_TYPE(gen) == GENERIC_LIQUID && IS_DRINK_CONTAINER(obj) && GET_DRINK_CONTAINER_TYPE(obj) == vnum) {
			++found;
			size += snprintf(buf + size, sizeof(buf) - size, "OBJ [%5d] %s\r\n", GET_OBJ_VNUM(obj), GET_OBJ_SHORT_DESC(obj));
		}
	}
	
	// vehicles
	HASH_ITER(hh, vehicle_table, veh, next_veh) {
		any = FALSE;
		for (res = VEH_YEARLY_MAINTENANCE(veh); res && !any; res = res->next) {
			if (res->vnum == vnum && ((GEN_TYPE(gen) == GENERIC_ACTION && res->type == RES_ACTION) || (GEN_TYPE(gen) == GENERIC_LIQUID && res->type == RES_LIQUID))) {
				any = TRUE;
				++found;
				size += snprintf(buf + size, sizeof(buf) - size, "VEH [%5d] %s\r\n", VEH_VNUM(veh), VEH_SHORT_DESC(veh));
			}
		}
	}
	
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
	void complete_building(room_data *room);
	
	struct trading_post_data *tpd, *next_tpd;
	struct empire_unique_storage *eus;
	craft_data *craft, *next_craft;
	augment_data *aug, *next_aug;
	vehicle_data *veh, *next_veh;
	room_data *room, *next_room;
	empire_data *emp, *next_emp;
	bld_data *bld, *next_bld;
	obj_data *obj, *next_obj;
	descriptor_data *desc;
	generic_data *gen;
	bool found, save;
	int res_type;
	
	if (!(gen = find_generic_by_vnum(vnum))) {
		msg_to_char(ch, "There is no such generic %d.\r\n", vnum);
		return;
	}
	
	switch (GEN_TYPE(gen)) {
		case GENERIC_ACTION: {
			res_type = RES_ACTION;
			break;
		}
		case GENERIC_LIQUID: {
			res_type = RES_LIQUID;
			break;
		}
		default: {
			res_type = NOTHING;
			break;
		}
	}
	
	// remove from live lists: drink containers
	LL_FOREACH(object_list, obj) {
		if (GEN_TYPE(gen) == GENERIC_LIQUID && IS_DRINK_CONTAINER(obj) && GET_DRINK_CONTAINER_TYPE(obj) == vnum) {
			GET_OBJ_VAL(obj, VAL_DRINK_CONTAINER_TYPE) = LIQ_WATER;
		}
	}
	
	// remove from live lists: trading post drink containers
	for (tpd = trading_list; tpd; tpd = next_tpd) {
		next_tpd = tpd->next;
		if (!tpd->obj) {
			continue;
		}
		
		if (GEN_TYPE(gen) == GENERIC_LIQUID && IS_DRINK_CONTAINER(tpd->obj) && GET_DRINK_CONTAINER_TYPE(tpd->obj) == vnum) {
			GET_OBJ_VAL(tpd->obj, VAL_DRINK_CONTAINER_TYPE) = LIQ_WATER;
		}
	}
	
	// remove from live lists: unique storage of drink containers
	HASH_ITER(hh, empire_table, emp, next_emp) {
		save = FALSE;
		LL_FOREACH(EMPIRE_UNIQUE_STORAGE(emp), eus) {
			if (!eus->obj) {
				continue;
			}
			if (GEN_TYPE(gen) == GENERIC_LIQUID && IS_DRINK_CONTAINER(eus->obj) && GET_DRINK_CONTAINER_TYPE(eus->obj) == vnum) {
				GET_OBJ_VAL(eus->obj, VAL_DRINK_CONTAINER_TYPE) = LIQ_WATER;
			}
		}
		
		if (save) {
			save_empire(emp);
		}
	}
	
	// remove from live resource lists: room resources
	HASH_ITER(hh, world_table, room, next_room) {
		if (!COMPLEX_DATA(room)) {
			continue;
		}
		
		if (GET_BUILT_WITH(room)) {
			remove_thing_from_resource_list(&GET_BUILT_WITH(room), res_type, vnum);
		}
		if (GET_BUILDING_RESOURCES(room)) {
			remove_thing_from_resource_list(&GET_BUILDING_RESOURCES(room), res_type, vnum);
			
			if (!GET_BUILDING_RESOURCES(room)) {
				// removing this resource finished the building
				if (IS_DISMANTLING(room)) {
					disassociate_building(room);
				}
				else {
					complete_building(room);
				}
			}
		}
	}
	
	// remove from live resource lists: vehicle maintenance
	LL_FOREACH(vehicle_list, veh) {
		if (VEH_NEEDS_RESOURCES(veh)) {
			remove_thing_from_resource_list(&VEH_NEEDS_RESOURCES(veh), res_type, vnum);
			
			if (!VEH_NEEDS_RESOURCES(veh)) {
				// removing the resource finished the vehicle
				if (VEH_FLAGGED(veh, VEH_INCOMPLETE)) {
					REMOVE_BIT(VEH_FLAGS(veh), VEH_INCOMPLETE);
					load_vtrigger(veh);
				}
			}
		}
	}
	
	// remove it from the hash table first
	remove_generic_from_table(gen);

	// save index and generic file now
	save_index(DB_BOOT_GEN);
	save_library_file_for_vnum(DB_BOOT_GEN, vnum);
	
	// now remove from prototypes
	
	// update augments
	HASH_ITER(hh, augment_table, aug, next_aug) {
		if (remove_thing_from_resource_list(&GET_AUG_RESOURCES(aug), res_type, vnum)) {
			SET_BIT(GET_AUG_FLAGS(aug), AUG_IN_DEVELOPMENT);
			save_library_file_for_vnum(DB_BOOT_AUG, GET_AUG_VNUM(aug));
		}
	}
	
	// update buildings
	HASH_ITER(hh, building_table, bld, next_bld) {
		if (remove_thing_from_resource_list(&GET_BLD_YEARLY_MAINTENANCE(bld), res_type, vnum)) {
			save_library_file_for_vnum(DB_BOOT_BLD, GET_BLD_VNUM(bld));
		}
	}
	
	// update crafts
	HASH_ITER(hh, craft_table, craft, next_craft) {
		found = FALSE;
		if (CRAFT_FLAGGED(craft, CRAFT_SOUP) && GET_CRAFT_OBJECT(craft) == vnum) {
			GET_CRAFT_OBJECT(craft) = LIQ_WATER;
			found |= TRUE;
		}
		found |= remove_thing_from_resource_list(&GET_CRAFT_RESOURCES(craft), res_type, vnum);
		if (found) {
			SET_BIT(GET_CRAFT_FLAGS(craft), CRAFT_IN_DEVELOPMENT);
			save_library_file_for_vnum(DB_BOOT_CRAFT, GET_CRAFT_VNUM(craft));
		}
	}
	
	// update objs
	HASH_ITER(hh, object_table, obj, next_obj) {
		if (GEN_TYPE(gen) == GENERIC_LIQUID && IS_DRINK_CONTAINER(obj) && GET_DRINK_CONTAINER_TYPE(obj) == vnum) {
			GET_OBJ_VAL(obj, VAL_DRINK_CONTAINER_TYPE) = LIQ_WATER;
			save_library_file_for_vnum(DB_BOOT_OBJ, GET_OBJ_VNUM(obj));
		}
	}
	
	// update vehicles
	HASH_ITER(hh, vehicle_table, veh, next_veh) {
		if (remove_thing_from_resource_list(&VEH_YEARLY_MAINTENANCE(veh), res_type, vnum)) {
			save_library_file_for_vnum(DB_BOOT_VEH, VEH_VNUM(veh));
		}
	}
	
	// olc editor updates
	LL_FOREACH(descriptor_list, desc) {
		if (desc->character && !IS_NPC(desc->character) && GET_ACTION_RESOURCES(desc->character)) {
			remove_thing_from_resource_list(&GET_ACTION_RESOURCES(desc->character), res_type, vnum);
		}
		
		if (GET_OLC_AUGMENT(desc)) {
			if (remove_thing_from_resource_list(&GET_AUG_RESOURCES(GET_OLC_AUGMENT(desc)), res_type, vnum)) {
				SET_BIT(GET_AUG_FLAGS(GET_OLC_AUGMENT(desc)), AUG_IN_DEVELOPMENT);
				msg_to_char(desc->character, "One of the resources used in the augment you're editing was deleted.\r\n");
			}
		}
		if (GET_OLC_BUILDING(desc)) {
			if (remove_thing_from_resource_list(&GET_BLD_YEARLY_MAINTENANCE(GET_OLC_BUILDING(desc)), res_type, vnum)) {
				msg_to_char(desc->character, "One of the resources used in the building you're editing was deleted.\r\n");
			}
		}
		if (GET_OLC_CRAFT(desc)) {
			found = FALSE;
			if (CRAFT_FLAGGED(GET_OLC_CRAFT(desc), CRAFT_SOUP) && GET_CRAFT_OBJECT(GET_OLC_CRAFT(desc)) == vnum) {
				GET_CRAFT_OBJECT(GET_OLC_CRAFT(desc)) = LIQ_WATER;
				found |= TRUE;
			}
			found |= remove_thing_from_resource_list(&GET_OLC_CRAFT(desc)->resources, res_type, vnum);
			if (found) {
				SET_BIT(GET_OLC_CRAFT(desc)->flags, CRAFT_IN_DEVELOPMENT);
				msg_to_char(desc->character, "One of the resources used in the craft you're editing was deleted.\r\n");
			}	
		}
		if (GET_OLC_OBJECT(desc)) {
			if (GEN_TYPE(gen) == GENERIC_LIQUID && IS_DRINK_CONTAINER(GET_OLC_OBJECT(desc)) && GET_DRINK_CONTAINER_TYPE(GET_OLC_OBJECT(desc)) == vnum) {
				GET_OBJ_VAL(GET_OLC_OBJECT(desc), VAL_DRINK_CONTAINER_TYPE) = LIQ_WATER;
				msg_to_char(desc->character, "The liquid used by the object you're editing was deleted.\r\n");
			}
		}
		if (GET_OLC_VEHICLE(desc)) {
			if (remove_thing_from_resource_list(&VEH_YEARLY_MAINTENANCE(GET_OLC_VEHICLE(desc)), res_type, vnum)) {
				msg_to_char(desc->character, "One of the resources used for maintenance for the vehicle you're editing was deleted.\r\n");
			}
		}
	}
	
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
		case GENERIC_ACTION: {
			size += snprintf(buf + size, sizeof(buf) - size, "Build-to-Char: %s\r\n", NULLSAFE(GEN_STRING(gen, GSTR_ACTION_BUILD_TO_CHAR)));
			size += snprintf(buf + size, sizeof(buf) - size, "Build-to-Room: %s\r\n", NULLSAFE(GEN_STRING(gen, GSTR_ACTION_BUILD_TO_ROOM)));
			size += snprintf(buf + size, sizeof(buf) - size, "Craft-to-Char: %s\r\n", NULLSAFE(GEN_STRING(gen, GSTR_ACTION_CRAFT_TO_CHAR)));
			size += snprintf(buf + size, sizeof(buf) - size, "Craft-to-Room: %s\r\n", NULLSAFE(GEN_STRING(gen, GSTR_ACTION_CRAFT_TO_ROOM)));
			size += snprintf(buf + size, sizeof(buf) - size, "Repair-to-Char: %s\r\n", NULLSAFE(GEN_STRING(gen, GSTR_ACTION_REPAIR_TO_CHAR)));
			size += snprintf(buf + size, sizeof(buf) - size, "Repair-to-Room: %s\r\n", NULLSAFE(GEN_STRING(gen, GSTR_ACTION_REPAIR_TO_ROOM)));
			break;
		}
		case GENERIC_COOLDOWN: {
			size += snprintf(buf + size, sizeof(buf) - size, "Wear-off: %s\r\n", GET_COOLDOWN_WEAR_OFF(gen) ? GET_COOLDOWN_WEAR_OFF(gen) : "(none)");
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
		case GENERIC_ACTION: {
			sprintf(buf + strlen(buf), "<\tybuild2char\t0> %s\r\n", GEN_STRING(gen, GSTR_ACTION_BUILD_TO_CHAR) ? GEN_STRING(gen, GSTR_ACTION_BUILD_TO_CHAR) : "(none)");
			sprintf(buf + strlen(buf), "<\tybuild2room\t0> %s\r\n", GEN_STRING(gen, GSTR_ACTION_BUILD_TO_ROOM) ? GEN_STRING(gen, GSTR_ACTION_BUILD_TO_ROOM) : "(none)");
			sprintf(buf + strlen(buf), "<\tycraft2char\t0> %s\r\n", GEN_STRING(gen, GSTR_ACTION_CRAFT_TO_CHAR) ? GEN_STRING(gen, GSTR_ACTION_CRAFT_TO_CHAR) : "(none)");
			sprintf(buf + strlen(buf), "<\tycraft2room\t0> %s\r\n", GEN_STRING(gen, GSTR_ACTION_CRAFT_TO_ROOM) ? GEN_STRING(gen, GSTR_ACTION_CRAFT_TO_ROOM) : "(none)");
			sprintf(buf + strlen(buf), "<\tyrepair2char\t0> %s\r\n", GEN_STRING(gen, GSTR_ACTION_REPAIR_TO_CHAR) ? GEN_STRING(gen, GSTR_ACTION_REPAIR_TO_CHAR) : "(none)");
			sprintf(buf + strlen(buf), "<\tyrepair2room\t0> %s\r\n", GEN_STRING(gen, GSTR_ACTION_REPAIR_TO_ROOM) ? GEN_STRING(gen, GSTR_ACTION_REPAIR_TO_ROOM) : "(none)");
			break;
		}
		case GENERIC_COOLDOWN: {
			sprintf(buf + strlen(buf), "<\tywearoff\t0> %s\r\n", GET_COOLDOWN_WEAR_OFF(gen) ? GET_COOLDOWN_WEAR_OFF(gen) : "(none)");
			sprintf(buf + strlen(buf), "<\tystandardwearoff\t0> (to add a basic wear-off message based on the name)\r\n");
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
			msg_to_char(ch, "%3d. [%5d] %s (%s)\r\n", ++found, GEN_VNUM(iter), GEN_NAME(iter), generic_types[GEN_TYPE(iter)]);
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
//// ACTION OLC MODULES //////////////////////////////////////////////////////

OLC_MODULE(genedit_build2char) {
	generic_data *gen = GET_OLC_GENERIC(ch->desc);
	
	if (GEN_TYPE(gen) != GENERIC_ACTION) {
		msg_to_char(ch, "You can only change that on an ACTION generic.\r\n");
	}
	else if (!str_cmp(arg, "none")) {
		if (GEN_STRING(gen, GSTR_ACTION_BUILD_TO_CHAR)) {
			free(GEN_STRING(gen, GSTR_ACTION_BUILD_TO_CHAR));
		}
		GEN_STRING(gen, GSTR_ACTION_BUILD_TO_CHAR) = NULL;
		msg_to_char(ch, "Build2char messsage removed.\r\n");
	}
	else {
		olc_process_string(ch, argument, "build2char", &GEN_STRING(gen, GSTR_ACTION_BUILD_TO_CHAR));
	}
}


OLC_MODULE(genedit_build2room) {
	generic_data *gen = GET_OLC_GENERIC(ch->desc);
	
	if (GEN_TYPE(gen) != GENERIC_ACTION) {
		msg_to_char(ch, "You can only change that on an ACTION generic.\r\n");
	}
	else if (!str_cmp(arg, "none")) {
		if (GEN_STRING(gen, GSTR_ACTION_BUILD_TO_ROOM)) {
			free(GEN_STRING(gen, GSTR_ACTION_BUILD_TO_ROOM));
		}
		GEN_STRING(gen, GSTR_ACTION_BUILD_TO_ROOM) = NULL;
		msg_to_char(ch, "Build2room messsage removed.\r\n");
	}
	else {
		olc_process_string(ch, argument, "build2room", &GEN_STRING(gen, GSTR_ACTION_BUILD_TO_ROOM));
	}
}


OLC_MODULE(genedit_craft2char) {
	generic_data *gen = GET_OLC_GENERIC(ch->desc);
	
	if (GEN_TYPE(gen) != GENERIC_ACTION) {
		msg_to_char(ch, "You can only change that on an ACTION generic.\r\n");
	}
	else if (!str_cmp(arg, "none")) {
		if (GEN_STRING(gen, GSTR_ACTION_CRAFT_TO_CHAR)) {
			free(GEN_STRING(gen, GSTR_ACTION_CRAFT_TO_CHAR));
		}
		GEN_STRING(gen, GSTR_ACTION_CRAFT_TO_CHAR) = NULL;
		msg_to_char(ch, "Craft2char messsage removed.\r\n");
	}
	else {
		olc_process_string(ch, argument, "craft2char", &GEN_STRING(gen, GSTR_ACTION_CRAFT_TO_CHAR));
	}
}


OLC_MODULE(genedit_craft2room) {
	generic_data *gen = GET_OLC_GENERIC(ch->desc);
	
	if (GEN_TYPE(gen) != GENERIC_ACTION) {
		msg_to_char(ch, "You can only change that on an ACTION generic.\r\n");
	}
	else if (!str_cmp(arg, "none")) {
		if (GEN_STRING(gen, GSTR_ACTION_CRAFT_TO_ROOM)) {
			free(GEN_STRING(gen, GSTR_ACTION_CRAFT_TO_ROOM));
		}
		GEN_STRING(gen, GSTR_ACTION_CRAFT_TO_ROOM) = NULL;
		msg_to_char(ch, "Craft2room messsage removed.\r\n");
	}
	else {
		olc_process_string(ch, argument, "craft2room", &GEN_STRING(gen, GSTR_ACTION_CRAFT_TO_ROOM));
	}
}


OLC_MODULE(genedit_repair2char) {
	generic_data *gen = GET_OLC_GENERIC(ch->desc);
	
	if (GEN_TYPE(gen) != GENERIC_ACTION) {
		msg_to_char(ch, "You can only change that on an ACTION generic.\r\n");
	}
	else if (!str_cmp(arg, "none")) {
		if (GEN_STRING(gen, GSTR_ACTION_REPAIR_TO_CHAR)) {
			free(GEN_STRING(gen, GSTR_ACTION_REPAIR_TO_CHAR));
		}
		GEN_STRING(gen, GSTR_ACTION_REPAIR_TO_CHAR) = NULL;
		msg_to_char(ch, "Repair2char messsage removed.\r\n");
	}
	else {
		olc_process_string(ch, argument, "repair2char", &GEN_STRING(gen, GSTR_ACTION_REPAIR_TO_CHAR));
	}
}


OLC_MODULE(genedit_repair2room) {
	generic_data *gen = GET_OLC_GENERIC(ch->desc);
	
	if (GEN_TYPE(gen) != GENERIC_ACTION) {
		msg_to_char(ch, "You can only change that on an ACTION generic.\r\n");
	}
	else if (!str_cmp(arg, "none")) {
		if (GEN_STRING(gen, GSTR_ACTION_REPAIR_TO_ROOM)) {
			free(GEN_STRING(gen, GSTR_ACTION_REPAIR_TO_ROOM));
		}
		GEN_STRING(gen, GSTR_ACTION_REPAIR_TO_ROOM) = NULL;
		msg_to_char(ch, "Repair2room messsage removed.\r\n");
	}
	else {
		olc_process_string(ch, argument, "repair2room", &GEN_STRING(gen, GSTR_ACTION_REPAIR_TO_ROOM));
	}
}


 //////////////////////////////////////////////////////////////////////////////
//// COOLDOWN OLC MODULES ////////////////////////////////////////////////////

OLC_MODULE(genedit_standardwearoff) {
	generic_data *gen = GET_OLC_GENERIC(ch->desc);
	char buf[MAX_STRING_LENGTH];
	
	if (GEN_TYPE(gen) != GENERIC_COOLDOWN) {
		msg_to_char(ch, "You can only change that on an COOLDOWN generic.\r\n");
	}
	else {
		snprintf(buf, sizeof(buf), "Your %s cooldown has ended.", GEN_NAME(gen));
		if (GEN_STRING(gen, GSTR_COOLDOWN_WEAR_OFF)) {
			free(GEN_STRING(gen, GSTR_COOLDOWN_WEAR_OFF));
		}
		GEN_STRING(gen, GSTR_COOLDOWN_WEAR_OFF) = str_dup(buf);

		if (PRF_FLAGGED(ch, PRF_NOREPEAT)) {
			send_config_msg(ch, "ok_string");
		}
		else {
			msg_to_char(ch, "Wear-off messsage changed to: %s\r\n", buf);
		}
	}
}


OLC_MODULE(genedit_wearoff) {
	generic_data *gen = GET_OLC_GENERIC(ch->desc);
	
	if (GEN_TYPE(gen) != GENERIC_COOLDOWN) {
		msg_to_char(ch, "You can only change that on an COOLDOWN generic.\r\n");
	}
	else if (!str_cmp(arg, "none")) {
		if (GEN_STRING(gen, GSTR_COOLDOWN_WEAR_OFF)) {
			free(GEN_STRING(gen, GSTR_COOLDOWN_WEAR_OFF));
		}
		GEN_STRING(gen, GSTR_COOLDOWN_WEAR_OFF) = NULL;
		msg_to_char(ch, "Wear-off messsage removed.\r\n");
	}
	else {
		olc_process_string(ch, argument, "wearoff", &GEN_STRING(gen, GSTR_COOLDOWN_WEAR_OFF));
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
