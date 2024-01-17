/* ************************************************************************
*   File: building.c                                      EmpireMUD 2.0b5 *
*  Usage: Miscellaneous player-level commands                             *
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
#include "comm.h"
#include "interpreter.h"
#include "handler.h"
#include "db.h"
#include "skills.h"
#include "vnums.h"
#include "dg_scripts.h"
#include "constants.h"

/**
* Contents:
*   Building Data
*   Special Handling
*   Building Helpers
*   Main Building Commands
*   Additional Commands
*/

// local prototypes
void setup_tunnel_entrance(char_data *ch, room_data *room, int dir);
bool start_upgrade(char_data *ch, craft_data *upgrade_craft, room_data *from_room, vehicle_data *from_veh, bld_data *to_building, vehicle_data *to_veh);


 //////////////////////////////////////////////////////////////////////////////
//// BUILDING DATA ///////////////////////////////////////////////////////////

// these correspond to digits 0-9, must be 2 chars each, and \n-terminated
const char *interlink_codes[11] = { "AX", "RB",	"UN", "DD", "WZ", "FG", "VI", "QP", "MY", "XE", "\n" };


 //////////////////////////////////////////////////////////////////////////////
//// SPECIAL HANDLING ////////////////////////////////////////////////////////

/**
* Any special handling when a new building is set up (at start of build):
*
* @param char_data *ch The builder (OPTIONAL: for skill setup)
* @param room_data *room The location to set up.
*/
void special_building_setup(char_data *ch, room_data *room) {
	empire_data *emp = ROOM_OWNER(room) ? ROOM_OWNER(room) : (ch ? GET_LOYALTY(ch) : NULL);
	
	// mine data
	if (room_has_function_and_city_ok(emp, room, FNC_MINE)) {
		init_mine(room, ch, emp);
	}
}


/**
* Any special handling room handling when a new vehicle is set up
* (at start of build):
*
* @param char_data *ch The builder (OPTIONAL: for skill setup)
* @param vehicle_data *veh The vehicle to set up.
*/
void special_vehicle_setup(char_data *ch, vehicle_data *veh) {
	room_data *room = IN_ROOM(veh);
	empire_data *emp = ROOM_OWNER(room) ? ROOM_OWNER(room) : (VEH_OWNER(veh) ? VEH_OWNER(veh) : (ch ? GET_LOYALTY(ch) : NULL));
	
	// mine data
	if (IS_SET(VEH_FUNCTIONS(veh), FNC_MINE)) {
		init_mine(room, ch, emp);
	}
}


 //////////////////////////////////////////////////////////////////////////////
//// BUILDING HELPERS ////////////////////////////////////////////////////////

/**
* @param room_data *room The map location to check
* @param bitvector_t flags BLD_ON_
* @return TRUE if valid, FALSE if not
*/
bool can_build_on(room_data *room, bitvector_t flags) {
	#define CLEAR_OPEN_BUILDING(r)	(IS_MAP_BUILDING(r) && ROOM_BLD_FLAGGED((r), BLD_OPEN) && !ROOM_BLD_FLAGGED((r), BLD_BARRIER) && (IS_COMPLETE(r) || !SECT_FLAGGED(BASE_SECT(r), SECTF_FRESH_WATER | SECTF_OCEAN)))
	#define IS_PLAYER_MADE(r)  (GET_ROOM_VNUM(r) < MAP_SIZE && SECT(r) != world_map[FLAT_X_COORD(r)][FLAT_Y_COORD(r)].natural_sector)

	return (!IS_SET(flags, BLD_ON_NOT_PLAYER_MADE) || !IS_PLAYER_MADE(room)) && (
		IS_SET(GET_SECT_BUILD_FLAGS(SECT(room)), flags) || 
		(IS_SET(flags, BLD_ON_BASE_TERRAIN_ALLOWED) && IS_SET(GET_SECT_BUILD_FLAGS(BASE_SECT(room)), flags)) ||
		(IS_SET(flags, BLD_FACING_OPEN_BUILDING) && CLEAR_OPEN_BUILDING(room))
	);
}


/**
* Checks the location and direction the player is trying to craft/build
* a map building or a building-vehicle. This function sends error messages if
* it fails.
*
* @param char_data *ch Optional: The person trying to build/craft (may be NULL if unavailable or no messages need to be sent).
* @param room_data *room The location for the building.
* @param craft_data *type A craft that makes a building or vehicle-building.
* @param int dir The direction that the character provided (may be NO_DIR or an invalid dir).
* @param bool is_upgrade If TRUE, skips/modifies some of the checks.
* @param bool *bld_is_closed A variable to bind whether or not the building is a 'closed' one (may be NULL).
* @param bool *bld_needs_reverse A variable to bind whether or not there needs to be a reverse exit (may be NULL).
* @return bool TRUE if it's ok to proceed; FALSE it if sent an error message and failed.
*/
bool check_build_location_and_dir(char_data *ch, room_data *room, craft_data *type, int dir, bool is_upgrade, bool *bld_is_closed, bool *bld_needs_reverse) {
	bool is_closed, needs_facing, needs_reverse;
	room_data *to_room = NULL, *to_rev = NULL;
	vehicle_data *make_veh = NULL, *veh_iter;
	char buf[MAX_STRING_LENGTH];
	bld_data *to_build = NULL;
	
	char *command = gen_craft_data[GET_CRAFT_TYPE(type)].command;
	
	// init vars
	if (bld_is_closed) {
		*bld_is_closed = FALSE;
	}
	if (bld_needs_reverse) {
		*bld_needs_reverse = FALSE;
	}
	
	// ensure we have something to make
	if (CRAFT_IS_VEHICLE(type) && !(make_veh = vehicle_proto(GET_CRAFT_OBJECT(type)))) {
		if (ch) {
			msg_to_char(ch, "That is not yet implemented.\r\n");
		}
		return FALSE;
	}
	if (CRAFT_IS_BUILDING(type) && !(to_build = building_proto(GET_CRAFT_BUILD_TYPE(type)))) {
		if (ch) {
			msg_to_char(ch, "That is not yet implemented.\r\n");
		}
		return FALSE;
	}
	if (!make_veh && !to_build) {
		// not trying to make a vehicle or building? no need for this
		return TRUE;
	}
	
	// setup
	is_closed = to_build && !IS_SET(GET_BLD_FLAGS(to_build), BLD_OPEN);
	needs_facing = (GET_CRAFT_BUILD_FACING(type) != NOBITS);
	needs_reverse = needs_facing && to_build && IS_SET(GET_BLD_FLAGS(to_build), BLD_TWO_ENTRANCES);
	
	// checks
	if (!is_upgrade && to_build && GET_BUILDING(room)) {
		if (ch) {
			msg_to_char(ch, "You can't %s that here.\r\n", command);
		}
		return FALSE;
	}
	if (!is_upgrade && (is_closed || needs_facing) && (GET_ROOM_VNUM(room) >= MAP_SIZE || !IS_OUTDOOR_TILE(room))) {
		if (ch) {
			msg_to_char(ch, "You can only %s that out on the map.\r\n", command);
		}
		return FALSE;
	}
	if (GET_CRAFT_BUILD_ON(type) && !can_build_on(room, GET_CRAFT_BUILD_ON(type) | (is_upgrade ? BLD_ON_BASE_TERRAIN_ALLOWED : NOBITS))) {
		if (ch) {
			ordered_sprintbit(GET_CRAFT_BUILD_ON(type), bld_on_flags, bld_on_flags_order, TRUE, buf);
			msg_to_char(ch, "You need to %s %s on: %s\r\n", command, GET_CRAFT_NAME(type), buf);
		}
		return FALSE;
	}
	if (!is_upgrade && to_build && !GET_CRAFT_BUILD_ON(type)) {
		if (ch) {
			msg_to_char(ch, "You can't %s that anywhere.\r\n", command);
		}
		return FALSE;
	}
	if ((is_closed || (to_build && IS_SET(GET_BLD_FLAGS(to_build), BLD_BARRIER))) && is_entrance(room)) {
		if (ch) {
			msg_to_char(ch, "You can't %s that in front of a building entrance.\r\n", command);
		}
		return FALSE;
	}
	
	// vehicle size/location checks
	if (!is_upgrade && make_veh && VEH_FLAGGED(make_veh, VEH_NO_BUILDING) && (ROOM_IS_CLOSED(room) || (GET_BUILDING(room) && !ROOM_BLD_FLAGGED(room, BLD_OPEN)))) {
		if (ch) {
			msg_to_char(ch, "You can't %s that in a building.\r\n", command);
		}
		return FALSE;
	}
	if (!is_upgrade && make_veh && GET_ROOM_VEHICLE(room) && (VEH_FLAGGED(make_veh, VEH_NO_LOAD_ONTO_VEHICLE) || !VEH_FLAGGED(GET_ROOM_VEHICLE(room), VEH_CARRY_VEHICLES))) {
		if (ch) {
			msg_to_char(ch, "You can't %s that in here.\r\n", command);
		}
		return FALSE;
	}
	if (make_veh && VEH_SIZE(make_veh) > 0 && total_vehicle_size_in_room(room, NULL) + VEH_SIZE(make_veh) > config_get_int("vehicle_size_per_tile")) {
		if (ch) {
			msg_to_char(ch, "This area is already too full to %s that.\r\n", command);
		}
		return FALSE;
	}
	if (make_veh && VEH_SIZE(make_veh) == 0 && total_small_vehicles_in_room(room, NULL) >= config_get_int("vehicle_max_per_tile")) {
		if (ch) {
			msg_to_char(ch, "This area is already too full to %s that.\r\n", command);
		}
		return FALSE;
	}
	if (make_veh && (is_upgrade || !ROOM_IS_CLOSED(room)) && !vehicle_allows_climate(make_veh, room, NULL)) {
		if (ch) {
			msg_to_char(ch, "You can't %s %s here.\r\n", command, get_vehicle_short_desc(make_veh, ch));
		}
		return FALSE;
	}
	
	// buildings around vehicles
	if (to_build && !IS_SET(GET_BLD_FLAGS(to_build), BLD_OPEN)) {
		DL_FOREACH2(ROOM_VEHICLES(room), veh_iter, next_in_room) {
			if (VEH_FLAGGED(veh_iter, VEH_NO_BUILDING)) {
				if (ch) {
					sprintf(buf, "You can't %s that around $V.", command);
					act(buf, FALSE, ch, NULL, veh_iter, TO_CHAR | ACT_VEH_VICT);
				}
				return FALSE;
			}
		}
	}
	
	// check facing dirs
	if (needs_facing) {
		if (dir == NO_DIR) {
			if (ch) {
				msg_to_char(ch, "Which direction would you like to %s it facing?\r\n", command);
			}
			return FALSE;
		}
		if (dir >= NUM_2D_DIRS) {
			if (ch) {
				msg_to_char(ch, "You can't face it that direction.\r\n");
			}
			return FALSE;
		}
		if (!(to_room = real_shift(room, shift_dir[dir][0], shift_dir[dir][1]))) {
			if (ch) {
				msg_to_char(ch, "You can't face it that direction.\r\n");
			}
			return FALSE;
		}
		if (!can_build_on(to_room, GET_CRAFT_BUILD_FACING(type))) {
			if (ch) {
				ordered_sprintbit(GET_CRAFT_BUILD_FACING(type), bld_on_flags, bld_on_flags_order, TRUE, buf);
				msg_to_char(ch, "You need to %s %s facing: %s\r\n", command, GET_CRAFT_NAME(type), buf);
			}
			return FALSE;
		}
	}	// end facing checks
	
	if (needs_reverse) {
		to_rev = real_shift(room, shift_dir[rev_dir[dir]][0], shift_dir[rev_dir[dir]][1]);

		if (!to_rev || !can_build_on(to_rev, GET_CRAFT_BUILD_FACING(type))) {
			if (ch) {
				ordered_sprintbit(GET_CRAFT_BUILD_FACING(type), bld_on_flags, bld_on_flags_order, TRUE, buf);
				msg_to_char(ch, "You need to %s it with the reverse side facing: %s\r\n", command, buf);
			}
			return FALSE;
		}
	}
	
	// bind those variables to pass back
	if (bld_is_closed) {
		*bld_is_closed = is_closed;
	}
	if (bld_needs_reverse) {
		*bld_needs_reverse = needs_reverse;
	}
	
	// if we got this far, it should be ok
	return TRUE;
}


/**
* This creates a resource list that is a merged copy of two lists. You will
* need to free_resource_list() on the result when done with it.
*
* @param struct resource_data *combine_a One list of resources.
* @param struct resource_data *combine_from Another list of resources.
* @return struct resource_data* The copied/merged list.
*/
struct resource_data *combine_resources(struct resource_data *combine_a, struct resource_data *combine_b) {
	struct resource_data *list, *iter;
	
	// start with a copy of 'a'
	list = copy_resource_list(combine_a);
	
	// add in each entry from 'b' for a clean combine
	LL_FOREACH(combine_b, iter) {
		add_to_resource_list(&list, iter->type, iter->vnum, iter->amount, iter->misc);
	}
	
	return list;
}


/**
* This function handles data for the final completion of a building on a
* map tile. It removes the to_build resources and.
*
* @param room_data *room The room to complete.
*/
void complete_building(room_data *room) {
	char_data *ch;
	empire_data *emp;
	
	// nothing to do if no building data
	if (!COMPLEX_DATA(room)) {
		return;
	}
	
	// stop builders
	stop_room_action(room, ACT_BUILDING);
	stop_room_action(room, ACT_MAINTENANCE);
	
	// remove any remaining resource requirements
	free_resource_list(GET_BUILDING_RESOURCES(room));
	GET_BUILDING_RESOURCES(room) = NULL;
	
	// ensure no damage (locally, not home-room)
	set_room_damage(room, 0);
	
	// remove incomplete
	REMOVE_BIT(ROOM_BASE_FLAGS(room), ROOM_AFF_INCOMPLETE);
	affect_total_room(room);
	
	complete_wtrigger(room);
	
	// check mounted people
	if (!BLD_ALLOWS_MOUNTS(room)) {
		DL_FOREACH2(ROOM_PEOPLE(room), ch, next_in_room) {
			if (IS_RIDING(ch)) {
				msg_to_char(ch, "You jump down from your mount.\r\n");
				act("$n jumps down from $s mount.", TRUE, ch, NULL, NULL, TO_ROOM);
				perform_dismount(ch);
			}
		}
	}
	
	herd_animals_out(room);
	
	// final setup
	if ((emp = ROOM_OWNER(room))) {
		adjust_building_tech(emp, room, TRUE);
		
		if (GET_BUILDING(room)) {
			qt_empire_players(emp, qt_gain_building, GET_BLD_VNUM(GET_BUILDING(room)));
			et_gain_building(emp, GET_BLD_VNUM(GET_BUILDING(room)));
		}
	}
	
	affect_total_room(room);
	request_mapout_update(GET_ROOM_VNUM(room));
	request_world_save(GET_ROOM_VNUM(room), WSAVE_ROOM);
}


/**
* Sets up building data for a map location.
*
* @param room_data *room Where to set up the building.
* @param bld_vnum type The building vnum to set up.
*/
void construct_building(room_data *room, bld_vnum type) {
	bool was_large, junk;
	int was_ter, is_ter;
	sector_data *sect;
	
	if (GET_ROOM_VNUM(room) >= MAP_SIZE) {
		log("SYSERR: construct_building called on location off the map");
		return;
	}
	
	disassociate_building(room);	// just in case
	
	// for updating territory counts
	was_large = LARGE_CITY_RADIUS(room);
	was_ter = ROOM_OWNER(room) ? get_territory_type_for_empire(room, ROOM_OWNER(room), FALSE, &junk, NULL) : TER_FRONTIER;
	
	sect = SECT(room);
	change_terrain(room, config_get_int("default_building_sect"), NOTHING);
	change_base_sector(room, sect);
	
	// set actual data
	attach_building_to_room(building_proto(type), room, TRUE);
	
	// check for territory updates
	if (ROOM_OWNER(room) && was_large != LARGE_CITY_RADIUS(room)) {
		struct empire_island *eisle = get_empire_island(ROOM_OWNER(room), GET_ISLAND_ID(room));
		is_ter = get_territory_type_for_empire(room, ROOM_OWNER(room), FALSE, &junk, NULL);
		
		if (was_ter != is_ter) {	// did territory type change?
			SAFE_ADD(EMPIRE_TERRITORY(ROOM_OWNER(room), was_ter), -1, 0, UINT_MAX, FALSE);
			SAFE_ADD(eisle->territory[was_ter], -1, 0, UINT_MAX, FALSE);
			
			SAFE_ADD(EMPIRE_TERRITORY(ROOM_OWNER(room), is_ter), 1, 0, UINT_MAX, FALSE);
			SAFE_ADD(eisle->territory[is_ter], 1, 0, UINT_MAX, FALSE);
			
			// (total territory does not change)
		}
	}
	
	if (ROOM_OWNER(room)) {
		deactivate_workforce_room(ROOM_OWNER(room), room);
	}
	
	load_wtrigger(room);
	request_mapout_update(GET_ROOM_VNUM(room));
	request_world_save(GET_ROOM_VNUM(room), WSAVE_ROOM);
}


/**
* do_tunnel passes off control here when the whole thing has been validated;
* this function just builds the tunnel data and updates the map, etc
*
* @param char_data *ch the builder
* @param int dir which way the tunnel goes
* @param room_data *entrance The first room of the tunnel (adjacent to ch)
* @param room_data *exit The last room of the tunnel
* @param int length The number of intervening rooms to add
*/
void construct_tunnel(char_data *ch, int dir, room_data *entrance, room_data *exit, int length) {
	static struct resource_data *resources = NULL;
	
	room_data *new_room, *last_room = entrance, *to_room;
	int iter;
	
	if (!resources) {
		add_to_resource_list(&resources, RES_COMPONENT, COMP_PILLAR, 12, 0);
		add_to_resource_list(&resources, RES_COMPONENT, COMP_LUMBER, 8, 0);
		add_to_resource_list(&resources, RES_COMPONENT, COMP_NAILS, 4, 0);
	}
	
	// entrance
	setup_tunnel_entrance(ch, entrance, dir);
	GET_BUILDING_RESOURCES(entrance) = copy_resource_list(resources);
	SET_BIT(ROOM_BASE_FLAGS(entrance), ROOM_AFF_INCOMPLETE);
	affect_total_room(entrance);
	create_exit(entrance, IN_ROOM(ch), rev_dir[dir], FALSE);

	// exit
	setup_tunnel_entrance(ch, exit, rev_dir[dir]);
	GET_BUILDING_RESOURCES(exit) = copy_resource_list(resources);
	SET_BIT(ROOM_BASE_FLAGS(exit), ROOM_AFF_INCOMPLETE);
	affect_total_room(exit);
	to_room = real_shift(exit, shift_dir[dir][0], shift_dir[dir][1]);
	create_exit(exit, to_room, dir, FALSE);

	// now the length of the tunnel
	for (iter = 0; iter < length; ++iter) {
		new_room = create_room((iter <= length/2) ? entrance : exit);
		attach_building_to_room(building_proto(RTYPE_TUNNEL), new_room, TRUE);
		GET_BUILDING_RESOURCES(new_room) = copy_resource_list(resources);
		SET_BIT(ROOM_BASE_FLAGS(new_room), ROOM_AFF_INCOMPLETE);
		affect_total_room(new_room);

		create_exit(last_room, new_room, dir, TRUE);
		
		// set ownership?
		if (iter <= length/2 && ROOM_OWNER(entrance)) {
			perform_claim_room(new_room, ROOM_OWNER(entrance));
		}
		else if (iter > length/2 && ROOM_OWNER(exit)) {
			perform_claim_room(new_room, ROOM_OWNER(exit));
		}
		
		// save for next cycle
		last_room = new_room;
	}
	
	// link the final tunnel room
	create_exit(last_room, exit, dir, TRUE);
}


/**
* Determines how many players are inside a building.
*
* @param room_data *loc The location of the building to check.
* @param bool ignore_home_room If TRUE, doesn't count players in the home-room, only ones deeper inside.
* @param bool ignore_invis_imms If TRUE, does not count immortals who can't be seen by players.
* @return int How many players were inside.
*/
int count_players_in_building(room_data *room, bool ignore_home_room, bool ignore_invis_imms) {
	room_data *interior;
	vehicle_data *viter;
	char_data *iter;
	int count = 0;
	
	room = HOME_ROOM(room);
	
	// home room
	if (!ignore_home_room) {
		DL_FOREACH2(ROOM_PEOPLE(room), iter, next_in_room) {
			if (IS_NPC(iter)) {
				continue;
			}
			if (ignore_invis_imms && IS_IMMORTAL(iter) && (GET_INVIS_LEV(iter) > 1 || PRF_FLAGGED(iter, PRF_WIZHIDE))) {
				continue;
			}
			++count;
		}
		DL_FOREACH2(ROOM_VEHICLES(room), viter, next_in_room) {
			count += count_players_in_vehicle(viter, ignore_invis_imms);
		}
	}
	
	// interior rooms
	DL_FOREACH2(interior_room_list, interior, next_interior) {
		if (HOME_ROOM(interior) == room) {
			DL_FOREACH2(ROOM_PEOPLE(interior), iter, next_in_room) {
				if (IS_NPC(iter)) {
					continue;
				}
				if (ignore_invis_imms && IS_IMMORTAL(iter) && (GET_INVIS_LEV(iter) > 1 || PRF_FLAGGED(iter, PRF_WIZHIDE))) {
					continue;
				}
		
				++count;
			}
	
			// check nested vehicles
			DL_FOREACH2(ROOM_VEHICLES(interior), viter, next_in_room) {
				count += count_players_in_vehicle(viter, ignore_invis_imms);
			}
		}
	}
	
	return count;
}


/**
* Function to tear down a map tile. This restores it to nature in every
* regard EXCEPT owner. Mines are also not reset this way.
*
* @param room_data *room The map room to disassociate.
*/
void disassociate_building(room_data *room) {
	sector_data *old_sect = SECT(room);
	bool was_large, junk;
	room_data *iter, *next_iter;
	struct instance_data *inst;
	bool deleted = FALSE;
	int was_ter, is_ter;
	char_data *temp_ch;
	
	// for updating territory counts
	was_large = ROOM_BLD_FLAGGED(room, BLD_LARGE_CITY_RADIUS) ? TRUE : FALSE;
	was_ter = ROOM_OWNER(room) ? get_territory_type_for_empire(room, ROOM_OWNER(room), FALSE, &junk, NULL) : TER_FRONTIER;
	
	if (ROOM_OWNER(room) && GET_BUILDING(room) && IS_COMPLETE(room)) {
		qt_empire_players(ROOM_OWNER(room), qt_lose_building, GET_BLD_VNUM(GET_BUILDING(room)));
		et_lose_building(ROOM_OWNER(room), GET_BLD_VNUM(GET_BUILDING(room)));
	}
	
	if (HAS_FUNCTION(room, FNC_LIBRARY)) {
		remove_library_from_books(GET_ROOM_VNUM(room));
	}
	
	if (ROOM_OWNER(room)) {
		adjust_building_tech(ROOM_OWNER(room), room, FALSE);
	}
	
	// delete any open instance here
	if (ROOM_AFF_FLAGGED(room, ROOM_AFF_HAS_INSTANCE) && (inst = find_instance_by_room(room, FALSE, FALSE))) {
		delete_instance(inst, TRUE);
	}
	
	dismantle_wtrigger(room, NULL, FALSE);
	if (GET_BUILDING(room)) {
		detach_building_from_room(room);
	}
	delete_room_npcs(room, NULL, TRUE);
	
	// remove bits including dismantle
	REMOVE_BIT(ROOM_BASE_FLAGS(room), ROOM_AFF_DISMANTLING | ROOM_AFF_TEMPORARY | ROOM_AFF_HAS_INSTANCE | ROOM_AFF_CHAMELEON | ROOM_AFF_NO_FLY | ROOM_AFF_NO_DISMANTLE | ROOM_AFF_NO_DISREPAIR | ROOM_AFF_INCOMPLETE | ROOM_AFF_BRIGHT_PAINT | ROOM_AFF_HIDE_REAL_NAME | ROOM_AFF_PERMANENT_PAINT);
	cancel_permanent_affects_room(room);
	affect_total_room(room);

	// free up the customs
	decustomize_room(room);
	
	// clean up scripts
	check_dg_owner_purged_room(room);
	if (SCRIPT(room)) {
		extract_script(room, WLD_TRIGGER);
	}
	free_proto_scripts(&room->proto_script);

	// restore sect: this does not use change_terrain()
	perform_change_sect(room, NULL, BASE_SECT(room));
	check_terrain_height(room);

	// update requirement trackers
	if (ROOM_OWNER(room)) {
		qt_empire_players(ROOM_OWNER(room), qt_lose_tile_sector, GET_SECT_VNUM(old_sect));
		et_lose_tile_sector(ROOM_OWNER(room), GET_SECT_VNUM(old_sect));
		
		qt_empire_players(ROOM_OWNER(room), qt_gain_tile_sector, GET_SECT_VNUM(SECT(room)));
		et_gain_tile_sector(ROOM_OWNER(room), GET_SECT_VNUM(SECT(room)));
	}
		
	// also check for missing crop data
	if (SECT_FLAGGED(SECT(room), SECTF_HAS_CROP_DATA | SECTF_CROP) && !ROOM_CROP(room)) {
		crop_data *new_crop = get_potential_crop_for_location(room, NOTHING);
		set_crop_type(room, new_crop ? new_crop : crop_table);
	}
	
	if (COMPLEX_DATA(room)) {
		COMPLEX_DATA(room)->home_room = NULL;
	}
	
	// some extra data safely clears now
	remove_room_extra_data(room, ROOM_EXTRA_FIRE_REMAINING);
	remove_room_extra_data(room, ROOM_EXTRA_TAVERN_TYPE);
	remove_room_extra_data(room, ROOM_EXTRA_TAVERN_BREWING_TIME);
	remove_room_extra_data(room, ROOM_EXTRA_TAVERN_AVAILABLE_TIME);
	remove_room_extra_data(room, ROOM_EXTRA_BUILD_RECIPE);
	remove_room_extra_data(room, ROOM_EXTRA_FOUND_TIME);
	remove_room_extra_data(room, ROOM_EXTRA_REDESIGNATE_TIME);
	remove_room_extra_data(room, ROOM_EXTRA_ORIGINAL_BUILDER);
	remove_room_extra_data(room, ROOM_EXTRA_PAINT_COLOR);
	remove_room_extra_data(room, ROOM_EXTRA_DEDICATE_ID);
	
	// some event types must be canceled
	cancel_stored_event_room(room, SEV_BURN_DOWN);
	cancel_stored_event_room(room, SEV_TAVERN);
	cancel_stored_event_room(room, SEV_RESET_TRIGGER);
	
	// disassociate inside rooms
	DL_FOREACH_SAFE2(interior_room_list, iter, next_iter, next_interior) {
		if (HOME_ROOM(iter) == room && iter != room) {
			dismantle_wtrigger(iter, NULL, FALSE);
			
			// move people and contents
			while ((temp_ch = ROOM_PEOPLE(iter))) {
				GET_LAST_DIR(temp_ch) = NO_DIR;
				char_to_room(temp_ch, room);
				msdp_update_room(temp_ch);
			}
			while (ROOM_CONTENTS(iter)) {
				obj_to_room(ROOM_CONTENTS(iter), room);
			}

			COMPLEX_DATA(iter)->home_room = NULL;
			delete_room(iter, FALSE);	// must check_all_exits
			deleted = TRUE;
		}
	}
	
	if (deleted) {
		check_all_exits();
	}
	
	// delete building properties
	if (COMPLEX_DATA(room)) {
		free_complex_data(COMPLEX_DATA(room));
		COMPLEX_DATA(room) = NULL;
	}
	
	// check for territory updates
	if (ROOM_OWNER(room) && was_large != (ROOM_BLD_FLAGGED(room, BLD_LARGE_CITY_RADIUS) ? TRUE : FALSE)) {
		struct empire_island *eisle = get_empire_island(ROOM_OWNER(room), GET_ISLAND_ID(room));
		is_ter = get_territory_type_for_empire(room, ROOM_OWNER(room), FALSE, &junk, NULL);
		
		if (was_ter != is_ter) {
			SAFE_ADD(EMPIRE_TERRITORY(ROOM_OWNER(room), was_ter), -1, 0, UINT_MAX, FALSE);
			SAFE_ADD(eisle->territory[was_ter], -1, 0, UINT_MAX, FALSE);
			
			SAFE_ADD(EMPIRE_TERRITORY(ROOM_OWNER(room), is_ter), 1, 0, UINT_MAX, FALSE);
			SAFE_ADD(eisle->territory[is_ter], 1, 0, UINT_MAX, FALSE);
			
			// (totals do not change)
		}
	}
	
	request_mapout_update(GET_ROOM_VNUM(room));
}


/**
* Finds the build recipe entry for a given building room.
*
* @param room_data *room The building location to find.
* @param byte type FIND_BUILD_UPGRADE or FIND_BUILD_NORMAL.
* @return craft_data* The craft for that building, or NULL.
*/
craft_data *find_building_list_entry(room_data *room, byte type) {
	room_data *r = HOME_ROOM(room);
	craft_data *craft, *iter, *next_iter;
	craft_vnum stored;
	
	// check the easy way -- does it have the recipe stored?
	if ((stored = get_room_extra_data(room, ROOM_EXTRA_BUILD_RECIPE)) > 0) {
		// if it was stored but fails real_recipe, this will still trigger the loop
		craft = craft_proto(stored);
		if (craft) {
			return craft;
		}
	}

	/* Each building in the building tree */
	HASH_ITER(hh, craft_table, iter, next_iter) {
		if (type == FIND_BUILD_UPGRADE && !IS_SET(GET_CRAFT_FLAGS(iter), CRAFT_UPGRADE)) {
			continue;
		}
		else if (type == FIND_BUILD_NORMAL && IS_SET(GET_CRAFT_FLAGS(iter), CRAFT_UPGRADE)) {
			continue;
		}
		
		if (COMPLEX_DATA(r) && IS_MAP_BUILDING(r) && BUILDING_VNUM(r) == GET_CRAFT_BUILD_TYPE(iter)) {
			return iter;
		}
	}
	
	return NULL;
}

/**
* Finds a room that the player could designate here.
*
* @param char *name The room name to search for.
* @param bitvector_t flags The DES_ designate flags that must match.
* @return bld_data *The matching room (building) entry or NULL if no match.
*/
bld_data *find_designate_room_by_name(char *name, bitvector_t flags) {
	bld_data *iter, *next_iter, *partial = NULL;
	
	HASH_ITER(hh, building_table, iter, next_iter) {
		if (!IS_SET(GET_BLD_FLAGS(iter), BLD_ROOM)) {
			continue;	// not designatable
		}
		if (flags && !IS_SET(GET_BLD_DESIGNATE_FLAGS(iter), flags)) {
			continue;	// not matching
		}
		if (!str_cmp(name, GET_BLD_NAME(iter))) {
			return iter;	// exact match
		}
		else if (!partial && is_abbrev(name, GET_BLD_NAME(iter))) {
			partial = iter;
		}
	}
	
	return partial;	// if any
}


/**
* Finds an upgrade craft for either a building or vehicle, for the 'upgrade'
* command. Pass either for_bld or for_veh, and pass NOTHING for the other.
*
* This is meant to be called by do_upgrade to find the craft for an upgraded
* building.
*
* @param char_data *ch Optional: the player, for checking skills and can-craft.
* @param bld_vnum for_bld Optional: The upgraded building to find the craft for (or NOTHING if checking for a vehicle).
* @param veh_vnum for_veh Optional: The upgraded vehicle to find the craft for (or NOTHING if checking for a building).
* @param craft_data **missing_abil Optional: A variable to bind a craft that would match except the ability is missing, to help give a better error message. (NULL to skip this)
* @return craft_data* The upgrade craft, if available. NULL otherwise.
*/
craft_data *find_upgrade_craft_for(char_data *ch, bld_vnum for_bld, veh_vnum for_veh, craft_data **missing_abil) {
	craft_data *iter, *next_iter;
	
	if (missing_abil) {
		// initialize
		*missing_abil = NULL;
	}
	
	HASH_ITER(hh, craft_table, iter, next_iter) {
		if (!CRAFT_FLAGGED(iter, CRAFT_UPGRADE)) {
			continue;	// not an upgrade
		}
		if (CRAFT_FLAGGED(iter, CRAFT_IN_DEVELOPMENT) && (!ch || !IS_IMMORTAL(ch))) {
			continue;	// not live
		}
		if (!(for_bld != NOTHING && CRAFT_IS_BUILDING(iter) && GET_CRAFT_BUILD_TYPE(iter) == for_bld) && !(for_veh != NOTHING && CRAFT_IS_VEHICLE(iter) && GET_CRAFT_OBJECT(iter) == for_veh)) {
			continue;	// did not match either for_bld or for_veh
		}
		if (ch && IS_SET(GET_CRAFT_FLAGS(iter), CRAFT_LEARNED) && !has_learned_craft(ch, GET_CRAFT_VNUM(iter))) {
			continue;	// not learned
		}
		if (ch && GET_CRAFT_REQUIRES_OBJ(iter) != NOTHING && !has_required_obj_for_craft(ch, GET_CRAFT_REQUIRES_OBJ(iter))) {
			continue;
		}
		if (ch && GET_CRAFT_ABILITY(iter) != NO_ABIL && !has_ability(ch, GET_CRAFT_ABILITY(iter))) {
			if (!*missing_abil) {
				*missing_abil = iter;
			}
			continue;	// missing ability
		}
		
		// it passes muster
		return iter;
	}
	
	return NULL;	// otherwise
}

#define find_upgrade_craft_for_bld(ch, bld, missing_abil)  find_upgrade_craft_for((ch), (bld), NOTHING, (missing_abil))
#define find_upgrade_craft_for_veh(ch, veh, missing_abil)  find_upgrade_craft_for((ch), NOTHING, (veh), (missing_abil))


/**
* Process the actual completion for a room.
*
* @param char_data *ch the builder (pc or npc)
* @param room_data *room the location
*/
void finish_building(char_data *ch, room_data *room) {
	craft_data *type = find_building_list_entry(room, FIND_BUILD_NORMAL);
	char_data *c = NULL;
	empire_data *emp = ROOM_OWNER(room);
	
	// check if it was actually an upgrade, not a new build
	if (!type) {
		type = find_building_list_entry(room, FIND_BUILD_UPGRADE);
	}
	
	msg_to_char(ch, "You complete the construction!\r\n");
	act("$n has completed the construction!", FALSE, ch, 0, 0, TO_ROOM);
	DL_FOREACH2(ROOM_PEOPLE(IN_ROOM(ch)), c, next_in_room) {
		if (!IS_NPC(c) && GET_ACTION(c) == ACT_BUILDING) {
			// if the player is loyal to the empire building here, gain skill
			if (!emp || GET_LOYALTY(c) == emp) {
				if (type && GET_CRAFT_ABILITY(type) != NO_ABIL) {
					gain_ability_exp(c, GET_CRAFT_ABILITY(type), 3);
					run_ability_hooks(c, AHOOK_ABILITY, GET_CRAFT_ABILITY(type), 0, NULL, NULL, NULL, room, NOBITS);
				}
			}
		}
	}
	
	complete_building(room);
}


/**
* finishes the actual dismantle for pc/npc building
*
* @param char_data *ch The dismantler.
* @param room_data *room The location that was dismantled.
*/
void finish_dismantle(char_data *ch, room_data *room) {
	obj_data *newobj, *proto;
	craft_data *type;
	char to[256];
	
	msg_to_char(ch, "You finish dismantling the building.\r\n");
	act("$n finishes dismantling the building.", FALSE, ch, 0, 0, TO_ROOM);
	stop_room_action(IN_ROOM(ch), ACT_DISMANTLING);
	
	// check for required obj and return it
	if ((type = find_building_list_entry(IN_ROOM(ch), FIND_BUILD_NORMAL)) || (type = find_building_list_entry(IN_ROOM(ch), FIND_BUILD_UPGRADE))) {
		if (CRAFT_FLAGGED(type, CRAFT_TAKE_REQUIRED_OBJ) && GET_CRAFT_REQUIRES_OBJ(type) != NOTHING && (proto = obj_proto(GET_CRAFT_REQUIRES_OBJ(type))) && !OBJ_FLAGGED(proto, OBJ_SINGLE_USE)) {
			newobj = read_object(GET_CRAFT_REQUIRES_OBJ(type), TRUE);
			
			// scale item to minimum level
			scale_item_to_level(newobj, 0);
			
			if (IS_NPC(ch)) {
				obj_to_room(newobj, room);
				check_autostore(newobj, TRUE, ROOM_OWNER(room));
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
	}
	
	disassociate_building(room);
	
	// message to update the room
	strcpy(to, GET_SECT_NAME(SECT(IN_ROOM(ch))));
	strtolower(to);
	sprintf(buf, "This area is now %s%s%s.", (to[strlen(to)-1] == 's' ? "" : AN(to)), (to[strlen(to)-1] == 's' ? "" : " "), to);
	act(buf, FALSE, ch, NULL, NULL, TO_CHAR | TO_ROOM);
}


/**
* Process the end of maintenance.
*
* @param char_data *ch Optional: The repairman (pc or npc; may be NULL).
* @param room_data *room The location.
*/
void finish_maintenance(char_data *ch, room_data *room) {
	// repair all damage
	set_room_damage(room, 0);
	if (IS_COMPLETE(room) && BUILDING_RESOURCES(room)) {
		free_resource_list(GET_BUILDING_RESOURCES(room));
		GET_BUILDING_RESOURCES(room) = NULL;
	}
	
	if (ch) {
		msg_to_char(ch, "You complete the maintenance.\r\n");
		act("$n has completed the maintenance.", FALSE, ch, NULL, NULL, TO_ROOM);
	}
	stop_room_action(room, ACT_MAINTENANCE);
	stop_room_action(room, ACT_BUILDING);
	request_world_save(GET_ROOM_VNUM(room), WSAVE_ROOM);
}


/**
* This pushes animals out of a building.
*
* @param room_data *location The building's tile.
*/
void herd_animals_out(room_data *location) {
	char_data *ch_iter, *next_ch;
	bool found_any, herd_msg = FALSE;
	room_data *to_room, *to_reverse;

	// no-work
	if (!IS_MAP_BUILDING(location) || ROOM_BLD_FLAGGED(location, BLD_OPEN) || BUILDING_ENTRANCE(location) == NO_DIR) {
		return;
	}
	
	to_room = real_shift(location, shift_dir[rev_dir[BUILDING_ENTRANCE(location)]][0], shift_dir[rev_dir[BUILDING_ENTRANCE(location)]][1]);
	to_reverse = real_shift(location, shift_dir[BUILDING_ENTRANCE(location)][0], shift_dir[BUILDING_ENTRANCE(location)][1]);

	// move everything out: the loop is because when 2 animals lead a wagon, next_ch can cause problems
	do {
		found_any = FALSE;
		
		DL_FOREACH_SAFE2(ROOM_PEOPLE(location), ch_iter, next_ch, next_in_room) {
			if (IS_NPC(ch_iter) && IN_ROOM(ch_iter) == location && !ch_iter->desc && !GET_LEADER(ch_iter) && !AFF_FLAGGED(ch_iter, AFF_CHARM) && MOB_FLAGGED(ch_iter, MOB_ANIMAL) && GET_POS(ch_iter) >= POS_STANDING && !MOB_FLAGGED(ch_iter, MOB_TIED)) {
				if (!herd_msg) {
					act("The animals are herded out of the building...", FALSE, ROOM_PEOPLE(location), NULL, NULL, TO_CHAR | TO_ROOM);
					herd_msg = TRUE;
				}
			
				// move the mob
				if ((!to_room || WATER_SECT(to_room)) && to_reverse && ROOM_BLD_FLAGGED(location, BLD_TWO_ENTRANCES)) {
					if (perform_move(ch_iter, BUILDING_ENTRANCE(location), NULL, MOVE_HERD)) {
						found_any = TRUE;
					}
				}
				else if (to_room) {
					if (perform_move(ch_iter, rev_dir[BUILDING_ENTRANCE(location)], NULL, MOVE_HERD)) {
						found_any = TRUE;
					}
				}
			}
		}
	} while (found_any);
}


/**
* This converts an interlink code back into a vnum.
*
* @param char *code The provided code.
* @return room_vnum The derived vnum.
*/
room_vnum interlink_to_vnum(char *code) {
	char bit[3];
	room_vnum val = 0;
	int pos = 1, this, iter;
	
	// convert to upper
	for (iter = 0; iter < strlen(code); ++iter) {
		code[iter] = UPPER(code[iter]);
	}
	
	while (*code && *(code+1)) {
		strncpy(bit, code, 2);
		bit[2] = '\0';
		
		this = search_block(bit, interlink_codes, TRUE);
		if (this != NOTHING) {
			val += this * pos;
			pos *= 10;
		}
		else {
			// error!
			val = NOWHERE;
			break;
		}
		
		// advance pointer two chars
		code += 2;
	}
	
	return val;
}


/**
* @param room_data *room The location to check.
* @return bool TRUE if this room is the entrance to another room.
*/
bool is_entrance(room_data *room) {
	int i;
	room_data *j;

	/* A few times I call this function with NULL.. easier here */
	if (!room)
		return FALSE;

	for (i = 0; i < NUM_2D_DIRS; i++) {
		j = real_shift(room, shift_dir[i][0], shift_dir[i][1]);
		if (j && IS_MAP_BUILDING(j)) {
			if (!ROOM_BLD_FLAGGED(j, BLD_OPEN) && BUILDING_ENTRANCE(j) == i) {
				return TRUE;
			}
			else if (ROOM_BLD_FLAGGED(j, BLD_TWO_ENTRANCES) && BUILDING_ENTRANCE(j) == rev_dir[i]) {
				return TRUE;
			}
		}
	}
	return FALSE;
}


/**
* Performs any forced-upgrades of buildings and vehicles. This is to be called
* at startup.
*/
void perform_force_upgrades(void) {
	room_data *room, *next_room;
	vehicle_data *veh, *next_veh;
	struct bld_relation *relat;
	bld_data *bld;
	
	HASH_ITER(hh, world_table, room, next_room) {
		if (IS_DISMANTLING(room)) {
			continue;	// skip anything beind dismantled
		}
		if (!(bld = GET_BUILDING(room)) || !GET_BLD_RELATIONS(bld)) {
			continue;	// invalid building
		}
		
		// look for a forced-relation and process it
		LL_FOREACH(GET_BLD_RELATIONS(bld), relat) {
			if (relat->type == BLD_REL_FORCE_UPGRADE_BLD) {
				log("perform_force_upgrades: %s%s: bld %d", get_room_name(room, FALSE), coord_display_room(NULL, room, FALSE), relat->vnum);
				start_upgrade(NULL, NULL, room, NULL, building_proto(relat->vnum), NULL);
				break;	// 1 only
			}
			else if (relat->type == BLD_REL_FORCE_UPGRADE_VEH) {
				log("perform_force_upgrades: %s%s: veh %d", get_room_name(room, FALSE), coord_display_room(NULL, room, FALSE), relat->vnum);
				start_upgrade(NULL, NULL, room, NULL, NULL, vehicle_proto(relat->vnum));
				break;	// 1 only
			}
		}
		// caution: building may be gone by the end of the loop
	}
	
	DL_FOREACH_SAFE(vehicle_list, veh, next_veh) {
		if (VEH_IS_DISMANTLING(veh)) {
			continue;	// skip anything beind dismantled
		}
		if (VEH_RELATIONS(veh)) {
			continue;	// invalid vehicle
		}
		
		// look for a forced-relation and process it
		LL_FOREACH(VEH_RELATIONS(veh), relat) {
			if (relat->type == BLD_REL_FORCE_UPGRADE_BLD) {
				log("perform_force_upgrades: %s%s: bld %d", VEH_SHORT_DESC(veh), coord_display_room(NULL, IN_ROOM(veh), FALSE), relat->vnum);
				start_upgrade(NULL, NULL, NULL, veh, building_proto(relat->vnum), NULL);
				break;	// 1 only
			}
			else if (relat->type == BLD_REL_FORCE_UPGRADE_VEH) {
				log("perform_force_upgrades: %s%s: veh %d", VEH_SHORT_DESC(veh), coord_display_room(NULL, IN_ROOM(veh), FALSE), relat->vnum);
				start_upgrade(NULL, NULL, NULL, veh, NULL, vehicle_proto(relat->vnum));
				break;	// 1 only
			}
		}
		// caution: vehicle may be gone by the end of the loop
	}
}


/**
* One build tick for char.
*
* @param char_data *ch The builder.
* @param room_data *room The location he/she is building.
* @param int act_type ACT_BUILDING or ACT_MAINTENANCE (determines outcome).
*/
void process_build(char_data *ch, room_data *room, int act_type) {
	craft_data *type = find_building_list_entry(room, FIND_BUILD_NORMAL);
	char buf[MAX_STRING_LENGTH];
	obj_data *found_obj = NULL;
	struct resource_data *res, temp_res;
	
	// Check if there's no longer a building in that place. 
	if (!GET_BUILDING(room)) {
		// Fail silently
		end_action(ch);
		return;
	}
	
	// just emergency check that it's not actually dismantling
	if (!IS_DISMANTLING(room) && BUILDING_RESOURCES(room)) {
		if ((res = get_next_resource(ch, BUILDING_RESOURCES(room), can_use_room(ch, room, GUESTS_ALLOWED), TRUE, &found_obj))) {
			// check required tool
			if (found_obj && GET_OBJ_REQUIRES_TOOL(found_obj) && !has_all_tools(ch, GET_OBJ_REQUIRES_TOOL(found_obj))) {
				prettier_sprintbit(GET_OBJ_REQUIRES_TOOL(found_obj), tool_flags, buf);
				if (count_bits(GET_OBJ_REQUIRES_TOOL(found_obj)) > 1) {
					msg_to_char(ch, "You need the following tools to use %s: %s\r\n", GET_OBJ_DESC(found_obj, ch, OBJ_DESC_SHORT), buf);
				}
				else {
					msg_to_char(ch, "You need %s %s to use %s.\r\n", AN(buf), buf, GET_OBJ_DESC(found_obj, ch, OBJ_DESC_SHORT));
				}
				cancel_action(ch);
				return;
			}
			
			// if maintaining, remove a "similar" item from the old built-with list -- this is what the maintaining item is replacing
			if (act_type == ACT_MAINTENANCE && found_obj) {
				remove_like_item_from_built_with(&GET_BUILT_WITH(room), found_obj);
			}
			
			// take the item; possibly free the res
			apply_resource(ch, res, &GET_BUILDING_RESOURCES(room), found_obj, APPLY_RES_BUILD, NULL, &GET_BUILT_WITH(room));
			request_world_save(GET_ROOM_VNUM(room), WSAVE_ROOM);
			
			// skillups (building only)
			if (type && act_type == ACT_BUILDING && GET_CRAFT_ABILITY(type) != NO_ABIL) {
				gain_ability_exp(ch, GET_CRAFT_ABILITY(type), 3);
				run_ability_hooks(ch, AHOOK_ABILITY, GET_CRAFT_ABILITY(type), 0, NULL, NULL, NULL, room, NOBITS);
			}
		}
		else {
			// missing next resource
			if (BUILDING_RESOURCES(room)) {
				// copy this to display the next 1
				temp_res = *BUILDING_RESOURCES(room);
				temp_res.next = NULL;
				show_resource_list(&temp_res, buf);
				msg_to_char(ch, "You don't have %s and stop working.\r\n", buf);
			}
			else {
				msg_to_char(ch, "You run out of resources and stop working.\r\n");
			}
			act("$n runs out of resources and stops working.", FALSE, ch, 0, 0, TO_ROOM);
			end_action(ch);
			return;
		}
	}
	
	// now finished?
	if (!BUILDING_RESOURCES(room)) {
		if (IS_INCOMPLETE(room)) {
			finish_building(ch, room);
		}
		else {
			finish_maintenance(ch, room);
		}
	}
}


/**
* Performs 1 step of the dismantle.
*
* @param char_data *ch The dismantler.
* @param room_data *room The location being dismantled.
*/
void process_dismantling(char_data *ch, room_data *room) {
	struct resource_data *res, *find_res, *next_res, *copy;
	char buf[MAX_STRING_LENGTH];

	// sometimes zeroes end up in here ... just clear them
	res = NULL;
	LL_FOREACH_SAFE(BUILDING_RESOURCES(room), find_res, next_res) {
		// things we can't refund
		if (find_res->amount <= 0 || find_res->type == RES_COMPONENT) {
			LL_DELETE(GET_BUILDING_RESOURCES(room), find_res);
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
				snprintf(buf, sizeof(buf), "You carefully remove %s from the structure.", get_obj_name_by_proto(res->vnum));
				act(buf, FALSE, ch, NULL, NULL, TO_CHAR | TO_SPAMMY);
				snprintf(buf, sizeof(buf), "$n removes %s from the structure.", get_obj_name_by_proto(res->vnum));
				act(buf, FALSE, ch, NULL, NULL, TO_ROOM | TO_SPAMMY);
				break;
			}
			case RES_LIQUID: {
				snprintf(buf, sizeof(buf), "You carefully retrieve %d unit%s of %s from the structure.", res->amount, PLURAL(res->amount), get_generic_string_by_vnum(res->vnum, GENERIC_LIQUID, GSTR_LIQUID_NAME));
				act(buf, FALSE, ch, NULL, NULL, TO_CHAR | TO_SPAMMY);
				snprintf(buf, sizeof(buf), "$n retrieves some %s from the structure.", get_generic_string_by_vnum(res->vnum, GENERIC_LIQUID, GSTR_LIQUID_NAME));
				act(buf, FALSE, ch, NULL, NULL, TO_ROOM | TO_SPAMMY);
				break;
			}
			case RES_COINS: {
				snprintf(buf, sizeof(buf), "You retrieve %s from the structure.", money_amount(real_empire(res->vnum), res->amount));
				act(buf, FALSE, ch, NULL, NULL, TO_CHAR | TO_SPAMMY);
				snprintf(buf, sizeof(buf), "$n retrieves %s from the structure.", res->amount == 1 ? "a coin" : "some coins");
				act(buf, FALSE, ch, NULL, NULL, TO_ROOM | TO_SPAMMY);
				break;
			}
			case RES_POOL: {
				snprintf(buf, sizeof(buf), "You regain %d %s point%s from the structure.", res->amount, pool_types[res->vnum], PLURAL(res->amount));
				act(buf, FALSE, ch, NULL, NULL, TO_CHAR | TO_SPAMMY);
				snprintf(buf, sizeof(buf), "$n retrieves %s from the structure.", pool_types[res->vnum]);
				act(buf, FALSE, ch, NULL, NULL, TO_ROOM | TO_SPAMMY);
				break;
			}
			case RES_CURRENCY: {
				snprintf(buf, sizeof(buf), "You retrieve %d %s from the structure.", res->amount, get_generic_string_by_vnum(res->vnum, GENERIC_CURRENCY, WHICH_CURRENCY(res->amount)));
				act(buf, FALSE, ch, NULL, NULL, TO_CHAR | TO_SPAMMY);
				snprintf(buf, sizeof(buf), "$n retrieves %d %s from the structure.", res->amount, get_generic_string_by_vnum(res->vnum, GENERIC_CURRENCY, WHICH_CURRENCY(res->amount)));
				act(buf, FALSE, ch, NULL, NULL, TO_ROOM | TO_SPAMMY);
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
			LL_DELETE(GET_BUILDING_RESOURCES(room), res);
			free(res);
		}
	}
	
	request_world_save(GET_ROOM_VNUM(room), WSAVE_ROOM);
	
	// done?
	if (!BUILDING_RESOURCES(room)) {
		finish_dismantle(ch, room);
	}
}


/**
* This removes an earlier entry (with the same component type) in a built-with
* list. This is called during maintenance so that the newly-added resource will
* replace an older one. Call this BEFORE adding the new resource to built-with.
*
* @param struct resource_data **built_with The room's &GET_BUILT_WITH()
* @param any_vnum component The component type that was added -- remove something with a matching component type.
*/
void remove_like_component_from_built_with(struct resource_data **built_with, any_vnum component) {
	struct resource_data *iter, *next;
	generic_data *my_cmp;
	obj_data *proto;
	
	if (!built_with || !*built_with || component == NOTHING) {
		return;	// no work
	}
	
	my_cmp = real_generic(component);
	
	LL_FOREACH_SAFE(*built_with, iter, next) {
		if (iter->type != RES_OBJECT || !(proto = obj_proto(iter->vnum))) {
			continue;	// no object
		}
		if (GET_OBJ_COMPONENT(proto) == NOTHING) {
			continue;	// not a component
		}
		if (GET_OBJ_COMPONENT(proto) != component && !is_component(proto, my_cmp) && !has_generic_relation(GEN_COMPUTED_RELATIONS(my_cmp), GET_OBJ_COMPONENT(proto))) {
			continue;	// not the right component or related component
		}
		
		// ok:
		iter->amount -= 1;
		if (iter->amount <= 0) {
			LL_DELETE(*built_with, iter);
			free(iter);
		}
		break;	// only need 1
	}
}


/**
* This removes an earlier entry (with the same component type or vnum as obj)
* in a built-with list. This is called during maintenance so that the newly-
* added resource will replace an older one. Call this BEFORE adding the new
* resource to built-with.
*
* @param struct resource_data **built_with The room's &GET_BUILT_WITH()
* @param obj_data *obj The obj that will be added (for matching the thing to remove).
*/
void remove_like_item_from_built_with(struct resource_data **built_with, obj_data *obj) {
	struct resource_data *iter, *next;
	obj_data *proto;
	
	if (!built_with || !*built_with || !obj) {
		return;	// no work
	}
	
	LL_FOREACH_SAFE(*built_with, iter, next) {
		if (iter->type == RES_OBJECT && (iter->vnum == GET_OBJ_VNUM(obj) || ((proto = obj_proto(iter->vnum)) && is_component_vnum(obj, GET_OBJ_COMPONENT(proto))))) {
			iter->amount -= 1;
			if (iter->amount <= 0) {
				LL_DELETE(*built_with, iter);
				free(iter);
			}
			break;	// only need 1
		}
	}
}


/**
* Helper function for construct_tunnel: sets up either entrance
*
* @param char_data *ch the builder
* @param room_data *room the location of this entrance
* @param int dir the direction it enters
*/
void setup_tunnel_entrance(char_data *ch, room_data *room, int dir) {
	bitvector_t tunnel_flags = 0;	// formerly ROOM_AFF_CHAMELEON;
	
	empire_data *emp = get_or_create_empire(ch);
	int ter_type;
	bool junk;
	
	construct_building(room, BUILDING_TUNNEL);
		
	SET_BIT(ROOM_BASE_FLAGS(room), tunnel_flags);
	affect_total_room(room);
	COMPLEX_DATA(room)->entrance = dir;
	if (emp && can_claim(ch) && !ROOM_AFF_FLAGGED(room, ROOM_AFF_UNCLAIMABLE)) {
		ter_type = get_territory_type_for_empire(room, emp, FALSE, &junk, NULL);
		if (EMPIRE_TERRITORY(emp, ter_type) < land_can_claim(emp, ter_type)) {
			claim_room(room, emp);
		}
	}
}


/**
* Starts the dismantle on a building. This sends no messages and starts no
* actions -- just updates the building to the dismantling state.
*
* @param room_data *loc The location to dismantle.
*/
void start_dismantle_building(room_data *loc) {
	struct resource_data *res, *next_res;
	room_data *room, *next_room;
	char_data *targ, *next_targ;
	craft_data *type;
	obj_data *obj, *next_obj, *proto;
	bool deleted = FALSE;
	bool complete = !IS_INCOMPLETE(loc);	// store now -- this gets changed part way through
	
	if (!IS_MAP_BUILDING(loc)) {
		log("SYSERR: Attempting to dismantle non-building room #%d", GET_ROOM_VNUM(loc));
		return;
	}
	
	// find the entry
	if (!(type = find_building_list_entry(loc, FIND_BUILD_NORMAL)) && !(type = find_building_list_entry(loc, FIND_BUILD_UPGRADE))) {
		log("SYSERR: Attempting to dismantle non-dismantlable building at #%d", GET_ROOM_VNUM(loc));
		return;
	}
	
	// shut off techs now
	if (ROOM_OWNER(loc)) {
		adjust_building_tech(ROOM_OWNER(loc), loc, FALSE);
	}
	
	// stored books?
	if (HAS_FUNCTION(loc, FNC_LIBRARY)) {
		dump_library_to_room(loc);
	}
	
	// interior only
	DL_FOREACH_SAFE2(interior_room_list, room, next_room, next_interior) {
		if (HOME_ROOM(room) == loc) {
			if (HAS_FUNCTION(room, FNC_LIBRARY)) {
				dump_library_to_room(room);
			}
			dismantle_wtrigger(room, NULL, FALSE);
			delete_room_npcs(room, NULL, TRUE);
			
			DL_FOREACH_SAFE2(ROOM_CONTENTS(room), obj, next_obj, next_content) {
				obj_to_room(obj, loc);
			}
			DL_FOREACH_SAFE2(ROOM_PEOPLE(room), targ, next_targ, next_in_room) {
				GET_LAST_DIR(targ) = NO_DIR;
				char_from_room(targ);
				char_to_room(targ, loc);
				msdp_update_room(targ);
			}
		
			delete_room(room, FALSE);	// must check_all_exits
			deleted = TRUE;
		}
	}
	
	if (deleted) {
		check_all_exits();
	}
	
	// unset private owner
	if (ROOM_PRIVATE_OWNER(loc) != NOBODY) {
		set_private_owner(loc, NOBODY);
	}
	
	// remove any existing resources remaining
	if (GET_BUILDING_RESOURCES(loc)) {
		free_resource_list(GET_BUILDING_RESOURCES(loc));
		GET_BUILDING_RESOURCES(loc) = NULL;
	}
	
	// set up resources
	if (GET_BUILT_WITH(loc)) {
		// normal setup: use the actual materials that built this building
		GET_BUILDING_RESOURCES(loc) = GET_BUILT_WITH(loc);
		GET_BUILT_WITH(loc) = NULL;
	}
	
	// remove liquids, etc
	LL_FOREACH_SAFE(GET_BUILDING_RESOURCES(loc), res, next_res) {
		// RES_x: only RES_OBJECT is refundable
		if (res->type != RES_OBJECT) {
			LL_DELETE(GET_BUILDING_RESOURCES(loc), res);
			free(res);
		}
		else {	// is object -- check for single_use
			if (!(proto = obj_proto(res->vnum)) || OBJ_FLAGGED(proto, OBJ_SINGLE_USE)) {
				LL_DELETE(GET_BUILDING_RESOURCES(loc), res);
				free(res);
			}
		}
	}
	
	if (!type || (!CRAFT_FLAGGED(type, CRAFT_FULL_DISMANTLE_REFUND) && (!CRAFT_FLAGGED(type, CRAFT_UNDAMAGED_DISMANTLE_REFUND) || BUILDING_DAMAGE(loc)))) {
		// reduce resource: they don't get it all back
		reduce_dismantle_resources(BUILDING_DAMAGE(loc), GET_BUILDING(loc) ? GET_BLD_MAX_DAMAGE(GET_BUILDING(loc)) : 1, &GET_BUILDING_RESOURCES(loc));
	}

	SET_BIT(ROOM_BASE_FLAGS(loc), ROOM_AFF_DISMANTLING);
	affect_total_room(loc);
	delete_room_npcs(loc, NULL, TRUE);
	
	if (loc && ROOM_OWNER(loc) && GET_BUILDING(loc) && complete) {
		qt_empire_players(ROOM_OWNER(loc), qt_lose_building, GET_BLD_VNUM(GET_BUILDING(loc)));
		et_lose_building(ROOM_OWNER(loc), GET_BLD_VNUM(GET_BUILDING(loc)));
	}
	
	stop_room_action(loc, ACT_DIGGING);
	stop_room_action(loc, ACT_BUILDING);
	stop_room_action(loc, ACT_MINING);
	stop_room_action(loc, ACT_MINTING);
	stop_room_action(loc, ACT_BATHING);
	stop_room_action(loc, ACT_SAWING);
	stop_room_action(loc, ACT_QUARRYING);
	stop_room_action(loc, ACT_MAINTENANCE);
	stop_room_action(loc, ACT_PICKING);
	
	affect_total_room(loc);
	request_mapout_update(GET_ROOM_VNUM(loc));
	request_world_save(GET_ROOM_VNUM(loc), WSAVE_ROOM);
}


/**
* This performs the actual upgrade of a building or vehicle. It will remove the
* old one and set up the new one, including any resources from the craft.
*
* Note this does not check permission or validity.
*
* @param char_data *ch Optional: The player who's doing the upgrade, for messaging (may be NULL).
* @param craft_data *upgrade_craft Optional: The craft recipe for the upgraded/new thing (required if a player is upgrading; optional if the MUD is forcing it).
* @param room_data *from_room Optional: The room/building being upgraded (MUST be NULL if upgrading a vehicle).
* @param vehicle_data *from_veh Optional: The vehicle being upgraded (may be NULL if it's a building/room being upgraded).
* @param bld_data *to_building Optional: New building's prototype for the upgrade (provide this OR to_vehicle, not both).
* @param vehicle_data *to_vehicle Optional: New vehicle's prototype for the upgrade (provide this OR to_building, not both).
* @return bool TRUE if the upgrade succeeded, FALSE if not.
*/
bool start_upgrade(char_data *ch, craft_data *upgrade_craft, room_data *from_room, vehicle_data *from_veh, bld_data *to_building, vehicle_data *to_vehicle) {
	vehicle_data *prev_veh, *new_veh, *old_proto;
	room_data *interior, *in_room, *room_iter, *next_iter;
	craft_data *from_craft = NULL;
	obj_data *obj, *next_obj;
	struct room_direction_data *exits;
	struct vehicle_room_list *vrl;
	bool fail, deleted;
	bitvector_t set_bits;
	char_data *temp_ch;
	int original_builder, veh_level = 0;
	
	// for moving data
	int private_owner = NOBODY, dedicated_to = 0;
	struct depletion_data *depletion = NULL;
	struct resource_data *needs_res = NULL, *built_with = NULL;
	bool bright_paint = FALSE;
	int paint_color = 0;
	
	if (!from_room && !from_veh) {
		if (ch) {
			msg_to_char(ch, "There doesn't seem to be anything to upgrade.\r\n");
		}
		else {
			log("Warning: perform_upgrade called without room/vehicle");
		}
		return FALSE;
	}
	
	// basic checking
	if (!to_building && !to_vehicle) {
		if (ch) {
			msg_to_char(ch, "That upgrade doesn't appear to be implemented correctly.\r\n");
		}
		else {
			log("Warning: perform_upgrade called without to_building or to_vehicle");
		}
		return FALSE;
	}
	if (upgrade_craft && CRAFT_IS_BUILDING(upgrade_craft) && !to_building) {
		if (ch) {
			msg_to_char(ch, "That upgrade is unimplemented.\r\n");
		}
		else {
			log("Warning: perform_upgrade called with invalid building craft");
		}
		return FALSE;
	}
	if (upgrade_craft && CRAFT_IS_VEHICLE(upgrade_craft) && !to_vehicle) {
		if (ch) {
			msg_to_char(ch, "That upgrade is unimplemented.\r\n");
		}
		else {
			log("Warning: perform_upgrade called with invalid vehicle craft");
		}
		return FALSE;
	}
	if (from_veh && to_building && !BLD_FLAGGED(to_building, BLD_OPEN)) {
		if (ch) {
			msg_to_char(ch, "You can't do that upgrade because it's incorrectly configured with a non-open building.\r\n");
		}
		else {
			log("Warning: perform_upgrade called with invalid vehicle-to-building craft");
		}
		return FALSE;
	}
	if (from_room && upgrade_craft && !check_build_location_and_dir(ch, from_room, upgrade_craft, BUILDING_ENTRANCE(from_room) != NO_DIR ? rev_dir[BUILDING_ENTRANCE(from_room)] : NO_DIR, TRUE, NULL, NULL)) {
		// sends own messages if ch is present
		if (!ch) {
			log("Warning: perform_upgrade called (on room) with invalid location or direction");
		}
		return FALSE;
	}
	if (from_veh) {	// check building location
		// temporarily remove from the room for validation
		prev_veh = (from_veh == ROOM_VEHICLES(IN_ROOM(from_veh))) ? NULL : from_veh->prev_in_room;
		DL_DELETE2(ROOM_VEHICLES(IN_ROOM(from_veh)), from_veh, prev_in_room, next_in_room);
		
		fail = upgrade_craft ? !check_build_location_and_dir(ch, IN_ROOM(from_veh), upgrade_craft, NO_DIR, TRUE, NULL, NULL) : FALSE;
		
		// re-add vehicle
		if (prev_veh) {
			DL_APPEND_ELEM2(ROOM_VEHICLES(IN_ROOM(from_veh)), prev_veh, from_veh, prev_in_room, next_in_room);
		}
		else {
			DL_PREPEND2(ROOM_VEHICLES(IN_ROOM(from_veh)), from_veh, prev_in_room, next_in_room);
		}
		
		if (fail) {
			// sends own messages if ch is present
			if (!ch) {
				log("Warning: perform_upgrade called (on vehicle) with invalid location or direction");
			}
			return FALSE;
		}
	}		
	
	// ---- start messages and pull some data for later ----
	
	if (from_room) {
		// upgrade FROM building
		in_room = from_room;
		
		if (ch) {
			msg_to_char(ch, "You begin to upgrade the building.\r\n");
			act("$n starts upgrading the building.", FALSE, ch, NULL, NULL, TO_ROOM);
		}
		
		// look up original recipe
		if (get_room_extra_data(from_room, ROOM_EXTRA_BUILD_RECIPE) > 0) {
			from_craft = craft_proto(get_room_extra_data(from_room, ROOM_EXTRA_BUILD_RECIPE));
		}
		
		// store some data
		built_with = GET_BUILT_WITH(from_room);
		GET_BUILT_WITH(from_room) = NULL;
		
		if (BUILDING_RESOURCES(from_room)) {
			needs_res = BUILDING_RESOURCES(from_room);
			GET_BUILDING_RESOURCES(from_room) = NULL;
		}
		
		depletion = ROOM_DEPLETION(from_room);
		ROOM_DEPLETION(from_room) = NULL;
		
		private_owner = ROOM_PRIVATE_OWNER(from_room);
		if (COMPLEX_DATA(from_room)) {
			set_private_owner(from_room, NOBODY);
		}
		
		// store dedication and remove it
		dedicated_to = get_room_extra_data(from_room, ROOM_EXTRA_DEDICATE_ID);
		remove_room_extra_data(from_room, ROOM_EXTRA_DEDICATE_ID);
		
		// store paint color and remove it
		original_builder = get_room_extra_data(from_room, ROOM_EXTRA_ORIGINAL_BUILDER);
		paint_color = get_room_extra_data(from_room, ROOM_EXTRA_PAINT_COLOR);
		remove_room_extra_data(from_room, ROOM_EXTRA_PAINT_COLOR);
		bright_paint = ROOM_AFF_FLAGGED(from_room, ROOM_AFF_BRIGHT_PAINT);
		REMOVE_BIT(ROOM_BASE_FLAGS(from_room), ROOM_AFF_BRIGHT_PAINT);
		// affect_total_room(from_room);	// probably don't need to do this here because it's done later
		request_world_save(GET_ROOM_VNUM(from_room), WSAVE_ROOM);
	}
	else if (from_veh) {
		// upgrade FROM vehicle
		in_room = IN_ROOM(from_veh);
		
		if (ch) {
			act("You start upgrading $V.", FALSE, ch, NULL, from_veh, TO_CHAR | ACT_VEH_VICT);
			act("$n starts upgrading $V.", FALSE, ch, NULL, from_veh, TO_ROOM | ACT_VEH_VICT);
		}
		
		// look up original recipe
		if (get_vehicle_extra_data(from_veh, ROOM_EXTRA_BUILD_RECIPE) > 0) {
			from_craft = craft_proto(get_vehicle_extra_data(from_veh, ROOM_EXTRA_BUILD_RECIPE));
		}
		
		// store some data
		built_with = VEH_BUILT_WITH(from_veh);
		VEH_BUILT_WITH(from_veh) = NULL;
		
		needs_res = VEH_NEEDS_RESOURCES(from_veh);
		VEH_NEEDS_RESOURCES(from_veh) = NULL;
		
		depletion = VEH_DEPLETION(from_veh);
		VEH_DEPLETION(from_veh) = NULL;
		
		if (VEH_INTERIOR_HOME_ROOM(from_veh) && COMPLEX_DATA(VEH_INTERIOR_HOME_ROOM(from_veh))) {
			private_owner = ROOM_PRIVATE_OWNER(VEH_INTERIOR_HOME_ROOM(from_veh));
			set_private_owner(VEH_INTERIOR_HOME_ROOM(from_veh), NOBODY);
		}
		
		// store dedication and remove it
		dedicated_to = get_vehicle_extra_data(from_veh, ROOM_EXTRA_DEDICATE_ID);
		remove_vehicle_extra_data(from_veh, ROOM_EXTRA_DEDICATE_ID);
		
		// record and remove paint color
		original_builder = get_vehicle_extra_data(from_veh, ROOM_EXTRA_ORIGINAL_BUILDER);
		paint_color = get_vehicle_extra_data(from_veh, ROOM_EXTRA_PAINT_COLOR);
		remove_vehicle_extra_data(from_veh, ROOM_EXTRA_PAINT_COLOR);
		bright_paint = VEH_FLAGGED(from_veh, VEH_BRIGHT_PAINT);
		remove_vehicle_flags(from_veh, VEH_BRIGHT_PAINT);
		veh_level = VEH_SCALE_LEVEL(from_veh);
	}
	else {
		if (ch) {
			msg_to_char(ch, "There was an unexpected error in the upgrade process.\r\n");
		}
		else {
			log("Warning: perform_upgrade had unknown error");
		}
		return FALSE;
	}
	
	// crunch some additional data
	if (from_craft && CRAFT_FLAGGED(from_craft, CRAFT_TAKE_REQUIRED_OBJ) && GET_CRAFT_REQUIRES_OBJ(from_craft) != NOTHING) {
		add_to_resource_list(&built_with, RES_OBJECT, GET_CRAFT_REQUIRES_OBJ(from_craft), 1, 0);
	}
	
	// ---- upgrade-from above; upgrade-to below -----
	
	// upgrade TO building
	if (to_building) {
		if (from_room) {	// upgrading from a traditional building
			if (GET_INSIDE_ROOMS(from_room) > 0 && !ROOM_BLD_FLAGGED(from_room, BLD_OPEN) && BLD_FLAGGED(to_building, BLD_OPEN)) {
				// closed building to open building with interior: remove the interior
				deleted = FALSE;
				DL_FOREACH_SAFE2(interior_room_list, room_iter, next_iter, next_interior) {
					if (HOME_ROOM(room_iter) == from_room && room_iter != from_room) {
						dismantle_wtrigger(room_iter, NULL, FALSE);
						
						// move people and contents
						while ((temp_ch = ROOM_PEOPLE(room_iter))) {
							GET_LAST_DIR(temp_ch) = NO_DIR;
							char_to_room(temp_ch, from_room);
							msdp_update_room(temp_ch);
						}
						while (ROOM_CONTENTS(room_iter)) {
							obj_to_room(ROOM_CONTENTS(room_iter), from_room);
						}
						while (ROOM_VEHICLES(room_iter)) {
							vehicle_to_room(ROOM_VEHICLES(room_iter), from_room);
						}

						COMPLEX_DATA(room_iter)->home_room = NULL;
						delete_room(room_iter, FALSE);	// must check_all_exits
						deleted = TRUE;
					}
				}
				
				if (deleted) {
					check_all_exits();
				}
			}
			
			// don't disassociate; just attach new building type
			dismantle_wtrigger(from_room, NULL, FALSE);
			detach_building_from_room(from_room);
			attach_building_to_room(building_proto(GET_BLD_VNUM(to_building)), from_room, TRUE);
			request_world_save(GET_ROOM_VNUM(from_room), WSAVE_ROOM);
		}
		else if (from_veh) {
			// upgraded from vehicle
			construct_building(in_room, GET_BLD_VNUM(to_building));
			
			// dump the vehicle -- no way to move the interior since the new building needs to be open
			fully_empty_vehicle(from_veh, in_room);
			while (VEH_ANIMALS(from_veh)) {
				unharness_mob_from_vehicle(VEH_ANIMALS(from_veh), from_veh);
			}
			
			// no need to herd animals out: it's got to be an open building
			
			// and remove it
			extract_vehicle(from_veh);
		}
		
		// main setup
		special_building_setup(ch, in_room);
		if (upgrade_craft) {
			set_room_extra_data(in_room, ROOM_EXTRA_BUILD_RECIPE, GET_CRAFT_VNUM(upgrade_craft));
		}
		if (ch) {
			set_room_extra_data(in_room, ROOM_EXTRA_ORIGINAL_BUILDER, GET_ACCOUNT(ch)->id);
		}
		else if (original_builder > 0) {
			set_room_extra_data(in_room, ROOM_EXTRA_ORIGINAL_BUILDER, original_builder);
		}
		if (upgrade_craft) {
			GET_BUILDING_RESOURCES(in_room) = combine_resources(needs_res, GET_CRAFT_RESOURCES(upgrade_craft));
			free_resource_list(needs_res);
		}
		else {
			GET_BUILDING_RESOURCES(in_room) = needs_res;
		}
		SET_BIT(ROOM_BASE_FLAGS(in_room), ROOM_AFF_INCOMPLETE);
		affect_total_room(in_room);	// do this right away to ensure incomplete flag
		
		// claim now if from-vehicle
		if (from_veh && VEH_OWNER(from_veh) && !ROOM_OWNER(in_room) && !ROOM_AFF_FLAGGED(in_room, ROOM_AFF_UNCLAIMABLE) && (!ch || can_claim(ch))) {
			perform_claim_room(in_room, VEH_OWNER(from_veh));
		}
		
		// transfer old data
		set_private_owner(in_room, private_owner);
		GET_BUILT_WITH(in_room) = built_with;
		ROOM_DEPLETION(in_room) = depletion;
		
		if (dedicated_to > 0 && ROOM_BLD_FLAGGED(in_room, BLD_DEDICATE)) {
			set_room_extra_data(in_room, ROOM_EXTRA_DEDICATE_ID, dedicated_to);
		}
		
		if (paint_color > 0 && !ROOM_BLD_FLAGGED(in_room, BLD_NO_PAINT)) {
			set_room_extra_data(in_room, ROOM_EXTRA_PAINT_COLOR, paint_color);
			if (bright_paint) {
				SET_BIT(ROOM_BASE_FLAGS(in_room), ROOM_AFF_BRIGHT_PAINT);
				// affect_total_room(in_room);	// probably don't need to do this here because it's done later
			}
		}
		
		// DONE: auto-complete it now if no resources
		if (!GET_BUILDING_RESOURCES(in_room)) {
			if (ch) {
				msg_to_char(ch, "You finish the upgrade!\r\n");
				act("$n finished the upgrade!", FALSE, ch, NULL, NULL, TO_ROOM);
				cancel_action(ch);
			}
			complete_building(in_room);
		}
	} // end upgrade-to-building
	
	// upgrade TO vehicle
	else if (to_vehicle) {
		new_veh = read_vehicle(VEH_VNUM(to_vehicle), TRUE);
		
		// set incomplete before putting in the room
		set_vehicle_flags(new_veh, VEH_INCOMPLETE);
		vehicle_to_room(new_veh, in_room);
		
		if (from_room) {
			// upgraded from building
			
			// claim check
			if (ROOM_OWNER(from_room) && !VEH_FLAGGED(new_veh, VEH_NO_CLAIM)) {
				perform_claim_vehicle(new_veh, ROOM_OWNER(from_room));
			}
			
			if ((interior = get_vehicle_interior(new_veh))) {
				// move objs/vehs inside the incomplete vehicle's main room?
				
				DL_FOREACH2(interior_room_list, room_iter, next_interior) {
					if (room_iter == from_room || HOME_ROOM(room_iter) != from_room) {
						continue;	// these are not the rooms you're looking for
					}
					
					COMPLEX_DATA(room_iter)->home_room = interior;
					add_room_to_vehicle(room_iter, new_veh);
					request_world_save(GET_ROOM_VNUM(room_iter), WSAVE_ROOM);
					
					// check exits to the old home room and move them
					LL_FOREACH(COMPLEX_DATA(room_iter)->exits, exits) {
						if (exits->to_room == GET_ROOM_VNUM(from_room)) {
							exits->to_room = GET_ROOM_VNUM(interior);
							exits->room_ptr = interior;
							++GET_EXITS_HERE(interior);
							--GET_EXITS_HERE(from_room);
						}
					}
				}
				
				// apply exits from the old home-room to the interior
				if (COMPLEX_DATA(from_room)) {
					LL_FOREACH(COMPLEX_DATA(from_room)->exits, exits) {
						if (exits->to_room >= MAP_SIZE) {
							// copy any exit that's off the map
							create_exit(interior, exits->room_ptr, exits->dir, FALSE);
						}
					}
				}
				
				// apply data inside
				set_private_owner(interior, private_owner);
				
				// need to run this early because it won't run on completion
				complete_wtrigger(interior);
			}
			
			// and remove old building
			disassociate_building(from_room);
		}
		else if (from_veh) {
			// upgraded from vehicle
			
			// see if we need to abandon the old one
			if (VEH_OWNER(from_veh) && VEH_FLAGGED(new_veh, VEH_NO_CLAIM)) {
				perform_abandon_vehicle(from_veh);
			}
			else if (VEH_OWNER(from_veh)) {
				perform_claim_vehicle(new_veh, VEH_OWNER(from_veh));
			}
			
			// veh-to-veh data
			VEH_INSTANCE_ID(new_veh) = VEH_INSTANCE_ID(from_veh);
			
			VEH_ANIMALS(new_veh) = VEH_ANIMALS(from_veh);
			VEH_ANIMALS(from_veh) = NULL;
			
			VEH_EXTRA_DATA(new_veh) = VEH_EXTRA_DATA(from_veh);
			VEH_EXTRA_DATA(from_veh) = NULL;
			
			// adapt flags: find flags added to from_veh
			if ((old_proto = vehicle_proto(VEH_VNUM(from_veh)))) {
				set_bits = VEH_FLAGS(from_veh) & SAVABLE_VEH_FLAGS & ~VEH_FLAGS(old_proto);
				set_vehicle_flags(new_veh, set_bits);
			}
		
			// transfer/dump contents
			if (VEH_CAPACITY(new_veh) > 0) {
				DL_FOREACH_SAFE2(VEH_CONTAINS(from_veh), obj, next_obj, next_content) {
					obj_to_vehicle(obj, new_veh);
				}
			}
			else {
				empty_vehicle(from_veh, in_room);
			}
			
			// transfer rooms
			if (VEH_INTERIOR_ROOM_VNUM(new_veh) != NOTHING && (interior = get_vehicle_interior(from_veh))) {
				VEH_INSIDE_ROOMS(new_veh) = VEH_INSIDE_ROOMS(from_veh);
				
				// move home room
				VEH_INTERIOR_HOME_ROOM(new_veh) = interior;
				COMPLEX_DATA(interior)->vehicle = new_veh;
				
				// move whole interior
				LL_FOREACH(VEH_ROOM_LIST(from_veh), vrl) {
					COMPLEX_DATA(vrl->room)->vehicle = new_veh;
				}
				
				// transfer room list
				VEH_ROOM_LIST(new_veh) = VEH_ROOM_LIST(from_veh);
				VEH_ROOM_LIST(from_veh) = NULL;
				VEH_INTERIOR_HOME_ROOM(from_veh) = NULL;
				
				// change room
				if (VEH_INTERIOR_ROOM_VNUM(new_veh) != VEH_INTERIOR_ROOM_VNUM(from_veh) && building_proto(VEH_INTERIOR_ROOM_VNUM(new_veh))) {
					detach_building_from_room(interior);
					attach_building_to_room(building_proto(VEH_INTERIOR_ROOM_VNUM(new_veh)), interior, TRUE);
				}
				
				// apply data
				set_private_owner(interior, private_owner);
				
				// need to run this early because it won't run on completion
				complete_wtrigger(interior);
			}
			else {
				fully_empty_vehicle(from_veh, in_room);
			}
			
			// remove old vehicle
			extract_vehicle(from_veh);
		}
	
		// additional setup
		special_vehicle_setup(ch, new_veh);
		if (upgrade_craft) {
			VEH_NEEDS_RESOURCES(new_veh) = combine_resources(needs_res, GET_CRAFT_RESOURCES(upgrade_craft));
			free_resource_list(needs_res);
		}
		else {
			VEH_NEEDS_RESOURCES(new_veh) = needs_res;
		}
		VEH_HEALTH(new_veh) = MAX(1, VEH_MAX_HEALTH(new_veh) * 0.2);	// start at 20% health, will heal on completion
		if (ch) {
			GET_ACTION_VNUM(ch, 1) = VEH_CONSTRUCTION_ID(new_veh);
		}
		if (ch && upgrade_craft) {
			scale_vehicle_to_level(new_veh, get_craft_scale_level(ch, upgrade_craft));
		}
		else {
			scale_vehicle_to_level(new_veh, veh_level);
		}
		VEH_CONSTRUCTION_ID(new_veh) = get_new_vehicle_construction_id();
		if (upgrade_craft) {
			set_vehicle_extra_data(new_veh, ROOM_EXTRA_BUILD_RECIPE, GET_CRAFT_VNUM(upgrade_craft));
		}
		if (ch) {
			set_vehicle_extra_data(new_veh, ROOM_EXTRA_ORIGINAL_BUILDER, GET_ACCOUNT(ch)->id);
		}
		else if (original_builder > 0) {
			set_vehicle_extra_data(new_veh, ROOM_EXTRA_ORIGINAL_BUILDER, original_builder);
		}
		
		// transfer stuff from old data
		VEH_BUILT_WITH(new_veh) = built_with;
		VEH_DEPLETION(new_veh) = depletion;
		
		if (dedicated_to > 0 && VEH_FLAGGED(new_veh, BLD_DEDICATE)) {
			set_vehicle_extra_data(new_veh, ROOM_EXTRA_DEDICATE_ID, dedicated_to);
		}
		
		// check the paint
		if (paint_color > 0 && !VEH_FLAGGED(new_veh, VEH_NO_PAINT)) {
			set_vehicle_extra_data(new_veh, ROOM_EXTRA_PAINT_COLOR, paint_color);
			if (bright_paint) {
				set_vehicle_flags(new_veh, VEH_BRIGHT_PAINT);
			}
		}
		else {	// ensure not painted
			remove_vehicle_flags(new_veh, VEH_BRIGHT_PAINT);
			remove_vehicle_extra_data(new_veh, ROOM_EXTRA_PAINT_COLOR);
		}
		
		// DONE: autocomplete if no resources on the upgrade
		if (!VEH_NEEDS_RESOURCES(new_veh)) {
			if (ch) {
				msg_to_char(ch, "You finish the upgrade!\r\n");
				act("$n finished the upgrade!", FALSE, ch, NULL, NULL, TO_ROOM);
				cancel_action(ch);
			}
			complete_vehicle(new_veh);
		}
		
		request_vehicle_save_in_world(new_veh);
	} // end upgrade-to-vehicle
	
	affect_total_room(in_room);
	
	return TRUE;
}


/**
* This converts a vnum into a semi-obfuscated interlink code.
*
* @param room_vnum vnum The vnum to convert.
* @return char* The interlink code.
*/
char *vnum_to_interlink(room_vnum vnum) {
	static char output[MAX_INPUT_LENGTH];
	
	int val = vnum, this;
	
	*output = '\0';
	
	while (val > 0) {
		this = val % 10;
		val /= 10;
		
		strcat(output, interlink_codes[this]);
	}
	
	return output;
}


 //////////////////////////////////////////////////////////////////////////////
//// MAIN BUILDING COMMANDS //////////////////////////////////////////////////

/**
* Processes the "dismantle" command when targeting a vehicle. It resumes
* an in-process dismantle or else it handles the start-dismantle process.
*
* @param char_data *ch The player (not NPC).
* @param vehicle_data *veh The vehicle they targeted.
*/
void do_dismantle_vehicle(char_data *ch, vehicle_data *veh) {
	char_data *ch_iter;
	craft_data *craft;
	
	if (!ch || !veh || IS_NPC(ch)) {
		return;	// safety
	}
	
	if (!IS_APPROVED(ch) && config_get_bool("build_approval")) {
		send_config_msg(ch, "need_approval_string");
	}
	else if (GET_ACTION(ch) != ACT_NONE) {
		msg_to_char(ch, "You're kinda busy right now.\r\n");
	}
	else if (VEH_IS_DISMANTLING(veh)) {
		// already being dismantled: RESUME
		if (can_use_vehicle(ch, veh, MEMBERS_ONLY)) {
			act("You begin to dismantle $V.", FALSE, ch, NULL, veh, TO_CHAR | ACT_VEH_VICT);
			act("$n begins to dismantle $V.", FALSE, ch, NULL, veh, TO_ROOM | ACT_VEH_VICT);
			start_action(ch, ACT_DISMANTLE_VEHICLE, 0);
			GET_ACTION_VNUM(ch, 1) = VEH_CONSTRUCTION_ID(veh);
			command_lag(ch, WAIT_OTHER);
		}
		else {
			act("You don't have permission to dismantle $V.", FALSE, ch, NULL, veh, TO_CHAR | ACT_VEH_VICT);
		}
	}
	else if (VEH_FLAGGED(veh, VEH_NEVER_DISMANTLE)) {
		msg_to_char(ch, "That cannot be dismantled.\r\n");
	}
	else if (VEH_OWNER(veh) && VEH_OWNER(veh) != GET_LOYALTY(ch)) {
		msg_to_char(ch, "You can't dismantle a %s you don't own.\r\n", VEH_OR_BLD(veh));
	}
	else if (WATER_SECT(IN_ROOM(ch))) {
		msg_to_char(ch, "You can't dismantle it in the water.\r\n");
	}
	else if (!HAS_DISMANTLE_PRIV_FOR_VEHICLE(ch, veh)) {
		msg_to_char(ch, "You don't have permission to dismantle that.\r\n");
	}
	else if ((craft = find_craft_for_vehicle(veh)) && !CRAFT_FLAGGED(craft, CRAFT_DISMANTLE_WITHOUT_ABILITY) && GET_CRAFT_ABILITY(craft) != NO_ABIL && !has_ability(ch, GET_CRAFT_ABILITY(craft)) && get_vehicle_extra_data(veh, ROOM_EXTRA_ORIGINAL_BUILDER) != GET_ACCOUNT(ch)->id) {
		msg_to_char(ch, "You don't have the skill needed to dismantle that properly.\r\n");
	}
	else if (VEH_FLAGGED(veh, VEH_PLAYER_NO_DISMANTLE) || ROOM_AFF_FLAGGED(IN_ROOM(veh), ROOM_AFF_NO_DISMANTLE)) {
		msg_to_char(ch, "Turn off no-dismantle before dismantling that %s (see HELP MANAGE).\r\n", VEH_OR_BLD(veh));
	}
	else if (VEH_FLAGGED(veh, VEH_ON_FIRE)) {
		msg_to_char(ch, "You can't dismantle that -- it's on fire!\r\n");
	}
	else if (count_harnessed_animals(veh) > 0) {
		msg_to_char(ch, "You can't dismantle that while animals are harnessed to it.\r\n");
	}
	else if (VEH_SITTING_ON(veh)) {
		msg_to_char(ch, "You can't dismantle it while someone is %s it.\r\n", IN_OR_ON(veh));
	}
	else if (VEH_LED_BY(veh)) {
		msg_to_char(ch, "You can't dismantle it while someone is leading it.\r\n");
	}
	else if (count_players_in_vehicle(veh, TRUE)) {
		msg_to_char(ch, "You can't dismantle it while someone is inside.\r\n");
	}
	else if (!dismantle_vtrigger(ch, veh, TRUE)) {
		// prevented by trigger
	}
	else {
		// ensure nobody is building it
		if (!VEH_IS_DISMANTLING(veh)) {
			DL_FOREACH2(ROOM_PEOPLE(IN_ROOM(ch)), ch_iter, next_in_room) {
				if (ch_iter != ch && !IS_NPC(ch_iter) && (GET_ACTION(ch_iter) == ACT_BUILDING || GET_ACTION(ch_iter) == ACT_MAINTENANCE || GET_ACTION(ch_iter) == ACT_GEN_CRAFT) && GET_ACTION_VNUM(ch_iter, 1) == VEH_CONSTRUCTION_ID(veh)) {
					msg_to_char(ch, "You can't start dismantling it while someone is working on it.\r\n");
					return;
				}
			}
		}
		
		// ok: start dismantle
		act("You begin to dismantle $V.", FALSE, ch, NULL, veh, TO_CHAR | ACT_VEH_VICT);
		act("$n begins to dismantle $V.", FALSE, ch, NULL, veh, TO_ROOM | ACT_VEH_VICT);
		
		start_dismantle_vehicle(veh, ch);
		start_action(ch, ACT_DISMANTLE_VEHICLE, 0);
		GET_ACTION_VNUM(ch, 1) = VEH_CONSTRUCTION_ID(veh);
		process_dismantle_vehicle(ch);
		command_lag(ch, WAIT_OTHER);
	}
}


ACMD(do_dismantle) {
	vehicle_data *veh;
	char_data *ch_iter;
	craft_data *type;

	if (IS_NPC(ch)) {
		msg_to_char(ch, "NPCs cannot use the dismantle command.\r\n");
		return;
	}
	
	// fall through to dismantle-vehicle?
	one_argument(argument, arg);
	if (*arg && (veh = get_vehicle_in_room_vis(ch, arg, NULL))) {
		do_dismantle_vehicle(ch, veh);
		return;
	}
	
	// not a vehicle
	if (GET_ROOM_VEHICLE(IN_ROOM(ch))) {
		msg_to_char(ch, "Dismantle this %s from outside instead.\r\n", VEH_OR_BLD(GET_ROOM_VEHICLE(IN_ROOM(ch))));
		return;
	}
	
	// otherwise args are not welcome
	if (*arg && !isname(arg, get_room_name(IN_ROOM(ch), FALSE))) {
		msg_to_char(ch, "You don't see that to dismantle here.\r\n");
		return;
	}
	
	if (GET_ACTION(ch) == ACT_DISMANTLING) {
		msg_to_char(ch, "You stop dismantling the building.\r\n");
		act("$n stops dismantling the building.", FALSE, ch, 0, 0, TO_ROOM);
		end_action(ch);
		return;
	}
	if (GET_ACTION(ch) == ACT_DISMANTLE_VEHICLE) {
		msg_to_char(ch, "You stop dismantling.\r\n");
		act("$n stops dismantling.", FALSE, ch, NULL, NULL, TO_ROOM);
		end_action(ch);
		return;
	}
	
	if (!IS_APPROVED(ch) && config_get_bool("build_approval")) {
		send_config_msg(ch, "need_approval_string");
		return;
	}

	if (GET_ACTION(ch) != ACT_NONE) {
		msg_to_char(ch, "You're kinda busy right now.\r\n");
		return;
	}
	
	// resume
	if (IS_DISMANTLING(IN_ROOM(ch))) {
		if (!can_use_room(ch, IN_ROOM(ch), MEMBERS_ONLY)) {
			msg_to_char(ch, "You don't have permission to dismantle here.\r\n");
			return;
		}
		
		msg_to_char(ch, "You begin to dismantle the building.\r\n");
		act("$n begins to dismantle the building.", FALSE, ch, 0, 0, TO_ROOM);
		start_action(ch, ACT_DISMANTLING, 0);
		return;
	}
	
	if (!COMPLEX_DATA(IN_ROOM(ch))) {
		// also check for dismantle-able vehicles
		DL_FOREACH2(ROOM_VEHICLES(IN_ROOM(ch)), veh, next_in_room) {
			if (VEH_FLAGGED(veh, VEH_DISMANTLING) || !VEH_FLAGGED(veh, VEH_NEVER_DISMANTLE)) {
				msg_to_char(ch, "Use 'dismantle <name>' to dismantle a vehicle or building in the room.\r\n");
				return;
			}
		}
		
		msg_to_char(ch, "You can't start dismantling anything here.\r\n");
		return;
	}
	
	if (HOME_ROOM(IN_ROOM(ch)) != IN_ROOM(ch)) {
		msg_to_char(ch, "You need to dismantle from the main room.\r\n");
		return;
	}
	
	if (IS_BURNING(IN_ROOM(ch))) {
		msg_to_char(ch, "You can't dismantle a burning building!\r\n");
		return;
	}
	
	if (ROOM_AFF_FLAGGED(IN_ROOM(ch), ROOM_AFF_HAS_INSTANCE)) {
		msg_to_char(ch, "You can't dismantle this building because an adventure is currently linked here.\r\n");
		return;
	}

	if (!(type = find_building_list_entry(IN_ROOM(ch), FIND_BUILD_NORMAL))) {
		if (!(type = find_building_list_entry(IN_ROOM(ch), FIND_BUILD_UPGRADE))) {
			// also check for dismantle-able vehicles
			DL_FOREACH2(ROOM_VEHICLES(IN_ROOM(ch)), veh, next_in_room) {
				if (VEH_FLAGGED(veh, VEH_DISMANTLING) || !VEH_FLAGGED(veh, VEH_NEVER_DISMANTLE)) {
					msg_to_char(ch, "Use 'dismantle <name>' to dismantle a vehicle or building in the room.\r\n");
					return;
				}
			}
			
			// if we got here, there weren't any
			msg_to_char(ch, "You can't dismantle anything here.\r\n");
			return;
		}
	}

	if (ROOM_AFF_FLAGGED(IN_ROOM(ch), ROOM_AFF_UNCLAIMABLE)) {
		msg_to_char(ch, "You can't dismantle this building because the tile is unclaimable.\r\n");
		return;
	}

	if (!HAS_DISMANTLE_PRIV_FOR_BUILDING(ch, IN_ROOM(ch))) {
		msg_to_char(ch, "You don't have permission to dismantle this building.\r\n");
		return;
	}

	if (!CRAFT_FLAGGED(type, CRAFT_DISMANTLE_WITHOUT_ABILITY) && GET_CRAFT_ABILITY(type) != NO_ABIL && !has_ability(ch, GET_CRAFT_ABILITY(type)) && get_room_extra_data(IN_ROOM(ch), ROOM_EXTRA_ORIGINAL_BUILDER) != GET_ACCOUNT(ch)->id) {
		msg_to_char(ch, "You don't have the skill needed to dismantle this building properly.\r\n");
		return;
	}
	
	if (SECT_FLAGGED(BASE_SECT(IN_ROOM(ch)), SECTF_FRESH_WATER | SECTF_OCEAN) && is_entrance(IN_ROOM(ch))) {
		msg_to_char(ch, "You can't dismantle here because it's the entrance for another building.\r\n");
		return;
	}

	if (ROOM_AFF_FLAGGED(IN_ROOM(ch), ROOM_AFF_NO_DISMANTLE)) {
		msg_to_char(ch, "You can't dismantle this building (use 'manage no-dismantle' to toggle).\r\n");
		return;
	}
	if (!can_see_in_dark_room(ch, IN_ROOM(ch), TRUE)) {
		msg_to_char(ch, "It's too dark to start dismantling.\r\n");
		return;
	}
	
	// ensure nobody is building
	if (!IS_DISMANTLING(IN_ROOM(ch))) {
		DL_FOREACH2(ROOM_PEOPLE(IN_ROOM(ch)), ch_iter, next_in_room) {
			if (ch_iter != ch && !IS_NPC(ch_iter) && (GET_ACTION(ch_iter) == ACT_BUILDING || GET_ACTION(ch_iter) == ACT_MAINTENANCE)) {
				msg_to_char(ch, "You can't start dismantling while someone is still building it.\r\n");
				return;
			}
		}
	}
	
	if (!dismantle_wtrigger(IN_ROOM(ch), ch, TRUE)) {
		return;	// this goes last
	}
	
	start_dismantle_building(IN_ROOM(ch));
	start_action(ch, ACT_DISMANTLING, 0);
	msg_to_char(ch, "You begin to dismantle the building.\r\n");
	act("$n begins to dismantle the building.\r\n", FALSE, ch, 0, 0, TO_ROOM);
	process_dismantling(ch, IN_ROOM(ch));
	command_lag(ch, WAIT_OTHER);
}


 //////////////////////////////////////////////////////////////////////////////
//// ADDITIONAL COMMANDS /////////////////////////////////////////////////////

// called by do_customize
void do_customize_room(char_data *ch, char *argument) {
	char arg2[MAX_STRING_LENGTH];
	empire_data *emp = GET_LOYALTY(ch);
	vehicle_data *veh;
	
	half_chop(argument, arg, arg2);
	
	if (!ch->desc) {
		msg_to_char(ch, "You can't do that.\r\n");
	}
	else if (((veh = get_vehicle_in_room_vis(ch, arg, NULL)) || ((veh = GET_ROOM_VEHICLE(IN_ROOM(ch))) && isname(arg, VEH_KEYWORDS(veh)))) && VEH_FLAGGED(veh, VEH_BUILDING)) {
		// pass through to customize-vehicle (probably a building vehicle)
		do_customize_vehicle(ch, arg2);
	}
	else if (!has_player_tech(ch, PTECH_CUSTOMIZE_BUILDING)) {
		msg_to_char(ch, "You don't have the right ability to customize buildings.\r\n");
	}
	else if (!emp || ROOM_OWNER(IN_ROOM(ch)) != emp) {
		msg_to_char(ch, "You must own the tile to do this.\r\n");
	}
	else if (!IS_ANY_BUILDING(IN_ROOM(ch)) || ROOM_BLD_FLAGGED(IN_ROOM(ch), BLD_NO_CUSTOMIZE)) {
		msg_to_char(ch, "You can't customize here.\r\n");
	}
	else if (!has_permission(ch, PRIV_CUSTOMIZE, IN_ROOM(ch))) {
		msg_to_char(ch, "You are not allowed to customize.\r\n");
	}
	else if (run_ability_triggers_by_player_tech(ch, PTECH_CUSTOMIZE_BUILDING, NULL, NULL, NULL)) {
		// triggered
	}
	else if (is_abbrev(arg, "name")) {
		if (!*arg2) {
			msg_to_char(ch, "What would you like to name this room (or \"none\")?\r\n");
		}
		else if (!str_cmp(arg2, "none")) {
			set_room_custom_name(IN_ROOM(ch), NULL);
			
			// they lose hide-real-name if they rename it themselves
			REMOVE_BIT(ROOM_BASE_FLAGS(IN_ROOM(ch)), ROOM_AFF_HIDE_REAL_NAME);
			affect_total_room(IN_ROOM(ch));
			
			msg_to_char(ch, "This room no longer has a custom name.\r\n");
			act("$n renames the room.", FALSE, ch, NULL, NULL, TO_ROOM);
			command_lag(ch, WAIT_ABILITY);
		}
		else if (color_code_length(arg2) > 0) {
			msg_to_char(ch, "You cannot use color codes in custom room names.\r\n");
		}
		else if (strlen(arg2) > 60) {
			msg_to_char(ch, "That name is too long. Room names may not be over 60 characters.\r\n");
		}
		else {
			if (!ROOM_CUSTOM_NAME(IN_ROOM(ch))) {
				gain_player_tech_exp(ch, PTECH_CUSTOMIZE_BUILDING, 33.4);
			}
			run_ability_hooks_by_player_tech(ch, PTECH_CUSTOMIZE_BUILDING, NULL, NULL, NULL, IN_ROOM(ch));
			set_room_custom_name(IN_ROOM(ch), arg2);
			
			// they lose hide-real-name if they rename it themselves
			REMOVE_BIT(ROOM_BASE_FLAGS(IN_ROOM(ch)), ROOM_AFF_HIDE_REAL_NAME);
			affect_total_room(IN_ROOM(ch));
			
			msg_to_char(ch, "This room is now called \"%s\".\r\n", arg2);
			act("$n renames the room.", FALSE, ch, NULL, NULL, TO_ROOM);
			command_lag(ch, WAIT_ABILITY);
		}
	}
	else if (is_abbrev(arg, "description")) {
		if (!*arg2) {
			msg_to_char(ch, "To set a description, use \"customize building description set\".\r\n");
			msg_to_char(ch, "To remove a description, use \"customize building description none\".\r\n");
		}
		else if (ch->desc->str) {
			msg_to_char(ch, "You are already editing something else.\r\n");
		}
		else if (is_abbrev(arg2, "none")) {
			set_room_custom_description(IN_ROOM(ch), NULL);
			msg_to_char(ch, "This room no longer has a custom description.\r\n");
		}
		else if (is_abbrev(arg2, "set")) {
			if (!ROOM_CUSTOM_DESCRIPTION(IN_ROOM(ch))) {
				gain_player_tech_exp(ch, PTECH_CUSTOMIZE_BUILDING, 33.4);
			}
			run_ability_hooks_by_player_tech(ch, PTECH_CUSTOMIZE_BUILDING, NULL, NULL, NULL, IN_ROOM(ch));
			start_string_editor(ch->desc, "room description", &(ROOM_CUSTOM_DESCRIPTION(IN_ROOM(ch))), MAX_ROOM_DESCRIPTION, TRUE);
			ch->desc->save_room_id = GET_ROOM_VNUM(IN_ROOM(ch));
			act("$n begins editing the room description.", TRUE, ch, 0, 0, TO_ROOM);
		}
		else {
			msg_to_char(ch, "You must specify whether you want to set or remove the description.\r\n");
		}
	}
	else {
		msg_to_char(ch, "You can customize room name or description.\r\n");
	}
}


ACMD(do_dedicate) {
	char arg1[MAX_INPUT_LENGTH], arg2[MAX_INPUT_LENGTH];
	vehicle_data *ded_veh = NULL;
	room_data *ded_room = NULL;
	player_index_data *index;
	
	two_arguments(argument, arg1, arg2);
	
	if (IS_NPC(ch) || !has_permission(ch, PRIV_CUSTOMIZE, IN_ROOM(ch))) {
		msg_to_char(ch, "You don't have permission to dedicate anything (customize).\r\n");
		return;
	}
	if (!*arg1) {
		msg_to_char(ch, "Usage: dedicate <building | vehicle> <person>\r\n");
		return;
	}
	
	// determine what they're trying to dedicate
	if (is_abbrev(arg1, "building") || is_abbrev(arg1, "room") || isname(arg1, skip_filler(get_room_name(HOME_ROOM(IN_ROOM(ch)), FALSE)))) {
		if (GET_ROOM_VEHICLE(IN_ROOM(ch))) {
			// actually dedicate the vehicle we're in
			ded_veh = GET_ROOM_VEHICLE(IN_ROOM(ch));
		}
		else {
			ded_room = IN_ROOM(ch);
		}
	}
	else if (!generic_find(arg1, NULL, FIND_VEHICLE_ROOM | FIND_VEHICLE_INSIDE, ch, NULL, NULL, &ded_veh)) {
		msg_to_char(ch, "You don't see %s %s here to dedicate.\r\n", AN(arg1), arg1);
		return;
	}
	
	// validate dedicate: room
	if (ded_room && ROOM_PATRON(ded_room) > 0) {
		msg_to_char(ch, "It is already dedicated.\r\n");
		return;
	}
	if (ded_room && GET_LOYALTY(ch) != ROOM_OWNER(HOME_ROOM(ded_room))) {
		msg_to_char(ch, "You can only dedicate it if you own it.\r\n");
		return;
	}
	if (ded_room && !ROOM_BLD_FLAGGED(ded_room, BLD_DEDICATE)) {
		msg_to_char(ch, "You cannot dedicate anything here.\r\n");
		return;
	}
	
	// validate dedicate: vehicle
	if (ded_veh && VEH_PATRON(ded_veh) > 0) {
		msg_to_char(ch, "It is already dedicated.\r\n");
		return;
	}
	if (ded_veh && !can_use_vehicle(ch, ded_veh, MEMBERS_ONLY)) {
		act("You need to own $V to dedicate it.", FALSE, ch, NULL, ded_veh, TO_CHAR | ACT_VEH_VICT);
		return;
	}
	if (ded_veh && !VEH_FLAGGED(ded_veh, VEH_DEDICATE)) {
		act("You cannot dedicate $V.", FALSE, ch, NULL, ded_veh, TO_CHAR | ACT_VEH_VICT);
		return;
	}
	
	// validate dedicate: general
	if ((ded_room && !IS_COMPLETE(ded_room)) || (ded_veh && !VEH_IS_COMPLETE(ded_veh))) {
		msg_to_char(ch, "Finish building it before you dedicate it.\r\n");
		return;
	}
	if ((ded_room && IS_DISMANTLING(ded_room)) || (ded_veh && VEH_IS_DISMANTLING(ded_veh))) {
		msg_to_char(ch, "You can't dedicate that while it is being dismantled.\r\n");
		return;
	}
	
	// ok find/validate the player target
	if (!*arg2) {
		msg_to_char(ch, "Dedicate this building to whom?\r\n");
		return;
	}
	if (!(index = find_player_index_by_name(arg2))) {
		msg_to_char(ch, "You must specify a valid player to dedicate it to.\r\n");
		return;
	}
	
	// success:
	if (ded_room) {
		msg_to_char(ch, "You dedicate the building to %s!\r\n", index->fullname);
		sprintf(buf, "$n dedicates the building to %s!", index->fullname);
		set_room_extra_data(ded_room, ROOM_EXTRA_DEDICATE_ID, index->idnum);
		
		snprintf(buf, sizeof(buf), "%s of %s", get_room_name(ded_room, FALSE), index->fullname);

		// grant them hide-real-name for this ONLY if it's not already renamed
		if (!ROOM_CUSTOM_NAME(ded_room)) {
			SET_BIT(ROOM_BASE_FLAGS(ded_room), ROOM_AFF_HIDE_REAL_NAME);
		}
		set_room_custom_name(ded_room, buf);
		affect_total_room(ded_room);
	}
	if (ded_veh) {
		snprintf(buf, sizeof(buf), "You dedicate $V to %s!", index->fullname);
		act(buf, FALSE, ch, NULL, ded_veh, TO_CHAR | ACT_VEH_VICT);
		snprintf(buf, sizeof(buf), "$n dedicates $V to %s!", index->fullname);
		act(buf, FALSE, ch, NULL, ded_veh, TO_ROOM | ACT_VEH_VICT);
		set_vehicle_extra_data(ded_veh, ROOM_EXTRA_DEDICATE_ID, index->idnum);
		
		// update strs:
		
		// keywords
		snprintf(buf, sizeof(buf), "%s %s", VEH_KEYWORDS(ded_veh), index->fullname);
		set_vehicle_keywords(ded_veh, buf);
		
		// short desc
		snprintf(buf, sizeof(buf), "%s of %s", VEH_SHORT_DESC(ded_veh), index->fullname);
		set_vehicle_short_desc(ded_veh, buf);
	}
}


// Takes subcmd SCMD_DESIGNATE, SCMD_REDESIGNATE
ACMD(do_designate) {
	struct empire_territory_data *ter;
	struct room_direction_data *ex;
	int dir = NO_DIR;
	room_data *new, *home = HOME_ROOM(IN_ROOM(ch));
	bld_data *bld, *next_bld;
	char_data *vict;
	bld_data *type;
	bool found, veh_dirs;
	
	vehicle_data *veh = NULL;	// if this is set, we're doing a vehicle designate instead of building
	bitvector_t valid_des_flags = NOBITS;
	int maxrooms = 0, hasrooms = 0;

	/*
	 * arg = direction (designate only)
	 * argument = room type
	 */
	
	if (subcmd == SCMD_DESIGNATE) {
		argument = one_argument(argument, arg);
	}

	skip_spaces(&argument);
	
	// detect based on vehicle or building
	veh = GET_ROOM_VEHICLE(home);
	maxrooms = veh ? VEH_MAX_ROOMS(veh) : BLD_MAX_ROOMS(home);
	valid_des_flags = veh ? VEH_DESIGNATE_FLAGS(veh) : (GET_BUILDING(home) ? GET_BLD_DESIGNATE_FLAGS(GET_BUILDING(home)) : NOBITS);
	hasrooms = veh ? VEH_INSIDE_ROOMS(veh) : GET_INSIDE_ROOMS(home);
	veh_dirs = veh && (!VEH_FLAGGED(veh, VEH_BUILDING) || VEH_FLAGGED(veh, MOVABLE_VEH_FLAGS));

	if (!IS_APPROVED(ch) && config_get_bool("build_approval")) {
		send_config_msg(ch, "need_approval_string");
	}
	else if (!*argument || !(type = find_designate_room_by_name(argument, valid_des_flags))) {
		if (*argument) {	// if any
			msg_to_char(ch, "You can't designate %s '%s' here.\r\n", AN(argument), argument);
		}
		msg_to_char(ch, "Usage: %s <room>\r\n", (subcmd == SCMD_REDESIGNATE) ? "redesignate" : "designate <direction>");

		if (!ROOM_IS_CLOSED(IN_ROOM(ch)) && (subcmd == SCMD_DESIGNATE) && GET_INSIDE_ROOMS(home) >= maxrooms) {
			msg_to_char(ch, "You can't designate any new rooms here.\r\n");
		}
		else {
			msg_to_char(ch, "Valid rooms are: ");
		
			found = FALSE;
			HASH_ITER(hh, building_table, bld, next_bld) {
				if (IS_SET(GET_BLD_FLAGS(bld), BLD_ROOM) && GET_BLD_DESIGNATE_FLAGS(bld) != NOBITS) {
					if (!ROOM_IS_CLOSED(IN_ROOM(ch)) || IS_SET(valid_des_flags, GET_BLD_DESIGNATE_FLAGS(bld))) {
						msg_to_char(ch, "%s%s", (found ? ", " : ""), GET_BLD_NAME(bld));
						found = TRUE;
					}
				}
			}
		
			msg_to_char(ch, "\r\n");
		}
	}
	else if (!ROOM_IS_CLOSED(IN_ROOM(ch))) {
		msg_to_char(ch, "You can't designate rooms here!\r\n");
	}
	else if (maxrooms <= 0) {
		msg_to_char(ch, "You can't designate here.\r\n");
	}
	else if (subcmd == SCMD_DESIGNATE && hasrooms >= maxrooms) {
		msg_to_char(ch, "There's no more free space.\r\n");
	}
	else if (subcmd == SCMD_DESIGNATE && ((dir = parse_direction(ch, arg)) == NO_DIR || !(veh_dirs ? can_designate_dir_vehicle[dir] : can_designate_dir[dir]))) {
		msg_to_char(ch, "Invalid direction.\r\n");
		msg_to_char(ch, "Usage: %s <room>\r\n", subcmd == SCMD_REDESIGNATE ? "redesignate" : "designate <direction>");
	}
	else if (!has_permission(ch, PRIV_BUILD, IN_ROOM(ch)) || !can_use_room(ch, IN_ROOM(ch), MEMBERS_ONLY)) {
		msg_to_char(ch, "You don't have permission to designate rooms here.\r\n");
	}
	else if (subcmd == SCMD_DESIGNATE && IS_MAP_BUILDING(IN_ROOM(ch)) && dir != BUILDING_ENTRANCE(IN_ROOM(ch))) {
		msg_to_char(ch, "You may only designate %s from here.\r\n", dirs[get_direction_for_char(ch, BUILDING_ENTRANCE(IN_ROOM(ch)))]);
	}
	else if (subcmd == SCMD_REDESIGNATE && ROOM_AFF_FLAGGED(IN_ROOM(ch), ROOM_AFF_HAS_INSTANCE)) {
		msg_to_char(ch, "You can't redesignate because an adventure is currently linked here.\r\n");
	}
	else if (!IS_COMPLETE(IN_ROOM(ch))) {
		msg_to_char(ch, "You need to finish the building before you can designate rooms.\r\n");
	}
	else if (!IS_INSIDE(IN_ROOM(ch)) && subcmd == SCMD_REDESIGNATE) {
		msg_to_char(ch, "You can't redesignate here.\r\n");
	}
	else if (subcmd == SCMD_REDESIGNATE && !IS_SET(valid_des_flags, GET_BLD_DESIGNATE_FLAGS(GET_BUILDING(IN_ROOM(ch))))) {
		// room the character is in does not match the valid flags for the building/vehicle
		msg_to_char(ch, "You can't redesignate this %s.\r\n", veh ? "part" : "room");
	}
	else if (subcmd == SCMD_REDESIGNATE && get_room_extra_data(IN_ROOM(ch), ROOM_EXTRA_REDESIGNATE_TIME) + (config_get_int("redesignate_time") * SECS_PER_REAL_MIN) > time(0)) {
		msg_to_char(ch, "You can't redesignate this %s so soon.\r\n", veh ? "part" : "room");
	}
	else if (subcmd == SCMD_DESIGNATE && (ex = find_exit(IN_ROOM(ch), dir)) && ex->room_ptr) {
		msg_to_char(ch, "There is already a room that direction.\r\n");
	}
	else if (GET_BLD_DESIGNATE_FLAGS(type) == NOBITS) {
		msg_to_char(ch, "You can't designate that type of room!\r\n");
	}
	else if (!IS_SET(valid_des_flags, GET_BLD_DESIGNATE_FLAGS(type))) {
		msg_to_char(ch, "You can't designate that here!\r\n");
	}
	else if (subcmd == SCMD_REDESIGNATE && !dismantle_wtrigger(IN_ROOM(ch), ch, TRUE)) {
		return;	// this goes last
	}
	else {
		if (subcmd == SCMD_REDESIGNATE) {
			// redesignate this room
			new = IN_ROOM(ch);
			
			if (ROOM_OWNER(home) && (ter = find_territory_entry(ROOM_OWNER(home), new))) {
				while (ter->npcs) {
					make_citizen_homeless(ROOM_OWNER(home), ter->npcs);
					delete_territory_npc(ter, ter->npcs);
				}
			}
			
			detach_building_from_room(new);
			attach_building_to_room(type, new, TRUE);
		}
		else {
			// create the new room
			new = create_room(home);
			create_exit(IN_ROOM(ch), new, dir, TRUE);
			attach_building_to_room(type, new, TRUE);

			COMPLEX_DATA(home)->inside_rooms++;
			
			if (veh) {
				add_room_to_vehicle(new, veh);
			}
			
			if (ROOM_OWNER(home)) {
				perform_claim_room(new, ROOM_OWNER(home));
			}
		}
		
		set_room_extra_data(new, ROOM_EXTRA_REDESIGNATE_TIME, time(0));

		/* send messages */
		if (subcmd == SCMD_REDESIGNATE) {
			msg_to_char(ch, "You redesignate this room as %s %s.\r\n", AN(GET_BLD_NAME(type)), GET_BLD_NAME(type));
			sprintf(buf, "$n redesignates this room as %s %s.", AN(GET_BLD_NAME(type)), GET_BLD_NAME(type));
			act(buf, FALSE, ch, 0, 0, TO_ROOM);
		}
		else {
			msg_to_char(ch, "You designate the %s %s as %s %s.\r\n", veh ? "area" : "room", dirs[get_direction_for_char(ch, dir)], AN(GET_BLD_NAME(type)), GET_BLD_NAME(type));
			
			DL_FOREACH2(ROOM_PEOPLE(IN_ROOM(ch)), vict, next_in_room) {
				if (vict != ch && vict->desc) {
					sprintf(buf, "$n designates the %s %s as %s %s.", veh ? "area" : "room", dirs[get_direction_for_char(vict, dir)], AN(GET_BLD_NAME(type)), GET_BLD_NAME(type));
					act(buf, FALSE, ch, 0, vict, TO_VICT);
				}
			}
		}
		
		complete_wtrigger(new);
	}
}


ACMD(do_interlink) {
	char arg2[MAX_INPUT_LENGTH];
	room_vnum vnum;
	room_data *to_room;
	int dir;
	
	half_chop(argument, arg, arg2);
	
	if (GET_ROOM_VEHICLE(IN_ROOM(ch)) && !VEH_FLAGGED(GET_ROOM_VEHICLE(IN_ROOM(ch)), VEH_INTERLINK)) {
		msg_to_char(ch, "This %s cannot be interlinked.\r\n", VEH_OR_BLD(GET_ROOM_VEHICLE(IN_ROOM(ch))));
	}
	else if (!GET_ROOM_VEHICLE(IN_ROOM(ch)) && GET_BUILDING(IN_ROOM(ch)) && !ROOM_BLD_FLAGGED(HOME_ROOM(IN_ROOM(ch)), BLD_INTERLINK)) {
		msg_to_char(ch, "This building cannot be interlinked.\r\n");
	}
	else if (!*arg || !*arg2) {
		msg_to_char(ch, "Usage: interlink <direction> <room code>\r\n");
		
		if (IS_INSIDE(IN_ROOM(ch))) {
			msg_to_char(ch, "This room's code is: %s\r\n", vnum_to_interlink(GET_ROOM_VNUM(IN_ROOM(ch))));
		}
		else {
			msg_to_char(ch, "You can't interlink to this room -- only to the additional interior rooms of buildings.\r\n");
		}
	}
	else if (!IS_INSIDE(IN_ROOM(ch))) {
		msg_to_char(ch, "You can only interlink the additional interior rooms of buildings.\r\n");
	}
	else if (!IS_COMPLETE(IN_ROOM(ch))) {
		msg_to_char(ch, "You must finish building the room before you can interlink it.\r\n");
	}
	else if (!has_permission(ch, PRIV_BUILD, IN_ROOM(ch)) || !can_use_room(ch, IN_ROOM(ch), MEMBERS_ONLY)) {
		msg_to_char(ch, "You don't have permission to interlink rooms here.\r\n");
	}
	else if ((dir = parse_direction(ch, arg)) == NO_DIR || !can_designate_dir[dir]) {
		msg_to_char(ch, "Invalid direction.\r\n");
	}
	else if (find_exit(IN_ROOM(ch), dir)) {
		msg_to_char(ch, "There is already a room in that direction.\r\n");
	}
	else if ((vnum = interlink_to_vnum(arg2)) < 0 || !(to_room = real_room(vnum))) {
		msg_to_char(ch, "Invalid interlink code.\r\n");
	}
	else if (!IS_INSIDE(to_room)) {
		msg_to_char(ch, "You can only interlink to an interior room of a building.\r\n");
	}
	else if (!can_use_room(ch, to_room, MEMBERS_ONLY)) {
		msg_to_char(ch, "You don't have permission to interlink to that building.\r\n");
	}
	else if (HOME_ROOM(IN_ROOM(ch)) == HOME_ROOM(to_room)) {
		msg_to_char(ch, "You can only use interlink to connect this building to a different one.\r\n");
	}
	else if (GET_ROOM_VEHICLE(to_room) && !VEH_FLAGGED(GET_ROOM_VEHICLE(to_room), VEH_INTERLINK)) {
		msg_to_char(ch, "That %s cannot be interlinked.\r\n", VEH_OR_BLD(GET_ROOM_VEHICLE(to_room)));
	}
	else if (!GET_ROOM_VEHICLE(to_room) && !ROOM_BLD_FLAGGED(HOME_ROOM(to_room), BLD_INTERLINK)) {
		msg_to_char(ch, "That building cannot be interlinked.\r\n");
	}
	else if (!IS_COMPLETE(to_room)) {
		msg_to_char(ch, "You must finish building the target room before you can interlink it.\r\n");
	}
	else if (find_exit(to_room, rev_dir[dir])) {
		msg_to_char(ch, "The target room already has an exit in that direction.\r\n");
	}
	else if (compute_distance(IN_ROOM(ch), to_room) > config_get_int("interlink_distance")) {
		msg_to_char(ch, "You can't interlink to places more than %d tiles away.\r\n", config_get_int("interlink_distance"));
	}
	else if (count_flagged_sect_between(SECTF_FRESH_WATER, IN_ROOM(ch), to_room, TRUE) > config_get_int("interlink_river_limit")) {
		msg_to_char(ch, "You can't interlink to there; there's too much water in the way.\r\n");
	}
	else if (count_flagged_sect_between(SECTF_ROUGH, IN_ROOM(ch), to_room, TRUE) > config_get_int("interlink_mountain_limit")) {
		msg_to_char(ch, "You can't interlink to there; the terrain is too rough.\r\n");
	}
	else {
		// success!
		create_exit(IN_ROOM(ch), to_room, dir, TRUE);
		
		msg_to_char(ch, "You interlink %s to %s%s.\r\n", dirs[get_direction_for_char(ch, dir)], get_room_name(to_room, FALSE), coord_display_room(ch, to_room, FALSE));
		act("$n interlinks the room with another building.", FALSE, ch, NULL, NULL, TO_ROOM);
		
		if (ROOM_PEOPLE(to_room)) {
			act("The room is now interlinked to another building.", FALSE, ROOM_PEOPLE(to_room), NULL, NULL, TO_CHAR | TO_ROOM);
		}
	}
}


ACMD(do_lay) {
	static struct resource_data *cost = NULL;
	sector_data *original_sect = SECT(IN_ROOM(ch));
	sector_data *road_sect = find_first_matching_sector(SECTF_IS_ROAD, NOBITS, get_climate(IN_ROOM(ch)));
	struct resource_data *charged = NULL;
	
	if (!cost) {
		add_to_resource_list(&cost, RES_COMPONENT, COMP_ROCK, 20, 0);
	}

	if (IS_NPC(ch)) {
		msg_to_char(ch, "NPCs can't lay roads.\r\n");
	}
	else if (!IS_APPROVED(ch) && config_get_bool("build_approval")) {
		send_config_msg(ch, "need_approval_string");
	}
	else if (GET_ACTION(ch) != ACT_NONE) {
		msg_to_char(ch, "You're a little busy right now.\r\n");
	}
	else if (ROOM_AFF_FLAGGED(IN_ROOM(ch), ROOM_AFF_HAS_INSTANCE)) {
		msg_to_char(ch, "You can't do that here.\r\n");
	}
	else if (!can_use_room(ch, IN_ROOM(ch), MEMBERS_ONLY))
		msg_to_char(ch, "You can't lay road in someone else's territory!\r\n");
	else if (!has_permission(ch, PRIV_BUILD, IN_ROOM(ch)))
		msg_to_char(ch, "You don't have permission to lay road.\r\n");
	else if (ROOM_SECT_FLAGGED(IN_ROOM(ch), SECTF_IS_ROAD)) {
		// already a road -- attempt to un-lay it
		if (COMPLEX_DATA(IN_ROOM(ch)) && GET_BUILT_WITH(IN_ROOM(ch))) {
			give_resources(ch, GET_BUILT_WITH(IN_ROOM(ch)), TRUE);
		}
		
		msg_to_char(ch, "You pull up the road.\r\n");
		act("$n pulls up the road.", FALSE, ch, 0, 0, TO_ROOM);
		
		// this will tear it back down to its base type
		disassociate_building(IN_ROOM(ch));
		command_lag(ch, WAIT_OTHER);
		request_world_save(GET_ROOM_VNUM(IN_ROOM(ch)), WSAVE_ROOM);
	}
	else if (!road_sect) {
		msg_to_char(ch, "Road data has not been set up for this game.\r\n");
	}
	else if (!ROOM_SECT_FLAGGED(IN_ROOM(ch), SECTF_LAY_ROAD))
		msg_to_char(ch, "You can't lay a road here!\r\n");
	else if (!has_resources(ch, cost, TRUE, TRUE, NULL)) {
		// sends own messages
	}
	else {
		extract_resources(ch, cost, TRUE, &charged);

		msg_to_char(ch, "You lay a road here.\r\n");
		act("$n lays a road here.", FALSE, ch, 0, 0, TO_ROOM);
				
		// change it over
		change_terrain(IN_ROOM(ch), GET_SECT_VNUM(road_sect), GET_SECT_VNUM(original_sect));
		
		// log charged resources
		if (charged) {
			if (!COMPLEX_DATA(IN_ROOM(ch))) {
				COMPLEX_DATA(IN_ROOM(ch)) = init_complex_data();
			}
			GET_BUILT_WITH(IN_ROOM(ch)) = charged;
		}
		
		if (ROOM_OWNER(IN_ROOM(ch))) {
			deactivate_workforce_room(ROOM_OWNER(IN_ROOM(ch)), IN_ROOM(ch));
		}
		
		command_lag(ch, WAIT_OTHER);
		request_world_save(GET_ROOM_VNUM(IN_ROOM(ch)), WSAVE_ROOM);
	}
}


ACMD(do_maintain) {
	char arg[MAX_INPUT_LENGTH];
	vehicle_data *veh;
	
	one_argument(argument, arg);
	
	if ((GET_ACTION(ch) == ACT_REPAIR_VEHICLE || GET_ACTION(ch) == ACT_MAINTENANCE) && !*arg) {
		act("You stop the repairs.", FALSE, ch, NULL, NULL, TO_CHAR);
		cancel_action(ch);
	}
	else if (GET_ACTION(ch) != ACT_NONE) {
		msg_to_char(ch, "You're busy right now.\r\n");
	}
	else if (*arg && (veh = get_vehicle_in_room_vis(ch, arg, NULL))) {
		// MAINTAIN VEHICLE
		if (!can_use_vehicle(ch, veh, MEMBERS_AND_ALLIES)) {
			msg_to_char(ch, "You can't repair something that belongs to someone else.\r\n");
		}
		else if (VEH_IS_DISMANTLING(veh)) {
			msg_to_char(ch, "You can't repair something that is being dismantled.\r\n");
		}
		else if (!VEH_IS_COMPLETE(veh)) {
			msg_to_char(ch, "You can only repair %ss that are finished.\r\n", VEH_OR_BLD(veh));
		}
		else if (!VEH_NEEDS_RESOURCES(veh) && VEH_HEALTH(veh) >= VEH_MAX_HEALTH(veh)) {
			msg_to_char(ch, "It doesn't need maintenance.\r\n");
		}
		else if (VEH_FLAGGED(veh, VEH_ON_FIRE)) {
			msg_to_char(ch, "You can't repair it while it's on fire!\r\n");
		}
		else if (!can_see_in_dark_room(ch, IN_ROOM(ch), TRUE)) {
			msg_to_char(ch, "It's too dark to repair anything here.\r\n");
		}
		else if (!vehicle_allows_climate(veh, IN_ROOM(veh), NULL)) {
			msg_to_char(ch, "You can't repair it -- it's falling into disrepair because it's in the wrong terrain.\r\n");
		}
		else {
			start_action(ch, ACT_REPAIR_VEHICLE, -1);
			GET_ACTION_VNUM(ch, 0) = veh_script_id(veh);
			act("You begin to repair $V.", FALSE, ch, NULL, veh, TO_CHAR | ACT_VEH_VICT);
			act("$n begins to repair $V.", FALSE, ch, NULL, veh, TO_ROOM | ACT_VEH_VICT);
		}
	}
	else if (*arg && !multi_isname(arg, get_room_name(IN_ROOM(ch), FALSE))) {
		msg_to_char(ch, "You don't see that here.\r\n");
	}
	else {
		// MAINTAIN CURRENT ROOM
		if (!can_use_room(ch, IN_ROOM(ch), GUESTS_ALLOWED)) {
			msg_to_char(ch, "You can't perform maintenance here.\r\n");
		}
		else if (!BUILDING_RESOURCES(IN_ROOM(ch)) && BUILDING_DAMAGE(IN_ROOM(ch)) == 0) {
			msg_to_char(ch, "It doesn't need any maintenance.\r\n");
		}
		else if (IS_INCOMPLETE(IN_ROOM(ch))) {
			msg_to_char(ch, "Use 'build' to finish the building instead.\r\n");
		}
		else if (IS_DISMANTLING(IN_ROOM(ch))) {
			msg_to_char(ch, "This building is being dismantled. Use 'dismantle' to continue instead.\r\n");
		}
		else if (IS_BURNING(IN_ROOM(ch))) {
			msg_to_char(ch, "You can't maintain a building that's on fire!\r\n");
		}
		else if (!can_see_in_dark_room(ch, IN_ROOM(ch), TRUE)) {
			msg_to_char(ch, "It's too dark to maintain anything here.\r\n");
		}
		else {
			start_action(ch, ACT_MAINTENANCE, -1);
			act("You set to work maintaining the building.", FALSE, ch, NULL, NULL, TO_CHAR);
			act("$n begins maintaining the building.", FALSE, ch, NULL, NULL, TO_ROOM);
		}
	}
}


ACMD(do_nodismantle) {
	if (!IS_APPROVED(ch) && config_get_bool("manage_empire_approval")) {
		send_config_msg(ch, "need_approval_string");
	}
	else if (!can_use_room(ch, IN_ROOM(ch), MEMBERS_ONLY)) {
		msg_to_char(ch, "Your empire doesn't own this tile.\r\n");
	}
	else if (!IS_ANY_BUILDING(IN_ROOM(ch))) {
		msg_to_char(ch, "You can only toggle nodismantle in buildings.\r\n");
	}
	else if (!has_permission(ch, PRIV_BUILD, IN_ROOM(ch))) {
		msg_to_char(ch, "You don't have permission to do that.\r\n");
	}
	else if (ROOM_AFF_FLAGGED(HOME_ROOM(IN_ROOM(ch)), ROOM_AFF_NO_DISMANTLE)) {
		REMOVE_BIT(ROOM_BASE_FLAGS(HOME_ROOM(IN_ROOM(ch))), ROOM_AFF_NO_DISMANTLE);
		affect_total_room(HOME_ROOM(IN_ROOM(ch)));
		msg_to_char(ch, "This building can now be dismantled.\r\n");
	}
	else {
		SET_BIT(ROOM_BASE_FLAGS(HOME_ROOM(IN_ROOM(ch))), ROOM_AFF_NO_DISMANTLE);
		affect_total_room(HOME_ROOM(IN_ROOM(ch)));
		msg_to_char(ch, "This building can no longer be dismantled.\r\n");
	}
}


ACMD(do_paint) {
	char arg1[MAX_INPUT_LENGTH], arg2[MAX_INPUT_LENGTH];
	vehicle_data *paint_veh = NULL;
	room_data *paint_room = NULL;
	obj_data *paint;
	
	two_arguments(argument, arg1, arg2);
	
	if (IS_NPC(ch) || !has_permission(ch, PRIV_CUSTOMIZE, IN_ROOM(ch))) {
		msg_to_char(ch, "You don't have permission to paint anything (customize).\r\n");
		return;
	}
	if (!*arg1 || !*arg2) {
		msg_to_char(ch, "Usage: paint <building | vehicle> <paint item to use>\r\n");
		return;
	}
	
	// determine what they're trying to paint
	if (is_abbrev(arg1, "building") || is_abbrev(arg1, "room") || isname(arg1, skip_filler(get_room_name(HOME_ROOM(IN_ROOM(ch)), FALSE)))) {
		if (GET_ROOM_VEHICLE(IN_ROOM(ch))) {
			// actually paint the vehicle we're in
			paint_veh = GET_ROOM_VEHICLE(IN_ROOM(ch));
		}
		else {
			paint_room = HOME_ROOM(IN_ROOM(ch));
		}
	}
	else if (!generic_find(arg1, NULL, FIND_VEHICLE_ROOM | FIND_VEHICLE_INSIDE, ch, NULL, NULL, &paint_veh)) {
		msg_to_char(ch, "You don't see %s %s here to paint.\r\n", AN(arg1), arg1);
		return;
	}
	
	// validate painting: room
	if (paint_room && (!can_use_room(ch, paint_room, MEMBERS_ONLY) || ROOM_AFF_FLAGGED(paint_room, ROOM_AFF_UNCLAIMABLE))) {
		msg_to_char(ch, "You don't have permission to paint it.\r\n");
		return;
	}
	if (paint_room && (!IS_ANY_BUILDING(paint_room) || ROOM_AFF_FLAGGED(paint_room, ROOM_AFF_PERMANENT_PAINT) || ROOM_BLD_FLAGGED(paint_room, BLD_NO_PAINT) || ROOM_AFF_FLAGGED(paint_room, ROOM_AFF_TEMPORARY))) {
		msg_to_char(ch, "You can't paint that.\r\n");
		return;
	}
	
	// validate painting: vehicle
	if (paint_veh && !can_use_vehicle(ch, paint_veh, MEMBERS_ONLY)) {
		act("You don't have permission to paint $V.", FALSE, ch, NULL, paint_veh, TO_CHAR | ACT_VEH_VICT);
		return;
	}
	if (paint_veh && (!VEH_ICON(paint_veh) || VEH_FLAGGED(paint_veh, VEH_NO_PAINT))) {
		act("You cannot paint $V.", FALSE, ch, NULL, paint_veh, TO_CHAR | ACT_VEH_VICT);
		return;
	}
	
	// validate painting: general
	if ((paint_room && !IS_COMPLETE(paint_room)) || (paint_veh && !VEH_IS_COMPLETE(paint_veh))) {
		msg_to_char(ch, "Finish building it before painting it.\r\n");
		return;
	}
	if ((paint_room && IS_DISMANTLING(paint_room)) || (paint_veh && VEH_IS_DISMANTLING(paint_veh))) {
		msg_to_char(ch, "You can't paint that while it is being dismantled.\r\n");
		return;
	}
	
	// ok find/validate the paint object
	if (!(paint = get_obj_in_list_vis(ch, arg2, NULL, ch->carrying))) {
		msg_to_char(ch, "You don't seem to have that paint with.\r\n");
		return;
	}
	if (!IS_PAINT(paint)) {
		act("$p isn't paint!", FALSE, ch, paint, NULL, TO_CHAR);
		return;
	}
	if (!consume_otrigger(paint, ch, OCMD_PAINT, NULL)) {
		return;	// check trigger
	}
	
	// SUCCESS
	if (paint_room) {
		act("You use $p to paint the building!", FALSE, ch, paint, NULL, TO_CHAR);
		act("$n uses $p to paint the building!", FALSE, ch, paint, NULL, TO_ROOM);
		
		// brighten if same color or remove bright if not
		if (ROOM_PAINT_COLOR(paint_room) == GET_PAINT_COLOR(paint)) {
			SET_BIT(ROOM_BASE_FLAGS(paint_room), ROOM_AFF_BRIGHT_PAINT);
		}
		else {
			REMOVE_BIT(ROOM_BASE_FLAGS(paint_room), ROOM_AFF_BRIGHT_PAINT);
		}
		
		set_room_extra_data(paint_room, ROOM_EXTRA_PAINT_COLOR, GET_PAINT_COLOR(paint));
	}
	if (paint_veh) {
		act("You use $p to paint $V!", FALSE, ch, paint, paint_veh, TO_CHAR | ACT_VEH_VICT);
		act("$n uses $p to paint $V!", FALSE, ch, paint, paint_veh, TO_ROOM | ACT_VEH_VICT);
		
		// brighten if same color or remove bright if not
		if (VEH_PAINT_COLOR(paint_veh) == GET_PAINT_COLOR(paint)) {
			set_vehicle_flags(paint_veh, VEH_BRIGHT_PAINT);
		}
		else {
			remove_vehicle_flags(paint_veh, VEH_BRIGHT_PAINT);
		}
		
		set_vehicle_extra_data(paint_veh, ROOM_EXTRA_PAINT_COLOR, GET_PAINT_COLOR(paint));
	}
	
	if (PRF_FLAGGED(ch, PRF_NO_PAINT)) {
		msg_to_char(ch, "Notice: You have no-paint toggled on, and won't be able to see the color.\r\n");
	}
	
	command_lag(ch, WAIT_ABILITY);
	affect_total_room(IN_ROOM(ch));
	run_interactions(ch, GET_OBJ_INTERACTIONS(paint), INTERACT_CONSUMES_TO, paint_room, NULL, paint, paint_veh, consumes_or_decays_interact);
	extract_obj(paint);
}


ACMD(do_tunnel) {
	bitvector_t exit_bld_flags = BLD_ON_PLAINS | BLD_ANY_FOREST | BLD_ON_DESERT | BLD_FACING_CROP | BLD_ON_GROVE | BLD_FACING_OPEN_BUILDING;
	bitvector_t mountain_bld_flags = BLD_ON_MOUNTAIN;

	room_data *entrance, *exit = NULL, *to_room, *last_room = NULL, *past_exit;
	int dir, iter, length;
	bool error;
	
	one_argument(argument, arg);
	
	if (!has_permission(ch, PRIV_BUILD, IN_ROOM(ch))) {
		msg_to_char(ch, "You do not have permission to build anything.\r\n");
	}
	else if (!GET_LOYALTY(ch) || !EMPIRE_HAS_TECH(GET_LOYALTY(ch), TECH_TUNNELS)) {
		msg_to_char(ch, "You must be in an empire with the technology to make tunnels to do that.\r\n");
	}
	else if (!can_build_on(IN_ROOM(ch), exit_bld_flags)) {
		ordered_sprintbit(exit_bld_flags, bld_on_flags, bld_on_flags_order, TRUE, buf);
		msg_to_char(ch, "You must start the tunnel from: %s\r\n", buf);
	}
	else if (!can_use_room(ch, IN_ROOM(ch), MEMBERS_AND_ALLIES)) {
		msg_to_char(ch, "You don't have permission to start a tunnel here.\r\n");
	}
	else if (!*arg) {
		msg_to_char(ch, "Usage: tunnel <direction>\r\n");
	}
	else if ((dir = parse_direction(ch, arg)) == NO_DIR || dir >= NUM_2D_DIRS) {
		msg_to_char(ch, "Invalid tunnel direction '%s'.\r\n", arg);
	}
	else if (!(entrance = real_shift(IN_ROOM(ch), shift_dir[dir][0], shift_dir[dir][1])) || !can_build_on(entrance, mountain_bld_flags)) {
		ordered_sprintbit(mountain_bld_flags, bld_on_flags, bld_on_flags_order, TRUE, buf);
		msg_to_char(ch, "You must have the entrance on: %s\r\n", buf);
	}
	else if (!can_use_room(ch, entrance, MEMBERS_ONLY)) {
		msg_to_char(ch, "You don't have permission to build the tunnel entrance.\r\n");
	}
	else {
		error = FALSE;
		length = 0;	// count intervening rooms
		
		// start 2 room away and follow the direction till we find an exit (we already have the entrance)
		for (iter = 2; !error && !exit && iter < MAX(MAP_WIDTH, MAP_HEIGHT); ++iter) {
			to_room = real_shift(IN_ROOM(ch), iter * shift_dir[dir][0], iter * shift_dir[dir][1]);
			
			if (!to_room) {
				error = TRUE;
				break;
			}

			// found a non-rough
			if (!SECT_FLAGGED(BASE_SECT(to_room), SECTF_ROUGH)) {
				// did we at least have a last room?
				if (last_room) {
					exit = last_room;
				}
				else {
					error = TRUE;
				}
			}

			++length;
			last_room = to_room;
		}
		
		// length goes slightly too far in that it counts the exit and the room after that
		length -= 2;
		
		// now more error-checking
		if (error || !exit || length < 0) {
			msg_to_char(ch, "You are unable to tunnel through the mountain here.\r\n");
		}
		else if (!can_build_on(exit, mountain_bld_flags)) {
			ordered_sprintbit(mountain_bld_flags, bld_on_flags, bld_on_flags_order, TRUE, buf);
			msg_to_char(ch, "You must have the exit on: %s (something may be in the way)\r\n", buf);
		}
		else if (!can_use_room(ch, exit, MEMBERS_ONLY)) {
			msg_to_char(ch, "You don't have permission to build at the tunnel exit.\r\n");
		}
		else if (!(past_exit = real_shift(exit, shift_dir[dir][0], shift_dir[dir][1]))) {
			msg_to_char(ch, "You are unable to tunnel through the mountain here.\r\n");
		}
		else if (!can_use_room(ch, past_exit, MEMBERS_AND_ALLIES)) {
			msg_to_char(ch, "You don't have permission to tunnel through the other side.\r\n");
		}
		else if (!can_build_on(past_exit, exit_bld_flags)) {
			ordered_sprintbit(exit_bld_flags, bld_on_flags, bld_on_flags_order, TRUE, buf);
			msg_to_char(ch, "You must end the tunnel on: %s\r\n", buf);
		}
		else {
			// ok, seems like we're good to go.
			construct_tunnel(ch, dir, entrance, exit, length);
			msg_to_char(ch, "You designate work to begin on a tunnel to the %s.\r\n", dirs[get_direction_for_char(ch, dir)]);
			act("$n designates work to begin on a tunnel.", TRUE, ch, NULL, NULL, TO_ROOM);
		}
	}
}


ACMD(do_unpaint) {
	vehicle_data *paint_veh = NULL;
	room_data *paint_room = NULL;
	
	one_argument(argument, arg);
	
	if (IS_NPC(ch) || !has_permission(ch, PRIV_CUSTOMIZE, IN_ROOM(ch))) {
		msg_to_char(ch, "You don't have permission to unpaint anything (customize).\r\n");
		return;
	}
	if (!*arg) {
		msg_to_char(ch, "Usage: unpaint <building | vehicle>\r\n");
		return;
	}
	
	// determine what they're trying to unpaint
	if (is_abbrev(arg, "building") || is_abbrev(arg, "room") || isname(arg, skip_filler(get_room_name(HOME_ROOM(IN_ROOM(ch)), FALSE)))) {
		if (GET_ROOM_VEHICLE(IN_ROOM(ch))) {
			// actually unpaint the vehicle we're in
			paint_veh = GET_ROOM_VEHICLE(IN_ROOM(ch));
		}
		else {
			paint_room = HOME_ROOM(IN_ROOM(ch));
		}
	}
	else if (!generic_find(arg, NULL, FIND_VEHICLE_ROOM | FIND_VEHICLE_INSIDE, ch, NULL, NULL, &paint_veh)) {
		msg_to_char(ch, "You don't see %s %s here to unpaint.\r\n", AN(arg), arg);
		return;
	}
	
	// validate unpainting: room
	if (paint_room && (!can_use_room(ch, paint_room, MEMBERS_ONLY) || ROOM_AFF_FLAGGED(paint_room, ROOM_AFF_UNCLAIMABLE))) {
		msg_to_char(ch, "You don't have permission to unpaint here.\r\n");
		return;
	}
	if (paint_room && !ROOM_PAINT_COLOR(paint_room)) {
		msg_to_char(ch, "It's not even painted.\r\n");
		return;
	}
	
	// validate unpainting: vehicle
	if (paint_veh && !can_use_vehicle(ch, paint_veh, MEMBERS_ONLY)) {
		act("You don't have permission to unpaint $V.", FALSE, ch, NULL, paint_veh, TO_CHAR | ACT_VEH_VICT);
		return;
	}
	if (paint_veh && !VEH_PAINT_COLOR(paint_veh)) {
		act("You cannot paint $V.", FALSE, ch, NULL, paint_veh, TO_CHAR | ACT_VEH_VICT);
		return;
	}
	
	// SUCCESS:
	if (paint_room) {
		act("You strip the paint from the building!", FALSE, ch, NULL, NULL, TO_CHAR);
		act("$n strips the paint from the building!", FALSE, ch, NULL, NULL, TO_ROOM);
		remove_room_extra_data(paint_room, ROOM_EXTRA_PAINT_COLOR);
		REMOVE_BIT(ROOM_BASE_FLAGS(paint_room), ROOM_AFF_BRIGHT_PAINT);
	}
	if (paint_veh) {
		act("You strip the paint from $V!", FALSE, ch, NULL, paint_veh, TO_CHAR | ACT_VEH_VICT);
		act("$n strips the paint from $V!", FALSE, ch, NULL, paint_veh, TO_ROOM | ACT_VEH_VICT);
		remove_vehicle_extra_data(paint_veh, ROOM_EXTRA_PAINT_COLOR);
		remove_vehicle_flags(paint_veh, VEH_BRIGHT_PAINT);
	}
	
	command_lag(ch, WAIT_ABILITY);
	affect_total_room(IN_ROOM(ch));
}


ACMD(do_upgrade) {
	char arg1[MAX_INPUT_LENGTH], *arg2, output[MAX_STRING_LENGTH];
	vehicle_data *from_veh = NULL, *prev_veh, *veh_proto = NULL;
	room_data *from_room = NULL;
	craft_data *find_craft, *to_craft, *missing_abil = NULL;
	struct bld_relation *relat, *lists[2];
	obj_data *found_obj;
	bld_data *bld_proto = NULL;
	bool done, fail;
	int iter, found;
	size_t size;
		
	arg2 = one_argument(argument, arg1);	// what to upgrade
	skip_spaces(&arg2);	// optional: what to upgrade it to
	
	if (IS_NPC(ch) || !has_permission(ch, PRIV_BUILD, IN_ROOM(ch))) {
		msg_to_char(ch, "You don't have permission to upgrade anything (build).\r\n");
		return;
	}
	if (!IS_APPROVED(ch) && config_get_bool("build_approval")) {
		send_config_msg(ch, "need_approval_string");
		return;
	}
	if (!*arg1) {
		msg_to_char(ch, "Usage: upgrade <building | vehicle> [upgrade name]\r\n");
		return;
	}
	
	// arg1: determine what they're trying to upgrade
	if (is_abbrev(arg1, "building") || is_abbrev(arg1, "room") || isname(arg1, skip_filler(get_room_name(HOME_ROOM(IN_ROOM(ch)), FALSE)))) {
		if (GET_ROOM_VEHICLE(IN_ROOM(ch))) {
			// actually upgrade the vehicle we're in
			from_veh = GET_ROOM_VEHICLE(IN_ROOM(ch));
		}
		else {
			from_room = HOME_ROOM(IN_ROOM(ch));
		}
	}
	else if (!generic_find(arg1, NULL, FIND_VEHICLE_ROOM | FIND_VEHICLE_INSIDE, ch, NULL, NULL, &from_veh)) {
		msg_to_char(ch, "You don't see %s %s here to upgrade.\r\n", AN(arg1), arg1);
		return;
	}
	
	// process the list of upgrades -- this will build the 'upgrade to what'
	// output if there's no arg2, will find the craft if there is, and can also
	// find the craft if there's no arg2 but there's also only 1 option
	size = snprintf(output, sizeof(output), "Upgrade %s to what:", (from_veh ? get_vehicle_short_desc(from_veh, ch) : "it"));
	to_craft = NULL;	// possibly the one the player requested
	found = 0;
	
	// put relations into an array to do this in 1 loop
	lists[0] = from_veh ? VEH_RELATIONS(from_veh) : NULL;
	lists[1] = (from_room && GET_BUILDING(from_room)) ? GET_BLD_RELATIONS(GET_BUILDING(from_room)) : NULL;
	for (iter = 0, done = FALSE; iter < 2 && !done; ++iter) {
		LL_FOREACH(lists[iter], relat) {
			// look up craft: this checks if the player can do it, too
			if (relat->type == BLD_REL_UPGRADES_TO_BLD) {
				find_craft = find_upgrade_craft_for_bld(ch, relat->vnum, missing_abil ? NULL : &missing_abil);
			}
			else if (relat->type == BLD_REL_UPGRADES_TO_VEH) {
				find_craft = find_upgrade_craft_for_veh(ch, relat->vnum, missing_abil ? NULL : &missing_abil);
			}
			else {
				continue;	// unsupported type
			}
			
			if (!find_craft) {
				continue;	// no craft available
			}
			if (*arg2 && !multi_isname(arg2, GET_CRAFT_NAME(find_craft))) {
				continue;	// not the requested craft
			}
			
			// SUCCESS: definitely an available craft
			if (size < sizeof(output)) {
				size += snprintf(output + size, sizeof(output) - size, "%s%s", (found ? ", " : " "), GET_CRAFT_NAME(find_craft));
			}
			++found;
			
			// save it for later if applicable (saves the first match)
			if (!to_craft) {
				to_craft = find_craft;
				
				if (*arg2) {
					// only looking for 1
					done = TRUE;
				}
			}
		}
	}
	
	// see what we've got from arg2
	if (!*arg2 && found > 1) {
		// too many choices
		msg_to_char(ch, "%s\r\n", output);
		return;
	}
	if (missing_abil && !to_craft && GET_CRAFT_ABILITY(missing_abil) != NOTHING) {
		// didn't find something but did find a missing ability
		msg_to_char(ch, "You need the %s ability to upgrade that.\r\n", get_ability_name_by_vnum(GET_CRAFT_ABILITY(missing_abil)));
		return;
	}
	if (found == 0 || !to_craft) {
		msg_to_char(ch, "You can't upgrade %s.\r\n", from_veh ? "that" : "anything here");
		return;
	}
	if (!CRAFT_IS_BUILDING(to_craft) && !CRAFT_IS_VEHICLE(to_craft)) {
		msg_to_char(ch, "That upgrade doesn't appear to be implemented correctly.\r\n");
		return;
	}
	if (CRAFT_IS_BUILDING(to_craft) && !(bld_proto = building_proto(GET_CRAFT_BUILD_TYPE(to_craft)))) {
		msg_to_char(ch, "That upgrade is unimplemented.\r\n");
		return;
	}
	if (CRAFT_IS_VEHICLE(to_craft) && !(veh_proto = vehicle_proto(GET_CRAFT_OBJECT(to_craft)))) {
		msg_to_char(ch, "That upgrade is unimplemented.\r\n");
		return;
	}
	
	// validate upgrade: room
	if (from_room && (!can_use_room(ch, from_room, MEMBERS_ONLY) || ROOM_AFF_FLAGGED(from_room, ROOM_AFF_UNCLAIMABLE))) {
		msg_to_char(ch, "You don't have permission to upgrade it.\r\n");
		return;
	}
	if (from_room && ROOM_AFF_FLAGGED(from_room, ROOM_AFF_HAS_INSTANCE)) {
		msg_to_char(ch, "You can't upgrade it while there's an adventure instance here.\r\n");
		return;
	}
	if (from_room && count_players_in_building(from_room, TRUE, TRUE)) {
		msg_to_char(ch, "You can't upgrade it while players are inside.\r\n");
		return;
	}
	
	// validate upgrade: vehicle
	if (from_veh && !can_use_vehicle(ch, from_veh, MEMBERS_ONLY)) {
		act("You don't have permission to upgrade $V.", FALSE, ch, NULL, from_veh, TO_CHAR | ACT_VEH_VICT);
		return;
	}
	if (from_veh && VEH_FLAGGED(from_veh, VEH_ON_FIRE)) {
		msg_to_char(ch, "You can't upgrade that... it's on fire!\r\n");
		return;
	}
	if (from_veh && VEH_DRIVER(from_veh)) {
		msg_to_char(ch, "You can't upgrade it while %s driving it.\r\n", (VEH_DRIVER(from_veh) == ch) ? "you're" : "someone is");
		return;
	}
	if (from_veh && VEH_LED_BY(from_veh)) {
		msg_to_char(ch, "You can't upgrade it while %s leading it.\r\n", (VEH_LED_BY(from_veh) == ch) ? "you're" : "someone is");
		return;
	}
	if (from_veh && VEH_SITTING_ON(from_veh)) {
		msg_to_char(ch, "You can't upgrade it while %s sitting %s it.\r\n", (VEH_SITTING_ON(from_veh) == ch) ? "you're" : "someone is", IN_OR_ON(from_veh));
		return;
	}
	if (from_veh && count_players_in_vehicle(from_veh, TRUE)) {
		msg_to_char(ch, "You can't upgrade it while players are inside.\r\n");
		return;
	}
	if (from_veh && CRAFT_IS_BUILDING(to_craft) && (IS_INSIDE(IN_ROOM(from_veh)) || IS_ANY_BUILDING(IN_ROOM(from_veh)))) {
		msg_to_char(ch, "You can't upgrade it %s a building.\r\n", ROOM_BLD_FLAGGED(IN_ROOM(from_veh), BLD_OPEN) ? "at" : "in");
		return;
	}
	
	// validate upgrade: general
	if (from_veh && bld_proto && !BLD_FLAGGED(bld_proto, BLD_OPEN)) {
		msg_to_char(ch, "You can't do that upgrade because it's incorrectly configured with a non-open building.\r\n");
		return;
	}
	if (from_veh && IN_ROOM(from_veh) != IN_ROOM(ch)) {
		act("You need to upgrade $V from outside of it.", FALSE, ch, NULL, from_veh, TO_CHAR);
		return;
	}
	if (from_room && IN_ROOM(ch) != HOME_ROOM(IN_ROOM(ch))) {
		msg_to_char(ch, "You need to upgrade the building from its main/entry room.\r\n");
		return;
	}
	if ((from_room && !IS_COMPLETE(from_room)) || (from_veh && !VEH_IS_COMPLETE(from_veh))) {
		msg_to_char(ch, "Finish building it before you upgrade it.\r\n");
		return;
	}
	if ((from_room && IS_DISMANTLING(from_room)) || (from_veh && VEH_IS_DISMANTLING(from_veh))) {
		msg_to_char(ch, "You can't upgrade that while it is being dismantled.\r\n");
		return;
	}
	if ((from_room && BUILDING_RESOURCES(from_room)) || (from_veh && VEH_NEEDS_RESOURCES(from_veh))) {
		msg_to_char(ch, "You need to do some repairs before you can upgrade it.\r\n");
		return;
	}
	if (GET_ACTION(ch) != ACT_NONE) {
		msg_to_char(ch, "You're already busy doing something else.\r\n");
		return;
	}
	if (!check_can_craft(ch, to_craft, FALSE)) {
		// sends own messages
		return;
	}
	if (from_room && !check_build_location_and_dir(ch, IN_ROOM(ch), to_craft, BUILDING_ENTRANCE(from_room) != NO_DIR ? rev_dir[BUILDING_ENTRANCE(from_room)] : NO_DIR, TRUE, NULL, NULL)) {
		// sends own messages
		return;
	}
	if (from_veh) {	// check building location
		// temporarily remove from the room for validation
		prev_veh = (from_veh == ROOM_VEHICLES(IN_ROOM(from_veh))) ? NULL : from_veh->prev_in_room;
		DL_DELETE2(ROOM_VEHICLES(IN_ROOM(from_veh)), from_veh, prev_in_room, next_in_room);
		
		fail = !check_build_location_and_dir(ch, IN_ROOM(ch), to_craft, NO_DIR, TRUE, NULL, NULL);
		
		// re-add vehicle
		if (prev_veh) {
			DL_APPEND_ELEM2(ROOM_VEHICLES(IN_ROOM(from_veh)), prev_veh, from_veh, prev_in_room, next_in_room);
		}
		else {
			DL_PREPEND2(ROOM_VEHICLES(IN_ROOM(from_veh)), from_veh, prev_in_room, next_in_room);
		}
		
		if (fail) {
			// sends own messages
			return;
		}
	}		
	
	// ---- SUCCESS: try start_upgrade ----
	if (start_upgrade(ch, to_craft, from_room, from_veh, bld_proto, veh_proto)) {
		if (CRAFT_IS_BUILDING(to_craft)) {
			start_action(ch, ACT_BUILDING, 0);
		}
		else {
			start_action(ch, ACT_GEN_CRAFT, -1);
		}
	
		GET_ACTION_VNUM(ch, 0) = GET_CRAFT_VNUM(to_craft);
		
		// check required obj
		if (GET_CRAFT_REQUIRES_OBJ(to_craft) != NOTHING) {
			find_and_bind(ch, GET_CRAFT_REQUIRES_OBJ(to_craft));
			
			if ((found_obj = has_required_obj_for_craft(ch, GET_CRAFT_REQUIRES_OBJ(to_craft))) && CRAFT_FLAGGED(to_craft, CRAFT_TAKE_REQUIRED_OBJ)) {
				act("You use $p.", FALSE, ch, found_obj, NULL, TO_CHAR);
				extract_obj(found_obj);
			}
		}
	}
}

