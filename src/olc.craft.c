/* ************************************************************************
*   File: olc.craft.c                                     EmpireMUD 2.0b5 *
*  Usage: OLC for craft recipes                                           *
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

/**
* Contents:
*   Helpers
*   Displays
*   Edit Modules
*/

// externs
extern const char *apply_types[];
extern const char *bld_on_flags[];
extern const char *craft_flags[];
extern const char *craft_types[];
extern const char *drinks[];
extern const char *road_types[];

// external funcs
void init_craft(craft_data *craft);

// locals


 //////////////////////////////////////////////////////////////////////////////
//// HELPERS /////////////////////////////////////////////////////////////////

/**
* Checks for common craft recipe problems and reports them to ch.
*
* @param craft_data *craft The item to audit.
* @param char_data *ch The person to report to.
* @return bool TRUE if any problems were reported; FALSE if all good.
*/
bool audit_craft(craft_data *craft, char_data *ch) {
	char temp[MAX_STRING_LENGTH];
	bool problem = FALSE;
	int count;

	if (GET_CRAFT_REQUIRES_OBJ(craft) == NOTHING && GET_CRAFT_ABILITY(craft) == NO_ABIL) {
		olc_audit_msg(ch, GET_CRAFT_VNUM(craft), "Craft requires no object or ability");
		problem = TRUE;
	}
	if (IS_SET(GET_CRAFT_FLAGS(craft), CRAFT_IN_DEVELOPMENT)) {
		olc_audit_msg(ch, GET_CRAFT_VNUM(craft), "IN-DEVELOPMENT");
		problem = TRUE;
	}
	if (!IS_SET(GET_CRAFT_FLAGS(craft), CRAFT_APIARIES | CRAFT_GLASS) && !GET_CRAFT_RESOURCES(craft)) {
		olc_audit_msg(ch, GET_CRAFT_VNUM(craft), "Craft requires no resources");
		problem = TRUE;
	}
	if (!str_cmp(GET_CRAFT_NAME(craft), "unnamed recipe")) {
		olc_audit_msg(ch, GET_CRAFT_VNUM(craft), "Craft not named");
		problem = TRUE;
	}
	
	strcpy(temp, GET_CRAFT_NAME(craft));
	strtolower(temp);
	if (strcmp(GET_CRAFT_NAME(craft), temp)) {
		olc_audit_msg(ch, GET_CRAFT_VNUM(craft), "Non-lowercase name");
		problem = TRUE;
	}
	
	if (GET_CRAFT_TYPE(craft) == CRAFT_TYPE_ERROR) {
		olc_audit_msg(ch, GET_CRAFT_VNUM(craft), "Craft type not set");
		problem = TRUE;
	}
	
	// different types of crafts
	if (GET_CRAFT_TYPE(craft) == CRAFT_TYPE_BUILD) {	// buildings only
		if (GET_CRAFT_BUILD_TYPE(craft) == NOTHING || !building_proto(GET_CRAFT_BUILD_TYPE(craft))) {
			olc_audit_msg(ch, GET_CRAFT_VNUM(craft), "Craft makes nothing");
			problem = TRUE;
		}
		if (GET_CRAFT_BUILD_TYPE(craft) != NOTHING && GET_CRAFT_BUILD_TYPE(craft) != GET_CRAFT_VNUM(craft)) {
			olc_audit_msg(ch, GET_CRAFT_VNUM(craft), "Craft creates building with different vnum");
			problem = TRUE;
		}
	}
	else if (CRAFT_FLAGGED(craft, CRAFT_VEHICLE)) {	// vehicles only
		if (GET_CRAFT_OBJECT(craft) == NOTHING || !vehicle_proto(GET_CRAFT_OBJECT(craft))) {
			olc_audit_msg(ch, GET_CRAFT_VNUM(craft), "Craft makes nothing");
			problem = TRUE;
		}
		if (GET_CRAFT_OBJECT(craft) != NOTHING && GET_CRAFT_OBJECT(craft) != GET_CRAFT_VNUM(craft)) {
			olc_audit_msg(ch, GET_CRAFT_VNUM(craft), "Craft creates vehicle with different vnum");
			problem = TRUE;
		}
	}
	else if (CRAFT_FLAGGED(craft, CRAFT_SOUP)) {	// soups only
		if (GET_CRAFT_OBJECT(craft) < 0 || GET_CRAFT_OBJECT(craft) > NUM_LIQUIDS) {
			olc_audit_msg(ch, GET_CRAFT_VNUM(craft), "Invalid liquid type on soup recipe");
			problem = TRUE;
		}
	}
	else {	// normal craft (not special type))
		if (GET_CRAFT_OBJECT(craft) == NOTHING || !obj_proto(GET_CRAFT_OBJECT(craft))) {
			olc_audit_msg(ch, GET_CRAFT_VNUM(craft), "Craft makes nothing");
			problem = TRUE;
		}
		if (GET_CRAFT_OBJECT(craft) != NOTHING && GET_CRAFT_OBJECT(craft) != GET_CRAFT_VNUM(craft)) {
			olc_audit_msg(ch, GET_CRAFT_VNUM(craft), "Craft creates item with different vnum");
			problem = TRUE;
		}
	}
	
	count = (CRAFT_FLAGGED(craft, CRAFT_SOUP) ? 1 : 0) + (CRAFT_FLAGGED(craft, CRAFT_VEHICLE) ? 1 : 0) + ((GET_CRAFT_TYPE(craft) == CRAFT_TYPE_BUILD) ? 1 : 0);
	if (count > 1) {
		olc_audit_msg(ch, GET_CRAFT_VNUM(craft), "Unusual combination of SOUP, VEHICLE, BUILD");
		problem = TRUE;
	}
	
	// anything not a building
	if (GET_CRAFT_TYPE(craft) != CRAFT_TYPE_BUILD) {
		if (GET_CRAFT_QUANTITY(craft) == 0) {
			olc_audit_msg(ch, GET_CRAFT_VNUM(craft), "Craft creates 0 quantity");
			problem = TRUE;
		}
		if (GET_CRAFT_TIME(craft) == 0) {
			olc_audit_msg(ch, GET_CRAFT_VNUM(craft), "Craft requires 0 time");
			problem = TRUE;
		}
	}
		
	return problem;
}


/**
* Creates a new craft table entry.
* 
* @param craft_vnum vnum The number to create.
* @return craft_data* The new recipe's prototypes.
*/
craft_data *create_craft_table_entry(craft_vnum vnum) {
	void add_craft_to_table(craft_data *craft);
	
	craft_data *craft;
	
	// sanity
	if (craft_proto(vnum)) {
		log("SYSERR: Attempting to insert craft at existing vnum %d", vnum);
		return craft_proto(vnum);
	}
	
	CREATE(craft, craft_data, 1);
	init_craft(craft);
	GET_CRAFT_VNUM(craft) = vnum;
	add_craft_to_table(craft);

	// save index and craft file now
	save_index(DB_BOOT_CRAFT);
	save_library_file_for_vnum(DB_BOOT_CRAFT, vnum);

	return craft;
}


/**
* For the .list command.
*
* @param craft_data *craft The thing to list.
* @param bool detail If TRUE, provide additional details
* @return char* The line to show (without a CRLF).
*/
char *list_one_craft(craft_data *craft, bool detail) {
	static char output[MAX_STRING_LENGTH];
	
	if (detail) {
		snprintf(output, sizeof(output), "[%5d] %s", GET_CRAFT_VNUM(craft), GET_CRAFT_NAME(craft));
	}
	else {
		snprintf(output, sizeof(output), "[%5d] %s", GET_CRAFT_VNUM(craft), GET_CRAFT_NAME(craft));
	}
	
	return output;
}


/**
* WARNING: This function actually deletes a craft recipe.
*
* @param char_data *ch The person doing the deleting.
* @param craft_vnum vnum The vnum to delete.
*/
void olc_delete_craft(char_data *ch, craft_vnum vnum) {
	void cancel_gen_craft(char_data *ch);
	void remove_craft_from_table(craft_data *craft);
	
	craft_data *craft;
	char_data *iter;
	
	if (!(craft = craft_proto(vnum))) {
		msg_to_char(ch, "There is no such craft %d.\r\n", vnum);
		return;
	}
	
	if (HASH_COUNT(craft_table) <= 1) {
		msg_to_char(ch, "You can't delete the last craft.\r\n");
		return;
	}
	
	// find players who are crafting it and stop them (BEFORE removing from table)
	for (iter = character_list; iter; iter = iter->next) {
		if (!IS_NPC(iter) && GET_ACTION(iter) == ACT_GEN_CRAFT && GET_ACTION_VNUM(iter, 0) == GET_CRAFT_VNUM(craft)) {
			msg_to_char(iter, "The craft you were making has been deleted.\r\n");
			cancel_gen_craft(iter);
		}
	}
	
	// remove from table -- nothing else to check here
	remove_craft_from_table(craft);

	// save index and craft file now
	save_index(DB_BOOT_CRAFT);
	save_library_file_for_vnum(DB_BOOT_CRAFT, vnum);
	
	syslog(SYS_OLC, GET_INVIS_LEV(ch), TRUE, "OLC: %s has deleted craft recipe %d", GET_NAME(ch), vnum);
	msg_to_char(ch, "Craft recipe %d deleted.\r\n", vnum);
	
	free_craft(craft);
}


/**
* Searches for all uses of a craft and displays them.
*
* @param char_data *ch The player.
* @param craft_vnum vnum The craft vnum.
*/
void olc_search_craft(char_data *ch, craft_vnum vnum) {
	char buf[MAX_STRING_LENGTH];
	craft_data *craft = craft_proto(vnum);
	int size, found;
	
	if (!craft) {
		msg_to_char(ch, "There is no craft %d.\r\n", vnum);
		return;
	}
	
	found = 0;
	size = snprintf(buf, sizeof(buf), "Occurrences of craft %d (%s):\r\n", vnum, GET_CRAFT_NAME(craft));
	
	// crafts are not found anywhere in the world yet
	
	if (found > 0) {
		size += snprintf(buf + size, sizeof(buf) - size, "%d location%s shown\r\n", found, PLURAL(found));
	}
	else {
		size += snprintf(buf + size, sizeof(buf) - size, " none\r\n");
	}
	
	page_string(ch->desc, buf, TRUE);
}


/**
* Removes any entries of an obj vnum from a resource list.
*
* @param struct resource_data **list A pointer to a resource list.
* @param obj_vnum vnum The vnum to remove.
* @return bool TRUE if any were removed.
*/
bool remove_obj_from_resource_list(struct resource_data **list, obj_vnum vnum) {
	struct resource_data *res, *next_res, *temp;
	int removed = 0;
	
	for (res = *list; res; res = next_res) {
		next_res = res->next;
		
		if (res->type == RES_OBJECT && res->vnum == vnum) {
			REMOVE_FROM_LIST(res, *list, next);
			free(res);
			++removed;
		}
	}
	
	return removed > 0;
}


/**
* Function to save a player's changes to an craft recipe (or a new one).
*
* @param descriptor_data *desc The descriptor who is saving.
*/
void save_olc_craft(descriptor_data *desc) {
	extern int sort_crafts_by_data(craft_data *a, craft_data *b);
	
	craft_data *proto, *craft = GET_OLC_CRAFT(desc);
	craft_vnum vnum = GET_OLC_VNUM(desc);
	UT_hash_handle hh, sorted_hh;
	
	// have a place to save it?
	if (!(proto = craft_proto(vnum))) {
		proto = create_craft_table_entry(vnum);
	}
	
	// free prototype strings and pointers
	if (GET_CRAFT_NAME(proto)) {
		free(GET_CRAFT_NAME(proto));
	}
	free_resource_list(GET_CRAFT_RESOURCES(proto));
	
	// sanity
	if (!GET_CRAFT_NAME(craft) || !*GET_CRAFT_NAME(craft)) {
		if (GET_CRAFT_NAME(craft)) {
			free(GET_CRAFT_NAME(craft));
		}
		GET_CRAFT_NAME(craft) = str_dup("unnamed recipe");
	}
	
	// save data back over the proto-type
	hh = proto->hh;	// save old hash handles
	sorted_hh = proto->sorted_hh;
	*proto = *craft;	// copy all data
	proto->vnum = vnum;	// ensure correct vnum
	proto->hh = hh;	// restore hash handles
	proto->sorted_hh = sorted_hh;
	
	// and save to file
	save_library_file_for_vnum(DB_BOOT_CRAFT, vnum);
	
	// ... and re-sort
	HASH_SRT(sorted_hh, sorted_crafts, sort_crafts_by_data);
}


/**
* Creates a copy of an craft recipe, or clears a new one, for editing.
* 
* @param craft_data *input The recipe to copy, or NULL to make a new one.
* @return craft_data * The copied recipe.
*/
craft_data *setup_olc_craft(craft_data *input) {
	extern struct resource_data *copy_resource_list(struct resource_data *input);

	craft_data *new;
	
	CREATE(new, craft_data, 1);
	init_craft(new);
	
	if (input) {
		// copy normal data
		*new = *input;

		// copy things that are pointers
		GET_CRAFT_NAME(new) = GET_CRAFT_NAME(input) ? str_dup(GET_CRAFT_NAME(input)) : NULL;
		GET_CRAFT_RESOURCES(new) = copy_resource_list(GET_CRAFT_RESOURCES(input));
	}
	else {
		// brand new: some defaults
		GET_CRAFT_NAME(new) = str_dup("unnamed recipe");
		GET_CRAFT_OBJECT(new) = NOTHING;
		GET_CRAFT_ABILITY(new) = NOTHING;
		GET_CRAFT_QUANTITY(new) = 1;
		GET_CRAFT_TIME(new) = 1;
		GET_CRAFT_FLAGS(new) = CRAFT_IN_DEVELOPMENT;
		GET_CRAFT_REQUIRES_OBJ(new) = NOTHING;
	}
	
	// done
	return new;	
}


 //////////////////////////////////////////////////////////////////////////////
//// DISPLAYS ////////////////////////////////////////////////////////////////

/**
* This is the main recipe display for craft OLC. It displays the user's
* currently-edited craft recipe.
*
* @param char_data *ch The person who is editing a craft and will see its display.
*/
void olc_show_craft(char_data *ch) {
	void get_resource_display(struct resource_data *list, char *save_buffer);

	craft_data *craft = GET_OLC_CRAFT(ch->desc);
	char lbuf[MAX_STRING_LENGTH];
	ability_data *abil;
	int seconds;
	
	if (!craft) {
		return;
	}
	
	*buf = '\0';
	sprintf(buf + strlen(buf), "[&c%d&0] &c%s&0\r\n", GET_OLC_VNUM(ch->desc), !craft_proto(GET_CRAFT_VNUM(craft)) ? "new craft" : GET_CRAFT_NAME(craft_proto(GET_CRAFT_VNUM(craft))));
	sprintf(buf + strlen(buf), "<&yname&0> %s\r\n", GET_CRAFT_NAME(craft));
	sprintf(buf + strlen(buf), "<&ytype&0> %s\r\n", craft_types[GET_CRAFT_TYPE(craft)]);
	
	if (GET_CRAFT_TYPE(craft) == CRAFT_TYPE_BUILD) {
		if (GET_CRAFT_BUILD_TYPE(craft) == NOTHING || !building_proto(GET_CRAFT_BUILD_TYPE(craft))) {
			strcpy(lbuf, "nothing");
		}
		else {
			strcpy(lbuf, GET_BLD_NAME(building_proto(GET_CRAFT_BUILD_TYPE(craft))));
		}
		sprintf(buf + strlen(buf), "<&ybuilds&0> [%d] %s\r\n", GET_CRAFT_BUILD_TYPE(craft), lbuf);
		
		prettier_sprintbit(GET_CRAFT_BUILD_ON(craft), bld_on_flags, buf1);
		sprintf(buf + strlen(buf), "<&ybuildon&0> %s\r\n", buf1);
		
		prettier_sprintbit(GET_CRAFT_BUILD_FACING(craft), bld_on_flags, buf1);
		sprintf(buf + strlen(buf), "<&ybuildfacing&0> %s\r\n", buf1);
	}
	else if (IS_SET(GET_CRAFT_FLAGS(craft), CRAFT_SOUP)) {
		sprintf(buf + strlen(buf), "<&yliquid&0> [%d] %s\r\n", GET_CRAFT_OBJECT(craft), GET_CRAFT_OBJECT(craft) == NOTHING ? "none" : drinks[GET_CRAFT_OBJECT(craft)]);
		sprintf(buf + strlen(buf), "<&yvolume&0> %d drink%s\r\n", GET_CRAFT_QUANTITY(craft), (GET_CRAFT_QUANTITY(craft) != 1 ? "s" : ""));
	}
	else if (IS_SET(GET_CRAFT_FLAGS(craft), CRAFT_VEHICLE)) {
		vehicle_data *proto = vehicle_proto(GET_CRAFT_OBJECT(craft));
		sprintf(buf + strlen(buf), "<&ycreates&0> [%d] %s\r\n", GET_CRAFT_OBJECT(craft), !proto ? "nothing" : VEH_SHORT_DESC(proto));
	
	}
	else {
		// non-soup, non-building, non-vehicle
		obj_data *proto = obj_proto(GET_CRAFT_OBJECT(craft));
		sprintf(buf + strlen(buf), "<&ycreates&0> [%d] %s\r\n", GET_CRAFT_OBJECT(craft), !proto ? "nothing" : GET_OBJ_SHORT_DESC(proto));
		sprintf(buf + strlen(buf), "<&yquantity&0> x%d\r\n", GET_CRAFT_QUANTITY(craft));
	}
	
	// ability required
	if (!(abil = find_ability_by_vnum(GET_CRAFT_ABILITY(craft)))) {
		strcpy(buf1, "none");
	}
	else {
		sprintf(buf1, "%s", ABIL_NAME(abil));
		if (ABIL_ASSIGNED_SKILL(abil)) {
			sprintf(buf1 + strlen(buf1), " (%s %d)", SKILL_NAME(ABIL_ASSIGNED_SKILL(abil)), ABIL_SKILL_LEVEL(abil));
		}
	}
	sprintf(buf + strlen(buf), "<&yrequiresability&0> %s\r\n", buf1);
	
	sprintf(buf + strlen(buf), "<&ylevelrequired&0> %d\r\n", GET_CRAFT_MIN_LEVEL(craft));

	if (GET_CRAFT_TYPE(craft) != CRAFT_TYPE_BUILD && !CRAFT_FLAGGED(craft, CRAFT_VEHICLE)) {
		seconds = (GET_CRAFT_TIME(craft) * ACTION_CYCLE_TIME);
		sprintf(buf + strlen(buf), "<&ytime&0> %d action tick%s (%d:%02d)\r\n", GET_CRAFT_TIME(craft), (GET_CRAFT_TIME(craft) != 1 ? "s" : ""), seconds / 60, seconds % 60);
	}

	sprintbit(GET_CRAFT_FLAGS(craft), craft_flags, buf1, TRUE);
	sprintf(buf + strlen(buf), "<&yflags&0> %s\r\n", buf1);
	
	sprintf(buf + strlen(buf), "<&yrequiresobject&0> %d - %s\r\n", GET_CRAFT_REQUIRES_OBJ(craft), GET_CRAFT_REQUIRES_OBJ(craft) == NOTHING ? "none" : get_obj_name_by_proto(GET_CRAFT_REQUIRES_OBJ(craft)));

	// resources
	sprintf(buf + strlen(buf), "Resources required: <&yresource&0>\r\n");
	get_resource_display(GET_CRAFT_RESOURCES(craft), lbuf);
	strcat(buf, lbuf);
		
	page_string(ch->desc, buf, TRUE);
}


 //////////////////////////////////////////////////////////////////////////////
//// EDIT MODULES ////////////////////////////////////////////////////////////

OLC_MODULE(cedit_ability) {
	craft_data *craft = GET_OLC_CRAFT(ch->desc);
	ability_data *abil;
	
	if (!*argument) {
		msg_to_char(ch, "Require what ability (or 'none')?\r\n");
	}
	else if (!str_cmp(argument, "none")) {
		GET_CRAFT_ABILITY(craft) = NO_ABIL;
		
		if (PRF_FLAGGED(ch, PRF_NOREPEAT)) {
			send_config_msg(ch, "ok_string");
		}
		else {
			msg_to_char(ch, "It will require no ability.\r\n");
		}
	}
	else if (!(abil = find_ability(argument))) {
		msg_to_char(ch, "Invalid ability '%s'.\r\n", argument);
	}
	else {
		GET_CRAFT_ABILITY(craft) = ABIL_VNUM(abil);
		
		if (PRF_FLAGGED(ch, PRF_NOREPEAT)) {
			send_config_msg(ch, "ok_string");
		}
		else {
			msg_to_char(ch, "It now requires the %s ability.\r\n", ABIL_NAME(abil));
		}
	}
}


OLC_MODULE(cedit_buildfacing) {	
	craft_data *craft = GET_OLC_CRAFT(ch->desc);
	
	if (GET_CRAFT_TYPE(craft) != CRAFT_TYPE_BUILD) {
		msg_to_char(ch, "You can only set that property on a building.\r\n");
	}
	else if (!str_cmp(argument, "generic")) {
		GET_CRAFT_BUILD_FACING(craft) = config_get_bitvector("generic_facing");
		
		if (PRF_FLAGGED(ch, PRF_NOREPEAT)) {
			send_config_msg(ch, "ok_string");
		}
		else {
			msg_to_char(ch, "This recipe has been set to use generic facing.\r\n");
		}
	}
	else {
		GET_CRAFT_BUILD_FACING(craft) = olc_process_flag(ch, argument, "build-facing", "buildfacing", bld_on_flags, GET_CRAFT_BUILD_FACING(craft));
	}
}


OLC_MODULE(cedit_buildon) {
	craft_data *craft = GET_OLC_CRAFT(ch->desc);
	
	if (GET_CRAFT_TYPE(craft) != CRAFT_TYPE_BUILD) {
		msg_to_char(ch, "You can only set that property on a building.\r\n");
	}
	else {
		GET_CRAFT_BUILD_ON(craft) = olc_process_flag(ch, argument, "build-on", "buildon", bld_on_flags, GET_CRAFT_BUILD_ON(craft));
	}
}


OLC_MODULE(cedit_builds) {
	craft_data *craft = GET_OLC_CRAFT(ch->desc);
	any_vnum old_b;
	
	if (GET_CRAFT_TYPE(craft) != CRAFT_TYPE_BUILD) {
		msg_to_char(ch, "You can only set that property on a building.\r\n");
		return;
	}
	
	old_b = GET_CRAFT_BUILD_TYPE(craft);
	GET_CRAFT_BUILD_TYPE(craft) = olc_process_number(ch, argument, "building vnum", "builds", 0, MAX_VNUM, GET_CRAFT_BUILD_TYPE(craft));
	if (!building_proto(GET_CRAFT_BUILD_TYPE(craft))) {
		GET_CRAFT_BUILD_TYPE(craft) = old_b;
		msg_to_char(ch, "Invalid building vnum. Old value restored.\r\n");
	}
	else if (IS_SET(GET_BLD_FLAGS(building_proto(GET_CRAFT_BUILD_TYPE(craft))), BLD_ROOM)) {
		GET_CRAFT_BUILD_TYPE(craft) = old_b;
		msg_to_char(ch, "You can't set it to build a ROOM type building. Old value restored.\r\n");
	}	
	else {
		msg_to_char(ch, "It now builds a %s.\r\n", GET_BLD_NAME(building_proto(GET_CRAFT_BUILD_TYPE(craft))));
	}
}


OLC_MODULE(cedit_creates) {
	craft_data *craft = GET_OLC_CRAFT(ch->desc);
	obj_vnum old = GET_CRAFT_OBJECT(craft);
	
	if (IS_SET(GET_CRAFT_FLAGS(craft), CRAFT_SOUP)) {
		msg_to_char(ch, "You can't set that property on a soup.\r\n");
	}
	else if (GET_CRAFT_TYPE(craft) == CRAFT_TYPE_BUILD) {
		msg_to_char(ch, "You can't set that property on a building.\r\n");
	}
	else if (IS_SET(GET_CRAFT_FLAGS(craft), CRAFT_VEHICLE)) {
		GET_CRAFT_OBJECT(craft) = olc_process_number(ch, argument, "vehicle vnum", "creates", 0, MAX_VNUM, GET_CRAFT_OBJECT(craft));
		if (!vehicle_proto(GET_CRAFT_OBJECT(craft))) {
			GET_CRAFT_OBJECT(craft) = old;
			msg_to_char(ch, "There is no vehicle with that vnum. Old value restored.\r\n");
		}
		else if (!PRF_FLAGGED(ch, PRF_NOREPEAT)) {
			msg_to_char(ch, "It now creates %s.\r\n", get_vehicle_name_by_proto(GET_CRAFT_OBJECT(craft)));
		}
	}
	else {	// normal obj craft
		GET_CRAFT_OBJECT(craft) = olc_process_number(ch, argument, "object vnum", "creates", 0, MAX_VNUM, GET_CRAFT_OBJECT(craft));
		if (!obj_proto(GET_CRAFT_OBJECT(craft))) {
			GET_CRAFT_OBJECT(craft) = old;
			msg_to_char(ch, "There is no object with that vnum. Old value restored.\r\n");
		}
		else if (!PRF_FLAGGED(ch, PRF_NOREPEAT)) {
			msg_to_char(ch, "It now creates %dx %s.\r\n", GET_CRAFT_QUANTITY(craft), get_obj_name_by_proto(GET_CRAFT_OBJECT(craft)));
		}
	}
}


OLC_MODULE(cedit_flags) {
	craft_data *craft = GET_OLC_CRAFT(ch->desc);
	bool had_in_dev = IS_SET(GET_CRAFT_FLAGS(craft), CRAFT_IN_DEVELOPMENT) ? TRUE : FALSE;
	bitvector_t has_flags, had_flags = GET_CRAFT_FLAGS(craft) & (CRAFT_VEHICLE | CRAFT_SOUP);
	
	GET_CRAFT_FLAGS(craft) = olc_process_flag(ch, argument, "craft", "flags", craft_flags, GET_CRAFT_FLAGS(craft));
	
	has_flags = GET_CRAFT_FLAGS(craft) & (CRAFT_VEHICLE | CRAFT_SOUP);
	if (has_flags != had_flags) {
		// clear these when vehicle/soup flags are added/removed
		GET_CRAFT_QUANTITY(craft) = 1;
		GET_CRAFT_OBJECT(craft) = NOTHING;
	}
	
	// validate removal of CRAFT_IN_DEVELOPMENT
	if (had_in_dev && !IS_SET(GET_CRAFT_FLAGS(craft), CRAFT_IN_DEVELOPMENT) && GET_ACCESS_LEVEL(ch) < LVL_UNRESTRICTED_BUILDER && !OLC_FLAGGED(ch, OLC_FLAG_CLEAR_IN_DEV)) {
		msg_to_char(ch, "You don't have permission to remove the IN-DEVELOPMENT flag.\r\n");
		SET_BIT(GET_CRAFT_FLAGS(craft), CRAFT_IN_DEVELOPMENT);
	}
}


OLC_MODULE(cedit_levelrequired) {
	craft_data *craft = GET_OLC_CRAFT(ch->desc);
	GET_CRAFT_MIN_LEVEL(craft) = olc_process_number(ch, argument, "minimum level", "levelrequired", 0, 1000, GET_CRAFT_MIN_LEVEL(craft));
}


OLC_MODULE(cedit_liquid) {
	craft_data *craft = GET_OLC_CRAFT(ch->desc);
	
	if (GET_CRAFT_TYPE(craft) == CRAFT_TYPE_BUILD || !IS_SET(GET_CRAFT_FLAGS(craft), CRAFT_SOUP)) {
		msg_to_char(ch, "You can only set the liquid type on a soup.\r\n");
	}
	else {
		GET_CRAFT_OBJECT(craft) = olc_process_type(ch, argument, "liquid", "liquid", drinks, GET_CRAFT_OBJECT(craft));
	}
}


OLC_MODULE(cedit_name) {
	craft_data *craft = GET_OLC_CRAFT(ch->desc);
	olc_process_string(ch, argument, "name", &GET_CRAFT_NAME(craft));
}


OLC_MODULE(cedit_quantity) {
	craft_data *craft = GET_OLC_CRAFT(ch->desc);
	
	if (IS_SET(GET_CRAFT_FLAGS(craft), CRAFT_SOUP)) {
		msg_to_char(ch, "You can't set that on a soup.\r\n");
	}
	else if (IS_SET(GET_CRAFT_FLAGS(craft), CRAFT_VEHICLE)) {
		msg_to_char(ch, "You can't set that on a vehicle craft.\r\n");
	}
	else if (GET_CRAFT_TYPE(craft) == CRAFT_TYPE_BUILD) {
		msg_to_char(ch, "You can't set that property on a building.\r\n");
	}
	else {
		GET_CRAFT_QUANTITY(craft) = olc_process_number(ch, argument, "quantity", "quantity", 0, 100, GET_CRAFT_QUANTITY(craft));
	}
}


OLC_MODULE(cedit_requiresobject) {
	craft_data *craft = GET_OLC_CRAFT(ch->desc);
	obj_vnum old = GET_CRAFT_REQUIRES_OBJ(craft);
	
	if (!str_cmp(argument, "none") || atoi(argument) == NOTHING) {
		GET_CRAFT_REQUIRES_OBJ(craft) = NOTHING;
		if (PRF_FLAGGED(ch, PRF_NOREPEAT)) {
			send_config_msg(ch, "ok_string");
		}
		else {
			msg_to_char(ch, "It no longer requires an object to see it in the craft list.\r\n");
		}
	}
	else {
		GET_CRAFT_REQUIRES_OBJ(craft) = olc_process_number(ch, argument, "object vnum", "requiresobject", 0, MAX_VNUM, GET_CRAFT_REQUIRES_OBJ(craft));
		if (!obj_proto(GET_CRAFT_REQUIRES_OBJ(craft))) {
			GET_CRAFT_REQUIRES_OBJ(craft) = old;
			msg_to_char(ch, "There is no object with that vnum. Old value restored.\r\n");
		}
		else if (!PRF_FLAGGED(ch, PRF_NOREPEAT)) {
			msg_to_char(ch, "It now requires %s.\r\n", get_obj_name_by_proto(GET_CRAFT_REQUIRES_OBJ(craft)));
		}
	}
}


OLC_MODULE(cedit_resource) {
	void olc_process_resources(char_data *ch, char *argument, struct resource_data **list);
	craft_data *craft = GET_OLC_CRAFT(ch->desc);
	olc_process_resources(ch, argument, &GET_CRAFT_RESOURCES(craft));
}


OLC_MODULE(cedit_time) {
	craft_data *craft = GET_OLC_CRAFT(ch->desc);
	
	if (GET_CRAFT_TYPE(craft) == CRAFT_TYPE_BUILD) {
		msg_to_char(ch, "You can't set that property on a building.\r\n");
	}
	else if (CRAFT_FLAGGED(craft, CRAFT_VEHICLE)) {
		msg_to_char(ch, "You can't set that property on a vehicle craft.\r\n");
	}
	else {
		GET_CRAFT_TIME(craft) = olc_process_number(ch, argument, "time", "time", 1, MAX_INT, GET_CRAFT_TIME(craft));
	}
}


OLC_MODULE(cedit_type) {
	craft_data *craft = GET_OLC_CRAFT(ch->desc);
	int old = GET_CRAFT_TYPE(craft);
	
	GET_CRAFT_TYPE(craft) = olc_process_type(ch, argument, "type", "type", craft_types, GET_CRAFT_TYPE(craft));
	
	// things that reset when type is changed
	if (GET_CRAFT_TYPE(craft) != old) {
		// if it changed to or from build, clear all the things that toggle for build
		if (GET_CRAFT_TYPE(craft) == CRAFT_TYPE_BUILD || old == CRAFT_TYPE_BUILD) {
			GET_CRAFT_OBJECT(craft) = NOTHING;
			GET_CRAFT_QUANTITY(craft) = 0;
			GET_CRAFT_TIME(craft) = 1;
			GET_CRAFT_BUILD_TYPE(craft) = NOTHING;
			GET_CRAFT_BUILD_ON(craft) = NOBITS;
			GET_CRAFT_BUILD_FACING(craft) = NOBITS;
			REMOVE_BIT(GET_CRAFT_FLAGS(craft), CRAFT_SOUP | CRAFT_VEHICLE);
		}
	}
}


OLC_MODULE(cedit_volume) {
	craft_data *craft = GET_OLC_CRAFT(ch->desc);
	
	if (GET_CRAFT_TYPE(craft) == CRAFT_TYPE_BUILD || !IS_SET(GET_CRAFT_FLAGS(craft), CRAFT_SOUP)) {
		msg_to_char(ch, "You can only set the drink volume on a soup.\r\n");
	}
	else {
		GET_CRAFT_QUANTITY(craft) = olc_process_number(ch, argument, "drink volume", "volume", 0, 1000, GET_CRAFT_QUANTITY(craft));
	}
}
