/* ************************************************************************
*   File: dg_mobcmd.c                                     EmpireMUD 2.0b5 *
*  Usage: Script-related commands for mobs                                *
*                                                                         *
*  DG Scripts code came with the attributions in the next two blocks      *
*  EmpireMUD code base by Paul Clarke, (C) 2000-2024                      *
*  All rights reserved.  See license.doc for complete information.        *
*                                                                         *
*  EmpireMUD based upon CircleMUD 3.0, bpl 17, by Jeremy Elson.           *
*  Death's Gate MUD is based on CircleMUD, Copyright (C) 1993, 94.        *
*  CircleMUD (C) 1993, 94 by the Trustees of the Johns Hopkins University *
*  CircleMUD is based on DikuMUD, Copyright (C) 1990, 1991.               *
************************************************************************ */
/***************************************************************************
*  Original Diku Mud copyright (C) 1990, 1991 by Sebastian Hammer,        *
*  Michael Seifert, Hans Henrik St{rfeldt, Tom Madsen, and Katja Nyboe.   *
*                                                                         *
*  Merc Diku Mud improvments copyright (C) 1992, 1993 by Michael          *
*  Chastain, Michael Quan, and Mitchell Tse.                              *
*                                                                         *
*  In order to use any part of this Merc Diku Mud, you must comply with   *
*  both the original Diku license in 'license.doc' as well the Merc       *
*  license in 'license.txt'.  In particular, you may not remove either of *
*  these copyright notices.                                               *
*                                                                         *
*  Much time and thought has gone into this software and you are          *
*  benefitting.  We hope that you share your changes too.  What goes      *
*  around, comes around.                                                  *
***************************************************************************/

/***************************************************************************
*  The MOBprograms have been contributed by N'Atas-ha.  Any support for   *
*  these routines should not be expected from Merc Industries.  However,  *
*  under no circumstances should the blame for bugs, etc be placed on     *
*  Merc Industries.  They are not guaranteed to work on all systems due   *
*  to their frequent use of strxxx functions.  They are also not the most *
*  efficient way to perform their tasks, but hopefully should be in the   *
*  easiest possible way to install and begin using. Documentation for     *
*  such installation can be found in INSTALL.  Enjoy........    N'Atas-Ha *
***************************************************************************/

#include "conf.h"
#include "sysdep.h"

#include "structs.h"
#include "dg_scripts.h"
#include "db.h"
#include "utils.h"
#include "handler.h"
#include "interpreter.h"
#include "comm.h"
#include "skills.h"
#include "vnums.h"
#include "constants.h"

/**
* Contents:
*   Helpers
*   Mobile Commands
*/

 //////////////////////////////////////////////////////////////////////////////
//// HELPERS /////////////////////////////////////////////////////////////////

/**
* For scaled damage functions, allows mob flags to contribute.
*
* @param char_data *mob The mob.
* @param double modifier The existing modifier.
* @return double The new damage modifier;
*/
double scale_modifier_by_mob(char_data *mob, double modifier) {
	if (!mob || !IS_NPC(mob)) {
		return modifier;
	}

	if (MOB_FLAGGED(mob, MOB_DPS)) {
		modifier *= 1.25;
	}
	if (MOB_FLAGGED(mob, MOB_HARD)) {
		modifier *= 1.5;
	}
	if (MOB_FLAGGED(mob, MOB_GROUP)) {
		modifier *= 2.0;
	}
	
	return modifier;
}


/* attaches mob's name and vnum to msg and sends it to script_log */
void mob_log(char_data *mob, const char *format, ...) {
	va_list args;
	char output[MAX_STRING_LENGTH];

	snprintf(output, sizeof(output), "Mob (%s, VNum %d):: %s", GET_SHORT(mob), GET_MOB_VNUM(mob), format);

	va_start(args, format);
	script_vlog(output, args);
	va_end(args);
}


/*
** macro to determine if a mob is permitted to use these commands
*/
#define MOB_OR_IMPL(ch) (IS_NPC(ch) && (!(ch)->desc || GET_ACCESS_LEVEL((ch)->desc->original) >= LVL_CIMPL))


 //////////////////////////////////////////////////////////////////////////////
//// MOBILE COMMANDS /////////////////////////////////////////////////////////

ACMD(do_madventurecomplete) {
	struct instance_data *inst;
	
	if (!MOB_OR_IMPL(ch)) {
		send_config_msg(ch, "huh_string");
		return;
	}

	if (AFF_FLAGGED(ch, AFF_ORDERED)) {
		return;
	}
	
	if ((inst = quest_instance_global) || (inst = get_instance_by_mob(ch))) {
		mark_instance_completed(inst);
	}
}


// attempts to aggro the player, his party, or any PC present (in order)
ACMD(do_maggro) {
	char_data *iter, *next_iter, *victim = NULL;
	char arg[MAX_INPUT_LENGTH];

	if (!MOB_OR_IMPL(ch) || AFF_FLAGGED(ch, AFF_ORDERED)) {
		send_config_msg(ch, "huh_string");
		return;
	}
	
	// argument is optional (preferred target)
	one_argument(argument, arg);
	if (*arg) {
		if (*arg == UID_CHAR) {
			if (!(victim = get_char(arg))) {
				// no victim: ok, just fall through to broad aggro
				// mob_log(ch, "maggro: victim (%s) not found", arg);
				// return;
			}
		}
		else if (!(victim = get_char_room_vis(ch, arg, NULL))) {
			// no victim: ok, just fall through to broad aggro
			// mob_log(ch, "maggro: victim (%s) not found",arg);
			// return;
		}
		
		// if we got a victim (optional), check them:
		if (victim) {
			if (victim == ch) {
				mob_log(ch, "maggro: victim is self");
				return;
			}
			if (IN_ROOM(victim) != IN_ROOM(ch)) {
				mob_log(ch, "maggro: victim is in wrong room");
				return;
			}
			if (!valid_dg_target(victim, DG_ALLOW_GODS)) {
				mob_log(ch, "maggro: target is invalid");
				return;
			}
		}
	}
	
	// stand if needed
	if (GET_POS(ch) < POS_FIGHTING) {
		GET_POS(ch) = POS_STANDING;
	}
	
	// attempt victim first
	if (victim && can_fight(ch, victim)) {
		hit(ch, victim, GET_EQ(ch, WEAR_WIELD), TRUE);
		
		// ensure hitting the right person (in this case only)
		if (victim && !EXTRACTED(victim) && !IS_DEAD(victim) && FIGHTING(ch) && FIGHTING(ch) != victim) {
			FIGHTING(ch) = victim;
		}
		return;
	}
	
	// if we got here, we missed the victim -- look for his allies
	DL_FOREACH_SAFE2(ROOM_PEOPLE(IN_ROOM(ch)), iter, next_iter, next_in_room) {
		if (iter == ch || is_fight_ally(ch, iter)) {
			continue;
		}
		if (victim && !is_fight_ally(victim, iter)) {
			continue;	// only looking for allies
		}
		if (IS_NPC(iter) && victim) {
			continue;	// not hitting mobs here (unless they are allies)
		}
		if (!can_fight(ch, iter)) {
			continue;
		}
		
		// success!
		hit(ch, iter, GET_EQ(ch, WEAR_WIELD), TRUE);
		return;
	}
	
	// okay, look for a broader target
	DL_FOREACH_SAFE2(ROOM_PEOPLE(IN_ROOM(ch)), iter, next_iter, next_in_room) {
		if (iter == ch || is_fight_ally(ch, iter)) {
			continue;
		}
		if (!can_fight(ch, iter)) {
			continue;
		}
		
		// success!
		hit(ch, iter, GET_EQ(ch, WEAR_WIELD), TRUE);
		return;
	}
}


/* prints the argument to all the rooms around the mobile */
ACMD(do_masound) {
	struct room_direction_data *ex;
	room_data *to_room, *room = IN_ROOM(ch);
	bool use_queue, map_echo = FALSE;
	int dir;

	if (!MOB_OR_IMPL(ch)) {
		send_config_msg(ch, "huh_string");
		return;
	}

	if (AFF_FLAGGED(ch, AFF_ORDERED))
		return;

	use_queue = script_message_should_queue(&argument);
	
	if (!*argument) {
		mob_log(ch, "masound called with no argument");
		return;
	}

	if (GET_ROOM_VNUM(room) < MAP_SIZE) {
		for (dir = 0; dir < NUM_2D_DIRS; ++dir) {
			if ((to_room = SHIFT_DIR(room, dir))) {
				sub_write_to_room(argument, to_room, use_queue);
			}
		}
		map_echo = TRUE;
	}

	if (COMPLEX_DATA(room)) {
		LL_FOREACH(COMPLEX_DATA(room)->exits, ex) {
			if ((to_room = ex->room_ptr) && room != to_room) {
				// this skips rooms already hit by the direction shift
				if (!map_echo || GET_ROOM_VNUM(to_room) >= MAP_SIZE) {
					sub_write_to_room(argument, to_room, use_queue);
				}
			}
		}
	}
}


/* lets the mobile kill any player or mobile without murder*/
ACMD(do_mkill) {	
	char arg[MAX_INPUT_LENGTH];
	char_data *victim;

	if (!MOB_OR_IMPL(ch)) {
		send_config_msg(ch, "huh_string");
		return;
	}

	if (AFF_FLAGGED(ch, AFF_ORDERED))
		return;

	one_argument(argument, arg);

	if (!*arg) {
		mob_log(ch, "mkill called with no argument");
		return;
	}

	if (*arg == UID_CHAR) {
		if (!(victim = get_char(arg))) {
			mob_log(ch, "mkill: victim (%s) not found", arg);
			return;
		}
	}
	else if (!(victim = get_char_room_vis(ch, arg, NULL))) {
		mob_log(ch, "mkill: victim (%s) not found",arg);
		return;
	}

	if (victim == ch) {
		mob_log(ch, "mkill: victim is self");
		return;
	}

	if (!valid_dg_target(victim, DG_ALLOW_GODS)) {
		mob_log(ch, "mkill: target is invalid");
		return;
	}
	
	if (!can_fight(ch, victim)) {
		mob_log(ch, "mkill: !can_fight");
		return;
	}

	// start fight!
	hit(ch, victim, GET_EQ(ch, WEAR_WIELD), TRUE);
	
	// ensure hitting the right person
	if (victim && !EXTRACTED(victim) && !IS_DEAD(victim) && FIGHTING(ch) && FIGHTING(ch) != victim) {
		FIGHTING(ch) = victim;
	}
	
	return;
}


/*
* lets the mobile destroy an object in its inventory
* it can also destroy a worn object and it can destroy 
* items using all.xxxxx or just plain all of them
*/
ACMD(do_mjunk) {
	char arg[MAX_INPUT_LENGTH];
	int pos, junk_all = 0;
	obj_data *obj, *obj_next;

	if (!MOB_OR_IMPL(ch)) {
		send_config_msg(ch, "huh_string");
		return;
	}

	if (AFF_FLAGGED(ch, AFF_ORDERED))
		return;

	one_argument(argument, arg);

	if (!*arg) {
		mob_log(ch, "mjunk called with no argument");
		return;
	}

	if (!str_cmp(arg, "all"))
		junk_all = 1;

	if ((find_all_dots(arg) == FIND_INDIV) && !junk_all) { 
		/* Thanks to Carlos Myers for fixing the line below */
		if ((pos = get_obj_pos_in_equip_vis(ch, arg, NULL, ch->equipment)) >= 0) {
			extract_obj(unequip_char(ch, pos));
			return;
		}
		if ((obj = get_obj_in_list_vis(ch, arg, NULL, ch->carrying)) != NULL)
			extract_obj(obj);
		return;
	}
	else {
		DL_FOREACH_SAFE2(ch->carrying, obj, obj_next, next_content) {
			// "all" or "all.name ?
			if (arg[3] == '\0' || isname(arg+4, obj->name)) {
				extract_obj(obj);
			}
		}
		/* Thanks to Carlos Myers for fixing the line below */
		while ((pos = get_obj_pos_in_equip_vis(ch, arg, NULL, ch->equipment)) >= 0)
			extract_obj(unequip_char(ch, pos));
	}
	return;
}


/* prints the message to everyone in the room other than the mob and victim */
ACMD(do_mechoaround) {
	char arg[MAX_INPUT_LENGTH], temp[MAX_INPUT_LENGTH];
	char_data *victim;
	bool use_queue;
	char *p;

	if (!MOB_OR_IMPL(ch)) {
		send_config_msg(ch, "huh_string");
		return;
	}

	if (AFF_FLAGGED(ch, AFF_ORDERED))
		return;
	
	strcpy(temp, argument);	// preserve a copy
	p = one_argument(temp, arg);
	use_queue = script_message_should_queue(&p);

	if (!*arg) {
		mob_log(ch, "mechoaround called with no argument");
		return;
	}

	if (*arg == UID_CHAR) {
		if (!(victim = get_char(arg))) {
			mob_log(ch, "mechoaround: victim (%s) does not exist",arg);
			return;
		}
	}
	else if (!(victim = get_char_room_vis(ch, arg, NULL))) {
		mob_log(ch, "mechoaround: victim (%s) does not exist",arg);
		return;
	}

	sub_write(p, victim, TRUE, TO_ROOM | (use_queue ? TO_QUEUE : 0));
}


// prints the message to everyone except two targets
ACMD(do_mechoneither) {
	char arg1[MAX_INPUT_LENGTH], arg2[MAX_INPUT_LENGTH];
	char_data *vict1, *vict2, *iter;
	bool use_queue;
	char *p;

	if (!MOB_OR_IMPL(ch)) {
		send_config_msg(ch, "huh_string");
		return;
	}

	if (AFF_FLAGGED(ch, AFF_ORDERED))
		return;

	p = two_arguments(argument, arg1, arg2);
	use_queue = script_message_should_queue(&p);

	if (!*arg1 || !*arg2 || !*p) {
		mob_log(ch, "mechoneither called with missing arguments");
		return;
	}

	if (*arg1 == UID_CHAR) {
		if (!(vict1 = get_char(arg1))) {
			mob_log(ch, "mechoneither: vict 1 (%s) does not exist", arg1);
			return;
		}
	}
	else if (!(vict1 = get_char_room_vis(ch, arg1, NULL))) {
		mob_log(ch, "mechoneither: vict 1 (%s) does not exist", arg1);
		return;
	}

	if (*arg2 == UID_CHAR) {
		if (!(vict2 = get_char(arg2))) {
			mob_log(ch, "mechoneither: vict 2 (%s) does not exist", arg2);
			return;
		}
	}
	else if (!(vict2 = get_char_room_vis(ch, arg2, NULL))) {
		mob_log(ch, "mechoneither: vict 2 (%s) does not exist", arg2);
		return;
	}
	
	DL_FOREACH2(ROOM_PEOPLE(IN_ROOM(vict1)), iter, next_in_room) {
		if (iter->desc && iter != vict1 && iter != vict2) {
			sub_write(p, iter, TRUE, TO_CHAR | (use_queue ? TO_QUEUE : 0));
		}
	}
}


/* sends the message to only the victim */
ACMD(do_msend) {
	char arg[MAX_INPUT_LENGTH], temp[MAX_INPUT_LENGTH];
	char_data *victim;
	bool use_queue;
	char *p;

	if (!MOB_OR_IMPL(ch)) {
		send_config_msg(ch, "huh_string");
		return;
	}

	if (AFF_FLAGGED(ch, AFF_ORDERED))
		return;
	
	strcpy(temp, argument);	// preserve this
	p = one_argument(temp, arg);
	use_queue = script_message_should_queue(&p);

	if (!*arg) {
		mob_log(ch, "msend called with no argument");
		return;
	}

	if (*arg == UID_CHAR) {
		if (!(victim = get_char(arg))) {
			mob_log(ch, "msend: victim (%s) does not exist",arg);
			return;
		}
	}
	else if (!(victim = get_char_room_vis(ch, arg, NULL))) {
		mob_log(ch, "msend: victim (%s) does not exist",arg);
		return;
	}

	sub_write(p, victim, TRUE, TO_CHAR | TO_SLEEP | (use_queue ? TO_QUEUE : 0));
}


/* prints the message to the room at large */
ACMD(do_mecho) {
	bool use_queue;
	char *p;

	if (!MOB_OR_IMPL(ch)) {
		send_config_msg(ch, "huh_string");
		return;
	}

	if (AFF_FLAGGED(ch, AFF_ORDERED))
		return;

	if (!*argument) {
		mob_log(ch, "mecho called with no arguments");
		return;
	}
	p = argument;
	use_queue = script_message_should_queue(&p);

	sub_write(p, ch, TRUE, TO_ROOM | (use_queue ? TO_QUEUE : 0));
}


ACMD(do_mbuildingecho) {
	room_data *home_room;
	char room_number[MAX_INPUT_LENGTH], *msg;
	char_data *iter;
	bool use_queue;

	if (!MOB_OR_IMPL(ch)) {
		send_config_msg(ch, "huh_string");
		return;
	}

	if (AFF_FLAGGED(ch, AFF_ORDERED))
		return;

	msg = any_one_word(argument, room_number);
	use_queue = script_message_should_queue(&msg);

	if (!*room_number || !*msg) {
		mob_log(ch, "mbuildingecho called with too few args");
	}
	else if (!(home_room = find_target_room(ch, room_number))) {
		mob_log(ch, "mbuildingecho called with invalid target");
	}
	else {
		home_room = HOME_ROOM(home_room);	// right?
		
		DL_FOREACH(character_list, iter) {
			if (HOME_ROOM(IN_ROOM(iter)) == home_room) {
				sub_write(msg, iter, TRUE, TO_CHAR | (use_queue ? TO_QUEUE : 0));
			}
		}
	}
}


ACMD(do_mregionecho) {
	char room_number[MAX_INPUT_LENGTH], radius_arg[MAX_INPUT_LENGTH], *msg;
	bool use_queue, outdoor_only = FALSE;
	room_data *center;
	char_data *targ;
	int radius;

	if (!MOB_OR_IMPL(ch)) {
		send_config_msg(ch, "huh_string");
		return;
	}

	if (AFF_FLAGGED(ch, AFF_ORDERED))
		return;

	argument = any_one_word(argument, room_number);
	msg = one_argument(argument, radius_arg);
	use_queue = script_message_should_queue(&msg);

	if (!*room_number || !*radius_arg || !*msg) {
		mob_log(ch, "mregionecho called with too few args");
	}
	else if (!isdigit(*radius_arg) && *radius_arg != '-') {
		mob_log(ch, "mregionecho called with invalid radius");
	}
	else if (!(center = find_target_room(ch, room_number))) {
		mob_log(ch, "mregionecho called with invalid target");
	}
	else {
		center = GET_MAP_LOC(center) ? real_room(GET_MAP_LOC(center)->vnum) : NULL;
		radius = atoi(radius_arg);
		if (radius < 0) {
			radius = -radius;
			outdoor_only = TRUE;
		}
		
		if (center) {
			DL_FOREACH(character_list, targ) {
				if (!same_subzone(center, IN_ROOM(targ))) {
					continue;
				}
				if (compute_distance(center, IN_ROOM(targ)) > radius) {
					continue;
				}
				if (outdoor_only && !IS_OUTDOORS(targ)) {
					continue;
				}
				
				// send
				sub_write(msg, targ, TRUE, TO_CHAR | (use_queue ? TO_QUEUE : 0));
			}
		}
	}
}


ACMD(do_msubecho) {
	char room_number[MAX_INPUT_LENGTH], *msg;
	bool use_queue;
	room_data *where;
	char_data *targ;

	if (!MOB_OR_IMPL(ch)) {
		send_config_msg(ch, "huh_string");
		return;
	}

	if (AFF_FLAGGED(ch, AFF_ORDERED))
		return;

	msg = any_one_word(argument, room_number);
	use_queue = script_message_should_queue(&msg);

	if (!*room_number || !*msg) {
		mob_log(ch, "msubecho called with too few args");
	}
	else if (!(where = find_target_room(ch, room_number))) {
		mob_log(ch, "msubecho called with invalid target");
	}
	else if (!ROOM_INSTANCE(where)) {
		mob_log(ch, "msubecho called outside an adventure");
	}
	else {
		DL_FOREACH(character_list, targ) {
			if (ROOM_INSTANCE(where) != ROOM_INSTANCE(IN_ROOM(targ))) {
				continue;	// wrong instance
			}
			if (!same_subzone(where, IN_ROOM(targ))) {
				continue;	// wrong subzone
			}
			
			// send
			sub_write(msg, targ, TRUE, TO_CHAR | (use_queue ? TO_QUEUE : 0));
		}
	}
}


ACMD(do_mvehicleecho) {
	char targ[MAX_INPUT_LENGTH], *msg;
	vehicle_data *veh;
	char_data *iter;
	bool use_queue;

	if (!MOB_OR_IMPL(ch) || AFF_FLAGGED(ch, AFF_ORDERED)) {
		send_config_msg(ch, "huh_string");
		return;
	}

	msg = any_one_word(argument, targ);
	use_queue = script_message_should_queue(&msg);
	
	if (!*targ || !*msg) {
		mob_log(ch, "mvehicleecho called with too few args");
	}
	else if ((*targ == UID_CHAR && (veh = get_vehicle(targ))) || (veh = get_vehicle_in_room_vis(ch, targ, NULL))) {
		mob_log(ch, "mvehicleecho called with invalid target");
	}
	else {
		DL_FOREACH(character_list, iter) {
			if (VEH_SITTING_ON(veh) == iter || GET_ROOM_VEHICLE(IN_ROOM(iter)) == veh) {
				sub_write(msg, iter, TRUE, TO_CHAR | (use_queue ? TO_QUEUE : 0));
			}
		}
	}
}


/*
* lets the mobile load an item or mobile.  All items
* are loaded into inventory, unless it is NO-TAKE. 
*/
ACMD(do_mload) {	
	struct instance_data *inst = get_instance_by_mob(ch);
	char arg1[MAX_INPUT_LENGTH], arg2[MAX_INPUT_LENGTH];
	int number = 0;
	room_data *in_room;
	char_data *mob, *tch;
	obj_data *object, *cnt;
	vehicle_data *veh;
	char *target;
	int pos;
	bool ally = FALSE;

	if (!MOB_OR_IMPL(ch)) {
		send_config_msg(ch, "huh_string");
		return;
	}

	if (AFF_FLAGGED(ch, AFF_ORDERED))
		return;

	if (ch->desc && GET_ACCESS_LEVEL(ch->desc->original) < LVL_CIMPL) {
		return;
	}

	target = two_arguments(argument, arg1, arg2);
	skip_spaces(&target);

	if (!*arg1 || !*arg2 || !is_number(arg2) || ((number = atoi(arg2)) < 0)) {
		mob_log(ch, "mload: bad syntax");
		return;
	}

	if (is_abbrev(arg1, "mobile")) {
		if (!mob_proto(number)) {
			mob_log(ch, "mload: bad mob vnum");
			return;
		}
		mob = read_mobile(number, TRUE);
		MOB_INSTANCE_ID(mob) = MOB_INSTANCE_ID(ch);
		if (MOB_INSTANCE_ID(mob) != NOTHING) {
			add_instance_mob(real_instance(MOB_INSTANCE_ID(mob)), GET_MOB_VNUM(mob));
		}
		char_to_room(mob, IN_ROOM(ch));
		setup_generic_npc(mob, GET_LOYALTY(ch), NOTHING, NOTHING);
		
		// check ally request
		if (!strn_cmp(target, "ally", 4)) {
			ally = TRUE;
			target = one_argument(target, arg1);	// clear out the ally
			skip_spaces(&target);
		}
		
		if (*target && isdigit(*target)) {
			// scale to requested level and lock it there
			scale_mob_to_level(mob, atoi(target));
			set_mob_flags(mob, MOB_NO_RESCALE);
		}
		else if (GET_CURRENT_SCALE_LEVEL(ch) > 0) {
			// only scale mob if self is scaled
			scale_mob_to_level(mob, GET_CURRENT_SCALE_LEVEL(ch));
			if (MOB_FLAGGED(ch, MOB_NO_RESCALE)) {
				set_mob_flags(mob, MOB_NO_RESCALE);
			}
		}
		
		if (ally) {
			add_follower(mob, ch, FALSE);
		}
		
		load_mtrigger(mob);
	}
	else if (is_abbrev(arg1, "object")) {
		if (!obj_proto(number)) {
			mob_log(ch, "mload: bad object vnum");
			return;
		}
		object = read_object(number, TRUE);
		
		if (inst) {
			instance_obj_setup(inst, object);
		}
		
		/* special handling to make objects able to load on a person/in a container/worn etc. */
		if (!target || !*target) {
			if (CAN_WEAR(object, ITEM_WEAR_TAKE)) {
				obj_to_char(object, ch);
			}
			else {
				obj_to_room(object, IN_ROOM(ch));
			}
		
			// must scale now
			scale_item_to_level(object, GET_CURRENT_SCALE_LEVEL(ch));
		
			if (load_otrigger(object) && object->carried_by) {
				get_otrigger(object, object->carried_by, FALSE);
			}
			return;
		}
		
		target = two_arguments(target, arg1, arg2); /* recycling ... */
		skip_spaces(&target);
		
		// if they're picking a room, move arg2 down a slot to "target" level
		in_room = NULL;
		if (!str_cmp(arg1, "room")) {
			in_room = IN_ROOM(ch);
			target = arg2;
		}
		else if (*arg1 == UID_CHAR) {
			if ((in_room = get_room(IN_ROOM(ch), arg1))) {
				target = arg2;
			}
		}
		else {	// not targeting a room
			in_room = NULL;
		}
		
		// scale based on remaining "target" arg
		if (*target && isdigit(*target)) {
			scale_item_to_level(object, atoi(target));
		}
		else {
			scale_item_to_level(object, GET_CURRENT_SCALE_LEVEL(ch));
		}
		
		if (in_room) {	// load in the room
			obj_to_room(object, in_room);
			load_otrigger(object);
			return;
		}
		
		tch = (*arg1 == UID_CHAR) ? get_char(arg1) : get_char_room_vis(ch, arg1, NULL);
		if (tch) {	// load on char
			// mark as "gathered" like a resource
			if (!IS_NPC(tch) && GET_LOYALTY(tch)) {
				add_production_total(GET_LOYALTY(tch), GET_OBJ_VNUM(object), 1);
			}
			
			if (*arg2 && (pos = find_eq_pos_script(arg2)) >= 0 && !GET_EQ(tch, pos) && can_wear_on_pos(object, pos)) {
				equip_char(tch, object, pos);
				load_otrigger(object);
				return;
			}
			obj_to_char(object, tch);
			if (load_otrigger(object)) {
				get_otrigger(object, tch, FALSE);
			}
			return;
		}
		
		cnt = (*arg1 == UID_CHAR) ? get_obj(arg1) : get_obj_vis(ch, arg1, NULL);
		if (cnt && (GET_OBJ_TYPE(cnt) == ITEM_CONTAINER || GET_OBJ_TYPE(cnt) == ITEM_CORPSE)) {
			// load in container
			obj_to_obj(object, cnt);
			load_otrigger(object);
			return;
		}

		/* neither char nor container found - just dump it in room */
		obj_to_room(object, IN_ROOM(ch)); 
		load_otrigger(object);
		return;
	}
	else if (is_abbrev(arg1, "vehicle")) {
		if (!vehicle_proto(number)) {
			mob_log(ch, "mload: bad vehicle vnum");
			return;
		}
		veh = read_vehicle(number, TRUE);
		VEH_INSTANCE_ID(veh) = MOB_INSTANCE_ID(ch);
		vehicle_to_room(veh, IN_ROOM(ch));
		
		if (*target && isdigit(*target)) {
			// scale to requested level
			scale_vehicle_to_level(veh, atoi(target));
		}
		else if (GET_CURRENT_SCALE_LEVEL(ch) > 0) {
			// only scale vehicle if self is scaled
			scale_vehicle_to_level(veh, GET_CURRENT_SCALE_LEVEL(ch));
		}
		else {
			// hope to inherit
			scale_vehicle_to_level(veh, 0);
		}
		
		get_vehicle_interior(veh);	// ensure inside is loaded
		
		// ownership
		if (VEH_CLAIMS_WITH_ROOM(veh) && ROOM_OWNER(HOME_ROOM(IN_ROOM(veh)))) {
			perform_claim_vehicle(veh, ROOM_OWNER(HOME_ROOM(IN_ROOM(veh))));
		}
		
		load_vtrigger(veh);
	}
	else
		mob_log(ch, "mload: bad type");
}


ACMD(do_mmod) {
	script_modify(argument);
}


ACMD(do_mmorph) {
	char tar_arg[MAX_INPUT_LENGTH], num_arg[MAX_INPUT_LENGTH];
	morph_data *morph = NULL;
	char_data *vict;
	bool normal;
	
	if (!MOB_OR_IMPL(ch)) {
		send_config_msg(ch, "huh_string");
		return;
	}
	
	two_arguments(argument, tar_arg, num_arg);
	normal = !str_cmp(num_arg, "normal");
	
	if (!*tar_arg || !*num_arg) {
		mob_log(ch, "mmorph: missing argument(s)");
	}
	else if (*tar_arg == UID_CHAR ? !(vict = get_char(tar_arg)) : !(vict = get_char_room_vis(ch, tar_arg, NULL))) {
		mob_log(ch, "mmorph: invalid target '%s'", tar_arg);
	}
	else if (!normal && (!isdigit(*num_arg) || !(morph = morph_proto(atoi(num_arg))))) {
		mob_log(ch, "mmorph: invalid morph '%s'", num_arg);
	}
	else if (morph && MORPH_FLAGGED(morph, MORPHF_IN_DEVELOPMENT)) {
		mob_log(ch, "mmorph: morph %d set in-development", MORPH_VNUM(morph));
	}
	else {
		perform_morph(vict, morph);
	}
}


ACMD(do_mmove) {
	if (!MOB_OR_IMPL(ch)) {
		send_config_msg(ch, "huh_string");
		return;
	}
	
	// things that block moves
	if (MOB_FLAGGED(ch, MOB_SENTINEL | MOB_TIED) || AFF_FLAGGED(ch, AFF_CHARM | AFF_IMMOBILIZED) || GET_POS(ch) != POS_STANDING || (GET_LEADER(ch) && IN_ROOM(ch) == IN_ROOM(GET_LEADER(ch)))) {
		return;
	}
	
	try_mobile_movement(ch);
}


/*
* lets the mobile purge all objects and other npcs in the room,
* or purge a specified object or mob in the room.  It can purge
*  itself, but this will be the last command it does.
*/
ACMD(do_mpurge) {
	char arg[MAX_INPUT_LENGTH];
	vehicle_data *veh;
	char_data *victim;
	obj_data *obj;

	if (!MOB_OR_IMPL(ch)) {
		send_config_msg(ch, "huh_string");
		return;
	}

	if (AFF_FLAGGED(ch, AFF_ORDERED))
		return;

	if (ch->desc && (GET_ACCESS_LEVEL(ch->desc->original) < LVL_CIMPL))
		return;

	argument = one_argument(argument, arg);
	skip_spaces(&argument);

	if (!*arg) {
		/* 'purge' */
		char_data *vnext;
		obj_data *obj_next;
		
		DL_FOREACH_SAFE2(ROOM_PEOPLE(IN_ROOM(ch)), victim, vnext, next_in_room) {
			if (IS_NPC(victim) && victim != ch)
				extract_char(victim);
		}
		
		DL_FOREACH_SAFE2(ROOM_CONTENTS(IN_ROOM(ch)), obj, obj_next, next_content) {
			extract_obj(obj);
		}

		return;
	}
	
	// purge all mobs/objs in an instance
	if (!str_cmp(arg, "instance")) {
		struct instance_data *inst = real_instance(MOB_INSTANCE_ID(ch));
		if (!inst) {
			mob_log(ch, "mpurge: non-instance mob using purge instance");
			return;
		}
		dg_purge_instance(ch, inst, argument);
	}
	// purge mob
	else if ((*arg == UID_CHAR && (victim = get_char(arg))) || (victim = get_char_room_vis(ch, arg, NULL))) {
		if (!IS_NPC(victim)) {
			mob_log(ch, "mpurge: purging a PC");
			return;
		}

		if (*argument) {
			act(argument, TRUE, victim, NULL, NULL, TO_ROOM);
		}
		extract_char(victim);
	}
	// purge vehicle
	else if ((*arg == UID_CHAR && (veh = get_vehicle(arg))) || (veh = get_vehicle_in_room_vis(ch, arg, NULL))) {
		if (*argument) {
			act(argument, TRUE, ROOM_PEOPLE(IN_ROOM(veh)), NULL, veh, TO_CHAR | TO_ROOM | ACT_VEH_VICT);
		}
		extract_vehicle(veh);
	}
	// purge obj
	else if ((*arg == UID_CHAR && (obj = get_obj(arg))) || (obj = get_obj_vis(ch, arg, NULL))) {
		if (*argument) {
			room_data *room = obj_room(obj);
			act(argument, TRUE, room ? ROOM_PEOPLE(room) : NULL, obj, NULL, TO_CHAR | TO_ROOM);
		}
		extract_obj(obj);
	}
	// bad arg
	else {
		mob_log(ch, "mpurge: bad argument");
	}
}


// quest commands
ACMD(do_mquest) {
	if (!MOB_OR_IMPL(ch) || AFF_FLAGGED(ch, AFF_ORDERED)) {
		send_config_msg(ch, "huh_string");
		return;
	}
	
	do_dg_quest(MOB_TRIGGER, ch, argument);
}


/* lets the mobile goto any location it wishes that is not private */
ACMD(do_mgoto) {
	char arg[MAX_INPUT_LENGTH];
	room_data *location;

	if (!MOB_OR_IMPL(ch)) {
		send_config_msg(ch, "huh_string");
		return;
	}

	if (AFF_FLAGGED(ch, AFF_ORDERED))
		return;

	one_word(argument, arg);

	if (!*arg) {
		mob_log(ch, "mgoto called with no argument");
		return;
	}

	if (!(location = find_target_room(ch, arg))) {
		mob_log(ch, "mgoto: invalid location");
		return;
	}

	if (FIGHTING(ch))
		stop_fighting(ch);

	char_from_room(ch);
	char_to_room(ch, location);
	enter_wtrigger(IN_ROOM(ch), ch, NO_DIR, "script");
	msdp_update_room(ch);
}


/* lets the mobile do a command at another location. Very useful */
ACMD(do_mat) {
	char_data *was_fighting = FIGHTING(ch);
	char arg[MAX_INPUT_LENGTH];
	room_data *location = NULL, *original;
	struct instance_data *inst;
	int fmode = FIGHT_MODE(ch);

	if (!MOB_OR_IMPL(ch)) {
		send_config_msg(ch, "huh_string");
		return;
	}

	if (AFF_FLAGGED(ch, AFF_ORDERED))
		return;

	argument = one_word(argument, arg);

	if (!*arg || !*argument) {
		mob_log(ch, "mat: bad argument");
		return;
	}

	// special case for use of i### room-template targeting when mob is outside its instance
	if (arg[0] == 'i' && isdigit(arg[1]) && MOB_INSTANCE_ID(ch) != NOTHING && (inst = real_instance(MOB_INSTANCE_ID(ch))) && INST_START(inst)) {
		// I know that's a lot to check but we want i###-targeting to work when a mob wanders out -pc 4/13/2015
		location = get_room(INST_START(inst), arg);
	}
	else {
		location = get_room(IN_ROOM(ch), arg);
	}

	if (!location) {
		mob_log(ch, "mat: invalid location");
		return;
	}

	original = IN_ROOM(ch);
	char_from_room(ch);
	char_to_room(ch, location);
	command_interpreter(ch, argument);

	/*
	* See if 'ch' still exists before continuing!
	* Handles 'at XXXX quit' case.
	*/
	if (IN_ROOM(ch) == location) {
		char_from_room(ch);
		char_to_room(ch, original);
	}
	
	if (was_fighting && IN_ROOM(was_fighting) == IN_ROOM(ch) && !IS_DEAD(ch) && !EXTRACTED(ch)) {
		set_fighting(ch, was_fighting, fmode);
	}
	
	msdp_update_room(ch);	// once we're sure we're staying
}


ACMD(do_mbuild) {
	char loc_arg[MAX_INPUT_LENGTH], bld_arg[MAX_INPUT_LENGTH], *tmp;
	room_data *target;
	
	if (!MOB_OR_IMPL(ch) || AFF_FLAGGED(ch, AFF_ORDERED)) {
		send_config_msg(ch, "huh_string");
		return;
	}

	tmp = any_one_word(argument, loc_arg);
	strcpy(bld_arg, tmp);
	
	// usage: %build% [location] <vnum [dir] | ruin | demolish>
	if (!*loc_arg) {
		mob_log(ch, "mbuild: bad syntax");
		return;
	}
	
	// check number of args
	if (!*bld_arg) {
		// only arg is actually bld arg
		strcpy(bld_arg, argument);
		target = IN_ROOM(ch);
	}
	else {
		// two arguments
		target = find_target_room(ch, loc_arg);
	}
	
	if (!target) {
		mob_log(ch, "mbuild: target is an invalid room");
		return;
	}
	
	// places you just can't build -- fail silently (currently)
	if (IS_INSIDE(target) || IS_ADVENTURE_ROOM(target) || IS_CITY_CENTER(target)) {
		return;
	}

	// good to go
	do_dg_build(target, bld_arg);
}


ACMD(do_mrestore) {
	struct affected_type *aff, *next_aff;
	char arg[MAX_INPUT_LENGTH];
	vehicle_data *veh = NULL;
	char_data *victim = NULL;
	obj_data *obj = NULL;
	room_data *room = NULL;
	bitvector_t bitv;
	bool done_aff;
	int pos;
	
	if (!MOB_OR_IMPL(ch) || AFF_FLAGGED(ch, AFF_ORDERED) || (ch->desc && (GET_ACCESS_LEVEL(ch->desc->original) < LVL_CIMPL))) {
		send_config_msg(ch, "huh_string");
		return;
	}
	
	one_argument(argument, arg);
	
	// find a target
	if (!*arg) {
		victim = ch;
	}
	else if (!str_cmp(arg, "room") || !str_cmp(arg, "building")) {
		room = IN_ROOM(ch);
	}
	else if ((*arg == UID_CHAR && (victim = get_char(arg))) || (victim = get_char_room_vis(ch, arg, NULL))) {
		// found victim
	}
	else if ((*arg == UID_CHAR && (veh = get_vehicle(arg))) || (veh = get_vehicle_in_room_vis(ch, arg, NULL))) {
		// found vehicle
		if (!VEH_IS_COMPLETE(veh)) {
			mob_log(ch, "mrestore: used on unfinished vehicle");
			return;
		}
	}
	else if ((*arg == UID_CHAR && (obj = get_obj(arg))) || (obj = get_obj_vis(ch, arg, NULL))) {
		// found obj
	}
	else if ((room = find_target_room(ch, arg))) {
		// found room
	}
	else {
		// bad arg
		mob_log(ch, "mrestore: bad argument");
		return;
	}
	
	if (room) {
		room = HOME_ROOM(room);
		if (!IS_COMPLETE(room)) {
			mob_log(ch, "mrestore: used on unfinished building");
			return;
		}
	}
	
	// perform the restoration
	if (victim) {
		while (victim->over_time_effects) {
			dot_remove(victim, victim->over_time_effects);
		}
		LL_FOREACH_SAFE(victim->affected, aff, next_aff) {
			// can't cleanse penalties (things cast by self)
			if (aff->cast_by == CAST_BY_ID(victim)) {
				continue;
			}
			
			done_aff = FALSE;
			if (aff->location != APPLY_NONE && (apply_values[(int) aff->location] == 0.0 || aff->modifier < 0)) {
				affect_remove(victim, aff);
				done_aff = TRUE;
			}
			if (!done_aff && (bitv = aff->bitvector) != NOBITS) {
				// check each bit
				for (pos = 0; bitv && !done_aff; ++pos, bitv >>= 1) {
					if (IS_SET(bitv, BIT(0)) && aff_is_bad[pos]) {
						affect_remove(victim, aff);
						done_aff = TRUE;
					}
				}
			}
		}
		if (GET_POS(victim) < POS_SLEEPING) {
			GET_POS(victim) = POS_STANDING;
		}
		affect_total(victim);
		set_health(victim, GET_MAX_HEALTH(victim));
		set_move(victim, GET_MAX_MOVE(victim));
		set_mana(victim, GET_MAX_MANA(victim));
		set_blood(victim, GET_MAX_BLOOD(victim));
	}
	if (obj) {
		// not sure what to do for objs
	}
	if (veh) {
		remove_vehicle_flags(veh, VEH_ON_FIRE);
		if (!VEH_IS_DISMANTLING(veh)) {
			complete_vehicle(veh);
		}
	}
	if (room) {
		if (COMPLEX_DATA(room)) {
			free_resource_list(GET_BUILDING_RESOURCES(room));
			GET_BUILDING_RESOURCES(room) = NULL;
			set_room_damage(room, 0);
			set_burn_down_time(room, 0, FALSE);
			request_world_save(GET_ROOM_VNUM(room), WSAVE_ROOM);
		}
	}
}


/*
* lets the mobile transfer people.  the all argument transfers
* everyone in the current room to the specified location
*/
ACMD(do_mteleport) {
	char arg1[MAX_INPUT_LENGTH], arg2[MAX_INPUT_LENGTH];
	room_data *target;
	char_data *vict, *next_ch;
	struct instance_data *inst;
	vehicle_data *veh;
	obj_data *obj, *cont;
	int iter;

	if (!MOB_OR_IMPL(ch)) {
		send_config_msg(ch, "huh_string");
		return;
	}

	if (AFF_FLAGGED(ch, AFF_ORDERED))
		return;

	argument = one_argument(argument, arg1);
	argument = one_word(argument, arg2);

	if (!*arg1 || !*arg2) {
		mob_log(ch, "mteleport: bad syntax");
		return;
	}

	// try to find a target for later -- this can fail
	target = (*arg2 == UID_CHAR ? find_room(atoi(arg2 + 1)) : find_target_room(ch, arg2));

	if (!str_cmp(arg1, "all")) {
		if (!target) {
			mob_log(ch, "mteleport all: target is an invalid room");
			return;
		}
		if (target == IN_ROOM(ch)) {
			mob_log(ch, "mteleport all: target is itself");
			return;
		}
		
		DL_FOREACH_SAFE2(ROOM_PEOPLE(IN_ROOM(ch)), vict, next_ch, next_in_room) {
			if (valid_dg_target(vict, DG_ALLOW_GODS)) {
				GET_LAST_DIR(vict) = NO_DIR;
				char_from_room(vict);
				char_to_room(vict, target);
				enter_wtrigger(IN_ROOM(vict), vict, NO_DIR, "script");
				qt_visit_room(vict, IN_ROOM(vict));
				RESET_LAST_MESSAGED_TEMPERATURE(vict);
				msdp_update_room(vict);
			}
		}
	}
	else if (!str_cmp(arg1, "adventure")) {
		// teleport all players in the adventure
		if (!(inst = real_instance(MOB_INSTANCE_ID(ch)))) {
			mob_log(ch, "mteleport adventure: called on non-adventure mob");
			return;
		}
		if (!target) {
			mob_log(ch, "mteleport: target is an invalid room");
			return;
		}
		
		for (iter = 0; iter < INST_SIZE(inst); ++iter) {
			// only if it's not the target room, or we'd be here all day
			if (INST_ROOM(inst, iter) && INST_ROOM(inst, iter) != target) {
			    DL_FOREACH_SAFE2(ROOM_PEOPLE(INST_ROOM(inst, iter)), vict, next_ch, next_in_room) {
					if (!valid_dg_target(vict, DG_ALLOW_GODS)) {
						continue;
					}
					
					// teleport players and their followers
					if (!IS_NPC(vict) || (GET_LEADER(vict) && !IS_NPC(GET_LEADER(vict)))) {
						char_from_room(vict);
						char_to_room(vict, target);
						GET_LAST_DIR(vict) = NO_DIR;
						enter_wtrigger(IN_ROOM(vict), ch, NO_DIR, "script");
						qt_visit_room(vict, IN_ROOM(vict));
						RESET_LAST_MESSAGED_TEMPERATURE(vict);
						msdp_update_room(vict);
					}
				}
			}
		}
	}
	else {	// single-target teleport
		if ((*arg1 == UID_CHAR && (vict = get_char(arg1))) || (vict = get_char_vis(ch, arg1, NULL, FIND_CHAR_WORLD))) {
			// teleport character
			if (!target) {
				mob_log(ch, "mteleport character: target is an invalid room");
				return;
			}
			if (valid_dg_target(vict, DG_ALLOW_GODS)) {
				GET_LAST_DIR(vict) = NO_DIR;
				char_from_room(vict);
				char_to_room(vict, target);
				enter_wtrigger(IN_ROOM(vict), vict, NO_DIR, "script");
				qt_visit_room(vict, IN_ROOM(vict));
				RESET_LAST_MESSAGED_TEMPERATURE(vict);
				msdp_update_room(vict);
			}
		}
		else if ((*arg1 == UID_CHAR && (veh = get_vehicle(arg1))) || (veh = get_vehicle_in_room_vis(ch, arg1, NULL))) {
			// teleport vehicle
			if (!target) {
				mob_log(ch, "mteleport vehicle: target is an invalid room");
				return;
			}
			vehicle_from_room(veh);
			vehicle_to_room(veh, target);
			entry_vtrigger(veh, "script");
		}
		else if ((*arg1 == UID_CHAR && (obj = get_obj(arg1))) || (obj = get_obj_vis(ch, arg1, NULL))) {
			// teleport object
			if (target) {
				// move obj to room
				obj_to_room(obj, target);
			}
			else if ((*arg2 == UID_CHAR && (vict = get_char(arg2))) || (vict = get_char_vis(ch, arg2, NULL, FIND_CHAR_WORLD))) {
				// move obj to character
				obj_to_char(obj, vict);
			}
			else if ((*arg2 == UID_CHAR && (veh = get_vehicle(arg2))) || (veh = get_vehicle_in_room_vis(ch, arg2, NULL))) {
				// move obj to vehicle
				if (!VEH_FLAGGED(veh, VEH_CONTAINER)) {
					mob_log(ch, "mteleport object: attempting to put an object in a vehicle that's not a container");
					return;
				}
				else {
					obj_to_vehicle(obj, veh);
				}
			}
			else if ((*arg2 == UID_CHAR && (cont = get_obj(arg2))) || (cont = get_obj_vis(ch, arg2, NULL))) {
				if (cont == obj || get_top_object(obj) == cont || get_top_object(cont) == obj) {
					mob_log(ch, "mteleport object: attempting to put an object inside itself");
					return;
				}
				else if (GET_OBJ_TYPE(cont) != ITEM_CONTAINER && GET_OBJ_TYPE(cont) != ITEM_CORPSE) {
					mob_log(ch, "mteleport object: attempting to put an object in something that's not a container");
					return;
				}
				else {
					// move obj into obj
					obj_to_obj(obj, cont);
				}
			}
			else {
				mob_log(ch, "mteleport object: no valid target location found");
			}
		
		}
		else {
			mob_log(ch, "mteleport: victim (%s) does not exist", arg1);
		}
	}
}


ACMD(do_mterracrop) {
	char loc_arg[MAX_INPUT_LENGTH], crop_arg[MAX_INPUT_LENGTH];
	crop_data *crop;
	room_data *target;
	crop_vnum vnum;

	if (!MOB_OR_IMPL(ch) || AFF_FLAGGED(ch, AFF_ORDERED)) {
		send_config_msg(ch, "huh_string");
		return;
	}

	argument = any_one_word(argument, loc_arg);
	any_one_word(argument, crop_arg);
	
	// usage: %terracrop% [location] <crop vnum>
	if (!*loc_arg) {
		mob_log(ch, "mterracrop: bad syntax");
		return;
	}
	
	// check number of args
	if (!*crop_arg) {
		// only arg is actually crop arg
		strcpy(crop_arg, loc_arg);
		target = IN_ROOM(ch);
	}
	else {
		// two arguments
		target = find_target_room(ch, loc_arg);
	}
	
	if (!target) {
		mob_log(ch, "mterracrop: target is an invalid room");
		return;
	}
	
	// places you just can't terracrop -- fail silently (currently)
	if (IS_INSIDE(target) || IS_ADVENTURE_ROOM(target) || IS_CITY_CENTER(target)) {
		return;
	}
	
	if (!isdigit(*crop_arg) || (vnum = atoi(crop_arg)) < 0 || !(crop = crop_proto(vnum))) {
		mob_log(ch, "mterracrop: invalid crop vnum");
		return;
	}

	// good to go
	do_dg_terracrop(target, crop);
}


ACMD(do_mterraform) {
	char loc_arg[MAX_INPUT_LENGTH], sect_arg[MAX_INPUT_LENGTH];
	sector_data *sect;
	room_data *target;
	sector_vnum vnum;

	if (!MOB_OR_IMPL(ch) || AFF_FLAGGED(ch, AFF_ORDERED)) {
		send_config_msg(ch, "huh_string");
		return;
	}

	argument = any_one_word(argument, loc_arg);
	any_one_word(argument, sect_arg);
	
	// usage: %terraform% [location] <sector vnum>
	if (!*loc_arg) {
		mob_log(ch, "mterraform: bad syntax");
		return;
	}
	
	// check number of args
	if (!*sect_arg) {
		// only arg is actually sect arg
		strcpy(sect_arg, loc_arg);
		target = IN_ROOM(ch);
	}
	else {
		// two arguments
		target = find_target_room(ch, loc_arg);
	}
	
	if (!target) {
		mob_log(ch, "mterraform: target is an invalid room");
		return;
	}
	
	// places you just can't terraform -- fail silently (currently)
	if (IS_INSIDE(target) || IS_ADVENTURE_ROOM(target) || IS_CITY_CENTER(target)) {
		return;
	}
	
	if (!isdigit(*sect_arg) || (vnum = atoi(sect_arg)) < 0 || !(sect = sector_proto(vnum))) {
		mob_log(ch, "mterraform: invalid sector vnum");
		return;
	}
	
	// validate sect
	if (SECT_FLAGGED(sect, SECTF_MAP_BUILDING | SECTF_INSIDE | SECTF_ADVENTURE)) {
		mob_log(ch, "mterraform: sector requires data that can't be set this way");
		return;
	}

	// good to go
	do_dg_terraform(target, sect);
}


ACMD(do_mdamage) {
	char name[MAX_INPUT_LENGTH], modarg[MAX_INPUT_LENGTH], typearg[MAX_INPUT_LENGTH], buf[MAX_STRING_LENGTH];
	double modifier = 1.0;
	char_data *vict;
	int type, show_attack_message = NOTHING;

	if (!MOB_OR_IMPL(ch)) {
		send_config_msg(ch, "huh_string");
		return;
	}

	if (AFF_FLAGGED(ch, AFF_ORDERED)) {
		return;
	}
	
	// optional attack message arg
	skip_spaces(&argument);
	if (*argument == '#') {
		argument = one_argument(argument, buf);
		if ((show_attack_message = atoi(buf+1)) < 1 || !real_attack_message(show_attack_message)) {
			mob_log(ch, "mdamage: invalid attack message #%s", buf);
			show_attack_message = NOTHING;
		}
	}

	argument = two_arguments(argument, name, modarg);
	argument = one_argument(argument, typearg);	// optional

	/* who cares if it's a number ? if not it'll just be 0 */
	if (!*name) {
		mob_log(ch, "mdamage: bad syntax");
		return;
	}
	
	if (*modarg) {
		modifier = atof(modarg) / 100.0;
	}
	
	// send negatives to %heal% instead
	if (modifier < 0) {
		sprintf(buf, "%s health %.2f", name, -atof(modarg));
		script_heal(ch, MOB_TRIGGER, buf);
		return;
	}
	
	modifier = scale_modifier_by_mob(ch, modifier);

	if (*name == UID_CHAR) {
		if (!(vict = get_char(name))) {
			mob_log(ch, "mdamage: victim (%s) does not exist", name);
			return;
		}
	}
	else if (!(vict = get_char_room_vis(ch, name, NULL))) {
		mob_log(ch, "mdamage: victim (%s) does not exist", name);
		return;
	}
	
	if (*typearg) {
		type = search_block(typearg, damage_types, FALSE);
		if (type == NOTHING) {
			mob_log(ch, "mdamage: invalid type argument, using physical damage");
			type = DAM_PHYSICAL;
		}
	}
	else {
		type = DAM_PHYSICAL;
	}
	
	script_damage(vict, ch, get_approximate_level(ch), type, modifier, show_attack_message);
}


ACMD(do_maoe) {
	char modarg[MAX_INPUT_LENGTH], typearg[MAX_INPUT_LENGTH];
	double modifier = 1.0;
	int level, type, show_attack_message = NOTHING;
	char_data *vict, *next_vict;

	if (!MOB_OR_IMPL(ch)) {
		send_config_msg(ch, "huh_string");
		return;
	}

	if (AFF_FLAGGED(ch, AFF_ORDERED)) {
		return;
	}

	// optional attack message arg
	skip_spaces(&argument);
	if (*argument == '#') {
		argument = one_argument(argument, modarg);
		if ((show_attack_message = atoi(modarg+1)) < 1 || !real_attack_message(show_attack_message)) {
			mob_log(ch, "maoe: invalid attack message #%s", modarg);
			show_attack_message = NOTHING;
		}
	}
	
	two_arguments(argument, modarg, typearg);
	
	if (*modarg) {
		modifier = atof(modarg) / 100.0;
	}
	
	if (*typearg) {
		type = search_block(typearg, damage_types, FALSE);
		if (type == NOTHING) {
			mob_log(ch, "maoe: invalid type argument, using physical damage");
			type = DAM_PHYSICAL;
		}
	}
	else {
		type = DAM_PHYSICAL;
	}
	
	modifier = scale_modifier_by_mob(ch, modifier);
	
	level = get_approximate_level(ch);
	DL_FOREACH_SAFE2(ROOM_PEOPLE(IN_ROOM(ch)), vict, next_vict, next_in_room) {
		if (ch == vict) {
			continue;
		}
		if (FIGHTING(ch) && !is_fight_enemy(ch, vict)) {
			continue;	// in combat, only hit enemies
		}
		if (!FIGHTING(ch) && !IS_NPC(vict)) {
			continue;	// out of combat, only hit players
		}
		
		script_damage(vict, ch, level, type, modifier, show_attack_message);
	}
}


ACMD(do_mdot) {
	char name[MAX_INPUT_LENGTH], modarg[MAX_INPUT_LENGTH], durarg[MAX_INPUT_LENGTH], typearg[MAX_INPUT_LENGTH], stackarg[MAX_INPUT_LENGTH];
	any_vnum atype = ATYPE_DG_AFFECT;
	double modifier = 1.0;
	char_data *vict;
	int type, max_stacks, duration;

	if (!MOB_OR_IMPL(ch)) {
		send_config_msg(ch, "huh_string");
		return;
	}

	if (AFF_FLAGGED(ch, AFF_ORDERED))
		return;
	
	argument = one_argument(argument, name);
	// sometimes name is an affect vnum
	if (*name == '#') {
		atype = atoi(name+1);
		argument = one_argument(argument, name);
		if (!find_generic(atype, GENERIC_AFFECT)) {
			atype = ATYPE_DG_AFFECT;
		}
	}
	argument = one_argument(argument, modarg);
	argument = one_argument(argument, durarg);
	argument = one_argument(argument, typearg);	// optional, default physical
	argument = one_argument(argument, stackarg);	// optional, default 1

	if (!*name || !*modarg || !*durarg) {
		mob_log(ch, "mdot: bad syntax");
		return;
	}
	
	if (*modarg) {
		modifier = atof(modarg) / 100.0;
	}
	modifier = scale_modifier_by_mob(ch, modifier);

	if (*name == UID_CHAR) {
		if (!(vict = get_char(name))) {
			mob_log(ch, "mdot: victim (%s) does not exist", name);
			return;
		}
	}
	else if (!(vict = get_char_room_vis(ch, name, NULL))) {
		mob_log(ch, "mdot: victim (%s) does not exist", name);
		return;
	}
	if ((duration = atoi(durarg)) < 1) {
		mob_log(ch, "mdot: invalid duration '%s'", durarg);
		return;
	}
	
	if (*typearg) {
		type = search_block(typearg, damage_types, FALSE);
		if (type == NOTHING) {
			mob_log(ch, "mdot: invalid type argument, using physical damage");
			type = DAM_PHYSICAL;
		}
	}
	else {
		type = DAM_PHYSICAL;
	}
	
	max_stacks = (*stackarg ? atoi(stackarg) : 1);
	script_damage_over_time(vict, atype, get_approximate_level(ch), type, modifier, duration, max_stacks, ch);
}


/*
* lets the mobile force someone to do something.  must be mortal level
* and the all argument only affects those in the room with the mobile
*/
ACMD(do_mforce) {
	char arg[MAX_INPUT_LENGTH];

	if (!MOB_OR_IMPL(ch)) {
		send_config_msg(ch, "huh_string");
		return;
	}

	if (AFF_FLAGGED(ch, AFF_ORDERED))
		return;

	if (ch->desc && (GET_ACCESS_LEVEL(ch->desc->original) < LVL_CIMPL))
		return;

	argument = one_argument(argument, arg);

	if (!*arg || !*argument) {
		mob_log(ch, "mforce: bad syntax");
		return;
	}

	if (!str_cmp(arg, "all")) {
		descriptor_data *i;
		char_data *vch;

		for (i = descriptor_list; i ; i = i->next) {
			if ((i->character != ch) && !i->connected && (IN_ROOM(i->character) == IN_ROOM(ch)) && STATE(i) == CON_PLAYING) {
				vch = i->character;
				if (GET_ACCESS_LEVEL(vch) < GET_ACCESS_LEVEL(ch) && CAN_SEE(ch, vch) && valid_dg_target(vch, 0)) {
					command_interpreter(vch, argument);
				}
			}
		}
	}
	else {
		char_data *victim;

		if (*arg == UID_CHAR) {
			if (!(victim = get_char(arg))) {
				mob_log(ch, "mforce: victim (%s) does not exist",arg);
				return;
			}
		}
		else if ((victim = get_char_room_vis(ch, arg, NULL)) == NULL) {
			mob_log(ch, "mforce: no such victim");
			return;
		}

		if (victim == ch) {
			mob_log(ch, "mforce: forcing self");
			return;
		}

		if (valid_dg_target(victim, 0))
			command_interpreter(victim, argument);
	}
}


ACMD(do_mheal) {
	if (!MOB_OR_IMPL(ch) || AFF_FLAGGED(ch, AFF_ORDERED) || (ch->desc && (GET_ACCESS_LEVEL(ch->desc->original) < LVL_CIMPL))) {
		send_config_msg(ch, "huh_string");
		return;
	}
	script_heal(ch, MOB_TRIGGER, argument);
}


/* hunt for someone */
ACMD(do_mhunt) {
	char_data *victim;
	char arg[MAX_INPUT_LENGTH];

	if (!MOB_OR_IMPL(ch)) {
		send_config_msg(ch, "huh_string");
		return;
	}

	if (AFF_FLAGGED(ch, AFF_ORDERED))
		return;

	if (!IS_NPC(ch)) {
		msg_to_char(ch, "Only mobs can use this.\r\n");
		return;
	}

	if (ch->desc && (GET_ACCESS_LEVEL(ch->desc->original) < LVL_CIMPL))
		return;

	one_argument(argument, arg);

	if (!*arg) {
		mob_log(ch, "mhunt called with no argument");
		return;
	}

	if (FIGHTING(ch))
		return;

	if (*arg == UID_CHAR) {
		if (!(victim = get_char(arg))) {
			mob_log(ch, "mhunt: victim (%s) does not exist", arg);
			return;
		}
	}
	else if (!(victim = get_char_vis(ch, arg, NULL, FIND_CHAR_WORLD))) {
		mob_log(ch, "mhunt: victim (%s) does not exist", arg);
		return;
	}
	
	add_pursuit(ch, victim);
	set_mob_flags(ch, MOB_PURSUE);
}


/* place someone into the mob's memory list */
ACMD(do_mremember) {
	char_data *victim;
	struct script_memory *mem;
	char arg[MAX_INPUT_LENGTH];

	if (!MOB_OR_IMPL(ch)) {
		send_config_msg(ch, "huh_string");
		return;
	}

	if (AFF_FLAGGED(ch, AFF_ORDERED))
		return;

	if (ch->desc && (GET_ACCESS_LEVEL(ch->desc->original) < LVL_CIMPL))
		return;

	argument = one_argument(argument, arg);

	if (!*arg) {
		mob_log(ch, "mremember: bad syntax");
		return;
	}

	if (*arg == UID_CHAR) {
		if (!(victim = get_char(arg))) {
			mob_log(ch, "mremember: victim (%s) does not exist", arg);
			return;
		}
	}
	else if (!(victim = get_char_vis(ch, arg, NULL, FIND_CHAR_WORLD))) {
		mob_log(ch,"mremember: victim (%s) does not exist", arg);
		return;
	}

	/* create a structure and add it to the list */
	CREATE(mem, struct script_memory, 1);
	LL_APPEND(SCRIPT_MEM(ch), mem);

	/* fill in the structure */
	mem->id = char_script_id(victim);
	if (argument && *argument) {
		mem->cmd = strdup(argument);
	}
}


/* remove someone from the list */
ACMD(do_mforget) {
	char_data *victim;
	struct script_memory *mem, *next_mem;
	char arg[MAX_INPUT_LENGTH];

	if (!MOB_OR_IMPL(ch)) {
		send_config_msg(ch, "huh_string");
		return;
	}

	if (AFF_FLAGGED(ch, AFF_ORDERED))
		return;

	if (ch->desc && (GET_ACCESS_LEVEL(ch->desc->original) < LVL_CIMPL))
		return;

	one_argument(argument, arg);

	if (!*arg) {
		mob_log(ch, "mforget: bad syntax");
		return;
	}

	if (*arg == UID_CHAR) {
		if (!(victim = get_char(arg))) {
			mob_log(ch, "mforget: victim (%s) does not exist", arg);
			return;
		}
	}
	else if (!(victim = get_char_vis(ch, arg, NULL, FIND_CHAR_WORLD))) {
		mob_log(ch, "mforget: victim (%s) does not exist", arg);
		return;
	}
	
	LL_FOREACH_SAFE(SCRIPT_MEM(ch), mem, next_mem) {
		if (mem->id == victim->script_id) {
			LL_DELETE(SCRIPT_MEM(ch), mem);
			if (mem->cmd) {
				free(mem->cmd);
			}
			free(mem);
		}
	}
}


ACMD(do_msiege) {
	char scale_arg[MAX_INPUT_LENGTH], tar_arg[MAX_INPUT_LENGTH];
	vehicle_data *veh_targ = NULL;
	room_data *room_targ = NULL;
	int dam, dir, scale = -1;

	if (!MOB_OR_IMPL(ch) || AFF_FLAGGED(ch, AFF_ORDERED)) {
		send_config_msg(ch, "huh_string");
		return;
	}
	
	two_arguments(argument, tar_arg, scale_arg);
	
	if (!*tar_arg) {
		mob_log(ch, "msiege called with no args");
		return;
	}
	// determine scale level if provided
	if (*scale_arg && (!isdigit(*scale_arg) || (scale = atoi(scale_arg)) < 0)) {
		mob_log(ch, "msiege called with invalid scale level '%s'", scale_arg);
		return;
	}
	
	// find a target
	if (!veh_targ && !room_targ && *tar_arg == UID_CHAR) {
		room_targ = find_room(atoi(tar_arg+1));
	}
	if (!veh_targ && !room_targ && *tar_arg == UID_CHAR) {
		veh_targ = get_vehicle(tar_arg);
	}
	if (!veh_targ && !room_targ && !find_siege_target_for_vehicle(ch, NULL, tar_arg, &room_targ, &dir, &veh_targ)) {
		// sends own messages -- last try here
		return;
	}
	
	// seems ok
	if (scale == -1) {
		scale = get_approximate_level(ch);
	}
	
	dam = scale * 8 / 100;	// 8 damage per 100 levels
	dam = MAX(1, dam);	// minimum 1
	
	if (room_targ) {
		if (validate_siege_target_room(ch, NULL, room_targ)) {
			besiege_room(NULL, room_targ, dam, NULL);
		}
	}
	else if (veh_targ) {
		besiege_vehicle(NULL, veh_targ, dam, SIEGE_PHYSICAL, NULL);
	}
	else {
		mob_log(ch, "osiege: invalid target");
	}
}


// kills the target
ACMD(do_mslay) {
	char name[MAX_INPUT_LENGTH];
	char_data *vict;

	if (!MOB_OR_IMPL(ch)) {
		send_config_msg(ch, "huh_string");
		return;
	}

	if (AFF_FLAGGED(ch, AFF_ORDERED))
		return;

	argument = one_argument(argument, name);

	if (!*name) {
		mob_log(ch, "mslay: no target");
		return;
	}
	
	if (*name == UID_CHAR) {
		if (!(vict = get_char(name))) {
			mob_log(ch, "mslay: victim (%s) does not exist", name);
			return;
		}
	}
	else if (!(vict = get_char_room_vis(ch, name, NULL))) {
		mob_log(ch, "mslay: victim (%s) does not exist", name);
		return;
	}
	
	if (IS_IMMORTAL(vict)) {
		msg_to_char(vict, "Being the cool immortal you are, you sidestep a trap, obviously placed to kill you.\r\n");
	}
	else {
		// log
		if (!IS_NPC(vict)) {
			if (*argument) {
				// custom death log?
				log_to_slash_channel_by_name(DEATH_LOG_CHANNEL, vict, "%s", argument);
			}
			else {
				// basic death log
				log_to_slash_channel_by_name(DEATH_LOG_CHANNEL, vict, "%s has died at (%d, %d)!", PERS(vict, vict, TRUE), X_COORD(IN_ROOM(vict)), Y_COORD(IN_ROOM(vict)));
			}
			syslog(SYS_DEATH, 0, TRUE, "DEATH: %s has been killed by a script at %s (mob %d)", GET_NAME(vict), room_log_identifier(IN_ROOM(vict)), GET_MOB_VNUM(ch));
		}
		
		die(vict, ch);
	}
}


/* transform into a different mobile */
ACMD(do_mtransform) {
	char arg[MAX_INPUT_LENGTH];
	// char_data *m, tmpmob;
	// obj_data *obj[NUM_WEARS];
	// mob_vnum this_vnum = GET_MOB_VNUM(ch);
	// bool keep_attr = TRUE; // new mob keeps the old mob's h/v/m/b
	// int pos, iter;

	if (!MOB_OR_IMPL(ch)) {
		send_config_msg(ch, "huh_string");
		return;
	}

	if (AFF_FLAGGED(ch, AFF_ORDERED))
		return;

	if (ch->desc || !IS_NPC(ch)) {
		msg_to_char(ch, "You've got no VNUM to return to, dummy! try 'switch'\r\n");
		return;
	}

	one_argument(argument, arg);

	if (TRUE) {
		mob_log(ch, "mtransform is currently disabled");
	}
	else if (!*arg)
		mob_log(ch, "mtransform: missing argument");
	else if (!isdigit(*arg) && *arg != '-')
		mob_log(ch, "mtransform: bad argument");
	else {
		/* none of this will work
		if (isdigit(*arg))
			m = read_mobile(atoi(arg), TRUE);
		else {
			keep_attr = 0;
			m = read_mobile(atoi(arg+1), TRUE);
		}
		if (m == NULL) {
			mob_log(ch, "mtransform: bad mobile vnum");
			return;
		}
		
		MOB_INSTANCE_ID(m) = MOB_INSTANCE_ID(ch);
		if (MOB_INSTANCE_ID(m) != NOTHING) {
			add_instance_mob(real_instance(MOB_INSTANCE_ID(m)), GET_MOB_VNUM(m));
		}
		
		setup_generic_npc(m, GET_LOYALTY(ch), MOB_DYNAMIC_NAME(ch), MOB_DYNAMIC_SEX(ch));

		// move new obj info over to old object and delete new obj

		for (pos = 0; pos < NUM_WEARS; pos++) {
			if (GET_EQ(ch, pos))
				obj[pos] = unequip_char(ch, pos);
			else
				obj[pos] = NULL;
		}

		// put the mob in the same room as ch so extract will work
		char_to_room(m, IN_ROOM(ch));
		
		memcpy(&tmpmob, m, sizeof(*m));
		tmpmob.script_id = ch->script_id;
		tmpmob.affected = ch->affected;
		tmpmob.carrying = ch->carrying;
		tmpmob.proto_script = ch->proto_script;
		tmpmob.script = ch->script;
		tmpmob.memory = ch->memory;
		tmpmob.followers = ch->followers;
		tmpmob.leader = GET_LEADER(ch);
		tmpmob.cooldowns = ch->cooldowns;
	
		if (keep_attr) {
			for (iter = 0; iter < NUM_POOLS; ++iter) {
				GET_CURRENT_POOL(&tmpmob, iter) = GET_CURRENT_POOL(ch, iter);	// careful setting this way -- does not schedule heals
				GET_MAX_POOL(&tmpmob, iter) = GET_MAX_POOL(ch, iter);
			}
		}
		GET_POS(&tmpmob) = GET_POS(ch);
		IS_CARRYING_N(&tmpmob) = IS_CARRYING_N(ch);
		FIGHTING(&tmpmob) = FIGHTING(ch);
		HUNTING(&tmpmob) = HUNTING(ch);
		memcpy(ch, &tmpmob, sizeof(*ch));

		for (pos = 0; pos < NUM_WEARS; pos++) {
			if (obj[pos])
				equip_char(ch, obj[pos], pos);
		}

		ch->vnum = this_vnum;
		extract_char(m);
		*/
	}
}


ACMD(do_mdoor) {
	char target[MAX_INPUT_LENGTH], direction[MAX_INPUT_LENGTH];
	char field[MAX_INPUT_LENGTH], *value;
	room_data *rm, *to_room;
	struct room_direction_data *newexit;
	int dir, fd;

	const char *door_field[] = {
		"purge",	// 0
		"flags",	// 1
		"name",	// 2
		"room",	// 3
		"add",	// 4
		"\n"
	};


	if (!MOB_OR_IMPL(ch)) {
		send_config_msg(ch, "huh_string");
		return;
	}

	if (AFF_FLAGGED(ch, AFF_ORDERED))
		return;

	argument = one_word(argument, target);
	argument = one_argument(argument, direction);
	value = one_argument(argument, field);
	skip_spaces(&value);

	if (!*target || !*direction || !*field) {
		mob_log(ch, "mdoor called with too few args");
		return;
	}

	if (!(rm = find_target_room(ch, target))) {
		mob_log(ch, "mdoor: invalid target");
		return;
	}

	if ((dir = search_block(direction, dirs, FALSE)) == NO_DIR && (dir = search_block(direction, alt_dirs, FALSE)) == NO_DIR) {
		mob_log(ch, "mdoor: invalid direction");
		return;
	}

	if ((fd = search_block(field, door_field, FALSE)) == NOTHING) {
		mob_log(ch, "mdoor: invalid field");
		return;
	}
	
	if (!COMPLEX_DATA(rm)) {
		mob_log(ch, "mdoor called on room with no building data");
		return;
	}

	newexit = find_exit(rm, dir);

	/* purge exit */
	if (fd == 0) {
		if (newexit) {
			LL_DELETE(COMPLEX_DATA(rm)->exits, newexit);
			if (newexit->room_ptr) {
				--GET_EXITS_HERE(newexit->room_ptr);
			}
			if (newexit->keyword)
				free(newexit->keyword);
			free(newexit);
		}
	}
	else {
		switch (fd) {
			case 1:  /* flags       */
				if (!newexit) {
					mob_log(ch, "mdoor: invalid door");
					break;
				}
				newexit->exit_info = (sh_int)asciiflag_conv(value);
				break;
			case 2:  /* name        */
				if (!newexit) {
					mob_log(ch, "mdoor: invalid door");
					break;
				}
				if (newexit->keyword)
					free(newexit->keyword);
				newexit->keyword = str_dup(value);
				break;
			case 3:  /* room        */
				if ((to_room = find_target_room(ch, value))) {
					if (!newexit) {
						newexit = create_exit(rm, to_room, dir, FALSE);
					}
					else {
						if (newexit->room_ptr) {
							// lower old one
							--GET_EXITS_HERE(newexit->room_ptr);
						}
						newexit->to_room = GET_ROOM_VNUM(to_room);
						newexit->room_ptr = to_room;
						++GET_EXITS_HERE(to_room);
					}
				}
				else
					mob_log(ch, "mdoor: invalid door target");
				break;
			case 4: {	// create room
				bld_data *bld;
				
				if (IS_ADVENTURE_ROOM(rm) || !ROOM_IS_CLOSED(rm) || !COMPLEX_DATA(rm)) {
					mob_log(ch, "mdoor: attempting to add a room in invalid location %d", GET_ROOM_VNUM(rm));
				}
				else if (!*value || !isdigit(*value) || !(bld = building_proto(atoi(value))) || !IS_SET(GET_BLD_FLAGS(bld), BLD_ROOM)) {
					mob_log(ch, "mdoor: attempting to add invalid room '%s'", value);
				}
				else {
					do_dg_add_room_dir(rm, dir, bld);
				}
				break;
			}
		}
	}
}


ACMD(do_mfollow) {
	char buf[MAX_INPUT_LENGTH];
	char_data *leader;
	struct follow_type *fol, *next_fol;

	if (!MOB_OR_IMPL(ch)) {
		send_config_msg(ch, "huh_string");
		return;
	}

	if (AFF_FLAGGED(ch, AFF_ORDERED))
		return;

	one_argument(argument, buf);

	if (!*buf) {
		mob_log(ch, "mfollow: bad syntax");
		return;
	}

	if (*buf == UID_CHAR) {
		if (!(leader = get_char(buf))) {
			mob_log(ch, "mfollow: victim (%s) does not exist", buf);
			return;
		}
	}
	else if (!(leader = get_char_vis(ch, buf, NULL, FIND_CHAR_ROOM))) {
		mob_log(ch,"mfollow: victim (%s) not found", buf);
		return;
	}

	if (GET_LEADER(ch) == leader) /* already following */
		return;

	if (AFF_FLAGGED(ch, AFF_CHARM) && (GET_LEADER(ch)))  /* can't override charm */
		return;

	/* stop following someone else first */
	if (GET_LEADER(ch)) {
		LL_FOREACH_SAFE(GET_LEADER(ch)->followers, fol, next_fol) {
			if (fol->follower == ch) {
				LL_DELETE(GET_LEADER(ch)->followers, fol);
				free(fol);
			}
		}
		
		GET_LEADER(ch) = NULL;
	}

	if (ch == leader) 
		return;

	if (circle_follow(ch, leader)) {
		mob_log(ch, "mfollow: Following in circles.");
		return;
	}

	GET_LEADER(ch) = leader;

	CREATE(fol, struct follow_type, 1);
	fol->follower = ch;
	LL_PREPEND(leader->followers, fol);
}


ACMD(do_mown) {
	char type_arg[MAX_INPUT_LENGTH], targ_arg[MAX_INPUT_LENGTH], emp_arg[MAX_INPUT_LENGTH];
	vehicle_data *vtarg = NULL;
	empire_data *emp = NULL;
	char_data *vict = NULL;
	room_data *rtarg = NULL;
	obj_data *otarg = NULL;
	
	*emp_arg = '\0';	// just in case

	if (!MOB_OR_IMPL(ch) || AFF_FLAGGED(ch, AFF_ORDERED)) {
		send_config_msg(ch, "huh_string");
		return;
	}
	
	// first arg -- possibly a type
	argument = one_argument(argument, type_arg);
	if (is_abbrev(type_arg, "room")) {
		argument = one_argument(argument, targ_arg);
		skip_spaces(&argument);
		
		if (!*targ_arg) {
			mob_log(ch, "mown: Too few arguments (mown room)");
			return;
		}
		else if (!*argument) {
			// this was the last arg
			strcpy(emp_arg, targ_arg);
		}
		else if (!(rtarg = find_target_room(ch, targ_arg))) {
			mob_log(ch, "mown: Invalid room target");
			return;
		}
		else {
			// room is set; remaining arg is owner
			strcpy(emp_arg, argument);
		}
	}
	else if (is_abbrev(type_arg, "mobile")) {
		argument = one_argument(argument, targ_arg);
		skip_spaces(&argument);
		strcpy(emp_arg, argument);	// always
		
		if (!*targ_arg || !*emp_arg) {
			mob_log(ch, "mown: Too few arguments (mown mob)");
			return;
		}
		else if (!(vict = ((*targ_arg == UID_CHAR) ? get_char(targ_arg) : get_char_room_vis(ch, targ_arg, NULL)))) {
			mob_log(ch, "mown: Invalid mob target");
			return;
		}
	}
	else if (is_abbrev(type_arg, "vehicle")) {
		argument = one_argument(argument, targ_arg);
		skip_spaces(&argument);
		strcpy(emp_arg, argument);	// always
		
		if (!*targ_arg || !*emp_arg) {
			mob_log(ch, "mown: Too few arguments (mown vehicle)");
			return;
		}
		else if (!(vtarg = ((*targ_arg == UID_CHAR) ? get_vehicle(targ_arg) : get_vehicle_in_room_vis(ch, targ_arg, NULL)))) {
			mob_log(ch, "mown: Invalid vehicle target");
			return;
		}
	}
	else if (is_abbrev(type_arg, "object")) {
		argument = one_argument(argument, targ_arg);
		skip_spaces(&argument);
		strcpy(emp_arg, argument);	// always
		
		if (!*targ_arg || !*emp_arg) {
			mob_log(ch, "mown: Too few arguments (mown obj)");
			return;
		}
		else if (!(otarg = ((*targ_arg == UID_CHAR) ? get_obj(targ_arg) : get_obj_vis(ch, targ_arg, NULL)))) {
			mob_log(ch, "mown: Invalid obj target");
			return;
		}
	}
	else {	// attempt to find a target
		strcpy(targ_arg, type_arg);	// there was no type
		skip_spaces(&argument);
		strcpy(emp_arg, argument);
		
		if (!*targ_arg) {
			mob_log(ch, "mown: Too few arguments");
			return;
		}
		else if (*targ_arg == UID_CHAR && ((vict = get_char(targ_arg)) || (vtarg = get_vehicle(targ_arg)) || (otarg = get_obj(targ_arg)) || (rtarg = get_room(IN_ROOM(ch), targ_arg)))) {
			// found by uid
		}
		else if ((vict = get_char_room_vis(ch, targ_arg, NULL)) || (vtarg = get_vehicle_in_room_vis(ch, targ_arg, NULL)) || (otarg = get_obj_in_list_vis(ch, targ_arg, NULL, ch->carrying)) || (otarg = get_obj_in_list_vis(ch, targ_arg, NULL, ROOM_CONTENTS(IN_ROOM(ch))))) {
			// found by name
		}
		else {
			mob_log(ch, "mown: Invalid target");
			return;
		}
	}
	
	// only change owner on the home room
	if (rtarg) {
		rtarg = HOME_ROOM(rtarg);
	}
	
	// check that we got a target
	if (!vict && !vtarg && !rtarg && !otarg) {
		mob_log(ch, "mown: Unable to find a target");
		return;
	}
	
	// process the empire
	if (!*emp_arg) {
		mob_log(ch, "mown: No empire argument given");
		return;
	}
	else if (!str_cmp(emp_arg, "none")) {
		emp = NULL;	// set owner to none
	}
	else if (!(emp = get_empire(emp_arg))) {
		mob_log(ch, "mown: Invalid empire");
		return;
	}
	
	// final target checks
	if (vict && !IS_NPC(vict)) {
		mob_log(ch, "mown: Attempting to change the empire of a player");
		return;
	}
	if (rtarg && IS_ADVENTURE_ROOM(rtarg)) {
		mob_log(ch, "mown: Attempting to change ownership of an adventure room");
		return;
	}
	
	// do the ownership change
	do_dg_own(emp, vict, otarg, rtarg, vtarg);
}


ACMD(do_mscale) {
	char arg[MAX_INPUT_LENGTH], lvl_arg[MAX_INPUT_LENGTH];
	char_data *victim;
	obj_data *obj, *fresh, *proto;
	vehicle_data *veh;
	int level;

	if (!MOB_OR_IMPL(ch)) {
		send_config_msg(ch, "huh_string");
		return;
	}

	if (AFF_FLAGGED(ch, AFF_ORDERED))
		return;

	if (ch->desc && (GET_ACCESS_LEVEL(ch->desc->original) < LVL_CIMPL))
		return;

	two_arguments(argument, arg, lvl_arg);

	if (!*arg) {
		mob_log(ch, "mscale: No target provided");
		return;
	}
	
	if (*lvl_arg && !isdigit(*lvl_arg)) {
		mob_log(ch, "mscale: invalid level '%s'", lvl_arg);
		return;
	}
	else if (*lvl_arg) {
		level = atoi(lvl_arg);
	}
	else {
		level = IS_NPC(ch) ? GET_CURRENT_SCALE_LEVEL(ch) : 0;
	}
	
	if (level <= 0) {
		mob_log(ch, "mscale: no valid level to scale to");
		return;
	}
	
	// scale adventure
	if (!str_cmp(arg, "instance")) {
		struct instance_data *inst;
		if (MOB_INSTANCE_ID(ch) != NOTHING && (inst = get_instance_by_mob(ch))) {
			scale_instance_to_level(inst, level);
		}
	}
	// scale char
	else if ((*arg == UID_CHAR && (victim = get_char(arg))) || (victim = get_char_room_vis(ch, arg, NULL))) {
		if (!IS_NPC(victim)) {
			mob_log(ch, "mscale: unable to scale a PC");
			return;
		}

		scale_mob_to_level(victim, level);
		set_mob_flags(victim, MOB_NO_RESCALE);
	}
	// scale evhicle
	else if ((*arg == UID_CHAR && (veh = get_vehicle(arg))) || (veh = get_vehicle_in_room_vis(ch, arg, NULL))) {
		scale_vehicle_to_level(veh, level);
	}
	// scale obj
	else if ((*arg == UID_CHAR && (obj = get_obj(arg))) || (obj = get_obj_vis(ch, arg, NULL))) {
		if (OBJ_FLAGGED(obj, OBJ_SCALABLE)) {
			scale_item_to_level(obj, level);
		}
		else if ((proto = obj_proto(GET_OBJ_VNUM(obj))) && OBJ_FLAGGED(proto, OBJ_SCALABLE)) {
			fresh = fresh_copy_obj(obj, level);
			swap_obj_for_obj(obj, fresh);
			extract_obj(obj);
		}
		else {
			// attempt to scale anyway
			scale_item_to_level(obj, level);
		}
	}
	else {
		mob_log(ch, "mscale: bad argument");
	}
}
