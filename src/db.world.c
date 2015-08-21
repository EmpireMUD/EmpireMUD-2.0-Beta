/* ************************************************************************
*   File: db.world.c                                      EmpireMUD 2.0b2 *
*  Usage: Modify functions for the map, interior, and game world          *
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
#include "db.h"
#include "comm.h"
#include "handler.h"
#include "skills.h"
#include "dg_scripts.h"
#include "vnums.h"

/**
* Contents:
*   World-Changers
*   Management
*   Annual Map Update
*   City Lib
*   Ocean Pool
*   Room Resets
*   Territory
*   Helpers
*   Map Output
*/

// external vars
extern struct city_metadata_type city_type[];
extern const sector_vnum climate_default_sector[NUM_CLIMATES];
extern bool need_world_index;
extern const int rev_dir[];

// external funcs
void add_room_to_world_tables(room_data *room);
void delete_territory_entry(empire_data *emp, struct empire_territory_data *ter);
extern struct complex_room_data *init_complex_data();
void free_complex_data(struct complex_room_data *bld);
extern FILE *open_world_file(int block);
void remove_room_from_world_tables(room_data *room);
void save_and_close_world_file(FILE *fl, int block);
void setup_start_locations();
void sort_exits(struct room_direction_data **list);
void sort_world_table();
void write_room_to_file(FILE *fl, room_data *room);

// locals
int count_city_points_used(empire_data *emp);
room_data *create_ocean_room(room_vnum vnum);
void decustomize_room(room_data *room);
static void evolve_one_map_tile(room_data *room);
room_vnum find_free_vnum();
void init_room(room_data *room, room_vnum vnum);
void ruin_one_building(room_data *room);


 //////////////////////////////////////////////////////////////////////////////
//// WORLD-CHANGERS //////////////////////////////////////////////////////////


/**
* This changes the territory when a chop finishes, and returns the number of
* trees.
*
* @param room_data *room The room.
* @return int the number of trees received
*/
int change_chop_territory(room_data *room) {	
	struct evolution_data *evo;
	int trees = 1;
	crop_data *cp;
	
	if (ROOM_SECT_FLAGGED(room, SECTF_CROP) && ROOM_CROP_FLAGGED(room, CROPF_IS_ORCHARD) && (cp = crop_proto(ROOM_CROP_TYPE(room)))) {
		trees = 1;
		change_terrain(room, climate_default_sector[GET_CROP_CLIMATE(cp)]);
	}
	else if ((evo = get_evolution_by_type(SECT(room), EVO_CHOPPED_DOWN))) {
		trees = evo->value;
		change_terrain(room, evo->becomes);
	}
	else {
		trees = 0;
		log("SYSERR: change_chop_terrain called on room %d, sect vnum %d, but no valid chop rules found", GET_ROOM_VNUM(room), GET_SECT_VNUM(SECT(room)));
	}

	return trees;
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
	
	bool belongs = BELONGS_IN_TERRITORY_LIST(room);
	sector_data *old_sect = SECT(room), *st = sector_proto(sect);
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
	if (SECT_FLAGGED(st, SECTF_HAS_CROP_DATA)) {
		new_crop = get_potential_crop_for_location(room);
	}
	
	// change sect
	SECT(room) = st;
	ROOM_ORIGINAL_SECT(room) = st;
		
	// need room data?
	if ((IS_ANY_BUILDING(room) || IS_ADVENTURE_ROOM(room)) && !COMPLEX_DATA(room)) {
		COMPLEX_DATA(room) = init_complex_data();
	}
	
	// some extra data can be safely cleared here
	remove_room_extra_data(room, ROOM_EXTRA_CHOP_PROGRESS);
	remove_room_extra_data(room, ROOM_EXTRA_HARVEST_PROGRESS);
	remove_room_extra_data(room, ROOM_EXTRA_TRENCH_PROGRESS);
	remove_room_extra_data(room, ROOM_EXTRA_CROP_TYPE);
	remove_room_extra_data(room, ROOM_EXTRA_SEED_TIME);
	
	// if we picked a crop type, 
	if (new_crop) {
		set_room_extra_data(room, ROOM_EXTRA_CROP_TYPE, GET_CROP_VNUM(new_crop));
	}
	
	// do we need to lock the icon?
	if (ROOM_SECT_FLAGGED(room, SECTF_LOCK_ICON)) {
		lock_icon(room, NULL);
	}
	
	// need start locations update?
	if (SECT_FLAGGED(old_sect, SECTF_START_LOCATION) != SECT_FLAGGED(st, SECTF_START_LOCATION)) {
		setup_start_locations();
	}
	
	// for later
	emp = ROOM_OWNER(room);
	
	// did it become unclaimable?
	if (emp && SECT_FLAGGED(st, SECTF_NO_CLAIM)) {
		abandon_room(room);
	}
	
	// do we need to re-read empire territory?
	if (emp) {
		if (belongs != BELONGS_IN_TERRITORY_LIST(room)) {
			read_empire_territory(emp);
		}
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
	}
	
	// re-find just in case
	return find_exit(from, dir);
}


/**
* Creates a new room, initializes it, inserts it in the world, and returns
* its pointer.
*
* @return room_data* the new room
*/
room_data *create_room(void) {
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
	void extract_pending_chars();
	extern struct instance_data *find_instance_by_room(room_data *room);
	void Objsave_char(char_data *ch, int rent_code);
	void perform_abandon_city(empire_data *emp, struct empire_city_data *city, bool full_abandon);
	void relocate_players(room_data *room, room_data *to_room);
	void save_instances(void);

	struct room_direction_data *ex, *next_ex, *temp;
	struct empire_territory_data *ter, *next_ter;
	struct empire_city_data *city, *next_city;
	room_data *rm_iter, *next_rm, *home;
	struct instance_data *inst;
	struct depletion_data *dep;
	struct track_data *track;
	struct affected_type *af;
	struct reset_com *reset;
	bool save_inst = FALSE;
	char_data *c, *next_c;
	obj_data *o, *next_o;
	empire_data *emp, *next_emp;
	int iter;

	if (!room || (GET_ROOM_VNUM(room) < MAP_SIZE && !CAN_UNLOAD_MAP_ROOM(room))) {
		syslog(SYS_ERROR, 0, TRUE, "SYSERR: delete_room() attempting to delete invalid room %d", room ? GET_ROOM_VNUM(room) : NOWHERE);
		return;
	}
	
	// delete any open instance here
	if (ROOM_AFF_FLAGGED(room, ROOM_AFF_HAS_INSTANCE) && (inst = find_instance_by_room(room))) {
		SET_BIT(inst->flags, INST_COMPLETED);
	}
	
	// remove it now
	remove_room_from_world_tables(room);
	
	if (check_exits) {
		// search world for portals that link here
		for (o = object_list; o; o = next_o) {
			next_o = o->next;
		
			if (IS_PORTAL(o) && GET_PORTAL_TARGET_VNUM(o) == GET_ROOM_VNUM(room)) {
				if (IN_ROOM(o)) {
					act("$p closes and vanishes!", FALSE, NULL, o, NULL, TO_ROOM);
				}
				extract_obj(o);
			}
		}
	}
	
	// shrink home
	home = HOME_ROOM(room);
	if (home != room && GET_INSIDE_ROOMS(home) > 0) {
		COMPLEX_DATA(home)->inside_rooms -= 1;
	}
	
	// get rid of players
	relocate_players(room, NULL);
	
	// Remove remaining chars
	for (c = ROOM_PEOPLE(room); c; c = next_c) {
		next_c = c->next_in_room;
		if (!IS_NPC(c)) {
			Objsave_char(c, RENT_RENTED);
			save_char(c, NULL);
		}
		extract_char(c);
	}
	extract_pending_chars();

	/* Remove the objects */
	while (ROOM_CONTENTS(room)) {
		extract_obj(ROOM_CONTENTS(room));
	}
	
	// free some crap
	decustomize_room(room);
	while ((dep = ROOM_DEPLETION(room))) {
		ROOM_DEPLETION(room) = dep->next;
		free(dep);
	}
	while ((track = ROOM_TRACKS(room))) {
		ROOM_TRACKS(room) = track->next;
		free(track);
	}
	if (COMPLEX_DATA(room)) {
		free_complex_data(COMPLEX_DATA(room));
		COMPLEX_DATA(room) = NULL;
	}
	if (SCRIPT(room)) {
		extract_script(room, WLD_TRIGGER);
	}
	free_proto_script(room, WLD_TRIGGER);
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
		free(af);
	}

	if (check_exits) {
		// update all home rooms and exits
		HASH_ITER(world_hh, world_table, rm_iter, next_rm) {
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
					}
				}
			}
		}
	}
	else {
		// just check home rooms of interiors
		HASH_ITER(interior_hh, interior_world_table, rm_iter, next_rm) {
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
	for (inst = instance_list; inst; inst = inst->next) {
		if (inst->location == room) {
			SET_BIT(inst->flags, INST_COMPLETED);
			inst->location = NULL;
			save_inst = TRUE;
		}
		if (inst->start == room) {
			SET_BIT(inst->flags, INST_COMPLETED);
			inst->start = NULL;
			save_inst = TRUE;
		}
		for (iter = 0; iter < inst->size; ++iter) {
			if (inst->room[iter] == room) {
				inst->room[iter] = NULL;
				save_inst = TRUE;
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
	
	// free the room
	free(room);
	
	if (save_inst) {
		save_instances();
	}
	
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
	}
}


/**
* sets up mine data only if not set yet
*
* @param room_data *room The mountain/mine room to set up
* @param char_data *ch Optional: for ability-checking (rare metals)
*/
void init_mine(room_data *room, char_data *ch) {
	extern const struct mine_data_type mine_data[];

	int prc, roll, iter, found = NOTHING;
	
	if (ROOM_CAN_MINE(room) && get_room_extra_data(room, ROOM_EXTRA_MINE_TYPE) == MINE_NOT_SET) {
		roll = number(1, 10000);
		prc = 0;
		
		for (iter = 0; mine_data[iter].type != NOTHING && found == NOTHING; ++iter) {
			if (mine_data[iter].ability == NO_ABIL || (ch && HAS_ABILITY(ch, mine_data[iter].ability))) {
				// add to old prc to see if we're in the "next X percent"
				prc += (int)(mine_data[iter].chance * 100);

				if (mine_data[iter].chance == -1 || roll <= prc) {
					found = iter;
				}
			}
		}
	}
	
	if (found != NOTHING) {
		set_room_extra_data(room, ROOM_EXTRA_MINE_TYPE, mine_data[found].type);
		set_room_extra_data(room, ROOM_EXTRA_MINE_AMOUNT, number(mine_data[found].min_amount, mine_data[found].max_amount));
		
		if (ch && HAS_ABILITY(ch, ABIL_DEEP_MINES)) {
			multiply_room_extra_data(room, ROOM_EXTRA_MINE_AMOUNT, 1.5);
			gain_ability_exp(ch, ABIL_DEEP_MINES, 15);
		}
		
		if (ch && mine_data[found].ability != NO_ABIL) {
			gain_ability_exp(ch, mine_data[found].ability, 75);
		}
	}
}


 //////////////////////////////////////////////////////////////////////////////
//// MANAGEMENT //////////////////////////////////////////////////////////////

/**
* Save a fresh index file for the world.
*/
void save_world_index(void) {
	char filename[64], tempfile[64];
	room_data *iter, *next_iter;
	room_vnum vnum;
	int this, last;
	FILE *fl;
	
	// we only need this if the size of the world changed
	if (!need_world_index) {
		return;
	}
	
	sort_world_table();
	
	sprintf(filename, "%s%s", WLD_PREFIX, INDEX_FILE);
	strcpy(tempfile, filename);
	strcat(tempfile, TEMP_SUFFIX);
	
	if (!(fl = fopen(tempfile, "w"))) {
		syslog(SYS_ERROR, LVL_START_IMM, TRUE, "SYSERR: Unable to write index file '%s': %s", filename, strerror(errno));
		return;
	}
	
	last = -1;
	HASH_ITER(world_hh, world_table, iter, next_iter) {
		vnum = GET_ROOM_VNUM(iter);
		this = GET_WORLD_BLOCK(vnum);
		
		if (this != last) {
			fprintf(fl, "%d%s\n", this, WLD_SUFFIX);
			last = this;
		}
	}
	
	fprintf(fl, "$\n");
	fclose(fl);
	
	// and move the temp file over
	rename(tempfile, filename);
	need_world_index = FALSE;
}


/**
* Executes a full-world save.
*/
void save_whole_world(void) {
	room_data *iter, *next_iter;
	room_vnum vnum;
	int block, last;
	FILE *fl = NULL;
	
	last = -1;
	
	// must sort first
	sort_world_table();
	
	HASH_ITER(world_hh, world_table, iter, next_iter) {
		vnum = GET_ROOM_VNUM(iter);
		block = GET_WORLD_BLOCK(vnum);
		
		if (block != last || !fl) {
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
	
	// ensure this
	save_world_index();
}


/**
* Handles non-adventure reset triggers as well as evolutions, on part of the
* world every 30 seconds. The world is only actually saved every 30 minutes --
* once per mud day.
*/
void update_world(void) {
	static int last_save_group = -1;

	room_data *iter, *next_iter;
	bool deleted = FALSE;
	
	// update save group
	++last_save_group;
	if (last_save_group >= NUM_WORLD_BLOCK_UPDATES) {
		last_save_group = 0;
	}
	
	HASH_ITER(world_hh, world_table, iter, next_iter) {
		// only do certain blocks
		if ((GET_WORLD_BLOCK(GET_ROOM_VNUM(iter)) % NUM_WORLD_BLOCK_UPDATES) != last_save_group) {
			continue;
		}
		
		// skip unload-able map rooms COMPLETELY
		if (CAN_UNLOAD_MAP_ROOM(iter)) {
			// unload it while we're here?
			if (!number(0, 4)) {
				delete_room(iter, FALSE);
				// deleted = TRUE;	// no need to check exits?
			}
		}
		else {
			// reset non-adventures (adventures reset themselves)
			if (!IS_ADVENTURE_ROOM(iter)) {
				reset_wtrigger(iter);
			}
		
			// evolve it
			evolve_one_map_tile(iter);	// world is unsorted after this
		}
	}
	
	// not currently bothering with this	
	if (deleted) {
		check_all_exits();
	}
}


 //////////////////////////////////////////////////////////////////////////////
//// ANNUAL MAP UPDATE ///////////////////////////////////////////////////////

// process map-only annual updates
static void annual_update_map_tile(room_data *room) {
	empire_data *emp;
	
	if (BUILDING_VNUM(room) == BUILDING_RUINS_OPEN || BUILDING_VNUM(room) == BUILDING_RUINS_CLOSED) {
		// roughly 2 real years for average chance for ruins to be gone
		if (!number(0, 89)) {
			disassociate_building(room);
			abandon_room(room);
			
			if (ROOM_PEOPLE(room)) {
				act("The ruins finally crumble to dust!", FALSE, ROOM_PEOPLE(room), NULL, NULL, TO_CHAR | TO_ROOM);
			}
		}
	}
	else if (COMPLEX_DATA(room) && !IS_CITY_CENTER(room) && HOME_ROOM(room) == room && !ROOM_AFF_FLAGGED(room, ROOM_AFF_UNCLAIMABLE | ROOM_AFF_NO_DISREPAIR | ROOM_AFF_HAS_INSTANCE)) {
		emp = ROOM_OWNER(room);

		// decay only non-imm empires				
		if (!emp || !EMPIRE_IMM_ONLY(emp)) {
			COMPLEX_DATA(room)->disrepair += 1;
		
			// 66% chance after 10 years for any building or 2 years for an unfinished one
			if ((BUILDING_DISREPAIR(room) >= config_get_int("disrepair_limit") && number(0, 2)) || (!IS_COMPLETE(room) && !IS_DISMANTLING(room) && BUILDING_DISREPAIR(room) >= config_get_int("disrepair_limit_unfinished"))) {
				ruin_one_building(room);
			}
		}
	}

	// clean mine data from anything that's not currently a mine
	if (!ROOM_BLD_FLAGGED(room, BLD_MINE)) {
		remove_room_extra_data(room, ROOM_EXTRA_MINE_TYPE);
		remove_room_extra_data(room, ROOM_EXTRA_MINE_AMOUNT);
	}
}


// process annual_world_update for 1 room (map or interior)
static void annual_update_room(room_data *room) {
	struct depletion_data *dep, *next_dep, *temp;
	
	// depletions
	for (dep = ROOM_DEPLETION(room); dep; dep = next_dep) {
		next_dep = dep->next;
		
		// halve each year
		dep->count /= 2;
		
		if (dep->count < 10) {
			// at this point just remove it to save space, or it will take years to hit 0
			REMOVE_FROM_LIST(dep, ROOM_DEPLETION(room), next);
			free(dep);
		}
	}
}


/**
* This runs once a mud year to update the world.
*/
void annual_world_update(void) {
	descriptor_data *d;
	room_data *room, *next_room;
	
	// MESSAGE TO ALL
	for (d = descriptor_list; d; d = d->next) {
		if (STATE(d) == CON_PLAYING && d->character) {
			write_to_descriptor(d->descriptor, "The ground under you shakes violently!\r\n");
			
			if (!IS_IMMORTAL(d->character)) {
				if (GET_POS(d->character) > POS_SITTING && !number(0, GET_DEXTERITY(d->character))) {
					write_to_descriptor(d->descriptor, "You're knocked to the ground!\r\n");
					act("$n is knocked to the ground!", TRUE, d->character, NULL, NULL, TO_ROOM);
					GET_POS(d->character) = POS_SITTING;
				}
			}
		}
	}

	HASH_ITER(world_hh, world_table, room, next_room) {
		if (GET_ROOM_VNUM(room) < MAP_SIZE) {
			annual_update_map_tile(room);
		}
		annual_update_room(room);
	}
	
	// lastly
	read_empire_territory(NULL);
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
	}
	
	// verify ownership
	ROOM_OWNER(location) = emp;
	
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
	void setup_generic_npc(char_data *mob, empire_data *emp, int name, int sex);
	void objpack_load_room(room_data *room);
	
	struct reset_com *reset;
	char_data *tmob = NULL; /* for trigger assignment */
	char_data *mob = NULL;
	obj_data *obj = NULL;
	bool found;
	
	// shortcut
	if (!room->reset_commands) {
		return;
	}
	
	// start loading
	for (reset = room->reset_commands; reset; reset = reset->next) {
		switch (reset->command) {
			case 'M': {	// read a mobile
				mob = read_mobile(reset->arg1);
				MOB_FLAGS(mob) = asciiflag_conv(reset->sarg1);
				char_to_room(mob, room);
				
				// sanity
				SET_BIT(MOB_FLAGS(mob), MOB_ISNPC);
				REMOVE_BIT(MOB_FLAGS(mob), MOB_EXTRACTED);
				
				// pulling? attempt to re-attach
				if (obj_proto(reset->arg2)) {
						found = FALSE;
						for (obj = ROOM_CONTENTS(IN_ROOM(mob)); obj && !found; obj = obj->next_content) {
							if (GET_OBJ_VNUM(obj) == reset->arg2) {
								// find available pull slot
								if (GET_PULLED_BY(obj, 0) && !GET_PULLED_BY(obj, 1) && GET_CART_ANIMALS_REQUIRED(obj) > 1) {
									obj->pulled_by2 = mob;
									GET_PULLING(mob) = obj;
									found = TRUE;
								}
								else if (!GET_PULLED_BY(obj, 0)) {
									obj->pulled_by1 = mob;
									GET_PULLING(mob) = obj;
									found = TRUE;
								}
							}
						}
					// couldn't find one -- just tie mob
					if (!found) {
						SET_BIT(MOB_FLAGS(mob), MOB_TIED);
					}
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
						CREATE(SCRIPT(tmob), struct script_data, 1);
					}
					add_trigger(SCRIPT(tmob), read_trigger(reset->arg2), -1);
				}
				else if (reset->arg1 == WLD_TRIGGER) {
					if (!room->script) {
						CREATE(room->script, struct script_data, 1);
						add_trigger(room->script, read_trigger(reset->arg2), -1);
					}
				}
				break;
			}
			
			case 'V': {	// variable assignment
				if (reset->arg1 == MOB_TRIGGER && tmob) {
					if (!SCRIPT(tmob)) {
						log("SYSERR: Attempt to give variable to scriptless mobile");
					}
					else {
						add_var(&(SCRIPT(tmob)->global_vars), reset->sarg1, reset->sarg2, reset->arg3);
					}
				}
				else if (reset->arg1 == WLD_TRIGGER) {
					if (!room->script) {
						log("SYSERR: Attempt to give variable to scriptless object");
					}
					else {
						add_var(&(room->script->global_vars), reset->sarg1, reset->sarg2, reset->arg2);
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

	HASH_ITER(world_hh, world_table, room, next_room) {
		if (room->reset_commands) {
			reset_one_room(room);
		}
	}
}


 //////////////////////////////////////////////////////////////////////////////
//// TERRITORY ///////////////////////////////////////////////////////////////

/**
* Mark empire technologies based on room.
*
* @param empire_data *emp the empire
* @param room_data *room the room to check
*/
void check_building_tech(empire_data *emp, room_data *room) {
	int island = GET_ISLAND_ID(room);
	
	if (!IS_COMPLETE(room)) {
		return;
	}
	
	if (ROOM_BLD_FLAGGED(room, BLD_APIARY)) {
		EMPIRE_TECH(emp, TECH_APIARIES) += 1;
		if (island != NO_ISLAND) {
			EMPIRE_ISLAND_TECH(emp, island, TECH_APIARIES) += 1;
		}
	}
	if (ROOM_BLD_FLAGGED(room, BLD_GLASSBLOWER)) {
		EMPIRE_TECH(emp, TECH_GLASSBLOWING) += 1;
		if (island != NO_ISLAND) {
			EMPIRE_ISLAND_TECH(emp, island, TECH_GLASSBLOWING) += 1;
		}
	}
	if (ROOM_BLD_FLAGGED(room, BLD_DOCKS)) {
		EMPIRE_TECH(emp, TECH_SEAPORT) += 1;
		if (island != NO_ISLAND) {
			EMPIRE_ISLAND_TECH(emp, island, TECH_SEAPORT) += 1;
		}
	}
	
	// buildings at all
	if (GET_BUILDING(room)) {
		// set up military right away
		EMPIRE_MILITARY(emp) += GET_BLD_MILITARY(GET_BUILDING(room));
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
	struct empire_territory_data *ter, *tt;
	
	CREATE(ter, struct empire_territory_data, 1);
	ter->room = room;
	ter->population_timer = config_get_int("building_population_timer");
	ter->npcs = NULL;
	ter->marked = FALSE;
	
	// link up
	if (EMPIRE_TERRITORY_LIST(emp)) {
		tt = EMPIRE_TERRITORY_LIST(emp);
		while (tt->next) {
			tt = tt->next;
		}
		
		tt->next = ter;
		ter->next = NULL;
	}
	else {
		ter->next = EMPIRE_TERRITORY_LIST(emp);
		EMPIRE_TERRITORY_LIST(emp) = ter;
	}
	
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

	struct empire_territory_data *temp;
	
	// prevent loss
	if (ter == global_next_territory_entry) {
		global_next_territory_entry = ter->next;
	}

	delete_room_npcs(NULL, ter);
	ter->npcs = NULL;
	
	REMOVE_FROM_LIST(ter, EMPIRE_TERRITORY_LIST(emp), next);
	free(ter);
}


/* This function sets up empire territory at startup and when two empires merge */
/**
* This function sets up empire territory. It is called at startup, after
* empires merge, during claims, during abandons, and some other actions.
* It can be called on one specific empire, or it can be used for ALL empires.
*
* However, calling this on all empires can get rather slow.
*
* @param empire_data *emp The empire to read, or NULL for "all".
*/
void read_empire_territory(empire_data *emp) {
	void read_vault(empire_data *emp);
	
	struct empire_territory_data *ter, *next_ter;
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
			EMPIRE_MILITARY(e) = 0;
			EMPIRE_FAME(e) = 0;
		
			read_vault(e);

			// reset marks to check for dead territory
			for (ter = EMPIRE_TERRITORY_LIST(e); ter; ter = ter->next) {
				ter->marked = FALSE;
			}
		}
	}

	// scan the whole world
	HASH_ITER(world_hh, world_table, iter, next_iter) {
		// skip dependent multi- rooms
		if (ROOM_OWNER(iter) && (!emp || ROOM_OWNER(iter) == emp) && (HOME_ROOM(iter) == iter || IS_INSIDE(iter))) {
			if ((e = ROOM_OWNER(iter))) {
				// only count each building as 1
				if (HOME_ROOM(iter) == iter) {
					if (COUNTS_AS_IN_CITY(iter) || is_in_city_for_empire(iter, e, FALSE, &junk)) {
						EMPIRE_CITY_TERRITORY(e) += 1;
					}
					else {
						EMPIRE_OUTSIDE_TERRITORY(e) += 1;
					}
				}

				if (IS_COMPLETE(iter)) {
					if (GET_BUILDING(iter)) {
						EMPIRE_FAME(e) += GET_BLD_FAME(GET_BUILDING(iter));
					}
			
					// check for tech
					check_building_tech(e, iter);
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
						for (npc = ter->npcs; npc; npc = npc->next) {
							EMPIRE_POPULATION(e) += 1;
						}
					}
				}
			}
			else if (ROOM_OWNER(iter)) {
				// real_empire did not check out
				abandon_room(iter);
			}
		}
	}
	
	// remove any territory that wasn't marked ... in case there is any
	HASH_ITER(hh, empire_table, e, next_e) {
		if (e == emp || !emp) {
			for (ter = EMPIRE_TERRITORY_LIST(e); ter; ter = next_ter) {
				next_ter = ter->next;
			
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
* @param empire_data *emp An empire number, or NOTHING to read all of them
*/
void reread_empire_tech(empire_data *emp) {
	void resort_empires();
	
	extern int top_island_num;
	empire_data *iter, *next_iter;
	int sub, pos;
	
	// nowork
	if (!empire_table) {
		return;
	}
	
	HASH_ITER(hh, empire_table, iter, next_iter) {
		if (emp == iter || !emp) {
			// free old island tech
			if (iter->island_tech != NULL) {
				for (pos = 0; pos < iter->size_island_tech; ++pos) {
					if (iter->island_tech[pos]) {
						free(iter->island_tech[pos]);
					}
				}
				free(iter->island_tech);
			}
			
			// island techs
			CREATE(iter->island_tech, int*, top_island_num+1);
			iter->size_island_tech = top_island_num+1;
			for (pos = 0; pos < iter->size_island_tech; ++pos) {
				CREATE(iter->island_tech[pos], int, NUM_TECHS);
				for (sub = 0; sub < NUM_TECHS; ++sub) {
					EMPIRE_ISLAND_TECH(iter, pos, sub) = 0;
				}
			}
			
			// main techs
			for (sub = 0; sub < NUM_TECHS; ++sub) {
				EMPIRE_TECH(iter, sub) = 0;
			}
		}
	}
	
	read_empire_members(emp, TRUE);
	read_empire_territory(emp);
	
	// special-handling for imm-only empires: give them all techs
	HASH_ITER(hh, empire_table, iter, next_iter) {
		if (emp == iter || !emp) {
			if (EMPIRE_IMM_ONLY(iter)) {
				for (sub = 0; sub < NUM_TECHS; ++sub) {
					EMPIRE_TECH(iter, sub) += 1;
				}
				for (pos = 0; pos < iter->size_island_tech; ++pos) {
					for (sub = 0; sub < NUM_TECHS; ++sub) {
						EMPIRE_ISLAND_TECH(iter, pos, sub) += 1;
					}
				}
			}
		}
	}
	
	resort_empires();
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
			if (IN_ROOM(o)) {
				act("$p closes and vanishes!", FALSE, NULL, o, NULL, TO_ROOM);
			}
			extract_obj(o);
		}
	}
	
	// exits
	HASH_ITER(world_hh, world_table, room, next_room) {
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
				}
			}	
		}
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
	
	HASH_ITER(world_hh, world_table, iter, next_iter) {
		if (COMPLEX_DATA(iter) && ROOM_PRIVATE_OWNER(iter) == id) {
			COMPLEX_DATA(iter)->private_owner = NOBODY;
		}
		
		// TODO some way to generalize this, please
		if (BUILDING_VNUM(iter) == RTYPE_BEDROOM && ROOM_PRIVATE_OWNER(HOME_ROOM(iter)) == NOBODY) {
			remove_designate_objects(iter);
		}
	}
}


/**
* Creates a blank ocean room using BASIC_OCEAN.
*
* @param room_vnum vnum The vnum of the new ocean room.
* @return room_data* A new ocean room.
*/
room_data *create_ocean_room(room_vnum vnum) {
	room_data *room;

	CREATE(room, room_data, 1);
	room->vnum = vnum;
	add_room_to_world_tables(room);
	
	SECT(room) = sector_proto(BASIC_OCEAN);
	ROOM_ORIGINAL_SECT(room) = SECT(room);
	SET_ISLAND_ID(room, NO_ISLAND);
	
	// only if saveable
	if (!CAN_UNLOAD_MAP_ROOM(room)) {
		need_world_index = TRUE;
	}
	
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


// evolutions for 1 tile
static void evolve_one_map_tile(room_data *room) {
	extern bool extract_tavern_resources(room_data *room);
	
	struct evolution_data *evo;
	bool changed;
	int type;
	
	// Tavern: charge periodic resources
	// TODO should this move to the room updates like trench?
	if (ROOM_BLD_FLAGGED(room, BLD_TAVERN) && IS_COMPLETE(room)) {
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
					set_room_extra_data(room, ROOM_EXTRA_TAVERN_TYPE, BREW_NONE);
					set_room_extra_data(room, ROOM_EXTRA_TAVERN_BREWING_TIME, config_get_int("tavern_brew_time"));
				}
			}
		}
	}
	
	// no further action if !evolve
	if (ROOM_AFF_FLAGGED(room, ROOM_AFF_NO_EVOLVE)) {
		return;
	}
	
	// to avoid running more than one:
	changed = FALSE;
	
	// run some evolutions!
	if (!changed && (evo = get_evolution_by_type(SECT(room), EVO_RANDOM))) {
		if (sector_proto(evo->becomes)) {
			change_terrain(room, evo->becomes);
			changed = TRUE;
		}
	}
	
	if (!changed && (evo = get_evolution_by_type(SECT(room), EVO_ADJACENT_ONE))) {
		if (sector_proto(evo->becomes)) {
			if (count_adjacent_sectors(room, evo->value, TRUE) >= 1) {
				change_terrain(room, evo->becomes);
				changed = TRUE;
			}
		}
	}
	
	if (!changed && (evo = get_evolution_by_type(SECT(room), EVO_ADJACENT_MANY))) {
		if (sector_proto(evo->becomes)) {
			if (count_adjacent_sectors(room, evo->value, TRUE) >= 6) {
				change_terrain(room, evo->becomes);
				changed = TRUE;
			}
		}
	}
	
	if (!changed && (evo = get_evolution_by_type(SECT(room), EVO_NEAR_SECTOR))) {
		if (sector_proto(evo->becomes)) {
			if (find_sect_within_distance_from_room(room, evo->value, config_get_int("nearby_sector_distance"))) {
				change_terrain(room, evo->becomes);
				changed = TRUE;
			}
		}
	}

	// Growing Seeds
	if (!changed && ROOM_SECT_FLAGGED(room, SECTF_HAS_CROP_DATA) && (evo = get_evolution_by_type(SECT(room), EVO_CROP_GROWS))) {
		if (sector_proto(evo->becomes)) {
			add_to_room_extra_data(room, ROOM_EXTRA_SEED_TIME, -1);
			if (get_room_extra_data(room, ROOM_EXTRA_SEED_TIME) <= 0) {
				type = get_room_extra_data(room, ROOM_EXTRA_CROP_TYPE);
				change_terrain(room, evo->becomes);
				set_room_extra_data(room, ROOM_EXTRA_CROP_TYPE, type);
				remove_depletion(room, DPLTN_PICK);
				changed = TRUE;
			}
		}
	}
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
	HASH_ITER(world_hh, world_table, iter, next_iter) {
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
		if (GET_CROP_CLIMATE(crop) == climate && !CROP_FLAGGED(crop, CROPF_NOT_WILD)) {
			if (water || !CROP_FLAGGED(crop, CROPF_REQUIRES_WATER) ) {
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
		}
	}
	
	if (!found) {
		// not found? possibly just bad configs -- default to first in table
		log("SYSERR: get_potential_crop_for_location unable to determine a crop for #%d", GET_ROOM_VNUM(location));
		found = crop_table;
	}
	
	return found;
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


void init_room(room_data *room, room_vnum vnum) {
	sector_data *inside = sector_proto(config_get_int("default_inside_sect"));
	
	if (!inside) {
		log("SYSERR: default_inside_sect does not exist");
		exit(1);
	}
	
	room->vnum = vnum;

	room->owner = NULL;
	
	room->sector_type = inside;
	room->original_sector = inside;
	
	COMPLEX_DATA(room) = init_complex_data();	// no type at this point
	room->light = 0;
	
	room->name = NULL;
	room->description = NULL;
	room->icon = NULL;
	
	room->af = NULL;
	room->affects = 0;
	room->base_affects = 0;
	
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
	int dir = BUILDING_ENTRANCE(room);
	room_data *to_room;
	
	// abandon first -- this will take care of accessory rooms, too
	abandon_room(room);
	
	// verify closed status and find a room to exit to
	if (closed) {
		to_room = SHIFT_DIR(room, rev_dir[dir]);
		if (!to_room) {
			closed = FALSE;
		}
	}

	// basic setup
	construct_building(room, closed ? BUILDING_RUINS_CLOSED : BUILDING_RUINS_OPEN);
	COMPLEX_DATA(room)->entrance = dir;
	
	// make the exit
	if (closed && to_room) {
		create_exit(room, to_room, rev_dir[dir], FALSE);
	}
	
	set_room_extra_data(room, ROOM_EXTRA_RUINS_ICON, number(0, NUM_RUINS_ICONS-1));

	if (ROOM_PEOPLE(room)) {
		act("The building around you crumbles to ruin!", FALSE, ROOM_PEOPLE(room), NULL, NULL, TO_CHAR | TO_ROOM);
	}
}


 //////////////////////////////////////////////////////////////////////////////
//// MAP OUTPUT //////////////////////////////////////////////////////////////

/**
* Writes the data files used to generate graphical maps.
*/
void output_map_to_file(void) {
	extern const char mapout_color_tokens[];
	
	FILE *out, *pol, *cit;
	int num, color = 0;
	struct empire_city_data *city;
	room_data *room, *next_room;
	empire_data *emp, *next_emp;
	sector_data *ocean = sector_proto(BASIC_OCEAN);
	crop_data *cp;
	char minibuf[10];
	room_vnum expecting;
	
	char *bannerlist = "0rgybmc";
	
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
	
	expecting = 0;	// next expected vnum
	HASH_ITER(world_hh, world_table, room, next_room) {
		// in case of partially-loaded world:
		while (expecting < GET_ROOM_VNUM(room) && expecting < MAP_SIZE) {
			// normal
			fprintf(out, "%c", mapout_color_tokens[GET_SECT_MAPOUT(ocean)]);
			
			// political
			if (SECT_FLAGGED(ocean, SECTF_SHOW_ON_POLITICAL_MAPOUT)) {
				fprintf(pol, "%c", mapout_color_tokens[GET_SECT_MAPOUT(ocean)]);
			}
			else {
				fprintf(pol, "\n");
			}
			
			++expecting;
			
			if (!(expecting % MAP_WIDTH)) {
				fprintf(out, "\n");
				fprintf(pol, "\n");
			}
		}
		expecting = GET_ROOM_VNUM(room)+1;	// for next iteration
		
		// check for done
		if (GET_ROOM_VNUM(room) >= MAP_SIZE) {
			break;
		}
		
		// normal map output
		if (ROOM_SECT_FLAGGED(room, SECTF_HAS_CROP_DATA) && (cp = crop_proto(ROOM_CROP_TYPE(room)))) {
			fprintf(out, "%c", mapout_color_tokens[GET_CROP_MAPOUT(cp)]);
		}
		else {
			fprintf(out, "%c", mapout_color_tokens[GET_SECT_MAPOUT(SECT(room))]);
		}
		
		// political output
		if ((emp = ROOM_OWNER(room))) {
			color = 0;
			for (num = 0; num < strlen(bannerlist); ++num) {
				sprintf(minibuf, "&%c", bannerlist[num]);
				if (strstr(EMPIRE_BANNER(emp), minibuf) != NULL) {
					color = num;
					break;
				}
				sprintf(minibuf, "&%c", UPPER(bannerlist[num]));
				if (strstr(EMPIRE_BANNER(emp), minibuf) != NULL) {
					color = num;
					break;
				}
			}
			// banner color
			fprintf(pol, "%d", color);
		}
		else {
			// no owner -- only some sects get printed
			if (ROOM_SECT_FLAGGED(room, SECTF_SHOW_ON_POLITICAL_MAPOUT)) {
				fprintf(pol, "%c", mapout_color_tokens[GET_SECT_MAPOUT(SECT(room))]);
			}
			else {
				fprintf(pol, "?");
			}
		}
		
		if (!((GET_ROOM_VNUM(room)+1) % MAP_WIDTH)) {
			fprintf(out, "\n");
			fprintf(pol, "\n");
		}
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

