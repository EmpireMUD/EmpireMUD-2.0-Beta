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
*   Storage Regions
*   World Storage
*/

// external vars
extern struct city_metadata_type city_type[];
extern struct world_storage *world_storage_list;

// external functions
void scale_item_to_level(obj_data *obj, int level);


 //////////////////////////////////////////////////////////////////////////////
//// HELPERS /////////////////////////////////////////////////////////////////


 //////////////////////////////////////////////////////////////////////////////
//// STORAGE REGIONS /////////////////////////////////////////////////////////

/**
* Deletes a city storage region and frees up all its data. This will
* un-associate any local storage to that region. After this, all local storage
* should have no city and all cities should have no region. This is called
* before setting up new regions.
*
* This will free the 'region'.
*
* @param empire_data *emp The empire whose region you're deleting.
* @param struct city_storage_region *region The region to delete.
*/
void delete_city_storage_region(empire_data *emp, struct city_storage_region *region) {
	struct empire_city_data *city, *next_city;
	struct local_storage *local, *next_local;
	struct city_local_assoc *cla;
	
	if (!emp || !region) {
		return;	// no work
	}
	
	// remove cities from the region
	LL_FOREACH_SAFE2(region->cities, city, next_city, next_in_storage_region) {
		city->storage_region = NULL;
		city->next_in_storage_region = NULL;
	}
	region->cities = NULL;
	
	// clean up city-local-assocs
	while ((cla = region->storage)) {
		region->storage = cla->next;
		
		// remove local storage from this city
		LL_FOREACH_SAFE2(cla->local, local, next_local, next_in_city) {
			local->city = NULL;
			local->next_in_city = NULL;
		}
		
		free(cla);
	}
	
	free(region);
}


/**
* Finds a matching city_local_assoc entry. This will also attempt to create
* one, if requested, but this cannot be guaranteed.
*
* @param struct city_local_assoc **list A pointer to the list to search.
* @param obj_vnum vnum The item you're looking for in the list.
* @param bool create_if_missing If TRUE, will attempt to add a new entry if needed (not guaranteed).
* @return struct city_local_assoc* A pointer to the entry in the *list, if available.
*/
struct city_local_assoc *find_city_local_assoc(struct city_local_assoc **list, obj_vnum vnum, bool create_if_missing) {
	struct city_local_assoc *cla;
	
	// only way to fail is if one of these is missing
	if (!list || vnum == NOTHING) {
		return NULL;
	}
	
	LL_FOREACH(*list, cla) {
		if (cla->local && cla->local->parent->vnum == vnum) {
			return cla;	// found one!
		}
	}
	
	// not found?
	if (create_if_missing) {
		CREATE(cla, struct city_local_assoc, 1);
		LL_PREPEND(*list, cla);
		return cla;
	}
	else {
		return NULL;	// no find
	}
}


/**
* Builds (or deletes and re-builds) the storage regions for an empire. This
* measures the distances between all cities, collects local data for storage,
* and computes city totals.
*
* @param empire_data *emp The empire to build storage regions for.
*/
void build_city_storage_regions(empire_data *emp) {
	struct empire_city_data *city, *other, *iter;
	struct empire_storage *estore, *next_estore;
	struct local_storage *local, *next_local;
	struct city_storage_region *region;
	struct city_local_assoc *cla;
	room_data *room;
	bool found;
	int dist;
	
	double outskirts_mod = config_get_double("outskirts_modifier");
	
	if (!emp || !EMPIRE_CITY_LIST(emp)) {
		return;	// no work
	}
	
	// free up any old storage regions
	LL_FOREACH(EMPIRE_CITY_LIST(emp), city) {
		if (city->storage_region) {
			delete_city_storage_region(emp, city->storage_region);
			city->storage_region = NULL;
		}
	}
	
	// create new storage regions for each city
	LL_FOREACH(EMPIRE_CITY_LIST(emp), city) {
		CREATE(region, struct city_storage_region, 1);
		region->cities = city;
		city->storage_region = region;
		city->next_in_storage_region = NULL;
	}
	
	// repeatedly look for overlapping storage regions and reduce them
	do {
		found = FALSE;	// 'found' will indicate we merged something and must check again
		
		// check each city
		LL_FOREACH(EMPIRE_CITY_LIST(emp), city) {
			// is it part of any other city's storage region?
			LL_FOREACH(EMPIRE_CITY_LIST(emp), other) {
				if (other == city || other->storage_region == city->storage_region) {
					continue;	// skip self / skip same storage region
				}
				if (GET_ISLAND_ID(other->location) != GET_ISLAND_ID(city->location)) {
					continue;	// different islands
				}
				
				// do their outskirts overlap
				dist = compute_distance(city->location, other->location);
				dist -= city_type[city->type].radius * outskirts_mod;
				dist -= city_type[other->type].radius * outskirts_mod;
				
				if (dist <= 0) {	// overlapping outskirts: merge!
					found = TRUE;
					region = other->storage_region;
					LL_FOREACH(EMPIRE_CITY_LIST(emp), iter) {
						if (iter->storage_region == region) {
							LL_DELETE2(region->cities, iter, next_in_storage_region);
							LL_PREPEND2(city->storage_region->cities, iter, next_in_storage_region);
							iter->storage_region = city->storage_region;
						}
					}
					
					// and delete the old region now that nothing is in it (its local storage IS empty here)
					free(region);
				}
			}
		}
	} while (found);
	
	// assign storage to its regions -- only if they have any cities (no need otherwise)
	if (EMPIRE_CITY_LIST(emp)) {
		HASH_ITER(hh, EMPIRE_STORAGE(emp), estore, next_estore) {
			// for each type of item stored to the empire
			HASH_ITER(hh, estore->local, local, next_local) {
				if (!(room = real_room(local->loc))) {
					continue;	// unknown location somehow
				}
				if (!ROOM_CITY(room) || !ROOM_CITY(room)->storage_region) {
					continue;	// not in a city -- no need to assign to a region
				}
			
				// TODO: don't assign to a city if it's stored to a vehicle that moves?
			
				// and add/update the entry
				if ((cla = find_city_local_assoc(&ROOM_CITY(room)->storage_region->storage, estore->vnum, TRUE))) {
					LL_PREPEND2(cla->local, local, next_in_city);
					local->city = ROOM_CITY(room)->storage_region;
					SAFE_ADD(cla->total, local->amount, 0, INT_MAX, FALSE);
				}
			}
		}
	}
}



/**
* Calls build_city_storage_regions() on all empires.
*/
void build_city_storage_regions_all(void) {
	empire_data *emp, *next_emp;
	
	HASH_ITER(hh, empire_table, emp, next_emp) {
		build_city_storage_regions(emp);
	}
}


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
	store->amount = amount;
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
	vehicle_data *veh;
	room_data *room;
	
	LL_FOREACH(land_map, map) {
		check_world_storage_one(&map->shared->storage);
	}
	LL_FOREACH2(interior_room_list, room, next_interior) {
		check_world_storage_one(&GET_WORLD_STORAGE(room));
	}
	LL_FOREACH(vehicle_list, veh) {
		check_world_storage_one(&VEH_WORLD_STORAGE(veh));
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


/**
* Called when a player walks into the room to unpack world-storage items back
* into the vehicle's contents.
*
* @param vehicle_data *veh The vehicle to unpack.
*/
void unpack_world_storage_veh(vehicle_data *veh) {
	struct world_storage *store, *next_store;
	obj_data *obj;
	int iter;
	
	if (!veh) {
		return;
	}
	
	LL_FOREACH_SAFE(VEH_WORLD_STORAGE(veh), store, next_store) {
		for (iter = 0; iter < store->amount; ++iter) {
			if ((obj = read_object(store->vnum, TRUE))) {
				scale_item_to_level(obj, store->level);
				GET_OBJ_TIMER(obj) = store->timer;
				obj_to_vehicle(obj, veh);
				
				// do NOT call the load trigger: it's not fresh-loaded
			}
		}
		
		// and clean up data
		LL_DELETE(VEH_WORLD_STORAGE(veh), store);
		LL_DELETE2(world_storage_list, store, next_global);
		free(store);
	}
}


/**
* Writes world-storage data to a map/vehicle/room file.
*
* @param char *header The leading text, e.g. "V" for a world entry or "Storage:" for a vehicle file.
* @param FILE *fl The file, open for writing.
* @param struct world_storage *list The list to write.
*/
void write_world_storage(char *header, FILE *fl, struct world_storage *list) {
	struct world_storage *store;
	
	if (!header || !fl || !list) {
		return;	// no work
	}
	
	LL_FOREACH(list, store) {
		if (store->amount > 0) {
			fprintf(fl, "%s %d %d %d %d\n", header, store->vnum, store->amount, store->level, store->timer);
		}
	}
}
