/* ************************************************************************
*   File: mapview.c                                       EmpireMUD 2.0b1 *
*  Usage: Map display functions                                           *
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


 //////////////////////////////////////////////////////////////////////////////
//// DATA ////////////////////////////////////////////////////////////////////

#define ANY_ROAD_TYPE(tile)  (IS_ROAD(tile) || BUILDING_VNUM(tile) == BUILDING_BRIDGE || BUILDING_VNUM(tile) == BUILDING_SWAMPWALK)
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
extern const int confused_dirs[NUM_SIMPLE_DIRS][2][NUM_OF_DIRS];
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
* @param empire_data *emp
* @return char* The background color that's the opposite of the banner color
*/
char *get_banner_complement_color(empire_data *emp) {
	if (!emp) {
		return "";
	}
	
	if (strchr(EMPIRE_BANNER(emp), 'r')) {
		return BACKGROUND_GREEN;
	}
	if (strchr(EMPIRE_BANNER(emp), 'g')) {
		return BACKGROUND_RED;
	}
	if (strchr(EMPIRE_BANNER(emp), 'y')) {
		return BACKGROUND_BLUE;
	}
	if (strchr(EMPIRE_BANNER(emp), 'b')) {
		return BACKGROUND_CYAN;
	}
	if (strchr(EMPIRE_BANNER(emp), 'm')) {
		return BACKGROUND_BLUE;
	}
	if (strchr(EMPIRE_BANNER(emp), 'c')) {
		return BACKGROUND_BLUE;
	}
	
	// default
	return BACKGROUND_BLUE;
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
* Returns the mineral name for a room with mine data.
*
* @param room_data *room The room
* @return char* The mineral name
*/
char *get_mine_type_name(room_data *room) {
	extern int find_mine_type(int type);
	extern const struct mine_data_type mine_data[];
	
	int t = find_mine_type(get_room_extra_data(room, ROOM_EXTRA_MINE_TYPE));
	
	return (t == NOTHING ? "unknown" : mine_data[t].name);
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
	else if (ROOM_SECT_FLAGGED(room, SECTF_CROP) && (cp = crop_proto(ROOM_CROP_TYPE(room))))
		strcat(name, GET_CROP_TITLE(cp));

	/* Custom names by type */
	else if (IS_ROAD(room) && SECT_FLAGGED(ROOM_ORIGINAL_SECT(room), SECTF_ROUGH)) {
		strcat(name, "A Winding Path");
	}

	// patron monuments
	else if (GET_BUILDING(room) && ROOM_PATRON(room) > 0) {
		strcat(name, GET_BLD_TITLE(GET_BUILDING(room)));
		sprintf(name + strlen(name), " of %s", get_name_by_id(ROOM_PATRON(room)) ? CAP(get_name_by_id(ROOM_PATRON(room))) : "a Former God");
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


// determines which tileset to use for sector color
int pick_season(room_data *room) {
	int ycoord = Y_COORD(room);
	double arctic = config_get_double("arctic_percent") / 200.0;	// split in half and convert from XX.XX to .XXXX (percent)
	double tropics = config_get_double("tropics_percent") / 200.0;
	bool northern = (ycoord >= MAP_HEIGHT/2);
	
	// month 0 is january
	
	// tropics? -- take half the tropic value, convert to percent, multiply by map height
	if (ycoord >= (tropics * MAP_HEIGHT) && ycoord <= (MAP_HEIGHT - (tropics * MAP_HEIGHT))) {
		if (time_info.month < 2) {
			return TILESET_SPRING;
		}
		else if (time_info.month > 10) {
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
	
	// all other regions:
	if (time_info.month < 2 || time_info.month > 10) {
		return northern ? TILESET_WINTER : TILESET_SUMMER;
	}
	else if (time_info.month < 5) {
		return northern ? TILESET_SPRING : TILESET_AUTUMN;
	}
	else if (time_info.month < 8) {
		return northern ? TILESET_SUMMER : TILESET_WINTER;
	}
	else {
		return northern ? TILESET_AUTUMN : TILESET_SPRING;
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
	
	if (!IS_NPC(vict) && CAN_SEE(ch, vict) && CAN_RECOGNIZE(ch, vict)) {
		if (compute_distance(IN_ROOM(ch), IN_ROOM(vict)) <= distance_can_see_players) {
			if (HAS_ABILITY(vict, ABIL_CLOAK_OF_DARKNESS)) {
				gain_ability_exp(vict, ABIL_CLOAK_OF_DARKNESS, 10);
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
	}
	
	// each case colors slightly differently
	if (count == 0) {
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
	void show_screenreader_room(char_data *ch, room_data *room, bitvector_t options);
	void list_obj_to_char(obj_data *list, char_data *ch, int mode, int show);
	void list_char_to_char(char_data *list, char_data *ch);
	extern const struct tavern_data_type tavern_data[];
	extern int how_to_show_map[NUM_SIMPLE_DIRS][2];
	extern int show_map_y_first[NUM_SIMPLE_DIRS];
	extern byte distance_can_see(char_data *ch);
	extern char *get_name_by_id(int id);
	extern int find_mine_type(int type);
	extern struct city_metadata_type city_type[];
	extern const char *bld_flags[];
	extern const char *room_aff_bits[];
	extern const char *room_template_flags[];

	struct mappc_data_container *mappc = NULL;
	struct mappc_data *pc, *next_pc;
	struct building_resource_type *res;
	struct empire_city_data *city;
	char output[MAX_STRING_LENGTH], shipbuf[256], flagbuf[MAX_STRING_LENGTH], partialbuf[MAX_STRING_LENGTH];
	int s, t, mapsize, iter, type;
	int first_iter, second_iter, xx, yy, magnitude, north;
	int first_start, first_end, second_start, second_end, temp;
	bool y_first, invert_x, invert_y, found, comma;
	room_data *to_room;
	empire_data *emp, *pcemp;
	crop_data *cp;
	char *strptr;
	
	// configs
	int trench_initial_value = config_get_int("trench_initial_value");
	
	// options
	bool ship_partial = IS_SET(options, LRR_SHIP_PARTIAL) ? TRUE : FALSE;
	bool has_ship = GET_BOAT(IN_ROOM(ch)) ? TRUE : FALSE;
	bool show_title = !has_ship || ship_partial;

	// begin with the sanity check
	if (!ch || !ch->desc)
		return;

	mapsize = GET_MAPSIZE(REAL_CHAR(ch));
	if (mapsize == 0) {
		mapsize = config_get_int("default_map_size");
	}

	if (AFF_FLAGGED(ch, AFF_BLIND)) {
		msg_to_char(ch, "You see nothing but infinite darkness...\r\n");
		return;
	}

	if (AFF_FLAGGED(ch, AFF_EARTHMELD) && IS_ANY_BUILDING(IN_ROOM(ch))) {
		msg_to_char(ch, "You are beneath a building.\r\n");
		return;
	}

	// check for ship
	if (!ship_partial && GET_BOAT(IN_ROOM(ch))) {
		look_at_room_by_loc(ch, IN_ROOM(GET_BOAT(IN_ROOM(ch))), LRR_SHIP_PARTIAL);
	}

	// mappc setup
	CREATE(mappc, struct mappc_data_container, 1);
	
	// put ship in name
	if (ship_partial && GET_BOAT(IN_ROOM(ch))) {
		snprintf(shipbuf, sizeof(shipbuf), ", Aboard %s", get_obj_desc(GET_BOAT(IN_ROOM(ch)), ch, OBJ_DESC_SHORT));
	}
	else {
		*shipbuf = '\0';
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
		
		sprintf(output, "[%d] %s%s (%d, %d)&0 %s[ %s]\r\n", GET_ROOM_VNUM(room), get_room_name(room, TRUE), shipbuf, X_COORD(room), Y_COORD(room), (SCRIPT(room) ? "[TRIG] " : ""), flagbuf);
	}
	else if (HAS_ABILITY(ch, ABIL_NAVIGATION)) {
		// need navigation to see coords
		sprintf(output, "%s%s (%d, %d)&0\r\n", get_room_name(room, TRUE), shipbuf, X_COORD(room), Y_COORD(room));
	}
	else {
		sprintf(output, "%s%s&0\r\n", get_room_name(room, TRUE), shipbuf);
	}

	// show the room
	if (!ROOM_IS_CLOSED(room)) {
		// map rooms:
		
		if (!IS_NPC(ch) && PRF_FLAGGED(ch, PRF_SCREEN_READER)) {
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
				msg_to_char(ch, output);
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
						if (PRF_FLAGGED(ch, PRF_COLOR) && !PRF_FLAGGED(ch, PRF_NOMAPCOL)) {
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
		
		if (show_title) {
			msg_to_char(ch, output);
		}
		
		if (!CAN_SEE_IN_DARK_ROOM(ch, room)) {
			send_to_char("It is pitch black...\r\n", ch);
		}
		else {
			// description
			if (!PRF_FLAGGED(ch, PRF_BRIEF) && (strptr = get_room_description(room))) {
				msg_to_char(ch, "%s", strptr);
			}
		}
	}
	
	// ship-partial ends here
	if (ship_partial) {
		if (mappc) {
			while ((pc = mappc->data)) {
				mappc->data = pc->next;
				free(pc);
			}
			free(mappc);
		}		
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

	if ((emp = ROOM_OWNER(HOME_ROOM(room)))) {
		if ((city = find_city(emp, room))) {
			msg_to_char(ch, "This is the %s%s&0 %s of %s.", EMPIRE_BANNER(emp), EMPIRE_ADJECTIVE(emp), city_type[city->type].name, city->name);
		}	
		else {
			msg_to_char(ch, "This area is claimed by %s%s&0.", EMPIRE_BANNER(emp), EMPIRE_NAME(emp));
		}
		
		if (ROOM_PRIVATE_OWNER(HOME_ROOM(room)) != NOBODY) {
			msg_to_char(ch, " This is %s's private residence.", get_name_by_id(ROOM_PRIVATE_OWNER(HOME_ROOM(room))) ? CAP(get_name_by_id(ROOM_PRIVATE_OWNER(HOME_ROOM(room)))) : "someone");
		}
		
		if (ROOM_AFF_FLAGGED(HOME_ROOM(room), ROOM_AFF_PUBLIC)) {
			msg_to_char(ch, " (public)");
		}
		if (emp == GET_LOYALTY(ch) && ROOM_AFF_FLAGGED(HOME_ROOM(IN_ROOM(ch)), ROOM_AFF_NO_DISMANTLE)) {
			msg_to_char(ch, " (no-dismantle)");
		}
		
		msg_to_char(ch, "\r\n");
	}
	if (emp && GET_LOYALTY(ch) == emp && ROOM_AFF_FLAGGED(room, ROOM_AFF_NO_WORK)) {
		msg_to_char(ch, "Workforce will not work this tile.\r\n");
	}
	
	// don't show this in adventures, which are always unclaimable
	if (ROOM_AFF_FLAGGED(room, ROOM_AFF_UNCLAIMABLE) && !IS_ADVENTURE_ROOM(room)) {
		msg_to_char(ch, "This area is unclaimable.\r\n");
	}
	
	if (BUILDING_DISREPAIR(room) > config_get_int("disrepair_major")) {
		msg_to_char(ch, "It's in a state of serious disrepair.\r\n");
	}
	else if (BUILDING_DISREPAIR(room) > config_get_int("disrepair_minor")) {
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
	
	if (ROOM_BLD_FLAGGED(room, BLD_TAVERN) && IS_COMPLETE(room)) {
		msg_to_char(ch, "The tavern has %s on tap.\r\n", tavern_data[get_room_extra_data(room, ROOM_EXTRA_TAVERN_TYPE)].name);
	}

	if (ROOM_BLD_FLAGGED(room, BLD_MINE) && IS_COMPLETE(room)) {
		type = find_mine_type(get_room_extra_data(room, ROOM_EXTRA_MINE_TYPE));
		if (get_room_extra_data(room, ROOM_EXTRA_MINE_AMOUNT) <= 0 || type == NOTHING) {
			msg_to_char(ch, "This mine is depleted.\r\n");
		}
		else {
			msg_to_char(ch, "The mine is gleaming with %s.\r\n", get_mine_type_name(room));
		}
	}
	
	// has a crop but doesn't show as crop probably == seeded field
	if (ROOM_SECT_FLAGGED(room, SECTF_HAS_CROP_DATA) && !ROOM_SECT_FLAGGED(room, SECTF_CROP) && (cp = crop_proto(ROOM_CROP_TYPE(room)))) {
		msg_to_char(ch, "The field is seeded with %s.\r\n", GET_CROP_NAME(cp));
	}

	if (!IS_COMPLETE(room)) {
		msg_to_char(ch, "Remaining to %s: ", (IS_DISMANTLING(room) ? "Dismantle" : "Completion"));
		
		found = FALSE;
		for (res = BUILDING_RESOURCES(room); res; res = res->next) {
			msg_to_char(ch, "%s%s (x%d)", (found ? ", " : ""), skip_filler(get_obj_name_by_proto(res->vnum)), res->amount);
			found = TRUE;
		}
		
		msg_to_char(ch, "\r\n");
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
	
	if (BUILDING_BURNING(room)) {
		msg_to_char(ch, "%sThe building is on fire!&0\r\n", BACKGROUND_RED);
	}

	if (!AFF_FLAGGED(ch, AFF_EARTHMELD)) {
		/* now list characters & objects */
		send_to_char("&g", ch);
		list_obj_to_char(ROOM_CONTENTS(room), ch, OBJ_DESC_LONG, FALSE);
		send_to_char("&y", ch);
		list_char_to_char(ROOM_PEOPLE(room), ch);
		send_to_char("&0", ch);
	}

	/* Exits ? */
	if (ROOM_IS_CLOSED(room)) {
		do_exits(ch, "", 0, GET_ROOM_VNUM(room));
	}
}


void look_in_direction(char_data *ch, int dir) {
	ACMD(do_weather);
	extern const char *from_dir[];
	extern const int rev_dir[];
	
	char_data *c;
	int i;
	room_data *to_room;
	struct room_direction_data *ex;
	
	// weather override
	if (IS_OUTDOORS(ch) && dir == UP && !find_exit(IN_ROOM(ch), dir)) {
		do_weather(ch, "", 0, 0);
		return;
	}

	if (ROOM_IS_CLOSED(IN_ROOM(ch))) {
		if (COMPLEX_DATA(IN_ROOM(ch)) && (ex = find_exit(IN_ROOM(ch), dir))) {
			if (EXIT_FLAGGED(ex, EX_CLOSED) && ex->keyword) {
				sprintf(buf, "The %s is closed.\r\n", fname(ex->keyword));
				send_to_char(buf, ch);
			}
			else if (EXIT_FLAGGED(ex, EX_ISDOOR) && ex->keyword) {
				sprintf(buf, "The %s is open.\r\n", fname(ex->keyword));
				send_to_char(buf, ch);
			}
			if (!EXIT_FLAGGED(ex, EX_CLOSED)) {
				*buf = '\0';
				to_room = ex->room_ptr;
				if (CAN_SEE_IN_DARK_ROOM(ch, to_room)) {
					for (c = ROOM_PEOPLE(to_room); c; c = c->next_in_room) {
						if (CAN_SEE(ch, c)) {
							sprintf(buf+strlen(buf), "%s, ", PERS(c, c, 0));
						}
					}
				}

				/* Now, we clean up that buf */
				if (*buf) {
					sprintf(buf+strlen(buf)-2, ".\r\n");

					for (i = strlen(buf)-1; i > 0; i--) {
						if (buf[i] == ',') {
							sprintf(buf1, buf + i+1);
							buf[i] = '\0';
							strcat(buf, " and");
							strcat(buf, buf1);
							break;
						}
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
		*buf = '\0';
		
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
				if (CAN_SEE(ch, c)) {
					sprintf(buf+strlen(buf), "%s, ", PERS(c, c, 0));
				}
			}
		}
		
		/* Shift, rinse, repeat */
		to_room = real_shift(to_room, shift_dir[dir][0], shift_dir[dir][1]);
		if (to_room && !ROOM_SECT_FLAGGED(to_room, SECTF_OBSCURE_VISION) && !ROOM_IS_CLOSED(to_room)) {
			if (CAN_SEE_IN_DARK_ROOM(ch, to_room)) {
				for (c = ROOM_PEOPLE(to_room); c; c = c->next_in_room) {
					if (CAN_SEE(ch, c)) {
						sprintf(buf+strlen(buf), "%s, ", PERS(c, c, 0));
					}
				}
			}
			/* And a third time for good measure */
			to_room = real_shift(to_room, shift_dir[dir][0], shift_dir[dir][1]);
			if (to_room && !ROOM_SECT_FLAGGED(to_room, SECTF_OBSCURE_VISION) && !ROOM_IS_CLOSED(to_room)) {
				if (CAN_SEE_IN_DARK_ROOM(ch, to_room)) {
					for (c = ROOM_PEOPLE(to_room); c; c = c->next_in_room) {
						if (CAN_SEE(ch, c)) {
							sprintf(buf+strlen(buf), "%s, ", PERS(c, c, 0));
						}
					}
				}
			}
		}

		/* Now, we clean up that buf */
		if (*buf) {
			sprintf(buf+strlen(buf)-2, ".\r\n");

			for (i = strlen(buf)-1; i > 0; i--) {
				if (buf[i] == ',') {
					sprintf(buf1, buf + i+1);
					buf[i] = '\0';
					strcat(buf, " and");
					strcat(buf, buf1);
					break;
				}
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
	extern const char *ruins_icons[NUM_RUINS_ICONS];
	extern int get_north_for_char(char_data *ch);
	extern int get_direction_for_char(char_data *ch, int dir);
	extern struct city_metadata_type city_type[];
	extern const char *boat_icon_ocean;
	extern const char *boat_icon_river;

	bool need_color_terminator = FALSE;
	char buf[30], buf1[30], lbuf[MAX_STRING_LENGTH];
	char wallcolor[10];
	struct empire_city_data *city;
	int iter;
	empire_data *emp, *chemp = GET_LOYALTY(ch);
	int tileset = pick_season(to_room);
	struct icon_data *base_icon, *icon, *crop_icon = NULL;
	bool hidden = FALSE;
	crop_data *cp = crop_proto(ROOM_CROP_TYPE(to_room));
	sector_data *st, *base_sect = ROOM_ORIGINAL_SECT(to_room);
	char *base_color, *str;
	room_data *map_loc = get_map_location_for(IN_ROOM(ch)), *map_to_room = get_map_location_for(to_room);
	
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

	// wall color setup
	if (ROOM_AFF_FLAGGED(to_room, ROOM_AFF_NO_FLY)) {
		strcpy(wallcolor, "&m");
	}
	else {
		strcpy(wallcolor, "&0");
	}

	#define distance(x, y, a, b)		((x - a) * (x - a) + (y - b) * (y - b))

	// detect base icon
	base_icon = get_icon_from_set(GET_SECT_ICONS(base_sect), tileset);
	base_color = base_icon->color;
	if (ROOM_SECT_FLAGGED(to_room, SECTF_CROP) && cp) {
		crop_icon = get_icon_from_set(GET_CROP_ICONS(cp), tileset);
		base_color = crop_icon->color;
	}

	// start with the sector color
	strcpy(buf, base_color);

	if (to_room == IN_ROOM(ch)) {
		sprintf(buf, "&0<%soo&0>", chemp ? EMPIRE_BANNER(chemp) : "");
	}
	else if (!show_dark && show_pc_in_room(ch, to_room, mappc)) {
		return;
	}

	/* Hidden buildings */
	else if (distance(FLAT_X_COORD(map_loc), FLAT_Y_COORD(map_loc), FLAT_X_COORD(map_to_room), FLAT_Y_COORD(map_to_room)) > 2 && ROOM_AFF_FLAGGED(to_room, ROOM_AFF_CHAMELEON)) {
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
	else if (ROOM_AFF_FLAGGED(to_room, ROOM_AFF_SHIP_PRESENT) && ROOM_SECT_FLAGGED(to_room, SECTF_FRESH_WATER | SECTF_OCEAN)) {
		// show boats in room
		if (ROOM_SECT_FLAGGED(to_room, SECTF_FRESH_WATER)) {
			strcat(buf, boat_icon_river);
		}
		else if (ROOM_SECT_FLAGGED(to_room, SECTF_OCEAN)) {
			strcat(buf, boat_icon_ocean);
		}
		else {
			// should never hit this case
			icon = get_icon_from_set(GET_SECT_ICONS(SECT(to_room)), tileset);
			strcat(buf, icon->icon);
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
	else if (BUILDING_VNUM(to_room) == BUILDING_RUINS_OPEN || BUILDING_VNUM(to_room) == BUILDING_RUINS_CLOSED) {
		// TODO could add variable icons system like sectors use
		strcat(buf, ruins_icons[get_room_extra_data(to_room, ROOM_EXTRA_RUINS_ICON)]);
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
		// east (@e) tile attachment
		if (strstr(buf, "@e")) {
			st = r_east ? ROOM_ORIGINAL_SECT(r_east) : ROOM_ORIGINAL_SECT(to_room);
			icon = get_icon_from_set(GET_SECT_ICONS(st), tileset);
			sprintf(buf1, "%s%c", icon->color, GET_SECT_ROADSIDE_ICON(st));
			str = str_replace("@e", buf1, buf);
			strcpy(buf, str);
			free(str);
		}
		// west (@w) tile attachment
		if (strstr(buf, "@w")) {
			st = r_west ? ROOM_ORIGINAL_SECT(r_west) : ROOM_ORIGINAL_SECT(to_room);
			icon = get_icon_from_set(GET_SECT_ICONS(st), tileset);
			sprintf(buf1, "%s%c", icon->color, GET_SECT_ROADSIDE_ICON(st));
			str = str_replace("@w", buf1, buf);
			strcpy(buf, str);
			free(str);
		}
		
		// west (@u) barrier attachment
		if (strstr(buf, "@u") || strstr(buf, "@U")) {
			if (!r_west || IS_BARRIER(r_west) || ROOM_IS_CLOSED(r_west)) {
				// west is a barrier
				sprintf(buf1, "%sv", wallcolor);
				str = str_replace("@u", buf1, buf);
				strcpy(buf, str);
				free(str);
				sprintf(buf1, "%sV", wallcolor);
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
			if (!r_east || IS_BARRIER(r_east) || ROOM_IS_CLOSED(r_east)) {
				// east is a barrier
				sprintf(buf1, "%sv", wallcolor);
				str = str_replace("@v", buf1, buf);
				strcpy(buf, str);
				free(str);
				sprintf(buf1, "%sV", wallcolor);
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

	if (BUILDING_BURNING(to_room)) {
		strcpy(buf1, strip_color(buf));
		sprintf(buf, "&0%s%s", BACKGROUND_RED, buf1);
		need_color_terminator = TRUE;
	}
	else if (PRF_FLAGGED(ch, PRF_NOMAPCOL | PRF_POLITICAL | PRF_INFORMATIVE) || show_dark) {
		strcpy(buf1, strip_color(buf));

		if (PRF_FLAGGED(ch, PRF_POLITICAL) && !show_dark) {
			emp = ROOM_OWNER(to_room);
			
			if (chemp && find_city(chemp, to_room)) {
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
			if (IS_IMMORTAL(ch) && ROOM_AFF_FLAGGED(to_room, ROOM_AFF_CHAMELEON) && distance(FLAT_X_COORD(map_loc), FLAT_Y_COORD(map_loc), FLAT_X_COORD(map_to_room), FLAT_Y_COORD(map_to_room)) > 2) {
				strcpy(buf2, "&y");
			}
			else if (IS_DISMANTLING(to_room)) {
				strcpy(buf2, "&C");
			}
			else if (!IS_COMPLETE(to_room)) {
				strcpy(buf2, "&c");
			}
			else if (BUILDING_DISREPAIR(to_room) > config_get_int("disrepair_major")) {
				strcpy(buf2, "&r");
			}
			else if (BUILDING_DISREPAIR(to_room) > config_get_int("disrepair_minor")) {
				strcpy(buf2, "&m");
			}
			else if (ROOM_BLD_FLAGGED(to_room, BLD_MINE)) {
				if (get_room_extra_data(to_room, ROOM_EXTRA_MINE_AMOUNT) > 0) {
					strcpy(buf2, "&g");
				}
				else {
					strcpy(buf2, "&r");
				}
			}
			else {
				strcpy(buf2, "&0");
			}
			
			sprintf(buf, "%s%s", buf2, buf1);
		}
		else if (!PRF_FLAGGED(ch, PRF_NOMAPCOL | PRF_INFORMATIVE | PRF_POLITICAL) && show_dark) {
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
		if (strstr(buf, "&V")) {
			str = str_replace("&V", wallcolor, buf);
			strcpy(buf, str);
			free(str);
		}
		
		// stoned coloring
		if (AFF_FLAGGED(ch, AFF_STONED)) {
			// check all but the final char
			for (iter = 0; buf[iter] != 0 && buf[iter+1] != 0; ++iter) {
				if (buf[iter] == '&') {
					switch(buf[iter+1]) {
						case 'r':	buf[iter+1] = 'b';	break;
						case 'g':	buf[iter+1] = 'c';	break;
						case 'y':	buf[iter+1] = 'm';	break;
						case 'b':	buf[iter+1] = 'g';	break;
						case 'm':	buf[iter+1] = 'r';	break;
						case 'c':	buf[iter+1] = 'y';	break;
						case 'R':	buf[iter+1] = 'B';	break;
						case 'G':	buf[iter+1] = 'C';	break;
						case 'Y':	buf[iter+1] = 'M';	break;
						case 'B':	buf[iter+1] = 'G';	break;
						case 'M':	buf[iter+1] = 'R';	break;
						case 'C':	buf[iter+1] = 'Y';	break;
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

char *get_screenreader_room_name(room_data *from_room, room_data *to_room) {
	static char lbuf[MAX_INPUT_LENGTH];
	crop_data *cp;
	
	strcpy(lbuf, "*");

	if (ROOM_CUSTOM_NAME(to_room)) {
		strcpy(lbuf, ROOM_CUSTOM_NAME(to_room));
	}
	else if (GET_BUILDING(to_room) && ROOM_BLD_FLAGGED(to_room, BLD_BARRIER) && ROOM_AFF_FLAGGED(to_room, ROOM_AFF_NO_FLY)) {
		sprintf(lbuf, "Enchanted %s", GET_BLD_NAME(GET_BUILDING(to_room)));
	}
	else if (GET_BUILDING(to_room)) {
		strcpy(lbuf, GET_BLD_NAME(GET_BUILDING(to_room)));
	}
	else if (GET_ROOM_TEMPLATE(to_room)) {
		strcpy(lbuf, GET_RMT_TITLE(GET_ROOM_TEMPLATE(to_room)));
	}
	else if (ROOM_SECT_FLAGGED(to_room, SECTF_CROP) && (cp = crop_proto(ROOM_CROP_TYPE(to_room)))) {
		strcpy(lbuf, GET_CROP_NAME(cp));
		CAP(lbuf);
	}
	else if (IS_ROAD(to_room) && SECT_FLAGGED(ROOM_ORIGINAL_SECT(to_room), SECTF_ROUGH)) {
		strcpy(lbuf, "Winding Path");
	}
	else {
		strcpy(lbuf, GET_SECT_NAME(SECT(to_room)));
	}
	
	return lbuf;
}


void screenread_one_dir(char_data *ch, room_data *origin, int dir) {
	extern byte distance_can_see(char_data *ch);
	extern bool can_see_player_in_other_room(char_data *ch, char_data *vict);
	
	char buf[MAX_STRING_LENGTH], roombuf[MAX_INPUT_LENGTH], lastroom[MAX_INPUT_LENGTH], dirbuf[MAX_STRING_LENGTH], plrbuf[MAX_INPUT_LENGTH], infobuf[MAX_INPUT_LENGTH];
	char_data *vict;
	int mapsize, dist_iter;
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
			strcat(roombuf, get_screenreader_room_name(origin, to_room));
		
			// show mappc
			if (SHOW_PEOPLE_IN_ROOM(to_room)) {
				*plrbuf = '\0';
			
				for (vict = ROOM_PEOPLE(to_room); vict; vict = vict->next_in_room) {
					if (can_see_player_in_other_room(ch, vict)) {
						sprintf(plrbuf + strlen(plrbuf), "%s%s", *plrbuf ? ", " : "", PERS(vict, ch, FALSE));
					}
				}
			
				if (*plrbuf) {
					sprintf(roombuf + strlen(roombuf), " <%s>", plrbuf);
				}
			}
			
			// show ships
			if (ROOM_AFF_FLAGGED(to_room, ROOM_AFF_SHIP_PRESENT) && ROOM_SECT_FLAGGED(to_room, SECTF_FRESH_WATER | SECTF_OCEAN)) {
				sprintf(roombuf + strlen(roombuf), " <ship>");
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
				*infobuf = '\0';
			
				if (IS_DISMANTLING(to_room)) {
					sprintf(infobuf + strlen(infobuf), "%sdismantling", *infobuf ? ", " : "");
				}
				else if (!IS_COMPLETE(to_room)) {
					sprintf(infobuf + strlen(infobuf), "%sunfinished", *infobuf ? ", " :"");
				}
				if (BUILDING_DISREPAIR(to_room) > config_get_int("disrepair_major")) {
					sprintf(infobuf + strlen(infobuf), "%sbad disrepair", *infobuf ? ", " :"");
				}
				else if (BUILDING_DISREPAIR(to_room) > config_get_int("disrepair_minor")) {
					sprintf(infobuf + strlen(infobuf), "%sdisrepair", *infobuf ? ", " :"");
				}
				if (IS_COMPLETE(to_room) && ROOM_BLD_FLAGGED(to_room, BLD_MINE)) {
					if (get_room_extra_data(to_room, ROOM_EXTRA_MINE_AMOUNT) > 0) {
						sprintf(infobuf + strlen(infobuf), "%shas ore", *infobuf ? ", " :"");
					}
					else {
						sprintf(infobuf + strlen(infobuf), "%sdepleted", *infobuf ? ", " :"");
					}
				}
			
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
	int each_dir;
		
	// each_dir: iterate over directions and show them in order
	for (each_dir = 0; each_dir < NUM_2D_DIRS; ++each_dir) {
		screenread_one_dir(ch, room, each_dir);
	}
	
	if (!IS_SET(options, LRR_SHIP_PARTIAL)) {
		msg_to_char(ch, "Here:\r\n");
	}
}


 //////////////////////////////////////////////////////////////////////////////
//// WHERE FUNCTIONS /////////////////////////////////////////////////////////

void perform_mortal_where(char_data *ch, char *arg) {
	extern struct instance_data *find_instance_by_room(room_data *room);
	
	register char_data *i;
	register descriptor_data *d;
	int max_distance = 20;
	bool found = FALSE;
	
	if (HAS_ABILITY(ch, ABIL_MASTER_TRACKER)) {
		max_distance = 75;
	}
	
	command_lag(ch, WAIT_OTHER);

	if (!*arg) {
		send_to_char("Players near you\r\n--------------------\r\n", ch);
		for (d = descriptor_list; d; d = d->next) {
			if (STATE(d) != CON_PLAYING || !(i = d->character) || IS_NPC(i) || ch == i || !IN_ROOM(i))
				continue;
			if (!CAN_SEE(ch, i) || !CAN_RECOGNIZE(ch, i))
				continue;
			if (compute_distance(IN_ROOM(ch), IN_ROOM(i)) > max_distance)
				continue;
			if (IS_ADVENTURE_ROOM(IN_ROOM(i)) && find_instance_by_room(IN_ROOM(ch)) != find_instance_by_room(IN_ROOM(i))) {
				// not in same adventure
				continue;
			}
			if (HAS_ABILITY(i, ABIL_NO_TRACE) && IS_OUTDOORS(i)) {
				gain_ability_exp(i, ABIL_NO_TRACE, 10);
				continue;
			}
			if (HAS_ABILITY(i, ABIL_UNSEEN_PASSING) && !IS_OUTDOORS(i)) {
				gain_ability_exp(i, ABIL_UNSEEN_PASSING, 10);
				continue;
			}
		
			if (HAS_ABILITY(ch, ABIL_NAVIGATION)) {
				msg_to_char(ch, "%-20s - (%*d, %*d) %s\r\n", PERS(i, ch, 0), X_PRECISION, X_COORD(IN_ROOM(i)), Y_PRECISION, Y_COORD(IN_ROOM(i)), get_room_name(IN_ROOM(i), FALSE));
			}
			else {
				msg_to_char(ch, "%-20s - %s\r\n", PERS(i, ch, 0), get_room_name(IN_ROOM(i), FALSE));
			}
			gain_ability_exp(ch, ABIL_MASTER_TRACKER, 10);
		}
	}
	else {			/* print only FIRST char, not all. */
		for (i = character_list; i; i = i->next) {
			if (i == ch || !IN_ROOM(i) || !CAN_RECOGNIZE(ch, i))
				continue;
			if (!multi_isname(arg, GET_PC_NAME(i)))
				continue;
			if (compute_distance(IN_ROOM(ch), IN_ROOM(i)) > max_distance)
				continue;
			if (IS_ADVENTURE_ROOM(IN_ROOM(i)) && find_instance_by_room(IN_ROOM(ch)) != find_instance_by_room(IN_ROOM(i))) {
				// not in same adventure
				continue;
			}
			if (HAS_ABILITY(i, ABIL_NO_TRACE) && IS_OUTDOORS(i)) {
				gain_ability_exp(i, ABIL_NO_TRACE, 10);
				continue;
			}
			if (HAS_ABILITY(i, ABIL_UNSEEN_PASSING) && !IS_OUTDOORS(i)) {
				gain_ability_exp(i, ABIL_UNSEEN_PASSING, 10);
				continue;
			}

			if (HAS_ABILITY(ch, ABIL_NAVIGATION)) {
				msg_to_char(ch, "%-25s - (%*d, %*d) %s\r\n", PERS(i, ch, 0), X_PRECISION, X_COORD(IN_ROOM(i)), Y_PRECISION, Y_COORD(IN_ROOM(i)), get_room_name(IN_ROOM(i), FALSE));
			}
			else {
				msg_to_char(ch, "%-25s - %s\r\n", PERS(i, ch, 0), get_room_name(IN_ROOM(i), FALSE));
			}
			gain_ability_exp(ch, ABIL_MASTER_TRACKER, 10);
			found = TRUE;
			break;
		}
		
		if (!found) {
			send_to_char("No-one around by that name.\r\n", ch);
		}
	}
}


void print_object_location(int num, obj_data *obj, char_data *ch, int recur) {
	if (num > 0) {
		sprintf(buf, "O%3d. %-25s - ", num, GET_OBJ_DESC(obj, ch, OBJ_DESC_SHORT));
	}
	else {
		sprintf(buf, "%33s", " - ");
	}
	
	if (obj->proto_script) {
		strcat(buf, "[TRIG] ");
	}

	if (IN_ROOM(obj)) {
		if (HAS_ABILITY(ch, ABIL_NAVIGATION)) {
			sprintf(buf + strlen(buf), "(%*d, %*d) %s\r\n", X_PRECISION, X_COORD(IN_ROOM(obj)), Y_PRECISION, Y_COORD(IN_ROOM(obj)), get_room_name(IN_ROOM(obj), FALSE));
		}
		else {
			sprintf(buf + strlen(buf), "%s\r\n", get_room_name(IN_ROOM(obj), FALSE));
		}
		
		send_to_char(buf, ch);
	}
	else if (obj->carried_by) {
		sprintf(buf + strlen(buf), "carried by %s\r\n", PERS(obj->carried_by, ch, 1));
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
	register char_data *i;
	register obj_data *k;
	descriptor_data *d;
	int num = 0, found = 0;

	if (!*arg) {
		send_to_char("Players\r\n-------\r\n", ch);
		for (d = descriptor_list; d; d = d->next) {
			if (STATE(d) == CON_PLAYING) {
				i = (d->original ? d->original : d->character);
				if (i && CAN_SEE(ch, i) && IN_ROOM(i)) {
					if (d->original) {
						msg_to_char(ch, "%-20s - (%*d, %*d) %s (in %s)\r\n", GET_NAME(i), X_PRECISION, X_COORD(IN_ROOM(d->character)), Y_PRECISION, Y_COORD(IN_ROOM(d->character)), get_room_name(IN_ROOM(d->character), FALSE), GET_NAME(d->character));
					}
					else {
						msg_to_char(ch, "%-20s - (%*d, %*d) %s\r\n", GET_NAME(i), X_PRECISION, X_COORD(IN_ROOM(i)), Y_PRECISION, Y_COORD(IN_ROOM(i)), get_room_name(IN_ROOM(i), FALSE));
					}
				}
			}
		}
	}
	else {
		for (i = character_list; i; i = i->next) {
			if (CAN_SEE(ch, i) && IN_ROOM(i) && multi_isname(arg, GET_PC_NAME(i))) {
				found = 1;
				msg_to_char(ch, "M%3d. %-25s - %s(%*d, %*d) %s\r\n", ++num, GET_NAME(i), (IS_NPC(i) && i->proto_script) ? "[TRIG] " : "", X_PRECISION, X_COORD(IN_ROOM(i)), Y_PRECISION, Y_COORD(IN_ROOM(i)), get_room_name(IN_ROOM(i), FALSE));
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

ACMD(do_exits) {
	char *get_room_name(room_data *room, bool color);
	
	struct room_direction_data *ex;
	room_data *room, *to_room;

	if (subcmd == -1) {
		room = IN_ROOM(ch);
	}
	else {
		room = real_room(subcmd);
	}

	if (AFF_FLAGGED(ch, AFF_BLIND)) {
		msg_to_char(ch, "You can't see a damned thing, you're blind!\r\n");
		return;
	}
	if (COMPLEX_DATA(room) && ROOM_IS_CLOSED(room)) {
		*buf = '\0';
		for (ex = COMPLEX_DATA(room)->exits; ex; ex = ex->next) {
			if ((to_room = ex->room_ptr) && !EXIT_FLAGGED(ex, EX_CLOSED)) {
				sprintf(buf2, "%-5s - ", dirs[get_direction_for_char(ch, ex->dir)]);
				if (!CAN_SEE_IN_DARK_ROOM(ch, to_room)) {
					strcat(buf2, "Too dark to tell\r\n");
				}
				else {
					if (IS_IMMORTAL(ch) && PRF_FLAGGED(ch, PRF_ROOMFLAGS)) {
						sprintf(buf2 + strlen(buf2), "[%d] %s (%d, %d)\r\n", GET_ROOM_VNUM(to_room), get_room_name(to_room, FALSE), X_COORD(to_room), Y_COORD(to_room));
					}
					else if (HAS_ABILITY(ch, ABIL_NAVIGATION) && (HOME_ROOM(to_room) == to_room || !ROOM_IS_CLOSED(to_room))) {
						sprintf(buf2 + strlen(buf2), "%s (%d, %d)\r\n", get_room_name(to_room, FALSE), X_COORD(to_room), Y_COORD(to_room));
					}
					else {
						sprintf(buf2 + strlen(buf2), "%s\r\n", get_room_name(to_room, FALSE));
					}
				}
				strcat(buf, CAP(buf2));
			}
		}
		msg_to_char(ch, "Obvious exits:\r\n%s", *buf ? buf : "None.\r\n");
	}
	else {
		// out on the map?
		look_at_room(ch);
	}
}


ACMD(do_scan) {
	int dir;
	
	room_data *use_room = get_map_location_for(IN_ROOM(ch));
	
	skip_spaces(&argument);
	
	if (!*argument) {
		msg_to_char(ch, "Scan which direction?\r\n");
	}
	else if (IS_ADVENTURE_ROOM(use_room) || ROOM_IS_CLOSED(use_room)) {	// check map room
		msg_to_char(ch, "You can only use scan out on the map.\r\n");
	}
	else if (!GET_BOAT(IN_ROOM(ch)) && (IS_ADVENTURE_ROOM(IN_ROOM(ch)) || ROOM_IS_CLOSED(IN_ROOM(ch)))) {
		// if not on a boat, can't see out from here
		msg_to_char(ch, "Scan only works out on the map.\r\n");
	}
	else if ((dir = parse_direction(ch, argument)) == NO_DIR) {
		msg_to_char(ch, "Invalid direction.\r\n");
	}
	else if (dir >= NUM_2D_DIRS) {
		msg_to_char(ch, "You can't scan that way.\r\n");
	}
	else {
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
