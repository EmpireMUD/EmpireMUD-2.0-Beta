/* ************************************************************************
*   File: olc.mobile.c                                    EmpireMUD 2.0b3 *
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
extern const byte interact_vnum_types[NUM_INTERACTS];
extern const char *mob_move_types[];
extern const char *name_sets[];

// external funcs
extern char **get_weapon_types_string();


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
	extern adv_data *get_adventure_for_vnum(rmt_vnum vnum);
	
	bool is_adventure = (get_adventure_for_vnum(GET_MOB_VNUM(mob)) != NULL);
	char temp[MAX_STRING_LENGTH], *ptr;
	bool problem = FALSE;

	if (!str_cmp(GET_PC_NAME(mob), "mobile new")) {
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
	
	if (!str_cmp(GET_SHORT_DESC(mob), "a new mobile")) {
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
	
	if (!str_cmp(GET_LONG_DESC(mob), "A new mobile is standing here.\r\n")) {
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
			free(inter);
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
* @param int spawn_type Any ADV_SPAWN_x.
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
	void extract_pending_chars();
	void remove_mobile_from_table(char_data *mob);
	
	char_data *proto, *mob_iter, *next_mob;
	descriptor_data *desc;
	struct global_data *glb, *next_glb;
	room_template *rmt, *next_rmt;
	sector_data *sect, *next_sect;
	crop_data *crop, *next_crop;
	bld_data *bld, *next_bld;
	bool found;
	
	if (!(proto = mob_proto(vnum))) {
		msg_to_char(ch, "There is no such mobile %d.\r\n", vnum);
		return;
	}
	
	if (HASH_COUNT(mobile_table) <= 1) {
		msg_to_char(ch, "You can't delete the last mob.\r\n");
		return;
	}
		
	// remove mobs from the list: DO THIS FIRST
	for (mob_iter = character_list; mob_iter; mob_iter = next_mob) {
		next_mob = mob_iter->next;
		
		if (IS_NPC(mob_iter)) {
			if (GET_MOB_VNUM(mob_iter) == vnum) {
				// this is the removed mob
				act("$n has been deleted.", FALSE, mob_iter, NULL, NULL, TO_ROOM);
				extract_char(mob_iter);
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
	
	// remove spawn locations and interactions from active editors
	for (desc = descriptor_list; desc; desc = desc->next) {
		if (GET_OLC_BUILDING(desc)) {
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
		if (GET_OLC_ROOM_TEMPLATE(desc)) {
			if (delete_from_spawn_template_list(&GET_OLC_ROOM_TEMPLATE(desc)->spawns, ADV_SPAWN_MOB, vnum)) {
				msg_to_char(desc->character, "One of the mobs that spawns in the room template you're editing was deleted.\r\n");
			}
			if (delete_from_interaction_list(&GET_OLC_ROOM_TEMPLATE(desc)->interactions, TYPE_MOB, vnum)) {
				msg_to_char(desc->character, "One of the mobs in an interaction for the room template you're editing was deleted.\r\n");
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
	}
	
	syslog(SYS_OLC, GET_INVIS_LEV(ch), TRUE, "OLC: %s has deleted mobile %d", GET_NAME(ch), vnum);
	msg_to_char(ch, "Mobile %d deleted.\r\n", vnum);
	
	free_char(proto);
}


/**
* Searches for all uses of a crop and displays them.
*
* @param char_data *ch The player.
* @param crop_vnum vnum The crop vnum.
*/
void olc_search_mob(char_data *ch, mob_vnum vnum) {
	char_data *proto, *mob, *next_mob;
	char buf[MAX_STRING_LENGTH];
	struct spawn_info *spawn;
	struct adventure_spawn *asp;
	struct interaction_item *inter;
	struct global_data *glb, *next_glb;
	room_template *rmt, *next_rmt;
	sector_data *sect, *next_sect;
	crop_data *crop, *next_crop;
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
		for (spawn = GET_BLD_SPAWNS(bld); spawn && !any; spawn = spawn->next) {
			if (spawn->vnum == vnum) {
				any = TRUE;
				++found;
				size += snprintf(buf + size, sizeof(buf) - size, "BDG [%5d] %s\r\n", GET_BLD_VNUM(bld), GET_BLD_NAME(bld));
			}
		}
		for (inter = GET_BLD_INTERACTIONS(bld); inter && !any; inter = inter->next) {
			if (interact_vnum_types[inter->type] == TYPE_MOB && inter->vnum == vnum) {
				any = TRUE;
				++found;
				size += snprintf(buf + size, sizeof(buf) - size, "BDG [%5d] %s\r\n", GET_BLD_VNUM(bld), GET_BLD_NAME(bld));
			}
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
	struct interaction_item *interact;
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

			// update pointers
			if (mob_iter->interactions == proto->interactions) {
				mob_iter->interactions = mob->interactions;
			}
			
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
				extract_script(mob_iter, MOB_TRIGGER);
			}
			if (mob_iter->proto_script && mob_iter->proto_script != proto->proto_script) {
				free_proto_script(mob_iter, MOB_TRIGGER);
			}
			
			// attach new scripts
			copy_proto_script(mob, mob_iter, MOB_TRIGGER);
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

	while ((interact = proto->interactions)) {
		proto->interactions = interact->next;
		free(interact);
	}

	if (proto->proto_script) {
		free_proto_script(proto, MOB_TRIGGER);
	}
	
	// save data back over the proto-type
	hh = proto->hh;	// save old hash handle
	*proto = *mob;
	proto->vnum = vnum;	// ensure correct vnum
	proto->hh = hh;	// restore hash handle
	
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

		// copy scripts
		SCRIPT(new) = NULL;
		new->proto_script = NULL;
		copy_proto_script(input, new, MOB_TRIGGER);
		
		// copy interactions
		new->interactions = copy_interaction_list(input->interactions);
	}
	else {
		// brand new
		GET_PC_NAME(new) = str_dup("mobile new");
		GET_SHORT_DESC(new) = str_dup("a new mobile");
		GET_LONG_DESC(new) = str_dup("A new mobile is standing here.\r\n");
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

	char_data *mob = GET_OLC_MOBILE(ch->desc);
	
	if (!mob) {
		return;
	}
	
	*buf = '\0';
	sprintf(buf + strlen(buf), "[&c%d&0] &c%s&0\r\n", GET_OLC_VNUM(ch->desc), !mob_proto(GET_OLC_VNUM(ch->desc)) ? "new mobile" : GET_SHORT_DESC(mob_proto(GET_OLC_VNUM(ch->desc))));
	sprintf(buf + strlen(buf), "<&ykeywords&0> %s\r\n", GET_PC_NAME(mob));
	sprintf(buf + strlen(buf), "<&yshortdescription&0> %s\r\n", GET_SHORT_DESC(mob));
	sprintf(buf + strlen(buf), "<&ylongdescription&0> %s", GET_LONG_DESC(mob));
	
	sprintf(buf + strlen(buf), "<&ysex&0> %s\r\n", genders[GET_SEX(mob)]);
	
	sprintbit(MOB_FLAGS(mob), action_bits, buf1, TRUE);
	sprintf(buf + strlen(buf), "<&yflags&0> %s\r\n", buf1);
	
	sprintbit(AFF_FLAGS(mob), affected_bits, buf1, TRUE);
	sprintf(buf + strlen(buf), "<&yaffects&0> %s\r\n", buf1);

	if (GET_MIN_SCALE_LEVEL(mob) > 0) {
		sprintf(buf + strlen(buf), "<&yminlevel&0> %d\r\n", GET_MIN_SCALE_LEVEL(mob));
	}
	else {
		sprintf(buf + strlen(buf), "<&yminlevel&0> none\r\n");
	}
	
	if (GET_MAX_SCALE_LEVEL(mob) > 0) {	
		sprintf(buf + strlen(buf), "<&ymaxlevel&0> %d\r\n", GET_MAX_SCALE_LEVEL(mob));
	}
	else {
		sprintf(buf + strlen(buf), "<&ymaxlevel&0> none\r\n");
	}
	
	sprintf(buf + strlen(buf), "<&yattack&0> %s\r\n", attack_hit_info[MOB_ATTACK_TYPE(mob)].name);
	sprintf(buf + strlen(buf), "<&ymovetype&0> %s\r\n", mob_move_types[(int) MOB_MOVE_TYPE(mob)]);
	sprintf(buf + strlen(buf), "<&ynameset&0> %s\r\n", name_sets[MOB_NAME_SET(mob)]);

	sprintf(buf + strlen(buf), "Interactions: <&yinteraction&0>\r\n");
	get_interaction_display(mob->interactions, buf1);
	strcat(buf, buf1);
	
	sprintf(buf + strlen(buf), "Scripts: <&yscript&0>\r\n");
	get_script_display(mob->proto_script, buf1);
	strcat(buf, buf1);
	
	page_string(ch->desc, buf, TRUE);
}


 //////////////////////////////////////////////////////////////////////////////
//// EDIT MODULES ////////////////////////////////////////////////////////////

OLC_MODULE(medit_affects) {
	char_data *mob = GET_OLC_MOBILE(ch->desc);
	AFF_FLAGS(mob) = olc_process_flag(ch, argument, "affect", "affects", affected_bits, AFF_FLAGS(mob));
}


OLC_MODULE(medit_attack) {
	extern char **get_weapon_types_string();
	
	char_data *mob = GET_OLC_MOBILE(ch->desc);	
	MOB_ATTACK_TYPE(mob) = olc_process_type(ch, argument, "attack type", "attack", (const char**)get_weapon_types_string(), MOB_ATTACK_TYPE(mob));
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
