/* ************************************************************************
*   File: mapview.c                                       EmpireMUD 2.0b5 *
*  Usage: Map display functions                                           *
*                                                                         *
*  EmpireMUD code base by Paul Clarke, (C) 2000-2015                      *
*  All rights reserved.  See license.doc for complete information.        *
*                                                                         *
*  EmpireMUD based upon CircleMUD 3.0, bpl 17, by Jeremy Elson.           *
*  CircleMUD (C) 1993, 94 by the Trustees of the Johns Hopkins University *
*  CircleMUD is based on DikuMUD, Copyright (C) 1990, 1991.               *
************************************************************************ */

#include <math.h>

#include "conf.h"
#include "sysdep.h"

#include "structs.h"
#include "utils.h"
#include "comm.h"
#include "interpreter.h"
#include "handler.h"
#include "db.h"
#include "utils.h"
#include "skills.h"
#include "vnums.h"
#include "dg_scripts.h"

/**
* Contents:
*   Data
*   Helpers
*   Mappc Functions
*   Map View Functions
*   Screen Reader Functions
*   Where Functions
*   Commands
*/

// external vars
extern const char *paint_colors[];
extern const char *paint_names[];
extern const int rev_dir[];
extern struct character_size_data size_data[];

// external functions
extern char *get_room_name(room_data *room, bool color);
extern char *get_vehicle_short_desc(vehicle_data *veh, char_data *to);
extern vehicle_data *find_vehicle_to_show(char_data *ch, room_data *room);


 //////////////////////////////////////////////////////////////////////////////
//// DATA ////////////////////////////////////////////////////////////////////

#define ANY_ROAD_TYPE(tile)  (IS_ROAD(tile) || BUILDING_VNUM(tile) == BUILDING_BRIDGE || BUILDING_VNUM(tile) == BUILDING_SWAMPWALK || BUILDING_VNUM(tile) == BUILDING_BOARDWALK)
#define CONNECTS_TO_ROAD(tile)  (tile && (ANY_ROAD_TYPE(tile) || ROOM_BLD_FLAGGED(tile, BLD_ATTACH_ROAD)))
#define IS_BARRIER(tile)  (BUILDING_VNUM(tile) == BUILDING_WALL || BUILDING_VNUM(tile) == BUILDING_FENCE || BUILDING_VNUM(tile) == BUILDING_GATE || BUILDING_VNUM(tile) == BUILDING_GATEHOUSE)


// for showing map pcs
struct mappc_data {
	room_data *room;
	char_data *character;
	
	struct mappc_data *next;
};

struct mappc_data_container {
	struct mappc_data *data;
};


// external vars
extern const int confused_dirs[NUM_2D_DIRS][2][NUM_OF_DIRS];
extern const char *dirs[];
extern const char *orchard_commands;

// external funcs
void replace_question_color(char *input, char *color, char *output);

// locals
ACMD(do_exits);
static void show_map_to_char(char_data *ch, struct mappc_data_container *mappc, room_data *to_room, bitvector_t options);


 //////////////////////////////////////////////////////////////////////////////
//// HELPERS /////////////////////////////////////////////////////////////////


/**
* @param room_data *room The room to check.
* @return bool TRUE if any adjacent room is light; otherwise FALSE.
*/
bool adjacent_room_is_light(room_data *room) {
	room_data *to_room;
	int i;
	
	// adventure rooms don't bother
	if (IS_ADVENTURE_ROOM(room)) {
		return FALSE;
	}

	for (i = 0; i < NUM_SIMPLE_DIRS; i++) {
		to_room = real_shift(room, shift_dir[i][0], shift_dir[i][1]);
		if (to_room && IS_REAL_LIGHT(to_room)) {
			return TRUE;
		}
	}
	
	return FALSE;
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
	if (!CAN_SEE_IN_DARK_ROOM(ch, room)) {
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
	if (ROOM_CUSTOM_NAME(room)) {
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
* @param empire_data *emp
* @return char* The background color that's a good fit for the banner color
*/
const char *get_banner_complement_color(empire_data *emp) {
	if (!emp) {
		return "";
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
* Gets basic info about the tile to be shown on 'toggle informative' displays.
*
* @param char_data *ch Optional: The person looking (for immortal-only data).
* @param room_data *room The location to check.
* @param char *buffer A string to store the result to.
*/
void get_informative_tile_string(char_data *ch, room_data *room, char *buffer) {
	*buffer = '\0';

	if (IS_DISMANTLING(room)) {
		sprintf(buffer + strlen(buffer), "%sdismantling", *buffer ? ", " : "");
	}
	else if (!IS_COMPLETE(room)) {
		sprintf(buffer + strlen(buffer), "%sunfinished", *buffer ? ", " :"");
	}
	if (HAS_MAJOR_DISREPAIR(room)) {
		sprintf(buffer + strlen(buffer), "%sbad disrepair", *buffer ? ", " :"");
	}
	else if (HAS_MINOR_DISREPAIR(room)) {
		sprintf(buffer + strlen(buffer), "%sdisrepair", *buffer ? ", " :"");
	}
	if (IS_COMPLETE(room) && room_has_function_and_city_ok(room, FNC_MINE)) {
		if (get_room_extra_data(room, ROOM_EXTRA_MINE_AMOUNT) > 0) {
			sprintf(buffer + strlen(buffer), "%shas ore", *buffer ? ", " :"");
		}
		else {
			sprintf(buffer + strlen(buffer), "%sdepleted", *buffer ? ", " :"");
		}
	}
	if (ROOM_AFF_FLAGGED(room, ROOM_AFF_NO_WORK)) {
		sprintf(buffer + strlen(buffer), "%sno-work", *buffer ? ", " :"");
	}
	if (ch && IS_IMMORTAL(ch) && ROOM_AFF_FLAGGED(room, ROOM_AFF_CHAMELEON) && IS_COMPLETE(room)) {
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
	extern int count_recent_moves(char_data *ch);
	
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
	player_index_data *index;
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

	/* Custom names by type */
	else if (IS_ROAD(room) && SECT_FLAGGED(BASE_SECT(room), SECTF_ROUGH)) {
		strcat(name, "A Winding Path");
	}

	// patron monuments
	else if (GET_BUILDING(room) && ROOM_PATRON(room) > 0) {
		strcat(name, GET_BLD_TITLE(GET_BUILDING(room)));
		sprintf(name + strlen(name), " of %s", (index = find_player_index_by_idnum(ROOM_PATRON(room))) ? index->fullname : "a Former God");
	}

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
* determines which tileset to use for sector color...
*
* This function is designed to do 3 things using a lot of math:
*  1. Past the arctic line, always Winter.
*  2. Between the tropics, alternate Spring, Summer, Autumn (no Winter).
*  3. Everywhere else, seasons are based on time of year, and gradually move
*     North/South each day. There should always be a boundary between the
*     Summer and Winter regions (thus all the squirrelly numbers).
*
* A near-copy of this function exists in util/evolve.c for map evolutions.
*
* @param room_data *room The room to get a season for.
* @return int TILESET_ const for the chosen season.
*/
int pick_season(room_data *room) {
	int ycoord = Y_COORD(room), y_max, y_arctic, y_tropics, half_y, day_of_year;
	double arctic = config_get_double("arctic_percent") / 200.0;	// split in half and convert from XX.XX to .XXXX (percent)
	double tropics = config_get_double("tropics_percent") / 200.0;
	bool northern = (ycoord >= MAP_HEIGHT/2);
	double a_slope, b_slope;
	
	// month 0 is january; year is 0-359 days
	
	// tropics? -- take half the tropic value, convert to percent, multiply by map height
	if (ycoord >= round(MAP_HEIGHT/2.0) - (tropics * MAP_HEIGHT) && ycoord <= round(MAP_HEIGHT/2.0) + (tropics * MAP_HEIGHT)) {
		if (time_info.month < 2) {
			return TILESET_SPRING;
		}
		else if (time_info.month >= 10) {
			return TILESET_AUTUMN;
		}
		else {
			return TILESET_SUMMER;
		}
	}
	
	// arctic? -- take half the arctic value, convert to percent, check map edges
	if (ycoord <= (arctic * MAP_HEIGHT) || ycoord >= (MAP_HEIGHT - (arctic * MAP_HEIGHT))) {
		return TILESET_WINTER;
	}
	
	// all other regions: first split the map in half (we'll invert for the south)
	y_max = round(MAP_HEIGHT / 2.0);
	day_of_year = time_info.month * 30 + time_info.day;
	
	if (northern) {
		y_arctic = round(y_max - (config_get_double("arctic_percent") * y_max / 100));
		y_tropics = round(config_get_double("tropics_percent") * y_max / 100);
		a_slope = ((y_arctic - 1) - (y_tropics + 1)) / 120.0;	// basic slope of the seasonal gradient
		b_slope = ((y_arctic - 1) - (y_tropics + 1)) / 90.0;
		half_y = ABSOLUTE(ycoord - y_max) - y_tropics; // simplify by moving the y axis to match the tropics line
	}
	else {
		y_arctic = round(config_get_double("arctic_percent") * y_max / 100);
		y_tropics = round(y_max - (config_get_double("tropics_percent") * y_max / 100));
		a_slope = ((y_tropics - 1) - (y_arctic + 1)) / 120.0;	// basic slope of the seasonal gradient
		b_slope = ((y_tropics - 1) - (y_arctic + 1)) / 90.0;
		half_y = ycoord - y_arctic;	// adjust to remove arctic
	}
	
	if (day_of_year < 6 * 30) {	// first half of year
		if (half_y >= round((day_of_year + 1) * a_slope)) {	// first winter line
			return northern ? TILESET_WINTER : TILESET_SUMMER;
		}
		else if (half_y >= round((day_of_year - 89) * b_slope)) {	// spring line
			return northern ? TILESET_SPRING : TILESET_AUTUMN;
		}
		else {
			return northern ? TILESET_SUMMER : TILESET_WINTER;
		}
	}
	else {	// 2nd half of year
		if (half_y >= round((day_of_year - 360) * -a_slope)) {	// second winter line
			return northern ? TILESET_WINTER : TILESET_SUMMER;
		}
		else if (half_y >= round((day_of_year - 268) * -b_slope)) {	// autumn line
			return northern ? TILESET_AUTUMN : TILESET_SPRING;
		}
		else {
			return northern ? TILESET_SUMMER : TILESET_WINTER;
		}
	}
	
	// fail? we should never reach this
	return TILESET_SUMMER;
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


bool show_pc_in_room(char_data *ch, room_data *room, struct mappc_data_container *mappc) {
	struct mappc_data *pc, *pc_iter, *start_this_room = NULL;
	bool show_mob = FALSE;
	char lbuf[60];
	char_data *c;
	empire_data *emp;
	int count = 0, iter;
	char *charmap = "<oo>";

	if (!SHOW_PEOPLE_IN_ROOM(room)) {
		return FALSE;
	}

	/* Hidden people are left off the map, even if you sense life */
	for (c = ROOM_PEOPLE(room); c; c = c->next_in_room) {
		if (can_see_player_in_other_room(ch, c)) {
			CREATE(pc, struct mappc_data, 1);
			pc->room = room;
			pc->character = c;
			pc->next = NULL;
	
			if (mappc->data) {
				// append to end
				pc_iter = mappc->data;
				while (pc_iter->next) {
					pc_iter = pc_iter->next;
				}
				pc_iter->next = pc;
			}
			else {
				mappc->data = pc;
			}
	
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
		msg_to_char(ch, "&0<%soo&0>", !emp ? "" : EMPIRE_BANNER(emp));
	}
	else if (count == 2) {
		pc = start_this_room;
		emp = GET_LOYALTY(pc->character);
		sprintf(lbuf, "&0<%so", !emp ? "&0" : EMPIRE_BANNER(emp));
		
		pc = pc->next;
		emp = GET_LOYALTY(pc->character);
		sprintf(lbuf + strlen(lbuf), "%so&0>", !emp ? "" : EMPIRE_BANNER(emp));
		
		send_to_char(lbuf, ch);
	}
	else if (count == 3) {
		pc = start_this_room;
		emp = GET_LOYALTY(pc->character);
		sprintf(lbuf, "%s<", !emp ? "&0" : EMPIRE_BANNER(emp));
		
		pc = pc->next;
		emp = GET_LOYALTY(pc->character);
		sprintf(lbuf + strlen(lbuf), "%soo", !emp ? "" : EMPIRE_BANNER(emp));
		
		pc = pc->next;
		emp = GET_LOYALTY(pc->character);
		sprintf(lbuf + strlen(lbuf), "%s>", !emp ? "" : EMPIRE_BANNER(emp));
		
		send_to_char(lbuf, ch);
	}
	else if (count >= 4) {
		pc = start_this_room;
		*lbuf = '\0';
		
		// only show the first 4 colors in this case
		for (iter = 0; iter < 4; ++iter) {
			emp = GET_LOYALTY(pc->character);
			sprintf(lbuf + strlen(lbuf), "%s%c", !emp ? "&0" : EMPIRE_BANNER(emp), charmap[iter]);
			pc = pc->next;
		}
		
		send_to_char(lbuf, ch);
	}
	else if (show_mob) {
		send_to_char("&0(oo)", ch);
	}
	
	// if we got here we showed a pc
	return TRUE;
}


 //////////////////////////////////////////////////////////////////////////////
//// MAP VIEW FUNCTIONS //////////////////////////////////////////////////////

/**
* Does a normal "look" at a room.
*
* @param char_data *ch The person doing the looking.
* @param room_data *room The room to look at.
* @param bitvector_t options LRR_x flags.
*/
void look_at_room_by_loc(char_data *ch, room_data *room, bitvector_t options) {
	extern bool can_get_quest_from_room(char_data *ch, room_data *room, struct quest_temp_list **build_list);
	extern bool can_turn_quest_in_to_room(char_data *ch, room_data *room, struct quest_temp_list **build_list);
	extern const char *color_by_difficulty(char_data *ch, int level);
	extern struct instance_data *find_instance_by_room(room_data *room, bool check_homeroom, bool allow_fake_loc);
	void show_screenreader_room(char_data *ch, room_data *room, bitvector_t options);
	void list_obj_to_char(obj_data *list, char_data *ch, int mode, int show);
	void list_char_to_char(char_data *list, char_data *ch);
	void list_vehicles_to_char(vehicle_data *list, char_data *ch);
	extern const struct tavern_data_type tavern_data[];
	extern int how_to_show_map[NUM_SIMPLE_DIRS][2];
	extern int show_map_y_first[NUM_SIMPLE_DIRS];
	extern byte distance_can_see(char_data *ch);
	extern struct city_metadata_type city_type[];
	extern const char *bld_flags[];
	extern const char *room_aff_bits[];
	extern const char *room_template_flags[];

	struct mappc_data_container *mappc = NULL;
	struct mappc_data *pc, *next_pc;
	struct empire_city_data *city;
	char output[MAX_STRING_LENGTH], veh_buf[256], col_buf[256], flagbuf[MAX_STRING_LENGTH], locbuf[128], partialbuf[MAX_STRING_LENGTH], rlbuf[MAX_STRING_LENGTH], tmpbuf[MAX_STRING_LENGTH], advcolbuf[128];
	char room_name_color[MAX_STRING_LENGTH];
	int s, t, mapsize, iter, check_x, check_y, level, ter_type;
	int first_iter, second_iter, xx, yy, magnitude, north;
	int first_start, first_end, second_start, second_end, temp;
	bool y_first, invert_x, invert_y, comma, junk;
	struct instance_data *inst;
	player_index_data *index;
	room_data *to_room;
	empire_data *emp, *pcemp;
	crop_data *cp;
	char *strptr;
	
	// configs
	int trench_initial_value = config_get_int("trench_initial_value");
	
	// options
	bool ship_partial = IS_SET(options, LRR_SHIP_PARTIAL) ? TRUE : FALSE;
	bool look_out = IS_SET(options, LRR_LOOK_OUT) ? TRUE : FALSE;
	bool has_ship = GET_ROOM_VEHICLE(IN_ROOM(ch)) ? TRUE : FALSE;
	bool show_on_ship = has_ship && ROOM_BLD_FLAGGED(IN_ROOM(ch), BLD_LOOK_OUT);
	bool show_title = !show_on_ship || ship_partial || look_out;

	// begin with the sanity check
	if (!ch || !ch->desc)
		return;

	mapsize = get_map_radius(ch);

	if (AFF_FLAGGED(ch, AFF_BLIND)) {
		msg_to_char(ch, "You see nothing but infinite darkness...\r\n");
		return;
	}

	if (!look_out && AFF_FLAGGED(ch, AFF_EARTHMELD) && IS_ANY_BUILDING(IN_ROOM(ch)) && !ROOM_BLD_FLAGGED(IN_ROOM(ch), BLD_OPEN)) {
		msg_to_char(ch, "You are beneath a building.\r\n");
		return;
	}

	// check for ship
	if (!look_out && !ship_partial && show_on_ship) {
		look_at_room_by_loc(ch, IN_ROOM(GET_ROOM_VEHICLE(IN_ROOM(ch))), LRR_SHIP_PARTIAL);
	}

	// mappc setup
	CREATE(mappc, struct mappc_data_container, 1);
	
	// BASE: room name
	strcpy(room_name_color, get_room_name(room, TRUE));
	
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
	if (ROOM_CUSTOM_NAME(room) && !str_str(room_name_color, GET_BUILDING(room) ? GET_BLD_NAME(GET_BUILDING(room)) : GET_SECT_NAME(SECT(room)))) {
		sprintf(rlbuf, " (%s)", GET_BUILDING(room) ? GET_BLD_NAME(GET_BUILDING(room)) : GET_SECT_NAME(SECT(room)));
	}
	
	// coloring for adventures
	*advcolbuf = '\0';
	if ((GET_ROOM_TEMPLATE(room) || ROOM_AFF_FLAGGED(room, ROOM_AFF_TEMPORARY)) && (inst = find_instance_by_room(room, FALSE, FALSE))) {
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
		
		if (MAGIC_DARKNESS(room) && !CAN_SEE_IN_DARK_ROOM(ch, room)) {
			// no title
			send_to_char("It is pitch black...\r\n", ch);
		}
		else if (PRF_FLAGGED(ch, PRF_SCREEN_READER)) {
			if (show_title) {
				send_to_char(output, ch);
			}
			show_screenreader_room(ch, room, options);
		}
		else {	// normal map view
			magnitude = PRF_FLAGGED(ch, PRF_BRIEF) ? 3 : mapsize;
			*buf = '\0';
			
			if (show_title) {
				// spacing to center the title
				s = ((4 * (magnitude * 2 + 1)) + 2 - (strlen(output)-4))/2;
				if (!PRF_FLAGGED(ch, PRF_BRIEF))
					for (t = 1; t <= s; t++)
						send_to_char(" ", ch);
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
			
			// map edges?
			if (!WRAP_Y) {
				if (Y_COORD(room) < magnitude) {
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
		
			// which iter is x/y depends on which way is north!
			for (first_iter = first_start; first_iter >= -first_end; --first_iter) {
				msg_to_char(ch, "|");
			
				for (second_iter = second_start; second_iter >= -second_end; --second_iter) {
					xx = (y_first ? second_iter : first_iter) * (invert_x ? -1 : 1);
					yy = (y_first ? first_iter : second_iter) * (invert_y ? -1 : 1);
				
					to_room = real_shift(room, xx, yy);
				
					if (!to_room) {
						// nothing to show?
						send_to_char("    ", ch);
					}
					else if (to_room != room && ROOM_AFF_FLAGGED(to_room, ROOM_AFF_DARK)) {
						// magic dark
						send_to_char("    ", ch);
					}
					else if (to_room != room && !CAN_SEE_IN_DARK_ROOM(ch, to_room) && compute_distance(room, to_room) > distance_can_see(ch) && !adjacent_room_is_light(to_room)) {
						// normal dark
						if (!PRF_FLAGGED(ch, PRF_NOMAPCOL)) {
							show_map_to_char(ch, mappc, to_room, options | LRR_SHOW_DARK);
						}
						else {
							send_to_char("    ", ch);
						}
					}
					else {
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
		}

		// notify character they can't see in the dark
		if (!CAN_SEE_IN_DARK_ROOM(ch, room)) {
			msg_to_char(ch, "It's dark and you're having trouble seeing items and people.\r\n");
		}
	}
	else {
		// show room: non-map
		if (!CAN_SEE_IN_DARK_ROOM(ch, room)) {
			send_to_char("It is pitch black...\r\n", ch);
		}
		else {
			if (show_title) {
				send_to_char(output, ch);
			}
			
			// description
			if (!PRF_FLAGGED(ch, PRF_BRIEF) && (strptr = get_room_description(room))) {
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
	
	// ship-partial ends here
	if (ship_partial) {
		return;
	}
	
	if (look_out) {
		// nothing else to show on a look-out
		return;
	}
	
	// commands: only show if the first entry is not a \0, which terminates the list
	if (GET_BUILDING(room)) {
		if (GET_BLD_COMMANDS(GET_BUILDING(room)) && *GET_BLD_COMMANDS(GET_BUILDING(room))) {
			msg_to_char(ch, "Commands: &c%s&0\r\n", GET_BLD_COMMANDS(GET_BUILDING(room)));
		}
	}
	else if (ROOM_SECT_FLAGGED(room, SECTF_CROP) && ROOM_CROP_FLAGGED(room, CROPF_IS_ORCHARD)) {
		msg_to_char(ch, "Commands: &c%s&0\r\n", orchard_commands);
	}
	else if (GET_SECT_COMMANDS(SECT(room)) && *GET_SECT_COMMANDS(SECT(room))) {
		msg_to_char(ch, "Commands: &c%s&0\r\n", GET_SECT_COMMANDS(SECT(room)));
	}
	
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
		if ((ter_type = get_territory_type_for_empire(room, emp, FALSE, &junk)) == TER_CITY && (city = find_closest_city(emp, room))) {
			msg_to_char(ch, "This is the %s%s&0 %s of %s.", EMPIRE_BANNER(emp), EMPIRE_ADJECTIVE(emp), city_type[city->type].name, city->name);
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
		strcpy(col_buf, paint_names[ROOM_PAINT_COLOR(room)]);
		*col_buf = LOWER(*col_buf);
		msg_to_char(ch, "The building has been painted %s%s.\r\n", (ROOM_AFF_FLAGGED(room, ROOM_AFF_BRIGHT_PAINT) ? "bright " : ""), col_buf);
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
	
	if (room_has_function_and_city_ok(room, FNC_TAVERN) && IS_COMPLETE(room)) {
		msg_to_char(ch, "The tavern has %s on tap.\r\n", tavern_data[get_room_extra_data(room, ROOM_EXTRA_TAVERN_TYPE)].name);
	}

	if (room_has_function_and_city_ok(room, FNC_MINE) && IS_COMPLETE(room)) {
		if (get_room_extra_data(room, ROOM_EXTRA_MINE_AMOUNT) <= 0) {
			msg_to_char(ch, "This mine is depleted.\r\n");
		}
		else {
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

	if (!AFF_FLAGGED(ch, AFF_EARTHMELD)) {
		if (can_get_quest_from_room(ch, room, NULL)) {
			msg_to_char(ch, "\tA...there is a quest here for you!\t0\r\n");
		}
		if (can_turn_quest_in_to_room(ch, room, NULL)) {
			msg_to_char(ch, "\tA...you can turn in a quest here!\t0\r\n");
		}
	
		/* now list characters & objects */
		send_to_char("&g", ch);
		list_obj_to_char(ROOM_CONTENTS(room), ch, OBJ_DESC_LONG, FALSE);
		send_to_char("&w", ch);
		list_vehicles_to_char(ROOM_VEHICLES(room), ch);
		send_to_char("&y", ch);
		list_char_to_char(ROOM_PEOPLE(room), ch);
		send_to_char("&0", ch);
	}

	/* Exits ? */
	if (COMPLEX_DATA(room) && ROOM_IS_CLOSED(room)) {
		do_exits(ch, "", -1, GET_ROOM_VNUM(room));
	}
}


void look_in_direction(char_data *ch, int dir) {
	ACMD(do_weather);
	extern const char *from_dir[];
	
	char buf[MAX_STRING_LENGTH - 9], buf2[MAX_STRING_LENGTH - 9];	// save room for the "You see "
	vehicle_data *veh;
	char_data *c;
	room_data *to_room;
	struct room_direction_data *ex;
	int last_comma_pos, prev_comma_pos, num_commas;
	size_t bufsize;
	
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
				*buf = '\0';
				bufsize = 0;
				last_comma_pos = prev_comma_pos = -1;
				num_commas = 0;
				
				to_room = ex->room_ptr;
				if (CAN_SEE_IN_DARK_ROOM(ch, to_room)) {
					for (c = ROOM_PEOPLE(to_room); c; c = c->next_in_room) {
						if (!AFF_FLAGGED(c, AFF_HIDE | AFF_NO_SEE_IN_ROOM) && CAN_SEE(ch, c) && WIZHIDE_OK(ch, c)) {
							bufsize += snprintf(buf + bufsize, sizeof(buf) - bufsize, "%s, ", PERS(c, ch, FALSE));
							if (last_comma_pos != -1) {
								prev_comma_pos = last_comma_pos;
							}
							last_comma_pos = bufsize - 2;
							++num_commas;
						}
					}
					
					LL_FOREACH2(ROOM_VEHICLES(to_room), veh, next_in_room) {
						if (CAN_SEE_VEHICLE(ch, veh)) {
							bufsize += snprintf(buf + bufsize, sizeof(buf) - bufsize, "%s, ", get_vehicle_short_desc(veh, ch));
							if (last_comma_pos != -1) {
								prev_comma_pos = last_comma_pos;
							}
							last_comma_pos = bufsize - 2;
							++num_commas;
						}
					}
				}

				/* Now, we clean up that buf */
				if (*buf) {
					if (last_comma_pos != -1) {
						bufsize = last_comma_pos + snprintf(buf + last_comma_pos, sizeof(buf) - last_comma_pos, ".\r\n");
					}
					if (prev_comma_pos != -1) {
						strcpy(buf2, buf + prev_comma_pos + 1);
						bufsize = snprintf(buf + prev_comma_pos, sizeof(buf) - prev_comma_pos, "%s and%s", (num_commas > 2 ? "," : ""), buf2);
					}

					msg_to_char(ch, "You see %s", buf);
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
		// map look
		bufsize = 0;
		*buf = '\0';
		last_comma_pos = prev_comma_pos = -1;
		num_commas = 0;
		
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
			send_to_char("You see a building", ch);
			
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

		if (CAN_SEE_IN_DARK_ROOM(ch, to_room)) {
			for (c = ROOM_PEOPLE(to_room); c; c = c->next_in_room) {
				if (CAN_SEE(ch, c) && WIZHIDE_OK(ch, c)) {
					bufsize += snprintf(buf + bufsize, sizeof(buf) - bufsize, "%s, ", PERS(c, ch, FALSE));
					if (last_comma_pos != -1) {
						prev_comma_pos = last_comma_pos;
					}
					last_comma_pos = bufsize - 2;
					++num_commas;
				}
			}
			
			LL_FOREACH2(ROOM_VEHICLES(to_room), veh, next_in_room) {
				if (CAN_SEE_VEHICLE(ch, veh)) {
					bufsize += snprintf(buf + bufsize, sizeof(buf) - bufsize, "%s, ", get_vehicle_short_desc(veh, ch));
					if (last_comma_pos != -1) {
						prev_comma_pos = last_comma_pos;
					}
					last_comma_pos = bufsize - 2;
					++num_commas;
				}
			}
		}
		
		/* Shift, rinse, repeat */
		to_room = real_shift(to_room, shift_dir[dir][0], shift_dir[dir][1]);
		if (to_room && !ROOM_SECT_FLAGGED(to_room, SECTF_OBSCURE_VISION) && !ROOM_IS_CLOSED(to_room)) {
			if (CAN_SEE_IN_DARK_ROOM(ch, to_room)) {
				for (c = ROOM_PEOPLE(to_room); c; c = c->next_in_room) {
					if (CAN_SEE(ch, c) && WIZHIDE_OK(ch, c)) {
						bufsize += snprintf(buf + bufsize, sizeof(buf) - bufsize, "%s, ", PERS(c, ch, FALSE));
						if (last_comma_pos != -1) {
							prev_comma_pos = last_comma_pos;
						}
						last_comma_pos = bufsize - 2;
						++num_commas;
					}
				}
			}
			/* And a third time for good measure */
			to_room = real_shift(to_room, shift_dir[dir][0], shift_dir[dir][1]);
			if (to_room && !ROOM_SECT_FLAGGED(to_room, SECTF_OBSCURE_VISION) && !ROOM_IS_CLOSED(to_room)) {
				if (CAN_SEE_IN_DARK_ROOM(ch, to_room)) {
					for (c = ROOM_PEOPLE(to_room); c; c = c->next_in_room) {
						if (CAN_SEE(ch, c) && WIZHIDE_OK(ch, c)) {
							bufsize += snprintf(buf + bufsize, sizeof(buf) - bufsize, "%s, ", PERS(c, ch, FALSE));
							if (last_comma_pos != -1) {
								prev_comma_pos = last_comma_pos;
							}
							last_comma_pos = bufsize - 2;
							++num_commas;
						}
					}
				}
			}
		}

		/* Now, we clean up that buf */
		if (*buf) {
			if (last_comma_pos != -1) {
				bufsize = last_comma_pos + snprintf(buf + last_comma_pos, sizeof(buf) - last_comma_pos, ".\r\n");
			}
			if (prev_comma_pos != -1) {
				strcpy(buf2, buf + prev_comma_pos + 1);
				bufsize = snprintf(buf + prev_comma_pos, sizeof(buf) - prev_comma_pos, "%s and%s", (num_commas > 2 ? "," : ""), buf2);
			}
			
			msg_to_char(ch, "You see %s", buf);
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
	extern const char *closed_ruins_icons[NUM_RUINS_ICONS];
	extern const char *open_ruins_icons[NUM_RUINS_ICONS];
	extern int get_north_for_char(char_data *ch);
	extern int get_direction_for_char(char_data *ch, int dir);
	extern struct city_metadata_type city_type[];
	
	bool need_color_terminator = FALSE;
	char buf[30], buf1[30], col_buf[256], lbuf[MAX_STRING_LENGTH];
	struct empire_city_data *city;
	int iter;
	empire_data *emp, *chemp = GET_LOYALTY(ch);
	int tileset = pick_season(to_room);
	struct icon_data *base_icon, *icon, *crop_icon = NULL;
	bool junk, enchanted, hidden = FALSE, painted;
	crop_data *cp = ROOM_CROP(to_room);
	sector_data *st, *base_sect = BASE_SECT(to_room);
	char *base_color, *str;
	room_data *map_loc, *map_to_room;
	vehicle_data *show_veh;
	
	// options
	bool show_dark = IS_SET(options, LRR_SHOW_DARK) ? TRUE : FALSE;
	// bool ship_partial = IS_SET(options, LRR_SHIP_PARTIAL) ? TRUE : FALSE;
		
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
	
	#define distance(x, y, a, b)		((x - a) * (x - a) + (y - b) * (y - b))
	
	// detect map locations
	map_loc = (GET_MAP_LOC(IN_ROOM(ch)) ? real_room(GET_MAP_LOC(IN_ROOM(ch))->vnum) : NULL);
	map_to_room = (GET_MAP_LOC(to_room) ? real_room(GET_MAP_LOC(to_room)->vnum) : NULL);

	// detect base icon
	base_icon = get_icon_from_set(GET_SECT_ICONS(base_sect), tileset);
	base_color = base_icon->color;
	if (ROOM_SECT_FLAGGED(to_room, SECTF_CROP) && cp) {
		crop_icon = get_icon_from_set(GET_CROP_ICONS(cp), tileset);
		base_color = crop_icon->color;
	}
	
	painted = (!IS_NPC(ch) && ROOM_PAINT_COLOR(to_room) && !PRF_FLAGGED(ch, PRF_NO_PAINT));

	// start with the sector color
	strcpy(buf, base_color);

	if (to_room == IN_ROOM(ch) && !ROOM_IS_CLOSED(IN_ROOM(ch))) {
		sprintf(buf, "&0<%soo&0>", chemp ? EMPIRE_BANNER(chemp) : "");
	}
	else if (!show_dark && !PRF_FLAGGED(ch, PRF_INFORMATIVE | PRF_POLITICAL) && show_pc_in_room(ch, to_room, mappc)) {
		return;
	}
	
	// check for a vehicle with an icon
	else if ((show_veh = find_vehicle_to_show(ch, to_room))) {
		strcat(buf, NULLSAFE(VEH_ICON(show_veh)));
	}

	/* Hidden buildings */
	else if (CHECK_CHAMELEON(map_loc, to_room)) {
		strcat(buf, base_icon->icon);
		hidden = TRUE;
	}

	/* Rooms with custom icons (take precedence over all but hidden rooms */
	else if (ROOM_CUSTOM_ICON(to_room)) {
		strcat(buf, ROOM_CUSTOM_ICON(to_room));
	}
	else if (ANY_ROAD_TYPE(to_room)) {
		// check west for the first 2 parts of the tile
		if (CONNECTS_TO_ROAD(r_west)) {
			// road west
			strcat(buf, "&0--");
		}
		else if ((CONNECTS_TO_ROAD(r_southwest) && !CONNECTS_TO_ROAD(r_south)) && (CONNECTS_TO_ROAD(r_northwest) && !CONNECTS_TO_ROAD(r_north))) {
			// both nw and sw
			strcat(buf, "&0>-");
		}
		else if (CONNECTS_TO_ROAD(r_southwest) && !CONNECTS_TO_ROAD(r_south)) {
			// just sw
			strcat(buf, "&0,-");
		}
		else if (CONNECTS_TO_ROAD(r_northwest) && !CONNECTS_TO_ROAD(r_north)) {
			// just nw
			strcat(buf, "&0`-");
		}
		else if (CONNECTS_TO_ROAD(r_north) || CONNECTS_TO_ROAD(r_south)) {
			// road north/south but NOT west
			sprintf(buf + strlen(buf), "&?%c%c", GET_SECT_ROADSIDE_ICON(base_sect), GET_SECT_ROADSIDE_ICON(base_sect));
		}
		else {
			// N/W/S were not road
			sprintf(buf + strlen(buf), "&?%c&0-", GET_SECT_ROADSIDE_ICON(base_sect));
		}
		
		// middle part
		if (CONNECTS_TO_ROAD(r_north) || CONNECTS_TO_ROAD(r_south)) {
			// N/S are road
			if (strstr(buf, "-") || CONNECTS_TO_ROAD(r_east) || (CONNECTS_TO_ROAD(r_northeast) && !CONNECTS_TO_ROAD(r_north)) || (CONNECTS_TO_ROAD(r_southeast) && !CONNECTS_TO_ROAD(r_south))) {
				strcat(buf, "&0+");
			}
			else {
				strcat(buf, "&0|");
			}
		}
		else {
			strcat(buf, "&0-");
		}
		
		// check east for last part of tile
		if (CONNECTS_TO_ROAD(r_east)) {
			strcat(buf, "-");
		}
		else if ((CONNECTS_TO_ROAD(r_northeast) && !CONNECTS_TO_ROAD(r_north)) && (CONNECTS_TO_ROAD(r_southeast) && !CONNECTS_TO_ROAD(r_south))) {
			// ne and se
			strcat(buf, "<");
		}
		else if (CONNECTS_TO_ROAD(r_northeast) && !CONNECTS_TO_ROAD(r_north)) {
			// just ne
			strcat(buf, "'");
		}
		else if (CONNECTS_TO_ROAD(r_southeast) && !CONNECTS_TO_ROAD(r_south)) {
			// just se
			strcat(buf, ",");
		}
		else {
			sprintf(buf + strlen(buf), "&?%c", GET_SECT_ROADSIDE_ICON(base_sect));
		}
	}
	else if (BUILDING_VNUM(to_room) == BUILDING_STEPS) {
		// check west for the first 2 parts of the tile
		if (CONNECTS_TO_ROAD(r_west) || (CONNECTS_TO_ROAD(r_southwest) && !CONNECTS_TO_ROAD(r_south)) || (CONNECTS_TO_ROAD(r_northwest) && !CONNECTS_TO_ROAD(r_north))) {
			// road west
			strcat(buf, "&0=");
		}
		else {
			sprintf(buf + strlen(buf), "&?%c", GET_SECT_ROADSIDE_ICON(base_sect));
		}
		
		// middle part
		if (CONNECTS_TO_ROAD(r_north) || CONNECTS_TO_ROAD(r_south)) {
			// N/S are road
			
			if (strstr(buf, "=") || CONNECTS_TO_ROAD(r_east) || (CONNECTS_TO_ROAD(r_northeast) && !CONNECTS_TO_ROAD(r_north)) || (CONNECTS_TO_ROAD(r_southeast) && !CONNECTS_TO_ROAD(r_south))) {
				strcat(buf, "&0++");
			}
			else {
				strcat(buf, "&0||");
			}
		}
		else {
			strcat(buf, "&0==");
		}
		
		// check east for last part of tile
		if (CONNECTS_TO_ROAD(r_east) || (CONNECTS_TO_ROAD(r_northeast) && !CONNECTS_TO_ROAD(r_north)) || (CONNECTS_TO_ROAD(r_southeast) && !CONNECTS_TO_ROAD(r_south))) {
			strcat(buf, "=");
		}
		else {
			sprintf(buf + strlen(buf), "&?%c", GET_SECT_ROADSIDE_ICON(base_sect));
		}
	}
	else if (ROOM_SECT_FLAGGED(to_room, SECTF_CROP) && cp) {
		strcat(buf, crop_icon->icon);
	}
	else if (IS_CITY_CENTER(to_room)) {
		emp = ROOM_OWNER(to_room);
		if ((city = find_city(emp, to_room))) {
			strcat(buf, city_type[city->type].icon);
		}
		else {
			strcat(buf, "[  ]");
		}
	}
	else if (BUILDING_VNUM(to_room) == BUILDING_RUINS_CLOSED) {
		// TODO could add variable icons system like sectors use, or a "custom ruins icon" to Building data
		strcat(buf, closed_ruins_icons[get_room_extra_data(to_room, ROOM_EXTRA_RUINS_ICON)]);
	}
	else if (BUILDING_VNUM(to_room) == BUILDING_RUINS_OPEN) {
		// TODO could add variable icons system like sectors use, or a "custom ruins icon" to Building data
		strcat(buf, open_ruins_icons[get_room_extra_data(to_room, ROOM_EXTRA_RUINS_ICON)]);
	}
	else if (IS_MAP_BUILDING(to_room) && GET_BUILDING(to_room)) {
		strcat(buf, GET_BLD_ICON(GET_BUILDING(to_room)));
	}
	else {
		icon = get_icon_from_set(GET_SECT_ICONS(SECT(to_room)), tileset);
		strcat(buf, icon->icon);
	}
	
	// buf is now the completed icon, but has both color codes (&) and variable tile codes (@)
	if (strchr(buf, '@')) {
		// NOTE: If you add new @ codes here, you must update "const char *icon_codes" in utils.c
		
		// here (@.) roadside icon
		if (strstr(buf, "@.")) {
			icon = get_icon_from_set(GET_SECT_ICONS(base_sect), tileset);
			sprintf(buf1, "%s%c", icon->color, GET_SECT_ROADSIDE_ICON(base_sect));
			str = str_replace("@.", buf1, buf);
			strcpy(buf, str);
			free(str);
		}
		// east (@e) tile attachment
		if (strstr(buf, "@e")) {
			st = r_east ? BASE_SECT(r_east) : BASE_SECT(to_room);
			icon = get_icon_from_set(GET_SECT_ICONS(st), tileset);
			sprintf(buf1, "%s%c", icon->color, GET_SECT_ROADSIDE_ICON(st));
			str = str_replace("@e", buf1, buf);
			strcpy(buf, str);
			free(str);
		}
		// west (@w) tile attachment
		if (strstr(buf, "@w")) {
			st = r_west ? BASE_SECT(r_west) : BASE_SECT(to_room);
			icon = get_icon_from_set(GET_SECT_ICONS(st), tileset);
			sprintf(buf1, "%s%c", icon->color, GET_SECT_ROADSIDE_ICON(st));
			str = str_replace("@w", buf1, buf);
			strcpy(buf, str);
			free(str);
		}
		
		// west (@u) barrier attachment
		if (strstr(buf, "@u") || strstr(buf, "@U")) {
			if (!r_west || ((IS_BARRIER(r_west) || ROOM_IS_CLOSED(r_west)) && !ROOM_AFF_FLAGGED(r_west, ROOM_AFF_CHAMELEON))) {
				enchanted = (r_west && ROOM_AFF_FLAGGED(r_west, ROOM_AFF_NO_FLY)) || ROOM_AFF_FLAGGED(to_room, ROOM_AFF_NO_FLY);
				// west is a barrier
				sprintf(buf1, "%sv", enchanted ? "&m" : "&0");
				str = str_replace("@u", buf1, buf);
				strcpy(buf, str);
				free(str);
				sprintf(buf1, "%sV", enchanted ? "&m" : "&0");
				str = str_replace("@U", buf1, buf);
				strcpy(buf, str);
				free(str);
			}
			else {
				// west is not a barrier
				sprintf(buf1, "&?%c", GET_SECT_ROADSIDE_ICON(base_sect));
				str = str_replace("@u", buf1, buf);
				strcpy(buf, str);
				free(str);
				str = str_replace("@U", buf1, buf);
				strcpy(buf, str);
				free(str);
			}
		}
		
		//  east (@v) barrier attachment
		if (strstr(buf, "@v") || strstr(buf, "@V")) {
			if (!r_east || ((IS_BARRIER(r_east) || ROOM_IS_CLOSED(r_east)) && !ROOM_AFF_FLAGGED(r_east, ROOM_AFF_CHAMELEON))) {
				enchanted = (r_east && ROOM_AFF_FLAGGED(r_east, ROOM_AFF_NO_FLY)) || ROOM_AFF_FLAGGED(to_room, ROOM_AFF_NO_FLY);
				// east is a barrier
				sprintf(buf1, "%sv", enchanted ? "&m" : "&0");
				str = str_replace("@v", buf1, buf);
				strcpy(buf, str);
				free(str);
				sprintf(buf1, "%sV", enchanted ? "&m" : "&0");
				str = str_replace("@V", buf1, buf);
				strcpy(buf, str);
				free(str);
			}
			else {
				// east is not a barrier
				sprintf(buf1, "&?%c", GET_SECT_ROADSIDE_ICON(base_sect));
				str = str_replace("@v", buf1, buf);
				strcpy(buf, str);
				free(str);
				str = str_replace("@V", buf1, buf);
				strcpy(buf, str);
				free(str);
			}
		}
	}

	// buf now contains the tile with preliminary color codes including &?

	if (IS_BURNING(to_room)) {
		strcpy(buf1, strip_color(buf));
		sprintf(buf, "\t0\t[B300]%s", buf1);
		need_color_terminator = TRUE;
	}
	else if (PRF_FLAGGED(ch, PRF_NOMAPCOL | PRF_POLITICAL | PRF_INFORMATIVE) || painted || show_dark) {
		strcpy(buf1, strip_color(buf));

		if (PRF_FLAGGED(ch, PRF_POLITICAL) && !show_dark) {
			emp = ROOM_OWNER(to_room);
			
			if (chemp && (chemp == emp || find_city(chemp, to_room)) && is_in_city_for_empire(to_room, chemp, FALSE, &junk)) {
				strcpy(buf2, get_banner_complement_color(chemp));
				need_color_terminator = TRUE;
			}
			else {
				*buf2 = '\0';
			}
			
			if (emp && (!hidden || emp == GET_LOYALTY(ch))) {
				sprintf(buf, "%s%s%s", EMPIRE_BANNER(emp) ? EMPIRE_BANNER(emp) : "&0", buf2, buf1);
			}
			else {
				sprintf(buf, "&0%s%s", buf2, buf1);
			}
		}
		else if (PRF_FLAGGED(ch, PRF_INFORMATIVE) && !show_dark) {
			if (IS_IMMORTAL(ch) && ROOM_AFF_FLAGGED(to_room, ROOM_AFF_CHAMELEON) && IS_COMPLETE(to_room) && distance(FLAT_X_COORD(map_loc), FLAT_Y_COORD(map_loc), FLAT_X_COORD(map_to_room), FLAT_Y_COORD(map_to_room)) > 2) {
				strcpy(buf2, "&y");
			}
			else if (IS_DISMANTLING(to_room)) {
				strcpy(buf2, "&C");
			}
			else if (!IS_COMPLETE(to_room)) {
				strcpy(buf2, "&c");
			}
			else if (HAS_MAJOR_DISREPAIR(to_room)) {
				strcpy(buf2, "&r");
			}
			else if (HAS_MINOR_DISREPAIR(to_room)) {
				strcpy(buf2, "&m");
			}
			else if (room_has_function_and_city_ok(to_room, FNC_MINE)) {
				if (get_room_extra_data(to_room, ROOM_EXTRA_MINE_AMOUNT) > 0) {
					strcpy(buf2, "&g");
				}
				else {
					strcpy(buf2, "&r");
				}
			}
			else if (ROOM_AFF_FLAGGED(to_room, ROOM_AFF_NO_WORK)) {
				strcpy(buf2, "&B");
			}
			else {
				strcpy(buf2, "&0");
			}
			
			sprintf(buf, "%s%s", buf2, buf1);
		}
		else if (painted && !show_dark) {
			strcpy(col_buf, paint_colors[ROOM_PAINT_COLOR(to_room)]);
			if (ROOM_AFF_FLAGGED(to_room, ROOM_AFF_BRIGHT_PAINT)) {
				strtoupper(col_buf);
			}
			
			sprintf(buf, "%s%s", col_buf, buf1);
		}
		else if (!PRF_FLAGGED(ch, PRF_NOMAPCOL | PRF_INFORMATIVE | PRF_POLITICAL) && !painted && show_dark) {
			sprintf(buf, "&b%s", buf1);
		}
		else {
			// color was stripped but no color added, so add a "normal" color to prevent color bleed
			sprintf(buf, "&0%s", buf1);
		}
	}
	else {
		// normal color
		if (strstr(buf, "&?")) {
			replace_question_color(buf, base_color, lbuf);
			strcpy(buf, lbuf);
		}
		if (strstr(buf, "&#")) {
			str = str_replace("&#", ROOM_AFF_FLAGGED(to_room, ROOM_AFF_NO_FLY) ? "&m" : "&0", buf);
			strcpy(buf, str);
			free(str);
		}
		
		// stoned coloring
		if (AFF_FLAGGED(ch, AFF_STONED)) {
			// check all but the final char
			for (iter = 0; buf[iter] != 0 && buf[iter+1] != 0; ++iter) {
				if (buf[iter] == '&' || buf[iter] == '\t') {
					switch(buf[iter+1]) {
						case 'r':	buf[iter+1] = 'b';	break;
						case 'g':	buf[iter+1] = 'c';	break;
						case 'y':	buf[iter+1] = 'm';	break;
						case 'b':	buf[iter+1] = 'g';	break;
						case 'm':	buf[iter+1] = 'r';	break;
						case 'c':	buf[iter+1] = 'y';	break;
						case 'p':	buf[iter+1] = 'l';	break;
						case 'v':	buf[iter+1] = 'o';	break;
						case 'a':	buf[iter+1] = 't';	break;
						case 'l':	buf[iter+1] = 'w';	break;
						case 'j':	buf[iter+1] = 'v';	break;
						case 'o':	buf[iter+1] = 'p';	break;
						case 't':	buf[iter+1] = 'j';	break;
						case 'R':	buf[iter+1] = 'B';	break;
						case 'G':	buf[iter+1] = 'C';	break;
						case 'Y':	buf[iter+1] = 'M';	break;
						case 'B':	buf[iter+1] = 'G';	break;
						case 'M':	buf[iter+1] = 'R';	break;
						case 'C':	buf[iter+1] = 'Y';	break;
						case 'P':	buf[iter+1] = 'L';	break;
						case 'V':	buf[iter+1] = 'O';	break;
						case 'A':	buf[iter+1] = 'T';	break;
						case 'L':	buf[iter+1] = 'W';	break;
						case 'J':	buf[iter+1] = 'V';	break;
						case 'O':	buf[iter+1] = 'P';	break;
						case 'T':	buf[iter+1] = 'J';	break;
					}
				}
			}
		}
	}
	
	if (need_color_terminator) {
		strcat(buf, "&0");
	}
	
	send_to_char(buf, ch);
}


 //////////////////////////////////////////////////////////////////////////////
//// SCREEN READER FUNCTIONS /////////////////////////////////////////////////

/**
* Gets the short name for a room, for a screenreader user.
*
* @param char_data *ch The person to get the view for.
* @param room_data *from_room The room being viewed from.
* @param room_data *to_room The room being shown.
* @return char* The string to show.
*/
char *get_screenreader_room_name(char_data *ch, room_data *from_room, room_data *to_room) {
	static char lbuf[MAX_STRING_LENGTH];
	char temp[MAX_STRING_LENGTH];
	crop_data *cp;
	
	strcpy(temp, "*");
	
	if (CHECK_CHAMELEON(from_room, to_room)) {
		strcpy(temp, GET_SECT_NAME(BASE_SECT(to_room)));
	}
	else if (GET_BUILDING(to_room) && ROOM_BLD_FLAGGED(to_room, BLD_BARRIER) && ROOM_AFF_FLAGGED(to_room, ROOM_AFF_NO_FLY)) {
		sprintf(temp, "Enchanted %s", GET_BLD_NAME(GET_BUILDING(to_room)));
	}
	else if (GET_BUILDING(to_room)) {
		strcpy(temp, GET_BLD_NAME(GET_BUILDING(to_room)));
	}
	else if (GET_ROOM_TEMPLATE(to_room)) {
		strcpy(temp, GET_RMT_TITLE(GET_ROOM_TEMPLATE(to_room)));
	}
	else if (ROOM_SECT_FLAGGED(to_room, SECTF_CROP) && (cp = ROOM_CROP(to_room))) {
		strcpy(temp, GET_CROP_NAME(cp));
		CAP(temp);
	}
	else if (IS_ROAD(to_room) && SECT_FLAGGED(BASE_SECT(to_room), SECTF_ROUGH)) {
		strcpy(temp, "Winding Path");
	}
	else {
		strcpy(temp, GET_SECT_NAME(SECT(to_room)));
	}
	
	// start lbuf: color
	if (ROOM_PAINT_COLOR(to_room) && !PRF_FLAGGED(ch, PRF_NO_PAINT)) {
		sprintf(lbuf, "%s ", paint_names[ROOM_PAINT_COLOR(to_room)]);
	}
	else {
		*lbuf = '\0';
	}
	
	// now check custom name
	if (ROOM_CUSTOM_NAME(to_room) && !CHECK_CHAMELEON(from_room, to_room)) {
		sprintf(lbuf + strlen(lbuf), "%s/%s", ROOM_CUSTOM_NAME(to_room), temp);
	}
	else {
		strcat(lbuf, temp);
	}
	
	return lbuf;
}


void screenread_one_dir(char_data *ch, room_data *origin, int dir) {
	extern byte distance_can_see(char_data *ch);
	
	char buf[MAX_STRING_LENGTH], roombuf[MAX_INPUT_LENGTH], lastroom[MAX_INPUT_LENGTH], dirbuf[MAX_STRING_LENGTH], plrbuf[MAX_INPUT_LENGTH], infobuf[MAX_INPUT_LENGTH];
	char_data *vict;
	int mapsize, dist_iter;
	vehicle_data *show_veh;
	empire_data *emp;
	room_data *to_room;
	int repeats;
	bool allow_stacking = TRUE;	// always
	
	mapsize = GET_MAPSIZE(REAL_CHAR(ch));
	if (mapsize == 0) {
		mapsize = config_get_int("default_map_size");
	}
	
	// constrain for brief
	if (PRF_FLAGGED(ch, PRF_BRIEF)) {
		mapsize = MIN(3, mapsize);
	}

	// setup
	*dirbuf = '\0';
	*lastroom = '\0';
	repeats = 0;

	// show distance that direction		
	for (dist_iter = 1; dist_iter <= mapsize; ++dist_iter) {
		to_room = real_shift(origin, shift_dir[dir][0] * dist_iter, shift_dir[dir][1] * dist_iter);
		
		if (!to_room) {
			break;
		}
		
		*roombuf = '\0';
		
		if (ROOM_AFF_FLAGGED(to_room, ROOM_AFF_DARK) || (!CAN_SEE_IN_DARK_ROOM(ch, to_room) && compute_distance(origin, to_room) > distance_can_see(ch) && !adjacent_room_is_light(to_room))) {
			strcpy(roombuf, "Dark");
		}
		else {
			// not dark
		
			// show tile type
			strcat(roombuf, get_screenreader_room_name(ch, origin, to_room));
		
			// show mappc
			if (SHOW_PEOPLE_IN_ROOM(to_room)) {
				*plrbuf = '\0';
			
				for (vict = ROOM_PEOPLE(to_room); vict; vict = vict->next_in_room) {
					if (can_see_player_in_other_room(ch, vict)) {
						sprintf(plrbuf + strlen(plrbuf), "%s%s", *plrbuf ? ", " : "", PERS(vict, ch, FALSE));
					}
					else if (IS_NPC(vict) && size_data[GET_SIZE(vict)].show_on_map) {
						sprintf(plrbuf + strlen(plrbuf), "%s%s", *plrbuf ? ", " : "", skip_filler(PERS(vict, ch, FALSE)));
					}
				}
			
				if (*plrbuf) {
					sprintf(roombuf + strlen(roombuf), " <%s>", plrbuf);
				}
			}
			
			// show ships
			if ((show_veh = find_vehicle_to_show(ch, to_room))) {
				sprintf(roombuf + strlen(roombuf), " <%s>", skip_filler(get_vehicle_short_desc(show_veh, ch)));
			}
			
			// show ownership (political)
			if (PRF_FLAGGED(ch, PRF_POLITICAL)) {
				emp = ROOM_OWNER(to_room);
			
				if (emp) {
					sprintf(roombuf + strlen(roombuf), " (%s)", EMPIRE_ADJECTIVE(emp));
				}
			}
		
			// show status (informative)
			if (PRF_FLAGGED(ch, PRF_INFORMATIVE)) {
				get_informative_tile_string(ch, to_room, infobuf);
				if (*infobuf) {
					sprintf(roombuf + strlen(roombuf), " [%s]", infobuf);
				}
			}
		}
		
		// check for repeats?
		if (allow_stacking && !strcmp(lastroom, roombuf)) {
			// same as last one
			++repeats;
		}
		else {
			// different
			if (*lastroom) {
				if (repeats > 0) {
					snprintf(dirbuf + strlen(dirbuf), sizeof(dirbuf) - strlen(dirbuf), "%s%dx %s", *dirbuf ? ", " : "", repeats+1, lastroom);
				}
				else {
					snprintf(dirbuf + strlen(dirbuf), sizeof(dirbuf) - strlen(dirbuf), "%s%s", *dirbuf ? ", " : "", lastroom);
				}
			}
			
			// reset
			repeats = 0;
			strcpy(lastroom, roombuf);
		}
	}
	
	// check for lingering data to append
	if (*lastroom) {
		if (repeats > 0) {
			snprintf(dirbuf + strlen(dirbuf), sizeof(dirbuf) - strlen(dirbuf), "%s%dx %s", *dirbuf ? ", " : "", repeats+1, lastroom);
		}
		else {
			snprintf(dirbuf + strlen(dirbuf), sizeof(dirbuf) - strlen(dirbuf), "%s%s", *dirbuf ? ", " : "", lastroom);
		}
	}

	snprintf(buf, sizeof(buf), "%s: %s\r\n", dirs[get_direction_for_char(ch, dir)], dirbuf);
	CAP(buf);
	msg_to_char(ch, "%s", buf);
}


/**
* Main "visual" interface for the visually impaired. This lists what you can
* see in all directions.
*
* @param char_data *ch The player "looking" at the room.
* @param room_data *room What we're looking at.
* @param bitvector_t options Any LLR_x flags that get passed along.
*/
void show_screenreader_room(char_data *ch, room_data *room, bitvector_t options) {
	extern int get_north_for_char(char_data *ch);
	
	int each_dir, north = get_north_for_char(ch);
		
	// each_dir: iterate over directions and show them in order
	for (each_dir = 0; each_dir < NUM_2D_DIRS; ++each_dir) {
		screenread_one_dir(ch, room, confused_dirs[north][0][each_dir]);
	}
	
	if (!IS_SET(options, LRR_SHIP_PARTIAL | LRR_LOOK_OUT)) {
		msg_to_char(ch, "Here:\r\n");
	}
}


 //////////////////////////////////////////////////////////////////////////////
//// WHERE FUNCTIONS /////////////////////////////////////////////////////////

void perform_mortal_where(char_data *ch, char *arg) {
	extern struct instance_data *find_instance_by_room(room_data *room, bool check_homeroom, bool allow_fake_loc);
	extern bool valid_no_trace(room_data *room);
	extern bool valid_unseen_passing(room_data *room);
	
	int closest, dir, dist, max_distance;
	struct instance_data *ch_inst, *i_inst;
	descriptor_data *d;
	char_data *i, *found = NULL;
	
	if (has_player_tech(ch, PTECH_WHERE_UPGRADE)) {
		max_distance = 75;
	}
	else {
		max_distance = 20;
	}
	
	command_lag(ch, WAIT_OTHER);

	if (!*arg) {
		send_to_char("Players near you\r\n--------------------\r\n", ch);
		for (d = descriptor_list; d; d = d->next) {
			if (STATE(d) != CON_PLAYING || !(i = d->character) || IS_NPC(i) || ch == i || !IN_ROOM(i))
				continue;
			if (!CAN_SEE(ch, i) || !CAN_RECOGNIZE(ch, i) || !WIZHIDE_OK(ch, i) || AFF_FLAGGED(i, AFF_NO_WHERE))
				continue;
			if ((dist = compute_distance(IN_ROOM(ch), IN_ROOM(i))) > max_distance)
				continue;
			
			ch_inst = find_instance_by_room(IN_ROOM(ch), FALSE, FALSE);
			i_inst = find_instance_by_room(IN_ROOM(i), FALSE, FALSE);
			if (ch_inst != i_inst || IS_ADVENTURE_ROOM(IN_ROOM(i)) != !IS_ADVENTURE_ROOM(IN_ROOM(ch))) {
				// not in same adventure...
				if (NO_LOCATION(IN_ROOM(ch)) || NO_LOCATION(IN_ROOM(i))) {
					// one or the other is set no-location
					continue;
				}
				if (i_inst && ADVENTURE_FLAGGED(INST_ADVENTURE(i_inst), ADV_NO_NEARBY)) {
					// target's adventure is !nearby
					continue;
				}
			}
			if (has_player_tech(i, PTECH_NO_TRACK_WILD) && valid_no_trace(IN_ROOM(i))) {
				gain_player_tech_exp(i, PTECH_NO_TRACK_WILD, 10);
				continue;
			}
			if (has_player_tech(i, PTECH_NO_TRACK_CITY) && valid_unseen_passing(IN_ROOM(i))) {
				gain_player_tech_exp(i, PTECH_NO_TRACK_CITY, 10);
				continue;
			}
			
			dir = get_direction_for_char(ch, get_direction_to(IN_ROOM(ch), IN_ROOM(i)));
			// dist already set for us
		
			msg_to_char(ch, "%-20s -%s %s, %d tile%s %s\r\n", PERS(i, ch, 0), coord_display_room(ch, IN_ROOM(i), TRUE), get_room_name(IN_ROOM(i), FALSE), dist, PLURAL(dist), (dir != NO_DIR ? dirs[dir] : "away"));
			gain_player_tech_exp(ch, PTECH_WHERE_UPGRADE, 10);
		}
	}
	else {			/* print only FIRST char, not all. */
		found = NULL;
		closest = MAP_SIZE;
		for (i = character_list; i; i = i->next) {
			if (i == ch || !IN_ROOM(i) || !CAN_RECOGNIZE(ch, i) || !CAN_SEE(ch, i) || AFF_FLAGGED(i, AFF_NO_WHERE))
				continue;
			if (!multi_isname(arg, GET_PC_NAME(i)))
				continue;
			if ((dist = compute_distance(IN_ROOM(ch), IN_ROOM(i))) > max_distance)
				continue;
				
			ch_inst = find_instance_by_room(IN_ROOM(ch), FALSE, FALSE);
			i_inst = find_instance_by_room(IN_ROOM(i), FALSE, FALSE);
			if (i_inst != ch_inst || find_instance_by_room(IN_ROOM(ch), FALSE, FALSE) != find_instance_by_room(IN_ROOM(i), FALSE, FALSE)) {
				// not in same adventure...
				if (NO_LOCATION(IN_ROOM(ch)) || NO_LOCATION(IN_ROOM(i))) {
					// one or the other is set no-location
					continue;
				}
				if (i_inst && ADVENTURE_FLAGGED(INST_ADVENTURE(i_inst), ADV_NO_NEARBY)) {
					// target's adventure is !nearby
					continue;
				}
			}
			if (has_player_tech(i, PTECH_NO_TRACK_WILD) && valid_no_trace(IN_ROOM(i))) {
				gain_player_tech_exp(i, PTECH_NO_TRACK_WILD, 10);
				continue;
			}
			if (has_player_tech(i, PTECH_NO_TRACK_CITY) && valid_unseen_passing(IN_ROOM(i))) {
				gain_player_tech_exp(i, PTECH_NO_TRACK_CITY, 10);
				continue;
			}
			
			// trying to find closest
			if (!found || dist < closest) {
				found = i;
				closest = dist;
			}
		}

		if (found) {
			dir = get_direction_for_char(ch, get_direction_to(IN_ROOM(ch), IN_ROOM(found)));
			// distance is already set for us as 'closest'
			msg_to_char(ch, "%-25s -%s %s, %d tile%s %s\r\n", PERS(found, ch, FALSE), coord_display_room(ch, IN_ROOM(found), TRUE), get_room_name(IN_ROOM(found), FALSE), closest, PLURAL(closest), (dir != NO_DIR ? dirs[dir] : "away"));
			gain_player_tech_exp(ch, PTECH_WHERE_UPGRADE, 10);
		}
		else {
			send_to_char("No-one around by that name.\r\n", ch);
		}
	}
}


void print_object_location(int num, obj_data *obj, char_data *ch, int recur) {
	if (num > 0) {
		sprintf(buf, "O%3d. %-25s - ", num, GET_OBJ_DESC(obj, ch, OBJ_DESC_SHORT));
	}
	else {
		sprintf(buf, "%35s", " - ");
	}
	
	if (HAS_TRIGGERS(obj)) {
		strcat(buf, "[TRIG] ");
	}

	if (IN_ROOM(obj)) {
		sprintf(buf + strlen(buf), "[%d]%s %s\r\n",  GET_ROOM_VNUM(IN_ROOM(obj)), coord_display_room(ch, IN_ROOM(obj), TRUE), get_room_name(IN_ROOM(obj), FALSE));
		send_to_char(buf, ch);
	}
	else if (obj->carried_by) {
		sprintf(buf + strlen(buf), "carried by %s\r\n", PERS(obj->carried_by, ch, 1));
		send_to_char(buf, ch);
	}
	else if (obj->in_vehicle) {
		sprintf(buf + strlen(buf), "inside %s%s\r\n", get_vehicle_short_desc(obj->in_vehicle, ch), recur ? ", which is" : " ");
		if (recur) {
			sprintf(buf + strlen(buf), "%35s[%d]%s %s\r\n", " - ", GET_ROOM_VNUM(IN_ROOM(obj->in_vehicle)), coord_display_room(ch, IN_ROOM(obj->in_vehicle), TRUE), get_room_name(IN_ROOM(obj->in_vehicle), FALSE));
		}
		send_to_char(buf, ch);
	}
	else if (obj->worn_by) {
		sprintf(buf + strlen(buf), "worn by %s\r\n", PERS(obj->worn_by, ch, 1));
		send_to_char(buf, ch);
	}
	else if (obj->in_obj) {
		sprintf(buf + strlen(buf), "inside %s%s\r\n", GET_OBJ_DESC(obj->in_obj, ch, OBJ_DESC_SHORT), (recur ? ", which is" : " "));
		send_to_char(buf, ch);
		if (recur) {
			print_object_location(0, obj->in_obj, ch, recur);
		}
	}
	else {
		sprintf(buf + strlen(buf), "in an unknown location\r\n");
		send_to_char(buf, ch);
	}
}


void perform_immort_where(char_data *ch, char *arg) {
	int num = 0, found = 0;
	descriptor_data *d;
	vehicle_data *veh;
	char_data *i;
	obj_data *k;

	if (!*arg) {
		send_to_char("Players\r\n-------\r\n", ch);
		for (d = descriptor_list; d; d = d->next) {
			if (STATE(d) == CON_PLAYING) {
				i = (d->original ? d->original : d->character);
				if (i && CAN_SEE(ch, i) && IN_ROOM(i) && WIZHIDE_OK(ch, i)) {
					if (d->original) {
						msg_to_char(ch, "%-20s - [%d]%s %s (in %s)\r\n", GET_NAME(i), GET_ROOM_VNUM(IN_ROOM(i)), coord_display_room(ch, IN_ROOM(d->character), TRUE), get_room_name(IN_ROOM(d->character), FALSE), GET_NAME(d->character));
					}
					else {
						msg_to_char(ch, "%-20s - [%d]%s %s\r\n", GET_NAME(i), GET_ROOM_VNUM(IN_ROOM(i)), coord_display_room(ch, IN_ROOM(i), TRUE), get_room_name(IN_ROOM(i), FALSE));
					}
				}
			}
		}
	}
	else {
		for (i = character_list; i; i = i->next) {
			if (CAN_SEE(ch, i) && IN_ROOM(i) && WIZHIDE_OK(ch, i) && multi_isname(arg, GET_PC_NAME(i))) {
				found = 1;
				msg_to_char(ch, "M%3d. %-25s - %s[%d]%s %s\r\n", ++num, GET_NAME(i), (IS_NPC(i) && HAS_TRIGGERS(i)) ? "[TRIG] " : "", GET_ROOM_VNUM(IN_ROOM(i)), coord_display_room(ch, IN_ROOM(i), TRUE), get_room_name(IN_ROOM(i), FALSE));
			}
		}
		num = 0;
		LL_FOREACH2(vehicle_list, veh, next) {
			if (CAN_SEE_VEHICLE(ch, veh) && multi_isname(arg, VEH_KEYWORDS(veh))) {
				found = 1;
				msg_to_char(ch, "V%3d. %-25s - %s[%d]%s %s\r\n", ++num, VEH_SHORT_DESC(veh), (HAS_TRIGGERS(veh) ? "[TRIG] " : ""), GET_ROOM_VNUM(IN_ROOM(veh)), coord_display_room(ch, IN_ROOM(veh), TRUE), get_room_name(IN_ROOM(veh), FALSE));
			}
		}
		for (num = 0, k = object_list; k; k = k->next) {
			if (CAN_SEE_OBJ(ch, k) && multi_isname(arg, GET_OBJ_KEYWORDS(k))) {
				found = 1;
				print_object_location(++num, k, ch, TRUE);
			}
		}
		if (!found) {
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
		// disembark?
		if (cmd != -1 && (veh = GET_ROOM_VEHICLE(room)) && IN_ROOM(veh)) {
			size += snprintf(buf + size, sizeof(buf) - size, " %s\r\n", exit_description(ch, IN_ROOM(veh), "Disembark"));
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
		LL_FOREACH2(ROOM_CONTENTS(room), obj, next_content) {
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
	extern const char *alt_dirs[];
	
	room_data *use_room = (GET_MAP_LOC(IN_ROOM(ch)) ? real_room(GET_MAP_LOC(IN_ROOM(ch))->vnum) : NULL);
	int dir, dist, last_isle;
	room_data *to_room;
	bool any, show_obscured;
	
	int max_dist = MIN(MAP_WIDTH, MAP_HEIGHT) / 2;
	
	skip_spaces(&argument);
	
	if (IS_NPC(ch) || !HAS_NAVIGATION(ch)) {
		msg_to_char(ch, "You need Navigation to use mapscan.\r\n");
	}
	else if (!*argument) {
		msg_to_char(ch, "Scan the map in which direction?\r\n");
	}
	else if (!use_room || IS_ADVENTURE_ROOM(use_room) || ROOM_IS_CLOSED(use_room)) {	// check map room
		msg_to_char(ch, "You can only use mapscan out on the map.\r\n");
	}
	else if ((!GET_ROOM_VEHICLE(IN_ROOM(ch)) || !ROOM_BLD_FLAGGED(IN_ROOM(ch), BLD_LOOK_OUT)) && (IS_ADVENTURE_ROOM(IN_ROOM(ch)) || ROOM_IS_CLOSED(IN_ROOM(ch)))) {
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
	void clear_recent_moves(char_data *ch);
	void scan_for_tile(char_data *ch, char *argument);

	int dir;
	
	room_data *use_room = (GET_MAP_LOC(IN_ROOM(ch)) ? real_room(GET_MAP_LOC(IN_ROOM(ch))->vnum) : NULL);
	
	skip_spaces(&argument);
	
	if (AFF_FLAGGED(ch, AFF_BLIND)) {
		msg_to_char(ch, "You can't see a damned thing, you're blind!\r\n");
	}
	else if (!*argument) {
		msg_to_char(ch, "Scan which direction or for what type of tile?\r\n");
	}
	else if (!use_room || IS_ADVENTURE_ROOM(use_room) || ROOM_IS_CLOSED(use_room)) {	// check map room
		msg_to_char(ch, "You can only use scan out on the map.\r\n");
	}
	else if ((!GET_ROOM_VEHICLE(IN_ROOM(ch)) || !ROOM_BLD_FLAGGED(IN_ROOM(ch), BLD_LOOK_OUT)) && (IS_ADVENTURE_ROOM(IN_ROOM(ch)) || ROOM_IS_CLOSED(IN_ROOM(ch)))) {
		msg_to_char(ch, "Scan only works out on the map.\r\n");
	}
	else if ((dir = parse_direction(ch, argument)) == NO_DIR) {
		clear_recent_moves(ch);
		scan_for_tile(ch, argument);
	}
	else if (dir >= NUM_2D_DIRS) {
		msg_to_char(ch, "You can't scan that way.\r\n");
	}
	else {
		clear_recent_moves(ch);
		screenread_one_dir(ch, use_room, dir);
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
