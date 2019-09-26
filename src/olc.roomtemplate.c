/* ************************************************************************
*   File: olc.roomtemplate.c                              EmpireMUD 2.0b5 *
*  Usage: OLC for room templates                                          *
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
extern const char *adventure_spawn_types[];
extern const char *dirs[];
extern const char *exit_bits[];
extern const char *function_flags[];
extern const char *interact_types[];
extern const byte interact_vnum_types[NUM_INTERACTS];
extern const char *room_aff_bits[];
extern const char *room_template_flags[];

// external funcs
extern adv_data *get_adventure_for_vnum(rmt_vnum vnum);
void init_room_template(room_template *rmt);
void sort_interactions(struct interaction_item **list);

// locals
const char *default_rmt_title = "An Unnamed Room";


 //////////////////////////////////////////////////////////////////////////////
//// HELPERS /////////////////////////////////////////////////////////////////

/**
* Checks for common room template problems and reports them to ch.
*
* @param room_template *rmt The thing to audit.
* @param char_data *ch The person to report to.
* @return bool TRUE if any problems were reported; FALSE if all good.
*/
bool audit_room_template(room_template *rmt, char_data *ch) {
	extern bool audit_extra_descs(any_vnum vnum, struct extra_descr_data *list, char_data *ch);
	extern bool audit_interactions(any_vnum vnum, struct interaction_item *list, int attach_type, char_data *ch);
	
	adv_data *my_adv = get_adventure_for_vnum(GET_RMT_VNUM(rmt));
	struct adventure_spawn *spawn;
	struct trig_proto_list *tpl;
	struct exit_template *ex;
	room_template *other;
	bool problem = FALSE;
	trig_data *trig;
	
	if (!my_adv) {
		olc_audit_msg(ch, GET_RMT_VNUM(rmt), "Not part of any adventure");
		problem = TRUE;
	}
	if (!strcmp(GET_RMT_TITLE(rmt), default_rmt_title)) {
		olc_audit_msg(ch, GET_RMT_VNUM(rmt), "Title not set");
		problem = TRUE;
	}
	if (!isupper(*GET_RMT_TITLE(rmt))) {
		olc_audit_msg(ch, GET_RMT_VNUM(rmt), "Title not capitalized");
		problem = TRUE;
	}
	if (ispunct(GET_RMT_TITLE(rmt)[strlen(GET_RMT_TITLE(rmt)) - 1])) {
		olc_audit_msg(ch, GET_RMT_VNUM(rmt), "Title is punctuated");
		problem = TRUE;
	}
	if (!GET_RMT_DESC(rmt) || !*GET_RMT_DESC(rmt) || !str_cmp(GET_RMT_DESC(rmt), "Nothing.\r\n")) {
		olc_audit_msg(ch, GET_RMT_VNUM(rmt), "Desc not set");
		problem = TRUE;
	}
	else if (!strn_cmp(GET_RMT_DESC(rmt), "Nothing.", 8)) {
		olc_audit_msg(ch, GET_RMT_VNUM(rmt), "Desc starts with 'Nothing.'");
		problem = TRUE;
	}
	
	if (IS_SET(GET_RMT_FLAGS(rmt), RMT_DARK) && IS_SET(GET_RMT_FLAGS(rmt), RMT_LIGHT)) {
		olc_audit_msg(ch, GET_RMT_VNUM(rmt), "Both DARK and LIGHT");
		problem = TRUE;
	}
	
	// check exits
	for (ex = GET_RMT_EXITS(rmt); ex; ex = ex->next) {
		other = room_template_proto(ex->target_room);
		if (!other) {
			olc_audit_msg(ch, GET_RMT_VNUM(rmt), "Exit %s: invalid target room %d", dirs[ex->dir], ex->target_room);
			problem = TRUE;
		}
		else if (get_adventure_for_vnum(ex->target_room) != my_adv) {
			olc_audit_msg(ch, GET_RMT_VNUM(rmt), "Exit %s: links outside the adventure", dirs[ex->dir]);
			problem = TRUE;
		}
	}
	
	// check spawns
	for (spawn = GET_RMT_SPAWNS(rmt); spawn; spawn = spawn->next) {
		switch (spawn->type) {
			case ADV_SPAWN_MOB: {
				char_data *mob = mob_proto(spawn->vnum);
				if (!mob) {
					olc_audit_msg(ch, GET_RMT_VNUM(rmt), "Spawn mob %d: No such mob", spawn->vnum);
					problem = TRUE;
				}
				break;
			}
			case ADV_SPAWN_OBJ: {
				obj_data *obj = obj_proto(spawn->vnum);
				if (!obj) {
					olc_audit_msg(ch, GET_RMT_VNUM(rmt), "Spawn obj %d: No such obj", spawn->vnum);
					problem = TRUE;
				}
				break;
			}
			case ADV_SPAWN_VEH: {
				vehicle_data *veh = vehicle_proto(spawn->vnum);
				if (!veh) {
					olc_audit_msg(ch, GET_RMT_VNUM(rmt), "Spawn veh %d: No such vehicle", spawn->vnum);
					problem = TRUE;
				}
				break;
			}
		}
	}

	// check scripts
	for (tpl = GET_RMT_SCRIPTS(rmt); tpl; tpl = tpl->next) {
		if (!(trig = real_trigger(tpl->vnum))) {
			continue;
		}
		if (trig->attach_type != WLD_TRIGGER) {
			olc_audit_msg(ch, GET_RMT_VNUM(rmt), "Incorrect trigger type (trg %d)", tpl->vnum);
			problem = TRUE;
		}
	}

	problem |= audit_extra_descs(GET_RMT_VNUM(rmt), GET_RMT_EX_DESCS(rmt), ch);
	problem |= audit_interactions(GET_RMT_VNUM(rmt), GET_RMT_INTERACTIONS(rmt), TYPE_ROOM, ch);
	
	return problem;
}


/**
* Creates a new room template entry.
* 
* @param rmt_vnum vnum The number to create.
* @return room_template* The new room template's prototype.
*/
room_template *create_room_template_table_entry(rmt_vnum vnum) {
	void add_room_template_to_table(room_template *rmt);

	room_template *rmt;
	
	// sanity
	if (room_template_proto(vnum)) {
		log("SYSERR: Attempting to insert room template at existing vnum %d", vnum);
		return room_template_proto(vnum);
	}
	
	CREATE(rmt, room_template, 1);
	init_room_template(rmt);
	GET_RMT_VNUM(rmt) = vnum;
	add_room_template_to_table(rmt);

	// save index and room template file now
	save_index(DB_BOOT_RMT);
	save_library_file_for_vnum(DB_BOOT_RMT, vnum);
	
	return rmt;
}


/**
* For the .list command.
*
* @param room_template *rmt The thing to list.
* @param bool detail If TRUE, provide additional details
* @return char* The line to show (without a CRLF).
*/
char *list_one_room_template(room_template *rmt, bool detail) {
	static char output[MAX_STRING_LENGTH];
	char flags[MAX_STRING_LENGTH];
	
	bitvector_t show_flags = RMT_OUTDOOR | RMT_DARK | RMT_LIGHT | RMT_NO_MOB | RMT_NO_TELEPORT | RMT_LOOK_OUT | RMT_NO_LOCATION;
	
	if (detail) {
		if (IS_SET(GET_RMT_FLAGS(rmt), show_flags)) {
			strcpy(flags, " - ");
			sprintbit(GET_RMT_FLAGS(rmt) & show_flags, room_template_flags, flags + 3, TRUE);
		}
		else {
			*flags = '\0';
		}
		
		snprintf(output, sizeof(output), "[%5d] %s%s", GET_RMT_VNUM(rmt), GET_RMT_TITLE(rmt), flags);
	}
	else {
		snprintf(output, sizeof(output), "[%5d] %s", GET_RMT_VNUM(rmt), GET_RMT_TITLE(rmt));
	}
	
	return output;
}


/**
* Processes the automatic addition of a "matching exit" to a room, from another
* room template.
*
* @param char_data *ch Person to receive info messages.
* @param room_template *add_exit_to The room that will gain new exits.
* @param room_template *origin The other room we're matching an exit from.
* @param struct exit_template *ex The exit to match.
* @return bool TRUE if it adds an exit; FALSE if not (no messages sent on FALSE).
*/
bool match_one_exit(char_data *ch, room_template *add_exit_to, room_template *origin, struct exit_template *ex) {
	extern int rev_dir[];
	
	struct exit_template *new, *temp;
	int dir = (ex->dir == DIR_RANDOM ? DIR_RANDOM : rev_dir[ex->dir]);
	bool found = FALSE;
	
	// ensure there isn't already an exit back
	for (temp = add_exit_to->exits; temp && !found; temp = temp->next) {
		// see if there's already an exit to that room in that dir
		if (temp->target_room == origin->vnum && temp->dir == dir) {
			found = TRUE;
		}
	}
	
	// also ensure we don't already have an exit blocking the desired dir
	if (dir != DIR_RANDOM) {
		for (temp = add_exit_to->exits; temp && !found; temp = temp->next) {
			if (temp->dir == dir) {
				found = TRUE;
			}
		}
	}
	
	// nothing to add
	if (found) {
		return FALSE;
	}
	
	// ok, go ahead and add it
	CREATE(new, struct exit_template, 1);

	new->dir = dir;
	new->target_room = origin->vnum;
	new->exit_info = ex->exit_info;
	new->keyword = (ex->keyword ? str_dup(ex->keyword) : NULL);
	new->next = NULL;
	
	// add at end
	if ((temp = add_exit_to->exits) != NULL) {
		while (temp->next) {
			temp = temp->next;
		}
		temp->next = new;
	}
	else {
		add_exit_to->exits = new;
	}
	
	msg_to_char(ch, "Matched exit %s to %d %s.\r\n", dirs[dir], origin->vnum, origin->title);
	return TRUE;
}


/**
* WARNING: This function actually deletes a room template.
*
* @param char_data *ch The person doing the deleting.
* @param rmt_vnum vnum The vnum to delete.
*/
void olc_delete_room_template(char_data *ch, rmt_vnum vnum) {
	extern bool delete_quest_giver_from_list(struct quest_giver **list, int type, any_vnum vnum);
	extern bool delete_requirement_from_list(struct req_data **list, int type, any_vnum vnum);
	void remove_room_template_from_table(room_template *rmt);
	
	quest_data *quest, *next_quest;
	progress_data *prg, *next_prg;
	room_data *room, *next_room;
	social_data *soc, *next_soc;
	shop_data *shop, *next_shop;
	descriptor_data *desc;
	room_template *rmt;
	bool found;
	int count;
	
	if (!(rmt = room_template_proto(vnum))) {
		msg_to_char(ch, "There is no such room template %d.\r\n", vnum);
		return;
	}
	
	if (HASH_COUNT(room_template_table) <= 1) {
		msg_to_char(ch, "You can't delete the last room template.\r\n");
		return;
	}
	
	// remove it first!
	remove_room_template_from_table(rmt);

	// save index and room template file now
	save_index(DB_BOOT_RMT);
	save_library_file_for_vnum(DB_BOOT_RMT, vnum);
	
	// update progress
	HASH_ITER(hh, progress_table, prg, next_prg) {
		found = delete_requirement_from_list(&PRG_TASKS(prg), REQ_VISIT_ROOM_TEMPLATE, vnum);
		
		if (found) {
			SET_BIT(PRG_FLAGS(prg), PRG_IN_DEVELOPMENT);
			save_library_file_for_vnum(DB_BOOT_PRG, PRG_VNUM(prg));
			need_progress_refresh = TRUE;
		}
	}
	
	// update quests
	HASH_ITER(hh, quest_table, quest, next_quest) {
		found = delete_quest_giver_from_list(&QUEST_STARTS_AT(quest), QG_ROOM_TEMPLATE, vnum);
		found |= delete_quest_giver_from_list(&QUEST_ENDS_AT(quest), QG_ROOM_TEMPLATE, vnum);
		found |= delete_requirement_from_list(&QUEST_TASKS(quest), REQ_VISIT_ROOM_TEMPLATE, vnum);
		found |= delete_requirement_from_list(&QUEST_PREREQS(quest), REQ_VISIT_ROOM_TEMPLATE, vnum);
		
		if (found) {
			SET_BIT(QUEST_FLAGS(quest), QST_IN_DEVELOPMENT);
			save_library_file_for_vnum(DB_BOOT_QST, QUEST_VNUM(quest));
		}
	}
	
	// update shops
	HASH_ITER(hh, shop_table, shop, next_shop) {
		found = delete_quest_giver_from_list(&SHOP_LOCATIONS(shop), QG_ROOM_TEMPLATE, vnum);
		
		if (found) {
			SET_BIT(SHOP_FLAGS(shop), SHOP_IN_DEVELOPMENT);
			save_library_file_for_vnum(DB_BOOT_SHOP, SHOP_VNUM(shop));
		}
	}
	
	// update socials
	HASH_ITER(hh, social_table, soc, next_soc) {
		found = delete_requirement_from_list(&SOC_REQUIREMENTS(soc), REQ_VISIT_ROOM_TEMPLATE, vnum);
		
		if (found) {
			SET_BIT(SOC_FLAGS(soc), SOC_IN_DEVELOPMENT);
			save_library_file_for_vnum(DB_BOOT_SOC, SOC_VNUM(soc));
		}
	}
	
	// update world
	count = 0;
	for (room = interior_room_list; room; room = next_room) {
		next_room = room->next_interior;
		
		if (ROOM_TEMPLATE_VNUM(room) == vnum) {
			delete_room(room, FALSE);	// must call check_all_exits
			++count;
		}
	}
	
	if (count > 0) {
		check_all_exits();
	}
	
	// remove templates from active editors
	for (desc = descriptor_list; desc; desc = desc->next) {
		if (GET_OLC_PROGRESS(desc)) {
			found = delete_requirement_from_list(&PRG_TASKS(GET_OLC_PROGRESS(desc)), REQ_VISIT_ROOM_TEMPLATE, vnum);
		
			if (found) {
				SET_BIT(QUEST_FLAGS(GET_OLC_PROGRESS(desc)), PRG_IN_DEVELOPMENT);
				msg_to_desc(desc, "A room template used by the progression goal you're editing has been deleted.\r\n");
			}
		}
		if (GET_OLC_QUEST(desc)) {
			found = delete_quest_giver_from_list(&QUEST_STARTS_AT(GET_OLC_QUEST(desc)), QG_ROOM_TEMPLATE, vnum);
			found |= delete_quest_giver_from_list(&QUEST_ENDS_AT(GET_OLC_QUEST(desc)), QG_ROOM_TEMPLATE, vnum);
			found |= delete_requirement_from_list(&QUEST_TASKS(GET_OLC_QUEST(desc)), REQ_VISIT_ROOM_TEMPLATE, vnum);
			found |= delete_requirement_from_list(&QUEST_PREREQS(GET_OLC_QUEST(desc)), REQ_VISIT_ROOM_TEMPLATE, vnum);
		
			if (found) {
				SET_BIT(QUEST_FLAGS(GET_OLC_QUEST(desc)), QST_IN_DEVELOPMENT);
				msg_to_desc(desc, "A room template used by the quest you are editing was deleted.\r\n");
			}
		}
		if (GET_OLC_SHOP(desc)) {
			found = delete_quest_giver_from_list(&SHOP_LOCATIONS(GET_OLC_SHOP(desc)), QG_ROOM_TEMPLATE, vnum);
		
			if (found) {
				SET_BIT(SHOP_FLAGS(GET_OLC_SHOP(desc)), SHOP_IN_DEVELOPMENT);
				msg_to_desc(desc, "A room template used by the shop you are editing was deleted.\r\n");
			}
		}
		if (GET_OLC_SOCIAL(desc)) {
			found = delete_requirement_from_list(&SOC_REQUIREMENTS(GET_OLC_SOCIAL(desc)), REQ_VISIT_ROOM_TEMPLATE, vnum);
		
			if (found) {
				SET_BIT(SOC_FLAGS(GET_OLC_SOCIAL(desc)), SOC_IN_DEVELOPMENT);
				msg_to_desc(desc, "A room template required by the social you are editing was deleted.\r\n");
			}
		}
	}
		
	syslog(SYS_OLC, GET_INVIS_LEV(ch), TRUE, "OLC: %s has deleted room template %d", GET_NAME(ch), vnum);
	msg_to_char(ch, "Room template %d deleted.\r\n", vnum);
	
	if (count > 0) {
		msg_to_char(ch, "%d live rooms deleted.\r\n", count);
	}
	
	free_room_template(rmt);
}


/**
* Searches for all uses of a crop and displays them.
*
* @param char_data *ch The player.
* @param crop_vnum vnum The crop vnum.
*/
void olc_search_room_template(char_data *ch, rmt_vnum vnum) {
	extern bool find_quest_giver_in_list(struct quest_giver *list, int type, any_vnum vnum);
	extern bool find_requirement_in_list(struct req_data *list, int type, any_vnum vnum);
	
	char buf[MAX_STRING_LENGTH];
	room_template *rmt = room_template_proto(vnum), *iter, *next_iter;
	quest_data *quest, *next_quest;
	progress_data *prg, *next_prg;
	social_data *soc, *next_soc;
	shop_data *shop, *next_shop;
	struct exit_template *ex;
	obj_data *obj, *next_obj;
	int size, found;
	bool any;
	
	if (!rmt) {
		msg_to_char(ch, "There is no room template %d.\r\n", vnum);
		return;
	}
	
	found = 0;
	size = snprintf(buf, sizeof(buf), "Occurrences of room template %d (%s):\r\n", vnum, GET_RMT_TITLE(rmt));
	
	// objects
	HASH_ITER(hh, object_table, obj, next_obj) {
		if (IS_PORTAL(obj) && GET_PORTAL_TARGET_VNUM(obj) == vnum) {
			++found;
			size += snprintf(buf + size, sizeof(buf) - size, "OBJ [%5d] %s\r\n", GET_OBJ_VNUM(obj), GET_OBJ_SHORT_DESC(obj));
		}
	}
	
	// progress
	HASH_ITER(hh, progress_table, prg, next_prg) {
		if (size >= sizeof(buf)) {
			break;
		}
		// REQ_x: requirement search
		any = find_requirement_in_list(PRG_TASKS(prg), REQ_VISIT_ROOM_TEMPLATE, vnum);
		
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
		any = find_quest_giver_in_list(QUEST_STARTS_AT(quest), QG_ROOM_TEMPLATE, vnum);
		any |= find_quest_giver_in_list(QUEST_ENDS_AT(quest), QG_ROOM_TEMPLATE, vnum);
		any |= find_requirement_in_list(QUEST_TASKS(quest), REQ_VISIT_ROOM_TEMPLATE, vnum);
		any |= find_requirement_in_list(QUEST_PREREQS(quest), REQ_VISIT_ROOM_TEMPLATE, vnum);
		
		if (any) {
			++found;
			size += snprintf(buf + size, sizeof(buf) - size, "QST [%5d] %s\r\n", QUEST_VNUM(quest), QUEST_NAME(quest));
		}
	}
	
	// shops
	HASH_ITER(hh, shop_table, shop, next_shop) {
		if (size >= sizeof(buf)) {
			break;
		}
		any = find_quest_giver_in_list(SHOP_LOCATIONS(shop), QG_ROOM_TEMPLATE, vnum);
		
		if (any) {
			++found;
			size += snprintf(buf + size, sizeof(buf) - size, "SHOP [%5d] %s\r\n", SHOP_VNUM(shop), SHOP_NAME(shop));
		}
	}

	// room templates
	HASH_ITER(hh, room_template_table, iter, next_iter) {
		any = FALSE;
		for (ex = GET_RMT_EXITS(iter); ex && !any; ex = ex->next) {
			if (ex->target_room == vnum) {
				any = TRUE;
				++found;
				size += snprintf(buf + size, sizeof(buf) - size, "RMT [%5d] %s\r\n", GET_RMT_VNUM(iter), GET_RMT_TITLE(iter));
			}
		}
	}
	
	// socials
	HASH_ITER(hh, social_table, soc, next_soc) {
		if (size >= sizeof(buf)) {
			break;
		}
		any = find_requirement_in_list(SOC_REQUIREMENTS(soc), REQ_VISIT_ROOM_TEMPLATE, vnum);
		
		if (any) {
			++found;
			size += snprintf(buf + size, sizeof(buf) - size, "SOC [%5d] %s\r\n", SOC_VNUM(soc), SOC_NAME(soc));
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
* @param room_template *rmt The room template to check.
* @param int dir Which direction to look for.
* @return bool TRUE if rmt has an exit in that direction.
*/
bool rmt_has_exit(room_template *rmt, int dir) {
	struct exit_template *ex;
	
	LL_FOREACH(GET_RMT_EXITS(rmt), ex) {
		if (ex->dir == dir) {
			return TRUE;
		}
	}
	
	return FALSE;
}


/**
* Function to save a player's changes to a room template (or a new one).
*
* @param descriptor_data *desc The descriptor who is saving.
*/
void save_olc_room_template(descriptor_data *desc) {
	void prune_extra_descs(struct extra_descr_data **list);
	
	room_template *proto, *rmt = GET_OLC_ROOM_TEMPLATE(desc);
	rmt_vnum vnum = GET_OLC_VNUM(desc);
	struct adventure_spawn *spawn;
	struct trig_proto_list *trig;
	struct exit_template *ex;
	struct quest_lookup *ql;
	struct shop_lookup *sl;
	UT_hash_handle hh;
	
	// have a place to save it?
	if (!(proto = room_template_proto(vnum))) {
		proto = create_room_template_table_entry(vnum);
	}
	
	// free prototype strings and pointers
	if (GET_RMT_TITLE(proto)) {
		free(GET_RMT_TITLE(proto));
	}
	if (GET_RMT_DESC(proto)) {
		free(GET_RMT_DESC(proto));
	}
	while ((spawn = GET_RMT_SPAWNS(proto))) {
		GET_RMT_SPAWNS(proto) = spawn->next;
		free(spawn);
	}
	while ((ex = GET_RMT_EXITS(proto))) {
		GET_RMT_EXITS(proto) = ex->next;
		free_exit_template(ex);
	}
	free_extra_descs(&GET_RMT_EX_DESCS(proto));
	free_interactions(&GET_RMT_INTERACTIONS(proto));
	while ((trig = GET_RMT_SCRIPTS(proto))) {
		GET_RMT_SCRIPTS(proto) = trig->next;
		free(trig);
	}
	
	// sanity
	prune_extra_descs(&GET_RMT_EX_DESCS(rmt));
	if (!GET_RMT_TITLE(rmt) || !*GET_RMT_TITLE(rmt)) {
		if (GET_RMT_TITLE(rmt)) {
			free(GET_RMT_TITLE(rmt));
		}
		GET_RMT_TITLE(rmt) = str_dup("Unnamed Room");
	}
	if (!GET_RMT_DESC(rmt) || !*GET_RMT_DESC(rmt)) {
		if (GET_RMT_DESC(rmt)) {
			free(GET_RMT_DESC(rmt));
		}
		GET_RMT_DESC(rmt) = str_dup("");
	}

	// save data back over the proto-type
	hh = proto->hh;	// save old hash handle
	ql = proto->quest_lookups;	// save lookups
	sl = proto->shop_lookups;
	
	*proto = *rmt;	// copy over all data
	proto->vnum = vnum;	// ensure correct vnum
	
	proto->hh = hh;	// restore old hash handle
	proto->quest_lookups = ql;	// restore lookups
	proto->shop_lookups = sl;
	
	// and save to file
	save_library_file_for_vnum(DB_BOOT_RMT, vnum);
}


/**
* Creates a copy of a room template, or clears a new one, for editing.
* 
* @param room_template *input The room template to copy, or NULL to make a new one.
* @return room_template* The copied room template.
*/
room_template *setup_olc_room_template(room_template *input) {
	extern struct extra_descr_data *copy_extra_descs(struct extra_descr_data *list);
	
	room_template *new;
	struct adventure_spawn *old_spawn, *new_spawn, *last_spawn;
	struct exit_template *old_ex, *new_ex, *last_ex;
	
	CREATE(new, room_template, 1);
	init_room_template(new);
	
	if (input) {
		// copy normal data
		*new = *input;

		// copy things that are pointers
		GET_RMT_TITLE(new) = GET_RMT_TITLE(input) ? str_dup(GET_RMT_TITLE(input)) : NULL;
		GET_RMT_DESC(new) = GET_RMT_DESC(input) ? str_dup(GET_RMT_DESC(input)) : NULL;
		
		// copy extra descs
		GET_RMT_EX_DESCS(new) = copy_extra_descs(GET_RMT_EX_DESCS(input));
		
		// copy spawns
		GET_RMT_SPAWNS(new) = NULL;
		last_spawn = NULL;
		for (old_spawn = GET_RMT_SPAWNS(input); old_spawn; old_spawn = old_spawn->next) {
			CREATE(new_spawn, struct adventure_spawn, 1);
			*new_spawn = *old_spawn;
			new_spawn->next = NULL;
			
			if (last_spawn) {
				last_spawn->next = new_spawn;
			}
			else {
				GET_RMT_SPAWNS(new) = new_spawn;
			}
			last_spawn = new_spawn;
		}
		
		// copy exits
		GET_RMT_EXITS(new) = NULL;
		last_ex = NULL;
		for (old_ex = GET_RMT_EXITS(input); old_ex; old_ex = old_ex->next) {
			CREATE(new_ex, struct exit_template, 1);
			*new_ex = *old_ex;
			if (old_ex->keyword) {
				new_ex->keyword = str_dup(old_ex->keyword);
			}
			new_ex->next = NULL;
			
			if (last_ex) {
				last_ex->next = new_ex;
			}
			else {
				GET_RMT_EXITS(new) = new_ex;
			}
			last_ex = new_ex;
		}
		
		// copy interactions
		GET_RMT_INTERACTIONS(new) = copy_interaction_list(GET_RMT_INTERACTIONS(input));
		
		// scripts
		GET_RMT_SCRIPTS(new) = copy_trig_protos(GET_RMT_SCRIPTS(input));
	}
	else {
		// brand new: some defaults
		GET_RMT_TITLE(new) = str_dup(default_rmt_title);
	}
	
	// done
	return new;	
}


/**
* Room templates have their own conditions: must be part of some adventure.
*
* @param rmt_vnum vnum The prospective vnum (need not exist).
* @return bool TRUE if the vnum is ok, FALSE if not.
*/
bool valid_room_template_vnum(rmt_vnum vnum) {
	adv_data *adv = get_adventure_for_vnum(vnum);
	return adv ? TRUE : FALSE;
}


 //////////////////////////////////////////////////////////////////////////////
//// DISPLAYS ////////////////////////////////////////////////////////////////

/**
* Displays the exit templates from a given list.
*
* @param struct exit_template *list Pointer to the start of a list of exits.
* @param char *save_buffer A buffer to store the result to.
*/
void get_exit_template_display(struct exit_template *list, char *save_buffer) {
	struct exit_template *ex;
	char lbuf[MAX_STRING_LENGTH], lbuf1[MAX_STRING_LENGTH], lbuf2[MAX_STRING_LENGTH];
	int count = 0;
	room_template *rmt;
	
	*save_buffer = '\0';
	
	for (ex = list; ex; ex = ex->next) {
		// lbuf: kw
		if (ex->keyword && *ex->keyword) {
			sprintf(lbuf, " keyword: \"%s\"", ex->keyword);
		}
		else {
			*lbuf = '\0';
		}
		
		// lbuf1: flags
		if (ex->exit_info) {
			sprintbit(ex->exit_info, exit_bits, lbuf2, TRUE);
			sprintf(lbuf1, " ( %s)", lbuf2);
		}
		else {
			*lbuf1 = '\0';
		}
		
		// lbuf2: target name
		if ((rmt = room_template_proto(ex->target_room))) {
			strcpy(lbuf2, GET_RMT_TITLE(rmt));
		}
		else {
			strcpy(lbuf2, "UNKNOWN");
		}
		
		sprintf(save_buffer + strlen(save_buffer), "%2d. %s: [%d] %s%s%s\r\n", ++count, dirs[ex->dir], ex->target_room, lbuf2, lbuf1, lbuf);
	}
	
	if (count == 0) {
		strcat(save_buffer, " none\r\n");
	}
}


/**
* Gets the name of an item from an adventure_spawn (mob, obj, etc).
*
* @param struct adventure_spawn *spawn The spawn item.
* @param char *save_buffer A string buffer to save to.
*/
void get_spawn_template_name(struct adventure_spawn *spawn, char *save_buffer) {
	switch (spawn->type) {
		case ADV_SPAWN_MOB: {
			strcpy(save_buffer, skip_filler(get_mob_name_by_proto(spawn->vnum)));
			break;
		}
		case ADV_SPAWN_OBJ: {
			strcpy(save_buffer, skip_filler(get_obj_name_by_proto(spawn->vnum)));
			break;
		}
		case ADV_SPAWN_VEH: {
			strcpy(save_buffer, skip_filler(get_vehicle_name_by_proto(spawn->vnum)));
			break;
		}
		default: {
			log("SYSERR: Unknown spawn type %d in get_spawn_template_name", spawn->type);
			strcpy(save_buffer, "???");
			break;
		}
	}
}


/**
* Displays the adventure spawn rules from a given list.
*
* @param struct adventure_spawn *list Pointer to the start of a list of spawns.
* @param char *save_buffer A buffer to store the result to.
*/
void get_template_spawns_display(struct adventure_spawn *list, char *save_buffer) {
	struct adventure_spawn *spawn;
	char lbuf[MAX_STRING_LENGTH];
	int count = 0;
	
	*save_buffer = '\0';
	
	for (spawn = list; spawn; spawn = spawn->next) {
		get_spawn_template_name(spawn, lbuf);	
		sprintf(save_buffer + strlen(save_buffer), "%2d. [%s] %d %s (%.2f%%, limit %d)\r\n", ++count, adventure_spawn_types[spawn->type], spawn->vnum, lbuf, spawn->percent, spawn->limit);
	}
	
	if (count == 0) {
		strcat(save_buffer, " none\r\n");
	}
}


/**
* This is the main recipe display for room template OLC. It displays the user's
* currently-edited template.
*
* @param char_data *ch The person who is editing a room template and will see its display.
*/
void olc_show_room_template(char_data *ch) {
	void get_extra_desc_display(struct extra_descr_data *list, char *save_buffer);
	void get_interaction_display(struct interaction_item *list, char *save_buffer);
	void get_script_display(struct trig_proto_list *list, char *save_buffer);
	
	room_template *rmt = GET_OLC_ROOM_TEMPLATE(ch->desc);
	char lbuf[MAX_STRING_LENGTH];
	
	adv_data *adv = get_adventure_for_vnum(GET_OLC_VNUM(ch->desc));
	
	if (!rmt) {
		return;
	}
	
	*buf = '\0';
	
	sprintf(buf + strlen(buf), "[%s%d\t0] %s%s\t0\r\n", OLC_LABEL_CHANGED, GET_OLC_VNUM(ch->desc), OLC_LABEL_UNCHANGED, !room_template_proto(GET_RMT_VNUM(rmt)) ? "new room template" : GET_RMT_TITLE(room_template_proto(GET_RMT_VNUM(rmt))));
	sprintf(buf + strlen(buf), "Adventure: %d %s%s\t0\r\n", adv ? GET_ADV_VNUM(adv) : NOTHING, OLC_LABEL_CHANGED, adv ? GET_ADV_NAME(adv) : "none");
	sprintf(buf + strlen(buf), "<%stitle\t0> %s\r\n", OLC_LABEL_STR(GET_RMT_TITLE(rmt), default_rmt_title), NULLSAFE(GET_RMT_TITLE(rmt)));
	sprintf(buf + strlen(buf), "<%sdescription\t0>\r\n%s", OLC_LABEL_STR(GET_RMT_DESC(rmt), ""), NULLSAFE(GET_RMT_DESC(rmt)));
	
	sprintbit(GET_RMT_FLAGS(rmt), room_template_flags, lbuf, TRUE);
	sprintf(buf + strlen(buf), "<%sflags\t0> %s\r\n", OLC_LABEL_VAL(GET_RMT_FLAGS(rmt), NOBITS), lbuf);
	
	sprintbit(GET_RMT_FUNCTIONS(rmt), function_flags, lbuf, TRUE);
	sprintf(buf + strlen(buf), "<%sfunctions\t0> %s\r\n", OLC_LABEL_VAL(GET_RMT_FUNCTIONS(rmt), NOBITS), lbuf);
	
	sprintbit(GET_RMT_BASE_AFFECTS(rmt), room_aff_bits, lbuf, TRUE);
	sprintf(buf + strlen(buf), "<%saffects\t0> %s\r\n", OLC_LABEL_VAL(GET_RMT_BASE_AFFECTS(rmt), NOBITS), lbuf);
	
	// exits
	sprintf(buf + strlen(buf), "Exits: <%sexit\t0>, <%smatchexits\t0>\r\n", OLC_LABEL_PTR(GET_RMT_EXITS(rmt)), OLC_LABEL_PTR(GET_RMT_EXITS(rmt)));
	if (GET_RMT_EXITS(rmt)) {
		get_exit_template_display(GET_RMT_EXITS(rmt), lbuf);
		strcat(buf, lbuf);
	}
	
	// exdesc
	sprintf(buf + strlen(buf), "Extra descriptions: <%sextra\t0>\r\n", OLC_LABEL_PTR(GET_RMT_EX_DESCS(rmt)));
	if (GET_RMT_EX_DESCS(rmt)) {
		get_extra_desc_display(GET_RMT_EX_DESCS(rmt), buf1);
		strcat(buf, buf1);
	}
	
	sprintf(buf + strlen(buf), "Interactions: <%sinteraction\t0>\r\n", OLC_LABEL_PTR(GET_RMT_INTERACTIONS(rmt)));
	if (GET_RMT_INTERACTIONS(rmt)) {
		get_interaction_display(GET_RMT_INTERACTIONS(rmt), buf1);
		strcat(buf, buf1);
	}
	
	// spawns
	sprintf(buf + strlen(buf), "Spawns: <%sspawns\t0>\r\n", OLC_LABEL_PTR(GET_RMT_SPAWNS(rmt)));
	if (GET_RMT_SPAWNS(rmt)) {
		get_template_spawns_display(GET_RMT_SPAWNS(rmt), lbuf);
		strcat(buf, lbuf);
	}
	
	// scripts
	sprintf(buf + strlen(buf), "Scripts: <%sscript\t0>\r\n", OLC_LABEL_PTR(GET_RMT_SCRIPTS(rmt)));
	if (GET_RMT_SCRIPTS(rmt)) {
		get_script_display(GET_RMT_SCRIPTS(rmt), lbuf);
		strcat(buf, lbuf);
	}
	
	page_string(ch->desc, buf, TRUE);
}


 //////////////////////////////////////////////////////////////////////////////
//// EDIT MODULES ////////////////////////////////////////////////////////////

OLC_MODULE(rmedit_affects) {
	room_template *rmt = GET_OLC_ROOM_TEMPLATE(ch->desc);
	GET_RMT_BASE_AFFECTS(rmt) = olc_process_flag(ch, argument, "room affect", "affects", room_aff_bits, GET_RMT_BASE_AFFECTS(rmt));
}


OLC_MODULE(rmedit_description) {
	room_template *rmt = GET_OLC_ROOM_TEMPLATE(ch->desc);
	
	if (ch->desc->str) {
		msg_to_char(ch, "You are already editing a string.\r\n");
	}
	else {
		sprintf(buf, "description for %s", GET_RMT_TITLE(rmt));
		start_string_editor(ch->desc, buf, &(GET_RMT_DESC(rmt)), MAX_ROOM_DESCRIPTION, FALSE);
	}
}


OLC_MODULE(rmedit_exit) {
	room_template *rmt = GET_OLC_ROOM_TEMPLATE(ch->desc);
	char arg1[MAX_INPUT_LENGTH], dir_arg[MAX_INPUT_LENGTH], room_arg[MAX_INPUT_LENGTH], num_arg[MAX_INPUT_LENGTH], field_arg[MAX_INPUT_LENGTH];
	adv_data *adv = get_adventure_for_vnum(GET_OLC_VNUM(ch->desc));
	int num, vnum, dir;
	room_template *to_template;
	struct exit_template *ex, *temp, *change;
	bool found;
	
	// arg1 argument
	argument = any_one_arg(argument, arg1);
	skip_spaces(&argument);
	
	if (is_abbrev(arg1, "remove")) {
		if (!*argument) {
			msg_to_char(ch, "Remove which exit (number)?\r\n");
		}
		else if (!str_cmp(argument, "all")) {
			while ((ex = GET_RMT_EXITS(rmt))) {
				GET_RMT_EXITS(rmt) = ex->next;
				if (ex->keyword) {
					free(ex->keyword);
				}
				free(ex);
			}
			msg_to_char(ch, "You remove all the exits.\r\n");
		}
		else if (!isdigit(*argument) || (num = atoi(argument)) < 1) {
			msg_to_char(ch, "Invalid exit number.\r\n");
		}
		else {
			found = FALSE;
			for (ex = GET_RMT_EXITS(rmt); ex && !found; ex = ex->next) {
				if (--num == 0) {
					found = TRUE;
					
					msg_to_char(ch, "You remove the %s exit.\r\n", dirs[ex->dir]);
					REMOVE_FROM_LIST(ex, GET_RMT_EXITS(rmt), next);
					if (ex->keyword) {
						free(ex->keyword);
					}
					free(ex);
				}
			}
			
			if (!found) {
				msg_to_char(ch, "Invalid exit number.\r\n");
			}
		}
	}
	else if (is_abbrev(arg1, "add")) {
		// .exit add <dir> <room template> [keywords if door]
		argument = any_one_arg(argument, dir_arg);
		argument = any_one_arg(argument, room_arg);
		skip_spaces(&argument);	// keywords?
		
		if (!*dir_arg || !*room_arg) {
			msg_to_char(ch, "Usage: exit add <dir> <room template vnum> [keywords if door]\r\n");
		}
		else if ((dir = parse_direction(ch, dir_arg)) == NO_DIR) {
			msg_to_char(ch, "Invalid direction '%s'.\r\n", dir_arg);
		}
		else if (!isdigit(*room_arg) || (vnum = atoi(room_arg)) < 0 || get_adventure_for_vnum(vnum) != adv) {
			msg_to_char(ch, "Invalid room template vnum '%s'; target room must be part of the same adventure zone.\r\n", room_arg);
		}
		else {
			CREATE(ex, struct exit_template, 1);
			
			ex->dir = dir;
			ex->target_room = vnum;
			ex->keyword = *argument ? str_dup(argument) : NULL;
			ex->exit_info = (*argument ? EX_ISDOOR | EX_CLOSED : NOBITS);
			ex->next = NULL;
			
			// append to end
			if ((temp = GET_RMT_EXITS(rmt)) != NULL) {
				while (temp->next) {
					temp = temp->next;
				}
				temp->next = ex;
			}
			else {
				GET_RMT_EXITS(rmt) = ex;
			}
			
			to_template = room_template_proto(vnum);
			
			msg_to_char(ch, "You add an exit %s to %d %s%s%s.\r\n", dirs[dir], vnum, !to_template ? "UNKNOWN" : GET_RMT_TITLE(to_template), *argument ? ", with door keywords: " : "", *argument ? argument : "");
		}
	}
	else if (is_abbrev(arg1, "change")) {
		argument = any_one_arg(argument, num_arg);
		argument = any_one_arg(argument, field_arg);
		skip_spaces(&argument);	// value

		if (!*argument || !*num_arg || !*field_arg || !isdigit(*num_arg)) {
			msg_to_char(ch, "Usage: exit change <number> <field> <value>\r\n");
			return;
		}
		
		// find entry
		num = atoi(num_arg);
		change = NULL;
		for (ex = GET_RMT_EXITS(rmt); ex; ex = ex->next) {
			if (--num == 0) {
				change = ex;
				break;
			}
		}
		
		if (!change) {
			msg_to_char(ch, "Invalid exit number.\r\n");
		}
		else if (is_abbrev(field_arg, "direction")) {
			if ((dir = parse_direction(ch, argument)) == NO_DIR) {
				msg_to_char(ch, "Invalid direction '%s'.\r\n", argument);
			}
			else {
				change->dir = dir;
				msg_to_char(ch, "Exit %d direction changed to %s.\r\n", atoi(num_arg), dirs[dir]);
			}
		}
		else if (is_abbrev(field_arg, "room") || is_abbrev(field_arg, "template") || is_abbrev(field_arg, "vnum") || is_abbrev(field_arg, "target")) {
			if (!isdigit(*argument) || (vnum = atoi(argument)) < 0 || get_adventure_for_vnum(vnum) != adv) {
				msg_to_char(ch, "Invalid room template vnum '%s'; target room must be part of the same adventure zone.\r\n", argument);
			}
			else {
				change->target_room = vnum;
				to_template = room_template_proto(vnum);
				msg_to_char(ch, "Exit %d (%s) target changed to [%d] %s.\r\n", atoi(num_arg), dirs[change->dir], vnum, !to_template ? "UNKNOWN" : GET_RMT_TITLE(to_template));
			}
		}
		else if (is_abbrev(field_arg, "keywords")) {
			if (!*argument) {
				msg_to_char(ch, "Change the keywords to what (or 'none' to have no door)?\r\n");
			}
			else if (!str_cmp(argument, "none")) {
				if (ex->keyword) {
					free(ex->keyword);
				}
				ex->keyword = NULL;
				ex->exit_info = NOBITS;
				
				msg_to_char(ch, "Exit %d (%s) keywords and door removed.\r\n", atoi(num_arg), dirs[change->dir]);
			}
			else {
				if (ex->keyword) {
					free(ex->keyword);
				}
				ex->keyword = str_dup(argument);
				ex->exit_info = EX_ISDOOR | EX_CLOSED;
				
				msg_to_char(ch, "Exit %d (%s) door keywords set to: %s\r\n", atoi(num_arg), dirs[change->dir], argument);
			}
		}
		else {
			msg_to_char(ch, "You can change: direction, room, keywords\r\n");
		}
	}
	else {
		msg_to_char(ch, "Usage: exit add <dir> <room template vnum> [keywords if door]\r\n");
		msg_to_char(ch, "Usage: exit change <number> <field> <value>\r\n");
		msg_to_char(ch, "Usage: exit remove <number | all>\r\n");
	}
}


OLC_MODULE(rmedit_extra_desc) {
	room_template *rmt = GET_OLC_ROOM_TEMPLATE(ch->desc);
	olc_process_extra_desc(ch, argument, &GET_RMT_EX_DESCS(rmt));
}


OLC_MODULE(rmedit_interaction) {
	room_template *rmt = GET_OLC_ROOM_TEMPLATE(ch->desc);
	olc_process_interactions(ch, argument, &GET_RMT_INTERACTIONS(rmt), TYPE_ROOM);
}


OLC_MODULE(rmedit_flags) {
	room_template *rmt = GET_OLC_ROOM_TEMPLATE(ch->desc);
	GET_RMT_FLAGS(rmt) = olc_process_flag(ch, argument, "room template", "flags", room_template_flags, GET_RMT_FLAGS(rmt));
}


OLC_MODULE(rmedit_functions) {
	room_template *rmt = GET_OLC_ROOM_TEMPLATE(ch->desc);
	GET_RMT_FUNCTIONS(rmt) = olc_process_flag(ch, argument, "function", "functions", function_flags, GET_RMT_FUNCTIONS(rmt));
}


OLC_MODULE(rmedit_matchexits) {
	room_template *rmt = GET_OLC_ROOM_TEMPLATE(ch->desc);
	adv_data *adv = get_adventure_for_vnum(GET_OLC_VNUM(ch->desc));
	struct exit_template *ex;
	room_template *iter, *next_iter;
	bool found = FALSE;
	
	if (!adv) {
		msg_to_char(ch, "You cannot match exits on a room template that is outside any adventure zone.\r\n");
		return;
	}
	
	HASH_ITER(hh, room_template_table, iter, next_iter) {
		// bounds-check: same zone only; not same room
		if (GET_RMT_VNUM(iter) == GET_OLC_VNUM(ch->desc) || GET_RMT_VNUM(iter) < GET_ADV_START_VNUM(adv) || GET_RMT_VNUM(iter) > GET_ADV_END_VNUM(adv)) {
			continue;
		}
		
		for (ex = GET_RMT_EXITS(iter); ex; ex = ex->next) {
			if (ex->target_room == GET_OLC_VNUM(ch->desc)) {
				found |= match_one_exit(ch, rmt, iter, ex);
			}
		}
	}
	
	if (!found) {
		msg_to_char(ch, "No exits to match.\r\n");
	}
}


OLC_MODULE(rmedit_title) {
	room_template *rmt = GET_OLC_ROOM_TEMPLATE(ch->desc);
	olc_process_string(ch, argument, "title", &(GET_RMT_TITLE(rmt)));
}


OLC_MODULE(rmedit_script) {
	room_template *rmt = GET_OLC_ROOM_TEMPLATE(ch->desc);
	olc_process_script(ch, argument, &(GET_RMT_SCRIPTS(rmt)), WLD_TRIGGER);
}


OLC_MODULE(rmedit_spawns) {
	extern const char *olc_type_bits[NUM_OLC_TYPES+1];

	room_template *rmt = GET_OLC_ROOM_TEMPLATE(ch->desc);
	char arg1[MAX_INPUT_LENGTH], arg2[MAX_INPUT_LENGTH], arg3[MAX_INPUT_LENGTH];
	char type_arg[MAX_INPUT_LENGTH], num_arg[MAX_INPUT_LENGTH], prc_arg[MAX_INPUT_LENGTH];
	char lbuf[MAX_STRING_LENGTH];
	int num, stype, limit, findtype;
	struct adventure_spawn *spawn, *temp, *change, *copyfrom = NULL;
	double prc;
	any_vnum vnum;
	bool found;
	
	// arg1 argument
	argument = any_one_arg(argument, arg1);
	skip_spaces(&argument);
	
	if (is_abbrev(arg1, "copy")) {
		argument = any_one_arg(argument, arg2);
		argument = any_one_arg(argument, arg3);
		
		if (!*arg2 || !*arg3) {
			msg_to_char(ch, "Usage: spawn copy <from type> <from vnum>\r\n");
		}
		else if ((findtype = find_olc_type(arg2)) == 0) {
			msg_to_char(ch, "Unknown olc type '%s'.\r\n", arg2);
		}
		else if (!isdigit(*arg3)) {
			sprintbit(findtype, olc_type_bits, buf, FALSE);
			msg_to_char(ch, "Copy spawns from which %s?\r\n", buf);
		}
		else if ((vnum = atoi(arg3)) < 0) {
			msg_to_char(ch, "Invalid vnum.\r\n");
		}
		else {
			sprintbit(findtype, olc_type_bits, buf, FALSE);
			
			switch (findtype) {
				case OLC_ROOM_TEMPLATE: {
					room_template *rmt = room_template_proto(vnum);
					if (rmt) {
						copyfrom = GET_RMT_SPAWNS(rmt);
					}
					break;
				}
				default: {
					msg_to_char(ch, "You can't copy template spawns from %ss.\r\n", buf);
					return;
				}
			}
			
			if (!copyfrom) {
				msg_to_char(ch, "Invalid %s vnum '%s'.\r\n", buf, arg3);
			}
			else {
				smart_copy_template_spawns(&GET_RMT_SPAWNS(rmt), copyfrom);
				msg_to_char(ch, "Spawns copied from %s %d.\r\n", buf, vnum);
			}
		}
	}
	else if (is_abbrev(arg1, "remove")) {
		if (!*argument) {
			msg_to_char(ch, "Remove which spawn (number)?\r\n");
		}
		else if (!str_cmp(argument, "all")) {
			while ((spawn = GET_RMT_SPAWNS(rmt))) {
				GET_RMT_SPAWNS(rmt) = spawn->next;
				free(spawn);
			}
			msg_to_char(ch, "You remove all the spawns.\r\n");
		}
		else if (!isdigit(*argument) || (num = atoi(argument)) < 1) {
			msg_to_char(ch, "Invalid spawn number.\r\n");
		}
		else {
			found = FALSE;
			for (spawn = GET_RMT_SPAWNS(rmt); spawn && !found; spawn = spawn->next) {
				if (--num == 0) {
					found = TRUE;
					
					get_spawn_template_name(spawn, lbuf);
					msg_to_char(ch, "You remove the spawn info for %s.\r\n", lbuf);
					REMOVE_FROM_LIST(spawn, GET_RMT_SPAWNS(rmt), next);
					free(spawn);
				}
			}
			
			if (!found) {
				msg_to_char(ch, "Invalid spawn number.\r\n");
			}
		}
	}
	else if (is_abbrev(arg1, "add")) {
		argument = any_one_arg(argument, type_arg);
		argument = any_one_arg(argument, num_arg);
		argument = any_one_arg(argument, prc_arg);
		skip_spaces(&argument);	// limit
		
		if (!*type_arg || !*num_arg || !*prc_arg || !*argument) {
			msg_to_char(ch, "Usage: spawn add <type> <vnum> <percent> <limit>\r\n");
		}
		else if ((stype = search_block(type_arg, adventure_spawn_types, FALSE)) == NOTHING) {
			msg_to_char(ch, "Invalid type '%s'.\r\n", type_arg);
		}
		else if (!isdigit(*num_arg) || (vnum = atoi(num_arg)) < 0) {
			msg_to_char(ch, "Invalid vnum '%s'.\r\n", num_arg);
		}
		else if (stype == ADV_SPAWN_MOB && !mob_proto(vnum)) {
			msg_to_char(ch, "Invalid mobile vnum '%s'.\r\n", num_arg);
		}
		else if (stype == ADV_SPAWN_OBJ && !obj_proto(vnum)) {
			msg_to_char(ch, "Invalid object vnum '%s'.\r\n", num_arg);
		}
		else if (stype == ADV_SPAWN_VEH && !vehicle_proto(vnum)) {
			msg_to_char(ch, "Invalid vehicle vnum '%s'.\r\n", num_arg);
		}
		else if ((prc = atof(prc_arg)) < .01 || prc > 100.00) {
			msg_to_char(ch, "Percentage must be between .01 and 100; '%s' given.\r\n", prc_arg);
		}
		else if (!isdigit(*argument) || (limit = atoi(argument)) > MAX_INT || limit < -1) {
			msg_to_char(ch, "Invalid limit '%s'.\r\n", argument);
		}
		else {
			CREATE(spawn, struct adventure_spawn, 1);
			
			spawn->type = stype;
			spawn->vnum = vnum;
			spawn->percent = prc;
			spawn->limit = limit;
			spawn->next = NULL;
			
			// append to end
			if ((temp = GET_RMT_SPAWNS(rmt)) != NULL) {
				while (temp->next) {
					temp = temp->next;
				}
				temp->next = spawn;
			}
			else {
				GET_RMT_SPAWNS(rmt) = spawn;
			}
			
			get_spawn_template_name(spawn, lbuf);
			msg_to_char(ch, "You add spawn for %s %s (%d) at %.2f%%, limit %d.\r\n", adventure_spawn_types[stype], lbuf, vnum, prc, limit);
		}
	}
	else if (is_abbrev(arg1, "change")) {
		argument = any_one_arg(argument, num_arg);
		argument = any_one_arg(argument, type_arg);
		skip_spaces(&argument);	// value
		
		if (!*num_arg || !isdigit(*num_arg) || !*type_arg || !*argument) {
			msg_to_char(ch, "Usage: spawn change <number> <vnum | percent | limit> <value>\r\n");
			return;
		}
		
		// find which one to change
		num = atoi(num_arg);
		change = NULL;
		for (spawn = GET_RMT_SPAWNS(rmt); spawn && !change; spawn = spawn->next) {
			if (--num == 0) {
				change = spawn;
				break;
			}
		}
		
		if (!change) {
			msg_to_char(ch, "Invalid spawn number.\r\n");
		}
		else if (is_abbrev(type_arg, "vnum")) {
			if (!isdigit(*argument) || (vnum = atoi(argument)) < 0) {
				msg_to_char(ch, "Invalid vnum '%s'.\r\n", argument);
			}
			else if (change->type == ADV_SPAWN_MOB && !mob_proto(vnum)) {
				msg_to_char(ch, "Invalid mobile vnum '%s'.\r\n", argument);
			}
			else if (change->type == ADV_SPAWN_OBJ && !obj_proto(vnum)) {
				msg_to_char(ch, "Invalid object vnum '%s'.\r\n", argument);
			}
			else if (change->type == ADV_SPAWN_VEH && !vehicle_proto(vnum)) {
				msg_to_char(ch, "Invalid vehicle vnum '%s'.\r\n", argument);
			}
			else {
				change->vnum = vnum;
				get_spawn_template_name(change, lbuf);
				msg_to_char(ch, "Spawn %d changed to vnum %d (%s).\r\n", atoi(num_arg), vnum, lbuf);
			}
		}
		else if (is_abbrev(type_arg, "percent")) {
			prc = atof(argument);
			
			if (prc < .01 || prc > 100.00) {
				msg_to_char(ch, "Percentage must be between .01 and 100; '%s' given.\r\n", argument);
			}
			else {
				change->percent = prc;
				msg_to_char(ch, "Spawn %d changed to %.2f percent.\r\n", atoi(num_arg), prc);
			}
		}
		else if (is_abbrev(type_arg, "limit")) {
			if (!isdigit(*argument) || (limit = atoi(argument)) < -1 || limit > MAX_INT) {
				msg_to_char(ch, "Invalid limit '%s'.\r\n", argument);
			}
			else {
				change->limit = limit;
				msg_to_char(ch, "Spawn %d changed to limit %d.\r\n", atoi(num_arg), limit);
			}
		}
		else {
			msg_to_char(ch, "You can only change the vnum, percent, or limit.\r\n");
		}
	}
	else {
		msg_to_char(ch, "Usage: spawn add <type> <vnum> <percent> <limit>\r\n");
		msg_to_char(ch, "Usage: spawn change <number> <vnum | percent | limit> <value>\r\n");
		msg_to_char(ch, "Usage: spawn copy <from type> <from vnum>\r\n");
		msg_to_char(ch, "Usage: spawn remove <number | all>\r\n");
	}
}
