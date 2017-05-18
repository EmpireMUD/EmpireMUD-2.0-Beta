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
	
	if (!GET_CROP_NAME(cp) || !*GET_CROP_NAME(cp) || !str_cmp(GET_CROP_NAME(cp), "Unnamed Crop")) {
		olc_audit_msg(ch, GET_CROP_VNUM(cp), "No name set");
		problem = TRUE;
	}
	
	strcpy(temp, GET_CROP_NAME(cp));
	strtolower(temp);
	if (strcmp(GET_CROP_NAME(cp), temp)) {
		olc_audit_msg(ch, GET_CROP_VNUM(cp), "Non-lowercase name");
		problem = TRUE;
	}
	
	if (!GET_CROP_TITLE(cp) || !*GET_CROP_TITLE(cp) || !str_cmp(GET_CROP_TITLE(cp), "An Unnamed Crop")) {
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
	void remove_crop_from_table(crop_data *crop);
	extern const sector_vnum climate_default_sector[NUM_CLIMATES];
	
	obj_data *obj, *next_obj;
	descriptor_data *desc;
	struct map_data *map;
	room_data *room;
	crop_data *crop;
	sector_data *base = NULL;
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
	
	// update objects
	HASH_ITER(hh, object_table, obj, next_obj) {
		if (OBJ_FLAGGED(obj, OBJ_PLANTABLE) && GET_OBJ_VAL(obj, VAL_FOOD_CROP_TYPE) == vnum) {
			GET_OBJ_VAL(obj, VAL_FOOD_CROP_TYPE) = NOTHING;
			save_library_file_for_vnum(DB_BOOT_OBJ, GET_OBJ_VNUM(obj));
		}
	}
	
	// olc editors
	for (desc = descriptor_list; desc; desc = desc->next) {
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
* Searches for all uses of a crop and displays them.
*
* @param char_data *ch The player.
* @param crop_vnum vnum The crop vnum.
*/
void olc_search_crop(char_data *ch, crop_vnum vnum) {
	char buf[MAX_STRING_LENGTH];
	crop_data *crop = crop_proto(vnum);
	obj_data *obj, *next_obj;
	int size, found;
	
	if (!crop) {
		msg_to_char(ch, "There is no crop %d.\r\n", vnum);
		return;
	}
	
	found = 0;
	size = snprintf(buf, sizeof(buf), "Occurrences of crop %d (%s):\r\n", vnum, GET_CROP_NAME(crop));
	
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
	struct interaction_item *interact;
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
	while ((interact = GET_CROP_INTERACTIONS(proto))) {
		GET_CROP_INTERACTIONS(proto) = interact->next;
		free(interact);
	}
	
	// sanity
	if (!GET_CROP_NAME(cp) || !*GET_CROP_NAME(cp)) {
		if (GET_CROP_NAME(cp)) {
			free(GET_CROP_NAME(cp));
		}
		GET_CROP_NAME(cp) = str_dup("Unnamed Crop");
	}
	if (!GET_CROP_TITLE(cp) || !*GET_CROP_TITLE(cp)) {
		if (GET_CROP_TITLE(cp)) {
			free(GET_CROP_TITLE(cp));
		}
		GET_CROP_TITLE(cp) = str_dup("An Unnamed Crop");
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
		GET_CROP_NAME(new) = str_dup("Unnamed Crop");
		GET_CROP_TITLE(new) = str_dup("An Unnamed Crop");
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
	
	sprintf(buf + strlen(buf), "[&c%d&0] &c%s&0\r\n", GET_OLC_VNUM(ch->desc), !crop_proto(cp->vnum) ? "new crop" : GET_CROP_NAME(crop_proto(cp->vnum)));
	sprintf(buf + strlen(buf), "<&yname&0> %s\r\n", NULLSAFE(GET_CROP_NAME(cp)));
	sprintf(buf + strlen(buf), "<&ytitle&0> %s\r\n", NULLSAFE(GET_CROP_TITLE(cp)));
	sprintf(buf + strlen(buf), "<&ymapout&0> %s\r\n", mapout_color_names[GET_CROP_MAPOUT(cp)]);

	sprintf(buf + strlen(buf), "<&yicons&0>\r\n");
	get_icons_display(GET_CROP_ICONS(cp), buf1);
	strcat(buf, buf1);
	
	sprintf(buf + strlen(buf), "<&yclimate&0> %s\r\n", climate_types[GET_CROP_CLIMATE(cp)]);

	sprintbit(GET_CROP_FLAGS(cp), crop_flags, lbuf, TRUE);
	sprintf(buf + strlen(buf), "<&yflags&0> %s\r\n", lbuf);
	
	sprintf(buf + strlen(buf), "Map region (percent of map size):\r\n");
	sprintf(buf + strlen(buf), " <&yxmin&0> %3d%%, <&yxmax&0> %3d%%\r\n", GET_CROP_X_MIN(cp), GET_CROP_X_MAX(cp));
	sprintf(buf + strlen(buf), " <&yymin&0> %3d%%, <&yymax&0> %3d%%\r\n", GET_CROP_Y_MIN(cp), GET_CROP_Y_MAX(cp));

	sprintf(buf + strlen(buf), "Interactions: <&yinteraction&0>\r\n");
	if (GET_CROP_INTERACTIONS(cp)) {
		get_interaction_display(GET_CROP_INTERACTIONS(cp), buf1);
		strcat(buf, buf1);
	}

	sprintf(buf + strlen(buf), "<&yspawns&0>\r\n");
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

