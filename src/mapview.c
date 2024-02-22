/* ************************************************************************
*   File: mapview.c                                       EmpireMUD 2.0b5 *
*  Usage: Map display functions                                           *
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
#include "comm.h"
#include "interpreter.h"
#include "handler.h"
#include "db.h"
#include "skills.h"
#include "vnums.h"
#include "dg_scripts.h"
#include "constants.h"

/**
* Contents:
*   Data
*   Helpers
*   Line-of-Sight Functions
*   Mappc Functions
*   Map View Functions
*   Screen Reader Functions
*   Where Functions
*   Commands
*/

// external functions
ACMD(do_weather);
vehicle_data *find_vehicle_to_show(char_data *ch, room_data *room, int *total_vehicles);

// locals
ACMD(do_exits);
void show_screenreader_room(char_data *ch, room_data *room, bitvector_t options, int max_dist, bitvector_t only_in_dirs);


 //////////////////////////////////////////////////////////////////////////////
//// DATA ////////////////////////////////////////////////////////////////////

#define WIDE_ROAD_TYPE(tile)  ROOM_BLD_FLAGGED(tile, BLD_ROAD_ICON_WIDE)
#define ANY_ROAD_TYPE(tile)  (IS_ROAD(tile) || ROOM_BLD_FLAGGED(tile, BLD_ROAD_ICON | BLD_ROAD_ICON_WIDE))
#define CONNECTS_TO_ROAD(tile)  (tile && (ANY_ROAD_TYPE(tile) || ROOM_BLD_FLAGGED(tile, BLD_ATTACH_ROAD)))
#define ATTACHES_TO_BARRIER(tile)  ((ROOM_BLD_FLAGGED((tile), BLD_BARRIER | BLD_ATTACH_BARRIER) || ROOM_IS_CLOSED(tile)) && !ROOM_AFF_FLAGGED((tile), ROOM_AFF_CHAMELEON))


// for showing map pcs
struct mappc_data {
	room_data *room;
	char_data *character;
	
	struct mappc_data *next;
};

struct mappc_data_container {
	struct mappc_data *data;
};

// prototype this here
static void show_map_to_char(char_data *ch, struct mappc_data_container *mappc, room_data *to_room, bitvector_t options);


 //////////////////////////////////////////////////////////////////////////////
//// HELPERS /////////////////////////////////////////////////////////////////

/**
* @param room_data *room The room to check.
* @param bool ignore_magic_darkness If TRUE, ignores ROOM_AFF_MAGIC_DARKNESS -- presumably because you already checked it.
* @return bool TRUE if any adjacent room is light; otherwise FALSE.
*/
bool adjacent_room_is_light(room_data *room, bool ignore_magic_darkness) {
	room_data *to_room;
	int dir;
	
	// adventure rooms don't bother
	if (IS_ADVENTURE_ROOM(room)) {
		return FALSE;
	}

	for (dir = 0; dir < NUM_SIMPLE_DIRS; ++dir) {
		if ((to_room = dir_to_room(room, dir, TRUE)) && room_is_light(to_room, FALSE, ignore_magic_darkness)) {
			return TRUE;
		}
	}
	
	return FALSE;
}


/**
* How many tiles a player can see at night, depending on moons, tech, and
* local light.
*
* @param char_data *ch The person.
* @return How many tiles they can see.
*/
int distance_can_see_in_dark(char_data *ch) {
	int dist;
	
	dist = compute_night_light_radius(IN_ROOM(ch)) + GET_EXTRA_ATT(ch, ATT_NIGHT_VISION);
	if (has_player_tech(ch, PTECH_LARGER_LIGHT_RADIUS)) {
		dist += 1;
	}
	if (HAS_BONUS_TRAIT(ch, BONUS_LIGHT_RADIUS)) {
		++dist;
	}
	if (room_is_light(IN_ROOM(ch), TRUE, CAN_SEE_IN_MAGIC_DARKNESS(ch))) {
		++dist;
	}

	return dist;
}


/**
* Shows the single-line version of available exits in the CircleMUD style.
*
* @param char_data *ch The person looking.
* @param room_data *room The room to see exits for.
*/
static void do_brief_auto_exits(struct char_data *ch, room_data *room) {
	struct room_direction_data *ex;
	room_data *to_room;
	vehicle_data *veh;
	bool any = FALSE;
	
	msg_to_char(ch, "\tc[ Exits: ");

	if (COMPLEX_DATA(room) && ROOM_IS_CLOSED(room)) {
		for (ex = COMPLEX_DATA(room)->exits; ex; ex = ex->next) {
			if (ex->room_ptr && !EXIT_FLAGGED(ex, EX_CLOSED)) {
				msg_to_char(ch, "\t(%s\t) ", alt_dirs[get_direction_for_char(ch, ex->dir)]);
				any = TRUE;
			}
		}
	}
	
	// can disembark/exit here?
	if (ROOM_CAN_EXIT(room)) {
		// 'disembark'
		if ((veh = GET_ROOM_VEHICLE(room)) && IN_ROOM(veh) && !VEH_FLAGGED(veh, VEH_BUILDING)) {
			msg_to_char(ch, "\t(disembark\t) ");
			any = TRUE;
		}
		// 'exit'
		else if ((to_room = get_exit_room(room)) && to_room != IN_ROOM(ch)) {
			msg_to_char(ch, "\t(exit\t) ");
			any = TRUE;
		}
	}
	
	msg_to_char(ch, "%s]\t0\r\n", any ? "" : "None! ");
}


/**
* Gets the one-line description for a single exit.
*
* @param char_data *ch The person looking at exits (for see-in-dark/roomflags).
* @param room_data *room The room they're looking at.
* @param const char *prefix The direction or other prefix.
*/
char *exit_description(char_data *ch, room_data *room, const char *prefix) {
	static char output[MAX_STRING_LENGTH];
	char coords[80], rlbuf[80];
	int check_x, check_y;
	size_t size;
	
	size = snprintf(output, sizeof(output), "%-5s - ", prefix);
	
	// done early if they can't see the target room
	if (!can_see_in_dark_room(ch, room, TRUE)) {
		size += snprintf(output + size, sizeof(output) - size, "Too dark to tell");
		return output;
	}
	
	if (GET_ROOM_VNUM(room) >= MAP_SIZE) {
		*coords = '\0';	// only show coords on the map
	}
	else {	// show coords
		check_x = X_COORD(room);
		check_y = Y_COORD(room);
		if (CHECK_MAP_BOUNDS(check_x, check_y)) {
			snprintf(coords, sizeof(coords), " (%d, %d)", check_x, check_y);
		}
		else {
			snprintf(coords, sizeof(coords), " (unknown)");
		}
	}
	
	*rlbuf = '\0';
	if (ROOM_CUSTOM_NAME(room) && !ROOM_AFF_FLAGGED(room, ROOM_AFF_HIDE_REAL_NAME)) {
		snprintf(rlbuf, sizeof(rlbuf), " (%s)", GET_BUILDING(room) ? GET_BLD_NAME(GET_BUILDING(room)) : GET_SECT_NAME(SECT(room)));
	}
	
	if (IS_IMMORTAL(ch) && PRF_FLAGGED(ch, PRF_ROOMFLAGS)) {
		size += snprintf(output + size, sizeof(output) - size, "[%d] %s%s%s", GET_ROOM_VNUM(room), get_room_name(room, FALSE), rlbuf, coords);
	}
	else if (HAS_NAVIGATION(ch) && !NO_LOCATION(room) && (HOME_ROOM(room) == room || !ROOM_IS_CLOSED(room)) && X_COORD(room) >= 0) {
		size += snprintf(output + size, sizeof(output) - size, "%s%s%s", get_room_name(room, FALSE), rlbuf, coords);
	}
	else {
		size += snprintf(output + size, sizeof(output) - size, get_room_name(room, FALSE), rlbuf);
	}
	
	return output;
}


/**
* @param char_data *ch Optional: The player looking at it. (May be NULL)
* @param empire_data *emp The empire to get a color for.
* @return char* The background color that's a good fit for the banner color
*/
const char *get_banner_complement_color(char_data *ch, empire_data *emp) {
	if (!emp) {
		return "";
	}
	
	// no extended colors? Send a gray background and hope for the best -- otherwise there's a risk of no-contrast coloring
	if (!ch || !ch->desc || !ch->desc->pProtocol->pVariables[eMSDP_XTERM_256_COLORS]->ValueInt) {
		return "\t[B111]";
	}

	if (strchrstr(EMPIRE_BANNER(emp), "rRtT")) {
		return "\t[B100]";
	}
	if (strchrstr(EMPIRE_BANNER(emp), "gG")) {
		return "\t[B010]";
	}
	if (strchrstr(EMPIRE_BANNER(emp), "yY")) {
		return "\t[B110]";
	}
	if (strchrstr(EMPIRE_BANNER(emp), "mMpPvV")) {
		return "\t[B101]";
	}
	if (strchrstr(EMPIRE_BANNER(emp), "aA")) {
		return "\t[B001]";
	}
	if (strchrstr(EMPIRE_BANNER(emp), "cCbBjJ")) {
		return "\t[B011]";
	}
	if (strchrstr(EMPIRE_BANNER(emp), "wW")) {
		return "\t[B111]";
	}
	if (strchrstr(EMPIRE_BANNER(emp), "lL")) {
		return "\t[B120]";
	}
	if (strchrstr(EMPIRE_BANNER(emp), "oO")) {
		return "\t[B310]";
	}

	// default
	return "\t[B210]";	// dark tan
}


/**
* Chooses an icon at random from a set of them, by type. This function is
* guaranteed to return an icon.
*
* @param struct icon_data *set The icon set to choose from.
* @param int type Any TILESET_x, or TILESET_ANY.
* @return struct icon_data* A pointer to the icon that was selected.
*/
struct icon_data *get_icon_from_set(struct icon_data *set, int type) {
	static struct icon_data emergency_backup_icon = { TILESET_ANY, "????", "&0", NULL };
	struct icon_data *iter, *found = NULL;
	int count = 0;
	
	for (iter = set; iter; iter = iter->next) {
		if (iter->type == type || iter->type == TILESET_ANY) {
			if (!number(0, count++) || found == NULL) {
				found = iter;
			}
		}
	}
	
	// just in case
	if (!found) {
		found = &emergency_backup_icon;
	}
	
	return found;
}


/**
* Gets a color string based on a series of conditions.
*
* @param char_data *ch Optional: Player observing (may be NULL; determines chameleon view).
* @param bool dismantling
* @param bool unfinished
* @param bool major_disrepair
* @param bool minor_disrepair
* @param int mine_view 0 = not a mine, 1 = mine with ore, -1 = mine with no ore
* @param bool public
* @param bool no_work
* @param bool no_abandon
* @param bool no_dismantle
* @param bool chameleon
* @return char* The color code.
*/
char *get_informative_color(char_data *ch, bool dismantling, bool unfinished, bool major_disrepair, bool minor_disrepair, int mine_view, bool public, bool no_work, bool no_abandon, bool no_dismantle, bool chameleon) {
	if (chameleon) {
		return "\ty";
	}
	else if (dismantling && (!ch || INFORMATIVE_FLAGGED(ch, INFORMATIVE_BUILDING_STATUS))) {
		return "\tC";
	}
	else if (unfinished && (!ch || INFORMATIVE_FLAGGED(ch, INFORMATIVE_BUILDING_STATUS))) {
		return "\tc";
	}
	else if (major_disrepair && (!ch || INFORMATIVE_FLAGGED(ch, INFORMATIVE_DISREPAIR))) {
		return "\tr";
	}
	else if (minor_disrepair && (!ch || INFORMATIVE_FLAGGED(ch, INFORMATIVE_DISREPAIR))) {
		return "\tm";
	}
	else if (mine_view > 0 && (!ch || INFORMATIVE_FLAGGED(ch, INFORMATIVE_MINE_STATUS))) {
		return "\tg";
	}
	else if (mine_view < 0 && (!ch || INFORMATIVE_FLAGGED(ch, INFORMATIVE_MINE_STATUS))) {
		return "\tr";
	}
	else if (public && (!ch || INFORMATIVE_FLAGGED(ch, INFORMATIVE_PUBLIC))) {
		return "\to";
	}
	else if (no_work && (!ch || INFORMATIVE_FLAGGED(ch, INFORMATIVE_NO_WORK))) {
		return "\tB";
	}
	else if (no_abandon && (!ch || INFORMATIVE_FLAGGED(ch, INFORMATIVE_NO_ABANDON))) {
		return "\tV";
	}
	else if (no_dismantle && (!ch || INFORMATIVE_FLAGGED(ch, INFORMATIVE_NO_DISMANTLE))) {
		return "\tJ";
	}
	else {
		return "\t0";
	}
}


/**
* Gets a string of text based on a series of conditions.
*
* @param char_data *ch Optional: Player observing (may be NULL; determines chameleon view).
* @param char *buffer A string for the output (MAX_STRING_LENGTH).
* @param bool dismantling
* @param bool unfinished
* @param bool major_disrepair
* @param bool minor_disrepair
* @param int mine_view 0 = not a mine, 1 = mine with ore, -1 = mine with no ore
* @param bool public
* @param bool no_work
* @param bool no_abandon
* @param bool no_dismantle
* @param bool chameleon
*/
void get_informative_string(char_data *ch, char *buffer, bool dismantling, bool unfinished, bool major_disrepair, bool minor_disrepair, int mine_view, bool public, bool no_work, bool no_abandon, bool no_dismantle, bool chameleon) {
	*buffer = '\0';

	if (dismantling && (!ch || INFORMATIVE_FLAGGED(ch, INFORMATIVE_BUILDING_STATUS))) {
		sprintf(buffer + strlen(buffer), "%sdismantling", *buffer ? ", " : "");
	}
	else if (unfinished && (!ch || INFORMATIVE_FLAGGED(ch, INFORMATIVE_BUILDING_STATUS))) {
		sprintf(buffer + strlen(buffer), "%sunfinished", *buffer ? ", " :"");
	}
	else if (major_disrepair && (!ch || INFORMATIVE_FLAGGED(ch, INFORMATIVE_DISREPAIR))) {
		sprintf(buffer + strlen(buffer), "%sbad disrepair", *buffer ? ", " :"");
	}
	else if (minor_disrepair && (!ch || INFORMATIVE_FLAGGED(ch, INFORMATIVE_DISREPAIR))) {
		sprintf(buffer + strlen(buffer), "%sdisrepair", *buffer ? ", " :"");
	}
	if (mine_view != 0 && (!ch || INFORMATIVE_FLAGGED(ch, INFORMATIVE_MINE_STATUS))) {
		sprintf(buffer + strlen(buffer), "%s%s", *buffer ? ", " :"", mine_view > 0 ? "has ore" : "ore depleted");
	}
	if (public && (!ch || INFORMATIVE_FLAGGED(ch, INFORMATIVE_PUBLIC))) {
		sprintf(buffer + strlen(buffer), "%spublic", *buffer ? ", " :"");
	}
	if (no_work && (!ch || INFORMATIVE_FLAGGED(ch, INFORMATIVE_NO_WORK))) {
		sprintf(buffer + strlen(buffer), "%sno-work", *buffer ? ", " :"");
	}
	if (no_abandon && (!ch || INFORMATIVE_FLAGGED(ch, INFORMATIVE_NO_ABANDON))) {
		sprintf(buffer + strlen(buffer), "%sno-abandon", *buffer ? ", " :"");
	}
	if (no_dismantle && (!ch || INFORMATIVE_FLAGGED(ch, INFORMATIVE_NO_DISMANTLE))) {
		sprintf(buffer + strlen(buffer), "%sno-dismantle", *buffer ? ", " :"");
	}
	if (chameleon) {
		sprintf(buffer + strlen(buffer), "%schameleon", *buffer ? ", " :"");
	}
}


/**
* Detects the player's effective map radius size -- how far away the player
* can see, as displayed on the map. This is controlled by the 'mapsize'
* command, which actually sets the diameter.
*
* @param char_data *ch The person to get mapsize for.
* @return int The map radius.
*/
int get_map_radius(char_data *ch) {
	int mapsize, recent, max, smallmax;

	mapsize = GET_MAPSIZE(REAL_CHAR(ch));
	if (mapsize == 0) {
		// auto-detected
		if (ch->desc && ch->desc->pProtocol->ScreenWidth > 0) {
			int wide = (ch->desc->pProtocol->ScreenWidth - 6) / 8;	// the /8 is 4 chars per tile, doubled
			int max_size = config_get_int("max_map_size");
			if (ch->desc->pProtocol->ScreenHeight > 0) {
				// cap based on height, too (save some room)
				// this saves roughly 4 lines below the map -- if you're going
				// to play around with it, be sure to test -- the math is not
				// very straightforward. -paul
				wide = MIN(wide, ((ch->desc->pProtocol->ScreenHeight - 7) / 2) - 1);	// the -1 at the end is to ensure even/odd numbers have an extra line rather than one too few
				wide = MAX(wide, 1);	// otherwise, NAWS sometimes leads to a map with only the player
			}
			mapsize = MIN(wide, max_size);
		}
		else {
			mapsize = config_get_int("default_map_size");
		}
	}
	
	// automatically limit size if the player is moving too fast
	if (mapsize > 5 && (recent = count_recent_moves(ch)) > 5) {
		max = config_get_int("max_map_size") - (recent - 5);
		mapsize = MIN(mapsize, max);
		smallmax = config_get_int("max_map_while_moving");
		mapsize = MAX(mapsize, smallmax);
	}
	
	return mapsize;
}


/**
* Returns the mine (global) name for a room with mine data.
*
* @param room_data *room The room
* @return char* The mineral name
*/
char *get_mine_type_name(room_data *room) {
	struct global_data *glb = global_proto(get_room_extra_data(room, ROOM_EXTRA_MINE_GLB_VNUM));
	
	if (glb) {
		return GET_GLOBAL_NAME(glb);
	}
	else {
		return "unknown";
	}
}


/**
* @param room_data *room The room to get a paint color for.
* @param char_data *ch The person viewing it, optionally, for no-paint preferences. (May be NULL.)
* @return char* The color code if any, e.g. "&r", or NULL if not painted.
*/
char *get_paint_color_string(room_data *room, char_data *ch) {
	static char color[8];
	
	if (!ROOM_PAINT_COLOR(room)) {
		return NULL;	// not painted
	}
	if (ch && !IS_NPC(ch) && PRF_FLAGGED(ch, PRF_NO_PAINT)) {
		return NULL;	// no paint
	}
	
	// otherwise...
	sprinttype(ROOM_PAINT_COLOR(room), paint_colors, color, sizeof(color), "&0");
	if (ROOM_AFF_FLAGGED(room, ROOM_AFF_BRIGHT_PAINT)) {
		strtoupper(color);
	}
	
	return color;
}


char *get_room_description(room_data *room) {
	static char desc[MAX_STRING_LENGTH];

	*desc = '\0';

	/* Note: room descs ALWAYS override other descs */
	if (ROOM_CUSTOM_DESCRIPTION(room))
		strcat(desc, ROOM_CUSTOM_DESCRIPTION(room));
	else if (ROOM_IS_CLOSED(room) && GET_BUILDING(room) && GET_BLD_DESC(GET_BUILDING(room))) {
		strcat(desc, GET_BLD_DESC(GET_BUILDING(room)));
	}
	else if (GET_ROOM_TEMPLATE(room)) {
		strcat(desc, NULLSAFE(GET_RMT_DESC(GET_ROOM_TEMPLATE(room))));
	}
	else
		strcpy(desc, "");

	return (desc);
}


char *get_room_name(room_data *room, bool color) {
	static char name[MAX_STRING_LENGTH];
	empire_data *emp = ROOM_OWNER(room);
	crop_data *cp;

	if (color && emp)
		strcpy(name, EMPIRE_BANNER(emp));
	else
		*name = '\0';

	/* Note: room names ALWAYS override other names */
	if (ROOM_CUSTOM_NAME(room))
		strcat(name, ROOM_CUSTOM_NAME(room));

	/* Names by type */
	else if (ROOM_SECT_FLAGGED(room, SECTF_CROP) && (cp = ROOM_CROP(room)))
		strcat(name, GET_CROP_TITLE(cp));

	/* Building */
	else if (GET_BUILDING(room)) {
		strcat(name, GET_BLD_TITLE(GET_BUILDING(room)));
	}
	
	// adventure zone
	else if (GET_ROOM_TEMPLATE(room)) {
		strcat(name, GET_RMT_TITLE(GET_ROOM_TEMPLATE(room)));
	}

	/* Normal */
	else
		strcat(name, GET_SECT_TITLE(SECT(room)));

	return (name);
}


/**
* Colors a partial room icon based on a character's political/informative
* preferences. All arguments are pre-validated.
*
* @param char_data *ch The observing player.
* @param room_data *room The room the character is looking at.
* @param char *icon The incoming icon, which may be recolored.
* @param int pos 0 is a building, 1 is the base tile.
* @return char* The possibly-recolored icon.
*/
char *partial_room_icon(char_data *ch, room_data *room, char *icon, int pos) {
	static char storage[2][100];
	char temp[80], col_buf[80];
	
	*storage[pos] = '\0';
	
	if (PRF_FLAGGED(ch, PRF_POLITICAL)) {
		strcpy(temp, strip_color(icon));
		sprintf(storage[pos], "%s%s", ROOM_OWNER(room) ? EMPIRE_BANNER(ROOM_OWNER(room)) : "&0", temp);
		return storage[pos];
	}
	else if (PRF_FLAGGED(ch, PRF_INFORMATIVE)) {
		strcpy(temp, strip_color(icon));
		sprintf(storage[pos], "%s%s", get_informative_color_room(ch, room), temp);
		return storage[pos];
	}
	else if (ROOM_PAINT_COLOR(room)) {
		strcpy(temp, strip_color(icon));
		
		sprinttype(ROOM_PAINT_COLOR(room), paint_colors, col_buf, sizeof(col_buf), "&0");
		if (ROOM_AFF_FLAGGED(room, ROOM_AFF_BRIGHT_PAINT)) {
			strtoupper(col_buf);
		}
		
		sprintf(storage[pos], "%s%s", col_buf, temp);
		return storage[pos];
	}
	else {
		// base icon -- just send it back
		return icon;
	}
}


/**
* Colors a partial icon based on a character's political/inforamtive
* preferences. All arguments are pre-validated.
*
* @param char_data *ch The observing player.
* @param vehicle_data *veh The vehicle to show.
* @param char *icon The incoming icon, which may be recolored.
* @param int pos Which position the partial icon is in (0-3 for quarters, 4 for whole, and 5-6 for halves).
* @return char* The possibly-recolored icon.
*/
char *partial_vehicle_icon(char_data *ch, vehicle_data *veh, char *icon, int pos) {
	// storage for multiple icons so this can be called multiple times in 1 sprintf
	static char storage[7][100];
	char temp[80], col_buf[80];
	
	// init
	*storage[pos] = '\0';
	
	if (PRF_FLAGGED(ch, PRF_POLITICAL)) {
		strcpy(temp, strip_color(icon));
		sprintf(storage[pos], "%s%s", VEH_OWNER(veh) ? EMPIRE_BANNER(VEH_OWNER(veh)) : "&0", temp);
		return storage[pos];
	}
	else if (PRF_FLAGGED(ch, PRF_INFORMATIVE)) {
		strcpy(temp, strip_color(icon));
		sprintf(storage[pos], "%s%s", get_informative_color_veh(ch, veh), temp);
		return storage[pos];
	}
	else if (VEH_PAINT_COLOR(veh)) {
		strcpy(temp, strip_color(icon));
		
		sprinttype(VEH_PAINT_COLOR(veh), paint_colors, col_buf, sizeof(col_buf), "&0");
		if (VEH_FLAGGED(veh, VEH_BRIGHT_PAINT)) {
			strtoupper(col_buf);
		}
		
		sprintf(storage[pos], "%s%s", col_buf, temp);
		return storage[pos];
	}
	else {
		// base icon -- just send it back
		return icon;
	}
}


/**
* Replaces all color codes in 'string' (except &?) with 'new_color'. The new
* string should be the same length so long as new_color is a valid single
* color code.
*
* @param char *string The initial string (will have its colors replaced)/
* @param char *new_color A single color code, like "&b".
*/
void replace_color_codes(char *string, char *new_color) {
	char temp[MAX_STRING_LENGTH];
	int iter = 0;
	
	*temp = '\0';
	
	while (string[iter]) {
		if (string[iter] == COLOUR_CHAR && string[iter+1] != '?') {
			// copy over new color and skip this part of string
			strcpy(temp + iter, new_color);
			++iter;
		}
		else {
			// direct copy
			temp[iter] = string[iter];
		}
		
		// advance it
		++iter;
	}
	temp[iter] = '\0';	// terminate
	strcpy(string, temp);	// copy back over
}


/**
* Replaces special icon codes like @. in the string.
*
* @param char_data *ch The person looking at the tile.
* @param room_data *to_room The room being looked at.
* @param char *icon_buf The buffer where the icon is stored -- text in this buffer will be replaced.
* @param int tileset Which tile set (season) to pull icons from.
*/
void replace_icon_codes(char_data *ch, room_data *to_room, char *icon_buf, int tileset) {
	struct icon_data *icon;
	sector_data *sect;
	char temp[256];
	char *str, *paint_code;
	
	// icon_buf is now the completed icon, but has both color codes (&) and variable tile codes (@)
	if (strchr(icon_buf, '@')) {
		room_data *r_east = SHIFT_CHAR_DIR(ch, to_room, EAST);
		room_data *r_west = SHIFT_CHAR_DIR(ch, to_room, WEST);
		// NOTE: If you add new @ codes here, you must update "const char *icon_codes" in utils.c
		
		// here (@.) roadside icon
		if (strstr(icon_buf, "@.")) {
			icon = get_icon_from_set(GET_SECT_ICONS(BASE_SECT(to_room)), tileset);
			sprintf(temp, "%s%c", icon ? icon->color : "&0", GET_SECT_ROADSIDE_ICON(BASE_SECT(to_room)));
			str = str_replace("@.", temp, icon_buf);
			strcpy(icon_buf, partial_room_icon(ch, to_room, str, 0));
			free(str);
		}
		// east (@e) tile attachment
		if (strstr(icon_buf, "@e")) {
			sect = r_east ? BASE_SECT(r_east) : BASE_SECT(to_room);
			icon = get_icon_from_set(GET_SECT_ICONS(sect), tileset);
			sprintf(temp, "%s%c", icon ? icon->color : "&0", GET_SECT_ROADSIDE_ICON(sect));
			str = str_replace("@e", temp, icon_buf);
			strcpy(icon_buf, str);
			free(str);
		}
		// west (@w) tile attachment
		if (strstr(icon_buf, "@w")) {
			sect = r_west ? BASE_SECT(r_west) : BASE_SECT(to_room);
			icon = get_icon_from_set(GET_SECT_ICONS(sect), tileset);
			sprintf(temp, "%s%c", icon ? icon->color : "&0", GET_SECT_ROADSIDE_ICON(sect));
			str = str_replace("@w", temp, icon_buf);
			strcpy(icon_buf, str);
			free(str);
		}
		
		// west (@u) barrier attachment
		if (strstr(icon_buf, "@u") || strstr(icon_buf, "@U")) {
			if (!r_west || ATTACHES_TO_BARRIER(r_west)) {
				paint_code = r_west ? get_paint_color_string(r_west, ch) : NULL;
				// west is a barrier
				sprintf(temp, "%sv", paint_code ? paint_code : "&0");
				str = str_replace("@u", temp, icon_buf);
				strcpy(icon_buf, str);
				free(str);
				sprintf(temp, "%sV", paint_code ? paint_code : "&0");
				str = str_replace("@U", temp, icon_buf);
				strcpy(icon_buf, str);
				free(str);
			}
			else {
				// west is not a barrier
				sprintf(temp, "&?%c", GET_SECT_ROADSIDE_ICON(BASE_SECT(to_room)));
				str = str_replace("@u", temp, icon_buf);
				strcpy(icon_buf, str);
				free(str);
				str = str_replace("@U", temp, icon_buf);
				strcpy(icon_buf, str);
				free(str);
			}
		}
		
		//  east (@v) barrier attachment
		if (strstr(icon_buf, "@v") || strstr(icon_buf, "@V")) {
			if (!r_east || ATTACHES_TO_BARRIER(r_east)) {
				paint_code = r_east ? get_paint_color_string(r_east, ch) : NULL;
				// east is a barrier
				sprintf(temp, "%sv", paint_code ? paint_code : "&0");
				str = str_replace("@v", temp, icon_buf);
				strcpy(icon_buf, str);
				free(str);
				sprintf(temp, "%sV", paint_code ? paint_code : "&0");
				str = str_replace("@V", temp, icon_buf);
				strcpy(icon_buf, str);
				free(str);
			}
			else {
				// east is not a barrier
				sprintf(temp, "&?%c", GET_SECT_ROADSIDE_ICON(BASE_SECT(to_room)));
				str = str_replace("@v", temp, icon_buf);
				strcpy(icon_buf, str);
				free(str);
				str = str_replace("@V", temp, icon_buf);
				strcpy(icon_buf, str);
				free(str);
			}
		}
	}
}


 //////////////////////////////////////////////////////////////////////////////
//// LINE-OF-SIGHT FUNCTIONS /////////////////////////////////////////////////

/**
* Builds part of the line-of-sight grid, using the line from the player to a
* given room near them. It also determines the grid info for any tile along
* the way. This function should ideally be called on tiles from the outside in,
* to avoid repeating work.
*
* @param char_data *ch The player who is looking at the map.
* @param room_data *from The room the player is looking from.
* @param int x_shift How far east/west the tile to check is.
* @param int y_shift How far north/south the tile to check is.
* @param room_vnum **grid The grid that's being built (by build_line_of_sight_grid).
* @param int radius The player's view radius.
* @param int side The size of one of the "sizes" of the grid var.
* @param int x_offset If the player is near an edge of the map, this offsets the grid.
* @param int y_offset If the player is near an edge of the map, this offsets the grid.
*/
void build_los_grid_one(char_data *ch, room_data *from, int x_shift, int y_shift, room_vnum **grid, int radius, int side, int x_offset, int y_offset) {
	room_vnum *dat = &grid[x_shift + radius][y_shift + radius];
	room_data *end_room, *room;
	room_vnum r_vnum;
	int iter, dist, x_pos, y_pos, top_height, view_height, r_height;
	bool blocked, blocking_veh;
	
	if (*dat != (NOWHERE - 1)) {
		return;	// already done
	}
	if (!(end_room = real_shift(from, x_shift - x_offset, y_shift - y_offset))) {
		// no room there
		*dat = NOWHERE;
		return;
	}
	
	view_height = get_view_height(ch, from);
	
	dist = compute_distance(from, end_room);
	blocked = FALSE;
	top_height = 0;
	for (iter = 1, room = straight_line(from, end_room, iter); iter <= dist && room && room != end_room; ++iter, room = straight_line(from, end_room, iter)) {
		r_vnum = GET_ROOM_VNUM(room);
		x_pos = MAP_X_COORD(r_vnum) - X_COORD(from) + x_offset;
		x_pos = ((x_pos > radius) ? (x_pos - MAP_WIDTH) : ((x_pos < -radius) ? (x_pos + MAP_WIDTH) : x_pos)) + radius;
		y_pos = MAP_Y_COORD(r_vnum) - Y_COORD(from) + y_offset;
		y_pos = ((y_pos > radius) ? (y_pos - MAP_HEIGHT) : ((y_pos < -radius) ? (y_pos + MAP_HEIGHT) : y_pos)) + radius;
		if (x_pos < 0 || x_pos >= side || y_pos < 0 || y_pos >= side) {
			// off the grid somehow
			break;
		}
		
		r_height = get_room_blocking_height(room, &blocking_veh);
		
		if (blocked && r_height <= top_height && (!ROOM_OWNER(room) || ROOM_OWNER(room) != GET_LOYALTY(ch))) {
			// already blocked unless it's talled than the previous top height, or owned by the player
			grid[x_pos][y_pos] = NOWHERE;
		}
		else {
			// record it even if it will block what's behind it
			grid[x_pos][y_pos] = r_vnum;
			
			// record new top height
			top_height = MAX(top_height, r_height);
			
			if (!blocked && (blocking_veh || ROOM_SECT_FLAGGED(room, SECTF_OBSCURE_VISION) || SECT_FLAGGED(BASE_SECT(room), SECTF_OBSCURE_VISION) || ROOM_BLD_FLAGGED(room, BLD_OBSCURE_VISION)) && r_height >= view_height) {
				// rest of line will be blocked
				blocked = TRUE;
			}
		}
	}
	
	// and record the end tile
	if (blocked && (!ROOM_OWNER(end_room) || ROOM_OWNER(end_room) != GET_LOYALTY(ch))) {
		*dat = NOWHERE;
	}
	else {
		*dat = GET_ROOM_VNUM(end_room);
	}
}


/**
* Builds a grid of tiles the player can see from here, based on line-of-sight
* rules. This returns an allocated two-dimensional array of room vnums for any
* rooms that can be seen. The player is at the center of this grid (e.g. if
* the radius is 7, the player is at grid[7][7] and the grid goes from 0 to 14
* in both the x and y dimensions.)
*
* Note: You MUST free the data that comes out of here with 
*       free_line_of_sight_grid() and you must pass it the same radius you
*       passed to this.
*
* @param char_data *ch The player who is viewing the area.
* @param room_data *from The room the player is looking at (the center).
* @param int radius The viewing radius to use (determines the size of the grid).
* @param int x_offset If the player is near an edge of the map, this offsets the grid.
* @param int y_offset If the player is near an edge of the map, this offsets the grid.
* @return room_vnum** The two-dimensional grid of rooms the player can see.
*/
room_vnum **build_line_of_sight_grid(char_data *ch, room_data *from, int radius, int x_offset, int y_offset) {
	room_vnum **grid;
	int x, y, r, side;
	
	if (!ch || radius < 1) {
		return NULL;	// bad input
	}
	if (radius > 100) {
		log("SYSERR: build_line_of_sight_grid requested with radius %d; defaulting to 7", radius);
		radius = 7;
	}
	
	// length of a side
	side = radius * 2 + 1;
	
	// create and initialize grid
	CREATE(grid, room_vnum*, side);
	for (x = 0; x < side; ++x) {
		CREATE(grid[x], room_vnum, side);
		for (y = 0; y < side; ++y) {
			grid[x][y] = NOWHERE - 1;
		}
	}
	
	// build lines to edges first then work the way in
	for (r = radius; r > 0; --r) {
		for (x = r; x >= 0; --x)  {
			for (y = r; y >= 0; --y) {
				if (x != r && y != r) {
					continue;	// only doing edges
				}
				// this builds from the outside in
				if (grid[radius+x][radius+y] == (NOWHERE - 1)) {
					build_los_grid_one(ch, from, x, y, grid, radius, side, x_offset, y_offset);
				}
				if (grid[radius-x][radius-y] == (NOWHERE - 1)) {
					build_los_grid_one(ch, from, -x, -y, grid, radius, side, x_offset, y_offset);
				}
				if (grid[radius+x][radius-y] == (NOWHERE - 1)) {
					build_los_grid_one(ch, from, x, -y, grid, radius, side, x_offset, y_offset);
				}
				if (grid[radius-x][radius+y] == (NOWHERE - 1)) {
					build_los_grid_one(ch, from, -x, y, grid, radius, side, x_offset, y_offset);
				}
			}
		}
	}
	
	// now wipe any locations that were missed (usually due to being off-grid)
	for (x = 0; x < side; ++x) {
		for (y = 0; y < side; ++y) {
			if (grid[x][y] == (NOWHERE - 1)) {
				grid[x][y] = NOWHERE;
			}
		}
	}
	
	return grid;
}


/**
* Frees a 2d array that was allocated by build_line_of_sight_grid().
*
* @param room_vnum **grid The grid to free.
* @param int radius The original radius that was given to create it.
*/
void free_line_of_sight_grid(room_vnum **grid, int radius) {
	int iter, side = radius * 2 + 1;
	
	if (grid) {
		for (iter = 0; iter < side; ++iter) {
			if (grid[iter]) {
				free(grid[iter]);
			}
		}
		free(grid);
	}
}


 //////////////////////////////////////////////////////////////////////////////
//// MAPPC FUNCTIONS /////////////////////////////////////////////////////////

/**
* determines if ch can see vict across the map
*
* @param char_data *ch The viewer
* @param char_data *vict The person being seen (maybe)
* @return bool TRUE if the ch sees vict over there
*/
bool can_see_player_in_other_room(char_data *ch, char_data *vict) {
	int distance_can_see_players = 7;
	
	if (!IS_NPC(vict) && CAN_SEE(ch, vict) && WIZHIDE_OK(ch, vict) && CAN_RECOGNIZE(ch, vict)) {
		if (compute_distance(IN_ROOM(ch), IN_ROOM(vict)) <= distance_can_see_players) {
			if (has_player_tech(vict, PTECH_MAP_INVIS)) {
				gain_player_tech_exp(vict, PTECH_MAP_INVIS, 10);
				return FALSE;
			}
			else {
				return TRUE;
			}
		}
	}
	
	// nope
	return FALSE;
}


/**
* Determines if there's a person or mob in the room to show as the map icon.
*
* @param char_data *ch The person viewing it.
* @param room_data *room The room they are looking at.
* @param struct mappc_data_container *mappc A container to store the discovered data here.
* @param char *icon_buf Where it will write the icon (a string of some kind).
*/
bool show_pc_in_room(char_data *ch, room_data *room, struct mappc_data_container *mappc, char *icon_buf) {
	struct mappc_data *pc, *start_this_room = NULL;
	bool show_mob = FALSE;
	char_data *c;
	empire_data *emp;
	int count = 0, iter;
	char *charmap = "<oo>";
	
	// init this, in case
	*icon_buf = '\0';

	if (!SHOW_PEOPLE_IN_ROOM(room)) {
		return FALSE;
	}

	/* Hidden people are left off the map, even if you sense life */
	DL_FOREACH2(ROOM_PEOPLE(room), c, next_in_room) {
		if (can_see_player_in_other_room(ch, c)) {
			CREATE(pc, struct mappc_data, 1);
			pc->room = room;
			pc->character = c;
			LL_APPEND(mappc->data, pc);
	
			if (!start_this_room) {
				start_this_room = pc;
			}
	
			++count;
		}
		else if (IS_NPC(c) && size_data[GET_SIZE(c)].show_on_map) {
			show_mob = TRUE;
		}
	}
	
	// each case colors slightly differently
	if (count == 0 && !show_mob) {
		return FALSE;
	}
	else if (count == 1) {
		emp = GET_LOYALTY(start_this_room->character);
		sprintf(icon_buf, "&0<%soo&0>", !emp ? "" : EMPIRE_BANNER(emp));
	}
	else if (count == 2) {
		pc = start_this_room;
		emp = GET_LOYALTY(pc->character);
		sprintf(icon_buf, "&0<%so", !emp ? "&0" : EMPIRE_BANNER(emp));
		
		pc = pc->next;
		emp = GET_LOYALTY(pc->character);
		sprintf(icon_buf + strlen(icon_buf), "%so&0>", !emp ? "" : EMPIRE_BANNER(emp));
	}
	else if (count == 3) {
		pc = start_this_room;
		emp = GET_LOYALTY(pc->character);
		sprintf(icon_buf, "%s<", !emp ? "&0" : EMPIRE_BANNER(emp));
		
		pc = pc->next;
		emp = GET_LOYALTY(pc->character);
		sprintf(icon_buf + strlen(icon_buf), "%soo", !emp ? "" : EMPIRE_BANNER(emp));
		
		pc = pc->next;
		emp = GET_LOYALTY(pc->character);
		sprintf(icon_buf + strlen(icon_buf), "%s>", !emp ? "" : EMPIRE_BANNER(emp));
	}
	else if (count >= 4) {
		pc = start_this_room;
		*icon_buf = '\0';
		
		// only show the first 4 colors in this case
		for (iter = 0; iter < 4; ++iter) {
			emp = GET_LOYALTY(pc->character);
			sprintf(icon_buf + strlen(icon_buf), "%s%c", !emp ? "&0" : EMPIRE_BANNER(emp), charmap[iter]);
			pc = pc->next;
		}
	}
	else if (show_mob) {
		strcpy(icon_buf, "&0(oo)");
	}
	
	// if we got here we found any pc/visible mob
	return TRUE;
}


 //////////////////////////////////////////////////////////////////////////////
//// MAP VIEW FUNCTIONS //////////////////////////////////////////////////////

/**
* Builds the final icon for a single map tile, based on its current conditions.
* This excludes any person/vehicle on it.
*
* @param char_data *ch The person looking at it.
* @param room_data *to_room The room they are looking at.
* @param struct icon_data *base_icon The icon for the base tile (used for color etc).
* @param struct icon_data *crop_icon The icon for a crop on the tile (may be NULL).
* @param int tileset Which tile set (season) to pull icons from.
* @param char *icon_buf A string to write the icon to.
* @param char *icon_color A string to write the icon color to -- if we can detect it.
*/
void build_map_icon(char_data *ch, room_data *to_room, struct icon_data *base_icon, struct icon_data *crop_icon, int tileset, char *icon_buf, char *icon_color) {
	struct empire_city_data *city;
	struct icon_data *icon;
	
	// initialize this
	*icon_buf = *icon_color = '\0';

	if (CHECK_CHAMELEON(IN_ROOM(ch), to_room)) {
		// Hidden buildings
		strcat(icon_buf, base_icon->icon);
		strcpy(icon_color, base_icon->color);
	}
	else if (ROOM_CUSTOM_ICON(to_room)) {
		// Rooms with custom icons (take precedence over all but hidden rooms
		strcat(icon_buf, ROOM_CUSTOM_ICON(to_room));
	}
	else if (WIDE_ROAD_TYPE(to_room)) {
		// adjacent rooms, shifted by map change
		// WARNING: You must make sure these are not NULL when you try to use them
		room_data *r_north = SHIFT_CHAR_DIR(ch, to_room, NORTH);
		room_data *r_east = SHIFT_CHAR_DIR(ch, to_room, EAST);
		room_data *r_south = SHIFT_CHAR_DIR(ch, to_room, SOUTH);
		room_data *r_west = SHIFT_CHAR_DIR(ch, to_room, WEST);
		room_data *r_northwest = SHIFT_CHAR_DIR(ch, to_room, NORTHWEST);
		room_data *r_northeast = SHIFT_CHAR_DIR(ch, to_room, NORTHEAST);
		room_data *r_southwest = SHIFT_CHAR_DIR(ch, to_room, SOUTHWEST);
		room_data *r_southeast = SHIFT_CHAR_DIR(ch, to_room, SOUTHEAST);
		
		// check west for the first 2 parts of the tile
		if (CONNECTS_TO_ROAD(r_west) || (CONNECTS_TO_ROAD(r_southwest) && !CONNECTS_TO_ROAD(r_south)) || (CONNECTS_TO_ROAD(r_northwest) && !CONNECTS_TO_ROAD(r_north))) {
			// road west
			strcat(icon_buf, "&0=");
		}
		else {
			sprintf(icon_buf + strlen(icon_buf), "&?%c", GET_SECT_ROADSIDE_ICON(BASE_SECT(to_room)));
		}
		
		// middle part
		if (CONNECTS_TO_ROAD(r_north) || CONNECTS_TO_ROAD(r_south)) {
			// N/S are road
			
			if (strstr(icon_buf, "=") || CONNECTS_TO_ROAD(r_east) || (CONNECTS_TO_ROAD(r_northeast) && !CONNECTS_TO_ROAD(r_north)) || (CONNECTS_TO_ROAD(r_southeast) && !CONNECTS_TO_ROAD(r_south))) {
				strcat(icon_buf, "&0++");
			}
			else {
				strcat(icon_buf, "&0||");
			}
		}
		else {
			strcat(icon_buf, "&0==");
		}
		
		// check east for last part of tile
		if (CONNECTS_TO_ROAD(r_east) || (CONNECTS_TO_ROAD(r_northeast) && !CONNECTS_TO_ROAD(r_north)) || (CONNECTS_TO_ROAD(r_southeast) && !CONNECTS_TO_ROAD(r_south))) {
			strcat(icon_buf, "=");
		}
		else {
			sprintf(icon_buf + strlen(icon_buf), "&?%c", GET_SECT_ROADSIDE_ICON(BASE_SECT(to_room)));
		}
	}
	else if (ANY_ROAD_TYPE(to_room)) {
		// adjacent rooms, shifted by map change
		// WARNING: You must make sure these are not NULL when you try to use them
		room_data *r_north = SHIFT_CHAR_DIR(ch, to_room, NORTH);
		room_data *r_east = SHIFT_CHAR_DIR(ch, to_room, EAST);
		room_data *r_south = SHIFT_CHAR_DIR(ch, to_room, SOUTH);
		room_data *r_west = SHIFT_CHAR_DIR(ch, to_room, WEST);
		room_data *r_northwest = SHIFT_CHAR_DIR(ch, to_room, NORTHWEST);
		room_data *r_northeast = SHIFT_CHAR_DIR(ch, to_room, NORTHEAST);
		room_data *r_southwest = SHIFT_CHAR_DIR(ch, to_room, SOUTHWEST);
		room_data *r_southeast = SHIFT_CHAR_DIR(ch, to_room, SOUTHEAST);
		
		// check west for the first 2 parts of the tile
		if (CONNECTS_TO_ROAD(r_west)) {
			// road west
			strcat(icon_buf, "&0--");
		}
		else if ((CONNECTS_TO_ROAD(r_southwest) && !CONNECTS_TO_ROAD(r_south)) && (CONNECTS_TO_ROAD(r_northwest) && !CONNECTS_TO_ROAD(r_north))) {
			// both nw and sw
			strcat(icon_buf, "&0>-");
		}
		else if (CONNECTS_TO_ROAD(r_southwest) && !CONNECTS_TO_ROAD(r_south)) {
			// just sw
			strcat(icon_buf, "&0,-");
		}
		else if (CONNECTS_TO_ROAD(r_northwest) && !CONNECTS_TO_ROAD(r_north)) {
			// just nw
			strcat(icon_buf, "&0`-");
		}
		else if (CONNECTS_TO_ROAD(r_north) || CONNECTS_TO_ROAD(r_south)) {
			// road north/south but NOT west
			sprintf(icon_buf + strlen(icon_buf), "&?%c%c", GET_SECT_ROADSIDE_ICON(BASE_SECT(to_room)), GET_SECT_ROADSIDE_ICON(BASE_SECT(to_room)));
		}
		else {
			// N/W/S were not road
			sprintf(icon_buf + strlen(icon_buf), "&?%c&0-", GET_SECT_ROADSIDE_ICON(BASE_SECT(to_room)));
		}
		
		// middle part
		if (CONNECTS_TO_ROAD(r_north) || CONNECTS_TO_ROAD(r_south)) {
			// N/S are road
			if (strstr(icon_buf, "-") || CONNECTS_TO_ROAD(r_east) || (CONNECTS_TO_ROAD(r_northeast) && !CONNECTS_TO_ROAD(r_north)) || (CONNECTS_TO_ROAD(r_southeast) && !CONNECTS_TO_ROAD(r_south))) {
				strcat(icon_buf, "&0+");
			}
			else {
				strcat(icon_buf, "&0|");
			}
		}
		else {
			strcat(icon_buf, "&0-");
		}
		
		// check east for last part of tile
		if (CONNECTS_TO_ROAD(r_east)) {
			strcat(icon_buf, "-");
		}
		else if ((CONNECTS_TO_ROAD(r_northeast) && !CONNECTS_TO_ROAD(r_north)) && (CONNECTS_TO_ROAD(r_southeast) && !CONNECTS_TO_ROAD(r_south))) {
			// ne and se
			strcat(icon_buf, "<");
		}
		else if (CONNECTS_TO_ROAD(r_northeast) && !CONNECTS_TO_ROAD(r_north)) {
			// just ne
			strcat(icon_buf, "'");
		}
		else if (CONNECTS_TO_ROAD(r_southeast) && !CONNECTS_TO_ROAD(r_south)) {
			// just se
			strcat(icon_buf, ",");
		}
		else {
			sprintf(icon_buf + strlen(icon_buf), "&?%c", GET_SECT_ROADSIDE_ICON(BASE_SECT(to_room)));
		}
	}
	else if (ROOM_SECT_FLAGGED(to_room, SECTF_CROP) && ROOM_CROP(to_room) && crop_icon) {
		strcat(icon_buf, crop_icon->icon);
		strcpy(icon_color, crop_icon->color);
	}
	else if (IS_CITY_CENTER(to_room)) {
		if ((city = find_city(ROOM_OWNER(to_room), to_room))) {
			strcat(icon_buf, city_type[city->type].icon);
		}
		else {
			strcat(icon_buf, "[  ]");
		}
	}
	else if (IS_MAP_BUILDING(to_room) && GET_BUILDING(to_room)) {
		strcat(icon_buf, GET_BLD_ICON(GET_BUILDING(to_room)));
	}
	else if ((icon = get_icon_from_set(GET_SECT_ICONS(SECT(to_room)), tileset))) {
		strcat(icon_buf, icon->icon);
		strcpy(icon_color, icon->color);
	}
	else {	// error: no icon available
		strcat(icon_buf, "????");
	}
}


/**
* Creates a vehicle icon for a tile based on the vehicles and buildings on it.
*
* @param char_data *ch The viewer.
* @param room_data *room The room being looked at -- the one we're building icons for.
* @param vehicle_data *main_veh The primary vehicle chosen to show on the tile.
* @param bool memory_only If TRUE, ignores any non-building vehicles or chameleon ones (including main_veh).
* @param char *outbuf A string to build the output in. It MUST be big enough to hold a 4-character icon plus all the color codes and special icon codes.
*/
void build_vehicle_icon(char_data *ch, room_data *room, vehicle_data *main_veh, bool memory_only, char *outbuf) {
	char icon_color[256], temp[60];
	char *whole, *half[2], *quarter[4];
	int iter, half_size, quarter_size, tileset, x;
	vehicle_data *veh;
	
	// for partial vehicle icons:
	const int WHOLE_ICON = 4;
	const int HALF_1 = 5;
	const int HALF_2 = 6;
	// (quarters just use their own pos)
	
	// initialize
	*outbuf = *icon_color = '\0';
	whole = NULL;
	half_size = quarter_size = 0;
	for (iter = 0; iter < 2; ++iter) {
		half[iter] = NULL;
	}
	for (iter = 0; iter < 4; ++iter) {
		quarter[iter] = NULL;
	}
	
	// include main vehicle first
	if (main_veh && (!memory_only || !VEH_IS_COMPLETE(main_veh) || !VEH_FLAGGED(main_veh, VEH_CHAMELEON)) && (!memory_only || VEH_FLAGGED(main_veh, VEH_BUILDING))) {
		if (VEH_ICON(main_veh)) {
			whole = partial_vehicle_icon(ch, main_veh, VEH_ICON(main_veh), WHOLE_ICON);
		}
		if (VEH_HALF_ICON(main_veh)) {
			half[half_size] = partial_vehicle_icon(ch, main_veh, VEH_HALF_ICON(main_veh), half_size == 0 ? HALF_1 : HALF_2);
			++half_size;
		}
		if (VEH_QUARTER_ICON(main_veh)) {
			quarter[quarter_size] = partial_vehicle_icon(ch, main_veh, VEH_QUARTER_ICON(main_veh), quarter_size);
			++quarter_size;
		}
	}
	
	// next find more vehicles
	DL_FOREACH2(ROOM_VEHICLES(room), veh, next_in_room) {
		if (half_size >= 2 && quarter_size >= 4) {
			break;	// no room remains
		}
		if (veh == main_veh) {
			continue;	// already shown
		}
		if (memory_only && VEH_IS_COMPLETE(veh) && VEH_FLAGGED(veh, VEH_CHAMELEON)) {
			continue;	// hide chameleon
		}
		if (memory_only && !VEH_FLAGGED(veh, VEH_BUILDING)) {
			continue;	// hide non-building
		}
		if (vehicle_is_chameleon(veh, IN_ROOM(ch))) {
			continue;	// hidden
		}
		
		// ok to show it: grab pointers to the icons
		if (VEH_ICON(veh) && !whole) {
			whole = partial_vehicle_icon(ch, veh, VEH_ICON(veh), WHOLE_ICON);
		}
		if (VEH_HALF_ICON(veh) && half_size < 2) {
			half[half_size] = partial_vehicle_icon(ch, veh, VEH_HALF_ICON(veh), half_size == 0 ? HALF_1 : HALF_2);
			++half_size;
		}
		if (VEH_QUARTER_ICON(veh) && quarter_size < 4) {
			quarter[quarter_size] = partial_vehicle_icon(ch, veh, VEH_QUARTER_ICON(veh), quarter_size);
			++quarter_size;
		}
	}
	
	// check building icon itself if space is left
	// TODO should the building be FIRST in some cases?
	if (GET_BUILDING(room) && (!memory_only || !ROOM_AFF_FLAGGED(room, ROOM_AFF_CHAMELEON)) && !CHECK_CHAMELEON(IN_ROOM(ch), room)) {
		if (!whole) {
			whole = partial_room_icon(ch, room, GET_BLD_ICON(GET_BUILDING(room)), 0);
		}
		if (GET_BLD_HALF_ICON(GET_BUILDING(room)) && half_size < 2) {
			half[half_size++] = partial_room_icon(ch, room, GET_BLD_HALF_ICON(GET_BUILDING(room)), 0);
		}
		if (GET_BLD_QUARTER_ICON(GET_BUILDING(room)) && quarter_size < 4) {
			quarter[quarter_size++] = partial_room_icon(ch, room, GET_BLD_QUARTER_ICON(GET_BUILDING(room)), 0);
		}
	}
	
	// ok, build string: how many did we find to show
	if (quarter_size > 2 || (quarter_size >= 2 && half_size == 0)) {
		// show quarter
		if (quarter_size == 4) {
			sprintf(outbuf, "%s%s%s%s", quarter[0], quarter[1], quarter[2], quarter[3]);
		}
		else if (quarter_size == 3) {
			// procedurally choose a spot for the gap
			x = X_COORD(room);
			x = MAX(0, x);
			for (iter = 0; iter < 4; ++iter) {
				if (iter < (x % 4)) {
					strcat(outbuf, quarter[iter]);
				}
				else if (iter == (x % 4)) {
					strcat(outbuf, partial_room_icon(ch, room, "@.", 1));
				}
				else {
					strcat(outbuf, quarter[iter-1]);
				}
			}
		}
		else if (quarter_size == 2) {
			strcpy(temp, partial_room_icon(ch, room, "@.", 1));
			sprintf(outbuf, "%s%s%s%s", quarter[0], temp, quarter[1], temp);
		}
	}
	else if (half_size == 2) {
		// show 2 half icons
		sprintf(outbuf, "%s%s", half[0], half[1]);
	}
	else if (half_size == 1 && !whole) {
		// show 1 half icon for some reason
		strcpy(temp, partial_room_icon(ch, room, "@.", 1));
		sprintf(outbuf, "%s%s%s", temp, half[0], temp);
	}
	else if (whole) {
		// show whole icon
		strcpy(outbuf, whole);
	}
	else {
		// this should not happen but we will return a 4-char icon anyway
		tileset = GET_SEASON(room);
		build_map_icon(ch, room, get_icon_from_set(GET_SECT_ICONS(BASE_SECT(room)), tileset), (ROOM_SECT_FLAGGED(room, SECTF_CROP) && ROOM_CROP(room)) ? get_icon_from_set(GET_CROP_ICONS(ROOM_CROP(room)), tileset) : NULL, tileset, outbuf, icon_color);
	}
}


/**
* Does a normal "look" at a room.
*
* @param char_data *ch The person doing the looking.
* @param room_data *room The room to look at.
* @param bitvector_t options LRR_x flags.
*/
void look_at_room_by_loc(char_data *ch, room_data *room, bitvector_t options) {
	struct mappc_data_container *mappc = NULL;
	struct mappc_data *pc, *next_pc;
	struct empire_city_data *city;
	char output[MAX_STRING_LENGTH], veh_buf[256], col_buf[256], flagbuf[MAX_STRING_LENGTH], locbuf[128], partialbuf[MAX_STRING_LENGTH], rlbuf[MAX_STRING_LENGTH], tmpbuf[MAX_STRING_LENGTH], advcolbuf[128];
	char room_name_color[MAX_STRING_LENGTH];
	int s, t, mapsize, iter, check_x, check_y, level, ter_type;
	int first_iter, second_iter, xx, yy, magnitude, north;
	int first_start, first_end, second_start, second_end, temp;
	int dist, can_see_in_dark_distance;
	int x_offset = 0, y_offset = 0;
	bool y_first, invert_x, invert_y, comma, junk, show_blocked, large_radius;
	struct instance_data *inst;
	player_index_data *index;
	room_vnum **view_grid = NULL;
	room_data *to_room, *outer;
	empire_data *emp, *pcemp;
	const char *memory;
	crop_data *cp;
	char *strptr;
	
	const char *blocked_tile = "\tw****";
	
	// configs
	int trench_initial_value = config_get_int("trench_initial_value");
	
	// options
	bool ship_partial = IS_SET(options, LRR_SHIP_PARTIAL) ? TRUE : FALSE;
	bool look_out = IS_SET(options, LRR_LOOK_OUT) ? TRUE : FALSE;
	bool has_ship = (GET_ROOM_VEHICLE(IN_ROOM(ch)) && !VEH_FLAGGED(GET_ROOM_VEHICLE(IN_ROOM(ch)), VEH_BUILDING)) ? TRUE : FALSE;
	bool show_on_ship = has_ship && CAN_LOOK_OUT(IN_ROOM(ch));
	bool show_title = !show_on_ship || ship_partial || look_out || IS_SET(options, LRR_LOOK_OUT_INSIDE);

	// begin with the sanity check
	if (!ch || !ch->desc)
		return;
	
	if (!IS_NPC(ch)) {
		// store the sun's status to be able to force a 'look' correctly at sunrise/set
		GET_LAST_LOOK_SUN(ch) = get_sun_status(IN_ROOM(ch));
	}

	mapsize = get_map_radius(ch);

	if (AFF_FLAGGED(ch, AFF_BLIND)) {
		msg_to_char(ch, "You see nothing but infinite darkness...\r\n");
		return;
	}

	if (!look_out && AFF_FLAGGED(ch, AFF_EARTHMELDED) && IS_ANY_BUILDING(IN_ROOM(ch)) && !ROOM_BLD_FLAGGED(IN_ROOM(ch), BLD_OPEN)) {
		msg_to_char(ch, "You are beneath a building.\r\n");
		return;
	}

	// check for ship
	if (!look_out && !ship_partial && show_on_ship && !IS_SET(options, LRR_LOOK_OUT_INSIDE)) {
		look_at_room_by_loc(ch, IN_ROOM(GET_ROOM_VEHICLE(IN_ROOM(ch))), LRR_SHIP_PARTIAL);
	}

	// mappc setup
	CREATE(mappc, struct mappc_data_container, 1);
	
	// BASE: room name
	strcpy(room_name_color, get_room_name(room, TRUE));
	
	// find outer room for vehicles
	outer = room;
	while (GET_ROOM_VEHICLE(outer) && IN_ROOM(GET_ROOM_VEHICLE(outer))) {
		outer = IN_ROOM(GET_ROOM_VEHICLE(outer));
	}
	
	// put ship in name
	if (ship_partial && GET_ROOM_VEHICLE(IN_ROOM(ch))) {
		strcpy(tmpbuf, skip_filler(VEH_SHORT_DESC(GET_ROOM_VEHICLE(IN_ROOM(ch)))));
		ucwords(tmpbuf);
		snprintf(veh_buf, sizeof(veh_buf), ", %s the %s", VEH_FLAGGED(GET_ROOM_VEHICLE(IN_ROOM(ch)), VEH_IN) ? "Inside" : "Aboard", tmpbuf);
	}
	else {
		*veh_buf = '\0';
	}
	
	// set up location: may not actually have a map location
	check_x = X_COORD(room);
	check_y = Y_COORD(room);
	if (CHECK_MAP_BOUNDS(check_x, check_y)) {
		snprintf(locbuf, sizeof(locbuf), "(%d, %d)", check_x, check_y);
	}
	else {
		snprintf(locbuf, sizeof(locbuf), "(unknown)");
	}
	
	// append (Real Name) of a room if the name has been customized and DOESN'T appear in the custom name
	*rlbuf = '\0';
	if (ROOM_CUSTOM_NAME(room) && !ROOM_AFF_FLAGGED(room, ROOM_AFF_HIDE_REAL_NAME) && !str_str(room_name_color, GET_BUILDING(room) ? GET_BLD_NAME(GET_BUILDING(room)) : GET_SECT_NAME(SECT(room)))) {
		sprintf(rlbuf, " (%s)", GET_BUILDING(room) ? GET_BLD_NAME(GET_BUILDING(room)) : GET_SECT_NAME(SECT(room)));
	}
	
	// coloring for adventures
	*advcolbuf = '\0';
	if ((GET_ROOM_TEMPLATE(outer) || ROOM_AFF_FLAGGED(outer, ROOM_AFF_TEMPORARY)) && (inst = find_instance_by_room(outer, FALSE, FALSE))) {
		level = (INST_LEVEL(inst) > 0 ? INST_LEVEL(inst) : get_approximate_level(ch));
		strcpy(advcolbuf, color_by_difficulty((ch), pick_level_from_range(level, GET_ADV_MIN_LEVEL(INST_ADVENTURE(inst)), GET_ADV_MAX_LEVEL(INST_ADVENTURE(inst)))));
	}

	if (IS_IMMORTAL(ch) && PRF_FLAGGED(ch, PRF_ROOMFLAGS)) {
		sprintbit(ROOM_AFF_FLAGS(IN_ROOM(ch)), room_aff_bits, flagbuf, TRUE);
		if (GET_BUILDING(IN_ROOM(ch))) {
			sprintbit(GET_BLD_FLAGS(GET_BUILDING(IN_ROOM(ch))), bld_flags, partialbuf, TRUE);
			snprintf(flagbuf + strlen(flagbuf), sizeof(flagbuf) - strlen(flagbuf), "| %s", partialbuf);
		}
		if (GET_ROOM_TEMPLATE(IN_ROOM(ch))) {
			sprintbit(GET_RMT_FLAGS(GET_ROOM_TEMPLATE(IN_ROOM(ch))), room_template_flags, partialbuf, TRUE);
			snprintf(flagbuf + strlen(flagbuf), sizeof(flagbuf) - strlen(flagbuf), "| %s", partialbuf);
		}
		
		sprintf(output, "[%d] %s%s%s%s %s&0 %s[ %s]\r\n", GET_ROOM_VNUM(room), advcolbuf, room_name_color, veh_buf, rlbuf, locbuf, (HAS_TRIGGERS(room) ? "[TRIG] " : ""), flagbuf);
	}
	else if (HAS_NAVIGATION(ch) && !NO_LOCATION(IN_ROOM(ch))) {
		// need navigation to see coords
		sprintf(output, "%s%s%s%s %s&0\r\n", advcolbuf, room_name_color, veh_buf, rlbuf, locbuf);
	}
	else {
		sprintf(output, "%s%s%s%s&0\r\n", advcolbuf, room_name_color, rlbuf, veh_buf);
	}

	// show the room
	if (!ROOM_IS_CLOSED(room) || look_out) {
		// map rooms:
		
		if (MAGIC_DARKNESS(room) && !CAN_SEE_IN_MAGIC_DARKNESS(ch) && !can_see_in_dark_room(ch, room, TRUE)) {
			// no title
			send_to_char("It is pitch black...\r\n", ch);
		}
		else if (PRF_FLAGGED(ch, PRF_SCREEN_READER)) {
			if (show_title) {
				send_to_char(output, ch);
			}
			if (!PRF_FLAGGED(ch, PRF_NO_EXITS)) {
				show_screenreader_room(ch, room, options, mapsize, NOBITS);
				if (!IS_SET(options, LRR_SHIP_PARTIAL | LRR_LOOK_OUT)) {
					msg_to_char(ch, "Here:\r\n");
				}
			}
		}
		else {	// ASCII map view
			magnitude = mapsize;
			*buf = '\0';
			
			if (show_title) {
				// spacing to center the title
				s = ((4 * (magnitude * 2 + 1)) + 2 - (strlen(output)-4))/2;
				for (t = 1; t <= s; t++) {
					send_to_char(" ", ch);
				}
				send_to_char(output, ch);
			}		
		
			// border
			send_to_char("+", ch);
			for (iter = 0; iter < (magnitude * 2 + 1); ++iter) {
				send_to_char("----", ch);
			}
			send_to_char("+\r\n", ch);
		
			// map setup
			north = get_direction_for_char(ch, NORTH);
			y_first = show_map_y_first[north];
			invert_x = (how_to_show_map[north][0] == -1);
			invert_y = (how_to_show_map[north][1] == -1);
			first_start = second_start = magnitude;
			first_end = second_end = magnitude;
			can_see_in_dark_distance = distance_can_see_in_dark(ch);
			
			// map edges?
			if (!WRAP_Y) {
				if (Y_COORD(room) < magnitude) {
					y_offset = Y_COORD(room) - magnitude;
					if (y_first) {
						first_end = Y_COORD(room);
						first_start = magnitude + magnitude - first_end;
					}
					else {
						second_end = Y_COORD(room);
						second_start = magnitude + magnitude - second_end;
					}
				}
				if (Y_COORD(room) >= (MAP_HEIGHT - magnitude)) {
					y_offset = magnitude - (MAP_HEIGHT - Y_COORD(room) - 1);
					if (y_first) {
						first_start = MAP_HEIGHT - Y_COORD(room) - 1;
						first_end = magnitude + magnitude - first_start;
					}
					else {
						second_start = MAP_HEIGHT - Y_COORD(room) - 1;
						second_end = magnitude + magnitude - second_start;
					}
				}
				
				if (invert_y) {
					if (y_first) {
						temp = first_start;
						first_start = first_end;
						first_end = temp;
					}
					else {
						temp = second_start;
						second_start = second_end;
						second_end = temp;
					}
				}
			}
			if (!WRAP_X) {
				if (X_COORD(room) < magnitude) {
					x_offset = X_COORD(room) - magnitude;
					if (y_first) {
						second_end = X_COORD(room);
						second_start = magnitude + magnitude - second_end;
					}
					else {
						first_end = X_COORD(room);
						first_start = magnitude + magnitude - first_end;
					}
				}
				if (X_COORD(room) >= (MAP_WIDTH - magnitude)) {
					x_offset = magnitude - (MAP_WIDTH - X_COORD(room) - 1);
					if (y_first) {
						second_start = MAP_WIDTH - X_COORD(room) - 1;
						second_end = magnitude + magnitude - second_start;
					}
					else {
						first_start = MAP_WIDTH - X_COORD(room) - 1;
						first_end = magnitude + magnitude - first_start;
					}
				}
				
				if (invert_x) {
					if (y_first) {
						temp = second_start;
						second_start = second_end;
						second_end = temp;
					}
					else {
						temp = first_start;
						first_start = first_end;
						first_end = temp;
					}
				}
			}
			
			// check if we need a line-of-sight grid AFTER determining offsets
			if (!PRF_FLAGGED(ch, PRF_HOLYLIGHT) && config_get_bool("line_of_sight")) {
				view_grid = build_line_of_sight_grid(ch, room, magnitude, x_offset, y_offset);
			}
		
			// which iter is x/y depends on which way is north!
			for (first_iter = first_start; first_iter >= -first_end; --first_iter) {
				msg_to_char(ch, "|");
			
				for (second_iter = second_start; second_iter >= -second_end; --second_iter) {
					xx = (y_first ? second_iter : first_iter) * (invert_x ? -1 : 1);
					yy = (y_first ? first_iter : second_iter) * (invert_y ? -1 : 1);
					show_blocked = FALSE;
					
					if (xx == 0 && yy == 0) {
						to_room = room;		
					}
					else if (view_grid) {
						to_room = real_room(view_grid[xx + magnitude + x_offset][yy + magnitude + y_offset]);
						if (!to_room) {
							show_blocked = TRUE;
							to_room = real_shift(room, xx, yy);
						}
					}
					else {
						to_room = real_shift(room, xx, yy);
					}
					
					if (!to_room) {
						// nothing to show?
						send_to_char(blocked_tile, ch);
					}
					else if (to_room != room && ROOM_AFF_FLAGGED(to_room, ROOM_AFF_MAGIC_DARKNESS) && !show_blocked) {
						// magic dark: show blank
						send_to_char("    ", ch);
					}
					else if (to_room != room && !can_see_in_dark_room(ch, to_room, FALSE)) {
						// normal dark
						dist = compute_distance(room, to_room);
						
						if (dist <= can_see_in_dark_distance) {
							// close enough to see
							if (show_blocked) {
								if ((memory = get_player_map_memory(ch, GET_ROOM_VNUM(to_room), MAP_MEM_ICON))) {
									msg_to_char(ch, "\tw%-4s", memory);
								}
								else {
									send_to_char(blocked_tile, ch);
								}
							}
							else {
								show_map_to_char(ch, mappc, to_room, options);
							}
						}
						else if ((dist <= (can_see_in_dark_distance + 2) || adjacent_room_is_light(to_room, CAN_SEE_IN_MAGIC_DARKNESS(ch))) && !PRF_FLAGGED(ch, PRF_NOMAPCOL | PRF_POLITICAL | PRF_INFORMATIVE) && !show_blocked) {
							// see-distance to see-distance+2: show as dark tile
							// note: no-map-color toggle will show these as blank instead
							show_map_to_char(ch, mappc, to_room, options | LRR_SHOW_DARK);
						}
						else {
							// too far (or color is off): show blank
							if (!PRF_FLAGGED(ch, PRF_NOMAPCOL | PRF_POLITICAL | PRF_INFORMATIVE) && (memory = get_player_map_memory(ch, GET_ROOM_VNUM(to_room), MAP_MEM_ICON))) {
								msg_to_char(ch, "\tb%-4s", memory);
							}
							else {
								send_to_char("    ", ch);
							}
						}
					}	// end dark
					else if (show_blocked) {
						if ((MAGIC_DARKNESS(to_room) || get_sun_status(to_room) == SUN_DARK) && (compute_distance(room, to_room) > can_see_in_dark_distance)) {
							// blocked dark tile
							if (!PRF_FLAGGED(ch, PRF_NOMAPCOL | PRF_POLITICAL | PRF_INFORMATIVE) && (memory = get_player_map_memory(ch, GET_ROOM_VNUM(to_room), MAP_MEM_ICON))) {
								msg_to_char(ch, "\tb%-4s", memory);
							}
							else {
								send_to_char("    ", ch);
							}
						}
						else {
							// blocked light tile
							if (!PRF_FLAGGED(ch, PRF_NOMAPCOL | PRF_POLITICAL | PRF_INFORMATIVE) && (memory = get_player_map_memory(ch, GET_ROOM_VNUM(to_room), MAP_MEM_ICON))) {
								msg_to_char(ch, "\tw%-4s", memory);
							}
							else {
								send_to_char(blocked_tile, ch);
							}
						}
					}
					else {
						// normal view
						show_map_to_char(ch, mappc, to_room, options);
					}
				}
			
				msg_to_char(ch, "&0|\r\n");
			}

			// border
			send_to_char("+", ch);
			for (iter = 0; iter < (magnitude * 2 + 1); ++iter) {
				send_to_char("----", ch);
			}
			send_to_char("+\r\n", ch);
			
			if (view_grid) {
				free_line_of_sight_grid(view_grid, magnitude);
				view_grid = NULL;
			}
		}

		// notify character they can't see in the dark
		if (!can_see_in_dark_room(ch, room, TRUE)) {
			msg_to_char(ch, "It's dark and you're having trouble seeing items and people.\r\n");
		}
	}
	else {
		// show room: non-map
		if (!can_see_in_dark_room(ch, room, TRUE)) {
			send_to_char("It is pitch black...\r\n", ch);
		}
		else {
			if (show_title) {
				send_to_char(output, ch);
			}
			
			// description
			if (!PRF_FLAGGED(ch, PRF_NO_ROOM_DESCS) && (strptr = get_room_description(room))) {
				msg_to_char(ch, "%s", strptr);
			}
		}
	}
	
	// mappc data
	if (mappc->data) {
		send_to_char("People you can see on the map: ", ch);
		
		comma = FALSE;
		for (pc = mappc->data; pc; pc = next_pc) {
			next_pc = pc->next;
			
			pcemp = GET_LOYALTY(pc->character);
			msg_to_char(ch, "%s%s%s&0", comma ? ", " : "", pcemp ? EMPIRE_BANNER(pcemp) : "", PERS(pc->character, ch, 0));
			comma = TRUE;
			
			// free as we go
			free(pc);
		}
		
		send_to_char("\r\n", ch);
	}
	free(mappc);
	
	// ship-partial ends here with some vehicles
	if (ship_partial) {
		send_to_char("\tw", ch);
		list_vehicles_to_char(ROOM_VEHICLES(room), ch, TRUE, GET_ROOM_VEHICLE(IN_ROOM(ch)));
		send_to_char("\t0", ch);
		return;
	}
	
	if (look_out) {
		// nothing else to show on a look-out
		return;
	}
	
	// brief version of exits goes here
	if (PRF_FLAGGED(ch, PRF_SHORT_EXITS) && !PRF_FLAGGED(ch, PRF_NO_EXITS) && COMPLEX_DATA(room) && ROOM_IS_CLOSED(room)) {
		do_brief_auto_exits(ch, room);
	}
	
	// commands: only show if the first entry is not a \0, which terminates the list
	if (!IS_SET(options, LRR_LOOK_OUT_INSIDE) && !PRF_FLAGGED(ch, PRF_NO_TUTORIALS)) {
		if (GET_BUILDING(room)) {
			if (GET_BLD_COMMANDS(GET_BUILDING(room)) && *GET_BLD_COMMANDS(GET_BUILDING(room))) {
				msg_to_char(ch, "Commands: &c%s&0\r\n", GET_BLD_COMMANDS(GET_BUILDING(room)));
			}
		}
		else if (GET_SECT_COMMANDS(SECT(room)) && *GET_SECT_COMMANDS(SECT(room))) {
			msg_to_char(ch, "Commands: &c%s&0\r\n", GET_SECT_COMMANDS(SECT(room)));
		}
		else if (ROOM_SECT_FLAGGED(room, SECTF_CROP)) {
			*locbuf = '\0';
			if (can_interact_room(IN_ROOM(ch), INTERACT_CHOP)) {
				sprintf(locbuf + strlen(locbuf), "%schop", (*locbuf ? ", " : ""));
			}
			if (can_interact_room(IN_ROOM(ch), INTERACT_DIG)) {
				sprintf(locbuf + strlen(locbuf), "%sdig", (*locbuf ? ", " : ""));
			}
			if (can_interact_room(IN_ROOM(ch), INTERACT_GATHER)) {
				sprintf(locbuf + strlen(locbuf), "%sgather", (*locbuf ? ", " : ""));
			}
			if (can_interact_room(IN_ROOM(ch), INTERACT_HARVEST)) {
				sprintf(locbuf + strlen(locbuf), "%sharvest", (*locbuf ? ", " : ""));
			}
			if (can_interact_room(IN_ROOM(ch), INTERACT_PICK)) {
				sprintf(locbuf + strlen(locbuf), "%spick", (*locbuf ? ", " : ""));
			}
			if (*locbuf) {
				msg_to_char(ch, "Commands: &c%s&0\r\n", locbuf);
			}
		}
	}	// end tutorials-only commands section
	
	// used from here on
	emp = ROOM_OWNER(HOME_ROOM(room));
	
	if (GET_ROOM_VEHICLE(room) && VEH_OWNER(GET_ROOM_VEHICLE(room))) {
		msg_to_char(ch, "The %s is owned by %s%s\t0%s.", skip_filler(VEH_SHORT_DESC(GET_ROOM_VEHICLE(room))), EMPIRE_BANNER(VEH_OWNER(GET_ROOM_VEHICLE(room))), EMPIRE_NAME(VEH_OWNER(GET_ROOM_VEHICLE(room))), ROOM_AFF_FLAGGED(HOME_ROOM(room), ROOM_AFF_PUBLIC) ? " (public)" : "");
		if (ROOM_PRIVATE_OWNER(HOME_ROOM(room)) != NOBODY) {
			msg_to_char(ch, " This is %s's private residence.", (index = find_player_index_by_idnum(ROOM_PRIVATE_OWNER(HOME_ROOM(room)))) ? index->fullname : "someone");
		}
		msg_to_char(ch, "\r\n");
	}
	else if (emp) {
		if ((ter_type = get_territory_type_for_empire(room, emp, FALSE, &junk, &large_radius)) == TER_CITY && (city = find_city(emp, room))) {
			msg_to_char(ch, "This is the %s%s&0 %s of %s%s.", EMPIRE_BANNER(emp), EMPIRE_ADJECTIVE(emp), city_type[city->type].name, city->name, large_radius ? " (extended radius)" : "");
		}
		else {
			msg_to_char(ch, "This area is claimed by %s%s&0%s.", EMPIRE_BANNER(emp), EMPIRE_NAME(emp), (ter_type == TER_OUTSKIRTS) ? ", on the outskirts of a city" : "");
		}
		
		if (ROOM_PRIVATE_OWNER(HOME_ROOM(room)) != NOBODY) {
			msg_to_char(ch, " This is %s's private residence.", (index = find_player_index_by_idnum(ROOM_PRIVATE_OWNER(HOME_ROOM(room)))) ? index->fullname : "someone");
		}
		
		if (ROOM_AFF_FLAGGED(HOME_ROOM(room), ROOM_AFF_PUBLIC)) {
			msg_to_char(ch, " (public)");
		}
		if (emp == GET_LOYALTY(ch) && ROOM_AFF_FLAGGED(HOME_ROOM(room), ROOM_AFF_NO_DISMANTLE)) {
			msg_to_char(ch, " (no-dismantle)");
		}
		if (emp == GET_LOYALTY(ch) && ROOM_AFF_FLAGGED(HOME_ROOM(room), ROOM_AFF_NO_ABANDON)) {
			msg_to_char(ch, " (no-abandon)");
		}
		
		msg_to_char(ch, "\r\n");
		
		// owned but not in-city?
		if ((ROOM_BLD_FLAGGED(room, BLD_IN_CITY_ONLY) || HAS_FUNCTION(room, FNC_IN_CITY_ONLY)) && !is_in_city_for_empire(room, emp, TRUE, &junk)) {
			msg_to_char(ch, "\trThis building needs to be in an established city.\t0\r\n");
		}
	}
	else if (!emp && ROOM_AFF_FLAGGED(HOME_ROOM(room), ROOM_AFF_NO_DISMANTLE)) {
		// show no-dismantle anyway
		msg_to_char(ch, "This area is (no-dismantle).\r\n");
	}
	
	if (ROOM_PAINT_COLOR(room)) {
		sprinttype(ROOM_PAINT_COLOR(room), paint_names, col_buf, sizeof(col_buf), "UNDEFINED");
		*col_buf = LOWER(*col_buf);
		msg_to_char(ch, "The building %s %s%s.\r\n", (ROOM_AFF_FLAGGED(room, ROOM_AFF_PERMANENT_PAINT) ? "is" : "has been painted"), (ROOM_AFF_FLAGGED(room, ROOM_AFF_BRIGHT_PAINT) ? "bright " : ""), col_buf);
	}
	
	if (emp && GET_LOYALTY(ch) == emp && ROOM_AFF_FLAGGED(room, ROOM_AFF_NO_WORK)) {
		msg_to_char(ch, "Workforce will not work this tile.\r\n");
	}
	
	// don't show this in adventures, which are always unclaimable
	if (ROOM_AFF_FLAGGED(room, ROOM_AFF_UNCLAIMABLE) && !IS_ADVENTURE_ROOM(room)) {
		msg_to_char(ch, "This area is unclaimable.\r\n");
	}
	
	if (HAS_MAJOR_DISREPAIR(room)) {
		msg_to_char(ch, "It's damaged and in a state of serious disrepair.\r\n");
	}
	else if (HAS_MINOR_DISREPAIR(room)) {
		msg_to_char(ch, "It's starting to show some wear.\r\n");
	}
	
	if (IS_ROAD(room) && ROOM_CUSTOM_DESCRIPTION(room)) {
		msg_to_char(ch, "Sign: %s\r\n", ROOM_CUSTOM_DESCRIPTION(room));
	}
	
	if (ROOM_PATRON(room) && (index = find_player_index_by_idnum(ROOM_PATRON(room)))) {
		msg_to_char(ch, "It is dedicated to %s.\r\n", index->fullname);
	}
	
	// custom descs on OPEN buildings show here
	if (IS_MAP_BUILDING(room) && IS_COMPLETE(room) && !ROOM_IS_CLOSED(room) && ROOM_CUSTOM_DESCRIPTION(room)) {
		send_to_char(ROOM_CUSTOM_DESCRIPTION(room), ch);
	}
	
	if (ROOM_SECT_FLAGGED(room, SECTF_IS_TRENCH)) {
		if (get_room_extra_data(room, ROOM_EXTRA_TRENCH_PROGRESS) >= 0) {
			msg_to_char(ch, "The trench is complete and filling with water.\r\n");
		}
		else {
			msg_to_char(ch, "The trench is %d%% complete.\r\n", ABSOLUTE((trench_initial_value - get_room_extra_data(room, ROOM_EXTRA_TRENCH_PROGRESS)) * 100 / trench_initial_value));
		}
	}

	if (room_has_function_and_city_ok(GET_LOYALTY(ch), room, FNC_MINE) && IS_COMPLETE(room)) {
		if (get_room_extra_data(room, ROOM_EXTRA_MINE_AMOUNT) <= 0) {
			msg_to_char(ch, "This mine is depleted.\r\n");
		}
		else if (GET_LOYALTY(ch) && get_room_extra_data(IN_ROOM(ch), ROOM_EXTRA_PROSPECT_EMPIRE) == EMPIRE_VNUM(GET_LOYALTY(ch))) {
			strcpy(locbuf, get_mine_type_name(room));
			msg_to_char(ch, "This appears to be %s %s.\r\n", AN(locbuf), locbuf);
		}
	}
	
	// has a crop but doesn't show as crop probably == seeded field
	if (ROOM_SECT_FLAGGED(room, SECTF_HAS_CROP_DATA) && !ROOM_SECT_FLAGGED(room, SECTF_CROP) && (cp = ROOM_CROP(room))) {
		msg_to_char(ch, "The field is seeded with %s.\r\n", GET_CROP_NAME(cp));
	}

	if (BUILDING_RESOURCES(room)) {
		show_resource_list(BUILDING_RESOURCES(room), partialbuf);
		msg_to_char(ch, "Remaining to %s: %s\r\n", (IS_DISMANTLING(room) ? "Dismantle" : (IS_INCOMPLETE(room) ? "Completion" : "Maintain")), partialbuf);
	}
	
	if (IS_BURNING(room)) {
		msg_to_char(ch, "\t[B300]The building is on fire!\t0\r\n");
	}
	if (GET_ROOM_VEHICLE(room) && VEH_FLAGGED(GET_ROOM_VEHICLE(room), VEH_ON_FIRE)) {
		msg_to_char(ch, "\t[B300]The %s has caught on fire!\t0\r\n", skip_filler(VEH_SHORT_DESC(GET_ROOM_VEHICLE(room))));
	}
	
	if (ROOM_AFF_FLAGGED(room, ROOM_AFF_MAGIC_DARKNESS)) {
		msg_to_char(ch, "The area is blanketed in an inky darkness.\r\n");
	}

	if (!AFF_FLAGGED(ch, AFF_EARTHMELDED)) {
		if (can_get_quest_from_room(ch, room, NULL)) {
			msg_to_char(ch, "\tA...there is a quest here for you!\t0\r\n");
		}
		if (can_turn_quest_in_to_room(ch, room, NULL)) {
			msg_to_char(ch, "\tA...you can turn in a quest here!\t0\r\n");
		}
	
		/* now list characters, vehicles, & objects */
		send_to_char("\tw", ch);
		list_vehicles_to_char(ROOM_VEHICLES(room), ch, FALSE, IS_SET(options, LRR_LOOK_OUT_INSIDE) ? GET_ROOM_VEHICLE(IN_ROOM(ch)) : NULL);
		
		if (!IS_SET(options, LRR_LOOK_OUT_INSIDE)) {
			send_to_char("\tg", ch);
			list_obj_to_char(ROOM_CONTENTS(room), ch, OBJ_DESC_LONG, FALSE);
		}
		
		if (!IS_SET(options, LRR_LOOK_OUT_INSIDE)) {
			send_to_char("\ty", ch);
			list_char_to_char(ROOM_PEOPLE(room), ch);
		}
		
		send_to_char("\t0", ch);
	}

	/* Exits ? */
	if (!PRF_FLAGGED(ch, PRF_NO_EXITS | PRF_SHORT_EXITS) && COMPLEX_DATA(room) && ROOM_IS_CLOSED(room)) {
		do_exits(ch, "", -1, GET_ROOM_VNUM(room));
	}
	
	if (IS_OUTDOORS(ch) && GET_ROOM_VNUM(IN_ROOM(ch)) < MAP_SIZE) {
		gain_player_tech_exp(ch, PTECH_MAP_MEMORY, 0.1);
	}
}


/**
* Called when a player types 'look <direction>' with no further pre-validation.
*
* @param char_data *ch The player.
* @param int dir The direction they typed.
*/
void look_in_direction(char_data *ch, int dir) {
	char buf[MAX_STRING_LENGTH];
	char *exdesc;
	vehicle_data *veh;
	char_data *c;
	room_data *to_room;
	struct room_direction_data *ex;
	struct string_hash *str_hash = NULL;
	
	// check first for an extra description that covers the direction
	snprintf(buf, sizeof(buf), "%s", dirs[dir]);
	if ((exdesc = find_exdesc_for_char(ch, buf, NULL, NULL, NULL, NULL))) {
		send_to_char(exdesc, ch);
		return;
	}
	
	// weather override
	if (IS_OUTDOORS(ch) && dir == UP && !find_exit(IN_ROOM(ch), dir)) {
		do_weather(ch, "", 0, 0);
		return;
	}

	if (ROOM_IS_CLOSED(IN_ROOM(ch))) {
		if (COMPLEX_DATA(IN_ROOM(ch)) && (ex = find_exit(IN_ROOM(ch), dir))) {
			if (EXIT_FLAGGED(ex, EX_CLOSED) && ex->keyword) {
				msg_to_char(ch, "The %s is closed.\r\n", fname(ex->keyword));
			}
			else if (EXIT_FLAGGED(ex, EX_ISDOOR) && ex->keyword) {
				msg_to_char(ch, "The %s is open.\r\n", fname(ex->keyword));
			}
			if (!EXIT_FLAGGED(ex, EX_CLOSED)) {
				to_room = ex->room_ptr;
				if (can_see_in_dark_room(ch, to_room, TRUE)) {
					// find people
					DL_FOREACH2(ROOM_PEOPLE(to_room), c, next_in_room) {
						if (!AFF_FLAGGED(c, AFF_HIDDEN | AFF_NO_SEE_IN_ROOM) && CAN_SEE(ch, c) && WIZHIDE_OK(ch, c)) {
							add_string_hash(&str_hash, PERS(c, ch, FALSE), 1);
						}
					}
					
					// find vehicles
					DL_FOREACH2(ROOM_VEHICLES(to_room), veh, next_in_room) {
						if (CAN_SEE_VEHICLE(ch, veh)) {
							add_string_hash(&str_hash, get_vehicle_short_desc(veh, ch), 1);
						}
					}
				}
				
				// prepare for display
				if (str_hash) {
					string_hash_to_string(str_hash, buf, sizeof(buf) - 14, TRUE, TRUE, TRUE);
					free_string_hash(&str_hash);
					msg_to_char(ch, "You see %s.\r\n", buf);
				}
				else {
					msg_to_char(ch, "You don't see anyone in that direction.\r\n");
				}
			}
		}
		else {
			send_to_char("Nothing special there...\r\n", ch);
		}
	}
	else {
		// map look:
		// dirs not shown on map
		if (dir == UP) {
			do_weather(ch, "", 0, 0);
			return;
		}
		else if (dir == DOWN) {
			msg_to_char(ch, "Yep, your feet are still there.\r\n");
			return;
		}
		else if (dir >= NUM_2D_DIRS) {
			msg_to_char(ch, "Nothing special there...\r\n");
			return;
		}

		to_room = real_shift(IN_ROOM(ch), shift_dir[dir][0], shift_dir[dir][1]);

		// blocked?
		if (!to_room || ROOM_SECT_FLAGGED(to_room, SECTF_OBSCURE_VISION)) {
			msg_to_char(ch, "You can't see anything that way.\r\n");
			return;
		}
		if (IS_ANY_BUILDING(to_room) && ROOM_IS_CLOSED(to_room)) {
			// start the sentence...
			msg_to_char(ch, "You see %s %s", (GET_BUILDING(to_room) ? AN(GET_BLD_NAME(GET_BUILDING(to_room))) : "a"), (GET_BUILDING(to_room) ? GET_BLD_NAME(GET_BUILDING(to_room)) : "building"));
			
			if (ROOM_OWNER(to_room)) {
				msg_to_char(ch, " owned by %s", EMPIRE_NAME(ROOM_OWNER(to_room)));
			}
			
			if (!ROOM_BLD_FLAGGED(to_room, BLD_OPEN)) {
				if (ROOM_BLD_FLAGGED(to_room, BLD_TWO_ENTRANCES)) {
					msg_to_char(ch, ", with entrances from %s and %s", from_dir[get_direction_for_char(ch, BUILDING_ENTRANCE(to_room))], from_dir[get_direction_for_char(ch, rev_dir[BUILDING_ENTRANCE(to_room)])]);
				}
				else {
					msg_to_char(ch, ", with an entrance from %s", from_dir[get_direction_for_char(ch, BUILDING_ENTRANCE(to_room))]);
				}
			}

			// trailing crlf
			send_to_char(".\r\n", ch);
			return;
		}

		if (can_see_in_dark_room(ch, to_room, FALSE)) {
			DL_FOREACH2(ROOM_PEOPLE(to_room), c, next_in_room) {
				if (!AFF_FLAGGED(c, AFF_HIDDEN | AFF_NO_SEE_IN_ROOM) && CAN_SEE(ch, c) && WIZHIDE_OK(ch, c)) {
					add_string_hash(&str_hash, PERS(c, ch, FALSE), 1);
				}
			}
			
			DL_FOREACH2(ROOM_VEHICLES(to_room), veh, next_in_room) {
				if (CAN_SEE_VEHICLE(ch, veh)) {
					add_string_hash(&str_hash, get_vehicle_short_desc(veh, ch), 1);
				}
			}
		}
		
		/* Shift, rinse, repeat */
		to_room = real_shift(to_room, shift_dir[dir][0], shift_dir[dir][1]);
		if (to_room && !ROOM_SECT_FLAGGED(to_room, SECTF_OBSCURE_VISION) && !ROOM_IS_CLOSED(to_room)) {
			if (can_see_in_dark_room(ch, to_room, FALSE)) {
				DL_FOREACH2(ROOM_PEOPLE(to_room), c, next_in_room) {
					if (!AFF_FLAGGED(c, AFF_HIDDEN | AFF_NO_SEE_IN_ROOM) && CAN_SEE(ch, c) && WIZHIDE_OK(ch, c)) {
						add_string_hash(&str_hash, PERS(c, ch, FALSE), 1);
					}
				}
			}
			/* And a third time for good measure */
			to_room = real_shift(to_room, shift_dir[dir][0], shift_dir[dir][1]);
			if (to_room && !ROOM_SECT_FLAGGED(to_room, SECTF_OBSCURE_VISION) && !ROOM_IS_CLOSED(to_room)) {
				if (can_see_in_dark_room(ch, to_room, FALSE)) {
					DL_FOREACH2(ROOM_PEOPLE(to_room), c, next_in_room) {
						if (!AFF_FLAGGED(c, AFF_HIDDEN | AFF_NO_SEE_IN_ROOM) && CAN_SEE(ch, c) && WIZHIDE_OK(ch, c)) {
							add_string_hash(&str_hash, PERS(c, ch, FALSE), 1);
						}
					}
				}
			}
		}
		
		// prepare for display
		if (str_hash) {
			string_hash_to_string(str_hash, buf, sizeof(buf) - 14, TRUE, TRUE, TRUE);
			free_string_hash(&str_hash);
			msg_to_char(ch, "You see %s.\r\n", buf);
		}
		else {
			msg_to_char(ch, "You don't see anyone in that direction.\r\n");
		}
	}
}

/**
* Shows one tile
*
* @param char_data *ch the viewer
* @param struct mappc_data_container *mappc Players visible on the map are stored in this, to be shown below the map
* @param room_data *to_room The room the character is looking at.
* @param bitvector_t options Will recolor the tile if TRUE
*/
static void show_map_to_char(char_data *ch, struct mappc_data_container *mappc, room_data *to_room, bitvector_t options) {
	bool need_color_terminator = FALSE;
	char no_color[256], col_buf[256], lbuf[MAX_STRING_LENGTH], self_icon[256], mappc_icon[256], map_icon[256], veh_icon[256], show_icon[256], temp[256], icon_color[256];
	int iter;
	int tileset = GET_SEASON(to_room);
	struct icon_data *base_icon, *crop_icon = NULL;
	bool junk, painted, veh_is_shown = FALSE, person_is_shown = FALSE;
	char *base_color, *str;
	vehicle_data *show_veh = NULL;
	
	// options
	bool show_dark = IS_SET(options, LRR_SHOW_DARK) ? TRUE : FALSE;
	bool remember_icon = has_player_tech(ch, PTECH_MAP_MEMORY) ? TRUE : FALSE;
	
	// clear icon strings
	*self_icon = *mappc_icon = *map_icon = *veh_icon = *show_icon = *icon_color = '\0';
	
	// detect base icon
	base_icon = get_icon_from_set(GET_SECT_ICONS(BASE_SECT(to_room)), tileset);
	base_color = base_icon->color;
	if (ROOM_SECT_FLAGGED(to_room, SECTF_CROP) && ROOM_CROP(to_room)) {
		crop_icon = get_icon_from_set(GET_CROP_ICONS(ROOM_CROP(to_room)), tileset);
		base_color = crop_icon->color;
	}
	
	show_veh = find_vehicle_to_show(ch, to_room, NULL);
	painted = (!IS_NPC(ch) && !PRF_FLAGGED(ch, PRF_NO_PAINT)) ? (show_veh ? VEH_PAINT_COLOR(show_veh) : ROOM_PAINT_COLOR(to_room)) : FALSE;

	// 1: Build basic icon strings: self_icon, mappc_icon, veh_icon, map_icon (any of these may be empty)
	
	// build possible icons: self, pc/mob
	if (to_room == IN_ROOM(ch) && !ROOM_IS_CLOSED(IN_ROOM(ch))) {
		sprintf(self_icon, "&0<%soo&0>", GET_LOYALTY(ch) ? EMPIRE_BANNER(GET_LOYALTY(ch)) : "");
	}
	else if (!show_dark && !PRF_FLAGGED(ch, PRF_INFORMATIVE | PRF_POLITICAL) && show_pc_in_room(ch, to_room, mappc, mappc_icon)) {
		// no work here: show_pc_in_room already set mappc_icon
	}
	
	// check for a vehicle with an icon: we do this even if it won't be displayed later (because it may be stored as the tile icon)
	if (show_veh && VEH_HAS_ANY_ICON(show_veh)) {
		// strcpy(veh_icon, VEH_ICON(show_veh));
		build_vehicle_icon(ch, to_room, show_veh, FALSE, veh_icon);
	}
	
	// determine if we need to build a map icon:
	if ((!*self_icon && !*mappc_icon && !*veh_icon) || remember_icon) {
		build_map_icon(ch, to_room, base_icon, crop_icon, tileset, map_icon, icon_color);
	}
	
	// ensure icon color
	if (!*icon_color) {
		strcpy(icon_color, base_color);
	}
	
	// 2. Determine which icon will be shown (but veh_/map_icon is often used later too)
	if (*self_icon) {
		strcpy(show_icon, self_icon);
		person_is_shown = TRUE;
	}
	else if (*mappc_icon) {
		strcpy(show_icon, mappc_icon);
		person_is_shown = TRUE;
	}
	else if (*veh_icon) {
		strcpy(show_icon, veh_icon);
		veh_is_shown = TRUE;
	}
	else if (*map_icon) {
		strcpy(show_icon, map_icon);
	}
	else {	// no valid icon??
		strcpy(show_icon, "????");
	}
	
	replace_icon_codes(ch, to_room, show_icon, tileset);
	
	// 3. Check for special icon coloring including &?
	if (IS_BURNING(to_room)) {
		strcpy(no_color, strip_color(show_icon));
		sprintf(show_icon, "\t0\t[B300]%s", no_color);
		need_color_terminator = TRUE;
	}
	else if (PRF_FLAGGED(ch, PRF_NOMAPCOL | PRF_POLITICAL | PRF_INFORMATIVE) || show_dark) {
		strcpy(no_color, strip_color(show_icon));
		
		if (PRF_FLAGGED(ch, PRF_POLITICAL | PRF_INFORMATIVE) && !show_dark && veh_is_shown) {
			// leave show_icon alone: it's colored for these already
		}
		else if (PRF_FLAGGED(ch, PRF_POLITICAL) && !show_dark) {
			if (GET_LOYALTY(ch) && (GET_LOYALTY(ch) == ROOM_OWNER(to_room) || find_city(GET_LOYALTY(ch), to_room)) && is_in_city_for_empire(to_room, GET_LOYALTY(ch), FALSE, &junk)) {
				strcpy(temp, get_banner_complement_color(ch, GET_LOYALTY(ch)));
				need_color_terminator = TRUE;
			}
			else {
				*temp = '\0';
			}
			
			if (show_veh && !VEH_FLAGGED(show_veh, VEH_NO_CLAIM)) {
				sprintf(show_icon, "%s%s%s", (VEH_OWNER(show_veh) && EMPIRE_BANNER(VEH_OWNER(show_veh))) ? EMPIRE_BANNER(VEH_OWNER(show_veh)) : "&0", temp, no_color);
			}
			else if (ROOM_OWNER(to_room) && (!CHECK_CHAMELEON(IN_ROOM(ch), to_room) || ROOM_OWNER(to_room) == GET_LOYALTY(ch))) {
				sprintf(show_icon, "%s%s%s", EMPIRE_BANNER(ROOM_OWNER(to_room)) ? EMPIRE_BANNER(ROOM_OWNER(to_room)) : "&0", temp, no_color);
			}
			else {
				sprintf(show_icon, "&0%s%s", temp, no_color);
			}
		}
		else if (PRF_FLAGGED(ch, PRF_INFORMATIVE) && !show_dark) {
			if (show_veh) {
				sprintf(show_icon, "%s%s", get_informative_color_veh(ch, show_veh), no_color);
			}
			else {
				sprintf(show_icon, "%s%s", get_informative_color_room(ch, to_room), no_color);
			}
		}
		else if (!PRF_FLAGGED(ch, PRF_NOMAPCOL | PRF_INFORMATIVE | PRF_POLITICAL) && show_dark) {
			sprintf(show_icon, "&b%s", no_color);
		}
		else {
			// color was stripped but no color added, so add a "normal" color to prevent color bleed
			sprintf(show_icon, "&0%s", no_color);
		}
	}
	else if (painted && !person_is_shown && (!show_veh || veh_is_shown)) {
		sprinttype(show_veh ? VEH_PAINT_COLOR(show_veh) : ROOM_PAINT_COLOR(to_room), paint_colors, col_buf, sizeof(col_buf), "&0");
		if (show_veh ? VEH_FLAGGED(show_veh, VEH_BRIGHT_PAINT) : ROOM_AFF_FLAGGED(to_room, ROOM_AFF_BRIGHT_PAINT)) {
			strtoupper(col_buf);
		}
		
		// show_icon is the icon
		replace_color_codes(show_icon, col_buf);
		
		// check for ? colors
		if (strstr(show_icon, "&?")) {
			replace_question_color(show_icon, base_color, lbuf);
			strcpy(show_icon, lbuf);
		}
		// need a leading color? This is ignored if the icon appears to start with a color code other than &u or &&
		if (*show_icon != COLOUR_CHAR || *(show_icon+1) == COLOUR_CHAR || *(show_icon+1) == 'u') {
			snprintf(lbuf, sizeof(lbuf), "%s%s", icon_color, show_icon);
			strcpy(show_icon, lbuf);
		}
	}
	else {
		// need a leading color? This is ignored if the icon appears to start with a color code other than &u or &&
		if (*show_icon != COLOUR_CHAR || *(show_icon+1) == COLOUR_CHAR || *(show_icon+1) == 'u') {
			snprintf(lbuf, sizeof(lbuf), "%s%s", icon_color, show_icon);
			strcpy(show_icon, lbuf);
		}
		
		// normal color
		if (strstr(show_icon, "&?")) {
			replace_question_color(show_icon, base_color, lbuf);
			strcpy(show_icon, lbuf);
		}
		if (strstr(show_icon, "&#")) {
			str = str_replace("&#", ROOM_AFF_FLAGGED(to_room, ROOM_AFF_NO_FLY) ? "&m" : "&0", show_icon);
			strcpy(show_icon, str);
			free(str);
		}
		
		// stoned coloring
		if (AFF_FLAGGED(ch, AFF_STONED)) {
			// check all but the final char
			for (iter = 0; show_icon[iter] != 0 && show_icon[iter+1] != 0; ++iter) {
				if (show_icon[iter] == COLOUR_CHAR || show_icon[iter] == '\t') {
					switch(show_icon[iter+1]) {
						case 'r':	show_icon[iter+1] = 'b';	break;
						case 'g':	show_icon[iter+1] = 'c';	break;
						case 'y':	show_icon[iter+1] = 'm';	break;
						case 'b':	show_icon[iter+1] = 'g';	break;
						case 'm':	show_icon[iter+1] = 'r';	break;
						case 'c':	show_icon[iter+1] = 'y';	break;
						case 'p':	show_icon[iter+1] = 'l';	break;
						case 'v':	show_icon[iter+1] = 'o';	break;
						case 'a':	show_icon[iter+1] = 't';	break;
						case 'l':	show_icon[iter+1] = 'w';	break;
						case 'j':	show_icon[iter+1] = 'v';	break;
						case 'o':	show_icon[iter+1] = 'p';	break;
						case 't':	show_icon[iter+1] = 'j';	break;
						case 'R':	show_icon[iter+1] = 'B';	break;
						case 'G':	show_icon[iter+1] = 'C';	break;
						case 'Y':	show_icon[iter+1] = 'M';	break;
						case 'B':	show_icon[iter+1] = 'G';	break;
						case 'M':	show_icon[iter+1] = 'R';	break;
						case 'C':	show_icon[iter+1] = 'Y';	break;
						case 'P':	show_icon[iter+1] = 'L';	break;
						case 'V':	show_icon[iter+1] = 'O';	break;
						case 'A':	show_icon[iter+1] = 'T';	break;
						case 'L':	show_icon[iter+1] = 'W';	break;
						case 'J':	show_icon[iter+1] = 'V';	break;
						case 'O':	show_icon[iter+1] = 'P';	break;
						case 'T':	show_icon[iter+1] = 'J';	break;
					}
				}
			}
		}
	}
	
	if (need_color_terminator) {
		strcat(show_icon, "&0");
	}
	
	// record uncolored version as memory
	if (has_player_tech(ch, PTECH_MAP_MEMORY)) {
		if (veh_is_shown) {
			// rebuild without chameleon/vehicles:
			build_vehicle_icon(ch, to_room, show_veh, TRUE, veh_icon);
			
			// memorize building-vehicle icon
			replace_icon_codes(ch, to_room, veh_icon, tileset);
			add_player_map_memory(ch, GET_ROOM_VNUM(to_room), veh_icon, NULL, 0);
		}
		else {
			// memorize map icon (may be a map building)
			// TODO: should this ignore chameleon buildings and show the terrain instead? if so, split buildings from other icons
			replace_icon_codes(ch, to_room, map_icon, tileset);
			add_player_map_memory(ch, GET_ROOM_VNUM(to_room), map_icon, NULL, 0);
		}
		
		// this will add the name, too
		// TODO: should this override chameleon and always remember the base tile?
		get_screenreader_room_name(ch, IN_ROOM(ch), to_room, FALSE);
	}
	
	send_to_char(show_icon, ch);
}


 //////////////////////////////////////////////////////////////////////////////
//// SCREEN READER FUNCTIONS /////////////////////////////////////////////////

/**
* Gets the short name for a room, for a screenreader user.
*
* @param char_data *ch The person to get the view for.
* @param room_data *from_room The room being viewed from.
* @param room_data *to_room The room being shown.
* @param bool show_dark If TRUE, tries to modify the name for darkened tiles.
* @return char* The string to show.
*/
char *get_screenreader_room_name(char_data *ch, room_data *from_room, room_data *to_room, bool show_dark) {
	static char lbuf[MAX_STRING_LENGTH];
	char temp[MAX_STRING_LENGTH], temp2[MAX_STRING_LENGTH], junk[MAX_STRING_LENGTH];
	bool whole_dark = FALSE, partial_dark = FALSE;
	crop_data *cp;
	
	strcpy(temp, "*");
	
	if (CHECK_CHAMELEON(from_room, to_room)) {
		strcpy(temp, GET_SECT_NAME(BASE_SECT(to_room)));
		partial_dark = show_dark;
	}
	/* not showing this anymore
	else if (GET_BUILDING(to_room) && ROOM_BLD_FLAGGED(to_room, BLD_BARRIER) && ROOM_AFF_FLAGGED(to_room, ROOM_AFF_NO_FLY)) {
		sprintf(temp, "Enchanted %s", GET_BLD_NAME(GET_BUILDING(to_room)));
		whole_dark = show_dark;
	}
	*/
	else if (GET_BUILDING(to_room)) {
		strcpy(temp, GET_BLD_NAME(GET_BUILDING(to_room)));
		whole_dark = show_dark;
	}
	else if (GET_ROOM_TEMPLATE(to_room)) {
		strcpy(temp, GET_RMT_TITLE(GET_ROOM_TEMPLATE(to_room)));
	}
	else if (ROOM_SECT_FLAGGED(to_room, SECTF_CROP) && (cp = ROOM_CROP(to_room))) {
		strcpy(temp, GET_CROP_NAME(cp));
		partial_dark = show_dark;
	
		// fix case issues on crops
		strtotitlecase(temp);
	}
	else {
		strcpy(temp, GET_SECT_NAME(SECT(to_room)));
		partial_dark = show_dark;
	}
	
	// dark check
	if (whole_dark) {
		// add Dark to whole tile name
		strcpy(temp2, temp);
		sprintf(temp, "Dark %s", temp2);
	}
	else if (partial_dark) {
		// on request, some types apply 'Dark' to a shortened name
		if (strchr(temp, ' ')) {
			chop_last_arg(temp, junk, temp2);
		}
		else {
			strcpy(temp2, temp);
		}
		sprintf(temp, "Dark %s", temp2);
	}
	else if (has_player_tech(ch, PTECH_MAP_MEMORY)) {
		// not dark: memorize it?
		if (strchr(temp, ' ')) {
			chop_last_arg(temp, junk, temp2);
		}
		else {
			*temp2 = '\0';
		}
		add_player_map_memory(ch, GET_ROOM_VNUM(to_room), NULL, *temp2 ? temp2 : temp, 0);
	}
	
	// start lbuf: color
	if (ROOM_PAINT_COLOR(to_room) && !PRF_FLAGGED(ch, PRF_NO_PAINT)) {
		sprinttype(ROOM_PAINT_COLOR(to_room), paint_names, lbuf, sizeof(lbuf), "Painted");
		strcat(lbuf, " ");
	}
	else {
		*lbuf = '\0';
	}
	
	// now check custom name
	if (ROOM_CUSTOM_NAME(to_room) && !CHECK_CHAMELEON(from_room, to_room)) {
		if (ROOM_AFF_FLAGGED(to_room, ROOM_AFF_HIDE_REAL_NAME)) {
			sprintf(lbuf + strlen(lbuf), "%s", ROOM_CUSTOM_NAME(to_room));
		}
		else {
			sprintf(lbuf + strlen(lbuf), "%s/%s", ROOM_CUSTOM_NAME(to_room), temp);
		}
	}
	else {
		strcat(lbuf, temp);
	}
	
	return lbuf;
}


/**
* Displays the tiles in one direction from the character, up to their mapsize
* (radius) in distance.
*
* @param char_data *ch The player.
* @param room_data *origin Where the player is looking from.
* @param int dir The direction they are looking.
* @param int max_dist How far to show (invalid distances <= 0 will use the default map size).
*/
void screenread_one_dir(char_data *ch, room_data *origin, int dir, int max_dist) {
	char buf[MAX_STRING_LENGTH], roombuf[MAX_INPUT_LENGTH], lastroom[MAX_INPUT_LENGTH], color[4];
	char dirbuf[MAX_STRING_LENGTH];
	int dist, dist_iter, can_see_in_dark_distance, view_height, r_height;
	room_data *to_room;
	int count, repeats, top_height;
	bool blocking_veh, check_blocking, is_blocked = FALSE;
	bool allow_stacking = TRUE;	// always
	const char *memory;
	
	if (!ch->desc) {
		return;	// nobody to show it to
	}
	
	// safety
	if (max_dist <= 0) {
		max_dist = config_get_int("default_map_size");
	}

	// setup
	check_blocking = (!PRF_FLAGGED(ch, PRF_HOLYLIGHT) && config_get_bool("line_of_sight"));
	can_see_in_dark_distance = distance_can_see_in_dark(ch);
	*dirbuf = '\0';
	*lastroom = '\0';
	repeats = 0;
	top_height = 0;
	count = 0;
	view_height = get_view_height(ch, origin);

	// show distance that direction		
	for (dist_iter = 1; dist_iter <= max_dist; ++dist_iter) {
		to_room = real_shift(origin, shift_dir[dir][0] * dist_iter, shift_dir[dir][1] * dist_iter);
		
		if (!to_room) {
			break;
		}
		
		r_height = get_room_blocking_height(to_room, &blocking_veh);
		
		*roombuf = '\0';
		if (is_blocked && r_height <= top_height && (!ROOM_OWNER(to_room) || ROOM_OWNER(to_room) != GET_LOYALTY(ch))) {
			// blocked by closer tile
			if ((memory = get_player_map_memory(ch, GET_ROOM_VNUM(to_room), MAP_MEM_NAME))) {
				snprintf(roombuf, sizeof(roombuf), "Blocked %s", memory);
			}
			else {
				strcpy(roombuf, "Blocked");
			}
		}
		else if (ROOM_AFF_FLAGGED(to_room, ROOM_AFF_MAGIC_DARKNESS)) {
			// magic dark
			strcpy(roombuf, "Dark");
		}
		else if (!can_see_in_dark_room(ch, to_room, FALSE)) {
			// normal dark
			dist = compute_distance(origin, to_room);
			
			if (dist <= can_see_in_dark_distance) {
				// close enough to see
				strcpy(roombuf, screenread_one_tile(ch, origin, to_room, FALSE));
			}
			else if (PRF_FLAGGED(ch, PRF_POLITICAL | PRF_INFORMATIVE)) {
				// political and informative don't show the 'dim' section
				strcpy(roombuf, "Dark");
			}
			else if ((dist <= (can_see_in_dark_distance + 2) || adjacent_room_is_light(to_room, CAN_SEE_IN_MAGIC_DARKNESS(ch))) && !PRF_FLAGGED(ch, PRF_POLITICAL | PRF_INFORMATIVE)) {
				// see-distance to see-distance+2: show as dim tile (if political/informative are off)
				strcpy(roombuf, screenread_one_tile(ch, origin, to_room, TRUE));
			}
			else {
				// too far: show only darkness
				if ((memory = get_player_map_memory(ch, GET_ROOM_VNUM(to_room), MAP_MEM_NAME))) {
					snprintf(roombuf, sizeof(roombuf), "Dark %s", memory);
				}
				else {
					strcpy(roombuf, "Dark");
				}
			}
		}
		else {	// not dark
			strcpy(roombuf, screenread_one_tile(ch, origin, to_room, FALSE));
		}
		
		// check for repeats?
		if (allow_stacking && !strcmp(lastroom, roombuf)) {
			// same as last one
			++repeats;
		}
		else {
			// different
			if (*lastroom) {
				snprintf(color, sizeof(color), "\t%c", (++count % 2) ? 'w' : '0');
				if (repeats > 0) {
					snprintf(dirbuf + strlen(dirbuf), sizeof(dirbuf) - strlen(dirbuf), "%s%s%dx %s", *dirbuf ? ", " : "", color, repeats+1, lastroom);
				}
				else {
					snprintf(dirbuf + strlen(dirbuf), sizeof(dirbuf) - strlen(dirbuf), "%s%s%s", *dirbuf ? ", " : "", color, lastroom);
				}
			}
			
			// reset
			repeats = 0;
			strcpy(lastroom, roombuf);
		}
		
		// record new top height
		top_height = MAX(top_height, r_height);
		
		// check blocking
		if (check_blocking && !is_blocked && (blocking_veh || ROOM_SECT_FLAGGED(to_room, SECTF_OBSCURE_VISION) || SECT_FLAGGED(BASE_SECT(to_room), SECTF_OBSCURE_VISION) || ROOM_BLD_FLAGGED(to_room, BLD_OBSCURE_VISION)) && r_height >= view_height) {
			is_blocked = TRUE;
		}
	}
	
	// check for lingering data to append
	if (*lastroom) {
		snprintf(color, sizeof(color), "\t%c", (++count % 2) ? 'w' : '0');
		if (repeats > 0) {
			snprintf(dirbuf + strlen(dirbuf), sizeof(dirbuf) - strlen(dirbuf), "%s%s%dx %s", *dirbuf ? ", " : "", color, repeats+1, lastroom);
		}
		else {
			snprintf(dirbuf + strlen(dirbuf), sizeof(dirbuf) - strlen(dirbuf), "%s%s%s", *dirbuf ? ", " : "", color, lastroom);
		}
	}

	snprintf(buf, sizeof(buf), "%s: %s\t0\r\n", dirs[get_direction_for_char(ch, dir)], dirbuf);
	CAP(buf);
	msg_to_char(ch, "%s", buf);
}


/**
* Gets the full display for a single screenreader tile.
*
* @param char_data *ch The observer.
* @param room_data *origin Where the player is looking from.
* @param room_data *to_room The room to show.
* @param bool show_dark If TRUE, prepends 'Dark' to some tile names.
*/
char *screenread_one_tile(char_data *ch, room_data *origin, room_data *to_room, bool show_dark) {
	static char output[1024];
	char plrbuf[MAX_INPUT_LENGTH], infobuf[MAX_INPUT_LENGTH], multi_str[256], paint_str[256];
	int total_vehicles = 0;
	vehicle_data *show_veh;
	empire_data *emp;
	char_data *vict;
	
	// start with tile type
	strcpy(output, get_screenreader_room_name(ch, origin, to_room, show_dark));

	// show mappc
	if (SHOW_PEOPLE_IN_ROOM(to_room)) {
		*plrbuf = '\0';
		
		DL_FOREACH2(ROOM_PEOPLE(to_room), vict, next_in_room) {
			if (can_see_player_in_other_room(ch, vict)) {
				sprintf(plrbuf + strlen(plrbuf), "%s%s", *plrbuf ? ", " : "", PERS(vict, ch, FALSE));
			}
			else if (IS_NPC(vict) && size_data[GET_SIZE(vict)].show_on_map) {
				sprintf(plrbuf + strlen(plrbuf), "%s%s", *plrbuf ? ", " : "", skip_filler(PERS(vict, ch, FALSE)));
			}
		}
	
		if (*plrbuf) {
			sprintf(output + strlen(output), " <%s>", plrbuf);
		}
	}
	
	// show ships
	if ((show_veh = find_vehicle_to_show(ch, to_room, &total_vehicles))) {
		if (VEH_PAINT_COLOR(show_veh)) {
			sprinttype(VEH_PAINT_COLOR(show_veh), paint_names, paint_str, sizeof(paint_str), "painted");
			*paint_str = LOWER(*paint_str);
			strcat(paint_str, " ");
		}
		else {
			*paint_str = '\0';
		}
		
		// indicate if more vehicles can be seen
		if (total_vehicles > 1) {
			snprintf(multi_str, sizeof(multi_str), ", %+d", total_vehicles - 1);
		}
		else {
			*multi_str = '\0';
		}
		
		if (PRF_FLAGGED(ch, PRF_INFORMATIVE)) {
			get_informative_vehicle_string(ch, show_veh, infobuf);
			sprintf(output + strlen(output), " <%s%s%s%s%s>", paint_str, skip_filler(get_vehicle_short_desc(show_veh, ch)), *infobuf ? ": " :"", infobuf, multi_str);
		}
		else if (VEH_OWNER(show_veh) && !VEH_CLAIMS_WITH_ROOM(show_veh) && PRF_FLAGGED(ch, PRF_POLITICAL)) {
			sprintf(output + strlen(output), " <%s %s%s%s>", EMPIRE_ADJECTIVE(VEH_OWNER(show_veh)), paint_str, skip_filler(get_vehicle_short_desc(show_veh, ch)), multi_str);
		}
		else {
			sprintf(output + strlen(output), " <%s%s%s>", paint_str, skip_filler(get_vehicle_short_desc(show_veh, ch)), multi_str);
		}
	}
	
	// show ownership (political)
	if (PRF_FLAGGED(ch, PRF_POLITICAL) && !CHECK_CHAMELEON(origin, to_room)) {
		emp = ROOM_OWNER(to_room);
	
		if (emp) {
			sprintf(output + strlen(output), " (%s)", EMPIRE_ADJECTIVE(emp));
		}
	}

	// show status (informative)
	if (PRF_FLAGGED(ch, PRF_INFORMATIVE)) {
		get_informative_tile_string(ch, to_room, infobuf);
		if (*infobuf) {
			sprintf(output + strlen(output), " [%s]", infobuf);
		}
	}
	
	return output;
}


/**
* Main "visual" interface for the visually impaired. This lists what you can
* see in all directions.
*
* @param char_data *ch The player "looking" at the room.
* @param room_data *room What we're looking at.
* @param bitvector_t options Any LLR_x flags that get passed along.
* @param int max_dist Maximum view distance.
* @param bitvector_t only_in_dirs Optional: Filters out directions not in this list (NOBITS for all-dirs).
*/
void show_screenreader_room(char_data *ch, room_data *room, bitvector_t options, int max_dist, bitvector_t only_in_dirs) {
	int each_dir, north = get_north_for_char(ch);
	
	// static bitvector_t north_dirs = BIT(NORTH) | BIT(NORTHWEST) | BIT(NORTHEAST);
	// static bitvector_t east_dirs = BIT(EAST) | BIT(SOUTHEAST) | BIT(NORTHEAST);
	// static bitvector_t south_dirs = BIT(SOUTH) | BIT(SOUTHWEST) | BIT(SOUTHEAST);
	// static bitvector_t west_dirs = BIT(WEST) | BIT(SOUTHWEST) | BIT(NORTHWEST);
	
	// expand slightly
	if (only_in_dirs == BIT(NORTH)) {
		only_in_dirs |= BIT(NORTHEAST) | BIT(NORTHWEST);
	}
	if (only_in_dirs == BIT(SOUTH)) {
		only_in_dirs |= BIT(SOUTHEAST) | BIT(SOUTHWEST);
	}
	if (only_in_dirs == BIT(EAST)) {
		only_in_dirs |= BIT(NORTHEAST) | BIT(SOUTHEAST);
	}
	if (only_in_dirs == BIT(WEST)) {
		only_in_dirs |= BIT(NORTHWEST) | BIT(SOUTHWEST);
	}
		
	// each_dir: iterate over directions and show them in order
	for (each_dir = 0; each_dir < NUM_2D_DIRS; ++each_dir) {
		// check if directions were requested (x):
		if (only_in_dirs) {
			if (!IS_SET(only_in_dirs, BIT(each_dir))) {
				continue;
			}
			/*
			if ((each_dir == NORTH || each_dir == NORTHWEST || each_dir == NORTHEAST) && IS_SET(only_in_dirs, south_dirs) && !IS_SET(only_in_dirs, north_dirs)) {
				continue;
			}
			if ((each_dir == SOUTH || each_dir == SOUTHWEST || each_dir == SOUTHEAST) && IS_SET(only_in_dirs, north_dirs) && !IS_SET(only_in_dirs, south_dirs)) {
				continue;
			}
			if ((each_dir == EAST || each_dir == NORTHEAST || each_dir == SOUTHEAST) && IS_SET(only_in_dirs, west_dirs) && !IS_SET(only_in_dirs, east_dirs)) {
				continue;
			}
			if ((each_dir == WEST || each_dir == NORTHWEST || each_dir == SOUTHWEST) && IS_SET(only_in_dirs, east_dirs) && !IS_SET(only_in_dirs, west_dirs)) {
				continue;
			}
			*/
		}
		
		screenread_one_dir(ch, room, confused_dirs[north][0][each_dir], max_dist);
	}
}


 //////////////////////////////////////////////////////////////////////////////
//// WHERE FUNCTIONS /////////////////////////////////////////////////////////

void perform_mortal_where(char_data *ch, char *arg) {
	int closest, dist, max_distance;
	const char *dir_str;
	char_data *i, *found = NULL;
	
	if (has_player_tech(ch, PTECH_WHERE_UPGRADE)) {
		max_distance = 75;
	}
	else {
		max_distance = 20;
	}
	
	max_distance += GET_EXTRA_ATT(ch, ATT_WHERE_RANGE);
	max_distance = MAX(0, max_distance);
	
	command_lag(ch, WAIT_OTHER);

	if (!*arg) {
		msg_to_char(ch, "Players near you (%d tile%s)\r\n------------------------------\r\n", max_distance, PLURAL(max_distance));
		DL_FOREACH2(player_character_list, i, next_plr) {
			if (IS_NPC(i) || ch == i || !IN_ROOM(i))
				continue;
			if (!CAN_SEE(ch, i) || !CAN_RECOGNIZE(ch, i) || !WIZHIDE_OK(ch, i) || AFF_FLAGGED(i, AFF_NO_WHERE))
				continue;
			if ((dist = compute_distance(IN_ROOM(ch), IN_ROOM(i))) > max_distance) {
				continue;
			}
			if (!same_subzone(IN_ROOM(ch), IN_ROOM(i))) {
				continue;
			}
			if (has_player_tech(i, PTECH_NO_TRACK_WILD) && valid_no_trace(IN_ROOM(i))) {
				gain_player_tech_exp(i, PTECH_NO_TRACK_WILD, 10);
				continue;
			}
			if (has_player_tech(i, PTECH_NO_TRACK_CITY) && valid_unseen_passing(IN_ROOM(i))) {
				gain_player_tech_exp(i, PTECH_NO_TRACK_CITY, 10);
				continue;
			}
			
			// we'll only show distance if they're not on the same location
			if (GET_MAP_LOC(IN_ROOM(ch)) == GET_MAP_LOC(IN_ROOM(i))) {
				// same map location:
				msg_to_char(ch, "%-20s - %s%s\r\n", PERS(i, ch, FALSE), get_room_name(IN_ROOM(i), FALSE), (IN_ROOM(ch) == IN_ROOM(i)) ? " (here)" : "");
			}
			else {
				// not the same map location -- show distance/coords:
				dir_str = get_partial_direction_to(ch, IN_ROOM(ch), IN_ROOM(i), FALSE);
				// dist already set for us
			
				msg_to_char(ch, "%-20s -%s %s, %d tile%s %s\r\n", PERS(i, ch, FALSE), coord_display_room(ch, IN_ROOM(i), TRUE), get_room_name(IN_ROOM(i), FALSE), dist, PLURAL(dist), (*dir_str ? dir_str : "away"));
			}
			gain_player_tech_exp(ch, PTECH_WHERE_UPGRADE, 10);
		}
	}
	else {			/* print only FIRST char, not all. */
		found = NULL;
		closest = MAP_SIZE;
		DL_FOREACH(character_list, i) {
			if (i == ch || !IN_ROOM(i) || !CAN_SEE(ch, i) || AFF_FLAGGED(i, AFF_NO_WHERE)) {
				continue;
			}
			if (!IS_NPC(i) && has_player_tech(i, PTECH_NO_TRACK_WILD) && valid_no_trace(IN_ROOM(i))) {
				gain_player_tech_exp(i, PTECH_NO_TRACK_WILD, 10);
				continue;
			}
			if (!IS_NPC(i) && has_player_tech(i, PTECH_NO_TRACK_CITY) && valid_unseen_passing(IN_ROOM(i))) {
				gain_player_tech_exp(i, PTECH_NO_TRACK_CITY, 10);
				continue;
			}
			if (!same_subzone(IN_ROOM(ch), IN_ROOM(i))) {
				continue;
			}
			if ((dist = compute_distance(IN_ROOM(ch), IN_ROOM(i))) > max_distance) {
				continue;
			}
			if (!match_char_name(ch, i, arg, NOBITS, NULL)) {
				continue;
			}
			
			// trying to find closest
			if (!found || dist < closest) {
				found = i;
				closest = dist;
			}
		}

		if (found) {
			if (GET_MAP_LOC(IN_ROOM(ch)) == GET_MAP_LOC(IN_ROOM(found))) {
				// same map location:
				msg_to_char(ch, "%s - %s%s\r\n", PERS(found, ch, FALSE), get_room_name(IN_ROOM(found), FALSE), (IN_ROOM(ch) == IN_ROOM(found)) ? " (here)" : "");
			}
			else {
				// not the same map location -- show distance/coords:
				dir_str = get_partial_direction_to(ch, IN_ROOM(ch), IN_ROOM(found), FALSE);
				// distance is already set for us as 'closest'
				msg_to_char(ch, "%s -%s %s, %d tile%s %s\r\n", PERS(found, ch, FALSE), coord_display_room(ch, IN_ROOM(found), FALSE), get_room_name(IN_ROOM(found), FALSE), closest, PLURAL(closest), (*dir_str ? dir_str : "away"));
			}
			gain_player_tech_exp(ch, PTECH_WHERE_UPGRADE, 10);
			run_ability_hooks_by_player_tech(ch, PTECH_WHERE_UPGRADE, NULL, NULL, NULL, NULL);
		}
		else {
			send_to_char("No-one around by that name.\r\n", ch);
		}
	}
}


/**
* For the immortal 'where' function. This is called recursively when the obj
* is inside another obj.
*
* As of b5.179, now puts the output of this function into the character's
* page_display.
*
* @param int num Number to show for the 'where' display.
* @param obj_data *obj The object to show.
* @param char_data *ch The person viewing it (text goes to the page_display).
* @param bool recur Whether or not to recursively show contents.
*/
void print_object_location(int num, obj_data *obj, char_data *ch, bool recur) {
	struct page_display *pd;
	
	if (num > 0) {
		pd = add_page_display(ch, "O%3d. %-25s - ", num, GET_OBJ_DESC(obj, ch, OBJ_DESC_SHORT));
	}
	else {
		pd = add_page_display(ch, "%35s", " - ");
	}
	
	if (HAS_TRIGGERS(obj)) {
		append_page_display_line(pd, "[TRIG] ");
	}

	if (IN_ROOM(obj)) {
		append_page_display_line(pd, "[%d]%s %s",  GET_ROOM_VNUM(IN_ROOM(obj)), coord_display_room(ch, IN_ROOM(obj), TRUE), get_room_name(IN_ROOM(obj), FALSE));
	}
	else if (obj->carried_by) {
		append_page_display_line(pd, "carried by %s", PERS(obj->carried_by, ch, 1));
	}
	else if (obj->in_vehicle) {
		append_page_display_line(pd, "inside %s%s", get_vehicle_short_desc(obj->in_vehicle, ch), recur ? ", which is" : " ");
		if (recur) {
			add_page_display(ch, "%35s[%d]%s %s", " - ", GET_ROOM_VNUM(IN_ROOM(obj->in_vehicle)), coord_display_room(ch, IN_ROOM(obj->in_vehicle), TRUE), get_room_name(IN_ROOM(obj->in_vehicle), FALSE));
		}
	}
	else if (obj->worn_by) {
		append_page_display_line(pd, "worn by %s", PERS(obj->worn_by, ch, 1));
	}
	else if (obj->in_obj) {
		append_page_display_line(pd, "inside %s%s", GET_OBJ_DESC(obj->in_obj, ch, OBJ_DESC_SHORT), (recur ? ", which is" : " "));
		if (recur) {
			print_object_location(0, obj->in_obj, ch, recur);
		}
	}
	else {
		append_page_display_line(pd, "in an unknown location\r\n");
	}
}


void perform_immort_where(char_data *ch, char *arg) {
	int num = 0, found = 0;
	descriptor_data *d;
	vehicle_data *veh;
	char_data *i;
	obj_data *k;
	struct page_display *pd;

	if (!*arg) {
		send_to_char("Players\r\n-------\r\n", ch);
		for (d = descriptor_list; d; d = d->next) {
			if (STATE(d) == CON_PLAYING) {
				i = (d->original ? d->original : d->character);
				if (i && CAN_SEE(ch, i) && IN_ROOM(i) && WIZHIDE_OK(ch, i)) {
					if (d->original) {
						pd = add_page_display(ch, "%-20s - [%7d]%s %s (in %s)", GET_NAME(i), GET_ROOM_VNUM(IN_ROOM(i)), coord_display(ch, X_COORD(IN_ROOM(d->character)), Y_COORD(IN_ROOM(d->character)), TRUE), get_room_name(IN_ROOM(d->character), FALSE), GET_NAME(d->character));
					}
					else {
						pd = add_page_display(ch, "%-20s - [%7d]%s %s", GET_NAME(i), GET_ROOM_VNUM(IN_ROOM(i)), coord_display(ch, X_COORD(IN_ROOM(i)), Y_COORD(IN_ROOM(i)), TRUE), get_room_name(IN_ROOM(i), FALSE));
					}
					
					if (ROOM_INSTANCE(IN_ROOM(d->character))) {
						append_page_display_line(pd, " (%s)", GET_ADV_NAME(INST_ADVENTURE(ROOM_INSTANCE(IN_ROOM(d->character)))));
					}
				}
			}
		}
		send_page_display(ch);
	}
	else {
		DL_FOREACH(character_list, i) {
			if (CAN_SEE(ch, i) && IN_ROOM(i) && WIZHIDE_OK(ch, i) && (multi_isname(arg, GET_PC_NAME(i)) || match_char_name(ch, i, arg, MATCH_GLOBAL, NULL))) {
				found = 1;
				add_page_display(ch, "M%3d. %-25s - %s[%7d]%s %s\r\n", ++num, GET_NAME(i), (IS_NPC(i) && HAS_TRIGGERS(i)) ? "[TRIG] " : "", GET_ROOM_VNUM(IN_ROOM(i)), coord_display(ch, X_COORD(IN_ROOM(i)), Y_COORD(IN_ROOM(i)), TRUE), get_room_name(IN_ROOM(i), FALSE));
			}
		}
		DL_FOREACH(vehicle_list, veh) {
			if (CAN_SEE_VEHICLE(ch, veh) && multi_isname(arg, VEH_KEYWORDS(veh))) {
				found = 1;
				add_page_display(ch, "V%3d. %-25s - %s[%7d]%s %s\r\n", ++num, VEH_SHORT_DESC(veh), (HAS_TRIGGERS(veh) ? "[TRIG] " : ""), GET_ROOM_VNUM(IN_ROOM(veh)), coord_display(ch, X_COORD(IN_ROOM(veh)), Y_COORD(IN_ROOM(veh)), TRUE), get_room_name(IN_ROOM(veh), FALSE));
			}
		}
		DL_FOREACH(object_list, k) {
			if (CAN_SEE_OBJ(ch, k) && multi_isname(arg, GET_OBJ_KEYWORDS(k))) {
				found = 1;
				print_object_location(++num, k, ch, TRUE);
			}
		}
		if (found) {
			send_page_display(ch);
		}
		else {
			send_to_char("Couldn't find any such thing.\r\n", ch);
		}
	}
}


 //////////////////////////////////////////////////////////////////////////////
//// COMMANDS ////////////////////////////////////////////////////////////////

// with cmd == -1, this suppresses extra exits
ACMD(do_exits) {
	char buf[MAX_STRING_LENGTH], buf2[MAX_STRING_LENGTH];
	struct room_direction_data *ex;
	room_data *room, *to_room;
	vehicle_data *veh;
	obj_data *obj;
	size_t size;
	bool any;
	int dir;

	if (subcmd == -1) {
		room = IN_ROOM(ch);
	}
	else {
		room = real_room(subcmd);
	}
	
	// using buf for the output
	*buf = '\0';
	size = 0;

	if (AFF_FLAGGED(ch, AFF_BLIND)) {
		msg_to_char(ch, "You can't see a damned thing, you're blind!\r\n");
		return;
	}
	if (COMPLEX_DATA(room) && ROOM_IS_CLOSED(room)) {
		for (ex = COMPLEX_DATA(room)->exits; ex; ex = ex->next) {
			if ((to_room = ex->room_ptr) && !EXIT_FLAGGED(ex, EX_CLOSED)) {
				snprintf(buf2, sizeof(buf2), "%s%s\r\n", (cmd != -1 ? " " : ""), CAP(exit_description(ch, to_room, dirs[get_direction_for_char(ch, ex->dir)])));
				if (size + strlen(buf2) < sizeof(buf)) {
					strcat(buf, buf2);
					size += strlen(buf2);
				}
			}
		}
		
		// can disembark/exit here?
		if (ROOM_CAN_EXIT(room)) {
			// 'disembark'
			if ((veh = GET_ROOM_VEHICLE(room)) && IN_ROOM(veh) && !VEH_FLAGGED(veh, VEH_BUILDING)) {
				size += snprintf(buf + size, sizeof(buf) - size, "%s%s\r\n", (cmd != -1 ? " " : ""), exit_description(ch, IN_ROOM(veh), "Disembark"));
			}
			// 'exit'
			else if ((to_room = get_exit_room(room)) && to_room != room) {
				size += snprintf(buf + size, sizeof(buf) - size, "%s%s\r\n", (cmd != -1 ? " " : ""), exit_description(ch, to_room, "Exit"));
			}
		}
	}
	else {	// out on the map
		for (dir = 0; dir < NUM_2D_DIRS; ++dir) {
			if (!(to_room = real_shift(room, shift_dir[dir][0], shift_dir[dir][1]))) {
				continue;	// no way to go that dir
			}
			if (ROOM_IS_CLOSED(to_room) && BUILDING_ENTRANCE(to_room) != dir && (!ROOM_BLD_FLAGGED(to_room, BLD_TWO_ENTRANCES) || BUILDING_ENTRANCE(to_room) != rev_dir[dir])) {
				continue;	// building blocking
			}
			
			// append
			snprintf(buf2, sizeof(buf2), "%s%s\r\n", (cmd != -1 ? " " : ""), CAP(exit_description(ch, to_room, dirs[get_direction_for_char(ch, dir)])));
			if (size + strlen(buf2) < sizeof(buf)) {
				strcat(buf, buf2);
				size += strlen(buf2);
			}
		}
	}
	
	msg_to_char(ch, "Obvious exits:\r\n%s", *buf ? buf : " None.\r\n");
	
	// portals
	if (cmd != -1) {
		any = FALSE;
		DL_FOREACH2(ROOM_CONTENTS(room), obj, next_content) {
			if (!IS_PORTAL(obj) || !(to_room = real_room(GET_PORTAL_TARGET_VNUM(obj)))) {
				continue;
			}
			
			// display
			if (!any) {
				msg_to_char(ch, "Portals:\r\n");
				any = TRUE;
			}
			msg_to_char(ch, " %s\r\n", CAP(exit_description(ch, to_room, skip_filler(GET_OBJ_SHORT_DESC(obj)))));
		}
	}
}


ACMD(do_mapscan) {
	room_data *use_room = (GET_MAP_LOC(IN_ROOM(ch)) ? real_room(GET_MAP_LOC(IN_ROOM(ch))->vnum) : NULL);
	int dir, dist, last_isle;
	room_data *to_room;
	bool any, show_obscured;
	
	int max_dist = MIN(MAP_WIDTH, MAP_HEIGHT) / 2;
	
	skip_spaces(&argument);
	
	if (IS_NPC(ch) || !HAS_NAVIGATION(ch)) {
		msg_to_char(ch, "You don't have the right ability to use mapscan (e.g. Navigation).\r\n");
	}
	else if (!*argument) {
		msg_to_char(ch, "Scan the map in which direction?\r\n");
	}
	else if (!use_room || IS_ADVENTURE_ROOM(use_room) || ROOM_IS_CLOSED(use_room)) {	// check map room
		msg_to_char(ch, "You can only use mapscan out on the map.\r\n");
	}
	else if ((!GET_ROOM_VEHICLE(IN_ROOM(ch)) || !CAN_LOOK_OUT(IN_ROOM(ch))) && (IS_ADVENTURE_ROOM(IN_ROOM(ch)) || ROOM_IS_CLOSED(IN_ROOM(ch)))) {
		msg_to_char(ch, "Scan only works out on the map.\r\n");
	}
	else if ((dir = parse_direction(ch, argument)) == NO_DIR) {
		msg_to_char(ch, "Invalid direction '%s'.\r\n", argument);
	}
	else if (dir >= NUM_2D_DIRS) {
		msg_to_char(ch, "You can't scan that way.\r\n");
	}
	else {	// success
		msg_to_char(ch, "You scan the map to the %s and see:\r\n", dirs[dir]);
		
		last_isle = GET_ISLAND_ID(use_room);
		any = FALSE;
		show_obscured = (GET_ISLAND_ID(IN_ROOM(ch)) != NO_ISLAND) ? TRUE : FALSE;	// if they're on an island, we will look for a vision-obscuring tile
		
		for (dist = 1; dist <= max_dist; dist += ((show_obscured || dist < 10) ? 1 : (dist < 70 ? 5 : 10))) {
			if (!(to_room = real_shift(use_room, shift_dir[dir][0] * dist, shift_dir[dir][1] * dist))) {
				break;
			}
			if (show_obscured && ROOM_SECT_FLAGGED(to_room, SECTF_OBSCURE_VISION)) {
				// we will show the first obscuring tile on the same island
				
				msg_to_char(ch, " %d %s: %s\r\n", dist, (PRF_FLAGGED(ch, PRF_SCREEN_READER) ? dirs[dir] : alt_dirs[dir]), last_isle == NO_ISLAND ? "The Ocean" : GET_SECT_NAME(SECT(to_room)));
				any = TRUE;
				show_obscured = FALSE;
				// don't continue -- fall through (but will likely hit the next continue for: same isle)
			}
			if (GET_ISLAND_ID(to_room) == last_isle) {
				continue;	// only look for changes
			}
			
			// got this far?
			last_isle = GET_ISLAND_ID(to_room);
			show_obscured = FALSE;	// don't show an obscuring tile if we switched islands
			msg_to_char(ch, " %d %s: %s\r\n", dist, (PRF_FLAGGED(ch, PRF_SCREEN_READER) ? dirs[dir] : alt_dirs[dir]), last_isle == NO_ISLAND ? "The Ocean" : get_island_name_for(last_isle, ch));
			any = TRUE;
		}
		
		if (!any) {
			msg_to_char(ch, " %s as far as you can see.\r\n", last_isle == NO_ISLAND ? "The Ocean" : get_island_name_for(last_isle, ch));
		}
		
		command_lag(ch, WAIT_OTHER);
	}
}


ACMD(do_scan) {
	bool dash_distance = FALSE, plus_distance = FALSE;
	char arg[MAX_INPUT_LENGTH], new_arg[MAX_INPUT_LENGTH];
	bitvector_t dir_modifiers = NOBITS;
	int dir, dist = -1, val;
	
	room_data *use_room = (GET_MAP_LOC(IN_ROOM(ch)) ? real_room(GET_MAP_LOC(IN_ROOM(ch))->vnum) : NULL);
	
	// this will hold the final argument
	*new_arg = '\0';
	
	// parse argument for optional modifiers
	while (*argument) {
		argument = any_one_word(argument, arg);
		
		if (*arg == '-' && *(arg+1)) {
			// -dir modifier (or maybe -dist)
			if (isdigit(*(arg+1))) {
				dist = atoi(arg+1);
				dash_distance = TRUE;	// distance with a dash
			}
			else if ((val = parse_direction(ch, arg+1)) == NO_DIR) {
				msg_to_char(ch, "Invalid direction modifier '%s'.\r\n", arg+1);
				return;
			}
			else if (val >= NUM_2D_DIRS) {
				msg_to_char(ch, "You can't scan that way.\r\n");
				return;
			}
			else {
				// accept direction modifier
				dir_modifiers |= BIT(val);
			}
		}
		else if (*arg == '+' && *(arg+1)) {
			// +dist
			if (isdigit(*(arg+1))) {
				dist = atoi(arg+1);
				plus_distance = TRUE;	// distance with a plus
			}
		}
		else if (isdigit(*arg)) {
			dist = atoi(arg);
		}
		else {
			// anything else, we keep
			snprintf(new_arg + strlen(new_arg), sizeof(new_arg) - strlen(new_arg), "%s%s", *new_arg ? " " : "", arg);
		}
	}
	
	// basic checks
	if (AFF_FLAGGED(ch, AFF_BLIND)) {
		msg_to_char(ch, "You can't see a damned thing, you're blind!\r\n");
	}
	else if (!use_room || IS_ADVENTURE_ROOM(use_room) || ROOM_IS_CLOSED(use_room)) {	// check map room
		msg_to_char(ch, "You can only use scan out on the map.\r\n");
	}
	else if ((!GET_ROOM_VEHICLE(IN_ROOM(ch)) || !CAN_LOOK_OUT(IN_ROOM(ch))) && (IS_ADVENTURE_ROOM(IN_ROOM(ch)) || ROOM_IS_CLOSED(IN_ROOM(ch)))) {
		msg_to_char(ch, "Scan only works out on the map.\r\n");
	}
	else if (dist == 0 || dist > config_get_int("max_map_size")) {
		// IF they specified a distance
		msg_to_char(ch, "Distance must be between 1 and %d.\r\n", config_get_int("max_map_size"));
	}
	else if (!*new_arg && dist == -1 && dir_modifiers == NOBITS) {
		msg_to_char(ch, "Scan which direction or for what type of tile?\r\n");
	}
	else if (!*new_arg && (dist >= 0 || dir_modifiers) && !dash_distance && !plus_distance) {
		// normal 'screenreader look' scan with a custom distance
		show_screenreader_room(ch, use_room, NOBITS, (dist != -1) ? dist : GET_MAPSIZE(ch), dir_modifiers);
	}
	else if ((dir = parse_direction(ch, new_arg)) == NO_DIR || (dist >= 0 && (dash_distance || plus_distance))) {
		// scanning by tile name
		clear_recent_moves(ch);
		scan_for_tile(ch, new_arg, (dist != -1) ? dist : GET_MAPSIZE(ch), dir_modifiers, dash_distance);
		gain_player_tech_exp(ch, PTECH_MAP_MEMORY, 0.1);
	}
	else if (dir >= NUM_2D_DIRS) {
		// found a dir but not a valid one
		msg_to_char(ch, "You can't scan that way.\r\n");
	}
	else {
		// valid dir: scan in one line
		clear_recent_moves(ch);
		screenread_one_dir(ch, use_room, dir, (dist != -1) ? dist : GET_MAPSIZE(ch));
		gain_player_tech_exp(ch, PTECH_MAP_MEMORY, 0.1);
	}
}


ACMD(do_where) {
	skip_spaces(&argument);

	if (GET_ACCESS_LEVEL(ch) >= LVL_GOD) {
		perform_immort_where(ch, argument);
	}
	else {
		perform_mortal_where(ch, argument);
	}
}
