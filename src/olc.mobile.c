/* ************************************************************************
*   File: olc.mobile.c                                    EmpireMUD 2.0b5 *
*  Usage: OLC for mobs                                                    *
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
extern const char *genders[];
extern const char *interact_types[];
extern const byte interact_vnum_types[NUM_INTERACTS];
extern const char *mob_custom_types[];
extern const char *mob_move_types[];
extern const char *name_sets[];
extern const char *size_types[];

// external funcs
extern char **get_weapon_types_string();

// locals
const char *default_mob_keywords = "mobile new";
const char *default_mob_short = "a new mobile";
const char *default_mob_long = "A new mobile is standing here.\r\n";



 //////////////////////////////////////////////////////////////////////////////
//// HELPERS /////////////////////////////////////////////////////////////////

/**
* Checks for common mob problems and reports them to ch.
*
* @param char_data *mob The mob to audit.
* @param char_data *ch The person to report to.
* @return bool TRUE if any problems were reported; FALSE if all good.
*/
bool audit_mobile(char_data *mob, char_data *ch) {
	extern bool audit_interactions(any_vnum vnum, struct interaction_item *list, int attach_type, char_data *ch);
	extern adv_data *get_adventure_for_vnum(rmt_vnum vnum);
	
	bool is_adventure = (get_adventure_for_vnum(GET_MOB_VNUM(mob)) != NULL);
	char temp[MAX_STRING_LENGTH], *ptr;
	bool problem = FALSE;

	if (!str_cmp(GET_PC_NAME(mob), default_mob_keywords)) {
		olc_audit_msg(ch, GET_MOB_VNUM(mob), "Keywords not set");
		problem = TRUE;
	}
	
	ptr = GET_PC_NAME(mob);
	do {
		ptr = any_one_arg(ptr, temp);
		if (*temp && !str_str(GET_SHORT_DESC(mob), temp) && !str_str(GET_LONG_DESC(mob), temp)) {
			olc_audit_msg(ch, GET_MOB_VNUM(mob), "Keyword '%s' not found in strings", temp);
			problem = TRUE;
		}
	} while (*ptr);
	
	if (!str_cmp(GET_SHORT_DESC(mob), default_mob_short)) {
		olc_audit_msg(ch, GET_MOB_VNUM(mob), "Short desc not set");
		problem = TRUE;
	}
	any_one_arg(GET_SHORT_DESC(mob), temp);
	if ((fill_word(temp) || reserved_word(temp)) && isupper(*temp)) {
		olc_audit_msg(ch, GET_MOB_VNUM(mob), "Short desc capitalized");
		problem = TRUE;
	}
	if (ispunct(GET_SHORT_DESC(mob)[strlen(GET_SHORT_DESC(mob)) - 1])) {
		olc_audit_msg(ch, GET_MOB_VNUM(mob), "Short desc has punctuation");
		problem = TRUE;
	}
	
	ptr = GET_SHORT_DESC(mob);
	do {
		ptr = any_one_arg(ptr, temp);
		// remove trailing punctuation
		while (*temp && ispunct(temp[strlen(temp)-1])) {
			temp[strlen(temp)-1] = '\0';
		}
		if (*temp && !fill_word(temp) && !reserved_word(temp) && !isname(temp, GET_PC_NAME(mob))) {
			olc_audit_msg(ch, GET_MOB_VNUM(mob), "Suggested missing keyword '%s'", temp);
			problem = TRUE;
		}
	} while (*ptr);
	
	if (!str_cmp(GET_LONG_DESC(mob), default_mob_long)) {
		olc_audit_msg(ch, GET_MOB_VNUM(mob), "Long desc not set");
		problem = TRUE;
	}
	if (!ispunct(GET_LONG_DESC(mob)[strlen(GET_LONG_DESC(mob)) - 3])) {
		olc_audit_msg(ch, GET_MOB_VNUM(mob), "Long desc missing punctuation");
		problem = TRUE;
	}
	if (islower(*GET_LONG_DESC(mob))) {
		olc_audit_msg(ch, GET_MOB_VNUM(mob), "Long desc not capitalized");
		problem = TRUE;
	}
	
	if (!GET_LOOK_DESC(mob) || !*GET_LOOK_DESC(mob)) {
		olc_audit_msg(ch, GET_MOB_VNUM(mob), "No look description");
		problem = TRUE;
	}
	
	if (!is_adventure && GET_MAX_SCALE_LEVEL(mob) == 0) {
		olc_audit_msg(ch, GET_MOB_VNUM(mob), "No maximum scale level on non-adventure mob");
		problem = TRUE;
	}
	if (MOB_ATTACK_TYPE(mob) == TYPE_RESERVED) {
		olc_audit_msg(ch, GET_MOB_VNUM(mob), "Invalid attack type");
		problem = TRUE;
	}
	if (MOB_FLAGGED(mob, MOB_ANIMAL) && !has_interaction(mob->interactions, INTERACT_SKIN)) {
		olc_audit_msg(ch, GET_MOB_VNUM(mob), "Animal has no skin");
		problem = TRUE;
	}
	if (MOB_FLAGGED(mob, MOB_ANIMAL) && !has_interaction(mob->interactions, INTERACT_BUTCHER)) {
		olc_audit_msg(ch, GET_MOB_VNUM(mob), "Animal can't be butchered");
		problem = TRUE;
	}
	
	problem |= audit_interactions(GET_MOB_VNUM(mob), mob->interactions, TYPE_MOB, ch);
	
	return problem;
}

/**
* Creates a new mob entry.
*
* @param mob_vnum vnum The number to create.
* @return char_data* The new mob's prototype.
*/
char_data *create_mob_table_entry(mob_vnum vnum) {
	void add_mobile_to_table(char_data *mob);
	
	char_data *mob;
	
	// sanity
	if (mob_proto(vnum)) {
		log("SYSERR: Attempting to insert mobile at existing vnum %d", vnum);
		return mob_proto(vnum);
	}
	
	CREATE(mob, char_data, 1);
	clear_char(mob);
	mob->vnum = vnum;
	SET_BIT(MOB_FLAGS(mob), MOB_ISNPC);	// need this for some macroes
	add_mobile_to_table(mob);

	// save mob index and mob file now so there's no trouble later
	save_index(DB_BOOT_MOB);
	save_library_file_for_vnum(DB_BOOT_MOB, vnum);

	return mob;
}


/**
* Deletes one interaction from a list, based on vnum type.
*
* @param struct interaction_item **list The interaction list to check.
* @param int vnum_type TYPE_OBJ or TYPE_MOB
* @param any_vnum vnum The vnum to delete.
* @return bool TRUE if at least 1 item was deleted, or FALSE
*/
bool delete_from_interaction_list(struct interaction_item **list, int vnum_type, any_vnum vnum) {
	struct interaction_item *inter, *next_inter, *temp;
	bool found = FALSE;
	
	for (inter = *list; inter; inter = next_inter) {
		next_inter = inter->next;
		
		// deleted!
		if (interact_vnum_types[inter->type] == vnum_type && inter->vnum == vnum) {
			found = TRUE;
			REMOVE_FROM_LIST(inter, *list, next);
			inter->next = NULL;
			free_interactions(&inter);
		}
	}
	
	return found;
}


/**
* Deletes a mob from a single spawn list, if it's in there. This is only for
* live spawn lists, not spawn templates (in room templates).
* 
* @param struct spawn_info **list A pointer to the start of a spawn_info list.
* @param mob_vnum vnum The mob to remove.
* @return bool TRUE if any spawn entries were deleted; FALSE otherwise.
*/
bool delete_mob_from_spawn_list(struct spawn_info **list, mob_vnum vnum) {
	struct spawn_info *spawn, *next_spawn, *temp;
	bool found = FALSE;
	
	for (spawn = *list; spawn; spawn = next_spawn) {
		next_spawn = spawn->next;
		
		// deleted!
		if (spawn->vnum == vnum) {
			found = TRUE;
			REMOVE_FROM_LIST(spawn, *list, next);
			free(spawn);
		}
	}
	
	return found;
}


/**
* Deletes a mob from a single adventure room template's spawn list. Not to be
* confused with live spawn lists (building, sector, etc).
* 
* @param struct adventure_spawn **list A pointer to the start of a spawn_info list.
* @param int spawn_type Any ADV_SPAWN_.
* @param mob_vnum vnum The mob to remove.
* @return bool TRUE if any spawn entries were deleted; FALSE otherwise.
*/
bool delete_from_spawn_template_list(struct adventure_spawn **list, int spawn_type, mob_vnum vnum) {
	struct adventure_spawn *spawn, *next_spawn, *temp;
	bool found = FALSE;
	
	for (spawn = *list; spawn; spawn = next_spawn) {
		next_spawn = spawn->next;
		
		// deleted!
		if (spawn->type == spawn_type && spawn->vnum == vnum) {
			found = TRUE;
			REMOVE_FROM_LIST(spawn, *list, next);
			free(spawn);
		}
	}
	
	return found;
}


/**
* For the .list command.
*
* @param char_data *mob The thing to list.
* @param bool detail If TRUE, provide additional details
* @return char* The line to show (without a CRLF).
*/
char *list_one_mobile(char_data *mob, bool detail) {
	static char output[MAX_STRING_LENGTH];
	char flags[MAX_STRING_LENGTH];
	
	bitvector_t show_flags = MOB_BRING_A_FRIEND | MOB_SENTINEL | MOB_AGGRESSIVE | MOB_MOUNTABLE | MOB_ANIMAL | MOB_AQUATIC | MOB_NO_ATTACK | MOB_SPAWNED | MOB_CHAMPION | MOB_FAMILIAR | MOB_EMPIRE | MOB_CITYGUARD | MOB_GROUP | MOB_HARD | MOB_DPS | MOB_TANK | MOB_CASTER | MOB_VAMPIRE | MOB_HUMAN;
	
	if (detail) {
		if (IS_SET(MOB_FLAGS(mob), show_flags)) {
			sprintbit(MOB_FLAGS(mob) & show_flags, action_bits, flags, TRUE);
		}
		else {
			*flags = '\0';
		}
		
		snprintf(output, sizeof(output), "[%5d] %s (%s) %s", GET_MOB_VNUM(mob), GET_SHORT_DESC(mob), level_range_string(GET_MIN_SCALE_LEVEL(mob), GET_MAX_SCALE_LEVEL(mob), 0), flags);
	}
	else {
		snprintf(output, sizeof(output), "[%5d] %s", GET_MOB_VNUM(mob), GET_SHORT_DESC(mob));
	}
	
	return output;
}


/**
* WARNING: This function actually deletes a mobile from the world.
*
* @param char_data *ch The person doing the deleting.
* @param mob_vnum vnum The vnum to delete.
*/
void olc_delete_mobile(char_data *ch, mob_vnum vnum) {
	extern bool delete_quest_giver_from_list(struct quest_giver **list, int type, any_vnum vnum);
	extern bool delete_quest_reward_from_list(struct quest_reward **list, int type, any_vnum vnum);
	extern bool delete_requirement_from_list(struct req_data **list, int type, any_vnum vnum);
	
	void extract_pending_chars();
	void remove_mobile_from_table(char_data *mob);
	
	char_data *proto, *mob_iter, *next_mob;
	descriptor_data *desc;
	struct global_data *glb, *next_glb;
	quest_data *quest, *next_quest;
	progress_data *prg, *next_prg;
	room_template *rmt, *next_rmt;
	vehicle_data *veh, *next_veh;
	sector_data *sect, *next_sect;
	crop_data *crop, *next_crop;
	shop_data *shop, *next_shop;
	social_data *soc, *next_soc;
	bld_data *bld, *next_bld;
	struct mount_data *mount;
	bool found;
	
	if (!(proto = mob_proto(vnum))) {
		msg_to_char(ch, "There is no such mobile %d.\r\n", vnum);
		return;
	}
	
	if (HASH_COUNT(mobile_table) <= 1) {
		msg_to_char(ch, "You can't delete the last mob.\r\n");
		return;
	}
		
	// remove mobs and mounts from the list: DO THIS FIRST
	for (mob_iter = character_list; mob_iter; mob_iter = next_mob) {
		next_mob = mob_iter->next;
		
		if (IS_NPC(mob_iter)) {
			if (GET_MOB_VNUM(mob_iter) == vnum) {
				// this is the removed mob
				act("$n has been deleted.", FALSE, mob_iter, NULL, NULL, TO_ROOM);
				extract_char(mob_iter);
			}
		}
		else {	// player
			if (GET_MOUNT_VNUM(mob_iter) == vnum) {
				if (IS_RIDING(mob_iter)) {
					msg_to_char(mob_iter, "Your mount has been deleted.\r\n");
					perform_dismount(mob_iter);
					GET_MOUNT_VNUM(mob_iter) = NOTHING;
					GET_MOUNT_FLAGS(mob_iter) = NOBITS;
				}
				if ((mount = find_mount_data(mob_iter, vnum))) {
					HASH_DEL(GET_MOUNT_LIST(mob_iter), mount);
					free(mount);
				}
			}
		}
	}
	
	// their data will already be free, so we need to clear them out now
	extract_pending_chars();
	
	// pull from hash ONLY after removing from the world
	remove_mobile_from_table(proto);

	// save mob index and mob file now so there's no trouble later
	save_index(DB_BOOT_MOB);
	save_library_file_for_vnum(DB_BOOT_MOB, vnum);
	
	// update buildings
	HASH_ITER(hh, building_table, bld, next_bld) {
		found = delete_mob_from_spawn_list(&GET_BLD_SPAWNS(bld), vnum);
		found |= delete_from_interaction_list(&GET_BLD_INTERACTIONS(bld), TYPE_MOB, vnum);
		if (GET_BLD_ARTISAN(bld) == vnum) {
			GET_BLD_ARTISAN(bld) = NOTHING;
			found |= TRUE;
		}
		if (found) {
			save_library_file_for_vnum(DB_BOOT_BLD, GET_BLD_VNUM(bld));
		}
	}
	
	// update crops
	HASH_ITER(hh, crop_table, crop, next_crop) {
		found = delete_mob_from_spawn_list(&GET_CROP_SPAWNS(crop), vnum);
		found |= delete_from_interaction_list(&GET_CROP_INTERACTIONS(crop), TYPE_MOB, vnum);
		if (found) {
			save_library_file_for_vnum(DB_BOOT_CROP, GET_CROP_VNUM(crop));
		}
	}
	
	// update globals
	HASH_ITER(hh, globals_table, glb, next_glb) {
		found = delete_from_interaction_list(&GET_GLOBAL_INTERACTIONS(glb), TYPE_MOB, vnum);
		if (found) {
			save_library_file_for_vnum(DB_BOOT_GLB, GET_GLOBAL_VNUM(glb));
		}
	}
	
	// update mob interactions
	HASH_ITER(hh, mobile_table, mob_iter, next_mob) {
		if (delete_from_interaction_list(&mob_iter->interactions, TYPE_MOB, vnum)) {
			save_library_file_for_vnum(DB_BOOT_MOB, mob_iter->vnum);
		}
	}
	
	// update progress
	HASH_ITER(hh, progress_table, prg, next_prg) {
		found = delete_requirement_from_list(&PRG_TASKS(prg), REQ_KILL_MOB, vnum);
		
		if (found) {
			SET_BIT(PRG_FLAGS(prg), PRG_IN_DEVELOPMENT);
			save_library_file_for_vnum(DB_BOOT_PRG, PRG_VNUM(prg));
			need_progress_refresh = TRUE;
		}
	}
	
	// update quests
	HASH_ITER(hh, quest_table, quest, next_quest) {
		found = delete_quest_giver_from_list(&QUEST_STARTS_AT(quest), QG_MOBILE, vnum);
		found |= delete_quest_giver_from_list(&QUEST_ENDS_AT(quest), QG_MOBILE, vnum);
		found |= delete_requirement_from_list(&QUEST_TASKS(quest), REQ_KILL_MOB, vnum);
		found |= delete_requirement_from_list(&QUEST_PREREQS(quest), REQ_KILL_MOB, vnum);
		
		if (found) {
			SET_BIT(QUEST_FLAGS(quest), QST_IN_DEVELOPMENT);
			save_library_file_for_vnum(DB_BOOT_QST, QUEST_VNUM(quest));
		}
	}
	
	// update room templates
	HASH_ITER(hh, room_template_table, rmt, next_rmt) {
		found = delete_from_spawn_template_list(&GET_RMT_SPAWNS(rmt), ADV_SPAWN_MOB, vnum);
		found |= delete_from_interaction_list(&GET_RMT_INTERACTIONS(rmt), TYPE_MOB, vnum);
		if (found) {
			save_library_file_for_vnum(DB_BOOT_RMT, GET_RMT_VNUM(rmt));
		}
	}
	
	// update sects
	HASH_ITER(hh, sector_table, sect, next_sect) {
		found = delete_mob_from_spawn_list(&GET_SECT_SPAWNS(sect), vnum);
		found |= delete_from_interaction_list(&GET_SECT_INTERACTIONS(sect), TYPE_MOB, vnum);
		if (found) {
			save_library_file_for_vnum(DB_BOOT_SECTOR, GET_SECT_VNUM(sect));
		}
	}
	
	// update shops
	HASH_ITER(hh, shop_table, shop, next_shop) {
		found = delete_quest_giver_from_list(&SHOP_LOCATIONS(shop), QG_MOBILE, vnum);
		
		if (found) {
			SET_BIT(SHOP_FLAGS(shop), SHOP_IN_DEVELOPMENT);
			save_library_file_for_vnum(DB_BOOT_SHOP, SHOP_VNUM(shop));
		}
	}
	
	// update socials
	HASH_ITER(hh, social_table, soc, next_soc) {
		found = delete_requirement_from_list(&SOC_REQUIREMENTS(soc), REQ_KILL_MOB, vnum);
		
		if (found) {
			SET_BIT(SOC_FLAGS(soc), SOC_IN_DEVELOPMENT);
			save_library_file_for_vnum(DB_BOOT_SOC, SOC_VNUM(soc));
		}
	}
	
	// update vehicles
	HASH_ITER(hh, vehicle_table, veh, next_veh) {
		found = delete_mob_from_spawn_list(&VEH_SPAWNS(veh), vnum);
		found |= delete_from_interaction_list(&VEH_INTERACTIONS(veh), TYPE_MOB, vnum);
		if (found) {
			save_library_file_for_vnum(DB_BOOT_VEH, VEH_VNUM(veh));
		}
	}
	
	// remove spawn locations and interactions from active editors
	for (desc = descriptor_list; desc; desc = desc->next) {
		if (GET_OLC_BUILDING(desc)) {
			if (GET_BLD_ARTISAN(GET_OLC_BUILDING(desc)) == vnum) {
				GET_BLD_ARTISAN(GET_OLC_BUILDING(desc)) = NOTHING;
				msg_to_char(desc->character, "The artisan mob for the building you're editing was deleted.\r\n");
			}
			if (delete_mob_from_spawn_list(&GET_OLC_BUILDING(desc)->spawns, vnum)) {
				msg_to_char(desc->character, "One of the mobs that spawns in the building you're editing was deleted.\r\n");
			}
			if (delete_from_interaction_list(&GET_OLC_BUILDING(desc)->interactions, TYPE_MOB, vnum)) {
				msg_to_char(desc->character, "One of the mobs in an interaction for the building you're editing was deleted.\r\n");
			}
		}
		if (GET_OLC_CROP(desc)) {
			if (delete_mob_from_spawn_list(&GET_OLC_CROP(desc)->spawns, vnum)) {
				msg_to_char(desc->character, "One of the mobs that spawns in the crop you're editing was deleted.\r\n");
			}
			if (delete_from_interaction_list(&GET_OLC_CROP(desc)->interactions, TYPE_MOB, vnum)) {
				msg_to_char(desc->character, "One of the mobs in an interaction for the crop you're editing was deleted.\r\n");
			}
		}
		if (GET_OLC_MOBILE(desc)) {
			if (delete_from_interaction_list(&GET_OLC_MOBILE(desc)->interactions, TYPE_MOB, vnum)) {
				msg_to_char(desc->character, "One of the mobs in an interaction for the mob you're editing was deleted.\r\n");
			}
		}
		if (GET_OLC_PROGRESS(desc)) {
			found = delete_requirement_from_list(&PRG_TASKS(GET_OLC_PROGRESS(desc)), REQ_KILL_MOB, vnum);
		
			if (found) {
				SET_BIT(QUEST_FLAGS(GET_OLC_PROGRESS(desc)), PRG_IN_DEVELOPMENT);
				msg_to_desc(desc, "A mobile used by the progression goal you're editing has been deleted.\r\n");
			}
		}
		if (GET_OLC_QUEST(desc)) {
			found = delete_quest_giver_from_list(&QUEST_STARTS_AT(GET_OLC_QUEST(desc)), QG_MOBILE, vnum);
			found |= delete_quest_giver_from_list(&QUEST_ENDS_AT(GET_OLC_QUEST(desc)), QG_MOBILE, vnum);
			found |= delete_requirement_from_list(&QUEST_TASKS(GET_OLC_QUEST(desc)), REQ_KILL_MOB, vnum);
			found |= delete_requirement_from_list(&QUEST_PREREQS(GET_OLC_QUEST(desc)), REQ_KILL_MOB, vnum);
			
			if (found) {
				SET_BIT(QUEST_FLAGS(GET_OLC_QUEST(desc)), QST_IN_DEVELOPMENT);
				msg_to_desc(desc, "A mobile used by the quest you are editing was deleted.\r\n");
			}
		}
		if (GET_OLC_ROOM_TEMPLATE(desc)) {
			if (delete_from_spawn_template_list(&GET_OLC_ROOM_TEMPLATE(desc)->spawns, ADV_SPAWN_MOB, vnum)) {
				msg_to_char(desc->character, "One of the mobs that spawns in the room template you're editing was deleted.\r\n");
			}
			if (delete_from_interaction_list(&GET_OLC_ROOM_TEMPLATE(desc)->interactions, TYPE_MOB, vnum)) {
				msg_to_char(desc->character, "One of the mobs in an interaction for the room template you're editing was deleted.\r\n");
			}
		}
		if (GET_OLC_SHOP(desc)) {
			found = delete_quest_giver_from_list(&SHOP_LOCATIONS(GET_OLC_SHOP(desc)), QG_MOBILE, vnum);
			
			if (found) {
				SET_BIT(SHOP_FLAGS(GET_OLC_SHOP(desc)), SHOP_IN_DEVELOPMENT);
				msg_to_desc(desc, "A mobile used by the shop you are editing was deleted.\r\n");
			}
		}
		if (GET_OLC_SECTOR(desc)) {
			if (delete_mob_from_spawn_list(&GET_OLC_SECTOR(desc)->spawns, vnum)) {
				msg_to_char(desc->character, "One of the mobs that spawns in the sector you're editing was deleted.\r\n");
			}
			if (delete_from_interaction_list(&GET_OLC_SECTOR(desc)->interactions, TYPE_MOB, vnum)) {
				msg_to_char(desc->character, "One of the mobs in an interaction for the sector you're editing was deleted.\r\n");
			}
		}
		if (GET_OLC_SOCIAL(desc)) {
			found = delete_requirement_from_list(&SOC_REQUIREMENTS(GET_OLC_SOCIAL(desc)), REQ_KILL_MOB, vnum);
			
			if (found) {
				SET_BIT(SOC_FLAGS(GET_OLC_SOCIAL(desc)), SOC_IN_DEVELOPMENT);
				msg_to_desc(desc, "A mobile required by the social you are editing was deleted.\r\n");
			}
		}
		if (GET_OLC_VEHICLE(desc)) {
			if (delete_mob_from_spawn_list(&VEH_SPAWNS(GET_OLC_VEHICLE(desc)), vnum)) {
				msg_to_char(desc->character, "One of the mobs that spawns in the vehicle you're editing was deleted.\r\n");
			}
			if (delete_from_interaction_list(&VEH_INTERACTIONS(GET_OLC_VEHICLE(desc)), TYPE_MOB, vnum)) {
				msg_to_char(desc->character, "One of the mobs in an interaction for the vehicle you're editing was deleted.\r\n");
			}
		}
	}
	
	syslog(SYS_OLC, GET_INVIS_LEV(ch), TRUE, "OLC: %s has deleted mobile %d", GET_NAME(ch), vnum);
	msg_to_char(ch, "Mobile %d deleted.\r\n", vnum);
	
	free_char(proto);
}


/**
* Searches properties of mobs.
*
* @param char_data *ch The person searching.
* @param char *argument The argument they entered.
*/
void olc_fullsearch_mob(char_data *ch, char *argument) {
	extern int get_attack_type_by_name(char *name);
	
	char buf[MAX_STRING_LENGTH], line[MAX_STRING_LENGTH], type_arg[MAX_INPUT_LENGTH], val_arg[MAX_INPUT_LENGTH], find_keywords[MAX_INPUT_LENGTH];
	bitvector_t  find_interacts = NOBITS, found_interacts, find_custom = NOBITS, found_custom;
	bitvector_t not_flagged = NOBITS, only_flags = NOBITS, only_affs = NOBITS;
	int only_attack = NOTHING, only_move = NOTHING, only_nameset = NOTHING;
	int count, only_level = NOTHING, only_sex = NOTHING, only_size = NOTHING;
	faction_data *only_fct = NULL;
	struct interaction_item *inter;
	struct custom_message *cust;
	char_data *mob, *next_mob;
	size_t size;
	
	if (!*argument) {
		msg_to_char(ch, "See HELP MEDIT FULLSEARCH for syntax.\r\n");
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
		FULLSEARCH_FUNC("allegiance", only_fct, find_faction(val_arg))
		FULLSEARCH_FUNC("faction", only_fct, find_faction(val_arg))
		FULLSEARCH_FLAGS("affects", only_affs, affected_bits)
		FULLSEARCH_FUNC("attack", only_attack, get_attack_type_by_name(val_arg))
		FULLSEARCH_FLAGS("custom", find_custom, mob_custom_types)
		FULLSEARCH_FLAGS("flags", only_flags, action_bits)
		FULLSEARCH_FLAGS("flagged", only_flags, action_bits)
		FULLSEARCH_FLAGS("interaction", find_interacts, interact_types)
		FULLSEARCH_INT("level", only_level, 0, INT_MAX)
		FULLSEARCH_LIST("movetype", only_move, mob_move_types)
		FULLSEARCH_LIST("nameset", only_nameset, name_sets)
		FULLSEARCH_LIST("sex", only_sex, genders)
		FULLSEARCH_LIST("gender", only_sex, genders)
		FULLSEARCH_LIST("size", only_size, size_types)
		FULLSEARCH_FLAGS("unflagged", not_flagged, action_bits)
		
		else {	// not sure what to do with it? treat it like a keyword
			sprintf(find_keywords + strlen(find_keywords), "%s%s", *find_keywords ? " " : "", type_arg);
		}
		
		// prepare for next loop
		skip_spaces(&argument);
	}
	
	size = snprintf(buf, sizeof(buf), "Mobile fullsearch: %s\r\n", find_keywords);
	count = 0;
	
	// okay now look up mobs
	HASH_ITER(hh, mobile_table, mob, next_mob) {
		if (only_level != NOTHING) {	// level-based checks
			if (GET_MAX_SCALE_LEVEL(mob) != 0 && only_level > GET_MAX_SCALE_LEVEL(mob)) {
				continue;
			}
			if (GET_MIN_SCALE_LEVEL(mob) != 0 && only_level < GET_MIN_SCALE_LEVEL(mob)) {
				continue;
			}
		}
		if (only_sex != NOTHING && GET_SEX(mob) != only_sex) {
			continue;
		}
		if (not_flagged != NOBITS && MOB_FLAGGED(mob, not_flagged)) {
			continue;
		}
		if (only_affs != NOBITS && (AFF_FLAGS(mob) & only_affs) != only_affs) {
			continue;
		}
		if (only_flags != NOBITS && (MOB_FLAGS(mob) & only_flags) != only_flags) {
			continue;
		}
		if (only_attack != NOTHING && MOB_ATTACK_TYPE(mob) != only_attack) {
			continue;
		}
		if (only_move != NOTHING && MOB_MOVE_TYPE(mob) != only_move) {
			continue;
		}
		if (only_size != NOTHING && SET_SIZE(mob) != only_size) {
			continue;
		}
		if (only_nameset != NOTHING && MOB_NAME_SET(mob) != only_nameset) {
			continue;
		}
		if (only_fct && MOB_FACTION(mob) != only_fct) {
			continue;
		}
		
		if (find_custom) {	// look up its custom messages
			found_custom = NOBITS;
			LL_FOREACH(MOB_CUSTOM_MSGS(mob), cust) {
				found_custom |= BIT(cust->type);
			}
			if ((find_custom & found_custom) != find_custom) {
				continue;
			}
		}
		if (find_interacts) {	// look up its interactions
			found_interacts = NOBITS;
			LL_FOREACH(mob->interactions, inter) {
				found_interacts |= BIT(inter->type);
			}
			if ((find_interacts & found_interacts) != find_interacts) {
				continue;
			}
		}
		if (*find_keywords && !multi_isname(find_keywords, GET_PC_NAME(mob)) && !multi_isname(find_keywords, GET_SHORT_DESC(mob)) && !multi_isname(find_keywords, GET_LONG_DESC(mob)) && (!GET_LOOK_DESC(mob) || !multi_isname(find_keywords, GET_LOOK_DESC(mob))) && !search_custom_messages(find_keywords, MOB_CUSTOM_MSGS(mob))) {
			continue;
		}
		
		// show it
		snprintf(line, sizeof(line), "[%5d] %s\r\n", GET_MOB_VNUM(mob), GET_SHORT_DESC(mob));
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
		size += snprintf(buf + size, sizeof(buf) - size, "(%d mobiles)\r\n", count);
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
void olc_search_mob(char_data *ch, mob_vnum vnum) {
	extern bool find_quest_giver_in_list(struct quest_giver *list, int type, any_vnum vnum);
	extern bool find_quest_reward_in_list(struct quest_reward *list, int type, any_vnum vnum);
	extern bool find_requirement_in_list(struct req_data *list, int type, any_vnum vnum);
	
	char_data *proto, *mob, *next_mob;
	char buf[MAX_STRING_LENGTH];
	struct spawn_info *spawn;
	struct adventure_spawn *asp;
	struct interaction_item *inter;
	struct global_data *glb, *next_glb;
	quest_data *quest, *next_quest;
	progress_data *prg, *next_prg;
	room_template *rmt, *next_rmt;
	vehicle_data *veh, *next_veh;
	sector_data *sect, *next_sect;
	crop_data *crop, *next_crop;
	shop_data *shop, *next_shop;
	social_data *soc, *next_soc;
	bld_data *bld, *next_bld;
	int size, found;
	bool any;
	
	if (!(proto = mob_proto(vnum))) {
		msg_to_char(ch, "There is no mobile %d.\r\n", vnum);
		return;
	}
	
	found = 0;
	size = snprintf(buf, sizeof(buf), "Occurrences of mobile %d (%s):\r\n", vnum, GET_SHORT_DESC(proto));
	
	// buildings
	HASH_ITER(hh, building_table, bld, next_bld) {
		any = FALSE;
		if (GET_BLD_ARTISAN(bld) == vnum) {
			any = TRUE;
			++found;
		}
		for (spawn = GET_BLD_SPAWNS(bld); spawn && !any; spawn = spawn->next) {
			if (spawn->vnum == vnum) {
				any = TRUE;
				++found;
			}
		}
		for (inter = GET_BLD_INTERACTIONS(bld); inter && !any; inter = inter->next) {
			if (interact_vnum_types[inter->type] == TYPE_MOB && inter->vnum == vnum) {
				any = TRUE;
				++found;
			}
		}
		
		if (any) {
			size += snprintf(buf + size, sizeof(buf) - size, "BDG [%5d] %s\r\n", GET_BLD_VNUM(bld), GET_BLD_NAME(bld));
		}
	}
	
	// crops
	HASH_ITER(hh, crop_table, crop, next_crop) {
		any = FALSE;
		for (spawn = GET_CROP_SPAWNS(crop); spawn && !any; spawn = spawn->next) {
			if (spawn->vnum == vnum) {
				any = TRUE;
				++found;
				size += snprintf(buf + size, sizeof(buf) - size, "CRP [%5d] %s\r\n", GET_CROP_VNUM(crop), GET_CROP_NAME(crop));
			}
		}
		for (inter = GET_CROP_INTERACTIONS(crop); inter && !any; inter = inter->next) {
			if (interact_vnum_types[inter->type] == TYPE_MOB && inter->vnum == vnum) {
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
			if (interact_vnum_types[inter->type] == TYPE_MOB && inter->vnum == vnum) {
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
			if (interact_vnum_types[inter->type] == TYPE_MOB && inter->vnum == vnum) {
				any = TRUE;
				++found;
				size += snprintf(buf + size, sizeof(buf) - size, "MOB [%5d] %s\r\n", GET_MOB_VNUM(mob), GET_SHORT_DESC(mob));
			}
		}
	}
	
	// progress
	HASH_ITER(hh, progress_table, prg, next_prg) {
		if (size >= sizeof(buf)) {
			break;
		}
		// REQ_x: requirement search
		any = find_requirement_in_list(PRG_TASKS(prg), REQ_KILL_MOB, vnum);
		
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
		if (find_quest_giver_in_list(QUEST_STARTS_AT(quest), QG_MOBILE, vnum) || find_quest_giver_in_list(QUEST_ENDS_AT(quest), QG_MOBILE, vnum) || find_requirement_in_list(QUEST_TASKS(quest), REQ_KILL_MOB, vnum) || find_requirement_in_list(QUEST_PREREQS(quest), REQ_KILL_MOB, vnum)) {
			++found;
			size += snprintf(buf + size, sizeof(buf) - size, "QST [%5d] %s\r\n", QUEST_VNUM(quest), QUEST_NAME(quest));
		}
	}
	
	// room templates
	HASH_ITER(hh, room_template_table, rmt, next_rmt) {
		any = FALSE;
		for (asp = GET_RMT_SPAWNS(rmt); asp && !any; asp = asp->next) {
			if (asp->type == ADV_SPAWN_MOB && asp->vnum == vnum) {
				any = TRUE;
				++found;
				size += snprintf(buf + size, sizeof(buf) - size, "RMT [%5d] %s\r\n", GET_RMT_VNUM(rmt), GET_RMT_TITLE(rmt));
			}
		}
		for (inter = GET_RMT_INTERACTIONS(rmt); inter && !any; inter = inter->next) {
			if (interact_vnum_types[inter->type] == TYPE_MOB && inter->vnum == vnum) {
				any = TRUE;
				++found;
				size += snprintf(buf + size, sizeof(buf) - size, "RMT [%5d] %s\r\n", GET_RMT_VNUM(rmt), GET_RMT_TITLE(rmt));
			}
		}
	}
	
	// sectors
	HASH_ITER(hh, sector_table, sect, next_sect) {
		any = FALSE;
		for (spawn = GET_SECT_SPAWNS(sect); spawn && !any; spawn = spawn->next) {
			if (spawn->vnum == vnum) {
				any = TRUE;
				++found;
				size += snprintf(buf + size, sizeof(buf) - size, "SCT [%5d] %s\r\n", GET_SECT_VNUM(sect), GET_SECT_NAME(sect));
			}
		}
		for (inter = GET_SECT_INTERACTIONS(sect); inter && !any; inter = inter->next) {
			if (interact_vnum_types[inter->type] == TYPE_MOB && inter->vnum == vnum) {
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
		if (find_quest_giver_in_list(SHOP_LOCATIONS(shop), QG_MOBILE, vnum)) {
			++found;
			size += snprintf(buf + size, sizeof(buf) - size, "SHOP [%5d] %s\r\n", SHOP_VNUM(shop), SHOP_NAME(shop));
		}
	}
	
	// socials
	HASH_ITER(hh, social_table, soc, next_soc) {
		if (size >= sizeof(buf)) {
			break;
		}
		if (find_requirement_in_list(SOC_REQUIREMENTS(soc), REQ_KILL_MOB, vnum)) {
			++found;
			size += snprintf(buf + size, sizeof(buf) - size, "SOC [%5d] %s\r\n", SOC_VNUM(soc), SOC_NAME(soc));
		}
	}
	
	// vehicles
	HASH_ITER(hh, vehicle_table, veh, next_veh) {
		any = FALSE;
		for (spawn = VEH_SPAWNS(veh); spawn && !any; spawn = spawn->next) {
			if (spawn->vnum == vnum) {
				any = TRUE;
				++found;
			}
		}
		for (inter = VEH_INTERACTIONS(veh); inter && !any; inter = inter->next) {
			if (interact_vnum_types[inter->type] == TYPE_MOB && inter->vnum == vnum) {
				any = TRUE;
				++found;
			}
		}
		
		if (any) {
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
* Function to save a player's changes to a mob (or a new mob).
*
* @param descriptor_data *desc The descriptor who is saving a mobile.
*/
void save_olc_mobile(descriptor_data *desc) {
	void scale_mob_to_level(char_data *mob, int level);

	char_data *mob = GET_OLC_MOBILE(desc), *mob_iter, *proto;
	mob_vnum vnum = GET_OLC_VNUM(desc);
	struct quest_lookup *ql;
	struct shop_lookup *sl;
	UT_hash_handle hh;
	bool changed;
	
	// have a place to save it?
	if (!(proto = mob_proto(vnum))) {
		proto = create_mob_table_entry(vnum);
	}
	
	// slight sanity checking
	if (GET_MAX_SCALE_LEVEL(mob) < GET_MIN_SCALE_LEVEL(mob) && GET_MAX_SCALE_LEVEL(mob) > 0) {
		GET_MAX_SCALE_LEVEL(mob) = GET_MIN_SCALE_LEVEL(mob);
	}
	
	// update the strings and pointers on live mobs
	for (mob_iter = character_list; mob_iter; mob_iter = mob_iter->next) {
		if (IS_NPC(mob_iter) && GET_MOB_VNUM(mob_iter) == vnum) {
			// update strings
			if (GET_PC_NAME(mob_iter) == GET_PC_NAME(proto)) {
				GET_PC_NAME(mob_iter) = GET_PC_NAME(mob);
			}
			if (GET_SHORT_DESC(mob_iter) == GET_SHORT_DESC(proto)) {
				GET_SHORT_DESC(mob_iter) = GET_SHORT_DESC(mob);
			}
			if (GET_LONG_DESC(mob_iter) == GET_LONG_DESC(proto)) {
				GET_LONG_DESC(mob_iter) = GET_LONG_DESC(mob);
			}
			if (GET_LOOK_DESC(mob_iter) == GET_LOOK_DESC(proto)) {
				GET_LOOK_DESC(mob_iter) = GET_LOOK_DESC(mob);
			}

			// update pointers
			if (mob_iter->interactions == proto->interactions) {
				mob_iter->interactions = mob->interactions;
			}
			if (MOB_CUSTOM_MSGS(mob_iter) == MOB_CUSTOM_MSGS(proto)) {
				MOB_CUSTOM_MSGS(mob_iter) = MOB_CUSTOM_MSGS(mob);
			}
			MOB_FACTION(mob_iter) = MOB_FACTION(mob);
			
			// check for changes to level, flags, or damage
			changed = (GET_MIN_SCALE_LEVEL(mob_iter) != GET_MIN_SCALE_LEVEL(mob)) || (GET_MAX_SCALE_LEVEL(mob_iter) != GET_MAX_SCALE_LEVEL(mob)) || (MOB_ATTACK_TYPE(mob_iter) != MOB_ATTACK_TYPE(mob)) || (MOB_FLAGS(mob_iter) != MOB_FLAGS(mob));
			
			// update stats
			GET_MIN_SCALE_LEVEL(mob_iter) = GET_MIN_SCALE_LEVEL(mob);
			GET_MAX_SCALE_LEVEL(mob_iter) = GET_MAX_SCALE_LEVEL(mob);
			MOB_ATTACK_TYPE(mob_iter) = MOB_ATTACK_TYPE(mob);
			MOB_FLAGS(mob_iter) = MOB_FLAGS(mob);
			
			// re-scale
			if (changed && GET_CURRENT_SCALE_LEVEL(mob_iter) > 0) {
				scale_mob_to_level(mob_iter, GET_CURRENT_SCALE_LEVEL(mob_iter));
			}

			// remove old scripts
			if (SCRIPT(mob_iter)) {
				remove_all_triggers(mob_iter, MOB_TRIGGER);
			}
			if (mob_iter->proto_script && mob_iter->proto_script != proto->proto_script) {
				free_proto_scripts(&mob_iter->proto_script);
			}
			
			// attach new scripts
			mob_iter->proto_script = copy_trig_protos(mob->proto_script);
			assign_triggers(mob_iter, MOB_TRIGGER);
		}
	}
	
	// free prototype strings and pointers
	if (GET_PC_NAME(proto)) {
		free(GET_PC_NAME(proto));
	}
	if (GET_SHORT_DESC(proto)) {
		free(GET_SHORT_DESC(proto));
	}
	if (GET_LONG_DESC(proto)) {
		free(GET_LONG_DESC(proto));
	}
	if (GET_LOOK_DESC(proto)) {
		free(GET_LOOK_DESC(proto));
	}

	free_interactions(&proto->interactions);
	free_custom_messages(MOB_CUSTOM_MSGS(proto));
	
	if (proto->proto_script) {
		free_proto_scripts(&proto->proto_script);
	}
	
	// save data back over the proto-type
	hh = proto->hh;	// save old hash handle
	ql = proto->quest_lookups;	// save lookups
	sl = proto->shop_lookups;
	
	*proto = *mob;
	proto->vnum = vnum;	// ensure correct vnum
	
	proto->hh = hh;	// restore hash handle
	proto->quest_lookups = ql;	// restore lookups
	proto->shop_lookups = sl;
	
	// and save to file
	save_library_file_for_vnum(DB_BOOT_MOB, vnum);
}


/**
* Creates a copy of a mob, or clears a new one, for editing.
* 
* @param char_data *input The mobile to copy, or NULL to make a new one.
* @return char_data *The copied mob.
*/
char_data *setup_olc_mobile(char_data *input) {
	char_data *new;
	
	CREATE(new, char_data, 1);
	clear_char(new);
	
	if (input) {
		// copy normal data
		*new = *input;

		// copy things that are pointers
		GET_PC_NAME(new) = GET_PC_NAME(input) ? str_dup(GET_PC_NAME(input)) : NULL;
		GET_SHORT_DESC(new) = GET_SHORT_DESC(input) ? str_dup(GET_SHORT_DESC(input)) : NULL;
		GET_LONG_DESC(new) = GET_LONG_DESC(input) ? str_dup(GET_LONG_DESC(input)) : NULL;
		GET_LOOK_DESC(new) = GET_LOOK_DESC(input) ? str_dup(GET_LOOK_DESC(input)) : NULL;

		// copy scripts
		SCRIPT(new) = NULL;
		new->proto_script = copy_trig_protos(input->proto_script);
		
		// copy interactions
		new->interactions = copy_interaction_list(input->interactions);
		
		// copy custom msgs
		MOB_CUSTOM_MSGS(new) = copy_custom_messages(MOB_CUSTOM_MSGS(input));
	}
	else {
		// brand new
		GET_PC_NAME(new) = str_dup(default_mob_keywords);
		GET_SHORT_DESC(new) = str_dup(default_mob_short);
		GET_LONG_DESC(new) = str_dup(default_mob_long);
		MOB_FLAGS(new) = MOB_ISNPC;
		MOB_ATTACK_TYPE(new) = TYPE_HIT;

		SCRIPT(new) = NULL;
		new->proto_script = NULL;
	}
	
	// done
	return new;	
}


 //////////////////////////////////////////////////////////////////////////////
//// DISPLAYS ////////////////////////////////////////////////////////////////

/**
* This is the main mob display for mobile OLC. It displays the user's
* currently-edited mob.
*
* @param char_data *ch The person who is editing an mobile and will see its display.
*/
void olc_show_mobile(char_data *ch) {
	void get_interaction_display(struct interaction_item *list, char *save_buffer);
	void get_script_display(struct trig_proto_list *list, char *save_buffer);
	
	char buf[MAX_STRING_LENGTH * 4];	// these get long
	char_data *mob = GET_OLC_MOBILE(ch->desc);
	struct custom_message *mcm;
	int count;
	
	if (!mob) {
		return;
	}
	
	*buf = '\0';
	sprintf(buf + strlen(buf), "[%s%d\t0] %s%s\t0\r\n", OLC_LABEL_CHANGED, GET_OLC_VNUM(ch->desc), OLC_LABEL_UNCHANGED, !mob_proto(GET_OLC_VNUM(ch->desc)) ? "new mobile" : GET_SHORT_DESC(mob_proto(GET_OLC_VNUM(ch->desc))));
	sprintf(buf + strlen(buf), "<%skeywords\t0> %s\r\n", OLC_LABEL_STR(GET_PC_NAME(mob), default_mob_keywords), GET_PC_NAME(mob));
	sprintf(buf + strlen(buf), "<%sshortdescription\t0> %s\r\n", OLC_LABEL_STR(GET_SHORT_DESC(mob), default_mob_short), GET_SHORT_DESC(mob));
	sprintf(buf + strlen(buf), "<%slongdescription\t0> %s", OLC_LABEL_STR(GET_LONG_DESC(mob), default_mob_long), GET_LONG_DESC(mob));
	sprintf(buf + strlen(buf), "<%slookdescription\t0>\r\n%s", OLC_LABEL_PTR(GET_LOOK_DESC(mob)), NULLSAFE(GET_LOOK_DESC(mob)));
	
	sprintf(buf + strlen(buf), "<%ssex\t0> %s\r\n", OLC_LABEL_VAL(GET_SEX(mob), 0), genders[GET_SEX(mob)]);
	
	sprintbit(MOB_FLAGS(mob), action_bits, buf1, TRUE);
	sprintf(buf + strlen(buf), "<%sflags\t0> %s\r\n", OLC_LABEL_VAL(MOB_FLAGS(mob), MOB_ISNPC), buf1);
	
	sprintbit(AFF_FLAGS(mob), affected_bits, buf1, TRUE);
	sprintf(buf + strlen(buf), "<%saffects\t0> %s\r\n", OLC_LABEL_VAL(AFF_FLAGS(mob), NOBITS), buf1);

	if (GET_MIN_SCALE_LEVEL(mob) > 0) {
		sprintf(buf + strlen(buf), "<%sminlevel\t0> %d\r\n", OLC_LABEL_VAL(GET_MIN_SCALE_LEVEL(mob), 0), GET_MIN_SCALE_LEVEL(mob));
	}
	else {
		sprintf(buf + strlen(buf), "<%sminlevel\t0> none\r\n", OLC_LABEL_UNCHANGED);
	}
	
	if (GET_MAX_SCALE_LEVEL(mob) > 0) {	
		sprintf(buf + strlen(buf), "<%smaxlevel\t0> %d\r\n", OLC_LABEL_VAL(GET_MAX_SCALE_LEVEL(mob), 0), GET_MAX_SCALE_LEVEL(mob));
	}
	else {
		sprintf(buf + strlen(buf), "<%smaxlevel\t0> none\r\n", OLC_LABEL_UNCHANGED);
	}
	
	sprintf(buf + strlen(buf), "<%sattack\t0> %s\r\n", OLC_LABEL_VAL(MOB_ATTACK_TYPE(mob), TYPE_HIT), attack_hit_info[MOB_ATTACK_TYPE(mob)].name);
	sprintf(buf + strlen(buf), "<%smovetype\t0> %s\r\n", OLC_LABEL_VAL(MOB_MOVE_TYPE(mob), 0), mob_move_types[(int) MOB_MOVE_TYPE(mob)]);
	sprintf(buf + strlen(buf), "<%ssize\t0> %s\r\n", OLC_LABEL_VAL(SET_SIZE(mob), SIZE_NORMAL), size_types[(int)SET_SIZE(mob)]);
	sprintf(buf + strlen(buf), "<%snameset\t0> %s\r\n", OLC_LABEL_VAL(MOB_NAME_SET(mob), 0), name_sets[MOB_NAME_SET(mob)]);
	sprintf(buf + strlen(buf), "<%sallegiance\t0> %s\r\n", OLC_LABEL_PTR(MOB_FACTION(mob)), MOB_FACTION(mob) ? FCT_NAME(MOB_FACTION(mob)) : "none");
	
	sprintf(buf + strlen(buf), "Interactions: <%sinteraction\t0>\r\n", OLC_LABEL_PTR(mob->interactions));
	if (mob->interactions) {
		get_interaction_display(mob->interactions, buf1);
		strcat(buf, buf1);
	}
	
	// custom messages
	sprintf(buf + strlen(buf), "Custom messages: <%scustom\t0>\r\n", OLC_LABEL_PTR(MOB_CUSTOM_MSGS(mob)));
	count = 0;
	LL_FOREACH(MOB_CUSTOM_MSGS(mob), mcm) {
		sprintf(buf + strlen(buf), " \ty%d\t0. [%s] %s\r\n", ++count, mob_custom_types[mcm->type], mcm->msg);
	}
	
	sprintf(buf + strlen(buf), "Scripts: <%sscript\t0>\r\n", OLC_LABEL_PTR(mob->proto_script));
	if (mob->proto_script) {
		get_script_display(mob->proto_script, buf1);
		strcat(buf, buf1);
	}
	
	page_string(ch->desc, buf, TRUE);
}


 //////////////////////////////////////////////////////////////////////////////
//// EDIT MODULES ////////////////////////////////////////////////////////////

OLC_MODULE(medit_affects) {
	char_data *mob = GET_OLC_MOBILE(ch->desc);
	AFF_FLAGS(mob) = olc_process_flag(ch, argument, "affect", "affects", affected_bits, AFF_FLAGS(mob));
}


OLC_MODULE(medit_allegiance) {
	char_data *mob = GET_OLC_MOBILE(ch->desc);
	faction_data *fct;
	
	if (!*argument) {
		msg_to_char(ch, "Set the mob's allegiance to which faction (or 'none')?\r\n");
	}
	else if (!str_cmp(argument, "none")) {
		msg_to_char(ch, "You set its allegiance to 'none'.\r\n");
		MOB_FACTION(mob) = NULL;
	}
	else if (!(fct = find_faction(argument))) {
		msg_to_char(ch, "Unknown faction '%s'.\r\n", argument);
	}
	else {
		MOB_FACTION(mob) = fct;
		msg_to_char(ch, "You set its allegiance to %s.\r\n", FCT_NAME(fct));
	}
}


OLC_MODULE(medit_attack) {
	char_data *mob = GET_OLC_MOBILE(ch->desc);
	MOB_ATTACK_TYPE(mob) = olc_process_type(ch, argument, "attack type", "attack", (const char**)get_weapon_types_string(), MOB_ATTACK_TYPE(mob));
}


OLC_MODULE(medit_custom) {
	void olc_process_custom_messages(char_data *ch, char *argument, struct custom_message **list, const char **type_names);
	char_data *mob = GET_OLC_MOBILE(ch->desc);
	olc_process_custom_messages(ch, argument, &MOB_CUSTOM_MSGS(mob), mob_custom_types);
}


OLC_MODULE(medit_flags) {
	char_data *mob = GET_OLC_MOBILE(ch->desc);
	MOB_FLAGS(mob) = olc_process_flag(ch, argument, "mob", "flags", action_bits, MOB_FLAGS(mob));

	// ensure
	SET_BIT(MOB_FLAGS(mob), MOB_ISNPC);
}


OLC_MODULE(medit_interaction) {
	char_data *mob = GET_OLC_MOBILE(ch->desc);
	olc_process_interactions(ch, argument, &mob->interactions, TYPE_MOB);
}


OLC_MODULE(medit_keywords) {
	char_data *mob = GET_OLC_MOBILE(ch->desc);
	olc_process_string(ch, argument, "keywords", &GET_PC_NAME(mob));
}


OLC_MODULE(medit_longdescription) {
	char_data *mob = GET_OLC_MOBILE(ch->desc);
	olc_process_string(ch, argument, "long description", &GET_LONG_DESC(mob));
	
	// long desc actually needs a \r\n
	if (GET_LONG_DESC(mob)) {
		sprintf(buf, "%s\r\n", GET_LONG_DESC(mob));
		free(GET_LONG_DESC(mob));
		GET_LONG_DESC(mob) = str_dup(buf);
	}
}


OLC_MODULE(medit_lookdescription) {
	char_data *mob = GET_OLC_MOBILE(ch->desc);

	if (ch->desc->str) {
		msg_to_char(ch, "You are already editing a string.\r\n");
	}
	else {
		sprintf(buf, "description for %s", GET_SHORT_DESC(mob));
		start_string_editor(ch->desc, buf, &GET_LOOK_DESC(mob), MAX_PLAYER_DESCRIPTION, TRUE);
	}
}


OLC_MODULE(medit_maxlevel) {
	char_data *mob = GET_OLC_MOBILE(ch->desc);	
	GET_MAX_SCALE_LEVEL(mob) = olc_process_number(ch, argument, "maximum level", "maxlevel", 0, MAX_INT, GET_MAX_SCALE_LEVEL(mob));
}


OLC_MODULE(medit_minlevel) {
	char_data *mob = GET_OLC_MOBILE(ch->desc);	
	GET_MIN_SCALE_LEVEL(mob) = olc_process_number(ch, argument, "minimum level", "minlevel", 0, MAX_INT, GET_MIN_SCALE_LEVEL(mob));
}


OLC_MODULE(medit_movetype) {
	char_data *mob = GET_OLC_MOBILE(ch->desc);
	MOB_MOVE_TYPE(mob) = olc_process_type(ch, argument, "move type", "movetype", mob_move_types, MOB_MOVE_TYPE(mob));
}


OLC_MODULE(medit_nameset) {
	char_data *mob = GET_OLC_MOBILE(ch->desc);
	MOB_NAME_SET(mob) = olc_process_type(ch, argument, "name set", "nameset", name_sets, MOB_NAME_SET(mob));
}


OLC_MODULE(medit_script) {
	char_data *mob = GET_OLC_MOBILE(ch->desc);
	olc_process_script(ch, argument, &(mob->proto_script), MOB_TRIGGER);
}


OLC_MODULE(medit_sex) {
	char_data *mob = GET_OLC_MOBILE(ch->desc);
	GET_REAL_SEX(mob) = olc_process_type(ch, argument, "sex", "sex", genders, GET_REAL_SEX(mob));
}


OLC_MODULE(medit_short_description) {
	char_data *mob = GET_OLC_MOBILE(ch->desc);
	olc_process_string(ch, argument, "short description", &GET_SHORT_DESC(mob));
}


OLC_MODULE(medit_size) {
	char_data *mob = GET_OLC_MOBILE(ch->desc);
	SET_SIZE(mob) = olc_process_type(ch, argument, "size", "size", size_types, SET_SIZE(mob));
}
