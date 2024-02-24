/* ************************************************************************
*   File: olc.craft.c                                     EmpireMUD 2.0b5 *
*  Usage: OLC for craft recipes                                           *
*                                                                         *
*  EmpireMUD code base by Paul Clarke, (C) 2000-2024                      *
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
#include "constants.h"

/**
* Contents:
*   Helpers
*   Displays
*   Edit Modules
*/

// locals
const char *default_craft_name = "unnamed recipe";


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
	bld_data *bld = NULL;

	if (GET_CRAFT_REQUIRES_OBJ(craft) == NOTHING && GET_CRAFT_ABILITY(craft) == NO_ABIL && !CRAFT_FLAGGED(craft, CRAFT_LEARNED) && GET_CRAFT_TYPE(craft) != CRAFT_TYPE_WORKFORCE) {
		olc_audit_msg(ch, GET_CRAFT_VNUM(craft), "Craft requires no object, ability, or recipe");
		problem = TRUE;
	}
	if (IS_SET(GET_CRAFT_FLAGS(craft), CRAFT_IN_DEVELOPMENT)) {
		olc_audit_msg(ch, GET_CRAFT_VNUM(craft), "IN-DEVELOPMENT");
		problem = TRUE;
	}
	if (!GET_CRAFT_RESOURCES(craft) && (!CRAFT_FLAGGED(craft, CRAFT_TAKE_REQUIRED_OBJ) || GET_CRAFT_REQUIRES_OBJ(craft) == NOTHING)) {
		olc_audit_msg(ch, GET_CRAFT_VNUM(craft), "Craft requires no resources");
		problem = TRUE;
	}
	if (!str_cmp(GET_CRAFT_NAME(craft), default_craft_name)) {
		olc_audit_msg(ch, GET_CRAFT_VNUM(craft), "Craft not named");
		problem = TRUE;
	}
	if (CRAFT_FLAGGED(craft, CRAFT_TAKE_REQUIRED_OBJ) && GET_CRAFT_REQUIRES_OBJ(craft) == NOTHING) {
		olc_audit_msg(ch, GET_CRAFT_VNUM(craft), "Has TAKE-REQUIRED-OBJ but requires no obj");
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
	if (CRAFT_IS_BUILDING(craft)) {	// buildings only
		if (GET_CRAFT_BUILD_TYPE(craft) == NOTHING || !(bld = building_proto(GET_CRAFT_BUILD_TYPE(craft)))) {
			olc_audit_msg(ch, GET_CRAFT_VNUM(craft), "Craft makes nothing");
			problem = TRUE;
		}
		if (GET_CRAFT_BUILD_TYPE(craft) != NOTHING && GET_CRAFT_BUILD_TYPE(craft) != GET_CRAFT_VNUM(craft)) {
			olc_audit_msg(ch, GET_CRAFT_VNUM(craft), "Craft creates building with different vnum");
			problem = TRUE;
		}
		if (bld && !CRAFT_FLAGGED(craft, CRAFT_IN_CITY_ONLY) && (IS_SET(GET_BLD_FLAGS(bld), BLD_IN_CITY_ONLY) || IS_SET(GET_BLD_FUNCTIONS(bld), FNC_IN_CITY_ONLY))) {
			olc_audit_msg(ch, GET_CRAFT_VNUM(craft), "Missing in-city-only flag on craft (already set on building or building functions)");
			problem = TRUE;
		}
		if (bld && CRAFT_FLAGGED(craft, CRAFT_IN_CITY_ONLY) && !IS_SET(GET_BLD_FLAGS(bld), BLD_IN_CITY_ONLY) && !IS_SET(GET_BLD_FUNCTIONS(bld), FNC_IN_CITY_ONLY)) {
			olc_audit_msg(ch, GET_CRAFT_VNUM(craft), "Possible unnecessary in-city-only flag (not set on building or building functions)");
			problem = TRUE;
		}
		if (bld && !GET_CRAFT_BUILD_FACING(craft) && !IS_SET(GET_BLD_FLAGS(bld), BLD_OPEN) && !CRAFT_FLAGGED(craft, CRAFT_UPGRADE)) {
			olc_audit_msg(ch, GET_CRAFT_VNUM(craft), "Missing build-facing");
			problem = TRUE;
		}
		if (bld && GET_CRAFT_BUILD_FACING(craft) && CRAFT_FLAGGED(craft, CRAFT_UPGRADE)) {
			olc_audit_msg(ch, GET_CRAFT_VNUM(craft), "Upgrade craft has build-facing");
			problem = TRUE;
		}
		if (GET_CRAFT_QUANTITY(craft) > 1) {
			olc_audit_msg(ch, GET_CRAFT_VNUM(craft), "Building craft with quantity > 1");
			problem = TRUE;
		}
		if (GET_CRAFT_TIME(craft) > 1) {
			olc_audit_msg(ch, GET_CRAFT_VNUM(craft), "Building craft with time set");
			problem = TRUE;
		}
		if (IS_SET(GET_CRAFT_BUILD_ON(craft), BLD_ON_ROAD)) {
			olc_audit_msg(ch, GET_CRAFT_VNUM(craft), "Building has 'road' build-on flag; this should only be used for vehicles and building-vehicles");
			problem = TRUE;
		}
	}
	else if (CRAFT_IS_VEHICLE(craft)) {	// vehicles only
		if (GET_CRAFT_OBJECT(craft) == NOTHING || !vehicle_proto(GET_CRAFT_OBJECT(craft))) {
			olc_audit_msg(ch, GET_CRAFT_VNUM(craft), "Craft makes nothing");
			problem = TRUE;
		}
		if (GET_CRAFT_OBJECT(craft) != NOTHING && GET_CRAFT_OBJECT(craft) != GET_CRAFT_VNUM(craft)) {
			olc_audit_msg(ch, GET_CRAFT_VNUM(craft), "Craft creates vehicle with different vnum");
			problem = TRUE;
		}
		if (GET_CRAFT_QUANTITY(craft) > 1) {
			olc_audit_msg(ch, GET_CRAFT_VNUM(craft), "Vehicle craft with quantity > 1");
			problem = TRUE;
		}
		/* probably don't need this:
		if (GET_CRAFT_TIME(craft) > 1) {
			olc_audit_msg(ch, GET_CRAFT_VNUM(craft), "Vehicle craft with time set");
			problem = TRUE;
		}
		*/
	}
	else if (CRAFT_FLAGGED(craft, CRAFT_SOUP) && !find_generic(GET_CRAFT_OBJECT(craft), GENERIC_LIQUID)) {	// soups only
		olc_audit_msg(ch, GET_CRAFT_VNUM(craft), "Invalid liquid type on soup recipe");
		problem = TRUE;
	}
	else {	// normal craft (not special type))
		if (GET_CRAFT_OBJECT(craft) == NOTHING || !obj_proto(GET_CRAFT_OBJECT(craft))) {
			olc_audit_msg(ch, GET_CRAFT_VNUM(craft), "Craft makes nothing");
			problem = TRUE;
		}
		if (GET_CRAFT_OBJECT(craft) != NOTHING && GET_CRAFT_OBJECT(craft) != GET_CRAFT_VNUM(craft) && GET_CRAFT_TYPE(craft) != CRAFT_TYPE_WORKFORCE) {
			olc_audit_msg(ch, GET_CRAFT_VNUM(craft), "Craft creates item with different vnum");
			problem = TRUE;
		}
	}
	
	if (GET_CRAFT_BUILD_ON(craft) && !CRAFT_IS_VEHICLE(craft) && IS_SET(GET_CRAFT_BUILD_ON(craft), BLD_ON_FLAT_TERRAIN | BLD_FACING_CROP | BLD_FACING_OPEN_BUILDING)) {
		olc_audit_msg(ch, GET_CRAFT_VNUM(craft), "Craft has invalid build-on flags!");
		problem = TRUE;
	}
	if (GET_CRAFT_BUILD_ON(craft) && IS_SET(GET_CRAFT_BUILD_ON(craft), BLD_ON_BASE_TERRAIN_ALLOWED)) {
		olc_audit_msg(ch, GET_CRAFT_VNUM(craft), "Builds on base-terrain-allowed -- this is not allowed in the buildon field");
		problem = TRUE;
	}
	if (CRAFT_FLAGGED(craft, CRAFT_SOUP) && (CRAFT_FLAGGED(craft, CRAFT_VEHICLE) || GET_CRAFT_TYPE(craft) == CRAFT_TYPE_BUILD || CRAFT_FLAGGED(craft, CRAFT_BUILDING))) {
		olc_audit_msg(ch, GET_CRAFT_VNUM(craft), "Unusual combination of SOUP and VEHICLE or BUILD");
		problem = TRUE;
	}
	if (CRAFT_FLAGGED(craft, CRAFT_VEHICLE) && CRAFT_FLAGGED(craft, CRAFT_BUILDING)) {
		olc_audit_msg(ch, GET_CRAFT_VNUM(craft), "Craft has both VEHICLE and BUILDING flags");
		problem = TRUE;
	}
	if (CRAFT_FLAGGED(craft, CRAFT_TOOL_OR_FUNCTION) && (GET_CRAFT_REQUIRES_TOOL(craft) == NOBITS || GET_CRAFT_REQUIRES_FUNCTION(craft) == NOBITS)) {
		olc_audit_msg(ch, GET_CRAFT_VNUM(craft), "Craft has TOOL-OR-FUCTION but is missing %s", (GET_CRAFT_REQUIRES_FUNCTION(craft) == NOBITS ? "tool" : "function"));
		problem = TRUE;
	}
	
	// anything not a building
	if (!CRAFT_IS_BUILDING(craft)) {
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
		snprintf(output, sizeof(output), "[%5d] %s (%s)", GET_CRAFT_VNUM(craft), GET_CRAFT_NAME(craft), craft_types[GET_CRAFT_TYPE(craft)]);
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
	progress_data *prg, *next_prg;
	empire_data *emp, *next_emp;
	obj_data *obj, *next_obj;
	descriptor_data *desc;
	craft_data *craft;
	char_data *iter;
	char name[256];
	
	if (!(craft = craft_proto(vnum))) {
		msg_to_char(ch, "There is no such craft %d.\r\n", vnum);
		return;
	}
	
	snprintf(name, sizeof(name), "%s", NULLSAFE(GET_CRAFT_NAME(craft)));
	
	if (HASH_COUNT(craft_table) <= 1) {
		msg_to_char(ch, "You can't delete the last craft.\r\n");
		return;
	}
	
	// find players who are crafting it and stop them (BEFORE removing from table)
	DL_FOREACH2(player_character_list, iter, next_plr) {
		// currently crafting
		if (GET_ACTION(iter) == ACT_GEN_CRAFT && GET_ACTION_VNUM(iter, 0) == GET_CRAFT_VNUM(craft)) {
			msg_to_char(iter, "The craft you were making has been deleted.\r\n");
			cancel_gen_craft(iter);
		}
		
		// possibly learned
		remove_learned_craft(ch, vnum);
	}
	
	// find empires who had this in their learned list
	HASH_ITER(hh, empire_table, emp, next_emp) {
		remove_learned_craft_empire(emp, vnum, TRUE);
	}
	
	// remove from table -- nothing else to check here
	remove_craft_from_table(craft);

	// save index and craft file now
	save_index(DB_BOOT_CRAFT);
	save_library_file_for_vnum(DB_BOOT_CRAFT, vnum);
	
	// update objs
	HASH_ITER(hh, object_table, obj, next_obj) {
		if (IS_RECIPE(obj) && GET_RECIPE_VNUM(obj) == vnum) {
			set_obj_val(obj, VAL_RECIPE_VNUM, 0);
			syslog(SYS_OLC, GET_INVIS_LEV(ch), TRUE, "OLC: Object %d %s lost deleted learnable craft", GET_OBJ_VNUM(obj), GET_OBJ_SHORT_DESC(obj));
			save_library_file_for_vnum(DB_BOOT_OBJ, GET_OBJ_VNUM(obj));
		}
	}
	
	// update progression
	HASH_ITER(hh, progress_table, prg, next_prg) {
		if (delete_progress_perk_from_list(&PRG_PERKS(prg), PRG_PERK_CRAFT, vnum)) {
			syslog(SYS_OLC, GET_INVIS_LEV(ch), TRUE, "OLC: Progress %d %s lost deleted craft perk", PRG_VNUM(prg), PRG_NAME(prg));
			save_library_file_for_vnum(DB_BOOT_PRG, PRG_VNUM(prg));
		}
	}
	
	// olc editor updates
	for (desc = descriptor_list; desc; desc = desc->next) {
		if (GET_OLC_OBJECT(desc)) {
			if (IS_RECIPE(GET_OLC_OBJECT(desc)) && GET_RECIPE_VNUM(GET_OLC_OBJECT(desc)) == vnum) {
				set_obj_val(GET_OLC_OBJECT(desc), VAL_RECIPE_VNUM, 0);
				msg_to_char(desc->character, "The recipe used by the item you're editing was deleted.\r\n");
			}
		}
		else if (GET_OLC_PROGRESS(desc)) {
			if (delete_progress_perk_from_list(&PRG_PERKS(GET_OLC_PROGRESS(desc)), PRG_PERK_CRAFT, vnum)) {
				save_library_file_for_vnum(DB_BOOT_PRG, PRG_VNUM(GET_OLC_PROGRESS(desc)));
				msg_to_char(desc->character, "A craft used by the progress goal you're editing was deleted.\r\n");
			}
		}
	}
	
	syslog(SYS_OLC, GET_INVIS_LEV(ch), TRUE, "OLC: %s has deleted craft recipe %d %s", GET_NAME(ch), vnum, name);
	msg_to_char(ch, "Craft recipe %d (%s) deleted.\r\n", vnum, name);
	
	free_craft(craft);
}


/**
* Searches properties of craft.
*
* @param char_data *ch The person searching.
* @param char *argument The argument they entered.
*/
void olc_fullsearch_craft(char_data *ch, char *argument) {
	char type_arg[MAX_INPUT_LENGTH], val_arg[MAX_INPUT_LENGTH], find_keywords[MAX_INPUT_LENGTH];
	int count;
	
	bitvector_t only_flags = NOBITS, not_flagged = NOBITS, only_tools = NOBITS, only_functions = NOBITS;
	bitvector_t only_buildon = NOBITS, only_buildfacing = NOBITS;
	int only_type = NOTHING, only_level = NOTHING, only_quantity = NOTHING, only_time = NOTHING;
	int quantity_over = NOTHING, level_over = NOTHING, time_over = NOTHING;
	int quantity_under = NOTHING, level_under = NOTHING, time_under = NOTHING, vmin = NOTHING, vmax = NOTHING;
	bool requires_obj = FALSE;
	
	craft_data *craft, *next_craft;
	
	if (!*argument) {
		msg_to_char(ch, "See HELP CEDIT FULLSEARCH for syntax.\r\n");
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
		
		FULLSEARCH_LIST("type", only_type, craft_types)
		FULLSEARCH_FLAGS("buildon", only_buildon, bld_on_flags)
		FULLSEARCH_FLAGS("buildfacing", only_buildfacing, bld_on_flags)
		FULLSEARCH_FLAGS("flags", only_flags, craft_flags)
		FULLSEARCH_FLAGS("flagged", only_flags, craft_flags)
		FULLSEARCH_FLAGS("unflagged", not_flagged, craft_flags)
		FULLSEARCH_INT("quantity", only_quantity, 0, INT_MAX)
		FULLSEARCH_INT("quantityover", quantity_over, 0, INT_MAX)
		FULLSEARCH_INT("quantityunder", quantity_under, 0, INT_MAX)
		FULLSEARCH_INT("level", only_level, 0, INT_MAX)
		FULLSEARCH_INT("levelover", level_over, 0, INT_MAX)
		FULLSEARCH_INT("levelunder", level_under, 0, INT_MAX)
		FULLSEARCH_FLAGS("requiresfunction", only_functions, function_flags)
		FULLSEARCH_BOOL("requiresobject", requires_obj)
		FULLSEARCH_INT("time", only_time, 0, INT_MAX)
		FULLSEARCH_INT("timesover", time_over, 0, INT_MAX)
		FULLSEARCH_INT("timeunder", time_under, 0, INT_MAX)
		FULLSEARCH_FLAGS("tools", only_tools, tool_flags)
		FULLSEARCH_INT("vmin", vmin, 0, INT_MAX)
		FULLSEARCH_INT("vmax", vmax, 0, INT_MAX)
		
		else {	// not sure what to do with it? treat it like a keyword
			sprintf(find_keywords + strlen(find_keywords), "%s%s", *find_keywords ? " " : "", type_arg);
		}
		
		// prepare for next loop
		skip_spaces(&argument);
	}
	
	add_page_display(ch, "Craft fullsearch: %s", show_color_codes(find_keywords));
	count = 0;
	
	// okay now look up crafts
	HASH_ITER(hh, craft_table, craft, next_craft) {
		if ((vmin != NOTHING && GET_CRAFT_VNUM(craft) < vmin) || (vmax != NOTHING && GET_CRAFT_VNUM(craft) > vmax)) {
			continue;	// vnum range
		}
		if (requires_obj && GET_CRAFT_REQUIRES_OBJ(craft) == NOTHING) {
			continue;
		}
		if (only_buildon != NOBITS && (GET_CRAFT_BUILD_ON(craft) & only_buildon) != only_buildon) {
			continue;
		}
		if (only_buildfacing != NOBITS && (GET_CRAFT_BUILD_FACING(craft) & only_buildfacing) != only_buildfacing) {
			continue;
		}
		if (only_type != NOTHING && GET_CRAFT_TYPE(craft) != only_type) {
			continue;
		}
		if (not_flagged != NOBITS && IS_SET(GET_CRAFT_FLAGS(craft), not_flagged)) {
			continue;
		}
		if (only_flags != NOBITS && (GET_CRAFT_FLAGS(craft) & only_flags) != only_flags) {
			continue;
		}
		if (only_functions != NOBITS && (GET_CRAFT_REQUIRES_FUNCTION(craft) & only_functions) != only_functions) {
			continue;
		}
		if (only_quantity != NOTHING && GET_CRAFT_QUANTITY(craft) != only_quantity) {
			continue;
		}
		if (quantity_over != NOTHING && GET_CRAFT_QUANTITY(craft) < quantity_over) {
			continue;
		}
		if (quantity_under != NOTHING && (GET_CRAFT_QUANTITY(craft) > quantity_under || GET_CRAFT_QUANTITY(craft) == 0)) {
			continue;
		}
		if (only_level != NOTHING && GET_CRAFT_MIN_LEVEL(craft) != only_level) {
			continue;
		}
		if (level_over != NOTHING && GET_CRAFT_MIN_LEVEL(craft) < level_over) {
			continue;
		}
		if (level_under != NOTHING && (GET_CRAFT_MIN_LEVEL(craft) > level_under || GET_CRAFT_MIN_LEVEL(craft) == 0)) {
			continue;
		}
		if (only_time != NOTHING && GET_CRAFT_TIME(craft) != only_time) {
			continue;
		}
		if (time_over != NOTHING && GET_CRAFT_TIME(craft) < time_over) {
			continue;
		}
		if (time_under != NOTHING && (GET_CRAFT_TIME(craft) > time_under || GET_CRAFT_TIME(craft) == 0)) {
			continue;
		}
		if (only_tools != NOBITS && (GET_CRAFT_REQUIRES_TOOL(craft) & only_tools) != only_tools) {
			continue;
		}
		
		if (*find_keywords && !multi_isname(find_keywords, GET_CRAFT_NAME(craft))) {
			continue;
		}
		
		// show it
		add_page_display(ch, "[%5d] %s", GET_CRAFT_VNUM(craft), GET_CRAFT_NAME(craft));
		++count;
	}
	
	if (count > 0) {
		add_page_display(ch, "(%d crafts)", count);
	}
	else {
		add_page_display_str(ch, " none");
	}
	
	send_page_display(ch);
}


/**
* Searches for all uses of a craft and displays them.
*
* @param char_data *ch The player.
* @param craft_vnum vnum The craft vnum.
*/
void olc_search_craft(char_data *ch, craft_vnum vnum) {
	craft_data *craft = craft_proto(vnum);
	progress_data *prg, *next_prg;
	obj_data *obj, *next_obj;
	int found;
	
	if (!craft) {
		msg_to_char(ch, "There is no craft %d.\r\n", vnum);
		return;
	}
	
	found = 0;
	add_page_display(ch, "Occurrences of craft %d (%s):", vnum, GET_CRAFT_NAME(craft));

	// objects
	HASH_ITER(hh, object_table, obj, next_obj) {
		if (IS_RECIPE(obj) && GET_RECIPE_VNUM(obj) == vnum) {
			++found;
			add_page_display(ch, "OBJ [%5d] %s", GET_OBJ_VNUM(obj), GET_OBJ_SHORT_DESC(obj));
		}
	}
	
	// progression
	HASH_ITER(hh, progress_table, prg, next_prg) {
		if (find_progress_perk_in_list(PRG_PERKS(prg), PRG_PERK_CRAFT, vnum)) {
			++found;
			add_page_display(ch, "PRG [%5d] %s", PRG_VNUM(prg), PRG_NAME(prg));
		}
	}
	
	if (found > 0) {
		add_page_display(ch, "%d location%s shown", found, PLURAL(found));
	}
	else {
		add_page_display_str(ch, " none");
	}
	
	send_page_display(ch);
}


/**
* Removes any entries of an obj vnum from a resource list.
*
* @param struct resource_data **list A pointer to a resource list.
* @param int type Any RES_ type, such as RES_OBJECT.
* @param any_vnum vnum The vnum to remove.
* @return bool TRUE if any were removed.
*/
bool remove_thing_from_resource_list(struct resource_data **list, int type, any_vnum vnum) {
	struct resource_data *res, *next_res;
	int removed = 0;
	
	for (res = *list; res; res = next_res) {
		next_res = res->next;
		
		if (res->type == type && res->vnum == vnum) {
			LL_DELETE(*list, res);
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
		GET_CRAFT_NAME(craft) = str_dup(default_craft_name);
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
		GET_CRAFT_NAME(new) = str_dup(default_craft_name);
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


/**
* Counts the words of text in a craft's strings.
*
* @param craft_data *craft The craft whose strings to count.
* @return int The number of words in the craft's strings.
*/
int wordcount_craft(craft_data *craft) {
	int count = 0;
	
	count += wordcount_string(GET_CRAFT_NAME(craft));
	
	return count;
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
	craft_data *craft = GET_OLC_CRAFT(ch->desc);
	char lbuf[MAX_STRING_LENGTH];
	ability_data *abil;
	int seconds;
	
	if (!craft) {
		return;
	}
	
	add_page_display(ch, "[%s%d\t0] %s%s\t0", OLC_LABEL_CHANGED, GET_OLC_VNUM(ch->desc), OLC_LABEL_UNCHANGED, !craft_proto(GET_CRAFT_VNUM(craft)) ? "new craft" : GET_CRAFT_NAME(craft_proto(GET_CRAFT_VNUM(craft))));
	add_page_display(ch, "<%sname\t0> %s", OLC_LABEL_STR(GET_CRAFT_NAME(craft), default_craft_name), GET_CRAFT_NAME(craft));
	add_page_display(ch, "<%stype\t0> %s", OLC_LABEL_VAL(GET_CRAFT_TYPE(craft), 0), craft_types[GET_CRAFT_TYPE(craft)]);
	
	if (CRAFT_IS_BUILDING(craft)) {
		if (GET_CRAFT_BUILD_TYPE(craft) == NOTHING || !building_proto(GET_CRAFT_BUILD_TYPE(craft))) {
			strcpy(lbuf, "nothing");
		}
		else {
			strcpy(lbuf, GET_BLD_NAME(building_proto(GET_CRAFT_BUILD_TYPE(craft))));
		}
		add_page_display(ch, "<%sbuilds\t0> [%d] %s", OLC_LABEL_VAL(GET_CRAFT_BUILD_TYPE(craft), NOTHING), GET_CRAFT_BUILD_TYPE(craft), lbuf);
	}
	else if (IS_SET(GET_CRAFT_FLAGS(craft), CRAFT_SOUP)) {
		add_page_display(ch, "<%sliquid\t0> [%d] %s", OLC_LABEL_VAL(GET_CRAFT_OBJECT(craft), NOTHING), GET_CRAFT_OBJECT(craft), get_generic_name_by_vnum(GET_CRAFT_OBJECT(craft)));
		add_page_display(ch, "<%svolume\t0> %d drink%s", OLC_LABEL_VAL(GET_CRAFT_QUANTITY(craft), 0), GET_CRAFT_QUANTITY(craft), (GET_CRAFT_QUANTITY(craft) != 1 ? "s" : ""));
	}
	else if (CRAFT_IS_VEHICLE(craft)) {
		vehicle_data *proto = vehicle_proto(GET_CRAFT_OBJECT(craft));
		add_page_display(ch, "<%screates\t0> [%d] %s", OLC_LABEL_VAL(GET_CRAFT_OBJECT(craft), NOTHING), GET_CRAFT_OBJECT(craft), !proto ? "nothing" : VEH_SHORT_DESC(proto));
	
	}
	else {
		// non-soup, non-building, non-vehicle
		obj_data *proto = obj_proto(GET_CRAFT_OBJECT(craft));
		add_page_display(ch, "<%screates\t0> [%d] %s", OLC_LABEL_VAL(GET_CRAFT_OBJECT(craft), NOTHING), GET_CRAFT_OBJECT(craft), !proto ? "nothing" : GET_OBJ_SHORT_DESC(proto));
		add_page_display(ch, "<%squantity\t0> x%d", OLC_LABEL_VAL(GET_CRAFT_QUANTITY(craft), 0), GET_CRAFT_QUANTITY(craft));
	}
	
	if (CRAFT_IS_BUILDING(craft) || CRAFT_IS_VEHICLE(craft)) {
		ordered_sprintbit(GET_CRAFT_BUILD_ON(craft), bld_on_flags, bld_on_flags_order, TRUE, lbuf);
		add_page_display(ch, "<%sbuildon\t0> %s", OLC_LABEL_VAL(GET_CRAFT_BUILD_ON(craft), NOBITS), lbuf);
		
		ordered_sprintbit(GET_CRAFT_BUILD_FACING(craft), bld_on_flags, bld_on_flags_order, TRUE, lbuf);
		add_page_display(ch, "<%sbuildfacing\t0> %s", OLC_LABEL_VAL(GET_CRAFT_BUILD_FACING(craft), NOBITS), lbuf);
	}
	
	// ability required
	if (!(abil = find_ability_by_vnum(GET_CRAFT_ABILITY(craft)))) {
		strcpy(lbuf, "none");
	}
	else {
		sprintf(lbuf, "%s", ABIL_NAME(abil));
		if (ABIL_ASSIGNED_SKILL(abil)) {
			sprintf(lbuf + strlen(lbuf), " (%s %d)", SKILL_NAME(ABIL_ASSIGNED_SKILL(abil)), ABIL_SKILL_LEVEL(abil));
		}
	}
	add_page_display(ch, "<%srequiresability\t0> %s", OLC_LABEL_VAL(GET_CRAFT_ABILITY(craft), NOTHING), lbuf);
	
	add_page_display(ch, "<%slevelrequired\t0> %d", OLC_LABEL_VAL(GET_CRAFT_MIN_LEVEL(craft), 0), GET_CRAFT_MIN_LEVEL(craft));

	if (!CRAFT_IS_BUILDING(craft) && !CRAFT_IS_VEHICLE(craft)) {
		seconds = (GET_CRAFT_TIME(craft) * ACTION_CYCLE_TIME);
		add_page_display(ch, "<%stime\t0> %d action tick%s (%s)", OLC_LABEL_VAL(GET_CRAFT_TIME(craft), 1), GET_CRAFT_TIME(craft), (GET_CRAFT_TIME(craft) != 1 ? "s" : ""), colon_time(seconds, FALSE, NULL));
	}

	sprintbit(GET_CRAFT_FLAGS(craft), craft_flags, lbuf, TRUE);
	add_page_display(ch, "<%sflags\t0> %s", OLC_LABEL_VAL(GET_CRAFT_FLAGS(craft), CRAFT_IN_DEVELOPMENT), lbuf);
	
	sprintbit(GET_CRAFT_REQUIRES_TOOL(craft), tool_flags, lbuf, TRUE);
	add_page_display(ch, "<%stools\t0> %s", OLC_LABEL_VAL(GET_CRAFT_REQUIRES_TOOL(craft), NOBITS), lbuf);
	
	sprintbit(GET_CRAFT_REQUIRES_FUNCTION(craft), function_flags, lbuf, TRUE);
	add_page_display(ch, "<%srequiresfunction\t0> %s", OLC_LABEL_VAL(GET_CRAFT_REQUIRES_FUNCTION(craft), NOBITS), lbuf);
	
	add_page_display(ch, "<%srequiresobject\t0> %d - %s", OLC_LABEL_VAL(GET_CRAFT_REQUIRES_OBJ(craft), NOTHING), GET_CRAFT_REQUIRES_OBJ(craft), GET_CRAFT_REQUIRES_OBJ(craft) == NOTHING ? "none" : get_obj_name_by_proto(GET_CRAFT_REQUIRES_OBJ(craft)));

	// resources
	add_page_display(ch, "Resources required: <%sresource\t0>", OLC_LABEL_PTR(GET_CRAFT_RESOURCES(craft)));
	show_resource_display(ch, GET_CRAFT_RESOURCES(craft), FALSE);
		
	send_page_display(ch);
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
	
	if (!CRAFT_IS_BUILDING(craft) && !CRAFT_IS_VEHICLE(craft) && !GET_CRAFT_BUILD_FACING(craft)) {
		msg_to_char(ch, "You can only set that property on a building or vehicle.\r\n");
	}
	else {
		GET_CRAFT_BUILD_FACING(craft) = olc_process_flag(ch, argument, "build-facing", "buildfacing", bld_on_flags, GET_CRAFT_BUILD_FACING(craft));
	}
}


OLC_MODULE(cedit_buildon) {
	craft_data *craft = GET_OLC_CRAFT(ch->desc);
	
	if (!CRAFT_IS_BUILDING(craft) && !CRAFT_IS_VEHICLE(craft) && !GET_CRAFT_BUILD_ON(craft)) {
		msg_to_char(ch, "You can only set that property on a building or vehicle.\r\n");
	}
	else {
		GET_CRAFT_BUILD_ON(craft) = olc_process_flag(ch, argument, "build-on", "buildon", bld_on_flags, GET_CRAFT_BUILD_ON(craft));
	}
}


OLC_MODULE(cedit_builds) {
	craft_data *craft = GET_OLC_CRAFT(ch->desc);
	any_vnum old_b;
	
	if (!CRAFT_IS_BUILDING(craft)) {
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
		msg_to_char(ch, "It now builds %s %s.\r\n", AN(GET_BLD_NAME(building_proto(GET_CRAFT_BUILD_TYPE(craft)))), GET_BLD_NAME(building_proto(GET_CRAFT_BUILD_TYPE(craft))));
	}
}


OLC_MODULE(cedit_creates) {
	craft_data *craft = GET_OLC_CRAFT(ch->desc);
	obj_vnum old = GET_CRAFT_OBJECT(craft);
	
	if (IS_SET(GET_CRAFT_FLAGS(craft), CRAFT_SOUP)) {
		msg_to_char(ch, "You can't set that property on a soup.\r\n");
	}
	else if (CRAFT_IS_BUILDING(craft)) {
		msg_to_char(ch, "You can't set that property on a building.\r\n");
	}
	else if (CRAFT_IS_VEHICLE(craft)) {
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
	bitvector_t has_flags, had_flags = GET_CRAFT_FLAGS(craft) & (CRAFT_VEHICLE | CRAFT_SOUP | CRAFT_BUILDING);
	
	GET_CRAFT_FLAGS(craft) = olc_process_flag(ch, argument, "craft", "flags", craft_flags, GET_CRAFT_FLAGS(craft));
	
	has_flags = GET_CRAFT_FLAGS(craft) & (CRAFT_VEHICLE | CRAFT_SOUP | CRAFT_BUILDING);
	if (has_flags != had_flags) {
		// clear these when vehicle/soup flags are added/removed
		GET_CRAFT_QUANTITY(craft) = 1;
		GET_CRAFT_OBJECT(craft) = NOTHING;
		GET_CRAFT_BUILD_TYPE(craft) = NOTHING;
	}
	
	// validate removal of CRAFT_IN_DEVELOPMENT
	if (had_in_dev && !IS_SET(GET_CRAFT_FLAGS(craft), CRAFT_IN_DEVELOPMENT) && GET_ACCESS_LEVEL(ch) < LVL_UNRESTRICTED_BUILDER && !OLC_FLAGGED(ch, OLC_FLAG_CLEAR_IN_DEV)) {
		msg_to_char(ch, "You don't have permission to remove the IN-DEVELOPMENT flag.\r\n");
		SET_BIT(GET_CRAFT_FLAGS(craft), CRAFT_IN_DEVELOPMENT);
	}
}


OLC_MODULE(cedit_functions) {
	craft_data *craft = GET_OLC_CRAFT(ch->desc);
	GET_CRAFT_REQUIRES_FUNCTION(craft) = olc_process_flag(ch, argument, "function", "requiresfunction", function_flags, GET_CRAFT_REQUIRES_FUNCTION(craft));
}


OLC_MODULE(cedit_levelrequired) {
	craft_data *craft = GET_OLC_CRAFT(ch->desc);
	GET_CRAFT_MIN_LEVEL(craft) = olc_process_number(ch, argument, "minimum level", "levelrequired", 0, 1000, GET_CRAFT_MIN_LEVEL(craft));
}


OLC_MODULE(cedit_liquid) {
	craft_data *craft = GET_OLC_CRAFT(ch->desc);
	any_vnum old;
	
	if (!IS_SET(GET_CRAFT_FLAGS(craft), CRAFT_SOUP)) {
		msg_to_char(ch, "You can only set the liquid type on a soup.\r\n");
	}
	else {
		old = GET_CRAFT_OBJECT(craft);
		GET_CRAFT_OBJECT(craft) = olc_process_number(ch, argument, "liquid vnum", "liquid", 0, MAX_VNUM, GET_CRAFT_OBJECT(craft));
		
		if (!find_generic(GET_CRAFT_OBJECT(craft), GENERIC_LIQUID)) {
			msg_to_char(ch, "Invalid liquid generic vnum %d. Old value restored.\r\n", GET_CRAFT_OBJECT(craft));
			GET_CRAFT_OBJECT(craft) = old;
		}
		else {
			msg_to_char(ch, "It now creates %s.\r\n", get_generic_name_by_vnum(GET_CRAFT_OBJECT(craft)));
		}
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
	else if (CRAFT_IS_VEHICLE(craft)) {
		msg_to_char(ch, "You can't set that on a vehicle craft.\r\n");
	}
	else if (CRAFT_IS_BUILDING(craft)) {
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
	craft_data *craft = GET_OLC_CRAFT(ch->desc);
	olc_process_resources(ch, argument, &GET_CRAFT_RESOURCES(craft));
}


OLC_MODULE(cedit_time) {
	craft_data *craft = GET_OLC_CRAFT(ch->desc);
	
	if (CRAFT_IS_BUILDING(craft)) {
		msg_to_char(ch, "You can't set that property on a building.\r\n");
	}
	else if (CRAFT_IS_VEHICLE(craft)) {
		msg_to_char(ch, "You can't set that property on a vehicle craft.\r\n");
	}
	else {
		GET_CRAFT_TIME(craft) = olc_process_number(ch, argument, "time", "time", 1, MAX_INT, GET_CRAFT_TIME(craft));
	}
}


OLC_MODULE(cedit_tools) {
	craft_data *craft = GET_OLC_CRAFT(ch->desc);
	GET_CRAFT_REQUIRES_TOOL(craft) = olc_process_flag(ch, argument, "tool", "tools", tool_flags, GET_CRAFT_REQUIRES_TOOL(craft));
}


OLC_MODULE(cedit_type) {
	craft_data *craft = GET_OLC_CRAFT(ch->desc);
	bool was_bld = CRAFT_IS_BUILDING(craft) ? TRUE : FALSE;
	
	GET_CRAFT_TYPE(craft) = olc_process_type(ch, argument, "type", "type", craft_types, GET_CRAFT_TYPE(craft));
	
	// things that reset when type is changed to or from a building
	if ((was_bld && !CRAFT_IS_BUILDING(craft)) || (!was_bld && CRAFT_IS_BUILDING(craft))) {
		GET_CRAFT_OBJECT(craft) = NOTHING;
		GET_CRAFT_QUANTITY(craft) = 1;
		GET_CRAFT_TIME(craft) = 1;
		GET_CRAFT_BUILD_TYPE(craft) = NOTHING;
		GET_CRAFT_BUILD_ON(craft) = NOBITS;
		GET_CRAFT_BUILD_FACING(craft) = NOBITS;
		REMOVE_BIT(GET_CRAFT_FLAGS(craft), CRAFT_SOUP);
	}
}


OLC_MODULE(cedit_volume) {
	craft_data *craft = GET_OLC_CRAFT(ch->desc);
	
	if (!IS_SET(GET_CRAFT_FLAGS(craft), CRAFT_SOUP)) {
		msg_to_char(ch, "You can only set the drink volume on a soup.\r\n");
	}
	else {
		GET_CRAFT_QUANTITY(craft) = olc_process_number(ch, argument, "drink volume", "volume", 0, 1000, GET_CRAFT_QUANTITY(craft));
	}
}
