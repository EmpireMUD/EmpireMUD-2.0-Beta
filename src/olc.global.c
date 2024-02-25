/* ************************************************************************
*   File: olc.global.c                                    EmpireMUD 2.0b5 *
*  Usage: OLC for globals                                                 *
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
const char *default_glb_name = "Unnamed Global";


 //////////////////////////////////////////////////////////////////////////////
//// HELPERS /////////////////////////////////////////////////////////////////

/**
* Checks for common global table problems and reports them to ch.
*
* @param struct global_data *global The item to audit.
* @param char_data *ch The person to report to.
* @return bool TRUE if any problems were reported; FALSE if all good.
*/
bool audit_global(struct global_data *glb, char_data *ch) {
	char buf[MAX_STRING_LENGTH];
	struct interaction_item *interact;
	struct spawn_info *spawn;
	bool problem = FALSE;

	if (IS_SET(GET_GLOBAL_FLAGS(glb), GLB_FLAG_IN_DEVELOPMENT)) {
		olc_audit_msg(ch, GET_GLOBAL_VNUM(glb), "IN-DEVELOPMENT");
		problem = TRUE;
	}
	if (IS_SET(GET_GLOBAL_FLAGS(glb), GLB_FLAG_CUMULATIVE_PERCENT) && IS_SET(GET_GLOBAL_FLAGS(glb), GLB_FLAG_CHOOSE_LAST)) {
		olc_audit_msg(ch, GET_GLOBAL_VNUM(glb), "Has both CUMULATIVE-PRC and CHOOSE-LAST");
		problem = TRUE;
	}
	if (GET_GLOBAL_MIN_LEVEL(glb) > GET_GLOBAL_MAX_LEVEL(glb) && GET_GLOBAL_MAX_LEVEL(glb) > 0) {
		olc_audit_msg(ch, GET_GLOBAL_VNUM(glb), "Min level is greater than max level");
		problem = TRUE;
	}
	if ((GET_GLOBAL_TYPE_FLAGS(glb) & GET_GLOBAL_TYPE_EXCLUDE(glb)) != 0) {
		olc_audit_msg(ch, GET_GLOBAL_VNUM(glb), "Same flags in required and excluded set");
		problem = TRUE;
	}
	
	for (interact = GET_GLOBAL_INTERACTIONS(glb); interact; interact = interact->next) {
		// GLOBAL_x
		switch (GET_GLOBAL_TYPE(glb)) {
			case GLOBAL_MOB_INTERACTIONS: {
				if (interact->type != INTERACT_SHEAR && interact->type != INTERACT_LOOT && interact->type != INTERACT_PICKPOCKET) {
					olc_audit_msg(ch, GET_GLOBAL_VNUM(glb), "Unsupported interaction type");
					problem = TRUE;
				}
				break;
			}
			case GLOBAL_OBJ_INTERACTIONS: {
				if (interact->type != INTERACT_DISENCHANT) {
					olc_audit_msg(ch, GET_GLOBAL_VNUM(glb), "Unsupported interaction type");
					problem = TRUE;
				}
				break;
			}
			case GLOBAL_MINE_DATA: {
				if (interact->type != INTERACT_MINE) {
					olc_audit_msg(ch, GET_GLOBAL_VNUM(glb), "Unsupported interaction type");
					problem = TRUE;
				}
				break;
			}
			case GLOBAL_NEWBIE_GEAR: {
				break;
			}
			case GLOBAL_MAP_SPAWNS: {
				break;
			}
		}
	}
	
	// audit spawns
	LL_FOREACH(GET_GLOBAL_SPAWNS(glb), spawn) {
		if ((spawn->flags & GET_GLOBAL_SPARE_BITS(glb)) != NOBITS) {
			prettier_sprintbit(spawn->flags & GET_GLOBAL_SPARE_BITS(glb), spawn_flags, buf);
			olc_audit_msg(ch, GET_GLOBAL_VNUM(glb), "Spawn has same flags as the global's spawn flags: %s", buf);
			problem = TRUE;
		}
	}
		
	return problem;
}


/**
* Creates a new global entry.
* 
* @param any_vnum vnum The number to create.
* @return struct global_data* The new global's prototype.
*/
struct global_data *create_global_table_entry(any_vnum vnum) {
	struct global_data *glb;
	
	// sanity
	if (global_proto(vnum)) {
		log("SYSERR: Attempting to insert global at existing vnum %d", vnum);
		return global_proto(vnum);
	}
	
	CREATE(glb, struct global_data, 1);
	GET_GLOBAL_VNUM(glb) = vnum;
	add_global_to_table(glb);

	// save index and global file now
	save_index(DB_BOOT_GLB);
	save_library_file_for_vnum(DB_BOOT_GLB, vnum);

	return glb;
}


/**
* For the .list command.
*
* @param struct global_data *glb The thing to list.
* @param bool detail If TRUE, provide additional details
* @return char* The line to show (without a CRLF).
*/
char *list_one_global(struct global_data *glb, bool detail) {
	static char output[MAX_STRING_LENGTH];
	char abil[MAX_STRING_LENGTH], flags[MAX_STRING_LENGTH], flags2[MAX_STRING_LENGTH];
	ability_data *ab;
	
	// ability required
	if (!(ab = find_ability_by_vnum(GET_GLOBAL_ABILITY(glb)))) {
		*abil = '\0';
	}
	else {
		sprintf(abil, " (%s", ABIL_NAME(ab));
		if (ABIL_ASSIGNED_SKILL(ab)) {
			sprintf(abil + strlen(abil), " - %s %d", SKILL_ABBREV(ABIL_ASSIGNED_SKILL(ab)), ABIL_SKILL_LEVEL(ab));
		}
		strcat(abil, ")");
	}
	
	// GLOBAL_x
	switch (GET_GLOBAL_TYPE(glb)) {
		case GLOBAL_MOB_INTERACTIONS: {
			if (detail) {
				sprintbit(GET_GLOBAL_TYPE_FLAGS(glb), action_bits, flags, TRUE);
				snprintf(output, sizeof(output), "[%5d] %s (%s)%s %.2f%%%s %s (%s)", GET_GLOBAL_VNUM(glb), GET_GLOBAL_NAME(glb), level_range_string(GET_GLOBAL_MIN_LEVEL(glb), GET_GLOBAL_MAX_LEVEL(glb), 0), abil, GET_GLOBAL_PERCENT(glb), IS_SET(GET_GLOBAL_FLAGS(glb), GLB_FLAG_CUMULATIVE_PERCENT) ? "C" : "", flags, global_types[GET_GLOBAL_TYPE(glb)]);
			}
			else {
				snprintf(output, sizeof(output), "[%5d] %s (%s)", GET_GLOBAL_VNUM(glb), GET_GLOBAL_NAME(glb), global_types[GET_GLOBAL_TYPE(glb)]);
			}
			break;
		}
		case GLOBAL_OBJ_INTERACTIONS: {
			if (detail) {
				sprintbit(GET_GLOBAL_TYPE_FLAGS(glb), extra_bits, flags, TRUE);
				snprintf(output, sizeof(output), "[%5d] %s (%s)%s %.2f%%%s %s (%s)", GET_GLOBAL_VNUM(glb), GET_GLOBAL_NAME(glb), level_range_string(GET_GLOBAL_MIN_LEVEL(glb), GET_GLOBAL_MAX_LEVEL(glb), 0), abil, GET_GLOBAL_PERCENT(glb), IS_SET(GET_GLOBAL_FLAGS(glb), GLB_FLAG_CUMULATIVE_PERCENT) ? "C" : "", flags, global_types[GET_GLOBAL_TYPE(glb)]);
			}
			else {
				snprintf(output, sizeof(output), "[%5d] %s (%s)", GET_GLOBAL_VNUM(glb), GET_GLOBAL_NAME(glb), global_types[GET_GLOBAL_TYPE(glb)]);
			}
			break;
		}
		case GLOBAL_MINE_DATA: {
			if (detail) {
				sprintbit(GET_GLOBAL_TYPE_FLAGS(glb), sector_flags, flags, TRUE);
				snprintf(output, sizeof(output), "[%5d] %s%s %.2f%%%s %s (%s)", GET_GLOBAL_VNUM(glb), GET_GLOBAL_NAME(glb), abil, GET_GLOBAL_PERCENT(glb), IS_SET(GET_GLOBAL_FLAGS(glb), GLB_FLAG_CUMULATIVE_PERCENT) ? "C" : "", flags, global_types[GET_GLOBAL_TYPE(glb)]);
			}
			else {
				snprintf(output, sizeof(output), "[%5d] %s (%s)", GET_GLOBAL_VNUM(glb), GET_GLOBAL_NAME(glb), global_types[GET_GLOBAL_TYPE(glb)]);
			}
			break;
		}
		case GLOBAL_MAP_SPAWNS: {
			if (detail) {
				ordered_sprintbit(GET_GLOBAL_TYPE_FLAGS(glb), climate_flags, climate_flags_order, TRUE, flags);
				sprintbit(GET_GLOBAL_SPARE_BITS(glb), spawn_flags_short, flags2, TRUE);
				snprintf(output, sizeof(output), "[%5d] %s%s %.2f%%%s %s | %s(%s)", GET_GLOBAL_VNUM(glb), GET_GLOBAL_NAME(glb), abil, GET_GLOBAL_PERCENT(glb), IS_SET(GET_GLOBAL_FLAGS(glb), GLB_FLAG_CUMULATIVE_PERCENT) ? "C" : "", flags, flags2, global_types[GET_GLOBAL_TYPE(glb)]);
			}
			else {
				snprintf(output, sizeof(output), "[%5d] %s (%s)", GET_GLOBAL_VNUM(glb), GET_GLOBAL_NAME(glb), global_types[GET_GLOBAL_TYPE(glb)]);
			}
			break;
		}
		default: {
			if (detail) {
				snprintf(output, sizeof(output), "[%5d] %s%s %.2f%%%s (%s)", GET_GLOBAL_VNUM(glb), GET_GLOBAL_NAME(glb), abil, GET_GLOBAL_PERCENT(glb), IS_SET(GET_GLOBAL_FLAGS(glb), GLB_FLAG_CUMULATIVE_PERCENT) ? "C" : "", global_types[GET_GLOBAL_TYPE(glb)]);
			}
			else {
				snprintf(output, sizeof(output), "[%5d] %s (%s)", GET_GLOBAL_VNUM(glb), GET_GLOBAL_NAME(glb), global_types[GET_GLOBAL_TYPE(glb)]);
			}
			break;
		}
	}
		
	return output;
}


/**
* WARNING: This function actually deletes a global.
*
* @param char_data *ch The person doing the deleting.
* @param any_vnum vnum The vnum to delete.
*/
void olc_delete_global(char_data *ch, any_vnum vnum) {
	struct global_data *glb;
	char name[256];
	
	if (!(glb = global_proto(vnum))) {
		msg_to_char(ch, "There is no such global %d.\r\n", vnum);
		return;
	}
	
	snprintf(name, sizeof(name), "%s", NULLSAFE(GET_GLOBAL_NAME(glb)));
	
	if (HASH_COUNT(globals_table) <= 1) {
		msg_to_char(ch, "You can't delete the last global.\r\n");
		return;
	}

	// remove it from the hash table first
	remove_global_from_table(glb);

	// save index and global file now
	save_index(DB_BOOT_GLB);
	save_library_file_for_vnum(DB_BOOT_GLB, vnum);
	
	syslog(SYS_OLC, GET_INVIS_LEV(ch), TRUE, "OLC: %s has deleted global %d %s", GET_NAME(ch), vnum, name);
	msg_to_char(ch, "Global %d (%s) deleted.\r\n", vnum, name);
	
	free_global(glb);
}


/**
* Searches for all uses of a global and displays them.
*
* @param char_data *ch The player.
* @param any_vnum vnum The global vnum.
*/
void olc_search_global(char_data *ch, any_vnum vnum) {
	struct global_data *glb = global_proto(vnum);
	int found;
	
	if (!glb) {
		msg_to_char(ch, "There is no global %d.\r\n", vnum);
		return;
	}
	
	found = 0;
	build_page_display(ch, "Occurrences of global %d (%s):", vnum, GET_GLOBAL_NAME(glb));
	
	// globals are not actually used anywhere else
	
	if (found > 0) {
		build_page_display(ch, "%d location%s shown", found, PLURAL(found));
	}
	else {
		build_page_display(ch, " none");
	}
	
	send_page_display(ch);
}


/**
* Function to save a player's changes to a global (or a new one).
*
* @param descriptor_data *desc The descriptor who is saving.
*/
void save_olc_global(descriptor_data *desc) {	
	struct global_data *proto, *glb = GET_OLC_GLOBAL(desc);
	any_vnum vnum = GET_OLC_VNUM(desc);
	struct spawn_info *spawn;
	UT_hash_handle hh;

	// have a place to save it?
	if (!(proto = global_proto(vnum))) {
		proto = create_global_table_entry(vnum);
	}
	
	// free prototype strings and pointers
	if (GET_GLOBAL_NAME(proto)) {
		free(GET_GLOBAL_NAME(proto));
	}
	free_interactions(&GET_GLOBAL_INTERACTIONS(proto));
	free_archetype_gear(GET_GLOBAL_GEAR(proto));
	while ((spawn = GET_GLOBAL_SPAWNS(proto))) {
		GET_GLOBAL_SPAWNS(proto) = spawn->next;
		free(spawn);
	}
	
	// sanity
	if (!GET_GLOBAL_NAME(glb) || !*GET_GLOBAL_NAME(glb)) {
		if (GET_GLOBAL_NAME(glb)) {
			free(GET_GLOBAL_NAME(glb));
		}
		GET_GLOBAL_NAME(glb) = str_dup(default_glb_name);
	}

	// save data back over the proto-type
	hh = proto->hh;	// save old hash handle
	*proto = *glb;	// copy over all data
	proto->vnum = vnum;	// ensure correct vnum
	proto->hh = hh;	// restore old hash handle
		
	// and save to file
	save_library_file_for_vnum(DB_BOOT_GLB, vnum);
}


/**
* Creates a copy of a global, or clears a new one, for editing.
* 
* @param struct global_data *input The global to copy, or NULL to make a new one.
* @return struct global_data* The copied global.
*/
struct global_data *setup_olc_global(struct global_data *input) {
	struct global_data *new;
	
	CREATE(new, struct global_data, 1);
	clear_global(new);
	
	if (input) {
		// copy normal data
		*new = *input;

		// copy things that are pointers
		GET_GLOBAL_NAME(new) = GET_GLOBAL_NAME(input) ? str_dup(GET_GLOBAL_NAME(input)) : NULL;
		
		// copy pointers
		GET_GLOBAL_INTERACTIONS(new) = copy_interaction_list(GET_GLOBAL_INTERACTIONS(input));
		GET_GLOBAL_GEAR(new) = copy_archetype_gear(GET_GLOBAL_GEAR(input));
		GET_GLOBAL_SPAWNS(new) = copy_spawn_list(GET_GLOBAL_SPAWNS(input));
	}
	else {
		// brand new: some defaults
		GET_GLOBAL_NAME(new) = str_dup(default_glb_name);
		GET_GLOBAL_FLAGS(new) = GLB_FLAG_IN_DEVELOPMENT;
	}
	
	// done
	return new;	
}


/**
* Counts the words of text in a global's strings.
*
* @param struct global_data *glb The global whose strings to count.
* @return int The number of words in the global's strings.
*/
int wordcount_global(struct global_data *glb) {
	int count = 0;
	
	// not player-facing
	// count += wordcount_string(GET_GLOBAL_NAME(glb));
	
	return count;
}


 //////////////////////////////////////////////////////////////////////////////
//// DISPLAYS ////////////////////////////////////////////////////////////////

/**
* This is the main recipe display for global OLC. It displays the user's
* currently-edited global.
*
* @param char_data *ch The person who is editing a global and will see its display.
*/
void olc_show_global(char_data *ch) {
	struct global_data *glb = GET_OLC_GLOBAL(ch->desc);
	char lbuf[MAX_STRING_LENGTH];
	struct spawn_info *spawn;
	ability_data *abil;
	int count;
	
	if (!glb) {
		return;
	}
	
	build_page_display(ch, "[%s%d\t0] %s%s\t0", OLC_LABEL_CHANGED, GET_OLC_VNUM(ch->desc), OLC_LABEL_UNCHANGED, !global_proto(GET_GLOBAL_VNUM(glb)) ? "new global" : GET_GLOBAL_NAME(global_proto(GET_GLOBAL_VNUM(glb))));
	build_page_display(ch, "<%sname\t0> %s", OLC_LABEL_STR(GET_GLOBAL_NAME(glb), default_glb_name), NULLSAFE(GET_GLOBAL_NAME(glb)));
	
	build_page_display(ch, "<%stype\t0> %s", OLC_LABEL_VAL(GET_GLOBAL_TYPE(glb), 0), global_types[GET_GLOBAL_TYPE(glb)]);

	sprintbit(GET_GLOBAL_FLAGS(glb), global_flags, lbuf, TRUE);
	build_page_display(ch, "<%sflags\t0> %s", OLC_LABEL_VAL(GET_GLOBAL_FLAGS(glb), GLB_FLAG_IN_DEVELOPMENT), lbuf);
	
	if (GET_GLOBAL_TYPE(glb) != GLOBAL_NEWBIE_GEAR && GET_GLOBAL_TYPE(glb) != GLOBAL_MAP_SPAWNS) {
		if (GET_GLOBAL_MIN_LEVEL(glb) == 0) {
			build_page_display(ch, "<%sminlevel\t0> none", OLC_LABEL_UNCHANGED);
		}
		else {
			build_page_display(ch, "<%sminlevel\t0> %d", OLC_LABEL_CHANGED, GET_GLOBAL_MIN_LEVEL(glb));
		}
	
		if (GET_GLOBAL_MAX_LEVEL(glb) == 0) {
			build_page_display(ch, "<%smaxlevel\t0> none", OLC_LABEL_UNCHANGED);
		}
		else {
			build_page_display(ch, "<%smaxlevel\t0> %d", OLC_LABEL_CHANGED, GET_GLOBAL_MAX_LEVEL(glb));
		}
	
		// ability required
		if (!(abil = find_ability_by_vnum(GET_GLOBAL_ABILITY(glb)))) {
			strcpy(lbuf, "none");
		}
		else {
			sprintf(lbuf, "%s", ABIL_NAME(abil));
			if (ABIL_ASSIGNED_SKILL(abil)) {
				sprintf(lbuf + strlen(lbuf), " (%s %d)", SKILL_NAME(ABIL_ASSIGNED_SKILL(abil)), ABIL_SKILL_LEVEL(abil));
			}
		}
		build_page_display(ch, "<%srequiresability\t0> %s", OLC_LABEL_PTR(abil), lbuf);
	}
	
	build_page_display(ch, "<%spercent\t0> %.2f%%", OLC_LABEL_VAL(GET_GLOBAL_PERCENT(glb), 100.0), GET_GLOBAL_PERCENT(glb));
	
	// GLOBAL_x: type-based data
	switch (GET_GLOBAL_TYPE(glb)) {
		case GLOBAL_MOB_INTERACTIONS: {
			sprintbit(GET_GLOBAL_TYPE_FLAGS(glb), action_bits, lbuf, TRUE);
			build_page_display(ch, "<%smobflags\t0> %s", OLC_LABEL_VAL(GET_GLOBAL_TYPE_FLAGS(glb), NOBITS), lbuf);
			sprintbit(GET_GLOBAL_TYPE_EXCLUDE(glb), action_bits, lbuf, TRUE);
			build_page_display(ch, "<%smobexclude\t0> %s", OLC_LABEL_VAL(GET_GLOBAL_TYPE_EXCLUDE(glb), NOBITS), lbuf);

			build_page_display(ch, "Interactions: <%sinteraction\t0>", OLC_LABEL_PTR(GET_GLOBAL_INTERACTIONS(glb)));
			if (GET_GLOBAL_INTERACTIONS(glb)) {
				show_interaction_display(ch, GET_GLOBAL_INTERACTIONS(glb), FALSE);
			}
			break;
		}
		case GLOBAL_OBJ_INTERACTIONS: {
			sprintbit(GET_GLOBAL_TYPE_FLAGS(glb), extra_bits, lbuf, TRUE);
			build_page_display(ch, "<%sobjflags\t0> %s", OLC_LABEL_VAL(GET_GLOBAL_TYPE_FLAGS(glb), NOBITS), lbuf);
			sprintbit(GET_GLOBAL_TYPE_EXCLUDE(glb), extra_bits, lbuf, TRUE);
			build_page_display(ch, "<%sobjexclude\t0> %s", OLC_LABEL_VAL(GET_GLOBAL_TYPE_EXCLUDE(glb), NOBITS), lbuf);

			build_page_display(ch, "Interactions: <%sinteraction\t0>", OLC_LABEL_PTR(GET_GLOBAL_INTERACTIONS(glb)));
			if (GET_GLOBAL_INTERACTIONS(glb)) {
				show_interaction_display(ch, GET_GLOBAL_INTERACTIONS(glb), FALSE);
			}
			break;
		}
		case GLOBAL_MINE_DATA: {
			sprintbit(GET_GLOBAL_TYPE_FLAGS(glb), sector_flags, lbuf, TRUE);
			build_page_display(ch, "<%ssectorflags\t0> %s", OLC_LABEL_VAL(GET_GLOBAL_TYPE_FLAGS(glb), NOBITS), lbuf);
			sprintbit(GET_GLOBAL_TYPE_EXCLUDE(glb), sector_flags, lbuf, TRUE);
			build_page_display(ch, "<%ssectorexclude\t0> %s", OLC_LABEL_VAL(GET_GLOBAL_TYPE_EXCLUDE(glb), NOBITS), lbuf);
			build_page_display(ch, "<%scapacity\t0> %d ore (%d-%d normal, %d-%d deep)", OLC_LABEL_VAL(GET_GLOBAL_VAL(glb, GLB_VAL_MAX_MINE_SIZE), 0), GET_GLOBAL_VAL(glb, GLB_VAL_MAX_MINE_SIZE), GET_GLOBAL_VAL(glb, GLB_VAL_MAX_MINE_SIZE)/2, GET_GLOBAL_VAL(glb, GLB_VAL_MAX_MINE_SIZE), (int)(GET_GLOBAL_VAL(glb, GLB_VAL_MAX_MINE_SIZE) / 2.0 * 1.5), (int)(GET_GLOBAL_VAL(glb, GLB_VAL_MAX_MINE_SIZE) * 1.5));
	
			build_page_display(ch, "Interactions: <%sinteraction\t0>", OLC_LABEL_PTR(GET_GLOBAL_INTERACTIONS(glb)));
			if (GET_GLOBAL_INTERACTIONS(glb)) {
				show_interaction_display(ch, GET_GLOBAL_INTERACTIONS(glb), FALSE);
			}
			break;
		}
		case GLOBAL_NEWBIE_GEAR: {
			build_page_display(ch, "Gear: <%sgear\t0>", OLC_LABEL_PTR(GET_GLOBAL_GEAR(glb)));
			if (GET_GLOBAL_GEAR(glb)) {
				show_archetype_gear_display(ch, GET_GLOBAL_GEAR(glb), FALSE);
			}
			break;
		}
		case GLOBAL_MAP_SPAWNS: {
			ordered_sprintbit(GET_GLOBAL_TYPE_FLAGS(glb), climate_flags, climate_flags_order, TRUE, lbuf);
			build_page_display(ch, "<%sclimateflags\t0> %s", OLC_LABEL_VAL(GET_GLOBAL_TYPE_FLAGS(glb), NOBITS), lbuf);
			ordered_sprintbit(GET_GLOBAL_TYPE_EXCLUDE(glb), climate_flags, climate_flags_order, TRUE, lbuf);
			build_page_display(ch, "<%sclimateexclude\t0> %s", OLC_LABEL_VAL(GET_GLOBAL_TYPE_EXCLUDE(glb), NOBITS), lbuf);
			sprintbit(GET_GLOBAL_SPARE_BITS(glb), spawn_flags, lbuf, TRUE);
			build_page_display(ch, "<%sspawnflags\t0> %s", OLC_LABEL_VAL(GET_GLOBAL_SPARE_BITS(glb), NOBITS), lbuf);
	
			build_page_display(ch, "<%sspawns\t0>", OLC_LABEL_PTR(GET_GLOBAL_SPAWNS(glb)));
			if (GET_GLOBAL_SPAWNS(glb)) {
				count = 0;
				LL_FOREACH(GET_GLOBAL_SPAWNS(glb), spawn) {
					sprintbit(spawn->flags, spawn_flags, lbuf, TRUE);
					build_page_display(ch, " %d. %s (%d) %.2f%% %s", ++count, skip_filler(get_mob_name_by_proto(spawn->vnum, FALSE)), spawn->vnum, spawn->percent, lbuf);
				}
			}
			break;
		}
	}
	
	send_page_display(ch);
}


 //////////////////////////////////////////////////////////////////////////////
//// EDIT MODULES ////////////////////////////////////////////////////////////

OLC_MODULE(gedit_ability) {
	struct global_data *glb = GET_OLC_GLOBAL(ch->desc);
	ability_data *abil;
	
	if (GET_GLOBAL_TYPE(glb) == GLOBAL_NEWBIE_GEAR || GET_GLOBAL_TYPE(glb) == GLOBAL_MAP_SPAWNS) {
		msg_to_char(ch, "This global type can't use abilities.\r\n");
		return;
	}
	
	if (!*argument) {
		msg_to_char(ch, "Require what ability (or 'none')?\r\n");
	}
	else if (!str_cmp(argument, "none")) {
		GET_GLOBAL_ABILITY(glb) = NO_ABIL;
		
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
		GET_GLOBAL_ABILITY(glb) = ABIL_VNUM(abil);
		
		if (PRF_FLAGGED(ch, PRF_NOREPEAT)) {
			send_config_msg(ch, "ok_string");
		}
		else {
			msg_to_char(ch, "It now requires the %s ability.\r\n", ABIL_NAME(abil));
		}
	}
}


OLC_MODULE(gedit_capacity) {
	struct global_data *glb = GET_OLC_GLOBAL(ch->desc);
	
	if (GET_GLOBAL_TYPE(glb) != GLOBAL_MINE_DATA) {
		msg_to_char(ch, "You can't set mine capacity on this type.\r\n");
	}
	else {
		GET_GLOBAL_VAL(glb, GLB_VAL_MAX_MINE_SIZE) = olc_process_number(ch, argument, "mine capacity", "capacity", 1, 1000, GET_GLOBAL_VAL(glb, GLB_VAL_MAX_MINE_SIZE));
	}
}


OLC_MODULE(gedit_climateexclude) {
	struct global_data *glb = GET_OLC_GLOBAL(ch->desc);
	
	if (GET_GLOBAL_TYPE(glb) != GLOBAL_MAP_SPAWNS) {
		msg_to_char(ch, "You can't set climateexclude on this type.\r\n");
	}
	else {
		GET_GLOBAL_TYPE_EXCLUDE(glb) = olc_process_flag(ch, argument, "climate", "climateexclude", climate_flags, GET_GLOBAL_TYPE_EXCLUDE(glb));
	}
}


OLC_MODULE(gedit_climateflags) {
	struct global_data *glb = GET_OLC_GLOBAL(ch->desc);
	
	if (GET_GLOBAL_TYPE(glb) != GLOBAL_MAP_SPAWNS) {
		msg_to_char(ch, "You can't set climateflags on this type.\r\n");
	}
	else {
		GET_GLOBAL_TYPE_FLAGS(glb) = olc_process_flag(ch, argument, "climate", "climateflags", climate_flags, GET_GLOBAL_TYPE_FLAGS(glb));
	}
}


OLC_MODULE(gedit_flags) {
	struct global_data *glb = GET_OLC_GLOBAL(ch->desc);
	bool had_indev = IS_SET(GET_GLOBAL_FLAGS(glb), GLB_FLAG_IN_DEVELOPMENT) ? TRUE : FALSE;
	
	GET_GLOBAL_FLAGS(glb) = olc_process_flag(ch, argument, "global", "flags", global_flags, GET_GLOBAL_FLAGS(glb));
	
	// validate removal of IN-DEVELOPMENT
	if (had_indev && !IS_SET(GET_GLOBAL_FLAGS(glb), GLB_FLAG_IN_DEVELOPMENT) && GET_ACCESS_LEVEL(ch) < LVL_UNRESTRICTED_BUILDER && !OLC_FLAGGED(ch, OLC_FLAG_CLEAR_IN_DEV)) {
		msg_to_char(ch, "You don't have permission to remove the IN-DEVELOPMENT flag.\r\n");
		SET_BIT(GET_GLOBAL_FLAGS(glb), GLB_FLAG_IN_DEVELOPMENT);
	}
}


OLC_MODULE(gedit_gear) {
	struct global_data *glb = GET_OLC_GLOBAL(ch->desc);
	
	if (GET_GLOBAL_TYPE(glb) != GLOBAL_NEWBIE_GEAR) {
		msg_to_char(ch, "You can't set gear on this type.\r\n");
	}
	else {
		archedit_process_gear(ch, argument, &GET_GLOBAL_GEAR(glb));
	}
}


OLC_MODULE(gedit_interaction) {
	struct global_data *glb = GET_OLC_GLOBAL(ch->desc);
	int itype = TYPE_MOB;
	
	switch (GET_GLOBAL_TYPE(glb)) {
		case GLOBAL_MOB_INTERACTIONS: {
			itype = TYPE_MOB;
			break;
		}
		case GLOBAL_OBJ_INTERACTIONS: {
			itype = TYPE_OBJ;
			break;
		}
		case GLOBAL_MINE_DATA: {
			itype = TYPE_MINE_DATA;
			break;
		}
		default: {
			msg_to_char(ch, "You can't set interactions on this type.\r\n");
			return;
		}
	}
	
	olc_process_interactions(ch, argument, &GET_GLOBAL_INTERACTIONS(glb), itype);
}


OLC_MODULE(gedit_maxlevel) {
	struct global_data *glb = GET_OLC_GLOBAL(ch->desc);
	
	if (GET_GLOBAL_TYPE(glb) == GLOBAL_NEWBIE_GEAR || GET_GLOBAL_TYPE(glb) == GLOBAL_MAP_SPAWNS) {
		msg_to_char(ch, "This global type can't require levels.\r\n");
		return;
	}
	
	GET_GLOBAL_MAX_LEVEL(glb) = olc_process_number(ch, argument, "maximum level", "maxlevel", 0, MAX_INT, GET_GLOBAL_MAX_LEVEL(glb));
}


OLC_MODULE(gedit_minlevel) {
	struct global_data *glb = GET_OLC_GLOBAL(ch->desc);
	
	if (GET_GLOBAL_TYPE(glb) == GLOBAL_NEWBIE_GEAR || GET_GLOBAL_TYPE(glb) == GLOBAL_MAP_SPAWNS) {
		msg_to_char(ch, "This global type can't require levels.\r\n");
		return;
	}
	
	GET_GLOBAL_MIN_LEVEL(glb) = olc_process_number(ch, argument, "minimum level", "minlevel", 0, MAX_INT, GET_GLOBAL_MIN_LEVEL(glb));
}


OLC_MODULE(gedit_mobexclude) {
	struct global_data *glb = GET_OLC_GLOBAL(ch->desc);
	
	if (GET_GLOBAL_TYPE(glb) != GLOBAL_MOB_INTERACTIONS) {
		msg_to_char(ch, "You can't set mobexclude on this type.\r\n");
	}
	else {
		GET_GLOBAL_TYPE_EXCLUDE(glb) = olc_process_flag(ch, argument, "mob", "mobexclude", action_bits, GET_GLOBAL_TYPE_EXCLUDE(glb));
	}
}


OLC_MODULE(gedit_mobflags) {
	struct global_data *glb = GET_OLC_GLOBAL(ch->desc);
	
	if (GET_GLOBAL_TYPE(glb) != GLOBAL_MOB_INTERACTIONS) {
		msg_to_char(ch, "You can't set mobflags on this type.\r\n");
	}
	else {
		GET_GLOBAL_TYPE_FLAGS(glb) = olc_process_flag(ch, argument, "mob", "mobflags", action_bits, GET_GLOBAL_TYPE_FLAGS(glb));
	}
}


OLC_MODULE(gedit_name) {
	struct global_data *glb = GET_OLC_GLOBAL(ch->desc);
	olc_process_string(ch, argument, "name", &GET_GLOBAL_NAME(glb));
}


OLC_MODULE(gedit_objexclude) {
	struct global_data *glb = GET_OLC_GLOBAL(ch->desc);
	
	if (GET_GLOBAL_TYPE(glb) != GLOBAL_OBJ_INTERACTIONS) {
		msg_to_char(ch, "You can't set objexclude on this type.\r\n");
	}
	else {
		GET_GLOBAL_TYPE_EXCLUDE(glb) = olc_process_flag(ch, argument, "obj", "objexclude", extra_bits, GET_GLOBAL_TYPE_EXCLUDE(glb));
	}
}


OLC_MODULE(gedit_objflags) {
	struct global_data *glb = GET_OLC_GLOBAL(ch->desc);
	
	if (GET_GLOBAL_TYPE(glb) != GLOBAL_OBJ_INTERACTIONS) {
		msg_to_char(ch, "You can't set objflags on this type.\r\n");
	}
	else {
		GET_GLOBAL_TYPE_FLAGS(glb) = olc_process_flag(ch, argument, "obj", "objflags", extra_bits, GET_GLOBAL_TYPE_FLAGS(glb));
	}
}


OLC_MODULE(gedit_percent) {
	struct global_data *glb = GET_OLC_GLOBAL(ch->desc);
	GET_GLOBAL_PERCENT(glb) = olc_process_double(ch, argument, "percent", "percent", 0.01, 100.00, GET_GLOBAL_PERCENT(glb));
}


OLC_MODULE(gedit_sectorexclude) {
	struct global_data *glb = GET_OLC_GLOBAL(ch->desc);
	
	if (GET_GLOBAL_TYPE(glb) != GLOBAL_MINE_DATA) {
		msg_to_char(ch, "You can't set sectorexclude on this type.\r\n");
	}
	else {
		GET_GLOBAL_TYPE_EXCLUDE(glb) = olc_process_flag(ch, argument, "sector", "sectorexclude", sector_flags, GET_GLOBAL_TYPE_EXCLUDE(glb));
	}
}


OLC_MODULE(gedit_sectorflags) {
	struct global_data *glb = GET_OLC_GLOBAL(ch->desc);
	
	if (GET_GLOBAL_TYPE(glb) != GLOBAL_MINE_DATA) {
		msg_to_char(ch, "You can't set sectorflags on this type.\r\n");
	}
	else {
		GET_GLOBAL_TYPE_FLAGS(glb) = olc_process_flag(ch, argument, "sector", "sectorflags", sector_flags, GET_GLOBAL_TYPE_FLAGS(glb));
	}
}


OLC_MODULE(gedit_spawnflags) {
	struct global_data *glb = GET_OLC_GLOBAL(ch->desc);
	
	if (GET_GLOBAL_TYPE(glb) != GLOBAL_MAP_SPAWNS) {
		msg_to_char(ch, "You can't set spawnflags on this type.\r\n");
	}
	else {
		GET_GLOBAL_SPARE_BITS(glb) = olc_process_flag(ch, argument, "spawn", "spawnflags", spawn_flags, GET_GLOBAL_SPARE_BITS(glb));
	}
}


OLC_MODULE(gedit_spawns) {
	struct global_data *glb = GET_OLC_GLOBAL(ch->desc);
	olc_process_spawns(ch, argument, &GET_GLOBAL_SPAWNS(glb));
}


OLC_MODULE(gedit_type) {
	struct global_data *glb = GET_OLC_GLOBAL(ch->desc);
	int iter, old = GET_GLOBAL_TYPE(glb);
	struct spawn_info *spawn;
	
	GET_GLOBAL_TYPE(glb) = olc_process_type(ch, argument, "type", "type", global_types, GET_GLOBAL_TYPE(glb));
	
	// reset excludes/vals if type changes
	if (GET_GLOBAL_TYPE(glb) != old) {
		GET_GLOBAL_TYPE_FLAGS(glb) = NOBITS;
		GET_GLOBAL_TYPE_EXCLUDE(glb) = NOBITS;
		GET_GLOBAL_SPARE_BITS(glb) = NOBITS;
		for (iter = 0; iter < NUM_GLB_VAL_POSITIONS; ++iter) {
			GET_GLOBAL_VAL(glb, iter) = 0;
		}
		
		// more resets by type: things not used by that type
		switch (GET_GLOBAL_TYPE(glb)) {
			case GLOBAL_NEWBIE_GEAR:
			case GLOBAL_MAP_SPAWNS: {
				GET_GLOBAL_ABILITY(glb) = NO_ABIL;
				GET_GLOBAL_MAX_LEVEL(glb) = 0;
				GET_GLOBAL_MIN_LEVEL(glb) = 0;
				break;
			}
		}
		
		// remove gear
		free_archetype_gear(GET_GLOBAL_GEAR(glb));
		GET_GLOBAL_GEAR(glb) = NULL;
		
		// remove spawns too
		while ((spawn = GET_GLOBAL_SPAWNS(glb))) {
			GET_GLOBAL_SPAWNS(glb) = spawn->next;
			free(spawn);
		}
	}
}
