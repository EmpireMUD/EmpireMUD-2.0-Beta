/* ************************************************************************
*   File: olc.object.c                                    EmpireMUD 2.0b5 *
*  Usage: OLC for items                                                   *
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
#include "interpreter.h"
#include "db.h"
#include "comm.h"
#include "olc.h"
#include "skills.h"
#include "handler.h"
#include "dg_scripts.h"

/**
* Contents:
*   Helpers
*   Displays
*   Edit Modules
*/

// externs
extern const char *action_bits[];
extern const char *affected_bits[];
extern const char *apply_type_names[];
extern const char *apply_types[];
extern const char *armor_types[NUM_ARMOR_TYPES+1];
extern const char *component_flags[];
extern const char *component_types[];
extern const char *container_bits[];
extern const char *extra_bits[];
extern const char *interact_types[];
extern const char *item_types[];
extern const struct material_data materials[NUM_MATERIALS];
extern const char *obj_custom_types[];
extern const char *offon_types[];
extern const char *paint_colors[];
extern const char *paint_names[];
extern const char *size_types[];
extern const char *storage_bits[];
extern const char *wear_bits[];

// external funcs
extern double get_base_dps(obj_data *weapon);
extern double get_weapon_speed(obj_data *weapon);

// locals
const char *default_obj_keywords = "object new";
const char *default_obj_short = "a new object";
const char *default_obj_long = "A new object is sitting here.";

char **get_weapon_types_string();

// data
char **olc_material_list = NULL;	// used for olc


 //////////////////////////////////////////////////////////////////////////////
//// HELPERS /////////////////////////////////////////////////////////////////

/**
* Checks for common object problems and reports them to ch.
*
* @param obj_data *obj The item to audit.
* @param char_data *ch The person to report to.
* @return bool TRUE if any problems were reported; FALSE if all good.
*/
bool audit_object(obj_data *obj, char_data *ch) {
	extern bool audit_interactions(any_vnum vnum, struct interaction_item *list, int attach_type, char_data *ch);
	extern adv_data *get_adventure_for_vnum(rmt_vnum vnum);
	
	bool is_adventure = (get_adventure_for_vnum(GET_OBJ_VNUM(obj)) != NULL);
	char temp[MAX_STRING_LENGTH], *ptr;
	bool problem = FALSE;
	
	if (!GET_OBJ_KEYWORDS(obj) || !*GET_OBJ_KEYWORDS(obj) || !str_cmp(GET_OBJ_KEYWORDS(obj), default_obj_keywords)) {
		olc_audit_msg(ch, GET_OBJ_VNUM(obj), "Keywords not set");
		problem = TRUE;
	}
	else {
		ptr = GET_OBJ_KEYWORDS(obj);
		do {
			ptr = any_one_arg(ptr, temp);
			if (*temp && !str_str(GET_OBJ_SHORT_DESC(obj), temp) && !str_str(GET_OBJ_LONG_DESC(obj), temp)) {
				olc_audit_msg(ch, GET_OBJ_VNUM(obj), "Keyword '%s' not found in strings", temp);
				problem = TRUE;
			}
		} while (*ptr);
	}
	
	if (!str_cmp(GET_OBJ_LONG_DESC(obj), default_obj_long)) {
		olc_audit_msg(ch, GET_OBJ_VNUM(obj), "Long desc not set");
		problem = TRUE;
	}
	if (!ispunct(GET_OBJ_LONG_DESC(obj)[strlen(GET_OBJ_LONG_DESC(obj)) - 1])) {
		olc_audit_msg(ch, GET_OBJ_VNUM(obj), "Long desc missing punctuation");
		problem = TRUE;
	}
	if (islower(*GET_OBJ_LONG_DESC(obj))) {
		olc_audit_msg(ch, GET_OBJ_VNUM(obj), "Long desc not capitalized");
		problem = TRUE;
	}
	if (!str_cmp(GET_OBJ_SHORT_DESC(obj), default_obj_short)) {
		olc_audit_msg(ch, GET_OBJ_VNUM(obj), "Short desc not set");
		problem = TRUE;
	}
	any_one_arg(GET_OBJ_SHORT_DESC(obj), temp);
	if ((fill_word(temp) || reserved_word(temp)) && isupper(*temp)) {
		olc_audit_msg(ch, GET_OBJ_VNUM(obj), "Short desc capitalized");
		problem = TRUE;
	}
	if (ispunct(GET_OBJ_SHORT_DESC(obj)[strlen(GET_OBJ_SHORT_DESC(obj)) - 1])) {
		olc_audit_msg(ch, GET_OBJ_VNUM(obj), "Short desc has punctuation");
		problem = TRUE;
	}
	
	ptr = GET_OBJ_SHORT_DESC(obj);
	do {
		ptr = any_one_arg(ptr, temp);
		// remove trailing punctuation
		while (*temp && ispunct(temp[strlen(temp)-1])) {
			temp[strlen(temp)-1] = '\0';
		}
		if (*temp && !fill_word(temp) && !reserved_word(temp) && !isname(temp, GET_OBJ_KEYWORDS(obj))) {
			olc_audit_msg(ch, GET_OBJ_VNUM(obj), "Suggested missing keyword '%s'", temp);
			problem = TRUE;
		}
	} while (*ptr);
	
	if (!GET_OBJ_ACTION_DESC(obj) || !*GET_OBJ_ACTION_DESC(obj) || !str_cmp(GET_OBJ_ACTION_DESC(obj), "Nothing.\r\n")) {
		olc_audit_msg(ch, GET_OBJ_VNUM(obj), "Look desc not set");
		problem = TRUE;
	}
	else if (!strn_cmp(GET_OBJ_ACTION_DESC(obj), "Nothing.", 8)) {
		olc_audit_msg(ch, GET_OBJ_VNUM(obj), "Look desc starting with 'Nothing.'");
		problem = TRUE;
	}
	if (OBJ_FLAGGED(obj, OBJ_CREATABLE)) {
		olc_audit_msg(ch, GET_OBJ_VNUM(obj), "CREATABLE");
		problem = TRUE;
	}
	if (OBJ_FLAGGED(obj, OBJ_LIGHT) && GET_OBJ_TIMER(obj) <= 0) {
		olc_audit_msg(ch, GET_OBJ_VNUM(obj), "Infinite light");
		problem = TRUE;
	}
	if (OBJ_FLAGGED(obj, OBJ_HARD_DROP | OBJ_GROUP_DROP)) {
		olc_audit_msg(ch, GET_OBJ_VNUM(obj), "Loot quality flags set");
		problem = TRUE;
	}
	if (OBJ_FLAGGED(obj, OBJ_TWO_HANDED) && CAN_WEAR(obj, ITEM_WEAR_HOLD)) {
		olc_audit_msg(ch, GET_OBJ_VNUM(obj), "2-handed + holdable");
		problem = TRUE;
	}
	if (OBJ_FLAGGED(obj, OBJ_KEEP)) {
		olc_audit_msg(ch, GET_OBJ_VNUM(obj), "KEEP flag");
		problem = TRUE;
	}
	if (OBJ_FLAGGED(obj, OBJ_BIND_ON_EQUIP) && OBJ_FLAGGED(obj, OBJ_BIND_ON_PICKUP)) {
		olc_audit_msg(ch, GET_OBJ_VNUM(obj), "Two bind flags");
		problem = TRUE;
	}
	if (!is_adventure && OBJ_FLAGGED(obj, OBJ_SCALABLE) && GET_OBJ_MAX_SCALE_LEVEL(obj) == 0) {
		olc_audit_msg(ch, GET_OBJ_VNUM(obj), "No maximum scale level on non-adventure obj");
		problem = TRUE;
	}
	if (OBJ_FLAGGED(obj, OBJ_SCALABLE) && obj->storage && GET_OBJ_MAX_SCALE_LEVEL(obj) != GET_OBJ_MIN_SCALE_LEVEL(obj)) {
		olc_audit_msg(ch, GET_OBJ_VNUM(obj), "Scalable object has storage locations");
		problem = TRUE;
	}
	if (OBJ_FLAGGED(obj, OBJ_SCALABLE) && !OBJ_FLAGGED(obj, OBJ_BIND_ON_EQUIP | OBJ_BIND_ON_PICKUP)) {
		olc_audit_msg(ch, GET_OBJ_VNUM(obj), "Scalable object has no bind flags");
		problem = TRUE;
	}
	if (OBJ_FLAGGED(obj, OBJ_NO_STORE)) {
		olc_audit_msg(ch, GET_OBJ_VNUM(obj), "Object has !STORE - this flag is meaningless on a prototype");
		problem = TRUE;
	}
	
	problem |= audit_interactions(GET_OBJ_VNUM(obj), obj->interactions, TYPE_OBJ, ch);
	
	// ITEM_X: auditors
	switch (GET_OBJ_TYPE(obj)) {
		case ITEM_COINS: {
			if (GET_COINS_AMOUNT(obj) == 0) {
				olc_audit_msg(ch, GET_OBJ_VNUM(obj), "Coin amount not set");
				problem = TRUE;
			}
			break;
		}
		case ITEM_CURRENCY: {
			if (GET_CURRENCY_AMOUNT(obj) == 0) {
				olc_audit_msg(ch, GET_OBJ_VNUM(obj), "Currency amount not set");
				problem = TRUE;
			}
			if (!find_generic(GET_CURRENCY_VNUM(obj), GENERIC_CURRENCY)) {
				olc_audit_msg(ch, GET_OBJ_VNUM(obj), "Currency vnum not set");
				problem = TRUE;
			}
			break;
		}
		case ITEM_WEAPON: {
			if (GET_WEAPON_TYPE(obj) == TYPE_RESERVED) {
				olc_audit_msg(ch, GET_OBJ_VNUM(obj), "Weapon type not set");
				problem = TRUE;
			}
			if (GET_WEAPON_DAMAGE_BONUS(obj) == 0) {
				olc_audit_msg(ch, GET_OBJ_VNUM(obj), "Damage bonus not set");
				problem = TRUE;
			}
			break;
		}
		case ITEM_CONTAINER: {
			if (GET_MAX_CONTAINER_CONTENTS(obj) == 0) {
				olc_audit_msg(ch, GET_OBJ_VNUM(obj), "Container capacity not set");
				problem = TRUE;
			}
			break;
		}
		case ITEM_DRINKCON: {
			if (GET_DRINK_CONTAINER_CAPACITY(obj) == 0) {
				olc_audit_msg(ch, GET_OBJ_VNUM(obj), "Drink container capacity not set");
				problem = TRUE;
			}
			break;
		}
		case ITEM_FOOD: {
			if (GET_FOOD_HOURS_OF_FULLNESS(obj) == 0) {
				olc_audit_msg(ch, GET_OBJ_VNUM(obj), "Food fullness not set");
				problem = TRUE;
			}
			break;
		}
		case ITEM_MISSILE_WEAPON: {
			if (GET_MISSILE_WEAPON_DAMAGE(obj) == 0) {
				olc_audit_msg(ch, GET_OBJ_VNUM(obj), "Damage amount not set");
				problem = TRUE;
			}
			break;
		}
		case ITEM_AMMO: {
			if (GET_AMMO_QUANTITY(obj) == 0) {
				olc_audit_msg(ch, GET_OBJ_VNUM(obj), "Ammo quantity not set");
				problem = TRUE;
			}
			break;
		}
		case ITEM_PACK: {
			if (GET_PACK_CAPACITY(obj) == 0) {
				olc_audit_msg(ch, GET_OBJ_VNUM(obj), "Pack capacity not set");
				problem = TRUE;
			}
			break;
		}
		case ITEM_PAINT: {
			if (!GET_PAINT_COLOR(obj)) {
				olc_audit_msg(ch, GET_OBJ_VNUM(obj), "Paint color not set");
				problem = TRUE;
			}
			break;
		}
		case ITEM_POISON: {
			if (GET_POISON_CHARGES(obj) == 0) {
				olc_audit_msg(ch, GET_OBJ_VNUM(obj), "Poison charges not set");
				problem = TRUE;
			}
			break;
		}
		case ITEM_RECIPE: {
			craft_data *craft = craft_proto(GET_RECIPE_VNUM(obj));
			if (GET_RECIPE_VNUM(obj) <= 0 || !craft) {
				olc_audit_msg(ch, GET_OBJ_VNUM(obj), "Invalid recipe vnum");
				problem = TRUE;
			}
			if (craft && !CRAFT_FLAGGED(craft, CRAFT_LEARNED)) {
				olc_audit_msg(ch, GET_OBJ_VNUM(obj), "Recipe is not set LEARNED");
				problem = TRUE;
			}
			break;
		}
		case ITEM_BOOK: {
			if (!book_proto(GET_BOOK_ID(obj))) {
				olc_audit_msg(ch, GET_OBJ_VNUM(obj), "Book type invalid");
				problem = TRUE;
			}
			break;
		}
		case ITEM_WEALTH: {
			if (GET_WEALTH_VALUE(obj) == 0) {
				olc_audit_msg(ch, GET_OBJ_VNUM(obj), "Wealth value not set");
				problem = TRUE;
			}
			break;
		}
		case ITEM_LIGHTER: {
			if (GET_LIGHTER_USES(obj) == 0) {
				olc_audit_msg(ch, GET_OBJ_VNUM(obj), "Number of uses not set");
				problem = TRUE;
			}
			break;
		}
		case ITEM_MINIPET: {
			if (GET_MINIPET_VNUM(obj) == NOTHING || !mob_proto(GET_MINIPET_VNUM(obj))) {
				olc_audit_msg(ch, GET_OBJ_VNUM(obj), "Mini-pet not set");
				problem = TRUE;
			}
			break;
		}
		case ITEM_UNDEFINED: {
			olc_audit_msg(ch, GET_OBJ_VNUM(obj), "Type is UNDEFINED");
			problem = TRUE;
			break;
		}
	}
	
	return problem;
}


/**
* Converts material data to a list that can be used by search_block and olc
* functions.
*
* TODO: Could macro this process since it is also used by weapons, etc.
*/
void check_oedit_material_list(void) {
	int iter;
	
	// this does not have a const char** list .. build one the first time
	if (!olc_material_list) {
		CREATE(olc_material_list, char*, NUM_MATERIALS+1);
		
		for (iter = 0; iter < NUM_MATERIALS; ++iter) {
			olc_material_list[iter] = str_dup(materials[iter].name);
		}
		
		// must terminate
		olc_material_list[NUM_MATERIALS] = str_dup("\n");
	}
}


/**
* Creates a new object entry.
*
* @param obj_vnum vnum The number to create.
* @return obj_data* The new object's prototype.
*/
obj_data *create_obj_table_entry(obj_vnum vnum) {
	void add_object_to_table(obj_data *obj);
	
	obj_data *obj;
	
	// sanity
	if (obj_proto(vnum)) {
		log("SYSERR: Attempting to insert object at existing vnum %d", vnum);
		return obj_proto(vnum);
	}
	
	CREATE(obj, obj_data, 1);
	clear_object(obj);
	obj->vnum = vnum;
	add_object_to_table(obj);
	
	// save obj index and obj file now so there's no trouble later
	save_index(DB_BOOT_OBJ);
	save_library_file_for_vnum(DB_BOOT_OBJ, vnum);

	return obj;
}


/**
* @return char** A "\n"-terminated list of weapon types.
*/
char **get_weapon_types_string(void) {
	static char **wtypes = NULL;
	int iter;
	
	// this does not have a const char** list .. build one the first time
	if (!wtypes) {
		CREATE(wtypes, char*, NUM_ATTACK_TYPES+1);
		
		for (iter = 0; iter < NUM_ATTACK_TYPES; ++iter) {
			wtypes[iter] = str_dup(attack_hit_info[iter].name);
		}
		
		// must terminate
		wtypes[NUM_ATTACK_TYPES] = str_dup("\n");
	}
	
	return wtypes;
}


/**
* For the .list command.
*
* @param obj_data *obj The thing to list.
* @param bool detail If TRUE, provide additional details
* @return char* The line to show (without a CRLF).
*/
char *list_one_object(obj_data *obj, bool detail) {
	static char output[MAX_STRING_LENGTH];
	char flags[MAX_STRING_LENGTH];
	
	bitvector_t show_flags = OBJ_SUPERIOR | OBJ_ENCHANTED | OBJ_SCALABLE | OBJ_BIND_ON_EQUIP | OBJ_BIND_ON_PICKUP | OBJ_NO_AUTOSTORE | OBJ_HARD_DROP | OBJ_GROUP_DROP | OBJ_GENERIC_DROP;
	
	if (detail) {
		if (IS_SET(GET_OBJ_EXTRA(obj), show_flags)) {
			sprintbit(GET_OBJ_EXTRA(obj) & show_flags, extra_bits, flags, TRUE);
		}
		else {
			*flags = '\0';
		}
		snprintf(output, sizeof(output), "[%5d] %s (%s) %s", GET_OBJ_VNUM(obj), GET_OBJ_SHORT_DESC(obj), level_range_string(GET_OBJ_MIN_SCALE_LEVEL(obj), GET_OBJ_MAX_SCALE_LEVEL(obj), 0), flags);
	}
	else {
		snprintf(output, sizeof(output), "[%5d] %s", GET_OBJ_VNUM(obj), GET_OBJ_SHORT_DESC(obj));
	}
	
	return output;
}


/**
* WARNING: This function actually deletes an object from the world.
*
* @param char_data *ch The person doing the deleting.
* @param obj_vnum vnum The vnum to delete.
*/
void olc_delete_object(char_data *ch, obj_vnum vnum) {
	void adjust_vehicle_tech(vehicle_data *veh, bool add);
	void complete_building(room_data *room);
	extern bool delete_from_interaction_list(struct interaction_item **list, int vnum_type, any_vnum vnum);
	extern bool delete_from_spawn_template_list(struct adventure_spawn **list, int spawn_type, mob_vnum vnum);
	extern bool delete_link_rule_by_portal(struct adventure_link_rule **list, obj_vnum portal_vnum);
	extern bool delete_quest_giver_from_list(struct quest_giver **list, int type, any_vnum vnum);
	extern bool delete_quest_reward_from_list(struct quest_reward **list, int type, any_vnum vnum);
	extern bool delete_requirement_from_list(struct req_data **list, int type, any_vnum vnum);
	extern bool delete_shop_item_from_list(struct shop_item **list, any_vnum vnum);
	void expire_trading_post_item(struct trading_post_data *tpd);
	extern bool remove_thing_from_resource_list(struct resource_data **list, int type, any_vnum vnum);
	void remove_object_from_table(obj_data *obj);
	void save_trading_post();

	struct empire_trade_data *trade, *next_trade;
	struct trading_post_data *tpd, *next_tpd;
	struct archetype_gear *gear, *next_gear;
	obj_data *proto, *obj_iter, *next_obj;
	struct global_data *glb, *next_glb;
	struct empire_production_total *egt;
	archetype_data *arch, *next_arch;
	room_template *rmt, *next_rmt;
	sector_data *sect, *next_sect;
	craft_data *craft, *next_craft;
	event_data *event, *next_event;
	morph_data *morph, *next_morph;
	quest_data *quest, *next_quest;
	progress_data *prg, *next_prg;
	augment_data *aug, *next_aug;
	vehicle_data *veh, *next_veh;
	crop_data *crop, *next_crop;
	room_data *room, *next_room;
	shop_data *shop, *next_shop;
	empire_data *emp, *next_emp;
	social_data *soc, *next_soc;
	char_data *mob, *next_mob;
	adv_data *adv, *next_adv;
	bld_data *bld, *next_bld;
	descriptor_data *desc;
	bool found, any_trades = FALSE;
	
	if (!(proto = obj_proto(vnum))) {
		msg_to_char(ch, "There is no such object %d.\r\n", vnum);
		return;
	}
	
	if (HASH_COUNT(object_table) <= 1) {
		msg_to_char(ch, "You can't delete the last object.\r\n");
		return;
	}
	
	// remove live objects: DO THIS FIRST
	for (obj_iter = object_list; obj_iter; obj_iter = next_obj) {
		next_obj = obj_iter->next;
		
		if (GET_OBJ_VNUM(obj_iter) == vnum) {
			// this is the removed item
			
			if (obj_iter->carried_by) {
				act("$p has been deleted.", FALSE, obj_iter->carried_by, obj_iter, NULL, TO_CHAR);
			}
			else if (obj_iter->worn_by) {
				act("$p has been deleted.", FALSE, obj_iter->worn_by, obj_iter, NULL, TO_CHAR);
			}
			else if (IN_ROOM(obj_iter) && ROOM_PEOPLE(IN_ROOM(obj_iter))) {
				act("$p has been deleted.", FALSE, ROOM_PEOPLE(IN_ROOM(obj_iter)), obj_iter, NULL, TO_CHAR);
			}
			
			extract_obj(obj_iter);
		}
	}
	
	// remove from live resource lists: room resources
	HASH_ITER(hh, world_table, room, next_room) {
		if (!COMPLEX_DATA(room)) {
			continue;
		}
		
		if (GET_BUILT_WITH(room)) {
			remove_thing_from_resource_list(&GET_BUILT_WITH(room), RES_OBJECT, vnum);
		}
		if (GET_BUILDING_RESOURCES(room)) {
			remove_thing_from_resource_list(&GET_BUILDING_RESOURCES(room), RES_OBJECT, vnum);
			
			if (!GET_BUILDING_RESOURCES(room)) {
				// removing this resource finished the building
				if (IS_DISMANTLING(room)) {
					disassociate_building(room);
				}
				else {
					complete_building(room);
				}
			}
		}
	}
	
	// remove from live resource lists: vehicle maintenance
	LL_FOREACH(vehicle_list, veh) {
		if (VEH_NEEDS_RESOURCES(veh)) {
			remove_thing_from_resource_list(&VEH_NEEDS_RESOURCES(veh), RES_OBJECT, vnum);
			
			if (!VEH_NEEDS_RESOURCES(veh)) {
				// removing the resource finished the vehicle
				if (VEH_FLAGGED(veh, VEH_INCOMPLETE)) {
					REMOVE_BIT(VEH_FLAGS(veh), VEH_INCOMPLETE);
					adjust_vehicle_tech(veh, TRUE);
					load_vtrigger(veh);
				}
			}
		}
	}
	
	// remove from empire inventories and trade -- DO THIS BEFORE REMOVING FROM OBJ TABLE
	HASH_ITER(hh, empire_table, emp, next_emp) {
		if (delete_stored_resource(emp, vnum)) {
			EMPIRE_NEEDS_STORAGE_SAVE(emp) = TRUE;
		}
		
		if (delete_unique_storage_by_vnum(emp, vnum)) {
			EMPIRE_NEEDS_STORAGE_SAVE(emp) = TRUE;
		}
		
		for (trade = EMPIRE_TRADE(emp); trade; trade = next_trade) {
			next_trade = trade->next;
			if (trade->vnum == vnum) {
				struct empire_trade_data *temp;
				REMOVE_FROM_LIST(trade, EMPIRE_TRADE(emp), next);
				free(trade);	// certified
				EMPIRE_NEEDS_SAVE(emp) = TRUE;
			}
		}
		
		// delete gather totals
		HASH_FIND_INT(EMPIRE_PRODUCTION_TOTALS(emp), &vnum, egt);
		if (egt) {
			HASH_DEL(EMPIRE_PRODUCTION_TOTALS(emp), egt);
			free(egt);
			EMPIRE_NEEDS_STORAGE_SAVE(emp) = TRUE;
		}
	}
	
	// remove from trading post -- again, BEFORE removing from obj table
	for (tpd = trading_list; tpd; tpd = next_tpd) {
		next_tpd = tpd->next;
		
		if (tpd->obj && GET_OBJ_VNUM(tpd->obj) == vnum) {
			expire_trading_post_item(tpd);
			add_to_object_list(tpd->obj);
			extract_obj(tpd->obj);
			tpd->obj = NULL;
			any_trades = TRUE;
		}
	}
	if (any_trades) {
		save_trading_post();
	}
	
	// pull from hash only after removing them from the world
	remove_object_from_table(proto);

	// save obj index and obj file now so there's no trouble later
	save_index(DB_BOOT_OBJ);
	save_library_file_for_vnum(DB_BOOT_OBJ, vnum);
	
	// update adventure zones
	HASH_ITER(hh, adventure_table, adv, next_adv) {
		found = delete_link_rule_by_portal(&(GET_ADV_LINKING(adv)), vnum);
		
		if (found) {
			save_library_file_for_vnum(DB_BOOT_ADV, GET_ADV_VNUM(adv));
		}
	}
	
	// update archetypes
	HASH_ITER(hh, archetype_table, arch, next_arch) {
		found = FALSE;
		for (gear = GET_ARCH_GEAR(arch); gear; gear = next_gear) {
			next_gear = gear->next;
			if (gear->vnum == vnum) {
				struct archetype_gear *temp;
				REMOVE_FROM_LIST(gear, GET_ARCH_GEAR(arch), next);
				free(gear);
				found = TRUE;
			}
		}
		
		if (found) {
			save_library_file_for_vnum(DB_BOOT_ARCH, GET_ARCH_VNUM(arch));
		}
	}
	
	// update augments
	HASH_ITER(hh, augment_table, aug, next_aug) {
		found = FALSE;
		
		if (GET_AUG_REQUIRES_OBJ(aug) == vnum) {
			GET_AUG_REQUIRES_OBJ(aug) = NOTHING;
			found = TRUE;
		}
		
		found |= remove_thing_from_resource_list(&GET_AUG_RESOURCES(aug), RES_OBJECT, vnum);
		
		if (found) {
			SET_BIT(GET_AUG_FLAGS(aug), AUG_IN_DEVELOPMENT);
			save_library_file_for_vnum(DB_BOOT_AUG, GET_AUG_VNUM(aug));
		}
	}
	
	// update buildings
	HASH_ITER(hh, building_table, bld, next_bld) {
		found = delete_from_interaction_list(&GET_BLD_INTERACTIONS(bld), TYPE_OBJ, vnum);
		found |= remove_thing_from_resource_list(&GET_BLD_YEARLY_MAINTENANCE(bld), RES_OBJECT, vnum);
		if (found) {
			save_library_file_for_vnum(DB_BOOT_BLD, GET_BLD_VNUM(bld));
		}
	}
	
	// update crafts
	HASH_ITER(hh, craft_table, craft, next_craft) {
		found = FALSE;
		if (GET_CRAFT_TYPE(craft) != CRAFT_TYPE_BUILD && !IS_SET(GET_CRAFT_FLAGS(craft), CRAFT_SOUP | CRAFT_VEHICLE) && GET_CRAFT_OBJECT(craft) == vnum) {
			GET_CRAFT_OBJECT(craft) = NOTHING;
			found = TRUE;
		}
		
		if (GET_CRAFT_REQUIRES_OBJ(craft) == vnum) {
			GET_CRAFT_REQUIRES_OBJ(craft) = NOTHING;
			found = TRUE;
		}
		
		found |= remove_thing_from_resource_list(&GET_CRAFT_RESOURCES(craft), RES_OBJECT, vnum);
		
		if (found) {
			SET_BIT(GET_CRAFT_FLAGS(craft), CRAFT_IN_DEVELOPMENT);
			save_library_file_for_vnum(DB_BOOT_CRAFT, GET_CRAFT_VNUM(craft));
		}
	}
	
	// update crops
	HASH_ITER(hh, crop_table, crop, next_crop) {
		found = delete_from_interaction_list(&GET_CROP_INTERACTIONS(crop), TYPE_OBJ, vnum);
		if (found) {
			save_library_file_for_vnum(DB_BOOT_CROP, GET_CROP_VNUM(crop));
		}
	}
	
	// update events
	HASH_ITER(hh, event_table, event, next_event) {
		// QR_x: event reward types
		found = delete_event_reward_from_list(&EVT_RANK_REWARDS(event), QR_OBJECT, vnum);
		found |= delete_event_reward_from_list(&EVT_THRESHOLD_REWARDS(event), QR_OBJECT, vnum);
		
		if (found) {
			// SET_BIT(EVT_FLAGS(event), EVTF_IN_DEVELOPMENT);
			save_library_file_for_vnum(DB_BOOT_EVT, EVT_VNUM(event));
		}
	}
	
	// update globals
	HASH_ITER(hh, globals_table, glb, next_glb) {
		found = delete_from_interaction_list(&GET_GLOBAL_INTERACTIONS(glb), TYPE_OBJ, vnum);
		if (found) {
			save_library_file_for_vnum(DB_BOOT_GLB, GET_GLOBAL_VNUM(glb));
		}
	}
	
	// update mobs
	HASH_ITER(hh, mobile_table, mob, next_mob) {
		found = delete_from_interaction_list(&mob->interactions, TYPE_OBJ, vnum);
		if (found) {
			save_library_file_for_vnum(DB_BOOT_MOB, mob->vnum);
		}
	}
	
	// update morphs
	HASH_ITER(hh, morph_table, morph, next_morph) {
		found = FALSE;
		
		if (MORPH_REQUIRES_OBJ(morph) == vnum) {
			MORPH_REQUIRES_OBJ(morph) = NOTHING;
			found = TRUE;
		}
		
		if (found) {
			SET_BIT(MORPH_FLAGS(morph), MORPHF_IN_DEVELOPMENT);
			save_library_file_for_vnum(DB_BOOT_MORPH, MORPH_VNUM(morph));
		}
	}
	
	// update objs
	HASH_ITER(hh, object_table, obj_iter, next_obj) {
		found = delete_from_interaction_list(&obj_iter->interactions, TYPE_OBJ, vnum);
		if (found) {
			save_library_file_for_vnum(DB_BOOT_OBJ, GET_OBJ_VNUM(obj_iter));
		}
	}
	
	// update progress
	HASH_ITER(hh, progress_table, prg, next_prg) {
		// REQ_x:
		found = delete_requirement_from_list(&PRG_TASKS(prg), REQ_GET_OBJECT, vnum);
		found |= delete_requirement_from_list(&PRG_TASKS(prg), REQ_WEARING, vnum);
		found |= delete_requirement_from_list(&PRG_TASKS(prg), REQ_WEARING_OR_HAS, vnum);
		found |= delete_requirement_from_list(&PRG_TASKS(prg), REQ_EMPIRE_PRODUCED_OBJECT, vnum);
		
		if (found) {
			SET_BIT(PRG_FLAGS(prg), PRG_IN_DEVELOPMENT);
			save_library_file_for_vnum(DB_BOOT_PRG, PRG_VNUM(prg));
			need_progress_refresh = TRUE;
		}
	}
	
	// update quests
	HASH_ITER(hh, quest_table, quest, next_quest) {
		// QG_x, QR_x, REQ_x:
		found = delete_quest_giver_from_list(&QUEST_STARTS_AT(quest), QG_OBJECT, vnum);
		found |= delete_quest_giver_from_list(&QUEST_ENDS_AT(quest), QG_OBJECT, vnum);
		found |= delete_quest_reward_from_list(&QUEST_REWARDS(quest), QR_OBJECT, vnum);
		found |= delete_requirement_from_list(&QUEST_TASKS(quest), REQ_GET_OBJECT, vnum);
		found |= delete_requirement_from_list(&QUEST_PREREQS(quest), REQ_GET_OBJECT, vnum);
		found |= delete_requirement_from_list(&QUEST_TASKS(quest), REQ_WEARING, vnum);
		found |= delete_requirement_from_list(&QUEST_PREREQS(quest), REQ_WEARING, vnum);
		found |= delete_requirement_from_list(&QUEST_TASKS(quest), REQ_WEARING_OR_HAS, vnum);
		found |= delete_requirement_from_list(&QUEST_PREREQS(quest), REQ_WEARING_OR_HAS, vnum);
		found |= delete_requirement_from_list(&QUEST_TASKS(quest), REQ_EMPIRE_PRODUCED_OBJECT, vnum);
		found |= delete_requirement_from_list(&QUEST_PREREQS(quest), REQ_EMPIRE_PRODUCED_OBJECT, vnum);
		
		if (found) {
			SET_BIT(QUEST_FLAGS(quest), QST_IN_DEVELOPMENT);
			save_library_file_for_vnum(DB_BOOT_QST, QUEST_VNUM(quest));
		}
	}
	
	// update room templates
	HASH_ITER(hh, room_template_table, rmt, next_rmt) {
		found = delete_from_spawn_template_list(&GET_RMT_SPAWNS(rmt), ADV_SPAWN_OBJ, vnum);
		found |= delete_from_interaction_list(&GET_RMT_INTERACTIONS(rmt), TYPE_OBJ, vnum);
		if (found) {
			save_library_file_for_vnum(DB_BOOT_RMT, GET_RMT_VNUM(rmt));
		}
	}
	
	// update sects
	HASH_ITER(hh, sector_table, sect, next_sect) {
		found = delete_from_interaction_list(&GET_SECT_INTERACTIONS(sect), TYPE_OBJ, vnum);
		if (found) {
			save_library_file_for_vnum(DB_BOOT_SECTOR, GET_SECT_VNUM(sect));
		}
	}
	
	// update shops
	HASH_ITER(hh, shop_table, shop, next_shop) {
		found = delete_quest_giver_from_list(&SHOP_LOCATIONS(shop), QG_OBJECT, vnum);
		found |= delete_shop_item_from_list(&SHOP_ITEMS(shop), vnum);
		
		if (found) {
			SET_BIT(SHOP_FLAGS(shop), SHOP_IN_DEVELOPMENT);
			save_library_file_for_vnum(DB_BOOT_SHOP, SHOP_VNUM(shop));
		}
	}
	
	// update socials
	HASH_ITER(hh, social_table, soc, next_soc) {
		// REQ_x:
		found = delete_requirement_from_list(&SOC_REQUIREMENTS(soc), REQ_GET_OBJECT, vnum);
		found |= delete_requirement_from_list(&SOC_REQUIREMENTS(soc), REQ_WEARING, vnum);
		found |= delete_requirement_from_list(&SOC_REQUIREMENTS(soc), REQ_WEARING_OR_HAS, vnum);
		found |= delete_requirement_from_list(&SOC_REQUIREMENTS(soc), REQ_EMPIRE_PRODUCED_OBJECT, vnum);
		
		if (found) {
			SET_BIT(SOC_FLAGS(soc), SOC_IN_DEVELOPMENT);
			save_library_file_for_vnum(DB_BOOT_SOC, SOC_VNUM(soc));
		}
	}
	
	// update vehicles
	HASH_ITER(hh, vehicle_table, veh, next_veh) {
		found = remove_thing_from_resource_list(&VEH_YEARLY_MAINTENANCE(veh), RES_OBJECT, vnum);
		if (found) {
			save_library_file_for_vnum(DB_BOOT_VEH, VEH_VNUM(veh));
		}
	}
	
	// olc editor updates
	for (desc = descriptor_list; desc; desc = desc->next) {
		if (desc->character && !IS_NPC(desc->character) && GET_ACTION_RESOURCES(desc->character)) {
			remove_thing_from_resource_list(&GET_ACTION_RESOURCES(desc->character), RES_OBJECT, vnum);
		}
		
		if (GET_OLC_ADVENTURE(desc)) {
			found = FALSE;
			found |= delete_link_rule_by_portal(&(GET_OLC_ADVENTURE(desc)->linking), vnum);
	
			if (found) {
				msg_to_desc(desc, "One or more linking rules have been removed from the adventure you are editing.\r\n");
			}
		}
		if (GET_OLC_ARCHETYPE(desc)) {
			found = FALSE;
			for (gear = GET_ARCH_GEAR(GET_OLC_ARCHETYPE(desc)); gear; gear = next_gear) {
				next_gear = gear->next;
				if (gear->vnum == vnum) {
					struct archetype_gear *temp;
					REMOVE_FROM_LIST(gear, GET_ARCH_GEAR(GET_OLC_ARCHETYPE(desc)), next);
					free(gear);
					found = TRUE;
				}
			}
		
			if (found) {
				msg_to_char(desc->character, "One of the objects used in the archetype you're editing was deleted.\r\n");
			}
		}
		if (GET_OLC_AUGMENT(desc)) {
			found = FALSE;
			
			if (GET_AUG_REQUIRES_OBJ(GET_OLC_AUGMENT(desc)) == vnum) {
				GET_AUG_REQUIRES_OBJ(GET_OLC_AUGMENT(desc)) = NOTHING;
				found = TRUE;
			}
			
			found |= remove_thing_from_resource_list(&GET_AUG_RESOURCES(GET_OLC_AUGMENT(desc)), RES_OBJECT, vnum);
			
			if (found) {
				SET_BIT(GET_AUG_FLAGS(GET_OLC_AUGMENT(desc)), AUG_IN_DEVELOPMENT);
				msg_to_char(desc->character, "One of the objects used in the augment you're editing was deleted.\r\n");
			}
	
		}
		if (GET_OLC_BUILDING(desc)) {
			found = delete_from_interaction_list(&GET_OLC_BUILDING(desc)->interactions, TYPE_OBJ, vnum);
			found |= remove_thing_from_resource_list(&GET_BLD_YEARLY_MAINTENANCE(GET_OLC_BUILDING(desc)), RES_OBJECT, vnum);
			if (found) {
				msg_to_char(desc->character, "One of the objects used in the building you're editing was deleted.\r\n");
			}
		}
		if (GET_OLC_CRAFT(desc)) {
			found = FALSE;
			if (GET_OLC_CRAFT(desc)->type != CRAFT_TYPE_BUILD && !IS_SET(GET_OLC_CRAFT(desc)->flags, CRAFT_SOUP | CRAFT_VEHICLE) && GET_OLC_CRAFT(desc)->object == vnum) {
				GET_OLC_CRAFT(desc)->object = NOTHING;
				found = TRUE;
			}
			
			if (GET_CRAFT_REQUIRES_OBJ(GET_OLC_CRAFT(desc)) == vnum) {
				GET_CRAFT_REQUIRES_OBJ(GET_OLC_CRAFT(desc)) = NOTHING;
				found = TRUE;
			}
			
			found |= remove_thing_from_resource_list(&GET_OLC_CRAFT(desc)->resources, RES_OBJECT, vnum);
		
			if (found) {
				SET_BIT(GET_OLC_CRAFT(desc)->flags, CRAFT_IN_DEVELOPMENT);
				msg_to_char(desc->character, "One of the objects used in the craft you're editing was deleted.\r\n");
			}	
		}
		if (GET_OLC_CROP(desc)) {
			if (delete_from_interaction_list(&GET_OLC_CROP(desc)->interactions, TYPE_OBJ, vnum)) {
				msg_to_char(desc->character, "One of the objects in an interaction for the crop you're editing was deleted.\r\n");
			}
		}
		if (GET_OLC_EVENT(desc)) {
			// QR_x: event reward types
			found = delete_event_reward_from_list(&EVT_RANK_REWARDS(GET_OLC_EVENT(desc)), QR_OBJECT, vnum);
			found |= delete_event_reward_from_list(&EVT_THRESHOLD_REWARDS(GET_OLC_EVENT(desc)), QR_OBJECT, vnum);
		
			if (found) {
				// SET_BIT(EVT_FLAGS(GET_OLC_EVENT(desc)), EVTF_IN_DEVELOPMENT);
				msg_to_desc(desc, "An object used as a reward by the event you are editing was deleted.\r\n");
			}
		}
		if (GET_OLC_GLOBAL(desc)) {
			found = delete_from_interaction_list(&GET_GLOBAL_INTERACTIONS(GET_OLC_GLOBAL(desc)), TYPE_OBJ, vnum);
			if (found) {
				msg_to_char(desc->character, "One of the objects in an interaction for the global you're editing was deleted.\r\n");
			}
		}
		if (GET_OLC_MOBILE(desc)) {
			if (delete_from_interaction_list(&GET_OLC_MOBILE(desc)->interactions, TYPE_OBJ, vnum)) {
				msg_to_char(desc->character, "One of the objects in an interaction for the mob you're editing was deleted.\r\n");
			}
		}
		if (GET_OLC_MORPH(desc)) {
			found = FALSE;
			
			if (MORPH_REQUIRES_OBJ(GET_OLC_MORPH(desc)) == vnum) {
				MORPH_REQUIRES_OBJ(GET_OLC_MORPH(desc)) = NOTHING;
				found = TRUE;
			}
			
			if (found) {
				SET_BIT(MORPH_FLAGS(GET_OLC_MORPH(desc)), MORPHF_IN_DEVELOPMENT);
				msg_to_char(desc->character, "The object required by the morph you're editing was deleted.\r\n");
			}
		}
		if (GET_OLC_OBJECT(desc)) {
			found = delete_from_interaction_list(&GET_OLC_OBJECT(desc)->interactions, TYPE_OBJ, vnum);
			if (found) {
				msg_to_char(desc->character, "One of the objects in an interaction for the item you're editing was deleted.\r\n");
			}
		}
		if (GET_OLC_PROGRESS(desc)) {
			// REQ_x:
			found = delete_requirement_from_list(&PRG_TASKS(GET_OLC_PROGRESS(desc)), REQ_GET_OBJECT, vnum);
			found |= delete_requirement_from_list(&PRG_TASKS(GET_OLC_PROGRESS(desc)), REQ_WEARING, vnum);
			found |= delete_requirement_from_list(&PRG_TASKS(GET_OLC_PROGRESS(desc)), REQ_WEARING_OR_HAS, vnum);
			found |= delete_requirement_from_list(&PRG_TASKS(GET_OLC_PROGRESS(desc)), REQ_EMPIRE_PRODUCED_OBJECT, vnum);
		
			if (found) {
				SET_BIT(QUEST_FLAGS(GET_OLC_PROGRESS(desc)), PRG_IN_DEVELOPMENT);
				msg_to_desc(desc, "An object used by the progression goal you're editing has been deleted.\r\n");
			}
		}
		if (GET_OLC_QUEST(desc)) {
			// QG_x, QR_x, REQ_x:
			found = delete_quest_giver_from_list(&QUEST_STARTS_AT(GET_OLC_QUEST(desc)), QG_OBJECT, vnum);
			found |= delete_quest_giver_from_list(&QUEST_ENDS_AT(GET_OLC_QUEST(desc)), QG_OBJECT, vnum);
			found |= delete_quest_reward_from_list(&QUEST_REWARDS(GET_OLC_QUEST(desc)), QR_OBJECT, vnum);
			found |= delete_requirement_from_list(&QUEST_TASKS(GET_OLC_QUEST(desc)), REQ_GET_OBJECT, vnum);
			found |= delete_requirement_from_list(&QUEST_PREREQS(GET_OLC_QUEST(desc)), REQ_GET_OBJECT, vnum);
			found |= delete_requirement_from_list(&QUEST_TASKS(GET_OLC_QUEST(desc)), REQ_WEARING, vnum);
			found |= delete_requirement_from_list(&QUEST_PREREQS(GET_OLC_QUEST(desc)), REQ_WEARING, vnum);
			found |= delete_requirement_from_list(&QUEST_TASKS(GET_OLC_QUEST(desc)), REQ_WEARING_OR_HAS, vnum);
			found |= delete_requirement_from_list(&QUEST_PREREQS(GET_OLC_QUEST(desc)), REQ_WEARING_OR_HAS, vnum);
			found |= delete_requirement_from_list(&QUEST_TASKS(GET_OLC_QUEST(desc)), REQ_EMPIRE_PRODUCED_OBJECT, vnum);
			found |= delete_requirement_from_list(&QUEST_PREREQS(GET_OLC_QUEST(desc)), REQ_EMPIRE_PRODUCED_OBJECT, vnum);
		
			if (found) {
				SET_BIT(QUEST_FLAGS(GET_OLC_QUEST(desc)), QST_IN_DEVELOPMENT);
				msg_to_desc(desc, "An object used by the quest you are editing was deleted.\r\n");
			}
		}
		if (GET_OLC_ROOM_TEMPLATE(desc)) {
			if (delete_from_spawn_template_list(&GET_OLC_ROOM_TEMPLATE(desc)->spawns, ADV_SPAWN_OBJ, vnum)) {
				msg_to_char(desc->character, "One of the objects that spawns in the room template you're editing was deleted.\r\n");
			}
			if (delete_from_interaction_list(&GET_OLC_ROOM_TEMPLATE(desc)->interactions, TYPE_OBJ, vnum)) {
				msg_to_char(desc->character, "One of the objects in an interaction for the room template you're editing was deleted.\r\n");
			}
		}
		if (GET_OLC_SECTOR(desc)) {
			if (delete_from_interaction_list(&GET_OLC_SECTOR(desc)->interactions, TYPE_OBJ, vnum)) {
				msg_to_char(desc->character, "One of the objects in an interaction for the sector you're editing was deleted.\r\n");
			}
		}
		if (GET_OLC_SHOP(desc)) {
			found = delete_quest_giver_from_list(&SHOP_LOCATIONS(GET_OLC_SHOP(desc)), QG_OBJECT, vnum);
			found |= delete_shop_item_from_list(&SHOP_ITEMS(GET_OLC_SHOP(desc)), vnum);
			
			if (found) {
				SET_BIT(SHOP_FLAGS(GET_OLC_SHOP(desc)), SHOP_IN_DEVELOPMENT);
				msg_to_desc(desc, "An object used by the shop you are editing was deleted.\r\n");
			}
		}
		if (GET_OLC_SOCIAL(desc)) {
			// REQ_x:
			found = delete_requirement_from_list(&SOC_REQUIREMENTS(GET_OLC_SOCIAL(desc)), REQ_GET_OBJECT, vnum);
			found |= delete_requirement_from_list(&SOC_REQUIREMENTS(GET_OLC_SOCIAL(desc)), REQ_WEARING, vnum);
			found |= delete_requirement_from_list(&SOC_REQUIREMENTS(GET_OLC_SOCIAL(desc)), REQ_WEARING_OR_HAS, vnum);
			found |= delete_requirement_from_list(&SOC_REQUIREMENTS(GET_OLC_SOCIAL(desc)), REQ_EMPIRE_PRODUCED_OBJECT, vnum);
		
			if (found) {
				SET_BIT(SOC_FLAGS(GET_OLC_SOCIAL(desc)), SOC_IN_DEVELOPMENT);
				msg_to_desc(desc, "An object required by the social you are editing was deleted.\r\n");
			}
		}
		if (GET_OLC_VEHICLE(desc)) {
			found = remove_thing_from_resource_list(&VEH_YEARLY_MAINTENANCE(GET_OLC_VEHICLE(desc)), RES_OBJECT, vnum);
			if (found) {
				msg_to_char(desc->character, "One of the objects used for maintenance for the vehicle you're editing was deleted.\r\n");
			}
		}
	}
	
	syslog(SYS_OLC, GET_INVIS_LEV(ch), TRUE, "OLC: %s has deleted object %d", GET_NAME(ch), vnum);
	msg_to_char(ch, "Object %d deleted.\r\n", vnum);
	
	free_obj(proto);
}


/**
* Searches properties of objects.
*
* @param char_data *ch The person searching.
* @param char *argument The argument they entered.
*/
void olc_fullsearch_obj(char_data *ch, char *argument) {
	extern int get_attack_type_by_name(char *name);
	
	char buf[MAX_STRING_LENGTH], line[MAX_STRING_LENGTH], type_arg[MAX_INPUT_LENGTH], val_arg[MAX_INPUT_LENGTH], find_keywords[MAX_INPUT_LENGTH];
	bitvector_t find_applies = NOBITS, found_applies, not_flagged = NOBITS, only_flags = NOBITS;
	bitvector_t only_worn = NOBITS, cmp_flags = NOBITS, not_cmp_flagged = NOBITS, only_affs = NOBITS;
	bitvector_t  find_interacts = NOBITS, found_interacts, find_custom = NOBITS, found_custom;
	int count, only_level = NOTHING, only_type = NOTHING, only_mat = NOTHING, only_cmp = NOTHING;
	int only_weapontype = NOTHING;
	struct interaction_item *inter;
	struct custom_message *cust;
	obj_data *obj, *next_obj;
	struct obj_apply *app;
	size_t size;
	
	if (!*argument) {
		msg_to_char(ch, "See HELP OEDIT FULLSEARCH for syntax.\r\n");
		return;
	}
	
	check_oedit_material_list();
	
	// process argument
	*find_keywords = '\0';
	while (*argument) {
		// figure out a type
		argument = any_one_arg(argument, type_arg);
		
		if (!strcmp(type_arg, "-")) {
			continue;	// just skip stray dashes
		}
		
		FULLSEARCH_FLAGS("affects", only_affs, affected_bits)
		FULLSEARCH_FLAGS("apply", find_applies, apply_types)
		FULLSEARCH_FLAGS("applies", find_applies, apply_types)
		FULLSEARCH_FLAGS("cflags", cmp_flags, component_flags)
		FULLSEARCH_FLAGS("cflagged", cmp_flags, component_flags)
		FULLSEARCH_LIST("component", only_cmp, component_types)
		FULLSEARCH_FLAGS("custom", find_custom, obj_custom_types)
		FULLSEARCH_FLAGS("flags", only_flags, extra_bits)
		FULLSEARCH_FLAGS("flagged", only_flags, extra_bits)
		FULLSEARCH_FLAGS("interaction", find_interacts, interact_types)
		FULLSEARCH_INT("level", only_level, 0, INT_MAX)
		FULLSEARCH_LIST("material", only_mat, (const char **)olc_material_list)
		FULLSEARCH_LIST("type", only_type, item_types)
		FULLSEARCH_FLAGS("uncflagged", not_cmp_flagged, component_flags)
		FULLSEARCH_FLAGS("unflagged", not_flagged, extra_bits)
		FULLSEARCH_FUNC("weapontype", only_weapontype, get_attack_type_by_name(val_arg))
		FULLSEARCH_FLAGS("wear", only_worn, wear_bits)
		FULLSEARCH_FLAGS("worn", only_worn, wear_bits)
		
		else {	// not sure what to do with it? treat it like a keyword
			sprintf(find_keywords + strlen(find_keywords), "%s%s", *find_keywords ? " " : "", type_arg);
		}
		
		// prepare for next loop
		skip_spaces(&argument);
	}
	
	size = snprintf(buf, sizeof(buf), "Object fullsearch: %s\r\n", find_keywords);
	count = 0;
	
	// okay now look up items
	HASH_ITER(hh, object_table, obj, next_obj) {
		if (only_level != NOTHING) {	// level-based checks
			if (GET_OBJ_MAX_SCALE_LEVEL(obj) != 0 && only_level > GET_OBJ_MAX_SCALE_LEVEL(obj)) {
				continue;
			}
			if (GET_OBJ_MIN_SCALE_LEVEL(obj) != 0 && only_level < GET_OBJ_MIN_SCALE_LEVEL(obj)) {
				continue;
			}
		}
		if (only_cmp != NOTHING && GET_OBJ_CMP_TYPE(obj) != only_cmp) {
			continue;
		}
		if (only_type != NOTHING && GET_OBJ_TYPE(obj) != only_type) {
			continue;
		}
		if (not_cmp_flagged != NOBITS && OBJ_CMP_FLAGGED(obj, not_cmp_flagged)) {
			continue;
		}
		if (not_flagged != NOBITS && OBJ_FLAGGED(obj, not_flagged)) {
			continue;
		}
		if (only_affs != NOBITS && (GET_OBJ_AFF_FLAGS(obj) & only_affs) != only_affs) {
			continue;
		}
		if (cmp_flags != NOBITS && (GET_OBJ_CMP_FLAGS(obj) & cmp_flags) != cmp_flags) {
			continue;
		}
		if (only_flags != NOBITS && (GET_OBJ_EXTRA(obj) & only_flags) != only_flags) {
			continue;
		}
		if (only_worn != NOBITS && (GET_OBJ_WEAR(obj) & only_worn) != only_worn) {
			continue;
		}
		if (only_mat != NOTHING && GET_OBJ_MATERIAL(obj) != only_mat) {
			continue;
		}
		if (only_weapontype != NOTHING && (!IS_WEAPON(obj) || GET_WEAPON_TYPE(obj) != only_weapontype)) {
			continue;
		}
		if (find_applies) {	// look up its applies
			found_applies = NOBITS;
			LL_FOREACH(GET_OBJ_APPLIES(obj), app) {
				found_applies |= BIT(app->location);
			}
			if ((find_applies & found_applies) != find_applies) {
				continue;
			}
		}
		if (find_custom) {	// look up its custom messages
			found_custom = NOBITS;
			LL_FOREACH(obj->custom_msgs, cust) {
				found_custom |= BIT(cust->type);
			}
			if ((find_custom & found_custom) != find_custom) {
				continue;
			}
		}
		if (find_interacts) {	// look up its interactions
			found_interacts = NOBITS;
			LL_FOREACH(obj->interactions, inter) {
				found_interacts |= BIT(inter->type);
			}
			if ((find_interacts & found_interacts) != find_interacts) {
				continue;
			}
		}
		if (*find_keywords && !multi_isname(find_keywords, GET_OBJ_KEYWORDS(obj)) && !multi_isname(find_keywords, GET_OBJ_SHORT_DESC(obj)) && !multi_isname(find_keywords, GET_OBJ_LONG_DESC(obj)) && !multi_isname(find_keywords, NULLSAFE(GET_OBJ_ACTION_DESC(obj))) && !search_custom_messages(find_keywords, obj->custom_msgs) && !search_extra_descs(find_keywords, obj->ex_description)) {
			continue;
		}
		
		// show it
		snprintf(line, sizeof(line), "[%5d] %s\r\n", GET_OBJ_VNUM(obj), GET_OBJ_SHORT_DESC(obj));
		if (strlen(line) + size < sizeof(buf)) {
			size += snprintf(buf + size, sizeof(buf) - size, "%s", line);
			++count;
		}
		else {
			size += snprintf(buf + size, sizeof(buf) - size, "OVERFLOW\r\n");
			break;
		}
	}
	
	if (count > 0 && (size + 14) < sizeof(buf)) {
		size += snprintf(buf + size, sizeof(buf) - size, "(%d objects)\r\n", count);
	}
	else if (count == 0) {
		size += snprintf(buf + size, sizeof(buf) - size, " none\r\n");
	}
	
	if (ch->desc) {
		page_string(ch->desc, buf, TRUE);
	}
}


/**
* Searches for all uses of a crop and displays them.
*
* @param char_data *ch The player.
* @param crop_vnum vnum The crop vnum.
*/
void olc_search_obj(char_data *ch, obj_vnum vnum) {
	extern bool find_quest_giver_in_list(struct quest_giver *list, int type, any_vnum vnum);
	extern bool find_quest_reward_in_list(struct quest_reward *list, int type, any_vnum vnum);
	extern bool find_requirement_in_list(struct req_data *list, int type, any_vnum vnum);
	extern bool find_shop_item_in_list(struct shop_item *list, any_vnum vnum);
	extern const byte interact_vnum_types[NUM_INTERACTS];
	
	char buf[MAX_STRING_LENGTH];
	struct adventure_spawn *asp;
	struct interaction_item *inter;
	struct adventure_link_rule *link;
	struct global_data *glb, *next_glb;
	archetype_data *arch, *next_arch;
	craft_data *craft, *next_craft;
	morph_data *morph, *next_morph;
	event_data *event, *next_event;
	quest_data *quest, *next_quest;
	progress_data *prg, *next_prg;
	room_template *rmt, *next_rmt;
	sector_data *sect, *next_sect;
	augment_data *aug, *next_aug;
	vehicle_data *veh, *next_veh;
	social_data *soc, *next_soc;
	struct archetype_gear *gear;
	crop_data *crop, *next_crop;
	shop_data *shop, *next_shop;
	char_data *mob, *next_mob;
	adv_data *adv, *next_adv;
	bld_data *bld, *next_bld;
	obj_data *proto, *obj, *next_obj;
	struct resource_data *res;
	int size, found;
	bool any;
	
	if (!(proto = obj_proto(vnum))) {
		msg_to_char(ch, "There is no object %d.\r\n", vnum);
		return;
	}
	
	found = 0;
	size = snprintf(buf, sizeof(buf), "Occurrences of object %d (%s):\r\n", vnum, GET_OBJ_SHORT_DESC(proto));
	
	// adventure zones
	HASH_ITER(hh, adventure_table, adv, next_adv) {
		for (link = GET_ADV_LINKING(adv); link; link = link->next) {
			if (link->portal_in == vnum || link->portal_out == vnum) {
				++found;
				size += snprintf(buf + size, sizeof(buf) - size, "ADV [%5d] %s\r\n", GET_ADV_VNUM(adv), GET_ADV_NAME(adv));
				break;	// only need 1
			}
		}
	}
	
	// archetypes
	HASH_ITER(hh, archetype_table, arch, next_arch) {
		any = FALSE;
		for (gear = GET_ARCH_GEAR(arch); gear && !any; gear = gear->next) {
			if (gear->vnum == vnum) {
				any = TRUE;
				++found;
				size += snprintf(buf + size, sizeof(buf) - size, "ARCH [%5d] %s\r\n", GET_ARCH_VNUM(arch), GET_ARCH_NAME(arch));
			}
		}
	}
	
	// augments
	HASH_ITER(hh, augment_table, aug, next_aug) {
		any = FALSE;
		if (!any && GET_AUG_REQUIRES_OBJ(aug) == vnum) {
			any = TRUE;
			++found;
			size += snprintf(buf + size, sizeof(buf) - size, "AUG [%5d] %s\r\n", GET_AUG_VNUM(aug), GET_AUG_NAME(aug));
		}
		for (res = GET_AUG_RESOURCES(aug); res && !any; res = res->next) {
			if (res->type == RES_OBJECT && res->vnum == vnum) {
				any = TRUE;
				++found;
				size += snprintf(buf + size, sizeof(buf) - size, "AUG [%5d] %s\r\n", GET_AUG_VNUM(aug), GET_AUG_NAME(aug));
			}
		}
	}
	
	// buildings
	HASH_ITER(hh, building_table, bld, next_bld) {
		any = FALSE;
		for (inter = GET_BLD_INTERACTIONS(bld); inter && !any; inter = inter->next) {
			if (interact_vnum_types[inter->type] == TYPE_OBJ && inter->vnum == vnum) {
				any = TRUE;
				++found;
				size += snprintf(buf + size, sizeof(buf) - size, "BDG [%5d] %s\r\n", GET_BLD_VNUM(bld), GET_BLD_NAME(bld));
			}
		}
		for (res = GET_BLD_YEARLY_MAINTENANCE(bld); res && !any; res = res->next) {
			if (res->type == RES_OBJECT && res->vnum == vnum) {
				any = TRUE;
				++found;
				size += snprintf(buf + size, sizeof(buf) - size, "BLD [%5d] %s\r\n", GET_BLD_VNUM(bld), GET_BLD_NAME(bld));
			}
		}
	}
	
	// crafts
	HASH_ITER(hh, craft_table, craft, next_craft) {
		any = FALSE;
		if (GET_CRAFT_TYPE(craft) != CRAFT_TYPE_BUILD && !IS_SET(GET_CRAFT_FLAGS(craft), CRAFT_SOUP | CRAFT_VEHICLE) && GET_CRAFT_OBJECT(craft) == vnum) {
			any = TRUE;
			++found;
			size += snprintf(buf + size, sizeof(buf) - size, "CFT [%5d] %s\r\n", GET_CRAFT_VNUM(craft), GET_CRAFT_NAME(craft));
		}
		if (!any && GET_CRAFT_REQUIRES_OBJ(craft) == vnum) {
			any = TRUE;
			++found;
			size += snprintf(buf + size, sizeof(buf) - size, "CFT [%5d] %s\r\n", GET_CRAFT_VNUM(craft), GET_CRAFT_NAME(craft));
		}
		for (res = GET_CRAFT_RESOURCES(craft); res && !any; res = res->next) {
			if (res->type == RES_OBJECT && res->vnum == vnum) {
				any = TRUE;
				++found;
				size += snprintf(buf + size, sizeof(buf) - size, "CFT [%5d] %s\r\n", GET_CRAFT_VNUM(craft), GET_CRAFT_NAME(craft));
			}
		}
	}

	// crops
	HASH_ITER(hh, crop_table, crop, next_crop) {
		any = FALSE;
		for (inter = GET_CROP_INTERACTIONS(crop); inter && !any; inter = inter->next) {
			if (interact_vnum_types[inter->type] == TYPE_OBJ && inter->vnum == vnum) {
				any = TRUE;
				++found;
				size += snprintf(buf + size, sizeof(buf) - size, "CRP [%5d] %s\r\n", GET_CROP_VNUM(crop), GET_CROP_NAME(crop));
			}
		}
	}
	
	// events
	HASH_ITER(hh, event_table, event, next_event) {
		if (size >= sizeof(buf)) {
			break;
		}
		// QR_x: event rewards
		any = find_event_reward_in_list(EVT_RANK_REWARDS(event), QR_OBJECT, vnum);
		any |= find_event_reward_in_list(EVT_THRESHOLD_REWARDS(event), QR_OBJECT, vnum);
		
		if (any) {
			++found;
			size += snprintf(buf + size, sizeof(buf) - size, "EVT [%5d] %s\r\n", EVT_VNUM(event), EVT_NAME(event));
		}
	}
	
	// globals
	HASH_ITER(hh, globals_table, glb, next_glb) {
		any = FALSE;
		for (inter = GET_GLOBAL_INTERACTIONS(glb); inter && !any; inter = inter->next) {
			if (interact_vnum_types[inter->type] == TYPE_OBJ && inter->vnum == vnum) {
				any = TRUE;
				++found;
				size += snprintf(buf + size, sizeof(buf) - size, "GLB [%5d] %s\r\n", GET_GLOBAL_VNUM(glb), GET_GLOBAL_NAME(glb));
			}
		}
		for (gear = GET_GLOBAL_GEAR(glb); gear && !any; gear = gear->next) {
			if (gear->vnum == vnum) {
				any = TRUE;
				++found;
				size += snprintf(buf + size, sizeof(buf) - size, "GLB [%5d] %s\r\n", GET_GLOBAL_VNUM(glb), GET_GLOBAL_NAME(glb));
			}
		}
	}
	
	// mob interactions
	HASH_ITER(hh, mobile_table, mob, next_mob) {
		any = FALSE;
		for (inter = mob->interactions; inter && !any; inter = inter->next) {
			if (interact_vnum_types[inter->type] == TYPE_OBJ && inter->vnum == vnum) {
				any = TRUE;
				++found;
				size += snprintf(buf + size, sizeof(buf) - size, "MOB [%5d] %s\r\n", GET_MOB_VNUM(mob), GET_SHORT_DESC(mob));
			}
		}
	}
	
	// morphs
	HASH_ITER(hh, morph_table, morph, next_morph) {
		if (MORPH_REQUIRES_OBJ(morph) == vnum) {
			++found;
			size += snprintf(buf + size, sizeof(buf) - size, "MPH [%5d] %s\r\n", MORPH_VNUM(morph), MORPH_SHORT_DESC(morph));
		}
	}
	
	// obj interactions
	HASH_ITER(hh, object_table, obj, next_obj) {
		any = FALSE;
		for (inter = obj->interactions; inter && !any; inter = inter->next) {
			if (interact_vnum_types[inter->type] == TYPE_OBJ && inter->vnum == vnum) {
				any = TRUE;
				++found;
				size += snprintf(buf + size, sizeof(buf) - size, "OBJ [%5d] %s\r\n", GET_OBJ_VNUM(obj), GET_OBJ_SHORT_DESC(obj));
			}
		}
	}
	
	// progress
	HASH_ITER(hh, progress_table, prg, next_prg) {
		if (size >= sizeof(buf)) {
			break;
		}
		// REQ_x: requirement search
		any = find_requirement_in_list(PRG_TASKS(prg), REQ_GET_OBJECT, vnum);
		any |= find_requirement_in_list(PRG_TASKS(prg), REQ_WEARING, vnum);
		any |= find_requirement_in_list(PRG_TASKS(prg), REQ_WEARING_OR_HAS, vnum);
		any |= find_requirement_in_list(PRG_TASKS(prg), REQ_EMPIRE_PRODUCED_OBJECT, vnum);
		
		if (any) {
			++found;
			size += snprintf(buf + size, sizeof(buf) - size, "PRG [%5d] %s\r\n", PRG_VNUM(prg), PRG_NAME(prg));
		}
	}
	
	// quests
	HASH_ITER(hh, quest_table, quest, next_quest) {
		if (size >= sizeof(buf)) {
			break;
		}
		// QG_x, QR_x, REQ_x:
		any = find_quest_giver_in_list(QUEST_STARTS_AT(quest), QG_OBJECT, vnum);
		any |= find_quest_giver_in_list(QUEST_ENDS_AT(quest), QG_OBJECT, vnum);
		any |= find_quest_reward_in_list(QUEST_REWARDS(quest), QR_OBJECT, vnum);
		any |= find_requirement_in_list(QUEST_TASKS(quest), REQ_GET_OBJECT, vnum);
		any |= find_requirement_in_list(QUEST_PREREQS(quest), REQ_GET_OBJECT, vnum);
		any |= find_requirement_in_list(QUEST_TASKS(quest), REQ_WEARING, vnum);
		any |= find_requirement_in_list(QUEST_PREREQS(quest), REQ_WEARING, vnum);
		any |= find_requirement_in_list(QUEST_TASKS(quest), REQ_WEARING_OR_HAS, vnum);
		any |= find_requirement_in_list(QUEST_PREREQS(quest), REQ_WEARING_OR_HAS, vnum);
		any |= find_requirement_in_list(QUEST_TASKS(quest), REQ_EMPIRE_PRODUCED_OBJECT, vnum);
		any |= find_requirement_in_list(QUEST_PREREQS(quest), REQ_EMPIRE_PRODUCED_OBJECT, vnum);
		
		if (any) {
			++found;
			size += snprintf(buf + size, sizeof(buf) - size, "QST [%5d] %s\r\n", QUEST_VNUM(quest), QUEST_NAME(quest));
		}
	}
	
	// room templates
	HASH_ITER(hh, room_template_table, rmt, next_rmt) {
		any = FALSE;
		for (asp = GET_RMT_SPAWNS(rmt); asp && !any; asp = asp->next) {
			if (asp->type == ADV_SPAWN_OBJ && asp->vnum == vnum) {
				any = TRUE;
				++found;
				size += snprintf(buf + size, sizeof(buf) - size, "RMT [%5d] %s\r\n", GET_RMT_VNUM(rmt), GET_RMT_TITLE(rmt));
			}
		}
		for (inter = GET_RMT_INTERACTIONS(rmt); inter && !any; inter = inter->next) {
			if (interact_vnum_types[inter->type] == TYPE_OBJ && inter->vnum == vnum) {
				any = TRUE;
				++found;
				size += snprintf(buf + size, sizeof(buf) - size, "RMT [%5d] %s\r\n", GET_RMT_VNUM(rmt), GET_RMT_TITLE(rmt));
			}
		}
	}
	
	// sectors
	HASH_ITER(hh, sector_table, sect, next_sect) {
		any = FALSE;
		for (inter = GET_SECT_INTERACTIONS(sect); inter && !any; inter = inter->next) {
			if (interact_vnum_types[inter->type] == TYPE_OBJ && inter->vnum == vnum) {
				any = TRUE;
				++found;
				size += snprintf(buf + size, sizeof(buf) - size, "SCT [%5d] %s\r\n", GET_SECT_VNUM(sect), GET_SECT_NAME(sect));
			}
		}
	}
	
	// shops
	HASH_ITER(hh, shop_table, shop, next_shop) {
		if (size >= sizeof(buf)) {
			break;
		}
		any = find_quest_giver_in_list(SHOP_LOCATIONS(shop), QG_OBJECT, vnum);
		any |= find_shop_item_in_list(SHOP_ITEMS(shop), vnum);
		
		if (any) {
			++found;
			size += snprintf(buf + size, sizeof(buf) - size, "SHOP [%5d] %s\r\n", SHOP_VNUM(shop), SHOP_NAME(shop));
		}
	}
	
	// socials
	HASH_ITER(hh, social_table, soc, next_soc) {
		if (size >= sizeof(buf)) {
			break;
		}
		// REQ_x:
		any = find_requirement_in_list(SOC_REQUIREMENTS(soc), REQ_GET_OBJECT, vnum);
		any |= find_requirement_in_list(SOC_REQUIREMENTS(soc), REQ_WEARING, vnum);
		any |= find_requirement_in_list(SOC_REQUIREMENTS(soc), REQ_WEARING_OR_HAS, vnum);
		any |= find_requirement_in_list(SOC_REQUIREMENTS(soc), REQ_EMPIRE_PRODUCED_OBJECT, vnum);
		
		if (any) {
			++found;
			size += snprintf(buf + size, sizeof(buf) - size, "SOC [%5d] %s\r\n", SOC_VNUM(soc), SOC_NAME(soc));
		}
	}
	
	// vehicles
	HASH_ITER(hh, vehicle_table, veh, next_veh) {
		any = FALSE;
		for (res = VEH_YEARLY_MAINTENANCE(veh); res && !any; res = res->next) {
			if (res->type == RES_OBJECT && res->vnum == vnum) {
				any = TRUE;
				++found;
				size += snprintf(buf + size, sizeof(buf) - size, "VEH [%5d] %s\r\n", VEH_VNUM(veh), VEH_SHORT_DESC(veh));
			}
		}
	}
	
	if (found > 0) {
		size += snprintf(buf + size, sizeof(buf) - size, "%d location%s shown\r\n", found, PLURAL(found));
	}
	else {
		size += snprintf(buf + size, sizeof(buf) - size, " none\r\n");
	}
	
	page_string(ch->desc, buf, TRUE);
}


/**
* Updates one live object whose string (etc) pointers link to an old prototype,
* by assigning them to a new prototype.
*
* @param obj_data *to_update The live object to update.
* @param obj_data *old_proto The old prototype.
* @param obj_data *new_proto The olc object being saved as the new prototype.
*/
void update_live_obj_from_olc(obj_data *to_update, obj_data *old_proto, obj_data *new_proto) {
	// NOTE: If you ever choose to update flags here, some flags like LIGHT
	// also require updates to the light counts in the world.
	
	// strings
	if (GET_OBJ_KEYWORDS(to_update) == GET_OBJ_KEYWORDS(old_proto)) {
		GET_OBJ_KEYWORDS(to_update) = GET_OBJ_KEYWORDS(new_proto);
	}
	if (GET_OBJ_LONG_DESC(to_update) == GET_OBJ_LONG_DESC(old_proto)) {
		GET_OBJ_LONG_DESC(to_update) = GET_OBJ_LONG_DESC(new_proto);
	}
	if (GET_OBJ_SHORT_DESC(to_update) == GET_OBJ_SHORT_DESC(old_proto)) {
		GET_OBJ_SHORT_DESC(to_update) = GET_OBJ_SHORT_DESC(new_proto);
	}
	if (GET_OBJ_ACTION_DESC(to_update) == GET_OBJ_ACTION_DESC(old_proto)) {
		GET_OBJ_ACTION_DESC(to_update) = GET_OBJ_ACTION_DESC(new_proto);
	}
	
	// pointers
	if (to_update->ex_description == old_proto->ex_description) {
		to_update->ex_description = new_proto->ex_description;
	}
	if (to_update->storage == old_proto->storage) {
		to_update->storage = new_proto->storage;
	}
	if (to_update->interactions == old_proto->interactions) {
		to_update->interactions = new_proto->interactions;
	}
	if (to_update->custom_msgs == old_proto->custom_msgs) {
		to_update->custom_msgs = new_proto->custom_msgs;
	}
	
	// remove old scripts
	if (SCRIPT(to_update)) {
		remove_all_triggers(to_update, OBJ_TRIGGER);
	}
	if (to_update->proto_script && to_update->proto_script != old_proto->proto_script) {
		free_proto_scripts(&to_update->proto_script);
	}
	
	// re-attach scripts
	to_update->proto_script = copy_trig_protos(new_proto->proto_script);
	assign_triggers(to_update, OBJ_TRIGGER);
}


/**
* Function to save a player's changes to an object (or a new object).
*
* @param descriptor_data *desc The descriptor who is saving an object.
*/
void save_olc_object(descriptor_data *desc) {
	void prune_extra_descs(struct extra_descr_data **list);
	
	obj_data *obj = GET_OLC_OBJECT(desc), *obj_iter, *proto;
	obj_vnum vnum = GET_OLC_VNUM(desc);
	struct empire_unique_storage *eus;
	struct obj_storage_type *store;
	struct trading_post_data *tpd;
	empire_data *emp, *next_emp;
	struct quest_lookup *ql;
	struct shop_lookup *sl;
	UT_hash_handle hh;
	
	// have a place to save it?
	if (!(proto = obj_proto(vnum))) {
		proto = create_obj_table_entry(vnum);
	}
	
	// update the strings, pointers, and stats on live items
	for (obj_iter = object_list; obj_iter; obj_iter = obj_iter->next) {
		if (GET_OBJ_VNUM(obj_iter) == vnum) {
			update_live_obj_from_olc(obj_iter, proto, obj);
		}
	}
	
	// update objects in unique storage
	HASH_ITER(hh, empire_table, emp, next_emp) {
		for (eus = EMPIRE_UNIQUE_STORAGE(emp); eus; eus = eus->next) {
			if (eus->obj && GET_OBJ_VNUM(eus->obj) == vnum) {
				update_live_obj_from_olc(eus->obj, proto, obj);
			}
		}
	}
	
	// update objs in trading post
	for (tpd = trading_list; tpd; tpd = tpd->next) {
		if (tpd->obj && GET_OBJ_VNUM(tpd->obj) == vnum) {
			update_live_obj_from_olc(tpd->obj, proto, obj);
		}
	}
	
	// free prototype strings and pointers
	if (GET_OBJ_KEYWORDS(proto)) {
		free(GET_OBJ_KEYWORDS(proto));
	}
	if (GET_OBJ_LONG_DESC(proto)) {
		free(GET_OBJ_LONG_DESC(proto));
	}
	if (GET_OBJ_SHORT_DESC(proto)) {
		free(GET_OBJ_SHORT_DESC(proto));
	}
	if (GET_OBJ_ACTION_DESC(proto)) {
		free(GET_OBJ_ACTION_DESC(proto));
	}
	free_extra_descs(&proto->ex_description);
	free_interactions(&proto->interactions);
	while ((store = proto->storage)) {
		proto->storage = store->next;
		free(store);
	}
	free_custom_messages(proto->custom_msgs);
	
	// old applies
	free_obj_apply_list(GET_OBJ_APPLIES(proto));
	GET_OBJ_APPLIES(proto) = NULL;
	
	// free old script?
	if (proto->proto_script) {
		free_proto_scripts(&proto->proto_script);
	}
	
	// timer must be converted
	if (GET_OBJ_TIMER(obj) <= 0) {
		GET_OBJ_TIMER(obj) = UNLIMITED;
	}
	prune_extra_descs(&obj->ex_description);

	// save data back over the proto-type
	hh = proto->hh;	// save old hash handle
	ql = proto->quest_lookups;	// save lookups
	sl = proto->shop_lookups;
	
	*proto = *obj;
	proto->vnum = vnum;	// ensure correct vnum
	
	proto->hh = hh;	// restore hash handle
	proto->quest_lookups = ql;	// restore lookups
	proto->shop_lookups = sl;
	
	// remove the reference to this so it won't be free'd
	GET_OBJ_APPLIES(obj) = NULL;
	
	// and save to file
	save_library_file_for_vnum(DB_BOOT_OBJ, vnum);
}


/**
* Sets up a string editor for an extra desc.
*
* @param char_data *ch The editor.
* @param extra_descr_data *ex The extra description.
*/
void setup_extra_desc_editor(char_data *ch, struct extra_descr_data *ex) {
	start_string_editor(ch->desc, "extra description", &(ex->description), MAX_ITEM_DESCRIPTION, FALSE);
}


/**
* Copies a set of extra descriptions to a new list.
*
* @param struct extra_descr_data *list The list to copy.
* @return struct extra_descr_data* The copied list.
*/
struct extra_descr_data *copy_extra_descs(struct extra_descr_data *list) {
	struct extra_descr_data *new, *newlist, *last, *iter;
	
	newlist = last = NULL;
	for (iter = list; iter; iter = iter->next) {
		// skip empty descriptions entirely
		if (!iter->description || !*iter->description || !iter->keyword || !*iter->keyword) {
			continue;
		}
		
		CREATE(new, struct extra_descr_data, 1);
		new->keyword = iter->keyword ? str_dup(iter->keyword) : NULL;
		new->description = iter->description ? str_dup(iter->description) : NULL;
		new->next = NULL;
	
		if (last) {
			last->next = new;
		}
		else {
			newlist = new;
		}
	
		last = new;
	}
	
	return newlist;
}


/**
* Creates a copy of an object, or clears a new one, for editing. Note that this
* will increase the version number of the item by 1, for the auto-update
* system.
* 
* @param obj_data *input The object to copy, or NULL to make a new one.
* @return obj_data *The copied object.
*/
obj_data *setup_olc_object(obj_data *input) {
	struct obj_storage_type *store, *new_store, *last_store;
	obj_data *new;
	
	CREATE(new, obj_data, 1);
	clear_object(new);
	
	if (input) {
		// copy normal data
		*new = *input;

		// copy things that are pointers
		GET_OBJ_KEYWORDS(new) = GET_OBJ_KEYWORDS(input) ? str_dup(GET_OBJ_KEYWORDS(input)) : NULL;
		GET_OBJ_LONG_DESC(new) = GET_OBJ_LONG_DESC(input) ? str_dup(GET_OBJ_LONG_DESC(input)) : NULL;
		GET_OBJ_SHORT_DESC(new) = GET_OBJ_SHORT_DESC(input) ? str_dup(GET_OBJ_SHORT_DESC(input)) : NULL;
		GET_OBJ_ACTION_DESC(new) = GET_OBJ_ACTION_DESC(input) ? str_dup(GET_OBJ_ACTION_DESC(input)) : NULL;
		
		// copy extra descs
		new->ex_description = copy_extra_descs(input->ex_description);
		
		// copy interactions
		new->interactions = copy_interaction_list(input->interactions);
		
		new->storage = NULL;
		last_store = NULL;
		for (store = input->storage; store; store = store->next) {
			CREATE(new_store, struct obj_storage_type, 1);
			
			*new_store = *store;
			new_store->next = NULL;
			
			if (last_store) {
				last_store->next = new_store;
			}
			else {
				new->storage = new_store;
			}
			last_store = new_store;
		}
		
		// copy custom msgs
		new->custom_msgs = copy_custom_messages(input->custom_msgs);
		
		// copy applies
		GET_OBJ_APPLIES(new) = copy_obj_apply_list(GET_OBJ_APPLIES(input));
		
		// copy scripts
		SCRIPT(new) = NULL;
		new->proto_script = copy_trig_protos(input->proto_script);
		
		// update version number
		OBJ_VERSION(new) += 1;
	}
	else {
		// brand new
		GET_OBJ_KEYWORDS(new) = str_dup(default_obj_keywords);
		GET_OBJ_SHORT_DESC(new) = str_dup(default_obj_short);
		GET_OBJ_LONG_DESC(new) = str_dup(default_obj_long);
		GET_OBJ_ACTION_DESC(new) = NULL;
		GET_OBJ_WEAR(new) = ITEM_WEAR_TAKE;
		OBJ_VERSION(new) = 1;

		SCRIPT(new) = NULL;
		new->proto_script = NULL;
	}
	
	// done
	return new;
}


 //////////////////////////////////////////////////////////////////////////////
//// DISPLAYS ////////////////////////////////////////////////////////////////

/**
* This function handles the display of object values for different item types.
*
* @param char_data *ch The person editing an object.
* @param char *storage A buffer to save the output text into.
*/
void olc_get_values_display(char_data *ch, char *storage) {
	obj_data *obj = GET_OLC_OBJECT(ch->desc);
	book_data *book;
	char temp[MAX_STRING_LENGTH];
	
	*storage = '\0';
	
	// ITEM_X: editor prompts
	switch (GET_OBJ_TYPE(obj)) {
		case ITEM_COINS: {
			sprintf(storage + strlen(storage), "<%scoinamount\t0> %d%s\r\n", OLC_LABEL_VAL(GET_COINS_AMOUNT(obj), 0), GET_COINS_AMOUNT(obj), OBJ_FLAGGED(obj, OBJ_SCALABLE) ? " (scalable)" : "");
			// empire number is not supported -- it will always use OTHER_COIN
			break;
		}
		case ITEM_CURRENCY: {
			sprintf(storage + strlen(storage), "<%scurrency\t0> %d %s\r\n", OLC_LABEL_VAL(GET_CURRENCY_VNUM(obj), NOTHING), GET_CURRENCY_VNUM(obj), get_generic_name_by_vnum(GET_CURRENCY_VNUM(obj)));
			sprintf(storage + strlen(storage), "<%scoinamount\t0> %d\r\n", OLC_LABEL_VAL(GET_CURRENCY_AMOUNT(obj), 0), GET_CURRENCY_AMOUNT(obj));
			break;
		}
		case ITEM_CORPSE: {
			sprintf(storage + strlen(storage), "<%scorpseof\t0> %d %s\r\n", OLC_LABEL_VAL(GET_CORPSE_NPC_VNUM(obj), 0), GET_CORPSE_NPC_VNUM(obj), get_mob_name_by_proto(GET_CORPSE_NPC_VNUM(obj)));
			sprintf(storage + strlen(storage), "<%ssize\t0> %s\r\n", OLC_LABEL_VAL(GET_CORPSE_SIZE(obj), 0), size_types[GET_CORPSE_SIZE(obj)]);
			break;
		}
		case ITEM_WEAPON: {
			sprintf(storage + strlen(storage), "<%sweapontype\t0> %s\r\n", OLC_LABEL_VAL(GET_WEAPON_TYPE(obj), 0), attack_hit_info[GET_WEAPON_TYPE(obj)].name);
			if (OBJ_FLAGGED(obj, OBJ_SCALABLE)) {
				sprintf(storage + strlen(storage), "<%sdamage\t0> %d (scalable, speed %.2f)\r\n", OLC_LABEL_VAL(GET_WEAPON_DAMAGE_BONUS(obj), 0), GET_WEAPON_DAMAGE_BONUS(obj), get_weapon_speed(obj));
			}
			else {	// not scalable
				sprintf(storage + strlen(storage), "<%sdamage\t0> %d (speed %.2f, %s+%.2f base dps)\r\n", OLC_LABEL_VAL(GET_WEAPON_DAMAGE_BONUS(obj), 0), GET_WEAPON_DAMAGE_BONUS(obj), get_weapon_speed(obj), (IS_MAGIC_ATTACK(GET_WEAPON_TYPE(obj)) ? "Intelligence" : "Strength"), get_base_dps(obj));
			}
			break;
		}
		case ITEM_CONTAINER: {
			sprintf(storage + strlen(storage), "<%scapacity\t0> %d object%s\r\n", OLC_LABEL_VAL(GET_MAX_CONTAINER_CONTENTS(obj), 0), GET_MAX_CONTAINER_CONTENTS(obj), PLURAL(GET_MAX_CONTAINER_CONTENTS(obj)));
			
			sprintbit(GET_CONTAINER_FLAGS(obj), container_bits, temp, TRUE);
			sprintf(storage + strlen(storage), "<%scontainerflags\t0> %s\r\n", OLC_LABEL_VAL(GET_CONTAINER_FLAGS(obj), NOBITS), temp);
			break;
		}
		case ITEM_DRINKCON: {
			if (OBJ_FLAGGED(obj, OBJ_SCALABLE)) {
				sprintf(storage + strlen(storage), "<%scapacity\t0> %d (scalable)\r\n", OLC_LABEL_VAL(GET_DRINK_CONTAINER_CAPACITY(obj), 0), GET_DRINK_CONTAINER_CAPACITY(obj));
			}
			else {
				sprintf(storage + strlen(storage), "<%scapacity\t0> %d drink%s\r\n", OLC_LABEL_VAL(GET_DRINK_CONTAINER_CAPACITY(obj), 0), GET_DRINK_CONTAINER_CAPACITY(obj), PLURAL(GET_DRINK_CONTAINER_CAPACITY(obj)));
			}
			sprintf(storage + strlen(storage), "<%scontents\t0> %d drink%s\r\n", OLC_LABEL_VAL(GET_DRINK_CONTAINER_CONTENTS(obj), 0), GET_DRINK_CONTAINER_CONTENTS(obj), PLURAL(GET_DRINK_CONTAINER_CONTENTS(obj)));
			sprintf(storage + strlen(storage), "<%sliquid\t0> %d %s\r\n", OLC_LABEL_VAL(GET_DRINK_CONTAINER_TYPE(obj), 0), GET_DRINK_CONTAINER_TYPE(obj), get_generic_name_by_vnum(GET_DRINK_CONTAINER_TYPE(obj)));
			break;
		}
		case ITEM_FOOD: {
			sprintf(storage + strlen(storage), "<%sfullness\t0> %d hour%s\r\n", OLC_LABEL_VAL(GET_FOOD_HOURS_OF_FULLNESS(obj), 0), GET_FOOD_HOURS_OF_FULLNESS(obj), (GET_FOOD_HOURS_OF_FULLNESS(obj) != 1 ? "s" : ""));
			break;
		}
		case ITEM_PORTAL: {
			sprintf(storage + strlen(storage), "<%sroomvnum\t0> %d\r\n", OLC_LABEL_VAL(GET_PORTAL_TARGET_VNUM(obj), 0), GET_PORTAL_TARGET_VNUM(obj));
			break;
		}
		case ITEM_MISSILE_WEAPON: {
			sprintf(storage + strlen(storage), "<%sweapontype\t0> %s\r\n", OLC_LABEL_VAL(GET_MISSILE_WEAPON_TYPE(obj), 0), attack_hit_info[GET_MISSILE_WEAPON_TYPE(obj)].name);
			if (OBJ_FLAGGED(obj, OBJ_SCALABLE)) {
				sprintf(storage + strlen(storage), "<%sdamage\t0> %d (scalable, speed %.2f)\r\n", OLC_LABEL_VAL(GET_MISSILE_WEAPON_DAMAGE(obj), 0), GET_MISSILE_WEAPON_DAMAGE(obj), get_weapon_speed(obj));
			}
			else {
				sprintf(storage + strlen(storage), "<%sdamage\t0> %d (speed %.2f, %s+%.2f base dps)\r\n", OLC_LABEL_VAL(GET_MISSILE_WEAPON_DAMAGE(obj), 0), GET_MISSILE_WEAPON_DAMAGE(obj), get_weapon_speed(obj), (IS_MAGIC_ATTACK(GET_MISSILE_WEAPON_TYPE(obj)) ? "Intelligence" : "Strength"), get_base_dps(obj));
			}
			sprintf(storage + strlen(storage), "<%sammotype\t0> %c\r\n", OLC_LABEL_VAL(GET_MISSILE_WEAPON_AMMO_TYPE(obj), 0), 'A' + GET_MISSILE_WEAPON_AMMO_TYPE(obj));
			break;
		}
		case ITEM_AMMO: {
			sprintf(storage + strlen(storage), "<%squantity\t0> %d\r\n", OLC_LABEL_VAL(GET_AMMO_QUANTITY(obj), 0), GET_AMMO_QUANTITY(obj));
			sprintf(storage + strlen(storage), "<%sdamage\t0> %+d%s\r\n", OLC_LABEL_VAL(GET_AMMO_DAMAGE_BONUS(obj), 0), GET_AMMO_DAMAGE_BONUS(obj), OBJ_FLAGGED(obj, OBJ_SCALABLE) ? " (scalable)" : "");
			sprintf(storage + strlen(storage), "<%sammotype\t0> %c\r\n", OLC_LABEL_VAL(GET_AMMO_TYPE(obj), 0), 'A' + GET_AMMO_TYPE(obj));
			break;
		}
		case ITEM_PACK: {
			sprintf(storage + strlen(storage), "<%scapacity\t0> %d object%s%s\r\n", OLC_LABEL_VAL(GET_PACK_CAPACITY(obj), 0), GET_PACK_CAPACITY(obj), PLURAL(GET_PACK_CAPACITY(obj)), OBJ_FLAGGED(obj, OBJ_SCALABLE) ? " (scalable)" : "");
			break;
		}
		case ITEM_PAINT: {
			sprintf(storage + strlen(storage), "<%spaint\t0> %s%s\t0\r\n", OLC_LABEL_VAL(GET_PAINT_COLOR(obj), 0), paint_colors[GET_PAINT_COLOR(obj)], paint_names[GET_PAINT_COLOR(obj)]);
			break;
		}
		case ITEM_POTION: {
			sprintf(storage + strlen(storage), "<%saffecttype\t0> [%d] %s\r\n", OLC_LABEL_VAL(GET_POTION_AFFECT(obj), NOTHING), GET_POTION_AFFECT(obj), GET_POTION_AFFECT(obj) != NOTHING ? get_generic_name_by_vnum(GET_POTION_AFFECT(obj)) : "not custom");
			sprintf(storage + strlen(storage), "<%scooldown\t0> [%d] %s, <%scdtime\t0> %d second%s\r\n", OLC_LABEL_VAL(GET_POTION_COOLDOWN_TYPE(obj), NOTHING), GET_POTION_COOLDOWN_TYPE(obj), GET_POTION_COOLDOWN_TYPE(obj) != NOTHING ? get_generic_name_by_vnum(GET_POTION_COOLDOWN_TYPE(obj)) : "no cooldown", OLC_LABEL_VAL(GET_POTION_COOLDOWN_TIME(obj), 0), GET_POTION_COOLDOWN_TIME(obj), PLURAL(GET_POTION_COOLDOWN_TIME(obj)));
			break;
		}
		case ITEM_POISON: {
			sprintf(storage + strlen(storage), "<%saffecttype\t0> [%d] %s\r\n", OLC_LABEL_VAL(GET_POISON_AFFECT(obj), NOTHING), GET_POISON_AFFECT(obj), GET_POISON_AFFECT(obj) != NOTHING ? get_generic_name_by_vnum(GET_POISON_AFFECT(obj)) : "not custom");
			sprintf(storage + strlen(storage), "<%scharges\t0> %d\r\n", OLC_LABEL_VAL(GET_POISON_CHARGES(obj), 0), GET_POISON_CHARGES(obj));
			break;
		}
		case ITEM_RECIPE: {
			craft_data *cft = craft_proto(GET_RECIPE_VNUM(obj));
			sprintf(storage + strlen(storage), "<%srecipe\t0> %d %s\r\n", OLC_LABEL_VAL(GET_RECIPE_VNUM(obj), 0), GET_RECIPE_VNUM(obj), cft ? GET_CRAFT_NAME(cft) : "none");
			break;
		}
		case ITEM_ARMOR: {
			sprintf(storage + strlen(storage), "<%sarmortype\t0> %s\r\n", OLC_LABEL_VAL(GET_ARMOR_TYPE(obj), 0), armor_types[GET_ARMOR_TYPE(obj)]);
			break;
		}
		case ITEM_BOOK: {
			book = book_proto(GET_BOOK_ID(obj));
			sprintf(storage + strlen(storage), "<%stext\t0> [%d] %s\r\n", OLC_LABEL_VAL(GET_BOOK_ID(obj), 0), GET_BOOK_ID(obj), (book ? book->title : "not set"));
			break;
		}
		case ITEM_WEALTH: {
			sprintf(storage + strlen(storage), "<%swealth\t0> %d\r\n", OLC_LABEL_VAL(GET_WEALTH_VALUE(obj), 0), GET_WEALTH_VALUE(obj));
			sprintf(storage + strlen(storage), "<%sautomint\t0> %s\r\n", OLC_LABEL_VAL(GET_WEALTH_AUTOMINT(obj), 0), offon_types[GET_WEALTH_AUTOMINT(obj)]);
			break;
		}
		case ITEM_LIGHTER: {
			if (GET_LIGHTER_USES(obj) == UNLIMITED) {
				sprintf(storage + strlen(storage), "<%suses\t0> unlimited\r\n", OLC_LABEL_VAL(GET_LIGHTER_USES(obj), 0));
			}
			else {
				sprintf(storage + strlen(storage), "<%suses\t0> %d\r\n", OLC_LABEL_VAL(GET_LIGHTER_USES(obj), 0), GET_LIGHTER_USES(obj));
			}
			break;
		}
		case ITEM_MINIPET: {
			sprintf(storage + strlen(storage), "<%sminipet\t0> %d %s\r\n", OLC_LABEL_VAL(GET_MINIPET_VNUM(obj), NOTHING), GET_MINIPET_VNUM(obj), get_mob_name_by_proto(GET_MINIPET_VNUM(obj)));
			break;
		}
		
		// types with no vals
		case ITEM_BOARD:
		case ITEM_MAIL:
		case ITEM_SHIELD:
		case ITEM_SHIP:
		case ITEM_CART:
		case ITEM_WORN: {
			break;
		}
		
		// generic
		case ITEM_OTHER:
		default: {
			// don't show "value2" (val 1) if it's plantable
			if (OBJ_FLAGGED(obj, OBJ_PLANTABLE) && GET_OBJ_VAL(obj, VAL_FOOD_CROP_TYPE) >= 0) {
				sprintf(storage + strlen(storage), "<%svalue0\t0> %d, <%svalue2\t0> %d\r\n", OLC_LABEL_VAL(GET_OBJ_VAL(obj, 0), 0), GET_OBJ_VAL(obj, 0), OLC_LABEL_VAL(GET_OBJ_VAL(obj, 2), 0), GET_OBJ_VAL(obj, 2));
			}
			else {
				sprintf(storage + strlen(storage), "<%svalue0\t0> %d, <%svalue1\t0> %d, <%svalue2\t0> %d\r\n", OLC_LABEL_VAL(GET_OBJ_VAL(obj, 0), 0), GET_OBJ_VAL(obj, 0), OLC_LABEL_VAL(GET_OBJ_VAL(obj, 1), 0), GET_OBJ_VAL(obj, 1), OLC_LABEL_VAL(GET_OBJ_VAL(obj, 2), 0), GET_OBJ_VAL(obj, 2));
			}
			break;
		}
	}
	
	// this is added for all of them
	if (OBJ_FLAGGED(obj, OBJ_PLANTABLE) && GET_OBJ_VAL(obj, VAL_FOOD_CROP_TYPE) >= 0) {
		sprintf(storage + strlen(storage), "<%splants\t0> [%d] %s\r\n", OLC_LABEL_VAL(GET_OBJ_VAL(obj, VAL_FOOD_CROP_TYPE), 0), GET_OBJ_VAL(obj, VAL_FOOD_CROP_TYPE), GET_CROP_NAME(crop_proto(GET_OBJ_VAL(obj, VAL_FOOD_CROP_TYPE))));
	}
}


/**
* This is the main object display for object OLC. It displays the user's
* currently-edited object.
*
* @param char_data *ch The person who is editing an object and will see its display.
*/
void olc_show_object(char_data *ch) {
	void get_extra_desc_display(struct extra_descr_data *list, char *save_buffer);
	void get_interaction_display(struct interaction_item *list, char *save_buffer);
	void get_script_display(struct trig_proto_list *list, char *save_buffer);
	
	obj_data *obj = GET_OLC_OBJECT(ch->desc);
	struct obj_storage_type *store;
	struct custom_message *ocm;
	struct obj_apply *apply;
	int count, minutes;
	bld_data *bld;
	
	if (!obj) {
		return;
	}
	
	*buf = '\0';
	sprintf(buf + strlen(buf), "[%s%d\t0] %s%s\t0 (Gear rating [%s%.2f\t0])\r\n", OLC_LABEL_CHANGED, GET_OLC_VNUM(ch->desc), OLC_LABEL_UNCHANGED, !obj_proto(GET_OLC_VNUM(ch->desc)) ? "new object" : get_obj_name_by_proto(GET_OLC_VNUM(ch->desc)), OLC_LABEL_CHANGED, rate_item(obj));
	sprintf(buf + strlen(buf), "<%skeywords\t0> %s\r\n", OLC_LABEL_STR(GET_OBJ_KEYWORDS(obj), default_obj_keywords), GET_OBJ_KEYWORDS(obj));
	sprintf(buf + strlen(buf), "<%sshortdescription\t0> %s\r\n", OLC_LABEL_STR(GET_OBJ_SHORT_DESC(obj), default_obj_short), GET_OBJ_SHORT_DESC(obj));
	sprintf(buf + strlen(buf), "<%slongdescription\t0> %s\r\n", OLC_LABEL_STR(GET_OBJ_LONG_DESC(obj), default_obj_long), GET_OBJ_LONG_DESC(obj));
	sprintf(buf + strlen(buf), "<%slookdescription\t0>\r\n%s", OLC_LABEL_STR(GET_OBJ_ACTION_DESC(obj), ""), NULLSAFE(GET_OBJ_ACTION_DESC(obj)));
	
	if (GET_OBJ_TIMER(obj) > 0) {
		minutes = GET_OBJ_TIMER(obj) * SECS_PER_MUD_HOUR / SECS_PER_REAL_MIN;
		sprintf(buf1, "%d ticks (%d:%02d)", GET_OBJ_TIMER(obj), minutes / 60, minutes % 60);
	}
	else {
		strcpy(buf1, "none");
	}
	sprintf(buf + strlen(buf), "<%stype\t0> %s, <%smaterial\t0> %s, <%stimer\t0> %s\r\n", OLC_LABEL_VAL(GET_OBJ_TYPE(obj), 0), item_types[(int) GET_OBJ_TYPE(obj)], OLC_LABEL_VAL(GET_OBJ_MATERIAL(obj), 0), materials[GET_OBJ_MATERIAL(obj)].name, OLC_LABEL_VAL(GET_OBJ_TIMER(obj), 0), buf1);
	
	sprintbit(GET_OBJ_WEAR(obj), wear_bits, buf1, TRUE);
	sprintf(buf + strlen(buf), "<%swear\t0> %s\r\n", OLC_LABEL_VAL(GET_OBJ_WEAR(obj), ITEM_WEAR_TAKE), buf1);
	
	sprintbit(GET_OBJ_EXTRA(obj), extra_bits, buf1, TRUE);
	sprintf(buf + strlen(buf), "<%sflags\t0> %s\r\n", OLC_LABEL_VAL(GET_OBJ_EXTRA(obj), NOBITS), buf1);
	
	sprintf(buf + strlen(buf), "<%scomponent\t0> %s\r\n", OLC_LABEL_VAL(GET_OBJ_CMP_TYPE(obj), 0), component_types[GET_OBJ_CMP_TYPE(obj)]);
	if (GET_OBJ_CMP_TYPE(obj) != CMP_NONE) {
		prettier_sprintbit(GET_OBJ_CMP_FLAGS(obj), component_flags, buf1);
		sprintf(buf + strlen(buf), "<%scompflags\t0> %s\r\n", OLC_LABEL_VAL(GET_OBJ_CMP_FLAGS(obj), NOBITS), buf1);
	}
	
	if (GET_OBJ_MIN_SCALE_LEVEL(obj) > 0) {
		sprintf(buf + strlen(buf), "<%sminlevel\t0> %d\r\n", OLC_LABEL_CHANGED, GET_OBJ_MIN_SCALE_LEVEL(obj));
	}
	else {
		sprintf(buf + strlen(buf), "<%sminlevel\t0> none\r\n", OLC_LABEL_UNCHANGED);
	}
	if (GET_OBJ_MAX_SCALE_LEVEL(obj) > 0) {
		sprintf(buf + strlen(buf), "<%smaxlevel\t0> %d\r\n", OLC_LABEL_CHANGED, GET_OBJ_MAX_SCALE_LEVEL(obj));
	}
	else {
		sprintf(buf + strlen(buf), "<%smaxlevel\t0> none\r\n", OLC_LABEL_UNCHANGED);
	}
	
	if (GET_OBJ_REQUIRES_QUEST(obj) != NOTHING) {
		sprintf(buf + strlen(buf), "<%srequiresquest\t0> [%d] %s\r\n", OLC_LABEL_CHANGED, GET_OBJ_REQUIRES_QUEST(obj), get_quest_name_by_proto(GET_OBJ_REQUIRES_QUEST(obj)));
	}
	else {
		sprintf(buf + strlen(buf), "<%srequiresquest\t0> none\r\n", OLC_LABEL_UNCHANGED);
	}
	
	olc_get_values_display(ch, buf1);
	strcat(buf, buf1);

	sprintbit(GET_OBJ_AFF_FLAGS(obj), affected_bits, buf1, TRUE);
	sprintf(buf + strlen(buf), "<%saffects\t0> %s\r\n", OLC_LABEL_VAL(GET_OBJ_AFF_FLAGS(obj), NOBITS), buf1);
	
	// applies / affected[]
	count = 0;
	sprintf(buf + strlen(buf), "Attribute Applies: <%sapply\t0>%s\r\n", OLC_LABEL_PTR(GET_OBJ_APPLIES(obj)), OBJ_FLAGGED(obj, OBJ_SCALABLE) ? " (scalable)" : "");
	for (apply = GET_OBJ_APPLIES(obj); apply; apply = apply->next) {
		sprintf(buf + strlen(buf), " \ty%2d\t0. %+d to %s (%s)\r\n", ++count, apply->modifier, apply_types[(int) apply->location], apply_type_names[(int)apply->apply_type]);
	}
	
	// exdesc
	sprintf(buf + strlen(buf), "Extra descriptions: <%sextra\t0>\r\n", OLC_LABEL_PTR(obj->ex_description));
	if (obj->ex_description) {
		get_extra_desc_display(obj->ex_description, buf1);
		strcat(buf, buf1);
	}

	sprintf(buf + strlen(buf), "Interactions: <%sinteraction\t0>\r\n", OLC_LABEL_PTR(obj->interactions));
	if (obj->interactions) {
		get_interaction_display(obj->interactions, buf1);
		strcat(buf, buf1);
	}
	
	// storage
	sprintf(buf + strlen(buf), "Storage: <%sstorage\t0>\r\n", OLC_LABEL_PTR(obj->storage));
	count = 0;
	for (store = obj->storage; store; store = store->next) {
		bld = building_proto(store->building_type);
		
		sprintbit(store->flags, storage_bits, buf2, TRUE);
		sprintf(buf + strlen(buf), " \ty%d\t0. [%d] %s ( %s)\r\n", ++count, GET_BLD_VNUM(bld), GET_BLD_NAME(bld), buf2);
	}
	
	// custom messages
	sprintf(buf + strlen(buf), "Custom messages: <%scustom\t0>\r\n", OLC_LABEL_PTR(obj->custom_msgs));
	count = 0;
	for (ocm = obj->custom_msgs; ocm; ocm = ocm->next) {
		sprintf(buf + strlen(buf), " \ty%d\t0. [%s] %s\r\n", ++count, obj_custom_types[ocm->type], ocm->msg);
	}
	
	// scripts
	sprintf(buf + strlen(buf), "Scripts: <%sscript\t0>\r\n", OLC_LABEL_PTR(obj->proto_script));
	if (obj->proto_script) {
		get_script_display(obj->proto_script, buf1);
		strcat(buf, buf1);
	}
	
	page_string(ch->desc, buf, TRUE);
}


 //////////////////////////////////////////////////////////////////////////////
//// EDIT MODULES ////////////////////////////////////////////////////////////

OLC_MODULE(oedit_action_desc) {
	obj_data *obj = GET_OLC_OBJECT(ch->desc);

	if (ch->desc->str) {
		msg_to_char(ch, "You are already editing a string.\r\n");
	}
	else {
		sprintf(buf, "description for %s", GET_OBJ_SHORT_DESC(obj));
		start_string_editor(ch->desc, buf, &GET_OBJ_ACTION_DESC(obj), MAX_ITEM_DESCRIPTION, TRUE);
	}
}


OLC_MODULE(oedit_affects) {
	obj_data *obj = GET_OLC_OBJECT(ch->desc);
	GET_OBJ_AFF_FLAGS(obj) = olc_process_flag(ch, argument, "affects", "affects", affected_bits, GET_OBJ_AFF_FLAGS(obj));
}


OLC_MODULE(oedit_affecttype) {
	obj_data *obj = GET_OLC_OBJECT(ch->desc);
	any_vnum old;
	int pos;
	
	switch (GET_OBJ_TYPE(obj)) {
		case ITEM_POTION: {
			pos = VAL_POTION_AFFECT;
			break;
		}
		case ITEM_POISON: {
			pos = VAL_POISON_AFFECT;
			break;
		}
		default: {
			msg_to_char(ch, "You can't set an affecttype on this type of item.\r\n");
			return;
		}
	}
	
	if (!str_cmp(argument, "none")) {
		GET_OBJ_VAL(obj, pos) = NOTHING;
		msg_to_char(ch, "It now has no custom affect type.\r\n");
	}
	else {
		old = GET_OBJ_VAL(obj, pos);
		GET_OBJ_VAL(obj, pos) = olc_process_number(ch, argument, "affect vnum", "affecttype", 0, MAX_VNUM, GET_OBJ_VAL(obj, pos));

		if (!find_generic(GET_OBJ_VAL(obj, pos), GENERIC_AFFECT)) {
			msg_to_char(ch, "Invalid affect generic vnum %d. Old value restored.\r\n", GET_OBJ_VAL(obj, pos));
			GET_OBJ_VAL(obj, pos) = old;
		}
		else {
			msg_to_char(ch, "Affect type set to [%d] %s.\r\n", GET_OBJ_VAL(obj, pos), get_generic_name_by_vnum(GET_OBJ_VAL(obj, pos)));
		}
	}
}


OLC_MODULE(oedit_apply) {
	obj_data *obj = GET_OLC_OBJECT(ch->desc);
	char arg1[MAX_INPUT_LENGTH], arg2[MAX_INPUT_LENGTH], arg3[MAX_INPUT_LENGTH], arg4[MAX_INPUT_LENGTH];
	char num_arg[MAX_INPUT_LENGTH], type_arg[MAX_INPUT_LENGTH], val_arg[MAX_INPUT_LENGTH];
	int loc, num, iter, apply_type;
	struct obj_apply *apply, *change, *temp;
	bool found;
	
	// arg1 arg2 arg3
	half_chop(argument, arg1, buf);
	half_chop(buf, arg2, arg3);
	
	if (is_abbrev(arg1, "remove")) {
		if (!*arg2) {
			msg_to_char(ch, "Remove which apply (number)?\r\n");
		}
		else if (!str_cmp(arg2, "all")) {
			free_obj_apply_list(GET_OBJ_APPLIES(obj));
			GET_OBJ_APPLIES(obj) = NULL;
			msg_to_char(ch, "You remove all the applies.\r\n");
		}
		else if (!isdigit(*arg2) || (num = atoi(arg2)) < 1) {
			msg_to_char(ch, "Invalid apply number.\r\n");
		}
		else {
			found = FALSE;
			for (apply = GET_OBJ_APPLIES(obj); apply && !found; apply = apply->next) {
				if (--num == 0) {
					found = TRUE;
					
					msg_to_char(ch, "You remove the %+d to %s (%s).\r\n", apply->modifier, apply_types[(int)apply->location], apply_type_names[(int)apply->apply_type]);
					REMOVE_FROM_LIST(apply, GET_OBJ_APPLIES(obj), next);
					free(apply);
				}
			}
			
			if (!found) {
				msg_to_char(ch, "Invalid apply number.\r\n");
			}
		}
	}
	else if (is_abbrev(arg1, "add")) {
		strcpy(arg1, arg3);
		half_chop(arg1, arg3, arg4);
		
		num = atoi(arg2);
		apply_type = APPLY_TYPE_NATURAL;	// default
		
		if (!*arg2 || !*arg3 || (!isdigit(*arg2) && *arg2 != '-')) {
			msg_to_char(ch, "Usage: apply add <value> <apply> [type]\r\n");
		}
		else if ((loc = search_block(arg3, apply_types, FALSE)) == NOTHING) {
			msg_to_char(ch, "Invalid apply.\r\n");
		}
		else if (*arg4 && (apply_type = search_block(arg4, apply_type_names, FALSE)) == NOTHING) {
			msg_to_char(ch, "Invalid apply type.\r\n");
		}
		else {
			CREATE(apply, struct obj_apply, 1);
			apply->location = loc;
			apply->modifier = num;
			apply->apply_type = apply_type;
			
			// append to end
			if ((temp = GET_OBJ_APPLIES(obj))) {
				while (temp->next) {
					temp = temp->next;
				}
				temp->next = apply;
			}
			else {
				GET_OBJ_APPLIES(obj) = apply;
			}
			
			msg_to_char(ch, "You add %+d to %s (%s).\r\n", num, apply_types[loc], apply_type_names[apply_type]);
		}
	}
	else if (is_abbrev(arg1, "change")) {
		strcpy(num_arg, arg2);
		half_chop(arg3, type_arg, val_arg);
		
		if (!*num_arg || !isdigit(*num_arg) || !*type_arg || !*val_arg) {
			msg_to_char(ch, "Usage: apply change <number> <value | apply | type> <new value>\r\n");
			return;
		}
		
		// find which one to change
		if (!isdigit(*num_arg) || (num = atoi(num_arg)) < 1) {
			msg_to_char(ch, "Invalid apply number.\r\n");
			return;
		}
		change = NULL;
		for (apply = GET_OBJ_APPLIES(obj); apply && !change; apply = apply->next) {
			if (--num == 0) {
				change = apply;
				break;
			}
		}
		if (!change) {
			msg_to_char(ch, "Invalid apply number.\r\n");
		}
		else if (is_abbrev(type_arg, "value") || is_abbrev(type_arg, "amount")) {
			num = atoi(val_arg);
			if ((!isdigit(*val_arg) && *val_arg != '-') || num == 0) {
				msg_to_char(ch, "Invalid value '%s'.\r\n", val_arg);
			}
			else {
				change->modifier = num;
				msg_to_char(ch, "Apply %d changed to value %+d.\r\n", atoi(num_arg), num);
			}
		}
		else if (is_abbrev(type_arg, "apply")) {
			if ((loc = search_block(val_arg, apply_types, FALSE)) == NOTHING) {
				msg_to_char(ch, "Invalid apply.\r\n");
			}
			else {
				change->location = loc;
				msg_to_char(ch, "Apply %d changed to %s.\r\n", atoi(num_arg), apply_types[loc]);
			}
		}
		else if (is_abbrev(type_arg, "type")) {
			if ((loc = search_block(val_arg, apply_type_names, FALSE)) == NOTHING) {
				msg_to_char(ch, "Invalid apply type.\r\n");
			}
			else {
				change->apply_type = loc;
				msg_to_char(ch, "Apply %d changed to %s.\r\n", atoi(num_arg), apply_type_names[loc]);
			}
		}
		else {
			msg_to_char(ch, "You can only change the value, apply, or type.\r\n");
		}
	}
	else {
		msg_to_char(ch, "Usage: apply add <value> <apply> [type]\r\n");
		msg_to_char(ch, "Usage: apply change <number> <value | apply | type> <new value>\r\n");
		msg_to_char(ch, "Usage: apply remove <number | all>\r\n");
		
		msg_to_char(ch, "Available applies:\r\n");
		for (iter = 0; *apply_types[iter] != '\n'; ++iter) {
			msg_to_char(ch, " %-24.24s%s", apply_types[iter], ((iter % 2) ? "\r\n" : ""));
		}
		if ((iter % 2) != 0) {
			msg_to_char(ch, "\r\n");
		}
		
		msg_to_char(ch, "Available types:\r\n");
		for (iter = 0; *apply_type_names[iter] != '\n'; ++iter) {
			msg_to_char(ch, " %-24.24s%s", apply_type_names[iter], ((iter % 2) ? "\r\n" : ""));
		}
		if ((iter % 2) != 0) {
			msg_to_char(ch, "\r\n");
		}
	}
}


OLC_MODULE(oedit_armortype) {
	obj_data *obj = GET_OLC_OBJECT(ch->desc);
	
	if (!IS_ARMOR(obj)) {
		msg_to_char(ch, "You can only set armor on an armor object.\r\n");
	}
	else {
		GET_OBJ_VAL(obj, VAL_ARMOR_TYPE) = olc_process_type(ch, argument, "armor type", "armortype", armor_types, GET_OBJ_VAL(obj, VAL_ARMOR_TYPE));
	}
}


OLC_MODULE(oedit_ammotype) {
	obj_data *obj = GET_OLC_OBJECT(ch->desc);
	int slot = NOTHING;
	char val;
	
	// this chooses the VAL slot and validates the type
	switch (GET_OBJ_TYPE(obj)) {
		case ITEM_MISSILE_WEAPON: {
			slot = VAL_MISSILE_WEAPON_AMMO_TYPE;
			break;
		}
		case ITEM_AMMO: {
			slot = VAL_AMMO_TYPE;
			break;
		}
	}
	
	if (slot == NOTHING) {
		msg_to_char(ch, "You can only set ammotype for missile weapons and ammo.\r\n");
	}
	else if (strlen(argument) > 1 || (val = UPPER(*argument)) < 'A' || val > 'Z') {
		msg_to_char(ch, "Ammotype must by a letter from A to Z.\r\n");
	}
	else {
		GET_OBJ_VAL(obj, slot) = val - 'A';
		msg_to_char(ch, "You set the ammotype to %c.\r\n", val);
	}
}


OLC_MODULE(oedit_automint) {
	obj_data *obj = GET_OLC_OBJECT(ch->desc);
	
	if (GET_OBJ_TYPE(obj) != ITEM_WEALTH) {
		msg_to_char(ch, "You can only set the automint value on a wealth item.\r\n");
	}
	else {
		GET_OBJ_VAL(obj, VAL_WEALTH_AUTOMINT) = olc_process_type(ch, argument, "automint value", "automint", offon_types, GET_OBJ_VAL(obj, VAL_WEALTH_AUTOMINT));
	}
}


// formerly oedit_book, which had a name conflict with ".book"
OLC_MODULE(oedit_text) {
	obj_data *obj = GET_OLC_OBJECT(ch->desc);
	int old = GET_OBJ_VAL(obj, VAL_BOOK_ID);
	book_data *book;
	
	if (!IS_BOOK(obj)) {
		msg_to_char(ch, "You can only set text id on a book.\r\n");
	}
	else {
		GET_OBJ_VAL(obj, VAL_BOOK_ID) = olc_process_number(ch, argument, "text id", "text", 0, MAX_INT, GET_OBJ_VAL(obj, VAL_BOOK_ID));
		book = book_proto(GET_BOOK_ID(obj));
		
		if (!book) {
			msg_to_char(ch, "Invalid text book vnum. Old vnum %d restored.\r\n", old);
			GET_OBJ_VAL(obj, VAL_BOOK_ID) = old;
		}
	}
}


OLC_MODULE(oedit_capacity) {
	obj_data *obj = GET_OLC_OBJECT(ch->desc);
	int slot = NOTHING;
	
	// this chooses the VAL slot and validates the type
	switch (GET_OBJ_TYPE(obj)) {
		case ITEM_CONTAINER: {
			slot = VAL_CONTAINER_MAX_CONTENTS;
			break;
		}
		case ITEM_DRINKCON: {
			slot = VAL_DRINK_CONTAINER_CAPACITY;
			break;
		}
		case ITEM_PACK: {
			slot = VAL_PACK_CAPACITY;
			break;
		}
	}
	
	if (slot == NOTHING) {
		msg_to_char(ch, "You can't set capacity on this type of object.\r\n");
	}
	else {
		GET_OBJ_VAL(obj, slot) = olc_process_number(ch, argument, "capacity", "capacity", 0, MAX_INT, GET_OBJ_VAL(obj, slot));
	}
}


OLC_MODULE(oedit_cdtime) {
	obj_data *obj = GET_OLC_OBJECT(ch->desc);
	
	GET_OBJ_VAL(obj, VAL_POTION_COOLDOWN_TIME) = olc_process_number(ch, argument, "cooldown time", "cdtime", 0, MAX_INT, GET_OBJ_VAL(obj, VAL_POTION_COOLDOWN_TIME));
}


OLC_MODULE(oedit_charges) {
	obj_data *obj = GET_OLC_OBJECT(ch->desc);
	
	if (!IS_POISON(obj)) {
		msg_to_char(ch, "You can only set charges on a poison.\r\n");
	}
	else {
		GET_OBJ_VAL(obj, VAL_POISON_CHARGES) = olc_process_number(ch, argument, "charges", "charges", 0, MAX_INT, GET_OBJ_VAL(obj, VAL_POISON_CHARGES));
	}
}


OLC_MODULE(oedit_coinamount) {
	obj_data *obj = GET_OLC_OBJECT(ch->desc);
	int pos;
	
	switch (GET_OBJ_TYPE(obj)) {
		case ITEM_COINS: {
			pos = VAL_COINS_AMOUNT;
			break;
		}
		case ITEM_CURRENCY: {
			pos = VAL_CURRENCY_AMOUNT;
			break;
		}
		default: {
			msg_to_char(ch, "You can only set this on coins or currency.\r\n");
			return;
		}
	}
	
	GET_OBJ_VAL(obj, pos) = olc_process_number(ch, argument, "coin amount", "coinamount", 0, MAX_COIN, GET_OBJ_VAL(obj, pos));
}


OLC_MODULE(oedit_compflags) {
	obj_data *obj = GET_OLC_OBJECT(ch->desc);
	
	if (GET_OBJ_CMP_TYPE(obj) == CMP_NONE) {
		msg_to_char(ch, "You can only set component flags once you've set a component type.\r\n");
	}
	else {
		GET_OBJ_CMP_FLAGS(obj) = olc_process_flag(ch, argument, "component", "compflags", component_flags, GET_OBJ_CMP_FLAGS(obj));
	}
}


OLC_MODULE(oedit_component) {
	obj_data *obj = GET_OLC_OBJECT(ch->desc);
	int old = GET_OBJ_CMP_TYPE(obj);
	
	GET_OBJ_CMP_TYPE(obj) = olc_process_type(ch, argument, "component type", "component", component_types, GET_OBJ_CMP_TYPE(obj));
	
	// reset flags if going to/from none
	if (old == CMP_NONE || GET_OBJ_CMP_TYPE(obj) == CMP_NONE) {
		GET_OBJ_CMP_FLAGS(obj) = NOBITS;
	}
}


OLC_MODULE(oedit_containerflags) {
	obj_data *obj = GET_OLC_OBJECT(ch->desc);
	
	if (!IS_CONTAINER(obj)) {
		msg_to_char(ch, "You can only set containerflags on a container.\r\n");
	}
	else {
		GET_OBJ_VAL(obj, VAL_CONTAINER_FLAGS) = olc_process_flag(ch, argument, "container", "containerflags", container_bits, GET_OBJ_VAL(obj, VAL_CONTAINER_FLAGS));
	}
}


OLC_MODULE(oedit_contents) {
	obj_data *obj = GET_OLC_OBJECT(ch->desc);
	
	if (!IS_DRINK_CONTAINER(obj)) {
		msg_to_char(ch, "You can only set contents on a drink container.\r\n");
	}
	else {
		GET_OBJ_VAL(obj, VAL_DRINK_CONTAINER_CONTENTS) = olc_process_number(ch, argument, "contents", "contents", 0, GET_DRINK_CONTAINER_CAPACITY(obj), GET_OBJ_VAL(obj, VAL_DRINK_CONTAINER_CONTENTS));
	}
}


OLC_MODULE(oedit_cooldown) {
	obj_data *obj = GET_OLC_OBJECT(ch->desc);
	any_vnum old;
	
	if (!IS_POTION(obj)) {
		msg_to_char(ch, "You can only set cooldown type on a potion.\r\n");
	}
	else if (!str_cmp(argument, "none")) {
		GET_OBJ_VAL(obj, VAL_POTION_COOLDOWN_TYPE) = NOTHING;
		msg_to_char(ch, "It now has no cooldown type.\r\n");
	}
	else {
		old = GET_OBJ_VAL(obj, VAL_POTION_COOLDOWN_TYPE);
		GET_OBJ_VAL(obj, VAL_POTION_COOLDOWN_TYPE) = olc_process_number(ch, argument, "cooldown vnum", "cooldown", 0, MAX_VNUM, GET_OBJ_VAL(obj, VAL_POTION_COOLDOWN_TYPE));

		if (!find_generic(GET_OBJ_VAL(obj, VAL_POTION_COOLDOWN_TYPE), GENERIC_COOLDOWN)) {
			msg_to_char(ch, "Invalid cooldown generic vnum %d. Old value restored.\r\n", GET_OBJ_VAL(obj, VAL_POTION_COOLDOWN_TYPE));
			GET_OBJ_VAL(obj, VAL_POTION_COOLDOWN_TYPE) = old;
		}
		else {
			msg_to_char(ch, "Cooldown type set to [%d] %s.\r\n", GET_OBJ_VAL(obj, VAL_POTION_COOLDOWN_TYPE), get_generic_name_by_vnum(GET_OBJ_VAL(obj, VAL_POTION_COOLDOWN_TYPE)));
		}
	}
}


OLC_MODULE(oedit_corpseof) {
	obj_data *obj = GET_OLC_OBJECT(ch->desc);
	mob_vnum old = GET_CORPSE_NPC_VNUM(obj);
	
	if (!IS_CORPSE(obj)) {
		msg_to_char(ch, "You can only set this on a corpse.\r\n");
	}
	else {
		GET_OBJ_VAL(obj, VAL_CORPSE_IDNUM) = olc_process_number(ch, argument, "corpse of", "corpseof", 0, MAX_VNUM, GET_OBJ_VAL(obj, VAL_CORPSE_IDNUM));
		if (!mob_proto(GET_CORPSE_NPC_VNUM(obj))) {
			GET_OBJ_VAL(obj, VAL_CORPSE_IDNUM) = old;
			msg_to_char(ch, "There is no mobile with that vnum. Old value restored.\r\n");
		}
		else if (!PRF_FLAGGED(ch, PRF_NOREPEAT)) {
			msg_to_char(ch, "It is now the corpse of: %s\r\n", get_mob_name_by_proto(GET_CORPSE_NPC_VNUM(obj)));
		}
		else {
			send_config_msg(ch, "ok_string");
		}
	}
}


OLC_MODULE(oedit_currency) {
	obj_data *obj = GET_OLC_OBJECT(ch->desc);
	any_vnum old = GET_CURRENCY_VNUM(obj);
	
	if (!IS_CURRENCY(obj)) {
		msg_to_char(ch, "You can only set that on a currency object.\r\n");
	}
	else {
		GET_OBJ_VAL(obj, VAL_CURRENCY_VNUM) = olc_process_number(ch, argument, "currency vnum", "currency", 0, MAX_VNUM, GET_OBJ_VAL(obj, VAL_CURRENCY_VNUM));
		if (GET_CURRENCY_VNUM(obj) != old && !find_generic(GET_CURRENCY_VNUM(obj), GENERIC_CURRENCY)) {
			msg_to_char(ch, "%d is not a currency generic. Old value restored.\r\n", GET_CURRENCY_VNUM(obj));
			GET_OBJ_VAL(obj, VAL_CURRENCY_VNUM) = old;
		}
	}
}


OLC_MODULE(oedit_custom) {
	void olc_process_custom_messages(char_data *ch, char *argument, struct custom_message **list, const char **type_names);
	obj_data *obj = GET_OLC_OBJECT(ch->desc);
	olc_process_custom_messages(ch, argument, &(obj->custom_msgs), obj_custom_types);
}


OLC_MODULE(oedit_damage) {
	obj_data *obj = GET_OLC_OBJECT(ch->desc);
	int slot = NOTHING;
	bool showdps = FALSE;
	
	// this chooses the VAL slot and validates the type
	switch (GET_OBJ_TYPE(obj)) {
		case ITEM_WEAPON: {
			slot = VAL_WEAPON_DAMAGE_BONUS;
			showdps = TRUE;
			break;
		}
		case ITEM_MISSILE_WEAPON: {
			slot = VAL_MISSILE_WEAPON_DAMAGE;
			showdps = TRUE;
			break;
		}
		case ITEM_AMMO: {
			slot = VAL_AMMO_DAMAGE_BONUS;
			break;
		}
	}
	
	if (slot == NOTHING) {
		msg_to_char(ch, "You can't set damage on this type of object.\r\n");
	}
	else {
		GET_OBJ_VAL(obj, slot) = olc_process_number(ch, argument, "damage", "damage", -10000, 10000, GET_OBJ_VAL(obj, slot));
		if (showdps) {
			msg_to_char(ch, "It now has %.2f base dps.\r\n", get_base_dps(obj));
		}
	}
}


OLC_MODULE(oedit_extra_desc) {
	obj_data *obj = GET_OLC_OBJECT(ch->desc);
	olc_process_extra_desc(ch, argument, &obj->ex_description);
}


OLC_MODULE(oedit_flags) {
	obj_data *obj = GET_OLC_OBJECT(ch->desc);
	GET_OBJ_EXTRA(obj) = olc_process_flag(ch, argument, "extra", "flags", extra_bits, GET_OBJ_EXTRA(obj));
}


OLC_MODULE(oedit_fullness) {
	obj_data *obj = GET_OLC_OBJECT(ch->desc);
	
	if (!IS_FOOD(obj)) {
		msg_to_char(ch, "You can only set fullness on food.\r\n");
	}
	else {
		GET_OBJ_VAL(obj, VAL_FOOD_HOURS_OF_FULLNESS) = olc_process_number(ch, argument, "fullness", "fullness", -(MAX_CONDITION / REAL_UPDATES_PER_MUD_HOUR), 10 * (MAX_CONDITION / REAL_UPDATES_PER_MUD_HOUR), GET_OBJ_VAL(obj, VAL_FOOD_HOURS_OF_FULLNESS));
	}
}


OLC_MODULE(oedit_interaction) {
	obj_data *obj = GET_OLC_OBJECT(ch->desc);
	olc_process_interactions(ch, argument, &obj->interactions, TYPE_OBJ);
}


OLC_MODULE(oedit_keywords) {
	obj_data *obj = GET_OLC_OBJECT(ch->desc);
	olc_process_string(ch, argument, "keywords", &GET_OBJ_KEYWORDS(obj));
}


OLC_MODULE(oedit_liquid) {
	obj_data *obj = GET_OLC_OBJECT(ch->desc);
	any_vnum old;
	
	if (!IS_DRINK_CONTAINER(obj)) {
		msg_to_char(ch, "You can only set liquid on a drink container.\r\n");
	}
	else {
		old = GET_OBJ_VAL(obj, VAL_DRINK_CONTAINER_TYPE);
		GET_OBJ_VAL(obj, VAL_DRINK_CONTAINER_TYPE) = olc_process_number(ch, argument, "liquid vnum", "liquid", 0, MAX_VNUM, GET_OBJ_VAL(obj, VAL_DRINK_CONTAINER_TYPE));

		if (!find_generic(GET_OBJ_VAL(obj, VAL_DRINK_CONTAINER_TYPE), GENERIC_LIQUID)) {
			msg_to_char(ch, "Invalid liquid generic vnum %d. Old value restored.\r\n", GET_OBJ_VAL(obj, VAL_DRINK_CONTAINER_TYPE));
			GET_OBJ_VAL(obj, VAL_DRINK_CONTAINER_TYPE) = old;
		}
		else {
			msg_to_char(ch, "It now contains %s.\r\n", get_generic_name_by_vnum(GET_OBJ_VAL(obj, VAL_DRINK_CONTAINER_TYPE)));
		}
	}
}


OLC_MODULE(oedit_long_desc) {
	obj_data *obj = GET_OLC_OBJECT(ch->desc);
	olc_process_string(ch, argument, "long description", &GET_OBJ_LONG_DESC(obj));
}


OLC_MODULE(oedit_material) {
	obj_data *obj = GET_OLC_OBJECT(ch->desc);
	check_oedit_material_list();
	GET_OBJ_MATERIAL(obj) = olc_process_type(ch, argument, "material", "material", (const char**)olc_material_list, GET_OBJ_MATERIAL(obj));
}


OLC_MODULE(oedit_maxlevel) {
	obj_data *obj = GET_OLC_OBJECT(ch->desc);
	
	GET_OBJ_MAX_SCALE_LEVEL(obj) = olc_process_number(ch, argument, "maximum level", "maxlevel", 0, MAX_INT, GET_OBJ_MAX_SCALE_LEVEL(obj));
}


OLC_MODULE(oedit_minipet) {
	extern bitvector_t default_minipet_flags, default_minipet_affs;
	
	obj_data *obj = GET_OLC_OBJECT(ch->desc);
	mob_vnum old = GET_MINIPET_VNUM(obj);
	char buf[MAX_STRING_LENGTH];
	char_data *mob;
	
	if (!IS_MINIPET(obj)) {
		msg_to_char(ch, "You can only set this on a MINIPET item.\r\n");
	}
	else {
		GET_OBJ_VAL(obj, VAL_MINIPET_VNUM) = olc_process_number(ch, argument, "mini-pet", "minipet", 0, MAX_VNUM, GET_OBJ_VAL(obj, VAL_MINIPET_VNUM));
		if (!(mob = mob_proto(GET_MINIPET_VNUM(obj)))) {
			GET_OBJ_VAL(obj, VAL_MINIPET_VNUM) = old;
			msg_to_char(ch, "There is no mobile with that vnum. Old value restored.\r\n");
			return;
		}
		else if (!PRF_FLAGGED(ch, PRF_NOREPEAT)) {
			msg_to_char(ch, "It now gives the mini-pet: %s\r\n", get_mob_name_by_proto(GET_MINIPET_VNUM(obj)));
		}
		else {
			send_config_msg(ch, "ok_string");
		}
		
		if (!MOB_FLAGGED(mob, default_minipet_flags)) {
			sprintbit(default_minipet_flags, action_bits, buf, TRUE);
			msg_to_char(ch, "Warning: mob should have these flags: buf\r\n");
		}
		if (!AFF_FLAGGED(mob, default_minipet_affs)) {
			sprintbit(default_minipet_affs, affected_bits, buf, TRUE);
			msg_to_char(ch, "Warning: mob should have these affects: buf\r\n");
		}
	}
}


OLC_MODULE(oedit_minlevel) {
	obj_data *obj = GET_OLC_OBJECT(ch->desc);
	
	GET_OBJ_MIN_SCALE_LEVEL(obj) = olc_process_number(ch, argument, "minimum level", "minlevel", 0, MAX_INT, GET_OBJ_MIN_SCALE_LEVEL(obj));
}


OLC_MODULE(oedit_paint) {
	obj_data *obj = GET_OLC_OBJECT(ch->desc);
	
	if (!IS_PAINT(obj)) {
		msg_to_char(ch, "You can only set paint color on a paint object.\r\n");
	}
	else {
		GET_OBJ_VAL(obj, VAL_PAINT_COLOR) = olc_process_type(ch, argument, "paint color", "paint", paint_names, GET_OBJ_VAL(obj, VAL_PAINT_COLOR));
	}
}


OLC_MODULE(oedit_plants) {
	obj_data *obj = GET_OLC_OBJECT(ch->desc);
	crop_vnum old = GET_OBJ_VAL(obj, VAL_FOOD_CROP_TYPE);
	crop_data *cp;

	if (!OBJ_FLAGGED(obj, OBJ_PLANTABLE)) {
		msg_to_char(ch, "You can only set plants on a plantable item.\r\n");
	}
	else {
		GET_OBJ_VAL(obj, VAL_FOOD_CROP_TYPE) = olc_process_number(ch, argument, "plant", "plants", 0, MAX_VNUM, GET_OBJ_VAL(obj, VAL_FOOD_CROP_TYPE));
		if ((cp = crop_proto(GET_OBJ_VAL(obj, VAL_FOOD_CROP_TYPE)))) {
			msg_to_char(ch, "It will plant %s.\r\n", GET_CROP_NAME(cp));
		}
		else {
			msg_to_char(ch, "No such crop vnum. Old value restored.\r\n");
			GET_OBJ_VAL(obj, VAL_FOOD_CROP_TYPE) = old;
		}
	}
}


OLC_MODULE(oedit_quantity) {
	obj_data *obj = GET_OLC_OBJECT(ch->desc);
	
	if (!IS_AMMO(obj)) {
		msg_to_char(ch, "You can't set quantity on this type of item.\r\n");
	}
	else {
		GET_OBJ_VAL(obj, VAL_AMMO_QUANTITY) = olc_process_number(ch, argument, "quantity", "quantity", 0, 100, GET_OBJ_VAL(obj, VAL_AMMO_QUANTITY));
	}
}


OLC_MODULE(oedit_quick_recipe) {
	extern bool can_start_olc_edit(char_data *ch, int type, any_vnum vnum);
	extern const char *craft_types[];
	
	char new_vnum_arg[MAX_INPUT_LENGTH], from_vnum_arg[MAX_INPUT_LENGTH], buf[MAX_STRING_LENGTH], cmd[256];
	any_vnum new_vnum, from_vnum;
	craft_data *cft;
	obj_data *obj;
	
	argument = any_one_arg(argument, new_vnum_arg);
	argument = any_one_arg(argument, from_vnum_arg);
	skip_spaces(&argument);
	
	if (!*new_vnum_arg || !*from_vnum_arg) {
		msg_to_char(ch, "Usage: .quickrecipe <new vnum> <craft vnum> [pattern | design | etc]\r\n");
		return;
	}
	if (!isdigit(*new_vnum_arg) || (new_vnum = atoi(new_vnum_arg)) < 0 || new_vnum > MAX_VNUM) {
		msg_to_char(ch, "You must pick a valid vnum between 0 and %d.\r\n", MAX_VNUM);
		return;
	}
	if (obj_proto(new_vnum)) {
		msg_to_char(ch, "There is already an object with that vnum.\r\n");
		return;
	}
	if (!isdigit(*from_vnum_arg) || (from_vnum = atoi(from_vnum_arg)) < 0 || from_vnum > MAX_VNUM) {
		msg_to_char(ch, "You must pick a valid vnum between 0 and %d.\r\n", MAX_VNUM);
		return;
	}
	if (!(cft = craft_proto(from_vnum))) {
		msg_to_char(ch, "There is no craft with that vnum.\r\n");
		return;
	}
	if (!CRAFT_FLAGGED(cft, CRAFT_LEARNED)) {
		msg_to_char(ch, "The craft must have the LEARNED flag to do that.\r\n");
		return;
	}
	if (!can_start_olc_edit(ch, OLC_OBJECT, new_vnum)) {
		return;	// sends own message
	}
	
	// create it
	obj = GET_OLC_OBJECT(ch->desc) = setup_olc_object(NULL);
	GET_OBJ_TYPE(obj) = ITEM_RECIPE;
	GET_OBJ_VAL(obj, VAL_RECIPE_VNUM) = GET_CRAFT_VNUM(cft);
	
	// keywords
	snprintf(buf, sizeof(buf), "%s %s", (*argument ? argument : "recipe"), GET_CRAFT_NAME(cft));
	if (GET_OBJ_KEYWORDS(obj)) {
		free(GET_OBJ_KEYWORDS(obj));
	}
	GET_OBJ_KEYWORDS(obj) = str_dup(buf);
	
	// short desc
	snprintf(buf, sizeof(buf), "the %s %s", GET_CRAFT_NAME(cft), *argument ? argument : "recipe");
	if (GET_OBJ_SHORT_DESC(obj)) {
		free(GET_OBJ_SHORT_DESC(obj));
	}
	GET_OBJ_SHORT_DESC(obj) = str_dup(buf);
	
	// long desc
	snprintf(buf, sizeof(buf), "The %s %s is on the ground.", GET_CRAFT_NAME(cft), *argument ? argument : "recipe");
	if (GET_OBJ_LONG_DESC(obj)) {
		free(GET_OBJ_LONG_DESC(obj));
	}
	GET_OBJ_LONG_DESC(obj) = str_dup(buf);
	
	// look desc
	strcpy(cmd, craft_types[GET_CRAFT_TYPE(cft)]);
	snprintf(buf, sizeof(buf), "This %s will teach you to %s: %s.\r\n", (*argument ? argument : "recipe"), strtolower(cmd), GET_CRAFT_NAME(cft));
	if (GET_OBJ_ACTION_DESC(obj)) {
		free(GET_OBJ_ACTION_DESC(obj));
	}
	GET_OBJ_ACTION_DESC(obj) = str_dup(buf);
	
	// SUCCESS
	msg_to_char(ch, "You create a quick recipe %d:\r\n", new_vnum);
	GET_OLC_TYPE(ch->desc) = OLC_OBJECT;
	GET_OLC_VNUM(ch->desc) = new_vnum;
	
	// ensure some data
	GET_OBJ_VNUM(GET_OLC_OBJECT(ch->desc)) = new_vnum;
	olc_show_object(ch);
}


OLC_MODULE(oedit_recipe) {
	obj_data *obj = GET_OLC_OBJECT(ch->desc);
	any_vnum old = GET_RECIPE_VNUM(obj);
	craft_data *cft;
	
	if (!IS_RECIPE(obj)) {
		msg_to_char(ch, "You can only set that on a recipe object.\r\n");
	}
	else {
		GET_OBJ_VAL(obj, VAL_RECIPE_VNUM) = olc_process_number(ch, argument, "recipe vnum", "recipe", 0, MAX_VNUM, GET_OBJ_VAL(obj, VAL_RECIPE_VNUM));
		if (GET_RECIPE_VNUM(obj) != old && (!(cft = craft_proto(GET_RECIPE_VNUM(obj))) || !CRAFT_FLAGGED(cft, CRAFT_LEARNED))) {
			msg_to_char(ch, "%d is not a learned recipe. Old value restored.\r\n", GET_RECIPE_VNUM(obj));
			GET_OBJ_VAL(obj, VAL_RECIPE_VNUM) = old;
		}
	}
}


OLC_MODULE(oedit_requiresquest) {
	obj_data *obj = GET_OLC_OBJECT(ch->desc);
	obj_vnum old = GET_OBJ_REQUIRES_QUEST(obj);
	
	if (!str_cmp(argument, "none") || atoi(argument) == NOTHING) {
		GET_OBJ_REQUIRES_QUEST(obj) = NOTHING;
		if (PRF_FLAGGED(ch, PRF_NOREPEAT)) {
			send_config_msg(ch, "ok_string");
		}
		else {
			msg_to_char(ch, "It no longer requires a quest.\r\n");
		}
	}
	else {
		GET_OBJ_REQUIRES_QUEST(obj) = olc_process_number(ch, argument, "quest vnum", "requiresquest", 0, MAX_VNUM, GET_OBJ_REQUIRES_QUEST(obj));
		if (!quest_proto(GET_OBJ_REQUIRES_QUEST(obj))) {
			GET_OBJ_REQUIRES_QUEST(obj) = old;
			msg_to_char(ch, "There is no quest with that vnum. Old value restored.\r\n");
		}
		else if (!PRF_FLAGGED(ch, PRF_NOREPEAT)) {
			msg_to_char(ch, "It now requires %s.\r\n", get_quest_name_by_proto(GET_OBJ_REQUIRES_QUEST(obj)));
		}
	}
}


OLC_MODULE(oedit_roomvnum) {
	obj_data *obj = GET_OLC_OBJECT(ch->desc);
	
	if (!IS_PORTAL(obj)) {
		msg_to_char(ch, "You can only set roomvnum on a portal.\r\n");
	}
	else {
		GET_OBJ_VAL(obj, VAL_PORTAL_TARGET_VNUM) = olc_process_number(ch, argument, "room vnum", "roomvnum", 0, MAX_INT, GET_OBJ_VAL(obj, VAL_PORTAL_TARGET_VNUM));
	}
}


OLC_MODULE(oedit_script) {
	obj_data *obj = GET_OLC_OBJECT(ch->desc);
	olc_process_script(ch, argument, &(obj->proto_script), OBJ_TRIGGER);
}


OLC_MODULE(oedit_short_description) {
	obj_data *obj = GET_OLC_OBJECT(ch->desc);
	olc_process_string(ch, argument, "short description", &GET_OBJ_SHORT_DESC(obj));
}


OLC_MODULE(oedit_size) {
	obj_data *obj = GET_OLC_OBJECT(ch->desc);
	
	if (!IS_CORPSE(obj)) {
		msg_to_char(ch, "You can only set the size on a corpse.\r\n");
	}
	else {
		GET_OBJ_VAL(obj, VAL_CORPSE_SIZE) = olc_process_type(ch, argument, "size", "size", size_types, GET_CORPSE_SIZE(obj));
	}
}


OLC_MODULE(oedit_storage) {
	extern bld_data *get_building_by_name(char *name, bool room_only);
	
	obj_data *obj = GET_OLC_OBJECT(ch->desc);
	char arg1[MAX_INPUT_LENGTH], arg2[MAX_INPUT_LENGTH], *flagarg;
	char num_arg[MAX_INPUT_LENGTH], type_arg[MAX_INPUT_LENGTH], val_arg[MAX_INPUT_LENGTH];
	int loc, num, iter;
	struct obj_storage_type *store, *change, *temp;
	bld_vnum bld;
	bool found;
	
	// arg1 arg2
	half_chop(argument, arg1, arg2);
	
	if (is_abbrev(arg1, "remove")) {
		if (!*arg2) {
			msg_to_char(ch, "Remove which storage location (number)?\r\n");
		}
		else if (!str_cmp(arg2, "all")) {
			while ((store = obj->storage)) {
				obj->storage = store->next;
				free(store);
			}
			msg_to_char(ch, "You remove all the storage locations.\r\n");
		}
		else if (!isdigit(*arg2) || (num = atoi(arg2)) < 1) {
			msg_to_char(ch, "Invalid storage number.\r\n");
		}
		else {
			found = FALSE;
			for (store = obj->storage; store && !found; store = store->next) {
				if (--num == 0) {
					found = TRUE;
					
					msg_to_char(ch, "You remove the storage in the %s.\r\n", GET_BLD_NAME(building_proto(store->building_type)));
					
					REMOVE_FROM_LIST(store, obj->storage, next);
					free(store);
				}
			}
			
			if (!found) {
				msg_to_char(ch, "Invalid storage number.\r\n");
			}
		}
	}
	else if (is_abbrev(arg1, "add")) {
		flagarg = any_one_word(arg2, arg);
		bld = atoi(arg);
		
		// flagarg MAY contain space-separated flags
		
		if (!*arg || !isdigit(*arg)) {
			msg_to_char(ch, "Usage: storage add <building/room> [flags]\r\n");
		}
		else if (!building_proto(bld)) {
			msg_to_char(ch, "Invalid building vnum '%s'.\r\n", arg);
		}
		else {
			CREATE(store, struct obj_storage_type, 1);
			
			store->building_type = bld;
			
			store->flags = 0;
			store->next = NULL;
			
			// append to end
			if (obj->storage) {
				temp = obj->storage;
				while (temp->next) {
					temp = temp->next;
				}
				temp->next = store;
			}
			else {
				obj->storage = store;
			}
			
			// process flags as space-separated string
			while (*flagarg) {
				flagarg = any_one_arg(flagarg, arg);
				
				if ((loc = search_block(arg, storage_bits, FALSE)) != NOTHING) {
					store->flags |= BIT(loc);
				}
			}
			
			sprintbit(store->flags, storage_bits, buf1, TRUE);
			msg_to_char(ch, "You add storage in the %s with flags: %s\r\n", GET_BLD_NAME(building_proto(store->building_type)), buf1);
		}
	}
	else if (is_abbrev(arg1, "change")) {
		half_chop(arg2, num_arg, arg1);
		half_chop(arg1, type_arg, val_arg);
		
		if (!*num_arg || !isdigit(*num_arg) || !*type_arg || !*val_arg) {
			msg_to_char(ch, "Usage: storage change <number> <building | flags> <value>\r\n");
			return;
		}
		
		// find which one to change
		num = atoi(num_arg);
		change = NULL;
		for (store = obj->storage; store && !change; store = store->next) {
			if (--num == 0) {
				change = store;
				break;
			}
		}
		
		if (!change) {
			msg_to_char(ch, "Invalid storage number.\r\n");
		}
		else if (is_abbrev(type_arg, "building") || is_abbrev(type_arg, "room")) {
			bld = atoi(val_arg);
			if (!isdigit(*val_arg) || !building_proto(bld)) {
				msg_to_char(ch, "Invalid building vnum '%s'.\r\n", val_arg);
			}
			else {
				change->building_type = bld;
				msg_to_char(ch, "Storage %d changed to building %d (%s).\r\n", atoi(num_arg), bld, GET_BLD_NAME(building_proto(bld)));
			}
		}
		else if (is_abbrev(type_arg, "flags")) {
			change->flags = olc_process_flag(ch, val_arg, "storage", "storage change flags", storage_bits, change->flags);
		}
		else {
			msg_to_char(ch, "You can only change the building/room or flags.\r\n");
		}
	}
	else {
		msg_to_char(ch, "Usage: storage add <building/room> [flags]\r\n");
		msg_to_char(ch, "Usage: storage change <number> <building | flags> <value>\r\n");
		msg_to_char(ch, "Usage: storage remove <number | all>\r\n");
		msg_to_char(ch, "Available flags:\r\n");
		
		for (iter = 0; *storage_bits[iter] != '\n'; ++iter) {
			msg_to_char(ch, " %-24.24s%s", storage_bits[iter], ((iter % 2) ? "\r\n" : ""));
		}
		if ((iter % 2) != 0) {
			msg_to_char(ch, "\r\n");
		}
	}
}


OLC_MODULE(oedit_timer) {
	obj_data *obj = GET_OLC_OBJECT(ch->desc);
	GET_OBJ_TIMER(obj) = olc_process_number(ch, argument, "decay timer", "timer", -1, MAX_INT, GET_OBJ_TIMER(obj));
}


OLC_MODULE(oedit_type) {
	obj_data *obj = GET_OLC_OBJECT(ch->desc);
	int old = GET_OBJ_TYPE(obj);
	
	GET_OBJ_TYPE(obj) = olc_process_type(ch, argument, "type", "type", item_types, GET_OBJ_TYPE(obj));
	
	// reset values to defaults now
	if (old != GET_OBJ_TYPE(obj)) {
		GET_OBJ_VAL(obj, 0) = 0;
		GET_OBJ_VAL(obj, 1) = 0;
		GET_OBJ_VAL(obj, 2) = 0;
	
		switch (GET_OBJ_TYPE(obj)) {
			case ITEM_COINS: {
				GET_OBJ_VAL(obj, VAL_COINS_AMOUNT) = 0;
				GET_OBJ_VAL(obj, VAL_COINS_EMPIRE_ID) = OTHER_COIN;
				break;
			}
			case ITEM_CURRENCY: {
				GET_OBJ_VAL(obj, VAL_CURRENCY_VNUM) = NOTHING;
				break;
			}
			case ITEM_PORTAL: {
				GET_OBJ_VAL(obj, VAL_PORTAL_TARGET_VNUM) = NOTHING;
				break;
			}
			case ITEM_WEAPON: {
				// do not set a default, or it shows as 'changed' in the editor/auditor even if it was missed
				// GET_OBJ_VAL(obj, VAL_WEAPON_TYPE) = TYPE_SLASH;
				break;
			}
			case ITEM_POTION: {
				GET_OBJ_VAL(obj, VAL_POTION_COOLDOWN_TYPE) = NOTHING;
				GET_OBJ_VAL(obj, VAL_POTION_AFFECT) = NOTHING;
				break;
			}
			case ITEM_POISON: {
				GET_OBJ_VAL(obj, VAL_POISON_AFFECT) = NOTHING;
				break;
			}
			case ITEM_MINIPET: {
				GET_OBJ_VAL(obj, VAL_MINIPET_VNUM) = NOTHING;
				break;
			}
			default: {
				break;
			}
		}
	}
}


OLC_MODULE(oedit_uses) {
	obj_data *obj = GET_OLC_OBJECT(ch->desc);
	
	if (GET_OBJ_TYPE(obj) != ITEM_LIGHTER) {
		msg_to_char(ch, "You can only set uses on a LIGHTER object.\r\n");
	}
	else if (is_abbrev(argument, "unlimited")) {
		GET_OBJ_VAL(obj, VAL_LIGHTER_USES) = UNLIMITED;
		
		if (PRF_FLAGGED(ch, PRF_NOREPEAT)) {
			send_config_msg(ch, "ok_string");
		}
		else {
			msg_to_char(ch, "It now has unlimited uses.\r\n");
		}
	}
	else {
		GET_OBJ_VAL(obj, VAL_LIGHTER_USES) = olc_process_number(ch, argument, "lighter uses", "uses", 0, MAX_INT, GET_OBJ_VAL(obj, VAL_LIGHTER_USES));
	}
}


OLC_MODULE(oedit_value0) {
	obj_data *obj = GET_OLC_OBJECT(ch->desc);
	
	if (GET_OBJ_TYPE(obj) != ITEM_OTHER) {
		msg_to_char(ch, "You can only manually set values on OTHER-type objects.\r\n");
	}
	else {
		GET_OBJ_VAL(obj, 0) = olc_process_number(ch, argument, "value 0", "value0", -MAX_INT, MAX_INT, GET_OBJ_VAL(obj, 0));
	}
}


OLC_MODULE(oedit_value1) {
	obj_data *obj = GET_OLC_OBJECT(ch->desc);
	
	if (GET_OBJ_TYPE(obj) != ITEM_OTHER) {
		msg_to_char(ch, "You can only manually set values on OTHER-type objects.\r\n");
	}
	else {
		GET_OBJ_VAL(obj, 1) = olc_process_number(ch, argument, "value 1", "value1", -MAX_INT, MAX_INT, GET_OBJ_VAL(obj, 1));
	}
}


OLC_MODULE(oedit_value2) {
	obj_data *obj = GET_OLC_OBJECT(ch->desc);
	
	if (GET_OBJ_TYPE(obj) != ITEM_OTHER) {
		msg_to_char(ch, "You can only manually set values on OTHER-type objects.\r\n");
	}
	else {
		GET_OBJ_VAL(obj, 2) = olc_process_number(ch, argument, "value 2", "value2", -MAX_INT, MAX_INT, GET_OBJ_VAL(obj, 2));
	}
}


OLC_MODULE(oedit_wealth) {
	obj_data *obj = GET_OLC_OBJECT(ch->desc);
	
	if (GET_OBJ_TYPE(obj) != ITEM_WEALTH) {
		msg_to_char(ch, "You can only set the wealth value on a wealth item.\r\n");
	}
	else {
		GET_OBJ_VAL(obj, VAL_WEALTH_VALUE) = olc_process_number(ch, argument, "wealth value", "wealth", 0, 1000, GET_OBJ_VAL(obj, VAL_WEALTH_VALUE));
	}
}


OLC_MODULE(oedit_weapontype) {
	obj_data *obj = GET_OLC_OBJECT(ch->desc);
	int pos = 0;
	
	switch (GET_OBJ_TYPE(obj)) {
		case ITEM_WEAPON: {
			pos = VAL_WEAPON_TYPE;
			break;
		}
		case ITEM_MISSILE_WEAPON: {
			pos = VAL_MISSILE_WEAPON_TYPE;
			break;
		}
		default: {
			msg_to_char(ch, "You can only set weapontype on a weapon.\r\n");
			return;
		}
	}
	
	GET_OBJ_VAL(obj, pos) = olc_process_type(ch, argument, "weapon type", "weapontype", (const char**)get_weapon_types_string(), GET_OBJ_VAL(obj, pos));
}


OLC_MODULE(oedit_wear) {
	obj_data *obj = GET_OLC_OBJECT(ch->desc);
	GET_OBJ_WEAR(obj) = olc_process_flag(ch, argument, "wear", "wear", wear_bits, GET_OBJ_WEAR(obj));
}
