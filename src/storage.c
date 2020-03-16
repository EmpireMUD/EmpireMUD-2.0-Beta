/* ************************************************************************
*   File: storage.c                                       EmpireMUD 2.0b5 *
*  Usage: contains various in-game storage systems                        *
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
#include "handler.h"


/**
* Contents:
*   Helpers
*   World Storage
*/

// external vars
extern struct world_storage *world_storage_list;

// external functions
void scale_item_to_level(obj_data *obj, int level);


 //////////////////////////////////////////////////////////////////////////////
//// HELPERS /////////////////////////////////////////////////////////////////


 //////////////////////////////////////////////////////////////////////////////
//// WORLD STORAGE ///////////////////////////////////////////////////////////

/**
* Adds an item to world (non-empire) storage for a given room. This does NOT
* extract the item; you must extract it if you're done with it.
*
* @param struct world_storage **list The world-storage list to store it to -- usually &GET_WORLD_STORAGE(room).
* @param obj_vnum vnum The item to store.
* @param int amount How many to store.
* @param int level What level the item is scaled to.
* @param int timer The timer on the object, if any.
* @return struct world_storage* The storage data where it was stored.
*/
struct world_storage *add_to_world_storage(struct world_storage **list, obj_vnum vnum, int amount, int level, int timer) {
	struct world_storage *store;
	
	if (!list || vnum == NOTHING) {
		return NULL;	// basic safety
	}
	
	LL_FOREACH(*list, store) {
		if (vnum == store->vnum && level == store->level && timer == store->timer) {
			SAFE_ADD(store->amount, amount, 0, INT_MAX, FALSE);
			return store;
		}
	}
	
	// if we didn't find it, create it
	CREATE(store, struct world_storage, 1);
	store->vnum = vnum;
	store->amount = 1;
	store->level = level;
	store->timer = timer;
	LL_PREPEND(*list, store);
	LL_PREPEND2(world_storage_list, store, next_global);
	return store;
}


/**
* Checks one world-storage list for items that don't exist and deletes them.
* It also prunes entires with zero quantity.
*
* @param struct world_storage **list A pointer to the list to clean up.
*/
void check_world_storage_one(struct world_storage **list) {
	struct world_storage *store, *next_store;
	
	if (list) {
		LL_FOREACH_SAFE(*list, store, next_store) {
			if (store->amount <= 0 || obj_proto(store->vnum) == NULL) {
				LL_DELETE(*list, store);
				LL_DELETE2(world_storage_list, store, next_global);
				free(store);
			}
		}
	}
}


/**
* Called on startup to ensure no bad items are in any world-storage data.
*/
void check_world_storage(void) {
	struct map_data *map;
	room_data *room;
	
	LL_FOREACH(land_map, map) {
		check_world_storage_one(&map->shared->storage);
	}
	LL_FOREACH2(interior_room_list, room, next_interior) {
		check_world_storage_one(&GET_WORLD_STORAGE(room));
	}
}


/**
* Called when a player enters a room to return world-storage items to the room
* before the player sees them. World storage is different from empire storage
* and mainly serves to clean up memory.
*
* @param room_data *room The room to unpack, if anything is world-stored there.
*/
void unpack_world_storage(room_data *room) {
	struct world_storage *store, *next_store;
	obj_data *obj;
	int iter;
	
	if (!room) {
		return;
	}
	
	LL_FOREACH_SAFE(GET_WORLD_STORAGE(room), store, next_store) {
		for (iter = 0; iter < store->amount; ++iter) {
			if ((obj = read_object(store->vnum, TRUE))) {
				scale_item_to_level(obj, store->level);
				GET_OBJ_TIMER(obj) = store->timer;
				obj_to_room(obj, room);
				
				// do NOT call the load trigger: it's not fresh-loaded
			}
		}
		
		// and clean up data
		LL_DELETE(GET_WORLD_STORAGE(room), store);
		LL_DELETE2(world_storage_list, store, next_global);
		free(store);
	}
	
	// don't bother marking the world as needing a save -- it just still has them saved as world-stored
}
