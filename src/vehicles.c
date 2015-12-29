/* ************************************************************************
*   File: vehicles.c                                      EmpireMUD 2.0b3 *
*  Usage: DB and OLC for vehicles (including ships)                       *
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
#include "dg_scripts.h"

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
const char *default_vehicle_keywords = "vehicle unnamed";
const char *default_vehicle_short_desc = "an unnamed vehicle";
const char *default_vehicle_long_desc = "An unnamed vehicle is parked here.";

// local protos
void clear_vehicle(vehicle_data *veh);

// external consts
extern const char *designate_flags[];
extern const char *mob_move_types[];
extern const char *vehicle_flags[];

// external funcs
extern struct resource_data *copy_resource_list(struct resource_data *input);
void get_resource_display(struct resource_data *list, char *save_buffer);
extern char *show_color_codes(char *string);


 //////////////////////////////////////////////////////////////////////////////
//// HELPERS /////////////////////////////////////////////////////////////////

/**
* @param vehicle_data *veh Any vehicle instance.
* @return int The number of animals harnessed to it.
*/
int count_harnessed_animals(vehicle_data *veh) {
	struct vehicle_attached_mob *iter;
	int count;
	LL_COUNT(VEH_ANIMALS(veh), iter, count);
	return count;
}


/**
* Empties the contents of a vehicle into the room it's in (if any) or extracts
* them.
*
* @param vehicle_data *veh The vehicle to empty.
*/
void empty_vehicle(vehicle_data *veh) {
	obj_data *obj, *next_obj;
	
	LL_FOREACH_SAFE2(VEH_CONTAINS(veh), obj, next_obj, next_content) {
		if (IN_ROOM(veh)) {
			obj_to_room(obj, IN_ROOM(veh));
		}
		else {
			extract_obj(obj);
		}
	}
}


/**
* Finds a mob (using multi-isname) attached to a vehicle, and returns the
* vehicle_attached_mob entry for it.
*
* @param vehicle_data *veh The vehicle to check.
* @param char *name The typed-in name (may contain "#.name" and/or multiple keywords).
* @return struct vehicle_attached_mob* The found entry, or NULL.
*/
struct vehicle_attached_mob *find_harnessed_mob_by_name(vehicle_data *veh, char *name) {
	struct vehicle_attached_mob *iter;
	char tmpname[MAX_INPUT_LENGTH];
	char *tmp = tmpname;
	char_data *proto;
	int number;
	
	// safety first
	if (!veh || !*name) {
		return NULL;
	}
	
	strcpy(tmp, name);
	number = get_number(&tmp);
	
	LL_FOREACH(VEH_ANIMALS(veh), iter) {
		if (!(proto = mob_proto(iter->mob))) {
			continue;
		}
		if (!multi_isname(tmp, GET_PC_NAME(proto))) {
			continue;
		}
		
		// found
		if (--number == 0) {
			return iter;
		}
	}
	
	return NULL;
}


/**
* Attaches an animal to a vehicle, and extracts the animal.
*
* @param char_data *mob The mob to attach.
* @param vehicle_data *veh The vehicle to attach it to.
*/
void harness_mob_to_vehicle(char_data *mob, vehicle_data *veh) {
	struct vehicle_attached_mob *vam;
	
	// safety first
	if (!mob || !IS_NPC(mob) || !veh) {
		return;
	}
	
	CREATE(vam, struct vehicle_attached_mob, 1);
	vam->mob = GET_MOB_VNUM(mob);
	vam->scale_level = GET_CURRENT_SCALE_LEVEL(mob);
	vam->flags = MOB_FLAGS(mob);
	vam->empire = GET_LOYALTY(mob) ? EMPIRE_VNUM(GET_LOYALTY(mob)) : NOTHING;
	
	LL_PREPEND(VEH_ANIMALS(veh), vam);
	extract_char(mob);
}


/**
* Gets the short description of a vehicle as seen by a particular person.
*
* @param vehicle_data *veh The vehicle to show.
* @param char_data *to Optional: A person who must be able to see it (or they get "something").
* @return char* The short description of the vehicle for that person.
*/
char *get_vehicle_short_desc(vehicle_data *veh, char_data *to) {
	if (!veh) {
		return "<nothing>";
	}
	
	if (to && !CAN_SEE_VEHICLE(to, veh)) {
		return "something";
	}
	
	return VEH_SHORT_DESC(veh);
}


/**
* Gets a nicely-formatted comma-separated list of all the animals leading
* the vehicle.
*
* @param vehicle_data *veh The vehicle.
* @return char* The list of animals pulling it.
*/
char *list_harnessed_mobs(vehicle_data *veh) {
	static char output[MAX_STRING_LENGTH];
	struct vehicle_attached_mob *iter, *next_iter;
	size_t size = 0;
	int num = 0;
	
	*output = '\0';
	
	LL_FOREACH_SAFE(VEH_ANIMALS(veh), iter, next_iter) {
		size += snprintf(output+size, sizeof(output)-size, "%s%s", ((num == 0) ? "" : (next_iter ? ", " : (num > 1 ? ", and " : " and "))), get_mob_name_by_proto(iter->mob));
		++num;
	}
	
	return output;
}


/**
* Unharnesses a mob and loads it back into the game. If it fails to load the
* mob, it will still remove 'vam' from the animals list.
*
* @param struct vehicle_attached_mob *vam The attached-mob entry to unharness.
* @param vehicle_data *veh The vehicle to remove it from.
* @return char_data* A pointer to the mob if one was loaded, or NULL if not.
*/
char_data *unharness_mob_from_vehicle(struct vehicle_attached_mob *vam, vehicle_data *veh) {
	void scale_mob_to_level(char_data *mob, int level);
	void setup_generic_npc(char_data *mob, empire_data *emp, int name, int sex);	
	
	char_data *mob;
	
	// safety first
	if (!vam || !veh) {
		return NULL;
	}
	
	// remove the vam entry now
	LL_DELETE(VEH_ANIMALS(veh), vam);
	
	// things that keep us from spawning the mob
	if (!IN_ROOM(veh) || !mob_proto(vam->mob)) {
		free(vam);
		return NULL;
	}
	
	mob = read_mobile(vam->mob, TRUE);
	MOB_FLAGS(mob) = vam->flags;
	setup_generic_npc(mob, real_empire(vam->empire), NOTHING, NOTHING);
	if (vam->scale_level > 0) {
		scale_mob_to_level(mob, vam->scale_level);
	}
	char_to_room(mob, IN_ROOM(veh));
	load_mtrigger(mob);
	
	free(vam);
	return mob;
}


 //////////////////////////////////////////////////////////////////////////////
//// UTILITIES ///////////////////////////////////////////////////////////////

/**
* Checks for common vehicle problems and reports them to ch.
*
* @param vehicle_data *veh The item to audit.
* @param char_data *ch The person to report to.
* @return bool TRUE if any problems were reported; FALSE if all good.
*/
bool audit_vehicle(vehicle_data *veh, char_data *ch) {
	// char temp[MAX_STRING_LENGTH];
	bool problem = FALSE;
	
	/*
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
	*/
	
	return problem;
}


/**
* For the .list command.
*
* @param vehicle_data *veh The thing to list.
* @param bool detail If TRUE, provide additional details
* @return char* The line to show (without a CRLF).
*/
char *list_one_vehicle(vehicle_data *veh, bool detail) {
	static char output[MAX_STRING_LENGTH];
	// char part[MAX_STRING_LENGTH], applies[MAX_STRING_LENGTH];
	
	if (detail) {
		snprintf(output, sizeof(output), "[%5d] %s", VEH_VNUM(veh), VEH_SHORT_DESC(veh));
	}
	else {
		snprintf(output, sizeof(output), "[%5d] %s", VEH_VNUM(veh), VEH_SHORT_DESC(veh));
	}
		
	return output;
}


/**
* Searches for all uses of a vehicle and displays them.
*
* @param char_data *ch The player.
* @param any_vnum vnum The vehicle vnum.
*/
void olc_search_vehicle(char_data *ch, any_vnum vnum) {
	char buf[MAX_STRING_LENGTH];
	vehicle_data *veh = vehicle_proto(vnum);
	int size, found;
	
	if (!veh) {
		msg_to_char(ch, "There is no vehicle %d.\r\n", vnum);
		return;
	}
	
	found = 0;
	size = snprintf(buf, sizeof(buf), "Occurrences of vehicle %d (%s):\r\n", vnum, VEH_SHORT_DESC(veh));
	
	// vehicles are not actually used anywhere else
	
	if (found > 0) {
		size += snprintf(buf + size, sizeof(buf) - size, "%d location%s shown\r\n", found, PLURAL(found));
	}
	else {
		size += snprintf(buf + size, sizeof(buf) - size, " none\r\n");
	}
	
	page_string(ch->desc, buf, TRUE);
}


/**
* Create a new vehicle from a prototype. You should almost always call this
* with with_triggers = TRUE.
*
* @param any_vnum vnum The vehicle vnum to load.
* @param bool with_triggers If TRUE, attaches all triggers.
* @return vehicle_data* The instantiated vehicle.
*/
vehicle_data *read_vehicle(any_vnum vnum, bool with_triggers) {
	extern int max_vehicle_id;
	
	vehicle_data *veh, *proto;
	
	if (!(proto = vehicle_proto(vnum))) {
		log("Vehicle vnum %d does not exist in database.", vnum);
		// grab first one (bug)
		proto = vehicle_table;
	}

	CREATE(veh, vehicle_data, 1);
	clear_vehicle(veh);

	*veh = *proto;
	LL_PREPEND2(vehicle_list, veh, next);
	
	// new vehicle setup
	VEH_OWNER(veh) = NULL;
	VEH_SCALE_LEVEL(veh) = 0;	// unscaled
	VEH_HEALTH(veh) = VEH_MAX_HEALTH(veh);
	VEH_CONTAINS(veh) = NULL;
	VEH_CARRYING_N(veh) = 0;
	VEH_ANIMALS(veh) = NULL;
	VEH_NEEDS_RESOURCES(veh) = NULL;
	IN_ROOM(veh) = NULL;
	REMOVE_BIT(VEH_FLAGS(veh), VEH_INCOMPLETE);	// ensure not marked incomplete
	
	// script id -- find_vehicle helper
	GET_ID(veh) = max_vehicle_id++;
	add_to_lookup_table(GET_ID(veh), (void *)veh);
	
	/*
	if (with_triggers) {
		copy_proto_script(proto, veh, VEH_TRIGGER);
		assign_triggers(veh, VEH_TRIGGER);
	}
	*/

	return veh;
}


// Simple vnum sorter for the vehicle hash
int sort_vehicles(vehicle_data *a, vehicle_data *b) {
	return VEH_VNUM(a) - VEH_VNUM(b);
}


 //////////////////////////////////////////////////////////////////////////////
//// DATABASE ////////////////////////////////////////////////////////////////

/**
* Puts a vehicle into the hash table.
*
* @param vehicle_data *veh The vehicle data to add to the table.
*/
void add_vehicle_to_table(vehicle_data *veh) {
	vehicle_data *find;
	any_vnum vnum;
	
	if (veh) {
		vnum = VEH_VNUM(veh);
		HASH_FIND_INT(vehicle_table, &vnum, find);
		if (!find) {
			HASH_ADD_INT(vehicle_table, vnum, veh);
			HASH_SORT(vehicle_table, sort_vehicles);
		}
	}
}


/**
* @param any_vnum vnum Any vehicle vnum
* @return vehicle_data* The vehicle, or NULL if it doesn't exist
*/
vehicle_data *vehicle_proto(any_vnum vnum) {
	vehicle_data *veh;
	
	if (vnum < 0 || vnum == NOTHING) {
		return NULL;
	}
	
	HASH_FIND_INT(vehicle_table, &vnum, veh);
	return veh;
}


/**
* Removes a vehicle from the hash table.
*
* @param vehicle_data *veh The vehicle data to remove from the table.
*/
void remove_vehicle_from_table(vehicle_data *veh) {
	HASH_DEL(vehicle_table, veh);
}


/**
* Initializes a new vehicle. This clears all memory for it, so set the vnum
* AFTER.
*
* @param vehicle_data *veh The vehicle to initialize.
*/
void clear_vehicle(vehicle_data *veh) {
	struct vehicle_attribute_data *attr = veh->attributes;
	
	memset((char *) veh, 0, sizeof(vehicle_data));
	
	VEH_VNUM(veh) = NOTHING;
	
	veh->attributes = attr;	// stored from earlier
	if (veh->attributes) {
		memset((char *)(veh->attributes), 0, sizeof(struct vehicle_attribute_data));
	}
	else {
		CREATE(veh->attributes, struct vehicle_attribute_data, 1);
	}
}


/**
* frees up memory for a vehicle data item.
*
* See also: olc_delete_vehicle
*
* @param vehicle_data *veh The vehicle data to free.
*/
void free_vehicle(vehicle_data *veh) {
	vehicle_data *proto = vehicle_proto(VEH_VNUM(veh));
	struct vehicle_attached_mob *vam;
	
	// strings
	if (VEH_KEYWORDS(veh) && (!proto || VEH_KEYWORDS(veh) != VEH_KEYWORDS(proto))) {
		free(VEH_KEYWORDS(veh));
	}
	if (VEH_SHORT_DESC(veh) && (!proto || VEH_SHORT_DESC(veh) != VEH_SHORT_DESC(proto))) {
		free(VEH_SHORT_DESC(veh));
	}
	if (VEH_LONG_DESC(veh) && (!proto || VEH_LONG_DESC(veh) != VEH_LONG_DESC(proto))) {
		free(VEH_LONG_DESC(veh));
	}
	if (VEH_LOOK_DESC(veh) && (!proto || VEH_LOOK_DESC(veh) != VEH_LOOK_DESC(proto))) {
		free(VEH_LOOK_DESC(veh));
	}
	if (VEH_ICON(veh) && (!proto || VEH_ICON(veh) != VEH_ICON(proto))) {
		free(VEH_ICON(veh));
	}
	
	// pointers
	if (VEH_NEEDS_RESOURCES(veh)) {
		free_resource_list(VEH_NEEDS_RESOURCES(veh));
	}
	while ((vam = VEH_ANIMALS(veh))) {
		VEH_ANIMALS(veh) = vam->next;
		free(vam);
	}
	empty_vehicle(veh);
	
	// attributes
	if (veh->attributes && (!proto || veh->attributes != proto->attributes)) {
		if (VEH_YEARLY_MAINTENANCE(veh)) {
			free_resource_list(VEH_YEARLY_MAINTENANCE(veh));
		}
		
		free(veh->attributes);
	}
	
	free(veh);
}


/**
* Read one vehicle from file.
*
* @param FILE *fl The open .veh file
* @param any_vnum vnum The vehicle vnum
*/
void parse_vehicle(FILE *fl, any_vnum vnum) {
	void parse_resource(FILE *fl, struct resource_data **list, char *error_str);

	char line[256], error[256], str_in[256];
	vehicle_data *veh, *find;
	int int_in[4];
	
	CREATE(veh, vehicle_data, 1);
	clear_vehicle(veh);
	VEH_VNUM(veh) = vnum;
	
	HASH_FIND_INT(vehicle_table, &vnum, find);
	if (find) {
		log("WARNING: Duplicate vehicle vnum #%d", vnum);
		// but have to load it anyway to advance the file
	}
	add_vehicle_to_table(veh);
		
	// for error messages
	sprintf(error, "vehicle vnum %d", vnum);
	
	// lines 1-5: strings
	VEH_KEYWORDS(veh) = fread_string(fl, error);
	VEH_SHORT_DESC(veh) = fread_string(fl, error);
	VEH_LONG_DESC(veh) = fread_string(fl, error);
	VEH_LOOK_DESC(veh) = fread_string(fl, error);
	VEH_ICON(veh) = fread_string(fl, error);
	
	// line 6: flags move_type maxhealth capacity animals_required
	if (!get_line(fl, line) || sscanf(line, "%s %d %d %d %d", str_in, &int_in[0], &int_in[1], &int_in[2], &int_in[3]) != 5) {
		log("SYSERR: Format error in line 6 of %s", error);
		exit(1);
	}
	
	VEH_FLAGS(veh) = asciiflag_conv(str_in);
	VEH_MOVE_TYPE(veh) = int_in[0];
	VEH_HEALTH(veh) = VEH_MAX_HEALTH(veh) = int_in[1];
	VEH_CAPACITY(veh) = int_in[2];
	VEH_ANIMALS_REQUIRED(veh) = int_in[3];
	
	// optionals
	for (;;) {
		if (!get_line(fl, line)) {
			log("SYSERR: Format error in %s, expecting alphabetic flags", error);
			exit(1);
		}
		switch (*line) {
			case 'D': {	// designate/rooms
				if (!get_line(fl, line) || sscanf(line, "%d %s", &int_in[0], str_in) != 2) {
					log("SYSERR: Format error in D line of %s", error);
					exit(1);
				}
				
				VEH_MAX_ROOMS(veh) = int_in[0];
				VEH_DESIGNATE_FLAGS(veh) = asciiflag_conv(str_in);
				break;
			}
			
			case 'R': {	// resources/yearly maintenance
				parse_resource(fl, &VEH_YEARLY_MAINTENANCE(veh), error);
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


// writes entries in the vehicle index
void write_vehicle_index(FILE *fl) {
	vehicle_data *veh, *next_veh;
	int this, last;
	
	last = -1;
	HASH_ITER(hh, vehicle_table, veh, next_veh) {
		// determine "zone number" by vnum
		this = (int)(VEH_VNUM(veh) / 100);
	
		if (this != last) {
			fprintf(fl, "%d%s\n", this, VEH_SUFFIX);
			last = this;
		}
	}
}


/**
* Outputs one vehicle item in the db file format, starting with a #VNUM and
* ending with an S.
*
* @param FILE *fl The file to write it to.
* @param vehicle_data *veh The thing to save.
*/
void write_vehicle_to_file(FILE *fl, vehicle_data *veh) {
	void write_resources_to_file(FILE *fl, struct resource_data *list);
	
	char temp[MAX_STRING_LENGTH];
	
	if (!fl || !veh) {
		syslog(SYS_ERROR, LVL_START_IMM, TRUE, "SYSERR: write_vehicle_to_file called without %s", !fl ? "file" : "vehicle");
		return;
	}
	
	fprintf(fl, "#%d\n", VEH_VNUM(veh));
	
	// 1-5. strings
	fprintf(fl, "%s~\n", NULLSAFE(VEH_KEYWORDS(veh)));
	fprintf(fl, "%s~\n", NULLSAFE(VEH_SHORT_DESC(veh)));
	fprintf(fl, "%s~\n", NULLSAFE(VEH_LONG_DESC(veh)));
	strcpy(temp, NULLSAFE(VEH_LOOK_DESC(veh)));
	strip_crlf(temp);
	fprintf(fl, "%s~\n", temp);
	fprintf(fl, "%s~\n", NULLSAFE(VEH_ICON(veh)));
	
	// 6. flags move_type maxhealth capacity animals_required
	strcpy(temp, bitv_to_alpha(VEH_FLAGS(veh)));
	fprintf(fl, "%s %d %d %d %d\n", temp, VEH_MOVE_TYPE(veh), VEH_MAX_HEALTH(veh), VEH_CAPACITY(veh), VEH_ANIMALS_REQUIRED(veh));
	
	// 'D': designate/rooms
	if (VEH_MAX_ROOMS(veh) || VEH_DESIGNATE_FLAGS(veh)) {
		strcpy(temp, bitv_to_alpha(VEH_DESIGNATE_FLAGS(veh)));
		fprintf(fl, "D\n%d %s\n", VEH_MAX_ROOMS(veh), temp);
	}
	
	// 'R': resources
	write_resources_to_file(fl, VEH_YEARLY_MAINTENANCE(veh));
	
	// end
	fprintf(fl, "S\n");
}


 //////////////////////////////////////////////////////////////////////////////
//// OLC HANDLERS ////////////////////////////////////////////////////////////


/**
* Creates a new vehicle entry.
* 
* @param any_vnum vnum The number to create.
* @return vehicle_data* The new vehicle's prototype.
*/
vehicle_data *create_vehicle_table_entry(any_vnum vnum) {
	vehicle_data *veh;
	
	// sanity
	if (vehicle_proto(vnum)) {
		log("SYSERR: Attempting to insert vehicle at existing vnum %d", vnum);
		return vehicle_proto(vnum);
	}
	
	CREATE(veh, vehicle_data, 1);
	clear_vehicle(veh);
	VEH_VNUM(veh) = vnum;
	
	VEH_KEYWORDS(veh) = str_dup(default_vehicle_keywords);
	VEH_SHORT_DESC(veh) = str_dup(default_vehicle_short_desc);
	VEH_LONG_DESC(veh) = str_dup(default_vehicle_long_desc);
	add_vehicle_to_table(veh);

	// save index and vehicle file now
	save_index(DB_BOOT_VEH);
	save_library_file_for_vnum(DB_BOOT_VEH, vnum);

	return veh;
}


/**
* WARNING: This function actually deletes a vehicle.
*
* @param char_data *ch The person doing the deleting.
* @param any_vnum vnum The vnum to delete.
*/
void olc_delete_vehicle(char_data *ch, any_vnum vnum) {
	vehicle_data *veh, *iter, *next_iter;
	
	if (!(veh = vehicle_proto(vnum))) {
		msg_to_char(ch, "There is no such vehicle %d.\r\n", vnum);
		return;
	}
	
	// remove live vehicles
	LL_FOREACH_SAFE(vehicle_list, iter, next_iter) {
		if (VEH_VNUM(iter) != vnum) {
			continue;
		}
		
		if (ROOM_PEOPLE(IN_ROOM(iter))) {
			act("$V vanishes.", FALSE, ROOM_PEOPLE(IN_ROOM(iter)), NULL, iter, TO_CHAR | TO_ROOM);
		}
		extract_vehicle(iter);
	}
	
	// remove it from the hash table first
	remove_vehicle_from_table(veh);
	
	// save index and vehicle file now
	save_index(DB_BOOT_VEH);
	save_library_file_for_vnum(DB_BOOT_VEH, vnum);
	
	syslog(SYS_OLC, GET_INVIS_LEV(ch), TRUE, "OLC: %s has deleted vehicle %d", GET_NAME(ch), vnum);
	msg_to_char(ch, "Vehicle %d deleted.\r\n", vnum);
	
	free_vehicle(veh);
}


/**
* Function to save a player's changes to a vehicle (or a new one).
*
* @param descriptor_data *desc The descriptor who is saving.
*/
void save_olc_vehicle(descriptor_data *desc) {	
	vehicle_data *proto, *veh = GET_OLC_VEHICLE(desc), *iter;
	any_vnum vnum = GET_OLC_VNUM(desc);
	UT_hash_handle hh;

	// have a place to save it?
	if (!(proto = vehicle_proto(vnum))) {
		proto = create_vehicle_table_entry(vnum);
	}
	
	// sanity
	if (!VEH_KEYWORDS(veh) || !*VEH_KEYWORDS(veh)) {
		if (VEH_KEYWORDS(veh)) {
			free(VEH_KEYWORDS(veh));
		}
		VEH_KEYWORDS(veh) = str_dup(default_vehicle_keywords);
	}
	if (!VEH_SHORT_DESC(veh) || !*VEH_SHORT_DESC(veh)) {
		if (VEH_SHORT_DESC(veh)) {
			free(VEH_SHORT_DESC(veh));
		}
		VEH_SHORT_DESC(veh) = str_dup(default_vehicle_short_desc);
	}
	if (!VEH_LONG_DESC(veh) || !*VEH_LONG_DESC(veh)) {
		if (VEH_LONG_DESC(veh)) {
			free(VEH_LONG_DESC(veh));
		}
		VEH_LONG_DESC(veh) = str_dup(default_vehicle_long_desc);
	}
	if (VEH_ICON(veh) && !*VEH_ICON(veh)) {
		free(VEH_ICON(veh));
		VEH_ICON(veh) = NULL;
	}
	VEH_HEALTH(veh) = VEH_MAX_HEALTH(veh);
	
	// update live vehicles
	LL_FOREACH(vehicle_list, iter) {
		if (VEH_VNUM(iter) != vnum) {
			continue;
		}
		
		// update pointers
		if (VEH_KEYWORDS(iter) == VEH_KEYWORDS(proto)) {
			VEH_KEYWORDS(iter) = VEH_KEYWORDS(veh);
		}
		if (VEH_SHORT_DESC(iter) == VEH_SHORT_DESC(proto)) {
			VEH_SHORT_DESC(iter) = VEH_SHORT_DESC(veh);
		}
		if (VEH_ICON(iter) == VEH_ICON(proto)) {
			VEH_ICON(iter) = VEH_ICON(veh);
		}
		if (VEH_LONG_DESC(iter) == VEH_LONG_DESC(proto)) {
			VEH_LONG_DESC(iter) = VEH_LONG_DESC(veh);
		}
		if (VEH_LOOK_DESC(iter) == VEH_LOOK_DESC(proto)) {
			VEH_LOOK_DESC(iter) = VEH_LOOK_DESC(veh);
		}
		if (iter->attributes == proto->attributes) {
			iter->attributes = veh->attributes;
		}
		
		// sanity checks
		if (VEH_HEALTH(iter) > VEH_MAX_HEALTH(iter)) {
			VEH_HEALTH(iter) = VEH_MAX_HEALTH(iter);
		}
	}
	
	// free prototype strings and pointers
	if (VEH_KEYWORDS(proto)) {
		free(VEH_KEYWORDS(proto));
	}
	if (VEH_SHORT_DESC(proto)) {
		free(VEH_SHORT_DESC(proto));
	}
	if (VEH_ICON(proto)) {
		free(VEH_ICON(proto));
	}
	if (VEH_LONG_DESC(proto)) {
		free(VEH_LONG_DESC(proto));
	}
	if (VEH_LOOK_DESC(proto)) {
		free(VEH_LOOK_DESC(proto));
	}
	if (VEH_YEARLY_MAINTENANCE(proto)) {
		free_resource_list(VEH_YEARLY_MAINTENANCE(proto));
	}
	free(proto->attributes);

	// save data back over the proto-type
	hh = proto->hh;	// save old hash handle
	*proto = *veh;	// copy over all data
	proto->vnum = vnum;	// ensure correct vnum
	proto->hh = hh;	// restore old hash handle
		
	// and save to file
	save_library_file_for_vnum(DB_BOOT_VEH, vnum);
}


/**
* Creates a copy of a vehicle, or clears a new one, for editing.
* 
* @param vehicle_data *input The vehicle to copy, or NULL to make a new one.
* @return vehicle_data* The copied vehicle.
*/
vehicle_data *setup_olc_vehicle(vehicle_data *input) {
	vehicle_data *new;
	
	CREATE(new, vehicle_data, 1);
	clear_vehicle(new);
	
	if (input) {
		free(new->attributes);	// created by clear_vehicle
		
		// copy normal data
		*new = *input;
		CREATE(new->attributes, struct vehicle_attribute_data, 1);
		*(new->attributes) = *(input->attributes);

		// copy things that are pointers
		VEH_KEYWORDS(new) = VEH_KEYWORDS(input) ? str_dup(VEH_KEYWORDS(input)) : NULL;
		VEH_SHORT_DESC(new) = VEH_SHORT_DESC(input) ? str_dup(VEH_SHORT_DESC(input)) : NULL;
		VEH_ICON(new) = VEH_ICON(input) ? str_dup(VEH_ICON(input)) : NULL;
		VEH_LONG_DESC(new) = VEH_LONG_DESC(input) ? str_dup(VEH_LONG_DESC(input)) : NULL;
		VEH_LOOK_DESC(new) = VEH_LOOK_DESC(input) ? str_dup(VEH_LOOK_DESC(input)) : NULL;
		
		// copy lists
		VEH_YEARLY_MAINTENANCE(new) = copy_resource_list(VEH_YEARLY_MAINTENANCE(input));
	}
	else {
		// brand new: some defaults
		VEH_KEYWORDS(new) = str_dup(default_vehicle_keywords);
		VEH_SHORT_DESC(new) = str_dup(default_vehicle_short_desc);
		VEH_LONG_DESC(new) = str_dup(default_vehicle_long_desc);
		VEH_MAX_HEALTH(new) = 1;
		VEH_MOVE_TYPE(new) = MOB_MOVE_DRIVES;
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
* @param vehicle_data *veh The vehicle to display.
*/
void do_stat_vehicle(char_data *ch, vehicle_data *veh) {
	char buf[MAX_STRING_LENGTH], part[MAX_STRING_LENGTH];
	obj_data *obj;
	size_t size;
	int found;
	
	if (!veh) {
		return;
	}
	
	// first line
	size = snprintf(buf, sizeof(buf), "VNum: [\tc%d\t0], S-Des: \tc%s\t0, Keywords: %s\r\n", VEH_VNUM(veh), VEH_SHORT_DESC(veh), VEH_KEYWORDS(veh));
	
	size += snprintf(buf + size, sizeof(buf) - size, "L-Des: %s\r\n", VEH_LONG_DESC(veh));
	
	if (VEH_LOOK_DESC(veh) && *VEH_LOOK_DESC(veh)) {
		size += snprintf(buf + size, sizeof(buf) - size, "%s", VEH_LOOK_DESC(veh));
	}
	
	if (VEH_ICON(veh)) {
		size += snprintf(buf + size, sizeof(buf) - size, "Map Icon: %s %s\r\n", VEH_ICON(veh), show_color_codes(VEH_ICON(veh)));
	}
	
	size += snprintf(buf + size, sizeof(buf) - size, "Health: [\tc%d\t0/\tc%d\t0], Capacity: [\tc%d\t0/\tc%d\t0], Animals Req: [\tc%d\t0], Move Type: [\ty%s\t0]\r\n", VEH_HEALTH(veh), VEH_MAX_HEALTH(veh), VEH_CARRYING_N(veh), VEH_CAPACITY(veh), VEH_ANIMALS_REQUIRED(veh), mob_move_types[VEH_MOVE_TYPE(veh)]);
	
	if (VEH_MAX_ROOMS(veh) || VEH_DESIGNATE_FLAGS(veh)) {
		sprintbit(VEH_DESIGNATE_FLAGS(veh), designate_flags, part, TRUE);
		size += snprintf(buf + size, sizeof(buf) - size, "Max Rooms: [\tc%d\t0], Designate: \ty%s\t0\r\n", VEH_MAX_ROOMS(veh), part);
	}
	
	sprintbit(VEH_FLAGS(veh), vehicle_flags, part, TRUE);
	size += snprintf(buf + size, sizeof(buf) - size, "Flags: \tg%s\t0\r\n", part);
	
	if (VEH_YEARLY_MAINTENANCE(veh)) {
		get_resource_display(VEH_YEARLY_MAINTENANCE(veh), part);
		size += snprintf(buf + size, sizeof(buf) - size, "Yearly maintenance:\r\n%s", part);
	}
	
	if (VEH_OWNER(veh) || VEH_SCALE_LEVEL(veh)) {
		size += snprintf(buf + size, sizeof(buf) - size, "Scaled to level: [\tc%d\t0], Owner: [%s%s\t0]\r\n", VEH_SCALE_LEVEL(veh), EMPIRE_BANNER(VEH_OWNER(veh)), EMPIRE_NAME(VEH_OWNER(veh)));
	}
	
	if (VEH_CONTAINS(veh)) {
		sprintf(part, "Contents:\tg");
		found = 0;
		LL_FOREACH2(VEH_CONTAINS(veh), obj, next_content) {
			sprintf(part + strlen(part), "%s %s", found++ ? "," : "", GET_OBJ_DESC(obj, ch, OBJ_DESC_SHORT));
			if (strlen(part) >= 62) {
				if (obj->next_content) {
					strcat(part, ",");
				}
				size += snprintf(buf + size, sizeof(buf) - size, "%s\r\n", part);
				*part = '\0';
				found = 0;
			}
		}
		if (*part) {
			size += snprintf(buf + size, sizeof(buf) - size, "%s\t0\r\n", part);
		}
		else {
			size += snprintf(buf + size, sizeof(buf) - size, "\t0");
		}
	}
	
	if (VEH_NEEDS_RESOURCES(veh)) {
		get_resource_display(VEH_NEEDS_RESOURCES(veh), part);
		size += snprintf(buf + size, sizeof(buf) - size, "Needs resources:\r\n%s", part);
	}
		
	page_string(ch->desc, buf, TRUE);
}


/**
* Perform a look-at-vehicle.
*
* @param vehicle_data *veh The vehicle to look at.
* @param char_data *ch The person to show the output to.
*/
void look_at_vehicle(vehicle_data *veh, char_data *ch) {
	if (!veh || !ch || !ch->desc) {
		return;
	}
	
	if (VEH_LOOK_DESC(veh) && *VEH_LOOK_DESC(veh)) {
		msg_to_char(ch, "%s", VEH_LOOK_DESC(veh));
	}
	else {
		act("You look at $V but see nothing special.", FALSE, ch, NULL, veh, TO_CHAR);
	}
}


/**
* This is the main recipe display for vehicle OLC. It displays the user's
* currently-edited vehicle.
*
* @param char_data *ch The person who is editing a vehicle and will see its display.
*/
void olc_show_vehicle(char_data *ch) {
	vehicle_data *veh = GET_OLC_VEHICLE(ch->desc);
	char buf[MAX_STRING_LENGTH], lbuf[MAX_STRING_LENGTH];
	
	if (!veh) {
		return;
	}
	
	*buf = '\0';
	
	sprintf(buf + strlen(buf), "[\tc%d\t0] \tc%s\t0\r\n", GET_OLC_VNUM(ch->desc), !vehicle_proto(VEH_VNUM(veh)) ? "new vehicle" : VEH_SHORT_DESC(vehicle_proto(VEH_VNUM(veh))));

	sprintf(buf + strlen(buf), "<\tykeywords\t0> %s\r\n", NULLSAFE(VEH_KEYWORDS(veh)));
	sprintf(buf + strlen(buf), "<\tyshortdescription\t0> %s\r\n", NULLSAFE(VEH_SHORT_DESC(veh)));
	sprintf(buf + strlen(buf), "<\tylongdescription\t0>\r\n%s\r\n", NULLSAFE(VEH_LONG_DESC(veh)));
	sprintf(buf + strlen(buf), "<\tylookdescription\t0>\r\n%s", NULLSAFE(VEH_LOOK_DESC(veh)));
	sprintf(buf + strlen(buf), "<\tyicon\t0> %s %s\r\n", VEH_ICON(veh) ? VEH_ICON(veh) : "none", VEH_ICON(veh) ? show_color_codes(VEH_ICON(veh)) : "");
	
	sprintbit(VEH_FLAGS(veh), vehicle_flags, lbuf, TRUE);
	sprintf(buf + strlen(buf), "<\tyflags\t0> %s\r\n", lbuf);
	
	sprintf(buf + strlen(buf), "<\tyhitpoints\t0> %d\r\n", VEH_MAX_HEALTH(veh));
	sprintf(buf + strlen(buf), "<\tymovetype\t0> %s\r\n", mob_move_types[VEH_MOVE_TYPE(veh)]);
	sprintf(buf + strlen(buf), "<\tycapacity\t0> %d item%s\r\n", VEH_CAPACITY(veh), PLURAL(VEH_CAPACITY(veh)));
	sprintf(buf + strlen(buf), "<\tyanimalsrequired\t0> %d\r\n", VEH_ANIMALS_REQUIRED(veh));

	sprintf(buf + strlen(buf), "<\tymaxrooms\t0> %d\r\n", VEH_MAX_ROOMS(veh));
	sprintbit(VEH_DESIGNATE_FLAGS(veh), designate_flags, lbuf, TRUE);
	sprintf(buf + strlen(buf), "<\tydesignate\t0> %s\r\n", lbuf);
	
	// maintenance resources
	sprintf(buf + strlen(buf), "Yearly maintenance resources required: <\tyresource\t0>\r\n");
	get_resource_display(VEH_YEARLY_MAINTENANCE(veh), lbuf);
	strcat(buf, lbuf);
	
	page_string(ch->desc, buf, TRUE);
}


/**
* Searches the vehicle db for a match, and prints it to the character.
*
* @param char *searchname The search string.
* @param char_data *ch The player who is searching.
* @return int The number of matches shown.
*/
int vnum_vehicle(char *searchname, char_data *ch) {
	vehicle_data *iter, *next_iter;
	int found = 0;
	
	HASH_ITER(hh, vehicle_table, iter, next_iter) {
		if (multi_isname(searchname, VEH_KEYWORDS(iter))) {
			msg_to_char(ch, "%3d. [%5d] %s\r\n", ++found, VEH_VNUM(iter), VEH_SHORT_DESC(iter));
		}
	}
	
	return found;
}


 //////////////////////////////////////////////////////////////////////////////
//// OLC MODULES /////////////////////////////////////////////////////////////

OLC_MODULE(vedit_animalsrequired) {
	vehicle_data *veh = GET_OLC_VEHICLE(ch->desc);
	VEH_ANIMALS_REQUIRED(veh) = olc_process_number(ch, argument, "animals required", "animalsrequired", 0, 100, VEH_ANIMALS_REQUIRED(veh));
}


OLC_MODULE(vedit_capacity) {
	vehicle_data *veh = GET_OLC_VEHICLE(ch->desc);
	VEH_CAPACITY(veh) = olc_process_number(ch, argument, "capacity", "capacity", 0, 10000, VEH_CAPACITY(veh));
}


OLC_MODULE(vedit_designate) {
	vehicle_data *veh = GET_OLC_VEHICLE(ch->desc);
	VEH_DESIGNATE_FLAGS(veh) = olc_process_flag(ch, argument, "designate", "designate", designate_flags, VEH_DESIGNATE_FLAGS(veh));
}


OLC_MODULE(vedit_flags) {
	vehicle_data *veh = GET_OLC_VEHICLE(ch->desc);
	VEH_FLAGS(veh) = olc_process_flag(ch, argument, "vehicle", "flags", vehicle_flags, VEH_FLAGS(veh));
}


OLC_MODULE(vedit_hitpoints) {
	vehicle_data *veh = GET_OLC_VEHICLE(ch->desc);
	VEH_MAX_HEALTH(veh) = olc_process_number(ch, argument, "hitpoints", "hitpoints", 1, 1000, VEH_MAX_HEALTH(veh));
}


OLC_MODULE(vedit_icon) {
	extern bool validate_icon(char *icon);

	vehicle_data *veh = GET_OLC_VEHICLE(ch->desc);
	
	delete_doubledollar(argument);
	
	if (!str_cmp(argument, "none")) {
		if (VEH_ICON(veh)) {
			free(VEH_ICON(veh));
		}
		VEH_ICON(veh) = NULL;
		msg_to_char(ch, "The vehicle now has no icon and will not appear on the map.\r\n");
	}
	else if (!validate_icon(argument)) {
		msg_to_char(ch, "You must specify an icon that is 4 characters long, not counting color codes.\r\n");
	}
	else {
		olc_process_string(ch, argument, "icon", &VEH_ICON(veh));
	}
}


OLC_MODULE(vedit_keywords) {
	vehicle_data *veh = GET_OLC_VEHICLE(ch->desc);
	olc_process_string(ch, argument, "keywords", &VEH_KEYWORDS(veh));
}


OLC_MODULE(vedit_longdescription) {
	vehicle_data *veh = GET_OLC_VEHICLE(ch->desc);
	olc_process_string(ch, argument, "long description", &VEH_LONG_DESC(veh));
}


OLC_MODULE(vedit_lookdescription) {
	vehicle_data *veh = GET_OLC_VEHICLE(ch->desc);
	
	if (ch->desc->str) {
		msg_to_char(ch, "You are already editing a string.\r\n");
	}
	else {
		sprintf(buf, "description for %s", VEH_SHORT_DESC(veh));
		start_string_editor(ch->desc, buf, &VEH_LOOK_DESC(veh), MAX_ITEM_DESCRIPTION);
	}
}


OLC_MODULE(vedit_maxrooms) {
	vehicle_data *veh = GET_OLC_VEHICLE(ch->desc);
	VEH_MAX_ROOMS(veh) = olc_process_number(ch, argument, "max rooms", "maxrooms", 0, 1000, VEH_MAX_ROOMS(veh));
}


OLC_MODULE(vedit_movetype) {
	vehicle_data *veh = GET_OLC_VEHICLE(ch->desc);
	VEH_MOVE_TYPE(veh) = olc_process_type(ch, argument, "move type", "movetype", mob_move_types, VEH_MOVE_TYPE(veh));
}


OLC_MODULE(vedit_resource) {
	void olc_process_resources(char_data *ch, char *argument, struct resource_data **list);
	vehicle_data *veh = GET_OLC_VEHICLE(ch->desc);
	olc_process_resources(ch, argument, &VEH_YEARLY_MAINTENANCE(veh));
}


OLC_MODULE(vedit_shortdescription) {
	vehicle_data *veh = GET_OLC_VEHICLE(ch->desc);
	olc_process_string(ch, argument, "short description", &VEH_SHORT_DESC(veh));
}
