/* ************************************************************************
*   File: instance.c                                      EmpireMUD 2.0b5 *
*  Usage: code related to instantiating adventure zones                   *
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
#include "dg_scripts.h"

/**
* Contents:
*   Instance-Building
*   Instance Generation
*   Instance Maintenance
*   Instance Utils
*   Instance File Utils
*/

// external consts
extern const int confused_dirs[NUM_2D_DIRS][2][NUM_OF_DIRS];
extern const char *dirs[];
extern const int rev_dir[];

// external funcs
void scale_item_to_level(obj_data *obj, int level);
void scale_mob_to_level(char_data *mob, int level);
void scale_vehicle_to_level(vehicle_data *veh, int level);
extern int stats_get_building_count(bld_data *bdg);
extern int stats_get_sector_count(sector_data *sect);

// locals
bool can_instance(adv_data *adv);
int count_instances(adv_data *adv);
int count_mobs_in_instance(struct instance_data *inst, mob_vnum vnum);
int count_objs_in_instance(struct instance_data *inst, obj_vnum vnum);
int count_players_in_instance(struct instance_data *inst, bool include_imms, char_data *ignore_ch);
int count_vehicles_in_instance(struct instance_data *inst, any_vnum vnum);
static int determine_random_exit(adv_data *adv, room_data *from, room_data *to);
struct instance_data *find_instance_by_room(room_data *room, bool check_homeroom);
room_data *find_room_template_in_instance(struct instance_data *inst, rmt_vnum vnum);
static struct adventure_link_rule *get_link_rule_by_type(adv_data *adv, int type);
any_vnum get_new_instance_id(void);
static void instantiate_rooms(adv_data *adv, struct instance_data *inst, struct adventure_link_rule *rule, room_data *loc, int dir, int rotation);
void reset_instance(struct instance_data *inst);
void scale_instance_to_level(struct instance_data *inst, int level);
void unlink_instance_entrance(room_data *room, struct instance_data *inst);


// local globals
struct instance_data *instance_list = NULL;	// global instance list
bool instance_save_wait = FALSE;	// prevents repeated instance saving
struct instance_data *quest_instance_global = NULL;	// passes instances through to some quest triggers

// ADV_LINK_x: whether or not a rule specifies a possible location (other types are for limits)
const bool is_location_rule[] = {
	TRUE,	// ADV_LINK_BUILDING_EXISTING
	TRUE,	// ADV_LINK_BUILDING_NEW
	TRUE,	// ADV_LINK_PORTAL_WORLD
	TRUE,	// ADV_LINK_PORTAL_BUILDING_EXISTING
	TRUE,	// ADV_LINK_PORTAL_BUILDING_NEW
	FALSE,	// ADV_LINK_TIME_LIMIT
	FALSE,	// ADV_LINK_NOT_NEAR_SELF
};


 //////////////////////////////////////////////////////////////////////////////
//// INSTANCE-BUILDING ///////////////////////////////////////////////////////

/**
* Sets up the world entrance for an instanced adventure zone.
*
* @param struct instance_data *inst The instance we're instantiating.
* @param struct adventure_link_rule *rule The rule we're using to set it up.
* @param room_data *loc The pre-validated world location.
* @param int dir The chosen direction, IF required (may be DIR_RANDOM or NO_DIR, too).
* @param int rotation The rotatable direction, if applicable (NO_DIR for none).
*/
static void build_instance_entrance(struct instance_data *inst, struct adventure_link_rule *rule, room_data *loc, int dir, int rotation) {
	void special_building_setup(char_data *ch, room_data *room);
	void complete_building(room_data *room);
	
	char_data *mob, *next_mob;
	obj_data *portal;
	bld_data *bdg;
	int my_dir;
	
	// nothing to link to!
	if (!inst->start) {
		return;
	}
	
	// purge mobs in the room
	for (mob = ROOM_PEOPLE(loc); mob; mob = next_mob) {
		next_mob = mob->next_in_room;
		
		if (IS_NPC(mob)) {
			extract_char(mob);
		}
	}
	
	// ADV_LINK_x part 1: things that need buildings added
	switch (rule->type) {
		case ADV_LINK_BUILDING_NEW:
		case ADV_LINK_PORTAL_BUILDING_NEW: {
			if (!(bdg = building_proto(rule->value))) {
				log("SYSERR: Error instantiating adventure #%d: invalid building in link rule", GET_ADV_VNUM(inst->adventure));
				return;
			}
			
			// make the building
			disassociate_building(loc);
			construct_building(loc, GET_BLD_VNUM(bdg));
			special_building_setup(NULL, loc);
			
			// exit?
			if (dir != NO_DIR && ROOM_IS_CLOSED(loc)) {
				create_exit(loc, SHIFT_DIR(loc, dir), dir, FALSE);

				COMPLEX_DATA(loc)->entrance = rev_dir[dir];
			}
			
			complete_building(loc);
			
			// set these so it can be cleaned up later
			SET_BIT(ROOM_BASE_FLAGS(loc), ROOM_AFF_TEMPORARY);
			SET_BIT(ROOM_AFF_FLAGS(loc), ROOM_AFF_TEMPORARY);			
			break;
		}
	}
	
	// small tweaks to room
	SET_BIT(ROOM_BASE_FLAGS(loc), ROOM_AFF_HAS_INSTANCE);
	SET_BIT(ROOM_AFF_FLAGS(loc), ROOM_AFF_HAS_INSTANCE);
	
	// and the home room
	SET_BIT(ROOM_BASE_FLAGS(HOME_ROOM(loc)), ROOM_AFF_HAS_INSTANCE);
	SET_BIT(ROOM_AFF_FLAGS(HOME_ROOM(loc)), ROOM_AFF_HAS_INSTANCE);
	
	// ADV_LINK_x part 2: portal or direction
	switch (rule->type) {
		case ADV_LINK_BUILDING_EXISTING:
		case ADV_LINK_BUILDING_NEW: {
			my_dir = (rule->dir != DIR_RANDOM ? rule->dir : determine_random_exit(inst->adventure, loc, inst->start));
			if (ADVENTURE_FLAGGED(inst->adventure, ADV_ROTATABLE) && rotation != NO_DIR && my_dir != NO_DIR) {
				my_dir = confused_dirs[rotation][0][my_dir];
			}
			if (my_dir != NO_DIR) {
				create_exit(loc, inst->start, my_dir, TRUE);
			}
			break;
		}
		case ADV_LINK_PORTAL_WORLD:
		case ADV_LINK_PORTAL_BUILDING_EXISTING:
		case ADV_LINK_PORTAL_BUILDING_NEW: {
			if (obj_proto(rule->portal_in)) {
				portal = read_object(rule->portal_in, TRUE);
				GET_OBJ_VAL(portal, VAL_PORTAL_TARGET_VNUM) = GET_ROOM_VNUM(inst->start);
				obj_to_room(portal, loc);
				if (ROOM_PEOPLE(IN_ROOM(portal))) {
					act("$p spins open!", FALSE, ROOM_PEOPLE(IN_ROOM(portal)), portal, NULL, TO_CHAR | TO_ROOM);
				}
				load_otrigger(portal);
			}
			if (obj_proto(rule->portal_out)) {
				portal = read_object(rule->portal_out, TRUE);
				GET_OBJ_VAL(portal, VAL_PORTAL_TARGET_VNUM) = GET_ROOM_VNUM(loc);
				obj_to_room(portal, inst->start);
				if (ROOM_PEOPLE(IN_ROOM(portal))) {
					act("$p spins open!", FALSE, ROOM_PEOPLE(IN_ROOM(portal)), portal, NULL, TO_CHAR | TO_ROOM);
				}
				load_otrigger(portal);
			}
			break;
		}
	}
}


/**
* Builds a new instances associated with a pre-approved location.
*
* @param adv_data *adv The adventure to instance.
* @param struct adventure_link_rule *rule The linking rule we're following.
* @param room_data *loc The pre-selected location to link from.
* @param int dir A direction, for rules that need it.
* @return struct instance_data* A pointer to the new instance, or NULL on failure.
*/
struct instance_data *build_instance_loc(adv_data *adv, struct adventure_link_rule *rule, room_data *loc, int dir) {
	struct instance_data *inst, *temp;
	int rotation;
	
	// basic sanitation
	if (!loc) {
		return NULL;
	}
	
	// import dir from existing building
	if (rule->type == ADV_LINK_BUILDING_EXISTING && dir == NO_DIR && !ROOM_BLD_FLAGGED(loc, BLD_OPEN | BLD_ROOM)) {
		dir = rev_dir[BUILDING_ENTRANCE(loc)];
	}
	
 	// make an instance
	CREATE(inst, struct instance_data, 1);
	inst->id = get_new_instance_id();
	inst->adventure = adv;
	inst->next = NULL;
	
	// append to end of list
	if ((temp = instance_list)) {
		while (temp->next) {
			temp = temp->next;
		}
		temp->next = inst;
	}
	else {
		instance_list = inst;
	}
	
	if (ADVENTURE_FLAGGED(adv, ADV_ROTATABLE)) {
		if (dir != NO_DIR && dir != DIR_RANDOM) {
			rotation = dir;
		}
		else {
			rotation = number(0, NUM_SIMPLE_DIRS-1);
		}
	}
	else {
		rotation = NORTH;
	}
	
	// basic data
	inst->location = loc;
	inst->level = 0;	// unscaled
	inst->created = time(0);
	inst->last_reset = 0;	// will update this on 1st reset
	inst->start = NULL;
	inst->size = 0;
	
	// make sure it is in the instance_list BEFORE adding rooms (for create_room updates)
	instantiate_rooms(adv, inst, rule, loc, dir, rotation);
	
	// reset instance (spawns)
	reset_instance(inst);
	
	return inst;
}


/**
* This function first sees if there is already an exit back, and mirrors it
* if possible. Then, it tries to find one that can work in both directions.
* Finally, it iterates and picks one, if possible.
*
* @param adv_data *adv The adventure it's linking in.
* @param room_data *from Origin room.
* @param room_data *to Destination room.
* @return int Any direction constant, or NO_DIR.
*/
static int determine_random_exit(adv_data *adv, room_data *from, room_data *to) {
	struct room_direction_data *ex;
	int try, dir;
	bool confuse = IS_SET(GET_ADV_FLAGS(adv), ADV_CONFUSING_RANDOMS) ? TRUE : FALSE;
	bool need_back = !confuse;
	
	const int max_tries = 24;
	
	// somehow... no building to attach to anyway
	if (!COMPLEX_DATA(from)) {
		return NO_DIR;
	}
	
	// first see if the to room has an exit to us already
	if (COMPLEX_DATA(to) && !confuse) {
		for (ex = COMPLEX_DATA(to)->exits; ex; ex = ex->next) {
			if (ex->room_ptr == from) {
				// found!
				if (!find_exit(from, rev_dir[ex->dir])) {
					return rev_dir[ex->dir];
				}
				else {
					// won't be able to generate the other side anyway
					need_back = FALSE;
				}
				break;
			}
		}
	}
	
	// now randomly try to pick a dir
	for (try = 0; try < max_tries; ++try) {
		// only try the simple dirs at first
		dir = number(0, (try < (max_tries/2) ? NUM_SIMPLE_DIRS-1 : NUM_2D_DIRS-1));
		
		if (!find_exit(from, dir)) {
			if (!need_back) {
				return dir;
			}
			else if (!find_exit(to, rev_dir[dir])) {
				return dir;
			}
			else {
				// keep looking
			}
		}
	}
	
	// did we get this far? iterate over available dirs (random failed us)
	for (dir = 0; dir < NUM_2D_DIRS; ++dir) {
		if (!find_exit(from, dir)) {
			if (!need_back) {
				return dir;
			}
			else if (!find_exit(to, rev_dir[dir])) {
				return dir;
			}
			else {
				// le sigh
			}
		}
	}
	
	// ... there won't be any dir back, take any free dir
	for (dir = 0; dir < NUM_2D_DIRS; ++dir) {
		if (!find_exit(from, dir)) {
			return dir;
		}
	}
	
	return NO_DIR;
}


/**
* Attempts to add an instance of an exit template to a room. This is not
* guaranteed to succeed, as it may not find a valid room, valid target room,
* or a free direction to add.
*
* DIR_RANDOM will attempt to find the best fit of any 2D direction.
*
* @param struct instance_data *inst The instance we're building.
* @param room_data *room The room we're adding an exit to.
* @param struct exit_template *exit The exit template to build from.
* @param int rotation The whole-zone rotation direction (e.g. NORTH).
*/
static void instantiate_one_exit(struct instance_data *inst, room_data *room, struct exit_template *exit, int rotation) {
	struct room_direction_data *new;
	struct exit_template *ex_iter;
	room_data *to_room = NULL;
	int iter, dir;
	
	// find the actual target
	for (iter = 0; iter < inst->size; ++iter) {
		if (inst->room[iter] && ROOM_TEMPLATE_VNUM(inst->room[iter]) == exit->target_room) {
			to_room = inst->room[iter];
			break;
		}
	}
	
	// no target / no problem
	if (!to_room) {
		return;
	}
	
	// determine dir
	if (exit->dir != DIR_RANDOM) {
		// only rotate on non-random exits -- random exits choose based on actual available dirs
		dir = confused_dirs[rotation][0][exit->dir];
	}
	else {
		dir = determine_random_exit(inst->adventure, room, to_room);
	}
	
	// unable to add exit
	if (dir == NO_DIR) {
		return;
	}
	
	// if we already have an exit, repurpose it; otherwise, make one
	if (!(new = find_exit(room, dir))) {
		CREATE(new, struct room_direction_data, 1);
		new->next = COMPLEX_DATA(room)->exits;
		COMPLEX_DATA(room)->exits = new;
	}
	
	// build the exit
	new->dir = dir;
	new->to_room = GET_ROOM_VNUM(to_room);
	new->room_ptr = to_room;
	++GET_EXITS_HERE(to_room);
	new->exit_info = exit->exit_info;
	if (new->keyword) {
		free(new->keyword);
	}
	new->keyword = exit->keyword ? str_dup(exit->keyword) : NULL;
	
	// mark this exit as done to prevent repeats
	exit->done = TRUE;
	
	// check if we need a back exit
	LL_FOREACH(GET_RMT_EXITS(GET_ROOM_TEMPLATE(to_room)), ex_iter) {
		if (!ex_iter->done && ex_iter->target_room == ROOM_TEMPLATE_VNUM(room)) {
			instantiate_one_exit(inst, to_room, ex_iter, rotation);
		}
	}
}


/**
* Instantiates a single room WITHOUT EXITS, and returns its room.
*
* @param struct instance_data *inst The instance we're instantiating in.
* @param room_template *rmt The room template to instantiate.
* @return room_data* The new room.
*/
static room_data *instantiate_one_room(struct instance_data *inst, room_template *rmt) {
	extern room_data *create_room();
	
	const bitvector_t default_affs = ROOM_AFF_UNCLAIMABLE;
	
	sector_data *sect;
	room_data *room;
	
	if (!rmt) {
		return NULL;
	}
	
	room = create_room();
	attach_template_to_room(rmt, room);
	sect = sector_proto(config_get_int("default_adventure_sect"));
	perform_change_sect(room, NULL, sect);
	perform_change_base_sect(room, NULL, sect);
	SET_BIT(ROOM_BASE_FLAGS(room), GET_RMT_BASE_AFFECTS(rmt) | default_affs);
	SET_BIT(ROOM_AFF_FLAGS(room), GET_RMT_BASE_AFFECTS(rmt) | default_affs);
	
	// copy proto script
	room->proto_script = copy_trig_protos(GET_RMT_SCRIPTS(rmt));
	assign_triggers(room, WLD_TRIGGER);
	
	COMPLEX_DATA(room)->instance = inst;
	
	// exits and spawns are not done here
	
	return room;
}


/**
* Generates the real copies of rooms for the adventure's instance, and sets
* up the room data for it.
*
* @param adv_data *adv Which adventure.
* @param struct instance_data *inst The data for the instance we're generating.
* @param struct adventure_link_rule *rule The linking rule we're following.
* @param room_data *loc The pre-selected location to link from.
* @param int dir A direction, for rules that need it.
* @param int rotation The direction the instance "faces", e.g. NORTH.
*/
static void instantiate_rooms(adv_data *adv, struct instance_data *inst, struct adventure_link_rule *rule, room_data *loc, int dir, int rotation) {
	void sort_exits(struct room_direction_data **list);
	
	room_data **room_list = NULL;
	room_template **template_list = NULL;
	room_template *rmt, *next_rmt;
	struct exit_template *ex;
	int iter, pos;
	
	// this is sometimes larger than it needs to be (and that's ok)
	inst->size = (GET_ADV_END_VNUM(adv) - GET_ADV_START_VNUM(adv)) + 1;
	CREATE(room_list, room_data*, inst->size);
	CREATE(template_list, room_template*, inst->size);
	for (iter = 0; iter < inst->size; ++iter) {
		room_list[iter] = NULL;
		template_list[iter] = NULL;
	}
	
	// gotta save the room list right away because create_room and delete_room use it
	inst->room = room_list;
	
	// begin instantiating rooms -- no exits at this point
	pos = 0;
	HASH_ITER(hh, room_template_table, rmt, next_rmt) {
		if (GET_RMT_VNUM(rmt) >= GET_ADV_START_VNUM(adv) && GET_RMT_VNUM(rmt) <= GET_ADV_END_VNUM(adv)) {
			room_list[pos] = instantiate_one_room(inst, rmt);
			template_list[pos] = rmt;
			
			// set up home room and inst->start now
			if (room_list[pos]) {
				if (!inst->start) {
					inst->start = room_list[pos];
				}
				else {
					COMPLEX_DATA(room_list[pos])->home_room = inst->start;
				}
			}
			
			// mark exits as non-instantiated
			LL_FOREACH(GET_RMT_EXITS(rmt), ex) {
				ex->done = FALSE;
			}
			
			// increment pos
			++pos;
		}
	}
	
	if (!inst->start) {
		// wtf?
		log("SYSERR: Failed to instantiate instance for adventure #%d: no start room?", GET_ADV_VNUM(adv));
		return;
	}
	
	// attach the instance to the world (do this before adding dirs, because it may take up a random dir slot)
	// do not rotate this dir; it was pre-validated for building
	build_instance_entrance(inst, rule, loc, dir, rotation);

	// exits: non-random first
	for (iter = 0; iter < inst->size; ++iter) {
		if (room_list[iter] && template_list[iter]) {
			for (ex = GET_RMT_EXITS(template_list[iter]); ex; ex = ex->next) {
				if (!ex->done && ex->dir != DIR_RANDOM) {
					instantiate_one_exit(inst, room_list[iter], ex, rotation);
				}
			}
		}
	}
	
	// exits: random second (to avoid picking a dir that's blocked)
	for (iter = 0; iter < inst->size; ++iter) {
		if (room_list[iter] && template_list[iter]) {
			for (ex = GET_RMT_EXITS(template_list[iter]); ex; ex = ex->next) {
				if (!ex->done && ex->dir == DIR_RANDOM) {
					instantiate_one_exit(inst, room_list[iter], ex, rotation);
				}
			}
		}
	}
	
	// sort exits
	for (iter = 0; iter < inst->size; ++iter) {
		if (room_list[iter]) {
			sort_exits(&COMPLEX_DATA(room_list[iter])->exits);
		}
	}
	
	// run load triggers
	for (iter = 0; iter < inst->size; ++iter) {
		if (room_list[iter]) {
			load_wtrigger(room_list[iter]);
		}
	}
}


 //////////////////////////////////////////////////////////////////////////////
//// INSTANCE GENERATION /////////////////////////////////////////////////////

/**
* Checks secondary link limiters like ADV_LINK_NOT_NEAR_SELF.
*
* @param adv_data *adv The adventure we are trying to link.
* @param room_data *loc The chosen room -- OPTIONAL (pass loc OR map).
* @param struct map_data *map The chosen map tile -- OPTIONAL (pass loc OR map).
* @return bool TRUE if the location is ok, FALSE if not.
*/
bool validate_linking_limits(adv_data *adv, room_data *loc, struct map_data *map) {
	struct adventure_link_rule *rule;
	struct instance_data *inst;
	
	if (!loc && !map) {
		return FALSE;
	}
	
	// Warning: these must all account for having loc OR map
	for (rule = GET_ADV_LINKING(adv); rule; rule = rule->next) {
		// ADV_LINK_x: but only some rules matter here (secondary limiters)
		switch (rule->type) {
			case ADV_LINK_NOT_NEAR_SELF: {
				// adventure cannot link within X tiles of itself
				for (inst = instance_list; inst; inst = inst->next) {
					if (GET_ADV_VNUM(inst->adventure) != GET_ADV_VNUM(adv)) {
						continue;
					}
					if (INSTANCE_FLAGGED(inst, INST_COMPLETED)) {
						// skip completed ones -- it's safe to link a new one now
						continue;
					}
					
					// check distance
					if (inst->location && compute_map_distance(X_COORD(inst->location), Y_COORD(inst->location), (loc ? X_COORD(loc) : MAP_X_COORD(map->vnum)), (loc ? Y_COORD(loc) : MAP_Y_COORD(map->vnum))) <= rule->value) {
						// NO! Too close.
						return FALSE;
					}
				}
				
				break;
			}
			// other types don't have secondary linking limits
		}
	}
	
	// all clear
	return TRUE;
}


/**
* This function checks basic room properties too see if the location is allowed
* at all. It does NOT check the sector/building/bld_on rules -- that is done
* in find_location_for_rule, which calls this. You must pass one of 'loc' or
* 'map' (if you pass both, they must refer to the same location).
*
* @param adv_data *adv The adventure we are linking.
* @param struct adventure_link_rule *rule The linking rule we're trying.
* @param room_data *loc A location to test -- OPTIONAL (pass this or map).
* @param struct map_data *map A location to test -- OPTIONAL (pass this or loc).
* @return bool TRUE if the location seems ok.
*/
bool validate_one_loc(adv_data *adv, struct adventure_link_rule *rule, room_data *loc, struct map_data *map) {
	extern bool is_entrance(room_data *room);
	
	room_data *home;
	struct island_info *isle;
	empire_data *emp;
	char_data *ch;
	int island_id;
	bool junk;
	
	const bitvector_t no_no_flags = ROOM_AFF_DISMANTLING | ROOM_AFF_HAS_INSTANCE;
	
	// need one of these
	if (!loc && !map) {
		return FALSE;
	}
	
	// detect map
	if (!map && GET_ROOM_VNUM(loc) < MAP_SIZE) {
		map = &(world_map[FLAT_X_COORD(loc)][FLAT_Y_COORD(loc)]);
	}
	
	// detect loc (still OPTIONAL at this stage)
	if (!loc) {
		loc = real_real_room(map->vnum);
	}
	
	// detect home room if applicable
	home = loc ? HOME_ROOM(loc) : NULL;
	
	// short-cut for city-only: no owner = definitely no city
	if (LINK_FLAGGED(rule, ADV_LINKF_CITY_ONLY) && (!home || !ROOM_OWNER(home))) {
		return FALSE;
	}
	
	// things that only matter if we received/found loc
	if (loc) {
		// ownership check
		if (ROOM_OWNER(home) && !LINK_FLAGGED(rule, ADV_LINKF_CLAIMED_OK | ADV_LINKF_CITY_ONLY)) {
			return FALSE;
		}
		if (ROOM_OWNER(home) && (EMPIRE_LAST_LOGON(ROOM_OWNER(home)) + (config_get_int("time_to_empire_emptiness") * SECS_PER_REAL_WEEK)) < time(0)) {
			return FALSE;	// owner is timed out -- don't spawn here
		}
	
		// certain room flags are always no-gos
		if ((ROOM_AFF_FLAGGED(loc, no_no_flags) || ROOM_AFF_FLAGGED(home, no_no_flags))) {
			return FALSE;
		}
	
		// do not generate instances in front of players
		for (ch = ROOM_PEOPLE(loc); ch; ch = ch->next_in_room) {
			if (!IS_NPC(ch)) {
				return FALSE;
			}
		}
	
		// rules based on specific ownership (only really checkable if we have a location)
		if (LINK_FLAGGED(rule, ADV_LINKF_CITY_ONLY | ADV_LINKF_NO_CITY)) {
			emp = ROOM_OWNER(home);
			if (LINK_FLAGGED(rule, ADV_LINKF_CITY_ONLY) && (!emp || !is_in_city_for_empire(loc, emp, FALSE, &junk))) {
				return FALSE;
			}
			if (LINK_FLAGGED(rule, ADV_LINKF_NO_CITY) && emp && is_in_city_for_empire(loc, emp, FALSE, &junk)) {
				return FALSE;
			}
		}
	}
	
	// newbie island checks
	island_id = map ? map->island : GET_ISLAND_ID(loc);
	if (island_id != NO_ISLAND) {
		isle = get_island(island_id, TRUE);
		if (IS_SET(isle->flags, ISLE_NEWBIE)) {	// is newbie island
			if (GET_ADV_MIN_LEVEL(adv) > config_get_int("newbie_adventure_cap") && !ADVENTURE_FLAGGED(adv, ADV_NEWBIE_ONLY)) {
				return FALSE;
			}
			if (ADVENTURE_FLAGGED(adv, ADV_NO_NEWBIE)) {
				return FALSE;
			}
		}
		else {	// not newbie
			if (ADVENTURE_FLAGGED(adv, ADV_NEWBIE_ONLY)) {
				return FALSE;
			}
		}
	}
	
	// room is an entrance for something?
	if (rule->type == ADV_LINK_BUILDING_NEW || rule->type == ADV_LINK_PORTAL_BUILDING_NEW) {
		bld_data *bdg = building_proto(rule->value);
		
		if (!IS_SET(GET_BLD_FLAGS(bdg), BLD_OPEN)) {
			// now we need loc
			if (!loc) {
				loc = real_room(map->vnum);
			}
			if (is_entrance(loc)) {
				return FALSE;
			}
		}
	}
	
	// seems ok
	return TRUE;
}


/**
* This function finds a location that matches a linking rule.
*
* @param adv_data *adv The adventure we are linking.
* @param struct adventure_link_rule *rule The linking rule we're trying.
* @param int *which_dir The direction, for linking rules that require one.
* @return room_data* A valid location, or else NULL if we couldn't find one.
*/
room_data *find_location_for_rule(adv_data *adv, struct adventure_link_rule *rule, int *which_dir) {
	extern bool can_build_on(room_data *room, bitvector_t flags);
	extern bool rmt_has_exit(room_template *rmt, int dir);
	
	room_template *start_room = room_template_proto(GET_ADV_START_VNUM(adv));
	room_data *room, *next_room, *loc, *shift, *found = NULL;
	int dir, iter, sub, num_found, pos;
	sector_data *findsect = NULL;
	bool match_buildon = FALSE;
	bld_data *findbdg = NULL, *bdg = NULL;
	struct map_data *map;
	
	const int max_tries = 500, max_dir_tries = 10;	// for random checks
	
	*which_dir = NO_DIR;
	
	// cannot find a location without a start room
	if (!start_room) {
		return NULL;
	}
	
	// ADV_LINK_x
	switch (rule->type) {
		case ADV_LINK_BUILDING_EXISTING: {
			findbdg = building_proto(rule->value);
			break;
		}
		case ADV_LINK_BUILDING_NEW: {
			match_buildon = TRUE;
			break;
		}
		case ADV_LINK_PORTAL_WORLD: {
			findsect = sector_proto(rule->value);
			break;
		}
		case ADV_LINK_PORTAL_BUILDING_EXISTING: {
			findbdg = building_proto(rule->value);
			break;
		}
		case ADV_LINK_PORTAL_BUILDING_NEW: {
			match_buildon = TRUE;
			break;
		}
		default: {
			// type not implemented or cannot be used to generate a link
			return NULL;
		}
	}
	
	// two ways of doing this:
	if (findsect) {	// scan the whole map
		num_found = 0;
		for (map = land_map; map; map = map->next) {
			// looking for sect: fail
			if (findsect && map->sector_type != findsect) {
				continue;
			}
			
			// attributes/limits checks
			if (!validate_one_loc(adv, rule, NULL, map) || !validate_linking_limits(adv, NULL, map)) {
				continue;
			}
			
			// SUCCESS: mark it ok
			if (!number(0, num_found++) || !found) {
				// may already have looked up room
				found = real_room(map->vnum);
			}
		}
	}
	else if (findbdg) {	// check live rooms for matching building
		num_found = 0;
		HASH_ITER(hh, world_table, room, next_room) {
			if (BUILDING_VNUM(room) != GET_BLD_VNUM(findbdg) || !IS_COMPLETE(room)) {
				continue;
			}
			
			// attributes/limits checks
			if (!validate_one_loc(adv, rule, room, NULL) || !validate_linking_limits(adv, room, NULL)) {
				continue;
			}
			
			// SUCCESS: mark it ok
			if (!number(0, num_found++) || !found) {
				found = room;
			}
		}
	}
	else if (match_buildon) {
		// try to match a build rule
		for (iter = 0; iter < max_tries && !found; ++iter) {
			// random location:
			pos = number(0, MAP_SIZE-1);
			map = &(world_map[MAP_X_COORD(pos)][MAP_Y_COORD(pos)]);
			
			// shortcut: skip BASIC_OCEAN
			if (GET_SECT_VNUM(map->sector_type) == BASIC_OCEAN) {
				continue;
			}
			
			// basic validation
			if (!validate_one_loc(adv, rule, NULL, map)) {
				continue;
			}
			
			// check secondary limits
			if (!validate_linking_limits(adv, NULL, map)) {
				continue;
			}
			
			// this spot SEEMS ok... now we need the actual spot:
			loc = real_room(pos);
			
			// can we build here? (never build on a closed location)
			if (ROOM_IS_CLOSED(loc) || !can_build_on(loc, rule->bld_on)) {
				continue;
			}
			
			if (rule->bld_facing == NOBITS) {
				// success!
				found = loc;
				break;
			}
			else {
				for (sub = 0; sub < max_dir_tries && !found; ++sub) {
					dir = number(0, NUM_2D_DIRS-1);
					
					// new non-open building
					if (rule->type == ADV_LINK_BUILDING_NEW && (bdg = building_proto(rule->value)) && !IS_SET(GET_BLD_FLAGS(bdg), BLD_OPEN)) {
						// matches a dir we need inside?
						if (dir == rule->dir || rmt_has_exit(start_room, rev_dir[dir])) {
							continue;
						}
					}
					
					// need a valid map tile to face
					if (!(shift = real_shift(loc, shift_dir[dir][0], shift_dir[dir][1]))) {
						continue;
					}
					
					// ok go
					if (can_build_on(shift, rule->bld_facing)) {
						*which_dir = dir;
						found = loc;
						break;
					}
				}
			}
		}
	}
	
	return found;
}


/**
* This function is called periodically to generate new instances of adventures,
* if possible.
*/
void generate_adventure_instances(void) {
	void sort_world_table();

	struct adventure_link_rule *rule, *rule_iter;
	adv_data *iter, *next_iter;
	room_data *loc;
	int try, num_rules, dir = NO_DIR;
	adv_vnum start_vnum;
	
	static adv_vnum last_adv_vnum = NOTHING;	// rotation
	
	// prevent it from looping forever
	start_vnum = last_adv_vnum;
	
	// try this twice (allows it to wrap if it hits the end)
	for (try = 0; try < 2; ++try) {
		HASH_ITER(hh, adventure_table, iter, next_iter) {
			// stop if we get back to where we started
			if (try > 0 && (start_vnum == NOTHING || GET_ADV_VNUM(iter) == start_vnum)) {
				return;
			}
			// skip past the last adventure we instanced
			if (GET_ADV_VNUM(iter) <= last_adv_vnum) {
				continue;
			}
			
			if (can_instance(iter)) {
				// randomly choose one rule to attempt
				num_rules = 0;
				rule = NULL;
				for (rule_iter = GET_ADV_LINKING(iter); rule_iter; rule_iter = rule_iter->next) {
					if (is_location_rule[rule_iter->type]) {
						// choose one at random
						if (!number(0, num_rules++) || !rule) {
							rule = rule_iter;
						}
					}
				}
				
				// did we find one?
				if (rule) {
					if ((loc = find_location_for_rule(iter, rule, &dir))) {
						// make it so!
						if (build_instance_loc(iter, rule, loc, dir)) {
							last_adv_vnum = GET_ADV_VNUM(iter);
							// only 1 instance per cycle
							return;
						}
					}
				}
			}
		}
	
		last_adv_vnum = NOTHING;
	}
}


 //////////////////////////////////////////////////////////////////////////////
//// INSTANCE MAINTENANCE ////////////////////////////////////////////////////

/**
* This deletes an instance but does NOT save the instance file.
*
* @param struct instance_data *inst The instance to delete.
*/
void delete_instance(struct instance_data *inst) {
	void expire_instance_quests(struct instance_data *inst);
	void extract_pending_chars();
	void relocate_players(room_data *room, room_data *to_room);
	
	struct instance_mob *im, *next_im;
	vehicle_data *veh, *next_veh;
	struct instance_data *temp;
	char_data *mob, *next_mob;
	room_data *room;
	int iter;
	
	// disable instance saving
	instance_save_wait = TRUE;
	
	expire_instance_quests(inst);
	
	if ((room = inst->location) != NULL) {
		// remove any players inside
		for (iter = 0; iter < inst->size; ++iter) {
			if (inst->room[iter]) {
				// get rid of vehicles first (helps relocate players inside)
				LL_FOREACH_SAFE2(ROOM_VEHICLES(inst->room[iter]), veh, next_veh, next_in_room) {
					extract_vehicle(veh);
				}
	
				relocate_players(inst->room[iter], room);
			}
		}
	
		// unlink from location:
		unlink_instance_entrance(inst->location, inst);
	}
	
	// any portal in will be cleaned up by delete_room
	
	// delete mobs
	for (mob = character_list; mob; mob = next_mob) {
		next_mob = mob->next;
		
		if (IS_NPC(mob) && MOB_INSTANCE_ID(mob) == inst->id) {
			if (ADVENTURE_FLAGGED(inst->adventure, ADV_NO_MOB_CLEANUP)) {
				// just disassociate
				MOB_INSTANCE_ID(mob) = NOTHING;
				// shouldn't need this: subtract_instance_mob(inst, GET_MOB_VNUM(mob));
			}
			else {
				act("$n leaves.", TRUE, mob, NULL, NULL, TO_ROOM);
				extract_char(mob);
			}
		}
	}
	extract_pending_chars();
	
	// remove rooms
	for (iter = 0; iter < inst->size; ++iter) {
		if (inst->room[iter]) {
			delete_room(inst->room[iter], FALSE);	// must call check_all_exits
			inst->room[iter] = NULL;
		}
	}
	
	check_all_exits();
	
	// remove from list AFTER removing rooms
	REMOVE_FROM_LIST(inst, instance_list, next);
	if (inst->room) {
		free(inst->room);
	}
	
	// other stuff to free
	HASH_ITER(hh, inst->mob_counts, im, next_im) {
		free(im);
	}
	
	free(inst);
	
	// re-enable instance saving
	instance_save_wait = FALSE;
}


/**
* Destroys all instances of a given adventure.
*
* @param adv_data *adv The adventure whose instances we'll delete.
* @return int The number deleted.
*/
int delete_all_instances(adv_data *adv) {
	struct instance_data *inst, *next_inst;
	int count = 0;
	
	for (inst = instance_list; inst; inst = next_inst) {
		next_inst = inst->next;
		
		if (inst->adventure == adv) {
			delete_instance(inst);
			++count;
		}
	}
	
	return count;
}


/**
* Special setup for some types of objects.
*
* @param struct instance_data *inst The instance.
* @param obj_data *obj The object to set up.
*/
void instance_obj_setup(struct instance_data *inst, obj_data *obj) {
	room_data *room;
	
	switch (GET_OBJ_TYPE(obj)) {
		case ITEM_PORTAL: {
			// resolve room template number
			if ((room = find_room_template_in_instance(inst, GET_PORTAL_TARGET_VNUM(obj)))) {
				GET_OBJ_VAL(obj, VAL_PORTAL_TARGET_VNUM) = GET_ROOM_VNUM(room);
			}
			else {
				// portals can't currently link out
				GET_OBJ_VAL(obj, VAL_PORTAL_TARGET_VNUM) = NOWHERE;
			}
			break;
		}
	}
}


/**
* Performs spawns/resets on 1 room.
*
* @param struct instance_data *inst The instance.
* @param room_data *room The room to reset.
*/
static void reset_instance_room(struct instance_data *inst, room_data *room) {
	void setup_generic_npc(char_data *mob, empire_data *emp, int name, int sex);
	
	room_template *rmt = GET_ROOM_TEMPLATE(room);
	struct adventure_spawn *spawn;
	vehicle_data *veh;
	char_data *mob;
	obj_data *obj;
	
	if (!rmt) {
		return;
	}
	
	reset_wtrigger(room);
	
	for (spawn = GET_RMT_SPAWNS(rmt); spawn; spawn = spawn->next) {
		if (number(0, 10000) <= 100 * spawn->percent) {
			switch (spawn->type) {
				case ADV_SPAWN_MOB: {
					if (mob_proto(spawn->vnum) && count_mobs_in_instance(inst, spawn->vnum) < spawn->limit) {
						mob = read_mobile(spawn->vnum, TRUE);
						MOB_INSTANCE_ID(mob) = inst->id;
						add_instance_mob(inst, GET_MOB_VNUM(mob));
						setup_generic_npc(mob, NULL, NOTHING, NOTHING);
						char_to_room(mob, room);
						if (inst->level > 0) {
							scale_mob_to_level(mob, inst->level);
						}
						act("$n arrives.", TRUE, mob, NULL, NULL, TO_ROOM);
						load_mtrigger(mob);
					}
					break;
				}
				case ADV_SPAWN_OBJ: {
					if (obj_proto(spawn->vnum) && count_objs_in_instance(inst, spawn->vnum) < spawn->limit) {
						obj = read_object(spawn->vnum, TRUE);
						instance_obj_setup(inst, obj);
						obj_to_room(obj, room);
						if (inst->level > 0) {
							scale_item_to_level(obj, inst->level);
						}
						if (ROOM_PEOPLE(IN_ROOM(obj))) {
							act("$p appears.", FALSE, ROOM_PEOPLE(IN_ROOM(obj)), obj, NULL, TO_CHAR | TO_ROOM);
						}
						load_otrigger(obj);
					}
					break;
				}
				case ADV_SPAWN_VEH: {
					if (vehicle_proto(spawn->vnum) && count_vehicles_in_instance(inst, spawn->vnum) < spawn->limit) {
						veh = read_vehicle(spawn->vnum, TRUE);
						vehicle_to_room(veh, room);
						if (inst->level > 0) {
							scale_vehicle_to_level(veh, inst->level);
						}
						if (ROOM_PEOPLE(IN_ROOM(veh))) {
							act("$V appears.", FALSE, ROOM_PEOPLE(IN_ROOM(veh)), NULL, veh, TO_CHAR | TO_ROOM);
						}
						load_vtrigger(veh);
					}
					break;
				}
			}
		}
	}
}


/**
* Do a zone reset on 1 instance.
*
* @param struct instance_data *inst The instance.
*/
void reset_instance(struct instance_data *inst) {
	int iter;
	
	for (iter = 0; iter < inst->size; ++iter) {
		if (inst->room[iter]) {
			reset_instance_room(inst, inst->room[iter]);
		}
	}
	
	inst->last_reset = time(0);
}


/**
* Resets all instances.
*/
void reset_instances(void) {
	struct instance_data *inst;
	
	for (inst = instance_list; inst; inst = inst->next) {
		// never reset?
		if (INSTANCE_FLAGGED(inst, INST_COMPLETED) || GET_ADV_RESET_TIME(inst->adventure) <= 0) {
			continue;
		}
		// it isn't time yet?
		if (inst->last_reset + (60 * GET_ADV_RESET_TIME(inst->adventure)) > time(0)) {
			continue;
		}
		// needs to be empty?
		if (ADVENTURE_FLAGGED(inst->adventure, ADV_EMPTY_RESET_ONLY) && count_players_in_instance(inst, FALSE, NULL) > 0) {
			continue;
		}
		
		reset_instance(inst);
	}
}


/**
* Looks for bad instances or orphaned rooms, and removes them.
*/
void prune_instances(void) {
	struct instance_data *inst, *next_inst;
	struct adventure_link_rule *rule;
	bool save = FALSE;
	room_data *room, *next_room;
	
	// look for dead instances
	for (inst = instance_list; inst; inst = next_inst) {
		next_inst = inst->next;
		
		rule = get_link_rule_by_type(inst->adventure, ADV_LINK_TIME_LIMIT);
		
		// look for completed or orphaned instances
		if (INSTANCE_FLAGGED(inst, INST_COMPLETED) || !inst->start || !inst->location || inst->size == 0 || (rule && (inst->created + 60 * rule->value) < time(0))) {
			// well, only if empty
			if (count_players_in_instance(inst, TRUE, NULL) == 0) {
				delete_instance(inst);
				save = TRUE;
			}
		}
	}
	
	// shut off saves briefly
	instance_save_wait = TRUE;
	
	for (room = interior_room_list; room; room = next_room) {
		next_room = room->next_interior;
		
		// scan only non-map rooms for orphaned instance rooms
		if (IS_ADVENTURE_ROOM(room) && (!COMPLEX_DATA(room) || !COMPLEX_DATA(room)->instance)) {
			log("Deleting room %d due to missing instance.", GET_ROOM_VNUM(room));
			delete_room(room, FALSE);	// must call check_all_exits
			save = TRUE;
		}
	}
	
	// reenable saves
	instance_save_wait = FALSE;
	
	if (save) {	
		check_all_exits();
	}
}


/**
* This function cleans up the map location (or sometimes designated room) that
* served as the link for an adventure instance.
*
* @param room_data *room The map (or interior) location that was the anchor for an instance.
* @param struct instance_data *inst The instance being cleaned up.
*/
void unlink_instance_entrance(room_data *room, struct instance_data *inst) {
	extern bool remove_live_script_by_vnum(struct script_data *script, trig_vnum vnum);
	
	adv_data *adv = inst->adventure;
	struct trig_proto_list *tpl;
	trig_data *proto, *trig;
	
	REMOVE_BIT(ROOM_BASE_FLAGS(room), ROOM_AFF_HAS_INSTANCE);
	REMOVE_BIT(ROOM_AFF_FLAGS(room), ROOM_AFF_HAS_INSTANCE);
	
	// and the home room
	REMOVE_BIT(ROOM_BASE_FLAGS(HOME_ROOM(room)), ROOM_AFF_HAS_INSTANCE);
	REMOVE_BIT(ROOM_AFF_FLAGS(HOME_ROOM(room)), ROOM_AFF_HAS_INSTANCE);
	
	// check for scripts
	if (adv && GET_ADV_SCRIPTS(adv)) {
		// add scripts
		LL_FOREACH(GET_ADV_SCRIPTS(adv), tpl) {
			if (!(proto = real_trigger(tpl->vnum))) {
				continue;
			}
			if (!IS_SET(GET_TRIG_TYPE(proto), WTRIG_ADVENTURE_CLEANUP)) {
				continue;
			}
			
			// attach this trigger
			if (!(trig = read_trigger(tpl->vnum))) {
				continue;
			}
			if (!SCRIPT(room)) {
				create_script_data(room, WLD_TRIGGER);
			}
			add_trigger(SCRIPT(room), trig, -1);
		}
		
		// run scripts
		adventure_cleanup_wtrigger(room);
		
		// now remove those triggers again
		LL_FOREACH(GET_ADV_SCRIPTS(adv), tpl) {
			if (!(proto = real_trigger(tpl->vnum))) {
				continue;
			}
			if (!IS_SET(GET_TRIG_TYPE(proto), WTRIG_ADVENTURE_CLEANUP)) {
				continue;
			}
			
			// find and remove
			if (SCRIPT(room)) {
				remove_live_script_by_vnum(SCRIPT(room), tpl->vnum);
			}
		}
	}
	
	// exits to it will be cleaned up by delete_room
	if (ROOM_AFF_FLAGGED(room, ROOM_AFF_TEMPORARY)) {
		if (ROOM_PEOPLE(room)) {
			act("The adventure vanishes around you!", FALSE, ROOM_PEOPLE(room), NULL, NULL, TO_CHAR | TO_ROOM);
		}
		disassociate_building(room);
	}
}


 //////////////////////////////////////////////////////////////////////////////
//// INSTANCE UTILS //////////////////////////////////////////////////////////

/**
* Marks a mob as belonging to an instance (for counting purposes).
*
* @param struct instance_data *inst The instance.
* @param mob_vnum vnum Which mob vnum to add to.
*/
void add_instance_mob(struct instance_data *inst, mob_vnum vnum) {
	struct instance_mob *im;
	
	if (!inst || vnum == NOTHING) {
		return;
	}
	
	// find or create
	HASH_FIND_INT(inst->mob_counts, &vnum, im);
	if (!im) {
		CREATE(im, struct instance_mob, 1);
		im->vnum = vnum;
		HASH_ADD_INT(inst->mob_counts, vnum, im);
	}
	
	// add
	im->count += 1;
}


/**
* Determines if a player is eligible to enter an instance in any way.
*
* @param char_data *ch The person trying to enter.
* @param struct instance_data *inst The instance they are trying to enter.
* @return bool TRUE if the player can enter, FALSE if not.
*/
bool can_enter_instance(char_data *ch, struct instance_data *inst) {
	// always
	if (IS_NPC(ch) || IS_IMMORTAL(ch)) {
		return TRUE;
	}
	
	if (GET_ADV_PLAYER_LIMIT(inst->adventure) > 0 && count_players_in_instance(inst, FALSE, ch) >= GET_ADV_PLAYER_LIMIT(inst->adventure)) {
		return FALSE;
	}
	else {
		return TRUE;
	}
}


/**
* @param adv_data *adv Any adventure.
* @return bool TRUE if it's ok to add an instance of the adventure, or FALSE.
*/
bool can_instance(adv_data *adv) {
	if (!adv) {
		return FALSE;
	}
	if (IS_SET(GET_ADV_FLAGS(adv), ADV_IN_DEVELOPMENT)) {
		return FALSE;
	}
	if (GET_ADV_START_VNUM(adv) > GET_ADV_END_VNUM(adv) || GET_ADV_START_VNUM(adv) == NOWHERE) {
		// invalid zone size
		return FALSE;
	}
	if (!room_template_proto(GET_ADV_START_VNUM(adv))) {
		// the start vnum MUST be a room template
		return FALSE;
	}
	if (count_instances(adv) >= GET_ADV_MAX_INSTANCES(adv)) {
		// never more
		return FALSE;
	}
	
	// yay!
	return TRUE;
}


/**
* @param adv_data *adv The adventure to count.
* @return int The number of active instances of that adventure.
*/
int count_instances(adv_data *adv) {
	struct instance_data *inst;
	int count = 0;
	
	for (inst = instance_list; inst; inst = inst->next) {
		if (inst->adventure == adv && !INSTANCE_FLAGGED(inst, INST_COMPLETED)) {
			++count;
		}
	}
	
	return count;
}


/**
* Returns how many of a mob are live in the world, who originated from the
* instance.
*
* @param struct instance_data *inst The instance.
* @param mob_vnum vnum Which mob vnum to count.
* @return int The number of that mob from the instance.
*/
int count_mobs_in_instance(struct instance_data *inst, mob_vnum vnum) {
	struct instance_mob *im;
	
	if (!inst || vnum == NOTHING) {
		return 0;
	}
	
	HASH_FIND_INT(inst->mob_counts, &vnum, im);
	if (im) {
		return im->count;
	}
	else {
		return 0;
	}
}


/**
* @param struct instance_data *inst The instance to check.
* @param obj_vnum vnum Object vnum to look for.
* @return int Total number of that obj (on the ground) in the instance.
*/
int count_objs_in_instance(struct instance_data *inst, obj_vnum vnum) {
	int iter, count = 0;
	obj_data *obj;
	
	for (iter = 0; iter < inst->size; ++iter) {
		if (inst->room[iter]) {
			for (obj = ROOM_CONTENTS(inst->room[iter]); obj; obj = obj->next_content) {
				if (GET_OBJ_VNUM(obj) == vnum) {
					++count;
				}
			}
		}
	}
	
	return count;
}


/**
* @param struct instance_data *inst The instance to check.
* @param bool count_imms If TRUE, includes immortals (who don't normally contribute to player limits).
* @param char_data *ignore_ch Optional: A character not to count, e.g. someone trying to enter the instance (to prevent counting them against their own entry) (may be NULL).
* @return int Total number of players in the instance.
*/
int count_players_in_instance(struct instance_data *inst, bool count_imms, char_data *ignore_ch) {
	int iter, count = 0;
	char_data *ch;
	
	for (iter = 0; iter < inst->size; ++iter) {
		if (inst->room[iter]) {
			for (ch = ROOM_PEOPLE(inst->room[iter]); ch; ch = ch->next_in_room) {
				if (ch != ignore_ch && !IS_NPC(ch) && (count_imms || !IS_IMMORTAL(ch))) {
					++count;
				}
			}
		}
	}
	
	return count;
}


/**
* @param struct instance_data *inst The instance to check.
* @param any_vnum vnum Vehicle vnum to look for.
* @return int Total number of that vehicle in the instance.
*/
int count_vehicles_in_instance(struct instance_data *inst, any_vnum vnum) {
	int iter, count = 0;
	vehicle_data *veh;
	
	for (iter = 0; iter < inst->size; ++iter) {
		if (inst->room[iter]) {
			LL_FOREACH2(ROOM_VEHICLES(inst->room[iter]), veh, next_in_room) {
				if (VEH_VNUM(veh) == vnum) {
					++count;
				}
			}
		}
	}
	
	return count;
}


/**
* Finds the closest instance with a given room template, and returns the
* instantiated room location.
*
* @param room_data *from Our "closest to" starting point.
* @param rmt_vnum vnum Which room template vnum to look for.
*/
room_data *find_nearest_rmt(room_data *from, rmt_vnum vnum) {
	extern adv_data *get_adventure_for_vnum(rmt_vnum vnum);
	
	adv_data *adv = get_adventure_for_vnum(vnum);
	struct instance_data *inst, *closest = NULL;
	int this, dist = 0;
	room_data *map;
	
	if (!adv) {
		return NULL;	// no such adventure
	}
	if (!(map = get_map_location_for(from))) {
		return NULL;	// does not work if no map loc
	}
	
	LL_FOREACH(instance_list, inst) {
		if (inst->adventure != adv) {
			continue;	// wrong adv
		}
		if (IS_SET(inst->flags, INST_COMPLETED)) {
			continue;	// do not pick a completed adventure
		}
		
		// this could work
		this = compute_distance(map, inst->location);
		if (!closest || this < dist) {
			closest = inst;	// save for later
			dist = this;
		}
	}
	
	if (closest) {
		return find_room_template_in_instance(closest, vnum);
	}
	else {
		return NULL;
	}
}


/**
* This finds a location in an instance by its room template vnum -- allowing
* portals and other things to target by the instance's local version of a
* room.
*
* @param struct instance_data *inst The instance to search.
* @param rmt_vnum vnum The room template number to find.
* @return room_data* The world location that matches, or NULL if none.
*/
room_data *find_room_template_in_instance(struct instance_data *inst, rmt_vnum vnum) {
	int iter;
	
	if (!inst || vnum == NOTHING) {
		return NULL;
	}
	
	for (iter = 0; iter < inst->size; ++iter) {
		if (inst->room[iter] && ROOM_TEMPLATE_VNUM(inst->room[iter]) == vnum) {
			return inst->room[iter];
		}
	}
	
	return NULL;
}


/**
* @param any_vnum instance_id An instance id number, e.g. from a mob.
* @return struct instance_data* A pointer to the instance, or NULL if none.
*/
struct instance_data *get_instance_by_id(any_vnum instance_id) {
	struct instance_data *inst;
	
	for (inst = instance_list; inst; inst = inst->next) {
		if (inst->id == instance_id) {
			return inst;
		}
	}
	
	return NULL;
}


/**
* This tries to find an adventure instance using a mob or the room it's in.
*
* @param char_data *mob A mob to use to try to look up an instance.
* @return struct instance_data* A pointer to the instance, or NULL if none.
*/
struct instance_data *get_instance_by_mob(char_data *mob) {
	struct instance_data *inst = NULL;
	
	// by mob instance id
	if (!inst && IS_NPC(mob) && MOB_INSTANCE_ID(mob) != NOTHING) {
		inst = get_instance_by_id(MOB_INSTANCE_ID(mob));
	}
	
	// try the room it's in?
	if (!inst && IN_ROOM(mob) && COMPLEX_DATA(IN_ROOM(mob))) {
		inst = COMPLEX_DATA(IN_ROOM(mob))->instance;
	}
	
	// one way or the other...
	return inst;
}


/*
* Try to find the relevant instance for a script.
*
* @param int go_type _TRIGGER type for 'go'
* @param void *go A pointer to the thing the script is running on.
* @return struct instance_data* The matching instance for where the script is running, or NULL if none.
*/
struct instance_data *get_instance_for_script(int go_type, void *go) {
	extern room_data *obj_room(obj_data *obj);
	
	struct instance_data *inst = NULL;
	room_data *orm;
	
	// prefer global instance if any (e.g. from a quest trigger)
	if (quest_instance_global) {
		inst = quest_instance_global;
	}
	if (!inst) {
		switch (go_type) {
			case MOB_TRIGGER: {
				// try mob first
				if (MOB_INSTANCE_ID((char_data*)go) != NOTHING) {
					inst = get_instance_by_id(MOB_INSTANCE_ID((char_data*)go));
				}
				if (!inst) {
					inst = find_instance_by_room(IN_ROOM((char_data*)go), FALSE);
				}
				break;
			}
			case OBJ_TRIGGER: {
				if ((orm = obj_room((obj_data*)go))) {
					inst = find_instance_by_room(orm, FALSE);
				}
				break;
			}
			case WLD_TRIGGER:
			case RMT_TRIGGER:
			case BLD_TRIGGER:
			case ADV_TRIGGER: {
				inst = find_instance_by_room((room_data*)go, FALSE);
				break;
			}
			case VEH_TRIGGER: {
				inst = find_instance_by_room(IN_ROOM((vehicle_data*)go), FALSE);
				break;
			}
		}
	}
	
	// may or may not have found one
	return inst;
}


/**
* @return any_vnum Finds the first free instance id.
*/
any_vnum get_new_instance_id(void) {
	struct instance_data *inst;
	any_vnum top_id = -1;
	bool found;
	
	for (inst = instance_list; inst; inst = inst->next) {
		top_id = MAX(top_id, inst->id);
	}
	
	// increment to get the new id
	++top_id;
	
	if (top_id >= MAX_INT) {
		// need to find a lower id available: this only fails if there are more than MAX_INT instances
		for (top_id = 0;; ++top_id) {
			found = FALSE;
			for (inst = instance_list; inst && !found; inst = inst->next) {
				if (inst->id == top_id) {
					found = TRUE;
				}
			}
			
			if (!found) {
				break;
			}
		}
	}
	
	return top_id;
}


/**
* @param any_vnum instance_id The instance id to look up.
* @return struct instance_data* The instance found, or NULL.
*/
struct instance_data *real_instance(any_vnum instance_id) {
	struct instance_data *inst;
	
	// shortcut
	if (instance_id == NOTHING) {
		return NULL;
	}
	
	for (inst = instance_list; inst; inst = inst->next) {
		if (inst->id == instance_id) {
			return inst;
		}
	}
	
	return NULL;
}


/**
* Removes a mob from the instance's count (e.g. when it dies).
*
* @param struct instance_data *inst The instance.
* @param mob_vnum vnum Which mob vnum to subtract.
*/
void subtract_instance_mob(struct instance_data *inst, mob_vnum vnum) {
	struct instance_mob *im;
	
	if (!inst || vnum == NOTHING) {
		return;
	}
	
	// find
	HASH_FIND_INT(inst->mob_counts, &vnum, im);
	if (im) {
		im->count -= 1;
		if (im->count <= 0) {
			HASH_DEL(inst->mob_counts, im);
			free(im);
		}
	}
}


 //////////////////////////////////////////////////////////////////////////////
//// INSTANCE FILE UTILS /////////////////////////////////////////////////////

/**
* @param room_data *room Any world location.
* @param bool check_homeroom If TRUE, also checks for extended instance (the homeroom is marked ROOM_AFF_HAS_INSTANCE too, to prevent claiming, etc)
* @return struct instance_data* An instance associated with that room (maybe one it's the map location for), or NULL if none.
*/
struct instance_data *find_instance_by_room(room_data *room, bool check_homeroom) {
	struct instance_data *inst;
	
	if (!room) {
		return NULL;
	}
	
	// check if the room itself has one
	if (COMPLEX_DATA(room) && COMPLEX_DATA(room)->instance) {
		return COMPLEX_DATA(room)->instance;
	}
	
	// check if it's the location for one
	if (ROOM_AFF_FLAGGED(room, ROOM_AFF_HAS_INSTANCE) || (check_homeroom && ROOM_AFF_FLAGGED(HOME_ROOM(room), ROOM_AFF_HAS_INSTANCE))) {
		for (inst = instance_list; inst; inst = inst->next) {
			if (inst->location == room || (check_homeroom && HOME_ROOM(inst->location) == room)) {
				return inst;
			}
		}
	}
	
	return NULL;
}


/**
* @param adv_data *adv The adventure to search.
* @param int type Any ADV_LINK_x const.
* @return struct adventure_link_rule* The found rule, or NULL.
*/
static struct adventure_link_rule *get_link_rule_by_type(adv_data *adv, int type) {
	struct adventure_link_rule *iter;
	
	for (iter = GET_ADV_LINKING(adv); iter; iter = iter->next) {
		if (iter->type == type) {
			return iter;
		}
	}
	
	return NULL;
}


/**
* This fetches any room- or adventure-based scaling constraints, and saves
* them to the bound variables.
*
* @param room_data *room The location to check (optional, or provide mob).
* @param char_data *mob A mob to use (as it may have an instance id; optional, or provide room).
* @param int *scale_level An int to save the current scale level of the room/zone (default: 0/unscaled).
* @param int *min An int to save the minimum scale level of the room/zone (default: 0/no min).
* @param int *max An int to save the maximum scale level of the room/zone (default: 0/no max).
*/
void get_scale_constraints(room_data *room, char_data *mob, int *scale_level, int *min, int *max) {
	struct instance_data *inst = NULL;
	
	// initialize
	*scale_level = *min = *max = 0;
	
	if (mob) {
		if (MOB_INSTANCE_ID(mob) != NOTHING) {
			inst = real_instance(MOB_INSTANCE_ID(mob));
		}
	}
	if (!inst && room && COMPLEX_DATA(room)) {
		inst = COMPLEX_DATA(room)->instance;
	}
	
	// if we found an instance anywhere
	if (inst) {
		*scale_level = inst->level;
		*min = GET_ADV_MIN_LEVEL(inst->adventure);
		*max = GET_ADV_MAX_LEVEL(inst->adventure);
	}
}


/**
* Load one instance from file.
*
* @param FILE *fl The open read file.
* @param any_vnum idnum The instance id.
* @return struct instance_data* The instance.
*/
static struct instance_data *load_one_instance(FILE *fl, any_vnum idnum) {	
	struct instance_data *inst;
	char line[256], str_in[256];
	int i_in[3];
	long l_in[2];

	CREATE(inst, struct instance_data, 1);
	inst->id = idnum;
	
	// line 1: adventure-vnum, location, start, flags
	if (!get_line(fl, line) || sscanf(line, "%d %d %d %s", &i_in[0], &i_in[1], &i_in[2], str_in) != 4) {
		log("SYSERR: Format error in line 1 of instance");
		exit(1);
	}
	
	inst->adventure = adventure_proto(i_in[0]);
	inst->location = real_room(i_in[1]);
	inst->start = real_room(i_in[2]);
	inst->flags = asciiflag_conv(str_in);

	// line 2: level, created, last-reset
	if (!get_line(fl, line) || sscanf(line, "%d %ld %ld", &i_in[0], &l_in[0], &l_in[1]) != 3) {
		log("SYSERR: Format error in line 2 of instance");
		exit(1);
	}
	
	inst->level = i_in[0];
	inst->created = l_in[0];
	inst->last_reset = l_in[1];
	
	inst->room = NULL;
	inst->size = 0;
	
	// optionals
	for (;;) {
		if (!get_line(fl, line)) {
			log("SYSERR: Format error in instance, expecting alphabetic flags, got: %s", line);
			exit(1);
		}
		switch (*line) {
			case 'R': {
				if (sscanf(line, "R %d", &i_in[0]) != 1) {
					log("SYSERR: Format error in R line of instance, gotL %s", line);
					exit(1);
				}
				
				if (inst->room) {
					RECREATE(inst->room, room_data*, inst->size+1);
				}
				else {
					CREATE(inst->room, room_data*, inst->size+1);
				}
				
				inst->room[inst->size] = real_room(i_in[0]);
				inst->size += 1;
				break;
			}
			case 'S': {
				// done
				return inst;
			}
		}
	}
	
	return inst;
}


/**
* Locks the instance level to the provided level, if it is not already locked,
* and returns the locked scale level of the instance (which may be different,
* if it was already set).
*
* @param room_data *room The location to lock (instance looked up by room).
* @param int level The level to lock to.
* @return int The scaled level of the instance.
*/
int lock_instance_level(room_data *room, int level) {
	struct instance_data *inst;
	
	if (IS_ADVENTURE_ROOM(room) && COMPLEX_DATA(room) && (inst = COMPLEX_DATA(room)->instance)) {
		if (inst->level <= 0) {
			scale_instance_to_level(inst, level);
		}
		else {
			level = inst->level;
		}
	}
	
	return level;
}


/**
* This marks an instance as COMPLETED and clean-up-able.
*
* @param struct instance_data *inst The instance to mark complete.
*/
void mark_instance_completed(struct instance_data *inst) {
	if (inst) {
		SET_BIT(inst->flags, INST_COMPLETED);
	}
}


/**
* Sets up instance info and prunes bad instances
*/
static void renum_instances(void) {
	struct instance_data *inst, *next_inst;
	int iter;
	
	LL_FOREACH_SAFE(instance_list, inst, next_inst) {
		// attach pointers
		for (iter = 0; iter < inst->size; ++iter) {			
			// set up instance data
			if (inst->room[iter] && COMPLEX_DATA(inst->room[iter])) {
				COMPLEX_DATA(inst->room[iter])->instance = inst;
			}
		}
		
		// check bad instance
		if (!inst->location || !inst->start) {
			delete_instance(inst);
		}
	}
}


/**
* Reads all instances from file.
*/
void load_instances(void) {
	struct instance_data *inst, *last_inst = NULL;
	char line[256];
	FILE *fl;
	
	if (!(fl = fopen(INSTANCE_FILE, "r"))) {
		// log("SYSERR: Unable to load instances from file.");
		// there just isn't a file -- that's ok
		return;
	}
	
	while (get_line(fl, line)) {
		if (*line == '#') {
			inst = load_one_instance(fl, atoi(line+1));
			
			if (last_inst) {
				last_inst->next = inst;
			}
			else {
				instance_list = inst;
			}
			last_inst = inst;
		}
		else if (*line == '$') {
			// done;
			break;
		}
		else {
			// junk data?
		}
	}
	
	fclose(fl);
	renum_instances();
}


/**
* Writes all instances to the instance file.
*/
void save_instances(void) {
	struct instance_data *inst;
	FILE *fl;
	int iter;
	
	// this prevents dozens of saves during an instance delete
	if (instance_save_wait) {
		return;
	}
	
	if (!(fl = fopen(INSTANCE_FILE TEMP_SUFFIX, "w"))) {
		log("SYSERR: Unable to write %s", INSTANCE_FILE TEMP_SUFFIX);
		return;
	}

	for (inst = instance_list; inst; inst = inst->next) {
		fprintf(fl, "#%d\n", inst->id);
		fprintf(fl, "%d %d %d %s\n", GET_ADV_VNUM(inst->adventure), inst->location ? GET_ROOM_VNUM(inst->location) : NOWHERE, inst->start ? GET_ROOM_VNUM(inst->start) : NOWHERE, bitv_to_alpha(inst->flags));
		fprintf(fl, "%d %ld %ld\n", inst->level, inst->created, inst->last_reset);
		for (iter = 0; iter < inst->size; ++iter) {
			if (inst->room[iter]) {
				fprintf(fl, "R %d\n", GET_ROOM_VNUM(inst->room[iter]));
			}
		}
		
		fprintf(fl, "S\n");
	}

	fprintf(fl, "$\n");
	fclose(fl);
	rename(INSTANCE_FILE TEMP_SUFFIX, INSTANCE_FILE);
}


/**
* This scales all mobs and objs in the instance to the given level, if
* possible.
* 
* @param struct instance_data *inst The instance to scale.
* @param int level A pre-validated level.
*/
void scale_instance_to_level(struct instance_data *inst, int level) {	
	void scale_vehicle_to_level(vehicle_data *veh, int level);
	
	int iter;
	vehicle_data *veh;
	char_data *ch;
	obj_data *obj;
	
	if (GET_ADV_MIN_LEVEL(inst->adventure) > 0) {
		level = MAX(level, GET_ADV_MIN_LEVEL(inst->adventure));
	}
	if (GET_ADV_MAX_LEVEL(inst->adventure) > 0) {
		level = MIN(level, GET_ADV_MAX_LEVEL(inst->adventure));
	}
	
	inst->level = level;
	
	for (iter = 0; iter < inst->size; ++iter) {
		if (inst->room[iter]) {
			for (obj = ROOM_CONTENTS(inst->room[iter]); obj; obj = obj->next_content) {
				if (GET_OBJ_CURRENT_SCALE_LEVEL(obj) == 0) {
					scale_item_to_level(obj, level);
				}
			}
			LL_FOREACH2(ROOM_VEHICLES(inst->room[iter]), veh, next_in_room) {
				if (VEH_SCALE_LEVEL(veh) == 0) {
					scale_vehicle_to_level(veh, level);
				}
			}
		}
	}
	
	LL_FOREACH(character_list, ch) {
		if (IS_NPC(ch) && MOB_INSTANCE_ID(ch) == inst->id && GET_CURRENT_SCALE_LEVEL(ch) != level) {
			GET_CURRENT_SCALE_LEVEL(ch) = 0;	// force override on level
			scale_mob_to_level(ch, level);
		}
	}
}
