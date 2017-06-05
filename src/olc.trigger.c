/* ************************************************************************
*   File: olc.trigger.c                                   EmpireMUD 2.0b5 *
*  Usage: OLC for triggers                                                *
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
#include "handler.h"
#include "dg_scripts.h"
#include "dg_event.h"

/**
* Contents:
*   Helpers
*   Displays
*   Edit Modules
*/

// external consts
extern const char **trig_attach_type_list[];
extern const char *trig_attach_types[];
extern const char *trig_types[];
extern const char *otrig_types[];
extern const char *vtrig_types[];
extern const char *wtrig_types[];


// external funcs
void extract_trigger(trig_data *trig);
void trig_data_init(trig_data *this_data);


// locals
const char *trig_arg_obj_where[] = {
	"equipment",
	"inventory",
	"room",
	"\n"
};

const char *trig_arg_phrase_type[] = {
	"phrase",
	"wordlist",
	"\n"
};


 //////////////////////////////////////////////////////////////////////////////
//// HELPERS /////////////////////////////////////////////////////////////////

/**
* Determines all the argument types to show for the various types of a trigger.
*
* @param trig_data *trig The trigger.
* @return bitvector_t A set of TRIG_ARG_x flags.
*/
bitvector_t compile_argument_types_for_trigger(trig_data *trig) {
	extern const bitvector_t *trig_argument_type_list[];
	
	bitvector_t flags = NOBITS, trigger_types = GET_TRIG_TYPE(trig);
	int pos;
	
	for (pos = 0; trigger_types; trigger_types >>= 1, ++pos) {
		if (IS_SET(trigger_types, BIT(0))) {
			flags |= trig_argument_type_list[trig->attach_type][pos];
		}
	}
	
	return flags;
}


/**
* Creates a new trigger entry.
* 
* @param trig_vnum vnum The number to create.
* @return trig_data* The new trigger.
*/
trig_data *create_trigger_table_entry(trig_vnum vnum) {
	void add_trigger_to_table(trig_data *trig);
	
	trig_data *trig;
	
	// sanity
	if (real_trigger(vnum)) {
		log("SYSERR: Attempting to insert trigger at existing vnum %d", vnum);
		return real_trigger(vnum);
	}
	
	CREATE(trig, trig_data, 1);
	trig_data_init(trig);
	trig->vnum = vnum;
	add_trigger_to_table(trig);
	
	// simple default data (triggers cannot be nameless)
	trig->name = str_dup("New Trigger");
		
	// save index and crop file now
	save_index(DB_BOOT_TRG);
	save_library_file_for_vnum(DB_BOOT_TRG, vnum);
	
	return trig;
}


/**
* For the .list command.
*
* @param trig_data *trig The thing to list.
* @param bool detail If TRUE, provide additional details
* @return char* The line to show (without a CRLF).
*/
char *list_one_trigger(trig_data *trig, bool detail) {
	static char output[MAX_STRING_LENGTH];
	
	if (detail) {
		snprintf(output, sizeof(output), "[%5d] %s", GET_TRIG_VNUM(trig), GET_TRIG_NAME(trig));
	}
	else {
		snprintf(output, sizeof(output), "[%5d] %s", GET_TRIG_VNUM(trig), GET_TRIG_NAME(trig));
	}
	
	return output;
}


/**
* Removes triggers from a live set of scripts, by vnum.
*
* @param struct script_data *script The script to remove triggers from.
* @param trig_vnum vnum The trigger vnum to remove.
* @return bool TRUE if any were removed; FALSE otherwise.
*/
bool remove_live_script_by_vnum(struct script_data *script, trig_vnum vnum) {
	struct trig_data *trig, *next_trig, *temp;
	bool found = FALSE;
	
	if (!script) {
		return found;
	}

	for (trig = TRIGGERS(script); trig; trig = next_trig) {
		next_trig = trig->next;
		
		if (GET_TRIG_VNUM(trig) == vnum) {
			found = TRUE;
			REMOVE_FROM_LIST(trig, TRIGGERS(script), next);
			extract_trigger(trig);
		}
	}
	
	return found;
}


/**
* Ensures there are no non-existent triggers attached to anything.
*/
void check_triggers(void) {
	struct trig_proto_list *tpl, *next_tpl;
	quest_data *quest, *next_quest;
	room_template *rmt, *next_rmt;
	vehicle_data *veh, *next_veh;
	char_data *mob, *next_mob;
	obj_data *obj, *next_obj;
	bld_data *bld, *next_bld;
	
	// check building protos
	HASH_ITER(hh, building_table, bld, next_bld) {
		LL_FOREACH_SAFE(GET_BLD_SCRIPTS(bld), tpl, next_tpl) {
			if (!real_trigger(tpl->vnum)) {
				log("SYSERR: Removing missing trigger %d from building %d.", tpl->vnum, GET_BLD_VNUM(bld));
				LL_DELETE(GET_BLD_SCRIPTS(bld), tpl);
				free(tpl);
			}
		}
	}
	
	// check mob protos
	HASH_ITER(hh, mobile_table, mob, next_mob) {
		LL_FOREACH_SAFE(mob->proto_script, tpl, next_tpl) {
			if (!real_trigger(tpl->vnum)) {
				log("SYSERR: Removing missing trigger %d from mobile %d.", tpl->vnum, GET_MOB_VNUM(mob));
				LL_DELETE(mob->proto_script, tpl);
				free(tpl);
			}
		}
	}
	
	// check obj protos
	HASH_ITER(hh, object_table, obj, next_obj) {
		LL_FOREACH_SAFE(obj->proto_script, tpl, next_tpl) {
			if (!real_trigger(tpl->vnum)) {
				log("SYSERR: Removing missing trigger %d from object %d.", tpl->vnum, GET_OBJ_VNUM(obj));
				LL_DELETE(obj->proto_script, tpl);
				free(tpl);
			}
		}
	}
	
	// check quests
	HASH_ITER(hh, quest_table, quest, next_quest) {
		LL_FOREACH_SAFE(QUEST_SCRIPTS(quest), tpl, next_tpl) {
			if (!real_trigger(tpl->vnum)) {
				log("SYSERR: Removing missing trigger %d from quest %d.", tpl->vnum, QUEST_VNUM(quest));
				LL_DELETE(QUEST_SCRIPTS(quest), tpl);
				free(tpl);
			}
		}
	}
	
	// check room templates
	HASH_ITER(hh, room_template_table, rmt, next_rmt) {
		LL_FOREACH_SAFE(GET_RMT_SCRIPTS(rmt), tpl, next_tpl) {
			if (!real_trigger(tpl->vnum)) {
				log("SYSERR: Removing missing trigger %d from room template %d.", tpl->vnum, GET_RMT_VNUM(rmt));
				LL_DELETE(GET_RMT_SCRIPTS(rmt), tpl);
				free(tpl);
			}
		}
	}
	
	// check vehicle protos
	HASH_ITER(hh, vehicle_table, veh, next_veh) {
		LL_FOREACH_SAFE(veh->proto_script, tpl, next_tpl) {
			if (!real_trigger(tpl->vnum)) {
				log("SYSERR: Removing missing trigger %d from vehicle %d.", tpl->vnum, VEH_VNUM(veh));
				LL_DELETE(veh->proto_script, tpl);
				free(tpl);
			}
		}
	}
}


/**
* Deletes a trigger from a proto list.
*
* @param struct trig_proto_list **list The list to check.
* @param trig_vnum vnum The trigger to remove.
* @return bool TRUE if any triggers were removed, FALSE if not.
*/
bool delete_from_proto_list_by_vnum(struct trig_proto_list **list, trig_vnum vnum) {
	struct trig_proto_list *trig, *next_trig, *temp;
	bool found = FALSE;
	
	for (trig = *list; trig; trig = next_trig) {
		next_trig = trig->next;
		if (trig->vnum == vnum) {
			found = TRUE;
			REMOVE_FROM_LIST(trig, *list, next);
			free(trig);
		}
	}
	
	return found;
}


/**
* WARNING: This function actually deletes a trigger.
*
* @param char_data *ch The person doing the deleting.
* @param trig_vnum vnum The vnum to delete.
*/
void olc_delete_trigger(char_data *ch, trig_vnum vnum) {
	extern bool delete_quest_giver_from_list(struct quest_giver **list, int type, any_vnum vnum);
	void remove_trigger_from_table(trig_data *trig);
	
	trig_data *trig;
	quest_data *quest, *next_quest;
	room_template *rmt, *next_rmt;
	vehicle_data *veh, *next_veh;
	room_data *room, *next_room;
	char_data *mob, *next_mob;
	descriptor_data *dsc;
	adv_data *adv, *next_adv;
	obj_data *obj, *next_obj;
	bld_data *bld, *next_bld;
	bool found;

	if (!(trig = real_trigger(vnum))) {
		msg_to_char(ch, "There is no such trigger %d.\r\n", vnum);
		return;
	}
	
	if (HASH_COUNT(trigger_table) <= 1) {
		msg_to_char(ch, "You can't delete the last trigger.\r\n");
		return;
	}
	
	// remove from hash table
	remove_trigger_from_table(trig);
	
	// look for live mobs with this script and remove
	for (mob = character_list; mob; mob = mob->next) {
		if (IS_NPC(mob) && SCRIPT(mob)) {
			remove_live_script_by_vnum(SCRIPT(mob), vnum);
		}
	}
	
	// look for live objs with this script and remove
	for (obj = object_list; obj; obj = obj->next) {
		if (SCRIPT(obj)) {
			remove_live_script_by_vnum(SCRIPT(obj), vnum);
		}
	}
	
	// live vehicles -> remove
	LL_FOREACH(vehicle_list, veh) {
		if (SCRIPT(veh)) {
			remove_live_script_by_vnum(SCRIPT(veh), vnum);
		}
	}
	
	// look for live rooms with this trigger
	HASH_ITER(hh, world_table, room, next_room) {
		if (SCRIPT(room)) {
			remove_live_script_by_vnum(SCRIPT(room), vnum);
		}
		delete_from_proto_list_by_vnum(&(room->proto_script), vnum);
	}
	
	// remove from adventures
	HASH_ITER(hh, adventure_table, adv, next_adv) {
		if (delete_from_proto_list_by_vnum(&GET_ADV_SCRIPTS(adv), vnum)) {
			save_library_file_for_vnum(DB_BOOT_ADV, GET_ADV_VNUM(adv));
		}
	}
	
	// update building protos
	HASH_ITER(hh, building_table, bld, next_bld) {
		if (delete_from_proto_list_by_vnum(&GET_BLD_SCRIPTS(bld), vnum)) {
			save_library_file_for_vnum(DB_BOOT_BLD, GET_BLD_VNUM(bld));
		}
	}
	
	// update mob protos
	HASH_ITER(hh, mobile_table, mob, next_mob) {
		if (delete_from_proto_list_by_vnum(&mob->proto_script, vnum)) {
			save_library_file_for_vnum(DB_BOOT_MOB, mob->vnum);
		}
	}
	
	// update obj protos
	HASH_ITER(hh, object_table, obj, next_obj) {
		if (delete_from_proto_list_by_vnum(&obj->proto_script, vnum)) {
			save_library_file_for_vnum(DB_BOOT_OBJ, GET_OBJ_VNUM(obj));
		}
	}
	
	// update quests
	HASH_ITER(hh, quest_table, quest, next_quest) {
		found = delete_quest_giver_from_list(&QUEST_STARTS_AT(quest), QG_TRIGGER, vnum);
		found |= delete_quest_giver_from_list(&QUEST_ENDS_AT(quest), QG_TRIGGER, vnum);
		found |= delete_from_proto_list_by_vnum(&QUEST_SCRIPTS(quest), vnum);
		
		if (found) {
			SET_BIT(QUEST_FLAGS(quest), QST_IN_DEVELOPMENT);
			save_library_file_for_vnum(DB_BOOT_QST, QUEST_VNUM(quest));
		}
	}
	
	// room templates
	HASH_ITER(hh, room_template_table, rmt, next_rmt) {
		if (delete_from_proto_list_by_vnum(&GET_RMT_SCRIPTS(rmt), vnum)) {
			save_library_file_for_vnum(DB_BOOT_RMT, GET_RMT_VNUM(rmt));
		}
	}
	
	// update vehicle protos
	HASH_ITER(hh, vehicle_table, veh, next_veh) {
		if (delete_from_proto_list_by_vnum(&veh->proto_script, vnum)) {
			save_library_file_for_vnum(DB_BOOT_VEH, VEH_VNUM(veh));
		}
	}
	
	// update olc editors
	for (dsc = descriptor_list; dsc; dsc = dsc->next) {
		if (GET_OLC_ADVENTURE(dsc) && delete_from_proto_list_by_vnum(&GET_ADV_SCRIPTS(GET_OLC_ADVENTURE(dsc)), vnum)) {
			msg_to_char(dsc->character, "A trigger attached to the adventure you're editing was deleted.\r\n");
		}
		if (GET_OLC_BUILDING(dsc) && delete_from_proto_list_by_vnum(&GET_BLD_SCRIPTS(GET_OLC_BUILDING(dsc)), vnum)) {
			msg_to_char(dsc->character, "A trigger attached to the building you're editing was deleted.\r\n");
		}
		if (GET_OLC_OBJECT(dsc) && delete_from_proto_list_by_vnum(&GET_OLC_OBJECT(dsc)->proto_script, vnum)) {
			msg_to_char(dsc->character, "A trigger attached to the object you're editing was deleted.\r\n");
		}
		if (GET_OLC_MOBILE(dsc) && delete_from_proto_list_by_vnum(&GET_OLC_MOBILE(dsc)->proto_script, vnum)) {
			msg_to_char(dsc->character, "A trigger attached to the mobile you're editing was deleted.\r\n");
		}
		if (GET_OLC_QUEST(dsc)) {
			found = delete_quest_giver_from_list(&QUEST_STARTS_AT(GET_OLC_QUEST(dsc)), QG_TRIGGER, vnum);
			found |= delete_quest_giver_from_list(&QUEST_ENDS_AT(GET_OLC_QUEST(dsc)), QG_TRIGGER, vnum);
			found |= delete_from_proto_list_by_vnum(&QUEST_SCRIPTS(GET_OLC_QUEST(dsc)), vnum);
			
			if (found) {
				SET_BIT(QUEST_FLAGS(GET_OLC_QUEST(dsc)), QST_IN_DEVELOPMENT);
				msg_to_desc(dsc, "A trigger used by the quest you are editing was deleted.\r\n");
			}
		}
		if (GET_OLC_ROOM_TEMPLATE(dsc) && delete_from_proto_list_by_vnum(&GET_OLC_ROOM_TEMPLATE(dsc)->proto_script, vnum)) {
			msg_to_char(dsc->character, "A trigger attached to the room template you're editing was deleted.\r\n");
		}
		if (GET_OLC_VEHICLE(dsc) && delete_from_proto_list_by_vnum(&GET_OLC_VEHICLE(dsc)->proto_script, vnum)) {
			msg_to_char(dsc->character, "A trigger attached to the vehicle you're editing was deleted.\r\n");
		}
	}
	
	// save index and trigger file now
	save_index(DB_BOOT_TRG);
	save_library_file_for_vnum(DB_BOOT_TRG, vnum);
	
	syslog(SYS_OLC, GET_INVIS_LEV(ch), TRUE, "OLC: %s has deleted trigger %d", GET_NAME(ch), vnum);
	msg_to_char(ch, "Trigger %d deleted.\r\n", vnum);
	
	free_trigger(trig);
}


/**
* Searches properties of triggers.
*
* @param char_data *ch The person searching.
* @param char *argument The argument they entered.
*/
void olc_fullsearch_trigger(char_data *ch, char *argument) {
	char buf[MAX_STRING_LENGTH], line[MAX_STRING_LENGTH], type_arg[MAX_INPUT_LENGTH], val_arg[MAX_INPUT_LENGTH], find_keywords[MAX_INPUT_LENGTH];
	int count, lookup, only_attaches = NOTHING;
	bitvector_t mob_types = NOBITS, obj_types = NOBITS, wld_types = NOBITS, veh_types = NOBITS;
	trig_data *trig, *next_trig;
	struct cmdlist_element *cmd;
	bool any_types = FALSE, any;
	size_t size;
	
	if (!*argument) {
		msg_to_char(ch, "See HELP TEDIT FULLSEARCH for syntax.\r\n");
		return;
	}
	
	// process argument
	*find_keywords = '\0';
	while (*argument) {
		// figure out a type
		argument = any_one_arg(argument, type_arg);
		
		if (is_abbrev(type_arg, "-attaches")) {
			argument = any_one_word(argument, val_arg);
			if ((only_attaches = search_block(val_arg, trig_attach_types, FALSE)) == NOTHING) {
				msg_to_char(ch, "Invalid attach type '%s'.\r\n", val_arg);
				return;
			}
		}
		else if (is_abbrev(type_arg, "-types")) {
			argument = any_one_word(argument, val_arg);
			any = FALSE;
			if ((lookup = search_block(val_arg, trig_types, FALSE)) != NOTHING) {
				mob_types |= BIT(lookup);
				any_types = any = TRUE;
			}
			if ((lookup = search_block(val_arg, otrig_types, FALSE)) != NOTHING) {
				obj_types |= BIT(lookup);
				any_types = any = TRUE;
			}
			if ((lookup = search_block(val_arg, vtrig_types, FALSE)) != NOTHING) {
				veh_types |= BIT(lookup);
				any_types = any = TRUE;
			}
			if ((lookup = search_block(val_arg, wtrig_types, FALSE)) != NOTHING) {
				wld_types |= BIT(lookup);
				any_types = any = TRUE;
			}
			
			if (!any) {
				msg_to_char(ch, "Invalid trigger type '%s'.\r\n", val_arg);
				return;
			}
		}
		else {	// not sure what to do with it? treat it like a keyword
			sprintf(find_keywords + strlen(find_keywords), "%s%s", *find_keywords ? " " : "", type_arg);
		}
		
		// prepare for next loop
		skip_spaces(&argument);
	}
	
	size = snprintf(buf, sizeof(buf), "Trigger fullsearch: %s\r\n", find_keywords);
	count = 0;
	
	// okay now look up items
	HASH_ITER(hh, trigger_table, trig, next_trig) {
		if (only_attaches != NOTHING && trig->attach_type != only_attaches) {
			continue;
		}
		// x_TRIGGER:
		if (mob_types && trig->attach_type == MOB_TRIGGER && (GET_TRIG_TYPE(trig) & mob_types) != mob_types) {
			continue;
		}
		if (obj_types && trig->attach_type == OBJ_TRIGGER && (GET_TRIG_TYPE(trig) & obj_types) != obj_types) {
			continue;
		}
		if (wld_types && (trig->attach_type == WLD_TRIGGER || trig->attach_type == ADV_TRIGGER || trig->attach_type == RMT_TRIGGER || trig->attach_type == BLD_TRIGGER) && (GET_TRIG_TYPE(trig) & wld_types) != wld_types) {
			continue;
		}
		if (veh_types && trig->attach_type == VEH_TRIGGER && (GET_TRIG_TYPE(trig) & veh_types) != veh_types) {
			continue;
		}
		if (any_types && trig->attach_type == MOB_TRIGGER && !mob_types) {
			continue;
		}
		if (any_types && trig->attach_type == OBJ_TRIGGER && !obj_types) {
			continue;
		}
		if (any_types && (trig->attach_type == WLD_TRIGGER || trig->attach_type == ADV_TRIGGER || trig->attach_type == RMT_TRIGGER || trig->attach_type == BLD_TRIGGER) && !wld_types) {
			continue;
		}
		if (any_types && trig->attach_type == VEH_TRIGGER && !veh_types) {
			continue;
		}
		if (*find_keywords) {
			any = multi_isname(find_keywords, GET_TRIG_NAME(trig));	// check name first
			
			if (!any) {
				LL_FOREACH(trig->cmdlist, cmd) {
					if (strstr(cmd->cmd, find_keywords)) {
						any = TRUE;
						break;
					}
				}
			}
			if (!any) {
				continue;
			}
		}
		
		// show it
		snprintf(line, sizeof(line), "[%5d] %s\r\n", GET_TRIG_VNUM(trig), GET_TRIG_NAME(trig));
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
		size += snprintf(buf + size, sizeof(buf) - size, "(%d triggers)\r\n", count);
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
void olc_search_trigger(char_data *ch, trig_vnum vnum) {
	extern bool find_quest_giver_in_list(struct quest_giver *list, int type, any_vnum vnum);
	
	char buf[MAX_STRING_LENGTH];
	trig_data *proto = real_trigger(vnum);
	quest_data *quest, *next_quest;
	struct trig_proto_list *trig;
	room_template *rmt, *next_rmt;
	vehicle_data *veh, *next_veh;
	char_data *mob, *next_mob;
	adv_data *adv, *next_adv;
	obj_data *obj, *next_obj;
	bld_data *bld, *next_bld;
	int size, found;
	bool any;
	
	if (!proto) {
		msg_to_char(ch, "There is no trigger %d.\r\n", vnum);
		return;
	}
	
	found = 0;
	size = snprintf(buf, sizeof(buf), "Occurrences of trigger %d (%s):\r\n", vnum, GET_TRIG_NAME(proto));
	
	// adventure scripts
	HASH_ITER(hh, adventure_table, adv, next_adv) {
		any = FALSE;
		for (trig = GET_ADV_SCRIPTS(adv); trig && !any; trig = trig->next) {
			if (trig->vnum == vnum) {
				any = TRUE;
				++found;
				size += snprintf(buf + size, sizeof(buf) - size, "ADV [%5d] %s\r\n", GET_ADV_VNUM(adv), GET_ADV_NAME(adv));
			}
		}
	}
	
	// buildings
	HASH_ITER(hh, building_table, bld, next_bld) {
		any = FALSE;
		for (trig = GET_BLD_SCRIPTS(bld); trig && !any; trig = trig->next) {
			if (trig->vnum == vnum) {
				any = TRUE;
				++found;
				size += snprintf(buf + size, sizeof(buf) - size, "BLD [%5d] %s\r\n", GET_BLD_VNUM(bld), GET_BLD_NAME(bld));
			}
		}
	}
	
	// mobs
	HASH_ITER(hh, mobile_table, mob, next_mob) {
		any = FALSE;
		for (trig = mob->proto_script; trig && !any; trig = trig->next) {
			if (trig->vnum == vnum) {
				any = TRUE;
				++found;
				size += snprintf(buf + size, sizeof(buf) - size, "MOB [%5d] %s\r\n", GET_MOB_VNUM(mob), GET_SHORT_DESC(mob));
			}
		}
	}
	
	// objs
	HASH_ITER(hh, object_table, obj, next_obj) {
		any = FALSE;
		for (trig = obj->proto_script; trig && !any; trig = trig->next) {
			if (trig->vnum == vnum) {
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
		any = find_quest_giver_in_list(QUEST_STARTS_AT(quest), QG_TRIGGER, vnum);
		any |= find_quest_giver_in_list(QUEST_ENDS_AT(quest), QG_TRIGGER, vnum);
		if (!any) {
			LL_FOREACH(QUEST_SCRIPTS(quest), trig) {
				if (trig->vnum == vnum) {
					any = TRUE;
					break;
				}
			}
		}
		
		if (any) {
			++found;
			size += snprintf(buf + size, sizeof(buf) - size, "QST [%5d] %s\r\n", QUEST_VNUM(quest), QUEST_NAME(quest));
		}
	}
	
	// room templates
	HASH_ITER(hh, room_template_table, rmt, next_rmt) {
		any = FALSE;
		for (trig = GET_RMT_SCRIPTS(rmt); trig && !any; trig = trig->next) {
			if (trig->vnum == vnum) {
				any = TRUE;
				++found;
				size += snprintf(buf + size, sizeof(buf) - size, "RMT [%5d] %s\r\n", GET_RMT_VNUM(rmt), GET_RMT_TITLE(rmt));
			}
		}
	}
	
	// vehicles
	HASH_ITER(hh, vehicle_table, veh, next_veh) {
		any = FALSE;
		for (trig = veh->proto_script; trig && !any; trig = trig->next) {
			if (trig->vnum == vnum) {
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
* Function to save a player's changes to a trigger (or a new one).
*
* @param descriptor_data *desc The descriptor who is saving.
*/
void save_olc_trigger(descriptor_data *desc, char *script_text) {
	extern struct cmdlist_element *compile_command_list(char *input);
	void free_varlist(struct trig_var_data *vd);
	
	trig_data *proto, *live_trig, *next_trig, *find, *trig = GET_OLC_TRIGGER(desc);
	trig_vnum vnum = GET_OLC_VNUM(desc);
	struct cmdlist_element *cmd, *next_cmd;
	struct script_data *sc;
	bool free_text = FALSE;
	UT_hash_handle hh;
	int pos;
	
	// have a place to save it?
	if (!(proto = real_trigger(vnum))) {
		proto = create_trigger_table_entry(vnum);
	}
	
	// free existing commands
	for (cmd = proto->cmdlist; cmd; cmd = next_cmd) { 
		next_cmd = cmd->next;
		if (cmd->cmd)
			free(cmd->cmd);
		free(cmd);
	}

	// free old data on the proto
	if (proto->arglist) {
		free(proto->arglist);
	}
	if (proto->name) {
		free(proto->name);
	}
	
	if (!*script_text) {
		// do not free old script text
		script_text = str_dup("%echo% This trigger commandlist is not complete!\r\n");
		free_text = TRUE;
	}
	
	// Recompile the command list from the new script
	trig->cmdlist = compile_command_list(script_text);
	
	if (free_text) {
		free(script_text);
		script_text = NULL;
	}

	// make the prorotype look like what we have
	hh = proto->hh;	// preserve hash handle
	trig_data_copy(proto, trig);
	proto->hh = hh;
	proto->vnum = vnum;	// ensure correct vnu,
	
	// remove and reattach existing copies of this trigger
	LL_FOREACH_SAFE2(trigger_list, live_trig, next_trig, next_in_world) {
		if (GET_TRIG_VNUM(live_trig) != vnum) {
			continue;	// wrong trigger
		}
		if (!(sc = live_trig->attached_to)) {
			continue;	// can't get attachment data for some reason
		}
		
		// determin position
		pos = 0;
		LL_FOREACH(TRIGGERS(sc), find) {
			if (find == trig) {
				break;
			}
			else {
				++pos;
			}
		}
		
		// un-attach and free
		LL_DELETE(TRIGGERS(sc), live_trig);
		extract_trigger(live_trig);
		
		// load and re-attach
		if ((live_trig = read_trigger(vnum))) {
			add_trigger(sc, live_trig, pos);
		}
	}
	
	save_library_file_for_vnum(DB_BOOT_TRG, vnum);
}


/**
* Creates a copy of a trigger, or clears a new one, for editing.
* 
* @param struct trig_data *input The trigger to copy, or NULL to make a new one.
* @param char **cmdlist_storage A place to store the command list e.g. &GET_OLC_STORAGE(ch->desc)
* @return struct trig_data* The copied trigger.
*/
struct trig_data *setup_olc_trigger(struct trig_data *input, char **cmdlist_storage) {
	struct cmdlist_element *c;
	struct trig_data *new;
	
	CREATE(*cmdlist_storage, char, MAX_CMD_LENGTH);
	CREATE(new, struct trig_data, 1);
	trig_data_init(new);
	
	if (input) {
		*new = *input;
		
		new->next = NULL;
		new->next_in_world = NULL;
		
		new->name = str_dup(NULLSAFE(input->name));
		new->arglist = input->arglist ? str_dup(input->arglist) : NULL;
		
		// don't copy these things
		new->cmdlist = NULL;
		new->curr_state = NULL;
		new->wait_event = NULL;
		new->var_list = NULL;

		// convert cmdlist to a char string
		c = input->cmdlist;
		strcpy(*cmdlist_storage, "");

		while (c) {
			strcat(*cmdlist_storage, c->cmd);
			strcat(*cmdlist_storage, "\r\n");
			c = c->next;
		}
		// now the cmdlist is something to pass to the text editor
		// it will be converted back to a real cmdlist_element list later
	}
	else {
		new->vnum = NOTHING;

		// Set up some defaults
		new->name = strdup("new trigger");
		new->attach_type = MOB_TRIGGER;
		new->trigger_type = NOBITS;

		// cmdlist will be a large char string until the trigger is saved
		strncpy(*cmdlist_storage, "", MAX_CMD_LENGTH-1);
		new->narg = 100;
	}
	
	// done
	return new;	
}


 //////////////////////////////////////////////////////////////////////////////
//// DISPLAYS ////////////////////////////////////////////////////////////////

/**
* This is the main display for trigger OLC. It displays the user's
* currently-edited trigger.
*
* @param char_data *ch The person who is editing a trigger and will see its display.
*/
void olc_show_trigger(char_data *ch) {
	extern char *show_color_codes(char *string);
	
	trig_data *trig = GET_OLC_TRIGGER(ch->desc);
	bitvector_t trig_arg_types = compile_argument_types_for_trigger(trig);
	char trgtypes[256], buf[MAX_STRING_LENGTH * 4];	// that HAS to be long enough, right?
	
	if (!trig) {
		return;
	}
	
	*buf = '\0';
	
	sprintf(buf + strlen(buf), "[&c%d&0] &c%s&0\r\n", GET_OLC_VNUM(ch->desc), !real_trigger(GET_OLC_VNUM(ch->desc)) ? "new trigger" : GET_TRIG_NAME(real_trigger(GET_OLC_VNUM(ch->desc))));
	sprintf(buf + strlen(buf), "<&yname&0> %s\r\n", NULLSAFE(GET_TRIG_NAME(trig)));
	sprintf(buf + strlen(buf), "<&yattaches&0> %s\r\n", trig_attach_types[trig->attach_type]);
	
	sprintbit(GET_TRIG_TYPE(trig), trig_attach_type_list[trig->attach_type], trgtypes, TRUE);
	sprintf(buf + strlen(buf), "<&ytypes&0> %s\r\n", trgtypes);
	
	if (IS_SET(trig_arg_types, TRIG_ARG_PERCENT)) {
		sprintf(buf + strlen(buf), "<&ypercent&0> %d%%\r\n", trig->narg);
	}
	if (IS_SET(trig_arg_types, TRIG_ARG_PHRASE_OR_WORDLIST)) {
		sprintf(buf + strlen(buf), "<&yargtype&0> %s\r\n", trig_arg_phrase_type[trig->narg]);
	}
	if (IS_SET(trig_arg_types, TRIG_ARG_OBJ_WHERE)) {
		sprintbit(trig->narg, trig_arg_obj_where, buf1, TRUE);
		sprintf(buf + strlen(buf), "<&ylocation&0> %s\r\n", trig->narg ? buf1 : "none");
	}
	if (IS_SET(trig_arg_types, TRIG_ARG_COMMAND | TRIG_ARG_PHRASE_OR_WORDLIST | TRIG_ARG_OBJ_WHERE)) {
		sprintf(buf + strlen(buf), "<&ystring&0> %s\r\n", NULLSAFE(trig->arglist));
	}
	if (IS_SET(trig_arg_types, TRIG_ARG_COST)) {
		sprintf(buf + strlen(buf), "<&ycosts&0> %d other coins\r\n", trig->narg);
	}
	
	sprintf(buf + strlen(buf), "<&ycommands&0>\r\n%s", show_color_codes(NULLSAFE(GET_OLC_STORAGE(ch->desc))));
	
	page_string(ch->desc, buf, TRUE);
}


 //////////////////////////////////////////////////////////////////////////////
//// EDIT MODULES ////////////////////////////////////////////////////////////

OLC_MODULE(tedit_argtype) {
	trig_data *trig = GET_OLC_TRIGGER(ch->desc);
	bitvector_t trig_arg_types = compile_argument_types_for_trigger(trig);
	
	if (!IS_SET(trig_arg_types, TRIG_ARG_PHRASE_OR_WORDLIST)) {
		msg_to_char(ch, "You can't set that property on this trigger.\r\n");
	}
	else {
		GET_TRIG_NARG(trig) = olc_process_type(ch, argument, "argument type", "argtype", trig_arg_phrase_type, GET_TRIG_NARG(trig));
	}
}


OLC_MODULE(tedit_attaches) {
	trig_data *trig = GET_OLC_TRIGGER(ch->desc);
	int old = trig->attach_type;
	
	trig->attach_type = olc_process_type(ch, argument, "attach type", "attaches", trig_attach_types, trig->attach_type);
	
	if (old != trig->attach_type) {
		GET_TRIG_TYPE(trig) = NOBITS;
	}
}


OLC_MODULE(tedit_commands) {
	if (ch->desc->str) {
		msg_to_char(ch, "You are already editing a string.\r\n");
	}
	else {
		start_string_editor(ch->desc, "trigger commands", &GET_OLC_STORAGE(ch->desc), MAX_CMD_LENGTH, FALSE);
	}
}


OLC_MODULE(tedit_costs) {
	trig_data *trig = GET_OLC_TRIGGER(ch->desc);
	bitvector_t trig_arg_types = compile_argument_types_for_trigger(trig);
	
	if (!IS_SET(trig_arg_types, TRIG_ARG_COST)) {
		msg_to_char(ch, "You can't set that property on this trigger.\r\n");
	}
	else {
		GET_TRIG_NARG(trig) = olc_process_number(ch, argument, "cost", "costs", 0, MAX_INT, GET_TRIG_NARG(trig));
	}
}


OLC_MODULE(tedit_location) {
	trig_data *trig = GET_OLC_TRIGGER(ch->desc);
	bitvector_t trig_arg_types = compile_argument_types_for_trigger(trig);
	
	if (!IS_SET(trig_arg_types, TRIG_ARG_OBJ_WHERE)) {
		msg_to_char(ch, "You can't set that property on this trigger.\r\n");
	}
	else {
		GET_TRIG_NARG(trig) = olc_process_flag(ch, argument, "location", "location", trig_arg_obj_where, GET_TRIG_NARG(trig));
	}
}


OLC_MODULE(tedit_name) {
	trig_data *trig = GET_OLC_TRIGGER(ch->desc);
	olc_process_string(ch, argument, "name", &GET_TRIG_NAME(trig));
}


OLC_MODULE(tedit_numarg) {
	trig_data *trig = GET_OLC_TRIGGER(ch->desc);
	GET_TRIG_NARG(trig) = olc_process_number(ch, argument, "numeric argument", "numarg", -MAX_INT, MAX_INT, GET_TRIG_NARG(trig));
}


OLC_MODULE(tedit_percent) {
	trig_data *trig = GET_OLC_TRIGGER(ch->desc);
	bitvector_t trig_arg_types = compile_argument_types_for_trigger(trig);
	
	if (!IS_SET(trig_arg_types, TRIG_ARG_PERCENT)) {
		msg_to_char(ch, "You can't set that property on this trigger.\r\n");
	}
	else {
		GET_TRIG_NARG(trig) = olc_process_number(ch, argument, "percent", "percent", 0, 100, GET_TRIG_NARG(trig));
	}
}


OLC_MODULE(tedit_string) {
	trig_data *trig = GET_OLC_TRIGGER(ch->desc);
	bitvector_t trig_arg_types = compile_argument_types_for_trigger(trig);
	
	if (!IS_SET(trig_arg_types, TRIG_ARG_COMMAND | TRIG_ARG_PHRASE_OR_WORDLIST | TRIG_ARG_OBJ_WHERE)) {
		msg_to_char(ch, "You can't set that property on this trigger.\r\n");
	}
	else {
		olc_process_string(ch, argument, "string", &GET_TRIG_ARG(trig));
	}
}


OLC_MODULE(tedit_types) {
	trig_data *trig = GET_OLC_TRIGGER(ch->desc);	
	bitvector_t old = GET_TRIG_TYPE(trig), diff;
	
	bitvector_t ignore_changes = MTRIG_GLOBAL | MTRIG_PLAYER_IN_ROOM;	// all types ignore global changes
	
	// mobs also ignore changes to the charmed flag
	if (trig->attach_type == MOB_TRIGGER) {
		ignore_changes |= MTRIG_CHARMED;
	}
	
	GET_TRIG_TYPE(trig) = olc_process_flag(ch, argument, "type", "types", trig_attach_type_list[trig->attach_type], GET_TRIG_TYPE(trig));
	
	diff = (old & ~GET_TRIG_TYPE(trig)) | (~old & GET_TRIG_TYPE(trig));	// find changed bits
	diff &= ~ignore_changes;	// filter out ones we don't care about
	
	// if we changed any relevant type, remove args
	if (diff != NOBITS) {
		trig->narg = 0;
		if (trig->arglist) {
			free(trig->arglist);
			trig->arglist = NULL;
			msg_to_char(ch, "Type changed. Arguments cleared.\r\n");
		}
	}
}
