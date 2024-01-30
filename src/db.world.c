/* ************************************************************************
*   File: db.world.c                                      EmpireMUD 2.0b5 *
*  Usage: Modify functions for the map, interior, and game world          *
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
#include "db.h"
#include "comm.h"
#include "handler.h"
#include "skills.h"
#include "dg_scripts.h"
#include "dg_event.h"
#include "vnums.h"
#include "constants.h"

/**
* Contents:
*   World-Changers
*   Management
*   World Reset Cycle
*   Burning Buildings
*   City Lib
*   Room Resets
*   Sector Indexing
*   Territory
*   Trench Filling
*   Evolutions
*   Island Descriptions
*   Helpers
*   World Map System
*   World Map Saving
*   World Map Loading
*   Mapout System
*/

// external vars
extern FILE *binary_map_fl;

// external funcs
EVENT_CANCEL_FUNC(cancel_room_event);
EVENT_CANCEL_FUNC(cancel_room_expire_event);
bool load_pre_b5_116_world_map_from_file();
bool objpack_save_room(room_data *room);
void save_instances();

// locals
void cancel_all_world_save_requests(int only_save_type);
void grow_crop(struct map_data *map);
void init_room(room_data *room, room_vnum vnum);
void perform_requested_world_saves();
int naturalize_newbie_island(struct map_data *tile, bool do_unclaim);
int sort_empire_islands(struct empire_island *a, struct empire_island *b);
bool write_map_and_room_to_file(room_vnum vnum, bool force_obj_pack);


 //////////////////////////////////////////////////////////////////////////////
//// WORLD-CHANGERS //////////////////////////////////////////////////////////

/**
* Creates and sets up an additional interior room. This runs completion
* triggers. It does NOT add exits to or from the new room.
*
* @param room_data *home_room The home room for the building/vehicle that's gaining a new room.
* @param bld_vnum building_type The room type (must be an interior ROOM-flagged building). May be NOTHING to use the default.
* @return room_data *to_room The created room.
*/
room_data *add_room_to_building(room_data *home_room, bld_vnum building_type) {
	room_data *to_room;
	
	// validate home_room
	if (home_room) {
		home_room = HOME_ROOM(home_room);
	}
	
	// validate building type
	if (building_type == NOTHING || !building_proto(building_type)) {
		building_type = config_get_int("default_interior");
	}
	
	// create it
	to_room = create_room(home_room);
	attach_building_to_room(building_proto(building_type), to_room, TRUE);
	
	// update interior count
	if (home_room) {
		COMPLEX_DATA(home_room)->inside_rooms++;
	}
	
	// check in a vehicle?
	if (home_room && GET_ROOM_VEHICLE(home_room)) {
		add_room_to_vehicle(to_room, GET_ROOM_VEHICLE(home_room));
	}
	
	if (home_room && ROOM_OWNER(home_room)) {
		perform_claim_room(to_room, ROOM_OWNER(home_room));
	}
	complete_wtrigger(to_room);
	
	return to_room;
}


/**
* Update the "base sector" of a room. This is the sector that underlies its
* current sector (e.g. Plains under Building).
*
* @param room_data *room The room to update.
* @param sector_data *sect The sector to change the base to.
*/
void change_base_sector(room_data *room, sector_data *sect) {
	// safety
	if (GET_ROOM_VNUM(room) >= MAP_SIZE) {
		syslog(SYS_ERROR, LVL_START_IMM, TRUE, "SYSERR: Attempting to change_base_sector outside the map (%d)", GET_ROOM_VNUM(room));
		return;
	}
	
	// TODO there is a 90% chance change_base_sector can be completely replaced by this:
	perform_change_base_sect(room, NULL, sect);
}


/**
* This changes the territory when a chop finishes, if there's an evolution for
* it. (No effect on rooms without the evolution.)
*
* @param room_data *room The room.
*/
void change_chop_territory(room_data *room) {
	struct evolution_data *evo;
	
	if (ROOM_SECT_FLAGGED(room, SECTF_CROP)) {
		// chopping a crop un-crops it
		uncrop_tile(room);
	}
	else if ((evo = get_evolution_by_type(SECT(room), EVO_CHOPPED_DOWN))) {
		// normal case
		change_terrain(room, evo->becomes, NOTHING);
	}
}


/**
* This function safely changes terrain by disassociating any old data, and
* also storing a new original sect if necessary.
*
* @param room_data *room The room to change.
* @param sector_vnum sect Any sector vnum
* @param sector_vnum base_sect Optional: Sets the base sector, too. (pass NOTHING to set it the same as sect)
*/
void change_terrain(room_data *room, sector_vnum sect, sector_vnum base_sect) {
	sector_data *old_sect = SECT(room), *st = sector_proto(sect), *base;
	vehicle_data *veh, *next_veh;
	struct map_data *map;
	crop_data *new_crop = NULL;
	empire_data *emp;
	
	if (GET_ROOM_VNUM(room) >= MAP_SIZE) {
		syslog(SYS_ERROR, LVL_START_IMM, TRUE, "SYSERR: Attempting to change_terrain outside the map (%d)", GET_ROOM_VNUM(room));
		return;
	}
	
	if (!st) {
		log("SYSERR: change_terrain called with invalid sector vnum %d", sect);
		return;
	}
	
	base = sector_proto(base_sect);
	if (!base) {
		base = st;
	}
	
	// tear down any building data and customizations
	disassociate_building(room);
	decustomize_room(room);
	
	// keep crop if it has one
	if (SECT_FLAGGED(st, SECTF_HAS_CROP_DATA) && ROOM_CROP(room)) {
		new_crop = ROOM_CROP(room);
	}
	
	// need land-map update?
	if (st != old_sect && SECT_IS_LAND_MAP(st) != SECT_IS_LAND_MAP(old_sect)) {
		map = &(world_map[FLAT_X_COORD(room)][FLAT_Y_COORD(room)]);
		if (SECT_IS_LAND_MAP(st)) {
			// add to land_map (at the start is fine)
			LL_PREPEND(land_map, map);
		}
		else {
			// remove from land_map
			LL_DELETE(land_map, map);
			// do NOT free map -- it's a pointer to something in world_map
		}
	}
	
	// change sect
	perform_change_sect(room, NULL, st);
	perform_change_base_sect(room, NULL, base);
	
	// need to determine a crop?
	if (!new_crop && SECT_FLAGGED(st, SECTF_HAS_CROP_DATA) && !ROOM_CROP(room)) {
		new_crop = get_potential_crop_for_location(room, NOTHING);
		if (!new_crop) {
			new_crop = crop_table;
		}
	}
		
	// need room data?
	if ((IS_ANY_BUILDING(room) || IS_ADVENTURE_ROOM(room)) && !COMPLEX_DATA(room)) {
		COMPLEX_DATA(room) = init_complex_data();
	}
	
	// some extra data can be safely cleared here
	remove_room_extra_data(room, ROOM_EXTRA_CHOP_PROGRESS);
	remove_room_extra_data(room, ROOM_EXTRA_HARVEST_PROGRESS);
	remove_room_extra_data(room, ROOM_EXTRA_TRENCH_PROGRESS);
	remove_room_extra_data(room, ROOM_EXTRA_SEED_TIME);
	remove_room_extra_data(room, ROOM_EXTRA_TRENCH_FILL_TIME);
	
	// always reset some depletions
	remove_depletion(room, DPLTN_CHOP);
	
	// if we picked a crop type, 
	if (new_crop) {
		set_crop_type(room, new_crop);
	}
	else if (!SECT_FLAGGED(st, SECTF_HAS_CROP_DATA)) {
		set_crop_type(room, NULL);
	}
	
	// do we need to lock the icon?
	if (ROOM_SECT_FLAGGED(room, SECTF_LOCK_ICON) || (ROOM_SECT_FLAGGED(room, SECTF_CROP) && ROOM_CROP(room) && CROP_FLAGGED(ROOM_CROP(room), CROPF_LOCK_ICON))) {
		lock_icon(room, NULL);
	}
	
	// need start locations update?
	if (SECT_FLAGGED(old_sect, SECTF_START_LOCATION) != SECT_FLAGGED(st, SECTF_START_LOCATION)) {
		setup_start_locations();
	}
	
	// ensure it has height if it needs it
	check_terrain_height(room);
	
	// for later
	emp = ROOM_OWNER(room);
	
	// did it become unclaimable?
	if (emp && SECT_FLAGGED(st, SECTF_NO_CLAIM)) {
		abandon_room(room);
	}
	
	// update requirement trackers
	if (emp) {
		qt_empire_players(emp, qt_lose_tile_sector, GET_SECT_VNUM(old_sect));
		et_lose_tile_sector(emp, GET_SECT_VNUM(old_sect));
		
		qt_empire_players(emp, qt_gain_tile_sector, GET_SECT_VNUM(st));
		et_gain_tile_sector(emp, GET_SECT_VNUM(st));
	}
	
	// lastly, see if any vehicles died from this
	DL_FOREACH_SAFE2(ROOM_VEHICLES(room), veh, next_veh, next_in_room) {
		check_vehicle_climate_change(veh, TRUE);
	}
}


/**
* Ensures a room or map tile has an island id/pointer -- to be called after the
* terrain is changed for any reason. This will add/remove the points and will
* update the island's tile count.
*
* @param room_data *room The map room data. (must provide room OR map)
* @param struct map_data *map The map tile data. (must provide room OR map)
*/
void check_island_assignment(room_data *room, struct map_data *map) {
	struct island_info **island;
	struct map_data *to_map;
	int x, y, new_x, new_y;
	sector_data *sect;
	int iter, *id;
	
	if (!room && !map) {
		return;
	}
	if (room && GET_ROOM_VNUM(room) >= MAP_SIZE) {
		return;
	}
	
	sect = room ? SECT(room) : map->sector_type;
	island = room ? &GET_ISLAND(room) : &map->shared->island_ptr;
	id = room ? &GET_ISLAND_ID(room) : &map->shared->island_id;
	x = room ? X_COORD(room) : MAP_X_COORD(map->vnum);
	y = room ? Y_COORD(room) : MAP_Y_COORD(map->vnum);
	
	// room is NOT on an island
	if (SECT_FLAGGED(sect, SECTF_NON_ISLAND)) {
		if (*island != NULL) {
			(*island)->tile_size -= 1;
			*island = NULL;
		}
		if (*id != NO_ISLAND) {
			*id = NO_ISLAND;
		}
		return;
	}
	
	// if we already have an island, we're done now
	if (*island) {
		return;
	}
	
	// try to detect an island nearby
	for (iter = 0; iter < NUM_2D_DIRS; ++iter) {
		if (!get_coord_shift(x, y, shift_dir[iter][0], shift_dir[iter][1], &new_x, &new_y)) {
			continue;	// no room that way
		}
		if (!(to_map = &(world_map[new_x][new_y]))) {
			continue;	// unable to get map tile in that dir
		}
		if (!to_map->shared->island_ptr) {
			continue;	// no island to copy
		}
		
		// found one!
		*id = to_map->shared->island_id;
		*island = to_map->shared->island_ptr;
		(*island)->tile_size += 1;
		return;	// done
	}
	
	// if we get this far, it was not able to detect an island -- make a new one
	number_and_count_islands(FALSE);
}


/**
* Ensures a room has (or loses) its terrain height, to be called after you
* change the sector with change_terrain().
*
* @param room_data *room The room to check.
*/
void check_terrain_height(room_data *room) {
	room_data *to_room;
	int dir;
	
	int good_match = 0, bad_match = 0;
	
	if ((ROOM_SECT_FLAGGED(room, SECTF_NEEDS_HEIGHT) || SECT_FLAGGED(BASE_SECT(room), SECTF_NEEDS_HEIGHT)) && ROOM_HEIGHT(room) == 0) {
		// attempt to detect from neighbors
		for (dir = 0; dir < NUM_2D_DIRS; ++dir) {
			if (!(to_room = real_shift(room, shift_dir[dir][0], shift_dir[dir][1]))) {
				continue;	// no room that way
			}
			if (!ROOM_HEIGHT(to_room)) {
				continue;	// no height to copy
			}
			
			// ok, it has a height we can borrow... let's see if it's a good match
			if (GET_SECT_CLIMATE(BASE_SECT(to_room)) == GET_SECT_CLIMATE(BASE_SECT(room))) {
				// perfect match: just copy it
				set_room_height(room, ROOM_HEIGHT(to_room));
				return;
			}
			else if (GET_SECT_CLIMATE(BASE_SECT(to_room)) & GET_SECT_CLIMATE(BASE_SECT(room)) && good_match != 0) {
				// good match: save for later
				good_match = ROOM_HEIGHT(to_room);
			}
			else if (!bad_match) {
				// bad match: use if necessary
				bad_match = ROOM_HEIGHT(to_room);
			}
		}
		
		set_room_height(room, good_match ? good_match : bad_match);
	}
	else if (!ROOM_SECT_FLAGGED(room, SECTF_NEEDS_HEIGHT | SECTF_KEEPS_HEIGHT) && !SECT_FLAGGED(BASE_SECT(room), SECTF_NEEDS_HEIGHT | SECTF_KEEPS_HEIGHT)) {
		// clear it
		set_room_height(room, 0);
	}
}


/**
* Adds an exit to a world room, and optionally adds the return exit.
*
* @param room_data *from The room to link from.
* @param room_data *to The room to link to. (OPTIONAL if you want to set your own)
* @param bool back If TRUE, creates the reverse exit, too.
* @return struct room_direction_data* A pointer to the new exit.
*/
struct room_direction_data *create_exit(room_data *from, room_data *to, int dir, bool back) {	
	struct room_direction_data *ex = NULL, *other = NULL;
	
	// safety first
	if (!from) {
		return NULL;
	}
	
	if (!(ex = find_exit(from, dir)) && COMPLEX_DATA(from)) {
		CREATE(ex, struct room_direction_data, 1);
		ex->dir = dir;
		
		LL_PREPEND(COMPLEX_DATA(from)->exits, ex);
		LL_SORT(COMPLEX_DATA(from)->exits, sort_exits);

		// re-find after sort
		ex = find_exit(from, dir);
	}
	
	// update an existing one or the new one
	if (ex) {
		ex->to_room = to ? GET_ROOM_VNUM(to) : NOWHERE;
		ex->room_ptr = to;
		if (to) {
			++GET_EXITS_HERE(to);
		}
	}
	
	if (back && to && COMPLEX_DATA(to) && !(other = find_exit(to, rev_dir[dir]))) {
		CREATE(other, struct room_direction_data, 1);
		other->dir = rev_dir[dir];
		
		LL_PREPEND(COMPLEX_DATA(to)->exits, other);
		LL_SORT(COMPLEX_DATA(to)->exits, sort_exits);
		
		// re-find after sort
		other = find_exit(to, rev_dir[dir]);
	}
	
	// similar to above
	if (other) {
		other->to_room = GET_ROOM_VNUM(from);
		other->room_ptr = from;
		++GET_EXITS_HERE(from);
	}
	
	request_world_save(GET_ROOM_VNUM(from), WSAVE_ROOM);
	if (to && back) {
		request_world_save(GET_ROOM_VNUM(to), WSAVE_ROOM);
	}
	
	// re-find just in case
	return find_exit(from, dir);
}


/**
* Creates a new room, initializes it, inserts it in the world, and returns
* its pointer.
*
* @param room_data *home The home room to attach to, if any.
* @return room_data* the new room
*/
room_data *create_room(room_data *home) {
	room_data *room;
	
	// to start
	room_vnum vnum = find_free_vnum();

	if (vnum < MAP_SIZE) {
		syslog(SYS_ERROR, 0, TRUE, "SYSERR: create_room() attempting to create low vnum %d", vnum);
		return NULL;
	}

	if (real_room(vnum)) {
		syslog(SYS_ERROR, 0, TRUE, "SYSERR: create_room() room %d already exists", vnum);
		return NULL;
	}
	
	// make room!
	CREATE(room, room_data, 1);
	init_room(room, vnum);
	add_room_to_world_tables(room);
	
	COMPLEX_DATA(room)->home_room = home;
	GET_MAP_LOC(room) = home ? GET_MAP_LOC(home) : NULL;
	GET_ISLAND_ID(room) = home ? GET_ISLAND_ID(home) : NO_ISLAND;
	GET_ISLAND(room) = home ? GET_ISLAND(home) : NULL;
	
	request_world_save(GET_ROOM_VNUM(room), WSAVE_ROOM);
	return room;
}


/**
* Deletes a room from the world, and all places it's referenced. This can only
* be used on interiors, never on the map.
*
* If you call this without check_exits, it'll run faster but you MUST call
* check_all_exits() when you're done.
*
* @param room_data *room The room to delete.
* @param bool check_exits If TRUE, updates all world exits right away*.
*/
void delete_room(room_data *room, bool check_exits) {
	struct room_direction_data *ex, *next_ex;
	struct empire_territory_data *ter;
	struct empire_city_data *city, *next_city;
	room_data *rm_iter, *next_rm, *home;
	room_data *extraction_room = NULL;
	vehicle_data *veh, *next_veh;
	struct instance_data *inst;
	struct affected_type *af;
	struct reset_com *reset, *next_reset;
	descriptor_data *desc;
	char_data *c, *next_c;
	obj_data *o, *next_o;
	empire_data *emp;
	int iter;
	
	// this blocks deleting map rooms unless they can be unloaded (or the mud is being shut down)
	if (!room || (GET_ROOM_VNUM(room) < MAP_SIZE && !CAN_UNLOAD_MAP_ROOM(room) && !block_all_saves_due_to_shutdown)) {
		syslog(SYS_ERROR, 0, TRUE, "SYSERR: delete_room() attempting to delete invalid room %d", room ? GET_ROOM_VNUM(room) : NOWHERE);
		return;
	}
	
	if (GET_ROOM_VNUM(room) >= MAP_SIZE) {
		// cancel anybody editing its description, if it's not a shared map room
		LL_FOREACH(descriptor_list, desc) {
			if (desc->str == &ROOM_CUSTOM_DESCRIPTION(room)) {
				// abort the editor
				string_add(desc, "/a");
			}
		}
	}
	
	check_dg_owner_purged_room(room);
	
	// this will ensure any map data is still saved and will update the world index, while also deleting the world file
	request_world_save(GET_ROOM_VNUM(room), WSAVE_MAP | WSAVE_ROOM);
		
	// delete this first
	if (ROOM_UNLOAD_EVENT(room)) {
		dg_event_cancel(ROOM_UNLOAD_EVENT(room), cancel_room_event);
		ROOM_UNLOAD_EVENT(room) = NULL;
	}
	
	if (HAS_FUNCTION(room, FNC_LIBRARY)) {
		delete_library(room);
	}
	
	// ensure not owned (and update empire stuff if so)
	if ((emp = ROOM_OWNER(room))) {
		if ((ter = find_territory_entry(emp, room))) {
			delete_territory_entry(emp, ter, TRUE);
		}
		
		// update all empire cities
		LL_FOREACH_SAFE(EMPIRE_CITY_LIST(emp), city, next_city) {
			if (city->location == room) {
				// ... this should not be possible, but just in case ...
				log_to_empire(emp, ELOG_TERRITORY, "%s was lost", city->name);
				perform_abandon_city(emp, city, FALSE);
			}
		}
		
		// still owned?
		if (ROOM_OWNER(room)) {
			perform_abandon_room(room);
		}
	}
	
	// delete any open instance here
	if (ROOM_AFF_FLAGGED(room, ROOM_AFF_HAS_INSTANCE) && (inst = find_instance_by_room(room, FALSE, FALSE))) {
		SET_BIT(INST_FLAGS(inst), INST_COMPLETED);
	}
	
	if (check_exits) {
		// search world for portals that link here
		DL_FOREACH_SAFE(object_list, o, next_o) {
			if (IS_PORTAL(o) && GET_PORTAL_TARGET_VNUM(o) == GET_ROOM_VNUM(room)) {
				if (IN_ROOM(o) && ROOM_PEOPLE(IN_ROOM(o))) {
					act("$p closes and vanishes!", FALSE, ROOM_PEOPLE(IN_ROOM(o)), o, NULL, TO_CHAR | TO_ROOM);
				}
				extract_obj(o);
			}
		}
	}
	
	// check if it was part of the interior of a vehicle
	if (GET_ROOM_VEHICLE(room) && VEH_INTERIOR_HOME_ROOM(GET_ROOM_VEHICLE(room)) == room) {
		VEH_INTERIOR_HOME_ROOM(GET_ROOM_VEHICLE(room)) = NULL;
		request_vehicle_save_in_world(GET_ROOM_VEHICLE(room));
	}
	
	// shrink home
	home = HOME_ROOM(room);
	if (home != room && GET_INSIDE_ROOMS(home) > 0) {
		COMPLEX_DATA(home)->inside_rooms -= 1;
	}
	if (home != room && GET_ROOM_VEHICLE(home)) {
		--VEH_INSIDE_ROOMS(GET_ROOM_VEHICLE(home));
	}
	
	// get rid of vehicles
	DL_FOREACH_SAFE2(ROOM_VEHICLES(room), veh, next_veh, next_in_room) {
		if (!extraction_room) {
			extraction_room = get_extraction_room();
		}
		vehicle_from_room(veh);
		vehicle_to_room(veh, extraction_room);
		extract_vehicle(veh);
	}
	
	// get rid of players
	relocate_players(room, NULL);
	
	// remove room from world now
	remove_room_from_world_tables(room);
	if (GET_ROOM_VEHICLE(room)) {
		remove_room_from_vehicle(room, GET_ROOM_VEHICLE(room));
	}
	
	// Remove remaining chars
	DL_FOREACH_SAFE2(ROOM_PEOPLE(room), c, next_c, next_in_room) {
		if (!extraction_room) {
			extraction_room = get_extraction_room();
		}
		char_to_room(c, extraction_room);
		
		if (!IS_NPC(c)) {
			save_char(c, NULL);
		}
		
		extract_all_items(c);
		extract_char(c);
	}

	/* Remove the objects */
	while (ROOM_CONTENTS(room)) {
		extract_obj(ROOM_CONTENTS(room));
	}
	
	// free some crap
	if (SCRIPT(room)) {
		extract_script(room, WLD_TRIGGER);
	}
	free_proto_scripts(&room->proto_script);
	room->proto_script = NULL;
	
	DL_FOREACH_SAFE(room->reset_commands, reset, next_reset) {
		if (reset->sarg1) {
			free(reset->sarg1);
		}
		if (reset->sarg2) {
			free(reset->sarg2);
		}
		free(reset);
	}
	while ((af = ROOM_AFFECTS(room))) {
		ROOM_AFFECTS(room) = af->next;
		if (af->expire_event) {
			dg_event_cancel(af->expire_event, cancel_room_expire_event);
		}
		free(af);
	}

	if (check_exits) {
		// update all home rooms and exits
		HASH_ITER(hh, world_table, rm_iter, next_rm) {
			// found them all?
			if (GET_EXITS_HERE(room) <= 0) {
				break;
			}
			if (rm_iter != room && COMPLEX_DATA(rm_iter)) {
				// unset homeroom -> this is just interior table
				if (COMPLEX_DATA(rm_iter)->home_room == room) {
					// this was contained in the deleted room... it should probably have been deleted, too,
					// but marking it as no home_room will trigger an auto-delete later
					COMPLEX_DATA(rm_iter)->home_room = NULL;
				}
			
				// delete exits
				for (ex = COMPLEX_DATA(rm_iter)->exits; ex; ex = next_ex) {
					next_ex = ex->next;
				
					if (ex->to_room == GET_ROOM_VNUM(room) || ex->room_ptr == room) {
						LL_DELETE(COMPLEX_DATA(rm_iter)->exits, ex);
						if (ex->keyword) {
							free(ex->keyword);
						}
						free(ex);
						--GET_EXITS_HERE(room);
					}
				}
			}
		}
	}
	else if (GET_INSIDE_ROOMS(room)) {
		// just check home rooms of interiors
		DL_FOREACH_SAFE2(interior_room_list, rm_iter, next_rm, next_interior) {
			if (rm_iter != room && COMPLEX_DATA(rm_iter)) {
				// unset homeroom -> this is just interior table
				if (COMPLEX_DATA(rm_iter)->home_room == room) {
					// this was contained in the deleted room... it should probably have been deleted, too,
					// but marking it as no home_room will trigger an auto-delete later
					COMPLEX_DATA(rm_iter)->home_room = NULL;
				}
			}
		}
	}
	
	// update instances (if it wasn't deleted already earlier)
	if (ROOM_AFF_FLAGGED(room, ROOM_AFF_HAS_INSTANCE | ROOM_AFF_FAKE_INSTANCE) || (COMPLEX_DATA(room) && COMPLEX_DATA(room)->instance)) {
		DL_FOREACH(instance_list, inst) {
			if (INST_LOCATION(inst) == room) {
				SET_BIT(INST_FLAGS(inst), INST_COMPLETED);
				INST_LOCATION(inst) = NULL;
			}
			if (INST_FAKE_LOC(inst) == room) {
				SET_BIT(INST_FLAGS(inst), INST_COMPLETED);
				remove_instance_fake_loc(inst);
			}
			if (INST_START(inst) == room) {
				SET_BIT(INST_FLAGS(inst), INST_COMPLETED);
				INST_START(inst) = NULL;
			}
			for (iter = 0; iter < INST_SIZE(inst); ++iter) {
				if (INST_ROOM(inst, iter) == room) {
					INST_ROOM(inst, iter) = NULL;
				}
			}
		}
	}
	
	// check for start location changes?
	if (ROOM_SECT_FLAGGED(room, SECTF_START_LOCATION)) {
		setup_start_locations();
	}
	
	// free complex data late
	if (COMPLEX_DATA(room)) {
		free_complex_data(COMPLEX_DATA(room));
		COMPLEX_DATA(room) = NULL;
	}
	
	// only have to free this info if not on the map (map rooms have a pointer to the map)
	if (GET_ROOM_VNUM(room) >= MAP_SIZE && SHARED_DATA(room) && SHARED_DATA(room) != &ocean_shared_data) {
		free_shared_room_data(SHARED_DATA(room));
	}
	
	// break the association with the world_map tile
	if (GET_MAP_LOC(room) && GET_MAP_LOC(room)->room == room) {
		GET_MAP_LOC(room)->room = NULL;
	}
	
	// free the room
	free(room);
}


/* When a trench fills up by "pour out" or by rain, this happens */
void fill_trench(room_data *room) {	
	char lbuf[MAX_INPUT_LENGTH];
	struct evolution_data *evo;
	sector_data *sect;
	
	if ((evo = get_evolution_by_type(SECT(room), EVO_TRENCH_FULL)) != NULL && !ROOM_AFF_FLAGGED(room, ROOM_AFF_NO_EVOLVE)) {
		if (ROOM_PEOPLE(room)) {
			sect = sector_proto(evo->becomes);
			sprintf(lbuf, "The trench is full! It is now %s %s!", sect ? AN(GET_SECT_NAME(sect)) : "something", sect ? GET_SECT_NAME(sect) : "else");
			act(lbuf, FALSE, ROOM_PEOPLE(room), 0, 0, TO_CHAR | TO_ROOM);
		}
		change_terrain(room, evo->becomes, NOTHING);
		remove_room_extra_data(room, ROOM_EXTRA_TRENCH_FILL_TIME);
	}
}


/**
* Marks a trench complete and starts filling it.
*
* @param room_data *room The room to fill.
*/
void finish_trench(room_data *room) {
	remove_room_extra_data(room, ROOM_EXTRA_TRENCH_PROGRESS);
	
	if (!ROOM_SECT_FLAGGED(room, SECTF_IS_TRENCH)) {
		return;	// wat
	}
	
	// check for adjacent water
	if (find_flagged_sect_within_distance_from_room(room, SECTF_FRESH_WATER | SECTF_OCEAN | SECTF_SHALLOW_WATER, NOBITS, 1)) {
		fill_trench(room);
	}
	else {	// otherwise schedule filling
		set_room_extra_data(room, ROOM_EXTRA_TRENCH_FILL_TIME, time(0) + config_get_int("trench_fill_time"));
		if (GET_MAP_LOC(room)) {	// can this BE null here?
			schedule_trench_fill(GET_MAP_LOC(room));
		}
	}
}


// validates rare metals for global mine data
GLB_VALIDATOR(validate_global_mine_data) {
	struct glb_room_emp_bean *data = (struct glb_room_emp_bean*)other_data;
	empire_data *emp = (data ? data->empire : NULL);
	
	if (IS_SET(GET_GLOBAL_FLAGS(glb), GLB_FLAG_RARE) && (!emp || !EMPIRE_HAS_TECH(emp, TECH_RARE_METALS)) && (!ch || !GET_LOYALTY(ch) || !EMPIRE_HAS_TECH(GET_LOYALTY(ch), TECH_RARE_METALS))) {
		return FALSE;
	}
	return TRUE;
}


// set up mine for init_mine()
GLB_FUNCTION(run_global_mine_data) {
	struct glb_room_emp_bean *data = (struct glb_room_emp_bean*)other_data;
	empire_data *emp = (data ? data->empire : NULL);
	room_data *room = (data ? data->room : NULL);
	
	if (!data || !room) {
		return FALSE;	// no work
	}
	
	set_room_extra_data(room, ROOM_EXTRA_MINE_GLB_VNUM, GET_GLOBAL_VNUM(glb));
	set_room_extra_data(room, ROOM_EXTRA_MINE_AMOUNT, number(GET_GLOBAL_VAL(glb, GLB_VAL_MAX_MINE_SIZE) / 2, GET_GLOBAL_VAL(glb, GLB_VAL_MAX_MINE_SIZE)));
	
	if ((ch && has_player_tech(ch, PTECH_DEEP_MINES)) || (emp && EMPIRE_HAS_TECH(emp, TECH_DEEP_MINES)) || (ch && GET_LOYALTY(ch) && EMPIRE_HAS_TECH(GET_LOYALTY(ch), TECH_DEEP_MINES))) {
		multiply_room_extra_data(room, ROOM_EXTRA_MINE_AMOUNT, 1.5);
		if (ch) {
			gain_player_tech_exp(ch, PTECH_DEEP_MINES, 15);
			run_ability_hooks_by_player_tech(ch, PTECH_DEEP_MINES, NULL, NULL, NULL, IN_ROOM(ch));
		}
	}
	
	if (ch && GET_GLOBAL_ABILITY(glb) != NO_ABIL) {
		gain_ability_exp(ch, GET_GLOBAL_ABILITY(glb), 75);
		run_ability_hooks(ch, AHOOK_ABILITY, GET_GLOBAL_ABILITY(glb), 0, NULL, NULL, NULL, room, NOBITS);
	}
	
	return TRUE;
}


/**
* Choose a mine type for a room. The 'ch' is optional, for ability
* requirements.
*
* @param room_data *room The room to choose a mine type for.
* @param char_data *ch Optional: The character setting up mine data (for abilities).
* @param empire_data *emp Optional: The empire/owner (for techs).
*/
void init_mine(room_data *room, char_data *ch, empire_data *emp) {
	struct glb_room_emp_bean *data;
	
	// no work
	if (!room || !ROOM_CAN_MINE(room) || get_room_extra_data(room, ROOM_EXTRA_MINE_GLB_VNUM) > 0) {
		return;
	}
	
	CREATE(data, struct glb_room_emp_bean, 1);
	data->empire = emp;
	data->room = room;
	run_globals(GLOBAL_MINE_DATA, run_global_mine_data, FALSE, GET_SECT_FLAGS(BASE_SECT(room)), ch, (GET_ROOM_TEMPLATE(room) ? get_adventure_for_vnum(GET_RMT_VNUM(GET_ROOM_TEMPLATE(room))) : NULL), ch ? GET_COMPUTED_LEVEL(ch) : 0, validate_global_mine_data, data);
	free(data);
}


/**
* Changes the sector when an outdoor tile is 'burned down', if there's an
* evolution for it. (No effect on rooms without the evolution.)
*
* @param room_data *room The outdoor room with a matching evolution.
* @param int evo_type Should be EVO_BURNS_TO or EVO_BURN_STUMPS
*/
void perform_burn_room(room_data *room, int evo_type) {
	char buf[MAX_STRING_LENGTH], from[256], to[256];
	struct evolution_data *evo;
	sector_data *sect;
	
	if ((evo = get_evolution_by_type(SECT(room), evo_type)) && (sect = sector_proto(evo->becomes)) && SECT(room) != sect) {
		if (ROOM_PEOPLE(room)) {
			if (ROOM_CROP(room)) {
				strcpy(from, GET_CROP_NAME(ROOM_CROP(room)));
			}
			else {
				strcpy(from, GET_SECT_NAME(SECT(room)));
			}
			strtolower(from);
			strcpy(to, GET_SECT_NAME(sect));
			strtolower(to);
			
			sprintf(buf, "The %s burn%s down and become%s %s%s%s.", from, (from[strlen(from)-1] == 's' ? "" : "s"), (from[strlen(from)-1] == 's' ? "" : "s"), (to[strlen(to)-1] == 's' ? "" : AN(to)), (to[strlen(to)-1] == 's' ? "" : " "), to);
			act(buf, FALSE, ROOM_PEOPLE(room), NULL, NULL, TO_CHAR | TO_ROOM);
		}
		
		change_terrain(room, evo->becomes, NOTHING);
		
		stop_room_action(room, ACT_BURN_AREA);
		stop_room_action(room, ACT_CHOPPING);
		stop_room_action(room, ACT_PICKING);
		stop_room_action(room, ACT_GATHERING);
		stop_room_action(room, ACT_HARVESTING);
		stop_room_action(room, ACT_PLANTING);
	}
}


/**
* Updates the natural sector of a map tile (interiors do not have this
* property).
*
* @param struct map_data *map The location.
* @param sector_data *sect The new natural sector.
*/
void set_natural_sector(struct map_data *map, sector_data *sect) {
	map->natural_sector = sect;
	request_world_save(map->vnum, WSAVE_MAP);
}


/**
* Updates the natural sector of a map tile (interiors do not have this
* property).
*
* @param room_data *room The room to change.
* @param int idnum The new owner id (NOBODY to clear it).
*/
void set_private_owner(room_data *room, int idnum) {
	if (COMPLEX_DATA(room) && COMPLEX_DATA(room)->private_owner != idnum) {
		COMPLEX_DATA(room)->private_owner = idnum;
		request_world_save(GET_ROOM_VNUM(room), WSAVE_ROOM);
	}
}


/**
* Updates the room's custom description. It does no validation, so you must
* pre-validate the description.
*
* @param room_data *room The room to change.
* @param char *desc The new description (will be copied).
*/
void set_room_custom_description(room_data *room, char *desc) {
	if (ROOM_CUSTOM_DESCRIPTION(room)) {
		free(ROOM_CUSTOM_DESCRIPTION(room));
	}
	ROOM_CUSTOM_DESCRIPTION(room) = desc ? str_dup(desc) : NULL;
	request_world_save(GET_ROOM_VNUM(room), WSAVE_ROOM);
}


/**
* Updates the room's custom icon. It does no validation, so you must
* pre-validate the icon.
*
* @param room_data *room The room to change.
* @param char *icon The new icon (will be copied).
*/
void set_room_custom_icon(room_data *room, char *icon) {
	if (ROOM_CUSTOM_ICON(room)) {
		free(ROOM_CUSTOM_ICON(room));
	}
	ROOM_CUSTOM_ICON(room) = icon ? str_dup(icon) : NULL;
	request_world_save(GET_ROOM_VNUM(room), WSAVE_ROOM);
}


/**
* Updates the room's custom name. It does no validation, so you must
* pre-validate the name.
*
* @param room_data *room The room to change.
* @param char *name The new name (will be copied).
*/
void set_room_custom_name(room_data *room, char *name) {
	if (ROOM_CUSTOM_NAME(room)) {
		free(ROOM_CUSTOM_NAME(room));
	}
	ROOM_CUSTOM_NAME(room) = name ? str_dup(name) : NULL;
	if (!name) {
		REMOVE_BIT(ROOM_BASE_FLAGS(room), ROOM_AFF_HIDE_REAL_NAME);
		affect_total_room(room);
	}
	request_world_save(GET_ROOM_VNUM(room), WSAVE_ROOM);
}


/**
* Updates the height of a map tile/room. Technically interiors have this
* property, too, but it has no function.
*
* @param room_data *room The room to change height on.
* @param int height The new height.
*/
void set_room_height(room_data *room, int height) {
	ROOM_HEIGHT(room) = height;
	request_world_save(GET_ROOM_VNUM(room), WSAVE_MAP | WSAVE_ROOM);
}


/**
* This handles returning a crop tile to a non-crop state after harvesting (or
* for any other reason). It prefers the harvest-to evolution, but has a chain
* of data sources for the target sector.
*
* @param room_data *room The map tile to un-crop.
*/
void uncrop_tile(room_data *room) {
	char buf[MAX_STRING_LENGTH], name[MAX_STRING_LENGTH];
	struct evolution_data *evo;
	sector_data *to_sect = NULL, *next_sect;
	int safety;
	
	// flags that cannot be on the sector we choose here
	const bitvector_t invalid_sect_flags = SECTF_HAS_CROP_DATA | SECTF_CROP | SECTF_MAP_BUILDING | SECTF_INSIDE | SECTF_ADVENTURE;
	
	// safety first
	if (!room) {
		return;
	}
	if (GET_ROOM_VNUM(room) >= MAP_SIZE) {
		syslog(SYS_ERROR, LVL_START_IMM, TRUE, "SYSERR: uncrop_tile called on non-map room vnum %d", GET_ROOM_VNUM(room));
		return;
	}
	
	// in order of priority for determining a sector:
	// TODO if crops get evolutions, the crop may override this
	
	// 1. harvest-to evo (overrides everything)
	if (!to_sect && (evo = get_evolution_by_type(SECT(room), EVO_HARVEST_TO))) {
		to_sect = sector_proto(evo->becomes);	// even if it has invalid_sect_flags
	}
	
	// 2. stored base sector from before it was a crop
	if (!to_sect && BASE_SECT(room) != SECT(room) && !SECT_FLAGGED(BASE_SECT(room), invalid_sect_flags)) {
		to_sect = BASE_SECT(room);
	}
	
	// 3. default-harvest-to evo
	if (!to_sect && (evo = get_evolution_by_type(SECT(room), EVO_DEFAULT_HARVEST_TO))) {
		to_sect = sector_proto(evo->becomes);	// even if it has invalid_sect_flags
	}
	
	// 4. attempt to find one
	if (!to_sect) {
		// should this be get_climate(room) rather than GET_SECT_CLIMATE(SECT(room)) ?
		to_sect = find_first_matching_sector(NOBITS, invalid_sect_flags, GET_SECT_CLIMATE(SECT(room)));
	}
	
	// 5. did we fail entirely?
	if (!to_sect) {
		to_sect = sector_proto(config_get_int("default_land_sect"));
	}
	
	// fail?
	if (!to_sect) {
		syslog(SYS_ERROR, LVL_START_IMM, TRUE, "SYSERR: uncrop_tile: unable to find a valid tile to return to; 'config world default_land_sect' may not be set");
		return;
	}
	
	// loop time: does THIS sect have a harvest-to? If so, follow it down the chain
	safety = 0;
	while ((evo = get_evolution_by_type(to_sect, EVO_HARVEST_TO)) && safety < 100) {
		++safety;
		if ((next_sect = sector_proto(evo->becomes))) {
			to_sect = next_sect;
		}
		else {
			break;	// missing next_sect; keep the one we have
		}
	}
	
	if (safety >= 100) {
		syslog(SYS_ERROR, LVL_START_IMM, TRUE, "SYSERR: uncrop_tile: possible infinte loop on HARVEST-TO interactions for sector [%d] %s", GET_SECT_VNUM(to_sect), GET_SECT_NAME(to_sect));
	}
	
	// ok: now change it
	change_terrain(room, GET_SECT_VNUM(to_sect), NOTHING);
	
	if (ROOM_PEOPLE(room)) {
		strcpy(name, GET_SECT_NAME(to_sect));
		strtolower(name);
		sprintf(buf, "The area is now %s%s%s.", (name[strlen(name)-1] == 's' ? "" : AN(name)), (name[strlen(name)-1] == 's' ? "" : " "), name);
		act(buf, FALSE, ROOM_PEOPLE(room), NULL, NULL, TO_CHAR | TO_ROOM);
	}
	
	// this could possibly remove DPLTN_FORAGE and DPLTN_PICK, but since it has
	// just changed back from being a crop, those are probably safe to keep
}


/**
* Converts a trench back into its base sect.
*
* @param room_data *room The trench to convert back.
*/
void untrench_room(room_data *room) {
	sector_data *to_sect = NULL;
	struct map_data *map;
	
	if (!ROOM_SECT_FLAGGED(room, SECTF_IS_TRENCH) || GET_ROOM_VNUM(room) >= MAP_SIZE) {
		return;
	}
					
	// stop BOTH actions -- it's not a trench!
	stop_room_action(room, ACT_FILLING_IN);
	stop_room_action(room, ACT_EXCAVATING);
	
	map = &(world_map[FLAT_X_COORD(room)][FLAT_Y_COORD(room)]);
	
	to_sect = sector_proto(get_room_extra_data(room, ROOM_EXTRA_TRENCH_ORIGINAL_SECTOR));
	
	if (!to_sect) {	// backup plan
		if (SECT(room) != map->natural_sector) {
			// return to nature
			to_sect = map->natural_sector;
		}
		else {
			// de-evolve sect
			to_sect = reverse_lookup_evolution_for_sector(SECT(room), EVO_TRENCH_START);
		}
	}
	
	if (to_sect) {
		change_terrain(room, GET_SECT_VNUM(to_sect), NOTHING);
	}
	
	remove_room_extra_data(room, ROOM_EXTRA_TRENCH_ORIGINAL_SECTOR);
}


/**
* Sets the burn-down time for a room and, on request, also schedules it to
* happen.
*
* @param room_data *room The room to set burn-down time for.
* @param time_t when The timestamp when the building will burn down (0 for never/not-burning).
* @param bool schedule_burn Whether or not to start the burn-down event (you should usually pass TRUE here).
*/
void set_burn_down_time(room_data *room, time_t when, bool schedule_burn) {
	if (COMPLEX_DATA(room)) {
		COMPLEX_DATA(room)->burn_down_time = when;
		cancel_stored_event_room(room, SEV_BURN_DOWN);
		if (when > 0 && schedule_burn) {
			schedule_burn_down(room);
		}
		request_world_save(GET_ROOM_VNUM(room), WSAVE_ROOM);
	}
}


/**
* Set the crop on a room, and mark it on the base map.
*
* @param room_data *room The room to set crop for.
* @param crop_data *cp The crop to set.
*/
void set_crop_type(room_data *room, crop_data *cp) {
	if (!room) {
		return;
	}
	
	ROOM_CROP(room) = cp;
	if (GET_ROOM_VNUM(room) < MAP_SIZE) {
		world_map[FLAT_X_COORD(room)][FLAT_Y_COORD(room)].crop_type = cp;
		
		// check locking
		if (cp && ROOM_SECT_FLAGGED(room, SECTF_CROP) && CROP_FLAGGED(cp, CROPF_LOCK_ICON)) {
			lock_icon(room, NULL);
		}
		
		request_world_save(GET_ROOM_VNUM(room), WSAVE_MAP);
	}
	else {
		request_world_save(GET_ROOM_VNUM(room), WSAVE_ROOM);
	}
}


/**
* Updates the damage on a room. This does NOT cause it to be destroyed; only
* sets the value.
*
* @param room_data *room The room.
* @param double damage_amount How much to set its damage to (not bounded).
*/
void set_room_damage(room_data *room, double damage_amount) {
	// rounding to 0.01
	damage_amount = round(damage_amount * 100.0) / 100.0;
	if (COMPLEX_DATA(room) && COMPLEX_DATA(room)->damage != damage_amount) {
		COMPLEX_DATA(room)->damage = damage_amount;
		request_world_save(GET_ROOM_VNUM(room), WSAVE_ROOM);
	}
}


 //////////////////////////////////////////////////////////////////////////////
//// MANAGEMENT //////////////////////////////////////////////////////////////

/**
* Frees up any map rooms that are no longer needed, and schedules the rest
* for delayed un-loads. To be run at the end of startup.
*/
void schedule_map_unloads(void) {
	room_data *room, *next_room;
	
	HASH_ITER(hh, world_table, room, next_room) {
		if (GET_ROOM_VNUM(room) >= MAP_SIZE) {
			continue;	// not a map room
		}
		
		if (CAN_UNLOAD_MAP_ROOM(room)) {	// unload it now
			delete_room(room, FALSE);	// no need to check exits (CAN_UNLOAD_MAP_ROOM checks them)
		}
		else {	// set up unload event
			if (!ROOM_OWNER(room)) {
				schedule_check_unload(room, TRUE);
			}
		}
	}
}


 //////////////////////////////////////////////////////////////////////////////
//// WORLD RESET CYCLE ///////////////////////////////////////////////////////

/**
* Updates a set of depletions (quarters them each cycle). This is part of the
* world reset.
*
* @param struct depletion_data **list The list to update.
*/
void annual_update_depletions(struct depletion_data **list) {
	struct depletion_data *dep, *next_dep;
	
	LL_FOREACH_SAFE(*list, dep, next_dep) {
		// quarter each year
		dep->count /= 4;

		if (dep->count < 10) {
			// at this point just remove it to save space, or it will take years to hit 0
			LL_DELETE(*list, dep);
			free(dep);
		}
	}
}


/**
* Process map-only world resets on one map tile.
* Note: This is not called on OCEAN tiles (ones that don't go in the land map).
*
* @param struct map_data *tile The map tile to update.
*/
void annual_update_map_tile(struct map_data *tile) {
	struct affected_type *aff, *next_aff;
	struct resource_data *old_list;
	sector_data *old_sect;
	int trenched, amount;
	empire_data *emp;
	room_data *room;
	double dmg;
	
	// actual room is optional
	room = tile->room;
	
	// updates that only matter if there's a room:
	if (room) {
		if (ROOM_BLD_FLAGGED(room, BLD_IS_RUINS)) {
			// roughly 2 real years for average chance for ruins to be gone
			if (!number(0, 89)) {
				if (!GET_BUILDING(room) || !BLD_FLAGGED(GET_BUILDING(room), BLD_NO_ABANDON_WHEN_RUINED)) {
					abandon_room(room);
				}
				disassociate_building(room);
			
				if (ROOM_PEOPLE(room)) {
					act("The ruins finally crumble to dust!", FALSE, ROOM_PEOPLE(room), NULL, NULL, TO_CHAR | TO_ROOM);
				}
			}
		}
		else if (GET_BUILDING(room) && !IS_CITY_CENTER(room) && HOME_ROOM(room) == room && !ROOM_AFF_FLAGGED(room, ROOM_AFF_UNCLAIMABLE | ROOM_AFF_NO_DISREPAIR | ROOM_AFF_TEMPORARY) && (!ROOM_OWNER(room) || !EMPIRE_IMM_ONLY(ROOM_OWNER(room)))) {
			// normal building decay:						TODO: City center building could be given the no-disrepair affect, but existing muds would need a patch-updater
		
			// determine damage to do each year
			if (IS_COMPLETE(room)) {
				dmg = (double) GET_BLD_MAX_DAMAGE(GET_BUILDING(room)) / (double) config_get_int("disrepair_limit");
			}
			else {
				dmg = (double) GET_BLD_MAX_DAMAGE(GET_BUILDING(room)) / (double) config_get_int("disrepair_limit_unfinished");
			}
			
			// ensure at least 1.0 and round to nearest 0.1
			dmg = MAX(1.0, dmg);
			dmg = round(dmg * 10.0) / 10.0;
		
			// apply damage
			set_room_damage(room, BUILDING_DAMAGE(room) + dmg);
		
			// apply maintenance resources (if any, and if it's not being dismantled, and it's either complete or has had at least 1 resource added)
			if (GET_BLD_REGULAR_MAINTENANCE(GET_BUILDING(room)) && !IS_DISMANTLING(room) && (IS_COMPLETE(room) || GET_BUILT_WITH(room))) {
				old_list = GET_BUILDING_RESOURCES(room);
				GET_BUILDING_RESOURCES(room) = combine_resources(old_list, GET_BLD_REGULAR_MAINTENANCE(GET_BUILDING(room)));
				free_resource_list(old_list);
			}
		
			// check high decay (damage) -- only if there is no instance
			if (!ROOM_AFF_FLAGGED(room, ROOM_AFF_HAS_INSTANCE) && BUILDING_DAMAGE(room) >= GET_BLD_MAX_DAMAGE(GET_BUILDING(room))) {
				emp = ROOM_OWNER(room);
				abandon_room(room);
			
				// 50% of the time we just abandon, the rest we also decay to ruins
				if (!number(0, 1)) {
					if (emp) {
						log_to_empire(emp, ELOG_TERRITORY, "%s (%d, %d) has crumbled to ruin", get_room_name(room, FALSE), X_COORD(room), Y_COORD(room));
					}
					ruin_one_building(room);
				}
				else if (emp) {
					log_to_empire(emp, ELOG_TERRITORY, "%s (%d, %d) has been abandoned due to decay", get_room_name(room, FALSE), X_COORD(room), Y_COORD(room));
				}
			}
		}
		
		// destroy roads -- randomly over time
		if (IS_ROAD(room) && !ROOM_OWNER(room) && number(1, 100) <= 2) {
			// this will tear it back down to its base type
			disassociate_building(room);
		}
		
		// random chance to lose permanent affs
		if (!ROOM_OWNER(room)) {
			LL_FOREACH_SAFE(ROOM_AFFECTS(room), aff, next_aff) {
				if (aff->expire_time == UNLIMITED && number(1, 100) <= 5) {
					affect_remove_room(room, aff);
				}
			}
		}
	}
	
	// 33% chance that unclaimed non-wild crops vanish, 2% chance of wild crops vanishing
	if (tile->crop_type && (!room || !ROOM_OWNER(room)) && ((!CROP_FLAGGED(tile->crop_type, CROPF_NOT_WILD) && !number(0, 49)) || (CROP_FLAGGED(tile->crop_type, CROPF_NOT_WILD) && !number(0, 2)))) {
		if (!room) {	// load room if needed
			room = real_room(tile->vnum);
		}
		
		// change to base sect
		uncrop_tile(room);
		
		// stop all possible chores here since the sector changed
		stop_room_action(room, ACT_HARVESTING);
		stop_room_action(room, ACT_CHOPPING);
		stop_room_action(room, ACT_PICKING);
		stop_room_action(room, ACT_GATHERING);
	}
	
	// fill in trenches slightly
	if (SECT_FLAGGED(tile->sector_type, SECTF_IS_TRENCH) && (!room || !ROOM_OWNER(room)) && (trenched = get_extra_data(tile->shared->extra_data, ROOM_EXTRA_TRENCH_PROGRESS)) < 0) {
		// move halfway toward initial: remember initial value is negative
		amount = (config_get_int("trench_initial_value") - trenched) / 2;
		trenched += amount;
		if (trenched > config_get_int("trench_initial_value") + 10) {
			set_extra_data(&tile->shared->extra_data, ROOM_EXTRA_TRENCH_PROGRESS, trenched);
		}
		else {
			if (!room) {
				room = real_room(tile->vnum);	// need room in memory
			}
			
			// untrench
			untrench_room(room);
			if (ROOM_PEOPLE(room)) {
				act("The trench collapses!", FALSE, ROOM_PEOPLE(room), NULL, NULL, TO_CHAR | TO_ROOM);
			}
		}
	}
	// randomly un-trench abandoned canals
	if ((!room || !ROOM_OWNER(room)) && number(1, 100) <= 2 && tile->sector_type != tile->natural_sector && (old_sect = reverse_lookup_evolution_for_sector(tile->sector_type, EVO_TRENCH_FULL))) {
		if (!room) {
			room = real_room(tile->vnum);	// neeed room in memory
		}
		
		change_terrain(room, GET_SECT_VNUM(old_sect), NOTHING);
		set_room_extra_data(room, ROOM_EXTRA_TRENCH_PROGRESS, -1);
	}
	
	// clean mine data from anything that's not currently a mine
	if (!room || !room_has_function_and_city_ok(NULL, room, FNC_MINE)) {
		remove_extra_data(&tile->shared->extra_data, ROOM_EXTRA_MINE_GLB_VNUM);
		remove_extra_data(&tile->shared->extra_data, ROOM_EXTRA_MINE_AMOUNT);
		remove_extra_data(&tile->shared->extra_data, ROOM_EXTRA_PROSPECT_EMPIRE);
		remove_extra_data(&tile->shared->extra_data, ROOM_EXTRA_WORKFORCE_PROSPECT);
	}
}


/**
* Runs world reset (mainly maintenance) on the vehicle.
*/
void annual_update_vehicle(vehicle_data *veh) {
	char *msg;
	
	if (VEH_OWNER(veh) && EMPIRE_IMM_ONLY(VEH_OWNER(veh))) {
		return;	// skip immortal vehicles
	}
	
	// ensure a save
	request_vehicle_save_in_world(veh);
	
	// non-damage stuff:
	annual_update_depletions(&VEH_DEPLETION(veh));
	
	// does not take annual damage (unless incomplete)
	if (!VEH_REGULAR_MAINTENANCE(veh) && VEH_IS_COMPLETE(veh)) {
		// check if it's abandoned furniture: no owner, unowned room, no instance id, not in an adventure, no players here
		if (!VEH_OWNER(veh) && VEH_INSTANCE_ID(veh) != NOTHING && IN_ROOM(veh) && !ROOM_OWNER(IN_ROOM(veh)) && !IS_ADVENTURE_ROOM(IN_ROOM(veh)) && !any_players_in_room(IN_ROOM(veh))) {
			// random chance of decay
			if (!number(0, 51)) {
				msg = veh_get_custom_message(veh, VEH_CUSTOM_RUINS_TO_ROOM);
				ruin_vehicle(veh, msg ? msg : "$V is carted off!");
			}
		}
		return;
	}
	
	// prepare to decay (ruins have special handling here)
	if (VEH_FLAGGED(veh, VEH_IS_RUINS)) {
		// chance of ruining ruins: roughly 2 real years for average chance for ruins to be gone
		if (!number(0, 89)) {
			msg = veh_get_custom_message(veh, VEH_CUSTOM_RUINS_TO_ROOM);
			ruin_vehicle(veh, msg ? msg : "$V finally crumbles to dust!");
		}
	}
	else {	// normal decay (not ruins)
		decay_one_vehicle(veh, "$V crumbles from disrepair!");
	}
}


/**
* World reset: runs every "world_reset_hours" (real hours) to update the world.
*/
void annual_world_update(void) {
	char message[MAX_STRING_LENGTH];
	const char *str;
	vehicle_data *veh;
	descriptor_data *desc;
	room_data *room, *next_room;
	struct map_data *tile;
	int newb_count = 0;
	
	bool naturalize_newbie_islands = config_get_bool("naturalize_newbie_islands");
	bool naturalize_unclaimable = config_get_bool("naturalize_unclaimable");
	
	// MESSAGE TO ALL ?
	if ((str = config_get_string("world_reset_message")) && *str) {
		snprintf(message, sizeof(message), "\r\n%s\r\n\r\n", str);
		
		LL_FOREACH(descriptor_list, desc) {
			if (STATE(desc) == CON_PLAYING && desc->character) {
				write_to_descriptor(desc->descriptor, message);
				desc->has_prompt = FALSE;
			}
		}
	}
	
	// update ALL map tiles
	LL_FOREACH(land_map, tile) {
		annual_update_map_tile(tile);
		annual_update_depletions(&tile->shared->depletion);
		if (naturalize_newbie_islands) {
			newb_count += naturalize_newbie_island(tile, naturalize_unclaimable);
		}
	}
	
	// interiors (not map tiles)
	DL_FOREACH_SAFE2(interior_room_list, room, next_room, next_interior) {
		annual_update_depletions(&ROOM_DEPLETION(room));
	}
	
	DL_FOREACH_SAFE(vehicle_list, veh, global_next_vehicle) {
		annual_update_vehicle(veh);
	}
	
	// crumble cities that lost their buildings
	check_ruined_cities(NULL);
	
	// rename islands
	update_island_names();
	
	// store the time now
	data_set_long(DATA_LAST_NEW_YEAR, time(0));
	
	if (newb_count) {
		log("New year: naturalized %d tile%s on newbie islands.", newb_count, PLURAL(newb_count));
	}
}


/**
* Checks periodically if it's time to reset maintenance and depletions.
*
* This is based on DATA_LAST_NEW_YEAR and the "world_reset_hours" config.
*/
void check_maintenance_and_depletion_reset(void) {
	if (data_get_long(DATA_LAST_NEW_YEAR) + (config_get_int("world_reset_hours") * SECS_PER_REAL_HOUR) < time(0)) {
		annual_world_update();
	}
}


/**
* Restores a newbie island island to nature at the end of the year, if the
* "naturalize_newbie_islands" option is configured on.
*
* @param struct map_data *tile The map tile to try to naturalize.
* @param bool do_unclaim If FALSE, skips unclaimable rooms.
* @return int 1 if the tile was naturalized (for counting), 0 if not.
*/
int naturalize_newbie_island(struct map_data *tile, bool do_unclaim) {
	crop_data *new_crop;
	room_data *room;
	
	// simple checks
	if (tile->sector_type == tile->natural_sector) {
		return 0;	// already same
	}
	if (!tile->shared->island_ptr) {
		return 0;	// no bother
	}
	if (!IS_SET(tile->shared->island_ptr->flags, ISLE_NEWBIE)) {
		return 0;	// not newbie
	}
	
	// checks needed if the room exists
	if ((room = tile->room)) {
		if (ROOM_OWNER(room)) {
			return 0;	// owned rooms don't naturalize
		}
		if (ROOM_AFF_FLAGGED(room, ROOM_AFF_HAS_INSTANCE | ROOM_AFF_NO_EVOLVE)) {
			return 0;	// these flags also prevent it
		}
		if (ROOM_AFF_FLAGGED(room, ROOM_AFF_UNCLAIMABLE) && !do_unclaim) {
			return 0;
		}
	}
	
	// looks good: naturalize it
	if (room) {
		if (ROOM_PEOPLE(room)) {
			act("The area returns to nature!", FALSE, ROOM_PEOPLE(room), NULL, NULL, TO_CHAR | TO_ROOM);
		}
		decustomize_room(room);
		change_terrain(room, GET_SECT_VNUM(tile->natural_sector), NOTHING);
		
		// no longer need this
		remove_room_extra_data(room, ROOM_EXTRA_TRENCH_ORIGINAL_SECTOR);
	}
	else {
		decustomize_shared_data(tile->shared);
		perform_change_sect(NULL, tile, tile->natural_sector);
		perform_change_base_sect(NULL, tile, tile->natural_sector);
		
		if (SECT_FLAGGED(tile->natural_sector, SECTF_HAS_CROP_DATA)) {
			room = real_room(tile->vnum);	// need it loaded after all
			new_crop = get_potential_crop_for_location(room, NOTHING);
			set_crop_type(room, new_crop ? new_crop : crop_table);
		}
		else {
			tile->crop_type = NULL;
		}
		
		// no longer need this
		remove_extra_data(&tile->shared->extra_data, ROOM_EXTRA_TRENCH_ORIGINAL_SECTOR);
	}
	
	request_world_save(tile->vnum, WSAVE_MAP | WSAVE_ROOM);
	return 1;
}


/**
* Checks periodically to see if an empire is the only one left on an island,
* and renames the island if they have a custom name for it.
*/
void update_island_names(void) {
	empire_data *emp, *next_emp, *found_emp;
	struct island_info *isle, *next_isle;
	struct empire_city_data *city;
	struct empire_island *eisle;
	char *last_name = NULL;
	int count;
	
	HASH_ITER(hh, island_table, isle, next_isle) {
		if (isle->id == NO_ISLAND || IS_SET(isle->flags, ISLE_NO_CUSTOMIZE)) {
			continue;
		}
		
		// look for empires with cities on the island
		found_emp = NULL;
		last_name = NULL;
		count = 0;
		HASH_ITER(hh, empire_table, emp, next_emp) {
			LL_FOREACH(EMPIRE_CITY_LIST(emp), city) {
				if (GET_ISLAND_ID(city->location) == isle->id) {
					eisle = get_empire_island(emp, isle->id);
					
					// if the empire HAS named the island
					if (eisle->name) {
						if (!last_name) {
							found_emp = emp;	// found an empire with a name
							last_name = eisle->name;
							++count;	// only count in this case
						}
						else if (!str_cmp(eisle->name, last_name)) {
							// matches last name, do nothing
						}
						else {
							++count;	// does not match last name
						}
					}
					else {
						++count;	// no name; always count
					}
					
					break;	// only care about 1 city per empire
				}
			}
			
			if (count > 1) {
				break;	// we only care if there's more than 1 empire
			}
		}
		
		if (count == 1 && found_emp) {
			eisle = get_empire_island(found_emp, isle->id);
			if (eisle->name && strcmp(eisle->name, isle->name)) {
				// HERE: We are now ready to change the name
				syslog(SYS_INFO, LVL_START_IMM, TRUE, "Island %d (%s) is now called %s (%s)", isle->id, NULLSAFE(isle->name), eisle->name, EMPIRE_NAME(found_emp));
				
				if (isle->name) {
					free(isle->name);
				}
				isle->name = str_dup(eisle->name);
				save_island_table();
			}
		}
	}
}


 //////////////////////////////////////////////////////////////////////////////
//// BURNING BUILDINGS ///////////////////////////////////////////////////////

// see dg_event.c/h
EVENTFUNC(burn_down_event) {
	struct room_event_data *burn_data = (struct room_event_data *)event_obj;
	obj_data *obj, *next_obj;
	char_data *ch, *next_ch;
	empire_data *emp;
	room_data *room;
	bool junk;
	
	// grab data and free it
	room = burn_data->room;
	emp = ROOM_OWNER(room);
	free(burn_data);
	
	// remove this first
	delete_stored_event_room(room, SEV_BURN_DOWN);
	if (!COMPLEX_DATA(room)) {
		// no complex data? nothing to burn
		return 0;
	}
	
	if (ROOM_IS_CLOSED(room)) {
		if (ROOM_PEOPLE(room)) {
			act("The building collapses in flames around you!", FALSE, ROOM_PEOPLE(room), NULL, NULL, TO_CHAR | TO_ROOM);
		}
		DL_FOREACH_SAFE2(ROOM_PEOPLE(room), ch, next_ch, next_in_room) {
			if (!IS_NPC(ch)) {
				death_log(ch, ch, ATTACK_SUFFERING);
			}
			die(ch, ch);
		}
	}
	else {
		// not closed
		if (ROOM_PEOPLE(room)) {
			act("The building burns to the ground!", FALSE, ROOM_PEOPLE(room), NULL, NULL, TO_CHAR | TO_ROOM);
		}
	}
	
	// inform
	if (emp) {
		log_to_empire(emp, ELOG_HOSTILITY, "Your %s has burned down at (%d, %d)", (GET_BUILDING(room) ? GET_BLD_NAME(GET_BUILDING(room)) : "building"), X_COORD(room), Y_COORD(room));
	}
	
	// auto-abandon if not in city
	if (emp && !is_in_city_for_empire(room, emp, TRUE, &junk)) {
		// does check the city time limit for abandon protection
		abandon_room(room);
	}
	
	disassociate_building(room);

	// Destroy 50% of the objects
	DL_FOREACH_SAFE2(ROOM_CONTENTS(room), obj, next_obj, next_content) {
		if (!number(0, 1)) {
			extract_obj(obj);
		}
	}
	
	// do not reenqueue
	return 0;
}


// frees memory when burning is canceled
EVENT_CANCEL_FUNC(cancel_burn_event) {
	struct room_event_data *burn_data = (struct room_event_data *)event_obj;
	free(burn_data);
}


/**
* Takes a building that already has a burning timer, and schedules the burn-
* down event for it.
*
* @param room_data *room The room to burn.
*/
void schedule_burn_down(room_data *room) {
	struct room_event_data *burn_data;
	struct dg_event *ev;
	
	if (COMPLEX_DATA(room) && COMPLEX_DATA(room)->burn_down_time > 0) {
		// create the event
		CREATE(burn_data, struct room_event_data, 1);
		burn_data->room = room;
		
		ev = dg_event_create(burn_down_event, burn_data, (COMPLEX_DATA(room)->burn_down_time - time(0)) * PASSES_PER_SEC);
		add_stored_event_room(room, SEV_BURN_DOWN, ev);
	}
}


/**
* Starts a room on fire. This sends no messages. Only the "home room" matters
* for burning, so this sets the data there.
*
* @param room_data *room The room to burn.
*/
void start_burning(room_data *room) {
	descriptor_data *desc;
	
	room = HOME_ROOM(room);	// burning is always on the home room
	
	if (!COMPLEX_DATA(room)) {
		return;	// nothing to burn
	}
	
	set_room_extra_data(room, ROOM_EXTRA_FIRE_REMAINING, config_get_int("fire_extinguish_value"));
	set_burn_down_time(room, time(0) + get_burn_down_time_seconds(room), TRUE);
	
	// ensure no building or dismantling
	stop_room_action(room, ACT_BUILDING);
	stop_room_action(room, ACT_DISMANTLING);
	
	if (ROOM_OWNER(room)) {
		log_to_empire(ROOM_OWNER(room), ELOG_HOSTILITY, "Your %s has caught on fire at (%d, %d)", GET_BUILDING(room) ? skip_filler(GET_BLD_NAME(GET_BUILDING(room))) : "building", X_COORD(room), Y_COORD(room));
	}
	
	// messaging
	LL_FOREACH(descriptor_list, desc) {
		if (STATE(desc) == CON_PLAYING && desc->character && AWAKE(desc->character) && HOME_ROOM(IN_ROOM(desc->character)) == HOME_ROOM(room)) {
			msg_to_char(desc->character, "The building is on fire!\r\n");
		}
	}
}


/**
* Turns the fire off.
*
* @param room_data *room The room that might be burning.
*/
void stop_burning(room_data *room) {
	room = HOME_ROOM(room);	// always
	
	if (COMPLEX_DATA(room)) {
		set_burn_down_time(room, 0, FALSE);
		remove_room_extra_data(room, ROOM_EXTRA_FIRE_REMAINING);
	}
}


 //////////////////////////////////////////////////////////////////////////////
//// CITY LIB ////////////////////////////////////////////////////////////////

/**
* @param empire_data *emp
* @return int how many city points emp has to spend
*/
int city_points_available(empire_data *emp) {
	int points = 0;
	
	if (emp) {
		// only get points if members are active
		if (EMPIRE_MEMBERS(emp) > 0) {
			points = 1;
			points += ((EMPIRE_MEMBERS(emp) - 1) / config_get_int("players_per_city_point"));
			points += EMPIRE_ATTRIBUTE(emp, EATT_BONUS_CITY_POINTS);
		}

		// minus any used points
		points -= count_city_points_used(emp);
	}

	return points;
}


/**
* @param empire_data *emp The empire.
* @return int The number of individual cities owned (not counting city points).
*/
int count_cities(empire_data *emp) {
	struct empire_city_data *city;
	int count = 0;
	
	if (emp) {
		for (city = EMPIRE_CITY_LIST(emp); city; city = city->next) {
			++count;
		}
	}
	
	return count;
}


/**
* @param empire_data *emp
* @return int number of city points emp has used (cities x city size)
*/
int count_city_points_used(empire_data *emp) {
	struct empire_city_data *city;
	int count = 0;
	
	if (emp) {
		for (city = EMPIRE_CITY_LIST(emp); city; city = city->next) {
			count += (city->type + 1);
		}
	}
	
	return count;
}


/**
* Create a new city entry
*
* @param empire_data *emp which empire
* @param char *name city name
* @param room_data *location A place to found it.
* @param int type city size/type (city_type[])
* @return struct empire_city_data* the city object
*/
struct empire_city_data *create_city_entry(empire_data *emp, char *name, room_data *location, int type) {
	char buf[256];
	struct empire_city_data *city;
	
	// sanity first
	if (!location) {
		return NULL;
	}
	
	if (!emp) {
		log("SYSERR: trying to create city entry for no-empire");
	}
	
	CREATE(city, struct empire_city_data, 1);
	
	city->name = str_dup(name);
	city->location = location;
	city->type = type;
	
	city->traits = EMPIRE_FRONTIER_TRAITS(emp);	// defaults
	
	LL_APPEND(EMPIRE_CITY_LIST(emp), city);
	
	// check building exists
	if (!IS_CITY_CENTER(location)) {
		construct_building(location, BUILDING_CITY_CENTER);
		set_room_extra_data(location, ROOM_EXTRA_FOUND_TIME, time(0));
		complete_building(location);
	}
	
	// rename
	snprintf(buf, sizeof(buf), "The Center of %s", name);
	set_room_custom_name(location, buf);
	
	// verify ownership
	claim_room(location, emp);
	
	return city;
}


 //////////////////////////////////////////////////////////////////////////////
//// ROOM RESETS /////////////////////////////////////////////////////////////

/**
* Runs reset commands on 1 room and then deletes those commands so they won't
* run a second time.
*
* @param room_data *room The room to reset.
*/
void reset_one_room(room_data *room) {
	char field[256], str[MAX_INPUT_LENGTH];
	struct affected_type *aff;
	struct reset_com *reset, *next_reset;
	char_data *tmob = NULL; /* for trigger assignment */
	char_data *mob = NULL;
	morph_data *morph;
	trig_data *trig;
	
	// shortcut
	if (!room->reset_commands) {
		return;
	}
	
	// start loading
	DL_FOREACH_SAFE(room->reset_commands, reset, next_reset) {
		switch (reset->command) {
			case 'A': {	// add affect to mob
				if (mob) {
					aff = create_aff(reset->arg1, reset->arg3, reset->arg5, reset->arg4, asciiflag_conv(reset->sarg1), NULL);
					aff->cast_by = reset->arg2;
					// arg3 comes in as either UNLIMITED or seconds-left, so no conversion needed
					affect_to_char_silent(mob, aff);
					free(aff);
				}
				break;
			}
			case 'M': {	// read a mobile
				mob = read_mobile(reset->arg1, FALSE);	// no scripts
				MOB_FLAGS(mob) = reset->arg2;
				char_to_room(mob, room);
				
				// sanity
				SET_BIT(MOB_FLAGS(mob), MOB_ISNPC);
				REMOVE_BIT(MOB_FLAGS(mob), MOB_EXTRACTED);
				
				GET_ROPE_VNUM(mob) = reset->arg3;
				check_scheduled_events_mob(mob);
				
				tmob = mob;
				break;
			}
			case 'N': {	// extra mob info
				if (mob) {
					if (reset->arg2 > 0) {
						set_mob_spawn_time(mob, reset->arg2);
					}
				}
				break;
			}
			
			case 'C': {	// add cooldown to mob
				if (mob) {
					add_cooldown(mob, reset->arg1, reset->arg2);
				}
				break;
			}
			
			case 'I': {	// add instance data to mob
				if (mob) {
					MOB_INSTANCE_ID(mob) = reset->arg1;
					add_instance_mob(real_instance(reset->arg1), GET_MOB_VNUM(mob));
				}
				break;
			}
			
			case 'Y': {	// add customizations to mob
				if (mob) {
					if (reset->arg1 != NOTHING) {
						change_sex(mob, reset->arg1);
						MOB_DYNAMIC_SEX(mob) = reset->arg1;
					}
					if (reset->arg2 != NOTHING) {
						MOB_DYNAMIC_NAME(mob) = reset->arg2;
					}
					if (reset->arg3 != NOTHING) {
						GET_LOYALTY(mob) = real_empire(reset->arg3);
					}
					
					// trigger actual sex/name
					setup_generic_npc(mob, GET_LOYALTY(mob), MOB_DYNAMIC_NAME(mob), MOB_DYNAMIC_SEX(mob));
				}
				break;
			}

			case 'O': {	// load an obj pack
				// if there's no arg1, assume it's an old pack file (second arg of objpack_load_room)
				objpack_load_room(room, (reset->arg1 == 0) ? TRUE : FALSE);
				break;
			}
			
			case 'S': { // custom string
				if (mob) {
					half_chop(reset->sarg1, field, str);
					if (is_abbrev(field, "sex")) {
						change_sex(mob, atoi(str));
					}
					else if (is_abbrev(field, "morph")) {
						if ((morph = morph_proto(atoi(str)))) {
							perform_morph(mob, morph);
						}
					}
					else if (is_abbrev(field, "keywords")) {
						change_keywords(mob, str);
					}
					else if (is_abbrev(field, "longdescription")) {
						strcat(str, "\r\n");	// required by long descs
						change_long_desc(mob, str);
					}
					else if (is_abbrev(field, "shortdescription")) {
						change_short_desc(mob, str);
					}
					else if (is_abbrev(field, "lookdescription")) {
						change_look_desc(mob, reset->sarg2, FALSE);
					}
					else {
						log("Warning: Unknown mob string in resets for room %d: C S %s", GET_ROOM_VNUM(room), reset->sarg1);
					}
				}
				break;
			}

			case 'T': {	// trigger attach
				if (reset->arg1 == MOB_TRIGGER && tmob) {
					if (!SCRIPT(tmob)) {
						create_script_data(tmob, MOB_TRIGGER);
					}
					if ((trig = read_trigger(reset->arg2))) {
						add_trigger(SCRIPT(tmob), trig, -1);
					}
				}
				else if (reset->arg1 == WLD_TRIGGER) {
					if (!room->script) {
						create_script_data(room, WLD_TRIGGER);
					}
					if ((trig = read_trigger(reset->arg2))) {
						add_trigger(room->script, trig, -1);
					}
				}
				break;
			}
			
			case 'V': {	// variable assignment
				if (reset->arg1 == MOB_TRIGGER && tmob) {
					if (!SCRIPT(tmob)) {
						create_script_data(tmob, MOB_TRIGGER);
					}
					add_var(&(SCRIPT(tmob)->global_vars), reset->sarg1, reset->sarg2, reset->arg3);
				}
				else if (reset->arg1 == WLD_TRIGGER) {
					if (!room->script) {
						create_script_data(room, WLD_TRIGGER);
					}
					add_var(&(room->script->global_vars), reset->sarg1, reset->sarg2, reset->arg3);
				}
				break;
			}
		}	// end switch
		
		// free as we go
		DL_DELETE(room->reset_commands, reset);
		if (reset->sarg1) {
			free(reset->sarg1);
		}
		if (reset->sarg2) {
			free(reset->sarg2);
		}
		free(reset);
	}
}


/**
* Resets all rooms that have reset commands waiting.
*/
void startup_room_reset(void) {
	room_data *room, *next_room;
	
	// prevent putting things in rooms from triggering a save
	block_world_save_requests = TRUE;
	
	// randomly spread out object timers so they're not all on the same second
	add_chaos_to_obj_timers = TRUE;
	
	HASH_ITER(hh, world_table, room, next_room) {
		affect_total_room(room);
		if (room->reset_commands) {
			reset_one_room(room);
		}
	}
	
	block_world_save_requests = FALSE;
	add_chaos_to_obj_timers = FALSE;
}


 //////////////////////////////////////////////////////////////////////////////
//// SECTOR INDEXING /////////////////////////////////////////////////////////


/**
* Finds the index entry for a sector type; adds a blank entry if it doesn't
* exist.
*
* @param sector_vnum vnum Which sector.
* @return struct sector_index_type* The index entry (guaranteed).
*/
struct sector_index_type *find_sector_index(sector_vnum vnum) {
	struct sector_index_type *idx;
	HASH_FIND_INT(sector_index, &vnum, idx);
	if (!idx) {
		CREATE(idx, struct sector_index_type, 1);
		idx->vnum = vnum;
		HASH_ADD_INT(sector_index, vnum, idx);
	}
	return idx;
}


/**
* Change a room's base sector (and the world_map) from one type to another, and
* update counts. ALL base sector changes should be done through this function.
*
* @param room_data *loc The location to change (optional, or provide map).
* @param struct map_data *map The location to change (optional, or provide room).
* @param sector_data *sect The type to change it to.
*/
void perform_change_base_sect(room_data *loc, struct map_data *map, sector_data *sect) {
	struct sector_index_type *idx;
	sector_data *old_sect;
	
	if (!loc && !map) {
		log("SYSERR: perform_change_base_sect called without loc or map");
		return;
	}
	if (!sect) {
		log("SYSERR: perform_change_base_sect called without sect");
		return;
	}
	
	// preserve
	old_sect = (loc ? BASE_SECT(loc) : map->base_sector);
	
	// update room
	if (loc || (loc = map->room)) {
		BASE_SECT(loc) = sect;
		request_world_save(GET_ROOM_VNUM(loc), WSAVE_ROOM);
	}
	
	// update the world map
	if (map || (GET_ROOM_VNUM(loc) < MAP_SIZE && (map = &(world_map[FLAT_X_COORD(loc)][FLAT_Y_COORD(loc)])))) {
		map->base_sector = sect;
		request_world_save(map->vnum, WSAVE_MAP);
	}
	
	// old index
	if (old_sect) {	// does not exist at first instantiation/set
		idx = find_sector_index(GET_SECT_VNUM(old_sect));
		--idx->base_count;
		if (map) {
			LL_DELETE2(idx->base_rooms, map, next_in_base_sect);
		}
	}
	
	// new index
	idx = find_sector_index(GET_SECT_VNUM(sect));
	++idx->base_count;
	if (map) {
		LL_PREPEND2(idx->base_rooms, map, next_in_base_sect);
	}
}


/**
* Change a room's sector (and the world_map) from one type to another, and
* update counts. ALL sector changes should be done through this function.
*
* @param room_data *loc The location to change (optional, or provide map).
* @param struct map_data *map The location to change (optional, or provide room).
* @param sector_data *sect The type to change it to.
*/
void perform_change_sect(room_data *loc, struct map_data *map, sector_data *sect) {
	bool belongs = (loc && SECT(loc) && BELONGS_IN_TERRITORY_LIST(loc));
	struct empire_territory_data *ter;
	bool was_large, junk;
	struct sector_index_type *idx;
	sector_data *old_sect;
	int was_ter, is_ter;
	
	if (!loc && !map) {
		log("SYSERR: perform_change_sect called without loc or map");
		return;
	}
	if (!sect) {
		log("SYSERR: perform_change_sect called without sect");
		return;
	}
	
	// ensure we have loc if possible
	if (!loc) {
		loc = map->room;
	}
	
	// for updating territory counts
	was_large = (loc && SECT(loc)) ? LARGE_CITY_RADIUS(loc) : FALSE;
	was_ter = (loc && ROOM_OWNER(loc)) ? get_territory_type_for_empire(loc, ROOM_OWNER(loc), FALSE, &junk, NULL) : TER_FRONTIER;
	
	// preserve
	old_sect = (loc ? SECT(loc) : map->sector_type);
	
	// decustomize
	if (loc) {
		decustomize_room(loc);
	}
	else {
		decustomize_shared_data(map->shared);
	}
	
	// update room
	if (loc) {
		SECT(loc) = sect;
		request_world_save(GET_ROOM_VNUM(loc), WSAVE_ROOM);
	}
	
	// update the world map
	if (map || (GET_ROOM_VNUM(loc) < MAP_SIZE && (map = &(world_map[FLAT_X_COORD(loc)][FLAT_Y_COORD(loc)])))) {
		map->sector_type = sect;
		request_world_save(map->vnum, WSAVE_MAP);
	}
	
	if (old_sect && GET_SECT_VNUM(old_sect) == BASIC_OCEAN && GET_SECT_VNUM(sect) != BASIC_OCEAN && map->shared == &ocean_shared_data) {
		// converting from ocean to non-ocean
		map->shared = NULL;	// unlink ocean share
		CREATE(map->shared, struct shared_room_data, 1);
		
		map->shared->island_id = NO_ISLAND;	// it definitely was before, must still be
		map->shared->island_ptr = NULL;
		request_world_save(map->vnum, WSAVE_MAP);
		
		if (loc) {
			SHARED_DATA(loc) = map->shared;
		}
	}
	else if (GET_SECT_VNUM(sect) == BASIC_OCEAN && map->shared != &ocean_shared_data) {
		free_shared_room_data(map->shared);
		map->shared = &ocean_shared_data;
		
		if (loc) {
			SHARED_DATA(loc) = map->shared;
		}
	}
	
	// old index
	if (old_sect) {	// does not exist at first instantiation/set
		idx = find_sector_index(GET_SECT_VNUM(old_sect));
		--idx->sect_count;
		if (map) {
			LL_DELETE2(idx->sect_rooms, map, next_in_sect);
		}
	}
	
	// new index
	idx = find_sector_index(GET_SECT_VNUM(sect));
	++idx->sect_count;
	if (map) {
		LL_PREPEND2(idx->sect_rooms, map, next_in_sect);
	}
	
	// check for territory updates
	if (loc && ROOM_OWNER(loc)) {
		if (was_large != LARGE_CITY_RADIUS(loc)) {
			is_ter = get_territory_type_for_empire(loc, ROOM_OWNER(loc), FALSE, &junk, NULL);
			
			if (was_ter != is_ter) {	// did territory type change?
				SAFE_ADD(EMPIRE_TERRITORY(ROOM_OWNER(loc), was_ter), -1, 0, UINT_MAX, FALSE);
				SAFE_ADD(EMPIRE_TERRITORY(ROOM_OWNER(loc), is_ter), 1, 0, UINT_MAX, FALSE);
				
				// (total counts do not change)
				
				if (GET_ISLAND_ID(loc) != NO_ISLAND) {
					struct empire_island *eisle = get_empire_island(ROOM_OWNER(loc), GET_ISLAND_ID(loc));
					SAFE_ADD(eisle->territory[was_ter], -1, 0, UINT_MAX, FALSE);
					SAFE_ADD(eisle->territory[is_ter], 1, 0, UINT_MAX, FALSE);
				}
			}
		}
		if (belongs != BELONGS_IN_TERRITORY_LIST(loc)) {	// do we need to add/remove the territory entry?
			if (belongs && (ter = find_territory_entry(ROOM_OWNER(loc), loc))) {	// losing
				delete_territory_entry(ROOM_OWNER(loc), ter, TRUE);
			}
			else if (!belongs && !find_territory_entry(ROOM_OWNER(loc), loc)) {	// adding
				create_territory_entry(ROOM_OWNER(loc), loc);
			}
		}
	}
	
	// make sure its island status is correct
	check_island_assignment(loc, map);
	
	// if needed, set a sector evolution time
	if (has_evolution_type(sect, EVO_TIMED)) {
		set_extra_data(loc ? &ROOM_EXTRA_DATA(loc) : &map->shared->extra_data, ROOM_EXTRA_SECTOR_TIME, time(0));
	}
	else {
		remove_extra_data(loc ? &ROOM_EXTRA_DATA(loc) : &map->shared->extra_data, ROOM_EXTRA_SECTOR_TIME);
	}
	
	// check for icon locking
	if (SECT_FLAGGED(sect, SECTF_LOCK_ICON) || (SECT_FLAGGED(sect, SECTF_CROP) && map && map->crop_type && CROP_FLAGGED(map->crop_type, CROPF_LOCK_ICON)) || (SECT_FLAGGED(sect, SECTF_CROP) && loc && ROOM_CROP(loc) && CROP_FLAGGED(ROOM_CROP(loc), CROPF_LOCK_ICON))) {
		if (map) {
			lock_icon_map(map, NULL);
		}
		else if (loc) {
			lock_icon(loc, NULL);
		}
	}
	
	request_mapout_update(map ? map->vnum : GET_ROOM_VNUM(loc));
	request_world_save(map ? map->vnum : GET_ROOM_VNUM(loc), WSAVE_MAP | WSAVE_ROOM);
}


 //////////////////////////////////////////////////////////////////////////////
//// TERRITORY ///////////////////////////////////////////////////////////////

/**
* Mark empire technologies, fame, etc; based on room. You should call this
* BEFORE starting a dismantle, as it only works on completed buildings. Also
* call it when you finish a building, or claim one. Call it with add=FALSE
* BEFORE an abandon, dismantle, or other loss of building.
*
* @param empire_data *emp the empire
* @param room_data *room the room to check
* @param bool add Adds the techs if TRUE, or removes them if FALSE.
*/
void adjust_building_tech(empire_data *emp, room_data *room, bool add) {
	int amt = add ? 1 : -1;	// adding or removing 1
	struct empire_island *e_isle = NULL;
	struct empire_npc_data *npc;
	struct empire_territory_data *ter;
	
	// only care about buildings
	if (!emp || !room || !GET_BUILDING(room) || IS_INCOMPLETE(room) || IS_DISMANTLING(room)) {
		return;
	}
	// ensure we're in the world
	if (GET_ROOM_VEHICLE(room) && (!IN_ROOM(GET_ROOM_VEHICLE(room)) || !VEH_IS_COMPLETE(GET_ROOM_VEHICLE(room)) || VEH_IS_DISMANTLING(GET_ROOM_VEHICLE(room)))) {
		return;
	}
	
	// look up empire island
	if (GET_ISLAND_ID(room) != NO_ISLAND) {
		e_isle = get_empire_island(emp, GET_ISLAND_ID(room));
	}
	
	// WARNING: do not check in-city status on these ... it can change at a time when territory is not re-scanned
	
	// building techs
	if (HAS_FUNCTION(room, FNC_DOCKS)) {
		EMPIRE_TECH(emp, TECH_SEAPORT) += amt;
		if (e_isle) {
			e_isle->tech[TECH_SEAPORT] += amt;
		}
	}
	
	// island population
	if (e_isle && (ter = find_territory_entry(emp, room))) {
		LL_FOREACH(ter->npcs, npc) {
			e_isle->population += amt;
		}
	}
	
	// other traits from buildings?
	EMPIRE_MILITARY(emp) += GET_BLD_MILITARY(GET_BUILDING(room)) * amt;
	EMPIRE_FAME(emp) += GET_BLD_FAME(GET_BUILDING(room)) * amt;
	
	// re-send claim info in case it changed
	TRIGGER_DELAYED_REFRESH(emp, DELAY_REFRESH_MSDP_UPDATE_CLAIMS);
}


/**
* Tech/fame marker for vehicles. Call this after you finish a vehicle or
* destroy it.
*
* Note: Vehicles inside of moving vehicles do not contribute tech or fame.
*
* @param vehicle_data *veh The vehicle to update.
* @param int island_id Which island to apply to (optional; will detect from vehicle's room if NO_ISLAND is given).
* @param bool add Adds the techs if TRUE, or removes them if FALSE.
*/
void adjust_vehicle_tech(vehicle_data *veh, int island_id, bool add) {
	int amt = add ? 1 : -1;	// adding or removing 1
	empire_data *emp = NULL;
	struct empire_island *e_isle = NULL;
	
	if (veh) {
		emp = VEH_OWNER(veh);
		if (IN_ROOM(veh) && island_id == NO_ISLAND) {
			island_id = GET_ISLAND_ID(IN_ROOM(veh));	// if any
		}
	}
	
	// only care about
	if (!emp || !veh || !VEH_IS_COMPLETE(veh)) {
		return;
	}
	
	// for island stats
	e_isle = (island_id == NO_ISLAND) ? NULL : get_empire_island(emp, island_id);
	
	// techs
	if (IS_SET(VEH_FUNCTIONS(veh), FNC_DOCKS)) {
		EMPIRE_TECH(emp, TECH_SEAPORT) += amt;
		if (e_isle) {
			e_isle->tech[TECH_SEAPORT] += amt;
		}
	}
	
	// TODO: count vehicle artisan as population?
	
	// other traits from buildings?
	EMPIRE_MILITARY(emp) += VEH_MILITARY(veh) * amt;
	EMPIRE_FAME(emp) += VEH_FAME(veh) * amt;
	
	// re-send claim info in case it changed
	TRIGGER_DELAYED_REFRESH(emp, DELAY_REFRESH_MSDP_UPDATE_CLAIMS);
}


/**
* Resets the techs for one (or all) empire(s).
*
* @param empire_data *emp An empire if doing one, or NULL to clear all of them.
*/
void clear_empire_techs(empire_data *emp) {
	struct empire_island *isle, *next_isle;
	empire_data *iter, *next_iter;
	int sub;
	
	HASH_ITER(hh, empire_table, iter, next_iter) {
		// find one or all?
		if (emp && iter != emp) {
			continue;
		}
		
		// zero out existing islands
		HASH_ITER(hh, EMPIRE_ISLANDS(iter), isle, next_isle) {
			isle->population = 0;
			
			for (sub = 0; sub < NUM_TECHS; ++sub) {
				isle->tech[sub] = EMPIRE_BASE_TECH(iter, sub);
			}
		}
		// main techs
		for (sub = 0; sub < NUM_TECHS; ++sub) {
			EMPIRE_TECH(iter, sub) = EMPIRE_BASE_TECH(iter, sub);
		}
	}
}


/**
* Creates a fresh territory entry for room
*
* @param empire_data *emp The empire
* @param room_data *room The room to add
* @return struct empire_territory_data* The new entry
*/
struct empire_territory_data *create_territory_entry(empire_data *emp, room_data *room) {
	struct empire_territory_data *ter;
	room_vnum vnum = GET_ROOM_VNUM(room);
	
	// only add if not already in there
	HASH_FIND_INT(EMPIRE_TERRITORY_LIST(emp), &vnum, ter);
	if (!ter) {
		CREATE(ter, struct empire_territory_data, 1);
		ter->vnum = GET_ROOM_VNUM(room);
		ter->room = room;
		ter->population_timer = config_get_int("building_population_timer");
		ter->npcs = NULL;
		ter->marked = FALSE;
		
		HASH_ADD_INT(EMPIRE_TERRITORY_LIST(emp), vnum, ter);
	}
	
	return ter;
}


/**
* Deletes one territory entry from the empire.
*
* @param empire_data *emp Which empire.
* @param struct empire_territory_data *ter Which entry to delete.
* @param bool make_npcs_homeless If TRUE, any NPCs in the territory become homeless.
*/
void delete_territory_entry(empire_data *emp, struct empire_territory_data *ter, bool make_npcs_homeless) {
	// prevent loss
	if (ter == global_next_territory_entry) {
		global_next_territory_entry = ter->hh.next;
	}

	delete_room_npcs(NULL, ter, make_npcs_homeless);
	ter->npcs = NULL;
	
	HASH_DEL(EMPIRE_TERRITORY_LIST(emp), ter);
	free(ter);
}


/**
* This function sets up empire territory. It is called rarely after startup.
* It can be called on one specific empire, or it can be used for ALL empires.
*
* @param empire_data *emp The empire to read, or NULL for "all".
* @param bool check_tech If TRUE, also does techs (you should almost never do this)
*/
void read_empire_territory(empire_data *emp, bool check_tech) {
	struct empire_territory_data *ter, *next_ter;
	struct empire_island *isle, *next_isle;
	struct empire_npc_data *npc;
	room_data *iter, *next_iter;
	empire_data *e, *next_e;
	int pos, ter_type;
	bool junk;

	/* Init empires */
	HASH_ITER(hh, empire_table, e, next_e) {
		if (e == emp || !emp) {
			for (pos = 0; pos < NUM_TERRITORY_TYPES; ++pos) {
				EMPIRE_TERRITORY(e, pos) = 0;
			}
			EMPIRE_POPULATION(e) = 0;
			
			if (check_tech) {	// this will only be re-read if we check tech
				EMPIRE_MILITARY(e) = 0;
				EMPIRE_FAME(e) = 0;
			}
		
			read_vault(e);

			// reset marks to check for dead territory
			HASH_ITER(hh, EMPIRE_TERRITORY_LIST(e), ter, next_ter) {
				ter->marked = FALSE;
			}
			
			// reset counters
			HASH_ITER(hh, EMPIRE_ISLANDS(e), isle, next_isle) {
				// island population only resets if we're calling tech funcs
				if (check_tech) {
					isle->population = 0;
				}
				
				for (pos = 0; pos < NUM_TERRITORY_TYPES; ++pos) {
					isle->territory[pos] = 0;
				}
			}
		}
	}

	// scan the whole world
	HASH_ITER(hh, world_table, iter, next_iter) {
		if ((e = ROOM_OWNER(iter)) && (!emp || e == emp)) {
			// only count each building as 1
			if (COUNTS_AS_TERRITORY(iter)) {
				ter_type = get_territory_type_for_empire(iter, e, FALSE, &junk, NULL);
				
				SAFE_ADD(EMPIRE_TERRITORY(e, ter_type), 1, 0, UINT_MAX, FALSE);
				
				SAFE_ADD(EMPIRE_TERRITORY(e, TER_TOTAL), 1, 0, UINT_MAX, FALSE);
				
				if (GET_ISLAND_ID(iter) != NO_ISLAND) {
					isle = get_empire_island(e, GET_ISLAND_ID(iter));
					SAFE_ADD(isle->territory[ter_type], 1, 0, UINT_MAX, FALSE);
					SAFE_ADD(isle->territory[TER_TOTAL], 1, 0, UINT_MAX, FALSE);
				}
			}
			
			// this is only done if we are re-reading techs (also updates island population)
			if (check_tech) {
				adjust_building_tech(e, iter, TRUE);
			}
			
			// only some things are in the territory list
			if (BELONGS_IN_TERRITORY_LIST(iter)) {
				// locate
				if (!(ter = find_territory_entry(e, iter))) {
					// or create
					ter = create_territory_entry(e, iter);
				}
				
				// mark it added/found
				ter->marked = TRUE;
				
				if (IS_COMPLETE(iter)) {
					if (!GET_ROOM_VEHICLE(iter)) {
						isle = get_empire_island(e, GET_ISLAND_ID(iter));
					}
					for (npc = ter->npcs; npc; npc = npc->next) {
						EMPIRE_POPULATION(e) += 1;
					}
				}
			}
		}
	}
	
	// remove any territory that wasn't marked ... in case there is any
	HASH_ITER(hh, empire_table, e, next_e) {
		if (e == emp || !emp) {
			HASH_ITER(hh, EMPIRE_TERRITORY_LIST(e), ter, next_ter) {
				if (!ter->marked) {
					delete_territory_entry(e, ter, TRUE);
				}
			}
		}
	}
}


/**
* This function clears and re-reads empire technology. In the process, it also
* resets greatness and membership.
*
* @param empire_data *emp An empire if doing one, or NULL to clear all of them.
*/
void reread_empire_tech(empire_data *emp) {
	struct empire_island *isle, *next_isle;
	empire_data *iter, *next_iter;
	vehicle_data *veh;
	int sub;
	
	// nowork
	if (!empire_table) {
		return;
	}
	
	
	// reset first
	clear_empire_techs(emp);
	
	// re-read both things
	read_empire_members(emp, TRUE);
	read_empire_territory(emp, TRUE);
	
	// also read vehicles for tech/etc
	DL_FOREACH(vehicle_list, veh) {
		if (emp && VEH_OWNER(veh) != emp) {
			continue;	// only checking one
		}
		if (!VEH_OWNER(veh) || !VEH_IS_COMPLETE(veh)) {
			continue;	// skip unowned/unfinished
		}
		
		adjust_vehicle_tech(veh, GET_ISLAND_ID(IN_ROOM(veh)), TRUE);
	}
	
	// special-handling for imm-only empires: give them all techs
	HASH_ITER(hh, empire_table, iter, next_iter) {
		if (emp && iter != emp) {
			continue;
		}
		if (!EMPIRE_IMM_ONLY(iter)) {
			continue;
		}
		
		// give all techs
		for (sub = 0; sub < NUM_TECHS; ++sub) {
			EMPIRE_TECH(iter, sub) += 1;
		}
		HASH_ITER(hh, EMPIRE_ISLANDS(iter), isle, next_isle) {
			// give all island techs
			for (sub = 0; sub < NUM_TECHS; ++sub) {
				isle->tech[sub] += 1;
			}
		}
	}
	
	// trigger a re-sort now
	resort_empires(FALSE);
	
	// re-send MSDP claim data
	HASH_ITER(hh, empire_table, iter, next_iter) {
		if (!emp || iter == emp) {
			TRIGGER_DELAYED_REFRESH(iter, DELAY_REFRESH_MSDP_UPDATE_CLAIMS);
		}
	}
}


 //////////////////////////////////////////////////////////////////////////////
//// TRENCH FILLING //////////////////////////////////////////////////////////

// fills trench when complete
EVENTFUNC(trench_fill_event) {
	struct map_event_data *trench_data = (struct map_event_data *)event_obj;
	struct map_data *map;
	room_data *room;
	
	// grab data and free it
	map = trench_data->map;
	room = real_room(map->vnum);
	free(trench_data);
	
	// cancel this first
	delete_stored_event(&map->shared->events, SEV_TRENCH_FILL);
	
	// check if trenchy
	if (!SECT_FLAGGED(map->sector_type, SECTF_IS_TRENCH)) {
		return 0;
	}
	
	// fill mctrenchy
	fill_trench(room);
	
	// do not reenqueue
	return 0;
}


/**
* Schedules the event handler for trench filling.
*
* @param struct map_data *map The map tile to schedule it on.
*/
void schedule_trench_fill(struct map_data *map) {
	long when = get_extra_data(map->shared->extra_data, ROOM_EXTRA_TRENCH_FILL_TIME);
	struct map_event_data *trench_data;
	struct dg_event *ev;
	
	if (!find_stored_event(map->shared->events, SEV_TRENCH_FILL)) {
		CREATE(trench_data, struct map_event_data, 1);
		trench_data->map = map;
		
		ev = dg_event_create(trench_fill_event, (void*)trench_data, (when > 0 ? ((when - time(0)) * PASSES_PER_SEC) : 1));
		add_stored_event(&map->shared->events, SEV_TRENCH_FILL, ev);
	}
}


 //////////////////////////////////////////////////////////////////////////////
//// EVOLUTIONS //////////////////////////////////////////////////////////////

bool evolutions_pending = FALSE;	// used to trigger logs when the evolver fails
bool manual_evolutions = FALSE;	// when an imm runs this manually, they still get a log


// This system deals with importing evolutions from the external bin/evolve
// program, which write the EVOLUTION_FILE (hints).
// Prior to b5.16, evolutions were done in-game using lots of processor cycles.

/**
* Attempts to process 1 sector evolution from the import file. Minor validation
* is done in this function.
*
* @param room_vnum loc Which room is changing.
* @param sector_vnum old_sect Sect that room should already have.
* @param sector_vnum new_sect Sect that the room is becoming.
*/
bool import_one_evo(room_vnum loc, sector_vnum old_sect, sector_vnum new_sect) {
	room_data *room;
	
	if (loc < 0 || loc >= MAP_SIZE || !(room = real_room(loc))) {
		return FALSE;	// bad room
	}
	if (GET_SECT_VNUM(SECT(room)) != old_sect || !sector_proto(new_sect)) {
		return FALSE;	// incorrect/bad data
	}
	
	/* -- as of b5.19, entrances DO evolve ('help unstuck' explains how players now get out of buildings with bad entrances)
	if (is_entrance(room) || ROOM_AFF_FLAGGED(room, ROOM_AFF_HAS_INSTANCE)) {
		return FALSE;	// never evolve these
	}
	*/
	
	// seems ok... If the new sector has crop data, we should store the original (e.g. a desert that randomly grows into a crop)
	change_terrain(room, new_sect, (ROOM_SECT_FLAGGED(room, SECTF_HAS_CROP_DATA) && BASE_SECT(room) == SECT(room)) ? old_sect : NOTHING);
	
	// deactivate workforce if the room type changed
	if (ROOM_OWNER(room)) {
		deactivate_workforce_room(ROOM_OWNER(room), room);
	}
	
	return TRUE;
}


/**
* This is called by SIGUSR1 to import pending evolutions from the bin/evolve
* program.
*/
void process_import_evolutions(void) {
	struct evo_import_data dat;
	int total, changed;
	FILE *fl;
	
	evolutions_pending = FALSE;	// prevents error log
	
	if (!(fl = fopen(EVOLUTION_FILE, "rb"))) {
		// no file
		log("SYSERR: import_evolutions triggered by SIGUSR1 but no evolution file '%s' to import", EVOLUTION_FILE);
		return;
	}
	
	total = changed = 0;
	
	for (;;) {
		fread(&dat, sizeof(struct evo_import_data), 1, fl);
		if (feof(fl)) {
			break;
		}
		
		++total;
		if (import_one_evo(dat.vnum, dat.old_sect, dat.new_sect)) {
			++changed;
		}
	}
	
	// log only if it was a requested evolve
	if (manual_evolutions) {
		syslog(SYS_INFO, LVL_START_IMM, TRUE, "Imported %d/%d map evolutions", changed, total);
		manual_evolutions = FALSE;
	}
	
	// cleanup
	fclose(fl);
	unlink(EVOLUTION_FILE);
}


/**
* Requests the external evolution program.
*/
void run_external_evolutions(void) {
	char buf[MAX_STRING_LENGTH];
	
	if (evolutions_pending) {
		log("SYSERR: Unable to import map evolutions: bin/evolve program does not respond");
	}
	
	evolutions_pending = TRUE;
	snprintf(buf, sizeof(buf), "nice ../bin/evolve %d %d %d &", config_get_int("nearby_sector_distance"), DAY_OF_YEAR(main_time_info), (int) getpid());
	// syslog(SYS_INFO, LVL_START_IMM, TRUE, "Running map evolutions...");
	system(buf);
}


 //////////////////////////////////////////////////////////////////////////////
//// ISLAND DESCRIPTIONS /////////////////////////////////////////////////////

struct genisdesc_isle {
	int id;
	struct genisdesc_terrain *ters;
	UT_hash_handle hh;
};

struct genisdesc_terrain {
	any_vnum vnum;
	int count;
	UT_hash_handle hh;
};


// quick sorter for tiles by count
int genisdesc_sort(struct genisdesc_terrain *a, struct genisdesc_terrain *b) {
	return b->count - a->count;
}


/**
* This generates descriptions of islands that have no description, with size
* and terrain data.
*/
void generate_island_descriptions(void) {
	struct genisdesc_isle *isle, *next_isle, *isle_hash = NULL;
	struct island_info *isliter, *next_isliter;
	struct genisdesc_terrain *ter, *next_ter;
	char buf[MAX_STRING_LENGTH];
	struct map_data *map;
	bool any = FALSE;
	int temp, count;
	double prc;
	
	bool override = (data_get_long(DATA_LAST_ISLAND_DESCS) + SECS_PER_REAL_DAY < time(0));
	
	// first determine which islands need descs
	HASH_ITER(hh, island_table, isliter, next_isliter) {
		if (isliter->tile_size < 1) {
			continue;	// don't bother?
		}
		
		if (!isliter->desc || !*isliter->desc || (override && !IS_SET(isliter->flags, ISLE_HAS_CUSTOM_DESC))) {
			CREATE(isle, struct genisdesc_isle, 1);
			isle->id = isliter->id;
			HASH_ADD_INT(isle_hash, id, isle);
		}
	}
	
	if (!isle_hash) {
		return;	// no work (no missing descs)
	}
	
	// now count terrains
	LL_FOREACH(land_map, map) {
		if (map->base_sector && SECT_FLAGGED(map->base_sector, SECTF_OCEAN)) {
			continue;	// skip ocean-flagged tiles
		}
		
		// find island
		temp = map->shared->island_id;
		HASH_FIND_INT(isle_hash, &temp, isle);
		if (!isle) {
			continue;	// not looking for this island
		}
		
		// mark terrain
		temp = map->base_sector ? GET_SECT_VNUM(map->base_sector) : BASIC_OCEAN;	// in case of errors?
		HASH_FIND_INT(isle->ters, &temp, ter);
		if (!ter) {	// or create
			CREATE(ter, struct genisdesc_terrain, 1);
			ter->vnum = temp;
			HASH_ADD_INT(isle->ters, vnum, ter);
		}
		
		++ter->count;
	}
	
	// build descs
	HASH_ITER(hh, isle_hash, isle, next_isle) {
		if (!isle->ters || !(isliter = get_island(isle->id, FALSE))) {
			continue;	// no work?
		}
		
		HASH_SORT(isle->ters, genisdesc_sort);	// by tile count
		
		sprintf(buf, "The island has %d map tile%s", isliter->tile_size, PLURAL(isliter->tile_size));
		count = 0;
		HASH_ITER(hh, isle->ters, ter, next_ter) {
			prc = (double)ter->count / isliter->tile_size * 100.0;
			if (prc < 5.0) {
				continue;
			}
			
			sprintf(buf + strlen(buf), "%s%d%% %s", (count == 0 ? " and is " : ", "), (int)prc, GET_SECT_NAME(sector_proto(ter->vnum)));
			if (++count >= 10) {
				break;
			}
		}
		
		strcat(buf, ".\r\n");
		
		if (isliter->desc) {
			free(isliter->desc);
		}
		isliter->desc = str_dup(buf);
		
		format_text(&isliter->desc, (strlen(isliter->desc) > 80 ? FORMAT_INDENT : 0), NULL, MAX_STRING_LENGTH);
		any = TRUE;
	}
	
	// and free the data
	HASH_ITER(hh, isle_hash, isle, next_isle) {
		HASH_ITER(hh, isle->ters, ter, next_ter) {
			HASH_DEL(isle->ters, ter);
			free(ter);
		}
		HASH_DEL(isle_hash, isle);
		free(isle);
	}
	
	if (any) {
		save_island_table();
	}
	data_set_long(DATA_LAST_ISLAND_DESCS, time(0));
}


 //////////////////////////////////////////////////////////////////////////////
//// HELPERS /////////////////////////////////////////////////////////////////

/**
* Validates all exits in the game, especially after a bunch of rooms have been
* deleted.
*/
void check_all_exits(void) {
	struct room_direction_data *ex, *next_ex;
	room_data *room, *next_room;
	obj_data *o, *next_o;
	
	// search world for portals that link to bad rooms
	DL_FOREACH_SAFE(object_list, o, next_o) {
		if (IS_PORTAL(o) && !real_real_room(GET_PORTAL_TARGET_VNUM(o))) {
			if (IN_ROOM(o) && ROOM_PEOPLE(IN_ROOM(o))) {
				act("$p closes and vanishes!", FALSE, ROOM_PEOPLE(IN_ROOM(o)), o, NULL, TO_CHAR | TO_ROOM);
			}
			extract_obj(o);
		}
	}
	
	// exits
	HASH_ITER(hh, world_table, room, next_room) {
		if (COMPLEX_DATA(room)) {
			for (ex = COMPLEX_DATA(room)->exits; ex; ex = next_ex) {
				next_ex = ex->next;
				
				// check for missing target
				if (!real_real_room(ex->to_room)) {
					if (ex->keyword) {
						free(ex->keyword);
					}
					LL_DELETE(COMPLEX_DATA(room)->exits, ex);
					free(ex);
					
					// no need to update GET_EXITS_HERE() as the target room is gone
				}
			}	
		}
	}
}


/**
* Ensures an island has the right min/max player levels marked.
*
* @param room_data *location Where to check.
* @param int level The level to mark.
*/
void check_island_levels(room_data *location, int level) {
	struct island_info *isle = location ? GET_ISLAND(location) : NULL;
	
	if (isle) {
		if (level > 0 && (!isle->min_level || level < isle->min_level)) {
			isle->min_level = level;
		}
		isle->max_level = MAX(isle->max_level, level);
	}
}


// checks if the room can be unloaded, and does it
EVENTFUNC(check_unload_room) {
	struct room_event_data *data = (struct room_event_data *)event_obj;
	room_data *room;
	
	// grab data but don't free
	room = data->room;
	
	if (!room) {	// somehow
		log("SYSERR: check_unload_room called with null room");
		free(data);
		return 0;
	}
	
	if (CAN_UNLOAD_MAP_ROOM(room)) {
		free(data);
		ROOM_UNLOAD_EVENT(room) = NULL;
		delete_room(room, FALSE);	// no need to check exits (CAN_UNLOAD_MAP_ROOM checks them)
		return 0;	// do not reenqueue
	}
	else {
		return (5 * 60) RL_SEC;	// reenqueue for 5 minutes
	}
}


/**
* clears ROOM_PRIVATE_OWNER for id
*
* @param int id The player id to clear rooms for.
*/
void clear_private_owner(int id) {
	room_data *iter, *next_iter;
	obj_data *obj;
	
	// check interior rooms first
	DL_FOREACH2(interior_room_list, iter, next_interior) {
		if (ROOM_PRIVATE_OWNER(HOME_ROOM(iter)) == id) {
			// reset autostore timer
			DL_FOREACH2(ROOM_CONTENTS(iter), obj, next_content) {
				schedule_obj_autostore_check(obj, time(0));
			}
		}
	}
	
	// now actually clear it from any remaining rooms
	HASH_ITER(hh, world_table, iter, next_iter) {
		if (COMPLEX_DATA(iter) && ROOM_PRIVATE_OWNER(iter) == id) {
			set_private_owner(iter, NOBODY);
			
			// reset autostore timer
			DL_FOREACH2(ROOM_CONTENTS(iter), obj, next_content) {
				schedule_obj_autostore_check(obj, time(0));
			}
		}
	}
}


/**
* Gets a simple movable room based on valid exits. This does not take a player
* into account, only that a room exists that way and it's possible to move to
* it.
*
* @param room_data *room Origin room.
* @param int dir Which way to target.
* @param bool ignore_entrance If TRUE, doesn't care which way the target tile is entered (e.g. for siege)
* @return room_data* The target room if it was valid, or NULL if not.
*/
room_data *dir_to_room(room_data *room, int dir, bool ignore_entrance) {
	struct room_direction_data *ex;
	room_data *to_room = NULL;
	
	// on the map
	if (!ROOM_IS_CLOSED(room) && GET_ROOM_VNUM(room) < MAP_SIZE) {
		if (dir >= NUM_2D_DIRS || dir < 0) {
			return NULL;
		}
		// may produce a NULL
		to_room = real_shift(room, shift_dir[dir][0], shift_dir[dir][1]);
		
		// check building entrance
		if (!ignore_entrance && to_room && IS_MAP_BUILDING(to_room) && !IS_INSIDE(room) && !IS_ADVENTURE_ROOM(room) && BUILDING_ENTRANCE(to_room) != dir && ROOM_IS_CLOSED(to_room) && (!ROOM_BLD_FLAGGED(to_room, BLD_TWO_ENTRANCES) || BUILDING_ENTRANCE(to_room) != rev_dir[dir])) {
			to_room = NULL;	// can't enter this way
		}
		else if (to_room == room) {
			to_room = NULL;	// don't return a shifted exit back to the room we're already in
		}
	}
	else {	// not on the map
		if (!(ex = find_exit(room, dir)) || !ex->room_ptr) {
			return NULL;
		}
		if (EXIT_FLAGGED(ex, EX_CLOSED) && ex->keyword) {
			return NULL;
		}
		to_room = ex->room_ptr;
	}
	
	return to_room;
}


/**
* Finds/creates the room where mobs go when extractions are pending.
*
* @return room_data* The extraction room.
*/
room_data *get_extraction_room(void) {
	room_data *room, *iter;
	
	DL_FOREACH2(interior_room_list, iter, next_interior) {
		if (GET_BUILDING(iter) && GET_BLD_VNUM(GET_BUILDING(iter)) == RTYPE_EXTRACTION_PIT) {
			return iter;
		}
	}
	
	// did not find -- make one
	room = create_room(NULL);
	attach_building_to_room(building_proto(RTYPE_EXTRACTION_PIT), room, TRUE);
	GET_ISLAND_ID(room) = NO_ISLAND;
	GET_ISLAND(room) = NULL;
	
	return room;
}


/**
* Creates a blank map room from the world_map data. This allows parts of the
* map to be unloaded using CAN_UNLOAD_MAP_ROOM(); this function builds new
* rooms as-needed.
*
* @param room_vnum vnum The vnum of the map room to load.
* @param bool schedule_unload If TRUE, runs schedule_check_unload().
* @return room_data* A fresh map room.
*/
room_data *load_map_room(room_vnum vnum, bool schedule_unload) {
	struct map_data *map;
	room_data *room;
	
	if (vnum < 0 || vnum >= MAP_SIZE) {
		log("SYSERR: load_map_room: request for out-of-bounds vnum %d", vnum);
		return NULL;
	}
	
	// find map data
	map = &(world_map[MAP_X_COORD(vnum)][MAP_Y_COORD(vnum)]);
	
	CREATE(room, room_data, 1);
	room->vnum = vnum;
	SHARED_DATA(room) = map->shared;	// point to map
	GET_MAP_LOC(room) = map;
	map->room = room;
	add_room_to_world_tables(room);
	
	// do not use perform_change_sect here because we're only loading from the existing data -- this already has height data
	SECT(room) = map->sector_type;
	BASE_SECT(room) = map->base_sector;
	
	ROOM_CROP(room) = map->crop_type;
	
	// checks if it's unloadable, and unloads it
	if (schedule_unload) {
		schedule_check_unload(room, FALSE);
	}
	
	return room;
}


/**
* Removes custom name/icon/desc on shared room data. Note: you must schedule
* a request_world_save(vnum, WSAVE_ROOM) after this.
*
* @param struct shared_room_data *shared The shared data to decustomize.
*/
void decustomize_shared_data(struct shared_room_data *shared) {
	if (shared) {
		if (shared->name) {
			free(shared->name);
			shared->name = NULL;
		}
		if (shared->description) {
			free(shared->description);
			shared->description = NULL;
		}
		if (shared->icon) {
			free(shared->icon);
			shared->icon = NULL;
		}
	}
	REMOVE_BIT(shared->affects, ROOM_AFF_HIDE_REAL_NAME);
	REMOVE_BIT(shared->base_affects, ROOM_AFF_HIDE_REAL_NAME);
}


/**
* Removes the custom name/icon/description on rooms
*
* @param room_data *room
*/
void decustomize_room(room_data *room) {
	if (room) {
		decustomize_shared_data(SHARED_DATA(room));
		request_world_save(GET_ROOM_VNUM(room), WSAVE_ROOM);
	}
}


/**
* Fetches island data for the empire, and creates an entry if it doesn't exist.
*
* @param empire_data *emp Empire to get data for.
* @param int island_id Which island.
* @return struct empire_island* The data for that island.
*/
struct empire_island *get_empire_island(empire_data *emp, int island_id) {
	struct empire_island *isle;
	
	HASH_FIND_INT(EMPIRE_ISLANDS(emp), &island_id, isle);
	if (!isle) {
		CREATE(isle, struct empire_island, 1);
		isle->island = island_id;
		HASH_ADD_INT(EMPIRE_ISLANDS(emp), island, isle);
		HASH_SORT(EMPIRE_ISLANDS(emp), sort_empire_islands);
	}
	
	return isle;
}


/**
* @param empire_data *emp An empire.
* @return int The "main island" for the empire, or NO_ISLAND if none.
*/
int get_main_island(empire_data *emp) {
	struct empire_city_data *city, *biggest = NULL;
	room_data *iter, *next_iter;
	int size = 0;
	
	for (city = EMPIRE_CITY_LIST(emp); city; city = city->next) {
		if (biggest == NULL || city->type > size) {
			biggest = city;
			size = city->type;
		}
	}
	
	if (biggest) {
		return GET_ISLAND_ID(biggest->location);
	}
	
	// no? just find their first territory
	HASH_ITER(hh, world_table, iter, next_iter) {
		if (ROOM_OWNER(iter) == emp && GET_ISLAND_ID(iter) != NO_ISLAND) {
			return GET_ISLAND_ID(iter);
		}
	}
	
	// sad
	return NO_ISLAND;
}


/**
* This determines what kind of crop should grow in a given location, mainly
* for the purposes of spawning fresh crops. This is only an approximation.
* 
* @param room_data *location The location to pick a crop for.
* @param int must_have_interact Optional: Only pick crops with this interaction (pass NOTHING to skip).
* @return crop_data* Any crop, or NULL if it can't find one.
*/
crop_data *get_potential_crop_for_location(room_data *location, int must_have_interact) {
	int x = X_COORD(location), y = Y_COORD(location);
	bool water = find_flagged_sect_within_distance_from_room(location, SECTF_FRESH_WATER, NOBITS, config_get_int("water_crop_distance"));
	bool x_min_ok, x_max_ok, y_min_ok, y_max_ok;
	struct island_info *isle = NULL;
	crop_data *found, *crop, *next_crop;
	int num_found = 0;
	bitvector_t climate;
	
	// small amount of random so the edges of the crops are less linear on the map
	x += number(-10, 10);
	y += number(-10, 10);
	
	// bounds checks
	x = WRAP_X_COORD(x);
	y = WRAP_Y_COORD(y);
	
	climate = get_climate(location);
	
	// find any match
	found = NULL;
	HASH_ITER(hh, crop_table, crop, next_crop) {
		// basic checks
		if (!MATCH_CROP_SECTOR_CLIMATE(crop, climate)) {
			continue;
		}
		if (CROP_FLAGGED(crop, CROPF_REQUIRES_WATER) && !water) {
			continue;
		}
		if (CROP_FLAGGED(crop, CROPF_NOT_WILD)) {
			continue;	// must be wild
		}

		if (must_have_interact != NOTHING && !has_interaction(GET_CROP_INTERACTIONS(crop), must_have_interact)) {
			continue;
		}

		if (CROP_FLAGGED(crop, CROPF_NO_NEWBIE | CROPF_NEWBIE_ONLY)) {
			if (!isle) {
				isle = GET_ISLAND(location);
			}
			if (CROP_FLAGGED(crop, CROPF_NO_NEWBIE) && (!isle || IS_SET(isle->flags, ISLE_NEWBIE))) {
				continue;
			}
			if (CROP_FLAGGED(crop, CROPF_NEWBIE_ONLY) && (!isle || !IS_SET(isle->flags, ISLE_NEWBIE))) {
				continue;
			}
		}
		
		// check bounds
		x_min_ok = (x >= (GET_CROP_X_MIN(crop) * MAP_WIDTH / 100));
		x_max_ok = (x <= (GET_CROP_X_MAX(crop) * MAP_WIDTH / 100));
		y_min_ok = (y >= (GET_CROP_Y_MIN(crop) * MAP_HEIGHT / 100));
		y_max_ok = (y <= (GET_CROP_Y_MAX(crop) * MAP_HEIGHT / 100));
		
		if ((x_min_ok && x_max_ok) || (GET_CROP_X_MIN(crop) > GET_CROP_X_MAX(crop) && (x_min_ok || x_max_ok))) {
			if ((y_min_ok && y_max_ok) || (GET_CROP_Y_MIN(crop) > GET_CROP_Y_MAX(crop) && (y_min_ok || y_max_ok))) {
				// valid
				if (!number(0, num_found++) || !found) {
					found = crop;
				}
			}
		}
	}
	
	return found;	// if any
}


EVENTFUNC(grow_crop_event) {
	struct map_event_data *data = (struct map_event_data *)event_obj;
	struct map_data *map;
	
	// grab data and free it
	map = data->map;
	free(data);
	
	// remove this first
	delete_stored_event(&map->shared->events, SEV_GROW_CROP);
	
	grow_crop(map);
	
	// do not reenqueue
	return 0;
}


/**
* @return room_vnum The first unused world vnum available.
*/
room_vnum find_free_vnum(void) {
	static room_vnum last_find = MAP_SIZE-1;
	room_vnum iter;
	bool last_try = FALSE;
	
	do {
		// search starting at the end of the map, or at the last vnum we found
		for (iter = last_find + 1; iter < MAX_INT; ++iter) {
			if (!real_room(iter)) {
				last_find = iter;
				return iter;
			}
		}
	
		if (last_try) {
			log("SYSERR: find_free_vnum() could not find a free vnum");
			exit(1);
		}
		else {
			// if we got this far, things were really full... but let's try again
			last_find = MAP_SIZE-1;
			last_try = TRUE;
		}
	} while (TRUE);
}


/**
* Finishes crop growth on a tile.
*
* @aram struct map_data *map The map location to grow.
*/
void grow_crop(struct map_data *map) {
	struct evolution_data *evo;
	sector_data *stored, *becomes;
	
	remove_extra_data(&map->shared->extra_data, ROOM_EXTRA_SEED_TIME);
	
	// nothing to grow
	if (IS_SET(map->shared->affects, ROOM_AFF_NO_EVOLVE) || !SECT_FLAGGED(map->sector_type, SECTF_HAS_CROP_DATA) || !(evo = get_evolution_by_type(map->sector_type, EVO_CROP_GROWS)) || !(becomes = sector_proto(evo->becomes))) {
		return;
	}
	
	// only going to use the original sect if it was different -- this preserves the stored sect
	stored = (map->base_sector != map->sector_type) ? map->base_sector : NULL;
	
	perform_change_sect(NULL, map, becomes);
	
	if (stored) {
		perform_change_base_sect(NULL, map, stored);
	}
	remove_depletion_from_list(&map->shared->depletion, DPLTN_PICK);
	remove_depletion_from_list(&map->shared->depletion, DPLTN_FORAGE);
	request_world_save(map->vnum, WSAVE_ROOM);
}


void init_room(room_data *room, room_vnum vnum) {
	sector_data *inside = sector_proto(config_get_int("default_inside_sect"));
	
	if (!inside) {
		log("SYSERR: default_inside_sect does not exist");
		exit(1);
	}
	
	room->vnum = vnum;
	
	if (vnum < MAP_SIZE) {
		// this should NEVER happen... but just in case!
		SHARED_DATA(room) = world_map[MAP_X_COORD(vnum)][MAP_Y_COORD(vnum)].shared;
	}
	else {
		CREATE(SHARED_DATA(room), struct shared_room_data, 1);
	}

	room->owner = NULL;
	
	perform_change_sect(room, NULL, inside);
	perform_change_base_sect(room, NULL, inside);
	
	COMPLEX_DATA(room) = init_complex_data();	// no type at this point
	ROOM_LIGHTS(room) = 0;
		
	room->af = NULL;
	
	room->last_spawn_time = 0;

	room->contents = NULL;
	room->people = NULL;
}

/**
* Interaction function for building-ruins-to-building. This replaces the
* building, ignoring interaction quantity, and transfers built-with resources
* and contents.
*
* INTERACTION_FUNC provides: ch, interaction, inter_room, inter_mob, inter_item, inter_veh
*/
INTERACTION_FUNC(ruin_building_to_building_interaction) {
	struct resource_data *res, *next_res, *save = NULL;
	vehicle_data *veh_iter, *next_veh;
	room_data *to_room = NULL;
	bld_data *old_bld, *proto;
	double save_resources;
	int dir, paint;
	char *temp;
	
	if (!inter_room || !(proto = building_proto(interaction->vnum)) || GET_ROOM_VNUM(inter_room) >= MAP_SIZE) {
		return FALSE;	// safety: only works on the map
	}
	
	// save data
	paint = ROOM_PAINT_COLOR(inter_room);
	old_bld = GET_BUILDING(inter_room);
	dir = BUILDING_ENTRANCE(inter_room);
	
	// move resources...
	if (GET_BUILT_WITH(inter_room)) {
		save = GET_BUILT_WITH(inter_room);
		GET_BUILT_WITH(inter_room) = NULL;
	}
	else if (IS_DISMANTLING(inter_room)) {
		save = GET_BUILDING_RESOURCES(inter_room);
		GET_BUILDING_RESOURCES(inter_room) = NULL;
	}
	
	// abandon first -- this will take care of accessory rooms, too
	if (!old_bld || !BLD_FLAGGED(old_bld, BLD_NO_ABANDON_WHEN_RUINED)) {
		abandon_room(inter_room);
	}
	disassociate_building(inter_room);
	
	if (ROOM_PEOPLE(inter_room)) {	// messaging to anyone left
		act("The building around you crumbles to ruin!", FALSE, ROOM_PEOPLE(inter_room), NULL, NULL, TO_CHAR | TO_ROOM);
	}
	
	// remove any unclaimed/empty vehicles (like furniture) -- those crumble with the building
	DL_FOREACH_SAFE2(ROOM_VEHICLES(inter_room), veh_iter, next_veh, next_in_room) {
		if (!VEH_OWNER(veh_iter) && !VEH_CONTAINS(veh_iter)) {
			extract_vehicle(veh_iter);
		}
	}
	
	if (!IS_SET(GET_BLD_FLAGS(proto), BLD_OPEN)) {
		// closed ruins
		to_room = SHIFT_DIR(inter_room, rev_dir[dir]);
	}
	construct_building(inter_room, interaction->vnum);
	COMPLEX_DATA(inter_room)->entrance = dir;
	
	// custom naming if #n is present (before complete_building)
	if (strstr(GET_BLD_TITLE(proto), "#n")) {
		set_room_custom_name(inter_room, NULL);
		temp = str_replace("#n", old_bld ? GET_BLD_NAME(old_bld) : "a Building", GET_BLD_TITLE(proto));
		CAP(temp);
		set_room_custom_name(inter_room, temp);
		free(temp);
	}
	
	complete_building(inter_room);
	
	if (paint) {
		set_room_extra_data(inter_room, ROOM_EXTRA_PAINT_COLOR, paint);
	}
	
	if (ROOM_IS_CLOSED(inter_room)) {
		create_exit(inter_room, to_room, rev_dir[dir], FALSE);
	}

	// reattach built-with (if any) and reduce it to 5-20%
	if (save) {
		save_resources = number(5, 20) / 100.0;
		GET_BUILT_WITH(inter_room) = save;
		LL_FOREACH_SAFE(GET_BUILT_WITH(inter_room), res, next_res) {
			res->amount = ceil(res->amount * save_resources);
		
			if (res->amount <= 0) {	// delete if empty
				LL_DELETE(GET_BUILT_WITH(inter_room), res);
				free(res);
			}
		}
	}
	
	request_world_save(GET_ROOM_VNUM(inter_room), WSAVE_ROOM);
	return TRUE;
}


/**
* Interaction function for building-ruins-to-vehicle. This loads a new vehicle,
* ignoring interaction quantity, and transfers built-with resources and
* contents.
*
* INTERACTION_FUNC provides: ch, interaction, inter_room, inter_mob, inter_item, inter_veh
*/
INTERACTION_FUNC(ruin_building_to_vehicle_interaction) {
	struct resource_data *res, *next_res, *save = NULL;
	vehicle_data *ruin, *proto, *veh_iter, *next_veh;
	obj_data *obj_iter, *next_obj;
	char_data *ch_iter, *next_ch;
	double save_resources;
	bld_data *old_bld;
	room_data *inside;
	char *to_free;
	int paint;
	
	if (!inter_room || !(proto = vehicle_proto(interaction->vnum)) || GET_ROOM_VNUM(inter_room) >= MAP_SIZE) {
		return FALSE;	// safety: only works on the map
	}
	
	paint = ROOM_PAINT_COLOR(inter_room);
	old_bld = GET_BUILDING(inter_room);
	ruin = read_vehicle(interaction->vnum, TRUE);
	vehicle_to_room(ruin, inter_room);
	scale_vehicle_to_level(ruin, 1);	// minimum available level
	
	// do not transfer ownership -- ruins never default to 'claimed'
	
	// move contents
	if ((inside = get_vehicle_interior(ruin))) {
		// move applicable vehicles
		if (VEH_FLAGGED(ruin, VEH_CARRY_VEHICLES)) {
			DL_FOREACH_SAFE2(ROOM_VEHICLES(inter_room), veh_iter, next_veh, next_in_room) {
				if (veh_iter != ruin && !VEH_FLAGGED(veh_iter, VEH_NO_LOAD_ONTO_VEHICLE)) {
					vehicle_from_room(veh_iter);
					vehicle_to_room(veh_iter, inside);
				}
			}
		}
		// move all mobs/players
		DL_FOREACH_SAFE2(ROOM_PEOPLE(inter_room), ch_iter, next_ch, next_in_room) {
			char_from_room(ch_iter);
			char_to_room(ch_iter, inside);
		}
		// and objs
		DL_FOREACH_SAFE2(ROOM_CONTENTS(inter_room), obj_iter, next_obj, next_content) {
			obj_from_room(obj_iter);
			obj_to_room(obj_iter, inside);
		}
		
		if (ROOM_PEOPLE(inside)) {
			act("The building around you crumbles to ruin!", FALSE, ROOM_PEOPLE(inside), NULL, NULL, TO_CHAR | TO_ROOM);
		}
	}
	
	// move resources...
	if (GET_BUILT_WITH(inter_room)) {
		save = GET_BUILT_WITH(inter_room);
		GET_BUILT_WITH(inter_room) = NULL;
	}
	else if (IS_DISMANTLING(inter_room)) {
		save = GET_BUILDING_RESOURCES(inter_room);
		GET_BUILDING_RESOURCES(inter_room) = NULL;
	}
	
	// abandon first -- this will take care of accessory rooms, too
	if (!old_bld || !BLD_FLAGGED(old_bld, BLD_NO_ABANDON_WHEN_RUINED)) {
		abandon_room(inter_room);
	}
	disassociate_building(inter_room);
	
	if (ROOM_PEOPLE(inter_room)) {	// messaging to anyone left
		act("The building around you crumbles to ruin!", FALSE, ROOM_PEOPLE(inter_room), NULL, NULL, TO_CHAR | TO_ROOM);
	}
	
	// remove any unclaimed/empty vehicles (like furniture) -- those crumble with the building
	DL_FOREACH_SAFE2(ROOM_VEHICLES(inter_room), veh_iter, next_veh, next_in_room) {
		if (veh_iter != ruin && !VEH_OWNER(veh_iter) && !VEH_CONTAINS(veh_iter)) {
			extract_vehicle(veh_iter);
		}
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
	
	// custom naming if #n is present: NOTE: this doesn't use set_vehicle_keywords etc because of special handling
	if (strstr(VEH_KEYWORDS(ruin), "#n")) {
		to_free = (!proto || VEH_KEYWORDS(ruin) != VEH_KEYWORDS(proto)) ? VEH_KEYWORDS(ruin) : NULL;
		VEH_KEYWORDS(ruin) = str_replace("#n", old_bld ? GET_BLD_NAME(old_bld) : "a building", VEH_KEYWORDS(ruin));
		if (to_free) {
			free(to_free);
		}
	}
	if (strstr(VEH_SHORT_DESC(ruin), "#n")) {
		to_free = (!proto || VEH_SHORT_DESC(ruin) != VEH_SHORT_DESC(proto)) ? VEH_SHORT_DESC(ruin) : NULL;
		VEH_SHORT_DESC(ruin) = str_replace("#n", old_bld ? GET_BLD_NAME(old_bld) : "a building", VEH_SHORT_DESC(ruin));
		if (to_free) {
			free(to_free);
		}
	}
	if (strstr(VEH_LONG_DESC(ruin), "#n")) {
		to_free = (!proto || VEH_LONG_DESC(ruin) != VEH_LONG_DESC(proto)) ? VEH_LONG_DESC(ruin) : NULL;
		VEH_LONG_DESC(ruin) = str_replace("#n", old_bld ? GET_BLD_NAME(old_bld) : "a building", VEH_LONG_DESC(ruin));
		CAP(VEH_LONG_DESC(ruin));
		if (to_free) {
			free(to_free);
		}
	}
	
	if (paint) {
		set_vehicle_extra_data(ruin, ROOM_EXTRA_PAINT_COLOR, paint);
	}
	
	request_vehicle_save_in_world(ruin);
	load_vtrigger(ruin);
	request_world_save(GET_ROOM_VNUM(inter_room), WSAVE_ROOM);
	return TRUE;
}


/**
* Replaces a building with ruins.
*
* @param room_data *room The location of the building.
*/
void ruin_one_building(room_data *room) {
	bld_data *bld = GET_BUILDING(room);
	vehicle_data *veh, *next_veh;
	
	if (bld && run_interactions(NULL, GET_BLD_INTERACTIONS(bld), INTERACT_RUINS_TO_VEH, room, NULL, NULL, NULL, ruin_building_to_vehicle_interaction)) {
		// succesfully ruined to a vehicle
	}
	else if (bld && run_interactions(NULL, GET_BLD_INTERACTIONS(bld), INTERACT_RUINS_TO_BLD, room, NULL, NULL, NULL, ruin_building_to_building_interaction)) {
		// succesfully ruined to a vehicle
	}
	else {	// failed to run a ruins interaction	
		// abandon first -- this will take care of accessory rooms, too
		if (!bld || !BLD_FLAGGED(bld, BLD_NO_ABANDON_WHEN_RUINED)) {
			abandon_room(room);
		}
		disassociate_building(room);
	
		if (ROOM_PEOPLE(room)) {
			act("The building around you crumbles to ruin!", FALSE, ROOM_PEOPLE(room), NULL, NULL, TO_CHAR | TO_ROOM);
		}
	
		// remove any unclaimed/empty vehicles (like furniture) -- those crumble with the building
		DL_FOREACH_SAFE2(ROOM_VEHICLES(room), veh, next_veh, next_in_room) {
			if (!VEH_OWNER(veh) && !VEH_CONTAINS(veh)) {
				extract_vehicle(veh);
			}
		}
	}
}


/**
* Schedules the event handler for unloading a map event.
*
* @param struct map_data *map The map tile to schedule it on.
* @param bool offset If TRUE, offsets the time to reduce how many are scheduled at once.
*/
void schedule_check_unload(room_data *room, bool offset) {
	struct room_event_data *data;
	double mins;
	
	if (!room) {	// somehow
		log("SYSERR: schedule_check_unload called with null room");
		return;
	}
	
	if (!ROOM_UNLOAD_EVENT(room)) {
		CREATE(data, struct room_event_data, 1);
		data->room = room;
		
		mins = 5;
		if (offset) {
			mins += number(-300, 300) / 100.0;
		}
		ROOM_UNLOAD_EVENT(room) = dg_event_create(check_unload_room, (void*)data, (mins * 60) RL_SEC);
	}
}


/**
* Schedules the crop growth.
*
* @param struct map_data *map The map location to setup growth for.
*/
void schedule_crop_growth(struct map_data *map) {
	struct map_event_data *data;
	struct dg_event *ev;
	long when;
	
	// cancel any existing event
	cancel_stored_event(&map->shared->events, SEV_GROW_CROP);
	
	when = get_extra_data(map->shared->extra_data, ROOM_EXTRA_SEED_TIME);
	
	if (SECT_FLAGGED(map->sector_type, SECTF_HAS_CROP_DATA) && when > 0) {
		// create the event
		CREATE(data, struct map_event_data, 1);
		data->map = map;
		
		ev = dg_event_create(grow_crop_event, data, (when - time(0)) * PASSES_PER_SEC);
		add_stored_event(&map->shared->events, SEV_GROW_CROP, ev);
	}
}


/**
* Assigns level data to islands based on empires with cities there
* (at startup).
*/
void setup_island_levels(void) {
	struct island_info *isle, *next_isle;
	struct empire_city_data *city;
	empire_data *emp, *next_emp;
	
	// initialize
	HASH_ITER(hh, island_table, isle, next_isle) {
		if (IS_SET(isle->flags, ISLE_NEWBIE)) {
			isle->min_level = 1;	// ensure newbies get adventures
			isle->max_level = 1;
		}
		else {
			isle->min_level = isle->max_level = 0;
		}
	}
	
	// mark empire locations
	HASH_ITER(hh, empire_table, emp, next_emp) {
		if (!EMPIRE_MEMBERS(emp) || !EMPIRE_CITY_LIST(emp) || EMPIRE_MAX_LEVEL(emp) == 0 || EMPIRE_IS_TIMED_OUT(emp)) {
			continue;	// no locations/levels to mark
		}
		
		LL_FOREACH(EMPIRE_CITY_LIST(emp), city) {
			if (!(isle = GET_ISLAND(city->location))) {
				continue;	// somehow
			}
			
			// assign levels
			if (EMPIRE_MIN_LEVEL(emp) > 0 && (!isle->min_level || EMPIRE_MIN_LEVEL(emp) < isle->min_level)) {
				isle->min_level = EMPIRE_MIN_LEVEL(emp);
			}
			isle->max_level = MAX(EMPIRE_MAX_LEVEL(emp), isle->max_level);
		}
	}
}


// Simple sorter for empire islands
int sort_empire_islands(struct empire_island *a, struct empire_island *b) {
	return a->island - b->island;
}


// Simple sorter for the world_table hash
int sort_world_table_func(room_data *a, room_data *b) {
	return a->vnum - b->vnum;
}


 //////////////////////////////////////////////////////////////////////////////
//// WORLD MAP SYSTEM ////////////////////////////////////////////////////////

/**
* Validates sectors and sets up the land_map linked list. This should be done
* at the end of world startup. Run this AFTER build_world_map().
*/
void build_land_map(void) {
	struct map_data *map, *last = NULL;
	sector_data *ocean = sector_proto(BASIC_OCEAN);
	struct sector_index_type *idx;
	room_data *room;
	int x, y;
	
	if (!ocean) {
		log("SYSERR: Unable to start game without basic ocean sector (%d)", BASIC_OCEAN);
		exit(1);
	}
	
	land_map = NULL;
	
	for (x = 0; x < MAP_WIDTH; ++x) {
		for (y = 0; y < MAP_HEIGHT; ++y) {
			map = &(world_map[x][y]);
			
			// ensure data
			if (!map->sector_type) {
				map->sector_type = ocean;
			}
			if (!map->base_sector) {
				map->base_sector = ocean;
			}
			if (!map->natural_sector) {
				map->natural_sector = ocean;
			}
			
			// update land_map
			map->next = NULL;
			if (map->sector_type != ocean) {
				if (last) {
					last->next = map;
				}
				else {
					land_map = map;
				}
				last = map;
			}
			
			// index sector
			idx = find_sector_index(GET_SECT_VNUM(map->sector_type));
			++idx->sect_count;
			LL_PREPEND2(idx->sect_rooms, map, next_in_sect);
			
			// index base
			if (map->base_sector != map->sector_type) {
				idx = find_sector_index(GET_SECT_VNUM(map->base_sector));
			}
			++idx->base_count;
			LL_PREPEND2(idx->base_rooms, map, next_in_base_sect);
		}
	}
	
	// also index the interior while we're here, so it's completely done
	DL_FOREACH2(interior_room_list, room, next_interior) {
		idx = find_sector_index(GET_SECT_VNUM(SECT(room)));
		++idx->sect_count;
		
		// index base
		if (BASE_SECT(room) != SECT(room)) {
			idx = find_sector_index(GET_SECT_VNUM(BASE_SECT(room)));
		}
		++idx->base_count;
	}
}


/**
* Fills in any gaps in the world_map data from the current live map. This
* should be run after the data is loaded from file. Run this BEFORE running
* build_land_map().
*/
void build_world_map(void) {
	room_data *room, *next_room;
	int x, y;
	
	HASH_ITER(hh, world_table, room, next_room) {
		if (GET_ROOM_VNUM(room) >= MAP_SIZE) {
			continue;
		}
		
		x = FLAT_X_COORD(room);
		y = FLAT_Y_COORD(room);
				
		if (SECT(room)) {
			world_map[x][y].sector_type = SECT(room);
		}
		if (BASE_SECT(room)) {
			world_map[x][y].base_sector = BASE_SECT(room);
		}
		
		// we only update the natural sector if it doesn't have one
		if (!world_map[x][y].natural_sector) {
			// it's PROBABLY the room's original sect
			world_map[x][y].natural_sector = BASE_SECT(room);
		}
	}
}


/**
* Called at startup to initialize the whole map.
*/
void init_map(void) {
	int x, y;
	
	// initialize ocean_shared_data
	memset((char*)&ocean_shared_data, 0, sizeof(struct shared_room_data));
	ocean_shared_data.island_id = NO_ISLAND;
	
	land_map = NULL;
	for (x = 0; x < MAP_WIDTH; ++x) {
		for (y = 0; y < MAP_HEIGHT; ++y) {
			memset((char*)&world_map[x][y], 0, sizeof(struct map_data));
			world_map[x][y].vnum = (y * MAP_WIDTH) + x;
			world_map[x][y].shared = &ocean_shared_data;
			world_map[x][y].shared->island_id = NO_ISLAND;
		}
	}
}


/**
* Parses any 'other data' for the 'shared room data' of a map tile or room.
*
* Note: This currently only includes 'height' but is expandable to handle other
* 'other data' in the future.
*
* @param struct shared_room_data *shared A pointer to the room's shared data.
* @param char *line The line from the file, starting with 'U'.
* @param char *error_part An explanation of where this data was read from, for error reporting.
*/
void parse_other_shared_data(struct shared_room_data *shared, char *line, char *error_part) {
	int int_in[2];
	
	if (!shared || !line || !*line || !error_part) {
		log("SYSERR: Bad shared line '%s' or missing shared pointer in %s", NULLSAFE(line), error_part ? error_part : "UNKNOWN");
		exit(1);
	}
	
	// expected format is: U0 0
	switch (line[1]) {
		case '0': {
			if (sscanf(line, "U0 %d", &int_in[0]) != 1) {
				log("SYSERR: Bad U0 entry '%s' in %s", line, error_part);
				exit(1);
			}
			
			shared->height = int_in[0];
			break;
		}
	}
}


 //////////////////////////////////////////////////////////////////////////////
//// WORLD MAP SAVING ////////////////////////////////////////////////////////

/**
* To be called after saving everything, to prevent anything currently
* requesting a save from re-saving.
*/
void cancel_all_world_save_requests(int only_save_type) {
	struct world_save_request_data *iter, *next_iter;
	HASH_ITER(hh, world_save_requests, iter, next_iter) {
		if (only_save_type != NOBITS) {
			REMOVE_BIT(iter->save_type, only_save_type);
		}
		
		if (!iter->save_type || only_save_type == NOBITS) {
			HASH_DEL(world_save_requests, iter);
			free(iter);
		}
	}
}


/**
* Cancels any open world-save-requests for a given room.
*
* @param room_vnum room The room to cancel requests for.
* @param int only_save_type Optional: Only clear certain flags (pass NOBITS to clear all).
*/
void cancel_world_save_request(room_vnum room, int only_save_type) {
	struct world_save_request_data *item;
	
	// check for existing entry
	HASH_FIND_INT(world_save_requests, &room, item);
	
	if (item) {
		if (only_save_type != NOBITS) {
			REMOVE_BIT(item->save_type, only_save_type);
		}
		
		if (!item->save_type || only_save_type == NOBITS) {
			HASH_DEL(world_save_requests, item);
			free(item);
		}
	}
}


/**
* This will either guarantee that the binary map file is open, or will SYSERR
* and exit.
*
* After calling this, you can use the global 'binary_map_fl' variable.
*/
void ensure_binary_map_file_is_open(void) {
	if (binary_map_fl || (binary_map_fl = fopen(BINARY_MAP_FILE, "r+b"))) {
		return;	// ok: was open or is now open
	}
	
	if (errno != ENOENT) {
		// failed to open for some reason OTHER than does-not-exist
		perror("SYSERR: fatal error opening binary map file");
		exit(1);
	}
	
	// try to create it
	touch(BINARY_MAP_FILE);
	if (!(binary_map_fl = fopen(BINARY_MAP_FILE, "r+b"))) {
		perror("SYSERR: fatal error opening binary map file");
		exit(1);
	}
	
	// successfully created it -- trigger this for later:
	save_world_after_startup = TRUE;
}


/**
* Copies the savable part of a map entry to storable for.
*
* @param struct map_data *map The map tile to save.
* @param map_file_data *store A pointer to the data to write to.
*/
void map_to_store(struct map_data *map, map_file_data *store) {
	memset((char *) store, 0, sizeof(map_file_data));
	store->island_id = map->shared->island_id;
	
	store->sector_type = map->sector_type ? GET_SECT_VNUM(map->sector_type) : NOTHING;
	store->base_sector = map->base_sector ? GET_SECT_VNUM(map->base_sector) : NOTHING;
	store->natural_sector = map->natural_sector ? GET_SECT_VNUM(map->natural_sector) : NOTHING;
	store->crop_type = map->crop_type ? GET_CROP_VNUM(map->crop_type) : NOTHING;
	
	store->affects = map->shared->affects;
	store->base_affects = map->shared->base_affects;
	store->height = map->shared->height;
	
	// for the evolver
	store->misc = 0;
	if (map->room && ROOM_OWNER(map->room)) {
		store->misc |= EVOLVER_OWNED;
	}
	store->sector_time = get_extra_data(map->shared->extra_data, ROOM_EXTRA_SECTOR_TIME);
}


/**
* Performs various world saves on-request. Schedule these with:
*   request_world_save()
*/
void perform_requested_world_saves(void) {
	struct world_save_request_data *iter, *next_iter;
	struct map_data *map;
	bool any_map = FALSE;
	
	if (block_all_saves_due_to_shutdown) {
		return;
	}
	
	HASH_ITER(hh, world_save_requests, iter, next_iter) {
		// WSAVE_x: what to do for different save bits
		if (IS_SET(iter->save_type, WSAVE_OBJS_AND_VEHS)) {
			// do this before WSAVE_ROOM because it may also trigger a WSAVE_ROOM
			objpack_save_room(real_real_room(iter->vnum));
			REMOVE_BIT(iter->save_type, WSAVE_OBJS_AND_VEHS);
		}
		if (IS_SET(iter->save_type, WSAVE_MAP)) {
			// do this before WSAVE_ROOM because it may also trigger a WSAVE_ROOM
			if (iter->vnum >= 0 && iter->vnum < MAP_SIZE) {
				map = &world_map[MAP_X_COORD(iter->vnum)][MAP_Y_COORD(iter->vnum)];
				write_one_tile_to_binary_map_file(map);
				if (HAS_SHARED_DATA_TO_SAVE(map)) {
					// ensure this is saved
					SET_BIT(iter->save_type, WSAVE_ROOM);
				}
				any_map = TRUE;
			}
			REMOVE_BIT(iter->save_type, WSAVE_MAP);
		}
		if (IS_SET(iter->save_type, WSAVE_ROOM)) {
			write_map_and_room_to_file(iter->vnum, FALSE);
			REMOVE_BIT(iter->save_type, WSAVE_ROOM);
		}
		
		// remove ONLY if no bits are left (in case any function queued more saves)
		if (iter->save_type == NOBITS) {
			HASH_DEL(world_save_requests, iter);
			free(iter);
		}
	}
	
	// check for map saves
	if (any_map && binary_map_fl) {
		fflush(binary_map_fl);
	}
	
	// check for instance saves
	if (need_instance_save) {
		save_instances();
		need_instance_save = FALSE;
	}
}


/**
* Schedules a save for any part of a room.
*
* @param room_vnum vnum The room/map vnum to save.
* @param int save_type Any WSAVE_ bits.
*/
void request_world_save(room_vnum vnum, int save_type) {
	struct world_save_request_data *item;
	
	// disallow save requests during parts of startup
	if (block_world_save_requests) {
		return;
	}
	
	// check for existing entry
	HASH_FIND_INT(world_save_requests, &vnum, item);
	
	// create if needed
	if (!item) {
		CREATE(item, struct world_save_request_data, 1);
		item->vnum = vnum;
		HASH_ADD_INT(world_save_requests, vnum, item);
	}
	
	// add  bits
	item->save_type |= save_type;
}


/**
* Calls a world save request based on whatever the script is attached to, using
* the "go" and "type" vars in the DG Scripts system.
*
* @param void *go The thing the script is attached to.
* @param int type The _TRIGGER type, indicating what type the 'go' is.
*/
void request_world_save_by_script(void *go, int type) {
	// X_TRIGGER:
	switch (type) {
		case MOB_TRIGGER: {
			request_char_save_in_world((char_data*)go);
			break;
		}
		case OBJ_TRIGGER: {
			request_obj_save_in_world((obj_data*)go);
			break;
		}
		case WLD_TRIGGER:
		case RMT_TRIGGER:
		case BLD_TRIGGER:
		case ADV_TRIGGER: {
			request_world_save(GET_ROOM_VNUM((room_data*)go), WSAVE_ROOM);
			break;
		}
		case VEH_TRIGGER: {
			request_vehicle_save_in_world((vehicle_data*)go);
			break;
		}
		// default: no work
	}
}


/**
* Updates the entry in the world index for a room vnu.
*
* @param room_vnum vnum The room to update.
* @param char value The value to set it to (0 = no room, 1 = room present; other values can be added).
*/
void update_world_index(room_vnum vnum, char value) {
	int iter;
	
	// check size of index
	if (vnum > top_of_world_index) {
		if (world_index_data) {
			// reallocate and clear the new space
			RECREATE(world_index_data, char, vnum + 1);
			for (iter = top_of_world_index+1; iter <= vnum; ++iter) {
				world_index_data[iter] = 0;
			}
		}
		else {
			CREATE(world_index_data, char, vnum + 1);
		}
		top_of_world_index = vnum;
	}
	
	// make the update if possible
	if (vnum >= 0 && world_index_data[vnum] != value) {
		world_index_data[vnum] = value;
		
		// and trigger a file save
		add_vnum_hash(&binary_world_index_updates, vnum, 1);
	}
}


/**
* Writes the .wld file for every room. CAUTION: You should only do this at
* shutdown, when it is done as a courtesy. Ideally, all these rooms are saved
* individually, within 1 second of being changed, by request_world_save().
*/
void write_all_wld_files(void) {
	room_data *room, *next_room;
	int x, y;
	
	HASH_ITER(hh, world_table, room, next_room) {
		write_map_and_room_to_file(GET_ROOM_VNUM(room), TRUE);
	}
	
	for (x = 0; x < MAP_WIDTH; ++x) {
		for (y = 0; y < MAP_HEIGHT; ++y) {
			if (!world_map[x][y].room && HAS_SHARED_DATA_TO_SAVE(&world_map[x][y])) {
				write_map_and_room_to_file((y * MAP_WIDTH + x), TRUE);
			}
			else if (!world_map[x][y].room) {
				// if there's no room and no shared data, ensure it has a zero here
				update_world_index((y * MAP_WIDTH + x), 0);
			}
		}
	}
	
	// no need to save any more room updates
	cancel_all_world_save_requests(WSAVE_ROOM | WSAVE_OBJS_AND_VEHS);
}


/**
* Writes changes to the binary world index without rewriting the entire file.
* This runs frequently. Add to its queue with:
* update_world_index(vnum, value)
*/
void write_binary_world_index_updates(void) {
	struct vnum_hash *iter, *next_iter;
	int max_vnum = 0, top_vnum = 0;
	char tmp, zero = 0;
	FILE *fl;
	
	if (!binary_world_index_updates || block_all_saves_due_to_shutdown) {
		return;	// shortcut/no work
	}
	
	if (!(fl = fopen(BINARY_WORLD_INDEX, "r+b"))) {
		log("write_binary_world_index_updates: %s doesn't exist, writing fresh one", BINARY_WORLD_INDEX);
		write_whole_binary_world_index();
		return;
	}
	
	// read header info (top vnum)
	fread(&top_vnum, sizeof(int), 1, fl);
	
	// determine maximum vnum needed
	HASH_ITER(hh, binary_world_index_updates, iter, next_iter) {
		if (iter->vnum > max_vnum) {
			max_vnum = iter->vnum;
		}
	}
	
	// ensure space for our vnum (or add space)
	if (max_vnum > top_vnum) {
		// add space at the end
		fseek(fl, 0L, SEEK_END);
		while (top_vnum++ < max_vnum) {
			fwrite(&zero, sizeof(char), 1, fl);
		}
		
		// rewrite header
		fseek(fl, 0L, SEEK_SET);
		fwrite(&top_vnum, sizeof(int), 1, fl);
	}
	
	// now do all the updates
	HASH_ITER(hh, binary_world_index_updates, iter, next_iter) {
		if (iter->vnum >= 0 && iter->vnum <= top_of_world_index) {
			// copy it to a char
			tmp = world_index_data[iter->vnum];
		
			// seek to the correct spot and write
			fseek(fl, sizeof(int) + (iter->vnum * sizeof(char)), SEEK_SET);
			fwrite(&tmp, sizeof(char), 1, fl);
		}
		
		// and clean up
		HASH_DEL(binary_world_index_updates, iter);
		free(iter);
	}
	
	fclose(fl);
}


/**
* Writes all the room/map data for a given map tile or interior room. Pass
* either a map tile or a room pointer (or pass both for the same room, but it
* can detect whichever one you don't pass). If there's nothing to save, it
* returns FALSE and deletes the wld file if present.
*
* @param room_data *room Optional: The full room to write (may be NULL).
* @param struct map_data *map Optional: The map tile to save (may be NULL).
* @param bool force_obj_pack If TRUE, will always write the obj/veh pack, if one exists, rather than skipping it if already written.
* @return bool TRUE if it wrote a file, FALSE if it did not.
*/
bool write_map_and_room_to_file(room_vnum vnum, bool force_obj_pack) {
	char temp[MAX_STRING_LENGTH], fname[256];
	struct affected_type *aff;
	struct shared_room_data *shared;
	struct room_extra_data *red, *next_red;
	struct depletion_data *dep;
	struct track_data *track, *next_track;
	struct room_direction_data *ex;
	struct cooldown_data *cool;
	struct resource_data *res;
	struct trig_var_data *tvd;
	struct affected_type *af;
	struct map_data *map;
	room_data *room;
	trig_data *trig;
	char_data *mob, *m_proto;
	time_t now = time(0);
	FILE *fl;
	
	if (vnum == NOTHING || block_all_saves_due_to_shutdown) {
		return FALSE;	// very early exit if we couldn't pull a vnum from the args
	}
	
	// determine what we've got:
	map = (vnum < MAP_SIZE) ? &world_map[MAP_X_COORD(vnum)][MAP_Y_COORD(vnum)] : NULL;
	room = (map && map->room) ? map->room : real_real_room(vnum);
	shared = (map ? map->shared : (room ? SHARED_DATA(room) : NULL));
	
	// cancel map/room if there's nothing to save for them
	if (room && CAN_UNLOAD_MAP_ROOM(room)) {	// but actually skip the room if it's unloadable
		room = NULL;
	}
	if (map && !HAS_SHARED_DATA_TO_SAVE(map)) {	// also skip map if it doesn't have anything that belongs here
		map = NULL;
	}
	
	// do we (still) have anything to save
	if (!room) {
		// delete pack file if no room
		get_world_filename(fname, vnum, SUF_PACK);
		if (access(fname, F_OK) == 0) {
			unlink(fname);
		}
	}
	if (!map && !room) {
		// yank from index
		update_world_index(vnum, 0);
		
		// delete wld file if we're not saving at all
		if (!block_all_saves_due_to_shutdown) {
			get_world_filename(fname, vnum, WLD_SUFFIX);
			if (access(fname, F_OK) == 0) {
				unlink(fname);
			}
		}
		
		// nothing to save
		return FALSE;
	}
	// safety check
	if (map && room && map->vnum != GET_ROOM_VNUM(room)) {
		log("SYSERR: write_map_and_room_to_file called with map and room that have different vnums");
		return FALSE;
	}
	
	// OK: open the file and start saving
	get_world_filename(fname, vnum, WLD_SUFFIX);
	if (!(fl = fopen(fname, "w"))) {
		log("SYSERR: write_map_and_room_to_file: unable to open world file %s", fname);
		return FALSE;
	}
	
	// ensure we are in the index
	update_world_index(vnum, 1);
	
	// print tagged file info
	fprintf(fl, "#%d %c\n", vnum, room ? 'R' : 'M');
	
	if (room) {
		fprintf(fl, "Sector: %d %d\n", GET_SECT_VNUM(SECT(room)), GET_SECT_VNUM(BASE_SECT(room)));
		
		LL_FOREACH(ROOM_AFFECTS(room), af) {
			fprintf(fl, "Affect: %d %d %ld %d %d %llu\n", af->type, af->cast_by, af->expire_time == UNLIMITED ? UNLIMITED : (af->expire_time - time(0)), af->modifier, af->location, af->bitvector);
		}
		if (HOME_ROOM(room) != room) {
			fprintf(fl, "Home-room: %d\n", GET_ROOM_VNUM(HOME_ROOM(room)));
		}
		if (ROOM_OWNER(room)) {
			fprintf(fl, "Owner: %d\n", EMPIRE_VNUM(ROOM_OWNER(room)));
		}
		
		if (COMPLEX_DATA(room)) {
			if (GET_BUILDING(room)) {
				fprintf(fl, "Building: %d\n", GET_BLD_VNUM(GET_BUILDING(room)));
			}
			if (GET_ROOM_TEMPLATE(room)) {
				fprintf(fl, "Template: %d\n", GET_RMT_VNUM(GET_ROOM_TEMPLATE(room)));
			}
			if (COMPLEX_DATA(room)->entrance != NO_DIR) {
				fprintf(fl, "Entrance: %d\n", COMPLEX_DATA(room)->entrance);
			}
			if (COMPLEX_DATA(room)->burn_down_time) {
				fprintf(fl, "Burn-down-time: %ld\n", COMPLEX_DATA(room)->burn_down_time);
			}
			if (COMPLEX_DATA(room)->damage) {
				fprintf(fl, "Damage: %.2f\n", COMPLEX_DATA(room)->damage);
			}
			if (COMPLEX_DATA(room)->private_owner != NOBODY) {
				fprintf(fl, "Private: %d\n", COMPLEX_DATA(room)->private_owner);
			}
			
			LL_FOREACH(BUILDING_RESOURCES(room), res) {
				fprintf(fl, "Resource: %d %d %d %d\n", res->type, res->vnum, res->misc, res->amount);
			}
			LL_FOREACH(GET_BUILT_WITH(room), res) {
				fprintf(fl, "Built-with: %d %d %d %d\n", res->type, res->vnum, res->misc, res->amount);
			}
			LL_FOREACH(COMPLEX_DATA(room)->exits, ex) {
				if (ex->keyword && *ex->keyword) {
					fprintf(fl, "Exit: %d %d %d 1\n%s~\n", ex->dir, ex->exit_info, ex->to_room, ex->keyword);
				}
				else {
					fprintf(fl, "Exit: %d %d %d 0\n", ex->dir, ex->exit_info, ex->to_room);
				}
			}
		}
	}
	
	// shared data
	if (shared->icon) {
		fprintf(fl, "Icon:\n%s~\n", shared->icon);
	}
	if (shared->description) {
		strcpy(temp, shared->description);
		strip_crlf(temp);
		fprintf(fl, "Desc:\n%s~\n", temp);
	}
	if (shared->name) {
		fprintf(fl, "Name:\n%s~\n", shared->name);
	}
	LL_FOREACH(shared->depletion, dep) {
		fprintf(fl, "Depletion: %d %d\n", dep->type, dep->count);
	}
	HASH_ITER(hh, shared->tracks, track, next_track) {
		if (now - track->timestamp > SECS_PER_REAL_HOUR) {
			// delete expired tracks while we're here
			HASH_DEL(shared->tracks, track);
			free(track);
		}
		else {
			fprintf(fl, "Track: %d %ld %d %d\n", track->id, track->timestamp, track->dir, track->to_room);
		}
	}
	HASH_ITER(hh, shared->extra_data, red, next_red) {
		fprintf(fl, "Extra: %d %d\n", red->type, red->value);
	}
	
	// data only stored if it's not in the binary map file
	if (room && !map) {
		if (shared->height) {
			fprintf(fl, "Height: %d\n", shared->height);
		}
		if (shared->island_id) {
			fprintf(fl, "Island: %d\n", shared->island_id);
		}
		if (shared->affects || shared->base_affects) {
			fprintf(fl, "Aff-flags: %llu %llu\n", shared->base_affects, shared->affects);
		}
		if (ROOM_CROP(room)) {
			fprintf(fl, "Crop: %d\n", GET_CROP_VNUM(ROOM_CROP(room)));
		}
	}
	
	// 'load' commands ("resets"), last
	// each reset (except 'S') has 4 digits. The first 3 are variable; the last 1 indicates 2 more lines of strings.
	if (room) {
		// must save obj pack instruction BEFORE mob instruction
		if ((!force_obj_pack && IS_SET(room->save_info, SAVE_INFO_PACK_SAVED)) || objpack_save_room(room)) {
			// the "1" in the arg1 slot indicates it will use the new objpack location, see objpack_load_room()
			fprintf(fl, "Load: O 1 0 0 0\n");
		}
	
		if (SCRIPT(room)) {
			// triggers
			LL_FOREACH(TRIGGERS(SCRIPT(room)), trig) {
				fprintf(fl, "Load: T %d %d 0 0\n", WLD_TRIGGER, GET_TRIG_VNUM(trig));
			}
			// vars
			LL_FOREACH(SCRIPT(room)->global_vars, tvd) {
				if (*tvd->name == '-' || !*tvd->value) { // don't save if it begins with - or is empty
					continue;
				}
			
				// trig-type 0 context name value
				fprintf(fl, "Load: V %d %d %d 1\n%s~\n%s~\n", WLD_TRIGGER, 0, (int)tvd->context, tvd->name, tvd->value);
			}
		}
		// people
		DL_FOREACH2(ROOM_PEOPLE(room), mob, next_in_room) {
			if (mob && MOB_SAVES_TO_ROOM(mob)) {
				// basic mob data
				fprintf(fl, "Load: M %d %llu %d 0\n", GET_MOB_VNUM(mob), MOB_FLAGS(mob), GET_ROPE_VNUM(mob));
				
				// extra mob data?
				if (MOB_FLAGGED(mob, MOB_SPAWNED)) {
					fprintf(fl, "Load: N 0 %ld 0 0\n", MOB_SPAWN_TIME(mob));
				}
				
				// save affects but not dots
				LL_FOREACH(mob->affected, aff) {
					fprintf(fl, "Load: A %d %d %ld %d %d %lld\n", aff->type, aff->cast_by, aff->expire_time == UNLIMITED ? UNLIMITED : (aff->expire_time - time(0)), aff->modifier, aff->location, aff->bitvector);
				}
				
				// instance id if any
				if (MOB_INSTANCE_ID(mob) != NOTHING) {
					fprintf(fl, "Load: I %d 0 0 0\n", MOB_INSTANCE_ID(mob));
				}
				
				// dynamic naming info
				if (MOB_DYNAMIC_SEX(mob) != NOTHING || MOB_DYNAMIC_NAME(mob) != NOTHING || GET_LOYALTY(mob)) {
					fprintf(fl, "Load: Y %d %d %d 0\n", MOB_DYNAMIC_SEX(mob), MOB_DYNAMIC_NAME(mob), GET_LOYALTY(mob) ? EMPIRE_VNUM(GET_LOYALTY(mob)) : NOTHING);
				}
				
				// cooldowns
				for (cool = mob->cooldowns; cool; cool = cool->next) {
					fprintf(fl, "Load: C %d %d 0 0\n", cool->type, (int)(cool->expire_time - time(0)));
				}
				
				// custom strings
				if (mob->customized) {
					m_proto = mob_proto(GET_MOB_VNUM(mob));
					if (!m_proto || GET_REAL_SEX(mob) != GET_REAL_SEX(m_proto)) {
						fprintf(fl, "Load: S sex %d\n", GET_REAL_SEX(mob));
					}
					if (!m_proto || GET_PC_NAME(mob) != GET_PC_NAME(m_proto)) {
						fprintf(fl, "Load: S keywords %s\n", NULLSAFE(GET_PC_NAME(mob)));
					}
					if (!m_proto || GET_SHORT_DESC(mob) != GET_SHORT_DESC(m_proto)) {
						fprintf(fl, "Load: S short %s\n", NULLSAFE(GET_SHORT_DESC(mob)));
					}
					if (!m_proto || GET_LONG_DESC(mob) != GET_LONG_DESC(m_proto)) {
						strcpy(temp, NULLSAFE(GET_LONG_DESC(mob)));
						strip_crlf(temp);
						fprintf(fl, "Load: S long %s\n", temp);
					}
					if (!m_proto || GET_LOOK_DESC(mob) != GET_LOOK_DESC(m_proto)) {
						strcpy(temp, NULLSAFE(GET_LOOK_DESC(mob)));
						strip_crlf(temp);
						fprintf(fl, "Load: S look\n%s~\n", temp);
					}
				}
				
				// morph?
				if (GET_MORPH(mob)) {
					fprintf(fl, "Load: S morph %d\n", MORPH_VNUM(GET_MORPH(mob)));
				}
			
				if (SCRIPT(mob)) {
					// triggers on the mob
					LL_FOREACH(TRIGGERS(SCRIPT(mob)), trig) {
						fprintf(fl, "Load: T %d %d 0 0\n", MOB_TRIGGER, GET_TRIG_VNUM(trig));
					}
				
					// variables on the mob
					LL_FOREACH(SCRIPT(mob)->global_vars, tvd) {
						if (*tvd->name == '-' || !*tvd->value) { // don't save if it begins with - or is empty
							continue;
						}
					
						// trig-type 0 context name value
						fprintf(fl, "Load: V %d %d %d 1\n%s~\n%s~\n\n", MOB_TRIGGER, 0, (int)tvd->context, tvd->name, tvd->value);
					}
				}
			}
		}
	}
	
	fprintf(fl, "End World File\n");
	fclose(fl);
	
	return TRUE;
}


/**
* Updates the binary map file with the data from a single map tile. Note that
* this is only flat data; anything in lists or dynamic strings must be saved
* with write_map_and_room_to_file() instead.
*
* Call this through: request_world_save(vnum, WSAVE_MAP)
*
* @param struct map_data *map The map tile to save.
*/
void write_one_tile_to_binary_map_file(struct map_data *map) {
	map_file_data store;
	
	if (!map) {
		return;	// no work
	}
	
	ensure_binary_map_file_is_open();
	
	// seek to the correct spot
	fseek(binary_map_fl, sizeof(struct map_file_header) + (sizeof(map_file_data) * map->vnum), SEEK_SET);
	
	map_to_store(map, &store);
	fwrite(&store, sizeof(map_file_data), 1, binary_map_fl);
	
	// leave the map file open; it will be flushed later
}


/**
* Saves a full fresh binary map file.
*/
void write_fresh_binary_map_file(void) {
	struct map_file_header header;
	map_file_data store;
	FILE *fl;
	int x, y;
	
	if (binary_map_fl) {
		// it will re-open itself when it needs to later
		fclose(binary_map_fl);
		binary_map_fl = NULL;
	}
	
	if (!(fl = fopen(BINARY_MAP_FILE TEMP_SUFFIX, "w+b"))) {
		log("SYSERR: Unable to open binary map file '%s' for writing.", BINARY_MAP_FILE TEMP_SUFFIX);
		exit(1);
	}
	
	// binary map file is now open at the start: write header...
	header.version = CURRENT_BINARY_MAP_VERSION;
	header.width = MAP_WIDTH;
	header.height = MAP_HEIGHT;
	fwrite(&header, sizeof(struct map_file_header), 1, fl);
	
	// write all entries sequentially
	for (y = 0; y < MAP_HEIGHT; ++y) {
		for (x = 0; x < MAP_WIDTH; ++x) {
			map_to_store(&world_map[x][y], &store);
			fwrite(&store, sizeof(map_file_data), 1, fl);
		}
	}
	
	fclose(fl);
	rename(BINARY_MAP_FILE TEMP_SUFFIX, BINARY_MAP_FILE);
	
	// no need to save any map updates
	cancel_all_world_save_requests(WSAVE_MAP);
}


/**
* Writes the full binary index file for the world. This should be called once
* at the end of startup to ensure a full binary file exists, and may also be
* called before shutting down/rebooting. After that, call
* update_world_index() to make changes.
*/
void write_whole_binary_world_index(void) {
	FILE *fl;
	
	if (block_all_saves_due_to_shutdown) {
		return;
	}
	
	// open temp file for writing
	if (!(fl = fopen(BINARY_WORLD_INDEX TEMP_SUFFIX, "w+b"))) {
		log("SYSERR: Unable to open %s for writing", BINARY_WORLD_INDEX TEMP_SUFFIX);
		return;
	}
	
	HASH_SORT(world_table, sort_world_table_func);
	
	// header info: top vnum
	fwrite(&top_of_world_index, sizeof(int), 1, fl);
	
	// write the whole index as a series of chars
	fwrite(world_index_data, sizeof(char), top_of_world_index+1, fl);
	
	// close and rename
	fclose(fl);
	rename(BINARY_WORLD_INDEX TEMP_SUFFIX, BINARY_WORLD_INDEX);
}


 //////////////////////////////////////////////////////////////////////////////
//// WORLD MAP LOADING ///////////////////////////////////////////////////////

// for more readable if/else chain
#define LOG_BAD_WLD_TAG_WARNINGS  TRUE	// triggers syslogs for invalid tags
#define BAD_WLD_TAG_WARNING  else if (LOG_BAD_WLD_TAG_WARNINGS) { log("SYSERR: Bad tag in wld file for room %d: %s%s", vnum, line, (room ? "" : " (no room)")); }


/**
* First part of loading the world: Loads the basic map data.
*
* This also includes some backwards-compatibility for systems older than b5.116
* when it used a larger text file.
*/
void load_binary_map_file(void) {
	int x, y, height, width;
	struct map_file_header header;
	bool eof = FALSE;
	
	if (binary_map_fl) {
		// this shouldln't be open, but just in case...
		fclose(binary_map_fl);
		binary_map_fl = NULL;
	}
	
	// don't allow loading to trigger saves
	block_world_save_requests = TRUE;
	
	// run this ONLY if this is the first step in booting the map
	init_map();
	
	if (!(binary_map_fl = fopen(BINARY_MAP_FILE, "r+b"))) {
		if (errno != ENOENT) {
			perror("SYSERR: fatal error opening binary map file");
			exit(1);
		}
		else {	// no file
			//log("No binary map file; creating one");
			log("No binary map file: attempting to load pre-b5.116 base_map file...");
			
			if (load_pre_b5_116_world_map_from_file()) {
				log("- successfully loaded old base_map file (deleting it now)");
			}
			
			// need a full file now
			write_fresh_binary_map_file();
		}
		ensure_binary_map_file_is_open();
	}
	
	// load header
	fread(&header, sizeof(struct map_file_header), 1, binary_map_fl);
	if (feof(binary_map_fl)) {
		// nothing to load?
		block_world_save_requests = FALSE;
		return;
	}
	
	// get the dimensions that the map was saved with
	width = (header.width ? header.width : MAP_WIDTH);
	height = (header.height ? header.height : MAP_HEIGHT);
	
	// load records: this will load the entire width/height the file was saved
	// with, even if it's larger than the current MAP_WIDTH/MAP_HEIGHT
	for (y = 0; y < height && !eof; ++y) {
		if (feof(binary_map_fl)) {
			eof = TRUE;
			break;
		}
		
		for (x = 0; x < width && !eof; ++x) {
			switch (header.version) {
				/* notes on adding backwards-compatibility here:
				*  - create a new map_file_data_v# struct and new store_to_map_v# function
				*  - update #define store_to_map to use your new one
				*  - update the map_file_data typedef to use your new one
				*  - use this example to allow the game to load the last version:
				* example:
				case 1: {
					struct map_file_data_v1 store;
					fread(&store, sizeof(struct map_file_data_v1), 1, binary_map_fl);
					if (feof(binary_map_fl)) {
						eof = TRUE;
						break;
					}
					
					if (x < MAP_WIDTH && y < MAP_HEIGHT) {
						// only use it if it's within the current map width/height
						store_to_map_v1(&store, &world_map[x][y]);
					}
					break;
				}
				*/
				case CURRENT_BINARY_MAP_VERSION:
				default: {
					map_file_data store;
					fread(&store, sizeof(map_file_data), 1, binary_map_fl);
					if (feof(binary_map_fl)) {
						eof = TRUE;
						break;
					}
					
					if (x < MAP_WIDTH && y < MAP_HEIGHT) {
						// only use it if it's within the current map width/height
						store_to_map(&store, &world_map[x][y]);
					}
					break;
				}
			}
		}
	}
	
	block_world_save_requests = FALSE;
	
	// but ensure a full save if the version has changed
	if (header.version != CURRENT_BINARY_MAP_VERSION) {
		save_world_after_startup = TRUE;
	}
}


/**
* Loads one room from its .wld file into the game. This file will contain map
* data, room data, or both.
*
* @param room_vnum vnum The vnum of the room to load.
* @param char index_data The load code from the binary index file.
*/
void load_one_room_from_wld_file(room_vnum vnum, char index_data) {
	char line[MAX_INPUT_LENGTH], error[256], fname[256];
	struct shared_room_data *shared;
	struct room_direction_data *ex;
	struct depletion_data *dep;
	struct resource_data *res;
	struct affected_type *af;
	struct track_data *track;
	struct reset_com *reset;
	struct map_data *map = NULL;
	room_data *room;
	bool end;
	bitvector_t bit_in[2];
	long long_in;
	int int_in[4];
	char char_in, *reset_read, str_in[256];
	FILE *fl;
	
	map = (vnum < MAP_SIZE) ? &world_map[MAP_X_COORD(vnum)][MAP_Y_COORD(vnum)] : NULL;
	room = real_real_room(vnum);	// if loaded already for some reason
	shared = map ? map->shared : (room ? SHARED_DATA(room) : NULL);
	
	get_world_filename(fname, vnum, WLD_SUFFIX);
	if (!(fl = fopen(fname, "r"))) {
		if (errno == ENOENT) {
			log("Warning: Unable to find wld file %s: removing from world index", fname);
			update_world_index(vnum, 0);
		}
		else {
			log("SYSERR: Unable to open wld file %s", fname);
		}
		return;
	}
	
	// for fread_string
	sprintf(error, "load_one_room_from_wld_file: vnum %d (code %d)", vnum, index_data);
	
	end = FALSE;
	while (!end) {
		// load next line
		if (!get_line(fl, line)) {
			log("SYSERR: Unexpected end of file: %s", error);
			exit(1);
		}
		
		// tags by starting letter (for speed)
		switch (UPPER(*line)) {
			case '#': {	// the vnum line with the load code
				if (sscanf(line, "#%d %c", &int_in[0], &char_in) != 2) {
					log("SYSERR: Invalid # line: %s", error);
					exit(1);
				}
				// #code may be M or R -- NEED room if it's R
				if (char_in == 'R' && !room) {
					if (vnum >= MAP_SIZE || !(room = load_map_room(vnum, FALSE))) {
						// note that for map rooms this was actually handled by real_room() above calling load_map_room()
						CREATE(room, room_data, 1);
						room->vnum = vnum;
						
						CREATE(SHARED_DATA(room), struct shared_room_data, 1);
						SHARED_DATA(room)->island_id = NO_ISLAND;
						
						shared = SHARED_DATA(room);
						
						// put it in the hash table -- do not need to update_world_index as this is a startup load
						add_room_to_world_tables(room);
					}
				}
				break;
			}
			case 'A': {
				if (!strn_cmp(line, "Affect: ", 8) && room) {
					if (sscanf(line + 8, "%d %d %ld %d %d %llu", &int_in[0], &int_in[1], &long_in, &int_in[2], &int_in[3], &bit_in[0]) != 6) {
						log("SYSERR: Invalid affect line: %s", error);
						exit(1);
					}
					
					CREATE(af, struct affected_type, 1);
					af->type = int_in[0];
					af->cast_by = int_in[1];
					af->modifier = int_in[2];
					af->location = int_in[3];
					af->bitvector = bit_in[0];
					af->expire_time = (long_in == UNLIMITED) ? UNLIMITED : (time(0) + long_in);
					
					LL_PREPEND(ROOM_AFFECTS(room), af);
				}
				else if (!strn_cmp(line, "Aff-flags: ", 11)) {
					if (sscanf(line + 11, "%llu %llu", &bit_in[0], &bit_in[1]) != 2) {
						log("SYSERR: Invalid aff-flags line: %s", error);
						exit(1);
					}
					
					shared->base_affects = bit_in[0];
					shared->affects = bit_in[1];
				}
				BAD_WLD_TAG_WARNING
				break;
			}
			case 'B': {
				if (!strn_cmp(line, "Building: ", 10) && room) {
					attach_building_to_room(building_proto(atoi(line+10)), room, FALSE);
				}
				else if (!strn_cmp(line, "Built-with: ", 12) && room) {
					if (sscanf(line + 12, "%d %d %d %d", &int_in[0], &int_in[1], &int_in[2], &int_in[3]) != 4) {
						log("SYSERR: Invalid built-with line: %s", error);
						exit(1);
					}
					if (!COMPLEX_DATA(room)) {
						COMPLEX_DATA(room) = init_complex_data();
					}
					
					CREATE(res, struct resource_data, 1);
					res->type = int_in[0];
					res->vnum = int_in[1];
					res->misc = int_in[2];
					res->amount = int_in[3];
					LL_APPEND(GET_BUILT_WITH(room), res);
				}
				else if (!strn_cmp(line, "Burn-down-time: ", 16) && room) {
					if (!COMPLEX_DATA(room)) {
						COMPLEX_DATA(room) = init_complex_data();
					}
					set_burn_down_time(room, atoi(line+16), TRUE);
				}
				BAD_WLD_TAG_WARNING
				break;
			}
			case 'C': {
				if (!strn_cmp(line, "Crop: ", 6) && room) {
					set_crop_type(room, crop_proto(atoi(line+6)));
				}
				BAD_WLD_TAG_WARNING
				break;
			}
			case 'D': {
				if (!strn_cmp(line, "Damage: ", 8) && room) {
					if (!COMPLEX_DATA(room)) {
						COMPLEX_DATA(room) = init_complex_data();
					}
					COMPLEX_DATA(room)->damage = atof(line+8);
				}
				else if (!strn_cmp(line, "Depletion: ", 11)) {
					if (sscanf(line + 11, "%d %d", &int_in[0], &int_in[1]) != 2) {
						log("SYSERR: Invalid depletion line: %s", error);
						exit(1);
					}
					
					CREATE(dep, struct depletion_data, 1);
					dep->type = int_in[0];
					dep->count = int_in[1];
					LL_PREPEND(shared->depletion, dep);
				}
				else if (!strn_cmp(line, "Desc:", 5)) {
					if (shared->description) {
						free(shared->description);
					}
					shared->description = fread_string(fl, error);
				}
				BAD_WLD_TAG_WARNING
				break;
			}
			case 'E': {
				if (!strn_cmp(line, "End World File", 14)) {
					// actual end
					end = TRUE;
				}
				else if (!strn_cmp(line, "Entrance: ", 10) && room) {
					if (!COMPLEX_DATA(room)) {
						COMPLEX_DATA(room) = init_complex_data();
					}
					COMPLEX_DATA(room)->entrance = atoi(line+10);
				}
				else if (!strn_cmp(line, "Exit: ", 6) && room) {
					if (sscanf(line + 6, "%d %d %d %d", &int_in[0], &int_in[1], &int_in[2], &int_in[3]) != 4) {
						log("SYSERR: Invalid exit line: %s", error);
						exit(1);
					}
					if (!COMPLEX_DATA(room)) {
						COMPLEX_DATA(room) = init_complex_data();
					}
					
					// see if there's already an exit before making one
					if (!(ex = find_exit(room, int_in[0]))) {
						CREATE(ex, struct room_direction_data, 1);
						ex->dir = int_in[0];
						LL_INSERT_INORDER(COMPLEX_DATA(room)->exits, ex, sort_exits);
					}
					
					ex->exit_info = int_in[1];
					ex->to_room = int_in[2];	// this is a vnum; room pointer will be set in renum_world
					if (int_in[3] != 0) {	// followed by keywords ONLY if the last int is set
						if (ex->keyword) {
							free(ex->keyword);
						}
						ex->keyword = fread_string(fl, error);
					}
				}
				else if (!strn_cmp(line, "Extra: ", 7)) {
					if (sscanf(line + 7, "%d %d", &int_in[0], &int_in[1]) != 2) {
						log("SYSERR: Invalid extra-data line: %s", error);
						exit(1);
					}
					set_extra_data(&shared->extra_data, int_in[0], int_in[1]);
				}
				BAD_WLD_TAG_WARNING
				break;
			}
			case 'H': {
				if (!strn_cmp(line, "Home-room: ", 11) && room) {
					add_trd_home_room(vnum, atoi(line + 11));
				}
				else if (!strn_cmp(line, "Height: ", 8)) {
					shared->height = atoi(line + 8);
				}
				BAD_WLD_TAG_WARNING
				break;
			}
			case 'I': {
				if (!strn_cmp(line, "Icon:", 5)) {
					if (shared->icon) {
						free(shared->icon);
					}
					shared->icon = fread_string(fl, error);
				}
				else if (!strn_cmp(line, "Island: ", 8)) {
					shared->island_id = atoi(line + 8);
					shared->island_ptr = (shared->island_id == NO_ISLAND ? NULL : get_island(shared->island_id, TRUE));
				}
				BAD_WLD_TAG_WARNING
				break;
			}
			case 'L': {
				if (!strn_cmp(line, "Load: ", 6) && room) {
					if (strlen(line) < 7 || line[6] == '*') {
						// skip (blank or comment)
						break;
					}
					
					reset_read = line + 6;
					
					CREATE(reset, struct reset_com, 1);
					DL_APPEND(room->reset_commands, reset);
					
					if (*reset_read == 'A') {	// affect: type cast_by duration modifier location bitvector
						if (sscanf(reset_read, "%c %lld %lld %lld %lld %lld %s", &reset->command, &reset->arg1, &reset->arg2, &reset->arg3, &reset->arg4, &reset->arg5, str_in) != 7) {
							log("SYSERR: Invalid load command: %s: '%s'", error, line);
							exit(1);
						}
						reset->sarg1 = str_dup(str_in);
					}
					else if (*reset_read == 'S') {	// mob custom strings: <string type> <value>
						reset->command = *reset_read;
						reset->sarg1 = strdup(reset_read + 2);
						if (!strn_cmp(reset->sarg1, "look", 4)) {
							// look desc pulls the next section as a string
							reset->sarg2 = fread_string(fl, error);
						}
					}
					else {	// all other types:
						if (sscanf(reset_read, "%c %lld %lld %lld %d", &reset->command, &reset->arg1, &reset->arg2, &reset->arg3, &int_in[0]) != 5) {
							log("SYSERR: Invalid load command: %s: '%s'", error, line);
							exit(1);
						}
						else {
							// try to load strings ONLY if the last digit is non-zero
							if (int_in[0] != 0) {
								reset->sarg1 = fread_string(fl, error);
								reset->sarg2 = fread_string(fl, error);
							}
						}
					}
				}
				BAD_WLD_TAG_WARNING
				break;
			}
			case 'N': {
				if (!strn_cmp(line, "Name:", 5)) {
					if (shared->name) {
						free(shared->name);
					}
					shared->name = fread_string(fl, error);
				}
				BAD_WLD_TAG_WARNING
				break;
			}
			case 'O': {
				if (!strn_cmp(line, "Owner: ", 7) && room) {
					add_trd_owner(vnum, atoi(line+7));
				}
				BAD_WLD_TAG_WARNING
				break;
			}
			case 'P': {
				if (!strn_cmp(line, "Private: ", 9) && room) {
					if (!COMPLEX_DATA(room)) {
						COMPLEX_DATA(room) = init_complex_data();
					}
					COMPLEX_DATA(room)->private_owner = atoi(line+9);
				}
				BAD_WLD_TAG_WARNING
				break;
			}
			case 'R': {
				if (!strn_cmp(line, "Resource: ", 10) && room) {
					if (sscanf(line + 10, "%d %d %d %d", &int_in[0], &int_in[1], &int_in[2], &int_in[3]) != 4) {
						log("SYSERR: Invalid resource line: %s", error);
						exit(1);
					}
					if (!COMPLEX_DATA(room)) {
						COMPLEX_DATA(room) = init_complex_data();
					}
					
					CREATE(res, struct resource_data, 1);
					res->type = int_in[0];
					res->vnum = int_in[1];
					res->misc = int_in[2];
					res->amount = int_in[3];
					LL_APPEND(COMPLEX_DATA(room)->to_build, res);
				}
				BAD_WLD_TAG_WARNING
				break;
			}
			case 'S': {
				if (!strn_cmp(line, "Sector: ", 8) && room) {
					if (sscanf(line + 8, "%d %d", &int_in[0], &int_in[1]) != 2) {
						log("SYSERR: Invalid sector line: %s", error);
						exit(1);
					}
					
					// need to unlink shared data, if present, if this is not an ocean
					if (int_in[0] != BASIC_OCEAN && SHARED_DATA(room) == &ocean_shared_data) {
						// converting from ocean to non-ocean
						SHARED_DATA(room) = NULL;	// unlink ocean share
						CREATE(SHARED_DATA(room), struct shared_room_data, 1);
						if (map) {	// and pass this shared data back up to the world
							map->shared = SHARED_DATA(room);
						}
						shared = SHARED_DATA(room);
					}
					
					room->sector_type = sector_proto(int_in[0]);
					room->base_sector = sector_proto(int_in[1]);
					
					// pass back up to map?
					if (map) {
						map->sector_type = room->sector_type;
						map->base_sector = room->base_sector;
					}
					
					// set up building data?
					if (IS_ANY_BUILDING(room) || IS_ADVENTURE_ROOM(room)) {
						COMPLEX_DATA(room) = init_complex_data();
					}
				}
				BAD_WLD_TAG_WARNING
				break;
			}
			case 'T': {
				if (!strn_cmp(line, "Template: ", 10) && room) {
					attach_template_to_room(room_template_proto(atoi(line+10)), room);
				}
				else if (!strn_cmp(line, "Track: ", 7)) {
					if (sscanf(line + 7, "%d %ld %d %d", &int_in[0], &long_in, &int_in[1], &int_in[2]) != 4) {
						log("SYSERR: Invalid track line: %s", error);
						exit(1);
					}
					
					HASH_FIND_INT(shared->tracks, &int_in[0], track);
					if (!track) {
						CREATE(track, struct track_data, 1);
						track->id = int_in[0];
						HASH_ADD_INT(shared->tracks, id, track);
					}
					track->timestamp = long_in;
					track->dir = int_in[1];
					track->to_room = int_in[2];
				}
				BAD_WLD_TAG_WARNING
				break;
			}
		}	// switch(starting letter)
	}	// loop
	
	fclose(fl);
}


/**
* Reads in the binary index file and then loads whatever world files it
* indicates. To be called at startup, AFTER loading in the world map.
*/
void load_world_from_binary_index(void) {
	FILE *index_fl;
	room_vnum vnum, top_vnum = 0, count;
	int iter;
	
	// open the index
	if (!(index_fl = fopen(BINARY_WORLD_INDEX, "r+b"))) {
		// b5.116: attempt to boot old-style world instead
		log("Booting with no world files (binary index file %s does not exist)", BINARY_WORLD_INDEX);
		log("Attempting to load pre-b5.116 world files...");
		index_boot(DB_BOOT_WLD);
		save_world_after_startup = TRUE;
		converted_to_b5_116 = TRUE;
		return;
	}
	
	// don't allow loading to trigger saves
	block_world_save_requests = TRUE;
	
	// read size
	fread(&top_vnum, sizeof(int), 1, index_fl);
	
	// make space
	if (top_vnum > top_of_world_index || !world_index_data) {
		if (world_index_data) {
			// reallocate and clear the new space
			RECREATE(world_index_data, char, top_vnum+1);
			for (iter = top_of_world_index+1; iter <= top_vnum; ++iter) {
				world_index_data[iter] = 0;
			}
		}
		else {
			CREATE(world_index_data, char, top_vnum+1);
		}
		top_of_world_index = top_vnum;
	}
	
	fread(world_index_data, sizeof(char), top_of_world_index+1, index_fl);
	fclose(index_fl);
	
	// and load rooms...
	count = 0;
	for (vnum = 0; vnum <= top_of_world_index; ++vnum) {
		// only load if it's not a 0
		if (world_index_data[vnum] != 0) {
			load_one_room_from_wld_file(vnum, world_index_data[vnum]);
			++count;
		}
	}
	
	log("Loaded %d room%s from wld files", count, PLURAL(count));
	block_world_save_requests = FALSE;
}


/**
* Applies a loaded map_file_data struct to a map tile in-game. Copy this
* function when you need to make changes to map_file_data, but keep the v1
* structure and function so your EmpireMUD can still load your existing binary
* map file.
*
* @param struct map_file_data_v1 *store The data loaded from file.
* @param struct map_data *map The tile to apply to.
*/
void store_to_map_v1(struct map_file_data_v1 *store, struct map_data *map) {
	// warning: do not assign any 'shared' data until after the SHARED assignment
	
	map->sector_type = sector_proto(store->sector_type);
	map->base_sector = sector_proto(store->base_sector);
	map->natural_sector = sector_proto(store->natural_sector);
	map->crop_type = crop_proto(store->crop_type);
	
	// SHARED: add/remove shared ocean pointer?
	if (store->sector_type != BASIC_OCEAN && map->shared == &ocean_shared_data) {
		map->shared = NULL;	// unlink basic ocean
		CREATE(map->shared, struct shared_room_data, 1);
	}
	else if (store->sector_type == BASIC_OCEAN && map->shared != &ocean_shared_data) {
		if (map->shared) {
			free_shared_room_data(map->shared);
		}
		map->shared = &ocean_shared_data;
	}
	
	// only grab these if it's not basic-ocean
	if (map->shared != &ocean_shared_data) {
		map->shared->island_id = store->island_id;
		map->shared->island_ptr = (store->island_id == NO_ISLAND) ? NULL : get_island(store->island_id, TRUE);
		map->shared->affects = store->affects;
		map->shared->base_affects = store->base_affects;
		map->shared->height = store->height;
	}
	
	// ignore store->misc
}


 //////////////////////////////////////////////////////////////////////////////
//// MAPOUT SYSTEM ///////////////////////////////////////////////////////////

/**
* This is centralized to make sure it's always consistent, which is important
* for fseek'ing the file.
*
* @return char* The header text for a mapout file (map.txt, etc).
*/
char *get_mapout_header(void) {
	static char output[256];
	snprintf(output, sizeof(output), "%dx%d\n", MAP_WIDTH, MAP_HEIGHT);
	return output;
}


/**
* Writes the city data used by the graphical map.
*/
void write_city_data_file(void) {
	struct empire_city_data *city;
	empire_data *emp, *next_emp;
	FILE *cit_fl;

	if (!(cit_fl = fopen(CITY_DATA_FILE TEMP_SUFFIX, "w"))) {
		log("SYSERR: Unable to open file '%s' for writing", CITY_DATA_FILE TEMP_SUFFIX);
		return;
	}
	
	HASH_ITER(hh, empire_table, emp, next_emp) {
		if (EMPIRE_IMM_ONLY(emp)) {
			continue;
		}
		
		for (city = EMPIRE_CITY_LIST(emp); city; city = city->next) {
			fprintf(cit_fl, "%d %d %d \"%s\" \"%s\"\n", X_COORD(city->location), Y_COORD(city->location), city_type[city->type].radius, city->name, EMPIRE_NAME(emp));
		}
	}
	
	fprintf(cit_fl, "$\n");
	fclose(cit_fl);
	rename(CITY_DATA_FILE TEMP_SUFFIX, CITY_DATA_FILE);
}


/**
* Writes one tile entry to the graphical map data files.
*
* @param struct map_data *map The tile to write.
* @param FILE *map_fl The geographical map file, open for writing.
* @param FILE *pol_fl The political map file, open for writing.
*/
void write_graphical_map_data(struct map_data *map, FILE *map_fl, FILE *pol_fl) {
	sector_data *sect = map->sector_type;
	empire_data *emp;
	
	if (map->room && ROOM_AFF_FLAGGED(map->room, ROOM_AFF_CHAMELEON) && IS_COMPLETE(map->room)) {
		sect = map->base_sector;
	}
	
	// normal map output
	if (IS_SET(map->shared->affects, ROOM_AFF_MAPOUT_BUILDING)) {
		fputc('s', map_fl);
	}
	else if (SECT_FLAGGED(sect, SECTF_HAS_CROP_DATA) && map->crop_type) {
		fputc(mapout_color_tokens[GET_CROP_MAPOUT(map->crop_type)], map_fl);
	}
	else {
		fputc(mapout_color_tokens[GET_SECT_MAPOUT(sect)], map_fl);
	}

	// political output
	if (map->room && (emp = ROOM_OWNER(map->room)) && (!ROOM_AFF_FLAGGED(map->room, ROOM_AFF_CHAMELEON) || !IS_COMPLETE(map->room)) && EMPIRE_MAPOUT_TOKEN(emp) != 0) {
		fputc(EMPIRE_MAPOUT_TOKEN(emp), pol_fl);
	}
	else {
		// no owner -- only some sects get printed
		if (IS_SET(map->shared->affects, ROOM_AFF_MAPOUT_BUILDING)) {
			fputc('s', pol_fl);
		}
		else if (SECT_FLAGGED(sect, SECTF_SHOW_ON_POLITICAL_MAPOUT)) {
			fputc(mapout_color_tokens[GET_SECT_MAPOUT(sect)], pol_fl);
		}
		else {
			fputc('?', pol_fl);
		}
	}
}


/**
* Writes the full mapout data (used to generate the graphical version of the
* map). This is called at startup or with the 'mapout' command. Any changes
* are updated piecemeal.
*/
void write_whole_mapout(void) {
	FILE *map_fl, *pol_fl;
	int x, y;

	// open geographical map data file
	if (!(map_fl = fopen(GEOGRAPHIC_MAP_FILE TEMP_SUFFIX, "w"))) {
		log("SYSERR: Unable to open file '%s' for writing", GEOGRAPHIC_MAP_FILE TEMP_SUFFIX);
		return;
	}
	
	// open political map data file
	if (!(pol_fl = fopen(POLITICAL_MAP_FILE TEMP_SUFFIX, "w"))) {
		log("SYSERR: Unable to open file '%s' for writing", POLITICAL_MAP_FILE TEMP_SUFFIX);
		return;
	}
	
	// write header data
	fprintf(map_fl, "%s", get_mapout_header());
	fprintf(pol_fl, "%s", get_mapout_header());
	
	// write world_map:
	for (y = 0; y < MAP_HEIGHT; ++y) {
		// iterating x inside of y will ensure we're in vnum-order here
		for (x = 0; x < MAP_WIDTH; ++x) {
			write_graphical_map_data(&world_map[x][y], map_fl, pol_fl);
		}	// end X
		
		// end of row
		fprintf(map_fl, "\n");
		fprintf(pol_fl, "\n");	
	}	// end Y

	// close files
	fclose(map_fl);
	rename(GEOGRAPHIC_MAP_FILE TEMP_SUFFIX, GEOGRAPHIC_MAP_FILE);
	
	fclose(pol_fl);
	rename(POLITICAL_MAP_FILE TEMP_SUFFIX, POLITICAL_MAP_FILE);
	
	// also write a city data file
	write_city_data_file();
}


/**
* Updates the mapout files (used to generate the graphical map) with any tiles
* that were added with request_mapout_update(vnum).
*/
void write_mapout_updates(void) {
	FILE *map_fl, *pol_fl;
	int x, y, header_size, pos;
	struct vnum_hash *iter, *next_iter;
	
	if (!mapout_update_requests) {
		return;	// no work
	}
	
	// open geographical map data file
	if (!(map_fl = fopen(GEOGRAPHIC_MAP_FILE, "r+"))) {
		log("SYSERR: Unable to open file '%s' for updates; saving fresh file", GEOGRAPHIC_MAP_FILE);
		write_whole_mapout();
		return;
	}
	
	// open political map data file
	if (!(pol_fl = fopen(POLITICAL_MAP_FILE, "r+"))) {
		log("SYSERR: Unable to open file '%s' for updates; saving fresh file", POLITICAL_MAP_FILE);
		write_whole_mapout();
		return;
	}
	
	header_size = strlen(get_mapout_header());
	
	HASH_ITER(hh, mapout_update_requests, iter, next_iter) {
		if (iter->vnum >= 0 && iter->vnum < MAP_SIZE) {
			x = MAP_X_COORD(iter->vnum);
			y = MAP_Y_COORD(iter->vnum);
			
			// each line after the header is width+1 (for the \n)
			pos = header_size + (y * (MAP_WIDTH + 1)) + x;
			fseek(map_fl, pos * sizeof(char), SEEK_SET);
			fseek(pol_fl, pos * sizeof(char), SEEK_SET);
			
			write_graphical_map_data(&world_map[x][y], map_fl, pol_fl);
		}
		
		// cleanup
		HASH_DEL(mapout_update_requests, iter);
		free(iter);
	}
	
	// close files
	fclose(map_fl);
	fclose(pol_fl);
}
