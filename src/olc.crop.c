/* ************************************************************************
*   File: olc.crop.c                                      EmpireMUD 2.0b5 *
*  Usage: OLC for crop prototypes                                         *
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
extern const char *climate_types[];
extern const char *crop_flags[];
extern const char *interact_types[];
extern const byte interact_vnum_types[NUM_INTERACTS];
extern const char *mapout_color_names[];
extern const char *spawn_flags[];

// external funcs
void init_crop(crop_data *cp);
void sort_interactions(struct interaction_item **list);

// locals
const char *default_crop_name = "Unnamed Crop";
const char *default_crop_title = "An Unnamed Crop";


 //////////////////////////////////////////////////////////////////////////////
//// HELPERS /////////////////////////////////////////////////////////////////

/**
* Checks for common crop problems and reports them to ch.
*
* @param crop_data *cp The item to audit.
* @param char_data *ch The person to report to.
* @return bool TRUE if any problems were reported; FALSE if all good.
*/
bool audit_crop(crop_data *cp, char_data *ch) {
	extern bool audit_interactions(any_vnum vnum, struct interaction_item *list, int attach_type, char_data *ch);
	extern bool audit_spawns(any_vnum vnum, struct spawn_info *list, char_data *ch);
	extern adv_data *get_adventure_for_vnum(rmt_vnum vnum);
	extern struct icon_data *get_icon_from_set(struct icon_data *set, int type);
	extern const char *icon_types[];
	
	struct interaction_item *inter;
	char temp[MAX_STRING_LENGTH];
	bool problem = FALSE;
	bool harv, forage;
	adv_data *adv;
	int iter;
	
	adv = get_adventure_for_vnum(GET_CROP_VNUM(cp));
	
	if (!GET_CROP_NAME(cp) || !*GET_CROP_NAME(cp) || !str_cmp(GET_CROP_NAME(cp), default_crop_name)) {
		olc_audit_msg(ch, GET_CROP_VNUM(cp), "No name set");
		problem = TRUE;
	}
	
	strcpy(temp, GET_CROP_NAME(cp));
	strtolower(temp);
	if (strcmp(GET_CROP_NAME(cp), temp)) {
		olc_audit_msg(ch, GET_CROP_VNUM(cp), "Non-lowercase name");
		problem = TRUE;
	}
	
	if (!GET_CROP_TITLE(cp) || !*GET_CROP_TITLE(cp) || !str_cmp(GET_CROP_TITLE(cp), default_crop_title)) {
		olc_audit_msg(ch, GET_CROP_VNUM(cp), "No title set");
		problem = TRUE;
	}
	
	if (adv && !CROP_FLAGGED(cp, CROPF_NOT_WILD)) {
		olc_audit_msg(ch, GET_CROP_VNUM(cp), "Missing !WILD flag in adventure crop");
		problem = TRUE;
	}
	if (!adv && CROP_FLAGGED(cp, CROPF_NOT_WILD)) {
		olc_audit_msg(ch, GET_CROP_VNUM(cp), "!WILD flag on non-adventure crop");
		problem = TRUE;
	}
	if (GET_CROP_MAPOUT(cp) == 0) {	// slightly magic-numbered
		olc_audit_msg(ch, GET_CROP_VNUM(cp), "Mapout color not set");
		problem = TRUE;
	}
	for (iter = 0; iter < NUM_TILESETS; ++iter) {
		if (!get_icon_from_set(GET_CROP_ICONS(cp), iter)) {
			olc_audit_msg(ch, GET_CROP_VNUM(cp), "No icon for '%s' tileset", icon_types[iter]);
			problem = TRUE;
		}
	}
	if (GET_CROP_CLIMATE(cp) == CLIMATE_NONE) {
		olc_audit_msg(ch, GET_CROP_VNUM(cp), "Climate not set");
		problem = TRUE;
	}
	if (!GET_CROP_SPAWNS(cp)) {
		olc_audit_msg(ch, GET_CROP_VNUM(cp), "No spawns set");
		problem = TRUE;
	}
	
	harv = forage = FALSE;
	LL_FOREACH(GET_CROP_INTERACTIONS(cp), inter) {
		if (inter->type == INTERACT_HARVEST) {
			harv = TRUE;
		}
		else if (inter->type == INTERACT_FORAGE) {
			forage = TRUE;
		}
	}
	if (!harv) {
		olc_audit_msg(ch, GET_CROP_VNUM(cp), "No HARVEST");
		problem = TRUE;
	}
	if (!forage) {
		olc_audit_msg(ch, GET_CROP_VNUM(cp), "No FORAGE");
		problem = TRUE;
	}
	
	problem |= audit_interactions(GET_CROP_VNUM(cp), GET_CROP_INTERACTIONS(cp), TYPE_ROOM, ch);
	problem |= audit_spawns(GET_CROP_VNUM(cp), GET_CROP_SPAWNS(cp), ch);
	
	return problem;
}


/**
* Creates a new crop entry.
* 
* @param crop_vnum vnum The number to create.
* @return crop_data* The new crop's prototype.
*/
crop_data* create_crop_table_entry(crop_vnum vnum) {
	void add_crop_to_table(crop_data *crop);
	
	crop_data *crop;
	
	// sanity
	if (crop_proto(vnum)) {
		log("SYSERR: Attempting to insert crop at existing vnum %d", vnum);
		return crop_proto(vnum);
	}
	
	CREATE(crop, crop_data, 1);
	init_crop(crop);
	GET_CROP_VNUM(crop) = vnum;
	add_crop_to_table(crop);

	// save index and crop file now
	save_index(DB_BOOT_CROP);
	save_library_file_for_vnum(DB_BOOT_CROP, vnum);

	return crop;
}


/**
* For the .list command.
*
* @param crop_data *crop The thing to list.
* @param bool detail If TRUE, provide additional details
* @return char* The line to show (without a CRLF).
*/
char *list_one_crop(crop_data *crop, bool detail) {
	static char output[MAX_STRING_LENGTH];
	
	if (detail) {
		snprintf(output, sizeof(output), "[%5d] %s", GET_CROP_VNUM(crop), GET_CROP_NAME(crop));
	}
	else {
		snprintf(output, sizeof(output), "[%5d] %s", GET_CROP_VNUM(crop), GET_CROP_NAME(crop));
	}
	
	return output;
}


/**
* WARNING: This function actually deletes a crop.
*
* @param char_data *ch The person doing the deleting.
* @param crop_vnum vnum The vnum to delete.
*/
void olc_delete_crop(char_data *ch, crop_vnum vnum) {
	extern bool delete_link_rule_by_type_value(struct adventure_link_rule **list, int type, any_vnum value);
	void remove_crop_from_table(crop_data *crop);
	extern const sector_vnum climate_default_sector[NUM_CLIMATES];
	
	adv_data *adv, *next_adv;
	obj_data *obj, *next_obj;
	descriptor_data *desc;
	struct map_data *map;
	room_data *room;
	crop_data *crop;
	sector_data *base = NULL;
	bool found;
	int count;
	
	if (!(crop = crop_proto(vnum))) {
		msg_to_char(ch, "There is no such crop %d.\r\n", vnum);
		return;
	}
	
	if (HASH_COUNT(crop_table) <= 1) {
		msg_to_char(ch, "You can't delete the last crop.\r\n");
		return;
	}

	// remove it from the hash table first
	remove_crop_from_table(crop);
	
	// save base sect for later
	base = sector_proto(climate_default_sector[GET_CROP_CLIMATE(crop)]);

	// save index and crop file now
	save_index(DB_BOOT_CROP);
	save_library_file_for_vnum(DB_BOOT_CROP, vnum);
	
	// update world
	count = 0;
	LL_FOREACH(land_map, map) {
		room = real_real_room(map->vnum);
		
		if (map->crop_type == crop || (room && ROOM_CROP(room) == crop)) {
			if (!room) {
				room = real_room(map->vnum);
			}
			set_crop_type(room, NULL);	// remove it explicitly
			change_terrain(room, GET_SECT_VNUM(base));
			++count;
		}
	}
	
	// adventure zones
	HASH_ITER(hh, adventure_table, adv, next_adv) {
		found = delete_link_rule_by_type_value(&GET_ADV_LINKING(adv), ADV_LINK_PORTAL_CROP, vnum);
		
		if (found) {
			save_library_file_for_vnum(DB_BOOT_ADV, GET_ADV_VNUM(adv));
		}
	}
	
	// update objects
	HASH_ITER(hh, object_table, obj, next_obj) {
		if (OBJ_FLAGGED(obj, OBJ_PLANTABLE) && GET_OBJ_VAL(obj, VAL_FOOD_CROP_TYPE) == vnum) {
			GET_OBJ_VAL(obj, VAL_FOOD_CROP_TYPE) = NOTHING;
			save_library_file_for_vnum(DB_BOOT_OBJ, GET_OBJ_VNUM(obj));
		}
	}
	
	// olc editors
	for (desc = descriptor_list; desc; desc = desc->next) {
		if (GET_OLC_ADVENTURE(desc)) {
			found = FALSE;
			found |= delete_link_rule_by_type_value(&(GET_OLC_ADVENTURE(desc)->linking), ADV_LINK_PORTAL_CROP, vnum);
	
			if (found) {
				msg_to_desc(desc, "One or more linking rules have been removed from the adventure you are editing.\r\n");
			}
		}
		if (GET_OLC_OBJECT(desc)) {
			if (OBJ_FLAGGED(GET_OLC_OBJECT(desc), OBJ_PLANTABLE) && GET_OBJ_VAL(GET_OLC_OBJECT(desc), VAL_FOOD_CROP_TYPE) == vnum) {
				GET_OBJ_VAL(GET_OLC_OBJECT(desc), VAL_FOOD_CROP_TYPE) = NOTHING;
				msg_to_char(desc->character, "The crop planted by the object you're editing was deleted.\r\n");
			}
		}
	}
	
	syslog(SYS_OLC, GET_INVIS_LEV(ch), TRUE, "OLC: %s has deleted crop %d", GET_NAME(ch), vnum);
	msg_to_char(ch, "Crop %d deleted.\r\n", vnum);

	if (count > 0) {
		msg_to_char(ch, "%d live crops destroyed.\r\n", count);
	}
	
	free_crop(crop);
}


/**
* Searches properties of crops.
*
* @param char_data *ch The person searching.
* @param char *argument The argument they entered.
*/
void olc_fullsearch_crop(char_data *ch, char *argument) {
	char buf[MAX_STRING_LENGTH], line[MAX_STRING_LENGTH], type_arg[MAX_INPUT_LENGTH], val_arg[MAX_INPUT_LENGTH], find_keywords[MAX_INPUT_LENGTH];
	bitvector_t  find_interacts = NOBITS, found_interacts;
	bitvector_t not_flagged = NOBITS, only_flags = NOBITS;
	int count, only_climate = NOTHING, only_mapout = NOTHING, only_x = NOTHING, only_y = NOTHING;
	struct interaction_item *inter;
	crop_data *crop, *next_crop;
	struct icon_data *icon;
	size_t size;
	bool match;
	
	if (!*argument) {
		msg_to_char(ch, "See HELP CROPEDIT FULLSEARCH for syntax.\r\n");
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
		
		FULLSEARCH_LIST("climate", only_climate, climate_types)
		FULLSEARCH_FLAGS("flags", only_flags, crop_flags)
		FULLSEARCH_FLAGS("flagged", only_flags, crop_flags)
		FULLSEARCH_FLAGS("interaction", find_interacts, interact_types)
		FULLSEARCH_LIST("mapout", only_mapout, mapout_color_names)
		FULLSEARCH_FLAGS("unflagged", not_flagged, crop_flags)
		FULLSEARCH_INT("x", only_x, 0, 100)
		FULLSEARCH_INT("y", only_y, 0, 100)
		
		else {	// not sure what to do with it? treat it like a keyword
			sprintf(find_keywords + strlen(find_keywords), "%s%s", *find_keywords ? " " : "", type_arg);
		}
		
		// prepare for next loop
		skip_spaces(&argument);
	}
	
	size = snprintf(buf, sizeof(buf), "Crop fullsearch: %s\r\n", find_keywords);
	count = 0;
	
	// okay now look up crpos
	HASH_ITER(hh, crop_table, crop, next_crop) {
		if ((only_x != NOTHING || only_y != NOTHING) && CROP_FLAGGED(crop, CROPF_NOT_WILD)) {
			continue;	// can't search x/y on not-wild crops
		}
		if (only_x != NOTHING) {
			if (GET_CROP_X_MAX(crop) > GET_CROP_X_MIN(crop) && (GET_CROP_X_MAX(crop) < only_x || GET_CROP_X_MIN(crop) > only_x)) {
				continue;	// outside of range (max > min)
			}
			if (GET_CROP_X_MAX(crop) < GET_CROP_X_MIN(crop) && (GET_CROP_X_MAX(crop) < only_x && GET_CROP_X_MIN(crop) > only_x)) {
				continue;	// outside of range (max < min)
			}
		}
		if (only_y != NOTHING) {
			if (GET_CROP_Y_MAX(crop) > GET_CROP_Y_MIN(crop) && (GET_CROP_Y_MAX(crop) < only_y || GET_CROP_Y_MIN(crop) > only_y)) {
				continue;	// outside of range (max > min)
			}
			if (GET_CROP_Y_MAX(crop) < GET_CROP_Y_MIN(crop) && (GET_CROP_Y_MAX(crop) < only_y && GET_CROP_Y_MIN(crop) > only_y)) {
				continue;	// outside of range (max < min)
			}
		}
		if (only_flags != NOBITS && (GET_CROP_FLAGS(crop) & only_flags) != only_flags) {
			continue;
		}
		if (not_flagged != NOBITS && CROP_FLAGGED(crop, not_flagged)) {
			continue;
		}
		if (only_climate != NOTHING && GET_CROP_CLIMATE(crop) != only_climate) {
			continue;
		}
		if (only_mapout != NOTHING && GET_CROP_MAPOUT(crop) != only_mapout) {
			continue;
		}
		if (find_interacts) {	// look up its interactions
			found_interacts = NOBITS;
			LL_FOREACH(GET_CROP_INTERACTIONS(crop), inter) {
				found_interacts |= BIT(inter->type);
			}
			if ((find_interacts & found_interacts) != find_interacts) {
				continue;
			}
		}
		
		// string search
		if (*find_keywords && !multi_isname(find_keywords, GET_CROP_NAME(crop)) && !multi_isname(find_keywords, GET_CROP_TITLE(crop))) {
			// check icons too
			match = FALSE;
			LL_FOREACH(GET_CROP_ICONS(crop), icon) {
				if (multi_isname(find_keywords, icon->color) || multi_isname(find_keywords, icon->icon)) {
					match = TRUE;
					break;	// only need 1 match
				}
			}
			if (!match) {
				continue;
			}
		}
		
		// show it
		snprintf(line, sizeof(line), "[%5d] %s\r\n", GET_CROP_VNUM(crop), GET_CROP_NAME(crop));
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
		size += snprintf(buf + size, sizeof(buf) - size, "(%d crop%s)\r\n", count, PLURAL(count));
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
void olc_search_crop(char_data *ch, crop_vnum vnum) {
	char buf[MAX_STRING_LENGTH];
	crop_data *crop = crop_proto(vnum);
	struct adventure_link_rule *link;
	adv_data *adv, *next_adv;
	obj_data *obj, *next_obj;
	int size, found;
	
	if (!crop) {
		msg_to_char(ch, "There is no crop %d.\r\n", vnum);
		return;
	}
	
	found = 0;
	size = snprintf(buf, sizeof(buf), "Occurrences of crop %d (%s):\r\n", vnum, GET_CROP_NAME(crop));
	
	// adventure rules
	HASH_ITER(hh, adventure_table, adv, next_adv) {
		if (size >= sizeof(buf)) {
			break;
		}
		for (link = GET_ADV_LINKING(adv); link; link = link->next) {
			if (link->type == ADV_LINK_PORTAL_CROP) {
				if (link->value == vnum) {
					++found;
					size += snprintf(buf + size, sizeof(buf) - size, "ADV [%5d] %s\r\n", GET_ADV_VNUM(adv), GET_ADV_NAME(adv));
					// only report once per adventure
					break;
				}
			}
		}
	}
	
	// plantables
	HASH_ITER(hh, object_table, obj, next_obj) {
		if (OBJ_FLAGGED(obj, OBJ_PLANTABLE) && GET_OBJ_VAL(obj, VAL_FOOD_CROP_TYPE) == vnum) {
			++found;
			size += snprintf(buf + size, sizeof(buf) - size, "OBJ [%5d] %s\r\n", GET_OBJ_VNUM(obj), GET_OBJ_SHORT_DESC(obj));
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
* Function to save a player's changes to a crop (or a new one).
*
* @param descriptor_data *desc The descriptor who is saving.
*/
void save_olc_crop(descriptor_data *desc) {	
	crop_data *proto, *cp = GET_OLC_CROP(desc);
	crop_vnum vnum = GET_OLC_VNUM(desc);
	struct spawn_info *spawn;
	UT_hash_handle hh;

	// have a place to save it?
	if (!(proto = crop_proto(vnum))) {
		proto = create_crop_table_entry(vnum);
	}
	
	// free prototype strings and pointers
	free_icon_set(&GET_CROP_ICONS(proto));
	if (GET_CROP_NAME(proto)) {
		free(GET_CROP_NAME(proto));
	}
	if (GET_CROP_TITLE(proto)) {
		free(GET_CROP_TITLE(proto));
	}
	while ((spawn = GET_CROP_SPAWNS(proto))) {
		GET_CROP_SPAWNS(proto) = spawn->next;
		free(spawn);
	}
	free_interactions(&GET_CROP_INTERACTIONS(proto));
	
	// sanity
	if (!GET_CROP_NAME(cp) || !*GET_CROP_NAME(cp)) {
		if (GET_CROP_NAME(cp)) {
			free(GET_CROP_NAME(cp));
		}
		GET_CROP_NAME(cp) = str_dup(default_crop_name);
	}
	if (!GET_CROP_TITLE(cp) || !*GET_CROP_TITLE(cp)) {
		if (GET_CROP_TITLE(cp)) {
			free(GET_CROP_TITLE(cp));
		}
		GET_CROP_TITLE(cp) = str_dup(default_crop_title);
	}

	// save data back over the proto-type
	hh = proto->hh;	// save old hash handle
	*proto = *cp;	// copy over all data
	proto->vnum = vnum;	// ensure correct vnum
	proto->hh = hh;	// restore old hash handle
		
	// and save to file
	save_library_file_for_vnum(DB_BOOT_CROP, vnum);
}


/**
* Creates a copy of a crop, or clears a new one, for editing.
* 
* @param crop_data *input The crop to copy, or NULL to make a new one.
* @return crop_data* The copied crop.
*/
crop_data *setup_olc_crop(crop_data *input) {
	crop_data *new;
	
	CREATE(new, crop_data, 1);
	init_crop(new);
	
	if (input) {
		// copy normal data
		*new = *input;

		// copy things that are pointers
		GET_CROP_NAME(new) = GET_CROP_NAME(input) ? str_dup(GET_CROP_NAME(input)) : NULL;
		GET_CROP_TITLE(new) = GET_CROP_TITLE(input) ? str_dup(GET_CROP_TITLE(input)) : NULL;
		
		// copy spawns
		GET_CROP_SPAWNS(new) = copy_spawn_list(GET_CROP_SPAWNS(input));
		
		// copy icons
		GET_CROP_ICONS(new) = copy_icon_set(GET_CROP_ICONS(input));
		
		// copy interactions
		GET_CROP_INTERACTIONS(new) = copy_interaction_list(GET_CROP_INTERACTIONS(input));
	}
	else {
		// brand new: some defaults
		GET_CROP_NAME(new) = str_dup(default_crop_name);
		GET_CROP_TITLE(new) = str_dup(default_crop_title);
		GET_CROP_X_MAX(new) = 100;
		GET_CROP_Y_MAX(new) = 100;
		GET_CROP_FLAGS(new) = CROPF_NOT_WILD;
	}
	
	// done
	return new;	
}


 //////////////////////////////////////////////////////////////////////////////
//// DISPLAYS ////////////////////////////////////////////////////////////////

/**
* This is the main recipe display for crop OLC. It displays the user's
* currently-edited crop.
*
* @param char_data *ch The person who is editing a crop and will see its display.
*/
void olc_show_crop(char_data *ch) {
	void get_icons_display(struct icon_data *list, char *save_buffer);
	void get_interaction_display(struct interaction_item *list, char *save_buffer);
	
	crop_data *cp = GET_OLC_CROP(ch->desc);
	char lbuf[MAX_STRING_LENGTH];
	struct spawn_info *spawn;
	int count;
	
	if (!cp) {
		return;
	}
	
	*buf = '\0';
	
	sprintf(buf + strlen(buf), "[%s%d\t0] %s%s\t0\r\n", OLC_LABEL_CHANGED, GET_OLC_VNUM(ch->desc), OLC_LABEL_UNCHANGED, !crop_proto(cp->vnum) ? "new crop" : GET_CROP_NAME(crop_proto(cp->vnum)));
	sprintf(buf + strlen(buf), "<%sname\t0> %s\r\n", OLC_LABEL_STR(GET_CROP_NAME(cp), default_crop_name), NULLSAFE(GET_CROP_NAME(cp)));
	sprintf(buf + strlen(buf), "<%stitle\t0> %s\r\n", OLC_LABEL_STR(GET_CROP_TITLE(cp), default_crop_title), NULLSAFE(GET_CROP_TITLE(cp)));
	sprintf(buf + strlen(buf), "<%smapout\t0> %s\r\n", OLC_LABEL_VAL(GET_CROP_MAPOUT(cp), 0), mapout_color_names[GET_CROP_MAPOUT(cp)]);

	sprintf(buf + strlen(buf), "<%sicons\t0>\r\n", OLC_LABEL_PTR(GET_CROP_ICONS(cp)));
	get_icons_display(GET_CROP_ICONS(cp), buf1);
	strcat(buf, buf1);
	
	sprintf(buf + strlen(buf), "<%sclimate\t0> %s\r\n", OLC_LABEL_VAL(GET_CROP_CLIMATE(cp), 0), climate_types[GET_CROP_CLIMATE(cp)]);

	sprintbit(GET_CROP_FLAGS(cp), crop_flags, lbuf, TRUE);
	sprintf(buf + strlen(buf), "<%sflags\t0> %s\r\n", OLC_LABEL_VAL(GET_CROP_FLAGS(cp), NOBITS), lbuf);
	
	sprintf(buf + strlen(buf), "Map region (percent of map size):\r\n");
	sprintf(buf + strlen(buf), " <%sxmin\t0> %3d%%, <%sxmax\t0> %3d%%\r\n", OLC_LABEL_VAL(GET_CROP_X_MIN(cp), 0), GET_CROP_X_MIN(cp), OLC_LABEL_VAL(GET_CROP_X_MAX(cp), 100), GET_CROP_X_MAX(cp));
	sprintf(buf + strlen(buf), " <%symin\t0> %3d%%, <%symax\t0> %3d%%\r\n", OLC_LABEL_VAL(GET_CROP_Y_MIN(cp), 0), GET_CROP_Y_MIN(cp), OLC_LABEL_VAL(GET_CROP_Y_MAX(cp), 100), GET_CROP_Y_MAX(cp));

	sprintf(buf + strlen(buf), "Interactions: <%sinteraction\t0>\r\n", OLC_LABEL_PTR(GET_CROP_INTERACTIONS(cp)));
	if (GET_CROP_INTERACTIONS(cp)) {
		get_interaction_display(GET_CROP_INTERACTIONS(cp), buf1);
		strcat(buf, buf1);
	}

	sprintf(buf + strlen(buf), "<%sspawns\t0>\r\n", OLC_LABEL_PTR(GET_CROP_SPAWNS(cp)));
	if (GET_CROP_SPAWNS(cp)) {
		count = 0;
		for (spawn = GET_CROP_SPAWNS(cp); spawn; spawn = spawn->next) {
			++count;
		}
		sprintf(buf + strlen(buf), " %d spawn%s set\r\n", count, PLURAL(count));
	}
		
	page_string(ch->desc, buf, TRUE);
}


 //////////////////////////////////////////////////////////////////////////////
//// EDIT MODULES ////////////////////////////////////////////////////////////

OLC_MODULE(cropedit_climate) {
	crop_data *cp = GET_OLC_CROP(ch->desc);
	GET_CROP_CLIMATE(cp) = olc_process_type(ch, argument, "climate", "climate", climate_types, GET_CROP_CLIMATE(cp));
}


OLC_MODULE(cropedit_flags) {
	crop_data *cp = GET_OLC_CROP(ch->desc);
	bool had_not_wild = IS_SET(GET_CROP_FLAGS(cp), CROPF_NOT_WILD) ? TRUE : FALSE;
	
	GET_CROP_FLAGS(cp) = olc_process_flag(ch, argument, "crop", "flags", crop_flags, GET_CROP_FLAGS(cp));
	
	// validate removal of !WILD
	if (had_not_wild && !IS_SET(GET_CROP_FLAGS(cp), CROPF_NOT_WILD) && GET_ACCESS_LEVEL(ch) < LVL_UNRESTRICTED_BUILDER && !OLC_FLAGGED(ch, OLC_FLAG_CLEAR_IN_DEV)) {
		msg_to_char(ch, "You don't have permission to remove the !WILD flag.\r\n");
		SET_BIT(GET_CROP_FLAGS(cp), CROPF_NOT_WILD);
	}
}


OLC_MODULE(cropedit_icons) {
	crop_data *cp = GET_OLC_CROP(ch->desc);
	delete_doubledollar(argument);
	olc_process_icons(ch, argument, &GET_CROP_ICONS(cp));
}


OLC_MODULE(cropedit_interaction) {
	crop_data *cp = GET_OLC_CROP(ch->desc);
	olc_process_interactions(ch, argument, &GET_CROP_INTERACTIONS(cp), TYPE_ROOM);
}


OLC_MODULE(cropedit_mapout) {
	crop_data *cp = GET_OLC_CROP(ch->desc);
	GET_CROP_MAPOUT(cp) = olc_process_type(ch, argument, "mapout color", "mapout", mapout_color_names, GET_CROP_MAPOUT(cp));
}


OLC_MODULE(cropedit_name) {
	crop_data *cp = GET_OLC_CROP(ch->desc);
	olc_process_string(ch, argument, "name", &GET_CROP_NAME(cp));
	*GET_CROP_NAME(cp) = LOWER(*GET_CROP_NAME(cp));
}


OLC_MODULE(cropedit_spawns) {
	crop_data *cp = GET_OLC_CROP(ch->desc);
	olc_process_spawns(ch, argument, &GET_CROP_SPAWNS(cp));
}


OLC_MODULE(cropedit_title) {
	crop_data *cp = GET_OLC_CROP(ch->desc);
	olc_process_string(ch, argument, "title", &GET_CROP_TITLE(cp));
	CAP(GET_CROP_TITLE(cp));
}


OLC_MODULE(cropedit_xmax) {
	crop_data *cp = GET_OLC_CROP(ch->desc);
	GET_CROP_X_MAX(cp) = olc_process_number(ch, argument, "x-max", "xmax", 0, 100, GET_CROP_X_MAX(cp));
}


OLC_MODULE(cropedit_xmin) {
	crop_data *cp = GET_OLC_CROP(ch->desc);
	GET_CROP_X_MIN(cp) = olc_process_number(ch, argument, "x-min", "xmin", 0, 100, GET_CROP_X_MIN(cp));
}


OLC_MODULE(cropedit_ymax) {
	crop_data *cp = GET_OLC_CROP(ch->desc);
	GET_CROP_Y_MAX(cp) = olc_process_number(ch, argument, "y-max", "ymax", 0, 100, GET_CROP_Y_MAX(cp));
}


OLC_MODULE(cropedit_ymin) {
	crop_data *cp = GET_OLC_CROP(ch->desc);
	GET_CROP_Y_MIN(cp) = olc_process_number(ch, argument, "y-min", "ymin", 0, 100, GET_CROP_Y_MIN(cp));
}

