/* ************************************************************************
*   File: olc.sector.c                                    EmpireMUD 2.0b5 *
*  Usage: OLC for sector prototypes                                       *
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

// external consts
extern const char *bld_on_flags[];
extern const char *climate_types[];
extern const char *evo_types[];
extern const int evo_val_types[NUM_EVOS];
extern const char *interact_types[];
extern const byte interact_vnum_types[NUM_INTERACTS];
extern const char *mapout_color_names[];
extern const char *sector_flags[];
extern const char *spawn_flags[];
extern const char *icon_types[];

// external funcs
void init_sector(sector_data *st);
void sort_interactions(struct interaction_item **list);

// locals
const char *default_sect_name = "Unnamed Sector";
const char *default_sect_title = "An Unnamed Sector";
const char default_roadside_icon = '.';


 //////////////////////////////////////////////////////////////////////////////
//// HELPERS /////////////////////////////////////////////////////////////////

/**
* Checks for common sector problems and reports them to ch.
*
* @param sector_data *sect The item to audit.
* @param char_data *ch The person to report to.
* @return bool TRUE if any problems were reported; FALSE if all good.
*/
bool audit_sector(sector_data *sect, char_data *ch) {
	extern bool audit_interactions(any_vnum vnum, struct interaction_item *list, int attach_type, char_data *ch);
	extern bool audit_spawns(any_vnum vnum, struct spawn_info *list, char_data *ch);
	extern struct icon_data *get_icon_from_set(struct icon_data *set, int type);
	extern const char *icon_types[];
	
	char temp[MAX_STRING_LENGTH];
	bool problem = FALSE;
	int iter;
	
	// flags to audit for
	const bitvector_t odd_flags = SECTF_ADVENTURE | SECTF_NON_ISLAND | SECTF_START_LOCATION | SECTF_MAP_BUILDING | SECTF_INSIDE;
	
	if (!GET_SECT_NAME(sect) || !*GET_SECT_NAME(sect) || !str_cmp(GET_SECT_NAME(sect), default_sect_name)) {
		olc_audit_msg(ch, GET_SECT_VNUM(sect), "No name set");
		problem = TRUE;
	}
	if (GET_SECT_NAME(sect) && islower(*GET_SECT_NAME(sect))) {
		olc_audit_msg(ch, GET_SECT_VNUM(sect), "Missing capital in sector name");
		problem = TRUE;
	}
	
	if (!GET_SECT_TITLE(sect) || !*GET_SECT_TITLE(sect) || !str_cmp(GET_SECT_TITLE(sect), default_sect_title)) {
		olc_audit_msg(ch, GET_SECT_VNUM(sect), "No title set");
		problem = TRUE;
	}
	
	for (iter = 0; iter < NUM_TILESETS; ++iter) {
		if (!get_icon_from_set(GET_SECT_ICONS(sect), iter)) {
			olc_audit_msg(ch, GET_SECT_VNUM(sect), "No icon for '%s' tileset", icon_types[iter]);
			problem = TRUE;
		}
	}
	if (GET_SECT_CLIMATE(sect) == CLIMATE_NONE) {
		olc_audit_msg(ch, GET_SECT_VNUM(sect), "Climate not set");
		problem = TRUE;
	}
	if (!GET_SECT_SPAWNS(sect)) {
		olc_audit_msg(ch, GET_SECT_VNUM(sect), "No spawns set");
		problem = TRUE;
	}
	if (GET_SECT_MAPOUT(sect) == 0) {
		olc_audit_msg(ch, GET_SECT_VNUM(sect), "Mapout color not set (or set to starting location)");
		problem = TRUE;
	}
	if (!GET_SECT_MOVE_LOSS(sect)) {
		olc_audit_msg(ch, GET_SECT_VNUM(sect), "No movement cost");
		problem = TRUE;
	}
	
	if (SECT_FLAGGED(sect, odd_flags)) {
		sprintbit(GET_SECT_FLAGS(sect) & odd_flags, sector_flags, temp, TRUE);
		olc_audit_msg(ch, GET_SECT_VNUM(sect), "Unusual flag(s): %s", temp);
		problem = TRUE;
	}
	if (SECT_FLAGGED(sect, SECTF_OCEAN | SECTF_FRESH_WATER) && !has_interaction(GET_SECT_INTERACTIONS(sect), INTERACT_FISH)) {
		olc_audit_msg(ch, GET_SECT_VNUM(sect), "Water sect with no fish interaction");
		problem = TRUE;
	}
	
	if (IS_SET(GET_SECT_BUILD_FLAGS(sect), BLD_ON_BASE_TERRAIN_ALLOWED)) {
		olc_audit_msg(ch, GET_SECT_VNUM(sect), "Has the base-terrain-allowed build flag -- this is not allowed on sectors");
		problem = TRUE;
	}
	
	if (has_evolution_type(sect, EVO_CHOPPED_DOWN) && !has_interaction(GET_SECT_INTERACTIONS(sect), INTERACT_CHOP)) {
		olc_audit_msg(ch, GET_SECT_VNUM(sect), "Choppable sect with no chop interaction");
		problem = TRUE;
	}
	
	problem |= audit_interactions(GET_SECT_VNUM(sect), GET_SECT_INTERACTIONS(sect), TYPE_ROOM, ch);
	problem |= audit_spawns(GET_SECT_VNUM(sect), GET_SECT_SPAWNS(sect), ch);
	
	return problem;
}


/**
* Creates a new sector entry.
* 
* @param sector_vnum vnum The number to create.
* @return sector_data* The new sector.
*/
sector_data *create_sector_table_entry(sector_vnum vnum) {
	void add_sector_to_table(sector_data *sect);

	sector_data *sect;
	
	// sanity
	if (sector_proto(vnum)) {
		log("SYSERR: Attempting to insert sector at existing vnum %d", vnum);
		return sector_proto(vnum);
	}
	
	CREATE(sect, sector_data, 1);
	init_sector(sect);
	GET_SECT_VNUM(sect) = vnum;
	
	add_sector_to_table(sect);

	// save index and sector file now
	save_index(DB_BOOT_SECTOR);
	save_library_file_for_vnum(DB_BOOT_SECTOR, vnum);
	
	return sect;
}


/**
* Deletes all evolutions that involve a given sector vnum, from a list.
*
* @param sector_vnum vnum The sector vnum to remove from the evolution list.
* @param struct evolution_data **list A reference to the list to edit.
* @return bool TRUE if any evolutions were deleted.
*/
bool delete_sector_from_evolutions(sector_vnum vnum, struct evolution_data **list) {
	struct evolution_data *evo, *next_evo, *temp;
	bool found = FALSE;
	
	for (evo = *list; evo; evo = next_evo) {
		next_evo = evo->next;
		
		if (evo->becomes == vnum || (evo_val_types[evo->type] == EVO_VAL_SECTOR && evo->value == vnum)) {
			REMOVE_FROM_LIST(evo, *list, next);
			free(evo);
			found = TRUE;
		}
	}
	
	return found;
}


/**
* For the .list command.
*
* @param sector_data *sect The thing to list.
* @param bool detail If TRUE, provide additional details
* @return char* The line to show (without a CRLF).
*/
char *list_one_sector(sector_data *sect, bool detail) {
	static char output[MAX_STRING_LENGTH];
	
	if (detail) {
		snprintf(output, sizeof(output), "[%5d] %s", GET_SECT_VNUM(sect), GET_SECT_NAME(sect));
	}
	else {
		snprintf(output, sizeof(output), "[%5d] %s", GET_SECT_VNUM(sect), GET_SECT_NAME(sect));
	}
	
	return output;
}


/**
* WARNING: This function actually deletes a sector.
*
* @param char_data *ch The person doing the deleting.
* @param sector_vnum vnum The vnum to delete.
*/
void olc_delete_sector(char_data *ch, sector_vnum vnum) {
	extern bool delete_link_rule_by_type_value(struct adventure_link_rule **list, int type, any_vnum value);
	extern bool delete_requirement_from_list(struct req_data **list, int type, any_vnum vnum);
	void remove_sector_from_table(sector_data *sect);
	extern const sector_vnum climate_default_sector[NUM_CLIMATES];
	
	sector_data *sect, *sect_iter, *next_sect, *replace_sect;
	quest_data *quest, *next_quest;
	progress_data *prg, *next_prg;
	social_data *soc, *next_soc;
	descriptor_data *desc;
	adv_data *adv, *next_adv;
	struct map_data *map;
	room_data *room;
	int count, x, y;
	bool found;
	
	if (!(sect = sector_proto(vnum))) {
		msg_to_char(ch, "There is no such sector %d.\r\n", vnum);
		return;
	}
	
	if (HASH_COUNT(sector_table) <= 1) {
		msg_to_char(ch, "You can't delete the last sector.\r\n");
		return;
	}
	
	// remove it from the hash table first
	remove_sector_from_table(sect);

	// save index and sector file now
	save_index(DB_BOOT_SECTOR);
	save_library_file_for_vnum(DB_BOOT_SECTOR, vnum);
	
	// find a replacement sector for the world
	replace_sect = sector_proto(climate_default_sector[CLIMATE_TEMPERATE]);
	if (!replace_sect) {
		// just pull the first one
		HASH_ITER(hh, sector_table, sect_iter, next_sect) {
			replace_sect = sect_iter;
			break;
		}
	}
	
	// update world: map
	count = 0;
	for (x = 0; x < MAP_WIDTH; ++x) {
		for (y = 0; y < MAP_HEIGHT; ++y) {
			map = &(world_map[x][y]);
			room = NULL;
			
			if (map->sector_type == sect) {
				perform_change_sect(NULL, map, replace_sect);
				++count;
			}
			if (map->base_sector == sect) {
				perform_change_base_sect(NULL, map, replace_sect);
			}
			if (map->natural_sector == sect) {
				map->natural_sector = replace_sect;
			}
		}
	}
	
	// update world: interior
	LL_FOREACH2(interior_room_list, room, next_interior) {
		if (SECT(room) == sect) {
			// can't use change_terrain() here
			perform_change_sect(room, NULL, replace_sect);
			++count;
		}
		if (BASE_SECT(room) == sect) {
			change_base_sector(room, replace_sect);
		}
	}
	
	// update sector evolutions
	HASH_ITER(hh, sector_table, sect_iter, next_sect) {
		if (delete_sector_from_evolutions(vnum, &GET_SECT_EVOS(sect_iter))) {
			save_library_file_for_vnum(DB_BOOT_SECTOR, GET_SECT_VNUM(sect_iter));
		}
	}
	
	// adventure zones
	HASH_ITER(hh, adventure_table, adv, next_adv) {
		found = delete_link_rule_by_type_value(&GET_ADV_LINKING(adv), ADV_LINK_PORTAL_WORLD, vnum);
		
		if (found) {
			save_library_file_for_vnum(DB_BOOT_ADV, GET_ADV_VNUM(adv));
		}
	}
	
	// update progress
	HASH_ITER(hh, progress_table, prg, next_prg) {
		found = delete_requirement_from_list(&PRG_TASKS(prg), REQ_VISIT_SECTOR, vnum);
		found |= delete_requirement_from_list(&PRG_TASKS(prg), REQ_OWN_SECTOR, vnum);
		
		if (found) {
			SET_BIT(PRG_FLAGS(prg), PRG_IN_DEVELOPMENT);
			save_library_file_for_vnum(DB_BOOT_PRG, PRG_VNUM(prg));
			need_progress_refresh = TRUE;
		}
	}
	
	// update quests
	HASH_ITER(hh, quest_table, quest, next_quest) {
		found = delete_requirement_from_list(&QUEST_TASKS(quest), REQ_VISIT_SECTOR, vnum);
		found |= delete_requirement_from_list(&QUEST_PREREQS(quest), REQ_VISIT_SECTOR, vnum);
		found |= delete_requirement_from_list(&QUEST_TASKS(quest), REQ_OWN_SECTOR, vnum);
		found |= delete_requirement_from_list(&QUEST_PREREQS(quest), REQ_OWN_SECTOR, vnum);
		
		if (found) {
			SET_BIT(QUEST_FLAGS(quest), QST_IN_DEVELOPMENT);
			save_library_file_for_vnum(DB_BOOT_QST, QUEST_VNUM(quest));
		}
	}
	
	// update socials
	HASH_ITER(hh, social_table, soc, next_soc) {
		found = delete_requirement_from_list(&SOC_REQUIREMENTS(soc), REQ_VISIT_SECTOR, vnum);
		found |= delete_requirement_from_list(&SOC_REQUIREMENTS(soc), REQ_OWN_SECTOR, vnum);
		
		if (found) {
			SET_BIT(SOC_FLAGS(soc), SOC_IN_DEVELOPMENT);
			save_library_file_for_vnum(DB_BOOT_SOC, SOC_VNUM(soc));
		}
	}
	
	// olc editors
	for (desc = descriptor_list; desc; desc = desc->next) {
		// update evolutions in olc editors
		if (GET_OLC_SECTOR(desc) && delete_sector_from_evolutions(vnum, &(GET_OLC_SECTOR(desc)->evolution))) {
			msg_to_char(desc->character, "Evolutions for the sector you're editing were removed because a sector was deleted.\r\n");
		}
		
		// adventure links
		if (GET_OLC_ADVENTURE(desc)) {
			found = FALSE;
			found |= delete_link_rule_by_type_value(&(GET_OLC_ADVENTURE(desc)->linking), ADV_LINK_PORTAL_WORLD, vnum);
	
			if (found) {
				msg_to_desc(desc, "One or more linking rules have been removed from the adventure you are editing.\r\n");
			}
		}
		if (GET_OLC_PROGRESS(desc)) {
			found = delete_requirement_from_list(&PRG_TASKS(GET_OLC_PROGRESS(desc)), REQ_VISIT_SECTOR, vnum);
			found |= delete_requirement_from_list(&PRG_TASKS(GET_OLC_PROGRESS(desc)), REQ_OWN_SECTOR, vnum);
		
			if (found) {
				SET_BIT(QUEST_FLAGS(GET_OLC_PROGRESS(desc)), PRG_IN_DEVELOPMENT);
				msg_to_desc(desc, "A sector used by the progression goal you're editing has been deleted.\r\n");
			}
		}
		if (GET_OLC_QUEST(desc)) {
			found = delete_requirement_from_list(&QUEST_TASKS(GET_OLC_QUEST(desc)), REQ_VISIT_SECTOR, vnum);
			found |= delete_requirement_from_list(&QUEST_PREREQS(GET_OLC_QUEST(desc)), REQ_VISIT_SECTOR, vnum);
			found |= delete_requirement_from_list(&QUEST_TASKS(GET_OLC_QUEST(desc)), REQ_OWN_SECTOR, vnum);
			found |= delete_requirement_from_list(&QUEST_PREREQS(GET_OLC_QUEST(desc)), REQ_OWN_SECTOR, vnum);
		
			if (found) {
				SET_BIT(QUEST_FLAGS(GET_OLC_QUEST(desc)), QST_IN_DEVELOPMENT);
				msg_to_desc(desc, "A sector used by the quest you are editing was deleted.\r\n");
			}
		}
		if (GET_OLC_SOCIAL(desc)) {
			found = delete_requirement_from_list(&SOC_REQUIREMENTS(GET_OLC_SOCIAL(desc)), REQ_VISIT_SECTOR, vnum);
			found |= delete_requirement_from_list(&SOC_REQUIREMENTS(GET_OLC_SOCIAL(desc)), REQ_OWN_SECTOR, vnum);
		
			if (found) {
				SET_BIT(SOC_FLAGS(GET_OLC_SOCIAL(desc)), SOC_IN_DEVELOPMENT);
				msg_to_desc(desc, "A sector required by the social you are editing was deleted.\r\n");
			}
		}
	}
	
	syslog(SYS_OLC, GET_INVIS_LEV(ch), TRUE, "OLC: %s has deleted sector %d", GET_NAME(ch), vnum);
	msg_to_char(ch, "Sector %d deleted.\r\n", vnum);
	
	if (count > 0) {
		msg_to_char(ch, "%d live sectors changed.\r\n", count);
	}
	
	// and last...
	free_sector(sect);
}


/**
* Searches for all uses of a crop and displays them.
*
* @param char_data *ch The player.
* @param crop_vnum vnum The crop vnum.
*/
void olc_search_sector(char_data *ch, sector_vnum vnum) {
	extern bool find_requirement_in_list(struct req_data *list, int type, any_vnum vnum);
	
	char buf[MAX_STRING_LENGTH];
	struct adventure_link_rule *link;
	struct evolution_data *evo;
	sector_data *real, *sect, *next_sect;
	quest_data *quest, *next_quest;
	progress_data *prg, *next_prg;
	social_data *soc, *next_soc;
	adv_data *adv, *next_adv;
	int size, found;
	bool any;
	
	real = sector_proto(vnum);
	if (!real) {
		msg_to_char(ch, "There is no sector %d.\r\n", vnum);
		return;
	}
	
	found = 0;
	size = snprintf(buf, sizeof(buf), "Occurrences of sector %d (%s):\r\n", vnum, GET_SECT_NAME(real));
	
	// adventure rules
	HASH_ITER(hh, adventure_table, adv, next_adv) {
		if (size >= sizeof(buf)) {
			break;
		}
		for (link = GET_ADV_LINKING(adv); link; link = link->next) {
			if (link->type == ADV_LINK_PORTAL_WORLD) {
				if (link->value == vnum) {
					++found;
					size += snprintf(buf + size, sizeof(buf) - size, "ADV [%5d] %s\r\n", GET_ADV_VNUM(adv), GET_ADV_NAME(adv));
					// only report once per adventure
					break;
				}
			}
		}
	}
	
	// progress
	HASH_ITER(hh, progress_table, prg, next_prg) {
		if (size >= sizeof(buf)) {
			break;
		}
		// REQ_x: requirement search
		any = find_requirement_in_list(PRG_TASKS(prg), REQ_VISIT_SECTOR, vnum);
		any |= find_requirement_in_list(PRG_TASKS(prg), REQ_OWN_SECTOR, vnum);
		
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
		any = find_requirement_in_list(QUEST_TASKS(quest), REQ_VISIT_SECTOR, vnum);
		any |= find_requirement_in_list(QUEST_PREREQS(quest), REQ_VISIT_SECTOR, vnum);
		any |= find_requirement_in_list(QUEST_TASKS(quest), REQ_OWN_SECTOR, vnum);
		any |= find_requirement_in_list(QUEST_PREREQS(quest), REQ_OWN_SECTOR, vnum);
		
		if (any) {
			++found;
			size += snprintf(buf + size, sizeof(buf) - size, "QST [%5d] %s\r\n", QUEST_VNUM(quest), QUEST_NAME(quest));
		}
	}
	
	// sector evos
	HASH_ITER(hh, sector_table, sect, next_sect) {
		if (size >= sizeof(buf)) {
			break;
		}
		for (evo = GET_SECT_EVOS(sect); evo; evo = evo->next) {
			if (evo->becomes == vnum || (evo_val_types[evo->type] == EVO_VAL_SECTOR && evo->value == vnum)) {
				++found;
				size += snprintf(buf + size, sizeof(buf) - size, "SCT [%5d] %s\r\n", GET_SECT_VNUM(sect), GET_SECT_NAME(sect));
				// only report once per sect
				break;
			}
		}
	}
	
	// socials
	HASH_ITER(hh, social_table, soc, next_soc) {
		if (size >= sizeof(buf)) {
			break;
		}
		any = find_requirement_in_list(SOC_REQUIREMENTS(soc), REQ_VISIT_SECTOR, vnum);
		any |= find_requirement_in_list(SOC_REQUIREMENTS(soc), REQ_OWN_SECTOR, vnum);
		
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
* Function to save a player's changes to a sector (or a new one).
*
* @param descriptor_data *desc The descriptor who is saving.
*/
void save_olc_sector(descriptor_data *desc) {	
	sector_data *proto, *st = GET_OLC_SECTOR(desc);
	sector_vnum vnum = GET_OLC_VNUM(desc);
	struct spawn_info *spawn;
	UT_hash_handle hh;
	
	// have a place to save it?
	if (!(proto = sector_proto(vnum))) {
		proto = create_sector_table_entry(vnum);
	}
	
	// free prototype strings and pointers
	free_icon_set(&GET_SECT_ICONS(proto));
	if (GET_SECT_NAME(proto)) {
		free(GET_SECT_NAME(proto));
	}
	if (GET_SECT_TITLE(proto)) {
		free(GET_SECT_TITLE(proto));
	}
	if (GET_SECT_COMMANDS(proto)) {
		free(GET_SECT_COMMANDS(proto));
	}
	while ((spawn = GET_SECT_SPAWNS(proto))) {
		GET_SECT_SPAWNS(proto) = spawn->next;
		free(spawn);
	}
	free_interactions(&GET_SECT_INTERACTIONS(proto));
	
	// sanity
	if (!GET_SECT_NAME(st) || !*GET_SECT_NAME(st)) {
		if (GET_SECT_NAME(st)) {
			free(GET_SECT_NAME(st));
		}
		GET_SECT_NAME(st) = str_dup(default_sect_name);
	}
	if (!GET_SECT_TITLE(st) || !*GET_SECT_TITLE(st)) {
		if (GET_SECT_TITLE(st)) {
			free(GET_SECT_TITLE(st));
		}
		GET_SECT_TITLE(st) = str_dup(default_sect_title);
	}
	if (GET_SECT_COMMANDS(st) && !*GET_SECT_COMMANDS(st)) {
		if (GET_SECT_COMMANDS(st)) {
			free(GET_SECT_COMMANDS(st));
		}
		GET_SECT_COMMANDS(st) = NULL;
	}
	
	// save data back over the proto-type
	hh = proto->hh;	// save old hash handle
	*proto = *st;	// copy over all data
	proto->vnum = vnum;	// ensure correct vnum
	proto->hh = hh;	// restore old hash handle
	
	// and save to file
	save_library_file_for_vnum(DB_BOOT_SECTOR, vnum);
}


/**
* Creates a copy of a sector, or clears a new one, for editing.
* 
* @param sector_data *input The sector to copy, or NULL to make a new one.
* @return sector_data* The copied sector.
*/
sector_data *setup_olc_sector(sector_data *input) {
	sector_data *new;
	struct evolution_data *old_evo, *new_evo, *last_evo;
	
	CREATE(new, sector_data, 1);
	init_sector(new);
	
	if (input) {
		// copy normal data
		*new = *input;

		// copy things that are pointers
		GET_SECT_NAME(new) = GET_SECT_NAME(input) ? str_dup(GET_SECT_NAME(input)) : NULL;
		GET_SECT_TITLE(new) = GET_SECT_TITLE(input) ? str_dup(GET_SECT_TITLE(input)) : NULL;
		GET_SECT_COMMANDS(new) = GET_SECT_COMMANDS(input) ? str_dup(GET_SECT_COMMANDS(input)) : NULL;
		
		// copy spawns
		GET_SECT_SPAWNS(new) = copy_spawn_list(GET_SECT_SPAWNS(input));
		
		// copy evolutions
		new->evolution = NULL;
		last_evo = NULL;
		for (old_evo = input->evolution; old_evo; old_evo = old_evo->next) {
			CREATE(new_evo, struct evolution_data, 1);
			*new_evo = *old_evo;
			new_evo->next = NULL;
			
			if (last_evo) {
				last_evo->next = new_evo;
			}
			else {
				new->evolution = new_evo;
			}
			last_evo = new_evo;
		}
		
		// copy icons
		GET_SECT_ICONS(new) = copy_icon_set(GET_SECT_ICONS(input));
		
		// copy interactions
		GET_SECT_INTERACTIONS(new) = copy_interaction_list(GET_SECT_INTERACTIONS(input));
	}
	else {
		// brand new: some defaults
		GET_SECT_NAME(new) = str_dup(default_sect_name);
		GET_SECT_TITLE(new) = str_dup(default_sect_title);
		new->roadside_icon = default_roadside_icon;
	}
	
	// done
	return new;	
}


 //////////////////////////////////////////////////////////////////////////////
//// DISPLAYS ////////////////////////////////////////////////////////////////

/**
* This is the main recipe display for sector OLC. It displays the user's
* currently-edited sector.
*
* @param char_data *ch The person who is editing a sector and will see its display.
*/
void olc_show_sector(char_data *ch) {
	void get_evolution_display(struct evolution_data *list, char *save_buffer);
	void get_icons_display(struct icon_data *list, char *save_buffer);
	void get_interaction_display(struct interaction_item *list, char *save_buffer);
	
	sector_data *st = GET_OLC_SECTOR(ch->desc);
	char lbuf[MAX_STRING_LENGTH];
	struct spawn_info *spawn;
	int count;
	
	if (!st) {
		return;
	}
	
	*buf = '\0';

	sprintf(buf + strlen(buf), "[%s%d\t0] %s%s\t0\r\n", OLC_LABEL_CHANGED, GET_OLC_VNUM(ch->desc), OLC_LABEL_UNCHANGED, sector_proto(st->vnum) ? GET_SECT_NAME(sector_proto(st->vnum)) : "new sector");
	sprintf(buf + strlen(buf), "<%sname\t0> %s\r\n", OLC_LABEL_STR(GET_SECT_NAME(st), default_sect_name), NULLSAFE(GET_SECT_NAME(st)));
	sprintf(buf + strlen(buf), "<%stitle\t0> %s\r\n", OLC_LABEL_STR(GET_SECT_TITLE(st), default_sect_title), NULLSAFE(GET_SECT_TITLE(st)));
	sprintf(buf + strlen(buf), "<%scommands\t0> %s\r\n", OLC_LABEL_STR(GET_SECT_COMMANDS(st), ""), NULLSAFE(GET_SECT_COMMANDS(st)));
	sprintf(buf + strlen(buf), "<%sroadsideicon\t0> %c\r\n", OLC_LABEL_VAL(st->roadside_icon, default_roadside_icon), st->roadside_icon);
	sprintf(buf + strlen(buf), "<%smapout\t0> %s\r\n", OLC_LABEL_VAL(GET_SECT_MAPOUT(st), 0), mapout_color_names[GET_SECT_MAPOUT(st)]);

	sprintf(buf + strlen(buf), "<%sicons\t0>\r\n", OLC_LABEL_PTR(GET_SECT_ICONS(st)));
	get_icons_display(GET_SECT_ICONS(st), buf1);
	strcat(buf, buf1);

	sprintf(buf + strlen(buf), "<%sclimate\t0> %s\r\n", OLC_LABEL_VAL(st->climate, 0), climate_types[st->climate]);
	sprintf(buf + strlen(buf), "<%smovecost\t0> %d\r\n", OLC_LABEL_VAL(st->movement_loss, 0), st->movement_loss);

	sprintbit(GET_SECT_FLAGS(st), sector_flags, lbuf, TRUE);
	sprintf(buf + strlen(buf), "<%sflags\t0> %s\r\n", OLC_LABEL_VAL(GET_SECT_FLAGS(st), NOBITS), lbuf);
	
	sprintbit(st->build_flags, bld_on_flags, lbuf, TRUE);
	sprintf(buf + strlen(buf), "<%sbuildflags\t0> %s\r\n", OLC_LABEL_VAL(st->build_flags, NOBITS), lbuf);
	
	sprintf(buf + strlen(buf), "<%sevolution\t0>\r\n", OLC_LABEL_PTR(st->evolution));
	if (st->evolution) {
		get_evolution_display(st->evolution, buf1);
		strcat(buf, buf1);
	}

	sprintf(buf + strlen(buf), "Interactions: <%sinteraction\t0>\r\n", OLC_LABEL_PTR(GET_SECT_INTERACTIONS(st)));
	if (GET_SECT_INTERACTIONS(st)) {
		get_interaction_display(GET_SECT_INTERACTIONS(st), buf1);
		strcat(buf, buf1);
	}
	
	sprintf(buf + strlen(buf), "<%sspawns\t0>\r\n", OLC_LABEL_PTR(GET_SECT_SPAWNS(st)));
	if (GET_SECT_SPAWNS(st)) {
		count = 0;
		for (spawn = GET_SECT_SPAWNS(st); spawn; spawn = spawn->next) {
			++count;
		}
		sprintf(buf + strlen(buf), " %d spawn%s set\r\n", count, PLURAL(count));
	}
		
	page_string(ch->desc, buf, TRUE);
}


 //////////////////////////////////////////////////////////////////////////////
//// EDIT MODULES ////////////////////////////////////////////////////////////

OLC_MODULE(sectedit_buildflags) {
	sector_data *st = GET_OLC_SECTOR(ch->desc);
	st->build_flags = olc_process_flag(ch, argument, "build", "buildflags", bld_on_flags, st->build_flags);
}


OLC_MODULE(sectedit_climate) {
	sector_data *st = GET_OLC_SECTOR(ch->desc);
	st->climate = olc_process_type(ch, argument, "climate", "climate", climate_types, st->climate);
}


OLC_MODULE(sectedit_commands) {
	sector_data *st = GET_OLC_SECTOR(ch->desc);

	if (!str_cmp(argument, "none")) {
		if (GET_SECT_COMMANDS(st)) {
			free(GET_SECT_COMMANDS(st));
		}
		GET_SECT_COMMANDS(st) = NULL;

		if (PRF_FLAGGED(ch, PRF_NOREPEAT)) {
			send_config_msg(ch, "ok_string");
		}
		else {
			msg_to_char(ch, "It no longer has commands.\r\n");
		}
	}
	else {
		olc_process_string(ch, argument, "commands", &GET_SECT_COMMANDS(st));
	}
}


OLC_MODULE(sectedit_evolution) {
	void sort_evolutions(sector_data *sect);
	
	sector_data *st = GET_OLC_SECTOR(ch->desc);
	struct evolution_data *evo, *change, *temp;
	char arg1[MAX_INPUT_LENGTH], arg2[MAX_INPUT_LENGTH], arg3[MAX_INPUT_LENGTH], valstr[MAX_INPUT_LENGTH], *sectarg, *tmp;
	char num_arg[MAX_INPUT_LENGTH], type_arg[MAX_INPUT_LENGTH], val_arg[MAX_INPUT_LENGTH];
	int num, iter, evo_type, value;
	sector_data *to_sect, *vsect;
	double prc;
	bool found;
	
	// arg1 arg2
	half_chop(argument, arg1, arg2);
	
	if (is_abbrev(arg1, "remove")) {
		if (!*arg2) {
			msg_to_char(ch, "Remove which evolution (number)?\r\n");
		}
		else if (!str_cmp(arg2, "all")) {
			while ((evo = st->evolution)) {
				st->evolution = evo->next;
				free(evo);
			}
			msg_to_char(ch, "You remove all evolutions.\r\n");
		}
		else if (!isdigit(*arg2) || (num = atoi(arg2)) < 1) {
			msg_to_char(ch, "Invalid evolution number.\r\n");
		}
		else {
			found = FALSE;
			for (evo = st->evolution; evo && !found; evo = evo->next) {
				if (--num == 0) {
					found = TRUE;
					
					msg_to_char(ch, "You remove evolution #%d.\r\n", atoi(arg2));
					
					REMOVE_FROM_LIST(evo, st->evolution, next);
					free(evo);
				}
			}
			
			if (!found) {
				msg_to_char(ch, "Invalid evolution number.\r\n");
			}
		}
	}
	else if (is_abbrev(arg1, "add")) {
		tmp = any_one_arg(arg2, arg);	// arg = evo_type
		
		if (!*arg) {
			msg_to_char(ch, "Usage: evolution add <evo_type> [value] <percent> <sector vnum>\r\n");
		}
		else if ((evo_type = search_block(arg, evo_types, FALSE)) == NOTHING) {
			msg_to_char(ch, "Invalid type.\r\n");
		}
		else {
			// type validated -- MUST determine arg3 (percent) and sectarg
			switch (evo_val_types[evo_type]) {
				case EVO_VAL_SECTOR: {
					tmp = any_one_arg(tmp, buf);	// buf = sector vnum
					sectarg = any_one_arg(tmp, arg3);
					
					if (!*buf || !isdigit(*buf) || !(vsect = sector_proto(atoi(buf)))) {
						msg_to_char(ch, "Invalid sector type '%s'.\r\n", buf);
						return;
					}
					
					sprintf(valstr, " [%d %s]", GET_SECT_VNUM(vsect), GET_SECT_NAME(vsect));
					value = GET_SECT_VNUM(vsect);
					break;
				}
				case EVO_VAL_NUMBER: {
					tmp = any_one_arg(tmp, buf);	// buf = number
					sectarg = any_one_arg(tmp, arg3);
					
					if (!*buf || (!isdigit(*buf) && *buf != '-')) {
						msg_to_char(ch, "Invalid numerical argument '%s'.\r\n", buf);
						return;
					}
					
					value = atoi(buf);
					sprintf(valstr, " [%d]", value);
					break;
				}
				case EVO_VAL_NONE:
				default: {
					sectarg = any_one_arg(tmp, arg3);
					value = 0;
					*valstr = '\0';
					break;
				}
			}
			
			skip_spaces(&sectarg);
		
			if (!*arg3 || !*sectarg || !isdigit(*sectarg)) {
				msg_to_char(ch, "Usage: evolution add <type> [value] <percent> <sector vnum>\r\n");
			}
			else if ((prc = atof(arg3)) < .01 || prc > 100.00) {
				msg_to_char(ch, "Percentage must be between .01 and 100; '%s' given.\r\n", arg3);
			}
			else if (!(to_sect = sector_proto(atoi(sectarg)))) {
				msg_to_char(ch, "Invalid sector type '%s'.\r\n", sectarg);
			}
			else {
				CREATE(evo, struct evolution_data, 1);
			
				evo->type = evo_type;
				evo->value = value;
				evo->percent = prc;
				evo->becomes = GET_SECT_VNUM(to_sect);
			
				evo->next = st->evolution;
				st->evolution = evo;
				sort_evolutions(st);
				
				msg_to_char(ch, "You add %s %s%s evolution at %.2f%%: %d %s\r\n", AN(evo_types[evo_type]), evo_types[evo_type], valstr, prc, GET_SECT_VNUM(to_sect), GET_SECT_NAME(to_sect));
			}
		}
	}
	else if (is_abbrev(arg1, "change")) {
		half_chop(arg2, num_arg, arg1);
		half_chop(arg1, type_arg, val_arg);
		
		if (!*num_arg || !isdigit(*num_arg) || !*type_arg || !*val_arg) {
			msg_to_char(ch, "Usage: evolution change <number> <type | value | percent | sector> <new value>\r\n");
			return;
		}
		
		// find which one to change
		num = atoi(num_arg);
		change = NULL;
		for (evo = st->evolution; evo && !change; evo = evo->next) {
			if (--num == 0) {
				change = evo;
				break;
			}
		}
		
		if (!change) {
			msg_to_char(ch, "Invalid evolution number.\r\n");
		}
		else if (is_abbrev(type_arg, "type")) {
			if ((evo_type = search_block(val_arg, evo_types, FALSE)) == NOTHING) {
				msg_to_char(ch, "Invalid type.\r\n");
			}
			else {
				change->type = evo_type;
				msg_to_char(ch, "Evolution %d changed to type %s.\r\n", atoi(num_arg), evo_types[evo_type]);
			}
		}
		else if (is_abbrev(type_arg, "value")) {
			// this is based on existing type
			switch (evo_val_types[change->type]) {
				case EVO_VAL_SECTOR: {
					if (!*val_arg || !isdigit(*val_arg) || !(vsect = sector_proto(atoi(val_arg)))) {
						msg_to_char(ch, "Invalid sector type '%s'.\r\n", val_arg);
						return;
					}
					
					sprintf(valstr, "%d %s", GET_SECT_VNUM(vsect), GET_SECT_NAME(vsect));
					value = GET_SECT_VNUM(vsect);
					break;
				}
				case EVO_VAL_NUMBER: {
					if (!*val_arg || (!isdigit(*val_arg) && *val_arg != '-')) {
						msg_to_char(ch, "Invalid numerical argument '%s'.\r\n", val_arg);
						return;
					}
					
					value = atoi(val_arg);
					sprintf(valstr, "%d", value);
					break;
				}
				case EVO_VAL_NONE:
				default: {
					msg_to_char(ch, "That evolution type does not have a value.\r\n");
					return;
				}
			}
			
			change->value = value;
			msg_to_char(ch, "Evolution %d changed to value %s.\r\n", atoi(num_arg), valstr);
		}
		else if (is_abbrev(type_arg, "percent")) {
			prc = atof(val_arg);
			
			if (prc < .01 || prc > 100.00) {
				msg_to_char(ch, "Percentage must be between .01 and 100; '%s' given.\r\n", val_arg);
			}
			else {
				change->percent = prc;
				msg_to_char(ch, "Evolution %d changed to %.2f percent.\r\n", atoi(num_arg), prc);
			}
		}
		else if (is_abbrev(type_arg, "sector")) {
			if (!(to_sect = sector_proto(atoi(val_arg)))) {
				msg_to_char(ch, "Invalid sector type '%s'.\r\n", val_arg);
			}
			else {
				change->becomes = GET_SECT_VNUM(to_sect);
				msg_to_char(ch, "Evolution %d now becomes sector %d %s.\r\n", atoi(num_arg), GET_SECT_VNUM(to_sect), GET_SECT_NAME(to_sect));
			}
		}
		else {
			msg_to_char(ch, "You can only change the type, value, percent, or sector.\r\n");
		}
		
		// if any of them hit (safe to do this anyway)
		if (change) {
			sort_evolutions(st);
		}
	}
	else {
		msg_to_char(ch, "Usage: evolution add <type> [value] <percent> <sector vnum>\r\n");
		msg_to_char(ch, "Usage: evolution change <number> <type | value | percent | sector> <new value>\r\n");
		msg_to_char(ch, "Usage: evolution remove <number | all>\r\n");
		msg_to_char(ch, "Available types:\r\n");
		
		for (iter = 0; *evo_types[iter] != '\n'; ++iter) {
			msg_to_char(ch, " %-24.24s%s", evo_types[iter], ((iter % 2) ? "\r\n" : ""));
		}
		if ((iter % 2) != 0) {
			msg_to_char(ch, "\r\n");
		}
	}
}


OLC_MODULE(sectedit_flags) {
	sector_data *st = GET_OLC_SECTOR(ch->desc);
	GET_SECT_FLAGS(st) = olc_process_flag(ch, argument, "sector", "flags", sector_flags, GET_SECT_FLAGS(st));
}


OLC_MODULE(sectedit_icons) {
	sector_data *st = GET_OLC_SECTOR(ch->desc);
	delete_doubledollar(argument);
	olc_process_icons(ch, argument, &GET_SECT_ICONS(st));
}


OLC_MODULE(sectedit_interaction) {
	sector_data *st = GET_OLC_SECTOR(ch->desc);
	olc_process_interactions(ch, argument, &GET_SECT_INTERACTIONS(st), TYPE_ROOM);
}


OLC_MODULE(sectedit_mapout) {
	sector_data *st = GET_OLC_SECTOR(ch->desc);
	GET_SECT_MAPOUT(st) = olc_process_type(ch, argument, "mapout color", "mapout", mapout_color_names, GET_SECT_MAPOUT(st));
}


OLC_MODULE(sectedit_movecost) {
	sector_data *st = GET_OLC_SECTOR(ch->desc);
	st->movement_loss = olc_process_number(ch, argument, "movement cost", "movecost", 0, 1000, st->movement_loss);
}


OLC_MODULE(sectedit_name) {
	sector_data *st = GET_OLC_SECTOR(ch->desc);
	olc_process_string(ch, argument, "name", &GET_SECT_NAME(st));
	CAP(GET_SECT_NAME(st));
}


OLC_MODULE(sectedit_roadsideicon) {
	sector_data *st = GET_OLC_SECTOR(ch->desc);
	
	if (!*argument) {
		msg_to_char(ch, "Set the roadside icon to what single character?\r\n");
	}
	else if (!isprint(*argument)) {
		msg_to_char(ch, "Invalid roadside icon '%c'.\r\n", *argument);
	}
	else {
		st->roadside_icon = *argument;
		
		if (PRF_FLAGGED(ch, PRF_NOREPEAT)) {
			send_config_msg(ch, "ok_string");
		}
		else {
			msg_to_char(ch, "The roadside icon is now '%c'.\r\n", st->roadside_icon);
		}
	}
}


OLC_MODULE(sectedit_spawns) {
	sector_data *st = GET_OLC_SECTOR(ch->desc);
	olc_process_spawns(ch, argument, &GET_SECT_SPAWNS(st));
}


OLC_MODULE(sectedit_title) {
	sector_data *st = GET_OLC_SECTOR(ch->desc);
	olc_process_string(ch, argument, "title", &GET_SECT_TITLE(st));
	CAP(GET_SECT_TITLE(st));
}
