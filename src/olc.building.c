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
extern const char *designate_flags[];
extern const char *function_flags[];
extern const char *interact_types[];
extern const byte interact_vnum_types[NUM_INTERACTS];
extern const char *room_aff_bits[];
extern const char *spawn_flags[];

// external funcs
void init_building(bld_data *building);
void replace_question_color(char *input, char *color, char *output);
void sort_interactions(struct interaction_item **list);


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
	
	if (!str_cmp(GET_BLD_NAME(bld), "Unnamed Building")) {
		olc_audit_msg(ch, GET_BLD_VNUM(bld), "Name not set");
		problem = TRUE;
	}
	if (!str_cmp(GET_BLD_TITLE(bld), "An Unnamed Building")) {
		olc_audit_msg(ch, GET_BLD_VNUM(bld), "Title not set");
		problem = TRUE;
	}
	if (!IS_SET(GET_BLD_FLAGS(bld), BLD_ROOM) && !str_cmp(GET_BLD_ICON(bld), "&0[  ]")) {
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
	vehicle_data *veh, *next_veh;
	room_data *room, *next_room;
	social_data *soc, *next_soc;
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
		if (GET_BLD_UPGRADES_TO(biter) == vnum) {
			GET_BLD_UPGRADES_TO(biter) = NOTHING;
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
			if (GET_BLD_UPGRADES_TO(GET_OLC_BUILDING(desc)) == vnum) {
				GET_BLD_UPGRADES_TO(GET_OLC_BUILDING(desc)) = NOTHING;
				msg_to_desc(desc, "The upgrades-to building for the building you're editing was deleted.\r\n");
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
	vehicle_data *veh, *next_veh;
	social_data *soc, *next_soc;
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
		if (GET_BLD_UPGRADES_TO(bld) == vnum) {
			++found;
			size += snprintf(buf + size, sizeof(buf) - size, "BLD [%5d] %s\r\n", GET_BLD_VNUM(bld), GET_BLD_NAME(bld));
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
	struct interaction_item *interact;
	struct trig_proto_list *trig;
	struct spawn_info *spawn;
	struct quest_lookup *ql;
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
	while ((interact = GET_BLD_INTERACTIONS(proto))) {
		GET_BLD_INTERACTIONS(proto) = interact->next;
		free(interact);
	}
	while ((trig = GET_BLD_SCRIPTS(proto))) {
		GET_BLD_SCRIPTS(proto) = trig->next;
		free(trig);
	}
	if (GET_BLD_YEARLY_MAINTENANCE(proto)) {
		free_resource_list(GET_BLD_YEARLY_MAINTENANCE(proto));
	}
	
	// sanity
	prune_extra_descs(&GET_BLD_EX_DESCS(bdg));
	if (!GET_BLD_NAME(bdg) || !*GET_BLD_NAME(bdg)) {
		if (GET_BLD_NAME(bdg)) {
			free(GET_BLD_NAME(bdg));
		}
		GET_BLD_NAME(bdg) = str_dup("unnamed building");
	}
	if (!GET_BLD_TITLE(bdg) || !*GET_BLD_TITLE(bdg)) {
		if (GET_BLD_TITLE(bdg)) {
			free(GET_BLD_TITLE(bdg));
		}
		GET_BLD_TITLE(bdg) = str_dup("An Unnamed Building");
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
	
	*proto = *bdg;	// copy
	proto->vnum = vnum;	// ensure correct vnum
	
	proto->hh = hh;	// restore hash handle
	proto->quest_lookups = ql;	// restore lookups
	
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
		GET_BLD_NAME(new) = str_dup("Unnamed Building");
		GET_BLD_TITLE(new) = str_dup("An Unnamed Building");
		GET_BLD_ICON(new) = str_dup("&0[  ]");
		GET_BLD_MAX_DAMAGE(new) = 1;
	}
	
	// done
	return new;	
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
	char lbuf[MAX_STRING_LENGTH];
	bool is_room = IS_SET(GET_BLD_FLAGS(bdg), BLD_ROOM) ? TRUE : FALSE;
	struct spawn_info *spawn;
	int count;
	
	if (!bdg) {
		return;
	}
	
	*buf = '\0';
	sprintf(buf + strlen(buf), "[&c%d&0] &c%s&0\r\n", GET_OLC_VNUM(ch->desc), !building_proto(GET_BLD_VNUM(bdg)) ? "new building" : GET_BLD_NAME(building_proto(GET_BLD_VNUM(bdg))));
	sprintf(buf + strlen(buf), "<&yname&0> %s\r\n", GET_BLD_NAME(bdg));
	sprintf(buf + strlen(buf), "<&ytitle&0> %s\r\n", GET_BLD_TITLE(bdg));
	
	if (!is_room) {
		replace_question_color(NULLSAFE(GET_BLD_ICON(bdg)), "&0", lbuf);
		sprintf(buf + strlen(buf), "<&yicon&0> %s&0  %s\r\n", lbuf, show_color_codes(NULLSAFE(GET_BLD_ICON(bdg))));
	}
	sprintf(buf + strlen(buf), "<&ycommands&0> %s\r\n", GET_BLD_COMMANDS(bdg) ? GET_BLD_COMMANDS(bdg) : "");
	sprintf(buf + strlen(buf), "<&ydescription&0>\r\n%s", GET_BLD_DESC(bdg) ? GET_BLD_DESC(bdg) : "");

	if (!is_room) {
		sprintf(buf + strlen(buf), "<&yhitpoints&0> %d\r\n", GET_BLD_MAX_DAMAGE(bdg));
		sprintf(buf + strlen(buf), "<&yfame&0> %d\r\n", GET_BLD_FAME(bdg));
		sprintf(buf + strlen(buf), "<&yrooms&0> %d\r\n", GET_BLD_EXTRA_ROOMS(bdg));
	}
	
	sprintf(buf + strlen(buf), "<&ycitizens&0> %d\r\n", GET_BLD_CITIZENS(bdg));
	
	if (!is_room) {
		sprintf(buf + strlen(buf), "<&ymilitary&0> %d\r\n", GET_BLD_MILITARY(bdg));
	}
	
	sprintf(buf + strlen(buf), "<&yartisan&0> [%d] %s\r\n", GET_BLD_ARTISAN(bdg), GET_BLD_ARTISAN(bdg) == NOTHING ? "none" : get_mob_name_by_proto(GET_BLD_ARTISAN(bdg)));
	
	sprintbit(GET_BLD_FLAGS(bdg), bld_flags, lbuf, TRUE);
	sprintf(buf + strlen(buf), "<&yflags&0> %s\r\n", lbuf);
	
	sprintbit(GET_BLD_FUNCTIONS(bdg), function_flags, lbuf, TRUE);
	sprintf(buf + strlen(buf), "<&yfunctions&0> %s\r\n", lbuf);
	
	sprintbit(GET_BLD_DESIGNATE_FLAGS(bdg), designate_flags, lbuf, TRUE);
	sprintf(buf + strlen(buf), "<&ydesignate&0> %s\r\n", lbuf);
	
	sprintbit(GET_BLD_BASE_AFFECTS(bdg), room_aff_bits, lbuf, TRUE);
	sprintf(buf + strlen(buf), "<&yaffects&0> %s\r\n", lbuf);
	
	if (!is_room) {
		sprintf(buf + strlen(buf), "<&yupgradesto&0> [%d] %s\r\n", GET_BLD_UPGRADES_TO(bdg), GET_BLD_UPGRADES_TO(bdg) == NOTHING ? "none" : GET_BLD_NAME(building_proto(GET_BLD_UPGRADES_TO(bdg))));
	}

	// exdesc
	sprintf(buf + strlen(buf), "Extra descriptions: <&yextra&0>\r\n");
	if (GET_BLD_EX_DESCS(bdg)) {
		get_extra_desc_display(GET_BLD_EX_DESCS(bdg), buf1);
		strcat(buf, buf1);
	}

	sprintf(buf + strlen(buf), "Interactions: <&yinteraction&0>\r\n");
	if (GET_BLD_INTERACTIONS(bdg)) {
		get_interaction_display(GET_BLD_INTERACTIONS(bdg), buf1);
		strcat(buf, buf1);
	}
	
	// maintenance resources
	sprintf(buf + strlen(buf), "Yearly maintenance resources required: <\tyresource\t0>\r\n");
	if (GET_BLD_YEARLY_MAINTENANCE(bdg)) {
		get_resource_display(GET_BLD_YEARLY_MAINTENANCE(bdg), buf1);
		strcat(buf, buf1);
	}

	// scripts
	sprintf(buf + strlen(buf), "Scripts: <&yscript&0>\r\n");
	if (GET_BLD_SCRIPTS(bdg)) {
		get_script_display(GET_BLD_SCRIPTS(bdg), lbuf);
		strcat(buf, lbuf);
	}
	
	sprintf(buf + strlen(buf), "<&yspawns&0>\r\n");
	if (GET_BLD_SPAWNS(bdg)) {
		count = 0;
		for (spawn = GET_BLD_SPAWNS(bdg); spawn; spawn = spawn->next) {
			++count;
		}
		sprintf(buf + strlen(buf), " %d spawn%s set\r\n", count, PLURAL(count));
	}
		
	page_string(ch->desc, buf, TRUE);
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
	
	if (IS_SET(GET_BLD_FLAGS(bdg), BLD_ROOM)) {
		msg_to_char(ch, "You can't set that on a ROOM.\r\n");
	}
	else {
		GET_BLD_FAME(bdg) = olc_process_number(ch, argument, "fame", "fame", -1000, 1000, GET_BLD_FAME(bdg));
	}
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
	
	if (IS_SET(GET_BLD_FLAGS(bdg), BLD_ROOM)) {
		msg_to_char(ch, "You can't set that on a ROOM.\r\n");
	}
	else {
		GET_BLD_MILITARY(bdg) = olc_process_number(ch, argument, "military", "military", 0, 1000, GET_BLD_MILITARY(bdg));
	}
}


OLC_MODULE(bedit_name) {
	bld_data *bdg = GET_OLC_BUILDING(ch->desc);
	olc_process_string(ch, argument, "name", &GET_BLD_NAME(bdg));
	CAP(GET_BLD_NAME(bdg));
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


OLC_MODULE(bedit_upgradesto) {
	bld_data *bdg = GET_OLC_BUILDING(ch->desc);
	bld_vnum old = GET_BLD_UPGRADES_TO(bdg);
	
	if (IS_SET(GET_BLD_FLAGS(bdg), BLD_ROOM)) {
		msg_to_char(ch, "You can't set that on a ROOM.\r\n");
	}
	else if (!str_cmp(argument, "none")) {
		GET_BLD_UPGRADES_TO(bdg) = NOTHING;
		if (PRF_FLAGGED(ch, PRF_NOREPEAT)) {
			send_config_msg(ch, "ok_string");
		}
		else {
			msg_to_char(ch, "It no longer upgrades to anything.\r\n");
		}
	}
	else {
		GET_BLD_UPGRADES_TO(bdg) = olc_process_number(ch, argument, "upgrade building", "upgradesto", 0, MAX_VNUM, GET_BLD_UPGRADES_TO(bdg));
		if (!building_proto(GET_BLD_UPGRADES_TO(bdg))) {
			GET_BLD_UPGRADES_TO(bdg) = old;
			msg_to_char(ch, "There is no building with that vnum. Old value restored.\r\n");
		}
		else if (!PRF_FLAGGED(ch, PRF_NOREPEAT)) {
			msg_to_char(ch, "It now upgrades to: %s\r\n", GET_BLD_NAME(building_proto(GET_BLD_UPGRADES_TO(bdg))));
		}
	}
}
