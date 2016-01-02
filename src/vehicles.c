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
*   2.0b3.8 Converter
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
extern room_data *create_room();

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
* Empties the contents (items) of a vehicle into the room it's in (if any) or
* extracts them.
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
* Removes everyone/everything from inside a vehicle, and puts it on the outside
* if possible.
*
* @param vehicle_data *veh The vehicle to empty.
*/
void fully_empty_vehicle(vehicle_data *veh) {
	vehicle_data *iter, *next_iter;
	obj_data *obj, *next_obj;
	char_data *ch, *next_ch;
	room_data *room;
	
	if (VEH_INTERIOR_HOME_ROOM(veh)) {
		LL_FOREACH2(interior_room_list, room, next_interior) {
			if (HOME_ROOM(room) != VEH_INTERIOR_HOME_ROOM(veh)) {
				continue;
			}
			
			// remove people
			LL_FOREACH_SAFE2(ROOM_PEOPLE(room), ch, next_ch, next_in_room) {
				act("You are ejected from $V!", FALSE, ch, NULL, veh, TO_CHAR);
				if (IN_ROOM(veh)) {
					char_to_room(ch, IN_ROOM(veh));
					look_at_room(ch);
					act("$n is ejected from $V!", TRUE, ch, NULL, veh, TO_ROOM);
				}
				else {
					extract_char(ch);
				}
			}
			
			// remove items
			LL_FOREACH_SAFE2(ROOM_CONTENTS(room), obj, next_obj, next_content) {
				if (IN_ROOM(veh)) {
					obj_to_room(obj, IN_ROOM(veh));
				}
				else {
					extract_obj(obj);
				}
			}
			
			// remove other vehicles
			LL_FOREACH_SAFE2(ROOM_VEHICLES(room), iter, next_iter, next_in_room) {
				if (IN_ROOM(veh)) {
					vehicle_to_room(iter, IN_ROOM(veh));
				}
				else {
					extract_vehicle(iter);
				}
			}
		}
	}
	
	// dump contents
	empty_vehicle(veh);
}


/**
* This returns (or creates, if necessary) the start of the interior of the
* vehicle. Some vehicles don't have this feature.
*
* @param vehicle_data *veh The vehicle to get the interior for.
* @return room_data* The interior home room, if it exists (may be NULL).
*/
room_data *get_vehicle_interior(vehicle_data *veh) {
	room_data *room;
	bld_data *bld;
	
	// already have one?
	if (VEH_INTERIOR_HOME_ROOM(veh)) {
		return VEH_INTERIOR_HOME_ROOM(veh);
	}
	// this vehicle has no interior available
	if (!VEH_IS_COMPLETE(veh) || !(bld = building_proto(VEH_INTERIOR_ROOM_VNUM(veh)))) {
		return NULL;
	}
	
	// otherwise, create the interior
	room = create_room();
	attach_building_to_room(bld, room);
	ROOM_OWNER(room) = VEH_OWNER(veh);
	COMPLEX_DATA(room)->home_room = NULL;
	SET_BIT(ROOM_AFF_FLAGS(room), ROOM_AFF_IN_VEHICLE);
	SET_BIT(ROOM_BASE_FLAGS(room), ROOM_AFF_IN_VEHICLE);
	
	// attach
	COMPLEX_DATA(room)->vehicle = veh;
	VEH_INTERIOR_HOME_ROOM(veh) = room;
		
	return room;
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
* Finds a vehicle that will be shown in the room. The vehicle must have an icon
* to qualify for this, and must also be complete. Vehicles in buildings are
* never shown. Only the first valid vehicle is returned.
*
* @param char_data *ch The player looking.
* @param room_data *room The room to check.
* @return vehicle_data* A vehicle to show, if any (NULL if not).
*/
vehicle_data *find_vehicle_to_show(char_data *ch, room_data *room) {
	vehicle_data *iter, *in_veh;
	bool is_on_vehicle = ((in_veh = GET_ROOM_VEHICLE(IN_ROOM(ch))) && room == IN_ROOM(in_veh));
	
	// we don't show vehicles in buildings or closed tiles (unless the player is on a vehicle in that room, in which case we override)
	if (!is_on_vehicle && (IS_ANY_BUILDING(room) || ROOM_IS_CLOSED(room))) {
		return NULL;
	}
	
	LL_FOREACH2(ROOM_VEHICLES(room), iter, next_in_room) {
		if (!VEH_ICON(iter) || !*VEH_ICON(iter)) {
			continue;	// no icon
		}
		if (!VEH_IS_COMPLETE(iter)) {
			continue;	// skip incomplete
		}
		
		// we'll show the first match
		return iter;
	}
	
	// nothing found
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
	struct vehicle_attached_mob *iter, *next_iter, *tmp;
	int count, num = 0;
	size_t size = 0;
	char mult[256];
	bool skip;
	
	*output = '\0';
	
	LL_FOREACH_SAFE(VEH_ANIMALS(veh), iter, next_iter) {
		// stacking: determine if already listed
		skip = FALSE;
		count = 1;
		LL_FOREACH(VEH_ANIMALS(veh), tmp) {
			if (tmp == iter) {
				break;	// stop when we find this one
			}
			if (tmp->mob == iter->mob) {
				skip = TRUE;
				break;
			}
		}
		if (skip) {
			continue;	// already showed this one
		}
		
		// count how many to show
		count = 1;	// this one
		LL_FOREACH(iter->next, tmp) {
			if (tmp->mob == iter->mob) {
				++count;
			}
		}
		
		if (count > 1) {
			snprintf(mult, sizeof(mult), " (x%d)", count);
		}
		else {
			*mult = '\0';
		}
	
		size += snprintf(output+size, sizeof(output)-size, "%s%s%s", ((num == 0) ? "" : (next_iter ? ", " : (num > 1 ? ", and " : " and "))), get_mob_name_by_proto(iter->mob), mult);
		++num;
	}
	
	return output;
}


/**
* @param vehicle_data *veh The vehicle to scale.
* @param int level What level to scale it to (passing 0 will trigger auto-detection).
*/
void scale_vehicle_to_level(vehicle_data *veh, int level) {
	struct instance_data *inst = NULL;
	
	// detect level if we weren't given a strong level
	if (!level) {
		if (IN_ROOM(veh) && (inst = ROOM_INSTANCE(IN_ROOM(veh)))) {
			if (inst->level > 0) {
				level = inst->level;
			}
		}
	}
	
	// constraints
	if (inst || (IN_ROOM(veh) && (inst = ROOM_INSTANCE(IN_ROOM(veh))))) {
		if (GET_ADV_MIN_LEVEL(inst->adventure) > 0) {
			level = MAX(level, GET_ADV_MIN_LEVEL(inst->adventure));
		}
		if (GET_ADV_MAX_LEVEL(inst->adventure) > 0) {
			level = MIN(level, GET_ADV_MAX_LEVEL(inst->adventure));
		}
	}
	
	// set the level
	VEH_SCALE_LEVEL(veh) = level;
}


/**
* Sets a vehicle ablaze!
*
* @param vehicle_data *veh The vehicle to ignite.
*/
void start_vehicle_burning(vehicle_data *veh) {
	if (VEH_OWNER(veh)) {
		log_to_empire(VEH_OWNER(veh), ELOG_HOSTILITY, "Your %s has caught on fire at (%d, %d)", skip_filler(VEH_SHORT_DESC(veh)), X_COORD(IN_ROOM(veh)), Y_COORD(IN_ROOM(veh)));
	}
	msg_to_vehicle(veh, TRUE, "It seems %s has caught fire!\r\n", VEH_SHORT_DESC(veh));
	SET_BIT(VEH_FLAGS(veh), VEH_ON_FIRE);

	if (VEH_SITTING_ON(veh)) {
		do_unseat_from_vehicle(VEH_SITTING_ON(veh));
	}
	if (VEH_LED_BY(veh)) {
		act("You stop leading $V.", FALSE, VEH_LED_BY(veh), NULL, veh, TO_CHAR);
		GET_LEADING_VEHICLE(VEH_LED_BY(veh)) = NULL;
		VEH_LED_BY(veh) = NULL;
	}
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
* Saves the vehicles list for a room to the room file.
*
* @param vehicle_data *room_list The list of vehicles in the room.
* @param FILE *fl The file open for writing.
*/
void Crash_save_vehicles(vehicle_data *room_list, FILE *fl) {
	void store_one_vehicle_to_file(vehicle_data *veh, FILE *fl);
	
	vehicle_data *iter;
	
	LL_FOREACH2(room_list, iter, next_in_room) {
		store_one_vehicle_to_file(iter, fl);
	}
}


/**
* Quick way to turn a vnum into a name, safely.
*
* @param any_vnum vnum The vehicle vnum to look up.
* @return char* A name for the vnum, or "UNKNOWN".
*/
char *get_vehicle_name_by_proto(obj_vnum vnum) {
	vehicle_data *proto = vehicle_proto(vnum);
	return proto ? VEH_SHORT_DESC(proto) : "UNKNOWN";
}


/**
* Looks for dead vehicle interiors at startup, and deletes them.
*/
void link_and_check_vehicles(void) {
	vehicle_data *veh, *next_veh;
	room_data *room, *next_room;
	bool found = FALSE;
	
	// reverse-link the home-room of vehicles to this one
	LL_FOREACH_SAFE(vehicle_list, veh, next_veh) {
		if (VEH_INTERIOR_HOME_ROOM(veh)) {
			COMPLEX_DATA(VEH_INTERIOR_HOME_ROOM(veh))->vehicle = veh;
		}
	}
	
	// check for orphaned ship rooms
	LL_FOREACH_SAFE2(interior_room_list, room, next_room, next_interior) {
		if (ROOM_AFF_FLAGGED(room, ROOM_AFF_IN_VEHICLE) && HOME_ROOM(room) == room && !GET_ROOM_VEHICLE(room)) {
			delete_room(room, FALSE);	// must check_all_exits later
			found = TRUE;
		}
	}
	
	// only bother this if we deleted anything
	if (found) {
		check_all_exits();
		read_empire_territory(NULL);
	}
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
	craft_data *craft, *next_craft;
	int size, found;
	
	if (!veh) {
		msg_to_char(ch, "There is no vehicle %d.\r\n", vnum);
		return;
	}
	
	found = 0;
	size = snprintf(buf, sizeof(buf), "Occurrences of vehicle %d (%s):\r\n", vnum, VEH_SHORT_DESC(veh));
	
	// crafts
	HASH_ITER(hh, craft_table, craft, next_craft) {
		if (CRAFT_FLAGGED(craft, CRAFT_VEHICLE) && GET_CRAFT_OBJECT(craft) == vnum) {
			++found;
			size += snprintf(buf + size, sizeof(buf) - size, "CFT [%5d] %s\r\n", GET_CRAFT_VNUM(craft), GET_CRAFT_NAME(craft));
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


/**
* write_one_vehicle_to_file: Write a vehicle to a tagged save file. Vehicle
* tags start with %VNUM instead of #VNUM because they may co-exist with items
* in the file.
*
* @param vehicle_data *veh The vehicle to save.
* @param FILE *fl The file to save to (open for writing).
*/
void store_one_vehicle_to_file(vehicle_data *veh, FILE *fl) {
	void Crash_save(obj_data *obj, FILE *fp, int location);
	
	struct vehicle_attached_mob *vam;
	char temp[MAX_STRING_LENGTH];
	struct resource_data *res;
	vehicle_data *proto;
	
	if (!fl || !veh) {
		log("SYSERR: write_one_vehicle_to_file called without %s", fl ? "vehicle" : "file");
		return;
	}
	
	proto = vehicle_proto(VEH_VNUM(veh));
	
	fprintf(fl, "%%%d\n", VEH_VNUM(veh));
	fprintf(fl, "Flags: %s\n", bitv_to_alpha(VEH_FLAGS(veh)));

	if (!proto || VEH_KEYWORDS(veh) != VEH_KEYWORDS(proto)) {
		fprintf(fl, "Keywords:\n%s~\n", NULLSAFE(VEH_KEYWORDS(veh)));
	}
	if (!proto || VEH_SHORT_DESC(veh) != VEH_SHORT_DESC(proto)) {
		fprintf(fl, "Short-desc:\n%s~\n", NULLSAFE(VEH_SHORT_DESC(veh)));
	}
	if (!proto || VEH_LONG_DESC(veh) != VEH_LONG_DESC(proto)) {
		fprintf(fl, "Long-desc:\n%s~\n", NULLSAFE(VEH_LONG_DESC(veh)));
	}
	if (!proto || VEH_LOOK_DESC(veh) != VEH_LOOK_DESC(proto)) {
		strcpy(temp, NULLSAFE(VEH_LOOK_DESC(veh)));
		strip_crlf(temp);
		fprintf(fl, "Look-desc:\n%s~\n", temp);
	}
	if (!proto || VEH_ICON(veh) != VEH_ICON(proto)) {
		fprintf(fl, "Icon:\n%s~\n", NULLSAFE(VEH_ICON(veh)));
	}

	if (VEH_OWNER(veh)) {
		fprintf(fl, "Owner: %d\n", EMPIRE_VNUM(VEH_OWNER(veh)));
	}
	if (VEH_SCALE_LEVEL(veh)) {
		fprintf(fl, "Scale: %d\n", VEH_SCALE_LEVEL(veh));
	}
	if (VEH_HEALTH(veh) != VEH_MAX_HEALTH(veh)) {
		fprintf(fl, "Health: %d\n", VEH_HEALTH(veh));
	}
	if (VEH_INTERIOR_HOME_ROOM(veh)) {
		fprintf(fl, "Interior-home: %d\n", GET_ROOM_VNUM(VEH_INTERIOR_HOME_ROOM(veh)));
	}
	if (VEH_CONTAINS(veh)) {
		fprintf(fl, "Contents:\n");
		Crash_save(VEH_CONTAINS(veh), fl, LOC_INVENTORY);
		fprintf(fl, "Contents-end\n");
	}
	LL_FOREACH(VEH_ANIMALS(veh), vam) {
		fprintf(fl, "Animal: %d %d %s %d\n", vam->mob, vam->scale_level, bitv_to_alpha(vam->flags), vam->empire);
	}
	LL_FOREACH(VEH_NEEDS_RESOURCES(veh), res) {
		fprintf(fl, "Needs-res: %d %d\n", res->vnum, res->amount);
	}
	
	// scripts
	if (SCRIPT(veh)) {
		trig_data *trig;
		
		for (trig = TRIGGERS(SCRIPT(veh)); trig; trig = trig->next) {
			fprintf(fl, "Trigger: %d\n", GET_TRIG_VNUM(trig));
		}
		
		// TODO could save SCRIPT(obj)->global_vars here too
	}
	
	fprintf(fl, "Vehicle-end\n");
}


/**
* Reads a vehicle from a tagged data file.
*
* @param FILE *fl The file open for reading, just after the %VNUM line.
* @param any_vnum vnum The vnum already read from the file.
* @return vehicle_data* The loaded vehicle, if possible.
*/
vehicle_data *unstore_vehicle_from_file(FILE *fl, any_vnum vnum) {
	extern obj_data *Obj_load_from_file(FILE *fl, obj_vnum vnum, int *location, char_data *notify);

	char line[MAX_INPUT_LENGTH], error[MAX_STRING_LENGTH], s_in[MAX_INPUT_LENGTH];
	obj_data *load_obj, *obj2, *cont_row[MAX_BAG_ROWS];
	struct vehicle_attached_mob *vam, *last_vam = NULL;
	int length, iter, i_in[3], location = 0, timer;
	struct resource_data *res, *last_res = NULL;
	vehicle_data *proto = vehicle_proto(vnum);
	bool end = FALSE, seek_end = FALSE;
	any_vnum load_vnum;
	vehicle_data *veh;
	
	// load based on vnum or, if NOTHING, create anonymous object
	if (proto) {
		veh = read_vehicle(vnum, FALSE);
	}
	else {
		veh = NULL;
		seek_end = TRUE;	// signal it to skip the whole vehicle
	}
		
	// for fread_string
	sprintf(error, "unstore_vehicle_from_file %d", vnum);
	
	// for more readable if/else chain	
	#define OBJ_FILE_TAG(src, tag, len)  (!strn_cmp((src), (tag), ((len) = strlen(tag))))

	while (!end) {
		if (!get_line(fl, line)) {
			log("SYSERR: Unexpected end of pack file in unstore_vehicle_from_file");
			exit(1);
		}
		
		if (OBJ_FILE_TAG(line, "Vehicle-end", length)) {
			end = TRUE;
			continue;
		}
		else if (seek_end) {
			// are we looking for the end of the vehicle? ignore this line
			// WARNING: don't put any ifs that require "veh" above seek_end; obj is not guaranteed
			continue;
		}
		
		// normal tags by letter
		switch (UPPER(*line)) {
			case 'A': {
				if (OBJ_FILE_TAG(line, "Animal:", length)) {
					if (sscanf(line + length + 1, "%d %d %s %d", &i_in[0], &i_in[1], s_in, &i_in[2]) == 4) {
						CREATE(vam, struct vehicle_attached_mob, 1);
						vam->mob = i_in[0];
						vam->scale_level = i_in[1];
						vam->flags = asciiflag_conv(s_in);
						vam->empire = i_in[2];
						
						// append
						if (last_vam) {
							last_vam->next = vam;
						}
						else {
							VEH_ANIMALS(veh) = vam;
						}
						last_vam = vam;
					}
				}
				break;
			}
			case 'C': {
				if (OBJ_FILE_TAG(line, "Contents:", length)) {
					// empty container lists
					for (iter = 0; iter < MAX_BAG_ROWS; iter++) {
						cont_row[iter] = NULL;
					}

					// load contents until we find an end
					for (;;) {
						if (!get_line(fl, line)) {
							log("SYSERR: Format error in pack file with vehicle %d", vnum);
							return NULL;
						}
						
						if (*line == '#') {
							if (sscanf(line, "#%d", &load_vnum) < 1) {
								log("SYSERR: Format error in vnum line of pack file with vehicle %d", vnum);
								return NULL;
							}
							if ((load_obj = Obj_load_from_file(fl, load_vnum, &location, NULL))) {
								// Obj_load_from_file may return a NULL for deleted objs
				
								// Not really an inventory, but same idea.
								if (location > 0) {
									location = LOC_INVENTORY;
								}

								// store autostore timer through obj_to_room
								timer = GET_AUTOSTORE_TIMER(load_obj);
								obj_to_vehicle(load_obj, veh);
								GET_AUTOSTORE_TIMER(load_obj) = timer;
				
								for (iter = MAX_BAG_ROWS - 1; iter > -location; --iter) {
									if (cont_row[iter]) {		/* No container, back to vehicle. */
										for (; cont_row[iter]; cont_row[iter] = obj2) {
											obj2 = cont_row[iter]->next_content;
											timer = GET_AUTOSTORE_TIMER(cont_row[iter]);
											obj_to_vehicle(cont_row[iter], veh);
											GET_AUTOSTORE_TIMER(cont_row[iter]) = timer;
										}
										cont_row[iter] = NULL;
									}
								}
								if (iter == -location && cont_row[iter]) {			/* Content list exists. */
									if (GET_OBJ_TYPE(load_obj) == ITEM_CONTAINER || GET_OBJ_TYPE(load_obj) == ITEM_CART || IS_CORPSE(load_obj)) {
										/* Take the item, fill it, and give it back. */
										obj_from_room(load_obj);
										load_obj->contains = NULL;
										for (; cont_row[iter]; cont_row[iter] = obj2) {
											obj2 = cont_row[iter]->next_content;
											obj_to_obj(cont_row[iter], load_obj);
										}
										timer = GET_AUTOSTORE_TIMER(load_obj);
										obj_to_vehicle(load_obj, veh);			/* Add to vehicle first. */
										GET_AUTOSTORE_TIMER(load_obj) = timer;
									}
									else {				/* Object isn't container, empty content list. */
										for (; cont_row[iter]; cont_row[iter] = obj2) {
											obj2 = cont_row[iter]->next_content;
											timer = GET_AUTOSTORE_TIMER(cont_row[iter]);
											obj_to_vehicle(cont_row[iter], veh);
											GET_AUTOSTORE_TIMER(cont_row[iter]) = timer;
										}
										cont_row[iter] = NULL;
									}
								}
								if (location < 0 && location >= -MAX_BAG_ROWS) {
									obj_from_room(load_obj);
									if ((obj2 = cont_row[-location - 1]) != NULL) {
										while (obj2->next_content) {
											obj2 = obj2->next_content;
										}
										obj2->next_content = load_obj;
									}
									else {
										cont_row[-location - 1] = load_obj;
									}
								}
							}
						}
						else if (!strn_cmp(line, "Contents-end", 12)) {
							// done
							break;
						}
						else {
							log("SYSERR: Format error in pack file for vehicle %d: %s", vnum, line);
							return NULL;
						}
					}
				}
				break;
			}
			case 'F': {
				if (OBJ_FILE_TAG(line, "Flags:", length)) {
					if (sscanf(line + length + 1, "%s", s_in)) {
						VEH_FLAGS(veh) = asciiflag_conv(s_in);
					}
				}
				break;
			}
			case 'H': {
				if (OBJ_FILE_TAG(line, "Health:", length)) {
					if (sscanf(line + length + 1, "%d", &i_in[0])) {
						VEH_HEALTH(veh) = MIN(i_in[0], VEH_MAX_HEALTH(veh));
					}
				}
				break;
			}
			case 'I': {
				if (OBJ_FILE_TAG(line, "Icon:", length)) {
					if (VEH_ICON(veh) && (!proto || VEH_ICON(veh) != VEH_ICON(proto))) {
						free(VEH_ICON(veh));
					}
					VEH_ICON(veh) = fread_string(fl, error);
				}
				else if (OBJ_FILE_TAG(line, "Interior-home:", length)) {
					if (sscanf(line + length + 1, "%d", &i_in[0])) {
						VEH_INTERIOR_HOME_ROOM(veh) = real_room(i_in[0]);
					}
				}
				break;
			}
			case 'K': {
				if (OBJ_FILE_TAG(line, "Keywords:", length)) {
					if (VEH_KEYWORDS(veh) && (!proto || VEH_KEYWORDS(veh) != VEH_KEYWORDS(proto))) {
						free(VEH_KEYWORDS(veh));
					}
					VEH_KEYWORDS(veh) = fread_string(fl, error);
				}
				break;
			}
			case 'L': {
				if (OBJ_FILE_TAG(line, "Long-desc:", length)) {
					if (VEH_LONG_DESC(veh) && (!proto || VEH_LONG_DESC(veh) != VEH_LONG_DESC(proto))) {
						free(VEH_LONG_DESC(veh));
					}
					VEH_LONG_DESC(veh) = fread_string(fl, error);
				}
				else if (OBJ_FILE_TAG(line, "Look-desc:", length)) {
					if (VEH_LOOK_DESC(veh) && (!proto || VEH_LOOK_DESC(veh) != VEH_LOOK_DESC(proto))) {
						free(VEH_LOOK_DESC(veh));
					}
					VEH_LOOK_DESC(veh) = fread_string(fl, error);
				}
				break;
			}
			case 'N': {
				if (OBJ_FILE_TAG(line, "Needs-res:", length)) {
					if (sscanf(line + length + 1, "%d %d", &i_in[0], &i_in[1]) == 2) {
						CREATE(res, struct resource_data, 1);
						res->vnum = i_in[0];
						res->amount = i_in[1];
						
						// append
						if (last_res) {
							last_res->next = res;
						}
						else {
							VEH_NEEDS_RESOURCES(veh) = res;
						}
						last_res = res;
					}
				}
				break;
			}
			case 'O': {
				if (OBJ_FILE_TAG(line, "Owner:", length)) {
					if (sscanf(line + length + 1, "%d", &i_in[0])) {
						VEH_OWNER(veh) = real_empire(i_in[0]);
					}
				}
				break;
			}
			case 'S': {
				if (OBJ_FILE_TAG(line, "Scale:", length)) {
					if (sscanf(line + length + 1, "%d", &i_in[0])) {
						VEH_SCALE_LEVEL(veh) = i_in[0];
					}
				}
				else if (OBJ_FILE_TAG(line, "Short-desc:", length)) {
					if (VEH_SHORT_DESC(veh) && (!proto || VEH_SHORT_DESC(veh) != VEH_SHORT_DESC(proto))) {
						free(VEH_SHORT_DESC(veh));
					}
					VEH_SHORT_DESC(veh) = fread_string(fl, error);
				}
				break;
			}
			case 'T': {
				if (OBJ_FILE_TAG(line, "Trigger:", length)) {
					if (sscanf(line + length + 1, "%d", &i_in[0]) && real_trigger(i_in[0])) {
						if (!SCRIPT(veh)) {
							CREATE(SCRIPT(veh), struct script_data, 1);
						}
						add_trigger(SCRIPT(veh), read_trigger(i_in[0]), -1);
					}
				}
				break;
			}
		}
	}
	
	return veh;	// if any
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
	
	// attributes init
	VEH_INTERIOR_ROOM_VNUM(veh) = NOTHING;
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
				if (!get_line(fl, line) || sscanf(line, "%d %d %s", &int_in[0], &int_in[1], str_in) != 3) {
					log("SYSERR: Format error in D line of %s", error);
					exit(1);
				}
				
				VEH_INTERIOR_ROOM_VNUM(veh) = int_in[0];
				VEH_MAX_ROOMS(veh) = int_in[1];
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
	if (VEH_INTERIOR_ROOM_VNUM(veh) != NOTHING || VEH_MAX_ROOMS(veh) || VEH_DESIGNATE_FLAGS(veh)) {
		strcpy(temp, bitv_to_alpha(VEH_DESIGNATE_FLAGS(veh)));
		fprintf(fl, "D\n%d %d %s\n", VEH_INTERIOR_ROOM_VNUM(veh), VEH_MAX_ROOMS(veh), temp);
	}
	
	// 'R': resources
	write_resources_to_file(fl, VEH_YEARLY_MAINTENANCE(veh));
	
	// end
	fprintf(fl, "S\n");
}


 //////////////////////////////////////////////////////////////////////////////
//// 2.0b3.8 CONVERTER ///////////////////////////////////////////////////////

// this system converts a set of objects to vehicles, including all the boats,
// catapults, carts, and ships.
	
// list of vnums to convert directly from obj to vehicle
any_vnum convert_list[] = {
	900,	// a rickety cart
	901,	// a carriage
	902,	// a covered wagon
	903,	// the catapult
	904,	// a chair
	905,	// a wooden bench
	906,	// a long table
	907,	// a stool
	917,	// the throne
	920,	// a wooden canoe
	952,	// the pinnace
	953,	// the brigantine
	954,	// the galley
	955,	// the argosy
	956,	// the galleon
	10715,	// the sleigh
	NOTHING	// end list
};

struct convert_vehicle_data {
	char_data *mob;	// mob to attach
	any_vnum vnum;	// vehicle vnum
	struct convert_vehicle_data *next;
};

struct convert_vehicle_data *convert_vehicle_list = NULL;

/**
* Stores data for a mob that was supposed to be attached to a vehicle.
*/
void add_convert_vehicle_data(char_data *mob, any_vnum vnum) {
	struct convert_vehicle_data *cvd;
	
	CREATE(cvd, struct convert_vehicle_data, 1);
	cvd->mob = mob;
	cvd->vnum = vnum;
	LL_PREPEND(convert_vehicle_list, cvd);
}


/**
* Processes any temporary data for mobs that should be attached to a vehicle.
* This basically assumes you're in the middle of upgrading to 2.0 b3.8 and
* works on any data it found. Mobs are only removed if they become attached
* to a vehicle.
*
* @return int the number converted
*/
int run_convert_vehicle_list(void) {
	struct convert_vehicle_data *cvd;
	vehicle_data *veh;
	int changed = 0;
	
	while ((cvd = convert_vehicle_list)) {
		convert_vehicle_list = cvd->next;
		
		if (cvd->mob && IN_ROOM(cvd->mob)) {
			LL_FOREACH2(ROOM_VEHICLES(IN_ROOM(cvd->mob)), veh, next_in_room) {
				if (VEH_VNUM(veh) == cvd->vnum && count_harnessed_animals(veh) < VEH_ANIMALS_REQUIRED(veh)) {
					harness_mob_to_vehicle(cvd->mob, veh);
					++changed;
					break;
				}
			}
		}
		
		free(cvd);
	}
	
	return changed;
}

/**
* Replaces an object with a vehicle of the same VNUM, and converts the traits
* that it can. This will result in partially-completed ships becoming fully-
* completed.
* 
* @param obj_data *obj The object to convert (will be extracted).
*/
void convert_one_obj_to_vehicle(obj_data *obj) {
	extern room_data *obj_room(obj_data *obj);
	
	obj_data *obj_iter, *next_obj;
	room_data *room, *room_iter, *main_room;
	vehicle_data *veh;
	
	// if there isn't a room or vehicle involved, just remove the object
	if (!(room = obj_room(obj)) || !vehicle_proto(GET_OBJ_VNUM(obj))) {
		extract_obj(obj);
		return;
	}
	
	// create the vehicle
	veh = read_vehicle(GET_OBJ_VNUM(obj), TRUE);
	vehicle_to_room(veh, room);
	
	// move inventory
	LL_FOREACH_SAFE2(obj->contains, obj_iter, next_obj, next_content) {
		obj_to_vehicle(obj_iter, veh);
	}
	
	// convert traits
	VEH_OWNER(veh) = real_empire(obj->last_empire_id);
	VEH_SCALE_LEVEL(veh) = GET_OBJ_CURRENT_SCALE_LEVEL(obj);
	
	// type-based traits
	switch (GET_OBJ_TYPE(obj)) {
		case ITEM_SHIP: {
			if ((main_room = real_room(GET_SHIP_MAIN_ROOM(obj)))) {
				VEH_INTERIOR_HOME_ROOM(veh) = main_room;
				
				// detect owner from room
				if (ROOM_OWNER(main_room)) {
					VEH_OWNER(veh) = ROOM_OWNER(main_room);
				}
				
				// apply vehicle aff
				LL_FOREACH2(interior_room_list, room_iter, next_interior) {
					if (room_iter == main_room || HOME_ROOM(room_iter) == main_room) {
						SET_BIT(ROOM_AFF_FLAGS(room_iter), ROOM_AFF_IN_VEHICLE);
						SET_BIT(ROOM_BASE_FLAGS(room_iter), ROOM_AFF_IN_VEHICLE);
					}
				}
			}
			break;
		}
		case ITEM_BOAT: {
			// nothing to convert?
			break;
		}
		case ITEM_CART: {
			// nothing to convert?
			break;
		}
	}
	
	// did we successfully get an owner? try the room it's in
	if (!VEH_OWNER(veh)) {
		VEH_OWNER(veh) = ROOM_OWNER(room);
	}
	
	// remove the object
	extract_obj(obj);
}


/**
* Converts a list of objects into vehicles with the same vnum. This converter
* was used during the initial implementation of vehicles in 2.0 b3.8.
*
* @return int the number converted
*/
int convert_to_vehicles(void) {
	obj_data *obj, *next_obj;
	int iter, changed = 0;
	bool found;
	
	LL_FOREACH_SAFE(object_list, obj, next_obj) {
		// determine if it's in the list to replace
		found = FALSE;
		for (iter = 0; convert_list[iter] != NOTHING && !found; ++iter) {
			if (convert_list[iter] == GET_OBJ_VNUM(obj)) {
				found = TRUE;
			}
		}
		if (!found) {
			continue;
		}
		
		// success
		convert_one_obj_to_vehicle(obj);
		++changed;
	}
	
	return changed;
}


/**
* Removes the old room affect flag that hinted when to show a ship in pre-
* b3.8.
*/
void b3_8_ship_update(void) {
	void save_whole_world();
	
	room_data *room, *next_room;
	int changed = 0;
	
	bitvector_t ROOM_AFF_SHIP_PRESENT = BIT(10);	// old bit to remove
	
	HASH_ITER(hh, world_table, room, next_room) {
		if (IS_SET(ROOM_AFF_FLAGS(room) | ROOM_BASE_FLAGS(room), ROOM_AFF_SHIP_PRESENT)) {
			REMOVE_BIT(ROOM_AFF_FLAGS(room), ROOM_AFF_SHIP_PRESENT);
			REMOVE_BIT(ROOM_BASE_FLAGS(room), ROOM_AFF_SHIP_PRESENT);
			++changed;
		}
	}
	
	changed += convert_to_vehicles();
	changed += run_convert_vehicle_list();
	
	if (changed > 0) {
		save_whole_world();
	}
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
	craft_data *craft, *next_craft;
	descriptor_data *desc;
	bool found;
	
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
	
	// update crafts
	HASH_ITER(hh, craft_table, craft, next_craft) {
		found = FALSE;
		if (CRAFT_FLAGGED(craft, CRAFT_VEHICLE) && GET_CRAFT_OBJECT(craft) == vnum) {
			GET_CRAFT_OBJECT(craft) = NOTHING;
			found = TRUE;
		}
		
		if (found) {
			SET_BIT(GET_CRAFT_FLAGS(craft), CRAFT_IN_DEVELOPMENT);
			save_library_file_for_vnum(DB_BOOT_CRAFT, GET_CRAFT_VNUM(craft));
		}
	}
	
	// olc editor updates
	LL_FOREACH(descriptor_list, desc) {
		if (GET_OLC_CRAFT(desc)) {
			found = FALSE;
			if (CRAFT_FLAGGED(GET_OLC_CRAFT(desc), CRAFT_VEHICLE) && GET_OLC_CRAFT(desc)->object == vnum) {
				GET_OLC_CRAFT(desc)->object = NOTHING;
				found = TRUE;
			}
		
			if (found) {
				SET_BIT(GET_OLC_CRAFT(desc)->flags, CRAFT_IN_DEVELOPMENT);
				msg_to_char(desc->character, "The vehicle made by the craft you're editing was deleted.\r\n");
			}
		}
	}
	
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
		size += snprintf(buf + size, sizeof(buf) - size, "Map Icon: %s\t0 %s\r\n", VEH_ICON(veh), show_color_codes(VEH_ICON(veh)));
	}
	
	size += snprintf(buf + size, sizeof(buf) - size, "Health: [\tc%d\t0/\tc%d\t0], Capacity: [\tc%d\t0/\tc%d\t0], Animals Req: [\tc%d\t0], Move Type: [\ty%s\t0]\r\n", VEH_HEALTH(veh), VEH_MAX_HEALTH(veh), VEH_CARRYING_N(veh), VEH_CAPACITY(veh), VEH_ANIMALS_REQUIRED(veh), mob_move_types[VEH_MOVE_TYPE(veh)]);
	
	if (VEH_INTERIOR_ROOM_VNUM(veh) != NOTHING || VEH_MAX_ROOMS(veh) || VEH_DESIGNATE_FLAGS(veh)) {
		sprintbit(VEH_DESIGNATE_FLAGS(veh), designate_flags, part, TRUE);
		size += snprintf(buf + size, sizeof(buf) - size, "Interior: [\tc%d\t0 - \ty%s\t0], Rooms: [\tc%d\t0], Designate: \ty%s\t0\r\n", VEH_INTERIOR_ROOM_VNUM(veh), building_proto(VEH_INTERIOR_ROOM_VNUM(veh)) ? GET_BLD_NAME(building_proto(VEH_INTERIOR_ROOM_VNUM(veh))) : "none", VEH_MAX_ROOMS(veh), part);
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
	
	if (VEH_INTERIOR_HOME_ROOM(veh) || VEH_INSIDE_ROOMS(veh) > 0) {
		// this adds 1 room for the home room, which isn't counted
		size += snprintf(buf + size, sizeof(buf) - size, "Interior location: [\ty%d\t0], Rooms inside: [\tg%d\t0]\r\n", VEH_INTERIOR_HOME_ROOM(veh) ? GET_ROOM_VNUM(VEH_INTERIOR_HOME_ROOM(veh)) : NOTHING, VEH_INSIDE_ROOMS(veh) + 1);
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
	
	if (VEH_OWNER(veh)) {
		msg_to_char(ch, "It is owned by %s%s\t0.\r\n", EMPIRE_BANNER(VEH_OWNER(veh)), EMPIRE_NAME(VEH_OWNER(veh)));
	}
	
	if (VEH_NEEDS_RESOURCES(veh)) {
		struct resource_data *res;
		bool found = FALSE;
		
		if (VEH_IS_COMPLETE(veh)) {
			msg_to_char(ch, "Mainteance needed: ");
		}
		else {
			msg_to_char(ch, "Resources to completion: ");
		}
		
		LL_FOREACH(VEH_NEEDS_RESOURCES(veh), res) {
			msg_to_char(ch, "%s%s (x%d)", (found ? ", " : ""), skip_filler(get_obj_name_by_proto(res->vnum)), res->amount);
			found = TRUE;
		}
		
		if (found) {
			msg_to_char(ch, "\r\n");
		}
		else {
			msg_to_char(ch, "none\r\n");	// can we even get here?
		}
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
	sprintf(buf + strlen(buf), "<\tyicon\t0> %s\t0 %s\r\n", VEH_ICON(veh) ? VEH_ICON(veh) : "none", VEH_ICON(veh) ? show_color_codes(VEH_ICON(veh)) : "");
	
	sprintbit(VEH_FLAGS(veh), vehicle_flags, lbuf, TRUE);
	sprintf(buf + strlen(buf), "<\tyflags\t0> %s\r\n", lbuf);
	
	sprintf(buf + strlen(buf), "<\tyhitpoints\t0> %d\r\n", VEH_MAX_HEALTH(veh));
	sprintf(buf + strlen(buf), "<\tymovetype\t0> %s\r\n", mob_move_types[VEH_MOVE_TYPE(veh)]);
	sprintf(buf + strlen(buf), "<\tycapacity\t0> %d item%s\r\n", VEH_CAPACITY(veh), PLURAL(VEH_CAPACITY(veh)));
	sprintf(buf + strlen(buf), "<\tyanimalsrequired\t0> %d\r\n", VEH_ANIMALS_REQUIRED(veh));

	sprintf(buf + strlen(buf), "<\tyinteriorroom\t0> %d - %s\r\n", VEH_INTERIOR_ROOM_VNUM(veh), building_proto(VEH_INTERIOR_ROOM_VNUM(veh)) ? GET_BLD_NAME(building_proto(VEH_INTERIOR_ROOM_VNUM(veh))) : "none");
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
		msg_to_char(ch, "\t0");	// in case color is unterminated
	}
}


OLC_MODULE(vedit_interiorroom) {
	vehicle_data *veh = GET_OLC_VEHICLE(ch->desc);
	any_vnum old_b;
	bld_data *bld;
	
	if (!str_cmp(argument, "none")) {
		VEH_INTERIOR_ROOM_VNUM(veh) = NOTHING;
		msg_to_char(ch, "It now has no interior room.\r\n");
		return;
	}
	
	old_b = VEH_INTERIOR_ROOM_VNUM(veh);
	VEH_INTERIOR_ROOM_VNUM(veh) = olc_process_number(ch, argument, "interior room vnum", "interiorroom", 0, MAX_VNUM, VEH_INTERIOR_ROOM_VNUM(veh));
	
	if (!(bld = building_proto(VEH_INTERIOR_ROOM_VNUM(veh)))) {
		VEH_INTERIOR_ROOM_VNUM(veh) = old_b;
		msg_to_char(ch, "Invalid room building vnum. Old value restored.\r\n");
	}
	else if (!IS_SET(GET_BLD_FLAGS(bld), BLD_ROOM)) {
		VEH_INTERIOR_ROOM_VNUM(veh) = old_b;
		msg_to_char(ch, "You can only set it to a building template with the ROOM flag. Old value restored.\r\n");
	}	
	else {
		msg_to_char(ch, "It now has interior room '%s'.\r\n", GET_BLD_NAME(bld));
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
