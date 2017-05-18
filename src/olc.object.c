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
extern const char *affected_bits[];
extern const char *apply_type_names[];
extern const char *apply_types[];
extern const char *armor_types[NUM_ARMOR_TYPES+1];
extern const char *component_flags[];
extern const char *component_types[];
extern const char *container_bits[];
extern const char *drinks[];
extern const char *extra_bits[];
extern const char *interact_types[];
extern const char *item_types[];
extern const struct material_data materials[NUM_MATERIALS];
extern const char *obj_custom_types[];
extern const char *offon_types[];
extern const struct poison_data_type poison_data[];
extern const struct potion_data_type potion_data[];
extern const char *storage_bits[];
extern const char *wear_bits[];

// external funcs
extern double get_base_dps(obj_data *weapon);
extern double get_weapon_speed(obj_data *weapon);

// locals
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
	extern adv_data *get_adventure_for_vnum(rmt_vnum vnum);
	
	bool is_adventure = (get_adventure_for_vnum(GET_OBJ_VNUM(obj)) != NULL);
	char temp[MAX_STRING_LENGTH], *ptr;
	bool problem = FALSE;
	
	if (!GET_OBJ_KEYWORDS(obj) || !*GET_OBJ_KEYWORDS(obj) || !str_cmp(GET_OBJ_KEYWORDS(obj), "object new")) {
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
	
	if (!str_cmp(GET_OBJ_LONG_DESC(obj), "A new object is sitting here.")) {
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
	if (!str_cmp(GET_OBJ_SHORT_DESC(obj), "a new object")) {
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
	if (OBJ_FLAGGED(obj, OBJ_SCALABLE) && obj->storage) {
		olc_audit_msg(ch, GET_OBJ_VNUM(obj), "Scalable object has storage locations");
		problem = TRUE;
	}
	if (OBJ_FLAGGED(obj, OBJ_SCALABLE) && !OBJ_FLAGGED(obj, OBJ_BIND_ON_EQUIP | OBJ_BIND_ON_PICKUP)) {
		olc_audit_msg(ch, GET_OBJ_VNUM(obj), "Scalable object has no bind flags");
		problem = TRUE;
	}
	
	switch (GET_OBJ_TYPE(obj)) {
		case ITEM_COINS: {
			if (GET_COINS_AMOUNT(obj) == 0) {
				olc_audit_msg(ch, GET_OBJ_VNUM(obj), "Coin amount not set");
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
		case ITEM_ARROW: {
			if (GET_ARROW_QUANTITY(obj) == 0) {
				olc_audit_msg(ch, GET_OBJ_VNUM(obj), "Arrow quantity not set");
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
		case ITEM_POISON: {
			if (GET_POISON_CHARGES(obj) == 0) {
				olc_audit_msg(ch, GET_OBJ_VNUM(obj), "Poison charges not set");
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
	void complete_building(room_data *room);
	extern bool delete_from_interaction_list(struct interaction_item **list, int vnum_type, any_vnum vnum);
	extern bool delete_from_spawn_template_list(struct adventure_spawn **list, int spawn_type, mob_vnum vnum);
	extern bool delete_link_rule_by_portal(struct adventure_link_rule **list, obj_vnum portal_vnum);
	extern bool delete_quest_giver_from_list(struct quest_giver **list, int type, any_vnum vnum);
	extern bool delete_quest_reward_from_list(struct quest_reward **list, int type, any_vnum vnum);
	extern bool delete_requirement_from_list(struct req_data **list, int type, any_vnum vnum);
	void expire_trading_post_item(struct trading_post_data *tpd);
	extern bool remove_obj_from_resource_list(struct resource_data **list, obj_vnum vnum);
	void remove_object_from_table(obj_data *obj);
	void save_trading_post();

	struct empire_trade_data *trade, *next_trade;
	struct trading_post_data *tpd, *next_tpd;
	struct archetype_gear *gear, *next_gear;
	obj_data *proto, *obj_iter, *next_obj;
	struct global_data *glb, *next_glb;
	archetype_data *arch, *next_arch;
	room_template *rmt, *next_rmt;
	sector_data *sect, *next_sect;
	craft_data *craft, *next_craft;
	morph_data *morph, *next_morph;
	quest_data *quest, *next_quest;
	augment_data *aug, *next_aug;
	vehicle_data *veh, *next_veh;
	crop_data *crop, *next_crop;
	room_data *room, *next_room;
	empire_data *emp, *next_emp;
	social_data *soc, *next_soc;
	char_data *mob, *next_mob;
	adv_data *adv, *next_adv;
	bld_data *bld, *next_bld;
	descriptor_data *desc;
	bool save, found, any_trades = FALSE;
	
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
			remove_obj_from_resource_list(&GET_BUILT_WITH(room), vnum);
		}
		if (GET_BUILDING_RESOURCES(room)) {
			remove_obj_from_resource_list(&GET_BUILDING_RESOURCES(room), vnum);
			
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
			remove_obj_from_resource_list(&VEH_NEEDS_RESOURCES(veh), vnum);
			
			if (!VEH_NEEDS_RESOURCES(veh)) {
				// removing the resource finished the vehicle
				if (VEH_FLAGGED(veh, VEH_INCOMPLETE)) {
					REMOVE_BIT(VEH_FLAGS(veh), VEH_INCOMPLETE);
					load_vtrigger(veh);
				}
			}
		}
	}
	
	// remove from empire inventories and trade -- DO THIS BEFORE REMOVING FROM OBJ TABLE
	HASH_ITER(hh, empire_table, emp, next_emp) {
		save = FALSE;
		
		if (delete_stored_resource(emp, vnum)) {
			save = TRUE;
		}
		
		if (delete_unique_storage_by_vnum(emp, vnum)) {
			save = TRUE;
		}
		
		for (trade = EMPIRE_TRADE(emp); trade; trade = next_trade) {
			next_trade = trade->next;
			if (trade->vnum == vnum) {
				struct empire_trade_data *temp;
				REMOVE_FROM_LIST(trade, EMPIRE_TRADE(emp), next);
				free(trade);	// certified
				save = TRUE;
			}
		}
		
		if (save) {
			save_empire(emp);
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
		
		found |= remove_obj_from_resource_list(&GET_AUG_RESOURCES(aug), vnum);
		
		if (found) {
			SET_BIT(GET_AUG_FLAGS(aug), AUG_IN_DEVELOPMENT);
			save_library_file_for_vnum(DB_BOOT_AUG, GET_AUG_VNUM(aug));
		}
	}
	
	// update buildings
	HASH_ITER(hh, building_table, bld, next_bld) {
		found = delete_from_interaction_list(&GET_BLD_INTERACTIONS(bld), TYPE_OBJ, vnum);
		found |= remove_obj_from_resource_list(&GET_BLD_YEARLY_MAINTENANCE(bld), vnum);
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
		
		found |= remove_obj_from_resource_list(&GET_CRAFT_RESOURCES(craft), vnum);
		
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
	
	// update quests
	HASH_ITER(hh, quest_table, quest, next_quest) {
		found = delete_quest_giver_from_list(&QUEST_STARTS_AT(quest), QG_OBJECT, vnum);
		found |= delete_quest_giver_from_list(&QUEST_ENDS_AT(quest), QG_OBJECT, vnum);
		found |= delete_quest_reward_from_list(&QUEST_REWARDS(quest), QR_OBJECT, vnum);
		found |= delete_requirement_from_list(&QUEST_TASKS(quest), REQ_GET_OBJECT, vnum);
		found |= delete_requirement_from_list(&QUEST_PREREQS(quest), REQ_GET_OBJECT, vnum);
		found |= delete_requirement_from_list(&QUEST_TASKS(quest), REQ_WEARING, vnum);
		found |= delete_requirement_from_list(&QUEST_PREREQS(quest), REQ_WEARING, vnum);
		found |= delete_requirement_from_list(&QUEST_TASKS(quest), REQ_WEARING_OR_HAS, vnum);
		found |= delete_requirement_from_list(&QUEST_PREREQS(quest), REQ_WEARING_OR_HAS, vnum);
		
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
	
	// update socials
	HASH_ITER(hh, social_table, soc, next_soc) {
		found = delete_requirement_from_list(&SOC_REQUIREMENTS(soc), REQ_GET_OBJECT, vnum);
		found |= delete_requirement_from_list(&SOC_REQUIREMENTS(soc), REQ_WEARING, vnum);
		found |= delete_requirement_from_list(&SOC_REQUIREMENTS(soc), REQ_WEARING_OR_HAS, vnum);
		
		if (found) {
			SET_BIT(SOC_FLAGS(soc), SOC_IN_DEVELOPMENT);
			save_library_file_for_vnum(DB_BOOT_SOC, SOC_VNUM(soc));
		}
	}
	
	// update vehicles
	HASH_ITER(hh, vehicle_table, veh, next_veh) {
		found = remove_obj_from_resource_list(&VEH_YEARLY_MAINTENANCE(veh), vnum);
		if (found) {
			save_library_file_for_vnum(DB_BOOT_VEH, VEH_VNUM(veh));
		}
	}
	
	// olc editor updates
	for (desc = descriptor_list; desc; desc = desc->next) {
		if (desc->character && !IS_NPC(desc->character) && GET_ACTION_RESOURCES(desc->character)) {
			remove_obj_from_resource_list(&GET_ACTION_RESOURCES(desc->character), vnum);
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
			
			found |= remove_obj_from_resource_list(&GET_AUG_RESOURCES(GET_OLC_AUGMENT(desc)), vnum);
			
			if (found) {
				SET_BIT(GET_AUG_FLAGS(GET_OLC_AUGMENT(desc)), AUG_IN_DEVELOPMENT);
				msg_to_char(desc->character, "One of the objects used in the augment you're editing was deleted.\r\n");
			}
	
		}
		if (GET_OLC_BUILDING(desc)) {
			found = delete_from_interaction_list(&GET_OLC_BUILDING(desc)->interactions, TYPE_OBJ, vnum);
			found |= remove_obj_from_resource_list(&GET_BLD_YEARLY_MAINTENANCE(GET_OLC_BUILDING(desc)), vnum);
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
			
			found |= remove_obj_from_resource_list(&GET_OLC_CRAFT(desc)->resources, vnum);
		
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
		if (GET_OLC_QUEST(desc)) {
			found = delete_quest_giver_from_list(&QUEST_STARTS_AT(GET_OLC_QUEST(desc)), QG_OBJECT, vnum);
			found |= delete_quest_giver_from_list(&QUEST_ENDS_AT(GET_OLC_QUEST(desc)), QG_OBJECT, vnum);
			found |= delete_quest_reward_from_list(&QUEST_REWARDS(GET_OLC_QUEST(desc)), QR_OBJECT, vnum);
			found |= delete_requirement_from_list(&QUEST_TASKS(GET_OLC_QUEST(desc)), REQ_GET_OBJECT, vnum);
			found |= delete_requirement_from_list(&QUEST_PREREQS(GET_OLC_QUEST(desc)), REQ_GET_OBJECT, vnum);
			found |= delete_requirement_from_list(&QUEST_TASKS(GET_OLC_QUEST(desc)), REQ_WEARING, vnum);
			found |= delete_requirement_from_list(&QUEST_PREREQS(GET_OLC_QUEST(desc)), REQ_WEARING, vnum);
			found |= delete_requirement_from_list(&QUEST_TASKS(GET_OLC_QUEST(desc)), REQ_WEARING_OR_HAS, vnum);
			found |= delete_requirement_from_list(&QUEST_PREREQS(GET_OLC_QUEST(desc)), REQ_WEARING_OR_HAS, vnum);
		
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
		if (GET_OLC_SOCIAL(desc)) {
			found = delete_requirement_from_list(&SOC_REQUIREMENTS(GET_OLC_SOCIAL(desc)), REQ_GET_OBJECT, vnum);
			found |= delete_requirement_from_list(&SOC_REQUIREMENTS(GET_OLC_SOCIAL(desc)), REQ_WEARING, vnum);
			found |= delete_requirement_from_list(&SOC_REQUIREMENTS(GET_OLC_SOCIAL(desc)), REQ_WEARING_OR_HAS, vnum);
		
			if (found) {
				SET_BIT(SOC_FLAGS(GET_OLC_SOCIAL(desc)), SOC_IN_DEVELOPMENT);
				msg_to_desc(desc, "An object required by the social you are editing was deleted.\r\n");
			}
		}
		if (GET_OLC_VEHICLE(desc)) {
			found = remove_obj_from_resource_list(&VEH_YEARLY_MAINTENANCE(GET_OLC_VEHICLE(desc)), vnum);
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
	char buf[MAX_STRING_LENGTH], line[MAX_STRING_LENGTH], type_arg[MAX_INPUT_LENGTH], val_arg[MAX_INPUT_LENGTH], find_keywords[MAX_INPUT_LENGTH];
	bitvector_t find_applies = NOBITS, found_applies, not_flagged = NOBITS, only_flags = NOBITS;
	bitvector_t only_worn = NOBITS, cmp_flags = NOBITS, not_cmp_flagged = NOBITS, only_affs = NOBITS;
	bitvector_t  find_interacts = NOBITS, found_interacts, find_custom = NOBITS, found_custom;
	int count, lookup, only_level = NOTHING, only_type = NOTHING, only_mat = NOTHING, only_cmp = NOTHING;
	struct interaction_item *inter;
	struct custom_message *cust;
	obj_data *obj, *next_obj;
	struct obj_apply *app;
	size_t size;
	
	if (!*argument) {
		msg_to_char(ch, "See HELP OEDIT FULLSEARCH for syntax.\r\n");
		return;
	}
	
	// process argument
	*find_keywords = '\0';
	while (*argument) {
		// figure out a type
		argument = any_one_arg(argument, type_arg);
		
		if (is_abbrev(type_arg, "-affects")) {
			argument = any_one_word(argument, val_arg);
			if ((lookup = search_block(val_arg, affected_bits, FALSE)) != NOTHING) {
				only_affs |= BIT(lookup);
			}
			else {
				msg_to_char(ch, "Invalid affect flag '%s'.\r\n", val_arg);
				return;
			}
		}
		else if (is_abbrev(type_arg, "-apply") || is_abbrev(type_arg, "-applies")) {
			argument = any_one_word(argument, val_arg);
			if ((lookup = search_block(val_arg, apply_types, FALSE)) != NOTHING) {
				find_applies |= BIT(lookup);
			}
			else {
				msg_to_char(ch, "Invalid apply type '%s'.\r\n", val_arg);
				return;
			}
		}
		else if (is_abbrev(type_arg, "-cflags") || is_abbrev(type_arg, "-cflagged")) {
			argument = any_one_word(argument, val_arg);
			if ((lookup = search_block(val_arg, component_flags, FALSE)) != NOTHING) {
				cmp_flags |= BIT(lookup);
			}
			else {
				msg_to_char(ch, "Invalid component flag '%s'.\r\n", val_arg);
				return;
			}
		}
		else if (is_abbrev(type_arg, "-component")) {
			argument = any_one_word(argument, val_arg);
			if ((only_cmp = search_block(val_arg, component_types, FALSE)) == NOTHING) {
				msg_to_char(ch, "Invalid component '%s'.\r\n", val_arg);
				return;
			}
		}
		else if (is_abbrev(type_arg, "-custom")) {
			argument = any_one_word(argument, val_arg);
			if ((lookup = search_block(val_arg, obj_custom_types, FALSE)) != NOTHING) {
				find_custom |= BIT(lookup);
			}
			else {
				msg_to_char(ch, "Invalid custom message type '%s'.\r\n", val_arg);
				return;
			}
		}
		else if (is_abbrev(type_arg, "-flags") || is_abbrev(type_arg, "-flagged")) {
			argument = any_one_word(argument, val_arg);
			if ((lookup = search_block(val_arg, extra_bits, FALSE)) != NOTHING) {
				only_flags |= BIT(lookup);
			}
			else {
				msg_to_char(ch, "Invalid flag '%s'.\r\n", val_arg);
				return;
			}
		}
		else if (is_abbrev(type_arg, "-interaction")) {
			argument = any_one_word(argument, val_arg);
			if ((lookup = search_block(val_arg, interact_types, FALSE)) != NOTHING) {
				find_interacts |= BIT(lookup);
			}
			else {
				msg_to_char(ch, "Invalid interaction type '%s'.\r\n", val_arg);
				return;
			}
		}
		else if (is_abbrev(type_arg, "-level")) {
			argument = any_one_word(argument, val_arg);
			if (!isdigit(*val_arg) || (only_level = atoi(val_arg)) < 0) {
				msg_to_char(ch, "Invalid level '%s'.\r\n", val_arg);
				return;
			}
		}
		else if (is_abbrev(type_arg, "-material")) {
			check_oedit_material_list();
			argument = any_one_word(argument, val_arg);
			if ((only_mat = search_block(val_arg, (const char **)olc_material_list, FALSE)) == NOTHING) {
				msg_to_char(ch, "Invalid material '%s'.\r\n", val_arg);
				return;
			}
		}
		else if (is_abbrev(type_arg, "-type")) {
			argument = any_one_word(argument, val_arg);
			if ((only_type = search_block(val_arg, item_types, FALSE)) == NOTHING) {
				msg_to_char(ch, "Invalid type '%s'.\r\n", val_arg);
				return;
			}
		}
		else if (is_abbrev(type_arg, "-uncflagged")) {
			argument = any_one_word(argument, val_arg);
			if ((lookup = search_block(val_arg, component_flags, FALSE)) != NOTHING) {
				not_cmp_flagged |= BIT(lookup);
			}
			else {
				msg_to_char(ch, "Invalid component flag '%s'.\r\n", val_arg);
				return;
			}
		}
		else if (is_abbrev(type_arg, "-unflagged")) {
			argument = any_one_word(argument, val_arg);
			if ((lookup = search_block(val_arg, extra_bits, FALSE)) != NOTHING) {
				not_flagged |= BIT(lookup);
			}
			else {
				msg_to_char(ch, "Invalid flag '%s'.\r\n", val_arg);
				return;
			}
		}
		else if (is_abbrev(type_arg, "-wear") || is_abbrev(type_arg, "-worn")) {
			argument = any_one_word(argument, val_arg);
			if ((lookup = search_block(val_arg, wear_bits, FALSE)) != NOTHING) {
				only_worn |= BIT(lookup);
			}
			else {
				msg_to_char(ch, "Invalid wear location '%s'.\r\n", val_arg);
				return;
			}
		}
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
		if (*find_keywords && !multi_isname(find_keywords, GET_OBJ_KEYWORDS(obj)) && !multi_isname(find_keywords, GET_OBJ_SHORT_DESC(obj)) && !multi_isname(find_keywords, GET_OBJ_LONG_DESC(obj)) && !multi_isname(find_keywords, NULLSAFE(GET_OBJ_ACTION_DESC(obj)))) {
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
	extern const byte interact_vnum_types[NUM_INTERACTS];
	
	char buf[MAX_STRING_LENGTH];
	struct adventure_spawn *asp;
	struct interaction_item *inter;
	struct adventure_link_rule *link;
	struct global_data *glb, *next_glb;
	archetype_data *arch, *next_arch;
	craft_data *craft, *next_craft;
	morph_data *morph, *next_morph;
	quest_data *quest, *next_quest;
	room_template *rmt, *next_rmt;
	sector_data *sect, *next_sect;
	augment_data *aug, *next_aug;
	vehicle_data *veh, *next_veh;
	social_data *soc, *next_soc;
	struct archetype_gear *gear;
	crop_data *crop, *next_crop;
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
			if (res->vnum == vnum) {
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
			if (res->vnum == vnum) {
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
	
	// quests
	HASH_ITER(hh, quest_table, quest, next_quest) {
		if (size >= sizeof(buf)) {
			break;
		}
		any = find_quest_giver_in_list(QUEST_STARTS_AT(quest), QG_OBJECT, vnum);
		any |= find_quest_giver_in_list(QUEST_ENDS_AT(quest), QG_OBJECT, vnum);
		any |= find_quest_reward_in_list(QUEST_REWARDS(quest), QR_OBJECT, vnum);
		any |= find_requirement_in_list(QUEST_TASKS(quest), REQ_GET_OBJECT, vnum);
		any |= find_requirement_in_list(QUEST_PREREQS(quest), REQ_GET_OBJECT, vnum);
		any |= find_requirement_in_list(QUEST_TASKS(quest), REQ_WEARING, vnum);
		any |= find_requirement_in_list(QUEST_PREREQS(quest), REQ_WEARING, vnum);
		any |= find_requirement_in_list(QUEST_TASKS(quest), REQ_WEARING_OR_HAS, vnum);
		any |= find_requirement_in_list(QUEST_PREREQS(quest), REQ_WEARING_OR_HAS, vnum);
		
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
	
	// socials
	HASH_ITER(hh, social_table, soc, next_soc) {
		if (size >= sizeof(buf)) {
			break;
		}
		any = find_requirement_in_list(SOC_REQUIREMENTS(soc), REQ_GET_OBJECT, vnum);
		any |= find_requirement_in_list(SOC_REQUIREMENTS(soc), REQ_WEARING, vnum);
		any |= find_requirement_in_list(SOC_REQUIREMENTS(soc), REQ_WEARING_OR_HAS, vnum);
		
		if (any) {
			++found;
			size += snprintf(buf + size, sizeof(buf) - size, "SOC [%5d] %s\r\n", SOC_VNUM(soc), SOC_NAME(soc));
		}
	}
	
	// vehicles
	HASH_ITER(hh, vehicle_table, veh, next_veh) {
		any = FALSE;
		for (res = VEH_YEARLY_MAINTENANCE(veh); res && !any; res = res->next) {
			if (res->vnum == vnum) {
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
		extract_script(to_update, OBJ_TRIGGER);
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
	struct interaction_item *interact;
	struct obj_storage_type *store;
	struct trading_post_data *tpd;
	empire_data *emp, *next_emp;
	struct quest_lookup *ql;
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
	while ((interact = proto->interactions)) {
		proto->interactions = interact->next;
		free(interact);
	}
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
	
	*proto = *obj;
	proto->vnum = vnum;	// ensure correct vnum
	
	proto->hh = hh;	// restore hash handle
	proto->quest_lookups = ql;	// restore lookups
	
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
		GET_OBJ_KEYWORDS(new) = str_dup("object new");
		GET_OBJ_SHORT_DESC(new) = str_dup("a new object");
		GET_OBJ_LONG_DESC(new) = str_dup("A new object is sitting here.");
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
	
	switch (GET_OBJ_TYPE(obj)) {
		case ITEM_COINS: {
			sprintf(storage + strlen(storage), "<&ycoinamount&0> %d\r\n", GET_COINS_AMOUNT(obj));
			// empire number is not supported -- it will always use OTHER_COIN
			break;
		}
		case ITEM_CORPSE: {
			sprintf(storage + strlen(storage), "<&ycorpseof&0> %d %s\r\n", GET_CORPSE_NPC_VNUM(obj), get_mob_name_by_proto(GET_CORPSE_NPC_VNUM(obj)));
			break;
		}
		case ITEM_WEAPON: {
			sprintf(storage + strlen(storage), "<&yweapontype&0> %s\r\n", attack_hit_info[GET_WEAPON_TYPE(obj)].name);
			sprintf(storage + strlen(storage), "<&ydamage&0> %d (speed %.2f, %s+%.2f base dps)\r\n", GET_WEAPON_DAMAGE_BONUS(obj), get_weapon_speed(obj), (IS_MAGIC_ATTACK(GET_WEAPON_TYPE(obj)) ? "Intelligence" : "Strength"), get_base_dps(obj));
			break;
		}
		case ITEM_CONTAINER: {
			sprintf(storage + strlen(storage), "<&ycapacity&0> %d object%s\r\n", GET_MAX_CONTAINER_CONTENTS(obj), PLURAL(GET_MAX_CONTAINER_CONTENTS(obj)));
			
			sprintbit(GET_CONTAINER_FLAGS(obj), container_bits, temp, TRUE);
			sprintf(storage + strlen(storage), "<&ycontainerflags&0> %s\r\n", temp);
			break;
		}
		case ITEM_DRINKCON: {
			sprintf(storage + strlen(storage), "<&ycapacity&0> %d drink%s\r\n", GET_DRINK_CONTAINER_CAPACITY(obj), PLURAL(GET_DRINK_CONTAINER_CAPACITY(obj)));
			sprintf(storage + strlen(storage), "<&ycontents&0> %d drink%s\r\n", GET_DRINK_CONTAINER_CONTENTS(obj), PLURAL(GET_DRINK_CONTAINER_CONTENTS(obj)));
			sprintf(storage + strlen(storage), "<&yliquid&0> %s\r\n", drinks[GET_DRINK_CONTAINER_TYPE(obj)]);
			break;
		}
		case ITEM_FOOD: {
			sprintf(storage + strlen(storage), "<&yfullness&0> %d hour%s\r\n", GET_FOOD_HOURS_OF_FULLNESS(obj), (GET_FOOD_HOURS_OF_FULLNESS(obj) != 1 ? "s" : ""));
			break;
		}
		case ITEM_PORTAL: {
			sprintf(storage + strlen(storage), "<&yroomvnum&0> %d\r\n", GET_PORTAL_TARGET_VNUM(obj));
			break;
		}
		case ITEM_MISSILE_WEAPON: {
			sprintf(storage + strlen(storage), "<&ymissilespeed&0> %.2f\r\n", missile_weapon_speed[GET_MISSILE_WEAPON_SPEED(obj)]);
			sprintf(storage + strlen(storage), "<&ydamage&0> %d (speed %.2f, %.2f base dps)\r\n", GET_MISSILE_WEAPON_DAMAGE(obj), get_weapon_speed(obj), get_base_dps(obj));
			sprintf(storage + strlen(storage), "<&yarrowtype&0> %c\r\n", 'A' + GET_MISSILE_WEAPON_TYPE(obj));
			break;
		}
		case ITEM_ARROW: {
			sprintf(storage + strlen(storage), "<&yquantity&0> %d\r\n", GET_ARROW_QUANTITY(obj));
			sprintf(storage + strlen(storage), "<&ydamage&0> %+d\r\n", GET_ARROW_DAMAGE_BONUS(obj));
			sprintf(storage + strlen(storage), "<&yarrowtype&0> %c\r\n", 'A' + GET_ARROW_TYPE(obj));
			break;
		}
		case ITEM_PACK: {
			sprintf(storage + strlen(storage), "<&ycapacity&0> %d object%s\r\n", GET_PACK_CAPACITY(obj), PLURAL(GET_PACK_CAPACITY(obj)));
			break;
		}
		case ITEM_POTION: {
			sprintf(storage + strlen(storage), "<&ypotion&0> %s\r\n", potion_data[GET_POTION_TYPE(obj)].name);
			sprintf(storage + strlen(storage), "<&ypotionscale&0> %d\r\n", GET_POTION_SCALE(obj));
			break;
		}
		case ITEM_POISON: {
			sprintf(storage + strlen(storage), "<&ypoison&0> %s\r\n", poison_data[GET_POISON_TYPE(obj)].name);
			sprintf(storage + strlen(storage), "<&ycharges&0> %d\r\n", GET_POISON_CHARGES(obj));
			break;
		}
		case ITEM_ARMOR: {
			sprintf(storage + strlen(storage), "<&yarmortype&0> %s\r\n", armor_types[GET_ARMOR_TYPE(obj)]);
			break;
		}
		case ITEM_BOOK: {
			book = book_proto(GET_BOOK_ID(obj));
			sprintf(storage + strlen(storage), "<&ytext&0> [%d] %s\r\n", GET_BOOK_ID(obj), (book ? book->title : "not set"));
			break;
		}
		case ITEM_WEALTH: {
			sprintf(storage + strlen(storage), "<&ywealth&0> %d\r\n", GET_WEALTH_VALUE(obj));
			sprintf(storage + strlen(storage), "<&yautomint&0> %s\r\n", offon_types[GET_WEALTH_AUTOMINT(obj)]);
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
				sprintf(storage + strlen(storage), "<&yvalue0&0> %d, <&yvalue2&0> %d\r\n", GET_OBJ_VAL(obj, 0), GET_OBJ_VAL(obj, 2));
			}
			else {
				sprintf(storage + strlen(storage), "<&yvalue0&0> %d, <&yvalue1&0> %d, <&yvalue2&0> %d\r\n", GET_OBJ_VAL(obj, 0), GET_OBJ_VAL(obj, 1), GET_OBJ_VAL(obj, 2));
			}
			break;
		}
	}
	
	// this is added for all of them
	if (OBJ_FLAGGED(obj, OBJ_PLANTABLE) && GET_OBJ_VAL(obj, VAL_FOOD_CROP_TYPE) >= 0) {
		sprintf(storage + strlen(storage), "<&yplants&0> [%d] %s\r\n", GET_OBJ_VAL(obj, VAL_FOOD_CROP_TYPE), GET_CROP_NAME(crop_proto(GET_OBJ_VAL(obj, VAL_FOOD_CROP_TYPE))));
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
	sprintf(buf + strlen(buf), "[&c%d&0] &c%s&0 (Gear rating [&c%.2f&0])\r\n", GET_OLC_VNUM(ch->desc), !obj_proto(GET_OLC_VNUM(ch->desc)) ? "new object" : get_obj_name_by_proto(GET_OLC_VNUM(ch->desc)), rate_item(obj));
	sprintf(buf + strlen(buf), "<&ykeywords&0> %s\r\n", GET_OBJ_KEYWORDS(obj));
	sprintf(buf + strlen(buf), "<&yshortdescription&0> %s\r\n", GET_OBJ_SHORT_DESC(obj));
	sprintf(buf + strlen(buf), "<&ylongdescription&0> %s\r\n", GET_OBJ_LONG_DESC(obj));
	sprintf(buf + strlen(buf), "<&ylookdescription&0>\r\n%s", NULLSAFE(GET_OBJ_ACTION_DESC(obj)));
	
	if (GET_OBJ_TIMER(obj) > 0) {
		minutes = GET_OBJ_TIMER(obj) * SECS_PER_MUD_HOUR / SECS_PER_REAL_MIN;
		sprintf(buf1, "%d ticks (%d:%02d)", GET_OBJ_TIMER(obj), minutes / 60, minutes % 60);
	}
	else {
		strcpy(buf1, "none");
	}
	sprintf(buf + strlen(buf), "<&ytype&0> %s, <&ymaterial&0> %s, <&ytimer&0> %s\r\n", item_types[(int) GET_OBJ_TYPE(obj)], materials[GET_OBJ_MATERIAL(obj)].name, buf1);
	
	sprintbit(GET_OBJ_WEAR(obj), wear_bits, buf1, TRUE);
	sprintf(buf + strlen(buf), "<&ywear&0> %s\r\n", buf1);
	
	sprintbit(GET_OBJ_EXTRA(obj), extra_bits, buf1, TRUE);
	sprintf(buf + strlen(buf), "<&yflags&0> %s\r\n", buf1);
	
	sprintf(buf + strlen(buf), "<&ycomponent&0> %s\r\n", component_types[GET_OBJ_CMP_TYPE(obj)]);
	if (GET_OBJ_CMP_TYPE(obj) != CMP_NONE) {
		prettier_sprintbit(GET_OBJ_CMP_FLAGS(obj), component_flags, buf1);
		sprintf(buf + strlen(buf), "<&ycompflags&0> %s\r\n", buf1);
	}
	
	if (GET_OBJ_MIN_SCALE_LEVEL(obj) > 0) {
		sprintf(buf + strlen(buf), "<&yminlevel&0> %d\r\n", GET_OBJ_MIN_SCALE_LEVEL(obj));
	}
	else {
		sprintf(buf + strlen(buf), "<&yminlevel&0> none\r\n");
	}
	if (GET_OBJ_MAX_SCALE_LEVEL(obj) > 0) {
		sprintf(buf + strlen(buf), "<&ymaxlevel&0> %d\r\n", GET_OBJ_MAX_SCALE_LEVEL(obj));
	}
	else {
		sprintf(buf + strlen(buf), "<&ymaxlevel&0> none\r\n");
	}
	
	if (GET_OBJ_REQUIRES_QUEST(obj) != NOTHING) {
		sprintf(buf + strlen(buf), "<&yrequiresquest&0> [%d] %s\r\n", GET_OBJ_REQUIRES_QUEST(obj), get_quest_name_by_proto(GET_OBJ_REQUIRES_QUEST(obj)));
	}
	else {
		sprintf(buf + strlen(buf), "<&yrequiresquest&0> none\r\n");
	}
	
	olc_get_values_display(ch, buf1);
	strcat(buf, buf1);

	sprintbit(GET_OBJ_AFF_FLAGS(obj), affected_bits, buf1, TRUE);
	sprintf(buf + strlen(buf), "<&yaffects&0> %s\r\n", buf1);
	
	// applies / affected[]
	count = 0;
	sprintf(buf + strlen(buf), "Attribute Applies: <&yapply&0>\r\n");
	for (apply = GET_OBJ_APPLIES(obj); apply; apply = apply->next) {
		sprintf(buf + strlen(buf), " &y%2d&0. %+d to %s (%s)\r\n", ++count, apply->modifier, apply_types[(int) apply->location], apply_type_names[(int)apply->apply_type]);
	}
	
	// exdesc
	sprintf(buf + strlen(buf), "Extra descriptions: <&yextra&0>\r\n");
	if (obj->ex_description) {
		get_extra_desc_display(obj->ex_description, buf1);
		strcat(buf, buf1);
	}

	sprintf(buf + strlen(buf), "Interactions: <&yinteraction&0>\r\n");
	if (obj->interactions) {
		get_interaction_display(obj->interactions, buf1);
		strcat(buf, buf1);
	}
	
	// storage
	sprintf(buf + strlen(buf), "Storage: <&ystorage&0>\r\n");
	count = 0;
	for (store = obj->storage; store; store = store->next) {
		bld = building_proto(store->building_type);
		
		sprintbit(store->flags, storage_bits, buf2, TRUE);
		sprintf(buf + strlen(buf), " &y%d&0. [%d] %s ( %s)\r\n", ++count, GET_BLD_VNUM(bld), GET_BLD_NAME(bld), buf2);
	}
	
	// custom messages
	sprintf(buf + strlen(buf), "Custom messages: <&ycustom&0>\r\n");
	count = 0;
	for (ocm = obj->custom_msgs; ocm; ocm = ocm->next) {
		sprintf(buf + strlen(buf), " &y%d&0. [%s] %s\r\n", ++count, obj_custom_types[ocm->type], ocm->msg);
	}
	
	// scripts
	sprintf(buf + strlen(buf), "Scripts: <&yscript&0>\r\n");
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
		else if (is_abbrev(type_arg, "value")) {
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


OLC_MODULE(oedit_arrowtype) {
	obj_data *obj = GET_OLC_OBJECT(ch->desc);
	int slot = NOTHING;
	char val;
	
	// this chooses the VAL slot and validates the type
	switch (GET_OBJ_TYPE(obj)) {
		case ITEM_MISSILE_WEAPON: {
			slot = VAL_MISSILE_WEAPON_TYPE;
			break;
		}
		case ITEM_ARROW: {
			slot = VAL_ARROW_TYPE;
			break;
		}
	}
	
	if (slot == NOTHING) {
		msg_to_char(ch, "You can only set arrowtype for missile weapons and arrows.\r\n");
	}
	else if (strlen(argument) > 1 || (val = UPPER(*argument)) < 'A' || val > 'Z') {
		msg_to_char(ch, "Arrowtype must by a letter from A to Z.\r\n");
	}
	else {
		GET_OBJ_VAL(obj, slot) = val - 'A';
		msg_to_char(ch, "You set the arrowtype to %c.\r\n", val);
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
		msg_to_char(ch, "You can only set etxt id on a book.\r\n");
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
	
	if (!IS_COINS(obj)) {
		msg_to_char(ch, "You can only set this on coins.\r\n");
	}
	else {
		GET_OBJ_VAL(obj, VAL_COINS_AMOUNT) = olc_process_number(ch, argument, "coin amount", "coinamount", 0, MAX_COIN, GET_OBJ_VAL(obj, VAL_COINS_AMOUNT));
	}
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
	}
}


OLC_MODULE(oedit_custom) {
	obj_data *obj = GET_OLC_OBJECT(ch->desc);
	char arg1[MAX_INPUT_LENGTH], arg2[MAX_INPUT_LENGTH], *msgstr;
	char num_arg[MAX_INPUT_LENGTH], type_arg[MAX_INPUT_LENGTH], val_arg[MAX_INPUT_LENGTH];
	int num, iter, msgtype;
	struct custom_message *ocm, *change, *temp;
	bool found;
	
	// arg1 arg2
	half_chop(argument, arg1, arg2);
	
	if (is_abbrev(arg1, "remove")) {
		if (!*arg2) {
			msg_to_char(ch, "Remove which custom message (number)?\r\n");
		}
		else if (!str_cmp(arg2, "all")) {
			while ((ocm = obj->custom_msgs)) {
				obj->custom_msgs = ocm->next;
				if (ocm->msg) {
					free(ocm->msg);
				}
				free(ocm);
			}
			msg_to_char(ch, "You remove all the custom messages.\r\n");
		}
		else if (!isdigit(*arg2) || (num = atoi(arg2)) < 1) {
			msg_to_char(ch, "Invalid custom message number.\r\n");
		}
		else {
			found = FALSE;
			for (ocm = obj->custom_msgs; ocm && !found; ocm = ocm->next) {
				if (--num == 0) {
					found = TRUE;
					
					msg_to_char(ch, "You remove custom message #%d.\r\n", atoi(arg2));
					
					REMOVE_FROM_LIST(ocm, obj->custom_msgs, next);
					if (ocm->msg) {
						free(ocm->msg);
					}
					free(ocm);
				}
			}
			
			if (!found) {
				msg_to_char(ch, "Invalid custom message number.\r\n");
			}
		}
	}
	else if (is_abbrev(arg1, "add")) {
		msgstr = any_one_word(arg2, arg);
		skip_spaces(&msgstr);
		
		if (!*arg || !*msgstr) {
			msg_to_char(ch, "Usage: custom add <type> <string>\r\n");
		}
		else if ((msgtype = search_block(arg, obj_custom_types, FALSE)) == NOTHING) {
			msg_to_char(ch, "Invalid type '%s'.\r\n", arg);
		}
		else {
			delete_doubledollar(msgstr);
			
			CREATE(ocm, struct custom_message, 1);

			ocm->type = msgtype;
			ocm->msg = str_dup(msgstr);
			ocm->next = NULL;
			
			// append to end
			if (obj->custom_msgs) {
				temp = obj->custom_msgs;
				while (temp->next) {
					temp = temp->next;
				}
				temp->next = ocm;
			}
			else {
				obj->custom_msgs = ocm;
			}
			
			msg_to_char(ch, "You add a custom '%s' message:\r\n%s\r\n", obj_custom_types[ocm->type], ocm->msg);			
		}
	}
	else if (is_abbrev(arg1, "change")) {
		half_chop(arg2, num_arg, arg1);
		half_chop(arg1, type_arg, val_arg);
		
		if (!*num_arg || !isdigit(*num_arg) || !*type_arg || !*val_arg) {
			msg_to_char(ch, "Usage: custom change <number> <type | message> <value>\r\n");
			return;
		}
		
		// find which one to change
		num = atoi(num_arg);
		change = NULL;
		for (ocm = obj->custom_msgs; ocm && !change; ocm = ocm->next) {
			if (--num == 0) {
				change = ocm;
				break;
			}
		}
		
		if (!change) {
			msg_to_char(ch, "Invalid custom message number.\r\n");
		}
		else if (is_abbrev(type_arg, "type")) {
			if ((msgtype = search_block(val_arg, obj_custom_types, FALSE)) == NOTHING) {
				msg_to_char(ch, "Invalid type '%s'.\r\n", val_arg);
			}
			else {
				change->type = msgtype;
				msg_to_char(ch, "Custom message %d changed to type %s.\r\n", atoi(num_arg), obj_custom_types[msgtype]);
			}
		}
		else if (is_abbrev(type_arg, "message")) {
			if (change->msg) {
				free(change->msg);
			}
			delete_doubledollar(val_arg);
			change->msg = str_dup(val_arg);
			msg_to_char(ch, "Custom message %d changed to: %s\r\n", atoi(num_arg), val_arg);
		}
		else {
			msg_to_char(ch, "You can only change the type or message.\r\n");
		}
	}
	else {
		msg_to_char(ch, "Usage: custom add <type> <message>\r\n");
		msg_to_char(ch, "Usage: custom change <number> <type | message> <value>\r\n");
		msg_to_char(ch, "Usage: custom remove <number | all>\r\n");
		msg_to_char(ch, "Available types:\r\n");
		for (iter = 0; *obj_custom_types[iter] != '\n'; ++iter) {
			msg_to_char(ch, " %s\r\n", obj_custom_types[iter]);
		}
	}
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
		case ITEM_ARROW: {
			slot = VAL_ARROW_DAMAGE_BONUS;
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
	
	if (!IS_DRINK_CONTAINER(obj)) {
		msg_to_char(ch, "You can only set liquid on a drink container.\r\n");
	}
	else {
		GET_OBJ_VAL(obj, VAL_DRINK_CONTAINER_TYPE) = olc_process_type(ch, argument, "liquid", "liquid", drinks, GET_OBJ_VAL(obj, VAL_DRINK_CONTAINER_TYPE));
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


OLC_MODULE(oedit_minlevel) {
	obj_data *obj = GET_OLC_OBJECT(ch->desc);
	
	GET_OBJ_MIN_SCALE_LEVEL(obj) = olc_process_number(ch, argument, "minimum level", "minlevel", 0, MAX_INT, GET_OBJ_MIN_SCALE_LEVEL(obj));
}


OLC_MODULE(oedit_missilespeed) {
	obj_data *obj = GET_OLC_OBJECT(ch->desc);
	static char **speed_types = NULL;
	int iter, count;
	
	// this does not have a const char** list .. build one the first time
	if (!speed_types) {
		// count types
		count = 0;
		for (iter = 0; missile_weapon_speed[iter] > 0; ++iter) {
			++count;
		}
	
		CREATE(speed_types, char*, count+1);
		
		for (iter = 0; iter < count; ++iter) {
			sprintf(buf, "%.2f", missile_weapon_speed[iter]);
			speed_types[iter] = str_dup(buf);
		}
		
		// must terminate
		speed_types[count] = str_dup("\n");
	}

	if (!IS_MISSILE_WEAPON(obj)) {
		msg_to_char(ch, "You can only set missilespeed type on a missile weapon.\r\n");
	}
	else {
		GET_OBJ_VAL(obj, VAL_MISSILE_WEAPON_SPEED) = olc_process_type(ch, argument, "missile speed", "missilespeed", (const char**)speed_types, GET_OBJ_VAL(obj, VAL_MISSILE_WEAPON_SPEED));
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


OLC_MODULE(oedit_poison) {
	obj_data *obj = GET_OLC_OBJECT(ch->desc);
	static char **poison_types = NULL;
	int iter, count;
	
	// this does not have a const char** list .. build one the first time
	if (!poison_types) {
		// count types
		count = 0;
		for (iter = 0; *poison_data[iter].name != '\n'; ++iter) {
			++count;
		}
	
		CREATE(poison_types, char*, count+1);
		
		for (iter = 0; iter < count; ++iter) {
			poison_types[iter] = str_dup(poison_data[iter].name);
		}
		
		// must terminate
		poison_types[count] = str_dup("\n");
	}
	
	if (!IS_POISON(obj)) {
		msg_to_char(ch, "You can only set poison type on a poison object.\r\n");
	}
	else {
		GET_OBJ_VAL(obj, VAL_POISON_TYPE) = olc_process_type(ch, argument, "poison", "poison", (const char**)poison_types, GET_OBJ_VAL(obj, VAL_POISON_TYPE));
	}
}


OLC_MODULE(oedit_potion) {
	obj_data *obj = GET_OLC_OBJECT(ch->desc);
	static char **potion_types = NULL;
	int iter, count;
	
	// this does not have a const char** list .. build one the first time
	if (!potion_types) {
		// count types
		count = 0;
		for (iter = 0; *potion_data[iter].name != '\n'; ++iter) {
			++count;
		}
	
		CREATE(potion_types, char*, count+1);
		
		for (iter = 0; iter < count; ++iter) {
			potion_types[iter] = str_dup(potion_data[iter].name);
		}
		
		// must terminate
		potion_types[count] = str_dup("\n");
	}

	if (!IS_POTION(obj)) {
		msg_to_char(ch, "You can only set potion type on a potion object.\r\n");
	}
	else {
		GET_OBJ_VAL(obj, VAL_POTION_TYPE) = olc_process_type(ch, argument, "potion", "potion", (const char**)potion_types, GET_OBJ_VAL(obj, VAL_POTION_TYPE));
	}
}


OLC_MODULE(oedit_potionscale) {
	obj_data *obj = GET_OLC_OBJECT(ch->desc);
	
	if (!IS_POTION(obj)) {
		msg_to_char(ch, "You can't set potionscale on this type of item.\r\n");
	}
	else {
		GET_OBJ_VAL(obj, VAL_POTION_SCALE) = olc_process_number(ch, argument, "potion scale", "potionscale", 0, 1000, GET_OBJ_VAL(obj, VAL_POTION_SCALE));
	}
}


OLC_MODULE(oedit_quantity) {
	obj_data *obj = GET_OLC_OBJECT(ch->desc);
	
	if (!IS_ARROW(obj)) {
		msg_to_char(ch, "You can't set quantity on this type of item.\r\n");
	}
	else {
		GET_OBJ_VAL(obj, VAL_ARROW_QUANTITY) = olc_process_number(ch, argument, "quantity", "quantity", 0, 100, GET_OBJ_VAL(obj, VAL_ARROW_QUANTITY));
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
			case ITEM_PORTAL: {
				GET_OBJ_VAL(obj, VAL_PORTAL_TARGET_VNUM) = NOTHING;
				break;
			}
			case ITEM_WEAPON: {
				GET_OBJ_VAL(obj, VAL_WEAPON_TYPE) = TYPE_SLASH;
				break;
			}
			default: {
				break;
			}
		}
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
	
	if (GET_OBJ_TYPE(obj) != ITEM_WEAPON) {
		msg_to_char(ch, "You can only set weapontype on a weapon.\r\n");
	}
	else {
		GET_OBJ_VAL(obj, VAL_WEAPON_TYPE) = olc_process_type(ch, argument, "weapon type", "weapontype", (const char**)get_weapon_types_string(), GET_OBJ_VAL(obj, VAL_WEAPON_TYPE));
	}
}


OLC_MODULE(oedit_wear) {
	obj_data *obj = GET_OLC_OBJECT(ch->desc);
	GET_OBJ_WEAR(obj) = olc_process_flag(ch, argument, "wear", "wear", wear_bits, GET_OBJ_WEAR(obj));
}
