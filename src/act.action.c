/* ************************************************************************
*   File: act.action.c                                    EmpireMUD 2.0b5 *
*  Usage: commands and processors related to the action system            *
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
#include "db.h"
#include "comm.h"
#include "handler.h"
#include "interpreter.h"
#include "skills.h"
#include "vnums.h"
#include "dg_scripts.h"
#include "constants.h"

/**
* Contents:
*   Control Data
*   Core Action Functions
*   Helpers
*   Action Cancelers
*   Action Starters
*   Action Finishers
*   Action Processes
*   Action Commands
*   Generic Interactions: do_gen_interact_room
*/

// cancel protos
void cancel_resource_list(char_data *ch);
void cancel_driving(char_data *ch);
void cancel_minting(char_data *ch);
void cancel_morphing(char_data *ch);
void cancel_movement_string(char_data *ch);
void cancel_siring(char_data *ch);

// process protos
void process_chipping(char_data *ch);
void process_driving(char_data *ch);
void perform_ritual(char_data *ch);
void perform_saw(char_data *ch);
void perform_study(char_data *ch);
void process_bathing(char_data *ch);
void process_burn_area(char_data *ch);
void process_chop(char_data *ch);
void process_copying_book(char_data *ch);
void process_digging(char_data *ch);
void process_dismantle_action(char_data *ch);
void process_escaping(char_data *ch);
void process_excavating(char_data *ch);
void process_fillin(char_data *ch);
void process_fishing(char_data *ch);
void process_foraging(char_data *ch);
void process_gathering(char_data *ch);
void process_gen_craft(char_data *ch);
void process_gen_interact_room(char_data *ch);
void process_harvesting(char_data *ch);
void process_hunting(char_data *ch);
void process_maintenance(char_data *ch);
void process_mining(char_data *ch);
void process_minting(char_data *ch);
void process_morphing(char_data *ch);
void process_music(char_data *ch);
void process_panning(char_data *ch);
void process_planting(char_data *ch);
void process_prospecting(char_data *ch);
void process_reading(char_data *ch);
void process_reclaim(char_data *ch);
void process_repairing(char_data *ch);
void process_running(char_data *ch);
void process_scraping(char_data *ch);
void process_siring(char_data *ch);
void process_start_fillin(char_data *ch);
void process_swap_skill_sets(char_data *ch);
void process_tanning(char_data *ch);

// other local protos
INTERACTION_FUNC(finish_foraging);

// external variables
extern bool catch_up_actions;

// external functions
ACMD(do_saw);
ACMD(do_mint);
ACMD(do_scrape);
ACMD(do_tan);
void perform_escape(char_data *ch);


 //////////////////////////////////////////////////////////////////////////////
//// CONTROL DATA ////////////////////////////////////////////////////////////

// ACT_x: main data for all chores
const struct action_data_struct action_data[] = {
	{ "", "", NOBITS, NULL, NULL },	// ACT_NONE
	{ "digging", "is digging at the ground.", ACTF_SHOVEL | ACTF_FINDER | ACTF_HASTE | ACTF_FAST_CHORES, process_digging, NULL },	// ACT_DIGGING
	{ "gathering", "is gathering plant material.", ACTF_FINDER | ACTF_HASTE | ACTF_FAST_CHORES, process_gathering, NULL },	// ACT_GATHERING
	{ "chopping", "is chopping down trees.", ACTF_HASTE | ACTF_FAST_CHORES, process_chop, NULL },	// ACT_CHOPPING
	{ "building", "is hard at work building.", ACTF_HASTE | ACTF_FAST_CHORES, process_build_action, NULL },	//  ACT_BUILDING (for any type of craft)
	{ "dismantling", "is dismantling the building.", ACTF_HASTE | ACTF_FAST_CHORES, process_dismantle_action, NULL },	// ACT_DISMANTLING
	{ "harvesting", "is harvesting the crop.", ACTF_HASTE | ACTF_FAST_CHORES, process_harvesting, NULL },	// ACT_HARVESTING
	{ "planting", "is planting seeds.", ACTF_HASTE | ACTF_FAST_CHORES, process_planting, NULL },	// ACT_PLANTING
	{ "mining", "is mining at the walls.", ACTF_HASTE | ACTF_FAST_CHORES, process_mining, NULL },	// ACT_MINING
	{ "minting", "is minting coins.", ACTF_ALWAYS_FAST | ACTF_FAST_CHORES, process_minting, cancel_minting },	// ACT_MINTING
	{ "fishing", "is fishing.", ACTF_SITTING, process_fishing, NULL },	// ACT_FISHING
	{ "preparing", "is preparing to fill in the trench.", NOBITS, process_start_fillin, NULL },	// ACT_START_FILLIN
	{ "repairing", "is doing some repairs.", ACTF_FAST_CHORES | ACTF_HASTE, process_repairing, NULL },	// ACT_REPAIR_VEHICLE
	{ "chipping", "is chipping flints.", ACTF_FAST_CHORES, process_chipping, cancel_resource_list },	// ACT_CHIPPING
	{ "panning", "is panning for gold.", ACTF_FINDER, process_panning, NULL },	// ACT_PANNING
	{ "playing", "is playing soothing music.", ACTF_ANYWHERE | ACTF_HASTE | ACTF_IGNORE_COND, process_music, NULL },	// ACT_MUSIC
	{ "excavating", "is excavating a trench.", ACTF_HASTE | ACTF_FAST_CHORES | ACTF_FAST_EXCAVATE, process_excavating, NULL },	// ACT_EXCAVATING
	{ "siring", "is hunched over.", NOBITS, process_siring, cancel_siring },	// ACT_SIRING
	{ "picking", "is looking around at the ground.", ACTF_FINDER | ACTF_HASTE | ACTF_FAST_CHORES, process_gen_interact_room, NULL },	// ACT_PICKING
	{ "morphing", "is morphing and changing shape!", ACTF_ANYWHERE, process_morphing, cancel_morphing },	// ACT_MORPHING
	{ "scraping", "is scraping something off.", ACTF_HASTE | ACTF_FAST_CHORES, process_scraping, cancel_resource_list },	// ACT_SCRAPING
	{ "bathing", "is bathing in the water.", NOBITS, process_bathing, NULL },	// ACT_BATHING
	{ "chanting", "is chanting a strange song.", NOBITS, perform_ritual, NULL },	// ACT_CHANTING
	{ "prospecting", "is prospecting.", ACTF_FAST_PROSPECT, process_prospecting, NULL },	// ACT_PROSPECTING
	{ "filling", "is filling in the trench.", ACTF_HASTE | ACTF_FAST_CHORES | ACTF_FAST_EXCAVATE, process_fillin, NULL },	// ACT_FILLING_IN
	{ "reclaiming", "is reclaiming the area!", NOBITS, process_reclaim, NULL },	// ACT_RECLAIMING
	{ "escaping", "is running toward the window!", NOBITS, process_escaping, NULL },	// ACT_ESCAPING
	{ "running", "runs past you.", ACTF_ALWAYS_FAST | ACTF_EVEN_FASTER | ACTF_FASTER_BONUS | ACTF_ANYWHERE, process_running, cancel_movement_string },	// unused
	{ "ritual", "is performing an arcane ritual.", NOBITS, perform_ritual, NULL },	// ACT_RITUAL
	{ "sawing", "is sawing something.", ACTF_HASTE | ACTF_FAST_CHORES, perform_saw, cancel_resource_list },	// ACT_SAWING
	{ "quarrying", "is quarrying stone.", ACTF_HASTE | ACTF_FAST_CHORES, process_gen_interact_room, NULL },	// ACT_QUARRYING
	{ "driving", "is driving.", ACTF_VEHICLE_SPEEDS | ACTF_SITTING, process_driving, cancel_driving },	// ACT_DRIVING
	{ "tanning", "is tanning leather.", ACTF_FAST_CHORES, process_tanning, cancel_resource_list },	// ACT_TANNING
	{ "reading", "is reading a book.", ACTF_SITTING | ACTF_ALWAYS_FAST | ACTF_EVEN_FASTER | ACTF_IGNORE_COND, process_reading, NULL },	// ACT_READING
	{ "copying", "is writing out a copy of a book.", NOBITS, process_copying_book, NULL },	// ACT_COPYING_BOOK
	{ "crafting", "is working on something.", NOBITS, process_gen_craft, cancel_gen_craft },	// ACT_GEN_CRAFT
	{ "sailing", "is sailing the ship.", ACTF_VEHICLE_SPEEDS | ACTF_SITTING, process_driving, cancel_driving },	// ACT_SAILING
	{ "piloting", "is piloting the vessel.", ACTF_VEHICLE_SPEEDS | ACTF_SITTING, process_driving, cancel_driving },	// ACT_PILOTING
	{ "skill-swapping", "is swapping skill sets.", NOBITS, process_swap_skill_sets, NULL },	// ACT_SWAP_SKILL_SETS
	{ "repairing", "is repairing the building.", ACTF_HASTE | ACTF_FAST_CHORES, process_maintenance, NULL },	// ACT_MAINTENANCE
	{ "burning", "is preparing to burn the area.", ACTF_FAST_CHORES, process_burn_area, NULL },	// ACT_BURN_AREA
	{ "hunting", "is low to the ground, hunting.", ACTF_FINDER, process_hunting, NULL },	// ACT_HUNTING
	{ "foraging", "is looking around for food.", ACTF_ALWAYS_FAST | ACTF_FINDER | ACTF_HASTE, process_foraging, NULL },	// ACT_FORAGING
	{ "dismantling", "is dismantling something.", ACTF_HASTE | ACTF_FAST_CHORES, process_dismantle_vehicle, NULL },	// ACT_DISMANTLING
	
	{ "\n", "\n", NOBITS, NULL, NULL }
};


// INTERACT_x: interactions that are processed by do_gen_interact_room
const struct gen_interact_data_t gen_interact_data[] = {
	// { interact, action, command, verb, timer
	//	ptech, depletion, approval_config
	//	{ { start-to-char, start-to-room }, { finish-to-char, finish-to-room },
	//		empty-to-char
	//		chance-of-random-tick-msg,
	//		{ { random-to-char, random-to-room }, ..., END_RANDOM_TICK_MSGS
	//	} }
	// }
	
	#define END_RANDOM_TICK_MSGS  { NULL, NULL }
	#define NO_RANDOM_TICK_MSGS  { END_RANDOM_TICK_MSGS }
	
	{ INTERACT_PICK, ACT_PICKING, "pick", "picking", 4,
		PTECH_PICK, DPLTN_PICK, "gather_approval",
		{ /* start msg */ { "You start looking for something to pick.", "$n starts looking for something to pick." },
		/* finish msg */ { "You find $p!", "$n finds $p!" },
		/* empty msg */ "You can't find anything here left to pick.",
		100, { // random tick messages:
			{ "You look around for something nice to pick...", "$n looks around for something to pick." },
			END_RANDOM_TICK_MSGS
		}}
	},
	{ INTERACT_QUARRY, ACT_QUARRYING, "quarry", "quarrying", 12,
		PTECH_QUARRY, DPLTN_QUARRY, "gather_approval",
		{ /* start msg */ { "You begin to work the quarry.", "$n begins to work the quarry." },
		/* finish msg */ { "You give the plug drill one final swing and pry loose $p!", "$n hits the plug drill hard with a hammer and pries loose $p!" },
		/* empty msg */ "You don't seem to find anything of use.",
		30, { // random tick messages:
			{ "You slip some shims into cracks in the stone.", "$n slips some shims into cracks in the stone." },
			{ "You brush dust out of the cracks in the stone.", "$n brushes dust out of the cracks in the stone." },
			{ "You hammer a plug drill into the stone.", "$n hammers a plug drill into the stone." },
			END_RANDOM_TICK_MSGS
		}}
	},
	
	{ -1, -1, "\n", "\n", 0, NOTHING, NOTHING, NULL, { { NULL, NULL }, { NULL, NULL }, NULL, 0, NO_RANDOM_TICK_MSGS } }	// last
};


 //////////////////////////////////////////////////////////////////////////////
//// CORE ACTION FUNCTIONS ///////////////////////////////////////////////////

/**
* Ends the character's current timed action prematurely.
*
* @param char_data *ch The actor.
*/
void cancel_action(char_data *ch) {
	if (!IS_NPC(ch) && GET_ACTION(ch) != ACT_NONE) {
		// is there a cancel function?
		if (action_data[GET_ACTION(ch)].cancel_function != NULL) {
			(action_data[GET_ACTION(ch)].cancel_function)(ch);
		}
		
		GET_ACTION(ch) = ACT_NONE;
	}
}


/**
* Begins an action.
*
* @param char_data *ch The actor
* @param int type ACT_ const
* @param int timer Countdown in action ticks
*/
void start_action(char_data *ch, int type, int timer) {
	// safety first
	if (GET_ACTION(ch) != ACT_NONE) {
		cancel_action(ch);
	}

	GET_ACTION(ch) = type;
	GET_ACTION_CYCLE(ch) = ACTION_CYCLE_TIME * ACTION_CYCLE_MULTIPLIER;
	GET_ACTION_TIMER(ch) = timer;
	GET_ACTION_VNUM(ch, 0) = 0;
	GET_ACTION_VNUM(ch, 1) = 0;
	GET_ACTION_VNUM(ch, 2) = 0;
	GET_ACTION_ROOM(ch) = GET_ROOM_VNUM(IN_ROOM(ch));
	
	// ensure no resources already stored
	free_resource_list(GET_ACTION_RESOURCES(ch));
	GET_ACTION_RESOURCES(ch) = NULL;
}


/**
* Cancels actions for anyone in the room on that action.
*
* @param room_data *room Where.
* @param int action ACTION_x or NOTHING to not stop actions.
*/
void stop_room_action(room_data *room, int action) {
	char_data *c;
	
	DL_FOREACH2(ROOM_PEOPLE(room), c, next_in_room) {
		// player actions
		if (action != NOTHING && !IS_NPC(c) && GET_ACTION(c) == action) {
			cancel_action(c);
		}
	}
}


/**
* This is the main processor for periodic actions (ACT_), once per second.
*/
void update_actions(void) {
	// prevent running multiple action rounds during a catch-up cycle
	if (!catch_up_actions) {
		return;
	}
	catch_up_actions = FALSE;
	
	descriptor_data *desc;
	bitvector_t act_flags;
	craft_data *craft;
	char_data *ch;
	double speed;
	bool junk;

	// only players with active connections can process actions
	for (desc = descriptor_list; desc; desc = desc->next) {
		ch = desc->character;
		
		// basic disqualifiers
		if (!ch || STATE(desc) != CON_PLAYING || IS_NPC(ch) || GET_ACTION(ch) == ACT_NONE) {
			continue;
		}
		
		// compile action flags
		act_flags = action_data[GET_ACTION(ch)].flags;
		if (GET_ACTION(ch) == ACT_GEN_CRAFT && (craft = craft_proto(GET_ACTION_VNUM(ch, 0)))) {
			act_flags |= gen_craft_data[GET_CRAFT_TYPE(craft)].actf_flags;
		}
		
		// things which terminate actions
		if (ACCOUNT_FLAGGED(ch, ACCT_FROZEN)) {
			cancel_action(ch);
			continue;
		}
		if (AFF_FLAGGED(ch, AFF_STUNNED | AFF_HARD_STUNNED)) {
			cancel_action(ch);
			continue;
		}
		if (action_data[GET_ACTION(ch)].process_function == NULL) {
			// no way to process this action
			cancel_action(ch);
			continue;
		}
		if (!IS_SET(act_flags, ACTF_ANYWHERE) && GET_ROOM_VNUM(IN_ROOM(ch)) != GET_ACTION_ROOM(ch)) {
			cancel_action(ch);
			continue;
		}
		if (FIGHTING(ch)) {
			cancel_action(ch);
			continue;
		}
		if (GET_POS(ch) < POS_SITTING || GET_POS(ch) == POS_FIGHTING || (!IS_SET(act_flags, ACTF_SITTING) && GET_POS(ch) < POS_STANDING)) {
			// in most positions, they should know why they're stopping... these two are an exception:
			if (GET_POS(ch) == POS_SITTING || GET_POS(ch) == POS_RESTING) {
				msg_to_char(ch, "You can't keep %s while %s.\r\n", action_data[GET_ACTION(ch)].name, (GET_POS(ch) == POS_RESTING ? "resting" : "sitting"));
			}
			cancel_action(ch);
			continue;
		}
		if ((GET_FEEDING_FROM(ch) || GET_FED_ON_BY(ch)) && GET_ACTION(ch) != ACT_SIRING) {
			cancel_action(ch);
			continue;
		}
		if (AFF_FLAGGED(ch, AFF_DISTRACTED)) {
			msg_to_char(ch, "You are distracted and stop what you were doing.\r\n");
			cancel_action(ch);
			continue;
		}
		
		// action-cycle is time remaining -- compute how fast we go through it
		speed = ACTION_CYCLE_SECOND;	// makes it a full second
		
		// things that modify speed...
		if (IS_SET(act_flags, ACTF_ALWAYS_FAST)) {
			speed += ACTION_CYCLE_SECOND;
		}
		if (IS_SET(act_flags, ACTF_EVEN_FASTER)) {
			speed += ACTION_CYCLE_SECOND;
		}
		if (IS_SET(act_flags, ACTF_HASTE) && AFF_FLAGGED(ch, AFF_HASTE)) {
			speed += ACTION_CYCLE_HALF_SEC;
		}
		if (IS_SET(act_flags, ACTF_FAST_CHORES) && HAS_BONUS_TRAIT(ch, BONUS_FAST_CHORES)) {
			speed += ACTION_CYCLE_HALF_SEC;
		}
		if (IS_SET(act_flags, ACTF_FASTER_BONUS) && HAS_BONUS_TRAIT(ch, BONUS_FASTER)) {
			speed += ACTION_CYCLE_HALF_SEC;
		}
		if (IS_SET(act_flags, ACTF_FAST_PROSPECT) && GET_LOYALTY(ch) && EMPIRE_HAS_TECH(GET_LOYALTY(ch), TECH_FAST_PROSPECT)) {
			speed += ACTION_CYCLE_SECOND;
		}
		if (IS_SET(act_flags, ACTF_FAST_EXCAVATE) && GET_LOYALTY(ch) && EMPIRE_HAS_TECH(GET_LOYALTY(ch), TECH_FAST_EXCAVATE) && is_in_city_for_empire(IN_ROOM(ch), GET_LOYALTY(ch), TRUE, &junk)) {
			speed += ACTION_CYCLE_SECOND;
		}
		if (IS_SET(act_flags, ACTF_FINDER) && has_player_tech(ch, PTECH_FAST_FIND)) {
			speed += ACTION_CYCLE_HALF_SEC;
			gain_player_tech_exp(ch, PTECH_FAST_FIND, 0.1);
		}
		if (IS_SET(act_flags, ACTF_SHOVEL) && has_tool(ch, TOOL_SHOVEL)) {
			speed += ACTION_CYCLE_HALF_SEC;
		}
		
		// Vehicles set a flat speed based on their number of speed bonuses.
		if (IS_SET(act_flags, ACTF_VEHICLE_SPEEDS)) {
			int half_secs_to_add_to_base_speed = VSPEED_NORMAL;
			vehicle_data *veh = get_current_piloted_vehicle(ch);
			
			if (veh) {
				// Bounds check the value for sanity.
				if (VEH_SPEED_BONUSES(veh) >= VSPEED_VERY_SLOW && VEH_SPEED_BONUSES(veh) <= VSPEED_VERY_FAST) {
					// Since we have a valid speed, set this as the multiplier for our half-second addition.
					half_secs_to_add_to_base_speed = VEH_SPEED_BONUSES(veh);
				} else {
					// In the case of no valid VSPEED_ flag being detected, move at VSPEED_VERY_SLOW and log.
					log("SYSERR: Unrecognized vehicle speed flag %d for vehicle #%d (%s).", VEH_SPEED_BONUSES(veh), VEH_VNUM(veh), VEH_SHORT_DESC(veh));
				}
				
				// Apply our vehicle movement modifier to speed, overriding any prior speed changes.
				speed = ACTION_CYCLE_SECOND + (ACTION_CYCLE_HALF_SEC * half_secs_to_add_to_base_speed);
			} else {
				// If we have no vehicle tp read from, mimic the behavior of the previous code (it didn't check for vehicles).
				// Previous code's behavior was to give all driving/piloting characters a flat +2 speed boost.
				speed += ACTION_CYCLE_SECOND;
			}
		}
		
		// things that slow you down
		if ((AFF_FLAGGED(ch, AFF_SLOW) || IS_HUNGRY(ch) || IS_THIRSTY(ch) || IS_BLOOD_STARVED(ch)) && !IS_SET(act_flags, ACTF_IGNORE_COND)) {
			speed /= 2.0;
			speed = MAX(1.0, speed);	// don't stall them completely
		}
		
		GET_ACTION_CYCLE(ch) -= speed;
		
		if (GET_ACTION_CYCLE(ch) <= 0.0) {
			// reset cycle timer
			GET_ACTION_CYCLE(ch) = ACTION_CYCLE_TIME * ACTION_CYCLE_MULTIPLIER;
			
			// end hide at this point, as if they had typed a command each tick
			REMOVE_BIT(AFF_FLAGS(ch), AFF_HIDE);
			
			if (action_data[GET_ACTION(ch)].process_function != NULL) {
				// call the process function
				(action_data[GET_ACTION(ch)].process_function)(ch);
			}
		}
	}
}


 //////////////////////////////////////////////////////////////////////////////
//// HELPERS /////////////////////////////////////////////////////////////////

/**
* Determines if the character is performing an action with a particular flag.
*
* @param char_data *ch The person.
* @param bitvector_t actf Any ACTF_ flag, like ACTF_SITTING.
* @return bool TRUE if the player's action has the flag, FALSE if not.
*/
bool action_flagged(char_data *ch, bitvector_t actf) {
	craft_data *craft;
	
	if (IS_NPC(ch) || GET_ACTION(ch) == ACT_NONE || !actf) {
		return FALSE;	// no work
	}
	
	if (IS_SET(action_data[GET_ACTION(ch)].flags, actf)) {
		return TRUE;
	}
	if (GET_ACTION(ch) == ACT_GEN_CRAFT && (craft = craft_proto(GET_ACTION_VNUM(ch, 0))) && IS_SET(gen_craft_data[GET_CRAFT_TYPE(craft)].actf_flags, actf)) {
		return TRUE;
	}
	
	return FALSE;	// otherwise
}


/**
* When a player forages in the wild and gets nothing, they get a chance at a
* local crop, too, even if that crop is not on this tile.
*
* @param char_data *ch The person trying to forage.
* @return bool TRUE if any forage interactions ran successfully, FALSE if not.
*/
bool do_crop_forage(char_data *ch) {
	crop_data *crop;
	
	if (!IS_OUTDOOR_TILE(IN_ROOM(ch)) || IS_MAP_BUILDING(IN_ROOM(ch))) {
		return FALSE;	// must be outdoor
	}
	
	if ((crop = get_potential_crop_for_location(IN_ROOM(ch), TRUE))) {
		return run_interactions(ch, GET_CROP_INTERACTIONS(crop), INTERACT_FORAGE, IN_ROOM(ch), NULL, NULL, NULL, finish_foraging);
	}
	else {
		return FALSE;	// no crop
	}
}


/**
* Finds a tool equipped by the character. Returns the best one.
*
* @param char_data *ch The person who might have the tool.
* @param bitvector_t flags The TOOL_ flags required -- player must have ONE of these flags (see has_all_tools).
* @return obj_data* The character's equipped tool, or NULL if they have none.
*/
obj_data *has_tool(char_data *ch, bitvector_t flags) {
	obj_data *tool, *best_tool = NULL;
	int iter;
	
	// prefer tool slot
	if (GET_EQ(ch, WEAR_TOOL) && TOOL_FLAGGED(GET_EQ(ch, WEAR_TOOL), flags)) {
		best_tool = GET_EQ(ch, WEAR_TOOL);
	}
	
	// then check other slots
	for (iter = 0; iter < NUM_WEARS; ++iter) {
		if (iter == WEAR_TOOL || iter == WEAR_SHARE) {
			continue;	// skip tool/share slots
		}
		if (!(tool = GET_EQ(ch, iter))) {
			continue;	// no item
		}
		if (!TOOL_FLAGGED(tool, flags)) {
			continue;	// not flagged
		}
		
		// ok! try to see if it's better
		if (!best_tool || (OBJ_FLAGGED(tool, OBJ_SUPERIOR) && !OBJ_FLAGGED(best_tool, OBJ_SUPERIOR)) || (!OBJ_FLAGGED(best_tool, OBJ_SUPERIOR) && GET_OBJ_CURRENT_SCALE_LEVEL(tool) > GET_OBJ_CURRENT_SCALE_LEVEL(best_tool))) {
			best_tool = tool;	// it seems better
		}
	}
	
	return best_tool;	// if any
}


/**
* Variant of has_tool() that requires ALL flags be present.
*
* @param char_data *ch The person who might have the tool.
* @param bitvector_t flags The TOOL_ flags required -- all flags must be present.
* @return obj_data* The best equipped tool that matches (more than 1 tool could have contributed the required flags), or NULL if they don't have all the required flags.
*/
obj_data *has_all_tools(char_data *ch, bitvector_t flags) {
	obj_data *tool, *best_tool = NULL;
	bitvector_t to_find = flags;
	int iter;
	
	// prefer tool slot
	if (GET_EQ(ch, WEAR_TOOL) && TOOL_FLAGGED(GET_EQ(ch, WEAR_TOOL), flags)) {
		best_tool = GET_EQ(ch, WEAR_TOOL);
		REMOVE_BIT(to_find, GET_OBJ_TOOL_FLAGS(best_tool));
	}
	
	for (iter = 0; to_find && iter < NUM_WEARS; ++iter) {
		if (iter == WEAR_TOOL || iter == WEAR_SHARE) {
			continue;	// skip tool/share slots
		}
		
		tool = GET_EQ(ch, iter);
		if (tool && TOOL_FLAGGED(tool, flags)) {
			// it has 1 or more of the flags (original, not remaining flags): try to see if it's better
			if (!best_tool || OBJ_FLAGGED(tool, OBJ_SUPERIOR) || (!OBJ_FLAGGED(best_tool, OBJ_SUPERIOR) && GET_OBJ_CURRENT_SCALE_LEVEL(tool) > GET_OBJ_CURRENT_SCALE_LEVEL(best_tool))) {
				best_tool = tool;	// it seems better
			}
			// now remove it from remaining flags (we only need to find any flags that remain
			REMOVE_BIT(to_find, GET_OBJ_TOOL_FLAGS(tool));
		}
	}
	
	if (to_find) {
		return NULL;	// we didn't find all the flags
	}
	else {
		return best_tool;	// we DID find all the flags; return the best tool
	}
}


/**
* Display prospect information for the room.
*
* @param char_data *ch The person seeing the info.
* @param room_data *room The room to prospect.
*/
void show_prospect_result(char_data *ch, room_data *room) {
	if (get_room_extra_data(room, ROOM_EXTRA_MINE_AMOUNT) <= 0 || !global_proto(get_room_extra_data(room, ROOM_EXTRA_MINE_GLB_VNUM))) {
		msg_to_char(ch, "This area has already been mined for all it's worth.\r\n");
	}
	else {
		if (is_deep_mine(room)) {
			msg_to_char(ch, "You discover that this area is a deep %s.\r\n", get_mine_type_name(room));
		}
		else {
			msg_to_char(ch, "You discover that this area is %s %s.\r\n", AN(get_mine_type_name(room)), get_mine_type_name(room));
		}
	}
}


/**
* Makes sure a person can [still] burn the room they are in.
*
* @param char_data *ch The player.
* @param int subcmd SCMD_LIGHT or SCMD_BURN.
* @return bool TRUE if safe, FALSE if they cannot burn it.
*/
bool validate_burn_area(char_data *ch, int subcmd) {
	const char *cmdname[] = { "light", "burn" };	// also in do_burn_area
	
	bool objless = has_player_tech(ch, PTECH_LIGHT_FIRE);
	obj_data *lighter = NULL;
	bool kept = FALSE;
	
	if (!objless) {	// find lighter if needed
		lighter = find_lighter_in_list(ch->carrying, &kept);
	}
	
	if (!has_evolution_type(SECT(IN_ROOM(ch)), EVO_BURNS_TO)) {
		msg_to_char(ch, "You can't %s this type of area.\r\n", cmdname[subcmd]);
	}
	else if (ROOM_AFF_FLAGGED(IN_ROOM(ch), ROOM_AFF_NO_EVOLVE)) {
		msg_to_char(ch, "You can't burn the area right now.\r\n");
	}
	else if (!objless && !lighter) {
		// nothing to light it with
		if (kept) {
			msg_to_char(ch, "You need a lighter that isn't marked 'keep'.\r\n");
		}
		else {
			msg_to_char(ch, "You don't have a lighter to %s the area with.\r\n", cmdname[subcmd]);
		}
	}
	else if (ROOM_OWNER(IN_ROOM(ch)) && ROOM_OWNER(IN_ROOM(ch)) != GET_LOYALTY(ch) && !has_relationship(GET_LOYALTY(ch), ROOM_OWNER(IN_ROOM(ch)), DIPL_WAR)) {
		msg_to_char(ch, "You must be at war to burn someone else's territory!\r\n");
	}
	else { // safe!
		return TRUE;
	}
	
	// if we got here:
	return FALSE;
}


 //////////////////////////////////////////////////////////////////////////////
//// ACTION CANCELERS ////////////////////////////////////////////////////////

/**
* Returns a set of resources from the resource list (if any). This will return
* the player to the room they started the action in (temporarily) if they have
* moved but the action doesn't allow it.
*
* @param char_data *ch The person canceling the craft (or whatever).
*/
void cancel_resource_list(char_data *ch) {
	room_data *action_room, *was_in = NULL;
	
	if (GET_ACTION_ROOM(ch) != GET_ROOM_VNUM(IN_ROOM(ch)) && !IS_SET(action_data[GET_ACTION(ch)].flags, ACTF_ANYWHERE) && (action_room = real_room(GET_ACTION_ROOM(ch)))) {
		was_in = IN_ROOM(ch);
		char_to_room(ch, action_room);
	}
	
	give_resources(ch, GET_ACTION_RESOURCES(ch), FALSE);
	free_resource_list(GET_ACTION_RESOURCES(ch));
	GET_ACTION_RESOURCES(ch) = NULL;
	
	if (was_in && was_in != IN_ROOM(ch)) {
		char_to_room(ch, was_in);
	}
}


/**
* Returns the item to the minter.
*
* @param char_data *ch The minting man.
*/
void cancel_minting(char_data *ch) {
	obj_data *obj = read_object(GET_ACTION_VNUM(ch, 0), TRUE);
	scale_item_to_level(obj, 1);	// minimum level
	obj_to_char(obj, ch);
	load_otrigger(obj);
}


/**
* Returns the morph item, if any.
*
* @param char_data *ch The morpher.
*/
void cancel_morphing(char_data *ch) {
	morph_data *morph = morph_proto(GET_ACTION_VNUM(ch, 0));
	obj_data *obj;
	
	if (morph && MORPH_FLAGGED(morph, MORPHF_CONSUME_OBJ) && MORPH_REQUIRES_OBJ(morph) != NOTHING) {
		obj = read_object(MORPH_REQUIRES_OBJ(morph), TRUE);
		scale_item_to_level(obj, 1);	// minimum level
		obj_to_char(obj, ch);
		load_otrigger(obj);
		
		if (!IS_IMMORTAL(ch) && OBJ_FLAGGED(obj, OBJ_BIND_FLAGS)) {	// bind when used (or gotten)
			bind_obj_to_player(obj, ch);
			reduce_obj_binding(obj, ch);
		}
	}
}

/**
* Frees the movement string.
*
* @param char_data *ch The person canceling the action.
*/
void cancel_movement_string(char_data *ch) {
	if (GET_MOVEMENT_STRING(ch)) {
		free(GET_MOVEMENT_STRING(ch));
	}
	GET_MOVEMENT_STRING(ch) = NULL;
}


 //////////////////////////////////////////////////////////////////////////////
//// ACTION STARTERS /////////////////////////////////////////////////////////

/**
* initiates chop and sends a start message or error message
*
* @param char_data *ch The player to start chopping.
*/
void start_chopping(char_data *ch) {
	char buf[MAX_STRING_LENGTH], weapon[MAX_STRING_LENGTH];
	obj_data *tool;
	
	if (!ROOM_AFF_FLAGGED(IN_ROOM(ch), ROOM_AFF_UNCLAIMABLE) && !can_use_room(ch, IN_ROOM(ch), MEMBERS_ONLY)) {
		msg_to_char(ch, "You don't have permission to chop here.\r\n");
	}
	else if (!CAN_CHOP_ROOM(IN_ROOM(ch)) || get_depletion(IN_ROOM(ch), DPLTN_CHOP) >= config_get_int("chop_depletion")) {
		msg_to_char(ch, "There's nothing left here to chop.\r\n");
	}
	else if (ROOM_AFF_FLAGGED(IN_ROOM(ch), ROOM_AFF_HAS_INSTANCE | ROOM_AFF_NO_EVOLVE)) {
		msg_to_char(ch, "You can't chop here right now.\r\n");
	}
	else {
		start_action(ch, ACT_CHOPPING, 0);
		
		// ensure progress data is set up
		if (get_room_extra_data(IN_ROOM(ch), ROOM_EXTRA_CHOP_PROGRESS) <= 0) {
			set_room_extra_data(IN_ROOM(ch), ROOM_EXTRA_CHOP_PROGRESS, config_get_int("chop_timer"));
		}
		
		if ((tool = has_tool(ch, TOOL_AXE))) {
			strcpy(weapon, GET_OBJ_SHORT_DESC(tool));
		}
		else {
			strcpy(weapon, "your axe");
		}
		
		snprintf(buf, sizeof(buf), "You swing back %s and prepare to chop...", weapon);
		act(buf, FALSE, ch, NULL, NULL, TO_CHAR);
		
		snprintf(buf, sizeof(buf), "$n swings %s over $s shoulder...", weapon);
		act(buf, TRUE, ch, NULL, NULL, TO_ROOM);
	}
}


/**
* begins the digging action if able
*
* @param char_data *ch The player to start digging.
*/
static void start_digging(char_data *ch) {
	int dig_base_timer = config_get_int("dig_base_timer");
	
	if (can_interact_room(IN_ROOM(ch), INTERACT_DIG)) {
		start_action(ch, ACT_DIGGING, dig_base_timer);

		send_to_char("You begin to dig into the ground.\r\n", ch);
		act("$n kneels down and begins to dig.", TRUE, ch, 0, 0, TO_ROOM);
	}
}


/**
* begins the "forage" action
*
* @param char_data *ch The future forager.
*/
void start_foraging(char_data *ch) {
	start_action(ch, ACT_FORAGING, config_get_int("forage_base_timer"));
	send_to_char("You begin foraging around for food...\r\n", ch);
}


/**
* Starts the mining action where possible.
*
* @param char_data *ch The prospective miner.
*/
void start_mining(char_data *ch) {
	int mining_timer = config_get_int("mining_timer");
	
	if (room_has_function_and_city_ok(GET_LOYALTY(ch), IN_ROOM(ch), FNC_MINE)) {
		if (get_room_extra_data(IN_ROOM(ch), ROOM_EXTRA_MINE_AMOUNT) > 0) {
			start_action(ch, ACT_MINING, mining_timer);
			
			msg_to_char(ch, "You look for a suitable place and begin to mine.\r\n");
			act("$n finds a suitable place and begins to mine.", FALSE, ch, 0, 0, TO_ROOM);
		}
		else {
			msg_to_char(ch, "The mine is depleted.\r\n");
		}
	}
	else {
		msg_to_char(ch, "You can't mine here.\r\n");
	}
}


/**
* Begins panning.
*
* @param char_data *ch The panner.
* @param int dir Which direction the character is panning (or NO_DIR for same-tile).
*/
void start_panning(char_data *ch, int dir) {
	int panning_timer = config_get_int("panning_timer");
	
	if (find_flagged_sect_within_distance_from_char(ch, SECTF_FRESH_WATER, NOBITS, 1)) {
		start_action(ch, ACT_PANNING, panning_timer);
		GET_ACTION_VNUM(ch, 0) = dir;
		
		msg_to_char(ch, "You kneel down and begin panning.\r\n");
		act("$n kneels down and begins panning.", TRUE, ch, NULL, NULL, TO_ROOM);
	}
}


 //////////////////////////////////////////////////////////////////////////////
//// ACTION FINISHERS ////////////////////////////////////////////////////////

INTERACTION_FUNC(finish_chopping) {
	char buf[MAX_STRING_LENGTH], *cust;
	obj_data *obj = NULL;
	int num;
	
	for (num = 0; num < interaction->quantity; ++num) {
		obj = read_object(interaction->vnum, TRUE);
		scale_item_to_level(obj, 1);	// minimum level
		obj_to_char_or_room(obj, ch);
		load_otrigger(obj);
	}
	
	// mark gained
	if (GET_LOYALTY(ch)) {
		add_production_total(GET_LOYALTY(ch), interaction->vnum, interaction->quantity);
	}
	
	// messaging
	if (obj) {
		cust = obj_get_custom_message(obj, OBJ_CUSTOM_RESOURCE_TO_CHAR);
		if (interaction->quantity > 1) {
			sprintf(buf, "%s (x%d)", cust ? cust : "With a loud crack, $p falls!", interaction->quantity);
			act(buf, FALSE, ch, obj, NULL, TO_CHAR);
		}
		else {
			act(cust ? cust : "With a loud crack, $p falls!", FALSE, ch, obj, NULL, TO_CHAR);
		}
		
		cust = obj_get_custom_message(obj, OBJ_CUSTOM_RESOURCE_TO_ROOM);
		act(cust ? cust : "$n collects $p.", FALSE, ch, obj, NULL, TO_ROOM);
	}
	
	return TRUE;
}


INTERACTION_FUNC(finish_digging) {	
	obj_vnum vnum = interaction->vnum;
	obj_data *obj = NULL;
	char *cust;
	int num;
	
	// depleted? (uses rock for all types except clay)
	if (get_depletion(inter_room, DPLTN_DIG) >= DEPLETION_LIMIT(inter_room)) {
		msg_to_char(ch, "The ground is too hard and there doesn't seem to be anything useful to dig up here.\r\n");
		return FALSE;
	}
	else {
		for (num = 0; num < interaction->quantity; ++num) {
			obj = read_object(vnum, TRUE);
			scale_item_to_level(obj, 1);	// minimum level
			obj_to_char_or_room(obj, ch);
			load_otrigger(obj);
			
			// add to depletion and 1/4 chance of adding a second one, to mix up the depletion values
			add_depletion(inter_room, DPLTN_DIG, TRUE);
		}
		
		// mark gained
		if (GET_LOYALTY(ch)) {
			add_production_total(GET_LOYALTY(ch), interaction->vnum, interaction->quantity);
		}
		
		if (obj) {
			// to-char
			cust = obj_get_custom_message(obj, OBJ_CUSTOM_RESOURCE_TO_CHAR);
			if (interaction->quantity > 1) {
				sprintf(buf1, "%s (x%d)", cust ? cust : "You pull $p from the ground!", interaction->quantity);
			}
			else {
				strcpy(buf1, cust ? cust : "You pull $p from the ground!");
			}
			act(buf1, FALSE, ch, obj, 0, TO_CHAR);
			
			// to-room
			cust = obj_get_custom_message(obj, OBJ_CUSTOM_RESOURCE_TO_ROOM);
			act(cust ? cust : "$n pulls $p from the ground!", FALSE, ch, obj, NULL, TO_ROOM);
		}
	}
	
	return TRUE;
}


INTERACTION_FUNC(finish_fishing) {
	char buf[MAX_STRING_LENGTH];
	char *to_char = NULL, *to_room = NULL;
	obj_data *obj = NULL, *tool;
	int num;
	
	const char *default_to_char = "You catch $p!";
	const char *default_to_room = "$n catches $p!";
	
	for (num = 0; num < interaction->quantity; ++num) {
		obj = read_object(interaction->vnum, TRUE);
		scale_item_to_level(obj, 1);	// minimum level
		obj_to_char(obj, ch);
		load_otrigger(obj);
	}
	
	// mark gained
	if (GET_LOYALTY(ch)) {
		add_production_total(GET_LOYALTY(ch), interaction->vnum, interaction->quantity);
	}
	
	// messaging
	if (obj) {
		// check for custom messages on their fishing tool
		if ((tool = has_tool(ch, TOOL_FISHING))) {
			to_char = obj_get_custom_message(tool, OBJ_CUSTOM_FISH_TO_CHAR);
			to_room = obj_get_custom_message(tool, OBJ_CUSTOM_FISH_TO_ROOM);
		}
		if (!to_char) {
			to_char = obj_get_custom_message(obj, OBJ_CUSTOM_RESOURCE_TO_CHAR);
		}
		if (!to_room) {
			to_room = obj_get_custom_message(obj, OBJ_CUSTOM_RESOURCE_TO_ROOM);
		}
		
		if (interaction->quantity > 1) {
			sprintf(buf, "%s (x%d)", to_char ? to_char : default_to_char, interaction->quantity);
			act(buf, FALSE, ch, obj, NULL, TO_CHAR);
		}
		else {
			act(to_char ? to_char : default_to_char, FALSE, ch, obj, NULL, TO_CHAR);
		}
		
		act(to_room ? to_room : default_to_room, TRUE, ch, obj, NULL, TO_ROOM);
	}
	
	return TRUE;
}


INTERACTION_FUNC(finish_foraging) {
	obj_data *obj = NULL;
	obj_vnum vnum = interaction->vnum;
	int iter, num = interaction->quantity;
	char *cust;

	// give objs
	for (iter = 0; iter < num; ++iter) {
		obj = read_object(vnum, TRUE);
		scale_item_to_level(obj, 1);	// minimum level
		obj_to_char_or_room(obj, ch);
		add_depletion(inter_room, DPLTN_FORAGE, TRUE);
		load_otrigger(obj);
	}
	
	// mark gained
	if (GET_LOYALTY(ch)) {
		add_production_total(GET_LOYALTY(ch), vnum, num);
	}
	
	if (obj) {
		cust = obj_get_custom_message(obj, OBJ_CUSTOM_RESOURCE_TO_CHAR);
		if (num > 1) {
			sprintf(buf, "%s (x%d)", cust ? cust : "You find $p!", num);
			act(buf, FALSE, ch, obj, 0, TO_CHAR);
		}
		else {
			act(cust ? cust : "You find $p!", FALSE, ch, obj, 0, TO_CHAR);
		}
		
		cust = obj_get_custom_message(obj, OBJ_CUSTOM_RESOURCE_TO_ROOM);
		act(cust ? cust : "$n finds $p!", TRUE, ch, obj, 0, TO_ROOM);
	}
	else {
		msg_to_char(ch, "You find nothing.\r\n");
	}
	
	return TRUE;
}


INTERACTION_FUNC(finish_gathering) {
	obj_data *obj = NULL;
	char *cust;
	int iter;
	
	for (iter = 0; iter < interaction->quantity; ++iter) {
		obj = read_object(interaction->vnum, TRUE);
		scale_item_to_level(obj, 1);	// minimum level
		obj_to_char_or_room(obj, ch);
		load_otrigger(obj);
		add_depletion(IN_ROOM(ch), DPLTN_GATHER, TRUE);
	}
	
	// mark gained
	if (GET_LOYALTY(ch)) {
		add_production_total(GET_LOYALTY(ch), interaction->vnum, interaction->quantity);
	}
	
	if (obj) {
		cust = obj_get_custom_message(obj, OBJ_CUSTOM_RESOURCE_TO_CHAR);
		if (interaction->quantity > 1) {
			sprintf(buf, "%s (x%d)", cust ? cust : "You find $p!", interaction->quantity);
		}
		else {
			strcpy(buf, cust ? cust : "You find $p!");
		}
		act(buf, FALSE, ch, obj, NULL, TO_CHAR);
		
		cust = obj_get_custom_message(obj, OBJ_CUSTOM_RESOURCE_TO_ROOM);
		act(cust ? cust : "$n finds $p!", TRUE, ch, obj, NULL, TO_ROOM);
		
		gain_player_tech_exp(ch, PTECH_GATHER, 10);
		
		// action does not end normally
		
		return TRUE;
	}
	
	return FALSE;
}


INTERACTION_FUNC(finish_harvesting) {
	crop_data *cp;
	int count, num;
	obj_data *obj = NULL;
	char *cust;
		
	if ((cp = ROOM_CROP(inter_room)) ) {
		// how many to get
		num = interaction->quantity * (has_player_tech(ch, PTECH_HARVEST_UPGRADE) ? 2 : 1);
		
		// give them over
		for (count = 0; count < num; ++count) {
			obj = read_object(interaction->vnum, TRUE);
			scale_item_to_level(obj, 1);	// minimum level
			obj_to_char_or_room(obj, ch);
			load_otrigger(obj);
		}
		
		// mark gained
		if (GET_LOYALTY(ch)) {
			add_production_total(GET_LOYALTY(ch), interaction->vnum, num);
		}
		
		// info messaging
		if (obj) {
			cust = obj_get_custom_message(obj, OBJ_CUSTOM_RESOURCE_TO_CHAR);
			if (num > 1) {
				sprintf(buf, "%s (x%d)", cust ? cust : "You got $p!", num);
			}
			else {
				strcpy(buf, cust ? cust : "You got $p!");
			}
			act(buf, FALSE, ch, obj, FALSE, TO_CHAR);
			
			cust = obj_get_custom_message(obj, OBJ_CUSTOM_RESOURCE_TO_ROOM);
			act(cust ? cust : "$n gets $p!", FALSE, ch, obj, NULL, TO_ROOM);
		}
	}
	else {
		msg_to_char(ch, "The crop appears to have died and you get nothing useful.\r\n");
	}
	
	return TRUE;
}


INTERACTION_FUNC(finish_mining) {
	bool any = FALSE;
	obj_data *obj;
	char *cust;
	int iter;
	
	for (iter = 0; iter < interaction->quantity; ++iter) {
		obj = read_object(interaction->vnum, TRUE);
		scale_item_to_level(obj, 1);	// ensure scaling
		obj_to_char_or_room(obj, ch);
		
		cust = obj_get_custom_message(obj, OBJ_CUSTOM_RESOURCE_TO_CHAR);
		act(cust ? cust : "With that last stroke, $p falls from the wall!", FALSE, ch, obj, NULL, TO_CHAR);
		
		cust = obj_get_custom_message(obj, OBJ_CUSTOM_RESOURCE_TO_ROOM);
		act(cust ? cust : "With $s last stroke, $p falls from the wall where $n was picking!", FALSE, ch, obj, NULL, TO_ROOM);
		
		GET_ACTION(ch) = ACT_NONE;
		load_otrigger(obj);
		any = TRUE;
	}
	
	// mark gained
	if (GET_LOYALTY(ch)) {
		add_production_total(GET_LOYALTY(ch), interaction->vnum, interaction->quantity);
	}
	
	return any;
}


INTERACTION_FUNC(finish_panning) {
	char buf[MAX_STRING_LENGTH];
	obj_data *obj = NULL;
	char *cust;
	int num;
	
	for (num = 0; num < interaction->quantity; ++num) {
		obj = read_object(interaction->vnum, TRUE);
		scale_item_to_level(obj, 1);	// minimum level
		obj_to_char(obj, ch);
		load_otrigger(obj);
	}
	
	// mark gained
	if (GET_LOYALTY(ch)) {
		add_production_total(GET_LOYALTY(ch), interaction->vnum, interaction->quantity);
	}
	
	// messaging
	if (obj) {
		cust = obj_get_custom_message(obj, OBJ_CUSTOM_RESOURCE_TO_CHAR);
		if (interaction->quantity > 1) {
			sprintf(buf, "%s (x%d)", cust ? cust : "You find $p!", interaction->quantity);
			act(buf, FALSE, ch, obj, NULL, TO_CHAR);
		}
		else {
			act(cust ? cust : "You find $p!", FALSE, ch, obj, NULL, TO_CHAR);
		}
		
		cust = obj_get_custom_message(obj, OBJ_CUSTOM_RESOURCE_TO_ROOM);
		act(cust ? cust : "$n finds $p!", FALSE, ch, obj, NULL, TO_ROOM);
	}
	
	return TRUE;
}


// also used for sawing, tanning, chipping
INTERACTION_FUNC(finish_scraping) {
	obj_vnum vnum = interaction->vnum;
	char buf[MAX_STRING_LENGTH];
	obj_data *load = NULL;
	char *cust;
	int num;
	
	for (num = 0; num < interaction->quantity; ++num) {
		// load
		load = read_object(vnum, TRUE);
		scale_item_to_level(load, GET_ACTION_VNUM(ch, 1));
		
		// ownership
		load->last_owner_id = GET_IDNUM(ch);
		load->last_empire_id = GET_LOYALTY(ch) ? EMPIRE_VNUM(GET_LOYALTY(ch)) : NOTHING;
		
		// put it somewhere
		if (CAN_WEAR(load, ITEM_WEAR_TAKE)) {
			obj_to_char(load, ch);
		}
		else {
			obj_to_room(load, IN_ROOM(ch));
		}
		load_otrigger(load);
	}
	
	// mark gained
	if (GET_LOYALTY(ch)) {
		add_production_total(GET_LOYALTY(ch), interaction->vnum, interaction->quantity);
	}
	
	if (load) {
		cust = obj_get_custom_message(load, OBJ_CUSTOM_RESOURCE_TO_CHAR);
		if (interaction->quantity > 1) {
			sprintf(buf, "%s (x%d)", cust ? cust : "You get $p.", interaction->quantity);
		}
		else {
			strcpy(buf, cust ? cust : "You get $p.");
		}
		act(buf, FALSE, ch, load, NULL, TO_CHAR);
		
		cust = obj_get_custom_message(load, OBJ_CUSTOM_RESOURCE_TO_ROOM);
		act(cust ? cust : "$n gets $p.", TRUE, ch, load, NULL, TO_ROOM);
	}
	
	return TRUE;
}


 //////////////////////////////////////////////////////////////////////////////
//// ACTION PROCESSES ////////////////////////////////////////////////////////

// for do_saw
void perform_saw(char_data *ch) {
	char buf[MAX_STRING_LENGTH];
	bool success = FALSE, room = FALSE;
	obj_data *proto, *saw;
	
	// check both of these because they both have bonuses
	room = room_has_function_and_city_ok(GET_LOYALTY(ch), IN_ROOM(ch), FNC_SAW);
	saw = has_tool(ch, TOOL_SAW);
	
	if (!has_player_tech(ch, PTECH_SAW_COMMAND)) {
		msg_to_char(ch, "You don't have the right ability to saw anything.\r\n");
		cancel_action(ch);
		return;
	}
	if (!room && !saw) {
		msg_to_char(ch, "You need to use a saw of some kind to do that.\r\n");
		cancel_action(ch);
		return;
	}
	if (!can_see_in_dark_room(ch, IN_ROOM(ch), TRUE)) {
		msg_to_char(ch, "It's too dark to finish sawing.\r\n");
		cancel_action(ch);
		return;
	}
	
	// base
	GET_ACTION_TIMER(ch) -= 1;
	
	// faster at a lumber mill OR if the tool is superior, and with the ptech
	if (room || (saw && OBJ_FLAGGED(saw, OBJ_SUPERIOR))) {
		GET_ACTION_TIMER(ch) -= 1;
	}
	if (has_player_tech(ch, PTECH_FAST_WOOD_PROCESSING)) {
		GET_ACTION_TIMER(ch) -= 1;
	}
		
	if (GET_ACTION_TIMER(ch) <= 0) {
		// will extract no matter what happens here
		if ((proto = obj_proto(GET_ACTION_VNUM(ch, 0)))) {
			act("You finish sawing $p.", FALSE, ch, proto, NULL, TO_CHAR);
			act("$n finishes sawing $p.", TRUE, ch, proto, NULL, TO_ROOM);
			
			success = run_interactions(ch, GET_OBJ_INTERACTIONS(proto), INTERACT_SAW, IN_ROOM(ch), NULL, proto, NULL, finish_scraping);
		}
		
		if (!success && !proto) {
			snprintf(buf, sizeof(buf), "You finish sawing %s but get nothing.", get_obj_name_by_proto(GET_ACTION_VNUM(ch, 0)));
			act(buf, FALSE, ch, NULL, NULL, TO_CHAR);
			snprintf(buf, sizeof(buf), "$n finishes sawing off %s.", get_obj_name_by_proto(GET_ACTION_VNUM(ch, 0)));
			act(buf, TRUE, ch, NULL, NULL, TO_ROOM);
		}
		
		GET_ACTION(ch) = ACT_NONE;
		free_resource_list(GET_ACTION_RESOURCES(ch));
		GET_ACTION_RESOURCES(ch) = NULL;
		
		if (success && proto) {
			gain_player_tech_exp(ch, PTECH_SAW_COMMAND, 10);
			
			// lather, rinse, rescrape
			do_saw(ch, fname(GET_OBJ_KEYWORDS(proto)), 0, 0);
		}
	}
	else if (!PRF_FLAGGED(ch, PRF_NOSPAM)) {
		// message last, only if they didn't finish
		msg_to_char(ch, "You saw %s...\r\n", get_obj_name_by_proto(GET_ACTION_VNUM(ch, 0)));
	}
}


/**
* Tick update for bathing action.
*
* @param char_data *ch The bather.
*/
void process_bathing(char_data *ch) {
	// can still bathe here?
	if (!room_has_function_and_city_ok(GET_LOYALTY(ch), IN_ROOM(ch), FNC_BATHS) && !ROOM_SECT_FLAGGED(IN_ROOM(ch), SECTF_FRESH_WATER | SECTF_SHALLOW_WATER)) {
		cancel_action(ch);
		return;
	}
	
	if (GET_ACTION_TIMER(ch) <= 0) {
		// finish
		msg_to_char(ch, "You finish bathing and climb out of the water to dry off.\r\n");
		act("$n finishes bathing and climbs out of the water to dry off.", FALSE, ch, 0, 0, TO_ROOM);
		GET_ACTION(ch) = ACT_NONE;
	}
	else {
		// decrement
		GET_ACTION_TIMER(ch) -= 1;
		
		// messaging
		switch (number(0, 2)) {
			case 0: {
				if (!PRF_FLAGGED(ch, PRF_NOSPAM)) {
					msg_to_char(ch, "You wash yourself off...\r\n");
				}
				act("$n washes $mself carefully...", FALSE, ch, 0, 0, TO_ROOM | TO_SPAMMY);
				break;
			}
			case 1: {
				if (!PRF_FLAGGED(ch, PRF_NOSPAM)) {
					msg_to_char(ch, "You scrub your hair to get out any dirt and insects...\r\n");
				}
				act("$n scrubs $s hair to get out any dirt and insects...", FALSE, ch, 0, 0, TO_ROOM | TO_SPAMMY);
				break;
			}
			case 2: {
				if (!PRF_FLAGGED(ch, PRF_NOSPAM)) {
					msg_to_char(ch, "You swim through the water...\r\n");
				}
				act("$n swims through the water...", FALSE, ch, 0, 0, TO_ROOM | TO_SPAMMY);
				break;
			}
		}
	}
}


/**
* Tick update for build action.
*
* @param char_data *ch The builder.
*/
void process_build_action(char_data *ch) {
	char buf1[MAX_STRING_LENGTH];
	craft_data *type = NULL;
	
	if (!can_see_in_dark_room(ch, IN_ROOM(ch), TRUE)) {
		msg_to_char(ch, "It's too dark to keep working here.\r\n");
		cancel_action(ch);
		return;
	}
	
	// check for build recipe
	type = find_building_list_entry(IN_ROOM(ch), FIND_BUILD_NORMAL);
	
	// ensure tools are still present
	if (type && GET_CRAFT_REQUIRES_TOOL(type) && !has_all_tools(ch, GET_CRAFT_REQUIRES_TOOL(type))) {
		prettier_sprintbit(GET_CRAFT_REQUIRES_TOOL(type), tool_flags, buf1);
		if (count_bits(GET_CRAFT_REQUIRES_TOOL(type)) > 1) {
			msg_to_char(ch, "You need the following tools to work on this building: %s\r\n", buf1);
		}
		else {
			msg_to_char(ch, "You need %s %s to work on this building.\r\n", AN(buf1), buf1);
		}
		cancel_action(ch);
		return;
	}

	process_build(ch, IN_ROOM(ch), ACT_BUILDING);
}


/**
* Tick update for burn area / do_burn_area.
*
* @param char_data *ch The person burning the area.
*/
void process_burn_area(char_data *ch) {
	if (!validate_burn_area(ch, GET_ACTION_VNUM(ch, 0))) {
		// sends own message
		cancel_action(ch);
		return;
	}
	
	GET_ACTION_TIMER(ch) -= 1;
	
	if (GET_ACTION_TIMER(ch) > 0) {
		act("You prepare to burn the area...", FALSE, ch, NULL, NULL, TO_CHAR | TO_SPAMMY);
		act("$n prepares to burn the area...", FALSE, ch, NULL, NULL, TO_ROOM | TO_SPAMMY);
	}
	else {	// done!
		bool objless = has_player_tech(ch, PTECH_LIGHT_FIRE);
		obj_data *lighter = NULL;
		bool kept = FALSE;
	
		if (!objless) {	// find lighter if needed
			lighter = find_lighter_in_list(ch->carrying, &kept);
		}
		
		// messaging
		if (lighter) {
			act("You use $p to light some fires!", FALSE, ch, lighter, NULL, TO_CHAR);
			act("$n uses $p to light some fires!", FALSE, ch, lighter, NULL, TO_ROOM);
		}
		else {
			act("You light some fires!", FALSE, ch, NULL, NULL, TO_CHAR);
			act("$n lights some fires!", FALSE, ch, NULL, NULL, TO_ROOM);
			gain_player_tech_exp(ch, PTECH_LIGHT_FIRE, 15);
		}
		
		// finished burning
		perform_burn_room(IN_ROOM(ch));
		cancel_action(ch);
		stop_room_action(IN_ROOM(ch), ACT_BURN_AREA);
		
		if (lighter) {
			used_lighter(ch, lighter);
		}
	}
}


/**
* Tick update for chip action.
*
* @param char_data *ch The chipper one.
*/
void process_chipping(char_data *ch) {
	obj_data *proto;
	bool success;
	
	if (!has_player_tech(ch, PTECH_CHIP_COMMAND)) {
		msg_to_char(ch, "You don't have the right ability to chip anything.\r\n");
		cancel_action(ch);
		return;
	}
	if (!has_tool(ch, TOOL_KNAPPER)) {
		msg_to_char(ch, "You need to be using a rock or other knapping tool to chip it.\r\n");
		cancel_action(ch);
		return;
	}
	if (!can_see_in_dark_room(ch, IN_ROOM(ch), TRUE)) {
		msg_to_char(ch, "It's too dark to finish chipping.\r\n");
		cancel_action(ch);
		return;
	}
	if (!(proto = obj_proto(GET_ACTION_VNUM(ch, 0)))) {
		// obj deleted?
		cancel_action(ch);
		return;
	}
	
	GET_ACTION_TIMER(ch) -= 1;
			
	if (!PRF_FLAGGED(ch, PRF_NOSPAM)) {
		act("You chip away at $p...", FALSE, ch, proto, NULL, TO_CHAR | TO_SPAMMY);
	}

	if (GET_ACTION_TIMER(ch) <= 0) {
		act("$p splits open!", FALSE, ch, proto, NULL, TO_CHAR);
		act("$n finishes chipping $p!", TRUE, ch, proto, NULL, TO_ROOM);
		GET_ACTION(ch) = ACT_NONE;
		
		success = run_interactions(ch, GET_OBJ_INTERACTIONS(proto), INTERACT_CHIP, IN_ROOM(ch), NULL, proto, NULL, finish_scraping);
		free_resource_list(GET_ACTION_RESOURCES(ch));
		GET_ACTION_RESOURCES(ch) = NULL;

		if (success) {
			gain_player_tech_exp(ch, PTECH_CHIP_COMMAND, 25);
			
			// repeat! (no -paul) note: keyword-targeting is hard because "chipped flint" also has "flint" as an alias
			// do_chip(ch, fname(GET_OBJ_KEYWORDS(proto)), 0, 0);
		}
	}
}


/**
* Run one chopping action for ch.
*
* @param char_data *ch The chopper.
*/
void process_chop(char_data *ch) {
	bool got_any = FALSE;
	char_data *ch_iter;
	obj_data *axe;
	int amt;
	
	const int min_progress_per_chop = 3;	// TODO this could be a config or related to the 'chop_timer' config; prevents it from taking too long
	
	if (!(axe = has_tool(ch, TOOL_AXE))) {
		send_to_char("You need to be using an axe to chop.\r\n", ch);
		cancel_action(ch);
		return;
	}
	if (!can_see_in_dark_room(ch, IN_ROOM(ch), TRUE)) {
		msg_to_char(ch, "It's too dark to finish chopping.\r\n");
		cancel_action(ch);
		return;
	}

	amt = round(GET_OBJ_CURRENT_SCALE_LEVEL(axe) / 10.0) * (OBJ_FLAGGED(axe, OBJ_SUPERIOR) ? 2 : 1);
	amt = MAX(min_progress_per_chop, amt);

	add_to_room_extra_data(IN_ROOM(ch), ROOM_EXTRA_CHOP_PROGRESS, -1 * amt);
	act("You swing $p hard!", FALSE, ch, axe, NULL, TO_CHAR | TO_SPAMMY);
	act("$n swings $p hard!", FALSE, ch, axe, NULL, TO_ROOM | TO_SPAMMY);
	
	// complete?
	if (get_room_extra_data(IN_ROOM(ch), ROOM_EXTRA_CHOP_PROGRESS) <= 0) {
		remove_room_extra_data(IN_ROOM(ch), ROOM_EXTRA_CHOP_PROGRESS);
		
		// run interacts for items only if not depleted
		if (get_depletion(IN_ROOM(ch), DPLTN_CHOP) < config_get_int("chop_depletion")) {
			got_any = run_room_interactions(ch, IN_ROOM(ch), INTERACT_CHOP, NULL, GUESTS_ALLOWED, finish_chopping);
		}
		
		if (!got_any) {
			// likely didn't get a completion message
			act("You finish chopping.", FALSE, ch, NULL, NULL, TO_CHAR);
			act("$n finishes chopping.", TRUE, ch, NULL, NULL, TO_ROOM);
		}
		
		// attempt to change terrain
		change_chop_territory(IN_ROOM(ch));
		
		gain_player_tech_exp(ch, PTECH_CHOP, 15);
		
		// stoppin choppin -- don't use stop_room_action because we also restart them
		// (this includes ch)
		DL_FOREACH2(ROOM_PEOPLE(IN_ROOM(ch)), ch_iter, next_in_room) {
			if (!IS_NPC(ch_iter) && GET_ACTION(ch_iter) == ACT_CHOPPING) {
				cancel_action(ch_iter);
				start_chopping(ch_iter);
			}
		}
	}
}


/**
* Tick update for digging action.
*
* @param char_data *ch The digger.
*/
void process_digging(char_data *ch) {
	room_data *in_room;
	char_data *iter;
	
	if (!can_see_in_dark_room(ch, IN_ROOM(ch), TRUE)) {
		msg_to_char(ch, "It's too dark to dig here.\r\n");
		cancel_action(ch);
		return;
	}

	// decrement timer
	GET_ACTION_TIMER(ch) -= 1;
		
	// done?
	if (GET_ACTION_TIMER(ch) <= 0) {
		GET_ACTION(ch) = ACT_NONE;
		in_room = IN_ROOM(ch);
		
		if (get_depletion(IN_ROOM(ch), DPLTN_DIG) < DEPLETION_LIMIT(IN_ROOM(ch)) && run_room_interactions(ch, IN_ROOM(ch), INTERACT_DIG, NULL, GUESTS_ALLOWED, finish_digging)) {
			// success
			gain_player_tech_exp(ch, PTECH_DIG, 10);
		
			// character is still there and not digging?
			if (GET_ACTION(ch) == ACT_NONE && in_room == IN_ROOM(ch)) {
				start_digging(ch);
			}
		}
		else {
			msg_to_char(ch, "You don't seem to be able to find anything to dig for.\r\n");
			cancel_action(ch);
		}
	}
	else {
		// normal tick message
		if (!PRF_FLAGGED(ch, PRF_NOSPAM)) {
			send_to_char("You dig vigorously at the ground.\r\n", ch);
		}
		act("$n digs vigorously at the ground.", FALSE, ch, 0, 0, TO_ROOM | TO_SPAMMY);
	}
	
	// look for earthmelders
	DL_FOREACH2(ROOM_PEOPLE(IN_ROOM(ch)), iter, next_in_room) {
		if (!AFF_FLAGGED(iter, AFF_EARTHMELD)) {
			continue;
		}
		if (iter == ch || IS_IMMORTAL(iter) || IS_NPC(iter) || IS_DEAD(iter) || EXTRACTED(iter)) {
			continue;
		}
		
		// earthmeld damage
		msg_to_char(iter, "You feel nature burning at your earthmelded form as someone digs above you!\r\n");
		apply_dot_effect(iter, ATYPE_NATURE_BURN, 30, DAM_MAGICAL, 5, 60, iter);
	}
}


/**
* Tick update for dismantle action.
*
* @param char_data *ch The dismantler.
*/
void process_dismantle_action(char_data *ch) {
	int count, total;
	
	if (!can_see_in_dark_room(ch, IN_ROOM(ch), TRUE)) {
		msg_to_char(ch, "It's too dark to finish dismantling.\r\n");
		cancel_action(ch);
		return;
	}
	
	total = 1;	// number of mats to dismantle at once (add things that speed up dismantle)
	for (count = 0; count < total && GET_ACTION(ch) == ACT_DISMANTLING; ++count) {
		process_dismantling(ch, IN_ROOM(ch));
	}
}


/**
* Tick update for escape action.
*
* @param char_data *ch The escapist.
*/
void process_escaping(char_data *ch) {
	if (!can_see_in_dark_room(ch, IN_ROOM(ch), TRUE)) {
		msg_to_char(ch, "It's too dark to find your way out!\r\n");
		cancel_action(ch);
		return;
	}
	
	if (--GET_ACTION_TIMER(ch) <= 0) {
		perform_escape(ch);
	}
}


/**
* Tick update for excavate action.
*
* @param char_data *ch The excavator.
*/
void process_excavating(char_data *ch) {
	int count, total;
	char_data *iter;

	total = 1;	// shovelfuls at once (add things that speed up excavate)
	for (count = 0; count < total && GET_ACTION(ch) == ACT_EXCAVATING; ++count) {
		if (!has_tool(ch, TOOL_SHOVEL)) {
			msg_to_char(ch, "You need a shovel to excavate.\r\n");
			cancel_action(ch);
		}
		else if (!ROOM_SECT_FLAGGED(IN_ROOM(ch), SECTF_IS_TRENCH)) {
			msg_to_char(ch, "You stop excavating, as this is no longer a trench.\r\n");
			cancel_action(ch);
		}
		else if (!ROOM_OWNER(IN_ROOM(ch))) {
			msg_to_char(ch, "You can only excavate claimed tiles.\r\n");
			cancel_action(ch);
		}
		else if (!can_use_room(ch, IN_ROOM(ch), GUESTS_ALLOWED)) {
			msg_to_char(ch, "You no longer have permission to excavate here.\r\n");
			cancel_action(ch);
		}
		else if (ROOM_AFF_FLAGGED(IN_ROOM(ch), ROOM_AFF_NO_EVOLVE)) {
			msg_to_char(ch, "You can't excavate here right now.\r\n");
			cancel_action(ch);
		}
		else {
			// count up toward zero
			add_to_room_extra_data(IN_ROOM(ch), ROOM_EXTRA_TRENCH_PROGRESS, 1);
			
			// messaging
			if (!number(0, 1)) {
				if (!PRF_FLAGGED(ch, PRF_NOSPAM)) {
					msg_to_char(ch, "You jab your shovel into the dirt...\r\n");
				}
				act("$n jabs $s shovel into the dirt...", FALSE, ch, 0, 0, TO_ROOM | TO_SPAMMY);
			}
			else {
				if (!PRF_FLAGGED(ch, PRF_NOSPAM)) {
					msg_to_char(ch, "You toss a shovel-full of dirt out of the trench.\r\n");
				}
				act("$n tosses a shovel-full of dirt out of the trench.", FALSE, ch, 0, 0, TO_ROOM | TO_SPAMMY);
			}

			if (get_room_extra_data(IN_ROOM(ch), ROOM_EXTRA_TRENCH_PROGRESS) >= 0) {
				msg_to_char(ch, "You finish excavating the trench!\r\n");
				act("$n finishes excavating the trench!", FALSE, ch, 0, 0, TO_ROOM);
				
				finish_trench(IN_ROOM(ch));
				
				// this also stops ch
				stop_room_action(IN_ROOM(ch), ACT_EXCAVATING);
			}
		}
	}
	
	// look for earthmelders
	DL_FOREACH2(ROOM_PEOPLE(IN_ROOM(ch)), iter, next_in_room) {
		if (!AFF_FLAGGED(iter, AFF_EARTHMELD)) {
			continue;
		}
		if (iter == ch || IS_IMMORTAL(iter) || IS_NPC(iter) || IS_DEAD(iter) || EXTRACTED(iter)) {
			continue;
		}
		
		// earthmeld damage
		msg_to_char(iter, "You feel nature burning at your earthmelded form as someone digs above you!\r\n");
		apply_dot_effect(iter, ATYPE_NATURE_BURN, 30, DAM_MAGICAL, 5, 60, iter);
	}
}


/**
* Tick update for fillin action.
*
* @param char_data *ch The filler-inner.
*/
void process_fillin(char_data *ch) {
	int count, total;

	total = 1;	// shovelfuls to fill in at once (add things that speed up fillin)
	for (count = 0; count < total && GET_ACTION(ch) == ACT_FILLING_IN; ++count) {
		if (!has_tool(ch, TOOL_SHOVEL)) {
			msg_to_char(ch, "You need a shovel to fill in the trench.\r\n");
			cancel_action(ch);
		}
		else if (!ROOM_SECT_FLAGGED(IN_ROOM(ch), SECTF_IS_TRENCH)) {
			msg_to_char(ch, "You stop filling in, as this is no longer a trench.\r\n");
			cancel_action(ch);
		}
		else if (!ROOM_OWNER(IN_ROOM(ch))) {
			msg_to_char(ch, "You can only fill in claimed tiles.\r\n");
			cancel_action(ch);
		}
		else if (!can_use_room(ch, IN_ROOM(ch), GUESTS_ALLOWED)) {
			msg_to_char(ch, "You no longer have permission to fill in here.\r\n");
			cancel_action(ch);
		}
		else if (ROOM_AFF_FLAGGED(IN_ROOM(ch), ROOM_AFF_NO_EVOLVE)) {
			msg_to_char(ch, "You can't fill in here right now.\r\n");
			cancel_action(ch);
		}
		else {
			// opposite of excavate
			add_to_room_extra_data(IN_ROOM(ch), ROOM_EXTRA_TRENCH_PROGRESS, -1);
		
			// mensaje
			if (!number(0, 1)) {
				if (!PRF_FLAGGED(ch, PRF_NOSPAM)) {
					msg_to_char(ch, "You jab your shovel into the dirt...\r\n");
				}
				act("$n jabs $s shovel into the dirt...", FALSE, ch, 0, 0, TO_ROOM | TO_SPAMMY);
			}
			else {
				if (!PRF_FLAGGED(ch, PRF_NOSPAM)) {
					msg_to_char(ch, "You toss a shovel-full of dirt back into the trench.\r\n");
				}
				act("$n tosses a shovel-full of dirt back into the trench.", FALSE, ch, 0, 0, TO_ROOM | TO_SPAMMY);
			}

			// done? only if we've gone negative enough!
			if (get_room_extra_data(IN_ROOM(ch), ROOM_EXTRA_TRENCH_PROGRESS) <= config_get_int("trench_initial_value")) {
				msg_to_char(ch, "You finish filling in the trench!\r\n");
				act("$n finishes filling in the trench!", FALSE, ch, 0, 0, TO_ROOM);
				untrench_room(IN_ROOM(ch));
			}
		}
	}
}


/**
* Manages a fishing tick and completion.
*
* @param char_data *ch The fisher (Bobby?)
*/
void process_fishing(char_data *ch) {
	bool success = FALSE;
	room_data *room;
	obj_data *tool;
	int amt, dir;
	
	dir = GET_ACTION_VNUM(ch, 0);
	room = (dir == NO_DIR) ? IN_ROOM(ch) : dir_to_room(IN_ROOM(ch), dir, FALSE);
	
	if (!(tool = has_tool(ch, TOOL_FISHING))) {
		msg_to_char(ch, "You aren't using any fishing equipment.\r\n");
		cancel_action(ch);
		return;
	}
	if (!room || !can_interact_room(room, INTERACT_FISH) || !can_use_room(ch, room, MEMBERS_ONLY)) {
		msg_to_char(ch, "You can no longer fish %s.\r\n", (room == IN_ROOM(ch)) ? "here" : "there");
		cancel_action(ch);
		return;
	}
	if (!can_see_in_dark_room(ch, IN_ROOM(ch), TRUE)) {
		msg_to_char(ch, "It's too dark to fish here.\r\n");
		cancel_action(ch);
		return;
	}
	
	amt = (GET_OBJ_CURRENT_SCALE_LEVEL(tool) / 20) + (player_tech_skill_check(ch, PTECH_FISH, DIFF_MEDIUM) ? 2 : 0);
	GET_ACTION_TIMER(ch) -= MAX(1, amt);
	
	if (GET_ACTION_TIMER(ch) > 0) {
		switch (number(0, 10)) {
			case 0: {
				if (!PRF_FLAGGED(ch, PRF_NOSPAM)) {
					msg_to_char(ch, "A fish darts past you, but you narrowly miss it!\r\n");
				}
				break;
			}
			case 1: {
				if (!PRF_FLAGGED(ch, PRF_NOSPAM)) {
					msg_to_char(ch, "The water waves peacefully, but you don't see any fish...\r\n");
				}
				break;
			}
			case 2: {
				if (!PRF_FLAGGED(ch, PRF_NOSPAM)) {
					msg_to_char(ch, "The fish are jumping off in the distance, but you can't seem to catch one!\r\n");
				}
				break;
			}
		}
	}
	else if (get_depletion(room, DPLTN_FISH) >= DEPLETION_LIMIT(room)) {
		msg_to_char(ch, "You just don't seem to be able to catch anything here.\r\n");
		GET_ACTION(ch) = ACT_NONE;
	}
	else {
		// SUCCESS
		msg_to_char(ch, "A fish darts past you...\r\n");
		success = run_room_interactions(ch, room, INTERACT_FISH, NULL, GUESTS_ALLOWED, finish_fishing);
		
		if (success) {
			add_depletion(room, DPLTN_FISH, TRUE);
		}
		else {
			msg_to_char(ch, "You can't seem to catch anything.\r\n");
		}
		
		gain_player_tech_exp(ch, PTECH_FISH, 15);
		
		// restart action
		start_action(ch, ACT_FISHING, config_get_int("fishing_timer") / (player_tech_skill_check(ch, PTECH_FISH, DIFF_EASY) ? 2 : 1));
		GET_ACTION_VNUM(ch, 0) = dir;
	}
}


/**
* Tick updates for foraging.
*
* @param char_data *ch The forager.
*/
void process_foraging(char_data *ch) {
	room_data *in_room = IN_ROOM(ch);
	bool found = FALSE;
	
	int forage_depletion = config_get_int("short_depletion");
	
	if (!IS_COMPLETE(IN_ROOM(ch))) {
		msg_to_char(ch, "You can't forage anything in an incomplete building.\r\n");
		cancel_action(ch);
		return;
	}
	if (!can_see_in_dark_room(ch, IN_ROOM(ch), TRUE)) {
		msg_to_char(ch, "It's too dark to forage for anything.\r\n");
		cancel_action(ch);
		return;
	}
	
	if (!PRF_FLAGGED(ch, PRF_NOSPAM)) {
		send_to_char("You look around for something to eat...\r\n", ch);
	}
	// no room message

	// decrement
	GET_ACTION_TIMER(ch) -= 1;
		
	if (GET_ACTION_TIMER(ch) <= 0) {
		GET_ACTION(ch) = ACT_NONE;
		
		// forage triggers
		if (run_ability_triggers_by_player_tech(ch, PTECH_FORAGE, NULL, NULL)) {
			return;
		}
		
		if (get_depletion(IN_ROOM(ch), DPLTN_FORAGE) >= forage_depletion) {
			msg_to_char(ch, "You can't find anything left to eat here.\r\n");
			act("$n stops looking for things to eat as $e comes up empty-handed.", TRUE, ch, NULL, NULL, TO_ROOM);
		}
		else {	// success
			if (run_room_interactions(ch, IN_ROOM(ch), INTERACT_FORAGE, NULL, GUESTS_ALLOWED, finish_foraging)) {
				gain_player_tech_exp(ch, PTECH_FORAGE, 10);
				found = TRUE;
			}
			else if (do_crop_forage(ch)) {
				gain_player_tech_exp(ch, PTECH_FORAGE, 10);
				found = TRUE;
			}
			
			if (found && IN_ROOM(ch) == in_room) {
				start_foraging(ch);
			}
			else if (!found) {
				msg_to_char(ch, "You couldn't find anything to eat here.\r\n");
			}
		}
	}
}


/**
* Tick update for gather action.
*
* @param char_data *ch The gatherer.
*/
void process_gathering(char_data *ch) {
	int gather_base_timer = config_get_int("gather_base_timer");
	int gather_depletion = config_get_int("gather_depletion");
	
	if (!PRF_FLAGGED(ch, PRF_NOSPAM)) {
		send_to_char("You search the ground for useful material...\r\n", ch);
	}
	act("$n searches around on the ground...", TRUE, ch, 0, 0, TO_ROOM | TO_SPAMMY);
	GET_ACTION_TIMER(ch) -= 1;
		
	// done ?
	if (GET_ACTION_TIMER(ch) <= 0) {
		if (get_depletion(IN_ROOM(ch), DPLTN_GATHER) >= gather_depletion) {
			msg_to_char(ch, "There's nothing good left to gather here.\r\n");
			GET_ACTION(ch) = ACT_NONE;
		}
		else {
			if (run_room_interactions(ch, IN_ROOM(ch), INTERACT_GATHER, NULL, GUESTS_ALLOWED, finish_gathering)) {
				// check repeatability
				if (can_interact_room(IN_ROOM(ch), INTERACT_GATHER)) {
					GET_ACTION_TIMER(ch) = gather_base_timer;
				}
				else {
					GET_ACTION(ch) = ACT_NONE;
				}
			}
			else {
				msg_to_char(ch, "You don't seem to be able to gather anything here.\r\n");
				GET_ACTION(ch) = ACT_NONE;
			}
		}
	}
}


/**
* Tick update for harvest action.
*
* @param char_data *ch The harvester.
*/
void process_harvesting(char_data *ch) {
	if (!ROOM_CROP(IN_ROOM(ch)) || !can_interact_room(IN_ROOM(ch), INTERACT_HARVEST)) {
		msg_to_char(ch, "There's nothing left to harvest here.\r\n");
		cancel_action(ch);
		return;
	}
	if (!has_tool(ch, TOOL_HARVESTING)) {
		send_to_char("You're not using the proper tool for harvesting.\r\n", ch);
		cancel_action(ch);
		return;
	}
	if (!can_see_in_dark_room(ch, IN_ROOM(ch), TRUE)) {
		msg_to_char(ch, "It's too dark to finish harvesting.\r\n");
		cancel_action(ch);
		return;
	}

	// tick messaging
	switch (number(0, 2)) {
		case 0: {
			if (!PRF_FLAGGED(ch, PRF_NOSPAM)) {
				msg_to_char(ch, "You walk through the field, harvesting the %s.\r\n", GET_CROP_NAME(ROOM_CROP(IN_ROOM(ch))));
			}
			sprintf(buf, "$n walks through the field, harvesting the %s.", GET_CROP_NAME(ROOM_CROP(IN_ROOM(ch))));
			act(buf, FALSE, ch, 0, 0, TO_ROOM | TO_SPAMMY);
			break;
		}
		case 1: {
			if (!PRF_FLAGGED(ch, PRF_NOSPAM)) {
				msg_to_char(ch, "You carefully harvest the %s.\r\n", GET_CROP_NAME(ROOM_CROP(IN_ROOM(ch))));
			}
			sprintf(buf, "$n carefully harvests the %s.", GET_CROP_NAME(ROOM_CROP(IN_ROOM(ch))));
			act(buf, FALSE, ch, 0, 0, TO_ROOM | TO_SPAMMY);
			break;
		}
	}
	
	// update progress (on the room, not the player's action timer)
	add_to_room_extra_data(IN_ROOM(ch), ROOM_EXTRA_HARVEST_PROGRESS, -1 * (1 + GET_STRENGTH(ch)));

	if (get_room_extra_data(IN_ROOM(ch), ROOM_EXTRA_HARVEST_PROGRESS) <= 0) {
		// lower limit
		remove_room_extra_data(IN_ROOM(ch), ROOM_EXTRA_HARVEST_PROGRESS);

		// stop all harvesters including ch
		stop_room_action(IN_ROOM(ch), ACT_HARVESTING);
		
		act("You finish harvesting the crop!", FALSE, ch, NULL, NULL, TO_CHAR);
		act("$n finished harvesting the crop!", FALSE, ch, NULL, NULL, TO_ROOM);
		
		if (run_room_interactions(ch, IN_ROOM(ch), INTERACT_HARVEST, NULL, GUESTS_ALLOWED, finish_harvesting)) {
			// skillups
			gain_player_tech_exp(ch, PTECH_HARVEST, 30);
			gain_player_tech_exp(ch, PTECH_HARVEST_UPGRADE, 5);
		}
		else {
			msg_to_char(ch, "You fail to harvest anything here.\r\n");
		}
		
		// change the sector:
		uncrop_tile(IN_ROOM(ch));
	}
}


/**
* Tick update for hunt action.
*
* @param char_data *ch The hunter.
*/
void process_hunting(char_data *ch) {
	struct affected_type *af;
	char_data *mob;
	
	// from stored data
	any_vnum mob_vnum = GET_ACTION_VNUM(ch, 0);
	int chance_times_100 = GET_ACTION_VNUM(ch, 1);
	
	if (!can_see_in_dark_room(ch, IN_ROOM(ch), TRUE)) {
		msg_to_char(ch, "It's too dark to keep hunting now.\r\n");
		cancel_action(ch);
		return;
	}
	
	if (number(1, 10000) <= chance_times_100) {
		// found it!
		
		if (get_depletion(IN_ROOM(ch), DPLTN_HUNT) >= config_get_int("short_depletion")) {
			// late check for depletion: make them hunt first
			msg_to_char(ch, "You don't seem to be able to find any. Maybe this area has been hunted to depletion.\r\n");
			GET_ACTION(ch) = ACT_NONE;
			return;
		}
		
		mob = read_mobile(mob_vnum, TRUE);
		
		// basic setup
		setup_generic_npc(mob, NULL, NOTHING, NOTHING);
		char_to_room(mob, IN_ROOM(ch));
		
		// scale it to the hunter's level and flag it to keep it there
		scale_mob_for_character(mob, ch);
		set_mob_flags(mob, MOB_NO_RESCALE | MOB_SPAWNED);
		
		// messaging
		act("You've found $N!", FALSE, ch, NULL, mob, TO_CHAR);
		act("$n has found $N!", FALSE, ch, NULL, mob, TO_NOTVICT);
		
		load_mtrigger(mob);
		
		// stun it if triggers allow
		if (!run_ability_triggers_by_player_tech(ch, PTECH_HUNT_ANIMALS, mob, NULL)) {
			af = create_flag_aff(ATYPE_HUNTED, 5, AFF_IMMOBILIZED, ch);
			affect_join(mob, af, 0);
		}
		
		GET_ACTION(ch) = ACT_NONE;
		gain_player_tech_exp(ch, PTECH_HUNT_ANIMALS, 10);
		add_depletion(IN_ROOM(ch), DPLTN_HUNT, TRUE);
	}
	else {
		// tick messaging
		if (!number(0, 2)) {
			act("You stalk low to the ground, hunting...", FALSE, ch, NULL, NULL, TO_CHAR | TO_SPAMMY);
		}
		
		// chance to raise depletion anyway (the hunter is scaring off game)
		if (!number(0, 5)) {
			add_depletion(IN_ROOM(ch), DPLTN_HUNT, FALSE);
		}
	}
}


/**
* Tick update for maintenance.
*
* @param char_data *ch The repairman.
*/
void process_maintenance(char_data *ch) {
	if (!can_see_in_dark_room(ch, IN_ROOM(ch), TRUE)) {
		msg_to_char(ch, "It's too dark to maintain the building.\r\n");
		cancel_action(ch);
		return;
	}
	
	process_build(ch, IN_ROOM(ch), ACT_MAINTENANCE);
}


/**
* Tick update for mine action.
*
* @param char_data *ch The miner.
*/
void process_mining(char_data *ch) {	
	struct global_data *glb;
	int count, total, amt;
	room_data *in_room;
	obj_data *tool;
	bool success;
	
	int min_progress_per_mine = 4;	// TODO this could be a config or related to the 'mining_timer' config; prevents it from taking too long
	
	if (!room_has_function_and_city_ok(GET_LOYALTY(ch), IN_ROOM(ch), FNC_MINE)) {
		msg_to_char(ch, "You can't mine here.\r\n");
		cancel_action(ch);
		return;
	}
	if (!can_see_in_dark_room(ch, IN_ROOM(ch), TRUE)) {
		msg_to_char(ch, "It's too dark to finish mining.\r\n");
		cancel_action(ch);
		return;
	}

	total = 1;	// pick swings per tick
	for (count = 0; count < total && GET_ACTION(ch) == ACT_MINING; ++count) {
		if (!(tool = has_tool(ch, TOOL_MINING))) {
			send_to_char("You're not using the proper tool for mining!\r\n", ch);
			cancel_action(ch);
			break;
		}
		if (!room_has_function_and_city_ok(GET_LOYALTY(ch), IN_ROOM(ch), FNC_MINE)) {
			msg_to_char(ch, "You can't mine here.\r\n");
			cancel_action(ch);
			break;
		}
		if (get_room_extra_data(IN_ROOM(ch), ROOM_EXTRA_MINE_AMOUNT) <= 0) {
			msg_to_char(ch, "You don't see even a hint of anything worthwhile in this depleted mine.\r\n");
			cancel_action(ch);
			break;
		}
		
		amt = round(GET_OBJ_CURRENT_SCALE_LEVEL(tool) / 6.66) * (OBJ_FLAGGED(tool, OBJ_SUPERIOR) ? 2 : 1);
		amt = MAX(min_progress_per_mine, amt);
		
		GET_ACTION_TIMER(ch) -= amt;

		act("You pick at the walls with $p, looking for ore.", FALSE, ch, tool, 0, TO_CHAR | TO_SPAMMY);
		act("$n picks at the walls with $p, looking for ore.", FALSE, ch, tool, 0, TO_ROOM | TO_SPAMMY);

		// done??
		if (GET_ACTION_TIMER(ch) <= 0) {
			in_room = IN_ROOM(ch);
			
			// amount of ore remaining
			add_to_room_extra_data(IN_ROOM(ch), ROOM_EXTRA_MINE_AMOUNT, -1);
			
			// set as prospected
			if (GET_LOYALTY(ch) && (!ROOM_OWNER(IN_ROOM(ch)) || GET_LOYALTY(ch) == ROOM_OWNER(IN_ROOM(ch)))) {
				set_room_extra_data(IN_ROOM(ch), ROOM_EXTRA_PROSPECT_EMPIRE, EMPIRE_VNUM(GET_LOYALTY(ch)));
			}
			
			glb = global_proto(get_room_extra_data(IN_ROOM(ch), ROOM_EXTRA_MINE_GLB_VNUM));
			if (!glb || GET_GLOBAL_TYPE(glb) != GLOBAL_MINE_DATA) {
				msg_to_char(ch, "You can't seem to mine here.\r\n");
				cancel_action(ch);
				break;
			}
			
			// attempt to mine it
			success = run_interactions(ch, GET_GLOBAL_INTERACTIONS(glb), INTERACT_MINE, IN_ROOM(ch), NULL, NULL, NULL, finish_mining);
			
			if (success && in_room == IN_ROOM(ch)) {
				// skillups
				if (GET_GLOBAL_ABILITY(glb) != NO_ABIL) {
					gain_ability_exp(ch, GET_GLOBAL_ABILITY(glb), 10);
				}
				
				// go again! (if ch is still there)
				if (in_room == IN_ROOM(ch)) {
					start_mining(ch);
				}
			}
			
			// stop all miners
			if (get_room_extra_data(in_room, ROOM_EXTRA_MINE_AMOUNT) <= 0) {
				stop_room_action(in_room, ACT_MINING);
			}
		}
	}
}


/**
* Tick update for mint action.
*
* @param char_data *ch The guy minting.
*/
void process_minting(char_data *ch) {
	empire_data *emp = ROOM_OWNER(IN_ROOM(ch));
	char tmp[MAX_INPUT_LENGTH];
	obj_data *proto;
	int num;
	
	if (!emp) {
		msg_to_char(ch, "The mint no longer belongs to any empire and can't be used to make coin.\r\n");
		cancel_action(ch);
		return;
	}
	if (!room_has_function_and_city_ok(GET_LOYALTY(ch), IN_ROOM(ch), FNC_MINT)) {
		msg_to_char(ch, "You can't mint anything here.\r\n");
		cancel_action(ch);
		return;
	}
	if (!can_see_in_dark_room(ch, IN_ROOM(ch), TRUE)) {
		msg_to_char(ch, "It's too dark to finish minting.\r\n");
		cancel_action(ch);
		return;
	}
	
	GET_ACTION_TIMER(ch) -= 1;
	
	if (GET_ACTION_TIMER(ch) > 0) {
		if (!number(0, 1)) {
			act("You turn the coin mill...", FALSE, ch, NULL, NULL, TO_CHAR | TO_SPAMMY);
			act("$n turns the coin mill...", FALSE, ch, NULL, NULL, TO_ROOM | TO_SPAMMY);
		}
	}
	else {
		num = GET_ACTION_VNUM(ch, 1);
		msg_to_char(ch, "You finish minting and receive %s!\r\n", money_amount(emp, num));
		act("$n finishes minting some coins!", FALSE, ch, NULL, NULL, TO_ROOM);
		increase_coins(ch, emp, num);
		
		GET_ACTION(ch) = ACT_NONE;
		gain_player_tech_exp(ch, PTECH_MINT, 30);
		
		if ((proto = obj_proto(GET_ACTION_VNUM(ch, 0)))) {
			strcpy(tmp, fname(GET_OBJ_KEYWORDS(proto)));
			do_mint(ch, tmp, 0, 0);
		}
	}
}


/**
* Tick update for morph action.
*
* @param char_data *ch The morpher.
*/
void process_morphing(char_data *ch) {
	GET_ACTION_TIMER(ch) -= 1;

	if (GET_ACTION_TIMER(ch) <= 0) {
		finish_morphing(ch, morph_proto(GET_ACTION_VNUM(ch, 0)));
		GET_ACTION(ch) = ACT_NONE;
	}
	else {
		if (!PRF_FLAGGED(ch, PRF_NOSPAM)) {
			msg_to_char(ch, "Your body warps and distorts painfully!\r\n");
		}
		act("$n's body warps and distorts hideously!", TRUE, ch, 0, 0, TO_ROOM | TO_SPAMMY);
	}
}


/**
* Tick update for music action. Plays the instrument-to-char and instrument-
* to-room messages in order and then repeats.
*
* @param char_data *ch The musician.
*/
void process_music(char_data *ch) {
	struct custom_message *mcm, *first_mcm, *found_mcm;
	obj_data *obj;
	int count;
	
	if (!(obj = GET_EQ(ch, WEAR_HOLD)) || GET_OBJ_TYPE(obj) != ITEM_INSTRUMENT) {
		msg_to_char(ch, "You need to hold an instrument to play music!\r\n");
		cancel_action(ch);
		return;
	}
	
	// instrument-to-char
	if (obj_has_custom_message(obj, OBJ_CUSTOM_INSTRUMENT_TO_CHAR)) {
		GET_ACTION_VNUM(ch, 0) += 1;
		
		first_mcm = found_mcm = NULL;
		count = 0;
		LL_FOREACH(GET_OBJ_CUSTOM_MSGS(obj), mcm) {
			if (mcm->type != OBJ_CUSTOM_INSTRUMENT_TO_CHAR) {
				continue;
			}
			
			// catch the first matching one just in case
			if (!first_mcm) {
				first_mcm = mcm;
			}
			if (++count == GET_ACTION_VNUM(ch, 0)) {
				found_mcm = mcm;
				break;
			}
		}
		if (!found_mcm) {
			// pull first one
			found_mcm = first_mcm;
			GET_ACTION_VNUM(ch, 0) = 1;
		}
		
		// did we find one?
		if (found_mcm && found_mcm->msg) {
			act(found_mcm->msg, FALSE, ch, obj, NULL, TO_CHAR | TO_SPAMMY);
		}
	}
	
	// instrument-to-room
	if (obj_has_custom_message(obj, OBJ_CUSTOM_INSTRUMENT_TO_ROOM)) {
		GET_ACTION_VNUM(ch, 1) += 1;
		
		first_mcm = found_mcm = NULL;
		count = 0;
		LL_FOREACH(GET_OBJ_CUSTOM_MSGS(obj), mcm) {
			if (mcm->type != OBJ_CUSTOM_INSTRUMENT_TO_ROOM) {
				continue;
			}
			
			// catch the first matching one just in case
			if (!first_mcm) {
				first_mcm = mcm;
			}
			if (++count == GET_ACTION_VNUM(ch, 1)) {
				found_mcm = mcm;
				break;
			}
		}
		if (!found_mcm) {
			// pull first one
			found_mcm = first_mcm;
			GET_ACTION_VNUM(ch, 1) = 1;
		}
		
		// did we find one?
		if (found_mcm && found_mcm->msg) {
			act(found_mcm->msg, FALSE, ch, obj, NULL, TO_ROOM | TO_SPAMMY);
		}
	}
}


/**
* Tick update for pan action.
*
* @param char_data *ch The panner.
*/
void process_panning(char_data *ch) {
	bool success = FALSE;
	room_data *room;
	int dir;
	
	dir = GET_ACTION_VNUM(ch, 0);
	room = (dir == NO_DIR) ? IN_ROOM(ch) : dir_to_room(IN_ROOM(ch), dir, FALSE);
	
	if (!has_tool(ch, TOOL_PAN)) {
		msg_to_char(ch, "You need to be using a pan to do that.\r\n");
		cancel_action(ch);
	}
	else if (!can_see_in_dark_room(ch, IN_ROOM(ch), TRUE)) {
		msg_to_char(ch, "It's too dark to pan here.\r\n");
		cancel_action(ch);
	}
	else if (!room || !can_interact_room(room, INTERACT_PAN) || !can_use_room(ch, room, MEMBERS_ONLY)) {
		msg_to_char(ch, "You can no longer pan %s.\r\n", (room == IN_ROOM(ch)) ? "here" : "there");
		cancel_action(ch);
	}
	else {
		GET_ACTION_TIMER(ch) -= 1;
		
		act("You sift through the sand and pebbles, looking for gold...", FALSE, ch, NULL, NULL, TO_CHAR | TO_SPAMMY);
		act("$n sifts through the sand, looking for gold...", TRUE, ch, NULL, NULL, TO_ROOM | TO_SPAMMY);
		
		if (GET_ACTION_TIMER(ch) <= 0) {
			GET_ACTION(ch) = ACT_NONE;
			
			// pan will silently fail if depleted
			if (get_depletion(room, DPLTN_PAN) <= config_get_int("short_depletion")) {
				success = run_room_interactions(ch, room, INTERACT_PAN, NULL, GUESTS_ALLOWED, finish_panning);
			}
			
			if (success) {
				add_depletion(room, DPLTN_PAN, TRUE);
			}
			else {
				msg_to_char(ch, "You find nothing of value.\r\n");
			}
			
			start_panning(ch, dir);
		}
	}
}


/**
* Tick update for plant action.
*
* @param char_data *ch The planter.
*/
void process_planting(char_data *ch) {
	int left = get_room_extra_data(IN_ROOM(ch), ROOM_EXTRA_SEED_TIME) - time(0);	// seconds left
	
	// decrement
	GET_ACTION_TIMER(ch) -= 1;
		
	// starts at 4
	switch (GET_ACTION_TIMER(ch)) {
		case 3: {
			if (!PRF_FLAGGED(ch, PRF_NOSPAM)) {
				msg_to_char(ch, "You carefully plant seeds in rows along the ground.\r\n");
			}
			act("$n carefully plants seeds in rows along the ground.", FALSE, ch, NULL, NULL, TO_ROOM | TO_SPAMMY);
			left /= 2;
			set_room_extra_data(IN_ROOM(ch), ROOM_EXTRA_SEED_TIME, time(0) + left);
			if (GET_MAP_LOC(IN_ROOM(ch))) {
				schedule_crop_growth(GET_MAP_LOC(IN_ROOM(ch)));
			}
			break;
		}
		case 2: {
			if (!PRF_FLAGGED(ch, PRF_NOSPAM)) {
				msg_to_char(ch, "You cover the seeds and gently pack the dirt.\r\n");
			}
			act("$n covers rows of seeds with dirt and gently packs them down.", FALSE, ch, NULL, NULL, TO_ROOM | TO_SPAMMY);
			left /= 2;
			set_room_extra_data(IN_ROOM(ch), ROOM_EXTRA_SEED_TIME, time(0) + left);
			if (GET_MAP_LOC(IN_ROOM(ch))) {
				schedule_crop_growth(GET_MAP_LOC(IN_ROOM(ch)));
			}
			break;
		}
		case 1: {
			if (!PRF_FLAGGED(ch, PRF_NOSPAM)) {
				msg_to_char(ch, "You water the freshly seeded ground.\r\n");
			}
			act("$n waters the freshly seeded ground.", FALSE, ch, NULL, NULL, TO_ROOM | TO_SPAMMY);
			left /= 2;
			set_room_extra_data(IN_ROOM(ch), ROOM_EXTRA_SEED_TIME, time(0) + left);
			if (GET_MAP_LOC(IN_ROOM(ch))) {
				schedule_crop_growth(GET_MAP_LOC(IN_ROOM(ch)));
			}
			break;
		}
	}
	
	if (GET_ACTION_TIMER(ch) <= 0) {
		msg_to_char(ch, "You have finished planting!\r\n");
		act("$n finishes planting!", FALSE, ch, 0, 0, TO_ROOM);
		
		gain_player_tech_exp(ch, PTECH_PLANT_CROPS, 30);
		
		GET_ACTION(ch) = ACT_NONE;
	}
}


/**
* Tick update for prospect action.
*
* @param char_data *ch The prospector.
*/
void process_prospecting(char_data *ch) {
	if (!can_see_in_dark_room(ch, IN_ROOM(ch), TRUE)) {
		msg_to_char(ch, "It's too dark for you to keep prospecting.\r\n");
		cancel_action(ch);
		return;
	}
		
	// simple decrement
	GET_ACTION_TIMER(ch) -= 1;
	
	switch (GET_ACTION_TIMER(ch)) {
		case 5: {
			if (!PRF_FLAGGED(ch, PRF_NOSPAM)) {
				msg_to_char(ch, "You pick at the ground a little bit...\r\n");
			}
			break;
		}
		case 3: {
			if (!PRF_FLAGGED(ch, PRF_NOSPAM)) {
				msg_to_char(ch, "You sift through some dirt...\r\n");
			}
			break;
		}
		case 1: {
			if (!PRF_FLAGGED(ch, PRF_NOSPAM)) {
				msg_to_char(ch, "You taste a bit of soil...\r\n");
			}
			break;
		}
		case 0: {
			GET_ACTION(ch) = ACT_NONE;
			init_mine(IN_ROOM(ch), ch, GET_LOYALTY(ch));
			set_room_extra_data(IN_ROOM(ch), ROOM_EXTRA_PROSPECT_EMPIRE, GET_LOYALTY(ch) ? EMPIRE_VNUM(GET_LOYALTY(ch)) : NOTHING);
			
			show_prospect_result(ch, IN_ROOM(ch));
			act("$n finishes prospecting.", TRUE, ch, NULL, NULL, TO_ROOM);
			break;
		}
	}
}


/**
* Tick update for repairing action.
*
* @param char_data *ch The character doing the repairing.
*/
void process_repairing(char_data *ch) {
	char buf[MAX_STRING_LENGTH];
	obj_data *found_obj = NULL;
	struct resource_data *res, temp_res;
	bool found = FALSE;
	vehicle_data *veh;
	
	// first attempt to re-find the vehicle
	if (!(veh = find_vehicle(GET_ACTION_VNUM(ch, 0))) || IN_ROOM(veh) != IN_ROOM(ch) || !VEH_IS_COMPLETE(veh)) {
		cancel_action(ch);
		return;
	}
	if (!VEH_NEEDS_RESOURCES(veh) && VEH_HEALTH(veh) >= VEH_MAX_HEALTH(veh)) {
		cancel_action(ch);
		return;
	}
	if (!can_see_in_dark_room(ch, IN_ROOM(ch), TRUE)) {
		msg_to_char(ch, "It's too dark to repair anything.\r\n");
		cancel_action(ch);
		return;
	}
	if (!vehicle_allows_climate(veh, IN_ROOM(veh), NULL)) {
		msg_to_char(ch, "You can't repair it -- it's falling into disrepair because it's in the wrong terrain.\r\n");
		cancel_action(ch);
		return;
	}
	
	// good to repair:
	if ((res = get_next_resource(ch, VEH_NEEDS_RESOURCES(veh), can_use_room(ch, IN_ROOM(ch), MEMBERS_ONLY), TRUE, &found_obj))) {
		// take the item; possibly free the res
		apply_resource(ch, res, &VEH_NEEDS_RESOURCES(veh), found_obj, APPLY_RES_REPAIR, veh, VEH_FLAGGED(veh, VEH_NEVER_DISMANTLE) ? NULL : &VEH_BUILT_WITH(veh));
		request_vehicle_save_in_world(veh);
		found = TRUE;
	}
	
	// done?
	if (!VEH_NEEDS_RESOURCES(veh)) {
		GET_ACTION(ch) = ACT_NONE;
		act("$V is fully repaired!", FALSE, ch, NULL, veh, TO_CHAR | TO_ROOM);
		complete_vehicle(veh);
	}
	else if (!found) {
		GET_ACTION(ch) = ACT_NONE;
		
		// missing next resource
		if (VEH_NEEDS_RESOURCES(veh)) {
			// copy this to display the next 1
			temp_res = *VEH_NEEDS_RESOURCES(veh);
			temp_res.next = NULL;
			show_resource_list(&temp_res, buf);
			msg_to_char(ch, "You don't have %s and stop repairing.\r\n", buf);
		}
		else {
			msg_to_char(ch, "You run out of resources and stop repairing.\r\n");
		}
		act("$n runs out of resources and stops.", FALSE, ch, NULL, NULL, TO_ROOM);
	}
}


/**
* Tick update for scrape action.
*
* @param char_data *ch The scraper.
*/
void process_scraping(char_data *ch) {
	char buf[MAX_STRING_LENGTH];
	bool success = FALSE;
	obj_data *proto;
	
	if (!has_player_tech(ch, PTECH_SCRAPE_COMMAND)) {
		msg_to_char(ch, "You don't have the right ability to scrape anything.\r\n");
		cancel_action(ch);
		return;
	}
	if (!has_tool(ch, TOOL_AXE | TOOL_KNIFE)) {
		msg_to_char(ch, "You need to be using a good axe or knife to scrape it.\r\n");
		cancel_action(ch);
		return;
	}
	if (!can_see_in_dark_room(ch, IN_ROOM(ch), TRUE)) {
		msg_to_char(ch, "It's too dark to finish scraping.\r\n");
		cancel_action(ch);
		return;
	}
	
	// skilled work
	GET_ACTION_TIMER(ch) -= 1 + (has_player_tech(ch, PTECH_FAST_WOOD_PROCESSING) ? 1 : 0);
	
	// messaging -- to player only
	if (!PRF_FLAGGED(ch, PRF_NOSPAM)) {
		msg_to_char(ch, "You scrape at %s...\r\n", get_obj_name_by_proto(GET_ACTION_VNUM(ch, 0)));
	}
	
	// done?
	if (GET_ACTION_TIMER(ch) <= 0) {
		
		// will extract no matter what happens here
		if ((proto = obj_proto(GET_ACTION_VNUM(ch, 0)))) {
			act("You finish scraping off $p.", FALSE, ch, proto, NULL, TO_CHAR);
			act("$n finishes scraping off $p.", TRUE, ch, proto, NULL, TO_ROOM);
			
			success = run_interactions(ch, GET_OBJ_INTERACTIONS(proto), INTERACT_SCRAPE, IN_ROOM(ch), NULL, proto, NULL, finish_scraping);
		}
		
		if (!success && !proto) {
			snprintf(buf, sizeof(buf), "You finish scraping off %s but get nothing.", get_obj_name_by_proto(GET_ACTION_VNUM(ch, 0)));
			act(buf, FALSE, ch, NULL, NULL, TO_CHAR);
			snprintf(buf, sizeof(buf), "$n finishes scraping off %s.", get_obj_name_by_proto(GET_ACTION_VNUM(ch, 0)));
			act(buf, TRUE, ch, NULL, NULL, TO_ROOM);
		}
		
		GET_ACTION(ch) = ACT_NONE;
		free_resource_list(GET_ACTION_RESOURCES(ch));
		GET_ACTION_RESOURCES(ch) = NULL;
		
		if (success && proto) {
			gain_player_tech_exp(ch, PTECH_SCRAPE_COMMAND, 10);
			
			// lather, rinse, rescrape
			do_scrape(ch, fname(GET_OBJ_KEYWORDS(proto)), 0, 0);
		}
	}
}


/**
* Tick update for siring action.
*
* @param char_data *ch The sirer (?).
*/
void process_siring(char_data *ch) {
	if (!GET_FEEDING_FROM(ch)) {
		cancel_action(ch);
	}
	else if (GET_BLOOD(GET_FEEDING_FROM(ch)) <= 0) {
		sire_char(ch, GET_FEEDING_FROM(ch));
	}
}


/**
* Tick update for start-fillin.
*
* @param char_data *ch The person trying to fill in.
*/
void process_start_fillin(char_data *ch) {
	sector_data *old_sect;
	
	if (ROOM_SECT_FLAGGED(IN_ROOM(ch), SECTF_IS_TRENCH)) {
		// someone already finished the start-fillin
		start_action(ch, ACT_FILLING_IN, 0);
		msg_to_char(ch, "You begin to fill in the trench.\r\n");
		
		// already finished the trench? start it back to -1 (otherwise, just continue)
		if (get_room_extra_data(IN_ROOM(ch), ROOM_EXTRA_TRENCH_PROGRESS) >= 0) {
			set_room_extra_data(IN_ROOM(ch), ROOM_EXTRA_TRENCH_PROGRESS, -1);
		}
	}
	else if (GET_ROOM_VNUM(IN_ROOM(ch)) >= MAP_SIZE || !(old_sect = reverse_lookup_evolution_for_sector(SECT(IN_ROOM(ch)), EVO_TRENCH_FULL))) {
		// anything to reverse it to?
		msg_to_char(ch, "You can't fill anything in here.\r\n");
		cancel_action(ch);
	}
	else if (!ROOM_OWNER(IN_ROOM(ch))) {
		msg_to_char(ch, "You can only fill in claimed tiles.\r\n");
		cancel_action(ch);
	}
	else if (!can_use_room(ch, IN_ROOM(ch), GUESTS_ALLOWED)) {
		msg_to_char(ch, "You no longer have permission to fill in here.\r\n");
		cancel_action(ch);
	}
	else if (GET_ACTION_TIMER(ch) <= 0) {
		// final permissions check
		if (!can_use_room(ch, IN_ROOM(ch), MEMBERS_ONLY)) {
			msg_to_char(ch, "You no longer have permission to fill in here.\r\n");
			cancel_action(ch);
			return;
		}
		
		// finished starting the fillin
		start_action(ch, ACT_FILLING_IN, 0);
		msg_to_char(ch, "You block off the water and begin to fill in the trench.\r\n");
		
		// set it up
		change_terrain(IN_ROOM(ch), GET_SECT_VNUM(old_sect), NOTHING);
		set_room_extra_data(IN_ROOM(ch), ROOM_EXTRA_TRENCH_PROGRESS, -1);
	}
	else {
		// decrement
		GET_ACTION_TIMER(ch) -= 1;
		
		if (!PRF_FLAGGED(ch, PRF_NOSPAM)) {
			msg_to_char(ch, "You prepare to fill in the trench...\r\n");
		}
		act("$n prepares to fill in the trench...", FALSE, ch, NULL, NULL, TO_ROOM | TO_SPAMMY);
	}
}


/**
* Tick update for start-fillin.
*
* @param char_data *ch The person trying to fill in.
*/
void process_swap_skill_sets(char_data *ch) {
	GET_ACTION_TIMER(ch) -= 1;
	
	if (GET_ACTION_TIMER(ch) <= 0) {
		msg_to_char(ch, "You swap skill sets.\r\n");
		act("$n swaps skill sets.", TRUE, ch, NULL, NULL, TO_ROOM);
		
		perform_swap_skill_sets(ch);
		GET_ACTION(ch) = ACT_NONE;
		
		set_move(ch, MIN(GET_MOVE(ch), GET_MAX_MOVE(ch)/4));
		set_mana(ch, MIN(GET_MANA(ch), GET_MAX_MANA(ch)/4));
	}
}


/**
* Ticker for tanners.
*
* @param char_data *ch The tanner.
*/
void process_tanning(char_data *ch) {
	obj_data *proto;
	bool success;
	
	if (!can_see_in_dark_room(ch, IN_ROOM(ch), TRUE)) {
		msg_to_char(ch, "It's too dark to finish tanning.\r\n");
		cancel_action(ch);
		return;
	}
	
	GET_ACTION_TIMER(ch) -= (room_has_function_and_city_ok(GET_LOYALTY(ch), IN_ROOM(ch), FNC_TANNERY) ? 4 : 1);
	
	// need the prototype
	if (!(proto = obj_proto(GET_ACTION_VNUM(ch, 0)))) {
		cancel_action(ch);
		return;
	}
	
	if (GET_ACTION_TIMER(ch) <= 0) {
		act("You finish tanning $p.", FALSE, ch, proto, NULL, TO_CHAR);
		act("$n finishes tanning $p.", TRUE, ch, proto, NULL, TO_ROOM);

		GET_ACTION(ch) = ACT_NONE;
		
		success = run_interactions(ch, GET_OBJ_INTERACTIONS(proto), INTERACT_TAN, IN_ROOM(ch), NULL, proto, NULL, finish_scraping);
		free_resource_list(GET_ACTION_RESOURCES(ch));
		GET_ACTION_RESOURCES(ch) = NULL;
		
		if (success) {
			gain_player_tech_exp(ch, PTECH_TAN, 20);
	
			// repeat!
			do_tan(ch, fname(GET_OBJ_KEYWORDS(proto)), 0, 0);
		}
	}
	else {
		switch (GET_ACTION_TIMER(ch)) {
			case 10: {	// non-tannery only
				act("You soak $p...", FALSE, ch, proto, NULL, TO_CHAR | TO_SPAMMY);
				act("$n soaks $p...", FALSE, ch, proto, NULL, TO_ROOM | TO_SPAMMY);
				break;
			}
			case 8: {	// all
				act("You scrape $p clean...", FALSE, ch, proto, NULL, TO_CHAR | TO_SPAMMY);
				act("$n scrapes $p clean...", FALSE, ch, proto, NULL, TO_ROOM | TO_SPAMMY);
				break;
			}
			case 6: {	// non-tannery only
				act("You brush $p...", FALSE, ch, proto, NULL, TO_CHAR | TO_SPAMMY);
				act("$n brushes $p...", FALSE, ch, proto, NULL, TO_ROOM | TO_SPAMMY);
				break;
			}
			case 4: {	// all
				act("You knead $p with a foul-smelling mixture...", FALSE, ch, proto, NULL, TO_CHAR | TO_SPAMMY);
				act("$n kneads $p with a foul-smelling mixture...", FALSE, ch, proto, NULL, TO_ROOM | TO_SPAMMY);
				break;
			}
			case 2: {	// non-tannery only
				act("You stretch $p...", FALSE, ch, proto, NULL, TO_CHAR | TO_SPAMMY);
				act("$n stretches $p...", FALSE, ch, proto, NULL, TO_ROOM | TO_SPAMMY);
				break;
			}
		}
	}
}


 //////////////////////////////////////////////////////////////////////////////
//// ACTION COMMANDS /////////////////////////////////////////////////////////

ACMD(do_bathe) {
	if (IS_NPC(ch)) {
		msg_to_char(ch, "NPCs cannot bathe. It's dirty, but it's true.\r\n");
	}
	else if (GET_ACTION(ch) == ACT_BATHING) {
		msg_to_char(ch, "You stop bathing and climb out of the water.\r\n");
		cancel_action(ch);
	}
	else if (GET_ACTION(ch) != ACT_NONE) {
		msg_to_char(ch, "You're a bit busy right now.\r\n");
	}
	else if (!room_has_function_and_city_ok(GET_LOYALTY(ch), IN_ROOM(ch), FNC_BATHS) && !ROOM_SECT_FLAGGED(IN_ROOM(ch), SECTF_FRESH_WATER | SECTF_SHALLOW_WATER)) {
		msg_to_char(ch, "You can't bathe here!\r\n");
	}
	else {
		start_action(ch, ACT_BATHING, 4);

		msg_to_char(ch, "You undress and climb into the water!\r\n");
		act("$n undresses and climbs into the water!", FALSE, ch, 0, 0, TO_ROOM);
	}
}


ACMD(do_chip) {
	obj_data *target;
	int number;
	int chip_timer = config_get_int("chip_timer");
	char *argptr = arg;
	
	one_argument(argument, arg);
	number = get_number(&argptr);

	if (IS_NPC(ch)) {
		msg_to_char(ch, "NPCs cannot chip.\r\n");
	}
	else if (GET_ACTION(ch) == ACT_CHIPPING) {
		msg_to_char(ch, "You stop chipping it.\r\n");
		act("$n stops chipping.", TRUE, ch, NULL, NULL, TO_ROOM);
		cancel_action(ch);
	}
	else if (!has_player_tech(ch, PTECH_CHIP_COMMAND)) {
		msg_to_char(ch, "You don't have the right ability to chip anything.\r\n");
	}
	else if (!IS_APPROVED(ch) && config_get_bool("craft_approval")) {
		send_config_msg(ch, "need_approval_string");
	}
	else if (GET_ACTION(ch) != ACT_NONE) {
		msg_to_char(ch, "You're already busy doing something else.\r\n");
	}
	else if (!can_see_in_dark_room(ch, IN_ROOM(ch), TRUE)) {
		msg_to_char(ch, "It's too dark to chip anything here.\r\n");
	}
	else if (!*argptr) {
		msg_to_char(ch, "Chip what?\r\n");
	}
	else if (!(target = get_obj_in_list_vis_prefer_interaction(ch, argptr, &number, ch->carrying, INTERACT_CHIP)) && (!can_use_room(ch, IN_ROOM(ch), MEMBERS_ONLY) || !(target = get_obj_in_list_vis_prefer_interaction(ch, argptr, &number, ROOM_CONTENTS(IN_ROOM(ch)), INTERACT_CHIP)))) {
		msg_to_char(ch, "You don't seem to have %s %s.\r\n", AN(arg), arg);
	}
	else if (!has_interaction(GET_OBJ_INTERACTIONS(target), INTERACT_CHIP)) {
		msg_to_char(ch, "You can't chip that!\r\n");
	}
	else if (!has_tool(ch, TOOL_KNAPPER)) {
		msg_to_char(ch, "You need to be wielding some kind of knapper (or basic rock) to chip it.\r\n");
	}
	else {
		start_action(ch, ACT_CHIPPING, chip_timer);
		
		// store the obj
		add_to_resource_list(&GET_ACTION_RESOURCES(ch), RES_OBJECT, GET_OBJ_VNUM(target), 1, GET_OBJ_CURRENT_SCALE_LEVEL(target));
		GET_ACTION_VNUM(ch, 0) = GET_OBJ_VNUM(target);
		GET_ACTION_VNUM(ch, 1) = GET_OBJ_CURRENT_SCALE_LEVEL(target);
		
		act("You begin to chip at $p.", FALSE, ch, target, NULL, TO_CHAR);
		act("$n begins to chip at $p.", TRUE, ch, target, NULL, TO_ROOM);
		extract_obj(target);
	}
}


ACMD(do_chop) {
	if (IS_NPC(ch)) {
		msg_to_char(ch, "NPCs cannot chop.\r\n");
	}
	else if (GET_ACTION(ch) == ACT_CHOPPING) {
		send_to_char("You stop chopping.\r\n", ch);
		act("$n stops chopping.", FALSE, ch, 0, 0, TO_ROOM);
		cancel_action(ch);
	}
	else if (!has_player_tech(ch, PTECH_CHOP)) {
		msg_to_char(ch, "You don't have the correct ability to chop anything.\r\n");
	}
	else if (!IS_APPROVED(ch) && config_get_bool("gather_approval")) {
		send_config_msg(ch, "need_approval_string");
	}
	else if (GET_ACTION(ch) != ACT_NONE) {
		send_to_char("You're already busy.\r\n", ch);
	}
	else if (!CAN_CHOP_ROOM(IN_ROOM(ch)) && IS_OUTDOORS(ch) && has_interaction(GET_SECT_INTERACTIONS(BASE_SECT(IN_ROOM(ch))), INTERACT_CHOP)) {
		// variant of can't-chop where it's blocked by something other than the base sector
		send_to_char("You can't really chop anything here right now.\r\n", ch);
	}
	else if (!CAN_CHOP_ROOM(IN_ROOM(ch))) {
		send_to_char("You can't really chop anything down here.\r\n", ch);
	}
	else if (ROOM_AFF_FLAGGED(IN_ROOM(ch), ROOM_AFF_HAS_INSTANCE | ROOM_AFF_NO_EVOLVE)) {
		msg_to_char(ch, "You can't chop here right now.\r\n");
	}
	else if (!ROOM_AFF_FLAGGED(IN_ROOM(ch), ROOM_AFF_UNCLAIMABLE) && !can_use_room(ch, IN_ROOM(ch), MEMBERS_ONLY)) {
		msg_to_char(ch, "You don't have permission to chop down trees here.\r\n");
	}
	else if (!ROOM_AFF_FLAGGED(IN_ROOM(ch), ROOM_AFF_UNCLAIMABLE) && !has_permission(ch, PRIV_CHOP, IN_ROOM(ch))) {
		msg_to_char(ch, "You don't have permission to chop down trees in the empire.\r\n");
	}
	else if (!has_tool(ch, TOOL_AXE)) {
		send_to_char("You need to be using some kind of axe to chop.\r\n", ch);
	}
	else if (!can_see_in_dark_room(ch, IN_ROOM(ch), TRUE)) {
		msg_to_char(ch, "It's too dark to chop anything here.\r\n");
	}
	else {
		start_chopping(ch);
	}
}


ACMD(do_dig) {
	skip_spaces(&argument);
	
	if (IS_NPC(ch)) {
		msg_to_char(ch, "NPCs can't dig.\r\n");
	}
	else if (*argument) {
		msg_to_char(ch, "You can't dig for specific items. Just type 'dig' and see what you get.\r\n");
	}
	else if (GET_ACTION(ch) == ACT_DIGGING) {
		send_to_char("You stop digging.\r\n", ch);
		act("$n stops digging.", FALSE, ch, 0, 0, TO_ROOM);
		cancel_action(ch);
	}
	else if (!has_player_tech(ch, PTECH_DIG)) {
		msg_to_char(ch, "You don't have the correct ability to dig anything.\r\n");
	}
	else if (!IS_APPROVED(ch) && config_get_bool("gather_approval")) {
		send_config_msg(ch, "need_approval_string");
	}
	else if (GET_ACTION(ch) != ACT_NONE) {
		send_to_char("You're already busy.\r\n", ch);
	}
	else if (!can_interact_room(IN_ROOM(ch), INTERACT_DIG)) {
		send_to_char("You can't dig here.\r\n", ch);
	}
	else if (!can_use_room(ch, IN_ROOM(ch), GUESTS_ALLOWED)) {
		msg_to_char(ch, "You don't have permission to dig here.\r\n");
	}
	else {
		start_digging(ch);
	}
}


/**
* do_light passes control here after finding that the target is the room.
*
* This is a timed action that triggers a room evolution.
*
* @param char_data *ch The character doing the action.
* @param int subcmd The subcmd that was passed to do_light (SCMD_LIGHT, SCMD_BURN).
*/
void do_burn_area(char_data *ch, int subcmd) {
	const char *cmdname[] = { "light", "burn" };	// also in do_light
	
	if (IS_NPC(ch)) {
		msg_to_char(ch, "You cannot %s the area.\r\n", cmdname[subcmd]);
	}
	else if (GET_ACTION(ch) != ACT_NONE) {
		msg_to_char(ch, "You're a little bit busy right now.\r\n");
	}
	else if (GET_POS(ch) != POS_STANDING) {
		send_low_pos_msg(ch);
	}
	else if (!validate_burn_area(ch, subcmd)) {
		// sends its own message
	}
	else {
		start_action(ch, ACT_BURN_AREA, 5);
		GET_ACTION_VNUM(ch, 0) = subcmd;
		
		msg_to_char(ch, "You prepare to burn the area...\r\n");
		act("$n prepares to burn the area...", FALSE, ch, NULL, NULL, TO_ROOM);
	}
}


ACMD(do_excavate) {
	struct evolution_data *evo;
	sector_data *orig;
	
	if (IS_NPC(ch)) {
		msg_to_char(ch, "NPCs cannot excavate.\r\n");
	}
	else if (GET_ACTION(ch) == ACT_EXCAVATING) {
		msg_to_char(ch, "You stop the excavation.\r\n");
		act("$n stops excavating the trench.", FALSE, ch, 0, 0, TO_ROOM);
		cancel_action(ch);
	}
	else if (!IS_APPROVED(ch) && config_get_bool("terraform_approval")) {
		send_config_msg(ch, "need_approval_string");
	}
	else if (ROOM_AFF_FLAGGED(IN_ROOM(ch), ROOM_AFF_HAS_INSTANCE)) {
		// even if already started
		msg_to_char(ch, "You can't excavate here.\r\n");
	}
	else if (GET_ACTION(ch) != ACT_NONE) {
		msg_to_char(ch, "You're already quite busy.\r\n");
	}
	else if (!ROOM_OWNER(IN_ROOM(ch))) {
		msg_to_char(ch, "You must claim a tile in order to excavate it.\r\n");
	}
	else if (!can_use_room(ch, IN_ROOM(ch), GUESTS_ALLOWED)) {
		// 1st check: allies ok
		msg_to_char(ch, "You don't have permission to excavate here!\r\n");
	}
	else if (!has_tool(ch, TOOL_SHOVEL)) {
		msg_to_char(ch, "You need a shovel to excavate.\r\n");
	}
	else if (ROOM_SECT_FLAGGED(IN_ROOM(ch), SECTF_IS_TRENCH) && get_room_extra_data(IN_ROOM(ch), ROOM_EXTRA_TRENCH_PROGRESS) < 0) {
		start_action(ch, ACT_EXCAVATING, 0);
		msg_to_char(ch, "You begin to excavate the trench.\r\n");
		act("$n begins excavating the trench.", FALSE, ch, NULL, NULL, TO_ROOM);
	}
	else if (ROOM_SECT_FLAGGED(IN_ROOM(ch), SECTF_IS_TRENCH)) {
		msg_to_char(ch, "The trench is already complete!\r\n");
	}
	else if (!(evo = get_evolution_by_type(SECT(IN_ROOM(ch)), EVO_TRENCH_START))) {
		msg_to_char(ch, "You can't excavate a trench here.\r\n");
	}
	else if (ROOM_AFF_FLAGGED(IN_ROOM(ch), ROOM_AFF_NO_EVOLVE)) {
		msg_to_char(ch, "You can't excavate here right now.\r\n");
	}
	else if (is_entrance(IN_ROOM(ch))) {
		msg_to_char(ch, "You can't excavate a trench in front of an entrance.\r\n");
	}
	else if (!can_use_room(ch, IN_ROOM(ch), MEMBERS_ONLY)) {
		// 2nd check: members only to start a new excav
		msg_to_char(ch, "You don't have permission to excavate here!\r\n");
	}
	else {
		start_action(ch, ACT_EXCAVATING, 0);
		
		msg_to_char(ch, "You begin to excavate a trench.\r\n");
		act("$n begins excavating a trench.", FALSE, ch, 0, 0, TO_ROOM);

		// Set up the trench
		orig = SECT(IN_ROOM(ch));
		change_terrain(IN_ROOM(ch), evo->becomes, NOTHING);
		
		set_room_extra_data(IN_ROOM(ch), ROOM_EXTRA_TRENCH_PROGRESS, config_get_int("trench_initial_value"));
		set_room_extra_data(IN_ROOM(ch), ROOM_EXTRA_TRENCH_ORIGINAL_SECTOR, GET_SECT_VNUM(orig));
	}
}


ACMD(do_fillin) {
	sector_data *old_sect;
	
	if (IS_NPC(ch)) {
		msg_to_char(ch, "NPCs cannot fillin.\r\n");
	}
	else if (GET_ACTION(ch) == ACT_START_FILLIN) {
		msg_to_char(ch, "You stop preparing to fill in the trench.\r\n");
		act("$n stops preparing to fill in the trench.", FALSE, ch, 0, 0, TO_ROOM);
		cancel_action(ch);
	}
	else if (GET_ACTION(ch) == ACT_FILLING_IN) {
		msg_to_char(ch, "You stop filling in the trench.\r\n");
		act("$n stops filling in the trench.", FALSE, ch, 0, 0, TO_ROOM);
		cancel_action(ch);
	}
	else if (!IS_APPROVED(ch) && config_get_bool("terraform_approval")) {
		send_config_msg(ch, "need_approval_string");
	}
	else if (GET_ACTION(ch) != ACT_NONE) {
		msg_to_char(ch, "You're already quite busy.\r\n");
	}
	else if (ROOM_AFF_FLAGGED(IN_ROOM(ch), ROOM_AFF_HAS_INSTANCE)) {
		msg_to_char(ch, "You can't do that here.\r\n");
	}
	else if (!ROOM_OWNER(IN_ROOM(ch))) {
		msg_to_char(ch, "You must claim a tile in order to fill it in.\r\n");
	}
	else if (!can_use_room(ch, IN_ROOM(ch), GUESTS_ALLOWED)) {
		// 1st check: allies can help
		msg_to_char(ch, "You don't have permission to fill in here!\r\n");
	}
	else if (!has_tool(ch, TOOL_SHOVEL)) {
		msg_to_char(ch, "You need a shovel to do that.\r\n");
	}
	else if (ROOM_SECT_FLAGGED(IN_ROOM(ch), SECTF_IS_TRENCH)) {
		// already a trench -- just fill in
		start_action(ch, ACT_FILLING_IN, 0);
		msg_to_char(ch, "You begin to fill in the trench.\r\n");
		act("$n begins to fill in the trench.", FALSE, ch, NULL, NULL, TO_ROOM);
		
		// already finished the trench? start it back to -1 (otherwise, just continue)
		if (get_room_extra_data(IN_ROOM(ch), ROOM_EXTRA_TRENCH_PROGRESS) >= 0) {
			set_room_extra_data(IN_ROOM(ch), ROOM_EXTRA_TRENCH_PROGRESS, -1);
		}
	}
	else if (GET_ROOM_VNUM(IN_ROOM(ch)) >= MAP_SIZE || !(old_sect = reverse_lookup_evolution_for_sector(SECT(IN_ROOM(ch)), EVO_TRENCH_FULL))) {
		// anything to reverse it to?
		msg_to_char(ch, "You can't fill anything in here.\r\n");
	}
	else if (SECT(IN_ROOM(ch)) == world_map[FLAT_X_COORD(IN_ROOM(ch))][FLAT_Y_COORD(IN_ROOM(ch))].natural_sector) {
		msg_to_char(ch, "You can only fill in a tile that was made by excavation, not a natural one.\r\n");
	}
	else if (!can_use_room(ch, IN_ROOM(ch), MEMBERS_ONLY)) {
		// 2nd check: members only to start new fillin
		msg_to_char(ch, "You don't have permission to fill in here!\r\n");
	}
	else if (ROOM_AFF_FLAGGED(IN_ROOM(ch), ROOM_AFF_NO_EVOLVE)) {
		msg_to_char(ch, "You can't fill in here right now.\r\n");
	}
	else {
		// always takes 1 minute
		start_action(ch, ACT_START_FILLIN, SECS_PER_REAL_MIN / ACTION_CYCLE_TIME);
		msg_to_char(ch, "You begin preparing to fill in the trench.\r\n");
		act("$n prepares to fill in the trench.", FALSE, ch, NULL, NULL, TO_ROOM);
	}
}


ACMD(do_forage) {
	if (IS_NPC(ch)) {
		msg_to_char(ch, "NPCs cannot forage.\r\n");
	}
	else if (GET_ACTION(ch) == ACT_FORAGING) {
		send_to_char("You stop searching.\r\n", ch);
		act("$n stops looking around.", TRUE, ch, 0, 0, TO_ROOM);
		cancel_action(ch);
	}
	else if (!has_player_tech(ch, PTECH_FORAGE)) {
		msg_to_char(ch, "You don't have the correct ability to forage for anything.\r\n");
	}
	else if (!IS_APPROVED(ch) && config_get_bool("gather_approval")) {
		send_config_msg(ch, "need_approval_string");
	}
	else if (GET_ACTION(ch) != ACT_NONE) {
		send_to_char("You're already busy.\r\n", ch);
	}
	else if (!IS_OUTDOORS(ch) && !can_interact_room(IN_ROOM(ch), INTERACT_FORAGE)) {
		send_to_char("You don't see anything to forage for here.\r\n", ch);
	}
	else if (!can_use_room(ch, IN_ROOM(ch), GUESTS_ALLOWED)) {
		msg_to_char(ch, "You don't have permission to forage for anything here.\r\n");
	}
	else if (!IS_COMPLETE(IN_ROOM(ch))) {
		msg_to_char(ch, "You can't forage for anything in an incomplete building.\r\n");
	}
	else if (!can_see_in_dark_room(ch, IN_ROOM(ch), TRUE)) {
		msg_to_char(ch, "It's too dark to forage for anything here.\r\n");
	}
	else {
		start_foraging(ch);
	}
}


ACMD(do_gather) {
	int gather_base_timer = config_get_int("gather_base_timer");
	
	if (IS_NPC(ch)) {
		msg_to_char(ch, "NPCs cannot gather.\r\n");
	}
	else if (GET_ACTION(ch) == ACT_GATHERING) {
		send_to_char("You stop gathering.\r\n", ch);
		act("$n stops looking around.", TRUE, ch, 0, 0, TO_ROOM);
		cancel_action(ch);
	}
	else if (!has_player_tech(ch, PTECH_GATHER)) {
		msg_to_char(ch, "You don't have the correct ability to gather anything.\r\n");
	}
	else if (!IS_APPROVED(ch) && config_get_bool("gather_approval")) {
		send_config_msg(ch, "need_approval_string");
	}
	else if (GET_ACTION(ch) != ACT_NONE) {
		send_to_char("You're already busy.\r\n", ch);
	}
	else if (!can_interact_room(IN_ROOM(ch), INTERACT_GATHER)) {
		send_to_char("You can't really gather anything useful here.\r\n", ch);
	}
	else if (!can_use_room(ch, IN_ROOM(ch), GUESTS_ALLOWED)) {
		msg_to_char(ch, "You don't have permission to gather here.\r\n");
	}
	else {
		start_action(ch, ACT_GATHERING, gather_base_timer);
		
		send_to_char("You begin looking around for plant material.\r\n", ch);
	}
}


ACMD(do_harvest) {
	int harvest_timer = config_get_int("harvest_timer");
	
	if (IS_NPC(ch)) {
		msg_to_char(ch, "You can't do that.\r\n");
	}
	else if (GET_ACTION(ch) == ACT_HARVESTING) {
		if (ROOM_CROP(IN_ROOM(ch))) {
			msg_to_char(ch, "You stop harvesting the %s.\r\n", GET_CROP_NAME(ROOM_CROP(IN_ROOM(ch))));
		}
		else {
			msg_to_char(ch, "You stop harvesting.\r\n");
		}
		act("$n stops harvesting.\r\n", FALSE, ch, 0, 0, TO_ROOM);
		cancel_action(ch);
	}
	else if (!has_player_tech(ch, PTECH_HARVEST)) {
		msg_to_char(ch, "You don't have the correct ability to harvest anything.\r\n");
	}
	else if (!IS_APPROVED(ch) && config_get_bool("gather_approval")) {
		send_config_msg(ch, "need_approval_string");
	}
	else if (ROOM_AFF_FLAGGED(IN_ROOM(ch), ROOM_AFF_HAS_INSTANCE)) {
		msg_to_char(ch, "You can't harvest here.\r\n");
	}
	else if (GET_ACTION(ch) != ACT_NONE) {
		msg_to_char(ch, "You are already busy doing something else.\r\n");
	}
	else if (!ROOM_SECT_FLAGGED(IN_ROOM(ch), SECTF_CROP) || !can_interact_room(IN_ROOM(ch), INTERACT_HARVEST)) {
		msg_to_char(ch, "You can't harvest anything here!%s\r\n", ROOM_CROP_FLAGGED(IN_ROOM(ch), CROPF_IS_ORCHARD) ? " (try pick or chop)" : "");
	}
	else if (!ROOM_AFF_FLAGGED(IN_ROOM(ch), ROOM_AFF_UNCLAIMABLE) && !can_use_room(ch, IN_ROOM(ch), MEMBERS_ONLY)) {
		msg_to_char(ch, "You don't have permission to harvest this crop.\r\n");
	}
	else if (!ROOM_AFF_FLAGGED(IN_ROOM(ch), ROOM_AFF_UNCLAIMABLE) && !has_permission(ch, PRIV_HARVEST, IN_ROOM(ch))) {
		msg_to_char(ch, "You don't have permission to harvest empire crops.\r\n");
	}
	else if (!has_tool(ch, TOOL_HARVESTING)) {
		msg_to_char(ch, "You aren't using the proper tool for that.\r\n");
	}
	else if (!can_see_in_dark_room(ch, IN_ROOM(ch), TRUE)) {
		msg_to_char(ch, "It's too dark to harvest anything here.\r\n");
	}
	else {
		start_action(ch, ACT_HARVESTING, 0);

		// ensure progress is set up
		if (get_room_extra_data(IN_ROOM(ch), ROOM_EXTRA_HARVEST_PROGRESS) == 0) {
			set_room_extra_data(IN_ROOM(ch), ROOM_EXTRA_HARVEST_PROGRESS, harvest_timer);
		}
		
		msg_to_char(ch, "You begin harvesting the %s.\r\n", GET_CROP_NAME(ROOM_CROP(IN_ROOM(ch))));
		act("$n begins to harvest the crop.", FALSE, ch, 0, 0, TO_ROOM);
	}
}


ACMD(do_mine) {
	if (IS_NPC(ch)) {
		msg_to_char(ch, "You can't mine.\r\n");
	}
	else if (GET_ACTION(ch) == ACT_MINING) {
		msg_to_char(ch, "You stop mining.\r\n");
		act("$n stops mining.", FALSE, ch, 0, 0, TO_ROOM);
		cancel_action(ch);
	}
	else if (!IS_APPROVED(ch) && config_get_bool("gather_approval")) {
		send_config_msg(ch, "need_approval_string");
	}
	else if (GET_ACTION(ch) != ACT_NONE) {
		msg_to_char(ch, "You're busy doing something else right now.\r\n");
	}
	else if (!can_use_room(ch, IN_ROOM(ch), MEMBERS_ONLY)) {
		msg_to_char(ch, "You don't have permission to mine here.\r\n");
	}
	else if (!room_has_function_and_city_ok(GET_LOYALTY(ch), IN_ROOM(ch), FNC_MINE)) {
		msg_to_char(ch, "This isn't a mine.\r\n");
	}
	else if (!check_in_city_requirement(IN_ROOM(ch), TRUE)) {
		msg_to_char(ch, "This mine only works in a city.\r\n");
	}
	else if (!IS_COMPLETE(IN_ROOM(ch))) {
		msg_to_char(ch, "The mine shafts aren't finished yet.\r\n");
	}
	else if (get_room_extra_data(IN_ROOM(ch), ROOM_EXTRA_MINE_AMOUNT) <= 0) {
		msg_to_char(ch, "The mine is depleted, you find nothing of use.\r\n");
	}
	else if (!has_tool(ch, TOOL_MINING)) {
		msg_to_char(ch, "You aren't wielding a tool suitable for mining.\r\n");
	}
	else if (!can_see_in_dark_room(ch, IN_ROOM(ch), TRUE)) {
		msg_to_char(ch, "It's too dark to mine anything here.\r\n");
	}
	else {
		start_mining(ch);
	}
}


ACMD(do_mint) {
	empire_data *emp;
	obj_data *obj;
	char *argptr = arg;
	int number;
	
	one_argument(argument, arg);
	number = get_number(&argptr);
	
	if (IS_NPC(ch)) {
		msg_to_char(ch, "You can't do that.\r\n");
	}
	else if (GET_ACTION(ch) == ACT_MINTING) {
		msg_to_char(ch, "You stop minting coins.\r\n");
		act("$n stops minting coins.", FALSE, ch, NULL, NULL, TO_ROOM);
		cancel_action(ch);
	}
	else if (!IS_APPROVED(ch) && config_get_bool("craft_approval")) {
		send_config_msg(ch, "need_approval_string");
	}
	else if (!has_player_tech(ch, PTECH_MINT)) {
		msg_to_char(ch, "You don't have the correct ability to mint anything.\r\n");
	}
	else if (GET_ACTION(ch) != ACT_NONE) {
		msg_to_char(ch, "You're busy doing something else right now.\r\n");
	}
	else if (!room_has_function_and_city_ok(GET_LOYALTY(ch), IN_ROOM(ch), FNC_MINT)) {
		msg_to_char(ch, "You can't mint anything here.\r\n");
	}
	else if (!IS_COMPLETE(IN_ROOM(ch))) {
		msg_to_char(ch, "You need to finish building first.\r\n");
	}
	else if (!(emp = ROOM_OWNER(IN_ROOM(ch)))) {
		msg_to_char(ch, "This mint does not belong to any empire, and can't make coins.\r\n");
	}
	else if (!can_use_room(ch, IN_ROOM(ch), MEMBERS_AND_ALLIES)) {
		msg_to_char(ch, "You don't have permission to mint here.\r\n");
	}
	else if (!*argptr) {
		msg_to_char(ch, "Mint which item into coins?\r\n");
	}
	else if (!(obj = get_obj_in_list_vis(ch, argptr, &number, ch->carrying)) && !(obj = get_obj_in_list_vis(ch, argptr, &number, ROOM_CONTENTS(IN_ROOM(ch))))) {
		msg_to_char(ch, "You don't seem to have %s %s.\r\n", AN(arg), arg);
	}
	else if (!IS_WEALTH_ITEM(obj) || GET_WEALTH_VALUE(obj) <= 0) {
		msg_to_char(ch, "You can't mint that into coins.\r\n");
	}
	else if (!can_see_in_dark_room(ch, IN_ROOM(ch), TRUE)) {
		msg_to_char(ch, "It's too dark to mint anything here.\r\n");
	}
	else {
		act("You melt down $p and pour it into the coin mill...", FALSE, ch, obj, NULL, TO_CHAR);
		act("$n melts down $p and pours it into the coin mill...", FALSE, ch, obj, NULL, TO_ROOM);
		
		start_action(ch, ACT_MINTING, 2 * GET_WEALTH_VALUE(obj));
		GET_ACTION_VNUM(ch, 0) = GET_OBJ_VNUM(obj);
		GET_ACTION_VNUM(ch, 1) = GET_WEALTH_VALUE(obj) * (1/COIN_VALUE);
		extract_obj(obj);
	}
}


ACMD(do_pan) {
	room_data *room = IN_ROOM(ch);
	int dir = NO_DIR;
	
	any_one_arg(argument, arg);
	
	if (IS_NPC(ch)) {
		msg_to_char(ch, "You can't do that.\r\n");
	}
	else if (GET_ACTION(ch) == ACT_PANNING) {
		msg_to_char(ch, "You stop panning.\r\n");
		cancel_action(ch);
	}
	else if (!IS_APPROVED(ch) && config_get_bool("gather_approval")) {
		send_config_msg(ch, "need_approval_string");
	}
	else if (GET_ACTION(ch) != ACT_NONE) {
		msg_to_char(ch, "You're a bit busy right now.\r\n");
	}
	else if (*arg && (dir = parse_direction(ch, arg)) == NO_DIR) {
		msg_to_char(ch, "Pan what direction?\r\n");
	}
	else if (dir != NO_DIR && !(room = dir_to_room(IN_ROOM(ch), dir, FALSE))) {
		msg_to_char(ch, "You can't pan in that direction.\r\n");
	}
	else if (!can_interact_room(room, INTERACT_PAN)) {
		msg_to_char(ch, "You can't pan for anything %s.\r\n", (room == IN_ROOM(ch)) ? "here" : "there");
	}
	else if (!can_use_room(ch, room, MEMBERS_ONLY)) {
		msg_to_char(ch, "You don't have permission to pan %s.\r\n", (room == IN_ROOM(ch)) ? "here" : "there");
	}
	else if (!has_tool(ch, TOOL_PAN)) {
		msg_to_char(ch, "You need to be using a pan to do that.\r\n");
	}
	else if (!can_see_in_dark_room(ch, IN_ROOM(ch), TRUE)) {
		msg_to_char(ch, "It's too dark to pan for anything here.\r\n");
	}
	else {
		start_panning(ch, dir);
	}
}


ACMD(do_plant) {
	struct string_hash *str_iter, *next_str, *str_hash = NULL;
	char buf[MAX_STRING_LENGTH], line[256], lbuf[256];
	struct evolution_data *evo;
	sector_data *original;
	obj_data *obj;
	crop_data *cp;
	size_t size;

	one_argument(argument, arg);

	if (IS_NPC(ch)) {
		msg_to_char(ch, "You can't plant.\r\n");
	}
	else if (!has_player_tech(ch, PTECH_PLANT_CROPS)) {
		msg_to_char(ch, "You don't have the right ability to plant crops.\r\n");
	}
	else if (!IS_APPROVED(ch) && config_get_bool("gather_approval")) {
		send_config_msg(ch, "need_approval_string");
	}
	else if (GET_ACTION(ch) == ACT_PLANTING) {
		msg_to_char(ch, "You're already planting something.\r\n");
	}
	else if (GET_ACTION(ch) != ACT_NONE) {
		msg_to_char(ch, "You're already busy doing something else.\r\n");
	}
	else if (ROOM_AFF_FLAGGED(IN_ROOM(ch), ROOM_AFF_HAS_INSTANCE)) {
		msg_to_char(ch, "You can't do that here.\r\n");
	}
	else if (!ROOM_AFF_FLAGGED(IN_ROOM(ch), ROOM_AFF_UNCLAIMABLE) && !can_use_room(ch, IN_ROOM(ch), MEMBERS_ONLY)) {
		msg_to_char(ch, "You don't have permission to plant anything here.\r\n");
	}
	else if (!ROOM_AFF_FLAGGED(IN_ROOM(ch), ROOM_AFF_UNCLAIMABLE) && !has_permission(ch, PRIV_HARVEST, IN_ROOM(ch))) {
		msg_to_char(ch, "You don't have permission to plant crops in the empire.\r\n");
	}
	else if (!*arg) {
		// check inventory for plantables...
		DL_FOREACH2(ch->carrying, obj, next_content) {
			if (OBJ_FLAGGED(obj, OBJ_PLANTABLE) && (cp = crop_proto(GET_OBJ_VAL(obj, VAL_FOOD_CROP_TYPE)))) {
				ordered_sprintbit(GET_CROP_CLIMATE(cp), climate_flags, climate_flags_order, CROP_FLAGGED(cp, CROPF_ANY_LISTED_CLIMATE) ? TRUE : FALSE, lbuf);
				snprintf(line, sizeof(line), "%s - %s%s", GET_OBJ_DESC(obj, ch, OBJ_DESC_SHORT), lbuf, (CROP_FLAGGED(cp, CROPF_REQUIRES_WATER) ? ", requires water" : ""));
				add_string_hash(&str_hash, line, 1);
			}
		}
		
		if (str_hash) {
			// show plantables
			size = snprintf(buf, sizeof(buf), "What do you want to plant:\r\n");
			HASH_ITER(hh, str_hash, str_iter, next_str) {
				if (str_iter->count == 1) {
					snprintf(line, sizeof(line), " %s\r\n", str_iter->str);
				}
				else {
					snprintf(line, sizeof(line), " %s (x%d)\r\n", str_iter->str, str_iter->count);
				}
				if (size + strlen(line) + 16 < sizeof(buf)) {
					strcat(buf, line);
					size += strlen(line);
				}
				else {
					size += snprintf(buf + size, sizeof(buf) - size, "OVERFLOW\r\n");
					break;
				}
			}
			free_string_hash(&str_hash);
			if (ch->desc) {
				page_string(ch->desc, buf, TRUE);
			}
		}
		else {
			// nothing to plant
			msg_to_char(ch, "What do you want to plant?\r\n");
		}
	}
	else if (!(evo = get_evolution_by_type(SECT(IN_ROOM(ch)), EVO_PLANTS_TO))) {
		msg_to_char(ch, "Nothing can be planted here.\r\n");
	}
	else if (ROOM_AFF_FLAGGED(IN_ROOM(ch), ROOM_AFF_NO_EVOLVE)) {
		msg_to_char(ch, "You can't plant here right now.\r\n");
	}
	else if (!(obj = get_obj_in_list_vis(ch, arg, NULL, ch->carrying))) {
		msg_to_char(ch, "You don't seem to have any %s.\r\n", arg);
	}
	else if (!OBJ_FLAGGED(obj, OBJ_PLANTABLE)) {
		msg_to_char(ch, "You can't plant that!%s\r\n", has_interaction(GET_OBJ_INTERACTIONS(obj), INTERACT_SEED) ? " Try seeding it first." : "");
	}
	else if (!(cp = crop_proto(GET_OBJ_VAL(obj, VAL_FOOD_CROP_TYPE)))) {
		// this is a sanity check for bad crop values
		msg_to_char(ch, "You can't plant that!\r\n");
	}
	else if (CROP_FLAGGED(cp, CROPF_REQUIRES_WATER) && !find_flagged_sect_within_distance_from_char(ch, SECTF_FRESH_WATER, NOBITS, config_get_int("water_crop_distance"))) {
		msg_to_char(ch, "You must plant that closer to fresh water.\r\n");
	}
	else if (!MATCH_CROP_SECTOR_CLIMATE(cp, SECT(IN_ROOM(ch)))) {
		if (CROP_FLAGGED(cp, CROPF_ANY_LISTED_CLIMATE)) {
			ordered_sprintbit(GET_CROP_CLIMATE(cp), climate_flags, climate_flags_order, TRUE, buf);
			msg_to_char(ch, "You can only plant that in areas that are: %s\r\n", buf);
		}
		else {
			ordered_sprintbit(GET_CROP_CLIMATE(cp), climate_flags, climate_flags_order, FALSE, buf);
			msg_to_char(ch, "You can only plant that in %s areas.\r\n", trim(buf));
		}
	}
	else {
		original = SECT(IN_ROOM(ch));
		change_terrain(IN_ROOM(ch), evo->becomes, GET_SECT_VNUM(original));
		
		// don't use GET_FOOD_CROP_TYPE because not all plantables are food
		set_crop_type(IN_ROOM(ch), cp);
		
		set_room_extra_data(IN_ROOM(ch), ROOM_EXTRA_SEED_TIME, time(0) + config_get_int("planting_base_timer"));
		if (GET_MAP_LOC(IN_ROOM(ch))) {
			schedule_crop_growth(GET_MAP_LOC(IN_ROOM(ch)));
		}
		
		extract_obj(obj);
		
		start_action(ch, ACT_PLANTING, 4);
		
		msg_to_char(ch, "You kneel and begin digging holes to plant %s here.\r\n", GET_CROP_NAME(cp));
		sprintf(buf, "$n kneels and begins to plant %s here.", GET_CROP_NAME(cp));
		act(buf, FALSE, ch, 0, 0, TO_ROOM);
	}
}


ACMD(do_play) {
	obj_data *obj;

	if (IS_NPC(ch)) {
		msg_to_char(ch, "NPCs cannot do that.\r\n");
	}
	else if (GET_ACTION(ch) == ACT_MUSIC) {
		msg_to_char(ch, "You stop playing music.\r\n");
		act("$n stops playing music.", FALSE, ch, 0, 0, TO_ROOM);
		cancel_action(ch);
	}
	else if (GET_ACTION(ch) != ACT_NONE) {
		msg_to_char(ch, "You're busy right now.\r\n");
	}
	else if (!(obj = GET_EQ(ch, WEAR_HOLD)) || GET_OBJ_TYPE(obj) != ITEM_INSTRUMENT) {
		msg_to_char(ch, "You need to hold an instrument to play music!\r\n");
	}
	else if (!obj_has_custom_message(obj, OBJ_CUSTOM_INSTRUMENT_TO_CHAR) || !obj_has_custom_message(obj, OBJ_CUSTOM_INSTRUMENT_TO_ROOM)) {
		msg_to_char(ch, "This instrument can't be played.\r\n");
	}
	else {
		start_action(ch, ACT_MUSIC, 0);
		
		act("You begin to play $p.", FALSE, ch, obj, 0, TO_CHAR);
		act("$n begins to play $p.", FALSE, ch, obj, 0, TO_ROOM);
	}
}


ACMD(do_prospect) {
	if (IS_NPC(ch)) {
		msg_to_char(ch, "You can't prospect.\r\n");
	}
	else if (GET_ACTION(ch) == ACT_PROSPECTING) {
		send_to_char("You stop prospecting.\r\n", ch);
		act("$n stops prospecting.", FALSE, ch, 0, 0, TO_ROOM);
		cancel_action(ch);
	}
	else if (GET_ACTION(ch) != ACT_NONE) {
		send_to_char("You're already busy.\r\n", ch);
	}
	else if (!ROOM_CAN_MINE(IN_ROOM(ch))) {
		send_to_char("You can't prospect here.\r\n", ch);
	}
	else if (!can_use_room(ch, IN_ROOM(ch), GUESTS_ALLOWED)) {
		msg_to_char(ch, "You don't have permission to prospect here.\r\n");
	}
	else if (!can_see_in_dark_room(ch, IN_ROOM(ch), TRUE)) {
		msg_to_char(ch, "It's too dark for you to prospect.\r\n");
	}
	else if (get_room_extra_data(IN_ROOM(ch), ROOM_EXTRA_MINE_GLB_VNUM) > 0 && GET_LOYALTY(ch) && get_room_extra_data(IN_ROOM(ch), ROOM_EXTRA_PROSPECT_EMPIRE) == EMPIRE_VNUM(GET_LOYALTY(ch))) {
		msg_to_char(ch, "You see evidence that someone has already prospected this area...\r\n");
		show_prospect_result(ch, IN_ROOM(ch));
		act("$n quickly prospects the area.", TRUE, ch, NULL, NULL, TO_ROOM);
		command_lag(ch, WAIT_ABILITY);
	}
	else {
		start_action(ch, ACT_PROSPECTING, 6);

		send_to_char("You begin prospecting...\r\n", ch);
		act("$n begins prospecting...", TRUE, ch, 0, 0, TO_ROOM);
	}	
}


ACMD(do_saw) {
	obj_data *obj, *saw;
	char *argptr = arg;
	int number;

	one_argument(argument, arg);
	number = get_number(&argptr);

	if (IS_NPC(ch)) {
		msg_to_char(ch, "NPCs cannot saw.\r\n");
	}
	else if (GET_ACTION(ch) == ACT_SAWING) {
		act("You stop sawing.", FALSE, ch, NULL, NULL, TO_CHAR);
		cancel_action(ch);
	}
	else if (!has_player_tech(ch, PTECH_SAW_COMMAND)) {
		msg_to_char(ch, "You don't have the right ability to saw anything.\r\n");
	}
	else if (!IS_APPROVED(ch) && config_get_bool("craft_approval")) {
		send_config_msg(ch, "need_approval_string");
	}
	else if (!(saw = has_tool(ch, TOOL_SAW)) && !room_has_function_and_city_ok(GET_LOYALTY(ch), IN_ROOM(ch), FNC_SAW)) {
		msg_to_char(ch, "You need to use a saw of some kind to do that.\r\n");
	}
	else if (!saw && !can_use_room(ch, IN_ROOM(ch), GUESTS_ALLOWED)) {
		msg_to_char(ch, "You don't have permission to saw here.\r\n");
	}
	else if (GET_ACTION(ch) != ACT_NONE) {
		msg_to_char(ch, "You're already busy doing something else.\r\n");
	}
	else if (!*argptr) {
		msg_to_char(ch, "Saw what?\r\n");
	}
	else if (!(obj = get_obj_in_list_vis_prefer_interaction(ch, argptr, &number, ch->carrying, INTERACT_SAW)) && !(obj = get_obj_in_list_vis_prefer_interaction(ch, argptr, &number, ROOM_CONTENTS(IN_ROOM(ch)), INTERACT_SAW))) {
		msg_to_char(ch, "You don't seem to have %s %s.\r\n", AN(arg), arg);
	}
	else if (!has_interaction(GET_OBJ_INTERACTIONS(obj), INTERACT_SAW)) {
		msg_to_char(ch, "You can't saw that!\r\n");
	}
	else if (!can_see_in_dark_room(ch, IN_ROOM(ch), TRUE)) {
		msg_to_char(ch, "It's too dark to saw anything here.\r\n");
	}
	else {
		// TODO: move the timer here to a config; timer is halved if there's a
		// SAW function or TOOL_SAW, and halves again if there's a ptech
		start_action(ch, ACT_SAWING, 16);
		
		// store the item that was used
		add_to_resource_list(&GET_ACTION_RESOURCES(ch), RES_OBJECT, GET_OBJ_VNUM(obj), 1, GET_OBJ_CURRENT_SCALE_LEVEL(obj));
		GET_ACTION_VNUM(ch, 0) = GET_OBJ_VNUM(obj);
		GET_ACTION_VNUM(ch, 1) = GET_OBJ_CURRENT_SCALE_LEVEL(obj);

		act("You begin sawing $p.", FALSE, ch, obj, 0, TO_CHAR);
		act("$n begins sawing $p.", TRUE, ch, obj, 0, TO_ROOM);
		extract_obj(obj);
	}
}


ACMD(do_scrape) {
	char *argptr = arg;
	obj_data *obj;
	int number;
	
	one_argument(argument, arg);
	number = get_number(&argptr);
	
	if (IS_NPC(ch)) {
		msg_to_char(ch, "NPCs cannot scrape.\r\n");
	}
	else if (GET_ACTION(ch) == ACT_SCRAPING) {
		act("You stop scraping.", FALSE, ch, NULL, NULL, TO_CHAR);
		cancel_action(ch);
	}
	else if (!has_player_tech(ch, PTECH_SCRAPE_COMMAND)) {
		msg_to_char(ch, "You don't have the right ability to scrape anything.\r\n");
	}
	else if (!IS_APPROVED(ch) && config_get_bool("craft_approval")) {
		send_config_msg(ch, "need_approval_string");
	}
	else if (GET_ACTION(ch) != ACT_NONE) {
		msg_to_char(ch, "You're already busy doing something else.\r\n");
	}
	else if (!*argptr) {
		msg_to_char(ch, "Scrape what?\r\n");
	}
	else if (!(obj = get_obj_in_list_vis_prefer_interaction(ch, argptr, &number, ch->carrying, INTERACT_SCRAPE)) && (!can_use_room(ch, IN_ROOM(ch), GUESTS_ALLOWED) || !(obj = get_obj_in_list_vis_prefer_interaction(ch, argptr, &number, ROOM_CONTENTS(IN_ROOM(ch)), INTERACT_SCRAPE)))) {
		msg_to_char(ch, "You don't seem to have %s %s.\r\n", AN(arg), arg);
	}
	else if (!has_interaction(GET_OBJ_INTERACTIONS(obj), INTERACT_SCRAPE)) {
		msg_to_char(ch, "You can't scrape that!\r\n");
	}
	else if (!has_tool(ch, TOOL_AXE | TOOL_KNIFE)) {
		msg_to_char(ch, "You need to be using a good axe or knife to scrape anything.\r\n");
	}
	else if (!can_see_in_dark_room(ch, IN_ROOM(ch), TRUE)) {
		msg_to_char(ch, "It's too dark to scrape anything here.\r\n");
	}
	else {
		start_action(ch, ACT_SCRAPING, 6);
		
		// store the item that was used
		add_to_resource_list(&GET_ACTION_RESOURCES(ch), RES_OBJECT, GET_OBJ_VNUM(obj), 1, GET_OBJ_CURRENT_SCALE_LEVEL(obj));
		GET_ACTION_VNUM(ch, 0) = GET_OBJ_VNUM(obj);
		GET_ACTION_VNUM(ch, 1) = GET_OBJ_CURRENT_SCALE_LEVEL(obj);
		
		act("You begin scraping $p.", FALSE, ch, obj, 0, TO_CHAR);
		act("$n begins scraping $p.", TRUE, ch, obj, 0, TO_ROOM);
		
		extract_obj(obj);
	}
}


ACMD(do_stop) {
	if (IS_NPC(ch)) {
		msg_to_char(ch, "No, you stop.\r\n");
	}
	else if (cancel_biting(ch)) {
		// sends its own message if the player was biting someone
	}
	else if (GET_ACTION(ch) == ACT_NONE) {
		msg_to_char(ch, "You can stop if you want to.\r\n");
	}
	else {
		cancel_action(ch);
		msg_to_char(ch, "You stop what you were doing.\r\n");
	}
}


ACMD(do_tan) {
	char *argptr = arg;
	obj_data *obj;
	int number;
	
	int tan_timer = config_get_int("tan_timer");

	one_argument(argument, arg);
	number = get_number(&argptr);

	if (IS_NPC(ch)) {
		msg_to_char(ch, "You can't tan.\r\n");
	}
	else if (!IS_APPROVED(ch) && config_get_bool("craft_approval")) {
		send_config_msg(ch, "need_approval_string");
	}
	else if (!has_player_tech(ch, PTECH_TAN)) {
		msg_to_char(ch, "You don't have the correct ability to tan anything.\r\n");
	}
	else if (!*argptr) {
		msg_to_char(ch, "What would you like to tan?\r\n");
	}
	else if (GET_ACTION(ch) != ACT_NONE) {
		msg_to_char(ch, "You're busy right now.\r\n");
	}
	else if (!(obj = get_obj_in_list_vis_prefer_interaction(ch, argptr, &number, ch->carrying, INTERACT_TAN)) && !(obj = get_obj_in_list_vis_prefer_interaction(ch, argptr, &number, ROOM_CONTENTS(IN_ROOM(ch)), INTERACT_TAN))) {
		msg_to_char(ch, "You don't seem to have more to tan.\r\n");
	}
	else if (!has_interaction(GET_OBJ_INTERACTIONS(obj), INTERACT_TAN)) {
		msg_to_char(ch, "You can't tan that!\r\n");
	}
	else if (!can_use_room(ch, IN_ROOM(ch), GUESTS_ALLOWED)) {
		msg_to_char(ch, "You don't have permission to tan here.\r\n");
	}
	else if (!can_see_in_dark_room(ch, IN_ROOM(ch), TRUE)) {
		msg_to_char(ch, "It's too dark to tan anything here.\r\n");
	}
	else {
		start_action(ch, ACT_TANNING, tan_timer);
		
		// store the obj
		add_to_resource_list(&GET_ACTION_RESOURCES(ch), RES_OBJECT, GET_OBJ_VNUM(obj), 1, GET_OBJ_CURRENT_SCALE_LEVEL(obj));
		GET_ACTION_VNUM(ch, 0) = GET_OBJ_VNUM(obj);
		GET_ACTION_VNUM(ch, 1) = GET_OBJ_CURRENT_SCALE_LEVEL(obj);
		
		act("You begin to tan $p.", FALSE, ch, obj, NULL, TO_CHAR);
		act("$n begins to tan $p.", FALSE, ch, obj, NULL, TO_ROOM);

		extract_obj(obj);
	}
}


 //////////////////////////////////////////////////////////////////////////////
//// GENERIC INTERACTIONS: do_gen_interact_room //////////////////////////////
/**
* This system uses the gen_interact_data config array to manage chores that
* meet these critera:
* - have a timed action using an ACT_ type
* - gather/produce a resource using an INTERACT_ type
*/


/**
* Get a gen_interact_data[] entry by type.
*
* @param int interact_type Any INTERACT_ type.
* @return const struct gen_interact_data_t* The gen_interact_data entry.
*/
const struct gen_interact_data_t *get_interact_data_by_type(int interact_type) {
	int iter;
	for (iter = 0; gen_interact_data[iter].interact != -1; ++iter) {
		if (gen_interact_data[iter].interact == interact_type) {
			return &gen_interact_data[iter];
		}
	}
	return NULL;	// not found
}


/**
* Get a gen_interact_data[] entry by action.
*
* @param int act_type Any ACT_ type.
* @return const struct gen_interact_data_t* The gen_interact_data entry.
*/
const struct gen_interact_data_t *get_interact_data_by_action(int act_type) {
	int iter;
	for (iter = 0; gen_interact_data[iter].interact != -1; ++iter) {
		if (gen_interact_data[iter].action == act_type) {
			return &gen_interact_data[iter];
		}
	}
	return NULL;	// not found
}


/**
* Looks for available non-depleted interactions.
*
* @param char_data *ch The player trying to gen-interact.
* @param room_data *room The location.
* @param const struct gen_interact_data_t *data 
* @return bool TRUE if you can, FALSE if not (messages when false).
*/ 
bool can_gen_interact_room(char_data *ch, room_data *room, const struct gen_interact_data_t *data) {
	bool can_room_but_no_permit = FALSE, can_room_but_depleted = FALSE, no_guest_room = FALSE;
	vehicle_data *can_veh_but_no_permit = NULL, *can_veh_but_depleted = NULL;
	char buf[MAX_STRING_LENGTH];
	vehicle_data *veh;
	
	if (!ch || !data) {
		return FALSE;	// safety-only; this should be impossible
	}
	
	// check room first
	if (SECT_CAN_INTERACT_ROOM(room, data->interact) || BLD_CAN_INTERACT_ROOM(room, data->interact) || RMT_CAN_INTERACT_ROOM(room, data->interact) || CROP_CAN_INTERACT_ROOM(room, data->interact)) {
		if (!can_use_room(ch, IN_ROOM(ch), MEMBERS_ONLY)) {
			can_room_but_no_permit = TRUE;	// error later
		}
		else if (data->depletion && get_depletion(room, data->depletion) >= get_interaction_depletion_room(ch, GET_LOYALTY(ch), room, data->interact, FALSE)) {
			can_room_but_depleted = TRUE;	// error later
		}
		else {
			return TRUE;	// room passed basic checks
		}
	}
	
	no_guest_room = !can_use_room(ch, IN_ROOM(ch), GUESTS_ALLOWED);
	
	DL_FOREACH2(ROOM_VEHICLES(room), veh, next_in_room) {
		if (!VEH_IS_COMPLETE(veh) || VEH_HEALTH(veh) < 1 || !has_interaction(VEH_INTERACTIONS(veh), data->interact)) {
			continue;	// can't act on veh at all
		}
		else if (no_guest_room || !can_use_vehicle(ch, veh, MEMBERS_ONLY)) {
			can_veh_but_no_permit = veh;
			continue;
		}
		else if (data->depletion != NOTHING && get_vehicle_depletion(veh, data->depletion) >= get_interaction_depletion(ch, GET_LOYALTY(ch), VEH_INTERACTIONS(veh), data->interact, FALSE)) {
			can_veh_but_depleted = veh;
			continue;
		}
		else {
			return TRUE;	// success!
		}
	}
	
	// if the room or any vehicle was valid, it already returned. Otherwise look for an error:
	if (can_room_but_depleted || can_veh_but_depleted) {
		msg_to_char(ch, "There's nothing to %s here.\r\n", data->command);
	}
	else if (can_room_but_no_permit) {
		msg_to_char(ch, "You don't have permission to %s here.\r\n", data->command);
	}
	else if (can_veh_but_no_permit && no_guest_room) {
		snprintf(buf, sizeof(buf), "You can't %s $V because someone else owns this area.", data->command);
		act(buf, FALSE, ch, NULL, can_veh_but_no_permit, TO_CHAR);
	}
	else if (can_veh_but_no_permit) {
		snprintf(buf, sizeof(buf), "You don't have permission to %s $V.", data->command);
		act(buf, FALSE, ch, NULL, can_veh_but_no_permit, TO_CHAR);
	}
	else {
		msg_to_char(ch, "You can't %s here.\r\n", data->command);
	}
	
	// reached end
	return FALSE;
}


/**
* Determines if you can (still) perform a do_gen_interact_room task.
*
* @param char_data *ch The player.
* @param const struct gen_interact_data_t *data Pointer to the gen_interact_data[] entry.
* @return bool TRUE if it's ok to proceed, FALSE if it sent an error message.
*/
bool validate_gen_interact_room(char_data *ch, const struct gen_interact_data_t *data) {
	if (data->ptech != NOTHING && !has_player_tech(ch, data->ptech)) {
		msg_to_char(ch, "You don't have the correct ability to %s.\r\n", data->command);
	}
	else if (data->approval_config && *(data->approval_config) && !IS_APPROVED(ch) && config_get_bool(data->approval_config)) {
		send_config_msg(ch, "need_approval_string");
	}
	else if (!can_see_in_dark_room(ch, IN_ROOM(ch), TRUE)) {
		msg_to_char(ch, "It's too dark to %s anything here.\r\n", data->command);
	}
	else if (!can_gen_interact_room(ch, IN_ROOM(ch), data)) {
		// sends own messages
	}
	else {
		return TRUE;	// safe!
	}
	
	return FALSE;	// if we got here
}


// note: still need GET_ACTION in here
INTERACTION_FUNC(finish_gen_interact_room) {
	const struct gen_interact_data_t *data = get_interact_data_by_action(GET_ACTION(ch));
	char buf[MAX_STRING_LENGTH];
	obj_data *obj = NULL;
	char *cust;
	int num, amount;
	
	// safety check
	if (!data) {
		return FALSE;
	}
	if (data->depletion != NOTHING && inter_veh && get_vehicle_depletion(inter_veh, data->depletion) >= (interact_one_at_a_time[interaction->type] ? interaction->quantity : config_get_int("common_depletion"))) {
		return FALSE;	// depleted vehicle
	}
	else if (data->depletion != NOTHING && !inter_veh && get_depletion(inter_room, data->depletion) >= (interact_one_at_a_time[interaction->type] ? interaction->quantity : config_get_int("common_depletion"))) {
		return FALSE;	// depleted room
	}
	
	amount = interact_one_at_a_time[interaction->type] ? 1 : interaction->quantity;
	
	// give items
	for (num = 0; num < amount; ++num) {
		obj = read_object(interaction->vnum, TRUE);
		scale_item_to_level(obj, 1);	// minimum level
		obj_to_char_or_room(obj, ch);
		load_otrigger(obj);
	}
	
	// mark gained
	if (GET_LOYALTY(ch)) {
		add_production_total(GET_LOYALTY(ch), interaction->vnum, amount);
	}
	
	// check depletion
	if (data->depletion) {
		if (inter_veh) {
			add_vehicle_depletion(inter_veh, data->depletion, TRUE);
		}
		else {
			add_depletion(inter_room ? inter_room : IN_ROOM(ch), data->depletion, TRUE);
		}
	}
	
	if (obj) {
		cust = obj_get_custom_message(obj, OBJ_CUSTOM_RESOURCE_TO_CHAR);
		if (cust || data->msg.finish[0]) {
			if (amount > 1) {
				sprintf(buf, "%s (x%d)", cust ? cust : data->msg.finish[0], amount);
			}
			else {
				strcpy(buf, cust ? cust : data->msg.finish[0]);
			}
			act(buf, FALSE, ch, obj, NULL, TO_CHAR);
		}
		
		cust = obj_get_custom_message(obj, OBJ_CUSTOM_RESOURCE_TO_ROOM);
		if (cust || data->msg.finish[1]) {
			act(cust ? cust : data->msg.finish[1], FALSE, ch, obj, NULL, TO_ROOM);
		}
	}
	
	return TRUE;
}


/**
* Begins a do_gen_interact_room action.
*
* @param char_data *ch A player with permission to interact here.
* @param const struct gen_interact_data_t *data A pointer to the gen_interact_data[].
*/
void start_gen_interact_room(char_data *ch, const struct gen_interact_data_t *data) {
	if (!data || !can_gen_interact_room(ch, IN_ROOM(ch), data)) {
		// fails silently if !data but that shouldn't even be possible; messages otherwise
		return;
	}
	
	start_action(ch, data->action, data->timer);
	if (data->msg.start[0]) {
		msg_to_char(ch, "%s\r\n", data->msg.start[0]);
	}
	if (data->msg.start[1]) {
		act(data->msg.start[1], FALSE, ch, NULL, NULL, TO_ROOM);
	}
}


/**
* Ticker function for do_gen_interact_room.
*
* @param char_data *ch The person doing the interaction.
*/
void process_gen_interact_room(char_data *ch) {
	const struct gen_interact_data_t *data;
	room_data *in_room;
	int count, pos;
	
	if (!(data = get_interact_data_by_action(GET_ACTION(ch))) || !validate_gen_interact_room(ch, data)) {
		cancel_action(ch);
		return;
	}
	
	GET_ACTION_TIMER(ch) -= 1;
	
	if (GET_ACTION_TIMER(ch) > 0) {
		// still going: tick messages
		if (data->msg.random_tick[0][0] && number(1, 100) <= data->msg.random_frequency) {
			for (count = 0; data->msg.random_tick[count][0] != NULL; ++count);
			if (count > 0) {
				pos = number(0, count-1);
				if (!PRF_FLAGGED(ch, PRF_NOSPAM)) {
					msg_to_char(ch, "%s\r\n", data->msg.random_tick[pos][0]);
				}
				if (data->msg.random_tick[pos][1]) {
					act(data->msg.random_tick[pos][1], FALSE, ch, NULL, NULL, TO_ROOM | TO_SPAMMY);
				}
			}
		}
	}
	else {	// done
		// do not un-set GET_ACTION before running interactions
		
		if (run_room_interactions(ch, IN_ROOM(ch), data->interact, NULL, MEMBERS_ONLY, finish_gen_interact_room)) {
			GET_ACTION(ch) = ACT_NONE;
			in_room = IN_ROOM(ch);
			
			if (data->ptech) {
				gain_player_tech_exp(ch, data->ptech, 10);
			}
			
			// character is still there? repeat
			if (IN_ROOM(ch) == in_room && GET_ACTION(ch) == ACT_NONE) {
				start_gen_interact_room(ch, data);
			}
		}
		else if (data->msg.empty && *(data->msg.empty)) {
			GET_ACTION(ch) = ACT_NONE;
			msg_to_char(ch, "%s\r\n", data->msg.empty);
		}
		else {
			GET_ACTION(ch) = ACT_NONE;
			msg_to_char(ch, "You find nothing.\r\n");
		}
	}
}


// subcommand is an INTERACT_ type
ACMD(do_gen_interact_room) {
	const struct gen_interact_data_t *data;
	char buf[MAX_STRING_LENGTH];
	
	if (!(data = get_interact_data_by_type(subcmd))) {
		msg_to_char(ch, "This command is not yet implemented.\r\n");
	}
	else if (IS_NPC(ch)) {
		msg_to_char(ch, "NPCs cannot do this.\r\n");
	}
	else if (GET_ACTION(ch) == data->action) {
		msg_to_char(ch, "You stop %s.\r\n", data->verb);
		snprintf(buf, sizeof(buf), "$n stops %s.", data->verb);
		act(buf, FALSE, ch, 0, 0, TO_ROOM);
		cancel_action(ch);
	}
	else if (GET_ACTION(ch) != ACT_NONE) {
		send_to_char("You're already busy.\r\n", ch);
	}
	else if (!validate_gen_interact_room(ch, data)) {
		// sends own message
	}
	else {
		start_gen_interact_room(ch, data);
	}
}
