/* ************************************************************************
*   File: workforce.c                                     EmpireMUD 2.0b5 *
*  Usage: functions related to npc chores and workforce                   *
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
#include "comm.h"
#include "handler.h"
#include "db.h"
#include "skills.h"
#include "vnums.h"
#include "constants.h"

/**
* Contents:
*   Data
*   Main Chore Control
*   EWT Tracker
*   Workforce Where Functions
*   Helpers
*   Generic Craft Workforce
*   Chore Functions
*   Vehicle Chore Functions
*/


// local chore prototypes
void do_chore_building(empire_data *emp, room_data *room, int mode);
void do_chore_burn_stumps(empire_data *emp, room_data *room);
void do_chore_chopping(empire_data *emp, room_data *room);
void do_chore_dismantle(empire_data *emp, room_data *room);
void do_chore_dismantle_mines(empire_data *emp, room_data *room, vehicle_data *veh);
void do_chore_einv_interaction(empire_data *emp, room_data *room, vehicle_data *veh, int chore, int interact_type);
void do_chore_farming(empire_data *emp, room_data *room);
void do_chore_fishing(empire_data *emp, room_data *room, vehicle_data *veh);
void do_chore_fire_brigade(empire_data *emp, room_data *room);
void do_chore_gen_craft(empire_data *emp, room_data *room, vehicle_data *veh, int chore, CHORE_GEN_CRAFT_VALIDATOR(*validator), bool is_skilled);
void do_chore_mining(empire_data *emp, room_data *room, vehicle_data *veh);
void do_chore_minting(empire_data *emp, room_data *room, vehicle_data *veh);
void do_chore_production(empire_data *emp, room_data *room, vehicle_data *veh, int interact_type);
void do_chore_prospecting(empire_data *emp, room_data *room);
void do_chore_shearing(empire_data *emp, room_data *room, vehicle_data *veh);
void vehicle_chore_fire_brigade(empire_data *emp, vehicle_data *veh);
void vehicle_chore_build(empire_data *emp, vehicle_data *veh, int chore);
void vehicle_chore_dismantle(empire_data *emp, vehicle_data *veh);
void workforce_crafting_chores(empire_data *emp, room_data *room, vehicle_data *veh);

// gen_craft prototypes
CHORE_GEN_CRAFT_VALIDATOR(chore_milling);
CHORE_GEN_CRAFT_VALIDATOR(chore_pressing);
CHORE_GEN_CRAFT_VALIDATOR(chore_smelting);
CHORE_GEN_CRAFT_VALIDATOR(chore_weaving);
CHORE_GEN_CRAFT_VALIDATOR(chore_workforce_crafting);

// other local prototypes
int get_workforce_production_limit(empire_data *emp, obj_vnum vnum);
int empire_chore_limit(empire_data *emp, int island_id, int chore);
void log_workforce_problem(empire_data *emp, room_data *room, int chore, int problem, bool is_delay);
void report_workforce_production_log(empire_data *emp);
bool workforce_is_delayed(empire_data *emp, room_data *room, int chore);


 /////////////////////////////////////////////////////////////////////////////
//// DATA ///////////////////////////////////////////////////////////////////

#define MIN_WORKER_POS  POS_SITTING	// minimum position for a worker to be used (otherwise it will spawn another worker)

// CHORE_x
struct empire_chore_type chore_data[NUM_CHORES] = {
	// name, npc vnum, hidden, requires tech
	{ "building", BUILDER, FALSE, NOTHING },
	{ "farming", FARMER, FALSE, NOTHING },
	{ "replanting", NOTHING, FALSE, NOTHING },
	{ "chopping", FELLER, FALSE, NOTHING },
	{ "maintenance", REPAIRMAN, FALSE, NOTHING },
	{ "mining", MINER, FALSE, NOTHING },
		{ "unused", NOTHING, TRUE, NOTHING },
	{ "sawing", SAWYER, FALSE, NOTHING },
	{ "scraping", SCRAPER, FALSE, NOTHING },
	{ "smelting", SMELTER, FALSE, NOTHING },
	{ "weaving", WEAVER, FALSE, NOTHING },
	{ "production", PRODUCTION_ASSISTANT, FALSE, NOTHING },
	{ "crafting", CRAFTING_APPRENTICE, FALSE, NOTHING },
		{ "unused", NOTHING, TRUE, NOTHING },
	{ "abandon-dismantled", NOTHING, FALSE, NOTHING },
		{ "unused", NOTHING, TRUE, NOTHING },
	{ "fire-brigade", FIRE_BRIGADE, FALSE, NOTHING },
		{ "unused", NOTHING, TRUE, NOTHING },
	{ "tanning", TANNER, FALSE, NOTHING },
	{ "shearing", SHEARER, FALSE, NOTHING },
	{ "minting", COIN_MAKER, FALSE, TECH_SKILLED_LABOR },
	{ "dismantle-mines", MINE_SUPERVISOR, FALSE, NOTHING },
	{ "abandon-chopped", NOTHING, FALSE, NOTHING },
	{ "abandon-farmed", NOTHING, FALSE, NOTHING },
		{ "unused", NOTHING, TRUE, NOTHING },
	{ "milling", MILL_WORKER, FALSE, NOTHING },
		{ "unused", NOTHING, TRUE, NOTHING },	// note: formerly repair-vehicles, which merged with maintenance
	{ "oilmaking", PRESS_WORKER, FALSE, NOTHING },
	{ "general", NOTHING, TRUE, NOTHING },
	{ "fishing", FISHERMAN, FALSE, NOTHING },
	{ "burn-stumps", STUMP_BURNER, FALSE, NOTHING },
	{ "prospecting", PROSPECTOR, FALSE, TECH_WORKFORCE_PROSPECTING },
		{ "unused", NOTHING, TRUE, NOTHING },
};


// global for which chore type might be running
int einv_interaction_chore_type = 0;


#define CHORE_ACTIVE(chore)  (empire_chore_limit(emp, island, (chore)) != 0 && !workforce_is_delayed(emp, room, (chore)) && (chore_data[(chore)].requires_tech == NOTHING || EMPIRE_HAS_TECH(emp, chore_data[(chore)].requires_tech)))
#define GET_CHORE_DEPLETION(room, veh, type)  ((veh) ? get_vehicle_depletion((veh), (type)) : get_depletion((room), (type)))
#define ADD_CHORE_DEPLETION(room, veh, type, multiple)  { if (veh) { add_vehicle_depletion((veh), (type), (multiple)); } else { add_depletion((room), (type), (multiple)); } }


 /////////////////////////////////////////////////////////////////////////////
//// MAIN CHORE CONTROL /////////////////////////////////////////////////////


/**
* This runs once an hour on every empire room. It will try to run a chore on
* that room, if applicable. This is only called if the empire has Workforce.
*
* @param empire_data *emp the empire -- a shortcut to prevent re-detecting
* @param room_data *room
*/
void process_one_chore(empire_data *emp, room_data *room) {
	int island = GET_ISLAND_ID(room);	// just look this up once
	
	// basic vars that determine what we do:
	bool no_work = ROOM_AFF_FLAGGED(room, ROOM_AFF_NO_WORK) ? TRUE : FALSE;
	bool has_instance = ROOM_AFF_FLAGGED(room, ROOM_AFF_HAS_INSTANCE) ? TRUE : FALSE;
	bool starving = empire_has_needs_status(emp, GET_ISLAND_ID(room), ENEED_WORKFORCE, ENEED_STATUS_UNSUPPLIED);
	
	// THING 1: burning
	if (IS_BURNING(room)) {
		if (!starving && CHORE_ACTIVE(CHORE_FIRE_BRIGADE)) {
			do_chore_fire_brigade(emp, room);
		}
		return;	// blocks all other chores
	}
	if (no_work) {
		return;	// only burning overrides no-work
	}
	
	// THING 2: crops/fishing (never blocked by workforce starvation)
	if (ROOM_SECT_FLAGGED(room, SECTF_CROP) && !ROOM_AFF_FLAGGED(room, ROOM_AFF_NO_EVOLVE | ROOM_AFF_NO_WORKFORCE_EVOS) && CHORE_ACTIVE(CHORE_FARMING) && !IS_BURNING(room)) {
		if (!has_instance || CHORE_ACTIVE(CHORE_REPLANTING)) {
			// work farming while an instance is present ONLY if replanting is on
			do_chore_farming(emp, room);
		}
	}
	if (IS_COMPLETE(room) && room_has_function_and_city_ok(emp, room, FNC_FISHING) && CHORE_ACTIVE(CHORE_FISHING)) {
		do_chore_fishing(emp, room, NULL);
	}
	if (starving) {
		return;	// done
	}
	
	// THING 3: BUILDING/MAINTENANCE (these block further chores)
	if ((IS_INCOMPLETE(room) || IS_DISMANTLING(room)) && CHORE_ACTIVE(CHORE_BUILDING)) {
		if (!IS_DISMANTLING(room)) {
			do_chore_building(emp, room, CHORE_BUILDING);
		}
		else if (!has_instance) {
			// dismantle blocked by instance
			do_chore_dismantle(emp, room);
		}
		
		// do not trigger other actions in the same room in 1 cycle
		return;
	}
	if (IS_COMPLETE(room) && (BUILDING_DAMAGE(room) > 0 || BUILDING_RESOURCES(room)) && HOME_ROOM(room) == room && CHORE_ACTIVE(CHORE_MAINTENANCE)) {
		do_chore_building(emp, room, CHORE_MAINTENANCE);
		return;	// no further work while undergoing maintenance
	}
	
	// THING 4: IN-CITY CHECK and NO-INSTANCE: everything else must pass this to run
	// NOTE: Further chores can use HAS_FUNCTION instead of room_has_function_and_city_ok
	if (!check_in_city_requirement(room, TRUE)) {
		log_workforce_problem(emp, room, CHORE_GENERAL, WF_PROB_OUT_OF_CITY, FALSE);
		return;
	}
	if (has_instance) {	// everything else is blocked by an instance
		log_workforce_problem(emp, room, CHORE_GENERAL, WF_PROB_ADVENTURE_PRESENT, FALSE);
		return;
	}
	
	// THING 5: Outdoor/non-building chores
	if (CHORE_ACTIVE(CHORE_BURN_STUMPS) && !ROOM_AFF_FLAGGED(room, ROOM_AFF_NO_EVOLVE | ROOM_AFF_NO_WORKFORCE_EVOS) && has_evolution_type(SECT(room), EVO_BURN_STUMPS)) {
		do_chore_burn_stumps(emp, room);
		return;
	}
	if (CHORE_ACTIVE(CHORE_CHOPPING) && !ROOM_CROP(room) && !ROOM_AFF_FLAGGED(room, ROOM_AFF_NO_EVOLVE | ROOM_AFF_NO_WORKFORCE_EVOS) && (has_evolution_type(SECT(room), EVO_CHOPPED_DOWN) || CAN_INTERACT_ROOM_NO_VEH((room), INTERACT_CHOP))) {
		// All choppables -- except crops, which are handled by farming
		do_chore_chopping(emp, room);
		return;
	}
	if (CHORE_ACTIVE(CHORE_PROSPECTING) && EMPIRE_HAS_TECH(emp, TECH_WORKFORCE_PROSPECTING) && !GET_BUILDING(room) && ROOM_CAN_MINE(room)) {
		// Any non-building that's prospectable
		do_chore_prospecting(emp, room);
		return;
	}
	
	// THING 6: all other chores (would be blocked by incompleteness)
	if (!IS_COMPLETE(room)) {
		return;
	}
	
	// function chores
	if (HAS_FUNCTION(room, FNC_STABLE) && CHORE_ACTIVE(CHORE_SHEARING)) {
		do_chore_shearing(emp, room, NULL);
	}
	if (HAS_FUNCTION(room, FNC_MINE)) {
		if (get_room_extra_data(room, ROOM_EXTRA_MINE_AMOUNT) > 0 && CHORE_ACTIVE(CHORE_MINING)) {
			do_chore_mining(emp, room, NULL);
		}
		else if (get_room_extra_data(room, ROOM_EXTRA_MINE_AMOUNT) <= 0 && IS_MAP_BUILDING(room) && !ROOM_AFF_FLAGGED(room, ROOM_AFF_NO_DISMANTLE) && CHORE_ACTIVE(CHORE_DISMANTLE_MINES)) {
			do_chore_dismantle_mines(emp, room, NULL);	// no ore left
		}
	}
	if (HAS_FUNCTION(room, FNC_MINT) && CHORE_ACTIVE(CHORE_MINTING)) {
		do_chore_minting(emp, room, NULL);
	}
	
	// gen-craft chores
	workforce_crafting_chores(emp, room, NULL);
	if (HAS_FUNCTION(room, FNC_MILL) && CHORE_ACTIVE(CHORE_MILLING)) {
		do_chore_gen_craft(emp, room, NULL, CHORE_MILLING, chore_milling, FALSE);
	}
	if (HAS_FUNCTION(room, FNC_PRESS) && CHORE_ACTIVE(CHORE_OILMAKING)) {
		do_chore_gen_craft(emp, room, NULL, CHORE_OILMAKING, chore_pressing, FALSE);
	}
	if (HAS_FUNCTION(room, FNC_SMELT) && CHORE_ACTIVE(CHORE_SMELTING)) {
		do_chore_gen_craft(emp, room, NULL, CHORE_SMELTING, chore_smelting, FALSE);
	}
	if (HAS_FUNCTION(room, FNC_TAILOR) && CHORE_ACTIVE(CHORE_WEAVING)) {
		do_chore_gen_craft(emp, room, NULL, CHORE_WEAVING, chore_weaving, FALSE);
	}
	
	// room interaction chores
	if (CHORE_ACTIVE(CHORE_PRODUCTION) && EMPIRE_HAS_TECH(emp, TECH_SKILLED_LABOR) && CAN_INTERACT_ROOM_NO_VEH(room, INTERACT_SKILLED_LABOR)) {
		// skilled production
		do_chore_production(emp, room, NULL, INTERACT_SKILLED_LABOR);
	}
	if (CHORE_ACTIVE(CHORE_PRODUCTION) && CAN_INTERACT_ROOM_NO_VEH(room, INTERACT_PRODUCTION)) {
		// unskilled production
		do_chore_production(emp, room, NULL, INTERACT_PRODUCTION);
	}
	
	// obj interaction chores
	if (HAS_FUNCTION(room, FNC_SAW) && CHORE_ACTIVE(CHORE_SCRAPING)) {
		do_chore_einv_interaction(emp, room, NULL, CHORE_SCRAPING, INTERACT_SCRAPE);
	}
	if (HAS_FUNCTION(room, FNC_TANNERY) && CHORE_ACTIVE(CHORE_TANNING)) {
		do_chore_einv_interaction(emp, room, NULL, CHORE_TANNING, INTERACT_TAN);
	}
	if (HAS_FUNCTION(room, FNC_SAW) && CHORE_ACTIVE(CHORE_SAWING)) {
		do_chore_einv_interaction(emp, room, NULL, CHORE_SAWING, INTERACT_SAW);
	}
}


/**
* Runs a single vehicle-related chore
*/
void process_one_vehicle_chore(empire_data *emp, vehicle_data *veh) {
	room_data *room = IN_ROOM(veh);
	bool on_fire, starving;
	int island;
	
	if (!room) {
		return;
	}
	
	// basic vars that determine what we do:
	on_fire = VEH_FLAGGED(veh, VEH_ON_FIRE) ? TRUE : FALSE;
	starving = empire_has_needs_status(emp, GET_ISLAND_ID(room), ENEED_WORKFORCE, ENEED_STATUS_UNSUPPLIED);
	
	// basic checks
	if ((VEH_FLAGGED(veh, VEH_PLAYER_NO_WORK) || (VEH_FLAGGED(veh, VEH_BUILDING) && ROOM_AFF_FLAGGED(room, ROOM_AFF_NO_WORK))) && !on_fire) {
		return;	// skip workforce if no-work AND not-on-fire
	}
	if ((island = GET_ISLAND_ID(room)) == NO_ISLAND) {
		return;	// no outside work off-island
	}
	if (ROOM_OWNER(room) && ROOM_OWNER(room) != emp && !on_fire && !has_relationship(emp, ROOM_OWNER(room), DIPL_ALLIED)) {
		return;	// cannot work in non-allied empires
	}
	if (!vehicle_allows_climate(veh, room, NULL)) {
		return;	// cannot work when the vehicle is in forbidden climates (it's ruining itself)
	}
	
	// PART 1: burning
	if (on_fire) {
		if (!starving && empire_chore_limit(emp, island, CHORE_FIRE_BRIGADE)) {
			vehicle_chore_fire_brigade(emp, veh);
		}
		return;	// prevent other chores from firing while burning
	}
	
	// PART 2: chores that happen even if starving (food chores)
	if (!on_fire && VEH_HEALTH(veh) >= 1 && vehicle_has_function_and_city_ok(veh, FNC_FISHING) && CHORE_ACTIVE(CHORE_FISHING)) {
		do_chore_fishing(emp, room, veh);
	}
	if (starving) {
		return;	// STARVATION: blocks further chores
	}
	
	// PART 3: BUILDING/REPAIR
	if (VEH_NEEDS_RESOURCES(veh) || !VEH_IS_COMPLETE(veh)) {
		if (VEH_IS_DISMANTLING(veh) && empire_chore_limit(emp, island, CHORE_BUILDING)) {
			vehicle_chore_dismantle(emp, veh);
		}
		else if (!VEH_IS_COMPLETE(veh) && empire_chore_limit(emp, island, CHORE_BUILDING)) {
			vehicle_chore_build(emp, veh, CHORE_BUILDING);
		}
		else if (empire_chore_limit(emp, island, CHORE_MAINTENANCE)) {
			vehicle_chore_build(emp, veh, CHORE_MAINTENANCE);
		}
		return;	// no further chores while working on the building (dismantle may even have purged it)
	}
	
	// PART 4: other chores (unlike room chores, you must check city status with vehicle_has_function_and_city_ok)
	if (VEH_HEALTH(veh) < 1) {
		return;	// can only fire-brigade/repair if low health
	}
	
	// function chores:
	if (vehicle_has_function_and_city_ok(veh, FNC_STABLE) && CHORE_ACTIVE(CHORE_SHEARING)) {
		do_chore_shearing(emp, room, veh);
	}
	if (vehicle_has_function_and_city_ok(veh, FNC_MINE)) {
		if (get_room_extra_data(room, ROOM_EXTRA_MINE_AMOUNT) > 0 && CHORE_ACTIVE(CHORE_MINING)) {
			do_chore_mining(emp, room, veh);
		}
		else if (get_room_extra_data(room, ROOM_EXTRA_MINE_AMOUNT) <= 0 && !VEH_FLAGGED(veh, VEH_NEVER_DISMANTLE | VEH_PLAYER_NO_DISMANTLE) && CHORE_ACTIVE(CHORE_DISMANTLE_MINES)) {
			do_chore_dismantle_mines(emp, room, veh);	// no ore left
		}
	}
	if (vehicle_has_function_and_city_ok(veh, FNC_MINT) && CHORE_ACTIVE(CHORE_MINTING)) {
		do_chore_minting(emp, room, veh);
	}
	
	// gen-craft chores:
	workforce_crafting_chores(emp, room, veh);
	if (vehicle_has_function_and_city_ok(veh, FNC_SMELT) && CHORE_ACTIVE(CHORE_SMELTING)) {
		do_chore_gen_craft(emp, room, veh, CHORE_SMELTING, chore_smelting, FALSE);
	}
	if (vehicle_has_function_and_city_ok(veh, FNC_TAILOR) && CHORE_ACTIVE(CHORE_WEAVING)) {
		do_chore_gen_craft(emp, room, veh, CHORE_WEAVING, chore_weaving, FALSE);
	}
	if (vehicle_has_function_and_city_ok(veh, FNC_MILL) && CHORE_ACTIVE(CHORE_MILLING)) {
		do_chore_gen_craft(emp, room, veh, CHORE_MILLING, chore_milling, FALSE);
	}
	if (vehicle_has_function_and_city_ok(veh, FNC_PRESS) && CHORE_ACTIVE(CHORE_OILMAKING)) {
		do_chore_gen_craft(emp, room, veh, CHORE_OILMAKING, chore_pressing, FALSE);
	}
	
	// vehicle interaction chores
	if (has_interaction(VEH_INTERACTIONS(veh), INTERACT_SKILLED_LABOR) && EMPIRE_HAS_TECH(emp, TECH_SKILLED_LABOR) && CHORE_ACTIVE(CHORE_PRODUCTION)) {
		// skilled production
		do_chore_production(emp, room, veh, INTERACT_SKILLED_LABOR);
	}
	if (has_interaction(VEH_INTERACTIONS(veh), INTERACT_PRODUCTION) && CHORE_ACTIVE(CHORE_PRODUCTION)) {
		// unskilled production
		do_chore_production(emp, room, veh, INTERACT_PRODUCTION);
	}
	
	// object interaction chores:
	if (vehicle_has_function_and_city_ok(veh, FNC_SAW) && CHORE_ACTIVE(CHORE_SAWING)) {
		do_chore_einv_interaction(emp, room, veh, CHORE_SAWING, INTERACT_SAW);
	}
	if (vehicle_has_function_and_city_ok(veh, FNC_SAW) && CHORE_ACTIVE(CHORE_SCRAPING)) {
		do_chore_einv_interaction(emp, room, veh, CHORE_SCRAPING, INTERACT_SCRAPE);
	}
	if (vehicle_has_function_and_city_ok(veh, FNC_TANNERY) && CHORE_ACTIVE(CHORE_TANNING)) {
		do_chore_einv_interaction(emp, room, veh, CHORE_TANNING, INTERACT_TAN);
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
	struct empire_island *eisle, *next_eisle;
	struct empire_workforce_tracker *tt;
	struct empire_storage_data *store;
	struct shipping_data *shipd;
	
	HASH_FIND_INT(EMPIRE_WORKFORCE_TRACKER(emp), &vnum, tt);
	if (!tt) {
		CREATE(tt, struct empire_workforce_tracker, 1);
		tt->vnum = vnum;
		HASH_ADD_INT(EMPIRE_WORKFORCE_TRACKER(emp), vnum, tt);
		
		// scan for data
		HASH_ITER(hh, EMPIRE_ISLANDS(emp), eisle, next_eisle) {
			HASH_FIND_INT(eisle->store, &vnum, store);
			if (store && store->amount > 0) {
				SAFE_ADD(tt->total_amount, store->amount, 0, INT_MAX, FALSE);
				isle = ewt_find_island(tt, eisle->island);
				SAFE_ADD(isle->amount, store->amount, 0, INT_MAX, FALSE);
			}
		}
	
		// count shipping, too
		DL_FOREACH(EMPIRE_SHIPPING_LIST(emp), shipd) {
			if (shipd->vnum == vnum) {
				SAFE_ADD(tt->total_amount, shipd->amount, 0, INT_MAX, FALSE);
				
				if (shipd->status == SHIPPING_QUEUED || shipd->status == SHIPPING_WAITING_FOR_SHIP) {
					isle = ewt_find_island(tt, shipd->from_island);
					SAFE_ADD(isle->amount, shipd->amount, 0, INT_MAX, FALSE);
				}
				else {
					isle = ewt_find_island(tt, shipd->to_island);
					SAFE_ADD(isle->amount, shipd->amount, 0, INT_MAX, FALSE);
				}
			}
		}
		
		// aaand dropped items
		SAFE_ADD(tt->total_amount, count_dropped_items(emp, vnum), 0, INT_MAX, FALSE);
	}
	
	return tt;
}


/**
* Tallies a worker on a resource, to help with limits.
*
* @param empire_data *emp The empire that's working.
* @param room_data *loc Where they're working (for island info).
* @param obj_vnum vnum Which resource.
* @param int quantity How many of the resource would be worked.
*/
static void ewt_mark_resource_worker(empire_data *emp, room_data *loc, obj_vnum vnum, int quantity) {
	struct empire_workforce_tracker_island *isle;
	struct empire_workforce_tracker *tt;
	
	// safety
	if (!emp || !loc || vnum == NOTHING || quantity < 1) {
		return;
	}
	
	tt = ewt_find_tracker(emp, vnum);
	isle = ewt_find_island(tt, GET_ISLAND_ID(loc));
	
	tt->total_workers += quantity;
	isle->workers += quantity;
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
	struct vnum_hash *vhash = NULL, *item;
	struct interaction_item *interact;
	int amount;
	
	LL_FOREACH(list, interact) {
		// should this be checking meets_interaction_restrictions() ?
		if (interact->type == interaction_type) {
			if (interact_data[interact->type].one_at_a_time) {
				// 1 at a time
				ewt_mark_resource_worker(emp, location, interact->vnum, 1);
			}
			else {
				// not 1-at-a-time: ensure we don't overcount if multiple interactions have the same item
				if ((item = find_in_vnum_hash(vhash, interact->vnum))) {
					amount = interact->quantity - item->count;
				}
				else {
					// not found -- just add full amount
					amount = interact->quantity;
				}
				
				// add if needed
				if (amount > 0) {
					ewt_mark_resource_worker(emp, location, interact->vnum, amount);
					add_vnum_hash(&vhash, interact->vnum, amount);
				}
			}
		}
	}
	
	free_vnum_hash(&vhash);
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
	if (ROOM_SECT_FLAGGED(room, SECTF_CROP) && ROOM_CROP(room)) {
		ewt_mark_for_interaction_list(emp, room, GET_CROP_INTERACTIONS(ROOM_CROP(room)), interaction_type);
	}
	if (GET_BUILDING(room)) {
		ewt_mark_for_interaction_list(emp, room, GET_BLD_INTERACTIONS(GET_BUILDING(room)), interaction_type);
	}
}


 /////////////////////////////////////////////////////////////////////////////
//// WORKFORCE WHERE FUNCTIONS //////////////////////////////////////////////

/**
* Frees any workforce-where log and sets the list to NULL.
*
* @param struct workforce_where_log **to_free A pointer to the list to free.
*/
void free_workforce_where_log(struct workforce_where_log **to_free) {
	struct workforce_where_log *wwl, *next;
	
	if (to_free) {
		DL_FOREACH_SAFE(*to_free, wwl, next) {
			free(wwl);
		}
		*to_free = NULL;
	}
}


/**
* Logs a mob as having worked a chore during this cycle (these logs are wiped
* during every cycle).
*
* @param empire_data *emp The empire to log to.
* @param char_data *mob The mob who did the work.
* @param int chore The CHORE_ performed.
*/
void log_workforce_where(empire_data *emp, char_data *mob, int chore) {
	struct workforce_where_log *wwl;
	
	if (!emp || !mob) {
		return;	// no work
	}
	
	CREATE(wwl, struct workforce_where_log, 1);
	wwl->mob = mob;
	wwl->chore = chore;
	wwl->loc = GET_ROOM_VNUM(IN_ROOM(mob));
	DL_APPEND(EMPIRE_WORKFORCE_WHERE_LOG(emp), wwl);
}


/**
* When a mob is purged or loses its loyalty, call this to ensure it's not in
* a 'workforce where' list.
*
* @param empire_data *emp The empire to remove from, if present.
* @param char_data *mob The mob to remove, if present.
*/
void remove_from_workforce_where_log(empire_data *emp, char_data *mob) {
	struct workforce_where_log *wwl, *next;
	
	if (emp) {
		DL_FOREACH_SAFE(EMPIRE_WORKFORCE_WHERE_LOG(emp), wwl, next) {
			if (wwl->mob == mob) {
				DL_DELETE(EMPIRE_WORKFORCE_WHERE_LOG(emp), wwl);
				free(wwl);
			}
		}
	}
}


 /////////////////////////////////////////////////////////////////////////////
//// HELPERS ////////////////////////////////////////////////////////////////

/**
* Marks production by workforce.
*
* @param empire_data *emp Which empire.
* @param int type Any WPLOG_ type.
* @param any_vnum vnum Usually an object, building, etc produced; corresponding to type.
* @param int amount How many were produced.
*/
void add_workforce_production_log(empire_data *emp, int type, any_vnum vnum, int amount) {
	struct workforce_production_log *wplog = NULL, *iter;
	
	if (!emp) {
		return;
	}
	
	// find
	LL_FOREACH(EMPIRE_PRODUCTION_LOGS(emp), iter) {
		if (iter->type == type && iter->vnum == vnum) {
			wplog = iter;
			break;
		}
	}
	
	// or create
	if (!wplog) {
		CREATE(wplog, struct workforce_production_log, 1);
		wplog->type = type;
		wplog->vnum = vnum;
		LL_PREPEND(EMPIRE_PRODUCTION_LOGS(emp), wplog);
	}
	
	// and tally
	SAFE_ADD(wplog->amount, amount, 0, INT_MAX, FALSE);
	EMPIRE_NEEDS_STORAGE_SAVE(emp) = TRUE;
}


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
	int island_id, max, glob_max, limit;
	struct empire_workforce_tracker *tt;
	struct empire_island *emp_isle;
	obj_data *proto;
	
	// safety
	if (!emp || !loc || vnum == NOTHING || !(proto = obj_proto(vnum)) || !GET_OBJ_STORAGE(proto)) {
		return FALSE;
	}
	
	// first look up data for this vnum
	tt = ewt_find_tracker(emp, vnum);
	
	// data is assumed to be accurate now
	island_id = GET_ISLAND_ID(loc);
	isle = ewt_find_island(tt, island_id);
	
	// base cap
	max = config_get_int("max_chore_resource_per_member") * EMPIRE_MEMBERS(emp);
	max += EMPIRE_ATTRIBUTE(emp, EATT_WORKFORCE_CAP);
	
	glob_max = max;	// does not change
	
	// check empire's own limit
	if ((emp_isle = get_empire_island(emp, island_id))) {
		if (emp_isle->workforce_limit[chore] > 0 && emp_isle->workforce_limit[chore] < max) {
			max = emp_isle->workforce_limit[chore];
		}
	}
	
	// check manual production limit
	if ((limit = get_workforce_production_limit(emp, vnum)) != UNLIMITED && limit < max) {
		max = limit;
	}

	// do we have too much?
	if (tt->total_amount + tt->total_workers >= glob_max) {
		if (isle->amount + isle->workers < config_get_int("max_chore_resource_over_total") && isle->amount + isle->workers < max) {
			return TRUE;
		}
	}
	else {
		if (isle->amount + isle->workers < max) {
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
		if (!GET_OBJ_STORAGE(proto)) {
			continue;	// MUST be storable
		}
		if (!meets_interaction_restrictions(interact->restrictions, NULL, emp, NULL, NULL)) {
			continue;
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
bool can_gain_chore_resource_from_interaction_room(empire_data *emp, room_data *room, int chore, int interaction_type) {
	bool found_any = FALSE;
	crop_data *cp;
	
	found_any |= can_gain_chore_resource_from_interaction_list(emp, room, chore, GET_SECT_INTERACTIONS(SECT(room)), interaction_type, FALSE);
	if (!found_any && ROOM_SECT_FLAGGED(room, SECTF_CROP) && (cp = ROOM_CROP(room))) {
		found_any |= can_gain_chore_resource_from_interaction_list(emp, room, chore, GET_CROP_INTERACTIONS(cp), interaction_type, FALSE);
	}
	if (!found_any && GET_BUILDING(room)) {
		found_any |= can_gain_chore_resource_from_interaction_list(emp, room, chore, GET_BLD_INTERACTIONS(GET_BUILDING(room)), interaction_type, FALSE);
	}
	
	return found_any;
}


/**
* Marks the empire and/or worker as having done some 'work'.
*
* @param empire_data *emp The empire whose workforce it is.
* @param int chore The CHORE_ being performed.
* @param room_data *room Optional: The location of the work (required for food_need but may be NULL otherwise).
* @param char_data *worker Optional: The worker NPC: will reset their spawn time.
* @param int food_need Optional: Add to the food 'need' that the empire must pay to continue having a workforce (pass 0 if no-charge). Requires the 'room' var.
* @param any_vnum resource Optional: Mark that someone is working a resource here (use NOTHING to skip this).
* @param int res_amt Optional: How many of the 'resource' were added, if any.
*/
void charge_workforce(empire_data *emp, int chore, room_data *room, char_data *worker, int food_need, any_vnum resource, int res_amount) {
	if (food_need && room) {
		add_empire_needs(emp, GET_ISLAND_ID(room), ENEED_WORKFORCE, food_need);
	}
	
	if (worker) {
		// update spawn time as they are still working (prevent despawn)
		// this also blocks another chore from grabbing them during this cycle
		set_mob_spawn_time(worker, time(0));
		
		// log for workforce-where
		log_workforce_where(emp, worker, chore);
	}
	
	if (resource != NOTHING && room) {
		ewt_mark_resource_worker(emp, room, resource, res_amount);
	}
}


/**
* This runs once per mud hour to update all empire chores.
*/
void chore_update(void) {
	struct empire_territory_data *ter, *next_ter;
	struct empire_island *eisle, *next_eisle;
	struct empire_needs *needs, *next_needs;
	struct workforce_log *wf_log;
	vehicle_data *veh;
	empire_data *emp, *next_emp;
	
	int time_to_empire_emptiness = config_get_int("time_to_empire_emptiness") * SECS_PER_REAL_WEEK;

	HASH_ITER(hh, empire_table, emp, next_emp) {
		// skip idle empires
		if (EMPIRE_LAST_LOGON(emp) + time_to_empire_emptiness < time(0)) {
			continue;
		}
		
		sort_einv_for_empire(emp);
		
		// update islands
		HASH_ITER(hh, EMPIRE_ISLANDS(emp), eisle, next_eisle) {
			// run needs and logs (8pm only)
			if (main_time_info.hours == 20) {
				// TODO: currently this runs 1 need at a time, but could probably save a lot of processing if it ran all needs at once
				HASH_ITER(hh, eisle->needs, needs, next_needs) {
					if (needs->needed > 0 && !EMPIRE_IMM_ONLY(emp)) {
						update_empire_needs(emp, eisle, needs);
					}
					else {
						REMOVE_BIT(needs->status, ENEED_STATUS_UNSUPPLIED);
					}
				}
			}
		}
		
		// run chores
		if (EMPIRE_HAS_TECH(emp, TECH_WORKFORCE)) {
			// free old log entries
			free_workforce_where_log(&EMPIRE_WORKFORCE_WHERE_LOG(emp));
			while ((wf_log = EMPIRE_WORKFORCE_LOG(emp))) {
				EMPIRE_WORKFORCE_LOG(emp) = wf_log->next;
				free(wf_log);
			}
			
			// this uses a global next to avoid an issue where territory is freed mid-execution
			global_next_territory_entry = NULL;
			HASH_ITER(hh, EMPIRE_TERRITORY_LIST(emp), ter, next_ter) {
				global_next_territory_entry = next_ter;
				process_one_chore(emp, ter->room);
				next_ter = global_next_territory_entry;
			}
			
			DL_FOREACH_SAFE(vehicle_list, veh, global_next_vehicle) {
				if (VEH_OWNER(veh) == emp && !VEH_IS_EXTRACTED(veh)) {
					process_one_vehicle_chore(emp, veh);
				}
			}
			
			read_vault(emp);
			
			// no longer need this -- free up the tracker
			ewt_free_tracker(&EMPIRE_WORKFORCE_TRACKER(emp));
		}
		
		// report production now, only at 8pm, like needs
		if (main_time_info.hours == 20) {
			report_workforce_production_log(emp);
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
		DL_FOREACH_SAFE(character_list, mob, next_mob) {
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
			
			// attempt to despawn
			if (!FIGHTING(mob)) {
				act("$n leaves to do other work.", TRUE, mob, NULL, NULL, TO_ROOM);
				extract_char(mob);
			}
			else {
				// mark for despawn just in case
				set_mob_flags(mob, MOB_SPAWNED);
			}
		}
	}
}


/**
* This deactivates the workforce for an empire on an island.
*
* @param empire_data *emp
* @param int island_id Which island (or NO_ISLAND for all)
*/
void deactivate_workforce_island(empire_data *emp, int island_id) {
	char_data *mob, *next_mob;
	bool found;
	int iter;
	
	DL_FOREACH_SAFE(character_list, mob, next_mob) {
		if (!IS_NPC(mob) || GET_LOYALTY(mob) != emp || FIGHTING(mob)) {
			continue;
		}
		if (island_id != NO_ISLAND && GET_ISLAND_ID(IN_ROOM(mob)) != island_id) {
			continue;
		}
		
		// see if (it's a workforce mob
		found = FALSE;
		for (iter = 0; iter < NUM_CHORES && !found; ++iter) {
			if (chore_data[iter].mob != NOTHING && chore_data[iter].mob == GET_MOB_VNUM(mob)) {
				found = TRUE;
			}
		}
		
		if (found) {// despawn it
			act("$n finishes working and leaves.", TRUE, mob, NULL, NULL, TO_ROOM);
			extract_char(mob);
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
	
	DL_FOREACH_SAFE2(ROOM_PEOPLE(room), mob, next_mob, next_in_room) {
		if (IS_NPC(mob) && GET_MOB_VNUM(mob) != NOTHING) {
			// check it's a workforce mob
			match = FALSE;
			for (iter = 0; iter < NUM_CHORES && !match; ++iter) {
				if (chore_data[iter].mob != NOTHING && GET_MOB_VNUM(mob) == chore_data[iter].mob) {
					match = TRUE;
				}
			}
		
			if (match) {
				if (!FIGHTING(mob)) {
					act("$n leaves to do other work.", TRUE, mob, NULL, NULL, TO_ROOM);
					extract_char(mob);
				}
				else {
					// mark for despawn
					set_mob_flags(mob, MOB_SPAWNED);
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
* any disabled copies of the worker, it skips them, which will likely trigger
* a new worker to be placed.
*
* It will also find an artisan in the room and use that, if there is one. It
* skips mobs whose spawn time is 'now', who are probably doing another chore
* or just spawned here.
*
* @param empire_data *emp Whose worker (not necssarily the same as the room owner).
* @param room_data *room The location to check.
* @param vehicle_data *veh Optional: If it's a vehicle chore, this will look for a vehicle artisan. (NULL for not-a-vehicle-chore.)
* @param mob_vnum vnum The worker vnum to look for.
* @return char_data* The found mob, or NULL;
*/
char_data *find_chore_worker_in_room(empire_data *emp, room_data *room, vehicle_data *veh, mob_vnum vnum) {
	any_vnum artisan_vnum = NOTHING;
	char_data *mob;
	
	// does not work if no vnum provided
	if (vnum == NOTHING) {
		return NULL;
	}
	
	// can also use an artisan
	if (veh) {
		artisan_vnum = VEH_ARTISAN(veh);
	}
	else if (GET_BUILDING(room)) {
		artisan_vnum = GET_BLD_ARTISAN(GET_BUILDING(room));
	}
	
	DL_FOREACH2(ROOM_PEOPLE(room), mob, next_in_room) {
		if (!IS_NPC(mob) || GET_LOYALTY(mob) != emp) {
			continue;	// player or wrong empire
		}
		if (GET_MOB_VNUM(mob) != vnum && (artisan_vnum == NOTHING || GET_MOB_VNUM(mob) != artisan_vnum)) {
			continue;	// wrong mob
		}
		if (MOB_SPAWN_TIME(mob) == time(0)) {
			continue;	// probably just spawned or doing another chore
		}
		if (IS_DEAD(mob) || EXTRACTED(mob) || GET_POS(mob) < MIN_WORKER_POS || FIGHTING(mob) || GET_FED_ON_BY(mob) || GET_FEEDING_FROM(mob)) {
			continue;	// mob is in some way incapacitated
		}
		
		// ok:
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
	struct empire_territory_data *ter_iter, *next_ter;
	struct empire_npc_data *backup = NULL, *npc_iter;
	room_data *rm;

	int chore_distance = config_get_int("chore_distance");
	
	if (!emp || !loc) {
		return NULL;
	}
	
	// massive iteration to try to find one
	HASH_ITER(hh, EMPIRE_TERRITORY_LIST(emp), ter_iter, next_ter) {
		// only bother checking anything if npcs live here
		if (ter_iter->npcs) {			
			if ((rm = ter_iter->room) && GET_ISLAND_ID(loc) == GET_ISLAND_ID(rm) && compute_distance(loc, rm) <= chore_distance) {
				// iterate over population
				for (npc_iter = ter_iter->npcs; npc_iter; npc_iter = npc_iter->next) {
					// only use citizens for labor
					if (npc_iter->vnum == CITIZEN_MALE || npc_iter->vnum == CITIZEN_FEMALE) {
						if (!backup && npc_iter->mob && GET_MOB_VNUM(npc_iter->mob) == npc_iter->vnum && !FIGHTING(npc_iter->mob)) {
							// already has a mob? save as backup (also making sure the npc's mob is just a citizen not a laborer)
							backup = npc_iter;
						}
						else if (!npc_iter->mob) {
							return npc_iter;
						}
					}
				}
			}
		}
	}
	
	if (backup) {
		if (backup->mob) {
			act("$n leaves to go to work.", TRUE, backup->mob, NULL, NULL, TO_ROOM);
			extract_char(backup->mob);
			extract_pending_chars();	// ensure char is gone
			backup->mob = NULL;
		}
		
		return backup;
	}
	else {
		return NULL;
	}
}


/**
* Fetches an int array of size NUM_CHORES with the chore ids in alphabetical
* order. Use it like this:
*   int *order = get_ordered_chores();
*   for (iter = 0; iter < NUM_CHORES; ++iter) {
*     strcpy(name, chore_data[order[iter]].name);
*     ...
*   }
*/
int *get_ordered_chores(void) {
	static int *data = NULL;
	int iter, sub, temp;
	
	if (data) {
		return data;	// only have to do this once per boot
	}
	// otherwise, determine it:
	
	// create and initialize:
	CREATE(data, int, NUM_CHORES);
	for (iter = 0; iter < NUM_CHORES; ++iter) {
		data[iter] = iter;
	}
	
	// sort it alphabetically
	for (iter = 0; iter < NUM_CHORES; ++iter) {
		for (sub = iter + 1; sub < NUM_CHORES; ++sub) {
			if (strcmp(chore_data[data[iter]].name, chore_data[data[sub]].name) > 0) {
				temp = data[iter];
				data[iter] = data[sub];
				data[sub] = temp;
			}
		}
	}
	
	return data;
}


/**
* Return the current limit for an item's production. -1 means no-limit.
*
* @param empire_data *emp The empire.
* @param obj_vnum vnum The object to get the limit for.
* @return int UNLIMITED for no limit, otherwise the maximum number the empire can have before it stops producing more.
*/
int get_workforce_production_limit(empire_data *emp, obj_vnum vnum) {
	struct workforce_production_limit *wpl;
	
	HASH_FIND_INT(EMPIRE_PRODUCTION_LIMITS(emp), &vnum, wpl);
	if (wpl) {
		return wpl->limit;
	}
	
	return UNLIMITED;	// no entry
}


/**
* Checks the depletion types for all matching interactions to see if there's at
* least 1 available for a chore. Also checks if the empire can gain it.
*
* @param empire_data *emp The empire for the chore.
* @param int chore Which CHORE_ is being done.
* @param room_data *room Optional: The room to check for interactions. May be NULL if using a vehicle; empire must own room if provided.
* @param vehicle_data *veh Optional: The vehicle to check for interactions. May be NULL if using a room.
* @param int interaction_type Any INTERACT_ type.
* @param bool *over_limit Optional: Detects if there was an undepleted item the empire can't gain.
* @return bool TRUE if at least 1 interaction is available and undepleted.
*/
bool has_any_undepleted_interaction_for_chore(empire_data *emp, int chore, room_data *room, vehicle_data *veh, int interaction_type, bool *over_limit) {
	struct interaction_item *interact, *list[4] = { NULL, NULL, NULL, NULL };
	int iter, list_size, common_depletion, depletion_type;
	crop_data *cp;
	obj_data *proto;
	
	if (over_limit) {
		*over_limit = FALSE;
	}
	
	if (!room && !veh) {
		return FALSE;	// requires 1 or the other
	}
	
	// prevent more lookups in the loop
	common_depletion = room ? DEPLETION_LIMIT(room) : config_get_int("common_depletion");
	
	// build lists of interactions to check
	list_size = 0;
	if (room && ROOM_OWNER(room) == emp) {
		list[list_size++] = GET_SECT_INTERACTIONS(SECT(room));
		if (ROOM_SECT_FLAGGED(room, SECTF_CROP) && (cp = ROOM_CROP(room))) {
			list[list_size++] = GET_CROP_INTERACTIONS(cp);
		}
		if (GET_BUILDING(room) && IS_COMPLETE(room)) {
			list[list_size++] = GET_BLD_INTERACTIONS(GET_BUILDING(room));
		}
	}
	if (veh) {
		list[list_size++] = VEH_INTERACTIONS(veh);
	}
	
	// check all lists
	for (iter = 0; iter < list_size; ++iter) {
		LL_FOREACH(list[iter], interact) {
			if (interact->type != interaction_type) {
				continue;	// wrong type
			}
			if (!(proto = obj_proto(interact->vnum))) {
				continue;	// no proto
			}
			if (!GET_OBJ_STORAGE(proto)) {
				continue;	// MUST be storable
			}
			if (!meets_interaction_restrictions(interact->restrictions, NULL, emp, NULL, NULL)) {
				continue;	// restrictions
			}
			depletion_type = determine_depletion_type(interact);
			if (GET_CHORE_DEPLETION(room, veh, depletion_type != NOTHING ? depletion_type : DPLTN_PRODUCTION) >= (interact_data[interaction_type].one_at_a_time ? interact->quantity : common_depletion)) {
				continue;	// depleted
			}
			
			// appears ok
			if (can_gain_chore_resource(emp, room ? room : IN_ROOM(veh), chore, interact->vnum)) {
				return TRUE;	// only need 1
			}
			else if (over_limit) {
				*over_limit = TRUE;
			}
		}
	}
	
	// did not find one
	return FALSE;
}


/**
* Mark the reason workforce couldn't work a given spot this time (logs are
* freed every time workforce runs).
*
* @param empire_data *emp The empire trying to work.
* @param room_data *room The location.
* @param int chore Any CHORE_ const.
* @param int problem Any WF_PROB_ const.
* @param bool is_delay If TRUE, marks it as a repeat-problem due to workforce delays.
*/
void log_workforce_problem(empire_data *emp, room_data *room, int chore, int problem, bool is_delay) {
	struct workforce_log *wf_log;
	bool found = FALSE;
	
	if (!emp || !room) {
		return;	// safety first
	}
	
	LL_FOREACH(EMPIRE_WORKFORCE_LOG(emp), wf_log) {
		if (wf_log->loc == GET_ROOM_VNUM(room) && wf_log->chore == chore && wf_log->problem == problem) {
			found = TRUE;
			++wf_log->count;
			wf_log->delayed |= is_delay;
		}
	}
	
	if (!found) {
		CREATE(wf_log, struct workforce_log, 1);
		wf_log->loc = GET_ROOM_VNUM(room);
		wf_log->chore = chore;
		wf_log->problem = problem;
		wf_log->delayed = is_delay;
		wf_log->count = 1;
		
		LL_PREPEND(EMPIRE_WORKFORCE_LOG(emp), wf_log);
	}
}


/**
* Adds a workforce delay to a given room for an empire.
*
* @param empire_data *emp The empire.
* @param room_data *room The location to mark it for.
* @param int chore Which chore to mark.
* @param int problem Which WF_PROB_ const might be the issue (pass NOTHING if n/a).
*/
void mark_workforce_delay(empire_data *emp, room_data *room, int chore, int problem) {
	struct workforce_delay_chore *wdc, *entry;
	struct workforce_delay *delay;
	room_vnum vnum;
	
	// find or add the delay entry
	vnum = GET_ROOM_VNUM(room);
	HASH_FIND_INT(EMPIRE_DELAYS(emp), &vnum, delay);
	if (!delay) {
		CREATE(delay, struct workforce_delay, 1);
		delay->location = vnum;
		HASH_ADD_INT(EMPIRE_DELAYS(emp), location, delay);
	}
	
	// check if chore exists
	entry = NULL;
	LL_FOREACH(delay->chores, wdc) {
		if (wdc->chore == chore) {
			entry = wdc;
			break;
		}
	}
	if (!entry) {
		CREATE(entry, struct workforce_delay_chore, 1);
		entry->chore = chore;
		LL_PREPEND(delay->chores, entry);
	}
	
	// set time on entry (random)
	entry->time = number(2, 3);
	entry->problem = problem;
}


/**
* @param empire_data *emp Empire the worker belongs to
* @param int chore Which CHORE_
* @param room_data *room Where to look
* @return char_data *the mob, or NULL if none to spawn
*/
char_data *place_chore_worker(empire_data *emp, int chore, room_data *room) {
	struct empire_territory_data *ter;
	struct empire_npc_data *npc;
	char_data *mob = NULL;
	any_vnum artisan_vnum;
	
	// nobody to spawn
	if (chore_data[chore].mob == NOTHING) {
		return NULL;
	}
	
	// check if there's an artisan first
	if (GET_BUILDING(room) && (artisan_vnum = GET_BLD_ARTISAN(GET_BUILDING(room))) != NOTHING && (ter = find_territory_entry(emp, room))) {
		LL_FOREACH(ter->npcs, npc) {
			// if there's already an npc->mob, the artisan is presumably busy
			if (npc->vnum == artisan_vnum && !npc->mob) {
				mob = spawn_empire_npc_to_room(emp, npc, room, NOTHING);
				
				// guarantee SPAWNED flag -- the spawn timer will be reset each time the mob works (charge_workforce), until it's done
				set_mob_flags(mob, MOB_SPAWNED);
				break;
			}
		}
	}
	
	// if there was no artisan
	if (!mob && (npc = find_free_npc_for_chore(emp, room))) {
		mob = spawn_empire_npc_to_room(emp, npc, room, chore_data[chore].mob);
		
		// guarantee SPAWNED flag -- the spawn timer will be reset each time the mob works (charge_workforce), until it's done
		set_mob_flags(mob, MOB_SPAWNED);
	}
	else if (!mob) {
		log_workforce_problem(emp, room, chore, WF_PROB_NO_WORKERS, FALSE);
	}
	
	return mob;
}


/**
* Reports (and deletes) the workforce production logs for an empire. These go
* to the 'elog workforce' command ONLY.
*
* @param empire_data *emp The empire.
*/
void report_workforce_production_log(empire_data *emp) {
	struct workforce_production_log *wplog, *next;
	char amount_str[256];
	
	if (EMPIRE_PRODUCTION_LOGS(emp)) {
		// mark for save now as we will be deleting logs
		EMPIRE_NEEDS_STORAGE_SAVE(emp) = TRUE;
	}
	
	LL_FOREACH_SAFE(EMPIRE_PRODUCTION_LOGS(emp), wplog, next) {
		if (wplog->amount > 1) {
			snprintf(amount_str, sizeof(amount_str), " (x%d)", wplog->amount);
		}
		else {
			*amount_str = '\0';
		}
		
		// WPLOG_x
		switch (wplog->type) {
			case WPLOG_COINS: {
				log_to_empire(emp, ELOG_WORKFORCE, "Minted coins%s", amount_str);
				break;
			}
			case WPLOG_OBJECT: {
				log_to_empire(emp, ELOG_WORKFORCE, "Produced %s%s", get_obj_name_by_proto(wplog->vnum), amount_str);
				break;
			}
			case WPLOG_BUILDING_DONE: {
				log_to_empire(emp, ELOG_WORKFORCE, "Built %s%s", get_bld_name_by_proto(wplog->vnum), amount_str);
				break;
			}
			case WPLOG_BUILDING_DISMANTLED: {
				log_to_empire(emp, ELOG_WORKFORCE, "Dismantled %s%s", get_bld_name_by_proto(wplog->vnum), amount_str);
				break;
			}
			case WPLOG_VEHICLE_DONE: {
				log_to_empire(emp, ELOG_WORKFORCE, "Built %s%s", get_vehicle_name_by_proto(wplog->vnum), amount_str);
				break;
			}
			case WPLOG_VEHICLE_DISMANTLED: {
				log_to_empire(emp, ELOG_WORKFORCE, "Dismantled %s%s", get_vehicle_name_by_proto(wplog->vnum), amount_str);
				break;
			}
			case WPLOG_STUMPS_BURNED: {
				log_to_empire(emp, ELOG_WORKFORCE, "Burned stumps%s", amount_str);
				break;
			}
			case WPLOG_FIRE_EXTINGUISHED: {
				log_to_empire(emp, ELOG_WORKFORCE, "Extinguished fires%s", amount_str);
				break;
			}
			case WPLOG_PROSPECTED: {
				log_to_empire(emp, ELOG_WORKFORCE, "Prospected%s", amount_str);
				break;
			}
			case WPLOG_MAINTENANCE: {
				log_to_empire(emp, ELOG_WORKFORCE, "Maintenance completed%s", amount_str);
				break;
			}
		}
		
		LL_DELETE(EMPIRE_PRODUCTION_LOGS(emp), wplog);
		free(wplog);
	}
}


/**
* Sets the workforce production limit for an item.
*
* @param empire_data *emp The empire.
* @param obj_vnum vnum The object to set the limit for.
* @param int amount The amount to set it to, or UNLIMITED for unlimited (deletes the entry).
*/
void set_workforce_production_limit(empire_data *emp, any_vnum vnum, int amount) {
	struct workforce_production_limit *wpl;
	
	HASH_FIND_INT(EMPIRE_PRODUCTION_LIMITS(emp), &vnum, wpl);

	// add if missing
	if (!wpl && amount != UNLIMITED) {
		CREATE(wpl, struct workforce_production_limit, 1);
		wpl->vnum = vnum;
		HASH_ADD_INT(EMPIRE_PRODUCTION_LIMITS(emp), vnum, wpl);
	}
	
	// update if needed
	if (wpl) {
		wpl->limit = amount;
	}
	
	// delete if needed
	if (wpl && amount == UNLIMITED) {
		HASH_DEL(EMPIRE_PRODUCTION_LIMITS(emp), wpl);
		free(wpl);
	}
	
	EMPIRE_NEEDS_STORAGE_SAVE(emp) = TRUE;
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


/**
* Ensures einv is sorted. Call before einv-related tasks.
*
* @param empire_data *emp The empire to sort.
*/
void sort_einv_for_empire(empire_data *emp) {
	struct empire_island *eisle, *next_eisle;
	
	if (emp) {
		HASH_ITER(hh, EMPIRE_ISLANDS(emp), eisle, next_eisle) {
			// sort einv now to ensure it's in a useful order (most quantity first)
			if (!eisle->store_is_sorted) {
				HASH_SORT(eisle->store, sort_einv);
				eisle->store_is_sorted = TRUE;
			}
		}
	}
}


/**
* Checks if a chore should be skipped, and decrements the delay timer if so.
*
* @param empire_data *emp The empire.
* @param room_data *room The location to check.
* @param int chore Which chore to check if delayed.
* @return bool TRUE if the chore is delayed, FALSE if not.
*/
bool workforce_is_delayed(empire_data *emp, room_data *room, int chore) {
	struct workforce_delay_chore *wdc, *next_wdc;
	struct workforce_delay *delay;
	int problem = WF_PROB_DELAYED;
	bool delayed = FALSE;
	room_vnum vnum;
	
	// find the delay entry
	vnum = GET_ROOM_VNUM(room);
	HASH_FIND_INT(EMPIRE_DELAYS(emp), &vnum, delay);
	if (!delay) {
		return FALSE;
	}
	
	// check if chore exists
	LL_FOREACH_SAFE(delay->chores, wdc, next_wdc) {
		if (wdc->chore == chore) {
			if (wdc->problem != NOTHING) {
				problem = wdc->problem;	// inherit if possible
			}
			
			if (--wdc->time <= 0) {
				LL_DELETE(delay->chores, wdc);
				free(wdc);
			}
			
			delayed = TRUE;
			break;	// any 1 is fine
		}
	}
	
	if (delayed) {
		log_workforce_problem(emp, room, chore, problem, TRUE);
	}
	
	return delayed;
}


 /////////////////////////////////////////////////////////////////////////////
//// GENERIC CRAFT WORKFORCE ////////////////////////////////////////////////

/**
* Function passed to do_chore_gen_craft()
*
* @param empire_data *emp The empire doing the chore.
* @param room_data *room The room the chore is in.
* @param vehicle_data *veh Optional: If it's a vehicle chore, not a room chore. (NULL for not-a-vehicle-chore.)
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
		if (!ABIL_IS_PURCHASE(abil)) {
			return FALSE;	// only works on purchasable (normal) abilities
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
* @param vehicle_data *veh Optional: If it's a vehicle chore, not a room chore. (NULL for not-a-vehicle-chore.)
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
		if (!ABIL_IS_PURCHASE(abil)) {
			return FALSE;	// only purchasable (normal) abilities
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
* @param vehicle_data *veh Optional: If it's a vehicle chore, not a room chore. (NULL for not-a-vehicle-chore.)
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
		if (!ABIL_IS_PURCHASE(abil)) {
			return FALSE;	// only purchasable (normal) abilities
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
* @param vehicle_data *veh Optional: If it's a vehicle chore, not a room chore. (NULL for not-a-vehicle-chore.)
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
		if (!ABIL_IS_PURCHASE(abil)) {
			return FALSE;	// only purchasable (normal) abilities
		}
		else if (ABIL_SKILL_LEVEL(abil) > BASIC_SKILL_CAP) {
			return FALSE;	// level too high
		}
	}
	// success
	return TRUE;
}


/**
* Function passed to do_chore_gen_craft() for generic 'crafting' workforce.
* By this point we have validated that the location is in-city if necessary,
* and that there should be at least 1 viable craft.
*
* See workforce_crafting_chores()
*
* @param empire_data *emp The empire doing the chore.
* @param room_data *room The room the chore is in.
* @param vehicle_data *veh Optional: If it's a vehicle chore, not a room chore. (NULL for not-a-vehicle-chore.)
* @param int chore CHORE_ const for this chore.
* @param craft_data *craft The craft to validate.
* @return bool TRUE if this workforce chore can work this craft, FALSE if not
*/
CHORE_GEN_CRAFT_VALIDATOR(chore_workforce_crafting) {
	if (GET_CRAFT_TYPE(craft) != CRAFT_TYPE_WORKFORCE || !GET_CRAFT_REQUIRES_FUNCTION(craft)) {
		return FALSE;
	}
	if (veh && !IS_SET(VEH_FUNCTIONS(veh), GET_CRAFT_REQUIRES_FUNCTION(craft))) {
		return FALSE;	// bad vehicle function (no need to test in-city again)
	}
	if (!veh && !HAS_FUNCTION(room, GET_CRAFT_REQUIRES_FUNCTION(craft))) {
		return FALSE;	// bad room function (no need to test in-city again)
	}
	
	// in all other cases, attempt the chore
	
	// success
	return TRUE;
}


/**
* Can manage any craft in the craft_table, validated by the 'validator' arg.
*
* @param empire_data *emp The empire doing the chore.
* @param room_data *room The room the chore is in.
* @param vehicle_data *veh Optional: If it's a vehicle chore, not a room chore. (NULL for not-a-vehicle-chore.)
* @param int chore CHORE_ const for this chore.
* @param CHORE_GEN_CRAFT_VALIDATOR *validator A function that validates a craft.
* @param bool is_skilled If TRUE, gains exp for Skilled Labor.
*/
void do_chore_gen_craft(empire_data *emp, room_data *room, vehicle_data *veh, int chore, CHORE_GEN_CRAFT_VALIDATOR(*validator), bool is_skilled) {
	struct empire_storage_data *store = NULL;
	char_data *worker;
	bool any_over_limit = FALSE, any_no_res = FALSE, done_any = FALSE;
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
		if (CRAFT_FLAGGED(craft, CRAFT_IN_DEVELOPMENT | CRAFT_SOUP) || CRAFT_IS_VEHICLE(craft) || CRAFT_IS_BUILDING(craft)) {
			continue;
		}
		if (CRAFT_FLAGGED(craft, CRAFT_SKILLED_LABOR) && !EMPIRE_HAS_TECH(emp, TECH_SKILLED_LABOR)) {
			continue;	// need skillz
		}
		if (GET_CRAFT_REQUIRES_OBJ(craft) != NOTHING && CRAFT_FLAGGED(craft, CRAFT_TAKE_REQUIRED_OBJ)) {
			continue;	// don't allow crafts with TAKE-REQUIRED-OBJ
		}
		if (GET_CRAFT_REQUIRES_OBJ(craft) != NOTHING && (!(store = find_stored_resource(emp, islid, GET_CRAFT_REQUIRES_OBJ(craft))) || store->amount < 1)) {
			continue;	// missing required-obj
		}
		// pass through validator function
		if (validator && !(validator)(emp, room, veh, chore, craft)) {
			continue;
		}
		
		// can we gain any of it?
		if (!can_gain_chore_resource(emp, room, chore, GET_CRAFT_OBJECT(craft))) {
			any_over_limit = TRUE;
			continue;
		}
		
		// check resources...
		has_res = TRUE;
		for (res = GET_CRAFT_RESOURCES(craft); res && has_res; res = res->next) {
			if (res->type != RES_OBJECT && res->type != RES_COMPONENT) {
				// can ONLY do crafts that use objects/components
				has_res = FALSE;
			}
			else if (res->type == RES_OBJECT && (!(store = find_stored_resource(emp, islid, res->vnum)) || store->amount < 1 || store->keep == UNLIMITED || store->amount <= store->keep || (store->amount - store->keep) < res->amount)) {
				has_res = FALSE;
			}
			else if (res->type == RES_COMPONENT && !empire_can_afford_component(emp, islid, res->vnum, res->amount, FALSE, TRUE)) {
				// this actually requires all of the component be the same type -- it won't mix component types
				has_res = FALSE;
			}
		}
		if (!has_res) {
			any_no_res = TRUE;
			continue;
		}
		
		// found one! (pick at radom if more than one)
		if (!number(0, crafts_found++) || !do_craft) {
			do_craft = craft;
		}
	}
	
	// now attempt to do the chore
	if (do_craft) {
		if ((worker = find_chore_worker_in_room(emp, room, veh, chore_data[chore].mob))) {
			charge_workforce(emp, chore, room, worker, 1, GET_CRAFT_OBJECT(do_craft), GET_CRAFT_QUANTITY(do_craft));
		
			// charge resources (we pre-validated res->type and availability)
			for (res = GET_CRAFT_RESOURCES(do_craft); res; res = res->next) {
				if (res->type == RES_OBJECT) {
					charge_stored_resource(emp, islid, res->vnum, res->amount);
				}
				else if (res->type == RES_COMPONENT) {
					charge_stored_component(emp, islid, res->vnum, res->amount, FALSE, TRUE, NULL);
				}
			}
		
			add_to_empire_storage(emp, islid, GET_CRAFT_OBJECT(do_craft), GET_CRAFT_QUANTITY(do_craft));
			add_production_total(emp, GET_CRAFT_OBJECT(do_craft), GET_CRAFT_QUANTITY(do_craft));
			add_workforce_production_log(emp, WPLOG_OBJECT, GET_CRAFT_OBJECT(do_craft), GET_CRAFT_QUANTITY(do_craft));
		
			// only send message if someone else is present (don't bother verifying it's a player)
			if (ROOM_PEOPLE(IN_ROOM(worker))->next_in_room) {
				snprintf(buf, sizeof(buf), "$n finishes %s %s.", gen_craft_data[GET_CRAFT_TYPE(do_craft)].verb, get_obj_name_by_proto(GET_CRAFT_OBJECT(do_craft)));
				act(buf, FALSE, worker, NULL, NULL, TO_ROOM);
			}
		
			done_any = TRUE;
		}
		else if ((worker = place_chore_worker(emp, chore, room))) {
			// fresh worker
			charge_workforce(emp, chore, room, worker, 1, GET_CRAFT_OBJECT(do_craft), GET_CRAFT_QUANTITY(do_craft));
		}
	}
	else {	// mark delays
		if (any_no_res) {
			mark_workforce_delay(emp, room, chore, WF_PROB_NO_RESOURCES);
		}
		if (any_over_limit) {
			mark_workforce_delay(emp, room, chore, WF_PROB_OVER_LIMIT);
		}
	}
	
	if (!done_any) {
		if (any_over_limit) {
			log_workforce_problem(emp, room, chore, WF_PROB_OVER_LIMIT, FALSE);
		}
		if (any_no_res) {
			log_workforce_problem(emp, room, chore, WF_PROB_NO_RESOURCES, FALSE);
		}
	}
}


/**
* For WORKFORCE-type crafts
*
* @param empire_data *emp The empire for the chore.
* @param room_data *room The location of the chore.
* @param vehicle_data *veh Optional: If the chore is performed by a vehicle, this is set.
*/
void workforce_crafting_chores(empire_data *emp, room_data *room, vehicle_data *veh) {
	int island = GET_ISLAND_ID(room);
	craft_data *craft, *next;
	bool any = FALSE;
	
	if (!CHORE_ACTIVE(CHORE_CRAFTING)) {
		return;	// no work
	}
	
	HASH_ITER(hh, craft_table, craft, next) {
		if (CRAFT_FLAGGED(craft, CRAFT_IN_DEVELOPMENT)) {
			continue;	// in-dev
		}
		if (GET_CRAFT_TYPE(craft) != CRAFT_TYPE_WORKFORCE || !GET_CRAFT_REQUIRES_FUNCTION(craft)) {
			continue;	// not workforce or no location configured
		}
		if (CRAFT_FLAGGED(craft, CRAFT_SKILLED_LABOR) && !EMPIRE_HAS_TECH(emp, TECH_SKILLED_LABOR)) {
			continue;	// need skillz
		}
		if (!veh && !room_has_function_and_city_ok(emp, room, GET_CRAFT_REQUIRES_FUNCTION(craft))) {
			continue;	// room-based chore missing function
		}
		if (veh && !vehicle_has_function_and_city_ok(veh, GET_CRAFT_REQUIRES_FUNCTION(craft))) {
			continue;	// vehicle-based chore missing function
		}
		
		// type/location ok: this seems like a valid chore to at least attempt
		any = TRUE;
		break;
	}
	
	// if we found any chore that can be done here, run it through gen-craft
	if (any) {
		do_chore_gen_craft(emp, room, veh, CHORE_CRAFTING, chore_workforce_crafting, FALSE);
	}
}


 /////////////////////////////////////////////////////////////////////////////
//// CHORE FUNCTIONS ////////////////////////////////////////////////////////

/**
* This is used by non-vehicles for both building and maintenance.
*
* @param empire_data *emp Which empire is doing the chore.
* @param room_data *room Which room to build/maintain
* @param int mode Which CHORE_ is being performed (build/maintain).
*/
void do_chore_building(empire_data *emp, room_data *room, int mode) {
	char_data *worker;
	struct empire_storage_data *store = NULL;
	bool can_do = FALSE;
	struct resource_data *res = NULL;
	int islid = GET_ISLAND_ID(room);
	
	if (!BUILDING_RESOURCES(room)) {
		// is already done; just have to finish
		can_do = TRUE;
	}
	else if ((res = BUILDING_RESOURCES(room))) {
		// RES_x: can only process some types in this way
		if (res->type == RES_OBJECT && (store = find_stored_resource(emp, islid, res->vnum)) && store->amount > 0 && store->keep != UNLIMITED && store->amount > store->keep && store->amount > 0) {
			can_do = TRUE;
		}
		else if (res->type == RES_COMPONENT && empire_can_afford_component(emp, islid, res->vnum, 1, FALSE, TRUE)) {
			can_do = TRUE;
		}
		else if (res->type == RES_ACTION || res->type == RES_TOOL) {
			// workforce can always do actions/tools
			// TODO: some day, empires could have a list of tools available and this could require them
			can_do = TRUE;
		}
	}
	
	if (can_do) {
		if ((worker = find_chore_worker_in_room(emp, room, NULL, chore_data[mode].mob))) {
			charge_workforce(emp, mode, room, worker, 1, NOTHING, 0);
		
			if (res) {
				if (res->type == RES_OBJECT) {
					if (mode == CHORE_MAINTENANCE) {
						// remove an older matching object
						remove_like_item_from_built_with(&GET_BUILT_WITH(room), obj_proto(res->vnum));
					}
				
					add_to_resource_list(&GET_BUILT_WITH(room), RES_OBJECT, res->vnum, 1, 1);
					charge_stored_resource(emp, islid, res->vnum, 1);
				}
				else if (res->type == RES_COMPONENT) {
					if (mode == CHORE_MAINTENANCE) {
						// remove an older matching component
						remove_like_component_from_built_with(&GET_BUILT_WITH(room), res->vnum);
					}
					charge_stored_component(emp, islid, res->vnum, 1, FALSE, TRUE, &GET_BUILT_WITH(room));
				}
			
				res->amount -= 1;
			
				// remove res?
				if (res->amount <= 0) {
					LL_DELETE(GET_BUILDING_RESOURCES(room), res);
					free(res);
				}
			
				request_world_save(GET_ROOM_VNUM(room), WSAVE_ROOM);
			}
		
			// check for completion
			if (!BUILDING_RESOURCES(room)) {
				if (mode == CHORE_BUILDING) {
					add_workforce_production_log(emp, WPLOG_BUILDING_DONE, GET_BUILDING(room) ? GET_BLD_VNUM(GET_BUILDING(room)) : NOTHING, 1);
					finish_building(worker, room);
					stop_room_action(room, ACT_BUILDING);
				}
				else if (mode == CHORE_MAINTENANCE) {
					add_workforce_production_log(emp, WPLOG_MAINTENANCE, 0, 1);
					finish_maintenance(worker, room);
					stop_room_action(room, ACT_MAINTENANCE);
				}
			}
			else {	// not complete
				// only send message if someone else is present (don't bother verifying it's a player)
				if (ROOM_PEOPLE(IN_ROOM(worker))->next_in_room) {
					act("$n works on the building.", FALSE, worker, NULL, NULL, TO_ROOM | TO_SPAMMY | TO_QUEUE);
				}
			}
		}
		else if ((worker = place_chore_worker(emp, mode, room))) {
			// fresh worker
			charge_workforce(emp, mode, room, worker, 1, NOTHING, 0);
		}
	}
	else {
		mark_workforce_delay(emp, room, mode, WF_PROB_NO_RESOURCES);
		log_workforce_problem(emp, room, mode, WF_PROB_NO_RESOURCES, FALSE);
	}
}


void do_chore_burn_stumps(empire_data *emp, room_data *room) {
	char_data *worker = find_chore_worker_in_room(emp, room, NULL, chore_data[CHORE_BURN_STUMPS].mob);
	
	if (!worker) {	// as a backup, use a chopper if present
		worker = find_chore_worker_in_room(emp, room, NULL, chore_data[CHORE_CHOPPING].mob);
	}
	
	if (worker) {	// always just 1 tick
		if (has_evolution_type(SECT(room), EVO_BURN_STUMPS)) {
			charge_workforce(emp, CHORE_BURN_STUMPS, room, worker, 1, NOTHING, 0);
			act("$n lights some fires!", FALSE, worker, NULL, NULL, TO_ROOM);
			perform_burn_room(room, EVO_BURN_STUMPS);
			add_workforce_production_log(emp, WPLOG_STUMPS_BURNED, 0, 1);
		}
		
		// done
		stop_room_action(room, ACT_BURN_AREA);
		
		if (!ROOM_AFF_FLAGGED(room, ROOM_AFF_NO_ABANDON) && empire_chore_limit(emp, GET_ISLAND_ID(room), CHORE_ABANDON_CHOPPED)) {
			abandon_room(room);
		}
	}
	else if ((worker = place_chore_worker(emp, CHORE_BURN_STUMPS, room))) {
		charge_workforce(emp, CHORE_BURN_STUMPS, room, worker, 1, NOTHING, 0);
	}
}


// INTERACTION_FUNC provides: ch, interaction, inter_room, inter_mob, inter_item, inter_veh
INTERACTION_FUNC(one_chop_chore) {
	empire_data *emp = inter_veh ? VEH_OWNER(inter_veh) : ROOM_OWNER(inter_room);
	char buf[MAX_STRING_LENGTH];
	int amt;
	
	if (emp && can_gain_chore_resource(emp, inter_room, CHORE_CHOPPING, interaction->vnum)) {
		amt = interact_data[interaction->type].one_at_a_time ? 1 : interaction->quantity;
		ewt_mark_resource_worker(emp, inter_room, interaction->vnum, amt);
		add_to_empire_storage(emp, GET_ISLAND_ID(inter_room), interaction->vnum, amt);
		add_production_total(emp, interaction->vnum, amt);
		add_workforce_production_log(emp, WPLOG_OBJECT, interaction->vnum, amt);
		
		// only send message if someone else is present (don't bother verifying it's a player)
		if (ROOM_PEOPLE(IN_ROOM(ch))->next_in_room) {
			if (amt > 1) {
				sprintf(buf, "$n chops %s (x%d).", get_obj_name_by_proto(interaction->vnum), amt);
			}
			else {
				sprintf(buf, "$n chops %s.", get_obj_name_by_proto(interaction->vnum));
			}
			act(buf, FALSE, ch, NULL, NULL, TO_ROOM | TO_SPAMMY | TO_QUEUE);
		}
		return TRUE;
	}
	
	return FALSE;
}


void do_chore_chopping(empire_data *emp, room_data *room) {
	char_data *worker;
	bool depleted = (get_depletion(room, DPLTN_CHOP) >= get_depletion_max(room, DPLTN_CHOP));
	bool can_gain = can_gain_chore_resource_from_interaction_room(emp, room, CHORE_CHOPPING, INTERACT_CHOP);
	bool can_do = !depleted && can_gain;
	
	if (can_do) {
		if ((worker = find_chore_worker_in_room(emp, room, NULL, chore_data[CHORE_CHOPPING].mob))) {
			charge_workforce(emp, CHORE_CHOPPING, room, worker, 1, NOTHING, 0);
		
			if (get_room_extra_data(room, ROOM_EXTRA_CHOP_PROGRESS) <= 0) {
				set_room_extra_data(room, ROOM_EXTRA_CHOP_PROGRESS, config_get_int("chop_timer"));
				ewt_mark_for_interactions(emp, room, INTERACT_CHOP);
			}
			else {
				add_to_room_extra_data(room, ROOM_EXTRA_CHOP_PROGRESS, -1);
				if (get_room_extra_data(room, ROOM_EXTRA_CHOP_PROGRESS) == 0) {
					// finished!
					run_room_interactions(worker, room, INTERACT_CHOP, NULL, NOTHING, one_chop_chore);
					change_chop_territory(room);
				
					if (CAN_CHOP_ROOM(room)) {
						set_room_extra_data(room, ROOM_EXTRA_CHOP_PROGRESS, config_get_int("chop_timer"));
					}
					else {
						// done
						stop_room_action(room, ACT_CHOPPING);
					
						if (!ROOM_AFF_FLAGGED(room, ROOM_AFF_NO_ABANDON) && empire_chore_limit(emp, GET_ISLAND_ID(room), CHORE_ABANDON_CHOPPED) && (!has_evolution_type(SECT(room), EVO_BURN_STUMPS) || !empire_chore_limit(emp, GET_ISLAND_ID(room), CHORE_BURN_STUMPS))) {
							abandon_room(room);
						}
					}
				}
				else {
					// only send message if someone else is present (don't bother verifying it's a player)
					if (ROOM_PEOPLE(IN_ROOM(worker))->next_in_room) {
						act("$n chops...", FALSE, worker, NULL, NULL, TO_ROOM | TO_SPAMMY | TO_QUEUE);
					}
					ewt_mark_for_interactions(emp, room, INTERACT_CHOP);
				}
			}
		}
		else if ((worker = place_chore_worker(emp, CHORE_CHOPPING, room))) {
			// fresh worker
			ewt_mark_for_interactions(emp, room, INTERACT_CHOP);
			charge_workforce(emp, CHORE_CHOPPING, room, worker, 1, NOTHING, 0);
		}
	}
	else {
		mark_workforce_delay(emp, room, CHORE_CHOPPING, depleted ? WF_PROB_DEPLETED : WF_PROB_OVER_LIMIT);
		log_workforce_problem(emp, room, CHORE_CHOPPING, depleted ? WF_PROB_DEPLETED : WF_PROB_OVER_LIMIT, FALSE);
	}
}


// for non-vehicles
void do_chore_dismantle(empire_data *emp, room_data *room) {
	struct resource_data *res, *next_res;
	bool can_do = FALSE;
	char_data *worker;
	obj_data *proto;
	
	// anything we can dismantle?
	if (!BUILDING_RESOURCES(room)) {
		can_do = TRUE;
	}
	else {
		LL_FOREACH(BUILDING_RESOURCES(room), res) {
			if (res->type == RES_OBJECT && (proto = obj_proto(res->vnum)) && GET_OBJ_STORAGE(proto)) {
				can_do = TRUE;
				break;
			}
		}
	}
	
	if (can_do) {
		if ((worker = find_chore_worker_in_room(emp, room, NULL, chore_data[CHORE_BUILDING].mob))) {
			charge_workforce(emp, CHORE_BUILDING, room, worker, 1, NOTHING, 0);
		
			LL_FOREACH_SAFE(BUILDING_RESOURCES(room), res, next_res) {
				// can only remove storable obj types
				if (res->type != RES_OBJECT || !(proto = obj_proto(res->vnum)) || !GET_OBJ_STORAGE(proto)) {
					continue;
				}
			
				if (res->amount > 0) {
					res->amount -= 1;
					add_to_empire_storage(emp, GET_ISLAND_ID(room), res->vnum, 1);
				}
			
				// remove res?
				if (res->amount <= 0) {
					LL_DELETE(GET_BUILDING_RESOURCES(room), res);
					free(res);
				}
			}
		
			request_world_save(GET_ROOM_VNUM(room), WSAVE_ROOM);
		
			// check for completion
			if (!BUILDING_RESOURCES(room)) {
				add_workforce_production_log(emp, WPLOG_BUILDING_DISMANTLED, GET_BUILDING(room) ? GET_BLD_VNUM(GET_BUILDING(room)) : NOTHING, 1);
				finish_dismantle(worker, room);
				if (!ROOM_AFF_FLAGGED(room, ROOM_AFF_NO_ABANDON) && empire_chore_limit(emp, GET_ISLAND_ID(room), CHORE_ABANDON_DISMANTLED)) {
					abandon_room(room);
				}
				stop_room_action(room, ACT_DISMANTLING);
			}
			else {
				// only send message if someone else is present (don't bother verifying it's a player)
				if (ROOM_PEOPLE(IN_ROOM(worker))->next_in_room) {
					act("$n works on dismantling the building.", FALSE, worker, NULL, NULL, TO_ROOM | TO_SPAMMY | TO_QUEUE);
				}
			}
		}
		else if ((worker = place_chore_worker(emp, CHORE_BUILDING, room))) {
			// fresh worker
			charge_workforce(emp, CHORE_BUILDING, room, worker, 1, NOTHING, 0);
		}
	}
	else {
		mark_workforce_delay(emp, room, CHORE_BUILDING, NOTHING);
	}
}


/**
* @param empire_data *emp The empire for the chore.
* @param room_data *room The location of the chore.
* @param vehicle_data *veh Optional: If the chore is performed by a vehicle, this is set.
*/
void do_chore_dismantle_mines(empire_data *emp, room_data *room, vehicle_data *veh) {
	char_data *worker;
	bool can_do = veh ? VEH_IS_COMPLETE(veh) : IS_COMPLETE(room);
	
	if (can_do) {
		if ((worker = find_chore_worker_in_room(emp, room, veh, chore_data[CHORE_DISMANTLE_MINES].mob)) || (worker = find_chore_worker_in_room(emp, room, veh, chore_data[CHORE_MINING].mob))) {
			// uses a dismantler OR a miner
			charge_workforce(emp, CHORE_DISMANTLE_MINES, room, worker, 1, NOTHING, 0);
			if (veh) {
				act("$n begins to dismantle $V.", FALSE, worker, NULL, veh, TO_ROOM | ACT_VEH_VICT);
				start_dismantle_vehicle(veh);
			}
			else {
				act("$n begins to dismantle the building.", FALSE, worker, NULL, NULL, TO_ROOM);
				start_dismantle_building(room);
			}
		}
		else if ((worker = place_chore_worker(emp, CHORE_DISMANTLE_MINES, room))) {
			// fresh worker
			charge_workforce(emp, CHORE_DISMANTLE_MINES, room, worker, 1, NOTHING, 0);
		}
	}
	else {
		mark_workforce_delay(emp, room, CHORE_DISMANTLE_MINES, NOTHING);
	}
}


// INTERACTION_FUNC provides: ch, interaction, inter_room, inter_mob, inter_item, inter_veh
INTERACTION_FUNC(one_einv_interaction_chore) {
	empire_data *emp = inter_veh ? VEH_OWNER(inter_veh) : ROOM_OWNER(inter_room);
	char buf[MAX_STRING_LENGTH];
	int amt;
	
	if (emp && can_gain_chore_resource(emp, inter_room, einv_interaction_chore_type, interaction->vnum)) {
		amt = interact_data[interaction->type].one_at_a_time ? 1 : interaction->quantity;
		ewt_mark_resource_worker(emp, inter_room, interaction->vnum, amt);
		add_to_empire_storage(emp, GET_ISLAND_ID(inter_room), interaction->vnum, amt);
		add_production_total(emp, interaction->vnum, amt);
		add_workforce_production_log(emp, WPLOG_OBJECT, interaction->vnum, amt);
		// only send message if someone else is present (don't bother verifying it's a player)
		if (ROOM_PEOPLE(IN_ROOM(ch))->next_in_room) {
			if (amt > 1) {
				sprintf(buf, "$n produces %s (x%d).", get_obj_name_by_proto(interaction->vnum), amt);
			}
			else {
				sprintf(buf, "$n produces %s.", get_obj_name_by_proto(interaction->vnum));
			}
			act(buf, FALSE, ch, NULL, NULL, TO_ROOM | TO_SPAMMY | TO_QUEUE);
		}
		return TRUE;
	}
	
	return FALSE;
}


/**
* Processes one einv-interaction chore (like sawing or scraping).
*
* @param empire_data *emp The empire doing the chore.
* @param room_data *room The location of the chore.
* @param vehicle_data *veh Optional: If it's a vehicle chore, not a room chore. (NULL for not-a-vehicle-chore.)
* @param int chore Which CHORE_ it is.
* @param int interact_type Which INTERACT_ to run on items in the einv.
*/
void do_chore_einv_interaction(empire_data *emp, room_data *room, vehicle_data *veh, int chore, int interact_type) {
	char_data *worker;
	struct empire_storage_data *store, *next_store, *found_store = NULL;
	obj_data *proto, *found_proto = NULL;
	int islid = GET_ISLAND_ID(room);
	struct empire_island *eisle;
	bool any_over_limit = FALSE;
	int most_found = -1;
	
	// look for something to process
	eisle = get_empire_island(emp, islid);
	HASH_ITER(hh, eisle->store, store, next_store) {
		if (store->amount < 1 || store->keep == UNLIMITED || store->amount <= store->keep) {
			continue;
		}
		if (!(proto = store->proto)) {
			continue;
		}
		if (!has_interaction(GET_OBJ_INTERACTIONS(proto), interact_type)) {
			continue;
		}
		if (!can_gain_chore_resource_from_interaction_list(emp, room, chore, GET_OBJ_INTERACTIONS(proto), interact_type, TRUE)) {
			any_over_limit = TRUE;
			continue;
		}
		
		// found!
		if (store->amount > most_found) {
			found_proto = proto;
			found_store = store;
			most_found = store->amount;
		}
	}
	
	if (found_proto) {
		if ((worker = find_chore_worker_in_room(emp, room, veh, chore_data[chore].mob))) {
			charge_workforce(emp, chore, room, worker, 1, NOTHING, 0);
			einv_interaction_chore_type = chore;
		
			if (run_interactions(worker, GET_OBJ_INTERACTIONS(found_proto), interact_type, room, worker, found_proto, veh, one_einv_interaction_chore) && found_store) {
				charge_stored_resource(emp, islid, found_store->vnum, 1);
			}
		}
		else if ((worker = place_chore_worker(emp, chore, room))) {
			// fresh worker
			ewt_mark_for_interaction_list(emp, room, GET_OBJ_INTERACTIONS(found_proto), interact_type);
			charge_workforce(emp, chore, room, worker, 1, NOTHING, 0);
		}
	}
	else {
		mark_workforce_delay(emp, room, chore, any_over_limit ? WF_PROB_OVER_LIMIT : WF_PROB_NO_RESOURCES);
		log_workforce_problem(emp, room, chore, any_over_limit ? WF_PROB_OVER_LIMIT : WF_PROB_NO_RESOURCES, FALSE);
	}
}


// INTERACTION_FUNC provides: ch, interaction, inter_room, inter_mob, inter_item, inter_veh
INTERACTION_FUNC(one_farming_chore) {
	empire_data *emp = inter_veh ? VEH_OWNER(inter_veh) : ROOM_OWNER(inter_room);
	obj_data *proto = obj_proto(interaction->vnum);
	int amt;
	
	if (emp && proto && GET_OBJ_STORAGE(proto) && can_gain_chore_resource(emp, inter_room, CHORE_FARMING, interaction->vnum)) {
		amt = interact_data[interaction->type].one_at_a_time ? 1 : interaction->quantity;
		ewt_mark_resource_worker(emp, inter_room, interaction->vnum, amt);
		
		add_to_empire_storage(emp, GET_ISLAND_ID(inter_room), interaction->vnum, amt);
		add_production_total(emp, interaction->vnum, amt);
		add_workforce_production_log(emp, WPLOG_OBJECT, interaction->vnum, amt);
		
		// add depletion only if 'pick' (INTERACT_HARVEST doesn't use it)
		if (interaction->type == INTERACT_PICK) {
			while (amt-- > 0) {
				add_depletion(inter_room, DPLTN_PICK, TRUE);
			}
		}

		// only send message if someone else is present (don't bother verifying it's a player)
		if (ROOM_PEOPLE(IN_ROOM(ch))->next_in_room) {
			if (interaction->type == INTERACT_PICK) {
				sprintf(buf, "$n picks %s.", get_obj_name_by_proto(interaction->vnum));
			}
			else {
				sprintf(buf, "$n finishes harvesting the %s.", GET_CROP_NAME(ROOM_CROP(inter_room)));
			}
			act(buf, FALSE, ch, NULL, NULL, TO_ROOM);
		}
		return TRUE;
	}

	return FALSE;
}


// handles harvest/pick (preferring harvest)
void do_chore_farming(empire_data *emp, room_data *room) {
	char_data *worker;
	sector_data *old_sect;
	
	if (CAN_INTERACT_ROOM_NO_VEH(room, INTERACT_HARVEST) && can_gain_chore_resource_from_interaction_room(emp, room, CHORE_FARMING, INTERACT_HARVEST)) {
		// HARVEST mode: all at once; not able to ewt_mark_resource_worker() until we're inside the interact
		if ((worker = find_chore_worker_in_room(emp, room, NULL, chore_data[CHORE_FARMING].mob))) {
			// farming is free
			charge_workforce(emp, CHORE_FARMING, room, worker, 0, NOTHING, 0);
			
			// set up harvest time if needed
			if (get_room_extra_data(room, ROOM_EXTRA_HARVEST_PROGRESS) <= 0) {
				set_room_extra_data(room, ROOM_EXTRA_HARVEST_PROGRESS, config_get_int("harvest_timer"));
			}
			
			// harvest ticker
			add_to_room_extra_data(room, ROOM_EXTRA_HARVEST_PROGRESS, -1);
			
			// check progress
			if (get_room_extra_data(room, ROOM_EXTRA_HARVEST_PROGRESS) > 0) {
				// not done:
				ewt_mark_for_interactions(emp, room, INTERACT_HARVEST);
				
				// only send message if someone else is present (don't bother verifying it's a player)
				if (ROOM_PEOPLE(IN_ROOM(worker))->next_in_room) {
					act("$n tends the crop...", FALSE, worker, NULL, NULL, TO_ROOM);
				}
			}
			else {	// DONE!
				remove_room_extra_data(room, ROOM_EXTRA_HARVEST_PROGRESS);
				run_room_interactions(worker, room, INTERACT_HARVEST, NULL, NOTHING, one_farming_chore);
				
				// now change to seeded or uncrop:
				if (empire_chore_limit(emp, GET_ISLAND_ID(room), CHORE_REPLANTING) && (old_sect = reverse_lookup_evolution_for_sector(SECT(room), EVO_CROP_GROWS))) {
					// sly-convert back to what it was grown from ... not using change_terrain
					perform_change_sect(room, NULL, old_sect);
					check_terrain_height(room);
			
					// we are keeping the original sect the same as it was; set the time to one game day
					set_room_extra_data(room, ROOM_EXTRA_SEED_TIME, time(0) + (24 * SECS_PER_MUD_HOUR));
					if (GET_MAP_LOC(room)) {
						schedule_crop_growth(GET_MAP_LOC(room));
					}
				}
				else {
					// change to base sect
					uncrop_tile(room);
					
					if (!ROOM_AFF_FLAGGED(room, ROOM_AFF_NO_ABANDON) && empire_chore_limit(emp, GET_ISLAND_ID(room), CHORE_ABANDON_FARMED)) {
						abandon_room(room);
					}
				}
				
				// stop all possible chores here since the sector changed
				stop_room_action(room, ACT_HARVESTING);
				stop_room_action(room, ACT_CHOPPING);
				stop_room_action(room, ACT_PICKING);
				stop_room_action(room, ACT_GATHERING);
			}
		}
		else if ((worker = place_chore_worker(emp, CHORE_FARMING, room))) {
			// fresh worker
			ewt_mark_for_interactions(emp, room, INTERACT_HARVEST);
			charge_workforce(emp, CHORE_FARMING, room, worker, 0, NOTHING, 0);
		}
	}
	else if (CAN_INTERACT_ROOM_NO_VEH(room, INTERACT_PICK) && can_gain_chore_resource_from_interaction_room(emp, room, CHORE_FARMING, INTERACT_PICK)) {
		// PICK mode: 1 at a time; not able to ewt_mark_resource_worker() until we're inside the interact
		if ((worker = find_chore_worker_in_room(emp, room, NULL, chore_data[CHORE_FARMING].mob))) {
			// farming is free
			charge_workforce(emp, CHORE_FARMING, room, worker, 0, NOTHING, 0);
			run_room_interactions(worker, room, INTERACT_PICK, NULL, NOTHING, one_farming_chore);
			
			// only change to seeded if it's over-picked			
			if (get_depletion(room, DPLTN_PICK) >= get_interaction_depletion_room(NULL, emp, room, INTERACT_PICK, TRUE)) {
				if (empire_chore_limit(emp, GET_ISLAND_ID(room), CHORE_REPLANTING) && (old_sect = reverse_lookup_evolution_for_sector(SECT(room), EVO_CROP_GROWS))) {
					// sly-convert back to what it was grown from ... not using change_terrain
					perform_change_sect(room, NULL, old_sect);
					check_terrain_height(room);
			
					// we are keeping the original sect the same as it was; set the time to one game day
					set_room_extra_data(room, ROOM_EXTRA_SEED_TIME, time(0) + (24 * SECS_PER_MUD_HOUR));
					if (GET_MAP_LOC(room)) {
						schedule_crop_growth(GET_MAP_LOC(room));
					}
				}
				else {
					// change to base sect
					uncrop_tile(room);
					
					if (!ROOM_AFF_FLAGGED(room, ROOM_AFF_NO_ABANDON) && empire_chore_limit(emp, GET_ISLAND_ID(room), CHORE_ABANDON_FARMED)) {
						abandon_room(room);
					}
				}
				
				// stop all possible chores here since the sector changed
				stop_room_action(room, ACT_HARVESTING);
				stop_room_action(room, ACT_CHOPPING);
				stop_room_action(room, ACT_PICKING);
				stop_room_action(room, ACT_GATHERING);
			}
		}
		else if ((worker = place_chore_worker(emp, CHORE_FARMING, room))) {
			// fresh worker
			ewt_mark_for_interactions(emp, room, INTERACT_PICK);
			charge_workforce(emp, CHORE_FARMING, room, worker, 0, NOTHING, 0);
		}
	}
	else {
		mark_workforce_delay(emp, room, CHORE_FARMING, WF_PROB_OVER_LIMIT);
		log_workforce_problem(emp, room, CHORE_FARMING, WF_PROB_OVER_LIMIT, FALSE);
	}
}


// INTERACTION_FUNC provides: ch, interaction, inter_room, inter_mob, inter_item, inter_veh
INTERACTION_FUNC(one_fishing_chore) {
	empire_data *emp = inter_veh ? VEH_OWNER(inter_veh) : ROOM_OWNER(inter_room);
	char buf[MAX_STRING_LENGTH];
	int amt, depletion_type;
	
	// make sure this item isn't depleted
	depletion_type = determine_depletion_type(interaction);
	if (emp && GET_CHORE_DEPLETION(inter_room, inter_veh, depletion_type != NOTHING ? depletion_type : DPLTN_PRODUCTION) >= (interact_data[interaction->type].one_at_a_time ? interaction->quantity : DEPLETION_LIMIT(inter_room))) {
		return FALSE;
	}
	
	if (emp && can_gain_chore_resource(emp, inter_room, CHORE_FISHING, interaction->vnum)) {
		amt = interact_data[interaction->type].one_at_a_time ? 1 : interaction->quantity;
		ewt_mark_resource_worker(emp, inter_room, interaction->vnum, amt);
		add_to_empire_storage(emp, GET_ISLAND_ID(inter_room), interaction->vnum, amt);
		add_production_total(emp, interaction->vnum, amt);
		add_workforce_production_log(emp, WPLOG_OBJECT, interaction->vnum, amt);
		ADD_CHORE_DEPLETION(inter_room, inter_veh, depletion_type, TRUE);
		// only send message if someone else is present (don't bother verifying it's a player)
		if (ROOM_PEOPLE(IN_ROOM(ch))->next_in_room) {
			if (amt > 1) {
				sprintf(buf, "$n catches %s (x%d).", get_obj_name_by_proto(interaction->vnum), amt);
			}
			else {
				sprintf(buf, "$n catches %s.", get_obj_name_by_proto(interaction->vnum));
			}
			act(buf, FALSE, ch, NULL, NULL, TO_ROOM | TO_SPAMMY | TO_QUEUE);
		}
		return TRUE;
	}
	
	return FALSE;
}


/**
* @param empire_data *emp The empire for the chore.
* @param room_data *room The location of the chore.
* @param vehicle_data *veh Optional: If the chore is performed by a vehicle, this is set.
*/
void do_chore_fishing(empire_data *emp, room_data *room, vehicle_data *veh) {	
	char_data *worker;
	bool over_limit = FALSE;
	
	if (has_any_undepleted_interaction_for_chore(emp, CHORE_FISHING, room, veh, INTERACT_FISH, &over_limit)) {
		if ((worker = find_chore_worker_in_room(emp, room, veh, chore_data[CHORE_FISHING].mob))) {
			// fishing is free
			charge_workforce(emp, CHORE_FISHING, room, worker, 0, NOTHING, 0);
			if (veh) {
				run_interactions(worker, VEH_INTERACTIONS(veh), INTERACT_FISH, room, NULL, NULL, veh, one_fishing_chore);
			}
			else {
				run_room_interactions(worker, room, INTERACT_FISH, veh, NOTHING, one_fishing_chore);
			}
		}
		else if ((worker = place_chore_worker(emp, CHORE_FISHING, room))) {
			// fresh worker
			ewt_mark_for_interactions(emp, room, INTERACT_FISH);
			charge_workforce(emp, CHORE_FISHING, room, worker, 0, NOTHING, 0);
		}
	}
	else {
		log_workforce_problem(emp, room, CHORE_FISHING, over_limit ? WF_PROB_OVER_LIMIT : WF_PROB_DEPLETED, FALSE);
	}
}


// for non-vehicles
void do_chore_fire_brigade(empire_data *emp, room_data *room) {
	char_data *worker;
	int total_ticks, per_hour;
	
	if (IS_BURNING(room)) {
		if ((worker = find_chore_worker_in_room(emp, room, NULL, chore_data[CHORE_FIRE_BRIGADE].mob))) {
			charge_workforce(emp, CHORE_FIRE_BRIGADE, room, worker, 1, NOTHING, 0);
		
			act("$n throws a bucket of water to douse the flames!", FALSE, worker, NULL, NULL, TO_ROOM);
		
			// compute how many in order to put it out before it burns down (giving the mob an hour to spawn)
			total_ticks = (int)(config_get_int("burn_down_time") / SECS_PER_MUD_HOUR) - 2;
			per_hour = ceil(config_get_int("fire_extinguish_value") / total_ticks) + 1;
		
			add_to_room_extra_data(room, ROOM_EXTRA_FIRE_REMAINING, -per_hour);

			if (get_room_extra_data(room, ROOM_EXTRA_FIRE_REMAINING) <= 0) {
				act("The flames have been extinguished!", FALSE, worker, 0, 0, TO_ROOM);
				stop_burning(room);
				add_workforce_production_log(emp, WPLOG_FIRE_EXTINGUISHED, 0, 1);
			}		
		}
		else if ((worker = place_chore_worker(emp, CHORE_FIRE_BRIGADE, room))) {
			// fresh worker
			charge_workforce(emp, CHORE_FIRE_BRIGADE, room, worker, 1, NOTHING, 0);
		}
	}
	// never mark delay on this
}


// INTERACTION_FUNC provides: ch, interaction, inter_room, inter_mob, inter_item, inter_veh
INTERACTION_FUNC(one_mining_chore) {
	empire_data *emp = inter_veh ? VEH_OWNER(inter_veh) : ROOM_OWNER(inter_room);
	struct global_data *mine;
	obj_data *proto;
	
	// no mine
	if (!(mine = global_proto(get_room_extra_data(inter_room, ROOM_EXTRA_MINE_GLB_VNUM)))) {
		return FALSE;
	}
	
	// find object
	proto = obj_proto(interaction->vnum);
	
	// check vars and limits
	if (!emp || !proto || !GET_OBJ_STORAGE(proto) || !can_gain_chore_resource(emp, inter_room, CHORE_MINING, interaction->vnum)) {
		return FALSE;
	}
	
	// good to go
	ewt_mark_resource_worker(emp, inter_room, interaction->vnum, interaction->quantity);
	
	// mine ~ every sixth time
	if (!number(0, 5)) {
		if (interaction->quantity > 0) {
			add_to_room_extra_data(inter_room, ROOM_EXTRA_MINE_AMOUNT, -1 * interaction->quantity);
			add_to_empire_storage(emp, GET_ISLAND_ID(inter_room), interaction->vnum, interaction->quantity);
			add_production_total(emp, interaction->vnum, interaction->quantity);
			add_workforce_production_log(emp, WPLOG_OBJECT, interaction->vnum, interaction->quantity);
			
			// set as prospected
			set_room_extra_data(inter_room, ROOM_EXTRA_PROSPECT_EMPIRE, EMPIRE_VNUM(emp));
			
			// only send message if someone else is present (don't bother verifying it's a player)
			if (ROOM_PEOPLE(IN_ROOM(ch))->next_in_room) {
				if (interaction->quantity > 1) {
					sprintf(buf, "$n strikes the wall and %s (x%d) falls loose!", get_obj_name_by_proto(interaction->vnum), interaction->quantity);
				}
				else {
					sprintf(buf, "$n strikes the wall and %s falls loose!", get_obj_name_by_proto(interaction->vnum));
				}
				act(buf, FALSE, ch, NULL, NULL, TO_ROOM | TO_SPAMMY | TO_QUEUE);
			}
			return TRUE;
		}
		else {
			return FALSE;
		}
	}
	else {
		// only send message if someone else is present (don't bother verifying it's a player)
		if (ROOM_PEOPLE(IN_ROOM(ch))->next_in_room) {
			act("$n works the mine...", FALSE, ch, NULL, NULL, TO_ROOM | TO_SPAMMY | TO_QUEUE);
		}
	}
	// didn't mine this time, still return TRUE
	return TRUE;
}


/**
* @param empire_data *emp The empire for the chore.
* @param room_data *room The location of the chore.
* @param vehicle_data *veh Optional: If the chore is performed by a vehicle, this is set.
*/
void do_chore_mining(empire_data *emp, room_data *room, vehicle_data *veh) {
	char_data *worker;
	struct global_data *mine = global_proto(get_room_extra_data(room, ROOM_EXTRA_MINE_GLB_VNUM));
	bool depleted = (!mine || GET_GLOBAL_TYPE(mine) != GLOBAL_MINE_DATA || get_room_extra_data(room, ROOM_EXTRA_MINE_AMOUNT) <= 0);
	bool can_gain = can_gain_chore_resource_from_interaction_list(emp, room, CHORE_MINING, GET_GLOBAL_INTERACTIONS(mine), INTERACT_MINE, TRUE);
	bool can_do = (!depleted && can_gain);
	
	if (can_do) {
		// not able to ewt_mark_resource_worker() until we're inside the interact
		if ((worker = find_chore_worker_in_room(emp, room, veh, chore_data[CHORE_MINING].mob))) {
			charge_workforce(emp, CHORE_MINING, room, worker, 1, NOTHING, 0);
			run_interactions(worker, GET_GLOBAL_INTERACTIONS(mine), INTERACT_MINE, room, worker, NULL, veh, one_mining_chore);
			
			// check for depletion
			if (get_room_extra_data(room, ROOM_EXTRA_MINE_AMOUNT) <= 0) {
				stop_room_action(room, ACT_MINING);
			}
		}
		else if ((worker = place_chore_worker(emp, CHORE_MINING, room))) {
			// fresh worker
			ewt_mark_for_interaction_list(emp, room, GET_GLOBAL_INTERACTIONS(mine), INTERACT_MINE);
			charge_workforce(emp, CHORE_MINING, room, worker, 1, NOTHING, 0);
		}
	}
	else {
		mark_workforce_delay(emp, room, CHORE_MINING, depleted ? WF_PROB_DEPLETED : WF_PROB_OVER_LIMIT);
		log_workforce_problem(emp, room, CHORE_MINING, depleted ? WF_PROB_DEPLETED : WF_PROB_OVER_LIMIT, FALSE);
	}
}


/**
* @param empire_data *emp The empire for the chore.
* @param room_data *room The location of the chore.
* @param vehicle_data *veh Optional: If the chore is performed by a vehicle, this is set.
*/
void do_chore_minting(empire_data *emp, room_data *room, vehicle_data *veh) {
	struct empire_storage_data *highest = NULL, *store, *next_store;
	char_data *worker;
	int amt, high_amt, limit, islid = GET_ISLAND_ID(room);
	struct empire_island *eisle = get_empire_island(emp, islid);
	char buf[MAX_STRING_LENGTH];
	bool can_do = TRUE;
	obj_data *orn;
	obj_vnum vnum;
	
	limit = empire_chore_limit(emp, islid, CHORE_MINTING);
	if (EMPIRE_COINS(emp) >= MAX_COIN || (limit != WORKFORCE_UNLIMITED && EMPIRE_COINS(emp) >= limit)) {
		mark_workforce_delay(emp, room, CHORE_MINTING, WF_PROB_OVER_LIMIT);
		log_workforce_problem(emp, room, CHORE_MINTING, WF_PROB_OVER_LIMIT, FALSE);
		can_do = FALSE;
	}
	
	// detect available treasure
	if (can_do) {
		// first, find the best item to mint
		highest = NULL;
		high_amt = 0;
		HASH_ITER(hh, eisle->store, store, next_store) {
			if (store->keep == UNLIMITED || store->amount <= store->keep) {
				continue;
			}
			
			orn = store->proto;
			if (orn && store->amount >= 1 && IS_WEALTH_ITEM(orn) && GET_WEALTH_VALUE(orn) > 0 && IS_MINT_FLAGGED(orn, MINT_FLAG_AUTOMINT) && !IS_MINT_FLAGGED(orn, MINT_FLAG_NO_MINT)) {
				if (highest == NULL || store->amount > high_amt) {
					highest = store;
					high_amt = store->amount;
				}
			}
		}
	}
	
	if (can_do && highest) {
		if ((worker = find_chore_worker_in_room(emp, room, veh, chore_data[CHORE_MINTING].mob))) {
			charge_workforce(emp, CHORE_MINTING, room, worker, 1, NOTHING, 0);
			
			// let's only do this every ~4 hours
			if (!number(0, 3)) {
				// only send message if someone else is present (don't bother verifying it's a player)
				if (ROOM_PEOPLE(IN_ROOM(worker))->next_in_room) {
					act("$n works the mint...", FALSE, worker, NULL, NULL, TO_ROOM | TO_SPAMMY | TO_QUEUE);
				}
				return;
			}
			
			vnum = highest->vnum;
			charge_stored_resource(emp, islid, highest->vnum, 1);
			
			orn = obj_proto(vnum);	// existence of this was pre-validated
			amt = GET_WEALTH_VALUE(orn) * (1.0/COIN_VALUE);
			increase_empire_coins(emp, emp, amt);
			add_workforce_production_log(emp, WPLOG_COINS, 0, amt);
			
			if (amt > 1) {
				snprintf(buf, sizeof(buf), "$n finishes minting a batch of %d %s coins.", amt, EMPIRE_ADJECTIVE(emp));
			}
			else {
				snprintf(buf, sizeof(buf), "$n finishes minting a single %s coin.", EMPIRE_ADJECTIVE(emp));
			}
			act(buf, FALSE, worker, NULL, NULL, TO_ROOM | TO_QUEUE | TO_SPAMMY);
		}
		else if ((worker = place_chore_worker(emp, CHORE_MINTING, room))) {
			// fresh worker
			charge_workforce(emp, CHORE_MINTING, room, worker, 1, NOTHING, 0);
		}
	}
	else if (!highest) {
		mark_workforce_delay(emp, room, CHORE_MINTING, WF_PROB_NO_RESOURCES);
		log_workforce_problem(emp, room, CHORE_MINTING, WF_PROB_NO_RESOURCES, FALSE);
	}
}


// INTERACTION_FUNC provides: ch, interaction, inter_room, inter_mob, inter_item, inter_veh
INTERACTION_FUNC(one_production_chore) {
	empire_data *emp = inter_veh ? VEH_OWNER(inter_veh) : ROOM_OWNER(inter_room);
	char buf[MAX_STRING_LENGTH], amtbuf[256];
	int amt, depletion_type;
	
	// make sure this item isn't depleted
	depletion_type = determine_depletion_type(interaction);
	if (emp && GET_CHORE_DEPLETION(inter_room, inter_veh, depletion_type != NOTHING ? depletion_type : DPLTN_PRODUCTION) >= (interact_data[interaction->type].one_at_a_time ? interaction->quantity : DEPLETION_LIMIT(inter_room))) {
		return FALSE;
	}
	
	if (emp && can_gain_chore_resource(emp, inter_room, CHORE_PRODUCTION, interaction->vnum)) {
		amt = interact_data[interaction->type].one_at_a_time ? 1 : interaction->quantity;
		ewt_mark_resource_worker(emp, inter_room, interaction->vnum, amt);
		add_to_empire_storage(emp, GET_ISLAND_ID(inter_room), interaction->vnum, amt);
		add_production_total(emp, interaction->vnum, amt);
		add_workforce_production_log(emp, WPLOG_OBJECT, interaction->vnum, amt);
		
		ADD_CHORE_DEPLETION(inter_room, inter_veh, depletion_type, TRUE);
		
		// only send message if someone else is present (don't bother verifying it's a player)
		if (ROOM_PEOPLE(IN_ROOM(ch))->next_in_room) {
			if (amt > 1) {
				snprintf(amtbuf, sizeof(amtbuf), " (x%d)", amt);
			}
			else {
				*amtbuf = '\0';
			}
			
			if (inter_veh) {
				sprintf(buf, "$n produces %s%s from $V.", get_obj_name_by_proto(interaction->vnum), amtbuf);
			}
			else {
				sprintf(buf, "$n produces %s%s.", get_obj_name_by_proto(interaction->vnum), amtbuf);
			}
			act(buf, FALSE, ch, NULL, inter_veh, TO_ROOM | TO_SPAMMY | TO_QUEUE | ACT_VEH_VICT);
		}
		return TRUE;
	}
	
	return FALSE;
}


/**
* General production
*
* @param empire_data *emp The empire for the chore.
* @param room_data *room The location of the chore.
* @param vehicle_data *veh Optional: If the chore is performed by a vehicle, this is set.
* @param int interact_type INTERACT_PRODUCTION or INTERACT_SKILLED_LABOR
*/
void do_chore_production(empire_data *emp, room_data *room, vehicle_data *veh, int interact_type) {
	bool over_limit = FALSE;
	char_data *worker;
	
	if (has_any_undepleted_interaction_for_chore(emp, CHORE_PRODUCTION, room, veh, interact_type, &over_limit)) {
		if ((worker = find_chore_worker_in_room(emp, room, veh, chore_data[CHORE_PRODUCTION].mob))) {
			charge_workforce(emp, CHORE_PRODUCTION, room, worker, 1, NOTHING, 0);
		
			if (veh && run_interactions(worker, VEH_INTERACTIONS(veh), interact_type, room, NULL, NULL, veh, one_production_chore)) {
				// successful vehicle interact
			}
			else if (!veh && run_room_interactions(worker, room, interact_type, veh, NOTHING, one_production_chore)) {
				// successful room interact
			}
			// no else: these interactions may fail due to low percentages
		}
		else if ((worker = place_chore_worker(emp, CHORE_PRODUCTION, room))) {
			// fresh worker
			ewt_mark_for_interactions(emp, room, interact_type);
			charge_workforce(emp, CHORE_PRODUCTION, room, worker, 1, NOTHING, 0);
		}
	}
	else {
		// problem
		log_workforce_problem(emp, room, CHORE_PRODUCTION, over_limit ? WF_PROB_OVER_LIMIT : WF_PROB_DEPLETED, FALSE);
	}
}


void do_chore_prospecting(empire_data *emp, room_data *room) {
	bool can_mine = ROOM_CAN_MINE(room);
	bool prospected_by_emp = (get_room_extra_data(room, ROOM_EXTRA_PROSPECT_EMPIRE) == EMPIRE_VNUM(emp));
	bool has_ore = (get_room_extra_data(room, ROOM_EXTRA_MINE_AMOUNT) > 0);
	bool undetermined = (get_room_extra_data(room, ROOM_EXTRA_MINE_GLB_VNUM) <= 0);
	bool needs_prospect = (can_mine && (undetermined || (!prospected_by_emp && has_ore))); 
	char_data *worker;
	
	if (needs_prospect) {
		if ((worker = find_chore_worker_in_room(emp, room, NULL, chore_data[CHORE_PROSPECTING].mob))) {
			charge_workforce(emp, CHORE_PROSPECTING, room, worker, 1, NOTHING, 0);
			add_to_room_extra_data(room, ROOM_EXTRA_WORKFORCE_PROSPECT, 1);
		
			if (get_room_extra_data(room, ROOM_EXTRA_WORKFORCE_PROSPECT) < config_get_int("prospecting_workforce_hours")) {
				// still working: only send message if someone else is present (don't bother verifying it's a player)
				if (ROOM_PEOPLE(IN_ROOM(worker))->next_in_room) {
					switch (number(0, 2)) {
						case 0: {
							act("$n picks at the soil...", FALSE, worker, NULL, NULL, TO_ROOM | TO_SPAMMY | TO_QUEUE);
							break;
						}
						case 1: {
							act("$n tastes a pinch of soil...", FALSE, worker, NULL, NULL, TO_ROOM | TO_SPAMMY | TO_QUEUE);
							break;
						}
						case 2: {
							act("$n sifts through the dirt...", FALSE, worker, NULL, NULL, TO_ROOM | TO_SPAMMY | TO_QUEUE);
							break;
						}
					}
				}
			}
			else {
				// finished working
				act("$n finishes prospecting!", FALSE, worker, NULL, NULL, TO_ROOM | TO_SPAMMY | TO_QUEUE);
			
				// pass NULL for ch to init_mine so it just uses the empire
				init_mine(room, NULL, emp);
				set_room_extra_data(room, ROOM_EXTRA_PROSPECT_EMPIRE, EMPIRE_VNUM(emp));
				remove_room_extra_data(room, ROOM_EXTRA_WORKFORCE_PROSPECT);
				add_workforce_production_log(emp, WPLOG_PROSPECTED, 0, 1);
			}
		}
		else if ((worker = place_chore_worker(emp, CHORE_PROSPECTING, room))) {
			// fresh worker
			charge_workforce(emp, CHORE_PROSPECTING, room, worker, 1, NOTHING, 0);
		}
	}
	else if (can_mine && !undetermined && !prospected_by_emp && !has_ore) {
		// empty -- just mark it for them
		set_room_extra_data(room, ROOM_EXTRA_PROSPECT_EMPIRE, EMPIRE_VNUM(emp));
		remove_room_extra_data(room, ROOM_EXTRA_WORKFORCE_PROSPECT);
	}
	else if (can_mine && !needs_prospect) {
		// actually do not need this error
		// mark_workforce_delay(emp, room, CHORE_PROSPECTING, WF_PROB_ALREADY_PROSPECTED);
	}
}


/**
* @param empire_data *emp The empire for the chore.
* @param room_data *room The location of the chore.
* @param vehicle_data *veh Optional: If the chore is performed by a vehicle, this is set.
*/
void do_chore_shearing(empire_data *emp, room_data *room, vehicle_data *veh) {
	int shear_growth_time = config_get_int("shear_growth_time");
	
	char_data *worker;
	char_data *mob, *shearable = NULL;
	
	bool any_already_sheared = FALSE;
	struct interact_exclusion_data *excl = NULL;
	struct interaction_item *interact;
	bool found;

	// find something shearable
	DL_FOREACH2(ROOM_PEOPLE(room), mob, next_in_room) {
		if (shearable) {
			break;	// found 1? exit early
		}
		
		// would its shearing timer be up?
		if (IS_NPC(mob)) {
			if (get_cooldown_time(mob, COOLDOWN_SHEAR) > 0) {
				any_already_sheared = TRUE;
				continue;
			}
			
			// find shear interaction
			for (interact = mob->interactions; interact && !shearable; interact = interact->next) {
				if (interact->type != INTERACT_SHEAR || !meets_interaction_restrictions(interact->restrictions, NULL, emp, mob, NULL)) {
					continue;
				}
				if (!can_gain_chore_resource(emp, room, CHORE_SHEARING, interact->vnum)) {
					continue;
				}
				
				shearable = mob;
			}
		}
	}
	
	// can work?
	if (shearable) {
		if ((worker = find_chore_worker_in_room(emp, room, veh, chore_data[CHORE_SHEARING].mob))) {
			charge_workforce(emp, CHORE_SHEARING , room, worker, 1, NOTHING, 0);
			found = FALSE;
		
			// we know it's shearable, but have to find the items
			for (interact = shearable->interactions; interact; interact = interact->next) {
				if (interact->type == INTERACT_SHEAR && check_exclusion_set(&excl, interact->exclusion_code, interact->percent)) {
					// messaging
					if (!found) {
						ewt_mark_resource_worker(emp, room, interact->vnum, interact->quantity);
					
						// only send message if someone else is present (don't bother verifying it's a player)
						if (ROOM_PEOPLE(IN_ROOM(worker))->next_in_room) {
							act("$n shears $N.", FALSE, worker, NULL, shearable, TO_ROOM | TO_QUEUE | TO_SPAMMY);
						}
						found = TRUE;
					}
				
					add_to_empire_storage(emp, GET_ISLAND_ID(room), interact->vnum, interact->quantity);
					add_production_total(emp, interact->vnum, interact->quantity);
					add_workforce_production_log(emp, WPLOG_OBJECT, interact->vnum, interact->quantity);
					add_cooldown(shearable, COOLDOWN_SHEAR, shear_growth_time * SECS_PER_REAL_HOUR);
				}
			}
		}
		else if ((worker = place_chore_worker(emp, CHORE_SHEARING, room))) {
			// fresh worker
			charge_workforce(emp, CHORE_SHEARING, room, worker, 1, NOTHING, 0);
		}
	}
	else {
		mark_workforce_delay(emp, room, CHORE_SHEARING, any_already_sheared ? WF_PROB_ALREADY_SHEARED : WF_PROB_OVER_LIMIT);
		log_workforce_problem(emp, room, CHORE_SHEARING, any_already_sheared ? WF_PROB_ALREADY_SHEARED : WF_PROB_OVER_LIMIT, FALSE);
	}
	
	free_exclusion_data(excl);
}


 /////////////////////////////////////////////////////////////////////////////
//// VEHICLE CHORE FUNCTIONS ////////////////////////////////////////////////

void vehicle_chore_fire_brigade(empire_data *emp, vehicle_data *veh) {
	char_data *worker = find_chore_worker_in_room(emp, IN_ROOM(veh), veh, chore_data[CHORE_FIRE_BRIGADE].mob);
	
	if (worker && VEH_FLAGGED(veh, VEH_ON_FIRE)) {
		charge_workforce(emp, CHORE_FIRE_BRIGADE, IN_ROOM(veh), worker, 1, NOTHING, 0);
		remove_vehicle_flags(veh, VEH_ON_FIRE);
		add_workforce_production_log(emp, WPLOG_FIRE_EXTINGUISHED, 0, 1);
		
		act("$n throws a bucket of water to douse the flames!", FALSE, worker, NULL, NULL, TO_ROOM);
		msg_to_vehicle(veh, FALSE, "The flames have been extinguished!\r\n");
	}
	else if (VEH_FLAGGED(veh, VEH_ON_FIRE)) {
		if ((worker = place_chore_worker(emp, CHORE_FIRE_BRIGADE, IN_ROOM(veh)))) {
			charge_workforce(emp, CHORE_FIRE_BRIGADE, IN_ROOM(veh), worker, 1, NOTHING, 0);
		}
	}
}


// handles both build (CHORE_BUILD) and repair (CHORE_MAINTENANCE)
void vehicle_chore_build(empire_data *emp, vehicle_data *veh, int chore) {
	char_data *worker;
	struct empire_storage_data *store = NULL;
	int islid = GET_ISLAND_ID(IN_ROOM(veh));
	char buf[MAX_STRING_LENGTH];
	struct resource_data *res;
	bool can_do = FALSE;
	
	if ((res = VEH_NEEDS_RESOURCES(veh))) {
		// RES_x: can ONLY do it if it requires an object, component, or action
		if (res->type == RES_OBJECT && (store = find_stored_resource(emp, islid, res->vnum)) && store->amount > 0 && store->keep != UNLIMITED && store->amount > store->keep && store->amount > 0) {
			can_do = TRUE;
		}
		else if (res->type == RES_COMPONENT && empire_can_afford_component(emp, islid, res->vnum, 1, FALSE, TRUE)) {
			can_do = TRUE;
		}
		else if (res->type == RES_ACTION || res->type == RES_TOOL) {
			// workforce can always do actions/tools
			// TODO: some day, empires could have a list of tools available and this could require them
			can_do = TRUE;
		}
	}
	
	if (can_do) {
		if ((worker = find_chore_worker_in_room(emp, IN_ROOM(veh), veh, chore_data[chore].mob))) {
			charge_workforce(emp, chore, IN_ROOM(veh), worker, 1, NOTHING, 0);
		
			if (res) {
				if (res->type == RES_OBJECT) {
					if (chore == CHORE_MAINTENANCE) {
						// remove an older matching object
						remove_like_item_from_built_with(&VEH_BUILT_WITH(veh), obj_proto(res->vnum));
					}
					charge_stored_resource(emp, islid, res->vnum, 1);
					if (!VEH_FLAGGED(veh, VEH_NEVER_DISMANTLE)) {
						add_to_resource_list(&VEH_BUILT_WITH(veh), RES_OBJECT, res->vnum, 1, 1);
					}
				}
				else if (res->type == RES_COMPONENT) {
					if (chore == CHORE_MAINTENANCE) {
						// remove an older matching component
						remove_like_component_from_built_with(&VEH_BUILT_WITH(veh), res->vnum);
					}
					charge_stored_component(emp, islid, res->vnum, 1, FALSE, TRUE, VEH_FLAGGED(veh, VEH_NEVER_DISMANTLE) ? NULL : &VEH_BUILT_WITH(veh));
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
				if (chore == CHORE_MAINTENANCE) {
					add_workforce_production_log(emp, WPLOG_MAINTENANCE, 0, 1);
					act("$n finishes repairing $V.", FALSE, worker, NULL, veh, TO_ROOM | ACT_VEH_VICT);
				}
				else {
					add_workforce_production_log(emp, WPLOG_VEHICLE_DONE, VEH_VNUM(veh), 1);
					act("$n finishes constructing $V.", FALSE, worker, NULL, veh, TO_ROOM | ACT_VEH_VICT);
				}
				complete_vehicle(veh);
			}
			else {
				sprintf(buf, "$n works on %s $V.", (chore == CHORE_MAINTENANCE) ? "repairing" : "constructing");
				act(buf, FALSE, worker, NULL, veh, TO_ROOM | TO_SPAMMY | ACT_VEH_VICT);
				request_vehicle_save_in_world(veh);
			}
		}
		else if ((worker = place_chore_worker(emp, chore, IN_ROOM(veh)))) {
			// fresh worker
			charge_workforce(emp, chore, IN_ROOM(veh), worker, 1, NOTHING, 0);
		}
	}
	else {
		// veh could be purged by this point, but only if can_do was TRUE
		log_workforce_problem(emp, IN_ROOM(veh), chore, WF_PROB_NO_RESOURCES, FALSE);
	}
}


// handles CHORE_BUILD on vehicles that are mid-dismantle
void vehicle_chore_dismantle(empire_data *emp, vehicle_data *veh) {
	char_data *worker;
	bool can_do = FALSE, claims_with_room;
	struct resource_data *res, *found_res = NULL;
	int islid = GET_ISLAND_ID(IN_ROOM(veh));
	room_data *room = IN_ROOM(veh);
	char_data *chiter;
	obj_data *proto;
	
	// anything we can dismantle?
	if (!VEH_NEEDS_RESOURCES(veh)) {
		can_do = TRUE;
	}
	else {
		LL_FOREACH(VEH_NEEDS_RESOURCES(veh), res) {
			if (res->type == RES_OBJECT && (proto = obj_proto(res->vnum)) && GET_OBJ_STORAGE(proto)) {
				can_do = TRUE;
				found_res = res;
				break;
			}
		}
	}
	
	if (can_do) {
		if ((worker = find_chore_worker_in_room(emp, IN_ROOM(veh), veh, chore_data[CHORE_BUILDING].mob))) {
			charge_workforce(emp, CHORE_BUILDING, room, worker, 1, NOTHING, 0);
		
			if (found_res) {
				if (found_res->amount > 0) {
					found_res->amount -= 1;
					add_to_empire_storage(emp, islid, found_res->vnum, 1);
				}
			
				// remove res?
				if (found_res->amount <= 0) {
					LL_DELETE(VEH_NEEDS_RESOURCES(veh), found_res);
					free(found_res);
					found_res = NULL;
				}
			}
		
			// check for completion
			if (!VEH_NEEDS_RESOURCES(veh)) {
				// shut off any player who was working on this dismantle
				DL_FOREACH2(ROOM_PEOPLE(room), chiter, next_in_room) {
					if (!IS_NPC(chiter) && (GET_ACTION(chiter) == ACT_DISMANTLE_VEHICLE || GET_ACTION(chiter) == ACT_GEN_CRAFT)) {
						if (GET_ACTION_VNUM(chiter, 1) == VEH_CONSTRUCTION_ID(veh)) {
							cancel_action(chiter);
						}
					}
				}
				// no need to stop worker or other npcs -- npcs working on vehicles have SPAWNED
			
				// ok, finish dismantle (purges vehicle)
				claims_with_room = VEH_CLAIMS_WITH_ROOM(veh);
				add_workforce_production_log(emp, WPLOG_VEHICLE_DISMANTLED, VEH_VNUM(veh), 1);
				finish_dismantle_vehicle(worker, veh);	// ** sends own message **
			
				// auto-abandon?
				if (claims_with_room && !ROOM_AFF_FLAGGED(room, ROOM_AFF_NO_ABANDON) && empire_chore_limit(emp, islid, CHORE_ABANDON_DISMANTLED)) {
					// auto-abandon only if they have no other buildings left
					if (count_building_vehicles_in_room(room, ROOM_OWNER(room)) == 0) {
						abandon_room(room);
					}
				}
			}
			else {	// still working on it
				act("$n works on dismantling $V.", FALSE, worker, NULL, veh, TO_ROOM | TO_SPAMMY | ACT_VEH_VICT);
			}
		}
		else if ((worker = place_chore_worker(emp, CHORE_BUILDING, IN_ROOM(veh)))) {
			// fresh worker
			charge_workforce(emp, CHORE_BUILDING, IN_ROOM(veh), worker, 1, NOTHING, 0);
		}
	}
	else {
		log_workforce_problem(emp, room, CHORE_BUILDING, WF_PROB_NO_RESOURCES, FALSE);
	}
}
