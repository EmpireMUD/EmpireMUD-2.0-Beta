/* ************************************************************************
*   File: building.c                                      EmpireMUD 2.0b5 *
*  Usage: Miscellaneous player-level commands                             *
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
#include "comm.h"
#include "interpreter.h"
#include "handler.h"
#include "db.h"
#include "skills.h"
#include "vnums.h"
#include "dg_scripts.h"

/**
* Contents:
*   Building Data
*   Special Handling
*   Building Helpers
*   Main Building Commands
*   Additional Commands
*/

// locals
void process_build(char_data *ch, room_data *room, int act_type);
void setup_tunnel_entrance(char_data *ch, room_data *room, int dir);

// externs
void adjust_building_tech(empire_data *emp, room_data *room, bool add);
extern bool can_claim(char_data *ch);
extern struct resource_data *copy_resource_list(struct resource_data *input);
void delete_room_npcs(room_data *room, struct empire_territory_data *ter);
void free_complex_data(struct complex_room_data *data);
extern char *get_room_name(room_data *room, bool color);
extern room_data *create_room(room_data *home);
extern bool has_learned_craft(char_data *ch, any_vnum vnum);
void scale_item_to_level(obj_data *obj, int level);
void stop_room_action(room_data *room, int action, int chore);

// external vars
extern const char *bld_on_flags[];
extern const bool can_designate_dir[NUM_OF_DIRS];
extern const bool can_designate_dir_vehicle[NUM_OF_DIRS];
extern const char *dirs[];
extern int rev_dir[];


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
	void init_mine(room_data *room, char_data *ch, empire_data *emp);
		
	// mine data
	if (room_has_function_and_city_ok(room, FNC_MINE)) {
		init_mine(room, ch, ROOM_OWNER(room) ? ROOM_OWNER(room) : (ch ? GET_LOYALTY(ch) : NULL));
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
	void herd_animals_out(room_data *location);
	
	char_data *ch;
	empire_data *emp;
	
	// nothing to do if no building data
	if (!COMPLEX_DATA(room)) {
		return;
	}
	
	// stop builders
	stop_room_action(room, ACT_BUILDING, CHORE_BUILDING);
	stop_room_action(room, ACT_MAINTENANCE, CHORE_MAINTENANCE);
	
	// remove any remaining resource requirements
	free_resource_list(GET_BUILDING_RESOURCES(room));
	GET_BUILDING_RESOURCES(room) = NULL;
	
	// ensure no damage (locally, not home-room)
	COMPLEX_DATA(room)->damage = 0;
	
	// remove incomplete
	REMOVE_BIT(ROOM_AFF_FLAGS(room), ROOM_AFF_INCOMPLETE);
	REMOVE_BIT(ROOM_BASE_FLAGS(room), ROOM_AFF_INCOMPLETE);
	
	complete_wtrigger(room);
	
	// check mounted people
	if (!BLD_ALLOWS_MOUNTS(room)) {
		for (ch = ROOM_PEOPLE(room); ch; ch = ch->next_in_room) {
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
	was_large = ROOM_BLD_FLAGGED(room, BLD_LARGE_CITY_RADIUS);
	was_ter = ROOM_OWNER(room) ? get_territory_type_for_empire(room, ROOM_OWNER(room), FALSE, &junk) : TER_FRONTIER;
	
	sect = SECT(room);
	change_terrain(room, config_get_int("default_building_sect"));
	change_base_sector(room, sect);
	
	// set actual data
	attach_building_to_room(building_proto(type), room, TRUE);
	
	SET_BIT(ROOM_BASE_FLAGS(room), BLD_BASE_AFFECTS(room));
	SET_BIT(ROOM_AFF_FLAGS(room), BLD_BASE_AFFECTS(room));
	
	// check for territory updates
	if (ROOM_OWNER(room) && was_large != ROOM_BLD_FLAGGED(room, BLD_LARGE_CITY_RADIUS)) {
		struct empire_island *eisle = get_empire_island(ROOM_OWNER(room), GET_ISLAND_ID(room));
		is_ter = get_territory_type_for_empire(room, ROOM_OWNER(room), FALSE, &junk);
		
		if (was_ter != is_ter) {	// did territory type change?
			SAFE_ADD(EMPIRE_TERRITORY(ROOM_OWNER(room), was_ter), -1, 0, UINT_MAX, FALSE);
			SAFE_ADD(eisle->territory[was_ter], -1, 0, UINT_MAX, FALSE);
			
			SAFE_ADD(EMPIRE_TERRITORY(ROOM_OWNER(room), is_ter), 1, 0, UINT_MAX, FALSE);
			SAFE_ADD(eisle->territory[is_ter], 1, 0, UINT_MAX, FALSE);
			
			// (total territory does not change)
		}
	}
	
	if (ROOM_OWNER(room)) {
		void deactivate_workforce_room(empire_data *emp, room_data *room);
		deactivate_workforce_room(ROOM_OWNER(room), room);
	}
	
	load_wtrigger(room);
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
		add_to_resource_list(&resources, RES_COMPONENT, CMP_PILLAR, 12, 0);
		add_to_resource_list(&resources, RES_COMPONENT, CMP_LUMBER, 8, 0);
		add_to_resource_list(&resources, RES_COMPONENT, CMP_NAILS, 4, 0);
	}
	
	// entrance
	setup_tunnel_entrance(ch, entrance, dir);
	GET_BUILDING_RESOURCES(entrance) = copy_resource_list(resources);
	SET_BIT(ROOM_BASE_FLAGS(entrance), ROOM_AFF_INCOMPLETE);
	SET_BIT(ROOM_AFF_FLAGS(entrance), ROOM_AFF_INCOMPLETE);
	create_exit(entrance, IN_ROOM(ch), rev_dir[dir], FALSE);

	// exit
	setup_tunnel_entrance(ch, exit, rev_dir[dir]);
	GET_BUILDING_RESOURCES(exit) = copy_resource_list(resources);
	SET_BIT(ROOM_BASE_FLAGS(exit), ROOM_AFF_INCOMPLETE);
	SET_BIT(ROOM_AFF_FLAGS(exit), ROOM_AFF_INCOMPLETE);
	to_room = real_shift(exit, shift_dir[dir][0], shift_dir[dir][1]);
	create_exit(exit, to_room, dir, FALSE);

	// now the length of the tunnel
	for (iter = 0; iter < length; ++iter) {
		new_room = create_room((iter <= length/2) ? entrance : exit);
		attach_building_to_room(building_proto(RTYPE_TUNNEL), new_room, TRUE);
		GET_BUILDING_RESOURCES(new_room) = copy_resource_list(resources);
		SET_BIT(ROOM_BASE_FLAGS(new_room), ROOM_AFF_INCOMPLETE);
		SET_BIT(ROOM_AFF_FLAGS(new_room), ROOM_AFF_INCOMPLETE);

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
* Function to tear down a map tile. This restores it to nature in every
* regard EXCEPT owner. Mines are also not reset this way.
*
* @param room_data *room The map room to disassociate.
*/
void disassociate_building(room_data *room) {
	void decustomize_room(room_data *room);
	void delete_instance(struct instance_data *inst, bool run_cleanup);
	extern struct instance_data *find_instance_by_room(room_data *room, bool check_homeroom, bool allow_fake_loc);
	extern crop_data *get_potential_crop_for_location(room_data *location);
	void remove_designate_objects(room_data *room);
	
	sector_data *old_sect = SECT(room);
	bool was_large, junk;
	room_data *iter, *next_iter;
	struct instance_data *inst;
	bool deleted = FALSE;
	int was_ter, is_ter;
	char_data *temp_ch;
	
	// for updating territory counts
	was_large = ROOM_BLD_FLAGGED(room, BLD_LARGE_CITY_RADIUS);
	was_ter = ROOM_OWNER(room) ? get_territory_type_for_empire(room, ROOM_OWNER(room), FALSE, &junk) : TER_FRONTIER;
	
	if (ROOM_OWNER(room) && GET_BUILDING(room) && IS_COMPLETE(room)) {
		qt_empire_players(ROOM_OWNER(room), qt_lose_building, GET_BLD_VNUM(GET_BUILDING(room)));
		et_lose_building(ROOM_OWNER(room), GET_BLD_VNUM(GET_BUILDING(room)));
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
	delete_room_npcs(room, NULL);
	
	// remove bits including dismantle
	REMOVE_BIT(ROOM_BASE_FLAGS(room), ROOM_AFF_DISMANTLING | ROOM_AFF_TEMPORARY | ROOM_AFF_HAS_INSTANCE | ROOM_AFF_CHAMELEON | ROOM_AFF_NO_FLY | ROOM_AFF_NO_DISMANTLE | ROOM_AFF_NO_DISREPAIR | ROOM_AFF_INCOMPLETE | ROOM_AFF_BRIGHT_PAINT);
	REMOVE_BIT(ROOM_AFF_FLAGS(room), ROOM_AFF_DISMANTLING | ROOM_AFF_TEMPORARY | ROOM_AFF_HAS_INSTANCE | ROOM_AFF_CHAMELEON | ROOM_AFF_NO_FLY | ROOM_AFF_NO_DISMANTLE | ROOM_AFF_NO_DISREPAIR | ROOM_AFF_INCOMPLETE | ROOM_AFF_BRIGHT_PAINT);
	
	// TODO should do an affect-total here in case any of those were also added by an affect?

	// free up the customs
	decustomize_room(room);
	
	// clean up scripts
	if (dg_owner_room == room) {
		// no longer safe to run scripts if this is the room running it
		dg_owner_purged = 1;
		dg_owner_room = NULL;
	}
	if (SCRIPT(room)) {
		extract_script(room, WLD_TRIGGER);
	}
	free_proto_scripts(&room->proto_script);

	// restore sect: this does not use change_terrain()
	perform_change_sect(room, NULL, BASE_SECT(room));

	// update requirement trackers
	if (ROOM_OWNER(room)) {
		qt_empire_players(ROOM_OWNER(room), qt_lose_tile_sector, GET_SECT_VNUM(old_sect));
		et_lose_tile_sector(ROOM_OWNER(room), GET_SECT_VNUM(old_sect));
		
		qt_empire_players(ROOM_OWNER(room), qt_gain_tile_sector, GET_SECT_VNUM(SECT(room)));
		et_gain_tile_sector(ROOM_OWNER(room), GET_SECT_VNUM(SECT(room)));
	}
		
	// also check for missing crop data
	if (SECT_FLAGGED(SECT(room), SECTF_HAS_CROP_DATA | SECTF_CROP) && !ROOM_CROP(room)) {
		crop_data *new_crop = get_potential_crop_for_location(room);
		if (new_crop) {
			set_crop_type(room, new_crop);
		}
	}
	
	if (COMPLEX_DATA(room)) {
		COMPLEX_DATA(room)->home_room = NULL;
	}
	
	// some extra data safely clears now
	remove_room_extra_data(room, ROOM_EXTRA_FIRE_REMAINING);
	remove_room_extra_data(room, ROOM_EXTRA_RUINS_ICON);
	remove_room_extra_data(room, ROOM_EXTRA_GARDEN_WORKFORCE_PROGRESS);
	remove_room_extra_data(room, ROOM_EXTRA_QUARRY_WORKFORCE_PROGRESS);
	remove_room_extra_data(room, ROOM_EXTRA_TAVERN_TYPE);
	remove_room_extra_data(room, ROOM_EXTRA_TAVERN_BREWING_TIME);
	remove_room_extra_data(room, ROOM_EXTRA_TAVERN_AVAILABLE_TIME);
	remove_room_extra_data(room, ROOM_EXTRA_BUILD_RECIPE);
	remove_room_extra_data(room, ROOM_EXTRA_FOUND_TIME);
	remove_room_extra_data(room, ROOM_EXTRA_REDESIGNATE_TIME);
	
	// some event types must be canceled
	cancel_stored_event_room(room, SEV_BURN_DOWN);
	cancel_stored_event_room(room, SEV_TAVERN);
	cancel_stored_event_room(room, SEV_RESET_TRIGGER);
	
	// disassociate inside rooms
	for (iter = interior_room_list; iter; iter = next_iter) {
		next_iter = iter->next_interior;
		
		if (HOME_ROOM(iter) == room && iter != room) {
			dismantle_wtrigger(iter, NULL, FALSE);
			remove_designate_objects(iter);
			
			// move people and contents
			while ((temp_ch = ROOM_PEOPLE(iter))) {
				if (!IS_NPC(temp_ch)) {
					GET_LAST_DIR(temp_ch) = NO_DIR;
				}
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
	if (ROOM_OWNER(room) && was_large != ROOM_BLD_FLAGGED(room, BLD_LARGE_CITY_RADIUS)) {
		struct empire_island *eisle = get_empire_island(ROOM_OWNER(room), GET_ISLAND_ID(room));
		is_ter = get_territory_type_for_empire(room, ROOM_OWNER(room), FALSE, &junk);
		
		if (was_ter != is_ter) {
			SAFE_ADD(EMPIRE_TERRITORY(ROOM_OWNER(room), was_ter), -1, 0, UINT_MAX, FALSE);
			SAFE_ADD(eisle->territory[was_ter], -1, 0, UINT_MAX, FALSE);
			
			SAFE_ADD(EMPIRE_TERRITORY(ROOM_OWNER(room), is_ter), 1, 0, UINT_MAX, FALSE);
			SAFE_ADD(eisle->territory[is_ter], 1, 0, UINT_MAX, FALSE);
			
			// (totals do not change)
		}
	}
}


/**
* Finds a craft_table entry matching building type.
*
* @param bld_vnum build_type e.g. a building vnum
* @return craft_data* The matching building recipe, or NULL.
*/
craft_data *find_build_craft(bld_vnum build_type) {
	craft_data *iter, *next_iter;
	
	HASH_ITER(hh, craft_table, iter, next_iter) {
		if (GET_CRAFT_TYPE(iter) == CRAFT_TYPE_BUILD && GET_CRAFT_BUILD_TYPE(iter) == build_type) {
			return iter;
		}
	}
	
	return NULL;
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
* @param bld_data *bb The building to search for.
* @return bld_data* The building that upgrades to it, or NULL if there isn't one.
*/
bld_data *find_upgraded_from(bld_data *bb) {
	bld_data *iter, *next_iter;
	
	if (!bb) {	// occasionally
		return NULL;
	}
	
	HASH_ITER(hh, building_table, iter, next_iter) {
		if (bld_has_relation(iter, BLD_REL_UPGRADES_TO, GET_BLD_VNUM(bb))) {
			return iter;
		}
	}
	
	return NULL;
}


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
	for (c = ROOM_PEOPLE(IN_ROOM(ch)); c; c = c->next_in_room) {
		if (!IS_NPC(c) && GET_ACTION(c) == ACT_BUILDING) {
			// if the player is loyal to the empire building here, gain skill
			if (!emp || GET_LOYALTY(c) == emp) {
				if (type && GET_CRAFT_ABILITY(type) != NO_ABIL) {
					gain_ability_exp(c, GET_CRAFT_ABILITY(type), 3);
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
	
	msg_to_char(ch, "You finish dismantling the building.\r\n");
	act("$n finishes dismantling the building.", FALSE, ch, 0, 0, TO_ROOM);
	stop_room_action(IN_ROOM(ch), ACT_DISMANTLING, CHORE_BUILDING);
	
	// check for required obj and return it
	if ((type = find_building_list_entry(IN_ROOM(ch), FIND_BUILD_NORMAL)) || (type = find_building_list_entry(IN_ROOM(ch), FIND_BUILD_UPGRADE))) {
		if (GET_CRAFT_REQUIRES_OBJ(type) != NOTHING && (proto = obj_proto(GET_CRAFT_REQUIRES_OBJ(type))) && !OBJ_FLAGGED(proto, OBJ_SINGLE_USE)) {
			newobj = read_object(GET_CRAFT_REQUIRES_OBJ(type), TRUE);
			
			// scale item to minimum level
			scale_item_to_level(newobj, 0);
			
			if (IS_NPC(ch)) {
				obj_to_room(newobj, room);
			}
			else {
				obj_to_char(newobj, ch);
				act("You get $p.", FALSE, ch, newobj, 0, TO_CHAR);
				
				// ensure binding
				if (OBJ_FLAGGED(newobj, OBJ_BIND_FLAGS)) {
					bind_obj_to_player(newobj, ch);
				}
			}
			load_otrigger(newobj);
		}
	}
			
	disassociate_building(room);
}


/**
* Process the end of maintenance.
*
* @param char_data *ch the repairman (pc or npc)
* @param room_data *room the location
*/
void finish_maintenance(char_data *ch, room_data *room) {
	// repair all damage
	if (COMPLEX_DATA(room)) {
		COMPLEX_DATA(room)->damage = 0;
	}
	if (IS_COMPLETE(room) && BUILDING_RESOURCES(room)) {
		free_resource_list(GET_BUILDING_RESOURCES(room));
		GET_BUILDING_RESOURCES(room) = NULL;
	}
	
	msg_to_char(ch, "You complete the maintenance.\r\n");
	act("$n has completed the maintenance.", FALSE, ch, NULL, NULL, TO_ROOM);
	stop_room_action(room, ACT_MAINTENANCE, CHORE_MAINTENANCE);
	stop_room_action(room, ACT_BUILDING, CHORE_BUILDING);
}


/**
* This pushes animals out of a building.
*
* @param room_data *location The building's tile.
*/
void herd_animals_out(room_data *location) {
	extern int perform_move(char_data *ch, int dir, bitvector_t flags);
	
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

		for (ch_iter = ROOM_PEOPLE(location); ch_iter; ch_iter = next_ch) {
			next_ch = ch_iter->next_in_room;
		
			if (IS_NPC(ch_iter) && IN_ROOM(ch_iter) == location && !ch_iter->desc && !ch_iter->master && !AFF_FLAGGED(ch_iter, AFF_CHARM) && MOB_FLAGGED(ch_iter, MOB_ANIMAL) && GET_POS(ch_iter) >= POS_STANDING && !MOB_FLAGGED(ch_iter, MOB_TIED)) {
				if (!herd_msg) {
					act("The animals are herded out of the building...", FALSE, ROOM_PEOPLE(location), NULL, NULL, TO_CHAR | TO_ROOM);
					herd_msg = TRUE;
				}
			
				// move the mob
				if ((!to_room || WATER_SECT(to_room)) && to_reverse && ROOM_BLD_FLAGGED(location, BLD_TWO_ENTRANCES)) {
					if (perform_move(ch_iter, BUILDING_ENTRANCE(location), MOVE_HERD)) {
						found_any = TRUE;
					}
				}
				else if (to_room) {
					if (perform_move(ch_iter, rev_dir[BUILDING_ENTRANCE(location)], MOVE_HERD)) {
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
* One build tick for char.
*
* @param char_data *ch The builder.
* @param room_data *room The location he/she is building.
* @param int act_type ACT_BUILDING or ACT_MAINTENANCE (determines outcome).
*/
void process_build(char_data *ch, room_data *room, int act_type) {
	craft_data *type = find_building_list_entry(room, FIND_BUILD_NORMAL);
	obj_data *found_obj = NULL;
	struct resource_data *res;
	
	// Check if there's no longer a building in that place. 
	if (!GET_BUILDING(room)) {
		// Fail silently
		GET_ACTION(ch) = ACT_NONE;
		return;
	}
	
	// just emergency check that it's not actually dismantling
	if (!IS_DISMANTLING(room) && BUILDING_RESOURCES(room)) {
		if ((res = get_next_resource(ch, BUILDING_RESOURCES(room), can_use_room(ch, room, GUESTS_ALLOWED), TRUE, &found_obj))) {
			// take the item; possibly free the res
			apply_resource(ch, res, &GET_BUILDING_RESOURCES(room), found_obj, APPLY_RES_BUILD, NULL, &GET_BUILT_WITH(room));
			
			// skillups (building only)
			if (type && act_type == ACT_BUILDING && GET_CRAFT_ABILITY(type) != NO_ABIL) {
				gain_ability_exp(ch, GET_CRAFT_ABILITY(type), 3);
			}
		}
		else {
			// missing next resource
			msg_to_char(ch, "You run out of resources and stop working.\r\n");
			act("$n runs out of resources and stops working.", FALSE, ch, 0, 0, TO_ROOM);
			GET_ACTION(ch) = ACT_NONE;
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
	extern const char *pool_types[];
	
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
			// RES_COMPONENT, RES_ACTION, and RES_CURRENCY aren't possible here
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
		copy->next = NULL;
		
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
	
	// done?
	if (!BUILDING_RESOURCES(room)) {
		finish_dismantle(ch, room);
	}
}


/**
* For do_designate: removes items from the room
*
* @param room_data *room The room to check for stray items.
*/
void remove_designate_objects(room_data *room) {
	obj_data *o, *next_o;

	// remove old objects
	for (o = ROOM_CONTENTS(room); o; o = next_o) {
		next_o = o->next_content;
		
		switch (BUILDING_VNUM(room)) {
			case RTYPE_BEDROOM: {
				if (GET_OBJ_VNUM(o) == o_HOME_CHEST) {
					while (o->contains) {
						obj_to_room(o->contains, room);
					}
					extract_obj(o);
				}
			}
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
	SET_BIT(ROOM_AFF_FLAGS(room), tunnel_flags);
	COMPLEX_DATA(room)->entrance = dir;
	if (emp && can_claim(ch) && !ROOM_AFF_FLAGGED(room, ROOM_AFF_UNCLAIMABLE)) {
		ter_type = get_territory_type_for_empire(room, emp, FALSE, &junk);
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
	struct resource_data *composite_resources = NULL, *crcp, *res, *next_res;
	room_data *room, *next_room;
	char_data *targ, *next_targ;
	craft_data *type, *up_type;
	obj_data *obj, *next_obj, *proto;
	bool deleted = FALSE;
	bld_data *up_bldg;
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
	
	// interior only
	for (room = interior_room_list; room; room = next_room) {
		next_room = room->next_interior;
		
		if (HOME_ROOM(room) == loc) {
			remove_designate_objects(room);
			dismantle_wtrigger(room, NULL, FALSE);
			delete_room_npcs(room, NULL);
		
			for (obj = ROOM_CONTENTS(room); obj; obj = next_obj) {
				next_obj = obj->next_content;
				obj_to_room(obj, loc);
			}
			for (targ = ROOM_PEOPLE(room); targ; targ = next_targ) {
				next_targ = targ->next_in_room;
				if (!IS_NPC(targ)) {
					GET_LAST_DIR(targ) = NO_DIR;
				}
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
		COMPLEX_DATA(loc)->private_owner = NOBODY;
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
	else if (complete) {
		// backwards-compatible: attempt to detect resources
		composite_resources = copy_resource_list(GET_CRAFT_RESOURCES(type));
		if (IS_SET(GET_CRAFT_FLAGS(type), CRAFT_UPGRADE)) {
			// for upgraded buildings, must work up the chain
			up_bldg = find_upgraded_from(building_proto(GET_CRAFT_BUILD_TYPE(type)));
			up_type = up_bldg ? find_build_craft(GET_BLD_VNUM(up_bldg)) : NULL;
		
			while (up_bldg && up_type) {
				crcp = composite_resources;
				composite_resources = combine_resources(crcp, GET_CRAFT_RESOURCES(up_type));
				free_resource_list(crcp);
			
				up_bldg = find_upgraded_from(up_bldg);
				up_type = up_bldg ? find_build_craft(GET_BLD_VNUM(up_bldg)) : NULL;
			}
		}
		
		// apply
		GET_BUILDING_RESOURCES(loc) = composite_resources;
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
	
	// cut resources in half: they only get that much back
	halve_resource_list(&GET_BUILDING_RESOURCES(loc), TRUE);

	SET_BIT(ROOM_AFF_FLAGS(loc), ROOM_AFF_DISMANTLING);
	SET_BIT(ROOM_BASE_FLAGS(loc), ROOM_AFF_DISMANTLING);
	delete_room_npcs(loc, NULL);
	
	if (loc && ROOM_OWNER(loc) && GET_BUILDING(loc) && complete) {
		qt_empire_players(ROOM_OWNER(loc), qt_lose_building, GET_BLD_VNUM(GET_BUILDING(loc)));
		et_lose_building(ROOM_OWNER(loc), GET_BLD_VNUM(GET_BUILDING(loc)));
	}
	
	stop_room_action(loc, ACT_DIGGING, CHORE_DIGGING);
	stop_room_action(loc, ACT_BUILDING, CHORE_BUILDING);
	stop_room_action(loc, ACT_MINING, CHORE_MINING);
	stop_room_action(loc, ACT_MINTING, ACT_MINTING);
	stop_room_action(loc, ACT_BATHING, NOTHING);
	stop_room_action(loc, ACT_ESCAPING, NOTHING);
	stop_room_action(loc, ACT_SAWING, CHORE_SAWING);
	stop_room_action(loc, ACT_QUARRYING, CHORE_QUARRYING);
	stop_room_action(loc, ACT_MAINTENANCE, CHORE_MAINTENANCE);
	stop_room_action(loc, ACT_PICKING, CHORE_HERB_GARDENING);
	stop_room_action(loc, NOTHING, CHORE_SCRAPING);
	stop_room_action(loc, NOTHING, CHORE_SMELTING);
	stop_room_action(loc, NOTHING, CHORE_WEAVING);
	stop_room_action(loc, NOTHING, CHORE_NAILMAKING);
	stop_room_action(loc, NOTHING, CHORE_BRICKMAKING);
	stop_room_action(loc, NOTHING, CHORE_TRAPPING);
	stop_room_action(loc, NOTHING, CHORE_TANNING);
	stop_room_action(loc, NOTHING, CHORE_SHEARING);
	stop_room_action(loc, NOTHING, CHORE_NEXUS_CRYSTALS);
	stop_room_action(loc, NOTHING, CHORE_MILLING);
	stop_room_action(loc, NOTHING, CHORE_OILMAKING);
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

ACMD(do_build) {
	extern bool find_and_bind(char_data *ch, obj_vnum vnum);
	extern int get_crafting_level(char_data *ch);
	void show_craft_info(char_data *ch, char *argument, int craft_types);
	
	room_data *to_room = NULL, *to_rev = NULL;
	any_vnum missing_abil = NO_ABIL;
	obj_data *found_obj = NULL;
	empire_data *e = NULL;
	int dir = NORTH, ter_type;
	craft_data *iter, *next_iter, *type = NULL, *abbrev_match = NULL;
	bool found = FALSE, found_any, this_line, is_closed, needs_facing, needs_reverse;
	bool junk, wait;
	
	// rules for ch building a given craft
	#define CHAR_CAN_BUILD_BASIC(ch, ttype)  (GET_CRAFT_TYPE((ttype)) == CRAFT_TYPE_BUILD && !IS_SET(GET_CRAFT_FLAGS((ttype)), CRAFT_UPGRADE | CRAFT_DISMANTLE_ONLY) && (IS_IMMORTAL(ch) || !IS_SET(GET_CRAFT_FLAGS((ttype)), CRAFT_IN_DEVELOPMENT)))
	#define CHAR_CAN_BUILD_LEARNED(ch, ttype)  (!IS_SET(GET_CRAFT_FLAGS(ttype), CRAFT_LEARNED) || has_learned_craft(ch, GET_CRAFT_VNUM(ttype)))
	#define CHAR_CAN_BUILD_ABIL(ch, ttype)  (GET_CRAFT_ABILITY((ttype)) == NO_ABIL || has_ability((ch), GET_CRAFT_ABILITY((ttype))))
	#define CHAR_CAN_BUILD_REQOBJ(ch, ttype)  (GET_CRAFT_REQUIRES_OBJ(ttype) == NOTHING || get_obj_in_list_vnum(GET_CRAFT_REQUIRES_OBJ(ttype), ch->carrying))
	// all rules combined:
	#define CHAR_CAN_BUILD(ch, ttype)  (CHAR_CAN_BUILD_BASIC(ch, ttype) && CHAR_CAN_BUILD_LEARNED(ch, ttype) && CHAR_CAN_BUILD_ABIL(ch, ttype) && CHAR_CAN_BUILD_REQOBJ(ch, ttype))
	
	skip_spaces(&argument);
	
	if (IS_NPC(ch)) {
		msg_to_char(ch, "NPCs cannot use the build command.\r\n");
		return;
	}
	if (!IS_APPROVED(ch) && config_get_bool("build_approval")) {
		send_config_msg(ch, "need_approval_string");
		return;
	}
	
	argument = any_one_word(argument, arg);
	skip_spaces(&argument);
	
	// optional info arg
	if (!str_cmp(arg, "info")) {
		show_craft_info(ch, argument, CRAFT_TYPE_BUILD);
		return;
	}
	// all other functions require standing
	if (GET_POS(ch) < POS_STANDING) {
		send_low_pos_msg(ch);
		return;
	}
	if (*arg && GET_BUILDING(IN_ROOM(ch)) && IS_INCOMPLETE(IN_ROOM(ch))) {
		msg_to_char(ch, "To continue working on this building, just type 'build' with no argument.\r\n");
		return;
	}
	
	// this figures out if the argument was a build recipe
	if (*arg) {
		HASH_ITER(sorted_hh, sorted_crafts, iter, next_iter) {
			if (!CHAR_CAN_BUILD_BASIC(ch, iter)) {
				continue;	// basic checks first
			}
			if (!is_abbrev(arg, GET_CRAFT_NAME(iter))) {
				continue;	// preliminary arg test saves some lookups
			}
			
			// ok, probably a match: test if they can build it
			if (CHAR_CAN_BUILD(ch, iter)) {
				if (!str_cmp(arg, GET_CRAFT_NAME(iter))) {
					type = iter;
					break;
				}
				else if (!abbrev_match && is_abbrev(arg, GET_CRAFT_NAME(iter))) {
					abbrev_match = iter;
				}
			}
			else if (!CHAR_CAN_BUILD_ABIL(ch, iter) && CHAR_CAN_BUILD_LEARNED(ch, iter) && CHAR_CAN_BUILD_REQOBJ(ch, iter)) {
				// if ONLY missing the ability
				missing_abil = GET_CRAFT_ABILITY(iter);
			}
		}
	}
	
	// prefer exact matches
	if (!type && abbrev_match) {
		type = abbrev_match;
	}
	
	// find the required-to-build obj if there is one
	if (type && GET_CRAFT_REQUIRES_OBJ(type) != NOTHING) {
		found_obj = get_obj_in_list_vnum(GET_CRAFT_REQUIRES_OBJ(type), ch->carrying);
	}
	
	if (!type && missing_abil != NOTHING) {
		msg_to_char(ch, "You need the %s ability to build that.\r\n", get_ability_name_by_vnum(missing_abil));
	}
	else if (!*arg || !type) {
		/* Cancel building */
		if (GET_ACTION(ch) == ACT_BUILDING) {
			msg_to_char(ch, "You stop building.\r\n");
			act("$n stops building.", FALSE, ch, 0, 0, TO_ROOM);
			GET_ACTION(ch) = ACT_NONE;
		}
		
		/* Continue building */
		else if (IS_INCOMPLETE(IN_ROOM(ch))) {
			if (GET_ACTION(ch) != ACT_NONE) {
				msg_to_char(ch, "You're kinda busy right now.\r\n");
			}
			else if (IS_BURNING(IN_ROOM(ch))) {
				msg_to_char(ch, "You can't work on a burning building!\r\n");
			}
			else if (!CAN_SEE_IN_DARK_ROOM(ch, IN_ROOM(ch))) {
				msg_to_char(ch, "It's too dark to work on the building.\r\n");
			}
			else {
				start_action(ch, ACT_BUILDING, 0);
				msg_to_char(ch, "You start building.\r\n");
				act("$n starts building.", FALSE, ch, 0, 0, TO_ROOM);
			}
		}
		
		// needs maintenance instead of build?
		else if (IS_COMPLETE(IN_ROOM(ch)) && BUILDING_RESOURCES(IN_ROOM(ch))) {
			msg_to_char(ch, "Use 'maintain' to repair the building.\r\n");
		}
		
		// is being dismantled
		else if (IS_DISMANTLING(IN_ROOM(ch))) {
			msg_to_char(ch, "The building is being dismantled, you can't rebuild it now.\r\n");
		}
		
		/* Send output */
		else {
			if (*arg) {
				msg_to_char(ch, "You don't know how to build '%s'.\r\n", arg);
			}
			else {
				msg_to_char(ch, "Usage: build <structure> [direction]\r\n");
				msg_to_char(ch, "       build info <structure>\r\n");
			}
			msg_to_char(ch, "You know how to build:\r\n");
			this_line = FALSE;
			found_any = FALSE;
			*buf = '\0';
		
			HASH_ITER(sorted_hh, sorted_crafts, iter, next_iter) {
				if (CHAR_CAN_BUILD(ch, iter)) {
					if (strlen(buf) + strlen(GET_CRAFT_NAME(iter)) + 2 >= 80) {
						this_line = FALSE;
						msg_to_char(ch, "%s\r\n", buf);
						*buf = '\0';
					}
					sprintf(buf + strlen(buf), "%s%s", (this_line ? ", " : " "), GET_CRAFT_NAME(iter));
					this_line = TRUE;
					found_any = TRUE;
				}
			}
			if (!found_any) {
				msg_to_char(ch, " nothing\r\n");
			}
			else if (this_line) {
				msg_to_char(ch, "%s\r\n", buf);
			}
		}
	}
	else if (GET_ACTION(ch) != ACT_NONE) {
		msg_to_char(ch, "You're already busy.\r\n");
	}
	else if (IS_SET(GET_CRAFT_FLAGS(type), CRAFT_IN_CITY_ONLY) && !is_in_city_for_empire(IN_ROOM(ch), GET_LOYALTY(ch), TRUE, &wait)) {
		msg_to_char(ch, "You can only build that in a city%s.\r\n", wait ? " (this city was founded too recently)" : "");
	}
	else if (CRAFT_FLAGGED(type, CRAFT_BY_RIVER) && !find_flagged_sect_within_distance_from_char(ch, SECTF_FRESH_WATER, NOBITS, 1)) {
		msg_to_char(ch, "You must build that next to a river.\r\n");
	}
	else if (GET_CRAFT_ABILITY(type) != NO_ABIL && !has_ability(ch, GET_CRAFT_ABILITY(type))) {
		msg_to_char(ch, "You don't have the skill to erect that structure.\r\n");
	}
	else if (GET_CRAFT_MIN_LEVEL(type) > get_crafting_level(ch)) {
		msg_to_char(ch, "You need to have a crafting level of %d to build that.\r\n", GET_CRAFT_MIN_LEVEL(type));
	}
	else if (ROOM_AFF_FLAGGED(IN_ROOM(ch), ROOM_AFF_UNCLAIMABLE)) {
		msg_to_char(ch, "You can't build on unclaimable land.\r\n");
	}
	else if (ROOM_AFF_FLAGGED(IN_ROOM(ch), ROOM_AFF_HAS_INSTANCE)) {
		msg_to_char(ch, "You can't build here until the adventure is gone.\r\n");
	}
	else if (!can_build_or_claim_at_war(ch, IN_ROOM(ch))) {
		msg_to_char(ch, "You can't build here while at war with the empire that controls this area.\r\n");
	}
	else if (GET_CRAFT_REQUIRES_OBJ(type) != NOTHING && !found_obj) {
		msg_to_char(ch, "You need to have %s to build that.\r\n", get_obj_name_by_proto(GET_CRAFT_REQUIRES_OBJ(type)));
	}
	else if (!(e = get_or_create_empire(ch)) && FALSE) {
		msg_to_char(ch, "You will never see this line, it's only here to set up an empire!\r\n");
	}
	else if (!has_permission(ch, PRIV_BUILD, IN_ROOM(ch))) {
		msg_to_char(ch, "You don't have permission to build anything.\r\n");
	}
	else if (!can_use_room(ch, IN_ROOM(ch), MEMBERS_ONLY) || ROOM_AFF_FLAGGED(IN_ROOM(ch), ROOM_AFF_UNCLAIMABLE)) {
		msg_to_char(ch, "You don't have permission to build here.\r\n");
	}
	else if (!can_build_on(IN_ROOM(ch), GET_CRAFT_BUILD_ON(type))) {
		prettier_sprintbit(GET_CRAFT_BUILD_ON(type), bld_on_flags, buf);
		msg_to_char(ch, "You need to build on: %s\r\n", buf);
	}
	else if (!CAN_SEE_IN_DARK_ROOM(ch, IN_ROOM(ch))) {
		msg_to_char(ch, "It's too dark to build anything here.\r\n");
	}
	else if (GET_CRAFT_BUILD_TYPE(type) == NOTHING || !building_proto(GET_CRAFT_BUILD_TYPE(type))) {
		msg_to_char(ch, "That build recipe is not implemented.\r\n");
	}
	else if (GET_CRAFT_REQUIRES_OBJ(type) != NOTHING && found_obj && !consume_otrigger(found_obj, ch, OCMD_BUILD, NULL)) {
		return;	// the trigger should send its own message if it prevented this
	}
	else {
		found = TRUE;
	}

	/* 'found' is used for clean viewing */
	if (!found)
		return;

	is_closed = !IS_SET(GET_BLD_FLAGS(building_proto(GET_CRAFT_BUILD_TYPE(type))), BLD_OPEN);
	needs_facing = (GET_CRAFT_BUILD_FACING(type) != NOBITS) || is_closed;
	needs_reverse = needs_facing && IS_SET(GET_BLD_FLAGS(building_proto(GET_CRAFT_BUILD_TYPE(type))), BLD_TWO_ENTRANCES);

	if ((is_closed || IS_SET(GET_BLD_FLAGS(building_proto(GET_CRAFT_BUILD_TYPE(type))), BLD_BARRIER)) && is_entrance(IN_ROOM(ch))) {
		msg_to_char(ch, "You can't build in front of an entrance.\r\n");
		return;
	}

	if (needs_facing) {
		if (!*argument || (dir = parse_direction(ch, argument)) == NO_DIR) {
			msg_to_char(ch, "Which direction would you like to face this building?\r\n");
			return;
		}
				
		// must be a flat map direction
		if (dir >= NUM_2D_DIRS) {
			msg_to_char(ch, "You can't face it that direction.\r\n");
			return;
		}

		if (!(to_room = real_shift(IN_ROOM(ch), shift_dir[dir][0], shift_dir[dir][1]))) {
			msg_to_char(ch, "You can't face it that direction.\r\n");
			return;
		}

		if (!can_build_on(to_room, GET_CRAFT_BUILD_FACING(type))) {
			prettier_sprintbit(GET_CRAFT_BUILD_FACING(type), bld_on_flags, buf);
			msg_to_char(ch, "You need to build facing: %s\r\n", buf);
			return;
		}
	}
	
	if (needs_reverse) {
		to_rev = real_shift(IN_ROOM(ch), shift_dir[rev_dir[dir]][0], shift_dir[rev_dir[dir]][1]);

		if (!to_rev || !can_build_on(to_rev, GET_CRAFT_BUILD_FACING(type))) {
			prettier_sprintbit(GET_CRAFT_BUILD_FACING(type), bld_on_flags, buf);
			msg_to_char(ch, "You need to build with the reverse side facing: %s\r\n", buf);
			return;
		}
	}
		
	// begin setup
	construct_building(IN_ROOM(ch), GET_CRAFT_BUILD_TYPE(type));
	set_room_extra_data(IN_ROOM(ch), ROOM_EXTRA_BUILD_RECIPE, GET_CRAFT_VNUM(type));
	
	special_building_setup(ch, IN_ROOM(ch));
	SET_BIT(ROOM_BASE_FLAGS(IN_ROOM(ch)), ROOM_AFF_INCOMPLETE);
	SET_BIT(ROOM_AFF_FLAGS(IN_ROOM(ch)), ROOM_AFF_INCOMPLETE);
	GET_BUILDING_RESOURCES(IN_ROOM(ch)) = copy_resource_list(GET_CRAFT_RESOURCES(type));
	
	// can_claim checks total available land, but the outside is check done within this block
	if (!ROOM_OWNER(IN_ROOM(ch)) && can_claim(ch) && !ROOM_AFF_FLAGGED(IN_ROOM(ch), ROOM_AFF_UNCLAIMABLE)) {
		if (e || (e = get_or_create_empire(ch))) {
			ter_type = get_territory_type_for_empire(IN_ROOM(ch), e, FALSE, &junk);
			if (EMPIRE_TERRITORY(e, ter_type) < land_can_claim(e, ter_type)) {
				claim_room(IN_ROOM(ch), e);
			}
		}
	}

	// closed buildings
	if (is_closed) {
		create_exit(IN_ROOM(ch), to_room, dir, FALSE);
		if (needs_reverse) {
			create_exit(IN_ROOM(ch), to_rev, rev_dir[dir], FALSE);
		}

		// entrance is the direction you type TO ENTER, so it's the reverse of the facing dir
		COMPLEX_DATA(IN_ROOM(ch))->entrance = rev_dir[dir];
		herd_animals_out(IN_ROOM(ch));
	}

	// take away the required item
	if (GET_CRAFT_REQUIRES_OBJ(type) != NOTHING && found_obj) {
		act("You use $p.", FALSE, ch, found_obj, 0, TO_CHAR);
		extract_obj(found_obj);
	}

	start_action(ch, ACT_BUILDING, 0);
	msg_to_char(ch, "You start to build %s %s!\r\n", AN(GET_CRAFT_NAME(type)), GET_CRAFT_NAME(type));
	sprintf(buf, "$n begins to build %s %s!", AN(GET_CRAFT_NAME(type)), GET_CRAFT_NAME(type));
	act(buf, FALSE, ch, 0, 0, TO_ROOM);
	
	// do a build action now
	process_build(ch, IN_ROOM(ch), ACT_BUILDING);
}


ACMD(do_dismantle) {
	craft_data *type;
	
	skip_spaces(&argument);
	if (*argument && !isname(arg, get_room_name(IN_ROOM(ch), FALSE))) {
		msg_to_char(ch, "Dismantle is only used to dismantle buildings. Just type 'dismantle'. (You get this error if you typed an argument.)\r\n");
		return;
	}

	if (IS_NPC(ch)) {
		msg_to_char(ch, "NPCs cannot use the dismantle command.\r\n");
		return;
	}
	
	if (GET_ACTION(ch) == ACT_DISMANTLING) {
		msg_to_char(ch, "You stop dismantling the building.\r\n");
		act("$n stops dismantling the building.", FALSE, ch, 0, 0, TO_ROOM);
		GET_ACTION(ch) = ACT_NONE;
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
	
	if (HOME_ROOM(IN_ROOM(ch)) != IN_ROOM(ch)) {
		msg_to_char(ch, "You need to dismantle from the main room.\r\n");
		return;
	}
	
	if (IS_BURNING(IN_ROOM(ch))) {
		msg_to_char(ch, "You can't dismantle a burning building!\r\n");
		return;
	}

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
		msg_to_char(ch, "You can't dismantle anything here.\r\n");
		return;
	}
	
	if (ROOM_AFF_FLAGGED(IN_ROOM(ch), ROOM_AFF_HAS_INSTANCE)) {
		msg_to_char(ch, "You can't dismantle this building because an adventure is currently linked here.\r\n");
		return;
	}

	if (!(type = find_building_list_entry(IN_ROOM(ch), FIND_BUILD_NORMAL))) {
		if (!(type = find_building_list_entry(IN_ROOM(ch), FIND_BUILD_UPGRADE))) {
			msg_to_char(ch, "You can't dismantle anything here.\r\n");
			return;
		}
	}

	if (ROOM_AFF_FLAGGED(IN_ROOM(ch), ROOM_AFF_UNCLAIMABLE)) {
		msg_to_char(ch, "You can't dismantle this building because the tile is unclaimable.\r\n");
		return;
	}

	if (!has_permission(ch, PRIV_BUILD, IN_ROOM(ch)) || !can_use_room(ch, IN_ROOM(ch), MEMBERS_ONLY)) {
		msg_to_char(ch, "You don't have permission to dismantle this building.\r\n");
		return;
	}

	if (GET_CRAFT_ABILITY(type) != NO_ABIL && !has_ability(ch, GET_CRAFT_ABILITY(type))) {
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
	
	half_chop(argument, arg, arg2);
	
	if (!ch->desc) {
		msg_to_char(ch, "You can't do that.\r\n");
	}
	else if (!has_player_tech(ch, PTECH_CUSTOMIZE_BUILDING)) {
		msg_to_char(ch, "You don't have the right ability to customize buildings.\r\n");
	}
	else if (!emp || ROOM_OWNER(IN_ROOM(ch)) != emp) {
		msg_to_char(ch, "You must own the tile to do this.\r\n");
	}
	else if (!IS_ANY_BUILDING(IN_ROOM(ch))) {
		msg_to_char(ch, "You can't customize here.\r\n");
	}
	else if (!has_permission(ch, PRIV_CUSTOMIZE, IN_ROOM(ch))) {
		msg_to_char(ch, "You are not allowed to customize.\r\n");
	}
	else if (is_abbrev(arg, "name")) {
		if (!*arg2) {
			msg_to_char(ch, "What would you like to name this room (or \"none\")?\r\n");
		}
		else if (!str_cmp(arg2, "none")) {
			if (ROOM_CUSTOM_NAME(IN_ROOM(ch))) {
				free(ROOM_CUSTOM_NAME(IN_ROOM(ch)));
				ROOM_CUSTOM_NAME(IN_ROOM(ch)) = NULL;
			}
			
			msg_to_char(ch, "This room no longer has a custom name.\r\n");
			command_lag(ch, WAIT_ABILITY);
		}
		else if (color_code_length(arg2) > 0) {
			msg_to_char(ch, "You cannot use color codes in custom room names.\r\n");
		}
		else if (strlen(arg2) > 60) {
			msg_to_char(ch, "That name is too long. Room names may not be over 60 characters.\r\n");
		}
		else {
			if (ROOM_CUSTOM_NAME(IN_ROOM(ch))) {
				free(ROOM_CUSTOM_NAME(IN_ROOM(ch)));
			}
			else {
				gain_player_tech_exp(ch, PTECH_CUSTOMIZE_BUILDING, 33.4);
			}
			ROOM_CUSTOM_NAME(IN_ROOM(ch)) = str_dup(arg2);
			
			msg_to_char(ch, "This room is now called \"%s\".\r\n", arg2);
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
			if (ROOM_CUSTOM_DESCRIPTION(IN_ROOM(ch))) {
				free(ROOM_CUSTOM_DESCRIPTION(IN_ROOM(ch)));
				ROOM_CUSTOM_DESCRIPTION(IN_ROOM(ch)) = NULL;
			}
			msg_to_char(ch, "This room no longer has a custom description.\r\n");
		}
		else if (is_abbrev(arg2, "set")) {
			if (!ROOM_CUSTOM_DESCRIPTION(IN_ROOM(ch))) {
				gain_player_tech_exp(ch, PTECH_CUSTOMIZE_BUILDING, 33.4);
			}
			start_string_editor(ch->desc, "room description", &(ROOM_CUSTOM_DESCRIPTION(IN_ROOM(ch))), MAX_ROOM_DESCRIPTION, TRUE);
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
	player_index_data *index;
	
	one_argument(argument, arg);
	
	if (!ROOM_BLD_FLAGGED(IN_ROOM(ch), BLD_DEDICATE) || !COMPLEX_DATA(IN_ROOM(ch))) {
		msg_to_char(ch, "You cannot dedicate anything here.\r\n");
	}
	else if (ROOM_PATRON(IN_ROOM(ch)) > 0) {
		msg_to_char(ch, "It is already dedicated.\r\n");
	}
	else if (!IS_COMPLETE(IN_ROOM(ch))) {
		msg_to_char(ch, "You must finish construction before dedicating it.\r\n");
	}
	else if (GET_LOYALTY(ch) != ROOM_OWNER(HOME_ROOM(IN_ROOM(ch)))) {
		msg_to_char(ch, "You can only dedicate it if you own it.\r\n");
	}
	else if (!has_permission(ch, PRIV_BUILD, IN_ROOM(ch)) || !can_use_room(ch, IN_ROOM(ch), MEMBERS_ONLY)) {
		msg_to_char(ch, "You don't have permission to dedicate buildings.\r\n");
	}
	else if (!*arg) {
		msg_to_char(ch, "Dedicate this building to whom?\r\n");
	}
	else if (!(index = find_player_index_by_name(arg))) {
		msg_to_char(ch, "You must specify a valid player to dedicate the building to.\r\n");
	}
	else {
		COMPLEX_DATA(IN_ROOM(ch))->patron = index->idnum;
		msg_to_char(ch, "You dedicate the building to %s!\r\n", index->fullname);
		sprintf(buf, "$n dedicates the building to %s!", index->fullname);
		act(buf, FALSE, ch, NULL, NULL, TO_ROOM);
	}
}


// Takes subcmd SCMD_DESIGNATE, SCMD_REDESIGNATE
ACMD(do_designate) {
	void add_room_to_vehicle(room_data *room, vehicle_data *veh);
	extern struct empire_territory_data *create_territory_entry(empire_data *emp, room_data *room);
	void delete_territory_npc(struct empire_territory_data *ter, struct empire_npc_data *npc);
	extern bld_data *get_building_by_name(char *name, bool room_only);
	void sort_world_table();
	
	struct empire_territory_data *ter;
	struct room_direction_data *ex;
	int dir = NO_DIR;
	room_data *new, *home = HOME_ROOM(IN_ROOM(ch));
	bld_data *bld, *next_bld;
	char_data *vict;
	bld_data *type;
	obj_data *obj;
	bool found;
	
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

	if (!IS_APPROVED(ch) && config_get_bool("build_approval")) {
		send_config_msg(ch, "need_approval_string");
	}
	else if (!*argument || !(type = find_designate_room_by_name(argument, valid_des_flags))) {
		if (*argument) {	// if any
			msg_to_char(ch, "You can't designate a '%s' here.\r\n", argument);
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
	else if (subcmd == SCMD_DESIGNATE && ((dir = parse_direction(ch, arg)) == NO_DIR || !(veh ? can_designate_dir_vehicle[dir] : can_designate_dir[dir]))) {
		msg_to_char(ch, "Invalid direction.\r\n");
		msg_to_char(ch, "Usage: %s <room>\r\n", subcmd == SCMD_REDESIGNATE ? "redesignate" : "designate <direction>");
	}
	else if (!has_permission(ch, PRIV_BUILD, IN_ROOM(ch)) || !can_use_room(ch, IN_ROOM(ch), MEMBERS_ONLY)) {
		msg_to_char(ch, "You don't have permission to designate rooms here.\r\n");
	}
	else if (subcmd == SCMD_DESIGNATE && IS_MAP_BUILDING(IN_ROOM(ch)) && dir != BUILDING_ENTRANCE(IN_ROOM(ch))) {
		msg_to_char(ch, "You may only designate %s from here.\r\n", dirs[get_direction_for_char(ch, BUILDING_ENTRANCE(IN_ROOM(ch)))]);
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
	else if (maxrooms <= 0) {
		msg_to_char(ch, "You can't designate here.\r\n");
	}
	else if (subcmd == SCMD_DESIGNATE && (ex = find_exit(IN_ROOM(ch), dir)) && ex->room_ptr) {
		msg_to_char(ch, "There is already a room that direction.\r\n");
	}
	else if (subcmd == SCMD_DESIGNATE && hasrooms >= maxrooms) {
		msg_to_char(ch, "There's no more free space.\r\n");
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
			
			remove_designate_objects(new);
			if (ROOM_OWNER(home) && (ter = find_territory_entry(ROOM_OWNER(home), new))) {
				while (ter->npcs) {
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
				++VEH_INSIDE_ROOMS(veh);
				COMPLEX_DATA(new)->vehicle = veh;
				add_room_to_vehicle(new, veh);
			}
			
			if (ROOM_OWNER(home)) {
				perform_claim_room(new, ROOM_OWNER(home));
			}
		}
		
		// add new objects
		switch (GET_BLD_VNUM(type)) {
			case RTYPE_BEDROOM: {
				if (ROOM_PRIVATE_OWNER(HOME_ROOM(IN_ROOM(ch))) != NOBODY) {
					obj_to_room((obj = read_object(o_HOME_CHEST, TRUE)), new);
					load_otrigger(obj);
				}
				break;
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
			
			for (vict = ROOM_PEOPLE(IN_ROOM(ch)); vict; vict = vict->next_in_room) {
				if (vict != ch && vict->desc) {
					sprintf(buf, "$n designates the %s %s as %s %s.", veh ? "area" : "room", dirs[get_direction_for_char(vict, dir)], AN(GET_BLD_NAME(type)), GET_BLD_NAME(type));
					act(buf, FALSE, ch, 0, vict, TO_VICT);
				}
			}
			
			// sort now just in case
			sort_world_table();
		}
		
		complete_wtrigger(new);
	}
}


ACMD(do_interlink) {
	extern int count_flagged_sect_between(bitvector_t sectf_bits, room_data *start, room_data *end, bool check_base_sect);
	
	char arg2[MAX_INPUT_LENGTH];
	room_vnum vnum;
	room_data *to_room;
	int dir;
	
	half_chop(argument, arg, arg2);
	
	if (!*arg || !*arg2) {
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
	else if (!ROOM_BLD_FLAGGED(HOME_ROOM(IN_ROOM(ch)), BLD_INTERLINK)) {
		msg_to_char(ch, "This building cannot be interlinked.\r\n");
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
	else if (!ROOM_BLD_FLAGGED(HOME_ROOM(to_room), BLD_INTERLINK)) {
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
	extern struct complex_room_data *init_complex_data();
	
	static struct resource_data *cost = NULL;
	sector_data *original_sect = SECT(IN_ROOM(ch));
	sector_data *check_sect = (ROOM_SECT_FLAGGED(IN_ROOM(ch), SECTF_IS_ROAD) ? BASE_SECT(IN_ROOM(ch)) : SECT(IN_ROOM(ch)));
	sector_data *road_sect = find_first_matching_sector(SECTF_IS_ROAD, NOBITS);
	struct resource_data *charged = NULL;
	
	if (!cost) {
		add_to_resource_list(&cost, RES_COMPONENT, CMP_ROCK, 20, 0);
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
	else if (SECT_FLAGGED(check_sect, SECTF_LAY_ROAD) && SECT_FLAGGED(check_sect, SECTF_ROUGH) && !has_ability(ch, ABIL_PATHFINDING)) {
		// rough requires Pathfinding
		msg_to_char(ch, "You don't have the skill to properly do that.\r\n");
	}
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
	}
	else if (!road_sect) {
		msg_to_char(ch, "Road data has not been set up for this game.\r\n");
	}
	else if (!ROOM_SECT_FLAGGED(IN_ROOM(ch), SECTF_LAY_ROAD))
		msg_to_char(ch, "You can't lay a road here!\r\n");
	else if (!has_resources(ch, cost, TRUE, TRUE)) {
		// sends own messages
	}
	else {
		extract_resources(ch, cost, TRUE, &charged);

		msg_to_char(ch, "You lay a road here.\r\n");
		act("$n lays a road here.", FALSE, ch, 0, 0, TO_ROOM);

		// skillup before sect change
		if (SECT_FLAGGED(check_sect, SECTF_ROUGH)) {
			gain_ability_exp(ch, ABIL_PATHFINDING, 15);
		}
				
		// change it over
		change_terrain(IN_ROOM(ch), GET_SECT_VNUM(road_sect));
		
		// preserve this for un-laying the road (disassociate_building)
		change_base_sector(IN_ROOM(ch), original_sect);
		
		// log charged resources
		if (charged) {
			if (!COMPLEX_DATA(IN_ROOM(ch))) {
				COMPLEX_DATA(IN_ROOM(ch)) = init_complex_data();
			}
			GET_BUILT_WITH(IN_ROOM(ch)) = charged;
		}
		
		if (ROOM_OWNER(IN_ROOM(ch))) {
			void deactivate_workforce_room(empire_data *emp, room_data *room);
			deactivate_workforce_room(ROOM_OWNER(IN_ROOM(ch)), IN_ROOM(ch));
		}
		
		command_lag(ch, WAIT_OTHER);
	}
}


ACMD(do_maintain) {
	if (GET_ACTION(ch) == ACT_MAINTENANCE) {
		act("You stop maintaining the building.", FALSE, ch, NULL, NULL, TO_CHAR);
		cancel_action(ch);
	}
	else if (!can_use_room(ch, IN_ROOM(ch), GUESTS_ALLOWED)) {
		msg_to_char(ch, "You can't perform maintenance here.\r\n");
	}
	else if (GET_ACTION(ch) != ACT_NONE) {
		msg_to_char(ch, "You're busy right now.\r\n");
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
	else if (!CAN_SEE_IN_DARK_ROOM(ch, IN_ROOM(ch))) {
		msg_to_char(ch, "It's too dark to maintain anything here.\r\n");
	}
	else {
		start_action(ch, ACT_MAINTENANCE, -1);
		act("You set to work maintaining the building.", FALSE, ch, NULL, NULL, TO_CHAR);
		act("$n begins maintaining the building.", FALSE, ch, NULL, NULL, TO_ROOM);
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
		REMOVE_BIT(ROOM_AFF_FLAGS(HOME_ROOM(IN_ROOM(ch))), ROOM_AFF_NO_DISMANTLE);
		REMOVE_BIT(ROOM_BASE_FLAGS(HOME_ROOM(IN_ROOM(ch))), ROOM_AFF_NO_DISMANTLE);
		msg_to_char(ch, "This building can now be dismantled.\r\n");
	}
	else {
		SET_BIT(ROOM_AFF_FLAGS(HOME_ROOM(IN_ROOM(ch))), ROOM_AFF_NO_DISMANTLE);
		SET_BIT(ROOM_BASE_FLAGS(HOME_ROOM(IN_ROOM(ch))), ROOM_AFF_NO_DISMANTLE);
		msg_to_char(ch, "This building can no longer be dismantled.\r\n");
	}
}


ACMD(do_paint) {
	obj_data *paint;
	
	one_argument(argument, arg);
	
	if (IS_NPC(ch)) {
		msg_to_char(ch, "Mobs can't paint.\r\n");
	}
	else if (!can_use_room(ch, IN_ROOM(ch), MEMBERS_ONLY) || ROOM_AFF_FLAGGED(IN_ROOM(ch), ROOM_AFF_UNCLAIMABLE)) {
		msg_to_char(ch, "You don't have permission to paint here.\r\n");
	}
	else if (!has_permission(ch, PRIV_CUSTOMIZE, IN_ROOM(ch))) {
		msg_to_char(ch, "You don't have permission to paint anything (customize).\r\n");
	}
	else if (!COMPLEX_DATA(IN_ROOM(ch)) || !IS_ANY_BUILDING(IN_ROOM(ch))) {
		msg_to_char(ch, "You can only paint buildings.\r\n");
	}
	else if (HOME_ROOM(IN_ROOM(ch)) != IN_ROOM(ch)) {
		msg_to_char(ch, "You need to be in the main entry room to paint the building.\r\n");
	}
	else if (!ROOM_IS_CLOSED(IN_ROOM(ch))) {
		msg_to_char(ch, "You can only paint an enclosed building.\r\n");
	}
	else if (!IS_COMPLETE(IN_ROOM(ch))) {
		msg_to_char(ch, "Finish the building before painting it.\r\n");
	}
	else if (IS_DISMANTLING(IN_ROOM(ch))) {
		msg_to_char(ch, "You can't paint a building that is being dismantled.\r\n");
	}
	else if (ROOM_BLD_FLAGGED(IN_ROOM(ch), BLD_NO_PAINT)) {
		msg_to_char(ch, "This building cannot be painted.\r\n");
	}
	else if (!*arg) {
		msg_to_char(ch, "Paint the building with what?\r\n");
	}
	else if (!(paint = get_obj_in_list_vis(ch, arg, ch->carrying))) {
		msg_to_char(ch, "You don't seem to have that paint with.\r\n");
	}
	else if (!IS_PAINT(paint)) {
		act("$p isn't paint!", FALSE, ch, paint, NULL, TO_CHAR);
	}
	else if (!consume_otrigger(paint, ch, OCMD_PAINT, NULL)) {
		return;	// check trigger
	}
	else {
		act("You use $p to paint the building!", FALSE, ch, paint, NULL, TO_CHAR);
		act("$n uses $p to paint the building!", FALSE, ch, paint, NULL, TO_ROOM);
		
		if (PRF_FLAGGED(ch, PRF_NO_PAINT)) {
			msg_to_char(ch, "Notice: You have no-paint toggled on, and won't be able to see the color.\r\n");
		}
		
		if (ROOM_PAINT_COLOR(IN_ROOM(ch)) == GET_PAINT_COLOR(paint)) {
			// same color -- brighten it
			SET_BIT(ROOM_AFF_FLAGS(IN_ROOM(ch)), ROOM_AFF_BRIGHT_PAINT);
			SET_BIT(ROOM_BASE_FLAGS(IN_ROOM(ch)), ROOM_AFF_BRIGHT_PAINT);
		}
		else {
			// different color -- remove bright
			REMOVE_BIT(ROOM_AFF_FLAGS(IN_ROOM(ch)), ROOM_AFF_BRIGHT_PAINT);
			REMOVE_BIT(ROOM_BASE_FLAGS(IN_ROOM(ch)), ROOM_AFF_BRIGHT_PAINT);
		}
		
		// update color
		COMPLEX_DATA(IN_ROOM(ch))->paint_color = GET_PAINT_COLOR(paint);
		
		command_lag(ch, WAIT_ABILITY);
		extract_obj(paint);
	}
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
		prettier_sprintbit(exit_bld_flags, bld_on_flags, buf);
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
		prettier_sprintbit(mountain_bld_flags, bld_on_flags, buf);
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
			prettier_sprintbit(mountain_bld_flags, bld_on_flags, buf);
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
			prettier_sprintbit(exit_bld_flags, bld_on_flags, buf);
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
	if (!can_use_room(ch, IN_ROOM(ch), MEMBERS_ONLY)) {
		msg_to_char(ch, "You don't have permission to remove the paint here.\r\n");
	}
	else if (!has_permission(ch, PRIV_CUSTOMIZE, IN_ROOM(ch))) {
		msg_to_char(ch, "You don't have permission to unpaint anything (customize).\r\n");
	}
	else if (HOME_ROOM(IN_ROOM(ch)) != IN_ROOM(ch)) {
		msg_to_char(ch, "You need to be in the main entry room to unpaint the building.\r\n");
	}
	else if (!ROOM_PAINT_COLOR(IN_ROOM(ch))) {
		msg_to_char(ch, "Nothing here is painted.\r\n");
	}
	
	else {
		act("You strip the paint from the building!", FALSE, ch, NULL, NULL, TO_CHAR);
		act("$n strips the paint from the building!", FALSE, ch, NULL, NULL, TO_ROOM);
		
		COMPLEX_DATA(IN_ROOM(ch))->paint_color = 0;
		REMOVE_BIT(ROOM_AFF_FLAGS(IN_ROOM(ch)), ROOM_AFF_BRIGHT_PAINT);
		REMOVE_BIT(ROOM_BASE_FLAGS(IN_ROOM(ch)), ROOM_AFF_BRIGHT_PAINT);
		
		command_lag(ch, WAIT_ABILITY);
	}
}


ACMD(do_upgrade) {
	craft_data *iter, *next_iter, *type;
	char valid_list[MAX_STRING_LENGTH];
	struct bld_relation *relat;
	bld_data *proto, *found_bld;
	int upgrade_count = 0;
	bool wait, room_wait;
	
	// determine what kind of upgrades are available here
	if (GET_BUILDING(IN_ROOM(ch))) {
		upgrade_count = count_bld_relations(GET_BUILDING(IN_ROOM(ch)), BLD_REL_UPGRADES_TO);
	}
	
	skip_spaces(&argument);
	
	if (IS_NPC(ch)) {
		msg_to_char(ch, "NPCs cannot use the upgrade command.\r\n");
	}
	else if (!IS_APPROVED(ch) && config_get_bool("build_approval")) {
		send_config_msg(ch, "need_approval_string");
	}
	else if (!GET_BUILDING(IN_ROOM(ch)) || upgrade_count == 0) {
		msg_to_char(ch, "You can't upgrade this.\r\n");
	}
	else if (!can_use_room(ch, IN_ROOM(ch), MEMBERS_ONLY)) {
		msg_to_char(ch, "You don't have permission to upgrade this building.\r\n");
	}
	else if (!has_permission(ch, PRIV_BUILD, IN_ROOM(ch))) {
		msg_to_char(ch, "You do not have permission to upgrade buildings.\r\n");
	}
	else if (GET_ACTION(ch) != ACT_NONE) {
		msg_to_char(ch, "You're already busy.\r\n");
	}
	else if (!IS_COMPLETE(IN_ROOM(ch))) {
		msg_to_char(ch, "You can't start to upgrade it until you finish the construction.\r\n");
	}
	else if (BUILDING_RESOURCES(IN_ROOM(ch))) {
		msg_to_char(ch, "The building needs some work before it can be upgraded.\r\n");
	}
	else {	// tentative success
		*valid_list = '\0';	// initialize
		found_bld = NULL;
		
		// attempt to figure out what we're upgrading to
		LL_FOREACH(GET_BLD_RELATIONS(GET_BUILDING(IN_ROOM(ch))), relat) {
			if (relat->type != BLD_REL_UPGRADES_TO || !(proto = building_proto(relat->vnum))) {
				continue;
			}
			
			// did we find a match?
			if (*argument && multi_isname(argument, GET_BLD_NAME(proto))) {
				found_bld = proto;
				break;
			}
			else if (upgrade_count == 1) {
				found_bld = proto;
				break;
			}
			
			// append to the valid list
			sprintf(valid_list + strlen(valid_list), "%s%s", (*valid_list ? ", " : ""), GET_BLD_NAME(proto));
		}
		
		if (!*argument && upgrade_count > 1) {
			msg_to_char(ch, "Upgrade it to what: %s\r\n", valid_list);
			return;
		}
		else if (!found_bld && !*valid_list) {
			msg_to_char(ch, "You can't seem to upgrade anything there.\r\n");
			return;
		}
		else if (!found_bld) {
			msg_to_char(ch, "You can't upgrade it to that. Valid options are: %s\r\n", valid_list);
			return;
		}
		
		// ok, we know it's upgradeable and they have permission... now locate the upgrade craft...
		type = NULL;
		HASH_ITER(hh, craft_table, iter, next_iter) {
			if (GET_CRAFT_TYPE(iter) == CRAFT_TYPE_BUILD && IS_SET(GET_CRAFT_FLAGS(iter), CRAFT_UPGRADE) && GET_CRAFT_BUILD_TYPE(iter) == GET_BLD_VNUM(found_bld)) {
				if (IS_IMMORTAL(ch) || !IS_SET(GET_CRAFT_FLAGS(iter), CRAFT_IN_DEVELOPMENT)) {
					type = iter;
					break;
				}
			}
		}
		
		if (!type) {
			msg_to_char(ch, "You don't know how to upgrade this building.\r\n");
		}
		else if (GET_CRAFT_ABILITY(type) != NO_ABIL && !has_ability(ch, GET_CRAFT_ABILITY(type))) {
			msg_to_char(ch, "You don't have the required ability to upgrade this building.\r\n");
		}
		else if (CRAFT_FLAGGED(type, CRAFT_LEARNED) && !has_learned_craft(ch, GET_CRAFT_VNUM(type))) {
			msg_to_char(ch, "You have not learned the correct recipe to upgrade this building.\r\n");
		}
		else if (IS_SET(GET_CRAFT_FLAGS(type), CRAFT_IN_CITY_ONLY) && !is_in_city_for_empire(IN_ROOM(ch), GET_LOYALTY(ch), TRUE, &wait) && !is_in_city_for_empire(IN_ROOM(ch), ROOM_OWNER(IN_ROOM(ch)), TRUE, &room_wait)) {
			msg_to_char(ch, "You can only upgrade this building in a city%s.\r\n", (wait || room_wait) ? " (this city was founded too recently)" : "");
		}
		else {
			// it's good!
			start_action(ch, ACT_BUILDING, 0);
			
			dismantle_wtrigger(IN_ROOM(ch), NULL, FALSE);
			detach_building_from_room(IN_ROOM(ch));
			attach_building_to_room(building_proto(GET_CRAFT_BUILD_TYPE(type)), IN_ROOM(ch), TRUE);
			set_room_extra_data(IN_ROOM(ch), ROOM_EXTRA_BUILD_RECIPE, GET_CRAFT_VNUM(type));
			SET_BIT(ROOM_BASE_FLAGS(IN_ROOM(ch)), ROOM_AFF_INCOMPLETE);
			SET_BIT(ROOM_AFF_FLAGS(IN_ROOM(ch)), ROOM_AFF_INCOMPLETE);
			GET_BUILDING_RESOURCES(IN_ROOM(ch)) = copy_resource_list(GET_CRAFT_RESOURCES(type));

			msg_to_char(ch, "You begin to upgrade the building.\r\n");
			act("$n starts upgrading the building.", FALSE, ch, 0, 0, TO_ROOM);
		}
	}
}

