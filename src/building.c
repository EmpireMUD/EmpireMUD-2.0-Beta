/* ************************************************************************
*   File: building.c                                      EmpireMUD 2.0b2 *
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
void process_build(char_data *ch, room_data *room);
void setup_building_resources(room_data *room, const Resource list[], bool half);
void setup_tunnel_entrance(char_data *ch, room_data *room, int dir);

// externs
extern bool can_claim(char_data *ch);
void delete_room_npcs(room_data *room, struct empire_territory_data *ter);
void free_complex_data(struct complex_room_data *data);
extern room_data *create_room();
void scale_item_to_level(obj_data *obj, int level);
void stop_room_action(room_data *room, int action, int chore);

// external vars
extern const char *bld_on_flags[];
extern const bool can_designate_dir[NUM_OF_DIRS];
extern const char *dirs[];
extern int rev_dir[];


 //////////////////////////////////////////////////////////////////////////////
//// BUILDING DATA ///////////////////////////////////////////////////////////

// these correspond to digits 0-9, must be 2 chars each, and \n-terminated
const char *interlink_codes[11] = { "AX", "RB",	"UN", "DD", "WZ", "FG", "VI", "QP", "MY", "XE", "\n" };


 //////////////////////////////////////////////////////////////////////////////
//// SPECIAL HANDLING ////////////////////////////////////////////////////////

/**
* Any special handling that must run when a building is completed.
*
* @param room_data *room The building location.
*/
static void special_building_completion(room_data *room) {
	extern const sector_vnum climate_default_sector[NUM_CLIMATES];
	struct room_direction_data *ex;
	room_data *new_room;
	
	switch (BUILDING_VNUM(room)) {
		case BUILDING_TAVERN: {
			new_room = create_room();
			attach_building_to_room(building_proto(RTYPE_STEALTH_HIDEOUT), new_room);
			COMPLEX_DATA(room)->inside_rooms++;
			
			ROOM_OWNER(new_room) = ROOM_OWNER(room);
			COMPLEX_DATA(new_room)->home_room = room;
	
			if ((ex = create_exit(room, new_room, DOWN, TRUE))) {
				SET_BIT(ex->exit_info, EX_ISDOOR | EX_CLOSED);
				ex->keyword = str_dup("trapdoor");
			}
			
			// back
			if ((ex = find_exit(new_room, UP))) {
				SET_BIT(ex->exit_info, EX_ISDOOR | EX_CLOSED);
				ex->keyword = str_dup("trapdoor");
			}
			break;
		}
		case BUILDING_SORCERER_TOWER: {
			new_room = create_room();
			attach_building_to_room(building_proto(RTYPE_SORCERER_TOWER), new_room);
			COMPLEX_DATA(room)->inside_rooms++;
			
			ROOM_OWNER(new_room) = ROOM_OWNER(room);
			COMPLEX_DATA(new_room)->home_room = room;
	
			create_exit(room, new_room, UP, TRUE);
			break;
		}
		case BUILDING_SWAMP_PLATFORM: {
			change_terrain(room, climate_default_sector[CLIMATE_TEMPERATE]);
			break;
		}
	}
}


/**
* Any special handling when a new building is set up (at start of build):
*
* @param char_data *ch The builder (OPTIONAL: for skill setup)
* @param room_data *room The location to set up.
*/
void special_building_setup(char_data *ch, room_data *room) {
	void init_mine(room_data *room, char_data *ch);
		
	// mine data
	if (ROOM_BLD_FLAGGED(room, BLD_MINE) && get_room_extra_data(room, ROOM_EXTRA_MINE_TYPE) == MINE_NOT_SET) {
		init_mine(room, ch);
	}
}


 //////////////////////////////////////////////////////////////////////////////
//// BUILDING HELPERS ////////////////////////////////////////////////////////

/**
* @param room_data *room The map location to check
* @param bitvector_t flags BLD_ON_x
* @return TRUE if valid, FALSE if not
*/
bool can_build_on(room_data *room, bitvector_t flags) {
	#define CLEAR_OPEN_BUILDING(r)	(IS_MAP_BUILDING(r) && ROOM_BLD_FLAGGED((r), BLD_OPEN) && !ROOM_BLD_FLAGGED((r), BLD_BARRIER) && !SECT_FLAGGED(ROOM_ORIGINAL_SECT(r), SECTF_FRESH_WATER | SECTF_OCEAN))

	return (
		IS_SET(GET_SECT_BUILD_FLAGS(SECT(room)), flags) || 
		(IS_SET(flags, BLD_FACING_OPEN_BUILDING) && CLEAR_OPEN_BUILDING(room))	// one special case -- any thoughts on a generic case? -pc
	);
}


/**
* makes sure to update territory, as roads "count as in city"
*
* @param char_data *ch The lay-er.
* @param room_data *room The location of the road.
*/
void check_lay_territory(char_data *ch, room_data *room) {
	empire_data *emp = GET_LOYALTY(ch);
	bool junk;
	
	if (emp && ROOM_OWNER(room) && !is_in_city_for_empire(room, emp, FALSE, &junk)) {
		read_empire_territory(emp);
	}
}


/**
* This merges a Resource list into another Resource list.
*
* @param Resource *combine_to The list to merge into.
* @param const Resource *combine_from The list to copy/merge.
*/
void combine_resources(Resource *combine_to, const Resource *combine_from) {
	int iter, pos;
	bool found;
	
	for (iter = 0; combine_from[iter].vnum != NOTHING; ++iter) {
		found = FALSE;
		
		for (pos = 0; combine_to[pos].vnum != NOTHING && !found; ++pos) {
			if (combine_to[pos].vnum == combine_from[iter].vnum) {
				found = TRUE;
				
				combine_to[pos].amount += combine_from[iter].amount;
			}
		}
		
		// did it find one to add to? if not, add to end
		if (!found) {
			combine_to[pos].vnum = combine_from[iter].vnum;
			combine_to[pos].amount = combine_from[iter].amount;
			
			// move up the end of the list
			combine_to[pos+1].vnum = NOTHING;
		}
	}
}


/**
* This function handles data for the final completion of a building on a
* map tile. It removes the to_build resources and.
*
* @param room_data *room The room to complete.
*/
void complete_building(room_data *room) {
	void herd_animals_out(room_data *location);
	
	struct building_resource_type *res, *temp;
	char_data *ch;
	empire_data *emp;
	
	// nothing to do if no building data
	if (!COMPLEX_DATA(room)) {
		return;
	}
	
	// stop builders
	stop_room_action(room, ACT_BUILDING, CHORE_BUILDING);
	
	// remove any remaining resource requirements
	while ((res = BUILDING_RESOURCES(room))) {
		REMOVE_FROM_LIST(res, GET_BUILDING_RESOURCES(room), next);
		free(res);
	}
	
	// SPECIAL HANDLING for building completion
	special_building_completion(room);
	
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
	
	// lastly
	if ((emp = ROOM_OWNER(room))) {
		read_empire_territory(emp);
	}
}


/**
* Sets up building data for a map location.
*
* @param room_data *room Where to set up the building.
* @param bld_vnum type The building vnum to set up.
*/
void construct_building(room_data *room, bld_vnum type) {
	sector_data *sect;
	
	if (GET_ROOM_VNUM(room) >= MAP_SIZE) {
		log("SYSERR: construct_building called on location off the map");
		return;
	}
	
	disassociate_building(room);	// just in case
	
	sect = SECT(room);
	change_terrain(room, config_get_int("default_building_sect"));
	ROOM_ORIGINAL_SECT(room) = sect;
	
	// set actual data
	attach_building_to_room(building_proto(type), room);
	
	SET_BIT(ROOM_BASE_FLAGS(room), BLD_BASE_AFFECTS(room));
	SET_BIT(ROOM_AFF_FLAGS(room), BLD_BASE_AFFECTS(room));
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
	Resource resources[4] = { { o_LOG, 12 }, { o_LUMBER, 8 }, { o_NAILS, 4 }, END_RESOURCE_LIST };
	
	room_data *new_room, *last_room = entrance, *to_room;
	int iter;

	// entrance
	setup_tunnel_entrance(ch, entrance, dir);
	setup_building_resources(entrance, resources, FALSE);
	create_exit(entrance, IN_ROOM(ch), rev_dir[dir], FALSE);

	// exit
	setup_tunnel_entrance(ch, exit, rev_dir[dir]);
	setup_building_resources(exit, resources, FALSE);
	to_room = real_shift(exit, shift_dir[dir][0], shift_dir[dir][1]);
	create_exit(exit, to_room, dir, FALSE);

	// now the length of the tunnel
	for (iter = 0; iter < length; ++iter) {
		new_room = create_room();
		attach_building_to_room(building_proto(RTYPE_TUNNEL), new_room);
		ROOM_OWNER(new_room) = ROOM_OWNER((iter <= length/2) ? entrance : exit);
		COMPLEX_DATA(new_room)->home_room = (iter <= length/2) ? entrance : exit;
		setup_building_resources(new_room, resources, FALSE);

		create_exit(last_room, new_room, dir, TRUE);
		
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
	extern struct instance_data *find_instance_by_room(room_data *room);
	void remove_designate_objects(room_data *room);
	
	room_data *iter, *next_iter;
	struct instance_data *inst;
	bool deleted = FALSE;
	
	// delete any open instance here
	if (ROOM_AFF_FLAGGED(room, ROOM_AFF_HAS_INSTANCE) && (inst = find_instance_by_room(room))) {
		SET_BIT(inst->flags, INST_COMPLETED);
	}
	
	delete_room_npcs(room, NULL);
	
	// remove bits including dismantle
	REMOVE_BIT(ROOM_BASE_FLAGS(room), ROOM_AFF_DISMANTLING | ROOM_AFF_TEMPORARY | ROOM_AFF_HAS_INSTANCE | ROOM_AFF_CHAMELEON | ROOM_AFF_NO_FLY | ROOM_AFF_NO_DISMANTLE | ROOM_AFF_NO_DISREPAIR);
	REMOVE_BIT(ROOM_AFF_FLAGS(room), ROOM_AFF_DISMANTLING | ROOM_AFF_TEMPORARY | ROOM_AFF_HAS_INSTANCE | ROOM_AFF_CHAMELEON | ROOM_AFF_NO_FLY | ROOM_AFF_NO_DISMANTLE | ROOM_AFF_NO_DISREPAIR);
	
	// TODO should do an affect-total here in case any of those were also added by an affect?

	// free up the customs
	decustomize_room(room);

	// restore sect
	SECT(room) = ROOM_ORIGINAL_SECT(room);
	
	if (COMPLEX_DATA(room)) {
		COMPLEX_DATA(room)->home_room = NULL;
	}
	
	// some extra data safely clears now
	remove_room_extra_data(room, ROOM_EXTRA_RUINS_ICON);
	remove_room_extra_data(room, ROOM_EXTRA_GARDEN_WORKFORCE_PROGRESS);
	remove_room_extra_data(room, ROOM_EXTRA_QUARRY_WORKFORCE_PROGRESS);
	remove_room_extra_data(room, ROOM_EXTRA_TAVERN_TYPE);
	remove_room_extra_data(room, ROOM_EXTRA_TAVERN_BREWING_TIME);
	remove_room_extra_data(room, ROOM_EXTRA_TAVERN_AVAILABLE_TIME);
	remove_room_extra_data(room, ROOM_EXTRA_BUILD_RECIPE);
	remove_room_extra_data(room, ROOM_EXTRA_FOUND_TIME);

	// disassociate inside rooms
	HASH_ITER(interior_hh, interior_world_table, iter, next_iter) {
		if (HOME_ROOM(iter) == room && iter != room) {
			remove_designate_objects(iter);
			
			// move people and contents
			while (ROOM_PEOPLE(iter)) {
				if (!IS_NPC(ROOM_PEOPLE(iter))) {
					GET_LAST_DIR(ROOM_PEOPLE(iter)) = NO_DIR;
				}
				char_to_room(ROOM_PEOPLE(iter), room);
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
* @param bld_data *bb The building to search for.
* @return bld_data* The building that upgrades to it, or NULL if there isn't one.
*/
bld_data *find_upgraded_from(bld_data *bb) {
	bld_data *iter, *next_iter;
	
	HASH_ITER(hh, building_table, iter, next_iter) {
		if (GET_BLD_UPGRADES_TO(iter) == GET_BLD_VNUM(bb)) {
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
				else if (GET_SKILL(ch, SKILL_EMPIRE) < EMPIRE_CHORE_SKILL_CAP) {
					gain_skill_exp(c, SKILL_EMPIRE, 3);
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
			newobj = read_object(GET_CRAFT_REQUIRES_OBJ(type));
			
			// scale item to minimum level
			scale_item_to_level(newobj, 0);
			
			if (IS_NPC(ch)) {
				obj_to_room(newobj, room);
			}
			else {
				obj_to_char_or_room(newobj, ch);
				act("You get $p.", FALSE, ch, newobj, 0, TO_CHAR);
			}
			load_otrigger(newobj);
		}
	}
			
	disassociate_building(room);
}


/**
* This pushes animals out of a building.
*
* @param room_data *location The building's tile.
*/
void herd_animals_out(room_data *location) {
	extern int perform_move(char_data *ch, int dir, int need_specials_check, byte mode);
	
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
				if ((!to_room || IS_WATER_SECT(SECT(to_room))) && to_reverse && ROOM_BLD_FLAGGED(location, BLD_TWO_ENTRANCES)) {
					if (perform_move(ch_iter, BUILDING_ENTRANCE(location), TRUE, 0)) {
						found_any = TRUE;
					}
				}
				else if (to_room) {
					if (perform_move(ch_iter, rev_dir[BUILDING_ENTRANCE(location)], TRUE, 0)) {
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
*/
void process_build(char_data *ch, room_data *room) {
	craft_data *type = find_building_list_entry(room, FIND_BUILD_NORMAL);
	obj_data *obj, *found_obj = NULL;
	bool found = FALSE;
	struct building_resource_type *res, *temp;

	if ((res = BUILDING_RESOURCES(room))) {
		// check inventory
		for (obj = ch->carrying; obj && !found; obj = obj->next_content) {
			if (GET_OBJ_VNUM(obj) == res->vnum) {
				found = TRUE;
				found_obj = obj;
			}
		}

		// check room (if !found)
		for (obj = ROOM_CONTENTS(room); obj && !found; obj = obj->next_content) {
			if (GET_OBJ_VNUM(obj) == res->vnum) {
				found = TRUE;
				found_obj = obj;
			}
		}
		
		// did we find one?
		if (found) {
			res->amount -= 1;
		}
		
		// check zero-res whether or not we found anything
		if (res->amount <= 0) {
			// remove the resource entry entirely
			REMOVE_FROM_LIST(res, GET_BUILDING_RESOURCES(room), next);
		}
	}

	// still working?
	if (!IS_COMPLETE(room)) {
		if (!found || !found_obj) {
			msg_to_char(ch, "You run out of resources and stop building.\r\n");
			act("$n runs out of resources and stops building.", FALSE, ch, 0, 0, TO_ROOM);
			GET_ACTION(ch) = ACT_NONE;
			return;
		}

		// messaging
		if (has_custom_message(found_obj, OBJ_CUSTOM_BUILD_TO_CHAR)) {
			act(get_custom_message(found_obj, OBJ_CUSTOM_BUILD_TO_CHAR), FALSE, ch, found_obj, 0, TO_CHAR | TO_SPAMMY);
		}
		else {
			act("You carefully place $p in the structure.", FALSE, ch, found_obj, 0, TO_CHAR | TO_SPAMMY);
		}
		
		if (has_custom_message(found_obj, OBJ_CUSTOM_BUILD_TO_ROOM)) {
			act(get_custom_message(found_obj, OBJ_CUSTOM_BUILD_TO_ROOM), FALSE, ch, found_obj, 0, TO_ROOM | TO_SPAMMY);
		}
		else {
			act("$n places $p carefully in the structure.", FALSE, ch, found_obj, 0, TO_ROOM | TO_SPAMMY);
		}
		
		// reset disrepair and damage
		COMPLEX_DATA(room)->disrepair = 0;
		COMPLEX_DATA(room)->damage = 0;
		
		// skillups
		if (type && GET_CRAFT_ABILITY(type) != NO_ABIL) {
			gain_ability_exp(ch, GET_CRAFT_ABILITY(type), 3);
		}
		else if (GET_SKILL(ch, SKILL_EMPIRE) < EMPIRE_CHORE_SKILL_CAP) {
			gain_skill_exp(ch, SKILL_EMPIRE, 3);
		}			
	}

	// extract either way
	if (found && found_obj) {
		extract_obj(found_obj);
	}

	// now finished?
	if (IS_COMPLETE(room)) {
		finish_building(ch, room);
	}
}


/**
* Performs 1 step of the dismantle.
*
* @param char_data *ch The dismantler.
* @param room_data *room The location being dismantled.
*/
void process_dismantling(char_data *ch, room_data *room) {
	obj_data *obj = NULL;
	struct building_resource_type *res, *temp;
	empire_data *emp;

	// sometimes zeroes end up in here ... just clear them
	while (BUILDING_RESOURCES(room) && !obj) {
		// find last entry, remove first
		res = BUILDING_RESOURCES(room);
		while (res->next) {
			res = res->next;
		}
		
		if (res->amount > 0) {
			obj = read_object(res->vnum);
			res->amount -= 1;
		}
			
		if (res->amount <= 0) {
			// remove the resource entry entirely
			REMOVE_FROM_LIST(res, GET_BUILDING_RESOURCES(room), next);
		}
	}

	if (obj) {
		// scale item to minimum level
		scale_item_to_level(obj, 0);
		obj_to_char_or_room(obj, ch);
		
		act("$n removes $p from the structure.", FALSE, ch, obj, 0, TO_ROOM | TO_SPAMMY);
		act("You carefully remove $p from the structure.", FALSE, ch, obj, 0, TO_CHAR | TO_SPAMMY);
		load_otrigger(obj);
	}

	if (IS_COMPLETE(room)) {
		finish_dismantle(ch, room);
		
		if ((emp = ROOM_OWNER(room))) {
			read_empire_territory(emp);
		}
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
			case RTYPE_STUDY: {
				if (GET_OBJ_VNUM(o) == BOARD_MORT) {
					extract_obj(o);
				}
				break;
			}
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
* This attaches the resources to a room, which puts it in an incomplete
* state.
*
* @param room_data *room The location.
* @param Resource list[] list of standard resources
* @param bool half if TRUE cuts amount in half (e.g. dismantle)
*/
void setup_building_resources(room_data *room, const Resource list[], bool half) {
	struct building_resource_type *res, *end;
	int div = (half ? 2 : 1);
	int iter;
	
	end = GET_BUILDING_RESOURCES(room);
	while (end && end->next) {
		end = end->next;
	}
	
	for (iter = 0; list[iter].vnum != NOTHING; ++iter) {
		if (list[iter].amount/div > 0) {
			CREATE(res, struct building_resource_type, 1);
			res->vnum = list[iter].vnum;
			res->amount = list[iter].amount/div;
			
			// apply to end
			if (end) {
				end->next = res;
				end = res;
			}
			else {
				GET_BUILDING_RESOURCES(room) = res;
				end = res;
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
	bool junk;
	
	construct_building(room, BUILDING_TUNNEL);
		
	SET_BIT(ROOM_BASE_FLAGS(room), tunnel_flags);
	SET_BIT(ROOM_AFF_FLAGS(room), tunnel_flags);
	COMPLEX_DATA(room)->entrance = dir;
	if (emp && can_claim(ch) && !ROOM_AFF_FLAGGED(room, ROOM_AFF_UNCLAIMABLE)) {
		if (EMPIRE_OUTSIDE_TERRITORY(emp) < land_can_claim(emp, TRUE) || COUNTS_AS_IN_CITY(room) || is_in_city_for_empire(room, emp, FALSE, &junk)) {
			ROOM_OWNER(room) = emp;
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
	Resource composite_resources[MAX_RESOURCES_REQUIRED*2] = { END_RESOURCE_LIST };
	struct building_resource_type *res, *next_res, *findres, *temp;
	bool deleted = FALSE, found, upgraded = FALSE;
	room_data *room, *next_room;
	char_data *targ, *next_targ;
	craft_data *type, *up_type;
	obj_data *obj, *next_obj;
	bld_data *up_bldg;
	int iter;
	
	if (!IS_MAP_BUILDING(loc)) {
		log("SYSERR: Attempting to dismantle non-building room #%d", GET_ROOM_VNUM(loc));
		return;
	}
	
	if (!(type = find_building_list_entry(loc, FIND_BUILD_NORMAL))) {
		if ((type = find_building_list_entry(loc, FIND_BUILD_UPGRADE))) {
			upgraded = TRUE;
		}
		else {
			log("SYSERR: Attempting to dismantle non-dismantlable building at #%d", GET_ROOM_VNUM(loc));
			return;
		}
	}

	// interior only
	HASH_ITER(interior_hh, interior_world_table, room, next_room) {
		if (HOME_ROOM(room) == loc) {
			remove_designate_objects(room);
			delete_room_npcs(room, NULL);
		
			for (obj = ROOM_CONTENTS(room); obj; obj = next_obj) {
				next_obj = obj->next_content;
				obj_to_room(obj, loc);
			}
			for (targ = ROOM_PEOPLE(loc); targ; targ = next_targ) {
				next_targ = targ->next_in_room;
				if (!IS_NPC(targ)) {
					GET_LAST_DIR(targ) = NO_DIR;
				}
				char_from_room(targ);
				char_to_room(targ, loc);
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
	
	// prepare resources:
	combine_resources(composite_resources, GET_CRAFT_RESOURCES(type));
	if (upgraded) {
		up_bldg = find_upgraded_from(building_proto(GET_CRAFT_BUILD_TYPE(type)));
		up_type = find_build_craft(GET_BLD_VNUM(up_bldg));
		
		while (up_bldg && up_type) {
			combine_resources(composite_resources, GET_CRAFT_RESOURCES(up_type));
			
			up_bldg = find_upgraded_from(up_bldg);
			up_type = find_build_craft(GET_BLD_VNUM(up_bldg));
		}
	}

	if (!IS_COMPLETE(loc)) {
		// room was already started, so dismantle is more complicated
	
		// iterate over all building resources
		for (iter = 0; composite_resources[iter].vnum != NOTHING; ++iter) {
		
			// find out if there's already an entry for this resource
			found = FALSE;
			for (findres = BUILDING_RESOURCES(loc); findres; findres = findres->next) {
				if (findres->vnum == composite_resources[iter].vnum) {
					found = TRUE;
					// new amount is the difference between the original and the remaining, divided by 2
					findres->amount = MAX(0, (composite_resources[iter].amount - findres->amount) / 2);
				}
			}
			
			// didn't find, so they already built that entire resource -- add it at half
			if (!found && composite_resources[iter].amount/2 > 0) {
				CREATE(res, struct building_resource_type, 1);
				res->vnum = composite_resources[iter].vnum;
				res->amount = composite_resources[iter].amount / 2;
				
				res->next = GET_BUILDING_RESOURCES(loc);
				GET_BUILDING_RESOURCES(loc) = res;
			}
		}
		
		// check for things that now require 0 and remove them
		for (res = GET_BUILDING_RESOURCES(loc); res; res = next_res) {
			next_res = res->next;
			
			if (res->amount <= 0) {
				REMOVE_FROM_LIST(res, GET_BUILDING_RESOURCES(loc), next);
				free(res);
			}
		}
	}
	else {
		// simple setup
		setup_building_resources(loc, composite_resources, TRUE);
	}

	SET_BIT(ROOM_AFF_FLAGS(loc), ROOM_AFF_DISMANTLING);
	SET_BIT(ROOM_BASE_FLAGS(loc), ROOM_AFF_DISMANTLING);
	delete_room_npcs(loc, NULL);
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
	extern int get_crafting_level(char_data *ch);
	
	room_data *to_room = NULL, *to_rev = NULL;
	obj_data *found_obj = NULL;
	empire_data *e = NULL, *emp;
	int dir = NORTH;
	craft_data *iter, *next_iter, *type = NULL, *abbrev_match = NULL;
	bool found = FALSE, found_any, this_line, is_closed, needs_facing, needs_reverse;
	bool junk, wait;
	
	// simple rules for ch building a given craft
	#define CHAR_CAN_BUILD(ch, ttype)  (GET_CRAFT_TYPE((ttype)) == CRAFT_TYPE_BUILD && !IS_SET(GET_CRAFT_FLAGS((ttype)), CRAFT_UPGRADE | CRAFT_DISMANTLE_ONLY) && (IS_IMMORTAL(ch) || !IS_SET(GET_CRAFT_FLAGS((ttype)), CRAFT_IN_DEVELOPMENT)) && (GET_CRAFT_ABILITY((ttype)) == NO_ABIL || HAS_ABILITY((ch), GET_CRAFT_ABILITY((ttype)))))
	
	if (IS_NPC(ch)) {
		msg_to_char(ch, "NPCs cannot use the build command.\r\n");
		return;
	}
	
	argument = any_one_word(argument, arg);
	skip_spaces(&argument);
	
	// this figures out if the argument was a build recipe
	if (*arg) {
		HASH_ITER(sorted_hh, sorted_crafts, iter, next_iter) {
			if (CHAR_CAN_BUILD(ch, iter)) {
				if (!str_cmp(arg, GET_CRAFT_NAME(iter))) {
					type = iter;
					break;
				}
				else if (!abbrev_match && is_abbrev(arg, GET_CRAFT_NAME(iter))) {
					abbrev_match = iter;
				}
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

	if (!*arg || !type) {
		/* Cancel building */
		if (GET_ACTION(ch) == ACT_BUILDING) {
			msg_to_char(ch, "You stop building.\r\n");
			act("$n stops building.", FALSE, ch, 0, 0, TO_ROOM);
			GET_ACTION(ch) = ACT_NONE;
		}

		/* Continue building */
		else if (!IS_COMPLETE(IN_ROOM(ch))) {
			if (IS_DISMANTLING(IN_ROOM(ch)))
				msg_to_char(ch, "The building is being dismantled, you can't rebuild it now.\r\n");
			else if (GET_ACTION(ch) != ACT_NONE)
				msg_to_char(ch, "You're kinda busy right now.\r\n");
			else if (BUILDING_BURNING(IN_ROOM(ch))) {
				msg_to_char(ch, "You can't work on a burning building!\r\n");
			}
			else {
				start_action(ch, ACT_BUILDING, 0, NOBITS);
				msg_to_char(ch, "You start building.\r\n");
				act("$n starts building.", FALSE, ch, 0, 0, TO_ROOM);
			}
		}

		/* Send output */
		else {
			msg_to_char(ch, "Usage: build <structure> [direction]\r\n");
			msg_to_char(ch, "You know how to build:\r\n");
			this_line = FALSE;
			found_any = FALSE;
			*buf = '\0';
		
			HASH_ITER(sorted_hh, sorted_crafts, iter, next_iter) {
				if (CHAR_CAN_BUILD(ch, iter)) {
					// only display if they have the required object
					if (GET_CRAFT_REQUIRES_OBJ(iter) != NOTHING && !get_obj_in_list_vnum(GET_CRAFT_REQUIRES_OBJ(iter), ch->carrying)) {
						continue;
					}

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
	else if (GET_CRAFT_ABILITY(type) != NO_ABIL && !HAS_ABILITY(ch, GET_CRAFT_ABILITY(type))) {
		msg_to_char(ch, "You don't have the skill to erect that structure.\r\n");
	}
	else if (GET_CRAFT_MIN_LEVEL(type) > get_crafting_level(ch)) {
		msg_to_char(ch, "You need to have a crafting level of %d to build that.\r\n", GET_CRAFT_MIN_LEVEL(type));
	}
	else if (ROOM_AFF_FLAGGED(IN_ROOM(ch), ROOM_AFF_UNCLAIMABLE | ROOM_AFF_HAS_INSTANCE)) {
		msg_to_char(ch, "You can't build here.\r\n");
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
	else if (!has_permission(ch, PRIV_BUILD)) {
		msg_to_char(ch, "You don't have permission to build anything.\r\n");
	}
	else if (!can_use_room(ch, IN_ROOM(ch), MEMBERS_ONLY) || ROOM_AFF_FLAGGED(IN_ROOM(ch), ROOM_AFF_UNCLAIMABLE)) {
		msg_to_char(ch, "You don't have permission to build here.\r\n");
	}
	else if (!can_build_on(IN_ROOM(ch), GET_CRAFT_BUILD_ON(type))) {
		prettier_sprintbit(GET_CRAFT_BUILD_ON(type), bld_on_flags, buf);
		msg_to_char(ch, "You need to build on: %s\r\n", buf);
	}
	else if (GET_CRAFT_BUILD_TYPE(type) == NOTHING || !building_proto(GET_CRAFT_BUILD_TYPE(type))) {
		msg_to_char(ch, "That build recipe is not implemented.\r\n");
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
	setup_building_resources(IN_ROOM(ch), GET_CRAFT_RESOURCES(type), FALSE);
	
	// can_claim checks total available land, but the outside is check done within this block
	if (can_claim(ch) && !ROOM_AFF_FLAGGED(IN_ROOM(ch), ROOM_AFF_UNCLAIMABLE)) {
		if (e || (e = get_or_create_empire(ch))) {
			if (EMPIRE_OUTSIDE_TERRITORY(e) < land_can_claim(e, TRUE) || COUNTS_AS_IN_CITY(IN_ROOM(ch)) || is_in_city_for_empire(IN_ROOM(ch), e, FALSE, &junk)) {
				ROOM_OWNER(IN_ROOM(ch)) = e;
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

	start_action(ch, ACT_BUILDING, 0, NOBITS);
	msg_to_char(ch, "You start to build %s %s!\r\n", AN(GET_CRAFT_NAME(type)), GET_CRAFT_NAME(type));
	sprintf(buf, "$n begins to build %s %s!", AN(GET_CRAFT_NAME(type)), GET_CRAFT_NAME(type));
	act(buf, FALSE, ch, 0, 0, TO_ROOM);
	
	// do a build action now
	process_build(ch, IN_ROOM(ch));
	
	// lastly
	if ((emp = ROOM_OWNER(IN_ROOM(ch)))) {
		read_empire_territory(emp);
	}
}


ACMD(do_dismantle) {
	craft_data *type;

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

	if (GET_ACTION(ch) != ACT_NONE) {
		msg_to_char(ch, "You're kinda busy right now.\r\n");
		return;
	}
	
	if (HOME_ROOM(IN_ROOM(ch)) != IN_ROOM(ch)) {
		msg_to_char(ch, "You need to dismantle from the main room.\r\n");
		return;
	}
	
	if (BUILDING_BURNING(IN_ROOM(ch))) {
		msg_to_char(ch, "You can't dismantle a burning building!\r\n");
		return;
	}

	if (IS_DISMANTLING(IN_ROOM(ch))) {
		msg_to_char(ch, "You begin to dismantle the building.\r\n");
		act("$n begins to dismantle the building.", FALSE, ch, 0, 0, TO_ROOM);
		start_action(ch, ACT_DISMANTLING, 0, NOBITS);
		return;
	}
	
	if (!COMPLEX_DATA(IN_ROOM(ch))) {
		msg_to_char(ch, "You can't dismantle anything here.\r\n");
		return;
	}
	
	if (ROOM_AFF_FLAGGED(IN_ROOM(ch), ROOM_AFF_HAS_INSTANCE)) {
		msg_to_char(ch, "You can't dismantle this building.\r\n");
		return;
	}

	if (!(type = find_building_list_entry(IN_ROOM(ch), FIND_BUILD_NORMAL))) {
		if (!(type = find_building_list_entry(IN_ROOM(ch), FIND_BUILD_UPGRADE))) {
			msg_to_char(ch, "You can't dismantle anything here.\r\n");
			return;
		}
	}

	if (ROOM_AFF_FLAGGED(IN_ROOM(ch), ROOM_AFF_UNCLAIMABLE)) {
		msg_to_char(ch, "You can't dismantle this.\r\n");
		return;
	}

	if (!has_permission(ch, PRIV_BUILD) || !can_use_room(ch, IN_ROOM(ch), MEMBERS_ONLY)) {
		msg_to_char(ch, "You don't have permission to dismantle this building.\r\n");
		return;
	}

	if (GET_CRAFT_ABILITY(type) != NO_ABIL && !HAS_ABILITY(ch, GET_CRAFT_ABILITY(type))) {
		msg_to_char(ch, "You don't have the skill needed to dismantle this building properly.\r\n");
		return;
	}

	if (ROOM_AFF_FLAGGED(IN_ROOM(ch), ROOM_AFF_NO_DISMANTLE)) {
		msg_to_char(ch, "You can't dismantle this building (use 'nodismantle' to toggle).\r\n");
		return;
	}
	
	start_dismantle_building(IN_ROOM(ch));
	if (ROOM_OWNER(IN_ROOM(ch))) {
		read_empire_territory(ROOM_OWNER(IN_ROOM(ch)));
	}
	start_action(ch, ACT_DISMANTLING, 0, NOBITS);
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
	else if (!HAS_ABILITY(ch, ABIL_CUSTOMIZE_BUILDING)) {
		msg_to_char(ch, "You must purchase Customize Building to do that.\r\n");
	}
	else if (!emp || ROOM_OWNER(IN_ROOM(ch)) != emp) {
		msg_to_char(ch, "You must own the tile to do this.\r\n");
	}
	else if (!IS_ANY_BUILDING(IN_ROOM(ch))) {
		msg_to_char(ch, "You can't customize here.\r\n");
	}
	else if (!has_permission(ch, PRIV_CUSTOMIZE)) {
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
		}
		else if (count_color_codes(arg2) > 0) {
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
				gain_ability_exp(ch, ABIL_CUSTOMIZE_BUILDING, 33.4);
			}
			ROOM_CUSTOM_NAME(IN_ROOM(ch)) = str_dup(arg2);
			
			msg_to_char(ch, "This room is now called \"%s\".\r\n", arg2);
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
				gain_ability_exp(ch, ABIL_CUSTOMIZE_BUILDING, 33.4);
			}
			start_string_editor(ch->desc, "room description", &(ROOM_CUSTOM_DESCRIPTION(IN_ROOM(ch))), MAX_ROOM_DESCRIPTION);
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
	int idnum;
	
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
	else if (!has_permission(ch, PRIV_BUILD) || !can_use_room(ch, IN_ROOM(ch), MEMBERS_ONLY)) {
		msg_to_char(ch, "You don't have permission to dedicate buildings.\r\n");
	}
	else if (!*arg) {
		msg_to_char(ch, "Dedicate this building to whom?\r\n");
	}
	else if ((idnum = get_id_by_name(arg)) <= 0) {
		msg_to_char(ch, "You must specify a valid player to dedicate the building to.\r\n");
	}
	else {
		COMPLEX_DATA(IN_ROOM(ch))->patron = idnum;
		msg_to_char(ch, "You dedicate the building to %s!\r\n", get_name_by_id(ROOM_PATRON(IN_ROOM(ch))) ? CAP(get_name_by_id(ROOM_PATRON(IN_ROOM(ch)))) : "a Former God");
		sprintf(buf, "$n dedicates the building to %s!", get_name_by_id(ROOM_PATRON(IN_ROOM(ch))) ? CAP(get_name_by_id(ROOM_PATRON(IN_ROOM(ch)))) : "a Former God");
		act(buf, FALSE, ch, NULL, NULL, TO_ROOM);
	}
}


// Takes subcmd SCMD_DESIGNATE, SCMD_REDESIGNATE
ACMD(do_designate) {
	extern bld_data *get_building_by_name(char *name, bool room_only);
	void sort_world_table();
	
	struct room_direction_data *ex;
	int dir = NO_DIR;
	room_data *new, *home = HOME_ROOM(IN_ROOM(ch));
	bld_data *bld, *next_bld;
	char_data *vict;
	bld_data *type;
	obj_data *obj;
	bool found;

	/*
	 * arg = direction (designate only)
	 * argument = room type
	 */
	
	if (subcmd == SCMD_DESIGNATE) {
		argument = one_argument(argument, arg);
	}

	skip_spaces(&argument);

	if (!*argument || !(type = get_building_by_name(argument, TRUE))) {
		msg_to_char(ch, "Usage: %s <room>\r\n", (subcmd == SCMD_REDESIGNATE) ? "redesignate" : "designate <direction>");

		if (!ROOM_IS_CLOSED(IN_ROOM(ch)) && (subcmd == SCMD_DESIGNATE) && GET_INSIDE_ROOMS(home) >= BLD_MAX_ROOMS(IN_ROOM(ch))) {
			msg_to_char(ch, "You can't designate any new rooms here.\r\n");
		}
		else {
			msg_to_char(ch, "Valid rooms are: ");
		
			found = FALSE;
			HASH_ITER(hh, building_table, bld, next_bld) {
				if (IS_SET(GET_BLD_FLAGS(bld), BLD_ROOM) && GET_BLD_DESIGNATE_FLAGS(bld) != NOBITS) {
					if (!ROOM_IS_CLOSED(IN_ROOM(ch)) || BLD_DESIGNATE_FLAGGED(home, GET_BLD_DESIGNATE_FLAGS(bld))) {
						msg_to_char(ch, "%s%s", (found ? ", " : ""), GET_BLD_NAME(bld));
						found = TRUE;
					}
				}
			}
		
			msg_to_char(ch, "\r\n");
		}
	}
	else if (!ROOM_IS_CLOSED(IN_ROOM(ch)))
		msg_to_char(ch, "You can't designate rooms here!\r\n");
	else if (subcmd == SCMD_DESIGNATE && ((dir = parse_direction(ch, arg)) == NO_DIR || !can_designate_dir[dir])) {
		msg_to_char(ch, "Invalid direction.\r\n");
		msg_to_char(ch, "Usage: %s <room>\r\n", subcmd == SCMD_REDESIGNATE ? "redesignate" : "designate <direction>");
	}
	else if (!has_permission(ch, PRIV_BUILD) || !can_use_room(ch, IN_ROOM(ch), MEMBERS_ONLY)) {
		msg_to_char(ch, "You don't have permission to designate rooms here.\r\n");
	}
	else if (subcmd == SCMD_DESIGNATE && IS_MAP_BUILDING(IN_ROOM(ch)) && dir != BUILDING_ENTRANCE(IN_ROOM(ch)))
		msg_to_char(ch, "You may only designate %s from here.\r\n", dirs[get_direction_for_char(ch, BUILDING_ENTRANCE(IN_ROOM(ch)))]);
	else if (!IS_COMPLETE(IN_ROOM(ch))) {
		msg_to_char(ch, "You need to finish the building before you can designate rooms.\r\n");
	}
	else if (!IS_INSIDE(IN_ROOM(ch)) && subcmd == SCMD_REDESIGNATE)
		msg_to_char(ch, "You can't redesignate here.\r\n");
	else if (BLD_MAX_ROOMS(IN_ROOM(ch)) <= 0)
		msg_to_char(ch, "You can't designate here.\r\n");
	else if (subcmd == SCMD_DESIGNATE && (ex = find_exit(IN_ROOM(ch), dir)) && ex->room_ptr)
		msg_to_char(ch, "There is already a room that direction.\r\n");
	else if (subcmd == SCMD_DESIGNATE && GET_INSIDE_ROOMS(home) >= BLD_MAX_ROOMS(IN_ROOM(ch)))
		msg_to_char(ch, "There's no more free space.\r\n");
	else if (GET_BLD_DESIGNATE_FLAGS(type) == NOBITS)
		msg_to_char(ch, "You can't designate that type of room!\r\n");
	else if (!BLD_DESIGNATE_FLAGGED(home, GET_BLD_DESIGNATE_FLAGS(type)))
		msg_to_char(ch, "You can't designate that here!\r\n");
	else {
		if (subcmd == SCMD_REDESIGNATE) {
			// redesignate this room
			new = IN_ROOM(ch);
		}
		else {
			// create the new room
			new = create_room();
			create_exit(IN_ROOM(ch), new, dir, TRUE);

			COMPLEX_DATA(home)->inside_rooms++;
		}

		// remove old objects
		remove_designate_objects(new);
		
		// attach new type
		attach_building_to_room(type, new);

		// add new objects
		switch (GET_BLD_VNUM(type)) {
			case RTYPE_STUDY: {
				obj_to_room((obj = read_object(BOARD_MORT)), new);
				load_otrigger(obj);
				break;
			}
			case RTYPE_BEDROOM: {
				if (ROOM_PRIVATE_OWNER(HOME_ROOM(IN_ROOM(ch))) != NOBODY) {
					obj_to_room((obj = read_object(o_HOME_CHEST)), new);
					load_otrigger(obj);
				}
				break;
			}
		}

		/* set applicable values */
		COMPLEX_DATA(new)->home_room = home;
		ROOM_OWNER(new) = ROOM_OWNER(home);

		/* send messages */
		if (subcmd == SCMD_REDESIGNATE) {
			msg_to_char(ch, "You redesignate this room as %s %s.\r\n", AN(GET_BLD_NAME(type)), GET_BLD_NAME(type));
			sprintf(buf, "$n redesignates this room as %s %s.", AN(GET_BLD_NAME(type)), GET_BLD_NAME(type));
			act(buf, FALSE, ch, 0, 0, TO_ROOM);
		}
		else {
			msg_to_char(ch, "You designate the room %s as %s %s.\r\n", dirs[get_direction_for_char(ch, dir)], AN(GET_BLD_NAME(type)), GET_BLD_NAME(type));
			
			for (vict = ROOM_PEOPLE(IN_ROOM(ch)); vict; vict = vict->next_in_room) {
				if (vict != ch && vict->desc) {
					sprintf(buf, "$n designates the room %s as %s %s.", dirs[get_direction_for_char(vict, dir)], AN(GET_BLD_NAME(type)), GET_BLD_NAME(type));
					act(buf, FALSE, ch, 0, vict, TO_VICT);
				}
			}

			// sort now just in case
			sort_world_table();
		}
	}
}


ACMD(do_interlink) {
	extern int count_flagged_sect_between(bitvector_t sectf_bits, room_data *start, room_data *end, bool check_base_sect);
	extern char *get_room_name(room_data *room, bool color);
	
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
	else if (!has_permission(ch, PRIV_BUILD) || !can_use_room(ch, IN_ROOM(ch), MEMBERS_ONLY)) {
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
		
		if (HAS_ABILITY(ch, ABIL_NAVIGATION)) {
			msg_to_char(ch, "You interlink %s to %s (%d, %d).\r\n", dirs[get_direction_for_char(ch, dir)], get_room_name(to_room, FALSE), X_COORD(to_room), Y_COORD(to_room));
		}
		else {
			msg_to_char(ch, "You interlink %s to %s.\r\n", dirs[get_direction_for_char(ch, dir)], get_room_name(to_room, FALSE));
		}
		
		act("$n interlinks the room with another building.", FALSE, ch, NULL, NULL, TO_ROOM);
		
		if (ROOM_PEOPLE(to_room)) {
			act("The room is now interlinked to another building.", FALSE, ROOM_PEOPLE(to_room), NULL, NULL, TO_CHAR | TO_ROOM);
		}
	}
}


ACMD(do_lay) {
	Resource cost[3] = { { o_ROCK, 20 }, END_RESOURCE_LIST };
	sector_data *original_sect = SECT(IN_ROOM(ch));
	sector_data *check_sect = (ROOM_SECT_FLAGGED(IN_ROOM(ch), SECTF_IS_ROAD) ? ROOM_ORIGINAL_SECT(IN_ROOM(ch)) : SECT(IN_ROOM(ch)));
	sector_data *road_sect = find_first_matching_sector(SECTF_IS_ROAD, NOBITS);

	if (IS_NPC(ch)) {
		msg_to_char(ch, "NPCs can't lay roads.\r\n");
	}
	else if (GET_ACTION(ch) != ACT_NONE) {
		msg_to_char(ch, "You're a little busy right now.\r\n");
	}
	else if (ROOM_AFF_FLAGGED(IN_ROOM(ch), ROOM_AFF_HAS_INSTANCE)) {
		msg_to_char(ch, "You can't do that here.\r\n");
	}
	else if (!can_use_room(ch, IN_ROOM(ch), MEMBERS_ONLY))
		msg_to_char(ch, "You can't lay road here!\r\n");
	else if (!has_permission(ch, PRIV_BUILD))
		msg_to_char(ch, "You don't have permission to lay road.\r\n");
	else if (SECT_FLAGGED(check_sect, SECTF_LAY_ROAD) && !SECT_FLAGGED(check_sect, SECTF_ROUGH) && !HAS_ABILITY(ch, ABIL_ROADS)) {
		// not rough requires Roads
		msg_to_char(ch, "You don't have the skill to properly do that.\r\n");
	}
	else if (SECT_FLAGGED(check_sect, SECTF_LAY_ROAD) && SECT_FLAGGED(check_sect, SECTF_ROUGH) && !HAS_ABILITY(ch, ABIL_PATHFINDING)) {
		// rough requires Pathfinding
		msg_to_char(ch, "You don't have the skill to properly do that.\r\n");
	}
	else if (ROOM_SECT_FLAGGED(IN_ROOM(ch), SECTF_IS_ROAD)) {
		// already a road -- attempt to un-lay it
		give_resources(ch, cost, TRUE);

		msg_to_char(ch, "You pull up the road.\r\n");
		act("$n pulls up the road.", FALSE, ch, 0, 0, TO_ROOM);
		
		// this will tear it back down to its base type
		disassociate_building(IN_ROOM(ch));
		command_lag(ch, WAIT_OTHER);
		check_lay_territory(ch, IN_ROOM(ch));
	}
	else if (!road_sect) {
		msg_to_char(ch, "Road data has not been set up for this game.\r\n");
	}
	else if (!ROOM_SECT_FLAGGED(IN_ROOM(ch), SECTF_LAY_ROAD))
		msg_to_char(ch, "You can't lay road here!\r\n");
	else if (!has_resources(ch, cost, TRUE, TRUE)) {
		// sends own messages
	}
	else {
		extract_resources(ch, cost, TRUE);

		msg_to_char(ch, "You lay a road here.\r\n");
		act("$n lays a road here.", FALSE, ch, 0, 0, TO_ROOM);

		// skillup before sect change
		if (SECT_FLAGGED(check_sect, SECTF_ROUGH)) {
			gain_ability_exp(ch, ABIL_PATHFINDING, 15);
		}
		else {
			gain_ability_exp(ch, ABIL_ROADS, 15);
		}
				
		// change it over
		change_terrain(IN_ROOM(ch), GET_SECT_VNUM(road_sect));
		
		// preserve this for un-laying the road (disassociate_building)
		ROOM_ORIGINAL_SECT(IN_ROOM(ch)) = original_sect;

		command_lag(ch, WAIT_OTHER);
		check_lay_territory(ch, IN_ROOM(ch));
	}
}


ACMD(do_maintain) {
	Resource res[3] = { { o_LUMBER, BUILDING_DISREPAIR(IN_ROOM(ch)) }, { o_NAILS, BUILDING_DISREPAIR(IN_ROOM(ch)) }, END_RESOURCE_LIST };
	
	if (BUILDING_DISREPAIR(IN_ROOM(ch)) <= 0) {
		msg_to_char(ch, "It needs no maintenance.\r\n");
	}
	else if (!can_use_room(ch, IN_ROOM(ch), GUESTS_ALLOWED)) {
		msg_to_char(ch, "You can't perform maintenance here.\r\n");
	}
	else if (!has_resources(ch, res, TRUE, TRUE)) {
		// sends own messages
	}
	else {
		extract_resources(ch, res, TRUE);
		COMPLEX_DATA(IN_ROOM(ch))->disrepair = 0;
		msg_to_char(ch, "You perform some quick maintenance on the building.\r\n");
		act("$n performs some quick maintenance on the building.", TRUE, ch, NULL, NULL, TO_ROOM);
		command_lag(ch, WAIT_OTHER);
	}
}


ACMD(do_nodismantle) {
	if (!can_use_room(ch, IN_ROOM(ch), MEMBERS_ONLY)) {
		msg_to_char(ch, "Your empire doesn't own this tile.\r\n");
	}
	else if (!IS_ANY_BUILDING(IN_ROOM(ch))) {
		msg_to_char(ch, "You can only toggle nodismantle in buildings.\r\n");
	}
	else if (!has_permission(ch, PRIV_BUILD)) {
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


ACMD(do_tunnel) {
	bitvector_t exit_bld_flags = BLD_ON_PLAINS | BLD_ANY_FOREST | BLD_ON_DESERT | BLD_FACING_CROP | BLD_ON_GROVE;
	bitvector_t mountain_bld_flags = BLD_ON_MOUNTAIN;

	room_data *entrance, *exit = NULL, *to_room, *last_room = NULL, *past_exit;
	int dir, iter, length;
	bool error;
	
	one_argument(argument, arg);
	
	if (!has_permission(ch, PRIV_BUILD)) {
		msg_to_char(ch, "You do not have permission to build anything.\r\n");
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
			if (!SECT_FLAGGED(ROOM_ORIGINAL_SECT(to_room), SECTF_ROUGH)) {
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


ACMD(do_upgrade) {
	craft_data *iter, *next_iter, *type;
	bld_vnum upgrade = GET_BLD_UPGRADES_TO(building_proto(BUILDING_VNUM(IN_ROOM(ch))));
	bool wait, room_wait;
	
	if (IS_NPC(ch)) {
		msg_to_char(ch, "NPCs cannot use the upgrade command.\r\n");
	}
	else if (!GET_BUILDING(IN_ROOM(ch)) || (upgrade = GET_BLD_UPGRADES_TO(GET_BUILDING(IN_ROOM(ch)))) == NOTHING || !building_proto(upgrade)) {
		msg_to_char(ch, "You can't upgrade this.\r\n");
	}
	else if (!can_use_room(ch, IN_ROOM(ch), MEMBERS_ONLY)) {
		msg_to_char(ch, "You don't have permission to upgrade this building.\r\n");
	}
	else if (!has_permission(ch, PRIV_BUILD)) {
		msg_to_char(ch, "You do not have permission to upgrade buildings.\r\n");
	}
	else if (GET_ACTION(ch) != ACT_NONE) {
		msg_to_char(ch, "You're already busy.\r\n");
	}
	else if (!IS_COMPLETE(IN_ROOM(ch))) {
		msg_to_char(ch, "You can't start to upgrade it until you finish the construction.\r\n");
	}
	else {
		// ok, we know it's upgradeable and they have permission... now locate the upgrade craft...
		type = NULL;
		HASH_ITER(hh, craft_table, iter, next_iter) {
			if (GET_CRAFT_TYPE(iter) == CRAFT_TYPE_BUILD && IS_SET(GET_CRAFT_FLAGS(iter), CRAFT_UPGRADE) && upgrade == GET_CRAFT_BUILD_TYPE(iter)) {
				if (IS_IMMORTAL(ch) || !IS_SET(GET_CRAFT_FLAGS(iter), CRAFT_IN_DEVELOPMENT)) {
					type = iter;
					break;
				}
			}
		}
		
		if (!type) {
			msg_to_char(ch, "You don't know how to upgrade this building.\r\n");
		}
		else if (GET_CRAFT_ABILITY(type) != NO_ABIL && !HAS_ABILITY(ch, GET_CRAFT_ABILITY(type))) {
			msg_to_char(ch, "You don't have the required ability to upgrade this building.\r\n");
		}
		else if (IS_SET(GET_CRAFT_FLAGS(type), CRAFT_IN_CITY_ONLY) && !is_in_city_for_empire(IN_ROOM(ch), GET_LOYALTY(ch), TRUE, &wait) && !is_in_city_for_empire(IN_ROOM(ch), ROOM_OWNER(IN_ROOM(ch)), TRUE, &room_wait)) {
			msg_to_char(ch, "You can only upgrade this building in a city%s.\r\n", (wait || room_wait) ? " (this city was founded too recently)" : "");
		}
		else {
			// it's good!
			start_action(ch, ACT_BUILDING, 0, NOBITS);

			attach_building_to_room(building_proto(GET_CRAFT_BUILD_TYPE(type)), IN_ROOM(ch));
			set_room_extra_data(IN_ROOM(ch), ROOM_EXTRA_BUILD_RECIPE, GET_CRAFT_VNUM(type));
			setup_building_resources(IN_ROOM(ch), GET_CRAFT_RESOURCES(type), FALSE);

			msg_to_char(ch, "You begin to upgrade the building.\r\n");
			act("$n starts upgrading the building.", FALSE, ch, 0, 0, TO_ROOM);
		}
	}
}

