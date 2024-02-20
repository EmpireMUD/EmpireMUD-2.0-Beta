/* ************************************************************************
*   File: act.item.c                                      EmpireMUD 2.0b5 *
*  Usage: object handling routines -- get/drop and container handling     *
*                                                                         *
*  EmpireMUD code base by Paul Clarke, (C) 2000-2024                      *
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
#include "constants.h"
#include "boards.h"
#include "olc.h"

/**
* Contents:
*   Helpers
*   Drop Helpers
*   Equipment Set Helpers
*   Get Helpers
*   Give Helpers
*   Liquid Helpers
*   Scaling
*   Shipping System
*   Trade Command Functions
*   Warehouse Command Functions
*   Commands
*/

// extern functions
ACMD(do_home);
void do_get_from_vehicle(char_data *ch, vehicle_data *veh, char *arg, int mode, int howmany);
void do_put_obj_in_vehicle(char_data *ch, vehicle_data *veh, int dotmode, char *arg, int howmany);
void use_ammo(char_data *ch, obj_data *obj);
void use_poison(char_data *ch, obj_data *obj);

// local protos
ACMD(do_pour);
ACMD(do_unshare);
ACMD(do_warehouse);
int get_wear_by_item_wear(bitvector_t item_wear);
void move_ship_to_destination(empire_data *emp, struct shipping_data *shipd, room_data *to_room);
obj_data *perform_eq_change_unequip(char_data *ch, int pos);
static void wear_message(char_data *ch, obj_data *obj, int where);

// local stuff
#define drink_OBJ  -1
#define drink_ROOM  1

// ONLY flags to show on identify / warehouse inv
// TODO consider moving this to structs.h near the flag list
bitvector_t show_obj_flags = OBJ_ENCHANTED | OBJ_JUNK | OBJ_TWO_HANDED | OBJ_BIND_ON_EQUIP | OBJ_BIND_ON_PICKUP | OBJ_GENERIC_DROP | OBJ_UNIQUE;


 //////////////////////////////////////////////////////////////////////////////
//// HELPERS /////////////////////////////////////////////////////////////////

/**
* Apply the actual effects of a potion. This does NOT extract the potion.
*
* @param obj_data *obj the potion
* @param char_data *ch the quaffer
*/
void apply_potion(obj_data *obj, char_data *ch) {
	any_vnum aff_type = GET_POTION_AFFECT(obj) != NOTHING ? GET_POTION_AFFECT(obj) : ATYPE_POTION;
	struct affected_type *af;
	struct obj_apply *apply;
	
	act("A swirl of light passes over you!", FALSE, ch, NULL, NULL, TO_CHAR);
	act("A swirl of light passes over $n!", FALSE, ch, NULL, NULL, TO_ROOM);
	
	// ensure scaled
	if (OBJ_FLAGGED(obj, OBJ_SCALABLE)) {
		scale_item_to_level(obj, 1);	// minimum level
	}
	
	// remove any old buffs (if adding a new one)
	if (GET_OBJ_AFF_FLAGS(obj) || GET_OBJ_APPLIES(obj)) {
		affect_from_char(ch, aff_type, FALSE);
	}
	
	if (GET_OBJ_AFF_FLAGS(obj)) {
		af = create_flag_aff(aff_type, 30 * SECS_PER_REAL_MIN, GET_OBJ_AFF_FLAGS(obj), ch);
		affect_to_char(ch, af);
		free(af);
	}

	LL_FOREACH(GET_OBJ_APPLIES(obj), apply) {
		af = create_mod_aff(aff_type, 30 * SECS_PER_REAL_MIN, apply->location, apply->modifier, ch);
		affect_to_char(ch, af);
		free(af);
	}
	
	if (GET_POTION_COOLDOWN_TYPE(obj) != NOTHING && GET_POTION_COOLDOWN_TIME(obj) > 0) {
		add_cooldown(ch, GET_POTION_COOLDOWN_TYPE(obj), GET_POTION_COOLDOWN_TIME(obj));
	}
}


/**
* @param char_data *ch Prospective taker.
* @param obj_data *obj The item he's trying to take.
* @return bool TRUE if ch can take obj.
*/
bool can_take_obj(char_data *ch, obj_data *obj) {
	if (!IS_NPC(ch) && !CAN_CARRY_OBJ(ch, obj)) {
		act("$p: you can't carry that many items.", FALSE, ch, obj, 0, TO_CHAR);
		return FALSE;
	}
	else if (!(CAN_WEAR(obj, ITEM_WEAR_TAKE))) {
		act("$p: you can't take that!", FALSE, ch, obj, 0, TO_CHAR);
		return FALSE;
	}
	return TRUE;
}


/**
* Interaction func for "combine". This almost always extracts the original
* item, so it should basically always return TRUE.
*
* INTERACTION_FUNC provides: ch, interaction, inter_room, inter_mob, inter_item, inter_veh
*/
INTERACTION_FUNC(combine_obj_interact) {
	char to_char[MAX_STRING_LENGTH], to_room[MAX_STRING_LENGTH];
	struct resource_data *res = NULL;
	obj_data *new_obj;
	
	// flags to keep on separate
	bitvector_t preserve_flags = OBJ_SEEDED | OBJ_NO_BASIC_STORAGE | OBJ_NO_WAREHOUSE | OBJ_CREATED;
	
	// how many they need
	add_to_resource_list(&res, RES_OBJECT, GET_OBJ_VNUM(inter_item), interaction->quantity, 0);
	
	if (!has_resources(ch, res, can_use_room(ch, IN_ROOM(ch), MEMBERS_ONLY), TRUE, NULL)) {
		// error message sent by has_resources
		free_resource_list(res);
		return TRUE;
	}
	
	snprintf(to_char, sizeof(to_char), "You combine %dx %s into $p!", interaction->quantity, skip_filler(GET_OBJ_SHORT_DESC(inter_item)));
	snprintf(to_room, sizeof(to_room), "$n combines %dx %s into $p!", interaction->quantity, skip_filler(GET_OBJ_SHORT_DESC(inter_item)));
	
	new_obj = read_object(interaction->vnum, TRUE);
	scale_item_to_level(new_obj, GET_OBJ_CURRENT_SCALE_LEVEL(inter_item));
	
	if (GET_OBJ_TIMER(new_obj) != UNLIMITED && GET_OBJ_TIMER(inter_item) != UNLIMITED) {
		GET_OBJ_TIMER(new_obj) = MIN(GET_OBJ_TIMER(new_obj), GET_OBJ_TIMER(inter_item));
	}
	
	if (GET_LOYALTY(ch)) {
		// subtract old item and add the new one
		add_production_total(GET_LOYALTY(ch), GET_OBJ_VNUM(inter_item), -1 * interaction->quantity);
		add_production_total(GET_LOYALTY(ch), interaction->vnum, 1);
	}
	extract_resources(ch, res, can_use_room(ch, IN_ROOM(ch), MEMBERS_ONLY), NULL);
	
	// ownership
	new_obj->last_owner_id = GET_IDNUM(ch);
	new_obj->last_empire_id = GET_LOYALTY(ch) ? EMPIRE_VNUM(GET_LOYALTY(ch)) : NOTHING;
	
	// copy these flags, if any
	SET_BIT(GET_OBJ_EXTRA(new_obj), (GET_OBJ_EXTRA(inter_item) & preserve_flags));
	
	// put it somewhere
	if (CAN_WEAR(new_obj, ITEM_WEAR_TAKE)) {
		obj_to_char(new_obj, ch);
	}
	else {
		obj_to_room(new_obj, IN_ROOM(ch));
	}
	
	act(to_char, FALSE, ch, new_obj, NULL, TO_CHAR);
	act(to_room, TRUE, ch, new_obj, NULL, TO_ROOM);
	
	if (load_otrigger(new_obj) && new_obj->carried_by) {
		get_otrigger(new_obj, new_obj->carried_by, FALSE);
	}
	
	free_resource_list(res);
	return TRUE;
}


/**
* @param room_data *room The room to check.
* @return int Number of items in the room (large counts double).
*/
int count_objs_in_room(room_data *room) {
	int items_in_room = 0;
	obj_data *search;
	
	DL_FOREACH2(ROOM_CONTENTS(room), search, next_content) {
		items_in_room += obj_carry_size(search);
	}
	
	return items_in_room;
}


/**
* Douses a LIGHT-type object, if possible. This turns it off, charges 1 hour of
* light time (lost fuel; also prevents well-timed infinite lights by dousing
* and re-lighting). This will extract JUNK-WHEN-EXPIRED and DESTROY-WHEN-DOUSED
* lights on its own.
*
* If this returns TRUE, you can check success with LIGHT_IS_LIT(obj).
*
* @param obj_data *obj The LIGHT + CAN-DOUSE object to be doused (may be purged).
* @return bool Returns FALSE if the light was purged; TRUE in all other cases (even if not doused).
*/
bool douse_light(obj_data *obj) {
	if (IS_LIGHT(obj) && LIGHT_IS_LIT(obj) && LIGHT_FLAGGED(obj, LIGHT_FLAG_CAN_DOUSE)) {
		// charge 1 hour
		if (GET_LIGHT_HOURS_REMAINING(obj) > 0) {
			set_obj_val(obj, VAL_LIGHT_HOURS_REMAINING, GET_LIGHT_HOURS_REMAINING(obj) - 1);
		}
		
		// turn off
		set_obj_val(obj, VAL_LIGHT_IS_LIT, 0);
		apply_obj_light(obj, FALSE);
		
		// should not be storable after this
		SET_BIT(GET_OBJ_EXTRA(obj), OBJ_NO_BASIC_STORAGE);
		
		if (LIGHT_FLAGGED(obj, LIGHT_FLAG_DESTROY_WHEN_DOUSED) || (GET_LIGHT_HOURS_REMAINING(obj) == 0 && LIGHT_FLAGGED(obj, LIGHT_FLAG_JUNK_WHEN_EXPIRED))) {
			extract_obj(obj);
			return FALSE;	// purged
		}
	}
	
	request_obj_save_in_world(obj);
	return TRUE;	// not purged
}


/**
* Attempts to find a container with the name given. Prefers inventory over
* room; will also check gear; will return a non-container if no container
* exists. Corpses count as containers for the purpose of this function, which
* is normally used for a "look in" or "get from".
*
* @param char_data *ch The person who's looking.
* @param char *name The target argument.
* @param int *number Optional: For multi-list number targeting (get all 4.corpse; may be NULL)
* @return obj_data *The item found, or NULL. May or may not be a container/corpse.
*/
obj_data *get_obj_for_char_prefer_container(char_data *ch, char *name, int *number) {
	char copy[MAX_INPUT_LENGTH], *tmp = copy;
	obj_data *obj, *backup = NULL, *list[2];
	int iter, num;
	bool gave_num;
	
	if (!number) {
		strcpy(tmp, name);
		number = &num;
		num = get_number(&tmp);
	}
	else {
		tmp = name;
	}
	if (*number == 0) {
		return NULL;
	}
	
	// if the number is > 1 (PROBABLY requested #.name) take any item that matches
	gave_num = (*number > 1);
	
	// looking in two places first:
	list[0] = ch->carrying;
	list[1] = ROOM_CONTENTS(IN_ROOM(ch));
	
	for (iter = 0; iter < 2; ++iter) {
		DL_FOREACH2(list[iter], obj, next_content) {
			if (CAN_SEE_OBJ(ch, obj) && MATCH_ITEM_NAME(tmp, obj)) {
				if (gave_num) {
					if (--(*number) == 0) {
						return obj;
					}
				}
				else {	// did not give a number
					if (IS_CONTAINER(obj) || IS_CORPSE(obj)) {
						return obj;	// is a match
					}
					else if (!backup) {
						backup = obj;	// missing type but otherwise a match
					}
				}
			}
		}
	}
	
	// then look in character equipment if we got here:
	for (iter = 0; iter < NUM_WEARS; ++iter) {
		if (GET_EQ(ch, iter) && CAN_SEE_OBJ(ch, GET_EQ(ch, iter)) && MATCH_ITEM_NAME(tmp, GET_EQ(ch, iter))) {
			if (gave_num) {
				if (--(*number) == 0) {
					return GET_EQ(ch, iter);
				}
			}
			else {	// did not give a number
				if (IS_CONTAINER(GET_EQ(ch, iter)) || IS_CORPSE(GET_EQ(ch, iter))) {
					return GET_EQ(ch, iter);	// is a match
				}
				else if (!backup) {
					backup = GET_EQ(ch, iter);	// missing type but otherwise a match
				}
			}
		}
	}

	return backup;
}


/**
* Finds an object for a character using hte 'light' command. This will prefer
* an object they CAN light.
*
* @param char_data *ch The person who's looking.
* @param char *name The target argument.
* @param int *number Optional: For multi-list number targeting (look 4.hat; may be NULL)
* @param obj_data *list The list to search.
* @return obj_data *The item found, or NULL. May or may not be lightable.
*/
obj_data *get_obj_in_list_vis_prefer_lightable(char_data *ch, char *name, int *number, obj_data *list) {
	char copy[MAX_INPUT_LENGTH], *tmp = copy;
	obj_data *i, *backup = NULL;
	int num;
	bool gave_num;
	
	if (!number) {
		strcpy(tmp, name);
		number = &num;
		num = get_number(&tmp);
	}
	else {
		tmp = name;
	}
	if (*number == 0) {
		return NULL;
	}
	
	// if the number is > 1 (PROBABLY requested #.name) take any item that matches
	// note: this changed in b5.108; previously it could detect this more accurately
	gave_num = (*number > 1);
	
	DL_FOREACH2(list, i, next_content) {
		if (CAN_SEE_OBJ(ch, i) && MATCH_ITEM_NAME(tmp, i)) {
			if (gave_num) {
				if (--(*number) == 0) {
					return i;
				}
			}
			else {	// did not give a number
				if (CAN_LIGHT_OBJ(i)) {
					return i;	// is a match
				}
				else if (!backup) {
					backup = i;	// missing interaction but otherwise a match
				}
			}
		}
	}

	return backup;
}


/**
* Returns the targeted position, or else the first place an item can be worn.
*
* @param char_data *ch The player.
* @param obj_data *obj The item trying to wear.
* @param char *arg The typed argument, which may be empty, or a body part.
* @return int A WEAR_ pos, or NO_WEAR.
*/
int find_eq_pos(char_data *ch, obj_data *obj, char *arg) {
	int where = NO_WEAR;

	if (!arg || !*arg) {
		where = get_wear_by_item_wear(GET_OBJ_WEAR(obj));
	}
	else {
		if ((where = search_block(arg, wear_keywords, FALSE)) == NOTHING) {
			sprintf(buf, "'%s'?  What part of your body is THAT?\r\n", arg);
			send_to_char(buf, ch);
		}
	}
	
	// check cascading wears (ring 1 -> ring 2)
	if (GET_EQ(ch, where)) {
		if (wear_data[where].cascade_pos != NO_WEAR) {
			where = wear_data[where].cascade_pos;
		}
	}

	return (where);
}


/**
* Finds a lighter. Limited-use lighters that are marked (keep) will NOT be
* used, but will result in the 'had_keep' argument being set to TRUE.
*
* This will return the BEST lighter in the inventory -- either an unlimited
* lighter, or the one with the fewest charges remaining.
*
* @param obj_data *list The list to check (usually ch->carrying).
* @param bool *had_keep If there's a keep'd lighter that can't be used, this will be set to TRUE (so you can inform the player).
* @return obj_data* The lighter, if it found one.
*/
obj_data *find_lighter_in_list(obj_data *list, bool *had_keep) {
	obj_data *obj, *best = NULL;
	
	*had_keep = FALSE;	// presumably
	
	DL_FOREACH2(list, obj, next_content) {
		// two types of lighters:
		if (IS_LIGHT(obj)) {
			if (LIGHT_IS_LIT(obj) && LIGHT_FLAGGED(obj, LIGHT_FLAG_LIGHT_FIRE) && (!best || !IS_LIGHT(best))) {
				best = obj;
			}
		}
		else if (IS_LIGHTER(obj)) {
			if (GET_LIGHTER_USES(obj) == UNLIMITED) {
				if (!best || (!IS_LIGHT(best) && GET_LIGHTER_USES(best) != UNLIMITED)) {
					best = obj;	// new best
				}
			}
			else if (TRUE || !OBJ_FLAGGED(obj, OBJ_KEEP)) {	// not unlmited; don't use kept items if not unlimited
				// NOTE: as of b5.176 we ignore keep on lighters-- use it anyway
				if (!best || (GET_LIGHTER_USES(best) != UNLIMITED && GET_LIGHTER_USES(best) > GET_LIGHTER_USES(obj))) {
					best = obj;	// new best
				}
			}
			else {
				*had_keep = TRUE;
			}
		}
	}
	
	return best;
}


/**
* Converts a "gotten" object into money, if it's coins. This will extract the
* object if so.
*
* @param char_data *ch The person getting obj.
* @param obj_data *obj The item being picked up.
* @return bool TRUE if the obj was extracted, FALSE if it stays.
*/
bool get_check_money(char_data *ch, obj_data *obj) {
	empire_data *emp = real_empire(GET_COINS_EMPIRE_ID(obj));
	int value;

	// npcs will keep the obj version
	if (IS_NPC(ch)) {
		return FALSE;
	}
	
	switch (GET_OBJ_TYPE(obj)) {
		case ITEM_COINS: {
			value = GET_COINS_AMOUNT(obj);
			increase_coins(ch, emp, value);
			if (ch->desc) {	// these usually appear with stacked messages
				stack_msg_to_desc(ch->desc, "There %s %s.\r\n", (value == 1 ? "was" : "were"), money_amount(emp, value));
			}
			break;
		}
		case ITEM_CURRENCY: {
			value = GET_CURRENCY_AMOUNT(obj);
			add_currency(ch, GET_CURRENCY_VNUM(obj), value);
			break;
		}
		default: {
			return FALSE;	// nope
		}
	}
	
	// made it this far
	extract_obj(obj);
	return TRUE;
}


/**
* Converts an ITEM_WEAR_ into a corresponding WEAR_ flag.
*
* @param bitvector_t item_wear The ITEM_WEAR_ bits.
* @return int A WEAR_ position that matches, or NOWEHRE.
*/
int get_wear_by_item_wear(bitvector_t item_wear) {
	int pos;
	
	for (pos = 0; item_wear; ++pos, item_wear >>= 1) {
		if (IS_SET(item_wear, BIT(0)) && item_wear_to_wear[pos] != NO_WEAR) {
			return item_wear_to_wear[pos];
		}
	}
	
	return NO_WEAR;
}



/**
* Interaction func for "identify".
* 
* INTERACTION_FUNC provides: ch, interaction, inter_room, inter_mob, inter_item, inter_veh
*/
INTERACTION_FUNC(identifies_to_interact) {
	char to_char[MAX_STRING_LENGTH];
	obj_data *new_obj;
	int iter;
	
	// flags to keep on identifies-to
	bitvector_t preserve_flags = OBJ_SEEDED | OBJ_CREATED | OBJ_KEEP;
	
	if (interaction->quantity > 1) {
		snprintf(to_char, sizeof(to_char), "%s turns out to be %s (x%d)!", GET_OBJ_SHORT_DESC(inter_item), get_obj_name_by_proto(interaction->vnum), interaction->quantity);
	}
	else {	// only 1
		snprintf(to_char, sizeof(to_char), "%s turns out to be %s!", GET_OBJ_SHORT_DESC(inter_item), get_obj_name_by_proto(interaction->vnum));
	}
	act(to_char, FALSE, ch, NULL, NULL, TO_CHAR | TO_QUEUE);
	
	if (GET_LOYALTY(ch)) {
		// add the gained items (the original item is subtracted in do_identify)
		add_production_total(GET_LOYALTY(ch), interaction->vnum, interaction->quantity);
	}
	
	for (iter = 0; iter < interaction->quantity; ++iter) {
		new_obj = read_object(interaction->vnum, TRUE);
		scale_item_to_level(new_obj, GET_OBJ_CURRENT_SCALE_LEVEL(inter_item));
	
		if (GET_OBJ_TIMER(new_obj) != UNLIMITED && GET_OBJ_TIMER(inter_item) != UNLIMITED) {
			GET_OBJ_TIMER(new_obj) = MIN(GET_OBJ_TIMER(new_obj), GET_OBJ_TIMER(inter_item));
		}
		
		// copy these flags, if any
		SET_BIT(GET_OBJ_EXTRA(new_obj), (GET_OBJ_EXTRA(inter_item) & preserve_flags));
		
		// ownership
		new_obj->last_owner_id = GET_IDNUM(ch);
		new_obj->last_empire_id = GET_LOYALTY(ch) ? EMPIRE_VNUM(GET_LOYALTY(ch)) : NOTHING;
	
		// put it somewhere
		if (CAN_WEAR(new_obj, ITEM_WEAR_TAKE)) {
			obj_to_char(new_obj, ch);
		}
		else {
			obj_to_room(new_obj, IN_ROOM(ch));
		}
		if (load_otrigger(new_obj) && new_obj->carried_by) {
			get_otrigger(new_obj, new_obj->carried_by, FALSE);
		}
	}
	
	return TRUE;
}


/**
* Runs the identifies-to interactions on an object, if any. This may change
* the object. In some cases it will extract the object itself. In other cases,
* the object must be extracted after the identify.
*
* @param char_data *ch The player identifying the object.
* @param obj_data **obj A pointer to the obj pointer (because the object may change during this function).
* @param bool *extract A pointer to a boolean. If it sets this bool to TRUE, 'obj' must be extracted afterwards. If FALSE, any extractions were already handled.
* @return bool TRUE if an object changed, FALSE if not.
*/
bool run_identifies_to(char_data *ch, obj_data **obj, bool *extract) {
	obj_data *first_inv, *first_room;
	bool result = FALSE;
	
	if (!ch || !obj || !*obj) {
		return result;	// no work
	}
	
	*extract = FALSE;
	
	// for detecting the change
	first_inv = ch->carrying;
	first_room = ROOM_CONTENTS(IN_ROOM(ch));
	
	if (run_interactions(ch, GET_OBJ_INTERACTIONS(*obj), INTERACT_IDENTIFIES_TO, IN_ROOM(ch), NULL, *obj, NULL, identifies_to_interact)) {
		result = TRUE;
		
		if (GET_LOYALTY(ch)) {
			// subtract old item from empire counts
			add_production_total(GET_LOYALTY(ch), GET_OBJ_VNUM(*obj), -1);
		}
	
		// did have one
		if (first_inv != ch->carrying) {
			extract_obj(*obj);	// done with the old obj
			*obj = ch->carrying;	// idenify this obj instead
			*extract = FALSE;
		}
		else if (first_room != ROOM_CONTENTS(IN_ROOM(ch))) {
			extract_obj(*obj);	// done with the old obj
			*obj = ROOM_CONTENTS(IN_ROOM(ch));	// idenify this obj instead
			*extract = FALSE;
		}
		else {
			// otherwise will still identify this object but then will extract it after
			*extract = TRUE;
		}
	}
	
	return result;
}


/**
* Shows an "identify" of the object -- its stats -- to a player.
*
* @param obj_data *obj The item to identify.
* @param char_data *ch The person to show the data to.
* @param bool simple If TRUE, hides some of the info.
*/
void identify_obj_to_char(obj_data *obj, char_data *ch, bool simple) {
	struct string_hash *str_iter, *next_str, *str_hash = NULL;
	vehicle_data *veh, *veh_iter, *next_veh;
	bld_data *bld, *bld_iter, *next_bld;
	struct obj_storage_type *store;
	craft_data *craft, *next_craft;
	struct custom_message *ocm;
	struct player_eq_set *pset;
	struct bld_relation *relat;
	player_index_data *index;
	struct eq_set_obj *oset;
	struct obj_apply *apply;
	char lbuf[MAX_STRING_LENGTH], part[MAX_STRING_LENGTH], location[MAX_STRING_LENGTH], *temp;
	size_t size, line_size, part_size;
	generic_data *comp = NULL;
	obj_data *proto;
	crop_data *cp;
	int found;
	double rating;
	bool any, library, showed_level = FALSE;
	
	// sanity / don't bother
	if (!obj || !ch || !ch->desc) {
		return;
	}
	
	// used by some things later
	proto = obj_proto(GET_OBJ_VNUM(obj));
	
	// determine location
	if (IN_ROOM(obj)) {
		strcpy(location, " (in room)");
	}
	else if (obj->carried_by) {
		snprintf(location, sizeof(location), ", carried by %s,", PERS(obj->carried_by, obj->carried_by, FALSE));
	}
	else if (obj->in_vehicle) {
		snprintf(location, sizeof(location), ", in %s,", get_vehicle_short_desc(obj->in_vehicle, ch));
	}
	else if (obj->in_obj) {
		snprintf(location, sizeof(location), ", in %s,", GET_OBJ_DESC(obj->in_obj, ch, OBJ_DESC_SHORT));
	}
	else if (obj->worn_by) {
		snprintf(location, sizeof(location), ", worn by %s,", PERS(obj->worn_by, obj->worn_by, FALSE));
	}
	else {
		*location = '\0';
	}
	
	// basic info
	snprintf(lbuf, sizeof(lbuf), "Your analysis of %s$p\t0%s reveals:", obj_color_by_quality(obj, ch), location);
	act(lbuf, FALSE, ch, obj, NULL, TO_CHAR);
	
	if (!simple && GET_OBJ_REQUIRES_QUEST(obj) != NOTHING) {
		msg_to_char(ch, "Quest: %s\r\n", get_quest_name_by_proto(GET_OBJ_REQUIRES_QUEST(obj)));
	}
	
	// if it has any wear bits other than TAKE, show if they can't use it
	if (!simple && CAN_WEAR(obj, ~ITEM_WEAR_TAKE)) {
		// the TRUE causes it to send a message if unusable
		msg_to_char(ch, "&r");
		can_wear_item(ch, obj, TRUE);
		msg_to_char(ch, "&0");
	}

	if (!simple && GET_OBJ_STORAGE(obj) && !OBJ_FLAGGED(obj, OBJ_NO_BASIC_STORAGE) && OBJ_CAN_STORE(obj)) {
		LL_FOREACH(GET_OBJ_STORAGE(obj), store) {
			if (store->type == TYPE_BLD && (bld = building_proto(store->vnum))) {
				add_string_hash(&str_hash, GET_BLD_NAME(bld), 1);
				
				// check stores-like relations
				HASH_ITER(hh, building_table, bld_iter, next_bld) {
					LL_FOREACH(GET_BLD_RELATIONS(bld_iter), relat) {
						if (relat->type == BLD_REL_STORES_LIKE_BLD && relat->vnum == GET_BLD_VNUM(bld)) {
							add_string_hash(&str_hash, GET_BLD_NAME(bld_iter), 1);
						}
					}
				}
				HASH_ITER(hh, vehicle_table, veh_iter, next_veh) {
					LL_FOREACH(VEH_RELATIONS(veh_iter), relat) {
						if (relat->type == BLD_REL_STORES_LIKE_BLD && relat->vnum == GET_BLD_VNUM(bld)) {
							add_string_hash(&str_hash, skip_filler(VEH_SHORT_DESC(veh_iter)), 1);
						}
					}
				}
			}
			else if (store->type == TYPE_VEH && (veh = vehicle_proto(store->vnum))) {
				add_string_hash(&str_hash, skip_filler(VEH_SHORT_DESC(veh)), 1);
				
				// check stores-like relations
				HASH_ITER(hh, building_table, bld_iter, next_bld) {
					LL_FOREACH(GET_BLD_RELATIONS(bld_iter), relat) {
						if (relat->type == BLD_REL_STORES_LIKE_VEH && relat->vnum == VEH_VNUM(veh)) {
							add_string_hash(&str_hash, GET_BLD_NAME(bld_iter), 1);
						}
					}
				}
				HASH_ITER(hh, vehicle_table, veh_iter, next_veh) {
					LL_FOREACH(VEH_RELATIONS(veh_iter), relat) {
						if (relat->type == BLD_REL_STORES_LIKE_VEH && relat->vnum == VEH_VNUM(veh)) {
							add_string_hash(&str_hash, skip_filler(VEH_SHORT_DESC(veh_iter)), 1);
						}
					}
				}
			}
		}
		
		snprintf(lbuf, sizeof(lbuf), "Storage locations:");
		found = 0;
		HASH_SORT(str_hash, sort_string_hash);
		HASH_ITER(hh, str_hash, str_iter, next_str) {
			if (next_str && !str_cmp(str_iter->str, next_str->str)) {
				continue;	// avoid case-sensitive dupes
			}
			snprintf(lbuf + strlen(lbuf), sizeof(lbuf) - strlen(lbuf), "%s%s", (found++ > 0 ? ", " : " "), str_iter->str);
		}
		free_string_hash(&str_hash);
		if (strlen(lbuf) < sizeof(lbuf) + 2) {
			strcat(lbuf, "\r\n");
		}
		temp = str_dup(lbuf);
		format_text(&temp, 0, NULL, MAX_STRING_LENGTH);
		send_to_char(temp, ch);
		free(temp);
	}
	
	// other storage
	if (!simple && CAN_WEAR(obj, ITEM_WEAR_TAKE)) {
		library = (IS_BOOK(obj) && book_proto(GET_BOOK_ID(obj)));
		if (UNIQUE_OBJ_CAN_STORE(obj, FALSE)) {
			msg_to_char(ch, "Storage location: Home, Warehouse%s\r\n", (library ? ", Library" : ""));
		}
		else if (UNIQUE_OBJ_CAN_STORE(obj, TRUE)) {
			msg_to_char(ch, "Storage location: Home%s\r\n", (library ? ", Library" : ""));
		}
		else if (library) {
			msg_to_char(ch, "Storage location: Library\r\n");
		}
		else if (OBJ_FLAGGED(obj, OBJ_NO_BASIC_STORAGE | OBJ_NO_WAREHOUSE) && (!proto || !OBJ_FLAGGED(proto, OBJ_NO_BASIC_STORAGE | OBJ_NO_WAREHOUSE))) {
			msg_to_char(ch, "Storage location: none (modified object)\r\n");
		}
	}

	// binding section
	if (!simple && OBJ_BOUND_TO(obj)) {
		struct obj_binding *bind;		
		msg_to_char(ch, "Bound to:");
		any = FALSE;
		for (bind = OBJ_BOUND_TO(obj); bind; bind = bind->next) {
			msg_to_char(ch, "%s %s", (any ? "," : ""), (index = find_player_index_by_idnum(bind->idnum)) ? index->fullname : "<unknown>");
			any = TRUE;
		}
		msg_to_char(ch, "\r\n");
	}
	
	// show level if scalable OR wearable (will show quality on the same line)
	if (GET_OBJ_CURRENT_SCALE_LEVEL(obj) > 0 && ((GET_OBJ_WEAR(obj) & ~ITEM_WEAR_TAKE) != NOBITS || (proto && OBJ_FLAGGED(proto, OBJ_SCALABLE)))) {
		msg_to_char(ch, "Level: %s%d\t0, ", color_by_difficulty(ch, GET_OBJ_CURRENT_SCALE_LEVEL(obj)), GET_OBJ_CURRENT_SCALE_LEVEL(obj));
		showed_level = TRUE;
	}
	
	// quality (same line as level, if applicable)
	if (OBJ_FLAGGED(obj, OBJ_SUPERIOR)) {
		msg_to_char(ch, "Quality: superior\r\n");
	}
	else if (OBJ_FLAGGED(obj, OBJ_HARD_DROP) && OBJ_FLAGGED(obj, OBJ_GROUP_DROP)) {
		msg_to_char(ch, "Quality: boss drop\r\n");
	}
	else if (OBJ_FLAGGED(obj, OBJ_GROUP_DROP)) {
		msg_to_char(ch, "Quality: group drop\r\n");
	}
	else if (OBJ_FLAGGED(obj, OBJ_HARD_DROP)) {
		msg_to_char(ch, "Quality: hard drop\r\n");
	}
	else if (showed_level) {
		// skip normal quality unless level also appeared
		msg_to_char(ch, "Quality: normal\r\n");
	}
	
	// only show gear if equippable (has more than ITEM_WEAR_TRADE)
	if ((GET_OBJ_WEAR(obj) & ~ITEM_WEAR_TAKE) != NOBITS) {
		if ((rating = rate_item(obj)) > 0) {
			msg_to_char(ch, "Gear rating: %.1f\r\n", rating);
		}
		
		if (!simple) {
			prettier_sprintbit(GET_OBJ_WEAR(obj) & ~ITEM_WEAR_TAKE, wear_bits, buf);
			msg_to_char(ch, "Can be worn on: %s\r\n", buf);
		}
	}
	
	// flags
	if (GET_OBJ_EXTRA(obj) & show_obj_flags) {
		prettier_sprintbit(GET_OBJ_EXTRA(obj) & show_obj_flags, extra_bits, buf);
		msg_to_char(ch, "It is: %s\r\n", buf);
	}
	
	// tool types
	if (GET_OBJ_TOOL_FLAGS(obj)) {
		prettier_sprintbit(GET_OBJ_TOOL_FLAGS(obj), tool_flags, buf);
		msg_to_char(ch, "Tool type: %s\r\n", buf);
	}
	if (GET_OBJ_REQUIRES_TOOL(obj)) {
		prettier_sprintbit(GET_OBJ_REQUIRES_TOOL(obj), tool_flags, buf);
		msg_to_char(ch, "Requires tool to use when crafting: %s\r\n", buf);
	}
	
	if (GET_OBJ_AFF_FLAGS(obj)) {
		prettier_sprintbit(GET_OBJ_AFF_FLAGS(obj), affected_bits, buf);
		msg_to_char(ch, "Affects: %s\r\n", buf);
	}
	
	// ITEM_x: identify obj
	switch (GET_OBJ_TYPE(obj)) {
		case ITEM_PAINT: {
			sprinttype(GET_PAINT_COLOR(obj), paint_names, lbuf, sizeof(lbuf), "UNDEFINED");
			sprinttype(GET_PAINT_COLOR(obj), paint_colors, part, sizeof(part), "&0");
			msg_to_char(ch, "Paint color: %s%s\t0\r\n", part, lbuf);
			break;
		}
		case ITEM_POISON: {
			if (GET_POISON_AFFECT(obj) != NOTHING) {
				msg_to_char(ch, "Poison type: %s\r\n", get_generic_name_by_vnum(GET_POISON_AFFECT(obj)));
			}
			msg_to_char(ch, "Poison has %d charges remaining.\r\n", GET_POISON_CHARGES(obj));
			break;
		}
		case ITEM_RECIPE: {
			craft_data *cft = craft_proto(GET_RECIPE_VNUM(obj));
			msg_to_char(ch, "Teaches craft: %s (%s%s)\r\n", cft ? GET_CRAFT_NAME(cft) : "UNKNOWN", cft ? craft_types[GET_CRAFT_TYPE(cft)] : "?", (cft && has_learned_craft(ch, GET_CRAFT_VNUM(cft))) ? ", learned" : "");
			break;
		}
		case ITEM_WEAPON: {
			msg_to_char(ch, "Speed: %.2f\r\n", get_weapon_speed(obj));
			msg_to_char(ch, "Damage: %d (%s+%.2f base dps)\r\n", GET_WEAPON_DAMAGE_BONUS(obj), (IS_MAGIC_ATTACK(GET_WEAPON_TYPE(obj)) ? "Intelligence" : "Strength"), get_base_dps(obj));
			msg_to_char(ch, "Damage type is %s (%s/%s).\r\n", get_attack_name_by_vnum(GET_WEAPON_TYPE(obj)), weapon_types[get_attack_weapon_type_by_vnum(GET_WEAPON_TYPE(obj))], damage_types[get_attack_damage_type_by_vnum(GET_WEAPON_TYPE(obj))]);
			break;
		}
		case ITEM_ARMOR:
			msg_to_char(ch, "Armor type: %s\r\n", armor_types[GET_ARMOR_TYPE(obj)]);
			break;
		case ITEM_CONTAINER:
			msg_to_char(ch, "Holds %d items.\r\n", GET_MAX_CONTAINER_CONTENTS(obj));
			break;
		case ITEM_DRINKCON:
			if (GET_DRINK_CONTAINER_CONTENTS(obj) > 0) {
				if (liquid_flagged(GET_DRINK_CONTAINER_TYPE(obj), LIQF_WATER) && !str_str(get_generic_string_by_vnum(GET_DRINK_CONTAINER_TYPE(obj), GENERIC_LIQUID, GSTR_LIQUID_NAME), "water")) {
					snprintf(part, sizeof(part), " (water)");
				}
				else {
					*part = '\0';
				}
				msg_to_char(ch, "Contains %d units of %s%s.\r\n", GET_DRINK_CONTAINER_CONTENTS(obj), get_generic_string_by_vnum(GET_DRINK_CONTAINER_TYPE(obj), GENERIC_LIQUID, GSTR_LIQUID_NAME), part);
				
				if (liquid_flagged(GET_DRINK_CONTAINER_TYPE(obj), LIQF_COOLING)) {
					msg_to_char(ch, "It will cool you down if you're warm.\r\n");
				}
				if (liquid_flagged(GET_DRINK_CONTAINER_TYPE(obj), LIQF_WARMING)) {
					msg_to_char(ch, "It will warm you up if you're cold.\r\n");
				}
			}
			else {
				msg_to_char(ch, "It is empty.\r\n");
			}
			break;
		case ITEM_FOOD:
			msg_to_char(ch, "Fills for %d hour%s.\r\n", GET_FOOD_HOURS_OF_FULLNESS(obj), PLURAL(GET_FOOD_HOURS_OF_FULLNESS(obj)));
			break;
		case ITEM_CORPSE:
			msg_to_char(ch, "Corpse of ");

			if (mob_proto(GET_CORPSE_NPC_VNUM(obj))) {
				strcpy(lbuf, get_mob_name_by_proto(GET_CORPSE_NPC_VNUM(obj), FALSE));
				if (strstr(lbuf, "#n") || strstr(lbuf, "#a") || strstr(lbuf, "#e")) {
					// #n
					temp = str_replace("#n", "somebody", lbuf);
					strcpy(lbuf, temp);
					free(temp);
					// #e
					temp = str_replace("#e", "the empire", lbuf);
					strcpy(lbuf, temp);
					free(temp);
					// #a
					temp = str_replace("#a", "imperial", lbuf);
					strcpy(lbuf, temp);
					free(temp);
				}
				msg_to_char(ch, "%s\r\n", lbuf);
				
				msg_to_char(ch, "Corpse size: %s\r\n", size_types[GET_CORPSE_SIZE(obj)]);
			}
			else if (IS_NPC_CORPSE(obj))
				msg_to_char(ch, "nothing.\r\n");
			else
				msg_to_char(ch, "%s.\r\n", (index = find_player_index_by_idnum(GET_CORPSE_PC_ID(obj))) ? index->fullname : "a player");
			break;
		case ITEM_COINS: {
			msg_to_char(ch, "Contains %s\r\n", money_amount(real_empire(GET_COINS_EMPIRE_ID(obj)), GET_COINS_AMOUNT(obj)));
			break;
		}
		case ITEM_CURRENCY: {
			msg_to_char(ch, "Amount: %d %s\r\n", GET_CURRENCY_AMOUNT(obj), get_generic_string_by_vnum(GET_CURRENCY_VNUM(obj), GENERIC_CURRENCY, WHICH_CURRENCY(GET_CURRENCY_AMOUNT(obj))));
			break;
		}
		case ITEM_MISSILE_WEAPON:
			msg_to_char(ch, "Speed: %.2f\r\n", get_weapon_speed(obj));
			msg_to_char(ch, "Damage: %d (%s+%.2f base dps)\r\n", GET_MISSILE_WEAPON_DAMAGE(obj), (IS_MAGIC_ATTACK(GET_MISSILE_WEAPON_TYPE(obj)) ? "Intelligence" : "Strength"), get_base_dps(obj));
			msg_to_char(ch, "Damage type is %s.\r\n", get_attack_name_by_vnum(GET_MISSILE_WEAPON_TYPE(obj)));
			break;
		case ITEM_AMMO:
			if (GET_AMMO_QUANTITY(obj) > 0) {
				msg_to_char(ch, "Quantity: %d\r\n", GET_AMMO_QUANTITY(obj));
			}
			if (GET_AMMO_DAMAGE_BONUS(obj)) {
				msg_to_char(ch, "Adds %+d damage.\r\n", GET_AMMO_DAMAGE_BONUS(obj));
			}
			
			if (GET_OBJ_AFF_FLAGS(obj) || GET_OBJ_APPLIES(obj)) {
				generic_data *aftype = find_generic(GET_OBJ_VNUM(obj), GENERIC_AFFECT);
				msg_to_char(ch, "Debuff name: %s\r\n", aftype ? GEN_NAME(aftype) : get_generic_name_by_vnum(ATYPE_RANGED_WEAPON));
			}
			break;
		case ITEM_PACK: {
			msg_to_char(ch, "Adds inventory space: %d\r\n", GET_PACK_CAPACITY(obj));
			break;
		}
		case ITEM_POTION: {
			if (GET_POTION_AFFECT(obj) != NOTHING) {
				msg_to_char(ch, "Potion type: %s\r\n", get_generic_name_by_vnum(GET_POTION_AFFECT(obj)));
			}
			if (GET_POTION_COOLDOWN_TYPE(obj) != NOTHING && GET_POTION_COOLDOWN_TIME(obj) > 0) {
				msg_to_char(ch, "Potion cooldown: %d second%s\r\n", GET_POTION_COOLDOWN_TIME(obj), PLURAL(GET_POTION_COOLDOWN_TIME(obj)));
			}
			break;
		}
		case ITEM_WEALTH: {
			msg_to_char(ch, "Adds %d wealth when stored.\r\n", GET_WEALTH_VALUE(obj));
			prettier_sprintbit(GET_WEALTH_MINT_FLAGS(obj), mint_flags_for_identify, part);
			if (*part) {
				msg_to_char(ch, "Additional information: %s\r\n", part);
			}
			break;
		}
		case ITEM_LIGHTER: {
			if (GET_LIGHTER_USES(obj) == UNLIMITED) {
				msg_to_char(ch, "Lighter uses: unlimited\r\n");
			}
			else {
				msg_to_char(ch, "Lighter uses: %d\r\n", GET_LIGHTER_USES(obj));
			}
			break;
		}
		case ITEM_MINIPET: {
			msg_to_char(ch, "Grants minipet: %s%s\r\n", get_mob_name_by_proto(GET_MINIPET_VNUM(obj), TRUE), has_minipet(ch, GET_MINIPET_VNUM(obj)) ? " (owned)" :"");
			break;
		}
		case ITEM_LIGHT: {
			if (GET_LIGHT_HOURS_REMAINING(obj) == UNLIMITED) {
				snprintf(part, sizeof(part), "does not burn out");
			}
			else {
				snprintf(part, sizeof(part), "has %d hour%s of light remaining", GET_LIGHT_HOURS_REMAINING(obj), PLURAL(GET_LIGHT_HOURS_REMAINING(obj)));
			}
			msg_to_char(ch, "It %s (%s).\r\n", part, (GET_LIGHT_IS_LIT(obj) ? "lit" : "unlit"));
			prettier_sprintbit(GET_LIGHT_FLAGS(obj), light_flags_for_identify, part);
			if (*part) {
				msg_to_char(ch, "Additional information: %s\r\n", part);
			}
			break;
		}
	}
	
	// data that isn't type-based:
	if (!simple && OBJ_FLAGGED(obj, OBJ_PLANTABLE) && (cp = crop_proto(GET_OBJ_VAL(obj, VAL_FOOD_CROP_TYPE)))) {
		ordered_sprintbit(GET_CROP_CLIMATE(cp), climate_flags, climate_flags_order, CROP_FLAGGED(cp, CROPF_ANY_LISTED_CLIMATE) ? TRUE : FALSE, lbuf);
		msg_to_char(ch, "Plants %s (%s%s).\r\n", GET_CROP_NAME(cp), GET_CROP_CLIMATE(cp) ? lbuf : "any climate", (CROP_FLAGGED(cp, CROPF_REQUIRES_WATER) ? "; must be near water" : ""));
	}
	
	// interactions
	if (!simple) {
		*lbuf = '\0';
		if (has_interaction(GET_OBJ_INTERACTIONS(obj), INTERACT_COMBINE)) {
			snprintf(lbuf + strlen(lbuf), sizeof(lbuf) - strlen(lbuf), "%s combined", *lbuf ? "," : "");
		}
		if (has_interaction(GET_OBJ_INTERACTIONS(obj), INTERACT_SEPARATE)) {
			snprintf(lbuf + strlen(lbuf), sizeof(lbuf) - strlen(lbuf), "%s separated", *lbuf ? "," : "");
		}
		if (CAN_LIGHT_OBJ(obj)) {
			snprintf(lbuf + strlen(lbuf), sizeof(lbuf) - strlen(lbuf), "%s lit", *lbuf ? "," : "");
		}
		if (has_interaction(GET_OBJ_INTERACTIONS(obj), INTERACT_SCRAPE)) {
			snprintf(lbuf + strlen(lbuf), sizeof(lbuf) - strlen(lbuf), "%s scraped", *lbuf ? "," : "");
		}
		if (has_interaction(GET_OBJ_INTERACTIONS(obj), INTERACT_SAW)) {
			snprintf(lbuf + strlen(lbuf), sizeof(lbuf) - strlen(lbuf), "%s sawed", *lbuf ? "," : "");
		}
		if (has_interaction(GET_OBJ_INTERACTIONS(obj), INTERACT_TAN)) {
			snprintf(lbuf + strlen(lbuf), sizeof(lbuf) - strlen(lbuf), "%s tanned", *lbuf ? "," : "");
		}
		if (has_interaction(GET_OBJ_INTERACTIONS(obj), INTERACT_CHIP)) {
			snprintf(lbuf + strlen(lbuf), sizeof(lbuf) - strlen(lbuf), "%s chipped", *lbuf ? "," : "");
		}
		if (has_interaction(GET_OBJ_INTERACTIONS(obj), INTERACT_SEED) && !OBJ_FLAGGED(obj, OBJ_SEEDED)) {
			snprintf(lbuf + strlen(lbuf), sizeof(lbuf) - strlen(lbuf), "%s seeded", *lbuf ? "," : "");
		}
		
		// show it
		if (*lbuf) {
			msg_to_char(ch, "It can be:%s\r\n", lbuf);
		}
	}
	
	*lbuf = '\0';
	for (apply = GET_OBJ_APPLIES(obj); apply; apply = apply->next) {
		if (apply->apply_type != APPLY_TYPE_NATURAL) {
			sprintf(part, " (%s)", apply_type_names[(int)apply->apply_type]);
		}
		else {
			*part = '\0';
		}
		sprintf(lbuf + strlen(lbuf), "%s%+d to %s%s", (*lbuf != '\0') ? ", " : "", apply->modifier, apply_types[(int) apply->location], part);
	}
	if (*lbuf) {
		msg_to_char(ch, "Modifiers: %s\r\n", lbuf);
	}
	
	// component info
	if (!simple && GET_OBJ_COMPONENT(obj) != NOTHING && (comp = find_generic(GET_OBJ_COMPONENT(obj), GENERIC_COMPONENT))) {
		msg_to_char(ch, "Component type: %s%s\r\n", GEN_NAME(comp), GEN_FLAGGED(comp, GEN_BASIC) ? " (basic)" : "");
	
		if (GEN_COMPUTED_RELATIONS(comp)) {
			get_generic_relation_display(GEN_COMPUTED_RELATIONS(comp), FALSE, lbuf, "Can be used as: ");
			msg_to_char(ch, "%s", lbuf);
		}
	}
	
	// recipes
	if (!simple && GET_OBJ_VNUM(obj) != NOTHING) {
		size = line_size = snprintf(lbuf, sizeof(lbuf), "Allows: ");
		any = FALSE;
		HASH_ITER(sorted_hh, sorted_crafts, craft, next_craft) {
			if (GET_CRAFT_REQUIRES_OBJ(craft) == GET_OBJ_VNUM(obj) && !CRAFT_FLAGGED(craft, CRAFT_IN_DEVELOPMENT | CRAFT_DISMANTLE_ONLY | CRAFT_UPGRADE)) {
				part_size = snprintf(part, sizeof(part), "%s %s", gen_craft_data[GET_CRAFT_TYPE(craft)].command, GET_CRAFT_NAME(craft));
			
				if (line_size + part_size + 2 >= 80 && part_size < 75 && any) {
					size += snprintf(lbuf + size, sizeof(lbuf) - size, ",\r\n%s", part);
					line_size = part_size;
				}
				else {
					size += snprintf(lbuf + size, sizeof(lbuf) - size, "%s%s", (any ? ", " : ""), part);
					line_size += part_size;
				}
			
				any = TRUE;
			}
		}
		if (any) {
			msg_to_char(ch, "%s\r\n", lbuf);
		}
	}
	
	// some custom messages
	if (!simple) {
		LL_FOREACH(GET_OBJ_CUSTOM_MSGS(obj), ocm) {
			switch (ocm->type) {
				case OBJ_CUSTOM_LONGDESC: {
					sprintf(lbuf, "Gives long description: %s", ocm->msg);
					temp = str_replace("$n", "$o", lbuf);
					strcpy(lbuf, temp);
					free(temp);
					act(lbuf, FALSE, ch, NULL, NULL, TO_CHAR);
					break;
				}
				case OBJ_CUSTOM_LONGDESC_FEMALE: {
					if (GET_SEX(ch) == SEX_FEMALE) {
						sprintf(lbuf, "Gives long description: %s", ocm->msg);
						temp = str_replace("$n", "$o", lbuf);
						strcpy(lbuf, temp);
						free(temp);
						act(lbuf, FALSE, ch, NULL, NULL, TO_CHAR);
					}
					break;
				}
				case OBJ_CUSTOM_LONGDESC_MALE: {
					if (GET_SEX(ch) == SEX_MALE) {
						sprintf(lbuf, "Gives long description: %s", ocm->msg);
						temp = str_replace("$n", "$o", lbuf);
						strcpy(lbuf, temp);
						free(temp);
						act(lbuf, FALSE, ch, NULL, NULL, TO_CHAR);
					}
					break;
				}
			}
		}
	}
	
	// expiry?
	if (GET_OBJ_TIMER(obj) > 0) {
		if (GET_OBJ_TIMER(obj) <= 12 && OBJ_IS_IN_WORLD(obj)) {
			msg_to_char(ch, "Expires in %d hour%s.\r\n", GET_OBJ_TIMER(obj), PLURAL(GET_OBJ_TIMER(obj)));
		}
		else if (GET_OBJ_TIMER(obj) <= 24) {
			msg_to_char(ch, "Expires within a day.\r\n");
		}
		else {
			// longer than 24 hours
			msg_to_char(ch, "This item will expire within %d days.\r\n", (int) ceil(GET_OBJ_TIMER(obj) / 24.0));
		}
	}
	
	// equipment sets?
	if (!simple && GET_OBJ_EQ_SETS(obj)) {
		any = FALSE;
		msg_to_char(ch, "Equipment sets:");
		LL_FOREACH(GET_OBJ_EQ_SETS(obj), oset) {
			if ((pset = get_eq_set_by_id(ch, oset->id))) {
				msg_to_char(ch, "%s%s (%s)", any ? ", " : " ", pset->name, wear_data[oset->pos].name);
				any = TRUE;
			}
		}
		msg_to_char(ch, "%s\r\n", any ? "" : " none");
	}
	
	// this only happens if they identify it somewhere that identifies-to can't happen
	if (has_interaction(GET_OBJ_INTERACTIONS(obj), INTERACT_IDENTIFIES_TO)) {
		msg_to_char(ch, "This item has a chance to become something else when identified in your inventory.\r\n");
	}
}


/**
* Shows an "identify" of the vehicle.
*
* @param vehicle_data *veh The vehicle to id.
* @param char_data *ch The person to show the data to.
*/
void identify_vehicle_to_char(vehicle_data *veh, char_data *ch) {
	bitvector_t show_flags;
	vehicle_data *proto = vehicle_proto(VEH_VNUM(veh));
	char buf[MAX_STRING_LENGTH], buf1[256];
	player_index_data *index;
	
	// basic info
	act("Your analysis of $V reveals:", FALSE, ch, NULL, veh, TO_CHAR | ACT_VEH_VICT);
	
	if (VEH_OWNER(veh)) {
		msg_to_char(ch, "Owner: %s%s\t0\r\n", EMPIRE_BANNER(VEH_OWNER(veh)), EMPIRE_NAME(VEH_OWNER(veh)));
	}
	if (VEH_MAX_ROOMS(veh) > 0 && (!VEH_OWNER(veh) || VEH_OWNER(veh) == GET_LOYALTY(ch) || IS_IMMORTAL(ch))) {
		// add +1 for the base room
		msg_to_char(ch, "Rooms: %d/%d\r\n", VEH_INSIDE_ROOMS(veh) + 1, VEH_MAX_ROOMS(veh) + 1);
	}
	
	msg_to_char(ch, "Type: %s\r\n", skip_filler((proto && !strchr(VEH_SHORT_DESC(proto), '#')) ? VEH_SHORT_DESC(proto) : VEH_SHORT_DESC(veh)));
	msg_to_char(ch, "Level: %d\r\n", VEH_SCALE_LEVEL(veh));
	
	if (VEH_SIZE(veh) > 0) {
		msg_to_char(ch, "Size: %d%% of tile\r\n", VEH_SIZE(veh) * 100 / config_get_int("vehicle_size_per_tile"));
	}
	
	if (VEH_FLAGGED(veh, MOVABLE_VEH_FLAGS)) {
		msg_to_char(ch, "Speed: %s\r\n", vehicle_speed_types[VEH_SPEED_BONUSES(veh)]);
	}
	
	if (VEH_CAPACITY(veh) > 0) {
		msg_to_char(ch, "Capacity: %d/%d\r\n", VEH_CARRYING_N(veh), VEH_CAPACITY(veh));
	}
	
	if (VEH_PATRON(veh) && (index = find_player_index_by_idnum(VEH_PATRON(veh)))) {
		msg_to_char(ch, "Dedicated to: %s\r\n", index->fullname);
	}
	
	if (VEH_PAINT_COLOR(veh)) {
		sprinttype(VEH_PAINT_COLOR(veh), paint_names, buf, sizeof(buf), "UNDEFINED");
		sprinttype(VEH_PAINT_COLOR(veh), paint_colors, buf1, sizeof(buf1), "&0");
		*buf = LOWER(*buf);
		if (VEH_FLAGGED(veh, VEH_BRIGHT_PAINT)) {
			strtoupper(buf1);
		}
		msg_to_char(ch, "Paint color: %s%s%s&0\r\n", buf1, (VEH_FLAGGED(veh, VEH_BRIGHT_PAINT) ? "bright " : ""), buf);
	}
	
	// flags as "notes":
	show_flags = VEH_FLAGS(veh);
	if (VEH_FLAGGED(veh, VEH_BUILDING)) {
		// do not show these on 'building' vehicles as they are very common and don't make sense in context
		REMOVE_BIT(show_flags, VEH_NO_BUILDING | VEH_NO_LOAD_ONTO_VEHICLE);
	}
	prettier_sprintbit(show_flags, identify_vehicle_flags, buf);
	if (VEH_FLAGGED(veh, VEH_SIT)) {
		sprintf(buf + strlen(buf), "%scan sit %s", *buf ? ", " : "", VEH_FLAGGED(veh, VEH_IN) ? "in" : "on");
	}
	if (VEH_FLAGGED(veh, VEH_SLEEP)) {
		sprintf(buf + strlen(buf), "%scan sleep %s", *buf ? ", " : "", VEH_FLAGGED(veh, VEH_IN) ? "in" : "on");
	}
	if (*buf) {
		msg_to_char(ch, "Notes: %s\r\n", buf);
	}
}


// INTERACTION_FUNC provides: ch, interaction, inter_room, inter_mob, inter_item, inter_veh
INTERACTION_FUNC(light_obj_interact) {	
	obj_vnum vnum = interaction->vnum;
	obj_data *new = NULL;
	int num, obj_ok = 0;
	
	for (num = 0; num < interaction->quantity; ++num) {
		// load
		new = read_object(vnum, TRUE);
		scale_item_to_level(new, GET_OBJ_CURRENT_SCALE_LEVEL(inter_item));
		
		// ownership
		new->last_owner_id = GET_IDNUM(ch);
		new->last_empire_id = GET_LOYALTY(ch) ? EMPIRE_VNUM(GET_LOYALTY(ch)) : NOTHING;
		
		// put it somewhere
		if (CAN_WEAR(new, ITEM_WEAR_TAKE)) {
			obj_to_char(new, ch);
		}
		else {
			obj_to_room(new, IN_ROOM(ch));
		}
		obj_ok = load_otrigger(new);
		if (obj_ok && new->carried_by) {
			get_otrigger(new, new->carried_by, FALSE);
		}
	}
	
	// mark gained
	if (GET_LOYALTY(ch)) {
		add_production_total(GET_LOYALTY(ch), vnum, interaction->quantity);
	}

	if (interaction->quantity > 1) {
		sprintf(buf1, "It is now $p (x%d).", interaction->quantity);
	}
	else {
		strcpy(buf1, "It is now $p.");
	}
		
	if (obj_ok && new) {
		act(buf1, FALSE, ch, new, NULL, TO_CHAR | TO_ROOM);
	}
	
	if (obj_ok && new && inter_item && IS_AMMO(inter_item) && IS_AMMO(new)) {
		set_obj_val(new, VAL_AMMO_QUANTITY, GET_AMMO_QUANTITY(inter_item));
	}
	
	return TRUE;
}


/**
* Gets the functional size of an object, where each object inside it adds to
* its size. This prevents corpses and containers from being used to hold more
* items than the normal limit.
*/
int obj_carry_size(obj_data *obj) {
	obj_data *iter;
	int size = 0;
	
	// my size
	if (FREE_TO_CARRY(obj)) {
		size = 0;
	}
	else if (OBJ_FLAGGED(obj, OBJ_LARGE)) {
		size = 2;
	}
	else {
		size = 1;
	}
	
	// size of contents
	DL_FOREACH2(obj->contains, iter, next_content) {
		size += obj_carry_size(iter);
	}
	
	return size;
}


/**
* Attempt 1 exchange. Returning FALSE will prevent further exchanges if this is
* in a loop. Returning TRUE will continue, even if no exchange happened here.
*
* @param char_data *ch The player.
* @param obj_data *obj The item to exchange.
* @param empire_data *emp The empire to exchange with.
* @return bool TRUE to continue exchanging items; FALSE to stop here.
*/
static bool perform_exchange(char_data *ch, obj_data *obj, empire_data *emp) {
	double coin_mod = 1 / COIN_VALUE;
	int amt = GET_WEALTH_VALUE(obj) * coin_mod;
	
	if (!IS_WEALTH_ITEM(obj) || GET_WEALTH_VALUE(obj) <= 0 || GET_OBJ_VNUM(obj) == NOTHING) {
		// not exchangable -- continue execution silently
		return TRUE;
	}
	else if (EMPIRE_COINS(emp) < amt) {
		act("The empire doesn't have enough coins to give you for $p.", FALSE, ch, obj, NULL, TO_CHAR);
		return FALSE;	// stop exchanges
	}
	else {
		sprintf(buf, "You exchange $p for %s.", money_amount(emp, amt));
		act(buf, FALSE, ch, obj, NULL, TO_CHAR | TO_QUEUE);
		act("$n exchanges $p for some coins.", TRUE, ch, obj, NULL, TO_ROOM | TO_QUEUE);
		
		increase_coins(ch, emp, amt);
		decrease_empire_coins(emp, emp, amt);
		add_to_empire_storage(emp, GET_ISLAND_ID(IN_ROOM(ch)), GET_OBJ_VNUM(obj), 1, GET_OBJ_TIMER(obj));
		extract_obj(obj);
		return TRUE;
	}
}


/**
* Processes a put-in-container.
*
* @param char_data *ch The putter.
* @param obj_data *obj The item to put.
* @param obj_data *cont The container to put it in.
* @return int 1 = success, 0 = fail
*/
static int perform_put(char_data *ch, obj_data *obj, obj_data *cont) {
	char_data *mort;
	
	if (!drop_otrigger(obj, ch, DROP_TRIG_PUT)) {	// also takes care of obj purging self
		return 0;
	}
	
	// don't let people drop bound items in other people's territory
	if (IN_ROOM(cont) && OBJ_BOUND_TO(obj) && ROOM_OWNER(IN_ROOM(cont)) && ROOM_OWNER(IN_ROOM(cont)) != GET_LOYALTY(ch) && !IS_IMMORTAL(ch)) {
		msg_to_char(ch, "You can't put bound items in items here.\r\n");
		return 0;
	}
	if (GET_OBJ_REQUIRES_QUEST(obj) != NOTHING && !IS_NPC(ch) && !IS_IMMORTAL(ch)) {
		act("$p: you can't put quest items in there.", FALSE, ch, obj, NULL, TO_CHAR);
		return 0;
	}
	
	if (GET_OBJ_CARRYING_N(cont) + obj_carry_size(obj) > GET_MAX_CONTAINER_CONTENTS(cont)) {
		act("$p won't fit in $P.", FALSE, ch, obj, cont, TO_CHAR | ACT_OBJ_VICT);
		return 0;
	}
	else if (OBJ_FLAGGED(obj, OBJ_LARGE) && !OBJ_FLAGGED(cont, OBJ_LARGE) && CAN_WEAR(cont, ITEM_WEAR_TAKE)) {
		act("$p is far too large to fit in $P.", FALSE, ch, obj, cont, TO_CHAR | ACT_OBJ_VICT);
		return 1;	// is this correct? I added it because it was implied by the drop-thru here -pc
	}
	else {
		obj_to_obj(obj, cont);

		act("$n puts $p in $P.", TRUE, ch, obj, cont, TO_ROOM | TO_QUEUE | ACT_OBJ_VICT);

		act("You put $p in $P.", FALSE, ch, obj, cont, TO_CHAR | TO_QUEUE | ACT_OBJ_VICT);

		if (IS_IMMORTAL(ch) && ROOM_OWNER(IN_ROOM(ch)) && !EMPIRE_IMM_ONLY(ROOM_OWNER(IN_ROOM(ch)))) {
			syslog(SYS_GC, GET_ACCESS_LEVEL(ch), TRUE, "ABUSE: %s puts %s into a container in mortal empire (%s) at %s", GET_NAME(ch), GET_OBJ_SHORT_DESC(obj), EMPIRE_NAME(ROOM_OWNER(IN_ROOM(ch))), room_log_identifier(IN_ROOM(ch)));
		}
		else if (IS_IMMORTAL(ch) && (mort = find_mortal_in_room(IN_ROOM(ch)))) {
			syslog(SYS_GC, GET_ACCESS_LEVEL(ch), TRUE, "ABUSE: %s puts %s into a container with mortal present (%s) at %s", GET_NAME(ch), GET_OBJ_SHORT_DESC(obj), GET_NAME(mort), room_log_identifier(IN_ROOM(ch)));
		}
	}
	return 1;
}


/**
* Removes an item from a wear location and gives it to the character (unless
* it is 1-use, in which case it is extracted).
*
* @param char_data *ch The character.
* @param int pos The WEAR_ to remove.
* @return obj_data* A pointer to the object if it was removed (unless it was extracted).
*/
obj_data *perform_remove(char_data *ch, int pos) {
	obj_data *obj = NULL;

	if (!(obj = GET_EQ(ch, pos)))
		log("SYSERR: perform_remove: bad pos %d passed.", pos);
	else if (pos == WEAR_SADDLE && IS_RIDING(ch)) {
		msg_to_char(ch, "You can't remove your saddle while you're riding!\r\n");
	}
	else {
		if (!remove_otrigger(obj, ch)) {
			return NULL;
		}

		// char message
		if (obj_has_custom_message(obj, OBJ_CUSTOM_REMOVE_TO_CHAR)) {
			act(obj_get_custom_message(obj, OBJ_CUSTOM_REMOVE_TO_CHAR), FALSE, ch, obj, NULL, TO_CHAR);
		}
		else {
			act("You stop using $p.", FALSE, ch, obj, NULL, TO_CHAR);
		}
		
		// room message
		if (obj_has_custom_message(obj, OBJ_CUSTOM_REMOVE_TO_ROOM)) {
			act(obj_get_custom_message(obj, OBJ_CUSTOM_REMOVE_TO_ROOM), TRUE, ch, obj, NULL, TO_ROOM);
		}
		else {
			act("$n stops using $p.", TRUE, ch, obj, NULL, TO_ROOM);
		}
		
		// this may extract it, or drop it
		obj = unequip_char_to_inventory(ch, pos);
		determine_gear_level(ch);
	}
	
	return obj;
}


static void perform_wear(char_data *ch, obj_data *obj, int where) {
	char buf[MAX_STRING_LENGTH];
	struct obj_apply *apply;
	int iter, type, val;

	/* first, make sure that the wear position is valid. */
	if (!CAN_WEAR(obj, wear_data[where].item_wear)) {
		act("You can't wear $p there.", FALSE, ch, obj, 0, TO_CHAR);
		return;
	}
	
	// check bindings
	if (!bind_ok(obj, ch)) {
		act("$p is bound to someone else.", FALSE, ch, obj, NULL, TO_CHAR | TO_QUEUE);
		return;
	}
	
	// position cascade (ring 1/2, etc)
	if (GET_EQ(ch, where) && wear_data[where].cascade_pos != NO_WEAR) {
		where = wear_data[where].cascade_pos;
	}
	
	// check 2h
	if (where == WEAR_WIELD && OBJ_FLAGGED(obj, OBJ_TWO_HANDED) && GET_EQ(ch, WEAR_HOLD)) {
		msg_to_char(ch, "You can't wield a two-handed weapon while you're holding something in your off-hand.\r\n");
		return;
	}
	if (where == WEAR_HOLD && GET_EQ(ch, WEAR_WIELD) && OBJ_FLAGGED(GET_EQ(ch, WEAR_WIELD), OBJ_TWO_HANDED)) {
		msg_to_char(ch, "You can't hold an item while wielding a two-handed weapon.\r\n");
		return;
	}
	
	if (where == WEAR_SADDLE && !IS_RIDING(ch)) {
		msg_to_char(ch, "You can't wear a saddle while you're not riding anything.\r\n");
		return;
	}

	if (GET_EQ(ch, where)) {
		act(wear_data[where].already_wearing, FALSE, ch, GET_EQ(ch, where), NULL, TO_CHAR);
		return;
	}
	
	// some checks are only needed when the slot counts for stats
	if (wear_data[where].count_stats) {
		// check uniqueness
		if (OBJ_FLAGGED(obj, OBJ_UNIQUE)) {
			for (iter = 0; iter < NUM_WEARS; ++iter) {
				if (GET_EQ(ch, iter) && GET_OBJ_VNUM(GET_EQ(ch, iter)) == GET_OBJ_VNUM(obj)) {
					act("You are already using another of $p (unique).", FALSE, ch, obj, NULL, TO_CHAR);
					return;
				}
			}
		}
	
		// check weakness (check all applies first, in case they contradict like -1str +2str)
		for (iter = 0; primary_attributes[iter] != NOTHING; ++iter) {
			type = primary_attributes[iter];
			val = GET_ATT(ch, type);
			for (apply = GET_OBJ_APPLIES(obj); apply; apply = apply->next) {
				if (apply_attribute[(int) apply->location] == type) {
					val += apply->modifier;
				}
			}
		
			if (val < 1) {
				sprintf(buf, "You are %s to use $p!", attributes[type].low_error);
				act(buf, FALSE, ch, obj, 0, TO_CHAR);
				return;
			}
		}

		/* See if a trigger disallows it */
		if (!wear_otrigger(obj, ch, where) || (obj->carried_by != ch)) {
			return;
		}
	}

	wear_message(ch, obj, where);
	equip_char(ch, obj, where);
	determine_gear_level(ch);
}


/**
* Removes all armor of a certain type from a character...
*
* @param char_data *ch the character
* @param int armor_type ARMOR_x
*/
void remove_armor_by_type(char_data *ch, int armor_type) {
	bool found = FALSE;
	int iter;
	
	for (iter = 0; iter < NUM_WEARS; ++iter) {
		if (GET_EQ(ch, iter) && GET_ARMOR_TYPE(GET_EQ(ch, iter)) == armor_type) {
			act("You take off $p.", FALSE, ch, GET_EQ(ch, iter), NULL, TO_CHAR);
			unequip_char_to_inventory(ch, iter);
			found = TRUE;
		}
	}
	
	if (found) {
		determine_gear_level(ch);
	}
}


/**
* Removes all honed items from a character's equipment.
*
* @param char_data *ch the character
*/
void remove_honed_gear(char_data *ch) {
	struct obj_apply *app;
	bool found = FALSE;
	int iter;
	
	for (iter = 0; iter < NUM_WEARS; ++iter) {
		if (!GET_EQ(ch, iter)) {
			continue;
		}
		LL_FOREACH(GET_OBJ_APPLIES(GET_EQ(ch, iter)), app) {
			if (app->apply_type == APPLY_TYPE_HONED) {
				act("You take off $p.", FALSE, ch, GET_EQ(ch, iter), NULL, TO_CHAR);
				unequip_char_to_inventory(ch, iter);
				found = TRUE;
				break;
			}
		}
	}
	
	if (found) {
		determine_gear_level(ch);
	}
}


/**
* Interaction func for "seed".
*
* INTERACTION_FUNC provides: ch, interaction, inter_room, inter_mob, inter_item, inter_veh
*/
INTERACTION_FUNC(seed_obj_interact) {
	char to_char[MAX_STRING_LENGTH], to_room[MAX_STRING_LENGTH];
	obj_data *new_obj;
	int iter;
	
	if (interaction->quantity) {
		snprintf(to_char, sizeof(to_char), "You seed %s and get %s (x%d)!", GET_OBJ_SHORT_DESC(inter_item), get_obj_name_by_proto(interaction->vnum), interaction->quantity);
		act(to_char, FALSE, ch, NULL, NULL, TO_CHAR);
		snprintf(to_room, sizeof(to_room), "$n seeds %s and gets %s (x%d)!", GET_OBJ_SHORT_DESC(inter_item), get_obj_name_by_proto(interaction->vnum), interaction->quantity);
		act(to_room, TRUE, ch, NULL, NULL, TO_ROOM);
	}
	else {
		snprintf(to_char, sizeof(to_char), "You seed %s and get %s!", GET_OBJ_SHORT_DESC(inter_item), get_obj_name_by_proto(interaction->vnum));
		act(to_char, FALSE, ch, NULL, NULL, TO_CHAR);
		snprintf(to_room, sizeof(to_room), "$n seeds %s and gets %s!", GET_OBJ_SHORT_DESC(inter_item), get_obj_name_by_proto(interaction->vnum));
		act(to_room, TRUE, ch, NULL, NULL, TO_ROOM);
	}
	
	if (GET_LOYALTY(ch)) {
		// add the gained items to production
		add_production_total(GET_LOYALTY(ch), interaction->vnum, interaction->quantity);
	}
	
	for (iter = 0; iter < interaction->quantity; ++iter) {
		new_obj = read_object(interaction->vnum, TRUE);
		scale_item_to_level(new_obj, GET_OBJ_CURRENT_SCALE_LEVEL(inter_item));
		
		// ownership
		new_obj->last_owner_id = GET_IDNUM(ch);
		new_obj->last_empire_id = GET_LOYALTY(ch) ? EMPIRE_VNUM(GET_LOYALTY(ch)) : NOTHING;
		
		// put it somewhere
		if (CAN_WEAR(new_obj, ITEM_WEAR_TAKE)) {
			obj_to_char(new_obj, ch);
		}
		else {
			obj_to_room(new_obj, IN_ROOM(ch));
		}
		if (load_otrigger(new_obj) && new_obj->carried_by) {
			get_otrigger(new_obj, new_obj->carried_by, FALSE);
		}
	}
	
	return TRUE;
}


/**
* Interaction func for "separate".
*
* INTERACTION_FUNC provides: ch, interaction, inter_room, inter_mob, inter_item, inter_veh
*/
INTERACTION_FUNC(separate_obj_interact) {
	char to_char[MAX_STRING_LENGTH], to_room[MAX_STRING_LENGTH];
	obj_data *new_obj;
	int iter;
	
	// flags to keep on separate
	bitvector_t preserve_flags = OBJ_SEEDED | OBJ_NO_BASIC_STORAGE | OBJ_NO_WAREHOUSE | OBJ_CREATED;
	
	snprintf(to_char, sizeof(to_char), "You separate %s into %s (x%d)!", GET_OBJ_SHORT_DESC(inter_item), get_obj_name_by_proto(interaction->vnum), interaction->quantity);
	act(to_char, FALSE, ch, NULL, NULL, TO_CHAR);
	snprintf(to_room, sizeof(to_room), "$n separates %s into %s (x%d)!", GET_OBJ_SHORT_DESC(inter_item), get_obj_name_by_proto(interaction->vnum), interaction->quantity);
	act(to_room, TRUE, ch, NULL, NULL, TO_ROOM);
	
	if (GET_LOYALTY(ch)) {
		// add the gained items (the original item is subtracted in do_separate)
		add_production_total(GET_LOYALTY(ch), interaction->vnum, interaction->quantity);
	}
	
	for (iter = 0; iter < interaction->quantity; ++iter) {
		new_obj = read_object(interaction->vnum, TRUE);
		scale_item_to_level(new_obj, GET_OBJ_CURRENT_SCALE_LEVEL(inter_item));
	
		if (GET_OBJ_TIMER(new_obj) != UNLIMITED && GET_OBJ_TIMER(inter_item) != UNLIMITED) {
			GET_OBJ_TIMER(new_obj) = MIN(GET_OBJ_TIMER(new_obj), GET_OBJ_TIMER(inter_item));
		}
		
		// copy these flags, if any
		SET_BIT(GET_OBJ_EXTRA(new_obj), (GET_OBJ_EXTRA(inter_item) & preserve_flags));
		
		// ownership
		new_obj->last_owner_id = GET_IDNUM(ch);
		new_obj->last_empire_id = GET_LOYALTY(ch) ? EMPIRE_VNUM(GET_LOYALTY(ch)) : NOTHING;
	
		// put it somewhere
		if (CAN_WEAR(new_obj, ITEM_WEAR_TAKE)) {
			obj_to_char(new_obj, ch);
		}
		else {
			obj_to_room(new_obj, IN_ROOM(ch));
		}
		if (load_otrigger(new_obj) && new_obj->carried_by) {
			get_otrigger(new_obj, new_obj->carried_by, FALSE);
		}
	}
	
	return TRUE;
}


/**
* Gets info on an item for sale in a shop, via "list id <item>".
*
* @param char_data *ch The player.
* @param obj_data *shop_obj A pointer to the prototype of an object for sale.
*/
void do_shop_identify(char_data *ch, obj_data *shop_obj) {
	obj_data *obj;
	
	if (!shop_obj || !(obj = read_object(GET_OBJ_VNUM(shop_obj), FALSE))) {
		msg_to_char(ch, "Unable to identify that item.\r\n");
		return;
	}
	
	// temporarily put it in the room to inherit scale constraints
	obj_to_room(obj, IN_ROOM(ch));
	scale_item_to_level(obj, GET_HIGHEST_KNOWN_LEVEL(ch));
	obj_from_room(obj);
	
	// show id and desc
	identify_obj_to_char(obj, ch, FALSE);
	if (GET_OBJ_ACTION_DESC(obj)) {
		msg_to_char(ch, "%s", GET_OBJ_ACTION_DESC(obj));
	}
	
	// get rid of loaded obj
	extract_obj(obj);
}


/**
* Sends the wear message when a person puts an item on.
*
* @param char_data *ch The person wearing the item.
* @param obj_data *obj The item he's wearing.
* @param int where Any WEAR_ position.
*/
static void wear_message(char_data *ch, obj_data *obj, int where) {
	// char message
	if (wear_data[where].allow_custom_msgs && obj_has_custom_message(obj, OBJ_CUSTOM_WEAR_TO_CHAR)) {
		act(obj_get_custom_message(obj, OBJ_CUSTOM_WEAR_TO_CHAR), FALSE, ch, obj, NULL, TO_CHAR);
	}
	else {
		act(wear_data[where].wear_msg_to_char, FALSE, ch, obj, NULL, TO_CHAR);
	}
	
	// room message
	if (wear_data[where].allow_custom_msgs && obj_has_custom_message(obj, OBJ_CUSTOM_WEAR_TO_ROOM)) {
		act(obj_get_custom_message(obj, OBJ_CUSTOM_WEAR_TO_ROOM), TRUE, ch, obj, NULL, TO_ROOM);
	}
	else {
		act(wear_data[where].wear_msg_to_room, TRUE, ch, obj, NULL, TO_ROOM);
	}
}


/**
* Consumes 1 hour of light on a LIGHT-type object. If this burns out the light
* and it has the JUNK-WHEN-EXPIRED flag, the object is purged.
*
* @param obj_data *obj The light (may be purged).
* @param bool messages If TRUE, will send burn-out messages.
* @return bool Returns FALSE if the light was purged; TRUE in all other cases.
*/
bool use_hour_of_light(obj_data *obj, bool messages) {
	bool was_lit = LIGHT_IS_LIT(obj) ? TRUE : FALSE;
	
	if (IS_LIGHT(obj) && GET_LIGHT_IS_LIT(obj)) {
		// charge 1 hour
		if (GET_LIGHT_HOURS_REMAINING(obj) > 0) {
			set_obj_val(obj, VAL_LIGHT_HOURS_REMAINING, GET_LIGHT_HOURS_REMAINING(obj) - 1);
			
			// and ensure it's set no-store
			SET_BIT(GET_OBJ_EXTRA(obj), OBJ_NO_BASIC_STORAGE);
		}
		
		// turn it off if necessary
		if (GET_LIGHT_HOURS_REMAINING(obj) == 0) {
			set_obj_val(obj, VAL_LIGHT_IS_LIT, 0);
			
			// to-char message
			if ((obj->worn_by || obj->carried_by) && obj_has_custom_message(obj, OBJ_CUSTOM_DECAYS_ON_CHAR)) {
				act(obj_get_custom_message(obj, OBJ_CUSTOM_DECAYS_ON_CHAR), FALSE, obj->worn_by ? obj->worn_by : obj->carried_by, obj, NULL, TO_CHAR);
			}
			else if (obj->worn_by) {
				act("Your light burns out.", FALSE, obj->worn_by, obj, NULL, TO_CHAR);
			}
			else if (obj->carried_by) {
				act("$p burns out.", FALSE, obj->carried_by, obj, NULL, TO_CHAR);
			}
			
			// to-room message(s)
			if (obj->worn_by) {
				act("$n's light burns out.", TRUE, obj->worn_by, obj, NULL, TO_ROOM);
			}
			else if (IN_ROOM(obj) && ROOM_PEOPLE(IN_ROOM(obj))) {
				if (obj_has_custom_message(obj, OBJ_CUSTOM_DECAYS_IN_ROOM)) {
					act(obj_get_custom_message(obj, OBJ_CUSTOM_DECAYS_IN_ROOM), FALSE, ROOM_PEOPLE(IN_ROOM(obj)), obj, NULL, TO_CHAR | TO_ROOM);
				}
				else {
					act("$p burns out.", FALSE, ROOM_PEOPLE(IN_ROOM(obj)), obj, NULL, TO_CHAR | TO_ROOM);
				}
			}
			
			// lights out (ensure it was lit coming in if we do this
			if (was_lit) {
				apply_obj_light(obj, FALSE);
			}
			
			if (LIGHT_FLAGGED(obj, LIGHT_FLAG_JUNK_WHEN_EXPIRED)) {
				extract_obj(obj);
				return FALSE;	// purged
			}
		}
		
		// save soon
		request_obj_save_in_world(obj);
	}
	
	// safe
	return TRUE;
}


/**
* Attempt to learn a minipet. Caution: this may extract the obj.
*
* @param char_data *ch The player trying to learn it.
* @param obj_data *obj The MINIPET item.
*/
void use_minipet_obj(char_data *ch, obj_data *obj) {
	char_data *mob;
	
	if (!ch || IS_NPC(ch)) {
		return;
	}
	else if (!obj || !IS_MINIPET(obj)) {
		msg_to_char(ch, "You can't use that like that.\r\n");
	}
	else if (!(mob = mob_proto(GET_MINIPET_VNUM(obj)))) {
		msg_to_char(ch, "It doesn't seem to do anything when used.\r\n");
	}
	else if (has_minipet(ch, GET_MINIPET_VNUM(obj))) {
		msg_to_char(ch, "You already have that minipet.\r\n");
	}
	else {
		add_minipet(ch, GET_MINIPET_VNUM(obj));
		msg_to_char(ch, "You gain '%s' as a minipet. Use the minipets command to summon it.\r\n", GET_SHORT_DESC(mob));
		act("$n uses $p.", TRUE, ch, obj, NULL, TO_ROOM);
		extract_obj(obj);
	}
}


/**
* Uses up a charge on a lighter (if not unlimited) and sends a message to the
* player if it runs out and is lost.
*
* @param char_data *ch Optional: The person using the lighter (who gets the message when it's used up; may be NULL).
* @param obj_data *obj The lighter being used (may be purged if the last charge is used).
* @return bool TRUE if the item was used up and purged; FALSE if the item was not purged.
*/
bool used_lighter(char_data *ch, obj_data *obj) {
	if (!ch || !obj) {
		return FALSE;	// huh
	}
	if (!IS_LIGHTER(obj) && !IS_LIGHT(obj)) {
		return FALSE;	// not even a lighter
	}
	
	if (ch && !consume_otrigger(obj, ch, OCMD_LIGHT, NULL)) {
		return TRUE;	// trigger kicked us out (return TRUE because possibly used up)
	}
	
	// check binding
	if (ch && !IS_IMMORTAL(ch) && OBJ_FLAGGED(obj, OBJ_BIND_FLAGS)) {
		bind_obj_to_player(obj, ch);
		reduce_obj_binding(obj, ch);
	}
	
	// only lighters (not lights) get used up here
	if (IS_LIGHTER(obj) && GET_LIGHTER_USES(obj) != UNLIMITED) {
		set_obj_val(obj, VAL_LIGHTER_USES, GET_LIGHTER_USES(obj) - 1);	// use 1 charge
		SET_BIT(GET_OBJ_EXTRA(obj), OBJ_NO_BASIC_STORAGE);	// no longer storable
		
		if (GET_LIGHTER_USES(obj) <= 0) {
			if (ch) {
				act("$p is used up, and you throw it away.", FALSE, ch, obj, NULL, TO_CHAR | TO_QUEUE);
			}
			extract_obj(obj);
			return TRUE;	// did use it up
		}
		else {
			request_obj_save_in_world(obj);
		}
	}
	
	return FALSE;	// did not use it up
}


 //////////////////////////////////////////////////////////////////////////////
//// DROP HELPERS ////////////////////////////////////////////////////////////

#define VANISH(mode) (mode == SCMD_JUNK ? " It vanishes in a puff of smoke!" : "")

/**
* Attempts to complete a drop/junk.
*
* @param char_data *ch The person dropping.
* @param obj_data *obj The thing to drop.
* @param byte mode SCMD_DROP or SCMD_JUNK.
* @param const char *sname The verb they are typing ("drop").
* @return int 1 for success, 0 for basic fail, -1 to stop further drops (in a drop all)
*/
int perform_drop(char_data *ch, obj_data *obj, byte mode, const char *sname) {
	bool need_capacity = ROOM_BLD_FLAGGED(IN_ROOM(ch), BLD_ITEM_LIMIT) ? TRUE : FALSE;
	char_data *mort;
	bool logged;
	int size;
	
	if (!drop_otrigger(obj, ch, mode == SCMD_JUNK ? DROP_TRIG_JUNK : DROP_TRIG_DROP)) {
		return 0;
	}

	if ((mode == SCMD_DROP) && !drop_wtrigger(obj, ch, DROP_TRIG_DROP)) {
		return 0;
	}
	
	// can't junk stolen items
	if (mode == SCMD_JUNK && IS_STOLEN(obj)) {
		act("$p: You can't junk stolen items!", FALSE, ch, obj, NULL, TO_CHAR | TO_QUEUE);
		return -1;
	}
	
	// can't junk kept items
	if (mode == SCMD_JUNK && OBJ_FLAGGED(obj, OBJ_KEEP)) {
		act("$p: You can't junk items with (keep).", FALSE, ch, obj, NULL, TO_CHAR | TO_QUEUE);
		return 0;
	}
	
	// don't let people drop bound items in other people's territory
	if (mode != SCMD_JUNK && OBJ_BOUND_TO(obj) && ROOM_OWNER(IN_ROOM(ch)) && ROOM_OWNER(IN_ROOM(ch)) != GET_LOYALTY(ch) && !IS_IMMORTAL(ch)) {
		act("$p: You can't drop bound items here.", FALSE, ch, obj, NULL, TO_CHAR | TO_QUEUE);
		return 0;	// don't break a drop-all
	}
	
	if (GET_OBJ_REQUIRES_QUEST(obj) != NOTHING && !IS_NPC(ch) && !IS_IMMORTAL(ch)) {
		act("$p: You can't drop quest items.", FALSE, ch, obj, NULL, TO_CHAR | TO_QUEUE);
		return 0;
	}
	
	// count items
	if (mode != SCMD_JUNK && need_capacity) {
		size = obj_carry_size(obj);
		if ((size + count_objs_in_room(IN_ROOM(ch))) > config_get_int("room_item_limit")) {
			msg_to_char(ch, "You can't drop any more items here.\r\n");
			return -1;
		}
	}

	sprintf(buf, "You %s $p.%s", sname, VANISH(mode));
	act(buf, FALSE, ch, obj, 0, TO_CHAR | TO_QUEUE);
	sprintf(buf, "$n %ss $p.%s", sname, VANISH(mode));
	act(buf, TRUE, ch, obj, 0, TO_ROOM | TO_QUEUE);

	switch (mode) {
		case SCMD_DROP:
			obj_to_room(obj, IN_ROOM(ch));
			
			// log dropping items in front of mortals
			if (IS_IMMORTAL(ch)) {
				logged = FALSE;
				
				if ((mort = find_mortal_in_room(IN_ROOM(ch)))) {
					syslog(SYS_GC, GET_ACCESS_LEVEL(ch), TRUE, "ABUSE: %s drops %s with mortal present (%s) at %s", GET_NAME(ch), GET_OBJ_SHORT_DESC(obj), GET_NAME(mort), room_log_identifier(IN_ROOM(ch)));
					logged = TRUE;
				}
				
				if (!logged && ROOM_OWNER(IN_ROOM(ch)) && !EMPIRE_IMM_ONLY(ROOM_OWNER(IN_ROOM(ch)))) {
					syslog(SYS_GC, GET_ACCESS_LEVEL(ch), TRUE, "ABUSE: %s drops %s with in mortal empire (%s) at %s", GET_NAME(ch), GET_OBJ_SHORT_DESC(obj), EMPIRE_NAME(ROOM_OWNER(IN_ROOM(ch))), room_log_identifier(IN_ROOM(ch)));
					logged = TRUE;
				}
			}
			
			return (1);
		case SCMD_JUNK:
			extract_obj(obj);
			return (1);
		default:
			log("SYSERR: Incorrect argument %d passed to perform_drop.", mode);
			break;
	}

	return (1);
}


/**
* Drop/junk coins of one type. This can't handle multitypes because the coin
* object must by one type only.
*
* @param char_data *ch The player dropping coins.
* @param empire_data *type The type of coin (by minting empire, or OTHER_COIN).
* @param int amount The number of coins to drop.
* @param byte mode SCMD_DROP, SCMD_JUNK
*/
static void perform_drop_coins(char_data *ch, empire_data *type, int amount, byte mode) {
	struct coin_data *coin;
	char buf[MAX_STRING_LENGTH];
	char_data *iter;
	obj_data *obj;

	if (amount <= 0)
		msg_to_char(ch, "Heh heh heh... we are jolly funny today, eh?\r\n");
	else if (IS_NPC(ch)) {
		msg_to_char(ch, "NPCs can't drop coins.\r\n");
	}
	else if (!(coin = find_coin_entry(GET_PLAYER_COINS(ch), type)) || coin->amount < amount) {
		msg_to_char(ch, "You don't have %s!\r\n", money_amount(type, amount));
	}
	else {
		// decrease coins first
		decrease_coins(ch, type, amount);
		
		if (mode != SCMD_JUNK) {
			/* to prevent coin-bombing */
			command_lag(ch, WAIT_OTHER);
			obj = create_money(type, amount);
			obj_to_char(obj, ch);	// temporarily

			if (!drop_wtrigger(obj, ch, DROP_TRIG_DROP)) {
				// stays in inventory, which is odd, but better than the alternative (a crash if the script purged the object and we extract it here)
				return;
			}

			obj_to_room(obj, IN_ROOM(ch));
			act("You drop $p.", FALSE, ch, obj, NULL, TO_CHAR | TO_QUEUE);
			act("$n drops $p.", FALSE, ch, obj, NULL, TO_ROOM | TO_QUEUE);
			
			// log dropping items in front of mortals
			if (IS_IMMORTAL(ch)) {
				DL_FOREACH2(ROOM_PEOPLE(IN_ROOM(ch)), iter, next_in_room) {
					if (iter != ch && !IS_NPC(iter) && !IS_IMMORTAL(iter)) {
						syslog(SYS_GC, GET_ACCESS_LEVEL(ch), TRUE, "ABUSE: %s drops %s with mortal present (%s) at %s", GET_NAME(ch), GET_OBJ_SHORT_DESC(obj), GET_NAME(iter), room_log_identifier(IN_ROOM(ch)));
						break;
					}
				}
			}
		}
		else {
			snprintf(buf, sizeof(buf), "$n drops %s which disappear%s in a puff of smoke!", money_desc(type, amount), (amount == 1 ? "s" : ""));
			act(buf, FALSE, ch, 0, 0, TO_ROOM | TO_QUEUE);

			stack_msg_to_desc(ch->desc, "You drop %s which disappear%s in a puff of smoke!\r\n", (amount != 1 ? "some coins" : "a coin"), (amount == 1 ? "s" : ""));
		}
	}
}


 //////////////////////////////////////////////////////////////////////////////
//// EQUIPMENT SET HELPERS ///////////////////////////////////////////////////

/**
* Helps a character understand their equipment and what they need.
*
* @param char_data *ch The character.
* @param char *argument Any additional args after "eq anaylze"
*/
void do_eq_analyze(char_data *ch, char *argument) {
	char low_string[MAX_STRING_LENGTH], empty_string[MAX_STRING_LENGTH], quality_string[MAX_STRING_LENGTH];
	size_t low_size, empty_size, quality_size;
	int items = 0, low = 0, empty = 0, quality = 0;
	double average_level, average_quality, slots, total_level, total_quality;
	int pos, my_level;
	obj_data *obj;
	
	#define _obj_quality(obj)  (1 + (OBJ_FLAGGED(obj, OBJ_HARD_DROP) ? 1 : 0) + (OBJ_FLAGGED(obj, OBJ_GROUP_DROP) ? 2 : 0) + (OBJ_FLAGGED(obj, OBJ_SUPERIOR) ? 4 : 0))
	
	if (!ch || !ch->desc) {
		return;
	}
	
	my_level = get_approximate_level(ch);
	slots = total_level = total_quality = 0.0;
	
	// first pass to determine averages and look for empties
	for (pos = 0; pos < NUM_WEARS; ++pos) {
		if (!wear_data[pos].count_stats || wear_data[pos].gear_level_mod < 0.1) {
			continue;	// doesn't count enough to matter
		}
		
		// compute
		slots += wear_data[pos].gear_level_mod;
		if ((obj = GET_EQ(ch, pos))) {
			++items;
			total_level += GET_OBJ_CURRENT_SCALE_LEVEL(obj) * wear_data[pos].gear_level_mod;
			total_quality += _obj_quality(obj) * wear_data[pos].gear_level_mod;
		}
	}
	
	slots = MAX(1, slots);
	average_level = total_level / slots;
	average_quality = total_quality / slots;
	*low_string = *empty_string = *quality_string = '\0';
	low_size = empty_size = quality_size = 0;
	
	// 2nd pass to look for low levels and build errors
	for (pos = 0; pos < NUM_WEARS; ++pos) {
		if (!wear_data[pos].count_stats || wear_data[pos].gear_level_mod < 0.1) {
			continue;	// doesn't count enough to matter
		}
		
		if ((obj = GET_EQ(ch, pos))) {
			// level
			if (GET_OBJ_CURRENT_SCALE_LEVEL(obj) + 64 < my_level) {
				++low;
				if (low_size < sizeof(low_string)) {
					low_size += snprintf(low_string + low_size, sizeof(low_string) - low_size, "%s%s", (*low_string ? ", " : ""), wear_data[pos].name);
				}
			}
			// quality
			if (_obj_quality(obj) < average_quality) {
				++quality;
				if (quality_size < sizeof(quality_string)) {
					quality_size += snprintf(quality_string + quality_size, sizeof(quality_string) - quality_size, "%s%s", (*quality_string ? ", " : ""), wear_data[pos].name);
				}
			}
		}
		else {	// no item
			++empty;
			if (empty_size < sizeof(empty_string)) {
				empty_size += snprintf(empty_string + empty_size, sizeof(empty_string) - empty_size, "%s%s", (*empty_string ? ", " : ""), wear_data[pos].name);
			}
		}
	}
	
	// output:
	msg_to_char(ch, "Equipment analysis:\r\n");
	msg_to_char(ch, "Wearing %d item%s with an average level of %.1f%s.\r\n", items, PLURAL(items), average_level, (average_level + 75 < my_level) ? " (low)" : "");
	if (empty) {
		msg_to_char(ch, "%d empty slot%s: %s\r\n", empty, PLURAL(empty), empty_string);
	}
	if (low) {
		msg_to_char(ch, "%d low-level slot%s: %s\r\n", low, PLURAL(low), low_string);
	}
	if (quality) {
		msg_to_char(ch, "%d lower-quality slot%s: %s\r\n", quality, PLURAL(quality), quality_string);
	}
	
	// help if none of those are low
	if (!empty && !low && !quality) {
		if (average_quality < 1.0) {
			// nothing here -- it's definitely covered by 'empty'
		}
		else if (average_quality < 1.75) {
			msg_to_char(ch, "Suggestion: Team up with other players to defeat higher difficulty mobs for better gear.\r\n");
		}
		else if (average_quality < 2.75) {
			msg_to_char(ch, "Suggestion: Get more group quality (or better) gear.\r\n");
		}
		else if (average_quality < 3.75) {
			msg_to_char(ch, "Suggestion: Get more superior or boss quality gear.\r\n");
		}
		else if (average_quality < 4.5) {
			msg_to_char(ch, "Suggestion: Get more superior gear.\r\n");
		}
		else if (average_level + 75 < my_level) {
			msg_to_char(ch, "Suggestion: Get more gear that's in your level range.\r\n");
		}
		else {
			msg_to_char(ch, "Your equipment looks good.\r\n");
		}
	}
}


/**
* Shows the character their current equipment.
*
* @param char_data *ch The character.
* @param bool show_all If TRUE, shows empty slots.
*/
void do_eq_show_current(char_data *ch, bool show_all) {
	bool found = FALSE;
	int pos;
	
	if (!IS_NPC(ch)) {
		msg_to_char(ch, "You are using (gear level %d):\r\n", GET_GEAR_LEVEL(ch));
	}
	else {
		send_to_char("You are using:\r\n", ch);
	}
	
	for (pos = 0; pos < NUM_WEARS; ++pos) {
		if (GET_EQ(ch, pos)) {
			if (CAN_SEE_OBJ(ch, GET_EQ(ch, pos))) {
				msg_to_char(ch, "%s%s", wear_data[pos].eq_prompt, obj_desc_for_char(GET_EQ(ch, pos), ch, OBJ_DESC_EQUIPMENT));
				found = TRUE;
			}
			else {
				send_to_char(wear_data[pos].eq_prompt, ch);
				send_to_char("Something.\r\n", ch);
				found = TRUE;
			}
		}
		else if (show_all) {
			if (PRF_FLAGGED(ch, PRF_SCREEN_READER)) {
				msg_to_char(ch, "%s(nothing)\r\n", wear_data[pos].eq_prompt);
			}
			else {
				msg_to_char(ch, "%s\r\n", wear_data[pos].eq_prompt);
			}
			found = TRUE;
		}
	}
	if (!found) {
		send_to_char(" Nothing.\r\n", ch);
	}
}


/**
* Changes the character into one of their other equipment sets.
*
* @param char_data *ch The character changing sets.
* @param char *argument The name of an eq set.
*/
void do_eq_change(char_data *ch, char *argument) {
	struct player_eq_set *eq_set;
	obj_data *obj, *next_obj;
	struct eq_set_obj *oset;
	bool any = FALSE;
	int iter;
	
	if (!(eq_set = get_eq_set_by_name(ch, argument))) {
		msg_to_char(ch, "You don't have an equipment set named '%s' to switch to.\r\n", argument);
		return;
	}
	if (GET_POS(ch) == POS_FIGHTING || GET_POS(ch) < POS_RESTING) {
		msg_to_char(ch, "You can't change equipment sets right now.\r\n");
		return;
	}
	
	msg_to_char(ch, "You switch to your '%s' equipment set.\r\n", eq_set->name);
	
	// look for misplaced or unneeded items already equipped
	for (iter = 0; iter < NUM_WEARS; ++iter) {
		if (!wear_data[iter].save_to_eq_set) {
			continue;	// ignore this slot
		}
		if (!(obj = GET_EQ(ch, iter))) {
			continue;	// no item
		}
		
		// move or remove?
		if ((oset = get_obj_eq_set_by_id(obj, eq_set->id))) {
			if (oset->pos == iter) {
				// already in the right place
			}
			else {	// swap
				if (GET_EQ(ch, oset->pos)) {
					// remove old item
					perform_eq_change_unequip(ch, oset->pos);
				}
				// attempt to move this one
				if ((obj = perform_eq_change_unequip(ch, iter))) {
					perform_wear(ch, obj, oset->pos);
					any = TRUE;
				}
			}
		}
		else { // not part of the set
			perform_eq_change_unequip(ch, iter);
		}
	}
	
	// now look for items in inventory to equip
	DL_FOREACH_SAFE2(ch->carrying, obj, next_obj, next_content) {
		if (!(oset = get_obj_eq_set_by_id(obj, eq_set->id))) {
			continue;	// not in set
		}
		
		// attempt to equip it
		if (GET_EQ(ch, oset->pos)) {
			// remove old item
			perform_eq_change_unequip(ch, oset->pos);
		}
		// attempt to equip this one
		perform_wear(ch, obj, oset->pos);
		any = TRUE;
	}
	
	// if we didn't call perform_wear, we MUST determine gear level again at the end
	if (!any) {
		determine_gear_level(ch);
	}
	
	command_lag(ch, WAIT_OTHER);
}


/**
* Deletes an equipment set.
*
* @param char_data *ch The person deleting a set.
* @param char *argument Name of the set to dleete.
*/
void do_eq_delete(char_data *ch, char *argument) {
	struct player_eq_set *eq_set;
	obj_data *obj;
	int iter;
	
	if (!(eq_set = get_eq_set_by_name(ch, argument))) {
		msg_to_char(ch, "You don't have an equipment set named '%s' to delete.\r\n", argument);
		return;
	}
	
	msg_to_char(ch, "You have deleted the equipment set '%s'.\r\n", eq_set->name);
	
	// remove all current gear from that set
	for (iter = 0; iter < NUM_WEARS; ++iter) {
		if ((obj = GET_EQ(ch, iter))) {
			remove_obj_from_eq_set(obj, eq_set->id);
		}
	}
	
	// remove set from inventory gear
	DL_FOREACH2(ch->carrying, obj, next_content) {
		remove_obj_from_eq_set(obj, eq_set->id);
	}
	
	LL_DELETE(GET_EQ_SETS(ch), eq_set);
	free_player_eq_set(eq_set);
	queue_delayed_update(ch, CDU_SAVE);
}


/**
* Lists a character's eq lists.
*
* @param char_data *ch The person viewing the list.
*/
void do_eq_list(char_data *ch, char *argument) {
	struct player_eq_set *eq_set;
	int count = 0;
	
	msg_to_char(ch, "Saved equipment sets:\r\n");
	LL_FOREACH(GET_EQ_SETS(ch), eq_set) {
		++count;
		msg_to_char(ch, " %s\r\n", eq_set->name);
	}
	
	if (!count) {
		msg_to_char(ch, " none\r\n");
	}
}


/**
* Sets the character's current eq as an equipment set.
*
* @param char_data *ch The person setting an eq set.
* @param char *argument The name to set.
*/
void do_eq_set(char_data *ch, char *argument) {
	const char *invalids[] = { "all", "delete", "list", "save", "set", "\n" };
	struct player_eq_set *eq_set;
	int iter, set_id = NOTHING;
	obj_data *obj;
	
	if (!*argument) {
		msg_to_char(ch, "You must specify a name to save this equipment set under.\r\n");
		return;
	}
	else if (strlen(argument) > 78) {
		msg_to_char(ch, "Set name too long. Pick something shorter.\r\n");
		return;
	}
	
	// check invalid words
	for (iter = 0; *invalids[iter] != '\n'; ++iter) {
		if (!str_cmp(argument, invalids[iter])) {
			msg_to_char(ch, "You cannot name an equipment set '%s'.\r\n", invalids[iter]);
			return;
		}
	}
	
	// check alphanumeric
	for (iter = 0; iter < strlen(argument); ++iter) {
		if (argument[iter] != ' ' && !isdigit(argument[iter]) && !isalpha(argument[iter])) {
			msg_to_char(ch, "Equipment set names must be alphanumeric.\r\n");
			return;
		}
	}
	
	// find or create set
	if ((eq_set = get_eq_set_by_name(ch, argument))) {
		set_id = eq_set->id;
	}
	else if (count_eq_sets(ch) > 50) {
		// only check this if it's NOT replacing a set
		// 50 is an arbitrary limit for sanity reasons
		msg_to_char(ch, "You already have too many equipment sets. Delete one first.\r\n");
		return;
	}
	else {
		set_id = add_eq_set_to_char(ch, NOTHING, str_dup(argument));
	}
	
	// put all current gear on that set
	for (iter = 0; iter < NUM_WEARS; ++iter) {
		if ((obj = GET_EQ(ch, iter))) {
			if (wear_data[iter].save_to_eq_set) {
				add_obj_to_eq_set(obj, set_id, iter);
				
				// auto-keep the item when it's added to a set
				if (!OBJ_FLAGGED(obj, OBJ_KEEP)) {
					SET_BIT(GET_OBJ_EXTRA(obj), OBJ_KEEP);
					qt_keep_obj(ch, obj, TRUE);
				}
			}
			else {	// otherwise treat it like inventory
				remove_obj_from_eq_set(obj, set_id);
			}
		}
	}
	
	// remove set from inventory gear
	DL_FOREACH2(ch->carrying, obj, next_content) {
		remove_obj_from_eq_set(obj, set_id);
	}
	
	msg_to_char(ch, "Your current equipment has been saved as '%s'.\r\n", argument);
	queue_delayed_update(ch, CDU_SAVE);
	
	command_lag(ch, WAIT_OTHER);
}


/**
* Unequips an object when it is removed via the "equip" command, as part of a
* set. These items are placed at the END of the player's inventory, not the
* start.
*
* @param char_data *ch The person to unequip.
* @param int pos Which WEAR_ pos to unequip.
* @return obj_data* A pointer to the removed object, if it exists.
*/
obj_data *perform_eq_change_unequip(char_data *ch, int pos) {
	obj_data *obj;
	
	if (GET_EQ(ch, pos)) {
		obj = unequip_char_to_inventory(ch, pos);
		if (obj && obj->carried_by == ch) {	// e.g. not single-use
			// move from start of inventory to end
			DL_DELETE2(ch->carrying, obj, prev_content, next_content);
			DL_APPEND2(ch->carrying, obj, prev_content, next_content);
		}
		return obj;
	}
	
	return NULL;
}


 //////////////////////////////////////////////////////////////////////////////
//// GET HELPERS /////////////////////////////////////////////////////////////

/**
* Processes a get-item-from-container.
*
* @param char_data *ch The person trying to get.
* @param obj_data *obj The item to get.
* @Param obj_data *cont The container the item is in.
* @param int mode A find-obj mode like FIND_OBJ_INV.
* @return bool TRUE if successful, FALSE on fail.
*/
static bool perform_get_from_container(char_data *ch, obj_data *obj, obj_data *cont, int mode) {	
	room_data *home = HOME_ROOM(IN_ROOM(ch));
	empire_data *emp = ROOM_OWNER(home);
	bool stealing = FALSE;
	int idnum = IS_NPC(ch) ? NOBODY : GET_IDNUM(ch);

	if (!bind_ok(cont, ch)) {
		// container is bound
		act("You can't get anything from $p because it is bound to someone else.", FALSE, ch, cont, NULL, TO_CHAR);
		return FALSE;
	}
	if (!bind_ok(obj, ch)) {
		act("$p: item is bound to someone else.", FALSE, ch, obj, NULL, TO_CHAR | TO_QUEUE);
		return TRUE;	// don't break loop
	}
	if (GET_OBJ_REQUIRES_QUEST(obj) != NOTHING && !IS_NPC(ch) && !IS_IMMORTAL(ch) && !is_on_quest(ch, GET_OBJ_REQUIRES_QUEST(obj))) {
		act("$p: you must be on the quest to get this.", FALSE, ch, obj, NULL, TO_CHAR | TO_QUEUE);
		return TRUE;
	}
	if (IN_ROOM(cont) && LAST_OWNER_ID(cont) != idnum && LAST_OWNER_ID(obj) != idnum && (!GET_LOYALTY(ch) || EMPIRE_VNUM(GET_LOYALTY(ch)) != GET_STOLEN_FROM(obj)) && (!GET_LOYALTY(ch) || EMPIRE_VNUM(GET_LOYALTY(ch)) != GET_STOLEN_FROM(cont)) && !can_use_room(ch, IN_ROOM(ch), GUESTS_ALLOWED)) {
		stealing = TRUE;
		
		if (!IS_IMMORTAL(ch) && emp && !can_steal(ch, emp)) {
			// sends own message
			return FALSE;
		}
		if (!PRF_FLAGGED(ch, PRF_STEALTHABLE)) {
			// can_steal() technically checks this, but it isn't always called
			msg_to_char(ch, "You cannot steal because your 'stealthable' toggle is off.\r\n");
			return FALSE;
		}
	}
	if (!IS_NPC(ch) && !CAN_CARRY_OBJ(ch, obj)) {
		act("$p: you can't hold any more items.", FALSE, ch, obj, 0, TO_CHAR | TO_QUEUE);
		return FALSE;
	}
	if (mode == FIND_OBJ_INV || can_take_obj(ch, obj)) {
		if (get_otrigger(obj, ch, TRUE)) {
			// last-minute scaling: scale to its minimum (adventures will override this on their own)
			if (GET_OBJ_CURRENT_SCALE_LEVEL(obj) < 1) {
				scale_item_to_level(obj, GET_OBJ_MIN_SCALE_LEVEL(obj));
			}
			
			obj_to_char(obj, ch);
			act("You get $p from $P.", FALSE, ch, obj, cont, TO_CHAR | TO_QUEUE | ACT_OBJ_VICT);
			act("$n gets $p from $P.", TRUE, ch, obj, cont, TO_ROOM | TO_QUEUE | ACT_OBJ_VICT);
			
			if (stealing) {
				record_theft_log(emp, GET_OBJ_VNUM(obj), 1);
				
				if (emp && IS_IMMORTAL(ch)) {
					syslog(SYS_GC, GET_ACCESS_LEVEL(ch), TRUE, "ABUSE: %s stealing %s from %s", GET_NAME(ch), GET_OBJ_SHORT_DESC(obj), EMPIRE_NAME(emp));
				}
				else if (emp && !player_tech_skill_check_by_ability_difficulty(ch, PTECH_STEAL_COMMAND)) {
					log_to_empire(emp, ELOG_HOSTILITY, "Theft at (%d, %d)", X_COORD(IN_ROOM(ch)), Y_COORD(IN_ROOM(ch)));
				}
				
				if (!IS_IMMORTAL(ch)) {
					GET_STOLEN_TIMER(obj) = time(0);
					GET_STOLEN_FROM(obj) = emp ? EMPIRE_VNUM(emp) : NOTHING;
					trigger_distrust_from_stealth(ch, emp);
					gain_player_tech_exp(ch, PTECH_STEAL_COMMAND, 50);
					add_offense(emp, OFFENSE_STEALING, ch, IN_ROOM(ch), offense_was_seen(ch, emp, NULL) ? OFF_SEEN : NOBITS);
				}
				run_ability_hooks_by_player_tech(ch, PTECH_STEAL_COMMAND, NULL, obj, NULL, NULL);
			}
			else if (IS_STOLEN(obj) && GET_LOYALTY(ch) && GET_STOLEN_FROM(obj) == EMPIRE_VNUM(GET_LOYALTY(ch))) {
				// un-steal if this was the original owner
				GET_STOLEN_TIMER(obj) = 0;
			}
			
			get_check_money(ch, obj);
			return TRUE;
		}
	}
	return TRUE;	// return TRUE even though it failed -- don't break "get all" loops
}


/**
* Get helper for getting from container.
*
* @param char_data *ch Person trying to get from container.
* @param obj_data *cont The container.
* @param char *arg The typed argument.
* @param int mode Passed through to perform_get_from_container.
* @param int howmany Number to get.
*/
static void get_from_container(char_data *ch, obj_data *cont, char *arg, int mode, int howmany) {
	obj_data *obj, *next_obj;
	int obj_dotmode, found = 0;

	obj_dotmode = find_all_dots(arg);

	if (GET_OBJ_TYPE(cont) == ITEM_CONTAINER && OBJVAL_FLAGGED(cont, CONT_CLOSED))
		act("$p is closed.", FALSE, ch, cont, 0, TO_CHAR);
	else if (obj_dotmode == FIND_INDIV) {
		if (!(obj = get_obj_in_list_vis(ch, arg, NULL, cont->contains))) {
			sprintf(buf, "There doesn't seem to be %s %s in $p.", AN(arg), arg);
			act(buf, FALSE, ch, cont, 0, TO_CHAR);
		}
		else {
			obj_data *obj_next;
			while(obj && howmany--) {
				obj_next = obj->next_content;
				if (!perform_get_from_container(ch, obj, cont, mode))
					break;
				obj = get_obj_in_list_vis(ch, arg, NULL, obj_next);
			}
		}
	}
	else {
		if (obj_dotmode == FIND_ALLDOT && !*arg) {
			send_to_char("Get all of what?\r\n", ch);
			return;
		}
		DL_FOREACH_SAFE2(cont->contains, obj, next_obj, next_content) {
			if (CAN_SEE_OBJ(ch, obj) && (obj_dotmode == FIND_ALL || isname(arg, GET_OBJ_KEYWORDS(obj)))) {
				found = 1;
				if (!perform_get_from_container(ch, obj, cont, mode))
					break;
			}
		}
		if (!found) {
			if (obj_dotmode == FIND_ALL)
				act("$p seems to be empty.", FALSE, ch, cont, 0, TO_CHAR);
			else {
				sprintf(buf, "You can't seem to find any %ss in $p.", arg);
				act(buf, FALSE, ch, cont, 0, TO_CHAR);
			}
		}
	}
}


/**
* Process actual get of an item from the ground.
*
* @param char_data *ch The getter.
* @param obj_data *obj The item to get.
* @return bool TRUE if he succeeds, FALSE if it fails.
*/
static bool perform_get_from_room(char_data *ch, obj_data *obj) {
	room_data *home = HOME_ROOM(IN_ROOM(ch));
	empire_data *emp = ROOM_OWNER(home);
	bool stealing = FALSE;
	int idnum = IS_NPC(ch) ? NOBODY : GET_IDNUM(ch);

	if (!bind_ok(obj, ch)) {
		act("$p: item is bound to someone else.", FALSE, ch, obj, NULL, TO_CHAR | TO_QUEUE);
		return TRUE;	// don't break loop
	}
	// check for stealing -- we check not-bound-to because you can always get an item bound to you
	if (!OBJ_BOUND_TO(obj) && LAST_OWNER_ID(obj) != idnum && (!GET_LOYALTY(ch) || EMPIRE_VNUM(GET_LOYALTY(ch)) != GET_STOLEN_FROM(obj)) && !can_use_room(ch, IN_ROOM(ch), GUESTS_ALLOWED)) {
		stealing = TRUE;
		
		if (!IS_IMMORTAL(ch) && emp && !can_steal(ch, emp)) {
			// sends own message
			return FALSE;
		}
		if (!PRF_FLAGGED(ch, PRF_STEALTHABLE)) {
			// can_steal() technically checks this, but it isn't always called
			msg_to_char(ch, "You cannot steal because your 'stealthable' toggle is off.\r\n");
			return FALSE;
		}
	}
	if (GET_OBJ_REQUIRES_QUEST(obj) != NOTHING && !IS_NPC(ch) && !IS_IMMORTAL(ch) && !is_on_quest(ch, GET_OBJ_REQUIRES_QUEST(obj))) {
		act("$p: you must be on the quest to get this.", FALSE, ch, obj, NULL, TO_CHAR | TO_QUEUE);
		return TRUE;
	}
	if (!IS_NPC(ch) && !CAN_CARRY_OBJ(ch, obj)) {
		act("$p: you can't hold any more items.", FALSE, ch, obj, 0, TO_CHAR | TO_QUEUE);
		return FALSE;
	}
	if (can_take_obj(ch, obj) && get_otrigger(obj, ch, TRUE)) {
		// last-minute scaling: scale to its minimum (adventures will override this on their own)
		if (GET_OBJ_CURRENT_SCALE_LEVEL(obj) < 1) {
			scale_item_to_level(obj, GET_OBJ_MIN_SCALE_LEVEL(obj));
		}
		
		obj_to_char(obj, ch);
		act("You get $p.", FALSE, ch, obj, 0, TO_CHAR | TO_QUEUE);
		act("$n gets $p.", TRUE, ch, obj, 0, TO_ROOM | TO_QUEUE);
					
		if (stealing) {
			record_theft_log(emp, GET_OBJ_VNUM(obj), 1);
			
			if (emp && IS_IMMORTAL(ch)) {
				syslog(SYS_GC, GET_ACCESS_LEVEL(ch), TRUE, "ABUSE: %s stealing %s from %s", GET_NAME(ch), GET_OBJ_SHORT_DESC(obj), EMPIRE_NAME(emp));
			}
			else if (emp && !player_tech_skill_check_by_ability_difficulty(ch, PTECH_STEAL_COMMAND)) {
				log_to_empire(emp, ELOG_HOSTILITY, "Theft at (%d, %d)", X_COORD(IN_ROOM(ch)), Y_COORD(IN_ROOM(ch)));
			}
			
			if (!IS_IMMORTAL(ch)) {
				GET_STOLEN_TIMER(obj) = time(0);
				GET_STOLEN_FROM(obj) = emp ? EMPIRE_VNUM(emp) : NOTHING;
				trigger_distrust_from_stealth(ch, emp);
				gain_player_tech_exp(ch, PTECH_STEAL_COMMAND, 50);
				add_offense(emp, OFFENSE_STEALING, ch, IN_ROOM(ch), offense_was_seen(ch, emp, NULL) ? OFF_SEEN : NOBITS);
			}
			
			run_ability_hooks_by_player_tech(ch, PTECH_STEAL_COMMAND, NULL, obj, NULL, NULL);
		}
		else if (IS_STOLEN(obj) && GET_LOYALTY(ch) && GET_STOLEN_FROM(obj) == EMPIRE_VNUM(GET_LOYALTY(ch))) {
			// un-steal if this was the original owner
			GET_STOLEN_TIMER(obj) = 0;
		}
		
		get_check_money(ch, obj);
		return TRUE;
	}
	return TRUE;	// return TRUE even though it failed -- don't break "get all" loops
}


/**
* Get helper for getting from the room.
*
* @param char_data *ch The getter.
* @param char *arg The typed argument.
* @param int howmany The number to get.
*/
static void get_from_room(char_data *ch, char *arg, int howmany) {
	obj_data *obj, *next_obj;
	int dotmode, found = 0;
	vehicle_data *veh;

	dotmode = find_all_dots(arg);

	if (dotmode == FIND_INDIV) {
		if (!(obj = get_obj_in_list_vis(ch, arg, NULL, ROOM_CONTENTS(IN_ROOM(ch))))) {
			// was it a vehicle, not an object?
			if ((veh = get_vehicle_in_room_vis(ch, arg, NULL))) {
				act("$V: you can't take that!", FALSE, ch, NULL, veh, TO_CHAR | ACT_VEH_VICT);
			}
			else {
				msg_to_char(ch, "You don't see %s %s here.\r\n", AN(arg), arg);
			}
		}
		else {
			obj_data *obj_next;
			while (obj && howmany--) {
				obj_next = obj->next_content;
				if (!perform_get_from_room(ch, obj)) {
					break;
				}
				obj = get_obj_in_list_vis(ch, arg, NULL, obj_next);
			}
		}
	}
	else {
		if (dotmode == FIND_ALLDOT && !*arg) {
			send_to_char("Get all of what?\r\n", ch);
			return;
		}
		DL_FOREACH_SAFE2(ROOM_CONTENTS(IN_ROOM(ch)), obj, next_obj, next_content) {
			if (CAN_SEE_OBJ(ch, obj) && (dotmode == FIND_ALL || isname(arg, GET_OBJ_KEYWORDS(obj)))) {
				found = 1;
				if (!perform_get_from_room(ch, obj)) {
					break;
				}
			}
		}
		if (!found) {
			if (dotmode == FIND_ALL) {
				msg_to_char(ch, "There doesn't seem to be anything here%s.\r\n", (ROOM_VEHICLES(IN_ROOM(ch)) ? " you can take" : ""));
			}
			else if (*arg && (veh = get_vehicle_in_room_vis(ch, arg, NULL))) {
				// tried to get a vehicle
				act("$V: you can't take that!", FALSE, ch, NULL, veh, TO_CHAR | ACT_VEH_VICT);
			}
			else {
				sprintf(buf, "You don't see any %ss here.\r\n", arg);
				send_to_char(buf, ch);
			}
		}
	}
}


 //////////////////////////////////////////////////////////////////////////////
//// GIVE HELPERS ////////////////////////////////////////////////////////////

/* utility function for give */
static char_data *give_find_vict(char_data *ch, char *arg) {
	char_data *vict;

	if (!*arg)
		send_to_char("To whom?\r\n", ch);
	else if (!(vict = get_char_vis(ch, arg, NULL, FIND_CHAR_ROOM)))
		send_config_msg(ch, "no_person");
	else if (vict == ch)
		send_to_char("What's the point of that?\r\n", ch);
	else
		return (vict);
	return NULL;
}


static void perform_give(char_data *ch, char_data *vict, obj_data *obj) {
	if (!give_otrigger(obj, ch, vict)) {
		return;
	}
	if (!receive_mtrigger(vict, ch, obj)) {
		return;
	}
	
	if (IS_NPC(vict) && AFF_FLAGGED(vict, AFF_CHARM)) {
		msg_to_char(ch, "You cannot give items to charmed NPCs.\r\n");
		return;
	}
	
	if (!bind_ok(obj, vict)) {
		act("$p: item is bound.", FALSE, ch, obj, vict, TO_CHAR | TO_QUEUE);
		return;
	}
	
	if (GET_OBJ_REQUIRES_QUEST(obj) != NOTHING && !IS_NPC(ch) && !IS_IMMORTAL(ch) && !IS_IMMORTAL(vict)) {
		act("$p: you can't give this item away.", FALSE, ch, obj, NULL, TO_CHAR | TO_QUEUE);
		return;
	}
	
	// NPCs usually have no carry limit, but 'give' is an exception because otherwise crazy ensues
	if (!CAN_CARRY_OBJ(vict, obj)) {
		act("$N seems to have $S hands full.", FALSE, ch, 0, vict, TO_CHAR | TO_QUEUE);
		return;
	}

	obj_to_char(obj, vict);
	
	if (IS_IMMORTAL(ch) && !IS_IMMORTAL(vict)) {
		syslog(SYS_GC, GET_ACCESS_LEVEL(ch), TRUE, "ABUSE: %s gives %s to %s", GET_NAME(ch), GET_OBJ_SHORT_DESC(obj), PERS(vict, vict, TRUE));
	}
	
	act("You give $p to $N.", FALSE, ch, obj, vict, TO_CHAR | TO_QUEUE);
	act("$n gives you $p.", FALSE, ch, obj, vict, TO_VICT | TO_QUEUE);
	act("$n gives $p to $N.", TRUE, ch, obj, vict, TO_NOTVICT | TO_QUEUE);
	
	if (IS_STOLEN(obj) && !IS_NPC(vict) && GET_LOYALTY(vict) && GET_STOLEN_FROM(obj) == EMPIRE_VNUM(GET_LOYALTY(vict))) {
		// un-steal if this was the original owner
		GET_STOLEN_TIMER(obj) = 0;
	}
}


/**
* @param char_data *ch The person trying to give coins.
* @param char_data *vict The person we're giving the coins to.
* @param empire_data *type The empire whose coins to give (or OTHER_COIN to give any/multiple).
* @param int amount How many to give.
*/
static void perform_give_coins(char_data *ch, char_data *vict, empire_data *type, int amount) {
	char buf[MAX_STRING_LENGTH];
	struct coin_data *coin = NULL;
	int remaining, this;
	obj_data *obj;

	if (IS_NPC(ch)) {
		msg_to_char(ch, "NPCs can't give coins.\r\n");
		return;
	}
	if (amount <= 0) {
		msg_to_char(ch, "Heh heh heh ... we are jolly funny today, eh?\r\n");
		return;
	}
	
	if (!type) {
		// check all coins if they're giving generic
		if (!can_afford_coins(ch, type, amount)) {
			msg_to_char(ch, "You don't have enough coins!\r\n");
			return;
		}
		if (!bribe_mtrigger(vict, ch, amount)) {
			return;
		}
		
		// give/take various types until done
		remaining = amount;
				
		// try the "other" coins first
		if ((coin = find_coin_entry(GET_PLAYER_COINS(ch), REAL_OTHER_COIN))) {
			this = MIN(coin->amount, remaining);
			decrease_coins(ch, REAL_OTHER_COIN, this);
			if (IS_NPC(vict)) {
				obj = create_money(REAL_OTHER_COIN, this);
				obj_to_char(obj, vict);
			}
			else {
				increase_coins(vict, REAL_OTHER_COIN, this);
			}
			remaining -= this;
		}
	
		for (coin = GET_PLAYER_COINS(ch); coin && remaining > 0; coin = coin->next) {
			if (coin->empire_id != OTHER_COIN) {
				this = MIN(coin->amount, remaining);
				decrease_coins(ch, real_empire(coin->empire_id), this);
				if (IS_NPC(vict)) {
					obj = create_money(real_empire(coin->empire_id), this);
					obj_to_char(obj, vict);
				}
				else {
					increase_coins(vict, real_empire(coin->empire_id), this);
				}
				remaining -= this;
			}
		}

		snprintf(buf, sizeof(buf), "$n gives you %d in various coin%s.", amount, amount == 1 ? "" : "s");
		act(buf, FALSE, ch, NULL, vict, TO_VICT | TO_QUEUE);
		// to-room/char messages below
	}
	else {
		// a type of coins was specified
		if (!(coin = find_coin_entry(GET_PLAYER_COINS(ch), type)) || coin->amount < amount) {
			msg_to_char(ch, "You don't have %s.\r\n", money_amount(type, amount));
			return;
		}
		if (!bribe_mtrigger(vict, ch, amount)) {
			return;
		}
		
		// simple money transfer
		decrease_coins(ch, type, amount);
		increase_coins(vict, type, amount);
		
		snprintf(buf, sizeof(buf), "$n gives you %s.", money_amount(type, amount));
		act(buf, FALSE, ch, NULL, vict, TO_VICT | TO_QUEUE);
		// to-room/char messages below
	}
	
	if (IS_IMMORTAL(ch) && !IS_IMMORTAL(vict)) {
		syslog(SYS_GC, GET_ACCESS_LEVEL(ch), TRUE, "ABUSE: %s gives %s to %s", GET_NAME(ch), money_amount(type, amount), PERS(vict, vict, TRUE));
	}
	
	// msg to char
	snprintf(buf, sizeof(buf), "You give %s to $N.", money_desc(type, amount));
	act(buf, FALSE, ch, NULL, vict, TO_CHAR | TO_QUEUE);

	// msg to room
	snprintf(buf, sizeof(buf), "$n gives %s to $N.", money_desc(type, amount));
	act(buf, TRUE, ch, NULL, vict, TO_NOTVICT | TO_QUEUE);
}


 //////////////////////////////////////////////////////////////////////////////
//// LIQUID HELPERS //////////////////////////////////////////////////////////


static bool can_drink_from_room(char_data *ch, byte type) {
	if (type == drink_ROOM) {
		if (!IS_COMPLETE(IN_ROOM(ch))) {
			msg_to_char(ch, "You can't drink anything here until construction is finished!\r\n");
			return FALSE;
		}
		else {
			return TRUE;
		}
	}
	return FALSE;
}


static void drink_message(char_data *ch, obj_data *obj, byte type, int subcmd, int *liq) {
	*liq = LIQ_WATER;

	switch (type) {
		case drink_OBJ: {
			// message to char
			if (subcmd == SCMD_SIP) {
				snprintf(buf, sizeof(buf), "You sip the %s liquid from $p.", get_generic_string_by_vnum(GET_DRINK_CONTAINER_TYPE(obj), GENERIC_LIQUID, GSTR_LIQUID_COLOR));
				act(buf, FALSE, ch, obj, NULL, TO_CHAR);
			}
			else if (obj_has_custom_message(obj, OBJ_CUSTOM_CONSUME_TO_CHAR)) {
				act(obj_get_custom_message(obj, OBJ_CUSTOM_CONSUME_TO_CHAR), FALSE, ch, obj, get_generic_string_by_vnum(GET_DRINK_CONTAINER_TYPE(obj), GENERIC_LIQUID, GSTR_LIQUID_NAME), TO_CHAR | ACT_STR_VICT);
			}
			else {
				msg_to_char(ch, "You drink the %s.\r\n", get_generic_string_by_vnum(GET_DRINK_CONTAINER_TYPE(obj), GENERIC_LIQUID, GSTR_LIQUID_NAME));
			}
			
			// message to room
			if (subcmd == SCMD_SIP) {
				act("$n sips from $p.", TRUE, ch, obj, NULL, TO_ROOM);
			}
			else if (obj_has_custom_message(obj, OBJ_CUSTOM_CONSUME_TO_ROOM)) {
				act(obj_get_custom_message(obj, OBJ_CUSTOM_CONSUME_TO_ROOM), TRUE, ch, obj, get_generic_string_by_vnum(GET_DRINK_CONTAINER_TYPE(obj), GENERIC_LIQUID, GSTR_LIQUID_NAME), TO_ROOM);
			}
			else {
				sprintf(buf, "$n %s from $p.", subcmd == SCMD_SIP ? "sips" : "drinks");
				act(buf, TRUE, ch, obj, NULL, TO_ROOM);
			}

			*liq = GET_DRINK_CONTAINER_TYPE(obj);
			break;
		}
		case drink_ROOM:
		default: {
			msg_to_char(ch, "You take a drink from the cool water.\r\n");
			act("$n takes a drink from the cool water.", TRUE, ch, 0, 0, TO_ROOM);
			break;
		}
	}

	if (subcmd == SCMD_SIP) {
		msg_to_char(ch, "It tastes like %s.\r\n", get_generic_string_by_vnum(*liq, GENERIC_LIQUID, GSTR_LIQUID_NAME));
	}
}


void fill_from_room(char_data *ch, obj_data *obj) {
	int amount = GET_DRINK_CONTAINER_CAPACITY(obj) - GET_DRINK_CONTAINER_CONTENTS(obj);
	int liquid = LIQ_WATER;
	int timer = UNLIMITED;
	
	if (amount <= 0) {
		send_to_char("There is no room for more.\r\n", ch);
		return;
	}

	if (OBJ_FLAGGED(obj, OBJ_SINGLE_USE)) {
		act("You can't put anything in $p.", FALSE, ch, obj, NULL, TO_CHAR);
		return;
	}
	if ((GET_DRINK_CONTAINER_CONTENTS(obj) > 0) && GET_DRINK_CONTAINER_TYPE(obj) != liquid) {
		send_to_char("There is already another liquid in it. Pour it out first.\r\n", ch);
		return;
	}
	
	if (room_has_function_and_city_ok(GET_LOYALTY(ch), IN_ROOM(ch), FNC_DRINK_WATER)) {
		if (!IS_COMPLETE(IN_ROOM(ch))) {
			msg_to_char(ch, "You can't fill your water until it's finished being built.\r\n");
			return;
		}
		act("You gently fill $p with water.", FALSE, ch, obj, 0, TO_CHAR);
		act("$n gently fills $p with water.", TRUE, ch, obj, 0, TO_ROOM);
	}
	else {
		act("You gently fill $p with water.", FALSE, ch, obj, 0, TO_CHAR);
		act("$n gently fills $p with water.", TRUE, ch, obj, 0, TO_ROOM);
	}

	/* First same type liq. */
	set_obj_val(obj, VAL_DRINK_CONTAINER_TYPE, liquid);
	GET_OBJ_TIMER(obj) = timer;
	schedule_obj_timer_update(obj, FALSE);

	set_obj_val(obj, VAL_DRINK_CONTAINER_CONTENTS, GET_DRINK_CONTAINER_CAPACITY(obj));
	request_obj_save_in_world(obj);
}


 //////////////////////////////////////////////////////////////////////////////
//// SCALING /////////////////////////////////////////////////////////////////

/**
* This functions an item that is OBJ_SCALALBLE to a given scale level. The
* formulas used for scaling are contained in this function.
*
* If this is called by an item inside a room that has level constraits (e.g.
* an adventure zone), the level may be constrained by that.
*
* @param obj_data *obj The item to scale.
* @param int level The level to scale it to (may be constrained by the room).
*/
void scale_item_to_level(obj_data *obj, int level) {
	int total_share, bonus, iter, amt;
	int room_lev = 0, room_min = 0, room_max = 0, sig;
	double share, this_share, points_to_give, per_point, base_mod = 1.0;
	room_data *room = NULL;
	obj_data *top_obj, *proto;
	struct obj_apply *apply, *next_apply;
	bool scale_negative = FALSE;
	bitvector_t bits;
	
	// configure this here
	double scale_points_at_100 = config_get_double("scale_points_at_100");
	
	// WEAR_POS_x: modifier based on best wear type
	const double wear_pos_modifier[] = { 0.75, 1.0 };
	
	// it will save no matter what else happens
	request_obj_save_in_world(obj);
	
	// gather info from the item
	switch (GET_OBJ_TYPE(obj)) {
		case ITEM_AMMO:{
			scale_negative = TRUE;
			break;
		}
		case ITEM_POISON: {
			scale_negative = TRUE;
			base_mod = 0.75;
			break;
		}
	}
	
	// determine any scale constraints from the room
	top_obj = get_top_object(obj);
	if (IN_ROOM(top_obj)) {
		room = IN_ROOM(top_obj);
	}
	else if (top_obj->carried_by) {
		room = IN_ROOM(top_obj->carried_by);
	}
	else if (top_obj->in_vehicle) {
		room = IN_ROOM(top_obj->in_vehicle);
	}
	else if (top_obj->worn_by) {
		room = IN_ROOM(top_obj->worn_by);
	}
	
	if (room) {
		get_scale_constraints(room, NULL, &room_lev, &room_min, &room_max);
	}
	
	// if the room is constraining scale, it can ONLY scale to that level
	if (room_lev != 0) {
		level = room_lev;
	}
	
	// ensure within range (prefer obj's own level limits; fall back to room's)
	if (GET_OBJ_MAX_SCALE_LEVEL(obj) > 0) {
		level = MIN(level, GET_OBJ_MAX_SCALE_LEVEL(obj));
	}
	else if (room_max > 0 && !GET_OBJ_MIN_SCALE_LEVEL(obj)) {
		level = MIN(level, room_max);
	}
	
	if (GET_OBJ_MIN_SCALE_LEVEL(obj) > 0) {
		level = MAX(level, GET_OBJ_MIN_SCALE_LEVEL(obj));
	}
	else if (room_min > 0 && !GET_OBJ_MAX_SCALE_LEVEL(obj)) {
		level = MAX(level, room_min);
	}
	
	// hard lower limit -- the stats are the same at 0 or 1, but 0 shows as "unscalable" because unscalable items have 0 scale level
	level = MAX(1, level);
	
	// rounding?
	if (round_level_scaling_to_nearest > 1 && level > 1 && (level % round_level_scaling_to_nearest) > 0) {
		level += (round_level_scaling_to_nearest - (level % round_level_scaling_to_nearest));
	}
	
	// if it's not scalable, we can still set its scale level if the prototype is not scalable
	// (if the prototype IS scalable, but this instance isn't, we can't rescale it this way)
	if (!OBJ_FLAGGED(obj, OBJ_SCALABLE)) {
		if (GET_OBJ_VNUM(obj) != NOTHING && (proto = obj_proto(GET_OBJ_VNUM(obj))) && !OBJ_FLAGGED(proto, OBJ_SCALABLE)) {
			GET_OBJ_CURRENT_SCALE_LEVEL(obj) = level;
		}
		
		// can't do anything else here
		return;
	}
	
	// scale data
	REMOVE_BIT(GET_OBJ_EXTRA(obj), OBJ_SCALABLE);
	GET_OBJ_CURRENT_SCALE_LEVEL(obj) = level;
	
	// remove applies that don't ... apply
	LL_FOREACH_SAFE(GET_OBJ_APPLIES(obj), apply, next_apply) {
		if (apply->apply_type == APPLY_TYPE_SUPERIOR && !OBJ_FLAGGED(obj, OBJ_SUPERIOR)) {
			LL_DELETE(GET_OBJ_APPLIES(obj), apply);
			free(apply);
		}
		else if (apply->apply_type == APPLY_TYPE_HARD_DROP && !OBJ_FLAGGED(obj, OBJ_HARD_DROP)) {
			LL_DELETE(GET_OBJ_APPLIES(obj), apply);
			free(apply);
		}
		else if (apply->apply_type == APPLY_TYPE_GROUP_DROP && !OBJ_FLAGGED(obj, OBJ_GROUP_DROP)) {
			LL_DELETE(GET_OBJ_APPLIES(obj), apply);
			free(apply);
		}
		else if (apply->apply_type == APPLY_TYPE_BOSS_DROP && (!OBJ_FLAGGED(obj, OBJ_HARD_DROP) || !OBJ_FLAGGED(obj, OBJ_GROUP_DROP))) {
			LL_DELETE(GET_OBJ_APPLIES(obj), apply);
			free(apply);
		}
	}
	
	// determine what we're working with
	total_share = 0;	// counting up the amount to split
	bonus = 0;	// extra share for negatives
	
	// helper
	#define SHARE_OR_BONUS(val)  { \
		if ((val) > 0 || scale_negative) { \
			total_share += ABSOLUTE(val); \
		} \
		else if ((val) < 0) { \
			bonus += ceil(ABSOLUTE(val) * apply_values[(int)apply->location]); \
		} \
	}
	// end helper
	
	// first check applies, count share/bonus
	for (apply = GET_OBJ_APPLIES(obj); apply; apply = apply->next) {
		if (!apply_never_scales[(int)apply->location]) {
			SHARE_OR_BONUS(apply->modifier);
		}
	}
	
	// next, count share by type
	switch (GET_OBJ_TYPE(obj)) {
		case ITEM_WEAPON: {
			SHARE_OR_BONUS(GET_WEAPON_DAMAGE_BONUS(obj));
			break;
		}
		case ITEM_DRINKCON: {
			SHARE_OR_BONUS(GET_DRINK_CONTAINER_CAPACITY(obj));
			break;
		}
		case ITEM_COINS: {
			SHARE_OR_BONUS(GET_COINS_AMOUNT(obj));
			break;
		}
		case ITEM_MISSILE_WEAPON: {
			SHARE_OR_BONUS(GET_MISSILE_WEAPON_DAMAGE(obj));
			break;
		}
		case ITEM_AMMO: {
			SHARE_OR_BONUS(GET_AMMO_DAMAGE_BONUS(obj));
			// negative damage bonus IS treated as a bonus, unlike scaling values
			break;
		}
		case ITEM_PACK: {
			SHARE_OR_BONUS(GET_PACK_CAPACITY(obj));
			break;
		}
	}
	
	// anything to scale?
	if (total_share <= 0) {
		return;
	}
	
	// NOW: total_share is the total amount to split, and bonus is extra points because some things are negative
	
	// points_to_give based on level and bonus!
	points_to_give = MAX(1.0, (double) level * scale_points_at_100 / 100) + bonus;
	// armor scaling
	if (IS_ARMOR(obj)) {
		points_to_give *= armor_scale_bonus[GET_ARMOR_TYPE(obj)];
	}
	// scaling based on obj flags
	for (bits = GET_OBJ_EXTRA(obj), iter = 0; bits; bits >>= 1, ++iter) {
		if (IS_SET(bits, BIT(0))) {
			points_to_give *= obj_flag_scaling_bonus[iter];
		}
	}
	// wear-based scaling
	sig = 0;
	for (bits = GET_OBJ_WEAR(obj), iter = 0; bits; bits >>= 1, ++iter) {
		if (IS_SET(bits, BIT(0))) {
			sig = MAX(sig, wear_significance[iter]);
		}
	}
	points_to_give *= wear_pos_modifier[sig];
	points_to_give *= base_mod;	// final modifier
	
	// ok get ready
	points_to_give = MAX(1.0, points_to_give);	// minimum of 1 point
	share = points_to_give / total_share;	// share is how much each +1 point is actually worth
	
	// NOTE: points_to_give always goes down by at least 1 if any points are assigned
	
	// now distribute those points: item values first (weapon damage may be more important!)
	this_share = MAX(0, MIN(share, points_to_give));
	switch (GET_OBJ_TYPE(obj)) {
		case ITEM_WEAPON: {
			// aiming for a dps around the point value
			if (GET_WEAPON_DAMAGE_BONUS(obj) > 0) {
				amt = round(this_share * GET_WEAPON_DAMAGE_BONUS(obj) * get_weapon_speed(obj));
				if (amt > 0) {
					points_to_give -= (this_share * GET_WEAPON_DAMAGE_BONUS(obj));
				}
				set_obj_val(obj, VAL_WEAPON_DAMAGE_BONUS, amt);
			}
			// leave negatives alone
			break;
		}
		case ITEM_DRINKCON: {
			amt = (int)round(this_share * GET_DRINK_CONTAINER_CAPACITY(obj) * config_get_double("scale_drink_capacity"));
			if (amt > 0) {
				points_to_give -= (this_share * GET_DRINK_CONTAINER_CAPACITY(obj));
			}
			set_obj_val(obj, VAL_DRINK_CONTAINER_CAPACITY, amt);
			set_obj_val(obj, VAL_DRINK_CONTAINER_CONTENTS, amt);
			// negatives aren't even possible here
			break;
		}
		case ITEM_COINS: {
			amt = (int)round(this_share * GET_COINS_AMOUNT(obj) * config_get_double("scale_coin_amount"));
			if (amt > 0) {
				points_to_give -= (this_share * GET_COINS_AMOUNT(obj));
			}
			set_obj_val(obj, VAL_COINS_AMOUNT, amt);
			// this can't realistically be negative
			break;
		}
		case ITEM_MISSILE_WEAPON: {
			// aiming for a dps around the damage value
			if (GET_MISSILE_WEAPON_DAMAGE(obj) > 0) {
				amt = round(this_share * GET_MISSILE_WEAPON_DAMAGE(obj) * get_weapon_speed(obj));
				if (amt > 0) {
					points_to_give -= (this_share * GET_MISSILE_WEAPON_DAMAGE(obj));
				}
				set_obj_val(obj, VAL_MISSILE_WEAPON_DAMAGE, amt);
			}
			// leave negatives alone
			break;
		}
		case ITEM_AMMO: {
			if (GET_AMMO_DAMAGE_BONUS(obj) > 0) {
				amt = (int)round(this_share * GET_AMMO_DAMAGE_BONUS(obj));
				if (amt > 0) {
					points_to_give -= (this_share * GET_AMMO_DAMAGE_BONUS(obj));
				}
				set_obj_val(obj, VAL_AMMO_DAMAGE_BONUS, amt);
			}
			// leave negatives alone
			break;
		}
		case ITEM_PACK: {
			amt = (int)round(this_share * GET_PACK_CAPACITY(obj) * config_get_double("scale_pack_size"));
			if (amt > 0) {
				points_to_give -= (this_share * GET_PACK_CAPACITY(obj));
			}
			set_obj_val(obj, VAL_PACK_CAPACITY, amt);
			// negatives aren't really possible here
			break;
		}
	}
	
	// distribute points: applies
	for (apply = GET_OBJ_APPLIES(obj); apply; apply = next_apply) {
		next_apply = apply->next;
		
		if (!apply_never_scales[(int)apply->location]) {
			this_share = MAX(0, MIN(share, points_to_give));
			// raw amount
			per_point = (1.0 / apply_values[(int)apply->location]);
			
			if (apply->modifier > 0 || scale_negative) {
				// positive benefit
				amt = round(this_share * apply->modifier * per_point);
				points_to_give -= (this_share * ABSOLUTE(apply->modifier));
				apply->modifier = amt;
			}
			else if (apply->modifier < 0) {
				// penalty: does not cost from points_to_give
				apply->modifier = round(apply->modifier * per_point);
			}
		}
		
		// remove zero-applies
		if (apply->modifier == 0) {
			LL_DELETE(GET_OBJ_APPLIES(obj), apply);
			free(apply);
		}
	}
	
	// cleanup
	#undef SHARE_OR_BONUS
}


 //////////////////////////////////////////////////////////////////////////////
//// SHIPPING SYSTEM /////////////////////////////////////////////////////////

/**
* Queues up a shipping order and messages the character.
*
* @param char_data *ch The person queueing the shipment.
* @param empire_data *emp The empire that is shipping.
* @param int from_island The origin island id.
* @param int to_island The destination island id.
* @param int number The quantity to ship.
* @param struct empire_storage_data *store Storage info for the item to ship.
* @param room_data *to_room Optional: Exact destination room (not just island). May be NULL.
*/
void add_shipping_queue(char_data *ch, empire_data *emp, int from_island, int to_island, int number, struct empire_storage_data *store, room_data *to_room) {
	struct shipping_data *sd;
	struct island_info *isle;
	bool done;
	
	if (!emp || from_island == NO_ISLAND || to_island == NO_ISLAND || number < 0 || !store) {
		msg_to_char(ch, "Unable to set up shipping: invalid input.\r\n");
		return;
	}
	
	// try to add to existing order
	done = FALSE;
	DL_FOREACH(EMPIRE_SHIPPING_LIST(emp), sd) {
		if (sd->vnum != store->vnum) {
			continue;
		}
		if (sd->from_island != from_island || sd->to_island != to_island) {
			continue;
		}
		if ((to_room && sd->to_room != GET_ROOM_VNUM(to_room)) || (!to_room && sd->to_room != NOWHERE)) {
			continue;	// non-matching to-room
		}
		if (sd->status != SHIPPING_QUEUED && sd->status != SHIPPING_WAITING_FOR_SHIP) {
			continue;	// can combine only with those 2
		}
		
		// found one to add to!
		sd->amount += number;
		done = TRUE;
		break;
	}
	
	if (!done) {
		// add shipping order
		CREATE(sd, struct shipping_data, 1);
		sd->vnum = store->vnum;
		sd->amount = number;
		sd->from_island = from_island;
		sd->to_island = to_island;
		sd->to_room = to_room ? GET_ROOM_VNUM(to_room) : NOWHERE;
		sd->status = SHIPPING_QUEUED;
		sd->status_time = time(0);
		sd->ship_origin = NOWHERE;
		sd->shipping_id = -1;
		
		// borrow the LATEST timers from storage
		sd->timers = split_storage_timers(&store->timers, number);
		
		// add to end
		DL_APPEND(EMPIRE_SHIPPING_LIST(emp), sd);
	}
	
	// charge resources -- we pass FALSE at the end because we already split out the timers
	charge_stored_resource(emp, from_island, store->vnum, number, FALSE);
	EMPIRE_NEEDS_STORAGE_SAVE(emp) = TRUE;
	
	// messaging
	isle = get_island(to_island, TRUE);
	if (to_room) {
		msg_to_char(ch, "You set %d '%s' to ship to %s%s.\r\n", number, skip_filler(GET_OBJ_SHORT_DESC(store->proto)), get_room_name(to_room, FALSE), coord_display(ch, X_COORD(to_room), Y_COORD(to_room), FALSE));
	}
	else {
		msg_to_char(ch, "You set %d '%s' to ship to %s.\r\n", number, skip_filler(GET_OBJ_SHORT_DESC(store->proto)), isle ? get_island_name_for(isle->id, ch) : "an unknown island");
	}
}


/**
* @param struct shipping_data *shipd The shipment.
* @return int Time (in seconds) this shipment takes.
*/
int calculate_shipping_time(struct shipping_data *shipd) {
	struct island_info *from, *to;
	room_data *from_center, *to_center;
	int dist, max, cost;
	
	if (shipd->ship_origin != NOWHERE && shipd->to_room != NOWHERE) {
		dist = compute_distance(real_room(shipd->ship_origin), real_room(shipd->to_room));
	}
	else {
		from = get_island(shipd->from_island, FALSE);
		to = get_island(shipd->to_island, FALSE);
		
		// unable to find islands?
		if (!from || !to) {
			return 0;
		}
		
		from_center = real_room(from->center);
		to_center = shipd->to_room ? real_room(shipd->to_room) : real_room(to->center);
	
		// unable to find locations?
		if (!from_center || !to_center) {
			return 0;
		}
		
		dist = compute_distance(from_center, to_center);
	}
	
	// maximum distance (further distances cost nothing): lesser of height/width
	max = MIN(MAP_WIDTH, MAP_HEIGHT);
	dist = MIN(dist, max);
	
	// time cost as a percentage of 2 real hours
	cost = (int) (((double)dist / max) * (2.0 * SECS_PER_REAL_HOUR));
	
	return cost;
}


/**
* Unloads a shipment at its destination island (or the origin, if it can't find
* docks). This frees the shipment data afterwards.
*
* @param empire_data *emp The empire whose shipment it is.
* @param struct shipping_data *shipd Which shipment to deliver.
*/
void deliver_shipment(empire_data *emp, struct shipping_data *shipd) {
	bool have_ship = (shipd->shipping_id != -1);
	struct shipping_data *iter;
	struct empire_storage_data *new_store;
	room_data *dock = NULL;
	
	// mark all shipments on this ship "delivered" (if we still have a ship)
	if (have_ship) {
		for (iter = shipd; iter; iter = iter->next) {
			if (iter->shipping_id == shipd->shipping_id) {
				iter->status = SHIPPING_DELIVERED;
			}
		}
	}
	
	// find target location: exact first
	if (shipd->to_room != NOWHERE && (dock = real_room(shipd->to_room))) {
		if (room_has_function_and_city_ok(emp, dock, FNC_DOCKS)) {
			// seems valid
		}
		else {
			// not a valid dock target
			dock = NULL;
		}
	}
	// backup target location: island
	if (!dock) {
		dock = find_docks(emp, shipd->to_island);
	}
	
	// did we find a dock?
	if (dock) {
		// unload the shipment at the destination
		if (shipd->vnum != NOTHING && shipd->amount > 0 && obj_proto(shipd->vnum)) {
			log_to_empire(emp, ELOG_SHIPPING, "%dx %s: shipped to %s%s", shipd->amount, get_obj_name_by_proto(shipd->vnum), get_island_name_for_empire(shipd->to_island, emp), coord_display(NULL, X_COORD(dock), Y_COORD(dock), FALSE));
			new_store = add_to_empire_storage(emp, shipd->to_island, shipd->vnum, shipd->amount, 0);
			if (new_store) {
				merge_storage_timers(&new_store->timers, shipd->timers, shipd->amount);
			}
		}
		if (have_ship) {
			move_ship_to_destination(emp, shipd, dock);
		}
	}
	else {
		// return dock (or point of origin)
		dock = real_room(shipd->ship_origin);
		
		// no docks -- unload the shipment at home
		if (shipd->vnum != NOTHING && shipd->amount > 0 && obj_proto(shipd->vnum)) {
			log_to_empire(emp, ELOG_SHIPPING, "%dx %s: returned to %s%s", shipd->amount, get_obj_name_by_proto(shipd->vnum), get_island_name_for_empire(shipd->from_island, emp), coord_display(NULL, dock ? X_COORD(dock) : -1, dock ? Y_COORD(dock) : -1, FALSE));
			new_store = add_to_empire_storage(emp, shipd->from_island, shipd->vnum, shipd->amount, 0);
			if (new_store) {
				merge_storage_timers(&new_store->timers, shipd->timers, shipd->amount);
			}
		}
		if (have_ship) {
			move_ship_to_destination(emp, shipd, dock);
		}
	}
	
	// and delete this entry from the list
	DL_DELETE(EMPIRE_SHIPPING_LIST(emp), shipd);
	free_shipping_data(shipd);
}


/**
* Finds a completed docks building on the given island, belonging to the given
* empire. It won't find no-work docks.
*
* @param empire_data *emp The empire to check.
* @param int island_id Which island to search.
* @return room_data* The found docks room, or NULL for none.
*/
room_data *find_docks(empire_data *emp, int island_id) {
	struct empire_territory_data *ter, *next_ter;
	struct empire_vehicle_data *vter, *next_vter;
	vehicle_data *veh;
	
	if (!emp || island_id == NO_ISLAND) {
		return NULL;
	}
	
	// try territory first
	HASH_ITER(hh, EMPIRE_TERRITORY_LIST(emp), ter, next_ter) {
		if (GET_ISLAND_ID(ter->room) != island_id) {
			continue;
		}
		if (!room_has_function_and_city_ok(emp, ter->room, FNC_DOCKS)) {
			continue;
		}
		if (ROOM_AFF_FLAGGED(ter->room, ROOM_AFF_NO_WORK)) {
			continue;
		}
				
		return ter->room;
	}
	
	// if not, try vehicles
	HASH_ITER(hh, EMPIRE_VEHICLE_LIST(emp), vter, next_vter) {
		if (!(veh = vter->veh)) {
			continue;	// wrong vehicle somehow?
		}
		if (!IN_ROOM(veh) || GET_ISLAND_ID(IN_ROOM(veh)) != island_id) {
			continue;	// wrong island
		}
		if (!VEH_IS_COMPLETE(veh) || VEH_HEALTH(veh) < 1 || VEH_FLAGGED(veh, VEH_ON_FIRE)) {
			continue;	// basic ship checks
		}
		if (!vehicle_has_function_and_city_ok(veh, FNC_DOCKS)) {
			continue;	// not a dock
		}
		if (VEH_FLAGGED(veh, VEH_PLAYER_NO_WORK) || ROOM_AFF_FLAGGED(IN_ROOM(veh), ROOM_AFF_NO_WORK) || (VEH_INTERIOR_HOME_ROOM(veh) && ROOM_AFF_FLAGGED(VEH_INTERIOR_HOME_ROOM(veh), ROOM_AFF_NO_WORK))) {
			continue;	// no-work
		}
		if (!room_has_function_and_city_ok(emp, IN_ROOM(veh), FNC_DOCKS)) {
			continue;	// definitely not a docks
		}
		
		return IN_ROOM(veh);
	}
	
	return NULL;
}


/**
* Finds a ship to use for a given cargo. Any ship on this island is good.
*
* Note: This cannot specify a single dock to check; it looks for any ship on
* the island.
*
* @param empire_data *emp The empire that is shipping.
* @param struct shipping_data *shipd The shipment.
* @return vehicle_data* A ship, or NULL if none.
*/
vehicle_data *find_free_ship(empire_data *emp, struct shipping_data *shipd) {
	struct shipping_data *iter;
	bool already_used;
	vehicle_data *veh;
	int capacity;
	struct empire_vehicle_data *vter, *next_vter;
	
	if (!emp || shipd->from_island == NO_ISLAND) {
		return NULL;
	}
	
	HASH_ITER(hh, EMPIRE_VEHICLE_LIST(emp), vter, next_vter) {
		if (!(veh = vter->veh) || !IN_ROOM(veh) || GET_ISLAND_ID(IN_ROOM(veh)) != shipd->from_island) {
			continue;	// wrong island
		}
		if (!VEH_FLAGGED(veh, VEH_SHIPPING) || !VEH_IS_COMPLETE(veh) || VEH_HEALTH(veh) < 1 || VEH_FLAGGED(veh, VEH_ON_FIRE) || VEH_CARRYING_N(veh) >= VEH_CAPACITY(veh)) {
			continue;	// basic ship checks
		}
		if (VEH_FLAGGED(veh, VEH_PLAYER_NO_WORK) || ROOM_AFF_FLAGGED(IN_ROOM(veh), ROOM_AFF_NO_WORK) || (VEH_INTERIOR_HOME_ROOM(veh) && ROOM_AFF_FLAGGED(VEH_INTERIOR_HOME_ROOM(veh), ROOM_AFF_NO_WORK))) {
			continue;	// no-work
		}
		if (!room_has_function_and_city_ok(emp, IN_ROOM(veh), FNC_DOCKS)) {
			continue;	// not at docks
		}
		
		// viable vehicle:
	
		// calculate capacity to see if it's full, and check if it's already used for a different island
		capacity = VEH_CARRYING_N(veh);
		already_used = FALSE;
		DL_FOREACH(EMPIRE_SHIPPING_LIST(emp), iter) {
			if (iter->shipping_id == VEH_IDNUM(veh)) {
				capacity += iter->amount;
				if (iter->from_island != shipd->from_island || iter->to_island != shipd->to_island || iter->to_room != shipd->to_room) {
					already_used = TRUE;
					break;
				}
			}
		}
		if (already_used || capacity >= VEH_CAPACITY(veh)) {
			// ship full or in use
			continue;
		}
		
		// ensure no players on board
		if (!ship_is_empty(veh)) {
			continue;
		}
		
		// looks like we actually found one!
		return veh;
	}
	
	return NULL;
}


/**
* Finds a ship using its shipping id.
*
* @param empire_data *emp The purported owner of the ship.
* @param int shipping_id The shipping id to find.
* @return vehicle_data* The found ship, or NULL.
*/
vehicle_data *find_ship_by_shipping_id(empire_data *emp, int shipping_id) {
	struct empire_vehicle_data *vter, *next_vter;
	
	// shortcut
	if (!emp || shipping_id == -1) {
		return NULL;
	}
	
	HASH_ITER(hh, EMPIRE_VEHICLE_LIST(emp), vter, next_vter) {
		if (VEH_IDNUM(vter->veh) == shipping_id) {
			return vter->veh;
		}
	}
	
	return NULL;
}


/**
* Frees one shipping_data.
*
* @param struct shipping_data *shipd The thing to free.
*/
void free_shipping_data(struct shipping_data *shipd) {
	if (shipd) {
		free_storage_timers(&shipd->timers);
		free(shipd);
	}
}


/**
* Finds/creates a holding pen for ships during the shipping system.
*
* @return room_data* The ship holding pen.
*/
room_data *get_ship_pen(void) {
	room_data *room, *iter;
	
	DL_FOREACH2(interior_room_list, iter, next_interior) {
		if (GET_BUILDING(iter) && GET_BLD_VNUM(GET_BUILDING(iter)) == RTYPE_SHIP_HOLDING_PEN) {
			return iter;
		}
	}
	
	// did not find -- make one
	room = create_room(NULL);
	attach_building_to_room(building_proto(RTYPE_SHIP_HOLDING_PEN), room, TRUE);
	GET_ISLAND_ID(room) = NO_ISLAND;
	GET_ISLAND(room) = NULL;
	
	return room;
}


/**
* Attaches a shipment to a boat, and splits the shipment if the boat is full.
* If you are iterating over the shipping list and have already grabbed
* shipd->next, you should re-grab it after calling this, as split shipments
* will be added immediately after.
*
* @param empire_data *emp The empire who is shipping.
* @param struct shipping_data *shipd The shipment
* @param vehicle_data *boat The ship.
* @param bool *full A variable to bind to if the ship filles up
*/
void load_shipment(struct empire_data *emp, struct shipping_data *shipd, vehicle_data *boat, bool *full) {
	struct shipping_data *iter, *newd;
	int capacity, size_avail;

	// on bad input, just tell them it was full and hope for better results next time
	if (!emp || !shipd || !boat) {
		*full = TRUE;
		return;
	}
	
	size_avail = VEH_CAPACITY(boat) - VEH_CARRYING_N(boat);
	
	// calculate capacity
	capacity = 0;
	DL_FOREACH(EMPIRE_SHIPPING_LIST(emp), iter) {
		if (iter->shipping_id == VEH_IDNUM(boat)) {
			capacity += iter->amount;
		}
	}
	
	// this shouldn't be possible... but just in case
	if (capacity >= size_avail) {
		*full = TRUE;
		return;
	}
	
	// ship full? need to split
	if (capacity + shipd->amount > size_avail) {
		CREATE(newd, struct shipping_data, 1);
		newd->vnum = shipd->vnum;
		newd->amount = shipd->amount - (size_avail - capacity);	// only what's left
		newd->from_island = shipd->from_island;
		newd->to_island = shipd->to_island;
		newd->to_room = shipd->to_room;
		newd->status = SHIPPING_WAITING_FOR_SHIP;	// at this point, definitely waiting
		newd->status_time = shipd->status_time;
		newd->ship_origin = NOWHERE;
		newd->shipping_id = -1;
		newd->timers = split_storage_timers(&shipd->timers, newd->amount);
		
		// put right after shipd in the list
		DL_APPEND_ELEM(EMPIRE_SHIPPING_LIST(emp), shipd, newd);
		
		// remove overage
		shipd->amount = size_avail - capacity;
		*full = TRUE;
	}
	else {
		*full = ((shipd->amount + capacity) >= size_avail);
	}
	
	// mark it as attached to this boat
	shipd->shipping_id = VEH_IDNUM(boat);
}


/**
* This function attempts to find the ship for a particular shipment, and send
* it to the room of your choice (may be the destination OR origin). The
* shipment's shipping id will be set to -1, to avoid re-moving ships.
*
* @param empire_data *emp The empire whose shipment it is.
* @param struct shipping_data *shipd The shipment data.
* @param room_data *to_room Which room to send it to.
*/
void move_ship_to_destination(empire_data *emp, struct shipping_data *shipd, room_data *to_room) {
	char buf[MAX_STRING_LENGTH];
	struct shipping_data *iter;
	vehicle_data *boat;
	int old;

	// sanity
	if (!emp || !shipd || !to_room || shipd->shipping_id == -1) {
		return;
	}
	
	// find the boat
	if (!(boat = find_ship_by_shipping_id(emp, shipd->shipping_id))) {
		shipd->shipping_id = -1;	// whoops? it doesn't exist
		return;
	}
	
	if (ROOM_PEOPLE(IN_ROOM(boat))) {
		snprintf(buf, sizeof(buf), "$V %s away.", mob_move_types[VEH_MOVE_TYPE(boat)]);
		act(buf, FALSE, ROOM_PEOPLE(IN_ROOM(boat)), NULL, boat, TO_CHAR | TO_ROOM | TO_QUEUE | ACT_VEH_VICT);
	}
	
	vehicle_from_room(boat);
	vehicle_to_room(boat, to_room);
	
	if (ROOM_PEOPLE(IN_ROOM(boat))) {
		snprintf(buf, sizeof(buf), "$V %s in.", mob_move_types[VEH_MOVE_TYPE(boat)]);
		act(buf, FALSE, ROOM_PEOPLE(IN_ROOM(boat)), NULL, boat, TO_CHAR | TO_ROOM | TO_QUEUE | ACT_VEH_VICT);
	}
	
	// remove the shipping id from all shipments that were on this ship (including this one)
	old = shipd->shipping_id;
	DL_FOREACH(EMPIRE_SHIPPING_LIST(emp), iter) {
		if (iter->shipping_id == old) {
			iter->shipping_id = -1;
		}
	}
}


/**
* Run one shipping cycle for an empire. This runs every 6 game hours -- at
* 1am, 7am, 1pm, and 7pm (global time, not regional time).
*
* @param empire_data *emp The empire to run.
*/
void process_shipping_one(empire_data *emp) {
	struct shipping_data *shipd, *next_shipd;
	vehicle_data *last_ship = NULL;
	bool full, changed = FALSE;
	int last_from = NO_ISLAND, last_to = NO_ISLAND;
	room_vnum last_to_vnum = NOWHERE;
	
	DL_FOREACH_SAFE(EMPIRE_SHIPPING_LIST(emp), shipd, next_shipd) {
		switch (shipd->status) {
			case SHIPPING_QUEUED:	// both these are 'waiting' states
			case SHIPPING_WAITING_FOR_SHIP: {
				// was this lost to decay before shipping?
				if (shipd->vnum != NOTHING && shipd->amount <= 0) {
					DL_DELETE(EMPIRE_SHIPPING_LIST(emp), shipd);
					free_shipping_data(shipd);
					break;
				}
				
				// attempt to find a(nother) ship
				if (!last_ship || last_from != shipd->from_island || last_to != shipd->to_island || last_to_vnum != shipd->to_room) {
					last_ship = find_free_ship(emp, shipd);
					last_from = shipd->from_island;
					last_to = shipd->to_island;
					last_to_vnum = shipd->to_room;
				}
				
				// this only works if we found a ship to use (or had a free one)
				if (last_ship) {
					changed = TRUE;
					load_shipment(emp, shipd, last_ship, &full);

					// update next_shipd in case shipd was split (from full ship)
					next_shipd = shipd->next;
					
					if (full) {
						sail_shipment(emp, last_ship);
						last_ship = NULL;
					}
				}
				else {	// did not find a ship
					shipd->status = SHIPPING_WAITING_FOR_SHIP;
				}
				break;
			}
			case SHIPPING_EN_ROUTE: {
				if (time(0) > shipd->status_time + calculate_shipping_time(shipd)) {
					deliver_shipment(emp, shipd);
					changed = TRUE;
				}
				break;
			}
			case SHIPPING_DELIVERED: {
				deliver_shipment(emp, shipd);
				changed = TRUE;
				break;
			}
		}
	}
	
	// check for unsailed ships
	DL_FOREACH_SAFE(EMPIRE_SHIPPING_LIST(emp), shipd, next_shipd) {
		if ((shipd->status == SHIPPING_QUEUED || shipd->status == SHIPPING_WAITING_FOR_SHIP) && shipd->shipping_id != -1) {
			vehicle_data *boat = find_ship_by_shipping_id(emp, shipd->shipping_id);
			if (boat) {
				sail_shipment(emp, boat);
			}
		}
	}
	
	if (changed) {
		EMPIRE_NEEDS_STORAGE_SAVE(emp) = TRUE;
	}
}


/**
* Runs a shipping cycle for all empires. This runs every 10 minutes.
*/
void process_shipping(void) {
	empire_data *emp, *next_emp;
	
	HASH_ITER(hh, empire_table, emp, next_emp) {
		if (EMPIRE_SHIPPING_LIST(emp) && !EMPIRE_IS_TIMED_OUT(emp)) {
			process_shipping_one(emp);
		}
	}
}


/**
* Puts the ship out to sea (in the holding pen), and ensures all its contents
* are marked en-route.
*
* @param empire_data *emp The empire sending the shipment.
* @param vehicle_data *boat The ship object.
*/
void sail_shipment(empire_data *emp, vehicle_data *boat) {
	char buf[MAX_STRING_LENGTH];
	struct shipping_data *iter;

	// sanity
	if (!emp|| !boat) {
		return;
	}
	
	// verify contents
	DL_FOREACH(EMPIRE_SHIPPING_LIST(emp), iter) {
		if (iter->shipping_id == VEH_IDNUM(boat)) {
			iter->status = SHIPPING_EN_ROUTE;
			iter->status_time = time(0);
			iter->ship_origin = GET_ROOM_VNUM(IN_ROOM(boat));
		}
	}
	
	if (ROOM_PEOPLE(IN_ROOM(boat))) {
		snprintf(buf, sizeof(buf), "$V %s away.", mob_move_types[VEH_MOVE_TYPE(boat)]);
		act(buf, FALSE, ROOM_PEOPLE(IN_ROOM(boat)), NULL, boat, TO_CHAR | TO_ROOM | TO_QUEUE | ACT_VEH_VICT);
	}
	
	vehicle_from_room(boat);
	vehicle_to_room(boat, get_ship_pen());
	
	if (ROOM_PEOPLE(IN_ROOM(boat))) {
		snprintf(buf, sizeof(buf), "$V %s in.", mob_move_types[VEH_MOVE_TYPE(boat)]);
		act(buf, FALSE, ROOM_PEOPLE(IN_ROOM(boat)), NULL, boat, TO_CHAR | TO_ROOM | TO_QUEUE | ACT_VEH_VICT);
	}
	
	EMPIRE_NEEDS_STORAGE_SAVE(emp) = TRUE;
}


/**
* Determines if a ship has any players in it.
*
* @param vehicle_data *ship The ship to check.
* @return bool TRUE if the ship is empty, FALSE if it has players inside.
*/
bool ship_is_empty(vehicle_data *ship) {
	struct vehicle_room_list *vrl;
	vehicle_data *iter;
	char_data *ch;
	
	if (!ship) {
		return FALSE;
	}
	
	// simple occupants
	if (VEH_SITTING_ON(ship) && !IS_NPC(VEH_SITTING_ON(ship))) {
		return FALSE;
	}
	if (VEH_LED_BY(ship) && !IS_NPC(VEH_LED_BY(ship))) {
		return FALSE;
	}
	if (VEH_DRIVER(ship) && !IS_NPC(VEH_DRIVER(ship))) {
		return FALSE;
	}
	
	// check all interior rooms
	LL_FOREACH(VEH_ROOM_LIST(ship), vrl) {
		// people
		DL_FOREACH2(ROOM_PEOPLE(vrl->room), ch, next_in_room) {
			if (!IS_NPC(ch)) {
				// not empty!
				return FALSE;
			}
		}
		
		// check nested vehicles recursively
		DL_FOREACH2(ROOM_VEHICLES(vrl->room), iter, next_in_room) {
			if (!ship_is_empty(iter)) {
				return FALSE;	// not empty
			}
		}
	}
	
	// didn't find anybody
	return TRUE;
}


 //////////////////////////////////////////////////////////////////////////////
//// TRADE COMMAND FUNCTIONS /////////////////////////////////////////////////

/**
* do_trade: trade check
*
* @param char_data *ch The player.
* @param char *argument Any text after the subcommand.
*/
void trade_check(char_data *ch, char *argument) {
	char output[MAX_STRING_LENGTH*2], line[256], scale[256];
	struct trading_post_data *tpd;
	int size = 0, count = 0;
	int to_collect = 0;
	
	double trading_post_fee = config_get_double("trading_post_fee");
	
	if (*argument) {
		size = snprintf(output, sizeof(output), "Your \"%s\" items for trade:\r\n", argument);
	}
	else {
		size = snprintf(output, sizeof(output), "Your items for trade:\r\n");
	}
	
	DL_FOREACH(trading_list, tpd) {
		// disqualifiers -- this should be the same as trade_cancel because players use this function to find numbers for that one
		if (tpd->player != GET_IDNUM(ch)) {
			continue;
		}
		// if it's just collectible coins, don't show
		if (IS_SET(tpd->state, TPD_BOUGHT) && IS_SET(tpd->state, TPD_COINS_PENDING)) {
			to_collect += round(tpd->buy_cost * (1.0 - trading_post_fee)) + tpd->post_cost;
			continue;
		}
		if (!tpd->obj) {
			continue;
		}
		if (IS_SET(tpd->state, TPD_EXPIRED) && (!IS_SET(tpd->state, TPD_OBJ_PENDING) || !tpd->obj)) {
			// if it's expired, only show it if the object is still attached
			continue;
		}
		if (!IS_SET(tpd->state, TPD_FOR_SALE | TPD_EXPIRED | TPD_COINS_PENDING)) {
			// only these 3 states matter for showing a player
			continue;
		}
		if (*argument && !multi_isname(argument, GET_OBJ_KEYWORDS(tpd->obj))) {
			continue;
		}
		
		// the rest are still things for-sale or expired and waiting to collect
		
		// parts
		if (GET_OBJ_CURRENT_SCALE_LEVEL(tpd->obj)) {
			snprintf(scale, sizeof(scale), " (lvl %d)", GET_OBJ_CURRENT_SCALE_LEVEL(tpd->obj));
		}
		else {
			*scale = '\0';
		}
		
		snprintf(line, sizeof(line), "%s%2d. %s: %d coin%s%s%s&0\r\n", IS_SET(tpd->state, TPD_EXPIRED) ? "&r" : "", ++count, GET_OBJ_SHORT_DESC(tpd->obj), tpd->buy_cost, PLURAL(tpd->buy_cost), scale, IS_SET(tpd->state, TPD_EXPIRED) ? " (expired)" : "");
		
		if (size + strlen(line) < sizeof(output)) {
			size += snprintf(output + size, sizeof(output) - size, "%s", line);
		}
		else {
			size += snprintf(output + size, sizeof(output) - size, "... and more\r\n");
			break;
		}
		
	}
	
	if (to_collect > 0) {
		size += snprintf(output + size, sizeof(output) - size, " &y%s to collect&0\r\n", money_amount(GET_LOYALTY(ch), to_collect));
	}
	else if (count == 0) {
		size += snprintf(output + size, sizeof(output) - size, " none\r\n");
	}
	
	page_string(ch->desc, output, TRUE);
}


/**
* do_trade: trade list [keywords]
*
* @param char_data *ch The player.
* @param char *argument Any text after the subcommand.
*/
void trade_list(char_data *ch, char *argument) {
	char output[MAX_STRING_LENGTH*2], line[256], scale[256], exchange[256];
	struct trading_post_data *tpd;
	int size = 0, count = 0;
	empire_data *coin_emp = NULL;
	empire_vnum last_emp = NOTHING;
	double rate = 0.5;
	bool can_wear;
	int my_cost;
	
	if (*argument) {
		size = snprintf(output, sizeof(output), "\"%s\" items for trade:\r\n", argument);
	}
	else {
		size = snprintf(output, sizeof(output), "Items for trade:\r\n");
	}
	
	DL_FOREACH(trading_list, tpd) {
		// disqualifiers -- should match trade_list because players use that to find numbers
		if (!IS_SET(tpd->state, TPD_FOR_SALE) || !tpd->obj) {
			continue;
		}
		if (*argument && !multi_isname(argument, GET_OBJ_KEYWORDS(tpd->obj))) {
			continue;
		}
		
		// we always mark can-wear if it doesn't have any wear flags other than take
		can_wear = (!CAN_WEAR(tpd->obj, ~ITEM_WEAR_TAKE) || can_wear_item(ch, tpd->obj, FALSE));
		
		// parts
		if (GET_OBJ_CURRENT_SCALE_LEVEL(tpd->obj)) {
			snprintf(scale, sizeof(scale), " (lvl %d)", GET_OBJ_CURRENT_SCALE_LEVEL(tpd->obj));
		}
		else {
			*scale = '\0';
		}
		
		// lazy-load
		if (tpd->coin_type != last_emp) {
			coin_emp = real_empire(tpd->coin_type);
			rate = exchange_rate(GET_LOYALTY(ch), coin_emp);
			last_emp = tpd->coin_type;
		}
		
		if (rate != 1.0) {
			my_cost = ceil(tpd->buy_cost * 1.0/rate);
			snprintf(exchange, sizeof(exchange), " (%d)", my_cost);
		}
		else {
			*exchange = '\0';
		}
		
		snprintf(line, sizeof(line), "%s%2d. %s: %d%s %s%s%s%s%s%s&0\r\n", (tpd->player == GET_IDNUM(ch)) ? "&r" : (can_wear ? "" : "&R"), ++count, GET_OBJ_SHORT_DESC(tpd->obj), tpd->buy_cost, exchange, (coin_emp ? EMPIRE_ADJECTIVE(coin_emp) : "misc"), scale, (OBJ_FLAGGED(tpd->obj, OBJ_SUPERIOR) ? " (sup)" : ""), OBJ_FLAGGED(tpd->obj, OBJ_ENCHANTED) ? " (ench)" : "", (tpd->player == GET_IDNUM(ch)) ? " (your auction)" : "", can_wear ? "" : " (can't use)");
		
		if (size + strlen(line) < sizeof(output)) {
			size += snprintf(output + size, sizeof(output) - size, "%s", line);
		}
		else {
			size += snprintf(output + size, sizeof(output) - size, "... and more\r\n");
			break;
		}
	}
	
	if (count == 0) {
		size += snprintf(output + size, sizeof(output) - size, " none\r\n");
	}
	
	page_string(ch->desc, output, TRUE);
}


/**
* do_trade: trade buy [number | name]
*
* @param char_data *ch The player.
* @param char *argument Any text after the subcommand.
*/
void trade_buy(char_data *ch, char *argument) {
	struct trading_post_data *tpd, *next_tpd;
	empire_data *coin_emp = NULL;
	char buf[MAX_STRING_LENGTH], buf2[MAX_STRING_LENGTH], *ptr1, *ptr2;
	char_data *seller;
	int num = 1;
	
	if (*argument && isdigit(*argument)) {
		num = atoi(argument);
		ptr1 = strchr(argument, '.');	// find first . or space
		ptr2 = strchr(argument, ' ');
		if (ptr1 && ptr2) {
			argument = (ptr1 < ptr2 ? ptr1 : ptr2) + 1;
		}
		else if (ptr1 || ptr2) {
			argument = ptr1 ? ptr1 : (ptr2 ? ptr2 : argument) + 1;
		}
		else {
			*argument = '\0';
		}
		skip_spaces(&argument);
	}
	
	DL_FOREACH_SAFE(trading_list, tpd, next_tpd) {
		// disqualifiers -- should match trade_list because players use that to find numbers
		if (!IS_SET(tpd->state, TPD_FOR_SALE) || !tpd->obj) {
			continue;
		}
		if (*argument && !multi_isname(argument, GET_OBJ_KEYWORDS(tpd->obj))) {
			continue;
		}
		
		// if player specified a number
		if (--num != 0) {
			continue;
		}
		
		// FOUND
		coin_emp = real_empire(tpd->coin_type);
		
		if (GET_IDNUM(ch) == tpd->player) {
			msg_to_char(ch, "You can't buy your own auctions.\r\n");
			return;
		}
		if (!can_afford_coins(ch, coin_emp, tpd->buy_cost)) {
			msg_to_char(ch, "You can't afford the cost of %s.\r\n", money_amount(coin_emp, tpd->buy_cost));
			return;
		}
		if (!CAN_CARRY_OBJ(ch, tpd->obj)) {
			msg_to_char(ch, "Your inventory is too full to buy that.\r\n");
			return;
		}
		
		// pay up
		charge_coins(ch, coin_emp, tpd->buy_cost, NULL, buf2);
		REMOVE_BIT(tpd->state, TPD_FOR_SALE);
		SET_BIT(tpd->state, TPD_BOUGHT | TPD_COINS_PENDING);
		
		// notify
		if ((seller = is_playing(tpd->player))) {
			msg_to_char(seller, "Your trading post item '%s' has sold for %s.\r\n", GET_OBJ_SHORT_DESC(tpd->obj), money_amount(coin_emp, tpd->buy_cost));
		}
		sprintf(buf, "You buy $p for %s.", buf2);
		act(buf, FALSE, ch, tpd->obj, NULL, TO_CHAR);
		act("$n buys $p.", FALSE, ch, tpd->obj, NULL, TO_ROOM | TO_SPAMMY);
			
		// obj
		add_to_object_list(tpd->obj);
		obj_to_char(tpd->obj, ch);
		get_otrigger(tpd->obj, ch, FALSE);
		tpd->obj = NULL;
		
		// cleanup
		queue_delayed_update(ch, CDU_SAVE);
		save_trading_post();
		return;
	}
	
	// no?
	msg_to_char(ch, "You don't see anything like that to buy.\r\n");
}


/**
* do_trade: trade cancel [number | name]
*
* @param char_data *ch The player.
* @param char *argument Any text after the subcommand.
*/
void trade_cancel(char_data *ch, char *argument) {
	struct trading_post_data *tpd, *next_tpd;
	char *ptr1, *ptr2;
	int num = 1;
	
	if (*argument && isdigit(*argument)) {
		num = atoi(argument);
		ptr1 = strchr(argument, '.');	// find first . or space
		ptr2 = strchr(argument, ' ');
		if (ptr1 && ptr2) {
			argument = (ptr1 < ptr2 ? ptr1 : ptr2) + 1;
		}
		else if (ptr1 || ptr2) {
			argument = ptr1 ? ptr1 : (ptr2 ? ptr2 : argument) + 1;
		}
		else {
			*argument = '\0';
		}
		skip_spaces(&argument);
	}
	
	if (num < 1) {
		msg_to_char(ch, "Usage: trade cancel [number] <keyword>\r\n");
		return;
	}
	
	DL_FOREACH_SAFE(trading_list, tpd, next_tpd) {
		// disqualifiers -- this should be the same as trade_check since the player uses it to find numbers
		if (tpd->player != GET_IDNUM(ch)) {
			continue;
		}
		if (!tpd->obj) {
			continue;
		}
		if (IS_SET(tpd->state, TPD_EXPIRED) && (!IS_SET(tpd->state, TPD_OBJ_PENDING) || !tpd->obj)) {
			continue;
		}
		if (!IS_SET(tpd->state, TPD_FOR_SALE | TPD_EXPIRED | TPD_COINS_PENDING)) {
			continue;
		}
		if (*argument && !multi_isname(argument, GET_OBJ_KEYWORDS(tpd->obj))) {
			continue;
		}
		if (IS_SET(tpd->state, TPD_BOUGHT) && IS_SET(tpd->state, TPD_COINS_PENDING)) {
			continue;
		}
		
		// passed the same checks as trade_check
		
		// the rest are still things for-sale or expired and waiting to collect
		if (--num != 0) {
			continue;
		}
		
		// FOUND
		if (!IS_SET(tpd->state, TPD_FOR_SALE)) {
			msg_to_char(ch, "You can only cancel trades that are still for sale.\r\n");
			return;
		}
		
		expire_trading_post_item(tpd);
		save_trading_post();
		msg_to_char(ch, "You have expired your post for %s. Use trade collect to get back the item.\r\n", GET_OBJ_SHORT_DESC(tpd->obj));
		return;
	}
	
	// if we got here, no matches
	msg_to_char(ch, "You can't find a matching post to cancel.\r\n");
}


/**
* do_trade: trade collect
*
* @param char_data *ch The player.
* @param char *argument Any text after the subcommand.
*/
void trade_collect(char_data *ch, char *argument) {	
	struct trading_post_data *tpd, *next_tpd;
	int to_collect = 0;
	bool full = FALSE, any = FALSE;
	
	double trading_post_fee = config_get_double("trading_post_fee");
	
	DL_FOREACH_SAFE(trading_list, tpd, next_tpd) {
		// disqualifiers
		if (tpd->player != GET_IDNUM(ch)) {
			continue;
		}

		if (IS_SET(tpd->state, TPD_EXPIRED) && IS_SET(tpd->state, TPD_COINS_PENDING)) {
			// should it be able to get into this state?
			REMOVE_BIT(tpd->state, TPD_COINS_PENDING);
		}
		if (IS_SET(tpd->state, TPD_EXPIRED) && IS_SET(tpd->state, TPD_OBJ_PENDING)) {
			if (tpd->obj) {
				if (!CAN_CARRY_OBJ(ch, tpd->obj)) {
					full = TRUE;
					continue;	// EARLY CONTINUE IN THE LOOP
				}
				
				// safe to give
				add_to_object_list(tpd->obj);
				obj_to_char(tpd->obj, ch);
				
				act("You collect $p from an expired auction.", FALSE, ch, tpd->obj, NULL, TO_CHAR);
				get_otrigger(tpd->obj, ch, FALSE);
				tpd->obj = NULL;
				any = TRUE;
			}
			
			REMOVE_BIT(tpd->state, TPD_OBJ_PENDING);
		}
		if (IS_SET(tpd->state, TPD_BOUGHT) && IS_SET(tpd->state, TPD_COINS_PENDING)) {
			to_collect += round(tpd->buy_cost * (1.0 - trading_post_fee)) + tpd->post_cost;
			REMOVE_BIT(tpd->state, TPD_COINS_PENDING);
		}
		
		// changes above may trigger auto-cleanup
		// in any other state, we do nothing
	}
	
	if (full) {
		msg_to_char(ch, "Your arms are too full to collect some expired auctions.\r\n");
	}
	if (to_collect > 0) {
		any = TRUE;
		increase_coins(ch, GET_LOYALTY(ch), to_collect);
		msg_to_char(ch, "You collect %s.\r\n", money_amount(GET_LOYALTY(ch), to_collect));
	}
	
	if (any) {
		queue_delayed_update(ch, CDU_SAVE);
		save_trading_post();
	}
	else {
		msg_to_char(ch, "There was nothing to collect.\r\n");
	}
}


/**
* do_trade: trade identify [number | name]
*
* @param char_data *ch The player.
* @param char *argument Any text after the subcommand.
*/
void trade_identify(char_data *ch, char *argument) {
	struct trading_post_data *tpd;
	char *ptr1, *ptr2;
	int num = 1;
	
	if (*argument && isdigit(*argument)) {
		num = atoi(argument);
		ptr1 = strchr(argument, '.');	// find first . or space
		ptr2 = strchr(argument, ' ');
		if (ptr1 && ptr2) {
			argument = (ptr1 < ptr2 ? ptr1 : ptr2) + 1;
		}
		else if (ptr1 || ptr2) {
			argument = ptr1 ? ptr1 : (ptr2 ? ptr2 : argument) + 1;
		}
		else {
			*argument = '\0';
		}
		skip_spaces(&argument);
	}
	
	DL_FOREACH(trading_list, tpd) {
		// disqualifiers -- should match trade_list because players use that to find numbers
		if (!IS_SET(tpd->state, TPD_FOR_SALE) || !tpd->obj) {
			continue;
		}
		if (*argument && !multi_isname(argument, GET_OBJ_KEYWORDS(tpd->obj))) {
			continue;
		}
		
		// if player specified a number
		if (--num != 0) {
			continue;
		}
		
		// FOUND
		identify_obj_to_char(tpd->obj, ch, FALSE);
		return;
	}
	
	// no?
	msg_to_char(ch, "You don't see anything like that to identify.\r\n");
}


/**
* do_trade: trade post <item> <cost> [time]
*
* @param char_data *ch The player.
* @param char *argument Any text after the subcommand.
*/
void trade_post(char_data *ch, char *argument) {	
	char buf[MAX_STRING_LENGTH], itemarg[MAX_INPUT_LENGTH], costarg[MAX_INPUT_LENGTH], *timearg;
	struct trading_post_data *tpd;
	obj_data *obj;
	int cost, length = config_get_int("trading_post_max_hours"), post_cost = 0;
	
	argument = any_one_arg(argument, itemarg);
	timearg = any_one_arg(argument, costarg);
	skip_spaces(&timearg);
	
	if (!*itemarg || !*costarg) {
		msg_to_char(ch, "Usage: trade post <item> <cost> [time]\r\n");
		msg_to_char(ch, "It costs 1 coin per gear rating to post an item (minimum 1; refundable).\r\n");
		msg_to_char(ch, "The item must be in your inventory. Cost is number of your empire's coins.\r\n");
		msg_to_char(ch, "Time is in real hours (default: %d).\r\n", config_get_int("trading_post_max_hours"));
	}
	else if (!(obj = get_obj_in_list_vis(ch, itemarg, NULL, ch->carrying))) {
		msg_to_char(ch, "You don't seem to have %s %s to trade.", AN(itemarg), itemarg);
	}
	else if (OBJ_BOUND_TO(obj)) {
		msg_to_char(ch, "You can't trade bound items.\r\n");
	}
	else if (IS_STOLEN(obj)) {
		msg_to_char(ch, "You can't post stolen items.\r\n");
	}
	else if (OBJ_FLAGGED(obj, OBJ_JUNK)) {
		msg_to_char(ch, "You can't post junk for sale.\r\n");
	}
	else if (GET_OBJ_TIMER(obj) > 0) {
		msg_to_char(ch, "You can't post items with timers on them for sale.\r\n");
	}
	else if ((cost = atoi(costarg)) < 1) {
		msg_to_char(ch, "You must charge at least 1 coin.\r\n");
	}
	else if (*timearg && (length = atoi(timearg)) < 1) {
		msg_to_char(ch, "You can't set the time for less than 1 hour.\r\n");
	}
	else if (length > config_get_int("trading_post_max_hours")) {
		msg_to_char(ch, "The longest you can set it is %d hour%s.\r\n", config_get_int("trading_post_max_hours"), PLURAL(config_get_int("trading_post_max_hours")));
	}
	else if (!can_afford_coins(ch, GET_LOYALTY(ch), (post_cost = MAX(1, (int)rate_item(obj))))) {
		msg_to_char(ch, "You need %s to post that.\r\n", money_amount(GET_LOYALTY(ch), post_cost));
	}
	else {
		// success!
		snprintf(buf, sizeof(buf), "You post $p for %s, for %d hour%s.", money_amount(GET_LOYALTY(ch), cost), length, PLURAL(length));
		act(buf, FALSE, ch, obj, NULL, TO_CHAR);
		snprintf(buf, sizeof(buf), "$n posts $p for %s, for %d hour%s.", money_amount(GET_LOYALTY(ch), cost), length, PLURAL(length));
		act(buf, FALSE, ch, obj, NULL, TO_ROOM | TO_SPAMMY);
		
		charge_coins(ch, GET_LOYALTY(ch), post_cost, NULL, NULL);
		
		// SEV_x: events that must be canceled or changed when an item is posted
		cancel_stored_event(&GET_OBJ_STORED_EVENTS(obj), SEV_OBJ_AUTOSTORE);
		cancel_stored_event(&GET_OBJ_STORED_EVENTS(obj), SEV_OBJ_TIMER);
		// TODO: convert to a timer that can tick while in the trade queue? or prevent timer items from trade
		
		CREATE(tpd, struct trading_post_data, 1);
		
		// put at end of list
		DL_APPEND(trading_list, tpd);
		
		// data
		tpd->player = GET_IDNUM(ch);
		tpd->state = TPD_FOR_SALE;
		tpd->start = time(0);
		tpd->for_hours = length;
		
		tpd->buy_cost = cost;
		tpd->post_cost = post_cost;
		tpd->coin_type = GET_LOYALTY(ch) ? EMPIRE_VNUM(GET_LOYALTY(ch)) : OTHER_COIN;
		
		obj_from_char(obj);
		tpd->obj = obj;
		remove_from_object_list(obj);
		
		queue_delayed_update(ch, CDU_SAVE);
		save_trading_post();
	}
}


 //////////////////////////////////////////////////////////////////////////////
//// WAREHOUSE COMMAND FUNCTIONS /////////////////////////////////////////////

/**
* Sub-processor for do_warehouse: inventory.
* This also handles home storage.
*
* @param char_data *ch The player.
* @param char *argument The rest of the argument after "list"
* @param int mode SCMD_WAREHOUSE or SCMD_HOME
*/
void warehouse_inventory(char_data *ch, char *argument, int mode) {
	char arg[MAX_INPUT_LENGTH], output[MAX_STRING_LENGTH*4], line[MAX_STRING_LENGTH], part[256], flags[256], quantity[256], *tmp;
	bool imm_access = (GET_ACCESS_LEVEL(ch) >= LVL_CIMPL || IS_GRANTED(ch, GRANT_EMPIRES));
	bool home_mode = (mode == SCMD_HOME);
	struct empire_unique_storage *iter;
	empire_data *emp = GET_LOYALTY(ch);
	int island = GET_ISLAND_ID(IN_ROOM(ch));
	struct empire_unique_storage *eus;
	char_data *targ_player = ch;
	bool file = FALSE;
	int num, size;
	
	if (mode == SCMD_WAREHOUSE && imm_access) {
		// if first word is 
		tmp = any_one_word(argument, arg);
		if (*arg && (emp = get_empire_by_name(arg))) {
			// move rest of argument over
			argument = tmp;
			skip_spaces(&argument);
		}
		else {
			emp = GET_LOYALTY(ch);
		}
	}
	else if (mode == SCMD_HOME && imm_access) {
		// check first word is player
		tmp = any_one_word(argument, arg);
		if (*arg && (targ_player = find_or_load_player(arg, &file))) {
			check_delayed_load(targ_player);
			// move rest over
			argument = tmp;
			skip_spaces(&argument);
		}
		else {
			targ_player = ch;
		}
	}
	
	if (!emp && !home_mode) {
		msg_to_char(ch, "You must be in an empire to do that.\r\n");
		return;
	}

	if (*argument) {
		snprintf(part, sizeof(part), "\"%s\"", argument);
	}
	else {
		strcpy(part, "Unique");	// size ok
	}
	
	if (home_mode) {
		DL_COUNT(GET_HOME_STORAGE(targ_player), eus, num);
		if (targ_player == ch) {
			size = snprintf(output, sizeof(output), "%s items stored in your home (%d/%d):\r\n", part, num, config_get_int("max_home_store_uniques"));
		}
		else {
			size = snprintf(output, sizeof(output), "%s items stored in %s's home (%d/%d):\r\n", part, GET_PC_NAME(targ_player), num, config_get_int("max_home_store_uniques"));
		}
	}
	else {
		size = snprintf(output, sizeof(output), "%s items stored in %s%s&0:\r\n", part, EMPIRE_BANNER(emp), EMPIRE_NAME(emp));
	}
	num = 0;
	
	DL_FOREACH((home_mode ? GET_HOME_STORAGE(targ_player) : EMPIRE_UNIQUE_STORAGE(emp)), iter) {
		if (!home_mode && !imm_access && iter->island != island) {
			continue;
		}
		if (*argument && !multi_isname(argument, GET_OBJ_KEYWORDS(iter->obj))) {
			continue;
		}
		
		if (iter->flags) {
			prettier_sprintbit(iter->flags, unique_storage_flags, flags);
			snprintf(part, sizeof(part), " [%s]", flags);
		}
		else {
			*part = '\0';
		}
		
		if (iter->amount != 1) {
			snprintf(quantity, sizeof(quantity), " (x%d)", iter->amount);
		}
		else {
			*quantity = '\0';
		}
		
		// build line
		snprintf(line, sizeof(line), "%3d. %s%s%s\t0\r\n", ++num, obj_desc_for_char(iter->obj, ch, OBJ_DESC_WAREHOUSE), part, quantity);
		
		if (size + strlen(line) < sizeof(output)) {
			size += snprintf(output + size, sizeof(output) - size, "%s", line);
		}
		else {
			size += snprintf(output + size, sizeof(output) - size, "... list too long\r\n");
			break;
		}
	}
	
	if (num == 0) {
		size += snprintf(output + size, sizeof(output) - size, " none\r\n");
	}
	
	page_string(ch->desc, output, TRUE);
}


/**
* Sub-processor for do_warehouse: identify.
* This also handles home storage.
*
* @param char_data *ch The player.
* @param char *argument The rest of the argument after "identify"
* @param int mode SCMD_WAREHOUSE or SCMD_HOME
*/
void warehouse_identify(char_data *ch, char *argument, int mode) {
	bool imm_access = (GET_ACCESS_LEVEL(ch) >= LVL_CIMPL || IS_GRANTED(ch, GRANT_EMPIRES));
	empire_data *room_emp = ROOM_OWNER(IN_ROOM(ch));
	struct empire_unique_storage *iter, *next_iter;
	int island = GET_ISLAND_ID(IN_ROOM(ch));
	bool home_mode = (mode == SCMD_HOME), num_only;
	int number;
	
	if (!*argument) {
		msg_to_char(ch, "Identify what?\r\n");
		return;
	}
	
	// access permission
	if (!home_mode && !imm_access && !room_has_function_and_city_ok(GET_LOYALTY(ch), IN_ROOM(ch), FNC_WAREHOUSE | FNC_VAULT)) {
		msg_to_char(ch, "You can't do that here.\r\n");
		return;
	}
	if (!home_mode && !imm_access && !IS_COMPLETE(IN_ROOM(ch))) {
		msg_to_char(ch, "Complete the building first.\r\n");
		return;
	}
	if (!home_mode && !imm_access && (!can_use_room(ch, IN_ROOM(ch), GUESTS_ALLOWED) || (room_emp && GET_LOYALTY(ch) != room_emp && !has_relationship(GET_LOYALTY(ch), room_emp, DIPL_TRADE)))) {
		msg_to_char(ch, "You don't have permission to do that here.\r\n");
		return;
	}
	if (!home_mode && !check_in_city_requirement(IN_ROOM(ch), TRUE)) {
		msg_to_char(ch, "This building must be in a city to use it for that.\r\n");
		return;
	}
	
	// list position or number?
	if (is_number(argument)) {
		number = atoi(argument);
		num_only = TRUE;
	}
	else {
		number = get_number(&argument);
		num_only = FALSE;
	}
	
	// final argument checks
	if (number < 1) {
		msg_to_char(ch, "Invalid list position %d.\r\n", number);
		return;
	}
	if (!*argument) {
		msg_to_char(ch, "Identify what?\r\n");
		return;
	}
	
	// ok, find it
	DL_FOREACH_SAFE((home_mode ? GET_HOME_STORAGE(ch) : EMPIRE_UNIQUE_STORAGE(GET_LOYALTY(ch))), iter, next_iter) {
		if (!home_mode && !imm_access && iter->island != island) {
			continue;
		}
		if (!num_only && !multi_isname(argument, GET_OBJ_KEYWORDS(iter->obj))) {
			continue;
		}
		
		// #. check
		if (--number > 0) {
			continue;
		}
		
		// FOUND:
		identify_obj_to_char(iter->obj, ch, FALSE);
		return;
	}
	
	// nothing?
	msg_to_char(ch, "You don't seem to be able to identify anything like that.\r\n");
}


/**
* Sub-processor for do_warehouse: retrieve.
* This also handles home storage.
*
* @param char_data *ch The player.
* @param char *argument The rest of the argument after "retrieve"
* @param int mode SCMD_WAREHOUSE or SCMD_HOME
*/
void warehouse_retrieve(char_data *ch, char *argument, int mode) {
	bool imm_access = (GET_ACCESS_LEVEL(ch) >= LVL_CIMPL || IS_GRANTED(ch, GRANT_EMPIRES));
	empire_data *room_emp = ROOM_OWNER(IN_ROOM(ch));
	struct empire_unique_storage *iter, *next_iter;
	int island = GET_ISLAND_ID(IN_ROOM(ch));
	bool home_mode = (mode == SCMD_HOME), num_only;
	char junk[MAX_INPUT_LENGTH], *tmp;
	obj_data *obj = NULL, *proto;
	int min, number, amt = 1;
	bool all = FALSE, any = FALSE, done = FALSE;
	trig_data *trig;
	
	if (!*argument) {
		msg_to_char(ch, "Retrieve what?\r\n");
		return;
	}
	
	// check location/access
	if (home_mode) {
		if (!imm_access && ROOM_PRIVATE_OWNER(HOME_ROOM(IN_ROOM(ch))) != GET_IDNUM(ch)) {
			msg_to_char(ch, "You can only do this in your own private home.\r\n");
			return;
		}
		if (!imm_access && time(0) - GET_LAST_HOME_SET_TIME(ch) < SECS_PER_REAL_HOUR) {
			amt = 60 - ((time(0) - GET_LAST_HOME_SET_TIME(ch)) / SECS_PER_REAL_MIN);
			msg_to_char(ch, "You must wait another %d minute%s -- your items are being moved.\r\n", amt, PLURAL(amt));
			return;
		}
	}
	else {	// warehouse mode
		// access permission
		if (!imm_access && !room_has_function_and_city_ok(GET_LOYALTY(ch), IN_ROOM(ch), FNC_WAREHOUSE | FNC_VAULT)) {
			msg_to_char(ch, "You can't do that here.\r\n");
			return;
		}
		if (!imm_access && !IS_COMPLETE(IN_ROOM(ch))) {
			msg_to_char(ch, "Complete the building first.\r\n");
			return;
		}
		if (!imm_access && (!can_use_room(ch, IN_ROOM(ch), GUESTS_ALLOWED) || (room_emp && GET_LOYALTY(ch) != room_emp && !has_relationship(GET_LOYALTY(ch), room_emp, DIPL_TRADE)))) {
			msg_to_char(ch, "You don't have permission to do that here.\r\n");
			return;
		}
		if (!imm_access && room_has_function_and_city_ok(GET_LOYALTY(ch), IN_ROOM(ch), FNC_VAULT) && !has_permission(ch, PRIV_WITHDRAW, IN_ROOM(ch))) {
			msg_to_char(ch, "You don't have permission to withdraw items here.\r\n");
			return;
		}
		if (!imm_access && room_has_function_and_city_ok(GET_LOYALTY(ch), IN_ROOM(ch), FNC_WAREHOUSE) && !has_permission(ch, PRIV_WAREHOUSE, IN_ROOM(ch))) {
			msg_to_char(ch, "You don't have permission to withdraw items here.\r\n");
			return;
		}
		if (!check_in_city_requirement(IN_ROOM(ch), TRUE)) {
			msg_to_char(ch, "This building must be in a city to use it for that.\r\n");
			return;
		}
	}
	
	// detect number-only OR args
	if (is_number(argument)) {
		num_only = TRUE;
		number = atoi(argument);
	}
	else {
		num_only = FALSE;
		// detect leading number (amount to retrieve) with a space
		tmp = any_one_arg(argument, junk);
		if (is_number(junk)) {
			if ((amt = atoi(junk)) < 1) {
				msg_to_char(ch, "Invalid number to retrieve.\r\n");
				return;
			}
		
			// skip past this arg
			argument = tmp;
			skip_spaces(&argument);
		}
	
		if (!strn_cmp(argument, "all ", 4) || !strn_cmp(argument, "all.", 4)) {
			argument += 4;
			all = TRUE;
		}
		
		number = get_number(&argument);
	}	// end !num_only
	
	// final argument checks
	if (number < 1) {
		msg_to_char(ch, "Invalid list position %d.\r\n", number);
		return;
	}
	if (!*argument) {
		msg_to_char(ch, "Retrieve %swhat?\r\n", all ? "all of " : "");
		return;
	}
	
	// ok, find it
	DL_FOREACH_SAFE((home_mode ? GET_HOME_STORAGE(ch) : EMPIRE_UNIQUE_STORAGE(GET_LOYALTY(ch))), iter, next_iter) {
		if (done || (amt <= 0 && !all)) {
			break;	// done early
		}
		
		if (!home_mode && !imm_access && iter->island != island) {
			continue;
		}
		if (!num_only && !multi_isname(argument, GET_OBJ_KEYWORDS(iter->obj))) {
			continue;
		}
		
		// #. check
		if (--number > 0) {
			continue;
		}
		
		// vault permission was pre-validated, but they have to be in one to use it
		if (IS_SET(iter->flags, EUS_VAULT) && !imm_access && !room_has_function_and_city_ok(GET_LOYALTY(ch), IN_ROOM(ch), FNC_VAULT)) {
			msg_to_char(ch, "You need to be in a vault to retrieve %s.\r\n", GET_OBJ_SHORT_DESC(iter->obj));
			return;
		}
		
		// load the actual objs
		while (!done && iter->amount > 0 && (all || amt-- > 0)) {
			if (iter->obj && !CAN_CARRY_OBJ(ch, iter->obj)) {
				act("Your arms are full.", FALSE, ch, NULL, NULL, TO_CHAR | TO_QUEUE);
				done = TRUE;
				break;
			}
		
			if (iter->amount > 1) {
				obj = copy_warehouse_obj(iter->obj);
			}
			else {
				// last one
				obj = iter->obj;
				iter->obj = NULL;
				add_to_object_list(obj);	// put back in object list
				obj->script_id = 0;	// clear this out so it's guaranteed to get a new one
				
				// re-add trigs to the random list
				if (SCRIPT(obj)) {
					LL_FOREACH(TRIGGERS(SCRIPT(obj)), trig) {
						add_trigger_to_global_lists(trig);
					}
				}
			}
			
			if (obj) {
				iter->amount -= 1;
				any = TRUE;
				
				// grab the timer from storage?
				if (iter->timers) {
					GET_OBJ_TIMER(obj) = iter->timers->timer;
					if ((min = config_get_int("min_timer_after_retrieve")) > 0 && min > GET_OBJ_TIMER(obj)) {
						if ((proto = obj_proto(GET_OBJ_VNUM(obj)))) {
							// don't raise it past the prototype (if it has one)
							min = MIN(min, GET_OBJ_TIMER(proto));
						}
						// raise it (but don't lower it if it dropped)
						GET_OBJ_TIMER(obj) = MAX(min, GET_OBJ_TIMER(obj));
					}
				}
				remove_storage_timer_items(&iter->timers, 1, TRUE);
				
				obj_to_char(obj, ch);	// inventory size pre-checked
				act("You retrieve $p.", FALSE, ch, obj, NULL, TO_CHAR | TO_QUEUE);
				act("$n retrieves $p.", FALSE, ch, obj, NULL, TO_ROOM | TO_QUEUE);
				
				// this should not be running load triggers
				// load_otrigger(obj);
				
				get_otrigger(obj, ch, FALSE);
			}
		}
		
		// remove the entry
		if (iter->amount <= 0 || !iter->obj) {
			if (home_mode) {
				DL_DELETE(GET_HOME_STORAGE(ch), iter);
			}
			else {
				DL_DELETE(EMPIRE_UNIQUE_STORAGE(GET_LOYALTY(ch)), iter);
				EMPIRE_NEEDS_STORAGE_SAVE(GET_LOYALTY(ch)) = TRUE;
			}
		    free_empire_unique_storage(iter);
		}
	}
	
	if (!any) {
		msg_to_char(ch, "You don't seem to be able to retrieve anything like that.\r\n");
	}
	else {
		queue_delayed_update(ch, CDU_SAVE);
	}
}


/**
* Sub-processor for do_warehouse: store.
* This also handles home storage.
*
* @param char_data *ch The player.
* @param char *argument The rest of the argument after "store"
* @param int mode SCMD_WAREHOUSE or SCMD_HOME
*/
void warehouse_store(char_data *ch, char *argument, int mode) {
	bool imm_access = (GET_ACCESS_LEVEL(ch) >= LVL_CIMPL || IS_GRANTED(ch, GRANT_EMPIRES));
	empire_data *room_emp = ROOM_OWNER(IN_ROOM(ch)), *use_emp = NULL;
	bool home_mode = (mode == SCMD_HOME);
	char arg[MAX_INPUT_LENGTH], numarg[MAX_INPUT_LENGTH], *tmp;
	obj_data *obj, *next_obj;
	int total = 1, done = 0, dotmode;
	bool full = FALSE, capped = FALSE, kept = FALSE;
	
	if (!*argument) {
		msg_to_char(ch, "Store what?\r\n");
		return;
	}
	
	// check location/access
	if (home_mode) {
		use_emp = NULL;
		if (!imm_access && ROOM_PRIVATE_OWNER(HOME_ROOM(IN_ROOM(ch))) != GET_IDNUM(ch)) {
			msg_to_char(ch, "You can only do this in your own private home.\r\n");
			return;
		}
	}
	else {	// warehouse mode
		use_emp = GET_LOYALTY(ch);
		if (!imm_access && !room_has_function_and_city_ok(GET_LOYALTY(ch), IN_ROOM(ch), FNC_WAREHOUSE | FNC_VAULT)) {
			msg_to_char(ch, "You can't do that here.\r\n");
			return;
		}
		if (!imm_access && !IS_COMPLETE(IN_ROOM(ch))) {
			msg_to_char(ch, "Complete the building first.\r\n");
			return;
		}
		if (!imm_access && (!can_use_room(ch, IN_ROOM(ch), GUESTS_ALLOWED) || (room_emp && use_emp != room_emp && !has_relationship(use_emp, room_emp, DIPL_TRADE)))) {
			msg_to_char(ch, "You don't have permission to do that here.\r\n");
			return;
		}
		if (!imm_access && room_has_function_and_city_ok(GET_LOYALTY(ch), IN_ROOM(ch), FNC_VAULT) && !has_permission(ch, PRIV_WITHDRAW, IN_ROOM(ch))) {
			msg_to_char(ch, "You don't have permission to store items here.\r\n");
			return;
		}
		if (!check_in_city_requirement(IN_ROOM(ch), TRUE)) {
			msg_to_char(ch, "This building must be in a city to use it for that.\r\n");
			return;
		}
	}
	
	// possible #
	tmp = any_one_arg(argument, numarg);
	skip_spaces(&tmp);
	if (*numarg && is_number(numarg)) {
		total = atoi(numarg);
		if (total < 1) {
			msg_to_char(ch, "You have to store at least 1.\r\n");
			return;
		}
		argument = tmp;
	}
	else if (*tmp && *numarg && !str_cmp(numarg, "all")) {
		total = CAN_CARRY_N(ch) + 1;
		argument = tmp;
	}
	else if (!*tmp && !str_cmp(numarg, "all")) {
		total = CAN_CARRY_N(ch);
		argument = numarg;
	}
	
	// argument may have moved for numarg
	skip_spaces(&argument);
	
	dotmode = find_all_dots(argument);
	
	if (dotmode == FIND_ALL) {
		DL_FOREACH_SAFE2(ch->carrying, obj, next_obj, next_content) {
			if (full) {
				break;	// early break
			}
			
			if (OBJ_FLAGGED(obj, OBJ_KEEP)) {
				kept = TRUE;
			}
			else if (UNIQUE_OBJ_CAN_STORE(obj, home_mode) && check_home_store_cap(ch, obj, FALSE, &capped)) {
				// may extract obj
				store_unique_item(ch, (home_mode ? &GET_HOME_STORAGE(ch) : &EMPIRE_UNIQUE_STORAGE(use_emp)), obj, use_emp, home_mode ? NULL : IN_ROOM(ch), &full);
				if (!full) {
					++done;
				}
			}
		}
		if (capped) {
			if (ch->desc) {
				stack_msg_to_desc(ch->desc, "You have hit the limit of %d unique items in your home storage.\r\n", config_get_int("max_home_store_uniques"));
			}
		}
		else if (!done) {
			if (full) {
				msg_to_char(ch, "It's full.\r\n");
			}
			else {
				msg_to_char(ch, "You don't have %s that can be stored.\r\n", (kept ? "any non-'keep' items" : "anything"));
			}
		}
	}
	else {
		// not "all"
		one_argument(argument, arg);
		
		if (!*arg) {
			msg_to_char(ch, "What do you want to store?\r\n");
			return;
		}
		if (!(obj = get_obj_in_list_vis(ch, arg, NULL, ch->carrying))) {
			msg_to_char(ch, "You don't seem to have any %ss.\r\n", arg);
			return;
		}

		while (obj && (dotmode == FIND_ALLDOT || done < total)) {
			next_obj = get_obj_in_list_vis(ch, arg, NULL, obj->next_content);
			
			if (OBJ_FLAGGED(obj, OBJ_KEEP)) {
				kept = TRUE;	// mark for later
			}
			
			if ((!OBJ_FLAGGED(obj, OBJ_KEEP) || (total == 1 && dotmode != FIND_ALLDOT)) && UNIQUE_OBJ_CAN_STORE(obj, home_mode) && check_home_store_cap(ch, obj, FALSE, &capped)) {
				// may extract obj
				store_unique_item(ch, (home_mode ? &GET_HOME_STORAGE(ch) : &EMPIRE_UNIQUE_STORAGE(use_emp)), obj, use_emp, home_mode ? NULL : IN_ROOM(ch), &full);
				if (!full) {
					++done;
				}
			}
			obj = next_obj;
		}
		if (capped) {
			if (ch->desc) {
				stack_msg_to_desc(ch->desc, "You have hit the limit of %d unique items in your home storage.\r\n", config_get_int("max_home_store_uniques"));
			}
		}
		else if (!done) {
			if (full) {	// this full is for MAX_STORAGE
				msg_to_char(ch, "It's full.\r\n");
			}
			else if (kept) {
				msg_to_char(ch, "You didn't have anything to store that wasn't marked 'keep'.\r\n");
			}
			else {
				msg_to_char(ch, "You can't store that here!\r\n");
			}
		}
	}

	if (done) {
		queue_delayed_update(ch, CDU_SAVE);
		if (use_emp) {
			EMPIRE_NEEDS_STORAGE_SAVE(use_emp) = TRUE;
		}
	}
}


 //////////////////////////////////////////////////////////////////////////////
//// COMMANDS ////////////////////////////////////////////////////////////////

ACMD(do_buy) {
	char buf[MAX_STRING_LENGTH], buf2[MAX_STRING_LENGTH], orig_arg[MAX_INPUT_LENGTH];
	struct shop_temp_list *stl, *shop_list = NULL;
	empire_data *coin_emp = NULL;
	struct shop_item *item;
	obj_data *obj;
	int number;
	
	skip_spaces(&argument);	// optional filter
	strcpy(orig_arg, argument);
	number = get_number(&argument);	// x.name syntax
	
	if (IS_NPC(ch)) {
		msg_to_char(ch, "NPCs don't buy.\r\n");
		return;
	}
	if (FIGHTING(ch)) {
		msg_to_char(ch, "You can't do that right now!\r\n");
		return;
	}
	if (!*argument) {
		msg_to_char(ch, "Buy what?\r\n");
		return;
	}
	
	// find shops: MUST free this before returning
	shop_list = build_available_shop_list(ch);
	
	// now show any shops available
	LL_FOREACH(shop_list, stl) {		
		// the nopes
		if (SHOP_FLAGGED(stl->shop, SHOP_IN_DEVELOPMENT) && !IS_IMMORTAL(ch)) {
			continue;	// in-dev
		}
		
		// list of items
		LL_FOREACH(SHOP_ITEMS(stl->shop), item) {
			if (!(obj = obj_proto(item->vnum))) {
				continue;	// no obj
			}
			if (!multi_isname(argument, GET_OBJ_KEYWORDS(obj))) {
				continue;
			}
			if (--number > 0) {
				continue;
			}
			
			// we have validated which object. Now try to afford it:
			if (SHOP_ALLEGIANCE(stl->shop) && item->min_rep != REP_NONE && !has_reputation(ch, FCT_VNUM(SHOP_ALLEGIANCE(stl->shop)), item->min_rep)) {
				msg_to_char(ch, "You must be at least %s by %s to buy %s.\r\n", reputation_levels[rep_const_to_index(item->min_rep)].name, FCT_NAME(SHOP_ALLEGIANCE(stl->shop)), GET_OBJ_SHORT_DESC(obj));
				free_shop_temp_list(shop_list);
				return;
			}
			if (item->currency == NOTHING) {
				coin_emp = (stl->from_mob ? GET_LOYALTY(stl->from_mob) : (stl->from_room ? ROOM_OWNER(stl->from_room) : NULL));
				if (!can_afford_coins(ch, coin_emp, item->cost)) {
					msg_to_char(ch, "You need %s to buy %s.\r\n", money_amount(coin_emp, item->cost), GET_OBJ_SHORT_DESC(obj));
					free_shop_temp_list(shop_list);
					return;
				}
			}
			else if (item->currency != NOTHING && get_currency(ch, item->currency) < item->cost) {
				msg_to_char(ch, "You need %d %s to buy %s.\r\n", item->cost, get_generic_string_by_vnum(item->currency, GENERIC_CURRENCY, WHICH_CURRENCY(item->cost)), GET_OBJ_SHORT_DESC(obj));
				free_shop_temp_list(shop_list);
				return;
			}
			else if (!CAN_CARRY_OBJ(ch, obj)) {
				msg_to_char(ch, "Your arms are already full!\r\n");
				free_shop_temp_list(shop_list);
				return;
			}
			
			// load the object before the buy trigger, in case
			obj = read_object(item->vnum, TRUE);
			obj_to_char(obj, ch);
			scale_item_to_level(obj, GET_HIGHEST_KNOWN_LEVEL(ch));
			
			if (!check_buy_trigger(ch, stl->from_mob, obj, item->cost, item->currency)) {
				// triggered: purchase failed
				extract_obj(obj);
				return;
			}
			
			// finish the purchase
			if (item->currency == NOTHING) {
				charge_coins(ch, coin_emp, item->cost, NULL, buf2);
				sprintf(buf, "You buy $p for %s.", buf2);
			}
			else {
				add_currency(ch, item->currency, -(item->cost));
				sprintf(buf, "You buy $p for %d %s.", item->cost, get_generic_string_by_vnum(item->currency, GENERIC_CURRENCY, WHICH_CURRENCY(item->cost)));
			}
			
			act(buf, FALSE, ch, obj, NULL, TO_CHAR);
			act("$n buys $p.", FALSE, ch, obj, NULL, TO_ROOM);
			
			if (load_otrigger(obj)) {
				get_check_money(ch, obj);
			}
			
			free_shop_temp_list(shop_list);
			return;	// done now
		}
	}
	
	// did we make it this far?
	msg_to_char(ch, "You don't see any %s for sale here.\r\n", orig_arg);
	free_shop_temp_list(shop_list);
}


ACMD(do_combine) {
	char arg[MAX_INPUT_LENGTH];
	obj_data *obj;
	
	one_argument(argument, arg);
	
	if (!*arg) {
		msg_to_char(ch, "Combine what?\r\n");
	}
	else if (!(obj = get_obj_in_list_vis_prefer_interaction(ch, arg, NULL, ch->carrying, INTERACT_COMBINE))) {
		msg_to_char(ch, "You don't have %s %s.\r\n", AN(arg), arg);
	}
	else if (!has_interaction(GET_OBJ_INTERACTIONS(obj), INTERACT_COMBINE)) {
		msg_to_char(ch, "You can't combine that!\r\n");
	}
	else {
		// will extract no matter what happens here
		if (!run_interactions(ch, GET_OBJ_INTERACTIONS(obj), INTERACT_COMBINE, IN_ROOM(ch), NULL, obj, NULL, combine_obj_interact)) {
			act("You fail to combine $p.", FALSE, ch, obj, NULL, TO_CHAR);
		}
		command_lag(ch, WAIT_OTHER);
	}
}


ACMD(do_compare) {
	char arg[MAX_INPUT_LENGTH], arg2[MAX_INPUT_LENGTH];
	obj_data *obj, *to_obj = NULL;
	bool extract_from = FALSE, extract_to = FALSE, inv_only = FALSE;
	char_data *tmp_char;
	vehicle_data *tmp_veh;
	int iter, pos;
	
	// compare [inventory] <first obj> [second obj]
	argument = two_arguments(argument, arg, arg2);
	
	// if first arg is 'inv', restrict to inventory
	if (strlen(arg) >= 3 && is_abbrev(arg, "inventory")) {
		inv_only = TRUE;
		strcpy(arg, arg2);
		argument = one_argument(argument, arg2);
	}
	
	if (GET_POS(ch) == POS_FIGHTING) {
		msg_to_char(ch, "You're too busy to do that now!\r\n");
		return;
	}
	if (!*arg) {
		msg_to_char(ch, "Identify what object%s?\r\n", inv_only ? " in your inventory" : "");
		return;
	}
	if (!generic_find(arg, NULL, inv_only ? FIND_OBJ_INV : (FIND_OBJ_INV | FIND_OBJ_ROOM | FIND_OBJ_EQUIP), ch, &tmp_char, &obj, &tmp_veh)) {
		msg_to_char(ch, "You see nothing like that %s.\r\n", inv_only ? "in your inventory" : "here");
		return;
	}
	
	// ok we have the 1st obj; find the 2nd
	if (*arg2 && !generic_find(arg2, NULL, inv_only ? FIND_OBJ_INV : (FIND_OBJ_INV | FIND_OBJ_ROOM | FIND_OBJ_EQUIP), ch, &tmp_char, &to_obj, &tmp_veh)) {
		msg_to_char(ch, "You don't seem to have %s %s to compare it to.\r\n", AN(arg2), arg2);
		return;
	}
	
	// ok
	charge_ability_cost(ch, NOTHING, 0, NOTHING, 0, WAIT_OTHER);
	
	// check identifies-to:
	if (has_interaction(GET_OBJ_INTERACTIONS(obj), INTERACT_IDENTIFIES_TO) && (WORN_OR_CARRIED_BY(obj, ch) || can_use_room(ch, IN_ROOM(ch), MEMBERS_ONLY))) {
		act("$n identifies $p.", TRUE, ch, obj, NULL, TO_ROOM);
		run_identifies_to(ch, &obj, &extract_from);
		if (ch->desc) {
			send_stacked_msgs(ch->desc);	// flush the stacked id message before id'ing it
		}
	}
	if (to_obj && has_interaction(GET_OBJ_INTERACTIONS(to_obj), INTERACT_IDENTIFIES_TO) && (WORN_OR_CARRIED_BY(to_obj, ch) || can_use_room(ch, IN_ROOM(ch), MEMBERS_ONLY))) {
		act("$n identifies $p.", TRUE, ch, to_obj, NULL, TO_ROOM);
		run_identifies_to(ch, &to_obj, &extract_to);
		if (ch->desc) {
			send_stacked_msgs(ch->desc);	// flush the stacked id message before id'ing it
		}
	}
	
	// ensure scaling
	if (!IS_IMMORTAL(ch) && GET_OBJ_CURRENT_SCALE_LEVEL(obj) == 0) {
		scale_item_to_level(obj, get_approximate_level(ch));
	}
	
	identify_obj_to_char(obj, ch, TRUE);
	
	if (to_obj) {
		msg_to_char(ch, "\r\n");
		identify_obj_to_char(to_obj, ch, TRUE);
		act("$n compares $p to $P.", TRUE, ch, obj, to_obj, TO_ROOM | ACT_OBJ_VICT);
	}
	else {
		// detect?
		for (iter = 0; BIT(iter) < GET_OBJ_WEAR(obj); ++iter) {
			if (BIT(iter) != ITEM_WEAR_TAKE && CAN_WEAR(obj, BIT(iter))) {
				pos = get_wear_by_item_wear(BIT(iter));
				if (GET_EQ(ch, pos)) {
					msg_to_char(ch, "\r\n");
					identify_obj_to_char(GET_EQ(ch, pos), ch, TRUE);
					act("$n compares $p to $P.", TRUE, ch, obj, GET_EQ(ch, pos), TO_ROOM | ACT_OBJ_VICT);
				}
				// cascade?
				while ((pos = wear_data[pos].cascade_pos) != NO_WEAR) {
					if (GET_EQ(ch, pos)) {
						msg_to_char(ch, "\r\n");
						identify_obj_to_char(GET_EQ(ch, pos), ch, TRUE);
						act("$n compares $p to $P.", TRUE, ch, obj, GET_EQ(ch, pos), TO_ROOM | ACT_OBJ_VICT);
					}
				}
			}
		}
	}

	// check requested extractions
	if (extract_from) {
		extract_obj(obj);
	}
	if (to_obj && extract_to) {
		extract_obj(to_obj);
	}
}


ACMD(do_draw) {
	obj_data *obj = NULL, *removed = NULL;
	int loc = 0;

	one_argument(argument, arg);

	if (!*arg) {
		// attempt to guess
		if (GET_EQ(ch, WEAR_SHEATH_1) && !GET_EQ(ch, WEAR_SHEATH_2)) {
			obj = GET_EQ(ch, WEAR_SHEATH_1);
			loc = WEAR_SHEATH_1;
		}
		else if (GET_EQ(ch, WEAR_SHEATH_2) && !GET_EQ(ch, WEAR_SHEATH_1)) {
			obj = GET_EQ(ch, WEAR_SHEATH_2);
			loc = WEAR_SHEATH_2;
		}
		else {	// have 2 things sheathed
			msg_to_char(ch, "Draw what?\r\n");
			return;
		}
	}
	
	// detect based on args
	if (!obj) {
		if ((obj = GET_EQ(ch, WEAR_SHEATH_1)) && isname(arg, GET_OBJ_KEYWORDS(obj)))
			loc = WEAR_SHEATH_1;
		else if ((obj = GET_EQ(ch, WEAR_SHEATH_2)) && isname(arg, GET_OBJ_KEYWORDS(obj)))
			loc = WEAR_SHEATH_2;
		else {
			msg_to_char(ch, "You have nothing by that name sheathed!\r\n");
			return;
		}
	}
	
	if (OBJ_FLAGGED(obj, OBJ_TWO_HANDED) && GET_EQ(ch, WEAR_HOLD)) {
		msg_to_char(ch, "You can't draw a two-handed weapon while you're holding something in your off-hand.\r\n");
		return;
	}

	// attempt to remove existing wield
	if (GET_EQ(ch, WEAR_WIELD)) {
		removed = perform_remove(ch, WEAR_WIELD);
		
		// did it work? if not, player got an error
		if (GET_EQ(ch, WEAR_WIELD)) {
			return;
		}
	}

	// SUCCESS
	// do not use unequip_char_to_inventory so that we don't hit OBJ_SINGLE_USE
	obj_to_char(unequip_char(ch, loc), ch);
	perform_wear(ch, obj, WEAR_WIELD);
	
	// attempt to sheathe the removed item
	if (removed && !GET_EQ(ch, loc)) {
		perform_wear(ch, removed, loc);
	}
	
	if (FIGHTING(ch)) {
		command_lag(ch, WAIT_COMBAT_ABILITY);
	}
}


ACMD(do_drink) {
	struct string_hash *str_iter, *next_str, *str_hash = NULL;
	char buf[MAX_STRING_LENGTH], line[256], part[256];
	char *thirst_str;
	obj_data *obj = NULL, *check_list[2];
	int amount, i, liquid;
	double thirst_amt, hunger_amt;
	int type = drink_OBJ, number, iter, warmed;
	room_data *to_room;
	char *argptr = arg;
	size_t size;
	generic_data *liq_generic;
	vehicle_data *veh;
	
	#define LIQ_VAL(val)  (liq_generic ? GEN_VALUE(liq_generic, (val)) : 0)

	one_argument(argument, arg);
	number = get_number(&argptr);

	if (REAL_NPC(ch))		/* Cannot use GET_COND() on mobs. */
		return;

	if (!*argptr) {
		if (ROOM_SECT_FLAGGED(IN_ROOM(ch), SECTF_DRINK))
			type = drink_ROOM;
		else if (room_has_function_and_city_ok(GET_LOYALTY(ch), IN_ROOM(ch), FNC_DRINK_WATER)) {
			if (!can_drink_from_room(ch, (type = drink_ROOM))) {
				return;
			}
		}
		else if (find_flagged_sect_within_distance_from_char(ch, SECTF_DRINK, NOBITS, 1)) {
			type = drink_ROOM;
		}
		else {	// no-arg and no room water: try to show drinkables
			check_list[0] = ch->carrying;
			check_list[1] = can_use_room(ch, IN_ROOM(ch), MEMBERS_AND_ALLIES) ? ROOM_CONTENTS(IN_ROOM(ch)) : NULL;
			for (iter = 0; iter < 2; ++iter) {
				if (check_list[iter]) {
					DL_FOREACH2(check_list[iter], obj, next_content) {
						if (IS_DRINK_CONTAINER(obj)) {
							if (GET_DRINK_CONTAINER_CONTENTS(obj) > 0) {
								snprintf(line, sizeof(line), "%s - %d drink%s of %s", GET_OBJ_DESC(obj, ch, OBJ_DESC_SHORT), GET_DRINK_CONTAINER_CONTENTS(obj), PLURAL(GET_DRINK_CONTAINER_CONTENTS(obj)), get_generic_string_by_vnum(GET_DRINK_CONTAINER_TYPE(obj), GENERIC_LIQUID, GSTR_LIQUID_NAME));
							}
							else {
								snprintf(line, sizeof(line), "%s - empty", GET_OBJ_DESC(obj, ch, OBJ_DESC_SHORT));
							}
							add_string_hash(&str_hash, line, 1);
						}
					}
				}
			}
			DL_FOREACH2(ROOM_VEHICLES(IN_ROOM(ch)), veh, next_in_room) {
				if (VEH_IS_COMPLETE(veh) && vehicle_has_function_and_city_ok(veh, FNC_DRINK_WATER)) {
					add_string_hash(&str_hash, VEH_SHORT_DESC(veh), 1);
				}
			}
		
			if (str_hash) {
				// show thirst?
				if ((thirst_str = how_thirsty(ch))) {
					snprintf(part, sizeof(part), " (you are %s)", thirst_str);
				}
				else {
					*part = '\0';
				}
				
				// show drinkables
				size = snprintf(buf, sizeof(buf), "What do you want to drink from%s:\r\n", part);
				HASH_ITER(hh, str_hash, str_iter, next_str) {
					if (str_iter->count == 1) {
						snprintf(line, sizeof(line), " %s\r\n", str_iter->str);
					}
					else {
						snprintf(line, sizeof(line), " %s (x%d)\r\n", str_iter->str, str_iter->count);
					}
					if (size + strlen(line) + 16 < sizeof(buf)) {
						strcat(buf, line);
						size += strlen(line);
					}
					else {
						size += snprintf(buf + size, sizeof(buf) - size, "OVERFLOW\r\n");
						break;
					}
				}
				free_string_hash(&str_hash);
				if (ch->desc) {
					page_string(ch->desc, buf, TRUE);
				}
			}
			else {
				// nothing to plant
				msg_to_char(ch, "Drink from what?\r\n");
			}
			return;
		}
	}

	if (type == NOTHING && !(obj = get_obj_in_list_vis(ch, argptr, &number, ch->carrying)) && (!can_use_room(ch, IN_ROOM(ch), MEMBERS_AND_ALLIES) || !(obj = get_obj_in_list_vis(ch, argptr, &number, ROOM_CONTENTS(IN_ROOM(ch)))))) {
		if ((veh = get_vehicle_in_room_vis(ch, argptr, NULL)) && vehicle_has_function_and_city_ok(veh, FNC_DRINK_WATER)) {
			// try the vehicle
			if (!VEH_IS_COMPLETE(veh)) {
				act("You need to complete $V before drinking from it.", FALSE, ch, NULL, veh, TO_CHAR | ACT_VEH_VICT);
				return;
			}
			// otherwise ok:
			type = drink_ROOM;
		}
		else if (room_has_function_and_city_ok(GET_LOYALTY(ch), IN_ROOM(ch), FNC_DRINK_WATER) && (is_abbrev(argptr, "water") || isname(argptr, get_room_name(IN_ROOM(ch), FALSE)))) {
			if (!can_drink_from_room(ch, (type = drink_ROOM))) {
				return;
			}
		}
		if (is_abbrev(argptr, "river") || is_abbrev(argptr, "water")) {
			if (ROOM_SECT_FLAGGED(IN_ROOM(ch), SECTF_DRINK))
				type = drink_ROOM;
			else if (!IS_ADVENTURE_ROOM(IN_ROOM(ch)) && IS_OUTDOORS(ch)) {
				for (i = 0; i < NUM_2D_DIRS; i++) {
					if ((to_room = real_shift(IN_ROOM(ch), shift_dir[i][0], shift_dir[i][1])) && ROOM_SECT_FLAGGED(to_room, SECTF_DRINK)) {
						type = drink_ROOM;
					}
				}
			}
		}

		if (type == NOTHING) {
			send_to_char("You can't find it!\r\n", ch);
			return;
		}
	}

	if (obj && GET_OBJ_TYPE(obj) != ITEM_DRINKCON) {
		act("$p: You can't drink from that!", FALSE, ch, obj, FALSE, TO_CHAR);
		return;
	}
	if (obj && !bind_ok(obj, ch)) {
		msg_to_char(ch, "You can't drink from an item that's bound to someone else.\r\n");
		return;
	}

	/* The pig is drunk */
	if (IS_DRUNK(ch) && !IS_THIRSTY(ch)) {
		if (type >= 0)
			msg_to_char(ch, "You're so inebriated that you don't dare get that close to the water.\r\n");
		else {
			msg_to_char(ch, "You can't seem to get close enough to your mouth.\r\n");
			act("$n tries to drink but misses $s mouth!", TRUE, ch, 0, 0, TO_ROOM);
		}
		return;
	}

	/*
	if (GET_COND(ch, THIRST) < REAL_UPDATES_PER_MUD_HOUR && GET_COND(ch, THIRST) != UNLIMITED) {
		msg_to_char(ch, "You're too full to drink anymore!\r\n");
		return;
	}

	if (GET_COND(ch, FULL) < REAL_UPDATES_PER_MUD_HOUR && GET_COND(ch, FULL) != UNLIMITED && GET_COND(ch, THIRST) < 24 * REAL_UPDATES_PER_MUD_HOUR && GET_COND(ch, THIRST) != UNLIMITED) {
		send_to_char("Your stomach can't contain anymore!\r\n", ch);
		return;
	}
	*/

	if (obj && GET_DRINK_CONTAINER_CONTENTS(obj) <= 0) {
		send_to_char("It's empty.\r\n", ch);
		return;
	}

	/* check trigger */
	if (obj && !consume_otrigger(obj, ch, (subcmd == SCMD_SIP) ? OCMD_SIP : OCMD_DRINK, NULL)) {
		return;
	}

	// send message
	drink_message(ch, obj, type, subcmd, &liquid);
	
	// no modifiers on sip
	if (subcmd == SCMD_SIP) {
		return;
	}
	
	// load the generic now (after drink_message, which can modify liquid
	liq_generic = real_generic(liquid);
	if (!liq_generic || GEN_TYPE(liq_generic) != GENERIC_LIQUID) {
		// clear it if invalid
		liq_generic = NULL;
	}
	
	// "amount" will be how many gulps to take from the CAPACITY
	if (liq_generic && IS_SET(GET_LIQUID_FLAGS(liq_generic), LIQF_BLOOD)) {
		// drinking blood?
		amount = GET_MAX_BLOOD(ch) - GET_BLOOD(ch);
	}
	else if ((LIQ_VAL(GVAL_LIQUID_THIRST) > 0 && GET_COND(ch, THIRST) == UNLIMITED) || (LIQ_VAL(GVAL_LIQUID_FULL) > 0 && GET_COND(ch, FULL) == UNLIMITED) || (LIQ_VAL(GVAL_LIQUID_DRUNK) > 0 && GET_COND(ch, DRUNK) == UNLIMITED)) {
		// if theirs is unlimited
		amount = 1;
	}
	else {
		// how many hours of thirst I have, divided by how much thirst it gives per hour
		thirst_amt = GET_COND(ch, THIRST) == UNLIMITED ? 1 : (((double) GET_COND(ch, THIRST) / REAL_UPDATES_PER_MUD_HOUR) / (double) LIQ_VAL(GVAL_LIQUID_THIRST));
		hunger_amt = GET_COND(ch, FULL) == UNLIMITED ? 1 : (((double) GET_COND(ch, FULL) / REAL_UPDATES_PER_MUD_HOUR) / (double) LIQ_VAL(GVAL_LIQUID_FULL));
		
		// round up
		thirst_amt = ceil(thirst_amt);
		hunger_amt = ceil(hunger_amt);
	
		// whichever is more
		amount = MAX((int)thirst_amt, (int)hunger_amt);
	
		// if it causes drunkenness, minimum of 1
		if (LIQ_VAL(GVAL_LIQUID_DRUNK) > 0) {
			amount = MAX(1, amount);
		}
	}
	
	// check warming/cooling needs
	if (liq_generic && IS_SET(GET_LIQUID_FLAGS(liq_generic), LIQF_WARMING) && GET_TEMPERATURE(ch) < (get_room_temperature(IN_ROOM(ch)) + 5)) {
		// needs warming: drink at least 4
		amount = MAX(4, amount);
	}
	if (liq_generic && IS_SET(GET_LIQUID_FLAGS(liq_generic), LIQF_COOLING) && GET_TEMPERATURE(ch) > (get_room_temperature(IN_ROOM(ch)) - 5)) {
		// needs cooling: drink at least 4
		amount = MAX(4, amount);
	}
	
	// amount is now the number of gulps to take to fill the player
	
	if (obj) {
		// bound by contents available
		amount = MIN(GET_DRINK_CONTAINER_CONTENTS(obj), amount);
		
		// can no longer be stored to prevent refilling, if single-use
		if (OBJ_FLAGGED(obj, OBJ_SINGLE_USE)) {
			SET_BIT(GET_OBJ_EXTRA(obj), OBJ_NO_BASIC_STORAGE);
		}
	}
	
	// apply effects
	if (liq_generic && IS_SET(GET_LIQUID_FLAGS(liq_generic), LIQF_BLOOD)) {
		set_blood(ch, GET_BLOOD(ch) + amount);
	}
	
	// -1 to remove condition, amount = number of gulps
	gain_condition(ch, THIRST, -1 * LIQ_VAL(GVAL_LIQUID_THIRST) * REAL_UPDATES_PER_MUD_HOUR * amount);
	gain_condition(ch, FULL, -1 * LIQ_VAL(GVAL_LIQUID_FULL) * REAL_UPDATES_PER_MUD_HOUR * amount);
	// drunk goes positive instead of negative
	gain_condition(ch, DRUNK, 1 * LIQ_VAL(GVAL_LIQUID_DRUNK) * REAL_UPDATES_PER_MUD_HOUR * amount);
	
	// check warming
	warmed = warm_player_from_liquid(ch, amount, liquid);
	if (warmed > 0) {
		msg_to_char(ch, "It warms you up%s\r\n", (get_relative_temperature(ch) >= config_get_int("temperature_discomfort") ? " -- you're getting too hot!" : "."));
	}
	else if (warmed < 0) {
		msg_to_char(ch, "It cools you down%s\r\n", (get_relative_temperature(ch) <= (-1 * config_get_int("temperature_discomfort")) ? " -- you're getting too cold!" : "."));
	}
	
	// messages based on what changed
	if (GET_COND(ch, DRUNK) > 12 * REAL_UPDATES_PER_MUD_HOUR && LIQ_VAL(GVAL_LIQUID_DRUNK) != 0) {
		send_to_char("You feel drunk.\r\n", ch);
	}
	else if (GET_COND(ch, DRUNK) > 2 * REAL_UPDATES_PER_MUD_HOUR && LIQ_VAL(GVAL_LIQUID_DRUNK) != 0) {
		send_to_char("You feel tipsy.\r\n", ch);
	}
	if (GET_COND(ch, THIRST) < 3 * REAL_UPDATES_PER_MUD_HOUR && LIQ_VAL(GVAL_LIQUID_THIRST) != 0) {
		send_to_char("You don't feel thirsty any more.\r\n", ch);
	}
	if (GET_COND(ch, FULL) < 3 * REAL_UPDATES_PER_MUD_HOUR && LIQ_VAL(GVAL_LIQUID_FULL) != 0) {
		send_to_char("You are full.\r\n", ch);
	}

	if (obj) {
		// ensure binding
		if (!IS_NPC(ch) && OBJ_FLAGGED(obj, OBJ_BIND_FLAGS)) {
			bind_obj_to_player(obj, ch);
			reduce_obj_binding(obj, ch);
		}
		
		// reduce contents
		set_obj_val(obj, VAL_DRINK_CONTAINER_CONTENTS, GET_DRINK_CONTAINER_CONTENTS(obj) - amount);
		
		if (GET_DRINK_CONTAINER_CONTENTS(obj) <= 0) {	/* The last bit */
			set_obj_val(obj, VAL_DRINK_CONTAINER_TYPE, 0);
			GET_OBJ_TIMER(obj) = UNLIMITED;
			request_obj_save_in_world(obj);
			
			if (OBJ_FLAGGED(obj, OBJ_SINGLE_USE)) {
				run_interactions(ch, GET_OBJ_INTERACTIONS(obj), INTERACT_CONSUMES_TO, IN_ROOM(ch), NULL, obj, NULL, consumes_or_decays_interact);
				empty_obj_before_extract(obj);
				extract_obj(obj);
			}
		}
	}
}


ACMD(do_drop) {
	obj_data *obj, *next_obj;
	byte mode = SCMD_DROP;
	int this, dotmode, multi, coin_amt;
	empire_data *coin_emp;
	const char *sname;
	bool any = FALSE;
	char *argpos;

	switch (subcmd) {
		case SCMD_JUNK:
			sname = "junk";
			mode = SCMD_JUNK;
			break;
		default:
			sname = "drop";
			break;
	}
	
	// drop coins?
	if ((argpos = find_coin_arg(argument, &coin_emp, &coin_amt, TRUE, FALSE, NULL)) > argument && coin_amt > 0) {
		perform_drop_coins(ch, coin_emp, coin_amt, mode);
		return;
	}

	argument = one_argument(argument, arg);

	if (!*arg) {
		sprintf(buf, "What do you want to %s?\r\n", sname);
		send_to_char(buf, ch);
		return;
	}
	else if (is_number(arg)) {
		multi = atoi(arg);
		one_argument(argument, arg);
		if (multi <= 0)
			send_to_char("Yeah, that makes sense.\r\n", ch);
		else if (!*arg) {
			sprintf(buf, "What do you want to %s %d of?\r\n", sname, multi);
			send_to_char(buf, ch);
			}
		else if (!(obj = get_obj_in_list_vis(ch, arg, NULL, ch->carrying))) {
			if (!str_cmp(arg, "coin") || !str_cmp(arg, "coins")) {
				msg_to_char(ch, "What kind of coins do you want to %s?\r\n", sname);
			}
			else {
				msg_to_char(ch, "You don't seem to have any %ss.\r\n", arg);
			}
		}
		else {
			do {
				next_obj = get_obj_in_list_vis(ch, arg, NULL, obj->next_content);
				this = perform_drop(ch, obj, mode, sname);
				obj = next_obj;
				if (this == -1) {
					break;
				}
				else {
					// amount += this;
				}
			} while (obj && --multi);
		}
	}
	else {
		dotmode = find_all_dots(arg);

		/* Can't junk all */
		if ((dotmode == FIND_ALL) && (subcmd == SCMD_JUNK)) {
			send_to_char("You can't junk EVERYTHING!\r\n", ch);
			return;
		}
		if (dotmode == FIND_ALL) {
			if (!ch->carrying)
				send_to_char("You don't seem to be carrying anything.\r\n", ch);
			else {
			    DL_FOREACH_SAFE2(ch->carrying, obj, next_obj, next_content) {
					if (OBJ_FLAGGED(obj, OBJ_KEEP)) {
						continue;
					}
					
					this = perform_drop(ch, obj, mode, sname);
					if (this == -1) {
						break;
					}
					else {
						// amount += this;
						any = TRUE;
					}
				}
				
				if (!any) {
					msg_to_char(ch, "You don't have anything that isn't marked 'keep'.\r\n");
				}
			}
		}
		else if (dotmode == FIND_ALLDOT) {
			if (!*arg) {
				sprintf(buf, "What do you want to %s all of?\r\n", sname);
				send_to_char(buf, ch);
				return;
			}
			if (!(obj = get_obj_in_list_vis(ch, arg, NULL, ch->carrying))) {
				if (!str_cmp(arg, "coin") || !str_cmp(arg, "coins")) {
					msg_to_char(ch, "What kind of coins do you want to %s?\r\n", sname);
				}
				else {
					msg_to_char(ch, "You don't seem to have any %ss.\r\n", arg);
				}
				return;
			}
			while (obj) {
				next_obj = get_obj_in_list_vis(ch, arg, NULL, obj->next_content);
				if (OBJ_FLAGGED(obj, OBJ_KEEP)) {
					obj = next_obj;
					continue;
				}
				
				this = perform_drop(ch, obj, mode, sname);
				obj = next_obj;
				if (this == -1) {
					break;
				}
				else {
					// amount += this;
					any = TRUE;
				}
			}
			
			if (!any) {
				msg_to_char(ch, "You don't have any that aren't marked 'keep'.\r\n");
			}
		}
		else {
			if (!(obj = get_obj_in_list_vis(ch, arg, NULL, ch->carrying))) {
				if (!str_cmp(arg, "coin") || !str_cmp(arg, "coins")) {
					msg_to_char(ch, "What kind of coins do you want to %s?\r\n", sname);
				}
				else {
					msg_to_char(ch, "You don't seem to have %s %s.\r\n", AN(arg), arg);
				}
			}
			else {
				// amount += 
				perform_drop(ch, obj, mode, sname);
			}
		}
	}
}


ACMD(do_eat) {
	struct string_hash *str_iter, *next_str, *str_hash = NULL;
	bool extract = FALSE, will_buff = FALSE;
	char buf[MAX_STRING_LENGTH], some_part[256], line[256], *argptr = arg;
	char *hungry_str;
	struct affected_type *af;
	struct obj_apply *apply;
	obj_data *food, *check_list[2], *obj;
	int eat_hours, number, iter;
	size_t size;

	one_argument(argument, arg);
	number = get_number(&argptr);
	
	// 1. basic validation
	if (REAL_NPC(ch)) {		/* Cannot use GET_COND() on mobs. */
		return;
	}
	if (!*argptr) {	// no-arg: try to show edibles
		// check inventory for edibles...
		check_list[0] = ch->carrying;
		check_list[1] = can_use_room(ch, IN_ROOM(ch), MEMBERS_AND_ALLIES) ? ROOM_CONTENTS(IN_ROOM(ch)) : NULL;
		for (iter = 0; iter < 2; ++iter) {
			if (check_list[iter]) {
				DL_FOREACH2(check_list[iter], obj, next_content) {
					if (IS_FOOD(obj)) {
						snprintf(line, sizeof(line), "%s - %d hour%s%s", GET_OBJ_DESC(obj, ch, OBJ_DESC_SHORT), GET_FOOD_HOURS_OF_FULLNESS(obj), PLURAL(GET_FOOD_HOURS_OF_FULLNESS(obj)), (GET_OBJ_APPLIES(obj) || GET_OBJ_AFF_FLAGS(obj)) ? ", buff" : "");
						add_string_hash(&str_hash, line, 1);
					}
				}
			}
		}
		
		if (str_hash) {
			// show hunger?
			if ((hungry_str = how_hungry(ch))) {
				snprintf(some_part, sizeof(some_part), " (you are %s)", hungry_str);
			}
			else {
				*some_part = '\0';
			}
			
			// show eatables
			size = snprintf(buf, sizeof(buf), "What do you want to eat%s:\r\n", some_part);
			HASH_ITER(hh, str_hash, str_iter, next_str) {
				if (str_iter->count == 1) {
					snprintf(line, sizeof(line), " %s\r\n", str_iter->str);
				}
				else {
					snprintf(line, sizeof(line), " %s (x%d)\r\n", str_iter->str, str_iter->count);
				}
				if (size + strlen(line) + 16 < sizeof(buf)) {
					strcat(buf, line);
					size += strlen(line);
				}
				else {
					size += snprintf(buf + size, sizeof(buf) - size, "OVERFLOW\r\n");
					break;
				}
			}
			free_string_hash(&str_hash);
			if (ch->desc) {
				page_string(ch->desc, buf, TRUE);
			}
		}
		else {
			// nothing to plant
			msg_to_char(ch, "Eat what?\r\n");
		}
		return;
	}
	if (!(food = get_obj_in_list_vis(ch, argptr, &number, ch->carrying))) {
		if (!can_use_room(ch, IN_ROOM(ch), MEMBERS_AND_ALLIES) || !(food = get_obj_in_list_vis(ch, argptr, &number, ROOM_CONTENTS(IN_ROOM(ch))))) {
			sprintf(buf, "You don't seem to have %s %s.\r\n", AN(arg), arg);
			send_to_char(buf, ch);
			return;
		}
	}
	if (GET_OBJ_TYPE(food) == ITEM_DRINKCON) {
		do_drink(ch, argument, 0, subcmd == SCMD_TASTE ? SCMD_SIP : SCMD_DRINK);
		return;
	}
	if ((GET_OBJ_TYPE(food) != ITEM_FOOD)) {
		act("$p: You can't eat THAT!", FALSE, ch, food, NULL, TO_CHAR);
		return;
	}
	if (!bind_ok(food, ch)) {
		msg_to_char(ch, "You can't eat something that's bound to someone else.\r\n");
		return;
	}
	
	// 2. determine how much they would eat and whether or not there's a buff
	if (subcmd == SCMD_EAT) {
		will_buff = (GET_OBJ_APPLIES(food) || GET_OBJ_AFF_FLAGS(food));
		eat_hours = GET_FOOD_HOURS_OF_FULLNESS(food);
	}
	else {	// just tasting
		eat_hours = 1;
		will_buff = FALSE;
	}

	/* check trigger */
	if (!consume_otrigger(food, ch, (subcmd == SCMD_TASTE) ? OCMD_TASTE : OCMD_EAT, NULL)) {
		return;
	}
	
	// 3. apply caps
	if (will_buff) {
		// limit to 24 hours at once (1 day)
		eat_hours = MIN(eat_hours, 24);
	}
	else {
		// limit to how hungry they are
		eat_hours = MIN(eat_hours, (GET_COND(ch, FULL) / REAL_UPDATES_PER_MUD_HOUR));
	}
	eat_hours = MAX(eat_hours, 0);
	
	// stomach full: only if we're not using it for a buff effect
	if (eat_hours == 0 && GET_COND(ch, FULL) != UNLIMITED && !will_buff) {
		send_to_char("You are too full to eat more!\r\n", ch);
		return;
	}
	
	// 4. ready to eat
	gain_condition(ch, FULL, -1 * eat_hours * REAL_UPDATES_PER_MUD_HOUR);
	if (IS_FOOD(food)) {
		set_obj_val(food, VAL_FOOD_HOURS_OF_FULLNESS, GET_FOOD_HOURS_OF_FULLNESS(food) - eat_hours);
		extract = (GET_FOOD_HOURS_OF_FULLNESS(food) <= 0);
		SET_BIT(GET_OBJ_EXTRA(food), OBJ_NO_BASIC_STORAGE);	// no longer storable
	}
	
	// 5. messaging
	if (extract || subcmd == SCMD_EAT) {
		// determine how the "some" will be shown
		if (extract) {
			// eating the whole thing
			*some_part = '\0';
		}
		else {
			if (!strn_cmp(GET_OBJ_SHORT_DESC(food), "a ", 2) || !strn_cmp(GET_OBJ_SHORT_DESC(food), "an ", 3) || !strn_cmp(GET_OBJ_SHORT_DESC(food), "the ", 4)) {
				strcpy(some_part, "some of ");
			}
			else if (!strn_cmp(GET_OBJ_SHORT_DESC(food), "some ", 5)) {
				// prevents "some of some"
				strcpy(some_part, "part of ");
			}
			else {
				strcpy(some_part, "some ");
			}
		}
		
		// message to char
		if (obj_has_custom_message(food, OBJ_CUSTOM_CONSUME_TO_CHAR)) {
			act(obj_get_custom_message(food, OBJ_CUSTOM_CONSUME_TO_CHAR), FALSE, ch, food, NULL, TO_CHAR);
		}
		else {
			snprintf(buf, sizeof(buf), "You eat %s$p.", some_part);
			act(buf, FALSE, ch, food, NULL, TO_CHAR);
		}

		// message to room
		if (obj_has_custom_message(food, OBJ_CUSTOM_CONSUME_TO_ROOM)) {
			act(obj_get_custom_message(food, OBJ_CUSTOM_CONSUME_TO_ROOM), FALSE, ch, food, NULL, TO_ROOM);
		}
		else {
			snprintf(buf, sizeof(buf), "$n eats %s$p.", some_part);
			act(buf, TRUE, ch, food, NULL, TO_ROOM);
		}
	}
	else {
		// just tasting
		act("You nibble a little bit of $p.", FALSE, ch, food, 0, TO_CHAR);
		act("$n tastes a little bit of $p.", TRUE, ch, food, 0, TO_ROOM);
	}
	
	// 6. apply buffs
	if (will_buff && eat_hours > 0) {
		// ensure scaled
		if (OBJ_FLAGGED(food, OBJ_SCALABLE)) {
			scale_item_to_level(food, 1);	// minimum level
		}
		
		// remove any old buffs
		affect_from_char(ch, ATYPE_WELL_FED, FALSE);
		
		if (GET_OBJ_AFF_FLAGS(food)) {
			af = create_flag_aff(ATYPE_WELL_FED, eat_hours * SECS_PER_MUD_HOUR, GET_OBJ_AFF_FLAGS(food), ch);
			affect_to_char(ch, af);
			free(af);
		}

		LL_FOREACH(GET_OBJ_APPLIES(food), apply) {
			af = create_mod_aff(ATYPE_WELL_FED, eat_hours * SECS_PER_MUD_HOUR, apply->location, apply->modifier, ch);
			affect_to_char(ch, af);
			free(af);
		}
		
		msg_to_char(ch, "You feel well-fed.\r\n");
	}
	else if (GET_COND(ch, FULL) < 3 * REAL_UPDATES_PER_MUD_HOUR) {	// additional messages
		send_to_char("You are full.\r\n", ch);
	}
	
	// 7. cleanup
	if (extract) {
		run_interactions(ch, GET_OBJ_INTERACTIONS(food), INTERACT_CONSUMES_TO, IN_ROOM(ch), NULL, food, NULL, consumes_or_decays_interact);
		extract_obj(food);
	}
	else {
		if (!IS_NPC(ch) && OBJ_FLAGGED(food, OBJ_BIND_FLAGS)) {
			bind_obj_to_player(food, ch);
			reduce_obj_binding(food, ch);
		}
		request_obj_save_in_world(food);
	}
}


ACMD(do_empty) {
	bool messaged;
	obj_data *found_obj, *obj, *next_obj;
	vehicle_data *found_veh;
	
	// may need these
	int size = count_objs_in_room(IN_ROOM(ch));
	int item_limit = config_get_int("room_item_limit");
	
	one_argument(argument, arg);
	if (!*arg) {
		msg_to_char(ch, "Empty what?\r\n");
		return;
	}
	
	generic_find(arg, NULL, FIND_OBJ_INV | FIND_OBJ_ROOM | FIND_VEHICLE_ROOM, ch, NULL, &found_obj, &found_veh);
	
	if (found_veh) {
		if (VEH_CAPACITY(found_veh) <= 0) {
			msg_to_char(ch, "You can't empty that.\r\n");
		}
		else if (WATER_SECT(IN_ROOM(ch))) {
			msg_to_char(ch, "You can't empty it in the water.\r\n");
		}
		else if (!can_use_vehicle(ch, found_veh, MEMBERS_ONLY)) {
			act("You don't own $V.", FALSE, ch, NULL, found_veh, TO_CHAR | ACT_VEH_VICT);
		}
		else if (!can_use_room(ch, IN_ROOM(ch), GUESTS_ALLOWED)) {
			msg_to_char(ch, "You don't have permission to empty it here.\r\n");
		}
		else if (!VEH_CONTAINS(found_veh)) {
			msg_to_char(ch, "There's nothing in it.\r\n");
		}
		else if (ROOM_BLD_FLAGGED(IN_ROOM(ch), BLD_ITEM_LIMIT) && size >= item_limit) {
			msg_to_char(ch, "The room is already full.\r\n");
		}
		else {
			// ok: empty vehicle
			messaged = FALSE;
			DL_FOREACH_SAFE2(VEH_CONTAINS(found_veh), obj, next_obj, next_content) {
				if (ROOM_BLD_FLAGGED(IN_ROOM(ch), BLD_ITEM_LIMIT) && size + obj_carry_size(obj) > item_limit) {
					continue;	// would be over the limit
				}
				if (!get_otrigger(obj, ch, TRUE)) {
					continue;
				}
				
				// prelim messaging on the first item
				if (!messaged) {
					act("You empty $V...", FALSE, ch, NULL, found_veh, TO_CHAR | TO_QUEUE | ACT_VEH_VICT);
					act("$n empties $V...", FALSE, ch, NULL, found_veh, TO_ROOM | TO_QUEUE | ACT_VEH_VICT);
					messaged = TRUE;
				}
				
				obj_to_room(obj, IN_ROOM(ch));
				size += obj_carry_size(obj);
				
				act("You unload $p onto the ground.", FALSE, ch, obj, NULL, TO_CHAR | TO_QUEUE);
				act("$n unloads $p onto the ground.", FALSE, ch, obj, NULL, TO_ROOM | TO_QUEUE);
			}
			
			if (!messaged) {
				act("There was nothing you could empty from $V.", FALSE, ch, NULL, found_veh, TO_CHAR | ACT_VEH_VICT);
			}
		}
	}
	else if (found_obj) {
		if (IS_DRINK_CONTAINER(found_obj)) {
			// pass through to "pour <item> out"
			snprintf(buf, sizeof(buf), " %s out", arg);
			do_pour(ch, buf, 0, 0);
		}
		else if (IS_CONTAINER(found_obj)) {
			if (IS_SET(GET_CONTAINER_FLAGS(found_obj), CONT_CLOSED)) {
				msg_to_char(ch, "It's closed.\r\n");
			}
			else if (!found_obj->contains) {
				msg_to_char(ch, "There's nothing inside it.\r\n");
			}
			else if (IN_ROOM(found_obj) && !can_use_room(ch, IN_ROOM(ch), GUESTS_ALLOWED)) {
				msg_to_char(ch, "You don't have permission to empty it here.\r\n");
			}
			else if (IN_ROOM(found_obj) && ROOM_BLD_FLAGGED(IN_ROOM(ch), BLD_ITEM_LIMIT) && size >= item_limit) {
				msg_to_char(ch, "The room is already full.\r\n");
			}
			else {
				// ok: empty container
				messaged = FALSE;
				DL_FOREACH_SAFE2(found_obj->contains, obj, next_obj, next_content) {
					if (IN_ROOM(found_obj) && ROOM_BLD_FLAGGED(IN_ROOM(ch), BLD_ITEM_LIMIT) && size + obj_carry_size(obj) > item_limit) {
						continue;	// would be over the limit
					}
					if (!get_otrigger(obj, ch, TRUE)) {
						continue;
					}
					
					// prelim messaging on the first item
					if (!messaged) {
						act("You empty $p...", FALSE, ch, found_obj, NULL, TO_CHAR | TO_QUEUE);
						act("$n empties $p...", FALSE, ch, found_obj, NULL, TO_ROOM | TO_QUEUE);
						messaged = TRUE;
					}
					
					if (IN_ROOM(found_obj)) {
						obj_to_room(obj, IN_ROOM(ch));
						size += obj_carry_size(obj);
						
						act("You dump $p onto the ground.", FALSE, ch, obj, NULL, TO_CHAR | TO_QUEUE);
						act("$n dumps $p onto the ground.", FALSE, ch, obj, NULL, TO_ROOM | TO_QUEUE);
					}
					else {
						// no need to check inventory size if it's going inventory-to-inventory
						obj_to_char(obj, ch);
						
						act("You dump $p out into your inventory.", FALSE, ch, obj, NULL, TO_CHAR | TO_QUEUE);
						act("$n dumps $p out into $s inventory.", FALSE, ch, obj, NULL, TO_ROOM | TO_QUEUE);
					}
				}
				
				if (!messaged) {
					act("There was nothing you could empty from $p.", FALSE, ch, found_obj, NULL, TO_CHAR);
				}
			}
		}
		else {
			act("You can't empty $p.", FALSE, ch, found_obj, NULL, TO_CHAR);
		}
	}
	else {
		msg_to_char(ch, "You don't see %s %s here.", AN(arg), arg);
	}
}


ACMD(do_equipment) {
	char *second;
	
	skip_spaces(&argument);
	second = one_argument(argument, arg);
	skip_spaces(&second);
	
	if (!*arg) {
		do_eq_show_current(ch, FALSE);
	}
	else if (!str_cmp(arg, "all") || !str_cmp(arg, "-all") || !str_cmp(arg, "-a")) {
		do_eq_show_current(ch, TRUE);
	}
	else if (IS_NPC(ch)) {
		msg_to_char(ch, "NPCs cannot have equipment sets.\r\n");
	}
	else if (!str_cmp(arg, "ana") || !str_cmp(arg, "-ana") || !str_cmp(arg, "analyze") || !str_cmp(arg, "-analyze")) {
		do_eq_analyze(ch, second);
	}
	else if (!str_cmp(arg, "del") || !str_cmp(arg, "-del") || !str_cmp(arg, "delete") || !str_cmp(arg, "-delete")) {
		do_eq_delete(ch, second);
	}
	else if (!str_cmp(arg, "list") || !str_cmp(arg, "-list")) {
		do_eq_list(ch, second);
	}
	else if (!str_cmp(arg, "set") || !str_cmp(arg, "-set") || !str_cmp(arg, "save") || !str_cmp(arg, "-save")) {
		do_eq_set(ch, second);
	}
	else {
		do_eq_change(ch, argument);	// full arg
	}
}


ACMD(do_exchange) {
	int dotmode, carrying, amount, new;
	empire_data *emp, *coin_emp;
	char buf[MAX_STRING_LENGTH];
	struct coin_data *coin;
	obj_data *obj, *next_obj;
	double rate;
	char *pos;
	
	skip_spaces(&argument);
	one_argument(argument, arg);
	
	if (IS_NPC(ch)) {
		msg_to_char(ch, "NPCs can't exchange anything.\r\n");
	}
	else if (!room_has_function_and_city_ok(GET_LOYALTY(ch), IN_ROOM(ch), FNC_MINT | FNC_VAULT)) {
		msg_to_char(ch, "You can't exchange treasure for coins here.\r\n");
	}
	else if (!IS_COMPLETE(IN_ROOM(ch))) {
		msg_to_char(ch, "You need to finish building first.\r\n");
	}
	else if (!(emp = ROOM_OWNER(IN_ROOM(ch)))) {
		msg_to_char(ch, "This building does not belong to any empire, and can't exchange coins.\r\n");
	}
	else if (!can_use_room(ch, IN_ROOM(ch), GUESTS_ALLOWED)) {
		msg_to_char(ch, "You don't have permission to exchange anything here.\r\n");
	}
	else if (!*arg) {
		msg_to_char(ch, "Exchange what for coins?\r\n");
	}
	else if ((pos = find_coin_arg(argument, &coin_emp, &amount, TRUE, TRUE, NULL)) > argument && amount > 0) {
		// exchanging coins
		if (!(coin = find_coin_entry(GET_PLAYER_COINS(ch), coin_emp))) {
			msg_to_char(ch, "You don't have any %s coins.\r\n", (coin_emp ? EMPIRE_ADJECTIVE(coin_emp) : "of those"));
		}
		else if (coin_emp == emp) {
			msg_to_char(ch, "You can't exchange coins for the same type of coins.\r\n");
		}
		else if (coin->amount < amount) {
			msg_to_char(ch, "You only have %s.\r\n", money_amount(coin_emp, coin->amount));
		}
		else if ((rate = exchange_rate(coin_emp, emp)) < 0.01) {
			msg_to_char(ch, "Those coins have no value here.\r\n");
		}
		else if (EMPIRE_COINS(emp) < (new = (int)(amount * rate))) {
			msg_to_char(ch, "This empire doesn't have enough coins to make that exchange.\r\n");
		}
		else if (new <= 0) {
			msg_to_char(ch, "You wouldn't get anything for that at these exchange rates.\r\n");
		}
		else if ((amount = (int)round(new / rate)) <= 0) {
			// prevent rounding errors e.g. 3 misc -> 1 empire coin
			msg_to_char(ch, "You wouldn't get anything for that at these exchange rates.\r\n");
		}
		else {
			// success!
			strcpy(buf, money_amount(coin_emp, amount));
			msg_to_char(ch, "You exchange %s for %s.\r\n", buf, money_amount(emp, new));
			act("$n exchanges some coins.", TRUE, ch, NULL, NULL, TO_ROOM);
			decrease_coins(ch, coin_emp, amount);
			increase_coins(ch, emp, new);
			
			// theoretically these had the same value so empire's coinage does not change
		}
	}
	else if (isdigit(*argument) && strstr(argument, "coins")) {
		// failed to detect as coins but the player is still (probably) trying to exchange coins
		msg_to_char(ch, "Usage: exchange <number> <type> coins\r\n");
	}
	else {
		// exchanging objs
		dotmode = find_all_dots(arg);
		
		if (dotmode == FIND_ALL) {
			carrying = IS_CARRYING_N(ch);
			
			DL_FOREACH_SAFE2(ch->carrying, obj, next_obj, next_content) {
				if (!OBJ_FLAGGED(obj, OBJ_KEEP)) {
					continue;
				}
				
				if (!perform_exchange(ch, obj, emp)) {
					break;
				}
			}
			
			if (carrying == IS_CARRYING_N(ch)) {
				msg_to_char(ch, "You don't have anything that could be exchanged.\r\n");
			}
		}
		else if (dotmode == FIND_ALLDOT) {
			if (!*arg) {
				msg_to_char(ch, "Exchange all of what?\r\n");
			}
			else if (!(obj = get_obj_in_list_vis(ch, arg, NULL, ch->carrying))) {
				msg_to_char(ch, "You don't seem to have any %ss to exchange.\r\n", arg);
			}
			else {
				carrying = IS_CARRYING_N(ch);
			
				while (obj) {
					next_obj = get_obj_in_list_vis(ch, arg, NULL, obj->next_content);
					if (!OBJ_FLAGGED(obj, OBJ_KEEP)) {
						if (!perform_exchange(ch, obj, emp)) {
							break;
						}
					}
					obj = next_obj;
				}

				if (carrying == IS_CARRYING_N(ch)) {
					msg_to_char(ch, "You don't have any %ss that could be exchanged.\r\n", arg);
				}
			}
		}
		else {
			if (!(obj = get_obj_in_list_vis(ch, arg, NULL, ch->carrying))) {
				msg_to_char(ch, "You don't seem to have %s %s.\r\n", AN(arg), arg);
			}
			else if (!IS_WEALTH_ITEM(obj) || GET_WEALTH_VALUE(obj) <= 0 || GET_OBJ_VNUM(obj) == NOTHING) {
				msg_to_char(ch, "You can't exchange that.\r\n");
			}
			else {
				perform_exchange(ch, obj, emp);
			}
		}
	}
}


ACMD(do_get) {
	char arg1[MAX_INPUT_LENGTH], arg2[MAX_INPUT_LENGTH], arg3[MAX_INPUT_LENGTH];
	char *argptr;
	int cont_dotmode, found = 0, iter, mode, number;
	vehicle_data *find_veh = NULL, *veh;
	obj_data *cont;
	char_data *tmp_char;

	argument = two_arguments(argument, arg1, arg2);
	one_argument(argument, arg3);

	if (IS_NPC(ch) && AFF_FLAGGED(ch, AFF_CHARM)) {
		msg_to_char(ch, "Charmed NPCs cannot pick items up.\r\n");
	}
	else if (!*arg1)
		send_to_char("Get what?\r\n", ch);
	else if (!*arg2)
		get_from_room(ch, arg1, 1);
	else if (is_number(arg1) && !*arg3)
		get_from_room(ch, arg2, atoi(arg1));
	else {
		int amount = 1;
		if (is_number(arg1)) {
			amount = atoi(arg1);
			strcpy(arg1, arg2);
			strcpy(arg2, arg3);
		}
		cont_dotmode = find_all_dots(arg2);
		if (cont_dotmode == FIND_INDIV) {
			argptr = arg2;
			number = get_number(&argptr);
			if ((cont = get_obj_for_char_prefer_container(ch, argptr, &number))) {
				// found preferred container
				mode = (cont->carried_by ? FIND_OBJ_INV : (cont->worn_by ? FIND_OBJ_EQUIP : FIND_OBJ_ROOM));
			}
			else {
				// try another way
				mode = generic_find(argptr, &number, FIND_OBJ_INV | FIND_OBJ_ROOM | FIND_OBJ_EQUIP | FIND_VEHICLE_ROOM | FIND_VEHICLE_INSIDE, ch, &tmp_char, &cont, &find_veh);
			}
			
			if (find_veh) {
				// pass off to vehicle handler
				do_get_from_vehicle(ch, find_veh, arg1, mode, amount);
			}
			else if (!cont) {
				sprintf(buf, "You don't have %s %s.\r\n", AN(argptr), argptr);
				send_to_char(buf, ch);
			}
			else if (GET_OBJ_TYPE(cont) != ITEM_CONTAINER && !IS_CORPSE(cont))
				act("$p is not a container.", FALSE, ch, cont, 0, TO_CHAR);
			else {
				get_from_container(ch, cont, arg1, mode, amount);
			}
		}
		else {
			if (cont_dotmode == FIND_ALLDOT && !*arg2) {
				send_to_char("Get from all of what?\r\n", ch);
				return;
			}
			DL_FOREACH2(ROOM_VEHICLES(IN_ROOM(ch)), veh, next_in_room) {
				if (CAN_SEE_VEHICLE(ch, veh) && VEH_FLAGGED(veh, VEH_CONTAINER) && (cont_dotmode == FIND_ALL || isname(arg2, VEH_KEYWORDS(veh)))) {
					found = 1;
					do_get_from_vehicle(ch, veh, arg1, FIND_OBJ_ROOM, amount);
				}
			}
			DL_FOREACH2(ch->carrying, cont, next_content) {
				if (CAN_SEE_OBJ(ch, cont) && (cont_dotmode == FIND_ALL || isname(arg2, GET_OBJ_KEYWORDS(cont)))) {
					if (GET_OBJ_TYPE(cont) == ITEM_CONTAINER || IS_CORPSE(cont)) {
						found = 1;
						get_from_container(ch, cont, arg1, FIND_OBJ_INV, amount);
					}
					else if (cont_dotmode == FIND_ALLDOT) {
						found = 1;
						act("$p is not a container.", FALSE, ch, cont, 0, TO_CHAR);
					}
				}
			}
			DL_FOREACH2(ROOM_CONTENTS(IN_ROOM(ch)), cont, next_content) {
				if (CAN_SEE_OBJ(ch, cont) && (cont_dotmode == FIND_ALL || isname(arg2, GET_OBJ_KEYWORDS(cont)))) {
					if (GET_OBJ_TYPE(cont) == ITEM_CONTAINER || IS_CORPSE(cont)) {
						get_from_container(ch, cont, arg1, FIND_OBJ_ROOM, amount);
						found = 1;
					}
					else if (cont_dotmode == FIND_ALLDOT) {
						act("$p is not a container.", FALSE, ch, cont, 0, TO_CHAR);
						found = 1;
					}
				}
			}
			for (iter = 0; iter < NUM_WEARS; ++iter) {
				if ((cont = GET_EQ(ch, iter)) && CAN_SEE_OBJ(ch, cont) && (cont_dotmode == FIND_ALL || isname(arg2, GET_OBJ_KEYWORDS(cont)))) {
					if (GET_OBJ_TYPE(cont) == ITEM_CONTAINER || IS_CORPSE(cont)) {
						get_from_container(ch, cont, arg1, FIND_OBJ_EQUIP, amount);
						found = 1;
					}
					else if (cont_dotmode == FIND_ALLDOT) {
						act("$p is not a container.", FALSE, ch, cont, NULL, TO_CHAR);
						found = 1;
					}
				}
			}
			if (!found) {
				if (cont_dotmode == FIND_ALL)
					send_to_char("You can't seem to find any containers.\r\n", ch);
				else {
					sprintf(buf, "You can't seem to find any %ss here.\r\n", arg2);
					send_to_char(buf, ch);
				}
			}
		}
	}
}


ACMD(do_give) {
	int amount, dotmode, coin_amt;
	char_data *vict;
	obj_data *obj, *next_obj;
	char *argpos;
	empire_data *coin_emp;
	bool any = FALSE;
	
	// give coins?
	if ((argpos = find_coin_arg(argument, &coin_emp, &coin_amt, TRUE, FALSE, NULL)) > argument && coin_amt > 0) {
		argument = one_argument(argpos, arg);
		if ((vict = give_find_vict(ch, arg))) {
			perform_give_coins(ch, vict, coin_emp, coin_amt);
		}
		return;
	}
	
	// otherwise continue with give object
	argument = one_argument(argument, arg);

	if (!*arg)
		send_to_char("Give what to whom?\r\n", ch);
	else if (is_number(arg)) {
		amount = atoi(arg);
		argument = one_argument(argument, arg);
		if (!*arg) {	/* Give multiple code. */
			sprintf(buf, "What do you want to give %d of?\r\n", amount);
			send_to_char(buf, ch);
		}
		else if (!(vict = give_find_vict(ch, argument))) {
			return;
		}
		else if (!(obj = get_obj_in_list_vis(ch, arg, NULL, ch->carrying))) {
			sprintf(buf, "You don't seem to have any %ss.\r\n", arg);
			send_to_char(buf, ch);
		}
		else {
			while (obj && amount > 0) {
				next_obj = get_obj_in_list_vis(ch, arg, NULL, obj->next_content);
				
				if (!OBJ_FLAGGED(obj, OBJ_KEEP)) {
					perform_give(ch, vict, obj);
					--amount;
					any = TRUE;
				}
				
				obj = next_obj;
			}
			
			if (!any) {
				msg_to_char(ch, "You don't seem to have any of those that aren't set 'keep'.\r\n");
			}
		}
	}
	else {
		one_argument(argument, buf1);
		if (!(vict = give_find_vict(ch, buf1)))
			return;
		
		dotmode = find_all_dots(arg);
		if (dotmode == FIND_INDIV) {
			if (!(obj = get_obj_in_list_vis(ch, arg, NULL, ch->carrying))) {
				sprintf(buf, "You don't seem to have %s %s.\r\n", AN(arg), arg);
				send_to_char(buf, ch);
			}
			else
				perform_give(ch, vict, obj);
		}
		else {
			if (dotmode == FIND_ALLDOT && !*arg) {
				send_to_char("All of what?\r\n", ch);
				return;
			}
			if (!ch->carrying)
				send_to_char("You don't seem to be holding anything.\r\n", ch);
			else {
				DL_FOREACH_SAFE2(ch->carrying, obj, next_obj, next_content) {
					if (CAN_SEE_OBJ(ch, obj) && !OBJ_FLAGGED(obj, OBJ_KEEP) && ((dotmode == FIND_ALL || isname(arg, GET_OBJ_KEYWORDS(obj))))) {
						perform_give(ch, vict, obj);
						any = TRUE;
					}
				}
				
				if (!any) {
					msg_to_char(ch, "You don't seem to have any of those that aren't set 'keep'.\r\n");
				}
			}
		}
	}
}


// do_hold <-- search alias
ACMD(do_grab) {
	obj_data *obj;

	one_argument(argument, arg);

	if (IS_NPC(ch)) {
		msg_to_char(ch, "NPCs may not hold items.\r\n");
	}
	else if (!*arg)
		send_to_char("Hold what?\r\n", ch);
	else if (!(obj = get_obj_in_list_vis(ch, arg, NULL, ch->carrying))) {
		if (GET_EQ(ch, WEAR_HOLD) && MATCH_ITEM_NAME(arg, GET_EQ(ch, WEAR_HOLD))) {
			msg_to_char(ch, "It looks like you're already holding it!\r\n");
		}
		else if (GET_EQ(ch, WEAR_RANGED) && MATCH_ITEM_NAME(arg, GET_EQ(ch, WEAR_RANGED))) {
			msg_to_char(ch, "It looks like you're already holding it!\r\n");
		}
		else {
			msg_to_char(ch, "You don't seem to have %s %s.\r\n", AN(arg), arg);
		}
	}
	else if (!can_wear_item(ch, obj, TRUE)) {
		// sends own messages
	}
	else if (CAN_WEAR(obj, ITEM_WEAR_RANGED)) {
		if (GET_EQ(ch, WEAR_RANGED)) {
			perform_remove(ch, WEAR_RANGED);
		}
		perform_wear(ch, obj, WEAR_RANGED);
	}
	else if (CAN_WEAR(obj, ITEM_WEAR_HOLD)) {
		if (GET_EQ(ch, WEAR_WIELD) && OBJ_FLAGGED(GET_EQ(ch, WEAR_WIELD), OBJ_TWO_HANDED)) {
			msg_to_char(ch, "You can't hold an item while wielding a two-handed weapon.\r\n");
		}
		else {
			// remove existing item
			if (GET_EQ(ch, WEAR_HOLD)) {
				perform_remove(ch, WEAR_HOLD);
			}
			perform_wear(ch, obj, WEAR_HOLD);
			
			if (FIGHTING(ch)) {
				command_lag(ch, WAIT_COMBAT_ABILITY);
			}
		}
	}
	else {
		send_to_char("You can't hold that.\r\n", ch);
	}
}


ACMD(do_identify) {
	obj_data *obj, *next_obj, *list[2];
	bool any, extract = FALSE, inv_only = FALSE;
	char_data *tmp_char;
	vehicle_data *veh;
	int dotmode, iter;
	
	argument = one_argument(argument, arg);
	
	// if first arg is 'inv', restrict to inventory
	if (strlen(arg) >= 3 && is_abbrev(arg, "inventory")) {
		inv_only = TRUE;
		one_argument(argument, arg);
	}
	
	if (GET_POS(ch) == POS_FIGHTING) {
		msg_to_char(ch, "You're too busy to do that now!\r\n");
		return;
	}
	if (!*arg) {
		msg_to_char(ch, "Identify what object%s?\r\n", inv_only ? " in your inventory" : "");
		return;
	}
	
	dotmode = find_all_dots(arg);
	
	if (dotmode == FIND_ALL) {
		any = FALSE;
		
		// inv
		DL_FOREACH_SAFE2(ch->carrying, obj, next_obj, next_content) {
			if (run_identifies_to(ch, &obj, &extract)) {
				any = TRUE;
			}
			if (extract) {	// usually it can do this itself, but just in case
				extract_obj(obj);
			}
		}
		
		// room
		if (!inv_only && can_use_room(ch, IN_ROOM(ch), MEMBERS_ONLY)) {
			DL_FOREACH_SAFE2(ROOM_CONTENTS(IN_ROOM(ch)), obj, next_obj, next_content) {
				if (run_identifies_to(ch, &obj, &extract)) {
					any = TRUE;
				}
				if (extract) {	// usually it can do this itself, but just in case
					extract_obj(obj);
				}
			}
		}
		
		if (any) {
			act("$n identifies some items.", TRUE, ch, NULL, NULL, TO_ROOM);
			command_lag(ch, WAIT_OTHER);
		}
		else {
			msg_to_char(ch, "You don't have anything special to identify%s.\r\n", inv_only ? " in your inventory" : "");
		}
	}	// /all
	else if (dotmode == FIND_ALLDOT) {
		if (!*arg) {
			msg_to_char(ch, "Identify all of what%s?\r\n", inv_only ? " in your inventory" : "");
			return;
		}
		
		any = FALSE;
		list[0] = ch->carrying;
		list[1] = (!inv_only && can_use_room(ch, IN_ROOM(ch), MEMBERS_ONLY)) ? ROOM_CONTENTS(IN_ROOM(ch)) : NULL;
		
		for (iter = 0; iter < 2; ++iter) {
			obj = get_obj_in_list_vis(ch, arg, NULL, list[iter]);
			while (obj) {
				next_obj = get_obj_in_list_vis(ch, arg, NULL, obj->next_content);
				
				if (run_identifies_to(ch, &obj, &extract)) {
					any = TRUE;
				}
				if (extract) {	// usually it can do this itself, but just in case
					extract_obj(obj);
				}
				
				obj = next_obj;
			}
		}
		
		if (any) {
			act("$n identifies some items.", TRUE, ch, NULL, NULL, TO_ROOM);
			command_lag(ch, WAIT_OTHER);
		}
		else {
			msg_to_char(ch, "You don't seem to have any %ss to identify%s.\r\n", arg, inv_only ? " in your inventory" : "");
		}
	}	// /all.
	else {	// specific obj/vehicle
		if (!generic_find(arg, NULL, inv_only ? FIND_OBJ_INV : (FIND_OBJ_INV | FIND_OBJ_ROOM | FIND_OBJ_EQUIP | FIND_VEHICLE_ROOM | FIND_VEHICLE_INSIDE), ch, &tmp_char, &obj, &veh)) {
			msg_to_char(ch, "You see nothing like that %s.\r\n", inv_only ? "in your inventory" : "here");
		}
		else if (obj) {
			// message first in case the item changes
			act("$n identifies $p.", TRUE, ch, obj, NULL, TO_ROOM);
			
			// check if it has identifies-to
			if (WORN_OR_CARRIED_BY(obj, ch) || can_use_room(ch, IN_ROOM(ch), MEMBERS_ONLY)) {
				run_identifies_to(ch, &obj, &extract);
				if (ch->desc) {
					// flush the stacked id message before id'ing it
					send_stacked_msgs(ch->desc);
				}
			}
		
			if (!IS_IMMORTAL(ch) && GET_OBJ_CURRENT_SCALE_LEVEL(obj) == 0) {
				// for non-immortals, ensure scaling is done
				scale_item_to_level(obj, get_approximate_level(ch));
			}
		
			charge_ability_cost(ch, NOTHING, 0, NOTHING, 0, WAIT_OTHER);
			identify_obj_to_char(obj, ch, FALSE);
		
			if (extract) {
				// ONLY if we need to extract it but didn't earlier
				extract_obj(obj);
			}
		}
		else if (veh) {
			charge_ability_cost(ch, NOTHING, 0, NOTHING, 0, WAIT_OTHER);
			act("$n identifies $V.", TRUE, ch, NULL, veh, TO_ROOM | ACT_VEH_VICT);
			identify_vehicle_to_char(veh, ch);
		}
	}
}


ACMD(do_keep) {
	char arg[MAX_INPUT_LENGTH];
	obj_data *obj, *next_obj;
	int dotmode, mode = SCMD_KEEP;
	const char *sname;
	bool any;

	// validate mode
	switch (subcmd) {
		case SCMD_KEEP: {
			sname = "keep";
			mode = SCMD_KEEP;
			break;
		}
		case SCMD_UNKEEP:
		default: {
			sname = "unkeep";
			mode = SCMD_UNKEEP;
			break;
		}
	}

	argument = one_argument(argument, arg);

	if (!*arg) {
		sprintf(buf, "What do you want to %s?\r\n", sname);
		send_to_char(buf, ch);
		return;
	}
	
	dotmode = find_all_dots(arg);

	if (dotmode == FIND_ALL) {
		if (!ch->carrying) {
			send_to_char("You don't seem to be carrying anything.\r\n", ch);
		}
		else {
			DL_FOREACH_SAFE2(ch->carrying, obj, next_obj, next_content) {
				if (mode == SCMD_KEEP) {
					if (GET_OBJ_REQUIRES_QUEST(obj) != NOTHING) {
						continue;	// skip quest items
					}
					
					SET_BIT(GET_OBJ_EXTRA(obj), OBJ_KEEP);
					qt_keep_obj(ch, obj, TRUE);
				}
				else {
					REMOVE_BIT(GET_OBJ_EXTRA(obj), OBJ_KEEP);
					qt_keep_obj(ch, obj, FALSE);
				}
			}
			
			msg_to_char(ch, "You %s all items in your inventory.\r\n", sname);
		}
	}
	else if (dotmode == FIND_ALLDOT) {
		if (!*arg) {
			msg_to_char(ch, "What do you want to %s all of?\r\n", sname);
			return;
		}
		if (!(obj = get_obj_in_list_vis(ch, arg, NULL, ch->carrying))) {
			msg_to_char(ch, "You don't seem to have any %ss.\r\n", arg);
			return;
		}
		any = FALSE;
		while (obj) {
			next_obj = get_obj_in_list_vis(ch, arg, NULL, obj->next_content);
			if (mode == SCMD_KEEP) {
				if (GET_OBJ_REQUIRES_QUEST(obj) != NOTHING) {
					obj = next_obj;
					continue;	// skip quest items
				}
				SET_BIT(GET_OBJ_EXTRA(obj), OBJ_KEEP);
				qt_keep_obj(ch, obj, TRUE);
			}
			else {
				REMOVE_BIT(GET_OBJ_EXTRA(obj), OBJ_KEEP);
				qt_keep_obj(ch, obj, FALSE);
			}
			sprintf(buf, "You %s $p.", sname);
			act(buf, FALSE, ch, obj, NULL, TO_CHAR | TO_QUEUE | TO_SLEEP);
			obj = next_obj;
			any = TRUE;
		}
		
		if (!any) {
			msg_to_char(ch, "You didn't have any you could keep.\r\n");
			return;
		}
	}
	else {
		if (!(obj = get_obj_in_list_vis(ch, arg, NULL, ch->carrying))) {
			msg_to_char(ch, "You don't seem to have %s %s.\r\n", AN(arg), arg);
		}
		else if (mode == SCMD_KEEP && GET_OBJ_REQUIRES_QUEST(obj) != NOTHING) {
			msg_to_char(ch, "You cannot use 'keep' on quest items (but you also can't drop them).\r\n");
		}
		else {
			if (mode == SCMD_KEEP) {
				SET_BIT(GET_OBJ_EXTRA(obj), OBJ_KEEP);
				qt_keep_obj(ch, obj, TRUE);
			}
			else {
				REMOVE_BIT(GET_OBJ_EXTRA(obj), OBJ_KEEP);
				qt_keep_obj(ch, obj, FALSE);
			}
			sprintf(buf, "You %s $p.", sname);
			act(buf, FALSE, ch, obj, NULL, TO_CHAR | TO_SLEEP);
		}
	}
	
	queue_delayed_update(ch, CDU_SAVE);
}


// prior to b5.152, this was also do_burn
ACMD(do_light) {
	bool objless = has_player_tech(ch, PTECH_LIGHT_FIRE);
	char *argptr = arg;
	obj_data *obj, *lighter = NULL;
	vehicle_data *veh;
	bool kept = FALSE;
	int number, dir;

	one_argument(argument, arg);
	number = get_number(&argptr);

	if (!objless) {
		lighter = find_lighter_in_list(ch->carrying, &kept);
	}

	if (!*argptr) {
		msg_to_char(ch, "Light what?\r\n");
	}
	else if (!IS_NPC(ch) && !objless && !lighter) {
		// nothing to light it with
		if (kept) {
			msg_to_char(ch, "You need a lighter that isn't marked 'keep'.\r\n");
		}
		else {
			msg_to_char(ch, "You don't have anything to light that with.\r\n");
		}
	}
	else if (!(obj = get_obj_in_list_vis_prefer_lightable(ch, argptr, &number, ch->carrying)) && !(obj = get_obj_in_list_vis_prefer_lightable(ch, argptr, &number, ROOM_CONTENTS(IN_ROOM(ch))))) {
		// invalid arg... trying to light one of these?
		if (generic_find(argptr, &number, FIND_VEHICLE_ROOM | FIND_VEHICLE_INSIDE, ch, NULL, NULL, &veh) || (dir = parse_direction(ch, argptr)) != NO_DIR || is_abbrev(arg, "building") || is_abbrev(arg, "area") || is_abbrev(arg, "room") || is_abbrev(arg, "here") || isname(arg, get_room_name(IN_ROOM(ch), FALSE)) || isname(arg, GET_SECT_NAME(SECT(IN_ROOM(ch))))) {
			msg_to_char(ch, "You can't light vehicles, buildings, or terrain with this command. Try 'burn' instead.\r\n");
		}
		else {
			msg_to_char(ch, "You don't have %s %s.\r\n", AN(arg), arg);
		}
	}
	else if (LIGHT_IS_LIT(obj) && !has_interaction(GET_OBJ_INTERACTIONS(obj), INTERACT_LIGHT)) {
		msg_to_char(ch, "It's already lit!\r\n");
	}
	else if (!CAN_LIGHT_OBJ(obj)) {
		act("You can't light $p!", FALSE, ch, obj, NULL, TO_CHAR);
	}
	else if (lighter == obj) {
		msg_to_char(ch, "You can't use an item to light itself.\r\n");
	}
	else {
		if (objless) {
			act("You light $p!", FALSE, ch, obj, NULL, TO_CHAR);
			act("$n lights $p!", FALSE, ch, obj, NULL, TO_ROOM);
			gain_player_tech_exp(ch, PTECH_LIGHT_FIRE, 15);
			run_ability_hooks_by_player_tech(ch, PTECH_LIGHT_FIRE, NULL, obj, NULL, NULL);
		}
		else if (lighter) {
			// obj message to char
			if (obj_has_custom_message(lighter, OBJ_CUSTOM_CONSUME_TO_CHAR)) {
				act(obj_get_custom_message(lighter, OBJ_CUSTOM_CONSUME_TO_CHAR), FALSE, ch, lighter, obj, TO_CHAR | ACT_OBJ_VICT);
			}
			else {
				act("You use $p to light $P.", FALSE, ch, lighter, obj, TO_CHAR | ACT_OBJ_VICT);
			}
			
			// obj message to room
			if (obj_has_custom_message(lighter, OBJ_CUSTOM_CONSUME_TO_ROOM)) {
				act(obj_get_custom_message(lighter, OBJ_CUSTOM_CONSUME_TO_ROOM), TRUE, ch, lighter, obj, TO_ROOM | ACT_OBJ_VICT);
			}
			else {
				act("$n uses $p to light $P.", FALSE, ch, lighter, obj, TO_ROOM | ACT_OBJ_VICT);
			}
		}
		else { // somehow?
			act("You light $P.", FALSE, ch, NULL, obj, TO_CHAR | ACT_OBJ_VICT);
			act("$n lights $P.", FALSE, ch, NULL, obj, TO_ROOM | ACT_OBJ_VICT);
		}
		
		if (!has_interaction(GET_OBJ_INTERACTIONS(obj), INTERACT_LIGHT) && IS_LIGHT(obj) && !GET_LIGHT_IS_LIT(obj) && GET_LIGHT_HOURS_REMAINING(obj) != 0) {
			// lighting a light
			set_obj_val(obj, VAL_LIGHT_IS_LIT, 1);
			apply_obj_light(obj, TRUE);
			schedule_obj_timer_update(obj, FALSE);
			
			// set no-store only if we kept the same object
			SET_BIT(GET_OBJ_EXTRA(obj), OBJ_NO_BASIC_STORAGE);
			request_obj_save_in_world(obj);
		}
		else {
			// running interactions: will extract no matter what happens here
			empty_obj_before_extract(obj);
			run_interactions(ch, GET_OBJ_INTERACTIONS(obj), INTERACT_LIGHT, IN_ROOM(ch), NULL, obj, NULL, light_obj_interact);
			extract_obj(obj);
		}
		
		command_lag(ch, WAIT_OTHER);
		
		if (lighter) {
			used_lighter(ch, lighter);
		}
	}
}


ACMD(do_list) {
	char buf[MAX_STRING_LENGTH * 2], line[MAX_STRING_LENGTH], rep[256], tmp[256], matching[MAX_INPUT_LENGTH], vstr[128], drinkstr[128], *ptr;
	struct shop_temp_list *stl, *shop_list = NULL;
	struct shop_item *item;
	bool any, any_cur, this;
	obj_data *obj;
	any_vnum vnum;
	size_t size;
	bool ok, id, all, found_id = FALSE;
	int amt, number;
	
	// helper type for displaying currencies at the end
	struct cur_t {
		any_vnum vnum;
		UT_hash_handle hh;
	} *curt, *next_curt, *curt_hash = NULL;
	
	// check for "list id <obj> syntax"
	ptr = one_argument(argument, line);
	if (!strn_cmp(line, "id", 2) && is_abbrev(line, "identify")) {
		id = TRUE;
		argument = ptr;
		
		if (!*argument) {	// filter arg required
			msg_to_char(ch, "Usage: list identify <item>\r\n");
			return;
		}
	}
	else {
		id = FALSE;
	}
	
	skip_spaces(&argument);	// optional filter (remaining args)
	if (isdigit(*argument)) {
		number = get_number(&argument);	// x.name syntax
	}
	else {
		number = -1;
	}
	all = (number <= 0);
	
	// find shops
	shop_list = build_available_shop_list(ch);
	any = FALSE;
	
	if (*argument) {
		snprintf(matching, sizeof(matching), " items matching '%s'", argument);
	}
	else {
		*matching = '\0';
	}
	
	size = 0;
	*buf = '\0';

	// now show any shops available
	LL_FOREACH(shop_list, stl) {
		this = FALSE;
		
		// the nopes
		if (SHOP_FLAGGED(stl->shop, SHOP_IN_DEVELOPMENT) && !IS_IMMORTAL(ch)) {
			continue;	// in-dev
		}
		
		// list of items
		LL_FOREACH(SHOP_ITEMS(stl->shop), item) {
			if (!(obj = obj_proto(item->vnum))) {
				continue;	// no obj
			}
			if (*argument) {	// search option
				ok = multi_isname(argument, GET_OBJ_KEYWORDS(obj));
				if (!ok) {
					ok = (item->currency == NOTHING && !str_cmp(argument, "coins"));
				}
				if (!ok) {
					ok = multi_isname(argument, get_generic_string_by_vnum(item->currency, GENERIC_CURRENCY, WHICH_CURRENCY(item->cost)));
				}
				
				if (!ok) {
					continue;	// search option
				}
			}
			if (!all && --number != 0) {
				continue;
			}
			
			if (id) {	// just identifying -- show shop id then exit the loop early
				do_shop_identify(ch, obj);
				found_id = TRUE;
				break;
			}
			
			if (!this) {
				this = TRUE;
				
				if (SHOP_ALLEGIANCE(stl->shop)) {
					snprintf(rep, sizeof(rep), " (%s)", FCT_NAME(SHOP_ALLEGIANCE(stl->shop)));
				}
				else {
					*rep = '\0';
				}
				
				if (PRF_FLAGGED(ch, PRF_ROOMFLAGS)) {
					sprintf(vstr, "[%5d] ", SHOP_VNUM(stl->shop));
				}
				else {
					*vstr = '\0';
				}
				
				if (stl->from_mob) {
					strcpy(tmp, PERS(stl->from_mob, ch, FALSE));
					snprintf(line, sizeof(line), "%s%s%s%s sells%s:\r\n", (*buf ? "\r\n" : ""), vstr, CAP(tmp), rep, matching);
				}
				else if (stl->from_obj) {
					strcpy(tmp, GET_OBJ_SHORT_DESC(stl->from_obj));
					snprintf(line, sizeof(line), "%s%s%s%s sells%s:\r\n", (*buf ? "\r\n" : ""), vstr, CAP(tmp), rep, matching);
				}
				else if (stl->from_veh) {
					strcpy(tmp, get_vehicle_short_desc(stl->from_veh, ch));
					snprintf(line, sizeof(line), "%s%s%s%s sells%s:\r\n", (*buf ? "\r\n" : ""), vstr, CAP(tmp), rep, matching);
				}
				else {
					snprintf(line, sizeof(line), "%s%sYou can %sbuy%s%s:\r\n", (*buf ? "\r\n" : ""), vstr, (*buf ? "also " : ""), rep, matching);
				}
				
				if (size + strlen(line) < sizeof(buf)) {
					strcat(buf, line);
					size += strlen(line);
					any = TRUE;
				}
				else {
					break;
				}
			}
			
			if (SHOP_ALLEGIANCE(stl->shop) && item->min_rep != REP_NONE) {
				snprintf(rep, sizeof(rep), ", %s reputation", reputation_levels[rep_const_to_index(item->min_rep)].name);
			}
			else {
				*rep = '\0';
			}
			
			if (PRF_FLAGGED(ch, PRF_ROOMFLAGS)) {
				sprintf(vstr, "[%5d] ", GET_OBJ_VNUM(obj));
			}
			else {
				*vstr = '\0';
			}
			
			if (IS_DRINK_CONTAINER(obj) && GET_DRINK_CONTAINER_CONTENTS(obj) > 0) {
				snprintf(drinkstr, sizeof(drinkstr), " (of %s)", get_generic_string_by_vnum(GET_DRINK_CONTAINER_TYPE(obj), GENERIC_LIQUID, GSTR_LIQUID_NAME));
			}
			else {
				*drinkstr = '\0';
			}
			
			snprintf(line, sizeof(line), " - %s%s%s (%d %s%s)\r\n", vstr, GET_OBJ_SHORT_DESC(obj), drinkstr, item->cost, (item->currency == NOTHING ? "coins" : get_generic_string_by_vnum(item->currency, GENERIC_CURRENCY, WHICH_CURRENCY(item->cost))), rep);
			
			// store currency for listing later
			if ((vnum = item->currency) != NOTHING) {
				HASH_FIND_INT(curt_hash, &vnum, curt);
				if (!curt) {
					CREATE(curt, struct cur_t, 1);
					curt->vnum = vnum;
					HASH_ADD_INT(curt_hash, vnum, curt);
				}
			}
			
			if (size + strlen(line) < sizeof(buf)) {
				strcat(buf, line);
				size += strlen(line);
				any = TRUE;
			}
			else {
				break;
			}
		}
	}
	
	if (!id) {	// normal view
		// append currencies if any
		if (curt_hash && size < sizeof(buf)) {
			size += snprintf(buf + size, sizeof(buf) - size, "You have:");
			any_cur = FALSE;
			HASH_ITER(hh, curt_hash, curt, next_curt) {
				amt = get_currency(ch, curt->vnum);
				snprintf(line, sizeof(line), "%s%d %s", any_cur ? ", " : " ", amt, get_generic_string_by_vnum(curt->vnum, GENERIC_CURRENCY, WHICH_CURRENCY(amt)));
			
				if (size + strlen(line) < sizeof(buf)) {
					strcat(buf, line);
					size += strlen(line);
					any_cur = TRUE;
				}
			}
			if (size + 2 < sizeof(buf)) {
				strcat(buf, "\r\n");
				size += 2;
			}
		}

		if (any) {
			if (ch->desc) {
				page_string(ch->desc, buf, TRUE);
			}
		}
		else {
			msg_to_char(ch, "There's nothing for sale here%s.\r\n", (*argument ? " by that name" : ""));
		}
	}
	else if (id && !found_id) {
		msg_to_char(ch, "You don't see anything like that here.\r\n");
	}

	free_shop_temp_list(shop_list);
	
	// clean up currency list
	HASH_ITER(hh, curt_hash, curt, next_curt) {
		HASH_DEL(curt_hash, curt);
		free(curt);
	}
}


// this is also 'do_fill'
ACMD(do_pour) {	
	char arg1[MAX_INPUT_LENGTH], arg2[MAX_INPUT_LENGTH];
	obj_data *from_obj = NULL, *to_obj = NULL;
	int amount;
	vehicle_data *veh;

	two_arguments(argument, arg1, arg2);
	
	if (GET_POS(ch) == POS_FIGHTING) {
		msg_to_char(ch, "You're too busy fighting!\r\n");
		return;
	}
	
	if (subcmd == SCMD_POUR) {
		if (!*arg1) {		/* No arguments */
			send_to_char("From what do you want to pour?\r\n", ch);
			return;
		}
		if (!(from_obj = get_obj_in_list_vis(ch, arg1, NULL, ch->carrying))) {
			send_to_char("You can't find it!\r\n", ch);
			return;
		}
		if (!IS_DRINK_CONTAINER(from_obj)) {
			send_to_char("You can't pour from that!\r\n", ch);
			return;
		}
	}
	if (subcmd == SCMD_FILL) {
		if (!*arg1) {		/* no arguments */
			send_to_char("What do you want to fill? And from what are you filling it?\r\n", ch);
			return;
		}
		if (!(to_obj = get_obj_in_list_vis(ch, arg1, NULL, ch->carrying))) {
			send_to_char("You can't find it!\r\n", ch);
			return;
		}
		if (!IS_DRINK_CONTAINER(to_obj)) {
			act("You can't fill $p!", FALSE, ch, to_obj, 0, TO_CHAR);
			return;
		}
		if (!*arg2) {	// no 2nd argument: fill from room
			if (ROOM_SECT_FLAGGED(IN_ROOM(ch), SECTF_DRINK) || find_flagged_sect_within_distance_from_char(ch, SECTF_DRINK, NOBITS, 1) || (room_has_function_and_city_ok(GET_LOYALTY(ch), IN_ROOM(ch), FNC_DRINK_WATER) && IS_COMPLETE(IN_ROOM(ch)))) {
				fill_from_room(ch, to_obj);
				return;
			}
			else if (IS_IMMORTAL(ch)) {
				// immortals get a free fill (with water) anywhere
				// empty before message
				set_obj_val(to_obj, VAL_DRINK_CONTAINER_CONTENTS, 0);
				
				// message before filling
				act("You fill $p with water from thin air!", FALSE, ch, to_obj, NULL, TO_CHAR);
				act("$n fills $p with water from thin air!", TRUE, ch, to_obj, NULL, TO_ROOM);
				
				set_obj_val(to_obj, VAL_DRINK_CONTAINER_TYPE, LIQ_WATER);
				set_obj_val(to_obj, VAL_DRINK_CONTAINER_CONTENTS, GET_DRINK_CONTAINER_CAPACITY(to_obj));

				return;
			}
			else {
				act("From what would you like to fill $p?", FALSE, ch, to_obj, NULL, TO_CHAR);
				return;
			}
		}
		if (!(from_obj = get_obj_in_list_vis(ch, arg2, NULL, ROOM_CONTENTS(IN_ROOM(ch))))) {
			// no matching obj
			if ((is_abbrev(arg2, "water") || isname(arg2, get_room_name(IN_ROOM(ch), FALSE))) && room_has_function_and_city_ok(GET_LOYALTY(ch), IN_ROOM(ch), FNC_DRINK_WATER)) {
				fill_from_room(ch, to_obj);
			}
			else if ((veh = get_vehicle_in_room_vis(ch, arg2, NULL)) && vehicle_has_function_and_city_ok(veh, FNC_DRINK_WATER)) {
				// targeting a vehicle with a drink-water flag
				if (!VEH_IS_COMPLETE(veh)) {
					act("You need to complete $V before filling from it.", FALSE, ch, NULL, veh, TO_CHAR | ACT_VEH_VICT);
					return;
				}
				fill_from_room(ch, to_obj);
			}
			else if (is_abbrev(arg2, "river") || is_abbrev(arg2, "water")) {
				if (ROOM_SECT_FLAGGED(IN_ROOM(ch), SECTF_DRINK)) {
					fill_from_room(ch, to_obj);
				}
				else if (find_flagged_sect_within_distance_from_char(ch, SECTF_DRINK, NOBITS, 1)) {
					fill_from_room(ch, to_obj);
				}
				else {
					msg_to_char(ch, "There doesn't seem to be %s %s here.\r\n", AN(arg2), arg2);
				}
			}
			else {
				msg_to_char(ch, "There doesn't seem to be %s %s here.\r\n", AN(arg2), arg2);
			}
			return;
		}
		if (!IS_DRINK_CONTAINER(from_obj) || CAN_WEAR(from_obj, ITEM_WEAR_TAKE)) {
			act("You can't fill anything from $p.", FALSE, ch, from_obj, NULL, TO_CHAR);
			return;
		}
	}
	if (GET_DRINK_CONTAINER_CONTENTS(from_obj) == 0) {
		act("$p is empty.", FALSE, ch, from_obj, 0, TO_CHAR);
		return;
	}
	if (subcmd == SCMD_POUR) {	/* pour */
		if (!*arg2) {
			send_to_char("Where do you want it?  Out or in what?\r\n", ch);
			return;
		}
		if (!str_cmp(arg2, "out")) {
			if (!CAN_WEAR(from_obj, ITEM_WEAR_TAKE)) {
				msg_to_char(ch, "You can't pour that out.\r\n");
				return;
			}
			
			// shortcut through do_douse_room if it's water and burning
			if (IS_ANY_BUILDING(IN_ROOM(ch)) && IS_BURNING(HOME_ROOM(IN_ROOM(ch))) && liquid_flagged(GET_DRINK_CONTAINER_TYPE(from_obj), LIQF_WATER)) {
				do_douse_room(ch, IN_ROOM(ch), from_obj);
				return;
			}
			
			act("$n empties $p.", TRUE, ch, from_obj, 0, TO_ROOM);
			act("You empty $p.", FALSE, ch, from_obj, 0, TO_CHAR);

			/* If it's a trench, fill her up */
			if (liquid_flagged(GET_DRINK_CONTAINER_TYPE(from_obj), LIQF_WATER) && ROOM_SECT_FLAGGED(IN_ROOM(ch), SECTF_IS_TRENCH) && get_room_extra_data(IN_ROOM(ch), ROOM_EXTRA_TRENCH_PROGRESS) >= 0) {
				add_to_room_extra_data(IN_ROOM(ch), ROOM_EXTRA_TRENCH_FILL_TIME, -25 * GET_DRINK_CONTAINER_CONTENTS(from_obj));
				if (find_stored_event(SHARED_DATA(IN_ROOM(ch))->events, SEV_TRENCH_FILL)) {
					cancel_stored_event(&SHARED_DATA(IN_ROOM(ch))->events, SEV_TRENCH_FILL);
				}
				if (GET_MAP_LOC(IN_ROOM(ch))) {	// can this be null?
					schedule_trench_fill(GET_MAP_LOC(IN_ROOM(ch)));
				}
			}

			set_obj_val(from_obj, VAL_DRINK_CONTAINER_CONTENTS, 0);
			set_obj_val(from_obj, VAL_DRINK_CONTAINER_TYPE, 0);
			
			// check single-use and timer
			if (OBJ_FLAGGED(from_obj, OBJ_SINGLE_USE)) {
				// single-use: extract it
				run_interactions(ch, GET_OBJ_INTERACTIONS(from_obj), INTERACT_CONSUMES_TO, IN_ROOM(ch), NULL, from_obj, NULL, consumes_or_decays_interact);
				empty_obj_before_extract(from_obj);
				extract_obj(from_obj);
			}
			else {
				// if it wasn't single-use, reset its timer to UNLIMITED since the timer refers to the contents
				GET_OBJ_TIMER(from_obj) = UNLIMITED;
			}
			return;
		}
		if (!(to_obj = get_obj_in_list_vis(ch, arg2, NULL, ch->carrying))) {
			send_to_char("You can't find it!\r\n", ch);
			return;
		}
		if (!IS_DRINK_CONTAINER(to_obj)) {
			send_to_char("You can't pour anything into that.\r\n", ch);
			return;
		}
		if (GET_DRINK_CONTAINER_CONTENTS(to_obj) >= GET_DRINK_CONTAINER_CAPACITY(to_obj)) {
			act("$p is already full.", FALSE, ch, to_obj, NULL, TO_CHAR);
			return;
		}
	}
	if (to_obj == from_obj) {
		send_to_char("A most unproductive effort.\r\n", ch);
		return;
	}
	if (OBJ_FLAGGED(to_obj, OBJ_SINGLE_USE)) {
		act("You can't put anything in $p.", FALSE, ch, to_obj, NULL, TO_CHAR);
		return;
	}
	if ((GET_DRINK_CONTAINER_CONTENTS(to_obj) != 0) && (GET_DRINK_CONTAINER_TYPE(to_obj) != GET_DRINK_CONTAINER_TYPE(from_obj))) {
		send_to_char("There is already another liquid in it. Pour it out first.\r\n", ch);
		return;
	}
	if (GET_DRINK_CONTAINER_CONTENTS(to_obj) > GET_DRINK_CONTAINER_CAPACITY(to_obj)) {
		send_to_char("There is no room for more.\r\n", ch);
		return;
	}
	if (subcmd == SCMD_POUR) {
		msg_to_char(ch, "You pour the %s into %s.\r\n", get_generic_string_by_vnum(GET_DRINK_CONTAINER_TYPE(from_obj), GENERIC_LIQUID, GSTR_LIQUID_NAME), GET_OBJ_SHORT_DESC(to_obj));
	}
	if (subcmd == SCMD_FILL) {
		act("You gently fill $p from $P.", FALSE, ch, to_obj, from_obj, TO_CHAR | ACT_OBJ_VICT);
		act("$n gently fills $p from $P.", TRUE, ch, to_obj, from_obj, TO_ROOM | ACT_OBJ_VICT);
	}

	/* First same type liq. */
	set_obj_val(to_obj, VAL_DRINK_CONTAINER_TYPE, GET_DRINK_CONTAINER_TYPE(from_obj));

	// Then how much to pour -- this CAN make the original container go negative
	amount = GET_DRINK_CONTAINER_CAPACITY(to_obj) - GET_DRINK_CONTAINER_CONTENTS(to_obj);
	set_obj_val(from_obj, VAL_DRINK_CONTAINER_CONTENTS, GET_DRINK_CONTAINER_CONTENTS(from_obj) - amount);

	set_obj_val(to_obj, VAL_DRINK_CONTAINER_CONTENTS, GET_DRINK_CONTAINER_CONTENTS(to_obj) + amount);
	
	// from-obj can no longer be stored to prevent refilling, if single-use
	if (OBJ_FLAGGED(from_obj, OBJ_SINGLE_USE)) {
		SET_BIT(GET_OBJ_EXTRA(from_obj), OBJ_NO_BASIC_STORAGE);
	}
	
	// copy the timer on the liquid, even if UNLIMITED
	GET_OBJ_TIMER(to_obj) = GET_OBJ_TIMER(from_obj);
	schedule_obj_timer_update(to_obj, FALSE);

	// check if there was too little to pour, and adjust
	if (GET_DRINK_CONTAINER_CONTENTS(from_obj) <= 0) {
		// add the negative amount to subtract it back
		set_obj_val(to_obj, VAL_DRINK_CONTAINER_CONTENTS, GET_DRINK_CONTAINER_CONTENTS(to_obj) + GET_DRINK_CONTAINER_CONTENTS(from_obj));
		amount += GET_DRINK_CONTAINER_CONTENTS(from_obj);
		set_obj_val(from_obj, VAL_DRINK_CONTAINER_CONTENTS, 0);
		set_obj_val(from_obj, VAL_DRINK_CONTAINER_TYPE, 0);
		
		if (OBJ_FLAGGED(from_obj, OBJ_SINGLE_USE)) {
			// single-use: extract it
			run_interactions(ch, GET_OBJ_INTERACTIONS(from_obj), INTERACT_CONSUMES_TO, IN_ROOM(ch), NULL, from_obj, NULL, consumes_or_decays_interact);
			empty_obj_before_extract(from_obj);
			extract_obj(from_obj);
		}
		else {
			// if it wasn't single-use, reset its timer to UNLIMITED since the timer refers to the contents
			GET_OBJ_TIMER(from_obj) = UNLIMITED;
		}
	}
	
	// check max
	set_obj_val(to_obj, VAL_DRINK_CONTAINER_CONTENTS, MIN(GET_DRINK_CONTAINER_CONTENTS(to_obj), GET_DRINK_CONTAINER_CAPACITY(to_obj)));
	request_obj_save_in_world(to_obj);
}


/**
* The following put modes are supported by the code below:
*   1) put <object> <container>
*   2) put all.<object> <container>
*   3) put all <container>
* 
*   <container> must be in inventory or on ground.
*  all objects to be put into container must be in inventory.
*/
ACMD(do_put) {
	char arg1[MAX_INPUT_LENGTH], arg2[MAX_INPUT_LENGTH], arg3[MAX_INPUT_LENGTH];
	obj_data *obj, *next_obj, *cont;
	vehicle_data *find_veh = NULL;
	char_data *tmp_char;
	int obj_dotmode, cont_dotmode, found = 0, howmany = 1, number;
	char *theobj, *thecont;
	bool multi = FALSE;

	argument = two_arguments(argument, arg1, arg2);
	one_argument(argument, arg3);

	if (*arg3 && is_number(arg1)) {
		howmany = atoi(arg1);
		theobj = arg2;
		thecont = arg3;
		multi = (howmany > 1);
	}
	else {
		theobj = arg1;
		thecont = arg2;
	}
	obj_dotmode = find_all_dots(theobj);
	cont_dotmode = find_all_dots(thecont);

	if (!*theobj)
		send_to_char("Put what in what?\r\n", ch);
	else if (cont_dotmode != FIND_INDIV)
		send_to_char("You can only put things into one container at a time.\r\n", ch);
	else if (!*thecont) {
		sprintf(buf, "What do you want to put %s in?\r\n", ((obj_dotmode == FIND_INDIV) ? "it" : "them"));
		send_to_char(buf, ch);
	}
	else {
		number = get_number(&thecont);
		if ((cont = get_obj_for_char_prefer_container(ch, thecont, &number))) {
			// found preferred container
		}
		else {
			// try another way
			generic_find(thecont, &number, FIND_OBJ_INV | FIND_OBJ_ROOM | FIND_OBJ_EQUIP | FIND_VEHICLE_ROOM | FIND_VEHICLE_INSIDE, ch, &tmp_char, &cont, &find_veh);
		}
		
		if (find_veh) {
			// override for put obj in vehicle
			do_put_obj_in_vehicle(ch, find_veh, obj_dotmode, theobj, howmany);
		}
		else if (!cont) {
			sprintf(buf, "You don't see %s %s here.\r\n", AN(thecont), thecont);
			send_to_char(buf, ch);
		}
		else if (GET_OBJ_TYPE(cont) != ITEM_CONTAINER)
			act("$p is not a container.", FALSE, ch, cont, 0, TO_CHAR);
		else if (OBJVAL_FLAGGED(cont, CONT_CLOSED) && GET_OBJ_TYPE(cont) == ITEM_CONTAINER)
			send_to_char("You'd better open it first!\r\n", ch);
		else {
			if (obj_dotmode == FIND_INDIV) {	/* put <obj> <container> */
				if (!(obj = get_obj_in_list_vis(ch, theobj, NULL, ch->carrying))) {
					sprintf(buf, "You aren't carrying %s %s.\r\n", AN(theobj), theobj);
					send_to_char(buf, ch);
				}
				else if (obj == cont)
					send_to_char("You attempt to fold it into itself, but fail.\r\n", ch);
				else if (multi && OBJ_FLAGGED(obj, OBJ_KEEP)) {
					msg_to_char(ch, "You marked that 'keep' and can't put it in anything unless you unkeep it.\r\n");
				}
				else {
					obj_data *next_obj;
					while(obj && howmany--) {
						next_obj = obj->next_content;
						
						if (multi && OBJ_FLAGGED(obj, OBJ_KEEP)) {
							continue;
						}
						
						if (!perform_put(ch, obj, cont))
							break;
						obj = get_obj_in_list_vis(ch, theobj, NULL, next_obj);
						found = 1;
					}
					
					if (!found) {
						msg_to_char(ch, "You didn't seem to have any that aren't marked 'keep'.\r\n");
					}
				}
			}
			else {
				DL_FOREACH_SAFE2(ch->carrying, obj, next_obj, next_content) {
					if (obj != cont && CAN_SEE_OBJ(ch, obj) && (obj_dotmode == FIND_ALL || isname(theobj, GET_OBJ_KEYWORDS(obj)))) {
						if (OBJ_FLAGGED(obj, OBJ_KEEP)) {
							continue;
						}
						found = 1;
						if (!perform_put(ch, obj, cont))
							break;
					}
				}
				if (!found) {
					if (obj_dotmode == FIND_ALL)
						send_to_char("You don't seem to have any non-keep items to put in it.\r\n", ch);
					else {
						sprintf(buf, "You don't seem to have any %ss that aren't marked 'keep'.\r\n", theobj);
						send_to_char(buf, ch);
					}
				}
			}
		}
	}
}


ACMD(do_quaff) {
	obj_data *obj;
	
	one_argument(argument, arg);
	
	if (!*arg) {
		msg_to_char(ch, "Which potion would you like to quaff?\r\n");
	}
	else if (!(obj = get_obj_in_list_vis(ch, arg, NULL, ch->carrying))) {
		msg_to_char(ch, "You don't seem to have %s %s.\r\n", AN(arg), arg);
	}
	else if (!IS_POTION(obj)) {
		msg_to_char(ch, "You can only quaff potions.\r\n");
	}
	else if (GET_POTION_COOLDOWN_TYPE(obj) != NOTHING && get_cooldown_time(ch, GET_POTION_COOLDOWN_TYPE(obj)) > 0) {
		msg_to_char(ch, "You can't quaff that until your %s cooldown expires.\r\n", get_generic_name_by_vnum(GET_POTION_COOLDOWN_TYPE(obj)));
	}
	else {
		if (GET_OBJ_CURRENT_SCALE_LEVEL(obj) == 0) {
			scale_item_to_level(obj, 1);	// just in case
		}
		
		if (!consume_otrigger(obj, ch, OCMD_QUAFF, NULL)) {
			return;	// check trigger last
		}

		// message to char
		if (obj_has_custom_message(obj, OBJ_CUSTOM_CONSUME_TO_CHAR)) {
			act(obj_get_custom_message(obj, OBJ_CUSTOM_CONSUME_TO_CHAR), FALSE, ch, obj, NULL, TO_CHAR);
		}
		else {
			act("You quaff $p!", FALSE, ch, obj, NULL, TO_CHAR);
		}
		
		// message to room
		if (obj_has_custom_message(obj, OBJ_CUSTOM_CONSUME_TO_ROOM)) {
			act(obj_get_custom_message(obj, OBJ_CUSTOM_CONSUME_TO_ROOM), TRUE, ch, obj, NULL, TO_ROOM);
		}
		else {
			act("$n quaffs $p!", TRUE, ch, obj, NULL, TO_ROOM);
		}
		
		apply_potion(obj, ch);
		
		run_interactions(ch, GET_OBJ_INTERACTIONS(obj), INTERACT_CONSUMES_TO, IN_ROOM(ch), NULL, obj, NULL, consumes_or_decays_interact);
		extract_obj(obj);
	}	
}


ACMD(do_remove) {
	int i, dotmode, found;
	int board_type;
	obj_data *board;

	one_argument(argument, arg);

	if (isdigit(*arg)) {
		DL_FOREACH2(ROOM_CONTENTS(IN_ROOM(ch)), board, next_content) {
			if (GET_OBJ_TYPE(board) == ITEM_BOARD) {
				break;
			}
		}
		if (board && ch->desc) {
			if (!board_loaded) {
				init_boards();
				board_loaded = 1;
				}
			if ((board_type = find_board(ch)) != -1)
				if (Board_remove_msg(board_type, ch, argument, board))
					return;
		}
	}

	if (!*arg) {
		send_to_char("Remove what?\r\n", ch);
		return;
	}
	dotmode = find_all_dots(arg);

	if (dotmode == FIND_ALL) {
		found = 0;
		for (i = 0; i < NUM_WEARS; i++)
			if (GET_EQ(ch, i)) {
				perform_remove(ch, i);
				found = 1;
			}
		if (!found)
			send_to_char("You're not using anything.\r\n", ch);
	}
	else if (dotmode == FIND_ALLDOT) {
		if (!*arg)
			send_to_char("Remove all of what?\r\n", ch);
		else {
			found = 0;
			for (i = 0; i < NUM_WEARS; i++)
				if (GET_EQ(ch, i) && CAN_SEE_OBJ(ch, GET_EQ(ch, i)) && isname(arg, GET_OBJ_KEYWORDS(GET_EQ(ch, i)))) {
					perform_remove(ch, i);
					found = 1;
				}
			if (!found) {
				sprintf(buf, "You don't seem to be using any %ss.\r\n", arg);
				send_to_char(buf, ch);
			}
		}
	}
	else {
		/* Returns object pointer but we don't need it, just true/false. */
		if (!get_obj_in_equip_vis(ch, arg, NULL, ch->equipment, &i)) {
			sprintf(buf, "You don't seem to be using %s %s.\r\n", AN(arg), arg);
			send_to_char(buf, ch);
		}
		else
			perform_remove(ch, i);
	}
}


ACMD(do_retrieve) {
	char buf[MAX_STRING_LENGTH], original[MAX_INPUT_LENGTH];
	struct empire_storage_data *store, *next_store;
	struct empire_island *isle;
	obj_data *objn;
	int count = 0, total = 1, number, pos;
	empire_data *emp, *room_emp = ROOM_OWNER(IN_ROOM(ch));
	char *objname;
	bool found = 0;

	if (!IS_COMPLETE(IN_ROOM(ch))) {
		msg_to_char(ch, "You'll need to finish the building first.\r\n");
		return;
	}
	if (!(emp = GET_LOYALTY(ch))) {
		msg_to_char(ch, "You can't store or retrieve resources unless you're a member of an empire.\r\n");
		return;
	}
	if (GET_RANK(ch) < EMPIRE_PRIV(emp, PRIV_STORAGE)) {
		msg_to_char(ch, "You aren't high enough rank to retrieve from the empire inventory.\r\n");
		return;
	}
	// requires room-use permission even if retrieving from a moving storage vehicle
	if (!can_use_room(ch, IN_ROOM(ch), GUESTS_ALLOWED)) {
		msg_to_char(ch, "You are not allowed to retrieve anything here (it isn't public).\r\n");
		return;
	}
	/* don't check relationship here -- it is checked in obj_can_be_retrieved
	if (room_emp && emp != room_emp && !has_relationship(emp, room_emp, DIPL_TRADE)) {
		msg_to_char(ch, "You need to establish a trade pact to retrieve anything here.\r\n");
		return;
	}
	*/
	if (GET_ISLAND_ID(IN_ROOM(ch)) == NO_ISLAND || !(isle = get_empire_island(emp, GET_ISLAND_ID(IN_ROOM(ch))))) {
		msg_to_char(ch, "You can't retrieve anything here.\r\n");
		return;
	}
	if (!check_in_city_requirement(IN_ROOM(ch), TRUE)) {
		msg_to_char(ch, "This storage building must be in a city to use it.\r\n");
		return;
	}
	
	skip_spaces(&argument);
	strcpy(original, argument);
	half_chop(argument, arg, buf);

	if (*arg && is_number(arg)) {
		// look for "retrieve ### item"
		total = atoi(arg);
		if (total < 1) {
			msg_to_char(ch, "You have to retrieve at least 1.\r\n");
			return;
		}
		strcpy(arg, buf);
	}
	else if (*arg && *buf && !str_cmp(arg, "all")) {
		// look for "retrieve all item"
		total = CAN_CARRY_N(ch) + 1;
		strcpy(arg, buf);
	}
	else if (*arg && *buf) {
		// it wasn't a number -- reappend the string
		sprintf(arg + strlen(arg), " %s", buf);
	}
	
	// look for all.
	if (*arg && !strn_cmp(arg, "all.", 4)) {
		total = CAN_CARRY_N(ch) + 1;	// we want to hit the overfull message if possible
		strcpy(buf, arg);
		strcpy(arg, buf + 4);
	}
	
	// find number
	objname = arg;
	number = get_number(&objname);

	// any remaining string for the item to retrieve?
	if (!*objname) {
		msg_to_char(ch, "What would you like to retrieve?\r\n");
		return;
	}

	/* they hit "ret all" */
	if (!str_cmp(objname, "all")) {
		HASH_ITER(hh, isle->store, store, next_store) {
			if (store->amount > 0 && (objn = store->proto) && obj_can_be_retrieved(objn, IN_ROOM(ch), GET_LOYALTY(ch))) {
				if (stored_item_requires_withdraw(objn) && !has_permission(ch, PRIV_WITHDRAW, IN_ROOM(ch))) {
					msg_to_char(ch, "You don't have permission to withdraw that!\r\n");
					return;
				}
				else {
					/* retrieve as much as possible.. */
					while (store->amount > 0) {
						++count;
						if (!retrieve_resource(ch, emp, store, FALSE)) {
							break;
						}
					}
				}
			}
		}
	}
	else {	// not "all"
		pos = 0;
		HASH_ITER(hh, isle->store, store, next_store) {
			if (found) {
				break;
			}
			
			if (store->amount > 0 && (objn = store->proto) && obj_can_be_retrieved(objn, IN_ROOM(ch), GET_LOYALTY(ch))) {
				if (multi_isname(objname, GET_OBJ_KEYWORDS(objn)) && (++pos == number)) {
					found = 1;
					
					if (stored_item_requires_withdraw(objn) && !has_permission(ch, PRIV_WITHDRAW, IN_ROOM(ch))) {
						msg_to_char(ch, "You don't have permission to withdraw that!\r\n");
						return;
					}
					else {
						/* found it!  retrieve now! */
						while (count < total && store->amount > 0) {
							++count;
							if (!retrieve_resource(ch, emp, store, FALSE)) {
								break;
							}
						}
					}
				}
			}
		}

		if (!found) {
			if (room_has_function_and_city_ok(GET_LOYALTY(ch), IN_ROOM(ch), FNC_WAREHOUSE | FNC_VAULT) && str_cmp(original, "all")) {
				// pass control to warehouse func
				sprintf(buf, "retrieve %s", original);
				do_warehouse(ch, buf, 0, 0);
			}
			else if (ROOM_PRIVATE_OWNER(HOME_ROOM(IN_ROOM(ch))) == GET_IDNUM(ch) && str_cmp(original, "all")) {
				// pass control to home store func
				sprintf(buf, "retrieve %s", original);
				do_home(ch, buf, 0, 0);
			}
			else {
				msg_to_char(ch, "Nothing like that is stored here!\r\n");
			}
			return;
		}
	}

	if (count == 0) {
		msg_to_char(ch, "There is nothing stored here!\r\n");
	}
	else {
		// remove the "ceded" bit on this room (it was used)
		if (GET_LOYALTY(ch) == room_emp) {
			remove_room_extra_data(IN_ROOM(ch), ROOM_EXTRA_CEDED);
		}
		
		read_vault(emp);
	}
}


ACMD(do_roadsign) {
	obj_data *sign;
	int max_length = 70;	// "Sign: " prefix; don't want to go over 80
	bool old_sign = FALSE;
	
	skip_spaces(&argument);
	
	if (IS_NPC(ch)) {
		msg_to_char(ch, "NPCs can't use roadsign.\r\n");
	}
	else if (!IS_APPROVED(ch) && config_get_bool("build_approval")) {
		send_config_msg(ch, "need_approval_string");
	}
	else if (!has_player_tech(ch, PTECH_CUSTOMIZE_BUILDING)) {
		msg_to_char(ch, "You require the Customize Building ability to set up road signs.\r\n");
	}
	else if (!IS_ROAD(IN_ROOM(ch)) || !IS_COMPLETE(IN_ROOM(ch))) {
		msg_to_char(ch, "You can only put up a roadsign on finished roads.\r\n");
	}
	else if (!can_use_room(ch, IN_ROOM(ch), MEMBERS_ONLY)) {
		msg_to_char(ch, "You do not have permission to set up road signs here.\r\n");
	}
	else if (!(sign = get_obj_in_list_num(o_BLANK_SIGN, ch->carrying)) && !(sign = get_obj_in_list_num(o_BLANK_SIGN, ROOM_CONTENTS(IN_ROOM(ch))))) {
		msg_to_char(ch, "You don't seem to have a blank sign to set up.\r\n");
	}
	else if (!*argument) {
		msg_to_char(ch, "Usage: roadsign <message>\r\n");
	}
	else if (color_strlen(argument) > max_length) {
		msg_to_char(ch, "Road signs can't be more than %d characters long.\r\n", max_length);
	}
	else if (run_ability_triggers_by_player_tech(ch, PTECH_CUSTOMIZE_BUILDING, NULL, NULL, NULL)) {
		// triggered
	}
	else {
		if (ROOM_CUSTOM_DESCRIPTION(IN_ROOM(ch))) {
			old_sign = TRUE;
		}
		set_room_custom_description(IN_ROOM(ch), argument);

		act("You engrave your message and plant $p in the ground by the road.", FALSE, ch, sign, NULL, TO_CHAR);
		act("$n engraves a message on $p and plants it in the ground by the road.", FALSE, ch, sign, NULL, TO_ROOM);
		
		if (old_sign) {
			msg_to_char(ch, "The old sign has been replaced.\r\n");
		}

		gain_player_tech_exp(ch, PTECH_CUSTOMIZE_BUILDING, 33.4);
		run_ability_hooks_by_player_tech(ch, PTECH_CUSTOMIZE_BUILDING, NULL, NULL, NULL, IN_ROOM(ch));
		extract_obj(sign);
	}
}


ACMD(do_seed) {
	char arg[MAX_INPUT_LENGTH];
	obj_data *obj;
	
	one_argument(argument, arg);
	
	if (!*arg) {
		msg_to_char(ch, "Remove the seeds from what?\r\n");
	}
	else if (!(obj = get_obj_in_list_vis_prefer_interaction(ch, arg, NULL, ch->carrying, INTERACT_SEED))) {
		msg_to_char(ch, "You don't have %s %s.\r\n", AN(arg), arg);
	}
	else if (!has_interaction(GET_OBJ_INTERACTIONS(obj), INTERACT_SEED)) {
		msg_to_char(ch, "You can't seed that!\r\n");
	}
	else if (OBJ_FLAGGED(obj, OBJ_SEEDED)) {
		msg_to_char(ch, "It has already been seeded.\r\n");
	}
	else {		
		if (run_interactions(ch, GET_OBJ_INTERACTIONS(obj), INTERACT_SEED, IN_ROOM(ch), NULL, obj, NULL, seed_obj_interact)) {
			SET_BIT(GET_OBJ_EXTRA(obj), OBJ_SEEDED | OBJ_NO_BASIC_STORAGE);
			request_obj_save_in_world(obj);
		}
		else {
			act("You fail to seed $p.", FALSE, ch, obj, NULL, TO_CHAR);
		}
		command_lag(ch, WAIT_OTHER);
	}
}


ACMD(do_separate) {
	char arg[MAX_INPUT_LENGTH];
	obj_data *obj;
	
	one_argument(argument, arg);
	
	if (!*arg) {
		msg_to_char(ch, "Separate what?\r\n");
	}
	else if (!(obj = get_obj_in_list_vis_prefer_interaction(ch, arg, NULL, ch->carrying, INTERACT_SEPARATE))) {
		msg_to_char(ch, "You don't have %s %s.\r\n", AN(arg), arg);
	}
	else if (!has_interaction(GET_OBJ_INTERACTIONS(obj), INTERACT_SEPARATE)) {
		msg_to_char(ch, "You can't separate that!\r\n");
	}
	else {		
		if (run_interactions(ch, GET_OBJ_INTERACTIONS(obj), INTERACT_SEPARATE, IN_ROOM(ch), NULL, obj, NULL, separate_obj_interact)) {
			if (GET_LOYALTY(ch)) {
				// subtract old item from empire counts
				add_production_total(GET_LOYALTY(ch), GET_OBJ_VNUM(obj), -1);
			}
			
			// and extract it
			extract_obj(obj);
		}
		else {
			act("You fail to separate $p.", FALSE, ch, obj, NULL, TO_CHAR);
		}
		command_lag(ch, WAIT_OTHER);
	}
}


// does not call can_wear_item() since the item doesn't count stats
ACMD(do_share) {
	obj_data *obj;
	
	one_argument(argument, arg);
	
	if (IS_NPC(ch)) {
		msg_to_char(ch, "NPCs may not share items.\r\n");
	}
	else if (!*arg) {
		msg_to_char(ch, "Share what?\r\n");
	}
	else if (!(obj = get_obj_in_list_vis(ch, arg, NULL, ch->carrying))) {
		msg_to_char(ch, "You don't seem to have %s %s.\r\n", AN(arg), arg);
	}
	else {
		if (GET_EQ(ch, WEAR_SHARE)) {
			do_unshare(ch, "", 0, 0);
		}
		perform_wear(ch, obj, WEAR_SHARE);
	}
}


ACMD(do_sheathe) {
	char arg[MAX_INPUT_LENGTH];
	obj_data *obj;
	int to_loc = NOWHERE, from_loc = NOWHERE;
	
	one_argument(argument, arg);
	
	/* are any of them even open? */
	if (!GET_EQ(ch, WEAR_SHEATH_1)) {
		to_loc = WEAR_SHEATH_1;
	}
	else if (!GET_EQ(ch, WEAR_SHEATH_2)) {
		to_loc = WEAR_SHEATH_2;
	}
	else {
		msg_to_char(ch, "You have no empty sheaths!\r\n");
		return;
	}
	
	// determine obj and from_loc
	if ((obj = GET_EQ(ch, WEAR_WIELD)) && (!*arg || isname(arg, GET_OBJ_KEYWORDS(obj)))) {
		from_loc = WEAR_WIELD;
	}
	else if ((obj = GET_EQ(ch, WEAR_HOLD)) && IS_WEAPON(obj) && (!*arg || isname(arg, GET_OBJ_KEYWORDS(obj)))) {
		from_loc = WEAR_HOLD;
	}
	else if (*arg) {
		if ((obj = get_obj_in_list_vis(ch, arg, NULL, ch->carrying))) {
			// found obj
			from_loc = NOWHERE;
		}
		else {
			msg_to_char(ch, "You don't have %s %s to sheathe.\r\n", AN(arg), arg);
			return;
		}
	}
	else {	// no arg and no weapons to sheath
		msg_to_char(ch, "You aren't wielding anything you can sheath!\r\n");
		return;
	}
	
	if (!IS_WEAPON(obj)) {
		msg_to_char(ch, "You can only sheathe weapons.\r\n");
		return;
	}
	
	// don't allow sheathing of 1-use objs
	if (OBJ_FLAGGED(obj, OBJ_SINGLE_USE)) {
		msg_to_char(ch, "You can't sheathe a single-use item.\r\n");
		return;
	}
	
	// and do it
	if (from_loc != NOWHERE && obj->worn_by) {
		obj_to_char(unequip_char(ch, from_loc), ch);
	}
	perform_wear(ch, obj, to_loc);
	
	if (FIGHTING(ch)) {
		command_lag(ch, WAIT_COMBAT_ABILITY);
	}
}


ACMD(do_ship) {
	char arg1[MAX_INPUT_LENGTH], arg2[MAX_INPUT_LENGTH], buf[MAX_STRING_LENGTH * 3], line[1000], keywords[MAX_INPUT_LENGTH];
	char *strptr;
	struct island_info *from_isle, *to_isle;
	empire_data *emp = GET_LOYALTY(ch);
	struct empire_storage_data *store, *new_store;
	struct shipping_data *sd;
	bool done, wrong_isle, gave_number = FALSE, all = FALSE, targeted_island = FALSE;
	bool imm_access = GET_ACCESS_LEVEL(ch) >= LVL_CIMPL || IS_GRANTED(ch, GRANT_EMPIRES);
	room_data *to_room, *docks;
	vehicle_data *veh;
	obj_data *proto;
	int number = 1;
	size_t size;
	
	// SHIPPING_x
	const char *status_type[] = { "preparing", "en route", "delivered", "waiting for ship", "\n" };
	
	// optional empire arg
	if (imm_access) {
		strptr = any_one_word(argument, arg1);
		if ((emp = get_empire_by_name(arg1))) {
			// was an empire arg
			argument = strptr;
		}
		else {
			// not an empire arg (keep original argument)
			emp = GET_LOYALTY(ch);
		}
	}
	
	argument = any_one_word(argument, arg1);	// command
	argument = any_one_word(argument, arg2);	// number/all or keywords
	skip_spaces(&argument);	// keywords
	
	if (isdigit(*arg2)) {
		number = atoi(arg2);
		gave_number = TRUE;
		snprintf(keywords, sizeof(keywords), "%s", argument);
	}
	else if (!str_cmp(arg2, "all")) {
		all = TRUE;
		snprintf(keywords, sizeof(keywords), "%s", argument);
	}
	else {
		// concatenate arg2 and argument back together, it's just keywords
		snprintf(keywords, sizeof(keywords), "%s%s%s", arg2, *argument ? " " : "", argument);
	}
	
	if (!IS_APPROVED(ch) && config_get_bool("manage_empire_approval")) {
		send_config_msg(ch, "need_approval_string");
	}
	else if (IS_NPC(ch) || !emp || !ch->desc) {
		msg_to_char(ch, "You can't use the shipping system unless you're in an empire.\r\n");
	}
	else if (!imm_access && GET_RANK(ch) < EMPIRE_PRIV(emp, PRIV_SHIPPING)) {
		msg_to_char(ch, "You don't have permission to ship anything.\r\n");
	}
	else if (!*arg1) {
		msg_to_char(ch, "Usage: ship status\r\n");
		msg_to_char(ch, "Usage: ship cancel [number] <item>\r\n");
		msg_to_char(ch, "Usage: ship <island> [number | all] <item>\r\n");
	}
	else if (!str_cmp(arg1, "status") || !str_cmp(arg1, "stat")) {
		size = snprintf(buf, sizeof(buf), "Shipping queue for %s:\r\n", EMPIRE_NAME(emp));
		
		done = FALSE;
		DL_FOREACH(EMPIRE_SHIPPING_LIST(emp), sd) {
			if (sd->vnum == NOTHING) {
				// just a ship, not a shipment
				if (sd->shipping_id == -1 || !(veh = find_ship_by_shipping_id(emp, sd->shipping_id))) {
					continue;
				}
				if (*keywords && !multi_isname(keywords, VEH_KEYWORDS(veh))) {
					continue;
				}
				
				from_isle = get_island(sd->from_island, TRUE);
				to_isle = get_island(sd->to_island, TRUE);
				to_room = sd->to_room == NOWHERE ? NULL : real_room(sd->to_room);
				snprintf(line, sizeof(line), "    %s (%s to %s%s, %s)\r\n", skip_filler(VEH_SHORT_DESC(veh)), from_isle ? get_island_name_for(from_isle->id, ch) : "unknown", to_isle ? get_island_name_for(to_isle->id, ch) : "unknown", coord_display(ch, to_room ? X_COORD(to_room) : -1, to_room ? Y_COORD(to_room) : -1, FALSE), status_type[sd->status]);
			}
			else {
				// normal object shipment
				if (!(proto = obj_proto(sd->vnum))) {
					continue;
				}
				if (*keywords && !multi_isname(keywords, GET_OBJ_KEYWORDS(proto))) {
					// skip non-matching keywords, if-requested
					continue;
				}
				
				from_isle = get_island(sd->from_island, TRUE);
				to_isle = get_island(sd->to_island, TRUE);
				to_room = sd->to_room == NOWHERE ? NULL : real_room(sd->to_room);
				snprintf(line, sizeof(line), " %dx %s (%s to %s%s, %s)\r\n", sd->amount, skip_filler(GET_OBJ_SHORT_DESC(proto)), from_isle ? get_island_name_for(from_isle->id, ch) : "unknown", to_isle ? get_island_name_for(to_isle->id, ch) : "unknown", coord_display(ch, to_room ? X_COORD(to_room) : -1, to_room ? Y_COORD(to_room) : -1, FALSE), status_type[sd->status]);
			}
			
			done = TRUE;
			if (size + strlen(line) >= sizeof(buf)) {
				// too long
				size += snprintf(buf + size, sizeof(buf) - size, " ...\r\n");
				break;
			}
			else {
				size += snprintf(buf + size, sizeof(buf) - size, "%s", line);
			}
		}
		
		if (!done) {
			size += snprintf(buf + size, sizeof(buf) - size, " nothing\r\n");
		}
		
		page_string(ch->desc, buf, TRUE);
	}
	else if (emp != GET_LOYALTY(ch)) {
		msg_to_char(ch, "You may only check the status of other empires' shipments.\r\n");
	}
	else if (GET_POS(ch) < POS_RESTING) {
		send_low_pos_msg(ch);
	}
	else if (GET_ISLAND_ID(IN_ROOM(ch)) == NO_ISLAND) {
		msg_to_char(ch, "You can't ship anything from here.\r\n");
	}
	else if (!str_cmp(arg1, "cancel")) {
		if (!*keywords) {
			msg_to_char(ch, "Cancel which shipment?\r\n");
			return;
		}
		
		// find a matching entry
		done = wrong_isle = FALSE;
		DL_FOREACH(EMPIRE_SHIPPING_LIST(emp), sd) {
			if ((sd->status != SHIPPING_QUEUED && sd->status != SHIPPING_WAITING_FOR_SHIP) || sd->shipping_id != -1) {
				continue;	// never cancel one in progress
			}
			if (gave_number && number != sd->amount) {
				continue;
			}
			if (!(proto = obj_proto(sd->vnum))) {
				continue;
			}
			if (!multi_isname(keywords, GET_OBJ_KEYWORDS(proto))) {
				continue;
			}
			if (sd->from_island != GET_ISLAND_ID(IN_ROOM(ch))) {
				wrong_isle = TRUE;
				continue;
			}
			
			// found!
			msg_to_char(ch, "You cancel the shipment for %d '%s'.\r\n", sd->amount, skip_filler(GET_OBJ_SHORT_DESC(proto)));
			if (sd->amount > 0) {
				// amount can drop to 0 if they all decayed
				new_store = add_to_empire_storage(emp, sd->from_island, sd->vnum, sd->amount, 0);
				if (new_store) {
					merge_storage_timers(&new_store->timers, sd->timers, sd->amount);
				}
			}
			
			DL_DELETE(EMPIRE_SHIPPING_LIST(emp), sd);
			free_shipping_data(sd);
			EMPIRE_NEEDS_STORAGE_SAVE(emp) = TRUE;
			
			done = TRUE;
			break;	// only allow 1st match
		}
		
		if (!done) {
			msg_to_char(ch, "No shipments like that found to cancel%s.\r\n", wrong_isle ? " on this island" : "");
		}
	}
	else {
		if (number < 1 || !*keywords) {
			msg_to_char(ch, "Usage: ship <island> [number] <item>\r\n");
		}
		else if (!(docks = find_docks(emp, GET_ISLAND_ID(IN_ROOM(ch))))) {
			msg_to_char(ch, "This island has no docks (docks must not be set no-work).\r\n");
		}
		else if (!(to_room = get_shipping_target(ch, arg1, &targeted_island))) {
			// sent own message
		}
		else if (to_room == docks) {
			msg_to_char(ch, "You can't ship to the same docks the ships are dispatched from.\r\n");
		}
		else if (targeted_island && GET_ISLAND_ID(to_room) == GET_ISLAND_ID(IN_ROOM(ch))) {
			msg_to_char(ch, "You are already on that island.\r\n");
		}
		else if (!(store = find_island_storage_by_keywords(emp, GET_ISLAND_ID(IN_ROOM(ch)), keywords, TRUE)) || store->amount < 1) {
			msg_to_char(ch, "You don't seem to have any '%s' stored on this island to ship.\r\n", keywords);
		}
		else if (!all && store->amount < number) {
			msg_to_char(ch, "You only have %d '%s' stored on this island.\r\n", store->amount, skip_filler(GET_OBJ_SHORT_DESC(store->proto)));
		}
		else {
			add_shipping_queue(ch, emp, GET_ISLAND_ID(IN_ROOM(ch)), GET_ISLAND_ID(to_room), all ? store->amount : number, store, to_room);
		}
	}
}


ACMD(do_split) {
	char buf[MAX_STRING_LENGTH];
	int coin_amt = 0, count, split;
	struct coin_data *coin = NULL;
	empire_data *coin_emp = NULL;
	char_data *vict;
	char *pos;
	
	// count group members present
	count = 0;
	DL_FOREACH2(ROOM_PEOPLE(IN_ROOM(ch)), vict, next_in_room) {
		if (!IS_NPC(vict) && GROUP(vict) && GROUP(vict) == GROUP(ch)) {
			++count;
		}
	}

	// parse args
	pos = find_coin_arg(argument, &coin_emp, &coin_amt, TRUE, TRUE, NULL);
	
	if (!*argument || pos == argument || coin_amt <= 0) {
		msg_to_char(ch, "Usage: split <amount> <type> coins\r\n");
	}
	else if (!GROUP(ch) || count <= 1) {
		msg_to_char(ch, "You don't have any group members here to split coins with.\r\n");
	}
	else if (coin_amt < count) {
		msg_to_char(ch, "There are %d group members here, so you'll need to split at least %d coins.\r\n", count, count);
	}
	else if (!(coin = find_coin_entry(GET_PLAYER_COINS(ch), coin_emp)) || coin->amount < coin_amt) {
		msg_to_char(ch, "You don't have %s.\r\n", money_amount(coin_emp, coin_amt));
	}
	else {
		split = (int) (coin_amt / count);	// clean split
		
		msg_to_char(ch, "You split %s among %d group member%s.\r\n", money_amount(coin_emp, coin_amt), count, PLURAL(count));
		sprintf(buf, "$n splits %s among %d group member%s; you receive %d.", money_amount(coin_emp, coin_amt), count, PLURAL(count), split);
		
		DL_FOREACH2(ROOM_PEOPLE(IN_ROOM(ch)), vict, next_in_room) {
			if (vict != ch && !IS_NPC(vict) && GROUP(vict) && GROUP(vict) == GROUP(ch)) {
				increase_coins(vict, coin_emp, split);
				decrease_coins(ch, coin_emp, split);
				act(buf, FALSE, ch, NULL, vict, TO_VICT | TO_SLEEP);
			}
		}
	}
}


ACMD(do_store) {
	char buf[MAX_STRING_LENGTH], arg1[MAX_INPUT_LENGTH], arg2[MAX_INPUT_LENGTH];
	struct empire_storage_data *store;
	obj_data *obj, *next_obj, *lists[2];
	int count = 0, total = 1, done = 0, dotmode, iter;
	empire_data *emp, *room_emp = ROOM_OWNER(IN_ROOM(ch));
	bool full = FALSE, use_room = FALSE, all = FALSE, room = FALSE, inv = FALSE;

	if (!IS_COMPLETE(IN_ROOM(ch))) {
		msg_to_char(ch, "You'll need to finish the building first.\r\n");
		return;
	}

	if (!(emp = GET_LOYALTY(ch))) {
		msg_to_char(ch, "You can't store or retrieve resources unless you're a member of an empire.\r\n");
		return;
	}
	if (!can_use_room(ch, IN_ROOM(ch), GUESTS_ALLOWED)) {
		msg_to_char(ch, "You are not allowed to store anything here (it isn't public).\r\n");
		return;
	}
	/* don't check relationship here -- it is checked in obj_can_be_retrieved
	if (room_emp && emp != room_emp && !has_relationship(emp, room_emp, DIPL_TRADE)) {
		msg_to_char(ch, "You need to establish a trade pact to store your things here.\r\n");
		return;
	}
	*/
	if (!check_in_city_requirement(IN_ROOM(ch), TRUE)) {
		msg_to_char(ch, "This storage building must be in a city to use it.\r\n");
		return;
	}

	skip_spaces(&argument);
	two_arguments(argument, arg1, arg2);

	/* This goes first because I want it to move arg2 to arg1 */
	if (*arg1 && is_number(arg1)) {
		total = atoi(arg1);
		if (total < 1) {
			msg_to_char(ch, "You have to store at least 1.\r\n");
			return;
		}
		strcpy(arg1, arg2);
		*arg2 = '\0';
	}
	else if (*arg1 && *arg2 && !str_cmp(arg1, "all")) {	// 'store all <thing>'
		total = CAN_CARRY_N(ch) + 1;
		strcpy(arg1, arg2);
		*arg2 = '\0';
	}
	else if (!str_cmp(arg1, "all") && !*arg2) {
		all = TRUE;	// basic 'store all'
	}
	else if (!str_cmp(arg1, "room") && !*arg2) {
		room = TRUE;	// store all on ground
	}
	else if ((!str_cmp(arg1, "inv") || !str_cmp(arg1, "inventory")) && !*arg2) {
		inv = TRUE;	// store all from inventory
	}

	if (!*arg1) {
		msg_to_char(ch, "What would you like to store?\r\n");
		return;
	}

	// set up search lists for "all" modes
	lists[0] = !room ? ch->carrying : NULL;	// only if they didn't specify 'room'
	use_room = (!inv && ROOM_OWNER(IN_ROOM(ch)) == GET_LOYALTY(ch));
	lists[1] = use_room ? ROOM_CONTENTS(IN_ROOM(ch)) : NULL;
	
	if (!use_room && room) {
		msg_to_char(ch, "You can't store items from the room because your empire doesn't own it.\r\n");
		return;
	}
	
	dotmode = find_all_dots(arg1);

	if (dotmode == FIND_ALL || all || room || inv) {
		for (iter = 0; iter < 2; ++iter) {
			DL_FOREACH_SAFE2(lists[iter], obj, next_obj, next_content) {
				if (!OBJ_FLAGGED(obj, OBJ_KEEP) && OBJ_CAN_STORE(obj) && obj_can_be_stored(obj, IN_ROOM(ch), GET_LOYALTY(ch), FALSE)) {
					if ((store = find_stored_resource(emp, GET_ISLAND_ID(IN_ROOM(ch)), GET_OBJ_VNUM(obj)))) {
						if (store->amount >= MAX_STORAGE) {
							full = 1;
						}
					}
					if (!full) {
						done += store_resource(ch, emp, obj);
					}
				}
			}
		}
		if (!done) {
			if (full) {
				msg_to_char(ch, "It's full.\r\n");
			}
			else if (room_has_function_and_city_ok(GET_LOYALTY(ch), IN_ROOM(ch), FNC_WAREHOUSE | FNC_VAULT) && str_cmp(argument, "all")) {
				// pass control to warehouse func
				sprintf(buf, "store %s", argument);
				do_warehouse(ch, buf, 0, 0);
			}
			else if (ROOM_PRIVATE_OWNER(HOME_ROOM(IN_ROOM(ch))) == GET_IDNUM(ch) && str_cmp(argument, "all")) {
				// pass control to home store func
				sprintf(buf, "store %s", argument);
				do_home(ch, buf, 0, 0);
			}
			else {
				msg_to_char(ch, "You don't have anything that can be stored here.\r\n");
			}
		}
	}
	else {
		char *argptr = arg1;
		int number = get_number(&argptr);
		
		if (!*arg1) {
			msg_to_char(ch, "What would you like to store all of?\r\n");
			return;
		}
		if (!(obj = get_obj_in_list_vis(ch, argptr, &number, ch->carrying)) && (!use_room || !(obj = get_obj_in_list_vis(ch, argptr, &number, ROOM_CONTENTS(IN_ROOM(ch)))))) {
			msg_to_char(ch, "You don't seem to have any %ss.\r\n", arg1);
			return;
		}
		while (obj && (dotmode == FIND_ALLDOT || count < total)) {
			// try to set up next-obj
			if (!(next_obj = get_obj_in_list_vis(ch, argptr, NULL, obj->next_content)) && obj->carried_by && use_room) {
				next_obj = get_obj_in_list_vis(ch, argptr, NULL, ROOM_CONTENTS(IN_ROOM(ch)));
			}
			
			if ((!OBJ_FLAGGED(obj, OBJ_KEEP) || (total == 1 && dotmode != FIND_ALLDOT)) && OBJ_CAN_STORE(obj) && obj_can_be_stored(obj, IN_ROOM(ch), GET_LOYALTY(ch), FALSE)) {
				if ((store = find_stored_resource(emp, GET_ISLAND_ID(IN_ROOM(ch)), GET_OBJ_VNUM(obj)))) {
					if (store->amount >= MAX_STORAGE) {
						full = 1;
					}
				}
				if (!full) {
					done += store_resource(ch, emp, obj);
					count++;
				}
			}
			obj = next_obj;
		}
		if (!done) {
			if (full) {
				msg_to_char(ch, "It's full.\r\n");
			}
			else if (room_has_function_and_city_ok(GET_LOYALTY(ch), IN_ROOM(ch), FNC_WAREHOUSE | FNC_VAULT)) {
				// pass control to warehouse func
				sprintf(buf, "store %s", argument);
				do_warehouse(ch, buf, 0, 0);
			}
			else if (ROOM_PRIVATE_OWNER(HOME_ROOM(IN_ROOM(ch))) == GET_IDNUM(ch)) {
				// pass control to home store func
				sprintf(buf, "store %s", argument);
				do_home(ch, buf, 0, 0);
			}
			else {
				msg_to_char(ch, "You can't store that here!\r\n");
			}
		}
	}
	
	if (done > 0) {
		// remove the "ceded" bit on this room
		if (GET_LOYALTY(ch) == room_emp) {
			remove_room_extra_data(IN_ROOM(ch), ROOM_EXTRA_CEDED);
		}
		
		read_vault(emp);
	}
}


ACMD(do_swap) {
	obj_data *wield, *hold;

	if (!(wield = GET_EQ(ch, WEAR_WIELD)))
		msg_to_char(ch, "You're not even wielding anything to swap!\r\n");
	else if (!(hold = GET_EQ(ch, WEAR_HOLD)))
		msg_to_char(ch, "You're not even holding anything to swap!\r\n");
	else if (!CAN_WEAR(wield, ITEM_WEAR_HOLD))
		act("You can't hold $p.", FALSE, ch, wield, 0, TO_CHAR);
	else if (!CAN_WEAR(hold, ITEM_WEAR_WIELD))
		act("You can't wield $p.", FALSE, ch, hold, 0, TO_CHAR);
	else if (OBJ_FLAGGED(wield, OBJ_SINGLE_USE) || OBJ_FLAGGED(hold, OBJ_SINGLE_USE)) {
		msg_to_char(ch, "You can't swap single-use items.\r\n");
	}
	else if (OBJ_FLAGGED(wield, OBJ_TWO_HANDED) || OBJ_FLAGGED(hold, OBJ_TWO_HANDED)) {
		// this shouldn't even be reachable, but just in case
		msg_to_char(ch, "You can't swap two-handed weapons.\r\n");
	}
	else {
		// do not use unequip_char_to_inventory here so that we do not check OBJ_SINGLE_USE
		obj_to_char(unequip_char(ch, WEAR_WIELD), ch);
		obj_to_char(unequip_char(ch, WEAR_HOLD), ch);
		perform_wear(ch, hold, WEAR_WIELD);
		perform_wear(ch, wield, WEAR_HOLD);
		
		if (FIGHTING(ch)) {
			command_lag(ch, WAIT_COMBAT_ABILITY);
		}
	}
}


ACMD(do_trade) {
	char command[MAX_INPUT_LENGTH];
	
	argument = any_one_arg(argument, command);
	skip_spaces(&argument);

	if (IS_NPC(ch) || !ch->desc) {
		msg_to_char(ch, "You can't trade right now.\r\n");
	}
	else if (is_abbrev(command, "check")) {
		trade_check(ch, argument);
	}
	else if (!room_has_function_and_city_ok(GET_LOYALTY(ch), IN_ROOM(ch), FNC_TRADING_POST) && !IS_IMMORTAL(ch)) {
		msg_to_char(ch, "You can't trade here.\r\n");
	}
	else if (!IS_COMPLETE(IN_ROOM(ch)) && !IS_IMMORTAL(ch)) {
		msg_to_char(ch, "Complete the building first.\r\n");
	}
	else if (!can_use_room(ch, IN_ROOM(ch), GUESTS_ALLOWED) && !IS_IMMORTAL(ch)) {
		msg_to_char(ch, "You don't have permission to trade here.\r\n");
	}
	else if (is_abbrev(command, "list")) {
		trade_list(ch, argument);
	}
	else if (is_abbrev(command, "buy")) {
		trade_buy(ch, argument);
	}
	else if (is_abbrev(command, "cancel")) {
		trade_cancel(ch, argument);
	}
	else if (is_abbrev(command, "collect")) {
		trade_collect(ch, argument);
	}
	else if (is_abbrev(command, "identify")) {
		trade_identify(ch, argument);
	}
	else if (is_abbrev(command, "post")) {
		trade_post(ch, argument);
	}
	else {
		msg_to_char(ch, "Usage: trade <check | list | buy | cancel | collect | identify | post>\r\n");
	}
}


ACMD(do_unshare) {
	if (!GET_EQ(ch, WEAR_SHARE)) {
		msg_to_char(ch, "You are not sharing anything.\r\n");
	}
	else {
		// we don't perform_remove() because it checks things we don't need to check, and we want a custom message
		
		act("You stop sharing $p.", FALSE, ch, GET_EQ(ch, WEAR_SHARE), 0, TO_CHAR);
		act("$n stops sharing $p.", TRUE, ch, GET_EQ(ch, WEAR_SHARE), 0, TO_ROOM);
		
		// this may extract it, or drop it
		unequip_char_to_inventory(ch, WEAR_SHARE);
		determine_gear_level(ch);
	}
}


ACMD(do_use) {
	obj_data *obj;
	
	one_argument(argument, arg);
	
	if (!*arg) {
		msg_to_char(ch, "Use what?\r\n");
	}
	else if (!(obj = get_obj_in_list_vis(ch, arg, NULL, ch->carrying))) {
		// check tool slot?
		if (GET_EQ(ch, WEAR_TOOL) && MATCH_ITEM_NAME(arg, GET_EQ(ch, WEAR_TOOL))) {
			msg_to_char(ch, "It looks like you're already using it!\r\n");
		}
		else {
			msg_to_char(ch, "You don't seem to have %s %s.\r\n", AN(arg), arg);
		}
	}
	else {
		if (IS_POISON(obj)) {
			use_poison(ch, obj);
			return;
		}
		if (IS_AMMO(obj)) {
			use_ammo(ch, obj);
			return;
		}
		if (IS_MINIPET(obj)) {
			use_minipet_obj(ch, obj);
			return;
		}
		if (GET_OBJ_TOOL_FLAGS(obj)) {	// use as tool
			if (GET_EQ(ch, WEAR_TOOL)) {
				// try to remove
				perform_remove(ch, WEAR_TOOL);
			}
			if (GET_EQ(ch, WEAR_TOOL)) {	// still?
				act("You're already using $p as a tool.", FALSE, ch, GET_EQ(ch, WEAR_TOOL), NULL, TO_CHAR);
			}
			else {
				perform_wear(ch, obj, WEAR_TOOL);
			}
			return;
		}
	
		switch (GET_OBJ_VNUM(obj)) {
			// this used to be for special-use items, but now we generally use triggers
		
			default: {
				act("$p has no effect with the 'use' command.", FALSE, ch, obj, NULL, TO_CHAR);
				break;
			}
		}
	}
}


ACMD(do_warehouse) {
	bool imm_access = (GET_ACCESS_LEVEL(ch) >= LVL_CIMPL || IS_GRANTED(ch, GRANT_EMPIRES));
	char command[MAX_INPUT_LENGTH];
	
	argument = any_one_arg(argument, command);
	skip_spaces(&argument);
	
	if (IS_NPC(ch) || !ch->desc) {
		msg_to_char(ch, "You cannot use that command.\r\n");
	}
	else if (!imm_access && !GET_LOYALTY(ch)) {
		msg_to_char(ch, "You can only do that if you're in an empire.\r\n");
	}
	else if (is_abbrev(command, "inventory")) {
		warehouse_inventory(ch, argument, SCMD_WAREHOUSE);
	}
	// all other commands require awakeness
	else if (GET_POS(ch) < POS_RESTING || FIGHTING(ch)) {
		msg_to_char(ch, "You can't do that right now.\r\n");
	}
	else if (is_abbrev(command, "identify")) {
		warehouse_identify(ch, argument, SCMD_WAREHOUSE);
	}
	else if (is_abbrev(command, "retrieve")) {
		warehouse_retrieve(ch, argument, SCMD_WAREHOUSE);
	}
	else if (is_abbrev(command, "store")) {
		warehouse_store(ch, argument, SCMD_WAREHOUSE);
	}
	else {
		msg_to_char(ch, "Options: inventory, identify, retrieve, store\r\n");
	}
}


ACMD(do_wear) {	
	char arg1[MAX_INPUT_LENGTH];
	char arg2[MAX_INPUT_LENGTH];
	char_data *iter;
	obj_data *obj, *next_obj;
	int where, dotmode, items_worn = 0;
	bool fighting_me = FALSE;
	
	if (IS_NPC(ch)) {
		msg_to_char(ch, "NPCs may not wear items.\r\n");
		return;
	}
	
	// check combat
	DL_FOREACH2(ROOM_PEOPLE(IN_ROOM(ch)), iter, next_in_room) {
		if (FIGHTING(iter) == ch) {
			fighting_me = TRUE;
			break;
		}
	}
	if (fighting_me || GET_POS(ch) == POS_FIGHTING || FIGHTING(ch)) {
		msg_to_char(ch, "You can't change your gear in combat!\r\n");
		return;
	}

	two_arguments(argument, arg1, arg2);

	if (!*arg1) {
		send_to_char("Wear what?\r\n", ch);
		return;
	}
	dotmode = find_all_dots(arg1);

	if (*arg2 && (dotmode != FIND_INDIV)) {
		send_to_char("You can't specify the same body location for more than one item!\r\n", ch);
		return;
	}
	if (dotmode == FIND_ALL) {
		DL_FOREACH_SAFE2(ch->carrying, obj, next_obj, next_content) {
			if (CAN_SEE_OBJ(ch, obj) && (where = find_eq_pos(ch, obj, 0)) != NO_WEAR && where < NUM_WEARS && !GET_EQ(ch, where) && can_wear_item(ch, obj, FALSE)) {
				items_worn++;
				perform_wear(ch, obj, where);
			}
		}
		if (!items_worn) {
			send_to_char("You don't seem to have anything else you can wear.\r\n", ch);
		}
		else if (FIGHTING(ch)) {
			command_lag(ch, WAIT_COMBAT_ABILITY);
		}
	}
	else if (dotmode == FIND_ALLDOT) {
		if (!*arg1) {
			send_to_char("Wear all of what?\r\n", ch);
			return;
		}
		if (!(obj = get_obj_in_list_vis(ch, arg1, NULL, ch->carrying))) {
			sprintf(buf, "You don't seem to have any %ss you can wear.\r\n", arg1);
			send_to_char(buf, ch);
		}
		else {
			while (obj) {
				next_obj = get_obj_in_list_vis(ch, arg1, NULL, obj->next_content);
				if ((where = find_eq_pos(ch, obj, 0)) != NO_WEAR && where < NUM_WEARS && !GET_EQ(ch, where) && can_wear_item(ch, obj, FALSE)) {
					perform_wear(ch, obj, where);
					++items_worn;
				}
				else if (!GET_EQ(ch, where)) {
					act("You can't wear $p.", FALSE, ch, obj, 0, TO_CHAR);
				}
				obj = next_obj;
			}
			
			if (items_worn && FIGHTING(ch)) {
				command_lag(ch, WAIT_COMBAT_ABILITY);
			}
		}
	}
	else {
		if (!(obj = get_obj_in_list_vis(ch, arg1, NULL, ch->carrying))) {
			sprintf(buf, "You don't seem to have %s %s.\r\n", AN(arg1), arg1);
			send_to_char(buf, ch);
		}
		else {
			if ((where = find_eq_pos(ch, obj, arg2)) >= 0) {
				if (can_wear_item(ch, obj, TRUE)) {
					// sends its own error message if it fails
					perform_wear(ch, obj, where);
					
					if (FIGHTING(ch)) {
						command_lag(ch, WAIT_COMBAT_ABILITY);
					}
				}
			}
			else if (!*arg2) {
				act("You can't wear $p.", FALSE, ch, obj, 0, TO_CHAR);
			}
		}
	}
}


ACMD(do_wield) {
	obj_data *obj;

	one_argument(argument, arg);
	
	if (IS_NPC(ch)) {
		msg_to_char(ch, "NPCs may not wield items.\r\n");
	}
	else if (!*arg) {
		send_to_char("Wield what?\r\n", ch);
	}
	else if (!(obj = get_obj_in_list_vis(ch, arg, NULL, ch->carrying))) {
		if (GET_EQ(ch, WEAR_WIELD) && MATCH_ITEM_NAME(arg, GET_EQ(ch, WEAR_WIELD))) {
			msg_to_char(ch, "It looks like you're already wielding it!\r\n");
		}
		else {
			msg_to_char(ch, "You don't seem to have %s %s.\r\n", AN(arg), arg);
		}
	}
	else if (!CAN_WEAR(obj, ITEM_WEAR_WIELD)) {
		send_to_char("You can't wield that.\r\n", ch);
	}
	else if (!can_wear_item(ch, obj, TRUE)) {
		// sends own error
	}
	else if (OBJ_FLAGGED(obj, OBJ_TWO_HANDED) && GET_EQ(ch, WEAR_HOLD)) {
		msg_to_char(ch, "You can't wield a two-handed weapon while you're holding something in your off-hand.\r\n");
	}
	else {
		if (GET_EQ(ch, WEAR_WIELD)) {
			perform_remove(ch, WEAR_WIELD);
		}
		perform_wear(ch, obj, WEAR_WIELD);
		
		if (FIGHTING(ch)) {
			command_lag(ch, WAIT_COMBAT_ABILITY);
		}
	}
}
