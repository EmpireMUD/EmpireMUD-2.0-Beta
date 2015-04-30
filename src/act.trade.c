/* ************************************************************************
*   File: act.trade.c                                     EmpireMUD 2.0b1 *
*  Usage: code related to crafting and the trade skill                    *
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
*   Helpers
*   Generic Craft (craft, forge, sew, cook)
*   Reforge / Refashion
*   Weave
*   Commands
*/

// external vars
extern int last_action_rotation;

// external functions
ACMD(do_gen_craft);
extern bool has_cooking_fire(char_data *ch);
extern obj_data *has_sharp_tool(char_data *ch);
void scale_item_to_level(obj_data *obj, int level);

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
* This finds a hammer in either tool slot, and returns it.
*
* @param char_data *ch The person using the hammer?
* @return obj_data *The hammer object.
*/
obj_data *has_hammer(char_data *ch) {
	obj_data *hammer = NULL;
	
	if (!(hammer = GET_EQ(ch, WEAR_WIELD)) || !IS_WEAPON(hammer) || GET_WEAPON_TYPE(hammer) != TYPE_HAMMER) {
		if (!(hammer = GET_EQ(ch, WEAR_HOLD)) || !IS_WEAPON(hammer) || GET_WEAPON_TYPE(hammer) != TYPE_HAMMER) {
			msg_to_char(ch, "You need to use a hammer to do that.\r\n");
			hammer = NULL;
		}
	}
	
	return hammer;
}


 //////////////////////////////////////////////////////////////////////////////
//// GENERIC CRAFT (craft, forge, sew, cook) /////////////////////////////////

#define SOUP_TIMER  336  // hours


// for gen_craft_data[].strings
#define GCD_STRING_TO_CHAR  0
#define GCD_STRING_TO_ROOM  1


// CRAFT_TYPE_x
struct {
	char *command;
	char *verb;
	char *strings[2];
} gen_craft_data[] = {
	{ "error", "erroring", { "", "" } },	// dummy to require scmd
	
	{ "forge", "forging", { "You hit the %s on the anvil hard with $p!", "$n hits the %s on the anvil hard with $p!" } },
	{ "craft", "crafting", { "You continue crafting the %s...", "$n continues crafting the %s..." } },
	{ "cook", "cooking", { "You continue cooking the %s...", "$n continues cooking the %s..." } },
	{ "sew", "sewing", { "You carefully sew the %s...", "$n carefully sews the %s..." } },
	{ "mill", "milling", { "You grind the millstone, making %s...", "$n grinds the millstone, making %s..." } },
	{ "brew", "brewing", { "You stir the potion and infuse it with mana...", "$n stirs the potion..." } },
	{ "mix", "mixing", { "The poison bubbles as you stir it...", "$n stirs the bubbling poison..." } },
	
	// build is special and doesn't use do_gen_craft, so doesn't really use this data
	{ "build", "building", { "You work on the building...", "$n works on the building..." } }
};


// related mastery skills
struct {
	int base_ability;
	int mastery_ability;
} related_mastery_data[] = {
	// smith block
	{ ABIL_FORGE, ABIL_MASTER_BLACKSMITH },
	{ ABIL_WEAPONCRAFTING, ABIL_MASTER_BLACKSMITH },
	{ ABIL_IRON_BLADES, ABIL_MASTER_BLACKSMITH },
	{ ABIL_DEADLY_WEAPONS, ABIL_MASTER_BLACKSMITH },
	{ ABIL_ARMORSMITHING, ABIL_MASTER_BLACKSMITH },
	{ ABIL_STURDY_ARMORS, ABIL_MASTER_BLACKSMITH },
	{ ABIL_IMPERIAL_ARMORS, ABIL_MASTER_BLACKSMITH },
	{ ABIL_JEWELRY, ABIL_MASTER_BLACKSMITH },
	
	// brew block
	{ ABIL_HEALING_ELIXIRS, ABIL_WRATH_OF_NATURE_POTIONS },
	{ ABIL_WRATH_OF_NATURE_POTIONS, ABIL_HEALING_ELIXIRS },
	
	// craft block
//	{ ABIL_BASIC_CRAFTS, ABIL_MASTER_WOODWORKING },
//	{ ABIL_WOODWORKING, ABIL_MASTER_WOODWORKING },
//	{ ABIL_ADVANCED_WOODWORKING, ABIL_MASTER_WOODWORKING },
	
	// pottery block
	{ ABIL_POTTERY, ABIL_MASTER_POTTER },
	{ ABIL_FINE_POTTERY, ABIL_MASTER_POTTER },
	
	// sew block
	{ ABIL_SEWING, ABIL_MASTER_TAILOR },
	{ ABIL_RAWHIDE_STITCHING, ABIL_MASTER_TAILOR },
	{ ABIL_LEATHERWORKING, ABIL_MASTER_TAILOR },
	{ ABIL_DANGEROUS_LEATHERS, ABIL_MASTER_TAILOR },
	{ ABIL_MAGIC_ATTIRE, ABIL_MASTER_TAILOR },
	{ ABIL_MAGICAL_VESTMENTS, ABIL_MASTER_TAILOR },
	
	// this goes last
	{ NO_ABIL, NO_ABIL }
};


void cancel_gen_craft(char_data *ch) {
	craft_data *type = craft_proto(GET_ACTION_VNUM(ch, 0));
	obj_data *obj;
	
	if (type) {
		give_resources(ch, GET_CRAFT_RESOURCES(type), FALSE);

		// load the drink container back
		if (IS_SET(GET_CRAFT_FLAGS(type), CRAFT_SOUP)) {
			obj = read_object(GET_ACTION_VNUM(ch, 1));

			// just empty it
			GET_OBJ_VAL(obj, VAL_DRINK_CONTAINER_CONTENTS) = 0;
			obj_to_char_or_room(obj, ch);
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
		if (!IS_SET(GET_CRAFT_FLAGS(iter), CRAFT_SOUP) && GET_CRAFT_OBJECT(iter) == vnum) {
			return iter;
		}
	}
	
	return NULL;
}


/**
* find the mastery ability for any crafting ability using the
* related_mastery_data table
*
* @param int base_ability The ability being used.
* @return int The mastery ability, or NO_ABIL if it has none.
*/
int get_mastery_ability(int base_ability) {
	int iter, found = NO_ABIL;
	
	for (iter = 0; found == NO_ABIL && related_mastery_data[iter].base_ability != NO_ABIL; ++iter) {
		if (related_mastery_data[iter].base_ability == base_ability) {
			found = related_mastery_data[iter].mastery_ability;
		}
	}
	
	return found;
}


void finish_gen_craft(char_data *ch) {
	craft_data *type = craft_proto(GET_ACTION_VNUM(ch, 0));
	int num = GET_ACTION_VNUM(ch, 2);
	int master_ability = get_mastery_ability(GET_CRAFT_ABILITY(type));
	char lbuf[MAX_INPUT_LENGTH];
	bool applied_master = FALSE;
	obj_data *obj = NULL;
	int iter, amt = 1;
	
	GET_ACTION(ch) = ACT_NONE;

	// basic sanity checking
	if (!type) {
		return;
	}
	
	// soup handling
	if (IS_SET(GET_CRAFT_FLAGS(type), CRAFT_SOUP)) {
		// load the drink container back
		obj = read_object(GET_ACTION_VNUM(ch, 1));
	
		GET_OBJ_VAL(obj, VAL_DRINK_CONTAINER_CONTENTS) = MIN(GET_CRAFT_QUANTITY(type), GET_DRINK_CONTAINER_CAPACITY(obj));
		GET_OBJ_VAL(obj, VAL_DRINK_CONTAINER_TYPE) = GET_CRAFT_OBJECT(type);
	
		// set it to go bad... very bad
		GET_OBJ_TIMER(obj) = SOUP_TIMER;
		obj_to_char_or_room(obj, ch);
		scale_craftable(obj, ch, type);
		
		load_otrigger(obj);
	}
	else if (GET_CRAFT_QUANTITY(type) > 0) {
		// NON-SOUP (careful, soup uses quantity for maximum contents
	
		amt = GET_CRAFT_QUANTITY(type);
	
		if (GET_CRAFT_TYPE(type) == CRAFT_TYPE_MILL && HAS_ABILITY(ch, ABIL_MASTER_FARMER)) {
			gain_ability_exp(ch, ABIL_MASTER_FARMER, 10);
			amt *= 2;
		}

		if (obj_proto(GET_CRAFT_OBJECT(type))) {
			for (iter = 0; iter < amt; ++iter) {
				// load and master it
				obj = read_object(GET_CRAFT_OBJECT(type));
				if (OBJ_FLAGGED(obj, OBJ_SCALABLE) && master_ability != NO_ABIL && HAS_ABILITY(ch, master_ability)) {
					applied_master = TRUE;
					SET_BIT(GET_OBJ_EXTRA(obj), OBJ_SUPERIOR);
				}
				scale_craftable(obj, ch, type);
	
				// where to put it
				if (CAN_WEAR(obj, ITEM_WEAR_TAKE)) {
					obj_to_char_or_room(obj, ch);
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
		if (GET_SKILL(ch, SKILL_TRADE) < EMPIRE_CHORE_SKILL_CAP) {
			gain_skill_exp(ch, SKILL_TRADE, 33.4);
		}
	}

	// master?
	if (master_ability != NO_ABIL && applied_master) {
		gain_ability_exp(ch, master_ability, 33.4);
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
	obj_data *req;
	
	if (!OBJ_FLAGGED(obj, OBJ_SCALABLE) || IS_NPC(ch)) {
		return;
	}
	
	// determine ideal scale level
	if (craft) {
		if (GET_CRAFT_REQUIRES_OBJ(craft) != NOTHING && (req = obj_proto(GET_CRAFT_REQUIRES_OBJ(craft)))) {
			level = GET_COMPUTED_LEVEL(ch);
			
			// check bounds on the required object
			if (GET_OBJ_MAX_SCALE_LEVEL(req) > 0) {
				level = MIN(level, GET_OBJ_MAX_SCALE_LEVEL(req));
			}
			if (GET_OBJ_MIN_SCALE_LEVEL(req) > 0) {
				level = MAX(level, GET_OBJ_MIN_SCALE_LEVEL(req));
			}
		}
		else {
			if (GET_CRAFT_ABILITY(craft) == NO_ABIL) {
				level = EMPIRE_CHORE_SKILL_CAP;	// considered the "default" level for unskilled things
			}
			else if (ability_data[GET_CRAFT_ABILITY(craft)].parent_skill == NO_SKILL) {
				// probably a class skill
				level = GET_COMPUTED_LEVEL(ch);
			}
			else if ((psr = GET_PARENT_SKILL_REQUIRED(GET_CRAFT_ABILITY(craft))) != NOTHING) {
				if (psr < BASIC_SKILL_CAP) {
					level = MIN(BASIC_SKILL_CAP, GET_SKILL(ch, ability_data[GET_CRAFT_ABILITY(craft)].parent_skill));
				}
				else if (psr < SPECIALTY_SKILL_CAP) {
					level = MIN(SPECIALTY_SKILL_CAP, GET_SKILL(ch, ability_data[GET_CRAFT_ABILITY(craft)].parent_skill));
				}
				else {
					level = MIN(CLASS_SKILL_CAP, GET_SKILL(ch, ability_data[GET_CRAFT_ABILITY(craft)].parent_skill));
				}
			}
			else {
				// this is probably unreachable
				level = GET_COMPUTED_LEVEL(ch);
			}
			
			// always bound by the computed level
			level = MIN(level, GET_COMPUTED_LEVEL(ch));
		}
	}
	else {
		// no craft given
		level = GET_COMPUTED_LEVEL(ch);
	}
	
	// do it
	scale_item_to_level(obj, level);	
}


 //////////////////////////////////////////////////////////////////////////////
//// REFORGE / REFASHION /////////////////////////////////////////////////////

const struct {
	char *command;
	int ability;	// required ability
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
//// WEAVE ///////////////////////////////////////////////////////////////////

// TODO should this be in act.action.c instead?

#define WEAVE_LINE_BREAK { "\t", 0, 0, NO_ABIL, { END_RESOURCE_LIST } }

struct {
	char *name;
	int time;	// in action ticks
	obj_vnum vnum;
	int ability;	// NO_ABIL for none
	Resource resources[5];
} weave_data[] = {
	{ "cloth", 12, o_CLOTH, ABIL_SEWING, { { o_COTTON, 2 }, END_RESOURCE_LIST } },
	{ "wool cloth", 12, o_CLOTH, ABIL_SEWING, { { o_WOOL, 2 }, END_RESOURCE_LIST } },
	
	// last
	{ "\n", 0, 0, NO_ABIL, { END_RESOURCE_LIST } },
};


void cancel_weaving(char_data *ch) {
	if (GET_ACTION(ch) == ACT_WEAVING) {
		GET_ACTION(ch) = ACT_NONE;
		give_resources(ch, weave_data[GET_ACTION_VNUM(ch, 0)].resources, FALSE);
	}
}


void finish_weaving(char_data *ch) {
	int type = GET_ACTION_VNUM(ch, 0);
	obj_data *obj;

	GET_ACTION(ch) = ACT_NONE;

	obj = read_object(weave_data[type].vnum);
	
	obj_to_char_or_room(obj, ch);

	act("You finish weaving $p!", FALSE, ch, obj, 0, TO_CHAR);
	act("$n finishes weaving $p!", FALSE, ch, obj, 0, TO_ROOM);
	
	if (weave_data[type].ability != NO_ABIL) {
		gain_ability_exp(ch, weave_data[type].ability, 10);
	}
	else {
		if (GET_SKILL(ch, SKILL_TRADE) < EMPIRE_CHORE_SKILL_CAP) {
			gain_skill_exp(ch, SKILL_TRADE, 10);
		}
	}
	
	load_otrigger(obj);
}


 //////////////////////////////////////////////////////////////////////////////
//// COMMANDS ////////////////////////////////////////////////////////////////

// subcmd must be CRAFT_TYPE_x
ACMD(do_gen_craft) {	
	int timer, master_ability, num = 1;
	bool this_line, found;
	craft_data *craft, *next_craft, *type = NULL, *abbrev_match = NULL;
	obj_data *drinkcon = NULL;

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
					if (GET_CRAFT_ABILITY(craft) == NO_ABIL || HAS_ABILITY(ch, GET_CRAFT_ABILITY(craft))) {
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
			if (GET_CRAFT_TYPE(craft) == subcmd && (!IS_SET(GET_CRAFT_FLAGS(craft), CRAFT_IN_DEVELOPMENT) || IS_IMMORTAL(ch)) && (GET_CRAFT_ABILITY(craft) == NO_ABIL || HAS_ABILITY(ch, GET_CRAFT_ABILITY(craft)))) {
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
	else if (GET_CRAFT_ABILITY(type) != NO_ABIL && !HAS_ABILITY(ch, GET_CRAFT_ABILITY(type))) {
		msg_to_char(ch, "You need to buy the %s ability to %s that.\r\n", ability_data[GET_CRAFT_ABILITY(type)].name, gen_craft_data[GET_CRAFT_TYPE(type)].command);
	}
	else if (!can_use_room(ch, IN_ROOM(ch), GUESTS_ALLOWED)) {
		msg_to_char(ch, "You don't have permission to %s here.\r\n", gen_craft_data[GET_CRAFT_TYPE(type)].command);
	}

	// type checks
	else if (IS_SET(GET_CRAFT_FLAGS(type), CRAFT_IN_CITY_ONLY) && !IS_IN_CITY(ch)) {
		msg_to_char(ch, "You can only make that in a city.\r\n");
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
	else if (!has_resources(ch, GET_CRAFT_RESOURCES(type), TRUE, TRUE)) {
		// this sends its own message ("You need X more of ...")
		//msg_to_char(ch, "You don't have the resources to %s that.\r\n", gen_craft_data[GET_CRAFT_TYPE(type)].command);
	}
	else {
		// base timer
		master_ability = get_mastery_ability(GET_CRAFT_ABILITY(type));
		timer = GET_CRAFT_TIME(type);

		// potter building bonus	
		if (IS_SET(GET_CRAFT_FLAGS(type), CRAFT_POTTERY) && ROOM_BLD_FLAGGED(IN_ROOM(ch), BLD_POTTER) && IS_COMPLETE(IN_ROOM(ch))) {
			timer /= 4;
		}
		
		// mastery
		if (master_ability != NO_ABIL && HAS_ABILITY(ch, master_ability)) {
			timer /= 2;
		}
	
		start_action(ch, ACT_GEN_CRAFT, timer, 0);
		GET_ACTION_VNUM(ch, 0) = GET_CRAFT_VNUM(type);
		
		if (drinkcon) {
			GET_ACTION_VNUM(ch, 1) = GET_OBJ_VNUM(drinkcon);
			extract_obj(drinkcon);
		}
		
		// how many
		GET_ACTION_VNUM(ch, 2) = num;
		
		extract_resources(ch, GET_CRAFT_RESOURCES(type), TRUE);
		
		msg_to_char(ch, "You start %s.\r\n", gen_craft_data[GET_CRAFT_TYPE(type)].verb);
		sprintf(buf, "$n starts %s.", gen_craft_data[GET_CRAFT_TYPE(type)].verb);
		act(buf, FALSE, ch, 0, 0, TO_ROOM);
	}
}


ACMD(do_recipes) {
	int sub, last_type = NOTHING;
	craft_data *craft, *next_craft;
	obj_data *obj;
	bool found_any = FALSE, found_type = FALSE, found_one = FALSE;
	
	one_argument(argument, arg);
	
	if (!*arg) {
		msg_to_char(ch, "Show recipes for which item?\r\n");
	}
	else if (!(obj = get_obj_in_list_vis(ch, arg, ch->carrying))) {
		msg_to_char(ch, "You don't seem to have a %s in your inventory.\r\n", arg);
	}
	else {
		act("With $p, you can make:", FALSE, ch, obj, NULL, TO_CHAR);

		HASH_ITER(sorted_hh, sorted_crafts, craft, next_craft) {
			// is it a live recipe?
			if (IS_SET(GET_CRAFT_FLAGS(craft), CRAFT_IN_DEVELOPMENT) && !IS_IMMORTAL(ch)) {
				continue;
			}
		
			// has right abil?
			if (*GET_CRAFT_NAME(craft) == '\t' || (GET_CRAFT_ABILITY(craft) != NO_ABIL && !HAS_ABILITY(ch, GET_CRAFT_ABILITY(craft)))) {
				continue;
			}
			
			// is the item used to make it?
			for (sub = 0, found_one = FALSE; !found_one && GET_CRAFT_RESOURCES(craft)[sub].vnum != NOTHING; ++sub) {
				if (GET_CRAFT_RESOURCES(craft)[sub].vnum != GET_OBJ_VNUM(obj)) {
					continue;
				}
				
				if (GET_CRAFT_REQUIRES_OBJ(craft) != NOTHING && !get_obj_in_list_vnum(GET_CRAFT_REQUIRES_OBJ(craft), ch->carrying)) {
					continue;
				}
				
				// MATCH!
			
				// need header?
				if (GET_CRAFT_TYPE(craft) != last_type) {
					msg_to_char(ch, "%s %s: ", found_type ? "\r\n" : "", gen_craft_data[GET_CRAFT_TYPE(craft)].command);
	
					found_type = FALSE;
					last_type = GET_CRAFT_TYPE(craft);
				}
		
				msg_to_char(ch, "%s%s", (found_type ? ", " : ""), GET_CRAFT_NAME(craft));
				found_any = found_one = found_type = TRUE;
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
	extern const char *item_types[];
	
	Resource res[2] = { { NOTHING, 0 }, END_RESOURCE_LIST };	// this cost is set later
	char arg2[MAX_INPUT_LENGTH], temp[MAX_INPUT_LENGTH];
	time_t old_stolen_time;
	craft_data *ctype;
	int old_timer, iter, level;
	bool found;
	obj_data *obj, *new, *proto;
	
	// reforge <item> name <name>
	// reforge <item> renew
	
	argument = two_arguments(argument, arg, arg2);
	skip_spaces(&argument);
	
	if (IS_NPC(ch)) {
		msg_to_char(ch, "NPCs cannot reforge.\r\n");
	}
	else if (reforge_data[subcmd].ability != NOTHING && !HAS_ABILITY(ch, reforge_data[subcmd].ability)) {
		msg_to_char(ch, "You must buy the %s ability to do that.\r\n", reforge_data[subcmd].command);
	}
	else if (!*arg || !*arg2) {
		msg_to_char(ch, "Usage: %s <item> name <name>\r\n", reforge_data[subcmd].command);
		msg_to_char(ch, "       %s <item> <renew | superior>\r\n", reforge_data[subcmd].command);
	}
	else if (reforge_data[subcmd].validate_func && !(reforge_data[subcmd].validate_func)(ch)) {
		// failed validate func -- sends own messages
	}
	else if (!(obj = get_obj_in_list_vis(ch, arg, ch->carrying))) {
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
		// calculate gem cost based on the gear level of the item
		res[0].vnum = o_IRIDESCENT_IRIS;
		res[0].amount = MAX(1, rate_item(obj) / 3);
		
		if (!validate_item_rename(ch, obj, argument)) {
			// sends own message
		}
		else if (!has_resources(ch, res, TRUE, TRUE)) {
			// sends own message
		}
		else {
			extract_resources(ch, res, TRUE);
			
			// prepare
			sprintf(buf1, "You name %s $p!", GET_OBJ_SHORT_DESC(obj));
			sprintf(buf2, "$n names %s $p!", GET_OBJ_SHORT_DESC(obj));
			
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
			act(buf1, FALSE, ch, obj, NULL, TO_CHAR);
			act(buf2, TRUE, ch, obj, NULL, TO_ROOM);
			
			if (reforge_data[subcmd].ability != NOTHING) {
				gain_ability_exp(ch, reforge_data[subcmd].ability, 50);
			}
		}
	}
	else if (is_abbrev(arg2, "renew")) {
		proto = obj_proto(GET_OBJ_VNUM(obj));
		
		// calculate gem cost based on the gear level of the original item
		res[0].vnum = o_BLOODSTONE;
		res[0].amount = MAX(1, (proto ? rate_item(proto) : rate_item(obj)) / 3);
		
		if (!proto) {
			msg_to_char(ch, "You can't renew that.\r\n");
		}
		else if (!has_resources(ch, res, TRUE, TRUE)) {
			// sends own message
		}
		else {
			extract_resources(ch, res, TRUE);
			
			// actual reforging: just junk it and load a new one
			old_stolen_time = obj->stolen_timer;
			old_timer = GET_OBJ_TIMER(obj);
			level = GET_OBJ_CURRENT_SCALE_LEVEL(obj);
			
			new = read_object(GET_OBJ_VNUM(proto));
			obj_to_char(new, ch);
			
			// re-apply
			new->stolen_timer = old_stolen_time;
			GET_OBJ_TIMER(new) = old_timer;
			if (level > 0 && OBJ_FLAGGED(new, OBJ_SCALABLE)) {
				scale_item_to_level(new, level);
			}
			
			// junk the old one
			extract_obj(obj);
			
			// message
			sprintf(buf, "You %s $p!", reforge_data[subcmd].command);
			act(buf, FALSE, ch, new, NULL, TO_CHAR);
			
			sprintf(buf, "$n %s $p!", reforge_data[subcmd].command);
			act(buf, TRUE, ch, new, NULL, TO_ROOM);
			
			if (reforge_data[subcmd].ability != NO_ABIL) {
				gain_ability_exp(ch, reforge_data[subcmd].ability, 50);
			}

			load_otrigger(new);
		}
	}
	else if (is_abbrev(arg2, "superior")) {
		// calculate gem cost based on the gear level of the item
		res[0].vnum = o_GLOWING_SEASHELL;
		res[0].amount = MAX(1, rate_item(obj) / 3);
		proto = obj_proto(GET_OBJ_VNUM(obj));
		
		if (OBJ_FLAGGED(obj, OBJ_SUPERIOR)) {
			msg_to_char(ch, "It is already superior.\r\n");
		}
		else if (!proto || !OBJ_FLAGGED(proto, OBJ_SCALABLE) || !(ctype = find_craft_for_obj_vnum(GET_OBJ_VNUM(obj)))) {
			msg_to_char(ch, "It can't be made superior.\r\n");
		}
		else if (GET_CRAFT_ABILITY(ctype) == NO_ABIL || get_mastery_ability(GET_CRAFT_ABILITY(ctype)) == NO_ABIL || !HAS_ABILITY(ch, get_mastery_ability(GET_CRAFT_ABILITY(ctype)))) {
			msg_to_char(ch, "You don't have the mastery to make that item superior.\r\n");
		}
		else if (!has_resources(ch, res, TRUE, TRUE)) {
			// sends own message
		}
		else {
			extract_resources(ch, res, TRUE);
			
			// actual reforging: just junk it and load a new one
			old_stolen_time = obj->stolen_timer;
			old_timer = GET_OBJ_TIMER(obj);
			level = GET_OBJ_CURRENT_SCALE_LEVEL(obj);
			
			new = read_object(GET_OBJ_VNUM(proto));
			obj_to_char(new, ch);
			
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
			if (level > 0 && OBJ_FLAGGED(new, OBJ_SCALABLE)) {
				scale_item_to_level(new, level);
			}
			
			// junk the old one
			extract_obj(obj);
			
			sprintf(buf, "You %s $p into a masterwork!", reforge_data[subcmd].command);
			act(buf, FALSE, ch, new, NULL, TO_CHAR);
			
			sprintf(buf, "$n %s $p into a masterwork!", reforge_data[subcmd].command);
			act(buf, TRUE, ch, new, NULL, TO_ROOM);

			if (reforge_data[subcmd].ability != NOTHING) {
				gain_ability_exp(ch, reforge_data[subcmd].ability, 50);
			}
		}
	}
	else {
		msg_to_char(ch, "That's not a valid %s option.\r\n", reforge_data[subcmd].command);
	}
}


ACMD(do_weave) {
	int type = NOTHING, iter = 0;
	bool this_line;
	
	one_argument(argument, arg);

	/* Find what they picked */
	if (*arg) {
		for (iter = 0; *weave_data[iter].name != '\n'; ++iter) {
			if (is_abbrev(arg, weave_data[iter].name) && (weave_data[iter].ability == NO_ABIL || HAS_ABILITY(ch, weave_data[iter].ability))) {
				type = iter;
				break;
			}
		}
	}

	if (GET_ACTION(ch) == ACT_WEAVING) {
		act("You stop weaving.", FALSE, ch, 0, 0, TO_CHAR);
		cancel_action(ch);
	}
	else if (GET_ACTION(ch) != ACT_NONE) {
		msg_to_char(ch, "You're already busy doing something else.\r\n");
	}
	else if (!*arg || type == NOTHING) {
		msg_to_char(ch, "You can weave:\r\n");
		this_line = FALSE;
		for (iter = 0; *weave_data[iter].name != '\n'; ++iter) {
			if (*weave_data[iter].name == '\t') {
				if (this_line) {
					msg_to_char(ch, "\r\n");
					this_line = FALSE;
				}
			}
			else if (weave_data[iter].ability == NO_ABIL || HAS_ABILITY(ch, weave_data[iter].ability)) {
				msg_to_char(ch, "%s%s", (this_line ? ", " : " "), weave_data[iter].name);
				this_line = TRUE;
			}
		}
		
		if (this_line) {
			msg_to_char(ch, "\r\n");
		}
	}
	else if (!can_use_room(ch, IN_ROOM(ch), GUESTS_ALLOWED)) {
		msg_to_char(ch, "You don't have permission to do that here.\r\n");
	}
	else if (!has_resources(ch, weave_data[type].resources, TRUE, TRUE)) {
		// message sent by has_resources
	}
	else {
		extract_resources(ch, weave_data[type].resources, TRUE);
		start_action(ch, ACT_WEAVING, weave_data[type].time / ((ROOM_BLD_FLAGGED(IN_ROOM(ch), BLD_TAILOR) && IS_COMPLETE(IN_ROOM(ch))) ? 4 : 1), 0);
		
		GET_ACTION_VNUM(ch, 0) = type;
		GET_ACTION_VNUM(ch, 1) = weave_data[type].vnum;

		sprintf(buf, "You begin weaving %s.", get_obj_name_by_proto(weave_data[type].vnum));
		act(buf, FALSE, ch, 0, 0, TO_CHAR);
		sprintf(buf1, "$n begins weaving %s.", get_obj_name_by_proto(weave_data[type].vnum));
		act(buf1, TRUE, ch, 0, 0, TO_ROOM);
	}
}
