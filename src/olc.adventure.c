/* ************************************************************************
*   File: olc.adventure.c                                 EmpireMUD 2.0b5 *
*  Usage: OLC for adventure zones                                         *
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
#include "dg_scripts.h"
#include "constants.h"

/**
* Contents:
*   Helpers
*   Displays
*   Edit Modules
*/

// external functions
OLC_MODULE(olc_audit);

// locals
const char *default_adv_name = "Unnamed Adventure Zone";
const char *default_adv_author = "Unknown";
int default_adv_reset = 30;


 //////////////////////////////////////////////////////////////////////////////
//// HELPERS /////////////////////////////////////////////////////////////////

/**
* Checks for common adventure problems and reports them to ch.
*
* @param adv_data *adv The adventure to audit.
* @param char_data *ch The person to report to.
* @param bool only_one If TRUE, we are only auditing 1 adventure (so do extended checks).
* @return bool TRUE if any problems were reported; FALSE if all good.
*/
bool audit_adventure(adv_data *adv, char_data *ch, bool only_one) {
	struct adventure_link_rule *link;
	char buf[MAX_STRING_LENGTH];
	struct trig_proto_list *tpl;
	bool found_limit, problem = FALSE;
	adv_data *adv_iter, *next_adv;
	trig_data *trig;
	
	if (GET_ADV_START_VNUM(adv) == 0 || GET_ADV_END_VNUM(adv) == 0) {
		olc_audit_msg(ch, GET_ADV_VNUM(adv), "Vnums not set");
		problem = TRUE;
	}
	else {
		// check overlapping vnums
		HASH_ITER(hh, adventure_table, adv_iter, next_adv) {
			if ((GET_ADV_START_VNUM(adv_iter) >= GET_ADV_START_VNUM(adv) && GET_ADV_START_VNUM(adv_iter) <= GET_ADV_END_VNUM(adv)) || (GET_ADV_END_VNUM(adv_iter) >= GET_ADV_START_VNUM(adv) && GET_ADV_END_VNUM(adv_iter) <= GET_ADV_END_VNUM(adv))) {
				olc_audit_msg(ch, GET_ADV_VNUM(adv), "Adventure %d %s is entirely within its vnum range", GET_ADV_VNUM(adv_iter), GET_ADV_NAME(adv_iter));
				problem = TRUE;
			}
			else if ((GET_ADV_START_VNUM(adv) >= GET_ADV_START_VNUM(adv_iter) && GET_ADV_START_VNUM(adv) <= GET_ADV_END_VNUM(adv_iter)) || (GET_ADV_END_VNUM(adv) >= GET_ADV_START_VNUM(adv_iter) && GET_ADV_END_VNUM(adv) <= GET_ADV_END_VNUM(adv_iter))) {
				olc_audit_msg(ch, GET_ADV_VNUM(adv), "Entirely with the vnum range of adventure %d %s", GET_ADV_VNUM(adv_iter), GET_ADV_NAME(adv_iter));
				problem = TRUE;
			}
		}
	}
	if (GET_ADV_START_VNUM(adv) > GET_ADV_END_VNUM(adv)) {
		olc_audit_msg(ch, GET_ADV_VNUM(adv), "Bad vnum set");
		problem = TRUE;
	}
	if (!str_cmp(GET_ADV_NAME(adv), default_adv_name) || !*GET_ADV_NAME(adv)) {
		olc_audit_msg(ch, GET_ADV_VNUM(adv), "Name not set");
		problem = TRUE;
	}
	if (!str_cmp(GET_ADV_AUTHOR(adv), default_adv_author) || !*GET_ADV_AUTHOR(adv)) {
		olc_audit_msg(ch, GET_ADV_VNUM(adv), "Name not set");
		problem = TRUE;
	}
	if (!GET_ADV_DESCRIPTION(adv) || !*GET_ADV_DESCRIPTION(adv) || !str_cmp(GET_ADV_DESCRIPTION(adv), "Nothing.\r\n")) {
		olc_audit_msg(ch, GET_ADV_VNUM(adv), "Description not set");
		problem = TRUE;
	}
	else if (!strn_cmp(GET_ADV_DESCRIPTION(adv), "Nothing.", 8)) {
		olc_audit_msg(ch, GET_ADV_VNUM(adv), "Description starting with 'Nothing.'");
		problem = TRUE;
	}
	if (!room_template_proto(GET_ADV_START_VNUM(adv))) {
		olc_audit_msg(ch, GET_ADV_VNUM(adv), "Missing start room vnum %d", GET_ADV_START_VNUM(adv));
		problem = TRUE;
	}
	if (GET_ADV_MIN_LEVEL(adv) > GET_ADV_MAX_LEVEL(adv) && GET_ADV_MAX_LEVEL(adv) != 0) {
		olc_audit_msg(ch, GET_ADV_VNUM(adv), "Max level is lower than min level");
		problem = TRUE;
	}
	if (GET_ADV_MAX_INSTANCES(adv) > 50) {
		olc_audit_msg(ch, GET_ADV_VNUM(adv), "Unusually high max-instances: %d", GET_ADV_MAX_INSTANCES(adv));
		problem = TRUE;
	}
	if (ADVENTURE_FLAGGED(adv, ADV_IN_DEVELOPMENT)) {
		olc_audit_msg(ch, GET_ADV_VNUM(adv), "IN-DEVELOPMENT");
		problem = TRUE;
	}
	if (ADVENTURE_FLAGGED(adv, ADV_LOCK_LEVEL_ON_ENTER) && ADVENTURE_FLAGGED(adv, ADV_LOCK_LEVEL_ON_COMBAT)) {
		olc_audit_msg(ch, GET_ADV_VNUM(adv), "Multiple lock flags");
		problem = TRUE;
	}
	if (ADVENTURE_FLAGGED(adv, ADV_NO_NEWBIE) && ADVENTURE_FLAGGED(adv, ADV_NEWBIE_ONLY)) {
		olc_audit_msg(ch, GET_ADV_VNUM(adv), "!NEWBIE and NEWBIE-ONLY");
		problem = TRUE;
	}
	
	// check linking
	found_limit = FALSE;
	for (link = GET_ADV_LINKING(adv); link; link = link->next) {
		switch (link->type) {
			case ADV_LINK_BUILDING_EXISTING:
			case ADV_LINK_BUILDING_NEW: {
				bld_data *bld = building_proto(link->value);
				if (bld && IS_SET(GET_BLD_FLAGS(bld), BLD_OPEN)) {
					olc_audit_msg(ch, GET_ADV_VNUM(adv), "Links to open building");
					problem = TRUE;
				}
				break;
			}
			case ADV_LINK_TIME_LIMIT: {
				found_limit = TRUE;
				break;
			}
		}
	}
	
	if (!found_limit) {
		olc_audit_msg(ch, GET_ADV_VNUM(adv), "No time limit");
		problem = TRUE;
	}

	// check scripts
	for (tpl = GET_ADV_SCRIPTS(adv); tpl; tpl = tpl->next) {
		if (!(trig = real_trigger(tpl->vnum))) {
			continue;
		}
		if (trig->attach_type != WLD_TRIGGER || !IS_SET(GET_TRIG_TYPE(trig), WTRIG_ADVENTURE_CLEANUP)) {
			olc_audit_msg(ch, GET_ADV_VNUM(adv), "Non-cleanup trigger");
			problem = TRUE;
		}
	}

	// sub-audits
	if (only_one && GET_ADV_START_VNUM(adv) <= GET_ADV_END_VNUM(adv)) {
		safe_snprintf(buf, sizeof(buf), "%d %d", GET_ADV_START_VNUM(adv), GET_ADV_END_VNUM(adv));
		// OLC_x: auto-auditors
		msg_to_char(ch, "Attack messages:\r\n");
		olc_audit(ch, OLC_ATTACK, buf);
		msg_to_char(ch, "Crafts:\r\n");
		olc_audit(ch, OLC_CRAFT, buf);
		msg_to_char(ch, "Mobs:\r\n");
		olc_audit(ch, OLC_MOBILE, buf);
		msg_to_char(ch, "Objects:\r\n");
		olc_audit(ch, OLC_OBJECT, buf);
		msg_to_char(ch, "Buildings:\r\n");
		olc_audit(ch, OLC_BUILDING, buf);
		msg_to_char(ch, "Triggers:\r\n");
		olc_audit(ch, OLC_TRIGGER, buf);
		msg_to_char(ch, "Crops:\r\n");
		olc_audit(ch, OLC_CROP, buf);
		msg_to_char(ch, "Sectors:\r\n");
		olc_audit(ch, OLC_SECTOR, buf);
		msg_to_char(ch, "Quests:\r\n");
		olc_audit(ch, OLC_QUEST, buf);
		msg_to_char(ch, "Room Templates:\r\n");
		olc_audit(ch, OLC_ROOM_TEMPLATE, buf);
		msg_to_char(ch, "Archetypes:\r\n");
		olc_audit(ch, OLC_ARCHETYPE, buf);
		msg_to_char(ch, "Augments:\r\n");
		olc_audit(ch, OLC_AUGMENT, buf);
		msg_to_char(ch, "Factions:\r\n");
		olc_audit(ch, OLC_FACTION, buf);
		msg_to_char(ch, "Generics:\r\n");
		olc_audit(ch, OLC_GENERIC, buf);
		msg_to_char(ch, "Globals:\r\n");
		olc_audit(ch, OLC_GLOBAL, buf);
		msg_to_char(ch, "Shops:\r\n");
		olc_audit(ch, OLC_SHOP, buf);
		msg_to_char(ch, "Vehicles:\r\n");
		olc_audit(ch, OLC_VEHICLE, buf);
		msg_to_char(ch, "Morphs:\r\n");
		olc_audit(ch, OLC_MORPH, buf);
	}
	
	return only_one ? TRUE : problem;	// prevents the no-problems message
}


/**
* Creates a new adventure zone entry.
* 
* @param adv_vnum vnum The number to create.
* @return adv_data* The new adventure.
*/
adv_data *create_adventure_table_entry(adv_vnum vnum) {
	adv_data *adv;
	
	// sanity
	if (adventure_proto(vnum)) {
		log("SYSERR: Attempting to insert adventure zone at existing vnum %d", vnum);
		return adventure_proto(vnum);
	}
	
	CREATE(adv, adv_data, 1);
	init_adventure(adv);
	GET_ADV_VNUM(adv) = vnum;
	add_adventure_to_table(adv);

	// save index and adventure file now
	save_index(DB_BOOT_ADV);
	save_library_file_for_vnum(DB_BOOT_ADV, vnum);
	
	return adv;
}


/**
* This function is meant to remove link rules when the portal they use is deleted.
*
* @param struct adventure_link_rule **list The list to remove from.
* @param obj_vnum portal_vnum The portal to look for.
* @return bool TRUE if any links were deleted, FALSE if not
*/
bool delete_link_rule_by_portal(struct adventure_link_rule **list, obj_vnum portal_vnum) {
	struct adventure_link_rule *link, *next_link;
	bool found = FALSE;
	
	// nope
	if (portal_vnum == NOTHING) {
		return FALSE;
	}

	for (link = *list; link; link = next_link) {
		next_link = link->next;
		if (link->portal_in == portal_vnum || link->portal_out == portal_vnum) {
			found = TRUE;
			
			LL_DELETE(*list, link);
			free(link);
		}
	}
	
	return found;
}


/**
* This function is meant to remove link rules when the place they link is deleted.
*
* @param struct adventure_link_rule **list The list to remove from.
* @param int type The ADV_LINK_ to remove.
* @param any_vnum value The item will only be removed if its type and value match.
* @return bool TRUE if any links were deleted, FALSE if not
*/
bool delete_link_rule_by_type_value(struct adventure_link_rule **list, int type, any_vnum value) {
	struct adventure_link_rule *link, *next_link;
	bool found = FALSE;

	for (link = *list; link; link = next_link) {
		next_link = link->next;
		if (link->type == type && link->value == value) {
			found = TRUE;
			
			LL_DELETE(*list, link);
			free(link);
		}
	}
	
	return found;
}


/**
* This function determines which parameters are needed for a bunch of the
* "linking" options. It is used by ".linking add" and ".linking change"
*
* @param int type ADV_LINK_ type.
* @param ... All other parameters are pointers to variables to set up based on type.
*/
void get_advedit_linking_params(int type, int *vnum_type, bool *need_vnum, bool *need_dir, bool *need_buildon, bool *need_buildfacing, bool *need_portalin, bool *need_portalout, bool *need_num, bool *no_rooms, bool *restrict_sect, bool *need_veh) {
	// init
	*vnum_type = OLC_SECTOR;
	*need_vnum = FALSE;
	*need_dir = FALSE;
	*need_buildon = FALSE;
	*need_buildfacing = FALSE;
	*need_portalin = FALSE;
	*need_portalout = FALSE;
	*need_num = FALSE;
	*no_rooms = FALSE;
	*restrict_sect = FALSE;
	*need_veh = FALSE;
	
	// ADV_LINK_x: needed params depend on type
	switch (type) {
		case ADV_LINK_BUILDING_EXISTING: {
			*need_vnum = TRUE;
			*need_dir = TRUE;
			*vnum_type = OLC_BUILDING;
			break;
		}
		case ADV_LINK_BUILDING_NEW: {
			*need_vnum = TRUE;
			*need_dir = TRUE;
			*need_buildon = TRUE;
			*need_buildfacing = TRUE;
			*vnum_type = OLC_BUILDING;
			*no_rooms = TRUE;
			break;
		}
		case ADV_LINK_PORTAL_WORLD: {
			*need_vnum = TRUE;
			*need_portalin = TRUE;
			*need_portalout = TRUE;
			*vnum_type = OLC_SECTOR;
			*restrict_sect = TRUE;
			break;
		}
		case ADV_LINK_PORTAL_BUILDING_EXISTING: {
			*need_vnum = TRUE;
			*need_portalin = TRUE;
			*need_portalout = TRUE;
			*vnum_type = OLC_BUILDING;
			break;
		}
		case ADV_LINK_PORTAL_BUILDING_NEW: {
			*need_vnum = TRUE;
			*need_portalin = TRUE;
			*need_portalout = TRUE;
			*need_buildon = TRUE;
			*need_buildfacing = TRUE;
			*vnum_type = OLC_BUILDING;
			*no_rooms = TRUE;
			break;
		}
		case ADV_LINK_PORTAL_CROP: {
			*need_vnum = TRUE;
			*need_portalin = TRUE;
			*need_portalout = TRUE;
			*vnum_type = OLC_CROP;
			break;
		}
		case ADV_LINK_TIME_LIMIT: {
			*need_num = TRUE;
			break;
		}
		case ADV_LINK_NOT_NEAR_SELF: {
			*need_num = TRUE;
			break;
		}
		case ADV_LINK_EVENT_RUNNING: {
			*need_vnum = TRUE;
			*vnum_type = OLC_EVENT;
			break;
		}
		
		// vehicle types:
		case ADV_LINK_PORTAL_VEH_EXISTING: {
			*need_veh = TRUE;
			*need_portalin = TRUE;
			*need_portalout = TRUE;
			break;
		}
		case ADV_LINK_PORTAL_VEH_NEW_BUILDING_EXISTING: {
			*need_veh = TRUE;
			*need_portalin = TRUE;
			*need_portalout = TRUE;
			*need_vnum = TRUE;
			*vnum_type = OLC_BUILDING;
			break;
		}
		case ADV_LINK_PORTAL_VEH_NEW_BUILDING_NEW: {
			*need_veh = TRUE;
			*need_portalin = TRUE;
			*need_portalout = TRUE;
			*need_vnum = TRUE;
			*need_buildon = TRUE;
			*need_buildfacing = TRUE;
			*no_rooms = TRUE;
			*vnum_type = OLC_BUILDING;
			break;
		}
		case ADV_LINK_PORTAL_VEH_NEW_CROP: {
			*need_veh = TRUE;
			*need_portalin = TRUE;
			*need_portalout = TRUE;
			*need_vnum = TRUE;
			*vnum_type = OLC_CROP;
			break;
		}
		case ADV_LINK_PORTAL_VEH_NEW_WORLD: {
			*need_veh = TRUE;
			*need_portalin = TRUE;
			*need_portalout = TRUE;
			*need_vnum = TRUE;
			*vnum_type = OLC_SECTOR;
			*restrict_sect = TRUE;
			break;
		}
		case ADV_LINK_IN_VEH_EXISTING: {
			*need_veh = TRUE;
			break;
		}
		case ADV_LINK_IN_VEH_NEW_BUILDING_EXISTING: {
			*need_veh = TRUE;
			*need_vnum = TRUE;
			*vnum_type = OLC_BUILDING;
			break;
		}
		case ADV_LINK_IN_VEH_NEW_BUILDING_NEW: {
			*need_veh = TRUE;
			*need_vnum = TRUE;
			*need_buildon = TRUE;
			*need_buildfacing = TRUE;
			*no_rooms = TRUE;
			*vnum_type = OLC_BUILDING;
			break;
		}
		case ADV_LINK_IN_VEH_NEW_CROP: {
			*need_veh = TRUE;
			*need_vnum = TRUE;
			*vnum_type = OLC_CROP;
			break;
		}
		case ADV_LINK_IN_VEH_NEW_WORLD: {
			*need_veh = TRUE;
			*need_vnum = TRUE;
			*vnum_type = OLC_SECTOR;
			*restrict_sect = TRUE;
			break;
		}
	}
}


/**
* For the .list command.
*
* @param adv_data *adv The thing to list.
* @param bool detail If TRUE, provide additional details
* @return char* The line to show (without a CRLF).
*/
char *list_one_adventure(adv_data *adv, bool detail) {
	static char output[MAX_STRING_LENGTH];
	
	if (detail) {
		int count_mobs = 0, count_objs = 0, count_rooms = 0;
		room_template *rmt, *next_rmt;
		char_data *mob, *next_mob;
		obj_data *obj, *next_obj;
		
		HASH_ITER(hh, mobile_table, mob, next_mob) {
			if (GET_MOB_VNUM(mob) >= GET_ADV_START_VNUM(adv) && GET_MOB_VNUM(mob) <= GET_ADV_END_VNUM(adv)) {
				++count_mobs;
			}
		}
		HASH_ITER(hh, object_table, obj, next_obj) {
			if (GET_OBJ_VNUM(obj) >= GET_ADV_START_VNUM(adv) && GET_OBJ_VNUM(obj) <= GET_ADV_END_VNUM(adv)) {
				++count_objs;
			}
		}
		HASH_ITER(hh, room_template_table, rmt, next_rmt) {
			if (GET_RMT_VNUM(rmt) >= GET_ADV_START_VNUM(adv) && GET_RMT_VNUM(rmt) <= GET_ADV_END_VNUM(adv)) {
				++count_rooms;
			}
		}
		
		safe_snprintf(output, sizeof(output), "[%5d] %s [%d-%d] (%s) %d mob%s, %d obj%s, %d room%s", GET_ADV_VNUM(adv), GET_ADV_NAME(adv), GET_ADV_START_VNUM(adv), GET_ADV_END_VNUM(adv), level_range_string(GET_ADV_MIN_LEVEL(adv), GET_ADV_MAX_LEVEL(adv), 0), count_mobs, PLURAL(count_mobs), count_objs, PLURAL(count_objs), count_rooms, PLURAL(count_rooms));
	}
	else {
		safe_snprintf(output, sizeof(output), "[%5d] %s", GET_ADV_VNUM(adv), GET_ADV_NAME(adv));
	}
	
	return output;
}


/**
* WARNING: This function actually deletes an adventure zone.
*
* @param char_data *ch The person doing the deleting.
* @param adv_vnum vnum The vnum to delete.
*/
void olc_delete_adventure(char_data *ch, adv_vnum vnum) {
	adv_data *adv;
	char name[256];
	int live = 0;
	
	if (!(adv = adventure_proto(vnum))) {
		msg_to_char(ch, "There is no such adventure zone %d.\r\n", vnum);
		return;
	}
	
	safe_snprintf(name, sizeof(name), "%s", NULLSAFE(GET_ADV_NAME(adv)));
	
	if (HASH_COUNT(adventure_table) <= 1) {
		msg_to_char(ch, "You can't delete the last adventure zone.\r\n");
		return;
	}
	
	// pull it from the hash FIRST
	remove_adventure_from_table(adv);
	
	// remove active instances
	live = delete_all_instances(adv);

	// save index and adventure file now
	save_index(DB_BOOT_ADV);
	save_library_file_for_vnum(DB_BOOT_ADV, vnum);
	
	syslog(SYS_OLC, GET_INVIS_LEV(ch), TRUE, "OLC: %s has deleted adventure zone %d %s", GET_NAME(ch), vnum, name);
	msg_to_char(ch, "Adventure zone %d (%s) deleted.\r\n", vnum, name);
	
	if (live > 0) {
		msg_to_char(ch, "%d live instances removed.\r\n", live);
	}
	
	free_adventure(adv);
}


/**
* Searches properties of adventures.
*
* @param char_data *ch The person searching.
* @param char *argument The argument they entered.
*/
void olc_fullsearch_adventure(char_data *ch, char *argument) {
	any_vnum only_vnum = NOTHING;
	char type_arg[MAX_INPUT_LENGTH], val_arg[MAX_INPUT_LENGTH], find_keywords[MAX_INPUT_LENGTH];
	char only_author[MAX_INPUT_LENGTH];
	bitvector_t not_flagged = NOBITS, only_flags = NOBITS, find_links = NOBITS, found_links = NOBITS;
	int count, level_over = NOTHING, level_under = NOTHING, only_level = NOTHING, only_temp = NOTHING, vmin = NOTHING, vmax = NOTHING;
	int limit_over = NOTHING, limit_under = NOTHING, only_limit = NOTHING;
	int plimit_over = NOTHING, plimit_under = NOTHING, only_plimit = NOTHING;
	int reset_over = NOTHING, reset_under = NOTHING, only_reset = NOTHING;
	adv_data *adv, *next_adv;
	struct adventure_link_rule *link;
	
	if (!*argument) {
		msg_to_char(ch, "See HELP ADVEDIT FULLSEARCH for syntax.\r\n");
		return;
	}
	
	// process argument
	*only_author = '\0';
	*find_keywords = '\0';
	while (*argument) {
		// figure out a type
		argument = any_one_arg(argument, type_arg);
		
		if (!strcmp(type_arg, "-")) {
			continue;	// just skip stray dashes
		}
		
		FULLSEARCH_STRING("author", only_author)
		FULLSEARCH_FLAGS("flags", only_flags, adventure_flags)
		FULLSEARCH_FLAGS("flagged", only_flags, adventure_flags)
		FULLSEARCH_INT("level", only_level, 0, INT_MAX)
		FULLSEARCH_INT("levelover", level_over, 0, INT_MAX)
		FULLSEARCH_INT("levelunder", level_under, 0, INT_MAX)
		FULLSEARCH_INT("limit", only_limit, 0, INT_MAX)
		FULLSEARCH_INT("limitover", limit_over, 0, INT_MAX)
		FULLSEARCH_INT("limitunder", limit_under, 0, INT_MAX)
		FULLSEARCH_FLAGS("link", find_links, adventure_link_types)
		FULLSEARCH_INT("playerlimit", only_plimit, 0, INT_MAX)
		FULLSEARCH_INT("playerlimitover", plimit_over, 0, INT_MAX)
		FULLSEARCH_INT("playerlimitunder", plimit_under, 0, INT_MAX)
		FULLSEARCH_INT("reset", only_reset, 0, INT_MAX)
		FULLSEARCH_INT("resetover", reset_over, 0, INT_MAX)
		FULLSEARCH_INT("resetunder", reset_under, 0, INT_MAX)
		FULLSEARCH_LIST("temperature", only_temp, temperature_types)
		FULLSEARCH_FLAGS("unflagged", not_flagged, adventure_flags)
		FULLSEARCH_INT("vnum", only_vnum, 0, INT_MAX)
		FULLSEARCH_INT("vmin", vmin, 0, INT_MAX)
		FULLSEARCH_INT("vmax", vmax, 0, INT_MAX)
		
		else {	// not sure what to do with it? treat it like a keyword
			sprintf(find_keywords + strlen(find_keywords), "%s%s", *find_keywords ? " " : "", type_arg);
		}
		
		// prepare for next loop
		skip_spaces(&argument);
	}
	
	build_page_display(ch, "Adventure fullsearch: %s", show_color_codes(find_keywords));
	count = 0;
	
	// okay now look them up
	HASH_ITER(hh, adventure_table, adv, next_adv) {
		if ((vmin != NOTHING && GET_ADV_VNUM(adv) < vmin) || (vmax != NOTHING && GET_ADV_VNUM(adv) > vmax)) {
			continue;	// vnum range
		}
		
		if (*only_author && !strstr(only_author, GET_ADV_AUTHOR(adv))) {
			continue;
		}
		if (not_flagged != NOBITS && ADVENTURE_FLAGGED(adv, not_flagged)) {
			continue;
		}
		if (only_flags != NOBITS && (GET_ADV_FLAGS(adv) & only_flags) != only_flags) {
			continue;
		}
		if (only_level != NOTHING && (only_level < GET_ADV_MIN_LEVEL(adv) || only_level > GET_ADV_MAX_LEVEL(adv))) {
			continue;
		}
		if (level_over != NOTHING && GET_ADV_MIN_LEVEL(adv) < level_over && GET_ADV_MAX_LEVEL(adv) < level_over) {
			continue;
		}
		if (level_under != NOTHING && GET_ADV_MIN_LEVEL(adv) > level_under && GET_ADV_MAX_LEVEL(adv) > level_under) {
			continue;
		}
		if (only_limit != NOTHING && GET_ADV_MAX_INSTANCES(adv) != only_limit) {
			continue;
		}
		if (limit_over != NOTHING && GET_ADV_MAX_INSTANCES(adv) < limit_over) {
			continue;
		}
		if (limit_under != NOTHING && (GET_ADV_MAX_INSTANCES(adv) == 0 || GET_ADV_MAX_INSTANCES(adv) > limit_under)) {
			continue;
		}
		if (only_plimit != NOTHING && GET_ADV_PLAYER_LIMIT(adv) != only_plimit) {
			continue;
		}
		if (plimit_over != NOTHING && GET_ADV_PLAYER_LIMIT(adv) < plimit_over) {
			continue;
		}
		if (plimit_under != NOTHING && (GET_ADV_PLAYER_LIMIT(adv) == 0 || GET_ADV_PLAYER_LIMIT(adv) > plimit_under)) {
			continue;
		}
		if (only_reset != NOTHING && GET_ADV_RESET_TIME(adv) != only_reset) {
			continue;
		}
		if (reset_over != NOTHING && GET_ADV_RESET_TIME(adv) < reset_over) {
			continue;
		}
		if (reset_under != NOTHING && (GET_ADV_RESET_TIME(adv) == 0 || GET_ADV_RESET_TIME(adv) > reset_under)) {
			continue;
		}
		if (only_temp != NOBITS && GET_ADV_TEMPERATURE_TYPE(adv) != only_temp) {
			continue;
		}
		if (only_vnum != NOTHING && (only_vnum < GET_ADV_START_VNUM(adv) || only_vnum > GET_ADV_END_VNUM(adv))) {
			continue;
		}
		if (find_links) {	// look up its linking rules
			found_links = NOBITS;
			LL_FOREACH(GET_ADV_LINKING(adv), link) {
				found_links |= BIT(link->type);
			}
			if ((find_links & found_links) != find_links) {
				continue;
			}
		}
		if (*find_keywords && !multi_isname(find_keywords, GET_ADV_NAME(adv)) && !multi_isname(find_keywords, GET_ADV_AUTHOR(adv)) && !multi_isname(find_keywords, GET_ADV_DESCRIPTION(adv))) {
			continue;
		}
		
		// show it
		build_page_display(ch, "[%5d] %s", GET_ADV_VNUM(adv), GET_ADV_NAME(adv));
		++count;
	}
	
	if (count > 0) {
		build_page_display(ch, "(%d adventures)", count);
	}
	else {
		build_page_display_str(ch, " none");
	}
	
	send_page_display(ch);
}


/**
* Function to save a player's changes to a adventure zone (or a new one).
*
* @param descriptor_data *desc The descriptor who is saving.
*/
void save_olc_adventure(descriptor_data *desc) {
	adv_data *proto, *adv = GET_OLC_ADVENTURE(desc);
	adv_vnum vnum = GET_OLC_VNUM(desc);
	struct adventure_link_rule *link;
	struct trig_proto_list *trig;
	UT_hash_handle hh;
	
	// have a place to save it?
	if (!(proto = adventure_proto(vnum))) {
		proto = create_adventure_table_entry(vnum);
	}
	
	// free prototype strings and pointers
	if (GET_ADV_NAME(proto)) {
		free(GET_ADV_NAME(proto));
	}
	if (GET_ADV_AUTHOR(proto)) {
		free(GET_ADV_AUTHOR(proto));
	}
	if (GET_ADV_DESCRIPTION(proto)) {
		free(GET_ADV_DESCRIPTION(proto));
	}
	while ((link = GET_ADV_LINKING(proto))) {
		GET_ADV_LINKING(proto) = link->next;
		free(link);
	}
	while ((trig = GET_ADV_SCRIPTS(proto))) {
		GET_ADV_SCRIPTS(proto) = trig->next;
		free(trig);
	}
	
	// sanity
	if (!GET_ADV_NAME(adv) || !*GET_ADV_NAME(adv)) {
		if (GET_ADV_NAME(adv)) {
			free(GET_ADV_NAME(adv));
		}
		GET_ADV_NAME(adv) = str_dup(default_adv_name);
	}
	if (!GET_ADV_AUTHOR(adv) || !*GET_ADV_AUTHOR(adv)) {
		if (GET_ADV_AUTHOR(adv)) {
			free(GET_ADV_AUTHOR(adv));
		}
		GET_ADV_AUTHOR(adv) = str_dup(default_adv_author);
	}
	if (GET_ADV_DESCRIPTION(adv) && !*GET_ADV_DESCRIPTION(adv)) {
		if (GET_ADV_DESCRIPTION(adv)) {
			free(GET_ADV_DESCRIPTION(adv));
		}
		GET_ADV_DESCRIPTION(adv) = str_dup("This new adventure zone has no description.\r\n");
	}
	
	// save data back over the proto-type
	hh = proto->hh;	// save old hash handle
	*proto = *adv;	// copy data
	proto->vnum = vnum;	// ensure correct vnum
	proto->hh = hh;	// restore hash handle
	
	// remove live instances if it's in-dev
	if (ADVENTURE_FLAGGED(proto, ADV_IN_DEVELOPMENT)) {
		delete_all_instances(proto);
	}
	
	// and save to file
	save_library_file_for_vnum(DB_BOOT_ADV, vnum);
}


/**
* Creates a copy of a adventure zone, or clears a new one, for editing.
* 
* @param adv_data *input The adventure zone to copy, or NULL to make a new one.
* @return adv_data* The copied adventure zone.
*/
adv_data *setup_olc_adventure(adv_data *input) {
	adv_data *new;
	struct adventure_link_rule *old_link, *new_link;
	
	CREATE(new, adv_data, 1);
	init_adventure(new);
	
	if (input) {
		// copy normal data
		*new = *input;

		// copy things that are pointers
		GET_ADV_NAME(new) = GET_ADV_NAME(input) ? str_dup(GET_ADV_NAME(input)) : NULL;
		GET_ADV_AUTHOR(new) = GET_ADV_AUTHOR(input) ? str_dup(GET_ADV_AUTHOR(input)) : NULL;
		GET_ADV_DESCRIPTION(new) = GET_ADV_DESCRIPTION(input) ? str_dup(GET_ADV_DESCRIPTION(input)) : NULL;
		
		// copy linking
		GET_ADV_LINKING(new) = NULL;
		for (old_link = GET_ADV_LINKING(input); old_link; old_link = old_link->next) {
			CREATE(new_link, struct adventure_link_rule, 1);
			*new_link = *old_link;
			LL_APPEND(GET_ADV_LINKING(new), new_link);
		}
		
		// scripts
		GET_ADV_SCRIPTS(new) = copy_trig_protos(GET_ADV_SCRIPTS(input));
	}
	else {
		// brand new: some defaults
		GET_ADV_NAME(new) = str_dup(default_adv_name);
		GET_ADV_AUTHOR(new) = str_dup(default_adv_author);
		GET_ADV_RESET_TIME(new) = default_adv_reset;

		// ensure
		GET_ADV_FLAGS(new) |= ADV_IN_DEVELOPMENT;
	}
	
	// done
	return new;	
}


/**
* Counts the words of text in an adventure's strings.
*
* @param struct adventure_data *adv The adventure whose strings to count.
* @return int The number of words in the adventure's strings.
*/
int wordcount_adventure(struct adventure_data *adv) {
	int count = 0;
	
	count += wordcount_string(GET_ADV_NAME(adv));
	count += wordcount_string(GET_ADV_AUTHOR(adv));
	count += wordcount_string(GET_ADV_DESCRIPTION(adv));
	
	return count;
}


 //////////////////////////////////////////////////////////////////////////////
//// DISPLAYS ////////////////////////////////////////////////////////////////

/**
* Displays the linking rules from a given list.
*
* @param char_data *ch The person to display it to.
* @param struct adventure_link_rule *list Pointer to the start of a list of links.
* @param bool send_output If TRUE, sends the page_display as text when done. Pass FALSE if you're building a larger page_display for the character.
*/
void show_adventure_linking_display(char_data *ch, struct adventure_link_rule *list, bool send_output) {
	struct adventure_link_rule *rule;
	char lbuf[MAX_STRING_LENGTH], flg[MAX_STRING_LENGTH], bon[MAX_STRING_LENGTH], bfac[MAX_STRING_LENGTH];
	sector_data *sect;
	crop_data *crop;
	bld_data *bld;
	int count = 0;
	size_t size;
	
	for (rule = list; rule; rule = rule->next) {
		// prepare build on/facing as several types use them
		ordered_sprintbit(rule->bld_on, bld_on_flags, bld_on_flags_order, FALSE, bon);
		if (bon[strlen(bon)-1] == ' ') {
			bon[strlen(bon)-1] = '\0';	// trim
		}
		ordered_sprintbit(rule->bld_facing, bld_on_flags, bld_on_flags_order, FALSE, bfac);
		if (bfac[strlen(bfac)-1] == ' ') {
			bfac[strlen(bfac)-1] = '\0';	// trim
		}
	
		// ADV_LINK_x: lbuf based on type
		switch (rule->type) {
			case ADV_LINK_BUILDING_EXISTING:	// drop-thru
			case ADV_LINK_BUILDING_NEW: {
				bld = building_proto(rule->value);
				sprintf(lbuf, "[\tc%d\t0] %s (%s)", rule->value, bld ? GET_BLD_NAME(bld) : "UNKNOWN", dirs[rule->dir]);
				
				if (rule->type == ADV_LINK_BUILDING_NEW) {
					sprintf(lbuf + strlen(lbuf), ", built on \"%s\"", bon);
					if (rule->bld_facing) {
						sprintf(lbuf + strlen(lbuf), ", facing \"%s\"", bfac);
					}
				}
				break;
			}
			case ADV_LINK_PORTAL_WORLD:	// drop-thru
			case ADV_LINK_PORTAL_BUILDING_EXISTING:	// drop-thru
			case ADV_LINK_PORTAL_BUILDING_NEW:	// drop-thru
			case ADV_LINK_PORTAL_CROP: {
				if (rule->type == ADV_LINK_PORTAL_WORLD) {
					sect = sector_proto(rule->value);
					sprintf(lbuf, "[\tc%d\t0] %s", rule->value, sect ? GET_SECT_NAME(sect) : "UNKNOWN");
				}
				else if (rule->type == ADV_LINK_PORTAL_BUILDING_EXISTING || rule->type == ADV_LINK_PORTAL_BUILDING_NEW) {
					bld = building_proto(rule->value);
					sprintf(lbuf, "[\tc%d\t0] %s", rule->value, bld ? GET_BLD_NAME(bld) : "UNKNOWN");
				}
				else if (rule->type == ADV_LINK_PORTAL_CROP) {
					crop = crop_proto(rule->value);
					sprintf(lbuf, "[\tc%d\t0] %s", rule->value, crop ? GET_CROP_NAME(crop) : "UNKNOWN");
				}
				else {
					// should never hit this, but want to be thorough
					*lbuf = '\0';
					break;
				}
				
				// portal objs
				sprintf(lbuf + strlen(lbuf), ", in: [\tc%d\t0] %s", rule->portal_in, skip_filler(get_obj_name_by_proto(rule->portal_in)));
				sprintf(lbuf + strlen(lbuf), ", out: [\tc%d\t0] %s", rule->portal_out, skip_filler(get_obj_name_by_proto(rule->portal_out)));

				if (rule->type == ADV_LINK_PORTAL_BUILDING_NEW) {
					sprintf(lbuf + strlen(lbuf), ", built on \"%s\", facing \"%s\"", bon, bfac);
				}
				break;
			}
			case ADV_LINK_TIME_LIMIT: {
				sprintf(lbuf, "expires after %d minutes (%s)", rule->value, colon_time(rule->value, TRUE, NULL));
				break;
			}
			case ADV_LINK_NOT_NEAR_SELF: {
				sprintf(lbuf, "not within %d tiles of itself", rule->value);
				break;
			}
			case ADV_LINK_EVENT_RUNNING: {
				sprintf(lbuf, "[\tc%d\t0] %s", rule->value, get_event_name_by_proto(rule->value));
				break;
			}
			
			// vehicle types:
			case ADV_LINK_PORTAL_VEH_EXISTING:	// drop-thru
			case ADV_LINK_PORTAL_VEH_NEW_BUILDING_EXISTING:	// drop-thru
			case ADV_LINK_PORTAL_VEH_NEW_BUILDING_NEW:	// drop-thru
			case ADV_LINK_PORTAL_VEH_NEW_CROP:	// drop-thru
			case ADV_LINK_PORTAL_VEH_NEW_WORLD:	// drop-thru
			case ADV_LINK_IN_VEH_EXISTING:	// drop-thru
			case ADV_LINK_IN_VEH_NEW_BUILDING_EXISTING:	// drop-thru
			case ADV_LINK_IN_VEH_NEW_BUILDING_NEW:	// drop-thru
			case ADV_LINK_IN_VEH_NEW_CROP:	// drop-thru
			case ADV_LINK_IN_VEH_NEW_WORLD: {
				size = snprintf(lbuf, sizeof(lbuf), "[\tc%d\t0] %s", rule->vehicle_vnum, get_vehicle_name_by_proto(rule->vehicle_vnum));
				
				// has portal
				if (rule->portal_in != NOTHING) {
					size += snprintf(lbuf + size, sizeof(lbuf) - size, ", in: [\tc%d\t0] %s", rule->portal_in, skip_filler(get_obj_name_by_proto(rule->portal_in)));
				}
				if (rule->portal_out != NOTHING) {
					size += snprintf(lbuf + size, sizeof(lbuf) - size, ", out: [\tc%d\t0] %s", rule->portal_out, skip_filler(get_obj_name_by_proto(rule->portal_out)));
				}
				
				// has building
				if (rule->type == ADV_LINK_PORTAL_VEH_NEW_BUILDING_EXISTING || rule->type == ADV_LINK_PORTAL_VEH_NEW_BUILDING_NEW || rule->type == ADV_LINK_IN_VEH_NEW_BUILDING_EXISTING || rule->type == ADV_LINK_IN_VEH_NEW_BUILDING_NEW) {
					size += snprintf(lbuf + size, sizeof(lbuf) - size, ", in building [\tc%d\t0] %s", rule->value, get_bld_name_by_proto(rule->value));
					
					if (rule->bld_on) {
						size += snprintf(lbuf + size, sizeof(lbuf) - size, ", built on \"%s\"", bon);
					}
					if (rule->bld_facing) {
						size += snprintf(lbuf + size, sizeof(lbuf) - size, ", facing \"%s\"", bfac);
					}
				}
				
				// has sector
				if (rule->type == ADV_LINK_PORTAL_VEH_NEW_WORLD || rule->type == ADV_LINK_IN_VEH_NEW_WORLD) {
					sect = sector_proto(rule->value);
					size += snprintf(lbuf + size, sizeof(lbuf) - size, "[\tc%d\t0] %s", rule->value, sect ? GET_SECT_NAME(sect) : "UNKNOWN");
				}
				
				// has crop
				if (rule->type == ADV_LINK_PORTAL_VEH_NEW_CROP || rule->type == ADV_LINK_IN_VEH_NEW_CROP) {
					crop = crop_proto(rule->value);
					size += snprintf(lbuf + size, sizeof(lbuf) - size, "[\tc%d\t0] %s", rule->value, crop ? GET_CROP_NAME(crop) : "UNKNOWN");
				}
				break;
			}
			
			default: {
				*lbuf = '\0';
				break;
			}
		}
		
		sprintbit(rule->flags, adventure_link_flags, flg, TRUE);
		build_page_display(ch, "%2d. %s: %s%s&g%s\t0", ++count, adventure_link_types[rule->type], lbuf, (rule->flags ? ", " : ""), (rule->flags ? flg : ""));
	}
	
	if (count == 0) {
		build_page_display_str(ch, " none");
	}
	
	if (send_output) {
		send_page_display_as(ch, PD_NO_PAGINATION | PD_FREE_DISPLAY_AFTER);
	}
}


/**
* This is the main recipe display for adventure zone OLC. It displays the 
* user's currently-edited adventure.
*
* @param char_data *ch The person who is editing a adventure and will see its display.
*/
void olc_show_adventure(char_data *ch) {
	adv_data *adv = GET_OLC_ADVENTURE(ch->desc);
	char lbuf[MAX_STRING_LENGTH];
	
	if (!adv) {
		return;
	}

	build_page_display(ch, "[%s%d\t0] %s%s\t0", OLC_LABEL_CHANGED, GET_OLC_VNUM(ch->desc), OLC_LABEL_UNCHANGED, !adventure_proto(GET_ADV_VNUM(adv)) ? "new adventure zone" : GET_ADV_NAME(adventure_proto(GET_ADV_VNUM(adv))));
	build_page_display(ch, "<%sstartvnum\t0> %d", OLC_LABEL_VAL(GET_ADV_START_VNUM(adv), 0), GET_ADV_START_VNUM(adv));
	build_page_display(ch, "<%sendvnum\t0> %d", OLC_LABEL_VAL(GET_ADV_END_VNUM(adv), 0), GET_ADV_END_VNUM(adv));
	build_page_display(ch, "<%sname\t0> %s", OLC_LABEL_STR(GET_ADV_NAME(adv), default_adv_name), NULLSAFE(GET_ADV_NAME(adv)));
	build_page_display(ch, "<%sauthor\t0> %s", OLC_LABEL_STR(GET_ADV_AUTHOR(adv), default_adv_author), NULLSAFE(GET_ADV_AUTHOR(adv)));
	build_page_display(ch, "<%sdescription\t0>\r\n%s", OLC_LABEL_STR(GET_ADV_DESCRIPTION(adv), ""), NULLSAFE(GET_ADV_DESCRIPTION(adv)));
	
	sprintbit(GET_ADV_FLAGS(adv), adventure_flags, lbuf, TRUE);
	build_page_display(ch, "<%sflags\t0> %s", OLC_LABEL_VAL(GET_ADV_FLAGS(adv), ADV_IN_DEVELOPMENT), lbuf);
	
	build_page_display(ch, "<%stemperature\t0> %s", OLC_LABEL_VAL(GET_ADV_TEMPERATURE_TYPE(adv), 0), temperature_types[GET_ADV_TEMPERATURE_TYPE(adv)]);
	
	build_page_display(ch, "<%sminlevel\t0> %d", OLC_LABEL_VAL(GET_ADV_MIN_LEVEL(adv), 0), GET_ADV_MIN_LEVEL(adv));
	build_page_display(ch, "<%smaxlevel\t0> %d", OLC_LABEL_VAL(GET_ADV_MAX_LEVEL(adv), 0), GET_ADV_MAX_LEVEL(adv));
	
	build_page_display(ch, "<%slimit\t0> %d instance%s (adjusts to %d)", OLC_LABEL_VAL(GET_ADV_MAX_INSTANCES(adv), 1), GET_ADV_MAX_INSTANCES(adv), (GET_ADV_MAX_INSTANCES(adv) != 1 ? "s" : ""), adjusted_instance_limit(adv));
	build_page_display(ch, "<%splayerlimit\t0> %d", OLC_LABEL_VAL(GET_ADV_PLAYER_LIMIT(adv), 0), GET_ADV_PLAYER_LIMIT(adv));
	
	// reset time display helper
	if (GET_ADV_RESET_TIME(adv) <= 0) {
		strcpy(lbuf, " (never)");
	}
	else {
		strcpy(lbuf, colon_time(GET_ADV_RESET_TIME(adv), TRUE, NULL));
	}
	build_page_display(ch, "<%sreset\t0> %d minutes %s", OLC_LABEL_VAL(GET_ADV_RESET_TIME(adv), default_adv_reset), GET_ADV_RESET_TIME(adv), lbuf);

	build_page_display(ch, "Linking rules: <%slinking\t0>", OLC_LABEL_PTR(GET_ADV_LINKING(adv)));
	if (GET_ADV_LINKING(adv)) {
		show_adventure_linking_display(ch, GET_ADV_LINKING(adv), FALSE);
	}
	
	// scripts
	build_page_display(ch, "Adventure Cleanup Scripts: <%sscript\t0>", OLC_LABEL_PTR(GET_ADV_SCRIPTS(adv)));
	if (GET_ADV_SCRIPTS(adv)) {
		show_script_display(ch, GET_ADV_SCRIPTS(adv), FALSE);
	}
		
	send_page_display(ch);
}


 //////////////////////////////////////////////////////////////////////////////
//// EDIT MODULES ////////////////////////////////////////////////////////////

OLC_MODULE(advedit_author) {
	adv_data *adv = GET_OLC_ADVENTURE(ch->desc);
	olc_process_string(ch, argument, "author", &(GET_ADV_AUTHOR(adv)));
}


// cascades min-max level ranges to mobs/objs in the adventure
OLC_MODULE(advedit_cascade) {
	bool save_mobs = FALSE, save_objs = FALSE;
	char line[MAX_STRING_LENGTH];
	char_data *mob, *next_mob;
	obj_data *obj, *next_obj;
	adv_data *adv;
	int iter, last;
	
	// validation
	if (!*argument || !isdigit(*argument)) {
		msg_to_char(ch, "Cascade levels for which adventure?\r\n");
		return;
	}
	else if (!(adv = adventure_proto(atoi(argument)))) {
		msg_to_char(ch, "Unknown adventure '%s'.\r\n", argument);
		return;
	}
	else if (!player_can_olc_edit(ch, OLC_ADVENTURE, GET_ADV_VNUM(adv))) {
		msg_to_char(ch, "You don't have permission to edit that adventure.\r\n");
		return;
	}
	else if (GET_ADV_MIN_LEVEL(adv) == 0 && GET_ADV_MAX_LEVEL(adv) == 0) {
		msg_to_char(ch, "That adventure has no scaling constraints.\r\n");
		return;
	}
	
	// mobs first
	HASH_ITER(hh, mobile_table, mob, next_mob) {
		if (GET_MOB_VNUM(mob) < GET_ADV_START_VNUM(adv) || GET_MOB_VNUM(mob) > GET_ADV_END_VNUM(adv)) {
			continue;	// not in this adventure
		}
		
		// READY TO ATTEMPT (will report errors after this
		*line = '\0';
		
		if (GET_MIN_SCALE_LEVEL(mob) > 0 || GET_MAX_SCALE_LEVEL(mob) > 0) {
			safe_snprintf(line, sizeof(line), "already has levels %d-%d", GET_MIN_SCALE_LEVEL(mob), GET_MAX_SCALE_LEVEL(mob));
		}
		else if (!player_can_olc_edit(ch, OLC_MOBILE, GET_MOB_VNUM(mob))) {
			safe_snprintf(line, sizeof(line), "no permission");
		}
		else {
			GET_MIN_SCALE_LEVEL(mob) = GET_ADV_MIN_LEVEL(adv);
			GET_MAX_SCALE_LEVEL(mob) = GET_ADV_MAX_LEVEL(adv);
			safe_snprintf(line, sizeof(line), "updated");
			save_mobs = TRUE;
		}
		
		msg_to_char(ch, "MOB [%d] %s - %s\r\n", GET_MOB_VNUM(mob), GET_SHORT_DESC(mob), line);
	}
	
	// objs second
	HASH_ITER(hh, object_table, obj, next_obj) {
		if (GET_OBJ_VNUM(obj) < GET_ADV_START_VNUM(adv) || GET_OBJ_VNUM(obj) > GET_ADV_END_VNUM(adv)) {
			continue;	// not in this adventure
		}
		
		// READY TO ATTEMPT (will report errors after this
		*line = '\0';
		
		if (GET_OBJ_MIN_SCALE_LEVEL(obj) > 0 || GET_OBJ_MAX_SCALE_LEVEL(obj) > 0) {
			safe_snprintf(line, sizeof(line), "already has levels %d-%d", GET_OBJ_MIN_SCALE_LEVEL(obj), GET_OBJ_MAX_SCALE_LEVEL(obj));
		}
		else if (!player_can_olc_edit(ch, OLC_OBJECT, GET_OBJ_VNUM(obj))) {
			safe_snprintf(line, sizeof(line), "no permission");
		}
		else if (!OBJ_FLAGGED(obj, OBJ_SCALABLE)) {
			safe_snprintf(line, sizeof(line), "not scalable");
		}
		else {
			obj->proto_data->min_scale_level = GET_ADV_MIN_LEVEL(adv);
			obj->proto_data->max_scale_level = GET_ADV_MAX_LEVEL(adv);
			OBJ_VERSION(obj) += 1;
			safe_snprintf(line, sizeof(line), "updated");
			save_objs = TRUE;
		}
		
		msg_to_char(ch, "OBJ [%d] %s - %s\r\n", GET_OBJ_VNUM(obj), GET_OBJ_SHORT_DESC(obj), line);
	}
	
	// saves
	last = -1;
	for (iter = GET_ADV_START_VNUM(adv); iter <= GET_ADV_END_VNUM(adv); ++iter) {
		if ((int)(iter / 100) != last) {
			last = iter / 100;	// once per 100 vnum block in the adventure
			
			if (save_mobs) {
				save_library_file_for_vnum(DB_BOOT_MOB, iter);
			}
			if (save_objs) {
				save_library_file_for_vnum(DB_BOOT_OBJ, iter);
			}
		}
	}
	
	if (save_objs || save_mobs) {
		syslog(SYS_OLC, GET_INVIS_LEV(ch), TRUE, "OLC: %s cascaded levels for adventure [%d] %s", GET_NAME(ch), GET_ADV_VNUM(adv), GET_ADV_NAME(adv));
	}
}


OLC_MODULE(advedit_description) {
	adv_data *adv = GET_OLC_ADVENTURE(ch->desc);
	
	if (ch->desc->str) {
		msg_to_char(ch, "You are already editing a string.\r\n");
	}
	else {
		sprintf(buf, "description for %s", GET_ADV_NAME(adv));
		start_string_editor(ch->desc, buf, &(GET_ADV_DESCRIPTION(adv)), MAX_ROOM_DESCRIPTION, FALSE);
	}
}


OLC_MODULE(advedit_endvnum) {
	adv_data *temp, *adv = GET_OLC_ADVENTURE(ch->desc);
	adv_vnum old;
	
	if (GET_ACCESS_LEVEL(ch) < LVL_UNRESTRICTED_BUILDER && !OLC_FLAGGED(ch, OLC_FLAG_ALL_VNUMS)) {
		msg_to_char(ch, "You must be level %d to do that.\r\n", LVL_UNRESTRICTED_BUILDER);
	}
	else {
		old = GET_ADV_START_VNUM(adv);
		GET_ADV_END_VNUM(adv) = olc_process_number(ch, argument, "ending vnum", "endvnum", GET_ADV_START_VNUM(adv), MAX_INT, GET_ADV_END_VNUM(adv));
		temp = get_adventure_for_vnum(GET_ADV_START_VNUM(adv));
		if (temp && GET_ADV_VNUM(temp) != GET_ADV_VNUM(adv)) {
			msg_to_char(ch, "New value %d is inside adventure [%d] %s; old value restored.\r\n", GET_ADV_START_VNUM(adv), GET_ADV_VNUM(temp), GET_ADV_NAME(temp));
			GET_ADV_START_VNUM(adv) = old;
		}
	}
}


OLC_MODULE(advedit_flags) {
	adv_data *adv = GET_OLC_ADVENTURE(ch->desc);
	bool had_in_dev = IS_SET(GET_ADV_FLAGS(adv), ADV_IN_DEVELOPMENT) ? TRUE : FALSE;
	GET_ADV_FLAGS(adv) = olc_process_flag(ch, argument, "adventure", "flags", adventure_flags, GET_ADV_FLAGS(adv));

	// validate removal of ADV_IN_DEVELOPMENT
	if (had_in_dev && !IS_SET(GET_ADV_FLAGS(adv), ADV_IN_DEVELOPMENT) && GET_ACCESS_LEVEL(ch) < LVL_UNRESTRICTED_BUILDER && !OLC_FLAGGED(ch, OLC_FLAG_CLEAR_IN_DEV)) {
		msg_to_char(ch, "You don't have permission to remove the IN-DEVELOPMENT flag.\r\n");
		SET_BIT(GET_ADV_FLAGS(adv), ADV_IN_DEVELOPMENT);
	}
}


OLC_MODULE(advedit_limit) {
	adv_data *adv = GET_OLC_ADVENTURE(ch->desc);
	GET_ADV_MAX_INSTANCES(adv) = olc_process_number(ch, argument, "instance limit", "limit", 0, 1000, GET_ADV_MAX_INSTANCES(adv));
}


// warning: linking rules require highly-customized input
OLC_MODULE(advedit_linking) {
	bool need_vnum, need_dir, need_buildon, need_buildfacing, need_portalin, need_portalout, need_num, no_rooms, restrict_sect, need_veh;
	char type_arg[MAX_INPUT_LENGTH], vnum_arg[MAX_INPUT_LENGTH], dir_arg[MAX_INPUT_LENGTH], buildon_arg[MAX_INPUT_LENGTH], buildfacing_arg[MAX_INPUT_LENGTH], portalin_arg[MAX_INPUT_LENGTH], portalout_arg[MAX_INPUT_LENGTH], num_arg[MAX_INPUT_LENGTH], val_arg[MAX_INPUT_LENGTH], veh_arg[MAX_INPUT_LENGTH];
	bitvector_t buildon = NOBITS, buildfacing = NOBITS;
	char arg1[MAX_INPUT_LENGTH], lbuf[MAX_STRING_LENGTH];
	struct adventure_link_rule *link, *change;
	obj_data *portalin = NULL, *portalout = NULL;
	adv_data *adv = GET_OLC_ADVENTURE(ch->desc);
	int linktype = 0, dir = NO_DIR, vnum_type;
	any_vnum value = NOTHING, veh_vnum = NOTHING;
	int iter, num;
	bool found;
	
	// arg1: add/remove
	argument = any_one_arg(argument, arg1);
	skip_spaces(&argument);
	
	if (is_abbrev(arg1, "remove")) {
		if (!*argument) {
			msg_to_char(ch, "Remove which linking rule (number)?\r\n");
		}
		else if (!str_cmp(argument, "all")) {
			while ((link = GET_ADV_LINKING(adv))) {
				GET_ADV_LINKING(adv) = link->next;
				free(link);
			}
			msg_to_char(ch, "You remove all linking rules.\r\n");
		}
		else if (!isdigit(*argument) || (num = atoi(argument)) < 1) {
			msg_to_char(ch, "Invalid rule number.\r\n");
		}
		else {
			found = FALSE;
			for (link = GET_ADV_LINKING(adv); link && !found; link = link->next) {
				if (--num == 0) {
					found = TRUE;
					
					msg_to_char(ch, "You remove rule %d (%s)\r\n", atoi(argument), adventure_link_types[link->type]);
					LL_DELETE(GET_ADV_LINKING(adv), link);
					free(link);
				}
			}
			
			if (!found) {
				msg_to_char(ch, "Invalid linking rule number.\r\n");
			}
		}
	}
	else if (is_abbrev(arg1, "add")) {
		// defaults
		*type_arg = *vnum_arg = *dir_arg = *buildon_arg = *buildfacing_arg = *portalin_arg = *portalout_arg = *num_arg = *veh_arg = '\0';
		
		// pull out type arg
		argument = any_one_word(argument, type_arg);
		if (!*type_arg || (linktype = search_block(type_arg, adventure_link_types, FALSE)) == NOTHING) {
			msg_to_char(ch, "Invalid type '%s'.\r\n", type_arg);
			return;
		}
		
		// determine params
		get_advedit_linking_params(linktype, &vnum_type, &need_vnum, &need_dir, &need_buildon, &need_buildfacing, &need_portalin, &need_portalout, &need_num, &no_rooms, &restrict_sect, &need_veh);
		
		// ADV_LINK_x: argument order depends on type
		switch (linktype) {
			case ADV_LINK_BUILDING_EXISTING: {
				argument = any_one_word(argument, vnum_arg);
				argument = any_one_word(argument, dir_arg);
				break;
			}
			case ADV_LINK_BUILDING_NEW: {
				argument = any_one_word(argument, vnum_arg);
				argument = any_one_word(argument, dir_arg);
				argument = any_one_word(argument, buildon_arg);
				argument = any_one_word(argument, buildfacing_arg);
				break;
			}
			case ADV_LINK_PORTAL_WORLD: {
				argument = any_one_word(argument, vnum_arg);
				argument = any_one_word(argument, portalin_arg);
				argument = any_one_word(argument, portalout_arg);
				break;
			}
			case ADV_LINK_PORTAL_BUILDING_EXISTING: {
				argument = any_one_word(argument, vnum_arg);
				argument = any_one_word(argument, portalin_arg);
				argument = any_one_word(argument, portalout_arg);
				break;
			}
			case ADV_LINK_PORTAL_BUILDING_NEW: {
				argument = any_one_word(argument, vnum_arg);
				argument = any_one_word(argument, portalin_arg);
				argument = any_one_word(argument, portalout_arg);
				argument = any_one_word(argument, buildon_arg);
				argument = any_one_word(argument, buildfacing_arg);
				break;
			}
			case ADV_LINK_PORTAL_CROP: {
				argument = any_one_word(argument, vnum_arg);
				argument = any_one_word(argument, portalin_arg);
				argument = any_one_word(argument, portalout_arg);
				break;
			}
			case ADV_LINK_TIME_LIMIT: {
				argument = any_one_word(argument, num_arg);
				break;
			}
			case ADV_LINK_NOT_NEAR_SELF: {
				argument = any_one_word(argument, num_arg);
				break;
			}
			case ADV_LINK_EVENT_RUNNING: {
				argument = any_one_word(argument, vnum_arg);
				break;
			}
			
			// vehicle types:
			case ADV_LINK_PORTAL_VEH_EXISTING: {
				argument = any_one_word(argument, veh_arg);
				argument = any_one_word(argument, portalin_arg);
				argument = any_one_word(argument, portalout_arg);
				break;
			}
			case ADV_LINK_PORTAL_VEH_NEW_BUILDING_EXISTING: {
				argument = any_one_word(argument, veh_arg);
				argument = any_one_word(argument, vnum_arg);
				argument = any_one_word(argument, portalin_arg);
				argument = any_one_word(argument, portalout_arg);
				break;
			}
			case ADV_LINK_PORTAL_VEH_NEW_BUILDING_NEW: {
				argument = any_one_word(argument, veh_arg);
				argument = any_one_word(argument, vnum_arg);
				argument = any_one_word(argument, portalin_arg);
				argument = any_one_word(argument, portalout_arg);
				argument = any_one_word(argument, buildon_arg);
				argument = any_one_word(argument, buildfacing_arg);
				break;
			}
			case ADV_LINK_PORTAL_VEH_NEW_CROP: {
				argument = any_one_word(argument, veh_arg);
				argument = any_one_word(argument, vnum_arg);
				argument = any_one_word(argument, portalin_arg);
				argument = any_one_word(argument, portalout_arg);
				break;
			}
			case ADV_LINK_PORTAL_VEH_NEW_WORLD: {
				argument = any_one_word(argument, veh_arg);
				argument = any_one_word(argument, vnum_arg);
				argument = any_one_word(argument, portalin_arg);
				argument = any_one_word(argument, portalout_arg);
				break;
			}
			case ADV_LINK_IN_VEH_EXISTING: {
				argument = any_one_word(argument, veh_arg);
				break;
			}
			case ADV_LINK_IN_VEH_NEW_BUILDING_EXISTING: {
				argument = any_one_word(argument, veh_arg);
				argument = any_one_word(argument, vnum_arg);
				break;
			}
			case ADV_LINK_IN_VEH_NEW_BUILDING_NEW: {
				argument = any_one_word(argument, veh_arg);
				argument = any_one_word(argument, vnum_arg);
				argument = any_one_word(argument, buildon_arg);
				argument = any_one_word(argument, buildfacing_arg);
				break;
			}
			case ADV_LINK_IN_VEH_NEW_CROP: {
				argument = any_one_word(argument, veh_arg);
				argument = any_one_word(argument, vnum_arg);
				break;
			}
			case ADV_LINK_IN_VEH_NEW_WORLD: {
				argument = any_one_word(argument, veh_arg);
				argument = any_one_word(argument, vnum_arg);
				break;
			}
		}
		
		// anything left is flags
		skip_spaces(&argument);
		
		if (need_veh && !*veh_arg) {
		}
		else if (need_vnum && !*vnum_arg) {
			msg_to_char(ch, "You must specify a vnum.\r\n");
		}
		else if (need_dir && !*dir_arg) {
			msg_to_char(ch, "You must specify a direction to link the entrance.\r\n");
		}
		else if (need_dir && (dir = parse_direction(ch, dir_arg)) == NO_DIR) {
			msg_to_char(ch, "Invalid direction '%s'.\r\n", dir_arg);
		}
		else if (need_buildon && !*buildon_arg) {
			msg_to_char(ch, "You must provide build-on information (in quotes if more than 1 flag).\r\n");
		}
		else if (need_portalin && !*portalin_arg) {
			msg_to_char(ch, "You must specify a vnum for the entrance portal.\r\n");
		}
		else if (need_portalin && (!(portalin = obj_proto(atoi(portalin_arg))) || !IS_PORTAL(portalin))) {
			msg_to_char(ch, "Invalid entrance portal vnum '%s'.\r\n", portalin_arg);
		}
		else if (need_portalout && !*portalout_arg) {
			msg_to_char(ch, "You must specify a vnum for the exit portal.\r\n");
		}
		else if (need_portalout && (!(portalout = obj_proto(atoi(portalout_arg))) || !IS_PORTAL(portalout))) {
			msg_to_char(ch, "Invalid exit portal vnum '%s'.\r\n", portalout_arg);
		}
		else if (need_num && !*num_arg) {
			msg_to_char(ch, "You must specify a number.\r\n");
		}
		else {
			// BASIC SUCCESS: some additional setup
			if (need_num) {
				value = atoi(num_arg);
			}
			if (need_veh) {
				veh_vnum = atoi(veh_arg);
				if (!vehicle_proto(veh_vnum)) {
					msg_to_char(ch, "Invalid vehicle vnum '%s'.\r\n", vnum_arg);
					return;
				}
			}
			if (need_vnum) {
				value = atoi(vnum_arg);

				// checking
				if (vnum_type == OLC_BUILDING && !building_proto(value)) {
					msg_to_char(ch, "Invalid building vnum '%s'.\r\n", vnum_arg);
					return;
				}
				if (no_rooms && IS_SET(GET_BLD_FLAGS(building_proto(value)), BLD_ROOM)) {
					msg_to_char(ch, "You may not use ROOM-type buildings for this.\r\n");
					return;
				}
				if (vnum_type == OLC_SECTOR && !sector_proto(value)) {
					msg_to_char(ch, "Invalid sector vnum '%s'.\r\n", vnum_arg);
					return;
				}
				if (vnum_type == OLC_CROP && !crop_proto(value)) {
					msg_to_char(ch, "Invalid crop vnum '%s'.\r\n", vnum_arg);
					return;
				}
				if (vnum_type == OLC_EVENT && !find_event_by_vnum(value)) {
					msg_to_char(ch, "Invalid event vnum '%s'.\r\n", vnum_arg);
					return;
				}
				if (restrict_sect && SECT_FLAGGED(sector_proto(value), SECTF_ADVENTURE)) {
					msg_to_char(ch, "You may not use ADVENTURE-type sectors for this.\r\n");
					return;
				}
			}
			if (need_buildon) {
				buildon = olc_process_flag(ch, buildon_arg, "build-on", NULL, bld_on_flags, NOBITS);
			}
			if (need_buildfacing) {
				buildfacing = olc_process_flag(ch, buildfacing_arg, "build-facing", NULL, bld_on_flags, NOBITS);
			}
			
			// setup done...
			CREATE(link, struct adventure_link_rule, 1);
			link->type = linktype;
			link->flags = *argument ? olc_process_flag(ch, argument, "linking", NULL, adventure_link_flags, NOBITS) : NOBITS;
			link->value = value;
			link->portal_in = portalin ? GET_OBJ_VNUM(portalin) : NOTHING;
			link->portal_out = portalout ? GET_OBJ_VNUM(portalout) : NOTHING;
			link->dir = dir;
			link->bld_on = buildon;
			link->bld_facing = buildfacing;
			link->vehicle_vnum = veh_vnum;
			LL_APPEND(GET_ADV_LINKING(adv), link);

			msg_to_char(ch, "You add a linking rule of type: %s\r\n", adventure_link_types[linktype]);
			if (need_veh) {
				msg_to_char(ch, " - vehicle: %d %s\r\n", veh_vnum, get_vehicle_name_by_proto(veh_vnum));
			}
			if (need_vnum) {
				msg_to_char(ch, " - vnum: %d\r\n", value);
			}
			if (need_dir) {
				msg_to_char(ch, " - dir: %s\r\n", dirs[dir]);
			}
			if (need_buildon) {
				ordered_sprintbit(buildon, bld_on_flags, bld_on_flags_order, TRUE, lbuf);
				msg_to_char(ch, " - built on: %s\r\n", lbuf);
			}
			if (need_buildfacing) {
				ordered_sprintbit(buildfacing, bld_on_flags, bld_on_flags_order, TRUE, lbuf);
				msg_to_char(ch, " - built facing: %s\r\n", lbuf);
			}
			if (need_portalin) {
				msg_to_char(ch, " - portal in: %d %s\r\n", GET_OBJ_VNUM(portalin), skip_filler(GET_OBJ_SHORT_DESC(portalin)));
			}
			if (need_portalout) {
				msg_to_char(ch, " - portal out: %d %s\r\n", GET_OBJ_VNUM(portalout), skip_filler(GET_OBJ_SHORT_DESC(portalout)));
			}
			if (need_num) {
				msg_to_char(ch, " - value: %d\r\n", value);
			}
			if (link->flags) {
				sprintbit(link->flags, adventure_link_flags, lbuf, TRUE);
				msg_to_char(ch, " - flags: %s\r\n", lbuf);
			}
		}
	}
	else if (is_abbrev(arg1, "change")) {		
		half_chop(argument, num_arg, arg1);
		half_chop(arg1, type_arg, val_arg);
		
		if (!*num_arg || !isdigit(*num_arg) || !*type_arg) {
			msg_to_char(ch, "Usage: linking change <number> <field> <value>\r\n");
			msg_to_char(ch, "Valid fields: vnum, vehicle, dir, flags, buildon, buildfacing, portalin, portalout, number\r\n");
			return;
		}
		
		// find which one to change
		num = atoi(num_arg);
		change = NULL;
		for (link = GET_ADV_LINKING(adv); link && !change; link = link->next) {
			if (--num == 0) {
				change = link;
				break;
			}
		}
		
		if (!change) {
			msg_to_char(ch, "Invalid linking rule number.\r\n");
			return;
		}
		
		// determine params
		get_advedit_linking_params(change->type, &vnum_type, &need_vnum, &need_dir, &need_buildon, &need_buildfacing, &need_portalin, &need_portalout, &need_num, &no_rooms, &restrict_sect, &need_veh);
		
		if (is_abbrev(type_arg, "vnum")) {
			if (!need_vnum) {
				msg_to_char(ch, "That link has no vnum.\r\n");
			}
			else if (!*val_arg) {
				msg_to_char(ch, "Set the vnum to what?\r\n");
			}
			else if (!isdigit(*val_arg) || (value = atoi(val_arg)) < 0) {
				msg_to_char(ch, "Invalid vnum '%s'.\r\n", val_arg);
			}
			else if (vnum_type == OLC_BUILDING && !building_proto(value)) {
				msg_to_char(ch, "Invalid building vnum '%s'.\r\n", val_arg);
			}
			else if (no_rooms && IS_SET(GET_BLD_FLAGS(building_proto(value)), BLD_ROOM)) {
				msg_to_char(ch, "You may not use ROOM-type buildings for this.\r\n");
			}
			else if (vnum_type == OLC_SECTOR && !sector_proto(value)) {
				msg_to_char(ch, "Invalid sector vnum '%s'.\r\n", val_arg);
			}
			else if (vnum_type == OLC_CROP && !crop_proto(value)) {
				msg_to_char(ch, "Invalid crop vnum '%s'.\r\n", val_arg);
			}
			else if (vnum_type == OLC_EVENT && !find_event_by_vnum(value)) {
				msg_to_char(ch, "Invalid event vnum '%s'.\r\n", val_arg);
			}
			else if (restrict_sect && SECT_FLAGGED(sector_proto(value), SECTF_ADVENTURE)) {
				msg_to_char(ch, "You may not use ADVENTURE-type sectors for this.\r\n");
			}
			else {
				change->value = value;
				msg_to_char(ch, "Linking rule %d changed to vnum %d.\r\n", atoi(num_arg), value);
			}
		}
		else if (is_abbrev(type_arg, "vehicle")) {
			if (!need_veh) {
				msg_to_char(ch, "That link has no vehicle.\r\n");
			}
			else if (!*val_arg) {
				msg_to_char(ch, "Set the vehicle to what vnum?\r\n");
			}
			else if (!isdigit(*val_arg) || (value = atoi(val_arg)) < 0 || !vehicle_proto(value)) {
				msg_to_char(ch, "Invalid vehicle vnum '%s'.\r\n", val_arg);
			}
			else {
				change->vehicle_vnum = value;
				msg_to_char(ch, "Linking rule %d changed to vehicle %d %s.\r\n", atoi(num_arg), value, get_vehicle_name_by_proto(value));
			}
		}
		else if (is_abbrev(type_arg, "direction")) {
			if (!need_dir) {
				msg_to_char(ch, "That link has no direction.\r\n");
			}
			else if (!*val_arg) {
				msg_to_char(ch, "Set the direction to what?\r\n");
			}
			else if ((dir = parse_direction(ch, val_arg)) == NO_DIR) {
				msg_to_char(ch, "Invalid direction '%s'.\r\n", val_arg);
			}
			else {
				change->dir = dir;
				msg_to_char(ch, "Linking rule %d changed to direction %s.\r\n", atoi(num_arg), dirs[dir]);
			}
		}
		else if (is_abbrev(type_arg, "flags")) {
			change->flags = olc_process_flag(ch, val_arg, "linking", "linking change flags", adventure_link_flags, change->flags);
		}
		else if (is_abbrev(type_arg, "buildon")) {
			if (!need_buildon) {
				msg_to_char(ch, "That link has no buildon.\r\n");
			}
			else {
				change->bld_on = olc_process_flag(ch, val_arg, "buildon", "linking change buildon", bld_on_flags, change->bld_on);
			}
		}
		else if (is_abbrev(type_arg, "buildfacing")) {
			if (!need_buildfacing) {
				msg_to_char(ch, "That link has no buildfacing.\r\n");
			}
			else {
				change->bld_facing = olc_process_flag(ch, val_arg, "buildfacing", "linking change buildfacing", bld_on_flags, change->bld_facing);
			}
		}
		else if (is_abbrev(type_arg, "portalin")) {
			if (!need_portalin) {
				msg_to_char(ch, "That link has no portalin.\r\n");
			}
			else if (!*val_arg) {
				msg_to_char(ch, "Set the portalin vnum to what?\r\n");
			}
			else if (!(portalin = obj_proto(atoi(val_arg))) || !IS_PORTAL(portalin)) {
				msg_to_char(ch, "Invalid entrance portal vnum '%s'.\r\n", val_arg);
			}
			else {
				change->portal_in = GET_OBJ_VNUM(portalin);
				msg_to_char(ch, "Linking rule %d entrance portal changed to %d (%s).\r\n", atoi(num_arg), GET_OBJ_VNUM(portalin), GET_OBJ_SHORT_DESC(portalin));
			}
		}
		else if (is_abbrev(type_arg, "portalout")) {
			if (!need_portalout) {
				msg_to_char(ch, "That link has no portalout.\r\n");
			}
			else if (!*val_arg) {
				msg_to_char(ch, "Set the portalout vnum to what?\r\n");
			}
			else if (!(portalout = obj_proto(atoi(val_arg))) || !IS_PORTAL(portalout)) {
				msg_to_char(ch, "Invalid exit portal vnum '%s'.\r\n", val_arg);
			}
			else {
				change->portal_out = GET_OBJ_VNUM(portalout);
				msg_to_char(ch, "Linking rule %d exit portal changed to %d (%s).\r\n", atoi(num_arg), GET_OBJ_VNUM(portalout), GET_OBJ_SHORT_DESC(portalout));
			}
		}
		else if (is_abbrev(type_arg, "number")) {
			value = atoi(val_arg);
			
			if (!need_num) {
				msg_to_char(ch, "That link has no number.\r\n");
			}
			else if (!*val_arg || (!isdigit(*val_arg) && *val_arg != '-')) {
				msg_to_char(ch, "Set the number to what?\r\n");
			}
			else {
				change->value = value;
				msg_to_char(ch, "Linking rule %d numeric value changed to %d.\r\n", atoi(num_arg), value);
			}
		}
		else {
			msg_to_char(ch, "You can only change the vnum, dir, flags, buildon, buildfacing, portalin, portalout, or number.\r\n");
		}
	}
	else {
		msg_to_char(ch, "Usage: linking add <type> <data> [flags] (see HELP ADVEDIT LINKING)\r\n");
		msg_to_char(ch, "Usage: linking change <number> <field> <value>\r\n");
		msg_to_char(ch, "Usage: linking remove <number | all>\r\n");
		msg_to_char(ch, "Available types:\r\n");
		
		for (iter = 0; *adventure_link_types[iter] != '\n'; ++iter) {
			msg_to_char(ch, " %-24.24s%s", adventure_link_types[iter], ((iter % 2) ? "\r\n" : ""));
		}
		if ((iter % 2) != 0) {
			msg_to_char(ch, "\r\n");
		}
	}

}


OLC_MODULE(advedit_maxlevel) {
	adv_data *adv = GET_OLC_ADVENTURE(ch->desc);
	GET_ADV_MAX_LEVEL(adv) = olc_process_number(ch, argument, "max level", "maxlevel", 0, MAX_INT, GET_ADV_MAX_LEVEL(adv));
}


OLC_MODULE(advedit_minlevel) {
	adv_data *adv = GET_OLC_ADVENTURE(ch->desc);
	GET_ADV_MIN_LEVEL(adv) = olc_process_number(ch, argument, "min level", "minlevel", 0, MAX_INT, GET_ADV_MIN_LEVEL(adv));
}


OLC_MODULE(advedit_name) {
	adv_data *adv = GET_OLC_ADVENTURE(ch->desc);
	olc_process_string(ch, argument, "name", &(GET_ADV_NAME(adv)));
}


OLC_MODULE(advedit_playerlimit) {
	adv_data *adv = GET_OLC_ADVENTURE(ch->desc);
	GET_ADV_PLAYER_LIMIT(adv) = olc_process_number(ch, argument, "player limit", "playerlimit", 0, 50, GET_ADV_PLAYER_LIMIT(adv));
}


OLC_MODULE(advedit_reset) {
	adv_data *adv = GET_OLC_ADVENTURE(ch->desc);
	GET_ADV_RESET_TIME(adv) = olc_process_number(ch, argument, "reset time", "reset", 0, MAX_INT, GET_ADV_RESET_TIME(adv));
}


OLC_MODULE(advedit_script) {
	adv_data *adv = GET_OLC_ADVENTURE(ch->desc);
	olc_process_script(ch, argument, &(GET_ADV_SCRIPTS(adv)), WLD_TRIGGER);
}


OLC_MODULE(advedit_temperature) {
	adv_data *adv = GET_OLC_ADVENTURE(ch->desc);
	GET_ADV_TEMPERATURE_TYPE(adv) = olc_process_type(ch, argument, "temperature", "temperature", temperature_types, GET_ADV_TEMPERATURE_TYPE(adv));
}


OLC_MODULE(advedit_startvnum) {
	adv_data *temp, *adv = GET_OLC_ADVENTURE(ch->desc);
	adv_vnum old;
	
	if (GET_ACCESS_LEVEL(ch) < LVL_UNRESTRICTED_BUILDER && !OLC_FLAGGED(ch, OLC_FLAG_ALL_VNUMS)) {
		msg_to_char(ch, "You must be level %d to do that.\r\n", LVL_UNRESTRICTED_BUILDER);
	}
	else {
		old = GET_ADV_START_VNUM(adv);
		GET_ADV_START_VNUM(adv) = olc_process_number(ch, argument, "starting vnum", "startvnum", 0, MAX_INT, GET_ADV_START_VNUM(adv));
		temp = get_adventure_for_vnum(GET_ADV_START_VNUM(adv));
		if (temp && GET_ADV_VNUM(temp) != GET_ADV_VNUM(adv)) {
			msg_to_char(ch, "New value %d is inside adventure [%d] %s; old value restored.\r\n", GET_ADV_START_VNUM(adv), GET_ADV_VNUM(temp), GET_ADV_NAME(temp));
			GET_ADV_START_VNUM(adv) = old;
		}
	}
}


// removes min/max levels from mobs/objs if those are the same as for the adventure itself
OLC_MODULE(advedit_uncascade) {
	bool save_mobs = FALSE, save_objs = FALSE;
	char line[MAX_STRING_LENGTH];
	char_data *mob, *next_mob;
	obj_data *obj, *next_obj;
	adv_data *adv;
	int iter, last;
	
	// validation
	if (!*argument || !isdigit(*argument)) {
		msg_to_char(ch, "Uncascade levels for which adventure?\r\n");
		return;
	}
	else if (!(adv = adventure_proto(atoi(argument)))) {
		msg_to_char(ch, "Unknown adventure '%s'.\r\n", argument);
		return;
	}
	else if (!player_can_olc_edit(ch, OLC_ADVENTURE, GET_ADV_VNUM(adv))) {
		msg_to_char(ch, "You don't have permission to edit that adventure.\r\n");
		return;
	}
	else if (GET_ADV_MIN_LEVEL(adv) == 0 && GET_ADV_MAX_LEVEL(adv) == 0) {
		msg_to_char(ch, "That adventure has no scaling constraints.\r\n");
		return;
	}
	
	// mobs first
	HASH_ITER(hh, mobile_table, mob, next_mob) {
		if (GET_MOB_VNUM(mob) < GET_ADV_START_VNUM(adv) || GET_MOB_VNUM(mob) > GET_ADV_END_VNUM(adv)) {
			continue;	// not in this adventure
		}
		
		// READY TO ATTEMPT (will report errors after this
		*line = '\0';
		
		if (GET_MIN_SCALE_LEVEL(mob) != GET_ADV_MIN_LEVEL(adv) || GET_MAX_SCALE_LEVEL(mob) != GET_ADV_MAX_LEVEL(adv)) {
			safe_snprintf(line, sizeof(line), "has levels %d-%d", GET_MIN_SCALE_LEVEL(mob), GET_MAX_SCALE_LEVEL(mob));
		}
		else if (!player_can_olc_edit(ch, OLC_MOBILE, GET_MOB_VNUM(mob))) {
			safe_snprintf(line, sizeof(line), "no permission");
		}
		else {
			GET_MIN_SCALE_LEVEL(mob) = 0;
			GET_MAX_SCALE_LEVEL(mob) = 0;
			safe_snprintf(line, sizeof(line), "removed");
			save_mobs = TRUE;
		}
		
		msg_to_char(ch, "MOB [%d] %s - %s\r\n", GET_MOB_VNUM(mob), GET_SHORT_DESC(mob), line);
	}
	
	// objs second
	HASH_ITER(hh, object_table, obj, next_obj) {
		if (GET_OBJ_VNUM(obj) < GET_ADV_START_VNUM(adv) || GET_OBJ_VNUM(obj) > GET_ADV_END_VNUM(adv)) {
			continue;	// not in this adventure
		}
		
		// READY TO ATTEMPT (will report errors after this
		*line = '\0';
		
		if (GET_OBJ_MIN_SCALE_LEVEL(obj) != GET_ADV_MIN_LEVEL(adv) || GET_OBJ_MAX_SCALE_LEVEL(obj) != GET_ADV_MAX_LEVEL(adv)) {
			safe_snprintf(line, sizeof(line), "has levels %d-%d", GET_OBJ_MIN_SCALE_LEVEL(obj), GET_OBJ_MAX_SCALE_LEVEL(obj));
		}
		else if (!player_can_olc_edit(ch, OLC_OBJECT, GET_OBJ_VNUM(obj))) {
			safe_snprintf(line, sizeof(line), "no permission");
		}
		else if (!OBJ_FLAGGED(obj, OBJ_SCALABLE)) {
			safe_snprintf(line, sizeof(line), "not scalable");
		}
		else {
			obj->proto_data->min_scale_level = 0;
			obj->proto_data->max_scale_level = 0;
			OBJ_VERSION(obj) += 1;
			safe_snprintf(line, sizeof(line), "updated");
			save_objs = TRUE;
		}
		
		msg_to_char(ch, "OBJ [%d] %s - %s\r\n", GET_OBJ_VNUM(obj), GET_OBJ_SHORT_DESC(obj), line);
	}
	
	// saves
	last = -1;
	for (iter = GET_ADV_START_VNUM(adv); iter <= GET_ADV_END_VNUM(adv); ++iter) {
		if ((int)(iter / 100) != last) {
			last = iter / 100;	// once per 100 vnum block in the adventure
			
			if (save_mobs) {
				save_library_file_for_vnum(DB_BOOT_MOB, iter);
			}
			if (save_objs) {
				save_library_file_for_vnum(DB_BOOT_OBJ, iter);
			}
		}
	}
	
	if (save_objs || save_mobs) {
		syslog(SYS_OLC, GET_INVIS_LEV(ch), TRUE, "OLC: %s un-cascaded levels for adventure [%d] %s", GET_NAME(ch), GET_ADV_VNUM(adv), GET_ADV_NAME(adv));
	}
}
