/* ************************************************************************
*   File: vehicles.c                                      EmpireMUD 2.0b5 *
*  Usage: DB and OLC for vehicles (including ships)                       *
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
#include "dg_scripts.h"
#include "constants.h"
#include "vnums.h"

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

// external vars
extern struct empire_chore_type chore_data[NUM_CHORES];

// local data
const char *default_vehicle_keywords = "vehicle unnamed";
const char *default_vehicle_short_desc = "an unnamed vehicle";
const char *default_vehicle_long_desc = "An unnamed vehicle is parked here.";

// local protos
void clear_vehicle(vehicle_data *veh);
void store_one_vehicle_to_file(vehicle_data *veh, FILE *fl);


 //////////////////////////////////////////////////////////////////////////////
//// HELPERS /////////////////////////////////////////////////////////////////

/**
* Cancels vehicle ownership on vehicles whose empires are gone, allowing those
* vehicles to be cleaned up by players.
*/
void abandon_lost_vehicles(void) {
	vehicle_data *veh;
	empire_data *emp;
	
	DL_FOREACH(vehicle_list, veh) {
		if (!(emp = VEH_OWNER(veh))) {
			continue;	// only looking to abandon owned vehs
		}
		if (EMPIRE_IMM_ONLY(emp)) {
			continue;	// imm empire vehicles could be disastrous
		}
		if (EMPIRE_MEMBERS(emp) > 0 || EMPIRE_TERRITORY(emp, TER_TOTAL) > 0) {
			continue;	// skip empires that still have territory or members
		}
		
		// found!
		perform_abandon_vehicle(veh);
	}
}


/**
* Determines if one or more climate flags are allowed to ruin a vehicle slowly
* (damaging it over time rather than all at once).
*
* @param bitvector_t climate_flags Any number of CLIM_ to check together.
* @param int mode CRVS_WHEN_GAINING or CRVS_WHEN_LOSING (whether you gained or lost this climate).
* @return bool TRUE if the climate(s) allow the vehicle to ruin slowly, or FALSE if any climate does not allow it.
*/
bool check_climate_ruins_vehicle_slowly(bitvector_t climate_flags, int mode) {
	bitvector_t check;
	int pos;
	bool ok = TRUE;
	
	for (check = climate_flags, pos = 0; check && ok; check >>= 1, ++pos) {
		if (IS_SET(check, 1)) {
			if (!climate_ruins_vehicle_slowly[pos][mode]) {
				// any FALSE ruins the whole bunch
				ok = FALSE;
			}
		}
	}
	
	return ok;
}


/**
* When a vehicle decays, call this to see if it and/or the room should also
* be abandoned.
*
* @param vehicle_data *veh The vehicle that's decayed.
*/
void check_decayed_vehicle_abandon(vehicle_data *veh) {
	vehicle_data *iter;
	bool any = FALSE;
	
	if (!VEH_OWNER(veh)) {
		return;
	}
	else if (!VEH_CLAIMS_WITH_ROOM(veh)) {
		perform_abandon_vehicle(veh);
		return;
	}
	
	// otherwise check the room for any non-decayed vehicles that claim with room
	if (IN_ROOM(veh)) {
		DL_FOREACH2(ROOM_VEHICLES(IN_ROOM(veh)), iter, next_in_room) {
			if (VEH_CLAIMS_WITH_ROOM(iter) && VEH_HEALTH(iter) > 0) {
				any = TRUE;
			}
		}
		
		if (!any && ROOM_OWNER(IN_ROOM(veh)) && HOME_ROOM(IN_ROOM(veh)) == IN_ROOM(veh)) {
			abandon_room(IN_ROOM(veh));
		}
	}
}


/**
* Checks the allowed-climates on a vehicle. This should be called if the
* terrain changes, and can also be called periodically to check for damage.
*
* @param vehicle_data *veh The vehicle to check.
* @param bool immediate_only If TRUE, only ruins a vehicle that must ruin immediately and skips any that ruin slowly. If FALSE, does both.
* @return bool TRUE if the vehicle survived, FALSE if it was destroyed.
*/
bool check_vehicle_climate_change(vehicle_data *veh, bool immediate_only) {
	char buf[256];
	bool res, slow_ruin = FALSE;
	char *msg;
	
	if (VEH_IS_EXTRACTED(veh) || ROOM_IS_CLOSED(IN_ROOM(veh))) {
		return TRUE;	// skip extracted vehicles or inside ones
	}
	if (vehicle_allows_climate(veh, IN_ROOM(veh), &slow_ruin)) {
		return TRUE;	// vehicle is safe
	}
	
	// oh no! the vehicle can't be here!
	
	// verify ruin speed
	if (VEH_FLAGGED(veh, VEH_RUIN_SLOWLY_FROM_CLIMATE)) {	
		slow_ruin = TRUE;
	}
	if (VEH_FLAGGED(veh, VEH_RUIN_QUICKLY_FROM_CLIMATE)) {	
		slow_ruin = FALSE;
	}
	
	// and now do the work...
	if (slow_ruin && !immediate_only) {
		// this does its own logging
		res = decay_one_vehicle(veh, "$V falls into ruin!");
		
		if (res && VEH_OWNER(veh)) {
			// log AFTER because it probably logged on its own if it auto-abandoned
			snprintf(buf, sizeof(buf), "%s%s is falling into ruin due to changing terrain", get_vehicle_short_desc(veh, NULL), coord_display_room(NULL, IN_ROOM(veh), FALSE));
			log_to_empire(VEH_OWNER(veh), ELOG_TERRITORY, "%s", CAP(buf));
		}
		
		return res;
	}
	else if (!slow_ruin) {
		if (VEH_OWNER(veh)) {
			// log before ruining (it'll be gone)
			snprintf(buf, sizeof(buf), "%s%s has crumbled to ruin", get_vehicle_short_desc(veh, NULL), coord_display_room(NULL, IN_ROOM(veh), FALSE));
			log_to_empire(VEH_OWNER(veh), ELOG_TERRITORY, "%s", CAP(buf));
		}
		// this will extract it (usually)
		msg = veh_get_custom_message(veh, VEH_CUSTOM_CLIMATE_CHANGE_TO_ROOM);
		ruin_vehicle(veh, msg ? msg : "$V falls into ruin!");
		return FALSE;
	}
	
	// other cases
	return TRUE;
}


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
* Counts how many building-vehicles are in the room (testing using the
* VEH_CLAIMS_WITH_ROOM() macro). Optionally, you can check the owner, too.
*
* @param room_data *room The location.
* @param empire_data *only_owner Optional: Only count ones owned by this person (NULL for any-owner).
* @return int The number of building-vehicles in the room.
*/
int count_building_vehicles_in_room(room_data *room, empire_data *only_owner) {
	vehicle_data *veh;
	int count = 0;
	if (room) {
		DL_FOREACH2(ROOM_VEHICLES(room), veh, next_in_room) {
			if (VEH_CLAIMS_WITH_ROOM(veh) && (!only_owner || VEH_OWNER(veh) == only_owner)) {
				++count;
			}
		}
	}
	return count;
}


/**
* Determines how many players are inside a vehicle.
*
* @param vehicle_data *veh The vehicle to check.
* @param bool ignore_invis_imms If TRUE, does not count immortals who can't be seen by players.
* @return int How many players were inside.
*/
int count_players_in_vehicle(vehicle_data *veh, bool ignore_invis_imms) {
	struct vehicle_room_list *vrl, *next_vrl;
	vehicle_data *viter;
	char_data *iter;
	int count = 0;
	
	LL_FOREACH_SAFE(VEH_ROOM_LIST(veh), vrl, next_vrl) {
		DL_FOREACH2(ROOM_PEOPLE(vrl->room), iter, next_in_room) {
			if (IS_NPC(iter)) {
				continue;
			}
			if (ignore_invis_imms && IS_IMMORTAL(iter) && (GET_INVIS_LEV(iter) > 1 || PRF_FLAGGED(iter, PRF_WIZHIDE))) {
				continue;
			}
			
			++count;
		}
		
		// check nested vehicles
		DL_FOREACH2(ROOM_VEHICLES(vrl->room), viter, next_in_room) {
			count += count_players_in_vehicle(viter, ignore_invis_imms);
		}
	}
	
	return count;
}


/**
* Deals 10% damage, adds needed-maintenance if applicable, and may ruin the
* vehicle when it's very damaged. It will always abandon it if the damage falls
* too low.
*
* @param vehicle_data *veh The vehicle.
* @param char *message A message to send if it's ruined ("$V crumbles from disrepair!") -- may be NULL and might be overridden by the vehicle's custom messages
* @return bool TRUE if the vehicle survived, FALSE if it was ruined.
*/
bool decay_one_vehicle(vehicle_data *veh, char *message) {
	static struct resource_data *default_res = NULL;
	empire_data *emp = VEH_OWNER(veh);
	struct resource_data *old_list;
	char buf[256];
	char *msg;
	
	// resources if it doesn't have its own
	if (!default_res) {
		add_to_resource_list(&default_res, RES_COMPONENT, COMP_NAILS, 1, 0);
	}
	
	// update health
	VEH_HEALTH(veh) -= MAX(1.0, ((double) VEH_MAX_HEALTH(veh) / 10.0));
	request_vehicle_save_in_world(veh);
	
	// check very low health
	if (VEH_HEALTH(veh) <= 0) {
		check_decayed_vehicle_abandon(veh);
		
		// 50% of the time we just abandon, the rest we also decay to ruins
		if (!number(0, 1) || VEH_IS_DISMANTLING(veh)) {
			if (emp) {
				snprintf(buf, sizeof(buf), "%s%s has crumbled to ruin", get_vehicle_short_desc(veh, NULL), coord_display_room(NULL, IN_ROOM(veh), FALSE));
				log_to_empire(emp, ELOG_TERRITORY, "%s", CAP(buf));
			}
			msg = veh_get_custom_message(veh, VEH_CUSTOM_RUINS_TO_ROOM);
			ruin_vehicle(veh, msg ? msg : message);
			return FALSE;	// returns only if ruined
		}
		else if (emp && !VEH_OWNER(veh)) {
			snprintf(buf, sizeof(buf), "%s%s has been abandoned due to decay", get_vehicle_short_desc(veh, NULL), coord_display_room(NULL, IN_ROOM(veh), FALSE));
			log_to_empire(emp, ELOG_TERRITORY, "%s", CAP(buf));
		}
	}
	
	// apply maintenance if not dismantling -- and is either complete OR has had at least 1 item added
	if (!VEH_IS_DISMANTLING(veh) && (VEH_IS_COMPLETE(veh) || VEH_BUILT_WITH(veh))) {
		old_list = VEH_NEEDS_RESOURCES(veh);
		VEH_NEEDS_RESOURCES(veh) = combine_resources(old_list, VEH_REGULAR_MAINTENANCE(veh) ? VEH_REGULAR_MAINTENANCE(veh) : default_res);
		free_resource_list(old_list);
	}
	
	return TRUE;
}


/**
* Deletes the interior rooms of a vehicle e.g. before an extract or dismantle.
*
* @param vehicle_data *veh The vehicle whose interior must go.
*/
void delete_vehicle_interior(vehicle_data *veh) {
	struct vehicle_room_list *vrl, *next_vrl;
	room_data *main_room;
	
	if ((main_room = VEH_INTERIOR_HOME_ROOM(veh)) || VEH_ROOM_LIST(veh)) {
		LL_FOREACH_SAFE(VEH_ROOM_LIST(veh), vrl, next_vrl) {
			if (vrl->room == main_room) {
				continue;	// do this one last
			}
			
			relocate_players(vrl->room, IN_ROOM(veh));
			delete_room(vrl->room, FALSE);	// MUST check_all_exits later
		}
		
		if (main_room) {
			relocate_players(main_room, IN_ROOM(veh));
			delete_room(main_room, FALSE);
		}
		check_all_exits();
	}
}


/**
* Empties the contents (items) of a vehicle into the room it's in (if any) or
* extracts them.
*
* @param vehicle_data *veh The vehicle to empty.
* @param room_data *to_room If not NULL, empties the vehicle contents to that room. Extracts them if NULL.
*/
void empty_vehicle(vehicle_data *veh, room_data *to_room) {
	obj_data *obj, *next_obj;
	
	DL_FOREACH_SAFE2(VEH_CONTAINS(veh), obj, next_obj, next_content) {
		if (to_room) {
			obj_to_room(obj, to_room);
		}
		else {
			extract_obj(obj);
		}
	}
}


/**
* Finds the craft recipe entry for a given building vehicle.
*
* @param room_data *room The building location to find.
* @param byte type FIND_BUILD_UPGRADE or FIND_BUILD_NORMAL.
* @return craft_data* The craft for that building, or NULL.
*/
craft_data *find_craft_for_vehicle(vehicle_data *veh) {
	craft_data *craft, *next_craft;
	any_vnum recipe;
	
	if ((recipe = get_vehicle_extra_data(veh, ROOM_EXTRA_BUILD_RECIPE)) > 0 && (craft = craft_proto(recipe))) {
		return craft;
	}
	else {
		HASH_ITER(hh, craft_table, craft, next_craft) {
			if (CRAFT_FLAGGED(craft, CRAFT_IN_DEVELOPMENT) || !CRAFT_IS_VEHICLE(craft)) {
				continue;	// not a valid target
			}
			if (GET_CRAFT_OBJECT(craft) != VEH_VNUM(veh)) {
				continue;
			}
		
			// we have a match!
			return craft;
		}
	}
	
	return NULL;	// if none
}


/**
* Finds a vehicle in the room that is mid-dismantle. Optionally, finds a
* specific one rather than ANY one.
*
* @param room_data *room The room to find the vehicle in.
* @param int with_id Optional: Find a vehicle with this construction-id (pass NOTHING to find any mid-dismantle vehicle).
* @return vehicle_data* The found vehicle, if any (otherwise NULL).
*/
vehicle_data *find_dismantling_vehicle_in_room(room_data *room, int with_id) {
	vehicle_data *veh;
	
	DL_FOREACH2(ROOM_VEHICLES(room), veh, next_in_room) {
		if (VEH_IS_EXTRACTED(veh) || !VEH_IS_DISMANTLING(veh)) {
			continue;	// not being dismantled
		}
		if (with_id != NOTHING && VEH_CONSTRUCTION_ID(veh) != with_id) {
			continue;	// wrong id
		}
		
		// found!
		return veh;
	}
	
	return NULL;	// not found
}


/**
* Finds a vehicle in the room with a interior vnum.
*
* @param room_data *room The room to start in.
* @param room_vnum interior_room The interior room to check for.
* @return vehicle_data* The vehicle with that interior, if any. Otherwise, NULL.
*/
vehicle_data *find_vehicle_in_room_with_interior(room_data *room, room_vnum interior_room) {
	vehicle_data *veh;
	
	DL_FOREACH2(ROOM_VEHICLES(room), veh, next_in_room) {
		if (!VEH_IS_EXTRACTED(veh) && VEH_INTERIOR_HOME_ROOM(veh) && GET_ROOM_VNUM(VEH_INTERIOR_HOME_ROOM(veh)) == interior_room) {
			return veh;
		}
	}
	
	return NULL;
}


/**
* Finishes the actual dismantle for a vehicle.
*
* @param char_data *ch Optional: The dismantler.
* @param vehicle_data *veh The vehicle being dismantled.
*/
void finish_dismantle_vehicle(char_data *ch, vehicle_data *veh) {
	obj_data *newobj, *proto;
	craft_data *type;
	char_data *iter;
	
	if (ch) {
		act("You finish dismantling $V.", FALSE, ch, NULL, veh, TO_CHAR | ACT_VEH_VICT);
		act("$n finishes dismantling $V.", FALSE, ch, NULL, veh, TO_ROOM | ACT_VEH_VICT);
	}
	
	if (IN_ROOM(veh)) {
		if (VEH_IS_VISIBLE_ON_MAPOUT(veh)) {
			request_mapout_update(GET_ROOM_VNUM(IN_ROOM(veh)));
		}
		DL_FOREACH2(ROOM_PEOPLE(IN_ROOM(veh)), iter, next_in_room) {
			if (!IS_NPC(iter) && GET_ACTION(iter) == ACT_DISMANTLE_VEHICLE && GET_ACTION_VNUM(iter, 1) == VEH_CONSTRUCTION_ID(veh)) {
				cancel_action(iter);
			}
			else if (IS_NPC(iter) && GET_MOB_VNUM(iter) == chore_data[CHORE_BUILDING].mob) {
				set_mob_flags(iter, MOB_SPAWNED);
			}
		}
	}
	
	// check for required obj and return it
	if (IN_ROOM(veh) && (type = find_craft_for_vehicle(veh)) && CRAFT_FLAGGED(type, CRAFT_TAKE_REQUIRED_OBJ) && GET_CRAFT_REQUIRES_OBJ(type) != NOTHING && (proto = obj_proto(GET_CRAFT_REQUIRES_OBJ(type))) && !OBJ_FLAGGED(proto, OBJ_SINGLE_USE)) {
		newobj = read_object(GET_CRAFT_REQUIRES_OBJ(type), TRUE);
		
		// scale item to minimum level
		scale_item_to_level(newobj, 0);
		
		if (!ch || IS_NPC(ch)) {
			obj_to_room(newobj, IN_ROOM(veh));
			check_autostore(newobj, TRUE, VEH_OWNER(veh));
		}
		else {
			obj_to_char(newobj, ch);
			act("You get $p.", FALSE, ch, newobj, 0, TO_CHAR);
			
			// ensure binding
			if (OBJ_FLAGGED(newobj, OBJ_BIND_FLAGS)) {
				bind_obj_to_player(newobj, ch);
			}
			if (load_otrigger(newobj)) {
				get_otrigger(newobj, ch, FALSE);
			}
		}
	}
			
	extract_vehicle(veh);
}


/**
* This runs after the vehicle is finished (or, in some cases, if it moves).
*
* @param vehicle_data *veh The vehicle being finished.
*/
void finish_vehicle_setup(vehicle_data *veh) {
	if (!veh || !VEH_IS_COMPLETE(veh)) {
		return;	// no work
	}
	
	// mine setup
	if (room_has_function_and_city_ok(VEH_OWNER(veh), IN_ROOM(veh), FNC_MINE)) {
		init_mine(IN_ROOM(veh), NULL, VEH_OWNER(veh) ? VEH_OWNER(veh) : ROOM_OWNER(IN_ROOM(veh)));
	}
}


/**
* Removes everyone/everything from inside a vehicle, and puts it on the outside
* if possible.
*
* @param vehicle_data *veh The vehicle to empty.
* @param room_data *to_room Optional: Where to send things (extracts them instead if this is NULL).
*/
void fully_empty_vehicle(vehicle_data *veh, room_data *to_room) {
	vehicle_data *iter, *next_iter;
	struct vehicle_room_list *vrl;
	obj_data *obj, *next_obj;
	char_data *ch, *next_ch;
	
	if (VEH_ROOM_LIST(veh)) {
		LL_FOREACH(VEH_ROOM_LIST(veh), vrl) {
			// remove other vehicles
			DL_FOREACH_SAFE2(ROOM_VEHICLES(vrl->room), iter, next_iter, next_in_room) {
				if (to_room) {
					vehicle_to_room(iter, to_room);
				}
				else {
					vehicle_from_room(iter);
					extract_vehicle(iter);
				}
			}
			
			// remove people
			DL_FOREACH_SAFE2(ROOM_PEOPLE(vrl->room), ch, next_ch, next_in_room) {
				act("You are ejected from $V!", FALSE, ch, NULL, veh, TO_CHAR | ACT_VEH_VICT);
				if (to_room) {
					char_to_room(ch, to_room);
					qt_visit_room(ch, IN_ROOM(ch));
					look_at_room(ch);
					act("$n is ejected from $V!", TRUE, ch, NULL, veh, TO_ROOM | ACT_VEH_VICT);
					msdp_update_room(ch);
				}
				else {
					extract_char(ch);
				}
			}
			
			// remove items
			DL_FOREACH_SAFE2(ROOM_CONTENTS(vrl->room), obj, next_obj, next_content) {
				if (to_room) {
					obj_to_room(obj, to_room);
				}
				else {
					extract_obj(obj);
				}
			}
		}
	}
	
	// dump contents
	empty_vehicle(veh, to_room);
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
	if (!(bld = building_proto(VEH_INTERIOR_ROOM_VNUM(veh)))) {
		return NULL;
	}
	
	// otherwise, create the interior
	// NOTE: I don't think this can use add_room_to_building() because it's the first room -paul 1/30/2024
	room = create_room(NULL);
	attach_building_to_room(bld, room, TRUE);
	COMPLEX_DATA(room)->home_room = NULL;
	affect_total_room(room);
	
	// attach
	VEH_INTERIOR_HOME_ROOM(veh) = room;
	add_room_to_vehicle(room, veh);
	
	if (VEH_OWNER(veh)) {
		claim_room(room, VEH_OWNER(veh));
	}
	
	complete_wtrigger(room);
	
	request_vehicle_save_in_world(veh);
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
* to qualify for this, and must also either be complete or be size > 0.
* Vehicles in buildings are never shown. Only the largest valid vehicle is
* returned.
*
* @param char_data *ch The player looking.
* @param room_data *room The room to check.
* @param int *total_vehicles Optional: If a variable is provided, it will be set to the total number of vehicles visible in the room from a distance. (May be NULL to skip.)
* @return vehicle_data* A vehicle to show, if any (NULL if not).
*/
vehicle_data *find_vehicle_to_show(char_data *ch, room_data *room, int *total_vehicles) {
	vehicle_data *iter, *in_veh, *found = NULL;
	bool is_on_vehicle = ((in_veh = GET_ROOM_VEHICLE(IN_ROOM(ch))) && room == IN_ROOM(in_veh));
	int found_size = -1, found_height = -1;
	
	if (total_vehicles) {
		*total_vehicles = 0;	// init
	}
	
	// we don't show vehicles in buildings or closed tiles (unless the player is on a vehicle in that room, in which case we override)
	if (!is_on_vehicle && ((IS_ANY_BUILDING(room) && !ROOM_BLD_FLAGGED(room, BLD_SHOW_VEHICLES)) || ROOM_IS_CLOSED(room))) {
		return NULL;
	}
	
	DL_FOREACH2(ROOM_VEHICLES(room), iter, next_in_room) {
		if ((!VEH_ICON(iter) || !*VEH_ICON(iter)) && (!VEH_HALF_ICON(iter) || !*VEH_HALF_ICON(iter)) && (!VEH_QUARTER_ICON(iter) || !*VEH_QUARTER_ICON(iter))) {
			continue;	// no icon
		}
		if (!VEH_IS_COMPLETE(iter) && VEH_SIZE(iter) < 1) {
			continue;	// skip incomplete unless it has size
		}
		if (vehicle_is_chameleon(iter, IN_ROOM(ch))) {
			continue;	// can't see from here
		}
		
		// valid to show! only if first/bigger (prefer height over size)
		if (!found || VEH_HEIGHT(iter) >= found_height) {
			found_height = VEH_HEIGHT(iter);
			if (!found || VEH_SIZE(iter) > found_size) {
				found_size = VEH_SIZE(iter);
				found = iter;
			}
		}
		
		// and increment the total
		if (total_vehicles) {
			++(*total_vehicles);
		}
	}
	
	return found;	// if any
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
	request_vehicle_save_in_world(veh);
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
	
		size += snprintf(output+size, sizeof(output)-size, "%s%s%s", ((num == 0) ? "" : (next_iter ? ", " : (num > 1 ? ", and " : " and "))), get_mob_name_by_proto(iter->mob, TRUE), mult);
		++num;
	}
	
	return output;
}


/**
* Interaction function for vehicle-ruins-to-vehicle. This loads a new vehicle,
* ignoring interaction quantity, and transfers built-with resources and
* contents.
*
* INTERACTION_FUNC provides: ch, interaction, inter_room, inter_mob, inter_item, inter_veh
*/
INTERACTION_FUNC(ruin_vehicle_to_vehicle_interaction) {
	room_data *room = inter_room ? inter_room : (inter_veh ? IN_ROOM(inter_veh) : NULL);
	struct resource_data *res, *next_res, *save = NULL;
	vehicle_data *ruin, *proto, *veh_iter, *next_veh;
	struct vehicle_room_list *vrl;
	double save_resources;
	room_data *inside;
	char *to_free;
	
	if (!inter_veh || !room || !(proto = vehicle_proto(interaction->vnum))) {
		return FALSE;	// safety
	}
	
	ruin = read_vehicle(interaction->vnum, TRUE);
	vehicle_to_room(ruin, room);
	scale_vehicle_to_level(ruin, VEH_SCALE_LEVEL(inter_veh));	// attempt auto-detect of level
	
	// do not transfer ownership -- ruins never default to 'claimed'
	/*
	if (VEH_CLAIMS_WITH_ROOM(ruin) && ROOM_OWNER(HOME_ROOM(room))) {
		perform_claim_vehicle(ruin, ROOM_OWNER(HOME_ROOM(room)));
	}
	*/
	
	// move contents
	if ((inside = get_vehicle_interior(ruin))) {
		LL_FOREACH(VEH_ROOM_LIST(inter_veh), vrl) {
			// remove any unclaimed/empty vehicles (like furniture) -- those crumble with the building
			DL_FOREACH_SAFE2(ROOM_VEHICLES(vrl->room), veh_iter, next_veh, next_in_room) {
				if (!VEH_IS_EXTRACTED(veh_iter) && !VEH_OWNER(veh_iter) && !VEH_CONTAINS(veh_iter)) {
					ruin_vehicle(veh_iter, "$V falls into ruin.");
				}
			}
		}
		
		fully_empty_vehicle(inter_veh, inside);
	}
	
	// move resources...
	if (VEH_BUILT_WITH(inter_veh)) {
		save = VEH_BUILT_WITH(inter_veh);
		VEH_BUILT_WITH(inter_veh) = NULL;
	}
	else if (VEH_IS_DISMANTLING(inter_veh)) {
		save = VEH_NEEDS_RESOURCES(inter_veh);
		VEH_NEEDS_RESOURCES(inter_veh) = NULL;
	}
	
	// reattach built-with (if any) and reduce it to 5-20%
	if (save) {
		save_resources = number(5, 20) / 100.0;
		VEH_BUILT_WITH(ruin) = save;
		LL_FOREACH_SAFE(VEH_BUILT_WITH(ruin), res, next_res) {
			res->amount = ceil(res->amount * save_resources);
			
			if (res->amount <= 0) {	// delete if empty
				LL_DELETE(VEH_BUILT_WITH(ruin), res);
				free(res);
			}
		}
	}
	
	// custom naming if #n is present: note does not use set_vehicle_keywords etc because of special handling
	if (strstr(VEH_KEYWORDS(ruin), "#n")) {
		to_free = (!proto || VEH_KEYWORDS(ruin) != VEH_KEYWORDS(proto)) ? VEH_KEYWORDS(ruin) : NULL;
		VEH_KEYWORDS(ruin) = str_replace("#n", VEH_SHORT_DESC(inter_veh), VEH_KEYWORDS(ruin));
		if (to_free) {
			free(to_free);
		}
	}
	if (strstr(VEH_SHORT_DESC(ruin), "#n")) {
		to_free = (!proto || VEH_SHORT_DESC(ruin) != VEH_SHORT_DESC(proto)) ? VEH_SHORT_DESC(ruin) : NULL;
		VEH_SHORT_DESC(ruin) = str_replace("#n", VEH_SHORT_DESC(inter_veh), VEH_SHORT_DESC(ruin));
		if (to_free) {
			free(to_free);
		}
	}
	if (strstr(VEH_LONG_DESC(ruin), "#n")) {
		to_free = (!proto || VEH_LONG_DESC(ruin) != VEH_LONG_DESC(proto)) ? VEH_LONG_DESC(ruin) : NULL;
		VEH_LONG_DESC(ruin) = str_replace("#n", VEH_SHORT_DESC(inter_veh), VEH_LONG_DESC(ruin));
		CAP(VEH_LONG_DESC(ruin));
		if (to_free) {
			free(to_free);
		}
	}
	
	if (VEH_PAINT_COLOR(inter_veh)) {
		set_vehicle_extra_data(ruin, ROOM_EXTRA_PAINT_COLOR, VEH_PAINT_COLOR(inter_veh));
	}
	
	request_vehicle_save_in_world(ruin);
	load_vtrigger(ruin);
	return TRUE;
}


/**
* Destroys a vehicle from disrepair or invalid location. A destroy trigger
* on the vehicle can prevent this. If it was a building-vehicle and is the last
* one in the room, the room will also auto-abandon.
*
* @param vehicle_data *veh The vehicle (will usually be extracted).
* @param char *message Optional: An act string (using $V for the vehicle) to send to the room. (NULL for none)
*/
void ruin_vehicle(vehicle_data *veh, char *message) {
	bool was_bld = VEH_FLAGGED(veh, VEH_BUILDING) ? TRUE : FALSE;
	empire_data *emp = VEH_OWNER(veh);
	room_data *room = IN_ROOM(veh);
	struct vehicle_room_list *vrl;
	
	if (!destroy_vtrigger(veh, "ruins")) {
		VEH_HEALTH(veh) = MAX(1, VEH_HEALTH(veh));	// ensure health
		return;
	}
	
	if (message && ROOM_PEOPLE(IN_ROOM(veh))) {
		act(message, FALSE, ROOM_PEOPLE(IN_ROOM(veh)), NULL, veh, TO_CHAR | TO_ROOM | ACT_VEH_VICT);
	}
	
	// delete the NPCs who live here first so they don't do an 'ejected-then-leave'
	LL_FOREACH(VEH_ROOM_LIST(veh), vrl) {
		delete_room_npcs(vrl->room, NULL, TRUE);
	}
	
	// dismantle triggers first -- despawns boards in studies, etc
	dismantle_vtrigger(NULL, veh, FALSE);
	vehicle_interior_dismantle_triggers(veh, NULL);
	
	// ruins
	run_interactions(NULL, VEH_INTERACTIONS(veh), INTERACT_RUINS_TO_VEH, IN_ROOM(veh), NULL, NULL, veh, ruin_vehicle_to_vehicle_interaction);
	
	fully_empty_vehicle(veh, IN_ROOM(veh));
	extract_vehicle(veh);
	
	// auto-abandon if it was the last building-vehicle and was ruined
	if (was_bld && room && emp && count_building_vehicles_in_room(room, emp) == 0) {
		abandon_room(room);
	}
}


/**
* @param vehicle_data *veh The vehicle to scale.
* @param int level What level to scale it to (passing 0 will trigger auto-detection).
*/
void scale_vehicle_to_level(vehicle_data *veh, int level) {
	struct instance_data *inst = NULL;
	
	// detect level if we weren't given a strong level
	if (!level && (inst = get_instance_by_id(VEH_INSTANCE_ID(veh)))) {
		if (INST_LEVEL(inst) > 0) {
			level = INST_LEVEL(inst);
		}
	}
	
	// outside constraints
	if (inst) {
		if (GET_ADV_MIN_LEVEL(INST_ADVENTURE(inst)) > 0) {
			level = MAX(level, GET_ADV_MIN_LEVEL(INST_ADVENTURE(inst)));
		}
		if (GET_ADV_MAX_LEVEL(INST_ADVENTURE(inst)) > 0) {
			level = MIN(level, GET_ADV_MAX_LEVEL(INST_ADVENTURE(inst)));
		}
	}
	
	// vehicle constraints
	if (VEH_MIN_SCALE_LEVEL(veh) > 0) {
		level = MAX(level, VEH_MIN_SCALE_LEVEL(veh));
	}
	if (VEH_MAX_SCALE_LEVEL(veh) > 0) {
		level = MIN(level, VEH_MAX_SCALE_LEVEL(veh));
	}
	
	// rounding?
	if (round_level_scaling_to_nearest > 1 && level > 1 && (level % round_level_scaling_to_nearest) > 0) {
		level += (round_level_scaling_to_nearest - (level % round_level_scaling_to_nearest));
	}
	
	// set the level
	VEH_SCALE_LEVEL(veh) = level;
	request_vehicle_save_in_world(veh);
}


/**
* Updates the vehicle's half icon. It does no validation, so you must
* pre-validate the text as 2 visible characters wide.
*
* @param vehicle_data *veh The vehicle to change.
* @param const char *str The new half icon (will be copied). Or, NULL to set it back to the prototype.
*/
void set_vehicle_half_icon(vehicle_data *veh, const char *str) {
	vehicle_data *proto = vehicle_proto(VEH_VNUM(veh));
	
	if (VEH_HALF_ICON(veh) && (!proto || VEH_HALF_ICON(veh) != VEH_HALF_ICON(proto))) {
		free(VEH_HALF_ICON(veh));
	}
	VEH_HALF_ICON(veh) = (str ? str_dup(str) : (proto ? VEH_HALF_ICON(proto) : NULL));
	request_vehicle_save_in_world(veh);
}


/**
* Updates the vehicle's icon. It does no validation, so you must
* pre-validate the text as 4 visible characters wide.
*
* @param vehicle_data *veh The vehicle to change.
* @param const char *str The new icon (will be copied). Or, NULL to set it back to the prototype.
*/
void set_vehicle_icon(vehicle_data *veh, const char *str) {
	vehicle_data *proto = vehicle_proto(VEH_VNUM(veh));
	
	if (VEH_ICON(veh) && (!proto || VEH_ICON(veh) != VEH_ICON(proto))) {
		free(VEH_ICON(veh));
	}
	VEH_ICON(veh) = (str ? str_dup(str) : (proto ? VEH_ICON(proto) : NULL));
	request_vehicle_save_in_world(veh);
}


/**
* Updates the vehicle's keywords. It does no validation, so you must
* pre-validate the text.
*
* @param vehicle_data *veh The vehicle to change.
* @param const char *str The new keywords (will be copied). Or, NULL to set it back to the prototype.
*/
void set_vehicle_keywords(vehicle_data *veh, const char *str) {
	vehicle_data *proto = vehicle_proto(VEH_VNUM(veh));
	const char *default_val = "vehicle unknown";
	
	if (VEH_KEYWORDS(veh) && (!proto || VEH_KEYWORDS(veh) != VEH_KEYWORDS(proto))) {
		free(VEH_KEYWORDS(veh));
	}
	VEH_KEYWORDS(veh) = (str ? str_dup(str) : (proto ? VEH_KEYWORDS(proto) : str_dup(default_val)));
	request_vehicle_save_in_world(veh);
}


/**
* Updates the vehicle's long desc. It does no validation, so you must
* pre-validate the text.
*
* @param vehicle_data *veh The vehicle to change.
* @param const char *str The new long desc (will be copied). Or, NULL to set it back to the prototype.
*/
void set_vehicle_long_desc(vehicle_data *veh, const char *str) {
	vehicle_data *proto = vehicle_proto(VEH_VNUM(veh));
	const char *default_val = "An unknown vehicle is here.";
	
	if (VEH_LONG_DESC(veh) && (!proto || VEH_LONG_DESC(veh) != VEH_LONG_DESC(proto))) {
		free(VEH_LONG_DESC(veh));
	}
	VEH_LONG_DESC(veh) = (str ? str_dup(str) : (proto ? VEH_LONG_DESC(proto) : str_dup(default_val)));
	request_vehicle_save_in_world(veh);
}


/**
* Updates the vehicle's look desc. It does no validation, so you must
* pre-validate the text.
*
* @param vehicle_data *veh The vehicle to change.
* @param const char *str The new look desc (will be copied). Or, NULL to set it back to the prototype.
* @param bool format If TRUE, will format it as a paragraph (IF str was not-null).
*/
void set_vehicle_look_desc(vehicle_data *veh, const char *str, bool format) {
	vehicle_data *proto = vehicle_proto(VEH_VNUM(veh));
	const char *default_val = "An unknown vehicle is here.\r\n";
	char temp[MAX_STRING_LENGTH];
	const char *val = str;
	
	if (VEH_LOOK_DESC(veh) && (!proto || VEH_LOOK_DESC(veh) != VEH_LOOK_DESC(proto))) {
		free(VEH_LOOK_DESC(veh));
	}
	
	// check trailing crlf
	if (str && *str && str[strlen(str)-1] != '\r' && str[strlen(str)-1] != '\n') {
		strcpy(temp, str);
		strcat(temp, "\r\n");	// I think there's always room for this
		val = temp;
	}
	VEH_LOOK_DESC(veh) = (val ? str_dup(val) : (proto ? VEH_LOOK_DESC(proto) : str_dup(default_val)));
	
	// format if requested
	if (val && format) {
		format_text(&VEH_LOOK_DESC(veh), (strlen(VEH_LOOK_DESC(veh)) > 80 ? FORMAT_INDENT : 0), NULL, MAX_STRING_LENGTH);
	}
	
	request_vehicle_save_in_world(veh);
}


/**
* Updates the vehicle's short desc. It does no validation, so you must
* pre-validate the text.
*
* @param vehicle_data *veh The vehicle to change.
* @param const char *str The new short desc (will be copied). Or, NULL to set it back to the prototype.
*/
void set_vehicle_short_desc(vehicle_data *veh, const char *str) {
	vehicle_data *proto = vehicle_proto(VEH_VNUM(veh));
	const char *default_val = "an unknown vehicle";
	
	if (VEH_SHORT_DESC(veh) && (!proto || VEH_SHORT_DESC(veh) != VEH_SHORT_DESC(proto))) {
		free(VEH_SHORT_DESC(veh));
	}
	VEH_SHORT_DESC(veh) = (str ? str_dup(str) : (proto ? VEH_SHORT_DESC(proto) : str_dup(default_val)));
	request_vehicle_save_in_world(veh);
}


/**
* Updates the vehicle's look desc by adding to the end. It does no validation,
* so you must pre-validate the text.
*
* @param vehicle_data *veh The vehicle to change.
* @param const char *str The text to append to the look desc (will be copied).
* @param bool format If TRUE, will format it as a paragraph.
*/
void set_vehicle_look_desc_append(vehicle_data *veh, const char *str, bool format) {
	vehicle_data *proto = vehicle_proto(VEH_VNUM(veh));
	char temp[MAX_STRING_LENGTH];
	
	if (str && *str) {
		snprintf(temp, sizeof(temp), "%s%s", NULLSAFE(VEH_LOOK_DESC(veh)), str);
		
		// check trailing crlf
		if (str[strlen(str)-1] != '\r' && str[strlen(str)-1] != '\n') {
			strcat(temp, "\r\n");
		}
		if (VEH_LOOK_DESC(veh) && (!proto || VEH_LOOK_DESC(veh) != VEH_LOOK_DESC(proto))) {
			free(VEH_LOOK_DESC(veh));
		}
		VEH_LOOK_DESC(veh) = str_dup(temp);
		if (format) {
			format_text(&VEH_LOOK_DESC(veh), (strlen(VEH_LOOK_DESC(veh)) > 80 ? FORMAT_INDENT : 0), NULL, MAX_STRING_LENGTH);
		}
		
		request_vehicle_save_in_world(veh);
	}
}


/**
* Updates the vehicle's quarter icon. It does no validation, so you must
* pre-validate the text as 2 visible characters wide.
*
* @param vehicle_data *veh The vehicle to change.
* @param const char *str The new quarter icon (will be copied). Or, NULL to set it back to the prototype.
*/
void set_vehicle_quarter_icon(vehicle_data *veh, const char *str) {
	vehicle_data *proto = vehicle_proto(VEH_VNUM(veh));
	
	if (VEH_QUARTER_ICON(veh) && (!proto || VEH_QUARTER_ICON(veh) != VEH_QUARTER_ICON(proto))) {
		free(VEH_QUARTER_ICON(veh));
	}
	VEH_QUARTER_ICON(veh) = (str ? str_dup(str) : (proto ? VEH_QUARTER_ICON(proto) : NULL));
	request_vehicle_save_in_world(veh);
}


/**
* Begins the dismantle process on a vehicle including setting its
* VEH_DISMANTLING flag and its VEH_CONSTRUCTION_ID().
*
* @param vehicle_data *veh The vehicle to dismantle.
* @param char_data *ch Optional: Player who started the dismantle (may be NULL).
*/
void start_dismantle_vehicle(vehicle_data *veh, char_data *ch) {
	struct resource_data *res, *next_res;
	craft_data *craft = find_craft_for_vehicle(veh);
	obj_data *proto;
	struct vehicle_room_list *vrl;
	
	// remove from goals/tech
	if (VEH_OWNER(veh) && VEH_IS_COMPLETE(veh)) {
		qt_empire_players_vehicle(VEH_OWNER(veh), qt_lose_vehicle, veh);
		et_lose_vehicle(VEH_OWNER(veh), veh);
		adjust_vehicle_tech(veh, GET_ISLAND_ID(IN_ROOM(veh)), FALSE);
		
		// adjust tech on interior
		LL_FOREACH(VEH_ROOM_LIST(veh), vrl) {
			adjust_building_tech(VEH_OWNER(veh), vrl->room, FALSE);
		}
	}
	
	// any npcs living on it
	delete_vehicle_npcs(veh, NULL, TRUE);
	
	// clear it out
	vehicle_interior_dismantle_triggers(veh, ch);
	fully_empty_vehicle(veh, IN_ROOM(veh));
	delete_vehicle_interior(veh);
	
	// set up flags
	set_vehicle_flags(veh, VEH_DISMANTLING);
	VEH_CONSTRUCTION_ID(veh) = get_new_vehicle_construction_id();
	
	// remove any existing resources remaining to build/maintain
	if (VEH_NEEDS_RESOURCES(veh)) {
		free_resource_list(VEH_NEEDS_RESOURCES(veh));
		VEH_NEEDS_RESOURCES(veh) = NULL;
	}
	
	// set up refundable resources: use the actual materials that built this thing
	if (VEH_BUILT_WITH(veh)) {
		VEH_NEEDS_RESOURCES(veh) = VEH_BUILT_WITH(veh);
		VEH_BUILT_WITH(veh) = NULL;
	}
	// remove liquids, etc
	LL_FOREACH_SAFE(VEH_NEEDS_RESOURCES(veh), res, next_res) {
		// RES_x: only RES_OBJECT is refundable
		if (res->type != RES_OBJECT) {
			LL_DELETE(VEH_NEEDS_RESOURCES(veh), res);
			free(res);
		}
		else {	// is object -- check for single_use
			if (!(proto = obj_proto(res->vnum)) || OBJ_FLAGGED(proto, OBJ_SINGLE_USE)) {
				LL_DELETE(VEH_NEEDS_RESOURCES(veh), res);
				free(res);
			}
		}
	}
	
	if (!craft || (!CRAFT_FLAGGED(craft, CRAFT_FULL_DISMANTLE_REFUND) && (!CRAFT_FLAGGED(craft, CRAFT_UNDAMAGED_DISMANTLE_REFUND) || VEH_HEALTH(veh) < VEH_MAX_HEALTH(veh)))) {
		// reduce resource: they don't get it all back
		reduce_dismantle_resources(VEH_MAX_HEALTH(veh) - VEH_HEALTH(veh), VEH_MAX_HEALTH(veh), &VEH_NEEDS_RESOURCES(veh));
	}
	
	// ensure it has no people/mobs on it
	if (VEH_LED_BY(veh)) {
		GET_LEADING_VEHICLE(VEH_LED_BY(veh)) = NULL;
		VEH_LED_BY(veh) = NULL;
	}
	if (VEH_SITTING_ON(veh)) {
		unseat_char_from_vehicle(VEH_SITTING_ON(veh));
	}
	if (VEH_DRIVER(veh)) {
		GET_DRIVING(VEH_DRIVER(veh)) = NULL;
		VEH_DRIVER(veh) = NULL;
	}
	while (VEH_ANIMALS(veh)) {
		unharness_mob_from_vehicle(VEH_ANIMALS(veh), veh);
	}
	
	affect_total_room(IN_ROOM(veh));
	request_vehicle_save_in_world(veh);
	if (VEH_IS_VISIBLE_ON_MAPOUT(veh)) {
		request_mapout_update(GET_ROOM_VNUM(IN_ROOM(veh)));
	}
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
	set_vehicle_flags(veh, VEH_ON_FIRE);

	if (VEH_SITTING_ON(veh)) {
		do_unseat_from_vehicle(VEH_SITTING_ON(veh));
	}
	if (VEH_LED_BY(veh)) {
		act("You stop leading $V.", FALSE, VEH_LED_BY(veh), NULL, veh, TO_CHAR | ACT_VEH_VICT);
		GET_LEADING_VEHICLE(VEH_LED_BY(veh)) = NULL;
		VEH_LED_BY(veh) = NULL;
	}
}


/**
* Determines the total number of vehicles in the room that don't have a size.
*
* @param room_data *room The room to check.
* @param empire_data *exclude_hostile_to_empire Optional: Ignore people who are hostile to this empire because they're not allowed to block them in. (Pass NULL to ignore.)
* @return int The total number of size-zero vehicles there.
*/
int total_small_vehicles_in_room(room_data *room, empire_data *exclude_hostile_to_empire) {
	vehicle_data *veh;
	int count = 0;
	
	DL_FOREACH2(ROOM_VEHICLES(room), veh, next_in_room) {
		if (VEH_SIZE(veh) > 0) {
			continue;	// skip large ones
		}
		if (exclude_hostile_to_empire && VEH_OWNER(veh) && VEH_OWNER(veh) != exclude_hostile_to_empire && has_relationship(exclude_hostile_to_empire, VEH_OWNER(veh), DIPL_WAR | DIPL_DISTRUST)) {
			continue;	// skip hostiles
		}
		
		// otherwise go ahead and count it
		++count;
	}
	
	return count;
}


/**
* Determines the total size of all vehicles in the room.
*
* @param room_data *room The room to check.
* @param empire_data *exclude_hostile_to_empire Optional: Ignore people who are hostile to this empire because they're not allowed to block them in. (Pass NULL to ignore.)
* @return int The total size of vehicles there.
*/
int total_vehicle_size_in_room(room_data *room, empire_data *exclude_hostile_to_empire) {
	vehicle_data *veh;
	int size = 0;
	
	DL_FOREACH2(ROOM_VEHICLES(room), veh, next_in_room) {
		if (VEH_SIZE(veh) == 0) {
			continue;	// skip small ones
		}
		if (exclude_hostile_to_empire && VEH_OWNER(veh) && VEH_OWNER(veh) != exclude_hostile_to_empire && has_relationship(exclude_hostile_to_empire, VEH_OWNER(veh), DIPL_WAR | DIPL_DISTRUST)) {
			continue;	// skip hostiles
		}
		
		size += VEH_SIZE(veh);
	}
	
	return size;
}


/**
* Counts vehicles that are owned by the empire in a room. If the room itself
* is owned, also counts unowned vehicles there.
*
* @param room_data *room The room to check.
* @param empire_data *emp The empire to look for.
* @return int The number of vehicles there.
*/
int total_vehicles_in_room_by_empire(room_data *room, empire_data *emp) {
	vehicle_data *veh;
	int count = 0;
	
	DL_FOREACH2(ROOM_VEHICLES(room), veh, next_in_room) {
		if (VEH_OWNER(veh) == emp || (!VEH_OWNER(veh) && ROOM_OWNER(room) == emp)) {
			++count;
		}
	}
	
	return count;
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
	char_data *mob;
	
	// safety first
	if (!vam || !veh) {
		return NULL;
	}
	
	// remove the vam entry now
	LL_DELETE(VEH_ANIMALS(veh), vam);
	request_vehicle_save_in_world(veh);
	
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


/**
* Updates the island/id and map location for the rooms inside a vehicle.
*
* @param vehicle_data *veh Which vehicle to update.
* @param room_data *loc Where the vehicle is.
*/
void update_vehicle_island_and_loc(vehicle_data *veh, room_data *loc) {
	struct vehicle_room_list *vrl;
	vehicle_data *iter;
	
	if (!veh) {
		return;
	}
	
	LL_FOREACH(VEH_ROOM_LIST(veh), vrl) {
		GET_MAP_LOC(vrl->room) = GET_MAP_LOC(loc);
		if (GET_ISLAND_ID(vrl->room) != GET_ISLAND_ID(loc) || GET_ISLAND(vrl->room) != GET_ISLAND(loc)) {
			GET_ISLAND_ID(vrl->room) = GET_ISLAND_ID(loc);
			GET_ISLAND(vrl->room) = GET_ISLAND(loc);
			request_world_save(GET_ROOM_VNUM(vrl->room), WSAVE_ROOM);
		}
		
		// check vehicles inside and cascade
		DL_FOREACH2(ROOM_VEHICLES(vrl->room), iter, next_in_room) {
			update_vehicle_island_and_loc(iter, loc);
		}
	}
}


/**
* Determines if a vehicle is allowed to be in a room based on climate. This
* only applies on the map. Open map buildings use the base sector; interiors
* are always allowed.
*
* @param vehicle_data *veh The vehicle to test.
* @param room_data *room What room to check for climate.
* @param bool *allow_slow_ruin Optional: A variable to pass back whether or not it can slowly ruin here (rather than all at once). Pass NULL to skip this.
* @return bool TRUE if the vehicle allows it; FALSE if not.
*/
bool vehicle_allows_climate(vehicle_data *veh, room_data *room, bool *allow_slow_ruin) {
	bitvector_t climate = NOBITS;
	bool ok = TRUE;
	
	if (allow_slow_ruin) {
		// default this to true; change if needed
		*allow_slow_ruin = TRUE;
	}
	
	if (!veh || !room) {
		return TRUE;	// junk in, junk out
	}
	
	// climate is complex
	climate = get_climate(room);
	
	// compare
	if (VEH_REQUIRES_CLIMATE(veh) && !(VEH_REQUIRES_CLIMATE(veh) & climate)) {
		// required climate type is missing
		ok = FALSE;
		if (allow_slow_ruin) {
			*allow_slow_ruin &= check_climate_ruins_vehicle_slowly(VEH_REQUIRES_CLIMATE(veh) & ~climate, CRVS_WHEN_LOSING);
		}
	}
	if (VEH_FORBID_CLIMATE(veh) && (VEH_FORBID_CLIMATE(veh) & climate)) {
		// has a forbidden climate type
		ok = FALSE;
		if (allow_slow_ruin) {
			*allow_slow_ruin &= check_climate_ruins_vehicle_slowly(VEH_FORBID_CLIMATE(veh) & climate, CRVS_WHEN_GAINING);
		}
	}
	
	return ok;
}


/**
* Runs the dismantle trigger on all interior rooms of a vehicle. This dismantle
* is not preventable, but is necessary for some rooms, like the Study, which
* remove unique items during dismantle.
*
* @param vehicle_data *veh The vehicle being dismantled.
* @param char_data *ch Optional: Who started dismantling it (may be NULL).
*/
void vehicle_interior_dismantle_triggers(vehicle_data *veh, char_data *ch) {
	struct vehicle_room_list *vrl;
	
	LL_FOREACH(VEH_ROOM_LIST(veh), vrl) {
		dismantle_wtrigger(vrl->room, ch, FALSE);
	}
}


/**
* Determines if a vehicle can be seen at a distance or not, using the chameleon
* flag and whether or not the vehicle is complete.
*
* @param vehicle_data *veh The vehicle.
* @param room_data *from Where it's being viewed from (to compute distance).
* @return bool TRUE if the vehicle should be hidden, FALSE if not.
*/
bool vehicle_is_chameleon(vehicle_data *veh, room_data *from) {
	if (!veh || !from || !IN_ROOM(veh)) {
		return FALSE;	// safety
	}
	if (!VEH_IS_COMPLETE(veh) || VEH_NEEDS_RESOURCES(veh)) {
		return FALSE;	// incomplete or unrepaired vehicles are not chameleon
	}
	if (!VEH_FLAGGED(veh, VEH_CHAMELEON) && (!IN_ROOM(veh) || !ROOM_AFF_FLAGGED(IN_ROOM(veh), ROOM_AFF_CHAMELEON))) {
		return FALSE;	// missing chameleon flags
	}
	
	// ok chameleon: now check distance
	return (compute_distance(from, IN_ROOM(veh)) >= 2);
}


/**
* Counts the words of text in a vehicle's strings.
*
* @param vehicle_data *veh The vehicle whose strings to count.
* @return int The number of words in the vehicle's strings.
*/
int wordcount_vehicle(vehicle_data *veh) {
	int count = 0;
	
	count += wordcount_string(VEH_KEYWORDS(veh));
	count += wordcount_string(VEH_SHORT_DESC(veh));
	count += wordcount_string(VEH_LONG_DESC(veh));
	count += wordcount_string(VEH_LOOK_DESC(veh));
	count += wordcount_extra_descriptions(VEH_EX_DESCS(veh));
	count += wordcount_custom_messages(VEH_CUSTOM_MSGS(veh));
	
	return count;
}


 //////////////////////////////////////////////////////////////////////////////
//// UTILITIES ///////////////////////////////////////////////////////////////

/**
* Adds a room to a vehicle's tracking list.
*
* @param room_data *room The room.
* @param vehicle_data *veh The vehicle to add it to.
*/
void add_room_to_vehicle(room_data *room, vehicle_data *veh) {
	struct vehicle_room_list *vrl;
	CREATE(vrl, struct vehicle_room_list, 1);
	vrl->room = room;
	LL_APPEND(VEH_ROOM_LIST(veh), vrl);
	
	SET_BIT(ROOM_BASE_FLAGS(room), ROOM_AFF_IN_VEHICLE);
	if (COMPLEX_DATA(room)) {
		COMPLEX_DATA(room)->vehicle = veh;
	}
	
	// count all rooms after the first
	if (room != VEH_INTERIOR_HOME_ROOM(veh)) {
		++VEH_INSIDE_ROOMS(veh);
	}
	
	affect_total_room(room);
	
	// initial island data
	if (IN_ROOM(veh)) {
		update_vehicle_island_and_loc(veh, IN_ROOM(veh));
	}
}


/**
* Applies a vehicle's traits/data to an island, including empire-island data if
* it's claimed. This also sets up the inside of the vehicle.
*
* @param vehicle_data *veh The vehicle.
* @param int island_id Which island to apply to, if any.
*/
void apply_vehicle_to_island(vehicle_data *veh, int island_id) {
	struct vehicle_room_list *vrl;
	vehicle_data *iter;
	
	if (!veh || VEH_APPLIED_TO_ISLAND(veh) == island_id) {
		return;	// no change or no vehicle
	}
	
	// remove first
	unapply_vehicle_to_island(veh);
	
	// update island id
	VEH_APPLIED_TO_ISLAND(veh) = island_id;
	
	// cascade to interior rooms and vehicles
	if (VEH_ROOM_LIST(veh)) {
		LL_FOREACH(VEH_ROOM_LIST(veh), vrl) {
			// adjust techs for this island
			if (VEH_OWNER(veh)) {
				adjust_building_tech(VEH_OWNER(veh), vrl->room, TRUE);
			}
			
			// check vehicles inside and cascade
			DL_FOREACH2(ROOM_VEHICLES(vrl->room), iter, next_in_room) {
				apply_vehicle_to_island(iter, island_id);
			}
		}
	}
	
	// and apply tech
	adjust_vehicle_tech(veh, island_id, TRUE);
}


/**
* Applies a vehicle's traits to a room. If they were already applied to another
* room, they will be removed from there first -- unless the two rooms are on
* the same island, in which case the applied-to-room is merely updated.
*
* @param vehicle_data *veh The vehicle.
* @param room_data *room The room to apply it to.
*/
void apply_vehicle_to_room(vehicle_data *veh, room_data *room) {
	if (room == VEH_APPLIED_TO_ROOM(veh)) {
		return;	// same room / no work
	}
	
	// ok: remove from old room first
	unapply_vehicle_to_room(veh);
	VEH_APPLIED_TO_ROOM(veh) = room;
	
	// update map/island pointers inside the vehicle
	update_vehicle_island_and_loc(veh, room);
	
	// and apply to the island (unapply_vehicle_to_room may have removed it from the island)
	apply_vehicle_to_island(veh, room ? GET_ISLAND_ID(room) : NO_ISLAND);
	
	// check lights
	if (VEH_PROVIDES_LIGHT(veh)) {
		++ROOM_LIGHTS(room);
	}
}


/**
* Checks for common vehicle problems and reports them to ch.
*
* @param vehicle_data *veh The item to audit.
* @param char_data *ch The person to report to.
* @return bool TRUE if any problems were reported; FALSE if all good.
*/
bool audit_vehicle(vehicle_data *veh, char_data *ch) {
	char temp[MAX_STRING_LENGTH], *ptr;
	bld_data *interior = building_proto(VEH_INTERIOR_ROOM_VNUM(veh)), *proto;
	struct obj_storage_type *store;
	struct bld_relation *relat;
	obj_data *obj, *next_obj;
	bool any, problem = FALSE;
	
	if (!str_cmp(VEH_KEYWORDS(veh), default_vehicle_keywords)) {
		olc_audit_msg(ch, VEH_VNUM(veh), "Keywords not set");
		problem = TRUE;
	}
	
	ptr = VEH_KEYWORDS(veh);
	do {
		ptr = any_one_arg(ptr, temp);
		if (*temp && !str_str(VEH_SHORT_DESC(veh), temp) && !str_str(VEH_LONG_DESC(veh), temp)) {
			olc_audit_msg(ch, VEH_VNUM(veh), "Keyword '%s' not found in strings", temp);
			problem = TRUE;
		}
	} while (*ptr);
	
	if (!str_cmp(VEH_LONG_DESC(veh), default_vehicle_long_desc)) {
		olc_audit_msg(ch, VEH_VNUM(veh), "Long desc not set");
		problem = TRUE;
	}
	if (!ispunct(VEH_LONG_DESC(veh)[strlen(VEH_LONG_DESC(veh)) - 1])) {
		olc_audit_msg(ch, VEH_VNUM(veh), "Long desc missing punctuation");
		problem = TRUE;
	}
	if (islower(*VEH_LONG_DESC(veh))) {
		olc_audit_msg(ch, VEH_VNUM(veh), "Long desc not capitalized");
		problem = TRUE;
	}
	if (!str_cmp(VEH_SHORT_DESC(veh), default_vehicle_short_desc)) {
		olc_audit_msg(ch, VEH_VNUM(veh), "Short desc not set");
		problem = TRUE;
	}
	any_one_arg(VEH_SHORT_DESC(veh), temp);
	if ((fill_word(temp) || reserved_word(temp)) && isupper(*temp)) {
		olc_audit_msg(ch, VEH_VNUM(veh), "Short desc capitalized");
		problem = TRUE;
	}
	if (ispunct(VEH_SHORT_DESC(veh)[strlen(VEH_SHORT_DESC(veh)) - 1])) {
		olc_audit_msg(ch, VEH_VNUM(veh), "Short desc has punctuation");
		problem = TRUE;
	}
	
	ptr = VEH_SHORT_DESC(veh);
	do {
		ptr = any_one_arg(ptr, temp);
		// remove trailing punctuation
		while (*temp && ispunct(temp[strlen(temp)-1])) {
			temp[strlen(temp)-1] = '\0';
		}
		if (*temp && !fill_word(temp) && !reserved_word(temp) && !isname(temp, VEH_KEYWORDS(veh)) && search_block(temp, ignore_missing_keywords, TRUE) == NOTHING) {
			olc_audit_msg(ch, VEH_VNUM(veh), "Suggested missing keyword '%s'", temp);
			problem = TRUE;
		}
	} while (*ptr);
	
	if (!VEH_LOOK_DESC(veh) || !*VEH_LOOK_DESC(veh) || !str_cmp(VEH_LOOK_DESC(veh), "Nothing.\r\n")) {
		olc_audit_msg(ch, VEH_VNUM(veh), "Look desc not set");
		problem = TRUE;
	}
	else if (!strn_cmp(VEH_LOOK_DESC(veh), "Nothing.", 8)) {
		olc_audit_msg(ch, VEH_VNUM(veh), "Look desc starting with 'Nothing.'");
		problem = TRUE;
	}
	
	if (VEH_ICON(veh) && !validate_icon(VEH_ICON(veh), 4)) {
		olc_audit_msg(ch, VEH_VNUM(veh), "Bad icon set");
		problem = TRUE;
	}
	if (VEH_HALF_ICON(veh) && !validate_icon(VEH_HALF_ICON(veh), 2)) {
		olc_audit_msg(ch, VEH_VNUM(veh), "Bad half icon set");
		problem = TRUE;
	}
	if (VEH_QUARTER_ICON(veh) && !validate_icon(VEH_QUARTER_ICON(veh), 1)) {
		olc_audit_msg(ch, VEH_VNUM(veh), "Bad quarter icon set");
		problem = TRUE;
	}
	
	if (VEH_MAX_HEALTH(veh) < 1) {
		olc_audit_msg(ch, VEH_VNUM(veh), "Hitpoints set lower than 1");
		problem = TRUE;
	}
	
	if (VEH_INTERIOR_ROOM_VNUM(veh) != NOTHING && (!interior || !IS_SET(GET_BLD_FLAGS(interior), BLD_ROOM))) {
		olc_audit_msg(ch, VEH_VNUM(veh), "Interior room set to invalid building vnum");
		problem = TRUE;
	}
	if (VEH_MAX_ROOMS(veh) > 0 && !interior) {
		olc_audit_msg(ch, VEH_VNUM(veh), "Extra rooms set but vehicle has no interior");
		problem = TRUE;
	}
	if (VEH_DESIGNATE_FLAGS(veh) && !interior) {
		olc_audit_msg(ch, VEH_VNUM(veh), "Designate flags set but vehicle has no interior");
		problem = TRUE;
	}
	if (!VEH_REGULAR_MAINTENANCE(veh) && !VEH_FLAGGED(veh, VEH_IS_RUINS)) {
		olc_audit_msg(ch, VEH_VNUM(veh), "Requires no maintenance");
		problem = TRUE;
	}
	
	// flags
	if (VEH_FLAGGED(veh, VEH_INCOMPLETE)) {
		olc_audit_msg(ch, VEH_VNUM(veh), "INCOMPLETE flag");
		problem = TRUE;
	}
	if (VEH_FLAGGED(veh, VEH_DISMANTLING)) {
		olc_audit_msg(ch, VEH_VNUM(veh), "DISMANTLING flag");
		problem = TRUE;
	}
	if (VEH_FLAGGED(veh, VEH_CONTAINER | VEH_SHIPPING) && VEH_CAPACITY(veh) < 1) {
		olc_audit_msg(ch, VEH_VNUM(veh), "No capacity set when container or shipping flag present");
		problem = TRUE;
	}
	if (!VEH_FLAGGED(veh, VEH_CONTAINER | VEH_SHIPPING) && VEH_CAPACITY(veh) > 0) {
		olc_audit_msg(ch, VEH_VNUM(veh), "Capacity set without container or shipping flag");
		problem = TRUE;
	}
	if (VEH_FLAGGED(veh, VEH_ALLOW_ROUGH) && !VEH_FLAGGED(veh, VEH_DRIVING | VEH_DRAGGABLE)) {
		olc_audit_msg(ch, VEH_VNUM(veh), "ALLOW-ROUGH set without driving/draggable");
		problem = TRUE;
	}
	if (VEH_FLAGGED(veh, VEH_IN) && !VEH_FLAGGED(veh, VEH_SIT | VEH_SLEEP) && VEH_INTERIOR_ROOM_VNUM(veh) == NOTHING) {
		olc_audit_msg(ch, VEH_VNUM(veh), "IN flag set without SIT/SLEEP or interior room");
		problem = TRUE;
	}
	if (VEH_FLAGGED(veh, VEH_CARRY_VEHICLES | VEH_CARRY_MOBS) && VEH_INTERIOR_ROOM_VNUM(veh) == NOTHING) {
		olc_audit_msg(ch, VEH_VNUM(veh), "CARRY-* flag present without interior room");
		problem = TRUE;
	}
	if (VEH_FLAGGED(veh, VEH_ON_FIRE)) {
		olc_audit_msg(ch, VEH_VNUM(veh), "ON-FIRE flag");
		problem = TRUE;
	}
	if (VEH_FLAGGED(veh, VEH_NO_CLAIM) && IS_SET(VEH_FUNCTIONS(veh), FNC_IN_CITY_ONLY)) {
		olc_audit_msg(ch, VEH_VNUM(veh), "Has !CLAIM flag but IN-CITY-ONLY function; will never function");
		problem = TRUE;
	}
	if (VEH_FLAGGED(veh, VEH_BUILDING) && VEH_FLAGGED(veh, MOVABLE_VEH_FLAGS)) {
		olc_audit_msg(ch, VEH_VNUM(veh), "Has both BUILDING flag and at least 1 movement flag");
		problem = TRUE;
	}
	if (VEH_FLAGGED(veh, VEH_BUILDING) && !VEH_FLAGGED(veh, VEH_VISIBLE_IN_DARK)) {
		olc_audit_msg(ch, VEH_VNUM(veh), "Has BUILDING flag but not VISIBLE-IN-DARK");
		problem = TRUE;
	}
	if (VEH_FLAGGED(veh, VEH_INTERLINK) && (!VEH_FLAGGED(veh, VEH_BUILDING) || VEH_FLAGGED(veh, MOVABLE_VEH_FLAGS))) {
		olc_audit_msg(ch, VEH_VNUM(veh), "Has INTERLINK but is not a stationary building");
		problem = TRUE;
	}
	
	// functions
	if (IS_SET(VEH_FUNCTIONS(veh), IMMOBILE_FNCS) && VEH_FLAGGED(veh, MOVABLE_VEH_FLAGS)) {
		olc_audit_msg(ch, VEH_VNUM(veh), "Has non-movable functions on a movable vehicle");
		problem = TRUE;
	}
	
	if (VEH_FLAGGED(veh, VEH_BUILDING) && !VEH_FLAGGED(veh, VEH_IS_RUINS) && !has_interaction(VEH_INTERACTIONS(veh), INTERACT_RUINS_TO_VEH)) {
		olc_audit_msg(ch, VEH_VNUM(veh), "No RUINS-TO-VEH interactions");
		problem = TRUE;
	}
	if (has_interaction(VEH_INTERACTIONS(veh), INTERACT_RUINS_TO_BLD)) {
		olc_audit_msg(ch, VEH_VNUM(veh), "Has RUINS-TO-BLD interaction; this won't work on vehicles");
		problem = TRUE;
	}
	if (VEH_HEIGHT(veh) < 0 || VEH_HEIGHT(veh) > 5) {
		olc_audit_msg(ch, VEH_VNUM(veh), "Unusual height: %d", VEH_HEIGHT(veh));
		problem = TRUE;
	}
	
	// check for storage on moving vehicles
	if (VEH_FLAGGED(veh, MOVABLE_VEH_FLAGS)) {
		any = FALSE;
		HASH_ITER(hh, object_table, obj, next_obj) {
			if (any) {
				break;
			}
			LL_FOREACH(GET_OBJ_STORAGE(obj), store) {
				if (store->type == TYPE_VEH && store->vnum == VEH_VNUM(veh)) {
					olc_audit_msg(ch, VEH_VNUM(veh), "At least 1 object stores to moving vehicle: %d %s", GET_OBJ_VNUM(obj), GET_OBJ_SHORT_DESC(obj));
					problem = any = TRUE;
				}
			}
		}
	}
	
	LL_FOREACH(VEH_RELATIONS(veh), relat) {
		if ((relat->type == BLD_REL_UPGRADES_TO_BLD || relat->type == BLD_REL_FORCE_UPGRADE_BLD) && (proto = building_proto(relat->vnum)) && !BLD_FLAGGED(proto, BLD_OPEN)) {
			olc_audit_msg(ch, VEH_VNUM(veh), "UPGRADES-TO a non-open building (cannot do this because there's no 'facing' information for the upgrade)");
			problem = TRUE;
		}
	}
	
	problem |= audit_extra_descs(VEH_VNUM(veh), VEH_EX_DESCS(veh), ch);
	problem |= audit_interactions(VEH_VNUM(veh), VEH_INTERACTIONS(veh), TYPE_ROOM, ch);
	problem |= audit_spawns(VEH_VNUM(veh), VEH_SPAWNS(veh), ch);
	
	return problem;
}


/**
* This is called when a vehicle no longer needs resources. It checks flags and
* health, and it will also remove the INCOMPLETE flag if present. It can safely
* be called on vehicles that are done being repaired, too.
*
* WARNING: Calling this on a vehicle that is being dismantled will result in
* it passing to finish_dismantle_vehicle(), which can purge the vehicle. Load
* triggers could also purge it.
*
* @param vehicle_data *veh The vehicle.
*/
void complete_vehicle(vehicle_data *veh) {
	room_data *room = IN_ROOM(veh);	// store room in case veh is purged during a trigger
	struct vehicle_room_list *vrl;
	
	if (VEH_IS_DISMANTLING(veh)) {
		// short-circuit out to dismantling
		finish_dismantle_vehicle(NULL, veh);
		return;
	}
	
	// restore
	VEH_HEALTH(veh) = VEH_MAX_HEALTH(veh);
	
	// remove resources
	free_resource_list(VEH_NEEDS_RESOURCES(veh));
	VEH_NEEDS_RESOURCES(veh) = NULL;
	
	// cancel construction id
	VEH_CONSTRUCTION_ID(veh) = NOTHING;
	
	// only if it was incomplete:
	if (VEH_FLAGGED(veh, VEH_INCOMPLETE)) {
		remove_vehicle_flags(veh, VEH_INCOMPLETE);
		
		if (VEH_OWNER(veh)) {
			qt_empire_players_vehicle(VEH_OWNER(veh), qt_gain_vehicle, veh);
			et_gain_vehicle(VEH_OWNER(veh), veh);
			adjust_vehicle_tech(veh, GET_ISLAND_ID(IN_ROOM(veh)), TRUE);
			
			// adjust tech on existing interior?
			LL_FOREACH(VEH_ROOM_LIST(veh), vrl) {
				adjust_building_tech(VEH_OWNER(veh), vrl->room, TRUE);
			}
		}
		
		finish_vehicle_setup(veh);
		
		// build the interior if not built?
		get_vehicle_interior(veh);
		
		// run triggers
		load_vtrigger(veh);
	}
	
	if (room) {
		affect_total_room(room);
		request_vehicle_save_in_world(veh);
		if (VEH_IS_VISIBLE_ON_MAPOUT(veh)) {
			request_mapout_update(GET_ROOM_VNUM(room));	// in case
		}
	}
}


/**
* Saves the vehicles list for a room to the room file. This is called
* recursively to save in reverse-order and thus load in correct order.
*
* @param vehicle_data *veh The vehicle to save.
* @param FILE *fl The file open for writing.
*/
void Crash_save_vehicles(vehicle_data *veh, FILE *fl) {
	// store next first so the order is right on reboot
	if (veh && veh->next_in_room) {
		Crash_save_vehicles(veh->next_in_room, fl);
	}
	if (veh) {
		store_one_vehicle_to_file(veh, fl);
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
	vehicle_data *veh;
	room_data *room, *next_room;
	bool found = FALSE;
	
	// reverse-link the home-room of vehicles to this one
	DL_FOREACH(vehicle_list, veh) {
		if (VEH_INTERIOR_HOME_ROOM(veh)) {
			COMPLEX_DATA(VEH_INTERIOR_HOME_ROOM(veh))->vehicle = veh;
		}
	}
	
	DL_FOREACH_SAFE2(interior_room_list, room, next_room, next_interior) {
		// check for orphaned ship rooms
		if (ROOM_AFF_FLAGGED(room, ROOM_AFF_IN_VEHICLE) && HOME_ROOM(room) == room && !GET_ROOM_VEHICLE(room)) {
			delete_room(room, FALSE);	// must check_all_exits later
			found = TRUE;
		}
		// otherwise add the room to the vehicle's interior list
		else if (GET_ROOM_VEHICLE(room)) {
			add_room_to_vehicle(room, GET_ROOM_VEHICLE(room));
		}
	}
	
	// only bother this if we deleted anything
	if (found) {
		check_all_exits();
	}
	
	// need to update territory counts
	read_empire_territory(NULL, FALSE);
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
	vehicle_data *veh_iter, *next_veh_iter;
	struct obj_storage_type *store;
	struct interaction_item *inter;
	ability_data *abil, *next_abil;
	craft_data *craft, *next_craft;
	quest_data *quest, *next_quest;
	progress_data *prg, *next_prg;
	room_template *rmt, *next_rmt;
	social_data *soc, *next_soc;
	shop_data *shop, *next_shop;
	struct adventure_spawn *asp;
	struct bld_relation *relat;
	bld_data *bld, *next_bld;
	obj_data *obj, *next_obj;
	int size, found;
	bool any;
	
	if (!veh) {
		msg_to_char(ch, "There is no vehicle %d.\r\n", vnum);
		return;
	}
	
	found = 0;
	size = snprintf(buf, sizeof(buf), "Occurrences of vehicle %d (%s):\r\n", vnum, VEH_SHORT_DESC(veh));
	
	// abilities
	HASH_ITER(hh, ability_table, abil, next_abil) {
		any = FALSE;
		LL_FOREACH(ABIL_INTERACTIONS(abil), inter) {
			if (interact_data[inter->type].vnum_type == TYPE_VEH && inter->vnum == vnum) {
				any = TRUE;
				break;
			}
		}
		
		if (any) {
			++found;
			size += snprintf(buf + size, sizeof(buf) - size, "ABIL [%5d] %s\r\n", ABIL_VNUM(abil), ABIL_NAME(abil));
		}
	}
	
	// buildings
	HASH_ITER(hh, building_table, bld, next_bld) {
		any = FALSE;
		LL_FOREACH(GET_BLD_INTERACTIONS(bld), inter) {
			if (interact_data[inter->type].vnum_type == TYPE_VEH && inter->vnum == vnum) {
				any = TRUE;
				break;
			}
		}
		LL_FOREACH(GET_BLD_RELATIONS(bld), relat) {
			if (relat->vnum != vnum) {
				continue;
			}
			if (bld_relationship_vnum_types[relat->type] != TYPE_VEH) {
				continue;
			}
			any = TRUE;
			break;
		}
		if (any) {
			++found;
			size += snprintf(buf + size, sizeof(buf) - size, "BLD [%5d] %s\r\n", GET_BLD_VNUM(bld), GET_BLD_NAME(bld));
		}
	}
	
	// crafts
	HASH_ITER(hh, craft_table, craft, next_craft) {
		if (size >= sizeof(buf)) {
			break;
		}
		if (CRAFT_IS_VEHICLE(craft) && GET_CRAFT_OBJECT(craft) == vnum) {
			++found;
			size += snprintf(buf + size, sizeof(buf) - size, "CFT [%5d] %s\r\n", GET_CRAFT_VNUM(craft), GET_CRAFT_NAME(craft));
		}
	}
	
	// obj storage
	HASH_ITER(hh, object_table, obj, next_obj) {
		if (size >= sizeof(buf)) {
			break;
		}
		any = FALSE;
		for (store = GET_OBJ_STORAGE(obj); store && !any; store = store->next) {
			if (store->type == TYPE_VEH && store->vnum == vnum) {
				any = TRUE;
				++found;
				size += snprintf(buf + size, sizeof(buf) - size, "OBJ [%5d] %s\r\n", GET_OBJ_VNUM(obj), GET_OBJ_SHORT_DESC(obj));
			}
		}
	}
	
	// progress
	HASH_ITER(hh, progress_table, prg, next_prg) {
		if (size >= sizeof(buf)) {
			break;
		}
		// REQ_x: requirement search
		any = find_requirement_in_list(PRG_TASKS(prg), REQ_OWN_VEHICLE, vnum);
		
		if (any) {
			++found;
			size += snprintf(buf + size, sizeof(buf) - size, "PRG [%5d] %s\r\n", PRG_VNUM(prg), PRG_NAME(prg));
		}
	}
	
	// quests
	HASH_ITER(hh, quest_table, quest, next_quest) {
		if (size >= sizeof(buf)) {
			break;
		}
		any = find_requirement_in_list(QUEST_TASKS(quest), REQ_OWN_VEHICLE, vnum);
		any |= find_requirement_in_list(QUEST_PREREQS(quest), REQ_OWN_VEHICLE, vnum);
		any |= find_quest_giver_in_list(QUEST_STARTS_AT(quest), QG_VEHICLE, vnum);
		any |= find_quest_giver_in_list(QUEST_ENDS_AT(quest), QG_VEHICLE, vnum);
		
		if (any) {
			++found;
			size += snprintf(buf + size, sizeof(buf) - size, "QST [%5d] %s\r\n", QUEST_VNUM(quest), QUEST_NAME(quest));
		}
	}
	
	// room templates
	HASH_ITER(hh, room_template_table, rmt, next_rmt) {
		LL_FOREACH(GET_RMT_SPAWNS(rmt), asp) {
			if (asp->type == ADV_SPAWN_VEH && asp->vnum == vnum) {
				++found;
				size += snprintf(buf + size, sizeof(buf) - size, "RMT [%5d] %s\r\n", GET_RMT_VNUM(rmt), GET_RMT_TITLE(rmt));
				break;	// only need 1
			}
		}
	}
	
	// on shops
	HASH_ITER(hh, shop_table, shop, next_shop) {
		if (size >= sizeof(buf)) {
			break;
		}
		// QG_x: shop types
		any = find_quest_giver_in_list(SHOP_LOCATIONS(shop), QG_VEHICLE, vnum);
		
		if (any) {
			++found;
			size += snprintf(buf + size, sizeof(buf) - size, "SHOP [%5d] %s\r\n", SHOP_VNUM(shop), SHOP_NAME(shop));
		}
	}
	
	// socials
	HASH_ITER(hh, social_table, soc, next_soc) {
		if (size >= sizeof(buf)) {
			break;
		}
		any = find_requirement_in_list(SOC_REQUIREMENTS(soc), REQ_OWN_VEHICLE, vnum);
		
		if (any) {
			++found;
			size += snprintf(buf + size, sizeof(buf) - size, "SOC [%5d] %s\r\n", SOC_VNUM(soc), SOC_NAME(soc));
		}
	}
	
	// vehicles
	HASH_ITER(hh, vehicle_table, veh_iter, next_veh_iter) {
		any = FALSE;
		LL_FOREACH(VEH_INTERACTIONS(veh_iter), inter) {
			if (interact_data[inter->type].vnum_type == TYPE_VEH && inter->vnum == vnum) {
				any = TRUE;
				break;
			}
		}
		LL_FOREACH(VEH_RELATIONS(veh_iter), relat) {
			if (relat->vnum != vnum) {
				continue;
			}
			if (bld_relationship_vnum_types[relat->type] != TYPE_VEH) {
				continue;
			}
			any = TRUE;
			break;
		}
		if (any) {
			++found;
			size += snprintf(buf + size, sizeof(buf) - size, "VEH [%5d] %s\r\n", VEH_VNUM(veh_iter), VEH_SHORT_DESC(veh_iter));
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
* Periodic action for character dismantling a vehicle.
*
* @param char_data *ch The person doing the dismantling.
*/
void process_dismantle_vehicle(char_data *ch) {
	struct resource_data *res, *find_res, *next_res, *copy;
	char buf[MAX_STRING_LENGTH];
	vehicle_data *veh;
	
	if (!(veh = find_dismantling_vehicle_in_room(IN_ROOM(ch), GET_ACTION_VNUM(ch, 1)))) {
		cancel_action(ch);	// no vehicle
		return;
	}
	else if (!can_use_vehicle(ch, veh, MEMBERS_ONLY)) {
		msg_to_char(ch, "You can't dismantle a %s you don't own.\r\n", VEH_OR_BLD(veh));
		cancel_action(ch);
		return;
	}
	else if (!CAN_SEE_VEHICLE(ch, veh)) {
		msg_to_char(ch, "You can't dismantle a %s you can't see.\r\n", VEH_OR_BLD(veh));
		cancel_action(ch);
		return;
	}
	
	// sometimes zeroes end up in here ... just clear them
	res = NULL;
	LL_FOREACH_SAFE(VEH_NEEDS_RESOURCES(veh), find_res, next_res) {
		// things we can't refund
		if (find_res->amount <= 0 || find_res->type == RES_COMPONENT) {
			LL_DELETE(VEH_NEEDS_RESOURCES(veh), find_res);
			free(find_res);
			continue;
		}
		
		// we actually only want the last valid thing, so save res now (and overwrite it on the next loop)
		res = find_res;
	}
	
	// did we find something to refund?
	if (res) {
		// RES_x: messaging
		switch (res->type) {
			// RES_COMPONENT (stored as obj), RES_ACTION, RES_TOOL (stored as obj), and RES_CURRENCY aren't possible here
			case RES_OBJECT: {
				snprintf(buf, sizeof(buf), "You carefully remove %s from $V.", get_obj_name_by_proto(res->vnum));
				act(buf, FALSE, ch, NULL, veh, TO_CHAR | TO_SPAMMY | ACT_VEH_VICT);
				snprintf(buf, sizeof(buf), "$n removes %s from $V.", get_obj_name_by_proto(res->vnum));
				act(buf, FALSE, ch, NULL, veh, TO_ROOM | TO_SPAMMY | ACT_VEH_VICT);
				break;
			}
			case RES_LIQUID: {
				snprintf(buf, sizeof(buf), "You carefully retrieve %d unit%s of %s from $V.", res->amount, PLURAL(res->amount), get_generic_string_by_vnum(res->vnum, GENERIC_LIQUID, GSTR_LIQUID_NAME));
				act(buf, FALSE, ch, NULL, veh, TO_CHAR | TO_SPAMMY | ACT_VEH_VICT);
				snprintf(buf, sizeof(buf), "$n retrieves some %s from $V.", get_generic_string_by_vnum(res->vnum, GENERIC_LIQUID, GSTR_LIQUID_NAME));
				act(buf, FALSE, ch, NULL, veh, TO_ROOM | TO_SPAMMY | ACT_VEH_VICT);
				break;
			}
			case RES_COINS: {
				snprintf(buf, sizeof(buf), "You retrieve %s from $V.", money_amount(real_empire(res->vnum), res->amount));
				act(buf, FALSE, ch, NULL, veh, TO_CHAR | TO_SPAMMY | ACT_VEH_VICT);
				snprintf(buf, sizeof(buf), "$n retrieves %s from $V.", res->amount == 1 ? "a coin" : "some coins");
				act(buf, FALSE, ch, NULL, veh, TO_ROOM | TO_SPAMMY | ACT_VEH_VICT);
				break;
			}
			case RES_POOL: {
				snprintf(buf, sizeof(buf), "You regain %d %s point%s from $V.", res->amount, pool_types[res->vnum], PLURAL(res->amount));
				act(buf, FALSE, ch, NULL, veh, TO_CHAR | TO_SPAMMY | ACT_VEH_VICT);
				snprintf(buf, sizeof(buf), "$n retrieves %s from $V.", pool_types[res->vnum]);
				act(buf, FALSE, ch, NULL, veh, TO_ROOM | TO_SPAMMY | ACT_VEH_VICT);
				break;
			}
			case RES_CURRENCY: {
				snprintf(buf, sizeof(buf), "You retrieve %d %s from $V.", res->amount, get_generic_string_by_vnum(res->vnum, GENERIC_CURRENCY, WHICH_CURRENCY(res->amount)));
				act(buf, FALSE, ch, NULL, veh, TO_CHAR | TO_SPAMMY | ACT_VEH_VICT);
				snprintf(buf, sizeof(buf), "$n retrieves %d %s from $V.", res->amount, get_generic_string_by_vnum(res->vnum, GENERIC_CURRENCY, WHICH_CURRENCY(res->amount)));
				act(buf, FALSE, ch, NULL, veh, TO_ROOM | TO_SPAMMY | ACT_VEH_VICT);
				break;
			}
		}
		
		// make a copy to pass to give_resources
		CREATE(copy, struct resource_data, 1);
		*copy = *res;
		copy->next = NULL;	// will be freed as a list
		
		if (copy->type == RES_OBJECT) {
			// for items, refund 1 at a time
			copy->amount = MIN(1, copy->amount);
			res->amount -= 1;
		}
		else {
			// all other types refund the whole thing
			res->amount = 0;
		}
		
		// give the thing(s)
		give_resources(ch, copy, FALSE);
		free(copy);
		
		if (res->amount <= 0) {
			LL_DELETE(VEH_NEEDS_RESOURCES(veh), res);
			free(res);
		}
	}
	
	// done?
	if (!VEH_NEEDS_RESOURCES(veh)) {
		finish_dismantle_vehicle(ch, veh);
	}
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
	vehicle_data *veh, *proto;
	
	if (!(proto = vehicle_proto(vnum))) {
		log("Vehicle vnum %d does not exist in database.", vnum);
		// grab first one (bug)
		proto = vehicle_table;
	}

	CREATE(veh, vehicle_data, 1);
	clear_vehicle(veh);
	
	// fix memory leak because attributes was allocated by clear_vehicle then overwritten by the next line
	if (veh->attributes) {
		free(veh->attributes);
	}

	*veh = *proto;
	DL_PREPEND(vehicle_list, veh);
	
	// new vehicle setup
	VEH_OWNER(veh) = NULL;
	VEH_SCALE_LEVEL(veh) = 0;	// unscaled
	VEH_HEALTH(veh) = VEH_MAX_HEALTH(veh);
	VEH_CONTAINS(veh) = NULL;
	VEH_CARRYING_N(veh) = 0;
	VEH_ANIMALS(veh) = NULL;
	VEH_NEEDS_RESOURCES(veh) = NULL;
	VEH_BUILT_WITH(veh) = NULL;
	IN_ROOM(veh) = NULL;
	remove_vehicle_flags(veh, VEH_INCOMPLETE | VEH_DISMANTLING);	// ensure not marked incomplete/dismantle
	
	// give it a new idnum
	VEH_IDNUM(veh) = data_get_int(DATA_TOP_VEHICLE_ID) + 1;
	data_set_int(DATA_TOP_VEHICLE_ID, VEH_IDNUM(veh));
	
	// this will be initialized only if needed
	veh->script_id = 0;
	
	if (with_triggers) {
		veh->proto_script = copy_trig_protos(proto->proto_script);
		assign_triggers(veh, VEH_TRIGGER);
	}
	else {
		veh->proto_script = NULL;
	}
	
	return veh;
}


/**
* Removes a room from a vehicle's tracking list, if it's present.
*
* @param room_data *room The room.
* @param vehicle_data *veh The vehicle to remove it from.
*/
void remove_room_from_vehicle(room_data *room, vehicle_data *veh) {
	struct vehicle_room_list *vrl, *next_vrl;
	
	LL_FOREACH_SAFE(VEH_ROOM_LIST(veh), vrl, next_vrl) {
		if (vrl->room == room) {
			LL_DELETE(VEH_ROOM_LIST(veh), vrl);
			free(vrl);
		}
	}
}


// Simple vnum sorter for the vehicle hash
int sort_vehicles(vehicle_data *a, vehicle_data *b) {
	return VEH_VNUM(a) - VEH_VNUM(b);
}


/**
* store_one_vehicle_to_file: Write a vehicle to a tagged save file. Vehicle
* tags start with %VNUM instead of #VNUM because they may co-exist with items
* in the file.
*
* @param vehicle_data *veh The vehicle to save.
* @param FILE *fl The file to save to (open for writing).
*/
void store_one_vehicle_to_file(vehicle_data *veh, FILE *fl) {
	struct room_extra_data *red, *next_red;
	struct vehicle_attached_mob *vam;
	char temp[MAX_STRING_LENGTH];
	struct depletion_data *dep;
	struct resource_data *res;
	struct trig_var_data *tvd;
	vehicle_data *proto;
	
	if (!fl || !veh) {
		log("SYSERR: store_one_vehicle_to_file called without %s", fl ? "vehicle" : "file");
		return;
	}
	
	proto = vehicle_proto(VEH_VNUM(veh));
	
	fprintf(fl, "%%%d\n", VEH_VNUM(veh));
	fprintf(fl, "Id: %d\n", VEH_IDNUM(veh));
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
	if (VEH_ICON(veh) && (!proto || VEH_ICON(veh) != VEH_ICON(proto))) {
		fprintf(fl, "Icon:\n%s~\n", NULLSAFE(VEH_ICON(veh)));
	}
	if (VEH_HALF_ICON(veh) && (!proto || VEH_HALF_ICON(veh) != VEH_HALF_ICON(proto))) {
		fprintf(fl, "Icon-2:\n%s~\n", NULLSAFE(VEH_HALF_ICON(veh)));
	}
	if (VEH_QUARTER_ICON(veh) && (!proto || VEH_QUARTER_ICON(veh) != VEH_QUARTER_ICON(proto))) {
		fprintf(fl, "Icon-1:\n%s~\n", NULLSAFE(VEH_QUARTER_ICON(veh)));
	}

	if (VEH_OWNER(veh)) {
		fprintf(fl, "Owner: %d\n", EMPIRE_VNUM(VEH_OWNER(veh)));
	}
	if (VEH_SCALE_LEVEL(veh)) {
		fprintf(fl, "Scale: %d\n", VEH_SCALE_LEVEL(veh));
	}
	if (VEH_HEALTH(veh) < VEH_MAX_HEALTH(veh)) {
		fprintf(fl, "Health: %.2f\n", VEH_HEALTH(veh));
	}
	if (VEH_INSTANCE_ID(veh) != NOTHING) {
		fprintf(fl, "Instance-id: %d\n", VEH_INSTANCE_ID(veh));
	}
	if (VEH_INTERIOR_HOME_ROOM(veh)) {
		fprintf(fl, "Interior-home: %d\n", GET_ROOM_VNUM(VEH_INTERIOR_HOME_ROOM(veh)));
	}
	if (VEH_CONSTRUCTION_ID(veh) != NOTHING) {
		fprintf(fl, "Construction-id: %d\n", VEH_CONSTRUCTION_ID(veh));
	}
	if (VEH_CONTAINS(veh)) {
		fprintf(fl, "Contents:\n");
		Crash_save(VEH_CONTAINS(veh), fl, LOC_INVENTORY);
		fprintf(fl, "Contents-end\n");
	}
	if (VEH_LAST_FIRE_TIME(veh)) {
		fprintf(fl, "Last-fired: %ld\n", VEH_LAST_FIRE_TIME(veh));
	}
	if (VEH_LAST_MOVE_TIME(veh)) {
		fprintf(fl, "Last-moved: %ld\n", VEH_LAST_MOVE_TIME(veh));
	}
	LL_FOREACH(VEH_ANIMALS(veh), vam) {
		fprintf(fl, "Animal: %d %d %s %d\n", vam->mob, vam->scale_level, bitv_to_alpha(vam->flags), vam->empire);
	}
	LL_FOREACH(VEH_NEEDS_RESOURCES(veh), res) {
		// arg order is backwards-compatible to pre-2.0b3.17
		fprintf(fl, "Needs-res: %d %d %d %d\n", res->vnum, res->amount, res->type, res->misc);
	}
	if (!VEH_FLAGGED(veh, VEH_NEVER_DISMANTLE)) {
		// only save built-with if it's dismantlable
		LL_FOREACH(VEH_BUILT_WITH(veh), res) {
			fprintf(fl, "Built-with: %d %d %d %d\n", res->vnum, res->amount, res->type, res->misc);
		}
	}
	LL_FOREACH(VEH_DEPLETION(veh), dep) {
		fprintf(fl, "Depletion: %d %d\n", dep->type, dep->count);
	}
	HASH_ITER(hh, VEH_EXTRA_DATA(veh), red, next_red) {
		fprintf(fl, "Extra-data: %d %d\n", red->type, red->value);
	}
	if (VEH_ROOM_AFFECTS(veh) && (!proto || VEH_ROOM_AFFECTS(veh) != VEH_ROOM_AFFECTS(proto))) {
		fprintf(fl, "Room-affects: %lld\r\n", VEH_ROOM_AFFECTS(veh));
	}
	
	// scripts
	if (SCRIPT(veh)) {
		trig_data *trig;
		
		for (trig = TRIGGERS(SCRIPT(veh)); trig; trig = trig->next) {
			fprintf(fl, "Trigger: %d\n", GET_TRIG_VNUM(trig));
		}
		
		LL_FOREACH (SCRIPT(veh)->global_vars, tvd) {
			if (*tvd->name == '-' || !*tvd->value) { // don't save if it begins with - or is empty
				continue;
			}
			
			fprintf(fl, "Variable: %s %ld\n%s\n", tvd->name, tvd->context, tvd->value);
		}
	}
	
	fprintf(fl, "Vehicle-end\n");
}


/**
* Removes a vehicle's traits from an island.
*
* @param vehicle_data *veh The vehicle.
*/
void unapply_vehicle_to_island(vehicle_data *veh) {
	struct vehicle_room_list *vrl;
	
	if (veh) {
		if (VEH_APPLIED_TO_ISLAND(veh) != UNAPPLIED_ISLAND) {
			// NOTE: do not remove the island-id on the interior rooms -- do this only when applying to a new room
			
			// un-apply tech -- do it before removing from the island
			adjust_vehicle_tech(veh, VEH_APPLIED_TO_ISLAND(veh), FALSE);
			
			// un-apply interior techs too
			if (VEH_OWNER(veh) && VEH_ROOM_LIST(veh)) {
				LL_FOREACH(VEH_ROOM_LIST(veh), vrl) {
					adjust_building_tech(VEH_OWNER(veh), vrl->room, FALSE);
				}
			}
			
			// and clear the data
			VEH_APPLIED_TO_ISLAND(veh) = UNAPPLIED_ISLAND;
		}
	}
}


/**
* Removes a vehicle's traits from a room.
*
* @param vehicle_data *veh The vehicle.
*/
void unapply_vehicle_to_room(vehicle_data *veh) {
	room_data *was_room;
	
	// do not unapply_vehicle_to_island: it doesn't unapply until it gets a new one
	
	if (veh && (was_room = VEH_APPLIED_TO_ROOM(veh))) {
		// NOTE: do not remove the map-loc on the interior rooms -- do this only when applying to a new room
		
		// and clear the data
		VEH_APPLIED_TO_ROOM(veh) = NULL;
		
		// check lights
		if (VEH_PROVIDES_LIGHT(veh)) {
			--ROOM_LIGHTS(was_room);
		}
	}
}


/**
* Reads a vehicle from a tagged data file.
*
* @param FILE *fl The file open for reading, just after the %VNUM line.
* @param any_vnum vnum The vnum already read from the file.
* @param char *error_str Used in logging errors, to indicate where they come from.
* @return vehicle_data* The loaded vehicle, if possible.
*/
vehicle_data *unstore_vehicle_from_file(FILE *fl, any_vnum vnum, char *error_str) {
	char line[MAX_INPUT_LENGTH], error[MAX_STRING_LENGTH], s_in[MAX_INPUT_LENGTH];
	obj_data *load_obj, *obj, *next_obj, *cont_row[MAX_BAG_ROWS];
	struct vehicle_attached_mob *vam;
	int iter, i_in[4], location = 0, timer;
	struct resource_data *res;
	vehicle_data *proto = vehicle_proto(vnum);
	bool end = FALSE, seek_end = FALSE;
	any_vnum load_vnum;
	vehicle_data *veh;
	long long_in[2];
	double dbl_in;
	struct shipping_data *shipd;
	
	#define LOG_BAD_TAG_WARNINGS  TRUE	// triggers syslogs for invalid vehicle tags
	#define BAD_TAG_WARNING(src)  else if (LOG_BAD_TAG_WARNINGS) { \
		log("SYSERR: Bad tag in vehicle %d for %s: %s", vnum, NULLSAFE(error_str), (src));	\
	}
	
	// load based on vnum or, if NOTHING, this will fail
	if (proto) {
		veh = read_vehicle(vnum, FALSE);
	}
	else {
		veh = NULL;
		seek_end = TRUE;	// signal it to skip the whole vehicle
	}
		
	// for fread_string
	sprintf(error, "unstore_vehicle_from_file %d", vnum);

	while (!end) {
		if (!get_line(fl, line)) {
			log("SYSERR: Unexpected end of pack file in unstore_vehicle_from_file for %s", NULLSAFE(error_str));
			exit(1);
		}
		
		if (!strn_cmp(line, "Vehicle-end", 11)) {
			// this MUST be before seek_end
			end = TRUE;
			continue;
		}
		if (seek_end) {
			// are we looking for the end of the vehicle? ignore this line
			// WARNING: don't put any ifs that require "veh" above seek_end; obj is not guaranteed
			continue;
		}
		
		// normal tags by letter
		switch (UPPER(*line)) {
			case 'A': {
				if (!strn_cmp(line, "Animal: ", 8)) {
					if (sscanf(line + 8, "%d %d %s %d", &i_in[0], &i_in[1], s_in, &i_in[2]) == 4) {
						CREATE(vam, struct vehicle_attached_mob, 1);
						vam->mob = i_in[0];
						vam->scale_level = i_in[1];
						vam->flags = asciiflag_conv(s_in);
						vam->empire = i_in[2];
						LL_APPEND(VEH_ANIMALS(veh), vam);
					}
				}
				BAD_TAG_WARNING(line)
				break;
			}
			case 'B': {
				if (!strn_cmp(line, "Built-with: ", 12)) {
					if (sscanf(line + 12, "%d %d %d %d", &i_in[0], &i_in[1], &i_in[2], &i_in[3]) != 4) {
						// unknown number of args?
						break;
					}
					
					CREATE(res, struct resource_data, 1);
					res->vnum = i_in[0];
					res->amount = i_in[1];
					res->type = i_in[2];
					res->misc = i_in[3];
					LL_APPEND(VEH_BUILT_WITH(veh), res);
				}
				BAD_TAG_WARNING(line)
				break;
			}
			case 'C': {
				if (!strn_cmp(line, "Construction-id: ", 17)) {
					if (sscanf(line + 17, "%d", &i_in[0])) {
						VEH_CONSTRUCTION_ID(veh) = i_in[0];
					}
				}
				else if (!strn_cmp(line, "Contents:", 9)) {
					// empty container lists
					for (iter = 0; iter < MAX_BAG_ROWS; iter++) {
						cont_row[iter] = NULL;
					}

					// load contents until we find an end
					for (;;) {
						if (!get_line(fl, line)) {
							log("SYSERR: Format error in pack file with vehicle %d", vnum);
							extract_vehicle(veh);
							return NULL;
						}
						
						if (*line == '#') {
							if (sscanf(line, "#%d", &load_vnum) < 1) {
								log("SYSERR: Format error in vnum line of pack file with vehicle %d", vnum);
								extract_vehicle(veh);
								return NULL;
							}
							if ((load_obj = Obj_load_from_file(fl, load_vnum, &location, NULL, error))) {
								// Obj_load_from_file may return a NULL for deleted objs
								suspend_autostore_updates = TRUE;	// see end of block for FALSE
				
								// Not really an inventory, but same idea.
								if (location > 0) {
									location = LOC_INVENTORY;
								}

								// store autostore timer through obj_to_room
								timer = GET_AUTOSTORE_TIMER(load_obj);
								obj_to_vehicle(load_obj, veh);
								schedule_obj_autostore_check(load_obj, timer);
				
								for (iter = MAX_BAG_ROWS - 1; iter > -location; --iter) {
									// No container, back to vehicle
									DL_FOREACH_SAFE2(cont_row[iter], obj, next_obj, next_content) {
										DL_DELETE2(cont_row[iter], obj, prev_content, next_content);
										timer = GET_AUTOSTORE_TIMER(obj);
										obj_to_vehicle(obj, veh);
										schedule_obj_autostore_check(obj, timer);
									}
								}
								if (iter == -location && cont_row[iter]) {			/* Content list exists. */
									if (GET_OBJ_TYPE(load_obj) == ITEM_CONTAINER || IS_CORPSE(load_obj)) {
										/* Take the item, fill it, and give it back. */
										obj_from_room(load_obj);
										load_obj->contains = NULL;
										DL_FOREACH_SAFE2(cont_row[iter], obj, next_obj, next_content) {
											DL_DELETE2(cont_row[iter], obj, prev_content, next_content);
											obj_to_obj(obj, load_obj);
										}
										timer = GET_AUTOSTORE_TIMER(load_obj);
										obj_to_vehicle(load_obj, veh);			/* Add to vehicle first. */
										schedule_obj_autostore_check(load_obj, timer);
									}
									else {				/* Object isn't container, empty content list. */
										DL_FOREACH_SAFE2(cont_row[iter], obj, next_obj, next_content) {
											DL_DELETE2(cont_row[iter], obj, prev_content, next_content);
											timer = GET_AUTOSTORE_TIMER(obj);
											obj_to_vehicle(obj, veh);
											schedule_obj_autostore_check(obj, timer);
										}
									}
								}
								if (location < 0 && location >= -MAX_BAG_ROWS) {
									obj_from_room(load_obj);
									DL_APPEND2(cont_row[-location - 1], load_obj, prev_content, next_content);
								}
								
								suspend_autostore_updates = FALSE;
							}
						}
						else if (!strn_cmp(line, "Contents-end", 12)) {
							// done
							break;
						}
						else {
							log("SYSERR: Format error in pack file for vehicle %d: %s", vnum, line);
							extract_vehicle(veh);
							return NULL;
						}
					}
				}
				BAD_TAG_WARNING(line)
				break;
			}
			case 'D': {
				if (!strn_cmp(line, "Depletion: ", 11)) {
					if (sscanf(line + 11, "%d %d", &i_in[0], &i_in[1]) == 2) {
						struct depletion_data *dep;
						CREATE(dep, struct depletion_data, 1);
						dep->type = i_in[0];
						dep->count = i_in[1];
						LL_PREPEND(VEH_DEPLETION(veh), dep);
					}
				}
				BAD_TAG_WARNING(line)
				break;
			}
			case 'E': {
				if (!strn_cmp(line, "Extra-data: ", 12)) {
					if (sscanf(line + 12, "%d %d", &i_in[0], &i_in[1]) == 2) {
						set_extra_data(&VEH_EXTRA_DATA(veh), i_in[0], i_in[1]);
					}
				}
				BAD_TAG_WARNING(line)
				break;
			}
			case 'F': {
				if (!strn_cmp(line, "Flags: ", 7)) {
					if (sscanf(line + 7, "%s", s_in)) {
						if (proto) {	// prefer to keep flags from the proto
							VEH_FLAGS(veh) = VEH_FLAGS(proto) & ~SAVABLE_VEH_FLAGS;
							VEH_FLAGS(veh) |= asciiflag_conv(s_in) & SAVABLE_VEH_FLAGS;
						}
						else {	// no proto
							VEH_FLAGS(veh) = asciiflag_conv(s_in);
						}
					}
				}
				BAD_TAG_WARNING(line)
				break;
			}
			case 'H': {
				if (!strn_cmp(line, "Health: ", 8)) {
					if (sscanf(line + 8, "%lf", &dbl_in)) {
						VEH_HEALTH(veh) = MIN(dbl_in, VEH_MAX_HEALTH(veh));
					}
				}
				BAD_TAG_WARNING(line)
				break;
			}
			case 'I': {
				if (!strn_cmp(line, "Icon:", 5)) {
					if (VEH_ICON(veh) && (!proto || VEH_ICON(veh) != VEH_ICON(proto))) {
						free(VEH_ICON(veh));
					}
					VEH_ICON(veh) = fread_string(fl, error);
				}
				else if (!strn_cmp(line, "Icon-1:", 7)) {	// quarter icon
					if (VEH_QUARTER_ICON(veh) && (!proto || VEH_QUARTER_ICON(veh) != VEH_QUARTER_ICON(proto))) {
						free(VEH_QUARTER_ICON(veh));
					}
					VEH_QUARTER_ICON(veh) = fread_string(fl, error);
				}
				else if (!strn_cmp(line, "Icon-2:", 7)) {	// half icon
					if (VEH_HALF_ICON(veh) && (!proto || VEH_HALF_ICON(veh) != VEH_HALF_ICON(proto))) {
						free(VEH_HALF_ICON(veh));
					}
					VEH_HALF_ICON(veh) = fread_string(fl, error);
				}
				else if (!strn_cmp(line, "Id: ", 4)) {
					if (sscanf(line + 4, "%d", &i_in[0])) {
						// did we already have an idnum? (usually due to being assigned a new one)
						if (VEH_IDNUM(veh) != 0) {
							// check if we used a new one
							if (VEH_IDNUM(veh) == data_get_int(DATA_TOP_VEHICLE_ID)) {
								// give back the top id
								data_set_int(DATA_TOP_VEHICLE_ID, data_get_int(DATA_TOP_VEHICLE_ID) - 1);
							}
						}
					
						VEH_IDNUM(veh) = i_in[0];
						
						// ensure it's not over the top known id
						if (i_in[0] > data_get_int(DATA_TOP_VEHICLE_ID)) {
							data_set_int(DATA_TOP_VEHICLE_ID, i_in[0]);
						}
					}
				}
				else if (!strn_cmp(line, "Instance-id: ", 13)) {
					if (sscanf(line + 13, "%d", &i_in[0])) {
						VEH_INSTANCE_ID(veh) = i_in[0];
					}
				}
				else if (!strn_cmp(line, "Interior-home: ", 15)) {
					if (sscanf(line + 15, "%d", &i_in[0])) {
						VEH_INTERIOR_HOME_ROOM(veh) = real_room(i_in[0]);
					}
				}
				BAD_TAG_WARNING(line)
				break;
			}
			case 'K': {
				if (!strn_cmp(line, "Keywords:", 9)) {
					if (VEH_KEYWORDS(veh) && (!proto || VEH_KEYWORDS(veh) != VEH_KEYWORDS(proto))) {
						free(VEH_KEYWORDS(veh));
					}
					VEH_KEYWORDS(veh) = fread_string(fl, error);
				}
				BAD_TAG_WARNING(line)
				break;
			}
			case 'L': {
				if (!strn_cmp(line, "Last-fired: ", 12)) {
					if (sscanf(line + 12, "%ld", &long_in[0])) {
						VEH_LAST_FIRE_TIME(veh) = long_in[0];
					}
				}
				else if (!strn_cmp(line, "Last-moved: ", 12)) {
					if (sscanf(line + 12, "%ld", &long_in[0])) {
						VEH_LAST_MOVE_TIME(veh) = long_in[0];
					}
				}
				else if (!strn_cmp(line, "Long-desc:", 10)) {
					if (VEH_LONG_DESC(veh) && (!proto || VEH_LONG_DESC(veh) != VEH_LONG_DESC(proto))) {
						free(VEH_LONG_DESC(veh));
					}
					VEH_LONG_DESC(veh) = fread_string(fl, error);
				}
				else if (!strn_cmp(line, "Look-desc:", 10)) {
					if (VEH_LOOK_DESC(veh) && (!proto || VEH_LOOK_DESC(veh) != VEH_LOOK_DESC(proto))) {
						free(VEH_LOOK_DESC(veh));
					}
					VEH_LOOK_DESC(veh) = fread_string(fl, error);
				}
				BAD_TAG_WARNING(line)
				break;
			}
			case 'N': {
				if (!strn_cmp(line, "Needs-res: ", 11)) {
					if (sscanf(line + 11, "%d %d %d %d", &i_in[0], &i_in[1], &i_in[2], &i_in[3]) == 4) {
						// all args present
					}
					else if (sscanf(line + 11, "%d %d", &i_in[0], &i_in[1]) == 2) {
						// backwards-compatible to pre-2.0b3.17
						i_in[2] = RES_OBJECT;
						i_in[3] = 0;
					}
					else {
						// unknown number of args?
						break;
					}
					
					CREATE(res, struct resource_data, 1);
					res->vnum = i_in[0];
					res->amount = i_in[1];
					res->type = i_in[2];
					res->misc = i_in[3];
					LL_APPEND(VEH_NEEDS_RESOURCES(veh), res);
				}
				BAD_TAG_WARNING(line)
				break;
			}
			case 'O': {
				if (!strn_cmp(line, "Owner: ", 7)) {
					if (sscanf(line + 7, "%d", &i_in[0])) {
						// VEH_OWNER(veh) = real_empire(i_in[0]);
						perform_claim_vehicle(veh, real_empire(i_in[0]));
					}
				}
				BAD_TAG_WARNING(line)
				break;
			}
			case 'R': {
				if (!strn_cmp(line, "Room-affects: ", 14)) {
					VEH_ROOM_AFFECTS(veh) = strtoull(line + 14, NULL, 10);
				}
				BAD_TAG_WARNING(line)
				break;
			}
			case 'S': {
				if (!strn_cmp(line, "Scale: ", 7)) {
					if (sscanf(line + 7, "%d", &i_in[0])) {
						VEH_SCALE_LEVEL(veh) = i_in[0];
					}
				}
				else if (!strn_cmp(line, "Shipping-id: ", 13)) {
					if (sscanf(line + 13, "%d", &i_in[0])) {
						// this was formerly is temporary id assigned to the vehicle
						// convert it to use the ships idnum now (safe because both idnum and owner are saved before shipping id in the file).
						if (VEH_OWNER(veh)) {
							DL_FOREACH(EMPIRE_SHIPPING_LIST(VEH_OWNER(veh)), shipd) {
								if (shipd->shipping_id == i_in[0]) {
									shipd->shipping_id = VEH_IDNUM(veh);
									EMPIRE_NEEDS_STORAGE_SAVE(VEH_OWNER(veh)) = TRUE;
								}
							}
						}
						
						// no longer need this:
						// VEH_SHIPPING_ID(veh) = i_in[0];
					}
				}
				else if (!strn_cmp(line, "Short-desc:", 11)) {
					if (VEH_SHORT_DESC(veh) && (!proto || VEH_SHORT_DESC(veh) != VEH_SHORT_DESC(proto))) {
						free(VEH_SHORT_DESC(veh));
					}
					VEH_SHORT_DESC(veh) = fread_string(fl, error);
				}
				BAD_TAG_WARNING(line)
				break;
			}
			case 'T': {
				if (!strn_cmp(line, "Trigger: ", 9)) {
					if (sscanf(line + 9, "%d", &i_in[0]) && real_trigger(i_in[0])) {
						if (!SCRIPT(veh)) {
							create_script_data(veh, VEH_TRIGGER);
						}
						add_trigger(SCRIPT(veh), read_trigger(i_in[0]), -1);
					}
				}
				BAD_TAG_WARNING(line)
				break;
			}
			case 'V': {
				if (!strn_cmp(line, "Variable: ", 10)) {
					if (sscanf(line + 10, "%s %d", s_in, &i_in[0]) != 2 || !get_line(fl, line)) {
						log("SYSERR: Bad variable format in unstore_vehicle_from_file for %s: #%d", NULLSAFE(error_str), VEH_VNUM(veh));
						exit(1);
					}
					if (!SCRIPT(veh)) {
						create_script_data(veh, VEH_TRIGGER);
					}
					add_var(&(SCRIPT(veh)->global_vars), s_in, line, i_in[0]);
				}
				BAD_TAG_WARNING(line)
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
	VEH_IDNUM(veh) = 0;
	
	veh->attributes = attr;	// stored from earlier
	if (veh->attributes) {
		memset((char *)(veh->attributes), 0, sizeof(struct vehicle_attribute_data));
	}
	else {
		CREATE(veh->attributes, struct vehicle_attribute_data, 1);
	}
	
	// attributes init
	VEH_INTERIOR_ROOM_VNUM(veh) = NOTHING;
	VEH_CONSTRUCTION_ID(veh) = NOTHING;
	VEH_INSTANCE_ID(veh) = NOTHING;
	VEH_SPEED_BONUSES(veh) = VSPEED_NORMAL;
	VEH_APPLIED_TO_ISLAND(veh) = UNAPPLIED_ISLAND;
	VEH_ARTISAN(veh) = NOTHING;
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
	struct depletion_data *dep;
	struct spawn_info *spawn;
	
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
	if (VEH_HALF_ICON(veh) && (!proto || VEH_HALF_ICON(veh) != VEH_HALF_ICON(proto))) {
		free(VEH_HALF_ICON(veh));
	}
	if (VEH_QUARTER_ICON(veh) && (!proto || VEH_QUARTER_ICON(veh) != VEH_QUARTER_ICON(proto))) {
		free(VEH_QUARTER_ICON(veh));
	}
	
	// pointers
	if (VEH_NEEDS_RESOURCES(veh)) {
		free_resource_list(VEH_NEEDS_RESOURCES(veh));
	}
	if (VEH_BUILT_WITH(veh)) {
		free_resource_list(VEH_BUILT_WITH(veh));
	}
	while ((vam = VEH_ANIMALS(veh))) {
		VEH_ANIMALS(veh) = vam->next;
		free(vam);
	}
	while ((dep = VEH_DEPLETION(veh))) {
		VEH_DEPLETION(veh) = dep->next;
		free(dep);
	}
	free_extra_data(&VEH_EXTRA_DATA(veh));
	empty_vehicle(veh, NULL);
	
	// free any assigned scripts and vars
	if (SCRIPT(veh)) {
		extract_script(veh, VEH_TRIGGER);
	}
	if (veh->proto_script && (!proto || veh->proto_script != proto->proto_script)) {
		free_proto_scripts(&veh->proto_script);
	}
	
	// attributes
	if (veh->attributes && (!proto || veh->attributes != proto->attributes)) {
		if (VEH_REGULAR_MAINTENANCE(veh)) {
			free_resource_list(VEH_REGULAR_MAINTENANCE(veh));
		}
		if (VEH_EX_DESCS(veh)) {
			free_extra_descs(&VEH_EX_DESCS(veh));
		}
		if (VEH_INTERACTIONS(veh) && (!proto || VEH_INTERACTIONS(veh) != VEH_INTERACTIONS(proto))) {
			free_interactions(&VEH_INTERACTIONS(veh));
		}
		if (VEH_SPAWNS(veh)) {
			while ((spawn = VEH_SPAWNS(veh))) {
				VEH_SPAWNS(veh) = spawn->next;
				free(spawn);
			}
		}
		if (VEH_CUSTOM_MSGS(veh) && (!proto || VEH_CUSTOM_MSGS(veh) != VEH_CUSTOM_MSGS(proto))) {
			free_custom_messages(VEH_CUSTOM_MSGS(veh));
		}
		if (VEH_RELATIONS(veh) && (!proto || VEH_RELATIONS(veh) != VEH_RELATIONS(proto))) {
			free_bld_relations(VEH_RELATIONS(veh));
		}
		
		free(veh->attributes);
	}
	
	if (veh->script_id > 0) {
		remove_from_lookup_table(veh->script_id);
	}
	
	free(veh);
}


/**
* Increments the last construction id by 1 and returns it, for use on a vehicle
* that's being constructed or dismantled. If it hits max-int, it wraps back to
* zero. The chances of a conflict doing this are VERY low since these are
* usually short-lived ids and it would take billions of vehicles to create a
* collision.
*
* @return int A new construction id.
*/
int get_new_vehicle_construction_id(void) {
	int id = data_get_int(DATA_LAST_CONSTRUCTION_ID);
	
	if (id < INT_MAX) {
		++id;
	}
	else {
		id = 0;
	}
	data_set_int(DATA_LAST_CONSTRUCTION_ID, id);
	return id;
}


/**
* Read one vehicle from file.
*
* @param FILE *fl The open .veh file
* @param any_vnum vnum The vehicle vnum
*/
void parse_vehicle(FILE *fl, any_vnum vnum) {
	char line[256], error[256], str_in[256], str_in2[256], str_in3[256];
	struct bld_relation *relat;
	struct spawn_info *spawn;
	vehicle_data *veh, *find;
	double dbl_in;
	int int_in[10];
	
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
	
	// line 6: flags move_type maxhealth capacity animals_required functions fame military size
	if (!get_line(fl, line)) {
		log("SYSERR: Missing line 6 of %s", error);
		exit(1);
	}
	if (sscanf(line, "%s %d %d %d %d %s %d %d %d %s %d %d", str_in, &int_in[0], &int_in[1], &int_in[2], &int_in[3], str_in2, &int_in[4], &int_in[5], &int_in[6], str_in3, &int_in[7], &int_in[8]) != 12) {
		int_in[7] = NOTHING;	// b5.175 backwards-compatible: artisan vnum
		int_in[8] = 0;					// and citizens
		
		if (sscanf(line, "%s %d %d %d %d %s %d %d %d %s", str_in, &int_in[0], &int_in[1], &int_in[2], &int_in[3], str_in2, &int_in[4], &int_in[5], &int_in[6], str_in3) != 10) {
			int_in[6] = 0;	// b5.104 backwards-compatible: size
			strcpy(str_in3, "0");	// affects
		
			if (sscanf(line, "%s %d %d %d %d %s %d %d", str_in, &int_in[0], &int_in[1], &int_in[2], &int_in[3], str_in2, &int_in[4], &int_in[5]) != 8) {
				strcpy(str_in2, "0");	// backwards-compatible: functions
				int_in[4] = 0;	// fame
				int_in[5] = 0;	// military
		
				if (sscanf(line, "%s %d %d %d %d", str_in, &int_in[0], &int_in[1], &int_in[2], &int_in[3]) != 5) {
					log("SYSERR: Format error in line 6 of %s", error);
					exit(1);
				}
			}
		}
	}
	
	VEH_FLAGS(veh) = asciiflag_conv(str_in);
	VEH_MOVE_TYPE(veh) = int_in[0];
	VEH_HEALTH(veh) = VEH_MAX_HEALTH(veh) = int_in[1];
	VEH_CAPACITY(veh) = int_in[2];
	VEH_ANIMALS_REQUIRED(veh) = int_in[3];
	VEH_FUNCTIONS(veh) = asciiflag_conv(str_in2);
	VEH_FAME(veh) = int_in[4];
	VEH_MILITARY(veh) = int_in[5];
	VEH_SIZE(veh) = int_in[6];
	VEH_ROOM_AFFECTS(veh) = asciiflag_conv(str_in3);
	VEH_ARTISAN(veh) = int_in[7];
	VEH_CITIZENS(veh) = int_in[8];
	
	// optionals
	for (;;) {
		if (!get_line(fl, line)) {
			log("SYSERR: Format error in %s, expecting alphabetic flags", error);
			exit(1);
		}
		switch (*line) {
			case 'B': {	// extra strings (or extra data
				switch (*(line+1)) {
					case '0': {	// B0: half icon
						if (VEH_HALF_ICON(veh)) {
							free(VEH_HALF_ICON(veh));
						}
						VEH_HALF_ICON(veh) = fread_string(fl, error);
						break;
					}
					case '1': {	// B1: quarter icon
						if (VEH_QUARTER_ICON(veh)) {
							free(VEH_QUARTER_ICON(veh));
						}
						VEH_QUARTER_ICON(veh) = fread_string(fl, error);
						break;
					}
					default: {
						log("SYSERR: Unknown B line in %s: %s", error, line);
						exit(1);
					}
				}
				break;
			}
			case 'C': {	// scalable
				if (!get_line(fl, line) || sscanf(line, "%d %d", &int_in[0], &int_in[1]) != 2) {
					log("SYSERR: Format error in C line of %s", error);
					exit(1);
				}
				
				VEH_MIN_SCALE_LEVEL(veh) = int_in[0];
				VEH_MAX_SCALE_LEVEL(veh) = int_in[1];
				break;
			}
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
			case 'E': {	// extra descs
				parse_extra_desc(fl, &VEH_EX_DESCS(veh), error);
				break;
			}
			case 'H': {	// height
				if (sscanf(line, "H 0 %d", &int_in[1]) != 1) {
					log("SYSERR: Format error in H line of %s: %s", error, line);
					exit(1);
				}
				VEH_HEIGHT(veh) = int_in[1];
				break;
			}
			case 'I': {	// interaction item
				parse_interaction(line, &VEH_INTERACTIONS(veh), error);
				break;
			}
			case 'K': {	// custom messages
				parse_custom_message(fl, &VEH_CUSTOM_MSGS(veh), error);
				break;
			}
			case 'L': {	// climate require/forbids
				if (sscanf(line, "L %s %s", str_in, str_in2) != 2) {
					log("SYSERR: Format error in L line of %s", error);
					exit(1);
				}
				
				VEH_REQUIRES_CLIMATE(veh) = asciiflag_conv(str_in);
				VEH_FORBID_CLIMATE(veh) = asciiflag_conv(str_in2);
				break;
			}
			case 'M': {	// mob spawn
				if (!get_line(fl, line) || sscanf(line, "%d %lf %s", &int_in[0], &dbl_in, str_in) != 3) {
					log("SYSERR: Format error in M line of %s", error);
					exit(1);
				}
				
				CREATE(spawn, struct spawn_info, 1);
				spawn->vnum = int_in[0];
				spawn->percent = dbl_in;
				spawn->flags = asciiflag_conv(str_in);
				
				LL_APPEND(VEH_SPAWNS(veh), spawn);
				break;
			}
			
			case 'P': { // speed bonuses (default is VSPEED_NORMAL, set above in clear_vehicle(veh)
				if (!get_line(fl, line) || sscanf(line, "%d", &int_in[0]) != 1) {
					log("SYSERR: Format error in P line of %s", error);
					exit(1);
				}
				
				VEH_SPEED_BONUSES(veh) = int_in[0];
				break;
			}
			
			case 'R': {	// resources/regular maintenance
				parse_resource(fl, &VEH_REGULAR_MAINTENANCE(veh), error);
				break;
			}
			
			case 'T': {	// trigger
				parse_trig_proto(line, &(veh->proto_script), error);
				break;
			}
			
			case 'U': {	// relation
				if (!get_line(fl, line)) {
					log("SYSERR: Missing U line of %s", error);
					exit(1);
				}
				if (sscanf(line, "%d %d", &int_in[0], &int_in[1]) != 2) {
					log("SYSERR: Format error in U line of %s", error);
					exit(1);
				}
				
				CREATE(relat, struct bld_relation, 1);
				relat->type = int_in[0];
				relat->vnum = int_in[1];
				LL_APPEND(VEH_RELATIONS(veh), relat);
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
	char temp[MAX_STRING_LENGTH], temp2[MAX_STRING_LENGTH], temp3[MAX_STRING_LENGTH];
	struct bld_relation *relat;
	struct spawn_info *spawn;
	
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
	
	// 6. flags move_type maxhealth capacity animals_required functions fame military size room-affects artisan
	strcpy(temp, bitv_to_alpha(VEH_FLAGS(veh)));
	strcpy(temp2, bitv_to_alpha(VEH_FUNCTIONS(veh)));
	strcpy(temp3, bitv_to_alpha(VEH_ROOM_AFFECTS(veh)));
	fprintf(fl, "%s %d %d %d %d %s %d %d %d %s %d %d\n", temp, VEH_MOVE_TYPE(veh), VEH_MAX_HEALTH(veh), VEH_CAPACITY(veh), VEH_ANIMALS_REQUIRED(veh), temp2, VEH_FAME(veh), VEH_MILITARY(veh), VEH_SIZE(veh), temp3, VEH_ARTISAN(veh), VEH_CITIZENS(veh));
	
	// B#: extra strings
	if (VEH_HALF_ICON(veh) && *VEH_HALF_ICON(veh)) {
		fprintf(fl, "B0\n%s~\n", VEH_HALF_ICON(veh));
	}
	if (VEH_QUARTER_ICON(veh) && *VEH_QUARTER_ICON(veh)) {
		fprintf(fl, "B1\n%s~\n", VEH_QUARTER_ICON(veh));
	}
	
	// C: scaling
	if (VEH_MIN_SCALE_LEVEL(veh) > 0 || VEH_MAX_SCALE_LEVEL(veh) > 0) {
		fprintf(fl, "C\n");
		fprintf(fl, "%d %d\n", VEH_MIN_SCALE_LEVEL(veh), VEH_MAX_SCALE_LEVEL(veh));
	}
	
	// 'D': designate/rooms
	if (VEH_INTERIOR_ROOM_VNUM(veh) != NOTHING || VEH_MAX_ROOMS(veh) || VEH_DESIGNATE_FLAGS(veh)) {
		strcpy(temp, bitv_to_alpha(VEH_DESIGNATE_FLAGS(veh)));
		fprintf(fl, "D\n%d %d %s\n", VEH_INTERIOR_ROOM_VNUM(veh), VEH_MAX_ROOMS(veh), temp);
	}
	
	// 'E': extra descs
	write_extra_descs_to_file(fl, 'E', VEH_EX_DESCS(veh));
	
	// I: interactions
	write_interactions_to_file(fl, VEH_INTERACTIONS(veh));
	
	// H: height
	if (VEH_HEIGHT(veh)) {
		fprintf(fl, "H 0 %d\n", VEH_HEIGHT(veh));
	}
	
	// K: custom messages
	write_custom_messages_to_file(fl, 'K', VEH_CUSTOM_MSGS(veh));
	
	// L: climate restrictions
	if (VEH_REQUIRES_CLIMATE(veh) || VEH_FORBID_CLIMATE(veh)) {
		strcpy(temp, bitv_to_alpha(VEH_REQUIRES_CLIMATE(veh)));
		strcpy(temp2, bitv_to_alpha(VEH_FORBID_CLIMATE(veh)));
		fprintf(fl, "L %s %s\n", temp, temp2);
	}
	
	// 'M': mob spawns
	LL_FOREACH(VEH_SPAWNS(veh), spawn) {
		fprintf(fl, "M\n%d %.2f %s\n", spawn->vnum, spawn->percent, bitv_to_alpha(spawn->flags));
	}
	
	// 'P': speed bonuses
	fprintf(fl, "P\n%d\n", VEH_SPEED_BONUSES(veh));
	
	// 'R': resources
	write_resources_to_file(fl, 'R', VEH_REGULAR_MAINTENANCE(veh));
	
	// T, V: triggers
	write_trig_protos_to_file(fl, 'T', veh->proto_script);
	
	// U: relations
	LL_FOREACH(VEH_RELATIONS(veh), relat) {
		fprintf(fl, "U\n%d %d\n", relat->type, relat->vnum);
	}
	
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
	struct obj_storage_type *store, *next_store;
	vehicle_data *veh, *iter, *next_iter;
	ability_data *abil, *next_abil;
	craft_data *craft, *next_craft;
	quest_data *quest, *next_quest;
	progress_data *prg, *next_prg;
	room_template *rmt, *next_rmt;
	social_data *soc, *next_soc;
	shop_data *shop, *next_shop;
	bld_data *bld, *next_bld;
	obj_data *obj, *next_obj;
	descriptor_data *desc;
	char name[256];
	bool found;
	
	if (!(veh = vehicle_proto(vnum))) {
		msg_to_char(ch, "There is no such vehicle %d.\r\n", vnum);
		return;
	}
	
	snprintf(name, sizeof(name), "%s", NULLSAFE(VEH_SHORT_DESC(veh)));
	
	// remove live vehicles
	DL_FOREACH_SAFE(vehicle_list, iter, next_iter) {
		if (VEH_VNUM(iter) != vnum) {
			continue;
		}
		
		if (ROOM_PEOPLE(IN_ROOM(iter))) {
			act("$V vanishes.", FALSE, ROOM_PEOPLE(IN_ROOM(iter)), NULL, iter, TO_CHAR | TO_ROOM | ACT_VEH_VICT);
		}
		extract_vehicle(iter);
	}
	
	// ensure vehicles are gone
	extract_pending_vehicles();
	
	// remove it from the hash table first
	remove_vehicle_from_table(veh);
	
	// save index and vehicle file now
	save_index(DB_BOOT_VEH);
	save_library_file_for_vnum(DB_BOOT_VEH, vnum);
	
	// update abilities
	HASH_ITER(hh, ability_table, abil, next_abil) {
		found = FALSE;
		found |= delete_from_interaction_list(&ABIL_INTERACTIONS(abil), TYPE_VEH, vnum);
		
		if (found) {
			syslog(SYS_OLC, GET_INVIS_LEV(ch), TRUE, "OLC: Ability %d %s lost deleted related vehicle", ABIL_VNUM(abil), ABIL_NAME(abil));
			save_library_file_for_vnum(DB_BOOT_ABIL, ABIL_VNUM(abil));
		}
	}
	
	// update buildings
	HASH_ITER(hh, building_table, bld, next_bld) {
		found = delete_from_interaction_list(&GET_BLD_INTERACTIONS(bld), TYPE_VEH, vnum);
		found |= delete_bld_relation_by_vnum(&GET_BLD_RELATIONS(bld), BLD_REL_STORES_LIKE_VEH, vnum);
		found |= delete_bld_relation_by_vnum(&GET_BLD_RELATIONS(bld), BLD_REL_UPGRADES_TO_VEH, vnum);
		found |= delete_bld_relation_by_vnum(&GET_BLD_RELATIONS(bld), BLD_REL_FORCE_UPGRADE_VEH, vnum);
		if (found) {
			syslog(SYS_OLC, GET_INVIS_LEV(ch), TRUE, "OLC: Building %d %s lost deleted related vehicle", GET_BLD_VNUM(bld), GET_BLD_NAME(bld));
			save_library_file_for_vnum(DB_BOOT_BLD, GET_BLD_VNUM(bld));
		}
	}
	
	// update crafts
	HASH_ITER(hh, craft_table, craft, next_craft) {
		found = FALSE;
		if (CRAFT_IS_VEHICLE(craft) && GET_CRAFT_OBJECT(craft) == vnum) {
			GET_CRAFT_OBJECT(craft) = NOTHING;
			found = TRUE;
		}
		
		if (found) {
			SET_BIT(GET_CRAFT_FLAGS(craft), CRAFT_IN_DEVELOPMENT);
			syslog(SYS_OLC, GET_INVIS_LEV(ch), TRUE, "OLC: Craft %d %s set IN-DEV due to deleted vehicle", GET_CRAFT_VNUM(craft), GET_CRAFT_NAME(craft));
			save_library_file_for_vnum(DB_BOOT_CRAFT, GET_CRAFT_VNUM(craft));
		}
	}
	
	// obj storage
	HASH_ITER(hh, object_table, obj, next_obj) {
		LL_FOREACH_SAFE(GET_OBJ_STORAGE(obj), store, next_store) {
			if (store->type == TYPE_VEH && store->vnum == vnum) {
				LL_DELETE(obj->proto_data->storage, store);
				free(store);
				syslog(SYS_OLC, GET_INVIS_LEV(ch), TRUE, "OLC: Object %d %s lost deleted storage vehicle", GET_OBJ_VNUM(obj), GET_OBJ_SHORT_DESC(obj));
				save_library_file_for_vnum(DB_BOOT_OBJ, GET_OBJ_VNUM(obj));
			}
		}
	}
	
	// update progress
	HASH_ITER(hh, progress_table, prg, next_prg) {
		found = delete_requirement_from_list(&PRG_TASKS(prg), REQ_OWN_VEHICLE, vnum);
		
		if (found) {
			SET_BIT(PRG_FLAGS(prg), PRG_IN_DEVELOPMENT);
			syslog(SYS_OLC, GET_INVIS_LEV(ch), TRUE, "OLC: Progress %d %s set IN-DEV due to deleted vehicle", PRG_VNUM(prg), PRG_NAME(prg));
			save_library_file_for_vnum(DB_BOOT_PRG, PRG_VNUM(prg));
			need_progress_refresh = TRUE;
		}
	}
	
	// update quests
	HASH_ITER(hh, quest_table, quest, next_quest) {
		found = delete_requirement_from_list(&QUEST_TASKS(quest), REQ_OWN_VEHICLE, vnum);
		found |= delete_requirement_from_list(&QUEST_PREREQS(quest), REQ_OWN_VEHICLE, vnum);
		found |= delete_quest_giver_from_list(&QUEST_STARTS_AT(quest), QG_VEHICLE, vnum);
		found |= delete_quest_giver_from_list(&QUEST_ENDS_AT(quest), QG_VEHICLE, vnum);
		
		if (found) {
			SET_BIT(QUEST_FLAGS(quest), QST_IN_DEVELOPMENT);
			syslog(SYS_OLC, GET_INVIS_LEV(ch), TRUE, "OLC: Quest %d %s set IN-DEV due to deleted vehicle", QUEST_VNUM(quest), QUEST_NAME(quest));
			save_library_file_for_vnum(DB_BOOT_QST, QUEST_VNUM(quest));
		}
	}
	
	// update room templates
	HASH_ITER(hh, room_template_table, rmt, next_rmt) {
		found = delete_from_spawn_template_list(&GET_RMT_SPAWNS(rmt), ADV_SPAWN_VEH, vnum);
		if (found) {
			syslog(SYS_OLC, GET_INVIS_LEV(ch), TRUE, "OLC: Room template %d %s lost deleted related vehicle", GET_RMT_VNUM(rmt), GET_RMT_TITLE(rmt));
			save_library_file_for_vnum(DB_BOOT_RMT, GET_RMT_VNUM(rmt));
		}
	}
	
	// update shops
	HASH_ITER(hh, shop_table, shop, next_shop) {
		// QG_x: quest types
		found = delete_quest_giver_from_list(&SHOP_LOCATIONS(shop), QG_VEHICLE, vnum);
		
		if (found) {
			SET_BIT(SHOP_FLAGS(shop), SHOP_IN_DEVELOPMENT);
			syslog(SYS_OLC, GET_INVIS_LEV(ch), TRUE, "OLC: Shop %d %s set IN-DEV due to deleted vehicle", SHOP_VNUM(shop), SHOP_NAME(shop));
			save_library_file_for_vnum(DB_BOOT_SHOP, SHOP_VNUM(shop));
		}
	}
	
	// update socials
	HASH_ITER(hh, social_table, soc, next_soc) {
		found = delete_requirement_from_list(&SOC_REQUIREMENTS(soc), REQ_OWN_VEHICLE, vnum);
		
		if (found) {
			SET_BIT(SOC_FLAGS(soc), SOC_IN_DEVELOPMENT);
			syslog(SYS_OLC, GET_INVIS_LEV(ch), TRUE, "OLC: Social %d %s set IN-DEV due to deleted vehicle", SOC_VNUM(soc), SOC_NAME(soc));
			save_library_file_for_vnum(DB_BOOT_SOC, SOC_VNUM(soc));
		}
	}
	
	// update vehicles
	HASH_ITER(hh, vehicle_table, iter, next_iter) {
		found = delete_from_interaction_list(&VEH_INTERACTIONS(iter), TYPE_VEH, vnum);
		found |= delete_bld_relation_by_vnum(&VEH_RELATIONS(iter), BLD_REL_STORES_LIKE_VEH, vnum);
		found |= delete_bld_relation_by_vnum(&VEH_RELATIONS(iter), BLD_REL_UPGRADES_TO_VEH, vnum);
		found |= delete_bld_relation_by_vnum(&VEH_RELATIONS(iter), BLD_REL_FORCE_UPGRADE_VEH, vnum);
		if (found) {
			syslog(SYS_OLC, GET_INVIS_LEV(ch), TRUE, "OLC: Vehicle %d %s lost deleted related vehicle", VEH_VNUM(iter), VEH_SHORT_DESC(veh));
			save_library_file_for_vnum(DB_BOOT_VEH, VEH_VNUM(iter));
		}
	}
	
	// olc editor updates
	LL_FOREACH(descriptor_list, desc) {
		if (GET_OLC_ABILITY(desc)) {
			found = FALSE;
			found |= delete_from_interaction_list(&ABIL_INTERACTIONS(GET_OLC_ABILITY(desc)), TYPE_VEH, vnum);
		
			if (found) {
				msg_to_desc(desc, "An vehicle listed in the interactions for the ability you're editing has been removed.\r\n");
			}
		}
		if (GET_OLC_BUILDING(desc)) {
			found = delete_from_interaction_list(&GET_BLD_INTERACTIONS(GET_OLC_BUILDING(desc)), TYPE_VEH, vnum);
			found |= delete_bld_relation_by_vnum(&GET_BLD_RELATIONS(GET_OLC_BUILDING(desc)), BLD_REL_STORES_LIKE_VEH, vnum);
			found |= delete_bld_relation_by_vnum(&GET_BLD_RELATIONS(GET_OLC_BUILDING(desc)), BLD_REL_UPGRADES_TO_VEH, vnum);
			found |= delete_bld_relation_by_vnum(&GET_BLD_RELATIONS(GET_OLC_BUILDING(desc)), BLD_REL_FORCE_UPGRADE_VEH, vnum);
			if (found) {
				msg_to_char(desc->character, "One of the vehicles used in the building you're editing was deleted.\r\n");
			}
		}
		if (GET_OLC_CRAFT(desc)) {
			found = FALSE;
			if (CRAFT_IS_VEHICLE(GET_OLC_CRAFT(desc)) && GET_OLC_CRAFT(desc)->object == vnum) {
				GET_OLC_CRAFT(desc)->object = NOTHING;
				found = TRUE;
			}
		
			if (found) {
				SET_BIT(GET_OLC_CRAFT(desc)->flags, CRAFT_IN_DEVELOPMENT);
				msg_to_char(desc->character, "The vehicle made by the craft you're editing was deleted.\r\n");
			}
		}
		if (GET_OLC_OBJECT(desc)) {
			found = FALSE;
			LL_FOREACH_SAFE(GET_OBJ_STORAGE(GET_OLC_OBJECT(desc)), store, next_store) {
				if (store->type == TYPE_VEH && store->vnum == vnum) {
					LL_DELETE(GET_OLC_OBJECT(desc)->proto_data->storage, store);
					free(store);
					if (!found) {
						msg_to_desc(desc, "A storage location for the the object you're editing was deleted.\r\n");
						found = TRUE;
					}
				}
			}
		}
		if (GET_OLC_PROGRESS(desc)) {
			found = delete_requirement_from_list(&PRG_TASKS(GET_OLC_PROGRESS(desc)), REQ_OWN_VEHICLE, vnum);
		
			if (found) {
				SET_BIT(QUEST_FLAGS(GET_OLC_PROGRESS(desc)), PRG_IN_DEVELOPMENT);
				msg_to_desc(desc, "A vehicle used by the progression goal you're editing has been deleted.\r\n");
			}
		}
		if (GET_OLC_QUEST(desc)) {
			found = delete_requirement_from_list(&QUEST_TASKS(GET_OLC_QUEST(desc)), REQ_OWN_VEHICLE, vnum);
			found |= delete_requirement_from_list(&QUEST_PREREQS(GET_OLC_QUEST(desc)), REQ_OWN_VEHICLE, vnum);
		
			if (found) {
				SET_BIT(QUEST_FLAGS(GET_OLC_QUEST(desc)), QST_IN_DEVELOPMENT);
				msg_to_desc(desc, "A vehicle used by the quest you are editing was deleted.\r\n");
			}
		}
		if (GET_OLC_ROOM_TEMPLATE(desc)) {
			if (delete_from_spawn_template_list(&GET_OLC_ROOM_TEMPLATE(desc)->spawns, ADV_SPAWN_VEH, vnum)) {
				msg_to_char(desc->character, "One of the vehicles that spawns in the room template you're editing was deleted.\r\n");
			}
		}
		if (GET_OLC_SHOP(desc)) {
			// QG_x: quest types
			found = delete_quest_giver_from_list(&SHOP_LOCATIONS(GET_OLC_SHOP(desc)), QG_VEHICLE, vnum);
		
			if (found) {
				SET_BIT(SHOP_FLAGS(GET_OLC_SHOP(desc)), SHOP_IN_DEVELOPMENT);
				msg_to_desc(desc, "A vehicle used by the shop you are editing was deleted.\r\n");
			}
		}
		if (GET_OLC_SOCIAL(desc)) {
			found = delete_requirement_from_list(&SOC_REQUIREMENTS(GET_OLC_SOCIAL(desc)), REQ_OWN_VEHICLE, vnum);
		
			if (found) {
				SET_BIT(SOC_FLAGS(GET_OLC_SOCIAL(desc)), SOC_IN_DEVELOPMENT);
				msg_to_desc(desc, "A vehicle required by the social you are editing was deleted.\r\n");
			}
		}
		if (GET_OLC_VEHICLE(desc)) {
			found = delete_from_interaction_list(&VEH_INTERACTIONS(GET_OLC_VEHICLE(desc)), TYPE_VEH, vnum);
			found |= delete_bld_relation_by_vnum(&VEH_RELATIONS(GET_OLC_VEHICLE(desc)), BLD_REL_STORES_LIKE_VEH, vnum);
			found |= delete_bld_relation_by_vnum(&VEH_RELATIONS(GET_OLC_VEHICLE(desc)), BLD_REL_UPGRADES_TO_VEH, vnum);
			found |= delete_bld_relation_by_vnum(&VEH_RELATIONS(GET_OLC_VEHICLE(desc)), BLD_REL_FORCE_UPGRADE_VEH, vnum);
			if (found) {
				msg_to_char(desc->character, "One of the vehicles used on the vehicle you're editing was deleted.\r\n");
			}
		}
	}
	
	syslog(SYS_OLC, GET_INVIS_LEV(ch), TRUE, "OLC: %s has deleted vehicle %d %s", GET_NAME(ch), vnum, name);
	msg_to_char(ch, "Vehicle %d (%s) deleted.\r\n", vnum, name);
	
	free_vehicle(veh);
}


/**
* Searches properties of vehicles.
*
* @param char_data *ch The person searching.
* @param char *argument The argument they entered.
*/
void olc_fullsearch_vehicle(char_data *ch, char *argument) {
	char buf[MAX_STRING_LENGTH * 2], line[MAX_STRING_LENGTH], type_arg[MAX_INPUT_LENGTH], val_arg[MAX_INPUT_LENGTH], find_keywords[MAX_INPUT_LENGTH], extra_search[MAX_INPUT_LENGTH];
	int count;
	bool found_one;
	
	char only_icon[MAX_INPUT_LENGTH], only_half_icon[MAX_INPUT_LENGTH], only_quarter_icon[MAX_INPUT_LENGTH];
	bitvector_t only_designate = NOBITS, only_flags = NOBITS, only_functions = NOBITS, only_affs = NOBITS;
	bitvector_t find_interacts = NOBITS, not_flagged = NOBITS, found_interacts = NOBITS, find_custom = NOBITS, found_custom = NOBITS;
	int only_animals = NOTHING, only_cap = NOTHING, cap_over = NOTHING, cap_under = NOTHING;
	int only_fame = NOTHING, fame_over = NOTHING, fame_under = NOTHING, only_speed = NOTHING;
	int only_height = NOTHING, height_over = NOTHING, height_under = NOTHING;
	int only_hitpoints = NOTHING, hitpoints_over = NOTHING, hitpoints_under = NOTHING, only_level = NOTHING;
	int only_military = NOTHING, military_over = NOTHING, military_under = NOTHING;
	int only_rooms = NOTHING, rooms_over = NOTHING, rooms_under = NOTHING, only_move = NOTHING;
	int size_under = NOTHING, size_over = NOTHING, only_depletion = NOTHING, vmin = NOTHING, vmax = NOTHING;
	struct custom_message *cust;
	bool needs_animals = FALSE;
	
	struct interaction_item *inter;
	struct interact_restriction *inter_res;
	vehicle_data *veh, *next_veh;
	size_t size;
	
	*only_icon = '\0';
	*only_half_icon = '\0';
	*only_quarter_icon = '\0';
	
	if (!*argument) {
		msg_to_char(ch, "See HELP VEDIT FULLSEARCH for syntax.\r\n");
		return;
	}
	
	// process argument
	*find_keywords = '\0';
	*extra_search = '\0';
	while (*argument) {
		// figure out a type
		argument = any_one_arg(argument, type_arg);
		
		if (!strcmp(type_arg, "-")) {
			continue;	// just skip stray dashes
		}
		
		// else-ifs defined in olc.h process these args:
		FULLSEARCH_FLAGS("affects", only_affs, room_aff_bits)
		FULLSEARCH_INT("animalsrequired", only_animals, 0, INT_MAX)
		FULLSEARCH_BOOL("anyanimalsrequired", needs_animals)
		FULLSEARCH_INT("capacity", only_cap, 0, INT_MAX)
		FULLSEARCH_INT("capacityover", cap_over, 0, INT_MAX)
		FULLSEARCH_INT("capacityunder", cap_under, 0, INT_MAX)
		FULLSEARCH_FLAGS("custom", find_custom, veh_custom_types)
		FULLSEARCH_LIST("depletion", only_depletion, depletion_types)
		FULLSEARCH_FLAGS("designate", only_designate, designate_flags)
		FULLSEARCH_STRING("extradesc", extra_search)
		FULLSEARCH_INT("fame", only_fame, 0, INT_MAX)
		FULLSEARCH_INT("fameover", fame_over, 0, INT_MAX)
		FULLSEARCH_INT("fameunder", fame_under, 0, INT_MAX)
		FULLSEARCH_FLAGS("flagged", only_flags, vehicle_flags)
		FULLSEARCH_FLAGS("flags", only_flags, vehicle_flags)
		FULLSEARCH_FLAGS("unflagged", not_flagged, vehicle_flags)
		FULLSEARCH_FLAGS("functions", only_functions, function_flags)
		FULLSEARCH_STRING("icon", only_icon)
		FULLSEARCH_STRING("halficon", only_half_icon)
		FULLSEARCH_STRING("quartericon", only_quarter_icon)
		FULLSEARCH_FLAGS("interaction", find_interacts, interact_types)
		FULLSEARCH_INT("height", only_height, 0, INT_MAX)
		FULLSEARCH_INT("heightover", height_over, 0, INT_MAX)
		FULLSEARCH_INT("heightunder", height_under, 0, INT_MAX)
		FULLSEARCH_INT("hitpoints", only_hitpoints, 0, INT_MAX)
		FULLSEARCH_INT("hitpointsover", hitpoints_over, 0, INT_MAX)
		FULLSEARCH_INT("hitpointsunder", hitpoints_under, 0, INT_MAX)
		FULLSEARCH_LIST("movetype", only_move, mob_move_types)
		FULLSEARCH_INT("level", only_level, 0, INT_MAX)
		FULLSEARCH_INT("rooms", only_rooms, 0, INT_MAX)
		FULLSEARCH_INT("roomsover", rooms_over, 0, INT_MAX)
		FULLSEARCH_INT("roomsunder", rooms_under, 0, INT_MAX)
		FULLSEARCH_INT("military", only_military, 0, INT_MAX)
		FULLSEARCH_INT("militaryover", military_over, 0, INT_MAX)
		FULLSEARCH_INT("militaryunder", military_under, 0, INT_MAX)
		FULLSEARCH_INT("sizeover", size_over, 0, INT_MAX)
		FULLSEARCH_INT("sizeunder", size_under, 0, INT_MAX)
		FULLSEARCH_LIST("speed", only_speed, vehicle_speed_types)
		FULLSEARCH_INT("vmin", vmin, 0, INT_MAX)
		FULLSEARCH_INT("vmax", vmax, 0, INT_MAX)
		
		else {	// not sure what to do with it? treat it like a keyword
			sprintf(find_keywords + strlen(find_keywords), "%s%s", *find_keywords ? " " : "", type_arg);
		}
		
		// prepare for next loop
		skip_spaces(&argument);
	}
	
	size = snprintf(buf, sizeof(buf), "Vehicle fullsearch: %s\r\n", show_color_codes(find_keywords));
	count = 0;
	
	// okay now look up items
	HASH_ITER(hh, vehicle_table, veh, next_veh) {
		if ((vmin != NOTHING && VEH_VNUM(veh) < vmin) || (vmax != NOTHING && VEH_VNUM(veh) > vmax)) {
			continue;	// vnum range
		}
		if (only_affs != NOBITS && (VEH_ROOM_AFFECTS(veh) & only_affs) != only_affs) {
			continue;
		}
		if (only_animals != NOTHING && VEH_ANIMALS_REQUIRED(veh) != only_animals) {
			continue;
		}
		if (needs_animals && VEH_ANIMALS_REQUIRED(veh) == 0) {
			continue;
		}
		if (only_cap != NOTHING && VEH_CAPACITY(veh) != only_cap) {
			continue;
		}
		if (cap_over != NOTHING && VEH_CAPACITY(veh) < cap_over) {
			continue;
		}
		if (cap_under != NOTHING && (VEH_CAPACITY(veh) > cap_under || VEH_CAPACITY(veh) == 0)) {
			continue;
		}
		if (only_designate != NOBITS && (VEH_DESIGNATE_FLAGS(veh) & only_designate) != only_designate) {
			continue;
		}
		if (only_fame != NOTHING && VEH_FAME(veh) != only_fame) {
			continue;
		}
		if (fame_over != NOTHING && VEH_FAME(veh) < fame_over) {
			continue;
		}
		if (fame_under != NOTHING && (VEH_FAME(veh) > fame_under || VEH_FAME(veh) == 0)) {
			continue;
		}
		if (not_flagged != NOBITS && VEH_FLAGGED(veh, not_flagged)) {
			continue;
		}
		if (only_flags != NOBITS && (VEH_FLAGS(veh) & only_flags) != only_flags) {
			continue;
		}
		if (only_functions != NOBITS && (VEH_FUNCTIONS(veh) & only_functions) != only_functions) {
			continue;
		}
		if (only_height != NOTHING && VEH_HEIGHT(veh) != only_height) {
			continue;
		}
		if (height_over != NOTHING && VEH_HEIGHT(veh) < height_over) {
			continue;
		}
		if (height_under != NOTHING && VEH_HEIGHT(veh) > height_under) {
			continue;
		}
		if (only_hitpoints != NOTHING && VEH_MAX_HEALTH(veh) != only_hitpoints) {
			continue;
		}
		if (hitpoints_over != NOTHING && VEH_MAX_HEALTH(veh) < hitpoints_over) {
			continue;
		}
		if (hitpoints_under != NOTHING && VEH_MAX_HEALTH(veh) > hitpoints_under) {
			continue;
		}
		if (*extra_search && !find_exdesc(extra_search, VEH_EX_DESCS(veh), NULL)) {
			continue;
		}
		if (*only_icon && !VEH_ICON(veh)) {
			continue;
		}
		if (*only_icon && str_cmp(only_icon, "any") && !strstr(only_icon, VEH_ICON(veh)) && !strstr(only_icon, strip_color(VEH_ICON(veh)))) {
			continue;
		}
		if (*only_half_icon && !VEH_HALF_ICON(veh)) {
			continue;
		}
		if (*only_half_icon && str_cmp(only_half_icon, "any") && !strstr(only_half_icon, VEH_HALF_ICON(veh)) && !strstr(only_half_icon, strip_color(VEH_HALF_ICON(veh)))) {
			continue;
		}
		if (*only_quarter_icon && !VEH_QUARTER_ICON(veh)) {
			continue;
		}
		if (*only_quarter_icon && str_cmp(only_quarter_icon, "any") && !strstr(only_quarter_icon, VEH_QUARTER_ICON(veh)) && !strstr(only_quarter_icon, strip_color(VEH_QUARTER_ICON(veh)))) {
			continue;
		}
		if (find_interacts) {	// look up its interactions
			found_interacts = NOBITS;
			LL_FOREACH(VEH_INTERACTIONS(veh), inter) {
				found_interacts |= BIT(inter->type);
			}
			if ((find_interacts & found_interacts) != find_interacts) {
				continue;
			}
		}
		if (only_depletion != NOTHING) {
			found_one = FALSE;
			LL_FOREACH(VEH_INTERACTIONS(veh), inter) {
				LL_FOREACH(inter->restrictions, inter_res) {
					if (inter_res->type == INTERACT_RESTRICT_DEPLETION && inter_res->vnum == only_depletion) {
						found_one = TRUE;
						break;
					}
				}
			}
			if (!found_one) {
				continue;
			}
		}
		if (only_level != NOTHING) {	// level-based checks
			if (VEH_MAX_SCALE_LEVEL(veh) != 0 && only_level > VEH_MAX_SCALE_LEVEL(veh)) {
				continue;
			}
			if (VEH_MIN_SCALE_LEVEL(veh) != 0 && only_level < VEH_MIN_SCALE_LEVEL(veh)) {
				continue;
			}
		}
		if (only_military != NOTHING && VEH_MILITARY(veh) != only_military) {
			continue;
		}
		if (military_over != NOTHING && VEH_MILITARY(veh) < military_over) {
			continue;
		}
		if (military_under != NOTHING && (VEH_MILITARY(veh) > military_under || VEH_MILITARY(veh) == 0)) {
			continue;
		}
		if (only_move != NOTHING && VEH_MOVE_TYPE(veh) != only_move) {
			continue;
		}
		if (only_rooms != NOTHING && VEH_MAX_ROOMS(veh) != only_rooms) {
			continue;
		}
		if (rooms_over != NOTHING && VEH_MAX_ROOMS(veh) < rooms_over) {
			continue;
		}
		if (rooms_under != NOTHING && (VEH_MAX_ROOMS(veh) > rooms_under || VEH_MAX_ROOMS(veh) == 0)) {
			continue;
		}
		if (size_over != NOTHING && VEH_SIZE(veh) < size_over) {
			continue;
		}
		if (size_under != NOTHING && VEH_SIZE(veh) > size_under) {
			continue;
		}
		if (only_speed != NOTHING && VEH_SPEED_BONUSES(veh) != only_speed) {
			continue;
		}
		if (find_custom) {	// look up its custom messages
			found_custom = NOBITS;
			LL_FOREACH(VEH_CUSTOM_MSGS(veh), cust) {
				found_custom |= BIT(cust->type);
			}
			if ((find_custom & found_custom) != find_custom) {
				continue;
			}
		}
		
		if (*find_keywords && !multi_isname(find_keywords, VEH_KEYWORDS(veh)) && !multi_isname(find_keywords, VEH_LONG_DESC(veh)) && !multi_isname(find_keywords, VEH_LOOK_DESC(veh)) && !multi_isname(find_keywords, VEH_SHORT_DESC(veh)) && !search_extra_descs(find_keywords, VEH_EX_DESCS(veh)) && !search_custom_messages(find_keywords, VEH_CUSTOM_MSGS(veh))) {
			continue;
		}
		
		// show it
		snprintf(line, sizeof(line), "[%5d] %s\r\n", VEH_VNUM(veh), VEH_SHORT_DESC(veh));
		if (strlen(line) + size < sizeof(buf)) {
			size += snprintf(buf + size, sizeof(buf) - size, "%s", line);
			++count;
		}
		else {
			size += snprintf(buf + size, sizeof(buf) - size, "OVERFLOW\r\n");
			break;
		}
	}
	
	if (count > 0 && (size + 20) < sizeof(buf)) {
		size += snprintf(buf + size, sizeof(buf) - size, "(%d vehicles)\r\n", count);
	}
	else if (count == 0) {
		size += snprintf(buf + size, sizeof(buf) - size, " none\r\n");
	}
	
	if (ch->desc) {
		page_string(ch->desc, buf, TRUE);
	}
}


/**
* Function to save a player's changes to a vehicle (or a new one).
*
* @param descriptor_data *desc The descriptor who is saving.
*/
void save_olc_vehicle(descriptor_data *desc) {
	vehicle_data *proto, *veh = GET_OLC_VEHICLE(desc), *iter;
	any_vnum vnum = GET_OLC_VNUM(desc);
	struct spawn_info *spawn;
	struct quest_lookup *ql;
	struct shop_lookup *sl;
	bitvector_t old_flags, add_affs, rem_affs;
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
	if (VEH_HALF_ICON(veh) && !*VEH_HALF_ICON(veh)) {
		free(VEH_HALF_ICON(veh));
		VEH_HALF_ICON(veh) = NULL;
	}
	if (VEH_QUARTER_ICON(veh) && !*VEH_QUARTER_ICON(veh)) {
		free(VEH_QUARTER_ICON(veh));
		VEH_QUARTER_ICON(veh) = NULL;
	}
	VEH_HEALTH(veh) = VEH_MAX_HEALTH(veh);
	prune_extra_descs(&VEH_EX_DESCS(veh));
	
	// determine if affs were added or removed, for updating live vehs
	add_affs = VEH_ROOM_AFFECTS(veh) & ~VEH_ROOM_AFFECTS(proto);
	rem_affs = VEH_ROOM_AFFECTS(proto) & ~VEH_ROOM_AFFECTS(veh);
	
	// update live vehicles
	DL_FOREACH(vehicle_list, iter) {
		if (VEH_VNUM(iter) != vnum) {
			continue;
		}
		
		// flags (preserve the state of the savable flags only)
		old_flags = VEH_FLAGS(iter) & SAVABLE_VEH_FLAGS;
		VEH_FLAGS(iter) = (VEH_FLAGS(veh) & ~SAVABLE_VEH_FLAGS) | old_flags;
		
		// apply any changed affs, but leave the rest alone
		SET_BIT(VEH_ROOM_AFFECTS(iter), add_affs);
		REMOVE_BIT(VEH_ROOM_AFFECTS(iter), rem_affs);
		
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
		if (VEH_HALF_ICON(iter) == VEH_HALF_ICON(proto)) {
			VEH_HALF_ICON(iter) = VEH_HALF_ICON(veh);
		}
		if (VEH_QUARTER_ICON(iter) == VEH_QUARTER_ICON(proto)) {
			VEH_QUARTER_ICON(iter) = VEH_QUARTER_ICON(veh);
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
		
		// remove old scripts
		if (SCRIPT(iter)) {
			remove_all_triggers(iter, VEH_TRIGGER);
		}
		if (iter->proto_script && iter->proto_script != proto->proto_script) {
			free_proto_scripts(&iter->proto_script);
		}
		
		// re-attach scripts
		iter->proto_script = copy_trig_protos(veh->proto_script);
		assign_triggers(iter, VEH_TRIGGER);
		
		// sanity checks
		if (VEH_HEALTH(iter) > VEH_MAX_HEALTH(iter)) {
			VEH_HEALTH(iter) = VEH_MAX_HEALTH(iter);
		}
		
		affect_total_room(IN_ROOM(iter));
		request_vehicle_save_in_world(iter);
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
	if (VEH_HALF_ICON(proto)) {
		free(VEH_HALF_ICON(proto));
	}
	if (VEH_QUARTER_ICON(proto)) {
		free(VEH_QUARTER_ICON(proto));
	}
	if (VEH_LONG_DESC(proto)) {
		free(VEH_LONG_DESC(proto));
	}
	if (VEH_LOOK_DESC(proto)) {
		free(VEH_LOOK_DESC(proto));
	}
	if (VEH_REGULAR_MAINTENANCE(proto)) {
		free_resource_list(VEH_REGULAR_MAINTENANCE(proto));
	}
	free_interactions(&VEH_INTERACTIONS(proto));
	while ((spawn = VEH_SPAWNS(proto))) {
		VEH_SPAWNS(proto) = spawn->next;
		free(spawn);
	}
	free_extra_descs(&VEH_EX_DESCS(proto));
	free_custom_messages(VEH_CUSTOM_MSGS(proto));
	free_bld_relations(VEH_RELATIONS(proto));
	free(proto->attributes);
	
	// free old script?
	if (proto->proto_script) {
		free_proto_scripts(&proto->proto_script);
	}
	
	// save data back over the proto-type
	hh = proto->hh;	// save old hash handle
	ql = proto->quest_lookups;	// save lookups
	sl = proto->shop_lookups;
	
	*proto = *veh;	// copy over all data
	proto->vnum = vnum;	// ensure correct vnum
	
	proto->hh = hh;	// restore old hash handle
	proto->quest_lookups = ql;	// restore lookups
	proto->shop_lookups = sl;
		
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
		VEH_HALF_ICON(new) = VEH_HALF_ICON(input) ? str_dup(VEH_HALF_ICON(input)) : NULL;
		VEH_QUARTER_ICON(new) = VEH_QUARTER_ICON(input) ? str_dup(VEH_QUARTER_ICON(input)) : NULL;
		VEH_LONG_DESC(new) = VEH_LONG_DESC(input) ? str_dup(VEH_LONG_DESC(input)) : NULL;
		VEH_LOOK_DESC(new) = VEH_LOOK_DESC(input) ? str_dup(VEH_LOOK_DESC(input)) : NULL;
		
		// copy lists
		VEH_REGULAR_MAINTENANCE(new) = copy_resource_list(VEH_REGULAR_MAINTENANCE(input));
		VEH_EX_DESCS(new) = copy_extra_descs(VEH_EX_DESCS(input));
		VEH_INTERACTIONS(new) = copy_interaction_list(VEH_INTERACTIONS(input));
		VEH_SPAWNS(new) = copy_spawn_list(VEH_SPAWNS(input));
		VEH_CUSTOM_MSGS(new) = copy_custom_messages(VEH_CUSTOM_MSGS(input));
		VEH_RELATIONS(new) = VEH_RELATIONS(input) ? copy_bld_relations(VEH_RELATIONS(input)) : NULL;
		
		// copy scripts
		SCRIPT(new) = NULL;
		new->proto_script = copy_trig_protos(input->proto_script);
	}
	else {
		// brand new: some defaults
		VEH_KEYWORDS(new) = str_dup(default_vehicle_keywords);
		VEH_SHORT_DESC(new) = str_dup(default_vehicle_short_desc);
		VEH_LONG_DESC(new) = str_dup(default_vehicle_long_desc);
		VEH_MAX_HEALTH(new) = 1;
		VEH_MOVE_TYPE(new) = MOB_MOVE_DRIVES;
		SCRIPT(new) = NULL;
		new->proto_script = NULL;
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
	char buf[MAX_STRING_LENGTH * 2], part[MAX_STRING_LENGTH];
	struct room_extra_data *red, *next_red;
	struct custom_message *custm;
	struct depletion_data *dep;
	obj_data *obj;
	size_t size;
	bool comma;
	int found;
	struct string_hash *str_iter, *next_str, *str_hash = NULL;
	
	if (!veh) {
		return;
	}
	
	// first line
	size = snprintf(buf, sizeof(buf), "VNum: [\tc%d\t0], S-Des: \tc%s\t0, Keywords: \ty%s\t0, Idnum: [%5d]\r\n", VEH_VNUM(veh), VEH_SHORT_DESC(veh), VEH_KEYWORDS(veh), IN_ROOM(veh) ? VEH_IDNUM(veh) : 0);
	
	size += snprintf(buf + size, sizeof(buf) - size, "L-Des: %s\r\n", VEH_LONG_DESC(veh));
	
	if (VEH_LOOK_DESC(veh) && *VEH_LOOK_DESC(veh)) {
		size += snprintf(buf + size, sizeof(buf) - size, "%s", VEH_LOOK_DESC(veh));
	}
	
	if (VEH_EX_DESCS(veh)) {
		struct extra_descr_data *desc;
		size += snprintf(buf + size, sizeof(buf) - size, "Extra descs:\tc");
		LL_FOREACH(VEH_EX_DESCS(veh), desc) {
			size += snprintf(buf + size, sizeof(buf) - size, " %s", desc->keyword);
		}
		size += snprintf(buf + size, sizeof(buf) - size, "\t0\r\n");
	}
	
	// icons:
	if (VEH_ICON(veh)) {
		replace_question_color(NULLSAFE(VEH_ICON(veh)), "&0", part);
		size += snprintf(buf + size, sizeof(buf) - size, "Full Icon: %s\t0 %s", part, show_color_codes(VEH_ICON(veh)));
	}
	if (VEH_HALF_ICON(veh)) {
		replace_question_color(NULLSAFE(VEH_HALF_ICON(veh)), "&0", part);
		size += snprintf(buf + size, sizeof(buf) - size, "%sHalf Icon: %s\t0 %s", (VEH_ICON(veh) ? ", " : ""), part, show_color_codes(VEH_HALF_ICON(veh)));
	}
	if (VEH_QUARTER_ICON(veh)) {
		replace_question_color(NULLSAFE(VEH_QUARTER_ICON(veh)), "&0", part);
		size += snprintf(buf + size, sizeof(buf) - size, "%sQuarter Icon: %s\t0 %s", ((VEH_ICON(veh) || VEH_HALF_ICON(veh)) ? ", " : ""), part, show_color_codes(VEH_QUARTER_ICON(veh)));
	}
	if (VEH_ICON(veh) || VEH_HALF_ICON(veh) || VEH_QUARTER_ICON(veh)) {
		size += snprintf(buf + size, sizeof(buf) - size, "\r\n");
	}
	
	// stats lines
	size += snprintf(buf + size, sizeof(buf) - size, "Health: [\tc%d\t0/\tc%d\t0], Capacity: [\tc%d\t0/\tc%d\t0], Animals Req: [\tc%d\t0], Move Type: [\ty%s\t0]\r\n", (int) VEH_HEALTH(veh), VEH_MAX_HEALTH(veh), VEH_CARRYING_N(veh), VEH_CAPACITY(veh), VEH_ANIMALS_REQUIRED(veh), mob_move_types[VEH_MOVE_TYPE(veh)]);
	size += snprintf(buf + size, sizeof(buf) - size, "Speed: [\ty%s\t0], Size: [\tc%d\t0], Height: [\tc%d\t0]\r\n", vehicle_speed_types[VEH_SPEED_BONUSES(veh)], VEH_SIZE(veh), VEH_HEIGHT(veh));
	size += snprintf(buf + size, sizeof(buf) - size, "Citizens: [\tc%d\t0], Artisan: [&c%d&0] &y%s&0\r\n", VEH_CITIZENS(veh), VEH_ARTISAN(veh), VEH_ARTISAN(veh) != NOTHING ? get_mob_name_by_proto(VEH_ARTISAN(veh), FALSE) : "none");
	size += snprintf(buf + size, sizeof(buf) - size, "Fame: [\tc%d\t0], Military: [\tc%d\t0]\r\n", VEH_FAME(veh), VEH_MILITARY(veh));
	
	if (VEH_INTERIOR_ROOM_VNUM(veh) != NOTHING || VEH_MAX_ROOMS(veh) || VEH_DESIGNATE_FLAGS(veh)) {
		sprintbit(VEH_DESIGNATE_FLAGS(veh), designate_flags, part, TRUE);
		size += snprintf(buf + size, sizeof(buf) - size, "Interior: [\tc%d\t0 - \ty%s\t0], Rooms: [\tc%d\t0], Designate: \ty%s\t0\r\n", VEH_INTERIOR_ROOM_VNUM(veh), building_proto(VEH_INTERIOR_ROOM_VNUM(veh)) ? GET_BLD_NAME(building_proto(VEH_INTERIOR_ROOM_VNUM(veh))) : "none", VEH_MAX_ROOMS(veh), part);
	}
	else {
		size += snprintf(buf + size, sizeof(buf) - size, "Interior: [\tc-1\t0 - \tynone\t0], Rooms: [\tc0\t0]\r\n");
	}
	
	sprintbit(VEH_FLAGS(veh), vehicle_flags, part, TRUE);
	size += snprintf(buf + size, sizeof(buf) - size, "Flags: \tg%s\t0\r\n", part);
	
	sprintbit(VEH_ROOM_AFFECTS(veh), room_aff_bits, part, TRUE);
	size += snprintf(buf + size, sizeof(buf) - size, "Affects: \tc%s\t0\r\n", part);
	
	sprintbit(VEH_FUNCTIONS(veh), function_flags, part, TRUE);
	size += snprintf(buf + size, sizeof(buf) - size, "Functions: \tg%s\t0\r\n", part);
	
	ordered_sprintbit(VEH_REQUIRES_CLIMATE(veh), climate_flags, climate_flags_order, FALSE, part);
	size += snprintf(buf + size, sizeof(buf) - size, "Requires climate: \tc%s\t0\r\n", part);
	ordered_sprintbit(VEH_FORBID_CLIMATE(veh), climate_flags, climate_flags_order, FALSE, part);
	size += snprintf(buf + size, sizeof(buf) - size, "Forbid climate: \tg%s\t0\r\n", part);
	
	if (VEH_INTERACTIONS(veh)) {
		get_interaction_display(VEH_INTERACTIONS(veh), part);
		size += snprintf(buf + size, sizeof(buf) - size, "Interactions:\r\n%s", part);
	}
	
	if (VEH_RELATIONS(veh)) {
		get_bld_relations_display(VEH_RELATIONS(veh), part);
		size += snprintf(buf + size, sizeof(buf) - size, "Relations:\r\n%s", part);
	}
	
	if (VEH_REGULAR_MAINTENANCE(veh)) {
		get_resource_display(ch, VEH_REGULAR_MAINTENANCE(veh), part);
		size += snprintf(buf + size, sizeof(buf) - size, "Regular maintenance:\r\n%s", part);
	}
	
	size += snprintf(buf + size, sizeof(buf) - size, "Scaled to level: [\tc%d (%d-%d)\t0], Owner: [%s%s\t0]\r\n", VEH_SCALE_LEVEL(veh), VEH_MIN_SCALE_LEVEL(veh), VEH_MAX_SCALE_LEVEL(veh), VEH_OWNER(veh) ? EMPIRE_BANNER(VEH_OWNER(veh)) : "", VEH_OWNER(veh) ? EMPIRE_NAME(VEH_OWNER(veh)) : "nobody");
	
	if (VEH_INTERIOR_HOME_ROOM(veh) || VEH_INSIDE_ROOMS(veh) > 0) {
		size += snprintf(buf + size, sizeof(buf) - size, "Interior location: [\ty%d\t0], Added rooms: [\tg%d\t0/\tg%d\t0]\r\n", VEH_INTERIOR_HOME_ROOM(veh) ? GET_ROOM_VNUM(VEH_INTERIOR_HOME_ROOM(veh)) : NOTHING, VEH_INSIDE_ROOMS(veh), VEH_MAX_ROOMS(veh));
	}
	
	if (IN_ROOM(veh)) {
		size += snprintf(buf + size, sizeof(buf) - size, "In room: [%d] %s, Led by: %s, ", GET_ROOM_VNUM(IN_ROOM(veh)), get_room_name(IN_ROOM(veh), FALSE), VEH_LED_BY(veh) ? PERS(VEH_LED_BY(veh), ch, TRUE) : "nobody");
		size += snprintf(buf + size, sizeof(buf) - size, "Sitting on: %s, ", VEH_SITTING_ON(veh) ? PERS(VEH_SITTING_ON(veh), ch, TRUE) : "nobody");
		size += snprintf(buf + size, sizeof(buf) - size, "Driven by: %s\r\n", VEH_DRIVER(veh) ? PERS(VEH_DRIVER(veh), ch, TRUE) : "nobody");
	}
	
	if (VEH_DEPLETION(veh)) {
		size += snprintf(buf + size, sizeof(buf) - size, "Depletion: ");
		
		comma = FALSE;
		for (dep = VEH_DEPLETION(veh); dep; dep = dep->next) {
			if (dep->type < NUM_DEPLETION_TYPES) {
				size += snprintf(buf + size, sizeof(buf) - size, "%s%s (%d)", comma ? ", " : "", depletion_types[dep->type], dep->count);
				comma = TRUE;
			}
		}
		size += snprintf(buf + size, sizeof(buf) - size, "\r\n");
	}
	
	if (VEH_CONTAINS(veh)) {
		DL_FOREACH2(VEH_CONTAINS(veh), obj, next_content) {
			add_string_hash(&str_hash, GET_OBJ_DESC(obj, ch, OBJ_DESC_SHORT), 1);
		}
		
		found = 0;
		sprintf(part, "Contents:\tg");
		HASH_ITER(hh, str_hash, str_iter, next_str) {
			if (str_iter->count == 1) {
				sprintf(part + strlen(part), "%s %s", found++ ? "," : "", skip_filler(str_iter->str));
			}
			else {
				sprintf(part + strlen(part), "%s %s (x%d)", found++ ? "," : "", skip_filler(str_iter->str), str_iter->count);
			}
			
			if (strlen(part) >= 62) {
				if (next_str) {
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
		
		free_string_hash(&str_hash);
	}
	
	if (VEH_NEEDS_RESOURCES(veh)) {
		get_resource_display(ch, VEH_NEEDS_RESOURCES(veh), part);
		size += snprintf(buf + size, sizeof(buf) - size, "%s resources:\r\n%s", VEH_IS_DISMANTLING(veh) ? "Dismantle" : "Needs", part);
	}
	
	if (VEH_CUSTOM_MSGS(veh)) {
		size += snprintf(buf + size, sizeof(buf) - size, "Custom messages:\r\n");
		
		LL_FOREACH(VEH_CUSTOM_MSGS(veh), custm) {
			size += snprintf(buf + size, sizeof(buf) - size, " %s: %s\r\n", veh_custom_types[custm->type], custm->msg);
		}
	}
	
	send_to_char(buf, ch);
	show_spawn_summary_to_char(ch, VEH_SPAWNS(veh));
	
	if (VEH_EXTRA_DATA(veh)) {
		msg_to_char(ch, "Extra data:\r\n");
		HASH_ITER(hh, VEH_EXTRA_DATA(veh), red, next_red) {
			sprinttype(red->type, room_extra_types, buf, sizeof(buf), "UNDEFINED");
			msg_to_char(ch, " %s: %d\r\n", buf, red->value);
		}
	}
	
	// script info
	msg_to_char(ch, "Script information (id %d):\r\n", (SCRIPT(veh) && IN_ROOM(veh)) ? veh_script_id(veh) : veh->script_id);
	if (SCRIPT(veh)) {
		script_stat(ch, SCRIPT(veh));
	}
	else {
		msg_to_char(ch, "  None.\r\n");
	}
}


/**
* Perform a look-at-vehicle.
*
* @param vehicle_data *veh The vehicle to look at.
* @param char_data *ch The person to show the output to.
*/
void look_at_vehicle(vehicle_data *veh, char_data *ch) {
	char lbuf[MAX_STRING_LENGTH], colbuf[256];
	player_index_data *index;
	vehicle_data *proto;
	
	if (!veh || !ch || !ch->desc) {
		return;
	}
	
	proto = vehicle_proto(VEH_VNUM(veh));
	
	if (VEH_LOOK_DESC(veh) && *VEH_LOOK_DESC(veh)) {
		msg_to_char(ch, "You look at %s:\r\n%s", VEH_SHORT_DESC(veh), VEH_LOOK_DESC(veh));
	}
	else {
		act("You look at $V but see nothing special.", FALSE, ch, NULL, veh, TO_CHAR | ACT_VEH_VICT);
	}
	
	if (proto && VEH_SHORT_DESC(veh) != VEH_SHORT_DESC(proto) && !strchr(VEH_SHORT_DESC(proto), '#')) {
		strcpy(lbuf, skip_filler(VEH_SHORT_DESC(proto)));
		msg_to_char(ch, "It appears to be %s %s.\r\n", AN(lbuf), lbuf);
	}
	
	if (VEH_OWNER(veh)) {
		msg_to_char(ch, "It is flying the flag of %s%s\t0", EMPIRE_BANNER(VEH_OWNER(veh)), EMPIRE_NAME(VEH_OWNER(veh)));
		
		if (VEH_OWNER(veh) == GET_LOYALTY(ch)) {
			if (VEH_FLAGGED(veh, VEH_PLAYER_NO_WORK)) {
				send_to_char(" (no-work)", ch);
			}
			if (VEH_FLAGGED(veh, VEH_PLAYER_NO_DISMANTLE)) {
				send_to_char(" (no-dismantle)", ch);
			}
		}
		send_to_char(".\r\n", ch);
	}
	
	if (VEH_PATRON(veh) && (index = find_player_index_by_idnum(VEH_PATRON(veh)))) {
		msg_to_char(ch, "It is dedicated to %s.\r\n", index->fullname);
	}
	
	if (VEH_PAINT_COLOR(veh)) {
		sprinttype(VEH_PAINT_COLOR(veh), paint_names, lbuf, sizeof(lbuf), "UNDEFINED");
		sprinttype(VEH_PAINT_COLOR(veh), paint_colors, colbuf, sizeof(colbuf), "&0");
		*lbuf = LOWER(*lbuf);
		if (VEH_FLAGGED(veh, VEH_BRIGHT_PAINT)) {
			strtoupper(buf1);
		}
		msg_to_char(ch, "It has been painted %s%s%s&0.\r\n", colbuf, (VEH_FLAGGED(veh, VEH_BRIGHT_PAINT) ? "bright " : ""), lbuf);
	}
	
	if (VEH_NEEDS_RESOURCES(veh)) {
		show_resource_list(VEH_NEEDS_RESOURCES(veh), lbuf);
		
		if (VEH_IS_COMPLETE(veh)) {
			msg_to_char(ch, "Maintenance needed: %s\r\n", lbuf);
		}
		else if (VEH_IS_DISMANTLING(veh)) {
			msg_to_char(ch, "Remaining to dismantle: %s\r\n", lbuf);
		}
		else {
			msg_to_char(ch, "Resources to completion: %s\r\n", lbuf);
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
	char buf[MAX_STRING_LENGTH*4], lbuf[MAX_STRING_LENGTH*4];
	struct custom_message *custm;
	struct spawn_info *spawn;
	int count;
	
	if (!veh) {
		return;
	}
	
	*buf = '\0';
	
	sprintf(buf + strlen(buf), "[%s%d\t0] %s%s\t0\r\n", OLC_LABEL_CHANGED, GET_OLC_VNUM(ch->desc), OLC_LABEL_UNCHANGED, !vehicle_proto(VEH_VNUM(veh)) ? "new vehicle" : VEH_SHORT_DESC(vehicle_proto(VEH_VNUM(veh))));

	sprintf(buf + strlen(buf), "<%skeywords\t0> %s\r\n", OLC_LABEL_STR(VEH_KEYWORDS(veh), default_vehicle_keywords), NULLSAFE(VEH_KEYWORDS(veh)));
	sprintf(buf + strlen(buf), "<%sshortdescription\t0> %s\r\n", OLC_LABEL_STR(VEH_SHORT_DESC(veh), default_vehicle_short_desc), NULLSAFE(VEH_SHORT_DESC(veh)));
	sprintf(buf + strlen(buf), "<%slongdescription\t0>\r\n%s\r\n", OLC_LABEL_STR(VEH_LONG_DESC(veh), default_vehicle_long_desc), NULLSAFE(VEH_LONG_DESC(veh)));
	sprintf(buf + strlen(buf), "<%slookdescription\t0>\r\n%s", OLC_LABEL_STR(VEH_LOOK_DESC(veh), ""), NULLSAFE(VEH_LOOK_DESC(veh)));
	sprintf(buf + strlen(buf), "<%sicon\t0> %s\t0 %s, ", OLC_LABEL_STR(VEH_ICON(veh), ""), VEH_ICON(veh) ? VEH_ICON(veh) : "none", VEH_ICON(veh) ? show_color_codes(VEH_ICON(veh)) : "");
	sprintf(buf + strlen(buf), "<%shalficon\t0> %s\t0 %s, ", OLC_LABEL_STR(VEH_HALF_ICON(veh), ""), VEH_HALF_ICON(veh) ? VEH_HALF_ICON(veh) : "none", VEH_HALF_ICON(veh) ? show_color_codes(VEH_HALF_ICON(veh)) : "");
	sprintf(buf + strlen(buf), "<%squartericon\t0> %s\t0 %s\r\n", OLC_LABEL_STR(VEH_QUARTER_ICON(veh), ""), VEH_QUARTER_ICON(veh) ? VEH_QUARTER_ICON(veh) : "none", VEH_QUARTER_ICON(veh) ? show_color_codes(VEH_QUARTER_ICON(veh)) : "");
	
	sprintbit(VEH_FLAGS(veh), vehicle_flags, lbuf, TRUE);
	sprintf(buf + strlen(buf), "<%sflags\t0> %s\r\n", OLC_LABEL_VAL(VEH_FLAGS(veh), NOBITS), lbuf);
	
	sprintf(buf + strlen(buf), "<%shitpoints\t0> %d\r\n", OLC_LABEL_VAL(VEH_MAX_HEALTH(veh), 1), VEH_MAX_HEALTH(veh));
	sprintf(buf + strlen(buf), "<%smovetype\t0> %s\r\n", OLC_LABEL_VAL(VEH_MOVE_TYPE(veh), 0), mob_move_types[VEH_MOVE_TYPE(veh)]);
	sprintf(buf + strlen(buf), "<%sspeed\t0> %s, <%ssize\t0> %d\r\n", OLC_LABEL_VAL(VEH_SPEED_BONUSES(veh), VSPEED_NORMAL), vehicle_speed_types[VEH_SPEED_BONUSES(veh)], OLC_LABEL_VAL(VEH_SIZE(veh), 0), VEH_SIZE(veh));
	sprintf(buf + strlen(buf), "<%scapacity\t0> %d item%s, <%sanimalsrequired\t0> %d\r\n", OLC_LABEL_VAL(VEH_CAPACITY(veh), 0), VEH_CAPACITY(veh), PLURAL(VEH_CAPACITY(veh)), OLC_LABEL_VAL(VEH_ANIMALS_REQUIRED(veh), 0), VEH_ANIMALS_REQUIRED(veh));
	
	if (VEH_MIN_SCALE_LEVEL(veh) > 0) {
		sprintf(lbuf, "<%sminlevel\t0> %d", OLC_LABEL_CHANGED, VEH_MIN_SCALE_LEVEL(veh));
	}
	else {
		sprintf(lbuf, "<%sminlevel\t0> none", OLC_LABEL_UNCHANGED);
	}
	if (VEH_MAX_SCALE_LEVEL(veh) > 0) {
		sprintf(buf + strlen(buf), "%s, <%smaxlevel\t0> %d\r\n", lbuf, OLC_LABEL_CHANGED, VEH_MAX_SCALE_LEVEL(veh));
	}
	else {
		sprintf(buf + strlen(buf), "%s, <%smaxlevel\t0> none\r\n", lbuf, OLC_LABEL_UNCHANGED);
	}

	sprintf(buf + strlen(buf), "<%sextrarooms\t0> %d, <%sinteriorroom\t0> %d - %s\r\n", OLC_LABEL_VAL(VEH_MAX_ROOMS(veh), 0), VEH_MAX_ROOMS(veh), OLC_LABEL_VAL(VEH_INTERIOR_ROOM_VNUM(veh), NOWHERE), VEH_INTERIOR_ROOM_VNUM(veh), building_proto(VEH_INTERIOR_ROOM_VNUM(veh)) ? GET_BLD_NAME(building_proto(VEH_INTERIOR_ROOM_VNUM(veh))) : "none");
	sprintbit(VEH_DESIGNATE_FLAGS(veh), designate_flags, lbuf, TRUE);
	sprintf(buf + strlen(buf), "<%sdesignate\t0> %s\r\n", OLC_LABEL_VAL(VEH_DESIGNATE_FLAGS(veh), NOBITS), lbuf);
	sprintf(buf + strlen(buf), "<%scitizens\t0> %d, <%sartisan\t0> [%d] %s\r\n", OLC_LABEL_VAL(VEH_CITIZENS(veh), 0), VEH_CITIZENS(veh), OLC_LABEL_VAL(VEH_ARTISAN(veh), NOTHING), VEH_ARTISAN(veh), VEH_ARTISAN(veh) == NOTHING ? "none" : get_mob_name_by_proto(VEH_ARTISAN(veh), FALSE));
	sprintf(buf + strlen(buf), "<%sheight\t0> %d, <%sfame\t0> %d, <%smilitary\t0> %d\r\n", OLC_LABEL_VAL(VEH_HEIGHT(veh), 0), VEH_HEIGHT(veh), OLC_LABEL_VAL(VEH_FAME(veh), 0), VEH_FAME(veh), OLC_LABEL_VAL(VEH_MILITARY(veh), 0), VEH_MILITARY(veh));
	
	sprintbit(VEH_ROOM_AFFECTS(veh), room_aff_bits, lbuf, TRUE);
	sprintf(buf + strlen(buf), "<%saffects\t0> %s\r\n", OLC_LABEL_VAL(VEH_ROOM_AFFECTS(veh), NOBITS), lbuf);
	
	sprintbit(VEH_FUNCTIONS(veh), function_flags, lbuf, TRUE);
	sprintf(buf + strlen(buf), "<%sfunctions\t0> %s\r\n", OLC_LABEL_VAL(VEH_FUNCTIONS(veh), NOBITS), lbuf);
	
	ordered_sprintbit(VEH_REQUIRES_CLIMATE(veh), climate_flags, climate_flags_order, FALSE, lbuf);
	sprintf(buf + strlen(buf), "<%srequiresclimate\t0> %s\r\n", OLC_LABEL_VAL(VEH_REQUIRES_CLIMATE(veh), NOBITS), lbuf);
	ordered_sprintbit(VEH_FORBID_CLIMATE(veh), climate_flags, climate_flags_order, FALSE, lbuf);
	sprintf(buf + strlen(buf), "<%sforbidclimate\t0> %s\r\n", OLC_LABEL_VAL(VEH_FORBID_CLIMATE(veh), NOBITS), lbuf);
	
	// exdesc
	sprintf(buf + strlen(buf), "Extra descriptions: <%sextra\t0>\r\n", OLC_LABEL_PTR(VEH_EX_DESCS(veh)));
	if (VEH_EX_DESCS(veh)) {
		get_extra_desc_display(VEH_EX_DESCS(veh), lbuf, sizeof(lbuf));
		strcat(buf, lbuf);
	}

	sprintf(buf + strlen(buf), "Interactions: <%sinteraction\t0>\r\n", OLC_LABEL_PTR(VEH_INTERACTIONS(veh)));
	if (VEH_INTERACTIONS(veh)) {
		get_interaction_display(VEH_INTERACTIONS(veh), lbuf);
		strcat(buf, lbuf);
	}
	
	sprintf(buf + strlen(buf), "Relationships: <%srelations\t0>\r\n", OLC_LABEL_PTR(VEH_RELATIONS(veh)));
	if (VEH_RELATIONS(veh)) {
		get_bld_relations_display(VEH_RELATIONS(veh), lbuf);
		strcat(buf, lbuf);
	}
	
	// maintenance resources
	sprintf(buf + strlen(buf), "Regular maintenance resources: <%sresource\t0>\r\n", OLC_LABEL_PTR(VEH_REGULAR_MAINTENANCE(veh)));
	if (VEH_REGULAR_MAINTENANCE(veh)) {
		get_resource_display(ch, VEH_REGULAR_MAINTENANCE(veh), lbuf);
		strcat(buf, lbuf);
	}
	
	// custom messages
	sprintf(buf + strlen(buf), "Custom messages: <%scustom\t0>\r\n", OLC_LABEL_PTR(VEH_CUSTOM_MSGS(veh)));
	count = 0;
	LL_FOREACH(VEH_CUSTOM_MSGS(veh), custm) {
		sprintf(buf + strlen(buf), " \ty%2d\t0. [%s] %s\r\n", ++count, veh_custom_types[custm->type], custm->msg);
	}
	
	// scripts
	sprintf(buf + strlen(buf), "Scripts: <%sscript\t0>\r\n", OLC_LABEL_PTR(veh->proto_script));
	if (veh->proto_script) {
		get_script_display(veh->proto_script, lbuf);
		strcat(buf, lbuf);
	}
	
	// spawns
	sprintf(buf + strlen(buf), "<%sspawns\t0>\r\n", OLC_LABEL_PTR(VEH_SPAWNS(veh)));
	if (VEH_SPAWNS(veh)) {
		count = 0;
		LL_FOREACH(VEH_SPAWNS(veh), spawn) {
			++count;
		}
		sprintf(buf + strlen(buf), " %d spawn%s set\r\n", count, PLURAL(count));
	}
	
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

OLC_MODULE(vedit_affects) {
	vehicle_data *veh = GET_OLC_VEHICLE(ch->desc);
	VEH_ROOM_AFFECTS(veh) = olc_process_flag(ch, argument, "affect", "affects", room_aff_bits, VEH_ROOM_AFFECTS(veh));
}


OLC_MODULE(vedit_animalsrequired) {
	vehicle_data *veh = GET_OLC_VEHICLE(ch->desc);
	VEH_ANIMALS_REQUIRED(veh) = olc_process_number(ch, argument, "animals required", "animalsrequired", 0, 100, VEH_ANIMALS_REQUIRED(veh));
}


OLC_MODULE(vedit_artisan) {
	vehicle_data *veh = GET_OLC_VEHICLE(ch->desc);
	mob_vnum old = VEH_ARTISAN(veh);
	
	if (!str_cmp(argument, "none")) {
		VEH_ARTISAN(veh) = NOTHING;
		if (PRF_FLAGGED(ch, PRF_NOREPEAT)) {
			send_config_msg(ch, "ok_string");
		}
		else {
			msg_to_char(ch, "It no longer has an artisan.\r\n");
		}
	}
	else {
		VEH_ARTISAN(veh) = olc_process_number(ch, argument, "artisan vnum", "artisanvnum", 0, MAX_VNUM, VEH_ARTISAN(veh));
		if (!mob_proto(VEH_ARTISAN(veh))) {
			VEH_ARTISAN(veh) = old;
			msg_to_char(ch, "There is no mobile with that vnum. Old value restored.\r\n");
		}
		else if (!PRF_FLAGGED(ch, PRF_NOREPEAT)) {
			msg_to_char(ch, "It now has artisan: %s\r\n", get_mob_name_by_proto(VEH_ARTISAN(veh), FALSE));
		}
	}
}


OLC_MODULE(vedit_capacity) {
	vehicle_data *veh = GET_OLC_VEHICLE(ch->desc);
	VEH_CAPACITY(veh) = olc_process_number(ch, argument, "capacity", "capacity", 0, 10000, VEH_CAPACITY(veh));
}


OLC_MODULE(vedit_citizens) {
	vehicle_data *veh = GET_OLC_VEHICLE(ch->desc);
	VEH_CITIZENS(veh) = olc_process_number(ch, argument, "citizens", "citizens", 0, 10, VEH_CITIZENS(veh));
}


OLC_MODULE(vedit_custom) {
	vehicle_data *veh = GET_OLC_VEHICLE(ch->desc);
	
	olc_process_custom_messages(ch, argument, &VEH_CUSTOM_MSGS(veh), veh_custom_types, NULL);
}


OLC_MODULE(vedit_speed) {
	// TODO: move this into alphabetic order on some future major version
	vehicle_data *veh = GET_OLC_VEHICLE(ch->desc);
	VEH_SPEED_BONUSES(veh) = olc_process_type(ch, argument, "speed", "speed", vehicle_speed_types, VEH_SPEED_BONUSES(veh));
}


OLC_MODULE(vedit_designate) {
	vehicle_data *veh = GET_OLC_VEHICLE(ch->desc);
	VEH_DESIGNATE_FLAGS(veh) = olc_process_flag(ch, argument, "designate", "designate", designate_flags, VEH_DESIGNATE_FLAGS(veh));
}


OLC_MODULE(vedit_extra_desc) {
	vehicle_data *veh = GET_OLC_VEHICLE(ch->desc);
	olc_process_extra_desc(ch, argument, &VEH_EX_DESCS(veh));
}


OLC_MODULE(vedit_extrarooms) {
	vehicle_data *veh = GET_OLC_VEHICLE(ch->desc);
	VEH_MAX_ROOMS(veh) = olc_process_number(ch, argument, "max rooms", "maxrooms", 0, 1000, VEH_MAX_ROOMS(veh));
}


OLC_MODULE(vedit_fame) {
	vehicle_data *veh = GET_OLC_VEHICLE(ch->desc);
	VEH_FAME(veh) = olc_process_number(ch, argument, "fame", "fame", -1000, 1000, VEH_FAME(veh));
}


OLC_MODULE(vedit_flags) {
	vehicle_data *veh = GET_OLC_VEHICLE(ch->desc);
	VEH_FLAGS(veh) = olc_process_flag(ch, argument, "vehicle", "flags", vehicle_flags, VEH_FLAGS(veh));
}


OLC_MODULE(vedit_forbidclimate) {
	vehicle_data *veh = GET_OLC_VEHICLE(ch->desc);
	VEH_FORBID_CLIMATE(veh) = olc_process_flag(ch, argument, "climate", "forbidclimate", climate_flags, VEH_FORBID_CLIMATE(veh));
}


OLC_MODULE(vedit_functions) {
	vehicle_data *veh = GET_OLC_VEHICLE(ch->desc);
	VEH_FUNCTIONS(veh) = olc_process_flag(ch, argument, "function", "functions", function_flags, VEH_FUNCTIONS(veh));
}


OLC_MODULE(vedit_half_icon) {
	vehicle_data *veh = GET_OLC_VEHICLE(ch->desc);
	
	delete_doubledollar(argument);
	
	if (!str_cmp(argument, "none")) {
		if (VEH_HALF_ICON(veh)) {
			free(VEH_HALF_ICON(veh));
		}
		VEH_HALF_ICON(veh) = NULL;
		msg_to_char(ch, "The vehicle now has no half icon and will not appear on the map.\r\n");
	}
	else if (!validate_icon(argument, 2)) {
		msg_to_char(ch, "You must specify a half icon that is 2 characters long, not counting color codes.\r\n");
	}
	else {
		olc_process_string(ch, argument, "halficon", &VEH_HALF_ICON(veh));
		msg_to_char(ch, "\t0");	// in case color is unterminated
	}
}


OLC_MODULE(vedit_height) {
	vehicle_data *veh = GET_OLC_VEHICLE(ch->desc);
	VEH_HEIGHT(veh) = olc_process_number(ch, argument, "height", "height", -100, 100, VEH_HEIGHT(veh));
}


OLC_MODULE(vedit_hitpoints) {
	vehicle_data *veh = GET_OLC_VEHICLE(ch->desc);
	VEH_MAX_HEALTH(veh) = olc_process_number(ch, argument, "hitpoints", "hitpoints", 1, 1000, VEH_MAX_HEALTH(veh));
}


OLC_MODULE(vedit_icon) {
	vehicle_data *veh = GET_OLC_VEHICLE(ch->desc);
	
	delete_doubledollar(argument);
	
	if (!str_cmp(argument, "none")) {
		if (VEH_ICON(veh)) {
			free(VEH_ICON(veh));
		}
		VEH_ICON(veh) = NULL;
		msg_to_char(ch, "The vehicle now has no icon and will not appear on the map.\r\n");
	}
	else if (!validate_icon(argument, 4)) {
		msg_to_char(ch, "You must specify an icon that is 4 characters long, not counting color codes.\r\n");
	}
	else {
		olc_process_string(ch, argument, "icon", &VEH_ICON(veh));
		msg_to_char(ch, "\t0");	// in case color is unterminated
	}
}


OLC_MODULE(vedit_interaction) {
	vehicle_data *veh = GET_OLC_VEHICLE(ch->desc);
	olc_process_interactions(ch, argument, &VEH_INTERACTIONS(veh), TYPE_ROOM);
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
		start_string_editor(ch->desc, buf, &VEH_LOOK_DESC(veh), MAX_ITEM_DESCRIPTION, TRUE);
	}
}


OLC_MODULE(vedit_maxlevel) {
	vehicle_data *veh = GET_OLC_VEHICLE(ch->desc);
	VEH_MAX_SCALE_LEVEL(veh) = olc_process_number(ch, argument, "maximum level", "maxlevel", 0, MAX_INT, VEH_MAX_SCALE_LEVEL(veh));
}


OLC_MODULE(vedit_military) {
	vehicle_data *veh = GET_OLC_VEHICLE(ch->desc);
	VEH_MILITARY(veh) = olc_process_number(ch, argument, "military", "military", 0, 1000, VEH_MILITARY(veh));
}


OLC_MODULE(vedit_minlevel) {
	vehicle_data *veh = GET_OLC_VEHICLE(ch->desc);
	VEH_MIN_SCALE_LEVEL(veh) = olc_process_number(ch, argument, "minimum level", "minlevel", 0, MAX_INT, VEH_MIN_SCALE_LEVEL(veh));
}


OLC_MODULE(vedit_movetype) {
	vehicle_data *veh = GET_OLC_VEHICLE(ch->desc);
	VEH_MOVE_TYPE(veh) = olc_process_type(ch, argument, "move type", "movetype", mob_move_types, VEH_MOVE_TYPE(veh));
}


OLC_MODULE(vedit_quarter_icon) {
	vehicle_data *veh = GET_OLC_VEHICLE(ch->desc);
	
	delete_doubledollar(argument);
	
	if (!str_cmp(argument, "none")) {
		if (VEH_QUARTER_ICON(veh)) {
			free(VEH_QUARTER_ICON(veh));
		}
		VEH_QUARTER_ICON(veh) = NULL;
		msg_to_char(ch, "The vehicle now has no quarter icon and will not appear on the map.\r\n");
	}
	else if (!validate_icon(argument, 1)) {
		msg_to_char(ch, "You must specify a quarter icon that is 1 character long, not counting color codes.\r\n");
	}
	else {
		olc_process_string(ch, argument, "quartericon", &VEH_QUARTER_ICON(veh));
		msg_to_char(ch, "\t0");	// in case color is unterminated
	}
}


OLC_MODULE(vedit_relations) {
	vehicle_data *veh = GET_OLC_VEHICLE(ch->desc);
	olc_process_relations(ch, argument, &VEH_RELATIONS(veh));
}


OLC_MODULE(vedit_requiresclimate) {
	vehicle_data *veh = GET_OLC_VEHICLE(ch->desc);
	VEH_REQUIRES_CLIMATE(veh) = olc_process_flag(ch, argument, "climate", "requiresclimate", climate_flags, VEH_REQUIRES_CLIMATE(veh));
}


OLC_MODULE(vedit_resource) {
	vehicle_data *veh = GET_OLC_VEHICLE(ch->desc);
	olc_process_resources(ch, argument, &VEH_REGULAR_MAINTENANCE(veh));
}


OLC_MODULE(vedit_script) {
	vehicle_data *veh = GET_OLC_VEHICLE(ch->desc);
	olc_process_script(ch, argument, &(veh->proto_script), VEH_TRIGGER);
}


OLC_MODULE(vedit_shortdescription) {
	vehicle_data *veh = GET_OLC_VEHICLE(ch->desc);
	olc_process_string(ch, argument, "short description", &VEH_SHORT_DESC(veh));
}


OLC_MODULE(vedit_size) {
	vehicle_data *veh = GET_OLC_VEHICLE(ch->desc);
	VEH_SIZE(veh) = olc_process_number(ch, argument, "size", "size", 0, config_get_int("vehicle_size_per_tile"), VEH_SIZE(veh));
}


OLC_MODULE(vedit_spawns) {
	vehicle_data *veh = GET_OLC_VEHICLE(ch->desc);
	olc_process_spawns(ch, argument, &VEH_SPAWNS(veh));
}
