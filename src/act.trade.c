/* ************************************************************************
*   File: act.trade.c                                     EmpireMUD 2.0b5 *
*  Usage: code related to crafting and the trade skill                    *
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
#include "comm.h"
#include "interpreter.h"
#include "handler.h"
#include "db.h"
#include "skills.h"
#include "vnums.h"
#include "dg_scripts.h"

/**
* Contents:
*   Helpers
*   Generic Craft (craft, forge, sew, cook)
*   Reforge / Refashion
*   Commands
*/

// external vars
extern const struct augment_type_data augment_info[];
extern const char *craft_types[];
extern struct gen_craft_data_t gen_craft_data[];

// external functions
extern struct resource_data *copy_resource_list(struct resource_data *input);
extern double get_enchant_scale_for_char(char_data *ch, int max_scale);
extern bool has_cooking_fire(char_data *ch);
extern bool has_learned_craft(char_data *ch, any_vnum vnum);
extern obj_data *has_sharp_tool(char_data *ch);
void scale_item_to_level(obj_data *obj, int level);
extern bool validate_augment_target(char_data *ch, obj_data *obj, augment_data *aug, bool send_messages);

// locals
bool can_forge(char_data *ch);
bool can_refashion(char_data *ch);
ACMD(do_gen_craft);
craft_data *find_craft_for_obj_vnum(obj_vnum vnum);
obj_data *find_water_container(char_data *ch, obj_data *list);
obj_data *has_hammer(char_data *ch);
obj_data *has_required_obj_for_craft(char_data *ch, obj_vnum vnum);


 //////////////////////////////////////////////////////////////////////////////
//// HELPERS /////////////////////////////////////////////////////////////////

/**
* This validates the craft by type/flags and can be called when setting up the
* craft, or while processing it.
*
* @param char_data *ch The person crafting, or trying to.
* @param craft_data *type The craft they are attempting.
* @return bool TRUE if okay, FALSE if not.
*/
bool check_can_craft(char_data *ch, craft_data *type) {
	bool wait, room_wait;
	
	// type checks
	if (GET_CRAFT_TYPE(type) == CRAFT_TYPE_MILL && !room_has_function_and_city_ok(IN_ROOM(ch), FNC_MILL)) {
		msg_to_char(ch, "You need to be in a mill to do that.\r\n");
	}
	else if (GET_CRAFT_TYPE(type) == CRAFT_TYPE_PRESS && !room_has_function_and_city_ok(IN_ROOM(ch), FNC_PRESS)) {
		msg_to_char(ch, "You need a press to do that.\r\n");
	}
	else if (CRAFT_FLAGGED(type, CRAFT_BY_RIVER) && (!IS_OUTDOORS(ch) || !find_flagged_sect_within_distance_from_char(ch, SECTF_FRESH_WATER, NOBITS, 1))) {
		msg_to_char(ch, "You must be next to a river to do that.\r\n");
	}
	else if (GET_CRAFT_TYPE(type) == CRAFT_TYPE_FORGE && !can_forge(ch)) {
		// sends its own message
	}
	else if (GET_CRAFT_TYPE(type) == CRAFT_TYPE_SMELT && !room_has_function_and_city_ok(IN_ROOM(ch), FNC_SMELT)) {
		msg_to_char(ch, "You can't %s here.\r\n", gen_craft_data[GET_CRAFT_TYPE(type)].command);
	}
	
	// flag checks
	else if (IS_SET(GET_CRAFT_FLAGS(type), CRAFT_IN_CITY_ONLY) && !is_in_city_for_empire(IN_ROOM(ch), GET_LOYALTY(ch), TRUE, &wait) && !is_in_city_for_empire(IN_ROOM(ch), ROOM_OWNER(IN_ROOM(ch)), TRUE, &room_wait)) {
		msg_to_char(ch, "You can only make that in a city%s.\r\n", (wait || room_wait) ? " (this city was founded too recently)" : "");
	}
	else if ((GET_CRAFT_TYPE(type) == CRAFT_TYPE_MILL || GET_CRAFT_TYPE(type) == CRAFT_TYPE_PRESS || GET_CRAFT_TYPE(type) == CRAFT_TYPE_FORGE || GET_CRAFT_TYPE(type) == CRAFT_TYPE_SMELT || IS_SET(GET_CRAFT_FLAGS(type), CRAFT_CARPENTER | CRAFT_SHIPYARD | CRAFT_GLASSBLOWER)) && !check_in_city_requirement(IN_ROOM(ch), TRUE)) {
		msg_to_char(ch, "You can't do that here because this building isn't in a city.\r\n");
	}
	else if (IS_SET(GET_CRAFT_FLAGS(type), CRAFT_SHARP) && !has_sharp_tool(ch)) {
		msg_to_char(ch, "You need to be using a sharp tool to %s.\r\n", gen_craft_data[GET_CRAFT_TYPE(type)].command);
	}
	else if (IS_SET(GET_CRAFT_FLAGS(type), CRAFT_APIARIES) && !has_tech_available(ch, TECH_APIARIES)) {
		// message sent for us
	}
	else if (IS_SET(GET_CRAFT_FLAGS(type), CRAFT_GLASS) && !has_tech_available(ch, TECH_GLASSBLOWING)) {
		// message sent for us
	}
	else if (IS_SET(GET_CRAFT_FLAGS(type), CRAFT_POTTERY) && !has_cooking_fire(ch)) {
		msg_to_char(ch, "You need a fire to bake the clay.\r\n");
	}
	else if (IS_SET(GET_CRAFT_FLAGS(type), CRAFT_FIRE | CRAFT_ALCHEMY) && !has_cooking_fire(ch)) {
		msg_to_char(ch, "You need a good fire to do that.\r\n");
	}
	else if (IS_SET(GET_CRAFT_FLAGS(type), CRAFT_CARPENTER) && !room_has_function_and_city_ok(IN_ROOM(ch), FNC_CARPENTER)) {
		msg_to_char(ch, "You need to %s that at the carpenter!\r\n", gen_craft_data[GET_CRAFT_TYPE(type)].command);
	}
	else if (IS_SET(GET_CRAFT_FLAGS(type), CRAFT_SHIPYARD) && !room_has_function_and_city_ok(IN_ROOM(ch), FNC_SHIPYARD)) {
		msg_to_char(ch, "You need to %s that at the shipyard!\r\n", gen_craft_data[GET_CRAFT_TYPE(type)].command);
	}
	else if (IS_SET(GET_CRAFT_FLAGS(type), CRAFT_BLD_UPGRADED) && !ROOM_BLD_FLAGGED(IN_ROOM(ch), BLD_UPGRADED)) {
		msg_to_char(ch, "The building needs to be upgraded to %s that!\r\n", gen_craft_data[GET_CRAFT_TYPE(type)].command);
	}
	else if (IS_SET(GET_CRAFT_FLAGS(type), CRAFT_GLASSBLOWER) && !room_has_function_and_city_ok(IN_ROOM(ch), FNC_GLASSBLOWER)) {
		msg_to_char(ch, "You need to %s that at the glassblower!\r\n", gen_craft_data[GET_CRAFT_TYPE(type)].command);
	}
	else if (IS_SET(GET_CRAFT_FLAGS(type), CRAFT_SOUP) && !find_water_container(ch, ch->carrying) && !find_water_container(ch, ROOM_CONTENTS(IN_ROOM(ch)))) {
		msg_to_char(ch, "You need a container of water to %s that.\r\n", gen_craft_data[GET_CRAFT_TYPE(type)].command);
	}
	else if (IS_SET(GET_CRAFT_FLAGS(type), CRAFT_ALCHEMY) && !room_has_function_and_city_ok(IN_ROOM(ch), FNC_ALCHEMIST) && !has_tech_available(ch, TECH_GLASSBLOWING)) {
		// sends its own messages -- needs glassblowing unless in alchemist room
	}
	else if (IS_SET(GET_CRAFT_FLAGS(type), CRAFT_FLAGS_REQUIRING_BUILDINGS) && !check_in_city_requirement(IN_ROOM(ch), TRUE)) {
		msg_to_char(ch, "This building must be in a city to use it.\r\n");
	}
	// end flag checks
	
	// types that require the building be complete
	else if (!IS_COMPLETE(IN_ROOM(ch)) && (IS_SET(GET_CRAFT_FLAGS(type), CRAFT_SHIPYARD) || IS_SET(GET_CRAFT_FLAGS(type), CRAFT_BLD_UPGRADED) || IS_SET(GET_CRAFT_FLAGS(type), CRAFT_GLASSBLOWER) || IS_SET(GET_CRAFT_FLAGS(type), CRAFT_CARPENTER) || GET_CRAFT_TYPE(type) == CRAFT_TYPE_MILL || GET_CRAFT_TYPE(type) == CRAFT_TYPE_SMELT || GET_CRAFT_TYPE(type) == CRAFT_TYPE_PRESS)) {
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
	
	if (!IS_IMMORTAL(ch) && !room_has_function_and_city_ok(IN_ROOM(ch), FNC_FORGE)) {
		msg_to_char(ch, "You need to be in a forge to do that.\r\n");
	}
	else if (!IS_IMMORTAL(ch) && !room_has_function_and_city_ok(IN_ROOM(ch), FNC_FORGE)) {
		msg_to_char(ch, "This forge only works if it's in a city.\r\n");
	}
	else if (!IS_COMPLETE(IN_ROOM(ch))) {
		msg_to_char(ch, "You must complete the building first.\r\n");
	}
	else if (!check_in_city_requirement(IN_ROOM(ch), TRUE)) {
		msg_to_char(ch, "This building must be in a city to use it.\r\n");
	}
	else if (!has_hammer(ch)) {
		// sends own message
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
		LL_FOREACH2(search[list], iter, next_content) {
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
* always prefers an exact match over anything. Immortals can also hit in-dev
* recipes.
*
* @param char_data *ch The person looking for a craft.
* @param char *argument The typed-in name.
* @param int craft_type Any CRAFT_TYPE_ to look up.
* @return craft_data* The matching craft, if any.
*/
craft_data *find_best_craft_by_name(char_data *ch, char *argument, int craft_type) {
	craft_data *unknown_abbrev = NULL;
	craft_data *known_abbrev = NULL;
	craft_data *craft, *next_craft;
	
	skip_spaces(&argument);
	
	HASH_ITER(sorted_hh, sorted_crafts, craft, next_craft) {
		if (GET_CRAFT_TYPE(craft) != craft_type) {
			continue;
		}
		if (IS_SET(GET_CRAFT_FLAGS(craft), CRAFT_IN_DEVELOPMENT) && !IS_IMMORTAL(ch)) {
			continue;
		}
		if (GET_CRAFT_REQUIRES_OBJ(craft) != NOTHING && !has_required_obj_for_craft(ch, GET_CRAFT_REQUIRES_OBJ(craft))) {
			continue;
		}
		
		if (!str_cmp(argument, GET_CRAFT_NAME(craft))) {
			// exact match!
			return craft;
		}
		else if (!known_abbrev && is_abbrev(argument, GET_CRAFT_NAME(craft))) {
			if (IS_SET(GET_CRAFT_FLAGS(craft), CRAFT_IN_DEVELOPMENT)) {
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
				known_abbrev = craft;
			}
		}
	}
	
	// if we got this far, it didn't return an exact match
	return known_abbrev ? known_abbrev : unknown_abbrev;
}


/**
* Finds an unfinished vehicle in the room that the character can finish.
*
* @param char_data *ch The person trying to craft a vehicle.
* @param craft_data *type The craft recipe to match up.
* @param bool *any Is set to TRUE if there are any unfinished vehicles that don't otherwise match.
* @return vehicle_data* The found vehicle, or NULL if none.
*/
vehicle_data *find_finishable_vehicle(char_data *ch, craft_data *type, bool *any) {
	vehicle_data *iter;
	
	*any = FALSE;
	
	LL_FOREACH2(ROOM_VEHICLES(IN_ROOM(ch)), iter, next_in_room) {
		// skip finished vehicles
		if (VEH_IS_COMPLETE(iter)) {
			continue;
		}
		// there is at least 1 incomplete vehicle here
		*any = TRUE;
		
		// right vehicle?
		if (VEH_VNUM(iter) != GET_CRAFT_OBJECT(type)) {
			continue;
		}
		if (!can_use_vehicle(ch, iter, GUESTS_ALLOWED)) {
			continue;
		}
		
		// found one!
		return iter;
	}
	
	return NULL;
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
	craft_data *craft, *next_craft;
	vehicle_data *veh;
	
	*found_craft = NULL;
	
	LL_FOREACH2(ROOM_VEHICLES(IN_ROOM(ch)), veh, next_in_room) {
		if (VEH_IS_COMPLETE(veh)) {
			continue;	// skip finished vehicles
		}
		if (!multi_isname(name, VEH_KEYWORDS(veh))) {
			continue;	// not a keyword match
		}
		if (!can_use_vehicle(ch, veh, GUESTS_ALLOWED)) {
			continue;	// not allowed to work on it
		}
		
		// seems to be a match... now find a matching craft
		HASH_ITER(hh, craft_table, craft, next_craft) {
			if (CRAFT_FLAGGED(craft, CRAFT_IN_DEVELOPMENT) || !CRAFT_FLAGGED(craft, CRAFT_VEHICLE)) {
				continue;	// not a valid target
			}
			if (GET_CRAFT_OBJECT(craft) != VEH_VNUM(veh)) {
				continue;
			}
			
			// we have a match!
			*found_craft = craft;
			return veh;
		}
	}
	
	return NULL;
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
	
	for (obj = list; !found && obj; obj = obj->next_content) {
		if (IS_DRINK_CONTAINER(obj) && CAN_SEE_OBJ(ch, obj) && GET_DRINK_CONTAINER_TYPE(obj) == LIQ_WATER && GET_DRINK_CONTAINER_CONTENTS(obj) >= (GET_DRINK_CONTAINER_CAPACITY(obj)/2)) {
			found = obj;
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
	obj_data *req;
	
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
	}
	else {
		// no craft given
		level = craft_lev;
	}
	
	return level;
}


/**
* This finds a hammer in either tool slot, and returns it.
*
* @param char_data *ch The person using the hammer?
* @return obj_data *The hammer object.
*/
obj_data *has_hammer(char_data *ch) {
	obj_data *hammer = NULL;
	int iter;
	
	// list of valid slots; terminate with -1
	int slots[] = { WEAR_WIELD, WEAR_HOLD, WEAR_SHEATH_1, WEAR_SHEATH_2, -1 };
	
	for (iter = 0; slots[iter] != -1; ++iter) {
		hammer = GET_EQ(ch, slots[iter]);
		if (hammer && IS_WEAPON(hammer) && GET_WEAPON_TYPE(hammer) == TYPE_HAMMER) {
			return hammer;
		}
	}
	
	// nope
	msg_to_char(ch, "You need to use a hammer to do that.\r\n");
	return NULL;
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
	LL_FOREACH2(ch->carrying, obj, next_content) {
		if (GET_OBJ_VNUM(obj) == vnum) {
			return obj;
		}
	}
	
	// room
	LL_FOREACH2(ROOM_CONTENTS(IN_ROOM(ch)), obj, next_content) {
		if (GET_OBJ_VNUM(obj) == vnum && bind_ok(obj, ch)) {
			return obj;
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
	
	start_action(ch, ACT_GEN_CRAFT, -1);
	GET_ACTION_VNUM(ch, 0) = GET_CRAFT_VNUM(craft);
	
	snprintf(buf, sizeof(buf), "You resume %s $V.", gen_craft_data[GET_CRAFT_TYPE(craft)].verb);
	act(buf, FALSE, ch, NULL, veh, TO_CHAR);
	snprintf(buf, sizeof(buf), "$n resumes %s $V.", gen_craft_data[GET_CRAFT_TYPE(craft)].verb);
	act(buf, FALSE, ch, NULL, veh, TO_ROOM);
}


/**
* This is like a mortal version of do_stat_craft().
*
* @param char_data *ch The person checking the craft info.
* @param craft_data *craft Which craft to show.
* @param int craft_type Whichever CRAFT_TYPE_ the player is using.
*/
void show_craft_info(char_data *ch, char *argument, int craft_type) {
	extern const char *affected_bits[];
	extern const char *apply_types[];
	extern const char *bld_on_flags[];
	extern const char *craft_flag_for_info[];
	extern const char *item_types[];
	extern const char *wear_bits[];
	
	char buf[MAX_STRING_LENGTH], part[MAX_STRING_LENGTH], range[MAX_STRING_LENGTH];
	struct obj_apply *apply;
	ability_data *abil;
	craft_data *craft;
	obj_data *proto;
	bld_data *bld;
	
	if (!*argument) {
		msg_to_char(ch, "Get %s info on what?\r\n", gen_craft_data[craft_type].command);
		return;
	}
	if (!(craft = find_best_craft_by_name(ch, argument, craft_type))) {
		msg_to_char(ch, "You don't know any such %s recipe.\r\n", gen_craft_data[craft_type].command);
		return;
	}
	
	msg_to_char(ch, "Information for %s:\r\n", GET_CRAFT_NAME(craft));
	
	if (GET_CRAFT_TYPE(craft) == CRAFT_TYPE_BUILD) {
		bld = building_proto(GET_CRAFT_BUILD_TYPE(craft));
		msg_to_char(ch, "Builds: %s\r\n", bld ? GET_BLD_NAME(bld) : "UNKNOWN");
	}
	else if (CRAFT_FLAGGED(craft, CRAFT_VEHICLE)) {
		msg_to_char(ch, "Creates vehicle: %s\r\n", get_vehicle_name_by_proto(GET_CRAFT_OBJECT(craft)));
	}
	else if (CRAFT_FLAGGED(craft, CRAFT_SOUP)) {
		msg_to_char(ch, "Creates liquid: %d unit%s of %s\r\n", GET_CRAFT_QUANTITY(craft), PLURAL(GET_CRAFT_QUANTITY(craft)), get_generic_string_by_vnum(GET_CRAFT_OBJECT(craft), GENERIC_LIQUID, GSTR_LIQUID_NAME));
	}
	else if ((proto = obj_proto(GET_CRAFT_OBJECT(craft)))) {
		// build info string
		sprintf(buf, " (%s", item_types[(int) GET_OBJ_TYPE(proto)]);
		if (GET_OBJ_WEAR(proto) & ~ITEM_WEAR_TAKE) {
			prettier_sprintbit(GET_OBJ_WEAR(proto) & ~ITEM_WEAR_TAKE, wear_bits, part);
			sprintf(buf + strlen(buf), ", %s", part);
		}
		LL_FOREACH(GET_OBJ_APPLIES(proto), apply) {
			// don't show applies that can't come from crafting
			if (apply->apply_type != APPLY_TYPE_HARD_DROP && apply->apply_type != APPLY_TYPE_GROUP_DROP && apply->apply_type != APPLY_TYPE_BOSS_DROP) {
				sprintf(buf + strlen(buf), ", %s%s", (apply->modifier<0 ? "-" : "+"), apply_types[(int) apply->location]);
			}
		}
		if (GET_OBJ_AFF_FLAGS(proto)) {
			prettier_sprintbit(GET_OBJ_AFF_FLAGS(proto), affected_bits, part);
			sprintf(buf + strlen(buf), ", %s", part);
		}
		strcat(buf, ")");
		
		if (GET_OBJ_MIN_SCALE_LEVEL(proto) > 0 || GET_OBJ_MAX_SCALE_LEVEL(proto) > 0) {
			sprintf(range, " [%s]", level_range_string(GET_OBJ_MIN_SCALE_LEVEL(proto), GET_OBJ_MAX_SCALE_LEVEL(proto), 0));
		}
		else {
			*range = '\0';
		}
		
		if (GET_CRAFT_QUANTITY(craft) == 1) {
			msg_to_char(ch, "Creates: %s%s%s\r\n", get_obj_name_by_proto(GET_CRAFT_OBJECT(craft)), range, buf);
		}
		else {
			msg_to_char(ch, "Creates: %dx %s%s%s\r\n", GET_CRAFT_QUANTITY(craft), get_obj_name_by_proto(GET_CRAFT_OBJECT(craft)), range, buf);
		}
	}
	
	if (GET_CRAFT_REQUIRES_OBJ(craft) != NOTHING) {
		msg_to_char(ch, "Requires: %s\r\n", get_obj_name_by_proto(GET_CRAFT_REQUIRES_OBJ(craft)));
	}
	
	if (GET_CRAFT_ABILITY(craft) != NO_ABIL) {
		sprintf(buf, "%s", get_ability_name_by_vnum(GET_CRAFT_ABILITY(craft)));
		if ((abil = find_ability_by_vnum(GET_CRAFT_ABILITY(craft))) && ABIL_ASSIGNED_SKILL(abil) != NULL) {
			sprintf(buf + strlen(buf), " (%s %d)", SKILL_NAME(ABIL_ASSIGNED_SKILL(abil)), ABIL_SKILL_LEVEL(abil));
		}
		if (abil && ABIL_MASTERY_ABIL(abil) != NOTHING) {
			sprintf(buf + strlen(buf), ", Mastery: %s", get_ability_name_by_vnum(ABIL_MASTERY_ABIL(abil)));
		}
		msg_to_char(ch, "Requires: %s\r\n", buf);
	}
	
	if (GET_CRAFT_MIN_LEVEL(craft) > 0) {
		msg_to_char(ch, "Requires: crafting level %d\r\n", GET_CRAFT_MIN_LEVEL(craft));
	}
	
	prettier_sprintbit(GET_CRAFT_FLAGS(craft), craft_flag_for_info, part);
	msg_to_char(ch, "Notes: %s\r\n", part);
	
	if (GET_CRAFT_TYPE(craft) == CRAFT_TYPE_BUILD) {
		prettier_sprintbit(GET_CRAFT_BUILD_ON(craft), bld_on_flags, buf);
		msg_to_char(ch, "Build on: %s\r\n", buf);
		if (GET_CRAFT_BUILD_FACING(craft)) {
			prettier_sprintbit(GET_CRAFT_BUILD_FACING(craft), bld_on_flags, buf);
			msg_to_char(ch, "Build facing: %s\r\n", buf);
		}
	}
	
	show_resource_list(GET_CRAFT_RESOURCES(craft), buf);
	msg_to_char(ch, "Resources: %s\r\n", buf);	
	
	if (IS_SET(GET_CRAFT_FLAGS(craft), CRAFT_LEARNED) && !has_learned_craft(ch, GET_CRAFT_VNUM(craft))) {
		msg_to_char(ch, "&rYou haven't learned this recipe.&0\r\n");
	}
}


// for do_tame
INTERACTION_FUNC(tame_interact) {
	void setup_generic_npc(char_data *mob, empire_data *emp, int name, int sex);
	
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
		GET_HEALTH(newmob) = (int)(prc * GET_MAX_HEALTH(newmob));
		
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


// for gen_craft_data[].strings
#define GCD_STRING_TO_CHAR  0
#define GCD_STRING_TO_ROOM  1


// CRAFT_TYPE_x
struct gen_craft_data_t gen_craft_data[] = {
	{ "error", "erroring", NOBITS, { "", "" } },	// dummy to require scmd
	
	// Note: These correspond to CRAFT_TYPE_x so you cannot change the order.
	{ "forge", "forging", NOBITS, { "You hit the %s on the anvil hard with $p!", "$n hits the %s on the anvil hard with $p!" } },
	{ "craft", "crafting", NOBITS, { "You continue crafting the %s...", "$n continues crafting the %s..." } },
	{ "cook", "cooking", ACTF_FAST_CHORES, { "You continue cooking the %s...", "$n continues cooking the %s..." } },
	{ "sew", "sewing", NOBITS, { "You carefully sew the %s...", "$n carefully sews the %s..." } },
	{ "mill", "milling", NOBITS, { "You grind the millstone, making %s...", "$n grinds the millstone, making %s..." } },
	{ "brew", "brewing", NOBITS, { "You stir the potion and infuse it with mana...", "$n stirs the potion..." } },
	{ "mix", "mixing", NOBITS, { "The poison bubbles as you stir it...", "$n stirs the bubbling poison..." } },
	
	// build is special and doesn't use do_gen_craft, so doesn't really use this data
	{ "build", "building", NOBITS, { "You work on the building...", "$n works on the building..." } },
	
	{ "weave", "weaving", NOBITS, { "You carefully weave the %s...", "$n carefully weaves the %s..." } },
	
	{ "workforce", "producing", NOBITS, { "You work on the %s...", "$n work on the %s..." } },	// not used by players
	
	{ "manufacture", "manufacturing", NOBITS, { "You carefully manufacture the %s...", "$n carefully manufactures the %s..." } },
	{ "smelt", "smelting", ACTF_FAST_CHORES, { "You smelt the %s in the fire...", "$n smelts the %s in the fire..." } },
	{ "press", "pressing", NOBITS, { "You press the %s...", "$n presses the %s..." } },
};


void cancel_gen_craft(char_data *ch) {
	craft_data *type = craft_proto(GET_ACTION_VNUM(ch, 0));
	obj_data *obj;
	
	if (type && !CRAFT_FLAGGED(type, CRAFT_VEHICLE)) {
		// refund the real resources they used
		give_resources(ch, GET_ACTION_RESOURCES(ch), FALSE);
		free_resource_list(GET_ACTION_RESOURCES(ch));
		GET_ACTION_RESOURCES(ch) = NULL;

		// load the drink container back
		if (IS_SET(GET_CRAFT_FLAGS(type), CRAFT_SOUP)) {
			obj = read_object(GET_ACTION_VNUM(ch, 1), TRUE);

			// just empty it
			GET_OBJ_VAL(obj, VAL_DRINK_CONTAINER_CONTENTS) = 0;
			if (CAN_WEAR(obj, ITEM_WEAR_TAKE)) {
				obj_to_char(obj, ch);
			}
			else {
				obj_to_room(obj, IN_ROOM(ch));
			}
			load_otrigger(obj);
		}
	}
	
	GET_ACTION(ch) = ACT_NONE;
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
		if (!IS_SET(GET_CRAFT_FLAGS(iter), CRAFT_SOUP | CRAFT_VEHICLE) && GET_CRAFT_TYPE(iter) != CRAFT_TYPE_BUILD && GET_CRAFT_OBJECT(iter) == vnum) {
			return iter;
		}
	}
	
	return NULL;
}


void finish_gen_craft(char_data *ch) {
	craft_data *type = craft_proto(GET_ACTION_VNUM(ch, 0));
	bool applied_master = FALSE, is_master = FALSE;
	int num = GET_ACTION_VNUM(ch, 2);
	char lbuf[MAX_INPUT_LENGTH];
	ability_data *cft_abil;
	obj_data *obj = NULL;
	int iter, amt = 1;
	
	cft_abil = find_ability_by_vnum(GET_CRAFT_ABILITY(type));
	is_master = (cft_abil && ABIL_MASTERY_ABIL(cft_abil) != NOTHING && has_ability(ch, ABIL_MASTERY_ABIL(cft_abil)));
	
	GET_ACTION(ch) = ACT_NONE;

	// basic sanity checking (vehicles are finished elsewhere
	if (!type || CRAFT_FLAGGED(type, CRAFT_VEHICLE)) {
		return;
	}
	
	// soup handling
	if (IS_SET(GET_CRAFT_FLAGS(type), CRAFT_SOUP)) {
		// load the drink container back
		obj = read_object(GET_ACTION_VNUM(ch, 1), TRUE);
	
		GET_OBJ_VAL(obj, VAL_DRINK_CONTAINER_CONTENTS) = MIN(GET_CRAFT_QUANTITY(type), GET_DRINK_CONTAINER_CAPACITY(obj));
		GET_OBJ_VAL(obj, VAL_DRINK_CONTAINER_TYPE) = GET_CRAFT_OBJECT(type);
	
		// set it to go bad... very bad
		GET_OBJ_TIMER(obj) = SOUP_TIMER;
		if (CAN_WEAR(obj, ITEM_WEAR_TAKE)) {
			obj_to_char(obj, ch);
		}
		else {
			obj_to_room(obj, IN_ROOM(ch));
		}
		scale_item_to_level(obj, get_craft_scale_level(ch, type));
		
		load_otrigger(obj);
	}
	else if (GET_CRAFT_QUANTITY(type) > 0) {
		// NON-SOUP (careful, soup uses quantity for maximum contents
	
		amt = GET_CRAFT_QUANTITY(type);
	
		if (GET_CRAFT_TYPE(type) == CRAFT_TYPE_MILL && has_player_tech(ch, PTECH_MILL_UPGRADE)) {
			gain_player_tech_exp(ch, PTECH_MILL_UPGRADE, 10);
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
				
				load_otrigger(obj);
			}
			
			// mark for the empire
			if (GET_LOYALTY(ch)) {
				add_production_total(GET_LOYALTY(ch), GET_CRAFT_OBJECT(type), amt);
			}
		}
	}

	// send message -- soup uses quantity for amount of soup instead of multiple items
	if (amt > 1 && !IS_SET(GET_CRAFT_FLAGS(type), CRAFT_SOUP)) {
		sprintf(buf, "You finish %s $p (x%d)!", gen_craft_data[GET_CRAFT_TYPE(type)].verb, amt);
		act(buf, FALSE, ch, obj, 0, TO_CHAR);
	}
	else {
		sprintf(buf, "You finish %s $p!", gen_craft_data[GET_CRAFT_TYPE(type)].verb);
		act(buf, FALSE, ch, obj, 0, TO_CHAR);
	}

	sprintf(buf, "$n finishes %s $p!", gen_craft_data[GET_CRAFT_TYPE(type)].verb);
	act(buf, FALSE, ch, obj, 0, TO_ROOM);
	
	if (GET_CRAFT_ABILITY(type) != NO_ABIL) {
		gain_ability_exp(ch, GET_CRAFT_ABILITY(type), 33.4);
	}
	
	// master?
	if (is_master && applied_master) {
		gain_ability_exp(ch, ABIL_MASTERY_ABIL(cft_abil), 33.4);
	}
	
	// free the stored action resources now -- we no longer risk refunding them
	free_resource_list(GET_ACTION_RESOURCES(ch));
	GET_ACTION_RESOURCES(ch) = NULL;

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
	void adjust_vehicle_tech(vehicle_data *veh, bool add);
	void finish_vehicle_setup(vehicle_data *veh);
	
	bool found = FALSE, any = FALSE;
	char buf[MAX_STRING_LENGTH];
	obj_data *found_obj = NULL;
	struct resource_data *res;
	vehicle_data *veh;
	char_data *vict;
	
	// basic setup
	if (!type || !check_can_craft(ch, type) || !(veh = find_finishable_vehicle(ch, type, &any))) {
		cancel_gen_craft(ch);
		return;
	}
	if (!CAN_SEE_IN_DARK_ROOM(ch, IN_ROOM(ch))) {
		msg_to_char(ch, "It's too dark to finish %s.\r\n", gen_craft_data[GET_CRAFT_TYPE(type)].verb);
		cancel_gen_craft(ch);
		return;
	}
	
	// find and apply something
	if ((res = get_next_resource(ch, VEH_NEEDS_RESOURCES(veh), can_use_room(ch, IN_ROOM(ch), MEMBERS_ONLY), FALSE, &found_obj))) {
		// take the item; possibly free the res
		apply_resource(ch, res, &VEH_NEEDS_RESOURCES(veh), found_obj, APPLY_RES_CRAFT, veh, NULL);
		
		// experience per resource
		if (GET_CRAFT_ABILITY(type) != NO_ABIL) {
			gain_ability_exp(ch, GET_CRAFT_ABILITY(type), 3);
		}
		
		found = TRUE;
	}
	
	// done?
	if (!VEH_NEEDS_RESOURCES(veh)) {
		REMOVE_BIT(VEH_FLAGS(veh), VEH_INCOMPLETE);
		VEH_HEALTH(veh) = VEH_MAX_HEALTH(veh);
		act("$V is finished!", FALSE, ch, NULL, veh, TO_CHAR | TO_ROOM);
		if (VEH_OWNER(veh)) {
			qt_empire_players(VEH_OWNER(veh), qt_gain_vehicle, VEH_VNUM(veh));
			et_gain_vehicle(VEH_OWNER(veh), VEH_VNUM(veh));
		}
		
		// stop all actors on this type
		LL_FOREACH2(ROOM_PEOPLE(IN_ROOM(ch)), vict, next_in_room) {
			if (!IS_NPC(vict) && GET_ACTION(vict) == ACT_GEN_CRAFT && GET_ACTION_VNUM(vict, 0) == GET_CRAFT_VNUM(type)) {
				GET_ACTION(vict) = ACT_NONE;
			}
		}
		
		if (VEH_OWNER(veh)) {
			adjust_vehicle_tech(veh, TRUE);
		}
		finish_vehicle_setup(veh);
		load_vtrigger(veh);
	}
	else if (!found) {
		msg_to_char(ch, "You run out of resources and stop %s.\r\n", gen_craft_data[GET_CRAFT_TYPE(type)].verb);
		snprintf(buf, sizeof(buf), "$n runs out of resources and stops %s.", gen_craft_data[GET_CRAFT_TYPE(type)].verb);
		act(buf, FALSE, ch, NULL, NULL, TO_ROOM);
		GET_ACTION(ch) = ACT_NONE;
	}
}


/**
* Action processor for gen_craft.
*
* @param char_data *ch The actor.
*/
void process_gen_craft(char_data *ch) {
	obj_data *weapon = NULL;
	craft_data *type = craft_proto(GET_ACTION_VNUM(ch, 0));
	
	if (!type) {
		cancel_gen_craft(ch);
		return;
	}
	
	// pass off control entirely for a vehicle craft
	if (CRAFT_FLAGGED(type, CRAFT_VEHICLE)) {
		process_gen_craft_vehicle(ch, type);
		return;
	}
	
	// things that check for & set weapon
	if (GET_CRAFT_TYPE(type) == CRAFT_TYPE_FORGE && !(weapon = has_hammer(ch)) && !can_forge(ch)) {
		// can_forge sends its own message
		cancel_gen_craft(ch);
	}
	else if (IS_SET(GET_CRAFT_FLAGS(type), CRAFT_SHARP) && !(weapon = has_sharp_tool(ch))) {
		msg_to_char(ch, "You need to be using a sharp tool to %s.\r\n", gen_craft_data[GET_CRAFT_TYPE(type)].command);
		cancel_gen_craft(ch);
	}
	else if (!CAN_SEE_IN_DARK_ROOM(ch, IN_ROOM(ch))) {
		msg_to_char(ch, "It's too dark to finish %s.\r\n", gen_craft_data[GET_CRAFT_TYPE(type)].verb);
		cancel_gen_craft(ch);
	}
	else {
		GET_ACTION_TIMER(ch) -= 1;

		// bonus point for superior tool -- if this craft requires a tool
		if (weapon && OBJ_FLAGGED(weapon, OBJ_SUPERIOR)) {
			GET_ACTION_TIMER(ch) -= 1;
		}
		
		// tailor bonus for weave
		if (GET_CRAFT_TYPE(type) == CRAFT_TYPE_WEAVE && room_has_function_and_city_ok(IN_ROOM(ch), FNC_TAILOR) && IS_COMPLETE(IN_ROOM(ch))) {
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
	char *command;
	any_vnum ability;	// required ability
	bool (*validate_func)(char_data *ch);	// e.g. can_forge, func that returns TRUE if ok -- must send own errors if FALSE
	int types[4];	// NOTHING-terminated list of valid obj types
} reforge_data[] = {
	{ "reforge", ABIL_REFORGE, can_forge, { ITEM_WEAPON, ITEM_MISSILE_WEAPON, NOTHING } },	// SCMD_REFORGE
	{ "refashion", ABIL_REFASHION, can_refashion, { ITEM_ARMOR, ITEM_SHIELD, ITEM_WORN, NOTHING } }	// SCMD_REFASHION
};


/**
* @param char_data *ch The person trying to refashion.
* @return bool TRUE if they can, false if they can't.
*/
bool can_refashion(char_data *ch) {
	bool ok = FALSE;
	
	if (!IS_IMMORTAL(ch) && !room_has_function_and_city_ok(IN_ROOM(ch), FNC_TAILOR)) {
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
	else if (strchr(name, '&') || strchr(name, '%')) {
		msg_to_char(ch, "Item names cannot contain the \t& or %% symbols.\r\n");
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
	extern augment_data *find_augment_by_name(char_data *ch, char *name, int type);
	extern char *shared_by(obj_data *obj, char_data *ch);
	extern const bool apply_never_scales[];
	extern const double apply_values[];
	
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
	else if (!(obj = get_obj_in_list_vis(ch, target_arg, ch->carrying)) && !(obj = get_obj_by_char_share(ch, target_arg))) {
		msg_to_char(ch, "You don't seem to have any %s.\r\n", target_arg);
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
		msg_to_char(ch, "That item already has a %s effect.\r\n", augment_info[subcmd].noun);
	}
	else if (!validate_augment_target(ch, obj, aug, TRUE)) {
		// sends own message
	}
	else if (!has_resources(ch, GET_AUG_RESOURCES(aug), FALSE, TRUE)) {
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
				
				if (last_apply) {
					last_apply->next = apply;
				}
				else {
					GET_OBJ_APPLIES(obj) = apply;
				}
				last_apply = apply;
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
		}
		if (abil && is_master) {
			gain_ability_exp(ch, ABIL_MASTERY_ABIL(abil), 50);
		}
		
		command_lag(ch, WAIT_ABILITY);
	}
}


/**
* Sub-processor for crafting a vehicle.
*
* @param char_data *ch The player trying to craft the vehicle.
* @param craft_data *type Pre-selected vehicle craft.
*/
void do_gen_craft_vehicle(char_data *ch, craft_data *type) {
	void scale_vehicle_to_level(vehicle_data *veh, int level);
	
	vehicle_data *veh;
	char buf[MAX_STRING_LENGTH];
	bool any = FALSE;
	
	// basic sanitation
	if (!type || !CRAFT_FLAGGED(type, CRAFT_VEHICLE) || !vehicle_proto(GET_CRAFT_OBJECT(type))) {
		log("SYSERR: do_gen_craft_vehicle called with invalid vehicle craft %d", type ? GET_CRAFT_VNUM(type) : NOTHING);
		return;
	}
	
	if (!CAN_SEE_IN_DARK_ROOM(ch, IN_ROOM(ch))) {
		msg_to_char(ch, "It's too dark to %s anything.\r\n", gen_craft_data[GET_CRAFT_TYPE(type)].command);
		return;
	}
	
	// found one to resume
	if ((veh = find_finishable_vehicle(ch, type, &any))) {
		resume_craft_vehicle(ch, veh, type);
		return;
	}
	
	if (any) {
		msg_to_char(ch, "You can't %s that while there's already an unfinished vehicle here.\r\n", gen_craft_data[GET_CRAFT_TYPE(type)].command);
		return;
	}
	
	// new vehicle craft setup
	veh = read_vehicle(GET_CRAFT_OBJECT(type), TRUE);
	vehicle_to_room(veh, IN_ROOM(ch));
	
	// additional setup
	SET_BIT(VEH_FLAGS(veh), VEH_INCOMPLETE);
	VEH_NEEDS_RESOURCES(veh) = copy_resource_list(GET_CRAFT_RESOURCES(type));
	if (!VEH_FLAGGED(veh, VEH_NO_CLAIM)) {
		VEH_OWNER(veh) = GET_LOYALTY(ch);
	}
	VEH_HEALTH(veh) = MAX(1, VEH_MAX_HEALTH(veh) * 0.2);	// start at 20% health, will heal on completion
	scale_vehicle_to_level(veh, get_craft_scale_level(ch, type));
	
	start_action(ch, ACT_GEN_CRAFT, -1);
	GET_ACTION_VNUM(ch, 0) = GET_CRAFT_VNUM(type);
	
	snprintf(buf, sizeof(buf), "You lay the framework and begin %s $V.", gen_craft_data[GET_CRAFT_TYPE(type)].verb);
	act(buf, FALSE, ch, NULL, veh, TO_CHAR);
	snprintf(buf, sizeof(buf), "$n lays the framework and begins %s $V.", gen_craft_data[GET_CRAFT_TYPE(type)].verb);
	act(buf, FALSE, ch, NULL, veh, TO_ROOM);
	
	if (GET_CRAFT_REQUIRES_OBJ(type) != NOTHING) {
		find_and_bind(ch, GET_CRAFT_REQUIRES_OBJ(type));
	}
}


// subcmd must be CRAFT_TYPE_
ACMD(do_gen_craft) {	
	int timer, num = 1;
	bool this_line, found;
	craft_data *craft, *next_craft, *type = NULL, *find_type = NULL, *abbrev_match = NULL;
	vehicle_data *veh;
	bool is_master;
	obj_data *found_obj = NULL, *drinkcon = NULL;
	ability_data *cft_abil;
	
	if (IS_NPC(ch)) {
		msg_to_char(ch, "NPCs can't craft.\r\n");
		return;
	}
	if (!IS_APPROVED(ch) && config_get_bool("craft_approval")) {
		send_config_msg(ch, "need_approval_string");
		return;
	}
	
	skip_spaces(&argument);
	
	// optional leading info request
	if (!strn_cmp(argument, "info ", 5)) {
		argument = any_one_arg(argument, arg);
		show_craft_info(ch, argument, subcmd);
		return;
	}
	// all other functions require standing
	if (GET_POS(ch) < POS_STANDING) {
		send_low_pos_msg(ch);
		return;
	}
	
	// optional leading number
	if ((num = atoi(argument)) > 0) {
		half_chop(argument, buf, arg);
		num = MAX(1, atoi(buf));
	}
	else {
		num = 1;
		strcpy(arg, argument);
	}
	
	// if there was an arg, find a matching craft_table entry (type)
	if (*arg) {
		HASH_ITER(sorted_hh, sorted_crafts, craft, next_craft) {
			if (GET_CRAFT_TYPE(craft) != subcmd) {
				continue;	// wrong craft type
			}	
			if (IS_SET(GET_CRAFT_FLAGS(craft), CRAFT_IN_DEVELOPMENT) && !IS_IMMORTAL(ch)) {
				continue;	// in-dev
			}
			if (GET_CRAFT_ABILITY(craft) != NO_ABIL && !has_ability(ch, GET_CRAFT_ABILITY(craft))) {
				continue;	// missing ability
			}
			if (GET_CRAFT_REQUIRES_OBJ(craft) != NOTHING && !has_required_obj_for_craft(ch, GET_CRAFT_REQUIRES_OBJ(craft))) {
				continue;	// missing requiresobj
			}
			if (IS_SET(GET_CRAFT_FLAGS(craft), CRAFT_LEARNED) && !has_learned_craft(ch, GET_CRAFT_VNUM(craft))) {
				continue;	// not learned
			}
			
			// match so far...
			if (!str_cmp(arg, GET_CRAFT_NAME(craft))) {
				// exact match!
				type = craft;
				break;
			}
			else if (!abbrev_match && is_abbrev(arg, GET_CRAFT_NAME(craft))) {
				// found! maybe
				abbrev_match = craft;
			}
		}
		
		// maybe we didn't find an exact match, but did find an abbrev match
		if (!type) {
			type = abbrev_match;
		}
	}

	if (subcmd == CRAFT_TYPE_ERROR) {
		msg_to_char(ch, "This command is not yet implemented.\r\n");
	}
	else if (*arg && (veh = find_vehicle_to_resume_by_name(ch, subcmd, arg, &find_type))) {
		// attempting to resume a vehicle by name
		if (GET_ACTION(ch) != ACT_NONE) {
			msg_to_char(ch, "You are already doing something.\r\n");
		}
		else {
			resume_craft_vehicle(ch, veh, find_type);
		}
	}
	else if (!*arg || !type) {
		// master craft list
		msg_to_char(ch, "What would you like to %s? You know how to make:\r\n", gen_craft_data[subcmd].command);
		this_line = FALSE;
		found = FALSE;
		*buf = '\0';
		
		HASH_ITER(sorted_hh, sorted_crafts, craft, next_craft) {
			if (GET_CRAFT_TYPE(craft) != subcmd) {
				continue;	// wrong craft type
			}
			if (IS_SET(GET_CRAFT_FLAGS(craft), CRAFT_IN_DEVELOPMENT) && !IS_IMMORTAL(ch)) {
				continue;	// in-dev
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
			
			// valid:
			if (strlen(buf) + strlen(GET_CRAFT_NAME(craft)) + 2 >= 80) {
				this_line = FALSE;
				msg_to_char(ch, "%s\r\n", buf);
				*buf = '\0';
			}
			sprintf(buf + strlen(buf), "%s%s", (this_line ? ", " : " "), GET_CRAFT_NAME(craft));
			this_line = TRUE;
			found = TRUE;
		}
		if (!found) {
			msg_to_char(ch, " nothing\r\n");
		}
		else if (this_line) {
			msg_to_char(ch, "%s\r\n", buf);
		}
	}
	else if (GET_ACTION(ch) != ACT_NONE) {
		msg_to_char(ch, "You're busy right now.\r\n");
	}
	else if (GET_CRAFT_ABILITY(type) != NO_ABIL && !has_ability(ch, GET_CRAFT_ABILITY(type))) {
		msg_to_char(ch, "You need to buy the %s ability to %s that.\r\n", get_ability_name_by_vnum(GET_CRAFT_ABILITY(type)), gen_craft_data[GET_CRAFT_TYPE(type)].command);
	}
	else if (GET_CRAFT_MIN_LEVEL(type) > get_crafting_level(ch)) {
		msg_to_char(ch, "You need to have a crafting level of %d to %s that.\r\n", GET_CRAFT_MIN_LEVEL(type), gen_craft_data[GET_CRAFT_TYPE(type)].command);
	}
	else if (GET_CRAFT_REQUIRES_OBJ(type) != NOTHING && !(found_obj = has_required_obj_for_craft(ch, GET_CRAFT_REQUIRES_OBJ(type)))) {
		msg_to_char(ch, "You need %s to make that.\r\n", get_obj_name_by_proto(GET_CRAFT_REQUIRES_OBJ(type)));
	}
	else if (IS_SET(GET_CRAFT_FLAGS(type), CRAFT_LEARNED) && !has_learned_craft(ch, GET_CRAFT_VNUM(type))) {
		msg_to_char(ch, "You have not learned that recipe.\r\n");
	}
	else if (!CAN_SEE_IN_DARK_ROOM(ch, IN_ROOM(ch))) {
		msg_to_char(ch, "It's too dark to %s anything.\r\n", gen_craft_data[GET_CRAFT_TYPE(type)].command);
	}
	else if (!check_can_craft(ch, type)) {
		// sends its own messages
	}
	else if (CRAFT_FLAGGED(type, CRAFT_VEHICLE)) {
		// vehicles pass off at this point
		do_gen_craft_vehicle(ch, type);
	}
	else if (IS_CARRYING_N(ch) > CAN_CARRY_N(ch)) {
		msg_to_char(ch, "You can't %s anything while overburdened.\r\n", gen_craft_data[subcmd].command);
	}
	
	// regular craft
	else if (!has_resources(ch, GET_CRAFT_RESOURCES(type), can_use_room(ch, IN_ROOM(ch), GUESTS_ALLOWED), TRUE)) {
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
		if (IS_SET(GET_CRAFT_FLAGS(type), CRAFT_POTTERY) && room_has_function_and_city_ok(IN_ROOM(ch), FNC_POTTER) && IS_COMPLETE(IN_ROOM(ch))) {
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
		}
		
		// must call this after start_action() because it stores resources
		extract_resources(ch, GET_CRAFT_RESOURCES(type), can_use_room(ch, IN_ROOM(ch), GUESTS_ALLOWED), &GET_ACTION_RESOURCES(ch));
		
		msg_to_char(ch, "You start %s.\r\n", gen_craft_data[GET_CRAFT_TYPE(type)].verb);
		sprintf(buf, "$n starts %s.", gen_craft_data[GET_CRAFT_TYPE(type)].verb);
		act(buf, FALSE, ch, 0, 0, TO_ROOM);
	}
}


ACMD(do_learn) {
	void add_learned_craft(char_data *ch, any_vnum vnum);
	extern bool has_learned_craft(char_data *ch, any_vnum vnum);
	
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
	else if (!(obj = get_obj_in_list_vis(ch, arg, ch->carrying))) {
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
		act("You commit $p to memory.", FALSE, ch, obj, NULL, TO_CHAR);
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
	else if (!(obj = get_obj_in_list_vis(ch, arg, ch->carrying))) {
		msg_to_char(ch, "You don't seem to have a %s in your inventory.\r\n", arg);
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
				else if (res->type == RES_COMPONENT && res->vnum == GET_OBJ_CMP_TYPE(obj) && (res->misc & GET_OBJ_CMP_FLAGS(obj)) == res->misc) {
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
				else if (res->type == RES_COMPONENT && res->vnum == GET_OBJ_CMP_TYPE(obj) && (res->misc & GET_OBJ_CMP_FLAGS(obj)) == res->misc) {
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
	extern char *shared_by(obj_data *obj, char_data *ch);
	extern const char *item_types[];
	
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
	else if (!(obj = get_obj_in_list_vis(ch, arg, ch->carrying)) && !(obj = get_obj_by_char_share(ch, arg))) {
		msg_to_char(ch, "You don't seem to have a %s.\r\n", arg);
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
		else if (!has_resources(ch, res, can_use_room(ch, IN_ROOM(ch), GUESTS_ALLOWED), TRUE)) {
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
			if (!proto || GET_OBJ_KEYWORDS(obj) != GET_OBJ_KEYWORDS(proto)) {
				free(GET_OBJ_KEYWORDS(obj));
			}
			GET_OBJ_KEYWORDS(obj) = str_dup(temp);
			
			// rename short desc
			if (!proto || GET_OBJ_SHORT_DESC(obj) != GET_OBJ_SHORT_DESC(proto)) {
				free(GET_OBJ_SHORT_DESC(obj));
			}
			GET_OBJ_SHORT_DESC(obj) = str_dup(argument);
			
			// rename long desc
			sprintf(temp, "%s is lying here.", argument);
			CAP(temp);
			if (!proto || GET_OBJ_LONG_DESC(obj) != GET_OBJ_LONG_DESC(proto)) {
				free(GET_OBJ_LONG_DESC(obj));
			}
			GET_OBJ_LONG_DESC(obj) = str_dup(temp);
			
			// message
			act(buf1, FALSE, ch, obj, obj->worn_by, TO_CHAR);
			act(buf2, TRUE, ch, obj, obj->worn_by, TO_ROOM);
			
			if (reforge_data[subcmd].ability != NOTHING) {
				gain_ability_exp(ch, reforge_data[subcmd].ability, 50);
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
		else if (!has_resources(ch, res, can_use_room(ch, IN_ROOM(ch), GUESTS_ALLOWED), TRUE)) {
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
			
			sprintf(buf, "$n %s $p%s!", reforge_data[subcmd].command, shared_by(obj, ch));
			act(buf, TRUE, ch, obj, obj->worn_by, TO_ROOM);
			
			if (reforge_data[subcmd].ability != NO_ABIL) {
				gain_ability_exp(ch, reforge_data[subcmd].ability, 50);
			}

			load_otrigger(obj);
		}
	}
	else if (is_abbrev(arg2, "superior")) {
		// calculate gem cost based on the gear rating of the item
		add_to_resource_list(&res, RES_OBJECT, o_GLOWING_SEASHELL, MAX(1, rate_item(obj) / 3), 0);
		proto = obj_proto(GET_OBJ_VNUM(obj));
		
		if (OBJ_FLAGGED(obj, OBJ_SUPERIOR)) {
			msg_to_char(ch, "It is already superior.\r\n");
		}
		else if (OBJ_FLAGGED(obj, OBJ_HARD_DROP | OBJ_GROUP_DROP)) {
			msg_to_char(ch, "You cannot make an item superior if it came from a Hard, Group, or Boss mob.\r\n");
		}
		else if (!proto || !OBJ_FLAGGED(proto, OBJ_SCALABLE) || !(ctype = find_craft_for_obj_vnum(GET_OBJ_VNUM(obj)))) {
			msg_to_char(ch, "It can't be made superior.\r\n");
		}
		else if (!(cft_abil = find_ability_by_vnum(GET_CRAFT_ABILITY(ctype))) || ABIL_MASTERY_ABIL(cft_abil) == NOTHING || !has_ability(ch, ABIL_MASTERY_ABIL(cft_abil))) {
			msg_to_char(ch, "You don't have the mastery to make that item superior.\r\n");
		}
		else if (!has_resources(ch, res, can_use_room(ch, IN_ROOM(ch), GUESTS_ALLOWED), TRUE)) {
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
			
			// set superior
			SET_BIT(GET_OBJ_EXTRA(new), OBJ_SUPERIOR);
			
			// copy over strings?
			if (GET_OBJ_KEYWORDS(obj) != GET_OBJ_KEYWORDS(proto)) {
				GET_OBJ_KEYWORDS(new) = GET_OBJ_KEYWORDS(obj) ? str_dup(GET_OBJ_KEYWORDS(obj)) : NULL;
			}
			if (GET_OBJ_SHORT_DESC(obj) != GET_OBJ_SHORT_DESC(proto)) {
				GET_OBJ_SHORT_DESC(new) = GET_OBJ_SHORT_DESC(obj) ? str_dup(GET_OBJ_SHORT_DESC(obj)) : NULL;
			}
			if (GET_OBJ_LONG_DESC(obj) != GET_OBJ_LONG_DESC(proto)) {
				GET_OBJ_LONG_DESC(new) = GET_OBJ_LONG_DESC(obj) ? str_dup(GET_OBJ_LONG_DESC(obj)) : NULL;
			}
			if (GET_OBJ_ACTION_DESC(obj) != GET_OBJ_ACTION_DESC(proto)) {
				GET_OBJ_ACTION_DESC(new) = GET_OBJ_ACTION_DESC(obj) ? str_dup(GET_OBJ_ACTION_DESC(obj)) : NULL;
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
			
			sprintf(buf, "$n %s $p%s into a masterwork!", reforge_data[subcmd].command, shared_by(obj, ch));
			act(buf, TRUE, ch, obj, obj->worn_by, TO_ROOM);

			if (reforge_data[subcmd].ability != NOTHING) {
				gain_ability_exp(ch, reforge_data[subcmd].ability, 50);
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

	if (!has_player_tech(ch, PTECH_TAME)) {
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
	else if (!(mob = get_char_vis(ch, arg, FIND_CHAR_ROOM))) {
		send_config_msg(ch, "no_person");
	}
	else if (!IS_NPC(mob) || !has_interaction(mob->interactions, INTERACT_TAME)) {
		act("You can't tame $N!", FALSE, ch, 0, mob, TO_CHAR);
	}
	else if (GET_LED_BY(mob)) {
		act("You can't tame $M right now.", FALSE, ch, NULL, mob, TO_CHAR);
	}
	else {
		act("You try to tame $N...", FALSE, ch, NULL, mob, TO_CHAR);
		act("$n tries to tame you...", FALSE, ch, NULL, mob, TO_VICT);
		act("$n tries to tame $N...", FALSE, ch, NULL, mob, TO_NOTVICT);
		
		any = run_interactions(ch, mob->interactions, INTERACT_TAME, IN_ROOM(ch), mob, NULL, tame_interact);
		
		if (any) {
			gain_player_tech_exp(ch, PTECH_TAME, 50);
			add_cooldown(ch, COOLDOWN_TAME, 3 * SECS_PER_REAL_MIN);
			
			// remove the original
			extract_char(mob);
		}
		else {
			act("You can't seem to tame $M!", FALSE, ch, NULL, mob, TO_CHAR);
		}
		
		command_lag(ch, WAIT_OTHER);
	}
}
