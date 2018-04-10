/* ************************************************************************
*   File: progress.c                                      EmpireMUD 2.0b5 *
*  Usage: loading, saving, OLC, and functions for empire progression      *
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
*   Empire Helpers
*   Empire Trackers
*   Utilities
*   Database
*   OLC Handlers
*   Displays
*   OLC Modules
*/

// local data
const char *default_progress_name = "Unnamed Goal";

// external consts
extern const char *progress_flags[];
extern const char *progress_perk_types[];
extern const char *progress_types[];
extern const char *techs[];

// external funcs
extern struct req_data *copy_requirements(struct req_data *from);
extern int count_owned_buildings(empire_data *emp, bld_vnum vnum);
extern int count_owned_vehicles(empire_data *emp, any_vnum vnum);
void count_quest_tasks(struct req_data *list, int *complete, int *total);
void get_requirement_display(struct req_data *list, char *save_buffer);
void olc_process_requirements(char_data *ch, char *argument, struct req_data **list, char *command, bool allow_tracker_types);

// local funcs
void apply_progress_to_empire(empire_data *emp, progress_data *prg, bool add);
void complete_goal(empire_data *emp, struct empire_goal *goal);
bool empire_meets_goal_prereqs(empire_data *emp, progress_data *prg);
void purchase_goal(empire_data *emp, progress_data *prg, char_data *purchased_by);
void refresh_empire_goals(empire_data *emp, any_vnum only_vnum);
void refresh_one_goal_tracker(empire_data *emp, struct empire_goal *goal);
void remove_completed_goal(empire_data *emp, any_vnum vnum);
struct empire_goal *start_empire_goal(empire_data *emp, progress_data *prg);


 //////////////////////////////////////////////////////////////////////////////
//// HELPERS /////////////////////////////////////////////////////////////////

/**
* Counts how many items an empire has in storage which match a component flag.
*
* @param empire_data *emp The empire.
* @param int type The CMP_ type.
* @param bitvector_t flags The CMPF_ flags (optional; all flags must be present to match).
* @return int The number of matching items.
*/
int count_empire_components(empire_data *emp, int type, bitvector_t flags) {
	struct empire_storage_data *store, *next_store;
	struct empire_island *isle, *next_isle;
	obj_data *proto;
	int count = 0;
	
	if (!emp) {
		return 0;	// safety first
	}
	
	HASH_ITER(hh, EMPIRE_ISLANDS(emp), isle, next_isle) {
		HASH_ITER(hh, isle->store, store, next_store) {
			if (store->amount < 1 || !(proto = obj_proto(store->vnum))) {
				continue;
			}
			
			if (GET_OBJ_CMP_TYPE(proto) != type) {
				continue;
			}
			if ((GET_OBJ_CMP_FLAGS(proto) & flags) != flags) {
				continue;
			}
			
			// safe!
			SAFE_ADD(count, store->amount, 0, INT_MAX, FALSE);
		}
	}
	
	return count;
}


/**
* Counts how many items an empire has in storage.
*
* @param empire_data *emp The empire.
* @param obj_vnum vnum The vnum of the item to look for.
* @return int The number of matching items.
*/
int count_empire_objects(empire_data *emp, obj_vnum vnum) {
	struct empire_storage_data *store, *next_store;
	struct empire_island *isle, *next_isle;
	int count = 0;
	
	if (!emp) {
		return 0;	// safety first
	}
	
	HASH_ITER(hh, EMPIRE_ISLANDS(emp), isle, next_isle) {
		HASH_ITER(hh, isle->store, store, next_store) {
			if (store->vnum == vnum) {
				SAFE_ADD(count, store->amount, 0, INT_MAX, FALSE);
			}
		}
	}
	
	return count;
}


/**
* Finds a goal the empire is currently on. This allows multi-word abbrevs, and
* prefers exact matches.
*
* @param empire_data *emp The empire to check.
* @param char *name The name to look for.
*/
progress_data *find_current_progress_goal_by_name(empire_data *emp, char *name) {
	struct empire_goal *goal, *next_goal;
	progress_data *prg, *partial = NULL;
	
	if (!emp || !*name) {
		return NULL;
	}
	
	HASH_ITER(hh, EMPIRE_GOALS(emp), goal, next_goal) {
		if (!(prg = real_progress(goal->vnum))) {
			continue;
		}
		
		if (!str_cmp(name, PRG_NAME(prg))) {
			return prg;	// exact match
		}
		else if (!partial && is_multiword_abbrev(name, PRG_NAME(prg))) {
			partial = prg;
		}
	}
	
	return partial;	// if any
}


/**
* Finds a goal from the list. This allows multi-word abbrevs, and  prefers
* exact matches.
*
* @param char *name The name to look for.
*/
progress_data *find_progress_goal_by_name(char *name) {
	progress_data *prg, *next_prg, *partial = NULL;
	
	if (!*name) {
		return NULL;
	}
	
	HASH_ITER(sorted_hh, sorted_progress, prg, next_prg) {
		if (!str_cmp(name, PRG_NAME(prg))) {
			return prg;	// exact match
		}
		else if (!partial && is_multiword_abbrev(name, PRG_NAME(prg))) {
			partial = prg;
		}
	}
	
	return partial;	// if any
}


/**
* Quick way to turn a vnum into a name, safely.
*
* @param any_vnum vnum The progression vnum to look up.
* @return char* A name for the vnum, or "UNKNOWN".
*/
char *get_progress_name_by_proto(any_vnum vnum) {
	progress_data *proto = real_progress(vnum);
	char *unk = "UNKNOWN";
	
	return proto ? PRG_NAME(proto) : unk;
}


/**
* Gets the display for a progress list.
*
* @param struct progress_list *list Pointer to the start of a list.
* @param char *save_buffer A buffer to store the result to.
*/
void get_progress_list_display(struct progress_list *list, char *save_buffer) {
	struct progress_list *item;
	int count = 0;
	
	*save_buffer = '\0';
	LL_FOREACH(list, item) {
		sprintf(save_buffer + strlen(save_buffer), "%2d. [%d] %s\r\n", ++count, item->vnum, get_progress_name_by_proto(item->vnum));
	}
	
	// empty list not shown
}


/**
* The display for a single perk.
*
* @param struct progress_perk *perk The perk to get display text for.
*/
char *get_one_perk_display(struct progress_perk *perk) {
	static char save_buffer[MAX_STRING_LENGTH];
	
	*save_buffer = '\0';
	
	// PRG_PERK_x: displays for each type
	switch (perk->type) {
		case PRG_PERK_TECH: {
			sprinttype(perk->value, techs, save_buffer);
			break;
		}
		default: {
			strcpy(save_buffer, "UNKNOWN");
			break;
		}
	}
	
	return save_buffer;
}


/**
* Gets the display for perks on a progress goal.
*
* @param struct progress_perk *list Pointer to the start of a list of perks.
* @param char *save_buffer A buffer to store the result to.
*/
void get_progress_perks_display(struct progress_perk *list, char *save_buffer) {
	struct progress_perk *item;
	int count = 0;
	
	*save_buffer = '\0';
	LL_FOREACH(list, item) {
		sprintf(save_buffer + strlen(save_buffer), "%2d. %s\r\n", ++count, get_one_perk_display(item));
	}
	
	// empty list not shown
}


 //////////////////////////////////////////////////////////////////////////////
//// EMPIRE HELPERS //////////////////////////////////////////////////////////

/**
* Marks one goal completed at the current timestamp.
*
* @param empire_data *emp The empire completing the goal.
* @param any_vnum vnum The progression goal they completed.
*/
void add_completed_goal(empire_data *emp, any_vnum vnum) {
	struct empire_completed_goal *ecg;
	progress_data *prg;
	
	if (!emp || vnum == NOTHING) {
		return;
	}
	
	HASH_FIND_INT(EMPIRE_COMPLETED_GOALS(emp), &vnum, ecg);
	if (!ecg) {
		CREATE(ecg, struct empire_completed_goal, 1);
		ecg->vnum = vnum;
		HASH_ADD_INT(EMPIRE_COMPLETED_GOALS(emp), vnum, ecg);
		
		if ((prg = real_progress(vnum))) {
			apply_progress_to_empire(emp, prg, TRUE);
		}
	}
	ecg->when = time(0);
	EMPIRE_NEEDS_SAVE(emp) = TRUE;
}


/**
* Applies (or removes) a progress goal to an empire. This does NOT validate
* whether or not the empire has it.
*
* This is called by:
* - complete_goal
* - purchase_goal
* - script_reward_goal
*
* @param empire_data *emp The empire apply it to.
* @param progress_data *prg Which goal is being aplied (or removed).
* @param bool add If TRUE, applies it. If FALSE, reverses it.
*/
void apply_progress_to_empire(empire_data *emp, progress_data *prg, bool add) {
	struct progress_perk *perk;
	
	if (!emp || !prg) {
		return;	// sanitation
	}
	
	// apply points/cost
	if (PRG_VALUE(prg)) {
		SAFE_ADD(EMPIRE_PROGRESS_POINTS(emp, PRG_TYPE(prg)), (add ? PRG_VALUE(prg) : -PRG_VALUE(prg)), INT_MIN, INT_MAX, FALSE);
		SAFE_ADD(EMPIRE_ATTRIBUTE(emp, EATT_PROGRESS_POOL), (add ? PRG_VALUE(prg) : -PRG_VALUE(prg)), INT_MIN, INT_MAX, FALSE);
	}
	if (PRG_COST(prg)) {
		SAFE_ADD(EMPIRE_ATTRIBUTE(emp, EATT_PROGRESS_POOL), (add ? -PRG_COST(prg) : PRG_COST(prg)), INT_MIN, INT_MAX, FALSE);
	}
	
	// PRG_PERK_x: apply perks
	LL_FOREACH(PRG_PERKS(prg), perk) {
		switch (perk->type) {
			case PRG_PERK_TECH: {
				if (perk->value >= 0 && perk->value < NUM_TECHS) {
					SAFE_ADD(EMPIRE_TECH(emp, perk->value), (add ? 1 : -1), 0, INT_MAX, TRUE);
				}
				break;
			}
		}
	}
	
	EMPIRE_NEEDS_SAVE(emp) = TRUE;
}


/**
* Removes, then frees, one goal from an empire.
*
* @param empire_data *emp The empire whose goal it is.
* @param struct empire_goal *goal The goal to cancel/free.
*/
void cancel_empire_goal(empire_data *emp, struct empire_goal *goal) {
	if (!emp || !goal) {
		return;
	}
	
	HASH_DEL(EMPIRE_GOALS(emp), goal);
	free_requirements(goal->tracker);
	free(goal);
	
	EMPIRE_NEEDS_SAVE(emp) = TRUE;
}


/**
* Looks for any goals the empire is now eligible for. If it finds one, it adds
* it, updates the tracker, and will achieve it immediately if it's finished.
*
* @param empire_data *emp Which empire to check.
*/
void check_for_eligible_goals(empire_data *emp) {
	progress_data *prg, *next_prg;
	struct empire_goal *goal;
	int complete, total;
	any_vnum vnum;
	
	if (!emp) {
		return;
	}
	
	// check all goals
	HASH_ITER(hh, progress_table, prg, next_prg) {
		vnum = PRG_VNUM(prg);
		
		if (PRG_FLAGGED(prg, PRG_IN_DEVELOPMENT | PRG_PURCHASABLE | PRG_SCRIPT_ONLY)) {
			continue;
		}
		if (get_current_goal(emp, vnum)) {
			continue;	// already on it
		}
		if (empire_has_completed_goal(emp, PRG_VNUM(prg))) {
			continue;	// already completed it
		}
		if (!empire_meets_goal_prereqs(emp, prg)) {
			continue;	// not eligible
		}
		
		// MADE IT!
		if ((goal = start_empire_goal(emp, prg))) {
			refresh_one_goal_tracker(emp, goal);
			
			// check if complete
			count_quest_tasks(goal->tracker, &complete, &total);
			if (complete == total) {
				complete_goal(emp, goal);
			}
		}
	}
}


/**
* Checks all empires for complete goals.
*/
void check_goals_complete(void) {
	if (check_completed_goals) {
		struct empire_goal *goal, *next_goal;
		empire_data *emp, *next_emp;
		int complete, total;
		
		HASH_ITER(hh, empire_table, emp, next_emp) {
			if (EMPIRE_CHECK_GOAL_COMPLETE(emp)) {
				EMPIRE_CHECK_GOAL_COMPLETE(emp) = FALSE;
				
				HASH_ITER(hh, EMPIRE_GOALS(emp), goal, next_goal) {
					count_quest_tasks(goal->tracker, &complete, &total);
					if (complete == total) {
						complete_goal(emp, goal);
					}
				}
			}
		}
		
		check_completed_goals = FALSE;
	}
}


/**
* Does a full refresh on all empires' goals and updates them all.
*/
void check_progress_refresh(void) {
	if (need_progress_refresh) {
		empire_data *emp, *next_emp;
		
		HASH_ITER(hh, empire_table, emp, next_emp) {
			refresh_empire_goals(emp, NOTHING);
		}
		
		need_progress_refresh = FALSE;
	}
}


/**
* This function changes an in-progress goal to a completed one. It does not
* validate that the goal is complete.
* 
* @param empire_data *emp The empire who has completed the goal.
* @param struct empire_goal *goal The goal being completed (will be removed/freed).
*/
void complete_goal(empire_data *emp, struct empire_goal *goal) {
	progress_data *prg;
	
	if (!emp || !goal) {
		return;	// somehow
	}
	if (!(prg = real_progress(goal->vnum))) {
		cancel_empire_goal(emp, goal);
		return;	// no data
	}
	
	log_to_empire(emp, ELOG_PROGRESS, "Achieved: %s", PRG_NAME(prg));
	
	add_completed_goal(emp, goal->vnum);
	cancel_empire_goal(emp, goal);
	
	check_for_eligible_goals(emp);
}


/**
* Determines if an empire has already completed a goal and returns the time-
* stamp if so.
*
* @param empire_data *emp Which empire.
* @param any_vnum vnum Which progression goal.
* @return time_t timestamp if they have completed it, 0 (FALSE) if not.
*/
time_t empire_has_completed_goal(empire_data *emp, any_vnum vnum) {
	struct empire_completed_goal *ecg;
	
	if (emp && vnum != NOTHING) {
		HASH_FIND_INT(EMPIRE_COMPLETED_GOALS(emp), &vnum, ecg);
		return ecg ? ecg->when : 0;
	}
	return 0;
}


/**
* Checks if an empire has completed all the prereqs to qualify for a goal.
*
* @param empire_data *emp The empire to check.
* @param progress_data *prg The progression goal to validate.
* @return bool TRUE if the empire qualifies, FALSE if not.
*/
bool empire_meets_goal_prereqs(empire_data *emp, progress_data *prg) {
	struct progress_list *iter;
	
	if (!emp || !prg) {
		return FALSE;
	}
	
	LL_FOREACH(PRG_PREREQS(prg), iter) {
		if (!empire_has_completed_goal(emp, iter->vnum)) {
			return FALSE;
		}
	}
	
	return TRUE;
}


/**
* Frees a hash of empire goal entries.
*
* struct empire_goal *hash The set to free.
*/
void free_empire_goals(struct empire_goal *hash) {
	struct empire_goal *eg, *next;
	HASH_ITER(hh, hash, eg, next) {
		free_requirements(eg->tracker);
		free(eg);
	}
}


/**
* Frees a hash of completed empire goals.
* 
* struct empire_completed_goal *hash The set to free.
*/
void free_empire_completed_goals(struct empire_completed_goal *hash) {
	struct empire_completed_goal *ecg, *next;
	HASH_ITER(hh, hash, ecg, next) {
		free(ecg);
	}
}


/**
* Finds the entry for a goal the empire is currently on, if they are on it.
*
* @param empire_data *emp Which empire.
* @param any_vnum vnum The goal to look for.
* @return struct empire_goal* The empire's goal entry, if they are on it. NULL if not.
*/
struct empire_goal *get_current_goal(empire_data *emp, any_vnum vnum) {
	struct empire_goal *goal = NULL;
	if (emp) {
		HASH_FIND_INT(EMPIRE_GOALS(emp), &vnum, goal);
	}
	return goal;	// if any
}


/**
* Call this function to buy a goal with the PURCHASABLE flag. This function
* does not validate prereqs or point availability. It only does the work.
*
* @param empire_data *emp Which empire is purchasing it.
* @param progress_data *prg The progression goal being purchased.
* @param char_data *purchased_by Optional: The person doing the purchasing (for the elog).
*/
void purchase_goal(empire_data *emp, progress_data *prg, char_data *purchased_by) {
	if (!emp || !prg) {
		return;	// nothing to do
	}
	if (!PRG_FLAGGED(prg, PRG_PURCHASABLE)) {
		log("SYSERR: purchase_goal called for non-purchasable goal %d", PRG_VNUM(prg));
		return;
	}
	
	if (purchased_by) {
		log_to_empire(emp, ELOG_PROGRESS, "%s purchased: %s", PERS(purchased_by, purchased_by, TRUE), PRG_NAME(prg));
	}
	else {
		log_to_empire(emp, ELOG_PROGRESS, "Purchased: %s", PRG_NAME(prg));
	}
	
	add_completed_goal(emp, PRG_VNUM(prg));
	check_for_eligible_goals(emp);
}


/**
* Refreshes the goals and checks progress on them, for 1 empire.
*
* @param empire_data *emp The empire to refresh.
* @param any_vnum only_vnum Optional: If provided, only checks and refreshes 1 goal (NOTHING = all vnums).
*/
void refresh_empire_goals(empire_data *emp, any_vnum only_vnum) {
	progress_data *prg, *next_prg;
	struct empire_goal *goal;
	int complete, total;
	any_vnum vnum;
	bool skip;
	
	if (!emp) {
		return;
	}
	
	// check all goals
	HASH_ITER(hh, progress_table, prg, next_prg) {
		vnum = PRG_VNUM(prg);
		if (only_vnum != NOTHING && only_vnum != vnum) {
			continue;
		}
		
		goal = get_current_goal(emp, vnum);	// maybe -- if they have the goal already
		skip = FALSE;
		
		// remove if not allowed to track it
		if (PRG_FLAGGED(prg, PRG_IN_DEVELOPMENT | PRG_PURCHASABLE | PRG_SCRIPT_ONLY)) {
			if (goal) {
				cancel_empire_goal(emp, goal);
				goal = NULL;
			}
			skip = TRUE;
		}
		
		// remove from completed if in-dev
		if (PRG_FLAGGED(prg, PRG_IN_DEVELOPMENT) && empire_has_completed_goal(emp, PRG_VNUM(prg))) {
			remove_completed_goal(emp, PRG_VNUM(prg));
			skip = TRUE;
		}
		
		// remove if no longer meets the pre-reqs
		if (goal && !empire_meets_goal_prereqs(emp, prg)) {
			cancel_empire_goal(emp, goal);
			goal = NULL;
			skip = TRUE;
		}
		else if (empire_has_completed_goal(emp, PRG_VNUM(prg)) && !empire_meets_goal_prereqs(emp, prg)) {
			remove_completed_goal(emp, PRG_VNUM(prg));
			skip = TRUE;
		}
		
		if (skip) {
			// do not attempt to add the goal if we marked it skip
			continue;
		}
		
		// check if they SHOULD be on it
		if (!goal && !empire_has_completed_goal(emp, PRG_VNUM(prg)) && empire_meets_goal_prereqs(emp, prg)) {
			goal = start_empire_goal(emp, prg);
		}
		// check if they SHOULDN'T
		if (goal && empire_has_completed_goal(emp, PRG_VNUM(prg))) {
			cancel_empire_goal(emp, goal);
			goal = NULL;
		}
		
		// check version
		if (goal && goal->version != PRG_VERSION(prg)) {
			goal->version = PRG_VERSION(prg);
			free_requirements(goal->tracker);
			goal->tracker = copy_requirements(PRG_TASKS(prg));
		}
		
		if (goal) {	// check if the goal is now complete
			refresh_one_goal_tracker(emp, goal);
			
			// check if complete
			count_quest_tasks(goal->tracker, &complete, &total);
			if (complete == total) {
				complete_goal(emp, goal);
			}
		}
	}
}


/**
* Re-counts a goal tracker for the empire. Not all task types are refreshable
* (e.g. mob kills) and those are not affected by this function.
*
* @param empire_data *emp The empire whose tracker it is.
* @param struct empire_goal *goal The goal to refresh.
*/
void refresh_one_goal_tracker(empire_data *emp, struct empire_goal *goal) {
	struct req_data *task;
	
	if (!emp || !goal) {
		return;	// safety first
	}
	
	LL_FOREACH(goal->tracker, task) {
		// REQ_x: refreshable/empire types only
		switch (task->type) {
			case REQ_GET_COMPONENT: {
				task->current = count_empire_components(emp, task->vnum, task->misc);
				break;
			}
			case REQ_GET_OBJECT: {
				task->current = count_empire_objects(emp, task->vnum);
				break;
			}
			case REQ_OWN_BUILDING: {
				task->current = count_owned_buildings(emp, task->vnum);
				break;
			}
			case REQ_OWN_VEHICLE: {
				task->current = count_owned_vehicles(emp, task->vnum);
				break;
			}
			case REQ_GET_COINS: {
				task->current = EMPIRE_COINS(emp);
				break;
			}
			
			// otherwise...
			default: {
				// this type is impossible for empires, so we will always count it as done
				task->current = task->needed;
				break;
			}
		}
	}
	
	// check it
	TRIGGER_CHECK_GOAL_COMPLETE(emp);
}


/**
* Removes completion of a goal, if present.
*
* @param empire_data *emp The empire un-completing the goal.
* @param any_vnum vnum The progression goal to lose.
*/
void remove_completed_goal(empire_data *emp, any_vnum vnum) {
	struct empire_completed_goal *ecg;
	progress_data *prg;
	
	HASH_FIND_INT(EMPIRE_COMPLETED_GOALS(emp), &vnum, ecg);
	if (ecg) {
		if ((prg = real_progress(vnum))) {
			apply_progress_to_empire(emp, prg, FALSE);	// strip perks
		}
		HASH_DEL(EMPIRE_COMPLETED_GOALS(emp), ecg);
		free(ecg);
		EMPIRE_NEEDS_SAVE(emp) = TRUE;
	}
}


/**
* Call this function to reward an empire with a SCRIPT-ONLY progress goal. This
* function does not validate prereqs or point availability. It only does the
* work.
*
* @param empire_data *emp Which empire is being rewarded.
* @param progress_data *prg The progression goal being added.
*/
void script_reward_goal(empire_data *emp, progress_data *prg) {
	if (!emp || !prg) {
		return;	// nothing to do
	}
	
	log_to_empire(emp, ELOG_PROGRESS, "Achieved: %s", PRG_NAME(prg));
	
	add_completed_goal(emp, PRG_VNUM(prg));
	check_for_eligible_goals(emp);
}


/**
* Starts a goal for an empire, and creates a tracker for it.
*
* @param empire_data *emp Which empire to add the goal to.
* @param progress_data *prg The data for the goal.
* @return struct empire_goal* The goal entry (may be NULL if bad input is passed).
*/
struct empire_goal *start_empire_goal(empire_data *emp, progress_data *prg) {
	struct empire_goal *goal;
	
	if (!emp || !prg) {
		return NULL;	// basic sanitation
	}
	if ((goal = get_current_goal(emp, PRG_VNUM(prg)))) {
		return goal;	// already on it
	}
	
	CREATE(goal, struct empire_goal, 1);
	goal->vnum = PRG_VNUM(prg);
	goal->version = PRG_VERSION(prg);
	goal->tracker = copy_requirements(PRG_TASKS(prg));
	
	HASH_ADD_INT(EMPIRE_GOALS(emp), vnum, goal);
	refresh_one_goal_tracker(emp, goal);
	EMPIRE_NEEDS_SAVE(emp) = TRUE;
	
	return goal;
}


/**
* Runs at startup to ensure no goals were deleted while the mud was down.
*/
void verify_empire_goals(void) {
	empire_data *emp, *next_emp;
	
	struct empire_completed_goal *ecg, *next_ecg;
	struct empire_goal *goal, *next_goal;
	
	HASH_ITER(hh, empire_table, emp, next_emp) {
		HASH_ITER(hh, EMPIRE_GOALS(emp), goal, next_goal) {
			if (!real_progress(goal->vnum)) {
				cancel_empire_goal(emp, goal);
			}
		}
		HASH_ITER(hh, EMPIRE_COMPLETED_GOALS(emp), ecg, next_ecg) {
			if (!real_progress(ecg->vnum)) {
				HASH_DEL(EMPIRE_COMPLETED_GOALS(emp), ecg);
				free(ecg);
			}
		}
	}
}


 //////////////////////////////////////////////////////////////////////////////
//// EMPIRE TRACKERS /////////////////////////////////////////////////////////

/**
* Empire Tracker: empire gains/loses coins
*
* @param empire_data *emp The empire.
* @param int amount How many coins gained/lost
*/
void et_change_coins(empire_data *emp, int amount) {
	struct empire_goal *goal, *next_goal;
	struct req_data *task;
	
	HASH_ITER(hh, EMPIRE_GOALS(emp), goal, next_goal) {
		LL_FOREACH(goal->tracker, task) {
			if (task->type == REQ_GET_COINS) {
				SAFE_ADD(task->current, amount, 0, INT_MAX, TRUE);
				EMPIRE_NEEDS_SAVE(emp) = TRUE;
				TRIGGER_CHECK_GOAL_COMPLETE(emp);
			}
		}
	}
}


/**
* Empire Tracker: empire gets a building
*
* @param empire_data *emp The empire.
* @param any_vnum vnum The building vnum.
*/
void et_gain_building(empire_data *emp, any_vnum vnum) {
	struct empire_goal *goal, *next_goal;
	struct req_data *task;
	
	HASH_ITER(hh, EMPIRE_GOALS(emp), goal, next_goal) {
		LL_FOREACH(goal->tracker, task) {
			if (task->type == REQ_OWN_BUILDING && task->vnum == vnum) {
				++task->current;
				EMPIRE_NEEDS_SAVE(emp) = TRUE;
				TRIGGER_CHECK_GOAL_COMPLETE(emp);
			}
		}
	}
}


/**
* Empire Tracker: empire gets a vehicle
*
* @param empire_data *emp The empire.
* @param any_vnum vnum The vehicle vnum.
*/
void et_gain_vehicle(empire_data *emp, any_vnum vnum) {
	struct empire_goal *goal, *next_goal;
	struct req_data *task;
	
	HASH_ITER(hh, EMPIRE_GOALS(emp), goal, next_goal) {
		LL_FOREACH(goal->tracker, task) {
			if (task->type == REQ_OWN_VEHICLE && task->vnum == vnum) {
				++task->current;
				EMPIRE_NEEDS_SAVE(emp) = TRUE;
				TRIGGER_CHECK_GOAL_COMPLETE(emp);
			}
		}
	}
}


/**
* Empire Tracker: empire obtains/loses items
*
* @param empire_data *emp The empire.
* @param obj_data *obj The item.
* @param int amount How many of it (may be positive or negative).
*/
void et_get_obj(empire_data *emp, obj_data *obj, int amount) {
	struct empire_goal *goal, *next;
	struct req_data *task;
	
	HASH_ITER(hh, EMPIRE_GOALS(emp), goal, next) {
		LL_FOREACH(goal->tracker, task) {
			if (task->type == REQ_GET_COMPONENT && GET_OBJ_CMP_TYPE(obj) == task->vnum && (GET_OBJ_CMP_FLAGS(obj) & task->misc) == task->misc) {
				SAFE_ADD(task->current, amount, 0, INT_MAX, TRUE);
				EMPIRE_NEEDS_SAVE(emp) = TRUE;
				TRIGGER_CHECK_GOAL_COMPLETE(emp);
			}
			else if (task->type == REQ_GET_OBJECT && GET_OBJ_VNUM(obj) == task->vnum) {
				SAFE_ADD(task->current, amount, 0, INT_MAX, TRUE);
				EMPIRE_NEEDS_SAVE(emp) = TRUE;
				TRIGGER_CHECK_GOAL_COMPLETE(emp);
			}
			else if (task->type == REQ_WEARING_OR_HAS && GET_OBJ_VNUM(obj) == task->vnum) {
				SAFE_ADD(task->current, amount, 0, INT_MAX, TRUE);
				EMPIRE_NEEDS_SAVE(emp) = TRUE;
				TRIGGER_CHECK_GOAL_COMPLETE(emp);
			}
		}
	}
}


/**
* Empire Tracker: empire loses/dismantles a building
*
* @param empire_data *emp The empire.
* @param any_vnum vnum The building vnum.
*/
void et_lose_building(empire_data *emp, any_vnum vnum) {
	struct empire_goal *goal, *next_goal;
	struct req_data *task;
	
	HASH_ITER(hh, EMPIRE_GOALS(emp), goal, next_goal) {
		LL_FOREACH(goal->tracker, task) {
			if (task->type == REQ_OWN_BUILDING && task->vnum == vnum) {
				--task->current;
				EMPIRE_NEEDS_SAVE(emp) = TRUE;
				TRIGGER_CHECK_GOAL_COMPLETE(emp);
				
				// check min
				task->current = MAX(task->current, 0);
			}
		}
	}
}


/**

* Empire Tracker: empire gets a building
*
* @param empire_data *emp The empire.
* @param any_vnum vnum The vehicle vnum.
*/
void et_lose_vehicle(empire_data *emp, any_vnum vnum) {
	struct empire_goal *goal, *next_goal;
	struct req_data *task;
	
	HASH_ITER(hh, EMPIRE_GOALS(emp), goal, next_goal) {
		LL_FOREACH(goal->tracker, task) {
			if (task->type == REQ_OWN_VEHICLE && task->vnum == vnum) {
				--task->current;
				EMPIRE_NEEDS_SAVE(emp) = TRUE;
				TRIGGER_CHECK_GOAL_COMPLETE(emp);
				
				// check min
				task->current = MAX(task->current, 0);
			}
		}
	}
}


 //////////////////////////////////////////////////////////////////////////////
//// UTILITIES ///////////////////////////////////////////////////////////////

/**
* Checks for common progression problems and reports them to ch.
*
* @param progress_data *prg The item to audit.
* @param char_data *ch The person to report to.
* @return bool TRUE if any problems were reported; FALSE if all good.
*/
bool audit_progress(progress_data *prg, char_data *ch) {
	struct progress_list *iter, *sub;
	progress_data *other;
	bool problem = FALSE;
	
	if (PRG_FLAGGED(prg, PRG_IN_DEVELOPMENT)) {
		olc_audit_msg(ch, PRG_VNUM(prg), "IN-DEVELOPMENT");
		problem = TRUE;
	}
	
	if (!PRG_NAME(prg) || !*PRG_NAME(prg) || !str_cmp(PRG_NAME(prg), default_progress_name)) {
		olc_audit_msg(ch, PRG_VNUM(prg), "No name set");
		problem = TRUE;
	}
	if (ispunct(*(PRG_NAME(prg) + strlen(PRG_NAME(prg)) - 1))) {
		olc_audit_msg(ch, PRG_VNUM(prg), "Name ends with punctuation");
		problem = TRUE;
	}
	
	if (!PRG_DESCRIPTION(prg) || !*PRG_DESCRIPTION(prg)) {
		olc_audit_msg(ch, PRG_VNUM(prg), "No description set");
		problem = TRUE;
	}
	
	if (PRG_FLAGGED(prg, PRG_PURCHASABLE) && (PRG_VALUE(prg) || PRG_TASKS(prg))) {
		olc_audit_msg(ch, PRG_VNUM(prg), "PURCHASABLE set with value and/or tasks");
		problem = TRUE;
	}
	if (PRG_FLAGGED(prg, PRG_PURCHASABLE) && !PRG_COST(prg)) {
		olc_audit_msg(ch, PRG_VNUM(prg), "PURCHASABLE set with zero cost");
		problem = TRUE;
	}
	if (PRG_COST(prg) && PRG_VALUE(prg)) {
		olc_audit_msg(ch, PRG_VNUM(prg), "Both cost and value set");
		problem = TRUE;
	}
	
	if (PRG_FLAGGED(prg, PRG_SCRIPT_ONLY) && PRG_FLAGGED(prg, PRG_PURCHASABLE)) {
		olc_audit_msg(ch, PRG_VNUM(prg), "PURCHASABLE set with SCRIPT-ONLY");
		problem = TRUE;
	}
	if (PRG_FLAGGED(prg, PRG_SCRIPT_ONLY) && (PRG_COST(prg) || PRG_TASKS(prg))) {
		olc_audit_msg(ch, PRG_VNUM(prg), "SCRIPT-ONLY set with cost and/or tasks");
		problem = TRUE;
	}
	
	LL_FOREACH(PRG_PREREQS(prg), iter) {
		if (iter->vnum == PRG_VNUM(prg)) {
			olc_audit_msg(ch, PRG_VNUM(prg), "Has self as prerequisite");
			problem = TRUE;
			break;	// only once
		}
		else if ((other = real_progress(iter->vnum))) {
			// check for a back-prereq
			LL_FOREACH(PRG_PREREQS(other), sub) {
				if (sub->vnum == PRG_VNUM(prg)) {
					olc_audit_msg(ch, PRG_VNUM(prg), "Circular prerequisites");
					problem = TRUE;
					break;	// only once
				}
			}
		}
	}
	
	return problem;
}


/**
* For the .list command.
*
* @param progress_data *prg The thing to list.
* @param bool detail If TRUE, provide additional details
* @return char* The line to show (without a CRLF).
*/
char *list_one_progress(progress_data *prg, bool detail) {
	static char output[MAX_STRING_LENGTH];
	
	if (detail) {
		snprintf(output, sizeof(output), "[%5d] %s (%s)%s", PRG_VNUM(prg), PRG_NAME(prg), progress_types[PRG_TYPE(prg)], PRG_FLAGGED(prg, PRG_IN_DEVELOPMENT) ? " (IN-DEV)" : "");
	}
	else {
		snprintf(output, sizeof(output), "[%5d] %s%s", PRG_VNUM(prg), PRG_NAME(prg), PRG_FLAGGED(prg, PRG_IN_DEVELOPMENT) ? " (IN-DEV)" : "");
	}
		
	return output;
}


/**
* Searches for all uses of a progress and displays them.
*
* @param char_data *ch The player.
* @param any_vnum vnum The progress vnum.
*/
void olc_search_progress(char_data *ch, any_vnum vnum) {
	char buf[MAX_STRING_LENGTH];
	progress_data *prg = real_progress(vnum), *iter, *next_iter;
	struct progress_list *pl;
	int size, found;
	
	if (!prg) {
		msg_to_char(ch, "There is no progression entry %d.\r\n", vnum);
		return;
	}
	
	found = 0;
	size = snprintf(buf, sizeof(buf), "Occurrences of progression %d (%s):\r\n", vnum, PRG_NAME(prg));
	
	// other progresses
	HASH_ITER(hh, progress_table, iter, next_iter) {
		LL_FOREACH(PRG_PREREQS(iter), pl) {
			if (pl->vnum == vnum) {
				++found;
				size += snprintf(buf + size, sizeof(buf) - size, "PRG [%5d] %s\r\n", PRG_VNUM(iter), PRG_NAME(iter));
				break;
			}
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
* Copies entries from one list into another, only if they are not already in
* the to_list.
*
* @param struct progress_list **to_list A pointer to the destination list.
* @param struct progress_list *from_list The list to copy from.
*/
void smart_copy_progress_prereqs(struct progress_list **to_list, struct progress_list *from_list) {
	struct progress_list *iter, *search, *ent;
	bool found;
	
	LL_FOREACH(from_list, iter) {
		// ensure not already in list
		found = FALSE;
		LL_FOREACH(*to_list, search) {
			if (search->vnum == iter->vnum) {
				found = TRUE;
				break;
			}
		}
		
		// add it
		if (!found) {
			CREATE(ent, struct progress_list, 1);
			*ent = *iter;
			LL_APPEND(*to_list, ent);
		}
	}
}


/**
* Copies entries from one list into another, only if they are not already in
* the to_list.
*
* @param struct progress_list **to_list A pointer to the destination list.
* @param struct progress_list *from_list The list to copy from.
*/
void smart_copy_progress_perks(struct progress_perk **to_list, struct progress_perk *from_list) {
	struct progress_perk *iter, *search, *ent;
	bool found;
	
	LL_FOREACH(from_list, iter) {
		// ensure not already in list
		found = FALSE;
		LL_FOREACH(*to_list, search) {
			if (search->type == iter->type && search->value == iter->value) {
				found = TRUE;
				break;
			}
		}
		
		// add it
		if (!found) {
			CREATE(ent, struct progress_perk, 1);
			*ent = *iter;
			LL_APPEND(*to_list, ent);
		}
	}
}


// Complex sorter for sorted_progress
int sort_progress_by_data(progress_data *a, progress_data *b) {
	if (PRG_TYPE(a) != PRG_TYPE(b)) {
		return PRG_TYPE(a) - PRG_TYPE(b);
	}
	else if (PRG_FLAGGED(a, PRG_PURCHASABLE) != PRG_FLAGGED(b, PRG_PURCHASABLE)) {
		return PRG_FLAGGED(a, PRG_PURCHASABLE) ? 1 : -1;
	}
	else if (PRG_FLAGGED(a, PRG_SCRIPT_ONLY) != PRG_FLAGGED(b, PRG_SCRIPT_ONLY)) {
		return PRG_FLAGGED(a, PRG_SCRIPT_ONLY) ? 1 : -1;
	}
	else {
		return str_cmp(PRG_NAME(a), PRG_NAME(b));
	}
}


// Simple vnum sorter for the progress hash
int sort_progress(progress_data *a, progress_data *b) {
	return PRG_VNUM(a) - PRG_VNUM(b);
}


 //////////////////////////////////////////////////////////////////////////////
//// DATABASE ////////////////////////////////////////////////////////////////

/**
* Puts a progress entry into the hash table.
*
* @param progress_data *prg The progress data to add to the table.
*/
void add_progress_to_table(progress_data *prg) {
	progress_data *find;
	any_vnum vnum;
	
	if (prg) {
		vnum = PRG_VNUM(prg);
		HASH_FIND_INT(progress_table, &vnum, find);
		if (!find) {
			HASH_ADD_INT(progress_table, vnum, prg);
			HASH_SORT(progress_table, sort_progress);
		}
		
		// sorted table
		HASH_FIND(sorted_hh, sorted_progress, &vnum, sizeof(int), find);
		if (!find) {
			HASH_ADD(sorted_hh, sorted_progress, vnum, sizeof(int), prg);
			HASH_SRT(sorted_hh, sorted_progress, sort_progress_by_data);
		}
	}
}


/**
* Removes a progress from the hash tables.
*
* @param progress_data *prg The progress data to remove from the tables.
*/
void remove_progress_from_table(progress_data *prg) {
	HASH_DEL(progress_table, prg);
	HASH_DELETE(sorted_hh, sorted_progress, prg);
}


/**
* Initializes a new progress entry. This clears all memory for it, so set the
* vnum AFTER.
*
* @param progress_data *prg The progress entry to initialize.
*/
void clear_progress(progress_data *prg) {
	memset((char *) prg, 0, sizeof(progress_data));
	
	PRG_VNUM(prg) = NOTHING;
}


/**
* Duplicates a list of 'struct progress_list'.
*
* @param struct progress_list *input The list to duplicate.
* @return struct progress_list* The copied list.
*/
struct progress_list *copy_progress_list(struct progress_list *input) {
	struct progress_list *iter, *el, *list = NULL;
	
	LL_FOREACH(input, iter) {
		CREATE(el, struct progress_list, 1);
		*el = *iter;
		LL_APPEND(list, el);
	}
	
	return list;
}


/**
* Duplicates a list of 'struct progress_perk'.
*
* @param struct progress_perk *input The list to duplicate.
* @return struct progress_perk* The copied list.
*/
struct progress_perk *copy_progress_perks(struct progress_perk *input) {
	struct progress_perk *iter, *el, *list = NULL;
	
	LL_FOREACH(input, iter) {
		CREATE(el, struct progress_perk, 1);
		*el = *iter;
		LL_APPEND(list, el);
	}
	
	return list;
}


/**
* Frees memory for a list of 'struct progress_list'.
*
* @param struct progress_list *list The linked list to free.
*/
void free_progress_list(struct progress_list *list) {
	struct progress_list *pl;
	while ((pl = list)) {
		list = list->next;
		free(pl);
	}
}


/**
* Frees memory for a list of 'struct progress_perk'.
*
* @param struct progress_perk *list The linked list to free.
*/
void free_progress_perks(struct progress_perk *list) {
	struct progress_perk *pl;
	while ((pl = list)) {
		list = list->next;
		free(pl);
	}
}


/**
* frees up memory for a progress data item.
*
* See also: olc_delete_progress
*
* @param progress_data *prg The progress data to free.
*/
void free_progress(progress_data *prg) {
	progress_data *proto = real_progress(PRG_VNUM(prg));
	
	if (PRG_NAME(prg) && (!proto || PRG_NAME(prg) != PRG_NAME(proto))) {
		free(PRG_NAME(prg));
	}
	if (PRG_DESCRIPTION(prg) && (!proto || PRG_DESCRIPTION(prg) != PRG_DESCRIPTION(proto))) {
		free(PRG_DESCRIPTION(prg));
	}
	if (PRG_PREREQS(prg) && (!proto || PRG_PREREQS(prg) != PRG_PREREQS(proto))) {
		free_progress_list(PRG_PREREQS(prg));
	}
	if (PRG_TASKS(prg) && (!proto || PRG_TASKS(prg) != PRG_TASKS(proto))) {
		free_requirements(PRG_TASKS(prg));
	}
	if (PRG_PERKS(prg) && (!proto || PRG_PERKS(prg) != PRG_PERKS(proto))) {
		free_progress_perks(PRG_PERKS(prg));
	}
	
	free(prg);
}


/**
* Read one progress from file.
*
* @param FILE *fl The open .prg file
* @param any_vnum vnum The progress vnum
*/
void parse_progress(FILE *fl, any_vnum vnum) {
	void parse_requirement(FILE *fl, struct req_data **list, char *error_str);
	
	char line[256], error[256], str_in[256];
	struct progress_perk *perk;
	progress_data *prg, *find;
	struct progress_list *pl;
	int int_in[4];
	
	CREATE(prg, progress_data, 1);
	clear_progress(prg);
	PRG_VNUM(prg) = vnum;
	
	HASH_FIND_INT(progress_table, &vnum, find);
	if (find) {
		log("WARNING: Duplicate progress vnum #%d", vnum);
		// but have to load it anyway to advance the file
	}
	add_progress_to_table(prg);
		
	// for error messages
	sprintf(error, "progress vnum %d", vnum);
	
	// lines 1-2: name, desc
	PRG_NAME(prg) = fread_string(fl, error);
	PRG_DESCRIPTION(prg) = fread_string(fl, error);
	
	// line 3: version type cost value flags
	if (!get_line(fl, line) || sscanf(line, "%d %d %d %d %s", &int_in[0], &int_in[1], &int_in[2], &int_in[3], str_in) != 5) {
		log("SYSERR: Format error in line 3 of %s", error);
		exit(1);
	}
	
	PRG_VERSION(prg) = int_in[0];
	PRG_TYPE(prg) = int_in[1];
	PRG_COST(prg) = int_in[2];
	PRG_VALUE(prg) = int_in[3];
	PRG_FLAGS(prg) = asciiflag_conv(str_in);
	
	// optionals
	for (;;) {
		if (!get_line(fl, line)) {
			log("SYSERR: Format error in %s, expecting alphabetic flags", error);
			exit(1);
		}
		switch (*line) {
			case 'K': {	// perks
				if (sscanf(line, "K %d %d", &int_in[0], &int_in[1]) != 2) {
					log("SYSERR: Format error in K line of %s", error);
					exit(1);
				}
				
				CREATE(perk, struct progress_perk, 1);
				perk->type = int_in[0];
				perk->value = int_in[1];
				LL_APPEND(PRG_PERKS(prg), perk);
				break;
			}
			case 'P': {	// prereqs
				if (sscanf(line, "P %d", &int_in[0]) != 1) {
					log("SYSERR: Format error in P line of %s", error);
					exit(1);
				}
				
				CREATE(pl, struct progress_list, 1);
				pl->vnum = int_in[0];
				LL_APPEND(PRG_PREREQS(prg), pl);
				break;
			}
			case 'W': {	// tasks / work
				parse_requirement(fl, &PRG_TASKS(prg), error);
				break;
			}
			
			// end
			case 'S': {
				return;
			}
			
			default: {
				log("SYSERR: Format error in %s, expecting alphabetic flags", error);
				exit(1);
			}
		}
	}
}


/**
* @param any_vnum vnum Any progress vnum
* @return progress_data* The progress data, or NULL if it doesn't exist
*/
progress_data *real_progress(any_vnum vnum) {
	progress_data *prg;
	
	if (vnum < 0 || vnum == NOTHING) {
		return NULL;
	}
	
	HASH_FIND_INT(progress_table, &vnum, prg);
	return prg;
}


// writes entries in the progress index
void write_progress_index(FILE *fl) {
	progress_data *prg, *next_prg;
	int this, last;
	
	last = -1;
	HASH_ITER(hh, progress_table, prg, next_prg) {
		// determine "zone number" by vnum
		this = (int)(PRG_VNUM(prg) / 100);
	
		if (this != last) {
			fprintf(fl, "%d%s\n", this, PRG_SUFFIX);
			last = this;
		}
	}
}


/**
* Outputs one progress item in the db file format, starting with a #VNUM and
* ending with an S.
*
* @param FILE *fl The file to write it to.
* @param progress_data *prg The thing to save.
*/
void write_progress_to_file(FILE *fl, progress_data *prg) {
	void write_requirements_to_file(FILE *fl, char letter, struct req_data *list);
	
	char temp[MAX_STRING_LENGTH];
	struct progress_perk *perk;
	struct progress_list *pl;
	
	if (!fl || !prg) {
		syslog(SYS_ERROR, LVL_START_IMM, TRUE, "SYSERR: write_progress_to_file called without %s", !fl ? "file" : "prg");
		return;
	}
	
	fprintf(fl, "#%d\n", PRG_VNUM(prg));
	
	// 1. name
	fprintf(fl, "%s~\n", NULLSAFE(PRG_NAME(prg)));
	
	// 2. desc
	strcpy(temp, NULLSAFE(PRG_DESCRIPTION(prg)));
	strip_crlf(temp);
	fprintf(fl, "%s~\n", temp);
	
	// 3. version type cost value flags
	fprintf(fl, "%d %d %d %d %s\n", PRG_VERSION(prg), PRG_TYPE(prg), PRG_COST(prg), PRG_VALUE(prg), bitv_to_alpha(PRG_FLAGS(prg)));
	
	// K. perks
	LL_FOREACH(PRG_PERKS(prg), perk) {
		fprintf(fl, "K %d %d\n", perk->type, perk->value);
	}
	
	// P: prereqs
	LL_FOREACH(PRG_PREREQS(prg), pl) {
		fprintf(fl, "P %d\n", pl->vnum);
	}
	
	// W. tasks (work)
	write_requirements_to_file(fl, 'W', PRG_TASKS(prg));
	
	// end
	fprintf(fl, "S\n");
}


 //////////////////////////////////////////////////////////////////////////////
//// OLC HANDLERS ////////////////////////////////////////////////////////////

/**
* Creates a new progress entry.
* 
* @param any_vnum vnum The number to create.
* @return progress_data* The new progress entry's prototype.
*/
progress_data *create_progress_table_entry(any_vnum vnum) {
	progress_data *prg;
	
	// sanity
	if (real_progress(vnum)) {
		log("SYSERR: Attempting to insert progress at existing vnum %d", vnum);
		return real_progress(vnum);
	}
	
	CREATE(prg, progress_data, 1);
	clear_progress(prg);
	PRG_VNUM(prg) = vnum;
	PRG_NAME(prg) = str_dup(default_progress_name);
	add_progress_to_table(prg);

	// save index and progress file now
	save_index(DB_BOOT_PRG);
	save_library_file_for_vnum(DB_BOOT_PRG, vnum);

	return prg;
}


/**
* WARNING: This function actually deletes a progress entry.
*
* @param char_data *ch The person doing the deleting.
* @param any_vnum vnum The vnum to delete.
*/
void olc_delete_progress(char_data *ch, any_vnum vnum) {
	progress_data *prg, *iter, *next_iter;
	struct progress_list *pl, *next_pl;
	empire_data *emp, *next_emp;
	struct empire_goal *goal;
	descriptor_data *desc;
	bool any;
	
	if (!(prg = real_progress(vnum))) {
		msg_to_char(ch, "There is no such progress entry %d.\r\n", vnum);
		return;
	}
	
	// removing live instances
	if (!PRG_FLAGGED(prg, PRG_IN_DEVELOPMENT)) {
		HASH_ITER(hh, empire_table, emp, next_emp) {
			if ((goal = get_current_goal(emp, vnum))) {
				cancel_empire_goal(emp, goal);
			}
			
			remove_completed_goal(emp, vnum);
		}
	}
	
	// remove it from the hash table first
	remove_progress_from_table(prg);

	// save index and progress file now
	save_index(DB_BOOT_PRG);
	save_library_file_for_vnum(DB_BOOT_PRG, vnum);
	
	// removing from prototypes goes here:
	
	// other progresses
	HASH_ITER(hh, progress_table, iter, next_iter) {
		any = FALSE;
		LL_FOREACH_SAFE(PRG_PREREQS(iter), pl, next_pl) {
			if (pl->vnum == vnum) {
				LL_DELETE(PRG_PREREQS(iter), pl);
				free(pl);
				any = TRUE;
			}
		}
		
		if (any) {
			SET_BIT(PRG_FLAGS(iter), PRG_IN_DEVELOPMENT);
			save_library_file_for_vnum(DB_BOOT_PRG, PRG_VNUM(iter));
			need_progress_refresh = TRUE;
		}
	}
	
	// remove from from active editors
	LL_FOREACH(descriptor_list, desc) {
		if (GET_OLC_PROGRESS(desc)) {
			any = FALSE;
			LL_FOREACH_SAFE(PRG_PREREQS(GET_OLC_PROGRESS(desc)), pl, next_pl) {
				if (pl->vnum == vnum) {
					LL_DELETE(PRG_PREREQS(GET_OLC_PROGRESS(desc)), pl);
					free(pl);
					any = TRUE;
				}
			}
		
			if (any) {
				SET_BIT(PRG_FLAGS(GET_OLC_PROGRESS(desc)), PRG_IN_DEVELOPMENT);
				msg_to_desc(desc, "A progression goal used as a prerequisite by the goal you're editing has been deleted.\r\n");
			}
		}
	}
	
	syslog(SYS_OLC, GET_INVIS_LEV(ch), TRUE, "OLC: %s has deleted progress entry %d", GET_NAME(ch), vnum);
	msg_to_char(ch, "Progress entry %d deleted.\r\n", vnum);
	
	free_progress(prg);
}


/**
* Function to save a player's changes to a progress entry (or a new one).
*
* @param descriptor_data *desc The descriptor who is saving.
*/
void save_olc_progress(descriptor_data *desc) {	
	progress_data *proto, *prg = GET_OLC_PROGRESS(desc);
	any_vnum vnum = GET_OLC_VNUM(desc);
	empire_data *emp, *next_emp;
	UT_hash_handle hh, sorted;

	// have a place to save it?
	if (!(proto = real_progress(vnum))) {
		proto = create_progress_table_entry(vnum);
	}
	
	// existing empires: strip perks if they have them
	HASH_ITER(hh, empire_table, emp, next_emp) {
		if (empire_has_completed_goal(emp, vnum)) {
			apply_progress_to_empire(emp, proto, FALSE);
		}
	}
	
	// free prototype strings and pointers
	if (PRG_NAME(proto)) {
		free(PRG_NAME(proto));
	}
	if (PRG_DESCRIPTION(proto)) {
		free(PRG_DESCRIPTION(proto));
	}
	free_progress_list(PRG_PREREQS(proto));
	free_requirements(PRG_TASKS(proto));
	free_progress_perks(PRG_PERKS(proto));
	
	// sanity
	if (!PRG_NAME(prg) || !*PRG_NAME(prg)) {
		if (PRG_NAME(prg)) {
			free(PRG_NAME(prg));
		}
		PRG_NAME(prg) = str_dup(default_progress_name);
	}
	if (PRG_DESCRIPTION(prg) && !*PRG_DESCRIPTION(prg)) {
		free(PRG_DESCRIPTION(prg));
		PRG_DESCRIPTION(prg) = NULL;
	}
	
	// save data back over the proto-type
	hh = proto->hh;	// save old hash handles
	sorted = proto->sorted_hh;
	*proto = *prg;	// copy over all data
	proto->vnum = vnum;	// ensure correct vnum
	proto->hh = hh;	// restore old hash handles
	proto->sorted_hh = sorted;
	
	// and save to file
	save_library_file_for_vnum(DB_BOOT_PRG, vnum);
	
	// existing empires: update all trackers
	HASH_ITER(hh, empire_table, emp, next_emp) {
		if (empire_has_completed_goal(emp, vnum)) {
			// re-apply even if it will be removed
			apply_progress_to_empire(emp, proto, TRUE);
		}
		
		// trigger a full refresh
		refresh_empire_goals(emp, vnum);
	}
}


/**
* Creates a copy of a progress entry, or clears a new one, for editing.
* 
* @param progress_data *input The progress data to copy, or NULL to make a new one.
* @return progress_data* The copied progress.
*/
progress_data *setup_olc_progress(progress_data *input) {
	progress_data *new;
	
	CREATE(new, progress_data, 1);
	clear_progress(new);
	
	if (input) {
		// copy normal data
		*new = *input;
		
		// copy things that are pointers
		PRG_NAME(new) = PRG_NAME(input) ? str_dup(PRG_NAME(input)) : NULL;
		PRG_DESCRIPTION(new) = PRG_DESCRIPTION(input) ? str_dup(PRG_DESCRIPTION(input)) : NULL;
		PRG_PREREQS(new) = copy_progress_list(PRG_PREREQS(input));
		PRG_TASKS(new) = copy_requirements(PRG_TASKS(input));
		PRG_PERKS(new) = copy_progress_perks(PRG_PERKS(input));
		
		PRG_VERSION(new) += 1;
	}
	else {
		// brand new: some defaults
		PRG_NAME(new) = str_dup(default_progress_name);
		PRG_FLAGS(new) = PRG_IN_DEVELOPMENT;
		
		PRG_VERSION(new) = 1;
	}
	
	// done
	return new;
}


 //////////////////////////////////////////////////////////////////////////////
//// DISPLAYS ////////////////////////////////////////////////////////////////

/**
* For vstat.
*
* @param char_data *ch The player requesting stats.
* @param progress_data *prg The progress entry to display.
*/
void do_stat_progress(char_data *ch, progress_data *prg) {
	char buf[MAX_STRING_LENGTH], part[MAX_STRING_LENGTH];
	size_t size;
	
	if (!prg) {
		return;
	}
	
	size = snprintf(buf, sizeof(buf), "VNum: [\tc%d\t0], Name: \ty%s\t0, Type: \ty%s\t0\r\n", PRG_VNUM(prg), PRG_NAME(prg), progress_types[PRG_TYPE(prg)]);
	size += snprintf(buf + size, sizeof(buf) - size, "%s", NULLSAFE(PRG_DESCRIPTION(prg)));
	
	size += snprintf(buf + size, sizeof(buf) - size, "Value: [\tc%d point%s\t0], Cost: [\tc%d point%s\t0]\r\n", PRG_VALUE(prg), PLURAL(PRG_VALUE(prg)), PRG_COST(prg), PLURAL(PRG_COST(prg)));
	
	get_progress_list_display(PRG_PREREQS(prg), part);
	size += snprintf(buf + size, sizeof(buf) - size, "Prerequisites:\r\n%s", *part ? part : " none\r\n");
	
	get_requirement_display(PRG_TASKS(prg), part);
	size += snprintf(buf + size, sizeof(buf) - size, "Tasks:\r\n%s", *part ? part : " none\r\n");
	
	get_progress_perks_display(PRG_PERKS(prg), part);
	size += snprintf(buf + size, sizeof(buf) - size, "Perks:\r\n%s", *part ? part : " none\r\n");
	
	page_string(ch->desc, buf, TRUE);
}


/**
* This is the main recipe display for progress OLC. It displays the user's
* currently-edited progress entry.
*
* @param char_data *ch The person who is editing a progress entry and will see its display.
*/
void olc_show_progress(char_data *ch) {
	progress_data *prg = GET_OLC_PROGRESS(ch->desc);
	char buf[MAX_STRING_LENGTH], lbuf[MAX_STRING_LENGTH];
	
	if (!prg) {
		return;
	}
	
	*buf = '\0';
	
	sprintf(buf + strlen(buf), "[%s%d\t0] %s%s\t0\r\n", OLC_LABEL_CHANGED, GET_OLC_VNUM(ch->desc), OLC_LABEL_UNCHANGED, !real_progress(PRG_VNUM(prg)) ? "new progression" : PRG_NAME(real_progress(PRG_VNUM(prg))));
	sprintf(buf + strlen(buf), "<%sname\t0> %s\r\n", OLC_LABEL_STR(PRG_NAME(prg), default_progress_name), NULLSAFE(PRG_NAME(prg)));
	sprintf(buf + strlen(buf), "<%sdescription\t0>\r\n%s", OLC_LABEL_STR(PRG_DESCRIPTION(prg), ""), NULLSAFE(PRG_DESCRIPTION(prg)));
	
	sprintf(buf + strlen(buf), "<%stype\t0> %s\r\n", OLC_LABEL_VAL(PRG_TYPE(prg), PROGRESS_UNDEFINED), progress_types[PRG_TYPE(prg)]);
	
	sprintbit(PRG_FLAGS(prg), progress_flags, lbuf, TRUE);
	sprintf(buf + strlen(buf), "<%sflags\t0> %s\r\n", OLC_LABEL_VAL(PRG_FLAGS(prg), PRG_IN_DEVELOPMENT), lbuf);
	
	if (PRG_FLAGGED(prg, PRG_PURCHASABLE)) {
		sprintf(buf + strlen(buf), "<%scost\t0> %d point%s\r\n", OLC_LABEL_VAL(PRG_COST(prg), 0), PRG_COST(prg), PLURAL(PRG_COST(prg)));
	}
	if (PRG_VALUE(prg) || !PRG_FLAGGED(prg, PRG_PURCHASABLE)) {
		sprintf(buf + strlen(buf), "<%svalue\t0> %d point%s\r\n", PRG_FLAGGED(prg, PRG_PURCHASABLE) ? "\tr" : OLC_LABEL_VAL(PRG_VALUE(prg), 0), PRG_VALUE(prg), PLURAL(PRG_VALUE(prg)));
	}
	
	get_progress_list_display(PRG_PREREQS(prg), lbuf);
	sprintf(buf + strlen(buf), "Prerequisites: <%sprereqs\t0>\r\n%s", OLC_LABEL_PTR(PRG_PREREQS(prg)), lbuf);
	
	if (PRG_TASKS(prg) || !PRG_FLAGGED(prg, PRG_PURCHASABLE | PRG_SCRIPT_ONLY)) {
		get_requirement_display(PRG_TASKS(prg), lbuf);
		sprintf(buf + strlen(buf), "Tasks: <%stasks\t0>\r\n%s", PRG_FLAGGED(prg, PRG_PURCHASABLE | PRG_SCRIPT_ONLY) ? "\tr" : OLC_LABEL_PTR(PRG_TASKS(prg)), lbuf);
	}
	
	get_progress_perks_display(PRG_PERKS(prg), lbuf);
	sprintf(buf + strlen(buf), "Perks: <%sperks\t0>\r\n%s", OLC_LABEL_PTR(PRG_PERKS(prg)), lbuf);
	
	page_string(ch->desc, buf, TRUE);
}


/**
* Searches the progress db for a match, and prints it to the character.
*
* @param char *searchname The search string.
* @param char_data *ch The player who is searching.
* @return int The number of matches shown.
*/
int vnum_progress(char *searchname, char_data *ch) {
	progress_data *iter, *next_iter;
	int found = 0;
	
	HASH_ITER(hh, progress_table, iter, next_iter) {
		if (multi_isname(searchname, PRG_NAME(iter))) {
			msg_to_char(ch, "%3d. [%5d] %s\r\n", ++found, PRG_VNUM(iter), PRG_NAME(iter));
		}
	}
	
	return found;
}


 //////////////////////////////////////////////////////////////////////////////
//// OLC MODULES /////////////////////////////////////////////////////////////

OLC_MODULE(progedit_cost) {
	progress_data *prg = GET_OLC_PROGRESS(ch->desc);
	
	if (!PRG_FLAGGED(prg, PRG_PURCHASABLE)) {
		msg_to_char(ch, "You cannot set the cost without the PURCHASABLE flag.\r\n");
	}
	else {
		PRG_COST(prg) = olc_process_number(ch, argument, "point cost", "cost", 0, INT_MAX, PRG_COST(prg));
	}
}


OLC_MODULE(progedit_description) {
	progress_data *prg = GET_OLC_PROGRESS(ch->desc);
	
	if (ch->desc->str) {
		msg_to_char(ch, "You are already editing a string.\r\n");
	}
	else {
		sprintf(buf, "description for %s", PRG_NAME(prg));
		start_string_editor(ch->desc, buf, &PRG_DESCRIPTION(prg), MAX_ITEM_DESCRIPTION, FALSE);
	}
}


OLC_MODULE(progedit_flags) {
	progress_data *prg = GET_OLC_PROGRESS(ch->desc);
	bool had_indev = IS_SET(PRG_FLAGS(prg), PRG_IN_DEVELOPMENT) ? TRUE : FALSE;
	PRG_FLAGS(prg) = olc_process_flag(ch, argument, "progress", "flags", progress_flags, PRG_FLAGS(prg));
		
	// validate removal of IN-DEVELOPMENT
	if (had_indev && !PRG_FLAGGED(prg, PRG_IN_DEVELOPMENT) && GET_ACCESS_LEVEL(ch) < LVL_UNRESTRICTED_BUILDER && !OLC_FLAGGED(ch, OLC_FLAG_CLEAR_IN_DEV)) {
		msg_to_char(ch, "You don't have permission to remove the IN-DEVELOPMENT flag.\r\n");
		SET_BIT(PRG_FLAGS(prg), PRG_IN_DEVELOPMENT);
	}
	
	// remove cost if !purchase
	if (!PRG_FLAGGED(prg, PRG_PURCHASABLE)) {
		PRG_COST(prg) = 0;
	}
}


OLC_MODULE(progedit_prereqs) {
	progress_data *prg = GET_OLC_PROGRESS(ch->desc);

	char cmd_arg[MAX_INPUT_LENGTH], vnum_arg[MAX_INPUT_LENGTH];
	struct progress_list *item, *iter;
	any_vnum vnum;
	bool found;
	int num;
	
	argument = any_one_arg(argument, cmd_arg);	// add/remove/change/copy
	
	if (is_abbrev(cmd_arg, "copy")) {
		// usage: .prereqs copy <from vnum>
		argument = any_one_arg(argument, vnum_arg);	// any vnum for that type
		
		if (!*vnum_arg) {
			msg_to_char(ch, "Usage: prereqs copy <from vnum>\r\n");
		}
		else if (!isdigit(*vnum_arg)) {
			msg_to_char(ch, "Copy from which progression goal?\r\n");
		}
		else if ((vnum = atoi(vnum_arg)) < 0 || !real_progress(vnum)) {
			msg_to_char(ch, "Invalid vnum.\r\n");
		}
		else {
			smart_copy_progress_prereqs(&PRG_PREREQS(prg), PRG_PREREQS(real_progress(vnum)));
			msg_to_char(ch, "Copied prereqs from progression %d.\r\n", vnum);
		}
	}	// end 'copy'
	else if (is_abbrev(cmd_arg, "remove")) {
		// usage: .prereqs remove <number | all>
		skip_spaces(&argument);	// only arg is number
		
		if (!*argument) {
			msg_to_char(ch, "Remove which prereq (number)?\r\n");
		}
		else if (!str_cmp(argument, "all")) {
			free_progress_list(PRG_PREREQS(prg));
			PRG_PREREQS(prg) = NULL;
			msg_to_char(ch, "You remove all the prereqs.\r\n");
		}
		else if (!isdigit(*argument) || (num = atoi(argument)) < 1) {
			msg_to_char(ch, "Invalid prereq number.\r\n");
		}
		else {
			found = FALSE;
			LL_FOREACH(PRG_PREREQS(prg), iter) {
				if (--num == 0) {
					found = TRUE;
					
					msg_to_char(ch, "You remove the prereq: %d %s.\r\n", iter->vnum, get_progress_name_by_proto(iter->vnum));
					LL_DELETE(PRG_PREREQS(prg), iter);
					free(iter);
					break;
				}
			}
			
			if (!found) {
				msg_to_char(ch, "Invalid prereq number.\r\n");
			}
		}
	}	// end 'remove'
	else if (is_abbrev(cmd_arg, "add")) {
		// usage: .prereqs add <vnum>
		argument = any_one_arg(argument, vnum_arg);
		
		if (!*vnum_arg ) {
			msg_to_char(ch, "Usage: prereqs add <vnum>\r\n");
		}
		else if (!isdigit(*vnum_arg) || (vnum = atoi(vnum_arg)) < 0 || !real_progress(vnum)) {
			msg_to_char(ch, "Invalid progression vnum '%s'.\r\n", vnum_arg);
		}
		else {
			// success
			CREATE(item, struct progress_list, 1);
			item->vnum = vnum;
			LL_APPEND(PRG_PREREQS(prg), item);
			
			msg_to_char(ch, "You add prereq: [%d] %s\r\n", vnum, get_progress_name_by_proto(vnum));
		}
	}	// end 'add'
	else {
		msg_to_char(ch, "Usage: prereq add <vnum>\r\n");
		msg_to_char(ch, "Usage: prereq copy <from vnum>\r\n");
		msg_to_char(ch, "Usage: prereq remove <number | all>\r\n");
	}
}


OLC_MODULE(progedit_name) {
	progress_data *prg = GET_OLC_PROGRESS(ch->desc);
	olc_process_string(ch, argument, "name", &PRG_NAME(prg));
}


OLC_MODULE(progedit_tasks) {
	progress_data *prg = GET_OLC_PROGRESS(ch->desc);
	olc_process_requirements(ch, argument, &PRG_TASKS(prg), "tasks", TRUE);
}


OLC_MODULE(progedit_perks) {
	progress_data *prg = GET_OLC_PROGRESS(ch->desc);

	char cmd_arg[MAX_INPUT_LENGTH], arg[MAX_INPUT_LENGTH];
	struct progress_perk *item, *iter;
	any_vnum vnum = NOTHING;
	int num, ptype;
	bool found;
	
	argument = any_one_arg(argument, cmd_arg);	// add/remove/change/copy
	
	if (is_abbrev(cmd_arg, "copy")) {
		// usage: .perks copy <from vnum>
		argument = any_one_arg(argument, arg);	// any vnum for that type
		
		if (!*arg) {
			msg_to_char(ch, "Usage: perks copy <from vnum>\r\n");
		}
		else if (!isdigit(*arg)) {
			msg_to_char(ch, "Copy perks from which progression goal?\r\n");
		}
		else if ((vnum = atoi(arg)) < 0 || !real_progress(vnum)) {
			msg_to_char(ch, "Invalid vnum.\r\n");
		}
		else {
			smart_copy_progress_perks(&PRG_PERKS(prg), PRG_PERKS(real_progress(vnum)));
			msg_to_char(ch, "Copied perks from progression %d.\r\n", vnum);
		}
	}	// end 'copy'
	else if (is_abbrev(cmd_arg, "remove")) {
		// usage: .perks remove <number | all>
		skip_spaces(&argument);	// only arg is number
		
		if (!*argument) {
			msg_to_char(ch, "Remove which perk (number)?\r\n");
		}
		else if (!str_cmp(argument, "all")) {
			free_progress_perks(PRG_PERKS(prg));
			PRG_PERKS(prg) = NULL;
			msg_to_char(ch, "You remove all the perks.\r\n");
		}
		else if (!isdigit(*argument) || (num = atoi(argument)) < 1) {
			msg_to_char(ch, "Invalid perk number.\r\n");
		}
		else {
			found = FALSE;
			LL_FOREACH(PRG_PERKS(prg), iter) {
				if (--num == 0) {
					found = TRUE;
					
					msg_to_char(ch, "You remove the perk: %s\r\n", get_one_perk_display(iter));
					LL_DELETE(PRG_PERKS(prg), iter);
					free(iter);
					break;
				}
			}
			
			if (!found) {
				msg_to_char(ch, "Invalid perk number.\r\n");
			}
		}
	}	// end 'remove'
	else if (is_abbrev(cmd_arg, "add")) {
		// usage: .perks add <type> <name/value>
		argument = any_one_arg(argument, arg);
		skip_spaces(&argument);
		
		if (!*arg || !*argument) {
			msg_to_char(ch, "Usage: perks add <type> <name/value>\r\nValid types:\r\n");
			for (num = 0; *progress_perk_types[num] != '\n'; ++num) {
				msg_to_char(ch, "  %s\r\n", progress_perk_types[num]);
			}
		}
		else if ((ptype = search_block(arg, progress_perk_types, FALSE)) == NOTHING) {
			msg_to_char(ch, "Invalid perk type '%s'.\r\n", arg);
		}
		else {
			// PRG_PERK_x: process value (argument) -- set 'vnum'
			switch (ptype) {
				case PRG_PERK_TECH: {
					if ((vnum = search_block(argument, techs, FALSE)) == NOTHING) {
						msg_to_char(ch, "Unknown tech '%s'.\r\n", arg);
						return;
					}
					// otherwise ok
					break;
				}
				default: {
					msg_to_char(ch, "That type is not yet implemented.\r\n");
					break;
				}
			}
			
			// success
			CREATE(item, struct progress_perk, 1);
			item->type = ptype;
			item->value = vnum;
			LL_APPEND(PRG_PERKS(prg), item);
			
			msg_to_char(ch, "You add the perk: %s\r\n", get_one_perk_display(item));
		}
	}	// end 'add'
	else {
		msg_to_char(ch, "Usage: perks add <type> <name/value>\r\n");
		msg_to_char(ch, "Usage: perks copy <from vnum>\r\n");
		msg_to_char(ch, "Usage: perks remove <number | all>\r\n");
	}
}


OLC_MODULE(progedit_type) {
	progress_data *prg = GET_OLC_PROGRESS(ch->desc);
	PRG_TYPE(prg) = olc_process_type(ch, argument, "type", "type", progress_types, PRG_TYPE(prg));
}

OLC_MODULE(progedit_value) {
	progress_data *prg = GET_OLC_PROGRESS(ch->desc);
	PRG_VALUE(prg) = olc_process_number(ch, argument, "point value", "value", 0, INT_MAX, PRG_VALUE(prg));
}
