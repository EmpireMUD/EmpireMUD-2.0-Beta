/* ************************************************************************
*   File: act.trade.c                                     EmpireMUD 2.0b5 *
*  Usage: code related to crafting and the trade skill                    *
*                                                                         *
*  EmpireMUD code base by Paul Clarke, (C) 2000-2024                      *
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
*   Helpers
*   Generic Craft (craft, forge, sew, cook)
*   Reforge / Refashion
*   Commands
*/

// locals
ACMD(do_gen_craft);
bool can_forge(char_data *ch);
bool can_refashion(char_data *ch);
craft_data *find_craft_for_obj_vnum(obj_vnum vnum);
obj_data *find_water_container(char_data *ch, obj_data *list);


 //////////////////////////////////////////////////////////////////////////////
//// HELPERS /////////////////////////////////////////////////////////////////

/**
* This validates the craft by type/flags and can be called when setting up the
* craft, or while processing it.
*
* @param char_data *ch The person crafting, or trying to.
* @param craft_data *type The craft they are attempting.
* @param bool continuing If TRUE, skips things that are only required to start a craft (like crafting level).
* @return bool TRUE if okay, FALSE if not.
*/
bool check_can_craft(char_data *ch, craft_data *type, bool continuing) {
	char buf1[MAX_STRING_LENGTH], *str, *ptr;
	vehicle_data *craft_veh;
	bool wait, room_wait, makes_building;
	bitvector_t fncs_minus_upgraded = (GET_CRAFT_REQUIRES_FUNCTION(type) & ~FNC_UPGRADED);
	
	char *command = gen_craft_data[GET_CRAFT_TYPE(type)].command;
	
	// some setup
	craft_veh = CRAFT_IS_VEHICLE(type) ? vehicle_proto(GET_CRAFT_OBJECT(type)) : NULL;
	makes_building = (CRAFT_IS_BUILDING(type) || (craft_veh && VEH_FLAGGED(craft_veh, VEH_BUILDING)));
	
	// attribute-based checks
	if (GET_CRAFT_MIN_LEVEL(type) > get_crafting_level(ch) && !continuing) {
		msg_to_char(ch, "You need to have a crafting level of %d to %s that.\r\n", GET_CRAFT_MIN_LEVEL(type), command);
	}
	else if (!CRAFT_FLAGGED(type, CRAFT_DARK_OK) && !can_see_in_dark_room(ch, IN_ROOM(ch), TRUE)) {
		msg_to_char(ch, "It's too dark to %s anything.\r\n", command);
	}
	
	// type checks
	else if (GET_CRAFT_TYPE(type) == CRAFT_TYPE_MILL && !has_tool(ch, TOOL_GRINDING_STONE) && !room_has_function_and_city_ok(GET_LOYALTY(ch), IN_ROOM(ch), FNC_MILL)) {
		msg_to_char(ch, "You need to be in a mill or have a grinding stone to do that.\r\n");
	}
	else if (GET_CRAFT_TYPE(type) == CRAFT_TYPE_PRESS && !room_has_function_and_city_ok(GET_LOYALTY(ch), IN_ROOM(ch), FNC_PRESS)) {
		msg_to_char(ch, "You need a press to do that.\r\n");
	}
	else if (CRAFT_FLAGGED(type, CRAFT_BY_RIVER) && (!IS_OUTDOORS(ch) || !find_flagged_sect_within_distance_from_char(ch, SECTF_FRESH_WATER, NOBITS, 1))) {
		msg_to_char(ch, "You must be next to a river to %s that.\r\n", command);
	}
	else if (GET_CRAFT_TYPE(type) == CRAFT_TYPE_FORGE && !can_forge(ch)) {
		// sends its own message
	}
	else if (GET_CRAFT_TYPE(type) == CRAFT_TYPE_SEW && !has_tool(ch, TOOL_SEWING_KIT) && !room_has_function_and_city_ok(GET_LOYALTY(ch), IN_ROOM(ch), FNC_TAILOR)) {
		msg_to_char(ch, "You need to equip a sewing kit to make that, or sew it at a tailor.\r\n");
	}
	else if (GET_CRAFT_TYPE(type) == CRAFT_TYPE_WEAVE && !room_has_function_and_city_ok(GET_LOYALTY(ch), IN_ROOM(ch), FNC_TAILOR) && !has_tool(ch, TOOL_LOOM)) {
		msg_to_char(ch, "You need a loom to do that.\r\n");
	}
	else if (GET_CRAFT_TYPE(type) == CRAFT_TYPE_SMELT && !room_has_function_and_city_ok(GET_LOYALTY(ch), IN_ROOM(ch), FNC_SMELT)) {
		msg_to_char(ch, "You can't %s here.\r\n", command);
	}
	else if (GET_CRAFT_TYPE(type) == CRAFT_TYPE_BAKE && !room_has_function_and_city_ok(GET_LOYALTY(ch), IN_ROOM(ch), FNC_OVEN)) {
		msg_to_char(ch, "You need an oven to %s that.\r\n", command);
	}
	
	// building type checks
	else if (makes_building && ROOM_AFF_FLAGGED(IN_ROOM(ch), ROOM_AFF_UNCLAIMABLE) && !continuing) {
		msg_to_char(ch, "You can't %s that on unclaimable land.\r\n", command);
	}
	else if (makes_building && ROOM_AFF_FLAGGED(IN_ROOM(ch), ROOM_AFF_HAS_INSTANCE) && !continuing) {
		msg_to_char(ch, "You can't %s here until the adventure is gone.\r\n", command);
	}
	else if (makes_building && !can_build_or_claim_at_war(ch, IN_ROOM(ch)) && !continuing) {
		msg_to_char(ch, "You can't %s that here while at war with the empire that controls this area.\r\n", command);
	}
	else if (makes_building && (!can_use_room(ch, IN_ROOM(ch), MEMBERS_ONLY) || !has_permission(ch, PRIV_BUILD, IN_ROOM(ch))) && !continuing) {
		msg_to_char(ch, "You don't have permission to %s that here.\r\n", command);
	}
	
	// flag checks
	else if (IS_SET(GET_CRAFT_FLAGS(type), CRAFT_IN_CITY_ONLY) && !is_in_city_for_empire(IN_ROOM(ch), GET_LOYALTY(ch), TRUE, &wait) && !is_in_city_for_empire(IN_ROOM(ch), ROOM_OWNER(IN_ROOM(ch)), TRUE, &room_wait)) {
		msg_to_char(ch, "You can only %s that in one of your cities%s.\r\n", command, (wait || room_wait) ? " (this city was founded too recently)" : "");
	}
	else if ((GET_CRAFT_TYPE(type) == CRAFT_TYPE_MILL || GET_CRAFT_TYPE(type) == CRAFT_TYPE_PRESS || GET_CRAFT_TYPE(type) == CRAFT_TYPE_FORGE || GET_CRAFT_TYPE(type) == CRAFT_TYPE_SMELT) && !check_in_city_requirement(IN_ROOM(ch), TRUE)) {
		msg_to_char(ch, "You can't do that here because this building isn't in a city.\r\n");
	}
	else if (IS_SET(GET_CRAFT_FLAGS(type), CRAFT_POTTERY) && !has_cooking_fire(ch)) {
		msg_to_char(ch, "You need a fire to bake the clay.\r\n");
	}
	else if (IS_SET(GET_CRAFT_FLAGS(type), CRAFT_FIRE) && !has_cooking_fire(ch)) {
		msg_to_char(ch, "You need a good fire to do that.\r\n");
	}
	else if (IS_SET(GET_CRAFT_FLAGS(type), CRAFT_SOUP) && !find_water_container(ch, ch->carrying) && !find_water_container(ch, ROOM_CONTENTS(IN_ROOM(ch)))) {
		msg_to_char(ch, "You need a container of water to %s that.\r\n", command);
	}
	else if (fncs_minus_upgraded && !room_has_function_and_city_ok(GET_LOYALTY(ch), IN_ROOM(ch), fncs_minus_upgraded)) {
		// this checks/shows without FNC_UPGRADED, which is handled separately after.
		prettier_sprintbit(fncs_minus_upgraded, function_flags_long, buf1);
		str = buf1;
		if ((ptr = strrchr(str, ','))) {
			msg_to_char(ch, "You must be %-*.*s or%s to %s that.\r\n", (int)(ptr-str), (int)(ptr-str), str, ptr+1, command);
		}
		else {	// no comma
			msg_to_char(ch, "You must be %s to %s that.\r\n", buf1, command);
		}
	}
	else if (IS_SET(GET_CRAFT_REQUIRES_FUNCTION(type), FNC_UPGRADED) && !ROOM_IS_UPGRADED(IN_ROOM(ch))) {
		msg_to_char(ch, "You need to be in an upgraded building to %s that!\r\n", command);
	}
	// end flag checks
	
	// tool checks
	else if (GET_CRAFT_REQUIRES_TOOL(type) && !has_all_tools(ch, GET_CRAFT_REQUIRES_TOOL(type))) {
		prettier_sprintbit(GET_CRAFT_REQUIRES_TOOL(type), tool_flags, buf1);
		if (count_bits(GET_CRAFT_REQUIRES_TOOL(type)) > 1) {
			msg_to_char(ch, "You need the to equip following tools to %s that: %s\r\n", command, buf1);
		}
		else {
			msg_to_char(ch, "You need to equip %s %s to %s that.\r\n", AN(buf1), buf1, command);
		}
	}
	
	// types that require the building be complete
	else if (!IS_COMPLETE(IN_ROOM(ch)) && (GET_CRAFT_TYPE(type) == CRAFT_TYPE_MILL || GET_CRAFT_TYPE(type) == CRAFT_TYPE_SMELT || GET_CRAFT_TYPE(type) == CRAFT_TYPE_PRESS)) {
		msg_to_char(ch, "You must complete the building first.\r\n");
	}
	
	else {
		// looks good!
		return TRUE;
	}
	
	return FALSE;	// if we got this far
}


/**
* This determines if the character is in a place where they can forge, and that
* they are wielding a hammer. If not, this function sends an error message.
*
* @param char_data *ch The person trying to forge.
* @return bool If the character can forge TRUE, otherwise FALSE.
*/
bool can_forge(char_data *ch) {
	bool ok = FALSE;
	
	if (!IS_IMMORTAL(ch) && !room_has_function_and_city_ok(GET_LOYALTY(ch), IN_ROOM(ch), FNC_FORGE)) {
		msg_to_char(ch, "You need to be in a forge to do that.\r\n");
	}
	else if (!IS_IMMORTAL(ch) && !room_has_function_and_city_ok(GET_LOYALTY(ch), IN_ROOM(ch), FNC_FORGE)) {
		msg_to_char(ch, "This forge only works if it's in a city.\r\n");
	}
	else if (!IS_COMPLETE(IN_ROOM(ch))) {
		msg_to_char(ch, "You must complete the building first.\r\n");
	}
	else if (!check_in_city_requirement(IN_ROOM(ch), TRUE)) {
		msg_to_char(ch, "This building must be in a city to use it.\r\n");
	}
	else if (!has_tool(ch, TOOL_HAMMER)) {
		msg_to_char(ch, "You need to use a hammer to do that.\r\n");
	}
	else {
		ok = TRUE;
	}
	
	return ok;
}


/**
* Looks for a bound version of the item in ch's inventory, and binds one if
* it only finds an unbound version. This is used for BoE recipes, for example.
* 
* @param char_data *ch The player to bind to.
* @param obj_vnum vnum The vnum item to look for in ch's inventory.
* @return bool TRUE if one was found and/or bound.
*/
bool find_and_bind(char_data *ch, obj_vnum vnum) {
	obj_data *iter, *unbound = NULL;
	struct obj_binding *bind;
	int list;
	
	if (IS_NPC(ch) || vnum == NOTHING) {
		return TRUE;	// don't bother
	}
	
	obj_data *search[2] = { ch->carrying, ROOM_CONTENTS(IN_ROOM(ch)) };
	
	for (list = 0; list < 2; ++list) {
		DL_FOREACH2(search[list], iter, next_content) {
			if (GET_OBJ_VNUM(iter) != vnum || !bind_ok(iter, ch)) {
				continue;	// wrong obj
			}
			if (!OBJ_FLAGGED(iter, OBJ_BIND_FLAGS)) {
				return TRUE;	// we found the object but it doesn't require binding
			}
		
			// ok we have the item, see if it's bound to ch
			LL_FOREACH(OBJ_BOUND_TO(iter), bind) {
				if (bind->idnum == GET_IDNUM(ch)) {
					reduce_obj_binding(iter, ch);
					return TRUE;	// already bound to ch
				}
			}
		
			// if we got this far, it's not bound (only want the first unbound one)
			if (!unbound) {
				unbound = iter;
			}
		}
	}
	
	if (unbound) {
		bind_obj_to_player(unbound, ch);
		return TRUE;
	}
	else {
		return FALSE;	// found no matching item
	}
}


/**
* Looks for a craft the player knows, and falls back to ones they don't. It
* always prefers an exact match over anything, and prefers crafts you have the
* resources for. Immortals can also hit in-dev recipes.
*
* @param char_data *ch The person looking for a craft.
* @param char *argument The typed-in name.
* @param int craft_type Any CRAFT_TYPE_ to look up.
* @param bool hide_dismantle_only If TRUE, skips crafts set dismantle-only.
* @param int *found_wrong_cmd Optional: If a craft otherwise matched but was for a different command, will set this var to the correct craft command e.g. CRAFT_TYPE_BREW (but the function will still return NULL). A NOTHING on this variable, or a successful return on the function, indicates there was no mismatched type.
* @return craft_data* The matching craft, if any.
*/
craft_data *find_best_craft_by_name(char_data *ch, char *argument, int craft_type, bool hide_dismantle_only, int *found_wrong_cmd) {
	craft_data *unknown_abbrev = NULL;
	craft_data *known_abbrev = NULL, *known_abbrev_no_res = NULL;
	craft_data *unknown_multi = NULL;
	craft_data *known_multi = NULL, *known_multi_no_res = NULL;
	craft_data *craft, *next_craft;
	bool use_room = can_use_room(ch, IN_ROOM(ch), GUESTS_ALLOWED);
	
	if (found_wrong_cmd) {
		*found_wrong_cmd = NOTHING;
	}
	
	skip_spaces(&argument);
	
	HASH_ITER(sorted_hh, sorted_crafts, craft, next_craft) {
		if (IS_SET(GET_CRAFT_FLAGS(craft), CRAFT_IN_DEVELOPMENT) && !IS_IMMORTAL(ch)) {
			continue;
		}
		if (hide_dismantle_only && CRAFT_FLAGGED(craft, CRAFT_DISMANTLE_ONLY)) {
			continue;
		}
		if (GET_CRAFT_REQUIRES_OBJ(craft) != NOTHING && !has_required_obj_for_craft(ch, GET_CRAFT_REQUIRES_OBJ(craft))) {
			continue;
		}
		
		if (!str_cmp(argument, GET_CRAFT_NAME(craft))) {
			if (GET_CRAFT_TYPE(craft) != craft_type) {
				// check this late because we want to record a mismatch
				if (found_wrong_cmd) {
					*found_wrong_cmd = GET_CRAFT_TYPE(craft);
				}
				continue;
			}
			
			// exact match!
			return craft;
		}
		else if (!known_abbrev && is_abbrev(argument, GET_CRAFT_NAME(craft))) {
			if (GET_CRAFT_TYPE(craft) != craft_type) {
				// check this late because we want to record a mismatch
				if (found_wrong_cmd) {
					*found_wrong_cmd = GET_CRAFT_TYPE(craft);
				}
				continue;
			}
			else if (IS_SET(GET_CRAFT_FLAGS(craft), CRAFT_IN_DEVELOPMENT)) {
				// only imms hit this block
				if (!unknown_abbrev) {
					unknown_abbrev = craft;
				}
			}
			else if (GET_CRAFT_ABILITY(craft) != NO_ABIL && !has_ability(ch, GET_CRAFT_ABILITY(craft))) {
				if (!unknown_abbrev) {	// player missing ability
					unknown_abbrev = craft;
				}
			}
			else if (IS_SET(GET_CRAFT_FLAGS(craft), CRAFT_LEARNED) && !has_learned_craft(ch, GET_CRAFT_VNUM(craft))) {
				if (!unknown_abbrev) {	// player missing 'learned'
					unknown_abbrev = craft;
				}
			}
			else {	// they should have access to it
				if (has_resources(ch, GET_CRAFT_RESOURCES(craft), use_room, FALSE, NULL)) {
					known_abbrev = craft;
				}
				else if (!known_abbrev_no_res) {
					known_abbrev_no_res = craft;
				}
			}
		}
		else if (!known_multi && multi_isname(argument, GET_CRAFT_NAME(craft))) {
			if (GET_CRAFT_TYPE(craft) != craft_type) {
				// check this late because we want to record a mismatch
				if (found_wrong_cmd) {
					*found_wrong_cmd = GET_CRAFT_TYPE(craft);
				}
				continue;
			}
			else if (IS_SET(GET_CRAFT_FLAGS(craft), CRAFT_IN_DEVELOPMENT)) {
				// only imms hit this block
				if (!unknown_multi) {
					unknown_multi = craft;
				}
			}
			else if (GET_CRAFT_ABILITY(craft) != NO_ABIL && !has_ability(ch, GET_CRAFT_ABILITY(craft))) {
				if (!unknown_multi) {	// player missing ability
					unknown_multi = craft;
				}
			}
			else if (IS_SET(GET_CRAFT_FLAGS(craft), CRAFT_LEARNED) && !has_learned_craft(ch, GET_CRAFT_VNUM(craft))) {
				if (!unknown_multi) {	// player missing 'learned'
					unknown_multi = craft;
				}
			}
			else {	// they should have access to it
				if (has_resources(ch, GET_CRAFT_RESOURCES(craft), use_room, FALSE, NULL)) {
					known_multi = craft;
				}
				else if (!known_multi_no_res) {
					known_multi_no_res = craft;
				}
			}
		}
	}
	
	// if we got this far, it didn't return an exact match with resources
	if (known_abbrev) {
		return known_abbrev;
	}
	else if (known_abbrev_no_res) {
		return known_abbrev_no_res;
	}
	else if (known_multi) {
		return known_multi;
	}
	else if (known_multi_no_res) {
		return known_multi_no_res;
	}
	else if (unknown_abbrev) {
		return unknown_abbrev;
	}
	else if (unknown_multi) {
		return unknown_multi;
	}
	else {
		// nooo
		return NULL;
	}
}


/**
* Finds an unfinished vehicle in the room that the character can finish.
*
* @param char_data *ch The person trying to craft a vehicle.
* @param craft_data *type Optional: The craft recipe to match up (may be NULL to ignore).
* @parma int with_id Optional: The vehicle construction id to find (may be NOTHING to ignore).
* @param vehicle_data **found_other A variable to bind an existing vehicle to -- if NULL, there are no unfinished vehicles lying around. Otherwise, one will be assigned here.
* @return vehicle_data* The found vehicle, or NULL if none.
*/
vehicle_data *find_finishable_vehicle(char_data *ch, craft_data *type, int with_id, vehicle_data **found_other) {
	vehicle_data *iter, *found = NULL;
	
	*found_other = NULL;
	
	DL_FOREACH2(ROOM_VEHICLES(IN_ROOM(ch)), iter, next_in_room) {
		// skip finished vehicles
		if (!VEH_FLAGGED(iter, VEH_INCOMPLETE)) {
			continue;	// don't use VEH_IS_COMPLETE as it includes no-resources-needed
		}
		// there is at least 1 incomplete vehicle here
		found = iter;
		
		// right vehicle?
		if (type && VEH_VNUM(iter) != GET_CRAFT_OBJECT(type)) {
			continue;
		}
		if (with_id != NOTHING && VEH_CONSTRUCTION_ID(iter) != with_id) {
			continue;
		}
		if (!can_use_vehicle(ch, iter, GUESTS_ALLOWED)) {
			continue;
		}
		
		// found one!
		return iter;
	}
	
	*found_other = found;	// if any
	return NULL;	// did not find the one requested
}


/**
* Finds an unfinished vehicle in the room that the character can finish, by
* name, and also locates the matching craft to resume.
*
* @param char_data *ch The person trying to craft a vehicle.
* @param int craft_type The command (CRAFT_TYPE_ const) the player is using.
* @param char *name The argument typed.
* @param craft_data **found_craft If a vehicle is found, this is the craft for it.
* @return vehicle_data* The found vehicle, or NULL if none.
*/
vehicle_data *find_vehicle_to_resume_by_name(char_data *ch, int craft_type, char *name, craft_data **found_craft) {
	vehicle_data *veh;
	
	*found_craft = NULL;
	
	DL_FOREACH2(ROOM_VEHICLES(IN_ROOM(ch)), veh, next_in_room) {
		if (VEH_IS_COMPLETE(veh)) {
			continue;	// skip finished vehicles
		}
		if (!multi_isname(name, VEH_KEYWORDS(veh))) {
			continue;	// not a keyword match
		}
		if (!can_use_vehicle(ch, veh, GUESTS_ALLOWED)) {
			continue;	// not allowed to work on it
		}
		
		// ok: see if we have a craft for it
		if ((*found_craft = find_craft_for_vehicle(veh))) {
			return veh;	// and return it if so
		}
	}
	
	return NULL;	// if not
}


/**
* This finds a drink container that is at least half-full of water.
*
* @param char_data *ch the person looking
* @param obj_data *list Any object list (ch->carrying)
* @return obj_data *the found drink container, or NULL
*/
obj_data *find_water_container(char_data *ch, obj_data *list) {
	obj_data *obj, *found = NULL;
	
	DL_FOREACH2(list, obj, next_content) {
		if (IS_DRINK_CONTAINER(obj) && CAN_SEE_OBJ(ch, obj) && liquid_flagged(GET_DRINK_CONTAINER_TYPE(obj), LIQF_WATER) && GET_DRINK_CONTAINER_CONTENTS(obj) >= (GET_DRINK_CONTAINER_CAPACITY(obj)/2)) {
			found = obj;
			break;
		}
	}
	
	return found;
}


/**
* Returns the total level to use for a character's crafts.
*
* @param char_data *ch The character to check.
*/
int get_crafting_level(char_data *ch) {
	if (IS_NPC(ch)) {
		return get_approximate_level(ch) + GET_CRAFTING_BONUS(ch);
	}
	else {
		return GET_SKILL_LEVEL(ch) + GET_CRAFTING_BONUS(ch);
	}
}


/**
* Determines what level to scale something to, based on who crafted it and what
* craft recipe they used.
*
* @param char_data *ch The crafter.
* @param craft_data *craft The recipe.
* @return int The best scale level.
*/
int get_craft_scale_level(char_data *ch, craft_data *craft) {
	int level = 1, psr, craft_lev;
	ability_data *abil;
	obj_data *req, *obj;
	vehicle_data *veh;
	
	if (IS_NPC(ch)) {
		return 0;
	}
	
	craft_lev = get_crafting_level(ch);
	
	// determine ideal scale level
	if (craft) {
		if (GET_CRAFT_REQUIRES_OBJ(craft) != NOTHING && (req = obj_proto(GET_CRAFT_REQUIRES_OBJ(craft)))) {
			// anything that requires an object handles its own level range (on the crafted item itself)
			level = craft_lev;
		}
		else if (CRAFT_FLAGGED(craft, CRAFT_LEARNED)) {
			// learned recipes would be constrained by the created obj, if anything
			level = craft_lev;
		}
		else {
			if (!(abil = find_ability_by_vnum(GET_CRAFT_ABILITY(craft)))) {
				level = EMPIRE_CHORE_SKILL_CAP;	// considered the "default" level for unskilled things
			}
			else if (!ABIL_ASSIGNED_SKILL(abil)) {
				// probably a class skill
				level = craft_lev;
			}
			else if ((psr = ABIL_SKILL_LEVEL(abil)) != NOTHING) {
				// craft comes from a skill-ability (and is not learned/requires-obj):
				// limit to the the top of that skill range
				if (psr < BASIC_SKILL_CAP) {
					level = MIN(BASIC_SKILL_CAP, craft_lev);
				}
				else if (psr < SPECIALTY_SKILL_CAP) {
					level = MIN(SPECIALTY_SKILL_CAP, craft_lev);
				}
				else if (psr < CLASS_SKILL_CAP) {
					level = MIN(CLASS_SKILL_CAP, craft_lev);
				}
				else {	// is a skill ability but >= class skill level (100) -- don't restrict
					level = craft_lev;
				}
			}
			else {
				// this is probably unreachable
				level = craft_lev;
			}
			
			// always bound by the crafting level
			level = MIN(level, craft_lev);
		}
		
		// and level bounds
		if (CRAFT_IS_VEHICLE(craft)) {
			if ((veh = vehicle_proto(GET_CRAFT_OBJECT(craft)))) {
				if (VEH_MIN_SCALE_LEVEL(veh) > 0) {
					level = MAX(level, VEH_MIN_SCALE_LEVEL(veh));
				}
				if (VEH_MAX_SCALE_LEVEL(veh) > 0) {
					level = MIN(level, VEH_MAX_SCALE_LEVEL(veh));
				}
			}
		}
		else if (!IS_SET(GET_CRAFT_FLAGS(craft), CRAFT_SOUP)) {
			if ((obj = obj_proto(GET_CRAFT_OBJECT(craft)))) {
				if (GET_OBJ_MIN_SCALE_LEVEL(obj) > 0) {
					level = MAX(level, GET_OBJ_MIN_SCALE_LEVEL(obj));
				}
				if (GET_OBJ_MAX_SCALE_LEVEL(obj) > 0) {
					level = MIN(level, GET_OBJ_MAX_SCALE_LEVEL(obj));
				}
			}
		}
	}
	else {
		// no craft given
		level = craft_lev;
	}
	
	return level;
}


/**
* Finds the required obj for a craft, if present, and returns it. If the item
* is in the room, it must be bind-ok.
*
* @param char_data *ch The player.
* @param obj_data vnum Which item to look for.
* @return obj_data* The object if found, NULL if not.
*/
obj_data *has_required_obj_for_craft(char_data *ch, obj_vnum vnum) {
	obj_data *obj;
	
	// inv
	DL_FOREACH2(ch->carrying, obj, next_content) {
		if (GET_OBJ_VNUM(obj) == vnum) {
			return obj;
		}
	}
	
	// room
	if (can_use_room(ch, IN_ROOM(ch), MEMBERS_ONLY)) {
		DL_FOREACH2(ROOM_CONTENTS(IN_ROOM(ch)), obj, next_content) {
			if (GET_OBJ_VNUM(obj) == vnum && bind_ok(obj, ch)) {
				return obj;
			}
		}
	}
	
	return NULL;	// none
}


/**
* Shows a character which augments are available. If a matching_obj is given,
* only lists augments that can be applied to that item.
*
* @param char_data *ch The person to list to (whose abilities will be checked).
* @param int type AUGMENT_x type.
* @param obj_data *matching_obj Optional: Only show augments that work on this item (or NULL).
*/
void list_available_augments(char_data *ch, int type, obj_data *matching_obj) {
	char buf[MAX_STRING_LENGTH];
	augment_data *aug, *next_aug;
	bool found, line;
	
	*buf = '\0';
	line = found = FALSE;
	HASH_ITER(sorted_hh, sorted_augments, aug, next_aug) {
		if (GET_AUG_TYPE(aug) != type || AUGMENT_FLAGGED(aug, AUG_IN_DEVELOPMENT)) {
			continue;
		}
		if (GET_AUG_ABILITY(aug) != NO_ABIL && !has_ability(ch, GET_AUG_ABILITY(aug))) {
			continue;
		}
		if (GET_AUG_REQUIRES_OBJ(aug) != NOTHING && !get_obj_in_list_vnum(GET_AUG_REQUIRES_OBJ(aug), ch->carrying)) {
			continue;
		}
		if (matching_obj && !validate_augment_target(ch, matching_obj, aug, FALSE)) {
			continue;
		}
		
		// send last line?
		if ((strlen(buf) + strlen(GET_AUG_NAME(aug)) + 2) >= 80) {
			msg_to_char(ch, "%s\r\n", buf);
			line = FALSE;
			*buf = '\0';
		}
		
		// add this entry to line
		sprintf(buf + strlen(buf), "%s%s", (line ? ", " : " "), GET_AUG_NAME(aug));
		line = found = TRUE;
	}
	
	if (line) {
		msg_to_char(ch, "%s\r\n", buf);
	}
	if (!found) {
		msg_to_char(ch, "  nothing\r\n");
	}
}


/**
* @param obj_data *obj Any item.
* @param int apply_type APPLY_TYPE_x
* @return bool TRUE if obj has at least 1 apply of that type.
*/
bool obj_has_apply_type(obj_data *obj, int apply_type) {
	struct obj_apply *app;
	for (app = GET_OBJ_APPLIES(obj); app; app = app->next) {
		if (app->apply_type == apply_type) {
			return TRUE;
		}
	}
	return FALSE;
}


/**
* Resumes working on an incomplete building.
*
* @param char_data *ch The player.
* @param craft_data *craft The craft recipe for the building.
*/
void resume_craft_building(char_data *ch, craft_data *craft) {
	char buf[MAX_STRING_LENGTH], bld_name[256], the_buf[256];
	
	if (!ch || IS_NPC(ch) || !craft || !IS_INCOMPLETE(IN_ROOM(ch))) {
		return;
	}
	
	// ensure they CAN resume
	if (GET_ACTION(ch) != ACT_NONE) {
		msg_to_char(ch, "You are already doing something.\r\n");
		return;
	}
	if (GET_POS(ch) < POS_STANDING) {
		send_low_pos_msg(ch);
		return;
	}
	
	start_action(ch, ACT_BUILDING, 0);
	GET_ACTION_VNUM(ch, 0) = GET_CRAFT_VNUM(craft);
	
	strcpy(bld_name, GET_BUILDING(IN_ROOM(ch)) ? GET_BLD_NAME(GET_BUILDING(IN_ROOM(ch))) : GET_CRAFT_NAME(craft));
	if (strn_cmp(bld_name, "the ", 4) && strn_cmp(bld_name, "a ", 2) && strn_cmp(bld_name, "an ", 3)) {
		strcpy(the_buf, "the ");
	}
	else {
		*the_buf = '\0';
	}
	
	msg_to_char(ch, "You continue working on %s%s!\r\n", the_buf, bld_name);
	sprintf(buf, "$n continues working on %s%s!", the_buf, bld_name);
	act(buf, FALSE, ch, NULL, NULL, TO_ROOM);
}


/**
* Resumes working on an incomplete vehicle.
*
* @param char_data *ch The player.
* @param vehicle_data *veh The vehicle to resume work on.
* @param craft_data *craft The craft recipe it uses.
*/
void resume_craft_vehicle(char_data *ch, vehicle_data *veh, craft_data *craft) {
	char buf[MAX_STRING_LENGTH];
	
	if (!ch || IS_NPC(ch) || !veh || !craft) {
		return;
	}
	
	// ensure they CAN resume
	if (GET_ACTION(ch) != ACT_NONE) {
		msg_to_char(ch, "You are already doing something.\r\n");
		return;
	}
	if (VEH_IS_DISMANTLING(veh)) {
		msg_to_char(ch, "You can't work on that -- it's being dismantled! (Try using 'dismantle' to do that instead.)\r\n");
		return;
	}
	if (GET_POS(ch) < POS_STANDING) {
		send_low_pos_msg(ch);
		return;
	}
	
	start_action(ch, ACT_GEN_CRAFT, -1);
	GET_ACTION_VNUM(ch, 0) = GET_CRAFT_VNUM(craft);
	GET_ACTION_VNUM(ch, 1) = VEH_CONSTRUCTION_ID(veh);
	
	snprintf(buf, sizeof(buf), "You resume %s $V.", gen_craft_data[GET_CRAFT_TYPE(craft)].verb);
	act(buf, FALSE, ch, NULL, veh, TO_CHAR | ACT_VEH_VICT);
	snprintf(buf, sizeof(buf), "$n resumes %s $V.", gen_craft_data[GET_CRAFT_TYPE(craft)].verb);
	act(buf, FALSE, ch, NULL, veh, TO_ROOM | ACT_VEH_VICT);
}


/**
* This is like a mortal version of do_stat_craft().
*
* @param char_data *ch The person checking the craft info.
* @param craft_data *craft Which craft to show.
* @param int craft_type Whichever CRAFT_TYPE_ the player is using.
*/
void show_craft_info(char_data *ch, char *argument, int craft_type) {
	char buf[MAX_STRING_LENGTH], part[MAX_STRING_LENGTH];
	struct obj_apply *apply;
	ability_data *abil;
	craft_data *craft;
	vehicle_data *veh;
	obj_data *proto;
	bld_data *bld;
	int craft_level, found_wrong_cmd = NOTHING;
	
	// these flags show on craft info
	bitvector_t show_flags = OBJ_UNIQUE | OBJ_LARGE | OBJ_TWO_HANDED | OBJ_BIND_ON_EQUIP | OBJ_BIND_ON_PICKUP;
	
	if (!*argument) {
		msg_to_char(ch, "Get %s info on what?\r\n", gen_craft_data[craft_type].command);
		return;
	}
	if (!(craft = find_best_craft_by_name(ch, argument, craft_type, TRUE, &found_wrong_cmd))) {
		if (found_wrong_cmd != NOTHING) {
			msg_to_char(ch, "Unknown %s. Try '%s' instead.\r\n", gen_craft_data[craft_type].command, gen_craft_data[found_wrong_cmd].command);
		}
		else {
			msg_to_char(ch, "You don't know any such %s recipe.\r\n", gen_craft_data[craft_type].command);
		}
		return;
	}
	
	msg_to_char(ch, "Information for %s:\r\n", GET_CRAFT_NAME(craft));
	
	if (CRAFT_IS_BUILDING(craft)) {
		bld = building_proto(GET_CRAFT_BUILD_TYPE(craft));
		msg_to_char(ch, "Builds: %s\r\n", bld ? GET_BLD_NAME(bld) : "UNKNOWN");
	}
	else if (CRAFT_IS_VEHICLE(craft)) {
		if ((veh = vehicle_proto(GET_CRAFT_OBJECT(craft)))) {
			msg_to_char(ch, "Creates %s: %s\r\n", VEH_OR_BLD(veh), VEH_SHORT_DESC(veh));
		}
		else {
			msg_to_char(ch, "This craft appears to be broken\r\n");
		}
	}
	else if (CRAFT_FLAGGED(craft, CRAFT_SOUP)) {
		msg_to_char(ch, "Creates liquid: %d unit%s of %s\r\n", GET_CRAFT_QUANTITY(craft), PLURAL(GET_CRAFT_QUANTITY(craft)), get_generic_string_by_vnum(GET_CRAFT_OBJECT(craft), GENERIC_LIQUID, GSTR_LIQUID_NAME));
	}
	else if ((proto = obj_proto(GET_CRAFT_OBJECT(craft)))) {
		craft_level = get_craft_scale_level(ch, craft);
		// build info string
		sprintf(buf, " (%s", item_types[(int) GET_OBJ_TYPE(proto)]);
		if (GET_OBJ_MIN_SCALE_LEVEL(proto) > 0 || GET_OBJ_MAX_SCALE_LEVEL(proto) > 0) {
			sprintf(buf + strlen(buf), ", level %d (%s)", craft_level, level_range_string(GET_OBJ_MIN_SCALE_LEVEL(proto), GET_OBJ_MAX_SCALE_LEVEL(proto), 0));
		}
		else if (OBJ_FLAGGED(proto, OBJ_SCALABLE) && craft_level > 0) {
			sprintf(buf + strlen(buf), ", level %d", craft_level);
		}
		if (GET_OBJ_WEAR(proto) & ~ITEM_WEAR_TAKE) {
			prettier_sprintbit(GET_OBJ_WEAR(proto) & ~ITEM_WEAR_TAKE, wear_bits, part);
			sprintf(buf + strlen(buf), ", %s", part);
		}
		LL_FOREACH(GET_OBJ_APPLIES(proto), apply) {
			// don't show applies that can't come from crafting
			if (apply->apply_type != APPLY_TYPE_HARD_DROP && apply->apply_type != APPLY_TYPE_GROUP_DROP && apply->apply_type != APPLY_TYPE_BOSS_DROP) {
				sprintf(buf + strlen(buf), ", %s%s%s", (apply->modifier<0 ? "-" : "+"), apply_types[(int) apply->location], (apply->apply_type == APPLY_TYPE_SUPERIOR ? " if superior" : ""));
			}
		}
		if (GET_OBJ_AFF_FLAGS(proto)) {
			prettier_sprintbit(GET_OBJ_AFF_FLAGS(proto), affected_bits, part);
			sprintf(buf + strlen(buf), ", %s", part);
		}
		
		// ITEM_x: type-based 'craft info'; must start with ", "
		switch (GET_OBJ_TYPE(proto)) {
			case ITEM_WEAPON: {
				sprintf(buf + strlen(buf), ", %s attack, speed %.2f", get_attack_name_by_vnum(GET_WEAPON_TYPE(proto)), get_weapon_speed(proto));
				break;
			}
			case ITEM_ARMOR: {
				sprintf(buf + strlen(buf), ", %s armor", armor_types[GET_ARMOR_TYPE(proto)]);
				break;
			}
			case ITEM_MISSILE_WEAPON: {
				sprintf(buf + strlen(buf), ", %s attack, speed %.2f", get_attack_name_by_vnum(GET_MISSILE_WEAPON_TYPE(proto)), get_weapon_speed(proto));
				break;
			}
			case ITEM_AMMO: {
				if (GET_AMMO_QUANTITY(proto) > 1) {
					sprintf(buf + strlen(buf), ", quantity: %d\r\n", GET_AMMO_QUANTITY(proto));
				}
				break;
			}
			case ITEM_LIGHTER: {
				if (GET_LIGHTER_USES(proto) == UNLIMITED) {
					strcat(buf, ", unlimited");
				}
				else {
					sprintf(buf + strlen(buf), ", %d use%s\r\n", GET_LIGHTER_USES(proto), PLURAL(GET_LIGHTER_USES(proto)));
				}
				break;
			}
			case ITEM_LIGHT: {
				if (GET_LIGHT_HOURS_REMAINING(proto) == UNLIMITED) {
					strcat(buf, ", unlimited");
				}
				else {
					sprintf(buf + strlen(buf), "%d hour%s of light", GET_LIGHT_HOURS_REMAINING(proto), PLURAL(GET_LIGHT_HOURS_REMAINING(proto)));
				}
				prettier_sprintbit(GET_LIGHT_FLAGS(proto), light_flags, part);
				if (*part) {
					sprintf(buf + strlen(buf), ", %s", part);
				}
				break;
			}
		}	// end 'type' portion of info string
		
		// tool flags in info string
		if (GET_OBJ_TOOL_FLAGS(proto)) {
			prettier_sprintbit(GET_OBJ_TOOL_FLAGS(proto), tool_flags, part);
			sprintf(buf + strlen(buf), ", %s", part);
		}
		
		// some extra flags
		if (GET_OBJ_EXTRA(proto) & show_flags) {
			prettier_sprintbit(GET_OBJ_EXTRA(proto) & show_flags, extra_bits, part);
			sprintf(buf + strlen(buf), ", %s", part);
		}
		
		// end info string
		strcat(buf, ")");
		strtolower(buf);
		
		if (GET_CRAFT_QUANTITY(craft) == 1) {
			msg_to_char(ch, "Creates: %s%s\r\n", get_obj_name_by_proto(GET_CRAFT_OBJECT(craft)), buf);
		}
		else {
			msg_to_char(ch, "Creates: %dx %s%s\r\n", GET_CRAFT_QUANTITY(craft), get_obj_name_by_proto(GET_CRAFT_OBJECT(craft)), buf);
		}
	}
	
	if (GET_CRAFT_REQUIRES_OBJ(craft) != NOTHING) {
		msg_to_char(ch, "Requires: %s\r\n", get_obj_name_by_proto(GET_CRAFT_REQUIRES_OBJ(craft)));
	}
	
	if (GET_CRAFT_ABILITY(craft) != NO_ABIL) {
		sprintf(buf, "%s%s", (has_ability(ch, GET_CRAFT_ABILITY(craft)) ? "" : "\tr"), get_ability_name_by_vnum(GET_CRAFT_ABILITY(craft)));
		if ((abil = find_ability_by_vnum(GET_CRAFT_ABILITY(craft))) && ABIL_ASSIGNED_SKILL(abil) != NULL) {
			sprintf(buf + strlen(buf), " (%s %d)", SKILL_NAME(ABIL_ASSIGNED_SKILL(abil)), ABIL_SKILL_LEVEL(abil));
		}
		if (!has_ability(ch, GET_CRAFT_ABILITY(craft))) {
			sprintf(buf + strlen(buf), " (not learned)\t0");
		}
		if (abil && ABIL_MASTERY_ABIL(abil) != NOTHING) {
			sprintf(buf + strlen(buf), ", Mastery: %s%s%s", (has_ability(ch, ABIL_MASTERY_ABIL(abil)) ? "" : "\tr"), get_ability_name_by_vnum(ABIL_MASTERY_ABIL(abil)), has_ability(ch, ABIL_MASTERY_ABIL(abil)) ? "" : " (not learned)\t0");
		}
		msg_to_char(ch, "Requires: %s\r\n", buf);
	}
	
	if (GET_CRAFT_MIN_LEVEL(craft) > 0) {
		msg_to_char(ch, "Requires: crafting level %d\r\n", GET_CRAFT_MIN_LEVEL(craft));
	}
	
	if (GET_CRAFT_REQUIRES_FUNCTION(craft)) {
		prettier_sprintbit(GET_CRAFT_REQUIRES_FUNCTION(craft), function_flags_long, buf);
		if (*buf) {
			msg_to_char(ch, "Must be: %s\r\n", buf);
		}
	}
	
	if (GET_CRAFT_REQUIRES_TOOL(craft)) {
		prettier_sprintbit(GET_CRAFT_REQUIRES_TOOL(craft), tool_flags, part);
		msg_to_char(ch, "Requires tool%s: %s\r\n", (count_bits(GET_CRAFT_REQUIRES_TOOL(craft)) != 1) ? "s" : "", part);
	}
	
	prettier_sprintbit(GET_CRAFT_FLAGS(craft), craft_flag_for_info, part);
	msg_to_char(ch, "Notes: %s\r\n", part);
	
	if (GET_CRAFT_BUILD_ON(craft)) {
		ordered_sprintbit(GET_CRAFT_BUILD_ON(craft), bld_on_flags, bld_on_flags_order, TRUE, buf);
		msg_to_char(ch, "Build on: %s\r\n", buf);
	}
	if (GET_CRAFT_BUILD_FACING(craft)) {
		ordered_sprintbit(GET_CRAFT_BUILD_FACING(craft), bld_on_flags, bld_on_flags_order, TRUE, buf);
		msg_to_char(ch, "Build facing: %s\r\n", buf);
	}
	
	show_resource_list(GET_CRAFT_RESOURCES(craft), buf);
	msg_to_char(ch, "Resources: %s\r\n", buf);	
	
	if (IS_SET(GET_CRAFT_FLAGS(craft), CRAFT_LEARNED) && !has_learned_craft(ch, GET_CRAFT_VNUM(craft))) {
		msg_to_char(ch, "&rYou haven't learned this recipe.&0\r\n");
	}
}


// for do_tame
// INTERACTION_FUNC provides: ch, interaction, inter_room, inter_mob, inter_item, inter_veh
INTERACTION_FUNC(tame_interact) {
	char buf[MAX_STRING_LENGTH];
	char_data *newmob;
	bool any = FALSE;
	double prc;
	int iter;
	
	for (iter = 0; iter < interaction->quantity; ++iter) {
		newmob = read_mobile(interaction->vnum, TRUE);
		if (!newmob) {
			continue;
		}
		
		setup_generic_npc(newmob, GET_LOYALTY(inter_mob), MOB_DYNAMIC_NAME(inter_mob), MOB_DYNAMIC_SEX(inter_mob));
		char_to_room(newmob, IN_ROOM(ch));
		MOB_INSTANCE_ID(newmob) = MOB_INSTANCE_ID(inter_mob);
		if (MOB_INSTANCE_ID(newmob) != NOTHING) {
			add_instance_mob(real_instance(MOB_INSTANCE_ID(newmob)), GET_MOB_VNUM(newmob));
		}

		prc = (double)GET_HEALTH(inter_mob) / MAX(1, GET_MAX_HEALTH(inter_mob));
		set_health(newmob, (int)(prc * GET_MAX_HEALTH(newmob)));
		
		// message before triggering
		if (!any) {
			if (interaction->quantity > 1) {
				sprintf(buf, "$e is now $N (x%d)!", interaction->quantity);
				act(buf, FALSE, inter_mob, NULL, newmob, TO_ROOM);
			}
			else {
				act("$e is now $N!", FALSE, inter_mob, NULL, newmob, TO_ROOM);
			}
			
			any = TRUE;
		}
		
		load_mtrigger(newmob);
	}
	
	return any;
}


 //////////////////////////////////////////////////////////////////////////////
//// GENERIC CRAFT (craft, forge, sew, cook) /////////////////////////////////

#define SOUP_TIMER  336  // hours


// CRAFT_TYPE_x
const struct gen_craft_data_t gen_craft_data[] = {
	{ "error", "erroring", NOBITS, { "", "", "" } },	// dummy to require scmd
	
	// Note: These correspond to CRAFT_TYPE_x so you cannot change the order.
	// strings are { periodic message to ch, periodic message to room, long desc to room }
	// - the messages may have one %s for the craft name. The long desc will automatically add a/an to the beginning of it.
	{ "forge", "forging", NOBITS, { "You hit the %s on the anvil hard with $p!", "$n hits the %s on the anvil hard with $p!", "$n is forging %s." } },
	{ "craft", "crafting", NOBITS, { "You continue crafting the %s...", "$n continues crafting the %s...", "$n is crafting %s." } },
	{ "cook", "cooking", ACTF_FAST_CHORES, { "You continue cooking the %s...", "$n continues cooking the %s...", "$n is cooking %s." } },
	{ "sew", "sewing", NOBITS, { "You carefully sew the %s...", "$n carefully sews the %s...", "$n is sewing %s." } },
	{ "mill", "milling", NOBITS, { "You grind the millstone, making %s...", "$n grinds the millstone, making %s...", "$n grinds the millstone, making %s." } },
	{ "brew", "brewing", NOBITS, { "You stir the potion and infuse it with mana...", "$n stirs the potion...", "$n stirs a potion." } },
	{ "mix", "mixing", NOBITS, { "The poison bubbles as you stir it...", "$n stirs the bubbling poison...", "$n stirs a bubbling poison." } },
									// note: build does not use the message strings
	{ "build", "building", ACTF_HASTE | ACTF_FAST_CHORES, { "You work on building the %s...", "$n works on the building the %s...", "$n is building %s." } },
	{ "weave", "weaving", NOBITS, { "You carefully weave the %s...", "$n carefully weaves the %s...", "$n is weaving %s." } },
	
	{ "workforce", "making", NOBITS, { "You work on the %s...", "$n works on the %s...", "$n is working dilligently." } },	// not used by players
	
	{ "manufacture", "manufacturing", ACTF_HASTE | ACTF_FAST_CHORES, { "You carefully manufacture the %s...", "$n carefully manufactures the %s...", "$n is manufacturing %s." } },
	{ "smelt", "smelting", ACTF_FAST_CHORES, { "You smelt the %s in the fire...", "$n smelts the %s in the fire...", "$n is smelting %s." } },
	{ "press", "pressing", NOBITS, { "You press the %s...", "$n presses the %s...", "$n is working the press." } },
	
	{ "bake", "baking", ACTF_FAST_CHORES, { "You wait for the %s to bake...", "$n waits for the %s to bake...", "$n is baking %s." } },
	{ "make", "making", NOBITS, { "You work on the %s...", "$n works on the %s...", "$n is making %s." } },
};


void cancel_gen_craft(char_data *ch) {
	craft_data *type = craft_proto(GET_ACTION_VNUM(ch, 0));
	obj_data *obj;
	
	if (type && !CRAFT_IS_VEHICLE(type)) {
		// refund the real resources they used
		give_resources(ch, GET_ACTION_RESOURCES(ch), FALSE);
		free_resource_list(GET_ACTION_RESOURCES(ch));
		GET_ACTION_RESOURCES(ch) = NULL;

		// load the drink container back
		if (IS_SET(GET_CRAFT_FLAGS(type), CRAFT_SOUP)) {
			obj = read_object(GET_ACTION_VNUM(ch, 1), TRUE);

			// just empty it
			set_obj_val(obj, VAL_DRINK_CONTAINER_CONTENTS, 0);
			if (CAN_WEAR(obj, ITEM_WEAR_TAKE)) {
				obj_to_char(obj, ch);
			}
			else {
				obj_to_room(obj, IN_ROOM(ch));
			}
			if (load_otrigger(obj) && obj->carried_by) {
				get_otrigger(obj, obj->carried_by, FALSE);
			}
		}
		
		if (CRAFT_FLAGGED(type, CRAFT_TAKE_REQUIRED_OBJ) && GET_CRAFT_REQUIRES_OBJ(type) != NOTHING && obj_proto(GET_CRAFT_REQUIRES_OBJ(type))) {
			obj = read_object(GET_CRAFT_REQUIRES_OBJ(type), TRUE);
			
			// scale item to minimum level
			scale_item_to_level(obj, 0);
			
			if (IS_NPC(ch)) {
				obj_to_room(obj, IN_ROOM(ch));
			}
			else {
				obj_to_char(obj, ch);
				act("You get $p.", FALSE, ch, obj, NULL, TO_CHAR);
				
				// ensure binding
				if (OBJ_FLAGGED(obj, OBJ_BIND_FLAGS)) {
					bind_obj_to_player(obj, ch);
				}
			}
			if (load_otrigger(obj) && obj->carried_by) {
				get_otrigger(obj, obj->carried_by, FALSE);
			}
		}
	}
	
	end_action(ch);
}


/**
* This function locates the entry in craft_table that creates an object
* with a given vnum.
*
* @param obj_vnum vnum The object vnum to lookup a craft for.
* @return craft_data* The prototype of the craft for that object, or NULL if none exists.
*/
craft_data *find_craft_for_obj_vnum(obj_vnum vnum) {
	craft_data *iter, *next_iter;
	
	HASH_ITER(hh, craft_table, iter, next_iter) {
		if (!IS_SET(GET_CRAFT_FLAGS(iter), CRAFT_SOUP) && !CRAFT_IS_VEHICLE(iter) && !CRAFT_IS_BUILDING(iter) && GET_CRAFT_OBJECT(iter) == vnum) {
			return iter;
		}
	}
	
	return NULL;
}


void finish_gen_craft(char_data *ch) {
	any_vnum obj_vnum = GET_ACTION_VNUM(ch, 1);
	craft_data *type = craft_proto(GET_ACTION_VNUM(ch, 0));
	bool applied_master = FALSE, is_master = FALSE;
	int num = GET_ACTION_VNUM(ch, 2);
	char lbuf[MAX_INPUT_LENGTH];
	struct resource_data *res;
	ability_data *cft_abil;
	obj_data *proto, *temp_obj, *obj = NULL;
	int iter, amt = 1, obj_ok = 0;
	
	cft_abil = find_ability_by_vnum(GET_CRAFT_ABILITY(type));
	is_master = (cft_abil && ABIL_MASTERY_ABIL(cft_abil) != NOTHING && has_ability(ch, ABIL_MASTERY_ABIL(cft_abil)));

	// basic sanity checking (vehicles are finished elsewhere)
	if (!type || CRAFT_IS_VEHICLE(type) || CRAFT_IS_BUILDING(type)) {
		return;
	}
	
	// soup handling
	if (IS_SET(GET_CRAFT_FLAGS(type), CRAFT_SOUP)) {
		// load the drink container back
		obj = read_object(obj_vnum, TRUE);
	
		set_obj_val(obj, VAL_DRINK_CONTAINER_CONTENTS, MIN(GET_CRAFT_QUANTITY(type), GET_DRINK_CONTAINER_CAPACITY(obj)));
		set_obj_val(obj, VAL_DRINK_CONTAINER_TYPE, GET_CRAFT_OBJECT(type));
	
		// set it to go bad... very bad
		GET_OBJ_TIMER(obj) = SOUP_TIMER;
		if (CAN_WEAR(obj, ITEM_WEAR_TAKE)) {
			obj_to_char(obj, ch);
		}
		else {
			obj_to_room(obj, IN_ROOM(ch));
		}
		scale_item_to_level(obj, get_craft_scale_level(ch, type));
		
		obj_ok = load_otrigger(obj);
		if (obj_ok && obj->carried_by) {
			get_otrigger(obj, obj->carried_by, FALSE);
		}
	}
	else if (GET_CRAFT_QUANTITY(type) > 0) {
		// NON-SOUP (careful, soup uses quantity for maximum contents
	
		amt = GET_CRAFT_QUANTITY(type);
	
		if (GET_CRAFT_TYPE(type) == CRAFT_TYPE_MILL && has_player_tech(ch, PTECH_MILL_UPGRADE)) {
			gain_player_tech_exp(ch, PTECH_MILL_UPGRADE, 10);
			run_ability_hooks_by_player_tech(ch, PTECH_MILL_UPGRADE, NULL, NULL, NULL, NULL);
			amt *= 2;
		}

		if (obj_proto(GET_CRAFT_OBJECT(type))) {
			for (iter = 0; iter < amt; ++iter) {
				// load and master it
				obj = read_object(GET_CRAFT_OBJECT(type), TRUE);
				if (OBJ_FLAGGED(obj, OBJ_SCALABLE) && is_master) {
					applied_master = TRUE;
					SET_BIT(GET_OBJ_EXTRA(obj), OBJ_SUPERIOR);
				}
				scale_item_to_level(obj, get_craft_scale_level(ch, type));
	
				// where to put it
				if (CAN_WEAR(obj, ITEM_WEAR_TAKE)) {
					obj_to_char(obj, ch);
				}
				else {
					obj_to_room(obj, IN_ROOM(ch));
				}
				
				obj_ok = load_otrigger(obj);
				if (obj_ok && obj->carried_by) {
					get_otrigger(obj, obj->carried_by, FALSE);
				}
			}
			
			// mark for the empire
			if (GET_LOYALTY(ch)) {
				add_production_total(GET_LOYALTY(ch), GET_CRAFT_OBJECT(type), amt);
			}
		}
	}

	// send message -- soup uses quantity for amount of soup instead of multiple items
	if (!obj_ok) {
		// object self-purged?
		sprintf(buf, "You finish %s!", gen_craft_data[GET_CRAFT_TYPE(type)].verb);
		act(buf, FALSE, ch, NULL, NULL, TO_CHAR);
	}
	else if (amt > 1 && !IS_SET(GET_CRAFT_FLAGS(type), CRAFT_SOUP)) {
		sprintf(buf, "You finish %s $p (x%d)!", gen_craft_data[GET_CRAFT_TYPE(type)].verb, amt);
		act(buf, FALSE, ch, obj, 0, TO_CHAR);
	}
	else {
		sprintf(buf, "You finish %s $p!", gen_craft_data[GET_CRAFT_TYPE(type)].verb);
		act(buf, FALSE, ch, obj, 0, TO_CHAR);
	}
	
	// to-room message
	if (obj_ok) {
		sprintf(buf, "$n finishes %s $p!", gen_craft_data[GET_CRAFT_TYPE(type)].verb);
		act(buf, FALSE, ch, obj, NULL, TO_ROOM);
	}
	else {
		sprintf(buf, "$n finishes %s!", gen_craft_data[GET_CRAFT_TYPE(type)].verb);
		act(buf, FALSE, ch, NULL, NULL, TO_ROOM);
	}
	
	if (GET_CRAFT_ABILITY(type) != NO_ABIL) {
		gain_ability_exp(ch, GET_CRAFT_ABILITY(type), 33.4);
		run_ability_hooks(ch, AHOOK_ABILITY, GET_CRAFT_ABILITY(type), 0, NULL, (obj_ok ? obj : NULL), NULL, NULL, NOBITS);
	}
	
	// master?
	if (is_master && applied_master) {
		gain_ability_exp(ch, ABIL_MASTERY_ABIL(cft_abil), 33.4);
		run_ability_hooks(ch, AHOOK_ABILITY, ABIL_MASTERY_ABIL(cft_abil), 0, NULL, (obj_ok ? obj : NULL), NULL, NULL, NOBITS);
	}
	
	// remove 'produced' amounts from the empire now, if applicable
	if (CRAFT_FLAGGED(type, CRAFT_REMOVE_PRODUCTION) && GET_LOYALTY(ch)) {
		LL_FOREACH(GET_ACTION_RESOURCES(ch), res) {
			add_production_total(GET_LOYALTY(ch), res->vnum, -(res->amount));
		}
	}
	
	// check for consumes-to on the resources
	if (!CRAFT_FLAGGED(type, CRAFT_SKIP_CONSUMES_TO)) {
		LL_FOREACH(GET_ACTION_RESOURCES(ch), res) {
			if ((proto = obj_proto(res->vnum)) && has_interaction(GET_OBJ_INTERACTIONS(proto), INTERACT_CONSUMES_TO)) {
				temp_obj = read_object(res->vnum, FALSE);
				obj_to_char(temp_obj, ch);
				for (iter = 0; iter < res->amount; ++iter) {
					run_interactions(ch, GET_OBJ_INTERACTIONS(temp_obj), INTERACT_CONSUMES_TO, IN_ROOM(ch), NULL, temp_obj, NULL, consumes_or_decays_interact);
				}
				extract_obj(temp_obj);
			}
		}
	}
	
	// end now, which frees up the resources
	end_action(ch);

	// repeat as desired
	if (num > 1) {
		sprintf(lbuf, "%d %s", num-1, GET_CRAFT_NAME(type));
		do_gen_craft(ch, lbuf, 0, GET_CRAFT_TYPE(type));
	}
}


/**
* Action processor for vehicle gen_crafts.
*
* @param char_data *ch The person crafting it.
* @param craft_data *type The craft recipe.
*/
void process_gen_craft_vehicle(char_data *ch, craft_data *type) {
	bool found = FALSE;
	char buf[MAX_STRING_LENGTH];
	obj_data *found_obj = NULL;
	struct resource_data *res, temp_res;
	vehicle_data *veh, *junk;
	char_data *vict;
	
	// basic setup
	if (!type || !check_can_craft(ch, type, TRUE) || !(veh = find_finishable_vehicle(ch, NULL, GET_ACTION_VNUM(ch, 1), &junk)) || VEH_IS_DISMANTLING(veh)) {
		cancel_gen_craft(ch);
		return;
	}
	if (VEH_IS_DISMANTLING(veh)) {
		act("You can't work on $V because it's being dismantled.", FALSE, ch, NULL, veh, TO_CHAR | ACT_VEH_VICT);
		cancel_gen_craft(ch);
		return;
	}
	if (!CRAFT_FLAGGED(type, CRAFT_DARK_OK) && !can_see_in_dark_room(ch, IN_ROOM(ch), TRUE)) {
		msg_to_char(ch, "It's too dark to finish %s.\r\n", gen_craft_data[GET_CRAFT_TYPE(type)].verb);
		cancel_gen_craft(ch);
		return;
	}
	
	// find and apply something
	if ((res = get_next_resource(ch, VEH_NEEDS_RESOURCES(veh), can_use_room(ch, IN_ROOM(ch), MEMBERS_ONLY), TRUE, &found_obj))) {
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
		
		// take the item; possibly free the res
		apply_resource(ch, res, &VEH_NEEDS_RESOURCES(veh), found_obj, APPLY_RES_CRAFT, veh, VEH_FLAGGED(veh, VEH_NEVER_DISMANTLE) ? NULL : &VEH_BUILT_WITH(veh));
		request_vehicle_save_in_world(veh);
		
		// experience per resource
		if (GET_CRAFT_ABILITY(type) != NO_ABIL) {
			gain_ability_exp(ch, GET_CRAFT_ABILITY(type), 3);
			run_ability_hooks(ch, AHOOK_ABILITY, GET_CRAFT_ABILITY(type), 0, NULL, NULL, veh, NULL, NOBITS);
		}
		
		found = TRUE;
	}
	
	// done?
	if (!VEH_NEEDS_RESOURCES(veh)) {
		act("$V is finished!", FALSE, ch, NULL, veh, TO_CHAR | TO_ROOM | ACT_VEH_VICT);
		
		// WARNING: complete_vehicle runs triggers that could purge the vehicle
		complete_vehicle(veh);
		
		// stop all actors on this type
		DL_FOREACH2(ROOM_PEOPLE(IN_ROOM(ch)), vict, next_in_room) {
			if (!IS_NPC(vict) && GET_ACTION(vict) == ACT_GEN_CRAFT && GET_ACTION_VNUM(vict, 0) == GET_CRAFT_VNUM(type)) {
				end_action(vict);
			}
		}
	}
	else if (!found) {
		// missing next resource
		if (VEH_NEEDS_RESOURCES(veh)) {
			// copy this to display the next 1
			temp_res = *VEH_NEEDS_RESOURCES(veh);
			temp_res.next = NULL;
			show_resource_list(&temp_res, buf);
			msg_to_char(ch, "You don't have %s and stop %s.\r\n", buf, gen_craft_data[GET_CRAFT_TYPE(type)].verb);
		}
		else {
			msg_to_char(ch, "You run out of resources and stop %s.\r\n", gen_craft_data[GET_CRAFT_TYPE(type)].verb);
		}
		snprintf(buf, sizeof(buf), "$n runs out of resources and stops %s.", gen_craft_data[GET_CRAFT_TYPE(type)].verb);
		act(buf, FALSE, ch, NULL, NULL, TO_ROOM);
		end_action(ch);
	}
}


/**
* Action processor for gen_craft.
*
* @param char_data *ch The actor.
*/
void process_gen_craft(char_data *ch) {
	char buf[MAX_STRING_LENGTH], *str, *ptr;
	obj_data *weapon = NULL, *tool = NULL;
	craft_data *type = craft_proto(GET_ACTION_VNUM(ch, 0));
	bool has_mill = FALSE;
	
	if (!type) {
		cancel_gen_craft(ch);
		return;
	}
	
	// pass off control entirely for a vehicle craft
	if (CRAFT_IS_VEHICLE(type)) {
		process_gen_craft_vehicle(ch, type);
		return;
	}
	else if (CRAFT_IS_BUILDING(type)) {
		process_build_action(ch);
		return;
	}
	
	// things that check for & set weapon
	if (GET_CRAFT_TYPE(type) == CRAFT_TYPE_FORGE && (!(weapon = has_tool(ch, TOOL_HAMMER)) || !can_forge(ch))) {
		// can_forge sends its own message
		cancel_gen_craft(ch);
	}
	else if (GET_CRAFT_TYPE(type) == CRAFT_TYPE_SEW && !(weapon = has_tool(ch, TOOL_SEWING_KIT)) && !room_has_function_and_city_ok(GET_LOYALTY(ch), IN_ROOM(ch), FNC_TAILOR)) {
		msg_to_char(ch, "You need to equip a sewing kit to sew this, or sew it at a tailor.\r\n");
		cancel_gen_craft(ch);
	}
	else if (GET_CRAFT_TYPE(type) == CRAFT_TYPE_WEAVE && !room_has_function_and_city_ok(GET_LOYALTY(ch), IN_ROOM(ch), FNC_TAILOR) && !has_tool(ch, TOOL_LOOM)) {
		msg_to_char(ch, "You need a loom to keep weaving.\r\n");
		cancel_gen_craft(ch);
	}
	else if (GET_CRAFT_TYPE(type) == CRAFT_TYPE_MILL && !(has_mill = room_has_function_and_city_ok(GET_LOYALTY(ch), IN_ROOM(ch), FNC_MILL)) &&  !has_tool(ch, TOOL_GRINDING_STONE)) {
		msg_to_char(ch, "You need to be in a mill or have a grinding stone to do keep milling.\r\n");
		cancel_gen_craft(ch);
	}
	else if (!CRAFT_FLAGGED(type, CRAFT_DARK_OK) && !can_see_in_dark_room(ch, IN_ROOM(ch), TRUE)) {
		msg_to_char(ch, "It's too dark to finish %s.\r\n", gen_craft_data[GET_CRAFT_TYPE(type)].verb);
		cancel_gen_craft(ch);
	}
	else if (GET_CRAFT_REQUIRES_TOOL(type) && !(tool = has_all_tools(ch, GET_CRAFT_REQUIRES_TOOL(type)))) {
		msg_to_char(ch, "You aren't using the right tool to finish %s.\r\n", gen_craft_data[GET_CRAFT_TYPE(type)].verb);
		cancel_gen_craft(ch);
	}
	else if (GET_CRAFT_REQUIRES_FUNCTION(type) && !room_has_function_and_city_ok(GET_LOYALTY(ch), IN_ROOM(ch), GET_CRAFT_REQUIRES_FUNCTION(type))) {
		prettier_sprintbit(GET_CRAFT_REQUIRES_FUNCTION(type), function_flags_long, buf);
		str = buf;
		if ((ptr = strrchr(str, ','))) {
			msg_to_char(ch, "You must be %-*.*s or%s to keep %s that.\r\n", (int)(ptr-str), (int)(ptr-str), str, ptr+1, gen_craft_data[GET_CRAFT_TYPE(type)].verb);
		}
		else {	// no comma
			msg_to_char(ch, "You must be %s to keep %s that.\r\n", buf, gen_craft_data[GET_CRAFT_TYPE(type)].verb);
		}
		cancel_gen_craft(ch);
	}
	else {
		GET_ACTION_TIMER(ch) -= 1;

		// bonus point for superior tool -- if this craft requires a tool
		if (weapon && OBJ_FLAGGED(weapon, OBJ_SUPERIOR)) {
			GET_ACTION_TIMER(ch) -= 1;
		}
		else if (tool && OBJ_FLAGGED(tool, OBJ_SUPERIOR)) {
			GET_ACTION_TIMER(ch) -= 1;
		}
		
		// mill penalty if using TOOL_GRINDING_STONE instead of FNC_MILL
		if (GET_CRAFT_TYPE(type) == CRAFT_TYPE_MILL && !has_mill && !number(0, 1)) {
			GET_ACTION_TIMER(ch) += 1;	// PENALTY
		}
		
		// tailor bonus for weave
		if (GET_CRAFT_TYPE(type) == CRAFT_TYPE_WEAVE && room_has_function_and_city_ok(GET_LOYALTY(ch), IN_ROOM(ch), FNC_TAILOR) && IS_COMPLETE(IN_ROOM(ch))) {
			GET_ACTION_TIMER(ch) -= 3;
		}

		if (GET_ACTION_TIMER(ch) <= 0) {
			finish_gen_craft(ch);
		}
		else {
			// messages just 50% of the time
			
			if (!number(0, 1)) {
				// message to char
				if (strstr(gen_craft_data[GET_CRAFT_TYPE(type)].strings[GCD_STRING_TO_CHAR], "%s")) {
					sprintf(buf, gen_craft_data[GET_CRAFT_TYPE(type)].strings[GCD_STRING_TO_CHAR], GET_CRAFT_NAME(type));
					act(buf, FALSE, ch, weapon, NULL, TO_CHAR | TO_SPAMMY);
				}
				else {
					act(gen_craft_data[GET_CRAFT_TYPE(type)].strings[GCD_STRING_TO_CHAR], FALSE, ch, weapon, NULL, TO_CHAR | TO_SPAMMY);
				}
			
				// message to room
				if (strstr(gen_craft_data[GET_CRAFT_TYPE(type)].strings[GCD_STRING_TO_ROOM], "%s")) {
					sprintf(buf, gen_craft_data[GET_CRAFT_TYPE(type)].strings[GCD_STRING_TO_ROOM], GET_CRAFT_NAME(type));
					act(buf, FALSE, ch, weapon, NULL, TO_ROOM | TO_SPAMMY);
				}
				else {
					act(gen_craft_data[GET_CRAFT_TYPE(type)].strings[GCD_STRING_TO_ROOM], FALSE, ch, weapon, NULL, TO_ROOM | TO_SPAMMY);
				}
			}
		}
	}
}


 //////////////////////////////////////////////////////////////////////////////
//// REFORGE / REFASHION /////////////////////////////////////////////////////

const struct {
	char *command, *command_3rd;
	any_vnum ability;	// required ability
	bool (*validate_func)(char_data *ch);	// e.g. can_forge, func that returns TRUE if ok -- must send own errors if FALSE
	int types[4];	// NOTHING-terminated list of valid obj types
} reforge_data[] = {
	{ "reforge", "reforges", ABIL_REFORGE, can_forge, { ITEM_WEAPON, ITEM_MISSILE_WEAPON, NOTHING } },	// SCMD_REFORGE
	{ "refashion", "refashions", ABIL_REFASHION, can_refashion, { ITEM_ARMOR, ITEM_SHIELD, ITEM_WORN, NOTHING } }	// SCMD_REFASHION
};


/**
* @param char_data *ch The person trying to refashion.
* @return bool TRUE if they can, false if they can't.
*/
bool can_refashion(char_data *ch) {
	bool ok = FALSE;
	
	if (!IS_IMMORTAL(ch) && !room_has_function_and_city_ok(GET_LOYALTY(ch), IN_ROOM(ch), FNC_TAILOR)) {
		msg_to_char(ch, "You need to be at the tailor to do that.\r\n");
	}
	else if (!IS_IMMORTAL(ch) && !check_in_city_requirement(IN_ROOM(ch), TRUE)) {
		msg_to_char(ch, "This tailor only works if it's in a city.\r\n");
	}
	else if (!check_in_city_requirement(IN_ROOM(ch), TRUE)) {
		msg_to_char(ch, "This building must be in a city to use it.\r\n");
	}
	else {
		ok = TRUE;
	}
	
	return ok;
}


/**
* @param obj_data *obj The object to test the type of.
* @int subcmd Any pos in reforge_data[]
* @return bool TRUE if the object matches any valid type, otherwise FALSE.
*/
bool match_reforge_type(obj_data *obj, int subcmd) {
	int iter;
	bool match = FALSE;
	
	for (iter = 0; reforge_data[subcmd].types[iter] != NOTHING && !match; ++iter) {
		if (GET_OBJ_TYPE(obj) == reforge_data[subcmd].types[iter]) {
			match = TRUE;
		}
	}
	
	return match;
}


/**
* This function validates an attempted item rename by a mortal, e.g. using
* reforge. It sends its own error messages.
*
* @param char_data *ch The person doing the renaming.
* @param obj_data *obj The item to be renamed.
* @param char *name The proposed name.
* @return bool TRUE if it's ok to rename, FALSE otherwise.
*/
bool validate_item_rename(char_data *ch, obj_data *obj, char *name) {
	char must_have[MAX_STRING_LENGTH];
	bool ok = FALSE, has_cap = FALSE;
	obj_data *proto;
	int iter;
	
	strcpy(must_have, fname(GET_OBJ_KEYWORDS(obj)));
	
	for (iter = 0; iter < strlen(GET_OBJ_SHORT_DESC(obj)) && !has_cap; ++iter) {
		if (isupper(*(GET_OBJ_SHORT_DESC(obj) + iter))) {
			has_cap = TRUE;
		}
	}
	
	if (!*name) {
		msg_to_char(ch, "What do you want to rename it?\r\n");
	}
	else if (has_cap || ((proto = obj_proto(GET_OBJ_VNUM(obj))) && GET_OBJ_SHORT_DESC(obj) != GET_OBJ_SHORT_DESC(proto))) {
		act("You can't rename $p.", FALSE, ch, obj, NULL, TO_CHAR);
	}
	else if (strchr(name, COLOUR_CHAR) || strchr(name, '%')) {
		msg_to_char(ch, "Item names cannot contain the \t%c or %% symbols.\r\n", COLOUR_CHAR);
	}
	else if (!str_str(name, must_have)) {
		msg_to_char(ch, "The new name must contain '%s'.\r\n", must_have);
	}
	else if (strlen(name) > 40) {
		msg_to_char(ch, "You can't set a name longer than 40 characters.\r\n");
	}
	else {
		ok = TRUE;
	}
	
	return ok;
}


 //////////////////////////////////////////////////////////////////////////////
//// COMMANDS ////////////////////////////////////////////////////////////////

// subcmd is AUGMENT_x
ACMD(do_gen_augment) {
	char buf[MAX_STRING_LENGTH], target_arg[MAX_INPUT_LENGTH], *augment_arg;
	double points_available, remaining, share;
	struct obj_apply *apply, *last_apply;
	int scale, total_weight, value;
	struct apply_data *app;
	ability_data *abil;
	augment_data *aug;
	bool is_master;
	obj_data *obj;
	
	augment_arg = one_argument(argument, target_arg);
	skip_spaces(&augment_arg);
	
	if (IS_NPC(ch)) {
		// TODO should we allow this for scripting?
		msg_to_char(ch, "NPCs can't augment items.\r\n");
	}
	else if (!*target_arg) {
		msg_to_char(ch, "Usage: %s <item> <type>\r\nYou know how to %s:\r\n", augment_info[subcmd].verb, augment_info[subcmd].verb);
		list_available_augments(ch, subcmd, NULL);
	}
	else if (!(obj = get_obj_in_list_vis(ch, target_arg, NULL, ch->carrying)) && !(obj = get_obj_by_char_share(ch, target_arg))) {
		msg_to_char(ch, "You don't seem to have %s %s.\r\n", AN(target_arg), target_arg);
	}
	else if (!*augment_arg) {
		msg_to_char(ch, "You can %s %s with:\r\n", augment_info[subcmd].verb, GET_OBJ_SHORT_DESC(obj));
		list_available_augments(ch, subcmd, obj);
	}
	else if (!(aug = find_augment_by_name(ch, augment_arg, subcmd))) {
		msg_to_char(ch, "You don't know that %s.\r\n", augment_info[subcmd].noun);
	}
	else if (IS_SET(GET_AUG_FLAGS(aug) | augment_info[subcmd].default_flags, AUG_SELF_ONLY) && ((obj->worn_by && obj->worn_by != ch) || !bind_ok(obj, ch))) {
		// targeting a shared item with a self-only enchant
		msg_to_char(ch, "You can only %s your own items like that.\r\n", augment_info[subcmd].verb);
	}
	else if (GET_OBJ_CURRENT_SCALE_LEVEL(obj) <= 0) {
		// always forbidden
		msg_to_char(ch, "You cannot %s that item.\r\n", augment_info[subcmd].verb);
	}
	else if (augment_info[subcmd].use_obj_flag && OBJ_FLAGGED(obj, augment_info[subcmd].use_obj_flag)) {
		msg_to_char(ch, "You cannot %s an item that already has %s %s.\r\n", augment_info[subcmd].verb, AN(augment_info[subcmd].noun), augment_info[subcmd].noun);
	}
	else if (obj_has_apply_type(obj, augment_info[subcmd].apply_type)) {
		msg_to_char(ch, "That item already has %s %s effect.\r\n", AN(augment_info[subcmd].noun), augment_info[subcmd].noun);
	}
	else if (!validate_augment_target(ch, obj, aug, TRUE)) {
		// sends own message
	}
	else if (!has_resources(ch, GET_AUG_RESOURCES(aug), FALSE, TRUE, GET_AUG_NAME(aug))) {
		// sends its own messages
	}
	else if (GET_AUG_ABILITY(aug) != NO_ABIL && ABILITY_TRIGGERS(ch, NULL, obj, GET_AUG_ABILITY(aug))) {
		return;
	}
	else {
		extract_resources(ch, GET_AUG_RESOURCES(aug), FALSE, NULL);
		
		// determine scale cap
		scale = GET_OBJ_CURRENT_SCALE_LEVEL(obj);
		if ((abil = find_ability_by_vnum(GET_AUG_ABILITY(aug))) && ABIL_ASSIGNED_SKILL(abil) != NULL && get_skill_level(ch, SKILL_VNUM(ABIL_ASSIGNED_SKILL(abil))) < CLASS_SKILL_CAP) {
			scale = MIN(scale, get_skill_level(ch, SKILL_VNUM(ABIL_ASSIGNED_SKILL(abil))));
		}
		
		// check mastery
		is_master = (abil && ABIL_MASTERY_ABIL(abil) != NOTHING && has_ability(ch, ABIL_MASTERY_ABIL(abil)));
		
		// determine points
		points_available = get_enchant_scale_for_char(ch, scale);
		if (is_master) {
			points_available *= config_get_double("greater_enchantments_bonus");
		}
		
		// figure out how many total weight points are used
		total_weight = 0;
		for (app = GET_AUG_APPLIES(aug); app; app = app->next) {
			if (!apply_never_scales[app->location]) {
				if (app->weight > 0) {
					total_weight += app->weight;
				}
				else if (app->weight < 0) {
					points_available += ABSOLUTE(app->weight);
				}
			}
		}
		
		// find end of current applies on obj
		if ((last_apply = GET_OBJ_APPLIES(obj))) {
			while (last_apply->next) {
				last_apply = last_apply->next;
			}
		}
		
		// start adding applies
		remaining = points_available;
		LL_FOREACH(GET_AUG_APPLIES(aug), app) {
			apply = NULL;
			
			if (apply_never_scales[app->location]) {	// non-scaling apply
				CREATE(apply, struct obj_apply, 1);
				apply->modifier = app->weight;
			}
			else if (app->weight > 0 && remaining > 0) {	// positive apply
				share = (((double)app->weight) / total_weight) * points_available;	// % of total
				share = MIN(share, remaining);	// check limit
				value = round(share * (1.0 / apply_values[app->location]));
				if (value > 0 || (app == GET_AUG_APPLIES(aug))) {	// always give at least 1 point on the first one
					value = MAX(1, value);
					remaining -= (value * apply_values[app->location]);	// subtract actual amount used
				
					// create the actual apply
					CREATE(apply, struct obj_apply, 1);
					apply->modifier = value;
				}
			}
			else if (app->weight < 0) {	// negative apply
				value = round(app->weight * (1.0 / apply_values[app->location]));
				value = MIN(-1, value);	// minimum of -1
				
				CREATE(apply, struct obj_apply, 1);
				apply->modifier = value;
			}
			
			// add it
			if (apply) {
				apply->apply_type = augment_info[subcmd].apply_type;
				apply->location = app->location;
				LL_APPEND(GET_OBJ_APPLIES(obj), apply);
			}
		}
		
		// enchanted bit*
		if (augment_info[subcmd].use_obj_flag) {
			SET_BIT(GET_OBJ_EXTRA(obj), augment_info[subcmd].use_obj_flag);
		}
		
		// self-only: force binding
		if (IS_SET(GET_AUG_FLAGS(aug) | augment_info[subcmd].default_flags, AUG_SELF_ONLY)) {
			if (!OBJ_FLAGGED(obj, OBJ_BIND_FLAGS)) {
				SET_BIT(GET_OBJ_EXTRA(obj), OBJ_BIND_ON_PICKUP);
			}
			bind_obj_to_player(obj, ch);
			reduce_obj_binding(obj, ch);
		}
		
		sprintf(buf, "You %s $p%s with %s.", augment_info[subcmd].verb, shared_by(obj, ch), GET_AUG_NAME(aug));
		act(buf, FALSE, ch, obj, obj->worn_by, TO_CHAR);
		
		sprintf(buf, "$n %ss $p%s with %s.", augment_info[subcmd].verb, shared_by(obj, ch), GET_AUG_NAME(aug));
		act(buf, FALSE, ch, obj, obj->worn_by, TO_ROOM);
		
		if (GET_AUG_ABILITY(aug) != NO_ABIL) {
			gain_ability_exp(ch, GET_AUG_ABILITY(aug), 50);
			run_ability_hooks(ch, AHOOK_ABILITY, GET_AUG_ABILITY(aug), 0, NULL, obj, NULL, NULL, NOBITS);
		}
		if (abil && is_master) {
			gain_ability_exp(ch, ABIL_MASTERY_ABIL(abil), 50);
			run_ability_hooks(ch, AHOOK_ABILITY, ABIL_MASTERY_ABIL(abil), 0, NULL, obj, NULL, NULL, NOBITS);
		}
		
		command_lag(ch, WAIT_ABILITY);
		request_obj_save_in_world(obj);
	}
}


/**
* Sub-processor for making a map building.
*
* @param char_data *ch The player trying to make the building.
* @param craft_data *type Pre-selected building craft (CRAFT_IS_BUILDING == TRUE).
* @param int dir The direction specified by the player (may be NO_DIR or an invalid dir).
*/
void do_gen_craft_building(char_data *ch, craft_data *type, int dir) {
	bool junk, is_closed, needs_reverse;
	char buf[MAX_STRING_LENGTH];
	room_data *to_room, *to_rev;
	obj_data *found_obj;
	bld_data *to_build;
	int ter_type;
	
	char *command = (type ? gen_craft_data[GET_CRAFT_TYPE(type)].command : "make");

	// basic sanitation
	if (!type || !CRAFT_IS_BUILDING(type) || !(to_build = building_proto(GET_CRAFT_BUILD_TYPE(type)))) {
		log("SYSERR: do_gen_craft_building called with invalid building craft %d", type ? GET_CRAFT_VNUM(type) : NOTHING);
		msg_to_char(ch, "That is not yet implemented.\r\n");
		return;
	}
	
	// found-obj has been pre-validated so if we need it, it must exist here
	found_obj = (GET_CRAFT_REQUIRES_OBJ(type) != NOTHING ? has_required_obj_for_craft(ch, GET_CRAFT_REQUIRES_OBJ(type)) : NULL);
	
	// validate
	if (!check_build_location_and_dir(ch, IN_ROOM(ch), type, dir, FALSE, &is_closed, &needs_reverse)) {
		return;	// sends own messages
	}
	else if (found_obj && !consume_otrigger(found_obj, ch, OCMD_BUILD, NULL)) {
		return;	// the trigger should send its own message if it prevented this
	}
	
	// ok: ready to build
	
	// bind or take the obj
	if (found_obj) {
		if (CRAFT_FLAGGED(type, CRAFT_TAKE_REQUIRED_OBJ)) {
			act("You use $p.", FALSE, ch, found_obj, NULL, TO_CHAR);
			extract_obj(found_obj);
		}
		else if (OBJ_BOUND_TO(found_obj)) {
			reduce_obj_binding(found_obj, ch);
		}
		else {	// not yet bound
			bind_obj_to_player(found_obj, ch);
		}
	}
	
	// setup
	construct_building(IN_ROOM(ch), GET_CRAFT_BUILD_TYPE(type));
	set_room_extra_data(IN_ROOM(ch), ROOM_EXTRA_BUILD_RECIPE, GET_CRAFT_VNUM(type));
	if (!IS_NPC(ch)) {
		set_room_extra_data(IN_ROOM(ch), ROOM_EXTRA_ORIGINAL_BUILDER, GET_ACCOUNT(ch)->id);
	}
	
	special_building_setup(ch, IN_ROOM(ch));
	SET_BIT(ROOM_BASE_FLAGS(IN_ROOM(ch)), ROOM_AFF_INCOMPLETE);
	affect_total_room(IN_ROOM(ch));
	GET_BUILDING_RESOURCES(IN_ROOM(ch)) = copy_resource_list(GET_CRAFT_RESOURCES(type));
	
	// can_claim checks total available land, but the outside is check done within this block
	if (!ROOM_OWNER(IN_ROOM(ch)) && can_claim(ch) && !ROOM_AFF_FLAGGED(IN_ROOM(ch), ROOM_AFF_UNCLAIMABLE)) {
		empire_data *emp = get_or_create_empire(ch);
		if (emp) {
			ter_type = get_territory_type_for_empire(IN_ROOM(ch), emp, FALSE, &junk, NULL);
			if (EMPIRE_TERRITORY(emp, ter_type) < land_can_claim(emp, ter_type)) {
				claim_room(IN_ROOM(ch), emp);
			}
		}
	}
	
	// closed buildings: to_room was really pre-validated in check_build_location_and_dir
	if (is_closed && (to_room = real_shift(IN_ROOM(ch), shift_dir[dir][0], shift_dir[dir][1]))) {
		create_exit(IN_ROOM(ch), to_room, dir, FALSE);
		// to_rev was pre-validated as well
		if (needs_reverse && (to_rev = real_shift(IN_ROOM(ch), shift_dir[rev_dir[dir]][0], shift_dir[rev_dir[dir]][1]))) {
			create_exit(IN_ROOM(ch), to_rev, rev_dir[dir], FALSE);
		}
		
		// entrance is the direction you type TO ENTER, so it's the reverse of the facing dir
		COMPLEX_DATA(IN_ROOM(ch))->entrance = rev_dir[dir];
		herd_animals_out(IN_ROOM(ch));
	}
	
	start_action(ch, ACT_BUILDING, 0);
	GET_ACTION_VNUM(ch, 0) = GET_CRAFT_VNUM(type);
	
	msg_to_char(ch, "You start to %s %s %s!\r\n", command, AN(GET_CRAFT_NAME(type)), GET_CRAFT_NAME(type));
	sprintf(buf, "$n begins to %s %s %s!", command, AN(GET_CRAFT_NAME(type)), GET_CRAFT_NAME(type));
	act(buf, FALSE, ch, NULL, NULL, TO_ROOM);
	
	// do a build action now
	process_build(ch, IN_ROOM(ch), ACT_BUILDING);
	request_world_save(GET_ROOM_VNUM(IN_ROOM(ch)), WSAVE_ROOM);
}


/**
* Sub-processor for crafting a vehicle.
*
* @param char_data *ch The player trying to craft the vehicle.
* @param craft_data *type Pre-selected vehicle craft.
* @param int dir Optional: If the player specified a direction (may by NO_DIR or an invalid dir for the craft).
*/
void do_gen_craft_vehicle(char_data *ch, craft_data *type, int dir) {
	vehicle_data *veh, *to_craft = NULL, *found_other = NULL;
	char buf[MAX_STRING_LENGTH];
	obj_data *found_obj = NULL;
	bool junk;
	
	// basic sanitation
	if (!type || !CRAFT_IS_VEHICLE(type) || !(to_craft = vehicle_proto(GET_CRAFT_OBJECT(type)))) {
		log("SYSERR: do_gen_craft_vehicle called with invalid vehicle craft %d", type ? GET_CRAFT_VNUM(type) : NOTHING);
		msg_to_char(ch, "That is not yet implemented.\r\n");
		return;
	}
	
	// found one to resume
	if ((veh = find_finishable_vehicle(ch, type, NOTHING, &found_other))) {
		resume_craft_vehicle(ch, veh, type);
		return;
	}
	if (found_other) {
		msg_to_char(ch, "You can't %s that while %s is unfinished here.\r\n", gen_craft_data[GET_CRAFT_TYPE(type)].command, get_vehicle_short_desc(found_other, ch));
		return;
	}
	if (!check_build_location_and_dir(ch, IN_ROOM(ch), type, dir, FALSE, NULL, NULL)) {
		return;	// sends own messages
	}
	
	// ok: new vehicle craft setup
	veh = read_vehicle(GET_CRAFT_OBJECT(type), TRUE);
	set_vehicle_flags(veh, VEH_INCOMPLETE);	// set incomplete before putting in the room
	vehicle_to_room(veh, IN_ROOM(ch));
	special_vehicle_setup(ch, veh);
	
	// additional setup
	VEH_NEEDS_RESOURCES(veh) = copy_resource_list(GET_CRAFT_RESOURCES(type));
	if (!VEH_FLAGGED(veh, VEH_NO_CLAIM)) {
		if (VEH_CLAIMS_WITH_ROOM(veh)) {
			// try to claim the room if unclaimed: can_claim checks total available land, but the outside is check done within this block
			if (!ROOM_OWNER(HOME_ROOM(IN_ROOM(ch))) && can_claim(ch) && !ROOM_AFF_FLAGGED(HOME_ROOM(IN_ROOM(ch)), ROOM_AFF_UNCLAIMABLE)) {
				empire_data *emp = get_or_create_empire(ch);
				if (emp) {
					int ter_type = get_territory_type_for_empire(HOME_ROOM(IN_ROOM(ch)), emp, FALSE, &junk, NULL);
					if (EMPIRE_TERRITORY(emp, ter_type) < land_can_claim(emp, ter_type)) {
						claim_room(HOME_ROOM(IN_ROOM(ch)), emp);
					}
				}
			}
			
			// and set the owner to the room owner
			perform_claim_vehicle(veh, ROOM_OWNER(HOME_ROOM(IN_ROOM(ch))));
		}
		else {
			perform_claim_vehicle(veh, GET_LOYALTY(ch));
		}
	}
	VEH_HEALTH(veh) = MAX(1, VEH_MAX_HEALTH(veh) * 0.2);	// start at 20% health, will heal on completion
	scale_vehicle_to_level(veh, get_craft_scale_level(ch, type));
	VEH_CONSTRUCTION_ID(veh) = get_new_vehicle_construction_id();
	set_vehicle_extra_data(veh, ROOM_EXTRA_BUILD_RECIPE, GET_CRAFT_VNUM(type));
	
	if (!IS_NPC(ch)) {
		set_room_extra_data(IN_ROOM(ch), ROOM_EXTRA_ORIGINAL_BUILDER, GET_ACCOUNT(ch)->id);
	}
	
	start_action(ch, ACT_GEN_CRAFT, -1);
	GET_ACTION_VNUM(ch, 0) = GET_CRAFT_VNUM(type);
	GET_ACTION_VNUM(ch, 1) = VEH_CONSTRUCTION_ID(veh);
	
	if (GET_CRAFT_REQUIRES_OBJ(type) != NOTHING) {
		find_and_bind(ch, GET_CRAFT_REQUIRES_OBJ(type));
		
		if ((found_obj = has_required_obj_for_craft(ch, GET_CRAFT_REQUIRES_OBJ(type))) && CRAFT_FLAGGED(type, CRAFT_TAKE_REQUIRED_OBJ)) {
			act("You use $p.", FALSE, ch, found_obj, NULL, TO_CHAR);
			extract_obj(found_obj);
		}
	}
	
	snprintf(buf, sizeof(buf), "You begin %s $V.", gen_craft_data[GET_CRAFT_TYPE(type)].verb);
	act(buf, FALSE, ch, NULL, veh, TO_CHAR | ACT_VEH_VICT);
	snprintf(buf, sizeof(buf), "$n begins %s $V.", gen_craft_data[GET_CRAFT_TYPE(type)].verb);
	act(buf, FALSE, ch, NULL, veh, TO_ROOM | ACT_VEH_VICT);
	
	process_gen_craft_vehicle(ch, type);
	request_vehicle_save_in_world(veh);
}


// subcmd must be CRAFT_TYPE_
ACMD(do_gen_craft) {
	char temp_arg[MAX_INPUT_LENGTH], short_arg[MAX_INPUT_LENGTH], last_arg[MAX_INPUT_LENGTH], buf[MAX_STRING_LENGTH * 2], line[256];
	int count, timer, num = 1, dir = NO_DIR, wrong_cmd = NOTHING;
	craft_data *craft, *next_craft, *type = NULL, *find_type = NULL, *abbrev_match = NULL, *abbrev_no_res = NULL, *multi_match = NULL, *multi_no_res = NULL;
	vehicle_data *veh;
	bool is_master, use_room, list_only = FALSE;
	obj_data *found_obj = NULL, *drinkcon = NULL;
	any_vnum missing_abil = NO_ABIL;
	ability_data *cft_abil;
	size_t size, lsize;
	
	if (IS_NPC(ch)) {
		msg_to_char(ch, "NPCs can't craft.\r\n");
		return;
	}
	if (!IS_APPROVED(ch)) {
		if (subcmd == CRAFT_TYPE_BUILD && config_get_bool("build_approval")) {
			send_config_msg(ch, "need_approval_string");
			return;
		}
		else if (subcmd != CRAFT_TYPE_BUILD && config_get_bool("craft_approval")) {
			send_config_msg(ch, "need_approval_string");
			return;
		}
	}
	
	skip_spaces(&argument);
	
	// optional leading info request
	if (!strn_cmp(argument, "info ", 5)) {
		argument = any_one_arg(argument, arg);
		quoted_arg_or_all(argument, temp_arg);
		show_craft_info(ch, temp_arg, subcmd);
		return;
	}
	
	// optional 'list' arg to search craftables
	if (!strn_cmp(argument, "list ", 5)) {
		argument = any_one_arg(argument, arg);
		list_only = TRUE;
		// keep going
	}
	
	// process what's left of argument to allow "quotes"
	skip_spaces(&argument);
	quoted_arg_or_all(argument, temp_arg);
	argument = temp_arg;
	
	// all other functions require standing
	if (*argument && !list_only && GET_POS(ch) < POS_STANDING) {
		send_low_pos_msg(ch);
		return;
	}
	
	// optional leading number
	if (isdigit(*argument) > 0) {
		half_chop(argument, buf, arg);
		if (is_number(buf)) {
			// really a number
			num = MAX(1, atoi(buf));
		}
		else {	// not a number -- restore whole argument
			num = 1;
			strcpy(arg, argument);
		}
	}
	else {	// no leading number
		num = 1;
		strcpy(arg, argument);
	}
	
	use_room = can_use_room(ch, IN_ROOM(ch), GUESTS_ALLOWED);
	
	// if there was an arg, find a matching craft_table entry (type)
	if (*arg && !list_only) {
		// attempt to split out a direction in case the craft makes a building
		chop_last_arg(arg, short_arg, last_arg);
		if (*last_arg && (dir = parse_direction(ch, last_arg)) == NO_DIR) {
			// wasn't a direction: don't use short/last arg
			*short_arg = *last_arg = '\0';
		}
		
		HASH_ITER(sorted_hh, sorted_crafts, craft, next_craft) {
			if (CRAFT_FLAGGED(craft, CRAFT_UPGRADE | CRAFT_DISMANTLE_ONLY)) {
				continue;	// never allow these flags
			}
			if (IS_SET(GET_CRAFT_FLAGS(craft), CRAFT_IN_DEVELOPMENT) && !IS_IMMORTAL(ch)) {
				continue;	// in-dev
			}
			if (GET_CRAFT_REQUIRES_OBJ(craft) != NOTHING && !has_required_obj_for_craft(ch, GET_CRAFT_REQUIRES_OBJ(craft))) {
				continue;	// missing requiresobj
			}
			if (IS_SET(GET_CRAFT_FLAGS(craft), CRAFT_LEARNED) && !has_learned_craft(ch, GET_CRAFT_VNUM(craft))) {
				continue;	// not learned
			}
			
			// match so far...
			if (!str_cmp(arg, GET_CRAFT_NAME(craft)) || (*short_arg && !str_cmp(short_arg, GET_CRAFT_NAME(craft)))) {
				// do this last because it records if they are on the wrong command or just missing an ability
				if (GET_CRAFT_TYPE(craft) != subcmd) {
					wrong_cmd = GET_CRAFT_TYPE(craft);
					continue;
				}
				if (GET_CRAFT_ABILITY(craft) != NO_ABIL && !has_ability(ch, GET_CRAFT_ABILITY(craft))) {
					missing_abil = GET_CRAFT_ABILITY(craft);
					continue;	// missing ability
				}
				
				// exact match! (don't care about resources)
				type = craft;
				break;
			}
			else if (!abbrev_match && (is_abbrev(arg, GET_CRAFT_NAME(craft)) || (*short_arg && is_abbrev(short_arg, GET_CRAFT_NAME(craft))))) {
				// do this last because it records if they are on the wrong command or just missing an ability
				if (GET_CRAFT_TYPE(craft) != subcmd) {
					wrong_cmd = GET_CRAFT_TYPE(craft);
					continue;
				}
				if (GET_CRAFT_ABILITY(craft) != NO_ABIL && !has_ability(ch, GET_CRAFT_ABILITY(craft))) {
					missing_abil = GET_CRAFT_ABILITY(craft);
					continue;	// missing ability
				}
				
				// found! maybe
				if (has_resources(ch, GET_CRAFT_RESOURCES(craft), use_room, FALSE, NULL)) {
					abbrev_match = craft;
				}
				else if (!abbrev_no_res) {
					abbrev_no_res = craft;
				}
			}
			else if (!multi_match && (multi_isname(arg, GET_CRAFT_NAME(craft)) || (*short_arg && multi_isname(short_arg, GET_CRAFT_NAME(craft))))) {
				// do this last because it records if they are on the wrong command or just missing an ability
				if (GET_CRAFT_TYPE(craft) != subcmd) {
					wrong_cmd = GET_CRAFT_TYPE(craft);
					continue;
				}
				if (GET_CRAFT_ABILITY(craft) != NO_ABIL && !has_ability(ch, GET_CRAFT_ABILITY(craft))) {
					missing_abil = GET_CRAFT_ABILITY(craft);
					continue;	// missing ability
				}
				
				// found! maybe
				if (has_resources(ch, GET_CRAFT_RESOURCES(craft), use_room, FALSE, NULL)) {
					multi_match = craft;
				}
				else if (!multi_no_res) {
					multi_no_res = craft;
				}
			}
		}
		
		// maybe we didn't find an exact match, but did find an abbrev/multi match
		// this also tries to prefer things they have the resources for
		if (!type) {
			type = abbrev_match ? abbrev_match : abbrev_no_res;	// if any
		}
		if (!type) {
			type = multi_match ? multi_match : multi_no_res;	// again, if any
		}
	}	// end arg-processing
	
	// preliminary checks
	if (subcmd == CRAFT_TYPE_ERROR) {
		msg_to_char(ch, "This command is not yet implemented.\r\n");
	}
	else if (*arg && !list_only && (veh = find_vehicle_to_resume_by_name(ch, subcmd, arg, &find_type))) {
		// attempting to resume a vehicle by name
		resume_craft_vehicle(ch, veh, find_type);
	}
	else if (*arg && !list_only && IS_INCOMPLETE(IN_ROOM(ch)) && type && CRAFT_IS_BUILDING(type) && GET_BUILDING(IN_ROOM(ch)) && GET_CRAFT_BUILD_TYPE(type) == GET_BLD_VNUM(GET_BUILDING(IN_ROOM(ch)))) {
		// attempting to resume a building by name
		resume_craft_building(ch, type);
	}
	else if (*arg && !list_only && !type && wrong_cmd != NOTHING) {
		// found no match but there was an almost-match with a different command (forge, sew, etc)
		msg_to_char(ch, "Unknown %s. Try '%s' instead.\r\n", gen_craft_data[subcmd].command, gen_craft_data[wrong_cmd].command);
	}
	else if (*arg && !list_only && !type && missing_abil != NO_ABIL) {
		// found no match but there was an almost-match with an ability requirement
		msg_to_char(ch, "You need the %s ability to %s that.\r\n", get_ability_name_by_vnum(missing_abil), gen_craft_data[subcmd].command);
	}
	else if (*arg && !list_only && !type) {
		// found no match
		msg_to_char(ch, "Unknown %s. Type '%s' by itself to see a list of what you can %s.\r\n", gen_craft_data[subcmd].command, gen_craft_data[subcmd].command, gen_craft_data[subcmd].command);
	}
	else if (!*arg && !list_only && subcmd == CRAFT_TYPE_BUILD && IS_INCOMPLETE(IN_ROOM(ch))) {
		// 'build' no-arg in an incomplete building: resume
		if (IS_BURNING(IN_ROOM(ch))) {
			msg_to_char(ch, "You can't work on a burning building!\r\n");
		}
		else if (!(find_type = find_building_list_entry(IN_ROOM(ch), FIND_BUILD_NORMAL)) && !(find_type = find_building_list_entry(IN_ROOM(ch), FIND_BUILD_UPGRADE))) {
			msg_to_char(ch, "It doesn't seem possible to continue the construction.\r\n");
		}
		else if (!CRAFT_FLAGGED(find_type, CRAFT_DARK_OK) && !can_see_in_dark_room(ch, IN_ROOM(ch), TRUE)) {
			msg_to_char(ch, "It's too dark to work on the building.\r\n");
		}
		else {
			resume_craft_building(ch, find_type);
		}
	}
	else if (!*arg || list_only) {	// main no-arg (or list-only): master craft list
		// master craft list
		size = snprintf(buf, sizeof(buf), "You know how to %s:\r\n", gen_craft_data[subcmd].command);
		count = 0;
		
		HASH_ITER(sorted_hh, sorted_crafts, craft, next_craft) {
			if (GET_CRAFT_TYPE(craft) != subcmd) {
				continue;	// wrong craft type
			}
			if (IS_SET(GET_CRAFT_FLAGS(craft), CRAFT_IN_DEVELOPMENT) && !IS_IMMORTAL(ch)) {
				continue;	// in-dev
			}
			if (CRAFT_FLAGGED(craft, CRAFT_UPGRADE | CRAFT_DISMANTLE_ONLY)) {
				continue;	// never allow these flags
			}
			if (GET_CRAFT_ABILITY(craft) != NO_ABIL && !has_ability(ch, GET_CRAFT_ABILITY(craft))) {
				continue;	// no abil
			}
			if (GET_CRAFT_REQUIRES_OBJ(craft) != NOTHING && !has_required_obj_for_craft(ch, GET_CRAFT_REQUIRES_OBJ(craft))) {
				continue;	// missing obj
			}
			if (IS_SET(GET_CRAFT_FLAGS(craft), CRAFT_LEARNED) && !has_learned_craft(ch, GET_CRAFT_VNUM(craft))) {
				continue;	// not learned
			}
			if (*arg && !multi_isname(arg, GET_CRAFT_NAME(craft)) && (!*short_arg || !multi_isname(short_arg, GET_CRAFT_NAME(craft)))) {
				continue;	// search exclusion
			}
			
			// valid:
			++count;
			if (PRF_FLAGGED(ch, PRF_ROOMFLAGS)) {
				lsize = snprintf(line, sizeof(line), "[%5d] %s", GET_CRAFT_VNUM(craft), GET_CRAFT_NAME(craft));
			}
			else {
				lsize = snprintf(line, sizeof(line), "%s", GET_CRAFT_NAME(craft));
			}
			
			// screenreader and non-screenreader add it differently
			if (PRF_FLAGGED(ch, PRF_SCREEN_READER)) {
				if (size + lsize + 2 < sizeof(buf)) {
					strcat(buf, line);
					strcat(buf, "\r\n");
					size += lsize + 2;
				}
				else {
					snprintf(buf + size, sizeof(buf) - size, "OVERFLOW\r\n");
					break;
				}
			}
			else {	// non-screenreader
				if ((count % 2) && lsize <= 38 && (size + MAX(40, lsize)) < sizeof(buf)) {
					// left column
					size += snprintf(buf + size, sizeof(buf) - size, "%-38.38s ", line);
				}
				else if (size + lsize + 2 < sizeof(buf)) {
					// right column or wide left column
					strcat(buf, line);
					strcat(buf, "\r\n");
					size += lsize + 2;
				}
				else {
					snprintf(buf + size, sizeof(buf) - size, "OVERFLOW\r\n");
					break;
				}
			}
		}	// end loop
		if (!count) {
			strcat(buf, " nothing\r\n");	// always room here
		}
		else if (!PRF_FLAGGED(ch, PRF_SCREEN_READER) && (count % 2)) {
			strcat(buf, "\r\n");
		}
		
		// and show it
		if (ch->desc) {
			page_string(ch->desc, buf, TRUE);
		}
	}
	else if (GET_ACTION(ch) != ACT_NONE) {
		msg_to_char(ch, "You're busy right now.\r\n");
	}
	else if (GET_CRAFT_ABILITY(type) != NO_ABIL && !has_ability(ch, GET_CRAFT_ABILITY(type))) {
		// this should be unreachable
		msg_to_char(ch, "You need to buy the %s ability to %s that.\r\n", get_ability_name_by_vnum(GET_CRAFT_ABILITY(type)), gen_craft_data[GET_CRAFT_TYPE(type)].command);
	}
	else if (GET_CRAFT_REQUIRES_OBJ(type) != NOTHING && !(found_obj = has_required_obj_for_craft(ch, GET_CRAFT_REQUIRES_OBJ(type)))) {
		// this block is unreachable but the found_obj is set in the if()
		msg_to_char(ch, "You need %s to %s that.\r\n", get_obj_name_by_proto(GET_CRAFT_REQUIRES_OBJ(type)), gen_craft_data[GET_CRAFT_TYPE(type)].command);
	}
	else if (IS_SET(GET_CRAFT_FLAGS(type), CRAFT_LEARNED) && !has_learned_craft(ch, GET_CRAFT_VNUM(type))) {
		// this should be unreachable
		msg_to_char(ch, "You have not learned that recipe.\r\n");
	}
	else if (!check_can_craft(ch, type, FALSE)) {
		// sends its own messages
	}
	else if (CRAFT_IS_VEHICLE(type)) {
		// vehicles pass off at this point
		do_gen_craft_vehicle(ch, type, dir);
	}
	else if (CRAFT_IS_BUILDING(type)) {
		// pass off buildings
		do_gen_craft_building(ch, type, dir);
	}
	
	// regular object-type craft (or soup)
	else if (IS_CARRYING_N(ch) > CAN_CARRY_N(ch)) {
		msg_to_char(ch, "You can't %s anything while overburdened.\r\n", gen_craft_data[subcmd].command);
	}
	else if (!has_resources(ch, GET_CRAFT_RESOURCES(type), use_room, TRUE, GET_CRAFT_NAME(type))) {
		// this sends its own message ("You need X more of ...")
		//msg_to_char(ch, "You don't have the resources to %s that.\r\n", gen_craft_data[GET_CRAFT_TYPE(type)].command);
	}
	else if (GET_CRAFT_REQUIRES_OBJ(type) != NOTHING && found_obj && !consume_otrigger(found_obj, ch, OCMD_CRAFT, NULL)) {
		return;	// trigger hopefully sent its own message
	}
	else {
		cft_abil = find_ability_by_vnum(GET_CRAFT_ABILITY(type));
		is_master = (cft_abil && ABIL_MASTERY_ABIL(cft_abil) != NOTHING && has_ability(ch, ABIL_MASTERY_ABIL(cft_abil)));
		
		// base timer
		timer = GET_CRAFT_TIME(type);
		
		// potter building bonus	
		if (IS_SET(GET_CRAFT_FLAGS(type), CRAFT_POTTERY) && room_has_function_and_city_ok(GET_LOYALTY(ch), IN_ROOM(ch), FNC_POTTER) && IS_COMPLETE(IN_ROOM(ch))) {
			timer /= 4;
		}
		
		// mastery
		if (is_master) {
			timer /= 2;
		}
	
		start_action(ch, ACT_GEN_CRAFT, timer);
		GET_ACTION_VNUM(ch, 0) = GET_CRAFT_VNUM(type);
		
		if (IS_SET(GET_CRAFT_FLAGS(type), CRAFT_SOUP) && ((drinkcon = find_water_container(ch, ch->carrying)) || (drinkcon = find_water_container(ch, ROOM_CONTENTS(IN_ROOM(ch)))))) {
			GET_ACTION_VNUM(ch, 1) = GET_OBJ_VNUM(drinkcon);
			extract_obj(drinkcon);
		}
		
		// how many
		GET_ACTION_VNUM(ch, 2) = num;
		
		// do this BEFORE extract, as it may be extracted
		if (GET_CRAFT_REQUIRES_OBJ(type) != NOTHING) {
			find_and_bind(ch, GET_CRAFT_REQUIRES_OBJ(type));
		
			if (found_obj && CRAFT_FLAGGED(type, CRAFT_TAKE_REQUIRED_OBJ)) {
				act("You use $p.", FALSE, ch, found_obj, NULL, TO_CHAR);
				extract_obj(found_obj);
			}
		}
		
		// must call this after start_action() because it stores resources
		extract_resources(ch, GET_CRAFT_RESOURCES(type), use_room, &GET_ACTION_RESOURCES(ch));
		
		if (GET_CRAFT_NAME(type)[strlen(GET_CRAFT_NAME(type))-1] == 's') {
			msg_to_char(ch, "You start %s %s.\r\n", gen_craft_data[GET_CRAFT_TYPE(type)].verb, GET_CRAFT_NAME(type));
		}
		else {
			msg_to_char(ch, "You start %s %s %s.\r\n", gen_craft_data[GET_CRAFT_TYPE(type)].verb, AN(GET_CRAFT_NAME(type)), GET_CRAFT_NAME(type));
		}
		sprintf(buf, "$n starts %s.", gen_craft_data[GET_CRAFT_TYPE(type)].verb);
		act(buf, FALSE, ch, NULL, NULL, TO_ROOM);
	}
}


ACMD(do_learn) {
	char arg[MAX_INPUT_LENGTH];
	craft_data *recipe;
	obj_data *obj;
	
	one_argument(argument, arg);
	
	if (IS_NPC(ch)) {
		msg_to_char(ch, "Mobs never learn.\r\n");
	}
	else if (!*argument) {
		msg_to_char(ch, "Learn what?\r\n");
	}
	else if (IS_IMMORTAL(ch) && is_number(arg)) {
		// immortal learn: learn <vnum>
		if (!(recipe = craft_proto(atoi(arg)))) {
			msg_to_char(ch, "Invalid craft vnum '%s'.\r\n", arg);
		}
		else if (!CRAFT_FLAGGED(recipe, CRAFT_LEARNED)) {
			msg_to_char(ch, "That is not a LEARNED-flagged craft.\r\n");
		}
		else {
			add_learned_craft(ch, GET_CRAFT_VNUM(recipe));
			msg_to_char(ch, "You have learned [%d] %s (%s).\r\n", GET_CRAFT_VNUM(recipe), GET_CRAFT_NAME(recipe), craft_types[GET_CRAFT_TYPE(recipe)]);
		}
	}
	else if (!(obj = get_obj_in_list_vis(ch, arg, NULL, ch->carrying))) {
		msg_to_char(ch, "You don't seem to have %s %s.\r\n", AN(arg), arg);
	}
	else if (!bind_ok(obj, ch)) {
		msg_to_char(ch, "That recipe is bound to someone else.\r\n");
	}
	else if (!IS_RECIPE(obj)) {
		msg_to_char(ch, "You can only learn a recipe object.\r\n");
	}
	else if (GET_RECIPE_VNUM(obj) <= 0 || !(recipe = craft_proto(GET_RECIPE_VNUM(obj)))) {
		msg_to_char(ch, "You can't learn that!\r\n");
	}
	else if (has_learned_craft(ch, GET_RECIPE_VNUM(obj))) {
		msg_to_char(ch, "You already know that recipe.\r\n");
	}
	
	// validate the player's ability to MAKE the recipe
	else if (!IS_IMMORTAL(ch) && IS_SET(GET_CRAFT_FLAGS(recipe), CRAFT_IN_DEVELOPMENT)) {
		msg_to_char(ch, "That recipe is not currently available to learn.\r\n");
	}
	else if (GET_CRAFT_ABILITY(recipe) != NO_ABIL && !has_ability(ch, GET_CRAFT_ABILITY(recipe))) {
		msg_to_char(ch, "You require the %s ability to learn that recipe.\r\n", get_ability_name_by_vnum(GET_CRAFT_ABILITY(recipe)));
	}
	
	// seems ok!
	else {
		add_learned_craft(ch, GET_RECIPE_VNUM(obj));
		snprintf(buf, sizeof(buf), "You commit $p to memory (%s command).", gen_craft_data[GET_CRAFT_TYPE(recipe)].command);
		act(buf, FALSE, ch, obj, NULL, TO_CHAR);
		act("$n commits $p to memory.", TRUE, ch, obj, NULL, TO_ROOM);
		extract_obj(obj);
	}
}





ACMD(do_learned) {
	char output[MAX_STRING_LENGTH * 2], line[MAX_STRING_LENGTH], temp[256];
	struct player_craft_data *pcd, *next_pcd, *lists[2];
	int l_pos, width, last_type;
	bool is_emp, overflow, comma;
	craft_data *craft;
	size_t size, l_size, count;
	
	if (IS_NPC(ch)) {
		msg_to_char(ch, "Mobs never learn.\r\n");
		return;
	}
	if (!ch->desc) {
		return;	// no point, nothing to show
	}
	
	skip_spaces(&argument);
	if (*argument) {
		size = snprintf(output, sizeof(output), "Learned recipes matching '%s':\r\n", argument);
	}
	else {
		size = snprintf(output, sizeof(output), "Learned recipes:\r\n");
	}
	
	// detect width for how wide the lists can go
	width = (ch->desc && ch->desc->pProtocol->ScreenWidth > 0) ? ch->desc->pProtocol->ScreenWidth : 80;
	width = MIN(width, sizeof(line) - 2);
	
	// search 2 lists
	overflow = FALSE;
	count = 0;
	lists[0] = GET_LEARNED_CRAFTS(ch);
	lists[1] = GET_LOYALTY(ch) ? EMPIRE_LEARNED_CRAFTS(GET_LOYALTY(ch)) : NULL;
	
	for (l_pos = 0, is_emp = FALSE; l_pos < 2 && !overflow; ++l_pos, is_emp = TRUE) {
		last_type = -1;	// reset each loop
		*line = '\0';
		comma = FALSE;
		
		HASH_ITER(hh, lists[l_pos], pcd, next_pcd) {
			if (!(craft = craft_proto(pcd->vnum))) {
				continue;	// no craft?
			}
			if (CRAFT_FLAGGED(craft, CRAFT_IN_DEVELOPMENT) && !IS_IMMORTAL(ch)) {
				continue;	// in-dev
			}
			if (*argument && !multi_isname(argument, GET_CRAFT_NAME(craft)) && str_cmp(craft_types[GET_CRAFT_TYPE(craft)], argument)) {
				continue;	// searched
			}
			
			// ok:
			++count;
			
			// check start of line
			if (last_type == -1 || last_type != GET_CRAFT_TYPE(craft)) {
				// append line now
				if (*line) {
					if (size + strlen(line) + 12 < sizeof(output)) {
						strcat(output, line);
						strcat(output, "\r\n");
						size += strlen(line) + 2;
					}
					else {
						overflow = TRUE;
						strcat(output, "OVERFLOW\r\n");	// 10 characters always reserved
					}
				}
				
				// prepare new line
				last_type = GET_CRAFT_TYPE(craft);
				strcpy(temp, craft_types[last_type]);
				strtolower(temp);
				ucwords(temp);
				l_size = snprintf(line, sizeof(line), "%s%s:", temp, is_emp ? " (empire)" : "");
				comma = FALSE;
			}
			
			// check line limit
			if (l_size + strlen(GET_CRAFT_NAME(craft)) + 3 > width) {
				if (size + strlen(line) + 13 < sizeof(output)) {
					strcat(output, line);
					strcat(output, ",\r\n");
					size += strlen(line) + 3;
				}
				else {
					overflow = TRUE;
					strcat(output, "OVERFLOW\r\n");	// 10 characters always reserved
					break;
				}
				l_size = snprintf(line, sizeof(line), "   %s", GET_CRAFT_NAME(craft));
				comma = TRUE;
			}
			else {	// room on this line
				l_size += snprintf(line + l_size, sizeof(line) - l_size, "%s%s", comma ? ", " : " ", GET_CRAFT_NAME(craft));
				comma = TRUE;
			}
		}
		
		// check for trailing text
		if (*line) {
			if (size + strlen(line) + 12 < sizeof(output)) {
				strcat(output, line);
				strcat(output, "\r\n");
				size += strlen(line) + 2;
			}
			else {
				overflow = TRUE;
				strcat(output, "OVERFLOW\r\n");	// 10 characters always reserved
			}
		}
	}
	
	if (!count) {
		strcat(output, "  none\r\n");	// space reserved for this for sure
	}
	
	if (ch->desc) {
		page_string(ch->desc, output, TRUE);
	}
}


ACMD(do_recipes) {
	int last_type = NOTHING;
	craft_data *craft, *next_craft;
	augment_data *aug, *next_aug;
	struct resource_data *res;
	obj_data *obj;
	bool found_any, found_type, uses_item;
	
	one_argument(argument, arg);
	
	if (!*arg) {
		msg_to_char(ch, "Show recipes for which item?\r\n");
	}
	else if (!(obj = get_obj_in_list_vis(ch, arg, NULL, ch->carrying))) {
		msg_to_char(ch, "You don't seem to have %s %s in your inventory.\r\n", AN(arg), arg);
	}
	else {
		act("With $p, you can make:", FALSE, ch, obj, NULL, TO_CHAR);
		found_any = FALSE;
		
		found_type = FALSE;
		HASH_ITER(sorted_hh, sorted_crafts, craft, next_craft) {
			// is it a live recipe?
			if (IS_SET(GET_CRAFT_FLAGS(craft), CRAFT_IN_DEVELOPMENT) && !IS_IMMORTAL(ch)) {
				continue;
			}
			if (GET_CRAFT_TYPE(craft) == CRAFT_TYPE_WORKFORCE) {
				continue;	// don't show workforce
			}
		
			// has right abil?
			if (GET_CRAFT_ABILITY(craft) != NO_ABIL && !has_ability(ch, GET_CRAFT_ABILITY(craft))) {
				continue;
			}
			
			if (GET_CRAFT_REQUIRES_OBJ(craft) != NOTHING && !has_required_obj_for_craft(ch, GET_CRAFT_REQUIRES_OBJ(craft))) {
				continue;
			}
			if (IS_SET(GET_CRAFT_FLAGS(craft), CRAFT_LEARNED) && !has_learned_craft(ch, GET_CRAFT_VNUM(craft))) {
				continue;	// not learned
			}
			
			// is the item used to make it?
			uses_item = FALSE;
			if (GET_OBJ_VNUM(obj) != NOTHING && GET_CRAFT_REQUIRES_OBJ(craft) == GET_OBJ_VNUM(obj)) {
				uses_item = TRUE;
			}
			for (res = GET_CRAFT_RESOURCES(craft); !uses_item && res; res = res->next) {
				if (res->type == RES_OBJECT && GET_OBJ_VNUM(obj) != NOTHING && res->vnum == GET_OBJ_VNUM(obj)) {
					uses_item = TRUE;
				}
				else if (res->type == RES_COMPONENT && is_component_vnum(obj, res->vnum)) {
					uses_item = TRUE;
				}
			}
				
			// MATCH!
			if (uses_item) {
				// need header?
				if (GET_CRAFT_TYPE(craft) != last_type) {
					msg_to_char(ch, "%s %s: ", found_type ? "\r\n" : "", gen_craft_data[GET_CRAFT_TYPE(craft)].command);
	
					found_type = FALSE;
					last_type = GET_CRAFT_TYPE(craft);
				}
		
				msg_to_char(ch, "%s%s", (found_type ? ", " : ""), GET_CRAFT_NAME(craft));
				found_any = found_type = TRUE;
			}
		}
		// trailing crlf
		if (found_type) {
			msg_to_char(ch, "\r\n");
		}
		
		found_type = FALSE;
		HASH_ITER(sorted_hh, sorted_augments, aug, next_aug) {
			if (AUGMENT_FLAGGED(aug, AUG_IN_DEVELOPMENT) && !IS_IMMORTAL(ch)) {
				continue;
			}
			if (GET_AUG_ABILITY(aug) != NO_ABIL && !has_ability(ch, GET_AUG_ABILITY(aug))) {
				continue;
			}
			
			if (GET_AUG_REQUIRES_OBJ(aug) != NOTHING && !get_obj_in_list_vnum(GET_AUG_REQUIRES_OBJ(aug), ch->carrying)) {
				continue;
			}
			
			// is the item used to make it?
			uses_item = FALSE;
			if (GET_OBJ_VNUM(obj) != NOTHING && GET_AUG_REQUIRES_OBJ(aug) == GET_OBJ_VNUM(obj)) {
				uses_item = TRUE;
			}
			for (res = GET_AUG_RESOURCES(aug); !uses_item && res; res = res->next) {
				if (GET_OBJ_VNUM(obj) != NOTHING && res->type == RES_OBJECT && res->vnum == GET_OBJ_VNUM(obj)) {
					uses_item = TRUE;
				}
				else if (res->type == RES_COMPONENT && is_component_vnum(obj, res->vnum)) {
					uses_item = TRUE;
				}
			}
				
			// MATCH!
			if (uses_item) {
				// need header?
				if (GET_AUG_TYPE(aug) != last_type) {
					msg_to_char(ch, "%s %s: ", found_type ? "\r\n" : "", augment_info[GET_AUG_TYPE(aug)].verb);
	
					found_type = FALSE;
					last_type = GET_AUG_TYPE(aug);
				}
		
				msg_to_char(ch, "%s%s", (found_type ? ", " : ""), GET_AUG_NAME(aug));
				found_any = found_type = TRUE;
			}
		}
		
		// trailing crlf
		if (found_type) {
			msg_to_char(ch, "\r\n");
		}
		
		if (!found_any) {
			msg_to_char(ch, "  nothing\r\n");
		}
	}
}


// do_refashion / this handles both 'reforge' and 'refashion'
ACMD(do_reforge) {
	char arg2[MAX_INPUT_LENGTH], temp[MAX_INPUT_LENGTH];
	struct resource_data *res = NULL;
	time_t old_stolen_time;
	ability_data *cft_abil;
	craft_data *ctype;
	int old_timer, iter, level, old_stolen_from;
	bool found;
	obj_data *obj, *new, *proto;
	
	bitvector_t preserve_flags = OBJ_PRESERVE_FLAGS;	// flags to copy over if obj is reloaded
	
	// reforge <item> name <name>
	// reforge <item> renew
	
	argument = two_arguments(argument, arg, arg2);
	skip_spaces(&argument);
	
	if (IS_NPC(ch)) {
		msg_to_char(ch, "NPCs cannot reforge.\r\n");
	}
	else if (reforge_data[subcmd].ability != NOTHING && !has_ability(ch, reforge_data[subcmd].ability)) {
		msg_to_char(ch, "You must buy the %s ability to do that.\r\n", reforge_data[subcmd].command);
	}
	else if (!*arg || !*arg2) {
		msg_to_char(ch, "Usage: %s <item> name <name>\r\n", reforge_data[subcmd].command);
		msg_to_char(ch, "       %s <item> <renew | superior>\r\n", reforge_data[subcmd].command);
	}
	else if (reforge_data[subcmd].validate_func && !(reforge_data[subcmd].validate_func)(ch)) {
		// failed validate func -- sends own messages
	}
	else if (!(obj = get_obj_in_list_vis(ch, arg, NULL, ch->carrying)) && !(obj = get_obj_by_char_share(ch, arg))) {
		msg_to_char(ch, "You don't seem to have %s %s.\r\n", AN(arg), arg);
	}
	else if (!match_reforge_type(obj, subcmd)) {
		msg_to_char(ch, "You can only %s ", reforge_data[subcmd].command);
		*buf = '\0';
		for (iter = 0, found = FALSE; reforge_data[subcmd].types[iter] != NOTHING; ++iter) {
			sprintf(buf + strlen(buf), "%s%ss", (found ? ", " : ""), item_types[reforge_data[subcmd].types[iter]]);
			found = TRUE;
		}
		// make buf lowercase -- item_types is all upper
		for (iter = 0; iter < strlen(buf); ++iter) {
			buf[iter] = LOWER(buf[iter]);
			if (buf[iter] == '_') {
				buf[iter] = ' ';	// convert underscore to space
			}
		}
		msg_to_char(ch, "%s.\r\n", buf);
	}
	else if (GET_OBJ_VNUM(obj) == NOTHING || !obj_proto(GET_OBJ_VNUM(obj))) {
		// since we compare to the prototype, this is not allowed
		msg_to_char(ch, "You can't %s that item.\r\n", reforge_data[subcmd].command);
	}
	else if (is_abbrev(arg2, "name")) {
		// calculate gem cost based on the gear rating of the item
		add_to_resource_list(&res, RES_OBJECT, o_IRIDESCENT_IRIS, MAX(1, rate_item(obj) / 3), 0);
		
		if (!validate_item_rename(ch, obj, argument)) {
			// sends own message
		}
		else if (!has_resources(ch, res, can_use_room(ch, IN_ROOM(ch), GUESTS_ALLOWED), TRUE, NULL)) {
			// sends own message
		}
		else {
			extract_resources(ch, res, can_use_room(ch, IN_ROOM(ch), GUESTS_ALLOWED), NULL);
			
			// prepare
			sprintf(buf1, "You name %s%s $p!", GET_OBJ_SHORT_DESC(obj), shared_by(obj, ch));
			sprintf(buf2, "$n names %s%s $p!", GET_OBJ_SHORT_DESC(obj), shared_by(obj, ch));
			
			proto = obj_proto(GET_OBJ_VNUM(obj));
			
			// rename keywords
			snprintf(temp, sizeof(temp), "%s %s", fname(GET_OBJ_KEYWORDS(proto)), skip_filler(argument));
			set_obj_keywords(obj, temp);
			
			// rename short desc
			set_obj_short_desc(obj, argument);
			
			// rename long desc
			sprintf(temp, "%s is lying here.", argument);
			CAP(temp);
			set_obj_long_desc(obj, temp);
			
			// message
			act(buf1, FALSE, ch, obj, obj->worn_by, TO_CHAR);
			act(buf2, TRUE, ch, obj, obj->worn_by, TO_ROOM);
			
			if (reforge_data[subcmd].ability != NOTHING) {
				gain_ability_exp(ch, reforge_data[subcmd].ability, 50);
				run_ability_hooks(ch, AHOOK_ABILITY, reforge_data[subcmd].ability, 0, NULL, obj, NULL, NULL, NOBITS);
			}
		}
	}
	else if (is_abbrev(arg2, "renew")) {
		proto = obj_proto(GET_OBJ_VNUM(obj));
		
		// calculate gem cost based on the gear rating of the original item
		add_to_resource_list(&res, RES_OBJECT, o_BLOODSTONE, MAX(1, (proto ? rate_item(proto) : rate_item(obj)) / 3), 0);
		
		if (!proto) {
			msg_to_char(ch, "You can't renew that.\r\n");
		}
		else if (!has_resources(ch, res, can_use_room(ch, IN_ROOM(ch), GUESTS_ALLOWED), TRUE, NULL)) {
			// sends own message
		}
		else {
			extract_resources(ch, res, can_use_room(ch, IN_ROOM(ch), GUESTS_ALLOWED), NULL);
			
			// actual reforging: just junk it and load a new one
			old_stolen_time = obj->stolen_timer;
			old_stolen_from = GET_STOLEN_FROM(obj);
			old_timer = GET_OBJ_TIMER(obj);
			level = GET_OBJ_CURRENT_SCALE_LEVEL(obj);
			
			new = read_object(GET_OBJ_VNUM(proto), TRUE);
			GET_OBJ_EXTRA(new) |= GET_OBJ_EXTRA(obj) & preserve_flags;
			
			// transfer bindings
			OBJ_BOUND_TO(new) = OBJ_BOUND_TO(obj);
			OBJ_BOUND_TO(obj) = NULL;
			
			// re-apply
			new->stolen_timer = old_stolen_time;
			GET_STOLEN_FROM(new) = old_stolen_from;
			GET_OBJ_TIMER(new) = old_timer;
			if (level > 0) {
				scale_item_to_level(new, level);
			}
			
			// junk the old one
			swap_obj_for_obj(obj, new);
			extract_obj(obj);
			obj = new;
			
			// message
			sprintf(buf, "You %s $p%s!", reforge_data[subcmd].command, shared_by(obj, ch));
			act(buf, FALSE, ch, obj, obj->worn_by, TO_CHAR);
			
			sprintf(buf, "$n %ss $p%s!", reforge_data[subcmd].command_3rd, shared_by(obj, ch));
			act(buf, TRUE, ch, obj, obj->worn_by, TO_ROOM);
			
			if (reforge_data[subcmd].ability != NO_ABIL) {
				gain_ability_exp(ch, reforge_data[subcmd].ability, 50);
				run_ability_hooks(ch, AHOOK_ABILITY, reforge_data[subcmd].ability, 0, NULL, obj, NULL, NULL, NOBITS);
			}
			
			// this seems like it should not be running a load trigger
			// load_otrigger(obj);
		}
	}
	else if (is_abbrev(arg2, "superior")) {
		// calculate gem cost based on the gear rating of the item
		add_to_resource_list(&res, RES_OBJECT, o_GLOWING_SEASHELL, MAX(1, rate_item(obj) / 3), 0);
		proto = obj_proto(GET_OBJ_VNUM(obj));
		
		if (OBJ_FLAGGED(obj, OBJ_SUPERIOR)) {
			msg_to_char(ch, "It is already superior.\r\n");
		}
		else if (!proto || !OBJ_FLAGGED(proto, OBJ_SCALABLE) || !(ctype = find_craft_for_obj_vnum(GET_OBJ_VNUM(obj)))) {
			msg_to_char(ch, "It can't be made superior.\r\n");
		}
		else if (!(cft_abil = find_ability_by_vnum(GET_CRAFT_ABILITY(ctype))) || ABIL_MASTERY_ABIL(cft_abil) == NOTHING || !has_ability(ch, ABIL_MASTERY_ABIL(cft_abil))) {
			msg_to_char(ch, "You don't have the mastery to make that item superior.\r\n");
		}
		else if (!has_resources(ch, res, can_use_room(ch, IN_ROOM(ch), GUESTS_ALLOWED), TRUE, NULL)) {
			// sends own message
		}
		else {
			extract_resources(ch, res, can_use_room(ch, IN_ROOM(ch), GUESTS_ALLOWED), NULL);
			
			// actual reforging: just junk it and load a new one
			old_stolen_time = obj->stolen_timer;
			old_stolen_from = GET_STOLEN_FROM(obj);
			old_timer = GET_OBJ_TIMER(obj);
			level = GET_OBJ_CURRENT_SCALE_LEVEL(obj);
			
			new = read_object(GET_OBJ_VNUM(proto), TRUE);
			GET_OBJ_EXTRA(new) |= GET_OBJ_EXTRA(obj) & preserve_flags;
			
			// ensure no hard/group
			REMOVE_BIT(GET_OBJ_EXTRA(new), OBJ_HARD_DROP);
			REMOVE_BIT(GET_OBJ_EXTRA(new), OBJ_GROUP_DROP);
			
			// transfer bindings
			OBJ_BOUND_TO(new) = OBJ_BOUND_TO(obj);
			OBJ_BOUND_TO(obj) = NULL;
			
			// set superior
			SET_BIT(GET_OBJ_EXTRA(new), OBJ_SUPERIOR);
			
			// copy over strings?
			if (GET_OBJ_KEYWORDS(obj) != GET_OBJ_KEYWORDS(proto)) {
				set_obj_keywords(new, GET_OBJ_KEYWORDS(obj));
			}
			if (GET_OBJ_SHORT_DESC(obj) != GET_OBJ_SHORT_DESC(proto)) {
				set_obj_short_desc(new, GET_OBJ_SHORT_DESC(obj));
			}
			if (GET_OBJ_LONG_DESC(obj) != GET_OBJ_LONG_DESC(proto)) {
				set_obj_long_desc(new, GET_OBJ_LONG_DESC(obj));
			}
			if (GET_OBJ_ACTION_DESC(obj) != GET_OBJ_ACTION_DESC(proto)) {
				set_obj_look_desc(new, GET_OBJ_ACTION_DESC(obj), FALSE);
			}
			
			// re-apply values
			new->stolen_timer = old_stolen_time;
			GET_STOLEN_FROM(new) = old_stolen_from;
			GET_OBJ_TIMER(new) = old_timer;
			if (level > 0) {
				scale_item_to_level(new, level);
			}
			
			// junk the old one
			swap_obj_for_obj(obj, new);
			extract_obj(obj);
			obj = new;
			
			sprintf(buf, "You %s $p%s into a masterwork!", reforge_data[subcmd].command, shared_by(obj, ch));
			act(buf, FALSE, ch, obj, obj->worn_by, TO_CHAR);
			
			sprintf(buf, "$n %s $p%s into a masterwork!", reforge_data[subcmd].command_3rd, shared_by(obj, ch));
			act(buf, TRUE, ch, obj, obj->worn_by, TO_ROOM);

			if (reforge_data[subcmd].ability != NOTHING) {
				gain_ability_exp(ch, reforge_data[subcmd].ability, 50);
				run_ability_hooks(ch, AHOOK_ABILITY, reforge_data[subcmd].ability, 0, NULL, obj, NULL, NULL, NOBITS);
			}
		}
	}
	else {
		msg_to_char(ch, "That's not a valid %s option.\r\n", reforge_data[subcmd].command);
	}
	
	// no leaks
	if (res) {
		free_resource_list(res);
	}
}


ACMD(do_tame) {
	char_data *mob;
	bool any;
	
	one_argument(argument, arg);

	if (!has_player_tech(ch, PTECH_TAME_ANIMALS)) {
		msg_to_char(ch, "You don't have the correct ability to tame animals.\r\n");
	}
	else if (get_cooldown_time(ch, COOLDOWN_TAME) > 0) {
		msg_to_char(ch, "You can't tame again yet.\r\n");
	}
	else if (!can_use_room(ch, IN_ROOM(ch), MEMBERS_ONLY)) {
		msg_to_char(ch, "You don't have permission to tame animals here.\r\n");
	}
	else if (!*arg) {
		msg_to_char(ch, "Which animal would you like to tame?\r\n");
	}
	else if (!(mob = get_char_vis(ch, arg, NULL, FIND_CHAR_ROOM))) {
		send_config_msg(ch, "no_person");
	}
	else if (!IS_NPC(mob) || !has_interaction(mob->interactions, INTERACT_TAME)) {
		act("You can't tame $N!", FALSE, ch, 0, mob, TO_CHAR);
	}
	else if (GET_LED_BY(mob)) {
		act("You can't tame $M right now.", FALSE, ch, NULL, mob, TO_CHAR);
	}
	else if (run_ability_triggers_by_player_tech(ch, PTECH_TAME_ANIMALS, mob, NULL)) {
		// triggered
	}
	else {
		act("You try to tame $N...", FALSE, ch, NULL, mob, TO_CHAR);
		act("$n tries to tame you...", FALSE, ch, NULL, mob, TO_VICT);
		act("$n tries to tame $N...", FALSE, ch, NULL, mob, TO_NOTVICT);
		
		any = run_interactions(ch, mob->interactions, INTERACT_TAME, IN_ROOM(ch), mob, NULL, NULL, tame_interact);
		
		if (any) {
			gain_player_tech_exp(ch, PTECH_TAME_ANIMALS, 50);
			add_cooldown(ch, COOLDOWN_TAME, 3 * SECS_PER_REAL_MIN);
			run_ability_hooks_by_player_tech(ch, PTECH_TAME_ANIMALS, NULL, NULL, NULL, NULL);
			
			// remove the original
			extract_char(mob);
		}
		else {
			act("You can't seem to tame $M!", FALSE, ch, NULL, mob, TO_CHAR);
		}
		
		command_lag(ch, WAIT_OTHER);
	}
}
