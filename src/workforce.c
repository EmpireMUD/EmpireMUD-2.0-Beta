/* ************************************************************************
*   File: workforce.c                                     EmpireMUD 2.0b5 *
*  Usage: functions related to npc chores and workforce                   *
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
#include "handler.h"
#include "db.h"
#include "skills.h"
#include "vnums.h"

/**
* Contents:
*   Data
*   Master Chore Control
*   EWT Tracker
*   Helpers
*   Generic Craft Workforce
*   Chore Functions
*   Vehicle Chore Functions
*/

// for territory iteration
struct empire_territory_data *global_next_territory_entry = NULL;

// protos
void do_chore_brickmaking(empire_data *emp, room_data *room);
void do_chore_building(empire_data *emp, room_data *room, int mode);
void do_chore_chopping(empire_data *emp, room_data *room);
void do_chore_digging(empire_data *emp, room_data *room);
void do_chore_dismantle(empire_data *emp, room_data *room);
void do_chore_dismantle_mines(empire_data *emp, room_data *room);
void do_chore_einv_interaction(empire_data *emp, room_data *room, int chore, int interact_type);
void do_chore_farming(empire_data *emp, room_data *room);
void do_chore_fire_brigade(empire_data *emp, room_data *room);
void do_chore_gardening(empire_data *emp, room_data *room);
void do_chore_mining(empire_data *emp, room_data *room);
void do_chore_minting(empire_data *emp, room_data *room);
void do_chore_nailmaking(empire_data *emp, room_data *room);
void do_chore_quarrying(empire_data *emp, room_data *room);
void do_chore_shearing(empire_data *emp, room_data *room);
void do_chore_trapping(empire_data *emp, room_data *room);

void vehicle_chore_fire_brigade(empire_data *emp, vehicle_data *veh);
void vehicle_chore_repair(empire_data *emp, vehicle_data *veh);

// other locals
int empire_chore_limit(empire_data *emp, int island_id, int chore);
int sort_einv(struct empire_storage_data *a, struct empire_storage_data *b);

// external functions
void empire_skillup(empire_data *emp, any_vnum ability, double amount);	// skills.c
void stop_room_action(room_data *room, int action, int chore);	// act.action.c

// gen_craft protos:
#define CHORE_GEN_CRAFT_VALIDATOR(name)  bool (name)(empire_data *emp, room_data *room, int chore, craft_data *craft)
CHORE_GEN_CRAFT_VALIDATOR(chore_nexus_crystals);
CHORE_GEN_CRAFT_VALIDATOR(chore_milling);
CHORE_GEN_CRAFT_VALIDATOR(chore_pressing);
CHORE_GEN_CRAFT_VALIDATOR(chore_smelting);
CHORE_GEN_CRAFT_VALIDATOR(chore_weaving);

void do_chore_gen_craft(empire_data *emp, room_data *room, int chore, CHORE_GEN_CRAFT_VALIDATOR(*validator), bool is_skilled);


 /////////////////////////////////////////////////////////////////////////////
//// DATA ///////////////////////////////////////////////////////////////////

#define MIN_WORKER_POS  POS_SITTING	// minimum position for a worker to be used (otherwise it will spawn another worker)


// CHORE_x
struct empire_chore_type chore_data[NUM_CHORES] = {
	{ "building", BUILDER },
	{ "farming", FARMER },
	{ "replanting", FARMER },
	{ "chopping", FELLER },
	{ "maintenance", REPAIRMAN },
	{ "mining", MINER },
	{ "digging", DIGGER },
	{ "sawing", SAWYER },
	{ "scraping", SCRAPER },
	{ "smelting", SMELTER },
	{ "weaving", WEAVER },
	{ "quarrying", STONECUTTER },
	{ "nailmaking", NAILMAKER },
	{ "brickmaking", BRICKMAKER },
	{ "abandon-dismantled", NOTHING },
	{ "herb-gardening", GARDENER },
	{ "fire brigade", FIRE_BRIGADE },
	{ "trapping", TRAPPER },
	{ "tanning", TANNER },
	{ "shearing", SHEARER },
	{ "minting", COIN_MAKER },
	{ "dismantle-mines", BUILDER },
	{ "abandon-chopped", NOTHING },
	{ "abandon-farmed", NOTHING },
	{ "nexus-crystals", APPRENTICE_EXARCH },
	{ "milling", MILL_WORKER },
	{ "repair-vehicles", VEHICLE_REPAIRMAN },
	{ "oilmaking", PRESS_WORKER },
};


// global for which chore type might be running
int einv_interaction_chore_type = 0;


 /////////////////////////////////////////////////////////////////////////////
//// MASTER CHORE CONTROL ///////////////////////////////////////////////////


/**
* This runs once an hour on every empire room. It will try to run a chore on
* that room, if applicable. This is only called if the empire has Workforce.
*
* @param empire_data *emp the empire -- a shortcut to prevent re-detecting
* @param room_data *room
*/
void process_one_chore(empire_data *emp, room_data *room) {	
	int island = GET_ISLAND_ID(room);	// just look this up once
	
	#define CHORE_ACTIVE(chore)  (empire_chore_limit(emp, island, (chore)) != 0)
	
	// fire!
	if (BUILDING_BURNING(room) > 0 && CHORE_ACTIVE(CHORE_FIRE_BRIGADE)) {
		do_chore_fire_brigade(emp, room);
		return;
	}
	
	// wait wait don't work here
	if (ROOM_AFF_FLAGGED(room, ROOM_AFF_NO_WORK | ROOM_AFF_HAS_INSTANCE) || !check_in_city_requirement(room, TRUE)) {
		return;
	}
	
	// All choppables -- except crops, which are handled by farming
	if (CHORE_ACTIVE(CHORE_CHOPPING) && !ROOM_CROP(room) && (has_evolution_type(SECT(room), EVO_CHOPPED_DOWN) || CAN_INTERACT_ROOM((room), INTERACT_CHOP))) {
		do_chore_chopping(emp, room);
		return;
	}
	
	// crops
	if (ROOM_SECT_FLAGGED(room, SECTF_CROP) && CHORE_ACTIVE(CHORE_FARMING)) {
		do_chore_farming(emp, room);
		return;
	}
	
	// building
	if ((IS_INCOMPLETE(room) || IS_DISMANTLING(room)) && CHORE_ACTIVE(CHORE_BUILDING)) {
		if (!IS_DISMANTLING(room)) {
			do_chore_building(emp, room, CHORE_BUILDING);
		}
		else {
			do_chore_dismantle(emp, room);
		}
		
		// do not trigger other actions in the same room in 1 cycle
		return;
	}
	
	// buildings
	if (IS_COMPLETE(room)) {
		if ((BUILDING_DAMAGE(room) > 0 || BUILDING_RESOURCES(room)) && HOME_ROOM(room) == room && CHORE_ACTIVE(CHORE_MAINTENANCE)) {
			do_chore_building(emp, room, CHORE_MAINTENANCE);
		}
		
		// this covers all the herbs
		if (EMPIRE_HAS_TECH(emp, TECH_SKILLED_LABOR) && CHORE_ACTIVE(CHORE_HERB_GARDENING) && IS_ANY_BUILDING(room) && CAN_INTERACT_ROOM(room, INTERACT_FIND_HERB)) {
			do_chore_gardening(emp, room);
		}
	
		if (HAS_FUNCTION(room, FNC_MINT) && EMPIRE_HAS_TECH(emp, TECH_SKILLED_LABOR) && CHORE_ACTIVE(CHORE_MINTING)) {
			do_chore_minting(emp, room);
		}
		
		if (HAS_FUNCTION(room, FNC_MINE)) {
			if (get_room_extra_data(room, ROOM_EXTRA_MINE_AMOUNT) > 0) {
				if (CHORE_ACTIVE(CHORE_MINING)) {
					do_chore_mining(emp, room);
				}
			}
			else if (IS_MAP_BUILDING(room) && !ROOM_AFF_FLAGGED(room, ROOM_AFF_NO_DISMANTLE) && CHORE_ACTIVE(CHORE_DISMANTLE_MINES)) {
				// no ore left
				do_chore_dismantle_mines(emp, room);
			}
		}
		
		if (HAS_FUNCTION(room, FNC_POTTER) && CHORE_ACTIVE(CHORE_BRICKMAKING)) {
			do_chore_brickmaking(emp, room);
		}
		if (HAS_FUNCTION(room, FNC_SMELT) && CHORE_ACTIVE(CHORE_SMELTING)) {
			do_chore_gen_craft(emp, room, CHORE_SMELTING, chore_smelting, FALSE);
		}
		if (HAS_FUNCTION(room, FNC_TAILOR) && CHORE_ACTIVE(CHORE_WEAVING)) {
			do_chore_gen_craft(emp, room, CHORE_WEAVING, chore_weaving, FALSE);
		}
		if (HAS_FUNCTION(room, FNC_FORGE) && CHORE_ACTIVE(CHORE_NAILMAKING)) {
			do_chore_nailmaking(emp, room);
		}
		if (HAS_FUNCTION(room, FNC_SAW) && CHORE_ACTIVE(CHORE_SCRAPING)) {
			do_chore_einv_interaction(emp, room, CHORE_SCRAPING, INTERACT_SCRAPE);
		}
		if (HAS_FUNCTION(room, FNC_DIGGING) && CHORE_ACTIVE(CHORE_DIGGING)) {
			do_chore_digging(emp, room);
		}
		if (HAS_FUNCTION(room, FNC_DIGGING) && CHORE_ACTIVE(CHORE_DIGGING)) {
			do_chore_digging(emp, room);
		}
		if (BUILDING_VNUM(room) == BUILDING_TRAPPERS_POST && EMPIRE_HAS_TECH(emp, TECH_SKILLED_LABOR) && CHORE_ACTIVE(CHORE_TRAPPING)) {
			do_chore_trapping(emp, room);
		}
		if (HAS_FUNCTION(room, FNC_TANNERY) && CHORE_ACTIVE(CHORE_TANNING)) {
			do_chore_einv_interaction(emp, room, CHORE_TANNING, INTERACT_TAN);
		}
		if (HAS_FUNCTION(room, FNC_STABLE) && CHORE_ACTIVE(CHORE_SHEARING)) {
			do_chore_shearing(emp, room);
		}
		if (CHORE_ACTIVE(CHORE_QUARRYING) && CAN_INTERACT_ROOM(room, INTERACT_QUARRY)) {
			do_chore_quarrying(emp, room);
		}
		if (HAS_FUNCTION(room, FNC_SAW) && CHORE_ACTIVE(CHORE_SAWING)) {
			do_chore_einv_interaction(emp, room, CHORE_SAWING, INTERACT_SAW);
		}
		if (HAS_FUNCTION(room, FNC_MILL) && CHORE_ACTIVE(CHORE_MILLING)) {
			do_chore_gen_craft(emp, room, CHORE_MILLING, chore_milling, FALSE);
		}
		if (HAS_FUNCTION(room, FNC_PRESS) && CHORE_ACTIVE(CHORE_OILMAKING)) {
			do_chore_gen_craft(emp, room, CHORE_OILMAKING, chore_pressing, FALSE);
		}
		if (BUILDING_VNUM(room) == RTYPE_SORCERER_TOWER && CHORE_ACTIVE(CHORE_NEXUS_CRYSTALS) && EMPIRE_HAS_TECH(emp, TECH_SKILLED_LABOR) && EMPIRE_HAS_TECH(emp, TECH_EXARCH_CRAFTS)) {
			do_chore_gen_craft(emp, room, CHORE_NEXUS_CRYSTALS, chore_nexus_crystals, TRUE);
		}
	}
}


/**
* Runs a single vehicle-related chore
*/
void process_one_vehicle_chore(empire_data *emp, vehicle_data *veh) {
	int island;
	
	// basic safety and sanitation
	if (!emp || !veh || !IN_ROOM(veh)) {
		return;
	}
	if (WATER_SECT(IN_ROOM(veh)) || (island = GET_ISLAND_ID(IN_ROOM(veh))) == NO_ISLAND) {
		return;
	}
	if (ROOM_AFF_FLAGGED(IN_ROOM(veh), ROOM_AFF_NO_WORK)) {
		return;
	}
	if (ROOM_OWNER(IN_ROOM(veh)) && ROOM_OWNER(IN_ROOM(veh)) != emp && !has_relationship(emp, ROOM_OWNER(IN_ROOM(veh)), DIPL_ALLIED)) {
		return;
	}
	
	// FIRE
	if (VEH_FLAGGED(veh, VEH_ON_FIRE)) {
		if (empire_chore_limit(emp, island, CHORE_FIRE_BRIGADE)) {
			vehicle_chore_fire_brigade(emp, veh);
		}
		// prevent other chores from firing while burning
		return;
	}
	// REPAIR
	if (VEH_IS_COMPLETE(veh) && VEH_NEEDS_RESOURCES(veh) && empire_chore_limit(emp, island, CHORE_REPAIR_VEHICLES)) {
		vehicle_chore_repair(emp, veh);
		return;	// no further chores while repairing
	}
	
	// any further chores require no no-work flag
	if (VEH_INTERIOR_HOME_ROOM(veh) && ROOM_AFF_FLAGGED(VEH_INTERIOR_HOME_ROOM(veh), ROOM_AFF_NO_WORK)) {
		return;
	}
}


 /////////////////////////////////////////////////////////////////////////////
//// EWT TRACKER ////////////////////////////////////////////////////////////

/**
* Finds an island entry for a workforce tracker.
*
* @param struct empire_workforce_tracker *tracker The tracker entry to use.
* @param int id The island id to find (will create if missing).
* @return struct empire_workforce_tracker_island* A pointer to the island entry in that tracker.
*/
static struct empire_workforce_tracker_island *ewt_find_island(struct empire_workforce_tracker *tracker, int id) {
	struct empire_workforce_tracker_island *ii;
	
	HASH_FIND_INT(tracker->islands, &id, ii);
	if (!ii) {
		CREATE(ii, struct empire_workforce_tracker_island, 1);
		ii->id = id;
		HASH_ADD_INT(tracker->islands, id, ii);
	}
	return ii;
}


/**
* This will find the workforce tracker for a given resource, creating it if
* necessary. Current storage numbers are read in when it's created.
* 
* @param empire_data *emp The empire we're tracking chores for.
* @param obj_vnum vnum What resource.
* @return struct empire_workforce_tracker* A pointer to the empire's tracker for that resource (guaranteed).
*/
static struct empire_workforce_tracker *ewt_find_tracker(empire_data *emp, obj_vnum vnum) {
	struct empire_workforce_tracker_island *isle;
	struct empire_workforce_tracker *tt;
	struct empire_storage_data *store;
	struct shipping_data *shipd;
	
	HASH_FIND_INT(EMPIRE_WORKFORCE_TRACKER(emp), &vnum, tt);
	if (!tt) {
		CREATE(tt, struct empire_workforce_tracker, 1);
		tt->vnum = vnum;
		HASH_ADD_INT(EMPIRE_WORKFORCE_TRACKER(emp), vnum, tt);
		
		// scan for data
		for (store = EMPIRE_STORAGE(emp); store; store = store->next) {
			if (store->vnum == vnum) {
				tt->total_amount += store->amount;
				isle = ewt_find_island(tt, store->island);
				isle->amount += store->amount;
			}
		}
	
		// count shipping, too
		for (shipd = EMPIRE_SHIPPING_LIST(emp); shipd; shipd = shipd->next) {
			if (shipd->vnum == vnum) {
				tt->total_amount += shipd->amount;
				
				if (shipd->status == SHIPPING_QUEUED) {
					isle = ewt_find_island(tt, shipd->from_island);
					isle->amount += shipd->amount;
				}
				else {
					isle = ewt_find_island(tt, shipd->to_island);
					isle->amount += shipd->amount;
				}
			}
		}
	}
	
	return tt;
}


/**
* Tallies a worker on a resource, to help with limits.
*
* @param empire_data *emp The empire that's working.
* @param room_data *loc Where they're working (for island info).
* @param obj_vnum vnum Which resource.
*/
static void ewt_mark_resource_worker(empire_data *emp, room_data *loc, obj_vnum vnum) {
	struct empire_workforce_tracker_island *isle;
	struct empire_workforce_tracker *tt;
	
	// safety
	if (!emp || !loc || vnum == NOTHING) {
		return;
	}
	
	tt = ewt_find_tracker(emp, vnum);
	isle = ewt_find_island(tt, GET_ISLAND_ID(loc));
	
	tt->total_workers += 1;
	isle->workers += 1;
}


/**
* Marks a worker on all resources for an interaction. This is used when a
* worker is spawned and the system isn't sure which items the worker might
* get from the interactions; it should prevent over-spawning workers.
*
* This function is called on interaction lists.
* 
* @param empire_data *emp The empire whose workers we'll mark.
* @param room_data *location The place we'll mark workers.
* @param struct interaction_item *list The list of interactions to check.
* @param int interaction_type Any INTERACT_ types.
*/
static void ewt_mark_for_interaction_list(empire_data *emp, room_data *location, struct interaction_item *list, int interaction_type) {
	struct interaction_item *interact;
	LL_FOREACH(list, interact) {
		if (interact->type == interaction_type) {
			ewt_mark_resource_worker(emp, location, interact->vnum);
		}
	}
}


/**
* Marks a worker on all resources for an interaction. This is used when a
* worker is spawned and the system isn't sure which items the worker might
* get from the interactions; it should prevent over-spawning workers.
*
* This function can be called on the whole room.
*
* @param empire_data *emp The empire whose workers we'll mark.
* @param room_data *room The room whose interactions we'll check.
* @param int interaction_type Any INTERACT_ type.
*/
static void ewt_mark_for_interactions(empire_data *emp, room_data *room, int interaction_type) {
	ewt_mark_for_interaction_list(emp, room, GET_SECT_INTERACTIONS(SECT(room)), interaction_type);
	if (ROOM_CROP(room)) {
		ewt_mark_for_interaction_list(emp, room, GET_CROP_INTERACTIONS(ROOM_CROP(room)), interaction_type);
	}
	if (GET_BUILDING(room)) {
		ewt_mark_for_interaction_list(emp, room, GET_BLD_INTERACTIONS(GET_BUILDING(room)), interaction_type);
	}
}


 /////////////////////////////////////////////////////////////////////////////
//// HELPERS ////////////////////////////////////////////////////////////////

/**
* This function sets the cap at which NPCs will no longer work a certain
* task. Data is tracked between calls in order to reduce the overall work, and
* should be freed with ewt_free_tracker() after each chore cycle.
*
* @param empire_data *emp which empire
* @param room_data *loc what location they're trying to gain at
* @param int chore which CHORE_
* @param obj_vnum which resource
* @return bool TRUE if the empire is below the cap
*/
static bool can_gain_chore_resource(empire_data *emp, room_data *loc, int chore, obj_vnum vnum) {
	struct empire_workforce_tracker_island *isle;
	int island_id, island_max, total_max;
	struct empire_workforce_tracker *tt;
	struct empire_island *emp_isle;
	obj_data *proto;
	
	// safety
	if (!emp || !loc || vnum == NOTHING || !(proto = obj_proto(vnum)) || !proto->storage) {
		return FALSE;
	}
	
	// first look up data for this vnum
	tt = ewt_find_tracker(emp, vnum);
	
	// data is assumed to be accurate now
	island_id = GET_ISLAND_ID(loc);
	isle = ewt_find_island(tt, island_id);

	// determine local maxima
	if (EMPIRE_HAS_TECH(emp, TECH_SKILLED_LABOR)) {
		island_max = config_get_int("max_chore_resource_skilled");
	}
	else {
		island_max = config_get_int("max_chore_resource");
	}
	
	// total max is a factor of this
	total_max = round(island_max * diminishing_returns(EMPIRE_MEMBERS(emp), 5));
	
	// check empire's own limit
	if ((emp_isle = get_empire_island(emp, island_id))) {
		if (emp_isle->workforce_limit[chore] > 0 && emp_isle->workforce_limit[chore] < island_max) {
			island_max = emp_isle->workforce_limit[chore];
		}
	}

	// do we have too much?
	if (tt->total_amount + tt->total_workers >= total_max) {
		if (isle->amount + isle->workers < config_get_int("max_chore_resource_over_total")) {
			return TRUE;
		}
	}
	else {
		if (isle->amount + isle->workers < island_max) {
			return TRUE;
		}
	}
	
	// in all other cases
	return FALSE;
}


/**
* Checks to see if the empire can gain any chore that's in an interaction list.
* If you pass TRUE for highest_only, it will only care about the thing with the
* highest percentage. This patches a bug where workforce might spawn for some-
* thing that only has a 1% chance of coming up, then despawn without finding
* any again.
*
* @param empire_data *emp The empire whose inventory we'll check.
* @param room_data *location The place we'll check for resource overages.
* @param int chore which CHORE_
* @param struct interaction_item *list The list of interactions to check.
* @param int interaction_type Any INTERACT_ types.
* @param bool highest_only If TRUE, only checks if the empire can gain the thing with the highest percent.
* @return bool TRUE if the empire could gain the resource(s) from the interaction list.
*/
bool can_gain_chore_resource_from_interaction_list(empire_data *emp, room_data *location, int chore, struct interaction_item *list, int interaction_type, bool highest_only) {
	struct interaction_item *interact, *found = NULL;
	double best_percent = 0.0;
	obj_data *proto;
	
	for (interact = list; interact; interact = interact->next) {
		if (interact->type != interaction_type) {
			continue;
		}
		if (!(proto = obj_proto(interact->vnum))) {
			continue;
		}
		if (!proto->storage) {
			continue;	// MUST be storable
		}
		
		// found
		if (highest_only) {
			if (!found || interact->percent > best_percent) {
				best_percent = interact->percent;
				found = interact;
			}
		}
		else if (can_gain_chore_resource(emp, location, chore, interact->vnum)) {
			// any 1 is fine
			return TRUE;
		}
	}
	
	return (found && can_gain_chore_resource(emp, location, chore, found->vnum));
}


/**
* Checks to see if the empire can gain any chore that's on an interaction for this room.
*
* @param empire_data *emp The empire whose inventory we'll check.
* @param room_data *room The room whose interactions we'll check.
* @param int chore which CHORE_
* @param int interaction_type Any INTERACT_ types.
* @return bool TRUE if the empire could gain at least one resource from the interactions on this room.
*/
bool can_gain_chore_resource_from_interaction(empire_data *emp, room_data *room, int chore, int interaction_type) {
	bool found_any = FALSE;
	crop_data *cp;
	
	found_any |= can_gain_chore_resource_from_interaction_list(emp, room, chore, GET_SECT_INTERACTIONS(SECT(room)), interaction_type, FALSE);
	if (!found_any && (cp = ROOM_CROP(room))) {
		found_any |= can_gain_chore_resource_from_interaction_list(emp, room, chore, GET_CROP_INTERACTIONS(cp), interaction_type, FALSE);
	}
	if (!found_any && GET_BUILDING(room)) {
		found_any |= can_gain_chore_resource_from_interaction_list(emp, room, chore, GET_BLD_INTERACTIONS(GET_BUILDING(room)), interaction_type, FALSE);
	}
	
	return found_any;
}


/**
* This runs once per mud hour to update all empire chores.
*/
void chore_update(void) {
	void ewt_free_tracker(struct empire_workforce_tracker **tracker);
	
	struct empire_territory_data *ter;
	vehicle_data *veh, *next_veh;
	empire_data *emp, *next_emp;
	
	int time_to_empire_emptiness = config_get_int("time_to_empire_emptiness") * SECS_PER_REAL_WEEK;

	HASH_ITER(hh, empire_table, emp, next_emp) {
		// skip idle empires
		if (EMPIRE_LAST_LOGON(emp) + time_to_empire_emptiness < time(0)) {
			continue;
		}

		if (EMPIRE_HAS_TECH(emp, TECH_WORKFORCE)) {
			// sort einv now to ensure it's in a useful order (most quantity first)
			LL_SORT(EMPIRE_STORAGE(emp), sort_einv);
			
			global_next_territory_entry = NULL;
			for (ter = EMPIRE_TERRITORY_LIST(emp); ter; ter = global_next_territory_entry) {
				global_next_territory_entry = ter->next;
				process_one_chore(emp, ter->room);
			}
			
			LL_FOREACH_SAFE(vehicle_list, veh, next_veh) {
				if (VEH_OWNER(veh) == emp) {
					process_one_vehicle_chore(emp, veh);
				}
			}
			
			EMPIRE_NEEDS_SAVE(emp) = TRUE;
			
			// no longer need this -- free up the tracker
			ewt_free_tracker(&EMPIRE_WORKFORCE_TRACKER(emp));
		}
	}
}


/**
* This deactivates the workforce for an empire chore.
*
* @param empire_data *emp
* @param int island_id Which island (or NO_ISLAND for all)
* @param int type CHORE_
*/
void deactivate_workforce(empire_data *emp, int island_id, int type) {
	char_data *mob, *next_mob;
	
	if (chore_data[type].mob != NOTHING) {
		for (mob = character_list; mob; mob = next_mob) {
			next_mob = mob->next;
		
			if (!IS_NPC(mob) || GET_LOYALTY(mob) != emp) {
				continue;
			}
			if (GET_MOB_VNUM(mob) != chore_data[type].mob) {
				continue;
			}
			if (island_id != NO_ISLAND && GET_ISLAND_ID(IN_ROOM(mob)) != island_id) {
				continue;
			}
		
			// mark in case we can't remove
			SET_BIT(MOB_FLAGS(mob), MOB_SPAWNED);
		
			// attempt to despawn
			if (!FIGHTING(mob)) {
				act("$n leaves to do other work.", TRUE, mob, NULL, NULL, TO_ROOM);
				extract_char(mob);
			}
		}
	}
}


/**
* This deactivates the workforce in one room for an empire.
*
* @param empire_data *emp Which empire.
* @param room_data *room Which room.
*/
void deactivate_workforce_room(empire_data *emp, room_data *room) {
	char_data *mob, *next_mob;
	int iter;
	bool match;
	
	for (mob = ROOM_PEOPLE(room); mob; mob = next_mob) {
		next_mob = mob->next_in_room;
		
		if (IS_NPC(mob) && GET_MOB_VNUM(mob) != NOTHING) {
			// check it's a workforce mob
			match = FALSE;
			for (iter = 0; iter < NUM_CHORES && !match; ++iter) {
				if (GET_MOB_VNUM(mob) == chore_data[iter].mob) {
					match = TRUE;
				}
			}
		
			if (match) {
				// mark in case we can't remove
				SET_BIT(MOB_FLAGS(mob), MOB_SPAWNED);
		
				if (!FIGHTING(mob)) {
					act("$n leaves to do other work.", TRUE, mob, NULL, NULL, TO_ROOM);
					extract_char(mob);
				}
			}
		}
	}
}


/**
* Gets an empire's workforce chore limit:
* 0: Do not work
* -1: Use natural limit
* >0: How much to produce before stopping
*
* @param empire_data *emp The empire.
* @param int island_id Which island we're on.
* @param int chore Which CHORE_ type.
* @return int The workforce limit.
*/
int empire_chore_limit(empire_data *emp, int island_id, int chore) {
	struct empire_island *isle;
	
	// sanity
	if (!emp || island_id == NO_ISLAND || chore < 0 || chore >= NUM_CHORES) {
		return 0;
	}
	
	// get island data
	if (!(isle = get_empire_island(emp, island_id))) {
		return 0;
	}
	
	return isle->workforce_limit[chore];
}


/**
* Looks for a matching worker in the room who can also do chores. If it finds
* any disabled copies of the worker, it marks them as spawned and does not
* return them, which should later trigger a new worker to be placed.
*
* @param room_data *room The location to check.
* @param mob_vnum vnum The worker vnum to look for.
* @return char_data* The found mob, or NULL;
*/
char_data *find_chore_worker_in_room(room_data *room, mob_vnum vnum) {
	char_data *mob;
	
	// does not work if no vnum provided
	if (vnum == NOTHING) {
		return NULL;
	}
	
	for (mob = ROOM_PEOPLE(room); mob; mob = mob->next_in_room) {
		// not our mob
		if (!IS_NPC(mob) || GET_MOB_VNUM(mob) != vnum) {
			continue;
		}
		
		// mob is in some way incapacitated -- mark for despawn
		if (IS_DEAD(mob) || EXTRACTED(mob) || GET_POS(mob) < MIN_WORKER_POS || GET_FED_ON_BY(mob) || GET_FEEDING_FROM(mob)) {
			SET_BIT(MOB_FLAGS(mob), MOB_SPAWNED);
			continue;
		}
		
		return mob;
	}
	
	return NULL;
}


/**
* This finds an NPC citizen who can do work in the area. It may return an
* npc who already has a loaded mob. If so, it's ok to repurpose this npc.
*
* @param empire_data *emp The empire looking for a worker.
* @param room_data *loc The location of the chore.
* @return struct empire_npc_data* The npc who will help, or NULL.
*/
struct empire_npc_data *find_free_npc_for_chore(empire_data *emp, room_data *loc) {
	struct empire_territory_data *ter_iter;
	struct empire_npc_data *found = NULL, *backup = NULL, *npc_iter;
	room_data *rm;

	int chore_distance = config_get_int("chore_distance");
	
	if (!emp || !loc) {
		return NULL;
	}
	
	// massive iteration to try to find one
	for (ter_iter = EMPIRE_TERRITORY_LIST(emp); ter_iter && !found; ter_iter = ter_iter->next) {
		// only bother checking anything if npcs live here
		if (ter_iter->npcs) {			
			if ((rm = ter_iter->room) && GET_ISLAND_ID(loc) == GET_ISLAND_ID(rm) && compute_distance(loc, rm) <= chore_distance) {
				// iterate over population
				for (npc_iter = ter_iter->npcs; npc_iter && !found; npc_iter = npc_iter->next) {
					// only use citizens for laber
					if (npc_iter->vnum == CITIZEN_MALE || npc_iter->vnum == CITIZEN_FEMALE) {
						if (!backup && npc_iter->mob && GET_MOB_VNUM(npc_iter->mob) == npc_iter->vnum && !FIGHTING(npc_iter->mob)) {
							// already has a mob? save as backup (also making sure the npc's mob is just a citizen not a laborer)
							backup = npc_iter;
						}
						else if (!npc_iter->mob) {
							found = npc_iter;
						}
					}
				}
			}
		}
	}
	
	if (found) {
		return found;
	}
	else if (backup) {
		if (backup->mob) {
			act("$n leaves to go to work.", TRUE, backup->mob, NULL, NULL, TO_ROOM);
			extract_char(backup->mob);
			backup->mob = NULL;
		}
		
		return backup;
	}
	else {
		return NULL;
	}
}


/**
* @param empire_data *emp Empire the worker belongs to
* @param int chore Which CHORE_
* @param room_data *room Where to look
* @return char_data *the mob, or NULL if none to spawn
*/
char_data *place_chore_worker(empire_data *emp, int chore, room_data *room) {
	extern char_data *spawn_empire_npc_to_room(empire_data *emp, struct empire_npc_data *npc, room_data *room, mob_vnum override_mob);
	
	struct empire_npc_data *npc;
	char_data *mob = NULL;
	
	// nobody to spawn
	if (chore_data[chore].mob == NOTHING) {
		return NULL;
	}
	
	if ((npc = find_free_npc_for_chore(emp, room))) {
		mob = spawn_empire_npc_to_room(emp, npc, room, chore_data[chore].mob);
		
		// shut off SPAWNED flag so chore mobs don't despawn
		REMOVE_BIT(MOB_FLAGS(mob), MOB_SPAWNED);
	}
	
	return mob;
}


/**
* Simple sorter puts higher quantities at the top (helps workforce optimize)
*
* @param struct empire_storage_data *a One element
* @param struct empire_storage_data *b Another element
* @return int Sort instruction of <0, 0, or >0
*/
int sort_einv(struct empire_storage_data *a, struct empire_storage_data *b) {
	return b->amount - a->amount;
}


 /////////////////////////////////////////////////////////////////////////////
//// GENERIC CRAFT WORKFORCE ////////////////////////////////////////////////

/**
* Function passed to do_chore_gen_craft()
*
* @param empire_data *emp The empire doing the chore.
* @param room_data *room The room the chore is in.
* @param int chore CHORE_ const for this chore.
* @param craft_data *craft The craft to validate.
* @return bool TRUE if this workforce chore can work this craft, FALSE if not
*/
CHORE_GEN_CRAFT_VALIDATOR(chore_nexus_crystals) {
	if (GET_CRAFT_OBJECT(craft) != o_NEXUS_CRYSTAL) {
		return FALSE;
	}
	// success
	return TRUE;
}


/**
* Function passed to do_chore_gen_craft()
*
* @param empire_data *emp The empire doing the chore.
* @param room_data *room The room the chore is in.
* @param int chore CHORE_ const for this chore.
* @param craft_data *craft The craft to validate.
* @return bool TRUE if this workforce chore can work this craft, FALSE if not
*/
CHORE_GEN_CRAFT_VALIDATOR(chore_milling) {
	ability_data *abil;
	
	if (GET_CRAFT_TYPE(craft) != CRAFT_TYPE_MILL) {
		return FALSE;
	}
	// won't mill things that require classes or high skill
	if ((abil = find_ability_by_vnum(GET_CRAFT_ABILITY(craft))) ) {
		if (!ABIL_ASSIGNED_SKILL(abil)) {
			return FALSE;	// class ability
		}
		else if (ABIL_SKILL_LEVEL(abil) > BASIC_SKILL_CAP) {
			return FALSE;	// level too high
		}
	}
	// success
	return TRUE;
}


/**
* Function passed to do_chore_gen_craft()
*
* @param empire_data *emp The empire doing the chore.
* @param room_data *room The room the chore is in.
* @param int chore CHORE_ const for this chore.
* @param craft_data *craft The craft to validate.
* @return bool TRUE if this workforce chore can work this craft, FALSE if not
*/
CHORE_GEN_CRAFT_VALIDATOR(chore_pressing) {
	ability_data *abil;
	
	if (GET_CRAFT_TYPE(craft) != CRAFT_TYPE_PRESS) {
		return FALSE;
	}
	// won't press things that require classes or high skill
	if ((abil = find_ability_by_vnum(GET_CRAFT_ABILITY(craft))) ) {
		if (!ABIL_ASSIGNED_SKILL(abil)) {
			return FALSE;	// class ability
		}
		else if (ABIL_SKILL_LEVEL(abil) > BASIC_SKILL_CAP) {
			return FALSE;	// level too high
		}
	}
	// success
	return TRUE;
}


/**
* Function passed to do_chore_gen_craft()
*
* @param empire_data *emp The empire doing the chore.
* @param room_data *room The room the chore is in.
* @param int chore CHORE_ const for this chore.
* @param craft_data *craft The craft to validate.
* @return bool TRUE if this workforce chore can work this craft, FALSE if not
*/
CHORE_GEN_CRAFT_VALIDATOR(chore_smelting) {
	ability_data *abil;
	
	if (GET_CRAFT_TYPE(craft) != CRAFT_TYPE_SMELT) {
		return FALSE;
	}
	// won't smelt things that require classes or high skill
	if ((abil = find_ability_by_vnum(GET_CRAFT_ABILITY(craft))) ) {
		if (!ABIL_ASSIGNED_SKILL(abil)) {
			return FALSE;	// class ability
		}
		else if (ABIL_SKILL_LEVEL(abil) > BASIC_SKILL_CAP) {
			return FALSE;	// level too high
		}
	}
	// success
	return TRUE;
}


/**
* Function passed to do_chore_gen_craft()
*
* @param empire_data *emp The empire doing the chore.
* @param room_data *room The room the chore is in.
* @param int chore CHORE_ const for this chore.
* @param craft_data *craft The craft to validate.
* @return bool TRUE if this workforce chore can work this craft, FALSE if not
*/
CHORE_GEN_CRAFT_VALIDATOR(chore_weaving) {
	ability_data *abil;
	
	if (GET_CRAFT_TYPE(craft) != CRAFT_TYPE_WEAVE) {
		return FALSE;
	}
	// won't weave things that require classes or high skill
	if ((abil = find_ability_by_vnum(GET_CRAFT_ABILITY(craft))) ) {
		if (!ABIL_ASSIGNED_SKILL(abil)) {
			return FALSE;	// class ability
		}
		else if (ABIL_SKILL_LEVEL(abil) > BASIC_SKILL_CAP) {
			return FALSE;	// level too high
		}
	}
	// success
	return TRUE;
}


/**
* Can manage any craft in the craft_table, validated by the 'validator' arg.
*
* @param empire_data *emp The empire doing the chore.
* @param room_data *room The room the chore is in.
* @param int chore CHORE_ const for this chore.
* @param CHORE_GEN_CRAFT_VALIDATOR *validator A function that validates a craft.
* @param bool is_skilled If TRUE, gains exp for Skilled Labor.
*/
void do_chore_gen_craft(empire_data *emp, room_data *room, int chore, CHORE_GEN_CRAFT_VALIDATOR(*validator), bool is_skilled) {
	extern struct gen_craft_data_t gen_craft_data[];
	
	struct empire_storage_data *store = NULL;
	char_data *worker = find_chore_worker_in_room(room, chore_data[chore].mob);
	craft_data *craft, *next_craft, *do_craft = NULL;
	int islid = GET_ISLAND_ID(room);
	struct resource_data *res;
	int crafts_found;
	char buf[256];
	bool has_res;
	
	// find a craft we can do
	crafts_found = 0;
	HASH_ITER(hh, craft_table, craft, next_craft) {
		// must be a live recipe; must make an item
		if (CRAFT_FLAGGED(craft, CRAFT_IN_DEVELOPMENT | CRAFT_SOUP | CRAFT_VEHICLE) || GET_CRAFT_TYPE(craft) == CRAFT_TYPE_BUILD) {
			continue;
		}
		// pass through validator function
		if (!validator || !(validator)(emp, room, chore, craft)) {
			continue;
		}
		
		// can we gain any of it?
		if (!can_gain_chore_resource(emp, room, chore, GET_CRAFT_OBJECT(craft))) {
			continue;
		}
		
		// check resources...
		has_res = TRUE;
		for (res = GET_CRAFT_RESOURCES(craft); res && has_res; res = res->next) {
			if (res->type != RES_OBJECT && res->type != RES_COMPONENT) {
				// can ONLY do crafts that use objects/components
				has_res = FALSE;
			}
			else if (res->type == RES_OBJECT && (!(store = find_stored_resource(emp, islid, res->vnum)) || store->amount < res->amount)) {
				has_res = FALSE;
			}
			else if (res->type == RES_COMPONENT && !empire_can_afford_component(emp, islid, res->vnum, res->misc, res->amount)) {
				// this actually requires all of the component be the same type -- it won't mix component types
				has_res = FALSE;
			}
		}
		if (!has_res) {
			continue;
		}
		
		// found one! (pick at radom if more than one)
		if (!number(0, crafts_found++) || !do_craft) {
			do_craft = craft;
		}
	}
	
	// now attempt to do the chore
	if (worker && do_craft) {
		ewt_mark_resource_worker(emp, room, GET_CRAFT_OBJECT(do_craft));
		
		// charge resources (we pre-validated res->type and availability)
		for (res = GET_CRAFT_RESOURCES(do_craft); res; res = res->next) {
			if (res->type == RES_OBJECT) {
				charge_stored_resource(emp, islid, res->vnum, res->amount);
			}
			else if (res->type == RES_COMPONENT) {
				charge_stored_component(emp, islid, res->vnum, res->misc, res->amount, NULL);
			}
		}
		
		add_to_empire_storage(emp, islid, GET_CRAFT_OBJECT(do_craft), GET_CRAFT_QUANTITY(do_craft));
		empire_skillup(emp, ABIL_WORKFORCE, config_get_double("exp_from_workforce"));
		if (is_skilled) {
			empire_skillup(emp, ABIL_SKILLED_LABOR, config_get_double("exp_from_workforce"));
		}
		
		// only send message if someone else is present (don't bother verifying it's a player)
		if (ROOM_PEOPLE(IN_ROOM(worker))->next_in_room) {
			snprintf(buf, sizeof(buf), "$n finishes %s %s.", gen_craft_data[GET_CRAFT_TYPE(do_craft)].verb, get_obj_name_by_proto(GET_CRAFT_OBJECT(do_craft)));
			act(buf, FALSE, worker, NULL, NULL, TO_ROOM);
		}
	}
	else if (do_craft) {
		// place worker
		if ((worker = place_chore_worker(emp, chore, room))) {
			ewt_mark_resource_worker(emp, room, GET_CRAFT_OBJECT(do_craft));
		}
	}
	else if (worker) {
		SET_BIT(MOB_FLAGS(worker), MOB_SPAWNED);
	}
}


 /////////////////////////////////////////////////////////////////////////////
//// CHORE FUNCTIONS ////////////////////////////////////////////////////////

void do_chore_brickmaking(empire_data *emp, room_data *room) {
	char_data *worker = find_chore_worker_in_room(room, chore_data[CHORE_BRICKMAKING].mob);
	int islid = GET_ISLAND_ID(room);
	bool can_do = can_gain_chore_resource(emp, room, CHORE_BRICKMAKING, o_BRICKS) && empire_can_afford_component(emp, islid, CMP_CLAY, NOBITS, 2);
	
	if (worker && can_do) {
		ewt_mark_resource_worker(emp, room, o_BRICKS);
		
		charge_stored_component(emp, islid, CMP_CLAY, NOBITS, 2, NULL);
		add_to_empire_storage(emp, islid, o_BRICKS, 1);
		
		act("$n finishes a pile of bricks.", FALSE, worker, NULL, NULL, TO_ROOM);
		empire_skillup(emp, ABIL_WORKFORCE, config_get_double("exp_from_workforce"));
	}
	else if (can_do) {
		// place worker
		if ((worker = place_chore_worker(emp, CHORE_BRICKMAKING, room))) {
			ewt_mark_resource_worker(emp, room, o_BRICKS);
		}
	}
	else if (worker) {
		SET_BIT(MOB_FLAGS(worker), MOB_SPAWNED);
	}
}


/**
* This is used for both building and maintenance.
*
* @param empire_data *emp Which empire is doing the chore.
* @param room_data *room Which room to build/maintain
* @param int mode Which CHORE_ is being performed (build/maintain).
*/
void do_chore_building(empire_data *emp, room_data *room, int mode) {
	void finish_building(char_data *ch, room_data *room);
	void finish_maintenance(char_data *ch, room_data *room);
	
	char_data *worker = find_chore_worker_in_room(room, chore_data[mode].mob);
	struct empire_storage_data *store = NULL;
	bool can_do = FALSE, found = FALSE;
	struct resource_data *res = NULL;
	int islid = GET_ISLAND_ID(room);
	
	if (!BUILDING_RESOURCES(room)) {
		// is already done; just have to finish
		can_do = TRUE;
	}
	else if ((res = BUILDING_RESOURCES(room))) {
		// RES_x: can only process some types in this way
		if (res->type == RES_OBJECT && (store = find_stored_resource(emp, islid, res->vnum)) && store->amount > 0) {
			can_do = TRUE;
		}
		else if (res->type == RES_COMPONENT && empire_can_afford_component(emp, islid, res->vnum, res->misc, 1)) {
			can_do = TRUE;
		}
		else if (res->type == RES_ACTION) {
			can_do = TRUE;
		}
	}
	
	if (worker && can_do) {
		if (res) {
			found = TRUE;
			empire_skillup(emp, ABIL_WORKFORCE, config_get_double("exp_from_workforce"));
			
			if (res->type == RES_OBJECT) {
				add_to_resource_list(&GET_BUILT_WITH(room), RES_OBJECT, res->vnum, 1, 0);
				charge_stored_resource(emp, islid, res->vnum, 1);
			}
			else if (res->type == RES_COMPONENT) {
				charge_stored_component(emp, islid, res->vnum, res->misc, 1, &GET_BUILT_WITH(room));
			}
			
			res->amount -= 1;
			
			// remove res?
			if (res->amount <= 0) {
				LL_DELETE(GET_BUILDING_RESOURCES(room), res);
				free(res);
			}
		}
		
		// check for completion
		if (!BUILDING_RESOURCES(room)) {
			SET_BIT(MOB_FLAGS(worker), MOB_SPAWNED);
			
			if (mode == CHORE_BUILDING) {
				finish_building(worker, room);
				stop_room_action(room, ACT_BUILDING, CHORE_BUILDING);
			}
			else if (mode == CHORE_MAINTENANCE) {
				finish_maintenance(worker, room);
				stop_room_action(room, ACT_MAINTENANCE, CHORE_MAINTENANCE);
			}
		}

		if (!found) {
			SET_BIT(MOB_FLAGS(worker), MOB_SPAWNED);
		}
	}
	else if (can_do) {
		worker = place_chore_worker(emp, mode, room);
	}
	else if (worker) {
		SET_BIT(MOB_FLAGS(worker), MOB_SPAWNED);
	}
}


INTERACTION_FUNC(one_chop_chore) {
	empire_data *emp = ROOM_OWNER(inter_room);
	
	if (emp && can_gain_chore_resource(emp, inter_room, CHORE_CHOPPING, interaction->vnum)) {
		ewt_mark_resource_worker(emp, inter_room, interaction->vnum);
		add_to_empire_storage(emp, GET_ISLAND_ID(inter_room), interaction->vnum, interaction->quantity);
		return TRUE;
	}
	
	return FALSE;
}


void do_chore_chopping(empire_data *emp, room_data *room) {
	extern void change_chop_territory(room_data *room);
	
	char_data *worker = find_chore_worker_in_room(room, chore_data[CHORE_CHOPPING].mob);
	bool can_do = (get_depletion(room, DPLTN_CHOP) < config_get_int("chop_depletion")) && can_gain_chore_resource_from_interaction(emp, room, CHORE_CHOPPING, INTERACT_CHOP);
	
	int chop_timer = config_get_int("chop_timer");
	
	if (worker && can_do) {
		if (get_room_extra_data(room, ROOM_EXTRA_CHOP_PROGRESS) <= 0) {
			set_room_extra_data(room, ROOM_EXTRA_CHOP_PROGRESS, chop_timer);
			ewt_mark_for_interactions(emp, room, INTERACT_CHOP);
		}
		else {
			add_to_room_extra_data(room, ROOM_EXTRA_CHOP_PROGRESS, -1);
			if (get_room_extra_data(room, ROOM_EXTRA_CHOP_PROGRESS) == 0) {
				// finished!
				run_room_interactions(worker, room, INTERACT_CHOP, one_chop_chore);
				change_chop_territory(room);
				empire_skillup(emp, ABIL_WORKFORCE, config_get_double("exp_from_workforce"));
				
				if (CAN_CHOP_ROOM(room)) {
					set_room_extra_data(room, ROOM_EXTRA_CHOP_PROGRESS, chop_timer);
				}
				else {
					// done: mark for de-spawn
					SET_BIT(MOB_FLAGS(worker), MOB_SPAWNED);
					stop_room_action(room, ACT_CHOPPING, CHORE_CHOPPING);
					
					if (empire_chore_limit(emp, GET_ISLAND_ID(room), CHORE_ABANDON_CHOPPED)) {
						abandon_room(room);
					}
				}
			}
			else {
				ewt_mark_for_interactions(emp, room, INTERACT_CHOP);
			}
		}
	}
	else if (can_do) {
		if ((worker = place_chore_worker(emp, CHORE_CHOPPING, room))) {
			ewt_mark_for_interactions(emp, room, INTERACT_CHOP);
		}
	}
	else if (worker) {
		SET_BIT(MOB_FLAGS(worker), MOB_SPAWNED);
	}
}


INTERACTION_FUNC(one_dig_chore) {
	empire_data *emp = ROOM_OWNER(inter_room);
	
	if (emp && can_gain_chore_resource(emp, inter_room, CHORE_DIGGING, interaction->vnum)) {
		ewt_mark_resource_worker(emp, inter_room, interaction->vnum);
		add_to_empire_storage(emp, GET_ISLAND_ID(inter_room), interaction->vnum, interaction->quantity);
		add_depletion(inter_room, DPLTN_DIG, TRUE);
		empire_skillup(emp, ABIL_WORKFORCE, config_get_double("exp_from_workforce"));
		return TRUE;
	}
	
	return FALSE;
}


void do_chore_digging(empire_data *emp, room_data *room) {	
	char_data *worker = find_chore_worker_in_room(room, chore_data[CHORE_DIGGING].mob);
	bool depleted = (get_depletion(room, DPLTN_DIG) >= DEPLETION_LIMIT(room)) ? TRUE : FALSE;
	bool can_do = !depleted && can_gain_chore_resource_from_interaction(emp, room, CHORE_DIGGING, INTERACT_DIG);
	
	if (CAN_INTERACT_ROOM(room, INTERACT_DIG) && can_do) {
		// not able to ewt_mark_resource_worker() until we're inside the interact
		if (worker) {
			if (!run_room_interactions(worker, room, INTERACT_DIG, one_dig_chore)) {
				SET_BIT(MOB_FLAGS(worker), MOB_SPAWNED);
			}
		}
		else {
			if ((worker = place_chore_worker(emp, CHORE_DIGGING, room))) {
				ewt_mark_for_interactions(emp, room, INTERACT_DIG);
			}
		}
	}
	else if (worker) {
		SET_BIT(MOB_FLAGS(worker), MOB_SPAWNED);
	}
}


void do_chore_dismantle(empire_data *emp, room_data *room) {
	void finish_dismantle(char_data *ch, room_data *room);
	void finish_building(char_data *ch, room_data *room);
	
	struct resource_data *res, *next_res;
	bool can_do = FALSE, found = FALSE;
	char_data *worker = find_chore_worker_in_room(room, chore_data[CHORE_BUILDING].mob);
	obj_data *proto;
	
	// anything we can dismantle?
	if (!BUILDING_RESOURCES(room)) {
		can_do = TRUE;
	}
	else {
		LL_FOREACH(BUILDING_RESOURCES(room), res) {
			if (res->type == RES_OBJECT && (proto = obj_proto(res->vnum)) && proto->storage) {
				can_do = TRUE;
				break;
			}
		}
	}
	
	if (can_do && worker) {
		for (res = BUILDING_RESOURCES(room); res && !found; res = next_res) {
			next_res = res->next;
			
			// can only remove storable obj types
			if (res->type != RES_OBJECT || !(proto = obj_proto(res->vnum)) || !proto->storage) {
				continue;
			}
			
			if (res->amount > 0) {
				found = TRUE;
				res->amount -= 1;
				add_to_empire_storage(emp, GET_ISLAND_ID(room), res->vnum, 1);
				empire_skillup(emp, ABIL_WORKFORCE, config_get_double("exp_from_workforce"));
			}
			
			// remove res?
			if (res->amount <= 0) {
				LL_DELETE(GET_BUILDING_RESOURCES(room), res);
				free(res);
			}
		}
		
		// check for completion
		if (!BUILDING_RESOURCES(room)) {
			finish_dismantle(worker, room);
			SET_BIT(MOB_FLAGS(worker), MOB_SPAWNED);
			if (empire_chore_limit(emp, GET_ISLAND_ID(room), CHORE_ABANDON_DISMANTLED)) {
				abandon_room(room);
			}
			stop_room_action(room, ACT_DISMANTLING, CHORE_BUILDING);
		}

		if (!found) {
			// no work remains: mark for despawn
			SET_BIT(MOB_FLAGS(worker), MOB_SPAWNED);
		}
	}
	else if (can_do) {
		worker = place_chore_worker(emp, CHORE_BUILDING, room);
	}
	else if (worker) {
		SET_BIT(MOB_FLAGS(worker), MOB_SPAWNED);
	}
}


void do_chore_dismantle_mines(empire_data *emp, room_data *room) {
	void start_dismantle_building(room_data *loc);
	
	char_data *worker = find_chore_worker_in_room(room, chore_data[CHORE_DISMANTLE_MINES].mob);
	bool can_do = IS_COMPLETE(room);
	
	if (worker && can_do) {
		start_dismantle_building(room);
		act("$n begins to dismantle the building.", FALSE, worker, NULL, NULL, TO_ROOM);
		
		// if they have the building chore on, we'll keep using the mob
		if (!empire_chore_limit(emp, GET_ISLAND_ID(room), CHORE_BUILDING)) {
			SET_BIT(MOB_FLAGS(worker), MOB_SPAWNED);
		}
	}
	else if (can_do) {
		worker = place_chore_worker(emp, CHORE_DISMANTLE_MINES, room);
	}
	else if (worker) {
		SET_BIT(MOB_FLAGS(worker), MOB_SPAWNED);
	}
}


INTERACTION_FUNC(one_einv_interaction_chore) {
	empire_data *emp = ROOM_OWNER(inter_room);
	
	if (emp && can_gain_chore_resource(emp, inter_room, einv_interaction_chore_type, interaction->vnum)) {
		ewt_mark_resource_worker(emp, inter_room, interaction->vnum);
		add_to_empire_storage(emp, GET_ISLAND_ID(inter_room), interaction->vnum, interaction->quantity);
		return TRUE;
	}
	
	return FALSE;
}


/**
* Processes one einv-interaction chore (like sawing or scraping).
*
* @param empire_data *emp The empire doing the chore.
* @param room_data *room The location of the chore.
* @param int chore Which CHORE_ it is.
* @param int interact_type Which INTERACT_ to run on items in the einv.
*/
void do_chore_einv_interaction(empire_data *emp, room_data *room, int chore, int interact_type) {
	char_data *worker = find_chore_worker_in_room(room, chore_data[chore].mob);
	struct empire_storage_data *store, *found_store = NULL;
	obj_data *proto, *found_proto = NULL;
	int islid = GET_ISLAND_ID(room);
	int most_found = -1;
	
	// look for something to process
	LL_FOREACH(EMPIRE_STORAGE(emp), store) {
		if (store->island != islid || store->amount < 1) {
			continue;
		}
		if (!(proto = obj_proto(store->vnum))) {
			continue;
		}
		if (!has_interaction(proto->interactions, interact_type)) {
			continue;
		}
		if (!can_gain_chore_resource_from_interaction_list(emp, room, chore, proto->interactions, interact_type, TRUE)) {
			continue;
		}
		
		// found!
		if (store->amount > most_found) {
			found_proto = proto;
			found_store = store;
			most_found = store->amount;
		}
	}
	
	if (found_proto && worker) {
		einv_interaction_chore_type = chore;
		
		if (run_interactions(worker, found_proto->interactions, interact_type, room, worker, found_proto, one_einv_interaction_chore) && found_store) {
			empire_skillup(emp, ABIL_WORKFORCE, config_get_double("exp_from_workforce"));
			
			found_store->amount -= 1;
			
			if (found_store->amount <= 0) {
				LL_DELETE(EMPIRE_STORAGE(emp), found_store);
				free(found_store);
			}
		}
		else {
			// failed to hit any interactions
			SET_BIT(MOB_FLAGS(worker), MOB_SPAWNED);
		}
	}
	else if (found_proto) {
		if ((worker = place_chore_worker(emp, chore, room))) {
			ewt_mark_for_interaction_list(emp, room, found_proto->interactions, interact_type);
		}
	}
	else if (worker) {
		SET_BIT(MOB_FLAGS(worker), MOB_SPAWNED);
	}
}


INTERACTION_FUNC(one_farming_chore) {
	empire_data *emp = ROOM_OWNER(inter_room);
	obj_data *proto = obj_proto(interaction->vnum);
	int amt;
	
	if (emp && proto && proto->storage && can_gain_chore_resource(emp, inter_room, CHORE_FARMING, interaction->vnum)) {
		ewt_mark_resource_worker(emp, inter_room, interaction->vnum);
		empire_skillup(emp, ABIL_WORKFORCE, config_get_double("exp_from_workforce"));
		
		amt = interaction->quantity;
		add_to_empire_storage(emp, GET_ISLAND_ID(inter_room), interaction->vnum, amt);
		
		// add depletion only if orchard
		if (ROOM_CROP_FLAGGED(inter_room, CROPF_IS_ORCHARD)) {
			while (amt-- > 0) {
				add_depletion(inter_room, DPLTN_PICK, TRUE);
			}
		}

		sprintf(buf, "$n finishes harvesting the %s.", GET_CROP_NAME(ROOM_CROP(inter_room)));
		act(buf, FALSE, ch, NULL, NULL, TO_ROOM);
		return TRUE;
	}

	return FALSE;
}


void do_chore_farming(empire_data *emp, room_data *room) {
	extern const sector_vnum climate_default_sector[NUM_CLIMATES];
	
	char_data *worker = find_chore_worker_in_room(room, chore_data[CHORE_FARMING].mob);
	bool can_do = can_gain_chore_resource_from_interaction(emp, room, CHORE_FARMING, INTERACT_HARVEST);
	sector_data *old_sect;
	
	if (CAN_INTERACT_ROOM(room, INTERACT_HARVEST) && can_do) {
		// not able to ewt_mark_resource_worker() until we're inside the interact
		if (worker) {
			// set up harvest time if needed
			if (get_room_extra_data(room, ROOM_EXTRA_HARVEST_PROGRESS) <= 0) {
				set_room_extra_data(room, ROOM_EXTRA_HARVEST_PROGRESS, config_get_int("harvest_timer") * (ROOM_CROP_FLAGGED(room, CROPF_IS_ORCHARD) ? 2 : 1));
			}
			
			// harvest ticker
			add_to_room_extra_data(room, ROOM_EXTRA_HARVEST_PROGRESS, -1 * number(1, 2));
			
			// check progress
			if (get_room_extra_data(room, ROOM_EXTRA_HARVEST_PROGRESS) > 0) {
				// not done:
				ewt_mark_for_interactions(emp, room, INTERACT_HARVEST);
			}
			else {	// DONE!
				remove_room_extra_data(room, ROOM_EXTRA_HARVEST_PROGRESS);
				run_room_interactions(worker, room, INTERACT_HARVEST, one_farming_chore);
				
				// only change to seeded if it's not an orchard OR if it's over-picked			
				if (!ROOM_CROP_FLAGGED(room, CROPF_IS_ORCHARD) || get_depletion(room, DPLTN_PICK) >= config_get_int("short_depletion")) {
					if (empire_chore_limit(emp, GET_ISLAND_ID(room), CHORE_REPLANTING) && (old_sect = reverse_lookup_evolution_for_sector(SECT(room), EVO_CROP_GROWS))) {
						// sly-convert back to what it was grown from ... not using change_terrain
						perform_change_sect(room, NULL, old_sect);
				
						// we are keeping the original sect the same as it was
						// TODO un-magic-number this
						set_room_extra_data(room, ROOM_EXTRA_SEED_TIME, 60);
					}
					else {
						// do we have a stored original sect?
						if (BASE_SECT(room) != SECT(room)) {
							change_terrain(room, GET_SECT_VNUM(BASE_SECT(room)));
						}
						else {
							// fallback
							change_terrain(room, climate_default_sector[GET_CROP_CLIMATE(ROOM_CROP(room))]);
						}
						
						if (empire_chore_limit(emp, GET_ISLAND_ID(room), CHORE_ABANDON_FARMED)) {
							abandon_room(room);
						}
					}
					
					// stop all possible chores here since the sector changed
					stop_room_action(room, ACT_HARVESTING, CHORE_FARMING);
					stop_room_action(room, ACT_CHOPPING, CHORE_CHOPPING);
					stop_room_action(room, ACT_PICKING, CHORE_HERB_GARDENING);
					stop_room_action(room, ACT_GATHERING, NOTHING);
					
					// mark for despawn
					SET_BIT(MOB_FLAGS(worker), MOB_SPAWNED);
				}
			}
		}
		else {
			if ((worker = place_chore_worker(emp, CHORE_FARMING, room))) {
				ewt_mark_for_interactions(emp, room, INTERACT_HARVEST);
			}
		}
	}
	else if (worker) {
		SET_BIT(MOB_FLAGS(worker), MOB_SPAWNED);
	}
}


void do_chore_fire_brigade(empire_data *emp, room_data *room) {
	int fire_extinguish_value = config_get_int("fire_extinguish_value");
	char_data *worker = find_chore_worker_in_room(room, chore_data[CHORE_FIRE_BRIGADE].mob);
	
	if (worker && BUILDING_BURNING(room) > 0) {
		act("$n throws a bucket of water to douse the flames!", FALSE, worker, NULL, NULL, TO_ROOM);

		COMPLEX_DATA(room)->burning = MIN(fire_extinguish_value, BUILDING_BURNING(room) + number(2, 6));

		if (BUILDING_BURNING(room) >= fire_extinguish_value) {
			act("The flames have been extinguished!", FALSE, worker, 0, 0, TO_ROOM);
			COMPLEX_DATA(room)->burning = 0;
			
			// despawn
			SET_BIT(MOB_FLAGS(worker), MOB_SPAWNED);
			empire_skillup(emp, ABIL_WORKFORCE, 10);	// special case: does not use exp_from_workforce
		}		
	}
	else if (BUILDING_BURNING(room) > 0) {
		worker = place_chore_worker(emp, CHORE_FIRE_BRIGADE, room);
	}
	else if (worker) {
		SET_BIT(MOB_FLAGS(worker), MOB_SPAWNED);
	}
}


INTERACTION_FUNC(one_gardening_chore) {
	empire_data *emp = ROOM_OWNER(inter_room);
	
	if (emp && can_gain_chore_resource(emp, inter_room, CHORE_HERB_GARDENING, interaction->vnum)) {
		ewt_mark_resource_worker(emp, inter_room, interaction->vnum);
		
		add_to_empire_storage(emp, GET_ISLAND_ID(inter_room), interaction->vnum, interaction->quantity);
		add_depletion(inter_room, DPLTN_PICK, TRUE);
		empire_skillup(emp, ABIL_WORKFORCE, config_get_double("exp_from_workforce"));
		empire_skillup(emp, ABIL_SKILLED_LABOR, config_get_double("exp_from_workforce"));

		act("$n picks an herb and carefully hangs it to dry.", FALSE, ch, NULL, NULL, TO_ROOM);
		return TRUE;
	}
	
	return FALSE;
}


void do_chore_gardening(empire_data *emp, room_data *room) {
	int garden_depletion = config_get_int("garden_depletion");
	
	char_data *worker = find_chore_worker_in_room(room, chore_data[CHORE_HERB_GARDENING].mob);
	bool depleted = (get_depletion(room, DPLTN_PICK) >= garden_depletion);
	bool can_do = !depleted && can_gain_chore_resource_from_interaction(emp, room, CHORE_HERB_GARDENING, INTERACT_FIND_HERB);
	
	if (CAN_INTERACT_ROOM(room, INTERACT_FIND_HERB) && can_do) {
		// not able to ewt_mark_resource_worker() until inside the interaction
		if (worker) {
			if (!run_room_interactions(worker, room, INTERACT_FIND_HERB, one_gardening_chore)) {
				SET_BIT(MOB_FLAGS(worker), MOB_SPAWNED);
			}
		}
		else {
			if ((worker = place_chore_worker(emp, CHORE_HERB_GARDENING, room))) {
				ewt_mark_for_interactions(emp, room, INTERACT_FIND_HERB);
			}
		}
	}
	else if (worker) {
		SET_BIT(MOB_FLAGS(worker), MOB_SPAWNED);
	}
}


INTERACTION_FUNC(one_mining_chore) {
	empire_data *emp = ROOM_OWNER(inter_room);
	struct global_data *mine;
	obj_data *proto;
	
	// no mine
	if (!(mine = global_proto(get_room_extra_data(inter_room, ROOM_EXTRA_MINE_GLB_VNUM)))) {
		return FALSE;
	}
	
	// find object
	proto = obj_proto(interaction->vnum);
	
	// check vars and limits
	if (!emp || !proto || !proto->storage || !can_gain_chore_resource(emp, inter_room, CHORE_MINING, interaction->vnum)) {
		return FALSE;
	}
	
	// good to go
	ewt_mark_resource_worker(emp, inter_room, interaction->vnum);
	
	// mine ~ every sixth time
	if (!number(0, 5)) {
		if (interaction->quantity > 0) {
			add_to_room_extra_data(inter_room, ROOM_EXTRA_MINE_AMOUNT, -1 * interaction->quantity);
			add_to_empire_storage(emp, GET_ISLAND_ID(inter_room), interaction->vnum, interaction->quantity);
			empire_skillup(emp, ABIL_WORKFORCE, config_get_double("exp_from_workforce"));
			
			sprintf(buf, "$n strikes the wall and %s falls loose!", get_obj_name_by_proto(interaction->vnum));
			act(buf, FALSE, ch, NULL, NULL, TO_ROOM);
			return TRUE;
		}
		else {
			return FALSE;
		}
	}
	// didn't mine this time, still return TRUE
	return TRUE;
}


void do_chore_mining(empire_data *emp, room_data *room) {
	char_data *worker = find_chore_worker_in_room(room, chore_data[CHORE_MINING].mob);
	struct global_data *mine = global_proto(get_room_extra_data(room, ROOM_EXTRA_MINE_GLB_VNUM));
	bool can_do = (mine && GET_GLOBAL_TYPE(mine) == GLOBAL_MINE_DATA && get_room_extra_data(room, ROOM_EXTRA_MINE_AMOUNT) > 0 && can_gain_chore_resource_from_interaction_list(emp, room, CHORE_MINING, GET_GLOBAL_INTERACTIONS(mine), INTERACT_MINE, TRUE));
	
	if (can_do) {
		// not able to ewt_mark_resource_worker() until we're inside the interact
		if (worker) {
			if (!run_interactions(worker, GET_GLOBAL_INTERACTIONS(mine), INTERACT_MINE, room, worker, NULL, one_mining_chore)) {
				SET_BIT(MOB_FLAGS(worker), MOB_SPAWNED);
			}
			
			// check for depletion
			if (get_room_extra_data(room, ROOM_EXTRA_MINE_AMOUNT) <= 0) {
				// mark for despawn
				SET_BIT(MOB_FLAGS(worker), MOB_SPAWNED);
				stop_room_action(room, ACT_MINING, CHORE_MINING);
			}
		}
		else {
			if ((worker = place_chore_worker(emp, CHORE_MINING, room))) {
				ewt_mark_for_interaction_list(emp, room, GET_GLOBAL_INTERACTIONS(mine), INTERACT_MINE);
			}
		}
	}
	else if (worker) {
		SET_BIT(MOB_FLAGS(worker), MOB_SPAWNED);
	}
}


void do_chore_minting(empire_data *emp, room_data *room) {
	struct empire_storage_data *highest, *store, *temp;
	char_data *worker = find_chore_worker_in_room(room, chore_data[CHORE_MINTING].mob);
	int high_amt, limit, islid = GET_ISLAND_ID(room);
	bool can_do = TRUE;
	obj_data *orn;
	obj_vnum vnum;
	
	limit = empire_chore_limit(emp, islid, CHORE_MINTING);
	if (EMPIRE_COINS(emp) >= MAX_COIN || (limit != WORKFORCE_UNLIMITED && EMPIRE_COINS(emp) >= limit)) {
		can_do = FALSE;
	}
	
	// detect available treasure
	if (can_do) {
		// first, find the best item to mint
		highest = NULL;
		high_amt = 0;
		for (store = EMPIRE_STORAGE(emp); store; store = store->next) {
			if (store->island != islid) {
				continue;
			}
			
			orn = obj_proto(store->vnum);
			if (orn && store->amount >= 1 && IS_WEALTH_ITEM(orn) && GET_WEALTH_VALUE(orn) > 0 && GET_WEALTH_AUTOMINT(orn)) {
				if (highest == NULL || store->amount > high_amt) {
					highest = store;
					high_amt = store->amount;
				}
			}
		}
	}
	
	if (worker && can_do) {
		// did we find anything at all?
		if (highest) {
			// let's only do this every ~4 hours
			if (!number(0, 3)) {
				return;
			}
			
			vnum = highest->vnum;
			highest->amount = MAX(0, highest->amount - 1);
			
			if (highest->amount == 0) {
				REMOVE_FROM_LIST(highest, EMPIRE_STORAGE(emp), next);
				free(highest);
			}
			
			orn = obj_proto(vnum);	// existence of this was pre-validated
			increase_empire_coins(emp, emp, GET_WEALTH_VALUE(orn) * (1.0/COIN_VALUE));
			
			act("$n finishes minting some coins.", FALSE, worker, NULL, NULL, TO_ROOM);
			empire_skillup(emp, ABIL_WORKFORCE, config_get_double("exp_from_workforce"));
			empire_skillup(emp, ABIL_SKILLED_LABOR, config_get_double("exp_from_workforce"));
		}
		else {
			// nothing remains: mark for despawn
			SET_BIT(MOB_FLAGS(worker), MOB_SPAWNED);
		}
	}
	else if (highest && can_do) {
		worker = place_chore_worker(emp, CHORE_MINTING, room);
	}
	else if (worker) {
		SET_BIT(MOB_FLAGS(worker), MOB_SPAWNED);
	}
}


void do_chore_nailmaking(empire_data *emp, room_data *room) {
	char_data *worker = find_chore_worker_in_room(room, chore_data[CHORE_NAILMAKING].mob);
	int islid = GET_ISLAND_ID(room);
	bool can_do = can_gain_chore_resource(emp, room, CHORE_NAILMAKING, o_NAILS) && empire_can_afford_component(emp, islid, CMP_METAL, CMPF_COMMON, 1);
	
	if (worker && can_do) {
		ewt_mark_resource_worker(emp, room, o_NAILS);
		
		charge_stored_component(emp, islid, CMP_METAL, CMPF_COMMON, 1, NULL);
		add_to_empire_storage(emp, islid, o_NAILS, 4);
		
		act("$n finishes a pouch of nails.", FALSE, worker, NULL, NULL, TO_ROOM);
		empire_skillup(emp, ABIL_WORKFORCE, config_get_double("exp_from_workforce"));
	}
	else if (can_do) {
		// place worker
		if ((worker = place_chore_worker(emp, CHORE_NAILMAKING, room))) {
			ewt_mark_resource_worker(emp, room, o_NAILS);
		}
	}
	else if (worker) {
		SET_BIT(MOB_FLAGS(worker), MOB_SPAWNED);
	}
}


INTERACTION_FUNC(one_quarry_chore) {
	empire_data *emp = ROOM_OWNER(inter_room);
	
	if (emp && can_gain_chore_resource(emp, inter_room, CHORE_QUARRYING, interaction->vnum)) {
		ewt_mark_resource_worker(emp, inter_room, interaction->vnum);
		add_to_empire_storage(emp, GET_ISLAND_ID(inter_room), interaction->vnum, interaction->quantity);
		return TRUE;
	}
	
	return FALSE;
}


void do_chore_quarrying(empire_data *emp, room_data *room) {
	char_data *worker = find_chore_worker_in_room(room, chore_data[CHORE_QUARRYING].mob);
	bool depleted = (get_depletion(room, DPLTN_QUARRY) >= config_get_int("common_depletion")) ? TRUE : FALSE;
	bool can_do = !depleted && can_gain_chore_resource_from_interaction(emp, room, CHORE_QUARRYING, INTERACT_QUARRY);
	
	if (worker && can_do) {
		if (get_room_extra_data(room, ROOM_EXTRA_QUARRY_WORKFORCE_PROGRESS) <= 0) {
			set_room_extra_data(room, ROOM_EXTRA_QUARRY_WORKFORCE_PROGRESS, 24);
			ewt_mark_for_interactions(emp, room, INTERACT_QUARRY);
		}
		else {
			add_to_room_extra_data(room, ROOM_EXTRA_QUARRY_WORKFORCE_PROGRESS, -1);
			if (get_room_extra_data(room, ROOM_EXTRA_QUARRY_WORKFORCE_PROGRESS) == 0) {
				if (run_room_interactions(worker, room, INTERACT_QUARRY, one_quarry_chore)) {
					add_depletion(room, DPLTN_QUARRY, TRUE);
					empire_skillup(emp, ABIL_WORKFORCE, config_get_double("exp_from_workforce"));
				}
				else {
					SET_BIT(MOB_FLAGS(worker), MOB_SPAWNED);
				}
			}
			else {
				ewt_mark_for_interactions(emp, room, INTERACT_QUARRY);
			}
		}
	}
	else if (can_do) {
		// place worker
		if ((worker = place_chore_worker(emp, CHORE_QUARRYING, room))) {
			ewt_mark_for_interactions(emp, room, INTERACT_QUARRY);
		}
	}
	else if (worker) {
		SET_BIT(MOB_FLAGS(worker), MOB_SPAWNED);
	}
}


void do_chore_shearing(empire_data *emp, room_data *room) {
	int shear_growth_time = config_get_int("shear_growth_time");
	
	char_data *worker = find_chore_worker_in_room(room, chore_data[CHORE_SHEARING].mob);
	char_data *mob, *shearable = NULL;
	
	struct interact_exclusion_data *excl = NULL;
	struct interaction_item *interact;
	bool found;

	// find something shearable
	for (mob = ROOM_PEOPLE(room); mob && !shearable; mob = mob->next_in_room) {
		// would its shearing timer be up?
		if (IS_NPC(mob) && !get_cooldown_time(mob, COOLDOWN_SHEAR)) {
			// find shear interaction
			for (interact = mob->interactions; interact && !shearable; interact = interact->next) {
				if (interact->type == INTERACT_SHEAR && can_gain_chore_resource(emp, room, CHORE_SHEARING, interact->vnum)) {
					shearable = mob;
				}
			}
		}
	}
	
	// can work?
	if (worker && shearable) {
		found = FALSE;
		
		// we know it's shearable, but have to find the items
		for (interact = shearable->interactions; interact; interact = interact->next) {
			if (interact->type == INTERACT_SHEAR && check_exclusion_set(&excl, interact->exclusion_code, interact->percent)) {
				// messaging
				if (!found) {
					ewt_mark_resource_worker(emp, room, interact->vnum);
					
					act("$n shears $N.", FALSE, worker, NULL, shearable, TO_ROOM);
					empire_skillup(emp, ABIL_WORKFORCE, config_get_double("exp_from_workforce"));
					found = TRUE;
				}
				
				add_to_empire_storage(emp, GET_ISLAND_ID(room), interact->vnum, interact->quantity);
				add_cooldown(shearable, COOLDOWN_SHEAR, shear_growth_time * SECS_PER_REAL_HOUR);
			}
		}		
	}
	else if (shearable) {
		worker = place_chore_worker(emp, CHORE_SHEARING, room);
		// not able to mark the interaction worker here (no vnum)
	}
	else if (worker) {
		SET_BIT(MOB_FLAGS(worker), MOB_SPAWNED);
	}
	
	free_exclusion_data(excl);
}


void do_chore_trapping(empire_data *emp, room_data *room) {
	int short_depletion = config_get_int("short_depletion");
	
	char_data *worker = find_chore_worker_in_room(room, chore_data[CHORE_TRAPPING].mob);
	obj_vnum vnum = number(0, 1) ? o_SMALL_SKIN : o_LARGE_SKIN;
	bool depleted = get_depletion(room, DPLTN_TRAPPING) >= short_depletion ? TRUE : FALSE;
	bool can_do = !depleted && can_gain_chore_resource(emp, room, CHORE_TRAPPING, vnum);
	
	if (worker && can_do) {
		ewt_mark_resource_worker(emp, room, vnum);
		
		// roughly 1 skin per game day
		if (!number(0, 23)) {
			add_to_empire_storage(emp, GET_ISLAND_ID(room), vnum, 1);
			add_depletion(room, DPLTN_TRAPPING, TRUE);
			empire_skillup(emp, ABIL_WORKFORCE, config_get_double("exp_from_workforce"));
			empire_skillup(emp, ABIL_SKILLED_LABOR, config_get_double("exp_from_workforce"));
		}
	}
	else if (can_do) {
		// place worker
		if ((worker = place_chore_worker(emp, CHORE_TRAPPING, room))) {
			ewt_mark_resource_worker(emp, room, vnum);
		}
	}
	else if (worker) {
		SET_BIT(MOB_FLAGS(worker), MOB_SPAWNED);
	}
}


 /////////////////////////////////////////////////////////////////////////////
//// VEHICLE CHORE FUNCTIONS ////////////////////////////////////////////////

void vehicle_chore_fire_brigade(empire_data *emp, vehicle_data *veh) {
	char_data *worker = find_chore_worker_in_room(IN_ROOM(veh), chore_data[CHORE_FIRE_BRIGADE].mob);
	
	if (worker && VEH_FLAGGED(veh, VEH_ON_FIRE)) {
		REMOVE_BIT(VEH_FLAGS(veh), VEH_ON_FIRE);
		
		act("$n throws a bucket of water to douse the flames!", FALSE, worker, NULL, NULL, TO_ROOM);
		msg_to_vehicle(veh, FALSE, "The flames have been extinguished!\r\n");
		empire_skillup(emp, ABIL_WORKFORCE, 10);	// special case: does not use exp_from_workforce
	}
	else if (VEH_FLAGGED(veh, VEH_ON_FIRE)) {
		if ((worker = place_chore_worker(emp, CHORE_FIRE_BRIGADE, IN_ROOM(veh)))) {
			SET_BIT(MOB_FLAGS(worker), MOB_SPAWNED);	// vehicle chore workers should always be marked SPAWNED right away
		}
	}
}


void vehicle_chore_repair(empire_data *emp, vehicle_data *veh) {
	char_data *worker = find_chore_worker_in_room(IN_ROOM(veh), chore_data[CHORE_REPAIR_VEHICLES].mob);
	struct empire_storage_data *store = NULL;
	int islid = GET_ISLAND_ID(IN_ROOM(veh));
	struct resource_data *res;
	bool can_do = FALSE;
	
	if ((res = VEH_NEEDS_RESOURCES(veh))) {
		// RES_x: can ONLY do it if it requires an object, component, or action
		if (res->type == RES_OBJECT && (store = find_stored_resource(emp, islid, res->vnum)) && store->amount > 0) {
			can_do = TRUE;
		}
		else if (res->type == RES_COMPONENT && empire_can_afford_component(emp, islid, res->vnum, res->misc, 1)) {
			can_do = TRUE;
		}
		else if (res->type == RES_ACTION) {
			can_do = TRUE;
		}
	}
	
	if (worker && can_do) {
		if (res) {
			empire_skillup(emp, ABIL_WORKFORCE, config_get_double("exp_from_workforce"));
			
			if (res->type == RES_OBJECT) {
				charge_stored_resource(emp, islid, res->vnum, 1);
			}
			else if (res->type == RES_COMPONENT) {
				charge_stored_component(emp, islid, res->vnum, res->misc, 1, NULL);
			}
			// apply it
			res->amount -= 1;
			
			// remove res?
			if (res->amount <= 0) {
				LL_DELETE(VEH_NEEDS_RESOURCES(veh), res);
				free(res);
			}
		}
		
		// check for completion
		if (!VEH_NEEDS_RESOURCES(veh)) {
			act("$n finishes repairing $V.", FALSE, worker, NULL, veh, TO_ROOM);
			REMOVE_BIT(VEH_FLAGS(veh), VEH_INCOMPLETE);
			VEH_HEALTH(veh) = VEH_MAX_HEALTH(veh);
		}
	}
	else if (can_do) {
		if ((worker = place_chore_worker(emp, CHORE_REPAIR_VEHICLES, IN_ROOM(veh)))) {
			SET_BIT(MOB_FLAGS(worker), MOB_SPAWNED);	// vehicle chore workers should always be marked SPAWNED right away
		}
	}
}
