/* ************************************************************************
*   File: act.action.c                                    EmpireMUD 2.0b1 *
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
*   Action Cancelers
*   Action Starters
*   Action Finishers
*   Action Processes
*   Action Commands
*/

// external vars
extern const struct smelt_data_type smelt_data[];
extern const struct tanning_data_type tan_data[];

// external funcs
extern double get_base_dps(obj_data *weapon);
extern obj_data *find_chip_weapon(char_data *ch);
extern obj_data *has_sharp_tool(char_data *ch);
void scale_item_to_level(obj_data *obj, int level);

// cancel protos
void cancel_chipping(char_data *ch);
void cancel_gen_craft(char_data *ch);
void cancel_minting(char_data *ch);
void cancel_sawing(char_data *ch);
void cancel_scraping(char_data *ch);
void cancel_siring(char_data *ch);
void cancel_smelting(char_data *ch);
void cancel_tanning(char_data *ch);
void cancel_weaving(char_data *ch);

// process protos
void perform_chant(char_data *ch);
void process_chipping(char_data *ch);
void perform_ritual(char_data *ch);
void perform_saw(char_data *ch);
void perform_study(char_data *ch);
void process_bathing(char_data *ch);
void process_build_action(char_data *ch);
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
void process_manufacturing(char_data *ch);
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
void process_scraping(char_data *ch);
void process_siring(char_data *ch);
void process_smelting(char_data *ch);
void process_tanning(char_data *ch);
void process_weaving(char_data *ch);


 //////////////////////////////////////////////////////////////////////////////
//// CONTROL DATA ////////////////////////////////////////////////////////////

// ACT_x
const struct action_data_struct action_data[] = {
	{ "", "", NULL, NULL },	// ACT_NONE
	{ "digging", "is digging at the ground.", process_digging, NULL },	// ACT_DIGGING
	{ "gathering", "is gathering sticks.", process_gathering, NULL },	// ACT_GATHERING
	{ "chopping", "is chopping down trees.", process_chop, NULL },	// ACT_CHOPPING
	{ "building", "is hard at work building.", process_build_action, NULL },	// ACT_BUILDING
	{ "dismantling", "is dismantling the building.", process_dismantle_action, NULL },	// ACT_DISMANTLING
	{ "harvesting", "is harvesting the crop.", process_harvesting, NULL },	// ACT_HARVESTING
	{ "planting", "is planting seeds.", process_planting, NULL },	// ACT_PLANTING
	{ "mining", "is mining at the walls.", process_mining, NULL },	// ACT_MINING
	{ "minting", "is minting coins.", process_minting, cancel_minting },	// ACT_MINTING
	{ "fishing", "is fishing.", process_fishing, NULL },	// ACT_FISHING
	{ "smelting", "is melting down ore.", process_smelting, cancel_smelting },	// ACT_MELTING
	{ "manufacturing", "is working on the ship.", process_manufacturing, NULL },	// ACT_MANUFACTURING
	{ "chipping", "is chipping rocks.", process_chipping, cancel_chipping },	// ACT_CHIPPING
	{ "panning", "is panning for gold.", process_panning, NULL },	// ACT_PANNING
	{ "music", "is playing soothing music.", process_music, NULL },	// ACT_MUSIC
	{ "excavating", "is excavating a trench.", process_excavating, NULL },	// ACT_EXCAVATING
	{ "siring", "is hunched over.", process_siring, cancel_siring },	// ACT_SIRING
	{ "picking", "is looking around at the ground.", process_picking, NULL },	// ACT_PICKING
	{ "morphing", "is morphing and changing shape!", process_morphing, NULL },	// ACT_MORPHING
	{ "scraping", "is scraping at a tree.", process_scraping, cancel_scraping },	// ACT_SCRAPING
	{ "bathing", "is bathing in the water.", process_bathing, NULL },	// ACT_BATHING
	{ "chanting", "is chanting a strange song.", perform_chant, NULL },	// ACT_CHANTING
	{ "prospecting", "is prospecting.", process_prospecting, NULL },	// ACT_PROSPECTING
	{ "filling", "is filling in the trench.", process_fillin, NULL },	// ACT_FILLING_IN
	{ "reclaiming", "is reclaiming this acre!", process_reclaim, NULL },	// ACT_RECLAIMING
	{ "escaping", "is running toward the window!", process_escaping, NULL },	// ACT_ESCAPING
	{ "studying", "is reading a book.", perform_study, NULL },	// ACT_STUDYING
	{ "ritual", "is performing an arcane ritual.", perform_ritual, NULL },	// ACT_RITUAL
	{ "sawing", "is sawing lumber.", perform_saw, cancel_sawing },	// ACT_SAWING
	{ "quarrying", "is quarrying stone.", process_quarrying, NULL },	// ACT_QUARRYING
	{ "weaving", "is weaving some cloth.", process_weaving, cancel_weaving },	// ACT_WEAVING
	{ "tanning", "is tanning leather.", process_tanning, cancel_tanning },	// ACT_TANNING
	{ "reading", "is reading a book.", process_reading, NULL },	// ACT_READING
	{ "copying", "is writing out a copy of a book.", process_copying_book, NULL },	// ACT_COPYING_BOOK
	{ "crafting", "is working on something.", process_gen_craft, cancel_gen_craft },	// ACT_GEN_CRAFT

	{ "\n", "\n", NULL, NULL }
};


// local vars
int last_action_rotation = 0;	// each player is on a different 1-second action rotation


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
* @param int type ACT_x const
* @param int timer Countdown in action ticks
* @param bitvector_t flags ACT_x flags (ACT_ANYWHERE)
*/
void start_action(char_data *ch, int type, int timer, bitvector_t flags) {
	// safety first
	if (GET_ACTION(ch) != ACT_NONE) {
		cancel_action(ch);
	}

	GET_ACTION(ch) = type;
	GET_ACTION_TIMER(ch) = timer;
	GET_ACTION_ROTATION(ch) = last_action_rotation;
	GET_ACTION_VNUM(ch, 0) = 0;
	GET_ACTION_VNUM(ch, 1) = 0;
	GET_ACTION_VNUM(ch, 2) = 0;
	
	if (IS_SET(flags, ACT_ANYWHERE)) {
		GET_ACTION_ROOM(ch) = NOWHERE;
	}
	else {
		GET_ACTION_ROOM(ch) = GET_ROOM_VNUM(IN_ROOM(ch));
	}
}


/**
* Cancels actions for anyone in the room on that action.
*
* @param room_data *room Where.
* @param int action ACTION_x or NOTHING to not stop actions.
* @param int chore CHORE_x or NOTHING to not stop workforce.
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
* This is the main processor for periodic actions (ACT_x).
*/
void update_actions(void) {
	descriptor_data *desc;
	char_data *ch;

	// action rotations keep players from all being on the same tick
	if (++last_action_rotation > 4) {
		last_action_rotation = 0;
	}

	// only players with active connections can process actions
	for (desc = descriptor_list; desc; desc = desc->next) {
		ch = desc->character;
		
		// basic qualifiers
		if (STATE(desc) == CON_PLAYING && ch && !IS_NPC(ch) && GET_ACTION(ch) != ACT_NONE && GET_ACTION_ROTATION(ch) == last_action_rotation) {
			// things which terminate actions
			if (GET_ACTION_ROOM(ch) != NOWHERE && GET_ROOM_VNUM(IN_ROOM(ch)) != GET_ACTION_ROOM(ch)) {
				cancel_action(ch);
			}
			else if (FIGHTING(ch) || GET_POS(ch) < POS_STANDING || IS_WRITING(ch)) {
				cancel_action(ch);
			}
			else if ((GET_FEEDING_FROM(ch) || GET_FED_ON_BY(ch)) && GET_ACTION(ch) != ACT_SIRING) {
				cancel_action(ch);
			}
			else {
				// end hide at this point, as if they had typed a command each tick
				REMOVE_BIT(AFF_FLAGS(ch), AFF_HIDE);
				
				if (action_data[GET_ACTION(ch)].process_function != NULL) {
					// call the process function
					(action_data[GET_ACTION(ch)].process_function)(ch);
				}
			}
		}
	}
}


 //////////////////////////////////////////////////////////////////////////////
//// ACTION CANCELERS ////////////////////////////////////////////////////////

/**
* Returns a rock (or chipped rock) to the person who was chipping.
*
* @param char_data *ch The chipper chap
*/
void cancel_chipping(char_data *ch) {
	obj_data *obj = read_object(GET_ACTION_VNUM(ch, 0));
	obj_to_char_or_room(obj, ch);
	load_otrigger(obj);
}


/**
* Returns the item to the minter.
*
* @param char_data *ch The minting man.
*/
void cancel_minting(char_data *ch) {
	obj_data *obj = read_object(GET_ACTION_VNUM(ch, 0));
	obj_to_char_or_room(obj, ch);
	load_otrigger(obj);
}


/**
* Returns a log/tree to the person who was sawing.
*
* @param char_data *ch The sawyer
*/
void cancel_sawing(char_data *ch) {
	obj_data *obj = read_object(GET_ACTION_VNUM(ch, 0));
	obj_to_char_or_room(obj, ch);
	load_otrigger(obj);
}


/**
* Returns a tree to the person who was scraping.
*
* @param char_data *ch The scraper
*/
void cancel_scraping(char_data *ch) {
	obj_data *obj = read_object(o_TREE);
	obj_to_char_or_room(obj, ch);
	load_otrigger(obj);
}


/**
* Return smelting resources on cancel.
*
* @param char_data *ch He who smelt it.
*/
void cancel_smelting(char_data *ch) {
	obj_data *obj;
	int iter, type = NOTHING;

	// Find the entry in the table
	for (iter = 0; smelt_data[iter].from != NOTHING && type == NOTHING; ++iter) {
		if (smelt_data[iter].from == GET_ACTION_VNUM(ch, 0)) {
			type = iter;
		}
	}
	
	if (type != NOTHING) {
		for (iter = 0; iter < smelt_data[type].from_amt; ++iter) {
			obj = read_object(smelt_data[type].from);
			obj_to_char_or_room(obj, ch);
			load_otrigger(obj);
		}
	}
}


/**
* Return tanning materials.
*
* @param char_data *ch Mr. Tanner
*/
void cancel_tanning(char_data *ch) {
	obj_data *obj = read_object(GET_ACTION_VNUM(ch, 0));
	obj_to_char_or_room(obj, ch);
	load_otrigger(obj);
}


 //////////////////////////////////////////////////////////////////////////////
//// ACTION STARTERS /////////////////////////////////////////////////////////

/**
* initiates chop and sends a start message or error message
*
* @param char_data *ch The player to start chopping.
*/
void start_chopping(char_data *ch) {
	int chop_timer = config_get_int("chop_timer");
	
	if (CAN_CHOP_ROOM(IN_ROOM(ch))) {
		start_action(ch, ACT_CHOPPING, 0, 0);

		// ensure progress data is set up
		if (get_room_extra_data(IN_ROOM(ch), ROOM_EXTRA_CHOP_PROGRESS) <= 0) {
			set_room_extra_data(IN_ROOM(ch), ROOM_EXTRA_CHOP_PROGRESS, chop_timer);
		}
		
		send_to_char("You swing back your axe and prepare to chop...\r\n", ch);
		act("$n swings $s axe over $s shoulder.", TRUE, ch, 0, 0, TO_ROOM);
	}
	else {
		msg_to_char(ch, "There's nothing left here to chop.\r\n");
	}
}


/**
* begins the digging action if able
*
* @param char_data *ch The player to start digging.
*/
static void start_digging(char_data *ch) {
	int dig_base_timer = config_get_int("dig_base_timer");
	
	if (CAN_INTERACT_ROOM(IN_ROOM(ch), INTERACT_DIG)) {
		start_action(ch, ACT_DIGGING, dig_base_timer / (HAS_ABILITY(ch, ABIL_FINDER) ? 2 : 1), 0);

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
	
	if (ROOM_BLD_FLAGGED(IN_ROOM(ch), BLD_MINE) && IS_COMPLETE(IN_ROOM(ch))) {
		if (get_room_extra_data(IN_ROOM(ch), ROOM_EXTRA_MINE_AMOUNT) > 0) {
			start_action(ch, ACT_MINING, mining_timer, 0);
			
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
*/
void start_panning(char_data *ch) {
	int panning_timer = config_get_int("panning_timer");
	
	if (find_flagged_sect_within_distance_from_char(ch, SECTF_FRESH_WATER, NOBITS, 1)) {
		start_action(ch, ACT_PANNING, panning_timer / (HAS_ABILITY(ch, ABIL_FINDER) ? 2 : 1), 0);
		
		msg_to_char(ch, "You kneel down and begin panning at the shore.\r\n");
		act("$n kneels down and begins panning at the shore.", TRUE, ch, 0, 0, TO_ROOM);
	}
}


/**
* begins the "pick" action
*
* @param char_data *ch The prospective picker.
*/
void start_picking(char_data *ch) {
	int pick_base_timer = config_get_int("pick_base_timer");
	
	start_action(ch, ACT_PICKING, pick_base_timer / (HAS_ABILITY(ch, ABIL_FINDER) ? 2 : 1), 0);
	send_to_char("You begin looking around.\r\n", ch);
}


/**
* Begin to quarry.
*
* @param char_data *ch The player who is to quarry.
*/
void start_quarrying(char_data *ch) {	
	if (BUILDING_VNUM(IN_ROOM(ch)) == BUILDING_QUARRY) {
		if (get_depletion(IN_ROOM(ch), DPLTN_QUARRY) >= config_get_int("common_depletion")) {
			msg_to_char(ch, "There's not enough stone left to cut here.\r\n");
		}
		else {
			start_action(ch, ACT_QUARRYING, 12, 0);
			send_to_char("You begin to quarry the stone.\r\n", ch);
			act("$n begins to quarry the stone.", TRUE, ch, 0, 0, TO_ROOM);
		}
	}
}


 //////////////////////////////////////////////////////////////////////////////
//// ACTION FINISHERS ////////////////////////////////////////////////////////

INTERACTION_FUNC(finish_digging) {	
	obj_vnum vnum = interaction->vnum;
	obj_data *obj = NULL;
	int num;
	
	// vnum override: clay happens near water tiles when NOT on rough terrain
	if (!ROOM_SECT_FLAGGED(inter_room, SECTF_ROUGH) && find_flagged_sect_within_distance_from_char(ch, SECTF_FRESH_WATER | SECTF_OCEAN, NOBITS, 1)) {
		if (number(0, 4)) {
			vnum = o_CLAY;
		}
	}
	
	// depleted? (uses rock for all types except clay)
	if (get_depletion(inter_room, DPLTN_DIG) >= DEPLETION_LIMIT(inter_room)) {
		msg_to_char(ch, "The ground is too hard and there doesn't seem to be anything useful to dig up here.\r\n");
	}
	else {
		for (num = 0; num < interaction->quantity; ++num) {
			obj = read_object(vnum);
			if (OBJ_FLAGGED(obj, OBJ_SCALABLE)) {
				scale_item_to_level(obj, 1);	// minimum level
			}
			obj_to_char_or_room(obj, ch);
			load_otrigger(obj);
			
			// add to depletion and 1/4 chance of adding a second one, to mix up the depletion values
			add_depletion(inter_room, DPLTN_DIG, TRUE);
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
		
			// if they have this, it levels now
			gain_ability_exp(ch, ABIL_FINDER, 5);
		}
	}
	
	return TRUE;
}


INTERACTION_FUNC(finish_gathering) {
	obj_data *obj = NULL;
	int iter;
	
	for (iter = 0; iter < interaction->quantity; ++iter) {
		obj = read_object(interaction->vnum);
		if (OBJ_FLAGGED(obj, OBJ_SCALABLE)) {
			scale_item_to_level(obj, 1);	// minimum level
		}
		obj_to_char_or_room(obj, ch);
		load_otrigger(obj);
		add_depletion(IN_ROOM(ch), DPLTN_GATHER, TRUE);
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
		
		if (GET_SKILL(ch, SKILL_SURVIVAL) < EMPIRE_CHORE_SKILL_CAP) {
			gain_skill_exp(ch, SKILL_SURVIVAL, 10);
		}
		// action does not end normally
		
		// if they have this, it levels now
		gain_ability_exp(ch, ABIL_FINDER, 5);
		
		return TRUE;
	}
	
	return FALSE;
}


INTERACTION_FUNC(finish_harvesting) {
	extern const sector_vnum climate_default_sector[NUM_CLIMATES];

	sector_data *sect = NULL;
	crop_data *cp;
	int count, num;
	obj_data *obj = NULL;
		
	if ((cp = crop_proto(ROOM_CROP_TYPE(inter_room))) && (sect = sector_proto(climate_default_sector[GET_CROP_CLIMATE(cp)]))) {
		// victory
		act("You finish harvesting the crop!", FALSE, ch, 0, 0, TO_CHAR);
		act("$n finished harvesting the crop!", FALSE, ch, 0, 0, TO_ROOM);

		// how many to get
		num = number(2, 6) + (HAS_ABILITY(ch, ABIL_MASTER_FARMER) ? number(2, 6) : 0);
		num *= interaction->quantity;
	
		// give them over
		for (count = 0; count < num; ++count) {
			obj = read_object(interaction->vnum);
			if (OBJ_FLAGGED(obj, OBJ_SCALABLE)) {
				scale_item_to_level(obj, 1);	// minimum level
			}
			obj_to_char_or_room(obj, ch);
			load_otrigger(obj);
		}

		// info messaging
		if (obj) {
			sprintf(buf, "You got $p (x%d)!", num);
			act(buf, FALSE, ch, obj, FALSE, TO_CHAR);
		}
	}
	else {
		sect = find_first_matching_sector(NOBITS, SECTF_HAS_CROP_DATA | SECTF_CROP | SECTF_MAP_BUILDING | SECTF_INSIDE | SECTF_ADVENTURE);
		msg_to_char(ch, "You finish harvesting but the crop appears to have died.\r\n");
		act("$n finishes harvesting, but the crop appears to have died.", FALSE, ch, NULL, NULL, TO_ROOM);
	}

	// finally, change the terrain
	if (sect) {
		change_terrain(inter_room, GET_SECT_VNUM(sect));
	}
	return TRUE;
}


INTERACTION_FUNC(finish_picking_herb) {
	obj_data *obj = NULL;
	room_data *in_room = IN_ROOM(ch);
	obj_vnum vnum = interaction->vnum;
	int iter, num = interaction->quantity;
	
	if (HAS_ABILITY(ch, ABIL_FIND_HERBS)) {
		gain_ability_exp(ch, ABIL_FIND_HERBS, 10);
	
		if (!number(0, 11)) {
			// random chance of gem
			vnum = o_IRIDESCENT_IRIS;
			num = 1;
		}
		else if (!number(0, 1) && find_flagged_sect_within_distance_from_room(inter_room, SECTF_FRESH_WATER, NOBITS, 1)) {
			vnum = o_BILEBERRIES;
		}
		else if (!number(0, 1) && find_flagged_sect_within_distance_from_room(inter_room, SECTF_OCEAN, NOBITS, 1)) {
			vnum = o_WHITEGRASS;
		}
	}
	else {
		// if they don't have find herbs, they get something different here
		// TODO move these to configs?
		vnum = (!number(0, 11) ? o_IRIDESCENT_IRIS : o_FLOWER);
		num = 1;
	}

	// give objs
	for (iter = 0; iter < num; ++iter) {
		obj = read_object(vnum);
		if (OBJ_FLAGGED(obj, OBJ_SCALABLE)) {
			scale_item_to_level(obj, 1);	// minimum level
		}
		obj_to_char_or_room(obj, ch);
		add_depletion(inter_room, DPLTN_PICK, TRUE);
		load_otrigger(obj);
	}
	
	if (obj) {
		if (num > 1) {
			sprintf(buf, "You find $p (d%d)!", num);
			act(buf, FALSE, ch, obj, 0, TO_CHAR);
		}
		else {
			act("You find $p!", FALSE, ch, obj, 0, TO_CHAR);
		}
		act("$n finds $p!", TRUE, ch, obj, 0, TO_ROOM);
		
		// if they have this, it levels now
		gain_ability_exp(ch, ABIL_FINDER, 5);
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


INTERACTION_FUNC(finish_picking_crop) {
	obj_data *obj = NULL;
	room_data *in_room = IN_ROOM(ch);
	int iter;
	
	// give objs
	for (iter = 0; iter < interaction->quantity; ++iter) {
		obj = read_object(interaction->vnum);
		if (OBJ_FLAGGED(obj, OBJ_SCALABLE)) {
			scale_item_to_level(obj, 1);	// minimum level
		}
		obj_to_char_or_room(obj, ch);
		add_depletion(inter_room, DPLTN_PICK, TRUE);
		load_otrigger(obj);
	}
	
	if (obj) {
		if (interaction->quantity > 1) {
			sprintf(buf, "You find $p (d%d)!", interaction->quantity);
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


 //////////////////////////////////////////////////////////////////////////////
//// ACTION PROCESSES ////////////////////////////////////////////////////////

// for do_saw
void perform_saw(char_data *ch) {
	ACMD(do_saw);
	
	char tmp[50];
	obj_data *obj, *proto;
	int iter;
	
	if (!PRF_FLAGGED(ch, PRF_NOSPAM)) {
		msg_to_char(ch, "You saw %s...\r\n", get_obj_name_by_proto(GET_ACTION_VNUM(ch, 0)));
	}
	
	// base
	GET_ACTION_TIMER(ch) -= 1;
	
	if (skill_check(ch, ABIL_WOODWORKING, DIFF_EASY)) {
		GET_ACTION_TIMER(ch) -= 1;
	}
	
	if (AFF_FLAGGED(ch, AFF_HASTE)) {
		GET_ACTION_TIMER(ch) -= 1;
	}
	
	if (HAS_BONUS_TRAIT(ch, BONUS_FAST_CHORES)) {
		GET_ACTION_TIMER(ch) -= 1;
	}

	if (GET_ACTION_TIMER(ch) <= 0) {
		GET_ACTION(ch) = ACT_NONE;
		
		// 2x lumber, always
		for (iter = 0; iter < 2; ++iter) {
			obj = read_object(o_LUMBER);
			obj_to_char_or_room(obj, ch);
			load_otrigger(obj);
		}

		act("You finish sawing $p (x2).", FALSE, ch, obj, NULL, TO_CHAR);
		act("$n finishes sawing $p.", TRUE, ch, obj, 0, TO_ROOM);
		
		if (GET_SKILL(ch, SKILL_EMPIRE) < EMPIRE_CHORE_SKILL_CAP) {
			gain_skill_exp(ch, SKILL_EMPIRE, 10);
		}
		if ((proto = obj_proto(GET_ACTION_VNUM(ch, 0)))) {
			strcpy(tmp, fname(GET_OBJ_KEYWORDS(proto)));
			do_saw(ch, tmp, 0, 0);
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
	void process_build(char_data *ch, room_data *room);
	
	int count, total;

	total = 1 + (AFF_FLAGGED(ch, AFF_HASTE) ? 1 : 0) + (HAS_BONUS_TRAIT(ch, BONUS_FAST_CHORES) ? 1 : 0);
	for (count = 0; count < total && GET_ACTION(ch) == ACT_BUILDING; ++count) {
		process_build(ch, IN_ROOM(ch));
	}
}


/**
* Tick update for chip action.
*
* @param char_data *ch The chipper one.
*/
void process_chipping(char_data *ch) {
	obj_data *obj;
	
	if (!find_chip_weapon(ch)) {
		msg_to_char(ch, "You need to be using some kind of hammer to chip it.\r\n");
		cancel_action(ch);
	}
	else {
		GET_ACTION_TIMER(ch) -= 1;
		
		if (HAS_BONUS_TRAIT(ch, BONUS_FAST_CHORES)) {
			GET_ACTION_TIMER(ch) -= 1;
		}
		
		if (!PRF_FLAGGED(ch, PRF_NOSPAM)) {
			msg_to_char(ch, "You chip away at the piece of rock...\r\n");
		}

		if (GET_ACTION_TIMER(ch) <= 0) {
			GET_ACTION(ch) = ACT_NONE;
			
			switch (GET_ACTION_VNUM(ch, 0)) {
				case o_ROCK: {
					obj = read_object(o_CHIPPED);
					obj_to_char_or_room(obj, ch);
					msg_to_char(ch, "It splits open!\r\n");
					if (GET_SKILL(ch, SKILL_TRADE) < EMPIRE_CHORE_SKILL_CAP) {
						gain_skill_exp(ch, SKILL_TRADE, 25);
					}
					load_otrigger(obj);
					break;
				}
				case o_CHIPPED: {
					obj = read_object(o_HANDAXE);
					obj_to_char_or_room(obj, ch);
					act("You have crafted $p!", FALSE, ch, obj, 0, TO_CHAR);
					if (GET_SKILL(ch, SKILL_TRADE) < EMPIRE_CHORE_SKILL_CAP) {
						gain_skill_exp(ch, SKILL_TRADE, 25);
					}
					load_otrigger(obj);
					break;
				}
				case o_HANDAXE: {
					obj = read_object(o_SPEARHEAD);
					obj_to_char_or_room(obj, ch);
					act("You have crafted $p!", FALSE, ch, obj, 0, TO_CHAR);
					if (GET_SKILL(ch, SKILL_TRADE) < EMPIRE_CHORE_SKILL_CAP) {
						gain_skill_exp(ch, SKILL_TRADE, 25);
					}
					load_otrigger(obj);
					break;
				}
			}
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
	
	char_data *ch_iter;
	int count, total, trees = 1;
	obj_data *obj;
	
	total = 1 + (AFF_FLAGGED(ch, AFF_HASTE) ? 1 : 0) + (HAS_BONUS_TRAIT(ch, BONUS_FAST_CHORES) ? 1 : 0);

	for (count = 0; count < total && GET_ACTION(ch) == ACT_CHOPPING; ++count) {
		if (!GET_EQ(ch, WEAR_WIELD) || GET_WEAPON_TYPE(GET_EQ(ch, WEAR_WIELD)) != TYPE_SLICE) {
			send_to_char("You need to be wielding an axe to chop.\r\n", ch);
			cancel_action(ch);
			break;
		}

		add_to_room_extra_data(IN_ROOM(ch), ROOM_EXTRA_CHOP_PROGRESS, -1 * (GET_STRENGTH(ch) + 1.5 * get_base_dps(GET_EQ(ch, WEAR_WIELD))));
		act("You swing $p hard into the tree!", FALSE, ch, GET_EQ(ch, WEAR_WIELD), 0, TO_CHAR | TO_SPAMMY);
		act("$n swings $p hard into the tree!", FALSE, ch, GET_EQ(ch, WEAR_WIELD), 0, TO_ROOM | TO_SPAMMY);

		if (get_room_extra_data(IN_ROOM(ch), ROOM_EXTRA_CHOP_PROGRESS) <= 0) {
			remove_room_extra_data(IN_ROOM(ch), ROOM_EXTRA_CHOP_PROGRESS);
			trees = change_chop_territory(IN_ROOM(ch));

			if (trees > 1) {
				sprintf(buf1, " (x%d)", trees);
			}
			else {
				*buf1 = '\0';
			}
			
			sprintf(buf, "With a loud crack, the tree falls%s!", buf1);
			act(buf, FALSE, ch, 0, 0, TO_CHAR | TO_ROOM);
			
			// give a tree
			while (trees-- > 0) {
				obj = read_object(o_TREE);
				obj_to_char_or_room(obj, ch);
				load_otrigger(obj);
			}
		
			if (GET_SKILL(ch, SKILL_EMPIRE) < EMPIRE_CHORE_SKILL_CAP) {
				gain_skill_exp(ch, SKILL_EMPIRE, 15);
			}
			
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
}


/**
* Tick update for digging action.
*
* @param char_data *ch The digger.
*/
void process_digging(char_data *ch) {
	room_data *in_room;

	// decrement timer
	GET_ACTION_TIMER(ch) -= 1;
	
	// detect shovel in either hand
	if ((GET_EQ(ch, WEAR_WIELD) && OBJ_FLAGGED(GET_EQ(ch, WEAR_WIELD), OBJ_TOOL_SHOVEL)) || (GET_EQ(ch, WEAR_HOLD) && OBJ_FLAGGED(GET_EQ(ch, WEAR_HOLD), OBJ_TOOL_SHOVEL))) {
		GET_ACTION_TIMER(ch) -= 1;
	}
	
	// fast chores?
	if (HAS_BONUS_TRAIT(ch, BONUS_FAST_CHORES)) {
		GET_ACTION_TIMER(ch) -= 1;
	}
	
	// done?
	if (GET_ACTION_TIMER(ch) <= 0) {
		GET_ACTION(ch) = ACT_NONE;
		in_room = IN_ROOM(ch);
		
		if (run_room_interactions(ch, IN_ROOM(ch), INTERACT_DIG, finish_digging)) {
			// success
			if (GET_SKILL(ch, SKILL_SURVIVAL) < EMPIRE_CHORE_SKILL_CAP) {
				gain_skill_exp(ch, SKILL_SURVIVAL, 10);
			}
		
			// character is still there and not digging?
			if (GET_ACTION(ch) == ACT_NONE && in_room == IN_ROOM(ch)) {
				if (get_depletion(IN_ROOM(ch), DPLTN_DIG) < DEPLETION_LIMIT(IN_ROOM(ch))) {
					start_digging(ch);
				}
			}
		}
		else {
			msg_to_char(ch, "You don't seem to be able to find anything to dig for.\r\n");
		}
	}
	else {
		// normal tick message
		if (!PRF_FLAGGED(ch, PRF_NOSPAM)) {
			send_to_char("You dig vigorously at the ground.\r\n", ch);
		}
		act("$n digs vigorously at the ground.", FALSE, ch, 0, 0, TO_ROOM | TO_SPAMMY);
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
	
	total = 1 + (AFF_FLAGGED(ch, AFF_HASTE) ? 1 : 0) + (HAS_BONUS_TRAIT(ch, BONUS_FAST_CHORES) ? 1 : 0);
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

	total = 1 + (AFF_FLAGGED(ch, AFF_HASTE) ? 1 : 0) + (HAS_BONUS_TRAIT(ch, BONUS_FAST_CHORES) ? 1 : 0);
	for (count = 0; count < total && GET_ACTION(ch) == ACT_EXCAVATING; ++count) {
		if ((!GET_EQ(ch, WEAR_HOLD) || !OBJ_FLAGGED(GET_EQ(ch, WEAR_HOLD), OBJ_TOOL_SHOVEL)) && (!GET_EQ(ch, WEAR_WIELD) || !OBJ_FLAGGED(GET_EQ(ch, WEAR_WIELD), OBJ_TOOL_SHOVEL))) {
			msg_to_char(ch, "You need a shovel to excavate.\r\n");
			cancel_action(ch);
		}
		else if (!ROOM_SECT_FLAGGED(IN_ROOM(ch), SECTF_IS_TRENCH)) {
			msg_to_char(ch, "You stop excavating, as this is no longer a trench.\r\n");
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
				
				// this also stops ch
				stop_room_action(IN_ROOM(ch), ACT_EXCAVATING, NOTHING);
			}
		}
	}
}


/**
* Tick update for fillin action.
*
* @param char_data *ch The filler-inner.
*/
void process_fillin(char_data *ch) {	
	sector_data *to_sect;
	int count, total;

	total = 1 + (AFF_FLAGGED(ch, AFF_HASTE) ? 1 : 0) + (HAS_BONUS_TRAIT(ch, BONUS_FAST_CHORES) ? 1 : 0);
	for (count = 0; count < total && GET_ACTION(ch) == ACT_FILLING_IN; ++count) {
		if ((!GET_EQ(ch, WEAR_HOLD) || !OBJ_FLAGGED(GET_EQ(ch, WEAR_HOLD), OBJ_TOOL_SHOVEL)) && (!GET_EQ(ch, WEAR_WIELD) || !OBJ_FLAGGED(GET_EQ(ch, WEAR_WIELD), OBJ_TOOL_SHOVEL))) {
			msg_to_char(ch, "You need a shovel to fill in the trench.\r\n");
			cancel_action(ch);
		}
		else if (!ROOM_SECT_FLAGGED(IN_ROOM(ch), SECTF_IS_TRENCH)) {
			msg_to_char(ch, "You stop filling in, as this is no longer a trench.\r\n");
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
				
				// stop BOTH actions -- it's not a trench!
				stop_room_action(IN_ROOM(ch), ACT_FILLING_IN, NOTHING);
				stop_room_action(IN_ROOM(ch), ACT_EXCAVATING, NOTHING);

				// de-evolve sect
				to_sect = reverse_lookup_evolution_for_sector(SECT(IN_ROOM(ch)), EVO_TRENCH_START);
				if (to_sect) {
					change_terrain(IN_ROOM(ch), GET_SECT_VNUM(to_sect));
					REMOVE_BIT(ROOM_AFF_FLAGS(IN_ROOM(ch)), ROOM_AFF_PLAYER_MADE);
					REMOVE_BIT(ROOM_BASE_FLAGS(IN_ROOM(ch)), ROOM_AFF_PLAYER_MADE);
				}
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
	extern const struct fishing_data_type fishing_data[];
	
	obj_data *obj;
	int prc, iter, rand;
	obj_vnum vnum = NOTHING;
	
	int fishing_timer = config_get_int("fishing_timer");
	
	if (!GET_EQ(ch, WEAR_WIELD) || GET_OBJ_TYPE(GET_EQ(ch, WEAR_WIELD)) != ITEM_WEAPON || GET_WEAPON_TYPE(GET_EQ(ch, WEAR_WIELD)) != TYPE_JAB) {
		msg_to_char(ch, "You'll need a spear to fish.\r\n");
		cancel_action(ch);
		return;
	}

	GET_ACTION_TIMER(ch) -= GET_CHARISMA(ch) + (skill_check(ch, ABIL_FISH, DIFF_MEDIUM) ? 2 : 0);
	
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
					msg_to_char(ch, "The water waves peacefully, but you don't see any fish..\r\n");
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
	else if (get_depletion(IN_ROOM(ch), DPLTN_FISH) >= DEPLETION_LIMIT(IN_ROOM(ch))) {
		msg_to_char(ch, "You just don't seem to be able to catch anything here.\r\n");
		GET_ACTION(ch) = ACT_NONE;
	}
	else {
		// restart action first
		start_action(ch, ACT_FISHING, fishing_timer / (skill_check(ch, ABIL_FISH, DIFF_EASY) ? 2 : 1), 0);

		// find loot
		prc = 0;
		rand = number(1, 10000);
		for (iter = 0; (iter == 0 || fishing_data[iter - 1].chance < 100) && vnum == NOTHING; ++iter) {
			prc += fishing_data[iter].chance * 100;
			if (rand < prc) {
				vnum = fishing_data[iter].vnum;
			}
		}

		// dole loot
		if (vnum != NOTHING) {
			obj = read_object(vnum);
			obj_to_char_or_room(obj, ch);
			
			add_depletion(IN_ROOM(ch), DPLTN_FISH, TRUE);
		
			msg_to_char(ch, "A fish darts past you..\r\n");
			act("You jab your spear into the water and when you extract it you find $p on the end!", FALSE, ch, obj, 0, TO_CHAR);
			act("$n jabs $s spear into the water and when $e draws it out, it has $p on the end!", TRUE, ch, obj, 0, TO_ROOM);
			gain_ability_exp(ch, ABIL_FISH, 10);
			load_otrigger(obj);
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
		send_to_char("You search the ground for sticks...\r\n", ch);
	}
	act("$n searches around on the ground...", TRUE, ch, 0, 0, TO_ROOM | TO_SPAMMY);
	GET_ACTION_TIMER(ch) -= 1;
	
	if (HAS_BONUS_TRAIT(ch, BONUS_FAST_CHORES)) {
		GET_ACTION_TIMER(ch) -= 1;
	}
	
	// done ?
	if (GET_ACTION_TIMER(ch) <= 0) {
		if (get_depletion(IN_ROOM(ch), DPLTN_GATHER) >= gather_depletion) {
			msg_to_char(ch, "There aren't any good sticks left to gather here.\r\n");
			GET_ACTION(ch) = ACT_NONE;
		}
		else {
			if (run_room_interactions(ch, IN_ROOM(ch), INTERACT_GATHER, finish_gathering)) {
				// check repeatability
				if (CAN_INTERACT_ROOM(IN_ROOM(ch), INTERACT_GATHER)) {
					GET_ACTION_TIMER(ch) = gather_base_timer / (HAS_ABILITY(ch, ABIL_FINDER) ? 2 : 1);
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
	if (!GET_EQ(ch, WEAR_WIELD) || (GET_WEAPON_TYPE(GET_EQ(ch, WEAR_WIELD)) != TYPE_SLICE && GET_WEAPON_TYPE(GET_EQ(ch, WEAR_WIELD)) != TYPE_SLASH)) {
		send_to_char("You're not wielding the proper tool for harvesting.\r\n", ch);
		cancel_action(ch);
		return;
	}

	// tick messaging
	switch (number(0, 2)) {
		case 0: {
			if (!PRF_FLAGGED(ch, PRF_NOSPAM)) {
				msg_to_char(ch, "You walk through the field, harvesting the %s.\r\n", GET_CROP_NAME(crop_proto(ROOM_CROP_TYPE(IN_ROOM(ch)))));
			}
			sprintf(buf, "$n walks through the field, harvesting the %s.", GET_CROP_NAME(crop_proto(ROOM_CROP_TYPE(IN_ROOM(ch)))));
			act(buf, FALSE, ch, 0, 0, TO_ROOM | TO_SPAMMY);
			break;
		}
		case 1: {
			if (!PRF_FLAGGED(ch, PRF_NOSPAM)) {
				msg_to_char(ch, "You carefully harvest the %s.\r\n", GET_CROP_NAME(crop_proto(ROOM_CROP_TYPE(IN_ROOM(ch)))));
			}
			sprintf(buf, "$n carefully harvests the %s.", GET_CROP_NAME(crop_proto(ROOM_CROP_TYPE(IN_ROOM(ch)))));
			act(buf, FALSE, ch, 0, 0, TO_ROOM | TO_SPAMMY);
			break;
		}
	}
	
	// update progress (on the room, not the player's action timer)
	add_to_room_extra_data(IN_ROOM(ch), ROOM_EXTRA_HARVEST_PROGRESS, -1 * (1 + (HAS_BONUS_TRAIT(ch, BONUS_FAST_CHORES) ? 1 : 0) + GET_STRENGTH(ch) * (AFF_FLAGGED(ch, AFF_HASTE) ? 2 : 1)));

	if (get_room_extra_data(IN_ROOM(ch), ROOM_EXTRA_HARVEST_PROGRESS) <= 0) {
		// lower limit
		remove_room_extra_data(IN_ROOM(ch), ROOM_EXTRA_HARVEST_PROGRESS);

		// stop all harvesters including ch
		stop_room_action(IN_ROOM(ch), ACT_HARVESTING, CHORE_FARMING);
		
		if (run_room_interactions(ch, IN_ROOM(ch), INTERACT_HARVEST, finish_harvesting)) {
			// skillups
			if (GET_SKILL(ch, SKILL_EMPIRE) < EMPIRE_CHORE_SKILL_CAP) {
				gain_skill_exp(ch, SKILL_EMPIRE, 30);
			}
			gain_ability_exp(ch, ABIL_MASTER_FARMER, 5);
		}
		else {
			msg_to_char(ch, "You fail to harvest anything here.\r\n");
		}
	}
}


/**
* Tick update for mine action.
*
* @param char_data *ch The miner.
*/
void process_mining(char_data *ch) {
	extern obj_vnum find_mine_vnum_by_type(int type);
	
	obj_data *obj;
	int count, total;
	room_data *in_room;

	total = 1 + (AFF_FLAGGED(ch, AFF_HASTE) ? 1 : 0) + (HAS_BONUS_TRAIT(ch, BONUS_FAST_CHORES) ? 1 : 0);
	for (count = 0; count < total && GET_ACTION(ch) == ACT_MINING; ++count) {
		if (!GET_EQ(ch, WEAR_WIELD) || ((GET_WEAPON_TYPE(GET_EQ(ch, WEAR_WIELD)) != TYPE_PICK) && GET_OBJ_VNUM(GET_EQ(ch, WEAR_WIELD)) != o_HANDAXE)) {
			send_to_char("You're not using the proper tool for mining!\r\n", ch);
			cancel_action(ch);
			break;
		}
		if (!ROOM_BLD_FLAGGED(IN_ROOM(ch), BLD_MINE) || !IS_COMPLETE(IN_ROOM(ch)) || get_room_extra_data(IN_ROOM(ch), ROOM_EXTRA_MINE_AMOUNT) <= 0) {
			msg_to_char(ch, "You can't mine here.\r\n");
			cancel_action(ch);
			break;
		}

		GET_ACTION_TIMER(ch) -= GET_STRENGTH(ch) + 1.5 * get_base_dps(GET_EQ(ch, WEAR_WIELD));

		act("You pick at the walls with $p, looking for ore.", FALSE, ch, GET_EQ(ch, WEAR_WIELD), 0, TO_CHAR | TO_SPAMMY);
		act("$n picks at the walls with $p, looking for ore.", FALSE, ch, GET_EQ(ch, WEAR_WIELD), 0, TO_ROOM | TO_SPAMMY);

		// done??
		if (GET_ACTION_TIMER(ch) <= 0) {
			in_room = IN_ROOM(ch);
			
			// amount of ore remaining
			add_to_room_extra_data(IN_ROOM(ch), ROOM_EXTRA_MINE_AMOUNT, -1);

			obj = read_object(find_mine_vnum_by_type(get_room_extra_data(IN_ROOM(ch), ROOM_EXTRA_MINE_TYPE)));
			obj_to_char_or_room(obj, ch);
	
			act("With that last stroke, $p falls from the wall!", FALSE, ch, obj, 0, TO_CHAR);
			act("With $s last stroke, $p falls from the wall where $n was picking!", FALSE, ch, obj, 0, TO_ROOM);

			GET_ACTION(ch) = ACT_NONE;
			if (GET_SKILL(ch, SKILL_EMPIRE) < EMPIRE_CHORE_SKILL_CAP) {
				gain_skill_exp(ch, SKILL_EMPIRE, 10);
			}
			load_otrigger(obj);
	
			// go again! (if ch is still there)
			if (in_room == IN_ROOM(ch)) {
				start_mining(ch);
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
	
	GET_ACTION_TIMER(ch) -= 1;
	if (HAS_BONUS_TRAIT(ch, BONUS_FAST_CHORES)) {
		GET_ACTION_TIMER(ch) -= 1;
	}
	
	if (GET_ACTION_TIMER(ch) > 0) {
		if (!number(0, 1)) {
			act("You turn the coin mill...", FALSE, ch, NULL, NULL, TO_CHAR | TO_SPAMMY);
			act("$n turns the coin mill...", FALSE, ch, NULL, NULL, TO_ROOM | TO_SPAMMY);
		}
	}
	else {
		num = GET_ACTION_VNUM(ch, 1);
		msg_to_char(ch, "You finish milling and receive %s!\r\n", money_amount(emp, num));
		act("$n finishes minting some coins!", FALSE, ch, NULL, NULL, TO_ROOM);
		increase_coins(ch, emp, num);
		
		GET_ACTION(ch) = ACT_NONE;
		if (GET_SKILL(ch, SKILL_EMPIRE) < EMPIRE_CHORE_SKILL_CAP) {
			gain_skill_exp(ch, SKILL_EMPIRE, 10);
		}
		
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
	void finish_morphing(char_data *ch, int morph_to);

	GET_ACTION_TIMER(ch) -= 1;

	if (GET_ACTION_TIMER(ch) <= 0) {
		finish_morphing(ch, GET_ACTION_VNUM(ch, 0));
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
		if (has_custom_message(obj, OBJ_CUSTOM_INSTRUMENT_TO_CHAR)) {
			act(get_custom_message(obj, OBJ_CUSTOM_INSTRUMENT_TO_CHAR), FALSE, ch, obj, 0, TO_CHAR | TO_SPAMMY);
		}
		
		if (has_custom_message(obj, OBJ_CUSTOM_INSTRUMENT_TO_ROOM)) {
			act(get_custom_message(obj, OBJ_CUSTOM_INSTRUMENT_TO_ROOM), FALSE, ch, obj, 0, TO_ROOM | TO_SPAMMY);
		}
	}
}


/**
* Tick update for pan action.
*
* @param char_data *ch The panner.
*/
void process_panning(char_data *ch) {	
	room_data *in_room;
	obj_data *obj;
	
	int short_depletion = config_get_int("short_depletion");

	if (!GET_EQ(ch, WEAR_HOLD) || !OBJ_FLAGGED(GET_EQ(ch, WEAR_HOLD), OBJ_TOOL_PAN)) {
		msg_to_char(ch, "You need to be holding a pan to do that.\r\n");
		cancel_action(ch);
	}
	else if (!find_flagged_sect_within_distance_from_char(ch, SECTF_FRESH_WATER, NOBITS, 1)) {
		msg_to_char(ch, "You can no longer pan here.\r\n");
		cancel_action(ch);
	}
	else {
		GET_ACTION_TIMER(ch) -= 1;

		if (!PRF_FLAGGED(ch, PRF_NOSPAM)) {
			msg_to_char(ch, "You sift through the sand and pebbles, looking for gold...\r\n");
		}
		act("$n sifts through the sand, looking for gold...", TRUE, ch, 0, 0, TO_ROOM | TO_SPAMMY);

		if (GET_ACTION_TIMER(ch) <= 0) {
			GET_ACTION(ch) = ACT_NONE;
			
			if (!number(0, 19) && get_depletion(IN_ROOM(ch), DPLTN_PAN) <= short_depletion) {
				in_room = IN_ROOM(ch);
				obj_to_char_or_room((obj = read_object(o_GOLD_SMALL)), ch);
				act("You find $p!", FALSE, ch, obj, 0, TO_CHAR);
				add_depletion(IN_ROOM(ch), DPLTN_PAN, TRUE);
				load_otrigger(obj);
				
				// if they have this, it levels now
				gain_ability_exp(ch, ABIL_FINDER, 5);
				
				if (in_room == IN_ROOM(ch)) {
					start_panning(ch);
				}
			}
			else {
				msg_to_char(ch, "You find nothing of value.\r\n");
				start_panning(ch);
			}
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
	
	if (!PRF_FLAGGED(ch, PRF_NOSPAM)) {
		send_to_char("You look around for something nice to pick...\r\n", ch);
	}
	// no room message

	// decrement
	GET_ACTION_TIMER(ch) -= 1;
	
	if (HAS_BONUS_TRAIT(ch, BONUS_FAST_CHORES)) {
		GET_ACTION_TIMER(ch) -= 1;
	}
	
	if (GET_ACTION_TIMER(ch) <= 0) {
		GET_ACTION(ch) = ACT_NONE;
		
		if (get_depletion(IN_ROOM(ch), DPLTN_PICK) >= (ROOM_SECT_FLAGGED(IN_ROOM(ch), SECTF_CROP) ? pick_depletion : (IS_ANY_BUILDING(IN_ROOM(ch)) ? garden_depletion : config_get_int("common_depletion")))) {
			msg_to_char(ch, "You can't find anything here left to pick.\r\n");
			act("$n stops looking for things to pick as $e comes up empty-handed.", TRUE, ch, NULL, NULL, TO_ROOM);
		}
		else {
			if (run_room_interactions(ch, IN_ROOM(ch), INTERACT_FIND_HERB, finish_picking_herb)) {
				if (GET_SKILL(ch, SKILL_SURVIVAL) < EMPIRE_CHORE_SKILL_CAP) {
					gain_skill_exp(ch, SKILL_SURVIVAL, 10);
				}
				found = TRUE;
			}
			else if (CAN_INTERACT_ROOM(IN_ROOM(ch), INTERACT_HARVEST) && (IS_ADVENTURE_ROOM(IN_ROOM(ch)) || ROOM_CROP_FLAGGED(IN_ROOM(ch), CROPF_IS_ORCHARD))) {
				// only orchards allow pick -- and only run this if we hit no herbs at all
				if (run_room_interactions(ch, IN_ROOM(ch), INTERACT_HARVEST, finish_picking_crop)) {
					if (GET_SKILL(ch, SKILL_SURVIVAL) < EMPIRE_CHORE_SKILL_CAP) {
						gain_skill_exp(ch, SKILL_SURVIVAL, 10);
					}
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
	// decrement
	GET_ACTION_TIMER(ch) -= 1;
	
	if (HAS_BONUS_TRAIT(ch, BONUS_FAST_CHORES)) {
		GET_ACTION_TIMER(ch) -= 1;
	}
	
	// starts at 4
	switch (GET_ACTION_TIMER(ch)) {
		case 3: {
			if (!PRF_FLAGGED(ch, PRF_NOSPAM)) {
				msg_to_char(ch, "You carefully plant seeds in rows along the ground.\r\n");
			}
			act("$n carefully plants seeds in rows along the ground.", FALSE, ch, NULL, NULL, TO_ROOM | TO_SPAMMY);
			multiply_room_extra_data(IN_ROOM(ch), ROOM_EXTRA_SEED_TIME, 0.5);
			break;
		}
		case 2: {
			if (!PRF_FLAGGED(ch, PRF_NOSPAM)) {
				msg_to_char(ch, "You cover the seeds and gently pack the dirt.\r\n");
			}
			act("$n covers rows of seeds with dirt and gently packs them down.", FALSE, ch, NULL, NULL, TO_ROOM | TO_SPAMMY);
			multiply_room_extra_data(IN_ROOM(ch), ROOM_EXTRA_SEED_TIME, 0.5);
			break;
		}
		case 1: {
			if (!PRF_FLAGGED(ch, PRF_NOSPAM)) {
				msg_to_char(ch, "You water the freshly seeded ground.\r\n");
			}
			act("$n waters the freshly seeded ground.", FALSE, ch, NULL, NULL, TO_ROOM | TO_SPAMMY);
			multiply_room_extra_data(IN_ROOM(ch), ROOM_EXTRA_SEED_TIME, 0.5);
			break;
		}
	}
	
	if (GET_ACTION_TIMER(ch) <= 0) {
		msg_to_char(ch, "You have finished planting!\r\n");
		act("$n finishes planting!", FALSE, ch, 0, 0, TO_ROOM);
		
		if (GET_SKILL(ch, SKILL_EMPIRE) < EMPIRE_CHORE_SKILL_CAP) {
			gain_skill_exp(ch, SKILL_EMPIRE, 30);
		}
		GET_ACTION(ch) = ACT_NONE;
	}
}


/**
* Tick update for prospect action.
*
* @param char_data *ch The prospector.
*/
void process_prospecting(char_data *ch) {
	extern int find_mine_type(int type);
	extern char *get_mine_type_name(room_data *room);
	extern bool is_deep_mine(room_data *room);
	void init_mine(room_data *room, char_data *ch);
	
	int type;
	
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
			init_mine(IN_ROOM(ch), ch);
			type = find_mine_type(get_room_extra_data(IN_ROOM(ch), ROOM_EXTRA_MINE_TYPE));
			
			if (get_room_extra_data(IN_ROOM(ch), ROOM_EXTRA_MINE_AMOUNT) <= 0 || type == NOTHING) {
				msg_to_char(ch, "This area has already been mined for all it's worth.\r\n");
			}
			else {
				msg_to_char(ch, "You discover that this area %s %s.\r\n", (is_deep_mine(IN_ROOM(ch)) ? "has a deep vein of" : "is rich in"), get_mine_type_name(IN_ROOM(ch)));
				act("$n finishes prospecting.", TRUE, ch, NULL, NULL, TO_ROOM);
			}
			
			gain_ability_exp(ch, ABIL_PROSPECT, 15);
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
	obj_vnum vnum = NOTHING;
	room_data *in_room;
	obj_data *obj;
	
	GET_ACTION_TIMER(ch) -= 1;
	
	if (HAS_BONUS_TRAIT(ch, BONUS_FAST_CHORES)) {
		GET_ACTION_TIMER(ch) -= 1;
	}
	
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
	else {
		in_room = IN_ROOM(ch);
		GET_ACTION(ch) = ACT_NONE;
		
		// possibly others later
		vnum = o_STONE_BLOCK;
		
		add_depletion(IN_ROOM(ch), DPLTN_QUARRY, TRUE);
		
		obj = read_object(vnum);
		obj_to_char_or_room(obj, ch);
		act("You give the plug drill one final swing and pry loose $p!", FALSE, ch, obj, 0, TO_CHAR);
		act("$n hits the plug drill hard with a hammer and pries loose $p!", FALSE, ch, obj, 0, TO_ROOM);
	
		if (GET_SKILL(ch, SKILL_EMPIRE) < EMPIRE_CHORE_SKILL_CAP) {
			gain_skill_exp(ch, SKILL_EMPIRE, 25);
		}
		load_otrigger(obj);
		
		// still there?
		if (in_room == IN_ROOM(ch)) {
			start_quarrying(ch);
		}
	}
}


/**
* Tick update for scrape action.
*
* @param char_data *ch The scraper.
*/
void process_scraping(char_data *ch) {
	ACMD(do_scrape);
	
	int count, total;
	obj_data *obj, *stick = NULL;

	if (!(obj = has_sharp_tool(ch))) {
		msg_to_char(ch, "You need to be using a sharp tool to scrape it.\r\n");
		cancel_action(ch);
	}
	else {
		// skilled work
		GET_ACTION_TIMER(ch) -= 1 + (skill_check(ch, ABIL_WOODWORKING, DIFF_EASY) ? 1 : 0) + (AFF_FLAGGED(ch, AFF_HASTE) ? 1 : 0) + (HAS_BONUS_TRAIT(ch, BONUS_FAST_CHORES) ? 1 : 0);
	
		// messaging -- to player only
		if (!PRF_FLAGGED(ch, PRF_NOSPAM)) {
			msg_to_char(ch, "You scrape at %s...\r\n", get_obj_name_by_proto(o_TREE));
		}
	
		// done?
		if (GET_ACTION_TIMER(ch) <= 0) {
			GET_ACTION(ch) = ACT_NONE;
			
			obj_to_char_or_room((obj = read_object(o_LOG)), ch);
			
			// sticks!
			total = number(2, 5);
			for (count = 0; count < total; ++count) {
				stick = read_object(o_STICK);
				obj_to_char_or_room(stick, ch);
				load_otrigger(stick);
			}
			
			sprintf(buf, "You finish scraping off $p and manage to get $P (x%d)!", total);
			act(buf, FALSE, ch, obj, stick, TO_CHAR);
			act("$n finishes scraping off $p!", TRUE, ch, obj, 0, TO_ROOM);
	
			if (GET_SKILL(ch, SKILL_EMPIRE) < EMPIRE_CHORE_SKILL_CAP) {
				gain_skill_exp(ch, SKILL_EMPIRE, 10);
			}
			load_otrigger(obj);
			
			// lather, rinse, rescrape
			do_scrape(ch, "tree", 0, 0);
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
* Tick update for smelt action.
*
* @param char_data *ch The smelter.
*/
void process_smelting(char_data *ch) {
	ACMD(do_smelt);
	
	obj_data *proto, *obj = NULL;
	int iter, type = NOTHING;

	// decrement
	GET_ACTION_TIMER(ch) -= 1;

	if (HAS_BONUS_TRAIT(ch, BONUS_FAST_CHORES)) {
		GET_ACTION_TIMER(ch) -= 1;
	}

	if (GET_ACTION_TIMER(ch) > 0) {
		if (!number(0, 2) && !PRF_FLAGGED(ch, PRF_NOSPAM)) {
			msg_to_char(ch, "You watch as the metal forms in the fire.\r\n");
		}
	}
	else {
		// DONE!

		// Find the entry in the table
		for (iter = 0; smelt_data[iter].from != NOTHING && type == NOTHING; ++iter) {
			if (smelt_data[iter].from == GET_ACTION_VNUM(ch, 0)) {
				type = iter;
			}
		}

		if (type == NOTHING) {
			// ... somehow
			cancel_action(ch);
			msg_to_char(ch, "Something went wrong with your smelting (error 1).\r\n");
		}
		else {
			for (iter = 0; iter < smelt_data[type].to_amt; ++iter) {
				obj_to_char_or_room((obj = read_object(smelt_data[type].to)), ch);
				load_otrigger(obj);
			}

			sprintf(buf2, " (x%d)", smelt_data[type].to_amt);
			
			sprintf(buf, "You have successfully created $p%s!", smelt_data[type].to_amt > 1 ? buf2 : "");
			sprintf(buf1, "$n has successfully created $p%s!", smelt_data[type].to_amt > 1 ? buf2 : "");

			act(buf, FALSE, ch, obj, 0, TO_CHAR);
			act(buf1, TRUE, ch, obj, 0, TO_ROOM);

			GET_ACTION(ch) = ACT_NONE;
	
			// repeat!
			if ((proto = obj_proto(smelt_data[type].from))) {
				strcpy(buf, fname(GET_OBJ_KEYWORDS(proto)));
				do_smelt(ch, buf, 0, 0);
			}
		}
	}
}


/**
* Ticker for tanners.
*
* @param char_data *ch The tanner.
*/
void process_tanning(char_data *ch) {
	ACMD(do_tan);
	obj_data *proto, *obj = NULL;
	int iter, type = NOTHING;
		
	GET_ACTION_TIMER(ch) -= (BUILDING_VNUM(IN_ROOM(ch)) == BUILDING_TANNERY ? 4 : 1);
	
	if (HAS_BONUS_TRAIT(ch, BONUS_FAST_CHORES)) {
		GET_ACTION_TIMER(ch) -= 1;
	}

	if (GET_ACTION_TIMER(ch) <= 0) {
		/* Find the entry in the table */
		for (iter = 0; type == NOTHING && tan_data[iter].from != NOTHING; ++iter) {
			if (tan_data[iter].from == GET_ACTION_VNUM(ch, 0)) {
				type = iter;
			}
		}

		if (type == NOTHING || tan_data[type].to == NOTHING) {
			cancel_action(ch);
			return;
		}

		obj_to_char_or_room((obj = read_object(tan_data[type].to)), ch);

		act("You finish tanning $p.", FALSE, ch, obj, 0, TO_CHAR);
		act("$n finishes tanning $p.", TRUE, ch, obj, 0, TO_ROOM);

		GET_ACTION(ch) = ACT_NONE;
	
		if (GET_SKILL(ch, SKILL_TRADE) < EMPIRE_CHORE_SKILL_CAP) {
			gain_skill_exp(ch, SKILL_TRADE, 20);
		}
		load_otrigger(obj);
	
		// repeat!
		if ((proto = obj_proto(tan_data[type].from))) {
			strcpy(buf, fname(GET_OBJ_KEYWORDS(proto)));
			do_tan(ch, buf, 0, 0);
		}
	}
	else {
		switch (GET_ACTION_TIMER(ch)) {
			case 10: {	// non-tannery only
				act("You soak the skin...", FALSE, ch, NULL, NULL, TO_CHAR | TO_SPAMMY);
				act("$n soaks the skin...", FALSE, ch, NULL, NULL, TO_ROOM | TO_SPAMMY);
				break;
			}
			case 8: {	// all
				act("You scrape the skin clean...", FALSE, ch, NULL, NULL, TO_CHAR | TO_SPAMMY);
				act("$n scrapes the skin clean...", FALSE, ch, NULL, NULL, TO_ROOM | TO_SPAMMY);
				break;
			}
			case 6: {	// non-tannery only
				act("You brush the skin...", FALSE, ch, NULL, NULL, TO_CHAR | TO_SPAMMY);
				act("$n brushes the skin...", FALSE, ch, NULL, NULL, TO_ROOM | TO_SPAMMY);
				break;
			}
			case 4: {	// all
				act("You knead the skin with a foul-smelling mixture...", FALSE, ch, NULL, NULL, TO_CHAR | TO_SPAMMY);
				act("$n kneads the skin with a foul-smelling mixture...", FALSE, ch, NULL, NULL, TO_ROOM | TO_SPAMMY);
				break;
			}
			case 2: {	// non-tannery only
				act("You stretch the raw leather...", FALSE, ch, NULL, NULL, TO_CHAR | TO_SPAMMY);
				act("$n stretches the raw leather...", FALSE, ch, NULL, NULL, TO_ROOM | TO_SPAMMY);
				break;
			}
		}
	}
}


/**
* Tick update for weave action.
*
* @param char_data *ch The weaver.
*/
void process_weaving(char_data *ch) {
	void finish_weaving(char_data *ch);
	
	GET_ACTION_TIMER(ch) -= 1;
	
	if (HAS_BONUS_TRAIT(ch, BONUS_FAST_CHORES)) {
		GET_ACTION_TIMER(ch) -= 1;
	}
	
	if (GET_ACTION_TIMER(ch) > 0) {
		if (!PRF_FLAGGED(ch, PRF_NOSPAM)) {
			msg_to_char(ch, "You carefully weave %s...\r\n", get_obj_name_by_proto(GET_ACTION_VNUM(ch, 1)));
		}
	}
	else {
		finish_weaving(ch);
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
	else if (!ROOM_BLD_FLAGGED(IN_ROOM(ch), BLD_BATHS) && !ROOM_SECT_FLAGGED(IN_ROOM(ch), SECTF_FRESH_WATER | SECTF_SHALLOW_WATER)) {
		msg_to_char(ch, "You can't bathe here!\r\n");
	}
	else if (!IS_COMPLETE(IN_ROOM(ch))) {
		msg_to_char(ch, "There isn't even any water in the baths yet!\r\n");
	}
	else {
		start_action(ch, ACT_BATHING, 4, 0);

		msg_to_char(ch, "You undress and climb into the water!\r\n");
		act("$n undresses and climbs into the water!", FALSE, ch, 0, 0, TO_ROOM);
	}
}


ACMD(do_chip) {
	obj_data *target;
	int chip_timer = config_get_int("chip_timer");

	one_argument(argument, arg);

	if (GET_ACTION(ch) == ACT_CHIPPING) {
		msg_to_char(ch, "You stop chipping it.\r\n");
		act("$n stops chipping.", TRUE, ch, NULL, NULL, TO_ROOM);
		cancel_action(ch);
	}
	else if (GET_ACTION(ch) != ACT_NONE) {
		msg_to_char(ch, "You're already busy doing something else.\r\n");
	}
	else if (!*arg) {
		msg_to_char(ch, "Chip what?\r\n");
	}
	else if (!(target = get_obj_in_list_vis(ch, arg, ch->carrying))) {
		msg_to_char(ch, "You don't seem to have a %s.\r\n", arg);
	}
	else if (GET_OBJ_VNUM(target) != o_ROCK && GET_OBJ_VNUM(target) != o_CHIPPED && GET_OBJ_VNUM(target) != o_HANDAXE) {
		msg_to_char(ch, "You can't chip that!\r\n");
	}
	else if (!find_chip_weapon(ch)) {
		msg_to_char(ch, "You need to be using some kind of hammer to chip it.\r\n");
	}
	else {
		start_action(ch, ACT_CHIPPING, chip_timer, 0);
		GET_ACTION_VNUM(ch, 0) = GET_OBJ_VNUM(target);
		
		act("You begin to chip at $p.", FALSE, ch, target, 0, TO_CHAR);
		act("$n begins to chip at $p.", TRUE, ch, target, 0, TO_ROOM);
		extract_obj(target);
	}
}


ACMD(do_chop) {
	if (GET_ACTION(ch) == ACT_CHOPPING) {
		send_to_char("You stop chopping the tree.\r\n", ch);
		act("$n stops chopping at the tree.", FALSE, ch, 0, 0, TO_ROOM);
		cancel_action(ch);
	}
	else if (GET_ACTION(ch) != ACT_NONE) {
		send_to_char("You're already busy.\r\n", ch);
	}
	else if (!CAN_CHOP_ROOM(IN_ROOM(ch))) {
		send_to_char("You can't really chop down trees unless you're in the forest.\r\n", ch);
	}
	else if (ROOM_AFF_FLAGGED(IN_ROOM(ch), ROOM_AFF_HAS_INSTANCE)) {
		msg_to_char(ch, "You can't chop here.\r\n");
	}
	else if (!ROOM_AFF_FLAGGED(IN_ROOM(ch), ROOM_AFF_UNCLAIMABLE) && !can_use_room(ch, IN_ROOM(ch), GUESTS_ALLOWED)) {
		msg_to_char(ch, "You don't have permission to chop down trees here.\r\n");
	}
	else if (!ROOM_AFF_FLAGGED(IN_ROOM(ch), ROOM_AFF_UNCLAIMABLE) && !has_permission(ch, PRIV_CHOP)) {
		msg_to_char(ch, "You don't have permission to chop down trees in the empire.\r\n");
	}
	else if (!GET_EQ(ch, WEAR_WIELD)) {
		send_to_char("You need to be wielding some kind of axe to chop.\r\n", ch);
	}
	else if (GET_WEAPON_TYPE(GET_EQ(ch, WEAR_WIELD)) != TYPE_SLICE) {
		send_to_char("You need to be wielding some kind of axe to chop.\r\n", ch);
	}
	else {
		start_chopping(ch);
	}
}


ACMD(do_dig) {
	if (GET_ACTION(ch) == ACT_DIGGING) {
		send_to_char("You stop digging.\r\n", ch);
		act("$n stops digging.", FALSE, ch, 0, 0, TO_ROOM);
		cancel_action(ch);
	}
	else if (GET_ACTION(ch) != ACT_NONE) {
		send_to_char("You're already busy.\r\n", ch);
	}
	else if (!CAN_INTERACT_ROOM(IN_ROOM(ch), INTERACT_DIG)) {
		send_to_char("You can't dig here.\r\n", ch);
	}
	else if (!can_use_room(ch, IN_ROOM(ch), GUESTS_ALLOWED)) {
		msg_to_char(ch, "You don't have permission to dig here.\r\n");
	}
	else {
		start_digging(ch);
	}
}


ACMD(do_excavate) {
	extern bool is_entrance(room_data *room);
	
	struct evolution_data *evo;

	if (GET_ACTION(ch) == ACT_EXCAVATING) {
		msg_to_char(ch, "You stop the excavation.\r\n");
		act("$n stops excavating the trench.", FALSE, ch, 0, 0, TO_ROOM);
		cancel_action(ch);
	}
	else if (ROOM_AFF_FLAGGED(IN_ROOM(ch), ROOM_AFF_HAS_INSTANCE)) {
		// even if already started
		msg_to_char(ch, "You can't excavate here.\r\n");
	}
	else if (GET_ACTION(ch) != ACT_NONE) {
		msg_to_char(ch, "You're already quite busy.\r\n");
	}
	else if (!can_use_room(ch, IN_ROOM(ch), MEMBERS_AND_ALLIES)) {
		// 1st check: allies ok
		msg_to_char(ch, "You don't have permission to excavate here!\r\n");
	}
	else if ((!GET_EQ(ch, WEAR_HOLD) || !OBJ_FLAGGED(GET_EQ(ch, WEAR_HOLD), OBJ_TOOL_SHOVEL)) && (!GET_EQ(ch, WEAR_WIELD) || !OBJ_FLAGGED(GET_EQ(ch, WEAR_WIELD), OBJ_TOOL_SHOVEL))) {
		msg_to_char(ch, "You need a shovel to excavate.\r\n");
	}
	else if (ROOM_SECT_FLAGGED(IN_ROOM(ch), SECTF_IS_TRENCH) && get_room_extra_data(IN_ROOM(ch), ROOM_EXTRA_TRENCH_PROGRESS) < 0) {
		start_action(ch, ACT_EXCAVATING, 0, 0);
		msg_to_char(ch, "You begin to excavate a trench.\r\n");
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
		start_action(ch, ACT_EXCAVATING, 0, 0);
		
		msg_to_char(ch, "You begin to excavate a trench.\r\n");
		act("$n begins excavating a trench.", FALSE, ch, 0, 0, TO_ROOM);

		// Set up the trench
		change_terrain(IN_ROOM(ch), evo->becomes);
		set_room_extra_data(IN_ROOM(ch), ROOM_EXTRA_TRENCH_PROGRESS, config_get_int("trench_initial_value"));
		SET_BIT(ROOM_AFF_FLAGS(IN_ROOM(ch)), ROOM_AFF_PLAYER_MADE);
		SET_BIT(ROOM_BASE_FLAGS(IN_ROOM(ch)), ROOM_AFF_PLAYER_MADE);
	}
}


ACMD(do_fillin) {
	sector_data *old_sect;
	
	if (GET_ACTION(ch) == ACT_FILLING_IN) {
		msg_to_char(ch, "You stop filling in the trench.\r\n");
		act("$n stops filling in the trench.", FALSE, ch, 0, 0, TO_ROOM);
		cancel_action(ch);
	}
	else if (GET_ACTION(ch) != ACT_NONE) {
		msg_to_char(ch, "You're already quite busy.\r\n");
	}
	else if (ROOM_AFF_FLAGGED(IN_ROOM(ch), ROOM_AFF_HAS_INSTANCE)) {
		msg_to_char(ch, "You can't do that here.\r\n");
	}
	else if (!can_use_room(ch, IN_ROOM(ch), MEMBERS_AND_ALLIES)) {
		// 1st check: allies can help
		msg_to_char(ch, "You don't have permission to fill in here!\r\n");
	}
	else if ((!GET_EQ(ch, WEAR_HOLD) || !OBJ_FLAGGED(GET_EQ(ch, WEAR_HOLD), OBJ_TOOL_SHOVEL)) && (!GET_EQ(ch, WEAR_WIELD) || !OBJ_FLAGGED(GET_EQ(ch, WEAR_WIELD), OBJ_TOOL_SHOVEL))) {
		msg_to_char(ch, "You need a shovel to do that.\r\n");
	}
	else if (ROOM_SECT_FLAGGED(IN_ROOM(ch), SECTF_IS_TRENCH)) {
		// already a trench -- just fill in
		start_action(ch, ACT_FILLING_IN, 0, 0);
		msg_to_char(ch, "You begin to fill in the trench.\r\n");
		
		// already finished the trench? start it back to -1 (otherwise, just continue)
		if (get_room_extra_data(IN_ROOM(ch), ROOM_EXTRA_TRENCH_PROGRESS) >= 0) {
			set_room_extra_data(IN_ROOM(ch), ROOM_EXTRA_TRENCH_PROGRESS, -1);
		}
	}
	else if (!(old_sect = reverse_lookup_evolution_for_sector(SECT(IN_ROOM(ch)), EVO_TRENCH_FULL))) {
		// anything to reverse it to?
		msg_to_char(ch, "You can't fill anything in here.\r\n");
	}
	else if (!ROOM_AFF_FLAGGED(IN_ROOM(ch), ROOM_AFF_PLAYER_MADE)) {
		msg_to_char(ch, "You can only fill in a tile that was made by excavation, not a natural one.\r\n");
	}
	else if (!can_use_room(ch, IN_ROOM(ch), MEMBERS_ONLY)) {
		// 2nd check: members only to start new fillin
		msg_to_char(ch, "You don't have permission to fill in here!\r\n");
	}
	else {
		start_action(ch, ACT_FILLING_IN, 0, 0);
		msg_to_char(ch, "You block off the water and begin to fill in the trench.\r\n");

		// set it up
		change_terrain(IN_ROOM(ch), GET_SECT_VNUM(old_sect));
		set_room_extra_data(IN_ROOM(ch), ROOM_EXTRA_TRENCH_PROGRESS, -1);
	}
}


ACMD(do_gather) {
	int gather_base_timer = config_get_int("gather_base_timer");
	
	if (GET_ACTION(ch) == ACT_GATHERING) {
		send_to_char("You stop searching for sticks.\r\n", ch);
		act("$n stops looking around.", TRUE, ch, 0, 0, TO_ROOM);
		cancel_action(ch);
	}
	else if (GET_ACTION(ch) != ACT_NONE) {
		send_to_char("You're already busy.\r\n", ch);
	}
	else if (!CAN_INTERACT_ROOM(IN_ROOM(ch), INTERACT_GATHER)) {
		send_to_char("You can't really gather anything useful here.\r\n", ch);
	}
	else if (!can_use_room(ch, IN_ROOM(ch), GUESTS_ALLOWED)) {
		msg_to_char(ch, "You don't have permission to gather here.\r\n");
	}
	else {
		start_action(ch, ACT_GATHERING, gather_base_timer / (HAS_ABILITY(ch, ABIL_FINDER) ? 2 : 1), 0);
		
		send_to_char("You begin looking around for sticks.\r\n", ch);
	}
}


ACMD(do_harvest) {
	int harvest_timer = config_get_int("harvest_timer");
	
	if (IS_NPC(ch)) {
		msg_to_char(ch, "You can't do that.\r\n");
	}
	else if (GET_ACTION(ch) == ACT_HARVESTING) {
		msg_to_char(ch, "You stop harvesting the %s.\r\n", GET_CROP_NAME(crop_proto(ROOM_CROP_TYPE(IN_ROOM(ch)))));
		act("$n stops harvesting.\r\n", FALSE, ch, 0, 0, TO_ROOM);
		cancel_action(ch);
	}
	else if (ROOM_AFF_FLAGGED(IN_ROOM(ch), ROOM_AFF_HAS_INSTANCE)) {
		msg_to_char(ch, "You can't harvest here.\r\n");
	}
	else if (GET_ACTION(ch) != ACT_NONE) {
		msg_to_char(ch, "You already busy doing something else.\r\n");
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
	else if (!ROOM_AFF_FLAGGED(IN_ROOM(ch), ROOM_AFF_UNCLAIMABLE) && !has_permission(ch, PRIV_HARVEST)) {
		msg_to_char(ch, "You don't have permission to harvest empire crops.\r\n");
	}
	else if (!GET_EQ(ch, WEAR_WIELD) || GET_OBJ_TYPE(GET_EQ(ch, WEAR_WIELD)) != ITEM_WEAPON || (GET_WEAPON_TYPE(GET_EQ(ch, WEAR_WIELD)) != TYPE_SLICE && GET_WEAPON_TYPE(GET_EQ(ch, WEAR_WIELD)) != TYPE_SLASH)) {
		msg_to_char(ch, "You aren't using the proper tool for that.\r\n");
	}
	else {
		start_action(ch, ACT_HARVESTING, 0, 0);

		// ensure progress is set up
		if (get_room_extra_data(IN_ROOM(ch), ROOM_EXTRA_HARVEST_PROGRESS) == 0) {
			set_room_extra_data(IN_ROOM(ch), ROOM_EXTRA_HARVEST_PROGRESS, harvest_timer);
		}
		
		msg_to_char(ch, "You begin harvesting the %s.\r\n", GET_CROP_NAME(crop_proto(ROOM_CROP_TYPE(IN_ROOM(ch)))));
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
	else if (GET_ACTION(ch) != ACT_NONE) {
		msg_to_char(ch, "You're busy doing something else right now.\r\n");
	}
	else if (!ROOM_BLD_FLAGGED(IN_ROOM(ch), BLD_MINE)) {
		msg_to_char(ch, "This isn't a mine..\r\n");
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
		msg_to_char(ch, "You don't have a tool suitable for mining.\r\n");
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
	}
	else if (GET_ACTION(ch) != ACT_NONE) {
		msg_to_char(ch, "You're busy doing something else right now.\r\n");
	}
	else if (!ROOM_BLD_FLAGGED(IN_ROOM(ch), BLD_MINT)) {
		msg_to_char(ch, "You can't mint anything here.\r\n");
	}
	else if (!IS_COMPLETE(IN_ROOM(ch))) {
		msg_to_char(ch, "You need to finish building first.\r\n");
	}
	else if (!(emp = ROOM_OWNER(IN_ROOM(ch)))) {
		msg_to_char(ch, "This mint does not belong to any empire, and can't make coins.\r\n");
	}
	else if (!EMPIRE_HAS_TECH(emp, TECH_COMMERCE)) {
		msg_to_char(ch, "This empire does not have Commerce, and cannot mint coins.\r\n");
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
	else {
		act("You melt down $p and pour it into the coin mill...", FALSE, ch, obj, NULL, TO_CHAR);
		act("$n melts down $p and pours it into the coin mill...", FALSE, ch, obj, NULL, TO_ROOM);
		
		start_action(ch, ACT_MINTING, 2 * GET_WEALTH_VALUE(obj), 0);
		GET_ACTION_VNUM(ch, 0) = GET_OBJ_VNUM(obj);
		GET_ACTION_VNUM(ch, 1) = GET_WEALTH_VALUE(obj) * (1/COIN_VALUE);
		extract_obj(obj);
	}
}


ACMD(do_pan) {
	if (IS_NPC(ch)) {
		msg_to_char(ch, "You can't do that.\r\n");
	}
	else if (GET_ACTION(ch) == ACT_PANNING) {
		msg_to_char(ch, "You stop panning for gold.\r\n");
		cancel_action(ch);
	}
	else if (GET_ACTION(ch) != ACT_NONE) {
		msg_to_char(ch, "You're a bit busy right now.\r\n");
	}
	else if (!find_flagged_sect_within_distance_from_char(ch, SECTF_FRESH_WATER, NOBITS, 1)) {
		msg_to_char(ch, "You need to be near fresh water to pan for gold.\r\n");
	}
	else if (!GET_EQ(ch, WEAR_HOLD) || !OBJ_FLAGGED(GET_EQ(ch, WEAR_HOLD), OBJ_TOOL_PAN)) {
		msg_to_char(ch, "You need to be holding a pan to do that.\r\n");
	}
	else {
		start_panning(ch);
	}
}


ACMD(do_pick) {
	if (IS_NPC(ch)) {
		msg_to_char(ch, "NPCs cannot pick.\r\n");
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
	else {
		start_picking(ch);
	}
}


ACMD(do_plant) {
	extern const char *climate_types[];
	
	struct evolution_data *evo;
	obj_data *obj;
	crop_data *cp;
	
	int planting_base_timer = config_get_int("planting_base_timer");

	one_argument(argument, arg);

	if (IS_NPC(ch)) {
		msg_to_char(ch, "You can't plant.\r\n");
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
	else if (!ROOM_AFF_FLAGGED(IN_ROOM(ch), ROOM_AFF_UNCLAIMABLE) && !has_permission(ch, PRIV_HARVEST)) {
		msg_to_char(ch, "You don't have permission to plant crops in the empire.\r\n");
	}
	else if (!*arg) {
		msg_to_char(ch, "What do you want to plant?\r\n");
	}
	else if (!(obj = get_obj_in_list_vis(ch, arg, ch->carrying))) {
		msg_to_char(ch, "You don't seem to have any %s.\r\n", arg);
	}
	else if (!OBJ_FLAGGED(obj, OBJ_PLANTABLE)) {
		msg_to_char(ch, "You can't plant that!\r\n");
	}
	else if (!(cp = crop_proto(GET_OBJ_VAL(obj, VAL_FOOD_CROP_TYPE)))) {
		// this is a sanity check for bad crop values
		msg_to_char(ch, "You can't plant that!\r\n");
	}
	else if (GET_SECT_CLIMATE(SECT(IN_ROOM(ch))) != GET_CROP_CLIMATE(cp)) {
		strcpy(buf, climate_types[GET_CROP_CLIMATE(cp)]);
		msg_to_char(ch, "You can only plant that in %s areas.\r\n", strtolower(buf));
	}
	else if (CROP_FLAGGED(cp, CROPF_REQUIRES_WATER) && !find_flagged_sect_within_distance_from_char(ch, SECTF_FRESH_WATER, NOBITS, config_get_int("water_crop_distance"))) {
		msg_to_char(ch, "You must plant that closer to fresh water.\r\n");
	}
	else if (!(evo = get_evolution_by_type(SECT(IN_ROOM(ch)), EVO_PLANTS_TO))) {
		msg_to_char(ch, "Nothing can be planted here.\r\n");
	}
	else {
		change_terrain(IN_ROOM(ch), evo->becomes);
		
		// don't use GET_FOOD_CROP_TYPE because not all plantables are food
		set_room_extra_data(IN_ROOM(ch), ROOM_EXTRA_CROP_TYPE, GET_OBJ_VAL(obj, VAL_FOOD_CROP_TYPE));
		set_room_extra_data(IN_ROOM(ch), ROOM_EXTRA_SEED_TIME, planting_base_timer);
		
		extract_obj(obj);
		
		start_action(ch, ACT_PLANTING, 4, 0);
		
		msg_to_char(ch, "You kneel and begin digging holes to plant %s here.\r\n", GET_CROP_NAME(cp));
		sprintf(buf, "$n kneels and begins to plant %s here.", GET_CROP_NAME(cp));
		act(buf, FALSE, ch, 0, 0, TO_ROOM);
	}
}


ACMD(do_play) {
	obj_data *obj;

	if (GET_ACTION(ch) == ACT_MUSIC) {
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
	else if (!has_custom_message(obj, OBJ_CUSTOM_INSTRUMENT_TO_CHAR) || !has_custom_message(obj, OBJ_CUSTOM_INSTRUMENT_TO_ROOM)) {
		msg_to_char(ch, "This instrument can't be played.\r\n");
	}
	else {
		start_action(ch, ACT_MUSIC, 0, ACT_ANYWHERE);
		
		act("You begin to play $p.", FALSE, ch, obj, 0, TO_CHAR);
		act("$n begins to play $p.", FALSE, ch, obj, 0, TO_ROOM);
	}
}


ACMD(do_prospect) {
	if (GET_ACTION(ch) == ACT_PROSPECTING) {
		send_to_char("You stop prospecting.\r\n", ch);
		act("$n stops prospecting.", FALSE, ch, 0, 0, TO_ROOM);
		cancel_action(ch);
	}
	else if (IS_NPC(ch) || !HAS_ABILITY(ch, ABIL_PROSPECT)) {
		msg_to_char(ch, "You need to buy the Prospect ability before you can use it.\r\n");
	}
	else if (GET_ACTION(ch) != ACT_NONE) {
		send_to_char("You're already busy.\r\n", ch);
	}
	else if (!ROOM_CAN_MINE(IN_ROOM(ch))) {
		send_to_char("You can't prospect here.\r\n", ch);
	}
	else if (ABILITY_TRIGGERS(ch, NULL, NULL, ABIL_PROSPECT)) {
		return;
	}
	else {
		start_action(ch, ACT_PROSPECTING, 6, 0);

		send_to_char("You begin prospecting...\r\n", ch);
		act("$n begins prospecting...", TRUE, ch, 0, 0, TO_ROOM);
	}	
}


ACMD(do_quarry) {
	if (GET_ACTION(ch) == ACT_QUARRYING) {
		send_to_char("You stop quarrying.\r\n", ch);
		act("$n stops quarrying.", FALSE, ch, 0, 0, TO_ROOM);
		cancel_action(ch);
	}
	else if (GET_ACTION(ch) != ACT_NONE) {
		send_to_char("You're already busy.\r\n", ch);
	}
	else if (BUILDING_VNUM(IN_ROOM(ch)) != BUILDING_QUARRY) {
		send_to_char("You can't quarry here.\r\n", ch);
	}
	else if (!can_use_room(ch, IN_ROOM(ch), GUESTS_ALLOWED)) {
		msg_to_char(ch, "You don't have permission to quarry here.\r\n");
	}
	else {
		start_quarrying(ch);
	}
}


ACMD(do_saw) {
	obj_data *obj;

	one_argument(argument, arg);

	if (GET_ACTION(ch) == ACT_SAWING) {
		act("You stop sawing.", FALSE, ch, NULL, NULL, TO_CHAR);
		cancel_action(ch);
	}
	else if (BUILDING_VNUM(IN_ROOM(ch)) != BUILDING_LUMBER_YARD || !IS_COMPLETE(IN_ROOM(ch))) {
		msg_to_char(ch, "You can only saw lumber in a lumber yard.\r\n");
	}
	else if (!can_use_room(ch, IN_ROOM(ch), GUESTS_ALLOWED)) {
		msg_to_char(ch, "You don't have permission to saw lumber here.\r\n");
	}
	else if (GET_ACTION(ch) != ACT_NONE)
		msg_to_char(ch, "You're already busy doing something else.\r\n");
	else if (!*arg) {
		msg_to_char(ch, "Saw what?\r\n");
	}
	else if (!(obj = get_obj_in_list_vis(ch, arg, ch->carrying)) && !(obj = get_obj_in_list_vis(ch, arg, ROOM_CONTENTS(IN_ROOM(ch))))) {
		msg_to_char(ch, "You don't seem to have a %s.\r\n", arg);
	}
	else if (GET_OBJ_VNUM(obj) != o_TREE && GET_OBJ_VNUM(obj) != o_LOG) {
		msg_to_char(ch, "You can't saw that!\r\n");
	}
	else {
		start_action(ch, ACT_SAWING, 8, 0);
		GET_ACTION_VNUM(ch, 0) = GET_OBJ_VNUM(obj);

		act("You begin sawing $p.", FALSE, ch, obj, 0, TO_CHAR);
		act("$n begins sawing $p.", TRUE, ch, obj, 0, TO_ROOM);
		extract_obj(obj);
	}
}


ACMD(do_scrape) {
	obj_data *obj, *weapon;

	one_argument(argument, arg);

	if (GET_ACTION(ch) == ACT_SCRAPING) {
		act("You stop scraping.", FALSE, ch, NULL, NULL, TO_CHAR);
		cancel_action(ch);
	}
	else if (GET_ACTION(ch) != ACT_NONE) {
		msg_to_char(ch, "You're already busy doing something else.\r\n");
	}
	else if (!*arg) {
		msg_to_char(ch, "Scrape what?\r\n");
	}
	else if (!(obj = get_obj_in_list_vis(ch, arg, ch->carrying))) {
		msg_to_char(ch, "You don't seem to have a %s.\r\n", arg);
	}
	else if (GET_OBJ_VNUM(obj) != o_TREE) {
		msg_to_char(ch, "You can't scrape that!\r\n");
	}
	else if (!(weapon = has_sharp_tool(ch))) {
		msg_to_char(ch, "You need to be using a sharp tool to scrape anything.\r\n");
	}
	else {
		start_action(ch, ACT_SCRAPING, 6, 0);

		act("You begin scraping $p.", FALSE, ch, obj, 0, TO_CHAR);
		act("$n begins scraping $p.", TRUE, ch, obj, 0, TO_ROOM);
		extract_obj(obj);
	}
}


ACMD(do_smelt) {
	int smelt_timer = config_get_int("smelt_timer");
	
	obj_data *obj[8], *obj_iter;
	int iter, type = NOTHING, num_found;
	
	for (iter = 0; iter < 8; ++iter) {
		obj[iter] = NULL;
	}

	one_argument(argument, arg);

	if (IS_NPC(ch)) {
		msg_to_char(ch, "You can't smelt.\r\n");
	}
	else if (!*arg) {
		msg_to_char(ch, "What would you like to smelt?\r\n");
	}
	else if (GET_ACTION(ch) != ACT_NONE) {
		msg_to_char(ch, "You're busy right now.\r\n");
	}
	else if (!(obj[0] = get_obj_in_list_vis(ch, arg, ch->carrying))) {
		msg_to_char(ch, "You don't seem to have more to smelt.\r\n");
	}
	else if ((!ROOM_BLD_FLAGGED(IN_ROOM(ch), BLD_FORGE) && BUILDING_VNUM(IN_ROOM(ch)) != BUILDING_FOUNDRY) || !IS_COMPLETE(IN_ROOM(ch))) {
		msg_to_char(ch, "You need to be in a forge or foundry to do this.\r\n");
	}
	else if (!can_use_room(ch, IN_ROOM(ch), GUESTS_ALLOWED)) {
		msg_to_char(ch, "You don't have permission to smelt here.\r\n");
	}
	else {
		/* Make sure it's in the list */
		for (iter = 0; smelt_data[iter].from != NOTHING && type == NOTHING; ++iter) {
			if (smelt_data[iter].from == GET_OBJ_VNUM(obj[0])) {
				type = iter;
			}
		}
		
		if (type == NOTHING) {
			msg_to_char(ch, "You can't smelt that.\r\n");
		}
		else {
			num_found = 1;
			
			// start at 1 to find some more of the "from" obj (we already have the first one)
			for (iter = 1; iter < smelt_data[type].from_amt && obj[iter-1]; ++iter) {
				for (obj_iter = obj[iter-1]->next_content; obj_iter && num_found <= iter; obj_iter = obj_iter->next_content) {
					if (GET_OBJ_VNUM(obj_iter) == smelt_data[type].from) {
						obj[iter] = obj_iter;
						num_found = iter+1;
					}
				}
			}

			// got enough?
			if (num_found < smelt_data[type].from_amt) {
				msg_to_char(ch, "You'll need %d of them to smelt.\r\n", smelt_data[type].from_amt);
			}
			else {
				start_action(ch, ACT_MELTING, smelt_timer, 0);
				GET_ACTION_VNUM(ch, 0) = smelt_data[type].from;
				
				sprintf(buf, "You begin to smelt $p");
				if (num_found > 1) {
					sprintf(buf + strlen(buf), " (x%d).", num_found);
				}
				else {
					strcat(buf, ".");
				}
				
				act(buf, FALSE, ch, obj[0], 0, TO_CHAR);
				act("$n begins to smelt $p.", FALSE, ch, obj[0], 0, TO_ROOM);

				// extract
				for (iter = 0; iter < num_found; ++iter) {
					if (obj[iter]) {
						extract_obj(obj[iter]);
					}
				}
			}
		}
	}
}


ACMD(do_stop) {
	void cancel_action(char_data *ch);
	ACMD(do_bite);
	
	if (IS_NPC(ch)) {
		msg_to_char(ch, "No, you stop.\r\n");
	}
	else if (GET_FEEDING_FROM(ch)) {
		do_bite(ch, "", 0, 0);
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
	int iter, type;
	
	int tan_timer = config_get_int("tan_timer");

	one_argument(argument, arg);

	if (IS_NPC(ch)) {
		msg_to_char(ch, "You can't tan.\r\n");
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
	else if (!can_use_room(ch, IN_ROOM(ch), GUESTS_ALLOWED)) {
		msg_to_char(ch, "You don't have permission to tan here.\r\n");
	}
	else {
		/* Make sure it's in the list */
		for (iter = 0, type = NOTHING; type == NOTHING && tan_data[iter].from != NOTHING; ++iter) {
			if (tan_data[iter].from == GET_OBJ_VNUM(obj)) {
				type = iter;
			}
		}
		
		if (type == NOTHING) {
			act("You can't tan $p.", FALSE, ch, obj, NULL, TO_CHAR);
			return;
		}

		start_action(ch, ACT_TANNING, tan_timer, 0);
		GET_ACTION_VNUM(ch, 0) = GET_OBJ_VNUM(obj);
		act("You begin to tan $p.", FALSE, ch, obj, 0, TO_CHAR);
		act("$n begins to tan $p.", FALSE, ch, obj, 0, TO_ROOM);

		extract_obj(obj);
	}
}
