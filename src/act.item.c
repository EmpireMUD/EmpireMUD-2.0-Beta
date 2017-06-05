/* ************************************************************************
*   File: act.item.c                                      EmpireMUD 2.0b5 *
*  Usage: object handling routines -- get/drop and container handling     *
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
*   Drop Helpers
*   Get Helpers
*   Give Helpers
*   Liquid Helpers
*   Scaling
*   Shipping System
*   Trade Command Functions
*   Warehouse Command Functions
*   Commands
*/

// extern variables
extern const char *drinks[];
extern int drink_aff[][3];
extern const char *extra_bits[];
extern const char *mob_move_types[];
extern const struct wear_data_type wear_data[NUM_WEARS];

// extern functions
extern bool can_steal(char_data *ch, empire_data *emp);
extern bool can_wear_item(char_data *ch, obj_data *item, bool send_messages);
void expire_trading_post_item(struct trading_post_data *tpd);
extern char *get_room_name(room_data *room, bool color);
extern struct player_quest *is_on_quest(char_data *ch, any_vnum quest);
void read_vault(empire_data *emp);
void save_trading_post();
void trigger_distrust_from_stealth(char_data *ch, empire_data *emp);

// local protos
ACMD(do_unshare);
room_data *find_docks(empire_data *emp, int island_id);
int get_wear_by_item_wear(bitvector_t item_wear);
void move_ship_to_destination(empire_data *emp, struct shipping_data *shipd, room_data *to_room);
void sail_shipment(empire_data *emp, vehicle_data *boat);
void scale_item_to_level(obj_data *obj, int level);
bool ship_is_empty(vehicle_data *ship);
static void wear_message(char_data *ch, obj_data *obj, int where);

// local stuff
#define drink_OBJ  -1
#define drink_ROOM  1

// ONLY flags to show on identify / warehouse inv
bitvector_t show_obj_flags = OBJ_LIGHT | OBJ_SUPERIOR | OBJ_ENCHANTED | OBJ_JUNK | OBJ_TWO_HANDED | OBJ_BIND_ON_EQUIP | OBJ_BIND_ON_PICKUP | OBJ_HARD_DROP | OBJ_GROUP_DROP | OBJ_GENERIC_DROP;


 //////////////////////////////////////////////////////////////////////////////
//// HELPERS /////////////////////////////////////////////////////////////////

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
*/
INTERACTION_FUNC(combine_obj_interact) {
	char to_char[MAX_STRING_LENGTH], to_room[MAX_STRING_LENGTH];
	struct resource_data *res = NULL;
	obj_data *new_obj;
	
	// how many they need
	add_to_resource_list(&res, RES_OBJECT, GET_OBJ_VNUM(inter_item), interaction->quantity, 0);
	
	if (!has_resources(ch, res, can_use_room(ch, IN_ROOM(ch), MEMBERS_ONLY), TRUE)) {
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
	
	extract_resources(ch, res, can_use_room(ch, IN_ROOM(ch), MEMBERS_ONLY), NULL);
	
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
	load_otrigger(new_obj);
	
	act(to_char, FALSE, ch, new_obj, NULL, TO_CHAR);
	act(to_room, TRUE, ch, new_obj, NULL, TO_ROOM);
	
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
	
	for (search = ROOM_CONTENTS(room); search; search = search->next_content) {
		items_in_room += OBJ_FLAGGED(search, OBJ_LARGE) ? 2 : 1;
	}
	
	return items_in_room;
}


/**
* Returns the targeted position, or else the first place an item can be worn.
*
* @param char_data *ch The player.
* @param obj_data *obj The item trying to wear.
* @param char *arg The typed argument, which may be empty, or a body part.
* @return int A WEAR_x pos, or NO_WEAR.
*/
int find_eq_pos(char_data *ch, obj_data *obj, char *arg) {
	extern const char *wear_keywords[];

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
* Converts a "gotten" object into money, if it's coins. This will extract the
* object if so.
*
* @param char_data *ch The person getting obj.
* @param obj_data *obj The item being picked up.
*/
void get_check_money(char_data *ch, obj_data *obj) {
	int value = GET_COINS_AMOUNT(obj);
	empire_data *emp = real_empire(GET_COINS_EMPIRE_ID(obj));

	// npcs will keep the obj version
	if (IS_NPC(ch) || GET_OBJ_TYPE(obj) != ITEM_COINS || value <= 0) {
		return;
	}

	extract_obj(obj);
	increase_coins(ch, emp, value);

	msg_to_char(ch, "There %s %s.\r\n", (value == 1 ? "was" : "were"), money_amount(emp, value));
}


/**
* Converts an ITEM_WEAR_x into a corresponding WEAR_x flag.
*
* @param bitvector_t item_wear The ITEM_WEAR_x bits.
* @return int A WEAR_x position that matches, or NOWEHRE.
*/
int get_wear_by_item_wear(bitvector_t item_wear) {
	extern int item_wear_to_wear[];
	int pos;
	
	for (pos = 0; item_wear; ++pos, item_wear >>= 1) {
		if (IS_SET(item_wear, BIT(0)) && item_wear_to_wear[pos] != NO_WEAR) {
			return item_wear_to_wear[pos];
		}
	}
	
	return NO_WEAR;
}


/**
* Shows an "identify" of the object -- its stats -- to a player.
*
* @param obj_data *obj The item to identify.
* @param char_data *ch The person to show the data to.
*/
void identify_obj_to_char(obj_data *obj, char_data *ch) {
	extern double get_base_dps(obj_data *weapon);
	extern char *get_vehicle_short_desc(vehicle_data *veh, char_data *to);
	extern double get_weapon_speed(obj_data *weapon);
	extern const char *apply_type_names[];
	extern const char *climate_types[];
	extern const char *drinks[];
	extern const char *affected_bits[];
	extern const char *apply_types[];
	extern const char *armor_types[NUM_ARMOR_TYPES+1];
	extern const char *wear_bits[];

	struct obj_storage_type *store;
	struct custom_message *ocm;
	player_index_data *index;
	struct obj_apply *apply;
	char lbuf[MAX_STRING_LENGTH], part[MAX_STRING_LENGTH], location[MAX_STRING_LENGTH], *temp;
	crop_data *cp;
	bld_data *bld;
	int found;
	double rating;
	bool any;
		
	// sanity / don't bother
	if (!obj || !ch || !ch->desc) {
		return;
	}
	
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
	
	// component info
	*part = '\0';
	if (GET_OBJ_CMP_TYPE(obj) != CMP_NONE) {
		sprintf(part, " (%s)", component_string(GET_OBJ_CMP_TYPE(obj), GET_OBJ_CMP_FLAGS(obj)));
	}
	
	// basic info
	snprintf(lbuf, sizeof(lbuf), "Your analysis of $p%s%s reveals:", part, location);
	act(lbuf, FALSE, ch, obj, NULL, TO_CHAR);
	
	if (GET_OBJ_REQUIRES_QUEST(obj) != NOTHING) {
		msg_to_char(ch, "Quest: %s\r\n", get_quest_name_by_proto(GET_OBJ_REQUIRES_QUEST(obj)));
	}
	
	// if it has any wear bits other than TAKE, show if they can't use it
	if (CAN_WEAR(obj, ~ITEM_WEAR_TAKE)) {
		// the TRUE causes it to send a message if unusable
		msg_to_char(ch, "&r");
		can_wear_item(ch, obj, TRUE);
		msg_to_char(ch, "&0");
	}

	if (obj->storage) {
		msg_to_char(ch, "Storage locations:");
		found = 0;
		for (store = obj->storage; store; store = store->next) {			
			if ((bld = building_proto(store->building_type))) {
				msg_to_char(ch, "%s%s", (found++ > 0 ? ", " : " "), GET_BLD_NAME(bld));
			}
		}		
		msg_to_char(ch, "\r\n");
	}

	// binding section
	if (OBJ_BOUND_TO(obj)) {
		struct obj_binding *bind;		
		msg_to_char(ch, "Bound to:");
		any = FALSE;
		for (bind = OBJ_BOUND_TO(obj); bind; bind = bind->next) {
			msg_to_char(ch, "%s %s", (any ? "," : ""), (index = find_player_index_by_idnum(bind->idnum)) ? index->fullname : "<unknown>");
			any = TRUE;
		}
		msg_to_char(ch, "\r\n");
	}
	
	if (GET_OBJ_CURRENT_SCALE_LEVEL(obj) > 0) {
		msg_to_char(ch, "Level: %d\r\n", GET_OBJ_CURRENT_SCALE_LEVEL(obj));
	}
	
	// only show gear if equippable (has more than ITEM_WEAR_TRADE)
	if ((GET_OBJ_WEAR(obj) & ~ITEM_WEAR_TAKE) != NOBITS) {
		if ((rating = rate_item(obj)) > 0) {
			msg_to_char(ch, "Gear rating: %.1f\r\n", rating);
		}
		
		prettier_sprintbit(GET_OBJ_WEAR(obj) & ~ITEM_WEAR_TAKE, wear_bits, buf);
		msg_to_char(ch, "Can be worn on: %s\r\n", buf);
	}

	
	// flags
	if (GET_OBJ_EXTRA(obj) & show_obj_flags) {
		prettier_sprintbit(GET_OBJ_EXTRA(obj) & show_obj_flags, extra_bits, buf);
		msg_to_char(ch, "It is: %s\r\n", buf);
	}
	
	if (GET_OBJ_AFF_FLAGS(obj)) {
		prettier_sprintbit(GET_OBJ_AFF_FLAGS(obj), affected_bits, buf);
		msg_to_char(ch, "Affects: %s\r\n", buf);
	}
	
	switch (GET_OBJ_TYPE(obj)) {
		case ITEM_POISON: {
			extern const struct poison_data_type poison_data[];
			msg_to_char(ch, "Poison type: %s\r\n", poison_data[GET_POISON_TYPE(obj)].name);
			msg_to_char(ch, "Has %d charges remaining.\r\n", GET_POISON_CHARGES(obj));
			break;
		}
		case ITEM_WEAPON:
			msg_to_char(ch, "Speed: %.2f\r\n", get_weapon_speed(obj));
			msg_to_char(ch, "Damage: %d (%s+%.2f base dps)\r\n", GET_WEAPON_DAMAGE_BONUS(obj), (IS_MAGIC_ATTACK(GET_WEAPON_TYPE(obj)) ? "Intelligence" : "Strength"), get_base_dps(obj));
			msg_to_char(ch, "Damage type is %s.\r\n", attack_hit_info[GET_WEAPON_TYPE(obj)].name);
			break;
		case ITEM_ARMOR:
			msg_to_char(ch, "Armor type: %s\r\n", armor_types[GET_ARMOR_TYPE(obj)]);
			break;
		case ITEM_CONTAINER:
			msg_to_char(ch, "Holds %d items.\r\n", GET_MAX_CONTAINER_CONTENTS(obj));
			break;
		case ITEM_DRINKCON:
			if (GET_DRINK_CONTAINER_CONTENTS(obj) > 0) {
				msg_to_char(ch, "Contains %d units of %s.\r\n", GET_DRINK_CONTAINER_CONTENTS(obj), drinks[GET_DRINK_CONTAINER_TYPE(obj)]);
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
				strcpy(lbuf, get_mob_name_by_proto(GET_CORPSE_NPC_VNUM(obj)));
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
		case ITEM_MISSILE_WEAPON:
			msg_to_char(ch, "Fires at a speed of %.2f for %d damage.\r\n", missile_weapon_speed[GET_MISSILE_WEAPON_SPEED(obj)], GET_MISSILE_WEAPON_DAMAGE(obj));
			break;
		case ITEM_ARROW:
			if (GET_ARROW_QUANTITY(obj) > 0) {
				msg_to_char(ch, "Quantity: %d\r\n", GET_ARROW_QUANTITY(obj));
			}
			if (GET_ARROW_DAMAGE_BONUS(obj)) {
				msg_to_char(ch, "Adds %+d damage.\r\n", GET_ARROW_DAMAGE_BONUS(obj));
			}
			break;
		case ITEM_PACK: {
			msg_to_char(ch, "Adds inventory space: %d\r\n", GET_PACK_CAPACITY(obj));
			break;
		}
		case ITEM_POTION: {
			extern const struct potion_data_type potion_data[];
			msg_to_char(ch, "Potion type: %s (%d)\r\n", potion_data[GET_POTION_TYPE(obj)].name, GET_POTION_SCALE(obj));
			break;
		}
		case ITEM_WEALTH: {
			msg_to_char(ch, "Adds %d wealth when stored.\r\n", GET_WEALTH_VALUE(obj));
			break;
		}
	}
	
	// data that isn't type-based:
	if (OBJ_FLAGGED(obj, OBJ_PLANTABLE) && (cp = crop_proto(GET_OBJ_VAL(obj, VAL_FOOD_CROP_TYPE)))) {
		msg_to_char(ch, "Plants %s (%s).\r\n", GET_CROP_NAME(cp), climate_types[GET_CROP_CLIMATE(cp)]);
	}
	
	if (has_interaction(obj->interactions, INTERACT_COMBINE)) {
		msg_to_char(ch, "It can be combined.\r\n");
	}
	if (has_interaction(obj->interactions, INTERACT_SEPARATE)) {
		msg_to_char(ch, "It can be separated.\r\n");
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
	
	// some custom messages
	LL_FOREACH(obj->custom_msgs, ocm) {
		switch (ocm->type) {
			case OBJ_CUSTOM_LONGDESC: {
				sprintf(lbuf, "Gives long description: %s", ocm->msg);
				act(lbuf, FALSE, ch, NULL, NULL, TO_CHAR);
				break;
			}
			case OBJ_CUSTOM_LONGDESC_FEMALE: {
				if (GET_SEX(ch) == SEX_FEMALE) {
					sprintf(lbuf, "Gives long description: %s", ocm->msg);
					act(lbuf, FALSE, ch, NULL, NULL, TO_CHAR);
				}
				break;
			}
			case OBJ_CUSTOM_LONGDESC_MALE: {
				if (GET_SEX(ch) == SEX_MALE) {
					sprintf(lbuf, "Gives long description: %s", ocm->msg);
					act(lbuf, FALSE, ch, NULL, NULL, TO_CHAR);
				}
				break;
			}
		}
	}
}


/**
* Shows an "identify" of the vehicle.
*
* @param vehicle_data *veh The vehicle to id.
* @param char_data *ch The person to show the data to.
*/
void identify_vehicle_to_char(vehicle_data *veh, char_data *ch) {
	vehicle_data *proto = vehicle_proto(VEH_VNUM(veh));
	
	// basic info
	act("Your analysis of $V reveals:", FALSE, ch, NULL, veh, TO_CHAR);
	
	if (VEH_OWNER(veh)) {
		msg_to_char(ch, "Owner: %s%s\t0\r\n", EMPIRE_BANNER(VEH_OWNER(veh)), EMPIRE_NAME(VEH_OWNER(veh)));
	}
	
	msg_to_char(ch, "Type: %s\r\n", skip_filler(proto ? VEH_SHORT_DESC(proto) : VEH_SHORT_DESC(veh)));
	msg_to_char(ch, "Level: %d\r\n", VEH_SCALE_LEVEL(veh));
}


INTERACTION_FUNC(light_obj_interact) {	
	obj_vnum vnum = interaction->vnum;
	obj_data *new = NULL;
	int num;
	
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
		load_otrigger(new);
	}

	if (interaction->quantity > 1) {
		sprintf(buf1, "It is now $p (x%d).", interaction->quantity);
	}
	else {
		strcpy(buf1, "It is now $p.");
	}
		
	if (new) {
		act(buf1, FALSE, ch, new, NULL, TO_CHAR | TO_ROOM);
	}
	
	if (inter_item && IS_ARROW(inter_item) && IS_ARROW(new)) {
		GET_OBJ_VAL(new, VAL_ARROW_QUANTITY) = GET_ARROW_QUANTITY(inter_item);
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
	for (iter = obj->contains; iter; iter = iter->next_content) {
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
		act(buf, FALSE, ch, obj, NULL, TO_CHAR);
		act("$n exchanges $p for some coins.", TRUE, ch, obj, NULL, TO_ROOM);
		
		increase_coins(ch, emp, amt);
		decrease_empire_coins(emp, emp, amt);
		add_to_empire_storage(emp, GET_ISLAND_ID(IN_ROOM(ch)), GET_OBJ_VNUM(obj), 1);
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
	
	if (!drop_otrigger(obj, ch)) {	// also takes care of obj purging self
		return 0;
	}
	
	// don't let people drop bound items in other people's territory
	if (IN_ROOM(cont) && OBJ_BOUND_TO(obj) && ROOM_OWNER(IN_ROOM(cont)) && ROOM_OWNER(IN_ROOM(cont)) != GET_LOYALTY(ch)) {
		msg_to_char(ch, "You can't put bound items in items here.\r\n");
		return 0;
	}
	if (GET_OBJ_REQUIRES_QUEST(obj) != NOTHING && !IS_NPC(ch) && !IS_IMMORTAL(ch)) {
		act("$p: you can't drop quest items.", FALSE, ch, obj, NULL, TO_CHAR);
		return 0;
	}
	
	if (GET_OBJ_CARRYING_N(cont) + obj_carry_size(obj) > GET_MAX_CONTAINER_CONTENTS(cont)) {
		act("$p won't fit in $P.", FALSE, ch, obj, cont, TO_CHAR);
		return 0;
	}
	else if (OBJ_FLAGGED(obj, OBJ_LARGE) && !OBJ_FLAGGED(cont, OBJ_LARGE) && CAN_WEAR(cont, ITEM_WEAR_TAKE)) {
		act("$p is far too large to fit in $P.", FALSE, ch, obj, cont, TO_CHAR);
		return 1;	// is this correct? I added it because it was implied by the drop-thru here -pc
	}
	else {
		obj_to_obj(obj, cont);

		act("$n puts $p in $P.", TRUE, ch, obj, cont, TO_ROOM);

		act("You put $p in $P.", FALSE, ch, obj, cont, TO_CHAR);

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
		if (has_custom_message(obj, OBJ_CUSTOM_REMOVE_TO_CHAR)) {
			act(get_custom_message(obj, OBJ_CUSTOM_REMOVE_TO_CHAR), FALSE, ch, obj, NULL, TO_CHAR);
		}
		else {
			act("You stop using $p.", FALSE, ch, obj, NULL, TO_CHAR);
		}
		
		// room message
		if (has_custom_message(obj, OBJ_CUSTOM_REMOVE_TO_ROOM)) {
			act(get_custom_message(obj, OBJ_CUSTOM_REMOVE_TO_ROOM), TRUE, ch, obj, NULL, TO_ROOM);
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
	extern const int apply_attribute[];
	extern struct attribute_data_type attributes[NUM_ATTRIBUTES];
	extern const int primary_attributes[];
	
	char buf[MAX_STRING_LENGTH];
	struct obj_apply *apply;
	int iter, type, val;

	/* first, make sure that the wear position is valid. */
	if (!CAN_WEAR(obj, wear_data[where].item_wear)) {
		act("You can't wear $p there.", FALSE, ch, obj, 0, TO_CHAR);
		return;
	}
	
	// position cascade (ring 1/2, etc)
	if (GET_EQ(ch, where) && wear_data[where].cascade_pos != NO_WEAR) {
		where = wear_data[where].cascade_pos;
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
* Interaction func for "separate".
*/
INTERACTION_FUNC(separate_obj_interact) {
	char to_char[MAX_STRING_LENGTH], to_room[MAX_STRING_LENGTH];
	obj_data *new_obj;
	int iter;
		
	snprintf(to_char, sizeof(to_char), "You separate %s into %s (x%d)!", GET_OBJ_SHORT_DESC(inter_item), get_obj_name_by_proto(interaction->vnum), interaction->quantity);
	act(to_char, FALSE, ch, NULL, NULL, TO_CHAR);
	snprintf(to_room, sizeof(to_room), "$n separates %s into %s (x%d)!", GET_OBJ_SHORT_DESC(inter_item), get_obj_name_by_proto(interaction->vnum), interaction->quantity);
	act(to_room, TRUE, ch, NULL, NULL, TO_ROOM);
	
	for (iter = 0; iter < interaction->quantity; ++iter) {
		new_obj = read_object(interaction->vnum, TRUE);
		scale_item_to_level(new_obj, GET_OBJ_CURRENT_SCALE_LEVEL(inter_item));
	
		if (GET_OBJ_TIMER(new_obj) != UNLIMITED && GET_OBJ_TIMER(inter_item) != UNLIMITED) {
			GET_OBJ_TIMER(new_obj) = MIN(GET_OBJ_TIMER(new_obj), GET_OBJ_TIMER(inter_item));
		}
		
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
		load_otrigger(new_obj);
	}
	
	return TRUE;
}


/**
* Sends the wear message when a person puts an item on.
*
* @param char_data *ch The person wearing the item.
* @param obj_data *obj The item he's wearing.
* @param int where Any WEAR_x position.
*/
static void wear_message(char_data *ch, obj_data *obj, int where) {
	// char message
	if (wear_data[where].allow_custom_msgs && has_custom_message(obj, OBJ_CUSTOM_WEAR_TO_CHAR)) {
		act(get_custom_message(obj, OBJ_CUSTOM_WEAR_TO_CHAR), FALSE, ch, obj, NULL, TO_CHAR);
	}
	else {
		act(wear_data[where].wear_msg_to_char, FALSE, ch, obj, NULL, TO_CHAR);
	}
	
	// room message
	if (wear_data[where].allow_custom_msgs && has_custom_message(obj, OBJ_CUSTOM_WEAR_TO_ROOM)) {
		act(get_custom_message(obj, OBJ_CUSTOM_WEAR_TO_ROOM), TRUE, ch, obj, NULL, TO_ROOM);
	}
	else {
		act(wear_data[where].wear_msg_to_room, TRUE, ch, obj, NULL, TO_ROOM);
	}
}


 //////////////////////////////////////////////////////////////////////////////
//// DROP HELPERS ////////////////////////////////////////////////////////////

#define VANISH(mode) (mode == SCMD_JUNK ? "  It vanishes in a puff of smoke!" : "")

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
	
	if (!drop_otrigger(obj, ch)) {
		return 0;
	}

	if ((mode == SCMD_DROP) && !drop_wtrigger(obj, ch)) {
		return 0;
	}
	
	// can't junk stolen items
	if (mode == SCMD_JUNK && IS_STOLEN(obj)) {
		act("$p: You can't junk stolen items!", FALSE, ch, obj, NULL, TO_CHAR);
		return -1;
	}
	
	// don't let people drop bound items in other people's territory
	if (mode != SCMD_JUNK && OBJ_BOUND_TO(obj) && ROOM_OWNER(IN_ROOM(ch)) && ROOM_OWNER(IN_ROOM(ch)) != GET_LOYALTY(ch)) {
		act("$p: You can't drop bound items here.", FALSE, ch, obj, NULL, TO_CHAR);
		return 0;	// don't break a drop-all
	}
	
	if (GET_OBJ_REQUIRES_QUEST(obj) != NOTHING && !IS_NPC(ch) && !IS_IMMORTAL(ch)) {
		act("$p: you can't drop quest items.", FALSE, ch, obj, NULL, TO_CHAR);
		return 0;
	}
	
	// count items
	if (mode != SCMD_JUNK && need_capacity) {
		size = (OBJ_FLAGGED(obj, OBJ_LARGE) ? 2 : 1);
		if ((size + count_objs_in_room(IN_ROOM(ch))) > config_get_int("room_item_limit")) {
			msg_to_char(ch, "You can't drop any more items here.\r\n");
			return -1;
		}
	}

	sprintf(buf, "You %s $p.%s", sname, VANISH(mode));
	act(buf, FALSE, ch, obj, 0, TO_CHAR);
	sprintf(buf, "$n %ss $p.%s", sname, VANISH(mode));
	act(buf, TRUE, ch, obj, 0, TO_ROOM);

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

			if (!drop_wtrigger(obj, ch)) {
				// stays in inventory, which is odd, but better than the alternative (a crash if the script purged the object and we extract it here)
				return;
			}

			obj_to_room(obj, IN_ROOM(ch));
			act("You drop $p.", FALSE, ch, obj, NULL, TO_CHAR);
			act("$n drops $p.", FALSE, ch, obj, NULL, TO_ROOM);
			
			// log dropping items in front of mortals
			if (IS_IMMORTAL(ch)) {
				for (iter = ROOM_PEOPLE(IN_ROOM(ch)); iter; iter = iter->next_in_room) {
					if (iter != ch && !IS_NPC(iter) && !IS_IMMORTAL(iter)) {
						syslog(SYS_GC, GET_ACCESS_LEVEL(ch), TRUE, "ABUSE: %s drops %s with mortal present (%s) at %s", GET_NAME(ch), GET_OBJ_SHORT_DESC(obj), GET_NAME(iter), room_log_identifier(IN_ROOM(ch)));
						break;
					}
				}
			}
		}
		else {
			snprintf(buf, sizeof(buf), "$n drops %s which disappear%s in a puff of smoke!", money_desc(type, amount), (amount == 1 ? "s" : ""));
			act(buf, FALSE, ch, 0, 0, TO_ROOM);

			msg_to_char(ch, "You drop %s which disappear%s in a puff of smoke!\r\n", (amount != 1 ? "some coins" : "a coin"), (amount == 1 ? "s" : ""));
		}
	}
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
		act("$p: item is bound to someone else.", FALSE, ch, obj, NULL, TO_CHAR);
		return TRUE;	// don't break loop
	}
	if (GET_OBJ_REQUIRES_QUEST(obj) != NOTHING && !IS_NPC(ch) && !IS_IMMORTAL(ch) && !is_on_quest(ch, GET_OBJ_REQUIRES_QUEST(obj))) {
		act("$p: you must be on the quest to get this.", FALSE, ch, obj, NULL, TO_CHAR);
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
		act("$p: you can't hold any more items.", FALSE, ch, obj, 0, TO_CHAR);
		return FALSE;
	}
	if (mode == FIND_OBJ_INV || can_take_obj(ch, obj)) {
		if (get_otrigger(obj, ch)) {
			// last-minute scaling: scale to its minimum (adventures will override this on their own)
			if (GET_OBJ_CURRENT_SCALE_LEVEL(obj) < 1) {
				scale_item_to_level(obj, GET_OBJ_MIN_SCALE_LEVEL(obj));
			}
			
			obj_to_char(obj, ch);
			act("You get $p from $P.", FALSE, ch, obj, cont, TO_CHAR);
			act("$n gets $p from $P.", TRUE, ch, obj, cont, TO_ROOM);
			
			if (stealing) {
				if (emp && IS_IMMORTAL(ch)) {
					syslog(SYS_GC, GET_ACCESS_LEVEL(ch), TRUE, "ABUSE: %s stealing %s from %s", GET_NAME(ch), GET_OBJ_SHORT_DESC(obj), EMPIRE_NAME(emp));
				}
				else if (emp && !skill_check(ch, ABIL_STEAL, DIFF_HARD)) {
					log_to_empire(emp, ELOG_HOSTILITY, "Theft at (%d, %d)", X_COORD(IN_ROOM(ch)), Y_COORD(IN_ROOM(ch)));
				}
				
				if (!IS_IMMORTAL(ch)) {
					GET_STOLEN_TIMER(obj) = time(0);
					GET_STOLEN_FROM(obj) = emp ? EMPIRE_VNUM(emp) : NOTHING;
					trigger_distrust_from_stealth(ch, emp);
					gain_ability_exp(ch, ABIL_STEAL, 50);
				}
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
		if (!(obj = get_obj_in_list_vis(ch, arg, cont->contains))) {
			sprintf(buf, "There doesn't seem to be %s %s in $p.", AN(arg), arg);
			act(buf, FALSE, ch, cont, 0, TO_CHAR);
		}
		else {
			obj_data *obj_next;
			while(obj && howmany--) {
				obj_next = obj->next_content;
				if (!perform_get_from_container(ch, obj, cont, mode))
					break;
				obj = get_obj_in_list_vis(ch, arg, obj_next);
			}
		}
	}
	else {
		if (obj_dotmode == FIND_ALLDOT && !*arg) {
			send_to_char("Get all of what?\r\n", ch);
			return;
		}
		for (obj = cont->contains; obj; obj = next_obj) {
			next_obj = obj->next_content;
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
		act("$p: item is bound to someone else.", FALSE, ch, obj, NULL, TO_CHAR);
		return TRUE;	// don't break loop
	}
	if (LAST_OWNER_ID(obj) != idnum && (!GET_LOYALTY(ch) || EMPIRE_VNUM(GET_LOYALTY(ch)) != GET_STOLEN_FROM(obj)) && !can_use_room(ch, IN_ROOM(ch), GUESTS_ALLOWED)) {
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
		act("$p: you must be on the quest to get this.", FALSE, ch, obj, NULL, TO_CHAR);
		return TRUE;
	}
	if (!IS_NPC(ch) && !CAN_CARRY_OBJ(ch, obj)) {
		act("$p: you can't hold any more items.", FALSE, ch, obj, 0, TO_CHAR);
		return FALSE;
	}
	if (can_take_obj(ch, obj) && get_otrigger(obj, ch)) {
		// last-minute scaling: scale to its minimum (adventures will override this on their own)
		if (GET_OBJ_CURRENT_SCALE_LEVEL(obj) < 1) {
			scale_item_to_level(obj, GET_OBJ_MIN_SCALE_LEVEL(obj));
		}
		
		obj_to_char(obj, ch);
		act("You get $p.", FALSE, ch, obj, 0, TO_CHAR);
		act("$n gets $p.", TRUE, ch, obj, 0, TO_ROOM);
					
		if (stealing) {
			if (emp && IS_IMMORTAL(ch)) {
				syslog(SYS_GC, GET_ACCESS_LEVEL(ch), TRUE, "ABUSE: %s stealing %s from %s", GET_NAME(ch), GET_OBJ_SHORT_DESC(obj), EMPIRE_NAME(emp));
			}
			else if (emp && !skill_check(ch, ABIL_STEAL, DIFF_HARD)) {
				log_to_empire(emp, ELOG_HOSTILITY, "Theft at (%d, %d)", X_COORD(IN_ROOM(ch)), Y_COORD(IN_ROOM(ch)));
			}
			
			if (!IS_IMMORTAL(ch)) {
				GET_STOLEN_TIMER(obj) = time(0);
				GET_STOLEN_FROM(obj) = emp ? EMPIRE_VNUM(emp) : NOTHING;
				trigger_distrust_from_stealth(ch, emp);
				gain_ability_exp(ch, ABIL_STEAL, 50);
			}
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

	dotmode = find_all_dots(arg);

	if (dotmode == FIND_INDIV) {
		if (!(obj = get_obj_in_list_vis(ch, arg, ROOM_CONTENTS(IN_ROOM(ch))))) {
			sprintf(buf, "You don't see %s %s here.\r\n", AN(arg), arg);
			send_to_char(buf, ch);
		}
		else {
			obj_data *obj_next;
			while(obj && howmany--) {
				obj_next = obj->next_content;
				if (!perform_get_from_room(ch, obj))
					break;
				obj = get_obj_in_list_vis(ch, arg, obj_next);
			}
		}
	}
	else {
		if (dotmode == FIND_ALLDOT && !*arg) {
			send_to_char("Get all of what?\r\n", ch);
			return;
		}
		for (obj = ROOM_CONTENTS(IN_ROOM(ch)); obj; obj = next_obj) {
			next_obj = obj->next_content;
			if (CAN_SEE_OBJ(ch, obj) && (dotmode == FIND_ALL || isname(arg, GET_OBJ_KEYWORDS(obj)))) {
				found = 1;
				if (!perform_get_from_room(ch, obj))
					break;
			}
		}
		if (!found) {
			if (dotmode == FIND_ALL)
				send_to_char("There doesn't seem to be anything here.\r\n", ch);
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
	else if (!(vict = get_char_vis(ch, arg, FIND_CHAR_ROOM)))
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
		act("$p: item is bound.", FALSE, ch, obj, vict, TO_CHAR);
		return;
	}
	
	if (GET_OBJ_REQUIRES_QUEST(obj) != NOTHING && !IS_NPC(ch) && !IS_IMMORTAL(ch) && !IS_IMMORTAL(vict)) {
		act("$p: you can't give this item away.", FALSE, ch, obj, NULL, TO_CHAR);
		return;
	}
	
	// NPCs usually have no carry limit, but 'give' is an exception because otherwise crazy ensues
	if (!CAN_CARRY_OBJ(vict, obj)) {
		act("$N seems to have $S hands full.", FALSE, ch, 0, vict, TO_CHAR);
		return;
	}

	obj_to_char(obj, vict);
	
	if (IS_IMMORTAL(ch) && !IS_IMMORTAL(vict)) {
		syslog(SYS_GC, GET_ACCESS_LEVEL(ch), TRUE, "ABUSE: %s gives %s to %s", GET_NAME(ch), GET_OBJ_SHORT_DESC(obj), PERS(vict, vict, TRUE));
	}
	
	act("You give $p to $N.", FALSE, ch, obj, vict, TO_CHAR);
	act("$n gives you $p.", FALSE, ch, obj, vict, TO_VICT);
	act("$n gives $p to $N.", TRUE, ch, obj, vict, TO_NOTVICT);
	
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
		
		// give/take various types until done
		remaining = amount;
				
		// try the "other" coins first
		if ((coin = find_coin_entry(GET_PLAYER_COINS(ch), REAL_OTHER_COIN))) {
			this = coin->amount;
			decrease_coins(ch, REAL_OTHER_COIN, MIN(this, remaining));
			increase_coins(vict, REAL_OTHER_COIN, MIN(this, remaining));
			remaining -= MIN(this, remaining);
		}
	
		for (coin = GET_PLAYER_COINS(ch); coin && remaining > 0; coin = coin->next) {
			if (coin->empire_id != OTHER_COIN) {
				this = coin->amount;
				decrease_coins(ch, real_empire(coin->empire_id), MIN(this, remaining));
				increase_coins(vict, real_empire(coin->empire_id), MIN(this, remaining));
				remaining -= MIN(this, remaining);
			}
		}

		snprintf(buf, sizeof(buf), "$n gives you %d in various coin%s.", amount, amount == 1 ? "" : "s");
		act(buf, FALSE, ch, NULL, vict, TO_VICT);
		// to-room/char messages below
	}
	else {
		// a type of coins was specified
		if (!(coin = find_coin_entry(GET_PLAYER_COINS(ch), type)) || coin->amount < amount) {
			msg_to_char(ch, "You don't have %s.\r\n", money_amount(type, amount));
			return;
		}
		
		// simple money transfer
		decrease_coins(ch, type, amount);
		increase_coins(vict, type, amount);
		
		snprintf(buf, sizeof(buf), "$n gives you %s.", money_amount(type, amount));
		act(buf, FALSE, ch, NULL, vict, TO_VICT);
		// to-room/char messages below
	}
	
	if (IS_IMMORTAL(ch) && !IS_IMMORTAL(vict)) {
		syslog(SYS_GC, GET_ACCESS_LEVEL(ch), TRUE, "ABUSE: %s gives %s to %s", GET_NAME(ch), money_desc(type, amount), PERS(vict, vict, TRUE));
	}
	
	// msg to char
	snprintf(buf, sizeof(buf), "You give %s to $N.", money_desc(type, amount));
	act(buf, FALSE, ch, NULL, vict, TO_CHAR);

	// msg to room
	snprintf(buf, sizeof(buf), "$n gives %s to $N.", money_desc(type, amount));
	act(buf, TRUE, ch, NULL, vict, TO_NOTVICT);

	bribe_mtrigger(vict, ch, amount);
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
		case drink_OBJ:
			sprintf(buf, "$n %s from $p.", subcmd == SCMD_SIP ? "sips" : "drinks");
			act(buf, TRUE, ch, obj, 0, TO_ROOM);
			msg_to_char(ch, "You %s the %s.\r\n", subcmd == SCMD_SIP ? "sip" : "drink", drinks[GET_DRINK_CONTAINER_TYPE(obj)]);

			*liq = GET_DRINK_CONTAINER_TYPE(obj);
			break;
		case drink_ROOM:
		default:
			msg_to_char(ch, "You take a drink from the cool water.\r\n");
			act("$n takes a drink from the cool water.", TRUE, ch, 0, 0, TO_ROOM);
			break;
	}

	if (subcmd == SCMD_SIP) {
		msg_to_char(ch, "It tastes like %s.\r\n", drinks[*liq]);
	}
}


void fill_from_room(char_data *ch, obj_data *obj) {
	extern const struct tavern_data_type tavern_data[];
	
	int amount = GET_DRINK_CONTAINER_CAPACITY(obj) - GET_DRINK_CONTAINER_CONTENTS(obj);
	int liquid = LIQ_WATER;
	int timer = UNLIMITED;

	if (room_has_function_and_city_ok(IN_ROOM(ch), FNC_TAVERN)) {
		liquid = tavern_data[get_room_extra_data(IN_ROOM(ch), ROOM_EXTRA_TAVERN_TYPE)].liquid;
	}
	
	if (amount <= 0) {
		send_to_char("There is no room for more.\r\n", ch);
		return;
	}

	if ((GET_DRINK_CONTAINER_CONTENTS(obj) > 0) && GET_DRINK_CONTAINER_TYPE(obj) != liquid) {
		send_to_char("There is already another liquid in it. Pour it out first.\r\n", ch);
		return;
	}
	
	if (HAS_FUNCTION(IN_ROOM(ch), FNC_DRINK_WATER)) {
		if (!IS_COMPLETE(IN_ROOM(ch))) {
			msg_to_char(ch, "You can't fill your water until it's finished being built.\r\n");
			return;
		}
		act("You gently fill $p with water.", FALSE, ch, obj, 0, TO_CHAR);
		act("$n gently fills $p with water.", TRUE, ch, obj, 0, TO_ROOM);
	}
	else if (HAS_FUNCTION(IN_ROOM(ch), FNC_TAVERN)) {
		if (get_room_extra_data(IN_ROOM(ch), ROOM_EXTRA_TAVERN_TYPE) == 0 || !IS_COMPLETE(IN_ROOM(ch))) {
			msg_to_char(ch, "This tavern has nothing on tap.\r\n");
			return;
		}
		else if (get_room_extra_data(IN_ROOM(ch), ROOM_EXTRA_TAVERN_BREWING_TIME) > 0) {
			msg_to_char(ch, "The tavern is brewing up a new batch. Try again later.\r\n");
			return;
		}
		else {
			sprintf(buf, "You fill $p with %s from the tap.", tavern_data[get_room_extra_data(IN_ROOM(ch), ROOM_EXTRA_TAVERN_TYPE)].name);
			act(buf, FALSE, ch, obj, 0, TO_CHAR);
		
			sprintf(buf, "$n fills $p with %s from the tap.", tavern_data[get_room_extra_data(IN_ROOM(ch), ROOM_EXTRA_TAVERN_TYPE)].name);
			act(buf, TRUE, ch, obj, 0, TO_ROOM);
		}
	}
	else {
		act("You gently fill $p with water.", FALSE, ch, obj, 0, TO_CHAR);
		act("$n gently fills $p with water.", TRUE, ch, obj, 0, TO_ROOM);
	}

	/* First same type liq. */
	GET_OBJ_VAL(obj, VAL_DRINK_CONTAINER_TYPE) = liquid;
	GET_OBJ_TIMER(obj) = timer;

	GET_OBJ_VAL(obj, VAL_DRINK_CONTAINER_CONTENTS) = GET_DRINK_CONTAINER_CAPACITY(obj);
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
	extern const double apply_values[];
	void get_scale_constraints(room_data *room, char_data *mob, int *scale_level, int *min, int *max);
	extern double get_weapon_speed(obj_data *weapon);
	extern const bool apply_never_scales[];
	extern const int wear_significance[];
	
	int total_share, bonus, iter, amt;
	int room_lev = 0, room_min = 0, room_max = 0, sig;
	double share, this_share, points_to_give, per_point;
	room_data *room = NULL;
	obj_data *top_obj, *proto;
	struct obj_apply *apply, *next_apply, *temp;
	bitvector_t bits;
	
	// configure this here
	double scale_points_at_100 = config_get_double("scale_points_at_100");
	extern const double obj_flag_scaling_bonus[];	// see constants.c
	extern const double armor_scale_bonus[NUM_ARMOR_TYPES];	// see constants.c
	
	// WEAR_POS_x: modifier based on best wear type
	const double wear_pos_modifier[] = { 0.75, 1.0 };
	
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
		if ((val) > 0) { \
			total_share += (val); \
		} \
		else if ((val) < 0) { \
			bonus += -1 * (val); \
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
		case ITEM_ARROW: {
			SHARE_OR_BONUS(GET_ARROW_DAMAGE_BONUS(obj));
			break;
		}
		case ITEM_PACK: {
			SHARE_OR_BONUS(GET_PACK_CAPACITY(obj));
			break;
		}
		case ITEM_POTION: {
			SHARE_OR_BONUS(GET_POTION_SCALE(obj));
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
				GET_OBJ_VAL(obj, VAL_WEAPON_DAMAGE_BONUS) = amt;
			}
			// leave negatives alone
			break;
		}
		case ITEM_DRINKCON: {
			amt = (int)round(this_share * GET_DRINK_CONTAINER_CAPACITY(obj) * config_get_double("scale_drink_capacity"));
			if (amt > 0) {
				points_to_give -= (this_share * GET_DRINK_CONTAINER_CAPACITY(obj));
			}
			GET_OBJ_VAL(obj, VAL_DRINK_CONTAINER_CAPACITY) = amt;
			GET_OBJ_VAL(obj, VAL_DRINK_CONTAINER_CONTENTS) = amt;
			// negatives aren't even possible here
			break;
		}
		case ITEM_COINS: {
			amt = (int)round(this_share * GET_COINS_AMOUNT(obj) * config_get_double("scale_coin_amount"));
			if (amt > 0) {
				points_to_give -= (this_share * GET_COINS_AMOUNT(obj));
			}
			GET_OBJ_VAL(obj, VAL_COINS_AMOUNT) = amt;
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
				GET_OBJ_VAL(obj, VAL_MISSILE_WEAPON_DAMAGE) = amt;
			}
			// leave negatives alone
			break;
		}
		case ITEM_ARROW: {
			if (GET_ARROW_DAMAGE_BONUS(obj) > 0) {
				amt = (int)round(this_share * GET_ARROW_DAMAGE_BONUS(obj));
				if (amt > 0) {
					points_to_give -= (this_share * GET_ARROW_DAMAGE_BONUS(obj));
				}
				GET_OBJ_VAL(obj, VAL_ARROW_DAMAGE_BONUS) = amt;
			}
			// leave negatives alone
			break;
		}
		case ITEM_PACK: {
			amt = (int)round(this_share * GET_PACK_CAPACITY(obj) * config_get_double("scale_pack_size"));
			if (amt > 0) {
				points_to_give -= (this_share * GET_PACK_CAPACITY(obj));
			}
			GET_OBJ_VAL(obj, VAL_PACK_CAPACITY) = amt;
			// negatives aren't really possible here
			break;
		}
		case ITEM_POTION: {
			// aiming for a scale of 100 at 100 -- and eliminate the wear_pos_modifier since potions are almost always WEAR_TAKE
			amt = (int)round(this_share * GET_POTION_SCALE(obj) * (100.0 / scale_points_at_100) / wear_pos_modifier[wear_significance[ITEM_WEAR_TAKE]]);
			if (amt > 0) {
				points_to_give -= (this_share * GET_POTION_SCALE(obj));
			}
			GET_OBJ_VAL(obj, VAL_POTION_SCALE) = amt;
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
			
			if (apply->modifier > 0) {
				// positive benefit
				amt = round(this_share * apply->modifier * per_point);
				points_to_give -= (this_share * apply->modifier);
				apply->modifier = amt;
			}
			else if (apply->modifier < 0) {
				// penalty: does not cost from points_to_give
				apply->modifier = round(apply->modifier * per_point);
			}
		}
		
		// remove zero-applies
		if (apply->modifier == 0) {
			REMOVE_FROM_LIST(apply, GET_OBJ_APPLIES(obj), next);
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
* @param obj_vnum vnum What item to ship.
*/
void add_shipping_queue(char_data *ch, empire_data *emp, int from_island, int to_island, int number, obj_vnum vnum) {
	struct shipping_data *sd, *temp;
	struct island_info *isle;
	bool done;
	
	if (!emp || from_island == NO_ISLAND || to_island == NO_ISLAND || number < 0 || vnum == NOTHING) {
		msg_to_char(ch, "Unable to set up shipping: invalid inpue.\r\n");
		return;
	}
	
	// try to add to existing order
	done = FALSE;
	for (sd = EMPIRE_SHIPPING_LIST(emp); sd && !done; sd = sd->next) {
		if (sd->vnum != vnum) {
			continue;
		}
		if (sd->from_island != from_island || sd->to_island != to_island) {
			continue;
		}
		if (sd->status != SHIPPING_QUEUED) {
			continue;
		}
		
		// found one to add to!
		sd->amount += number;
		done = TRUE;
		break;
	}
	
	if (!done) {
		// add shipping order
		CREATE(sd, struct shipping_data, 1);
		sd->vnum = vnum;
		sd->amount = number;
		sd->from_island = from_island;
		sd->to_island = to_island;
		sd->status = SHIPPING_QUEUED;
		sd->status_time = time(0);
		sd->ship_origin = NOWHERE;
		sd->shipping_id = -1;
		sd->next = NULL;
		
		// add to end
		if ((temp = EMPIRE_SHIPPING_LIST(emp))) {
			while (temp->next) {
				temp = temp->next;
			}
			temp->next = sd;
		}
		else {
			EMPIRE_SHIPPING_LIST(emp) = sd;
		}
	}
	
	// charge resources
	charge_stored_resource(emp, from_island, vnum, number);
	EMPIRE_NEEDS_SAVE(emp) = TRUE;
	
	// messaging
	isle = get_island(to_island, TRUE);
	msg_to_char(ch, "You set %d '%s' to ship to %s.\r\n", number, skip_filler(get_obj_name_by_proto(vnum)), isle ? get_island_name_for(isle->id, ch) : "an unknown island");
}


/**
* @param struct shipping_data *shipd The shipment.
* @return int Time (in seconds) this shipment takes.
*/
int calculate_shipping_time(struct shipping_data *shipd) {
	struct island_info *from, *to;
	room_data *from_center, *to_center;
	int dist, max, cost;
	
	from = get_island(shipd->from_island, FALSE);
	to = get_island(shipd->to_island, FALSE);
	
	// unable to find islands?
	if (!from || !to) {
		return 0;
	}
	
	from_center = real_room(from->center);
	to_center = real_room(to->center);
	
	// unable to find locations?
	if (!from_center || !to_center) {
		return 0;
	}
	
	dist = compute_distance(from_center, to_center);
	
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
	struct shipping_data *iter, *temp;
	room_data *dock;
	
	// mark all shipments on this ship "delivered" (if we still have a ship)
	if (have_ship) {
		for (iter = shipd; iter; iter = iter->next) {
			if (iter->shipping_id == shipd->shipping_id) {
				iter->status = SHIPPING_DELIVERED;
			}
		}
	}
	
	if ((dock = find_docks(emp, shipd->to_island))) {
		// unload the shipment at the destination
		if (shipd->vnum != NOTHING && shipd->amount > 0) {
			log_to_empire(emp, ELOG_SHIPPING, "%dx %s: shipped to %s", shipd->amount, get_obj_name_by_proto(shipd->vnum), get_island(shipd->to_island, TRUE)->name);
			add_to_empire_storage(emp, shipd->to_island, shipd->vnum, shipd->amount);
		}
		if (have_ship) {
			move_ship_to_destination(emp, shipd, dock);
		}
	}
	else {
		// no docks -- unload the shipment at home
		if (shipd->vnum != NOTHING && shipd->amount > 0) {
			log_to_empire(emp, ELOG_SHIPPING, "%dx %s: returned to %s", shipd->amount, get_obj_name_by_proto(shipd->vnum), get_island(shipd->from_island, TRUE)->name);
			add_to_empire_storage(emp, shipd->from_island, shipd->vnum, shipd->amount);
		}
		if (have_ship) {
			move_ship_to_destination(emp, shipd, real_room(shipd->ship_origin));
		}
	}
	
	// and delete this entry from the list
	REMOVE_FROM_LIST(shipd, EMPIRE_SHIPPING_LIST(emp), next);
	free(shipd);
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
	struct empire_territory_data *ter;
	
	if (!emp || island_id == NO_ISLAND) {
		return NULL;
	}
	
	for (ter = EMPIRE_TERRITORY_LIST(emp); ter; ter = ter->next) {
		if (GET_ISLAND_ID(ter->room) != island_id) {
			continue;
		}
		if (!room_has_function_and_city_ok(ter->room, FNC_DOCKS)) {
			continue;
		}
		if (ROOM_AFF_FLAGGED(ter->room, ROOM_AFF_NO_WORK)) {
			continue;
		}
				
		return ter->room;
	}
	
	return NULL;
}


/**
* Finds a ship to use for a given cargo. Any ship on this island is good.
*
* @param empire_data *emp The empire that is shipping.
* @param struct shipping_data *shipd The shipment.
* @return vehicle_data* A ship, or NULL if none.
*/
vehicle_data *find_free_ship(empire_data *emp, struct shipping_data *shipd) {
	struct empire_territory_data *ter;
	struct shipping_data *iter;
	bool already_used;
	vehicle_data *veh;
	int capacity;
	
	if (!emp || shipd->from_island == NO_ISLAND) {
		return NULL;
	}
	
	for (ter = EMPIRE_TERRITORY_LIST(emp); ter; ter = ter->next) {
		if (GET_ISLAND_ID(ter->room) != shipd->from_island) {
			continue;
		}
		if (!room_has_function_and_city_ok(ter->room, FNC_DOCKS)) {
			continue;
		}
		if (ROOM_AFF_FLAGGED(ter->room, ROOM_AFF_NO_WORK)) {
			continue;
		}
		
		// found docks...
		LL_FOREACH2(ROOM_VEHICLES(ter->room), veh, next_in_room) {
			if (VEH_OWNER(veh) != emp) {
				continue;
			}
			if (!VEH_IS_COMPLETE(veh) || VEH_FLAGGED(veh, VEH_ON_FIRE)) {
				continue;
			}
			if (!VEH_FLAGGED(veh, VEH_SHIPPING) || VEH_CARRYING_N(veh) >= VEH_CAPACITY(veh)) {
				continue;
			}
			if (VEH_INTERIOR_HOME_ROOM(veh) && ROOM_AFF_FLAGGED(VEH_INTERIOR_HOME_ROOM(veh), ROOM_AFF_NO_WORK)) {
				continue;
			}
			
			// calculate capacity to see if it's full, and check if it's already used for a different island
			if (VEH_SHIPPING_ID(veh) != -1) {
				capacity = VEH_CARRYING_N(veh);
				already_used = FALSE;
				for (iter = EMPIRE_SHIPPING_LIST(emp); iter && !already_used; iter = iter->next) {
					if (iter->shipping_id == VEH_SHIPPING_ID(veh)) {
						capacity += iter->amount;
						if (iter->from_island != shipd->from_island || iter->to_island != shipd->to_island) {
							already_used = TRUE;
						}
					}
				}
				if (already_used || capacity >= VEH_CAPACITY(veh)) {
					// ship full or in use
					continue;
				}
			}
			
			// ensure no players on board
			if (!ship_is_empty(veh)) {
				continue;
			}
			
			// looks like we actually found one!
			return veh;
		}
	}
	
	return NULL;
}


/**
* @param empire_data *emp The empire that needs a new shipping id.
* @return int The first free shipping id.
*/
int find_free_shipping_id(empire_data *emp) {
	struct shipping_data *find;
	int id;
	
	// shortcut
	if (EMPIRE_TOP_SHIPPING_ID(emp) < INT_MAX) {
		return ++EMPIRE_TOP_SHIPPING_ID(emp);
	}
	
	// better look it up
	for (id = 0; id < INT_MAX; ++id) {
		LL_SEARCH_SCALAR(EMPIRE_SHIPPING_LIST(emp), find, shipping_id, id);
		if (!find) {
			return id;
		}
	}
	
	// this really shouldn't even be possible
	syslog(SYS_ERROR, 0, TRUE, "SYSERR: find_free_shipping_id found no free id for empire [%d] %s", EMPIRE_VNUM(emp), EMPIRE_NAME(emp));
	return -1;
}


/**
* Finds a ship using its shipping id.
*
* @param empire_data *emp The purported owner of the ship.
* @param int shipping_id The shipping id to find.
* @return vehicle_data* The found ship, or NULL.
*/
vehicle_data *find_ship_by_shipping_id(empire_data *emp, int shipping_id) {
	vehicle_data *veh;
	
	// shortcut
	if (!emp || shipping_id == -1) {
		return NULL;
	}
	
	LL_FOREACH(vehicle_list, veh) {
		if (VEH_OWNER(veh) == emp && VEH_SHIPPING_ID(veh) == shipping_id) {
			return veh;
		}
	}
	
	return NULL;
}


/**
* Finds/creates a holding pen for ships during the shipping system.
*
* @return room_data* The ship holding pen.
*/
room_data *get_ship_pen(void) {
	extern room_data *create_room();

	room_data *room, *iter;
	
	for (iter = interior_room_list; iter; iter = iter->next_interior) {
		if (GET_BUILDING(iter) && GET_BLD_VNUM(GET_BUILDING(iter)) == RTYPE_SHIP_HOLDING_PEN) {
			return iter;
		}
	}
	
	// did not find -- make one
	room = create_room();
	attach_building_to_room(building_proto(RTYPE_SHIP_HOLDING_PEN), room, TRUE);
	
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
	if (VEH_SHIPPING_ID(boat) != -1) {
		for (iter = EMPIRE_SHIPPING_LIST(emp); iter; iter = iter->next) {
			if (iter->shipping_id == VEH_SHIPPING_ID(boat)) {
				capacity += iter->amount;
			}
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
		newd->status = SHIPPING_QUEUED;
		newd->status_time = shipd->status_time;
		newd->ship_origin = NOWHERE;
		newd->shipping_id = -1;
		
		// put right after shipd in the list
		newd->next = shipd->next;
		shipd->next = newd;
		
		// remove overage
		shipd->amount = size_avail - capacity;
		*full = TRUE;
	}
	else {
		*full = ((shipd->amount + capacity) >= size_avail);
	}
	
	// mark it as attached to this boat
	if (VEH_SHIPPING_ID(boat) == -1) {
		VEH_SHIPPING_ID(boat) = find_free_shipping_id(emp);
	}
	shipd->shipping_id = VEH_SHIPPING_ID(boat);
}


/**
* This function attempts to find the ship for a particular shipment, and send
* it to the room of your choice (may be the destination OR origin). The
* shipment's ship homeroom will be set to NOWHERE, to avoid re-moving ships.
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
		act(buf, FALSE, ROOM_PEOPLE(IN_ROOM(boat)), NULL, boat, TO_CHAR | TO_ROOM);
	}
	vehicle_to_room(boat, to_room);
	if (ROOM_PEOPLE(IN_ROOM(boat))) {
		snprintf(buf, sizeof(buf), "$V %s in.", mob_move_types[VEH_MOVE_TYPE(boat)]);
		act(buf, FALSE, ROOM_PEOPLE(IN_ROOM(boat)), NULL, boat, TO_CHAR | TO_ROOM);
	}
	
	VEH_SHIPPING_ID(boat) = -1;
	
	// remove the ship homeroom from all shipments that were on this ship (including this one)
	old = shipd->shipping_id;
	for (iter = EMPIRE_SHIPPING_LIST(emp); iter; iter = iter->next) {
		if (iter->shipping_id == old) {
			iter->shipping_id = -1;
		}
	}
}


/**
* Run one shipping cycle for an empire. This runs every 12 game hours -- at
* 7am and 7pm.
*
* @param empire_data *emp The empire to run.
*/
void process_shipping_one(empire_data *emp) {
	struct shipping_data *shipd, *next_shipd;
	vehicle_data *last_ship = NULL;
	bool full, changed = FALSE;
	int last_from = NO_ISLAND, last_to = NO_ISLAND;
	
	for (shipd = EMPIRE_SHIPPING_LIST(emp); shipd; shipd = next_shipd) {
		next_shipd = shipd->next;
		
		switch (shipd->status) {
			case SHIPPING_QUEUED: {
				if (!last_ship || last_from != shipd->from_island || last_to != shipd->to_island) {
					// attempt to find a(nother) ship
					last_ship = find_free_ship(emp, shipd);
					last_from = shipd->from_island;
					last_to = shipd->to_island;
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
	for (shipd = EMPIRE_SHIPPING_LIST(emp); shipd; shipd = next_shipd) {
		next_shipd = shipd->next;
		
		if (shipd->status == SHIPPING_QUEUED && shipd->shipping_id != -1) {
			vehicle_data *boat = find_ship_by_shipping_id(emp, shipd->shipping_id);
			if (boat) {
				sail_shipment(emp, boat);
			}
		}
	}
	
	if (changed) {
		EMPIRE_NEEDS_SAVE(emp) = TRUE;
	}
}


/**
* Runs a shipping cycle for all empires. This runs every 12 game hours -- at
* 7am and 7pm.
*/
void process_shipping(void) {
	empire_data *emp, *next_emp;
	
	HASH_ITER(hh, empire_table, emp, next_emp) {
		if (EMPIRE_SHIPPING_LIST(emp)) {
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
	for (iter = EMPIRE_SHIPPING_LIST(emp); iter; iter = iter->next) {
		if (iter->shipping_id == VEH_SHIPPING_ID(boat)) {
			iter->status = SHIPPING_EN_ROUTE;
			iter->status_time = time(0);
			iter->ship_origin = GET_ROOM_VNUM(IN_ROOM(boat));
		}
	}
	
	if (ROOM_PEOPLE(IN_ROOM(boat))) {
		snprintf(buf, sizeof(buf), "$V %s away.", mob_move_types[VEH_MOVE_TYPE(boat)]);
		act(buf, FALSE, ROOM_PEOPLE(IN_ROOM(boat)), NULL, boat, TO_CHAR | TO_ROOM);
	}
	vehicle_to_room(boat, get_ship_pen());
	if (ROOM_PEOPLE(IN_ROOM(boat))) {
		snprintf(buf, sizeof(buf), "$V %s in.", mob_move_types[VEH_MOVE_TYPE(boat)]);
		act(buf, FALSE, ROOM_PEOPLE(IN_ROOM(boat)), NULL, boat, TO_CHAR | TO_ROOM);
	}
}


/**
* Determines if a ship has any players in it.
*
* @param vehicle_data *ship The ship to check.
* @return bool TRUE if the ship is empty, FALSE if it has players inside.
*/
bool ship_is_empty(vehicle_data *ship) {
	struct vehicle_room_list *vrl;
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
		LL_FOREACH2(ROOM_PEOPLE(vrl->room), ch, next_in_room) {
			if (!IS_NPC(ch)) {
				// not empty!
				return FALSE;
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
	
	for (tpd = trading_list; tpd; tpd = tpd->next) {
		// disqualifiers -- this should be the same as trade_cancel because players use this function to find numbers for that one
		if (tpd->player != GET_IDNUM(ch)) {
			continue;
		}
		// if it's just collectable coins, don't show
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
	
	for (tpd = trading_list; tpd; tpd = tpd->next) {
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
	struct trading_post_data *tpd;
	empire_data *coin_emp = NULL;
	char buf[MAX_STRING_LENGTH], *ptr1, *ptr2;
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
		
	for (tpd = trading_list; tpd; tpd = tpd->next) {
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
		charge_coins(ch, coin_emp, tpd->buy_cost, NULL);
		REMOVE_BIT(tpd->state, TPD_FOR_SALE);
		SET_BIT(tpd->state, TPD_BOUGHT | TPD_COINS_PENDING);
		
		// notify
		if ((seller = is_playing(tpd->player))) {
			msg_to_char(seller, "Your trading post item '%s' has sold for %s.\r\n", GET_OBJ_SHORT_DESC(tpd->obj), money_amount(coin_emp, tpd->buy_cost));
		}
		sprintf(buf, "You buy $p for %s.", money_amount(coin_emp, tpd->buy_cost));
		act(buf, FALSE, ch, tpd->obj, NULL, TO_CHAR);
		act("$n buys $p.", FALSE, ch, tpd->obj, NULL, TO_ROOM | TO_SPAMMY);
			
		// obj
		add_to_object_list(tpd->obj);
		obj_to_char(tpd->obj, ch);
		tpd->obj = NULL;
		
		// cleanup
		SAVE_CHAR(ch);
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
	
	if (num < 1) {
		msg_to_char(ch, "Usage: trade cancel [number] <keyword>\r\n");
		return;
	}
	
	for (tpd = trading_list; tpd; tpd = tpd->next) {
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
	struct trading_post_data *tpd;
	int to_collect = 0;
	bool full = FALSE, any = FALSE;
	
	double trading_post_fee = config_get_double("trading_post_fee");
	
	for (tpd = trading_list; tpd; tpd = tpd->next) {
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
		SAVE_CHAR(ch);
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
		
	for (tpd = trading_list; tpd; tpd = tpd->next) {
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
		identify_obj_to_char(tpd->obj, ch);
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
	struct trading_post_data *tpd, *end;
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
	else if (!(obj = get_obj_in_list_vis(ch, itemarg, ch->carrying))) {
		msg_to_char(ch, "You don't seem to have a %s to trade.", itemarg);
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
		
		charge_coins(ch, GET_LOYALTY(ch), post_cost, NULL);
		
		CREATE(tpd, struct trading_post_data, 1);
		tpd->next = NULL;
		
		// put at end of list
		if ((end = trading_list)) {
			while (end->next) {
				end = end->next;
			}
		}
		if (end) {
			end->next = tpd;
		}
		else {
			trading_list = tpd;
		}
		
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
		
		SAVE_CHAR(ch);
		save_trading_post();
	}
}


 //////////////////////////////////////////////////////////////////////////////
//// WAREHOUSE COMMAND FUNCTIONS /////////////////////////////////////////////

/**
* Sub-processor for do_warehouse: inventory.
*
* @param char_data *ch The player.
* @param char *argument The rest of the argument after "list"
*/
void warehouse_inventory(char_data *ch, char *argument) {
	extern const char *unique_storage_flags[];

	char arg[MAX_INPUT_LENGTH], output[MAX_STRING_LENGTH*2], line[MAX_STRING_LENGTH], part[256], flags[256], quantity[256], level[256], objflags[256], *tmp;
	bool imm_access = (GET_ACCESS_LEVEL(ch) >= LVL_CIMPL || IS_GRANTED(ch, GRANT_EMPIRES));
	struct empire_unique_storage *iter;
	empire_data *emp = GET_LOYALTY(ch);
	int island = GET_ISLAND_ID(IN_ROOM(ch));
	int num, size;
	
	if (imm_access) {
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
	
	if (!emp) {
		msg_to_char(ch, "You must be in an empire to do that.\r\n");
		return;
	}

	if (*argument) {
		snprintf(part, sizeof(part), "\"%s\"", argument);
	}
	else {
		snprintf(part, sizeof(part), "Unique");
	}
	
	size = snprintf(output, sizeof(output), "%s items stored in %s%s&0:\r\n", part, EMPIRE_BANNER(emp), EMPIRE_NAME(emp));
	num = 0;
	
	for (iter = EMPIRE_UNIQUE_STORAGE(emp); iter; iter = iter->next) {
		if (!imm_access && iter->island != island) {
			continue;
		}
		if (*argument && !multi_isname(argument, GET_OBJ_KEYWORDS(iter->obj))) {
			continue;
		}
		
		if (GET_OBJ_CURRENT_SCALE_LEVEL(iter->obj) > 0) {
			snprintf(level, sizeof(level), " (level %d)", GET_OBJ_CURRENT_SCALE_LEVEL(iter->obj));
		}
		else {
			*level = '\0';
		}
		
		if (iter->flags) {
			prettier_sprintbit(iter->flags, unique_storage_flags, flags);
			snprintf(part, sizeof(part), " [%s]", flags);
		}
		else {
			*part = '\0';
		}
		
		if (GET_OBJ_EXTRA(iter->obj) & show_obj_flags) {
			prettier_sprintbit(GET_OBJ_EXTRA(iter->obj) & show_obj_flags, extra_bits, flags);
			snprintf(objflags, sizeof(objflags), " (%s)", flags);
		}
		else {
			*objflags = '\0';
		}
		
		if (iter->amount != 1) {
			snprintf(quantity, sizeof(quantity), "(%d) ", iter->amount);
		}
		else {
			*quantity = '\0';
		}
		
		// build line
		snprintf(line, sizeof(line), "%2d. %s%s%s%s%s\r\n", ++num, quantity, GET_OBJ_SHORT_DESC(iter->obj), level, objflags, part);
		
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
*
* @param char_data *ch The player.
* @param char *argument The rest of the argument after "identify"
*/
void warehouse_identify(char_data *ch, char *argument) {
	bool imm_access = (GET_ACCESS_LEVEL(ch) >= LVL_CIMPL || IS_GRANTED(ch, GRANT_EMPIRES));
	empire_data *room_emp = ROOM_OWNER(IN_ROOM(ch));
	struct empire_unique_storage *iter, *next_iter;
	int island = GET_ISLAND_ID(IN_ROOM(ch));
	int number;
	
	if (!*argument) {
		msg_to_char(ch, "Identify what?\r\n");
		return;
	}
	
	// access permission
	if (!imm_access && (!room_has_function_and_city_ok(IN_ROOM(ch), FNC_WAREHOUSE | FNC_VAULT) || !IS_COMPLETE(IN_ROOM(ch)))) {
		msg_to_char(ch, "You can't do that here.\r\n");
		return;
	}
	if (!imm_access && (!can_use_room(ch, IN_ROOM(ch), GUESTS_ALLOWED) || (room_emp && GET_LOYALTY(ch) != room_emp && !has_relationship(GET_LOYALTY(ch), room_emp, DIPL_TRADE)))) {
		msg_to_char(ch, "You don't have permission to do that here.\r\n");
		return;
	}
	
	// list position
	number = get_number(&argument);
	
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
	for (iter = EMPIRE_UNIQUE_STORAGE(GET_LOYALTY(ch)); iter; iter = next_iter) {
		next_iter = iter->next;
		
		if (!imm_access && iter->island != island) {
			continue;
		}
		if (!multi_isname(argument, GET_OBJ_KEYWORDS(iter->obj))) {
			continue;
		}
		
		// #. check
		if (--number > 0) {
			continue;
		}
		
		// FOUND:
		identify_obj_to_char(iter->obj, ch);
		return;
	}
	
	// nothing?
	msg_to_char(ch, "You don't seem to be able to identify anything like that.\r\n");
}


/**
* Sub-processor for do_warehouse: retrieve.
*
* @param char_data *ch The player.
* @param char *argument The rest of the argument after "retrieve"
*/
void warehouse_retrieve(char_data *ch, char *argument) {
	bool imm_access = (GET_ACCESS_LEVEL(ch) >= LVL_CIMPL || IS_GRANTED(ch, GRANT_EMPIRES));
	empire_data *room_emp = ROOM_OWNER(IN_ROOM(ch));
	struct empire_unique_storage *iter, *next_iter;
	int island = GET_ISLAND_ID(IN_ROOM(ch));
	char junk[MAX_INPUT_LENGTH], *tmp;
	obj_data *obj = NULL;
	int number, amt = 1;
	bool all = FALSE, any = FALSE, one = FALSE, done = FALSE;
	
	if (!*argument) {
		msg_to_char(ch, "Retrieve what?\r\n");
		return;
	}
	
	// access permission
	if (!imm_access && (!room_has_function_and_city_ok(IN_ROOM(ch), FNC_WAREHOUSE | FNC_VAULT) || !IS_COMPLETE(IN_ROOM(ch)))) {
		msg_to_char(ch, "You can't do that here.\r\n");
		return;
	}
	if (!imm_access && (!can_use_room(ch, IN_ROOM(ch), GUESTS_ALLOWED) || (room_emp && GET_LOYALTY(ch) != room_emp && !has_relationship(GET_LOYALTY(ch), room_emp, DIPL_TRADE)))) {
		msg_to_char(ch, "You don't have permission to do that here.\r\n");
		return;
	}
	if (!imm_access && room_has_function_and_city_ok(IN_ROOM(ch), FNC_VAULT) && !has_permission(ch, PRIV_WITHDRAW)) {
		msg_to_char(ch, "You don't have permission to withdraw items here.\r\n");
		return;
	}
	if (!imm_access && room_has_function_and_city_ok(IN_ROOM(ch), FNC_WAREHOUSE) && !has_permission(ch, PRIV_WAREHOUSE)) {
		msg_to_char(ch, "You don't have permission to withdraw items here.\r\n");
		return;
	}
	
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
	
	// for later
	if (isdigit(*argument)) {
		one = TRUE;
	}
	
	// list position
	number = get_number(&argument);
	
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
	for (iter = EMPIRE_UNIQUE_STORAGE(GET_LOYALTY(ch)); iter && !done; iter = next_iter) {
		next_iter = iter->next;
		
		if (!imm_access && iter->island != island) {
			continue;
		}
		if (!multi_isname(argument, GET_OBJ_KEYWORDS(iter->obj))) {
			continue;
		}
		
		// #. check
		if (--number > 0) {
			continue;
		}
		
		// vault permission was pre-validated, but they have to be in one to use it
		if (IS_SET(iter->flags, EUS_VAULT) && !imm_access && !room_has_function_and_city_ok(IN_ROOM(ch), FNC_VAULT)) {
			msg_to_char(ch, "You need to be in a vault to retrieve %s.\r\n", GET_OBJ_SHORT_DESC(iter->obj));
			return;
		}
		
		// load the actual objs
		while (!done && iter->amount > 0 && (all || amt-- > 0)) {
			if (iter->obj && !CAN_CARRY_OBJ(ch, iter->obj)) {
				msg_to_char(ch, "Your arms are full.\r\n");
				done = TRUE;
				break;
			}
		
			if (iter->amount > 0) {
				obj = copy_warehouse_obj(iter->obj);
			}
			else {
				// last one
				obj = iter->obj;
				iter->obj = NULL;
				add_to_object_list(obj);	// put back in object list
				obj->script_id = 0;	// clear this out so it's guaranteed to get a new one
			}
			
			if (obj) {
				iter->amount -= 1;
				any = TRUE;
				obj_to_char(obj, ch);	// inventory size pre-checked
				act("You retrieve $p.", FALSE, ch, obj, NULL, TO_CHAR);
				act("$n retrieves $p.", FALSE, ch, obj, NULL, TO_ROOM);
				load_otrigger(obj);
			}
		}
		
		// remove the entry
		if (iter->amount <= 0 || !iter->obj) {
			remove_eus_entry(iter, GET_LOYALTY(ch));
		}
		
		// only ever retrieve entry of thing, unless they used an all, unless unless they requested as specific one.
		if (one || !all) {
			break;
		}
	}
	
	if (!any) {
		msg_to_char(ch, "You don't seem to be able to retrieve anything like that.\r\n");
	}
	else {
		SAVE_CHAR(ch);
		EMPIRE_NEEDS_SAVE(GET_LOYALTY(ch)) = TRUE;
	}
}


/**
* Sub-processor for do_warehouse: store.
*
* @param char_data *ch The player.
* @param char *argument The rest of the argument after "store"
*/
void warehouse_store(char_data *ch, char *argument) {
	bool imm_access = (GET_ACCESS_LEVEL(ch) >= LVL_CIMPL || IS_GRANTED(ch, GRANT_EMPIRES));
	empire_data *room_emp = ROOM_OWNER(IN_ROOM(ch));
	char numarg[MAX_INPUT_LENGTH], *tmp;
	obj_data *obj, *next_obj;
	int total = 1, done = 0, dotmode;
	bool full = FALSE;
	
	if (!*argument) {
		msg_to_char(ch, "Store what?\r\n");
		return;
	}
	
	// access permission
	if (!imm_access && (!room_has_function_and_city_ok(IN_ROOM(ch), FNC_WAREHOUSE | FNC_VAULT) || !IS_COMPLETE(IN_ROOM(ch)))) {
		msg_to_char(ch, "You can't do that here.\r\n");
		return;
	}
	if (!imm_access && (!can_use_room(ch, IN_ROOM(ch), GUESTS_ALLOWED) || (room_emp && GET_LOYALTY(ch) != room_emp && !has_relationship(GET_LOYALTY(ch), room_emp, DIPL_TRADE)))) {
		msg_to_char(ch, "You don't have permission to do that here.\r\n");
		return;
	}
	if (!imm_access && room_has_function_and_city_ok(IN_ROOM(ch), FNC_VAULT) && !has_permission(ch, PRIV_WITHDRAW)) {
		msg_to_char(ch, "You don't have permission to store items here.\r\n");
		return;
	}
	
	// possible #
	tmp = any_one_arg(argument, numarg);
	if (*numarg && is_number(numarg)) {
		total = atoi(numarg);
		if (total < 1) {
			msg_to_char(ch, "You have to store at least 1.\r\n");
			return;
		}
		argument = tmp;
	}
	else if (*argument && *numarg && !str_cmp(numarg, "all")) {
		total = CAN_CARRY_N(ch) + 1;
		argument = tmp;
	}
	
	// argument may have moved for numarg
	skip_spaces(&argument);
	
	dotmode = find_all_dots(argument);
	
	if (dotmode == FIND_ALL) {
		if (!ch->carrying) {
			msg_to_char(ch, "You don't seem to be carrying anything.\r\n");
			return;
		}
		for (obj = ch->carrying; obj && !full; obj = next_obj) {
			next_obj = obj->next_content;
			
			// bound objects never store, nor can torches
			if (!OBJ_FLAGGED(obj, OBJ_KEEP) && UNIQUE_OBJ_CAN_STORE(obj)) {
				// may extract obj
				store_unique_item(ch, obj, GET_LOYALTY(ch), IN_ROOM(ch), &full);
				if (!full) {
					++done;
				}
			}
		}
		if (!done) {
			if (full) {
				msg_to_char(ch, "It's full.\r\n");
			}
			else {
				msg_to_char(ch, "You don't have anything that can be stored.\r\n");
			}
		}
	}
	else {
		if (!*argument) {
			msg_to_char(ch, "What do you want to store?\r\n");
			return;
		}
		if (!(obj = get_obj_in_list_vis(ch, argument, ch->carrying))) {
			msg_to_char(ch, "You don't seem to have any %ss.\r\n", argument);
			return;
		}

		while (obj && (dotmode == FIND_ALLDOT || done < total)) {
			next_obj = get_obj_in_list_vis(ch, argument, obj->next_content);

			// bound objects never store
			if ((!OBJ_FLAGGED(obj, OBJ_KEEP) || (total == 1 && dotmode != FIND_ALLDOT)) && UNIQUE_OBJ_CAN_STORE(obj)) {
				// may extract obj
				store_unique_item(ch, obj, GET_LOYALTY(ch), IN_ROOM(ch), &full);
				if (!full) {
					++done;
				}
			}
			obj = next_obj;
		}
		if (!done) {
			if (full) {
				msg_to_char(ch, "It's full.\r\n");
			}
			else {
				msg_to_char(ch, "You can't store that here!\r\n");
			}
		}
	}

	if (done) {
		SAVE_CHAR(ch);
		EMPIRE_NEEDS_SAVE(GET_LOYALTY(ch)) = TRUE;
	}
}


 //////////////////////////////////////////////////////////////////////////////
//// COMMANDS ////////////////////////////////////////////////////////////////

ACMD(do_combine) {
	char arg[MAX_INPUT_LENGTH];
	obj_data *obj;
	
	one_argument(argument, arg);
	
	if (!*arg) {
		msg_to_char(ch, "Combine what?\r\n");
	}
	else if (!(obj = get_obj_in_list_vis(ch, arg, ch->carrying))) {
		msg_to_char(ch, "You don't have %s %s.\r\n", AN(arg), arg);
	}
	else if (!has_interaction(obj->interactions, INTERACT_COMBINE)) {
		msg_to_char(ch, "You can't combine that!\r\n");
	}
	else {
		// will extract no matter what happens here
		if (!run_interactions(ch, obj->interactions, INTERACT_COMBINE, IN_ROOM(ch), NULL, obj, combine_obj_interact)) {
			act("You fail to combine $p.", FALSE, ch, obj, NULL, TO_CHAR);
		}
		command_lag(ch, WAIT_OTHER);
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
}


ACMD(do_drink) {	
	obj_data *obj = NULL;
	int amount, i, liquid;
	double thirst_amt, hunger_amt;
	int type = drink_OBJ;
	room_data *to_room;

	one_argument(argument, arg);

	if (REAL_NPC(ch))		/* Cannot use GET_COND() on mobs. */
		return;

	if (!*arg) {
		if (ROOM_SECT_FLAGGED(IN_ROOM(ch), SECTF_DRINK))
			type = drink_ROOM;
		else if (room_has_function_and_city_ok(IN_ROOM(ch), FNC_DRINK_WATER)) {
			if (!can_drink_from_room(ch, (type = drink_ROOM))) {
				return;
			}
		}
		else if (find_flagged_sect_within_distance_from_char(ch, SECTF_DRINK, NOBITS, 1)) {
			type = drink_ROOM;
		}
		else {
			msg_to_char(ch, "Drink from what?\r\n");
			return;
		}
	}

	if (type == NOTHING && !(obj = get_obj_in_list_vis(ch, arg, ch->carrying)) && (!can_use_room(ch, IN_ROOM(ch), MEMBERS_AND_ALLIES) || !(obj = get_obj_in_list_vis(ch, arg, ROOM_CONTENTS(IN_ROOM(ch)))))) {
		if (room_has_function_and_city_ok(IN_ROOM(ch), FNC_DRINK_WATER) && (is_abbrev(arg, "water") || isname(arg, get_room_name(IN_ROOM(ch), FALSE)))) {
			if (!can_drink_from_room(ch, (type = drink_ROOM))) {
				return;
			}
		}
		if (is_abbrev(arg, "river") || is_abbrev(arg, "water")) {
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
		send_to_char("You can't drink from that!\r\n", ch);
		return;
	}

	/* The pig is drunk */
	if (GET_COND(ch, DRUNK) > 360 && GET_COND(ch, THIRST) < 345) {
		if (type >= 0)
			msg_to_char(ch, "You're so inebriated that you don't dare get that close to the water.\r\n");
		else {
			msg_to_char(ch, "You can't seem to get close enough to your mouth.\r\n");
			act("$n tries to drink but misses $s mouth!", TRUE, ch, 0, 0, TO_ROOM);
		}
		return;
	}

	/*
	if (GET_COND(ch, THIRST) < 10 && GET_COND(ch, THIRST) != UNLIMITED) {
		msg_to_char(ch, "You're too full to drink anymore!\r\n");
		return;
	}

	if (GET_COND(ch, FULL) < 75 && GET_COND(ch, FULL) != UNLIMITED && GET_COND(ch, THIRST) < 345 && GET_COND(ch, THIRST) != UNLIMITED) {
		send_to_char("Your stomach can't contain anymore!\r\n", ch);
		return;
	}
	*/

	if (obj && GET_DRINK_CONTAINER_CONTENTS(obj) <= 0) {
		send_to_char("It's empty.\r\n", ch);
		return;
	}

	/* check trigger */
	if (obj && !consume_otrigger(obj, ch, OCMD_DRINK)) {
		return;
	}

	// this both sends the message & returns the amount for some reason
	drink_message(ch, obj, type, subcmd, &liquid);
	
	// no modifiers on sip
	if (subcmd == SCMD_SIP) {
		return;
	}
	
	// "amount" will be how many gulps to take from the CAPACITY

	if (liquid == LIQ_BLOOD) {
		// drinking blood?
		amount = GET_MAX_BLOOD(ch) - GET_BLOOD(ch);
	}
	else if ((drink_aff[liquid][THIRST] > 0 && GET_COND(ch, THIRST) == UNLIMITED) || (drink_aff[liquid][FULL] > 0 && GET_COND(ch, FULL) == UNLIMITED) || (drink_aff[liquid][DRUNK] > 0 && GET_COND(ch, DRUNK) == UNLIMITED)) {
		// if theirs is unlimited
		amount = 1;
	}
	else {
		// how many hours of thirst I have, divided by how much thirst it gives per hour
		thirst_amt = GET_COND(ch, THIRST) == UNLIMITED ? 1 : (((double) GET_COND(ch, THIRST) / REAL_UPDATES_PER_MUD_HOUR) / (double) drink_aff[liquid][THIRST]);
		hunger_amt = GET_COND(ch, FULL) == UNLIMITED ? 1 : (((double) GET_COND(ch, FULL) / REAL_UPDATES_PER_MUD_HOUR) / (double) drink_aff[liquid][FULL]);
		
		// poor man's round-up
		if (((int)(thirst_amt * 10)) % 10 > 0) {
			thirst_amt += 1;
		}
		// poor man's round-up
		if (((int)(hunger_amt * 10)) % 10 > 0) {
			hunger_amt += 1;
		}
	
		// whichever is more
		amount = MAX((int)thirst_amt, (int)hunger_amt);
	
		// if it causes drunkenness, minimum of 1
		if (drink_aff[liquid][DRUNK] > 0) {
			amount = MAX(1, amount);
		}
	}
	
	// amount is now the number of gulps to take to fill the player
	
	if (obj) {
		// bound by contents available
		amount = MIN(GET_DRINK_CONTAINER_CONTENTS(obj), amount);
	}
	
	// apply effects
	if (liquid == LIQ_BLOOD) {
		GET_BLOOD(ch) = MIN(GET_MAX_BLOOD(ch), GET_BLOOD(ch) + amount);
	}
	
	// -1 to remove condition, amount = number of gulps
	gain_condition(ch, THIRST, -1 * drink_aff[liquid][THIRST] * REAL_UPDATES_PER_MUD_HOUR * amount);
	gain_condition(ch, FULL, -1 * drink_aff[liquid][FULL] * REAL_UPDATES_PER_MUD_HOUR * amount);
	// drunk goes positive instead of negative
	gain_condition(ch, DRUNK, 1 * drink_aff[liquid][DRUNK] * REAL_UPDATES_PER_MUD_HOUR * amount);

	// messages based on what changed
	if (GET_COND(ch, DRUNK) > 150 && drink_aff[liquid][DRUNK] != 0) {
		send_to_char("You feel drunk.\r\n", ch);
	}
	if (GET_COND(ch, THIRST) < 75 && drink_aff[liquid][THIRST] != 0) {
		send_to_char("You don't feel thirsty any more.\r\n", ch);
	}
	if (GET_COND(ch, FULL) < 75 && drink_aff[liquid][FULL] != 0) {
		send_to_char("You are full.\r\n", ch);
	}

	if (obj) {
		GET_OBJ_VAL(obj, VAL_DRINK_CONTAINER_CONTENTS) -= amount;
		if (GET_DRINK_CONTAINER_CONTENTS(obj) <= 0) {	/* The last bit */
			GET_OBJ_VAL(obj, VAL_DRINK_CONTAINER_TYPE) = 0;
			GET_OBJ_TIMER(obj) = UNLIMITED;
		}
		
		// ensure binding
		if (!IS_NPC(ch) && OBJ_FLAGGED(obj, OBJ_BIND_FLAGS)) {
			bind_obj_to_player(obj, ch);
			reduce_obj_binding(obj, ch);
		}
	}
}


ACMD(do_drop) {	
	obj_data *obj, *next_obj;
	byte mode = SCMD_DROP;
	int this, dotmode, amount = 0, multi, coin_amt;
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
	if ((argpos = find_coin_arg(argument, &coin_emp, &coin_amt, FALSE)) > argument && coin_amt > 0) {
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
		else if (!(obj = get_obj_in_list_vis(ch, arg, ch->carrying))) {
			sprintf(buf, "You don't seem to have any %ss.\r\n", arg);
			send_to_char(buf, ch);
		}
		else {
			do {
				next_obj = get_obj_in_list_vis(ch, arg, obj->next_content);
				this = perform_drop(ch, obj, mode, sname);
				obj = next_obj;
				if (this == -1) {
					break;
				}
				else {
					amount += this;
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
				for (obj = ch->carrying; obj; obj = next_obj) {
					next_obj = obj->next_content;
					
					if (OBJ_FLAGGED(obj, OBJ_KEEP)) {
						continue;
					}
					
					this = perform_drop(ch, obj, mode, sname);
					if (this == -1) {
						break;
					}
					else {
						amount += this;
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
			if (!(obj = get_obj_in_list_vis(ch, arg, ch->carrying))) {
				sprintf(buf, "You don't seem to have any %ss.\r\n", arg);
				send_to_char(buf, ch);
				return;
			}
			while (obj) {
				next_obj = get_obj_in_list_vis(ch, arg, obj->next_content);
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
					amount += this;
					any = TRUE;
				}
			}
			
			if (!any) {
				msg_to_char(ch, "You don't have any that aren't marked 'keep'.\r\n");
			}
		}
		else {
			if (!(obj = get_obj_in_list_vis(ch, arg, ch->carrying))) {
				sprintf(buf, "You don't seem to have %s %s.\r\n", AN(arg), arg);
				send_to_char(buf, ch);
			}
			else {
				amount += perform_drop(ch, obj, mode, sname);
			}
		}
	}
}


ACMD(do_eat) {
	extern bool check_vampire_sun(char_data *ch, bool message);
	void taste_blood(char_data *ch, char_data *vict);
	
	bool extract = FALSE, will_buff = FALSE;
	char buf[MAX_STRING_LENGTH];
	struct affected_type *af;
	struct obj_apply *apply;
	obj_data *food;
	int eat_hours;

	one_argument(argument, arg);
	
	// 1. basic validation
	if (REAL_NPC(ch))		/* Cannot use GET_COND() on mobs. */
		return;
	if (!*arg) {
		send_to_char("Eat what?\r\n", ch);
		return;
	}
	if (!(food = get_obj_in_list_vis(ch, arg, ch->carrying))) {
		if (!(food = get_obj_in_list_vis(ch, arg, ROOM_CONTENTS(IN_ROOM(ch))))) {
			// special case: Taste Blood
			char_data *vict;
			if (subcmd == SCMD_TASTE && IS_VAMPIRE(ch) && has_ability(ch, ABIL_TASTE_BLOOD) && (vict = get_char_vis(ch, arg, FIND_CHAR_ROOM))) {
				if (check_vampire_sun(ch, TRUE) && !ABILITY_TRIGGERS(ch, vict, NULL, ABIL_TASTE_BLOOD)) {
					taste_blood(ch, vict);
				}
				return;
			}
			else {
				sprintf(buf, "You don't seem to have %s %s.\r\n", AN(arg), arg);
				send_to_char(buf, ch);
				return;
			}
		}
	}
	if (GET_OBJ_TYPE(food) == ITEM_DRINKCON) {
		do_drink(ch, argument, 0, subcmd == SCMD_TASTE ? SCMD_SIP : SCMD_DRINK);
		return;
	}
	if ((GET_OBJ_TYPE(food) != ITEM_FOOD)) {
		send_to_char("You can't eat THAT!\r\n", ch);
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
	if (!consume_otrigger(food, ch, OCMD_EAT)) {
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
		GET_OBJ_VAL(food, VAL_FOOD_HOURS_OF_FULLNESS) -= eat_hours;
		extract = (GET_FOOD_HOURS_OF_FULLNESS(food) <= 0);
	}
	
	// 5. messaging
	if (extract || subcmd == SCMD_EAT) {
		// message to char
		if (has_custom_message(food, OBJ_CUSTOM_EAT_TO_CHAR)) {
			act(get_custom_message(food, OBJ_CUSTOM_EAT_TO_CHAR), FALSE, ch, food, NULL, TO_CHAR);
		}
		else {
			snprintf(buf, sizeof(buf), "You eat %s$p.", (extract ? "" : "some of "));
			act(buf, FALSE, ch, food, NULL, TO_CHAR);
		}

		// message to room
		if (has_custom_message(food, OBJ_CUSTOM_EAT_TO_ROOM)) {
			act(get_custom_message(food, OBJ_CUSTOM_EAT_TO_ROOM), FALSE, ch, food, NULL, TO_ROOM);
		}
		else {
			snprintf(buf, sizeof(buf), "$n eats %s$p.", (extract ? "" : "some of "));
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
			af = create_flag_aff(ATYPE_WELL_FED, eat_hours MUD_HOURS, GET_OBJ_AFF_FLAGS(food), ch);
			affect_to_char(ch, af);
			free(af);
		}

		LL_FOREACH(GET_OBJ_APPLIES(food), apply) {
			af = create_mod_aff(ATYPE_WELL_FED, eat_hours MUD_HOURS, apply->location, apply->modifier, ch);
			affect_to_char(ch, af);
			free(af);
		}
		
		msg_to_char(ch, "You feel well-fed.\r\n");
	}
	else if (GET_COND(ch, FULL) < 75) {	// additional messages
		send_to_char("You are full.\r\n", ch);
	}
	
	// 7. cleanup
	if (extract) {
		extract_obj(food);
	}
	else {
		if (!IS_NPC(ch) && OBJ_FLAGGED(food, OBJ_BIND_FLAGS)) {
			bind_obj_to_player(food, ch);
			reduce_obj_binding(food, ch);
		}
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
	
	one_argument(argument, arg);
	
	if (IS_NPC(ch)) {
		msg_to_char(ch, "NPCs can't exchange anything.\r\n");
	}
	else if (!room_has_function_and_city_ok(IN_ROOM(ch), FNC_MINT | FNC_VAULT)) {
		msg_to_char(ch, "You can't exchange treasure for coins here.\r\n");
	}
	else if (!IS_COMPLETE(IN_ROOM(ch))) {
		msg_to_char(ch, "You need to finish building first.\r\n");
	}
	else if (!(emp = ROOM_OWNER(IN_ROOM(ch)))) {
		msg_to_char(ch, "This building does not belong to any empire, and can't exchange coins.\r\n");
	}
	else if (!EMPIRE_HAS_TECH(emp, TECH_COMMERCE)) {
		msg_to_char(ch, "This empire does not have Commerce, and cannot exchange coins.\r\n");
	}
	else if (!can_use_room(ch, IN_ROOM(ch), GUESTS_ALLOWED)) {
		msg_to_char(ch, "You don't have permission to exchange anything here.\r\n");
	}
	else if (!*arg) {
		msg_to_char(ch, "Exchange what for coins?\r\n");
	}
	else if ((pos = find_coin_arg(argument, &coin_emp, &amount, FALSE)) > argument && amount > 0) {
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
	else {
		// exchanging objs
		dotmode = find_all_dots(arg);
		
		if (dotmode == FIND_ALL) {
			carrying = IS_CARRYING_N(ch);
			
			for (obj = ch->carrying; obj; obj = next_obj) {
				next_obj = obj->next_content;
				
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
			else if (!(obj = get_obj_in_list_vis(ch, arg, ch->carrying))) {
				msg_to_char(ch, "You don't seem to have any %ss to exchange.\r\n", arg);
			}
			else {
				carrying = IS_CARRYING_N(ch);
			
				while (obj) {
					next_obj = get_obj_in_list_vis(ch, arg, obj->next_content);
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
			if (!(obj = get_obj_in_list_vis(ch, arg, ch->carrying))) {
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
	void do_get_from_vehicle(char_data *ch, vehicle_data *veh, char *arg, int mode, int howmany);

	char arg1[MAX_INPUT_LENGTH], arg2[MAX_INPUT_LENGTH], arg3[MAX_INPUT_LENGTH];
	int cont_dotmode, found = 0, mode;
	vehicle_data *find_veh, *veh;
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
			mode = generic_find(arg2, FIND_OBJ_INV | FIND_OBJ_ROOM | FIND_OBJ_EQUIP | FIND_VEHICLE_ROOM | FIND_VEHICLE_INSIDE, ch, &tmp_char, &cont, &find_veh);
			
			if (find_veh) {
				// pass off to vehicle handler
				do_get_from_vehicle(ch, find_veh, arg1, mode, amount);
			}
			else if (!cont) {
				sprintf(buf, "You don't have %s %s.\r\n", AN(arg2), arg2);
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
			LL_FOREACH2(ROOM_VEHICLES(IN_ROOM(ch)), veh, next_in_room) {
				if (CAN_SEE_VEHICLE(ch, veh) && VEH_FLAGGED(veh, VEH_CONTAINER) && (cont_dotmode == FIND_ALL || isname(arg2, VEH_KEYWORDS(veh)))) {
					found = 1;
					do_get_from_vehicle(ch, veh, arg1, FIND_OBJ_ROOM, amount);
				}
			}
			for (cont = ch->carrying; cont; cont = cont->next_content) {
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
			for (cont = ROOM_CONTENTS(IN_ROOM(ch)); cont; cont = cont->next_content) {
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
	if ((argpos = find_coin_arg(argument, &coin_emp, &coin_amt, FALSE)) > argument && coin_amt > 0) {
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
		else if (!(obj = get_obj_in_list_vis(ch, arg, ch->carrying))) {
			sprintf(buf, "You don't seem to have any %ss.\r\n", arg);
			send_to_char(buf, ch);
		}
		else {
			while (obj && amount > 0) {
				next_obj = get_obj_in_list_vis(ch, arg, obj->next_content);
				
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
			if (!(obj = get_obj_in_list_vis(ch, arg, ch->carrying))) {
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
				for (obj = ch->carrying; obj; obj = next_obj) {
					next_obj = obj->next_content;
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
	else if (!(obj = get_obj_in_list_vis(ch, arg, ch->carrying))) {
		sprintf(buf, "You don't seem to have %s %s.\r\n", AN(arg), arg);
		send_to_char(buf, ch);
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
		}
	}
	else {
		send_to_char("You can't hold that.\r\n", ch);
	}
}


ACMD(do_identify) {
	char_data *tmp_char;
	vehicle_data *veh;
	obj_data *obj;
	
	one_argument(argument, arg);
	
	if (GET_POS(ch) == POS_FIGHTING) {
		msg_to_char(ch, "You're too busy to do that now!\r\n");
	}
	else if (!*arg) {
		msg_to_char(ch, "Identify what object?\r\n");
	}
	else if (!generic_find(arg, FIND_OBJ_INV | FIND_OBJ_ROOM | FIND_OBJ_EQUIP | FIND_VEHICLE_ROOM | FIND_VEHICLE_INSIDE, ch, &tmp_char, &obj, &veh)) {
		msg_to_char(ch, "You see nothing like that here.\r\n");
	}
	else if (obj) {
		charge_ability_cost(ch, NOTHING, 0, NOTHING, 0, WAIT_OTHER);
		act("$n identifies $p.", TRUE, ch, obj, NULL, TO_ROOM);
		identify_obj_to_char(obj, ch);
	}
	else if (veh) {
		charge_ability_cost(ch, NOTHING, 0, NOTHING, 0, WAIT_OTHER);
		act("$n identifies $V.", TRUE, ch, NULL, veh, TO_ROOM);
		identify_vehicle_to_char(veh, ch);
	}
}


ACMD(do_keep) {
	char arg[MAX_INPUT_LENGTH];
	obj_data *obj, *next_obj;
	int dotmode, mode = SCMD_KEEP;
	const char *sname;

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
			for (obj = ch->carrying; obj; obj = next_obj) {
				next_obj = obj->next_content;
				
				if (mode == SCMD_KEEP) {
					SET_BIT(GET_OBJ_EXTRA(obj), OBJ_KEEP);
				}
				else {
					REMOVE_BIT(GET_OBJ_EXTRA(obj), OBJ_KEEP);
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
		if (!(obj = get_obj_in_list_vis(ch, arg, ch->carrying))) {
			msg_to_char(ch, "You don't seem to have any %ss.\r\n", arg);
		}
		while (obj) {
			next_obj = get_obj_in_list_vis(ch, arg, obj->next_content);
			if (mode == SCMD_KEEP) {
				SET_BIT(GET_OBJ_EXTRA(obj), OBJ_KEEP);
			}
			else {
				REMOVE_BIT(GET_OBJ_EXTRA(obj), OBJ_KEEP);
			}
			sprintf(buf, "You %s $p.", sname);
			act(buf, FALSE, ch, obj, NULL, TO_CHAR);
			obj = next_obj;
		}
	}
	else {
		if (!(obj = get_obj_in_list_vis(ch, arg, ch->carrying))) {
			msg_to_char(ch, "You don't seem to have %s %s.\r\n", AN(arg), arg);
		}
		else {
			if (mode == SCMD_KEEP) {
				SET_BIT(GET_OBJ_EXTRA(obj), OBJ_KEEP);
			}
			else {
				REMOVE_BIT(GET_OBJ_EXTRA(obj), OBJ_KEEP);
			}
			sprintf(buf, "You %s $p.", sname);
			act(buf, FALSE, ch, obj, NULL, TO_CHAR);
		}
	}
}


ACMD(do_light) {
	void do_light_vehicle(char_data *ch, vehicle_data *veh, obj_data *flint);
	
	obj_data *obj, *flint = NULL;
	bool magic = !IS_NPC(ch) && has_ability(ch, ABIL_TOUCH_OF_FLAME);
	vehicle_data *veh;

	one_argument(argument, arg);

	if (!magic) {
		flint = get_obj_in_list_num(o_FLINT_SET, ch->carrying);
	}

	if (!*arg)
		msg_to_char(ch, "Light what?\r\n");
	else if (!IS_NPC(ch) && !magic && !flint)
		msg_to_char(ch, "You don't have a flint set to light that with.\r\n");
	else if (!(obj = get_obj_in_list_vis(ch, arg, ch->carrying)) && !(obj = get_obj_in_list_vis(ch, arg, ROOM_CONTENTS(IN_ROOM(ch))))) {
		// try burning a vehicle
		if ((veh = get_vehicle_in_room_vis(ch, arg))) {
			do_light_vehicle(ch, veh, flint);
		}
		else {
			msg_to_char(ch, "You don't have a %s.\r\n", arg);
		}
	}
	else if (!has_interaction(obj->interactions, INTERACT_LIGHT)) {
		msg_to_char(ch, "You can't light that!\r\n");
	}
	else {
		if (magic) {
			act("You click your fingers and $p catches fire!", FALSE, ch, obj, NULL, TO_CHAR);
			act("$n clicks $s fingers and $p catches fire!", FALSE, ch, obj, NULL, TO_ROOM);
			gain_ability_exp(ch, ABIL_TOUCH_OF_FLAME, 15);
		}
		else if (flint) {
			act("You strike $p and light $P.", FALSE, ch, flint, obj, TO_CHAR);
			act("$n strikes $p and lights $P.", FALSE, ch, flint, obj, TO_ROOM);
		}
		else {
			act("You light $P.", FALSE, ch, NULL, obj, TO_CHAR);
			act("$n lights $P.", FALSE, ch, NULL, obj, TO_ROOM);
		}
		
		// will extract no matter what happens here
		empty_obj_before_extract(obj);
		run_interactions(ch, obj->interactions, INTERACT_LIGHT, IN_ROOM(ch), NULL, obj, light_obj_interact);
		extract_obj(obj);
		command_lag(ch, WAIT_OTHER);
	}
}


ACMD(do_pour) {	
	char arg1[MAX_INPUT_LENGTH];
	char arg2[MAX_INPUT_LENGTH];
	obj_data *from_obj = NULL, *to_obj = NULL;
	int amount;

	two_arguments(argument, arg1, arg2);

	if (subcmd == SCMD_POUR) {
		if (!*arg1) {		/* No arguments */
			send_to_char("From what do you want to pour?\r\n", ch);
			return;
		}
		if (!(from_obj = get_obj_in_list_vis(ch, arg1, ch->carrying))) {
			send_to_char("You can't find it!\r\n", ch);
			return;
		}
		if (GET_OBJ_TYPE(from_obj) != ITEM_DRINKCON) {
			send_to_char("You can't pour from that!\r\n", ch);
			return;
		}
	}
	if (subcmd == SCMD_FILL) {
		if (!*arg1) {		/* no arguments */
			send_to_char("What do you want to fill?  And from what are you filling it?\r\n", ch);
			return;
		}
		if (!(to_obj = get_obj_in_list_vis(ch, arg1, ch->carrying))) {
			send_to_char("You can't find it!\r\n", ch);
			return;
		}
		if (GET_OBJ_TYPE(to_obj) != ITEM_DRINKCON) {
			act("You can't fill $p!", FALSE, ch, to_obj, 0, TO_CHAR);
			return;
		}
		if (!*arg2) {		/* no 2nd argument */
			if (ROOM_SECT_FLAGGED(IN_ROOM(ch), SECTF_DRINK) || find_flagged_sect_within_distance_from_char(ch, SECTF_DRINK, NOBITS, 1) || (room_has_function_and_city_ok(IN_ROOM(ch), FNC_DRINK_WATER | FNC_TAVERN) && IS_COMPLETE(IN_ROOM(ch)))) {
				fill_from_room(ch, to_obj);
				return;
			}
			else if (IS_IMMORTAL(ch)) {
				// empty before message
				GET_OBJ_VAL(to_obj, VAL_DRINK_CONTAINER_CONTENTS) = 0;
				
				// message before filling
				act("You fill $p with water from thin air!", FALSE, ch, to_obj, NULL, TO_CHAR);
				act("$n fills $p with water from thin air!", TRUE, ch, to_obj, NULL, TO_ROOM);
				
				GET_OBJ_VAL(to_obj, VAL_DRINK_CONTAINER_TYPE) = LIQ_WATER;
				GET_OBJ_VAL(to_obj, VAL_DRINK_CONTAINER_CONTENTS) = GET_DRINK_CONTAINER_CAPACITY(to_obj);

				return;
			}
			else {
				act("From what would you like to fill $p?", FALSE, ch, to_obj, 0, TO_CHAR);
				return;
			}
		}
		if (is_abbrev(arg2, "water") && room_has_function_and_city_ok(IN_ROOM(ch), FNC_DRINK_WATER)) {
			fill_from_room(ch, to_obj);
			return;
		}
		if (is_abbrev(arg2, "river") || is_abbrev(arg2, "water")) {
			if (ROOM_SECT_FLAGGED(IN_ROOM(ch), SECTF_DRINK)) {
				fill_from_room(ch, to_obj);
				return;
			}
			else if (find_flagged_sect_within_distance_from_char(ch, SECTF_DRINK, NOBITS, 1)) {
				fill_from_room(ch, to_obj);
				return;
			}
			msg_to_char(ch, "There doesn't seem to be %s %s here.\r\n", AN(arg2), arg2);
			return;
		}
		sprintf(buf, "There doesn't seem to be %s %s here.\r\n", AN(arg2), arg2);
		send_to_char(buf, ch);
		return;
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
			act("$n empties $p.", TRUE, ch, from_obj, 0, TO_ROOM);
			act("You empty $p.", FALSE, ch, from_obj, 0, TO_CHAR);

			/* If it's a trench, fill her up */
			if (GET_DRINK_CONTAINER_TYPE(from_obj) == LIQ_WATER && ROOM_SECT_FLAGGED(IN_ROOM(ch), SECTF_IS_TRENCH) && get_room_extra_data(IN_ROOM(ch), ROOM_EXTRA_TRENCH_PROGRESS) >= 0) {
				add_to_room_extra_data(IN_ROOM(ch), ROOM_EXTRA_TRENCH_PROGRESS, GET_DRINK_CONTAINER_CONTENTS(from_obj));

				if (get_room_extra_data(IN_ROOM(ch), ROOM_EXTRA_TRENCH_PROGRESS) >= config_get_int("trench_full_value")) {
					void fill_trench(room_data *room);
					fill_trench(IN_ROOM(ch));
				}
			}

			GET_OBJ_VAL(from_obj, VAL_DRINK_CONTAINER_CONTENTS) = 0;
			GET_OBJ_VAL(from_obj, VAL_DRINK_CONTAINER_TYPE) = 0;

			return;
		}
		if (!(to_obj = get_obj_in_list_vis(ch, arg2, ch->carrying))) {
			send_to_char("You can't find it!\r\n", ch);
			return;
		}
		if ((GET_OBJ_TYPE(to_obj) != ITEM_DRINKCON)) {
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
	if ((GET_DRINK_CONTAINER_CONTENTS(to_obj) != 0) && (GET_DRINK_CONTAINER_TYPE(to_obj) != GET_DRINK_CONTAINER_TYPE(from_obj))) {
		send_to_char("There is already another liquid in it. Pour it out first.\r\n", ch);
		return;
	}
	if (GET_DRINK_CONTAINER_CONTENTS(to_obj) > GET_DRINK_CONTAINER_CAPACITY(to_obj)) {
		send_to_char("There is no room for more.\r\n", ch);
		return;
	}
	if (subcmd == SCMD_POUR) {
		sprintf(buf, "You pour the %s into %s.", drinks[GET_DRINK_CONTAINER_TYPE(from_obj)], GET_OBJ_SHORT_DESC(to_obj));
		send_to_char(buf, ch);
	}
	if (subcmd == SCMD_FILL) {
		act("You gently fill $p from $P.", FALSE, ch, to_obj, from_obj, TO_CHAR);
		act("$n gently fills $p from $P.", TRUE, ch, to_obj, from_obj, TO_ROOM);
	}

	/* First same type liq. */
	GET_OBJ_VAL(to_obj, VAL_DRINK_CONTAINER_TYPE) = GET_DRINK_CONTAINER_TYPE(from_obj);

	// Then how much to pour -- this CAN make the original container go negative
	amount = GET_DRINK_CONTAINER_CAPACITY(to_obj) - GET_DRINK_CONTAINER_CONTENTS(to_obj);
	GET_OBJ_VAL(from_obj, VAL_DRINK_CONTAINER_CONTENTS) -= amount;

	GET_OBJ_VAL(to_obj, VAL_DRINK_CONTAINER_CONTENTS) += amount;
	
	// copy the timer on the liquid, even if UNLIMITED
	GET_OBJ_TIMER(to_obj) = GET_OBJ_TIMER(from_obj);

	// check if there was too little to pour, and adjust
	if (GET_DRINK_CONTAINER_CONTENTS(from_obj) <= 0) {
		// add the negative amount to subtract it back
		GET_OBJ_VAL(to_obj, VAL_DRINK_CONTAINER_CONTENTS) += GET_DRINK_CONTAINER_CONTENTS(from_obj);
		amount += GET_DRINK_CONTAINER_CONTENTS(from_obj);
		GET_OBJ_VAL(from_obj, VAL_DRINK_CONTAINER_CONTENTS) = 0;
		GET_OBJ_VAL(from_obj, VAL_DRINK_CONTAINER_TYPE) = 0;
		GET_OBJ_TIMER(from_obj) = UNLIMITED;
	}
	
	// check max
	GET_OBJ_VAL(to_obj, VAL_DRINK_CONTAINER_CONTENTS) = MIN(GET_DRINK_CONTAINER_CONTENTS(to_obj), GET_DRINK_CONTAINER_CAPACITY(to_obj));
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
	void do_put_obj_in_vehicle(char_data *ch, vehicle_data *veh, int dotmode, char *arg, int howmany);
	
	char arg1[MAX_INPUT_LENGTH], arg2[MAX_INPUT_LENGTH], arg3[MAX_INPUT_LENGTH];
	obj_data *obj, *next_obj, *cont;
	vehicle_data *find_veh;
	char_data *tmp_char;
	int obj_dotmode, cont_dotmode, found = 0, howmany = 1;
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
		generic_find(thecont, FIND_OBJ_INV | FIND_OBJ_ROOM | FIND_OBJ_EQUIP | FIND_VEHICLE_ROOM | FIND_VEHICLE_INSIDE, ch, &tmp_char, &cont, &find_veh);
		
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
				if (!(obj = get_obj_in_list_vis(ch, theobj, ch->carrying))) {
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
						obj = get_obj_in_list_vis(ch, theobj, next_obj);
						found = 1;
					}
					
					if (!found) {
						msg_to_char(ch, "You didn't seem to have any that aren't marked 'keep'.\r\n");
					}
				}
			}
			else {
				for (obj = ch->carrying; obj; obj = next_obj) {
					next_obj = obj->next_content;
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


ACMD(do_remove) {
	extern int board_loaded;
	void init_boards(void);
	extern int find_board(char_data *ch);
	extern int Board_remove_msg(int board_type, char_data *ch, char *arg, obj_data *board);
	int i, dotmode, found;
	int board_type;
	obj_data *board;

	one_argument(argument, arg);

	if (isdigit(*arg)) {
		for (board = ROOM_CONTENTS(IN_ROOM(ch)); board; board = board->next_content)
			if (GET_OBJ_TYPE(board) == ITEM_BOARD)
				break;
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
		if (!get_object_in_equip_vis(ch, arg, ch->equipment, &i)) {
			sprintf(buf, "You don't seem to be using %s %s.\r\n", AN(arg), arg);
			send_to_char(buf, ch);
		}
		else
			perform_remove(ch, i);
	}
}


ACMD(do_retrieve) {	
	struct empire_storage_data *store, *next_store;
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
	if (!can_use_room(ch, IN_ROOM(ch), GUESTS_ALLOWED) || (room_emp && emp != room_emp && !has_relationship(emp, room_emp, DIPL_TRADE))) {
		msg_to_char(ch, "You need to establish a trade pact to retrieve anything here.\r\n");
		return;
	}

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
		for (store = EMPIRE_STORAGE(emp); store; store = next_store) {
			next_store = store->next;
			
			// same island?
			if (store->island != GET_ISLAND_ID(IN_ROOM(ch))) {
				continue;
			}

			if ((objn = obj_proto(store->vnum)) && obj_can_be_stored(objn, IN_ROOM(ch))) {
				if (stored_item_requires_withdraw(objn) && !has_permission(ch, PRIV_WITHDRAW)) {
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
		for (store = EMPIRE_STORAGE(emp); store && !found; store = next_store) {
			next_store = store->next;
			
			// island check
			if (store->island != GET_ISLAND_ID(IN_ROOM(ch))) {
				continue;
			}
			
			if ((objn = obj_proto(store->vnum)) && obj_can_be_stored(objn, IN_ROOM(ch))) {
				if (multi_isname(objname, GET_OBJ_KEYWORDS(objn)) && (++pos == number)) {
					found = 1;
					
					if (stored_item_requires_withdraw(objn) && !has_permission(ch, PRIV_WITHDRAW)) {
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
			msg_to_char(ch, "Nothing like that is stored here!\r\n");
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

		/* save the empire */
		SAVE_CHAR(ch);
		EMPIRE_NEEDS_SAVE(emp) = TRUE;
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
	else if (get_skill_level(ch, SKILL_EMPIRE) <= BASIC_SKILL_CAP) {
		msg_to_char(ch, "You need the Roads ability and an Empire skill of at least %d to set up road signs.\r\n", BASIC_SKILL_CAP+1);
	}
	else if (!has_ability(ch, ABIL_ROADS)) {
		msg_to_char(ch, "You require the Roads ability to set up road signs.\r\n");
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
	else {
		if (ROOM_CUSTOM_DESCRIPTION(IN_ROOM(ch))) {
			free(ROOM_CUSTOM_DESCRIPTION(IN_ROOM(ch)));
			old_sign = TRUE;
		}
		ROOM_CUSTOM_DESCRIPTION(IN_ROOM(ch)) = str_dup(argument);

		act("You engrave your message and plant $p in the ground by the road.", FALSE, ch, sign, NULL, TO_CHAR);
		act("$n engraves a message on $p and plants it in the ground by the road.", FALSE, ch, sign, NULL, TO_ROOM);
		
		if (old_sign) {
			msg_to_char(ch, "The old sign has been replaced.\r\n");
		}

		gain_ability_exp(ch, ABIL_ROADS, 33.4);
		extract_obj(sign);
	}
}


ACMD(do_separate) {
	char arg[MAX_INPUT_LENGTH];
	obj_data *obj;
	
	one_argument(argument, arg);
	
	if (!*arg) {
		msg_to_char(ch, "Separate what?\r\n");
	}
	else if (!(obj = get_obj_in_list_vis(ch, arg, ch->carrying))) {
		msg_to_char(ch, "You don't have %s %s.\r\n", AN(arg), arg);
	}
	else if (!has_interaction(obj->interactions, INTERACT_SEPARATE)) {
		msg_to_char(ch, "You can't separate that!\r\n");
	}
	else {		
		if (run_interactions(ch, obj->interactions, INTERACT_SEPARATE, IN_ROOM(ch), NULL, obj, separate_obj_interact)) {
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
	else if (!(obj = get_obj_in_list_vis(ch, arg, ch->carrying))) {
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
	obj_data *obj;
	int loc = 0;

	if (!(obj = GET_EQ(ch, WEAR_WIELD))) {
		msg_to_char(ch, "You aren't even wielding anything!\r\n");
		return;
	}
	
	if (OBJ_FLAGGED(obj, OBJ_SINGLE_USE)) {
		msg_to_char(ch, "You can't sheathe that.\r\n");
		return;
	}

	/* are any of them even open? */
	if (!GET_EQ(ch, WEAR_SHEATH_1)) {
		loc = WEAR_SHEATH_1;
	}
	else if (!GET_EQ(ch, WEAR_SHEATH_2)) {
		loc = WEAR_SHEATH_2;
	}
	else {
		msg_to_char(ch, "You have no empty sheaths!\r\n");
		return;
	}

	obj_to_char(unequip_char(ch, WEAR_WIELD), ch);
	perform_wear(ch, obj, loc);
}


ACMD(do_ship) {
	char arg1[MAX_INPUT_LENGTH], arg2[MAX_INPUT_LENGTH], buf[MAX_STRING_LENGTH * 3], line[1000], keywords[MAX_INPUT_LENGTH];
	struct island_info *from_isle, *to_isle;
	struct empire_storage_data *store;
	struct shipping_data *sd, *temp;
	bool done, gave_number = FALSE;
	vehicle_data *veh;
	obj_data *proto;
	int number = 1;
	size_t size;
	
	// SHIPPING_x
	const char *status_type[] = { "waiting for ship", "en route", "delivered", "\n" };
	
	argument = any_one_word(argument, arg1);	// command
	argument = any_one_word(argument, arg2);	// number or keywords
	skip_spaces(&argument);	// keywords
	
	if (isdigit(*arg2)) {
		number = atoi(arg2);
		gave_number = TRUE;
		snprintf(keywords, sizeof(keywords), "%s", argument);
	}
	else {
		// concatenate arg2 and argument back together, it's just keywords
		snprintf(keywords, sizeof(keywords), "%s%s%s", arg2, *argument ? " " : "", argument);
	}
	
	if (!IS_APPROVED(ch) && config_get_bool("manage_empire_approval")) {
		send_config_msg(ch, "need_approval_string");
	}
	else if (IS_NPC(ch) || !GET_LOYALTY(ch) || !ch->desc) {
		msg_to_char(ch, "You can't use the shipping system unless you're in an empire.\r\n");
	}
	else if (GET_RANK(ch) < EMPIRE_PRIV(GET_LOYALTY(ch), PRIV_SHIPPING)) {
		msg_to_char(ch, "You don't have permission to ship anything.\r\n");
	}
	else if (!*arg1) {
		msg_to_char(ch, "Usage: ship status\r\n");
		msg_to_char(ch, "Usage: ship cancel [number] <item>\r\n");
		msg_to_char(ch, "Usage: ship <island> [number] <item>\r\n");
	}
	else if (!str_cmp(arg1, "status") || !str_cmp(arg1, "stat")) {
		size = snprintf(buf, sizeof(buf), "Shipping queue for %s:\r\n", EMPIRE_NAME(GET_LOYALTY(ch)));
		
		done = FALSE;
		for (sd = EMPIRE_SHIPPING_LIST(GET_LOYALTY(ch)); sd; sd = sd->next) {
			if (sd->vnum == NOTHING) {
				// just a ship, not a shipment
				if (sd->shipping_id == -1 || !(veh = find_ship_by_shipping_id(GET_LOYALTY(ch), sd->shipping_id))) {
					continue;
				}
				if (*keywords && !multi_isname(keywords, VEH_KEYWORDS(veh))) {
					continue;
				}
				
				from_isle = get_island(sd->from_island, TRUE);
				to_isle = get_island(sd->to_island, TRUE);
				snprintf(line, sizeof(line), "    %s (%s to %s, %s)\r\n", skip_filler(VEH_SHORT_DESC(veh)), from_isle ? from_isle->name : "unknown", to_isle ? to_isle->name : "unknown", status_type[sd->status]);
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
				snprintf(line, sizeof(line), " %dx %s (%s to %s, %s)\r\n", sd->amount, skip_filler(GET_OBJ_SHORT_DESC(proto)), from_isle ? from_isle->name : "unknown", to_isle ? to_isle->name : "unknown", status_type[sd->status]);
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
	else if (GET_ISLAND_ID(IN_ROOM(ch)) == NO_ISLAND) {
		msg_to_char(ch, "You can't ship anything from here.\r\n");
	}
	else if (!str_cmp(arg1, "cancel")) {
		if (!*keywords) {
			msg_to_char(ch, "Cancel which shipment?\r\n");
			return;
		}
		
		// find a matching entry
		done = FALSE;
		for (sd = EMPIRE_SHIPPING_LIST(GET_LOYALTY(ch)); sd; sd = sd->next) {
			if (sd->status != SHIPPING_QUEUED || sd->shipping_id != -1) {
				continue;	// never cancel one in progress
			}
			if (sd->from_island != GET_ISLAND_ID(IN_ROOM(ch))) {
				continue;
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
			
			// found!
			msg_to_char(ch, "You cancel the shipment for %d '%s'.\r\n", sd->amount, skip_filler(GET_OBJ_SHORT_DESC(proto)));
			add_to_empire_storage(GET_LOYALTY(ch), sd->from_island, sd->vnum, sd->amount);
			
			REMOVE_FROM_LIST(sd, EMPIRE_SHIPPING_LIST(GET_LOYALTY(ch)), next);
			free(sd);
			EMPIRE_NEEDS_SAVE(GET_LOYALTY(ch)) = TRUE;
			
			done = TRUE;
			break;	// only allow 1st match
		}
		
		if (!done) {
			msg_to_char(ch, "No shipments like that found to cancel.\r\n");
		}
	}
	else {
		if (number < 1 || !*keywords) {
			msg_to_char(ch, "Usage: ship <island> [number] <item>\r\n");
		}
		else if (!find_docks(GET_LOYALTY(ch), GET_ISLAND_ID(IN_ROOM(ch)))) {
			msg_to_char(ch, "This island has no docks (docks must not be set no-work).\r\n");
		}
		else if (!(to_isle = get_island_by_name(ch, arg1)) && !(to_isle = get_island_by_coords(arg1))) {
			msg_to_char(ch, "Unknown target island \"%s\".\r\n", arg1);
		}
		else if (to_isle->id == GET_ISLAND_ID(IN_ROOM(ch))) {
			msg_to_char(ch, "You are already on that island.\r\n");
		}
		else if (!(store = find_island_storage_by_keywords(GET_LOYALTY(ch), GET_ISLAND_ID(IN_ROOM(ch)), keywords))) {
			msg_to_char(ch, "You don't seem to have any '%s' stored on this island to ship.\r\n", keywords);
		}
		else if (store->amount < number) {
			msg_to_char(ch, "You only have %d '%s' stored on this island.\r\n", store->amount, skip_filler(get_obj_name_by_proto(store->vnum)));
		}
		else if (!find_docks(GET_LOYALTY(ch), to_isle->id)) {
			msg_to_char(ch, "%s has no docks (docks must not be set no-work).\r\n", to_isle->name);
		}
		else {
			add_shipping_queue(ch, GET_LOYALTY(ch), GET_ISLAND_ID(IN_ROOM(ch)), to_isle->id, number, store->vnum);
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
	for (vict = ROOM_PEOPLE(IN_ROOM(ch)); vict; vict = vict->next_in_room) {
		if (!IS_NPC(vict) && GROUP(vict) && GROUP(vict) == GROUP(ch)) {
			++count;
		}
	}

	// parse args
	pos = find_coin_arg(argument, &coin_emp, &coin_amt, TRUE);
	
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
		
		for (vict = ROOM_PEOPLE(IN_ROOM(ch)); vict; vict = vict->next_in_room) {
			if (vict != ch && !IS_NPC(vict) && GROUP(vict) && GROUP(vict) == GROUP(ch)) {
				increase_coins(vict, coin_emp, split);
				decrease_coins(ch, coin_emp, split);
				act(buf, FALSE, ch, NULL, vict, TO_VICT | TO_SLEEP);
			}
		}
	}
}


ACMD(do_store) {	
	struct empire_storage_data *store;
	obj_data *obj, *next_obj;
	int count = 0, total = 1, done = 0, dotmode;
	empire_data *emp, *room_emp = ROOM_OWNER(IN_ROOM(ch));
	bool full = 0;

	if (!IS_COMPLETE(IN_ROOM(ch))) {
		msg_to_char(ch, "You'll need to finish the building first.\r\n");
		return;
	}

	if (!(emp = GET_LOYALTY(ch))) {
		msg_to_char(ch, "You can't store or retrieve resources unless you're a member of an empire.\r\n");
		return;
	}
	if (!can_use_room(ch, IN_ROOM(ch), GUESTS_ALLOWED) || (room_emp && emp != room_emp && !has_relationship(emp, room_emp, DIPL_TRADE))) {
		msg_to_char(ch, "You need to establish a trade pact to store your things here.\r\n");
		return;
	}

	two_arguments(argument, arg, buf);

	/* This goes first because I want it to move buf to arg */
	if (*arg && is_number(arg)) {
		total = atoi(arg);
		if (total < 1) {
			msg_to_char(ch, "You have to store at least 1.\r\n");
			return;
		}
		strcpy(arg, buf);
	}
	else if (*arg && *buf && !str_cmp(arg, "all")) {
		total = CAN_CARRY_N(ch) + 1;
		strcpy(arg, buf);
	}

	if (!*arg) {
		msg_to_char(ch, "What would you like to store?\r\n");
		return;
	}

	dotmode = find_all_dots(arg);

	if (dotmode == FIND_ALL) {
		if (!ch->carrying) {
			send_to_char("You don't seem to be carrying anything.\r\n", ch);
			return;
		}
		for (obj = ch->carrying; obj; obj = next_obj) {
			next_obj = obj->next_content;
			
			if (!OBJ_FLAGGED(obj, OBJ_KEEP) && OBJ_CAN_STORE(obj) && obj_can_be_stored(obj, IN_ROOM(ch))) {
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
		if (!done) {
			if (full) {
				msg_to_char(ch, "It's full.\r\n");
			}
			else {
				msg_to_char(ch, "You don't have anything that can be stored here.\r\n");
			}
		}
	}
	else {
		if (!*arg) {
			msg_to_char(ch, "What do you want to store all of?\r\n");
			return;
		}
		if (!(obj = get_obj_in_list_vis(ch, arg, ch->carrying))) {
			msg_to_char(ch, "You don't seem to have any %ss.\r\n", arg);
			return;
		}
		while (obj && (dotmode == FIND_ALLDOT || count < total)) {
			next_obj = get_obj_in_list_vis(ch, arg, obj->next_content);
			
			if ((!OBJ_FLAGGED(obj, OBJ_KEEP) || (total == 1 && dotmode != FIND_ALLDOT)) && OBJ_CAN_STORE(obj) && obj_can_be_stored(obj, IN_ROOM(ch))) {
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

		/* save the empire */
		SAVE_CHAR(ch);
		EMPIRE_NEEDS_SAVE(emp) = TRUE;
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
	else if ((!room_has_function_and_city_ok(IN_ROOM(ch), FNC_TRADING_POST) || !IS_COMPLETE(IN_ROOM(ch))) && !IS_IMMORTAL(ch)) {
		msg_to_char(ch, "You can't trade here.\r\n");
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
	void use_poison(char_data *ch, obj_data *obj);

	obj_data *obj;
	
	one_argument(argument, arg);
	
	if (!*arg) {
		msg_to_char(ch, "Use what?\r\n");
	}
	else if (!(obj = get_obj_in_list_vis(ch, arg, ch->carrying))) {
		msg_to_char(ch, "You don't seem to have a %s.\r\n", arg);
	}
	else {
		if (IS_POISON(obj)) {
			use_poison(ch, obj);
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
		warehouse_inventory(ch, argument);
	}
	// all other commands require awakeness
	else if (GET_POS(ch) < POS_RESTING || FIGHTING(ch)) {
		msg_to_char(ch, "You can't do that right now.\r\n");
	}
	else if (is_abbrev(command, "identify")) {
		warehouse_identify(ch, argument);
	}
	else if (is_abbrev(command, "retrieve")) {
		warehouse_retrieve(ch, argument);
	}
	else if (is_abbrev(command, "store")) {
		warehouse_store(ch, argument);
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
	for (iter = ROOM_PEOPLE(IN_ROOM(ch)); iter && !fighting_me; iter = iter->next_in_room) {
		if (FIGHTING(iter) == ch) {
			fighting_me = TRUE;
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
		for (obj = ch->carrying; obj; obj = next_obj) {
			next_obj = obj->next_content;
			if (CAN_SEE_OBJ(ch, obj) && (where = find_eq_pos(ch, obj, 0)) != NO_WEAR && where < NUM_WEARS && !GET_EQ(ch, where) && can_wear_item(ch, obj, FALSE)) {
				items_worn++;
				perform_wear(ch, obj, where);
			}
		}
		if (!items_worn) {
			send_to_char("You don't seem to have anything else you can wear.\r\n", ch);
		}
	}
	else if (dotmode == FIND_ALLDOT) {
		if (!*arg1) {
			send_to_char("Wear all of what?\r\n", ch);
			return;
		}
		if (!(obj = get_obj_in_list_vis(ch, arg1, ch->carrying))) {
			sprintf(buf, "You don't seem to have any %ss you can wear.\r\n", arg1);
			send_to_char(buf, ch);
		}
		else {
			while (obj) {
				next_obj = get_obj_in_list_vis(ch, arg1, obj->next_content);
				if ((where = find_eq_pos(ch, obj, 0)) != NO_WEAR && where < NUM_WEARS && !GET_EQ(ch, where) && can_wear_item(ch, obj, FALSE)) {
					perform_wear(ch, obj, where);
				}
				else if (!GET_EQ(ch, where)) {
					act("You can't wear $p.", FALSE, ch, obj, 0, TO_CHAR);
				}
				obj = next_obj;
			}
		}
	}
	else {
		if (!(obj = get_obj_in_list_vis(ch, arg1, ch->carrying))) {
			sprintf(buf, "You don't seem to have %s %s.\r\n", AN(arg1), arg1);
			send_to_char(buf, ch);
		}
		else {
			if ((where = find_eq_pos(ch, obj, arg2)) >= 0) {
				if (can_wear_item(ch, obj, TRUE)) {
					// sends its own error message if it fails
					perform_wear(ch, obj, where);
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
	else if (!(obj = get_obj_in_list_vis(ch, arg, ch->carrying))) {
		sprintf(buf, "You don't seem to have %s %s.\r\n", AN(arg), arg);
		send_to_char(buf, ch);
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
	}
}
