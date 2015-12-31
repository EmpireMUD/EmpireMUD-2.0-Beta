/* ************************************************************************
*   File: act.trade.c                                     EmpireMUD 2.0b3 *
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

// external functions
ACMD(do_gen_craft);
extern double get_enchant_scale_for_char(char_data *ch, int max_scale);
extern bool has_cooking_fire(char_data *ch);
extern obj_data *has_sharp_tool(char_data *ch);
void scale_item_to_level(obj_data *obj, int level);
extern bool validate_augment_target(char_data *ch, obj_data *obj, augment_data *aug, bool send_messages);

// locals
bool can_refashion(char_data *ch);
craft_data *find_craft_for_obj_vnum(obj_vnum vnum);
obj_data *has_hammer(char_data *ch);
void scale_craftable(obj_data *obj, char_data *ch, craft_data *craft);


 //////////////////////////////////////////////////////////////////////////////
//// HELPERS /////////////////////////////////////////////////////////////////

/**
* This determines if the character is in a place where they can forge, and that
* they are wielding a hammer. If not, this function sends an error message.
*
* @param char_data *ch The person trying to forge.
* @return bool If the character can forge TRUE, otherwise FALSE.
*/
bool can_forge(char_data *ch) {
	bool ok = FALSE;
	
	if (!IS_IMMORTAL(ch) && (!ROOM_BLD_FLAGGED(IN_ROOM(ch), BLD_FORGE) || !IS_COMPLETE(IN_ROOM(ch)))) {
		msg_to_char(ch, "You need to be in a forge to do that.\r\n");
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


 //////////////////////////////////////////////////////////////////////////////
//// GENERIC CRAFT (craft, forge, sew, cook) /////////////////////////////////

#define SOUP_TIMER  336  // hours


// for gen_craft_data[].strings
#define GCD_STRING_TO_CHAR  0
#define GCD_STRING_TO_ROOM  1


// CRAFT_TYPE_x
struct gen_craft_data_t gen_craft_data[] = {
	{ "error", "erroring", { "", "" } },	// dummy to require scmd
	
	// Note: These correspond to CRAFT_TYPE_x so you cannot change the order.
	{ "forge", "forging", { "You hit the %s on the anvil hard with $p!", "$n hits the %s on the anvil hard with $p!" } },
	{ "craft", "crafting", { "You continue crafting the %s...", "$n continues crafting the %s..." } },
	{ "cook", "cooking", { "You continue cooking the %s...", "$n continues cooking the %s..." } },
	{ "sew", "sewing", { "You carefully sew the %s...", "$n carefully sews the %s..." } },
	{ "mill", "milling", { "You grind the millstone, making %s...", "$n grinds the millstone, making %s..." } },
	{ "brew", "brewing", { "You stir the potion and infuse it with mana...", "$n stirs the potion..." } },
	{ "mix", "mixing", { "The poison bubbles as you stir it...", "$n stirs the bubbling poison..." } },
	
	// build is special and doesn't use do_gen_craft, so doesn't really use this data
	{ "build", "building", { "You work on the building...", "$n works on the building..." } },
	
	{ "weave", "weaving", { "You carefully weave the %s...", "$n carefully weaves the %s..." } },
	{ "workforce", "producing", { "You work on the %s...", "$n work on the %s..." } }	// not used by players
};


void cancel_gen_craft(char_data *ch) {
	craft_data *type = craft_proto(GET_ACTION_VNUM(ch, 0));
	obj_data *obj;
	
	if (type) {
		give_resources(ch, GET_CRAFT_RESOURCES(type), FALSE);

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
		if (!IS_SET(GET_CRAFT_FLAGS(iter), CRAFT_SOUP | CRAFT_VEHICLE) && GET_CRAFT_TYPE(iter) != CRAFT_BUILD && GET_CRAFT_OBJECT(iter) == vnum) {
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

	// basic sanity checking
	if (!type) {
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
		scale_craftable(obj, ch, type);
		
		load_otrigger(obj);
	}
	else if (GET_CRAFT_QUANTITY(type) > 0) {
		// NON-SOUP (careful, soup uses quantity for maximum contents
	
		amt = GET_CRAFT_QUANTITY(type);
	
		if (GET_CRAFT_TYPE(type) == CRAFT_TYPE_MILL && has_ability(ch, ABIL_MASTER_FARMER)) {
			gain_ability_exp(ch, ABIL_MASTER_FARMER, 10);
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
				scale_craftable(obj, ch, type);
	
				// where to put it
				if (CAN_WEAR(obj, ITEM_WEAR_TAKE)) {
					obj_to_char(obj, ch);
				}
				else {
					obj_to_room(obj, IN_ROOM(ch));
				}
				
				load_otrigger(obj);
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
	else {
		if (get_skill_level(ch, SKILL_TRADE) < EMPIRE_CHORE_SKILL_CAP) {
			gain_skill_exp(ch, SKILL_TRADE, 33.4);
		}
	}

	// master?
	if (is_master && applied_master) {
		gain_ability_exp(ch, ABIL_MASTERY_ABIL(cft_abil), 33.4);
	}

	// repeat as desired
	if (num > 1) {
		sprintf(lbuf, "%d %s", num-1, GET_CRAFT_NAME(type));
		do_gen_craft(ch, lbuf, 0, GET_CRAFT_TYPE(type));
	}
}


void process_gen_craft(char_data *ch) {
	obj_data *weapon = NULL;
	craft_data *type = craft_proto(GET_ACTION_VNUM(ch, 0));
	
	if (!type) {
		cancel_gen_craft(ch);
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
	else {
		GET_ACTION_TIMER(ch) -= 1;

		// bonus point for superior tool -- if this craft requires a tool
		if (weapon && OBJ_FLAGGED(weapon, OBJ_SUPERIOR)) {
			GET_ACTION_TIMER(ch) -= 1;
		}
		
		// tailor bonus for weave
		if (GET_CRAFT_TYPE(type) == CRAFT_TYPE_WEAVE && ROOM_BLD_FLAGGED(IN_ROOM(ch), BLD_TAILOR) && IS_COMPLETE(IN_ROOM(ch))) {
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


/**
* This function attempts to scale a crafted item. It will ignore non-scalables,
* so any crafted item can be passed through this.
*
* @param obj_data *obj The item to scale.
* @param char_data *ch The person who crafted it (for skill levels).
* @param craft_data *craft The craft recipe (used to get requires-object info).
*/
void scale_craftable(obj_data *obj, char_data *ch, craft_data *craft) {
	int level = 1, psr;
	ability_data *abil;
	obj_data *req;
	
	if (IS_NPC(ch)) {
		return;
	}
	
	// determine ideal scale level
	if (craft) {
		if (GET_CRAFT_REQUIRES_OBJ(craft) != NOTHING && (req = obj_proto(GET_CRAFT_REQUIRES_OBJ(craft)))) {
			level = get_crafting_level(ch);
			
			// check bounds on the required object
			if (GET_OBJ_MAX_SCALE_LEVEL(req) > 0) {
				level = MIN(level, GET_OBJ_MAX_SCALE_LEVEL(req));
			}
			if (GET_OBJ_MIN_SCALE_LEVEL(req) > 0) {
				level = MAX(level, GET_OBJ_MIN_SCALE_LEVEL(req));
			}
		}
		else {
			if (!(abil = find_ability_by_vnum(GET_CRAFT_ABILITY(craft)))) {
				level = EMPIRE_CHORE_SKILL_CAP;	// considered the "default" level for unskilled things
			}
			else if (!ABIL_ASSIGNED_SKILL(abil)) {
				// probably a class skill
				level = get_crafting_level(ch);
			}
			else if ((psr = ABIL_SKILL_LEVEL(abil)) != NOTHING) {
				if (psr < BASIC_SKILL_CAP) {
					level = MIN(BASIC_SKILL_CAP, get_skill_level(ch, SKILL_VNUM(ABIL_ASSIGNED_SKILL(abil))));
				}
				else if (psr < SPECIALTY_SKILL_CAP) {
					level = MIN(SPECIALTY_SKILL_CAP, get_skill_level(ch, SKILL_VNUM(ABIL_ASSIGNED_SKILL(abil))));
				}
				else {
					level = MIN(CLASS_SKILL_CAP, get_skill_level(ch, SKILL_VNUM(ABIL_ASSIGNED_SKILL(abil))));
				}
			}
			else {
				// this is probably unreachable
				level = get_crafting_level(ch);
			}
			
			// always bound by the crafting level
			level = MIN(level, get_crafting_level(ch));
		}
	}
	else {
		// no craft given
		level = get_crafting_level(ch);
	}
	
	// do it
	scale_item_to_level(obj, level);	
}


 //////////////////////////////////////////////////////////////////////////////
//// REFORGE / REFASHION /////////////////////////////////////////////////////

const struct {
	char *command;
	any_vnum ability;	// required ability
	bool (*validate_func)(char_data *ch);	// e.g. can_forge, func that returns TRUE if ok -- must send own errors if FALSE
	int types[3];	// NOTHING-terminated list of valid obj types
} reforge_data[] = {
	{ "reforge", ABIL_REFORGE, can_forge, { ITEM_WEAPON, ITEM_MISSILE_WEAPON, NOTHING } },	// SCMD_REFORGE
	{ "refashion", ABIL_REFASHION, can_refashion, { ITEM_ARMOR, ITEM_SHIELD, NOTHING } }	// SCMD_REFASHION
};


/**
* @param char_data *ch The person trying to refashion.
* @return bool TRUE if they can, false if they can't.
*/
bool can_refashion(char_data *ch) {
	bool ok = FALSE;
	
	if (!IS_IMMORTAL(ch) && !ROOM_BLD_FLAGGED(IN_ROOM(ch), BLD_TAILOR)) {
		msg_to_char(ch, "You need to be at the tailor to do that.\r\n");
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
	bool ok = FALSE, has_cap = FALSE;
	obj_data *proto;
	int iter;
	
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
		msg_to_char(ch, "Item names cannot contain the && or %% symbols.\r\n");
	}
	else if (!str_str(name, (char*)skip_filler(GET_OBJ_SHORT_DESC(obj)))) {
		msg_to_char(ch, "The new name must contain '%s'.\r\n", skip_filler(GET_OBJ_SHORT_DESC(obj)));
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
	extern const double apply_values[];
	
	char buf[MAX_STRING_LENGTH], target_arg[MAX_INPUT_LENGTH], *augment_arg;
	double points_available, remaining, share;
	struct obj_apply *apply, *last_apply;
	int scale, total_weight, value;
	struct augment_apply *app;
	ability_data *abil;
	augment_data *aug;
	obj_data *obj;
	
	augment_arg = one_argument(argument, target_arg);
	
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
		extract_resources(ch, GET_AUG_RESOURCES(aug), FALSE);
		
		// determine scale cap
		scale = GET_OBJ_CURRENT_SCALE_LEVEL(obj);
		if ((abil = find_ability_by_vnum(GET_AUG_ABILITY(aug))) && ABIL_ASSIGNED_SKILL(abil) != NULL && get_skill_level(ch, SKILL_VNUM(ABIL_ASSIGNED_SKILL(abil))) < CLASS_SKILL_CAP) {
			scale = MIN(scale, get_skill_level(ch, SKILL_VNUM(ABIL_ASSIGNED_SKILL(abil))));
		}
		
		// determine points
		points_available = get_enchant_scale_for_char(ch, scale);
		if (augment_info[subcmd].greater_abil && has_ability(ch, augment_info[subcmd].greater_abil)) {
			points_available *= config_get_double("greater_enchantments_bonus");
		}
		
		// figure out how many total weight points are used
		total_weight = 0;
		for (app = GET_AUG_APPLIES(aug); app; app = app->next) {
			total_weight += app->weight;
		}
		
		// find end of current applies on obj
		if ((last_apply = GET_OBJ_APPLIES(obj))) {
			while (last_apply->next) {
				last_apply = last_apply->next;
			}
		}
		
		// start adding applies
		remaining = points_available;
		for (app = GET_AUG_APPLIES(aug); app && remaining > 0; app = app->next) {
			share = (((double)app->weight) / total_weight) * points_available;	// % of total
			share = MIN(share, remaining);	// check limit
			value = round(share * (1.0 / apply_values[app->location]));
			if (value > 0 || (app == GET_AUG_APPLIES(aug))) {	// always give at least 1 point on the first one
				value = MAX(1, value);
				remaining -= (value * apply_values[app->location]);	// subtract actual amount used
				
				// create the actual apply
				CREATE(apply, struct obj_apply, 1);
				apply->apply_type = augment_info[subcmd].apply_type;
				apply->location = app->location;
				apply->modifier = value;
				
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
		if (augment_info[subcmd].greater_abil != NO_ABIL) {
			gain_ability_exp(ch, augment_info[subcmd].greater_abil, 50);
		}
		
		command_lag(ch, WAIT_ABILITY);
	}
}


// subcmd must be CRAFT_TYPE_x
ACMD(do_gen_craft) {	
	int timer, num = 1;
	bool this_line, found;
	craft_data *craft, *next_craft, *type = NULL, *abbrev_match = NULL;
	bool is_master, wait, room_wait;
	obj_data *drinkcon = NULL;
	ability_data *cft_abil;

	if (IS_NPC(ch)) {
		msg_to_char(ch, "NPCs can't craft.\r\n");
		return;
	}

	skip_spaces(&argument);

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
			if (GET_CRAFT_TYPE(craft) == subcmd) {
				if (!IS_SET(GET_CRAFT_FLAGS(craft), CRAFT_IN_DEVELOPMENT) || IS_IMMORTAL(ch)) {
					if (GET_CRAFT_ABILITY(craft) == NO_ABIL || has_ability(ch, GET_CRAFT_ABILITY(craft))) {
						if (GET_CRAFT_REQUIRES_OBJ(craft) == NOTHING || get_obj_in_list_vnum(GET_CRAFT_REQUIRES_OBJ(craft), ch->carrying)) {
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
					}
				}
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
	else if (!*arg || !type) {
		// master craft list
		msg_to_char(ch, "What would you like to %s? You know how to make:\r\n", gen_craft_data[subcmd].command);
		this_line = FALSE;
		found = FALSE;
		*buf = '\0';
		
		HASH_ITER(sorted_hh, sorted_crafts, craft, next_craft) {
			if (GET_CRAFT_TYPE(craft) == subcmd && (!IS_SET(GET_CRAFT_FLAGS(craft), CRAFT_IN_DEVELOPMENT) || IS_IMMORTAL(ch)) && (GET_CRAFT_ABILITY(craft) == NO_ABIL || has_ability(ch, GET_CRAFT_ABILITY(craft)))) {
				if (GET_CRAFT_REQUIRES_OBJ(craft) == NOTHING || get_obj_in_list_vnum(GET_CRAFT_REQUIRES_OBJ(craft), ch->carrying)) {
					if (strlen(buf) + strlen(GET_CRAFT_NAME(craft)) + 2 >= 80) {
						this_line = FALSE;
						msg_to_char(ch, "%s\r\n", buf);
						*buf = '\0';
					}
					sprintf(buf + strlen(buf), "%s%s", (this_line ? ", " : " "), GET_CRAFT_NAME(craft));
					this_line = TRUE;
					found = TRUE;
				}
			}
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

	// type checks
	else if (IS_SET(GET_CRAFT_FLAGS(type), CRAFT_IN_CITY_ONLY) && !is_in_city_for_empire(IN_ROOM(ch), GET_LOYALTY(ch), TRUE, &wait) && !is_in_city_for_empire(IN_ROOM(ch), ROOM_OWNER(IN_ROOM(ch)), TRUE, &room_wait)) {
		msg_to_char(ch, "You can only make that in a city%s.\r\n", (wait || room_wait) ? " (this city was founded too recently)" : "");
	}
	else if (GET_CRAFT_TYPE(type) == CRAFT_TYPE_MILL && (!ROOM_BLD_FLAGGED(IN_ROOM(ch), BLD_MILL) || !IS_COMPLETE(IN_ROOM(ch)))) {
		msg_to_char(ch, "You need to be in a mill to do that.\r\n");
	}
	else if (GET_CRAFT_TYPE(type) == CRAFT_TYPE_FORGE && !can_forge(ch)) {
		// sends its own message
	}

	// flag checks
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
	else if (IS_SET(GET_CRAFT_FLAGS(type), CRAFT_CARPENTER) && (BUILDING_VNUM(IN_ROOM(ch)) != BUILDING_CARPENTER || !IS_COMPLETE(IN_ROOM(ch)))) {
		msg_to_char(ch, "You need to %s that at the carpenter!\r\n", gen_craft_data[GET_CRAFT_TYPE(type)].command);
	}
	else if (IS_SET(GET_CRAFT_FLAGS(type), CRAFT_GLASSBLOWER) && (!ROOM_BLD_FLAGGED(IN_ROOM(ch), BLD_GLASSBLOWER) || !IS_COMPLETE(IN_ROOM(ch)))) {
		msg_to_char(ch, "You need to %s that at the glassblower!\r\n", gen_craft_data[GET_CRAFT_TYPE(type)].command);
	}
	else if (IS_SET(GET_CRAFT_FLAGS(type), CRAFT_SOUP) && !(drinkcon = find_water_container(ch, ch->carrying)) && !(drinkcon = find_water_container(ch, ROOM_CONTENTS(IN_ROOM(ch))))) {
		msg_to_char(ch, "You need a container of water to %s that.\r\n", gen_craft_data[GET_CRAFT_TYPE(type)].command);
	}
	else if (IS_SET(GET_CRAFT_FLAGS(type), CRAFT_ALCHEMY) && !ROOM_BLD_FLAGGED(IN_ROOM(ch), BLD_ALCHEMIST) && !has_tech_available(ch, TECH_GLASSBLOWING)) {
		// sends its own messages -- needs glassblowing unless in alchemist room
	}
	
	// end flag checks

	else if (GET_CRAFT_REQUIRES_OBJ(type) != NOTHING && !get_obj_in_list_vnum(GET_CRAFT_REQUIRES_OBJ(type), ch->carrying)) {
		msg_to_char(ch, "You need %s to make that.\r\n", get_obj_name_by_proto(GET_CRAFT_REQUIRES_OBJ(type)));
	}
	else if (!has_resources(ch, GET_CRAFT_RESOURCES(type), can_use_room(ch, IN_ROOM(ch), GUESTS_ALLOWED), TRUE)) {
		// this sends its own message ("You need X more of ...")
		//msg_to_char(ch, "You don't have the resources to %s that.\r\n", gen_craft_data[GET_CRAFT_TYPE(type)].command);
	}
	else {
		cft_abil = find_ability_by_vnum(GET_CRAFT_ABILITY(type));
		is_master = (cft_abil && ABIL_MASTERY_ABIL(cft_abil) != NOTHING && has_ability(ch, ABIL_MASTERY_ABIL(cft_abil)));
		
		// base timer
		timer = GET_CRAFT_TIME(type);

		// potter building bonus	
		if (IS_SET(GET_CRAFT_FLAGS(type), CRAFT_POTTERY) && ROOM_BLD_FLAGGED(IN_ROOM(ch), BLD_POTTER) && IS_COMPLETE(IN_ROOM(ch))) {
			timer /= 4;
		}
		
		// mastery
		if (is_master) {
			timer /= 2;
		}
	
		start_action(ch, ACT_GEN_CRAFT, timer);
		GET_ACTION_VNUM(ch, 0) = GET_CRAFT_VNUM(type);
		
		if (drinkcon) {
			GET_ACTION_VNUM(ch, 1) = GET_OBJ_VNUM(drinkcon);
			extract_obj(drinkcon);
		}
		
		// how many
		GET_ACTION_VNUM(ch, 2) = num;
		
		extract_resources(ch, GET_CRAFT_RESOURCES(type), can_use_room(ch, IN_ROOM(ch), GUESTS_ALLOWED));
		
		msg_to_char(ch, "You start %s.\r\n", gen_craft_data[GET_CRAFT_TYPE(type)].verb);
		sprintf(buf, "$n starts %s.", gen_craft_data[GET_CRAFT_TYPE(type)].verb);
		act(buf, FALSE, ch, 0, 0, TO_ROOM);
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
			
			if (GET_CRAFT_REQUIRES_OBJ(craft) != NOTHING && !get_obj_in_list_vnum(GET_CRAFT_REQUIRES_OBJ(craft), ch->carrying)) {
				continue;
			}
			
			// is the item used to make it?
			uses_item = FALSE;
			if (GET_CRAFT_REQUIRES_OBJ(craft) == GET_OBJ_VNUM(obj)) {
				uses_item = TRUE;
			}
			for (res = GET_CRAFT_RESOURCES(craft); !uses_item && res; res = res->next) {
				if (res->vnum == GET_OBJ_VNUM(obj)) {
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
			if (GET_AUG_REQUIRES_OBJ(aug) == GET_OBJ_VNUM(obj)) {
				uses_item = TRUE;
			}
			for (res = GET_AUG_RESOURCES(aug); !uses_item && res; res = res->next) {
				if (res->vnum == GET_OBJ_VNUM(obj)) {
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


// this handles both 'reforge' and 'refashion'
ACMD(do_reforge) {
	extern char *shared_by(obj_data *obj, char_data *ch);
	extern const char *item_types[];
	
	char arg2[MAX_INPUT_LENGTH], temp[MAX_INPUT_LENGTH];
	struct resource_data *res = NULL;
	time_t old_stolen_time;
	ability_data *cft_abil;
	craft_data *ctype;
	int old_timer, iter, level;
	bool found;
	obj_data *obj, *new, *proto;
	
	bitvector_t preserve_flags = OBJ_HARD_DROP | OBJ_GROUP_DROP;	// flags to copy over if obj is reloaded
	
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
		res = create_resource_list(o_IRIDESCENT_IRIS, MAX(1, rate_item(obj) / 3), NOTHING);
		
		if (!validate_item_rename(ch, obj, argument)) {
			// sends own message
		}
		else if (!has_resources(ch, res, can_use_room(ch, IN_ROOM(ch), GUESTS_ALLOWED), TRUE)) {
			// sends own message
		}
		else {
			extract_resources(ch, res, can_use_room(ch, IN_ROOM(ch), GUESTS_ALLOWED));
			
			// prepare
			sprintf(buf1, "You name %s%s $p!", GET_OBJ_SHORT_DESC(obj), shared_by(obj, ch));
			sprintf(buf2, "$n names %s%s $p!", GET_OBJ_SHORT_DESC(obj), shared_by(obj, ch));
			
			proto = obj_proto(GET_OBJ_VNUM(obj));
			
			// rename keywords
			if (!proto || GET_OBJ_KEYWORDS(obj) != GET_OBJ_KEYWORDS(proto)) {
				free(GET_OBJ_KEYWORDS(obj));
			}
			GET_OBJ_KEYWORDS(obj) = str_dup(skip_filler(argument));
			
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
		res = create_resource_list(o_BLOODSTONE, MAX(1, (proto ? rate_item(proto) : rate_item(obj)) / 3), NOTHING);
		
		if (!proto) {
			msg_to_char(ch, "You can't renew that.\r\n");
		}
		else if (!has_resources(ch, res, can_use_room(ch, IN_ROOM(ch), GUESTS_ALLOWED), TRUE)) {
			// sends own message
		}
		else {
			extract_resources(ch, res, can_use_room(ch, IN_ROOM(ch), GUESTS_ALLOWED));
			
			// actual reforging: just junk it and load a new one
			old_stolen_time = obj->stolen_timer;
			old_timer = GET_OBJ_TIMER(obj);
			level = GET_OBJ_CURRENT_SCALE_LEVEL(obj);
			
			new = read_object(GET_OBJ_VNUM(proto), TRUE);
			GET_OBJ_EXTRA(new) |= GET_OBJ_EXTRA(obj) & preserve_flags;
			
			// transfer bindings
			OBJ_BOUND_TO(new) = OBJ_BOUND_TO(obj);
			OBJ_BOUND_TO(obj) = NULL;
			
			// re-apply
			new->stolen_timer = old_stolen_time;
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
		res = create_resource_list(o_GLOWING_SEASHELL, MAX(1, rate_item(obj) / 3), NOTHING);
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
		else if (!has_resources(ch, res, can_use_room(ch, IN_ROOM(ch), GUESTS_ALLOWED), TRUE)) {
			// sends own message
		}
		else {
			extract_resources(ch, res, can_use_room(ch, IN_ROOM(ch), GUESTS_ALLOWED));
			
			// actual reforging: just junk it and load a new one
			old_stolen_time = obj->stolen_timer;
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
