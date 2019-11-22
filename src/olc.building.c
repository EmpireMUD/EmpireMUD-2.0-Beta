/* ************************************************************************
*   File: olc.building.c                                  EmpireMUD 2.0b5 *
*  Usage: OLC for building prototypes                                     *
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

// external consts
extern const char *bld_flags[];
extern const char *bld_relationship_types[];
extern const char *designate_flags[];
extern const char *function_flags[];
extern const char *interact_types[];
extern const byte interact_vnum_types[NUM_INTERACTS];
extern const char *olc_type_bits[NUM_OLC_TYPES+1];
extern const char *room_aff_bits[];
extern const char *spawn_flags[];

// external funcs
void init_building(bld_data *building);
void replace_question_color(char *input, char *color, char *output);
void sort_interactions(struct interaction_item **list);

// local funcs
void get_bld_relations_display(struct bld_relation *list, char *save_buffer);

// locals
const char *default_building_name = "Unnamed Building";
const char *default_building_title = "An Unnamed Building";
const char *default_building_icon = "&0[  ]";


 //////////////////////////////////////////////////////////////////////////////
//// HELPERS /////////////////////////////////////////////////////////////////

/**
* Checks for common building problems and reports them to ch.
*
* @param bld_data *bld The building to audit.
* @param char_data *ch The person to report to.
* @return bool TRUE if any problems were reported; FALSE if all good.
*/
bool audit_building(bld_data *bld, char_data *ch) {
	extern bool audit_extra_descs(any_vnum vnum, struct extra_descr_data *list, char_data *ch);
	extern bool audit_interactions(any_vnum vnum, struct interaction_item *list, int attach_type, char_data *ch);
	extern bool audit_spawns(any_vnum vnum, struct spawn_info *list, char_data *ch);
	
	struct trig_proto_list *tpl;
	bool problem = FALSE;
	trig_data *trig;
	
	if (!str_cmp(GET_BLD_NAME(bld), default_building_name)) {
		olc_audit_msg(ch, GET_BLD_VNUM(bld), "Name not set");
		problem = TRUE;
	}
	if (!str_cmp(GET_BLD_TITLE(bld), default_building_title)) {
		olc_audit_msg(ch, GET_BLD_VNUM(bld), "Title not set");
		problem = TRUE;
	}
	if (!IS_SET(GET_BLD_FLAGS(bld), BLD_ROOM) && !str_cmp(GET_BLD_ICON(bld), default_building_icon)) {
		olc_audit_msg(ch, GET_BLD_VNUM(bld), "Icon not set");
		problem = TRUE;
	}
	if (!IS_SET(GET_BLD_FLAGS(bld), BLD_OPEN) && (!GET_BLD_DESC(bld) || !*GET_BLD_DESC(bld) || !str_cmp(GET_BLD_DESC(bld), "Nothing.\r\n"))) {
		olc_audit_msg(ch, GET_BLD_VNUM(bld), "Description not set");
		problem = TRUE;
	}
	else if (!IS_SET(GET_BLD_FLAGS(bld), BLD_OPEN) && !strn_cmp(GET_BLD_DESC(bld), "Nothing.", 8)) {
		olc_audit_msg(ch, GET_BLD_VNUM(bld), "Description starting with 'Nothing.'");
		problem = TRUE;
	}
	if (GET_BLD_EXTRA_ROOMS(bld) > 0 && IS_SET(GET_BLD_FLAGS(bld), BLD_ROOM)) {
		olc_audit_msg(ch, GET_BLD_VNUM(bld), "Designated room has extra rooms set");
		problem = TRUE;
	}
	if (GET_BLD_EXTRA_ROOMS(bld) > 0 && GET_BLD_DESIGNATE_FLAGS(bld) == NOBITS) {
		olc_audit_msg(ch, GET_BLD_VNUM(bld), "Has extra rooms but no designate flags");
		problem = TRUE;
	}
	if (IS_SET(GET_BLD_FLAGS(bld), BLD_ROOM) && IS_SET(GET_BLD_FLAGS(bld), BLD_OPEN | BLD_CLOSED | BLD_TWO_ENTRANCES | BLD_ATTACH_ROAD | BLD_INTERLINK)) {
		olc_audit_msg(ch, GET_BLD_VNUM(bld), "Designated room has incompatible flag(s)");
		problem = TRUE;
	}
	if (!IS_SET(GET_BLD_FLAGS(bld), BLD_ROOM) && IS_SET(GET_BLD_FLAGS(bld), BLD_SECONDARY_TERRITORY)) {
		olc_audit_msg(ch, GET_BLD_VNUM(bld), "2ND-TERRITORY flag on a non-designated building");
		problem = TRUE;
	}
	if (!IS_SET(GET_BLD_FLAGS(bld), BLD_ROOM) && !GET_BLD_YEARLY_MAINTENANCE(bld)) {
		olc_audit_msg(ch, GET_BLD_VNUM(bld), "Requires no maintenance");
		problem = TRUE;
	}
	if (IS_SET(GET_BLD_FLAGS(bld), BLD_ROOM) && GET_BLD_YEARLY_MAINTENANCE(bld)) {
		olc_audit_msg(ch, GET_BLD_VNUM(bld), "Interior room has yearly maintenance (will have no effect)");
		problem = TRUE;
	}
	if (IS_SET(GET_BLD_FLAGS(bld), BLD_ROOM) && count_bld_relations(bld, BLD_REL_UPGRADES_TO) > 0) {
		olc_audit_msg(ch, GET_BLD_VNUM(bld), "Interior room has upgrades-to");
		problem = TRUE;
	}
	
	// check scripts
	for (tpl = GET_BLD_SCRIPTS(bld); tpl; tpl = tpl->next) {
		if (!(trig = real_trigger(tpl->vnum))) {
			continue;
		}
		if (trig->attach_type != WLD_TRIGGER) {
			olc_audit_msg(ch, GET_BLD_VNUM(bld), "Incorrect trigger type (trg %d)", tpl->vnum);
			problem = TRUE;
		}
	}
	
	problem |= audit_extra_descs(GET_BLD_VNUM(bld), GET_BLD_EX_DESCS(bld), ch);
	problem |= audit_spawns(GET_BLD_VNUM(bld), GET_BLD_SPAWNS(bld), ch);
	problem |= audit_interactions(GET_BLD_VNUM(bld), GET_BLD_INTERACTIONS(bld), TYPE_ROOM, ch);
	
	return problem;
}


/**
* Determines if a building has a specific relationship with another building.
*
* @param bld_data *bld The building to test.
* @param int type The BLD_REL_ type to look for.
* @param bld_vnum vnum The building vnum we're looking for.
* @return bool TRUE if there is a relationship of that type; FALSE if not.
*/
bool bld_has_relation(bld_data *bld, int type, bld_vnum vnum) {
	struct bld_relation *relat;

	if (!bld || !GET_BLD_RELATIONS(bld)) {
		return FALSE;	// sanity/shortcut
	}
	
	LL_FOREACH(GET_BLD_RELATIONS(bld), relat) {
		if (relat->type == type && relat->vnum == vnum) {
			return TRUE;
		}
	}
	return FALSE;
}


/**
* Creates a copy of a building relation list.
*
* @param struct bld_relation *input_list A pointer to the start of the list to copy.
* @return struct bld_relation* A pointer to the start of the copy list.
*/
struct bld_relation *copy_bld_relations(struct bld_relation *input_list) {
	struct bld_relation *relat, *new_relat, *list = NULL;
	
	LL_FOREACH(input_list, relat) {
		CREATE(new_relat, struct bld_relation, 1);
		*new_relat = *relat;
		new_relat->next = NULL;
		LL_APPEND(list, new_relat);
	}
	
	return list;
}


/**
* Determine how many relations a building has, of a certain type.
*
* @param bld_data *bld The building to check.
* @param int type The BLD_REL_ type to count.
* @return int The number of relations of that type.
*/
int count_bld_relations(bld_data *bld, int type) {
	struct bld_relation *relat;
	int count = 0;
	
	LL_FOREACH(GET_BLD_RELATIONS(bld), relat) {
		if (relat->type == type) {
			++count;
		}
	}
	return count;
}


/**
* Creates a new building entry.
* 
* @param bld_vnum vnum The number to create.
* @return bld_data *The new building's prototype.
*/
bld_data *create_building_table_entry(bld_vnum vnum) {
	void add_building_to_table(bld_data *bld);
	
	bld_data *bld;
	
	// sanity
	if (building_proto(vnum)) {
		log("SYSERR: Attempting to insert building at existing vnum %d", vnum);
		return building_proto(vnum);
	}
	
	CREATE(bld, bld_data, 1);
	init_building(bld);
	GET_BLD_VNUM(bld) = vnum;
	add_building_to_table(bld);
	
	// save index and building file now
	save_index(DB_BOOT_BLD);
	save_library_file_for_vnum(DB_BOOT_BLD, vnum);

	return bld;
}


/**
* This function is meant to remove building relationships when the bld they
* relate to is deleted.
*
* @param struct bld_relation **list The list to remove from.
* @param int type The BLD_REL_ type to remove.
* @param bld_vnum vnum The item will only be removed if its type and vnum match.
* @return bool TRUE if any relations were deleted, FALSE if not
*/
bool delete_bld_relation_by_type_and_vnum(struct bld_relation **list, int type, bld_vnum vnum) {
	struct bld_relation *relat, *next_relat;
	bool found = FALSE;
	
	LL_FOREACH_SAFE(*list, relat, next_relat) {
		if (relat->type == type && relat->vnum == vnum) {
			found = TRUE;
			LL_DELETE(*list, relat);
			free(relat);
		}
	}
	
	return found;
}


/**
* This function is meant to remove building relationships when the bld they
* relate to is deleted -- without regard to type.
*
* @param struct bld_relation **list The list to remove from.
* @param bld_vnum vnum The vnum to remove.
* @return bool TRUE if any relations were deleted, FALSE if not
*/
bool delete_bld_relation_by_vnum(struct bld_relation **list, bld_vnum vnum) {
	struct bld_relation *relat, *next_relat;
	bool found = FALSE;
	
	LL_FOREACH_SAFE(*list, relat, next_relat) {
		if (relat->vnum == vnum) {
			found = TRUE;
			LL_DELETE(*list, relat);
			free(relat);
		}
	}
	
	return found;
}


/**
* @param struct bld_relation *list The list to free.
*/
void free_bld_relations(struct bld_relation *list) {
	struct bld_relation *iter, *next_iter;
	LL_FOREACH_SAFE(list, iter, next_iter) {
		free(iter);
	}
}


/**
* Quick way to turn a vnum into a name, safely.
*
* @param bld_vnum vnum The vnum to look up.
* @return char* A name for the vnum, or "UNKNOWN".
*/
char *get_bld_name_by_proto(bld_vnum vnum) {
	bld_data *proto = building_proto(vnum);
	char *unk = "UNKNOWN";
	
	return proto ? GET_BLD_NAME(proto) : unk;
}


/**
* For the .list command.
*
* @param bld_data *bld The thing to list.
* @param bool detail If TRUE, provide additional details
* @return char* The line to show (without a CRLF).
*/
char *list_one_building(bld_data *bld, bool detail) {
	static char output[MAX_STRING_LENGTH];
	
	if (detail) {
		snprintf(output, sizeof(output), "[%5d] %s", GET_BLD_VNUM(bld), GET_BLD_NAME(bld));
	}
	else {
		snprintf(output, sizeof(output), "[%5d] %s", GET_BLD_VNUM(bld), GET_BLD_NAME(bld));
	}
	
	return output;
}


/**
* WARNING: This function actually deletes a building.
*
* @param char_data *ch The person doing the deleting.
* @param bld_vnum vnum The vnum to delete.
*/
void olc_delete_building(char_data *ch, bld_vnum vnum) {
	void check_for_bad_buildings();
	extern bool delete_link_rule_by_type_value(struct adventure_link_rule **list, int type, any_vnum value);
	extern bool delete_quest_giver_from_list(struct quest_giver **list, int type, any_vnum vnum);
	extern bool delete_requirement_from_list(struct req_data **list, int type, any_vnum vnum);
	void remove_building_from_table(bld_data *bld);
	
	struct obj_storage_type *store, *next_store;
	bld_data *bld, *biter, *next_biter;
	craft_data *craft, *next_craft;
	quest_data *quest, *next_quest;
	progress_data *prg, *next_prg;
	vehicle_data *veh, *next_veh;
	room_data *room, *next_room;
	social_data *soc, *next_soc;
	shop_data *shop, *next_shop;
	adv_data *adv, *next_adv;
	obj_data *obj, *next_obj;
	descriptor_data *desc;
	int count;
	bool found, deleted = FALSE;
	
	if (!(bld = building_proto(vnum))) {
		msg_to_char(ch, "There is no such building %d.\r\n", vnum);
		return;
	}
	
	if (HASH_COUNT(building_table) <= 1) {
		msg_to_char(ch, "You can't delete the last building.\r\n");
		return;
	}
	
	// pull from hash
	remove_building_from_table(bld);

	// save index and building file now
	save_index(DB_BOOT_BLD);
	save_library_file_for_vnum(DB_BOOT_BLD, vnum);
	
	// update world
	count = 0;
	HASH_ITER(hh, world_table, room, next_room) {
		if (IS_ANY_BUILDING(room) && BUILDING_VNUM(room) == vnum) {
			if (GET_ROOM_VNUM(room) >= MAP_SIZE) {
				delete_room(room, FALSE);	// must call check_all_exits
				deleted = TRUE;
			}
			else {
				// map room
				if (ROOM_PEOPLE(room)) {
					act("The building has been deleted.", FALSE, ROOM_PEOPLE(room), NULL, NULL, TO_CHAR | TO_ROOM);
				}
				disassociate_building(room);
			}
		}
	}
	if (deleted) {
		check_all_exits();
	}
	
	// adventure zones
	HASH_ITER(hh, adventure_table, adv, next_adv) {
		found = delete_link_rule_by_type_value(&GET_ADV_LINKING(adv), ADV_LINK_BUILDING_EXISTING, vnum);
		found |= delete_link_rule_by_type_value(&GET_ADV_LINKING(adv), ADV_LINK_BUILDING_NEW, vnum);
		found |= delete_link_rule_by_type_value(&GET_ADV_LINKING(adv), ADV_LINK_PORTAL_BUILDING_EXISTING, vnum);
		found |= delete_link_rule_by_type_value(&GET_ADV_LINKING(adv), ADV_LINK_PORTAL_BUILDING_NEW, vnum);
		
		if (found) {
			save_library_file_for_vnum(DB_BOOT_ADV, GET_ADV_VNUM(adv));
		}
	}
	
	// buildings
	HASH_ITER(hh, building_table, biter, next_biter) {
		if (delete_bld_relation_by_vnum(&GET_BLD_RELATIONS(biter), vnum)) {
			save_library_file_for_vnum(DB_BOOT_BLD, GET_BLD_VNUM(biter));
		}
	}
	
	// crafts
	HASH_ITER(hh, craft_table, craft, next_craft) {
		if (GET_CRAFT_TYPE(craft) == CRAFT_TYPE_BUILD && GET_CRAFT_BUILD_TYPE(craft) == vnum) {
			GET_CRAFT_BUILD_TYPE(craft) = NOTHING;
			SET_BIT(GET_CRAFT_FLAGS(craft), CRAFT_IN_DEVELOPMENT);
			save_library_file_for_vnum(DB_BOOT_CRAFT, GET_CRAFT_VNUM(craft));
		}
	}
	
	// obj storage
	HASH_ITER(hh, object_table, obj, next_obj) {
		LL_FOREACH_SAFE(obj->storage, store, next_store) {
			if (store->building_type == vnum) {
				LL_DELETE(obj->storage, store);
				free(store);
				save_library_file_for_vnum(DB_BOOT_OBJ, GET_OBJ_VNUM(obj));
			}
		}
	}
	
	// update progress
	HASH_ITER(hh, progress_table, prg, next_prg) {
		found = delete_requirement_from_list(&PRG_TASKS(prg), REQ_OWN_BUILDING, vnum);
		found |= delete_requirement_from_list(&PRG_TASKS(prg), REQ_VISIT_BUILDING, vnum);
		
		if (found) {
			SET_BIT(PRG_FLAGS(prg), PRG_IN_DEVELOPMENT);
			save_library_file_for_vnum(DB_BOOT_PRG, PRG_VNUM(prg));
			need_progress_refresh = TRUE;
		}
	}
	
	// quests
	HASH_ITER(hh, quest_table, quest, next_quest) {
		found = delete_quest_giver_from_list(&QUEST_STARTS_AT(quest), QG_BUILDING, vnum);
		found |= delete_quest_giver_from_list(&QUEST_ENDS_AT(quest), QG_BUILDING, vnum);
		found |= delete_requirement_from_list(&QUEST_TASKS(quest), REQ_OWN_BUILDING, vnum);
		found |= delete_requirement_from_list(&QUEST_PREREQS(quest), REQ_OWN_BUILDING, vnum);
		found |= delete_requirement_from_list(&QUEST_TASKS(quest), REQ_VISIT_BUILDING, vnum);
		found |= delete_requirement_from_list(&QUEST_PREREQS(quest), REQ_VISIT_BUILDING, vnum);
		
		if (found) {
			SET_BIT(QUEST_FLAGS(quest), QST_IN_DEVELOPMENT);
			save_library_file_for_vnum(DB_BOOT_QST, QUEST_VNUM(quest));
		}
	}
	
	// shops
	HASH_ITER(hh, shop_table, shop, next_shop) {
		found = delete_quest_giver_from_list(&SHOP_LOCATIONS(shop), QG_BUILDING, vnum);
		
		if (found) {
			SET_BIT(SHOP_FLAGS(shop), SHOP_IN_DEVELOPMENT);
			save_library_file_for_vnum(DB_BOOT_SHOP, SHOP_VNUM(shop));
		}
	}
	
	// socials
	HASH_ITER(hh, social_table, soc, next_soc) {
		found = delete_requirement_from_list(&SOC_REQUIREMENTS(soc), REQ_OWN_BUILDING, vnum);
		found |= delete_requirement_from_list(&SOC_REQUIREMENTS(soc), REQ_VISIT_BUILDING, vnum);
		
		if (found) {
			SET_BIT(SOC_FLAGS(soc), SOC_IN_DEVELOPMENT);
			save_library_file_for_vnum(DB_BOOT_SOC, SOC_VNUM(soc));
		}
	}
	
	// vehicles
	HASH_ITER(hh, vehicle_table, veh, next_veh) {
		if (VEH_INTERIOR_ROOM_VNUM(veh) == vnum) {
			VEH_INTERIOR_ROOM_VNUM(veh) = NOTHING;
			save_library_file_for_vnum(DB_BOOT_VEH, VEH_VNUM(veh));
		}
	}

	// olc editors
	for (desc = descriptor_list; desc; desc = desc->next) {
		if (GET_OLC_ADVENTURE(desc)) {
			found = delete_link_rule_by_type_value(&(GET_OLC_ADVENTURE(desc)->linking), ADV_LINK_BUILDING_EXISTING, vnum);
			found |= delete_link_rule_by_type_value(&(GET_OLC_ADVENTURE(desc)->linking), ADV_LINK_BUILDING_NEW, vnum);
			found |= delete_link_rule_by_type_value(&(GET_OLC_ADVENTURE(desc)->linking), ADV_LINK_PORTAL_BUILDING_EXISTING, vnum);
			found |= delete_link_rule_by_type_value(&(GET_OLC_ADVENTURE(desc)->linking), ADV_LINK_PORTAL_BUILDING_NEW, vnum);
	
			if (found) {
				msg_to_desc(desc, "One or more linking rules have been removed from the adventure you are editing.\r\n");
			}
		}
		if (GET_OLC_BUILDING(desc)) {
			if (delete_bld_relation_by_vnum(&GET_BLD_RELATIONS(GET_OLC_BUILDING(desc)), vnum)) {
				msg_to_desc(desc, "A building related to the building you're editing was deleted.\r\n");
			}
		}
		if (GET_OLC_CRAFT(desc)) {
			if (GET_OLC_CRAFT(desc)->type == CRAFT_TYPE_BUILD && GET_OLC_CRAFT(desc)->build_type == vnum) {
				GET_OLC_CRAFT(desc)->build_type = NOTHING;
				SET_BIT(GET_OLC_CRAFT(desc)->flags, CRAFT_IN_DEVELOPMENT);
				msg_to_desc(desc, "The building built by the craft you're editing was deleted.\r\n");
			}
		}
		if (GET_OLC_OBJECT(desc)) {
			found = FALSE;
			LL_FOREACH_SAFE(GET_OLC_OBJECT(desc)->storage, store, next_store) {
				if (store->building_type == vnum) {
					LL_DELETE(GET_OLC_OBJECT(desc)->storage, store);
					free(store);
					if (!found) {
						msg_to_desc(desc, "A storage location for the the object you're editing was deleted.\r\n");
						found = TRUE;
					}
				}
			}
		}
		if (GET_OLC_PROGRESS(desc)) {
			found = delete_requirement_from_list(&PRG_TASKS(GET_OLC_PROGRESS(desc)), REQ_OWN_BUILDING, vnum);
			found |= delete_requirement_from_list(&PRG_TASKS(GET_OLC_PROGRESS(desc)), REQ_VISIT_BUILDING, vnum);
		
			if (found) {
				SET_BIT(QUEST_FLAGS(GET_OLC_PROGRESS(desc)), PRG_IN_DEVELOPMENT);
				msg_to_desc(desc, "A building used by the progression goal you're editing has been deleted.\r\n");
			}
		}
		if (GET_OLC_QUEST(desc)) {
			found = delete_quest_giver_from_list(&QUEST_STARTS_AT(GET_OLC_QUEST(desc)), QG_BUILDING, vnum);
			found |= delete_quest_giver_from_list(&QUEST_ENDS_AT(GET_OLC_QUEST(desc)), QG_BUILDING, vnum);
			found |= delete_requirement_from_list(&QUEST_TASKS(GET_OLC_QUEST(desc)), REQ_OWN_BUILDING, vnum);
			found |= delete_requirement_from_list(&QUEST_PREREQS(GET_OLC_QUEST(desc)), REQ_OWN_BUILDING, vnum);
			found |= delete_requirement_from_list(&QUEST_TASKS(GET_OLC_QUEST(desc)), REQ_VISIT_BUILDING, vnum);
			found |= delete_requirement_from_list(&QUEST_PREREQS(GET_OLC_QUEST(desc)), REQ_VISIT_BUILDING, vnum);
		
			if (found) {
				SET_BIT(QUEST_FLAGS(GET_OLC_QUEST(desc)), QST_IN_DEVELOPMENT);
				msg_to_desc(desc, "A building used by the quest you are editing was deleted.\r\n");
			}
		}
		if (GET_OLC_SHOP(desc)) {
			found = delete_quest_giver_from_list(&SHOP_LOCATIONS(GET_OLC_SHOP(desc)), QG_BUILDING, vnum);
		
			if (found) {
				SET_BIT(SHOP_FLAGS(GET_OLC_SHOP(desc)), SHOP_IN_DEVELOPMENT);
				msg_to_desc(desc, "A building used by the shop you are editing was deleted.\r\n");
			}
		}
		if (GET_OLC_SOCIAL(desc)) {
			found = delete_requirement_from_list(&SOC_REQUIREMENTS(GET_OLC_SOCIAL(desc)), REQ_OWN_BUILDING, vnum);
			found |= delete_requirement_from_list(&SOC_REQUIREMENTS(GET_OLC_SOCIAL(desc)), REQ_VISIT_BUILDING, vnum);
		
			if (found) {
				SET_BIT(SOC_FLAGS(GET_OLC_SOCIAL(desc)), SOC_IN_DEVELOPMENT);
				msg_to_desc(desc, "A building required by the social you are editing was deleted.\r\n");
			}
		}
		if (GET_OLC_VEHICLE(desc)) {
			if (VEH_INTERIOR_ROOM_VNUM(GET_OLC_VEHICLE(desc)) == vnum) {
				VEH_INTERIOR_ROOM_VNUM(GET_OLC_VEHICLE(desc)) = NOTHING;
				msg_to_desc(desc, "The interior home room building for the the vehicle you're editing was deleted.\r\n");
			}
		}
	}
	
	syslog(SYS_OLC, GET_INVIS_LEV(ch), TRUE, "OLC: %s has deleted building %d", GET_NAME(ch), vnum);
	msg_to_char(ch, "Building %d deleted.\r\n", vnum);
	
	if (count > 0) {
		msg_to_char(ch, "%d live buildings changed.\r\n", count);
	}
	
	free_building(bld);
	
	// various building cleanup
	check_for_bad_buildings();
}


/**
* Searches properties of buildings.
*
* @param char_data *ch The person searching.
* @param char *argument The argument they entered.
*/
void olc_fullsearch_building(char_data *ch, char *argument) {
	char buf[MAX_STRING_LENGTH], line[MAX_STRING_LENGTH], type_arg[MAX_INPUT_LENGTH], val_arg[MAX_INPUT_LENGTH], find_keywords[MAX_INPUT_LENGTH];
	int count;
	
	char only_icon[MAX_INPUT_LENGTH], only_commands[MAX_INPUT_LENGTH];
	bitvector_t only_designate = NOBITS, only_flags = NOBITS, only_functions = NOBITS;
	bitvector_t find_interacts = NOBITS, not_flagged = NOBITS, found_interacts = NOBITS;
	bitvector_t only_affs = NOBITS;
	int only_cits = NOTHING, cits_over = NOTHING, cits_under = NOTHING;
	int only_fame = NOTHING, fame_over = NOTHING, fame_under = NOTHING;
	int only_hitpoints = NOTHING, hitpoints_over = NOTHING, hitpoints_under = NOTHING;
	int only_military = NOTHING, military_over = NOTHING, military_under = NOTHING;
	int only_rooms = NOTHING, rooms_over = NOTHING, rooms_under = NOTHING;
	
	struct interaction_item *inter;
	bld_data *bld, *next_bld;
	size_t size;
	
	*only_icon = '\0';
	*only_commands = '\0';
	
	if (!*argument) {
		msg_to_char(ch, "See HELP BEDIT FULLSEARCH for syntax.\r\n");
		return;
	}
	
	// process argument
	*find_keywords = '\0';
	while (*argument) {
		// figure out a type
		argument = any_one_arg(argument, type_arg);
		
		if (!strcmp(type_arg, "-")) {
			continue;	// just skip stray dashes
		}
		
		FULLSEARCH_FLAGS("affects", only_affs, room_aff_bits)
		FULLSEARCH_INT("citizens", only_cits, 0, INT_MAX)
		FULLSEARCH_INT("citizensover", cits_over, 0, INT_MAX)
		FULLSEARCH_INT("citizensunder", cits_under, 0, INT_MAX)
		FULLSEARCH_STRING("commands", only_commands)
		FULLSEARCH_FLAGS("designate", only_designate, designate_flags)
		FULLSEARCH_INT("fame", only_fame, 0, INT_MAX)
		FULLSEARCH_INT("fameover", fame_over, 0, INT_MAX)
		FULLSEARCH_INT("fameunder", fame_under, 0, INT_MAX)
		FULLSEARCH_FLAGS("flags", only_flags, bld_flags)
		FULLSEARCH_FLAGS("flagged", only_flags, bld_flags)
		FULLSEARCH_FLAGS("unflagged", not_flagged, bld_flags)
		FULLSEARCH_FLAGS("functions", only_functions, function_flags)
		FULLSEARCH_STRING("icon", only_icon)
		FULLSEARCH_FLAGS("interaction", find_interacts, interact_types)
		FULLSEARCH_INT("hitpoints", only_hitpoints, 0, INT_MAX)
		FULLSEARCH_INT("hitpointsover", hitpoints_over, 0, INT_MAX)
		FULLSEARCH_INT("hitpointsunder", hitpoints_under, 0, INT_MAX)
		FULLSEARCH_INT("rooms", only_rooms, 0, INT_MAX)
		FULLSEARCH_INT("roomsover", rooms_over, 0, INT_MAX)
		FULLSEARCH_INT("roomsunder", rooms_under, 0, INT_MAX)
		FULLSEARCH_INT("military", only_military, 0, INT_MAX)
		FULLSEARCH_INT("militaryover", military_over, 0, INT_MAX)
		FULLSEARCH_INT("militaryunder", military_under, 0, INT_MAX)
		
		else {	// not sure what to do with it? treat it like a keyword
			sprintf(find_keywords + strlen(find_keywords), "%s%s", *find_keywords ? " " : "", type_arg);
		}
		
		// prepare for next loop
		skip_spaces(&argument);
	}
	
	size = snprintf(buf, sizeof(buf), "Building fullsearch: %s\r\n", find_keywords);
	count = 0;
	
	// okay now look up items
	HASH_ITER(hh, building_table, bld, next_bld) {
		if (only_affs != NOBITS && (GET_BLD_BASE_AFFECTS(bld) & only_affs) != only_affs) {
			continue;
		}
		if (only_cits != NOTHING && GET_BLD_CITIZENS(bld) != only_cits) {
			continue;
		}
		if (cits_over != NOTHING && GET_BLD_CITIZENS(bld) < cits_over) {
			continue;
		}
		if (cits_under != NOTHING && (GET_BLD_CITIZENS(bld) > cits_under || GET_BLD_CITIZENS(bld) == 0)) {
			continue;
		}
		if (only_designate != NOBITS && (GET_BLD_DESIGNATE_FLAGS(bld) & only_designate) != only_designate) {
			continue;
		}
		if (only_fame != NOTHING && GET_BLD_FAME(bld) != only_fame) {
			continue;
		}
		if (fame_over != NOTHING && GET_BLD_FAME(bld) < fame_over) {
			continue;
		}
		if (fame_under != NOTHING && (GET_BLD_FAME(bld) > fame_under || GET_BLD_FAME(bld) == 0)) {
			continue;
		}
		if (not_flagged != NOBITS && IS_SET(GET_BLD_FLAGS(bld), not_flagged)) {
			continue;
		}
		if (only_flags != NOBITS && (GET_BLD_FLAGS(bld) & only_flags) != only_flags) {
			continue;
		}
		if (only_functions != NOBITS && (GET_BLD_FUNCTIONS(bld) & only_functions) != only_functions) {
			continue;
		}
		if (only_hitpoints != NOTHING && GET_BLD_MAX_DAMAGE(bld) != only_hitpoints) {
			continue;
		}
		if (hitpoints_over != NOTHING && GET_BLD_MAX_DAMAGE(bld) < hitpoints_over) {
			continue;
		}
		if (hitpoints_under != NOTHING && GET_BLD_MAX_DAMAGE(bld) > hitpoints_under) {
			continue;
		}
		if (*only_icon && !GET_BLD_ICON(bld)) {
			continue;
		}
		if (*only_icon && !strstr(GET_BLD_ICON(bld), only_icon) && !strstr(strip_color(GET_BLD_ICON(bld)), only_icon)) {
			continue;
		}
		if (*only_commands && !GET_BLD_COMMANDS(bld)) {
			continue;
		}
		if (*only_commands && !multi_isname(only_commands, GET_BLD_COMMANDS(bld))) {
			continue;
		}
		if (find_interacts) {	// look up its interactions
			found_interacts = NOBITS;
			LL_FOREACH(GET_BLD_INTERACTIONS(bld), inter) {
				found_interacts |= BIT(inter->type);
			}
			if ((find_interacts & found_interacts) != find_interacts) {
				continue;
			}
		}
		if (only_military != NOTHING && GET_BLD_MILITARY(bld) != only_military) {
			continue;
		}
		if (military_over != NOTHING && GET_BLD_MILITARY(bld) < military_over) {
			continue;
		}
		if (military_under != NOTHING && (GET_BLD_MILITARY(bld) > military_under || GET_BLD_MILITARY(bld) == 0)) {
			continue;
		}
		if (only_rooms != NOTHING && GET_BLD_EXTRA_ROOMS(bld) != only_rooms) {
			continue;
		}
		if (rooms_over != NOTHING && GET_BLD_EXTRA_ROOMS(bld) < rooms_over) {
			continue;
		}
		if (rooms_under != NOTHING && (GET_BLD_EXTRA_ROOMS(bld) > rooms_under || GET_BLD_EXTRA_ROOMS(bld) == 0)) {
			continue;
		}

		if (*find_keywords && !multi_isname(find_keywords, GET_BLD_NAME(bld)) && !multi_isname(find_keywords, GET_BLD_TITLE(bld)) && !multi_isname(find_keywords, GET_BLD_DESC(bld)) && !search_extra_descs(find_keywords, GET_BLD_EX_DESCS(bld))) {
			continue;
		}
		
		// show it
		snprintf(line, sizeof(line), "[%5d] %s\r\n", GET_BLD_VNUM(bld), GET_BLD_NAME(bld));
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
		size += snprintf(buf + size, sizeof(buf) - size, "(%d buildings)\r\n", count);
	}
	else if (count == 0) {
		size += snprintf(buf + size, sizeof(buf) - size, " none\r\n");
	}
	
	if (ch->desc) {
		page_string(ch->desc, buf, TRUE);
	}
}


/**
* Searches for all uses of a building and displays them.
*
* @param char_data *ch The player.
* @param bld_vnum vnum The building vnum.
*/
void olc_search_building(char_data *ch, bld_vnum vnum) {
	extern bool find_quest_giver_in_list(struct quest_giver *list, int type, any_vnum vnum);
	extern bool find_requirement_in_list(struct req_data *list, int type, any_vnum vnum);
	
	char buf[MAX_STRING_LENGTH];
	bld_data *proto = building_proto(vnum);
	struct adventure_link_rule *link;
	struct obj_storage_type *store;
	craft_data *craft, *next_craft;
	quest_data *quest, *next_quest;
	progress_data *prg, *next_prg;
	vehicle_data *veh, *next_veh;
	social_data *soc, *next_soc;
	shop_data *shop, *next_shop;
	struct bld_relation *relat;
	adv_data *adv, *next_adv;
	bld_data *bld, *next_bld;
	obj_data *obj, *next_obj;
	bool any;
	int size, found;
	
	if (!proto) {
		msg_to_char(ch, "There is no building %d.\r\n", vnum);
		return;
	}
	
	found = 0;
	size = snprintf(buf, sizeof(buf), "Occurrences of building %d (%s):\r\n", vnum, GET_BLD_NAME(proto));
	
	// adventure rules
	HASH_ITER(hh, adventure_table, adv, next_adv) {
		if (size >= sizeof(buf)) {
			break;
		}
		for (link = GET_ADV_LINKING(adv); link; link = link->next) {
			if (link->type == ADV_LINK_BUILDING_EXISTING || link->type == ADV_LINK_BUILDING_NEW || link->type == ADV_LINK_PORTAL_BUILDING_EXISTING || link->type == ADV_LINK_PORTAL_BUILDING_NEW) {
				if (link->value == vnum) {
					++found;
					size += snprintf(buf + size, sizeof(buf) - size, "ADV [%5d] %s\r\n", GET_ADV_VNUM(adv), GET_ADV_NAME(adv));
					// only report once per adventure
					break;
				}
			}
		}
	}
	
	// buildings
	HASH_ITER(hh, building_table, bld, next_bld) {
		if (size >= sizeof(buf)) {
			break;
		}
		LL_FOREACH(GET_BLD_RELATIONS(bld), relat) {
			if (relat->vnum == vnum) {
				++found;
				size += snprintf(buf + size, sizeof(buf) - size, "BLD [%5d] %s\r\n", GET_BLD_VNUM(bld), GET_BLD_NAME(bld));
				break;
			}
		}
	}
	
	// craft builds
	HASH_ITER(hh, craft_table, craft, next_craft) {
		if (size >= sizeof(buf)) {
			break;
		}
		if (GET_CRAFT_TYPE(craft) == CRAFT_TYPE_BUILD && GET_CRAFT_BUILD_TYPE(craft) == vnum) {
			++found;
			size += snprintf(buf + size, sizeof(buf) - size, "CFT [%5d] %s\r\n", GET_CRAFT_VNUM(craft), GET_CRAFT_NAME(craft));
		}
	}
	
	// obj storage
	HASH_ITER(hh, object_table, obj, next_obj) {
		if (size >= sizeof(buf)) {
			break;
		}
		any = FALSE;
		for (store = obj->storage; store && !any; store = store->next) {
			if (store->building_type == vnum) {
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
		any = find_requirement_in_list(PRG_TASKS(prg), REQ_OWN_BUILDING, vnum);
		any |= find_requirement_in_list(PRG_TASKS(prg), REQ_VISIT_BUILDING, vnum);
		
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
		if (find_quest_giver_in_list(QUEST_STARTS_AT(quest), QG_BUILDING, vnum) || find_quest_giver_in_list(QUEST_ENDS_AT(quest), QG_BUILDING, vnum) || find_requirement_in_list(QUEST_TASKS(quest), REQ_OWN_BUILDING, vnum) || find_requirement_in_list(QUEST_PREREQS(quest), REQ_OWN_BUILDING, vnum) || find_requirement_in_list(QUEST_TASKS(quest), REQ_VISIT_BUILDING, vnum) || find_requirement_in_list(QUEST_PREREQS(quest), REQ_VISIT_BUILDING, vnum)) {
			++found;
			size += snprintf(buf + size, sizeof(buf) - size, "QST [%5d] %s\r\n", QUEST_VNUM(quest), QUEST_NAME(quest));
		}
	}
	
	// shops
	HASH_ITER(hh, shop_table, shop, next_shop) {
		if (size >= sizeof(buf)) {
			break;
		}
		if (find_quest_giver_in_list(SHOP_LOCATIONS(shop), QG_BUILDING, vnum)) {
			++found;
			size += snprintf(buf + size, sizeof(buf) - size, "SHOP [%5d] %s\r\n", SHOP_VNUM(shop), SHOP_NAME(shop));
		}
	}
	
	// socials
	HASH_ITER(hh, social_table, soc, next_soc) {
		if (size >= sizeof(buf)) {
			break;
		}
		if (find_requirement_in_list(SOC_REQUIREMENTS(soc), REQ_OWN_BUILDING, vnum) || find_requirement_in_list(SOC_REQUIREMENTS(soc), REQ_VISIT_BUILDING, vnum)) {
			++found;
			size += snprintf(buf + size, sizeof(buf) - size, "SOC [%5d] %s\r\n", SOC_VNUM(soc), SOC_NAME(soc));
		}
	}
	
	// vehicles
	HASH_ITER(hh, vehicle_table, veh, next_veh) {
		if (VEH_INTERIOR_ROOM_VNUM(veh) == vnum) {
			++found;
			size += snprintf(buf + size, sizeof(buf) - size, "VEH [%5d] %s\r\n", VEH_VNUM(veh), VEH_SHORT_DESC(veh));
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
* Function to save a player's changes to a building (or a new one).
*
* @param descriptor_data *desc The descriptor who is saving.
*/
void save_olc_building(descriptor_data *desc) {
	void prune_extra_descs(struct extra_descr_data **list);

	bld_data *proto, *bdg = GET_OLC_BUILDING(desc);
	bld_vnum vnum = GET_OLC_VNUM(desc);
	struct trig_proto_list *trig;
	struct spawn_info *spawn;
	struct quest_lookup *ql;
	struct shop_lookup *sl;
	UT_hash_handle hh;
	
	// have a place to save it?
	if (!(proto = building_proto(vnum))) {
		proto = create_building_table_entry(vnum);
	}
	
	// free prototype strings and pointers
	if (GET_BLD_NAME(proto)) {
		free(GET_BLD_NAME(proto));
	}
	if (GET_BLD_TITLE(proto)) {
		free(GET_BLD_TITLE(proto));
	}
	if (GET_BLD_ICON(proto)) {
		free(GET_BLD_ICON(proto));
	}
	if (GET_BLD_COMMANDS(proto)) {
		free(GET_BLD_COMMANDS(proto));
	}
	if (GET_BLD_DESC(proto)) {
		free(GET_BLD_DESC(proto));
	}
	free_extra_descs(&GET_BLD_EX_DESCS(proto));
	while ((spawn = GET_BLD_SPAWNS(proto))) {
		GET_BLD_SPAWNS(proto) = spawn->next;
		free(spawn);
	}
	free_interactions(&GET_BLD_INTERACTIONS(proto));
	while ((trig = GET_BLD_SCRIPTS(proto))) {
		GET_BLD_SCRIPTS(proto) = trig->next;
		free(trig);
	}
	if (GET_BLD_YEARLY_MAINTENANCE(proto)) {
		free_resource_list(GET_BLD_YEARLY_MAINTENANCE(proto));
	}
	if (GET_BLD_RELATIONS(proto)) {
		free_bld_relations(GET_BLD_RELATIONS(proto));
	}
	
	// sanity
	prune_extra_descs(&GET_BLD_EX_DESCS(bdg));
	if (!GET_BLD_NAME(bdg) || !*GET_BLD_NAME(bdg)) {
		if (GET_BLD_NAME(bdg)) {
			free(GET_BLD_NAME(bdg));
		}
		GET_BLD_NAME(bdg) = str_dup(default_building_name);
	}
	if (!GET_BLD_TITLE(bdg) || !*GET_BLD_TITLE(bdg)) {
		if (GET_BLD_TITLE(bdg)) {
			free(GET_BLD_TITLE(bdg));
		}
		GET_BLD_TITLE(bdg) = str_dup(default_building_title);
	}
	if (GET_BLD_COMMANDS(bdg) && !*GET_BLD_COMMANDS(bdg)) {
		if (GET_BLD_COMMANDS(bdg)) {
			free(GET_BLD_COMMANDS(bdg));
		}
		GET_BLD_COMMANDS(bdg) = NULL;
	}
	if (GET_BLD_DESC(bdg) && !*GET_BLD_DESC(bdg)) {
		if (GET_BLD_DESC(bdg)) {
			free(GET_BLD_DESC(bdg));
		}
		GET_BLD_DESC(bdg) = NULL;
	}
	
	// save data back over the proto-type
	hh = proto->hh;	// save old hash handle
	ql = proto->quest_lookups;	// save lookups
	sl = proto->shop_lookups;
	
	*proto = *bdg;	// copy
	proto->vnum = vnum;	// ensure correct vnum
	
	proto->hh = hh;	// restore hash handle
	proto->quest_lookups = ql;	// restore lookups
	proto->shop_lookups = sl;
	
	// and save to file
	save_library_file_for_vnum(DB_BOOT_BLD, vnum);
}


/**
* Creates a copy of a building, or clears a new one, for editing.
* 
* @param bld_data *input The building to copy, or NULL to make a new one.
* @return bld_data* The copied building.
*/
bld_data *setup_olc_building(bld_data *input) {
	extern struct extra_descr_data *copy_extra_descs(struct extra_descr_data *list);
	extern struct resource_data *copy_resource_list(struct resource_data *input);
	
	bld_data *new;
	
	CREATE(new, bld_data, 1);
	init_building(new);
	
	if (input) {
		// copy normal data
		*new = *input;

		// copy things that are pointers
		GET_BLD_NAME(new) = GET_BLD_NAME(input) ? str_dup(GET_BLD_NAME(input)) : NULL;
		GET_BLD_TITLE(new) = GET_BLD_TITLE(input) ? str_dup(GET_BLD_TITLE(input)) : NULL;
		GET_BLD_ICON(new) = GET_BLD_ICON(input) ? str_dup(GET_BLD_ICON(input)) : NULL;
		GET_BLD_COMMANDS(new) = GET_BLD_COMMANDS(input) ? str_dup(GET_BLD_COMMANDS(input)) : NULL;
		GET_BLD_DESC(new) = GET_BLD_DESC(input) ? str_dup(GET_BLD_DESC(input)) : NULL;
		GET_BLD_RELATIONS(new) = GET_BLD_RELATIONS(input) ? copy_bld_relations(GET_BLD_RELATIONS(input)) : NULL;
		
		// copy extra descs
		GET_BLD_EX_DESCS(new) = copy_extra_descs(GET_BLD_EX_DESCS(input));
		
		// copy spawns
		GET_BLD_SPAWNS(new) = copy_spawn_list(GET_BLD_SPAWNS(input));
		
		// copy interactions
		GET_BLD_INTERACTIONS(new) = copy_interaction_list(GET_BLD_INTERACTIONS(input));
		
		// scripts
		GET_BLD_SCRIPTS(new) = copy_trig_protos(GET_BLD_SCRIPTS(input));
		
		// maintenance
		GET_BLD_YEARLY_MAINTENANCE(new) = copy_resource_list(GET_BLD_YEARLY_MAINTENANCE(input));
	}
	else {
		// brand new: some defaults
		GET_BLD_NAME(new) = str_dup(default_building_name);
		GET_BLD_TITLE(new) = str_dup(default_building_title);
		GET_BLD_ICON(new) = str_dup(default_building_icon);
		GET_BLD_MAX_DAMAGE(new) = 1;
	}
	
	// done
	return new;	
}


/**
* Copies entries from one list into another, only if they are not already in
* the to_list.
*
* @param struct bld_relation **to_list A pointer to the destination list.
* @param struct bld_relation *from_list The list to copy from.
*/
void smart_copy_bld_relations(struct bld_relation **to_list, struct bld_relation *from_list) {
	struct bld_relation *iter, *search, *relat;
	bool found;
	
	LL_FOREACH(from_list, iter) {
		// ensure not already in list
		found = FALSE;
		LL_FOREACH(*to_list, search) {
			if (search->type == iter->type && search->vnum == iter->vnum) {
				found = TRUE;
				break;
			}
		}
		
		// add it
		if (!found) {
			CREATE(relat, struct bld_relation, 1);
			relat->type = iter->type;
			relat->vnum = iter->vnum;
			LL_APPEND(*to_list, relat);
		}
	}
}


 //////////////////////////////////////////////////////////////////////////////
//// DISPLAYS ////////////////////////////////////////////////////////////////

/**
* This is the main recipe display for building OLC. It displays the user's
* currently-edited building.
*
* @param char_data *ch The person who is editing a building and will see its display.
*/
void olc_show_building(char_data *ch) {
	void get_extra_desc_display(struct extra_descr_data *list, char *save_buffer);
	void get_interaction_display(struct interaction_item *list, char *save_buffer);
	void get_resource_display(struct resource_data *list, char *save_buffer);
	void get_script_display(struct trig_proto_list *list, char *save_buffer);
	extern char *show_color_codes(char *string);
	
	bld_data *bdg = GET_OLC_BUILDING(ch->desc);
	char lbuf[MAX_STRING_LENGTH], buf1[MAX_STRING_LENGTH];
	bool is_room = IS_SET(GET_BLD_FLAGS(bdg), BLD_ROOM) ? TRUE : FALSE;
	struct spawn_info *spawn;
	int count;
	
	if (!bdg) {
		return;
	}
	
	*buf = '\0';
	sprintf(buf + strlen(buf), "[%s%d\t0] %s%s\t0\r\n", OLC_LABEL_CHANGED, GET_OLC_VNUM(ch->desc), OLC_LABEL_UNCHANGED, !building_proto(GET_BLD_VNUM(bdg)) ? "new building" : GET_BLD_NAME(building_proto(GET_BLD_VNUM(bdg))));
	sprintf(buf + strlen(buf), "<%sname\t0> %s\r\n", OLC_LABEL_STR(GET_BLD_NAME(bdg), default_building_name), GET_BLD_NAME(bdg));
	sprintf(buf + strlen(buf), "<%stitle\t0> %s\r\n", OLC_LABEL_STR(GET_BLD_TITLE(bdg), default_building_title), GET_BLD_TITLE(bdg));
	
	if (!is_room) {
		replace_question_color(NULLSAFE(GET_BLD_ICON(bdg)), "\t0", lbuf);
		sprintf(buf + strlen(buf), "<%sicon\t0> %s\t0  %s\r\n", OLC_LABEL_STR(GET_BLD_ICON(bdg), default_building_icon), lbuf, show_color_codes(NULLSAFE(GET_BLD_ICON(bdg))));
	}
	sprintf(buf + strlen(buf), "<%scommands\t0> %s\r\n", OLC_LABEL_STR(GET_BLD_COMMANDS(bdg), ""), GET_BLD_COMMANDS(bdg) ? GET_BLD_COMMANDS(bdg) : "");
	sprintf(buf + strlen(buf), "<%sdescription\t0>\r\n%s", OLC_LABEL_STR(GET_BLD_DESC(bdg), ""), GET_BLD_DESC(bdg) ? GET_BLD_DESC(bdg) : "");

	if (!is_room) {
		sprintf(buf + strlen(buf), "<%shitpoints\t0> %d\r\n", OLC_LABEL_VAL(GET_BLD_MAX_DAMAGE(bdg), 1), GET_BLD_MAX_DAMAGE(bdg));
		sprintf(buf + strlen(buf), "<%srooms\t0> %d\r\n", OLC_LABEL_VAL(GET_BLD_EXTRA_ROOMS(bdg), 0), GET_BLD_EXTRA_ROOMS(bdg));
	}

	sprintf(buf + strlen(buf), "<%sfame\t0> %d\r\n", OLC_LABEL_VAL(GET_BLD_FAME(bdg), 0), GET_BLD_FAME(bdg));	
	sprintf(buf + strlen(buf), "<%scitizens\t0> %d\r\n", OLC_LABEL_VAL(GET_BLD_CITIZENS(bdg), 0), GET_BLD_CITIZENS(bdg));
	sprintf(buf + strlen(buf), "<%smilitary\t0> %d\r\n", OLC_LABEL_VAL(GET_BLD_MILITARY(bdg), 0), GET_BLD_MILITARY(bdg));
	
	sprintf(buf + strlen(buf), "<%sartisan\t0> [%d] %s\r\n", OLC_LABEL_VAL(GET_BLD_ARTISAN(bdg), NOTHING), GET_BLD_ARTISAN(bdg), GET_BLD_ARTISAN(bdg) == NOTHING ? "none" : get_mob_name_by_proto(GET_BLD_ARTISAN(bdg)));
	
	sprintbit(GET_BLD_FLAGS(bdg), bld_flags, lbuf, TRUE);
	sprintf(buf + strlen(buf), "<%sflags\t0> %s\r\n", OLC_LABEL_VAL(GET_BLD_FLAGS(bdg), NOBITS), lbuf);
	
	sprintbit(GET_BLD_FUNCTIONS(bdg), function_flags, lbuf, TRUE);
	sprintf(buf + strlen(buf), "<%sfunctions\t0> %s\r\n", OLC_LABEL_VAL(GET_BLD_FUNCTIONS(bdg), NOBITS), lbuf);
	
	sprintbit(GET_BLD_DESIGNATE_FLAGS(bdg), designate_flags, lbuf, TRUE);
	sprintf(buf + strlen(buf), "<%sdesignate\t0> %s\r\n", OLC_LABEL_VAL(GET_BLD_DESIGNATE_FLAGS(bdg), NOBITS), lbuf);
	
	sprintbit(GET_BLD_BASE_AFFECTS(bdg), room_aff_bits, lbuf, TRUE);
	sprintf(buf + strlen(buf), "<%saffects\t0> %s\r\n", OLC_LABEL_VAL(GET_BLD_BASE_AFFECTS(bdg), NOBITS), lbuf);
	
	sprintf(buf + strlen(buf), "Relationships: <%srelations\t0>\r\n", OLC_LABEL_PTR(GET_BLD_RELATIONS(bdg)));
	if (GET_BLD_RELATIONS(bdg)) {
		get_bld_relations_display(GET_BLD_RELATIONS(bdg), buf1);
		strcat(buf, buf1);
	}

	// exdesc
	sprintf(buf + strlen(buf), "Extra descriptions: <%sextra\t0>\r\n", OLC_LABEL_PTR(GET_BLD_EX_DESCS(bdg)));
	if (GET_BLD_EX_DESCS(bdg)) {
		get_extra_desc_display(GET_BLD_EX_DESCS(bdg), buf1);
		strcat(buf, buf1);
	}

	sprintf(buf + strlen(buf), "Interactions: <%sinteraction\t0>\r\n", OLC_LABEL_PTR(GET_BLD_INTERACTIONS(bdg)));
	if (GET_BLD_INTERACTIONS(bdg)) {
		get_interaction_display(GET_BLD_INTERACTIONS(bdg), buf1);
		strcat(buf, buf1);
	}
	
	// maintenance resources
	sprintf(buf + strlen(buf), "Yearly maintenance resources required: <%sresource\t0>\r\n", OLC_LABEL_PTR(GET_BLD_YEARLY_MAINTENANCE(bdg)));
	if (GET_BLD_YEARLY_MAINTENANCE(bdg)) {
		get_resource_display(GET_BLD_YEARLY_MAINTENANCE(bdg), buf1);
		strcat(buf, buf1);
	}

	// scripts
	sprintf(buf + strlen(buf), "Scripts: <%sscript\t0>\r\n", OLC_LABEL_PTR(GET_BLD_SCRIPTS(bdg)));
	if (GET_BLD_SCRIPTS(bdg)) {
		get_script_display(GET_BLD_SCRIPTS(bdg), lbuf);
		strcat(buf, lbuf);
	}
	
	sprintf(buf + strlen(buf), "<%sspawns\t0>\r\n", OLC_LABEL_PTR(GET_BLD_SPAWNS(bdg)));
	if (GET_BLD_SPAWNS(bdg)) {
		count = 0;
		for (spawn = GET_BLD_SPAWNS(bdg); spawn; spawn = spawn->next) {
			++count;
		}
		sprintf(buf + strlen(buf), " %d spawn%s set\r\n", count, PLURAL(count));
	}
		
	page_string(ch->desc, buf, TRUE);
}


/**
* Displays the relationship data from a given list.
*
* @param struct bld_relation *list Pointer to the start of a list of relations.
* @param char *save_buffer A buffer to store the result to.
*/
void get_bld_relations_display(struct bld_relation *list, char *save_buffer) {
	struct bld_relation *relat;
	int count = 0;
	
	*save_buffer = '\0';
	LL_FOREACH(list, relat) {
		sprintf(save_buffer + strlen(save_buffer), "%2d. %s: [%5d] %s\r\n", ++count, bld_relationship_types[relat->type], relat->vnum, get_bld_name_by_proto(relat->vnum));
	}
	
	if (count == 0) {
		strcat(save_buffer, " none\r\n");
	}
}


 //////////////////////////////////////////////////////////////////////////////
//// EDIT MODULES ////////////////////////////////////////////////////////////

OLC_MODULE(bedit_affects) {
	bld_data *bdg = GET_OLC_BUILDING(ch->desc);
	GET_BLD_BASE_AFFECTS(bdg) = olc_process_flag(ch, argument, "affect", "affects", room_aff_bits, GET_BLD_BASE_AFFECTS(bdg));
}


OLC_MODULE(bedit_artisan) {
	bld_data *bdg = GET_OLC_BUILDING(ch->desc);
	mob_vnum old = GET_BLD_ARTISAN(bdg);
	
	if (!str_cmp(argument, "none")) {
		GET_BLD_ARTISAN(bdg) = NOTHING;
		if (PRF_FLAGGED(ch, PRF_NOREPEAT)) {
			send_config_msg(ch, "ok_string");
		}
		else {
			msg_to_char(ch, "It no longer has an artisan.\r\n");
		}
	}
	else {
		GET_BLD_ARTISAN(bdg) = olc_process_number(ch, argument, "artisan vnum", "artisanvnum", 0, MAX_VNUM, GET_BLD_ARTISAN(bdg));
		if (!mob_proto(GET_BLD_ARTISAN(bdg))) {
			GET_BLD_ARTISAN(bdg) = old;
			msg_to_char(ch, "There is no mobile with that vnum. Old value restored.\r\n");
		}
		else if (!PRF_FLAGGED(ch, PRF_NOREPEAT)) {
			msg_to_char(ch, "It now has artisan: %s\r\n", get_mob_name_by_proto(GET_BLD_ARTISAN(bdg)));
		}
	}
}


OLC_MODULE(bedit_citizens) {
	bld_data *bdg = GET_OLC_BUILDING(ch->desc);
	GET_BLD_CITIZENS(bdg) = olc_process_number(ch, argument, "citizens", "citizens", 0, 10, GET_BLD_CITIZENS(bdg));
}


OLC_MODULE(bedit_commands) {
	bld_data *bdg = GET_OLC_BUILDING(ch->desc);
	if (!str_cmp(argument, "none")) {
		if (GET_BLD_COMMANDS(bdg)) {
			free(GET_BLD_COMMANDS(bdg));
		}
		GET_BLD_COMMANDS(bdg) = NULL;

		if (PRF_FLAGGED(ch, PRF_NOREPEAT)) {
			send_config_msg(ch, "ok_string");
		}
		else {
			msg_to_char(ch, "It no longer has commands.\r\n");
		}
	}
	else {
		olc_process_string(ch, argument, "commands", &GET_BLD_COMMANDS(bdg));
	}
}


OLC_MODULE(bedit_description) {
	bld_data *bdg = GET_OLC_BUILDING(ch->desc);

	if (ch->desc->str) {
		msg_to_char(ch, "You are already editing a string.\r\n");
	}
	else {
		sprintf(buf, "description for %s", GET_BLD_NAME(bdg));
		start_string_editor(ch->desc, buf, &(GET_BLD_DESC(bdg)), MAX_ROOM_DESCRIPTION, TRUE);
	}
}


OLC_MODULE(bedit_designate) {
	bld_data *bdg = GET_OLC_BUILDING(ch->desc);
	GET_BLD_DESIGNATE_FLAGS(bdg) = olc_process_flag(ch, argument, "designate", "designate", designate_flags, GET_BLD_DESIGNATE_FLAGS(bdg));
}


OLC_MODULE(bedit_extra_desc) {
	bld_data *bdg = GET_OLC_BUILDING(ch->desc);
	olc_process_extra_desc(ch, argument, &GET_BLD_EX_DESCS(bdg));
}


OLC_MODULE(bedit_extrarooms) {
	bld_data *bdg = GET_OLC_BUILDING(ch->desc);
	
	if (IS_SET(GET_BLD_FLAGS(bdg), BLD_ROOM)) {
		msg_to_char(ch, "You can't set that on a ROOM.\r\n");
	}
	else {
		GET_BLD_EXTRA_ROOMS(bdg) = olc_process_number(ch, argument, "extra rooms", "rooms", 0, 1000, GET_BLD_EXTRA_ROOMS(bdg));
	}
}


OLC_MODULE(bedit_fame) {
	bld_data *bdg = GET_OLC_BUILDING(ch->desc);
	GET_BLD_FAME(bdg) = olc_process_number(ch, argument, "fame", "fame", -1000, 1000, GET_BLD_FAME(bdg));
}


OLC_MODULE(bedit_flags) {
	bld_data *bdg = GET_OLC_BUILDING(ch->desc);
	GET_BLD_FLAGS(bdg) = olc_process_flag(ch, argument, "building", "flags", bld_flags, GET_BLD_FLAGS(bdg));
}


OLC_MODULE(bedit_functions) {
	bld_data *bdg = GET_OLC_BUILDING(ch->desc);
	GET_BLD_FUNCTIONS(bdg) = olc_process_flag(ch, argument, "function", "functions", function_flags, GET_BLD_FUNCTIONS(bdg));
}


OLC_MODULE(bedit_hitpoints) {
	bld_data *bdg = GET_OLC_BUILDING(ch->desc);
	
	if (IS_SET(GET_BLD_FLAGS(bdg), BLD_ROOM)) {
		msg_to_char(ch, "You can't set that on a ROOM.\r\n");
	}
	else {
		GET_BLD_MAX_DAMAGE(bdg) = olc_process_number(ch, argument, "hitpoints", "hitpoints", 1, 1000, GET_BLD_MAX_DAMAGE(bdg));
	}
}


OLC_MODULE(bedit_icon) {
	extern bool validate_icon(char *icon);

	bld_data *bdg = GET_OLC_BUILDING(ch->desc);
	
	delete_doubledollar(argument);
	
	if (IS_SET(GET_BLD_FLAGS(bdg), BLD_ROOM)) {
		msg_to_char(ch, "You can't set that on a ROOM.\r\n");
	}
	else if (!validate_icon(argument)) {
		msg_to_char(ch, "You must specify an icon that is 4 characters long, not counting color codes.\r\n");
	}
	else {
		olc_process_string(ch, argument, "icon", &GET_BLD_ICON(bdg));
	}
}


OLC_MODULE(bedit_interaction) {
	bld_data *bdg = GET_OLC_BUILDING(ch->desc);
	olc_process_interactions(ch, argument, &GET_BLD_INTERACTIONS(bdg), TYPE_ROOM);
}


OLC_MODULE(bedit_military) {
	bld_data *bdg = GET_OLC_BUILDING(ch->desc);
	GET_BLD_MILITARY(bdg) = olc_process_number(ch, argument, "military", "military", 0, 1000, GET_BLD_MILITARY(bdg));
}


OLC_MODULE(bedit_name) {
	bld_data *bdg = GET_OLC_BUILDING(ch->desc);
	olc_process_string(ch, argument, "name", &GET_BLD_NAME(bdg));
	CAP(GET_BLD_NAME(bdg));
}


OLC_MODULE(bedit_relations) {
	bld_data *bld = GET_OLC_BUILDING(ch->desc);
	struct bld_relation **list = &GET_BLD_RELATIONS(bld);
	char cmd_arg[MAX_INPUT_LENGTH], field_arg[MAX_INPUT_LENGTH];
	char num_arg[MAX_INPUT_LENGTH], type_arg[MAX_INPUT_LENGTH];
	char vnum_arg[MAX_INPUT_LENGTH], buf[MAX_STRING_LENGTH];
	struct bld_relation *relat, *iter, *change, *copyfrom;
	int findtype, num, rtype;
	any_vnum vnum;
	bool found;
	
	argument = any_one_arg(argument, cmd_arg);	// add/remove/change/copy
	
	if (is_abbrev(cmd_arg, "copy")) {
		// usage: qedit starts/ends copy <from type> <from vnum> <starts/ends>
		argument = any_one_arg(argument, type_arg);	// just "building" for now
		argument = any_one_arg(argument, vnum_arg);	// any vnum for that type
		argument = any_one_arg(argument, field_arg);	// starts/ends
		
		if (!*type_arg || !*vnum_arg) {
			msg_to_char(ch, "Usage: relation copy <from type> <from vnum> [starts | ends]\r\n");
		}
		else if ((findtype = find_olc_type(type_arg)) == 0) {
			msg_to_char(ch, "Unknown olc type '%s'.\r\n", type_arg);
		}
		else if (!isdigit(*vnum_arg)) {
			sprintbit(findtype, olc_type_bits, buf, FALSE);
			msg_to_char(ch, "Copy from which %s?\r\n", buf);
		}
		else if ((vnum = atoi(vnum_arg)) < 0) {
			msg_to_char(ch, "Invalid vnum.\r\n");
		}
		else {
			sprintbit(findtype, olc_type_bits, buf, FALSE);
			copyfrom = NULL;
			
			switch (findtype) {
				case OLC_BUILDING: {
					bld_data *from_bld = building_proto(vnum);
					if (from_bld) {
						copyfrom = GET_BLD_RELATIONS(from_bld);
					}
					break;
				}
				default: {
					msg_to_char(ch, "You can't copy relations from %ss.\r\n", buf);
					return;
				}
			}
			
			if (!copyfrom) {
				msg_to_char(ch, "Invalid %s vnum '%s'.\r\n", buf, vnum_arg);
			}
			else {
				smart_copy_bld_relations(list, copyfrom);
				msg_to_char(ch, "Copied relations from %s %d.\r\n", buf, vnum);
			}
		}
	}	// end 'copy'
	else if (is_abbrev(cmd_arg, "remove")) {
		// usage: qedit starts|ends remove <number | all>
		skip_spaces(&argument);	// only arg is number
		
		if (!*argument) {
			msg_to_char(ch, "Remove which relation (number)?\r\n");
		}
		else if (!str_cmp(argument, "all")) {
			free_bld_relations(*list);
			*list = NULL;
			msg_to_char(ch, "You remove all the relations.\r\n");
		}
		else if (!isdigit(*argument) || (num = atoi(argument)) < 1) {
			msg_to_char(ch, "Invalid relation number.\r\n");
		}
		else {
			found = FALSE;
			LL_FOREACH(*list, iter) {
				if (--num == 0) {
					found = TRUE;
					
					msg_to_char(ch, "You remove the relation info for %s %d.\r\n", bld_relationship_types[iter->type], iter->vnum);
					LL_DELETE(*list, iter);
					free(iter);
					break;
				}
			}
			
			if (!found) {
				msg_to_char(ch, "Invalid relation number.\r\n");
			}
		}
	}	// end 'remove'
	else if (is_abbrev(cmd_arg, "add")) {
		// usage: qedit starts|ends add <type> <vnum>
		argument = any_one_arg(argument, type_arg);
		argument = any_one_arg(argument, vnum_arg);
		
		if (!*type_arg || !*vnum_arg) {
			msg_to_char(ch, "Usage: relation add <type> <vnum>\r\n");
		}
		else if ((rtype = search_block(type_arg, bld_relationship_types, FALSE)) == NOTHING) {
			msg_to_char(ch, "Invalid type '%s'.\r\n", type_arg);
		}
		else if (!isdigit(*vnum_arg) || (vnum = atoi(vnum_arg)) < 0) {
			msg_to_char(ch, "Invalid vnum '%s'.\r\n", vnum_arg);
		}
		else if (!building_proto(vnum)) {
			msg_to_char(ch, "Unable to find building %d.\r\n", vnum);
			return;
		}
		else {
			// success
			CREATE(relat, struct bld_relation, 1);
			relat->type = rtype;
			relat->vnum = vnum;
			
			LL_APPEND(*list, relat);
			msg_to_char(ch, "You add %s relation: [%d] %s\r\n", bld_relationship_types[rtype], vnum, get_bld_name_by_proto(vnum));
		}
	}	// end 'add'
	else if (is_abbrev(cmd_arg, "change")) {
		// usage: qedit starts|ends change <number> vnum <number>
		argument = any_one_arg(argument, num_arg);
		argument = any_one_arg(argument, field_arg);
		argument = any_one_arg(argument, vnum_arg);
		
		if (!*num_arg || !isdigit(*num_arg) || !*field_arg || !*vnum_arg) {
			msg_to_char(ch, "Usage: relation change <number> vnum <value>\r\n");
			return;
		}
		
		// find which one to change
		num = atoi(num_arg);
		change = NULL;
		LL_FOREACH(*list, iter) {
			if (--num == 0) {
				change = iter;
				break;
			}
		}
		
		if (!change) {
			msg_to_char(ch, "Invalid relation number.\r\n");
		}
		else if (is_abbrev(field_arg, "vnum")) {
			if (!isdigit(*vnum_arg) || (vnum = atoi(vnum_arg)) < 0) {
				msg_to_char(ch, "Invalid vnum '%s'.\r\n", vnum_arg);
				return;
			}
			if (!building_proto(vnum)) {
				msg_to_char(ch, "Unable to find building %d.\r\n", vnum);
				return;
			}
			
			change->vnum = vnum;
			msg_to_char(ch, "Changed relation %d to: [%d] %s\r\n", atoi(num_arg), vnum, get_bld_name_by_proto(vnum));
		}
		else {
			msg_to_char(ch, "You can only change the vnum.\r\n");
		}
	}	// end 'change'
	else {
		msg_to_char(ch, "Usage: relation add <type> <vnum>\r\n");
		msg_to_char(ch, "Usage: relation change <number> vnum <value>\r\n");
		msg_to_char(ch, "Usage: relation copy <from type> <from vnum> [starts/ends]\r\n");
		msg_to_char(ch, "Usage: relation remove <number | all>\r\n");
	}
}


OLC_MODULE(bedit_resource) {
	void olc_process_resources(char_data *ch, char *argument, struct resource_data **list);
	bld_data *bdg = GET_OLC_BUILDING(ch->desc);
	olc_process_resources(ch, argument, &GET_BLD_YEARLY_MAINTENANCE(bdg));
}


OLC_MODULE(bedit_script) {
	bld_data *bdg = GET_OLC_BUILDING(ch->desc);
	olc_process_script(ch, argument, &(GET_BLD_SCRIPTS(bdg)), WLD_TRIGGER);
}


OLC_MODULE(bedit_spawns) {
	bld_data *bdg = GET_OLC_BUILDING(ch->desc);
	olc_process_spawns(ch, argument, &GET_BLD_SPAWNS(bdg));
}


OLC_MODULE(bedit_title) {
	bld_data *bdg = GET_OLC_BUILDING(ch->desc);
	olc_process_string(ch, argument, "title", &GET_BLD_TITLE(bdg));
	CAP(GET_BLD_TITLE(bdg));
}
