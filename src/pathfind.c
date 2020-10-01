/* ************************************************************************
*   File: pathfind.c                                      EmpireMUD 2.0b5 *
*  Usage: code for detecting paths between two locations                  *
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
#include "interpreter.h"
#include "db.h"
#include "comm.h"
#include "handler.h"
#include "vnums.h"

/**
* Contents:
*   Validators
*   Helpers
*   Pathfinding Core
*/

// external vars
extern const char *alt_dirs[];
extern const int rev_dir[];

// local protos
bool append_move_string(char *string, int dir, int dist);
bool pathfind_get_dir(room_data *from_room, struct map_data *from_map, int dir, room_data **to_room, struct map_data **to_map);


 //////////////////////////////////////////////////////////////////////////////
//// VALIDATORS //////////////////////////////////////////////////////////////

/**
* Validators validate a location for the pathfinding system.
*
* This must be implemented for both 'room' and 'map', as only 1 of those is set
* each time it's called.
*
* #define PATHFIND_VALIDATOR(name)  bool (name)(room_data *room, struct map_data *map, struct pathfind_controller *controller)
*
* @param room_data *room Optional: An interior room (if this is NULL, map will be set instead).
* @param struct map_data *map Optional: A map room if outdoors (if this is NULL, room will be set instead).
* @param struct pathfind_controller *controller The pathfinding controller and all its data.
* @return bool TRUE if the room/map is ok, FALSE if not.
*/


// example: validator for following roads
PATHFIND_VALIDATOR(pathfind_road) {
	room_data *find;
	
	if (room) {
		return (IS_ROAD(room) || ROOM_BLD_FLAGGED(room, BLD_ATTACH_ROAD)) ? TRUE : FALSE;
	}
	else if (map) {
		if (SECT_FLAGGED(map->sector_type, SECTF_IS_ROAD)) {
			return TRUE;	// true road
		}
		else if ((find = real_real_room(map->vnum)) && ROOM_BLD_FLAGGED(find, BLD_ATTACH_ROAD)) {
			return TRUE;	// attach-road
		}
	}
	
	return FALSE;	// all other cases
}


 //////////////////////////////////////////////////////////////////////////////
//// HELPERS /////////////////////////////////////////////////////////////////

/**
* @param struct pathfind_controller *controller The pathfind controller.
* @param room_data *inside_room Optional: If it's an interior, this is the room. (NULL if exterior)
* @param struct map_data *map_tile Optional: If it's on the map, this is the loc. (NULL if interior)
* @oaram int dir Optional: Which direction from the previous tile. (NO_DIR for the first tile)
* @param struct pathfind_node *from_node Optional: The previous node on the path -- will copy its path-so-far (may be NULL for the first tile)
*/
void add_pathfind_node(struct pathfind_controller *controller, room_data *inside_room, struct map_data *map_tile, struct pathfind_node *from_node, int dir) {
	struct pathfind_node *node;
	
	if (!controller || (!inside_room && !map_tile)) {
		return;	// no work?
	}
	
	CREATE(node, struct pathfind_node, 1);
	
	// store locations
	if (map_tile) {
		node->map_loc = map_tile;
	}
	else if (inside_room) {
		node->inside_room = inside_room;
	}
	
	if (from_node) {
		node->cur_dir = from_node->cur_dir;
		node->cur_dist = from_node->cur_dist;
		strcpy(node->string, from_node->string);
	}
	else {
		node->cur_dir = NO_DIR;
		*(node->string) = '\0';
	}
	
	// check if it changed directions
	if (dir != node->cur_dir) {
		if (!append_move_string(node->string, node->cur_dir, node->cur_dist)) {
			// path too long -- this path fails
			free(node);
			return;
		}
		
		// reset
		node->cur_dir = dir;
		node->cur_dist = 1;
	}
	else if (dir != NO_DIR) {	// same direction
		++(node->cur_dist);
	}
	
	// and append
	DL_APPEND(controller->nodes, node);
}


/**
* Dumps a dir/dist to the end of a movement string.
*
* @param char *string The current move string, in a buffer at least MAX_MOVEMENT_STRING long.
* @param int dir The direction to add.
* @param int dist The distance to add.
* @return bool TRUE if we successfully added, FALSE if the string was full.
*/
bool append_move_string(char *string, int dir, int dist) {
	char temp[16];
	size_t add;
	
	// prepare string
	if (dir != NO_DIR) {
		add = snprintf(temp, sizeof(temp), "%d%s", dist, alt_dirs[dir]);
	}
	else {
		add = 0;
		*temp = '\0';
	}
	
	if (strlen(string) + add > MAX_MOVEMENT_STRING) {
		return FALSE;
	}
	else {
		strcat(string, temp);
		return TRUE;
	}
}


/**
* @param struct pathfind_controller *pc The controller to free.
*/
void free_pathfind_controller(struct pathfind_controller *pc) {
	struct pathfind_node *node, *next;
	
	DL_FOREACH_SAFE(pc->nodes, node, next) {
		free(node);
	}
	
	free(pc);
}


/**
* This rotates through 255 keys to avoid having to clear the search mark on
* map tiles. After 255, it clears the whole map and then starts over with key
* #1. These are used like the BFS mark in stock CircleMUD: they tell the path-
* finding algorithm a tile/room has been hit already.
*
* @return ubyte The next key for pathfinding.
*/
ubyte get_pathfind_key(void) {
	static ubyte top_pathfind_key = 0;
	room_data *room;
	int x, y;
	
	if (top_pathfind_key == UCHAR_MAX) {
		top_pathfind_key = 0;
		
		log("Resetting pathfinding key (get_pathfind_key)...");
		
		// reset all rooms
		LL_FOREACH2(interior_room_list, room, next_interior) {
			ROOM_PATHFIND_KEY(room) = 0;
		}
		// reset map
		for (x = 0; x < MAP_WIDTH; ++x) {
			for (y = 0; y < MAP_HEIGHT; ++y) {
				world_map[x][y].pathfind_key = 0;
			}
		}
	}
	
	return ++top_pathfind_key;
}


/**
* For use in pathfinding, take EITHER a from-room or from-map and determines if
* there's anything in a given direction from it, returning a room if it's an
* interior or a map tile on the map (to avoid loading a room into RAM).
*
* @param room_data *from_room A room to start from (optional; may pass from_map instead).
* @param struct map_data *from_map A map tile to start from (optional; may pass from_room instead).
* @param int dir The direction to check.
* @param room_data **to_room A variable to bind the found room to, if any (and only if it's an interior).
* @param struct map_data **to_map A variable to bind the found map tile to, if any (and only if the target room is on the map).
* @return bool TRUE if it found either to_room or to_map; FALSE if not.
*/
bool pathfind_get_dir(room_data *from_room, struct map_data *from_map, int dir, room_data **to_room, struct map_data **to_map) {
	struct room_direction_data *ex;
	room_data *room;
	int x, y;
	
	// init
	*to_room = NULL;
	*to_map = NULL;
	
	if (from_room) {	// interior
		if ((ex = find_exit(from_room, dir)) && ex->room_ptr && !EXIT_FLAGGED(ex, EX_CLOSED)) {
			// found:
			if (GET_ROOM_VNUM(ex->room_ptr) < MAP_SIZE && !ROOM_IS_CLOSED(ex->room_ptr) && GET_MAP_LOC(ex->room_ptr)) {
				*to_map = GET_MAP_LOC(ex->room_ptr);
			}
			else {
				*to_room = ex->room_ptr;
			}
			return TRUE;
		}
		// else: did not find an exit
	}
	if (from_map && dir < NUM_2D_DIRS) {
		if (get_coord_shift(MAP_X_COORD(from_map->vnum), MAP_Y_COORD(from_map->vnum), shift_dir[dir][0], shift_dir[dir][1], &x, &y)) {
			if (SECT_FLAGGED(world_map[x][y].sector_type, SECTF_MAP_BUILDING) && (room = real_room(world_map[x][y].vnum)) && ROOM_IS_CLOSED(room)) {
				*to_room = room;
			}
			else {
				*to_map = &world_map[x][y];
			}
			return TRUE;
		}
		// else: probably edge of map
	}
	
	return FALSE;	// if we got this far
}


 //////////////////////////////////////////////////////////////////////////////
//// PATHFINDING CORE ////////////////////////////////////////////////////////

/**
* Attempts to find a path between two rooms. Warning: this could get very slow
* on a large map if it does a lot of calculations or allows any tile. It does
* not take player abilities (e.g. Mountainclimbing) or move costs into account;
* it builds a movement string (like "2n3w") for running/sailing.
*
* This will fail if the movement string it builds is somehow longer than
* MAX_MOVEMENT_STRING (the longest string that can be saved to file).
*
* @param room_data *start The start room.
* @param room_data *end The end room.
* @param PATHFIND_VALIDATOR(*validator) Function pointer for validating each room.
* @return char* A movement string (like "2n3w") or NULL if not found.
*/
char *get_pathfind_string(room_data *start, room_data *end, PATHFIND_VALIDATOR(*validator)) {
	struct pathfind_controller *controller;
	static char output[MAX_STRING_LENGTH];
	struct pathfind_node *node, *end_node;
	struct map_data *to_map;
	room_data *to_room;
	int dir, end_dir;
	bool success;
	
	// shortcut for safety
	if (!start || !end || start == end || !validator) {
		return NULL;
	}
	
	*output = '\0';
	
	// make the controller
	CREATE(controller, struct pathfind_controller, 1);
	controller->start = start;
	controller->end = end;
	controller->key = get_pathfind_key();
	
	// start pathing
	if (GET_ROOM_VNUM(start) < MAP_SIZE && !ROOM_IS_CLOSED(start) && GET_MAP_LOC(start)) {	// on the map
		add_pathfind_node(controller, NULL, GET_MAP_LOC(start), NULL, NO_DIR);
	}
	else {	// inside
		add_pathfind_node(controller, start, NULL, NULL, NO_DIR);
	}
	
	end_node = NULL;
	end_dir = NO_DIR;
	
	// do the thing
	while ((node = controller->nodes) && !end_node) {
		// pop it
		DL_DELETE(controller->nodes, node);
		
		// check dirs from this node
		for (dir = 0; dir < (node->inside_room ? NUM_NATURAL_DIRS : NUM_2D_DIRS) && !end_node; ++dir) {
			// preliminary checks
			if (!pathfind_get_dir(node->inside_room, node->map_loc, dir, &to_room, &to_map)) {
				continue;	// nothing in that dir
			}
			if (to_room && ROOM_PATHFIND_KEY(to_room) == controller->key) {
				continue;	// already checked: inside
			}
			if (to_map && to_map->pathfind_key == controller->key) {
				continue;	// already checked: map
			}
			if (to_room && ROOM_IS_CLOSED(to_room) && (!node->inside_room || !ROOM_IS_CLOSED(node->inside_room)) && dir != BUILDING_ENTRANCE(to_room) && (!ROOM_BLD_FLAGGED(to_room, BLD_TWO_ENTRANCES) || dir != rev_dir[BUILDING_ENTRANCE(to_room)])) {
				continue;	// invalid entrance dir for map building
			}
			
			// ok: is it the end loc?
			if ((to_room && to_room == end) || (to_map && to_map->vnum == GET_ROOM_VNUM(end))) {
				end_node = node;
				end_dir = dir;
				break;
			}
			
			// ok: mark it then check the validator
			if (to_room) {
				ROOM_PATHFIND_KEY(to_room) = controller->key;
			}
			else if (to_map) {
				to_map->pathfind_key = controller->key;
			}
			
			if (!validator(to_room, to_map, controller)) {
				continue;	// failed validator
			}
			
			// ok: queue it
			add_pathfind_node(controller, to_room, to_map, node, dir);
		}
		
		// usually free here
		if (node != end_node) {
			free(node);
		}
	}
	
	// did we find the end? if so, it's no longer in controller->nodes and has not been freed
	if (end_node) {
		success = FALSE;	// unless otherwise stated
		
		if (end_dir != NO_DIR && end_dir == end_node->cur_dir) {
			// same dir
			success = append_move_string(end_node->string, end_dir, end_node->cur_dist + 1);
		}
		else if (end_dir != NO_DIR) {
			// different dir: append old dir and 
			success = append_move_string(end_node->string, end_node->cur_dir, end_node->cur_dist);
			if (success) {
				success = append_move_string(end_node->string, end_dir, 1);
			}
		}
		
		// did we make it?
		if (success) {
			strcpy(output, end_node->string);
		}
		free(end_node);
	}
	
	// free the controller (and any remaining nodes)
	free_pathfind_controller(controller);
	
	return *output ? output : NULL;
}
