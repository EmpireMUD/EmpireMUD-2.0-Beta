/* ************************************************************************
*   File: db.world.c                                      EmpireMUD 2.0b5 *
*  Usage: Modify functions for the map, interior, and game world          *
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
#include "db.h"
#include "comm.h"
#include "handler.h"
#include "skills.h"
#include "dg_scripts.h"
#include "dg_event.h"
#include "vnums.h"

/**
* Contents:
*   World-Changers
*   Management
*   Annual Map Update
*   Burning Buildings
*   City Lib
*   Room Resets
*   Sector Indexing
*   Taverns
*   Territory
*   Trench Filling
*   Evolutions
*   Helpers
*   Map Output
*   World Map System
*/

// external vars
extern struct city_metadata_type city_type[];
extern const sector_vnum climate_default_sector[NUM_CLIMATES];
extern bool need_world_index;
extern const int rev_dir[];
extern bool world_map_needs_save;
extern struct map_data *last_evo_tile;
extern sector_data *last_evo_sect;
extern int evos_per_hour;


// external funcs
void add_room_to_world_tables(room_data *room);
extern struct resource_data *combine_resources(struct resource_data *combine_a, struct resource_data *combine_b);
void complete_building(room_data *room);
void delete_territory_entry(empire_data *emp, struct empire_territory_data *ter);
extern struct complex_room_data *init_complex_data();
void free_complex_data(struct complex_room_data *bld);
void free_shared_room_data(struct shared_room_data *data);
void grow_crop(struct map_data *map);
extern FILE *open_world_file(int block);
void remove_room_from_world_tables(room_data *room);
void save_and_close_world_file(FILE *fl, int block);
void setup_start_locations();
void sort_exits(struct room_direction_data **list);
void sort_world_table();
void stop_room_action(room_data *room, int action, int chore);
void write_room_to_file(FILE *fl, room_data *room);

// locals
int count_city_points_used(empire_data *emp);
struct empire_territory_data *create_territory_entry(empire_data *emp, room_data *room);
void decustomize_room(room_data *room);
room_vnum find_free_vnum();
crop_data *get_potential_crop_for_location(room_data *location);
void init_room(room_data *room, room_vnum vnum);
int naturalize_newbie_island(struct map_data *tile, bool do_unclaim);
void ruin_one_building(room_data *room);
void save_world_map_to_file();
void schedule_check_unload(room_data *room, bool offset);
void schedule_trench_fill(struct map_data *map);
extern int sort_empire_islands(struct empire_island *a, struct empire_island *b);
void update_island_names();


 //////////////////////////////////////////////////////////////////////////////
//// WORLD-CHANGERS //////////////////////////////////////////////////////////

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
	crop_data *cp;
	
	if (ROOM_SECT_FLAGGED(room, SECTF_CROP) && ROOM_CROP_FLAGGED(room, CROPF_IS_ORCHARD) && (cp = ROOM_CROP(room))) {
		// TODO: This is a special case for orchards
		
		// check if original sect was stored to the crop
		if (BASE_SECT(room) != SECT(room)) {
			change_terrain(room, GET_SECT_VNUM(BASE_SECT(room)));
		}
		else {
			// default
			change_terrain(room, climate_default_sector[GET_CROP_CLIMATE(cp)]);
		}
	}
	else if ((evo = get_evolution_by_type(SECT(room), EVO_CHOPPED_DOWN))) {
		// normal case
		change_terrain(room, evo->becomes);
	}
	else {
		// it's actually okay to call this on an unchoppable room... just mark it more depleted
		add_depletion(room, DPLTN_CHOP, FALSE);
	}
}


/**
* This function safely changes terrain by disassociating any old data, and
* also storing a new original sect if necessary.
*
* @param room_data *room The room to change.
* @param sector_vnum sect Any sector vnum
*/
void change_terrain(room_data *room, sector_vnum sect) {
	extern crop_data *get_potential_crop_for_location(room_data *location);
	void lock_icon(room_data *room, struct icon_data *use_icon);
	
	sector_data *old_sect = SECT(room), *st = sector_proto(sect);
	struct map_data *map, *temp;
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
	
	// tear down any building data and customizations
	disassociate_building(room);
	
	// need to determine a crop before we change it?
	if (SECT_FLAGGED(st, SECTF_HAS_CROP_DATA) && !ROOM_CROP(room)) {
		new_crop = get_potential_crop_for_location(room);
	}
	
	// change sect
	perform_change_sect(room, NULL, st);
	perform_change_base_sect(room, NULL, st);
		
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
	if (ROOM_SECT_FLAGGED(room, SECTF_LOCK_ICON)) {
		lock_icon(room, NULL);
	}
	
	// need start locations update?
	if (SECT_FLAGGED(old_sect, SECTF_START_LOCATION) != SECT_FLAGGED(st, SECTF_START_LOCATION)) {
		setup_start_locations();
	}
	
	// need land-map update?
	if (st != old_sect && SECT_IS_LAND_MAP(st) != SECT_IS_LAND_MAP(old_sect)) {
		map = &(world_map[FLAT_X_COORD(room)][FLAT_Y_COORD(room)]);
		if (SECT_IS_LAND_MAP(st)) {
			// add to land_map (at the start is fine)
			map->next = land_map;
			land_map = map;
		}
		else {
			// remove from land_map
			REMOVE_FROM_LIST(map, land_map, next);
			// do NOT free map -- it's a pointer to something in world_map
		}
	}
	
	// for later
	emp = ROOM_OWNER(room);
	
	// did it become unclaimable?
	if (emp && SECT_FLAGGED(st, SECTF_NO_CLAIM)) {
		abandon_room(room);
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
		
		ex->next = COMPLEX_DATA(from)->exits;
		COMPLEX_DATA(from)->exits = ex;
		sort_exits(&(COMPLEX_DATA(from)->exits));

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
		
		other->next = COMPLEX_DATA(to)->exits;
		COMPLEX_DATA(to)->exits = other;
		sort_exits(&(COMPLEX_DATA(to)->exits));
		
		// re-find after sort
		other = find_exit(to, rev_dir[dir]);
	}
	
	// similar to above
	if (other) {
		other->to_room = GET_ROOM_VNUM(from);
		other->room_ptr = from;
		++GET_EXITS_HERE(from);
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
	
	// only if saveable
	if (!CAN_UNLOAD_MAP_ROOM(room)) {
		need_world_index = TRUE;
	}
	
	COMPLEX_DATA(room)->home_room = home;
	GET_MAP_LOC(room) = home ? GET_MAP_LOC(home) : NULL;
	GET_ISLAND_ID(room) = home ? GET_ISLAND_ID(home) : NO_ISLAND;
	GET_ISLAND(room) = home ? GET_ISLAND(home) : NULL;
	
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
	EVENT_CANCEL_FUNC(cancel_room_expire_event);
	void extract_pending_chars();
	extern struct instance_data *find_instance_by_room(room_data *room, bool check_homeroom);
	void perform_abandon_city(empire_data *emp, struct empire_city_data *city, bool full_abandon);
	void relocate_players(room_data *room, room_data *to_room);
	void remove_room_from_vehicle(room_data *room, vehicle_data *veh);

	struct room_direction_data *ex, *next_ex, *temp;
	struct empire_territory_data *ter, *next_ter;
	struct empire_city_data *city, *next_city;
	room_data *rm_iter, *next_rm, *home;
	vehicle_data *veh, *next_veh;
	struct instance_data *inst;
	struct affected_type *af;
	struct reset_com *reset;
	char_data *c, *next_c;
	obj_data *o, *next_o;
	empire_data *emp, *next_emp;
	int iter;

	if (!room || (GET_ROOM_VNUM(room) < MAP_SIZE && !CAN_UNLOAD_MAP_ROOM(room))) {
		syslog(SYS_ERROR, 0, TRUE, "SYSERR: delete_room() attempting to delete invalid room %d", room ? GET_ROOM_VNUM(room) : NOWHERE);
		return;
	}
	
	if (room == dg_owner_room) {
		dg_owner_purged = 1;
		dg_owner_room = NULL;
	}
	
	// delete this first
	cancel_stored_event_room(room, SEV_CHECK_UNLOAD);
	
	// ensure not owned
	if (ROOM_OWNER(room)) {
		perform_abandon_room(room);
	}
	
	// delete any open instance here
	if (ROOM_AFF_FLAGGED(room, ROOM_AFF_HAS_INSTANCE) && (inst = find_instance_by_room(room, FALSE))) {
		SET_BIT(inst->flags, INST_COMPLETED);
	}
	
	if (check_exits) {
		// search world for portals that link here
		for (o = object_list; o; o = next_o) {
			next_o = o->next;
		
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
	LL_FOREACH_SAFE2(ROOM_VEHICLES(room), veh, next_veh, next_in_room) {
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
	for (c = ROOM_PEOPLE(room); c; c = next_c) {
		next_c = c->next_in_room;
		if (!IS_NPC(c)) {
			save_char(c, NULL);
		}
		extract_all_items(c);
		extract_char(c);
	}
	extract_pending_chars();

	/* Remove the objects */
	while (ROOM_CONTENTS(room)) {
		extract_obj(ROOM_CONTENTS(room));
	}
	
	// free some crap
	if (COMPLEX_DATA(room)) {
		free_complex_data(COMPLEX_DATA(room));
		COMPLEX_DATA(room) = NULL;
	}
	if (SCRIPT(room)) {
		extract_script(room, WLD_TRIGGER);
	}
	free_proto_scripts(&room->proto_script);
	room->proto_script = NULL;
	
	while ((reset = room->reset_commands)) {
		room->reset_commands = reset->next;
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
			event_cancel(af->expire_event, cancel_room_expire_event);
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
						REMOVE_FROM_LIST(ex, COMPLEX_DATA(rm_iter)->exits, next);
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
	else {
		// just check home rooms of interiors
		for (rm_iter = interior_room_list; rm_iter; rm_iter = next_rm) {
			next_rm = rm_iter->next_interior;
			
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
	if (ROOM_AFF_FLAGGED(room, ROOM_AFF_HAS_INSTANCE) || (COMPLEX_DATA(room) && COMPLEX_DATA(room)->instance)) {
		for (inst = instance_list; inst; inst = inst->next) {
			if (inst->location == room) {
				SET_BIT(inst->flags, INST_COMPLETED);
				inst->location = NULL;
			}
			if (inst->start == room) {
				SET_BIT(inst->flags, INST_COMPLETED);
				inst->start = NULL;
			}
			for (iter = 0; iter < inst->size; ++iter) {
				if (inst->room[iter] == room) {
					inst->room[iter] = NULL;
				}
			}
		}
	}

	// update empires
	HASH_ITER(hh, empire_table, emp, next_emp) {
		// update empire territory
		for (ter = EMPIRE_TERRITORY_LIST(emp); ter; ter = next_ter) {
			next_ter = ter->next;
			
			if (ter->room == room) {
				delete_territory_entry(emp, ter);
			}
		}
		
		// update all empire cities
		for (city = EMPIRE_CITY_LIST(emp); city; city = next_city) {
			next_city = city->next;
			if (city->location == room) {
				// ... this should not be possible, but just in case ...
				perform_abandon_city(emp, city, FALSE);
			}
		}
	}
	
	// check for start location changes?
	if (ROOM_SECT_FLAGGED(room, SECTF_START_LOCATION)) {
		setup_start_locations();
	}
	
	// only have to free this info if not on the map (map rooms have a pointer to the map)
	if (GET_ROOM_VNUM(room) >= MAP_SIZE && SHARED_DATA(room)) {
		free_shared_room_data(SHARED_DATA(room));
	}
	
	// free the room
	free(room);
	
	// maybe
	// world_is_sorted = FALSE;
	need_world_index = TRUE;
}


/* When a trench fills up by "pour out" or by rain, this happens */
void fill_trench(room_data *room) {	
	char lbuf[MAX_INPUT_LENGTH];
	struct evolution_data *evo;
	
	if ((evo = get_evolution_by_type(SECT(room), EVO_TRENCH_FULL)) != NULL) {
		if (ROOM_PEOPLE(room)) {
			sprintf(lbuf, "The trench is full! It is now a %s!", GET_SECT_NAME(sector_proto(evo->becomes)));
			act(lbuf, FALSE, ROOM_PEOPLE(room), 0, 0, TO_CHAR | TO_ROOM);
		}
		change_terrain(room, evo->becomes);
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


/**
* Choose a mine type for a room. The 'ch' is optional, for ability
* requirements.
*
* @param room_data *room The room to choose a mine type for.
* @param char_data *ch The character setting up mine data (for abilities).
*/
void init_mine(room_data *room, char_data *ch) {
	extern adv_data *get_adventure_for_vnum(rmt_vnum vnum);
	
	struct global_data *glb, *next_glb, *choose_last, *found;
	bool done_cumulative = FALSE;
	int cumulative_prc;
	adv_data *adv;
	
	// no work
	if (!room || !ROOM_CAN_MINE(room) || get_room_extra_data(room, ROOM_EXTRA_MINE_GLB_VNUM) > 0) {
		return;
	}
	
	adv = (GET_ROOM_TEMPLATE(room) ? get_adventure_for_vnum(GET_RMT_VNUM(GET_ROOM_TEMPLATE(room))) : NULL);
	cumulative_prc = number(1, 10000);
	choose_last = found = NULL;
	
	HASH_ITER(hh, globals_table, glb, next_glb) {
		if (GET_GLOBAL_TYPE(glb) != GLOBAL_MINE_DATA) {
			continue;
		}
		if (IS_SET(GET_GLOBAL_FLAGS(glb), GLB_FLAG_IN_DEVELOPMENT)) {
			continue;
		}
		if (GET_GLOBAL_ABILITY(glb) != NO_ABIL && (!ch || !has_ability(ch, GET_GLOBAL_ABILITY(glb)))) {
			continue;
		}
		
		// level limits
		if (GET_GLOBAL_MIN_LEVEL(glb) > 0 && (!ch || GET_COMPUTED_LEVEL(ch) < GET_GLOBAL_MIN_LEVEL(glb))) {
			continue;
		}
		if (GET_GLOBAL_MAX_LEVEL(glb) > 0 && ch && GET_COMPUTED_LEVEL(ch) > GET_GLOBAL_MAX_LEVEL(glb)) {
			continue;
		}
		
		// match ALL type-flags
		if ((GET_SECT_FLAGS(BASE_SECT(room)) & GET_GLOBAL_TYPE_FLAGS(glb)) != GET_GLOBAL_TYPE_FLAGS(glb)) {
			continue;
		}
		// match ZERO type-excludes
		if ((GET_SECT_FLAGS(BASE_SECT(room)) & GET_GLOBAL_TYPE_EXCLUDE(glb)) != 0) {
			continue;
		}
		
		// check adventure-only -- late-matching because it does more work than other conditions
		if (IS_SET(GET_GLOBAL_FLAGS(glb), GLB_FLAG_ADVENTURE_ONLY) && get_adventure_for_vnum(GET_GLOBAL_VNUM(glb)) != adv) {
			continue;
		}
		
		// percent checks last
		if (IS_SET(GET_GLOBAL_FLAGS(glb), GLB_FLAG_CUMULATIVE_PERCENT)) {
			if (done_cumulative) {
				continue;
			}
			cumulative_prc -= (int)(GET_GLOBAL_PERCENT(glb) * 100);
			if (cumulative_prc <= 0) {
				done_cumulative = TRUE;
			}
			else {
				continue;	// not this time
			}
		}
		else if (number(1, 10000) > (int)(GET_GLOBAL_PERCENT(glb) * 100)) {
			// normal not-cumulative percent
			continue;
		}
		
		// we have a match!
		
		// check choose-last
		if (IS_SET(GET_GLOBAL_FLAGS(glb), GLB_FLAG_CHOOSE_LAST)) {
			if (!choose_last) {
				choose_last = glb;
			}
			continue;
		}
		else {	// not choose-last
			found = glb;
			break;	// can only use first match
		}
	}
	
	// failover
	if (!found) {
		found = choose_last;
	}
	
	// VICTORY: set mine type
	if (found) {
		set_room_extra_data(room, ROOM_EXTRA_MINE_GLB_VNUM, GET_GLOBAL_VNUM(found));
		set_room_extra_data(room, ROOM_EXTRA_MINE_AMOUNT, number(GET_GLOBAL_VAL(found, GLB_VAL_MAX_MINE_SIZE) / 2, GET_GLOBAL_VAL(found, GLB_VAL_MAX_MINE_SIZE)));
		
		if (ch && has_ability(ch, ABIL_DEEP_MINES)) {
			multiply_room_extra_data(room, ROOM_EXTRA_MINE_AMOUNT, 1.5);
			gain_ability_exp(ch, ABIL_DEEP_MINES, 15);
		}
		
		if (ch && GET_GLOBAL_ABILITY(found) != NO_ABIL) {
			gain_ability_exp(ch, GET_GLOBAL_ABILITY(found), 75);
		}
	}
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
	stop_room_action(room, ACT_FILLING_IN, NOTHING);
	stop_room_action(room, ACT_EXCAVATING, NOTHING);
	
	map = &(world_map[FLAT_X_COORD(room)][FLAT_Y_COORD(room)]);
	if (SECT(room) !=  map->natural_sector) {
		// return to nature
		to_sect = map->natural_sector;
	}
	else {
		// de-evolve sect
		to_sect = reverse_lookup_evolution_for_sector(SECT(room), EVO_TRENCH_START);
	}
	
	if (to_sect) {
		change_terrain(room, GET_SECT_VNUM(to_sect));
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
		world_map_needs_save = TRUE;
	}
}


 //////////////////////////////////////////////////////////////////////////////
//// MANAGEMENT //////////////////////////////////////////////////////////////

/**
* Executes a full-world save.
*/
void save_whole_world(void) {
	void save_instances();
	
	FILE *fl = NULL, *index = NULL;
	room_data *iter, *next_iter;
	room_vnum vnum;
	int block, last;
	
	last = -1;
	
	// must sort first
	sort_world_table();
	
	// open index file
	if (need_world_index) {
		if (!(index = fopen(WLD_PREFIX INDEX_FILE TEMP_SUFFIX, "w"))) {
			syslog(SYS_ERROR, LVL_START_IMM, TRUE, "SYSERR: Unable to write index file '%s': %s", WLD_PREFIX INDEX_FILE TEMP_SUFFIX, strerror(errno));
			return;
		}
	}
	
	HASH_ITER(hh, world_table, iter, next_iter) {
		vnum = GET_ROOM_VNUM(iter);
		block = GET_WORLD_BLOCK(vnum);
		
		if (block != last || !fl) {
			if (index) {
				fprintf(index, "%d%s\n", block, WLD_SUFFIX);
			}
			
			if (fl) {
				save_and_close_world_file(fl, last);
				fl = NULL;
			}
			fl = open_world_file(block);
			last = block;
		}
		
		// only save a room at all if it couldn't be unloaded
		if (!CAN_UNLOAD_MAP_ROOM(iter)) {
			write_room_to_file(fl, iter);
		}
	}
	
	// cleanup
	if (fl) {
		save_and_close_world_file(fl, last);
	}
	if (index) {
		fprintf(index, "$\n");
		fclose(index);
		rename(WLD_PREFIX INDEX_FILE TEMP_SUFFIX, WLD_PREFIX INDEX_FILE);
		need_world_index = FALSE;
	}
	
	// ensure this
	save_instances();
	save_world_map_to_file();
}


 //////////////////////////////////////////////////////////////////////////////
//// ANNUAL MAP UPDATE ///////////////////////////////////////////////////////

/**
* Updates a set of depletions (halves them each year).
*
* @param struct depletion_data **list The list to update.
*/
void annual_update_depletions(struct depletion_data **list) {
	struct depletion_data *dep, *next_dep;
	
	LL_FOREACH_SAFE(*list, dep, next_dep) {
		// halve each year
		dep->count /= 2;

		if (dep->count < 10) {
			// at this point just remove it to save space, or it will take years to hit 0
			LL_DELETE(*list, dep);
			free(dep);
		}
	}
}


/**
* Process map-only annual updates on one map tile.
* Note: This is not called on OCEAN tiles (ones that don't go in the land map).
*
* @param struct map_data *tile The map tile to update.
*/
void annual_update_map_tile(struct map_data *tile) {
	extern char *get_room_name(room_data *room, bool color);
	
	struct resource_data *old_list;
	int trenched, amount;
	empire_data *emp;
	room_data *room;
	double dmg;
	
	// actual room is optional
	room = real_real_room(tile->vnum);
	
	// updates that only matter if there's a room:
	if (room) {
		if (IS_RUINS(room)) {
			// roughly 2 real years for average chance for ruins to be gone
			if (!number(0, 89)) {
				disassociate_building(room);
				abandon_room(room);
			
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
			dmg = MAX(1.0, dmg);
		
			// apply damage
			COMPLEX_DATA(room)->damage += dmg;
		
			// apply maintenance resources (if any, and if it's not being dismantled)
			if (GET_BLD_YEARLY_MAINTENANCE(GET_BUILDING(room)) && !IS_DISMANTLING(room)) {
				old_list = GET_BUILDING_RESOURCES(room);
				GET_BUILDING_RESOURCES(room) = combine_resources(old_list, GET_BLD_YEARLY_MAINTENANCE(GET_BUILDING(room)));
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
	
	// clean mine data from anything that's not currently a mine
	if (!room || !HAS_FUNCTION(room, FNC_MINE)) {
		remove_extra_data(&tile->shared->extra_data, ROOM_EXTRA_MINE_GLB_VNUM);
		remove_extra_data(&tile->shared->extra_data, ROOM_EXTRA_MINE_AMOUNT);
		remove_extra_data(&tile->shared->extra_data, ROOM_EXTRA_PROSPECT_EMPIRE);
	}
}


/**
* Runs an annual update (mainly, maintenance) on the vehicle.
*/
void annual_update_vehicle(vehicle_data *veh) {
	void fully_empty_vehicle(vehicle_data *veh);
	
	static struct resource_data *default_res = NULL;
	struct resource_data *old_list;
	
	// resources if it doesn't have its own
	if (!default_res) {
		add_to_resource_list(&default_res, RES_OBJECT, o_NAILS, 1, 0);
	}
	
	// does not take annual damage (unless incomplete)
	if (!VEH_YEARLY_MAINTENANCE(veh) && VEH_IS_COMPLETE(veh)) {
		return;
	}
	
	VEH_HEALTH(veh) -= MAX(1.0, ((double) VEH_MAX_HEALTH(veh) / 10.0));
	
	if (VEH_HEALTH(veh) > 0) {
		// add maintenance
		old_list = VEH_NEEDS_RESOURCES(veh);
		VEH_NEEDS_RESOURCES(veh) = combine_resources(old_list, VEH_YEARLY_MAINTENANCE(veh) ? VEH_YEARLY_MAINTENANCE(veh) : default_res);
		free_resource_list(old_list);
	}
	else {	// destroyed
		// return of 0 prevents the decay
		if (!destroy_vtrigger(veh)) {
			VEH_HEALTH(veh) = MAX(1, VEH_HEALTH(veh));	// ensure health
			return;
		}
		
		if (ROOM_PEOPLE(IN_ROOM(veh))) {
			act("$V crumbles from disrepair!", FALSE, ROOM_PEOPLE(IN_ROOM(veh)), NULL, veh, TO_CHAR | TO_ROOM);
		}
		fully_empty_vehicle(veh);
		extract_vehicle(veh);
	}
}


/**
* This runs once a mud year to update the world.
*/
void annual_world_update(void) {
	void check_ruined_cities();
	
	char message[MAX_STRING_LENGTH];
	vehicle_data *veh, *next_veh;
	descriptor_data *d;
	room_data *room, *next_room;
	struct map_data *tile;
	int newb_count = 0;
	
	bool naturalize_newbie_islands = config_get_bool("naturalize_newbie_islands");
	bool naturalize_unclaimable = config_get_bool("naturalize_unclaimable");
	
	snprintf(message, sizeof(message), "\r\n%s\r\n", config_get_string("newyear_message"));
	
	// MESSAGE TO ALL
	for (d = descriptor_list; d; d = d->next) {
		if (STATE(d) == CON_PLAYING && d->character) {
			write_to_descriptor(d->descriptor, message);
			d->has_prompt = FALSE;
			
			if (!IS_IMMORTAL(d->character)) {
				if (GET_POS(d->character) > POS_SITTING && !number(0, GET_DEXTERITY(d->character))) {
					if (IS_RIDING(d->character)) {
						perform_dismount(d->character);
					}
					write_to_descriptor(d->descriptor, "You're knocked to the ground!\r\n");
					act("$n is knocked to the ground!", TRUE, d->character, NULL, NULL, TO_ROOM);
					GET_POS(d->character) = POS_SITTING;
				}
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
	LL_FOREACH_SAFE2(interior_room_list, room, next_room, next_interior) {
		annual_update_depletions(&ROOM_DEPLETION(room));
	}
	
	LL_FOREACH_SAFE(vehicle_list, veh, next_veh) {
		annual_update_vehicle(veh);
	}
	
	// crumble cities that lost their buildings
	check_ruined_cities();
	
	// rename islands
	update_island_names();
	save_whole_world();
	
	// store the time now
	data_set_long(DATA_LAST_NEW_YEAR, time(0));
	
	if (newb_count) {
		log("New year: naturalized %d tile%s on newbie islands.", newb_count, PLURAL(newb_count));
		world_map_needs_save = TRUE;
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
	if ((room = real_real_room(tile->vnum))) {
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
		change_terrain(room, GET_SECT_VNUM(tile->natural_sector));
		if (ROOM_PEOPLE(room)) {
			act("The area returns to nature!", FALSE, ROOM_PEOPLE(room), NULL, NULL, TO_CHAR | TO_ROOM);
		}
	}
	else {
		perform_change_sect(NULL, tile, tile->natural_sector);
		perform_change_base_sect(NULL, tile, tile->natural_sector);
		
		if (SECT_FLAGGED(tile->natural_sector, SECTF_HAS_CROP_DATA)) {
			room = real_room(tile->vnum);	// need it loaded after all
			set_crop_type(room, get_potential_crop_for_location(room));
		}
		else {
			tile->crop_type = NULL;
		}
	}
	
	return 1;
}


/**
* Checks periodically to see if an empire is the only one left on an island,
* and renames the island if they have a custom name for it.
*/
void update_island_names(void) {
	void save_island_table();
	
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
		count = 0;
		HASH_ITER(hh, empire_table, emp, next_emp) {
			LL_FOREACH(EMPIRE_CITY_LIST(emp), city) {
				if (GET_ISLAND_ID(city->location) == isle->id) {
					eisle = get_empire_island(emp, isle->id);
					
					// if the empire HAS named the island
					if (eisle->name) {
						if (!last_name || !str_cmp(eisle->name, last_name)) {
							++count;	// only count in this case
							found_emp = emp;	// found an empire with a name
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
	void death_log(char_data *ch, char_data *killer, int type);
	extern obj_data *die(char_data *ch, char_data *killer);
	
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
		LL_FOREACH_SAFE2(ROOM_PEOPLE(room), ch, next_ch, next_in_room) {
			if (!IS_NPC(ch)) {
				death_log(ch, ch, TYPE_SUFFERING);
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
	LL_FOREACH_SAFE2(ROOM_CONTENTS(room), obj, next_obj, next_content) {
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
	struct event *ev;
	
	if (COMPLEX_DATA(room) && COMPLEX_DATA(room)->burn_down_time > 0) {
		// create the event
		CREATE(burn_data, struct room_event_data, 1);
		burn_data->room = room;
		
		ev = event_create(burn_down_event, burn_data, (COMPLEX_DATA(room)->burn_down_time - time(0)) * PASSES_PER_SEC);
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
	room = HOME_ROOM(room);	// burning is always on the home room
	
	if (!COMPLEX_DATA(room)) {
		return;	// nothing to burn
	}
	
	set_room_extra_data(room, ROOM_EXTRA_FIRE_REMAINING, config_get_int("fire_extinguish_value"));
	COMPLEX_DATA(room)->burn_down_time = time(0) + config_get_int("burn_down_time");
	schedule_burn_down(room);
	
	// ensure no building or dismantling
	stop_room_action(room, ACT_BUILDING, CHORE_BUILDING);
	stop_room_action(room, ACT_DISMANTLING, CHORE_BUILDING);
}


/**
* Turns the fire off.
*
* @param room_data *room The room that might be burning.
*/
void stop_burning(room_data *room) {
	room = HOME_ROOM(room);	// always
	
	if (COMPLEX_DATA(room)) {
		COMPLEX_DATA(room)->burn_down_time = 0;
		cancel_stored_event_room(room, SEV_BURN_DOWN);
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
		points = 1;
		points += (EMPIRE_HAS_TECH(emp, TECH_PROMINENCE) ? 1 : 0);
		points += ((EMPIRE_MEMBERS(emp) - 1) / config_get_int("players_per_city_point"));
		points += (GET_TOTAL_WEALTH(emp) >= config_get_int("bonus_city_point_wealth")) ? 1 : 0;
		points += (count_tech(emp) >= config_get_int("bonus_city_point_techs")) ? 1 : 0;

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
	struct empire_city_data *city, *cc;
	
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

	city->population = 0;
	city->military = 0;
	
	city->traits = EMPIRE_FRONTIER_TRAITS(emp);	// defaults

	city->next = NULL;
	
	if ((cc = EMPIRE_CITY_LIST(emp))) {
		// append to end
		while (cc->next) {
			cc = cc->next;
		}
		cc->next = city;
	}
	else {
		EMPIRE_CITY_LIST(emp) = city;
	}
	
	// check building exists
	if (!IS_CITY_CENTER(location)) {
		construct_building(location, BUILDING_CITY_CENTER);
		set_room_extra_data(location, ROOM_EXTRA_FOUND_TIME, time(0));
		complete_building(location);
	}
	
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
	void add_convert_vehicle_data(char_data *mob, any_vnum vnum);
	void setup_generic_npc(char_data *mob, empire_data *emp, int name, int sex);
	void objpack_load_room(room_data *room);
	
	struct reset_com *reset;
	char_data *tmob = NULL; /* for trigger assignment */
	char_data *mob = NULL;
	trig_data *trig;
	
	// shortcut
	if (!room->reset_commands) {
		return;
	}
	
	// start loading
	for (reset = room->reset_commands; reset; reset = reset->next) {
		switch (reset->command) {
			case 'M': {	// read a mobile
				mob = read_mobile(reset->arg1, FALSE);	// no scripts
				MOB_FLAGS(mob) = asciiflag_conv(reset->sarg1);
				char_to_room(mob, room);
				
				// sanity
				SET_BIT(MOB_FLAGS(mob), MOB_ISNPC);
				REMOVE_BIT(MOB_FLAGS(mob), MOB_EXTRACTED);
				
				// has old pulling data? attempt to convert
				if (reset->arg2 > 0) {
					add_convert_vehicle_data(mob, reset->arg2);
				}
				
				load_mtrigger(mob);
				tmob = mob;
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
						GET_REAL_SEX(mob) = reset->arg1;
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
				objpack_load_room(room);
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
					else {
						add_var(&(SCRIPT(tmob)->global_vars), reset->sarg1, reset->sarg2, reset->arg3);
					}
				}
				else if (reset->arg1 == WLD_TRIGGER) {
					if (!room->script) {
						create_script_data(room, WLD_TRIGGER);
					}
					else {
						add_var(&(room->script->global_vars), reset->sarg1, reset->sarg2, reset->arg3);
					}
				}
				break;
			}
		}
	}
	
	// remove resets
	while ((reset = room->reset_commands)) {
		room->reset_commands = reset->next;
		if (reset->sarg1) {
			free(reset->sarg1);
		}
		if (reset->sarg2) {
			free(reset->sarg2);
		}
		free(reset);
	}
	room->reset_commands = NULL;
}


/**
* Resets all rooms that have reset commands waiting.
*/
void startup_room_reset(void) {
	room_data *room, *next_room;

	HASH_ITER(hh, world_table, room, next_room) {
		if (room->reset_commands) {
			reset_one_room(room);
		}
	}
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
	if (loc || (loc = real_real_room(map->vnum))) {
		BASE_SECT(loc) = sect;
	}
	
	// update the world map
	if (map || (GET_ROOM_VNUM(loc) < MAP_SIZE && (map = &(world_map[FLAT_X_COORD(loc)][FLAT_Y_COORD(loc)])))) {
		map->base_sector = sect;
		world_map_needs_save = TRUE;
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
	bool was_large, was_in_city, junk;
	struct sector_index_type *idx;
	sector_data *old_sect;
	
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
		loc = real_real_room(map->vnum);
	}
	
	// for updating territory counts
	was_large = (loc && SECT(loc)) ? ROOM_SECT_FLAGGED(loc, SECTF_LARGE_CITY_RADIUS) : FALSE;
	was_in_city = (loc && ROOM_OWNER(loc)) ? is_in_city_for_empire(loc, ROOM_OWNER(loc), FALSE, &junk) : FALSE;
	
	// preserve
	old_sect = (loc ? SECT(loc) : map->sector_type);
	
	// update room
	if (loc) {
		SECT(loc) = sect;
	}
	
	// update the world map
	if (map || (GET_ROOM_VNUM(loc) < MAP_SIZE && (map = &(world_map[FLAT_X_COORD(loc)][FLAT_Y_COORD(loc)])))) {
		map->sector_type = sect;
		world_map_needs_save = TRUE;
	}
	
	if (old_sect && GET_SECT_VNUM(old_sect) == BASIC_OCEAN && GET_SECT_VNUM(sect) != BASIC_OCEAN && map->shared == &ocean_shared_data) {
		// converting from ocean to non-ocean
		map->shared = NULL;	// unlink ocean share
		CREATE(map->shared, struct shared_room_data, 1);
		
		map->shared->island_id = NO_ISLAND;	// it definitely was before, must still be
		map->shared->island_ptr = NULL;
		
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
			if (last_evo_tile == map) {
				last_evo_tile = map->next_in_sect;
			}
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
		if (was_large != ROOM_SECT_FLAGGED(loc, SECTF_LARGE_CITY_RADIUS)) {
			struct empire_island *eisle = get_empire_island(ROOM_OWNER(loc), GET_ISLAND_ID(loc));
			if (was_large && was_in_city && !is_in_city_for_empire(loc, ROOM_OWNER(loc), FALSE, &junk)) {
				// changing from in-city to not
				EMPIRE_CITY_TERRITORY(ROOM_OWNER(loc)) -= 1;
				eisle->city_terr -= 1;
				EMPIRE_OUTSIDE_TERRITORY(ROOM_OWNER(loc)) += 1;
				eisle->outside_terr += 1;
			}
			else if (ROOM_SECT_FLAGGED(loc, SECTF_LARGE_CITY_RADIUS) && !was_in_city && is_in_city_for_empire(loc, ROOM_OWNER(loc), FALSE, &junk)) {
				// changing from outside-territory to in-city
				EMPIRE_CITY_TERRITORY(ROOM_OWNER(loc)) += 1;
				eisle->city_terr -= 1;
				EMPIRE_OUTSIDE_TERRITORY(ROOM_OWNER(loc)) -= 1;
				eisle->outside_terr += 1;
			}
			else {
				// no relevant change
			}
		}
		if (belongs != BELONGS_IN_TERRITORY_LIST(loc)) {	// do we need to add/remove the territory entry?
			if (belongs && (ter = find_territory_entry(ROOM_OWNER(loc), loc))) {	// losing
				delete_territory_entry(ROOM_OWNER(loc), ter);
			}
			else if (!belongs && !find_territory_entry(ROOM_OWNER(loc), loc)) {	// adding
				create_territory_entry(ROOM_OWNER(loc), loc);
			}
		}
	}
}


 //////////////////////////////////////////////////////////////////////////////
//// TAVERNS /////////////////////////////////////////////////////////////////

EVENTFUNC(tavern_update) {
	extern bool extract_tavern_resources(room_data *room);
	
	struct room_event_data *data = (struct room_event_data *)event_obj;
	room_data *room;
	
	// grab data but don't free (we usually reschedule this)
	room = data->room;
	
	// still a tavern?
	if (!ROOM_OWNER(room) || !room_has_function_and_city_ok(room, FNC_TAVERN)) {
		// remove data and cancel event
		remove_room_extra_data(room, ROOM_EXTRA_TAVERN_TYPE);
		remove_room_extra_data(room, ROOM_EXTRA_TAVERN_BREWING_TIME);
		remove_room_extra_data(room, ROOM_EXTRA_TAVERN_AVAILABLE_TIME);
		free(data);
		return 0;
	}
	
	if (get_room_extra_data(room, ROOM_EXTRA_TAVERN_BREWING_TIME) > 0) {
		add_to_room_extra_data(room, ROOM_EXTRA_TAVERN_BREWING_TIME, -1);
		if (get_room_extra_data(room, ROOM_EXTRA_TAVERN_BREWING_TIME) == 0) {
			// brew's ready!
			set_room_extra_data(room, ROOM_EXTRA_TAVERN_AVAILABLE_TIME, config_get_int("tavern_timer"));
		}
	}
	else if (get_room_extra_data(room, ROOM_EXTRA_TAVERN_AVAILABLE_TIME) >= 0) {
		// count down to re-brew
		add_to_room_extra_data(room, ROOM_EXTRA_TAVERN_AVAILABLE_TIME, -1);
		
		if (get_room_extra_data(room, ROOM_EXTRA_TAVERN_AVAILABLE_TIME) <= 0) {
			// enough to go again?
			if (extract_tavern_resources(room)) {
				set_room_extra_data(room, ROOM_EXTRA_TAVERN_AVAILABLE_TIME, config_get_int("tavern_timer"));
			}
			else {
				// can't afford to keep brewing
				remove_room_extra_data(room, ROOM_EXTRA_TAVERN_TYPE);
				remove_room_extra_data(room, ROOM_EXTRA_TAVERN_BREWING_TIME);
				remove_room_extra_data(room, ROOM_EXTRA_TAVERN_AVAILABLE_TIME);
				
				// free up data and cancel the event (it'll be rescheduled if they set a brew
				free(data);
				return 0;
			}
		}
	}

	return (7.5 * 60) RL_SEC;	// reenqueue for the original time
}


/**
* Checks if a building is a tavern and can run. If so, sets up the data for
* it and schedules the event. If not, it clears that data.
*
* @param room_data *room The room to check for tavernness.
*/
void check_tavern_setup(room_data *room) {
	struct room_event_data *data;
	struct event *ev;
	
	if (!ROOM_OWNER(room) || !room_has_function_and_city_ok(room, FNC_TAVERN)) {
		// not a tavern or not set up
		remove_room_extra_data(room, ROOM_EXTRA_TAVERN_BREWING_TIME);
		remove_room_extra_data(room, ROOM_EXTRA_TAVERN_TYPE);
		remove_room_extra_data(room, ROOM_EXTRA_TAVERN_AVAILABLE_TIME);
		cancel_stored_event_room(room, SEV_TAVERN);
		return;
	}
	
	// otherwise, it IS a tavern and we'll just let the event function handle it
	if (!find_stored_event_room(room, SEV_TAVERN)) {
		CREATE(data, struct room_event_data, 1);
		data->room = room;
		
		// schedule every 7.5 minutes -- the same frequency this ran under the old system
		ev = event_create(tavern_update, (void*)data, (7.5 * 60) RL_SEC);
		add_stored_event_room(room, SEV_TAVERN, ev);
	}
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
	struct empire_island *isle = NULL;
	int amt = add ? 1 : -1;	// adding or removing 1
	int island;
	
	// only care about buildings
	if (!emp || !GET_BUILDING(room) || IS_INCOMPLETE(room) || IS_DISMANTLING(room)) {
		return;
	}
	
	// must be on an island
	if ((island = GET_ISLAND_ID(room)) == NO_ISLAND) {
		return;
	}
	
	// WARNING: do not check in-city status on these ... it can change at a time when territory is not re-scanned
	
	if (HAS_FUNCTION(room, FNC_APIARY)) {
		EMPIRE_TECH(emp, TECH_APIARIES) += amt;
		if (isle || (isle = get_empire_island(emp, island))) {
			isle->tech[TECH_APIARIES] += amt;
		}
	}
	if (HAS_FUNCTION(room, FNC_GLASSBLOWER)) {
		EMPIRE_TECH(emp, TECH_GLASSBLOWING) += amt;
		if (isle || (isle = get_empire_island(emp, island))) {
			isle->tech[TECH_GLASSBLOWING] += amt;
		}
	}
	if (HAS_FUNCTION(room, FNC_DOCKS)) {
		EMPIRE_TECH(emp, TECH_SEAPORT) += amt;
		if (isle || (isle = get_empire_island(emp, island))) {
			isle->tech[TECH_SEAPORT] += amt;
		}
	}
	
	// other traits from buildings?
	EMPIRE_MILITARY(emp) += GET_BLD_MILITARY(GET_BUILDING(room)) * amt;
	EMPIRE_FAME(emp) += GET_BLD_FAME(GET_BUILDING(room)) * amt;
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
			for (sub = 0; sub < NUM_TECHS; ++sub) {
				isle->population = 0;
				isle->tech[sub] = 0;
			}
		}
		// main techs
		for (sub = 0; sub < NUM_TECHS; ++sub) {
			EMPIRE_TECH(iter, sub) = 0;
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
	
	CREATE(ter, struct empire_territory_data, 1);
	ter->room = room;
	ter->population_timer = config_get_int("building_population_timer");
	ter->npcs = NULL;
	ter->marked = FALSE;
	
	// put it at the end
	LL_APPEND(EMPIRE_TERRITORY_LIST(emp), ter);
	
	return ter;
}


/**
* Deletes one territory entry from the empire.
*
* @param empire_data *emp Which empire.
* @param struct empire_territory_data *ter Which entry to delete.
*/
void delete_territory_entry(empire_data *emp, struct empire_territory_data *ter) {
	void delete_room_npcs(room_data *room, struct empire_territory_data *ter);
	extern struct empire_territory_data *global_next_territory_entry;
	
	// prevent loss
	if (ter == global_next_territory_entry) {
		global_next_territory_entry = ter->next;
	}

	delete_room_npcs(NULL, ter);
	ter->npcs = NULL;
	
	LL_DELETE(EMPIRE_TERRITORY_LIST(emp), ter);
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
	void read_vault(empire_data *emp);
	
	struct empire_territory_data *ter, *next_ter;
	struct empire_island *isle, *next_isle;
	struct empire_npc_data *npc;
	room_data *iter, *next_iter;
	empire_data *e, *next_e;
	bool junk;

	/* Init empires */
	HASH_ITER(hh, empire_table, e, next_e) {
		if (e == emp || !emp) {
			EMPIRE_CITY_TERRITORY(e) = 0;
			EMPIRE_OUTSIDE_TERRITORY(e) = 0;
			EMPIRE_POPULATION(e) = 0;
			
			if (check_tech) {	// this will only be re-read if we check tech
				EMPIRE_MILITARY(e) = 0;
				EMPIRE_FAME(e) = 0;
			}
		
			read_vault(e);

			// reset marks to check for dead territory
			for (ter = EMPIRE_TERRITORY_LIST(e); ter; ter = ter->next) {
				ter->marked = FALSE;
			}
			
			// reset counters
			HASH_ITER(hh, EMPIRE_ISLANDS(e), isle, next_isle) {
				isle->population = 0;
				isle->city_terr = 0;
				isle->outside_terr = 0;
			}
		}
	}

	// scan the whole world
	HASH_ITER(hh, world_table, iter, next_iter) {
		if ((e = ROOM_OWNER(iter)) && (!emp || e == emp)) {
			// only count each building as 1
			if (COUNTS_AS_TERRITORY(iter)) {
				isle = get_empire_island(e, GET_ISLAND_ID(iter));
				if (is_in_city_for_empire(iter, e, FALSE, &junk)) {
					EMPIRE_CITY_TERRITORY(e) += 1;
					isle->city_terr += 1;
				}
				else {
					EMPIRE_OUTSIDE_TERRITORY(e) += 1;
					isle->outside_terr += 1;
				}
			}
			
			// this is only done if we are re-reading techs
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
						if (!GET_ROOM_VEHICLE(iter)) {
							isle->population += 1;
						}
					}
				}
			}
		}
	}
	
	// remove any territory that wasn't marked ... in case there is any
	HASH_ITER(hh, empire_table, e, next_e) {
		if (e == emp || !emp) {
			LL_FOREACH_SAFE(EMPIRE_TERRITORY_LIST(e), ter, next_ter) {
				if (!ter->marked) {
					delete_territory_entry(e, ter);
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
	void resort_empires(bool force);
	
	struct empire_island *isle, *next_isle;
	empire_data *iter, *next_iter;
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
}


 //////////////////////////////////////////////////////////////////////////////
//// TRENCH FILLING //////////////////////////////////////////////////////////

// fills trench when complete
EVENTFUNC(trench_fill_event) {
	void fill_trench(room_data *room);
	
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
	struct event *ev;
	
	if (!find_stored_event(map->shared->events, SEV_TRENCH_FILL)) {
		CREATE(trench_data, struct map_event_data, 1);
		trench_data->map = map;
		
		ev = event_create(trench_fill_event, (void*)trench_data, (when > 0 ? ((when - time(0)) * PASSES_PER_SEC) : 1));
		add_stored_event(&map->shared->events, SEV_TRENCH_FILL, ev);
	}
}


 //////////////////////////////////////////////////////////////////////////////
//// EVOLUTIONS //////////////////////////////////////////////////////////////

/**
* This code handles the massive evolution of map tiles, with the goal of load-
* balancing it over the number of game hours.
*/


/**
* Determines whether a sector periodicaly evolves at all.
*
* @param sector_data *sect Which sector.
* @return bool TRUE if at least one over-time evo, FALSE if none.
*/
inline bool has_over_time_evo(sector_data *sect) {
	extern bool evo_is_over_time[];
	struct evolution_data *evo;
	LL_FOREACH(GET_SECT_EVOS(sect), evo) {
		if (evo_is_over_time[evo->type]) {
			return TRUE;
		}
	}
	return FALSE;
}


/**
* Updates the number of evolutions to do per hour. This is run at startup,
* and is re-run periodically to ensure it's doing the right number. This func
* should be called any time there are LARGE changes to the world.
*/
void detect_evos_per_hour(void) {
	sector_data *sect, *next_sect;
	struct sector_index_type *idx;
	int total_evolvable_tiles = 0;
	
	HASH_ITER(hh, sector_table, sect, next_sect) {
		if (!has_over_time_evo(sect)) {
			continue;
		}
		idx = find_sector_index(GET_SECT_VNUM(sect));
		total_evolvable_tiles += idx->sect_count;
	}
	
	evos_per_hour = total_evolvable_tiles / 24;
	evos_per_hour = MAX(1, evos_per_hour);	// safe limit
}


/**
* Checks and runs evolutions for a single map tile.
*
* @param struct map_data *tile The map location to evolve.
*/
static void evolve_one_map_tile(struct map_data *tile) {
	extern bool is_entrance(room_data *room);
	
	struct evolution_data *evo;
	sector_data *original;
	sector_vnum become;
	room_data *room;
	
	// this may return NULL -- we don't need it if so
	room = real_real_room(tile->vnum);
	
	// no further action if !evolve or if no evos
	if ((room && ROOM_AFF_FLAGGED(room, ROOM_AFF_NO_EVOLVE)) || !GET_SECT_EVOS(tile->sector_type)) {
		return;
	}
	
	// to avoid running more than one:
	original = tile->sector_type;
	become = NOTHING;
	
	// run some evolutions!
	if (become == NOTHING && (evo = get_evolution_by_type(tile->sector_type, EVO_RANDOM))) {
		become = evo->becomes;
	}
	
	if (become == NOTHING && (evo = get_evolution_by_type(tile->sector_type, EVO_ADJACENT_ONE))) {
		room = room ? room : real_room(tile->vnum);
		if (count_adjacent_sectors(room, evo->value, TRUE) >= 1) {
			become = evo->becomes;
		}
	}
	
	if (become == NOTHING && (evo = get_evolution_by_type(tile->sector_type, EVO_NOT_ADJACENT))) {
		room = room ? room : real_room(tile->vnum);
		if (count_adjacent_sectors(room, evo->value, TRUE) < 1) {
			become = evo->becomes;
		}
	}
	
	if (become == NOTHING && (evo = get_evolution_by_type(tile->sector_type, EVO_ADJACENT_MANY))) {
		room = room ? room : real_room(tile->vnum);
		if (count_adjacent_sectors(room, evo->value, TRUE) >= 6) {
			become = evo->becomes;
		}
	}
	
	if (become == NOTHING && (evo = get_evolution_by_type(tile->sector_type, EVO_NEAR_SECTOR))) {
		room = room ? room : real_room(tile->vnum);
		if (find_sect_within_distance_from_room(room, evo->value, config_get_int("nearby_sector_distance"))) {
			become = evo->becomes;
		}
	}
	
	if (become == NOTHING && (evo = get_evolution_by_type(tile->sector_type, EVO_NOT_NEAR_SECTOR))) {
		room = room ? room : real_room(tile->vnum);
		if (!find_sect_within_distance_from_room(room, evo->value, config_get_int("nearby_sector_distance"))) {
			become = evo->becomes;
		}
	}
	
	// DONE: now change it
	if (become != NOTHING && sector_proto(become)) {
		// in case we didn't get it earlier
		if (!room) {
			room = real_room(tile->vnum);
		}
		
	 	if (room && !is_entrance(room)) {
			change_terrain(room, become);
			
			// If the new sector has crop data, we should store the original (e.g. a desert that randomly grows into a crop)
			if (ROOM_SECT_FLAGGED(room, SECTF_HAS_CROP_DATA) && BASE_SECT(room) == SECT(room)) {
				change_base_sector(room, original);
			}
			
			if (ROOM_OWNER(room)) {
				void deactivate_workforce_room(empire_data *emp, room_data *room);
				deactivate_workforce_room(ROOM_OWNER(room), room);
			}
		}
	}
}


/**
* Runs evolutions on 1/24 of evolvable map tiles per hour.
*/
void run_map_evolutions(void) {
	struct map_data *map, *next_map, *map_start;
	struct sector_index_type *idx;
	sector_data *sect, *next_sect;
	bool found_start;
	int try, to_do;
	
	to_do = evos_per_hour;	// how many tiles to evolve before we quit
	
	// going to loop through sectors twice: once to find the last starting pos, and a second time if we have to wrap around
	found_start = FALSE;
	for (try = 1; try <= 2 && to_do > 0; ++try) {
		HASH_ITER(hh, sector_table, sect, next_sect) {
			// early exit
			if (to_do <= 0) {
				break;
			}
			
			// finding where we left off
			if (try == 1 && sect == last_evo_sect) {
				// mark that we found the sector to start on
				found_start = TRUE;
				
				// we will skip this sector if there's no work to be done in it
				if (last_evo_tile == NULL) {
					continue;
				}
			}
			// otherwise if it's try 1, skip sectors BEFORE the start sector
			if (try == 1 && !found_start) {
				continue;
			}
			
			// other basic validation
			if (!SECT_IS_LAND_MAP(sect)) {
				continue;	// always skip
			}
			if (!has_over_time_evo(sect)) {
				continue;	// never evolves
			}
			
			// READY: figure out where to start
			if (last_evo_tile && last_evo_tile->next_in_sect && last_evo_tile->sector_type == sect) {
				map_start = last_evo_tile->next_in_sect;
			}
			else {
				idx = find_sector_index(GET_SECT_VNUM(sect));
				map_start = idx->sect_rooms;
			}
			
			// update this now, just in case
			last_evo_sect = sect;
			
			// now attempt to evolve rooms in the list
			LL_FOREACH_SAFE2(map_start, map, next_map, next_in_sect) {
				last_evo_tile = map;	// update this NOW
				
				evolve_one_map_tile(map);
				
				// end if done
				if (--to_do <= 0) {
					break;
				}
			}
		}
	}
}


 //////////////////////////////////////////////////////////////////////////////
//// HELPERS /////////////////////////////////////////////////////////////////

/**
* Validates all exits in the game, especially after a bunch of rooms have been
* deleted.
*/
void check_all_exits(void) {
	struct room_direction_data *ex, *next_ex, *temp;
	room_data *room, *next_room;
	obj_data *o, *next_o;
	
	// search world for portals that link to bad rooms
	for (o = object_list; o; o = next_o) {
		next_o = o->next;
		
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
					REMOVE_FROM_LIST(ex, COMPLEX_DATA(room)->exits, next);
					free(ex);
					
					// no need to update GET_EXITS_HERE() as the target room is gone
				}
			}	
		}
	}
}


// checks if the room can be unloaded, and does it
EVENTFUNC(check_unload_room) {
	struct room_event_data *data = (struct room_event_data *)event_obj;
	room_data *room;
	
	// grab data but don't free
	room = data->room;
	
	if (CAN_UNLOAD_MAP_ROOM(room)) {
		free(data);
		delete_stored_event_room(room, SEV_CHECK_UNLOAD);	// delete first so it doesn't free/cancel
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
	void remove_designate_objects(room_data *room);
	room_data *iter, *next_iter;
	obj_data *obj;
	
	HASH_ITER(hh, world_table, iter, next_iter) {
		if (COMPLEX_DATA(iter) && ROOM_PRIVATE_OWNER(iter) == id) {
			COMPLEX_DATA(iter)->private_owner = NOBODY;
		
			// TODO some way to generalize this, please
			if (BUILDING_VNUM(iter) == RTYPE_BEDROOM) {
				remove_designate_objects(iter);
			}
			
			// reset autostore timer
			LL_FOREACH2(ROOM_CONTENTS(iter), obj, next_content) {
				GET_AUTOSTORE_TIMER(obj) = time(0);
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
* Creates a blank map room from the world_map data. This allows parts of the
* map to be unloaded using CAN_UNLOAD_MAP_ROOM(); this function builds new
* rooms as-needed.
*
* @param room_vnum vnum The vnum of the map room to load.
* @return room_data* A fresh map room.
*/
room_data *load_map_room(room_vnum vnum) {
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
	add_room_to_world_tables(room);
	
	// do not use perform_change_sect here because we're only loading from the existing data
	SECT(room) = map->sector_type;
	BASE_SECT(room) = map->base_sector;
	
	ROOM_CROP(room) = map->crop_type;
	
	// only if saveable
	if (!CAN_UNLOAD_MAP_ROOM(room)) {
		need_world_index = TRUE;
	}
	
	// checks if it's unloadable, and unloads it
	schedule_check_unload(room, FALSE);
	
	return room;
}


/**
* Removes the custom name/icon/description on rooms
*
* @param room_data *room
*/
void decustomize_room(room_data *room) {
	if (ROOM_CUSTOM_NAME(room)) {
		free(ROOM_CUSTOM_NAME(room));
		ROOM_CUSTOM_NAME(room) = NULL;
	}
	if (ROOM_CUSTOM_DESCRIPTION(room)) {
		free(ROOM_CUSTOM_DESCRIPTION(room));
		ROOM_CUSTOM_DESCRIPTION(room) = NULL;
	}
	if (ROOM_CUSTOM_ICON(room)) {
		free(ROOM_CUSTOM_ICON(room));
		ROOM_CUSTOM_ICON(room) = NULL;
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
* @return int The "main island" for the empire.
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
	
	// sad: this is a default so that it goes SOMEWHERE (an imm can move it)
	return 0;
}


/**
* This determines what kind of crop should grow in a given location, mainly
* for the purposes of spawning fresh crops. This is only an approximation.
* 
* @param room_data *location The location to pick a crop for.
* @return crop_data* Any crop.
*/
crop_data *get_potential_crop_for_location(room_data *location) {
	int x = X_COORD(location), y = Y_COORD(location);
	bool water = find_flagged_sect_within_distance_from_room(location, SECTF_FRESH_WATER, NOBITS, config_get_int("water_crop_distance"));
	bool x_min_ok, x_max_ok, y_min_ok, y_max_ok;
	struct island_info *isle = NULL;
	int climate;
	crop_data *found, *crop, *next_crop;
	int num_found = 0;
	
	// small amount of random so the edges of the crops are less linear on the map
	x += number(-10, 10);
	y += number(-10, 10);
	
	// bounds checks
	x = WRAP_X_COORD(x);
	y = WRAP_Y_COORD(y);
	
	// determine climate
	climate = GET_SECT_CLIMATE(SECT(location));
	
	// don't allow NONE climates in here
	if (climate == CLIMATE_NONE) {
		climate = CLIMATE_TEMPERATE;
	}
	
	// find any match
	found = NULL;
	HASH_ITER(hh, crop_table, crop, next_crop) {
		// basic checks
		if (GET_CROP_CLIMATE(crop) != climate || CROP_FLAGGED(crop, CROPF_NOT_WILD)) {
			continue;
		}
		if (CROP_FLAGGED(crop, CROPF_REQUIRES_WATER) && !water) {
			continue;
		}
		if (CROP_FLAGGED(crop, CROPF_NO_NEWBIE | CROPF_NEWBIE_ONLY)) {
			if (!isle) {
				isle = GET_ISLAND(location);
			}
			if (CROP_FLAGGED(crop, CROPF_NO_NEWBIE) && IS_SET(isle->flags, ISLE_NEWBIE)) {
				continue;
			}
			if (CROP_FLAGGED(crop, CROPF_NEWBIE_ONLY) && !IS_SET(isle->flags, ISLE_NEWBIE)) {
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
	
	if (!found) {
		// not found? possibly just bad configs -- default to first in table
		log("SYSERR: get_potential_crop_for_location unable to determine a crop for #%d", GET_ROOM_VNUM(location));
		found = crop_table;
	}
	
	return found;
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
	if (!SECT_FLAGGED(map->sector_type, SECTF_HAS_CROP_DATA) || !(evo = get_evolution_by_type(map->sector_type, EVO_CROP_GROWS)) || !(becomes = sector_proto(evo->becomes))) {
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
	room->light = 0;
		
	room->af = NULL;
	
	room->last_spawn_time = 0;

	room->contents = NULL;
	room->people = NULL;
}


/**
* Replaces a building with ruins.
*
* @param room_data *room The location of the building.
*/
void ruin_one_building(room_data *room) {
	bool closed = ROOM_IS_CLOSED(room) ? TRUE : FALSE;
	bld_data *bld = GET_BUILDING(room);
	int dir = BUILDING_ENTRANCE(room);
	char buf[MAX_STRING_LENGTH];
	room_data *to_room;
	bld_vnum type;
	
	// abandon first -- this will take care of accessory rooms, too
	abandon_room(room);
	disassociate_building(room);
	
	if (ROOM_PEOPLE(room)) {
		act("The building around you crumbles to ruin!", FALSE, ROOM_PEOPLE(room), NULL, NULL, TO_CHAR | TO_ROOM);
	}
	
	// create ruins building
	if (bld && !IS_SET(GET_BLD_FLAGS(bld), BLD_NO_RUINS)) {
		// verify closed status and find a room to exit to
		if (closed) {
			to_room = SHIFT_DIR(room, rev_dir[dir]);
			if (!to_room) {
				closed = FALSE;
			}
		}

		// basic setup
		if (SECT_FLAGGED(BASE_SECT(room), SECTF_FRESH_WATER | SECTF_OCEAN)) {
			type = BUILDING_RUINS_FLOODED;
		}
		else if (closed) {
			type = BUILDING_RUINS_CLOSED;
		}
		else {
			type = BUILDING_RUINS_OPEN;
		}
		construct_building(room, type);
		COMPLEX_DATA(room)->entrance = dir;
	
		// make the exit
		if (closed && to_room) {
			create_exit(room, to_room, rev_dir[dir], FALSE);
		}
		
		// customized ruins
		sprintf(buf, "The Ruins of %s %s", AN(GET_BLD_NAME(bld)), GET_BLD_NAME(bld));
		if (ROOM_CUSTOM_NAME(room)) {
			free(ROOM_CUSTOM_NAME(room));
		}
		ROOM_CUSTOM_NAME(room) = str_dup(buf);
		set_room_extra_data(room, ROOM_EXTRA_RUINS_ICON, number(0, NUM_RUINS_ICONS-1));
		
		// run completion on the ruins
		complete_building(room);
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
	struct event *ev;
	double mins;
	
	if (!find_stored_event_room(room, SEV_CHECK_UNLOAD)) {
		CREATE(data, struct room_event_data, 1);
		data->room = room;
		
		mins = 5;
		if (offset) {
			mins += number(-300, 300) / 100.0;
		}
		ev = event_create(check_unload_room, (void*)data, (mins * 60) RL_SEC);
		add_stored_event_room(room, SEV_CHECK_UNLOAD, ev);
	}
}


/**
* Schedules the crop growth.
*
* @param struct map_data *map The map location to setup growth for.
*/
void schedule_crop_growth(struct map_data *map) {
	struct map_event_data *data;
	struct event *ev;
	long when;
	
	// cancel any existing event
	cancel_stored_event(&map->shared->events, SEV_GROW_CROP);
	
	when = get_extra_data(map->shared->extra_data, ROOM_EXTRA_SEED_TIME);
	
	if (SECT_FLAGGED(map->sector_type, SECTF_HAS_CROP_DATA) && when > 0) {
		// create the event
		CREATE(data, struct map_event_data, 1);
		data->map = map;
		
		ev = event_create(grow_crop_event, data, (when - time(0)) * PASSES_PER_SEC);
		add_stored_event(&map->shared->events, SEV_GROW_CROP, ev);
	}
}


// Simple sorter for empire islands
int sort_empire_islands(struct empire_island *a, struct empire_island *b) {
	return a->island - b->island;
}


 //////////////////////////////////////////////////////////////////////////////
//// MAP OUTPUT //////////////////////////////////////////////////////////////

/**
* Writes the data files used to generate graphical maps.
*/
void output_map_to_file(void) {
	extern const char banner_to_mapout_token[][2];
	extern const char mapout_color_tokens[];
	
	FILE *out, *pol, *cit;
	int num, color = 0, x, y;
	struct empire_city_data *city;
	room_data *room;
	empire_data *emp, *next_emp;
	sector_data *ocean = sector_proto(BASIC_OCEAN);
	sector_data *sect;
	
	// basic ocean sector is required
	if (!ocean) {
		log("SYSERR: Basic ocean sector %d is missing", BASIC_OCEAN);
		return;
	}
	
	sort_world_table();

	// NORMAL MAP
	if (!(out = fopen(GEOGRAPHIC_MAP_FILE TEMP_SUFFIX, "w"))) {
		log("SYSERR: Unable to open file '%s' for writing", GEOGRAPHIC_MAP_FILE TEMP_SUFFIX);
		return;
	}
	
	// POLITICAL MAP
	if (!(pol = fopen(POLITICAL_MAP_FILE TEMP_SUFFIX, "w"))) {
		log("SYSERR: Unable to open file '%s' for writing", POLITICAL_MAP_FILE TEMP_SUFFIX);
		return;
	}
	
	fprintf(out, "%dx%d\n", MAP_WIDTH, MAP_HEIGHT);
	fprintf(pol, "%dx%d\n", MAP_WIDTH, MAP_HEIGHT);
	
	for (y = 0; y < MAP_HEIGHT; ++y) {
		for (x = 0; x < MAP_WIDTH; ++x) {
			// load room only if in memory
			room = real_real_room(world_map[x][y].vnum);
			sect = world_map[x][y].sector_type;
			if (room && ROOM_AFF_FLAGGED(room, ROOM_AFF_CHAMELEON) && IS_COMPLETE(room)) {
				sect = world_map[x][y].base_sector;
			}
			
			// normal map output
			if (SECT_FLAGGED(sect, SECTF_HAS_CROP_DATA) && world_map[x][y].crop_type) {
				fprintf(out, "%c", mapout_color_tokens[GET_CROP_MAPOUT(world_map[x][y].crop_type)]);
			}
			else {
				fprintf(out, "%c", mapout_color_tokens[GET_SECT_MAPOUT(sect)]);
			}
		
			// political output
			if (room && (emp = ROOM_OWNER(room)) && (!ROOM_AFF_FLAGGED(room, ROOM_AFF_CHAMELEON) || !IS_COMPLETE(room))) {
				// find the first color in banner_to_mapout_token that is in the banner
				color = -1;
				for (num = 0; banner_to_mapout_token[0][0] != '\n'; ++num) {
					if (strchr(EMPIRE_BANNER(emp), banner_to_mapout_token[num][0])) {
						color = num;
						break;
					}
				}
			
				fprintf(pol, "%c", color != -1 ? banner_to_mapout_token[color][1] : '?');
			}
			else {
				// no owner -- only some sects get printed
				if (SECT_FLAGGED(sect, SECTF_SHOW_ON_POLITICAL_MAPOUT)) {
					fprintf(pol, "%c", mapout_color_tokens[GET_SECT_MAPOUT(sect)]);
				}
				else {
					fprintf(pol, "?");
				}
			}
		}
		
		// end of row
		fprintf(out, "\n");
		fprintf(pol, "\n");	
	}

	fclose(out);
	rename(GEOGRAPHIC_MAP_FILE TEMP_SUFFIX, GEOGRAPHIC_MAP_FILE);
	fclose(pol);
	rename(POLITICAL_MAP_FILE TEMP_SUFFIX, POLITICAL_MAP_FILE);
	
	// and city data
	if (!(cit = fopen(CITY_DATA_FILE TEMP_SUFFIX, "w"))) {
		log("SYSERR: Unable to open file '%s' for writing", CITY_DATA_FILE TEMP_SUFFIX);
		return;
	}
	
	HASH_ITER(hh, empire_table, emp, next_emp) {
		if (EMPIRE_IMM_ONLY(emp)) {
			continue;
		}
		
		for (city = EMPIRE_CITY_LIST(emp); city; city = city->next) {
			fprintf(cit, "%d %d %d \"%s\" \"%s\"\n", X_COORD(city->location), Y_COORD(city->location), city_type[city->type].radius, city->name, EMPIRE_NAME(emp));
		}
	}
	
	fprintf(cit, "$\n");
	fclose(cit);
	rename(CITY_DATA_FILE TEMP_SUFFIX, CITY_DATA_FILE);
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
	LL_FOREACH2(interior_room_list, room, next_interior) {
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
		
		if (world_map[x][y].shared->island_id != GET_ISLAND_ID(room)) {
			world_map[x][y].shared->island_id = GET_ISLAND_ID(room);
			world_map[x][y].shared->island_ptr = (world_map[x][y].shared->island_id != NOTHING) ? get_island(world_map[x][y].shared->island_id, TRUE) : NULL;
		}
		
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
* This loads the world_map array from file. This is optional, and this data
* can be overwritten by the actual rooms from the .wld files. This should be
* run after sectors are loaded, and before the .wld files are read in.
*/
void load_world_map_from_file(void) {
	char line[256], line2[256], error_buf[MAX_STRING_LENGTH];
	struct map_data *map, *last = NULL;
	struct depletion_data *dep;
	struct track_data *track;
	int var[7], x, y;
	long l_in;
	FILE *fl;
	
	// initialize ocean_shared_data
	ocean_shared_data.island_id = NO_ISLAND;
	ocean_shared_data.island_ptr = NULL;
	ocean_shared_data.name = NULL;
	ocean_shared_data.description = NULL;
	ocean_shared_data.icon = NULL;
	ocean_shared_data.affects = NOBITS;
	ocean_shared_data.base_affects = NOBITS;
	ocean_shared_data.depletion = NULL;
	ocean_shared_data.extra_data = NULL;
	ocean_shared_data.tracks = NULL;
	
	// init
	land_map = NULL;
	for (x = 0; x < MAP_WIDTH; ++x) {
		for (y = 0; y < MAP_HEIGHT; ++y) {
			world_map[x][y].vnum = (y * MAP_WIDTH) + x;
			world_map[x][y].shared = &ocean_shared_data;
			world_map[x][y].shared->island_id = NO_ISLAND;
			world_map[x][y].shared->island_ptr = NULL;
			world_map[x][y].sector_type = NULL;
			world_map[x][y].base_sector = NULL;
			world_map[x][y].natural_sector = NULL;
			world_map[x][y].crop_type = NULL;
			world_map[x][y].next = NULL;
		}
	}
	
	if (!(fl = fopen(WORLD_MAP_FILE, "r"))) {
		log(" - no %s file, booting without one", WORLD_MAP_FILE);
		return;
	}
	
	strcpy(error_buf, "map file");
	
	// optionals
	while (get_line(fl, line)) {
		if (*line == '$') {
			break;
		}
		
		// new room
		if (isdigit(*line)) {
			// x y island sect base natural crop
			if (sscanf(line, "%d %d %d %d %d %d %d", &var[0], &var[1], &var[2], &var[3], &var[4], &var[5], &var[6]) != 7) {
				log("Encountered bad line in world map file: %s", line);
				continue;
			}
			if (var[0] < 0 || var[0] >= MAP_WIDTH || var[1] < 0 || var[1] >= MAP_HEIGHT) {
				log("Encountered bad location in world map file: (%d, %d)", var[0], var[1]);
				continue;
			}
		
			map = &(world_map[var[0]][var[1]]);
			sprintf(error_buf, "map tile %d", map->vnum);
			
			if (var[3] != BASIC_OCEAN) {
				map->shared = NULL;	// unlink basic ocean
				CREATE(map->shared, struct shared_room_data, 1);
			}
		
			map->shared->island_id = var[2];
		
			// these will be validated later
			map->sector_type = sector_proto(var[3]);
			map->base_sector = sector_proto(var[4]);
			map->natural_sector = sector_proto(var[5]);
			map->crop_type = crop_proto(var[6]);
			
			last = map;	// store in case of more data
		}
		else if (last) {
			switch (*line) {
				case 'E': {	// affects
					if (!get_line(fl, line2)) {
						log("SYSERR: Unable to get E line for map tile #%d", last->vnum);
						break;
					}

					last->shared->base_affects = asciiflag_conv(line2);
					last->shared->affects = last->shared->base_affects;
					break;
				}
				case 'I': {	// icon
					if (last->shared->icon) {
						free(last->shared->icon);
					}
					last->shared->icon = fread_string(fl, error_buf);
					break;
				}
				case 'M': {	// description
					if (last->shared->description) {
						free(last->shared->description);
					}
					last->shared->description = fread_string(fl, error_buf);
					break;
				}
				case 'N': {	// name
					if (last->shared->name) {
						free(last->shared->name);
					}
					last->shared->name = fread_string(fl, error_buf);
					break;
				}
				case 'X': {	// resource depletion
					if (!get_line(fl, line2)) {
						log("SYSERR: Unable to read depletion line of map tile #%d", last->vnum);
						exit(1);
					}
					if ((sscanf(line2, "%d %d", &var[0], &var[1])) != 2) {
						log("SYSERR: Format in depletion line of map tile #%d", last->vnum);
						exit(1);
					}
				
					CREATE(dep, struct depletion_data, 1);
					dep->type = var[0];
					dep->count = var[1];
					LL_PREPEND(last->shared->depletion, dep);
					break;
				}
				case 'Y': {	// tracks
					if (!get_line(fl, line2) || sscanf(line2, "%d %d %ld %d", &var[0], &var[1], &l_in, &var[2]) != 4) {
						log("SYSERR: Bad formatting in Y section of map tile #%d", last->vnum);
						exit(1);
					}
					
					CREATE(track, struct track_data, 1);
					track->player_id = var[0];
					track->mob_num = var[1];
					track->timestamp = l_in;
					track->dir = var[2];
					
					LL_PREPEND(last->shared->tracks, track);
					break;
				}
				case 'Z': {	// extra data
					if (!get_line(fl, line2) || sscanf(line2, "%d %d", &var[0], &var[1]) != 2) {
						log("SYSERR: Bad formatting in Z section of map tile #%d", last->vnum);
						exit(1);
					}
					
					set_extra_data(&last->shared->extra_data, var[0], var[1]);
					break;
				}
			}
		}
		else {
			log("Junk data found in base_map file: %s", line);
			exit(0);
		}
	}
	
	fclose(fl);
}


/**
* Outputs the land portion of the world map to the map file. This function also
* does a small amount of updating (e.g. removes stale tracks) since it is
* iterating the whole world anyway.
*/
void save_world_map_to_file(void) {
	void write_shared_room_data(FILE *fl, struct shared_room_data *dat);
	
	struct track_data *track, *next_track;
	struct map_data *iter;
	long now = time(0);
	FILE *fl;
	
	int tracks_lifespan = config_get_int("tracks_lifespan");
	
	// shortcut
	if (!world_map_needs_save) {
		return;
	}
	
	if (!(fl = fopen(WORLD_MAP_FILE TEMP_SUFFIX, "w"))) {
		log("Unable to open %s for writing", WORLD_MAP_FILE TEMP_SUFFIX);
		return;
	}
	
	// only bother with ones that aren't base ocean
	for (iter = land_map; iter; iter = iter->next) {
		// free some junk while we're here anyway
		LL_FOREACH_SAFE(iter->shared->tracks, track, next_track) {
			if (now - track->timestamp > tracks_lifespan * SECS_PER_REAL_MIN) {
				LL_DELETE(iter->shared->tracks, track);
				free(track);
			}
		}
		
		// SAVE: x y island sect base natural crop
		fprintf(fl, "%d %d %d %d %d %d %d\n", MAP_X_COORD(iter->vnum), MAP_Y_COORD(iter->vnum), iter->shared->island_id, (iter->sector_type ? GET_SECT_VNUM(iter->sector_type) : -1), (iter->base_sector ? GET_SECT_VNUM(iter->base_sector) : -1), (iter->natural_sector ? GET_SECT_VNUM(iter->natural_sector) : -1), (iter->crop_type ? GET_CROP_VNUM(iter->crop_type) : -1));
		write_shared_room_data(fl, iter->shared);
	}
	
	fclose(fl);
	rename(WORLD_MAP_FILE TEMP_SUFFIX, WORLD_MAP_FILE);
	world_map_needs_save = FALSE;
}
