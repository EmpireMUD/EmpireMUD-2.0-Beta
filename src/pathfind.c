/* ************************************************************************
*   File: pathfind.c                                      EmpireMUD 2.0b5 *
*  Usage: code for detecting paths between two locations                  *
*                                                                         *
*  EmpireMUD code base by Paul Clarke, (C) 2000-2024                      *
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
#include "constants.h"

/**
* Contents:
*   Validators
*   Helpers
*   Pathfinding Core
*/

 //////////////////////////////////////////////////////////////////////////////
//// VALIDATORS //////////////////////////////////////////////////////////////

/**
* Validators validate a location for the pathfinding system.
*
* This must be implemented for both 'room' and 'map', as only 1 of those is set
* each time it's called.
*
* #define PATHFIND_VALIDATOR(name)  bool (name)(room_data *room, struct map_data *map, char_data *ch, vehicle_data *veh, struct pathfind_controller *controller)
*
* @param room_data *room Optional: An interior room (if this is NULL, map will be set instead).
* @param struct map_data *map Optional: A map room if outdoors (if this is NULL, room will be set instead).
* @param char_data *ch Optional: Player trying to find the path (may be NULL).
* @param vehicle_data *veh Optional: Vehicle trying to find the paath (may be NULL).
* @param struct pathfind_controller *controller The pathfinding controller and all its data.
* @return bool TRUE if the room/map is ok, FALSE if not.
*/

#define CHAR_OR_VEH_ROOM_PERMISSION(ch, veh, room, mode)  (!ROOM_OWNER(room) || ((ch) && can_use_room((ch), (room), (mode))) || ((veh) && VEH_OWNER(veh) && emp_can_use_room(VEH_OWNER(veh), (room), (mode))))
#define CHAR_OR_VEH_ROOM_PERMISSION_SIMPLE(ch, veh, room, junk)  (!ROOM_OWNER(room) || ((ch) && GET_LOYALTY(ch) == ROOM_OWNER(room)) || ((veh) && VEH_OWNER(veh) == ROOM_OWNER(room)))

// example: validator for ships
PATHFIND_VALIDATOR(pathfind_ocean) {
	room_data *find;
	
	if (room) {
		if (ROOM_SECT_FLAGGED(room, SECTF_FRESH_WATER | SECTF_OCEAN) || ROOM_BLD_FLAGGED(room, BLD_SAIL)) {
			if (!ROOM_IS_CLOSED(room)) {
				return TRUE;	// open
			}
			else if (CHAR_OR_VEH_ROOM_PERMISSION(ch, veh, room, GUESTS_ALLOWED)) {
				return TRUE;	// allowed in
			}
		}
	}
	else if (map) {
		if (SECT_FLAGGED(map->sector_type, SECTF_FRESH_WATER | SECTF_OCEAN)) {
			return TRUE;	// true ocean
		}
		else if ((find = map->room) && ROOM_BLD_FLAGGED(find, BLD_SAIL)) {
			if (!ROOM_IS_CLOSED(find)) {
				return TRUE;	// open
			}
			else if (CHAR_OR_VEH_ROOM_PERMISSION(ch, veh, find, GUESTS_ALLOWED)) {
				return TRUE;	// allowed in
			}
		}
	}
	
	return FALSE;	// all other cases
}


// example: validator for piloting air vehicles
PATHFIND_VALIDATOR(pathfind_pilot) {
	room_data *find;
	
	if (room) {
		if (ROOM_AFF_FLAGGED(room, ROOM_AFF_NO_FLY)) {
			return FALSE;	// cannot fly there
		}
		if (!ROOM_IS_CLOSED(room) || (ROOM_BLD_FLAGGED(room, BLD_ATTACH_ROAD) && CHAR_OR_VEH_ROOM_PERMISSION(ch, veh, room, GUESTS_ALLOWED))) {
			return TRUE;	// free to pass
		}
	}
	else if (map) {
		if (IS_SET(map->shared->affects, ROOM_AFF_NO_FLY)) {
			return FALSE;	// can't fly there
		}
		if (!(find = map->room)) {
			return TRUE;	// no real-real-room means not a building so we're ok now
		}
		if (!ROOM_IS_CLOSED(find) || (ROOM_BLD_FLAGGED(find, BLD_ATTACH_ROAD) && CHAR_OR_VEH_ROOM_PERMISSION(ch, veh, find, GUESTS_ALLOWED))) {
			return TRUE; // free to pass
		}
	}
	
	return FALSE;	// all other cases?
}


// example: validator for following roads
PATHFIND_VALIDATOR(pathfind_road) {
	room_data *find;
	
	if (room) {
		if (IS_ROAD(room) || room == controller->end) {
			return TRUE;	// real road
		}
		else if (!ROOM_BLD_FLAGGED(room, BLD_ATTACH_ROAD)) {
			return FALSE;	// not a road-building
		}
		else if (!ROOM_IS_CLOSED(room) || CHAR_OR_VEH_ROOM_PERMISSION(ch, veh, room, GUESTS_ALLOWED)) {
			return TRUE;	// open-road building or closed and free to pass
		}
	}
	else if (map) {
		if (SECT_FLAGGED(map->sector_type, SECTF_IS_ROAD) || map->vnum == GET_ROOM_VNUM(controller->end)) {
			return TRUE;	// true road
		}
		else if (!(find = map->room) || !ROOM_BLD_FLAGGED(find, BLD_ATTACH_ROAD)) {
			return FALSE;	// not a building that we can use
		}
		else if (!ROOM_IS_CLOSED(find) || CHAR_OR_VEH_ROOM_PERMISSION(ch, veh, find, GUESTS_ALLOWED)) {
			return TRUE;	// open-road building or closed and free to pass
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
* @return struct pathfind_node* The new node, if added. (May be NULL.)
*/
struct pathfind_node *add_pathfind_node(struct pathfind_controller *controller, room_data *inside_room, struct map_data *map_tile, struct pathfind_node *from_node, int dir) {
	struct pathfind_node *node, *iter;
	static double sqrt2 = 0.0;
	bool found;
	
	// compute this once (for diagonal moves)
	if (sqrt2 < 1.0) {
		sqrt2 = sqrt(2.0);
	}
	
	if (!controller || (!inside_room && !map_tile)) {
		return NULL;	// no work?
	}
	
	CREATE(node, struct pathfind_node, 1);
	
	// store locations
	if (map_tile) {
		node->map_loc = map_tile;
		node->estimate = compute_map_distance(MAP_X_COORD(map_tile->vnum), MAP_Y_COORD(map_tile->vnum), controller->end_x, controller->end_y) + (from_node ? from_node->steps : 0.0) + 1.0;
	}
	else if (inside_room) {
		node->inside_room = inside_room;
		node->estimate = compute_map_distance(X_COORD(inside_room), Y_COORD(inside_room), controller->end_x, controller->end_y) + (from_node ? from_node->steps : 0.0) + 1.0;
	}
	
	if (from_node) {
		node->parent = from_node;
		node->steps = from_node->steps + (dir < NUM_SIMPLE_DIRS ? 1.0 : sqrt2);
		node->cur_dir = from_node->cur_dir;
		node->cur_dist = from_node->cur_dist;
	}
	else {
		node->steps = (dir < NUM_SIMPLE_DIRS ? 1.0 : sqrt2);
		node->cur_dir = NO_DIR;
	}
	
	// check if it changed directions
	if (dir != node->cur_dir) {
		// reset
		node->cur_dir = dir;
		node->cur_dist = 1;
	}
	else if (dir != NO_DIR) {	// same direction
		++(node->cur_dist);
	}
		
	// insert in-order
	found = FALSE;
	DL_FOREACH(controller->nodes, iter) {
		if (node->estimate < iter->estimate) {
			DL_PREPEND_ELEM(controller->nodes, iter, node);
			found = TRUE;
			break;
		}
	}
	if (!found) {
		DL_APPEND(controller->nodes, node);
	}
	
	return node;
}


/**
* Builds a pathfinding string from a set of nodes, starting from the successful
* end node.
*
* @param struct pathfind_node *end_node The final node in the path.
* @param char *buffer A string buffer of at least MAX_ACTION_STRING+1 in length.
* @return bool TRUE if it managed to build a string, even an empty one. FALSE if the string ended up too long.
*/
bool build_pathfind_string(struct pathfind_node *end_node, char *buffer) {
	struct pathfind_node *node;
	int last_dir = NO_DIR;
	bool full = FALSE;
	int size;
	
	struct bps_string_part {
		char str[16];	// one direction (!)
		byte size;
		struct bps_string_part *next;	// linked list
	} *bps, *next_bps, *bps_list = NULL;
	
	// init string
	*buffer = '\0';
	size = 0;
	
	// build parts in reverse order (since we can only go from end_node->parent)
	for (node = end_node; node; node = node->parent) {
		// detect dir changes; we only need the distances on those
		if (node->cur_dir != last_dir && node->cur_dir != NO_DIR) {
			last_dir = node->cur_dir;
			
			CREATE(bps, struct bps_string_part, 1);
			bps->size = (byte)snprintf(bps->str, sizeof(bps->str), "%d%s", node->cur_dist, alt_dirs[node->cur_dir]);
			
			// now PREPEND it to the list, since we're building backwards
			LL_PREPEND(bps_list, bps);
		}
	}
	
	// now build the final string
	LL_FOREACH_SAFE(bps_list, bps, next_bps) {
		if (!full && size + bps->size < MAX_ACTION_STRING) {
			strcat(buffer, bps->str);
		}
		else {
			full = TRUE;
		}
		free(bps);
	}
	
	// clear the string (no valid string) if full
	if (full) {
		*buffer = '\0';
	}
	
	return !full;
}


/**
* @param struct pathfind_controller *pc The controller to free.
*/
void free_pathfind_controller(struct pathfind_controller *pc) {
	struct pathfind_node *node, *next;
	
	DL_FOREACH_SAFE(pc->nodes, node, next) {
		free(node);
	}
	
	DL_FOREACH_SAFE(pc->free_nodes, node, next) {
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
		DL_FOREACH2(interior_room_list, room, next_interior) {
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
* MAX_ACTION_STRING (the longest string that can be saved to file).
*
* @param room_data *start The start room.
* @param room_data *end The end room.
* @param char_data *ch Optional: Player trying to find the path (may be NULL).
* @param vehicle_data *veh Optional: Vehicle trying to find the paath (may be NULL).
* @param PATHFIND_VALIDATOR(*validator) Function pointer for validating each room.
* @return char* A movement string (like "2n3w") or NULL if not found.
*/
char *get_pathfind_string(room_data *start, room_data *end, char_data *ch, vehicle_data *veh, PATHFIND_VALIDATOR(*validator)) {
	struct pathfind_controller *controller;
	static char output[MAX_STRING_LENGTH];
	struct pathfind_node *node, *end_node;
	unsigned long long start_time;
	struct map_data *to_map;
	room_data *to_room;
	int dir, count;
	
	// shortcut for safety
	if (!start || !end || start == end || !validator) {
		return NULL;
	}
	
	*output = '\0';
	
	// make the controller
	CREATE(controller, struct pathfind_controller, 1);
	controller->start = start;
	controller->end = end;
	controller->end_x = X_COORD(end);
	controller->end_y = Y_COORD(end);
	controller->key = get_pathfind_key();
	
	start_time = microtime();
	
	// if it couldn't pathfind to the end, bug out early rather than waste time
	if (!validator(end, NULL, ch, veh, controller)) {
		free_pathfind_controller(controller);
		return NULL;
	}
	
	// start pathing
	if (GET_ROOM_VNUM(start) < MAP_SIZE && !ROOM_IS_CLOSED(start) && GET_MAP_LOC(start)) {	// on the map
		add_pathfind_node(controller, NULL, GET_MAP_LOC(start), NULL, NO_DIR);
	}
	else {	// inside
		add_pathfind_node(controller, start, NULL, NULL, NO_DIR);
	}
	
	end_node = NULL;
	count = 0;
	
	// do the thing
	while ((node = controller->nodes) && !end_node) {
		// pop node off and move it to the free_nodes list now
		DL_DELETE(controller->nodes, node);
		DL_PREPEND(controller->free_nodes, node);
		
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
				end_node = add_pathfind_node(controller, to_room, to_map, node, dir);
				break;
			}
		
			// ok: mark it then check the validator
			if (to_room) {
				ROOM_PATHFIND_KEY(to_room) = controller->key;
			}
			else if (to_map) {
				to_map->pathfind_key = controller->key;
			}
		
			if (!validator(to_room, to_map, ch, veh, controller)) {
				continue;	// failed validator
			}
		
			// ok: queue it
			add_pathfind_node(controller, to_room, to_map, node, dir);
		}
		
		// check time limit every 500 nodes: stop after 0.5 seconds
		if ((++count % 500) == 0) {
			if (microtime() - start_time > 500000) {
				break;
			}
		}
	}
	
	// did we find the end?
	if (end_node) {
		build_pathfind_string(end_node, output);
	}
	
	// free the controller (and any remaining nodes)
	free_pathfind_controller(controller);
	
	return *output ? output : NULL;
}
