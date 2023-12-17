/* ************************************************************************
*   File: dg_wldcmd.c                                     EmpireMUD 2.0b5 *
*  Usage: contains the command_interpreter for rooms, room commands.      *
*                                                                         *
*  DG Scripts code by galion, 1996/08/05 03:27:07, revision 3.12          *
*  EmpireMUD code base by Paul Clarke, (C) 2000-2015                      *
*  All rights reserved.  See license.doc for complete information.        *
*                                                                         *
*  EmpireMUD based upon CircleMUD 3.0, bpl 17, by Jeremy Elson.           *
*  Death's Gate MUD is based on CircleMUD, Copyright (C) 1993, 94.        *
*  CircleMUD (C) 1993, 94 by the Trustees of the Johns Hopkins University *
*  CircleMUD is based on DikuMUD, Copyright (C) 1990, 1991.               *
************************************************************************ */

#include "conf.h"
#include "sysdep.h"

#include "structs.h"
#include "dg_scripts.h"
#include "utils.h"
#include "comm.h"
#include "interpreter.h"
#include "handler.h"
#include "db.h"
#include "skills.h"
#include "vnums.h"
#include "constants.h"

/**
* Contents:
*   Helpers
*   World Commands
*   World Command Interpreter
*/

 //////////////////////////////////////////////////////////////////////////////
//// HELPERS /////////////////////////////////////////////////////////////////

/**
* Determines the scale level for a room, with a character target as a backup
* for unscaled rooms.
*
* @param room_data *room The room to check.
* @param char_data *targ Optional: A character target.
* @return int A scale level.
*/
int get_room_scale_level(room_data *room, char_data *targ) {
	struct instance_data *inst;
	vehicle_data *veh;
	int level = 0;
	
	if ((inst = find_instance_by_room(room, FALSE, TRUE))) {
		if (INST_LEVEL(inst)) {
			level = INST_LEVEL(inst);
		}
		else if (GET_ADV_MIN_LEVEL(INST_ADVENTURE(inst)) > 0) {
			level = GET_ADV_MIN_LEVEL(INST_ADVENTURE(inst));
		}
		else if (GET_ADV_MAX_LEVEL(INST_ADVENTURE(inst)) > 0) {
			level = GET_ADV_MAX_LEVEL(INST_ADVENTURE(inst)) / 2; // average?
		}
	}
	else if ((veh = GET_ROOM_VEHICLE(room))) {
		if (VEH_SCALE_LEVEL(veh) > 0) {
			level = VEH_SCALE_LEVEL(veh);
		}
		else if (VEH_MIN_SCALE_LEVEL(veh) > 0) {
			level = VEH_MIN_SCALE_LEVEL(veh);
		}
		else if (VEH_MAX_SCALE_LEVEL(veh) > 0) {
			level = VEH_MAX_SCALE_LEVEL(veh) / 2;	// average
		}
	}
	
	// backup (if something above got a 0)
	if (!level && targ) {
		level = get_approximate_level(targ);
	}
	
	return MAX(1, level);
}


/* attaches room vnum to msg and sends it to script_log */
void wld_log(room_data *room, const char *format, ...) {
	va_list args;
	char output[MAX_STRING_LENGTH];

	snprintf(output, sizeof(output), "Room %d :: %s", GET_ROOM_VNUM(room), format);

	va_start(args, format);
	script_vlog(output, args);
	va_end(args);
}


/**
* Sends a script message to everyone in the room (formerly act_to_room) using
* sub_write.
*
* @param char *str The message.
* @param room_data *room The room to send to.
* @param bool use_queue If TRUE, stacks the message to the queue instead of sending immediately.
*/
void sub_write_to_room(char *str, room_data *room, bool use_queue) {
	char_data *iter;
	
	DL_FOREACH2(ROOM_PEOPLE(room), iter, next_in_room) {
		sub_write(str, iter, TRUE, TO_CHAR | (use_queue ? TO_QUEUE : 0));
	}
}


 //////////////////////////////////////////////////////////////////////////////
//// WORLD COMMANDS //////////////////////////////////////////////////////////

WCMD(do_wadventurecomplete) {
	struct instance_data *inst;
	
	inst = quest_instance_global;
	if (!inst) {
		inst = find_instance_by_room(room, FALSE, TRUE);
	}
	
	if (inst) {
		mark_instance_completed(inst);
	}
}


/* prints the argument to all the rooms aroud the room */
WCMD(do_wasound) {
	bool use_queue, map_echo = FALSE;
	struct room_direction_data *ex;
	room_data *to_room;
	int dir;

	use_queue = script_message_should_queue(&argument);

	if (!*argument) {
		wld_log(room, "wasound called with no argument");
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


WCMD(do_wbuild) {
	char loc_arg[MAX_INPUT_LENGTH], bld_arg[MAX_INPUT_LENGTH], *tmp;
	room_data *target;
	
	tmp = any_one_word(argument, loc_arg);
	strcpy(bld_arg, tmp);
	
	// usage: %build% [location] <vnum [dir] | ruin | demolish>
	if (!*loc_arg) {
		wld_log(room, "obuild: bad syntax");
		return;
	}
	
	// check number of args
	if (!*bld_arg) {
		// only arg is actually building arg
		strcpy(bld_arg, argument);
		target = room;
	}
	else {
		// two arguments
		target = get_room(room, loc_arg);
	}
	
	if (!target) {
		wld_log(room, "obuild: target is an invalid room");
		return;
	}
	
	// places you just can't build -- fail silently (currently)
	if (IS_INSIDE(target) || IS_ADVENTURE_ROOM(target) || IS_CITY_CENTER(target)) {
		return;
	}

	// good to go
	do_dg_build(target, bld_arg);
}


WCMD(do_wecho) {
	bool use_queue = script_message_should_queue(&argument);

	if (!*argument) {
		wld_log(room, "wecho called with no args");
	}
	else {
		sub_write_to_room(argument, room, use_queue);
	}
}


// prints the message to everyone except two targets
WCMD(do_wechoneither) {
	char arg1[MAX_INPUT_LENGTH], arg2[MAX_INPUT_LENGTH];
	char_data *vict1, *vict2, *iter;
	bool use_queue;
	char *p;

	p = two_arguments(argument, arg1, arg2);
	use_queue = script_message_should_queue(&p);

	if (!*arg1 || !*arg2 || !*p) {
		wld_log(room, "oechoneither called with missing arguments");
		return;
	}
	
	if (!(vict1 = get_char_by_room(room, arg1))) {
		wld_log(room, "oechoneither: vict 1 (%s) does not exist", arg1);
		return;
	}
	if (!(vict2 = get_char_by_room(room, arg2))) {
		wld_log(room, "oechoneither: vict 2 (%s) does not exist", arg2);
		return;
	}
	
	DL_FOREACH2(ROOM_PEOPLE(room), iter, next_in_room) {
		if (iter->desc && iter != vict1 && iter != vict2) {
			sub_write(p, iter, TRUE, TO_CHAR | (use_queue ? TO_QUEUE : 0));
		}
	}
}


WCMD(do_wsend) {
	char buf[MAX_INPUT_LENGTH], *msg;
	bool use_queue;
	char_data *ch;

	msg = any_one_arg(argument, buf);

	if (!*buf) {
		wld_log(room, "wsend called with no args");
		return;
	}

	use_queue = script_message_should_queue(&msg);

	if (!*msg) {
		wld_log(room, "wsend called without a message");
		return;
	}

	if ((ch = get_char_by_room(room, buf))) {
		if (subcmd == SCMD_WSEND)
			sub_write(msg, ch, TRUE, TO_CHAR | TO_SLEEP | (use_queue ? TO_QUEUE : 0));
		else if (subcmd == SCMD_WECHOAROUND)
			sub_write(msg, ch, TRUE, TO_ROOM | (use_queue ? TO_QUEUE : 0));
	}
	else
		wld_log(room, "no target found for wsend");
}


WCMD(do_wbuildingecho) {
	room_data *froom, *home_room;
	char room_num[MAX_INPUT_LENGTH], *msg;
	char_data *iter;
	bool use_queue;

	msg = any_one_word(argument, room_num);
	use_queue = script_message_should_queue(&msg);

	if (!*room_num || !*msg)
		wld_log(room, "wbuildingecho called with too few args");
	else if (!(froom = get_room(room, room_num))) {
		wld_log(room, "wbuildingecho called with bad target");
	}
	else {
		home_room = HOME_ROOM(froom);
		
		DL_FOREACH(character_list, iter) {
			if (HOME_ROOM(IN_ROOM(iter)) == home_room) {
				sub_write(msg, iter, TRUE, TO_CHAR | (use_queue ? TO_QUEUE : 0));
			}
		}
	}
}


WCMD(do_wregionecho) {
	char room_number[MAX_INPUT_LENGTH], radius_arg[MAX_INPUT_LENGTH], *msg;
	bool use_queue, outdoor_only = FALSE;
	room_data *center;
	char_data *targ;
	int radius;

	argument = any_one_word(argument, room_number);
	msg = one_argument(argument, radius_arg);
	use_queue = script_message_should_queue(&msg);

	if (!*room_number || !*radius_arg || !*msg) {
		wld_log(room, "wregionecho called with too few args");
	}
	else if (!isdigit(*radius_arg) && *radius_arg != '-') {
		wld_log(room, "wregionecho called with invalid radius");
	}
	else if (!(center = get_room(room, room_number))) {
		wld_log(room, "wregionecho called with invalid target");
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


WCMD(do_wsubecho) {
	char room_number[MAX_INPUT_LENGTH], *msg;
	bool use_queue;
	room_data *where;
	char_data *targ;
	
	msg = any_one_word(argument, room_number);
	use_queue = script_message_should_queue(&msg);

	if (!*room_number || !*msg) {
		wld_log(room, "wsubecho called with too few args");
	}
	else if (!(where = get_room(room, room_number))) {
		wld_log(room, "wsubecho called with invalid target");
	}
	else if (!ROOM_INSTANCE(where)) {
		wld_log(room, "wsubecho called outside an adventure");
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


WCMD(do_wvehicleecho) {
	char targ[MAX_INPUT_LENGTH], *msg;
	vehicle_data *veh;
	char_data *iter;
	bool use_queue;

	msg = any_one_word(argument, targ);
	use_queue = script_message_should_queue(&msg);

	if (!*targ || !*msg)
		wld_log(room, "wvehicleecho called with too few args");
	else if (!(*targ == UID_CHAR && (veh = get_vehicle(targ))) && !(veh = get_vehicle_room(room, targ, NULL))) {
		wld_log(room, "wvehicleecho called with bad target");
	}
	else {
		DL_FOREACH(character_list, iter) {
			if (VEH_SITTING_ON(veh) == iter || GET_ROOM_VEHICLE(IN_ROOM(iter)) == veh) {
				sub_write(msg, iter, TRUE, TO_CHAR | (use_queue ? TO_QUEUE : 0));
			}
		}
	}
}


WCMD(do_wdoor) {
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


	argument = one_word(argument, target);
	argument = one_argument(argument, direction);
	value = one_argument(argument, field);
	skip_spaces(&value);

	if (!*target || !*direction || !*field) {
		wld_log(room, "wdoor called with too few args");
		return;
	}

	if ((rm = get_room(room, target)) == NULL) {
		wld_log(room, "wdoor: invalid target");
		return;
	}

	if ((dir = search_block(direction, dirs, FALSE)) == NO_DIR && (dir = search_block(direction, alt_dirs, FALSE)) == NO_DIR) {
		wld_log(room, "wdoor: invalid direction");
		return;
	}

	if ((fd = search_block(field, door_field, FALSE)) == NOTHING) {
		wld_log(room, "wdoor: invalid field");
		return;
	}
	
	if (!COMPLEX_DATA(rm)) {
		wld_log(room, "wdoor: called on room with no building data");
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
					wld_log(room, "wdoor: invalid door");
					break;
				}
				newexit->exit_info = (sh_int)asciiflag_conv(value);
				break;
			case 2:  /* name        */
				if (!newexit) {
					wld_log(room, "wdoor: invalid door");
					break;
				}
				if (newexit->keyword)
					free(newexit->keyword);
				CREATE(newexit->keyword, char, strlen(value) + 1);
				strcpy(newexit->keyword, value);
				break;
			case 3:  /* room        */
				if ((to_room = get_room(room, value))) {
					if (!newexit) {
						newexit = create_exit(rm, to_room, dir, FALSE);
					}
					else {
						if (newexit->room_ptr) {
							// lower old room
							--GET_EXITS_HERE(newexit->room_ptr);
						}
						newexit->to_room = GET_ROOM_VNUM(to_room);
						newexit->room_ptr = to_room;
						++GET_EXITS_HERE(to_room);
					}
				}
				else
					wld_log(room, "wdoor: invalid door target");
				break;
			case 4: {	// create room
				bld_data *bld;
				
				if (IS_ADVENTURE_ROOM(rm) || !ROOM_IS_CLOSED(rm) || !COMPLEX_DATA(rm)) {
					wld_log(room, "wdoor: attempting to add a room in invalid location %d", GET_ROOM_VNUM(rm));
				}
				else if (!*value || !isdigit(*value) || !(bld = building_proto(atoi(value))) || !IS_SET(GET_BLD_FLAGS(bld), BLD_ROOM)) {
					wld_log(room, "wdoor: attempting to add invalid room '%s'", value);
				}
				else {
					do_dg_add_room_dir(rm, dir, bld);
				}
				break;
			}
		}
	}
}


WCMD(do_wsiege) {
	char scale_arg[MAX_INPUT_LENGTH], tar_arg[MAX_INPUT_LENGTH];
	vehicle_data *veh_targ = NULL;
	room_data *room_targ = NULL;
	int dam, dir, scale = -1;
	
	two_arguments(argument, tar_arg, scale_arg);
	
	if (!*tar_arg) {
		wld_log(room, "wsiege called with no args");
		return;
	}
	// determine scale level if provided
	if (*scale_arg && (!isdigit(*scale_arg) || (scale = atoi(scale_arg)) < 0)) {
		wld_log(room, "wsiege called with invalid scale level '%s'", scale_arg);
		return;
	}
	
	// find a target
	if (!veh_targ && !room_targ && *tar_arg == UID_CHAR) {
		room_targ = find_room(atoi(tar_arg+1));
	}
	if (!veh_targ && !room_targ && *tar_arg == UID_CHAR) {
		veh_targ = get_vehicle(tar_arg);
	}
	if (!veh_targ && !room_targ) {
		if ((dir = search_block(tar_arg, dirs, FALSE)) != NOTHING || (dir = search_block(tar_arg, alt_dirs, FALSE)) != NOTHING) {
			room_targ = dir_to_room(room, dir, FALSE);
		}
	}
	if (!veh_targ && !room_targ) {
		veh_targ = get_vehicle_room(room, tar_arg, NULL);
	}
	
	// seems ok
	if (scale == -1) {
		scale = get_room_scale_level(room, NULL);
	}
	
	dam = scale * 8 / 100;	// 8 damage per 100 levels
	dam = MAX(1, dam);	// minimum 1
	
	if (room_targ) {
		if (validate_siege_target_room(NULL, NULL, room_targ)) {
			besiege_room(NULL, room_targ, dam, NULL);
		}
	}
	else if (veh_targ) {
		besiege_vehicle(NULL, veh_targ, dam, SIEGE_PHYSICAL, NULL);
	}
	else {
		wld_log(room, "wsiege: invalid target");
	}
}


// kills the target
WCMD(do_wslay) {
	char name[MAX_INPUT_LENGTH];
	char_data *vict;
	
	argument = one_argument(argument, name);

	if (!*name) {
		wld_log(room, "wslay: no target");
		return;
	}
	
	if (*name == UID_CHAR) {
		if (!(vict = get_char(name))) {
			wld_log(room, "wslay: victim (%s) does not exist", name);
			return;
		}
	}
	else if (!(vict = get_char_by_room(room, name))) {
		wld_log(room, "wslay: victim (%s) does not exist", name);
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
			syslog(SYS_DEATH, 0, TRUE, "DEATH: %s has been killed by a script at %s (room %d, rmt %d, bld %d)", GET_NAME(vict), room_log_identifier(IN_ROOM(vict)), GET_ROOM_VNUM(room), GET_ROOM_TEMPLATE(room) ? GET_RMT_VNUM(GET_ROOM_TEMPLATE(room)) : -1, GET_BUILDING(room) ? GET_BLD_VNUM(GET_BUILDING(room)) : -1);
		}
		
		die(vict, vict);
	}
}


WCMD(do_wteleport) {
	char_data *ch, *next_ch;
	vehicle_data *veh;
	room_data *target;
	obj_data *obj, *cont;
	char arg1[MAX_INPUT_LENGTH], arg2[MAX_INPUT_LENGTH];
	struct instance_data *inst;
	int iter;

	argument = one_argument(argument, arg1);
	argument = one_word(argument, arg2);

	if (!*arg1 || !*arg2) {
		wld_log(room, "wteleport: called with too few args");
		return;
	}

	// try to find a target for later -- this can fail
	target = (*arg2 == UID_CHAR ? find_room(atoi(arg2 + 1)) : get_room(room, arg2));

	if (!str_cmp(arg1, "all")) {
		if (!target) {
			wld_log(room, "wteleport all: target is an invalid room");
			return;
		}
		else if (target == room) {
			wld_log(room, "wteleport all: target is itself");
			return;
		}
		
		DL_FOREACH_SAFE2(ROOM_PEOPLE(room), ch, next_ch, next_in_room) {
			if (!valid_dg_target(ch, DG_ALLOW_GODS)) 
				continue;
			GET_LAST_DIR(ch) = NO_DIR;
			char_from_room(ch);
			char_to_room(ch, target);
			enter_wtrigger(IN_ROOM(ch), ch, NO_DIR, "script");
			qt_visit_room(ch, IN_ROOM(ch));
			RESET_LAST_MESSAGED_TEMPERATURE(ch);
			msdp_update_room(ch);	// once we're sure we're staying
		}
	}
	else if (!str_cmp(arg1, "adventure")) {
		// teleport all players in the adventure
		if (!(inst = ROOM_INSTANCE(room))) {
			wld_log(room, "wteleport adventure: called outside any adventure");
			return;
		}
		if (!target) {
			wld_log(room, "wteleport adventure: target is an invalid room");
			return;
		}
		
		for (iter = 0; iter < INST_SIZE(inst); ++iter) {
			// only if it's not the target room, or we'd be here all day
			if (INST_ROOM(inst, iter) && INST_ROOM(inst, iter) != target) {
				DL_FOREACH_SAFE2(ROOM_PEOPLE(INST_ROOM(inst, iter)), ch, next_ch, next_in_room) {
					if (!valid_dg_target(ch, DG_ALLOW_GODS)) {
						continue;
					}
					
					// teleport players and their followers
					if (!IS_NPC(ch) || (GET_LEADER(ch) && !IS_NPC(GET_LEADER(ch)))) {
						char_from_room(ch);
						char_to_room(ch, target);
						GET_LAST_DIR(ch) = NO_DIR;
						enter_wtrigger(IN_ROOM(ch), ch, NO_DIR, "script");
						qt_visit_room(ch, IN_ROOM(ch));
						RESET_LAST_MESSAGED_TEMPERATURE(ch);
						msdp_update_room(ch);	// once we're sure we're staying
					}
				}
			}
		}
	}
	else {	// single-target teleprot
		if ((ch = get_char_by_room(room, arg1))) {
			if (!target) {
				wld_log(room, "wteleport character: target is an invalid room");
				return;
			}
			if (valid_dg_target(ch, DG_ALLOW_GODS)) {
				GET_LAST_DIR(ch) = NO_DIR;
				char_from_room(ch);
				char_to_room(ch, target);
				enter_wtrigger(IN_ROOM(ch), ch, NO_DIR, "script");
				qt_visit_room(ch, IN_ROOM(ch));
				RESET_LAST_MESSAGED_TEMPERATURE(ch);
				msdp_update_room(ch);	// once we're sure we're staying
			}
		}
		else if ((*arg1 == UID_CHAR && (veh = get_vehicle(arg1))) || (veh = get_vehicle_room(room, arg1, NULL))) {
			if (!target) {
				wld_log(room, "wteleport vehicle: target is an invalid room");
				return;
			}
			vehicle_from_room(veh);
			vehicle_to_room(veh, target);
			entry_vtrigger(veh, "script");
		}
		else if ((*arg1 == UID_CHAR && (obj = get_obj(arg1))) || (obj = get_obj_by_room(room, arg1))) {
			// teleport object
			if (target) {
				// move obj to room
				obj_to_room(obj, target);
			}
			else if ((ch = get_char_by_room(room, arg2))) {
				// move obj to character
				obj_to_char(obj, ch);
			}
			else if ((*arg2 == UID_CHAR && (veh = get_vehicle(arg2))) || (veh = get_vehicle_room(room, arg2, NULL))) {
				// move obj to vehicle
				if (!VEH_FLAGGED(veh, VEH_CONTAINER)) {
					wld_log(room, "wteleport object: attempting to put an object in a vehicle that's not a container");
					return;
				}
				else {
					obj_to_vehicle(obj, veh);
				}
			}
			else if ((*arg2 == UID_CHAR && (cont = get_obj(arg2))) || (cont = get_obj_by_room(room, arg2))) {
				if (cont == obj || get_top_object(obj) == cont || get_top_object(cont) == obj) {
					wld_log(room, "mteleport object: attempting to put an object inside itself");
					return;
				}
				else if (GET_OBJ_TYPE(cont) != ITEM_CONTAINER && GET_OBJ_TYPE(cont) != ITEM_CORPSE) {
					wld_log(room, "mteleport object: attempting to put an object in something that's not a container");
					return;
				}
				else {
					// move obj into obj
					obj_to_obj(obj, cont);
				}
			}
			else {
				wld_log(room, "wteleport object: no valid target location found");
			}
		}
		else {
			wld_log(room, "wteleport: no target found");
		}
	}
}


WCMD(do_wterracrop) {
	char loc_arg[MAX_INPUT_LENGTH], crop_arg[MAX_INPUT_LENGTH];
	crop_data *crop;
	room_data *target;
	crop_vnum vnum;

	argument = any_one_word(argument, loc_arg);
	any_one_word(argument, crop_arg);
	
	// usage: %terracrop% [location] <crop vnum>
	if (!*loc_arg) {
		wld_log(room, "oterracrop: bad syntax");
		return;
	}
	
	// check number of args
	if (!*crop_arg) {
		// only arg is actually crop arg
		strcpy(crop_arg, loc_arg);
		target = room;
	}
	else {
		// two arguments
		target = get_room(room, loc_arg);
	}
	
	if (!target) {
		wld_log(room, "oterracrop: target is an invalid room");
		return;
	}
	
	// places you just can't terracrop -- fail silently (currently)
	if (IS_INSIDE(target) || IS_ADVENTURE_ROOM(target) || IS_CITY_CENTER(target)) {
		return;
	}
	
	if (!isdigit(*crop_arg) || (vnum = atoi(crop_arg)) < 0 || !(crop = crop_proto(vnum))) {
		wld_log(room, "oterracrop: invalid crop vnum");
		return;
	}

	// good to go
	do_dg_terracrop(target, crop);
}


WCMD(do_wterraform) {
	char loc_arg[MAX_INPUT_LENGTH], sect_arg[MAX_INPUT_LENGTH];
	sector_data *sect;
	room_data *target;
	sector_vnum vnum;

	argument = any_one_word(argument, loc_arg);
	any_one_word(argument, sect_arg);
	
	// usage: %terraform% [location] <sector vnum>
	if (!*loc_arg) {
		wld_log(room, "oterraform: bad syntax");
		return;
	}
	
	// check number of args
	if (!*sect_arg) {
		// only arg is actually sect arg
		strcpy(sect_arg, loc_arg);
		target = room;
	}
	else {
		// two arguments
		target = get_room(room, loc_arg);
	}
	
	if (!target) {
		wld_log(room, "oterraform: target is an invalid room");
		return;
	}
	
	// places you just can't terraform -- fail silently (currently)
	if (IS_INSIDE(target) || IS_ADVENTURE_ROOM(target) || IS_CITY_CENTER(target)) {
		return;
	}
	
	if (!isdigit(*sect_arg) || (vnum = atoi(sect_arg)) < 0 || !(sect = sector_proto(vnum))) {
		wld_log(room, "oterraform: invalid sector vnum");
		return;
	}
	
	// validate sect
	if (SECT_FLAGGED(sect, SECTF_MAP_BUILDING | SECTF_INSIDE | SECTF_ADVENTURE)) {
		wld_log(room, "oterraform: sector requires data that can't be set this way");
		return;
	}

	// good to go
	do_dg_terraform(target, sect);
}


WCMD(do_wforce) {
	char_data *ch, *next_ch;
	char arg1[MAX_INPUT_LENGTH], *line;

	line = one_argument(argument, arg1);

	if (!*arg1 || !*line) {
		wld_log(room, "wforce called with too few args");
		return;
	}

	if (!str_cmp(arg1, "all")) {
		DL_FOREACH_SAFE2(ROOM_PEOPLE(room), ch, next_ch, next_in_room) {
			if (valid_dg_target(ch, 0)) {
				command_interpreter(ch, line);
			}
		}
	}

	else {
		if ((ch = get_char_by_room(room, arg1))) {
			if (valid_dg_target(ch, 0)) {
				command_interpreter(ch, line);
			}
		}

		else
			wld_log(room, "wforce: no target found");
	}
}


WCMD(do_wheal) {
	script_heal(room, WLD_TRIGGER, argument);
}


WCMD(do_wown) {
	char type_arg[MAX_INPUT_LENGTH], targ_arg[MAX_INPUT_LENGTH], emp_arg[MAX_INPUT_LENGTH];
	vehicle_data *vtarg = NULL;
	empire_data *emp = NULL;
	char_data *vict = NULL;
	room_data *rtarg = NULL;
	obj_data *otarg = NULL;
	
	*emp_arg = '\0';	// just in case
		
	// first arg -- possibly a type
	argument = one_argument(argument, type_arg);
	if (is_abbrev(type_arg, "room")) {
		argument = one_argument(argument, targ_arg);
		skip_spaces(&argument);
		
		if (!*targ_arg) {
			wld_log(room, "wown: Too few arguments (wown room)");
			return;
		}
		else if (!*argument) {
			// this was the last arg
			strcpy(emp_arg, targ_arg);
		}
		else if (!(rtarg = get_room(room, targ_arg))) {
			wld_log(room, "wown: Invalid room target");
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
			wld_log(room, "wown: Too few arguments (wown mob)");
			return;
		}
		else if (!(vict = ((*targ_arg == UID_CHAR) ? get_char(targ_arg) : get_char_by_room(room, targ_arg)))) {
			wld_log(room, "wown: Invalid mob target");
			return;
		}
	}
	else if (is_abbrev(type_arg, "vehicle")) {
		argument = one_argument(argument, targ_arg);
		skip_spaces(&argument);
		strcpy(emp_arg, argument);	// always
		
		if (!*targ_arg || !*emp_arg) {
			wld_log(room, "wown: Too few arguments (wown vehicle)");
			return;
		}
		else if (!(vtarg = ((*targ_arg == UID_CHAR) ? get_vehicle(targ_arg) : get_vehicle_room(room, targ_arg, NULL)))) {
			wld_log(room, "wown: Invalid vehicle target");
			return;
		}
	}
	else if (is_abbrev(type_arg, "object")) {
		argument = one_argument(argument, targ_arg);
		skip_spaces(&argument);
		strcpy(emp_arg, argument);	// always
		
		if (!*targ_arg || !*emp_arg) {
			wld_log(room, "wown: Too few arguments (wown obj)");
			return;
		}
		else if (!(otarg = ((*targ_arg == UID_CHAR) ? get_obj(targ_arg) : get_obj_by_room(room, targ_arg)))) {
			wld_log(room, "wown: Invalid obj target");
			return;
		}
	}
	else {	// attempt to find a target
		strcpy(targ_arg, type_arg);	// there was no type
		skip_spaces(&argument);
		strcpy(emp_arg, argument);
		
		if (!*targ_arg) {
			wld_log(room, "wown: Too few arguments");
			return;
		}
		else if (*targ_arg == UID_CHAR && ((vict = get_char(targ_arg)) || (vtarg = get_vehicle(targ_arg)) || (otarg = get_obj(targ_arg)) || (rtarg = get_room(room, targ_arg)))) {
			// found by uid
		}
		else if ((vict = get_char_by_room(room, targ_arg)) || (vtarg = get_vehicle_room(room, targ_arg, NULL)) || (otarg = get_obj_by_room(room, targ_arg))) {
			// found by name
		}
		else {
			wld_log(room, "wown: Invalid target");
			return;
		}
	}
	
	// only change owner on the home room
	if (rtarg) {
		rtarg = HOME_ROOM(rtarg);
	}
	
	// check that we got a target
	if (!vict && !vtarg && !rtarg && !otarg) {
		wld_log(room, "wown: Unable to find a target");
		return;
	}
	
	// process the empire
	if (!*emp_arg) {
		wld_log(room, "wown: No empire argument given");
		return;
	}
	else if (!str_cmp(emp_arg, "none")) {
		emp = NULL;	// set owner to none
	}
	else if (!(emp = get_empire(emp_arg))) {
		wld_log(room, "wown: Invalid empire");
		return;
	}
	
	// final target checks
	if (vict && !IS_NPC(vict)) {
		wld_log(room, "wown: Attempting to change the empire of a player");
		return;
	}
	if (rtarg && IS_ADVENTURE_ROOM(rtarg)) {
		wld_log(room, "wown: Attempting to change ownership of an adventure room");
		return;
	}
	
	// do the ownership change
	do_dg_own(emp, vict, otarg, rtarg, vtarg);
}


/* purge all objects an npcs in room, or specified object or mob */
WCMD(do_wpurge) {
	char arg[MAX_INPUT_LENGTH];
	char_data *ch, *next_ch;
	obj_data *obj, *next_obj;
	vehicle_data *veh;

	argument = one_argument(argument, arg);
	skip_spaces(&argument);

	if (!*arg) {
		/* purge all */
		DL_FOREACH_SAFE2(ROOM_PEOPLE(room), ch, next_ch, next_in_room) {
			if (IS_NPC(ch))
				extract_char(ch);
		}
		
		DL_FOREACH_SAFE2(ROOM_CONTENTS(room), obj, next_obj, next_content) {
			extract_obj(obj);
		}

		return;
	}
	
	// purge all mobs/objs in an instance
	if (!str_cmp(arg, "instance")) {
		struct instance_data *inst = ROOM_INSTANCE(room);
		if (!inst) {
			wld_log(room, "wpurge: room using purge instance outside an instance");
			return;
		}
		dg_purge_instance(room, inst, argument);
	}
	// purge char
	else if ((*arg == UID_CHAR && (ch = get_char(arg))) || (ch = get_char_in_room(room, arg))) {
		if (!IS_NPC(ch)) {
			wld_log(room, "wpurge: purging a PC");
			return;
		}
		
		if (*argument) {
			act(argument, TRUE, ch, NULL, NULL, TO_ROOM);
		}
		extract_char(ch);
	}
	// purge vehicle
	else if ((*arg == UID_CHAR && (veh = get_vehicle(arg))) || (veh = get_vehicle_room(room, arg, NULL))) {
		if (*argument) {
			act(argument, TRUE, ROOM_PEOPLE(IN_ROOM(veh)), NULL, veh, TO_CHAR | TO_ROOM | ACT_VEH_VICT);
		}
		extract_vehicle(veh);
	}
	// purge obj
	else if ((*arg == UID_CHAR && (obj = get_obj(arg))) || (obj = get_obj_in_room(room, arg))) {
		if (*argument) {
			room_data *room = obj_room(obj);
			act(argument, TRUE, room ? ROOM_PEOPLE(room) : NULL, obj, NULL, TO_CHAR | TO_ROOM);
		}
		extract_obj(obj);
	}
	else {
		wld_log(room, "wpurge: bad argument");
	}
}


// quest commands
WCMD(do_wquest) {
	do_dg_quest(WLD_TRIGGER, room, argument);
}


/* loads a mobile or object into the room */
WCMD(do_wload) {
	char arg1[MAX_INPUT_LENGTH], arg2[MAX_INPUT_LENGTH];
	struct instance_data *inst = find_instance_by_room(room, FALSE, TRUE);
	int number = 0;
	char_data *mob, *tch;
	obj_data *object, *cnt;
	room_data *in_room;
	vehicle_data *veh;
	char *target;
	int pos;

	target = two_arguments(argument, arg1, arg2);
	skip_spaces(&target);

	if (!*arg1 || !*arg2 || !is_number(arg2) || ((number = atoi(arg2)) < 0)) {
		wld_log(room, "wload: bad syntax");
		return;
	}

	if (is_abbrev(arg1, "mobile")) {
		if (!mob_proto(number)) {
			wld_log(room, "wload: bad mob vnum");
			return;
		}
		mob = read_mobile(number, TRUE);
		
		// store instance id
		if ((inst = find_instance_by_room(room, FALSE, TRUE))) {
			MOB_INSTANCE_ID(mob) = INST_ID(inst);
			if (MOB_INSTANCE_ID(mob) != NOTHING) {
				add_instance_mob(real_instance(MOB_INSTANCE_ID(mob)), GET_MOB_VNUM(mob));
			}
		}
		
		if (*target && isdigit(*target)) {
			scale_mob_to_level(mob, atoi(target));
			set_mob_flags(mob, MOB_NO_RESCALE);
		}
		else if (inst && INST_LEVEL(inst) > 0) {
			// instance level-locked
			scale_mob_to_level(mob, INST_LEVEL(inst));
		}
		
		char_to_room(mob, room);
		setup_generic_npc(mob, NULL, NOTHING, NOTHING);
		load_mtrigger(mob);
	}
	else if (is_abbrev(arg1, "object")) {
		if (!obj_proto(number)) {
			wld_log(room, "wload: bad object vnum");
			return;
		}
		object = read_object(number, TRUE);
		
		if (inst) {
			instance_obj_setup(inst, object);
		}
		
		/* special handling to make objects able to load on a person/in a container/worn etc. */
		if (!target || !*target) {
			obj_to_room(object, room);

			// adventure is level-locked?		
			if (inst && INST_LEVEL(inst) > 0) {
				scale_item_to_level(object, INST_LEVEL(inst));
			}
		
			load_otrigger(object);
			return;
		}

		target = two_arguments(target, arg1, arg2); /* recycling ... */
		skip_spaces(&target);
		
		// if they're picking a room, move arg2 down a slot to "target" level
		in_room = NULL;
		if (!str_cmp(arg1, "room")) {
			in_room = room;
			target = arg2;
		}
		else if (*arg1 == UID_CHAR) {
			if ((in_room = get_room(room, arg1))) {
				target = arg2;
			}
		}
		else {	// not targeting a room
			in_room = NULL;
		}
		
		// scaling on request
		if (*target && isdigit(*target)) {
			scale_item_to_level(object, atoi(target));
		}
		else if ((inst = find_instance_by_room(room, FALSE, TRUE)) && INST_LEVEL(inst) > 0) {
			// scaling by locked adventure
			scale_item_to_level(object, INST_LEVEL(inst));
		}
		
		if (in_room) {	// load in room
			obj_to_room(object, in_room); 
			load_otrigger(object);
			return;
		}
		
		tch = get_char_in_room(room, arg1);
		if (tch) {
			// mark as "gathered" like a resource
			if (!IS_NPC(tch) && GET_LOYALTY(tch)) {
				add_production_total(GET_LOYALTY(tch), GET_OBJ_VNUM(object), 1);
			}
			
			if (*arg2 && (pos = find_eq_pos_script(arg2)) >= 0 && !GET_EQ(tch, pos) && can_wear_on_pos(object, pos)) {
				equip_char(tch, object, pos);
				load_otrigger(object);
				// get_otrigger(object, tch, FALSE);
				determine_gear_level(tch);
				return;
			}
			obj_to_char(object, tch);
			if (load_otrigger(object)) {
				get_otrigger(object, tch, FALSE);
			}
			return;
		}
		cnt = get_obj_in_room(room, arg1);
		if (cnt && (GET_OBJ_TYPE(cnt) == ITEM_CONTAINER || GET_OBJ_TYPE(cnt) == ITEM_CORPSE)) {
			obj_to_obj(object, cnt);
			load_otrigger(object);
			return;
		}
		/* neither char nor container found - just dump it in room */
		obj_to_room(object, room); 
		load_otrigger(object);
		return;
	}
	else if (is_abbrev(arg1, "vehicle")) {
		if (!vehicle_proto(number)) {
			wld_log(room, "wload: bad vehicle vnum");
			return;
		}
		veh = read_vehicle(number, TRUE);
		if ((inst = find_instance_by_room(room, FALSE, TRUE))) {
			VEH_INSTANCE_ID(veh) = INST_ID(inst);
		}
		
		if (*target && isdigit(*target)) {
			scale_vehicle_to_level(veh, atoi(target));
		}
		else {
			// hope to inherit
			scale_vehicle_to_level(veh, 0);
		}
		
		vehicle_to_room(veh, room);
		get_vehicle_interior(veh);	// ensure inside is loaded
		// ownership
		if (VEH_CLAIMS_WITH_ROOM(veh) && ROOM_OWNER(HOME_ROOM(room))) {
			perform_claim_vehicle(veh, ROOM_OWNER(HOME_ROOM(room)));
		}
		load_vtrigger(veh);
	}
	else
		wld_log(room, "wload: bad type");
}


WCMD(do_wmod) {
	script_modify(argument);
}


WCMD(do_wmorph) {
	char tar_arg[MAX_INPUT_LENGTH], num_arg[MAX_INPUT_LENGTH];
	morph_data *morph = NULL;
	char_data *vict;
	bool normal;
	
	two_arguments(argument, tar_arg, num_arg);
	normal = !str_cmp(num_arg, "normal");
	
	if (!*tar_arg || !*num_arg) {
		wld_log(room, "wmorph: missing argument(s)");
	}
	else if (!(vict = get_char_by_room(room, tar_arg))) {
		wld_log(room, "wmorph: invalid target '%s'", tar_arg);
	}
	else if (!normal && (!isdigit(*num_arg) || !(morph = morph_proto(atoi(num_arg))))) {
		wld_log(room, "wmorph: invalid morph '%s'", num_arg);
	}
	else if (morph && MORPH_FLAGGED(morph, MORPHF_IN_DEVELOPMENT)) {
		wld_log(room, "wmorph: morph %d set in-development", MORPH_VNUM(morph));
	}
	else {
		perform_morph(vict, morph);
	}
}


WCMD(do_wdamage) {
	char name[MAX_INPUT_LENGTH], modarg[MAX_INPUT_LENGTH], typearg[MAX_INPUT_LENGTH], buf[MAX_STRING_LENGTH];
	double modifier = 1.0;
	char_data *ch;
	int type, show_attack_message = NOTHING;
	
	// optional attack message arg
	skip_spaces(&argument);
	if (*argument == '#') {
		argument = one_argument(argument, buf);
		if ((show_attack_message = atoi(buf+1)) < 1 || !real_attack_message(show_attack_message)) {
			wld_log(room, "wdamage: invalid attack message #%s", buf);
			show_attack_message = NOTHING;
		}
	}

	argument = two_arguments(argument, name, modarg);
	argument = one_argument(argument, typearg);	// optional

	if (!*name) {
		wld_log(room, "wdamage: bad syntax");
		return;
	}

	if (*modarg) {
		modifier = atof(modarg) / 100.0;
	}
	
	// send negatives to %heal% instead
	if (modifier < 0) {
		sprintf(buf, "%s health %.2f", name, -atof(modarg));
		script_heal(room, WLD_TRIGGER, buf);
		return;
	}
	
	ch = get_char_by_room(room, name);

	if (!ch) {
		wld_log(room, "wdamage: target not found");      
		return;
	}
	
	if (*typearg) {
		type = search_block(typearg, damage_types, FALSE);
		if (type == NOTHING) {
			wld_log(room, "wdamage: invalid type argument, using physical damage");
			type = DAM_PHYSICAL;
		}
	}
	else {
		type = DAM_PHYSICAL;
	}

	script_damage(ch, NULL, get_room_scale_level(room, ch), type, modifier, show_attack_message);
}


WCMD(do_waoe) {
	char modarg[MAX_INPUT_LENGTH], typearg[MAX_INPUT_LENGTH];
	char_data *vict, *next_vict;
	double modifier = 1.0;
	int level, type, show_attack_message = NOTHING;

	// optional attack message arg
	skip_spaces(&argument);
	if (*argument == '#') {
		argument = one_argument(argument, modarg);
		if ((show_attack_message = atoi(modarg+1)) < 1 || !real_attack_message(show_attack_message)) {
			wld_log(room, "waoe: invalid attack message #%s", modarg);
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
			wld_log(room, "waoe: invalid type argument, using physical damage");
			type = DAM_PHYSICAL;
		}
	}
	else {
		type = DAM_PHYSICAL;
	}
	
	level = get_room_scale_level(room, NULL);
	DL_FOREACH_SAFE2(ROOM_PEOPLE(room), vict, next_vict, next_in_room) {
		// harder to tell friend from foe: hit PCs or people following PCs
		if (!IS_NPC(vict) || (GET_LEADER(vict) && !IS_NPC(GET_LEADER(vict)))) {
			script_damage(vict, NULL, level, type, modifier, show_attack_message);
		}
	}
}


WCMD(do_wdot) {
	char name[MAX_INPUT_LENGTH], modarg[MAX_INPUT_LENGTH], durarg[MAX_INPUT_LENGTH], typearg[MAX_INPUT_LENGTH], stackarg[MAX_INPUT_LENGTH];
	any_vnum atype = ATYPE_DG_AFFECT;
	double modifier = 1.0;
	char_data *ch;
	int type, max_stacks, duration;

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
	argument = one_argument(argument, typearg);	// optional, defualt: physical
	argument = one_argument(argument, stackarg);	// optional, defualt: 1

	if (!*name || !*modarg || !*durarg) {
		wld_log(room, "wdot: bad syntax");
		return;
	}

	if (*modarg) {
		modifier = atof(modarg) / 100.0;
	}
	
	ch = get_char_by_room(room, name);

	if (!ch) {
		wld_log(room, "wdot: target not found");      
		return;
	}
	if ((duration = atoi(durarg)) < 1) {
		wld_log(room, "wdot: invalid duration '%s'", durarg);
		return;
	}
	
	if (*typearg) {
		type = search_block(typearg, damage_types, FALSE);
		if (type == NOTHING) {
			wld_log(room, "wdot: invalid type argument, using physical damage");
			type = DAM_PHYSICAL;
		}
	}
	else {
		type = DAM_PHYSICAL;
	}

	max_stacks = (*stackarg ? atoi(stackarg) : 1);
	script_damage_over_time(ch, atype, get_room_scale_level(room, ch), type, modifier, duration, max_stacks, NULL);
}


WCMD(do_wat) {
	char location[MAX_INPUT_LENGTH], arg2[MAX_INPUT_LENGTH];
	room_data *r2;

	half_chop(argument, location, arg2);

	if (!*location || !*arg2) {
		wld_log(room, "wat: bad syntax");
		return;
	}
	r2 = get_room(room, location);
	if (!r2) {
		wld_log(room, "wat: location not found");
		return;
	}

	wld_command_interpreter(r2, arg2);
}


WCMD(do_wrestore) {
	struct affected_type *aff, *next_aff;
	char arg[MAX_INPUT_LENGTH];
	vehicle_data *veh = NULL;
	char_data *victim = NULL;
	obj_data *obj = NULL;
	room_data *rtarg = NULL;
	bitvector_t bitv;
	bool done_aff;
	int pos;
		
	one_argument(argument, arg);
	
	// find a target
	if (!*arg) {
		rtarg = room;
	}
	else if (!str_cmp(arg, "room") || !str_cmp(arg, "building")) {
		rtarg = room;
	}
	else if ((*arg == UID_CHAR && (victim = get_char(arg))) || (victim = get_char_in_room(room, arg))) {
		// found victim
	}
	else if ((*arg == UID_CHAR && (veh = get_vehicle(arg))) || (veh = get_vehicle_room(room, arg, NULL))) {
		// found vehicle
		if (!VEH_IS_COMPLETE(veh)) {
			wld_log(room, "wrestore: used on unfinished vehicle");
			return;
		}
	}
	else if ((*arg == UID_CHAR && (obj = get_obj(arg))) || (obj = get_obj_in_room(room, arg))) {
		// found obj
	}
	else if ((rtarg = get_room(room, arg))) {
		// found room
	}
	else {
		// bad arg
		wld_log(room, "wrestore: bad argument");
		return;
	}
	
	if (rtarg) {
		rtarg = HOME_ROOM(rtarg);
		if (!IS_COMPLETE(rtarg)) {
			wld_log(room, "wrestore: used on unfinished building");
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
	if (rtarg) {
		if (COMPLEX_DATA(rtarg)) {
			free_resource_list(GET_BUILDING_RESOURCES(rtarg));
			GET_BUILDING_RESOURCES(rtarg) = NULL;
			set_room_damage(rtarg, 0);
			set_burn_down_time(rtarg, 0, FALSE);
			request_world_save(GET_ROOM_VNUM(rtarg), WSAVE_ROOM);
		}
	}
}


WCMD(do_wscale) {
	char arg[MAX_INPUT_LENGTH], lvl_arg[MAX_INPUT_LENGTH];
	struct instance_data *inst = NULL;
	char_data *victim;
	obj_data *obj, *fresh, *proto;
	vehicle_data *veh;
	int level;

	two_arguments(argument, arg, lvl_arg);

	if (!*arg) {
		wld_log(room, "wscale: No target provided");
		return;
	}
	
	if (*lvl_arg && !isdigit(*lvl_arg)) {
		wld_log(room, "wscale: invalid level '%s'", lvl_arg);
		return;
	}
	else if (*lvl_arg) {
		level = atoi(lvl_arg);
	}
	else if ((inst = find_instance_by_room(room, FALSE, TRUE))) {
		level = INST_LEVEL(inst);
	}
	else {
		level = 0;
	}
	
	if (level <= 0) {
		wld_log(room, "wscale: no valid level to scale to");
		return;
	}
	
	// scale adventure
	if (!str_cmp(arg, "instance")) {
		if (inst || (inst = find_instance_by_room(room, FALSE, TRUE))) {
			scale_instance_to_level(inst, level);
		}
	}
	// scale char
	else if ((*arg == UID_CHAR && (victim = get_char(arg))) || (victim = get_char_in_room(room, arg))) {
		if (!IS_NPC(victim)) {
			wld_log(room, "wscale: unable to scale a PC");
			return;
		}

		scale_mob_to_level(victim, level);
		set_mob_flags(victim, MOB_NO_RESCALE);
	}
	// scale vehicle
	else if ((*arg == UID_CHAR && (veh = get_vehicle(arg))) || (veh = get_vehicle_room(room, arg, NULL))) {
		scale_vehicle_to_level(veh, level);
	}
	// scale obj
	else if ((*arg == UID_CHAR && (obj = get_obj(arg))) || (obj = get_obj_in_room(room, arg))) {
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
		wld_log(room, "wscale: bad argument");
	}
}


 //////////////////////////////////////////////////////////////////////////////
//// WORLD COMMAND INTERPRETER ///////////////////////////////////////////////

const struct wld_command_info wld_cmd_info[] = {
	{ "RESERVED", NULL, NO_SCMD },/* this must be first -- for specprocs */

	{ "wadventurecomplete", do_wadventurecomplete, NO_SCMD },
	{ "wasound", do_wasound, NO_SCMD },
	{ "wbuild", do_wbuild, NO_SCMD },
	{ "wdoor", do_wdoor, NO_SCMD },
	{ "wecho", do_wecho, NO_SCMD },
	{ "wechoaround", do_wsend, SCMD_WECHOAROUND },
	{ "wechoneither", do_wechoneither, NO_SCMD },
	{ "wforce", do_wforce, NO_SCMD },
	{ "wheal", do_wheal, NO_SCMD },
	{ "wload", do_wload, NO_SCMD },
	{ "wmod", do_wmod, NO_SCMD },
	{ "wmorph", do_wmorph, NO_SCMD },
	{ "wpurge", do_wpurge, NO_SCMD },
	{ "wquest", do_wquest, NO_SCMD },
	{ "wrestore", do_wrestore, NO_SCMD },
	{ "wscale", do_wscale, NO_SCMD },
	{ "wsend", do_wsend, SCMD_WSEND },
	{ "wsiege", do_wsiege, NO_SCMD },
	{ "wslay", do_wslay, NO_SCMD },
	{ "wteleport", do_wteleport, NO_SCMD },
	{ "wterracrop", do_wterracrop, NO_SCMD },
	{ "wterraform", do_wterraform, NO_SCMD },
	{ "wbuildingecho", do_wbuildingecho, NO_SCMD },
	{ "wregionecho", do_wregionecho, NO_SCMD },
	{ "wsubecho", do_wsubecho, NO_SCMD },
	{ "wvehicleecho", do_wvehicleecho, NO_SCMD },
	{ "wdamage", do_wdamage, NO_SCMD },
	{ "waoe", do_waoe, NO_SCMD },
	{ "wdot", do_wdot, NO_SCMD },
	{ "wat", do_wat, NO_SCMD },

	{ "\n", NULL, NO_SCMD }        /* this must be last */
};


/**
*  This is the command interpreter used by rooms, called by script_driver.
*
* @param room_data *room The room running the command.
* @param char *argument The rest of the line from the script.
*/
void wld_command_interpreter(room_data *room, char *argument) {
	int cmd, length;
	char line[MAX_INPUT_LENGTH], arg[MAX_INPUT_LENGTH];

	skip_spaces(&argument);

	/* just drop to next line for hitting CR */
	if (!*argument) {
		return;
	}

	half_chop(argument, arg, line);

	/* find the command */
	for (length = strlen(arg), cmd = 0; *wld_cmd_info[cmd].command != '\n'; cmd++) {
		if (!strn_cmp(wld_cmd_info[cmd].command, arg, length)) {
			break;
		}
	}

	if (*wld_cmd_info[cmd].command == '\n') {
		wld_log(room, "Unknown world cmd: '%s'", argument);
	}
	else {
		((*wld_cmd_info[cmd].command_pointer)(room, line, cmd, wld_cmd_info[cmd].subcmd));
	}
}
