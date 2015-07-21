/* ************************************************************************
*   File: act.item.c                                      EmpireMUD 2.0b2 *
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
*   Special Helpers
*   Trade Command Functions
*   Warehouse Command Functions
*   Commands
*/

// extern variables
extern const char *drinks[];
extern int drink_aff[][3];
extern const struct wear_data_type wear_data[NUM_WEARS];

// extern functions
extern bool can_steal(char_data *ch, empire_data *emp);
extern bool can_wear_item(char_data *ch, obj_data *item, bool send_messages);
void expire_trading_post_item(struct trading_post_data *tpd);
extern char *get_room_name(room_data *room, bool color);
void read_vault(empire_data *emp);
void save_trading_post();
void trigger_distrust_from_stealth(char_data *ch, empire_data *emp);

// local protos
int get_wear_by_item_wear(bitvector_t item_wear);
void scale_item_to_level(obj_data *obj, int level);
static void wear_message(char_data *ch, obj_data *obj, int where);

// local stuff
#define drink_OBJ  -1
#define drink_ROOM  1


 //////////////////////////////////////////////////////////////////////////////
//// HELPERS /////////////////////////////////////////////////////////////////

/**
* @param char_data *ch Prospective taker.
* @param obj_data *obj The item he's trying to take.
* @return bool TRUE if ch can take obj.
*/
static bool can_take_obj(char_data *ch, obj_data *obj) {
	if (IS_CARRYING_N(ch) + GET_OBJ_INVENTORY_SIZE(obj) > CAN_CARRY_N(ch)) {
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
	extern double get_weapon_speed(obj_data *weapon);
	extern const char *extra_bits[];
	extern const char *drinks[];
	extern const char *affected_bits[];
	extern const char *apply_types[];
	extern const char *armor_types[NUM_ARMOR_TYPES+1];
	extern const char *wear_bits[];

	struct obj_storage_type *store;
	char lbuf[MAX_STRING_LENGTH], location[MAX_STRING_LENGTH];
	bld_data *bld;
	int iter, found;
	
	// ONLY flags to show
	bitvector_t show_obj_flags = OBJ_LIGHT | OBJ_SUPERIOR | OBJ_ENCHANTED | OBJ_JUNK | OBJ_TWO_HANDED | OBJ_BIND_ON_EQUIP | OBJ_BIND_ON_PICKUP | OBJ_HARD_DROP | OBJ_GROUP_DROP | OBJ_GENERIC_DROP;
	
	// sanity / don't bother
	if (!obj || !ch || !ch->desc) {
		return;
	}
	
	// determine location
	if (IN_ROOM(obj)) {
		strcpy(location, " (in room)");
	}
	else if (obj->carried_by) {
		snprintf(location, sizeof(location), " (carried by %s)", PERS(obj->carried_by, obj->carried_by, FALSE));
	}
	else if (obj->in_obj) {
		snprintf(location, sizeof(location), " (in %s)", GET_OBJ_DESC(obj->in_obj, ch, OBJ_DESC_SHORT));
	}
	else if (obj->worn_by) {
		snprintf(location, sizeof(location), " (worn by %s)", PERS(obj->worn_by, obj->worn_by, FALSE));
	}
	else {
		*location = '\0';
	}
	
	// basic info
	snprintf(lbuf, sizeof(lbuf), "Your analysis of $p%s reveals:", location);
	act(lbuf, FALSE, ch, obj, NULL, TO_CHAR);
	
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
		for (bind = OBJ_BOUND_TO(obj); bind; bind = bind->next) {
			msg_to_char(ch, " %s", get_name_by_id(bind->idnum) ? CAP(get_name_by_id(bind->idnum)) : "<unknown>");
		}
		msg_to_char(ch, "\r\n");
	}
	
	// only show gear if equippable (has more than ITEM_WEAR_TRADE)
	if ((GET_OBJ_WEAR(obj) & ~ITEM_WEAR_TAKE) != NOBITS) {
		msg_to_char(ch, "Gear level: %.1f", rate_item(obj));
		if (GET_OBJ_CURRENT_SCALE_LEVEL(obj) > 0) {
			msg_to_char(ch, " (scaled to %d)", GET_OBJ_CURRENT_SCALE_LEVEL(obj));
		}
		msg_to_char(ch, "\r\n");
		
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
			msg_to_char(ch, "Has %d charges remaining.\r\n", GET_POISON_CHARGES(obj));
			break;
		}
		case ITEM_WEAPON:
			msg_to_char(ch, "Speed: %.2f\r\n", get_weapon_speed(obj));

			msg_to_char(ch, "Damage equal to %s", (IS_MAGIC_ATTACK(GET_WEAPON_TYPE(obj)) ? "Intelligence" : "Strength"));
			if (GET_WEAPON_DAMAGE_BONUS(obj) != 0)
				msg_to_char(ch, " %+d", GET_WEAPON_DAMAGE_BONUS(obj));

			msg_to_char(ch, " (%.2f base dps)\r\n", get_base_dps(obj));

			msg_to_char(ch, "Damage type is %s.\r\n", attack_hit_info[GET_WEAPON_TYPE(obj)].name);
			break;
		case ITEM_ARMOR:
			msg_to_char(ch, "Armor type: %s\r\n", armor_types[GET_ARMOR_TYPE(obj)]);
			break;
		case ITEM_CONTAINER:
			msg_to_char(ch, "Holds %d items.\r\n", GET_MAX_CONTAINER_CONTENTS(obj));
			break;
		case ITEM_DRINKCON:
			msg_to_char(ch, "Contains %d ounces of %s.\r\n", GET_DRINK_CONTAINER_CONTENTS(obj), drinks[GET_DRINK_CONTAINER_TYPE(obj)]);
			break;
		case ITEM_FOOD:
			msg_to_char(ch, "Fills for %d hours.\r\n", GET_FOOD_HOURS_OF_FULLNESS(obj));
			if (OBJ_FLAGGED(obj, OBJ_PLANTABLE))
				msg_to_char(ch, "Plants %s.\r\n", GET_CROP_NAME(crop_proto(GET_FOOD_CROP_TYPE(obj))));
			break;
		case ITEM_CORPSE:
			msg_to_char(ch, "Corpse of ");

			if (mob_proto(GET_CORPSE_NPC_VNUM(obj)))
				msg_to_char(ch, "%s\r\n", get_mob_name_by_proto(GET_CORPSE_NPC_VNUM(obj)));
			else if (IS_NPC_CORPSE(obj))
				msg_to_char(ch, "nothing.\r\n");
			else
				msg_to_char(ch, "%s.\r\n", get_name_by_id(GET_CORPSE_PC_ID(obj)) ? CAP(get_name_by_id(GET_CORPSE_PC_ID(obj))) : "a player");
			break;
		case ITEM_COINS: {
			msg_to_char(ch, "Contains %s\r\n", money_amount(real_empire(GET_COINS_EMPIRE_ID(obj)), GET_COINS_AMOUNT(obj)));
			break;
		}
		case ITEM_CART:
			msg_to_char(ch, "Holds %d items.\r\n", GET_MAX_CART_CONTENTS(obj));
			if (CART_CAN_FIRE(obj))
				msg_to_char(ch, "Capable of firing shots.\r\n");
			break;
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
	
	
	*lbuf = '\0';
	for (iter = 0; iter < MAX_OBJ_AFFECT; ++iter) {
		if (obj->affected[iter].modifier) {
			sprintf(lbuf + strlen(lbuf), "%s%+d to %s", (*lbuf != '\0') ? ", " : "", obj->affected[iter].modifier, apply_types[(int) obj->affected[iter].location]);
		}
	}
	if (*lbuf) {
		msg_to_char(ch, "Modifiers: %s\r\n", lbuf);
	}
}


INTERACTION_FUNC(light_obj_interact) {	
	obj_vnum vnum = interaction->vnum;
	obj_data *new = NULL;
	int num;
	
	for (num = 0; num < interaction->quantity; ++num) {
		// load
		new = read_object(vnum);
		
		if (OBJ_FLAGGED(new, OBJ_SCALABLE)) {
			scale_item_to_level(new, GET_OBJ_CURRENT_SCALE_LEVEL(inter_item));
		}
		
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
	if (!drop_otrigger(obj, ch)) {	// also takes care of obj purging self
		return 0;
	}
	
	// don't let people drop bound items in other people's territory
	if (IN_ROOM(cont) && OBJ_BOUND_TO(obj) && ROOM_OWNER(IN_ROOM(cont)) && ROOM_OWNER(IN_ROOM(cont)) != GET_LOYALTY(ch)) {
		msg_to_char(ch, "You can't put bound items in items here.\r\n");
		return 0;
	}
	
	if (GET_OBJ_CARRYING_N(cont) + GET_OBJ_INVENTORY_SIZE(obj) > GET_MAX_CONTAINER_CONTENTS(cont)) {
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
	}
	return 1;
}


void perform_remove(char_data *ch, int pos) {
	obj_data *obj;

	if (!(obj = GET_EQ(ch, pos)))
		log("SYSERR: perform_remove: bad pos %d passed.", pos);
	else if (pos == WEAR_SADDLE && IS_RIDING(ch)) {
		msg_to_char(ch, "You can't remove your saddle while you're riding!\r\n");
	}
	else {
		if (!remove_otrigger(obj, ch)) {
			return;
		}

		act("You stop using $p.", FALSE, ch, obj, 0, TO_CHAR);
		act("$n stops using $p.", TRUE, ch, obj, 0, TO_ROOM);
		
		// this may extract it, or drop it
		unequip_char_to_inventory(ch, pos);
	}
}


static void perform_wear(char_data *ch, obj_data *obj, int where) {
	extern const int apply_attribute[];
	extern const int primary_attributes[];
	int iter, app;

	/* first, make sure that the wear position is valid. */
	if (!CAN_WEAR(obj, wear_data[where].item_wear)) {
		act("You can't wear $p there.", FALSE, ch, obj, 0, TO_CHAR);
		return;
	}
	
	// position cascade (ring 1/2, etc)
	if (GET_EQ(ch, where) && wear_data[where].cascade_pos != NO_WEAR) {
		where = wear_data[where].cascade_pos;
	}

	// make sure it wouldn't drop any primary attribute below 1
	for (iter = 0; primary_attributes[iter] != NOTHING; ++iter) {
		for (app = 0; app < MAX_OBJ_AFFECT; app++) {
			if (apply_attribute[(int) obj->affected[app].location] == primary_attributes[iter] && GET_ATT(ch, primary_attributes[iter]) + obj->affected[app].modifier < 1) {
				act("You are too weak to use $p!", FALSE, ch, obj, 0, TO_CHAR);
				return;
			}
		}
	}
		
	if (where == WEAR_SADDLE && !IS_RIDING(ch)) {
		msg_to_char(ch, "You can't wear a saddle while you're not riding anything.\r\n");
		return;
	}

	if (GET_EQ(ch, where)) {
		act(wear_data[where].already_wearing, FALSE, ch, GET_EQ(ch, where), NULL, TO_CHAR);
		return;
	}

	/* See if a trigger disallows it */
	if (!wear_otrigger(obj, ch, where) || (obj->carried_by != ch)) {
		return;
	}

	wear_message(ch, obj, where);
	equip_char(ch, obj, where);
}


/**
* Removes all armor of a certain type from a character...
*
* @param char_data *ch the character
* @param int armor_type ARMOR_x
*/
void remove_armor_by_type(char_data *ch, int armor_type) {
	int iter;
	
	for (iter = 0; iter < NUM_WEARS; ++iter) {
		if (GET_EQ(ch, iter) && GET_ARMOR_TYPE(GET_EQ(ch, iter)) == armor_type) {
			act("You take off $p.", FALSE, ch, GET_EQ(ch, iter), NULL, TO_CHAR);
			unequip_char_to_inventory(ch, iter);
		}
	}
}


/**
* Sends the wear message when a person puts an item on.
*
* @param char_data *ch The person wearing the item.
* @param obj_data *obj The item he's wearing.
* @param int where Any WEAR_x position.
*/
static void wear_message(char_data *ch, obj_data *obj, int where) {
	act(wear_data[where].wear_msg_to_room, TRUE, ch, obj, NULL, TO_ROOM);
	act(wear_data[where].wear_msg_to_char, FALSE, ch, obj, NULL, TO_CHAR);
}


 //////////////////////////////////////////////////////////////////////////////
//// DROP HELPERS ////////////////////////////////////////////////////////////

#define VANISH(mode) (mode == SCMD_JUNK ? "  It vanishes in a puff of smoke!" : "")

// returns 0 in all cases except -1 to cancel execution
int perform_drop(char_data *ch, obj_data *obj, byte mode, const char *sname) {
	bool need_capacity = ROOM_BLD_FLAGGED(IN_ROOM(ch), BLD_ITEM_LIMIT) ? TRUE : FALSE;
	char_data *iter;
	int size;
	
	if (!drop_otrigger(obj, ch)) {
		return 0;
	}

	if ((mode == SCMD_DROP) && !drop_wtrigger(obj, ch)) {
		return 0;
	}
	
	// don't let people drop bound items in other people's territory
	if (mode != SCMD_JUNK && OBJ_BOUND_TO(obj) && ROOM_OWNER(IN_ROOM(ch)) && ROOM_OWNER(IN_ROOM(ch)) != GET_LOYALTY(ch)) {
		msg_to_char(ch, "You can't drop bound items here.\r\n");
		return -1;
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
				for (iter = ROOM_PEOPLE(IN_ROOM(ch)); iter; iter = iter->next_in_room) {
					if (iter != ch && !IS_NPC(iter) && !IS_IMMORTAL(iter)) {
						syslog(SYS_GC, GET_ACCESS_LEVEL(ch), TRUE, "ABUSE: %s drops %s with mortal present (%s) at %s", GET_NAME(ch), GET_OBJ_SHORT_DESC(obj), GET_NAME(iter), room_log_identifier(IN_ROOM(ch)));
						break;
					}
				}
			}
			
			return (0);
		case SCMD_JUNK:
			extract_obj(obj);
			return (0);
		default:
			log("SYSERR: Incorrect argument %d passed to perform_drop.", mode);
			break;
	}

	return (0);
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
		if (mode != SCMD_JUNK) {
			/* to prevent coin-bombing */
			command_lag(ch, WAIT_OTHER);
			obj = create_money(type, amount);

			if (!drop_wtrigger(obj, ch)) {
				extract_obj(obj);
				return;
			}

			obj_to_room(obj, IN_ROOM(ch));
			act("You drop $p.", FALSE, ch, obj, NULL, TO_CHAR);
			act("$n drops $p.", FALSE, ch, obj, NULL, TO_ROOM);
		}
		else {
			snprintf(buf, sizeof(buf), "$n drops %s which disappear%s in a puff of smoke!", money_desc(type, amount), (amount == 1 ? "s" : ""));
			act(buf, FALSE, ch, 0, 0, TO_ROOM);

			msg_to_char(ch, "You drop %s which disappear%s in a puff of smoke!\r\n", (amount != 1 ? "some coins" : "a coin"), (amount == 1 ? "s" : ""));
		}
		
		// in any event
		decrease_coins(ch, type, amount);
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
	if (!IS_IMMORTAL(ch) && IN_ROOM(cont) && LAST_OWNER_ID(cont) != idnum && LAST_OWNER_ID(obj) != idnum && !can_use_room(ch, IN_ROOM(ch), GUESTS_ALLOWED)) {
		stealing = TRUE;
		
		if (emp && !can_steal(ch, emp)) {
			// sends own message
			return FALSE;
		}		
	}
	if (mode == FIND_OBJ_INV || can_take_obj(ch, obj)) {
		if (IS_CARRYING_N(ch) + GET_OBJ_INVENTORY_SIZE(obj) > CAN_CARRY_N(ch)) {
			act("$p: you can't hold any more items.", FALSE, ch, obj, 0, TO_CHAR);
			return FALSE;
		}
		else if (get_otrigger(obj, ch)) {
			// last-minute scaling: scale to its minimum (adventures will override this on their own)
			if (OBJ_FLAGGED(obj, OBJ_SCALABLE)) {
				scale_item_to_level(obj, GET_OBJ_MIN_SCALE_LEVEL(obj));
			}
			
			obj_to_char(obj, ch);
			act("You get $p from $P.", FALSE, ch, obj, cont, TO_CHAR);
			act("$n gets $p from $P.", TRUE, ch, obj, cont, TO_ROOM);
			
			if (stealing) {
				if (emp && !skill_check(ch, ABIL_STEAL, DIFF_HARD)) {
					log_to_empire(emp, ELOG_HOSTILITY, "Theft at (%d, %d)", X_COORD(IN_ROOM(ch)), Y_COORD(IN_ROOM(ch)));
				}
				
				GET_STOLEN_TIMER(obj) = time(0);
				trigger_distrust_from_stealth(ch, emp);
				gain_ability_exp(ch, ABIL_STEAL, 50);
			}
			
			get_check_money(ch, obj);
			return TRUE;
		}
	}
	return FALSE;
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
	if (!IS_IMMORTAL(ch) && LAST_OWNER_ID(obj) != idnum && !can_use_room(ch, IN_ROOM(ch), GUESTS_ALLOWED)) {
		stealing = TRUE;
		
		if (emp && !can_steal(ch, emp)) {
			// sends own message
			return FALSE;
		}
	}
	if (can_take_obj(ch, obj) && get_otrigger(obj, ch)) {
		// last-minute scaling: scale to its minimum (adventures will override this on their own)
		if (OBJ_FLAGGED(obj, OBJ_SCALABLE)) {
			scale_item_to_level(obj, GET_OBJ_MIN_SCALE_LEVEL(obj));
		}
		
		obj_to_char(obj, ch);
		act("You get $p.", FALSE, ch, obj, 0, TO_CHAR);
		act("$n gets $p.", TRUE, ch, obj, 0, TO_ROOM);
					
		if (stealing) {
			if (emp && !skill_check(ch, ABIL_STEAL, DIFF_HARD)) {
				log_to_empire(emp, ELOG_HOSTILITY, "Theft at (%d, %d)", X_COORD(IN_ROOM(ch)), Y_COORD(IN_ROOM(ch)));
			}
			
			GET_STOLEN_TIMER(obj) = time(0);
			trigger_distrust_from_stealth(ch, emp);
			gain_ability_exp(ch, ABIL_STEAL, 50);
		}
		
		get_check_money(ch, obj);
		return TRUE;
	}
	return FALSE;
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
		send_to_char("To who?\r\n", ch);
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

	if (IS_CARRYING_N(vict) + GET_OBJ_INVENTORY_SIZE(obj) > CAN_CARRY_N(vict)) {
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

	if (ROOM_BLD_FLAGGED(IN_ROOM(ch), BLD_TAVERN)) {
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
	
	if (ROOM_BLD_FLAGGED(IN_ROOM(ch), BLD_DRINK)) {
		if (!IS_COMPLETE(IN_ROOM(ch))) {
			msg_to_char(ch, "You can't fill your water until it's finished being built.\r\n");
			return;
		}
		act("You gently fill $p with water.", FALSE, ch, obj, 0, TO_CHAR);
		act("$n gently fills $p with water.", TRUE, ch, obj, 0, TO_ROOM);
	}
	else if (ROOM_BLD_FLAGGED(IN_ROOM(ch), BLD_TAVERN)) {
		if (get_room_extra_data(IN_ROOM(ch), ROOM_EXTRA_TAVERN_TYPE) == 0 || !IS_COMPLETE(IN_ROOM(ch))) {
			msg_to_char(ch, "This tavern has nothing on tap.\r\n");
			return;
		}
		else if (get_room_extra_data(IN_ROOM(ch), ROOM_EXTRA_TAVERN_BREWING_TIME) > 0) {
			msg_to_char(ch, "The tavern is brewing up a new batch. Try back later.\r\n");
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
	extern const int wear_significance[];
	
	int total_share, bonus, iter, amt;
	int room_lev = 0, room_min = 0, room_max = 0, sig;
	double share, this_share, points_to_give, per_point;
	room_data *room = NULL;
	obj_data *top_obj;
	bitvector_t bits;
	
	// configure this here
	double scale_points_at_100 = config_get_double("scale_points_at_100");
	extern const double obj_flag_scaling_bonus[];	// see constants.c
	extern const double armor_scale_bonus[NUM_ARMOR_TYPES];	// see constants.c
	
	// WEAR_POS_x: modifier based on best wear type
	const double wear_pos_modifier[] = { 0.75, 1.0 };

	if (!OBJ_FLAGGED(obj, OBJ_SCALABLE)) {
		log("SYSERR: Attempting to scale item which is not scalable or is already scaled: %s", GET_OBJ_SHORT_DESC(obj));
		return;
	}
	
	// determine any scale constraints from the room
	top_obj = get_top_object(obj);
	if (IN_ROOM(top_obj)) {
		room = IN_ROOM(top_obj);
	}
	else if (top_obj->carried_by) {
		room = IN_ROOM(top_obj->carried_by);
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
	else if (room_max > 0) {
		level = MIN(level, room_max);
	}
	
	if (GET_OBJ_MIN_SCALE_LEVEL(obj) > 0) {
		level = MAX(level, GET_OBJ_MIN_SCALE_LEVEL(obj));
	}
	else if (room_min > 0) {
		level = MAX(level, room_min);
	}
	
	// hard lower limit -- the stats are the same at 0 or 1, but 0 shows as "unscalable" because unscalable items have 0 scale level
	level = MAX(1, level);
	
	// scale data
	REMOVE_BIT(GET_OBJ_EXTRA(obj), OBJ_SCALABLE);
	GET_OBJ_CURRENT_SCALE_LEVEL(obj) = level;
	
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
	for (iter = 0; iter < MAX_OBJ_AFFECT; ++iter) {
		if (obj->affected[iter].location != APPLY_NONE && obj->affected[iter].location != APPLY_GREATNESS) {
			SHARE_OR_BONUS(obj->affected[iter].modifier);
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
		case ITEM_FOOD: {
			SHARE_OR_BONUS(GET_FOOD_HOURS_OF_FULLNESS(obj));
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
					points_to_give -= round(this_share * GET_WEAPON_DAMAGE_BONUS(obj));
				}
				GET_OBJ_VAL(obj, VAL_WEAPON_DAMAGE_BONUS) = amt;
			}
			// leave negatives alone
			break;
		}
		case ITEM_DRINKCON: {
			amt = (int)round(this_share * GET_DRINK_CONTAINER_CAPACITY(obj) * config_get_double("scale_drink_capacity"));
			if (amt > 0) {
				points_to_give -= round(this_share * GET_DRINK_CONTAINER_CAPACITY(obj));
			}
			GET_OBJ_VAL(obj, VAL_DRINK_CONTAINER_CAPACITY) = amt;
			GET_OBJ_VAL(obj, VAL_DRINK_CONTAINER_CONTENTS) = amt;
			// negatives aren't even possible here
			break;
		}
		case ITEM_FOOD: {
			amt = (int)round(this_share * GET_FOOD_HOURS_OF_FULLNESS(obj) * config_get_double("scale_food_fullness"));
			if (amt > 0) {
				points_to_give -= round(this_share * GET_FOOD_HOURS_OF_FULLNESS(obj));
			}
			GET_OBJ_VAL(obj, VAL_FOOD_HOURS_OF_FULLNESS) = amt;
			// negatives aren't possible here
			break;
		}
		case ITEM_COINS: {
			amt = (int)round(this_share * GET_COINS_AMOUNT(obj) * config_get_double("scale_coin_amount"));
			if (amt > 0) {
				points_to_give -= round(this_share * GET_COINS_AMOUNT(obj));
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
					points_to_give -= round(this_share * GET_MISSILE_WEAPON_DAMAGE(obj));
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
					points_to_give -= round(this_share * GET_ARROW_DAMAGE_BONUS(obj));
				}
				GET_OBJ_VAL(obj, VAL_ARROW_DAMAGE_BONUS) = amt;
			}
			// leave negatives alone
			break;
		}
		case ITEM_PACK: {
			amt = (int)round(this_share * GET_PACK_CAPACITY(obj) * config_get_double("scale_pack_size"));
			if (amt > 0) {
				points_to_give -= round(this_share * GET_PACK_CAPACITY(obj));
			}
			GET_OBJ_VAL(obj, VAL_PACK_CAPACITY) = amt;
			// negatives aren't really possible here
			break;
		}
		case ITEM_POTION: {
			// aiming for a scale of 100 at 100 -- and eliminate the wear_pos_modifier since potions are almost always WEAR_TAKE
			amt = (int)round(this_share * GET_POTION_SCALE(obj) * (100.0 / scale_points_at_100) / wear_pos_modifier[wear_significance[ITEM_WEAR_TAKE]]);
			if (amt > 0) {
				points_to_give -= round(this_share * GET_POTION_SCALE(obj));
			}
			GET_OBJ_VAL(obj, VAL_POTION_SCALE) = amt;
			// negatives aren't really possible here
			break;
		}
	}
	
	// distribute points: applies
	for (iter = 0; iter < MAX_OBJ_AFFECT; ++iter) {
		if (obj->affected[iter].location != APPLY_NONE && obj->affected[iter].location != APPLY_GREATNESS) {
			this_share = MAX(0, MIN(share, points_to_give));
			// raw amount
			per_point = (1.0 / apply_values[(int)obj->affected[iter].location]);
			
			if (obj->affected[iter].modifier > 0) {
				// positive benefit
				amt = round(this_share * obj->affected[iter].modifier * per_point);
				points_to_give -= round(this_share * obj->affected[iter].modifier);
				obj->affected[iter].modifier = amt;
			}
			else if (obj->affected[iter].modifier < 0) {
				// penalty: does not cost from points_to_give
				obj->affected[iter].modifier = round(obj->affected[iter].modifier * per_point);
			}
		}
	}
	
	// cleanup
	#undef SHARE_OR_BONUS
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
		
		// if it's just collectable coins, don't show
		if (IS_SET(tpd->state, TPD_BOUGHT) && IS_SET(tpd->state, TPD_COINS_PENDING)) {
			to_collect += round(tpd->buy_cost * (1.0 - trading_post_fee)) + tpd->post_cost;
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
		
		snprintf(line, sizeof(line), "%s%2d. %s: %d coin%s [%.1f]%s%s&0\r\n", IS_SET(tpd->state, TPD_EXPIRED) ? "&r" : "", ++count, GET_OBJ_SHORT_DESC(tpd->obj), tpd->buy_cost, PLURAL(tpd->buy_cost), rate_item(tpd->obj), scale, IS_SET(tpd->state, TPD_EXPIRED) ? " (expired)" : "");
		
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
		
		snprintf(line, sizeof(line), "%s%2d. %s: %d%s %s [%.1f]%s%s%s%s%s&0\r\n", (tpd->player == GET_IDNUM(ch)) ? "&r" : (can_wear ? "" : "&R"), ++count, GET_OBJ_SHORT_DESC(tpd->obj), tpd->buy_cost, exchange, (coin_emp ? EMPIRE_ADJECTIVE(coin_emp) : "misc"), rate_item(tpd->obj), scale, (OBJ_FLAGGED(tpd->obj, OBJ_SUPERIOR) ? " (sup)" : ""), OBJ_FLAGGED(tpd->obj, OBJ_ENCHANTED) ? " (ench)" : "", (tpd->player == GET_IDNUM(ch)) ? " (your auction)" : "", can_wear ? "" : " (can't use)");
		
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
		if (IS_CARRYING_N(ch) + GET_OBJ_INVENTORY_SIZE(tpd->obj) > CAN_CARRY_N(ch)) {
			msg_to_char(ch, "Your inventory is too full to buy that.\r\n");
			return;
		}
		
		// pay up
		charge_coins(ch, coin_emp, tpd->buy_cost);
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
				if (IS_CARRYING_N(ch) + GET_OBJ_INVENTORY_SIZE(tpd->obj) > CAN_CARRY_N(ch)) {
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
		msg_to_char(ch, "It costs 1 coin per gear level to post an item (minimum 1; refundable).\r\n");
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
	else if ((cost = atoi(costarg)) < 1) {
		msg_to_char(ch, "You must charge at least 1 coin.\r\n");
	}
	else if (*timearg && (length = atoi(timearg)) < 1) {
		msg_to_char(ch, "You can't set the time for less than 1 hour.\r\n");
	}
	else if (length > config_get_int("trading_post_max_hours")) {
		msg_to_char(ch, "The longest you can set it is %d hours.\r\n", config_get_int("trading_post_max_hours"));
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
		
		charge_coins(ch, GET_LOYALTY(ch), post_cost);
		
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

	char arg[MAX_INPUT_LENGTH], output[MAX_STRING_LENGTH*2], line[MAX_STRING_LENGTH], part[256], flags[256], quantity[256], *tmp;
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
		
		if (iter->flags) {
			prettier_sprintbit(iter->flags, unique_storage_flags, flags);
			snprintf(part, sizeof(part), " (%s)", flags);
		}
		else {
			*part = '\0';
		}
		
		if (iter->amount != 1) {
			snprintf(quantity, sizeof(quantity), "(%d) ", iter->amount);
		}
		else {
			*quantity = '\0';
		}
		
		// build line
		snprintf(line, sizeof(line), "%2d. %s%s%s\r\n", ++num, quantity, GET_OBJ_SHORT_DESC(iter->obj), part);
		
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
	if (!imm_access && (!ROOM_BLD_FLAGGED(IN_ROOM(ch), BLD_WAREHOUSE | BLD_VAULT) || !IS_COMPLETE(IN_ROOM(ch)))) {
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
	extern int max_obj_id;
	
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
	if (!imm_access && (!ROOM_BLD_FLAGGED(IN_ROOM(ch), BLD_WAREHOUSE | BLD_VAULT) || !IS_COMPLETE(IN_ROOM(ch)))) {
		msg_to_char(ch, "You can't do that here.\r\n");
		return;
	}
	if (!imm_access && (!can_use_room(ch, IN_ROOM(ch), GUESTS_ALLOWED) || (room_emp && GET_LOYALTY(ch) != room_emp && !has_relationship(GET_LOYALTY(ch), room_emp, DIPL_TRADE)))) {
		msg_to_char(ch, "You don't have permission to do that here.\r\n");
		return;
	}
	if (!imm_access && ROOM_BLD_FLAGGED(IN_ROOM(ch), BLD_VAULT) && !has_permission(ch, PRIV_WITHDRAW)) {
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
		msg_to_char(ch, "Retreive %swhat?\r\n", all ? "all of " : "");
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
		if (IS_SET(iter->flags, EUS_VAULT) && !imm_access && !ROOM_BLD_FLAGGED(IN_ROOM(ch), BLD_VAULT)) {
			msg_to_char(ch, "You need to be in a vault to retrieve %s.\r\n", GET_OBJ_SHORT_DESC(iter->obj));
			return;
		}
		
		// load the actual objs
		while (!done && iter->amount > 0 && (all || amt-- > 0)) {
			if (iter->obj && IS_CARRYING_N(ch) + GET_OBJ_INVENTORY_SIZE(iter->obj) > CAN_CARRY_N(ch)) {
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
				GET_ID(obj) = max_obj_id++;
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
		save_empire(GET_LOYALTY(ch));
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
	int total = 1, done = 0, count = 0, dotmode;
	bool full = FALSE;
	
	if (!*argument) {
		msg_to_char(ch, "Store what?\r\n");
		return;
	}
	
	// access permission
	if (!imm_access && (!ROOM_BLD_FLAGGED(IN_ROOM(ch), BLD_WAREHOUSE | BLD_VAULT) || !IS_COMPLETE(IN_ROOM(ch)))) {
		msg_to_char(ch, "You can't do that here.\r\n");
		return;
	}
	if (!imm_access && (!can_use_room(ch, IN_ROOM(ch), GUESTS_ALLOWED) || (room_emp && GET_LOYALTY(ch) != room_emp && !has_relationship(GET_LOYALTY(ch), room_emp, DIPL_TRADE)))) {
		msg_to_char(ch, "You don't have permission to do that here.\r\n");
		return;
	}
	if (!imm_access && ROOM_BLD_FLAGGED(IN_ROOM(ch), BLD_VAULT) && !has_permission(ch, PRIV_WITHDRAW)) {
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

		while (obj && (dotmode == FIND_ALLDOT || count < total)) {
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
		save_empire(GET_LOYALTY(ch));
	}
}


 //////////////////////////////////////////////////////////////////////////////
//// COMMANDS ////////////////////////////////////////////////////////////////

ACMD(do_draw) {
	obj_data *obj = NULL;
	int loc = 0;

	one_argument(argument, arg);

	if (!*arg) {
		msg_to_char(ch, "Draw what?\r\n");
		return;
	}

	if ((obj = GET_EQ(ch, WEAR_SHEATH_1)) && isname(arg, GET_OBJ_KEYWORDS(obj)))
		loc = WEAR_SHEATH_1;
	else if ((obj = GET_EQ(ch, WEAR_SHEATH_2)) && isname(arg, GET_OBJ_KEYWORDS(obj)))
		loc = WEAR_SHEATH_2;
	else {
		msg_to_char(ch, "You have nothing by that name sheathed!\r\n");
		return;
	}
	
	if (OBJ_FLAGGED(obj, OBJ_TWO_HANDED) && GET_EQ(ch, WEAR_HOLD)) {
		msg_to_char(ch, "You can't draw a two-handed weapon while you're holding something in your off-hand.\r\n");
		return;
	}

	// attempt to remove existing wield
	if (GET_EQ(ch, WEAR_WIELD)) {
		perform_remove(ch, WEAR_WIELD);
		
		// did it work? if not, player got an error
		if (GET_EQ(ch, WEAR_WIELD)) {
			return;
		}
	}

	// SUCCESS
	// do not use unequip_char_to_inventory so that we don't hit OBJ_SINGLE_USE
	obj_to_char(unequip_char(ch, loc), ch);
	perform_wear(ch, obj, WEAR_WIELD);
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
		else if (ROOM_BLD_FLAGGED(IN_ROOM(ch), BLD_DRINK)) {
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

	if (type == NOTHING && !(obj = get_obj_in_list_vis(ch, arg, ch->carrying))) {
		if (ROOM_BLD_FLAGGED(IN_ROOM(ch), BLD_DRINK) && (is_abbrev(arg, "water") || isname(arg, get_room_name(IN_ROOM(ch), FALSE)))) {
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
	
	char buf[MAX_STRING_LENGTH];
	char_data *vict;
	obj_data *food;
	int amount;
	bool extract = FALSE;

	one_argument(argument, arg);

	if (REAL_NPC(ch))		/* Cannot use GET_COND() on mobs. */
		return;

	if (!*arg) {
		send_to_char("Eat what?\r\n", ch);
		return;
	}
	if (!(food = get_obj_in_list_vis(ch, arg, ch->carrying))) {
		if (!(food = get_obj_in_list_vis(ch, arg, ROOM_CONTENTS(IN_ROOM(ch))))) {
			// special case: Taste Blood
			if (subcmd == SCMD_TASTE && IS_VAMPIRE(ch) && HAS_ABILITY(ch, ABIL_TASTE_BLOOD) && (vict = get_char_vis(ch, arg, FIND_CHAR_ROOM))) {
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
	if (GET_COND(ch, FULL) < 75 && GET_COND(ch, FULL) != UNLIMITED) {	/* Stomach full */
		send_to_char("You are too full to eat more!\r\n", ch);
		return;
	}

	/* check trigger */
	if (!consume_otrigger(food, ch, OCMD_EAT)) {
		return;
	}

	// go
	amount = (subcmd == SCMD_EAT ? (GET_FOOD_HOURS_OF_FULLNESS(food) * REAL_UPDATES_PER_MUD_HOUR) : REAL_UPDATES_PER_MUD_HOUR);
	amount = MAX(0, MIN(GET_COND(ch, FULL), amount));

	gain_condition(ch, FULL, -1 * amount);
	
	// subtract values first
	if (IS_FOOD(food)) {
		GET_OBJ_VAL(food, VAL_FOOD_HOURS_OF_FULLNESS) -= amount / REAL_UPDATES_PER_MUD_HOUR;
	}

	// prepare for messaging
	if (IS_FOOD(food) && GET_FOOD_HOURS_OF_FULLNESS(food) <= 0) {
		// end of the food
		extract = TRUE;
	}
	
	// messaging
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

	// additional messages
	if (GET_COND(ch, FULL) < 75) {
		send_to_char("You are full.\r\n", ch);
	}

	if (extract) {
		extract_obj(food);
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
	else if (!ROOM_BLD_FLAGGED(IN_ROOM(ch), BLD_MINT | BLD_VAULT)) {
		msg_to_char(ch, "You can't exchange treasure for coins here.\r\n");
	}
	else if (!IS_COMPLETE(IN_ROOM(ch))) {
		msg_to_char(ch, "You need to finish building first.\r\n");
	}
	else if (!(emp = ROOM_OWNER(IN_ROOM(ch)))) {
		msg_to_char(ch, "This building does not belong to any empire, and can't exchange coins.\r\n");
	}
	else if (!EMPIRE_HAS_TECH(emp, TECH_COMMERCE)) {
		msg_to_char(ch, "This empire does not have Commerce, and cannot exchnage coins.\r\n");
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
	char arg1[MAX_INPUT_LENGTH], arg2[MAX_INPUT_LENGTH], arg3[MAX_INPUT_LENGTH];
	int cont_dotmode, found = 0, mode;
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
			mode = generic_find(arg2, FIND_OBJ_INV | FIND_OBJ_ROOM | FIND_OBJ_EQUIP, ch, &tmp_char, &cont);
			if (!cont) {
				sprintf(buf, "You don't have %s %s.\r\n", AN(arg2), arg2);
				send_to_char(buf, ch);
			}
			else if (GET_OBJ_TYPE(cont) != ITEM_CONTAINER && GET_OBJ_TYPE(cont) != ITEM_CART && !IS_CORPSE(cont))
				act("$p is not a container.", FALSE, ch, cont, 0, TO_CHAR);
			else
				get_from_container(ch, cont, arg1, mode, amount);
		}
		else {
			if (cont_dotmode == FIND_ALLDOT && !*arg2) {
				send_to_char("Get from all of what?\r\n", ch);
				return;
			}
			for (cont = ch->carrying; cont; cont = cont->next_content) {
				if (CAN_SEE_OBJ(ch, cont) && (cont_dotmode == FIND_ALL || isname(arg2, GET_OBJ_KEYWORDS(cont)))) {
					if (GET_OBJ_TYPE(cont) == ITEM_CONTAINER || GET_OBJ_TYPE(cont) == ITEM_CART || IS_CORPSE(cont)) {
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
					if (GET_OBJ_TYPE(cont) == ITEM_CONTAINER || GET_OBJ_TYPE(cont) == ITEM_CART || IS_CORPSE(cont)) {
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
		send_to_char("Give what to who?\r\n", ch);
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
	if (!*arg)
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
	obj_data *obj;
	
	one_argument(argument, arg);

	if (!*arg) {
		msg_to_char(ch, "Identify what object?\r\n");
	}
	else if (!generic_find(arg, FIND_OBJ_INV | FIND_OBJ_ROOM | FIND_OBJ_EQUIP, ch, &tmp_char, &obj)) {
		msg_to_char(ch, "You see nothing like that here.\r\n");
	}
	else {
		charge_ability_cost(ch, NOTHING, 0, NOTHING, 0, WAIT_OTHER);
		identify_obj_to_char(obj, ch);
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
	obj_data *obj, *flint = NULL;
	bool magic = !IS_NPC(ch) && HAS_ABILITY(ch, ABIL_TOUCH_OF_FLAME);

	one_argument(argument, arg);

	if (!magic) {
		flint = get_obj_in_list_num(o_FLINT_SET, ch->carrying);
	}

	if (!*arg)
		msg_to_char(ch, "Light what?\r\n");
	else if (!IS_NPC(ch) && !magic && !flint)
		msg_to_char(ch, "You don't have a flint set to light that with.\r\n");
	else if (!(obj = get_obj_in_list_vis(ch, arg, ch->carrying)) && !(obj = get_obj_in_list_vis(ch, arg, ROOM_CONTENTS(IN_ROOM(ch)))))
		msg_to_char(ch, "You don't have a %s.\r\n", arg);
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
			if (IS_COMPLETE(IN_ROOM(ch)) && ROOM_BLD_FLAGGED(IN_ROOM(ch), BLD_DRINK | BLD_TAVERN)) {
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
		if (is_abbrev(arg2, "water") && ROOM_BLD_FLAGGED(IN_ROOM(ch), BLD_DRINK)) {
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
	char arg1[MAX_INPUT_LENGTH], arg2[MAX_INPUT_LENGTH], arg3[MAX_INPUT_LENGTH];
	obj_data *obj, *next_obj, *cont;
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
		generic_find(thecont, FIND_OBJ_INV | FIND_OBJ_ROOM | FIND_OBJ_EQUIP, ch, &tmp_char, &cont);
		if (!cont) {
			sprintf(buf, "You don't see %s %s here.\r\n", AN(thecont), thecont);
			send_to_char(buf, ch);
		}
		else if (GET_OBJ_TYPE(cont) != ITEM_CONTAINER && GET_OBJ_TYPE(cont) != ITEM_CART)
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
					msg_to_char(ch, "You can't withdraw that!\r\n");
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
						msg_to_char(ch, "You can't withdraw that!\r\n");
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

	/* save the empire */
	SAVE_CHAR(ch);
	save_empire(emp);
	read_vault(emp);
}


ACMD(do_roadsign) {
	obj_data *sign;
	int max_length = 70;	// "Sign: " prefix; don't want to go over 80
	bool old_sign = FALSE;
	
	skip_spaces(&argument);
	
	if (IS_NPC(ch)) {
		msg_to_char(ch, "NPCs can't use roadsign.\r\n");
	}
	else if (GET_SKILL(ch, SKILL_EMPIRE) <= BASIC_SKILL_CAP) {
		msg_to_char(ch, "You need the Roads ability and an Empire skill of at least %d to set up road signs.\r\n", BASIC_SKILL_CAP+1);
	}
	else if (!HAS_ABILITY(ch, ABIL_ROADS)) {
		msg_to_char(ch, "You must purchase the Roads ability to set up road signs.\r\n");
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
	else if ((strlen(argument) - (2 * count_color_codes(argument))) > max_length) {
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

	/* save the empire */
	SAVE_CHAR(ch);
	save_empire(emp);
	read_vault(emp);
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
	else if ((!ROOM_BLD_FLAGGED(IN_ROOM(ch), BLD_TRADE) || !IS_COMPLETE(IN_ROOM(ch))) && !IS_IMMORTAL(ch)) {
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
