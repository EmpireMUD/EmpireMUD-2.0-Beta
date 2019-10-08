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
*/

// external vars
extern const sector_vnum climate_default_sector[NUM_CLIMATES];

// external funcs
extern room_data *dir_to_room(room_data *room, int dir, bool ignore_entrance);
extern double get_base_dps(obj_data *weapon);
extern obj_data *find_chip_weapon(char_data *ch);
extern obj_data *find_lighter_in_list(obj_data *list, bool *had_keep);
extern char *get_mine_type_name(room_data *room);
extern obj_data *has_sharp_tool(char_data *ch);
extern bool is_deep_mine(room_data *room);
void process_build(char_data *ch, room_data *room, int act_type);
void scale_item_to_level(obj_data *obj, int level);
void schedule_crop_growth(struct map_data *map);

// local prototypes
obj_data *has_shovel(char_data *ch);

// cancel protos
void cancel_resource_list(char_data *ch);
void cancel_driving(char_data *ch);
void cancel_gen_craft(char_data *ch);
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
void process_build_action(char_data *ch);
void process_burn_area(char_data *ch);
void process_chop(char_data *ch);
void process_copying_book(char_data *ch);
void process_digging(char_data *ch);
void process_dismantle_action(char_data *ch);
void process_escaping(char_data *ch);
void process_excavating(char_data *ch);
void process_fillin(char_data *ch);
void process_fishing(char_data *ch);
void process_gathering(char_data *ch);
void process_gen_craft(char_data *ch);
void process_harvesting(char_data *ch);
void process_maintenance(char_data *ch);
void process_mining(char_data *ch);
void process_minting(char_data *ch);
void process_morphing(char_data *ch);
void process_music(char_data *ch);
void process_panning(char_data *ch);
void process_picking(char_data *ch);
void process_planting(char_data *ch);
void process_prospecting(char_data *ch);
void process_quarrying(char_data *ch);
void process_reading(char_data *ch);
void process_reclaim(char_data *ch);
void process_repairing(char_data *ch);
void process_running(char_data *ch);
void process_scraping(char_data *ch);
void process_siring(char_data *ch);
void process_start_fillin(char_data *ch);
void process_swap_skill_sets(char_data *ch);
void process_tanning(char_data *ch);


 //////////////////////////////////////////////////////////////////////////////
//// CONTROL DATA ////////////////////////////////////////////////////////////

// ACT_x
const struct action_data_struct action_data[] = {
	{ "", "", NOBITS, NULL, NULL },	// ACT_NONE
	{ "digging", "is digging at the ground.", ACTF_SHOVEL | ACTF_FINDER | ACTF_HASTE | ACTF_FAST_CHORES, process_digging, NULL },	// ACT_DIGGING
	{ "gathering", "is gathering plant material.", ACTF_FINDER | ACTF_HASTE | ACTF_FAST_CHORES, process_gathering, NULL },	// ACT_GATHERING
	{ "chopping", "is chopping down trees.", ACTF_HASTE | ACTF_FAST_CHORES, process_chop, NULL },	// ACT_CHOPPING
	{ "building", "is hard at work building.", ACTF_HASTE | ACTF_FAST_CHORES, process_build_action, NULL },	// ACT_BUILDING
	{ "dismantling", "is dismantling the building.", ACTF_HASTE | ACTF_FAST_CHORES, process_dismantle_action, NULL },	// ACT_DISMANTLING
	{ "harvesting", "is harvesting the crop.", ACTF_HASTE | ACTF_FAST_CHORES, process_harvesting, NULL },	// ACT_HARVESTING
	{ "planting", "is planting seeds.", ACTF_HASTE | ACTF_FAST_CHORES, process_planting, NULL },	// ACT_PLANTING
	{ "mining", "is mining at the walls.", ACTF_HASTE | ACTF_FAST_CHORES, process_mining, NULL },	// ACT_MINING
	{ "minting", "is minting coins.", ACTF_ALWAYS_FAST | ACTF_FAST_CHORES, process_minting, cancel_minting },	// ACT_MINTING
	{ "fishing", "is fishing.", ACTF_SITTING, process_fishing, NULL },	// ACT_FISHING
	{ "preparing", "is preparing to fill in the trench.", NOBITS, process_start_fillin, NULL },	// ACT_START_FILLIN
	{ "repairing", "is doing some repairs.", ACTF_FAST_CHORES | ACTF_HASTE, process_repairing, NULL },	// ACT_REPAIRING
	{ "chipping", "is chipping flints.", ACTF_FAST_CHORES, process_chipping, cancel_resource_list },	// ACT_CHIPPING
	{ "panning", "is panning for gold.", ACTF_FINDER, process_panning, NULL },	// ACT_PANNING
	{ "music", "is playing soothing music.", ACTF_ANYWHERE | ACTF_HASTE, process_music, NULL },	// ACT_MUSIC
	{ "excavating", "is excavating a trench.", ACTF_HASTE | ACTF_FAST_CHORES | ACTF_FAST_EXCAVATE, process_excavating, NULL },	// ACT_EXCAVATING
	{ "siring", "is hunched over.", NOBITS, process_siring, cancel_siring },	// ACT_SIRING
	{ "picking", "is looking around at the ground.", ACTF_FINDER | ACTF_HASTE | ACTF_FAST_CHORES, process_picking, NULL },	// ACT_PICKING
	{ "morphing", "is morphing and changing shape!", ACTF_ANYWHERE, process_morphing, cancel_morphing },	// ACT_MORPHING
	{ "scraping", "is scraping something off.", ACTF_HASTE | ACTF_FAST_CHORES, process_scraping, cancel_resource_list },	// ACT_SCRAPING
	{ "bathing", "is bathing in the water.", NOBITS, process_bathing, NULL },	// ACT_BATHING
	{ "chanting", "is chanting a strange song.", NOBITS, perform_ritual, NULL },	// ACT_CHANTING
	{ "prospecting", "is prospecting.", ACTF_FAST_PROSPECT, process_prospecting, NULL },	// ACT_PROSPECTING
	{ "filling", "is filling in the trench.", ACTF_HASTE | ACTF_FAST_CHORES | ACTF_FAST_EXCAVATE, process_fillin, NULL },	// ACT_FILLING_IN
	{ "reclaiming", "is reclaiming this acre!", NOBITS, process_reclaim, NULL },	// ACT_RECLAIMING
	{ "escaping", "is running toward the window!", NOBITS, process_escaping, NULL },	// ACT_ESCAPING
	{ "running", "runs past you.", ACTF_ALWAYS_FAST | ACTF_EVEN_FASTER | ACTF_FASTER_BONUS | ACTF_ANYWHERE, process_running, cancel_movement_string },	// unused
	{ "ritual", "is performing an arcane ritual.", NOBITS, perform_ritual, NULL },	// ACT_RITUAL
	{ "sawing", "is sawing something.", ACTF_HASTE | ACTF_FAST_CHORES, perform_saw, cancel_resource_list },	// ACT_SAWING
	{ "quarrying", "is quarrying stone.", ACTF_HASTE | ACTF_FAST_CHORES, process_quarrying, NULL },	// ACT_QUARRYING
	{ "driving", "is driving.", ACTF_VEHICLE_SPEEDS | ACTF_SITTING, process_driving, cancel_driving },	// ACT_DRIVING
	{ "tanning", "is tanning leather.", ACTF_FAST_CHORES, process_tanning, cancel_resource_list },	// ACT_TANNING
	{ "reading", "is reading a book.", ACTF_SITTING, process_reading, NULL },	// ACT_READING
	{ "copying", "is writing out a copy of a book.", NOBITS, process_copying_book, NULL },	// ACT_COPYING_BOOK
	{ "crafting", "is working on something.", NOBITS, process_gen_craft, cancel_gen_craft },	// ACT_GEN_CRAFT
	{ "sailing", "is sailing the ship.", ACTF_VEHICLE_SPEEDS | ACTF_SITTING, process_driving, cancel_driving },	// ACT_SAILING
	{ "piloting", "is piloting the vessel.", ACTF_VEHICLE_SPEEDS | ACTF_SITTING, process_driving, cancel_driving },	// ACT_PILOTING
	{ "skillswap", "is swapping skill sets.", NOBITS, process_swap_skill_sets, NULL },	// ACT_SWAP_SKILL_SETS
	{ "maintenance", "is repairing the building.", ACTF_HASTE | ACTF_FAST_CHORES, process_maintenance, NULL },	// ACT_MAINTENANCE
	{ "burning", "is preparing to burn the area.", ACTF_FAST_CHORES, process_burn_area, NULL },	// ACT_BURN_AREA
	
	{ "\n", "\n", NOBITS, NULL, NULL }
};


 //////////////////////////////////////////////////////////////////////////////
//// CORE ACTION FUNCTIONS ///////////////////////////////////////////////////


/**
* Ends the character's current timed action prematurely.
*
* @param char_data *ch The actor.
*/
void cancel_action(char_data *ch) {
	if (GET_ACTION(ch) != ACT_NONE) {
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
* @param int chore CHORE_ or NOTHING to not stop workforce.
*/
void stop_room_action(room_data *room, int action, int chore) {
	extern struct empire_chore_type chore_data[NUM_CHORES];
	
	char_data *c;
	
	for (c = ROOM_PEOPLE(room); c; c = c->next_in_room) {
		// player actions
		if (action != NOTHING && !IS_NPC(c) && GET_ACTION(c) == action) {
			cancel_action(c);
		}
		// mob actions
		if (chore != NOTHING && IS_NPC(c) && GET_MOB_VNUM(c) == chore_data[chore].mob) {
			SET_BIT(MOB_FLAGS(c), MOB_SPAWNED);
		}
	}
}


/**
* This is the main processor for periodic actions (ACT_), once per second.
*/
void update_actions(void) {
	// Extern functions.
	extern vehicle_data *get_current_piloted_vehicle(char_data *ch);
	
	// Extern vars.
	extern struct gen_craft_data_t gen_craft_data[];
	extern bool catch_up_actions;
	
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
		if (IS_SET(act_flags, ACTF_SHOVEL) && has_shovel(ch)) {
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
		if (AFF_FLAGGED(ch, AFF_SLOW) || IS_HUNGRY(ch) || IS_THIRSTY(ch) || IS_BLOOD_STARVED(ch)) {
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
* Finds a shovel for the character.
*
* @param char_data *ch The person who might have a shovel.
* @return obj_data* The character's shovel, or NULL if they have none.
*/
obj_data *has_shovel(char_data *ch) {
	obj_data *shovel = NULL;
	int iter;
	
	// list of valid slots; terminate with -1
	int slots[] = { WEAR_WIELD, WEAR_HOLD, WEAR_SHEATH_1, WEAR_SHEATH_2, -1 };
	
	for (iter = 0; slots[iter] != -1; ++iter) {
		shovel = GET_EQ(ch, slots[iter]);
		if (shovel && OBJ_FLAGGED(shovel, OBJ_TOOL_SHOVEL)) {
			return shovel;
		}
	}
	
	return NULL;
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
* Returns a set of resources from the resource list (if any).
*
* @param char_data *ch The person canceling the craft (or whatever).
*/
void cancel_resource_list(char_data *ch) {
	give_resources(ch, GET_ACTION_RESOURCES(ch), FALSE);
	free_resource_list(GET_ACTION_RESOURCES(ch));
	GET_ACTION_RESOURCES(ch) = NULL;
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
	
	if (!ROOM_AFF_FLAGGED(IN_ROOM(ch), ROOM_AFF_UNCLAIMABLE) && !can_use_room(ch, IN_ROOM(ch), MEMBERS_ONLY)) {
		msg_to_char(ch, "You don't have permission to chop here.\r\n");
	}
	else if (!CAN_CHOP_ROOM(IN_ROOM(ch)) || get_depletion(IN_ROOM(ch), DPLTN_CHOP) >= config_get_int("chop_depletion")) {
		msg_to_char(ch, "There's nothing left here to chop.\r\n");
	}
	else {
		start_action(ch, ACT_CHOPPING, 0);
		
		// ensure progress data is set up
		if (get_room_extra_data(IN_ROOM(ch), ROOM_EXTRA_CHOP_PROGRESS) <= 0) {
			set_room_extra_data(IN_ROOM(ch), ROOM_EXTRA_CHOP_PROGRESS, config_get_int("chop_timer"));
		}
		
		if (GET_EQ(ch, WEAR_WIELD)) {
			strcpy(weapon, GET_OBJ_SHORT_DESC(GET_EQ(ch, WEAR_WIELD)));
		}
		else {
			strcpy(weapon, "axe");
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
* Starts the mining action where possible.
*
* @param char_data *ch The prospective miner.
*/
void start_mining(char_data *ch) {
	int mining_timer = config_get_int("mining_timer");
	
	if (room_has_function_and_city_ok(IN_ROOM(ch), FNC_MINE)) {
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


/**
* begins the "pick" action
*
* @param char_data *ch The prospective picker.
*/
void start_picking(char_data *ch) {
	int pick_base_timer = config_get_int("pick_base_timer");
	
	start_action(ch, ACT_PICKING, pick_base_timer);
	send_to_char("You begin looking around.\r\n", ch);
}


/**
* Begin to quarry.
*
* @param char_data *ch The player who is to quarry.
*/
void start_quarrying(char_data *ch) {	
	if (can_interact_room(IN_ROOM(ch), INTERACT_QUARRY) && IS_COMPLETE(IN_ROOM(ch))) {
		if (get_depletion(IN_ROOM(ch), DPLTN_QUARRY) >= config_get_int("common_depletion")) {
			msg_to_char(ch, "There's not enough left to quarry here.\r\n");
		}
		else {
			start_action(ch, ACT_QUARRYING, 12);
			send_to_char("You begin to work the quarry.\r\n", ch);
			act("$n begins to work the quarry.", TRUE, ch, 0, 0, TO_ROOM);
		}
	}
}


 //////////////////////////////////////////////////////////////////////////////
//// ACTION FINISHERS ////////////////////////////////////////////////////////

INTERACTION_FUNC(finish_chopping) {
	char buf[MAX_STRING_LENGTH];
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
		if (interaction->quantity > 1) {
			sprintf(buf, "With a loud crack, $p (x%d) falls!", interaction->quantity);
			act(buf, FALSE, ch, obj, NULL, TO_CHAR);
		}
		else {
			act("With a loud crack, $p falls!", FALSE, ch, obj, NULL, TO_CHAR);
		}		
		
		act("$n collects $p.", FALSE, ch, obj, NULL, TO_ROOM);
	}
	
	return TRUE;
}


INTERACTION_FUNC(finish_digging) {	
	obj_vnum vnum = interaction->vnum;
	obj_data *obj = NULL;
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
		
		if (interaction->quantity > 1) {
			sprintf(buf1, "You pull $p from the ground (x%d)!", interaction->quantity);
		}
		else {
			strcpy(buf1, "You pull $p from the ground!");
		}
		
		if (obj) {
			act(buf1, FALSE, ch, obj, 0, TO_CHAR);
			act("$n pulls $p from the ground!", FALSE, ch, obj, 0, TO_ROOM);
		}
	}
	
	return TRUE;
}


INTERACTION_FUNC(finish_fishing) {
	char buf[MAX_STRING_LENGTH];
	obj_data *obj = NULL;
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
		if (interaction->quantity > 1) {
			sprintf(buf, "You jab your spear into the water and when you extract it you find $p (x%d) on the end!", interaction->quantity);
			act(buf, FALSE, ch, obj, NULL, TO_CHAR);
		}
		else {
			act("You jab your spear into the water and when you extract it you find $p on the end!", FALSE, ch, obj, NULL, TO_CHAR);
		}
		
		act("$n jabs $s spear into the water and when $e draws it out, it has $p on the end!", TRUE, ch, obj, NULL, TO_ROOM);
	}
	
	return TRUE;
}


INTERACTION_FUNC(finish_gathering) {
	obj_data *obj = NULL;
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
		if (interaction->quantity > 1) {
			sprintf(buf, "You find $p (x%d)!", interaction->quantity);
		}
		else {
			strcpy(buf, "You find $p!");
		}
		
		act(buf, FALSE, ch, obj, 0, TO_CHAR);
		act("$n finds $p!", TRUE, ch, obj, 0, TO_ROOM);
		
		gain_ability_exp(ch, ABIL_SCAVENGING, 10);
		
		// action does not end normally
		
		return TRUE;
	}
	
	return FALSE;
}


INTERACTION_FUNC(finish_harvesting) {
	crop_data *cp;
	int count, num;
	obj_data *obj = NULL;
		
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
			sprintf(buf, "You got $p (x%d)!", num);
			act(buf, FALSE, ch, obj, FALSE, TO_CHAR);
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
	int iter;
	
	for (iter = 0; iter < interaction->quantity; ++iter) {
		obj = read_object(interaction->vnum, TRUE);
		scale_item_to_level(obj, 1);	// ensure scaling
		obj_to_char_or_room(obj, ch);

		act("With that last stroke, $p falls from the wall!", FALSE, ch, obj, 0, TO_CHAR);
		act("With $s last stroke, $p falls from the wall where $n was picking!", FALSE, ch, obj, 0, TO_ROOM);

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
		if (interaction->quantity > 1) {
			sprintf(buf, "You find $p (x%d)!", interaction->quantity);
			act(buf, FALSE, ch, obj, NULL, TO_CHAR);
		}
		else {
			act("You find $p!", FALSE, ch, obj, NULL, TO_CHAR);
		}
		
		act("$n finds $p!", FALSE, ch, obj, NULL, TO_ROOM);
	}
	
	return TRUE;
}


INTERACTION_FUNC(finish_picking) {
	obj_data *obj = NULL;
	room_data *in_room = IN_ROOM(ch);
	obj_vnum vnum = interaction->vnum;
	int iter, num = interaction->quantity;

	// give objs
	for (iter = 0; iter < num; ++iter) {
		obj = read_object(vnum, TRUE);
		scale_item_to_level(obj, 1);	// minimum level
		obj_to_char_or_room(obj, ch);
		add_depletion(inter_room, DPLTN_PICK, TRUE);
		load_otrigger(obj);
	}
	
	// mark gained
	if (GET_LOYALTY(ch)) {
		add_production_total(GET_LOYALTY(ch), vnum, num);
	}
	
	if (obj) {
		if (num > 1) {
			sprintf(buf, "You find $p (x%d)!", num);
			act(buf, FALSE, ch, obj, 0, TO_CHAR);
		}
		else {
			act("You find $p!", FALSE, ch, obj, 0, TO_CHAR);
		}
		act("$n finds $p!", TRUE, ch, obj, 0, TO_ROOM);
	}
	else {
		msg_to_char(ch, "You find nothing.\r\n");
	}
		
	// re-start
	if (in_room == IN_ROOM(ch)) {
		start_picking(ch);
	}
	
	return TRUE;
}


INTERACTION_FUNC(finish_quarrying) {
	char buf[MAX_STRING_LENGTH];
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
	
	if (interaction->quantity > 1) {
		sprintf(buf, "You give the plug drill one final swing and pry loose $p (x%d)!", interaction->quantity);
	}
	else {
		strcpy(buf, "You give the plug drill one final swing and pry loose $p!");
	}
	
	if (obj) {
		act(buf, FALSE, ch, obj, NULL, TO_CHAR);
		act("$n hits the plug drill hard with a hammer and pries loose $p!", FALSE, ch, obj, NULL, TO_ROOM);
	}
	
	return TRUE;
}


// also used for sawing, tanning, chipping
INTERACTION_FUNC(finish_scraping) {
	obj_vnum vnum = interaction->vnum;
	char buf[MAX_STRING_LENGTH];
	obj_data *load = NULL;
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

	if (interaction->quantity > 1) {
		sprintf(buf, "You get $p (x%d).", interaction->quantity);
	}
	else {
		strcpy(buf, "You get $p.");
	}
		
	if (load) {
		act(buf, FALSE, ch, load, NULL, TO_CHAR);
	}
	
	return TRUE;
}


 //////////////////////////////////////////////////////////////////////////////
//// ACTION PROCESSES ////////////////////////////////////////////////////////

// for do_saw
void perform_saw(char_data *ch) {
	ACMD(do_saw);
	
	char buf[MAX_STRING_LENGTH];
	bool success = FALSE;
	obj_data *proto;
	
	if (!CAN_SEE_IN_DARK_ROOM(ch, IN_ROOM(ch))) {
		msg_to_char(ch, "It's too dark to finish sawing.\r\n");
		cancel_action(ch);
		return;
	}
	
	if (!PRF_FLAGGED(ch, PRF_NOSPAM)) {
		msg_to_char(ch, "You saw %s...\r\n", get_obj_name_by_proto(GET_ACTION_VNUM(ch, 0)));
	}
	
	// base
	GET_ACTION_TIMER(ch) -= 1;
	
	if (has_player_tech(ch, PTECH_FAST_WOOD_PROCESSING)) {
		GET_ACTION_TIMER(ch) -= 1;
	}
		
	if (GET_ACTION_TIMER(ch) <= 0) {
		// will extract no matter what happens here
		if ((proto = obj_proto(GET_ACTION_VNUM(ch, 0)))) {
			act("You finish sawing $p.", FALSE, ch, proto, NULL, TO_CHAR);
			act("$n finishes sawing $p.", TRUE, ch, proto, NULL, TO_ROOM);
			
			success = run_interactions(ch, proto->interactions, INTERACT_SAW, IN_ROOM(ch), NULL, proto, finish_scraping);
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
			gain_ability_exp(ch, ABIL_PRIMITIVE_CRAFTS, 10);
			
			// lather, rinse, rescrape
			do_saw(ch, fname(GET_OBJ_KEYWORDS(proto)), 0, 0);
		}
	}
}


/**
* Tick update for bathing action.
*
* @param char_data *ch The bather.
*/
void process_bathing(char_data *ch) {
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
	int count, total;
	
	if (!CAN_SEE_IN_DARK_ROOM(ch, IN_ROOM(ch))) {
		msg_to_char(ch, "It's too dark to keep working here.\r\n");
		cancel_action(ch);
		return;
	}

	total = 1;	// number of materials to attach in one go (add things that speed up building)
	for (count = 0; count < total && GET_ACTION(ch) == ACT_BUILDING; ++count) {
		process_build(ch, IN_ROOM(ch), ACT_BUILDING);
	}
}


/**
* Tick update for burn area / do_burn_area.
*
* @param char_data *ch The person burning the area.
*/
void process_burn_area(char_data *ch) {
	void perform_burn_room(room_data *room);
	extern bool used_lighter(char_data *ch, obj_data *obj);
	
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
		stop_room_action(IN_ROOM(ch), ACT_BURN_AREA, CHORE_BURN_STUMPS);
		
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
	
	if (!find_chip_weapon(ch)) {
		msg_to_char(ch, "You need to be wielding some kind of hammer or rock to chip it.\r\n");
		cancel_action(ch);
		return;
	}
	if (!CAN_SEE_IN_DARK_ROOM(ch, IN_ROOM(ch))) {
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
		
		success = run_interactions(ch, proto->interactions, INTERACT_CHIP, IN_ROOM(ch), NULL, proto, finish_scraping);
		free_resource_list(GET_ACTION_RESOURCES(ch));
		GET_ACTION_RESOURCES(ch) = NULL;

		if (success) {
			gain_ability_exp(ch, ABIL_PRIMITIVE_CRAFTS, 25);
			
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
	extern int change_chop_territory(room_data *room);
	
	bool got_any = FALSE;
	char_data *ch_iter;
	
	if (!GET_EQ(ch, WEAR_WIELD) || GET_WEAPON_TYPE(GET_EQ(ch, WEAR_WIELD)) != TYPE_SLICE) {
		send_to_char("You need to be wielding an axe to chop.\r\n", ch);
		cancel_action(ch);
		return;
	}
	if (!CAN_SEE_IN_DARK_ROOM(ch, IN_ROOM(ch))) {
		msg_to_char(ch, "It's too dark to finish chopping.\r\n");
		cancel_action(ch);
		return;
	}

	add_to_room_extra_data(IN_ROOM(ch), ROOM_EXTRA_CHOP_PROGRESS, -1 * (GET_STRENGTH(ch) + 3 * get_base_dps(GET_EQ(ch, WEAR_WIELD))));
	act("You swing $p hard!", FALSE, ch, GET_EQ(ch, WEAR_WIELD), NULL, TO_CHAR | TO_SPAMMY);
	act("$n swings $p hard!", FALSE, ch, GET_EQ(ch, WEAR_WIELD), NULL, TO_ROOM | TO_SPAMMY);
	
	// complete?
	if (get_room_extra_data(IN_ROOM(ch), ROOM_EXTRA_CHOP_PROGRESS) <= 0) {
		remove_room_extra_data(IN_ROOM(ch), ROOM_EXTRA_CHOP_PROGRESS);
		
		// run interacts for items only if not depleted
		if (get_depletion(IN_ROOM(ch), DPLTN_CHOP) < config_get_int("chop_depletion")) {
			got_any = run_room_interactions(ch, IN_ROOM(ch), INTERACT_CHOP, finish_chopping);
		}
		
		if (!got_any) {
			// likely didn't get a completion message
			act("You finish chopping.", FALSE, ch, NULL, NULL, TO_CHAR);
			act("$n finishes chopping.", TRUE, ch, NULL, NULL, TO_ROOM);
		}
		
		// attempt to change terrain
		change_chop_territory(IN_ROOM(ch));
		
		gain_ability_exp(ch, ABIL_SCAVENGING, 15);
		
		// stoppin choppin -- don't use stop_room_action because we also restart them
		// (this includes ch)
		for (ch_iter = ROOM_PEOPLE(IN_ROOM(ch)); ch_iter; ch_iter = ch_iter->next_in_room) {
			if (!IS_NPC(ch_iter) && GET_ACTION(ch_iter) == ACT_CHOPPING) {
				cancel_action(ch_iter);
				start_chopping(ch_iter);
			}
		}
		// but stop npc choppers
		stop_room_action(IN_ROOM(ch), NOTHING, CHORE_CHOPPING);
		// and harvesters
		stop_room_action(IN_ROOM(ch), NOTHING, CHORE_FARMING);
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
	
	if (!CAN_SEE_IN_DARK_ROOM(ch, IN_ROOM(ch))) {
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
		
		if (get_depletion(IN_ROOM(ch), DPLTN_DIG) < DEPLETION_LIMIT(IN_ROOM(ch)) && run_room_interactions(ch, IN_ROOM(ch), INTERACT_DIG, finish_digging)) {
			// success
			gain_ability_exp(ch, ABIL_SCAVENGING, 10);
		
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
	LL_FOREACH2(ROOM_PEOPLE(IN_ROOM(ch)), iter, next_in_room) {
		if (!AFF_FLAGGED(iter, AFF_EARTHMELD)) {
			continue;
		}
		if (iter == ch || IS_IMMORTAL(iter) || IS_NPC(iter) || IS_DEAD(iter) || EXTRACTED(iter)) {
			continue;
		}
		
		// earthmeld damage
		msg_to_char(iter, "You feel nature burning at your earthmelded form as someone digs above you!\r\n");
		apply_dot_effect(iter, ATYPE_NATURE_BURN, 6, DAM_MAGICAL, 5, 60, iter);
	}
}


/**
* Tick update for dismantle action.
*
* @param char_data *ch The dismantler.
*/
void process_dismantle_action(char_data *ch) {
	void process_dismantling(char_data *ch, room_data *room);
	
	int count, total;
	
	if (!CAN_SEE_IN_DARK_ROOM(ch, IN_ROOM(ch))) {
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
	void perform_escape(char_data *ch);
	
	if (!CAN_SEE_IN_DARK_ROOM(ch, IN_ROOM(ch))) {
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
	void finish_trench(room_data *room);
	
	int count, total;
	char_data *iter;

	total = 1;	// shovelfuls at once (add things that speed up excavate)
	for (count = 0; count < total && GET_ACTION(ch) == ACT_EXCAVATING; ++count) {
		if (!has_shovel(ch)) {
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
				stop_room_action(IN_ROOM(ch), ACT_EXCAVATING, NOTHING);
			}
		}
	}
	
	// look for earthmelders
	LL_FOREACH2(ROOM_PEOPLE(IN_ROOM(ch)), iter, next_in_room) {
		if (!AFF_FLAGGED(iter, AFF_EARTHMELD)) {
			continue;
		}
		if (iter == ch || IS_IMMORTAL(iter) || IS_NPC(iter) || IS_DEAD(iter) || EXTRACTED(iter)) {
			continue;
		}
		
		// earthmeld damage
		msg_to_char(iter, "You feel nature burning at your earthmelded form as someone digs above you!\r\n");
		apply_dot_effect(iter, ATYPE_NATURE_BURN, 6, DAM_MAGICAL, 5, 60, iter);
	}
}


/**
* Tick update for fillin action.
*
* @param char_data *ch The filler-inner.
*/
void process_fillin(char_data *ch) {
	void untrench_room(room_data *room);

	int count, total;

	total = 1;	// shovelfuls to fill in at once (add things that speed up fillin)
	for (count = 0; count < total && GET_ACTION(ch) == ACT_FILLING_IN; ++count) {
		if (!has_shovel(ch)) {
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
	int dir;
	
	dir = GET_ACTION_VNUM(ch, 0);
	room = (dir == NO_DIR) ? IN_ROOM(ch) : dir_to_room(IN_ROOM(ch), dir, FALSE);
	
	if (!GET_EQ(ch, WEAR_WIELD) || GET_OBJ_TYPE(GET_EQ(ch, WEAR_WIELD)) != ITEM_WEAPON || GET_WEAPON_TYPE(GET_EQ(ch, WEAR_WIELD)) != TYPE_JAB) {
		msg_to_char(ch, "You'll need a spear to fish.\r\n");
		cancel_action(ch);
		return;
	}
	if (!room || !can_interact_room(room, INTERACT_FISH) || !can_use_room(ch, room, MEMBERS_ONLY)) {
		msg_to_char(ch, "You can no longer fish %s.\r\n", (room == IN_ROOM(ch)) ? "here" : "there");
		cancel_action(ch);
		return;
	}
	if (!CAN_SEE_IN_DARK_ROOM(ch, IN_ROOM(ch))) {
		msg_to_char(ch, "It's too dark to fish here.\r\n");
		cancel_action(ch);
		return;
	}
	
	GET_ACTION_TIMER(ch) -= GET_CHARISMA(ch) + (player_tech_skill_check(ch, PTECH_FISH, DIFF_MEDIUM) ? 2 : 0);
	
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
		success = run_room_interactions(ch, room, INTERACT_FISH, finish_fishing);
		
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
			if (run_room_interactions(ch, IN_ROOM(ch), INTERACT_GATHER, finish_gathering)) {
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
	if (!ROOM_CROP(IN_ROOM(ch))) {
		msg_to_char(ch, "There's nothing left to harvest here.\r\n");
		cancel_action(ch);
		return;
	}
	if (!GET_EQ(ch, WEAR_WIELD) || (GET_WEAPON_TYPE(GET_EQ(ch, WEAR_WIELD)) != TYPE_SLICE && GET_WEAPON_TYPE(GET_EQ(ch, WEAR_WIELD)) != TYPE_SLASH)) {
		send_to_char("You're not wielding the proper tool for harvesting.\r\n", ch);
		cancel_action(ch);
		return;
	}
	if (!CAN_SEE_IN_DARK_ROOM(ch, IN_ROOM(ch))) {
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
		stop_room_action(IN_ROOM(ch), ACT_HARVESTING, CHORE_FARMING);
		
		act("You finish harvesting the crop!", FALSE, ch, NULL, NULL, TO_CHAR);
		act("$n finished harvesting the crop!", FALSE, ch, NULL, NULL, TO_ROOM);
		
		if (run_room_interactions(ch, IN_ROOM(ch), INTERACT_HARVEST, finish_harvesting)) {
			// skillups
			gain_ability_exp(ch, ABIL_SCAVENGING, 30);
			gain_player_tech_exp(ch, PTECH_HARVEST_UPGRADE, 5);
		}
		else {
			msg_to_char(ch, "You fail to harvest anything here.\r\n");
		}
		
		// change the sector: attempt to detect
		crop_data *cp = ROOM_CROP(IN_ROOM(ch));
		sector_data *sect = cp ? sector_proto(climate_default_sector[GET_CROP_CLIMATE(cp)]) : NULL;
		if (!sect) {
			sect = find_first_matching_sector(NOBITS, SECTF_HAS_CROP_DATA | SECTF_CROP | SECTF_MAP_BUILDING | SECTF_INSIDE | SECTF_ADVENTURE);
		}
		if (sect) {
			change_terrain(IN_ROOM(ch), GET_SECT_VNUM(sect));
		}
	}
}


/**
* Tick update for maintenance.
*
* @param char_data *ch The repairman.
*/
void process_maintenance(char_data *ch) {
	if (!CAN_SEE_IN_DARK_ROOM(ch, IN_ROOM(ch))) {
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
	int count, total;
	room_data *in_room;
	bool success;
	
	if (!CAN_SEE_IN_DARK_ROOM(ch, IN_ROOM(ch))) {
		msg_to_char(ch, "It's too dark to finish mining.\r\n");
		cancel_action(ch);
		return;
	}

	total = 1;	// pick swings at once (add things that speed up mining)
	for (count = 0; count < total && GET_ACTION(ch) == ACT_MINING; ++count) {
		if (!GET_EQ(ch, WEAR_WIELD) || ((GET_WEAPON_TYPE(GET_EQ(ch, WEAR_WIELD)) != TYPE_PICK) && GET_OBJ_VNUM(GET_EQ(ch, WEAR_WIELD)) != o_HANDAXE)) {
			send_to_char("You're not using the proper tool for mining!\r\n", ch);
			cancel_action(ch);
			break;
		}
		if (!room_has_function_and_city_ok(IN_ROOM(ch), FNC_MINE)) {
			msg_to_char(ch, "You can't mine here.\r\n");
			cancel_action(ch);
			break;
		}
		if (get_room_extra_data(IN_ROOM(ch), ROOM_EXTRA_MINE_AMOUNT) <= 0) {
			msg_to_char(ch, "You don't see even a hint of anything worthwhile in this depleted mine.\r\n");
			cancel_action(ch);
			break;
		}

		GET_ACTION_TIMER(ch) -= GET_STRENGTH(ch) + 3 * get_base_dps(GET_EQ(ch, WEAR_WIELD));

		act("You pick at the walls with $p, looking for ore.", FALSE, ch, GET_EQ(ch, WEAR_WIELD), 0, TO_CHAR | TO_SPAMMY);
		act("$n picks at the walls with $p, looking for ore.", FALSE, ch, GET_EQ(ch, WEAR_WIELD), 0, TO_ROOM | TO_SPAMMY);

		// done??
		if (GET_ACTION_TIMER(ch) <= 0) {
			in_room = IN_ROOM(ch);
			
			// amount of ore remaining
			add_to_room_extra_data(IN_ROOM(ch), ROOM_EXTRA_MINE_AMOUNT, -1);
			
			glb = global_proto(get_room_extra_data(IN_ROOM(ch), ROOM_EXTRA_MINE_GLB_VNUM));
			if (!glb || GET_GLOBAL_TYPE(glb) != GLOBAL_MINE_DATA) {
				msg_to_char(ch, "You can't seem to mine here.\r\n");
				cancel_action(ch);
				break;
			}
			
			// attempt to mine it
			success = run_interactions(ch, GET_GLOBAL_INTERACTIONS(glb), INTERACT_MINE, IN_ROOM(ch), NULL, NULL, finish_mining);
			
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
				stop_room_action(in_room, ACT_MINING, CHORE_MINING);
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
	ACMD(do_mint);

	empire_data *emp = ROOM_OWNER(IN_ROOM(ch));
	char tmp[MAX_INPUT_LENGTH];
	obj_data *proto;
	int num;
	
	if (!emp) {
		msg_to_char(ch, "The mint no longer belongs to any empire and can't be used to make coin.\r\n");
		cancel_action(ch);
		return;
	}
	if (!CAN_SEE_IN_DARK_ROOM(ch, IN_ROOM(ch))) {
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
		gain_ability_exp(ch, ABIL_BASIC_CRAFTS, 30);
		
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
	void finish_morphing(char_data *ch, morph_data *morph);

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
* Tick update for music action.
*
* @param char_data *ch The musician.
*/
void process_music(char_data *ch) {
	obj_data *obj;
	
	if (!(obj = GET_EQ(ch, WEAR_HOLD)) || GET_OBJ_TYPE(obj) != ITEM_INSTRUMENT) {
		msg_to_char(ch, "You need to hold an instrument to play music!\r\n");
		cancel_action(ch);
	}
	else {
		if (obj_has_custom_message(obj, OBJ_CUSTOM_INSTRUMENT_TO_CHAR)) {
			act(obj_get_custom_message(obj, OBJ_CUSTOM_INSTRUMENT_TO_CHAR), FALSE, ch, obj, 0, TO_CHAR | TO_SPAMMY);
		}
		
		if (obj_has_custom_message(obj, OBJ_CUSTOM_INSTRUMENT_TO_ROOM)) {
			act(obj_get_custom_message(obj, OBJ_CUSTOM_INSTRUMENT_TO_ROOM), FALSE, ch, obj, 0, TO_ROOM | TO_SPAMMY);
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

	if ((!GET_EQ(ch, WEAR_WIELD) || !OBJ_FLAGGED(GET_EQ(ch, WEAR_WIELD), OBJ_TOOL_PAN)) && (!GET_EQ(ch, WEAR_HOLD) || !OBJ_FLAGGED(GET_EQ(ch, WEAR_HOLD), OBJ_TOOL_PAN))) {
		msg_to_char(ch, "You need to be holding a pan to do that.\r\n");
		cancel_action(ch);
	}
	else if (!CAN_SEE_IN_DARK_ROOM(ch, IN_ROOM(ch))) {
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
				success = run_room_interactions(ch, room, INTERACT_PAN, finish_panning);
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
* Tick updates for picking.
*
* @param char_data *ch The picker.
*/
void process_picking(char_data *ch) {	
	bool found = FALSE;
	
	int garden_depletion = config_get_int("garden_depletion");
	int pick_depletion = config_get_int("pick_depletion");
	
	if (!IS_COMPLETE(IN_ROOM(ch))) {
		msg_to_char(ch, "You can't pick anything in an incomplete building.\r\n");
		cancel_action(ch);
		return;
	}
	if (!CAN_SEE_IN_DARK_ROOM(ch, IN_ROOM(ch))) {
		msg_to_char(ch, "It's too dark to pick anything.\r\n");
		cancel_action(ch);
		return;
	}
	
	if (!PRF_FLAGGED(ch, PRF_NOSPAM)) {
		send_to_char("You look around for something nice to pick...\r\n", ch);
	}
	// no room message

	// decrement
	GET_ACTION_TIMER(ch) -= 1;
		
	if (GET_ACTION_TIMER(ch) <= 0) {
		GET_ACTION(ch) = ACT_NONE;
		
		if (get_depletion(IN_ROOM(ch), DPLTN_PICK) >= (ROOM_SECT_FLAGGED(IN_ROOM(ch), SECTF_CROP) ? pick_depletion : (IS_ANY_BUILDING(IN_ROOM(ch)) ? garden_depletion : config_get_int("common_depletion")))) {
			msg_to_char(ch, "You can't find anything here left to pick.\r\n");
			act("$n stops looking for things to pick as $e comes up empty-handed.", TRUE, ch, NULL, NULL, TO_ROOM);
		}
		else {
			if (run_room_interactions(ch, IN_ROOM(ch), INTERACT_PICK, finish_picking)) {
				gain_ability_exp(ch, ABIL_SCAVENGING, 10);
				found = TRUE;
			}
			else if (can_interact_room(IN_ROOM(ch), INTERACT_HARVEST) && (IS_ADVENTURE_ROOM(IN_ROOM(ch)) || ROOM_CROP_FLAGGED(IN_ROOM(ch), CROPF_IS_ORCHARD))) {
				// only orchards allow pick -- and only run this if we hit no herbs at all
				if (run_room_interactions(ch, IN_ROOM(ch), INTERACT_HARVEST, finish_picking)) {
					gain_ability_exp(ch, ABIL_SCAVENGING, 10);
					found = TRUE;
				}
			}
			
			if (!found) {
				msg_to_char(ch, "You couldn't find anything to pick here.\r\n");
			}
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
		
		gain_ability_exp(ch, ABIL_COOK, 30);
		
		GET_ACTION(ch) = ACT_NONE;
	}
}


/**
* Tick update for prospect action.
*
* @param char_data *ch The prospector.
*/
void process_prospecting(char_data *ch) {
	void init_mine(room_data *room, char_data *ch, empire_data *emp);
		
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
* The ol' quarry ticker.
*
* @param char_data *ch The quarrior.
*/
void process_quarrying(char_data *ch) {
	room_data *in_room;
	
	if (!can_interact_room(IN_ROOM(ch), INTERACT_QUARRY) || !IS_COMPLETE(IN_ROOM(ch)) || get_depletion(IN_ROOM(ch), DPLTN_QUARRY) >= config_get_int("common_depletion")) {
		msg_to_char(ch, "You can't quarry anything here.\r\n");
		cancel_action(ch);
		return;
	}
	if (!CAN_SEE_IN_DARK_ROOM(ch, IN_ROOM(ch))) {
		msg_to_char(ch, "It's too dark to finish quarrying.\r\n");
		cancel_action(ch);
		return;
	}
	
	GET_ACTION_TIMER(ch) -= 1;
		
	if (GET_ACTION_TIMER(ch) > 0) {
		// tick messages
		switch (number(0, 9)) {
			case 0: {
				if (!PRF_FLAGGED(ch, PRF_NOSPAM)) {
					send_to_char("You slip some shims into cracks in the stone.\r\n", ch);
				}
				act("$n slips some shims into cracks in the stone.", FALSE, ch, 0, 0, TO_ROOM | TO_SPAMMY);
				break;
			}
			case 1: {
				if (!PRF_FLAGGED(ch, PRF_NOSPAM)) {
					send_to_char("You brush dust out of the cracks in the stone.\r\n", ch);
				}
				act("$n brushes dust out of the cracks in the stone.", FALSE, ch, 0, 0, TO_ROOM | TO_SPAMMY);
				break;
			}
			case 2: {
				if (!PRF_FLAGGED(ch, PRF_NOSPAM)) {
					send_to_char("You hammer a plug drill into the stone.\r\n", ch);
				}
				act("$n hammers a plug drill into the stone.", FALSE, ch, 0, 0, TO_ROOM | TO_SPAMMY);
				break;
			}
		}
	}
	else {	// done
		in_room = IN_ROOM(ch);
		GET_ACTION(ch) = ACT_NONE;
		
		if (run_room_interactions(ch, IN_ROOM(ch), INTERACT_QUARRY, finish_quarrying)) {
			gain_ability_exp(ch, ABIL_SCAVENGING, 25);
		
			add_depletion(IN_ROOM(ch), DPLTN_QUARRY, TRUE);
			
			// character is still there?
			if (IN_ROOM(ch) == in_room && GET_ACTION(ch) == ACT_NONE) {
				start_quarrying(ch);
			}
		}
		else {
			msg_to_char(ch, "You don't seem to find anything of use.\r\n");
		}
	}
}


/**
* Tick update for repairing action.
*
* @param char_data *ch The character doing the repairing.
*/
void process_repairing(char_data *ch) {
	extern vehicle_data *find_vehicle(int n);

	obj_data *found_obj = NULL;
	struct resource_data *res;
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
	if (!CAN_SEE_IN_DARK_ROOM(ch, IN_ROOM(ch))) {
		msg_to_char(ch, "It's too dark to repair anything.\r\n");
		cancel_action(ch);
		return;
	}
	
	// good to repair:
	if ((res = get_next_resource(ch, VEH_NEEDS_RESOURCES(veh), can_use_room(ch, IN_ROOM(ch), MEMBERS_ONLY), FALSE, &found_obj))) {
		// take the item; possibly free the res
		apply_resource(ch, res, &VEH_NEEDS_RESOURCES(veh), found_obj, APPLY_RES_REPAIR, veh, NULL);
		found = TRUE;
	}
	
	// done?
	if (!VEH_NEEDS_RESOURCES(veh)) {
		GET_ACTION(ch) = ACT_NONE;
		REMOVE_BIT(VEH_FLAGS(veh), VEH_INCOMPLETE);
		VEH_HEALTH(veh) = VEH_MAX_HEALTH(veh);
		act("$V is fully repaired!", FALSE, ch, NULL, veh, TO_CHAR | TO_ROOM);
	}
	else if (!found) {
		GET_ACTION(ch) = ACT_NONE;
		msg_to_char(ch, "You run out of resources and stop repairing.\r\n");
		act("$n runs out of resources and stops.", FALSE, ch, NULL, NULL, TO_ROOM);
	}
}


/**
* Tick update for scrape action.
*
* @param char_data *ch The scraper.
*/
void process_scraping(char_data *ch) {
	ACMD(do_scrape);
	
	char buf[MAX_STRING_LENGTH];
	bool success = FALSE;
	obj_data *proto;
	
	if (!has_sharp_tool(ch)) {
		msg_to_char(ch, "You need to be using a sharp tool to scrape it.\r\n");
		cancel_action(ch);
		return;
	}
	if (!CAN_SEE_IN_DARK_ROOM(ch, IN_ROOM(ch))) {
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
			
			success = run_interactions(ch, proto->interactions, INTERACT_SCRAPE, IN_ROOM(ch), NULL, proto, finish_scraping);
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
			gain_ability_exp(ch, ABIL_PRIMITIVE_CRAFTS, 10);
			
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
	void sire_char(char_data *ch, char_data *victim);
	
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
		change_terrain(IN_ROOM(ch), GET_SECT_VNUM(old_sect));
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
	void perform_swap_skill_sets(char_data *ch);
	
	GET_ACTION_TIMER(ch) -= 1;
	
	if (GET_ACTION_TIMER(ch) <= 0) {
		msg_to_char(ch, "You swap skill sets.\r\n");
		act("$n swaps skill sets.", TRUE, ch, NULL, NULL, TO_ROOM);
		
		perform_swap_skill_sets(ch);
		GET_ACTION(ch) = ACT_NONE;
		
		GET_MOVE(ch) = MIN(GET_MOVE(ch), GET_MAX_MOVE(ch)/4);
		GET_MANA(ch) = MIN(GET_MANA(ch), GET_MAX_MANA(ch)/4);
	}
}


/**
* Ticker for tanners.
*
* @param char_data *ch The tanner.
*/
void process_tanning(char_data *ch) {
	ACMD(do_tan);
	
	obj_data *proto;
	bool success;
	
	if (!CAN_SEE_IN_DARK_ROOM(ch, IN_ROOM(ch))) {
		msg_to_char(ch, "It's too dark to finish tanning.\r\n");
		cancel_action(ch);
		return;
	}
	
	GET_ACTION_TIMER(ch) -= (room_has_function_and_city_ok(IN_ROOM(ch), FNC_TANNERY) ? 4 : 1);
	
	// need the prototype
	if (!(proto = obj_proto(GET_ACTION_VNUM(ch, 0)))) {
		cancel_action(ch);
		return;
	}
	
	if (GET_ACTION_TIMER(ch) <= 0) {
		act("You finish tanning $p.", FALSE, ch, proto, NULL, TO_CHAR);
		act("$n finishes tanning $p.", TRUE, ch, proto, NULL, TO_ROOM);

		GET_ACTION(ch) = ACT_NONE;
		
		success = run_interactions(ch, proto->interactions, INTERACT_TAN, IN_ROOM(ch), NULL, proto, finish_scraping);
		free_resource_list(GET_ACTION_RESOURCES(ch));
		GET_ACTION_RESOURCES(ch) = NULL;
		
		if (success) {
			gain_ability_exp(ch, ABIL_BASIC_CRAFTS, 20);
	
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
	else if (!room_has_function_and_city_ok(IN_ROOM(ch), FNC_BATHS) && !ROOM_SECT_FLAGGED(IN_ROOM(ch), SECTF_FRESH_WATER | SECTF_SHALLOW_WATER)) {
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
	int chip_timer = config_get_int("chip_timer");

	one_argument(argument, arg);

	if (IS_NPC(ch)) {
		msg_to_char(ch, "NPCs cannot chip.\r\n");
	}
	else if (GET_ACTION(ch) == ACT_CHIPPING) {
		msg_to_char(ch, "You stop chipping it.\r\n");
		act("$n stops chipping.", TRUE, ch, NULL, NULL, TO_ROOM);
		cancel_action(ch);
	}
	else if (!IS_APPROVED(ch) && config_get_bool("craft_approval")) {
		send_config_msg(ch, "need_approval_string");
	}
	else if (GET_ACTION(ch) != ACT_NONE) {
		msg_to_char(ch, "You're already busy doing something else.\r\n");
	}
	else if (!CAN_SEE_IN_DARK_ROOM(ch, IN_ROOM(ch))) {
		msg_to_char(ch, "It's too dark to chip anything here.\r\n");
	}
	else if (!*arg) {
		msg_to_char(ch, "Chip what?\r\n");
	}
	else if (!(target = get_obj_in_list_vis(ch, arg, ch->carrying)) && (!can_use_room(ch, IN_ROOM(ch), MEMBERS_ONLY) || !(target = get_obj_in_list_vis(ch, arg, ROOM_CONTENTS(IN_ROOM(ch)))))) {
		msg_to_char(ch, "You don't seem to have %s %s.\r\n", AN(arg), arg);
	}
	else if (!has_interaction(target->interactions, INTERACT_CHIP)) {
		msg_to_char(ch, "You can't chip that!\r\n");
	}
	else if (!find_chip_weapon(ch)) {
		msg_to_char(ch, "You need to be wielding some kind of hammer or rock to chip it.\r\n");
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
	else if (!IS_APPROVED(ch) && config_get_bool("gather_approval")) {
		send_config_msg(ch, "need_approval_string");
	}
	else if (GET_ACTION(ch) != ACT_NONE) {
		send_to_char("You're already busy.\r\n", ch);
	}
	else if (!CAN_CHOP_ROOM(IN_ROOM(ch))) {
		send_to_char("You can't really chop anything down here.\r\n", ch);
	}
	else if (ROOM_AFF_FLAGGED(IN_ROOM(ch), ROOM_AFF_HAS_INSTANCE)) {
		msg_to_char(ch, "You can't chop here right now.\r\n");
	}
	else if (!ROOM_AFF_FLAGGED(IN_ROOM(ch), ROOM_AFF_UNCLAIMABLE) && !can_use_room(ch, IN_ROOM(ch), MEMBERS_ONLY)) {
		msg_to_char(ch, "You don't have permission to chop down trees here.\r\n");
	}
	else if (!ROOM_AFF_FLAGGED(IN_ROOM(ch), ROOM_AFF_UNCLAIMABLE) && !has_permission(ch, PRIV_CHOP, IN_ROOM(ch))) {
		msg_to_char(ch, "You don't have permission to chop down trees in the empire.\r\n");
	}
	else if (!GET_EQ(ch, WEAR_WIELD) || GET_WEAPON_TYPE(GET_EQ(ch, WEAR_WIELD)) != TYPE_SLICE) {
		send_to_char("You need to be wielding some kind of axe to chop.\r\n", ch);
	}
	else if (!CAN_SEE_IN_DARK_ROOM(ch, IN_ROOM(ch))) {
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
	extern bool is_entrance(room_data *room);
	
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
	else if (!has_shovel(ch)) {
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
		change_terrain(IN_ROOM(ch), evo->becomes);
		
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
	else if (!has_shovel(ch)) {
		msg_to_char(ch, "You need a shovel to do that.\r\n");
	}
	else if (ROOM_SECT_FLAGGED(IN_ROOM(ch), SECTF_IS_TRENCH)) {
		// already a trench -- just fill in
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
	}
	else if (SECT(IN_ROOM(ch)) == world_map[FLAT_X_COORD(IN_ROOM(ch))][FLAT_Y_COORD(IN_ROOM(ch))].natural_sector) {
		msg_to_char(ch, "You can only fill in a tile that was made by excavation, not a natural one.\r\n");
	}
	else if (!can_use_room(ch, IN_ROOM(ch), MEMBERS_ONLY)) {
		// 2nd check: members only to start new fillin
		msg_to_char(ch, "You don't have permission to fill in here!\r\n");
	}
	else {
		// always takes 1 minute
		start_action(ch, ACT_START_FILLIN, SECS_PER_REAL_MIN / ACTION_CYCLE_TIME);
		msg_to_char(ch, "You begin preparing to fill in the trench.\r\n");
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
		msg_to_char(ch, "You stop harvesting the %s.\r\n", GET_CROP_NAME(ROOM_CROP(IN_ROOM(ch))));
		act("$n stops harvesting.\r\n", FALSE, ch, 0, 0, TO_ROOM);
		cancel_action(ch);
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
	else if (!ROOM_SECT_FLAGGED(IN_ROOM(ch), SECTF_CROP)) {
		msg_to_char(ch, "You can't harvest anything here!\r\n");
	}
	else if (ROOM_CROP_FLAGGED(IN_ROOM(ch), CROPF_IS_ORCHARD)) {
		msg_to_char(ch, "You can't harvest the orchard. Use pick or chop.\r\n");
	}
	else if (!ROOM_AFF_FLAGGED(IN_ROOM(ch), ROOM_AFF_UNCLAIMABLE) && !can_use_room(ch, IN_ROOM(ch), MEMBERS_ONLY)) {
		msg_to_char(ch, "You don't have permission to harvest this crop.\r\n");
	}
	else if (!ROOM_AFF_FLAGGED(IN_ROOM(ch), ROOM_AFF_UNCLAIMABLE) && !has_permission(ch, PRIV_HARVEST, IN_ROOM(ch))) {
		msg_to_char(ch, "You don't have permission to harvest empire crops.\r\n");
	}
	else if (!GET_EQ(ch, WEAR_WIELD) || GET_OBJ_TYPE(GET_EQ(ch, WEAR_WIELD)) != ITEM_WEAPON || (GET_WEAPON_TYPE(GET_EQ(ch, WEAR_WIELD)) != TYPE_SLICE && GET_WEAPON_TYPE(GET_EQ(ch, WEAR_WIELD)) != TYPE_SLASH)) {
		msg_to_char(ch, "You aren't using the proper tool for that.\r\n");
	}
	else if (!CAN_SEE_IN_DARK_ROOM(ch, IN_ROOM(ch))) {
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
	else if (!room_has_function_and_city_ok(IN_ROOM(ch), FNC_MINE)) {
		msg_to_char(ch, "This isn't a mine.\r\n");
	}
	else if (!check_in_city_requirement(IN_ROOM(ch), TRUE)) {
		msg_to_char(ch, "This mine only works in a city.\r\n");
	}
	else if (!check_in_city_requirement(IN_ROOM(ch), TRUE)) {
		msg_to_char(ch, "You can't mine here because it's not in a city.\r\n");
	}
	else if (!IS_COMPLETE(IN_ROOM(ch))) {
		msg_to_char(ch, "The mine shafts aren't finished yet.\r\n");
	}
	else if (!can_use_room(ch, IN_ROOM(ch), GUESTS_ALLOWED)) {
		msg_to_char(ch, "You don't have permission to mine here.\r\n");
	}
	else if (get_room_extra_data(IN_ROOM(ch), ROOM_EXTRA_MINE_AMOUNT) <= 0) {
		msg_to_char(ch, "The mine is depleted, you find nothing of use.\r\n");
	}
	else if (!GET_EQ(ch, WEAR_WIELD) || ((GET_OBJ_TYPE(GET_EQ(ch, WEAR_WIELD)) != ITEM_WEAPON || GET_WEAPON_TYPE(GET_EQ(ch, WEAR_WIELD)) != TYPE_PICK) && GET_OBJ_VNUM(GET_EQ(ch, WEAR_WIELD)) != o_HANDAXE)) {
		msg_to_char(ch, "You aren't wielding a tool suitable for mining.\r\n");
	}
	else if (!CAN_SEE_IN_DARK_ROOM(ch, IN_ROOM(ch))) {
		msg_to_char(ch, "It's too dark to mine anything here.\r\n");
	}
	else {
		start_mining(ch);
	}
}


ACMD(do_mint) {
	empire_data *emp;
	obj_data *obj;
	
	one_argument(argument, arg);
	
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
	else if (!has_ability(ch, ABIL_BASIC_CRAFTS)) {
		msg_to_char(ch, "You need the Basic Crafts ability to mint anything.\r\n");
	}
	else if (GET_ACTION(ch) != ACT_NONE) {
		msg_to_char(ch, "You're busy doing something else right now.\r\n");
	}
	else if (!room_has_function_and_city_ok(IN_ROOM(ch), FNC_MINT)) {
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
	else if (!*arg) {
		msg_to_char(ch, "Mint which item into coins?\r\n");
	}
	else if (!(obj = get_obj_in_list_vis(ch, arg, ch->carrying)) && !(obj = get_obj_in_list_vis(ch, arg, ROOM_CONTENTS(IN_ROOM(ch))))) {
		msg_to_char(ch, "You don't seem to have %s %s.\r\n", AN(arg), arg);
	}
	else if (!IS_WEALTH_ITEM(obj) || GET_WEALTH_VALUE(obj) <= 0) {
		msg_to_char(ch, "You can't mint that into coins.\r\n");
	}
	else if (!CAN_SEE_IN_DARK_ROOM(ch, IN_ROOM(ch))) {
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
	else if ((!GET_EQ(ch, WEAR_WIELD) || !OBJ_FLAGGED(GET_EQ(ch, WEAR_WIELD), OBJ_TOOL_PAN)) && (!GET_EQ(ch, WEAR_HOLD) || !OBJ_FLAGGED(GET_EQ(ch, WEAR_HOLD), OBJ_TOOL_PAN))) {
		msg_to_char(ch, "You need to be holding a pan to do that.\r\n");
	}
	else if (!CAN_SEE_IN_DARK_ROOM(ch, IN_ROOM(ch))) {
		msg_to_char(ch, "It's too dark to pan for anything here.\r\n");
	}
	else {
		start_panning(ch, dir);
	}
}


ACMD(do_pick) {
	if (IS_NPC(ch)) {
		msg_to_char(ch, "NPCs cannot pick.\r\n");
	}
	else if (!IS_APPROVED(ch) && config_get_bool("gather_approval")) {
		send_config_msg(ch, "need_approval_string");
	}
	else if (GET_ACTION(ch) == ACT_PICKING) {
		send_to_char("You stop searching.\r\n", ch);
		act("$n stops looking around.", TRUE, ch, 0, 0, TO_ROOM);
		cancel_action(ch);
	}
	else if (GET_ACTION(ch) != ACT_NONE) {
		send_to_char("You're already busy.\r\n", ch);
	}
	else if (!IS_OUTDOORS(ch)) {
		send_to_char("You can only pick things outdoors!\r\n", ch);
	}
	else if (!can_use_room(ch, IN_ROOM(ch), GUESTS_ALLOWED)) {
		msg_to_char(ch, "You don't have permission to pick anything here.\r\n");
	}
	else if (!IS_COMPLETE(IN_ROOM(ch))) {
		msg_to_char(ch, "You can't pick anything in an incomplete building.\r\n");
	}
	else if (!CAN_SEE_IN_DARK_ROOM(ch, IN_ROOM(ch))) {
		msg_to_char(ch, "It's too dark to pick anything here.\r\n");
	}
	else {
		start_picking(ch);
	}
}


ACMD(do_plant) {
	extern const char *climate_types[];
	
	struct evolution_data *evo;
	sector_data *original;
	obj_data *obj;
	crop_data *cp;

	one_argument(argument, arg);

	if (IS_NPC(ch)) {
		msg_to_char(ch, "You can't plant.\r\n");
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
		msg_to_char(ch, "What do you want to plant?\r\n");
	}
	else if (!(obj = get_obj_in_list_vis(ch, arg, ch->carrying))) {
		msg_to_char(ch, "You don't seem to have any %s.\r\n", arg);
	}
	else if (!OBJ_FLAGGED(obj, OBJ_PLANTABLE)) {
		msg_to_char(ch, "You can't plant that!%s\r\n", has_interaction(obj->interactions, INTERACT_SEED) ? " Try seeding it first." : "");
	}
	else if (!(cp = crop_proto(GET_OBJ_VAL(obj, VAL_FOOD_CROP_TYPE)))) {
		// this is a sanity check for bad crop values
		msg_to_char(ch, "You can't plant that!\r\n");
	}
	else if (CROP_FLAGGED(cp, CROPF_REQUIRES_WATER) && !find_flagged_sect_within_distance_from_char(ch, SECTF_FRESH_WATER, NOBITS, config_get_int("water_crop_distance"))) {
		msg_to_char(ch, "You must plant that closer to fresh water.\r\n");
	}
	else if (!(evo = get_evolution_by_type(SECT(IN_ROOM(ch)), EVO_PLANTS_TO))) {
		msg_to_char(ch, "Nothing can be planted here.\r\n");
	}
	else if (GET_SECT_CLIMATE(SECT(IN_ROOM(ch))) != GET_CROP_CLIMATE(cp)) {
		strcpy(buf, climate_types[GET_CROP_CLIMATE(cp)]);
		msg_to_char(ch, "You can only plant that in %s areas.\r\n", strtolower(buf));
	}
	else {
		original = SECT(IN_ROOM(ch));
		change_terrain(IN_ROOM(ch), evo->becomes);
		change_base_sector(IN_ROOM(ch), original);
		
		// don't use GET_FOOD_CROP_TYPE because not all plantables are food
		set_crop_type(IN_ROOM(ch), cp);
		
		set_room_extra_data(IN_ROOM(ch), ROOM_EXTRA_SEED_TIME, time(0) + config_get_int("planting_base_timer"));
		if (GET_MAP_LOC(IN_ROOM(ch))) {
			schedule_crop_growth(GET_MAP_LOC(IN_ROOM(ch)));
		}
		
		// temporarily deplete seeded rooms
		set_depletion(IN_ROOM(ch), DPLTN_FORAGE, config_get_int("short_depletion"));
		set_depletion(IN_ROOM(ch), DPLTN_PICK, config_get_int("pick_depletion"));
		
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


ACMD(do_quarry) {
	if (IS_NPC(ch)) {
		msg_to_char(ch, "NPCs cannot quarry.\r\n");
	}
	else if (GET_ACTION(ch) == ACT_QUARRYING) {
		send_to_char("You stop quarrying.\r\n", ch);
		act("$n stops quarrying.", FALSE, ch, 0, 0, TO_ROOM);
		cancel_action(ch);
	}
	else if (!IS_APPROVED(ch) && config_get_bool("gather_approval")) {
		send_config_msg(ch, "need_approval_string");
	}
	else if (GET_ACTION(ch) != ACT_NONE) {
		send_to_char("You're already busy.\r\n", ch);
	}
	else if (!can_interact_room(IN_ROOM(ch), INTERACT_QUARRY)) {
		send_to_char("You can't quarry here.\r\n", ch);
	}
	else if (!IS_COMPLETE(IN_ROOM(ch))) {
		msg_to_char(ch, "You can't quarry until it's finished!\r\n");
	}
	else if (!can_use_room(ch, IN_ROOM(ch), GUESTS_ALLOWED)) {
		msg_to_char(ch, "You don't have permission to quarry here.\r\n");
	}
	else if (!CAN_SEE_IN_DARK_ROOM(ch, IN_ROOM(ch))) {
		msg_to_char(ch, "It's too dark to quarry anything here.\r\n");
	}
	else {
		start_quarrying(ch);
	}
}


ACMD(do_saw) {
	obj_data *obj;

	one_argument(argument, arg);

	if (IS_NPC(ch)) {
		msg_to_char(ch, "NPCs cannot saw.\r\n");
	}
	else if (GET_ACTION(ch) == ACT_SAWING) {
		act("You stop sawing.", FALSE, ch, NULL, NULL, TO_CHAR);
		cancel_action(ch);
	}
	else if (!IS_APPROVED(ch) && config_get_bool("craft_approval")) {
		send_config_msg(ch, "need_approval_string");
	}
	else if (!room_has_function_and_city_ok(IN_ROOM(ch), FNC_SAW)) {
		msg_to_char(ch, "You can only saw in a lumber yard.\r\n");
	}
	else if (!check_in_city_requirement(IN_ROOM(ch), TRUE)) {
		msg_to_char(ch, "You can only saw in this building if it's in a city.\r\n");
	}
	else if (!IS_COMPLETE(IN_ROOM(ch))) {
		msg_to_char(ch, "Complete the building first.\r\n");
	}
	else if (!check_in_city_requirement(IN_ROOM(ch), TRUE)) {
		msg_to_char(ch, "This building must be in a city to use it.\r\n");
	}
	else if (!can_use_room(ch, IN_ROOM(ch), GUESTS_ALLOWED)) {
		msg_to_char(ch, "You don't have permission to saw here.\r\n");
	}
	else if (GET_ACTION(ch) != ACT_NONE) {
		msg_to_char(ch, "You're already busy doing something else.\r\n");
	}
	else if (!*arg) {
		msg_to_char(ch, "Saw what?\r\n");
	}
	else if (!(obj = get_obj_in_list_vis(ch, arg, ch->carrying)) && !(obj = get_obj_in_list_vis(ch, arg, ROOM_CONTENTS(IN_ROOM(ch))))) {
		msg_to_char(ch, "You don't seem to have %s %s.\r\n", AN(arg), arg);
	}
	else if (!has_interaction(obj->interactions, INTERACT_SAW)) {
		msg_to_char(ch, "You can't saw that!\r\n");
	}
	else if (!CAN_SEE_IN_DARK_ROOM(ch, IN_ROOM(ch))) {
		msg_to_char(ch, "It's too dark to saw anything here.\r\n");
	}
	else {
		start_action(ch, ACT_SAWING, 8);
		
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
	obj_data *obj;
	
	one_argument(argument, arg);
	
	if (IS_NPC(ch)) {
		msg_to_char(ch, "NPCs cannot scrape.\r\n");
	}
	else if (GET_ACTION(ch) == ACT_SCRAPING) {
		act("You stop scraping.", FALSE, ch, NULL, NULL, TO_CHAR);
		cancel_action(ch);
	}
	else if (!IS_APPROVED(ch) && config_get_bool("craft_approval")) {
		send_config_msg(ch, "need_approval_string");
	}
	else if (GET_ACTION(ch) != ACT_NONE) {
		msg_to_char(ch, "You're already busy doing something else.\r\n");
	}
	else if (!*arg) {
		msg_to_char(ch, "Scrape what?\r\n");
	}
	else if (!(obj = get_obj_in_list_vis(ch, arg, ch->carrying)) && (!can_use_room(ch, IN_ROOM(ch), GUESTS_ALLOWED) || !(obj = get_obj_in_list_vis(ch, arg, ROOM_CONTENTS(IN_ROOM(ch)))))) {
		msg_to_char(ch, "You don't seem to have %s %s.\r\n", AN(arg), arg);
	}
	else if (!has_interaction(obj->interactions, INTERACT_SCRAPE)) {
		msg_to_char(ch, "You can't scrape that!\r\n");
	}
	else if (!has_sharp_tool(ch)) {
		msg_to_char(ch, "You need to be using a sharp tool to scrape anything.\r\n");
	}
	else if (!CAN_SEE_IN_DARK_ROOM(ch, IN_ROOM(ch))) {
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
	void cancel_action(char_data *ch);
	extern bool cancel_biting(char_data *ch);
	
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
	obj_data *obj;
	
	int tan_timer = config_get_int("tan_timer");

	one_argument(argument, arg);

	if (IS_NPC(ch)) {
		msg_to_char(ch, "You can't tan.\r\n");
	}
	else if (!IS_APPROVED(ch) && config_get_bool("craft_approval")) {
		send_config_msg(ch, "need_approval_string");
	}
	else if (!has_ability(ch, ABIL_BASIC_CRAFTS)) {
		msg_to_char(ch, "You need the Basic Crafts ability to tan anything.\r\n");
	}
	else if (!*arg) {
		msg_to_char(ch, "What would you like to tan?\r\n");
	}
	else if (GET_ACTION(ch) != ACT_NONE) {
		msg_to_char(ch, "You're busy right now.\r\n");
	}
	else if (!(obj = get_obj_in_list_vis(ch, arg, ch->carrying)) && !(obj = get_obj_in_list_vis(ch, arg, ROOM_CONTENTS(IN_ROOM(ch))))) {
		msg_to_char(ch, "You don't seem to have more to tan.\r\n");
	}
	else if (!has_interaction(obj->interactions, INTERACT_TAN)) {
		msg_to_char(ch, "You can't tan that!\r\n");
	}
	else if (!can_use_room(ch, IN_ROOM(ch), GUESTS_ALLOWED)) {
		msg_to_char(ch, "You don't have permission to tan here.\r\n");
	}
	else if (!CAN_SEE_IN_DARK_ROOM(ch, IN_ROOM(ch))) {
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
