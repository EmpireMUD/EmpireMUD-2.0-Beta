/* ************************************************************************
*   File: instance.c                                      EmpireMUD 2.0b2 *
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
extern const int rev_dir[];

// external funcs
void scale_item_to_level(obj_data *obj, int level);
void scale_mob_to_level(char_data *mob, int level);
extern int stats_get_building_count(bld_data *bdg);
extern int stats_get_sector_count(sector_data *sect);

// locals
bool can_instance(adv_data *adv);
int count_instances(adv_data *adv);
int count_mobs_in_instance(struct instance_data *inst, mob_vnum vnum);
int count_objs_in_instance(struct instance_data *inst, obj_vnum vnum);
int count_players_in_instance(struct instance_data *inst, bool include_imms);
static int determine_random_exit(adv_data *adv, room_data *from, room_data *to);
room_data *find_room_template_in_instance(struct instance_data *inst, rmt_vnum vnum);
static struct adventure_link_rule *get_link_rule_by_type(adv_data *adv, int type);
any_vnum get_new_instance_id(void);
static void instantiate_rooms(adv_data *adv, struct instance_data *inst, struct adventure_link_rule *rule, room_data *loc, int dir, int rotation);
struct instance_data *real_instance(any_vnum instance_id);
void reset_instance(struct instance_data *inst);
void save_instances();
static void scale_instance_to_level(struct instance_data *inst, int level);
void unlink_instance_entrance(room_data *room);


// local globals
struct instance_data *instance_list = NULL;	// global instance list
bool instance_save_wait = FALSE;	// prevents repeated instance saving


 //////////////////////////////////////////////////////////////////////////////
//// INSTANCE-BUILDING ///////////////////////////////////////////////////////

/**
* Sets up the world entrance for an instanced adventure zone.
*
* @param struct instance_data *inst The instance we're instantiating.
* @param struct adventure_link_rule *rule The rule we're using to set it up.
* @param room_data *loc The pre-validated world location.
* @param int dir The chosen direction, IF required (may be DIR_RANDOM or NO_DIR, too).
*/
static void build_instance_entrance(struct instance_data *inst, struct adventure_link_rule *rule, room_data *loc, int dir) {
	void special_building_setup(char_data *ch, room_data *room);
	void complete_building(room_data *room);
	
	obj_data *portal;
	bld_data *bdg;
	int my_dir;
	
	// nothing to link to!
	if (!inst->start) {
		return;
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
			if (my_dir != NO_DIR) {
				create_exit(loc, inst->start, my_dir, TRUE);
			}
			break;
		}
		case ADV_LINK_PORTAL_WORLD:
		case ADV_LINK_PORTAL_BUILDING_EXISTING:
		case ADV_LINK_PORTAL_BUILDING_NEW: {
			if (obj_proto(rule->portal_in)) {
				portal = read_object(rule->portal_in);
				GET_OBJ_VAL(portal, VAL_PORTAL_TARGET_VNUM) = GET_ROOM_VNUM(inst->start);
				obj_to_room(portal, loc);
				act("$p spins open!", FALSE, NULL, portal, NULL, TO_ROOM);
				load_otrigger(portal);
			}
			if (obj_proto(rule->portal_out)) {
				portal = read_object(rule->portal_out);
				GET_OBJ_VAL(portal, VAL_PORTAL_TARGET_VNUM) = GET_ROOM_VNUM(loc);
				obj_to_room(portal, inst->start);
				act("$p spins open!", FALSE, NULL, portal, NULL, TO_ROOM);
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
		rotation = number(0, NUM_SIMPLE_DIRS-1);
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
	extern const int confused_dirs[NUM_SIMPLE_DIRS][2][NUM_OF_DIRS];

	struct room_direction_data *new;
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
	
	new->dir = dir;
	new->to_room = GET_ROOM_VNUM(to_room);
	new->room_ptr = to_room;
	new->exit_info = exit->exit_info;
	if (new->keyword) {
		free(new->keyword);
	}
	new->keyword = exit->keyword ? str_dup(exit->keyword) : NULL;
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
	
	room_template *temp;
	room_data *room;
	
	if (!rmt) {
		return NULL;
	}
	
	room = create_room();
	attach_template_to_room(rmt, room);
	ROOM_ORIGINAL_SECT(room) = SECT(room) = sector_proto(config_get_int("default_adventure_sect"));
	SET_BIT(ROOM_BASE_FLAGS(room), GET_RMT_BASE_AFFECTS(rmt) | default_affs);
	SET_BIT(ROOM_AFF_FLAGS(room), GET_RMT_BASE_AFFECTS(rmt) | default_affs);
	
	// copy proto script
	CREATE(temp, room_template, 1);
	copy_proto_script(rmt, temp, RMT_TRIGGER);
	room->proto_script = temp->proto_script;
	free(temp);
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
	build_instance_entrance(inst, rule, loc, dir);

	// exits: non-random first
	for (iter = 0; iter < inst->size; ++iter) {
		if (room_list[iter] && template_list[iter]) {
			for (ex = GET_RMT_EXITS(template_list[iter]); ex; ex = ex->next) {
				if (ex->dir != DIR_RANDOM) {
					instantiate_one_exit(inst, room_list[iter], ex, rotation);
				}
			}
		}
	}
	
	// exits: random second (to avoid picking a dir that's blocked)
	for (iter = 0; iter < inst->size; ++iter) {
		if (room_list[iter] && template_list[iter]) {
			for (ex = GET_RMT_EXITS(template_list[iter]); ex; ex = ex->next) {
				if (ex->dir == DIR_RANDOM) {
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
}


 //////////////////////////////////////////////////////////////////////////////
//// INSTANCE GENERATION /////////////////////////////////////////////////////

/**
* Checks secondary link limiters like ADV_LINK_NOT_NEAR_SELF.
*
* @param adv_data *adv The adventure we are trying to link.
* @param room_data *loc The chosen location.
* @return bool TRUE if the location is ok, FALSE if not.
*/
bool validate_linking_limits(adv_data *adv, room_data *loc) {
	struct adventure_link_rule *rule;
	struct instance_data *inst;
	
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
					if (inst->location && compute_distance(inst->location, loc) <= rule->value) {
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
* in find_location_for_rule, which calls this.
*
* @param struct adventure_link_rule *rule The linking rule we're trying.
* @param room_data *loc A location to test.
* @return bool TRUE if the location seems ok.
*/
inline bool validate_one_loc(struct adventure_link_rule *rule, room_data *loc) {
	extern bool is_entrance(room_data *room);
	
	room_data *home = HOME_ROOM(loc);
	empire_data *emp;
	char_data *ch;
	
	const bitvector_t no_no_flags = ROOM_AFF_UNCLAIMABLE | ROOM_AFF_DISMANTLING | ROOM_AFF_HAS_INSTANCE;
	
	// ownership check
	if (ROOM_OWNER(home) && !LINK_FLAGGED(rule, ADV_LINKF_CLAIMED_OK | ADV_LINKF_CITY_ONLY)) {
		return FALSE;
	}
	
	// rules based on specific ownership
	if (LINK_FLAGGED(rule, ADV_LINKF_CITY_ONLY | ADV_LINKF_NO_CITY)) {
		emp = ROOM_OWNER(home);
		if (LINK_FLAGGED(rule, ADV_LINKF_CITY_ONLY) && (!emp || !find_city(emp, loc))) {
			return FALSE;
		}
		if (LINK_FLAGGED(rule, ADV_LINKF_NO_CITY) && emp && find_city(emp, loc)) {
			return FALSE;
		}
	}
	
	// certain room flags are always no-gos
	if (ROOM_AFF_FLAGGED(loc, no_no_flags) || ROOM_AFF_FLAGGED(home, no_no_flags)) {
		return FALSE;
	}
	
	// room is an entrance for something?
	if (rule->type == ADV_LINK_BUILDING_NEW || rule->type == ADV_LINK_PORTAL_BUILDING_NEW) {
		bld_data *bdg = building_proto(rule->value);
		
		if (!IS_SET(GET_BLD_FLAGS(bdg), BLD_OPEN) && is_entrance(loc)) {
			return FALSE;
		}
	}
	
	// do not generate instances in front of players
	for (ch = ROOM_PEOPLE(loc); ch; ch = ch->next_in_room) {
		if (!IS_NPC(ch)) {
			return FALSE;
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
	
	room_data *room, *next_room, *loc, *shift, *found = NULL;
	bld_data *findbdg = NULL;
	sector_data *findsect = NULL;
	room_template *start_room = room_template_proto(GET_ADV_START_VNUM(adv));
	bool match_buildon = FALSE;
	int dir, iter, sub, num_found;
	
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
	if (!match_buildon) {
		// scan the whole world world
		if (findsect || findbdg) {
			num_found = 0;
			HASH_ITER(world_hh, world_table, room, next_room) {
				if (!validate_one_loc(rule, room)) {
					continue;
				}
			
				// check secondary limits
				if (!validate_linking_limits(adv, room)) {
					continue;
				}
			
				// TODO this specifically does not work on ocean without the whole map loaded
				if ((findsect && SECT(room) == findsect) || (findbdg && BUILDING_VNUM(room) == GET_BLD_VNUM(findbdg))) {
					if (!number(0, num_found++) || !found) {
						found = room;
					}
				}
			}
		}
	}
	else {
		// try to match a build rule
		
		for (iter = 0; iter < max_tries && !found; ++iter) {
			// random map spot: this will ignore BASIC_OCEAN rooms using real_real_room
			if (!(loc = real_real_room(number(0, MAP_SIZE-1)))) {
				continue;
			}

			if (!validate_one_loc(rule, loc)) {
				continue;
			}
			
			// check secondary limits
			if (!validate_linking_limits(adv, loc)) {
				continue;
			}
			
			// never build on a closed location
			if (!ROOM_IS_CLOSED(loc) && can_build_on(loc, rule->bld_on)) {
				if (rule->bld_facing == NOBITS) {
					found = loc;
					break;
				}
				else {
					for (sub = 0; sub < max_dir_tries && !found; ++sub) {
						dir = number(0, NUM_2D_DIRS-1);
						
						// matches the dir we need inside?
						if (dir == rule->dir) {
							continue;
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
	}
	
	return found;
}


/**
* This function is called periodically to generate new instances of adventures,
* if possible.
*/
void generate_adventure_instances(void) {
	void sort_world_table();

	struct adventure_link_rule *rule;
	adv_data *iter, *next_iter;
	room_data *loc;
	int try, dir = NO_DIR;
	
	static adv_vnum last_adv_vnum = NOTHING;	// rotation
	
	// try this twice (allows it to wrap if it hits the end)
	for (try = 0; try < 2; ++try) {
		HASH_ITER(hh, adventure_table, iter, next_iter) {
			// skip past the last adventure we instanced
			if (GET_ADV_VNUM(iter) <= last_adv_vnum) {
				continue;
			}
		
			if (can_instance(iter)) {
				// mark it done no matter what -- we only instance 1 per cycle
				last_adv_vnum = GET_ADV_VNUM(iter);
				
				for (rule = GET_ADV_LINKING(iter); rule; rule = rule->next) {
					if ((loc = find_location_for_rule(iter, rule, &dir))) {
						// make it so!
						if (build_instance_loc(iter, rule, loc, dir)) {
							save_instances();
							// only 1 instance per cycle
							return;
						}
					}
				}
				
				// we ran rules on this one; don't run any more
				return;
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
	void extract_pending_chars();
	void relocate_players(room_data *room, room_data *to_room);
	
	struct instance_data *temp;
	char_data *mob, *next_mob;
	room_data *room;
	int iter;
	
	// disable instance saving
	instance_save_wait = TRUE;
	
	if ((room = inst->location) != NULL) {
		// remove any players inside
		for (iter = 0; iter < inst->size; ++iter) {
			if (inst->room[iter]) {
				relocate_players(inst->room[iter], room);
			}
		}
	
		// unlink from location:
		unlink_instance_entrance(inst->location);
	}
	
	// any portal in will be cleaned up by delete_room
	
	// delete mobs
	for (mob = character_list; mob; mob = next_mob) {
		next_mob = mob->next;
		
		if (IS_NPC(mob) && MOB_INSTANCE_ID(mob) == inst->id) {
			act("$n leaves.", TRUE, mob, NULL, NULL, TO_ROOM);
			extract_char(mob);
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
	
	save_instances();
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
						mob = read_mobile(spawn->vnum);
						MOB_INSTANCE_ID(mob) = inst->id;
						setup_generic_npc(mob, NULL, NOTHING, NOTHING);
						char_to_room(mob, room);
						if (inst->level > 0) {
							scale_mob_to_level(mob, inst->level);
						}
						act("$n arrives.", FALSE, mob, NULL, NULL, TO_ROOM);
						load_mtrigger(mob);
					}
					break;
				}
				case ADV_SPAWN_OBJ: {
					if (obj_proto(spawn->vnum) && count_objs_in_instance(inst, spawn->vnum) < spawn->limit) {
						obj = read_object(spawn->vnum);
						instance_obj_setup(inst, obj);
						obj_to_room(obj, room);
						if (inst->level > 0) {
							scale_item_to_level(obj, inst->level);
						}
						act("$p appears.", FALSE, NULL, obj, NULL, TO_ROOM);
						load_otrigger(obj);
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
	bool any = FALSE;
	
	for (inst = instance_list; inst; inst = inst->next) {
		// never reset?
		if (INSTANCE_FLAGGED(inst, INST_COMPLETED) || GET_ADV_RESET_TIME(inst->adventure) <= 0) {
			continue;
		}
		// it isn't time yet?
		if (inst->last_reset + (60 * GET_ADV_RESET_TIME(inst->adventure)) > time(0)) {
			continue;
		}
		
		reset_instance(inst);
		any = TRUE;
	}
	
	if (any) {
		save_instances();
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
			if (count_players_in_instance(inst, TRUE) == 0) {
				delete_instance(inst);
				save = TRUE;
			}
		}
	}
	
	// shut off saves briefly
	instance_save_wait = TRUE;
	
	HASH_ITER(interior_hh, interior_world_table, room, next_room) {
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
		save_instances();
	}
}


/**
* This function cleans up the map location (or sometimes designated room) that
* served as the link for an adventure instance.
*
* @param room_data *room The map (or interior) location that was the anchor for an instance.
*/
void unlink_instance_entrance(room_data *room) {
	// exits to it will be cleaned up by delete_room
	if (ROOM_AFF_FLAGGED(room, ROOM_AFF_TEMPORARY)) {
		if (ROOM_PEOPLE(room)) {
			act("The adventure vanishes around you!", FALSE, ROOM_PEOPLE(room), NULL, NULL, TO_CHAR | TO_ROOM);
		}
		disassociate_building(room);
	}
	
	REMOVE_BIT(ROOM_BASE_FLAGS(room), ROOM_AFF_HAS_INSTANCE);
	REMOVE_BIT(ROOM_AFF_FLAGS(room), ROOM_AFF_HAS_INSTANCE);
	
	// and the home room
	REMOVE_BIT(ROOM_BASE_FLAGS(HOME_ROOM(room)), ROOM_AFF_HAS_INSTANCE);
	REMOVE_BIT(ROOM_AFF_FLAGS(HOME_ROOM(room)), ROOM_AFF_HAS_INSTANCE);
}


 //////////////////////////////////////////////////////////////////////////////
//// INSTANCE UTILS //////////////////////////////////////////////////////////

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
	
	if (GET_ADV_PLAYER_LIMIT(inst->adventure) > 0 && count_players_in_instance(inst, FALSE) >= GET_ADV_PLAYER_LIMIT(inst->adventure)) {
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
* @param struct instance_data *inst The instance to check.
* @param mob_vnum vnum Mob vnum to look for.
* @return int Total number of that mob in the instance.
*/
int count_mobs_in_instance(struct instance_data *inst, mob_vnum vnum) {
	int iter, count = 0;
	char_data *ch;
	
	for (iter = 0; iter < inst->size; ++iter) {
		if (inst->room[iter]) {
			for (ch = ROOM_PEOPLE(inst->room[iter]); ch; ch = ch->next_in_room) {
				if (IS_NPC(ch) && GET_MOB_VNUM(ch) == vnum) {
					++count;
				}
			}
		}
	}
	
	return count;
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
* @return int Total number of players in the instance.
*/
int count_players_in_instance(struct instance_data *inst, bool count_imms) {
	int iter, count = 0;
	char_data *ch;
	
	for (iter = 0; iter < inst->size; ++iter) {
		if (inst->room[iter]) {
			for (ch = ROOM_PEOPLE(inst->room[iter]); ch; ch = ch->next_in_room) {
				if (!IS_NPC(ch) && (count_imms || !IS_IMMORTAL(ch))) {
					++count;
				}
			}
		}
	}
	
	return count;
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


 //////////////////////////////////////////////////////////////////////////////
//// INSTANCE FILE UTILS /////////////////////////////////////////////////////

/**
* @param room_data *room Any world location.
* @return struct instance_data* An instance associated with that room (maybe one it's the map location for), or NULL if none.
*/
struct instance_data *find_instance_by_room(room_data *room) {
	struct instance_data *inst;
	
	if (!room) {
		return NULL;
	}
	
	// check if the room itself has one
	if (COMPLEX_DATA(room) && COMPLEX_DATA(room)->instance) {
		return COMPLEX_DATA(room)->instance;
	}
	
	// check if it's the location for one
	for (inst = instance_list; inst; inst = inst->next) {
		if (inst->location == room) {
			return inst;
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
		if (!inst && IN_ROOM(mob)) {
			if (COMPLEX_DATA(IN_ROOM(mob))) {
				inst = COMPLEX_DATA(IN_ROOM(mob))->instance;
			}
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
			if (GET_ADV_MIN_LEVEL(inst->adventure) > 0) {
				level = MAX(level, GET_ADV_MIN_LEVEL(inst->adventure));
			}
			if (GET_ADV_MAX_LEVEL(inst->adventure) > 0) {
				level = MIN(level, GET_ADV_MAX_LEVEL(inst->adventure));
			}
			
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
		save_instances();
	}
}


/**
* Sets up instance info.
*/
static void renum_instances(void) {
	struct instance_data *inst;
	int iter;
	
	for (inst = instance_list; inst; inst = inst->next) {
		for (iter = 0; iter < inst->size; ++iter) {			
			// set up instance data
			if (inst->room[iter] && COMPLEX_DATA(inst->room[iter])) {
				COMPLEX_DATA(inst->room[iter])->instance = inst;
			}
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
static void scale_instance_to_level(struct instance_data *inst, int level) {	
	int iter;
	char_data *ch;
	obj_data *obj;
	
	inst->level = level;
	
	for (iter = 0; iter < inst->size; ++iter) {
		if (inst->room[iter]) {
			for (ch = ROOM_PEOPLE(inst->room[iter]); ch; ch = ch->next_in_room) {
				if (IS_NPC(ch)) {
					scale_mob_to_level(ch, level);
				}
			}
			for (obj = ROOM_CONTENTS(inst->room[iter]); obj; obj = obj->next_content) {
				scale_item_to_level(obj, level);
			}
		}
	}
	
	save_instances();
}
